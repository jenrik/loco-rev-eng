// Status: VALIDATED
/** sdl3_primary_present_test.cpp — logical canvas → SDL display regression. */
#include "sdl3_ddraw.h"
#include "sdl3_window.h"

#include <SDL3/SDL.h>
#include <cstdio>

int main()
{
    if (SDL3_WindowInit("primary-present-test", 800, 600) != 0) {
        std::fprintf(stderr, "FAIL: SDL3_WindowInit: %s\n", SDL_GetError());
        return 1;
    }
    Sdl3DirectDrawSurface* primary = SDL3_GetPrimarySurface();
    SDL_Renderer* renderer = SDL3_GetRenderer();
    if (!primary || !primary->texture || !renderer ||
        primary->width != SDL3_PRIMARY_CANVAS_WIDTH ||
        primary->height != SDL3_PRIMARY_CANVAS_HEIGHT) {
        std::fprintf(stderr, "FAIL: fixed logical canvas was not initialized\n");
        SDL3_WindowQuit();
        return 1;
    }

    SDL_Surface* sprite = SDL_CreateSurface(160, 160, SDL_PIXELFORMAT_XRGB8888);
    bool sprite_ready = sprite != nullptr;
    for (int y = 0; sprite_ready && y < 160; ++y) {
        for (int x = 0; x < 160; ++x) {
            sprite_ready = SDL_WriteSurfacePixel(sprite, x, y, 0xd0, 0x30, 0x20, 0xff);
            if (!sprite_ready) break;
        }
    }
    sprite_ready = sprite_ready && SDL3_ClearPrimarySurface(0x2468ac) &&
        SDL3_BlitSurfaceToPrimary(sprite, 560, 440) &&
        // EditWindow_HandleClick @0x4214BA sets this recovered logical rect.
        SDL3_DrawPrimaryTextInput(0x232, 0x2CC, 0x34D, 0x2ED, "LEGO", true);
    if (sprite) SDL_DestroySurface(sprite);
    if (!sprite_ready || !SDL3_PresentPrimarySurface()) {
        std::fprintf(stderr, "FAIL: could not compose/present logical canvas: %s\n", SDL_GetError());
        SDL3_WindowQuit();
        return 1;
    }

    float canvas_x = 0.0f;
    float canvas_y = 0.0f;
    const bool center_maps = SDL3_DisplayToPrimaryCanvas(400.0f, 300.0f, &canvas_x, &canvas_y) &&
        canvas_x > 639.0f && canvas_x < 641.0f && canvas_y > 511.0f && canvas_y < 513.0f;
    const bool pillarbox_rejected = !SDL3_DisplayToPrimaryCanvas(0.0f, 300.0f, &canvas_x, &canvas_y);

    SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
    bool rendered = false;
    bool input_rendered = false;
    for (int y = 0; pixels && (!rendered || !input_rendered) && y < pixels->h; ++y) {
        for (int x = 0; x < pixels->w; ++x) {
            Uint8 red = 0, green = 0, blue = 0, alpha = 0;
            if (!SDL_ReadSurfacePixel(pixels, x, y, &red, &green, &blue, &alpha)) continue;
            if (red == 0xd0 && green == 0x30 && blue == 0x20 && alpha == 0xff) rendered = true;
            if (red == 0x3c && green == 0x3c && blue == 0x3c && alpha == 0xff) input_rendered = true;
        }
    }
    if (pixels) SDL_DestroySurface(pixels);
    SDL3_WindowQuit();
    if (!rendered || !input_rendered || !center_maps || !pillarbox_rejected) {
        std::fprintf(stderr, "FAIL: logical canvas, input field, presentation, or pointer mapping failed\n");
        return 1;
    }
    std::puts("PASS: fixed primary canvas, input field, display scaling, and pointer mapping work");
    return 0;
}
