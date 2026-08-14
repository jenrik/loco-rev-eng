/**
 * platform/ddraw_globals.cpp — canonical DirectDraw global defining declarations
 *
 * g_ddraw (0x485440) is declared void* at every consumer site tree-wide
 * (native/ddraw_surface_ops.c, input/Cursor.cpp, input/Cursor_Editor.cpp,
 * input/Cursor_internal.h, core/CGWND.cpp, graphics/DDRAW.h,
 * graphics/LOCOBITMAP.cpp, ui/GameWindow.cpp, ui/UIPANEL.cpp,
 * ui/UIPANEL_Surface.cpp) rather than IDirectDraw4* — see PROGRESS.md's
 * DirectDraw-shim Phase 5 note for why: several of those consumers still
 * dispatch through it by raw vtable slot, and this shim's vtable is not
 * ABI-slot-accurate, so a shared typed declaration would just move a
 * type mismatch to a different call site rather than resolve it.
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

int32_t g_surface_bpp = 0;
int32_t g_surface_channel1 = 0;
int32_t g_surface_channel2 = 0;
int32_t g_surface_bshift = 0;
int32_t g_surface_red_mask = 0;
int32_t g_surface_blue_mask = 0;
