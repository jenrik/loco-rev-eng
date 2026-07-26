/**
 * ddraw_audio_destroy.c — Game audio subsystem shutdown
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C free function, __cdecl. Saves current volume settings to LOCO.INI,
 * then shuts down and destroys the global GameAudio instance.
 */

#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void __cdecl Config_WriteInt(void* config_ini,       /* 0x452DB0 */
                                     const char* section,
                                     const char* key,
                                     int32_t value);
extern void __cdecl GameAudio_Cleanup(void* audio,          /* 0x412EE0 */
                                       void* hWnd);

/* Globals */
extern void* g_audio;               /* 0x4FD3BC */
extern void* g_config_ini;          /* 0x4A9EEC */
extern void* g_main_window;         /* 0x4AA4A0 */

/* Config section/key constants */
extern const char s_Sound_0047e2c0[];
extern const char s_VolumeLow_0047f164[];
extern const char s_VolumeMed_0047f158[];
extern const char s_VolumeHigh_0047f14c[];

/* ================================================================== */
/* DDRAW_DestroyAudio — Save volume settings and shut down audio       */
/* Address: 0x45BB20                                                   */
/* Size: 152 bytes (39 insn)                                           */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* 1. If g_audio == NULL, return immediately                           */
/* 2. If g_config_ini != NULL, save volume settings (fields at         */
/*    g_audio+0x10/0x0C/0x08) to LOCO.INI [Sound] section             */
/* 3. Call GameAudio_Cleanup to shut down DirectSound                  */
/* 4. Destroy the GameAudio via vtable[0] scalar destructor (flags=1)  */
/* 5. Set g_audio = NULL                                               */
/*                                                                     */
/* Called by: RESMGR_Shutdown                                          */
/* ================================================================== */
void __cdecl DDRAW_DestroyAudio(void)
{
    if (g_audio == NULL) return;

    /* Save volume settings to config file */
    if (g_config_ini != NULL) {
        Config_WriteInt(g_config_ini, s_Sound_0047e2c0,
                        s_VolumeLow_0047f164,
                        *(int32_t*)((uint8_t*)g_audio + 0x10));
        Config_WriteInt(g_config_ini, s_Sound_0047e2c0,
                        s_VolumeMed_0047f158,
                        *(int32_t*)((uint8_t*)g_audio + 0x0C));
        Config_WriteInt(g_config_ini, s_Sound_0047e2c0,
                        s_VolumeHigh_0047f14c,
                        *(int32_t*)((uint8_t*)g_audio + 0x08));
    }

    /* Cleanup DirectSound */
    {
        void* hWnd = *(void**)((uint8_t*)g_main_window + 8);
        GameAudio_Cleanup(g_audio, hWnd);
    }

    /* Destroy GameAudio (vtable[0] = scalar destructor with delete) */
    if (g_audio != NULL) {
        void* vtable = *(void**)g_audio;
        void (*dtor)(uint32_t) = (void (*)(uint32_t))vtable;
        dtor(1);
    }

    g_audio = NULL;
}
