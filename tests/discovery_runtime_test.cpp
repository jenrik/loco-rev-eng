#include "discovery_runtime.h"

#ifndef _WIN32

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace lego_loco::network;
using namespace std::chrono_literals;

namespace lego_loco::network {
// discovery_runtime.cpp references the process-wide registry even though this
// test constructs its worker with injected factories.
std::vector<DiscoveryBackendFactory> PlatformDiscoveryBackendFactories() {
    return {};
}
}  // namespace lego_loco::network

namespace {

struct FakeState {
    std::atomic<int> starts{0};
    std::atomic<int> stops{0};
    std::atomic<int> updates{0};
    std::atomic<bool> emitted{false};
};

class FakeBackend final : public IDiscoveryBackend {
public:
    explicit FakeBackend(FakeState& state) : state_(state) {}
    DiscoveryBackendKind kind() const noexcept override {
        return DiscoveryBackendKind::AvahiDbus;
    }
    BackendStartResult Start(const DiscoveryRequest&) override {
        ++state_.starts;
        return {true, {}};
    }
    BackendUpdateResult Update(const SessionAdvertisement&) override {
        ++state_.updates;
        return {true, false, {}};
    }
    BackendPollResult Poll() override {
        if (state_.emitted.exchange(true)) {
            return {};
        }
        SessionAdvertisement metadata;
        metadata.session_uuid = "12345678-1234-1234-1234-123456789abc";
        metadata.display_name = "Worker session";
        metadata.transport_version = 1;
        metadata.legacy_version = 300;
        metadata.current_players = 1;
        metadata.max_players = 9;
        metadata.tcp_port = 42424;
        DiscoveredSession session{metadata, {{"192.0.2.20", 42424, 0}}};
        return {
            BackendHealth::Running,
            {
                {BackendEventKind::Added, metadata.session_uuid, session, {}},
                {BackendEventKind::Ready, {}, std::nullopt, {}},
            },
            {},
        };
    }
    void Stop() noexcept override { ++state_.stops; }

private:
    FakeState& state_;
};

DiscoveryRuntimeSnapshot WaitFor(
    DiscoveryWorker& worker,
    bool (*predicate)(const DiscoveryRuntimeSnapshot&)) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        DiscoveryRuntimeSnapshot snapshot = worker.Snapshot();
        if (predicate(snapshot)) {
            return snapshot;
        }
        std::this_thread::sleep_for(5ms);
    }
    assert(false && "timed out waiting for discovery worker state");
    return {};
}

bool HasReadySession(const DiscoveryRuntimeSnapshot& snapshot) {
    return snapshot.active && snapshot.browse_ready && snapshot.sessions.size() == 1;
}

bool IsStopped(const DiscoveryRuntimeSnapshot& snapshot) {
    return !snapshot.active && snapshot.sessions.empty();
}

void TestWorkerLifecycle() {
    FakeState state;
    DiscoveryBackendFactory factory{
        DiscoveryBackendKind::AvahiDbus,
        [&state] { return std::make_unique<FakeBackend>(state); },
    };
    DiscoveryWorker worker({factory}, 50ms);
    worker.Start({true, std::nullopt});

    DiscoveryRuntimeSnapshot ready = WaitFor(worker, &HasReadySession);
    assert(ready.backend == DiscoveryBackendKind::AvahiDbus);
    assert(ready.sessions[0].metadata.display_name == "Worker session");
    assert(state.starts == 1);

    SessionAdvertisement update = ready.sessions[0].metadata;
    update.current_players = 2;
    worker.UpdatePublication(update);
    for (int attempt = 0; attempt < 200 && state.updates == 0; ++attempt) {
        std::this_thread::sleep_for(5ms);
    }
    assert(state.updates == 1);

    worker.StopDiscovery();
    WaitFor(worker, &IsStopped);
    assert(state.stops == 1);
}

}  // namespace

int main() {
    TestWorkerLifecycle();
    std::cout << "PASS: discovery worker command, snapshot, update, and stop lifecycle\n";
    return 0;
}

#endif  // !_WIN32
