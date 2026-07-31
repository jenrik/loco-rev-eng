/**
 * GameAudio.h — Top-level DirectSound audio manager class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * GameAudio owns the DirectSound device, creates/manages a fixed-size pool of
 * AudioChannel instances (default 16 channels), and exposes convenience
 * wrappers for playing sound effects by resource ID, muting, and listener
 * position management.
 *
 * Size: 0xB8 bytes (184)
 * Vtable address in loco.exe: 0x477894
 *   [0] compiler-generated deleting-destructor slot wrapping 0x412C20
 *
 * Class hierarchy:
 *   (standalone class, no parent)
 */

#pragma once

#include "../shared/types.h"
#include "AudioChannel.h"

class GameAudio {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* vtable at +0x00 is compiler-managed via virtual methods */
    int32_t     saved_bounds[4];        /* +0x04  saved volume bounds (backup) */
    int32_t     active_bounds[4];       /* +0x14  active volume bounds */
    uint8_t     pad_24[0x60];           /* +0x24  zeroed buffer (WAVEFORMATEX at +0x24) */
    /* +0x84: DirectSound device enumeration callback area (8 bytes?) */
    int32_t     max_channels;           /* +0x94  number of AudioChannels */
    AudioChannel* channels;             /* +0x98  array of max_channels * 0x3C bytes */
    uint32_t*   channel_usage;          /* +0x9c  per-channel allocation timestamps */
    int32_t     listener_x;             /* +0xa0  listener world X */
    int32_t     listener_y;             /* +0xa4  listener world Y */
    AudioDirectSoundDevice* ds_device;  /* +0xa8  IDirectSound* */
    AudioDirectSoundBuffer* ds_primary;  /* +0xac  IDirectSoundBuffer* (primary) */
    int32_t     flags;                  /* +0xb0  flags (set to 0) */
    uint8_t     muted;                  /* +0xb4  mute flag (0=unmuted) */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * GameAudio constructor.
     * Address: 0x412BD0
     *
     * C++ construction installs the vtable and zeros all fields.
     * Called from DDRAW_InitAudio after heap allocation of 0xB8 bytes.
     */
    GameAudio();
    GameAudio(const GameAudio&) = delete;
    GameAudio& operator=(const GameAudio&) = delete;

    /**
     * Destructor (vtable[0]).
     * Address: 0x412C20
     *
     * Calls Cleanup(); the compiler supplies the deleting-destructor wrapper.
     */
    virtual ~GameAudio();

    /* ================================================================ */
    /* Init / Cleanup                                                    */
    /* ================================================================ */

    /**
     * Init — Initialize DirectSound and create channel pool.
     * Address: 0x412C50
     *
     * Enumerates or creates a DirectSound device, creates primary buffer
     * (PCM 22050Hz 16-bit stereo), allocates channel array and per-channel
     * initialization map, initialises all AudioChannels, sets default
     * volume bounds. Returns 0 on success.
     */
    uint32_t Init();

    /**
     * Cleanup — Release all resources.
     * Address: 0x412EE0
     *
     * Releases all channels (CGWND_AudioChannel_Release), frees channel
     * arrays, releases primary buffer and DirectSound device.
     */
    void Cleanup(void* hwnd);

    /* ================================================================ */
    /* Playback methods                                                  */
    /* ================================================================ */

    /**
     * Play — Delegate sound playback to DirectSound device.
     * Address: 0x413070
     *
     * Calls the recovered DirectSound device slot 3. Returns -1 if no device.
     */
    int32_t Play(void* param1, void* param2, void* param3);

    /**
     * PlayResource — Play a named resource sound at listener position.
     * Address: 0x413180
     *
     * Looks up resource by ID via RESMGR_GetById, then allocates a channel
     * at the global listener position with attenuation=4, no looping.
     * Used for palette click sounds (Game_SelectGameObject).
     */
    void PlayResource(uint32_t resource_id);

    /**
     * PlayResourceEx — Play resource with caller-provided output pointer.
     * Address: 0x4131C0
     *
     * Like PlayResource, but releases any existing channel at *output_ptr
     * before allocating, and stores the new channel pointer back.
     */
    void PlayResourceEx(uint32_t resource_id, AudioChannel** output_ptr);

    /**
     * AllocChannel — Core channel allocator with priority-stealing.
     * Address: 0x413210
     *
     * Finds or reuses a channel for the given sound resource. Implements
     * per-sound instance limits and cooldown timers. Supports priority-
     * based channel stealing when all channels are in use.
     *
     * @param resource    SoundResource to play
     * @param output_ptr  Optional: stores channel* on success
     * @param pos_x       World X position for 3D pan
     * @param pos_y       World Y position for 3D pan
     * @param attenuation Attenuation curve type (1-4)
     * @param looping     0=oneshot, 1=looping
     */
    void AllocChannel(SoundResource* resource, AudioChannel** output_ptr,
                      int32_t pos_x, int32_t pos_y,
                      uint32_t attenuation, uint8_t looping);

    /* ================================================================ */
    /* Control methods                                                   */
    /* ================================================================ */

    /**
     * SetListenerPos — Update listener position across all channels.
     * Address: 0x4130A0
     *
     * Stores listener_x/y and calls AudioChannel::SetPosition on each
     * channel to recalculate 3D pan values.
     */
    void SetListenerPos(int32_t x, int32_t y);

    /**
     * StopFinished — Stop all active channels.
     * Address: 0x4130F0
     *
     * Iterates channels and releases any that are active (IsActive).
     * Called from LoadSound when DSERR_BUFFERLOST occurs.
     */
    void StopFinished();

    /**
     * StopAll — Release every channel unconditionally.
     * Address: 0x413140
     *
     * Unlike StopFinished, does not check IsActive — releases all.
     * Called from RESMGR_Shutdown during resource manager teardown.
     */
    void StopAll();

    /**
     * SetMute — Hard mute/unmute all channels.
     * Address: 0x413530
     *
     * mute=1: zeroes active volume bounds (silent).
     * mute=0: restores from saved_bounds to active_bounds.
     * Propagates to all channels via AudioChannel::SetBounds.
     */
    void SetMute(uint8_t mute);

    /**
     * UpdateVolume — Lightweight silence/restore for UI transitions.
     * Address: 0x4135B0
     *
     * Unlike SetMute, does NOT touch the mute flag or saved_bounds.
     * Used by CGWND_SetMode when entering/exiting menu/postcard screens.
     */
    void UpdateVolume(uint8_t silence);

    /**
     * SetBounds — Store volume boundaries and re-apply mute state.
     * Address: 0x413630
     *
     * @param volume_low    low volume bound
     * @param volume_medium medium volume bound
     * @param volume_high   high volume bound
     * @param volume_max    max volume bound
     */
    void SetBounds(int32_t volume_low, int32_t volume_medium,
                   int32_t volume_high, int32_t volume_max);
};
