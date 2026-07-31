#pragma once

#ifndef _WIN32

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace lego_loco::network {

/** Host-only DNS-SD backend identity.
 *
 * Platform SDK handles deliberately do not appear in this contract. */
enum class DiscoveryBackendKind {
    None,
    AvahiDbus,
    EmbeddedMdns,
    Bonjour,
    WindowsDnsSd,
    AndroidNsd,
};

struct EndpointCandidate {
    std::string host_or_numeric_address;
    std::uint16_t port = 0;
    // Interface scope for link-local IPv6. Zero means no explicit scope.
    std::uint32_t scope_id = 0;
};

struct SessionAdvertisement {
    std::string session_uuid;
    std::string display_name;
    std::uint16_t transport_version = 0;
    std::uint16_t legacy_version = 0;
    std::uint8_t current_players = 0;
    std::uint8_t max_players = 0;
    std::uint16_t tcp_port = 0;
};

struct DiscoveredSession {
    SessionAdvertisement metadata;
    std::vector<EndpointCandidate> endpoints;
};

struct DiscoveryRequest {
    bool browse = false;
    std::optional<SessionAdvertisement> publication;
};

enum class BackendEventKind {
    Added,
    Updated,
    Removed,
    Ready,
};

struct BackendEvent {
    BackendEventKind kind = BackendEventKind::Ready;
    std::string session_uuid;
    std::optional<DiscoveredSession> session;
    std::string detail;
};

enum class BackendHealth {
    Running,
    Fatal,
};

struct BackendStartResult {
    bool ok = false;
    std::string error;
};

struct BackendUpdateResult {
    bool ok = false;
    bool fatal = false;
    std::string error;
};

struct BackendPollResult {
    BackendHealth health = BackendHealth::Running;
    std::vector<BackendEvent> events;
    std::string error;
};

/** One platform discovery implementation.
 *
 * All calls are nonblocking after Start and confined to the owner thread.
 * Stop must release publications, browser objects, bus handles, and sockets. */
class IDiscoveryBackend {
public:
    virtual ~IDiscoveryBackend() = default;

    virtual DiscoveryBackendKind kind() const noexcept = 0;
    virtual BackendStartResult Start(const DiscoveryRequest& request) = 0;
    virtual BackendUpdateResult Update(const SessionAdvertisement& advertisement) = 0;
    virtual BackendPollResult Poll() = 0;
    virtual void Stop() noexcept = 0;
};

struct DiscoveryBackendFactory {
    DiscoveryBackendKind kind = DiscoveryBackendKind::None;
    std::function<std::unique_ptr<IDiscoveryBackend>()> create;
};

enum class DiscoveryEventKind {
    Added,
    Updated,
    Removed,
    Ready,
    BackendChanged,
};

struct DiscoveryEvent {
    DiscoveryEventKind kind = DiscoveryEventKind::Ready;
    DiscoveryBackendKind backend = DiscoveryBackendKind::None;
    std::uint64_t generation = 0;
    std::string session_uuid;
    std::optional<DiscoveredSession> session;
    std::string detail;
};

struct DiscoveryPollResult {
    bool backend_running = false;
    std::vector<DiscoveryEvent> events;
    std::string error;
};

/** Owns exactly one backend and performs ordered, non-oscillating fallback.
 *
 * A failed preferred backend is not retried until the next explicit Start.
 * This prevents duplicate publishers and Avahi/embedded-mDNS oscillation. */
class DiscoveryCoordinator {
public:
    explicit DiscoveryCoordinator(std::vector<DiscoveryBackendFactory> factories);
    ~DiscoveryCoordinator();

    DiscoveryCoordinator(const DiscoveryCoordinator&) = delete;
    DiscoveryCoordinator& operator=(const DiscoveryCoordinator&) = delete;

    bool Start(const DiscoveryRequest& request, std::string* error = nullptr);
    BackendUpdateResult Update(const SessionAdvertisement& advertisement);
    DiscoveryPollResult Poll();
    void Stop() noexcept;

    DiscoveryBackendKind active_backend() const noexcept;
    std::uint64_t generation() const noexcept;

private:
    bool ActivateFrom(std::size_t first_factory, std::string& errors);
    void QueueBackendTransition(DiscoveryBackendKind backend, const std::string& detail);
    void QueueGenerationRemovalEvents();
    void AcceptBackendEvents(std::vector<BackendEvent> events);
    bool FailOver(const std::string& reason, std::string& errors);

    std::vector<DiscoveryBackendFactory> factories_;
    std::unique_ptr<IDiscoveryBackend> active_;
    DiscoveryRequest request_;
    std::size_t next_factory_ = 0;
    std::uint64_t generation_ = 0;
    std::set<std::string> visible_sessions_;
    std::vector<DiscoveryEvent> pending_events_;
};

/** Preferred backend order for the platform being compiled.
 * Concrete factories are registered separately by platform adapters. */
std::vector<DiscoveryBackendKind> DefaultDiscoveryBackendOrder();

const char* DiscoveryBackendName(DiscoveryBackendKind kind) noexcept;

}  // namespace lego_loco::network

#endif  // !_WIN32
