/**
 * ddraw_audio_init.c — Game audio subsystem initialisation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C free function, __cdecl. Creates the global GameAudio instance,
 * initialises DirectSound, configures listener position, and loads
 * volume levels from LOCO.INI [Sound] section.
 */

#include <stdint.h>

#include "../audio/GameAudio.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* GameAudio::Init (0x412C50) is a real, in-tree method
 * (audio/GameAudio.h/.cpp), but its currently-transcribed signature
 * (zero args) does not match the real one: disassembly of 0x412C50
 * shows `RET 0x8` (two real __thiscall stack params — num_channels,
 * hwnd), and the body genuinely uses both (hwnd flows into
 * SetCooperativeLevel; num_channels flows into max_channels, gated by
 * an untranscribed field pair inside GameAudio::pad_24 that the
 * current header treats as opaque). Fixing that is a GameAudio.cpp
 * validation task, not an old-style-cast cleanup, so this call is left
 * as the pre-existing free-function extern (still call-0/unresolved —
 * see commit message) rather than migrated to a typed call that would
 * silently drop both real parameters. */
extern uint32_t __cdecl GameAudio_Init(void* audio, int channels, void* hwnd); /* 0x412C50 */

/* Config reader */
extern int32_t __cdecl Config_GetIniInt(void* config_ini,    /* 0x452D60 */
                                         const char* section,
                                         const char* key,
                                         int32_t default_val);

/* Globals */
extern GameAudio* g_audio;          /* 0x4FD3BC — global GameAudio* */
extern void* g_config_ini;          /* 0x4A9EEC */
extern void* g_main_window;         /* 0x4AA4A0 */
extern int32_t g_world_width;       /* 0x4AAD10 */
extern int32_t g_world_height;      /* 0x4AAD0C */

/* Forward declaration (STRICT=2 -Wmissing-declarations); matches
 * graphics/DDRAW.h's existing declaration exactly, but this file stays
 * self-contained rather than including that header (which also
 * declares several *other* DDRAW_* functions with signatures that do
 * not match their native/*.c definitions — see commit message). */
uint32_t __cdecl DDRAW_InitAudio(void);

/* ================================================================== */
/* DDRAW_InitAudio — Initialise the game audio system                  */
/* Address: 0x45B7E0                                                   */
/* Size: 346 bytes (104 insn)                                          */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* SEH-protected audio subsystem init:                                 */
/*   1. Check if g_audio already exists — return 0 if so               */
/*   2. Allocate 0xB8 bytes for GameAudio, call GameAudio_Ctor         */
/*   3. If allocation fails, return 0                                  */
/*   4. Call GameAudio_Init with HWND for DirectSound initialisation   */
/*   5. If Init fails, destroy GameAudio and return 0                  */
/*   6. Call GameAudio_SetListenerPos with world dimensions            */
/*   7. Read volume levels from LOCO.INI [Sound] section:              */
/*      - VolumeLow  (default: 0x4B = 75)                              */
/*      - VolumeMed  (default: 0x4B = 75)                              */
/*      - VolumeHigh (default: 0x4E = 78)                              */
/*   8. Call GameAudio_SetBounds with volume parameters                */
/*   9. Return 1 on success                                            */
/*                                                                     */
/* Called by: CGWND_InitAllSubsystems (0x407058)                       */
/*            DirectPlay_Init (?), startup path                         */
/*                                                                     */
/* @return  1 on success, 0 on failure or already initialised          */
/* ================================================================== */
uint32_t __cdecl DDRAW_InitAudio(void)
{
    uint32_t result;

    /* Already initialised? */
    if (g_audio != NULL) {
        return 0;
    }

    /* Allocate + construct GameAudio. GameAudio's real ctor (0x412BD0)
     * only zero-initialises fields (no failure path), matching plain
     * `new`; the original's separate operator_new(0xB8)+Ctor(mem) step
     * is one call here, same as the established host idiom in
     * core/HostMode3Bootstrap.cpp's own `g_audio = new GameAudio;`. */
    g_audio = new GameAudio();

    /* Initialise DirectSound */
    {
        void* hWnd = *reinterpret_cast<void**>(static_cast<uint8_t*>(g_main_window) + 8);
        result = GameAudio_Init(g_audio, 16, hWnd);
    }

    if (result == 0) {
        delete g_audio;
        g_audio = nullptr;
        return 0;
    }

    /* Set listener position at world centre */
    g_audio->SetListenerPos(g_world_width, g_world_height);

    /* Read volume levels from config */
    uint32_t vol_low, vol_med, vol_high;

    if (g_config_ini == NULL) {
        vol_low  = 0x4B;   /* 75 */
        vol_med  = 0x4B;   /* 75 */
        vol_high = 0x4E;   /* 78 */
    } else {
        vol_low  = Config_GetIniInt(g_config_ini, "Sound", "VolumeLow",  0x4B);
        vol_med  = Config_GetIniInt(g_config_ini, "Sound", "VolumeMed",  0x4B);
        vol_high = Config_GetIniInt(g_config_ini, "Sound", "VolumeHigh", 0x4E);
    }

    /* Apply volume bounds */
    g_audio->SetBounds(static_cast<int32_t>(vol_low), static_cast<int32_t>(vol_med),
                        static_cast<int32_t>(vol_high), static_cast<int32_t>(vol_high));

    return 1;
}
