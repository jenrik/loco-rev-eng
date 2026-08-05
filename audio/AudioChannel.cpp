/**
 * AudioChannel.cpp — AudioChannel implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * All 11 methods reverse-engineered from disassembly (functions span
 * 0x40EC70 – 0x40F1A1). The pan and volume computations use x87 log
 * math (FYL2X, F2XM1, FSCALE) ported to standard C++ <cmath>.
 */

// Status: TRANSCRIBED

#define _USE_MATH_DEFINES

#include "AudioChannel.h"
#include "GameAudio.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include <cmath>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

#ifdef _WIN32
#define LOCO_AUDIO_STDCALL __stdcall
#define LOCO_AUDIO_FASTCALL __fastcall
#else
#ifndef _WIN32
/* Host-only ABI deviation: native GCC has no Win32 calling conventions. */
#define LOCO_AUDIO_STDCALL
#define LOCO_AUDIO_FASTCALL
#endif
#endif

extern "C" {
    /* Win32 / CRT */
    void LOCO_AUDIO_STDCALL OutputDebugStringA(const char* lpOutputString);
}

/* Resource manager */
void LOCO_AUDIO_FASTCALL RESMGR_LoadSoundResource(void* sound_res); /* 0x448D60 */

/* Error string helper */
void LOCO_AUDIO_STDCALL DDRAW_GetDsoundErrorString(int32_t hr);     /* 0x45C2E0 */

/* Global audio instance (used only by LoadSound) */
extern GameAudio* g_audio;                                         /* 0x4FD3BC */

#undef LOCO_AUDIO_STDCALL
#undef LOCO_AUDIO_FASTCALL

/* ================================================================== */
/* Debug string constant                                               */
/* ================================================================== */
static const char kAssertNeverHere[] = "Should never get here!";

/* ================================================================== */
/* Internal double constants (from .rdata at 0x00477628 – 0x00477678)  */
/*                                                                     */
/* These are the double-precision constants embedded in the PE .rdata  */
/* section. The listed values are educated estimates; the exact bytes  */
/* should be verified against the binary at each address.              */
/* ================================================================== */

/* --- UpdatePosition constants --- */

/* 0x00477628 — 0.0: zero sentinel / default pan value                */
static const double kDblZero     = 0.0;

/* 0x00477630 — 1.0: used in denominator 1/(1-|ratio|) and FSUB      */
static const double kDblOne      = 1.0;

/* 0x00477638 — 0.0: fallback pan when ABS(ratio) is unordered/NaN   */
static const double kDblNaNPan   = 0.0;

/* 0x00477640 — 2.0: base of the log in pan curve (log_BASE)          */
static const double kPanLogBase  = 2.0;

/* 0x00477648 — ~1000.0: linear multiplier after log computation      */
static const double kPanLogMult  = 1000.0;

/* 0x00477650 — ~10000.0: overall pan scale (also used as attenuation */
/*               upper threshold in ApplyAttenuation).                 */
static const double kPanScale    = 10000.0;

/* --- ApplyAttenuation constants --- */

/* 0x00477658 — scale factor for (multiplier * level) product.
   Converts the 32-bit product to a value comparable to kPanScale.    */
static const double kAttenScale  = 0.01;

/* 0x00477660 — volume sentinel when product <= 0 (max silence).
   DSBVOLUME_MIN = -10000 (hundredths of dB).                         */
static const double kAttenSilence = -10000.0;

/* 0x00477668 — inner multiplier M1 in exp formula: fVar2 *= M1      */
static const double kAttenM1     = 1.0;

/* 0x00477670 — subtrahend S in exp formula: S - fVar2 * M1          */
static const double kAttenS      = 128.0;

/* 0x00477678 — outer multiplier M2 in exp formula: (S - ...) * M2   */
static const double kAttenM2     = 0.09;

/* ================================================================== */
/* Init                                                                 */
/* Address: 0x40EC70                                                   */
/*                                                                     */
/* Called by: GameAudio::Init (0x412C50) once per channel              */
/* ================================================================== */
void AudioChannel::Init()
{
    this->attenuation_type  = 2;    /* +0x08 */
    this->output_ptr        = nullptr; /* +0x00 */
    this->looping           = 0;       /* +0x04 */
    this->attenuation_level = 100;  /* +0x0c */
    this->state             = CHANNEL_STATE_IDLE; /* +0x10 */
    this->ds_buffer         = nullptr; /* +0x14 */
    this->pos_x             = 0;    /* +0x18 */
    this->pos_y             = 0;    /* +0x1c */
    this->bounds_max_x      = 0;    /* +0x20 */
    this->bounds_max_y      = 0;    /* +0x24 */
    this->resource_id       = 0;    /* +0x38 */
}

/* ================================================================== */
/* Release                                                              */
/* Address: 0x40ECA0                                                   */
/*                                                                     */
/* Called by: GameAudio::Cleanup, GameAudio::AllocChannel,             */
/*            GameObject_DtorBody, CGWND_SetMode, etc.                 */
/* ================================================================== */
void AudioChannel::Release()
{
    /* Release the DS secondary buffer if it exists */
    if (this->ds_buffer != nullptr) {                   /* +0x14 */
        /* DirectSoundBuffer slots 18 and 2: Stop, then IUnknown::Release. */
        this->ds_buffer->Stop();
        this->ds_buffer->Release();
        this->ds_buffer = nullptr;                       /* +0x14 */
    }

    /* Clear the output pointer back-link */
    if (this->output_ptr != nullptr) {                  /* +0x00 */
        *this->output_ptr = nullptr;
        this->output_ptr = nullptr;                      /* +0x00 */
    }

    /* Reset state back to idle */
    this->state        = CHANNEL_STATE_IDLE;            /* +0x10 */
    this->resource_id  = 0;                             /* +0x38 */
}

/* ================================================================== */
/* Reset                                                                */
/* Address: 0x40ECF0                                                   */
/*                                                                     */
/* Called by: GameAudio::AllocChannel before reconfiguring a channel   */
/* ================================================================== */
void AudioChannel::Reset()
{
    if (this->output_ptr != nullptr) {                  /* +0x00 */
        *this->output_ptr = nullptr;
        this->output_ptr = nullptr;                      /* +0x00 */
    }
}

/* ================================================================== */
/* SetOutput                                                            */
/* Address: 0x40ED10                                                   */
/*                                                                     */
/* Called by: GameAudio::AllocChannel after loading/playing             */
/* ================================================================== */
void AudioChannel::SetOutput(AudioChannel** ptr)
{
    this->output_ptr = ptr;                             /* +0x00 */
    if (ptr != nullptr) {
        *ptr = this;    /* back-link: caller can find this channel */
    }
}

/* ================================================================== */
/* LoadSound                                                            */
/* Address: 0x40ED20                                                   */
/*                                                                     */
/* Called by: GameAudio::AllocChannel to set up a new sound             */
/*                                                                     */
/* Parameters (RET 0x18 = 6 stack params, __thiscall with this=ECX):   */
/*   ds_device      — IDirectSound* to create buffers from              */
/*   sound_resource — SoundResource* containing sound data reference    */
/*   pos_x, pos_y   — initial world position for pan computation        */
/*   atten_type     — attenuation curve type to apply (1-4)             */
/*   looping        — 0=oneshot, 1=looping                             */
/* ================================================================== */
void AudioChannel::LoadSound(AudioDirectSoundDevice* ds_device,
                             SoundResource* sound_resource,
                             int32_t pos_x, int32_t pos_y,
                             int32_t atten_type, uint8_t looping)
{
    /* Debug assert: should not have an existing buffer */
    if (this->ds_buffer != nullptr) {                   /* +0x14 */
        OutputDebugStringA(kAssertNeverHere);
    }

    /* Ensure the sound resource has its data loaded */
    if (sound_resource->buffer == nullptr) {
        RESMGR_LoadSoundResource(sound_resource);        /* 0x448D60 */
    }

    /* Attempt the recovered secondary-buffer call (device slot 5, +0x14). */
    int32_t hr = ds_device->DuplicateSoundBuffer(
        sound_resource->buffer,                          /* wave data */
        &this->ds_buffer);                               /* +0x14 */

    /* If DSERR_BUFFERLOST, stop all channels and retry */
    if (hr == 0x8878000a /* DSERR_BUFFERLOST */) {
        g_audio->StopFinished();                         /* 0x4130F0 */

        hr = ds_device->DuplicateSoundBuffer(
            sound_resource->buffer,
            &this->ds_buffer);
    }

    /* On any error, print debug string and return without loading */
    if (hr < 0) {
        DDRAW_GetDsoundErrorString(hr);                 /* 0x45C2E0 */
        return;
    }

    /* Store attenuation curve type and apply initial level (100 from Init) */
    this->attenuation_type = atten_type;                /* +0x08 */
    this->ApplyAttenuation(this->attenuation_level);    /* +0x0c */

    /* Set initial position (computes and applies DS pan) */
    this->UpdatePosition(pos_x, pos_y);                 /* +0x18, +0x1c */

    /* Record looping flag and resource ID */
    this->looping      = looping;                       /* +0x04 */
    this->resource_id  = sound_resource->resource_id;   /* +0x38 */

    /* Start playback — position 0, either oneshot or looping */
    if (looping == 0) {
        this->ds_buffer->Play(0, 0, 0);
    } else {
        this->ds_buffer->Play(0, 0, 1);                  /* DSBPLAY_LOOPING */
    }

    this->state = CHANNEL_STATE_LOADED;                 /* +0x10 */
}

/* ================================================================== */
/* Pause                                                                */
/* Address: 0x40EE00                                                   */
/*                                                                     */
/* Called by: CGWND_SetPause, Vehicle_UpdatePosition                   */
/* ================================================================== */
void AudioChannel::Pause()
{
    if (this->ds_buffer != nullptr) {                   /* +0x14 */
        this->ds_buffer->Stop();                         /* Stop */
    }
    this->state = CHANNEL_STATE_PAUSED;                  /* +0x10 */
}

/* ================================================================== */
/* Play                                                                 */
/* Address: 0x40EE20                                                   */
/*                                                                     */
/* Called by: GameAudio_AllocChannel, GameObject_PlayAnimation, etc.   */
/*                                                                     */
/* Note: Sets state to LOADED (not PLAYING) after starting DS play.    */
/* The actual playback status is tracked by the DS buffer via GetStatus */
/* ================================================================== */
void AudioChannel::Play()
{
    if (this->ds_buffer == nullptr) {                   /* +0x14 */
        return;
    }

    /* If state == LOADED, check if the DS buffer is already playing */
    if (this->state == CHANNEL_STATE_LOADED) {          /* +0x10 */
        uint32_t status = 0;
        this->ds_buffer->GetStatus(&status);

        /* DSBSTATUS_PLAYING is bit 0 — if set, buffer is already playing */
        if ((status & 1) != 0) {
            goto done;
        }
    }

    /* Seek to start if not paused (paused should resume from current pos) */
    if (this->state != CHANNEL_STATE_PAUSED) {          /* +0x10 */
        this->ds_buffer->SetCurrentPosition(0);         /* SetCurrentPosition(0) */
    }

    /* Start playback with or without looping */
    if (this->looping == 0) {                           /* +0x04 */
        this->ds_buffer->Play(0, 0, 0);
    } else {
        this->ds_buffer->Play(0, 0, 1);                 /* DSBPLAY_LOOPING */
    }

done:
    this->state = CHANNEL_STATE_LOADED;                 /* +0x10 */
}

/* ================================================================== */
/* IsPlaying                                                            */
/* Address: 0x40EEA0                                                   */
/*                                                                     */
/* Returns true when state == IDLE (channel is free/unloaded).         */
/* Despite the name, this identifies channels that are available for   */
/* immediate reuse without needing a buffer release.                   */
/* ================================================================== */
bool AudioChannel::IsPlaying()
{
    return (this->state == CHANNEL_STATE_IDLE);         /* +0x10 */
}

/* ================================================================== */
/* IsActive                                                             */
/* Address: 0x40EEB0                                                   */
/*                                                                     */
/* Returns true if the channel is "in use" and should not be stolen.   */
/* State IDLE or STOPPING: always active.                              */
/* State LOADED: active if ds_buffer exists (checked via GetStatus).   */
/* State PAUSED: not active.                                           */
/* ================================================================== */
bool AudioChannel::IsActive()
{
    switch (this->state) {                              /* +0x10 */
    case CHANNEL_STATE_IDLE:        /* 1 */
    case CHANNEL_STATE_STOPPING:    /* 4 */
        return true;

    case CHANNEL_STATE_LOADED:      /* 2 */
        if (this->ds_buffer == nullptr) {               /* +0x14 */
            return false;
        }
        /* Call GetStatus to verify buffer is still valid */
        {
            uint32_t status;
            this->ds_buffer->GetStatus(&status);
        }
        return true;

    case CHANNEL_STATE_PAUSED:      /* 3 */
    default:
        return false;
    }
}

/* ================================================================== */
/* SetPosition                                                          */
/* Address: 0x40EF00                                                   */
/*                                                                     */
/* Called by: GameAudio::SetListenerPos (for all channels on update)   */
/* ================================================================== */
void AudioChannel::SetPosition(int32_t max_x, int32_t max_y)
{
    this->bounds_max_x = max_x;                         /* +0x20 */
    this->bounds_max_y = max_y;                         /* +0x24 */
    this->UpdatePosition(this->pos_x, this->pos_y);     /* +0x18, +0x1c */
}

/* ================================================================== */
/* UpdatePosition                                                       */
/* Address: 0x40EF20 (96 bytes)                                         */
/*                                                                     */
/* Clamps (pos_x, pos_y) to [0, bounds_max) and computes a log-based   */
/* pan value for IDirectSoundBuffer::SetPan(vtbl[0x40]).               */
/*                                                                     */
/* The original asm uses the x87 coprocessor:                           */
/*   1. Clamp X → [0, bounds_max_x-1], clamp Y → [0, bounds_max_y-1] */
/*   2. If either bound is zero → pan = 0 (no spatial audio)           */
/*   3. center_x = bounds_max_x >> 1                                   */
/*   4. ratio = (pos_x - center_x) / center_x           (FILD/FSUB/FDIV) */
/*   5. If ratio == 0.0 → pan = 0                                      */
/*   6. abs_r = |ratio|                                 (FABS)          */
/*   7. Logarithmic pan curve:                                          */
/*        t = 1.0 / (1.0 - abs_r)                     (FDIVR)          */
/*        pan_linear = kPanLogMult * log(t) / log(kPanLogBase)         */
/*        pan = kPanScale * pan_linear                   (FYL2X chain) */
/*   8. If ratio < 0 → pan = -pan                     (FCHS)           */
/*   9. ftol(pan) → ds_buffer->SetPan(vtbl[0x40])                     */
/*                                                                     */
/* Called by: LoadSound, SetPosition                                    */
/* ================================================================== */
void AudioChannel::UpdatePosition(int32_t pos_x, int32_t pos_y)
{
    /* ---------- Clamp X ---------- */
    if (pos_x < 0) {
        pos_x = 0;
    }
    if (this->bounds_max_x != 0 && pos_x >= this->bounds_max_x) { /* +0x20 */
        pos_x = this->bounds_max_x - 1;
    }

    /* ---------- Clamp Y ---------- */
    if (pos_y < 0) {
        pos_y = 0;
    }
    if (this->bounds_max_y != 0 && pos_y >= this->bounds_max_y) { /* +0x24 */
        pos_y = this->bounds_max_y - 1;
    }

    /* Store clamped position */
    this->pos_x = pos_x;                                /* +0x18 */
    this->pos_y = pos_y;                                /* +0x1c */

    /* ---------- Compute DS pan ---------- */
    int32_t pan = 0;

    if (this->bounds_max_x != 0 && this->bounds_max_y != 0) {
        int32_t center_x = this->bounds_max_x >> 1;    /* +0x20 / 2 */

        /* Ratio in [-1, 1]: horizontal position relative to center */
        double ratio = static_cast<double>(pos_x - center_x) /
                       static_cast<double>(center_x);

        if (ratio != kDblZero) {
            double abs_r = fabs(ratio);

            /* Logarithmic pan curve (matches x87 FYL2X chain):
               t   = 1.0 / (1.0 - abs_r)
               pan = kPanScale * kPanLogMult * log(t) / log(kPanLogBase) */
            double t = kDblOne / (kDblOne - abs_r);
            double log_val = log(t) / log(kPanLogBase);

            double raw_pan = kPanScale * kPanLogMult * log_val;

            /* Apply sign: negative ratio = left pan */
            if (ratio < 0.0) {
                raw_pan = -raw_pan;
            }

            pan = static_cast<int32_t>(raw_pan);
        }
        /* else ratio == 0.0 → pan stays 0 (center) */
    }

    /* Apply pan to DS buffer */
    if (this->ds_buffer != nullptr) {                   /* +0x14 */
        this->ds_buffer->SetPan(pan);
    }
}

/* ================================================================== */
/* SetBounds                                                            */
/* Address: 0x40F040                                                   */
/*                                                                     */
/* Stores four volume curve multipliers and re-applies attenuation.    */
/* Called by: GameAudio::SetBounds, GameAudio::SetMute/UpdateVolume    */
/*                                                                     */
/* NOTE: The original asm stores params in a different order than the  */
/* C parameter order. The hardware write order is:                     */
/*   MOV [this+0x28], param_1  (a)   ← first                          */
/*   MOV [this+0x30], param_3  (c)   ← third, but written second      */
/*   MOV [this+0x2c], param_2  (b)   ← second, but written third      */
/*   MOV [this+0x34], param_4  (d)   ← last                           */
/* ================================================================== */
void AudioChannel::SetBounds(int32_t a, int32_t b, int32_t c, int32_t d)
{
    /* Written in ASM order: a, c, b, d — NOT a, b, c, d */
    this->attenuations[0] = a;                          /* +0x28 */
    this->attenuations[2] = c;                          /* +0x30 */
    this->attenuations[1] = b;                          /* +0x2c */
    this->attenuations[3] = d;                          /* +0x34 */

    this->ApplyAttenuation(this->attenuation_level);    /* +0x0c */
}

/* ================================================================== */
/* SetAttenuation                                                       */
/* Address: 0x40F070                                                   */
/*                                                                     */
/* Changes the curve type and recalculates volume.                      */
/* Called by: GameAudio::AllocChannel before Play                      */
/* ================================================================== */
void AudioChannel::SetAttenuation(int32_t type)
{
    this->attenuation_type = type;                      /* +0x08 */
    this->ApplyAttenuation(this->attenuation_level);    /* +0x0c */
}

/* ================================================================== */
/* ApplyAttenuation                                                     */
/* Address: 0x40F090 (81 bytes)                                         */
/*                                                                     */
/* Selects a volume multiplier by attenuation_type (1-4), then          */
/* computes the DS volume via an exponential formula.                   */
/*                                                                     */
/* The original x87 implementation in detail:                           */
/*                                                                     */
/*   1. Select multiplier from attenuations[type-1]                    */
/*   2. product = (int32_t)(multiplier * level)        (IMUL)           */
/*   3. fVar2   = (double)(uint32_t)product * kAttenScale (FILD, FMUL) */
/*                                                                     */
/*   4. If fVar2 <= 0.0:                                               */
/*        → volume = kAttenSilence (-10000 = DSBVOLUME_MIN)            */
/*      Else if fVar2 >= kPanScale (10000):                            */
/*        → volume = 0 (DSBVOLUME_MAX = full volume)                   */
/*      Else:                                                          */
/*        tmp = log2(e) * kAttenM2 * (kAttenS - fVar2 * kAttenM1)     */
/*             = log2(e) * M2 * (S - fVar2 * M1)                       */
/*        volume = 1 - 2^tmp                                           */
/*               = 1 - exp(M2 * (S - fVar2 * M1))                     */
/*                                                                     */
/*   5. (int32_t)volume → ds_buffer->SetVolume(vtbl[0x3c])             */
/*                                                                     */
/* The x87 sequence (notable instructions):                             */
/*    IMUL ECX, EAX           — multiplier * level                     */
/*    MOV [ESP+0x08], 0      — zero-extend to 64-bit                  */
/*    FILD qword [ESP+0x04]  — load as signed 64-bit int              */
/*    FMUL [kAttenScale]     — * SCALE                                  */
/*    FCOM [kDblZero]        — check <= 0                              */
/*    FCOM [kPanScale]       — check >= threshold                      */
/*    FMUL [kAttenM1]        — inner multiplier                        */
/*    FSUBR [kAttenS]        — S - result                              */
/*    FMUL [kAttenM2]        — outer multiplier                        */
/*    FLDL2E                  — log2(e)                                 */
/*    FMULP                   — multiply                                */
/*    FRNDINT + FXCH + FSUB  — split into integer/fractional parts     */
/*    F2XM1 + FLD1 + FADDP   — 2^fraction                              */
/*    FSCALE                 — * 2^integer = 2^tmp                     */
/*    FSUB [kDblOne]         — 2^tmp - 1                               */
/*    FCHS                   — -(2^tmp - 1) = 1 - 2^tmp               */
/*    __ftol                                                           */
/*                                                                     */
/* Called by: LoadSound, SetBounds, SetAttenuation                     */
/* ================================================================== */
void AudioChannel::ApplyAttenuation(int32_t level)
{
    this->attenuation_level = level;                    /* +0x0c */

    /* ---------- Select multiplier by curve type ---------- */
    int32_t multiplier;
    switch (this->attenuation_type) {                   /* +0x08 */
    case 1:  multiplier = this->attenuations[0]; break; /* +0x28 */
    case 2:  multiplier = this->attenuations[1]; break; /* +0x2c */
    case 3:  multiplier = this->attenuations[2]; break; /* +0x30 */
    case 4:  multiplier = this->attenuations[3]; break; /* +0x34 */
    default: multiplier = this->attenuations[2]; break; /* +0x30 (fallback = type 3) */
    }

    /* ---------- Compute fVar2 = (double)(uint32_t)(multiplier * level) * scale ---------- */
    /* Original: IMUL truncates to 32 bits, zero-extended via MOV [ESP+0x08]=0, then FILD */
    int32_t product_32 = multiplier * level;
    double fVar2 = static_cast<double>(static_cast<uint32_t>(product_32))
                 * kAttenScale;

    /* ---------- Volume computation ---------- */
    double volume;

    if (fVar2 <= kDblZero) {
        /* product * scale <= 0 → silence */
        volume = kAttenSilence;                         /* -10000 */
    } else if (fVar2 >= kPanScale) {
        /* Above threshold → full volume (no attenuation) */
        volume = kDblZero;                              /* 0 */
    } else {
        /* Exponential attenuation curve:
           1 - 2^(log2(e) * M2 * (S - fVar2 * M1))
           = 1 - exp(M2 * (S - fVar2 * M1)) */
        double tmp = M_LOG2E
                   * kAttenM2
                   * (kAttenS - fVar2 * kAttenM1);

        /* pow(2.0, tmp) = 2^(log2(e) * M2 * (S - fVar2 * M1)) */
        volume = kDblOne - pow(2.0, tmp);
    }

    /* ---------- Apply to DS buffer ---------- */
    if (this->ds_buffer != nullptr) {                   /* +0x14 */
        this->ds_buffer->SetVolume(static_cast<int32_t>(volume));
    }
}
