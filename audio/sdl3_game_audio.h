// Status: VALIDATED
/**
 * sdl3_game_audio.h — Host-only sound-resource playback boundary.
 *
 * This adapter is intentionally separate from the decompiled DirectSound
 * classes.  It consumes the same decoded RFH/RFD WAV assets selected by the
 * original resource IDs, but uses SDL3 streams on non-Windows hosts.
 */
#pragma once

#include <cstdint>

#ifndef _WIN32
// Preloads the WAVE selected by the original ResourceManager resource ID.
bool SDL3_GameAudioPreloadResource(uint32_t resource_id);

// Queues a one-shot WAVE selected by the original ResourceManager resource ID.
bool SDL3_GameAudioPlayResource(uint32_t resource_id);

// Reaps completed streams and reports whether one-shot audio is still queued.
bool SDL3_GameAudioPump();

// Plays a WAVE file from disk with optional looping.  Returns true on success.
// When looping is true, the stream cycles until explicit stop or shutdown.
bool SDL3_GameAudioPlayFile(const char* path, bool looping);

// Returns true when a looping file stream is currently active.
bool SDL3_GameAudioFilePlaying();

// Stops only the looping background-music stream, leaving one-shot
// streams (e.g. the exit sweep) intact for the drain loop.
void SDL3_GameAudioStopLooping();

// Stops and destroys every host-owned stream before SDL audio is shut down.
void SDL3_GameAudioStopAll();
#endif
