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

#include "../audio/GameAudio.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void __cdecl Config_WriteInt(void* config_ini,       /* 0x452DB0 */
                                     const char* section,
                                     const char* key,
                                     int32_t value);

/* Globals */
extern GameAudio* g_audio;          /* 0x4FD3BC */
extern void* g_config_ini;          /* 0x4A9EEC */
extern void* g_main_window;         /* 0x4AA4A0 */

/* Config section/key constants */
extern const char s_Sound_0047e2c0[];
extern const char s_VolumeLow_0047f164[];
extern const char s_VolumeMed_0047f158[];
extern const char s_VolumeHigh_0047f14c[];

/* Forward declaration (STRICT=2 -Wmissing-declarations); matches
 * graphics/DDRAW.h's existing declaration exactly, but this file stays
 * self-contained rather than including that header (which also
 * declares several *other* DDRAW_* functions with signatures that do
 * not match their native/*.c definitions — see commit message). */
void __cdecl DDRAW_DestroyAudio(void);

/* ================================================================== */
/* DDRAW_DestroyAudio — Save volume settings and shut down audio       */
/* Address: 0x45BB20                                                   */
/* Size: 152 bytes (39 insn)                                           */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* 1. If g_audio == NULL, return immediately                           */
/* 2. If g_config_ini != NULL, save volume settings (original x86      */
/*    offsets g_audio+0x10/0x0C/0x08, i.e. saved_bounds[3]/[2]/[1])    */
/*    to LOCO.INI [Sound] section                                      */
/* 3. Call GameAudio::Cleanup to shut down DirectSound                 */
/* 4. Destroy the GameAudio (real virtual destructor)                  */
/* 5. Set g_audio = nullptr                                            */
/*                                                                     */
/* Called by: RESMGR_Shutdown                                          */
/* ================================================================== */
void __cdecl DDRAW_DestroyAudio(void)
{
    if (g_audio == NULL) return;

    /* Save volume settings to config file. BUG FIX, not just a cast/
     * style cleanup: the raw offsets +0x10/+0x0C/+0x08 are the original
     * x86 byte offsets of GameAudio::saved_bounds[3]/[2]/[1]
     * (saved_bounds is the class's first data member, right after the
     * vtable pointer — +0x04 on the original's 4-byte x86 vtable slot,
     * per audio/GameAudio.h). On this 64-bit host the vtable pointer is
     * 8 bytes, so saved_bounds actually starts at host offset +0x08 —
     * the old `(uint8_t*)g_audio + 0x10` raw-pointer arithmetic landed
     * one element short of the field it was named for (host
     * saved_bounds[2]/[1]/[0] instead of the intended [3]/[2]/[1]),
     * silently reading/writing the wrong volume slider to LOCO.INI.
     * Named field access below is resolved against the compiler's own
     * (correct) layout, so indexing by the x86-derived element number
     * is now correct on host regardless of vtable pointer width. */
    if (g_config_ini != NULL) {
        Config_WriteInt(g_config_ini, s_Sound_0047e2c0,
                        s_VolumeLow_0047f164,
                        g_audio->saved_bounds[3]);
        Config_WriteInt(g_config_ini, s_Sound_0047e2c0,
                        s_VolumeMed_0047f158,
                        g_audio->saved_bounds[2]);
        Config_WriteInt(g_config_ini, s_Sound_0047e2c0,
                        s_VolumeHigh_0047f14c,
                        g_audio->saved_bounds[1]);
    }

    /* Cleanup DirectSound */
    {
        void* hWnd = *reinterpret_cast<void**>(static_cast<uint8_t*>(g_main_window) + 8);
        g_audio->Cleanup(hWnd);
    }

    /* Destroy GameAudio (real virtual destructor, not a manual
     * vtable[0] scalar-dtor dispatch — see commit message). */
    delete g_audio;

    g_audio = nullptr;
}
