// Status: VALIDATED
/**
 * resource_manager_sdl3.cpp — Host bridge for ResourceManager asset lookups
 *
 * ResourceManager_Init (0x446050) receives a PE string-table path and its
 * sprite consumers establish these layouts: EditWindow::initSprites (0x421500)
 * reads resource +0x14/+0x16; EditWindow::render (0x4216F0) reads bitmap
 * +0x08/+0x0C. The bridge preserves those contracts, parses paired DAT
 * animation metadata, and implements the DirectDraw magenta source color key
 * (RGB 255,0,255) documented by DDRAW_RestoreSurfaces.
 *
 * SpriteResource is a real ResourceObject (resources/ResourceObject.h),
 * matching RESDATA's original 3-slot vtable ([0]=Destroy, [1]=Lock/
 * GetSurface, [2]=Unlock) via genuine C++ virtual dispatch instead of a
 * manually-built function-pointer array -- see Lock()'s doc comment for how
 * it reaches the UIPANEL_Surface adapter without this file linking
 * graphics/sdl3_ddraw.cpp.
 */
#include "resource_manager_sdl3.h"

#include "ResourceObject.h"
#include "pe_string_table.h"
#include "resource_archive.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef _WIN32

namespace loco::assets {

struct SpriteBitmap {
    void* abi_reserved;       // +0x00: binary bitmap implementation details
    uint32_t width;           // +0x08: read by EditWindow::render
    uint32_t height;          // +0x0C: read by EditWindow::render
    SDL_Surface* surface;

    ~SpriteBitmap() {
        if (surface) SDL_DestroySurface(surface);
    }
};

// Identifies a genuine loco::assets::SpriteResource to is_host_sprite_resource()
// below. Placed after every offset EditWindow's ABI depends on (width/height at
// +0x14/+0x16) so adding it can never shift those asserted offsets.
constexpr uint32_t kSpriteResourceMagic = 0x53505231u; // 'SPR1'

// SpriteResource is a real ResourceObject subclass -- the compiler-managed
// vptr occupies +0x00 (where a manually-assigned `void** vtable` used to
// live), so the two offsetof asserts below still hold: nothing repositions
// width/height, which several still-untranslated callers
// (VehicleEditor.cpp, Cursor.cpp, Town.cpp) also read directly at these same
// offsets pending their own typed-accessor migration. offsetof on a
// non-standard-layout type (SpriteResource now has virtual functions) is a
// long-standing, widely-relied-upon compiler extension here, suppressed
// locally rather than tree-wide.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
struct SpriteResource : public ResourceObject {
    uint8_t padding_08_to_13[0x14 - sizeof(void*)];    // +0x08..+0x13 on x86_64
    uint16_t width;                                    // +0x14: read by initSprites
    uint16_t height;                                   // +0x16: read by initSprites
    std::unique_ptr<SpriteBitmap> bitmap;
    uint32_t resource_id;
    SpriteMetadata metadata;
    bool has_metadata;
    uint32_t magic = kSpriteResourceMagic;

    // ResourceObject overrides -- Lock()'s definition (below) is how this
    // reaches the SDL3-backed UIPANEL_Surface adapter. No destructor
    // override needed: host sprites are cached and owned for the process
    // lifetime by ResourceManagerSdl3::Impl::resources (never individually
    // deleted), so the compiler-generated ~SpriteResource() (which tears
    // down the unique_ptr<SpriteBitmap>/metadata members normally) is
    // exactly right.
    void* Lock(int32_t flags, int32_t mode) override;
    void Unlock() override;
};

namespace {

static_assert(offsetof(SpriteResource, width) == 0x14,
              "Host resource width must match EditWindow::initSprites");
static_assert(offsetof(SpriteResource, height) == 0x16,
              "Host resource height must match EditWindow::initSprites");
static_assert(offsetof(SpriteBitmap, width) == 0x08,
              "Host bitmap width must match EditWindow::render");
static_assert(offsetof(SpriteBitmap, height) == 0x0c,
              "Host bitmap height must match EditWindow::render");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// Registered by resources/sprite_uipanel_adapter.cpp (a separate translation
// unit specifically so this file, and the narrow test binaries that link it
// without the graphics subsystem, don't pull in graphics/sdl3_ddraw.cpp).
// SpriteResource::Lock() calls through this if a hook is registered,
// otherwise returns nullptr -- matching the pre-existing "no adapter loaded"
// behavior for those narrow targets.
SurfaceLockHook g_surface_lock_hook = nullptr;

std::string game_root_from_environment() {
    const char* configured = std::getenv("LEGO_LOCO_DATA");
    return configured && *configured ? configured : "lego-loco-unpacked";
}

bool parse_int(const std::string& token, int* value) {
    try {
        size_t parsed = 0;
        const int result = std::stoi(token, &parsed, 10);
        if (parsed != token.size()) return false;
        *value = result;
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> split_tokens(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) tokens.push_back(token);
    return tokens;
}

// Classifies a "tunnel"/"depot"/"bridge"/"points"/"switch"/"crosstrack"/
// "levelcrossing"/"station" directive line into TileTrackType. `tokens[0]`
// is the type keyword; `tokens[1]`, when the keyword needs one, is the
// direction/orientation qualifier on the same line (confirmed against real
// track/*.dat archive data -- see TileTrackType's doc comment). Returns
// false (leaving *type unset) for anything that isn't one of these exact
// keyword/qualifier pairs.
bool classify_tile_track_type(const std::vector<std::string>& tokens, TileTrackType* type) {
    static const std::unordered_map<std::string, TileTrackType> kDirectional = {
        {"tunnel left", TileTrackType::TunnelLeft},
        {"tunnel right", TileTrackType::TunnelRight},
        {"tunnel top", TileTrackType::TunnelTop},
        {"tunnel bottom", TileTrackType::TunnelBottom},
        {"bridge horizontal", TileTrackType::BridgeHorizontal},
        {"bridge vertical", TileTrackType::BridgeVertical},
        {"depot left", TileTrackType::DepotLeft},
        {"depot right", TileTrackType::DepotRight},
        {"depot top", TileTrackType::DepotTop},
        {"depot bottom", TileTrackType::DepotBottom},
        {"levelcrossing path-x-h", TileTrackType::LevelCrossingPathHorizontal},
        {"levelcrossing path-x-v", TileTrackType::LevelCrossingPathVertical},
        {"levelcrossing road-x-h", TileTrackType::LevelCrossingRoadHorizontal},
        {"levelcrossing road-x-v", TileTrackType::LevelCrossingRoadVertical},
        {"station station-h", TileTrackType::StationHorizontal},
        {"station station-v", TileTrackType::StationVertical},
    };
    static const std::unordered_map<std::string, TileTrackType> kStandalone = {
        {"points", TileTrackType::Points},
        {"switch", TileTrackType::Switch},
        {"crosstrack", TileTrackType::CrossTrack},
    };
    if (tokens.size() == 2) {
        const auto it = kDirectional.find(tokens[0] + " " + tokens[1]);
        if (it != kDirectional.end()) {
            *type = it->second;
            return true;
        }
    }
    if (tokens.size() == 1) {
        const auto it = kStandalone.find(tokens[0]);
        if (it != kStandalone.end()) {
            *type = it->second;
            return true;
        }
    }
    return false;
}

bool parse_sprite_metadata(const std::vector<uint8_t>& bytes, SpriteMetadata* metadata) {
    *metadata = SpriteMetadata{};
    const std::string text(bytes.begin(), bytes.end());
    // Frame-set rows (name + 10 numeric fields) directly follow the
    // "number_of_frame_sets N" directive -- confirmed against real archive
    // files (e.g. track/strhlf-v.dat, track/switch-h.dat): there is no
    // literal "animation" keyword line anywhere in the shipped .dat corpus.
    // A prior version of this parser gated frame-set-row recognition on
    // seeing that fictional keyword, so `frame_sets` stayed permanently
    // empty regardless of frame_set_count -- every placed entity with any
    // declared frame set then failed Entity::SetAnimState's bounds check
    // and was torn down as "uninitialized" (see PROGRESS.md's
    // INPUT_PlaceObject recovery entry). Countdown-bounded the same way
    // physical_rows_remaining/bitmap_rows_remaining are, matching the
    // original's exact `frameSetCount`-iteration loop
    // (UI_ChildWindow_Render, 0x424E00).
    int frame_set_rows_remaining = 0;
    // The "physical_occupancy"/"bitmap_occupancy" section headers are each
    // immediately followed (skipping blank lines) by one dims line ("W H D"
    // / "W H"); the occupancy grid rows that follow are plain integer lines
    // that match no other keyword, so they fall through the loop untouched.
    bool expect_physical_dims = false;
    bool expect_bitmap_dims = false;
    // Set to grid_height*grid_depth right after the physical dims line parses;
    // counted down one data row at a time rather than scanned-for-next-keyword,
    // because some .dat files (e.g. scenery/bigfount.dat) interleave an
    // unrelated directive ("LeisureDestination 1") between the last occupancy
    // row and the "bitmap_occupancy" header -- a keyword-boundary scan would
    // swallow it as if it were grid data.
    int physical_rows_remaining = 0;
    // Same row-countdown rationale as physical_rows_remaining -- some .dat
    // files interleave directives (e.g. "LeisureDestination") right after
    // the last bitmap_occupancy row.
    int bitmap_rows_remaining = 0;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        const std::vector<std::string> tokens = split_tokens(line);
        if (tokens.empty() || tokens[0].rfind("//", 0) == 0) continue;
        if (expect_physical_dims) {
            expect_physical_dims = false;
            if (tokens.size() == 3 &&
                parse_int(tokens[0], &metadata->footprint.grid_width) &&
                parse_int(tokens[1], &metadata->footprint.grid_height) &&
                parse_int(tokens[2], &metadata->footprint.grid_depth)) {
                metadata->footprint.has_footprint = true;
                metadata->footprint.physical_occupancy_grid.clear();
                physical_rows_remaining =
                    metadata->footprint.grid_height * metadata->footprint.grid_depth;
            }
            continue;
        }
        if (physical_rows_remaining > 0) {
            if (tokens.size() == static_cast<size_t>(metadata->footprint.grid_width)) {
                std::vector<uint8_t> row;
                row.reserve(tokens.size());
                bool row_ok = true;
                for (const std::string& cell : tokens) {
                    int value = 0;
                    if (!parse_int(cell, &value)) { row_ok = false; break; }
                    row.push_back(static_cast<uint8_t>(value));
                }
                if (row_ok) {
                    metadata->footprint.physical_occupancy_grid.insert(
                        metadata->footprint.physical_occupancy_grid.end(),
                        row.begin(), row.end());
                    --physical_rows_remaining;
                    continue;
                }
            }
            // Not a grid data row after all (e.g. an interleaved directive
            // line) -- stop collecting instead of misparsing it as a cell row.
            physical_rows_remaining = 0;
        }
        if (expect_bitmap_dims) {
            expect_bitmap_dims = false;
            if (tokens.size() == 2 &&
                parse_int(tokens[0], &metadata->footprint.bitmap_grid_width) &&
                parse_int(tokens[1], &metadata->footprint.bitmap_grid_height)) {
                metadata->footprint.has_footprint = true;
                metadata->footprint.bitmap_occupancy_grid.clear();
                bitmap_rows_remaining = metadata->footprint.bitmap_grid_height;
            }
            continue;
        }
        if (bitmap_rows_remaining > 0) {
            if (tokens.size() == static_cast<size_t>(metadata->footprint.bitmap_grid_width)) {
                std::vector<uint8_t> row;
                row.reserve(tokens.size());
                bool row_ok = true;
                for (const std::string& cell : tokens) {
                    int value = 0;
                    if (!parse_int(cell, &value)) { row_ok = false; break; }
                    row.push_back(static_cast<uint8_t>(value));
                }
                if (row_ok) {
                    metadata->footprint.bitmap_occupancy_grid.insert(
                        metadata->footprint.bitmap_occupancy_grid.end(),
                        row.begin(), row.end());
                    --bitmap_rows_remaining;
                    continue;
                }
            }
            bitmap_rows_remaining = 0;
        }
        if (tokens[0] == "physical_occupancy") {
            expect_physical_dims = true;
            continue;
        }
        if (tokens[0] == "bitmap_occupancy") {
            expect_bitmap_dims = true;
            continue;
        }
        {
            TileTrackType track_type{};
            if (classify_tile_track_type(tokens, &track_type)) {
                metadata->tile_type = track_type;
                metadata->has_tile_type = true;
                continue;
            }
        }
        if (tokens[0] == "Name") {
            // Real archive lines are "Name<one space><text>" (e.g. "Name
            // Factory", "Name School " with a trailing space preserved) --
            // skip exactly the keyword plus one separator character, matching
            // the original's `strncpy(this+0x14D, line+1, 10)` (see
            // ui/UI_ChildWindow.cpp's "Name" branch), then trim up to 2
            // trailing \r/\n the same way, and truncate to the buffer's
            // effective 9-character content.
            std::string rest = line.substr(4);  // tokens[0] == "Name" guarantees line.size() >= 4
            if (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest.erase(0, 1);
            for (int trim = 0; trim < 2 && !rest.empty() &&
                                (rest.back() == '\r' || rest.back() == '\n');
                 ++trim) {
                rest.pop_back();
            }
            if (rest.size() > 9) rest.resize(9);
            metadata->name = rest;
            continue;
        }
        if (tokens[0] == "EEReplayDelay" && tokens.size() == 2) {
            int value = 0;
            if (parse_int(tokens[1], &value)) {
                metadata->ee_replay_delay = static_cast<uint8_t>(value);
                if (metadata->ee_replay_delay > 5) metadata->ee_replay_delay = 5;
            }
            continue;
        }
        if (tokens[0] == "LeisureDestination" && tokens.size() == 2) {
            int value = 0;
            if (parse_int(tokens[1], &value)) {
                metadata->leisure_destination = static_cast<uint8_t>(value);
            }
            continue;
        }
        if (tokens[0] == "button") {
            metadata->is_button = true;
            if (tokens.size() >= 5 && tokens[1] == "offset") {
                if (!parse_int(tokens[2], &metadata->offset_x) ||
                    !parse_int(tokens[3], &metadata->offset_y) ||
                    !parse_int(tokens[4], &metadata->offset_z)) return false;
            }
            continue;
        }
        if (tokens[0] == "offset" && tokens.size() >= 4) {
            if (!parse_int(tokens[1], &metadata->offset_x) ||
                !parse_int(tokens[2], &metadata->offset_y) ||
                !parse_int(tokens[3], &metadata->offset_z)) return false;
            continue;
        }
        if (tokens[0] == "total_number_of_frames" && tokens.size() == 2) {
            if (!parse_int(tokens[1], &metadata->total_frames)) return false;
            continue;
        }
        if (tokens[0] == "number_of_frame_sets" && tokens.size() == 2) {
            if (!parse_int(tokens[1], &metadata->frame_set_count)) return false;
            frame_set_rows_remaining = metadata->frame_set_count;
            continue;
        }
        // Real archive data uses two distinct spellings for this directive
        // (379 files "cursor_frame_set", 198 files "cursor/default_frame_set"
        // -- matching UI_ChildWindow.cpp's two CRT_wcsstr checks against
        // s_cursor_frame_set/s_cursor_default_frame_set exactly). A third,
        // rarer typo'd spelling ("cursor_Frame_set", capital F, 8 files) is
        // deliberately NOT recognized here: CRT_wcsstr is case-sensitive, so
        // the original parser doesn't recognize it either and falls through
        // to `break` for those files -- reproduced, not "fixed".
        if ((tokens[0] == "cursor_frame_set" || tokens[0] == "cursor/default_frame_set") &&
            tokens.size() == 3) {
            if (!parse_int(tokens[1], &metadata->cursor_frame_set) ||
                !parse_int(tokens[2], &metadata->cursor_frame)) return false;
            continue;
        }
        // DAT comments define an animation row as name plus ten integer fields.
        if (frame_set_rows_remaining > 0 && tokens.size() == 11) {
            AnimationFrameSet frame_set;
            frame_set.name = tokens[0];
            int is_connected_int = 0;
            int* fields[] = {&frame_set.start_frame, &frame_set.end_frame,
                             &frame_set.frame_delay, &is_connected_int,
                             &frame_set.restart_delay, &frame_set.next_frame_set,
                             &frame_set.sound_resource_id, &frame_set.replay_delay,
                             &frame_set.volume};
            bool valid = true;
            for (size_t index = 0; index < 9; ++index) {
                valid = valid && parse_int(tokens[index + 1], fields[index]);
            }
            frame_set.is_connected = (is_connected_int != 0);
            // The 10th numeric token -- see AnimationFrameSet::flip_horizontal's
            // doc comment for the disassembly evidence that this is a real,
            // used field (FrameData::flip_horizontal, +0x16), not discardable.
            int flip_horizontal_int = 0;
            valid = valid && parse_int(tokens[10], &flip_horizontal_int);
            frame_set.flip_horizontal = (flip_horizontal_int != 0);
            --frame_set_rows_remaining;
            if (valid) metadata->frame_sets.push_back(std::move(frame_set));
            continue;
        }
    }
    return metadata->frame_set_count == 0 ||
           metadata->frame_sets.size() == static_cast<size_t>(metadata->frame_set_count);
}

void apply_magenta_color_key(SDL_Surface* surface) {
    if (!surface || !SDL_ISPIXELFORMAT_INDEXED(surface->format)) return;
    SDL_Palette* palette = SDL_GetSurfacePalette(surface);
    if (!palette) return;
    for (int index = 0; index < palette->ncolors; ++index) {
        const SDL_Color& color = palette->colors[index];
        if (color.r == 255 && color.g == 0 && color.b == 255) {
            SDL_SetSurfaceColorKey(surface, true, static_cast<Uint32>(index));
            return;
        }
    }
}

AssetType asset_type_for_path(const std::string& path) {
    const size_t dot = path.rfind('.');
    const std::string extension = dot == std::string::npos ? "" : path.substr(dot);
    if (extension == ".bmp") return AssetType::Bitmap;
    if (extension == ".dat") return AssetType::Metadata;
    if (extension == ".wav") return AssetType::Wave;
    if (extension == ".but") return AssetType::Button;
    if (extension == ".ani") return AssetType::AnimatedCursor;
    if (extension == ".lay") return AssetType::Layout;
    if (extension == ".sav") return AssetType::Save;
    if (extension == ".ini") return AssetType::Ini;
    return AssetType::Unknown;
}

}  // namespace

struct ResourceManagerSdl3::Impl {
    Archive archive;
    PeStringTable strings;
    std::unordered_map<uint32_t, std::unique_ptr<SpriteResource>> resources;
    std::unordered_map<uint64_t, AssetBlob> assets;
};

bool ResourceManagerSdl3::initialize(const std::string& game_root, std::string* error) {
    reset();
    auto next = std::make_unique<Impl>();
    std::string detail;
    if (!next->archive.open(game_root + "/art-res", &detail) ||
        !next->strings.open(game_root + "/Exe/loco.exe", &detail)) {
        last_error_ = detail;
        if (error) *error = detail;
        return false;
    }
    impl_ = next.release();
    last_error_.clear();
    return true;
}

void ResourceManagerSdl3::reset() {
    delete impl_;
    impl_ = nullptr;
}

const AssetBlob* ResourceManagerSdl3::load_asset_by_id(uint32_t resource_id, AssetType type) {
    if (!impl_ && !initialize(game_root_from_environment(), nullptr)) return nullptr;
    static const char* const extensions[] = {".bmp", ".dat", ".wav", ".but", ".ani", ".lay", ".sav", ".ini"};
    const char* extension = nullptr;
    for (const char* candidate : extensions) {
        if (asset_type_for_path(candidate) == type) { extension = candidate; break; }
    }
    if (!extension) return nullptr;
    const uint64_t cache_key = (static_cast<uint64_t>(resource_id) << 32) | static_cast<uint32_t>(type);
    const auto cached = impl_->assets.find(cache_key);
    if (cached != impl_->assets.end()) return &cached->second;
    const std::string* stem = impl_->strings.find(resource_id);
    if (!stem) return nullptr;
    AssetBlob asset;
    asset.path = *stem + extension;
    asset.type = type;
    if (!impl_->archive.read(asset.path, &asset.bytes, nullptr)) return nullptr;
    const auto result = impl_->assets.emplace(cache_key, std::move(asset));
    return &result.first->second;
}

const AssetBlob* ResourceManagerSdl3::load_asset_by_id(uint32_t resource_id) {
    static const AssetType preferred[] = {AssetType::Bitmap, AssetType::Metadata, AssetType::Wave,
                                          AssetType::Button, AssetType::AnimatedCursor, AssetType::Layout,
                                          AssetType::Save, AssetType::Ini};
    for (const AssetType type : preferred) {
        if (const AssetBlob* asset = load_asset_by_id(resource_id, type)) return asset;
    }
    return nullptr;
}

SpriteResource* ResourceManagerSdl3::get_sprite_by_id(uint32_t resource_id) {
    if (!impl_ && !initialize(game_root_from_environment(), nullptr)) return nullptr;
    const auto cached = impl_->resources.find(resource_id);
    if (cached != impl_->resources.end()) return cached->second.get();

    const std::string* stem = impl_->strings.find(resource_id);
    if (!stem) return nullptr;
    std::vector<uint8_t> bytes;
    if (!impl_->archive.read(*stem + ".bmp", &bytes, nullptr)) return nullptr;
    SDL_IOStream* stream = SDL_IOFromConstMem(bytes.data(), bytes.size());
    if (!stream) return nullptr;
    SDL_Surface* surface = SDL_LoadBMP_IO(stream, true);
    if (!surface) return nullptr;
    apply_magenta_color_key(surface);

    const int width = surface->w;
    const int height = surface->h;
    if (width < 0 || height < 0 || width > 0xffff || height > 0xffff) {
        SDL_DestroySurface(surface);
        return nullptr;
    }
    auto bitmap = std::make_unique<SpriteBitmap>();
    bitmap->abi_reserved = nullptr;
    bitmap->width = static_cast<uint32_t>(width);
    bitmap->height = static_cast<uint32_t>(height);
    bitmap->surface = surface;

    auto resource = std::make_unique<SpriteResource>();
    resource->width = static_cast<uint16_t>(width);
    resource->height = static_cast<uint16_t>(height);
    resource->bitmap = std::move(bitmap);
    resource->resource_id = resource_id;
    resource->has_metadata = false;
    std::vector<uint8_t> dat;
    if (impl_->archive.read(*stem + ".dat", &dat, nullptr)) {
        // parse_sprite_metadata's return value only reports whether the
        // animation frame-set *count* matched the rows actually parsed (a
        // narrower, separately-tracked gap in the animation-row grammar —
        // see PROGRESS.md's ResourceManager-consumers item); every field it
        // populates along the way (footprint, offsets, is_button, ...) is
        // real regardless, so metadata is exposed either way rather than
        // hidden behind an unrelated animation-parsing shortfall.
        parse_sprite_metadata(dat, &resource->metadata);
        resource->has_metadata = true;
    }
    SpriteResource* result = resource.get();
    impl_->resources.emplace(resource_id, std::move(resource));
    return result;
}

SDL_Surface* ResourceManagerSdl3::load_bitmap_by_path(const std::string& archive_path) {
    if (!impl_ && !initialize(game_root_from_environment(), nullptr)) return nullptr;
    std::vector<uint8_t> bytes;
    if (!impl_->archive.read(archive_path, &bytes, nullptr)) return nullptr;
    SDL_IOStream* stream = SDL_IOFromConstMem(bytes.data(), bytes.size());
    if (!stream) return nullptr;
    // No magenta color-keying here (unlike get_sprite_by_id's sprites): the
    // backdrop is an opaque full-frame background, not a keyed sprite --
    // punching transparency into it on an incidental color match would be
    // wrong, not just unnecessary.
    return SDL_LoadBMP_IO(stream, true);
}

const SpriteMetadata* ResourceManagerSdl3::sprite_metadata(const SpriteResource* sprite) const {
    return sprite && sprite->has_metadata ? &sprite->metadata : nullptr;
}

SDL_Surface* ResourceManagerSdl3::sprite_surface(const SpriteResource* sprite) const {
    return sprite && sprite->bitmap ? sprite->bitmap->surface : nullptr;
}

void* ResourceManagerSdl3::get_by_id(uint32_t resource_id) {
    return get_sprite_by_id(resource_id);
}

SpriteBitmap* sprite_bitmap(SpriteResource* resource) {
    return resource ? resource->bitmap.get() : nullptr;
}
uint32_t sprite_width(const SpriteResource* resource) { return resource ? resource->width : 0; }
uint32_t sprite_height(const SpriteResource* resource) { return resource ? resource->height : 0; }
uint32_t sprite_frame_width(const SpriteResource* resource) {
    if (!resource) return 0;
    // The decoded bitmap is a single horizontal strip of total_frames equal-
    // width frames (confirmed by Entity::SetFrame's own indexing: it only
    // ever offsets source_rect by frame_id * frame_w along X, never
    // adjusts Y or wraps rows) -- resource->width is the whole strip's
    // width, not one frame's. A resource with no metadata or a
    // total_frames <= 1 is a single-frame sprite, where the two are the
    // same value anyway.
    const int total_frames =
        (resource->has_metadata && resource->metadata.total_frames > 0)
            ? resource->metadata.total_frames : 1;
    return resource->width / static_cast<uint32_t>(total_frames);
}
uint32_t sprite_resource_id(const SpriteResource* resource) { return resource ? resource->resource_id : 0; }
uint32_t bitmap_width(const SpriteBitmap* bitmap) { return bitmap ? bitmap->width : 0; }
uint32_t bitmap_height(const SpriteBitmap* bitmap) { return bitmap ? bitmap->height : 0; }
SDL_Surface* bitmap_surface(const SpriteBitmap* bitmap) { return bitmap ? bitmap->surface : nullptr; }
void release_sprite(SpriteResource*) {}

void register_surface_lock_hook(SurfaceLockHook hook) {
    g_surface_lock_hook = hook;
}

// Unlock is a no-op: host sprites aren't refcounted per-lock -- matches
// release_sprite() above and Entity::~Entity's already-documented "not
// per-entity refcounted" host semantics (core/GameObject.cpp).
void SpriteResource::Unlock() {}

void* SpriteResource::Lock(int32_t flags, int32_t mode) {
    return g_surface_lock_hook ? g_surface_lock_hook(this, flags, mode) : nullptr;
}

ResourceManagerSdl3& host_resource_manager() {
    static ResourceManagerSdl3 instance;
    return instance;
}

SpriteResource* host_get_sprite_by_id(uint32_t resource_id) {
    return host_resource_manager().get_sprite_by_id(resource_id);
}

bool initialize_host_resource_manager(const std::string& game_root, std::string* error) {
    return host_resource_manager().initialize(game_root, error);
}

void reset_host_resource_manager() {
    host_resource_manager().reset();
}

bool is_host_sprite_resource(const void* resource) {
    if (resource == nullptr) {
        return false;
    }
    return static_cast<const SpriteResource*>(resource)->magic == kSpriteResourceMagic;
}

bool sprite_tile_type_byte(const void* resource, uint8_t* out_byte) {
    if (!is_host_sprite_resource(resource)) {
        return false;
    }
    const SpriteMetadata* metadata =
        ResourceManager_GetSpriteMetadata(const_cast<void*>(resource));
    if (metadata == nullptr || !metadata->has_tile_type) {
        return false;
    }
    *out_byte = static_cast<uint8_t>(metadata->tile_type);
    return true;
}

bool sprite_leisure_destination_byte(const void* resource, uint8_t* out_byte) {
    if (!is_host_sprite_resource(resource)) {
        return false;
    }
    const SpriteMetadata* metadata =
        ResourceManager_GetSpriteMetadata(const_cast<void*>(resource));
    if (metadata == nullptr) {
        return false;
    }
    *out_byte = metadata->leisure_destination;
    return true;
}

}  // namespace loco::assets

int ResourceManager_Init(void*) {
    return loco::assets::host_resource_manager().initialize(
        [] { const char* root = std::getenv("LEGO_LOCO_DATA");
             return root && *root ? std::string(root) : std::string("lego-loco-unpacked"); }(),
        nullptr) ? 1 : 0;
}

void* ResourceManager_GetById(void*, int32_t resource_id) {
    return resource_id < 0 ? nullptr : loco::assets::host_resource_manager().get_by_id(
        static_cast<uint32_t>(resource_id));
}
void* ResourceManager_GetById(void*, uint32_t resource_id) {
    return loco::assets::host_resource_manager().get_by_id(resource_id);
}
void* ResourceManager_GetById(void**, int32_t resource_id) {
    return ResourceManager_GetById(static_cast<void*>(nullptr), resource_id);
}
void* ResourceManager_GetById(void**, uint32_t resource_id) {
    return ResourceManager_GetById(static_cast<void*>(nullptr), resource_id);
}
SDL_Surface* ResourceManager_GetSpriteSurface(void* resource) {
    return loco::assets::host_resource_manager().sprite_surface(static_cast<loco::assets::SpriteResource*>(resource));
}
const loco::assets::SpriteMetadata* ResourceManager_GetSpriteMetadata(void* resource) {
    return loco::assets::host_resource_manager().sprite_metadata(static_cast<loco::assets::SpriteResource*>(resource));
}

#endif /* _WIN32 */
