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

#ifndef _WIN32
/* Host: DirectDraw 4 has no Windows build path in this tree; the whole
 * translation unit is host-only and compiles to nothing under MinGW. */

/* =========================================================================
 * Sdl3DirectDrawSurface — SDL3-backed IDirectDrawSurface4
 *
 * Wraps an SDL_Texture for GPU rendering and an optional SDL_Surface
 * for CPU-side pixel access (Lock/Unlock). The surface owns its
 * SDL resources and destroys them on Release().
 *
 * Overrides IDirectDrawSurface4 (platform/ddraw_interfaces.h) — real
 * DirectDraw method names/signatures, API-compatible only (no ABI/vtable-
 * slot accuracy target; see that header's scope note).
 * ========================================================================= */

struct Sdl3DirectDrawSurface : IDirectDrawSurface4 {
    SDL_Texture* texture;    /* GPU texture for Blit operations             */
    SDL_Surface* cpu_surface; /* CPU buffer for Lock/Unlock, or nullptr      */
    int          width;       /* Surface width in pixels                    */
    int          height;      /* Surface height in pixels                   */
    uint32_t     color_key;   /* Active source color key, or 0 if disabled  */
    bool         has_color_key;

    Sdl3DirectDrawSurface();
    ~Sdl3DirectDrawSurface() override;
    Sdl3DirectDrawSurface(const Sdl3DirectDrawSurface&) = delete;
    Sdl3DirectDrawSurface& operator=(const Sdl3DirectDrawSurface&) = delete;

    int32_t  QueryInterface(void* iid, void** object) override;
    uint32_t AddRef() override;
    uint32_t Release() override;

    HRESULT Blt(RECT* dest_rect, IDirectDrawSurface4* src_surface,
                RECT* src_rect, DWORD flags, DDBLTFX* fx) override;
    HRESULT BltFast(DWORD dx, DWORD dy, IDirectDrawSurface4* src_surface,
                     RECT* src_rect, DWORD flags) override;
    HRESULT Lock(RECT* rect, DDSURFACEDESC* desc, DWORD flags,
                 void* event_handle) override;
    HRESULT Unlock(RECT* rect) override;
    HRESULT GetSurfaceDesc(DDSURFACEDESC* desc) override;
    HRESULT GetPixelFormat(DDPIXELFORMAT* fmt) override;
    HRESULT SetColorKey(DWORD flags, const DDCOLORKEY* key) override;
    HRESULT Restore() override;
    HRESULT IsLost() override;
    HRESULT GetDC(void** hdc) override;
    HRESULT ReleaseDC(void* hdc) override;
    HRESULT SetPalette(void* palette) override;
    HRESULT GetAttachedSurface(void* caps, IDirectDrawSurface4** out) override;
    HRESULT EnumSurfaces(void* callback, void* context) override;
    HRESULT GetCaps(void* caps) override;
    HRESULT WaitForVerticalBlank(DWORD flags, void* event_handle) override;
};

/* =========================================================================
 * Sdl3DirectDraw4 — SDL3-backed IDirectDraw4
 *
 * Wraps SDL_Window + SDL_Renderer. CreateSurface creates SDL_Texture-
 * backed surfaces. This is a singleton; only one device exists.
 * ========================================================================= */

struct Sdl3DirectDraw4 : IDirectDraw4 {
    SDL_Window*   window;
    SDL_Renderer* renderer;
    bool          owned;    /* true if we created the window/renderer       */

    Sdl3DirectDraw4();
    ~Sdl3DirectDraw4() override;
    Sdl3DirectDraw4(const Sdl3DirectDraw4&) = delete;
    Sdl3DirectDraw4& operator=(const Sdl3DirectDraw4&) = delete;

    int32_t  QueryInterface(void* iid, void** object) override;
    uint32_t AddRef() override;
    uint32_t Release() override;

    HRESULT CreateSurface(DDSURFACEDESC* desc, IDirectDrawSurface4** out,
                           void* unused) override;
    HRESULT SetCooperativeLevel(void* hwnd, DWORD flags) override;
    HRESULT SetDisplayMode(DWORD width, DWORD height, DWORD bpp,
                            DWORD refresh_rate, DWORD flags) override;
    HRESULT GetDeviceIdentifier(void* identifier, DWORD flags) override;
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
 * separate from both the physical SDL display and untranslated DirectDraw globals.
 * Returns the concrete type (not just IDirectDrawSurface4*) because real callers
 * (ui/GameSetupPanel.cpp, tests) reach into .texture directly for SDL_SetRenderTarget —
 * host-only test/rendering code, not decompiled game logic, so this is not a
 * modeled-object-domain violation. */
bool SDL3_EnsurePrimarySurface();
Sdl3DirectDrawSurface* SDL3_GetPrimarySurface();

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

/** Same projection as SDL3_DisplayToPrimaryCanvas, but clamps into the
 * canvas instead of rejecting letterbox/pillarbox margins — for host
 * input paths (e.g. mode-3/9 WM_LBUTTONUP/WM_RBUTTONUP translation,
 * core/CGWND_sdl3.cpp) whose original writes are unconditional and would
 * receive an out-of-client-rect lParam under real Win32 mouse capture
 * rather than have the message dropped. Returns false only when
 * canvas/window setup is unavailable (same as the strict variant). */
bool SDL3_DisplayToPrimaryCanvasClamped(float display_x, float display_y,
                                        float* canvas_x, float* canvas_y);

/** Clear the SDL primary render target to an XRGB color. */
bool SDL3_ClearPrimarySurface(uint32_t xrgb);

/** Composite a decoded resource bitmap at native pixel coordinates. */
bool SDL3_BlitSurfaceToPrimary(SDL_Surface* source, int x, int y);

/** Composite a source rectangle from a decoded bitmap at native pixel coordinates. */
bool SDL3_BlitSurfaceRectToPrimary(SDL_Surface* source, const SDL_Rect& source_rect,
                                   int x, int y);

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

#endif /* _WIN32 */

#endif /* LOCO_SDL3_DDRAW_H */
