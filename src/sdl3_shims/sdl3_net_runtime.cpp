#ifndef _WIN32

#include "sdl3_net_runtime.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <memory>
#include <utility>

namespace lego_loco::network {
namespace {
constexpr std::size_t kMaximumQueuedEvents = 1024;
constexpr std::size_t kMaximumQueuedEventBytes = 4 * 1024 * 1024;
}

TransportWorker::TransportWorker()
    : wake_event_type_(SDL_RegisterEvents(1)),
      thread_(&TransportWorker::Run, this) {}
TransportWorker::~TransportWorker() {
    Enqueue({CommandKind::Shutdown, {}, {}, 0, {}});
    if (thread_.joinable()) thread_.join();
}

void TransportWorker::StartHost(const TransportHostConfig& config) {
    Enqueue({CommandKind::StartHost, config, {}, 0, {}});
}
void TransportWorker::Join(const TransportClientConfig& config) {
    Enqueue({CommandKind::Join, {}, config, 0, {}});
}
void TransportWorker::SendLegacy(VirtualPlayerId destination,
                                 std::vector<std::uint8_t> payload) {
    Enqueue({CommandKind::Send, {}, {}, destination, std::move(payload)});
}
void TransportWorker::StopTransport() {
    Enqueue({CommandKind::Stop, {}, {}, 0, {}});
}

TransportRuntimeSnapshot TransportWorker::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

std::vector<TransportEvent> TransportWorker::TakeEvents() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TransportEvent> result;
    result.reserve(events_.size());
    while (!events_.empty()) {
        queued_event_bytes_ -= events_.front().payload.size();
        result.push_back(std::move(events_.front()));
        events_.pop_front();
    }
    return result;
}

void TransportWorker::Enqueue(Command command) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) return;
        commands_.push_back(std::move(command));
    }
    wake_.notify_one();
}

void TransportWorker::ResetSnapshotLocked() {
    snapshot_ = {};
    events_.clear();
    queued_event_bytes_ = 0;
    event_queue_overflow_ = false;
}

void TransportWorker::RecordEvents(std::vector<TransportEvent> events) {
    if (events.empty()) return;
    {
    std::lock_guard<std::mutex> lock(mutex_);
    for (TransportEvent& event : events) {
        if (event.kind == TransportEventKind::Listening) {
            snapshot_.state = TransportRuntimeState::Listening;
        } else if (event.kind == TransportEventKind::Connected) {
            snapshot_.state = TransportRuntimeState::Connected;
            snapshot_.local_player_id = event.player_id;
        } else if (event.kind == TransportEventKind::Error) {
            snapshot_.error = event.message;
            if (snapshot_.state != TransportRuntimeState::Connected &&
                snapshot_.state != TransportRuntimeState::Listening) {
                snapshot_.state = TransportRuntimeState::Failed;
            }
        } else if (event.kind == TransportEventKind::Disconnected) {
            snapshot_.state = TransportRuntimeState::Failed;
            if (snapshot_.error.empty()) snapshot_.error = event.message;
        }
        if (events_.size() >= kMaximumQueuedEvents ||
            queued_event_bytes_ + event.payload.size() > kMaximumQueuedEventBytes) {
            event_queue_overflow_ = true;
            snapshot_.state = TransportRuntimeState::Failed;
            snapshot_.error = "transport event queue high-water mark exceeded";
        }
        queued_event_bytes_ += event.payload.size();
        events_.push_back(std::move(event));
    }
    }
    if (wake_event_type_ != static_cast<std::uint32_t>(-1)) {
        SDL_Event wake_event{};
        wake_event.type = wake_event_type_;
        SDL_PushEvent(&wake_event);
    }
}

void TransportWorker::Run() {
    using namespace std::chrono_literals;
    std::unique_ptr<SdlNetTransportHost> host;
    std::unique_ptr<SdlNetTransportClient> client;

    for (;;) {
        std::deque<Command> commands;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait_for(lock, 5ms, [&] { return !commands_.empty(); });
            commands.swap(commands_);
        }

        for (Command& command : commands) {
            if (command.kind == CommandKind::Shutdown) {
                if (client) client->Stop();
                if (host) host->Stop();
                std::lock_guard<std::mutex> lock(mutex_);
                shutdown_ = true;
                ResetSnapshotLocked();
                // SDL error reporting allocates per-thread storage. This owner
                // thread is not created by SDL, so release its TLS explicitly.
                SDL_CleanupTLS();
                return;
            }
            if (command.kind == CommandKind::Stop) {
                if (client) client->Stop();
                if (host) host->Stop();
                client.reset();
                host.reset();
                std::lock_guard<std::mutex> lock(mutex_);
                ResetSnapshotLocked();
                continue;
            }
            if (command.kind == CommandKind::StartHost) {
                if (client) client->Stop();
                if (host) host->Stop();
                client.reset();
                host = std::make_unique<SdlNetTransportHost>();
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ResetSnapshotLocked();
                    snapshot_.mode = TransportRuntimeMode::Host;
                    snapshot_.state = TransportRuntimeState::Starting;
                    snapshot_.port = command.host.port;
                }
                std::string error;
                if (!host->Start(command.host, &error)) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    snapshot_.state = TransportRuntimeState::Failed;
                    snapshot_.error = std::move(error);
                } else {
                    std::lock_guard<std::mutex> lock(mutex_);
                    snapshot_.state = TransportRuntimeState::Listening;
                    snapshot_.session_uuid = host->session_uuid();
                    snapshot_.local_player_id = kHostPlayerId;
                    snapshot_.player_count = host->player_count();
                }
                continue;
            }
            if (command.kind == CommandKind::Join) {
                if (client) client->Stop();
                if (host) host->Stop();
                host.reset();
                client = std::make_unique<SdlNetTransportClient>();
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ResetSnapshotLocked();
                    snapshot_.mode = TransportRuntimeMode::Client;
                    snapshot_.state = TransportRuntimeState::Connecting;
                    snapshot_.port = command.client.port;
                    snapshot_.session_uuid = command.client.expected_session_uuid;
                }
                std::string error;
                if (!client->Connect(command.client, &error)) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    snapshot_.state = TransportRuntimeState::Failed;
                    snapshot_.error = std::move(error);
                }
                continue;
            }
            if (command.kind == CommandKind::Send) {
                std::string error;
                const bool ok = host
                    ? host->SendLegacy(command.destination, command.payload, &error)
                    : client && client->SendLegacy(command.destination, command.payload, &error);
                if (!ok) RecordEvents({TransportEvent{
                    TransportEventKind::Error, 0, 0, 0, {}, std::move(error)}});
            }
        }

        if (host && host->running()) {
            RecordEvents(host->Poll());
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.player_count = host->player_count();
            snapshot_.session_uuid = host->session_uuid();
        }
        if (client && client->active()) {
            RecordEvents(client->Poll());
            std::lock_guard<std::mutex> lock(mutex_);
            if (client->admitted() && !event_queue_overflow_) {
                snapshot_.state = TransportRuntimeState::Connected;
                snapshot_.local_player_id = client->player_id();
                snapshot_.player_count = 1;
                snapshot_.session_uuid = client->session_uuid();
            }
        }

        bool overflow = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            overflow = event_queue_overflow_;
        }
        if (overflow) {
            if (client) client->Stop();
            if (host) host->Stop();
            client.reset();
            host.reset();
        }
    }
}

TransportWorker& HostTransportWorker() {
    static TransportWorker worker;
    return worker;
}

}  // namespace lego_loco::network

#endif  // !_WIN32
