#pragma once

#include <cstddef>
#include <cstdint>

#ifndef _WIN32

namespace loco::host_test {

// Passive, host-only observability for external GUI tests. Events are emitted
// only when LEGO_LOCO_TEST_EVENTS names a writable JSONL artifact. Input still
// enters through SDL/Wayland; this protocol never drives game behavior.
void emit_process_started();
void emit_mode_changed(int old_mode, int new_mode);
void emit_screen_presented(const char* screen, int dialog_state);
void emit_search_completed(int sessions);
void emit_transport_listening(uint16_t port);
void emit_transport_connected(uint32_t player_id);
void emit_netman_session_ready(uint32_t player_id, bool hosting);
void emit_netman_player_joined(uint32_t player_id);
void emit_netman_message_processed(int type, int flags);
void emit_netman_vehicle_adopted(uint32_t editor_count, uint16_t network_id,
                                 uint32_t list_depth);
void emit_legacy_service_applied(uint32_t packet_type, std::size_t byte_count);
void emit_legacy_attachment_updated(
    uint32_t sender, uint16_t train_type, uint16_t sequence, uint8_t subtype,
    std::size_t attachment_bytes, std::size_t final_bytes, bool complete);
void emit_legacy_asset_consumed(uint8_t mode, uint8_t type,
                                std::size_t byte_count);
void emit_legacy_asset_owned(uint8_t mode, uint8_t type, std::size_t byte_count,
                             bool replaced, std::size_t asset_count);
void emit_legacy_track_sessions_materialized(
    uint32_t session_count, uint32_t vehicle_count, uint32_t editor_count,
    int32_t config, uint16_t first_entry_count, uint8_t first_signal_type);
void emit_netman_ping_updated(int32_t dp_id, uint8_t slot, uint8_t peer,
                              int32_t x, int32_t y);
void emit_netman_pixel_data_updated(int slot, int byte_count,
                                    uint16_t width, uint16_t height);
// Passive record of a host-only layout choice projected into Netman.
void emit_layout_selected(int columns, int rows, int slots);
// Emitted only after SDL accepted the original resource WAVE into a stream.
void emit_audio_queued(uint32_t resource_id);
// Passive record of the resolved PlayerConfig name accepted by Go.
void emit_player_name_committed(const char* name);
void emit_menu_mode_selected(bool multiplayer);
// Passive launch-video observability. It reports decoded presentation and
// terminal states but never controls playback or input.
void emit_intro_video_started(int index, const char* path);
void emit_intro_video_frame(int index, int width, int height);
void emit_intro_video_finished(int index, bool skipped);
void emit_intro_video_failed(int index, const char* message);
void emit_intro_sequence_complete();
void emit_clean_shutdown();

}  // namespace loco::host_test

#endif  // !_WIN32
