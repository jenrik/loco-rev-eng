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
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32

/* =========================================================================
 * Forward: SDL3 BMP loader
 * ========================================================================= */

/* BMP dimension cache (set by DDRAW_LoadBmpToSurface, read by helpers) */
static uint32_t g_last_bmp_width  = 0;
static uint32_t g_last_bmp_height = 0;

// Host-only SDL ownership. g_ddraw (the canonical decompiled-code-visible
// global, shared/stubs_impl.cpp) is pointed at this same device once it
// exists (see SDL3_EnsurePrimarySurface below) so real callers dispatching
// through g_ddraw by name (native/ddraw_surface_ops.c, input/Cursor.cpp,
// input/Cursor_Editor.cpp — all converted off raw vtable-slot dispatch,
// PROGRESS.md's DirectDraw-shim Phase 5 note) see a real object. The
// primary/backbuffer surfaces stay host-private (g_primary_surface/
// g_backbuffer remain null) — see that same PROGRESS.md note for the
// 16bpp-vs-XRGB8888 pixel-format mismatch blocking that half.
static Sdl3DirectDraw4* g_sdl_ddraw = nullptr;
static Sdl3DirectDrawSurface* g_sdl_primary_surface = nullptr;
static Sdl3DirectDrawSurface* g_sdl_backbuffer = nullptr;

/* Canonical global, real defining declaration in shared/stubs_impl.cpp
 * (0x485440). Declared void* there and at every consumer site — see this
 * file's own IDirectDraw4-shaped g_sdl_ddraw above for the typed object;
 * g_ddraw is just pointed at it below. */
extern void* g_ddraw;
static SDL3PrimaryPresentationMode g_primary_presentation_mode =
    SDL3PrimaryPresentationMode::Auto;

/* Real DirectDraw rects are (left, top, right, bottom), not (x, y, w, h) —
 * convert at the API boundary so the interface stays faithful to the real
 * Blt/Lock/Unlock signatures while the SDL3 backing keeps using SDL_Rect. */
static SDL_Rect to_sdl_rect(const RECT& r)
{
    return SDL_Rect{ r.left, r.top, r.right - r.left, r.bottom - r.top };
}

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

Sdl3DirectDrawSurface::Sdl3DirectDrawSurface()
    : texture(nullptr)
    , cpu_surface(nullptr)
    , width(0)
    , height(0)
    , color_key(0)
    , has_color_key(false)
{}

Sdl3DirectDrawSurface::~Sdl3DirectDrawSurface()
{
    if (texture) SDL_DestroyTexture(texture);
    if (cpu_surface) SDL_DestroySurface(cpu_surface);
}

int32_t Sdl3DirectDrawSurface::QueryInterface(void* iid, void** object)
{
    (void)iid;
    if (object) *object = nullptr;
    return -1; /* E_NOINTERFACE-equivalent: no real caller in this tree */
}

uint32_t Sdl3DirectDrawSurface::AddRef()
{
    return 1; /* refcounting not modeled; lifetime is Release()-owned */
}

uint32_t Sdl3DirectDrawSurface::Release()
{
    delete this;
    return 0;
}

HRESULT Sdl3DirectDrawSurface::Blt(RECT* dest_rect,
                                    IDirectDrawSurface4* src_surface,
                                    RECT* src_rect,
                                    DWORD flags, DDBLTFX* fx)
{
    (void)flags;

    if (!texture) return -1;

    auto* src = static_cast<Sdl3DirectDrawSurface*>(src_surface);
    const SDL_Rect dst_sdl = dest_rect ? to_sdl_rect(*dest_rect) : SDL_Rect{};
    const SDL_Rect src_sdl = src_rect ? to_sdl_rect(*src_rect) : SDL_Rect{};

    /* Color-fill blit (no source surface) */
    if (!src || !src->texture) {
        if (fx) {
            /* Extract RGBA from dwFillColor (assumed X8R8G8B8) */
            uint8_t r = (fx->dwFillColor >> 16) & 0xFF;
            uint8_t g = (fx->dwFillColor >> 8)  & 0xFF;
            uint8_t b =  fx->dwFillColor        & 0xFF;

            SDL_SetRenderTarget(g_sdl_ddraw->renderer, texture);
            SDL_SetRenderDrawColor(g_sdl_ddraw->renderer, r, g, b, 255);
            if (dest_rect) {
                SDL_FRect fr = { static_cast<float>(dst_sdl.x), static_cast<float>(dst_sdl.y),
                                 static_cast<float>(dst_sdl.w), static_cast<float>(dst_sdl.h) };
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

    SDL_FRect dst = dest_rect
        ? SDL_FRect{ static_cast<float>(dst_sdl.x), static_cast<float>(dst_sdl.y),
                      static_cast<float>(dst_sdl.w), static_cast<float>(dst_sdl.h) }
        : SDL_FRect{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };

    const SDL_FRect* src_frect = nullptr;
    SDL_FRect src_fr;
    if (src_rect) {
        src_fr = { static_cast<float>(src_sdl.x), static_cast<float>(src_sdl.y),
                   static_cast<float>(src_sdl.w), static_cast<float>(src_sdl.h) };
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

HRESULT Sdl3DirectDrawSurface::BltFast(DWORD dx, DWORD dy,
                                        IDirectDrawSurface4* src_surface,
                                        RECT* src_rect,
                                        DWORD flags)
{
    auto* src = static_cast<Sdl3DirectDrawSurface*>(src_surface);
    int w = 0, h = 0;
    if (src_rect) {
        w = src_rect->right - src_rect->left;
        h = src_rect->bottom - src_rect->top;
    } else if (src) {
        w = src->width;
        h = src->height;
    }
    RECT dest_rect{ static_cast<int32_t>(dx), static_cast<int32_t>(dy),
                     static_cast<int32_t>(dx) + w, static_cast<int32_t>(dy) + h };
    return Blt(&dest_rect, src_surface, src_rect, flags, nullptr);
}

HRESULT Sdl3DirectDrawSurface::Lock(RECT* rect,
                                     DDSURFACEDESC* desc,
                                     DWORD flags, void* event_handle)
{
    (void)rect; (void)flags; (void)event_handle;

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
        desc->dwWidth   = static_cast<uint32_t>(width);
        desc->dwHeight  = static_cast<uint32_t>(height);
    }

    return 0;
}

HRESULT Sdl3DirectDrawSurface::Unlock(RECT* rect)
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

HRESULT Sdl3DirectDrawSurface::SetColorKey(DWORD flags, const DDCOLORKEY* key)
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

HRESULT Sdl3DirectDrawSurface::Restore()
{
    /* SDL3 surfaces don't get lost; no-op */
    return 0;
}

HRESULT Sdl3DirectDrawSurface::IsLost()
{
    return 0; /* never lost in SDL3 */
}

HRESULT Sdl3DirectDrawSurface::GetDC(void** hdc)
{
    (void)hdc;
    return -1; /* GDI DC not supported */
}

HRESULT Sdl3DirectDrawSurface::ReleaseDC(void* hdc)
{
    (void)hdc;
    return -1;
}

HRESULT Sdl3DirectDrawSurface::SetPalette(IDirectDrawPalette* palette)
{
    (void)palette;
    /* Palette handling: no-op. SDL3 uses true-color surfaces;
     * palette-based rendering would need a shader or lookup table. */
    return 0;
}

HRESULT Sdl3DirectDrawSurface::GetSurfaceDesc(DDSURFACEDESC* desc)
{
    if (!desc) return -1;
    *desc = DDSURFACEDESC{};
    desc->dwSize   = sizeof(*desc);
    desc->dwFlags  = DDSD_WIDTH | DDSD_HEIGHT;
    desc->dwWidth  = static_cast<uint32_t>(width);
    desc->dwHeight = static_cast<uint32_t>(height);
    return 0;
}

HRESULT Sdl3DirectDrawSurface::GetPixelFormat(DDPIXELFORMAT* fmt)
{
    (void)fmt;
    return -1; /* not needed for our use cases */
}

HRESULT Sdl3DirectDrawSurface::GetAttachedSurface(void* caps, IDirectDrawSurface4** out)
{
    (void)caps;
    /* No real caller today; primary/backbuffer are tracked separately by
     * the free-function bridge below, not via surface attachment chains. */
    if (out) *out = nullptr;
    return -1;
}

HRESULT Sdl3DirectDrawSurface::EnumSurfaces(void* callback, void* context)
{
    (void)callback; (void)context;
    return -1; /* not needed for our use cases */
}

HRESULT Sdl3DirectDrawSurface::GetCaps(void* caps)
{
    (void)caps;
    return -1; /* not needed for our use cases */
}

HRESULT Sdl3DirectDrawSurface::WaitForVerticalBlank(DWORD flags, void* event_handle)
{
    (void)flags; (void)event_handle;
    return 0; /* no real vsync modeling; report success like a no-op wait */
}

/* =========================================================================
 * Sdl3DirectDrawPalette
 * ========================================================================= */

Sdl3DirectDrawPalette::Sdl3DirectDrawPalette()
{
    std::memset(entries, 0, sizeof(entries));
}

int32_t Sdl3DirectDrawPalette::QueryInterface(void* iid, void** object)
{
    (void)iid;
    if (object) *object = nullptr;
    return -1; /* E_NOINTERFACE-equivalent: no real caller in this tree */
}

uint32_t Sdl3DirectDrawPalette::AddRef()
{
    return 1; /* refcounting not modeled; lifetime is Release()-owned */
}

uint32_t Sdl3DirectDrawPalette::Release()
{
    delete this;
    return 0;
}

HRESULT Sdl3DirectDrawPalette::SetEntries(DWORD flags, DWORD start, DWORD count, void* entries_in)
{
    (void)flags;
    if (!entries_in || start >= kMaxEntries) return -1;
    const uint32_t n = std::min<uint32_t>(count, kMaxEntries - start);
    std::memcpy(&entries[start], entries_in, n * sizeof(entries[0]));
    return 0;
}

HRESULT Sdl3DirectDrawPalette::GetEntries(DWORD flags, DWORD start, DWORD count, void* entries_out)
{
    (void)flags;
    if (!entries_out || start >= kMaxEntries) return -1;
    const uint32_t n = std::min<uint32_t>(count, kMaxEntries - start);
    std::memcpy(entries_out, &entries[start], n * sizeof(entries[0]));
    return 0;
}

/* =========================================================================
 * Sdl3DirectDrawClipper
 * ========================================================================= */

int32_t Sdl3DirectDrawClipper::QueryInterface(void* iid, void** object)
{
    (void)iid;
    if (object) *object = nullptr;
    return -1; /* E_NOINTERFACE-equivalent: no real caller in this tree */
}

uint32_t Sdl3DirectDrawClipper::AddRef()
{
    return 1; /* refcounting not modeled; lifetime is Release()-owned */
}

uint32_t Sdl3DirectDrawClipper::Release()
{
    delete this;
    return 0;
}

/* =========================================================================
 * IDirectDraw4
 * ========================================================================= */

Sdl3DirectDraw4::Sdl3DirectDraw4()
    : window(nullptr)
    , renderer(nullptr)
    , owned(false)
{}

Sdl3DirectDraw4::~Sdl3DirectDraw4()
{
    if (owned) {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window)   SDL_DestroyWindow(window);
    }
}

int32_t Sdl3DirectDraw4::QueryInterface(void* iid, void** object)
{
    (void)iid;
    if (object) *object = nullptr;
    return -1; /* E_NOINTERFACE-equivalent: no real caller in this tree */
}

uint32_t Sdl3DirectDraw4::AddRef()
{
    return 1; /* refcounting not modeled; lifetime is Release()-owned */
}

uint32_t Sdl3DirectDraw4::Release()
{
    if (owned) {
        if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
        if (window)   { SDL_DestroyWindow(window);     window   = nullptr; }
    }
    delete this;
    return 0;
}

HRESULT Sdl3DirectDraw4::CreateSurface(DDSURFACEDESC* desc,
                                        IDirectDrawSurface4** out,
                                        void* unused)
{
    (void)unused;

    if (!desc || !out || !renderer) return -1;

    Sdl3DirectDrawSurface* surf = new Sdl3DirectDrawSurface();
    if (!surf) return -1;

    surf->width  = static_cast<int>(desc->dwWidth);
    surf->height = static_cast<int>(desc->dwHeight);

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

HRESULT Sdl3DirectDraw4::CreatePalette(DWORD flags, void* color_array,
                                        IDirectDrawPalette** out, void* unused)
{
    (void)unused;
    if (!out) return -1;

    Sdl3DirectDrawPalette* pal = new Sdl3DirectDrawPalette();
    if (color_array) {
        pal->SetEntries(flags, 0, Sdl3DirectDrawPalette::kMaxEntries, color_array);
    }
    *out = pal;
    return 0;
}

HRESULT Sdl3DirectDraw4::CreateClipper(DWORD flags, IDirectDrawClipper** out,
                                        void* unused)
{
    (void)flags; (void)unused;
    if (!out) return -1;
    *out = new Sdl3DirectDrawClipper();
    return 0;
}

HRESULT Sdl3DirectDraw4::SetCooperativeLevel(void* hwnd, DWORD flags)
{
    (void)hwnd; (void)flags;
    return 0;
}

HRESULT Sdl3DirectDraw4::SetDisplayMode(DWORD width, DWORD height, DWORD bpp,
                                         DWORD refresh_rate, DWORD flags)
{
    (void)bpp; (void)refresh_rate; (void)flags;

    if (!window) return -1;

    /* SDL3 window size is managed through SDL_SetWindowSize;
     * display mode is set at window creation time. */
    SDL_SetWindowSize(window, static_cast<int>(width), static_cast<int>(height));
    return 0;
}

HRESULT Sdl3DirectDraw4::GetDeviceIdentifier(void* identifier, DWORD flags)
{
    (void)identifier; (void)flags;
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
        g_sdl_ddraw = new Sdl3DirectDraw4();
        if (!g_sdl_ddraw) return false;
        g_sdl_ddraw->renderer = renderer;
        g_sdl_ddraw->window = window;
        g_sdl_ddraw->owned = false;
        g_ddraw = g_sdl_ddraw;
    }
    if (g_sdl_primary_surface && g_sdl_primary_surface->texture &&
        g_sdl_backbuffer && g_sdl_backbuffer->texture) return true;

    // EditWindow::render (0x4216F0) creates a 0x500 x 0x400 surface.
    // Keep that binary coordinate space independent of window size so the
    // display projection cannot clip UI elements on smaller windows.
    DDSURFACEDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
    desc.dwWidth = SDL3_PRIMARY_CANVAS_WIDTH;
    desc.dwHeight = SDL3_PRIMARY_CANVAS_HEIGHT;
    IDirectDrawSurface4* primary_iface = nullptr;
    IDirectDrawSurface4* backbuffer_iface = nullptr;
    if (g_sdl_ddraw->CreateSurface(&desc, &primary_iface, nullptr) != 0 ||
        g_sdl_ddraw->CreateSurface(&desc, &backbuffer_iface, nullptr) != 0) {
        if (primary_iface) primary_iface->Release();
        if (backbuffer_iface) backbuffer_iface->Release();
        g_sdl_primary_surface = nullptr;
        g_sdl_backbuffer = nullptr;
        return false;
    }
    g_sdl_primary_surface = static_cast<Sdl3DirectDrawSurface*>(primary_iface);
    g_sdl_backbuffer = static_cast<Sdl3DirectDrawSurface*>(backbuffer_iface);

    SDL_SetRenderTarget(renderer, g_sdl_primary_surface->texture);
    SDL_SetRenderDrawColor(renderer, 0, 40, 80, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);
    return true;
}

Sdl3DirectDrawSurface* SDL3_GetPrimarySurface()
{
    return SDL3_EnsurePrimarySurface() ? g_sdl_primary_surface : nullptr;
}

bool SDL3_ClearPrimarySurface(uint32_t xrgb)
{
    if (!SDL3_EnsurePrimarySurface()) return false;
    SDL_Renderer* renderer = g_sdl_ddraw->renderer;
    SDL_SetRenderTarget(renderer, g_sdl_primary_surface->texture);
    SDL_SetRenderDrawColor(renderer, (xrgb >> 16) & 0xFF,
                            (xrgb >> 8) & 0xFF, xrgb & 0xFF, 255);
    const bool cleared = SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);
    return cleared;
}

bool SDL3_BlitSurfaceToPrimary(SDL_Surface* source, int x, int y)
{
    if (!source || !SDL3_EnsurePrimarySurface()) return false;
    SDL_Renderer* renderer = g_sdl_ddraw->renderer;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, source);
    if (!texture) return false;

    SDL_FRect destination = { static_cast<float>(x), static_cast<float>(y),
                              static_cast<float>(source->w), static_cast<float>(source->h) };
    SDL_SetRenderTarget(renderer, g_sdl_primary_surface->texture);
    const bool rendered = SDL_RenderTexture(renderer, texture, nullptr, &destination);
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(texture);
    return rendered;
}

bool SDL3_BlitSurfaceRectToPrimary(SDL_Surface* source, const SDL_Rect& source_rect,
                                   int x, int y)
{
    if (!source || source_rect.x < 0 || source_rect.y < 0 || source_rect.w <= 0 ||
        source_rect.h <= 0 || source_rect.x + source_rect.w > source->w ||
        source_rect.y + source_rect.h > source->h || !SDL3_EnsurePrimarySurface()) {
        return false;
    }

    SDL_Renderer* renderer = g_sdl_ddraw->renderer;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, source);
    if (!texture) return false;

    const SDL_FRect source_rect_f = {static_cast<float>(source_rect.x), static_cast<float>(source_rect.y),
                                      static_cast<float>(source_rect.w), static_cast<float>(source_rect.h)};
    const SDL_FRect destination = {static_cast<float>(x), static_cast<float>(y),
                                   static_cast<float>(source_rect.w),
                                   static_cast<float>(source_rect.h)};
    SDL_SetRenderTarget(renderer, g_sdl_primary_surface->texture);
    const bool rendered = SDL_RenderTexture(renderer, texture, &source_rect_f, &destination);
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(texture);
    return rendered;
}

bool SDL3_DrawPrimaryTextInput(int left, int top, int right, int bottom,
                               const char* text, bool focused)
{
    if (!text || right <= left || bottom <= top || !SDL3_EnsurePrimarySurface()) return false;

    SDL_Renderer* renderer = g_sdl_ddraw->renderer;
    SDL_SetRenderTarget(renderer, g_sdl_primary_surface->texture);

    // UI_MainMenu_Create (0x4204D0) creates an EDIT child with
    // WS_EX_CLIENTEDGE. This is the SDL canvas equivalent of that native
    // recessed light field; it deliberately uses the assembly-derived RECT.
    const SDL_FRect outer = {static_cast<float>(left), static_cast<float>(top),
                             static_cast<float>(right - left), static_cast<float>(bottom - top)};
    SDL_SetRenderDrawColor(renderer, 0x3c, 0x3c, 0x3c, 255);
    bool rendered = SDL_RenderFillRect(renderer, &outer);
    const SDL_FRect inner = {static_cast<float>(left + 2), static_cast<float>(top + 2),
                             static_cast<float>(right - left - 4), static_cast<float>(bottom - top - 4)};
    SDL_SetRenderDrawColor(renderer, focused ? 0xff : 0xe8, focused ? 0xff : 0xe8,
                           focused ? 0xff : 0xe8, 255);
    rendered = SDL_RenderFillRect(renderer, &inner) && rendered;

    // SDL_RenderDebugText is SDL3's fixed 8x8 ASCII bitmap font. Scale it to
    // 16px so the host field retains the original 33px control height.
    SDL_SetRenderScale(renderer, 2.0f, 2.0f);
    SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 255);
    rendered = SDL_RenderDebugText(renderer, static_cast<float>(left + 7) / 2.0f,
                                   static_cast<float>(top + 8) / 2.0f, text) && rendered;
    if (focused && std::strlen(text) < 11) {
        const float caret_x = static_cast<float>(left + 7 + std::strlen(text) * 16) / 2.0f;
        rendered = SDL_RenderDebugText(renderer, caret_x, static_cast<float>(top + 8) / 2.0f, "_") && rendered;
    }
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_SetRenderTarget(renderer, nullptr);
    return rendered;
}

static bool primary_presentation_rect(SDL_Renderer* renderer, SDL_FRect* destination,
                                      int* output_width, int* output_height)
{
    // SDL_GetRenderOutputSize reports physical output pixels even when a
    // render target is bound, so this handles resizable and high-DPI windows.
    int width = 0;
    int height = 0;
    if (!renderer || !destination || !SDL_GetRenderOutputSize(renderer, &width, &height) ||
        width <= 0 || height <= 0) return false;

    float scale = 1.0f;
    if (g_primary_presentation_mode == SDL3PrimaryPresentationMode::Fit ||
        (g_primary_presentation_mode == SDL3PrimaryPresentationMode::Auto &&
         (width < SDL3_PRIMARY_CANVAS_WIDTH || height < SDL3_PRIMARY_CANVAS_HEIGHT))) {
        const float scale_x = static_cast<float>(width) / SDL3_PRIMARY_CANVAS_WIDTH;
        const float scale_y = static_cast<float>(height) / SDL3_PRIMARY_CANVAS_HEIGHT;
        scale = scale_x < scale_y ? scale_x : scale_y;
    }

    *destination = {
        (width - SDL3_PRIMARY_CANVAS_WIDTH * scale) * 0.5f,
        (height - SDL3_PRIMARY_CANVAS_HEIGHT * scale) * 0.5f,
        SDL3_PRIMARY_CANVAS_WIDTH * scale,
        SDL3_PRIMARY_CANVAS_HEIGHT * scale,
    };
    if (output_width) *output_width = width;
    if (output_height) *output_height = height;
    return true;
}

void SDL3_SetPrimaryPresentationMode(SDL3PrimaryPresentationMode mode)
{
    g_primary_presentation_mode = mode;
}

SDL3PrimaryPresentationMode SDL3_GetPrimaryPresentationMode()
{
    return g_primary_presentation_mode;
}

bool SDL3_PresentPrimarySurface()
{
    if (!SDL3_EnsurePrimarySurface()) return false;
    SDL_Renderer* renderer = g_sdl_ddraw->renderer;
    SDL_FRect destination{};
    if (!primary_presentation_rect(renderer, &destination, nullptr, nullptr)) return false;

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    if (!SDL_RenderClear(renderer) ||
        !SDL_RenderTexture(renderer, g_sdl_primary_surface->texture, nullptr, &destination)) return false;
    SDL_RenderPresent(renderer);
    return true;
}

bool SDL3_DisplayToPrimaryCanvas(float display_x, float display_y,
                                 float* canvas_x, float* canvas_y)
{
    if (!canvas_x || !canvas_y || !SDL3_EnsurePrimarySurface()) return false;

    SDL_FRect destination{};
    int output_width = 0;
    int output_height = 0;
    if (!primary_presentation_rect(g_sdl_ddraw->renderer, &destination,
                                   &output_width, &output_height)) return false;

    // SDL mouse events use window coordinates; convert them to the physical
    // render-output coordinate system before reversing the projection.
    int window_width = 0;
    int window_height = 0;
    if (!SDL_GetWindowSize(g_sdl_ddraw->window, &window_width, &window_height) ||
        window_width <= 0 || window_height <= 0) return false;
    const float output_x = display_x * output_width / window_width;
    const float output_y = display_y * output_height / window_height;
    if (output_x < destination.x || output_y < destination.y ||
        output_x >= destination.x + destination.w ||
        output_y >= destination.y + destination.h) return false;

    *canvas_x = (output_x - destination.x) * SDL3_PRIMARY_CANVAS_WIDTH / destination.w;
    *canvas_y = (output_y - destination.y) * SDL3_PRIMARY_CANVAS_HEIGHT / destination.h;
    return true;
}

bool SDL3_DisplayToPrimaryCanvasClamped(float display_x, float display_y,
                                        float* canvas_x, float* canvas_y)
{
    if (!canvas_x || !canvas_y || !SDL3_EnsurePrimarySurface()) return false;

    SDL_FRect destination{};
    int output_width = 0;
    int output_height = 0;
    if (!primary_presentation_rect(g_sdl_ddraw->renderer, &destination,
                                   &output_width, &output_height)) return false;

    int window_width = 0;
    int window_height = 0;
    if (!SDL_GetWindowSize(g_sdl_ddraw->window, &window_width, &window_height) ||
        window_width <= 0 || window_height <= 0) return false;
    const float output_x = display_x * output_width / window_width;
    const float output_y = display_y * output_height / window_height;

    // Unlike SDL3_DisplayToPrimaryCanvas, never reject: MainWndProc's
    // WM_LBUTTONUP/WM_RBUTTONUP handlers (0x4623A7/0x462404) write
    // unconditionally, and Windows would happily deliver an out-of-client-
    // rect lParam under mouse capture. Clamp into the letterboxed canvas
    // instead of dropping the release, matching that unconditional write
    // as closely as a host without real capture/grab semantics can.
    const float clamped_output_x =
        std::clamp(output_x, destination.x, destination.x + destination.w - 1.0f);
    const float clamped_output_y =
        std::clamp(output_y, destination.y, destination.y + destination.h - 1.0f);

    *canvas_x = (clamped_output_x - destination.x) * SDL3_PRIMARY_CANVAS_WIDTH / destination.w;
    *canvas_y = (clamped_output_y - destination.y) * SDL3_PRIMARY_CANVAS_HEIGHT / destination.h;
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

    g_last_bmp_width  = static_cast<uint32_t>(raw->w);
    g_last_bmp_height = static_cast<uint32_t>(raw->h);

    Sdl3DirectDrawSurface* surf = new Sdl3DirectDrawSurface();
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


#endif /* _WIN32 */
