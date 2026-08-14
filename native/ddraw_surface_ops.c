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
#include "../platform/ddraw_interfaces.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void __cdecl DDRAW_ReleaseClippers(void);  /* 0x45C970 */

/* Globals. Kept void* at the declaration site (matching every other TU
 * that declares them) rather than retyped to IDirectDraw4* /
 * IDirectDrawSurface4* tree-wide: several other consumers of g_ddraw
 * (input/Cursor.cpp, input/Cursor_Editor.cpp) still index its vtable by
 * raw slot number for CreateSurface, and that slot does not land on
 * CreateSurface in this shim's (non-ABI) declaration order — retyping
 * the shared declaration everywhere would just move the mismatch to a
 * compile error there instead of fixing it. This file casts to the real
 * interface at the point of use instead (see below), same boundary-cast
 * idiom as DDRAW_RestoreSurfaces's `surface` parameter. */
extern int32_t g_surface_bpp;       /* 0x485274 — pixel format: 0x22B=555, 0x235=565 */
extern void*   g_ddraw;             /* 0x485440 — IDirectDraw4* */
extern void*   g_backbuffer;        /* 0x4FD3C0 — backbuffer surface, IDirectDrawSurface4* */
extern void*   g_primary_surface;   /* 0x4FD3C4 — primary surface, IDirectDrawSurface4* */

extern void* DAT_004A9908;          /* 0x4A9908 — DirectDraw module/something */

/* Forward declarations (STRICT=2 -Wmissing-declarations). Self-contained
 * rather than pulled from graphics/DDRAW.h: that header's own
 * declarations for these two functions use different, non-matching
 * parameter types (DDRAW_RestoreSurfaces(int*, uint32_t) vs. this
 * file's (void*, void*); a stale/never-validated declaration, see
 * commit message and the landmine doc's existing note that
 * DDRAW_RestoreSurfaces has two genuinely distinct real overloads). */
void __cdecl DDRAW_RestoreSurfaces(void* surface, void* unused);
void __cdecl DDRAW_ReleaseSurfaces(void);

/* ================================================================== */
/* DDRAW_RestoreSurfaces — Restore a lost surface and re-apply colorkey*/
/* Address: 0x45BA50                                                   */
/* Size: 76 bytes (27 insn)                                            */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Calls IDirectDrawSurface::Restore (real ABI vtable[22]) then          */
/* re-applies the colour key mask via SetColorKey (real ABI vtable[29]). */
/* Dispatched by name through platform/ddraw_interfaces.h, not by raw   */
/* slot — this shim is API- not ABI-compatible (see CLAUDE.md). The     */
/* colour key mask depends on the pixel format:                        */
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
    /* `surface` stays void* at the parameter boundary (see the forward-
     * declaration comment above — graphics/DDRAW.h's stale sibling
     * declaration disagrees on parameter types, a separate pre-existing
     * landmine out of scope here); cast once to the real interface and
     * dispatch by name instead of raw vtable slots. */
    IDirectDrawSurface4* surf = static_cast<IDirectDrawSurface4*>(surface);
    surf->Restore();

    /* Determine colour key mask based on pixel format */
    uint32_t color_key;
    if (g_surface_bpp == 0x22B) {
        color_key = 0x7C1F;   /* 555: bits 0-4=R,5-9=G,10-14=B */
    } else if (g_surface_bpp == 0x235) {
        color_key = 0xF81F;   /* 565: bits 0-4=R,5-10=G,11-15=B */
    } else {
        color_key = 0;        /* unknown format — no colour key */
    }

    DDCOLORKEY key{ color_key, color_key };
    surf->SetColorKey(8, &key);
}

/* ================================================================== */
/* DDRAW_ReleaseSurfaces — Release all DirectDraw surfaces & g_ddraw  */
/* Address: 0x45BAA0                                                   */
/* Size: 113 bytes (40 insn)                                           */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Releases in order, each dispatched by name (real ABI slot noted for  */
/* evidence only — this shim is API- not ABI-compatible):               */
/*   1. Backbuffer surface (g_backbuffer at 0x4FD3C0, real vtable[2])  */
/*   2. Primary surface (g_primary_surface at 0x4FD3C4, real vtable[2])*/
/*   3. All clipper objects (DDRAW_ReleaseClippers)                    */
/*   4. SetCooperativeLevel(NULL, 8) then Release on g_ddraw           */
/*      (IDirectDraw4, real vtable[20]/[2])                             */
/*   5. Release DAT_004A9908 (unmodeled intermediate IDirectDraw,      */
/*      raw vtable[2] dispatch — see in-body comment)                   */
/*                                                                     */
/* Called by: RESMGR_Shutdown                                          */
/* ================================================================== */
void __cdecl DDRAW_ReleaseSurfaces(void)
{
    if (DAT_004A9908 == NULL) return;

    /* Release backbuffer */
    if (g_backbuffer != NULL) {
        static_cast<IDirectDrawSurface4*>(g_backbuffer)->Release();
        g_backbuffer = NULL;
    }

    /* Release primary surface */
    if (g_primary_surface != NULL) {
        static_cast<IDirectDrawSurface4*>(g_primary_surface)->Release();
        g_primary_surface = NULL;
    }

    /* Release clipper objects */
    DDRAW_ReleaseClippers();

    /* Release g_ddraw (IDirectDraw4) */
    if (g_ddraw != NULL) {
        IDirectDraw4* dd = static_cast<IDirectDraw4*>(g_ddraw);
        dd->SetCooperativeLevel(NULL, 8);  /* restore normal coop level */
        dd->Release();
        g_ddraw = NULL;
    }

    /* Release the DD module wrapper. DAT_004A9908 is NOT an IDirectDraw4 —
     * xref analysis (2026-08-14) shows it's the intermediate, pre-
     * QueryInterface-upgrade IDirectDraw object DDRAW_GetSurface obtains
     * before upgrading to the real g_ddraw (IDirectDraw4); no interface is
     * modeled for it yet, so its raw vtable[2] Release dispatch is left
     * as-is rather than folded into this fix (same left-as-documented
     * treatment as g_clipper_surf in native/ddraw_clippers.c). */
    void** vtable = *static_cast<void***>(DAT_004A9908);
    (reinterpret_cast<void (*)(void*)>(vtable[2]))(DAT_004A9908);
    DAT_004A9908 = NULL;
}
