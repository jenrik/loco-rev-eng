// Status: VALIDATED
/**
 * mode2_menu_backdrop_test.cpp — EditWindow::render host-composition regression.
 *
 * Verifies the five resource loads and destinations recovered from
 * EditWindow_render (0x4216F0) reach the SDL primary target.
 */
#include "resource_manager_sdl3.h"
#include "sdl3_ddraw.h"
#include "sdl3_window.h"

#include <SDL3/SDL.h>
#include <cstdio>

namespace {
struct BackdropElement { uint32_t resource_id; int x; int y; };
// 0x4216F0 backdrop order, followed by the default-SP draw path at
// 0x421C9B/0x422010: Play, Exit, text, and the two option controls.
constexpr BackdropElement k_backdrop[] = {
    {0x413, 0x000, 0x000}, {0x444, 0x0F4, 0x1D6}, {0x445, 0x204, 0x0F9},
    {0x446, 0x11A, 0x0F0}, {0x443, 0x20B, 0x2A8},
    {0x407, 0x212, 0x1EA}, {0x40B, 0x387, 0x1BD}, {0x40F, 0x387, 0x231},
    {0x403, 0x387, 0x2A5}, {0x405, 0x18B, 0x2A5},
};

bool fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}
}  // namespace

int main() {
    if (SDL3_WindowInit("mode2-menu-backdrop-test", 800, 600) != 0) return fail(SDL_GetError()) ? 0 : 1;
    if (!ResourceManager_Init(nullptr) || !SDL3_ClearPrimarySurface(0x002850)) {
        SDL3_WindowQuit();
        return fail("could not initialize the host menu compositor") ? 0 : 1;
    }

    for (const BackdropElement& element : k_backdrop) {
        auto* resource = loco::assets::host_resource_manager().get_sprite_by_id(element.resource_id);
        auto* bitmap = loco::assets::sprite_bitmap(resource);
        if (!bitmap || !SDL3_BlitSurfaceToPrimary(loco::assets::bitmap_surface(bitmap), element.x, element.y)) {
            loco::assets::release_sprite(resource);
            loco::assets::host_resource_manager().reset();
            SDL3_WindowQuit();
            return fail("a recovered EditWindow backdrop blit failed") ? 0 : 1;
        }
        loco::assets::release_sprite(resource);
    }

    SDL_Renderer* renderer = SDL3_GetRenderer();
    if (!SDL3_PresentPrimarySurface() || !renderer) {
        loco::assets::host_resource_manager().reset();
        SDL3_WindowQuit();
        return fail("could not present the composed menu") ? 0 : 1;
    }
    SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
    bool changed = false;
    for (int y = 0; pixels && !changed && y < pixels->h; ++y) {
        for (int x = 0; x < pixels->w; ++x) {
            Uint8 r = 0, g = 0, b = 0, a = 0;
            if (SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a) &&
                (r != 0 || g != 0x28 || b != 0x50 || a != 0xff)) {
                changed = true;
                break;
            }
        }
    }
    if (pixels) SDL_DestroySurface(pixels);
    loco::assets::host_resource_manager().reset();
    SDL3_WindowQuit();
    if (!changed) return fail("all presented menu pixels retained the clear color") ? 0 : 1;
    std::puts("PASS: mode 2 menu backdrop elements reach the SDL primary surface");
    return 0;
}
