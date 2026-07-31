#ifndef _WIN32

#include "sdl3_net_transport.h"

#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <utility>

namespace lego_loco::network {
namespace {

using Clock = std::chrono::steady_clock;
constexpr auto kHandshakeTimeout = std::chrono::seconds(5);
constexpr int kReadChunkSize = 4096;

std::string NetError(const char* prefix) {
    const char* detail = SDL_GetError();
    return std::string(prefix) + (detail && *detail ? std::string(": ") + detail : "");
}

bool WriteFrame(NET_StreamSocket* socket, const TransportFrame& frame,
                std::string* error) {
    std::vector<std::uint8_t> bytes = EncodeTransportFrame(frame);
    if (bytes.empty()) {
        if (error) *error = "failed to encode transport frame";
        return false;
    }
    const int pending = NET_GetStreamSocketPendingWrites(socket);
    if (pending < 0) {
        if (error) *error = NetError("pending-write query failed");
        return false;
    }
    if (pending > kMaximumPendingTransportBytes - static_cast<int>(bytes.size())) {
        if (error) *error = "transport pending-write high-water mark exceeded";
        return false;
    }
    if (!NET_WriteToStreamSocket(socket, bytes.data(), static_cast<int>(bytes.size()))) {
        if (error) *error = NetError("stream write failed");
        return false;
    }
    return true;
}

void DrainAndDestroy(NET_StreamSocket*& socket) {
    if (!socket) return;
    (void)NET_WaitUntilStreamSocketDrained(socket, 250);
    NET_DestroyStreamSocket(socket);
    socket = nullptr;
}

TransportEvent ErrorEvent(std::string message) {
    TransportEvent event;
    event.kind = TransportEventKind::Error;
    event.message = std::move(message);
    return event;
}

std::vector<std::uint8_t> RejectionPayload(const std::string& message) {
    const std::size_t length = std::min<std::size_t>(message.size(), 255);
    return std::vector<std::uint8_t>(message.begin(), message.begin() + length);
}

}  // namespace

bool ParseDirectConnectEndpoint(const std::string& text,
                                DirectConnectEndpoint* endpoint,
                                std::string* error) {
    if (!endpoint) {
        if (error) *error = "direct-connect endpoint output is null";
        return false;
    }
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        if (error) *error = "direct-connect host is empty";
        return false;
    }
    const std::string value = text.substr(begin, end - begin + 1);
    std::string host;
    std::string port_text;
    if (value.front() == '[') {
        const std::size_t close = value.find(']');
        if (close == std::string::npos || close == 1 ||
            (close + 1 < value.size() && value[close + 1] != ':')) {
            if (error) *error = "IPv6 endpoints must use [address]:port";
            return false;
        }
        host = value.substr(1, close - 1);
        if (close + 1 < value.size()) port_text = value.substr(close + 2);
    } else {
        const std::size_t first_colon = value.find(':');
        const std::size_t last_colon = value.rfind(':');
        if (first_colon != std::string::npos && first_colon == last_colon) {
            host = value.substr(0, first_colon);
            port_text = value.substr(first_colon + 1);
        } else {
            host = value;
        }
    }
    if (host.empty() || host.size() > 253 ||
        host.find_first_of(" \t\r\n/[]") != std::string::npos) {
        if (error) *error = "invalid direct-connect host";
        return false;
    }
    unsigned long port = kDefaultLegoLocoPort;
    if (!port_text.empty()) {
        char* parse_end = nullptr;
        port = std::strtoul(port_text.c_str(), &parse_end, 10);
        if (!parse_end || *parse_end != '\0' || port == 0 || port > 65535) {
            if (error) *error = "direct-connect port must be 1..65535";
            return false;
        }
    } else if ((!value.empty() && value.back() == ':') ||
               (!value.empty() && value.front() == '[' && value.back() == ':')) {
        if (error) *error = "direct-connect port is empty";
        return false;
    }
    endpoint->host = std::move(host);
    endpoint->port = static_cast<std::uint16_t>(port);
    return true;
}

std::string GenerateSessionUuid() {
    std::array<std::uint8_t, 16> bytes{};
    std::random_device random;
    for (std::uint8_t& byte : bytes) byte = static_cast<std::uint8_t>(random());
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3f) | 0x80);
    char text[37];
    std::snprintf(text, sizeof(text),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
                  bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11],
                  bytes[12], bytes[13], bytes[14], bytes[15]);
    return text;
}

struct SdlNetTransportHost::Impl {
    struct Peer {
        NET_StreamSocket* socket = nullptr;
        TransportFrameDecoder decoder;
        VirtualPlayerId id = 0;
        std::string name;
        Clock::time_point accepted_at = Clock::now();
        bool admitted = false;
    };

    NET_Server* server = nullptr;
    bool net_initialized = false;
    TransportHostConfig config;
    std::vector<Peer> peers;
    std::vector<TransportEvent> events;
    std::uint32_t sequence = 1;

    std::size_t AdmittedCount() const {
        return static_cast<std::size_t>(std::count_if(
            peers.begin(), peers.end(), [](const Peer& peer) { return peer.admitted; }));
    }

    VirtualPlayerId AllocateId() const {
        for (VirtualPlayerId id = 2; id <= config.maximum_players; ++id) {
            const bool used = std::any_of(peers.begin(), peers.end(),
                [id](const Peer& peer) { return peer.admitted && peer.id == id; });
            if (!used) return id;
        }
        return 0;
    }

    bool Send(Peer& peer, TransportFrameKind kind, VirtualPlayerId source,
              VirtualPlayerId destination, const std::vector<std::uint8_t>& payload,
              std::string* error) {
        TransportFrame frame{kind, 0, source, destination, sequence++, payload};
        return WriteFrame(peer.socket, frame, error);
    }

    void NotifyJoined(const Peer& joined) {
        const std::vector<std::uint8_t> payload =
            EncodePlayerNotice({joined.id, joined.name});
        for (Peer& peer : peers) {
            if (peer.admitted && peer.id != joined.id) {
                std::string ignored;
                (void)Send(peer, TransportFrameKind::PlayerJoined, kHostPlayerId,
                           peer.id, payload, &ignored);
            }
        }
    }

    void NotifyLeft(VirtualPlayerId id, const std::string& name) {
        const std::vector<std::uint8_t> payload = EncodePlayerNotice({id, name});
        for (Peer& peer : peers) {
            if (peer.admitted && peer.id != id) {
                std::string ignored;
                (void)Send(peer, TransportFrameKind::PlayerLeft, kHostPlayerId,
                           peer.id, payload, &ignored);
            }
        }
    }

    bool Reject(Peer& peer, const std::string& reason) {
        std::string ignored;
        (void)Send(peer, TransportFrameKind::Rejected, kHostPlayerId, 0,
                   RejectionPayload(reason), &ignored);
        (void)NET_WaitUntilStreamSocketDrained(peer.socket, 100);
        events.push_back(ErrorEvent(reason));
        return false;
    }

    bool HandleHello(Peer& peer, const TransportFrame& frame) {
        if (frame.kind != TransportFrameKind::ClientHello || frame.source != 0 ||
            frame.destination != kHostPlayerId) {
            return Reject(peer, "first client frame is not a valid hello");
        }
        ClientHello hello;
        std::string error;
        if (!DecodeClientHello(frame.payload, &hello, &error)) return Reject(peer, error);
        if (!hello.expected_session_uuid.empty() &&
            hello.expected_session_uuid != config.session_uuid) {
            return Reject(peer, "stale or wrong session UUID");
        }
        const VirtualPlayerId id = AllocateId();
        if (id == 0 || 1 + AdmittedCount() >= config.maximum_players) {
            return Reject(peer, "session is full");
        }

        peer.id = id;
        peer.name = std::move(hello.player_name);
        peer.admitted = true;
        const ServerWelcome welcome{
            id, kHostPlayerId,
            static_cast<std::uint8_t>(1 + AdmittedCount()),
            config.maximum_players,
            config.session_uuid,
            config.host_player_name,
        };
        if (!Send(peer, TransportFrameKind::ServerWelcome, kHostPlayerId, id,
                  EncodeServerWelcome(welcome), &error)) {
            events.push_back(ErrorEvent(error));
            return false;
        }

        for (const Peer& existing : peers) {
            if (existing.admitted && existing.id != peer.id) {
                if (!Send(peer, TransportFrameKind::PlayerJoined, kHostPlayerId,
                          peer.id, EncodePlayerNotice({existing.id, existing.name}), &error)) {
                    events.push_back(ErrorEvent(error));
                    return false;
                }
            }
        }
        NotifyJoined(peer);
        TransportEvent event;
        event.kind = TransportEventKind::PlayerJoined;
        event.player_id = peer.id;
        event.message = peer.name;
        events.push_back(std::move(event));
        return true;
    }

    Peer* FindPeer(VirtualPlayerId id) {
        auto found = std::find_if(peers.begin(), peers.end(),
            [id](Peer& peer) { return peer.admitted && peer.id == id; });
        return found == peers.end() ? nullptr : &*found;
    }

    bool HandleLegacy(Peer& peer, const TransportFrame& frame) {
        std::string error;
        if (frame.source != peer.id ||
            (frame.destination != kBroadcastPlayerId &&
             frame.destination != kHostPlayerId && !FindPeer(frame.destination)) ||
            !ValidateLegacyPayload(frame.payload, &error)) {
            return Reject(peer, error.empty() ? "invalid legacy routing envelope" : error);
        }

        if (frame.destination == kBroadcastPlayerId ||
            frame.destination == kHostPlayerId) {
            TransportEvent event;
            event.kind = TransportEventKind::LegacyPayload;
            event.sender = peer.id;
            event.destination = frame.destination;
            event.payload = frame.payload;
            events.push_back(std::move(event));
        }
        if (frame.destination == kBroadcastPlayerId) {
            for (Peer& destination : peers) {
                if (destination.admitted && destination.id != peer.id) {
                    if (!Send(destination, TransportFrameKind::LegacyPayload, peer.id,
                              kBroadcastPlayerId, frame.payload, &error)) {
                        events.push_back(ErrorEvent(error));
                    }
                }
            }
        } else if (frame.destination != kHostPlayerId) {
            Peer* destination = FindPeer(frame.destination);
            if (!destination || !Send(*destination, TransportFrameKind::LegacyPayload,
                                      peer.id, frame.destination, frame.payload, &error)) {
                events.push_back(ErrorEvent(error.empty() ? "destination disappeared" : error));
            }
        }
        return true;
    }

    bool Handle(Peer& peer, const TransportFrame& frame) {
        if (!peer.admitted) return HandleHello(peer, frame);
        if (frame.kind != TransportFrameKind::LegacyPayload) {
            return Reject(peer, "client sent a server-only frame kind");
        }
        return HandleLegacy(peer, frame);
    }

    bool ReadPeer(Peer& peer) {
        std::array<std::uint8_t, kReadChunkSize> buffer{};
        for (;;) {
            const int received = NET_ReadFromStreamSocket(
                peer.socket, buffer.data(), static_cast<int>(buffer.size()));
            if (received < 0) return false;
            if (received == 0) break;
            std::vector<TransportFrame> frames;
            std::string error;
            if (!peer.decoder.Append(buffer.data(), static_cast<std::size_t>(received),
                                     &frames, &error)) {
                events.push_back(ErrorEvent(error));
                return false;
            }
            for (const TransportFrame& frame : frames) {
                if (!Handle(peer, frame)) return false;
            }
        }
        if (!peer.admitted && Clock::now() - peer.accepted_at > kHandshakeTimeout) {
            events.push_back(ErrorEvent("client handshake timed out"));
            return false;
        }
        const int pending = NET_GetStreamSocketPendingWrites(peer.socket);
        if (pending < 0 || pending > kMaximumPendingTransportBytes) {
            events.push_back(ErrorEvent(pending < 0 ? NetError("peer write queue failed")
                                                    : "peer write queue exceeded limit"));
            return false;
        }
        return true;
    }

    void Drop(std::size_t index, bool notify) {
        Peer& peer = peers[index];
        const bool was_admitted = peer.admitted;
        const VirtualPlayerId id = peer.id;
        const std::string name = peer.name;
        DrainAndDestroy(peer.socket);
        peers.erase(peers.begin() + static_cast<std::ptrdiff_t>(index));
        if (was_admitted) {
            if (notify) NotifyLeft(id, name);
            TransportEvent event;
            event.kind = TransportEventKind::PlayerLeft;
            event.player_id = id;
            event.message = name;
            events.push_back(std::move(event));
        }
    }
};

SdlNetTransportHost::SdlNetTransportHost() : impl_(std::make_unique<Impl>()) {}
SdlNetTransportHost::~SdlNetTransportHost() { Stop(); }

bool SdlNetTransportHost::Start(const TransportHostConfig& requested,
                                std::string* error) {
    Stop();
    if (requested.port == 0 || requested.maximum_players < 2 ||
        requested.maximum_players > 9 || requested.host_player_name.empty() ||
        requested.host_player_name.size() > 11) {
        if (error) *error = "invalid host transport configuration";
        return false;
    }
    impl_->config = requested;
    if (impl_->config.session_uuid.empty()) impl_->config.session_uuid = GenerateSessionUuid();
    if (impl_->config.session_uuid.size() != 36) {
        if (error) *error = "session UUID must contain 36 characters";
        return false;
    }
    if (!NET_Init()) {
        if (error) *error = NetError("NET_Init failed");
        return false;
    }
    impl_->net_initialized = true;
    impl_->server = NET_CreateServer(nullptr, impl_->config.port, 0);
    if (!impl_->server) {
        if (error) *error = NetError("NET_CreateServer failed");
        Stop();
        return false;
    }
    TransportEvent event;
    event.kind = TransportEventKind::Listening;
    event.player_id = kHostPlayerId;
    impl_->events.push_back(std::move(event));
    return true;
}

std::vector<TransportEvent> SdlNetTransportHost::Poll() {
    if (impl_->server) {
        for (;;) {
            NET_StreamSocket* socket = nullptr;
            if (!NET_AcceptClient(impl_->server, &socket)) {
                impl_->events.push_back(ErrorEvent(NetError("NET_AcceptClient failed")));
                break;
            }
            if (!socket) break;
            Impl::Peer peer;
            peer.socket = socket;
            peer.accepted_at = Clock::now();
            impl_->peers.push_back(std::move(peer));
        }
        for (std::size_t index = 0; index < impl_->peers.size();) {
            if (!impl_->ReadPeer(impl_->peers[index])) {
                impl_->Drop(index, true);
            } else {
                ++index;
            }
        }
    }
    std::vector<TransportEvent> result;
    result.swap(impl_->events);
    return result;
}

bool SdlNetTransportHost::SendLegacy(VirtualPlayerId destination,
                                     const std::vector<std::uint8_t>& payload,
                                     std::string* error) {
    if (!impl_->server || !ValidateLegacyPayload(payload, error)) return false;
    bool ok = true;
    if (destination == kBroadcastPlayerId) {
        for (Impl::Peer& peer : impl_->peers) {
            if (peer.admitted && !impl_->Send(peer, TransportFrameKind::LegacyPayload,
                                             kHostPlayerId, destination, payload, error)) ok = false;
        }
    } else if (destination != kHostPlayerId) {
        Impl::Peer* peer = impl_->FindPeer(destination);
        if (!peer) {
            if (error) *error = "unknown destination player ID";
            return false;
        }
        ok = impl_->Send(*peer, TransportFrameKind::LegacyPayload, kHostPlayerId,
                         destination, payload, error);
    }
    return ok;
}

void SdlNetTransportHost::Stop() {
    if (!impl_) return;
    while (!impl_->peers.empty()) impl_->Drop(impl_->peers.size() - 1, false);
    if (impl_->server) {
        NET_DestroyServer(impl_->server);
        impl_->server = nullptr;
    }
    if (impl_->net_initialized) {
        NET_Quit();
        impl_->net_initialized = false;
    }
    impl_->events.clear();
    impl_->sequence = 1;
}

bool SdlNetTransportHost::running() const noexcept { return impl_->server != nullptr; }
std::uint16_t SdlNetTransportHost::port() const noexcept { return impl_->config.port; }
std::uint8_t SdlNetTransportHost::player_count() const noexcept {
    return static_cast<std::uint8_t>(impl_->server ? 1 + impl_->AdmittedCount() : 0);
}
const std::string& SdlNetTransportHost::session_uuid() const noexcept {
    return impl_->config.session_uuid;
}

struct SdlNetTransportClient::Impl {
    NET_Address* address = nullptr;
    NET_StreamSocket* socket = nullptr;
    bool net_initialized = false;
    bool hello_sent = false;
    bool is_admitted = false;
    bool terminal = false;
    TransportClientConfig config;
    TransportFrameDecoder decoder;
    std::vector<TransportEvent> events;
    VirtualPlayerId id = 0;
    std::string connected_session_uuid;
    Clock::time_point started_at = Clock::now();
    std::uint32_t sequence = 1;

    bool Send(TransportFrameKind kind, VirtualPlayerId source,
              VirtualPlayerId destination, const std::vector<std::uint8_t>& payload,
              std::string* error) {
        return WriteFrame(socket, {kind, 0, source, destination, sequence++, payload}, error);
    }

    bool Fail(const std::string& error) {
        if (terminal) return false;
        terminal = true;
        events.push_back(ErrorEvent(error));
        TransportEvent disconnected;
        disconnected.kind = TransportEventKind::Disconnected;
        disconnected.player_id = id;
        disconnected.message = error;
        events.push_back(std::move(disconnected));
        return false;
    }

    bool Handle(const TransportFrame& frame) {
        std::string error;
        if (!is_admitted) {
            if (frame.kind == TransportFrameKind::Rejected) {
                return Fail(std::string(frame.payload.begin(), frame.payload.end()));
            }
            ServerWelcome welcome;
            if (frame.kind != TransportFrameKind::ServerWelcome ||
                frame.source != kHostPlayerId ||
                !DecodeServerWelcome(frame.payload, &welcome, &error) ||
                frame.destination != welcome.assigned_player_id ||
                (!config.expected_session_uuid.empty() &&
                 config.expected_session_uuid != welcome.session_uuid)) {
                return Fail(error.empty() ? "invalid server welcome envelope" : error);
            }
            id = welcome.assigned_player_id;
            connected_session_uuid = std::move(welcome.session_uuid);
            is_admitted = true;
            TransportEvent event;
            event.kind = TransportEventKind::Connected;
            event.player_id = id;
            events.push_back(std::move(event));
            TransportEvent host_joined;
            host_joined.kind = TransportEventKind::PlayerJoined;
            host_joined.player_id = kHostPlayerId;
            host_joined.message = std::move(welcome.host_player_name);
            events.push_back(std::move(host_joined));
            return true;
        }

        if (frame.source == 0 ||
            (frame.destination != id && frame.destination != kBroadcastPlayerId)) {
            return Fail("server frame has invalid virtual routing IDs");
        }
        if (frame.kind == TransportFrameKind::LegacyPayload) {
            if (!ValidateLegacyPayload(frame.payload, &error)) return Fail(error);
            TransportEvent event;
            event.kind = TransportEventKind::LegacyPayload;
            event.sender = frame.source;
            event.destination = frame.destination;
            event.payload = frame.payload;
            events.push_back(std::move(event));
            return true;
        }
        if (frame.kind == TransportFrameKind::PlayerJoined ||
            frame.kind == TransportFrameKind::PlayerLeft) {
            PlayerNotice notice;
            if (!DecodePlayerNotice(frame.payload, &notice, &error)) return Fail(error);
            TransportEvent event;
            event.kind = frame.kind == TransportFrameKind::PlayerJoined
                ? TransportEventKind::PlayerJoined : TransportEventKind::PlayerLeft;
            event.player_id = notice.player_id;
            event.message = std::move(notice.player_name);
            events.push_back(std::move(event));
            return true;
        }
        if (frame.kind == TransportFrameKind::SessionEnded ||
            frame.kind == TransportFrameKind::Rejected) {
            return Fail(std::string(frame.payload.begin(), frame.payload.end()));
        }
        return Fail("unexpected server frame kind");
    }
};

SdlNetTransportClient::SdlNetTransportClient() : impl_(std::make_unique<Impl>()) {}
SdlNetTransportClient::~SdlNetTransportClient() { Stop(); }

bool SdlNetTransportClient::Connect(const TransportClientConfig& config,
                                    std::string* error) {
    Stop();
    if (config.host.empty() || config.port == 0 || config.player_name.empty() ||
        config.player_name.size() > 11 ||
        (!config.expected_session_uuid.empty() && config.expected_session_uuid.size() != 36)) {
        if (error) *error = "invalid client transport configuration";
        return false;
    }
    impl_->config = config;
    if (!NET_Init()) {
        if (error) *error = NetError("NET_Init failed");
        return false;
    }
    impl_->net_initialized = true;
    impl_->address = NET_ResolveHostname(config.host.c_str());
    if (!impl_->address) {
        if (error) *error = NetError("hostname resolution could not start");
        Stop();
        return false;
    }
    impl_->started_at = Clock::now();
    return true;
}

std::vector<TransportEvent> SdlNetTransportClient::Poll() {
    if (impl_->terminal) {
        std::vector<TransportEvent> result;
        result.swap(impl_->events);
        return result;
    }
    if (impl_->address && !impl_->socket) {
        const NET_Status resolved = NET_GetAddressStatus(impl_->address);
        if (resolved == NET_SUCCESS) {
            impl_->socket = NET_CreateClient(impl_->address, impl_->config.port, 0);
            if (!impl_->socket) impl_->Fail(NetError("NET_CreateClient failed"));
        } else if (resolved == NET_FAILURE) {
            impl_->Fail(NetError("hostname resolution failed"));
        }
    }

    if (impl_->socket && !impl_->hello_sent) {
        const NET_Status connected = NET_GetConnectionStatus(impl_->socket);
        if (connected == NET_SUCCESS) {
            std::string error;
            const std::vector<std::uint8_t> hello = EncodeClientHello(
                {impl_->config.player_name, impl_->config.expected_session_uuid});
            if (hello.empty() || !impl_->Send(TransportFrameKind::ClientHello, 0,
                                              kHostPlayerId, hello, &error)) {
                impl_->Fail(error.empty() ? "failed to encode client hello" : error);
            } else {
                impl_->hello_sent = true;
            }
        } else if (connected == NET_FAILURE) {
            impl_->Fail(NetError("connection failed"));
        }
    }

    if (impl_->socket && impl_->hello_sent) {
        std::array<std::uint8_t, kReadChunkSize> buffer{};
        bool keep = true;
        for (;;) {
            const int received = NET_ReadFromStreamSocket(
                impl_->socket, buffer.data(), static_cast<int>(buffer.size()));
            if (received < 0) { keep = impl_->Fail(NetError("server disconnected")); break; }
            if (received == 0) break;
            std::vector<TransportFrame> frames;
            std::string error;
            if (!impl_->decoder.Append(buffer.data(), static_cast<std::size_t>(received),
                                       &frames, &error)) {
                keep = impl_->Fail(error);
                break;
            }
            for (const TransportFrame& frame : frames) {
                if (!impl_->Handle(frame)) { keep = false; break; }
            }
            if (!keep) break;
        }
        if (keep) {
            const int pending = NET_GetStreamSocketPendingWrites(impl_->socket);
            if (pending < 0 || pending > kMaximumPendingTransportBytes) {
                impl_->Fail(pending < 0 ? NetError("client write queue failed")
                                        : "client write queue exceeded limit");
            }
        }
    }

    if ((impl_->address || impl_->socket) && !impl_->is_admitted &&
        Clock::now() - impl_->started_at > kHandshakeTimeout) {
        impl_->Fail("connection handshake timed out");
    }

    std::vector<TransportEvent> result;
    result.swap(impl_->events);
    return result;
}

bool SdlNetTransportClient::SendLegacy(VirtualPlayerId destination,
                                       const std::vector<std::uint8_t>& payload,
                                       std::string* error) {
    if (!impl_->is_admitted) {
        if (error) *error = "client has not completed the handshake";
        return false;
    }
    if (!ValidateLegacyPayload(payload, error)) return false;
    return impl_->Send(TransportFrameKind::LegacyPayload, impl_->id, destination,
                       payload, error);
}

void SdlNetTransportClient::Stop() {
    if (!impl_) return;
    DrainAndDestroy(impl_->socket);
    if (impl_->address) {
        NET_UnrefAddress(impl_->address);
        impl_->address = nullptr;
    }
    if (impl_->net_initialized) {
        NET_Quit();
        impl_->net_initialized = false;
    }
    impl_->hello_sent = false;
    impl_->is_admitted = false;
    impl_->terminal = false;
    impl_->id = 0;
    impl_->connected_session_uuid.clear();
    impl_->decoder = TransportFrameDecoder{};
    impl_->events.clear();
    impl_->sequence = 1;
}

bool SdlNetTransportClient::active() const noexcept {
    return impl_->address != nullptr || impl_->socket != nullptr;
}
bool SdlNetTransportClient::admitted() const noexcept { return impl_->is_admitted; }
VirtualPlayerId SdlNetTransportClient::player_id() const noexcept { return impl_->id; }
const std::string& SdlNetTransportClient::session_uuid() const noexcept {
    return impl_->connected_session_uuid;
}

}  // namespace lego_loco::network

#endif  // !_WIN32
