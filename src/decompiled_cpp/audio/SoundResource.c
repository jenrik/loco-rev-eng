/**
 * SoundResource.c — Sound resource manager functions (C free functions)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Sound resource lifecycle functions. Sound objects are refcounted
 * wrappers around DirectSound secondary buffers. RESMGR_LoadSoundResource
 * loads a .wav file into a DS buffer, RESMGR_ReleaseSoundResource
 * decrements the refcount and releases the buffer when it hits zero.
 *
 * RESMGR_SoundObject_Ctor constructs a SoundObject (a TrackPiece
 * subclass with text label, used for sound-related UI elements).
 */

#include "../shared/types.h"

/* ================================================================== */
/* External declarations                                               */
/* ================================================================== */

struct GameAudio;

extern GameAudio* g_audio;                /* 0x4FD3BC */

/* DirectSound interface — vtable dispatch helpers */
extern const char* __cdecl DDRAW_GetDsoundErrorString(int32_t hr);
extern void* __cdecl operator_new(size_t size);       /* 0x465CE0 */
extern void  __cdecl CRT_free(void* ptr);               /* 0x465CD0 */

/* Game_LoadWaveFile — load WAV data from file into PCM buffer */
extern uint32_t __cdecl Game_LoadWaveFile(const char* filePath, void* pcmBuffer);

/* GameAudio_Play — create/return DS secondary buffer from PCM format descriptor */
extern int32_t __fastcall GameAudio_Play(GameAudio* audio, void* formatDesc,
                                           void** outBuffer, int32_t flags);

/* TrackPiece constructor (base class for SoundObject) */
extern void __thiscall TrackPiece_Ctor(void* this, int32_t param2,
                                         int32_t param3, uint16_t param5);

/* ================================================================== */
/* SoundObject structure layout                                        */
/* SoundObject inherits from TrackPiece (which inherits from GameObj). */
/* Key fields:                                                         */
/*   +0x00: vtable (VTBL_SOUND_OBJECT = 0x478280)                     */
/*   +0x04: type/subtype (set to 8)                                   */
/*   +0x08..+0x5B: inherited (TrackPiece/GameObject)                  */
/*   +0x5C: text length (param_1 from Ctor)                           */
/*   +0x60: text buffer pointer (allocated copy of empty string)      */
/*   +0x58: flag byte (set to 0)                                      */
/*   +0x64: font/data param_4                                         */
/* ================================================================== */

/* ================================================================== */
/* RESMGR_LoadSoundResource — 0x448D60                                  */
/*                                                                      */
/* Load a .wav file into a DirectSound secondary buffer. Called on a   */
/* SoundResource object (RESDATA struct with +0x0C = DS buffer ptr).   */
/*                                                                      */
/* Fields accessed on the resource object:                              */
/*   +0x00..+0x07: magic/type info                                     */
/*   +0x08: loaded flag (set to 1 on success)                          */
/*   +0x09: load status byte (set to 1 on success)                     */
/*   +0x0C: DS secondary buffer pointer (IDirectSoundBuffer*)          */
/*   +0x18: file path / resource name                                  */
/*   +0x120: refcount                                                  */
/*   +0x128: PCM format flags (little-endian? if non-zero, use 0x80CA)*/
/*                                                                      */
/* Flow:                                                                */
/*   1. Increment refcount at +0x120                                   */
/*   2. If buffer at +0x0C is already loaded, return (refcount++)      */
/*   3. Call Game_LoadWaveFile to load .wav into PCM buffer            */
/*   4. Build WAVEFORMATEX-style descriptor from loaded PCM data       */
/*   5. Call GameAudio_Play to create DS secondary buffer              */
/*   6. Lock DS buffer, copy PCM data, unlock                          */
/*   7. Mark +0x09 = 1 (loaded), return                                */
/*                                                                      */
/* __fastcall (ECX = resource object). Returns 1 on success.           */
/* ================================================================== */
uint32_t __fastcall RESMGR_LoadSoundResource(int32_t resourceObj)
{
    void* ds_buffer;           /* +0x0C */
    uint32_t result;
    uint32_t pcm_data[8];      /* WAVEFORMATEX descriptor */
    uint32_t format_dw[4];     /* WAVEFORMATEX for DS */
    uint32_t* pcm_ptr;
    void* locked_ptr;
    uint32_t locked_size;
    int32_t  hr;
    uint32_t wave_type;
    uint32_t extra;

    if (g_audio == NULL) return 0;

    ds_buffer = *(void**)(resourceObj + 0x0C);
    /* BUG: the refcount at +0x120 is increment/decrement on a field
       that overlaps with ds_buffer in some layouts. The original code
       uses raw offsets. */
    *(int32_t*)(resourceObj + 0x120) += 1;

    /* Already loaded? */
    if (*(int32_t*)(resourceObj + 0x0C) != 0) {
        return 1;
    }

    *(uint8_t*)(resourceObj + 9) = 0;

    /* Clear PCM descriptor */
    {
        int32_t i;
        for (i = 0; i < 8; i++) pcm_data[i] = 0;
    }

    /* Load WAV file */
    result = Game_LoadWaveFile((const char*)(resourceObj + 0x18), (int32_t)pcm_data);
    if (result != 0) {
        return result & 0xFFFFFF00;
    }

    if (pcm_data[4] == 0) {  /* data size check */
        return 0;
    }

    /* Build WAVEFORMATEX (0x14 = sizeof(PCMWAVEFORMAT)) */
    format_dw[0] = 0x14;               /* wFormatTag + nChannels + nSamplesPerSec */
    format_dw[1] = 0xCA;               /* nAvgBytesPerSec (default) */
    format_dw[2] = 0;                  /* nBlockAlign */
    format_dw[3] = 0;                  /* wBitsPerSample + cbSize */

    /* If +0x128 has a flag, adjust nAvgBytesPerSec */
    if (*(uint8_t*)(resourceObj + 0x128) != 0) {
        format_dw[1] = 0x80CA;
    }

    /* PCM data pointer (after WAVEFORMATEX header) */
    pcm_ptr = pcm_data + 1;

    /* Set wave type from loaded data */
    wave_type = 0x14;  /* sizeof(PCMWAVEFORMATEX) */
    hr = GameAudio_Play(g_audio, format_dw, (void**)(resourceObj + 0x0C), 0);
    if (hr != 0) {
        /* DS error — get error string */
        const char* err = DDRAW_GetDsoundErrorString(hr);
        return (uint32_t)err & 0xFFFFFF00;
    }

    /* Lock DS buffer to copy PCM data */
    locked_ptr = NULL;
    locked_size = 0;
    result = (*(uint32_t(__stdcall**)(int32_t*, uint32_t, uint32_t, void**, uint32_t*, void*, uint32_t, uint32_t))
        (*(*(int32_t**)ds_buffer + 0x2C)))  /* IDirectSoundBuffer::Lock */
        (ds_buffer, 0, 0, &locked_ptr, &locked_size, NULL, 0, 2);
    if (result != 0) {
        return result & 0xFFFFFF00;
    }

    /* Copy PCM data into locked buffer */
    {
        uint32_t* src = pcm_ptr;
        uint32_t* dst = (uint32_t*)locked_ptr;
        uint32_t i;
        for (i = locked_size >> 2; i != 0; i--) {
            *dst = *src;
            src++; dst++;
        }
        for (i = locked_size & 3; i != 0; i--) {
            *(uint8_t*)dst = *(uint8_t*)src;
            src = (uint32_t*)((int8_t*)src + 1);
            dst = (uint32_t*)((int8_t*)dst + 1);
        }
    }

    /* Unlock */
    (*(void(__stdcall**)(void*, void*, uint32_t, void*, uint32_t))
        (*(*(int32_t**)ds_buffer + 0x4C)))
        (ds_buffer, locked_ptr, locked_size, NULL, 0);

    /* Free the PCM data that Game_LoadWaveFile allocated */
    if (locked_size != 0) {
        CRT_free(locked_ptr);
    }

    *(uint8_t*)(resourceObj + 9) = 1;
    return 1;
}

/* ================================================================== */
/* RESMGR_ReleaseSoundResource — 0x448EE0                               */
/*                                                                      */
/* Release a refcounted sound resource. Decrements refcount at +0x120. */
/* When refcount reaches 0 AND a DS buffer exists at +0x0C AND the     */
/* resource is not flagged as persistent (+0x08 != 1), stops and       */
/* releases the DirectSound buffer.                                     */
/*                                                                      */
/* Called by: PlaySoundFile after playback completes.                   */
/* ================================================================== */
uint32_t __fastcall RESMGR_ReleaseSoundResource(int32_t resourceObj)
{
    int32_t* refcount;

    /* Decrement refcount */
    refcount = (int32_t*)(resourceObj + 0x120);
    if (*refcount > 0) {
        *refcount = *refcount - 1;
    }

    /* Check if we should release the DS buffer */
    if (*refcount == 0) {
        int32_t* ds_buf = *(int32_t**)(resourceObj + 0x0C);
        if (ds_buf != NULL && *(uint8_t*)(resourceObj + 8) != 1) {
            /* Stop the buffer */
            (*(void(__stdcall**)(int32_t*))(ds_buf[0x48/4]))(ds_buf);
            /* Release the buffer */
            (*(int32_t(__stdcall**)(int32_t*))(ds_buf[8/4]))(ds_buf);
            *(int32_t*)(resourceObj + 0x0C) = 0;
        }
    }

    return 1;
}

/* ================================================================== */
/* RESMGR_SoundObject_Ctor — 0x448F30                                   */
/*                                                                      */
/* Construct a SoundObject (TrackPiece subclass with text label).      */
/*                                                                      */
/* Steps:                                                               */
/*   1. Call TrackPiece_Ctor(this, param_2, param_3, param_5)         */
/*   2. Set +0x5C = param_1 (text buffer length)                       */
/*   3. Override vtable to VTBL_SOUND_OBJECT (0x478280)               */
/*   4. Set +0x04 = 8 (type)                                           */
/*   5. Allocate text buffer of param_1+1 bytes, copy empty string     */
/*   6. Set +0x58 = 0 (flag)                                           */
/*   7. Set +0x64 = param_4 (font reference)                           */
/* ================================================================== */
void* __thiscall RESMGR_SoundObject_Ctor(void* this,
                                           int32_t textLen,
                                           int32_t param_2,
                                           int32_t param_3,
                                           uint32_t param_4,
                                           uint16_t param_5)
{
    /* Call TrackPiece base constructor */
    TrackPiece_Ctor(this, param_2, param_3, param_5);

    *(int32_t*)((int8_t*)this + 0x5C) = textLen;

    /* Override vtable */
    *(void***)this = (void**)0x00478280;  /* VTBL_SOUND_OBJECT */
    *(int32_t*)((int8_t*)this + 4) = 8;

    /* Allocate and initialize text buffer */
    {
        char* textBuf = (char*)operator_new(textLen + 1);
        *(char**)((int8_t*)this + 0x60) = textBuf;

        if (textBuf != NULL) {
            const char* emptyStr = (const char*)0x4851D0;  /* g_empty_string */
            char* dst = textBuf;
            const char* src = emptyStr;
            uint32_t len;

            for (len = 0; src[len] != '\0'; len++) {}

            {
                uint32_t i;
                for (i = len >> 2; i != 0; i--) {
                    *(uint32_t*)dst = *(uint32_t*)src;
                    src += 4; dst += 4;
                }
                for (i = len & 3; i != 0; i--) {
                    *dst = *src;
                    src++; dst++;
                }
            }
        }
    }

    *(uint8_t*)((int8_t*)this + 0x58) = 0;
    *(uint32_t*)((int8_t*)this + 0x64) = param_4;

    return this;
}
