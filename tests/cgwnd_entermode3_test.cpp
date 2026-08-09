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
 *
 * LINK-001 note: this target links only obj_cgwnd, not obj_sdl3_window, so
 * it does not get the real GetFileVersionInfoSizeA/GetFileVersionInfoA/
 * VerQueryValueA/lstrcpyA/lstrcatA/lstrlenA from graphics/sdl3_window.cpp.
 * CGWND.cpp used to carry local static duplicates of these (removed as
 * part of LINK-001 — they were byte-identical to sdl3_window.cpp's and
 * GCC's ICF was silently folding them into colliding global symbols).
 * These 6 functions are called only from CGWND's install-path/version-
 * dialog code, never from the mode-2 early-return path this test
 * exercises, so leaving them unresolved would not fail this test — but it
 * would be a new call-0 landmine in the linked binary. Provide minimal
 * test-local implementations instead, matching CGWND.cpp's own extern "C"
 * declarations.
 */
#include "core/CGWND.h"

#include <cstdio>
#include <cstring>
#include <cstdint>

extern "C" {
uint32_t GetFileVersionInfoSizeA(const char*, uint32_t*);
uint32_t GetFileVersionInfoSizeA(const char*, uint32_t*) { return 0; }
int GetFileVersionInfoA(const char*, uint32_t, uint32_t, void*);
int GetFileVersionInfoA(const char*, uint32_t, uint32_t, void*) { return 0; }
int VerQueryValueA(void*, const char*, void**, uint32_t*);
int VerQueryValueA(void*, const char*, void**, uint32_t*) { return 0; }
char* lstrcpyA(char* dst, const char* src);
char* lstrcpyA(char* dst, const char* src) { return dst && src ? strcpy(dst, src) : dst; }
char* lstrcatA(char* dst, const char* src);
char* lstrcatA(char* dst, const char* src) { return dst && src ? strcat(dst, src) : dst; }
int lstrlenA(const char* s);
int lstrlenA(const char* s) { return s ? (int)strlen(s) : 0; }
}

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
