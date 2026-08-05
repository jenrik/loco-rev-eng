#include "sdl3_net_transport.h"

#ifndef _WIN32

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace lego_loco::network;
using namespace std::chrono_literals;

namespace {
constexpr const char* kSessionUuid = "12345678-1234-4234-9234-123456789abc";
const std::vector<std::uint8_t> kLegacyPacket = {0x06, 0x00, 0x2c, 0x01, 0xde, 0xad};

bool Before(const std::chrono::steady_clock::time_point& deadline) {
    std::this_thread::sleep_for(2ms);
    return std::chrono::steady_clock::now() < deadline;
}

int RunServer(std::uint16_t port) {
    SdlNetTransportHost host;
    std::string error;
    if (!host.Start({port, 9, "Host", kSessionUuid}, &error)) {
        std::cerr << "server start failed: " << error << "\n";
        return 2;
    }
    std::cout << "READY port=" << port << " uuid=" << host.session_uuid() << std::endl;

    bool joined = false;
    bool received = false;
    bool left = false;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    do {
        for (TransportEvent& event : host.Poll()) {
            if (event.kind == TransportEventKind::Error) {
                std::cerr << "server transport error: " << event.message << "\n";
                return 3;
            }
            if (event.kind == TransportEventKind::PlayerJoined) {
                assert(event.player_id == 2);
                assert(event.message == "Client");
                joined = true;
                std::cout << "JOIN id=" << event.player_id << std::endl;
            }
            if (event.kind == TransportEventKind::LegacyPayload) {
                assert(joined);
                assert(event.sender == 2);
                assert(event.destination == kHostPlayerId);
                assert(event.payload == kLegacyPacket);
                received = true;
                if (!host.SendLegacy(2, event.payload, &error)) {
                    std::cerr << "server echo failed: " << error << "\n";
                    return 4;
                }
                std::cout << "PAYLOAD sender=" << event.sender << std::endl;
            }
            if (event.kind == TransportEventKind::PlayerLeft) {
                assert(event.player_id == 2);
                left = true;
                std::cout << "LEAVE id=" << event.player_id << std::endl;
            }
        }
    } while (!(joined && received && left) && Before(deadline));

    host.Stop();
    if (!(joined && received && left)) {
        std::cerr << "server timed out: joined=" << joined << " received=" << received
                  << " left=" << left << "\n";
        return 5;
    }
    std::cout << "PASS server handshake, route, and leave" << std::endl;
    return 0;
}

int RunAdmissionServer(std::uint16_t port) {
    SdlNetTransportHost server;
    std::string error;
    if (!server.Start({port, 9, "Host", kSessionUuid}, &error)) {
        std::cerr << "admission server start failed: " << error << "\n";
        return 13;
    }
    std::cout << "ADMISSION_READY " << port << std::endl;
    VirtualPlayerId joined_id = 0;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    do {
        for (const TransportEvent& event : server.Poll()) {
            if (event.kind == TransportEventKind::Error) {
                std::cerr << "admission server failed: " << event.message << "\n";
                return 14;
            }
            if (event.kind == TransportEventKind::PlayerJoined) joined_id = event.player_id;
        }
    } while (joined_id == 0 && Before(deadline));
    if (joined_id == 0) return 15;
    std::cout << "ADMISSION_JOIN id=" << joined_id << std::endl;
    const std::vector<std::uint8_t> ping_packet = {
        0xf6, 0x03, 0x2c, 0x01, 1, 0, 1, 0, 1,
        0x34, 0x12, 7, 0, 8, 0, 1, 1, 0,
    };
    if (!server.SendLegacy(joined_id, ping_packet, &error)) {
        std::cerr << "admission ping send failed: " << error << "\n";
        return 18;
    }
    const auto flush_deadline = std::chrono::steady_clock::now() + 500ms;
    do {
        for (const TransportEvent& event : server.Poll()) {
            if (event.kind == TransportEventKind::Error) {
                std::cerr << "admission ping flush failed: " << event.message << "\n";
                return 19;
            }
        }
    } while (Before(flush_deadline));
    server.Stop();
    return 0;
}

int RunAdmissionClient(std::uint16_t port) {
    SdlNetTransportClient client;
    std::string error;
    if (!client.Connect({"127.0.0.1", port, "Client", {}}, &error)) {
        std::cerr << "admission connect start failed: " << error << "\n";
        return 10;
    }
    VirtualPlayerId assigned_id = 0;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    do {
        for (const TransportEvent& event : client.Poll()) {
            if (event.kind == TransportEventKind::Error) {
                std::cerr << "admission client failed: " << event.message << "\n";
                return 11;
            }
            if (event.kind == TransportEventKind::Connected) {
                assigned_id = event.player_id;
                std::vector<std::vector<std::uint8_t>> game_packets = {
                    {0xea, 0x03, 0x2c, 0x01, 0x78, 0x56, 0x34, 0x12, 1},
                    {0xeb, 0x03, 0x2c, 0x01, 0, 0, 0, 0, 0,
                     'l', 'o', 'c', 'a', 'l', 0},
                    {0xec, 0x03, 0x2c, 0x01, 0x78, 0x56, 0x34, 0x12,
                     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                    {0xed, 0x03, 0x2c, 0x01, 2, 7},
                    {0xee, 0x03, 0x2c, 0x01, 2, 7, 0, 0,
                     4, 0, 0, 0, 'L', 'O', 'C', 'O'},
                    {0xee, 0x03, 0x2c, 0x01, 2, 7, 0, 0,
                     3, 0, 0, 0, 'X', 'Y', 'Z'},
                    {0xef, 0x03, 0x2c, 0x01},
                    {0xfc, 0x03, 0x2c, 0x01, 2, 0, 0, 0,
                     0x55, 0, 0, 0, 0, 'A', 'B', 0, 0, 0},
                    {0xfc, 0x03, 0x2c, 0x01, 2, 0, 0, 0,
                     0x55, 0, 1, 0, 1, 'C', 'D', 0, 0, 0},
                    {0xfc, 0x03, 0x2c, 0x01, 3, 0, 0, 0,
                     0x55, 0, 2, 0, 2, 'E', 'F', 'G', 0, 0, 0},
                    {0xf4, 0x03, 0x2c, 0x01},
                    {0xf6, 0x03, 0x2c, 0x01, 1, 0, 1, 0, 1,
                     0x34, 0x12, 7, 0, 8, 0, 1, 1, 0},
                    {0xf7, 0x03, 0x2c, 0x01,
                     0x34, 0x12, 0, 0, 8, 7, 0, 0},
                    {0xf9, 0x03, 0x2c, 0x01, 0, 0, 2, 0,
                     0, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0,
                     0x11, 0x22, 0x33, 0x44},
                };
                std::vector<std::uint8_t> track_sessions(0x14 + 0x390, 0);
                track_sessions[0] = 0xec; track_sessions[1] = 0x03;
                track_sessions[2] = 0x2c; track_sessions[3] = 0x01;
                track_sessions[4] = 0x78; track_sessions[5] = 0x56;
                track_sessions[6] = 0x34; track_sessions[7] = 0x12;
                track_sessions[8] = 1; track_sessions[12] = 1;
                track_sessions[0x14 + 0x1c] = 1;
                track_sessions[0x14 + 0x8e] = 0x80;
                track_sessions[0x14 + 0x90] = 7;
                track_sessions[0x14 + 0x91] = 2;
                game_packets.insert(game_packets.begin() + 3,
                                    std::move(track_sessions));
                for (const auto& packet : game_packets) {
                    if (!client.SendLegacy(kHostPlayerId, packet, &error)) {
                        std::cerr << "admission game-packet send failed: " << error << "\n";
                        return 16;
                    }
                }
            }
        }
    } while (assigned_id == 0 && Before(deadline));
    if (assigned_id == 0) return 12;
    std::cout << "ADMITTED id=" << assigned_id << std::endl;
    bool received_track_response = false;
    const auto flush_deadline = std::chrono::steady_clock::now() + 500ms;
    do {
        for (const TransportEvent& event : client.Poll()) {
            if (event.kind == TransportEventKind::Error) {
                std::cerr << "admission flush failed: " << event.message << "\n";
                return 17;
            }
            if (event.kind == TransportEventKind::LegacyPayload &&
                event.payload.size() == 0x14 && event.payload[0] == 0xec &&
                event.payload[1] == 0x03) {
                received_track_response = true;
            }
        }
    } while (!received_track_response && Before(flush_deadline));
    if (!received_track_response) {
        std::cerr << "admission client did not receive 0x3EA -> 0x3EC response\n";
        return 20;
    }
    client.Stop();
    return 0;
}

int RunClient(std::uint16_t port) {
    SdlNetTransportClient client;
    std::string error;
    if (!client.Connect({"127.0.0.1", port, "Client", kSessionUuid}, &error)) {
        std::cerr << "client connect start failed: " << error << "\n";
        return 6;
    }

    bool connected = false;
    bool echoed = false;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    do {
        for (TransportEvent& event : client.Poll()) {
            if (event.kind == TransportEventKind::Error) {
                std::cerr << "client transport error: " << event.message << "\n";
                return 7;
            }
            if (event.kind == TransportEventKind::Connected) {
                assert(event.player_id == 2);
                assert(client.session_uuid() == kSessionUuid);
                connected = true;
                if (!client.SendLegacy(kHostPlayerId, kLegacyPacket, &error)) {
                    std::cerr << "client send failed: " << error << "\n";
                    return 8;
                }
                std::cout << "CONNECTED id=" << event.player_id << std::endl;
            }
            if (event.kind == TransportEventKind::LegacyPayload) {
                assert(connected);
                assert(event.sender == kHostPlayerId);
                assert(event.destination == 2);
                assert(event.payload == kLegacyPacket);
                echoed = true;
                std::cout << "ECHO bytes=" << event.payload.size() << std::endl;
            }
        }
    } while (!echoed && Before(deadline));

    client.Stop();
    if (!(connected && echoed)) {
        std::cerr << "client timed out: connected=" << connected << " echoed=" << echoed << "\n";
        return 9;
    }
    std::cout << "PASS client handshake, virtual ID, and framed payload" << std::endl;
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " --server|--client|--admit-client|--admit-server PORT\n";
        return 1;
    }
    const long parsed = std::strtol(argv[2], nullptr, 10);
    if (parsed <= 0 || parsed > 65535) return 1;
    const std::uint16_t port = static_cast<std::uint16_t>(parsed);
    if (std::string(argv[1]) == "--server") return RunServer(port);
    if (std::string(argv[1]) == "--admit-server") return RunAdmissionServer(port);
    if (std::string(argv[1]) == "--client") return RunClient(port);
    if (std::string(argv[1]) == "--admit-client") return RunAdmissionClient(port);
    return 1;
}

#endif  // !_WIN32
