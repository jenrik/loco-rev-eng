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

    // physical_occupancy cell values, exactly grid_width*grid_height*grid_depth
    // entries, stored in file order (depth-major, then height, then width --
    // see parse_sprite_metadata). Empty when has_footprint is false or the
    // .dat's row count didn't match its own declared dims. Values observed in
    // the shipped archive are strictly 0/1 (verified against scenery/statue1.dat
    // and scenery/bigfount.dat). Access via physical_occupied(), not the raw
    // index formula below, so the storage order stays an implementation detail.
    std::vector<uint8_t> physical_occupancy_grid;

    // bitmap_occupancy's cell values are NOT parsed here: unlike
    // physical_occupancy, its rows carry non-binary values (0 through at least
    // 6, observed in scenery/bigfount.dat) whose semantics were not resolved
    // against the original consumer at the time this was written -- only the
    // bounding dims above are trustworthy.

    bool physical_occupied(int x, int y, int z) const {
        if (!has_footprint || x < 0 || x >= grid_width || y < 0 ||
            y >= grid_height || z < 0 || z >= grid_depth) {
            return false;
        }
        const size_t index = (static_cast<size_t>(z) * grid_height + y) *
                                  grid_width +
                              x;
        return index < physical_occupancy_grid.size() &&
               physical_occupancy_grid[index] != 0;
    }
};

// Track-connectivity classification, matching TileMapResource::state_63A
// (world/tilemap.h). Values and their originating keyword/direction pair are
// fully evidenced from decompiling FUN_0044b4f0 (vtable 0x478358 slot [3] --
// still no confirmed caller, but the keyword table itself doesn't depend on
// finding one). Confirmed against real archive data (track/*.dat) that the
// type keyword and its direction/orientation qualifier are space-separated
// tokens on one line, e.g. "tunnel right", "depot top", "levelcrossing
// road-x-v", or a lone keyword with no qualifier ("points", "switch",
// "crosstrack").
enum class TileTrackType : uint8_t {
    TunnelLeft = 1,
    TunnelRight = 2,
    TunnelTop = 3,
    TunnelBottom = 4,
    BridgeHorizontal = 5,
    BridgeVertical = 6,
    DepotLeft = 7,
    DepotRight = 8,
    DepotTop = 9,
    DepotBottom = 10,
    Points = 0xb,
    Switch = 0xc,
    CrossTrack = 0xd,
    LevelCrossingPathHorizontal = 0xe,
    LevelCrossingPathVertical = 0xf,
    LevelCrossingRoadHorizontal = 0x10,
    LevelCrossingRoadVertical = 0x11,
    StationHorizontal = 0x12,
    StationVertical = 0x13,
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

    // Track-connectivity type, parsed from one of the "tunnel"/"depot"/
    // "bridge"/"points"/"switch"/"crosstrack"/"levelcrossing"/"station"
    // directive lines (see TileTrackType). false for the majority of
    // resources (buildings, scenery, UI) that carry no such directive.
    bool has_tile_type = false;
    TileTrackType tile_type = TileTrackType::TunnelLeft;

    // Display name, parsed from the "Name" directive (ui/UI_ChildWindow.h's
    // ChildWindow::name, +0x14D). The original truncates to a 10-byte buffer
    // without guaranteeing null-termination if the source fills all 10 bytes;
    // this host copy keeps the same effective content (up to 9 characters)
    // without reproducing that non-termination quirk. Empty when no "Name"
    // directive is present.
    std::string name;

    // Easter-egg replay-delay terminator value, parsed from the
    // "EEReplayDelay" directive (input/BuildingDescriptorEditor.h's
    // BuildingDescriptorEditor::ee_replay_delay, +0x522), clamped to <= 5 by
    // the original parser (a negative value like the shipped archive's only
    // instance, "EEReplayDelay -1", wraps through uint8_t and clamps to 5 --
    // reproduced exactly, not "fixed"). Defaults to 0, matching the
    // original's unconditional zero-init when no directive is present.
    // core/BuildingMgrObjectGroup.cpp's ResourceGameObject reads this value
    // as its group member-count ceiling -- not from "MaxMinifigForResource"
    // as a naive reading of the .dat text might suggest.
    uint8_t ee_replay_delay = 0;

    // "LeisureDestination" directive value (input/BuildingDescriptorEditor.h's
    // BuildingDescriptorEditor::leisure_destination, +0x62C). INPUT_PlaceObject
    // (0x41DD80) reads this off the newly-placed entity's resource pointer
    // (nonzero test) to decide whether to increment InputMgr's special_count
    // counter alongside the general entity count. Defaults to 0, matching the
    // original's unconditional zero-init when no directive is present.
    uint8_t leisure_destination = 0;
};

class SpriteResource;
class SpriteBitmap;

SpriteBitmap* sprite_bitmap(SpriteResource* resource);
uint32_t sprite_width(const SpriteResource* resource);
uint32_t sprite_height(const SpriteResource* resource);
uint32_t sprite_resource_id(const SpriteResource* resource);
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

// True when `resource` is a genuine loco::assets::SpriteResource returned by
// this bridge (tag-checked; false for null or any other pointer). Host
// resources never carry the original x86 TileMapObject/TileMapResource
// layout (grid_width/occupancy_grid/... at fixed offsets like +0x168) that
// decompiled code built around real placed objects expects -- callers that
// would otherwise reinterpret_cast a resource pointer to read those offsets
// must check this first and reject instead of reading out-of-bounds garbage.
// See PROGRESS.md's "raw fixed-offset reads against undersized host
// resource objects" landmine item.
bool is_host_sprite_resource(const void* resource);

// Host source for the RESDATA+0x63A tile-state byte the RESDATA_Is*Tile
// family (world/tilemap.cpp, shared/stubs_impl.cpp) and RESDATA_GameVehicle
// (game/ResdataGameVehicle.cpp) read unconditionally off a resource pointer.
// Returns false (leaving *out_byte untouched) when `resource` isn't a host
// SpriteResource, or is one but carries no tile_type directive -- callers
// must treat that the same as "no category matches", not fall back to
// SpriteMetadata::tile_type's arbitrary default enumerator value.
bool sprite_tile_type_byte(const void* resource, uint8_t* out_byte);

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
