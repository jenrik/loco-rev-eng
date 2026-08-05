#include "embedded_mdns_discovery.h"
#include "sdl3_net_transport.h"

#ifndef _WIN32

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace lego_loco::network;
using namespace std::chrono_literals;

namespace {
constexpr const char* kUuid = "fedcba98-7654-4321-8765-0123456789ab";
const std::vector<std::uint8_t> kPacket = {0x06, 0x00, 0x2c, 0x01, 0x55};

SessionAdvertisement Advertisement(std::uint16_t port) {
    SessionAdvertisement ad;
    ad.session_uuid = kUuid;
    ad.display_name = "Reachable Lego Loco";
    ad.transport_version = kTransportVersion;
    ad.legacy_version = kLegacyProtocolVersion;
    ad.current_players = 1;
    ad.max_players = 9;
    ad.tcp_port = port;
    return ad;
}

int Server(std::uint16_t port) {
    SdlNetTransportHost host;
    std::string error;
    if (!host.Start({port, 9, "Host", kUuid}, &error)) {
        std::cerr << error << "\n";
        return 2;
    }
    std::unique_ptr<IDiscoveryBackend> publisher = EmbeddedMdnsDiscoveryFactory().create();
    BackendStartResult started = publisher->Start({false, Advertisement(port)});
    if (!started.ok) {
        std::cerr << started.error << "\n";
        return 3;
    }
    std::cout << "PUBLISHED uuid=" << kUuid << " port=" << port << std::endl;

    bool joined = false;
    bool payload = false;
    const auto deadline = std::chrono::steady_clock::now() + 12s;
    while (!(joined && payload) && std::chrono::steady_clock::now() < deadline) {
        BackendPollResult discovery = publisher->Poll();
        if (discovery.health != BackendHealth::Running) return 4;
        for (const TransportEvent& event : host.Poll()) {
            if (event.kind == TransportEventKind::Error) {
                std::cerr << event.message << "\n";
                return 5;
            }
            if (event.kind == TransportEventKind::PlayerJoined) joined = true;
            if (event.kind == TransportEventKind::LegacyPayload) {
                assert(event.sender == 2 && event.payload == kPacket);
                payload = true;
                assert(host.SendLegacy(2, event.payload, &error));
            }
        }
        std::this_thread::sleep_for(5ms);
    }
    publisher->Stop();
    host.Stop();
    if (!(joined && payload)) return 6;
    std::cout << "PASS published listener admitted discovered client" << std::endl;
    return 0;
}

int Client() {
    std::unique_ptr<IDiscoveryBackend> browser = EmbeddedMdnsDiscoveryFactory().create();
    BackendStartResult started = browser->Start({true, std::nullopt});
    if (!started.ok) return 7;

    DiscoveredSession found;
    bool discovered = false;
    const auto discovery_deadline = std::chrono::steady_clock::now() + 8s;
    while (!discovered && std::chrono::steady_clock::now() < discovery_deadline) {
        BackendPollResult poll = browser->Poll();
        if (poll.health != BackendHealth::Running) return 8;
        for (const BackendEvent& event : poll.events) {
            if ((event.kind == BackendEventKind::Added ||
                 event.kind == BackendEventKind::Updated) &&
                event.session && event.session->metadata.session_uuid == kUuid) {
                found = *event.session;
                for (const EndpointCandidate& candidate : found.endpoints) {
                    if (candidate.host_or_numeric_address.find('.') != std::string::npos &&
                        candidate.host_or_numeric_address.find(".local.") == std::string::npos) {
                        found.endpoints = {candidate};
                        discovered = true;
                        break;
                    }
                }
            }
        }
        std::this_thread::sleep_for(5ms);
    }
    browser->Stop();
    if (!discovered) return 9;
    assert(found.metadata.transport_version == kTransportVersion);
    assert(found.metadata.legacy_version == kLegacyProtocolVersion);
    const EndpointCandidate endpoint = found.endpoints.front();
    std::cout << "DISCOVERED host=" << endpoint.host_or_numeric_address
              << " port=" << endpoint.port << std::endl;

    SdlNetTransportClient client;
    std::string error;
    assert(client.Connect({endpoint.host_or_numeric_address, endpoint.port,
                           "Client", found.metadata.session_uuid}, &error));
    bool sent = false;
    bool echoed = false;
    const auto deadline = std::chrono::steady_clock::now() + 8s;
    while (!echoed && std::chrono::steady_clock::now() < deadline) {
        for (const TransportEvent& event : client.Poll()) {
            if (event.kind == TransportEventKind::Error) {
                std::cerr << event.message << "\n";
                return 10;
            }
            if (event.kind == TransportEventKind::Connected && !sent) {
                assert(event.player_id == 2);
                assert(client.SendLegacy(kHostPlayerId, kPacket, &error));
                sent = true;
            }
            if (event.kind == TransportEventKind::LegacyPayload) {
                assert(event.sender == kHostPlayerId && event.payload == kPacket);
                echoed = true;
            }
        }
        std::this_thread::sleep_for(5ms);
    }
    client.Stop();
    if (!echoed) return 11;
    std::cout << "PASS discovered endpoint completed authoritative handshake" << std::endl;
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    if (std::string(argv[1]) == "--client") return Client();
    if (std::string(argv[1]) == "--server" && argc == 3) {
        const long port = std::strtol(argv[2], nullptr, 10);
        if (port <= 0 || port > 65535) return 1;
        return Server(static_cast<std::uint16_t>(port));
    }
    return 1;
}

#endif  // !_WIN32
