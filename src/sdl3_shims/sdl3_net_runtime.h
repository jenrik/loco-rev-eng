#pragma once

#ifndef _WIN32

#include "sdl3_net_transport.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace lego_loco::network {

enum class TransportRuntimeMode { None, Host, Client };
enum class TransportRuntimeState {
    Idle,
    Starting,
    Listening,
    Connecting,
    Connected,
    Failed,
};

struct TransportRuntimeSnapshot {
    TransportRuntimeMode mode = TransportRuntimeMode::None;
    TransportRuntimeState state = TransportRuntimeState::Idle;
    std::uint16_t port = 0;
    VirtualPlayerId local_player_id = 0;
    std::uint8_t player_count = 0;
    std::string session_uuid;
    std::string error;
};

/** Dedicated owner thread for every SDL_net object used by the host runtime. */
class TransportWorker {
public:
    TransportWorker();
    ~TransportWorker();
    TransportWorker(const TransportWorker&) = delete;
    TransportWorker& operator=(const TransportWorker&) = delete;

    void StartHost(const TransportHostConfig& config);
    void Join(const TransportClientConfig& config);
    void SendLegacy(VirtualPlayerId destination, std::vector<std::uint8_t> payload);
    void StopTransport();
    TransportRuntimeSnapshot Snapshot() const;
    std::vector<TransportEvent> TakeEvents();

private:
    enum class CommandKind { StartHost, Join, Send, Stop, Shutdown };
    struct Command {
        CommandKind kind = CommandKind::Stop;
        TransportHostConfig host;
        TransportClientConfig client;
        VirtualPlayerId destination = 0;
        std::vector<std::uint8_t> payload;
    };

    void Enqueue(Command command);
    void Run();
    void ResetSnapshotLocked();
    void RecordEvents(std::vector<TransportEvent> events);

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Command> commands_;
    std::deque<TransportEvent> events_;
    TransportRuntimeSnapshot snapshot_;
    std::size_t queued_event_bytes_ = 0;
    bool event_queue_overflow_ = false;
    bool shutdown_ = false;
    std::uint32_t wake_event_type_ = 0;
    std::thread thread_;
};

TransportWorker& HostTransportWorker();

}  // namespace lego_loco::network

#endif  // !_WIN32
