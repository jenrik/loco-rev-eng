/**
 * sdl3_dsound_adapter.h — bridges audio/AudioChannel.h's declared
 * AudioDirectSoundDevice/AudioDirectSoundBuffer virtual interfaces to the
 * concrete SDL3 backend (audio/sdl3_dsound.h).
 *
 * GameAudio/AudioChannel already call the declared interfaces by name (real
 * virtual dispatch, not raw vtable-slot indexing) -- the gap this file closes
 * is that nothing implements them: `sdl3_dsound.h`'s `IDirectSound`/
 * `IDirectSoundBuffer` are a separate, non-virtual, concrete struct pair with
 * a narrower method set. A real inheritance relationship isn't viable
 * (sdl3_dsound.h is explicitly "NOT part of the Lego Loco reverse-engineering
 * project" and is missing ~11 declared methods this interface requires), so
 * this is a thin adapter: each class here owns one sdl3_dsound object and
 * forwards every call with a live caller to it, matching the existing
 * `resources/sprite_uipanel_adapter.cpp` pattern for the same kind of gap.
 */
#pragma once

#ifndef _WIN32

class AudioDirectSoundDevice;

// Constructs a real device via sdl3_dsound.h's DirectSoundCreate() and wraps
// it as a AudioDirectSoundDevice*. Returns nullptr on failure (matching
// DirectSoundCreate's own failure contract).
AudioDirectSoundDevice* Sdl3CreateAudioDirectSoundDevice();

#endif  // !_WIN32
