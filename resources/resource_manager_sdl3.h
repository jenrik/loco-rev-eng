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
    // The 4th numeric .dat token. Original writes this same token
    // (truncated to a byte) to FrameData::is_connected (+0x17,
    // shared/types.h) -- confirmed via disassembly of both the writer
    // (UI_ChildWindow_Render, 0x42528B -- confirmed by direct disassembly:
    // the 4th numeric token is extracted into a stack temp at 0x42527F
    // then stored truncated via `MOV byte [rec+0x17], CL` at 0x42528B,
    // the 5th distinct numeric-field write in the record, following
    // start_frame/end_frame/frame_delay at +0x00/+0x02/+0x04) and the
    // two readers
    // (TileMap::ProcessRect 0x4569AF, Entity::Update 0x405CCE).
    bool is_connected = false;
    int restart_delay = 0;
    int next_frame_set = 0;
    int sound_resource_id = 0;
    int replay_delay = 0;
    // The 9th numeric .dat token. Disassembly confirms the original writes
    // this token directly (no truncation) to FrameData::volume (+0x14,
    // shared/types.h): `LEA EDX,[rec+0x14]` at 0x4252CE followed by the
    // extractor call at 0x4252D5 (UI_ChildWindow_Render, 0x424E00).
    // Previously misnamed `flip_x` here -- a stale guess, not evidenced;
    // corrected 2026-08-14 while sourcing FrameData::flip_horizontal below
    // (see PROGRESS.md's 2026-08-14 "priority3-blit-adapter" entry).
    int volume = 0;
    // The 10th (last) numeric .dat token. Disassembly confirms the
    // original writes this token, truncated to a byte via the same
    // stack-temp idiom is_connected uses above, to FrameData::
    // flip_horizontal (+0x16, shared/types.h): `LEA EAX,[ESP+0x12]` +
    // extractor call at 0x4252DA/0x4252E1, then
    // `MOV byte ptr [rec+0x16],DL` at 0x4252ED. Previously discarded here
    // as an unnamed "opaque_field" that only validated row shape -- that
    // was wrong; it is a real, used field.
    bool flip_horizontal = false;
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

    // bitmap_occupancy cell values, exactly bitmap_grid_width*bitmap_grid_height
    // entries, row-major (row 0 first, bitmap_grid_width entries per row).
    // Unlike physical_occupancy, values are not booleans -- they are 1-based
    // layer indices into the tile's ORIGIN region (TileMap::FindObject,
    // 0x4550C0: value-1 = layer, 0 = cell not part of this span). Verified
    // against the shipped resource.RFD directly (each bitmap_occupancy
    // section is one "W H" dims line followed by exactly H rows of W
    // space-separated tokens, values 0 through at least 6 observed) before
    // writing this parser, matching the same row/line-per-row shape
    // physical_occupancy already used. Access via bitmap_occupancy_value(),
    // not the raw index formula, so the storage order stays an
    // implementation detail (this array is row-major; the original x86
    // BuildingDescriptorEditor::bitmap_occupancy_grid stores it column-major
    // with a 9-byte column stride -- see world/tilemap.h's
    // TileMapResource::span_map).
    std::vector<uint8_t> bitmap_occupancy_grid;

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

    // Raw bitmap_occupancy cell value at (x, y); 0 if out of bounds, no
    // footprint, or the .dat's row count didn't match its declared dims.
    uint8_t bitmap_occupancy_value(int x, int y) const {
        if (!has_footprint || x < 0 || x >= bitmap_grid_width || y < 0 ||
            y >= bitmap_grid_height) {
            return 0;
        }
        const size_t index = static_cast<size_t>(y) * bitmap_grid_width + x;
        return index < bitmap_occupancy_grid.size() ? bitmap_occupancy_grid[index] : 0;
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

// SpriteResource::Lock() (a real ResourceObject override, resources/
// ResourceObject.h) needs to reach the UIPANEL_Surface adapter implemented
// in resources/sprite_uipanel_adapter.cpp -- a separate translation unit
// from this file specifically so narrow test executables that link this
// file without the graphics subsystem don't pick up a
// UIPANEL_Surface::UIPANEL_Surface()/graphics/sdl3_ddraw.cpp dependency they
// never call. sprite_uipanel_adapter.cpp registers the hook at static-init
// time; SpriteResource::Lock() calls it if registered, else returns nullptr
// (matching the pre-existing "no adapter loaded" behavior for those narrow
// targets). See PROGRESS.md's DDRAW sprite-data management item.
using SurfaceLockHook = void* (*)(SpriteResource* resource, int32_t flags, int32_t mode);
void register_surface_lock_hook(SurfaceLockHook hook);

SpriteBitmap* sprite_bitmap(SpriteResource* resource);
uint32_t sprite_width(const SpriteResource* resource);
uint32_t sprite_height(const SpriteResource* resource);
// Width of a single animation frame -- resource->width is the whole decoded
// bitmap's width, which is total_frames-many equal-width frames laid out
// side by side (see Entity::SetFrame). Use this, not sprite_width(), for
// any frame-relative source-rect X computation.
uint32_t sprite_frame_width(const SpriteResource* resource);
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

    // Decodes an arbitrary archive-relative bitmap path (e.g.
    // "backdrop/arrid.bmp") that isn't reached through the numeric resource-
    // ID table -- the town backdrop is selected by name (SaveRegion::name,
    // shared/types.h) rather than by ID. Caller owns the returned surface.
    SDL_Surface* load_bitmap_by_path(const std::string& archive_path);

private:
    struct Impl;
    Impl* impl_ = nullptr;
    std::string last_error_;
};

ResourceManagerSdl3& host_resource_manager();

// Free-function wrappers around host_resource_manager().initialize()/reset(),
// for translation units that need the real bridge but can't include this
// whole header -- e.g. a test that also includes network/Netman.h, whose
// ResourceManager_Init(void*) -> void declaration ambiguates against this
// header's ResourceManager_Init(void*) -> int (see PROGRESS.md's "raw fixed-
// offset reads" landmine item; tests/persistence_fixtures.h documents the
// same collision). Forward-declaring just these two avoids pulling in the
// conflicting declaration.
bool initialize_host_resource_manager(const std::string& game_root, std::string* error = nullptr);
void reset_host_resource_manager();

// Narrow host-compositor bridge. These free functions let translated UI files
// use archive-backed sprites without importing the binary-facing C wrappers.
SpriteResource* host_get_sprite_by_id(uint32_t resource_id);

// Loads "backdrop/<name>.bmp" (SaveRegion::name, shared/types.h) and Blts it
// as the primary surface's base layer -- the mode-3 town's terrain, selected
// by the .sav header rather than drawn through any per-tile object (see
// resources/town_backdrop_adapter.cpp). Returns false if the name is empty,
// the bitmap can't be found, or there is no primary surface to draw into.
bool load_and_draw_town_backdrop(const char* name);

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

// Host source for the RESDATA+0x62C leisure_destination byte
// INPUT_PlaceObject (0x41DE9B..0x41DEC6) reads off a newly-placed entity's
// resource pointer. Returns false (leaving *out_byte untouched) when
// `resource` isn't a host SpriteResource -- callers should treat that as
// "no leisure destination" (matching the original's zero-init default),
// not fall back to an arbitrary value.
bool sprite_leisure_destination_byte(const void* resource, uint8_t* out_byte);

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
