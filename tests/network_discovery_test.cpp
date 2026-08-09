#include "network_discovery.h"

#ifndef _WIN32

#include <cassert>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace lego_loco::network;

namespace {

struct FakeState {
    DiscoveryBackendKind kind = DiscoveryBackendKind::None;
    BackendStartResult start_result{true, {}};
    BackendUpdateResult update_result{true, false, {}};
    std::deque<BackendPollResult> polls{};
    int starts = 0;
    int updates = 0;
    int stops = 0;
    DiscoveryRequest last_request{};
    SessionAdvertisement last_update{};
};

class FakeBackend final : public IDiscoveryBackend {
public:
    explicit FakeBackend(FakeState& state) : state_(state) {}

    DiscoveryBackendKind kind() const noexcept override { return state_.kind; }

    BackendStartResult Start(const DiscoveryRequest& request) override {
        ++state_.starts;
        state_.last_request = request;
        return state_.start_result;
    }

    BackendUpdateResult Update(const SessionAdvertisement& advertisement) override {
        ++state_.updates;
        state_.last_update = advertisement;
        return state_.update_result;
    }

    BackendPollResult Poll() override {
        if (state_.polls.empty()) {
            return {};
        }
        BackendPollResult result = std::move(state_.polls.front());
        state_.polls.pop_front();
        return result;
    }

    void Stop() noexcept override { ++state_.stops; }

private:
    FakeState& state_;
};

DiscoveryBackendFactory Factory(FakeState& state) {
    return {state.kind, [&state] { return std::make_unique<FakeBackend>(state); }};
}

SessionAdvertisement Advertisement(std::uint8_t players = 1) {
    SessionAdvertisement result;
    result.session_uuid = "12345678-1234-1234-1234-123456789abc";
    result.display_name = "Test session";
    result.transport_version = 1;
    result.legacy_version = 300;
    result.current_players = players;
    result.max_players = 9;
    result.tcp_port = 42424;
    return result;
}

BackendEvent AddedEvent() {
    DiscoveredSession session;
    session.metadata = Advertisement();
    session.endpoints.push_back({"192.0.2.10", 42424, 0});
    return {BackendEventKind::Added, session.metadata.session_uuid, session, {}};
}

void TestLinuxOrder() {
    const std::vector<DiscoveryBackendKind> order = DefaultDiscoveryBackendOrder();
#if defined(__linux__) && !defined(__ANDROID__)
    assert(order.size() == 2);
    assert(order[0] == DiscoveryBackendKind::AvahiDbus);
    assert(order[1] == DiscoveryBackendKind::EmbeddedMdns);
#else
    assert(!order.empty());
#endif
}

void TestStartupFallback() {
    FakeState avahi;
    avahi.kind = DiscoveryBackendKind::AvahiDbus;
    avahi.start_result = {false, "name has no owner"};

    FakeState embedded;
    embedded.kind = DiscoveryBackendKind::EmbeddedMdns;

    DiscoveryCoordinator coordinator({Factory(avahi), Factory(embedded)});
    DiscoveryRequest request;
    request.browse = true;

    std::string error;
    assert(coordinator.Start(request, &error));
    assert(error.empty());
    assert(coordinator.active_backend() == DiscoveryBackendKind::EmbeddedMdns);
    assert(coordinator.generation() == 1);
    assert(avahi.starts == 1);
    assert(avahi.stops == 1);
    assert(embedded.starts == 1);

    DiscoveryPollResult poll = coordinator.Poll();
    assert(poll.backend_running);
    assert(poll.events.size() == 1);
    assert(poll.events[0].kind == DiscoveryEventKind::BackendChanged);
    assert(poll.events[0].backend == DiscoveryBackendKind::EmbeddedMdns);
    assert(poll.events[0].detail.find("name has no owner") != std::string::npos);
}

void TestFatalRuntimeFallbackClearsGeneration() {
    FakeState avahi;
    avahi.kind = DiscoveryBackendKind::AvahiDbus;
    avahi.polls.push_back({BackendHealth::Running, {AddedEvent()}, {}});
    avahi.polls.push_back({BackendHealth::Fatal, {}, "daemon disappeared"});

    FakeState embedded;
    embedded.kind = DiscoveryBackendKind::EmbeddedMdns;

    DiscoveryCoordinator coordinator({Factory(avahi), Factory(embedded)});
    DiscoveryRequest request;
    request.browse = true;
    assert(coordinator.Start(request));

    DiscoveryPollResult first = coordinator.Poll();
    assert(first.events.size() == 2);
    assert(first.events[0].kind == DiscoveryEventKind::BackendChanged);
    assert(first.events[0].generation == 1);
    assert(first.events[1].kind == DiscoveryEventKind::Added);
    assert(first.events[1].generation == 1);

    DiscoveryPollResult second = coordinator.Poll();
    assert(second.backend_running);
    assert(coordinator.active_backend() == DiscoveryBackendKind::EmbeddedMdns);
    assert(coordinator.generation() == 2);
    assert(avahi.starts == 1);
    assert(avahi.stops == 1);
    assert(embedded.starts == 1);
    assert(second.events.size() == 2);
    assert(second.events[0].kind == DiscoveryEventKind::Removed);
    assert(second.events[0].generation == 1);
    assert(second.events[1].kind == DiscoveryEventKind::BackendChanged);
    assert(second.events[1].generation == 2);
    assert(second.events[1].detail.find("daemon disappeared") != std::string::npos);

    // The preferred backend is not retried during this lifecycle.
    coordinator.Poll();
    assert(avahi.starts == 1);
}

void TestFatalUpdateRestartsPublicationOnFallback() {
    FakeState avahi;
    avahi.kind = DiscoveryBackendKind::AvahiDbus;
    avahi.update_result = {false, true, "entry group lost"};

    FakeState embedded;
    embedded.kind = DiscoveryBackendKind::EmbeddedMdns;

    DiscoveryCoordinator coordinator({Factory(avahi), Factory(embedded)});
    DiscoveryRequest request;
    request.publication = Advertisement(1);
    assert(coordinator.Start(request));

    const SessionAdvertisement updated = Advertisement(2);
    BackendUpdateResult result = coordinator.Update(updated);
    assert(result.ok);
    assert(!result.fatal);
    assert(coordinator.active_backend() == DiscoveryBackendKind::EmbeddedMdns);
    assert(embedded.last_request.publication.has_value());
    assert(embedded.last_request.publication->current_players == 2);
}

void TestAllBackendsFail() {
    FakeState avahi;
    avahi.kind = DiscoveryBackendKind::AvahiDbus;
    avahi.start_result = {false, "no system bus"};

    FakeState embedded;
    embedded.kind = DiscoveryBackendKind::EmbeddedMdns;
    embedded.start_result = {false, "multicast unavailable"};

    DiscoveryCoordinator coordinator({Factory(avahi), Factory(embedded)});
    std::string error;
    assert(!coordinator.Start({true, std::nullopt}, &error));
    assert(coordinator.active_backend() == DiscoveryBackendKind::None);
    assert(error.find("no system bus") != std::string::npos);
    assert(error.find("multicast unavailable") != std::string::npos);
}

}  // namespace

int main() {
    TestLinuxOrder();
    TestStartupFallback();
    TestFatalRuntimeFallbackClearsGeneration();
    TestFatalUpdateRestartsPublicationOnFallback();
    TestAllBackendsFail();
    std::cout << "PASS: discovery backend abstraction and fallback policy\n";
    return 0;
}

#endif  // !_WIN32
