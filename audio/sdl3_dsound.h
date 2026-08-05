/**
 * sdl3_dsound.h — DirectSound → SDL3 audio compatibility shim
 *
 * Provides IDirectSound and IDirectSoundBuffer implementations
 * backed by SDL3 audio. Drop-in replacement for stubs/dsound.h.
 *
 * NOT part of the Lego Loco reverse-engineering project.
 */

#ifndef LOCO_SDL3_DSOUND_H
#define LOCO_SDL3_DSOUND_H

#include "sdl3_types.h"
#include <SDL3/SDL.h>

#ifndef _WIN32

struct IDirectSoundBuffer;

/* =========================================================================
 * IDirectSound — SDL3-backed audio device
 * ========================================================================= */

struct IDirectSound {
    SDL_AudioDeviceID device_id;
    int               sample_rate;
    int               channels;
    bool              initialized;

    IDirectSound();
    ~IDirectSound();

    int  Release();
    int  SetCooperativeLevel(void* hwnd, int level);
    int  CreateSoundBuffer(DSBUFFERDESC* desc, IDirectSoundBuffer** out,
                           void* wave_data);
};

/* =========================================================================
 * IDirectSoundBuffer — SDL3-backed sound buffer
 * ========================================================================= */

struct IDirectSoundBuffer {
    SDL_AudioStream* stream;       /* Audio stream for playback              */
    uint8_t*         audio_data;    /* Raw PCM data (owned)                   */
    uint32_t         data_len;      /* Length of audio_data in bytes          */
    WAVEFORMATEX     format;        /* Audio format                          */
    bool             playing;
    bool             looping;
    int              volume;        /* 0 (silent) to 100 (full)              */

    IDirectSoundBuffer();
    ~IDirectSoundBuffer();

    int  Play(uint32_t reserved1, uint32_t reserved2, uint32_t flags);
    int  Stop();
    int  SetVolume(int32_t vol);     /* DirectSound volume: 0=max, -10000=silent */
    int  SetPan(int32_t pan);
    int  SetFrequency(uint32_t freq);
    int  GetStatus(uint32_t* out_status);
    int  SetCurrentPosition(uint32_t pos);
    int  Lock(uint32_t offset, uint32_t bytes, void** ptr1, uint32_t* bytes1,
             void** ptr2, uint32_t* bytes2, uint32_t flags);
    int  Unlock(void* ptr1, uint32_t bytes1, void* ptr2, uint32_t bytes2);
    int  Release();
};

/* =========================================================================
 * Helper: DirectSoundCreate
 * ========================================================================= */

int DirectSoundCreate(void* guid, IDirectSound** ppDS, void* unk);

/* =========================================================================
 * DS_Init / DS_SaveAndShutdown
 * ========================================================================= */

uint32_t DS_Init(void);
void     DS_SaveAndShutdown(void);

/* =========================================================================
 * Globals
 * ========================================================================= */

extern IDirectSound* g_pDS;

#endif /* _WIN32 */

#endif /* LOCO_SDL3_DSOUND_H */
