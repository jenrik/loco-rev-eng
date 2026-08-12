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

    // scenery\statue1.dat's physical_occupancy is a non-uniform 3x3x3 grid
    // (verified by extracting resource.RFD directly), chosen specifically
    // because its asymmetric pattern can catch an x/y transposition that a
    // uniform or 1x1x1 fixture (like 0x341d above) cannot:
    //   z=0: fully occupied (all nine cells 1)
    //   z=1: only (x=1,y=1) occupied
    //   z=2: (x=1,y=1) and (x=2,y=1) occupied -- asymmetric across x
    void* statue = ResourceManager_GetById(static_cast<void*>(nullptr), 0x1012);
    const loco::assets::SpriteMetadata* statue_metadata = ResourceManager_GetSpriteMetadata(statue);
    if (!statue_metadata || !statue_metadata->footprint.has_footprint ||
        statue_metadata->footprint.grid_width != 3 || statue_metadata->footprint.grid_height != 3 ||
        statue_metadata->footprint.grid_depth != 3 ||
        statue_metadata->footprint.physical_occupancy_grid.size() != 27) {
        return fail("resource 0x1012's physical_occupancy dims/row count were not parsed") ? 0 : 1;
    }
    const loco::assets::SpriteFootprint& statue_grid = statue_metadata->footprint;
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            if (!statue_grid.physical_occupied(x, y, 0)) {
                return fail("resource 0x1012's z=0 physical_occupancy layer was not fully occupied") ? 0 : 1;
            }
        }
    }
    if (!statue_grid.physical_occupied(1, 1, 1) || statue_grid.physical_occupied(0, 1, 1) ||
        statue_grid.physical_occupied(1, 0, 1) || statue_grid.physical_occupied(2, 1, 1)) {
        return fail("resource 0x1012's z=1 physical_occupancy layer did not match the single occupied cell") ? 0 : 1;
    }
    if (!statue_grid.physical_occupied(1, 1, 2) || !statue_grid.physical_occupied(2, 1, 2) ||
        statue_grid.physical_occupied(0, 1, 2) || statue_grid.physical_occupied(1, 2, 2)) {
        return fail("resource 0x1012's z=2 physical_occupancy layer did not match its asymmetric pattern "
                    "(possible x/y transposition)") ? 0 : 1;
    }

    // track\pnt-ws.dat's last directive line is the standalone keyword
    // "points" (no direction qualifier, verified by extracting resource.RFD
    // directly) -> TileTrackType::Points, matching TileMapResource::state_63A's
    // value table (FUN_0044b4f0).
    void* points = ResourceManager_GetById(static_cast<void*>(nullptr), 0xc0a);
    const loco::assets::SpriteMetadata* points_metadata = ResourceManager_GetSpriteMetadata(points);
    if (!points_metadata || !points_metadata->has_tile_type ||
        points_metadata->tile_type != loco::assets::TileTrackType::Points) {
        return fail("resource 0xc0a's standalone \"points\" tile_type was not parsed") ? 0 : 1;
    }

    // track\Depot-n.dat carries "depot top" (type keyword + direction
    // qualifier on the same line, verified by extracting resource.RFD
    // directly) -> TileTrackType::DepotTop == 9, not the sequential-looking
    // 7 a naive left/right/top/bottom ordering guess would produce.
    void* depot = ResourceManager_GetById(static_cast<void*>(nullptr), 0xc54);
    const loco::assets::SpriteMetadata* depot_metadata = ResourceManager_GetSpriteMetadata(depot);
    if (!depot_metadata || !depot_metadata->has_tile_type ||
        depot_metadata->tile_type != loco::assets::TileTrackType::DepotTop) {
        return fail("resource 0xc54's \"depot top\" tile_type was not parsed") ? 0 : 1;
    }

    // building\factory1.dat carries "Name Factory" (verified by extracting
    // resource.RFD directly) -> ChildWindow::name's host equivalent.
    void* factory = ResourceManager_GetById(static_cast<void*>(nullptr), 0x816);
    const loco::assets::SpriteMetadata* factory_metadata = ResourceManager_GetSpriteMetadata(factory);
    if (!factory_metadata || factory_metadata->name != "Factory") {
        return fail("resource 0x816's \"Name Factory\" directive was not parsed") ? 0 : 1;
    }

    // building\launcher.dat is the only shipped resource carrying an
    // "EEReplayDelay" directive, with value -1 (verified by extracting
    // resource.RFD directly) -- the parser's uint8_t wrap-then-clamp must
    // turn that into 5, not -1 or 255.
    void* launcher = ResourceManager_GetById(static_cast<void*>(nullptr), 0x852);
    const loco::assets::SpriteMetadata* launcher_metadata = ResourceManager_GetSpriteMetadata(launcher);
    if (!launcher_metadata || launcher_metadata->ee_replay_delay != 5) {
        return fail("resource 0x852's \"EEReplayDelay -1\" did not clamp to 5") ? 0 : 1;
    }

    // scenery\bigfount.dat's "LeisureDestination 1" line (the same directive
    // that motivated physical_occupancy's row-counted parsing above, since it
    // interleaves before that resource's "bitmap_occupancy" header) should
    // now itself parse correctly as leisure_destination == 1.
    void* bigfount = ResourceManager_GetById(static_cast<void*>(nullptr), 0x1020);
    const loco::assets::SpriteMetadata* bigfount_metadata = ResourceManager_GetSpriteMetadata(bigfount);
    if (!bigfount_metadata || bigfount_metadata->leisure_destination != 1) {
        return fail("resource 0x1020's \"LeisureDestination 1\" directive was not parsed") ? 0 : 1;
    }

    // The same resource's "cursor/default_frame_set 12 0" line (verified by
    // extracting resource.RFD directly) uses the slash-spelling variant of
    // this directive, not the far more common plain "cursor_frame_set" --
    // the parser must recognize both (198 vs 379 real files respectively;
    // a real, previously-unnoticed gap, since the slash variant was not
    // recognized at all before this fix). Non-zero cursor_frame_set makes
    // this a meaningful check, unlike a resource that happens to default to 0.
    if (bigfount_metadata->cursor_frame_set != 12 || bigfount_metadata->cursor_frame != 0) {
        return fail("resource 0x1020's \"cursor/default_frame_set 12 0\" directive was not parsed") ? 0 : 1;
    }

    manager.reset();
    SDL_Quit();
    std::puts("PASS: ResourceManager loads real BMP, DAT/color key, WAVE, BUT, and ANI archive assets");
    return 0;
}
