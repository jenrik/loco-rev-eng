// Status: VALIDATED
/**
 * menu_sprite_viewer.cpp — Render the original Lego Loco startup artwork
 *
 * This diagnostic composes the default single-player menu path from
 * EditWindow::render (0x4216F0), EditWindow::drawButtons (0x422010), and
 * fixed button rectangles in EditWindow::HandleClick (0x421200). It uses the
 * production ResourceManager SDL bridge, including DAT metadata and magenta
 * source color-key handling; no raw ResourceManager ABI calls remain here.
 */
#include "../src/sdl3_shims/resource_manager_sdl3.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {
struct SpritePlacement { uint32_t resource_id; float x; float y; };

bool load_texture(SDL_Renderer* renderer, uint32_t resource_id, SDL_Texture** texture) {
    auto* resource = loco::assets::host_resource_manager().get_sprite_by_id(resource_id);
    auto* bitmap = loco::assets::sprite_bitmap(resource);
    SDL_Surface* surface = loco::assets::bitmap_surface(bitmap);
    if (!surface) {
        std::fprintf(stderr, "ResourceManager could not load sprite %#x\n", resource_id);
        return false;
    }
    *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!*texture) {
        std::fprintf(stderr, "Resource %#x: SDL_CreateTextureFromSurface failed: %s\n", resource_id, SDL_GetError());
        return false;
    }
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    int frame_limit = -1;
    if (argc == 3 && std::string(argv[1]) == "--frames") frame_limit = std::atoi(argv[2]);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (!ResourceManager_Init(nullptr)) {
        std::fprintf(stderr, "ResourceManager initialization failed: %s\n", loco::assets::host_resource_manager().last_error().c_str());
        SDL_Quit();
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Lego Loco: real startup sprites", 1280, 1024, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "SDL renderer setup failed: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        loco::assets::host_resource_manager().reset();
        SDL_Quit();
        return 1;
    }

    // Exact assets and positions from 0x4216F0 plus default SP controls from 0x422010/0x421200.
    const SpritePlacement placements[] = {
        {0x413, 0, 0}, {0x444, 244, 470}, {0x445, 516, 249}, {0x446, 282, 240},
        {0x443, 523, 680}, {0x407, 530, 490}, {0x40b, 903, 445},
    };
    std::vector<std::pair<SDL_Texture*, SpritePlacement>> textures;
    for (const SpritePlacement& placement : placements) {
        SDL_Texture* texture = nullptr;
        if (!load_texture(renderer, placement.resource_id, &texture)) {
            for (const auto& item : textures) SDL_DestroyTexture(item.first);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            loco::assets::host_resource_manager().reset();
            SDL_Quit();
            return 1;
        }
        textures.emplace_back(texture, placement);
    }

    bool running = true;
    for (int frames = 0; running && (frame_limit < 0 || frames < frame_limit); ++frames) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) running = false;
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        for (const auto& item : textures) {
            float width = 0, height = 0;
            SDL_GetTextureSize(item.first, &width, &height);
            const SDL_FRect destination = {item.second.x, item.second.y, width, height};
            SDL_RenderTexture(renderer, item.first, nullptr, &destination);
        }
        SDL_RenderPresent(renderer);
        if (frame_limit < 0) SDL_Delay(16);
    }
    for (const auto& item : textures) SDL_DestroyTexture(item.first);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    loco::assets::host_resource_manager().reset();
    SDL_Quit();
    return 0;
}
