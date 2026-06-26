/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: Graphics / DirectDraw Init / Surface Management
 * Original: loco.exe (Windows 95/98, DirectX 5 era)
 * Developer: Intelligent Games for LEGO Media
 *
 * This file was produced by reverse engineering the original binary.
 * Windows API calls are marked with WIN32: comments.
 * Linux/SDL2 replacement suggestions are marked with LINUX: comments.
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

/*
 * Platform abstraction: on Windows pull in DirectDraw / DirectSound;
 * on Linux include SDL2 equivalents for porting.
 */
#ifdef _WIN32  /* WIN32 */
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <ddraw.h>
#   include <dsound.h>
#else          /* LINUX — SDL2 equivalents */
#   include <SDL2/SDL.h>
#   include <SDL2/SDL_image.h>
#   include <SDL2/SDL_mixer.h>
/*
 * Stub types so the rest of the header compiles on Linux without
 * requiring Wine headers.  The actual port will replace these with
 * proper SDL2 types or custom wrappers.
 */
typedef void            IDirectDraw;
typedef void            IDirectDrawSurface;
typedef void            IDirectDrawClipper;
typedef void            IDirectSound;
typedef unsigned int    UINT;
typedef unsigned long   DWORD;
typedef int             BOOL;
typedef int             HRESULT;
typedef void           *HWND;
typedef void           *HINSTANCE;
typedef void           *HANDLE;
typedef void           *HBRUSH;
typedef void           *HICON;
typedef const char     *LPCSTR;
typedef struct { long left, top, right, bottom; } RECT;
typedef struct { long x, y; } POINT;
#endif /* _WIN32 */

/* ═══════════════════════════════════════════════════════════════════════════
 * Struct: DD_PixelFormat                              @ 0x00485274
 * ═══════════════════════════════════════════════════════════════════════════
 * Global pixel-format descriptor block stored as consecutive DWORDs
 * starting at 0x485274.  Populated during DD_Init once the primary surface
 * has been created and queried.  All rendering code reads these globals to
 * pack/unpack 16-bit colour values without branching on the format at every
 * blit site.
 *
 * LINUX: Eliminated entirely; SDL2 always works in 32-bit RGBA internally.
 */
typedef struct DD_PixelFormat {
    DWORD pixFmtId;      /* 0x485274 — 0x22B = RGB555, 0x235 = RGB565       */
    DWORD rShift;        /* 0x485278 — red bit-shift: 10 (555) or 11 (565)  */
    DWORD gBits;         /* 0x48527C — green bit-count: 5 (555) or 6 (565)  */
    DWORD whitePixel;    /* 0x485280 — max white: 0x3DEF (555) or 0x7BEF (565) */
    DWORD whitePixelAlt; /* 0x485284 — mirror of whitePixel (written after)  */
    DWORD rMask;         /* 0x485288 — red   channel bitmask from surface    */
    DWORD gMask;         /* 0x48528C — green channel bitmask from surface    */
    DWORD bMask;         /* 0x485290 — blue  channel bitmask from surface    */
} DD_PixelFormat;

/*
 * Pixel-format IDs stored in DD_PixelFormat.pixFmtId
 * LINUX: Not needed — SDL2 uses SDL_PIXELFORMAT_* constants instead.
 */
#define PIXFMT_RGB555  0x22Bu  /* 15-bit: R=5 G=5 B=5  */
#define PIXFMT_RGB565  0x235u  /* 16-bit: R=5 G=6 B=5  */

/*
 * Encoded magenta colour keys for IDirectDrawSurface::SetColorKey.
 * Magenta = R=31, G=0, B=31 packed into the respective 16-bit format.
 * LINUX: SDL_MapRGB(fmt, 255, 0, 255) replaces both constants.
 */
#define MAGENTA_KEY_RGB555  0x7C1Fu  /* R=11111 G=00000 B=11111 in RGB555 */
#define MAGENTA_KEY_RGB565  0xF81Fu  /* R=11111 G=000000 B=11111 in RGB565 */

/* ═══════════════════════════════════════════════════════════════════════════
 * Struct: DD_Globals (logical grouping — not a real struct in the binary)
 * ═══════════════════════════════════════════════════════════════════════════
 * Scattered global COM-interface pointers that together define the
 * DirectDraw runtime state.  Listed in address order.
 *
 * LINUX equivalent types:
 *   g_pDD / g_pDDRaw → SDL_Window*, SDL_Renderer*
 *   g_pDDSPrimary    → SDL_Renderer* (or the window's render target)
 *   g_pDDSBack       → SDL_Texture* with SDL_TEXTUREACCESS_TARGET
 *   g_pSurf[0-5]     → SDL_Texture* for each sprite/tile cache slot
 *   g_pDS            → Mix_* state (no single pointer equivalent)
 */

/* Primary DirectDraw interfaces */
extern IDirectDraw         *g_pDD;          /* 0x485440 — upgraded via QI  */
extern IDirectDraw         *g_pDDRaw;       /* 0x4A9908 — from DirectDrawCreate */
extern DWORD                g_fullscreen;   /* 0x4A9918 — 1=exclusive, 0=windowed */

/* Main rendering surfaces */
extern IDirectDrawSurface  *g_pDDSPrimary;  /* 0x4FD3C0 — front/primary surface */
extern IDirectDrawSurface  *g_pDDSBack;     /* 0x4FD3C4 — back-buffer surface   */
extern IDirectDrawSurface  *g_pDDSSplash;   /* 0x4FD3D8 — splash/loading screen */

/* DirectSound device (torn down in same shutdown sequence as DD) */
extern IDirectSound        *g_pDS;          /* 0x4FD3BC */

/* Sprite / texture cache — freed by DD_ReleaseAuxSurfaces() */
extern IDirectDrawSurface  *g_pSurf0;       /* 0x4FF0F8 */
extern IDirectDrawSurface  *g_pSurf1;       /* 0x4FF0FC */
extern IDirectDrawSurface  *g_pSurf2;       /* 0x4FF100 */
extern IDirectDrawSurface  *g_pSurf3;       /* 0x4FF104 */
extern IDirectDrawSurface  *g_pSurf4;       /* 0x4FF108 */
extern IDirectDrawSurface  *g_pSurf5;       /* 0x4FF10C */

/*
 * Palette or thumbnail COM-like wrapper object.
 * Released via vtable[0](self, 1) — not a standard IDirectDrawSurface.
 * LINUX: Replace with an SDL_Palette* or equivalent custom structure.
 */
extern void                *g_pPalOrThumb;  /* 0x4FF110 */

/* Pixel-format globals — members of DD_PixelFormat, exposed individually  */
extern DWORD  g_pixFmtId;    /* 0x485274 */
extern DWORD  g_rShift;      /* 0x485278 */
extern DWORD  g_gBits;       /* 0x48527C */
extern DWORD  g_whitePixel;  /* 0x485280 */
extern DWORD  g_whiteAlt;    /* 0x485284 */
extern DWORD  g_rMask;       /* 0x485288 */
extern DWORD  g_gMask;       /* 0x48528C */
extern DWORD  g_bMask;       /* 0x485290 */

/* ═══════════════════════════════════════════════════════════════════════════
 * Struct: APP_Window (logical grouping — original is an opaque C++ object)
 * ═══════════════════════════════════════════════════════════════════════════
 * Main application object.  The fields below are the subset relevant to
 * the graphics / windowing subsystem.  The object is accessed through the
 * g_appObj pointer; field offsets are baked into the code.
 *
 * LINUX: The HWND / HINSTANCE fields map to SDL_Window* / the SDL
 *        event loop.  The child-window subclassing (GWL_WNDPROC) is
 *        removed — SDL2 dispatches all events through SDL_PollEvent.
 */
typedef struct APP_Window {
    DWORD      field_00;        /* +0x000 — purpose unknown                 */
    HINSTANCE  hInstance;       /* +0x004 — process instance handle         */
    HWND       hWndMain;        /* +0x008 — top-level window handle         */
    HWND       hWndChild;       /* +0x00C — child/render-target window      */
    /* ... many further fields not directly related to the DD subsystem ... */
    /* +0x0F8 */ HICON hIcon;   /* game icon loaded via LoadIconA           */
    /* +0x15C */ int   childX0; /* child window left   coordinate           */
    /* +0x160 */ int   childY0; /* child window top    coordinate           */
    /* +0x164 */ int   childX1; /* child window right  coordinate           */
    /* +0x168 */ int   childY1; /* child window bottom coordinate           */
    /* +0x214 */ long  origChildProc; /* saved WNDPROC before subclassing   */
    /* +0x20C */ HWND  hWndChildRender; /* child render-area window handle  */
    /* +0x21C */ void *pManager0;       /* input or graphics manager object */
    /* +0x220 */ void *pManager1;       /* secondary manager object         */
} APP_Window;

/* Render-active flag: non-zero → DD_SendFrameMessage fires each loop tick */
extern DWORD  g_renderActive;   /* 0x4AA4A4 */

/* Global application object and main-window shortcut */
extern void  *g_appObj;         /* base of APP_Window / application object  */
extern HWND   hWndMain;         /* HWND shortcut == *(HWND*)(g_appObj + 8)  */

/* Full-screen rect used as the default blit area (640×480 or 800×600) */
extern RECT   g_screenRect;     /* 0x485220 */

/* Config store (INI/registry abstraction used for audio volume persistence) */
extern void  *g_configStore;

/*
 * Opaque data references from APP_InitWindow / APP_Window registration.
 * These are pointer-sized data globals at fixed addresses in .data/.rdata.
 */
extern char   DAT_0047e464;     /* child window class name string           */
extern char   DAT_004851d0;     /* child window title string                */
extern HANDLE g_defaultFont;    /* default system font handle (DAT_004855F8) */
extern void  *g_aadm0c;        /* audio manager param — window or handle   */
extern void  *g_aadm10;        /* audio manager param — window or handle   */

/* ═══════════════════════════════════════════════════════════════════════════
 * Forward declarations for internal (game-engine) helper functions
 * referenced in the graphics subsystem.  These are not part of the public
 * API but must be visible to graphics.c.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Memory allocator wrapper (malloc equivalent) — 0x00465CE0 */
void *FUN_00465ce0(unsigned int size);

/* Splash sprite constructor — 0x00405790 */
void *FUN_00405790(void *mem, int resourceId, int param3, int param4, int param5);

/* Window class registration — 0x00421500 */
void FUN_00421500(int appObj);

/* Main-window creation wrapper (CreateWindowExA) — 0x00425B70 */
BOOL FUN_00425b70(void *self, int flags, HWND hWndParent,
                  int x, int y, int w, int h,
                  void *reserved, HICON hIcon, int extra);

/* Manager sub-object constructors and window binders */
void *FUN_00440F20(void *mem, DWORD hInstance, int classId); /* 0x00440F20 */
void  FUN_004412F0(void *mgr, HWND hWnd);                   /* 0x004412F0 */
void *FUN_00408AA0(void *mem, DWORD hInstance, int classId); /* 0x00408AA0 */
void  FUN_00408F00(void *mgr, HWND hWnd);                   /* 0x00408F00 */

/* DirectSound device constructor/helpers */
void *FUN_00412bd0(void *mem);                               /* 0x00412BD0 */
BOOL  FUN_00412c50(void *dsObj);                             /* 0x00412C50 */
void  FUN_004130a0(void *dsObj, void *p0, void *p1);        /* 0x004130A0 */
void  FUN_00412ee0(void *dsObj, HWND hWnd);                  /* 0x00412EE0 */
void  FUN_00413630(void *dsObj, DWORD volLow, DWORD volMed,
                   DWORD volHigh, DWORD volHigh2);           /* 0x00413630 */

/* Config store read/write helpers */
DWORD FUN_00452d60(void *store, const char *section,
                   const char *key, DWORD defaultVal);       /* 0x00452D60 */
void  FUN_00452db0(void *store, const char *section,
                   const char *key, DWORD value);            /* 0x00452DB0 */

/* Scroll/map system error notifier — called on blit failure */
void  FUN_00455840(void);                                    /* 0x00455840 */

/* ═══════════════════════════════════════════════════════════════════════════
 * Public function declarations — graphics subsystem
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * DD_Init                                                       0x0045B500
 *
 * Master DirectDraw initialisation: creates interfaces, sets cooperative
 * level, builds primary and back-buffer surfaces, detects pixel format,
 * sets the magenta colour key, and attaches the window clipper.
 *
 * Returns 1 on full success, 0 on any failure.
 *
 * WIN32: DirectDrawCreate + IDirectDraw4 + IDirectDrawSurface4
 * LINUX: SDL_CreateWindow + SDL_CreateRenderer
 */
UINT DD_Init(void);

/*
 * DD_Shutdown                                                    0x0045BAA0
 *
 * Releases all DirectDraw resources in safe dependency order.
 * Idempotent: no-op if already shut down.
 *
 * WIN32: IDirectDraw::SetCooperativeLevel + Release cascade
 * LINUX: SDL_DestroyRenderer + SDL_DestroyWindow + SDL_Quit
 */
void DD_Shutdown(void);

/*
 * DD_ReattachClipper                                             0x0045B940
 *
 * Re-binds the window clipper to the primary surface after a mode change.
 * Creates a new IDirectDrawClipper if none is currently attached.
 *
 * WIN32: IDirectDrawSurface::GetClipper / SetClipper / IDirectDrawClipper::SetHWnd
 * LINUX: Not required — SDL_RenderSetClipRect handles clipping per-frame.
 */
void DD_ReattachClipper(void);

/*
 * DD_SetPixelFormatGlobals                                       0x0045B9B0
 *
 * Reads the pixel format from a DirectDraw surface descriptor and writes
 * the results into the global DD_PixelFormat block (g_pixFmtId etc.).
 *
 * Parameters:
 *   pSurf  — surface to query via GetSurfaceDesc
 *   pDesc  — DDSURFACEDESC buffer (caller zero-inits; this function fills it)
 *
 * WIN32: IDirectDrawSurface::GetSurfaceDesc
 * LINUX: Eliminated — SDL2 exposes pixel format via SDL_PixelFormat* directly.
 */
#ifdef _WIN32
void DD_SetPixelFormatGlobals(IDirectDrawSurface *pSurf, DDSURFACEDESC *pDesc);
#endif

/*
 * DD_SetTransparentColorKey                                      0x0045BA50
 *
 * Installs a magenta (R=31, G=0, B=31) source colour key on pSurf so that
 * IDirectDrawSurface::Blt treats that colour as fully transparent.
 *
 * Parameters:
 *   pSurf  — target DirectDraw surface
 *   unused — historical second parameter, always pass 0
 *
 * WIN32: IDirectDrawSurface::SetColorKey (DDCKEY_SRCBLT)
 * LINUX: SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(fmt, 255, 0, 255))
 */
void DD_SetTransparentColorKey(IDirectDrawSurface *pSurf, DWORD unused);

/*
 * DD_LoadBitmap                                                  0x00401000
 *
 * Loads a BMP file from disk into a new DirectDraw offscreen surface.
 * Tries video memory first when useVideoMem is TRUE; falls back to system
 * memory on failure.  Installs a magenta colour key and copies pixels.
 *
 * Parameters:
 *   path        — filesystem path to a BMP file
 *   unused      — reserved; pass 0
 *   width       — desired width  (0 = use bitmap's natural width)
 *   height      — desired height (0 = use bitmap's natural height)
 *   useVideoMem — TRUE = prefer DDSCAPS_VIDEOMEMORY; FALSE = system memory
 *
 * Returns: IDirectDrawSurface* on success, NULL on failure.
 *
 * WIN32: GetFileAttributesA, LoadImageA, GetObjectA, IDirectDraw::CreateSurface
 * LINUX: SDL_LoadBMP + SDL_SetColorKey + SDL_CreateTextureFromSurface
 */
IDirectDrawSurface *DD_LoadBitmap(
    LPCSTR path,
    DWORD  unused,
    int    width,
    int    height,
    BOOL   useVideoMem);

/*
 * DD_CopyBitmapToSurface                                         0x00401170
 *
 * Copies a GDI HBITMAP into a DirectDraw surface using the GDI-compat DC
 * path (GetDC / StretchBlt / ReleaseDC).  Restores the surface if lost.
 *
 * Parameters:
 *   pSurf   — destination DirectDraw surface
 *   hBitmap — source GDI HBITMAP handle
 *   unused  — reserved; pass 0
 *   srcW    — source width  (0 = use bitmap's natural width)
 *   srcH    — source height (0 = use bitmap's natural height)
 *
 * Returns: HRESULT from IDirectDrawSurface::GetDC (DD_OK = 0 on success).
 *
 * WIN32: CreateCompatibleDC, SelectObject, StretchBlt, DeleteDC, GetObjectA
 * LINUX: SDL_BlitScaled(srcSurf, NULL, dstSurf, &dstRect)
 */
int DD_CopyBitmapToSurface(
    IDirectDrawSurface *pSurf,
    HANDLE              hBitmap,
    DWORD               unused,
    int                 srcW,
    int                 srcH);

/*
 * DD_GetSurfaceDesc                                              0x004014E0
 *
 * Retrieves the DDSURFACEDESC for a surface into a zero-initialised local
 * buffer.  Sets dwSize before the call as required by the DirectDraw ABI.
 *
 * Parameters:
 *   pSurf — surface to query
 *
 * WIN32: IDirectDrawSurface::GetSurfaceDesc
 * LINUX: SDL_QueryTexture / SDL_GetWindowSize
 */
void DD_GetSurfaceDesc(IDirectDrawSurface *pSurf);

/*
 * DD_BlitToScreen                                                0x00401280
 *
 * Blits a rectangle from the back buffer (g_pDDSBack) to the primary
 * surface (g_pDDSPrimary) with correct window-relative coordinate mapping.
 * Handles DDERR_SURFACELOST by calling Restore() and retrying.
 *
 * Parameters:
 *   pSrcRect      — source rectangle in back-buffer coordinates
 *   hWnd          — window whose client origin is used for coordinate mapping
 *   pScrollOffset — optional [x, y] scroll origin subtracted before mapping
 *                   (pass NULL for no scroll adjustment)
 *   forceBlt      — if TRUE, forces a second DDBLT_WAIT blit on success
 *
 * WIN32: IDirectDrawSurface::Blt, IsRectEmpty, ClientToScreen, GetWindowRect
 * LINUX: SDL_RenderCopy(renderer, backTex, &srcRect, &dstRect)
 */
void DD_BlitToScreen(
    RECT  *pSrcRect,
    HWND   hWnd,
    int   *pScrollOffset,
    BOOL   forceBlt);

/*
 * DD_SendFrameMessage                                            0x0045E1E0
 *
 * Triggers a rendered frame by posting WM_USER+7 (0x407) to the main
 * window.  Called from the game loop when g_renderActive is non-zero.
 *
 * Parameters:
 *   frameParam — low byte is passed as wParam of the window message
 *
 * WIN32: SendMessageA(hWndMain, WM_USER+7, frameParam & 0xFF, 0)
 * LINUX: SDL_RenderPresent(renderer) — message-pump indirection removed.
 */
void DD_SendFrameMessage(UINT frameParam);

/*
 * DD_ShowSplashScreen                                            0x0045E090
 *
 * Clears the back buffer to black, constructs the loading-screen sprite,
 * centres it on screen, and blits it to the primary surface.
 *
 * WIN32: PlaySoundA, GetStockObject/FillRect, GetSystemMetrics
 * LINUX: Mix_HaltMusic + SDL_RenderClear + IMG_LoadTexture + SDL_RenderCopy
 */
void DD_ShowSplashScreen(void);

/*
 * DS_Init                                                        0x0045B7E0
 *
 * Allocates and initialises the DirectSound device, reads stored volume
 * levels from the config store, and applies them.
 *
 * Returns 1 on success, 0 on failure.
 *
 * WIN32: DirectSound COM vtable; config store helpers
 * LINUX: Mix_OpenAudio + Mix_AllocateChannels + Mix_Volume
 */
UINT DS_Init(void);

/*
 * DS_SaveAndShutdown                                             0x0045BB20
 *
 * Saves current volume settings to the config store then releases the
 * DirectSound device object.  Companion to DS_Init.
 *
 * WIN32: DirectSound COM vtable; config store helpers
 * LINUX: Mix_CloseAudio; write volume to application config file
 */
void DS_SaveAndShutdown(void);

/*
 * APP_InitWindow                                                 0x004204D0
 *
 * Creates the main application window (full-screen sized), loads the game
 * icon, registers the window class, instantiates the two manager sub-objects,
 * creates the child render-area window, subclasses it, and gives it focus.
 *
 * Parameters:
 *   self       — application object pointer (APP_Window* equivalent)
 *   hWndParent — parent window; NULL for a top-level window
 *
 * Returns 1 on success, 0 if main window creation failed.
 *
 * WIN32: GetDesktopWindow, LoadIconA, CreateWindowExA, SetWindowLongA, SetFocus
 * LINUX: SDL_CreateWindow + SDL_CreateRenderer + SDL_SetWindowFullscreen
 */
int APP_InitWindow(void *self, HWND hWndParent);

/* ═══════════════════════════════════════════════════════════════════════════
 * Linux / SDL2 port stubs
 *
 * When building without DirectX headers these thin wrappers stand in for
 * the graphics entry points.  Each stub documents the SDL2 call(s) that
 * should replace the Win32 implementation.
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef _WIN32

/*
 * LINUX_DD_Init — create the SDL2 window and renderer.
 *
 *   SDL_Window   *win = SDL_CreateWindow("Lego Loco",
 *                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
 *                           640, 480, SDL_WINDOW_SHOWN);
 *   SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
 *                           SDL_RENDERER_ACCELERATED |
 *                           SDL_RENDERER_PRESENTVSYNC);
 *
 * Returns 1 on success, 0 on failure.
 */
static inline int LINUX_DD_Init(void) { return SDL_Init(SDL_INIT_VIDEO) == 0; }

/*
 * LINUX_DD_Shutdown — destroy renderer and window, quit SDL.
 *
 *   SDL_DestroyRenderer(ren);
 *   SDL_DestroyWindow(win);
 *   SDL_Quit();
 */
static inline void LINUX_DD_Shutdown(void) { SDL_Quit(); }

/*
 * LINUX_DD_BlitToScreen — present the current render target.
 *
 *   SDL_RenderCopy(renderer, backTexture, &srcRect, &dstRect);
 *   SDL_RenderPresent(renderer);
 */
static inline void LINUX_DD_BlitToScreen(void) { /* SDL_RenderPresent(renderer); */ }

/*
 * LINUX_DD_SendFrameMessage — call SDL_RenderPresent directly.
 *
 *   SDL_RenderPresent(renderer);
 */
static inline void LINUX_DD_SendFrameMessage(void) { /* SDL_RenderPresent(renderer); */ }

/*
 * LINUX_DD_SetTransparentColorKey — set SDL colour key for a surface.
 *
 *   SDL_SetColorKey(surface, SDL_TRUE,
 *       SDL_MapRGB(surface->format, 255, 0, 255));
 */
static inline void LINUX_DD_SetTransparentColorKey(SDL_Surface *s) {
    SDL_SetColorKey(s, SDL_TRUE,
                    SDL_MapRGB(s->format, 255, 0, 255));
}

/*
 * LINUX_DD_ShowSplashScreen — clear to black and display splash texture.
 *
 *   Mix_HaltMusic();
 *   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
 *   SDL_RenderClear(renderer);
 *   // Load and center splash texture:
 *   SDL_Texture *tex = IMG_LoadTexture(renderer, splashPath);
 *   SDL_RenderCopy(renderer, tex, NULL, &centeredRect);
 *   SDL_RenderPresent(renderer);
 */
static inline void LINUX_DD_ShowSplashScreen(void) { /* see comment above */ }

/*
 * LINUX_DS_Init — open the SDL_mixer audio device.
 *
 *   Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
 *   Mix_AllocateChannels(32);
 *   Mix_Volume(-1, savedVolume);
 */
static inline int LINUX_DS_Init(void) {
    return Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == 0;
}

/*
 * LINUX_DS_SaveAndShutdown — close SDL_mixer.
 *
 *   // save volume to config
 *   Mix_CloseAudio();
 */
static inline void LINUX_DS_SaveAndShutdown(void) { Mix_CloseAudio(); }

#endif /* !_WIN32 */

#endif /* GRAPHICS_H */
