// Status: VALIDATED
/** sdl3_primary_present_test.cpp — primary surface → SDL window regression. */
#include "../src/sdl3_shims/sdl3_ddraw.h"
#include "../src/sdl3_shims/sdl3_window.h"

#include <SDL3/SDL.h>
#include <cstdio>

int main()
{
    if (SDL3_WindowInit("primary-present-test", 32, 24) != 0) {
        std::fprintf(stderr, "FAIL: SDL3_WindowInit: %s\n", SDL_GetError());
        return 1;
    }
    IDirectDrawSurface4* primary = SDL3_GetPrimarySurface();
    SDL_Renderer* renderer = SDL3_GetRenderer();
    if (!primary || !primary->texture || !renderer) {
        std::fprintf(stderr, "FAIL: primary render target was not initialized\n");
        SDL3_WindowQuit();
        return 1;
    }

    SDL_Surface* sprite = SDL_CreateSurface(2, 2, SDL_PIXELFORMAT_XRGB8888);
    const bool sprite_ready = sprite &&
        SDL_WriteSurfacePixel(sprite, 0, 0, 0xd0, 0x30, 0x20, 0xff) &&
        SDL_WriteSurfacePixel(sprite, 1, 0, 0xd0, 0x30, 0x20, 0xff) &&
        SDL_WriteSurfacePixel(sprite, 0, 1, 0xd0, 0x30, 0x20, 0xff) &&
        SDL_WriteSurfacePixel(sprite, 1, 1, 0xd0, 0x30, 0x20, 0xff) &&
        SDL3_ClearPrimarySurface(0x2468ac) &&
        SDL3_BlitSurfaceToPrimary(sprite, 5, 7);
    if (sprite) SDL_DestroySurface(sprite);
    if (!sprite_ready) {
        std::fprintf(stderr, "FAIL: could not compose source surface: %s\n", SDL_GetError());
        SDL3_WindowQuit();
        return 1;
    }
    if (!SDL3_PresentPrimarySurface()) {
        std::fprintf(stderr, "FAIL: primary surface was not presented: %s\n", SDL_GetError());
        SDL3_WindowQuit();
        return 1;
    }

    SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
    Uint8 red = 0, green = 0, blue = 0, alpha = 0;
    const bool rendered = pixels && SDL_ReadSurfacePixel(pixels, 5, 7, &red, &green, &blue, &alpha);
    if (pixels) SDL_DestroySurface(pixels);
    SDL3_WindowQuit();
    if (!rendered || red != 0xd0 || green != 0x30 || blue != 0x20 || alpha != 0xff) {
        std::fprintf(stderr, "FAIL: composited sprite pixel was not copied to the window (%u,%u,%u,%u)\n", red, green, blue, alpha);
        return 1;
    }
    std::puts("PASS: SDL primary surface is composited and presented");
    return 0;
}
