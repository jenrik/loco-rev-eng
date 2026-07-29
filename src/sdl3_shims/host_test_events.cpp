#include "host_test_events.h"

#ifndef _WIN32

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

namespace loco::host_test {
namespace {

class EventSink {
public:
    EventSink()
        : file_(nullptr), sequence_(0), started_(std::chrono::steady_clock::now())
    {
        const char* path = std::getenv("LEGO_LOCO_TEST_EVENTS");
        if (path == nullptr || *path == '\0') return;

        file_ = std::fopen(path, "w");
        if (file_ == nullptr) {
            std::fprintf(stderr,
                         "[HOST-TEST] cannot open LEGO_LOCO_TEST_EVENTS artifact\n");
        }
    }

    ~EventSink()
    {
        if (file_ != nullptr) std::fclose(file_);
    }

    EventSink(const EventSink&) = delete;
    EventSink& operator=(const EventSink&) = delete;

    void event(const char* name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_ == nullptr) return;
        prefix(name);
        std::fputs("}\n", file_);
        std::fflush(file_);
    }

    void mode_changed(int old_mode, int new_mode)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_ == nullptr) return;
        prefix("mode_changed");
        std::fprintf(file_, ",\"old_mode\":%d,\"new_mode\":%d}\n",
                     old_mode, new_mode);
        std::fflush(file_);
    }

    void screen_presented(const char* screen, int dialog_state)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_ == nullptr || screen == nullptr) return;
        if (last_screen_ == screen && last_dialog_state_ == dialog_state) return;

        last_screen_ = screen;
        last_dialog_state_ = dialog_state;
        prefix("screen_presented");
        std::fprintf(file_, ",\"screen\":\"%s\",\"dialog_state\":%d}\n",
                     screen, dialog_state);
        std::fflush(file_);
    }

    void search_completed(int sessions)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_ == nullptr) return;
        prefix("search_completed");
        std::fprintf(file_, ",\"sessions\":%d}\n", sessions);
        std::fflush(file_);
    }

private:
    void prefix(const char* event)
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_).count();
        std::fprintf(file_,
                     "{\"sequence\":%llu,\"elapsed_ms\":%lld,\"event\":\"%s\"",
                     static_cast<unsigned long long>(++sequence_),
                     static_cast<long long>(elapsed), event);
    }

    FILE* file_;
    unsigned long long sequence_;
    std::chrono::steady_clock::time_point started_;
    std::mutex mutex_;
    std::string last_screen_;
    int last_dialog_state_ = -1;
};

EventSink& sink()
{
    static EventSink instance;
    return instance;
}

}  // namespace

void emit_process_started()
{
    sink().event("process_started");
}

void emit_mode_changed(int old_mode, int new_mode)
{
    sink().mode_changed(old_mode, new_mode);
}

void emit_screen_presented(const char* screen, int dialog_state)
{
    sink().screen_presented(screen, dialog_state);
}

void emit_search_completed(int sessions)
{
    sink().search_completed(sessions);
}

void emit_clean_shutdown()
{
    sink().event("clean_shutdown");
}

}  // namespace loco::host_test

#endif  // !_WIN32
