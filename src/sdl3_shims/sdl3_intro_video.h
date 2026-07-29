#pragma once

#ifndef _WIN32

namespace loco::intro {

// Host-only SDL3/GStreamer replacement for the original MCIWnd child-video
// player (RESDATA_CtorBase, 0x454380). The original Win32 path remains intact.
bool startLaunchSequence();
bool isActive();
// Pumps GStreamer, renders the latest decoded frame, and returns whether a
// clip remains active after processing EOS/error transitions.
bool pumpAndRender();
void skipCurrent();
void stop();

}  // namespace loco::intro

#endif  // !_WIN32
