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

/* ================================================================== */
/* External globals — 6 clipper pointers + 1 surface pointer           */
/* ================================================================== */

extern void* g_clipper_0;    /* 0x004FF0FC — 1st clipper handle */
extern void* g_clipper_1;    /* 0x004FF100 — 2nd clipper handle */
extern void* g_clipper_2;    /* 0x004FF104 — 3rd clipper handle */
extern void* g_clipper_3;    /* 0x004FF108 — 4th clipper handle */
extern void* g_clipper_4;    /* 0x004FF10C — 5th clipper handle */
extern void* g_clipper_5;    /* 0x004FF0F8 — 6th clipper handle */
extern void* g_clipper_surf; /* 0x004FF110 — clipper surface/UIPANEL surface */

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
    void** ptr;
    int32_t (*release)(void*);
    int32_t (*destroy)(uint32_t flags);

    /* Release clipper 0 */
    if (g_clipper_0 != NULL) {
        release = *(int32_t (**)(void*))g_clipper_0;
        release[2](g_clipper_0);   /* vtable[2] = Release() */
        g_clipper_0 = NULL;
    }

    /* Release clipper 1 */
    if (g_clipper_1 != NULL) {
        release = *(int32_t (**)(void*))g_clipper_1;
        release[2](g_clipper_1);
        g_clipper_1 = NULL;
    }

    /* Release clipper 2 */
    if (g_clipper_2 != NULL) {
        release = *(int32_t (**)(void*))g_clipper_2;
        release[2](g_clipper_2);
        g_clipper_2 = NULL;
    }

    /* Release clipper 3 */
    if (g_clipper_3 != NULL) {
        release = *(int32_t (**)(void*))g_clipper_3;
        release[2](g_clipper_3);
        g_clipper_3 = NULL;
    }

    /* Release clipper 4 */
    if (g_clipper_4 != NULL) {
        release = *(int32_t (**)(void*))g_clipper_4;
        release[2](g_clipper_4);
        g_clipper_4 = NULL;
    }

    /* Release clipper 5 */
    if (g_clipper_5 != NULL) {
        release = *(int32_t (**)(void*))g_clipper_5;
        release[2](g_clipper_5);
        g_clipper_5 = NULL;
    }

    /* Destroy UIPANEL surface */
    if (g_clipper_surf != NULL) {
        destroy = *(int32_t (**)(uint32_t))g_clipper_surf;
        destroy(1);   /* vtable[0] = scalar destructor with delete */
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
/* Called by: ?                                                        */
/*                                                                     */
/* @param clipper  Pointer to 16-byte clipper handle struct            */
/* ================================================================== */
void __fastcall DDRAW_FreeClipper(void* clipper)
{
    uint32_t* p = (uint32_t*)clipper;
    p[0] = 0;  /* +0x00 */
    p[1] = 0;  /* +0x04 */
    p[3] = 0;  /* +0x0C — note: +0x08 is NOT cleared */
}
