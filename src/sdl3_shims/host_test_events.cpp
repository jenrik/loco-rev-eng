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
        if (file_ == nullptr) std::fprintf(stderr, "[HOST-TEST] cannot open LEGO_LOCO_TEST_EVENTS artifact\n");
    }

    ~EventSink() { if (file_ != nullptr) std::fclose(file_); }
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
        write("mode_changed", ",\"old_mode\":%d,\"new_mode\":%d", old_mode, new_mode);
    }

    void screen_presented(const char* screen, int dialog_state)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_ == nullptr || screen == nullptr ||
            (last_screen_ == screen && last_dialog_state_ == dialog_state)) return;
        last_screen_ = screen;
        last_dialog_state_ = dialog_state;
        prefix("screen_presented");
        std::fprintf(file_, ",\"screen\":\"%s\",\"dialog_state\":%d}\n", screen, dialog_state);
        std::fflush(file_);
    }

    void search_completed(int sessions) { write("search_completed", ",\"sessions\":%d", sessions); }
    void layout_selected(int columns, int rows, int slots) {
        write("layout_selected", ",\"columns\":%d,\"rows\":%d,\"slots\":%d",
              columns, rows, slots);
    }
    void audio_queued(uint32_t resource_id) { write("audio_queued", ",\"resource_id\":%u", resource_id); }

    void player_name_committed(const char* name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_ == nullptr || name == nullptr) return;
        prefix("player_name_committed");
        std::fputs(",\"name\":\"", file_);
        for (const unsigned char* ch = reinterpret_cast<const unsigned char*>(name); *ch != 0; ++ch) {
            if (*ch == '\\' || *ch == '"') std::fputc('\\', file_);
            if (*ch >= 0x20) std::fputc(*ch, file_);
        }
        std::fputs("\"}\n", file_);
        std::fflush(file_);
    }

    void intro_video_started(int index, const char* path)
    {
        write("intro_video_started", ",\"index\":%d,\"path\":\"%s\"", index, path ? path : "");
    }
    void intro_video_frame(int index, int width, int height)
    {
        write("intro_video_frame", ",\"index\":%d,\"width\":%d,\"height\":%d", index, width, height);
    }
    void intro_video_finished(int index, bool skipped)
    {
        write("intro_video_finished", ",\"index\":%d,\"skipped\":%s", index, skipped ? "true" : "false");
    }
    void intro_video_failed(int index, const char* message)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_ == nullptr) return;
        prefix("intro_video_failed");
        std::fprintf(file_, ",\"index\":%d,\"message\":", index);
        writeJsonString(message ? message : "unknown error");
        std::fputs("}\n", file_);
        std::fflush(file_);
    }

private:
    template <typename... Args>
    void write(const char* name, const char* format, Args... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_ == nullptr) return;
        prefix(name);
        std::fprintf(file_, format, args...);
        std::fputs("}\n", file_);
        std::fflush(file_);
    }

    void writeJsonString(const char* value)
    {
        std::fputc('"', file_);
        for (const unsigned char* ch = reinterpret_cast<const unsigned char*>(value); *ch != 0; ++ch) {
            if (*ch == '\\' || *ch == '"') std::fputc('\\', file_);
            if (*ch == '\n') { std::fputs("\\n", file_); continue; }
            if (*ch == '\r') { std::fputs("\\r", file_); continue; }
            if (*ch == '\t') { std::fputs("\\t", file_); continue; }
            if (*ch >= 0x20) std::fputc(*ch, file_);
        }
        std::fputc('"', file_);
    }

    void prefix(const char* event)
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_).count();
        std::fprintf(file_, "{\"sequence\":%llu,\"elapsed_ms\":%lld,\"event\":\"%s\"",
                     static_cast<unsigned long long>(++sequence_), static_cast<long long>(elapsed), event);
    }

    FILE* file_;
    unsigned long long sequence_;
    std::chrono::steady_clock::time_point started_;
    std::mutex mutex_;
    std::string last_screen_;
    int last_dialog_state_ = -1;
};

EventSink& sink() { static EventSink instance; return instance; }
}  // namespace

void emit_process_started() { sink().event("process_started"); }
void emit_mode_changed(int old_mode, int new_mode) { sink().mode_changed(old_mode, new_mode); }
void emit_screen_presented(const char* screen, int dialog_state) { sink().screen_presented(screen, dialog_state); }
void emit_search_completed(int sessions) { sink().search_completed(sessions); }
void emit_layout_selected(int columns, int rows, int slots) {
    sink().layout_selected(columns, rows, slots);
}
void emit_audio_queued(uint32_t resource_id) { sink().audio_queued(resource_id); }
void emit_player_name_committed(const char* name) { sink().player_name_committed(name); }
void emit_intro_video_started(int index, const char* path) { sink().intro_video_started(index, path); }
void emit_intro_video_frame(int index, int width, int height) { sink().intro_video_frame(index, width, height); }
void emit_intro_video_finished(int index, bool skipped) { sink().intro_video_finished(index, skipped); }
void emit_intro_video_failed(int index, const char* message) { sink().intro_video_failed(index, message); }
void emit_intro_sequence_complete() { sink().event("intro_sequence_complete"); }
void emit_clean_shutdown() { sink().event("clean_shutdown"); }

}  // namespace loco::host_test

#endif  // !_WIN32
