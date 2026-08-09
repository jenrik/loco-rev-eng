/**
 * ddraw_init.c — DDRAW subsystem initialisation (thumbnail palette)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C free function, __cdecl. Creates a UIPANEL surface and loads the
 * "2__smisc_thumbpal.bmp" thumbnail palette resource onto it.
 * Called once during startup from GameLoop_Setup.
 */

#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* operator_new(uint32_t size);

extern void* __cdecl UIPANEL_CreateSurface(void* surface_mem);
extern void  __cdecl UIPANEL_StretchBlit(void* surface, char* bmp_name,
                                          uint32_t bmp_type, int32_t x, int32_t y);
extern void  __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...);

/* Global surface pointer at 0x4FF110 */
extern void* g_thumbpal_surface;   /* 0x004FF110 — thumbnail palette surface */

/* Global format string at 0x481178 — offset past "Lego_" prefix */
extern const char g_thumbpal_bmp_name[];  /* "2__smisc_thumbpal_bmp" at 0x4A99C8 */

/* ================================================================== */
/* DDRAW_Init — Create thumbnail palette surface and load bitmap       */
/* Address: 0x45C8A0                                                   */
/* Size: 197 bytes (58 insn)                                           */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Creates a UIPANEL surface (0x20 byte wrapper), stores the result    */
/* in g_thumbpal_surface (0x4FF110), then loads the string             */
/* "2__smisc_thumbpal_bmp" from a local buffer and calls               */
/* UIPANEL_StretchBlit to blit the thumbnail palette bitmap onto it.   */
/*                                                                     */
/* Has SEH for operator_new(0x20) — if it fails, g_thumbpal_surface    */
/* is set to NULL and init returns false.                               */
/*                                                                     */
/* Called by: GameLoop_Setup (0x406CC7)                                 */
/*                                                                     */
/* @return  1 if surface was created successfully, 0 on failure        */
/* ================================================================== */
uint32_t __cdecl DDRAW_Init(void);
uint32_t __cdecl DDRAW_Init(void)
{
    void* surface_mem = operator_new(0x20);

    if (surface_mem != NULL) {
        g_thumbpal_surface = UIPANEL_CreateSurface(surface_mem);
    } else {
        g_thumbpal_surface = NULL;
    }

    /* "2__smisc_thumbpal_bmp" resource (at 0x4A99C8) */
    char local_buf[0x104];
    CRT_sprintf_buf(local_buf, g_thumbpal_bmp_name + 2);

    /* Blit thumbnail palette onto the surface */
    UIPANEL_StretchBlit(g_thumbpal_surface, local_buf, 1, 0, 0);

    return (g_thumbpal_surface != NULL) ? 1 : 0;
}
