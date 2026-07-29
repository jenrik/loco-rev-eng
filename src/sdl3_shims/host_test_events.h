#pragma once

#ifndef _WIN32

namespace loco::host_test {

// Passive, host-only observability for external GUI tests. Events are emitted
// only when LEGO_LOCO_TEST_EVENTS names a writable JSONL artifact. Input still
// enters through SDL/Wayland; this protocol never drives game behavior.
void emit_process_started();
void emit_mode_changed(int old_mode, int new_mode);
void emit_screen_presented(const char* screen, int dialog_state);
void emit_search_completed(int sessions);
void emit_clean_shutdown();

}  // namespace loco::host_test

#endif  // !_WIN32
