/**
 * ddraw_surface_ops.c — DirectDraw surface restore and release
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C free functions, __cdecl. Restore lost surfaces and release all
 * DirectDraw surfaces/clippers/g_ddraw during shutdown.
 */

#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void __cdecl DDRAW_ReleaseClippers(void);  /* 0x45C970 */

/* Globals */
extern int32_t g_surface_bpp;       /* 0x485274 — pixel format: 0x22B=555, 0x235=565 */
extern void*   g_ddraw;             /* 0x485440 — IDirectDraw4* */
extern void*   g_backbuffer;        /* 0x4FD3C0 — backbuffer surface */
extern void*   g_primary_surface;   /* 0x4FD3C4 — primary surface */

extern void* DAT_004A9908;          /* 0x4A9908 — DirectDraw module/something */

/* ================================================================== */
/* DDRAW_RestoreSurfaces — Restore a lost surface and re-apply colorkey*/
/* Address: 0x45BA50                                                   */
/* Size: 76 bytes (27 insn)                                            */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Calls IDirectDrawSurface::Restore (vtable[22]) then re-applies the */
/* colour key mask via SetColorKey (vtable[29]). The colour key mask   */
/* depends on the pixel format:                                        */
/*   - 0x22B (555 bpp, 15-bit): 0x7C1F                                */
/*   - 0x235 (565 bpp, 16-bit): 0xF81F                                */
/*                                                                     */
/* The colour key is passed as a DWORD embedded in a DDSURFACEDESC-like*/
/* struct on the stack. The original code writes the colour-key value  */
/* at two adjacent stack locations.                                    */
/*                                                                     */
/* Called by: DDRAW_LoadBmpToSurface, various surface ops              */
/*                                                                     */
/* @param surface  IDirectDrawSurface* to restore                      */
/* @param unused   Unused parameter (typically a DDSURFACEDESC*)       */
/* ================================================================== */
void __cdecl DDRAW_RestoreSurfaces(void* surface, void* unused)
{
    /* COM interface vtable at +0x00. */
    void** vtable = *(void***)surface;
    ((void (*)(void*))vtable[22])(surface);

    /* Determine colour key mask based on pixel format */
    uint32_t color_key;
    if (g_surface_bpp == 0x22B) {
        color_key = 0x7C1F;   /* 555: bits 0-4=R,5-9=G,10-14=B */
    } else if (g_surface_bpp == 0x235) {
        color_key = 0xF81F;   /* 565: bits 0-4=R,5-10=G,11-15=B */
    } else {
        color_key = 0;        /* unknown format — no colour key */
    }

    /* Build a small DWORD pair on stack for SetColorKey param */
    /* The original code puts the same value in two adjacent DWORDs */
    uint32_t key_buf[2];
    key_buf[0] = color_key;
    key_buf[1] = color_key;

    /* Call SetColorKey (vtable[0x74/4 = 29]) with DDCOLORKEY struct */
    /* DDCOLORKEY has dwColorSpaceLowValue + dwColorSpaceHighValue */
    vtable = *(void***)surface;
    ((void (*)(void*, uint32_t, uint32_t*))vtable[29])(surface, 8, key_buf);
}

/* ================================================================== */
/* DDRAW_ReleaseSurfaces — Release all DirectDraw surfaces & g_ddraw  */
/* Address: 0x45BAA0                                                   */
/* Size: 113 bytes (40 insn)                                           */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Releases in order:                                                  */
/*   1. Backbuffer surface (g_backbuffer at 0x4FD3C0, vtable[2])      */
/*   2. Primary surface (g_primary_surface at 0x4FD3C4, vtable[2])    */
/*   3. All clipper objects (DDRAW_ReleaseClippers)                    */
/*   4. CooperativeLevel (vtable[20] = SetCooperativeLevel with NULL)  */
/*      then Release (vtable[2]) on g_ddraw (IDirectDraw4)            */
/*   5. Release DAT_004A9908 (vtable[2])                               */
/*                                                                     */
/* Called by: RESMGR_Shutdown                                          */
/* ================================================================== */
void __cdecl DDRAW_ReleaseSurfaces(void)
{
    if (DAT_004A9908 == NULL) return;

    /* Release backbuffer */
    if (g_backbuffer != NULL) {
        void** vtable = *(void***)g_backbuffer;
        ((void (*)(void*))vtable[2])(g_backbuffer);  /* Release() */
        g_backbuffer = NULL;
    }

    /* Release primary surface */
    if (g_primary_surface != NULL) {
        void** vtable = *(void***)g_primary_surface;
        ((void (*)(void*))vtable[2])(g_primary_surface);
        g_primary_surface = NULL;
    }

    /* Release clipper objects */
    DDRAW_ReleaseClippers();

    /* Release g_ddraw (IDirectDraw4) */
    if (g_ddraw != NULL) {
        void** vtable = *(void***)g_ddraw;
        /* SetCooperativeLevel(NULL, 8) — restore normal coop level */
        ((void (*)(void*, void*, uint32_t))vtable[20])(g_ddraw, NULL, 8);
        /* Release() */
        ((void (*)(void*))vtable[2])(g_ddraw);
        g_ddraw = NULL;
    }

    /* Release the DD module wrapper */
    void** vtable = *(void***)DAT_004A9908;
    ((void (*)(void*))vtable[2])(DAT_004A9908);
    DAT_004A9908 = NULL;
}
