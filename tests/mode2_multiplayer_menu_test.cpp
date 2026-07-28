// Status: VALIDATED
/**
 * mode2_multiplayer_menu_test.cpp — GameSetupPanel SDL compositor regression.
 *
 * Verifies archive-backed multiplayer artwork: apback (0x439), frame 0 of
 * Exit/Search/Options (0x42C/0x429/0x42B), and frame 1 of every 492x123
 * aplayer resource (0x43A..0x442). Coordinates derive from show (0x408F70)
 * and the crop/increments from drawGrid (0x409980).
 */
#include "../src/sdl3_shims/resource_manager_sdl3.h"
#include "../src/sdl3_shims/sdl3_ddraw.h"
#include "../src/sdl3_shims/sdl3_window.h"

#include <SDL3/SDL.h>
#include <cstdio>

namespace {
constexpr int kGridLeft = (SDL3_PRIMARY_CANVAS_WIDTH - 800) / 2 + 0x1B;
constexpr int kGridTop = (SDL3_PRIMARY_CANVAS_HEIGHT - 600) / 2 + 0x27;
constexpr int kExitLeft = (SDL3_PRIMARY_CANVAS_WIDTH - 800) / 2 + 800 - 208;
constexpr int kExitTop = kGridTop + 0x1C0;
constexpr int kOptionsLeft = kGridLeft + 0x20C;
constexpr int kOptionsTop = kGridTop + 0x162;

bool fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}

bool blit(uint32_t resource_id, int x, int y, int frame = -1, int width = 0, int height = 0) {
    auto* resource = loco::assets::host_resource_manager().get_sprite_by_id(resource_id);
    auto* bitmap = loco::assets::sprite_bitmap(resource);
    SDL_Surface* surface = loco::assets::bitmap_surface(bitmap);
    bool rendered = false;
    if (frame < 0) {
        rendered = surface && SDL3_BlitSurfaceToPrimary(surface, x, y);
    } else {
        const SDL_Rect source = {frame * width, 0, width, height};
        rendered = surface && SDL3_BlitSurfaceRectToPrimary(surface, source, x, y);
    }
    loco::assets::release_sprite(resource);
    return rendered;
}
}  // namespace

int main() {
    if (SDL3_WindowInit("mode2-multiplayer-menu-test", 800, 600) != 0) return fail(SDL_GetError()) ? 0 : 1;
    if (!ResourceManager_Init(nullptr) || !SDL3_ClearPrimarySurface(0x003050)) {
        SDL3_WindowQuit();
        return fail("could not initialize the multiplayer compositor") ? 0 : 1;
    }
    if (!blit(0x439, 0, 0) ||
        !blit(0x42C, kExitLeft, kExitTop, 0, 144, 112) ||
        !blit(0x429, kOptionsLeft + 79, kOptionsTop, 0, 72, 72) ||
        !blit(0x42B, kOptionsLeft, kOptionsTop, 0, 72, 72)) {
        loco::assets::host_resource_manager().reset();
        SDL3_WindowQuit();
        return fail("could not blit recovered multiplayer background controls") ? 0 : 1;
    }
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            if (!blit(0x43A + row * 3 + column, kGridLeft + column * 0xA5,
                      kGridTop + row * 0x7C, 1, 0xA4, 0x7B)) {
                loco::assets::host_resource_manager().reset();
                SDL3_WindowQuit();
                return fail("could not blit an original multiplayer player-slot frame") ? 0 : 1;
            }
        }
    }
    SDL_Renderer* renderer = SDL3_GetRenderer();
    SDL_Surface* pixels = SDL3_PresentPrimarySurface() && renderer ? SDL_RenderReadPixels(renderer, nullptr) : nullptr;
    bool changed = false;
    for (int y = 0; pixels && !changed && y < pixels->h; ++y) {
        for (int x = 0; x < pixels->w; ++x) {
            Uint8 r = 0, g = 0, b = 0, a = 0;
            if (SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a) &&
                (r != 0 || g != 0x30 || b != 0x50 || a != 0xff)) {
                changed = true;
                break;
            }
        }
    }
    if (pixels) SDL_DestroySurface(pixels);
    loco::assets::host_resource_manager().reset();
    SDL3_WindowQuit();
    if (!changed) return fail("multiplayer compositor retained only its fallback clear color") ? 0 : 1;
    std::puts("PASS: multiplayer menu uses original background, controls, and player-slot sprites");
    return 0;
}
