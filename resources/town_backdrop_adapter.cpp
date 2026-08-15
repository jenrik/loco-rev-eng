/**
 * town_backdrop_adapter.cpp — host town-backdrop loader/blit
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 *
 * The mode-3 town's base terrain is a single full-frame bitmap selected by
 * name, not drawn through TileMap's per-tile object mechanism: the .sav
 * header's SaveRegion::name field (shared/types.h, +0x0E) carries a backdrop
 * filename ("ARRID" for the shipped Wildwest.sav -- confirmed against the
 * raw file header and Wildwest's own desert/sand look), and the original's
 * INPUT_LoadSaveFile (0x41D5C0) feeds that name straight to UIPANEL_Hide
 * (0x429EF0, ui/UIPANEL_Draw.cpp -- misnamed; it shows/creates the backdrop,
 * not hides it), which loads "backdrop\<name>.bmp" and paints it as the
 * world-sized base layer. UIPANEL_Hide's own body is raw decompiler-shaped
 * vtable/offset dispatch (Status: TRANSCRIBED, not a call target per
 * CLAUDE.md); this adapter reproduces its real effect -- load the named
 * backdrop bitmap and Blt it as the primary surface's base layer -- through
 * the real host resource/DirectDraw types instead.
 *
 * Deliberately a separate translation unit from resource_manager_sdl3.cpp
 * for the same reason as sprite_uipanel_adapter.cpp: SDL3_WrapSdlSurfaceAsDirectDraw
 * lives in graphics/sdl3_ddraw.cpp, which several narrow test executables
 * don't link.
 */

#include "resource_manager_sdl3.h"

#ifndef _WIN32

#include "../graphics/sdl3_ddraw.h"

#include <SDL3/SDL.h>

extern void* g_primary_surface; /* 0x4FD3C4 -- graphics/DDRAW.h */

namespace loco::assets {

bool load_and_draw_town_backdrop(const char* name) {
    if (name == nullptr || *name == '\0' || g_primary_surface == nullptr) {
        return false;
    }

    const std::string path = std::string("backdrop/") + name + ".bmp";
    SDL_Surface* surface = host_resource_manager().load_bitmap_by_path(path);
    if (surface == nullptr) {
        return false;
    }

    IDirectDrawSurface4* backdrop = SDL3_WrapSdlSurfaceAsDirectDraw(surface);
    SDL_DestroySurface(surface);
    if (backdrop == nullptr) {
        return false;
    }

    HRESULT hr = static_cast<IDirectDrawSurface4*>(g_primary_surface)
        ->Blt(nullptr, backdrop, nullptr, 0, nullptr);
    backdrop->Release();
    return hr == 0;
}

}  // namespace loco::assets

#endif  // !_WIN32
