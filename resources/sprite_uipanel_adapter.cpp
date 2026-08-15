/**
 * sprite_uipanel_adapter.cpp — host SpriteResource -> UIPANEL_Surface bridge
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 *
 * Entity::Draw/DrawConnected (core/GameObject.cpp) blit through
 * UIPANEL_Blit(), whose mode==1 path expects a real UIPANEL_Surface with
 * `ddraw_surf` pointing at a real IDirectDrawSurface4 (graphics/LOCOBITMAP.h,
 * ui/UIPANEL_Surface.cpp). Host SpriteResources (resources/
 * resource_manager_sdl3.h) carry a decoded SDL_Surface* instead of that x86
 * layout. host_lock_sprite_surface() lazily builds and caches (for the
 * process lifetime, matching SpriteResource's own never-released cache) one
 * small adapter object per resource wrapping that SDL_Surface* the same way
 * DDRAW_LoadBmpToSurface wraps a fresh BMP load. It's registered as
 * SpriteResource::Lock()'s implementation (resources/resource_manager_sdl3.cpp)
 * via register_surface_lock_hook() below, rather than called directly, so
 * every caller reaches it through ordinary ResourceObject virtual dispatch
 * (resources/ResourceObject.h) instead of a free function they'd need to
 * special-case around.
 *
 * Deliberately a separate translation unit from resource_manager_sdl3.cpp:
 * UIPANEL_Surface's constructor lives in the much heavier
 * graphics/LOCOBITMAP.cpp (PostcardAlbum class and friends), and several
 * narrow test executables link resource_manager_sdl3.cpp.o without that
 * subsystem at all. Keeping this bridge in its own file means only the main
 * game binary (whose sources are auto-discovered per subsystem directory,
 * see meson.build) picks up that dependency; SpriteResource::Lock() calls
 * through a hook that stays null (returning nullptr) unless this file's
 * static registrar below has run.
 */

#include "resource_manager_sdl3.h"

#ifndef _WIN32

#include "../graphics/LOCOBITMAP.h"
#include "../graphics/sdl3_ddraw.h"

#include <memory>
#include <unordered_map>

namespace loco::assets {

namespace {
std::unordered_map<const void*, std::unique_ptr<UIPANEL_Surface>>& adapter_cache() {
    static std::unordered_map<const void*, std::unique_ptr<UIPANEL_Surface>> cache;
    return cache;
}

void* host_lock_sprite_surface(SpriteResource* sprite, int32_t /*flags*/, int32_t /*mode*/) {
    if (sprite == nullptr) {
        return nullptr;
    }
    SpriteBitmap* bitmap = sprite_bitmap(sprite);
    SDL_Surface* sdl_surface = bitmap_surface(bitmap);
    if (sdl_surface == nullptr) {
        return nullptr;
    }

    auto& cache = adapter_cache();
    auto existing = cache.find(sprite);
    if (existing != cache.end()) {
        return existing->second.get();
    }

    auto surface = std::make_unique<UIPANEL_Surface>();
    surface->mode = 1;
    surface->width = static_cast<int32_t>(bitmap_width(bitmap));
    surface->height = static_cast<int32_t>(bitmap_height(bitmap));
    surface->ddraw_surf = SDL3_WrapSdlSurfaceAsDirectDraw(sdl_surface);

    UIPANEL_Surface* raw = surface.get();
    cache.emplace(sprite, std::move(surface));
    return raw;
}

// Static-init-time registration: runs once, before any Entity::Draw/
// InitBase call, since it's a global object's constructor in the same
// translation unit as host_lock_sprite_surface.
struct SurfaceLockHookRegistrar {
    SurfaceLockHookRegistrar() { register_surface_lock_hook(&host_lock_sprite_surface); }
} g_surface_lock_hook_registrar;

}  // namespace

}  // namespace loco::assets

#endif  // !_WIN32
