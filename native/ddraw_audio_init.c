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

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* operator_new(uint32_t size);

/* GameAudio methods */
extern void* __fastcall GameAudio_Ctor(void* mem);         /* 0x412BD0 */
extern uint32_t __cdecl GameAudio_Init(void* audio, int channels, void* hwnd); /* 0x412C50 */
extern void __cdecl GameAudio_SetListenerPos(void* audio,    /* 0x4130A0 */
                                              int32_t world_w,
                                              int32_t world_h);
extern uint32_t __cdecl GameAudio_SetBounds(void* audio,     /* 0x413630 */
                                             uint32_t low,
                                             uint32_t med,
                                             uint32_t high,
                                             uint32_t high2);

/* Config reader */
extern int32_t __cdecl Config_GetIniInt(void* config_ini,    /* 0x452D60 */
                                         const char* section,
                                         const char* key,
                                         int32_t default_val);

/* Globals */
extern void* g_audio;               /* 0x4FD3BC — global GameAudio* */
extern void* g_config_ini;          /* 0x4A9EEC */
extern void* g_main_window;         /* 0x4AA4A0 */
extern int32_t g_world_width;       /* 0x4AAD10 */
extern int32_t g_world_height;      /* 0x4AAD0C */

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
    void* audio_mem;
    uint32_t result;

    /* Already initialised? */
    if (g_audio != NULL) {
        return 0;
    }

    /* Allocate GameAudio (0xB8 bytes) */
    audio_mem = operator_new(0xB8);
    if (audio_mem != NULL) {
        g_audio = GameAudio_Ctor(audio_mem);
    } else {
        g_audio = NULL;
    }

    if (g_audio == NULL) {
        return 0;
    }

    /* Initialise DirectSound */
    {
        void* hWnd = *(void**)((uint8_t*)g_main_window + 8);
        result = GameAudio_Init(g_audio, 16, hWnd);
    }

    if (result == 0) {
        if (g_audio != NULL) {
            /* Scalar destructor via vtable[0] */
            void* vtable = *(void**)g_audio;
            void (*dtor)(uint32_t) = (void (*)(uint32_t))vtable;
            dtor(1);
        }
        g_audio = NULL;
        return 0;
    }

    /* Set listener position at world centre */
    GameAudio_SetListenerPos(g_audio, g_world_width, g_world_height);

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
    GameAudio_SetBounds(g_audio, vol_low, vol_med, vol_high, vol_high);

    return 1;
}
