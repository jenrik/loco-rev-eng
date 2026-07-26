/**
 * AudioChannel.h — Per-channel audio playback state (0x3C bytes)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * AudioChannel manages a single DirectSound secondary buffer and its associated
 * playback state (position, attenuation, looping). Channels are created in a
 * fixed-size array owned by GameAudio (up to 16 channels by default). The
 * state machine tracks whether the channel is idle, loaded, paused, or
 * flagged for reclaiming.
 *
 * Size: 0x3C bytes (60)
 * No vtable (non-polymorphic struct/class)
 */

#pragma once

#include "../shared/types.h"

/* ------------------------------------------------------------------ */
/* AudioChannel state constants                                        */
/* ------------------------------------------------------------------ */
#define CHANNEL_STATE_IDLE      1   /* Initialized, no active playback */
#define CHANNEL_STATE_LOADED    2   /* DS buffer loaded (may be playing) */
#define CHANNEL_STATE_PAUSED    3   /* Playback paused */
#define CHANNEL_STATE_STOPPING  4   /* Flagged for reclaim by allocator */

struct AudioChannel {
    /* ============================================================ */
    /* Fields (offsets from this)                                    */
    /* ============================================================ */

    void**      output_ptr;         /* +0x00  pointer to caller's slot (SetOutput) */
    uint8_t     looping;            /* +0x04  0=oneshot, 1=looping */
    int32_t     attenuation_type;   /* +0x08  curve selector (1-4, used by SetAttenuation) */
    int32_t     attenuation_level;  /* +0x0c  0-100 applied level */
    int32_t     state;              /* +0x10  CHANNEL_STATE_* constant */
    void*       ds_buffer;          /* +0x14  IDirectSoundBuffer* (secondary) */
    int32_t     pos_x;              /* +0x18  current world X */
    int32_t     pos_y;              /* +0x1c  current world Y */
    int32_t     bounds_max_x;       /* +0x20  clamp maximum X */
    int32_t     bounds_max_y;       /* +0x24  clamp maximum Y */
    int32_t     attenuations[4];    /* +0x28  precomputed curve multipliers */
    int32_t     resource_id;        /* +0x38  sound resource identifier */

    /* ============================================================ */
    /* Methods (all __thiscall, no vtables)                          */
    /* ============================================================ */

    /**
     * Init — Reset channel to default state.
     * Address: 0x40EC70
     *
     * Sets attenuation_type=2, attenuation_level=100, state=IDLE.
     * Zeroes all positions, bounds, output_ptr, and ds_buffer.
     * Called once per channel during GameAudio::Init.
     */
    void Init();

    /**
     * Release — Stop playback and release the DS secondary buffer.
     * Address: 0x40ECA0
     *
     * Stops the DS buffer (vtable[0x48]=Stop), releases it (vtable[8]=Release),
     * clears the output pointer back-link, and resets state to IDLE.
     */
    void Release();

    /**
     * Reset — Clear only the output pointer back-link.
     * Address: 0x40ECF0
     *
     * If output_ptr is non-NULL, writes NULL to *output_ptr and NULLs the field.
     * Does NOT release the DS buffer or change state.
     */
    void Reset();

    /**
     * SetOutput — Record a caller-owned pointer and optionally set a back-link.
     * Address: 0x40ED10
     *
     * Stores param_1 as output_ptr. If param_1 is non-NULL, also writes `this`
     * into *param_1 so the caller can track which channel owns a slot.
     */
    void SetOutput(void** ptr);

    /**
     * LoadSound — Create a DS secondary buffer and load PCM data.
     * Address: 0x40ED20
     *
     * Full calling signature (6 stack params):
     *   LoadSound(IDirectSound* ds, SoundResource* res, int pos_x, int pos_y,
     *             int atten_type, uint8_t looping)
     *
     * Calls ds->CreateSoundBuffer(res->data, &ds_buffer). On DSERR_BUFFERLOST
     * (0x8878000a), calls GameAudio::StopFinished to reclaim DS memory, then retries.
     * Stores attenuation_type, resource_id, looping flag, then sets state=LOADED.
     */
    void LoadSound(void* ds_device, void* sound_resource, int32_t pos_x,
                   int32_t pos_y, int32_t atten_type, uint8_t looping);

    /**
     * Pause — Stop DS buffer playback, set state to PAUSED.
     * Address: 0x40EE00
     *
     * Calls ds_buffer->Stop() (vtable[0x48]), sets state=3.
     */
    void Pause();

    /**
     * Play — Start or resume playback from current position.
     * Address: 0x40EE20
     *
     * If state is LOADED and the DS buffer is already playing (GetStatus
     * returns DSBSTATUS_PLAYING), does nothing. Otherwise seeks to position 0
     * (via SetCurrentPosition, vtable[0x34]) and calls Play() (vtable[0x30])
     * with DSBPLAY_LOOPING if looping==1. Sets state to LOADED.
     */
    void Play();

    /**
     * IsPlaying — Check if channel is in IDLE (unloaded) state.
     * Address: 0x40EEA0
     *
     * Returns (state == CHANNEL_STATE_IDLE). Warning: despite the name, this
     * returns true when the channel has NO active DS buffer — it is used by
     * GameAudio::AllocChannel to find channels that can be reused without
     * needing a Release/cleanup step.
     */
    bool IsPlaying();

    /**
     * IsActive — Check if channel is currently active (loaded or playing).
     * Address: 0x40EEB0
     *
     * State IDLE or STOPPING: always active.
     * State LOADED: active if ds_buffer exists (checks via GetStatus).
     * State PAUSED: not active.
     */
    bool IsActive();

    /**
     * SetPosition — Set position bounds and update DS buffer pan.
     * Address: 0x40EF00
     *
     * Stores bounds_max_x and bounds_max_y, then calls UpdatePosition with
     * the current pos_x/pos_y to recalculate DS buffer pan.
     */
    void SetPosition(int32_t max_x, int32_t max_y);

    /**
     * UpdatePosition — Clamp and apply new world position to DS buffer.
     * Address: 0x40EF20
     *
     * Clamps pos_x to [0, bounds_max_x) and pos_y to [0, bounds_max_y),
     * stores them, then calls ds_buffer->SetPan(vtable[0x40]) with a computed
     * pan value based on the relative position.
     */
    void UpdatePosition(int32_t pos_x, int32_t pos_y);

    /**
     * SetBounds — Set attenuation curve multipliers and re-apply level.
     * Address: 0x40F040
     *
     * Stores the four curve multipliers (a through d) into the attenuations[4]
     * array, then re-runs ApplyAttenuation with the current attenuation_level.
     */
    void SetBounds(int32_t a, int32_t b, int32_t c, int32_t d);

    /**
     * SetAttenuation — Change attenuation curve type.
     * Address: 0x40F070
     *
     * Sets attenuation_type and re-runs ApplyAttenuation with current level.
     */
    void SetAttenuation(int32_t type);

    /**
     * ApplyAttenuation — Calculate and apply volume to DS buffer.
     * Address: 0x40F090
     *
     * Selects attenuation curve by type (1-4) from precomputed multipliers,
     * computes a log-scale volume value, and calls ds_buffer->SetVolume()
     * (vtable[0x3c]).
     */
    void ApplyAttenuation(int32_t level);
};
