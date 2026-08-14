// Status: VALIDATED
/** sdl3_ddraw_surfacedesc_test.cpp — DDSURFACEDESC field-order regression.
 *
 * Guards against the exact landmine this struct already had once: dwWidth
 * and dwHeight silently transposed (platform/sdl3_types.h used to declare
 * dwWidth before dwHeight, the opposite of the real DirectX 6 SDK order).
 * Uses a non-square surface — a square one can't distinguish a swap.
 */
#include "sdl3_ddraw.h"

#include <cstdio>

int main()
{
    Sdl3DirectDrawSurface surface;
    surface.width = 320;
    surface.height = 200;

    DDSURFACEDESC got;
    if (surface.GetSurfaceDesc(&got) != 0) {
        std::fprintf(stderr, "FAIL: GetSurfaceDesc failed\n");
        return 1;
    }

    bool ok = (got.dwWidth == 320 && got.dwHeight == 200);
    if (!ok) {
        std::fprintf(stderr,
            "FAIL: GetSurfaceDesc returned dwWidth=%u dwHeight=%u, expected 320x200 "
            "(width/height transposed?)\n",
            got.dwWidth, got.dwHeight);
        return 1;
    }

    std::puts("PASS: DDSURFACEDESC dwWidth/dwHeight field order is correct");
    return 0;
}
