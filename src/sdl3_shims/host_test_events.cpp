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
    void transport_listening(uint16_t port) {
        write("transport_listening", ",\"port\":%u", static_cast<unsigned>(port));
    }
    void transport_connected(uint32_t player_id) {
        write("transport_connected", ",\"player_id\":%u", player_id);
    }
    void netman_session_ready(uint32_t player_id, bool hosting) {
        write("netman_session_ready",
              ",\"player_id\":%u,\"hosting\":%s",
              player_id, hosting ? "true" : "false");
    }
    void netman_player_joined(uint32_t player_id) {
        write("netman_player_joined", ",\"player_id\":%u", player_id);
    }
    void netman_vehicle_adopted(uint32_t editor_count, uint16_t network_id,
                                uint32_t list_depth) {
        write("netman_vehicle_adopted",
              ",\"editor_count\":%u,\"network_id\":%u,"
              "\"list_depth\":%u", editor_count, network_id, list_depth);
    }
    void netman_message_processed(int type, int flags) {
        write("netman_message_processed",
              ",\"type\":%d,\"flags\":%d", type, flags);
    }
    void legacy_service_applied(uint32_t packet_type, std::size_t byte_count) {
        write("legacy_service_applied",
              ",\"packet_type\":%u,\"byte_count\":%zu",
              packet_type, byte_count);
    }
    void legacy_attachment_updated(
        uint32_t sender, uint16_t train_type, uint16_t sequence, uint8_t subtype,
        std::size_t attachment_bytes, std::size_t final_bytes, bool complete) {
        write("legacy_attachment_updated",
              ",\"sender\":%u,\"train_type\":%u,\"sequence\":%u,"
              "\"subtype\":%u,\"attachment_bytes\":%zu,"
              "\"final_bytes\":%zu,\"complete\":%s",
              sender, train_type, sequence, subtype, attachment_bytes,
              final_bytes, complete ? "true" : "false");
    }
    void legacy_asset_owned(uint8_t mode, uint8_t type, std::size_t byte_count,
                            bool replaced, std::size_t asset_count) {
        write("legacy_asset_owned",
              ",\"mode\":%u,\"type\":%u,\"byte_count\":%zu,"
              "\"replaced\":%s,\"asset_count\":%zu",
              mode, type, byte_count, replaced ? "true" : "false", asset_count);
    }
    void legacy_track_sessions_materialized(
        uint32_t session_count, uint32_t vehicle_count, uint32_t editor_count,
        int32_t config, uint16_t first_entry_count, uint8_t first_signal_type) {
        write("legacy_track_sessions_materialized",
              ",\"session_count\":%u,\"vehicle_count\":%u,"
              "\"editor_count\":%u,\"config\":%d,"
              "\"first_entry_count\":%u,\"first_signal_type\":%u",
              session_count, vehicle_count, editor_count, config,
              first_entry_count, first_signal_type);
    }
    void netman_ping_updated(int32_t dp_id, uint8_t slot, uint8_t peer,
                             int32_t x, int32_t y) {
        write("netman_ping_updated",
              ",\"dp_id\":%d,\"slot\":%u,\"peer\":%u,\"x\":%d,\"y\":%d",
              dp_id, slot, peer, x, y);
    }
    void netman_pixel_data_updated(int slot, int byte_count,
                                   uint16_t width, uint16_t height) {
        write("netman_pixel_data_updated",
              ",\"slot\":%d,\"byte_count\":%d,\"width\":%u,\"height\":%u",
              slot, byte_count, width, height);
    }
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

    void menu_mode_selected(bool multiplayer) {
        write("menu_mode_selected", ",\"multiplayer\":%s",
              multiplayer ? "true" : "false");
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
void emit_transport_listening(uint16_t port) { sink().transport_listening(port); }
void emit_transport_connected(uint32_t player_id) { sink().transport_connected(player_id); }
void emit_netman_session_ready(uint32_t player_id, bool hosting) {
    sink().netman_session_ready(player_id, hosting);
}
void emit_netman_player_joined(uint32_t player_id) {
    sink().netman_player_joined(player_id);
}
void emit_netman_vehicle_adopted(uint32_t editor_count, uint16_t network_id,
                                 uint32_t list_depth) {
    sink().netman_vehicle_adopted(editor_count, network_id, list_depth);
}
void emit_netman_message_processed(int type, int flags) {
    sink().netman_message_processed(type, flags);
}
void emit_legacy_service_applied(uint32_t packet_type, std::size_t byte_count) {
    sink().legacy_service_applied(packet_type, byte_count);
}
void emit_legacy_attachment_updated(
    uint32_t sender, uint16_t train_type, uint16_t sequence, uint8_t subtype,
    std::size_t attachment_bytes, std::size_t final_bytes, bool complete) {
    sink().legacy_attachment_updated(
        sender, train_type, sequence, subtype, attachment_bytes, final_bytes,
        complete);
}
void emit_legacy_asset_owned(uint8_t mode, uint8_t type, std::size_t byte_count,
                             bool replaced, std::size_t asset_count) {
    sink().legacy_asset_owned(mode, type, byte_count, replaced, asset_count);
}
void emit_legacy_track_sessions_materialized(
    uint32_t session_count, uint32_t vehicle_count, uint32_t editor_count,
    int32_t config, uint16_t first_entry_count, uint8_t first_signal_type) {
    sink().legacy_track_sessions_materialized(
        session_count, vehicle_count, editor_count, config,
        first_entry_count, first_signal_type);
}
void emit_netman_ping_updated(int32_t dp_id, uint8_t slot, uint8_t peer,
                              int32_t x, int32_t y) {
    sink().netman_ping_updated(dp_id, slot, peer, x, y);
}
void emit_netman_pixel_data_updated(int slot, int byte_count,
                                    uint16_t width, uint16_t height) {
    sink().netman_pixel_data_updated(slot, byte_count, width, height);
}
void emit_layout_selected(int columns, int rows, int slots) {
    sink().layout_selected(columns, rows, slots);
}
void emit_audio_queued(uint32_t resource_id) { sink().audio_queued(resource_id); }
void emit_player_name_committed(const char* name) { sink().player_name_committed(name); }
void emit_menu_mode_selected(bool multiplayer) { sink().menu_mode_selected(multiplayer); }
void emit_intro_video_started(int index, const char* path) { sink().intro_video_started(index, path); }
void emit_intro_video_frame(int index, int width, int height) { sink().intro_video_frame(index, width, height); }
void emit_intro_video_finished(int index, bool skipped) { sink().intro_video_finished(index, skipped); }
void emit_intro_video_failed(int index, const char* message) { sink().intro_video_failed(index, message); }
void emit_intro_sequence_complete() { sink().event("intro_sequence_complete"); }
void emit_clean_shutdown() { sink().event("clean_shutdown"); }

}  // namespace loco::host_test

#endif  // !_WIN32
