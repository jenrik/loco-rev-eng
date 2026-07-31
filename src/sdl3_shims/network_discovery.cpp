#ifndef _WIN32

#include "network_discovery.h"

#include <sstream>
#include <utility>

namespace lego_loco::network {
namespace {

void AppendError(std::string& errors, DiscoveryBackendKind kind, const std::string& error) {
    if (!errors.empty()) {
        errors += "; ";
    }
    errors += DiscoveryBackendName(kind);
    errors += ": ";
    errors += error.empty() ? "backend failed without an error" : error;
}

DiscoveryEventKind ToPublicEventKind(BackendEventKind kind) {
    switch (kind) {
        case BackendEventKind::Added:
            return DiscoveryEventKind::Added;
        case BackendEventKind::Updated:
            return DiscoveryEventKind::Updated;
        case BackendEventKind::Removed:
            return DiscoveryEventKind::Removed;
        case BackendEventKind::Ready:
            return DiscoveryEventKind::Ready;
    }
    return DiscoveryEventKind::Ready;
}

}  // namespace

DiscoveryCoordinator::DiscoveryCoordinator(std::vector<DiscoveryBackendFactory> factories)
    : factories_(std::move(factories)) {}

DiscoveryCoordinator::~DiscoveryCoordinator() {
    Stop();
}

bool DiscoveryCoordinator::Start(const DiscoveryRequest& request, std::string* error) {
    Stop();
    request_ = request;

    std::string errors;
    const bool started = ActivateFrom(0, errors);
    if (!started && error) {
        *error = errors;
    }
    return started;
}

BackendUpdateResult DiscoveryCoordinator::Update(const SessionAdvertisement& advertisement) {
    request_.publication = advertisement;
    if (!active_) {
        return {false, true, "no discovery backend is running"};
    }

    BackendUpdateResult result = active_->Update(advertisement);
    if (!result.fatal) {
        return result;
    }

    std::string errors;
    const std::string reason = result.error.empty() ? "publication update failed" : result.error;
    if (FailOver(reason, errors)) {
        return {true, false, {}};
    }
    return {false, true, errors};
}

DiscoveryPollResult DiscoveryCoordinator::Poll() {
    DiscoveryPollResult result;

    if (active_) {
        BackendPollResult backend_result = active_->Poll();
        if (backend_result.health == BackendHealth::Running) {
            AcceptBackendEvents(std::move(backend_result.events));
        } else {
            std::string errors;
            const std::string reason = backend_result.error.empty()
                ? "backend reported a fatal error"
                : backend_result.error;
            if (!FailOver(reason, errors)) {
                result.error = std::move(errors);
            }
        }
    }

    result.backend_running = static_cast<bool>(active_);
    result.events = std::move(pending_events_);
    pending_events_.clear();
    return result;
}

void DiscoveryCoordinator::Stop() noexcept {
    if (active_) {
        active_->Stop();
        active_.reset();
    }
    next_factory_ = 0;
    visible_sessions_.clear();
    pending_events_.clear();
}

DiscoveryBackendKind DiscoveryCoordinator::active_backend() const noexcept {
    return active_ ? active_->kind() : DiscoveryBackendKind::None;
}

std::uint64_t DiscoveryCoordinator::generation() const noexcept {
    return generation_;
}

bool DiscoveryCoordinator::ActivateFrom(std::size_t first_factory, std::string& errors) {
    for (std::size_t index = first_factory; index < factories_.size(); ++index) {
        next_factory_ = index + 1;
        const DiscoveryBackendFactory& factory = factories_[index];
        if (!factory.create) {
            AppendError(errors, factory.kind, "factory is not registered");
            continue;
        }

        std::unique_ptr<IDiscoveryBackend> candidate = factory.create();
        if (!candidate) {
            AppendError(errors, factory.kind, "factory returned null");
            continue;
        }
        if (candidate->kind() != factory.kind) {
            candidate->Stop();
            AppendError(errors, factory.kind, "factory returned the wrong backend kind");
            continue;
        }

        BackendStartResult start = candidate->Start(request_);
        if (!start.ok) {
            candidate->Stop();
            AppendError(errors, factory.kind, start.error);
            continue;
        }

        active_ = std::move(candidate);
        ++generation_;
        visible_sessions_.clear();
        QueueBackendTransition(active_->kind(), errors);
        return true;
    }
    return false;
}

void DiscoveryCoordinator::QueueBackendTransition(
    DiscoveryBackendKind backend,
    const std::string& detail) {
    DiscoveryEvent event;
    event.kind = DiscoveryEventKind::BackendChanged;
    event.backend = backend;
    event.generation = generation_;
    event.detail = detail;
    pending_events_.push_back(std::move(event));
}

void DiscoveryCoordinator::QueueGenerationRemovalEvents() {
    for (const std::string& session_uuid : visible_sessions_) {
        DiscoveryEvent event;
        event.kind = DiscoveryEventKind::Removed;
        event.backend = active_backend();
        event.generation = generation_;
        event.session_uuid = session_uuid;
        event.detail = "discovery backend generation ended";
        pending_events_.push_back(std::move(event));
    }
    visible_sessions_.clear();
}

void DiscoveryCoordinator::AcceptBackendEvents(std::vector<BackendEvent> events) {
    for (BackendEvent& backend_event : events) {
        std::string session_uuid = backend_event.session_uuid;
        if (session_uuid.empty() && backend_event.session) {
            session_uuid = backend_event.session->metadata.session_uuid;
        }

        switch (backend_event.kind) {
            case BackendEventKind::Added:
            case BackendEventKind::Updated:
                if (!session_uuid.empty()) {
                    visible_sessions_.insert(session_uuid);
                }
                break;
            case BackendEventKind::Removed:
                if (!session_uuid.empty()) {
                    visible_sessions_.erase(session_uuid);
                }
                break;
            case BackendEventKind::Ready:
                break;
        }

        DiscoveryEvent event;
        event.kind = ToPublicEventKind(backend_event.kind);
        event.backend = active_backend();
        event.generation = generation_;
        event.session_uuid = std::move(session_uuid);
        event.session = std::move(backend_event.session);
        event.detail = std::move(backend_event.detail);
        pending_events_.push_back(std::move(event));
    }
}

bool DiscoveryCoordinator::FailOver(const std::string& reason, std::string& errors) {
    const DiscoveryBackendKind failed_kind = active_backend();
    QueueGenerationRemovalEvents();
    if (active_) {
        active_->Stop();
        active_.reset();
    }
    AppendError(errors, failed_kind, reason);

    if (ActivateFrom(next_factory_, errors)) {
        return true;
    }

    ++generation_;
    QueueBackendTransition(DiscoveryBackendKind::None, errors);
    return false;
}

std::vector<DiscoveryBackendKind> DefaultDiscoveryBackendOrder() {
#if defined(__ANDROID__)
    return {DiscoveryBackendKind::AndroidNsd};
#elif defined(__APPLE__)
    return {DiscoveryBackendKind::Bonjour};
#elif defined(__linux__)
    return {DiscoveryBackendKind::AvahiDbus, DiscoveryBackendKind::EmbeddedMdns};
#else
    return {DiscoveryBackendKind::EmbeddedMdns};
#endif
}

const char* DiscoveryBackendName(DiscoveryBackendKind kind) noexcept {
    switch (kind) {
        case DiscoveryBackendKind::None:
            return "none";
        case DiscoveryBackendKind::AvahiDbus:
            return "avahi-dbus";
        case DiscoveryBackendKind::EmbeddedMdns:
            return "embedded-mdns";
        case DiscoveryBackendKind::Bonjour:
            return "bonjour";
        case DiscoveryBackendKind::WindowsDnsSd:
            return "windows-dnssd";
        case DiscoveryBackendKind::AndroidNsd:
            return "android-nsd";
    }
    return "unknown";
}

}  // namespace lego_loco::network

#endif  // !_WIN32
