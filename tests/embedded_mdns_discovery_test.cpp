#include "embedded_mdns_discovery.h"

#ifndef _WIN32

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

using namespace lego_loco::network;
using namespace std::chrono_literals;

namespace {

SessionAdvertisement Advertisement(std::uint8_t players = 1) {
    SessionAdvertisement result;
    result.session_uuid = "87654321-4321-4321-4321-cba987654321";
    result.display_name = "Embedded discovery test";
    result.transport_version = 1;
    result.legacy_version = 300;
    result.current_players = players;
    result.max_players = 9;
    result.tcp_port = 42425;
    return result;
}

void TestPublishBrowseUpdateAndGoodbye() {
    std::unique_ptr<IDiscoveryBackend> browser = EmbeddedMdnsDiscoveryFactory().create();
    std::unique_ptr<IDiscoveryBackend> publisher = EmbeddedMdnsDiscoveryFactory().create();
    assert(publisher->Start({false, Advertisement()}).ok);
    assert(browser->Start({true, std::nullopt}).ok);

    bool added = false;
    for (int attempt = 0; attempt < 500 && !added; ++attempt) {
        BackendPollResult host = publisher->Poll();
        BackendPollResult client = browser->Poll();
        assert(host.health == BackendHealth::Running);
        assert(client.health == BackendHealth::Running);
        for (const BackendEvent& event : client.events) {
            if (event.kind == BackendEventKind::Added ||
                event.kind == BackendEventKind::Updated) {
                assert(event.session.has_value());
                assert(event.session->metadata.session_uuid == Advertisement().session_uuid);
                assert(event.session->metadata.current_players == 1);
                for (const EndpointCandidate& endpoint : event.session->endpoints)
                    added |= endpoint.host_or_numeric_address == "192.0.2.1";
            }
        }
        std::this_thread::sleep_for(10ms);
    }
    assert(added);

    assert(publisher->Update(Advertisement(2)).ok);
    bool updated = false;
    for (int attempt = 0; attempt < 300 && !updated; ++attempt) {
        BackendPollResult host = publisher->Poll();
        BackendPollResult client = browser->Poll();
        assert(host.health == BackendHealth::Running);
        assert(client.health == BackendHealth::Running);
        for (const BackendEvent& event : client.events) {
            if (event.kind == BackendEventKind::Updated && event.session &&
                event.session->metadata.current_players == 2) {
                updated = true;
            }
        }
        std::this_thread::sleep_for(10ms);
    }
    assert(updated);

    publisher->Stop();
    bool removed = false;
    for (int attempt = 0; attempt < 300 && !removed; ++attempt) {
        BackendPollResult client = browser->Poll();
        assert(client.health == BackendHealth::Running);
        for (const BackendEvent& event : client.events)
            removed |= event.kind == BackendEventKind::Removed;
        std::this_thread::sleep_for(10ms);
    }
    assert(removed);
    browser->Stop();
}

void TestProbeDetectsExistingName() {
    std::unique_ptr<IDiscoveryBackend> first = EmbeddedMdnsDiscoveryFactory().create();
    assert(first->Start({false, Advertisement()}).ok);
    for (int attempt = 0; attempt < 120; ++attempt) {
        assert(first->Poll().health == BackendHealth::Running);
        std::this_thread::sleep_for(10ms);
    }

    std::unique_ptr<IDiscoveryBackend> duplicate = EmbeddedMdnsDiscoveryFactory().create();
    BackendStartResult duplicate_start = duplicate->Start({false, Advertisement()});
    bool conflict = !duplicate_start.ok;
    for (int attempt = 0; attempt < 200 && !conflict; ++attempt) {
        BackendPollResult original = first->Poll();
        BackendPollResult contender = duplicate->Poll();
        assert(original.health == BackendHealth::Running);
        conflict = contender.health == BackendHealth::Fatal;
        std::this_thread::sleep_for(10ms);
    }
    assert(conflict);
    duplicate->Stop();
    first->Stop();
}

}  // namespace

int main() {
    TestPublishBrowseUpdateAndGoodbye();
    TestProbeDetectsExistingName();
    std::cout << "PASS: embedded mDNS publish, browse, update, goodbye, and conflict detection\n";
    return 0;
}

#endif  // !_WIN32
