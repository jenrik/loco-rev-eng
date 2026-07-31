#ifndef _WIN32

#include "sdl3_net_game_bridge.h"

#include "sdl3_net_runtime.h"
#include "host_test_events.h"
#include "../decompiled_cpp/game/Train.h"
#include "../decompiled_cpp/game/Vehicle.h"
#include "../decompiled_cpp/game/PlayerConfig.h"
#include "../decompiled_cpp/network/DPlayManager.h"
#include "../decompiled_cpp/network/Netman.h"

#include <algorithm>
#include <cstring>
#include <new>

extern void* operator_new(std::size_t size);
extern void GLOBAL_free(void* pointer);

namespace lego_loco::network {
namespace {

std::uint16_t Read16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(bytes[1] << 8);
}
std::uint32_t Read32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

TrainMessage* NewMessage(std::int32_t type) {
    // Keep the recovered operator_new ownership while starting the native
    // TrainMessage lifetime through its constructor, not byte clearing.
    void* storage = operator_new(sizeof(TrainMessage));
    if (!storage) return nullptr;
    auto* message = ::new (storage) TrainMessage{};
    message->type = type;
    return message;
}

void* CopyPayload(const std::vector<std::uint8_t>& payload) {
    void* copy = operator_new(payload.size());
    if (copy) std::memcpy(copy, payload.data(), payload.size());
    return copy;
}

void CopyCompactPlayer(PlayerSlot& destination, const std::uint8_t* source) {
    destination.dpId = static_cast<std::int32_t>(Read32(source));
    destination.is_connected = source[0x3a];
    std::memcpy(destination.compact_name, source + 0x0c,
                sizeof(destination.compact_name));
    destination.compact_name[sizeof(destination.compact_name) - 1] = '\0';
    std::memcpy(destination.layout_name, source + 0x19, 32);
    destination.layout_name[31] = '\0';
    destination.player_id = static_cast<std::uint16_t>(Read32(source + 4));
    destination.player_color = static_cast<std::uint16_t>(Read32(source + 4) >> 16);
    destination.flag_36 = source[0x39];
    destination.version = static_cast<std::int32_t>(Read32(source + 8));
}

bool QueueMessage(TrainSubsystem* train, TrainMessage* message, std::string* error) {
    if (!message) {
        if (error) *error = "could not allocate TrainMessage";
        return false;
    }
    train->QueueMessage(message);
    return true;
}

void ResetHostOwnedServiceState(TrainSubsystem* train) {
    train->ClearHostTrackSessions();
    train->host_received_assets.clear();
    train->host_attachment_transfers.clear();
    train->host_track_build_packet.clear();
    train->host_remote_player_config = 0;
    train->host_connection_flags = 0;
    train->host_connection_target.clear();
}

bool RequireSize(const std::vector<std::uint8_t>& payload, std::size_t minimum,
                 std::string* error) {
    if (payload.size() >= minimum) return true;
    if (error) *error = "legacy packet is shorter than its recovered message layout";
    return false;
}

}  // namespace

bool QueueLegacyPayloadForGame(Netman* netman, TrainSubsystem* train,
                               std::uint32_t sender,
                               const std::vector<std::uint8_t>& payload,
                               std::string* error) {
    if (!netman || !train || !ValidateLegacyPayload(payload, error)) return false;
    const std::uint16_t type = Read16(payload.data());

    if (type == 10) {
        netman->HostEndTransportSession();
        return true;
    }
    if (type == 0x14) {
        if (!RequireSize(payload, 8, error)) return false;
        train->HandlePlayerLeave(static_cast<int>(Read32(payload.data() + 4)));
        return true;
    }

    switch (type) {
    case 0x3ea: {
        if (!RequireSize(payload, 9, error)) return false;
        train->host_remote_player_config = static_cast<std::int32_t>(Read32(payload.data() + 4));
        if (payload[8] != 0) train->field_30 = 1;

        // 0x439A50 answers PlayerInfo with 0x3EC. The mode-2 host has no
        // original VehicleEditor list yet, so this is the exact empty-list form.
        std::vector<std::uint8_t> response(0x14, 0);
        response[0] = 0xec; response[1] = 0x03;
        response[2] = static_cast<std::uint8_t>(kLegacyProtocolVersion);
        response[3] = static_cast<std::uint8_t>(kLegacyProtocolVersion >> 8);
        std::memcpy(response.data() + 4, payload.data() + 4, 4);
        HostTransportWorker().SendLegacy(sender, std::move(response));
        loco::host_test::emit_legacy_service_applied(type, payload.size());
        return true;
    }
    case 0x3eb: {
        if (!RequireSize(payload, 10, error)) return false;
        const auto terminator = std::find(payload.begin() + 9, payload.end(), 0);
        if (terminator == payload.end()) {
            if (error) *error = "0x3EB connection target is not NUL terminated";
            return false;
        }
        train->host_connection_flags = payload[8];
        train->host_connection_target.assign(payload.begin() + 9, terminator);
        // The original target was an obsolete central DirectPlay server or an
        // external browser URL. SDL_net retains the validated control state but
        // deliberately performs neither platform side effect.
        loco::host_test::emit_legacy_service_applied(type, payload.size());
        return true;
    }
    case 0x3ec: {
        if (!RequireSize(payload, 0x14, error)) return false;
        const std::uint32_t header_count = Read32(payload.data() + 8);
        const std::uint32_t count = Read32(payload.data() + 0x0c);
        // Vehicle::InitRoute (0x44C220) permits at most three secondary
        // editors; Train_SendPlayerInfo (0x43CCC0) serializes only those.
        if (header_count != count || count > 3 ||
            payload.size() != 0x14 + static_cast<std::size_t>(count) * 0x390) {
            if (error) *error = "0x3EC track-build payload has an invalid session count";
            return false;
        }

        train->ClearHostTrackSessions();
        train->host_track_build_packet = payload;
        train->host_remote_player_config =
            static_cast<std::int32_t>(Read32(payload.data() + 4));
        Vehicle* track_vehicle = nullptr;
        if (count != 0) {
            void* storage = operator_new(sizeof(Vehicle));
            if (storage == nullptr) {
                if (error) *error = "could not allocate 0x3EC Vehicle";
                return false;
            }
            track_vehicle = ::new (storage)
                Vehicle(HostNetworkVehicleTag{}, 0x1804);
            if (track_vehicle->editors[0] == nullptr) {
                track_vehicle->~Vehicle();
                GLOBAL_free(track_vehicle);
                if (error) *error = "could not construct 0x3EC Vehicle";
                return false;
            }
            train->host_track_vehicles.push_back(track_vehicle);
        }
        for (std::uint32_t index = 0; index < count; ++index) {
            void* storage = operator_new(sizeof(DPlayManager));
            if (storage == nullptr) {
                train->ClearHostTrackSessions();
                if (error) *error = "could not allocate decoded 0x3EC session";
                return false;
            }
            auto* session = ::new (storage) DPlayManager;
            session->CreatePlayer();
            const std::uint8_t* wire = payload.data() + 0x14 + index * 0x390;
            if (!session->LoadLegacySessionWire(wire, 0x390)) {
                session->~DPlayManager();
                GLOBAL_free(session);
                train->ClearHostTrackSessions();
                if (error) *error = "could not decode 0x3EC session";
                return false;
            }
            // 0x43CF04..0x43CF41 replaces snapshot block 1 with the local
            // player name and clears m_wordValue before assigning an editor.
            if (g_player_config != nullptr) {
                const std::size_t name_size = std::min(
                    std::strlen(g_player_config->name) + 1,
                    sizeof(session->m_sessionBlk1));
                std::memcpy(session->m_sessionBlk1, g_player_config->name,
                            name_size);
            }
            session->m_wordValue = 0;
            train->host_track_sessions.push_back(session);
            train->DownloadMissingAssets(session);
            if (!track_vehicle->AddHostNetworkRoute(*session)) {
                train->ClearHostTrackSessions();
                if (error) *error = "could not attach decoded 0x3EC route";
                return false;
            }
        }
        const std::uint32_t materialized_vehicle_count =
            track_vehicle == nullptr ? 0 : 1;
        const std::uint32_t editor_count = track_vehicle == nullptr ? 0 :
            static_cast<std::uint32_t>(track_vehicle->editor_count + 1);
        if (track_vehicle != nullptr) {
            track_vehicle->init_flag = 0;
            TrainMessage* message = NewMessage(0x0F);
            if (message == nullptr) {
                train->ClearHostTrackSessions();
                if (error) *error = "could not queue materialized 0x3EC Vehicle";
                return false;
            }
            message->data_ptr = track_vehicle;
            train->host_track_vehicles.pop_back();
            if (!QueueMessage(train, message, error)) return false;
        }
        const std::uint16_t first_entry_count = count == 0 ? 0 :
            Read16(payload.data() + 0x14 + 0x8e);
        const std::uint8_t first_signal_type = count == 0 ? 0 :
            payload[0x14 + 0x91];
        loco::host_test::emit_legacy_track_sessions_materialized(
            count, materialized_vehicle_count, editor_count,
            train->host_remote_player_config,
            first_entry_count > 128 ? 128 : first_entry_count,
            first_signal_type);
        loco::host_test::emit_legacy_service_applied(type, payload.size());
        return true;
    }
    case 0x3ed:
        // The original Train_ProcessMessages jump table has no case 3: the
        // client frees this server-only asset request without side effects.
        if (!RequireSize(payload, 6, error)) return false;
        loco::host_test::emit_legacy_service_applied(type, payload.size());
        return true;
    case 0x3ee: {
        if (!RequireSize(payload, 0x0c, error)) return false;
        const std::uint32_t byte_count = Read32(payload.data() + 8);
        if (byte_count > kMaximumTransportPayload - 0x0c ||
            payload.size() != 0x0c + byte_count) {
            if (error) *error = "0x3EE asset payload length does not match its header";
            return false;
        }
        auto existing = std::find_if(
            train->host_received_assets.begin(), train->host_received_assets.end(),
            [&](const TrainSubsystem::HostReceivedAsset& asset) {
                return asset.mode == payload[4] && asset.type == payload[5];
            });
        const bool replaced = existing != train->host_received_assets.end();
        if (!replaced) {
            TrainSubsystem::HostReceivedAsset asset;
            asset.mode = payload[4];
            asset.type = payload[5];
            asset.bytes.assign(payload.begin() + 0x0c, payload.end());
            train->host_received_assets.push_back(std::move(asset));
        } else {
            existing->bytes.assign(payload.begin() + 0x0c, payload.end());
        }
        if (train->request_count > 0) --train->request_count;
        loco::host_test::emit_legacy_asset_owned(
            payload[4], payload[5], byte_count, replaced,
            train->host_received_assets.size());
        for (DPlayManager* session : train->host_track_sessions)
            train->DownloadMissingAssets(session);
        loco::host_test::emit_legacy_service_applied(type, payload.size());
        return true;
    }
    case 0x3ef:
        // Reserved hole in the original jump table; accepted and discarded.
        loco::host_test::emit_legacy_service_applied(type, payload.size());
        return true;
    case 0x3f0: {
        if (!RequireSize(payload, 0x1c, error)) return false;
        TrainMessage* message = NewMessage(4);
        if (!message) return QueueMessage(train, message, error);
        char* name = static_cast<char*>(operator_new(13));
        if (!name) { if (error) *error = "could not allocate scenario player name"; return false; }
        std::memcpy(name, payload.data() + 8, 13);
        name[12] = '\0';
        message->data_ptr = name;
        message->target_dpId = static_cast<std::int32_t>(sender);
        message->flags = static_cast<std::int32_t>(Read32(payload.data() + 4));
        message->setMetadata0(payload[0x16]);
        message->setMetadata1(payload[0x18]);
        return QueueMessage(train, message, error);
    }
    case 0x3f1: {
        constexpr std::size_t kPacketSize = 0x0c + 9 * 0x3c;
        if (!RequireSize(payload, kPacketSize, error)) return false;
        TrainMessage* message = NewMessage(9);
        if (!message) return QueueMessage(train, message, error);
        auto* slots = static_cast<PlayerSlot*>(operator_new(sizeof(PlayerSlot) * 9));
        if (!slots) { if (error) *error = "could not allocate synchronized player slots"; return false; }
        std::memset(slots, 0, sizeof(PlayerSlot) * 9);
        for (int index = 0; index < 9; ++index)
            CopyCompactPlayer(slots[index], payload.data() + 0x0c + index * 0x3c);
        message->data_ptr = slots;
        message->flags = static_cast<std::int32_t>(Read32(payload.data() + 4));
        message->setMetadata0(payload[8]);
        message->setMetadata1(payload[9]);
        return QueueMessage(train, message, error);
    }
    case 0x3f2:
        train->HandleConnectionSetup(const_cast<std::uint8_t*>(payload.data()));
        return true;
    case 0x3f3:
        train->HandleControllerInit(const_cast<std::uint8_t*>(payload.data()),
                                    static_cast<int>(sender));
        return true;
    case 0x3f4:
    case 0x3f5: {
        TrainMessage* message = NewMessage(type == 0x3f4 ? 0x13 : 0x14);
        if (!message) return QueueMessage(train, message, error);
        message->flags = netman->FindPlayerIndex(static_cast<std::int32_t>(sender));
        return QueueMessage(train, message, error);
    }
    case 0x3f6: {
        if (!RequireSize(payload, 9, error)) return false;
        const std::uint16_t count = Read16(payload.data() + 6);
        if (payload[4] >= 9 || count > (kMaximumTransportPayload - 10) / 8 ||
            payload.size() != 10 + static_cast<std::size_t>(count) * 8) {
            if (error) *error = "0x3F6 ping payload has invalid slot/count metadata";
            return false;
        }
        for (std::uint16_t index = 0; index < count; ++index) {
            const std::size_t offset = 9 + static_cast<std::size_t>(index) * 8;
            if (payload[offset + 6] >= 9 || payload[offset + 7] >= 9) {
                if (error) *error = "0x3F6 ping entry names an invalid player slot";
                return false;
            }
        }
        TrainMessage* message = NewMessage(0x15);
        if (!message) return QueueMessage(train, message, error);
        message->data_ptr = CopyPayload(payload);
        if (!message->data_ptr) {
            GLOBAL_free(message);
            if (error) *error = "could not allocate 0x3F6 ping payload";
            return false;
        }
        return QueueMessage(train, message, error);
    }
    case 0x3f7: {
        LegacyTrainPositionAck decoded;
        if (!DecodeLegacyTrainPositionAck(payload, &decoded, error)) return false;
        const int index = netman->FindPlayerIndex(
            static_cast<std::int32_t>(sender));
        if (index < 0) return true;
        void* storage = operator_new(sizeof(TrainPositionAckPacket));
        if (storage == nullptr) {
            if (error) *error = "could not allocate 0x3F7 transfer acknowledgment";
            return false;
        }
        auto* packet = ::new (storage) TrainPositionAckPacket{};
        packet->packet_type = 0x3F7;
        packet->protocol_version = kLegacyProtocolVersion;
        packet->network_id = decoded.network_id;
        packet->slot_index = decoded.slot_index;
        packet->peer_index = decoded.peer_index;
        packet->reserved = decoded.reserved;
        train->HandleTrainPosUpdate(packet, index);
        loco::host_test::emit_legacy_service_applied(type, payload.size());
        return true;
    }
    case 0x3f8: {
        if (!RequireSize(payload, 5, error)) return false;
        TrainMessage* message = NewMessage(0x1a);
        if (!message) return QueueMessage(train, message, error);
        message->data_ptr = reinterpret_cast<void*>(static_cast<std::uintptr_t>(payload[4]));
        message->target_dpId = static_cast<std::int32_t>(sender);
        train->UpdatePlayerCount(payload[4]);
        return QueueMessage(train, message, error);
    }
    case 0x3f9: {
        if (!RequireSize(payload, 0x14, error)) return false;
        const std::uint32_t byte_count = Read32(payload.data() + 0x10);
        const int slot = netman->FindPlayerIndex(static_cast<std::int32_t>(sender));
        if (slot < 0 || slot >= 9 || byte_count > kMaximumTransportPayload - 0x14 ||
            payload.size() != 0x14 + byte_count) {
            if (error) *error = "0x3F9 pixel payload has invalid owner or byte count";
            return false;
        }
        TrainMessage* message = NewMessage(0x16);
        if (!message) return QueueMessage(train, message, error);
        message->data_ptr = CopyPayload(payload);
        message->flags = slot;
        if (!message->data_ptr) {
            GLOBAL_free(message);
            if (error) *error = "could not allocate 0x3F9 pixel payload";
            return false;
        }
        return QueueMessage(train, message, error);
    }
    case 0x3fa:
        netman->SendBuildingData(static_cast<std::int32_t>(sender));
        return true;
    case 0x3fb:
        train->HandlePlayerJoin(const_cast<std::uint8_t*>(payload.data()),
                                static_cast<int>(sender));
        return true;
    case 0x3fc: {
        if (!RequireSize(payload, 0x10, error)) return false;
        const std::uint32_t byte_count = Read32(payload.data() + 4);
        const std::uint16_t train_type = Read16(payload.data() + 8);
        const std::uint16_t sequence = Read16(payload.data() + 10);
        const std::uint8_t subtype = payload[12];
        if (subtype > 2 || byte_count > kMaximumTransportPayload - 0x10 ||
            payload.size() != 0x10 + byte_count) {
            if (error) *error = "0x3FC attachment block has invalid subtype or length";
            return false;
        }
        auto transfer = std::find_if(
            train->host_attachment_transfers.begin(),
            train->host_attachment_transfers.end(),
            [&](const TrainSubsystem::HostAttachmentTransfer& item) {
                return item.sender == sender && item.train_type == train_type;
            });
        const auto data_begin = payload.begin() + 0x0d;
        const auto data_end = data_begin + byte_count;
        if (subtype == 0) {
            if (sequence != 0) {
                if (error) *error = "0x3FC FIRST block has nonzero sequence";
                return false;
            }
            if (transfer == train->host_attachment_transfers.end()) {
                train->host_attachment_transfers.push_back({});
                transfer = train->host_attachment_transfers.end() - 1;
            }
            transfer->sender = sender;
            transfer->train_type = train_type;
            transfer->sequence = 0;
            transfer->complete = false;
            transfer->attachment_bytes.assign(data_begin, data_end);
            transfer->final_bytes.clear();
        } else {
            if (transfer == train->host_attachment_transfers.end() ||
                transfer->complete || sequence != transfer->sequence + 1) {
                if (error) *error = "0x3FC attachment block is out of sequence";
                return false;
            }
            transfer->sequence = sequence;
            if (subtype == 1)
                transfer->attachment_bytes.insert(
                    transfer->attachment_bytes.end(), data_begin, data_end);
            else {
                transfer->final_bytes.assign(data_begin, data_end);
                transfer->complete = true;
            }
        }
        loco::host_test::emit_legacy_attachment_updated(
            sender, train_type, sequence, subtype,
            transfer->attachment_bytes.size(), transfer->final_bytes.size(),
            transfer->complete);
        return true;
    }
    case 0x3fd:
        train->HandlePlayerLeave(static_cast<int>(sender));
        return true;
    default:
        if (error) *error = "legacy packet type is not integrated with the host TrainSubsystem";
        return false;
    }
}

Netman* CreateHostNetman() { return new Netman(); }

void PumpTransportIntoGame(Netman* netman, TrainSubsystem* train,
                           const char* local_player_name) {
    if (!netman || !train) return;
    TransportWorker& worker = HostTransportWorker();
    for (TransportEvent& event : worker.TakeEvents()) {
        const TransportRuntimeSnapshot snapshot = worker.Snapshot();
        switch (event.kind) {
        case TransportEventKind::Listening:
            ResetHostOwnedServiceState(train);
            netman->HostBeginTransportSession(true, kHostPlayerId, local_player_name);
            train->player_peer_id = kHostPlayerId;
            loco::host_test::emit_netman_session_ready(kHostPlayerId, true);
            break;
        case TransportEventKind::Connected:
            ResetHostOwnedServiceState(train);
            netman->HostBeginTransportSession(false,
                static_cast<std::int32_t>(event.player_id), local_player_name);
            train->player_peer_id = static_cast<std::int32_t>(event.player_id);
            loco::host_test::emit_netman_session_ready(event.player_id, false);
            break;
        case TransportEventKind::PlayerJoined:
            netman->HostAddTransportPlayer(static_cast<std::int32_t>(event.player_id),
                                           event.message.c_str());
            loco::host_test::emit_netman_player_joined(event.player_id);
            break;
        case TransportEventKind::PlayerLeft:
            netman->HostRemoveTransportPlayer(static_cast<std::int32_t>(event.player_id));
            break;
        case TransportEventKind::LegacyPayload: {
            std::string error;
            if (!QueueLegacyPayloadForGame(netman, train, event.sender,
                                           event.payload, &error)) {
                worker.StopTransport();
                netman->HostEndTransportSession();
            }
            break;
        }
        case TransportEventKind::Disconnected:
            netman->HostEndTransportSession();
            break;
        case TransportEventKind::Error:
            // A host can reject one malformed client and continue listening.
            if (snapshot.mode == TransportRuntimeMode::Client)
                netman->HostEndTransportSession();
            break;
        }
    }
}

}  // namespace lego_loco::network

#endif
