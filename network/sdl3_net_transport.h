#pragma once

#ifndef _WIN32

#include "sdl3_net_protocol.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lego_loco::network {

constexpr std::uint16_t kDefaultLegoLocoPort = 23000;
constexpr int kMaximumPendingTransportBytes = 256 * 1024;

enum class TransportEventKind {
    Listening,
    Connected,
    PlayerJoined,
    PlayerLeft,
    LegacyPayload,
    Disconnected,
    Error,
};

struct TransportEvent {
    TransportEventKind kind = TransportEventKind::Error;
    VirtualPlayerId player_id = 0;
    VirtualPlayerId sender = 0;
    VirtualPlayerId destination = 0;
    std::vector<std::uint8_t> payload;
    std::string message;
};

struct TransportHostConfig {
    std::uint16_t port = kDefaultLegoLocoPort;
    std::uint8_t maximum_players = 9;
    std::string host_player_name = "Host";
    std::string session_uuid;
};

struct TransportClientConfig {
    std::string host;
    std::uint16_t port = kDefaultLegoLocoPort;
    std::string player_name;
    std::string expected_session_uuid;
};

struct DirectConnectEndpoint {
    std::string host;
    std::uint16_t port = kDefaultLegoLocoPort;
};

/** Parse host, host:port, IPv4:port, or [IPv6]:port. Unbracketed IPv6
 * uses the default port so colons are never interpreted ambiguously. */
bool ParseDirectConnectEndpoint(const std::string& text,
                                DirectConnectEndpoint* endpoint,
                                std::string* error);

std::string GenerateSessionUuid();

/** Nonblocking host-relayed SDL_net endpoint. Every method must be called by
 * one owner thread; SDL_net handles never cross this typed boundary. */
class SdlNetTransportHost {
public:
    SdlNetTransportHost();
    ~SdlNetTransportHost();
    SdlNetTransportHost(const SdlNetTransportHost&) = delete;
    SdlNetTransportHost& operator=(const SdlNetTransportHost&) = delete;

    bool Start(const TransportHostConfig& config, std::string* error);
    std::vector<TransportEvent> Poll();
    bool SendLegacy(VirtualPlayerId destination,
                    const std::vector<std::uint8_t>& payload, std::string* error);
    void Stop();

    bool running() const noexcept;
    std::uint16_t port() const noexcept;
    std::uint8_t player_count() const noexcept;
    const std::string& session_uuid() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/** Nonblocking direct/discovered SDL_net client endpoint. */
class SdlNetTransportClient {
public:
    SdlNetTransportClient();
    ~SdlNetTransportClient();
    SdlNetTransportClient(const SdlNetTransportClient&) = delete;
    SdlNetTransportClient& operator=(const SdlNetTransportClient&) = delete;

    bool Connect(const TransportClientConfig& config, std::string* error);
    std::vector<TransportEvent> Poll();
    bool SendLegacy(VirtualPlayerId destination,
                    const std::vector<std::uint8_t>& payload, std::string* error);
    void Stop();

    bool active() const noexcept;
    bool admitted() const noexcept;
    VirtualPlayerId player_id() const noexcept;
    const std::string& session_uuid() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lego_loco::network

#endif  // !_WIN32
