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

void* g_town_view = nullptr;
DDRAW_Building* g_ddraw_building = nullptr;

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
