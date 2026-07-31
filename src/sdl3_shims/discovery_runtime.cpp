#ifndef _WIN32

#include "discovery_runtime.h"
#include "discovery_backends.h"

#include <algorithm>
#include <utility>

namespace lego_loco::network {

DiscoveryWorker::DiscoveryWorker(
    std::vector<DiscoveryBackendFactory> factories,
    std::chrono::milliseconds browse_settle_time)
    : coordinator_(std::move(factories)),
      browse_settle_time_(browse_settle_time),
      thread_(&DiscoveryWorker::Run, this) {}

DiscoveryWorker::~DiscoveryWorker() {
    Enqueue({CommandKind::Shutdown, {}, std::nullopt});
    if (thread_.joinable()) {
        thread_.join();
    }
}

void DiscoveryWorker::Start(const DiscoveryRequest& request) {
    Enqueue({CommandKind::Start, request, std::nullopt});
}

void DiscoveryWorker::UpdatePublication(
    const SessionAdvertisement& advertisement) {
    Enqueue({CommandKind::Update, {}, advertisement});
}

void DiscoveryWorker::StopDiscovery() {
    Enqueue({CommandKind::Stop, {}, std::nullopt});
}

DiscoveryRuntimeSnapshot DiscoveryWorker::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void DiscoveryWorker::Enqueue(Command command) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) {
            return;
        }
        commands_.push_back(std::move(command));
    }
    wake_.notify_one();
}

void DiscoveryWorker::ClearSnapshotLocked() {
    snapshot_ = {};
    sessions_.clear();
}

void DiscoveryWorker::Run() {
    using namespace std::chrono_literals;
    for (;;) {
        std::deque<Command> commands;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait_for(lock, 20ms, [&] { return !commands_.empty(); });
            commands.swap(commands_);
        }

        for (Command& command : commands) {
            if (command.kind == CommandKind::Shutdown) {
                coordinator_.Stop();
                std::lock_guard<std::mutex> lock(mutex_);
                shutdown_ = true;
                ClearSnapshotLocked();
                return;
            }
            if (command.kind == CommandKind::Stop) {
                coordinator_.Stop();
                browsing_ = false;
                std::lock_guard<std::mutex> lock(mutex_);
                ClearSnapshotLocked();
                continue;
            }
            if (command.kind == CommandKind::Start) {
                std::string error;
                const bool started = coordinator_.Start(command.request, &error);
                browsing_ = command.request.browse;
                browse_deadline_ = std::chrono::steady_clock::now() + browse_settle_time_;
                std::lock_guard<std::mutex> lock(mutex_);
                ClearSnapshotLocked();
                snapshot_.active = started;
                snapshot_.backend = coordinator_.active_backend();
                snapshot_.generation = coordinator_.generation();
                snapshot_.error = std::move(error);
                snapshot_.browse_ready = command.request.browse && !started;
                continue;
            }
            if (command.kind == CommandKind::Update && command.advertisement) {
                BackendUpdateResult update = coordinator_.Update(*command.advertisement);
                if (!update.ok) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    snapshot_.error = std::move(update.error);
                    snapshot_.active = coordinator_.active_backend() !=
                        DiscoveryBackendKind::None;
                }
            }
        }

        if (coordinator_.active_backend() == DiscoveryBackendKind::None) {
            continue;
        }
        DiscoveryPollResult poll = coordinator_.Poll();
        ApplyEvents(poll);

        if (browsing_ && std::chrono::steady_clock::now() >= browse_deadline_) {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.browse_ready = true;
        }
    }
}

void DiscoveryWorker::ApplyEvents(const DiscoveryPollResult& poll) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.active = poll.backend_running;
    snapshot_.backend = coordinator_.active_backend();
    snapshot_.generation = coordinator_.generation();
    if (!poll.error.empty()) {
        snapshot_.error = poll.error;
    }

    for (const DiscoveryEvent& event : poll.events) {
        switch (event.kind) {
            case DiscoveryEventKind::Added:
            case DiscoveryEventKind::Updated:
                if (event.session && !event.session_uuid.empty()) {
                    sessions_[event.session_uuid] = *event.session;
                }
                break;
            case DiscoveryEventKind::Removed:
                sessions_.erase(event.session_uuid);
                break;
            case DiscoveryEventKind::Ready:
                snapshot_.browse_ready = true;
                break;
            case DiscoveryEventKind::BackendChanged:
                if (event.backend == DiscoveryBackendKind::None) {
                    sessions_.clear();
                }
                break;
        }
    }

    snapshot_.sessions.clear();
    snapshot_.sessions.reserve(sessions_.size());
    for (const auto& entry : sessions_) {
        snapshot_.sessions.push_back(entry.second);
    }
}

DiscoveryWorker& HostDiscoveryWorker() {
    static DiscoveryWorker worker(PlatformDiscoveryBackendFactories());
    return worker;
}

}  // namespace lego_loco::network

#endif  // !_WIN32
