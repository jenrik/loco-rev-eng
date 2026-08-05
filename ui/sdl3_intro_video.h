#pragma once

#ifndef _WIN32

#include <array>
#include <cstddef>
#include <string_view>

namespace loco::intro {

// Original UI-main-menu sequence, recovered from the state-zero render at
// 0x421EB0 and the MM_MCINOTIFY handler at 0x420F7F: legoSpin → IgSpin →
// the [Video]/Dir INI target (locointr.avi in the shipped LEGO.INI).
inline constexpr std::array<std::string_view, 3> kOriginalLaunchVideoPaths = {
    "art-res/video/legospin.avi",
    "art-res/video/IgSpin.avi",
    "Video/locoIntr.avi",
};

// Host-only SDL3/GStreamer replacement for the original MCIWnd child-video
// player (RESDATA_InitScriptEngine 0x454250 / RESDATA_CtorBase 0x454380).
bool startLaunchSequence();
bool isActive();
// Pumps GStreamer, renders the latest decoded frame, and returns whether a
// clip remains active after processing EOS/error transitions.
bool pumpAndRender();
// Original video child subclass 0x4207C0 maps every keyboard/button-down
// event to parent message 0x40A, whose 0x420F6F handler enters menu state 7.
// Therefore an input abandons the entire remaining sequence, not one clip.
void skipAll();
void stop();

}  // namespace loco::intro

#endif  // !_WIN32
