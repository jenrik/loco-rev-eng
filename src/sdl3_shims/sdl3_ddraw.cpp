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
static SDL3PrimaryPresentationMode g_primary_presentation_mode =
    SDL3PrimaryPresentationMode::Auto;

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

    // EditWindow::render (0x4216F0) creates a 0x500 x 0x400 surface.
    // Keep that binary coordinate space independent of window size so the
    // display projection cannot clip UI elements on smaller windows.
    DDSURFACEDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
    desc.dwWidth = SDL3_PRIMARY_CANVAS_WIDTH;
    desc.dwHeight = SDL3_PRIMARY_CANVAS_HEIGHT;
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

