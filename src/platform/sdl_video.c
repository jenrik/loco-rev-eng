/*
 * Lego Loco (1998) - Native Linux Port
 * src/platform/sdl_video.c — SDL2 replacement for DirectDraw surface management
 *
 * Replaces the following Win32 subsystems documented in src/graphics/ddraw_init.c:
 *   DDRAW.DLL  DirectDrawCreate                    -> SDL_CreateRenderer
 *   DDRAW.DLL  IDirectDraw::SetCooperativeLevel    -> SDL_SetWindowFullscreen
 *   DDRAW.DLL  IDirectDraw::SetDisplayMode         -> SDL_RenderSetLogicalSize
 *   DDRAW.DLL  IDirectDraw::CreateSurface          -> SDL_CreateTexture
 *   DDRAW.DLL  IDirectDrawSurface::BltFast         -> SDL_RenderCopy
 *   DDRAW.DLL  IDirectDrawSurface::Blt             -> SDL_RenderCopy
 *   DDRAW.DLL  IDirectDrawSurface::Flip            -> SDL_RenderPresent
 *   DDRAW.DLL  IDirectDrawSurface::Lock/Unlock     -> SDL_LockTexture/SDL_UnlockTexture
 *   DDRAW.DLL  IDirectDrawSurface::GetDC/ReleaseDC -> SDL_LockSurface/SDL_UnlockSurface
 *   DDRAW.DLL  IDirectDrawSurface::SetColorKey     -> SDL_SetColorKey
 *   DDRAW.DLL  IDirectDrawSurface::Restore         -> no-op (SDL never loses surfaces)
 *   DDRAW.DLL  IDirectDraw::CreateClipper          -> no-op (use SDL_Rect on blits)
 *   GDI32.DLL  LoadImageA / GetObjectA / StretchBlt -> SDL_LoadBMP + SDL_BlitScaled
 *   USER32.DLL GetSystemMetrics(SM_CXSCREEN)       -> SDL_GetCurrentDisplayMode
 *   USER32.DLL SendMessageA (WM_USER+7 frame msg)  -> SDL_RenderPresent
 *
 * WIN32 → LINUX API mapping table:
 *
 *   DirectDrawCreate(NULL, &g_pDD, NULL)
 *     -> g_sdlRenderer already created by sdl_window.c Platform_CreateWindow
 *        (SDL2 has no separate "DirectDraw object" — the renderer IS the device)
 *
 *   IDirectDraw::SetCooperativeLevel(hwnd, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN)
 *     -> SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN)
 *
 *   IDirectDraw::SetCooperativeLevel(hwnd, DDSCL_NORMAL)
 *     -> SDL_SetWindowFullscreen(window, 0)
 *
 *   IDirectDraw::SetDisplayMode(640, 480, 16)
 *     -> SDL_SetWindowSize(window, 640, 480)
 *        SDL_RenderSetLogicalSize(renderer, 640, 480)
 *
 *   IDirectDraw::CreateSurface(DDSCAPS_PRIMARYSURFACE)
 *     -> SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
 *                          SDL_TEXTUREACCESS_TARGET, 640, 480)
 *        set as render target with SDL_SetRenderTarget
 *
 *   IDirectDraw::CreateSurface(DDSCAPS_OFFSCREENPLAIN, w, h)
 *     -> SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
 *                          SDL_TEXTUREACCESS_STREAMING, w, h)
 *        for CPU-writable surfaces; STATIC for read-only sprite surfaces
 *
 *   IDirectDrawSurface::BltFast(x, y, src, rect, DDBLTFAST_SRCCOLORKEY)
 *     -> SDL_SetTextureBlendMode(srcTex, SDL_BLENDMODE_BLEND)
 *        SDL_RenderCopy(renderer, srcTex, &srcSDLRect, &dstSDLRect)
 *
 *   IDirectDrawSurface::Blt(&dst, src, &src, DDBLT_WAIT, NULL)
 *     -> SDL_RenderCopy(renderer, srcTex, &srcRect, &dstRect)
 *
 *   IDirectDrawSurface::Flip(NULL, DDFLIP_WAIT)
 *     -> SDL_RenderPresent(renderer)
 *
 *   IDirectDrawSurface::Lock(&desc)     [CPU pixel access]
 *     -> SDL_LockTexture(tex, NULL, &pixels, &pitch)
 *
 *   IDirectDrawSurface::Unlock(NULL)
 *     -> SDL_UnlockTexture(tex)
 *
 *   IDirectDrawSurface::GetDC(&hdc)    [GDI compat DC access]
 *     -> SDL_LockSurface(surface)  then direct pixel write via surface->pixels
 *        (no GDI HDC equivalent; use SDL_FillRect or pixel arithmetic instead)
 *
 *   IDirectDrawSurface::ReleaseDC(hdc)
 *     -> SDL_UnlockSurface(surface)
 *
 *   IDirectDrawSurface::SetColorKey(DDCKEY_SRCBLT, &ck)
 *     -> SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(fmt, 255, 0, 255))
 *        Magenta: LOCO_COLOR_KEY_R=255, LOCO_COLOR_KEY_G=0, LOCO_COLOR_KEY_B=255
 *
 *   IDirectDrawSurface::Restore()      [recover from DDERR_SURFACELOST]
 *     -> no-op; SDL2 does not have a "lost surface" concept
 *
 *   LoadImageA + StretchBlt            [load BMP to surface]
 *     -> SDL_LoadBMP(path) + SDL_BlitScaled
 *
 *   GetFileAttributesA(path)           [check file exists]
 *     -> access(path, F_OK)  (unistd.h)
 *
 *   GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)
 *     -> SDL_GetCurrentDisplayMode(0, &mode); mode.w / mode.h
 *
 *   SendMessageA(hwnd, WM_USER+7, param, 0)   [trigger frame present]
 *     -> SDL_RenderPresent(renderer)  called directly in game loop
 *
 *   OutputDebugStringA("message")
 *     -> fprintf(stderr, "message\n")
 *
 * Pixel format notes:
 *   The game internally uses 16-bit RGB565 (g_pixFmtId = 0x235 in ddraw_init.c).
 *   SDL2 textures should use SDL_PIXELFORMAT_RGB565 for streaming surfaces to
 *   match the game's pixel arithmetic.  The primary render target can use
 *   SDL_PIXELFORMAT_ARGB8888 for compatibility; the renderer handles conversion.
 *
 * Surface dimension reference:
 *   Primary surface:  640×480 (g_screenRect from ddraw_init.c)
 *   Back buffer:      640×480 (same as primary)
 *   Cursor staging:   256×256 (g_CursorSurface in input.c)
 *   Tile sprites:     variable (from RFH archive, typically 32x32 or 64x64)
 *   Splash screen:    640×480 or sub-region
 *
 * Build dependencies: -lSDL2
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* access() */

#include "../core/loco_types.h"

/* =========================================================================
 * Surface handle type
 *
 * In Win32, IDirectDrawSurface* pointers are passed everywhere.
 * In the Linux port, each "surface" is one of these.  Callers cast as needed.
 * ========================================================================= */
typedef struct LocoSurface {
    SDL_Texture *texture;    /* GPU-side; used for rendering            */
    SDL_Surface *surface;    /* CPU-side; used for pixel read/write     */
    int          width;
    int          height;
    int          isCpuWrite; /* 1 = STREAMING (CPU writable), 0 = STATIC */
    void        *lockedPixels;  /* non-NULL while between Lock/Unlock   */
    int          lockedPitch;
} LocoSurface;

/* =========================================================================
 * Module globals
 *
 * These replace the DirectDraw global COM interface pointers in ddraw_init.c.
 * ========================================================================= */

/* WIN32: IDirectDraw4 *g_pDD  (0x485440) — DirectDraw factory */
/* LINUX: the SDL renderer from sdl_window.c serves this role   */
extern SDL_Renderer *g_sdlRenderer;  /* defined in sdl_window.c */
extern SDL_Window   *g_sdlWindow;    /* defined in sdl_window.c */

/* WIN32: IDirectDrawSurface *g_pDDSPrimary (0x4FD3C0) — front buffer  */
/* LINUX: render target texture at 640×480                               */
static LocoSurface *g_primarySurface = NULL;

/* WIN32: IDirectDrawSurface *g_pDDSBack (0x4FD3C4) — back-buffer       */
/* LINUX: secondary render target; game draws here, then blits to primary */
static LocoSurface *g_backBuffer     = NULL;

/* WIN32: IDirectDrawSurface *g_pDDSSplash (0x4FD3D8) — loading screen  */
static LocoSurface *g_splashSurface  = NULL;

/* Pixel format globals (originally at 0x485274 in ddraw_init.c).
 * On Linux we always use RGB565 to match game's pixel arithmetic. */
static uint32_t g_pixelFormat = SDL_PIXELFORMAT_RGB565;

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

static LocoSurface *Video_AllocSurface(void)
{
    LocoSurface *s = (LocoSurface *)calloc(1, sizeof(LocoSurface));
    return s;
}

static void Video_FreeSurface(LocoSurface *s)
{
    if (s == NULL) return;
    if (s->texture) {
        /* WIN32: IDirectDrawSurface::Release() */
        /* LINUX: SDL_DestroyTexture */
        SDL_DestroyTexture(s->texture);
    }
    if (s->surface) {
        /* WIN32: no SDL_Surface equivalent in DirectDraw path */
        /* LINUX: SDL_FreeSurface */
        SDL_FreeSurface(s->surface);
    }
    free(s);
}

/* =========================================================================
 * SDL_Video_Init  —  replaces DD_Init (0x0045b500)
 *
 * Initialises the DirectDraw equivalent subsystem using SDL2.
 * The SDL_Window and SDL_Renderer are assumed to be already created by
 * Platform_CreateWindow (sdl_window.c); this function creates the surface
 * layer on top of that renderer.
 *
 * WIN32 sequence in DD_Init:
 *   1. DirectDrawCreate(NULL, &g_pDDRaw, NULL)
 *   2. QueryInterface(IID_IDirectDraw4, &g_pDD)
 *   3. SetCooperativeLevel(hwnd, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN)
 *   4. CreateSurface(DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP, &g_pDDSPrimary)
 *   5. GetSurfaceDesc → DD_SetPixelFormatGlobals (pixel format detection)
 *   6. CreateSurface(DDSCAPS_OFFSCREENPLAIN, 640, 480) → g_pDDSBack
 *   7. SetColorKey(DDCKEY_SRCBLT, magenta) on back buffer
 *   8. CreateClipper / SetHWnd / SetClipper
 *
 * LINUX sequence here:
 *   1. Verify g_sdlRenderer is valid
 *   2. SDL_SetWindowFullscreen if fullscreen requested
 *   3. SDL_RenderSetLogicalSize(640, 480)
 *   4. Create primary render target texture (SDL_TEXTUREACCESS_TARGET)
 *   5. Create back-buffer render target texture
 *   6. SDL_SetColorKey on a CPU-side surface (magenta transparency)
 *   (clipper: not needed — SDL_RenderSetClipRect handles per-draw clipping)
 *
 * Returns 1 on success, 0 on failure.
 * ========================================================================= */
int SDL_Video_Init(int fullscreen)
{
    if (g_sdlRenderer == NULL) {
        fprintf(stderr, "SDL_Video_Init: renderer not created yet\n");
        return 0;
    }

    /* ── Step 3: cooperative level / fullscreen mode ── */
    /* WIN32: IDirectDraw::SetCooperativeLevel(hwnd, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN) */
    /* LINUX: SDL_SetWindowFullscreen */
    if (fullscreen) {
        /* WIN32: DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN (0x14) */
        /* LINUX: SDL_WINDOW_FULLSCREEN uses the exact desktop resolution */
        SDL_SetWindowFullscreen(g_sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        /* WIN32: DDSCL_NORMAL (0x08) — windowed mode */
        /* LINUX: 0 = windowed */
        SDL_SetWindowFullscreen(g_sdlWindow, 0);
    }

    /* ── Step 3b: set display mode ── */
    /* WIN32: IDirectDraw::SetDisplayMode(640, 480, 16)
     *   Forces 640×480 @ 16-bit regardless of desktop mode in exclusive FS.
     * LINUX: SDL_RenderSetLogicalSize creates a virtual 640×480 coordinate
     *   space that is scaled to the physical display by the renderer. */
    SDL_RenderSetLogicalSize(g_sdlRenderer, 640, 480);

    /* ── Step 4: create primary surface ── */
    /* WIN32: IDirectDraw::CreateSurface(&desc, &g_pDDSPrimary, NULL)
     *   desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP (0x4040)
     *   The primary surface represents the visible front buffer.
     * LINUX: SDL_CreateTexture with SDL_TEXTUREACCESS_TARGET
     *   This texture is used as a render target; SDL_RenderPresent flips it. */
    g_primarySurface = Video_AllocSurface();
    if (g_primarySurface == NULL) return 0;
    g_primarySurface->width  = 640;
    g_primarySurface->height = 480;
    g_primarySurface->isCpuWrite = 0;

    /* WIN32: IDirectDraw4::CreateSurface(..., &g_pDDSPrimary, NULL) */
    /* LINUX: SDL_CreateTexture (TEXTUREACCESS_TARGET = GPU render target) */
    g_primarySurface->texture = SDL_CreateTexture(
        g_sdlRenderer,
        g_pixelFormat,                /* SDL_PIXELFORMAT_RGB565 */
        SDL_TEXTUREACCESS_TARGET,
        640, 480);
    if (g_primarySurface->texture == NULL) {
        fprintf(stderr, "SDL_Video_Init: primary texture: %s\n", SDL_GetError());
        Video_FreeSurface(g_primarySurface);
        g_primarySurface = NULL;
        return 0;
    }

    /* ── Step 6: create back-buffer ── */
    /* WIN32: second IDirectDraw::CreateSurface call
     *   desc.dwFlags = DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT
     *   desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN (0x40)
     *   This is the off-screen buffer the game draws into each frame.
     * LINUX: second SDL_CreateTexture with SDL_TEXTUREACCESS_TARGET */
    g_backBuffer = Video_AllocSurface();
    if (g_backBuffer == NULL) return 0;
    g_backBuffer->width  = 640;
    g_backBuffer->height = 480;
    g_backBuffer->isCpuWrite = 0;

    /* WIN32: IDirectDraw4::CreateSurface → g_pDDSBack */
    /* LINUX: SDL_CreateTexture (TEXTUREACCESS_TARGET) */
    g_backBuffer->texture = SDL_CreateTexture(
        g_sdlRenderer,
        g_pixelFormat,
        SDL_TEXTUREACCESS_TARGET,
        640, 480);
    if (g_backBuffer->texture == NULL) {
        fprintf(stderr, "SDL_Video_Init: back-buffer texture: %s\n", SDL_GetError());
        Video_FreeSurface(g_primarySurface);
        Video_FreeSurface(g_backBuffer);
        g_primarySurface = NULL;
        g_backBuffer     = NULL;
        return 0;
    }

    /* ── Step 7: set transparent colour key (magenta) ── */
    /* WIN32: IDirectDrawSurface::SetColorKey(DDCKEY_SRCBLT, &ck)
     *   ck.dwColorSpaceLowValue = 0xF81F (RGB565 magenta = R=31,G=0,B=31)
     *   Applied to g_pDDSBack and sprite surfaces.
     * LINUX: SDL_SetColorKey applies to SDL_Surface* objects (CPU-side).
     *   For GPU textures, use SDL_SetTextureBlendMode + SDL_BLENDMODE_BLEND
     *   after setting alpha in the source surface before texture upload. */
    /* Note: g_backBuffer->texture is GPU-side; colour-key is applied when
     *       individual sprite surfaces are loaded in SDL_Video_LoadBitmap. */

    /* ── Step 8: clipper ── */
    /* WIN32: IDirectDraw::CreateClipper + SetHWnd + SetClipper
     *   Clips blits to the window client area.
     * LINUX: Not needed — SDL_RenderSetClipRect or SDL_Rect* on each blit
     *   provides equivalent clipping per-draw-call.
     *   The logical size set above (640×480) implicitly clips to that area. */

    /* Set back buffer as the initial render target */
    /* WIN32: all drawing goes to g_pDDSBack; Flip/Blt presents to primary */
    /* LINUX: SDL_SetRenderTarget switches the render target to back-buffer */
    SDL_SetRenderTarget(g_sdlRenderer, g_backBuffer->texture);

    fprintf(stderr, "SDL_Video_Init: surfaces created (640x480, %s)\n",
            fullscreen ? "fullscreen" : "windowed");
    return 1;
}

/* =========================================================================
 * SDL_Video_Shutdown  —  replaces DD_Shutdown (0x0045baa0)
 *
 * Releases all DirectDraw surface equivalents in safe dependency order.
 *
 * WIN32 DD_Shutdown sequence:
 *   IDirectDrawSurface::Release(g_pDDSPrimary)
 *   IDirectDrawSurface::Release(g_pDDSBack)
 *   DD_ReleaseAuxSurfaces()  (g_pSurf[0..5])
 *   IDirectDraw::SetCooperativeLevel(NULL, DDSCL_NORMAL)  [restore GDI]
 *   IDirectDraw::Release(g_pDD)
 *   IDirectDraw::Release(g_pDDRaw)
 *
 * LINUX:
 *   SDL_DestroyTexture for each surface
 *   SDL_SetWindowFullscreen(0)  [restore windowed mode]
 * ========================================================================= */
void SDL_Video_Shutdown(void)
{
    /* Reset render target to default (window) before destroying textures */
    if (g_sdlRenderer != NULL)
        SDL_SetRenderTarget(g_sdlRenderer, NULL);

    /* WIN32: IDirectDrawSurface::Release(g_pDDSSplash) */
    /* LINUX: SDL_DestroyTexture */
    Video_FreeSurface(g_splashSurface);
    g_splashSurface = NULL;

    /* WIN32: IDirectDrawSurface::Release(g_pDDSBack) */
    Video_FreeSurface(g_backBuffer);
    g_backBuffer = NULL;

    /* WIN32: IDirectDrawSurface::Release(g_pDDSPrimary) */
    Video_FreeSurface(g_primarySurface);
    g_primarySurface = NULL;

    /* WIN32: IDirectDraw::SetCooperativeLevel(NULL, DDSCL_NORMAL) */
    /* LINUX: restore windowed mode */
    if (g_sdlWindow != NULL)
        SDL_SetWindowFullscreen(g_sdlWindow, 0);

    /* WIN32: IDirectDraw::Release(g_pDD); IDirectDraw::Release(g_pDDRaw) */
    /* LINUX: renderer and window are owned by sdl_window.c; not freed here */

    fprintf(stderr, "SDL_Video_Shutdown: surfaces released\n");
}

/* =========================================================================
 * SDL_Video_CreateOffscreenSurface  —  replaces IDirectDraw::CreateSurface
 *                                       (DDSCAPS_OFFSCREENPLAIN)
 *
 * Creates an off-screen surface for sprite/texture data.
 *
 * WIN32: IDirectDraw::CreateSurface(&desc, &pSurf, NULL)
 *   desc.dwFlags = DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT
 *   desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN
 *     | DDSCAPS_VIDEOMEMORY (0x4040) or DDSCAPS_SYSTEMMEMORY (0x840)
 *
 * LINUX:
 *   cpuWritable == 0 → SDL_TEXTUREACCESS_STATIC  (upload once, draw many)
 *   cpuWritable == 1 → SDL_TEXTUREACCESS_STREAMING (Lock/Unlock pixel access)
 *
 * Returns LocoSurface* on success (cast to void* in caller code), NULL on failure.
 * ========================================================================= */
LocoSurface *SDL_Video_CreateOffscreenSurface(int width, int height, int cpuWritable)
{
    LocoSurface *s;
    SDL_TextureAccess access;

    s = Video_AllocSurface();
    if (s == NULL) return NULL;

    s->width      = width;
    s->height     = height;
    s->isCpuWrite = cpuWritable;

    /* WIN32: DDSCAPS_VIDEOMEMORY → GPU preferred; DDSCAPS_SYSTEMMEMORY → CPU
     * LINUX: SDL_TEXTUREACCESS_STATIC for GPU-only; STREAMING for CPU access */
    access = cpuWritable ? SDL_TEXTUREACCESS_STREAMING : SDL_TEXTUREACCESS_STATIC;

    /* WIN32: IDirectDraw4::CreateSurface(&desc, &pSurf, NULL) */
    /* LINUX: SDL_CreateTexture(renderer, format, access, w, h) */
    s->texture = SDL_CreateTexture(g_sdlRenderer, g_pixelFormat, access,
                                   width, height);
    if (s->texture == NULL) {
        fprintf(stderr, "SDL_Video_CreateOffscreenSurface: %dx%d: %s\n",
                width, height, SDL_GetError());
        free(s);
        return NULL;
    }

    /* Enable blending for color-key transparency.
     * WIN32: DDBLTFAST_SRCCOLORKEY enables transparency on BltFast.
     * LINUX: SDL_BLENDMODE_BLEND applies alpha from SDL_SetColorKey. */
    SDL_SetTextureBlendMode(s->texture, SDL_BLENDMODE_BLEND);

    return s;
}

/* =========================================================================
 * SDL_Video_DestroySurface  —  replaces IDirectDrawSurface::Release
 *
 * WIN32: pSurf->Release()  (vtable[2], COM reference count drop)
 * LINUX: SDL_DestroyTexture + free
 * ========================================================================= */
void SDL_Video_DestroySurface(LocoSurface *s)
{
    /* WIN32: IDirectDrawSurface::Release() */
    /* LINUX: SDL_DestroyTexture + free */
    Video_FreeSurface(s);
}

/* =========================================================================
 * SDL_Video_SetColorKey  —  replaces DD_SetTransparentColorKey (0x0045ba50)
 *
 * Sets magenta (R=255, G=0, B=255) as the transparent color key on a surface.
 * This function operates on the CPU-side SDL_Surface* associated with a
 * LocoSurface.  The texture must be re-uploaded after calling this.
 *
 * WIN32: IDirectDrawSurface::SetColorKey(DDCKEY_SRCBLT, &ck)
 *   ck.dwColorSpaceLowValue:
 *     RGB565: 0xF81F  (R=11111, G=000000, B=11111)
 *     RGB555: 0x7C1F  (R=11111, G=00000, B=11111)
 *   Called in DD_Init step 9 and DD_SetTransparentColorKey (0x0045ba50).
 *
 * LINUX: SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(fmt, 255, 0, 255))
 *   Must be called on the SDL_Surface*, not the SDL_Texture*.
 *   After calling SDL_SetColorKey, re-upload via SDL_UpdateTexture or
 *   recreate texture with SDL_CreateTextureFromSurface.
 * ========================================================================= */
void SDL_Video_SetColorKey(SDL_Surface *surface)
{
    Uint32 magenta;

    if (surface == NULL) return;

    /* WIN32: encode magenta as 0xF81F (RGB565) or 0x7C1F (RGB555)
     * LINUX: SDL_MapRGB computes the correct pixel value for the surface format */
    magenta = SDL_MapRGB(surface->format,
                         LOCO_COLOR_KEY_R,   /* 255 */
                         LOCO_COLOR_KEY_G,   /* 0   */
                         LOCO_COLOR_KEY_B);  /* 255 */

    /* WIN32: IDirectDrawSurface::SetColorKey(DDCKEY_SRCBLT, &ck) */
    /* LINUX: SDL_SetColorKey(surface, SDL_TRUE, key) */
    SDL_SetColorKey(surface, SDL_TRUE, magenta);
}

/* =========================================================================
 * SDL_Video_LoadBitmap  —  replaces DD_LoadBitmap (0x00401000)
 *
 * Loads a BMP file from disk into a LocoSurface, applying magenta colour key.
 *
 * WIN32 DD_LoadBitmap sequence:
 *   1. GetFileAttributesA(path)                — check existence
 *   2. LoadImageA(hInst, path, IMAGE_BITMAP, w, h, LR_LOADFROMFILE)
 *   3. GetObject(hBitmap, sizeof(BITMAP), &bm) — read dimensions
 *   4. IDirectDraw::CreateSurface(...)         — allocate DD surface
 *   5. DD_SetTransparentColorKey               — set magenta key
 *   6. DD_CopyBitmapToSurface (GDI StretchBlt) — copy pixels in
 *   7. DeleteObject(hBitmap)
 *
 * LINUX replacement:
 *   1. access(path, F_OK)           — check file exists
 *   2. SDL_LoadBMP(path)            — load BMP to SDL_Surface
 *   3. SDL_ConvertSurfaceFormat     — convert to RGB565
 *   4. SDL_SetColorKey              — set magenta transparent
 *   5. SDL_CreateTextureFromSurface — upload to GPU
 *   6. SDL_FreeSurface
 *
 * Parameters:
 *   path        — filesystem path to BMP file
 *   width       — desired width  (0 = use BMP natural width)
 *   height      — desired height (0 = use BMP natural height)
 *   cpuWritable — non-zero = STREAMING texture (Lock/Unlock pixel access)
 *
 * Returns LocoSurface* on success, NULL on failure.
 * ========================================================================= */
LocoSurface *SDL_Video_LoadBitmap(const char *path, int width, int height,
                                   int cpuWritable)
{
    SDL_Surface  *raw;
    SDL_Surface  *converted;
    LocoSurface  *s;
    int           bmpW, bmpH;

    /* WIN32: GetFileAttributesA(path) == 0xFFFFFFFF means file not found */
    /* LINUX: access(path, F_OK) returns -1 if not found */
    if (access(path, F_OK) != 0) {
        fprintf(stderr, "SDL_Video_LoadBitmap: file not found: '%s'\n", path);
        return NULL;
    }

    /* WIN32: LoadImageA(hInst, path, IMAGE_BITMAP, w, h, LR_LOADFROMFILE) */
    /* LINUX: SDL_LoadBMP(path) */
    raw = SDL_LoadBMP(path);
    if (raw == NULL) {
        fprintf(stderr, "SDL_Video_LoadBitmap: SDL_LoadBMP('%s'): %s\n",
                path, SDL_GetError());
        return NULL;
    }

    /* Read natural dimensions (equivalent of GetObject / BITMAP.bmWidth/bmHeight) */
    bmpW = (width  > 0) ? width  : raw->w;
    bmpH = (height > 0) ? height : raw->h;

    /* Convert to RGB565 to match the game's native pixel format.
     * WIN32: DirectDraw creates the surface in the display's native format
     *        (detected by DD_SetPixelFormatGlobals as RGB565 on modern hardware).
     * LINUX: SDL_ConvertSurfaceFormat normalises to a known format before upload. */
    converted = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGB565, 0);
    SDL_FreeSurface(raw);
    if (converted == NULL) {
        fprintf(stderr, "SDL_Video_LoadBitmap: convert: %s\n", SDL_GetError());
        return NULL;
    }

    /* Scale if requested (equivalent of StretchBlt in DD_CopyBitmapToSurface).
     * WIN32: StretchBlt(ddHdc, 0, 0, destW, destH, srcHdc, 0, 0, srcW, srcH, SRCCOPY)
     * LINUX: SDL_BlitScaled to a new surface of the target dimensions */
    if ((bmpW != converted->w || bmpH != converted->h) && (bmpW > 0 && bmpH > 0)) {
        SDL_Surface *scaled = SDL_CreateRGBSurfaceWithFormat(0, bmpW, bmpH, 16,
                                                              SDL_PIXELFORMAT_RGB565);
        if (scaled != NULL) {
            /* WIN32: StretchBlt */
            /* LINUX: SDL_BlitScaled (bilinear scale) */
            SDL_BlitScaled(converted, NULL, scaled, NULL);
            SDL_FreeSurface(converted);
            converted = scaled;
        }
    }

    /* Set magenta colour key for transparency.
     * WIN32: DD_SetTransparentColorKey (0x0045ba50)
     *        IDirectDrawSurface::SetColorKey(DDCKEY_SRCBLT, &ck)
     * LINUX: SDL_SetColorKey(surface, SDL_TRUE, magenta) */
    SDL_Video_SetColorKey(converted);

    /* Allocate LocoSurface wrapper */
    s = Video_AllocSurface();
    if (s == NULL) {
        SDL_FreeSurface(converted);
        return NULL;
    }
    s->width      = bmpW;
    s->height     = bmpH;
    s->isCpuWrite = cpuWritable;

    if (cpuWritable) {
        /* Keep CPU-side surface for streaming updates.
         * WIN32: DDSCAPS_SYSTEMMEMORY surface allows Lock/Unlock pixel access.
         * LINUX: retain SDL_Surface*; create STREAMING texture from it. */
        s->surface = converted;
        s->texture = SDL_CreateTexture(g_sdlRenderer, g_pixelFormat,
                                       SDL_TEXTUREACCESS_STREAMING, bmpW, bmpH);
        if (s->texture != NULL) {
            /* Upload initial pixels */
            SDL_UpdateTexture(s->texture, NULL, converted->pixels, converted->pitch);
        }
    } else {
        /* Static upload: GPU-only after initial transfer.
         * WIN32: DDSCAPS_VIDEOMEMORY (preferred) or DDSCAPS_SYSTEMMEMORY fallback.
         * LINUX: SDL_CreateTextureFromSurface uploads and the SDL_Surface can be freed. */
        s->texture = SDL_CreateTextureFromSurface(g_sdlRenderer, converted);
        SDL_FreeSurface(converted);
        converted = NULL;
    }

    if (s->texture == NULL) {
        fprintf(stderr, "SDL_Video_LoadBitmap: texture upload: %s\n", SDL_GetError());
        Video_FreeSurface(s);
        return NULL;
    }

    /* Enable blending for colour-key transparency.
     * WIN32: BltFast with DDBLTFAST_SRCCOLORKEY performs keyed transparency.
     * LINUX: SDL_BLENDMODE_BLEND applies the color key as an alpha mask. */
    SDL_SetTextureBlendMode(s->texture, SDL_BLENDMODE_BLEND);

    return s;
}

/* =========================================================================
 * SDL_Video_BltFast  —  replaces IDirectDrawSurface::BltFast
 *
 * Blits a rectangular region from src to dst at position (destX, destY).
 * Transparency is applied via SDL_BLENDMODE_BLEND (color key already set
 * on the source texture by SDL_Video_SetColorKey at load time).
 *
 * WIN32: IDirectDrawSurface::BltFast(destX, destY, srcSurf, &srcRect,
 *                                    DDBLTFAST_SRCCOLORKEY | DDBLTFAST_WAIT)
 *   DDBLTFAST_SRCCOLORKEY: treat the source surface's colour key as transparent.
 *   DDBLTFAST_WAIT:        block until the hardware is ready.
 *
 * LINUX: SDL_RenderCopy(renderer, srcTex, &srcSDLRect, &dstSDLRect)
 *   srcSDLRect = {srcRect.left, srcRect.top,
 *                 srcRect.right - srcRect.left, srcRect.bottom - srcRect.top}
 *   dstSDLRect = {destX, destY, srcW, srcH}
 *
 * Note: DDBLTFAST is to an offscreen destination.  If dst is the back-buffer,
 * ensure SDL_SetRenderTarget(renderer, dst->texture) was called first.
 * ========================================================================= */
int SDL_Video_BltFast(LocoSurface *dst, int destX, int destY,
                      LocoSurface *src,
                      int srcX, int srcY, int srcW, int srcH)
{
    SDL_Rect srcRect, dstRect;

    if (src == NULL || src->texture == NULL) return -1;

    /* Set render target to destination surface if not the screen */
    if (dst != NULL && dst->texture != NULL) {
        /* WIN32: BltFast writes directly to the destination surface
         * LINUX: SDL_SetRenderTarget switches the render destination */
        SDL_SetRenderTarget(g_sdlRenderer, dst->texture);
    }

    srcRect.x = srcX;
    srcRect.y = srcY;
    srcRect.w = srcW;
    srcRect.h = srcH;

    dstRect.x = destX;
    dstRect.y = destY;
    dstRect.w = srcW;
    dstRect.h = srcH;

    /* WIN32: BltFast with DDBLTFAST_SRCCOLORKEY — transparent blit
     * LINUX: SDL_RenderCopy uses the blend mode set on the source texture */
    return SDL_RenderCopy(g_sdlRenderer, src->texture, &srcRect, &dstRect);
}

/* =========================================================================
 * SDL_Video_Blt  —  replaces IDirectDrawSurface::Blt
 *
 * Full-featured blit with optional source and destination rectangles.
 * Used in DD_BlitToScreen (0x00401280) and cursor compositing.
 *
 * WIN32: g_pDDSPrimary->Blt(&dstRect, g_pDDSBack, &srcRect, DDBLT_WAIT, NULL)
 *   DDBLT_WAIT = 0x1000000 — block until blit hardware ready.
 *   DDBLT_KEYSRC can be combined for colour-key transparency.
 *   Handles DDERR_SURFACELOST by calling Restore() and retrying.
 *
 * LINUX: SDL_RenderCopy(renderer, srcTex, &srcRect, &dstRect)
 *   No retry logic needed — SDL2 does not have "lost surfaces".
 * ========================================================================= */
int SDL_Video_Blt(LocoSurface *dst, const SDL_Rect *dstRect,
                  LocoSurface *src, const SDL_Rect *srcRect)
{
    int result;

    if (src == NULL || src->texture == NULL) return -1;

    /* Set render target.
     * WIN32: the destination surface IS the render target implicitly.
     * LINUX: must switch SDL render target explicitly. */
    if (dst != NULL && dst->texture != NULL) {
        /* WIN32: IDirectDrawSurface::Blt writes to 'this' surface */
        /* LINUX: SDL_SetRenderTarget changes where SDL_RenderCopy draws */
        SDL_SetRenderTarget(g_sdlRenderer, dst->texture);
    } else {
        SDL_SetRenderTarget(g_sdlRenderer, NULL); /* render to window/screen */
    }

    /* WIN32: pDst->Blt(&dstRect, pSrc, &srcRect, DDBLT_WAIT, NULL) */
    /* LINUX: SDL_RenderCopy(renderer, srcTex, srcRect, dstRect) */
    result = SDL_RenderCopy(g_sdlRenderer, src->texture, srcRect, dstRect);

    /* WIN32: on DDERR_SURFACELOST: call Restore() then retry
     * LINUX: no equivalent error; SDL handles device reset internally */

    return result;
}

/* =========================================================================
 * SDL_Video_Lock  —  replaces IDirectDrawSurface::Lock
 *
 * Locks a surface for direct CPU pixel access.
 *
 * WIN32: IDirectDrawSurface::Lock(NULL, &desc, DDLOCK_WAIT, NULL)
 *   desc.lpSurface = pointer to pixel data
 *   desc.lPitch    = row pitch in bytes
 *   After Lock, pixels are accessible at desc.lpSurface + y*lPitch + x*bpp
 *
 * LINUX: SDL_LockTexture(texture, NULL, &pixels, &pitch)
 *   pixels = pointer to pixel data
 *   pitch  = row pitch in bytes
 *   Note: only works on STREAMING textures (SDL_TEXTUREACCESS_STREAMING).
 * ========================================================================= */
void *SDL_Video_Lock(LocoSurface *s, int *outPitch)
{
    void *pixels = NULL;
    int   pitch  = 0;

    if (s == NULL || s->texture == NULL) return NULL;

    if (!s->isCpuWrite) {
        fprintf(stderr, "SDL_Video_Lock: cannot lock non-streaming texture\n");
        return NULL;
    }

    /* WIN32: IDirectDrawSurface::Lock(NULL, &ddsd, DDLOCK_WAIT, NULL)
     *        ddsd.lpSurface = pixel pointer;  ddsd.lPitch = row stride */
    /* LINUX: SDL_LockTexture — only valid for STREAMING textures */
    if (SDL_LockTexture(s->texture, NULL, &pixels, &pitch) != 0) {
        fprintf(stderr, "SDL_Video_Lock: SDL_LockTexture: %s\n", SDL_GetError());
        return NULL;
    }

    s->lockedPixels = pixels;
    s->lockedPitch  = pitch;
    if (outPitch) *outPitch = pitch;

    return pixels;
}

/* =========================================================================
 * SDL_Video_Unlock  —  replaces IDirectDrawSurface::Unlock
 *
 * Releases the CPU pixel lock on a surface.
 *
 * WIN32: IDirectDrawSurface::Unlock(NULL)
 *   After unlock the surface returns to GPU control.
 * LINUX: SDL_UnlockTexture(texture)
 *   After unlock the modified pixels are uploaded to the GPU.
 * ========================================================================= */
void SDL_Video_Unlock(LocoSurface *s)
{
    if (s == NULL || s->texture == NULL) return;

    /* WIN32: IDirectDrawSurface::Unlock(NULL) */
    /* LINUX: SDL_UnlockTexture uploads modified pixels to GPU */
    SDL_UnlockTexture(s->texture);

    s->lockedPixels = NULL;
    s->lockedPitch  = 0;
}

/* =========================================================================
 * SDL_Video_Flip  —  replaces IDirectDrawSurface::Flip (0x0045e1e0)
 *
 * Presents the back buffer to the screen, completing one rendered frame.
 * Called once per frame when g_renderActive != 0 in the game loop.
 *
 * WIN32:
 *   IDirectDrawSurface::Flip(NULL, DDFLIP_WAIT)  — exclusive mode
 *   OR IDirectDrawSurface::Blt (primary ← back)  — windowed mode
 *   OR DD_SendFrameMessage: SendMessageA(hwnd, WM_USER+7, param, 0)
 *     → WndProc handles 0x407 and calls the flip internally
 *
 * LINUX:
 *   1. Set render target back to NULL (screen)
 *   2. SDL_RenderCopy(renderer, backBuffer->texture, NULL, NULL) — full copy
 *   3. SDL_RenderPresent(renderer) — swap to display
 * ========================================================================= */
void SDL_Video_Flip(void)
{
    /* Restore render target to the screen/window (not the back-buffer texture) */
    /* WIN32: Flip or Blt from g_pDDSBack to g_pDDSPrimary */
    /* LINUX: first copy back-buffer texture to renderer output */
    SDL_SetRenderTarget(g_sdlRenderer, NULL);

    if (g_backBuffer != NULL && g_backBuffer->texture != NULL) {
        /* WIN32: g_pDDSPrimary->Blt(NULL, g_pDDSBack, NULL, DDBLT_WAIT, NULL) */
        /* LINUX: SDL_RenderCopy with NULL rects = full surface copy */
        SDL_RenderCopy(g_sdlRenderer, g_backBuffer->texture, NULL, NULL);
    }

    /* WIN32: IDirectDrawSurface::Flip(NULL, DDFLIP_WAIT) */
    /* LINUX: SDL_RenderPresent swaps the back and front buffers */
    SDL_RenderPresent(g_sdlRenderer);

    /* Restore render target to back-buffer for next frame's draw calls */
    if (g_backBuffer != NULL && g_backBuffer->texture != NULL)
        SDL_SetRenderTarget(g_sdlRenderer, g_backBuffer->texture);
}

/* =========================================================================
 * SDL_Video_ClearBackBuffer  —  replaces FillRect with BLACK_BRUSH
 *
 * Clears the back buffer to solid black.
 * Called in DD_ShowSplashScreen and CGame_StopAllSounds.
 *
 * WIN32: GetStockObject(BLACK_BRUSH) + FillRect(hdcSurf, &g_screenRect, hBlack)
 *   Requires GetDC on the back-buffer surface first.
 * LINUX: SDL_SetRenderDrawColor + SDL_RenderClear (no DC needed)
 * ========================================================================= */
void SDL_Video_ClearBackBuffer(void)
{
    SDL_Texture *prevTarget = SDL_GetRenderTarget(g_sdlRenderer);

    /* Switch to back-buffer target */
    if (g_backBuffer != NULL && g_backBuffer->texture != NULL)
        SDL_SetRenderTarget(g_sdlRenderer, g_backBuffer->texture);

    /* WIN32: GetStockObject(BLACK_BRUSH); FillRect(hdc, &rect, hBlack) */
    /* LINUX: SDL_SetRenderDrawColor(0, 0, 0, 255) + SDL_RenderClear */
    SDL_SetRenderDrawColor(g_sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(g_sdlRenderer);

    /* Restore previous render target */
    SDL_SetRenderTarget(g_sdlRenderer, prevTarget);
}

/* =========================================================================
 * SDL_Video_GetDisplaySize  —  replaces GetSystemMetrics
 *
 * Returns the physical display dimensions.
 *
 * WIN32: GetSystemMetrics(SM_CXSCREEN) / GetSystemMetrics(SM_CYSCREEN)
 *   Used in DD_ShowSplashScreen and APP_InitWindow to center windows.
 * LINUX: SDL_GetCurrentDisplayMode(0, &mode)
 *   mode.w = screen width, mode.h = screen height.
 * ========================================================================= */
void SDL_Video_GetDisplaySize(int *outW, int *outH)
{
    SDL_DisplayMode mode;

    /* WIN32: int w = GetSystemMetrics(SM_CXSCREEN);
     *        int h = GetSystemMetrics(SM_CYSCREEN); */
    /* LINUX: SDL_GetCurrentDisplayMode(displayIndex=0, &mode) */
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
        if (outW) *outW = mode.w;
        if (outH) *outH = mode.h;
    } else {
        /* Fallback to 640×480 if display query fails */
        if (outW) *outW = 640;
        if (outH) *outH = 480;
        fprintf(stderr, "SDL_Video_GetDisplaySize: SDL_GetCurrentDisplayMode: %s\n",
                SDL_GetError());
    }
}

/* =========================================================================
 * SDL_Video_GetPrimarySurface  —  accessor for g_pDDSPrimary equivalent
 *
 * WIN32: extern IDirectDrawSurface *g_pDDSPrimary (0x4FD3C0)
 * LINUX: returns g_primarySurface
 * ========================================================================= */
LocoSurface *SDL_Video_GetPrimarySurface(void)
{
    return g_primarySurface;
}

/* =========================================================================
 * SDL_Video_GetBackBuffer  —  accessor for g_pDDSBack equivalent
 *
 * WIN32: extern IDirectDrawSurface *g_pDDSBack (0x4FD3C4)
 * LINUX: returns g_backBuffer
 * ========================================================================= */
LocoSurface *SDL_Video_GetBackBuffer(void)
{
    return g_backBuffer;
}
