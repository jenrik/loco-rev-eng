/**
 * platform/ddraw_globals.cpp — canonical g_ddraw defining declaration
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
 * Kept in its own translation unit (rather than shared/stubs_impl.cpp,
 * where it lived before this file existed) with no dependencies beyond
 * shared/types.h, so small standalone unit tests that link
 * graphics/sdl3_ddraw.cpp — which assigns a real device here once SDL3's
 * window/renderer exist, see SDL3_EnsurePrimarySurface — can link this
 * definition without pulling in the rest of shared/stubs_impl.cpp's
 * dependency graph.
 */
void* g_ddraw = nullptr;
