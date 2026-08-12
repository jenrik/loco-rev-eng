// Status: VALIDATED
/**
 * resource_manager_sdl3.h — Host bridge for ResourceManager asset lookups
 *
 * ResourceManager_Init (0x446050) loads strings before resource IDs are
 * dereferenced. This bridge reads those same PE RT_STRING values, resolves the
 * matching RFH/RFD asset, and supplies objects compatible with the observed
 * EditWindow resource calls at 0x421500/0x4216F0.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifndef _WIN32

struct SDL_Surface;

namespace loco::assets {

enum class AssetType {
    Bitmap,
    Metadata,
    Wave,
    Button,
    AnimatedCursor,
    Layout,
    Save,
    Ini,
    Unknown,
};

struct AssetBlob {
    AssetType type = AssetType::Unknown;
    std::string path;
    std::vector<uint8_t> bytes;
};

// DAT animation-row layout, verified from startup/loading.dat comments.
struct AnimationFrameSet {
    std::string name;
    int start_frame = 0;
    int end_frame = 0;
    int frame_delay = 0;
    int split_frames = 0;
    int restart_delay = 0;
    int next_frame_set = 0;
    int sound_resource_id = 0;
    int replay_delay = 0;
    int flip_x = 0;
};

// Tile-placement footprint, parsed from the .dat "physical_occupancy" /
// "bitmap_occupancy" sections (BuildingDescriptorEditor's border_width/
// border_height/border_depth and bitmap_occupancy_width/height at the
// original x86 +0x168../+0x16C offsets — see input/BuildingDescriptorEditor.h
// and world/tilemap.h's TileMapResource::grid_width/grid_height/grid_depth/
// grid_span_y/original_span, the same layout convention). has_footprint is
// false for the majority of resources (sprites, buttons, cursors) that carry
// no occupancy sections at all.
struct SpriteFootprint {
    bool has_footprint = false;
    int grid_width = 0;         // physical_occupancy width
    int grid_height = 0;        // physical_occupancy height
    int grid_depth = 0;         // physical_occupancy depth
    int bitmap_grid_width = 0;  // bitmap_occupancy width
    int bitmap_grid_height = 0; // bitmap_occupancy height
};

struct SpriteMetadata {
    bool is_button = false;
    int offset_x = 0;
    int offset_y = 0;
    int offset_z = 0;
    int total_frames = 0;
    int frame_set_count = 0;
    int cursor_frame_set = 0;
    int cursor_frame = 0;
    std::vector<AnimationFrameSet> frame_sets;
    SpriteFootprint footprint;
};

class SpriteResource;
class SpriteBitmap;

SpriteBitmap* sprite_bitmap(SpriteResource* resource);
uint32_t sprite_width(const SpriteResource* resource);
uint32_t sprite_height(const SpriteResource* resource);
uint32_t bitmap_width(const SpriteBitmap* bitmap);
uint32_t bitmap_height(const SpriteBitmap* bitmap);
SDL_Surface* bitmap_surface(const SpriteBitmap* bitmap);
void release_sprite(SpriteResource* resource);

class ResourceManagerSdl3 {
public:
    bool initialize(const std::string& game_root, std::string* error = nullptr);
    void reset();

    // Typed host-side API. The void* wrapper remains only for untranslated
    // binary-facing callers elsewhere in the decompilation.
    SpriteResource* get_sprite_by_id(uint32_t resource_id);
    void* get_by_id(uint32_t resource_id);
    const AssetBlob* load_asset_by_id(uint32_t resource_id);
    const AssetBlob* load_asset_by_id(uint32_t resource_id, AssetType type);
    const SpriteMetadata* sprite_metadata(const SpriteResource* resource) const;
    SDL_Surface* sprite_surface(const SpriteResource* resource) const;
    const std::string& last_error() const { return last_error_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;
    std::string last_error_;
};

ResourceManagerSdl3& host_resource_manager();

// Narrow host-compositor bridge. These free functions let translated UI files
// use archive-backed sprites without importing the binary-facing C wrappers.
SpriteResource* host_get_sprite_by_id(uint32_t resource_id);

}  // namespace loco::assets

// C++ linkage is intentional: existing translated callers use overloads.
int ResourceManager_Init(void* ignored_original_manager);
void* ResourceManager_GetById(void* ignored_original_manager, int32_t resource_id);
void* ResourceManager_GetById(void* ignored_original_manager, uint32_t resource_id);
void* ResourceManager_GetById(void** ignored_original_manager, int32_t resource_id);
void* ResourceManager_GetById(void** ignored_original_manager, uint32_t resource_id);
SDL_Surface* ResourceManager_GetSpriteSurface(void* resource);
const loco::assets::SpriteMetadata* ResourceManager_GetSpriteMetadata(void* resource);

#endif /* _WIN32 */
