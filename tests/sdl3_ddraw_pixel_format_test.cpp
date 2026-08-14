// Status: VALIDATED
/** sdl3_ddraw_pixel_format_test.cpp — Lock()/Unlock() 16bpp adapter regression.
 *
 * Confirms Sdl3DirectDrawSurface::Lock() hands back a genuine RGB565
 * (16bpp) buffer — pitch == width*2, not width*4 — and that a raw
 * uint16_t pixel write through that pointer round-trips through
 * Unlock()+Lock() unchanged. This is exactly the access pattern
 * native/DDRAW_DimSurfaceRect.c and town/TownTiles.cpp's
 * Town_CheckOccupiedEx use; see PROGRESS.md's DirectDraw-shim
 * pixel-format note for why 32bpp XRGB8888 would have silently
 * corrupted their output instead.
 */
#include "sdl3_ddraw.h"
#include "sdl3_window.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>

extern int32_t g_surface_bpp;
extern int32_t g_surface_bshift;
extern int32_t g_surface_channel1;
extern int32_t g_surface_channel2;
extern int32_t g_surface_red_mask;
extern int32_t g_surface_blue_mask;

int main()
{
    if (SDL3_WindowInit("ddraw-pixel-format-test", 800, 600) != 0) {
        std::fprintf(stderr, "FAIL: SDL3_WindowInit: %s\n", SDL_GetError());
        return 1;
    }

    Sdl3DirectDrawSurface* primary = SDL3_GetPrimarySurface();
    if (!primary) {
        std::fprintf(stderr, "FAIL: no primary surface\n");
        SDL3_WindowQuit();
        return 1;
    }

    /* SDL3_EnsurePrimarySurface should have set the RGB565 constants for
     * real (matching DDRAW_GetSurface's own decompiled 565 branch). */
    if (g_surface_bpp != 0x235 || g_surface_channel1 != 0xb ||
        g_surface_channel2 != 6 || g_surface_bshift != 0x7bef ||
        g_surface_red_mask != 0xF800 || g_surface_blue_mask != 0x001F) {
        std::fprintf(stderr,
            "FAIL: pixel-format globals not set to RGB565 constants "
            "(bpp=%#x ch1=%#x ch2=%#x bshift=%#x red=%#x blue=%#x)\n",
            g_surface_bpp, g_surface_channel1, g_surface_channel2,
            g_surface_bshift, g_surface_red_mask, g_surface_blue_mask);
        SDL3_WindowQuit();
        return 1;
    }

    DDSURFACEDESC desc{};
    if (primary->Lock(nullptr, &desc, 0, nullptr) != 0 || !desc.lpSurface) {
        std::fprintf(stderr, "FAIL: Lock failed\n");
        SDL3_WindowQuit();
        return 1;
    }

    /* Real 16bpp stride is width*2 bytes, not width*4 (XRGB8888). */
    if (desc.lPitch != static_cast<int32_t>(desc.dwWidth) * 2) {
        std::fprintf(stderr,
            "FAIL: lPitch (%d) is not a 16bpp stride for width %u\n",
            desc.lPitch, desc.dwWidth);
        primary->Unlock(nullptr);
        SDL3_WindowQuit();
        return 1;
    }

    /* Write a distinctive RGB565 pixel (pure green: bits 5-10) at (2, 1),
     * exactly how DDRAW_DimSurfaceRect/Town_CheckOccupiedEx address pixels
     * — uint16_t*, pitch in bytes / 2 for the row stride. */
    const uint16_t kTestPixel = 0x07E0;
    uint16_t* row1 = reinterpret_cast<uint16_t*>(
        static_cast<uint8_t*>(desc.lpSurface) + desc.lPitch);
    row1[2] = kTestPixel;

    if (primary->Unlock(nullptr) != 0) {
        std::fprintf(stderr, "FAIL: Unlock failed\n");
        SDL3_WindowQuit();
        return 1;
    }

    /* Re-lock and confirm the write survived the 565->8888->565 round trip
     * (Unlock converts up to the GPU texture's XRGB8888, Lock reads it back
     * down to RGB565 again). */
    DDSURFACEDESC desc2{};
    if (primary->Lock(nullptr, &desc2, 0, nullptr) != 0 || !desc2.lpSurface) {
        std::fprintf(stderr, "FAIL: second Lock failed\n");
        SDL3_WindowQuit();
        return 1;
    }
    uint16_t* row1_again = reinterpret_cast<uint16_t*>(
        static_cast<uint8_t*>(desc2.lpSurface) + desc2.lPitch);
    uint16_t readback = row1_again[2];
    primary->Unlock(nullptr);
    SDL3_WindowQuit();

    if (readback != kTestPixel) {
        std::fprintf(stderr,
            "FAIL: pixel did not round-trip (wrote %#06x, read back %#06x)\n",
            kTestPixel, readback);
        return 1;
    }

    std::puts("PASS: Lock()/Unlock() hand back a real 16bpp RGB565 buffer "
               "that round-trips correctly");
    return 0;
}
