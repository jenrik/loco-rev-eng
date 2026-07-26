/**
 * DDRAudio.c — DirectSound lifecycle management (C free functions)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Functions for initializing, destroying, and error-reporting the
 * DirectSound audio subsystem.
 */

#include "../shared/types.h"

/* ================================================================== */
/* External declarations                                               */
/* ================================================================== */

/* Game Audio class forward — defined in GameAudio.h */
struct GameAudio;

/* Singleton global */
extern GameAudio* g_audio;              /* 0x4FD3BC */
extern void*      g_config_ini;         /* 0x4A9EEC */
extern void*      g_main_window;        /* 0x4A885C (or 0x4FD3C0) */

/* Config helpers */
extern int32_t __fastcall Config_GetIniInt(void* ini, const char* section,
                                             const char* key, int32_t def);
extern void __fastcall Config_WriteInt(void* ini, const char* section,
                                         const char* key, int32_t val);

/* GameAudio methods (declared in GameAudio.h, defined in GameAudio.cpp) */
extern void __fastcall GameAudio_Ctor(GameAudio* audio);
extern uint32_t __fastcall GameAudio_Init(GameAudio* audio);
extern void __fastcall GameAudio_SetListenerPos(GameAudio* audio,
                                                  int32_t world_x, int32_t world_y);
extern uint32_t __fastcall GameAudio_SetBounds(GameAudio* audio,
                                                 int32_t low, int32_t med,
                                                 int32_t high, int32_t max);
extern void __fastcall GameAudio_Cleanup(GameAudio* audio, void* hwnd);

/* MSVC CRT */
extern void* __cdecl operator_new(size_t size);       /* 0x465CE0 */

/* ================================================================== */
/* DDRAW_InitAudio — 0x45B7E0                                           */
/*                                                                      */
/* Create the GameAudio singleton, initialize DirectSound, configure   */
/* PCM output (22050Hz 16-bit mono), and load volume bounds from       */
/* the config INI file.                                                 */
/*                                                                      */
/* Flow:                                                                */
/*   1. If g_audio already exists, return early (idempotent)           */
/*   2. Allocate 0xB8-byte GameAudio, call Ctor + Init                 */
/*   3. If Init fails, destroy and return 0                             */
/*   4. Set listener position to world dimensions                       */
/*   5. Read VolumeLow/VolumeMed/VolumeHigh from [Sound] section       */
/*   6. Default values: 0x4B (75) for low/med, 0x4E (78) for high     */
/*                                                                      */
/* Return: 1 on success, 0 on failure.                                 */
/* ================================================================== */
uint32_t __cdecl DDRAW_InitAudio(void)
{
    if (g_audio != NULL) {
        return (uint32_t)g_audio & 0xFFFFFF00;  /* Already initialized */
    }

    /* Allocate and construct GameAudio */
    {
        GameAudio* audio = (GameAudio*)operator_new(0xB8);
        if (audio == NULL) {
            g_audio = NULL;
        } else {
            g_audio = audio;
            GameAudio_Ctor(audio);
        }
    }

    if (g_audio == NULL) {
        return 0;
    }

    /* Initialize DirectSound */
    {
        uint32_t result = GameAudio_Init(g_audio);
        if ((uint8_t)result == 0) {
            /* Init failed — clean up */
            if (g_audio != NULL) {
                (*(void(**)(void*, int))(* (void**)g_audio))(g_audio, 1);
            }
            g_audio = NULL;
            return result & 0xFFFFFF00;
        }
    }

    /* Set listener to world dimensions */
    {
        extern int32_t g_world_width;   /* 0x4AAD10 — world tile width  */
        extern int32_t g_world_height;  /* 0x4AAD14 — world tile height */
        GameAudio_SetListenerPos(g_audio, g_world_width, g_world_height);
    }

    /* Load volume bounds from config */
    {
        int32_t volLow;
        int32_t volMed;
        int32_t volHigh;

        if (g_config_ini == NULL) {
            volLow  = 0x4B;  /* 75 */
            volMed  = 0x4B;  /* 75 */
            volHigh = 0x4E;  /* 78 */
        } else {
            volLow  = Config_GetIniInt(g_config_ini,
                                        (const char*)0x0047e2c0,  /* "Sound" */
                                        (const char*)0x0047f164,  /* "VolumeLow" */
                                        0x4B);
            volMed  = Config_GetIniInt(g_config_ini,
                                        (const char*)0x0047e2c0,
                                        (const char*)0x0047f158,  /* "VolumeMed" */
                                        0x4B);
            volHigh = Config_GetIniInt(g_config_ini,
                                        (const char*)0x0047e2c0,
                                        (const char*)0x0047f14c,  /* "VolumeHigh" */
                                        0x4E);
        }

        GameAudio_SetBounds(g_audio, volLow, volMed, volHigh, volHigh);
    }

    return 1;
}

/* ================================================================== */
/* DDRAW_DestroyAudio — 0x45BB20                                        */
/*                                                                      */
/* Save volume settings to config INI, clean up GameAudio, and free    */
/* the singleton. Called during application shutdown.                  */
/* ================================================================== */
void __cdecl DDRAW_DestroyAudio(void)
{
    if (g_audio == NULL) return;

    /* Save volume settings back to config */
    if (g_config_ini != NULL) {
        Config_WriteInt(g_config_ini,
                        (const char*)0x0047e2c0,  /* "Sound" */
                        (const char*)0x0047f164,  /* "VolumeLow" */
                        *(int32_t*)((int8_t*)g_audio + 0x10));  /* saved_bounds[3] */
        Config_WriteInt(g_config_ini,
                        (const char*)0x0047e2c0,
                        (const char*)0x0047f158,  /* "VolumeMed" */
                        *(int32_t*)((int8_t*)g_audio + 0x0C));  /* saved_bounds[2] */
        Config_WriteInt(g_config_ini,
                        (const char*)0x0047e2c0,
                        (const char*)0x0047f14c,  /* "VolumeHigh" */
                        *(int32_t*)((int8_t*)g_audio + 0x08));  /* saved_bounds[1] */
    }

    /* Cleanup audio subsystem */
    GameAudio_Cleanup(g_audio, *(void**)((int8_t*)g_main_window + 8));

    /* Destroy the GameAudio object */
    if (g_audio != NULL) {
        (*(void(**)(void*, int))(* (void**)g_audio))(g_audio, 1);
    }
    g_audio = NULL;
}

/* ================================================================== */
/* DDRAW_GetDsoundErrorString — 0x45C2E0                                */
/*                                                                      */
/* Convert a DirectSound error HRESULT to a human-readable string.     */
/* Covers all DSERR_* codes used by the game.                          */
/* ================================================================== */
const char* __cdecl DDRAW_GetDsoundErrorString(int32_t hr)
{
    /* The original is a dense series of comparisons with magic constants.
       These map to DSERR_* values as follows: */

    if (hr < -0x7FF8FFF1) {
        if (hr == -0x7FF8FFF2)  return (const char*)0x004810F0;  /* "DSERR_OUTOFMEMORY" */
        if (hr == -0x7FFFBFFF)  return (const char*)0x00481110;  /* "DSERR_UNSUPPORTED" */
        if (hr == -0x7FFFBFFE)  return (const char*)0x00481130;  /* "DSERR_NOINTERFACE" */
        if (hr == -0x7FFFBFFB)  return (const char*)0x00481150;  /* "DSERR_GENERIC" */
    }
    else if (hr < -0x7787FFF5) {
        if (hr == -0x7787FFF6)  return (const char*)0x004810AC;  /* "DSERR_ALLOCATED" */
        if (hr == -0x7FF8FFA9)  return (const char*)0x004810CC;  /* "DSERR_INVALIDPARAM" */
    }
    else if (hr < -0x7787FFCD) {
        if (hr == -0x7787FFCE)  return (const char*)0x00481068;  /* "DSERR_INVALIDCALL" */
        if (hr == -0x7787FFE2)  return (const char*)0x00481088;  /* "DSERR_CONTROLUNAVAIL" */
    }
    else if (hr < -0x7787FF9B) {
        if (hr == -0x7787FF9C)  return (const char*)0x00481024;  /* "DSERR_BADFORMAT" */
        if (hr == -0x7787FFBA)  return (const char*)0x00481044;  /* "DSERR_PRIOLEVELNEEDED" */
    }
    else if (hr < -0x7787FF7D) {
        if (hr == -0x7787FF7E)  return (const char*)0x00480FDC;  /* "DSERR_ALREADYINITIALIZED" */
        if (hr == -0x7787FF88)  return (const char*)0x00481004;  /* "DSERR_NODRIVER" */
    }
    else {
        if (hr == -0x7787FF60)  return (const char*)0x00480F94;  /* "DSERR_OTHERAPPHASPRIO" */
        if (hr == -0x7787FF56)  return (const char*)0x00480FB8;  /* "DSERR_UNINITIALIZED" */
        if (hr == 0)            return (const char*)0x0047F18C;  /* "No error." */
    }

    return (const char*)0x0047F170;  /* "Unrecognized error value." */
}
