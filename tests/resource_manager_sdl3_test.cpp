// Status: VALIDATED
/** resource_manager_sdl3_test.cpp — translated ResourceManager sprite ABI test. */
#include "resource_manager_sdl3.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
bool fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}
uint16_t read_u16(const void* base, size_t offset) {
    const auto* bytes = static_cast<const uint8_t*>(base);
    return static_cast<uint16_t>(bytes[offset]) | (static_cast<uint16_t>(bytes[offset + 1]) << 8);
}
uint32_t read_u32(const void* base, size_t offset) {
    const auto* bytes = static_cast<const uint8_t*>(base);
    return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return fail(SDL_GetError()) ? 0 : 1;
    auto& manager = loco::assets::host_resource_manager();
    std::string error;
    if (!manager.initialize("lego-loco-unpacked", &error)) return fail(error.c_str()) ? 0 : 1;

    // Exact consumer protocol from EditWindow::initSprites (0x421500).
    void* resource = ResourceManager_GetById(static_cast<void*>(nullptr), 0x407);
    if (!resource || read_u16(resource, 0x14) != 155 || read_u16(resource, 0x16) != 130) {
        return fail("resource 0x407 did not expose original sprite dimensions") ? 0 : 1;
    }
    auto** vtable = *reinterpret_cast<void***>(resource);
    using GetBitmap = void* (*)(void*, int, int);
    void* bitmap = reinterpret_cast<GetBitmap>(vtable[4])(resource, 0, 0);
    if (!bitmap || read_u32(bitmap, 0x08) != 155 || read_u32(bitmap, 0x0c) != 130) {
        return fail("vtable[4] bitmap ABI does not match EditWindow::render") ? 0 : 1;
    }
    if (ResourceManager_GetById(static_cast<void**>(nullptr), 0x407) != resource) {
        return fail("ResourceManager did not cache resource 0x407") ? 0 : 1;
    }
    using Release = void (*)(void*);
    reinterpret_cast<Release>(vtable[8])(resource);

    SDL_Surface* surface = ResourceManager_GetSpriteSurface(resource);
    Uint32 color_key = 0xffffffffu;
    if (!surface || !SDL_GetSurfaceColorKey(surface, &color_key) || color_key != 0) {
        return fail("magenta palette index was not applied as the source color key") ? 0 : 1;
    }

    void* loading = ResourceManager_GetById(static_cast<void*>(nullptr), 0x402);
    const loco::assets::SpriteMetadata* metadata = ResourceManager_GetSpriteMetadata(loading);
    if (!metadata || !metadata->is_button || metadata->total_frames != 9 ||
        metadata->frame_set_count != 6 || metadata->frame_sets.size() != 6 ||
        metadata->frame_sets[1].name != "complete" || metadata->frame_sets[1].sound_resource_id != 22301) {
        return fail("startup/loading.dat animation metadata was not parsed") ? 0 : 1;
    }

    const loco::assets::AssetBlob* sound = manager.load_asset_by_id(0x5015);
    if (!sound || sound->type != loco::assets::AssetType::Wave || sound->bytes.size() < 12 ||
        std::string(sound->bytes.begin(), sound->bytes.begin() + 4) != "RIFF" ||
        std::string(sound->bytes.begin() + 8, sound->bytes.begin() + 12) != "WAVE") {
        return fail("sound resource 0x5015 did not load as a RIFF WAVE asset") ? 0 : 1;
    }
    const loco::assets::AssetBlob* button = manager.load_asset_by_id(0x341c, loco::assets::AssetType::Button);
    if (!button || button->type != loco::assets::AssetType::Button || button->path != "roads\\half-hwint.but" ||
        button->bytes.empty()) {
        return fail("resource 0x341c did not load its archived .but descriptor") ? 0 : 1;
    }
    const loco::assets::AssetBlob* cursor = manager.load_asset_by_id(0x414, loco::assets::AssetType::AnimatedCursor);
    if (!cursor || cursor->type != loco::assets::AssetType::AnimatedCursor || cursor->bytes.size() < 12 ||
        std::string(cursor->bytes.begin(), cursor->bytes.begin() + 4) != "RIFF" ||
        std::string(cursor->bytes.begin() + 8, cursor->bytes.begin() + 12) != "ACON") {
        return fail("resource 0x414 did not load its archived animated cursor") ? 0 : 1;
    }

    // roads\half-vwint.dat carries a real "physical_occupancy"/"bitmap_occupancy"
    // tile-placement footprint (verified by extracting resource.RFD directly):
    //   physical_occupancy\n\n2 1 1 \n\n1 1 ...\nbitmap_occupancy\n\n2 1 \n\n1 1 ...
    void* road = ResourceManager_GetById(static_cast<void*>(nullptr), 0x341d);
    const loco::assets::SpriteMetadata* road_metadata = ResourceManager_GetSpriteMetadata(road);
    if (!road_metadata || !road_metadata->footprint.has_footprint ||
        road_metadata->footprint.grid_width != 2 || road_metadata->footprint.grid_height != 1 ||
        road_metadata->footprint.grid_depth != 1 || road_metadata->footprint.bitmap_grid_width != 2 ||
        road_metadata->footprint.bitmap_grid_height != 1) {
        return fail("resource 0x341d's physical_occupancy/bitmap_occupancy footprint was not parsed") ? 0 : 1;
    }

    manager.reset();
    SDL_Quit();
    std::puts("PASS: ResourceManager loads real BMP, DAT/color key, WAVE, BUT, and ANI archive assets");
    return 0;
}
