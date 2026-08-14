/**
 * ddraw_clippers.c — Release and free DirectDraw clipper/aeroplane objects
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C free functions, __cdecl and __fastcall. Release 6 DirectDraw
 * clipper objects (vtable[2] = Release) and one UIPANEL surface
 * (vtable[0] = scalar destructor). The "free" helper zeros out
 * a small 16-byte struct used as a temporary clipper handle buffer.
 */

#include <stdint.h>
#include "../platform/ddraw_interfaces.h"

/* ================================================================== */
/* External globals — 6 clipper pointers + 1 surface pointer           */
/*                                                                     */
/* The 6 clipper handles are real IDirectDrawClipper* now (see         */
/* platform/ddraw_interfaces.h) — typed here so Release goes through a */
/* real virtual method instead of manual vtable-offset arithmetic, per */
/* CLAUDE.md's anti-pattern rule ("Do not manually read or write       */
/* VTBL_* in executable code"). They stay permanently null on host     */
/* today (no code path constructs one), same as before this change.    */
/*                                                                     */
/* g_clipper_surf is NOT a DirectDraw object — it's a UIPANEL_Surface*  */
/* (graphics/LOCOBITMAP.h), a real modeled C++ class. Its destructor    */
/* isn't declared virtual in the current model yet (a separate,        */
/* pre-existing gap unrelated to DirectDraw — see that header), so its  */
/* raw vtable[0] dispatch is left as-is rather than folded into this    */
/* DirectDraw-interface fix; it's also always null today (same          */
/* liveness as the clippers), so this is a documentation-only note.     */
/* ================================================================== */

extern IDirectDrawClipper* g_clipper_0;    /* 0x004FF0FC — 1st clipper handle */
extern IDirectDrawClipper* g_clipper_1;    /* 0x004FF100 — 2nd clipper handle */
extern IDirectDrawClipper* g_clipper_2;    /* 0x004FF104 — 3rd clipper handle */
extern IDirectDrawClipper* g_clipper_3;    /* 0x004FF108 — 4th clipper handle */
extern IDirectDrawClipper* g_clipper_4;    /* 0x004FF10C — 5th clipper handle */
extern IDirectDrawClipper* g_clipper_5;    /* 0x004FF0F8 — 6th clipper handle */
extern void* g_clipper_surf; /* 0x004FF110 — clipper surface/UIPANEL surface */

/* Forward declarations (STRICT=2 -Wmissing-declarations). Self-contained
 * rather than pulled from a header: graphics/DDRAW.h declares both of
 * these with matching signatures, but several *other* DDRAW_* functions
 * in that header have signatures that do NOT match their native/*.c
 * definitions (pre-existing landmine, out of scope here — see commit
 * message), so this file avoids depending on it entirely. */
void __cdecl DDRAW_ReleaseClippers(void);
void __fastcall DDRAW_FreeClipper(void* clipper);

/* ================================================================== */
/* DDRAW_ReleaseClippers — Release all clipper objects                 */
/* Address: 0x45C970                                                   */
/* Size: 153 bytes (53 insn)                                           */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* MISNAMED: releases 6 IDirectDrawClipper objects (vtable[2]) + 1    */
/* UIPANEL surface (vtable[0]). Does NOT create clippers.             */
/*                                                                     */
/* Each clipper is released via its vtable slot [2] (Release method),  */
/* then the pointer is nulled. The UIPANEL surface is destroyed via    */
/* its vtable slot [0] (scalar destructor, called with flags=1).      */
/*                                                                     */
/* Called by: DDRAW_ReleaseSurfaces (0x45BABC)                         */
/* ================================================================== */
void __cdecl DDRAW_ReleaseClippers(void)
{
    if (g_clipper_0 != NULL) { g_clipper_0->Release(); g_clipper_0 = NULL; }
    if (g_clipper_1 != NULL) { g_clipper_1->Release(); g_clipper_1 = NULL; }
    if (g_clipper_2 != NULL) { g_clipper_2->Release(); g_clipper_2 = NULL; }
    if (g_clipper_3 != NULL) { g_clipper_3->Release(); g_clipper_3 = NULL; }
    if (g_clipper_4 != NULL) { g_clipper_4->Release(); g_clipper_4 = NULL; }
    if (g_clipper_5 != NULL) { g_clipper_5->Release(); g_clipper_5 = NULL; }

    /* Destroy UIPANEL surface — not a DirectDraw object, see the comment
     * on g_clipper_surf's declaration above. */
    if (g_clipper_surf != NULL) {
        uint8_t* vtbl = *static_cast<uint8_t**>(g_clipper_surf);
        int32_t (*destroy_fn)(uint32_t) = *reinterpret_cast<int32_t (**)(uint32_t)>(vtbl + 0 * sizeof(void*));
        destroy_fn(1);   /* vtable[0] = scalar destructor with delete */
        g_clipper_surf = NULL;
    }
}

/* ================================================================== */
/* DDRAW_FreeClipper — Zero out a small clipper handle struct          */
/* Address: 0x45CA10                                                   */
/* Size: 13 bytes (6 insn)                                             */
/* Calling convention: __fastcall (param_1 in ECX)                     */
/*                                                                     */
/* Zeros three fields in a 16-byte struct: [0], [4], [0xC].            */
/* Field at +0x08 is NOT touched (left intact).                        */
/*                                                                     */
/* Called by: NET_Lock (0x445F77)                                       */
/*                                                                     */
/* @param clipper  Pointer to 16-byte clipper handle struct            */
/* ================================================================== */
void __fastcall DDRAW_FreeClipper(void* clipper)
{
    uint32_t* p = static_cast<uint32_t*>(clipper);
    p[0] = 0;  /* +0x00 */
    p[1] = 0;  /* +0x04 */
    p[3] = 0;  /* +0x0C — note: +0x08 is NOT cleared */
}
