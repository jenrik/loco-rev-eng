#pragma once

#ifndef _WIN32

#include "network_discovery.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace lego_loco::network {

struct DiscoveryRuntimeSnapshot {
    bool active = false;
    bool browse_ready = false;
    DiscoveryBackendKind backend = DiscoveryBackendKind::None;
    std::uint64_t generation = 0;
    std::vector<DiscoveredSession> sessions;
    std::string error;
};

/** Thread owner for concrete discovery backends.
 *
 * UI/game callers enqueue lifecycle requests and copy snapshots. Only this
 * worker thread calls DiscoveryCoordinator or platform APIs. */
class DiscoveryWorker {
public:
    explicit DiscoveryWorker(
        std::vector<DiscoveryBackendFactory> factories,
        std::chrono::milliseconds browse_settle_time = std::chrono::milliseconds(1500));
    ~DiscoveryWorker();

    DiscoveryWorker(const DiscoveryWorker&) = delete;
    DiscoveryWorker& operator=(const DiscoveryWorker&) = delete;

    void Start(const DiscoveryRequest& request);
    void UpdatePublication(const SessionAdvertisement& advertisement);
    void StopDiscovery();
    DiscoveryRuntimeSnapshot Snapshot() const;

private:
    enum class CommandKind { Start, Update, Stop, Shutdown };
    struct Command {
        CommandKind kind = CommandKind::Stop;
        DiscoveryRequest request;
        std::optional<SessionAdvertisement> advertisement;
    };

    void Enqueue(Command command);
    void Run();
    void ApplyEvents(const DiscoveryPollResult& poll);
    void ClearSnapshotLocked();

    DiscoveryCoordinator coordinator_;
    const std::chrono::milliseconds browse_settle_time_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Command> commands_;
    DiscoveryRuntimeSnapshot snapshot_;
    std::map<std::string, DiscoveredSession> sessions_;
    bool browsing_ = false;
    bool shutdown_ = false;
    std::chrono::steady_clock::time_point browse_deadline_{};
    std::thread thread_;
};

/** Process-wide host discovery worker using platform-preferred factories. */
DiscoveryWorker& HostDiscoveryWorker();

}  // namespace lego_loco::network

#endif  // !_WIN32
