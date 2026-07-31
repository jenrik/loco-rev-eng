#pragma once

#ifndef _WIN32

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lego_loco::network {

using VirtualPlayerId = std::uint32_t;

constexpr std::uint16_t kTransportVersion = 1;
constexpr std::uint16_t kLegacyProtocolVersion = 300;
constexpr std::size_t kTransportHeaderSize = 24;
constexpr std::size_t kMaximumTransportPayload = 64 * 1024;
constexpr VirtualPlayerId kBroadcastPlayerId = 0;
constexpr VirtualPlayerId kHostPlayerId = 1;
constexpr std::array<std::uint8_t, 16> kLegoLocoApplicationId = {
    0xf9, 0xcd, 0x25, 0x46, 0x57, 0x7f, 0x11, 0xd2,
    0x94, 0x26, 0x00, 0xa0, 0x24, 0x4b, 0xda, 0x7a,
};

enum class TransportFrameKind : std::uint8_t {
    ClientHello = 1,
    ServerWelcome = 2,
    LegacyPayload = 3,
    PlayerJoined = 4,
    PlayerLeft = 5,
    Rejected = 6,
    SessionEnded = 7,
};

struct TransportFrame {
    TransportFrameKind kind = TransportFrameKind::Rejected;
    std::uint8_t flags = 0;
    VirtualPlayerId source = 0;
    VirtualPlayerId destination = 0;
    std::uint32_t sequence = 0;
    std::vector<std::uint8_t> payload;
};

struct ClientHello {
    std::string player_name;
    std::string expected_session_uuid;
};

struct ServerWelcome {
    VirtualPlayerId assigned_player_id = 0;
    VirtualPlayerId host_player_id = 0;
    std::uint8_t current_players = 0;
    std::uint8_t maximum_players = 0;
    std::string session_uuid;
    std::string host_player_name;
};

struct PlayerNotice {
    VirtualPlayerId player_id = 0;
    std::string player_name;
};

std::vector<std::uint8_t> EncodeTransportFrame(const TransportFrame& frame);
bool DecodeClientHello(const std::vector<std::uint8_t>& payload, ClientHello* hello,
                       std::string* error);
std::vector<std::uint8_t> EncodeClientHello(const ClientHello& hello);
bool DecodeServerWelcome(const std::vector<std::uint8_t>& payload, ServerWelcome* welcome,
                         std::string* error);
std::vector<std::uint8_t> EncodeServerWelcome(const ServerWelcome& welcome);
bool DecodePlayerNotice(const std::vector<std::uint8_t>& payload, PlayerNotice* notice,
                        std::string* error);
std::vector<std::uint8_t> EncodePlayerNotice(const PlayerNotice& notice);
bool ValidateLegacyPayload(const std::vector<std::uint8_t>& payload, std::string* error);

/** Incremental stream decoder. It owns partial bytes and drains every complete
 * frame after each append; malformed input permanently fails the decoder. */
class TransportFrameDecoder {
public:
    bool Append(const std::uint8_t* data, std::size_t size,
                std::vector<TransportFrame>* frames, std::string* error);
    bool failed() const noexcept { return failed_; }
    std::size_t buffered_bytes() const noexcept { return bytes_.size(); }

private:
    std::vector<std::uint8_t> bytes_;
    bool failed_ = false;
};

}  // namespace lego_loco::network

#endif  // !_WIN32
