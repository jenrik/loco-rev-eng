#include "sdl3_net_runtime.h"

#ifndef _WIN32

#include <unistd.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace lego_loco::network;
using namespace std::chrono_literals;

namespace {
constexpr const char* kUuid = "abcdefab-cdef-4abc-8def-abcdefabcdef";
const std::vector<std::uint8_t> kPacket = {9, 0, 0x2c, 1, 0x44};

template <typename Predicate>
void WaitFor(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (!predicate()) {
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(2ms);
    }
}

void TestDirectConnectParser() {
    DirectConnectEndpoint endpoint;
    std::string error;
    assert(ParseDirectConnectEndpoint("example.test", &endpoint, &error));
    assert(endpoint.host == "example.test" && endpoint.port == kDefaultLegoLocoPort);
    assert(ParseDirectConnectEndpoint("  192.0.2.4:42424  ", &endpoint, &error));
    assert(endpoint.host == "192.0.2.4" && endpoint.port == 42424);
    assert(ParseDirectConnectEndpoint("[2001:db8::1]:65535", &endpoint, &error));
    assert(endpoint.host == "2001:db8::1" && endpoint.port == 65535);
    assert(ParseDirectConnectEndpoint("2001:db8::2", &endpoint, &error));
    assert(endpoint.host == "2001:db8::2" && endpoint.port == kDefaultLegoLocoPort);
    assert(!ParseDirectConnectEndpoint("[2001:db8::1", &endpoint, &error));
    assert(!ParseDirectConnectEndpoint("host:", &endpoint, &error));
    assert(!ParseDirectConnectEndpoint("host:70000", &endpoint, &error));
    assert(!ParseDirectConnectEndpoint("bad host:23000", &endpoint, &error));
}

void TestWorkerOwnership() {
    const std::uint16_t port = static_cast<std::uint16_t>(
        45000 + (static_cast<unsigned>(getpid()) % 10000));
    TransportWorker worker;
    worker.StartHost({port, 9, "Host", kUuid});
    WaitFor([&] {
        const TransportRuntimeSnapshot snapshot = worker.Snapshot();
        return snapshot.state == TransportRuntimeState::Listening;
    });
    TransportRuntimeSnapshot snapshot = worker.Snapshot();
    assert(snapshot.mode == TransportRuntimeMode::Host);
    assert(snapshot.port == port);
    assert(snapshot.session_uuid == kUuid);
    assert(snapshot.player_count == 1);

    SdlNetTransportClient stale_client;
    std::string error;
    assert(stale_client.Connect({"127.0.0.1", port, "Stale",
                                 "00000000-0000-4000-8000-000000000000"}, &error));
    bool rejected = false;
    WaitFor([&] {
        for (const TransportEvent& event : stale_client.Poll())
            rejected |= event.kind == TransportEventKind::Error &&
                        event.message.find("session UUID") != std::string::npos;
        return rejected;
    });
    stale_client.Stop();
    assert(worker.Snapshot().state == TransportRuntimeState::Listening);

    SdlNetTransportClient client;
    assert(client.Connect({"127.0.0.1", port, "Client", kUuid}, &error));
    WaitFor([&] {
        (void)client.Poll();
        return client.admitted();
    });
    assert(client.player_id() == 2);

    bool joined = false;
    WaitFor([&] {
        for (const TransportEvent& event : worker.TakeEvents()) {
            if (event.kind == TransportEventKind::PlayerJoined && event.player_id == 2) {
                joined = true;
            }
        }
        return joined;
    });
    assert(worker.Snapshot().player_count == 2);

    assert(client.SendLegacy(kHostPlayerId, kPacket, &error));
    bool received = false;
    WaitFor([&] {
        for (const TransportEvent& event : worker.TakeEvents()) {
            if (event.kind == TransportEventKind::LegacyPayload) {
                assert(event.sender == 2 && event.destination == kHostPlayerId);
                assert(event.payload == kPacket);
                received = true;
            }
        }
        return received;
    });

    worker.SendLegacy(2, kPacket);
    bool echoed = false;
    WaitFor([&] {
        for (const TransportEvent& event : client.Poll()) {
            if (event.kind == TransportEventKind::LegacyPayload) {
                assert(event.sender == kHostPlayerId && event.destination == 2);
                assert(event.payload == kPacket);
                echoed = true;
            }
        }
        return echoed;
    });

    client.Stop();
    bool left = false;
    WaitFor([&] {
        for (const TransportEvent& event : worker.TakeEvents()) {
            if (event.kind == TransportEventKind::PlayerLeft && event.player_id == 2) left = true;
        }
        return left;
    });
    worker.StopTransport();
    WaitFor([&] { return worker.Snapshot().state == TransportRuntimeState::Idle; });
}

void TestInboundQueueHighWaterDisconnects() {
    const std::uint16_t port = static_cast<std::uint16_t>(
        56000 + (static_cast<unsigned>(getpid()) % 1000));
    TransportWorker worker;
    worker.StartHost({port, 9, "Host", kUuid});
    WaitFor([&] { return worker.Snapshot().state == TransportRuntimeState::Listening; });

    SdlNetTransportClient client;
    std::string error;
    assert(client.Connect({"127.0.0.1", port, "Flood", kUuid}, &error));
    WaitFor([&] { (void)client.Poll(); return client.admitted(); });
    WaitFor([&] {
        for (const TransportEvent& event : worker.TakeEvents())
            if (event.kind == TransportEventKind::PlayerJoined) return true;
        return false;
    });

    for (int index = 0; index < 1100; ++index)
        assert(client.SendLegacy(kHostPlayerId, kPacket, &error));
    WaitFor([&] {
        (void)client.Poll();
        return worker.Snapshot().state == TransportRuntimeState::Failed;
    });
    const TransportRuntimeSnapshot failed = worker.Snapshot();
    assert(failed.error.find("event queue high-water") != std::string::npos);
    assert(worker.TakeEvents().size() >= 1025);
    client.Stop();
    worker.StopTransport();
}
}  // namespace

int main() {
    TestDirectConnectParser();
    TestWorkerOwnership();
    TestInboundQueueHighWaterDisconnects();
    std::cout << "PASS: dedicated transport worker owns SDL_net host lifecycle and queues\n";
    return 0;
}

#endif  // !_WIN32
