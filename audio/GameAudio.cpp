/**
 * GameAudio.cpp — GameAudio implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "GameAudio.h"
#include "AudioChannel.h"
#include "../shared/types.h"
#include <cstring>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Resource manager */
#ifdef _WIN32
#define LOCO_AUDIO_FASTCALL __fastcall
#else
#ifndef _WIN32
/* Host-only ABI deviation: native GCC has no Win32 calling conventions. */
#define LOCO_AUDIO_FASTCALL
#endif
#endif
void* LOCO_AUDIO_FASTCALL RESMGR_GetById(void* resmgr, uint32_t id); /* 0x4472B0 */

/* Heap allocation */
void* operator_new(size_t size);                    /* 0x465CE0 */
void  GLOBAL_free(void* ptr);                       /* 0x465CD0 */

/* CRT memory ops */
void  CRT_memset_pattern(void*, int, int, void*);  /* 0x40EC30 pattern init */

/* Config */
int32_t Config_GetIniInt(void* ini, const char* section,
                         const char* key, int32_t def); /* 0x452D60 */

/* DirectSound ordinal imports */
int32_t Ordinal_1(int32_t, void*);         /* DirectSoundCreate / Enum */
int32_t Ordinal_2(void*);                  /* DirectSoundEnumerateA */

/* Global pointer to the singleton GameAudio instance */
extern GameAudio* g_audio;                              /* 0x4FD3BC */

/* Listener position globals (in screen/world coords) */
/* Global resource manager */
class ResourceManager;
extern ResourceManager g_resmgr;    /* 0x4855E8 — object, not a pointer (was void*,
                                      * a widespread cross-TU landmine — see
                                      * PROGRESS.md's g_resmgr sweep) */

extern int32_t g_listener_x;                            /* 0x4AAD2C */
extern int32_t g_listener_y;                            /* 0x4AAD30 */

/* Four-byte prefix retained by the original channel-pool allocation. */
struct ChannelAllocationHeader {
    int32_t count;
};

#undef LOCO_AUDIO_FASTCALL

/* ================================================================== */
/* GameAudio_Ctor                                                       */
/* Address: 0x412BD0                                                   */
/* ================================================================== */
GameAudio::GameAudio()
    : saved_bounds{},
      active_bounds{},
      pad_24{},
      max_channels(0),
      channels(nullptr),
      channel_usage(nullptr),
      listener_x(0),
      listener_y(0),
      ds_device(nullptr),
      ds_primary(nullptr),
      flags(0),
      muted(0)
{
    /* The member initializers reproduce the original zeroing of the
       +0x24 format buffer, saved/active bounds, and high-offset state. */
}

/* ================================================================== */
/* Destructor body (compiler supplies the deleting wrapper)              */
/* Address: 0x412C20 (vtable[0])                                       */
/* ================================================================== */
GameAudio::~GameAudio()
{
    this->Cleanup(nullptr);
}

/* ================================================================== */
/* Init                                                                 */
/* Address: 0x412C50                                                   */
/* ================================================================== */
uint32_t GameAudio::Init()
{
    int32_t select_best = 1;

    /* Cleanup any previous init state */
    this->Cleanup(nullptr);

    /* Check config for device enumeration preference */
    if (g_config_ini != nullptr) {
        select_best = Config_GetIniInt(g_config_ini, "Sound", "SelectBestDevice", 1);
    }

    /* DSound init */
    if (select_best == 0) {
        /* Use default device */
        int32_t result = Ordinal_1(0, &this->ds_device);
        if (result != 0) {
            return result & 0xFFFFFF00;
        }

        /* Zero the WAVEFORMATEX/device config at +0x24 for 0x60 bytes */
        std::memset(this->pad_24, 0, sizeof(this->pad_24));
        const int32_t format_size = 0x60;
        std::memcpy(this->pad_24, &format_size, sizeof(format_size));
    } else {
        /* Enumerate devices, pick best */
        Ordinal_2(reinterpret_cast<void*>(static_cast<uintptr_t>(0x412FB0)));
        int32_t result = Ordinal_1(0, &this->ds_device);
        if (result != 0) {
            return result & 0xFFFFFF00;
        }
    }

    /* Set cooperative level */
    AudioDirectSoundDevice* dev = this->ds_device;
    int32_t hr = dev->SetCooperativeLevel(nullptr, 2); /* DSSCL_NORMAL */

    if (hr == 0) {
        /* Create primary buffer */
        struct {
            int32_t size;
            int32_t format;
        } wfx;
        wfx.size   = 0x14;
        wfx.format = 1;

        hr = dev->CreateSoundBuffer(&wfx, &this->ds_primary, nullptr);

        if (hr == 0) {
            /* Set primary buffer format */
            this->ds_primary->SetFormat(&wfx);
        }

        /* Determine channel count */
        int32_t num_channels = 16;  /* default */

        if (num_channels < 1) {
            num_channels = 1;
        }

        this->max_channels = num_channels;

        /* Allocate channel array.  The four-byte count header is part of the
           original allocation and is retained for the matching free below. */
        int32_t alloc_size = num_channels * sizeof(AudioChannel)
                           + sizeof(ChannelAllocationHeader);
        ChannelAllocationHeader* channel_mem =
            static_cast<ChannelAllocationHeader*>(operator_new(alloc_size));
        if (channel_mem != nullptr) {
            channel_mem->count = num_channels;
            this->channels = reinterpret_cast<AudioChannel*>(channel_mem + 1);
        } else {
            this->channels = nullptr;
        }

        /* Allocate usage tracking array */
        uint32_t* usage = static_cast<uint32_t*>(
            operator_new(num_channels * sizeof(uint32_t)));
        this->channel_usage = usage;

        /* Set default volume bounds */
        this->active_bounds[0] = 100;
        this->active_bounds[1] = 100;
        this->active_bounds[2] = 100;
        this->active_bounds[3] = 100;

        this->saved_bounds[0] = 0x28;
        this->saved_bounds[1] = 0x14;
        this->saved_bounds[2] = 0x28;
        this->saved_bounds[3] = 0x14;

        /* Init each channel */
        for (int i = 0; i < this->max_channels; i++) {
            this->channel_usage[i] = 0;
            this->channels[i].Init();
            this->channels[i].SetBounds(
                this->active_bounds[3],
                this->active_bounds[1],
                this->active_bounds[2],
                this->active_bounds[0]
            );
        }

        this->flags        = 0;
        this->listener_x   = 0;
        this->listener_y   = 0;
    }

    return static_cast<uint32_t>(hr == 0);
}

/* ================================================================== */
/* Cleanup                                                              */
/* Address: 0x412EE0                                                   */
/* ================================================================== */
void GameAudio::Cleanup(void* hwnd)
{
    /* Release all channels and free the channel array */
    if (this->channels != nullptr) {
        for (uint32_t i = 0; i < this->max_channels; i++) {
            this->channels[i].Release();
        }

        ChannelAllocationHeader* base =
            reinterpret_cast<ChannelAllocationHeader*>(this->channels) - 1;
        GLOBAL_free(base);
        this->channels = nullptr;
    }

    /* Free usage tracking array */
    if (this->channel_usage != nullptr) {
        GLOBAL_free(this->channel_usage);
        this->channel_usage = nullptr;
    }

    /* Release primary buffer */
    if (this->ds_primary != nullptr) {
        this->ds_primary->Release();
        this->ds_primary = nullptr;
    }

    /* Release DS device */
    if (this->ds_device != nullptr) {
        this->ds_device->SetCooperativeLevel(hwnd, 1);
        this->ds_device->Release();
        this->ds_device = nullptr;
    }
}

/* ================================================================== */
/* Play                                                                 */
/* Address: 0x413070                                                   */
/* ================================================================== */
int32_t GameAudio::Play(void* param1, void* param2, void* param3)
{
    if (this->ds_device != nullptr) {
        return this->ds_device->CreateSoundBuffer(param1, param2, param3);
    }
    return -1;
}

/* ================================================================== */
/* SetListenerPos                                                       */
/* Address: 0x4130A0                                                   */
/* ================================================================== */
void GameAudio::SetListenerPos(int32_t x, int32_t y)
{
    this->listener_x = x;
    this->listener_y = y;

    if (this->channels != nullptr) {
        for (uint32_t i = 0; i < this->max_channels; i++) {
            this->channels[i].SetPosition(x, y);
        }
    }
}

/* ================================================================== */
/* StopFinished                                                         */
/* Address: 0x4130F0                                                   */
/* ================================================================== */
void GameAudio::StopFinished()
{
    if (this->channels != nullptr) {
        for (uint32_t i = 0; i < this->max_channels; i++) {
            if (this->channels[i].IsActive()) {
                this->channels[i].Release();
            }
        }
    }
}

/* ================================================================== */
/* StopAll                                                              */
/* Address: 0x413140                                                   */
/* ================================================================== */
void GameAudio::StopAll()
{
    if (this->channels != nullptr) {
        for (uint32_t i = 0; i < this->max_channels; i++) {
            this->channels[i].Release();
        }
    }
}

/* ================================================================== */
/* PlayResource                                                         */
/* Address: 0x413180                                                   */
/* ================================================================== */
void GameAudio::PlayResource(uint32_t resource_id)
{
    SoundResource* res = static_cast<SoundResource*>(
        RESMGR_GetById(&g_resmgr, resource_id));
    this->AllocChannel(res, nullptr,
                       g_listener_x, g_listener_y,
                       4, 0);
}

/* ================================================================== */
/* PlayResourceEx                                                       */
/* Address: 0x4131C0                                                   */
/* ================================================================== */
void GameAudio::PlayResourceEx(uint32_t resource_id, AudioChannel** output_ptr)
{
    if (output_ptr != nullptr && *output_ptr != nullptr) {
        (*output_ptr)->Release();
    }

    SoundResource* res = static_cast<SoundResource*>(
        RESMGR_GetById(&g_resmgr, resource_id));
    this->AllocChannel(res, output_ptr,
                       g_listener_x, g_listener_y,
                       4, 0);
}

/* ================================================================== */
/* AllocChannel                                                         */
/* Address: 0x413210                                                   */
/* ================================================================== */
void GameAudio::AllocChannel(SoundResource* resource, AudioChannel** output_ptr,
                             int32_t pos_x, int32_t pos_y,
                             uint32_t attenuation, uint8_t looping)
{
    int32_t found_idx = -1;
    uint32_t free_count = 0;
    int32_t res_id = 0;

    if (this->channels == nullptr || resource == nullptr) {
        goto no_channel;
    }

    res_id = resource->resource_id;

    /* --- Pass 1: count free (inactive) same-resource channels --- */
    for (uint32_t i = 0; i < this->max_channels; i++) {
        if (this->channels[i].resource_id == res_id) {
            if (!this->channels[i].IsActive()) {
                free_count++;
            } else {
                this->channels[i].state = CHANNEL_STATE_STOPPING;
            }
        }
    }

    /* --- Check instance limits and cooldown --- */
    if (free_count < static_cast<uint32_t>(resource->max_instances)) {
        goto no_channel;
    }
    if (resource->cooldown_timer > g_game_time) {
        goto no_channel;
    }

    /* --- Try to reuse a same-resource channel --- */
    if (res_id >= 0) {
        for (uint32_t i = 0; i < this->max_channels && found_idx == -1; i++) {
            AudioChannel* ch = &this->channels[i];
            if (ch->resource_id == res_id && ch->IsActive()) {
                found_idx = static_cast<int32_t>(i);
            }
        }

        if (found_idx != -1) {
            AudioChannel* ch = &this->channels[found_idx];
            ch->Reset();
            ch->UpdatePosition(pos_x, pos_y);
            ch->SetAttenuation(static_cast<int32_t>(attenuation));
            ch->looping = looping;
            ch->Play();
            ch->SetOutput(output_ptr);
            return;
        }
    }

    /* --- Find any non-IDLE (state != 1) channel --- */
    for (uint32_t i = 0; i < this->max_channels && found_idx == -1; i++) {
        if (!this->channels[i].IsPlaying()) {
            found_idx = static_cast<int32_t>(i);
        }
    }

    if (found_idx != -1) {
        AudioChannel* ch = &this->channels[found_idx];
        ch->state = CHANNEL_STATE_STOPPING;
        ch->Reset();
        ch->UpdatePosition(pos_x, pos_y);
        ch->SetAttenuation(static_cast<int32_t>(attenuation));
        ch->looping = looping;
        ch->Play();
        ch->SetOutput(output_ptr);
        return;
    }

    /* --- Find any active channel to steal --- */
    for (uint32_t i = 0; i < this->max_channels && found_idx == -1; i++) {
        if (this->channels[i].IsActive()) {
            this->channels[i].state = CHANNEL_STATE_STOPPING;
            found_idx = static_cast<int32_t>(i);
        }
    }

    /* --- Find lowest-priority channel --- */
    if (found_idx == -1) {
        for (uint32_t i = 0; i < this->max_channels; i++) {
            AudioChannel* ch = &this->channels[i];
            if (static_cast<uint32_t>(ch->attenuation_type) <= attenuation) {
                if (found_idx == -1 ||
                    ch->attenuation_type < this->channels[found_idx].attenuation_type ||
                    this->channel_usage[i] < this->channel_usage[found_idx]) {
                    found_idx = static_cast<int32_t>(i);
                }
            }
        }
    }

    if (found_idx != -1) {
        /* --- Steal the found channel: release, load, play --- */
        AudioChannel* ch = &this->channels[found_idx];
        ch->Release();

        ch->LoadSound(this->ds_device, resource,
                      pos_x, pos_y,
                      static_cast<int32_t>(attenuation), looping);

        if (ch->state != CHANNEL_STATE_LOADED) {
            ch->Release();
            return;
        }

        ch->SetOutput(output_ptr);

        /* Set cooldown on the sound resource */
        if (resource->cooldown_interval > 0) {
            resource->cooldown_timer = g_game_time + resource->cooldown_interval;
        }
        return;
    }

no_channel:
    if (output_ptr != nullptr) {
        *output_ptr = nullptr;
    }
}

/* ================================================================== */
/* SetMute                                                              */
/* Address: 0x413530                                                   */
/* ================================================================== */
void GameAudio::SetMute(uint8_t mute)
{
    this->muted = mute;

    if (mute == 0) {
        this->active_bounds[1] = this->saved_bounds[0];
        this->active_bounds[2] = this->saved_bounds[1];
        this->active_bounds[3] = this->saved_bounds[2];
        this->active_bounds[0] = this->saved_bounds[3];
    } else {
        this->active_bounds[0] = 0;
        this->active_bounds[1] = 0;
        this->active_bounds[2] = 0;
        this->active_bounds[3] = 0;
    }

    if (this->channels != nullptr) {
        for (uint32_t i = 0; i < this->max_channels; i++) {
            this->channels[i].SetBounds(
                this->active_bounds[3],
                this->active_bounds[1],
                this->active_bounds[2],
                this->active_bounds[0]
            );
        }
    }
}

/* ================================================================== */
/* UpdateVolume                                                         */
/* Address: 0x4135B0                                                   */
/* ================================================================== */
void GameAudio::UpdateVolume(uint8_t silence)
{
    if (silence == 0) {
        if (this->muted == 0) {
            this->active_bounds[1] = this->saved_bounds[0];
            this->active_bounds[2] = this->saved_bounds[1];
            this->active_bounds[3] = this->saved_bounds[2];
        }
    } else {
        this->active_bounds[1] = 0;
        this->active_bounds[2] = 0;
        this->active_bounds[3] = 0;
    }

    if (this->channels != nullptr) {
        for (uint32_t i = 0; i < this->max_channels; i++) {
            this->channels[i].SetBounds(
                this->active_bounds[3],
                this->active_bounds[1],
                this->active_bounds[2],
                this->active_bounds[0]
            );
        }
    }
}

/* ================================================================== */
/* SetBounds                                                            */
/* Address: 0x413630                                                   */
/* ================================================================== */
void GameAudio::SetBounds(int32_t volume_low, int32_t volume_medium,
                          int32_t volume_high, int32_t volume_max)
{
    this->saved_bounds[3] = volume_max;
    this->saved_bounds[1] = volume_medium;
    this->saved_bounds[0] = volume_high;
    this->saved_bounds[2] = volume_low;

    this->SetMute(this->muted);
}
