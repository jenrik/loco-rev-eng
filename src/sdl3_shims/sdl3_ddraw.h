/**
 * sdl3_ddraw.h — DirectDraw 4 → SDL3 compatibility shim
 *
 * Provides IDirectDraw4 and IDirectDrawSurface4 implementations
 * backed by SDL3 renderer and textures. Drop-in replacement for
 * the type-only stubs/ddraw.h.
 *
 * NOT part of the Lego Loco reverse-engineering project.
 */

#ifndef LOCO_SDL3_DDRAW_H
#define LOCO_SDL3_DDRAW_H

#include "sdl3_types.h"
#include <SDL3/SDL.h>

/* =========================================================================
 * Forward declarations
 * ========================================================================= */

struct IDirectDrawSurface4;
struct IDirectDraw4;
struct IDirectDrawPalette;
struct IDirectDrawClipper;

/* =========================================================================
 * IDirectDrawSurface4 — SDL3-backed surface
 *
 * Wraps an SDL_Texture for GPU rendering and an optional SDL_Surface
 * for CPU-side pixel access (Lock/Unlock). The surface owns its
 * SDL resources and destroys them on Release().
 * ========================================================================= */

struct IDirectDrawSurface4 {
    SDL_Texture* texture;    /* GPU texture for Blit operations             */
    SDL_Surface* cpu_surface; /* CPU buffer for Lock/Unlock, or nullptr      */
    int          width;       /* Surface width in pixels                    */
    int          height;      /* Surface height in pixels                   */
    uint32_t     color_key;   /* Active source color key, or 0 if disabled  */
    bool         has_color_key;

    IDirectDrawSurface4();
    ~IDirectDrawSurface4();

    int  Release();
    int  Blt(const SDL_Rect* dst_rect, IDirectDrawSurface4* src,
             const SDL_Rect* src_rect, uint32_t flags, DDBLTFX* fx);
    int  BltFast(int dx, int dy, IDirectDrawSurface4* src,
                 const SDL_Rect* src_rect, uint32_t flags);
    int  Lock(const SDL_Rect* rect, DDSURFACEDESC* desc,
              uint32_t flags, void* unused);
    int  Unlock(const SDL_Rect* rect);
    int  SetColorKey(uint32_t flags, const DDCOLORKEY* key);
    int  Restore();
    int  IsLost();
    int  GetDC(void** hdc);
    int  ReleaseDC(void* hdc);
    int  SetPalette(void* pal);
    int  GetSurfaceDesc(DDSURFACEDESC* desc);
    int  GetPixelFormat(void* fmt);
};

/* =========================================================================
 * IDirectDraw4 — SDL3-backed DirectDraw device
 *
 * Wraps SDL_Window + SDL_Renderer. CreateSurface creates SDL_Texture-
 * backed surfaces. This is a singleton; only one device exists.
 * ========================================================================= */

struct IDirectDraw4 {
    SDL_Window*   window;
    SDL_Renderer* renderer;
    bool          owned;    /* true if we created the window/renderer       */

    IDirectDraw4();
    ~IDirectDraw4();

    int  Release();
    int  CreateSurface(DDSURFACEDESC* desc, IDirectDrawSurface4** out,
                       void* unused);
    int  SetCooperativeLevel(void* hwnd, int level);
    int  SetDisplayMode(int w, int h, int bpp, int refresh, int flags);
    int  GetDeviceIdentifier(void* a, int b);
};

/* =========================================================================
 * SDL renderer bridge
 * ========================================================================= */

/** The translated game renders in the binary's fixed 1280x1024 coordinate
 * space. The canvas is separate from the physical SDL window. */
constexpr int SDL3_PRIMARY_CANVAS_WIDTH = 1280;
constexpr int SDL3_PRIMARY_CANVAS_HEIGHT = 1024;

enum class SDL3PrimaryPresentationMode {
    Auto,          // Pixel-perfect when it fits; otherwise aspect-preserving fit.
    PixelPerfect,  // Center the canvas at 1:1, intentionally clipping if required.
    Fit,           // Always scale uniformly to fit the display with letterboxing.
};

/** Ensure fixed-size canvas and backbuffer targets exist. They are deliberately
 * separate from both the physical SDL display and untranslated DirectDraw globals. */
bool SDL3_EnsurePrimarySurface();
IDirectDrawSurface4* SDL3_GetPrimarySurface();

/** Select how the logical canvas is projected to the SDL output. */
void SDL3_SetPrimaryPresentationMode(SDL3PrimaryPresentationMode mode);
SDL3PrimaryPresentationMode SDL3_GetPrimaryPresentationMode();

/** Project the logical canvas to the SDL window and present it.
 * Returns false only when canvas/window setup is unavailable. */
bool SDL3_PresentPrimarySurface();

/** Convert an SDL window-coordinate pointer position to logical canvas space.
 * Returns false for letterbox/pillarbox margins or unavailable display state. */
bool SDL3_DisplayToPrimaryCanvas(float display_x, float display_y,
                                 float* canvas_x, float* canvas_y);

/** Clear the SDL primary render target to an XRGB color. */
bool SDL3_ClearPrimarySurface(uint32_t xrgb);

/** Composite a decoded resource bitmap at native pixel coordinates. */
bool SDL3_BlitSurfaceToPrimary(SDL_Surface* source, int x, int y);

/** Draw the host replacement for the original native EDIT control directly
 * onto the fixed primary canvas. Coordinates are logical canvas pixels. */
bool SDL3_DrawPrimaryTextInput(int left, int top, int right, int bottom,
                               const char* text, bool focused);

/* =========================================================================
 * DDRAW helper functions
 *
 * These are free functions called directly by the decompiled C++ code.
 * They correspond to the identically-named functions declared in the
 * stub headers and implemented by the original loco.exe.
 * ========================================================================= */

IDirectDrawSurface4* DDRAW_LoadBmpToSurface(
    const char* path, int bpp, int unk1, int unk2, char unk3);

int  DDRAW_RestoreSurfaces(IDirectDrawSurface4* surf, void* desc);

void DDRAW_GetSurfaceWidthHeight(uint32_t* out_w, uint32_t* out_h);

void DDRAW_PresentRect(void* rect, void* hwnd, int* scroll, int force);

#endif /* LOCO_SDL3_DDRAW_H */
