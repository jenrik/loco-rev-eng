/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: CURSOR_INPUT — Custom software cursor compositing and input capture
 * Original: loco.exe (Windows 95/98, DirectX 5 era)
 * Developer: Intelligent Games for LEGO Media
 *
 * This file was produced by reverse engineering the original binary.
 * Windows API calls are marked with WIN32: comments.
 * Linux/SDL2 replacement suggestions are marked with LINUX: comments.
 *
 * Analyzed functions:
 *   0x004140a0  UpdateClientRect
 *   0x00414130  InitCursorResources
 *   0x00414290  SetCursorCapture
 *   0x00414340  SetCursorState
 *   0x00414a80  OnWindowActivate
 *   0x00414b80  OnWindowDestroy
 *   0x00414bb0  WaitForSurface
 *   0x00414c20  RenderCursor
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stddef.h>

/* =========================================================================
 * Platform abstraction: SDL2 vs Win32
 *
 * Build with -DLOCO_LINUX to activate the SDL2 path.
 * Without that flag the Win32 types and stubs are used, allowing the code
 * to compile on Windows or for pure analysis purposes.
 * ========================================================================= */

#ifdef LOCO_LINUX
/* ---- Linux / SDL2 path ---- */
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>

/* Win32 primitive types remapped to standard C / SDL equivalents */
typedef void           *HWND;          /* LINUX: SDL_Window* */
typedef unsigned int    UINT;
typedef unsigned long   DWORD;
typedef int             INT;
typedef int             BOOL;
typedef char           *LPSTR;
typedef const char     *LPCSTR;
typedef long            LONG;

/* RECT is used internally; map to an SDL_Rect-compatible layout */
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *LPRECT;

/*
 * WIN32: GetClientRect(hwnd, &rect)
 *   Fills rect with {0, 0, clientWidth, clientHeight}.
 * LINUX: SDL_GetWindowSize(window, &w, &h)
 *   SDL client area equals the window drawable area; no title-bar offset.
 */
static inline void loco_GetClientRect(SDL_Window *win, RECT *r)
{
    int w = 0, h = 0;
    SDL_GetWindowSize(win, &w, &h);
    r->left   = 0;
    r->top    = 0;
    r->right  = w;
    r->bottom = h;
}
#define GetClientRect(hwnd, r)  loco_GetClientRect((SDL_Window *)(hwnd), (r))

/*
 * WIN32: SetCapture / ReleaseCapture / GetCapture
 * LINUX: SDL_CaptureMouse + SDL_SetWindowGrab
 */
#define SetCapture(hwnd)        (SDL_CaptureMouse(SDL_TRUE),  \
                                 SDL_SetWindowGrab((SDL_Window *)(hwnd), SDL_TRUE))
#define ReleaseCapture()        (SDL_CaptureMouse(SDL_FALSE), \
                                 SDL_SetWindowGrabAny(SDL_FALSE))
#define GetCapture()            ((HWND)NULL) /* SDL has no direct equivalent */

/*
 * WIN32: ShowCursor(FALSE) — loops until per-thread counter < 0
 * LINUX: SDL_ShowCursor(SDL_DISABLE) — single call; no counter
 */
#define ShowCursor(show)        SDL_ShowCursor((show) ? SDL_ENABLE : SDL_DISABLE)

/*
 * WIN32: GetCursorPos(&pt) — returns screen (global) coordinates
 * LINUX: SDL_GetGlobalMouseState(&x, &y) — same coordinate space
 */
typedef struct tagPOINT { LONG x; LONG y; } POINT;
static inline BOOL loco_GetCursorPos(POINT *p)
{
    SDL_GetGlobalMouseState(&p->x, &p->y);
    return 1;
}
#define GetCursorPos(p)         loco_GetCursorPos(p)

/*
 * WIN32: ClientToScreen(hwnd, &pt)
 *   Converts window-local (0,0) to screen coords of the window origin.
 * LINUX: SDL_GetWindowPosition(window, &wx, &wy)
 */
static inline BOOL loco_ClientToScreen(SDL_Window *win, POINT *p)
{
    int wx = 0, wy = 0;
    SDL_GetWindowPosition(win, &wx, &wy);
    p->x += wx;
    p->y += wy;
    return 1;
}
#define ClientToScreen(hwnd, p) loco_ClientToScreen((SDL_Window *)(hwnd), (p))

/*
 * WIN32: GDI rect helpers
 * LINUX: thin inline wrappers operating on RECT
 */
static inline BOOL SetRect(LPRECT r, int l, int t, int ri, int b)
{ r->left=l; r->top=t; r->right=ri; r->bottom=b; return 1; }
static inline BOOL CopyRect(LPRECT dst, const RECT *src)
{ *dst = *src; return 1; }
static inline BOOL OffsetRect(LPRECT r, int dx, int dy)
{ r->left+=dx; r->right+=dx; r->top+=dy; r->bottom+=dy; return 1; }

/*
 * WIN32: DestroyWindow(hwnd) / PostQuitMessage(0)
 * LINUX: SDL_DestroyWindow + SDL_PushEvent(SDL_QUIT)
 */
static inline BOOL loco_DestroyWindow(SDL_Window *win)
{ SDL_DestroyWindow(win); return 1; }
#define DestroyWindow(hwnd) loco_DestroyWindow((SDL_Window *)(hwnd))
static inline void loco_PostQuitMessage(int code)
{
    (void)code;
    SDL_Event ev;
    ev.type = SDL_QUIT;
    SDL_PushEvent(&ev);
}
#define PostQuitMessage(code)   loco_PostQuitMessage(code)

/*
 * WIN32: Sleep(ms) / ExitProcess(code)
 * LINUX: SDL_Delay(ms) / exit(code)
 */
#define Sleep(ms)               SDL_Delay(ms)
#define ExitProcess(code)       (SDL_Quit(), exit(code))

/*
 * WIN32: OutputDebugStringA(str) — writes to debugger output
 * LINUX: fprintf(stderr, ...) or SDL_Log(...)
 */
#define OutputDebugStringA(s)   SDL_Log("%s", (s))

#elif defined(_WIN32)
/* ---- Native Win32 build ---- */
#include <windows.h>
#else
/* ---- Analysis build on Linux (no SDL2, no windows.h) ---- */
/* Provide the minimal Win32 type stubs needed for parsing this header. */
typedef void            *HWND;
typedef void            *HCURSOR;
typedef unsigned int     UINT;
typedef unsigned long    DWORD;
typedef int              BOOL;
typedef char            *LPSTR;
typedef const char      *LPCSTR;
typedef long             LONG;
typedef struct { LONG left, top, right, bottom; } RECT, *LPRECT;
typedef struct { LONG x, y; } POINT;
static inline int   ShowCursor(int s) { (void)s; return 0; }
static inline BOOL  GetCursorPos(POINT *p) { (void)p; return 0; }
static inline HWND  SetCapture(HWND h) { (void)h; return 0; }
static inline BOOL  ReleaseCapture(void) { return 0; }
static inline HWND  WindowFromPoint(POINT p) { (void)p; return 0; }
static inline BOOL  ClientToScreen(HWND h, POINT *p) { (void)h; (void)p; return 0; }
static inline BOOL  ScreenToClient(HWND h, POINT *p) { (void)h; (void)p; return 0; }
static inline HCURSOR LoadCursorFromFileA(const char *path) { (void)path; return 0; }
static inline HCURSOR SetCursor(HCURSOR hc) { (void)hc; return 0; }
static inline BOOL  PtInRect(const RECT *r, POINT p)
    { return p.x>=r->left && p.x<r->right && p.y>=r->top && p.y<r->bottom; }
static inline BOOL  SetRect(LPRECT r, int l, int t, int ri, int b)
    { r->left=l; r->top=t; r->right=ri; r->bottom=b; return 1; }
static inline BOOL  CopyRect(LPRECT dst, const RECT *src)
    { *dst = *src; return 1; }
static inline BOOL  OffsetRect(LPRECT r, int dx, int dy)
    { r->left+=dx; r->right+=dx; r->top+=dy; r->bottom+=dy; return 1; }
#define OutputDebugStringA(s) ((void)(s))
#endif /* LOCO_LINUX / _WIN32 / analysis */

/* =========================================================================
 * Constants
 * ========================================================================= */

/* =========================================================================
 * Cursor Resource IDs
 * =========================================================================
 * All cursor sprite resources are in the 0x1400..0x17FF range (type 5),
 * which is always marked persistent and never evicted from the cache.
 * Looked up via RESMGR_GetResource(g_ResourceManager, id).
 * ========================================================================= */

/* Core cursor sprites */
#define CURSOR_RES_IDLE         0x1400u /* idle / default open-hand cursor            */
#define CURSOR_RES_ROTATE       0x1402u /* rotate / alternate mode cursor             */
#define CURSOR_RES_GRAB         0x1403u /* shadow sprite composited under main cursor  */
#define CURSOR_RES_PLACE        0x1404u /* grab / place cursor (over draggable object) */
#define CURSOR_RES_OPEN_HAND    0x1405u /* open-hand hover (carrying item, valid tile) */

/* Edge-scroll cursor IDs (substituted near viewport edges in BuildModeCursor) */
#define CURSOR_RES_SCROLL_RIGHT 0x0C26u /* within SCROLL_EDGE_LRT_PX of right edge   */
#define CURSOR_RES_SCROLL_UP    0x0C28u /* within SCROLL_EDGE_LRT_PX of top edge     */
#define CURSOR_RES_SCROLL_DOWN  0x0C2Au /* within SCROLL_EDGE_BOT_PX of bottom edge  */
#define CURSOR_RES_SCROLL_LEFT  0x0C2Cu /* within SCROLL_EDGE_LRT_PX of left edge    */
#define CURSOR_RES_SCROLL_UL    0x0C42u /* diagonal upper-left scroll                 */
#define CURSOR_RES_SCROLL_UR    0x0C44u /* diagonal upper-right scroll                */
#define CURSOR_RES_SCROLL_DL    0x0C46u /* diagonal lower-left scroll                 */
#define CURSOR_RES_SCROLL_DR    0x0C48u /* diagonal lower-right scroll                */

/* Rail-scroll axis-clamp cursor IDs (constrain mouse to one axis on warp) */
#define CURSOR_RES_RAIL_CLAMP_Y1  0x0C1Cu /* clamp Y movement (rail type A)          */
#define CURSOR_RES_RAIL_CLAMP_Y2  0x3408u /* clamp Y movement (rail type B)          */
#define CURSOR_RES_RAIL_CLAMP_X1  0x0C1Au /* clamp X movement (rail type A)          */
#define CURSOR_RES_RAIL_CLAMP_X2  0x3409u /* clamp X movement (rail type B)          */

/* =========================================================================
 * Game Mode Values  (global at DAT_004851F4 / g_GameMode)
 * =========================================================================
 * Set exclusively through GameMode_Set (0x00408130).
 * InputCursor_UpdateCursorState dispatches on these values every frame.
 * ========================================================================= */
#define GAME_MODE_LOADING       1  /* splash / load screen; cursor updates suppressed */
#define GAME_MODE_TRANSITION    2  /* between modes; DDraw cursor hidden, OS shown    */
#define GAME_MODE_PLACE         3  /* place / train-layout mode                       */
#define GAME_MODE_BUILD         4  /* build / scroll mode                             */
#define GAME_MODE_CREDITS       5  /* credits screen                                  */
#define GAME_MODE_OPTIONS       6  /* options dialog                                  */
#define GAME_MODE_HELP          7  /* help window                                     */
#define GAME_MODE_SAVE          8  /* save-state transition                           */
#define GAME_MODE_SETTINGS      9  /* options / settings panel                        */
#define GAME_MODE_SHUTDOWN     10  /* drains message queue, posts WM_QUIT             */

/* =========================================================================
 * Edge-Scroll Thresholds and Tile Grid Constants
 * ========================================================================= */
#define SCROLL_EDGE_LRT_PX  0x10  /* left/right/top scroll trigger zone (16 px)     */
#define SCROLL_EDGE_BOT_PX  0x40  /* bottom scroll trigger zone (64 px, for toolbar) */
#define TILE_SIZE_PX        16    /* pixels per isometric grid tile in screen space   */
#define DRAG_ITEMS_MAX       7    /* max items a locomotive can carry at once         */

/* Shared cursor compositing surface dimensions */
#define CURSOR_SURFACE_DIM      0x100   /* 256 pixels wide and tall */

/* DDSURFACEDESC2 dwSize field — identifies the DirectDraw structure version */
#define DDSURFACEDESC2_SIZE     0x7c    /* 124 bytes */

/* dwFlags for the cursor DDSURFACEDESC2: DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH */
#define DDSD_FLAGS_CURSOR       0x07

/* ddsCaps.dwCaps: DDSCAPS_OFFSCREENPLAIN (0x40) | DDSCAPS_SYSTEMMEMORY (0x800) */
#define DDSCAPS_CURSOR_SURFACE  0x840

/* DirectDraw Blt flags for cursor-frame-to-staging blit:
 *   DDBLT_WAIT (0x1000000) | DDBLT_KEYSRC (0x8000) — color-key transparency */
#define DDBLT_SRC_FLAGS         0x1008000

/* DirectDraw Blt flags for staging-to-primary blit: DDBLT_WAIT only */
#define DDBLT_DST_FLAGS         0x1000000

/* Surface-ready poll interval in milliseconds (WaitForSurface) */
#define SURFACE_WAIT_INTERVAL_MS    10

/* Maximum poll iterations before ExitProcess(1) — 10 s total at 10 ms each */
#define SURFACE_WAIT_MAX_RETRIES    1000

/* Sentinel value for CursorManager lastBlitX/lastBlitY: position not yet recorded */
#define CURSOR_INVALID_POS      0xFFFFFFFF

/* =========================================================================
 * Data Structures
 * ========================================================================= */

/*
 * CursorFrameSet  --  one animation-set descriptor, 0x18 bytes per entry.
 *
 * Parsed from the binary cursor sprite resource by CursorSpriteResource_Parse
 * (0x00424E00).  The parent CursorAnimData allocates (num_frame_sets * 0x18)
 * bytes at +0x20 to hold all entries.
 *
 * Two index fields on CursorAnimData select the active sets:
 *   cursorFrameSet        -- the frame set played while this cursor is active
 *   cursorDefaultFrameSet -- the idle / looping default animation set
 *
 * -------------------------------------------------------------------------
 * ANIMATION UPDATE LOOP
 * -------------------------------------------------------------------------
 * The loop runs every timer tick in CursorWindow_Tick (0x00426EB0) and every
 * frame in the CursorRenderer dirty-repaint path (0x00414FB0 / 0x00415440).
 *
 * The sprite sheet is a single horizontal row:
 *
 *   [ frame 0 ][ frame 1 ][ frame 2 ] ... [ frame N-1 ]
 *   <--fw----><--fw----><--fw---->     <---fw-------->
 *
 * where fw = CursorAnimData.frameWidth.  All frames share one row (src_y = 0).
 *
 * HOT-PATH LOOP (observed in decompile — fast path when one set is active):
 *
 *   // renderer->anim_frame is uint16_t at CursorManager+0x48 (CursorRenderer)
 *   // or CursorWindow+0x24 (CGWND timer path)
 *
 *   if (anim->frameCount > 1) {
 *       renderer->anim_frame =
 *           (renderer->anim_frame + 1) % anim->frameCount;
 *   }
 *   uint16_t src_x = renderer->anim_frame * anim->frameWidth;
 *   uint16_t src_y = 0;
 *   // Blit frame strip [ src_x, 0, src_x+frameWidth, frameHeight ]
 *   //   onto the primary/back-buffer at the cursor screen position.
 *
 * FULL FRAME-SET AWARE LOOP (conceptual reconstruction):
 *
 *   CursorFrameSet *fs = &anim->frame_sets[active_set_index];
 *
 *   if (renderer->anim_frame < fs->start_frame ||
 *       renderer->anim_frame > fs->end_frame) {
 *       renderer->anim_frame = fs->start_frame;   // reset on set change
 *       renderer->tick_ctr   = 0;
 *   }
 *   if (++renderer->tick_ctr >= fs->duration) {
 *       renderer->tick_ctr = 0;
 *       if (++renderer->anim_frame > fs->end_frame) {
 *           renderer->anim_frame = fs->loop ? fs->start_frame : fs->end_frame;
 *       }
 *   }
 *   uint16_t src_x = renderer->anim_frame * anim->frameWidth;
 *
 * LINUX port: replace the DirectDraw BltFast call with:
 *   SDL_Rect src = { (int)src_x, 0,
 *                    (int)anim->frameWidth, (int)anim->frameHeight };
 *   SDL_Rect dst = { cursor_screen_x, cursor_screen_y,
 *                    (int)anim->frameWidth, (int)anim->frameHeight };
 *   SDL_RenderCopy(renderer, cursor_texture, &src, &dst);
 * -------------------------------------------------------------------------
 */
typedef struct CursorFrameSet {
    /* +0x00 */ uint16_t start_frame;   /* first frame index in this set (0-based)   */
    /* +0x02 */ uint16_t end_frame;     /* last frame index in this set (inclusive)   */
    /* +0x04 */ uint16_t duration;      /* game-ticks between frame advances;
                                         * min-clamped to 1 during parse to prevent
                                         * divide-by-zero in the tick counter         */
    /* +0x06 */ uint16_t sound_id_0;    /* sound resource IDs associated with this set;
                                         * exact usage (entry / loop / exit) TBD from
                                         * parse context; three 16-bit IDs observed   */
    /* +0x08 */ uint16_t sound_id_1;
    /* +0x0A */ uint16_t sound_id_2;
    /* +0x0C */ uint32_t _unknown_0c;   /* unresolved; possibly event or state flags  */
    /* +0x10 */ uint32_t _unknown_10;   /* unresolved                                 */
    /* +0x14 */ uint16_t _unknown_14;   /* unresolved                                 */
    /* +0x16 */ uint8_t  playback_mode; /* animation playback mode:
                                         *   0 = forward, one-shot or loop per 'loop'
                                         *   1 = ping-pong (inferred; TBD)           */
    /* +0x17 */ uint8_t  loop;          /* non-zero = loop this frame set repeatedly;
                                         * 0 = play once then hold the last frame     */
    /* sizeof = 0x18 = 24 bytes; confirmed by parser: allocates num_frame_sets*0x18  */
} CursorFrameSet;

/* Animation flag bits in CursorAnimData.animFlags */
#define ANIM_FLAG_SEMI_TRANSPARENT  0x400u  /* blend with background on blit          */
#define ANIM_FLAG_HAS_SHADOW        0x002u  /* draw shadow sprite (shadowId) behind   */

/*
 * CursorAnimData  --  animation metadata parsed from a cursor sprite resource.
 *
 * Parsed by CursorSpriteResource_Parse (0x00424E00).  The sprite sheet is a
 * single horizontal row of frameCount frames, each frameWidth x frameHeight
 * pixels.  The frame_sets array points to num_frame_sets CursorFrameSet entries
 * that control which sub-range of frames plays in each cursor state.
 *
 * Field offsets confirmed by Ghidra decompile of FUN_00424E00.
 * Minimum allocated size: at least 0x52F bytes (field at +0x52E observed).
 */
typedef struct CursorAnimData {
    /* +0x00 */ void    **vtable;        /* C++ vtable pointer                        */
    /* +0x04 */ uint32_t  resourceId;    /* sprite resource ID (e.g. 0x1400)          */
    /* +0x08 */ uint8_t   cursorType;    /* 0x05 = isometric-drag (uses tile grid),
                                          * 0x01 = normal/validated pointer            */
    /* +0x09 */ uint8_t   validFlag;     /* must == 1 for sprite to pass Cursor_SetType*/
    /* +0x0A */ uint8_t   _pad0a[2];
    /* +0x0C */ uint32_t  shadowId;      /* resource ID of shadow sprite (0x1403);
                                          * 0 = no shadow                              */
    /* +0x10 */ uint8_t   _pad10[4];
    /* +0x14 */ uint16_t  frameWidth;    /* pixel width of ONE animation frame;
                                          * src_x = anim_frame * frameWidth            */
    /* +0x16 */ uint16_t  frameHeight;   /* pixel height of each frame                 */
    /* +0x18 */ uint8_t   _pad18[2];
    /* +0x1A */ uint16_t  num_frame_sets;         /* count of CursorFrameSet entries   */
    /* +0x1C */ uint16_t  cursorFrameSet;          /* index into frame_sets[] for the
                                                     * active cursor-state animation    */
    /* +0x1E */ uint16_t  cursorDefaultFrameSet;   /* index of the idle / default set  */
    /* +0x20 */ CursorFrameSet *frame_sets;        /* heap array, num_frame_sets*0x18  */
    /* +0x24 */ uint8_t   _pad24[4];
    /* +0x28 */ uint16_t  animDelayMin;  /* min random initial-delay (ticks) before
                                          * animation starts after spawn               */
    /* +0x2A */ uint16_t  animDelayMax;  /* max random initial-delay                   */
    /* +0x2C */ uint16_t  buttonConfig0; /* button / interaction config fields          */
    /* +0x2E */ uint16_t  buttonConfig1; /* (exact semantics TBD from parse context)   */
    /* +0x30 */ uint16_t  buttonConfig2;
    /* +0x32 */ int16_t   hotspotX;      /* cursor hotspot offset from frame top-left;
                                          * subtracted from GetCursorPos before blit   */
    /* +0x34 */ int16_t   hotspotY;
    /* +0x36 */ uint8_t   _pad36[2];
    /* +0x38 */ int32_t   shadowOffsetX; /* X offset to draw shadow relative to cursor */
    /* +0x3C */ int32_t   shadowOffsetY;
    /* +0x40 */ uint32_t  mustHaveFlags; /* 'must' attribute mask for animation select */
    /* +0x44 */ uint32_t  cantHaveFlags; /* 'cant_have' attribute mask                 */
    /* +0x48 */ uint8_t   _pad48[0x105];/* unresolved fields                          */
    /* +0x14D */ char     name[0xF];     /* display name string (null-terminated)      */
    /* +0x15C */ uint16_t maxInstances;  /* MaxInstances from resource definition      */
    /* +0x15E */ uint8_t  _pad15e[2];
    /* +0x160 */ uint16_t frameCount;    /* total animation frames across ALL sets;
                                          * min-clamped to 1; drives the hot-path wrap
                                          * in the animation update loop               */
    /* +0x162 */ uint8_t  _pad162[2];
    /* +0x164 */ uint32_t animFlags;     /* animation attribute flags:
                                          *   ANIM_FLAG_SEMI_TRANSPARENT (0x400)
                                          *   ANIM_FLAG_HAS_SHADOW       (0x002)      */
    /* +0x168 */ uint8_t  _pad168[5];
    /* +0x16D */ uint8_t  heightOffset;  /* Y hotspot adjustment in SetCursorSprite
                                          * when cursorType != 5                       */
    /* struct continues; confirmed size >= 0x52F */
    /* +0x52E */ uint16_t frameSetRedirect; /* right-click cursor ID redirect:
                                              * when non-zero, RightButtonDown mutates
                                              * the active cursor to this ID           */
} CursorAnimData;

/*
 * CursorAnimResource  --  vtable object returned by the resource manager for
 * cursor animation resources (IDs 0x1400, 0x1403, etc.).
 *
 * Accessed via vtable slot +4 (getLocoBitmap) to obtain the backing surface.
 * Struct size unknown; fields beyond +0x16 not yet observed.
 */
typedef struct CursorAnimResource {
    void    **vtable;       /* +0x00  C++ vtable pointer */
                            /*        vtable[1] = getLocoBitmap() -> surface ptr */
    /* +0x04..+0x13  unknown */
    uint8_t   _pad[0x10];
    uint16_t  frameWidth;   /* +0x14  frame width (mirrors CursorAnimData+0x14) */
    uint16_t  frameHeight;  /* +0x16  frame height */
    /* remaining fields unknown */
} CursorAnimResource;

/*
 * CursorManager  --  the game's custom software-cursor manager.
 *
 * One instance exists per game window.  Manages:
 *   - OS cursor capture / hide
 *   - Animation state machine (which .ani strip is shown, which frame)
 *   - Per-frame compositing: sprite strip -> 256x256 staging surface -> primary
 *   - Client-rect caching for viewport clipping
 *
 * All field offsets are from the object base pointer (this).
 * Total size: at least 0x114 (276) bytes.
 *
 * WIN32 equivalents are noted per field where the platform matters.
 * LINUX: HWND -> SDL_Window*; surface pointers -> SDL_Surface*.
 */
typedef struct CursorManager {
    void            **vtable;           /* +0x00  C++ vtable (implied by __thiscall) */
    /* +0x04  unknown / padding */
    uint8_t           _pad0[4];
    void             *hwnd;             /* +0x08  game window handle
                                         *  WIN32: HWND   LINUX: SDL_Window* */
    void             *parentHwnd;       /* +0x0c  parent window; NULL = top-level
                                         *  WIN32: HWND   LINUX: SDL_Window* or NULL */
    /* +0x10..+0x13  unknown */
    uint8_t           _pad1[4];
    int32_t           cursorStateId;    /* +0x14  active animation state ID (0 = hidden) */
    /* +0x18  viewport scroll offsets used during RenderCursor compositing */
    int32_t           scrollX;          /* +0x18  horizontal scroll offset (pixels) */
    int32_t           scrollY;          /* +0x1c  vertical scroll offset (pixels) */
    int32_t           viewportRight;    /* +0x20  right clipping boundary */
    int32_t           viewportBottom;   /* +0x24  bottom clipping boundary */
    /* +0x28..+0x37  unknown */
    uint8_t           _pad2[0x10];
    void             *pGameSurface;     /* +0x38  game-world surface (vtable object)
                                         *  WIN32: IDirectDrawSurface4*
                                         *  LINUX: SDL_Surface* or SDL_Texture* */
    uint32_t          frameWidth;       /* +0x3c  idle cursor frame width (pixels) */
    uint32_t          frameHeight;      /* +0x40  idle cursor frame height (pixels) */
    CursorAnimData   *pCurrentAnim;     /* +0x44  active animation resource data ptr */
    int32_t           currentAnimFrame; /* +0x48  current frame index; wraps at frameCount */
    /* +0x4c..+0x4f  unknown */
    uint8_t           _pad3[4];
    int32_t           lastBlitX;        /* +0x50  last cursor blit X (-1 = not yet drawn) */
    int32_t           lastBlitY;        /* +0x54  last cursor blit Y (-1 = not yet drawn) */
    char              deactivated;      /* +0x58  1 = capture released, OS cursor visible */
    /* +0x59..+0x5b  padding */
    uint8_t           _pad4[3];
    void             *pCursorSurface;   /* +0x5c  shared 256x256 staging surface (ref-counted)
                                         *  WIN32: IDirectDrawSurface4* (DAT_004fd3cc)
                                         *  LINUX: SDL_Surface* (256x256, 32bpp) */
    /* +0x60..+0x67  unknown */
    uint8_t           _pad5[8];
    LONG              destLeft;         /* +0x68  last drawn cursor dest rect left */
    LONG              destTop;          /* +0x6c  last drawn cursor dest rect top */
    LONG              destRight;        /* +0x70  last drawn cursor dest rect right */
    LONG              destBottom;       /* +0x74  last drawn cursor dest rect bottom */
    LONG              auxRect[4];       /* +0x78..+0x84  auxiliary dirty/prev-frame rects
                                         *               zeroed by SetCursorState */
    /* +0x88 */
    char              hasOverlayCursor; /* +0x88  1 = secondary cursor overlay active */
    /* +0x89..+0x8f  padding */
    uint8_t           _pad6[7];
    DWORD             idleSurfacePitch; /* +0x90  pitch of idle cursor surface (from +0x1c) */
    void             *pIdleSurface;     /* +0x94  locked pixel ptr for resource 0x1400
                                         *  WIN32: DDSURFACEDESC2.lpSurface
                                         *  LINUX: SDL_Surface->pixels after SDL_LockSurface */
    CursorAnimResource *pIdleCursorRes; /* +0x98  resource object for 0x1400 (idle cursor) */
    DWORD             grabSurfacePitch; /* +0x9c  pitch of grab cursor surface */
    void             *pGrabSurface;     /* +0xa0  locked pixel ptr for resource 0x1403 */
    CursorAnimResource *pGrabCursorRes; /* +0xa4  resource object for 0x1403 (grab/busy) */
    /* +0xa8..+0xda  unknown fields */
    uint8_t           _pad7[0x33];
    char              active;           /* +0xdb  1 = initialized and window valid
                                         *        cleared by OnWindowDestroy */
    /* +0xdc..+0xe3  unknown */
    uint8_t           _pad8[8];
    int32_t           clientWidth;      /* +0xe4  cached: GetClientRect right - left */
    int32_t           clientHeight;     /* +0xe8  cached: GetClientRect bottom - top */
    int32_t           clipWidth;        /* +0xec  working clip rect width (same formula) */
    int32_t           clipHeight;       /* +0xf0  working clip rect height */
    RECT              clientRect;       /* +0xf4  raw GetClientRect output {l,t,r,b} */
    /* +0x104  screen-space clip rect used with ClientToScreen offset in RenderCursor */
    LONG              clipLeft;         /* +0x104 */
    LONG              clipTop;          /* +0x108 */
    LONG              clipRight;        /* +0x10c */
    LONG              clipBottom;       /* +0x110 */
    /* minimum struct size = 0x114 */
} CursorManager;

/*
 * InputCursor  --  main per-frame input state object.
 *
 * One instance persists for the lifetime of the game.  Passed as 'this'
 * (or param_1 in Ghidra output) to every InputCursor_* function.
 *
 * All game-space coordinates are isometric tile units after conversion
 * through Mouse_ScreenToIso (FUN_00412060):
 *   game_x = (screen_x + g_ViewPanX) / TILE_SIZE_PX
 *   game_y = (screen_y + g_ViewPanY) / TILE_SIZE_PX
 *
 * Field offsets: param_1[N] as DWORD array => byte offset N*4.
 * Byte-addressed fields noted with exact byte offset.
 */
typedef struct InputCursor {
    /* +0x00 */ void    **vtable;
    /* +0x04 */ uint32_t  _res04;
    /* +0x08 */ void     *worldObj;         /* pointer to world/scene object          */
    /* +0x0C */ uint32_t  _res0c;
    /* +0x10 */ void     *hwnd;             /* game window HWND for ClientToScreen,
                                             * SetCapture, WindowFromPoint             */
    /* +0x14 */ uint32_t  _res14;
    /* +0x18 */ uint8_t   gameCursorEnabled;/* 0x01 = DDraw sprite cursor active;
                                             * 0x00 = show OS cursor                  */
    /* +0x19 */ uint8_t   _pad19[3];
    /* +0x20 */ uint32_t  _res20;
    /* +0x24 */ uint32_t  dirtyFlag;        /* param_1[9]; non-zero = input pending;
                                             * cleared each InputCursor_Tick           */
    /* +0x28 */ uint32_t  _res28[24];
    /* +0x88 */ uint32_t  activeCursorId;   /* param_1[0x22]; currently selected cursor
                                             * resource ID (e.g. CURSOR_RES_IDLE);
                                             * mutated by RightButtonDown on redirect  */
    /* +0x8C */ uint32_t  _res8c;
    /* +0x8E */ uint8_t   warpPending;      /* set by SetSystemCursorMode when cursor
                                             * position needs re-evaluation via
                                             * ClientToScreen + WindowFromPoint        */
    /* +0x8F */ uint8_t   _pad8f;
    /* +0x90 */ uint32_t  warpPackedPos;    /* param_1[0x24]; lo16=X, hi16=Y;
                                             * packed screen-space warp target          */
    /* +0x94 */ void     *cursorRenderer;   /* sub-object; *(cursorRenderer+4) = the
                                             * currently displayed resource ID         */
    /* +0x98 */ uint32_t  _res98;
    /* +0x9C */ int32_t   cursorGameX;      /* param_1[0x27]; current isometric X     */
    /* +0xA0 */ int32_t   cursorGameY;      /* param_1[0x28]; current isometric Y     */
    /* +0xA4 */ uint8_t   leftClickFlag;    /* non-zero = pending left-button event    */
    /* +0xA5 */ uint8_t   _padA5[3];
    /* +0xA8 */ uint32_t  clickPackedL;     /* param_1[0x2A]; packed left-click coords
                                             * lo16=X, hi16=Y; also used for snap-back
                                             * last-valid position                      */
    /* +0xAC */ int32_t   lClickGameX;      /* param_1[0x2B]; left-click isometric X  */
    /* +0xB0 */ int32_t   lClickGameY;      /* param_1[0x2C]; left-click isometric Y  */
    /* +0xB4 */ uint8_t   rightClickFlag;   /* non-zero = pending right-button event   */
    /* +0xB5 */ uint8_t   _padB5[3];
    /* +0xB8 */ uint32_t  clickPackedR;     /* param_1[0x2E]; packed right-click coords*/
    /* +0xBC */ int32_t   rClickGameX;      /* param_1[0x2F]; right-click isometric X */
    /* +0xC0 */ int32_t   rClickGameY;      /* param_1[0x30]; right-click isometric Y */
    /* +0xC4 */ uint32_t  mouseMovePacked0; /* pending WM_MOUSEMOVE coords lo16=X,hi16=Y*/
    /* +0xC8 */ uint32_t  _resC8[3];
    /* +0xD4 */ uint32_t  mouseMovePacked1; /* second pending WM_MOUSEMOVE coords      */
    /* +0xD8 */ uint32_t  _resD8[4];
    /* +0xE8 */ void     *draggedObj;       /* param_1[0x3A]; held train-locomotive;
                                             * NULL = not dragging                     */
    /* +0xEC */ uint32_t  dragLocked;       /* param_1[0x3B]; non-zero = dragged object
                                             * is locked/placed on current tile        */
    /* +0xF0 */ uint32_t  _resF0[6];
    /* +0x108 */ void    *busyCursor;       /* cached HCURSOR from LoadCursorFromFileA
                                             * ("CURSORS\busy_ani.ani")                */
    /* +0x10C */ void    *tileContainer;    /* world-tile child container; iterated in
                                             * ProcessPlacement                        */
    /* +0x110 */ uint32_t _res110[0x16];
    /* +0x168 */ uint8_t  brushRows;        /* stamp-brush height in tiles (build mode
                                             * sub-mode 2 area placement)              */
    /* +0x169 */ uint8_t  brushCols;        /* stamp-brush width in tiles              */
} InputCursor;

/*
 * DDSURFACEDESC2_Cursor  --  stack layout used in InitCursorResources when
 * creating the shared 256x256 staging surface.
 *
 * dwSize must be 0x7c (124) to identify this as DDSURFACEDESC2 to DirectDraw.
 * dwFlags = 0x07 = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH.
 * ddsCaps.dwCaps = 0x840 = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY.
 *
 * LINUX: replaced entirely by SDL_CreateRGBSurface(0, 256, 256, 32, ...).
 * This struct is kept for documentation only.
 */
typedef struct DDSURFACEDESC2_Cursor {
    DWORD dwSize;       /* +0x00  0x7c — must equal sizeof(DDSURFACEDESC2) */
    DWORD dwFlags;      /* +0x04  0x07 — DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH */
    DWORD dwHeight;     /* +0x08  0x100 (256) */
    DWORD dwWidth;      /* +0x0c  0x100 (256) */
    /* +0x10..+0x73  intervening fields (pitch, pixelformat, etc.) zeroed */
    uint8_t _pad[0x64];
    DWORD ddsCaps_dwCaps; /* +0x74  0x840 — DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY */
    /* +0x78..+0x7b  ddsCaps reserved fields, zeroed */
    DWORD _reserved;
} DDSURFACEDESC2_Cursor; /* total = 0x7c bytes */

/* =========================================================================
 * Global Variable Declarations  (original PE addresses noted)
 * ========================================================================= */

/*
 * g_ResourceManager  (DAT_004855e8)
 *   Global resource manager vtable object.
 *   Passed to FUN_00446ea0 in InitCursorResources to load cursor animation
 *   resources by ID (0x1400 for idle, 0x1403 for grab/busy).
 *   WIN32: opaque vtable object    LINUX: equivalent resource-cache struct
 */
extern void *g_ResourceManager;          /* 0x004855e8 */

/*
 * g_CursorSurface  (DAT_004fd3cc)
 *   Shared 256x256 offscreen surface used as staging buffer for all
 *   CursorManager instances.  Created once in InitCursorResources when first
 *   needed; reference-counted via g_CursorSurfaceRefCount.
 *   WIN32: IDirectDrawSurface4*    LINUX: SDL_Surface* (256x256, 32bpp)
 */
extern void *g_CursorSurface;            /* 0x004fd3cc */

/*
 * g_CursorSurfaceRefCount  (DAT_004fd3d0)
 *   Reference count for g_CursorSurface.  Incremented by each CursorManager
 *   instance in InitCursorResources.  Surface must be freed only when this
 *   reaches zero.
 */
extern int   g_CursorSurfaceRefCount;   /* 0x004fd3d0 */

/*
 * g_DirectDraw  (DAT_00485440)
 *   Global DirectDraw factory object.
 *   Vtable slot +0x18 = CreateSurface; called with DDSURFACEDESC2_Cursor to
 *   allocate the 256x256 staging surface in InitCursorResources.
 *   WIN32: IDirectDraw4*    LINUX: not used (SDL_CreateRGBSurface replaces)
 */
extern void *g_DirectDraw;               /* 0x00485440 */

/*
 * g_PrimaryDDrawSurface  (DAT_004fd3c0)
 *   Global primary or back-buffer DirectDraw surface.
 *   Vtable slot +0x14 = Blt; used in RenderCursor to blit the 256x256 staging
 *   surface to the screen.
 *   WIN32: IDirectDrawSurface4*    LINUX: SDL_Surface* (screen / backbuffer)
 */
extern void *g_PrimaryDDrawSurface;      /* 0x004fd3c0 */

/*
 * g_InputDispatcher  (DAT_004854c8)
 *   Unknown subsystem object passed to FUN_00411dc0 during cursor deactivation
 *   in SetCursorCapture.  Likely the input manager or window-message dispatcher.
 */
extern void *g_InputDispatcher;          /* 0x004854c8 */

/*
 * g_GameMode  (DAT_004851F4)
 *   Global game-mode state variable.  Set exclusively through GameMode_Set
 *   (0x00408130).  InputCursor_UpdateCursorState dispatches on this value
 *   every frame to select the cursor-sprite decision branch.
 *   Values: GAME_MODE_* constants above.
 */
extern int   g_GameMode;                /* 0x004851F4 */

/*
 * g_BulldozeFlag  (DAT_004AA648)
 *   Non-zero when the bulldoze/delete tool is the active toolbar selection.
 *   Forces CURSOR_RES_PLACE (0x1404 grab cursor) in both BuildModeCursor
 *   and PlaceModeCursor.
 */
extern int   g_BulldozeFlag;            /* 0x004AA648 */

/*
 * g_FreePlaceFlag  (DAT_004A9F80)
 *   Non-zero when an object is being freely dragged (not grid-snapped).
 *   Forces CURSOR_RES_PLACE in PlaceModeCursor.
 */
extern int   g_FreePlaceFlag;           /* 0x004A9F80 */

/*
 * g_RegionOverlay  (DAT_004A9F78)
 *   Non-zero when a map-region overlay is active (highlights selectable zone).
 *   Used by PlaceModeCursor to set CURSOR_RES_PLACE over the highlighted zone.
 */
extern int   g_RegionOverlay;           /* 0x004A9F78 */

/*
 * g_ViewPanX / g_ViewPanY  (DAT_004AAD24 / DAT_004AAD28)
 *   Viewport isometric-scroll pan offsets in pixels.
 *   Added to screen coordinates in Mouse_ScreenToIso to compute world tile pos.
 *   WIN32/LINUX: pure integer offsets; no platform difference.
 */
extern int   g_ViewPanX;               /* 0x004AAD24 */
extern int   g_ViewPanY;               /* 0x004AAD28 */

/*
 * g_GridTileW / g_GridTileH  (DAT_004AAD46 / DAT_004AAD48)
 *   Dimensions of the scrollable world map in tiles (width x height).
 *   Used in Mouse_ScreenToIso to clamp the converted tile coordinates.
 */
extern int   g_GridTileW;              /* 0x004AAD46 */
extern int   g_GridTileH;              /* 0x004AAD48 */

/*
 * g_SubMode  (DAT_00485234)
 *   Sub-mode within the current game mode (1 or 2).
 *   In GAME_MODE_BUILD (4):
 *     1 = single-tile click placement
 *     2 = stamp-brush area placement (brush_rows x brush_cols)
 *   In GAME_MODE_BUILD (4) / Mouse_ProcessMove:
 *     2 = triggers train hit-test via FUN_00449D00
 */
extern int   g_SubMode;                /* 0x00485234 */

/*
 * g_TickBase  (DAT_004A99B4)
 *   Base game-tick counter used with random jitter to compute the initial
 *   animation delay for cursor sprites that have multiple frames.
 *   See Cursor_SetType (0x00405AB0).
 */
extern int   g_TickBase;               /* 0x004A99B4 */

/* =========================================================================
 * Input Dispatch Functions  (Batch 1 — cursor state machine)
 * =========================================================================
 * These functions implement the per-frame input processing and cursor-sprite
 * selection state machine.  They share the same InputCursor 'this' pointer
 * (raw PE addresses shown for Ghidra cross-reference).
 * ========================================================================= */

/*
 * InputCursor_Tick  (0x00410840)
 *
 * Main per-frame input processor.  Dispatches pending mouse events in order:
 *   1. Mouse position update via FUN_00405C40
 *   2. Warp event (warpPending flag) -> InputCursor_ProcessWarpedPosition
 *   3. Left-click (leftClickFlag)    -> InputCursor_LeftButtonDown
 *   4. Right-click (rightClickFlag)  -> InputCursor_RightButtonDown
 *   5. Drag-drop release             -> InputCursor_DragDrop
 *   6. WM_MOUSEMOVE packed coords at +0xC4 and +0xD4
 * After all events are consumed, calls InputCursor_UpdateCursorState to
 * select the correct sprite.  Forces CURSOR_RES_IDLE if gameCursorEnabled != 0x01.
 *
 * WIN32: WM_* messages deliver packed LPARAM coords (lo16=X, hi16=Y)
 * LINUX: SDL_MOUSEMOTION / SDL_MOUSEBUTTONDOWN events replace WM_*
 */
void InputCursor_Tick(InputCursor *ic);

/*
 * InputCursor_ProcessWarpedPosition  (0x00410A40)
 *
 * Handles a pending mouse-warp event (warpPending byte at this+0x8E).
 * Converts packed warpPackedPos (lo16=X, hi16=Y) from client to screen via
 * ClientToScreen, verifies the cursor is over the game window via
 * WindowFromPoint, transforms to isometric tile coords via Mouse_ScreenToIso,
 * and stores the result in cursorGameX/Y.
 * Rail-scroll axis clamping:
 *   CURSOR_RES_RAIL_CLAMP_Y1 / _Y2 -> clamp Y coordinate
 *   CURSOR_RES_RAIL_CLAMP_X1 / _X2 -> clamp X coordinate
 * If draggedObj is non-NULL, updates its on-screen position.
 * Calls InputCursor_SetSystemCursorMode when game window loses focus.
 *
 * WIN32: ClientToScreen, WindowFromPoint
 * LINUX: SDL_GetWindowPosition + SDL_GetMouseFocus
 */
void InputCursor_ProcessWarpedPosition(InputCursor *ic);

/*
 * InputCursor_ProcessPlacement  (0x00410D20)
 *
 * Left-button-release / placement event handler.
 * Iterates tileContainer children looking for a tile under cursorGameX/Y.
 * Build mode (GAME_MODE_BUILD=4) dispatch:
 *   subMode 1: single-tile lookup via FUN_00455670, fires event 0x400
 *   subMode 2: 2-D stamp placement across brushRows x brushCols grid,
 *              calls FUN_00455620 per tile for each non-zero brush cell
 * Handles snap-back when drag target is invalid (sets leftClickFlag,
 * copies last valid position into clickPackedL).
 */
void InputCursor_ProcessPlacement(InputCursor *ic);

/*
 * InputCursor_LeftButtonDown  (0x00411000)
 *
 * Left-mouse-button-down handler.
 * Unpacks clickPackedL via Mouse_ScreenToIso into lClickGameX/Y.
 * When activeCursorId is CURSOR_RES_ROTATE (0x1402) and the active item
 * has a new default frame-set, fires a frame-advance via vtable+0x1C.
 * Hit-test priority order (first match wins):
 *   1. Mini-train-world overlay  (FUN_00459D60)
 *   2. Main world map            (FUN_00449D00)
 *   3. Toolbar items             (FUN_00434C50)
 *   4. Train tile grid           (FUN_004556F0)
 * On grid hit with draggedObj: increments load counter at draggedObj+0x88
 * (capped at DRAG_ITEMS_MAX=7), calls vtable+0x44 to attach to tile,
 * fires event 0x386D.
 *
 * WIN32: none specific
 * LINUX: SDL_MOUSEBUTTONDOWN with button == SDL_BUTTON_LEFT
 */
void InputCursor_LeftButtonDown(InputCursor *ic);

/*
 * InputCursor_RightButtonDown  (0x00411230)
 *
 * Right-mouse-button-down handler.
 * Unpacks clickPackedR into rClickGameX/Y.
 * If activeCursorId matches the item's ID and the item has a non-zero
 * frameSetRedirect (at sprite+0x52E), mutates activeCursorId.
 * Otherwise hit-tests: toolbar, minimap (FUN_0044E830), tile grid
 * (FUN_00455D60), sending events 0x502C or 0x5015.
 * Clears draggedObj via FUN_004113A0 on button-up.
 *
 * WIN32: none specific
 * LINUX: SDL_MOUSEBUTTONDOWN with button == SDL_BUTTON_RIGHT
 */
void InputCursor_RightButtonDown(InputCursor *ic);

/*
 * InputCursor_DragDrop  (0x00411580)
 *
 * Drag-release / drop handler for the held train-locomotive.
 * draggedObj pointer is at this+0xE8.
 * If cursor type byte (sprite+0x18) is 0x01 and placement mode is 0x07:
 *   Searches g_TrainSlotArray0 for a compatible slot at the drop position.
 *   Fallback: iterates all slots for best available (vtable+0x1C, vtable+0x48).
 * Handles placement mode 0x08 in the second slot array similarly.
 * On success calls vtable+0x0 to commit placement.
 * Adjusts load counter at draggedObj+0x88 (clamped 0..DRAG_ITEMS_MAX).
 * Clears dragLocked flag at this+0xEC.
 */
void InputCursor_DragDrop(InputCursor *ic);

/*
 * InputCursor_UpdateCursorState  (0x00411760)
 *
 * Top-level cursor sprite selector, called every frame after event dispatch.
 * Dispatches by g_GameMode:
 *   GAME_MODE_LOADING  (1) -> skip (no cursor change during loading)
 *   GAME_MODE_PLACE    (3) -> InputCursor_PlaceModeCursor
 *   GAME_MODE_BUILD    (4) -> InputCursor_BuildModeCursor + ProcessPlacement
 *   all other modes        -> InputCursor_SetCursorSprite(CURSOR_RES_IDLE)
 * After dispatch: if gameCursorEnabled != 0x01, forces CURSOR_RES_IDLE.
 */
void InputCursor_UpdateCursorState(InputCursor *ic);

/*
 * InputCursor_BuildModeCursor  (0x004117B0)
 *
 * Cursor-sprite selection for build/scroll mode (g_GameMode == GAME_MODE_BUILD).
 * Decision tree (first match wins):
 *   1. g_BulldozeFlag set          -> CURSOR_RES_PLACE (0x1404), scrollDir=2
 *   2. Outside world map           -> fall through to edge-scroll check
 *   3. Hoverable zone checks       -> FUN_00449CE0 / FUN_00436A10
 *   4. Screen-edge scroll zones (cursor within threshold of viewport edge):
 *        left  (< SCROLL_EDGE_LRT_PX)   -> CURSOR_RES_SCROLL_LEFT  (0x0C2C)
 *        right (> width-SCROLL_EDGE_LRT) -> CURSOR_RES_SCROLL_RIGHT (0x0C26)
 *        top   (< SCROLL_EDGE_LRT_PX)   -> CURSOR_RES_SCROLL_UP    (0x0C28)
 *        bot   (> height-SCROLL_EDGE_BOT)-> CURSOR_RES_SCROLL_DOWN  (0x0C2A)
 *        diagonal variants               -> 0x0C42..0x0C48
 *   5. Fall back to activeCursorId, or CURSOR_RES_IDLE on no match
 */
void InputCursor_BuildModeCursor(InputCursor *ic);

/*
 * InputCursor_PlaceModeCursor  (0x00411AE0)
 *
 * Cursor-sprite selection for place/train mode (g_GameMode == GAME_MODE_PLACE).
 * Forces CURSOR_RES_PLACE (0x1404) when any of:
 *   - draggedObj != NULL AND dragLocked != 0  (object locked on a tile)
 *   - g_BulldozeFlag set
 *   - g_FreePlaceFlag set
 * Inside world map region:
 *   - g_RegionOverlay active AND cursor in highlight zone -> CURSOR_RES_PLACE
 *   - In boundary area (FUN_00459D60)                    -> CURSOR_RES_IDLE
 *   - g_PanelArea hit                                    -> CURSOR_RES_IDLE
 *   - draggedObj != NULL AND dragLocked == 0             -> CURSOR_RES_OPEN_HAND
 */
void InputCursor_PlaceModeCursor(InputCursor *ic);

/*
 * InputCursor_SetSystemCursorMode  (0x00411DC0)
 *
 * Controls the Windows system cursor and mouse capture for non-DDraw states
 * (loading screens, dialogs, busy animations).
 *
 * @param ic              InputCursor instance
 * @param active          desired active state (0 = deactivate, non-zero = activate)
 * @param showSysCursor   show OS cursor on deactivation (ShowCursor loop to 1)
 * @param useBusyCursor   load "CURSORS\busy_ani.ani" via LoadCursorFromFileA
 *                        instead of hiding the cursor
 *
 * Deactivation (active == 0):
 *   ReleaseCapture(); if showSysCursor: ShowCursor loop to 1
 * Activation without busy cursor:
 *   ShowCursor loop to -1 (hide)
 * Activation with busy cursor:
 *   LoadCursorFromFileA -> SetCursor; SetCapture; GetCursorPos + ScreenToClient;
 *   set warpPending to trigger deferred position update
 *
 * WIN32: ShowCursor, SetCapture, ReleaseCapture, LoadCursorFromFileA, SetCursor,
 *        GetCursorPos, ScreenToClient
 * LINUX: SDL_ShowCursor, SDL_CaptureMouse, SDL_GetMouseState; custom ANI loader
 */
void InputCursor_SetSystemCursorMode(InputCursor *ic,
                                     int active,
                                     int showSysCursor,
                                     int useBusyCursor);

/*
 * InputCursor_SetCursorSprite  (0x00411FB0)
 *
 * Core cursor sprite switch.  Early-outs if resource_id already matches the
 * currently displayed ID (from *(ic->cursorRenderer + 4)).
 * Looks up the resource in g_ResourceManager via FUN_00446EA0 (RESMGR_GetResource).
 * On a hit, calls vtable[ic][0x18](resource_id, hotspot_x, 0) to push the new
 * sprite ID to the cursor-renderer sub-object.
 * Hotspot adjustment (when gameCursorEnabled is set):
 *   cursorType == 5 -> hotspot from (field at ic+0x9C/0xA0) minus sprite->hotspotX/Y
 *   cursorType != 5 -> Y only, subtract sprite->heightOffset (+0x16D)
 * Stores new resource ID in ic->activeCursorId.
 */
void InputCursor_SetCursorSprite(InputCursor *ic, uint32_t resource_id);

/*
 * InputCursor_GameModeSet  (0x00408130)
 *
 * Sets g_GameMode and performs mode-transition side-effects.
 * Modes 5/6/7/9: call InputCursor_SetSystemCursorMode(0,0,0) before showing UI.
 * Mode 2: hide DDraw cursor, show OS cursor, focus inventory window.
 * Mode 3: intermediate step sets mode=2 before switching to 3.
 * Mode 4: clear draggedObj, reset toolbar, clear world-event queue.
 * Mode 10: drain message queue, post WM_QUIT.
 *
 * WIN32: PostQuitMessage(0) for mode 10
 * LINUX: SDL_PushEvent({SDL_QUIT}) for mode 10
 */
void InputCursor_GameModeSet(int new_mode);

/*
 * Mouse_ProcessMove  (0x00410A40)
 *
 * Main WM_MOUSEMOVE handler.
 * Reads packed coords from ic->warpPackedPos ((y<<16)|x), converts via
 * ClientToScreen -> WindowFromPoint -> Mouse_ScreenToIso, stores in
 * cursorGameX/Y.
 * If cursor sprite type == 5 (isometric drag): delegates tile hit to
 * vtable+0xC for hotspot-adjusted tile position.
 * In GAME_MODE_BUILD with g_SubMode == 2: triggers train hit-test
 * via FUN_00449D00.
 *
 * WIN32: ClientToScreen, WindowFromPoint
 * LINUX: SDL_GetWindowPosition + SDL_GetMouseFocus; SDL_MOUSEMOTION event
 */
void Mouse_ProcessMove(InputCursor *ic);

/*
 * Mouse_ScreenToIso  (0x00412060)
 *
 * Converts screen pixel coordinates to isometric tile coordinates.
 *   tile_x = clamp((screen_x + g_ViewPanX) / TILE_SIZE_PX, 0, g_GridTileW-1)
 *   tile_y = clamp((screen_y + g_ViewPanY) / TILE_SIZE_PX, 0, g_GridTileH-1)
 * When sprite->cursorType != 5: further clips to sprite's own grid boundary
 * (sprite->frameWidth column cap, sprite->heightOffset / +0x169 row range).
 * Output written to out[0] (x) and out[1] (y).
 *
 * WIN32/LINUX: pure arithmetic; no platform-specific calls.
 */
void Mouse_ScreenToIso(const InputCursor *ic,
                       int screen_x, int screen_y,
                       int out[2]);

/* =========================================================================
 * Forward Declarations for Internal Helper Functions
 *
 * These are Ghidra-named internal functions called by the documented
 * functions below.  Their purpose is noted from call-site context.
 * ========================================================================= */

/* Cursor surface flush / clear helper called on deactivation and focus loss */
extern void FUN_0045b940(void);

/* Cursor surface blit helper; param_1=1 clears, param_1=0 normal render */
extern void FUN_00414fb0(CursorManager *mgr, int clearFlag);

/* Cursor position restore / cleanup called during deactivation */
extern void FUN_00414ef0(CursorManager *mgr);

/* Input dispatcher notification called on cursor deactivation */
extern void FUN_00411dc0(void *dispatcher);

/* Overlay cursor render/clear helper; called when hasOverlayCursor is set */
extern void FUN_00415440(CursorManager *mgr, int clearFlag);

/* Resource manager lookup: returns loaded resource for resID (lazy-loads) */
extern void *FUN_00446ea0(void *resMgr, int resID);

/* Surface lock/prepare helper called on each cursor resource after load */
extern void FUN_0042a3d0(void *surface);

/* Surface initializer called after creating the shared staging surface */
extern void FUN_004014e0(void *surface);

/* Additional staging-surface setup helpers */
extern void FUN_0045b9b0(void *surface);
extern void FUN_0045ba50(void *surface);

/* Error handler / assert called on irrecoverable failures */
extern void FUN_00463600(void);

/* =========================================================================
 * Public Function Declarations
 * ========================================================================= */

/*
 * UpdateClientRect  (0x004140a0)
 *
 * Recomputes and caches the window client-area rect and derived clip fields
 * inside the CursorManager.  No-op when active flag (this+0xdb) is zero.
 *
 * WIN32: GetClientRect(this->hwnd, &rect)
 * LINUX: SDL_GetWindowSize(window, &w, &h)
 *
 * @param mgr  CursorManager instance
 */
void UpdateClientRect(CursorManager *mgr);

/*
 * InitCursorResources  (0x00414130)
 *
 * One-time initialization of the cursor subsystem.  Loads idle (0x1400) and
 * grab (0x1403) cursor animation resources, caches frame dimensions, and
 * creates the shared 256x256 staging surface if not already allocated.
 *
 * WIN32: IDirectDraw4::CreateSurface via vtable (DDSURFACEDESC2, dwSize=0x7c)
 * LINUX: SDL_CreateRGBSurface(0, 256, 256, 32, Rmask, Gmask, Bmask, Amask)
 *
 * @param mgr  CursorManager instance to initialize
 */
void InitCursorResources(CursorManager *mgr);

/*
 * SetCursorCapture  (0x00414290)
 *
 * Activates or deactivates the game's custom cursor mode.
 *
 * param_1 == 0  -> activate: SetCapture, hide OS cursor
 * param_1 != 0  -> deactivate: ReleaseCapture, show OS cursor, clean up
 *
 * WIN32: SetCapture, ReleaseCapture, ShowCursor (looped until count < 0)
 * LINUX: SDL_CaptureMouse, SDL_SetWindowGrab, SDL_ShowCursor (single call)
 *
 * @param mgr      CursorManager instance
 * @param deactivate  0 = activate capture, non-zero = deactivate
 */
void SetCursorCapture(CursorManager *mgr, int deactivate);

/*
 * SetCursorState  (0x00414340)
 *
 * Changes the active cursor animation state.
 *
 * @param mgr        CursorManager instance
 * @param stateId    new cursor state ID (0 = no cursor / hidden)
 * @param pAnimData  CursorAnimData for the new state
 * @param resetPos   if non-zero, zeroes the eight dirty-rect cache fields
 * @param forceRedraw  if non-zero and cursor is active, triggers immediate redraw
 */
void SetCursorState(CursorManager *mgr,
                    int             stateId,
                    CursorAnimData *pAnimData,
                    int             resetPos,
                    int             forceRedraw);

/*
 * OnWindowActivate  (0x00414a80)
 *
 * Window focus-change handler.  When the game window loses activation,
 * flushes the cursor surface and releases capture.
 *
 * WIN32: driven by WM_ACTIVATE / WM_KILLFOCUS message
 * LINUX: SDL_WINDOWEVENT_FOCUS_LOST event in the SDL event loop
 *
 * @param mgr   CursorManager instance
 * @param hwnd  window handle that lost focus (compared to mgr->hwnd)
 * @return      always 0
 */
int OnWindowActivate(CursorManager *mgr, void *hwnd);

/*
 * OnWindowDestroy  (0x00414b80)
 *
 * WM_DESTROY equivalent.  Clears the active flag, destroys the window,
 * and posts WM_QUIT if this is the top-level (root) window.
 *
 * WIN32: DestroyWindow, PostQuitMessage(0)
 * LINUX: SDL_DestroyWindow, SDL_PushEvent({SDL_QUIT})
 *
 * @param mgr  CursorManager instance (also used as param_1 in original)
 * @return     always 0
 */
int OnWindowDestroy(CursorManager *mgr);

/*
 * WaitForSurface  (0x00414bb0)
 *
 * Blocking poll that waits for the game's primary rendering surface to become
 * ready.  Up to SURFACE_WAIT_MAX_RETRIES attempts at SURFACE_WAIT_INTERVAL_MS
 * ms apart; calls ExitProcess(1) on timeout.
 *
 * WIN32: Sleep(10), ExitProcess(1), vtable query on DirectDraw surface
 * LINUX: SDL_Delay(10), exit(1); surface creation is synchronous so this
 *        function body reduces to a NULL-check after SDL_CreateRGBSurface.
 *
 * @param mgr  CursorManager whose pGameSurface vtable is polled
 * @return     surface status value on success (non-zero)
 */
int WaitForSurface(CursorManager *mgr);

/*
 * RenderCursor  (0x00414c20)
 *
 * Main per-frame cursor compositing and blit.
 *
 * Normal path (param_clearOnly == 0, cursor active):
 *   1. GetCursorPos -> screen mouse position
 *   2. Subtract hotspot -> sprite top-left
 *   3. Clip against viewport bounds
 *   4. Select animation frame column from horizontal strip
 *   5. Blit frame from animation surface -> 256x256 staging surface (DDBLT_KEYSRC)
 *   6. ClientToScreen for window-origin screen coords
 *   7. Blit staging surface -> primary surface (DDBLT_WAIT)
 *
 * Alternate path (param_clearOnly != 0 or no active cursor):
 *   Clears the cursor surface rectangle only.
 *
 * WIN32: GetCursorPos, ClientToScreen, SetRect, CopyRect, OffsetRect,
 *        IDirectDrawSurface4::Blt with flags 0x1008000 / 0x1000000,
 *        OutputDebugStringA on blit failure
 * LINUX: SDL_GetGlobalMouseState, SDL_GetWindowPosition (manual add),
 *        SDL_SetColorKey + SDL_BlitSurface (two-stage), SDL_Log on failure
 *
 * @param mgr           CursorManager instance
 * @param beginFrame    if non-zero, calls a 'begin frame' vtable method first
 * @param clearOnly     if non-zero, performs clear-only path
 */
void RenderCursor(CursorManager *mgr, int beginFrame, int clearOnly);

#endif /* INPUT_H */
