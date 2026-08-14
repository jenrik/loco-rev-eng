// Status: VALIDATED
/** sdl3_ddraw_palette_clipper_test.cpp — Palette/Clipper lifecycle regression.
 *
 * Confirms IDirectDrawPalette/IDirectDrawClipper round-trip through the
 * real typed interface (construct via IDirectDraw4's factory methods,
 * read/write entries, release) without leaking or double-freeing.
 */
#include "sdl3_ddraw.h"

#include <cstdio>
#include <cstring>

int main()
{
    Sdl3DirectDraw4 dd;

    /* Palette: round-trip a few entries through Set/GetEntries. */
    IDirectDrawPalette* pal_iface = nullptr;
    uint8_t initial[4] = { 0x10, 0x20, 0x30, 0x00 };
    if (dd.CreatePalette(0, initial, &pal_iface, nullptr) != 0 || !pal_iface) {
        std::fprintf(stderr, "FAIL: CreatePalette failed\n");
        return 1;
    }

    uint8_t roundtrip[4] = {};
    if (pal_iface->GetEntries(0, 0, 1, roundtrip) != 0 ||
        std::memcmp(initial, roundtrip, sizeof(initial)) != 0) {
        std::fprintf(stderr, "FAIL: palette entry 0 did not round-trip\n");
        pal_iface->Release();
        return 1;
    }

    uint8_t updated[4] = { 0x40, 0x50, 0x60, 0x00 };
    if (pal_iface->SetEntries(0, 5, 1, updated) != 0) {
        std::fprintf(stderr, "FAIL: SetEntries failed\n");
        pal_iface->Release();
        return 1;
    }
    if (pal_iface->GetEntries(0, 5, 1, roundtrip) != 0 ||
        std::memcmp(updated, roundtrip, sizeof(updated)) != 0) {
        std::fprintf(stderr, "FAIL: palette entry 5 did not round-trip after SetEntries\n");
        pal_iface->Release();
        return 1;
    }
    pal_iface->Release();

    /* Clipper: construct and release cleanly through the typed interface. */
    IDirectDrawClipper* clip_iface = nullptr;
    if (dd.CreateClipper(0, &clip_iface, nullptr) != 0 || !clip_iface) {
        std::fprintf(stderr, "FAIL: CreateClipper failed\n");
        return 1;
    }
    clip_iface->Release();

    std::puts("PASS: IDirectDrawPalette/IDirectDrawClipper lifecycle works through the typed interface");
    return 0;
}
