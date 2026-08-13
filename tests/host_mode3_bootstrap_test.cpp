// Status: INTEGRATED
/** Host mode-3 bootstrap regression.
 *
 * The coredump lego_loco-3.core stopped at RIP=0 after the first mode-3
 * frame dispatched Town_TrackBuilding and DDRAW_UpdateBuilding with null
 * dependencies.  GameLoop_Setup must call BootstrapTownMode3Objects before
 * that frame.  This component test verifies the bootstrap creates stable,
 * zero-initialized host backing storage and that both first-frame callees
 * return through their inactive guards.
 */
#include "sdl3_town_mode3.h"
#include "graphics/DDRAW.h"

#include <cstdio>
#include <cstdlib>

void* g_town_view = nullptr;
/* g_ddraw_building is now defined in graphics/DDRAW.cpp (linked in via
 * obj_ddraw, tests/meson.build) since BootstrapTownMode3Objects()
 * placement-new constructs a real DDRAW_Building — see
 * BUG-mode3-input-processing-crashes.md. Real construction (GameView →
 * Panel → GameObject; Entity, via GameView's embedded game_object_sub)
 * also needs g_empty_string (canonically defined in
 * shared/stubs_impl.cpp, not linked into this isolated-object test) —
 * defined locally rather than pulling that whole stub file in. */
char g_empty_string = 0;

/* DDRAW.cpp's static UI_Manager singleton (g_tooltip_mgr, 0x4FD220 — see
 * that file's doc comment) is now constructed unconditionally at process
 * startup, matching the original CRT static-initializer behavior. That
 * construction calls UITimerList::Resize (ui/UI_Utils.cpp, obj_ui_utils
 * in tests/meson.build), which allocates via operator_new/GLOBAL_free.
 * Neither is provided by the narrow object set this test links, so —
 * following this file's existing g_empty_string precedent — they are
 * defined locally rather than pulling in shared/link_stubs.cpp's whole
 * Win32-stub translation unit. */
void* operator_new(size_t size) { return std::malloc(size); }
void  GLOBAL_free(void* ptr) { std::free(ptr); }

void Town_TrackBuilding(void* self);
void DDRAW_UpdateBuilding(void* self);

int main()
{
    if (loco::host::Mode3FrameDependenciesReady()) {
        std::fputs("FAIL: mode-3 frame dependencies started initialized\n", stderr);
        return 1;
    }

    loco::host::BootstrapTownMode3Objects();
    if (!loco::host::Mode3FrameDependenciesReady()) {
        std::fputs("FAIL: mode-3 frame dependencies were not bootstrapped\n", stderr);
        return 1;
    }
    void* const game_view = g_town_view;
    DDRAW_Building* const ddraw_building = g_ddraw_building;
    loco::host::BootstrapTownMode3Objects();
    if (g_town_view != game_view || g_ddraw_building != ddraw_building) {
        std::fputs("FAIL: mode-3 bootstrap was not idempotent\n", stderr);
        return 1;
    }

    Town_TrackBuilding(g_town_view);
    DDRAW_UpdateBuilding(g_ddraw_building);
    std::puts("PASS: mode-3 frame dependencies are ready before dispatch");
    return 0;
}
