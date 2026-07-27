/**
 * sdl3_ddraw.cpp — DirectDraw 4 → SDL3 compatibility shim implementation
 *
 * Implements IDirectDraw4 and IDirectDrawSurface4 using SDL3's
 * renderer API (SDL_Texture for GPU blits, SDL_Surface for CPU
 * pixel access during Lock/Unlock).
 *
 * NOT part of the Lego Loco reverse-engineering project.
 */

#include "sdl3_ddraw.h"
#include "sdl3_window.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* =========================================================================
 * Forward: SDL3 BMP loader
 * ========================================================================= */

/* BMP dimension cache (set by DDRAW_LoadBmpToSurface, read by helpers) */
static uint32_t g_last_bmp_width  = 0;
static uint32_t g_last_bmp_height = 0;

// Host-only SDL ownership. The translated program's legacy void* DirectDraw
// globals remain untouched until their COM-compatible adapter is complete.
static IDirectDraw4* g_sdl_ddraw = nullptr;
static IDirectDrawSurface4* g_sdl_primary_surface = nullptr;
static IDirectDrawSurface4* g_sdl_backbuffer = nullptr;

static SDL_Surface* loadBmpToSdlSurface(const char* path, int bpp)
{
    (void)bpp;

    /* SDL3_image would be better, but we use SDL's built-in BMP loader */
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (!io) {
        fprintf(stderr, "SDL3: Cannot open %s: %s\n", path, SDL_GetError());
        return nullptr;
    }

    SDL_Surface* surface = SDL_LoadBMP_IO(io, true); /* true = close IO */
    if (!surface) {
        fprintf(stderr, "SDL3: Failed to load BMP %s: %s\n", path, SDL_GetError());
        return nullptr;
    }

    /* Palettized BMPs (1/2/4/8-bit) are loaded as indexed surfaces.
     * SDL_ConvertSurface to XRGB8888 uses the BMP's embedded palette
     * to expand each index to a full 32-bit RGBA color. This bakes
     * the palette at load time — runtime palette swaps (e.g., palette
     * cycling animations) are not supported. */
    if (SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        /* Count palette entries for debugging */
        SDL_Palette* pal = SDL_GetSurfacePalette(surface);
        int ncolors = pal ? pal->ncolors : 0;
        fprintf(stderr, "SDL3: %s is %d-bit indexed (%d colors), expanding to 32-bit\n",
                path, (int)SDL_BITSPERPIXEL(surface->format), ncolors);
    }

    /* Convert to XRGB8888 for GPU-friendly format.
     * For indexed surfaces this expands palette indices to RGB.
     * For RGB surfaces this ensures consistent pixel layout. */
    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_XRGB8888);
    SDL_DestroySurface(surface);

    if (!converted) {
        fprintf(stderr, "SDL3: Failed to convert %s to XRGB8888: %s\n", path, SDL_GetError());
        return nullptr;
    }

    return converted;
}

IDirectDrawSurface4::IDirectDrawSurface4()
    : texture(nullptr)
    , cpu_surface(nullptr)
    , width(0)
    , height(0)
    , color_key(0)
    , has_color_key(false)
{}

IDirectDrawSurface4::~IDirectDrawSurface4()
{
    if (texture) SDL_DestroyTexture(texture);
    if (cpu_surface) SDL_DestroySurface(cpu_surface);
}

int IDirectDrawSurface4::Release()
{
    delete this;
    return 0;
}

int IDirectDrawSurface4::Blt(const SDL_Rect* dst_rect,
                              IDirectDrawSurface4* src,
                              const SDL_Rect* src_rect,
                              uint32_t flags, DDBLTFX* fx)
{
    (void)flags;

    if (!texture) return -1;

    /* Color-fill blit (no source surface) */
    if (!src || !src->texture) {
        if (fx) {
            /* Extract RGBA from dwFillColor (assumed X8R8G8B8) */
            uint8_t r = (fx->dwFillColor >> 16) & 0xFF;
            uint8_t g = (fx->dwFillColor >> 8)  & 0xFF;
            uint8_t b =  fx->dwFillColor        & 0xFF;

            SDL_SetRenderTarget(g_sdl_ddraw->renderer, texture);
            SDL_SetRenderDrawColor(g_sdl_ddraw->renderer, r, g, b, 255);
            if (dst_rect) {
                SDL_FRect fr = { (float)dst_rect->x, (float)dst_rect->y,
                                 (float)dst_rect->w, (float)dst_rect->h };
                SDL_RenderFillRect(g_sdl_ddraw->renderer, &fr);
            } else {
                SDL_RenderFillRect(g_sdl_ddraw->renderer, nullptr);
            }
            SDL_SetRenderTarget(g_sdl_ddraw->renderer, nullptr);
        }
        return 0;
    }

    /* Texture-to-texture blit */
    SDL_SetRenderTarget(g_sdl_ddraw->renderer, texture);

    SDL_FRect dst = dst_rect
        ? SDL_FRect{ (float)dst_rect->x, (float)dst_rect->y,
                      (float)dst_rect->w, (float)dst_rect->h }
        : SDL_FRect{ 0.0f, 0.0f, (float)width, (float)height };

    const SDL_FRect* src_frect = nullptr;
    SDL_FRect src_fr;
    if (src_rect) {
        src_fr = { (float)src_rect->x, (float)src_rect->y,
                   (float)src_rect->w, (float)src_rect->h };
        src_frect = &src_fr;
    }

    /* Set color key modulation if source has one */
    if (src->has_color_key) {
        uint8_t r = (src->color_key >> 16) & 0xFF;
        uint8_t g = (src->color_key >> 8)  & 0xFF;
        uint8_t b =  src->color_key        & 0xFF;
        SDL_SetTextureColorMod(src->texture, r, g, b);
    }

    SDL_RenderTexture(g_sdl_ddraw->renderer, src->texture, src_frect, &dst);

    /* Reset color mod */
    if (src->has_color_key) {
        SDL_SetTextureColorMod(src->texture, 255, 255, 255);
    }

    SDL_SetRenderTarget(g_sdl_ddraw->renderer, nullptr);
    return 0;
}

int IDirectDrawSurface4::BltFast(int dx, int dy,
                                  IDirectDrawSurface4* src,
                                  const SDL_Rect* src_rect,
                                  uint32_t flags)
{
    SDL_Rect dst_rect = { dx, dy, 0, 0 };
    if (src_rect) {
        dst_rect.w = src_rect->w;
        dst_rect.h = src_rect->h;
    } else if (src) {
        dst_rect.w = src->width;
        dst_rect.h = src->height;
    }
    return Blt(&dst_rect, src, src_rect, flags, nullptr);
}

int IDirectDrawSurface4::Lock(const SDL_Rect* rect,
                               DDSURFACEDESC* desc,
                               uint32_t flags, void* unused)
{
    (void)rect; (void)flags; (void)unused;

    if (!texture) return -1;

    /* Create or refresh CPU surface from texture for pixel access */
    if (cpu_surface) SDL_DestroySurface(cpu_surface);

    cpu_surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_XRGB8888);
    if (!cpu_surface) return -1;

    /* Read back texture to CPU surface */
    SDL_SetRenderTarget(g_sdl_ddraw->renderer, texture);
    SDL_Surface* readback = SDL_RenderReadPixels(g_sdl_ddraw->renderer, nullptr);
    SDL_SetRenderTarget(g_sdl_ddraw->renderer, nullptr);
    if (readback) {
        /* Convert to our desired format and copy pixels */
        SDL_Surface* converted = SDL_ConvertSurface(readback, SDL_PIXELFORMAT_XRGB8888);
        SDL_DestroySurface(readback);
        if (converted) {
            SDL_DestroySurface(cpu_surface);
            cpu_surface = converted;
        }
    }

    if (desc) {
        desc->lpSurface = cpu_surface->pixels;
        desc->lPitch    = cpu_surface->pitch;
        desc->dwWidth   = (uint32_t)width;
        desc->dwHeight  = (uint32_t)height;
    }

    return 0;
}

int IDirectDrawSurface4::Unlock(const SDL_Rect* rect)
{
    (void)rect;

    if (!cpu_surface || !texture) return -1;

    /* Upload modified CPU surface back to texture */
    SDL_UpdateTexture(texture, nullptr,
                      cpu_surface->pixels, cpu_surface->pitch);

    SDL_DestroySurface(cpu_surface);
    cpu_surface = nullptr;

    return 0;
}

int IDirectDrawSurface4::SetColorKey(uint32_t flags, const DDCOLORKEY* key)
{
    (void)flags;
    if (key) {
        has_color_key = true;
        color_key     = key->dwColorSpaceLowValue;
    } else {
        has_color_key = false;
        color_key     = 0;
    }
    return 0;
}

int IDirectDrawSurface4::Restore()
{
    /* SDL3 surfaces don't get lost; no-op */
    return 0;
}

int IDirectDrawSurface4::IsLost()
{
    return 0; /* never lost in SDL3 */
}

int IDirectDrawSurface4::GetDC(void** hdc)
{
    (void)hdc;
    return -1; /* GDI DC not supported */
}

int IDirectDrawSurface4::ReleaseDC(void* hdc)
{
    (void)hdc;
    return -1;
}

int IDirectDrawSurface4::SetPalette(void* pal)
{
    (void)pal;
    /* Palette handling: no-op. SDL3 uses true-color surfaces;
     * palette-based rendering would need a shader or lookup table. */
    return 0;
}

int IDirectDrawSurface4::GetSurfaceDesc(DDSURFACEDESC* desc)
{
    if (!desc) return -1;
    *desc = DDSURFACEDESC{};
    desc->dwSize   = sizeof(*desc);
    desc->dwFlags  = DDSD_WIDTH | DDSD_HEIGHT;
    desc->dwWidth  = (uint32_t)width;
    desc->dwHeight = (uint32_t)height;
    return 0;
}

int IDirectDrawSurface4::GetPixelFormat(void* fmt)
{
    (void)fmt;
    return -1; /* not needed for our use cases */
}

/* =========================================================================
 * IDirectDraw4
 * ========================================================================= */

IDirectDraw4::IDirectDraw4()
    : window(nullptr)
    , renderer(nullptr)
    , owned(false)
{}

IDirectDraw4::~IDirectDraw4()
{
    if (owned) {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window)   SDL_DestroyWindow(window);
    }
}

int IDirectDraw4::Release()
{
    if (owned) {
        if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
        if (window)   { SDL_DestroyWindow(window);     window   = nullptr; }
    }
    delete this;
    return 0;
}

int IDirectDraw4::CreateSurface(DDSURFACEDESC* desc,
                                 IDirectDrawSurface4** out,
                                 void* unused)
{
    (void)unused;

    if (!desc || !out || !renderer) return -1;

    IDirectDrawSurface4* surf = new IDirectDrawSurface4();
    if (!surf) return -1;

    surf->width  = (int)desc->dwWidth;
    surf->height = (int)desc->dwHeight;

    surf->texture = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_XRGB8888,
                                      SDL_TEXTUREACCESS_TARGET,
                                      surf->width, surf->height);
    if (!surf->texture) {
        fprintf(stderr, "SDL3: CreateSurface(%dx%d) failed: %s\n",
                surf->width, surf->height, SDL_GetError());
        delete surf;
        return -1;
    }

    /* Enable blending for color-key transparency */
    SDL_SetTextureBlendMode(surf->texture, SDL_BLENDMODE_BLEND);

    *out = surf;
    return 0;
}

int IDirectDraw4::SetCooperativeLevel(void* hwnd, int level)
{
    (void)hwnd; (void)level;
    return 0;
}

int IDirectDraw4::SetDisplayMode(int w, int h, int bpp, int refresh, int flags)
{
    (void)w; (void)h; (void)bpp; (void)refresh; (void)flags;

    if (!window) return -1;

    /* SDL3 window size is managed through SDL_SetWindowSize;
     * display mode is set at window creation time. */
    SDL_SetWindowSize(window, w, h);
    return 0;
}

int IDirectDraw4::GetDeviceIdentifier(void* a, int b)
{
    (void)a; (void)b;
    return -1; /* stub */
}

/* =========================================================================
 * SDL primary-surface bridge
 * ========================================================================= */

bool SDL3_EnsurePrimarySurface()
{
    SDL_Renderer* renderer = SDL3_GetRenderer();
    SDL_Window* window = SDL3_GetWindow();
    if (!renderer || !window) return false;

    if (!g_sdl_ddraw) {
        g_sdl_ddraw = new IDirectDraw4();
        if (!g_sdl_ddraw) return false;
        g_sdl_ddraw->renderer = renderer;
        g_sdl_ddraw->window = window;
        g_sdl_ddraw->owned = false;
    }
    if (g_sdl_primary_surface && g_sdl_primary_surface->texture &&
        g_sdl_backbuffer && g_sdl_backbuffer->texture) return true;

    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSize(window, &width, &height) || width <= 0 || height <= 0) return false;

    DDSURFACEDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
    desc.dwWidth = static_cast<uint32_t>(width);
    desc.dwHeight = static_cast<uint32_t>(height);
    if (g_sdl_ddraw->CreateSurface(&desc, &g_sdl_primary_surface, nullptr) != 0 ||
        g_sdl_ddraw->CreateSurface(&desc, &g_sdl_backbuffer, nullptr) != 0) {
        if (g_sdl_primary_surface) { g_sdl_primary_surface->Release(); g_sdl_primary_surface = nullptr; }
        if (g_sdl_backbuffer) { g_sdl_backbuffer->Release(); g_sdl_backbuffer = nullptr; }
        return false;
    }

    SDL_SetRenderTarget(renderer, g_sdl_primary_surface->texture);
    SDL_SetRenderDrawColor(renderer, 0, 40, 80, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);
    return true;
}

IDirectDrawSurface4* SDL3_GetPrimarySurface()
{
    return SDL3_EnsurePrimarySurface() ? g_sdl_primary_surface : nullptr;
}

bool SDL3_PresentPrimarySurface()
{
    if (!SDL3_EnsurePrimarySurface()) return false;
    SDL_Renderer* renderer = g_sdl_ddraw->renderer;
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    if (!SDL_RenderClear(renderer) ||
        !SDL_RenderTexture(renderer, g_sdl_primary_surface->texture, nullptr, nullptr)) return false;
    SDL_RenderPresent(renderer);
    return true;
}

/* =========================================================================
 * DDRAW helper functions
 * ========================================================================= */

IDirectDrawSurface4* DDRAW_LoadBmpToSurface(
    const char* path, int bpp, int unk1, int unk2, char unk3)
{
    (void)unk1; (void)unk2; (void)unk3;

    if (!g_sdl_ddraw || !g_sdl_ddraw->renderer) return nullptr;

    SDL_Surface* raw = loadBmpToSdlSurface(path, bpp);
    if (!raw) return nullptr;

    g_last_bmp_width  = (uint32_t)raw->w;
    g_last_bmp_height = (uint32_t)raw->h;

    IDirectDrawSurface4* surf = new IDirectDrawSurface4();
    surf->width  = raw->w;
    surf->height = raw->h;

    surf->texture = SDL_CreateTextureFromSurface(g_sdl_ddraw->renderer, raw);
    SDL_DestroySurface(raw);

    if (!surf->texture) {
        fprintf(stderr, "SDL3: LoadBmpToSurface(%s) texture creation failed: %s\n",
                path, SDL_GetError());
        delete surf;
        return nullptr;
    }

    SDL_SetTextureBlendMode(surf->texture, SDL_BLENDMODE_BLEND);
    return surf;
}

int DDRAW_RestoreSurfaces(IDirectDrawSurface4* surf, void* desc)
{
    (void)surf; (void)desc;
    /* SDL3 surfaces are never lost */
    return 0;
}

void DDRAW_GetSurfaceWidthHeight(uint32_t* out_w, uint32_t* out_h)
{
    if (out_w) *out_w = g_last_bmp_width;
    if (out_h) *out_h = g_last_bmp_height;
}

void DDRAW_PresentRect(void* rect, void* hwnd, int* scroll, int force)
{
    (void)rect; (void)hwnd; (void)scroll; (void)force;

    SDL3_PresentPrimarySurface();
}

