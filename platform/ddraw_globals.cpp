/**
 * platform/ddraw_globals.cpp — canonical DirectDraw global defining declarations
 *
 * g_ddraw (0x485440), g_primary_surface (0x4FD3C4), and g_backbuffer
 * (0x4FD3C0) are declared void* at every consumer site tree-wide rather
 * than typed IDirectDraw4 / IDirectDrawSurface4 pointers — see PROGRESS.md's
 * DirectDraw-shim Phase 5/6 notes for why: keeping the declaration untyped avoids
 * forcing an unrelated retype at call sites that still do their own
 * pointer arithmetic on the same object for other reasons (e.g.
 * input/Cursor.cpp's CreateSurface desc buffer), and this shim's vtable
 * is not ABI-slot-accurate, so nothing may reach these through raw
 * vtable-slot dispatch regardless of the declared type. Every real
 * dispatch site now goes through static_cast<IDirectDrawSurface4*>/
 * static_cast<IDirectDraw4*> at the call boundary (Phase 6, done
 * 2026-08-14: ui/UIPANEL_Surface.cpp, graphics/LOCOBITMAP.cpp,
 * ui/UIPANEL.cpp, world/tilemap.cpp, the Cursor subsystem's 28 sites).
 *
 * `_g_primary_surface`/`_g_backbuffer` (input/Cursor_internal.h and a few
 * other files) are a SEPARATE pair of globals sharing the exact same
 * confirmed addresses as g_primary_surface/g_backbuffer — very likely
 * duplicate C++ variables for the same original global from incremental
 * decompilation, not a real distinction, but NOT consolidated here: that
 * "very likely" hasn't been fully confirmed, so this file deliberately
 * wires only the non-underscore pair. Consumers of the underscore-
 * prefixed pair (several Cursor subsystem call sites) stay dormant behind
 * their existing null checks even after this wiring — a known, separately
 * tracked gap, not an oversight.
 *
 * g_surface_bpp/g_surface_channel1/g_surface_channel2/g_surface_bshift/
 * g_surface_red_mask/g_surface_blue_mask (0x485274/0x485278/0x48527C/
 * 0x485280/0x485288/0x485290) are the pixel-format globals the original
 * DDRAW_GetSurface (0x45B500) sets after querying the real display's
 * DDPIXELFORMAT; native/ddraw_surface_ops.c, native/DDRAW_DimSurfaceRect.c,
 * and town/TownTiles.cpp's Town_CheckOccupiedEx read them for colour-key
 * and 16bpp pixel masking. g_surface_red_mask/g_surface_blue_mask had NO
 * defining declaration anywhere in the tree before this file (silently
 * tolerated only by the main binary's -Wl,--unresolved-symbols=ignore-all,
 * LINK-001 TODO in meson.build) — added here alongside their siblings.
 * SDL3_EnsurePrimarySurface (graphics/sdl3_ddraw.cpp) sets all six to real
 * RGB565 values once a device exists — see PROGRESS.md's DirectDraw-shim
 * pixel-format note.
 *
 * Kept in its own translation unit (rather than shared/stubs_impl.cpp /
 * shared/link_stubs.cpp, where these lived before this file existed) with
 * no dependencies beyond shared/types.h, so small standalone unit tests
 * that link graphics/sdl3_ddraw.cpp — which assigns/sets all of these —
 * can link these definitions without pulling in the rest of those files'
 * dependency graphs.
 */
#include <cstdint>

void* g_ddraw = nullptr;
void* g_primary_surface = nullptr;
void* g_backbuffer = nullptr;

int32_t g_surface_bpp = 0;
int32_t g_surface_channel1 = 0;
int32_t g_surface_channel2 = 0;
int32_t g_surface_bshift = 0;
int32_t g_surface_red_mask = 0;
int32_t g_surface_blue_mask = 0;
