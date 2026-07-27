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

    SDL_SetRenderTarget(renderer, primary->texture);
    SDL_SetRenderDrawColor(renderer, 0x24, 0x68, 0xac, 0xff);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);
    if (!SDL3_PresentPrimarySurface()) {
        std::fprintf(stderr, "FAIL: primary surface was not presented: %s\n", SDL_GetError());
        SDL3_WindowQuit();
        return 1;
    }

    SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
    Uint8 red = 0, green = 0, blue = 0, alpha = 0;
    const bool rendered = pixels && SDL_ReadSurfacePixel(pixels, 0, 0, &red, &green, &blue, &alpha);
    if (pixels) SDL_DestroySurface(pixels);
    SDL3_WindowQuit();
    if (!rendered || red != 0x24 || green != 0x68 || blue != 0xac || alpha != 0xff) {
        std::fprintf(stderr, "FAIL: primary pixel was not copied to the window (%u,%u,%u,%u)\n", red, green, blue, alpha);
        return 1;
    }
    std::puts("PASS: SDL primary surface is composited and presented");
    return 0;
}
