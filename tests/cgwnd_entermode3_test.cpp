// Status: VALIDATED
/**
 * CGWND_EnterMode3(2) safe early return — component regression.
 *
 * Address: 0x4086F0 (CGWND_EnterMode3), 0x408726 (case 2 early return)
 *
 * CGWND_EnterMode3 receives the PREVIOUS mode and transitions into mode 3.
 * The mode-2 branch (coming from the main menu) sets g_game_mode = 2 and
 * returns immediately — it does not fall through to the common-tail cleanup
 * that accesses Town, PostcardAlbum, Cursor, BuildingMgr, World, etc.
 *
 * This test links against the real CGWND.o and provides only the one global
 * that the mode-2 branch touches (g_game_mode).  All other undefined symbols
 * are left for the linker to ignore, isolating the early-return contract
 * without pulling in the full game-object graph.
 *
 * Symbol ownership: CGWND_EnterMode3 is a C++ free function (mangled
 * _Z16CGWND_EnterMode3i) declared in core/CGWND.h — not extern "C".
 */
#include "core/CGWND.h"

#include <cstdio>

/* ---- The single global touched in the mode-2 early-return path ---- */
int g_game_mode = 0;

int main()
{
    /* Sanity: g_game_mode starts at a known sentinel */
    g_game_mode = 99;
    CGWND_EnterMode3(2);
    if (g_game_mode != 2) {
        std::fprintf(stderr,
            "FAIL: CGWND_EnterMode3(2) set g_game_mode=%d, expected 2\n",
            g_game_mode);
        return 1;
    }

    /* Round-trip: a second call still exits through the same branch */
    CGWND_EnterMode3(2);
    if (g_game_mode != 2) {
        std::fprintf(stderr,
            "FAIL: second CGWND_EnterMode3(2) set g_game_mode=%d, expected 2\n",
            g_game_mode);
        return 1;
    }

    std::puts("PASS: CGWND_EnterMode3(2) safe early return sets g_game_mode=2");
    return 0;
}
