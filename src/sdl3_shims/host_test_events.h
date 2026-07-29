#pragma once

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
// Emitted only after SDL accepted the original resource WAVE into a stream.
void emit_audio_queued(uint32_t resource_id);
// Passive record of the resolved PlayerConfig name accepted by Go.
void emit_player_name_committed(const char* name);
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
