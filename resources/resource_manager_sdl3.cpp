// Status: VALIDATED
/**
 * resource_manager_sdl3.cpp — Host bridge for ResourceManager asset lookups
 *
 * ResourceManager_Init (0x446050) receives a PE string-table path and its
 * sprite consumers establish these layouts: EditWindow::initSprites (0x421500)
 * reads resource +0x14/+0x16 and calls vtable[4]; EditWindow::render
 * (0x4216F0) reads bitmap +0x08/+0x0C. The bridge preserves those contracts,
 * parses paired DAT animation metadata, and implements the DirectDraw magenta
 * source color key (RGB 255,0,255) documented by DDRAW_RestoreSurfaces.
 */
#include "resource_manager_sdl3.h"

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

struct SpriteResource {
    void** vtable;                                     // +0x00
    uint8_t padding_08_to_13[0x14 - sizeof(void*)];    // +0x08..+0x13 on x86_64
    uint16_t width;                                    // +0x14: read by initSprites
    uint16_t height;                                   // +0x16: read by initSprites
    std::unique_ptr<SpriteBitmap> bitmap;
    uint32_t resource_id;
    SpriteMetadata metadata;
    bool has_metadata;
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

void* resource_get_bitmap(void* resource, int, int) {
    return static_cast<SpriteResource*>(resource)->bitmap.get();
}

// Legacy ABI adapter only: untranslated callers still invoke the original
// resource vtable slots. Typed consumers use sprite_bitmap/release_sprite below
// and never inspect this table. Remove it once every GetById caller is typed.
void resource_release(void*) {}

void* k_resource_vtable[9] = {
    nullptr, nullptr, nullptr, nullptr,
    reinterpret_cast<void*>(&resource_get_bitmap), nullptr, nullptr, nullptr,
    reinterpret_cast<void*>(&resource_release),
};

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

bool parse_sprite_metadata(const std::vector<uint8_t>& bytes, SpriteMetadata* metadata) {
    *metadata = SpriteMetadata{};
    const std::string text(bytes.begin(), bytes.end());
    bool in_animation = false;
    // The "physical_occupancy"/"bitmap_occupancy" section headers are each
    // immediately followed (skipping blank lines) by one dims line ("W H D"
    // / "W H"); the occupancy grid rows that follow are plain integer lines
    // that match no other keyword, so they fall through the loop untouched.
    bool expect_physical_dims = false;
    bool expect_bitmap_dims = false;
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
            }
            continue;
        }
        if (expect_bitmap_dims) {
            expect_bitmap_dims = false;
            if (tokens.size() == 2 &&
                parse_int(tokens[0], &metadata->footprint.bitmap_grid_width) &&
                parse_int(tokens[1], &metadata->footprint.bitmap_grid_height)) {
                metadata->footprint.has_footprint = true;
            }
            continue;
        }
        if (tokens[0] == "physical_occupancy") {
            expect_physical_dims = true;
            continue;
        }
        if (tokens[0] == "bitmap_occupancy") {
            expect_bitmap_dims = true;
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
        if (tokens[0] == "animation") {
            in_animation = true;
            continue;
        }
        if (tokens[0] == "total_number_of_frames" && tokens.size() == 2) {
            if (!parse_int(tokens[1], &metadata->total_frames)) return false;
            continue;
        }
        if (tokens[0] == "number_of_frame_sets" && tokens.size() == 2) {
            if (!parse_int(tokens[1], &metadata->frame_set_count)) return false;
            continue;
        }
        if (tokens[0] == "cursor_frame_set" && tokens.size() == 3) {
            if (!parse_int(tokens[1], &metadata->cursor_frame_set) ||
                !parse_int(tokens[2], &metadata->cursor_frame)) return false;
            continue;
        }
        // DAT comments define an animation row as name plus ten integer fields.
        if (in_animation && tokens.size() == 11) {
            AnimationFrameSet frame_set;
            frame_set.name = tokens[0];
            int* fields[] = {&frame_set.start_frame, &frame_set.end_frame,
                             &frame_set.frame_delay, &frame_set.split_frames,
                             &frame_set.restart_delay, &frame_set.next_frame_set,
                             &frame_set.sound_resource_id, &frame_set.replay_delay,
                             &frame_set.flip_x};
            bool valid = true;
            for (size_t index = 0; index < 9; ++index) {
                valid = valid && parse_int(tokens[index + 1], fields[index]);
            }
            // The tenth numeric field is retained only by the original opaque
            // animation implementation; parsing it validates row shape.
            int opaque_field = 0;
            valid = valid && parse_int(tokens[10], &opaque_field);
            if (valid) metadata->frame_sets.push_back(std::move(frame_set));
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
    resource->vtable = k_resource_vtable;
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
uint32_t bitmap_width(const SpriteBitmap* bitmap) { return bitmap ? bitmap->width : 0; }
uint32_t bitmap_height(const SpriteBitmap* bitmap) { return bitmap ? bitmap->height : 0; }
SDL_Surface* bitmap_surface(const SpriteBitmap* bitmap) { return bitmap ? bitmap->surface : nullptr; }
void release_sprite(SpriteResource*) {}

ResourceManagerSdl3& host_resource_manager() {
    static ResourceManagerSdl3 instance;
    return instance;
}

SpriteResource* host_get_sprite_by_id(uint32_t resource_id) {
    return host_resource_manager().get_sprite_by_id(resource_id);
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
