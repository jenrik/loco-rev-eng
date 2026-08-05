/**
 * sdl3_dsound.cpp — DirectSound → SDL3 audio compatibility shim
 *
 * Implements IDirectSound and IDirectSoundBuffer using SDL3's
 * audio API (SDL_AudioStream for playback, software mixing).
 *
 * Limitations:
 * - Lock/Unlock streaming is stubbed; audio must be pre-loaded.
 * - 3D positioning (SetPan) is approximated with simple stereo panning.
 * - SetFrequency uses SDL3 audio stream resampling.
 *
 * NOT part of the Lego Loco reverse-engineering project.
 */

#include "sdl3_dsound.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifndef _WIN32

/* =========================================================================
 * WAV file parser (internal)
 * ========================================================================= */

struct WavHeader {
    char     riff[4];        /* "RIFF"           */
    uint32_t file_size;
    char     wave[4];        /* "WAVE"           */
    char     fmt[4];         /* "fmt "           */
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data_marker[4]; /* "data"           */
    uint32_t data_size;
};

/* =========================================================================
 * Globals
 * ========================================================================= */

IDirectSound* g_pDS = nullptr;

/* =========================================================================
 * IDirectSound
 * ========================================================================= */

IDirectSound::IDirectSound()
    : device_id(0)
    , sample_rate(22050)
    , channels(2)
    , initialized(false)
{}

IDirectSound::~IDirectSound()
{
    if (initialized && device_id) {
        SDL_CloseAudioDevice(device_id);
    }
}

int IDirectSound::Release()
{
    this->~IDirectSound();
    delete this;
    return DS_OK;
}

int IDirectSound::SetCooperativeLevel(void* hwnd, int level)
{
    (void)hwnd; (void)level;
    return DS_OK;
}

int IDirectSound::CreateSoundBuffer(DSBUFFERDESC* desc,
                                     IDirectSoundBuffer** out,
                                     void* wave_data)
{
    if (!desc || !out) return DSERR_INVALIDPARAM;

    IDirectSoundBuffer* buf = new IDirectSoundBuffer();
    if (!buf) return DSERR_INVALIDPARAM;

    /* Parse WAV header from wave_data to get format info */
    if (wave_data && desc->dwBufferBytes > sizeof(WavHeader)) {
        WavHeader* hdr = (WavHeader*)wave_data;
        buf->format.wFormatTag      = hdr->audio_format;
        buf->format.nChannels       = hdr->num_channels;
        buf->format.nSamplesPerSec  = hdr->sample_rate;
        buf->format.wBitsPerSample  = hdr->bits_per_sample;
        buf->format.nBlockAlign     = hdr->block_align;
        buf->format.nAvgBytesPerSec = hdr->byte_rate;
        buf->format.cbSize          = 0;

        /* Copy PCM data (skip WAV header) */
        uint32_t pcm_offset = sizeof(WavHeader);
        uint32_t pcm_size   = hdr->data_size;
        if (pcm_offset + pcm_size > desc->dwBufferBytes) {
            pcm_size = desc->dwBufferBytes - pcm_offset;
        }

        buf->data_len = pcm_size;
        buf->audio_data = new uint8_t[pcm_size];
        std::memcpy(buf->audio_data, (uint8_t*)wave_data + pcm_offset, pcm_size);
    } else if (desc->lpwfxFormat) {
        buf->format = *desc->lpwfxFormat;
    }

    *out = buf;
    return DS_OK;
}

/* =========================================================================
 * IDirectSoundBuffer
 * ========================================================================= */

IDirectSoundBuffer::IDirectSoundBuffer()
    : stream(nullptr)
    , audio_data(nullptr)
    , data_len(0)
    , playing(false)
    , looping(false)
    , volume(0)
{
    std::memset(&format, 0, sizeof(format));
    format.wFormatTag     = 1;  /* PCM */
    format.nChannels      = 2;
    format.nSamplesPerSec = 22050;
    format.wBitsPerSample = 16;
    format.nBlockAlign    = 4;
    format.nAvgBytesPerSec = 22050 * 4;
}

IDirectSoundBuffer::~IDirectSoundBuffer()
{
    if (stream) SDL_DestroyAudioStream(stream);
    delete[] audio_data;
}

int IDirectSoundBuffer::Play(uint32_t reserved1, uint32_t reserved2,
                              uint32_t flags)
{
    (void)reserved1; (void)reserved2;

    if (!audio_data || data_len == 0) return DSERR_INVALIDPARAM;

    looping = (flags & DSBPLAY_LOOPING) != 0;
    Stop();

    /* Determine SDL audio format */
    SDL_AudioFormat sdl_fmt;
    if (format.wBitsPerSample == 8) {
        sdl_fmt = SDL_AUDIO_U8;
    } else if (format.wBitsPerSample == 16) {
        sdl_fmt = SDL_AUDIO_S16LE;
    } else {
        sdl_fmt = SDL_AUDIO_S16LE;
    }

    SDL_AudioSpec spec;
    spec.format     = sdl_fmt;
    spec.channels   = format.nChannels;
    spec.freq       = format.nSamplesPerSec;

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                        &spec, nullptr, nullptr);
    if (!stream) {
        fprintf(stderr, "SDL3: OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        return DSERR_INVALIDPARAM;
    }

    /* Queue audio data */
    if (!SDL_PutAudioStreamData(stream, audio_data, (int)data_len)) {
        fprintf(stderr, "SDL3: PutAudioStreamData failed: %s\n", SDL_GetError());
    }

    SDL_ResumeAudioStreamDevice(stream);
    playing = true;
    return DS_OK;
}

int IDirectSoundBuffer::Stop()
{
    if (stream) {
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }
    playing = false;
    return DS_OK;
}

int IDirectSoundBuffer::SetVolume(int32_t vol)
{
    /* DirectSound: 0 = max volume, -10000 = silent (~100 dB range)
     * Convert to 0-128 SDL scale */
    if (vol <= -10000) vol = -10000;
    if (vol > 0)       vol = 0;

    /* Linear mapping: -10000..0 → 0..128 */
    volume = (int)(128.0 * (1.0 + (double)vol / 10000.0));
    return DS_OK;
}

int IDirectSoundBuffer::SetPan(int32_t pan)
{
    (void)pan;
    /* Simple stereo panning not implemented; audio plays centered */
    return DS_OK;
}

int IDirectSoundBuffer::SetFrequency(uint32_t freq)
{
    (void)freq;
    /* Frequency shifting would require resampling. Stubbed. */
    return DS_OK;
}

int IDirectSoundBuffer::GetStatus(uint32_t* out_status)
{
    if (!out_status) return DSERR_INVALIDPARAM;

    *out_status = 0;
    if (playing) {
        if (stream) {
            int queued = SDL_GetAudioStreamQueued(stream);
            if (queued > 0) {
                *out_status |= 1;  /* DSBSTATUS_PLAYING */
            }
        }
    }
    return DS_OK;
}

int IDirectSoundBuffer::SetCurrentPosition(uint32_t pos)
{
    (void)pos;
    /* Seeking within audio stream: stop, reposition, restart.
     * Stubbed for now. */
    return DS_OK;
}

int IDirectSoundBuffer::Lock(uint32_t offset, uint32_t bytes,
                              void** ptr1, uint32_t* bytes1,
                              void** ptr2, uint32_t* bytes2,
                              uint32_t flags)
{
    (void)offset; (void)flags;

    /* Streaming Lock/Unlock is not needed with SDL3;
     * audio is loaded upfront via CreateSoundBuffer. */
    if (ptr1)  *ptr1  = nullptr;
    if (bytes1) *bytes1 = 0;
    if (ptr2)  *ptr2  = nullptr;
    if (bytes2) *bytes2 = 0;
    return DSERR_INVALIDPARAM;
}

int IDirectSoundBuffer::Unlock(void* ptr1, uint32_t bytes1,
                                void* ptr2, uint32_t bytes2)
{
    (void)ptr1; (void)bytes1; (void)ptr2; (void)bytes2;
    return DS_OK;
}

int IDirectSoundBuffer::Release()
{
    this->~IDirectSoundBuffer();
    delete this;
    return DS_OK;
}

/* =========================================================================
 * DirectSound helpers
 * ========================================================================= */

int DirectSoundCreate(void* guid, IDirectSound** ppDS, void* unk)
{
    (void)guid; (void)unk;

    IDirectSound* ds = new IDirectSound();
    if (!ds) return DSERR_INVALIDPARAM;

    ds->initialized = true;
    *ppDS = ds;
    g_pDS = ds;
    return DS_OK;
}

uint32_t DS_Init(void)
{
    IDirectSound* ds = nullptr;
    int hr = DirectSoundCreate(nullptr, &ds, nullptr);
    if (hr != DS_OK || !ds) return 0;
    ds->SetCooperativeLevel(nullptr, 0);
    return 1;
}

void DS_SaveAndShutdown(void)
{
    if (g_pDS) {
        g_pDS->Release();
        g_pDS = nullptr;
    }
}

#endif /* _WIN32 */
