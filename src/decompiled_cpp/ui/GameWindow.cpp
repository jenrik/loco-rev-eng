/**
 * GameWindow.cpp — GameWindow implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * GameWindow is the base class for DirectDraw-backed game overlay windows.
 * It creates its own offscreen DDraw surface for rendering and blits it
 * to the primary backbuffer during Show/Hide transitions. Subclasses
 * include AboutDialog, TrainStationWindow, and AudioMgr (HelpWnd).
 */

// Status: TRANSCRIBED

#include "GameWindow.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include "../stubs/ddraw.h"
#include "../shared/vtable_addrs.h"
/* vtable_addrs.h: VTBL_* reference constants for documentation and cross-validation */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Shared GameWindow window procedure (0x415900) */
extern LRESULT CALLBACK GameWindow_WndProc(HWND, UINT, WPARAM, LPARAM);

#ifdef _WIN32
extern "C" {
    /* Win32 API imports (via IAT thunks) */
    extern HWND  __stdcall CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName,
                                           LPCSTR lpWindowName, DWORD dwStyle,
                                           int X, int Y, int nWidth, int nHeight,
                                           HWND hWndParent, HMENU hMenu,
                                           HINSTANCE hInstance, void* lpParam);
    extern ATOM  __stdcall RegisterClassA(const void* lpWndClass);
    extern BOOL  __stdcall ShowWindow(HWND hWnd, int nCmdShow);
    extern BOOL  __stdcall UpdateWindow(HWND hWnd);
    extern HWND  __stdcall SetTimer(HWND hWnd, UINT_PTR nIDEvent,
                                    UINT uElapse, void* lpTimerFunc);
    extern BOOL  __stdcall KillTimer(HWND hWnd, UINT_PTR uIDEvent);
    extern HWND  __stdcall GetCapture(void);
    extern BOOL  __stdcall ReleaseCapture(void);
    extern int   __stdcall GetWindowTextA(HWND hWnd, char* lpString, int nMaxCount);
    extern BOOL  __stdcall SetWindowPos(HWND hWnd, HWND hWndInsertAfter,
                                        int X, int Y, int cx, int cy, UINT uFlags);
    extern DWORD __stdcall GetLastError(void);
    extern DWORD __stdcall FormatMessageA(DWORD dwFlags, const void* lpSource,
                                          DWORD dwMessageId, DWORD dwLanguageId,
                                          char* lpBuffer, DWORD nSize,
                                          void* Arguments);
    extern void* __stdcall LocalFree(void* hMem);

    /* Win32 GDI wrappers */
    extern BOOL  __stdcall CopyRect(RECT* lprcDst, const RECT* lprcSrc);
    extern BOOL  __stdcall OffsetRect(RECT* lprc, int dx, int dy);
}
#endif /* _WIN32 */

#ifndef _WIN32
/* Non-Windows declarations supplied by sdl3_window.cpp. The SDL window
 * header is intentionally not included because its Win32 aliases conflict
 * with the translated compatibility headers. */
extern BOOL CopyRect(RECT* dst, const RECT* src);
extern DWORD GetLastError();
extern DWORD FormatMessageA(DWORD flags, const void* source, DWORD message,
                            DWORD language, char* buffer, DWORD size, void* arguments);
extern void* LocalFree(void* memory);
/* Inline stubs for functions not yet covered by sdl3_window.h */
static inline HWND GetCapture(void) { return NULL; }
static inline BOOL ReleaseCapture(void) { return TRUE; }
static inline int GetWindowTextA(HWND, char* buf, int max) {
    if (buf && max > 0) buf[0] = 0;
    return 0;
}
#endif /* !_WIN32 */

/* Game window subsystem functions */
extern void Cursor_InitSprites(GameWindow* this_);                        /* 0x414130 */
extern void Cursor_UnlockAllSurfaces(GameWindow* this_);                  /* 0x414EF0 */
extern void Cursor_UpdateDirtyRect(GameWindow* this_, uint8_t flag);      /* 0x414770 (set_mode helper) */
extern void Cursor_RenderWithViewport(GameWindow* this_, uint8_t param);  /* 0x414810 (set_mode helper) */
extern void Cursor_SetCapture(GameWindow* this_, byte capture);           /* 0x414290 */
extern void __cdecl    DDRAW_UnlockPrimary(HWND hWnd);                     /* 0x45B940 */
extern void FormatResourceString(void* resmgr, int string_id,
                                      char* out_buf, int max_len);         /* 0x447330 */

/* DDraw surface helpers (defined in native/ddraw_helpers.c) */
extern void __cdecl DDRAW_GetSurfaceWidthHeight(void* surf,
                                                uint16_t* out_height,
                                                uint16_t* out_width);      /* 0x4014E0 */
extern int  __cdecl DDRAW_SetSurfaceFormat(void* surf, void* desc);        /* 0x45B9B0 */
extern int  __cdecl DDRAW_RestoreSurfaces(void* surf, void* desc);         /* 0x45BA50 */

/* ================================================================== */
/* Global variable references                                           */
/* ================================================================== */

/* ResourceManager is an embedded object at 0x4855E8, not a pointer.
 * FormatResourceString receives &g_resmgr (ResourceManager*) as this. */
class ResourceManager;
extern ResourceManager g_resmgr;                                    /* 0x4855E8 */

extern void*  g_ddraw;              /* 0x485440 — IDirectDraw4* */
extern void*  g_backbuffer;         /* 0x4FD3C0 — main scene backbuffer (IDirectDrawSurface*) */
extern void*  g_primary_surface;    /* 0x4FD3C4 — primary surface (IDirectDrawSurface*) */
extern uint8_t g_surface_lost;      /* 0x4FD218 — 0=ready, 1=lost */
extern uint8_t g_is_fullscreen;     /* 0x485210 — 0=windowed, 1=fullscreen */
extern int32_t g_client_width;      /* 0x485220 — client area width (stored negative) */
extern int32_t g_client_height;     /* 0x485224 — client area height (stored negative) */
extern int32_t g_window_left;       /* 0x485200 — main window left position */
extern int32_t g_window_top;        /* 0x485204 — main window top position */

/* Shared cursor backbuffer + refcount (managed across all window instances) */
extern void*  g_cursor_back;        /* 0x4FD3CC */
extern int32_t g_cursor_refcount;   /* 0x4FD3D0 */

/* ================================================================== */
/* Surface creation constants                                          */
/* ================================================================== */
#define DDSD_SIZE           0x7C     /* sizeof(DDSURFACEDESC2) */
#define DDSD_CAPS           0x0001
#define DDSD_HEIGHT         0x0002
#define DDSD_WIDTH          0x0004
#define DDSD_CAPS_HEIGHT_WIDTH (DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH)  /* 0x7 */

/* WNDCLASS style / window style constants */
#define CS_HREDRAW          0x0001
#define CS_VREDRAW          0x0002
#define GW_STYLE            0x86000000  /* WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN */
#define DDBLT_WAIT          0x1000000
#define SW_HIDE             0
#define SW_SHOW             5

/* ================================================================== */
/* GameWindow Constructor                                               */
/* Address: 0x413AB0                                                    */
/*                                                                     */
/* Called by: AboutDialog_Ctor @ 0x40F1E9,                             */
/*            TrainStationWindow_Ctor @ 0x436B2F,                      */
/*            AudioMgr_Ctor @ 0x44F4B9                                 */
/* ================================================================== */
GameWindow::GameWindow(HINSTANCE hInstance, UINT resId)
{
    /* +0x04 */ this->hInstance         = hInstance;
    /* +0x3C */ this->defaultWidth      = 0x20;
    /* +0x40 */ this->defaultHeight     = 0x20;

    /* In the binary: sets vtable = VTBL_GAMEWINDOW (0x477898).
     * Natural C++ manages vtable via the constructor chain. */
    /* +0x00 */

    /* Zero all working fields */
    /* +0x08 */ this->hWnd              = NULL;
    /* +0x0C */ this->hWndParent        = NULL;
    /* +0x38 */ this->backbufferSurface = NULL;
    /* +0x5C */ this->cursorRefcount    = 0;
    /* +0x9C */ this->ddrawAuxCount2    = 0;
    /* +0x90 */ this->ddrawAuxCount1    = 0;
    /* +0xA0 */ this->ddrawAuxField2    = 0;
    /* +0x94 */ this->ddrawAuxField1    = 0;
    /* +0x58 */ this->visible           = 0;
    /* +0x14 */ this->field_14          = 0;
    /* +0x44 */ this->field_44          = 0;
    /* +0x48 */ this->field_48          = 0;
    /* +0xDB */ this->createdFlag       = 0;

    /* NOTE: Offsets +0x50, +0x54, +0x60, +0x64, +0x98 (ddrawAuxPtr1),
       +0xA4 (ddrawAuxPtr2) are NOT zeroed — left uninitialized. */

    /* +0x10 */ this->resourceId        = resId;

    /* Zero reserved work fields (8 DWORDs at +0x68..+0x84).
     * Binary zeroes in non-sequential order; array order matches field layout. */
    this->reserved_work[0] = 0;                             /* +0x68 */
    this->reserved_work[1] = 0;                             /* +0x6C */
    this->reserved_work[2] = 0;                             /* +0x70 */
    this->reserved_work[3] = 0;                             /* +0x74 */
    this->reserved_work[4] = 0;                             /* +0x78 */
    this->reserved_work[5] = 0;                             /* +0x7C */
    this->reserved_work[6] = 0;                             /* +0x80 */
    this->reserved_work[7] = 0;                             /* +0x84 */

    /* Load window title from string resources into +0xA8 (50 bytes max) */
    FormatResourceString(&g_resmgr, resId, this->title, sizeof(this->title));

    /* +0x88 */ this->captureFlag = 0;
    /* +0x114 */ this->visible2   = 0;
}

/* ================================================================== */
/* GameWindow::scalar deleting destructor (vtable[0])                   */
/* Address: 0x413B50                                                    */
/* ================================================================== */
GameWindow::~GameWindow()
{
    this->base_destructor();
}

/* ================================================================== */
/* GameWindow::base_destructor                                          */
/* Address: 0x413B70                                                    */
/*                                                                     */
/* Release sequence:                                                    */
/*   1. Reset vtable to VTBL_GAMEWINDOW                                 */
/*   2. Release aux DDraw object 1 (+0x90 flag, +0x98 pointer)          */
/*   3. Release aux DDraw object 2 (+0x9C flag, +0xA4 pointer)          */
/*   4. Decrement cursor backbuffer refcount (+0x5C), free when 0       */
/*   5. Release own backbuffer surface (+0x38)                          */
/* ================================================================== */
void GameWindow::base_destructor()
{
    /* Reset vtable to base for correct dispatch during dtor chaining */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Release auxiliary DDraw object 1 */
    if (this->ddrawAuxCount1 != 0) {                            /* +0x90 */
        ((IDirectDrawSurface4*)this->ddrawAuxPtr1)->Release();  /* +0x98, vtable[2] */
        this->ddrawAuxField1 = 0;                               /* +0x94 */
        this->ddrawAuxPtr1   = NULL;                            /* +0x98 */
        this->ddrawAuxCount1 = 0;                               /* +0x90 */
    }

    /* Release auxiliary DDraw object 2 */
    if (this->ddrawAuxCount2 != 0) {                            /* +0x9C */
        ((IDirectDrawSurface4*)this->ddrawAuxPtr2)->Release();  /* +0xA4, vtable[2] */
        this->ddrawAuxField2 = 0;                               /* +0xA0 */
        this->ddrawAuxPtr2   = NULL;                            /* +0xA4 */
        this->ddrawAuxCount2 = 0;                               /* +0x9C */
    }

    /* Decrement global cursor backbuffer refcount */
    if (this->cursorRefcount != 0) {                            /* +0x5C */
        g_cursor_refcount--;
        if (g_cursor_refcount == 0 && g_cursor_back != NULL) {
            ((IDirectDrawSurface4*)g_cursor_back)->Release();   /* vtable[2] = Release */
            g_cursor_back     = NULL;
            g_cursor_refcount = 0;
        }
        this->cursorRefcount = 0;                               /* +0x5C */
    }

    /* Release own backbuffer surface */
    if (this->backbufferSurface != NULL) {                      /* +0x38 */
        ((IDirectDrawSurface4*)this->backbufferSurface)->Release(); /* vtable[2] */
        this->backbufferSurface = NULL;                         /* +0x38 */
    }
}

/* ================================================================== */
/* GameWindow::hide (vtable[1])                                         */
/* Address: 0x413C10                                                    */
/*                                                                     */
/* Saves the screen area behind this window into the main backbuffer    */
/* (so it can be restored on next Show), kills the timer, releases      */
/* mouse capture, hides the HWND, and clears the visible2 flag.        */
/*                                                                     */
/* Called by: HelpWnd_Hide @ 0x450AE4,                                  */
/*            TrainStationWindow_Hide @ 0x436F78,                       */
/*            CGWND_Screensaver_Hide @ 0x40F48D                        */
/* ================================================================== */
void GameWindow::hide()
{
    /* Set capture mode */
    Cursor_SetCapture(this, 1);                     /* 1 = begin capture */

    /* Kill the window timer */
    KillTimer(this->hWnd, this->timerId);           /* +0x08, +0x4C */

    /* Release mouse capture if this window owns it */
    if (this->captureFlag != 0) {                   /* +0x88 */
        HWND hCurWnd = GetCapture();
        if (hCurWnd == this->hWnd) {                /* +0x08 */
            ReleaseCapture();
        }
    }

    /* Unlock primary surface */
    DDRAW_UnlockPrimary(this->hWnd);                /* +0x08 */

    /* Compute source and dest rectangles for the Blt that saves
       the screen content behind the window to the backbuffer.
       Both source (primary surface) and dest (backbuffer) use
       the same window rect so the saved area can be restored. */
    RECT srcRect;                                    /* local_20 */
    RECT destRect;                                   /* local_10 */

    CopyRect(&destRect, (RECT*)&this->rectLeft);    /* copy window rect +0x18..+0x24 */
    CopyRect(&srcRect,  (RECT*)&this->rectLeft);    /* copy window rect +0x18..+0x24 */

    /* Adjust rectangles for fullscreen mode */
    if (g_is_fullscreen != 0) {                      /* 0x485210 */
        /* In fullscreen mode, the window rect is relative to the viewport.
           Offset the source rect by (-g_client_width, -g_client_height) to
           reach the primary surface position, and the dest rect by
           (g_window_left, g_window_top) to reach the backbuffer position. */
        /* +0x485220/4: g_client_width  (stored negative, NEG'd before use) */
        /* +0x485224/4: g_client_height (stored negative, NEG'd before use) */
        OffsetRect(&srcRect,  -g_client_width, -g_client_height);
        /* +0x485200: g_window_left, +0x485204: g_window_top */
        OffsetRect(&destRect, g_window_left,   g_window_top);
    }

    /* Restore primary surface if lost */
    if (g_surface_lost != 0) {                       /* 0x4FD218 */
        int result = ((IDirectDrawSurface4*)g_primary_surface)->Restore();  /* 0x4FD3C4, vtable[32] */
        if (result == 0) {
            g_surface_lost = 0;
        }
    }

    /* Blt: save screen content below window from primary to backbuffer */
    ((IDirectDrawSurface4*)g_backbuffer)->Blt(                     /* 0x4FD3C0, vtable[5] */
        &destRect, g_primary_surface, &srcRect,
        DDBLT_WAIT, NULL);

    Cursor_UnlockAllSurfaces(this);

    /* Hide the window */
    ShowWindow(this->hWnd, SW_HIDE);                /* SW_HIDE = 0 */
    this->visible2 = 0;                             /* +0x114 */
}

/* ================================================================== */
/* GameWindow::show (vtable[2])                                         */
/* Address: 0x413D10                                                    */
/*                                                                     */
/* Releases capture, sets a 190ms timer (ID 0x43), shows the window,   */
/* sets visible flags, fires vtable[7] callback, and blits the window's */
/* private surface content onto the main backbuffer.                    */
/*                                                                     */
/* Called by: HelpWnd_Show @ 0x4503F9,                                  */
/*            TrainStationWindow_Show @ 0x436EC4,                       */
/*            CGWND_AboutDialog_Show @ 0x40F2AA                       */
/* ================================================================== */
void GameWindow::show()
{
    /* Release capture */
    Cursor_SetCapture(this, 0);                     /* 0 = end capture */

    /* Create 190ms timer (ID 0x43) */
    this->timerId = (UINT_PTR)SetTimer(this->hWnd,  /* +0x08 */
                                        0x43,        /* timer ID */
                                        0xBE,        /* 190ms elapse */
                                        NULL);       /* no callback */

    /* Show the window */
    ShowWindow(this->hWnd, SW_SHOW);                /* SW_SHOW = 5 */

    /* Set visible flags */
    this->visible  = 1;                             /* +0x58 */
    this->visible2 = 1;                             /* +0x114 */

    /* Fire vtable[7] callback (update_anim) with arg=0 */
    this->update_anim(0);

    /* Unlock primary surface */
    DDRAW_UnlockPrimary(this->hWnd);                /* +0x08 */

    /* Blt: restore window content from its private surface to main backbuffer */
    ((IDirectDrawSurface4*)g_backbuffer)->Blt(                     /* 0x4FD3C0, vtable[5] */
        (RECT*)&this->rectLeft,                                     /* +0x18 dest rect */
        this->backbufferSurface,                                    /* +0x38 src surface */
        (RECT*)&this->rectLeft,                                     /* +0x18 src rect */
        DDBLT_WAIT, NULL);

    Cursor_UnlockAllSurfaces(this);
}

/* ================================================================== */
/* GameWindow::set_mode — vtable[3] callback                             */
/* Address: 0x414340 (default impl: Cursor_SetMode)                      */
/*                                                                     */
/* Sets the cursor/animation state. If stateId matches current and is   */
/* non-zero, skips to the resetRedraw/forceRedraw phase. Otherwise      */
/* updates field_14, field_44, field_48. If resetPos is set, zeroes     */
/* the reserved_work block (+0x68..+0x84). If forceRedraw is set and    */
/* window is not yet visible, performs a dirty-rect render cycle.       */
/*                                                                     */
/* Binary signature: (int32_t stateId, void* resdata, uint8_t resetPos, */
/*                    uint8_t forceRedraw)                               */
/* ================================================================== */
void GameWindow::set_mode(int32_t stateId, void* resdata, uint8_t resetPos, uint8_t forceRedraw)
{
    /* If state hasn't changed and is non-zero, skip to redraw phase */
    if (this->field_14 == stateId) {                        /* +0x14 */
        if (stateId == 0) {
            return;
        }
        if (this->field_14 == stateId) {
            goto do_redraw;
        }
    }

    /* Update state */
    this->field_14 = stateId;                               /* +0x14 */
    this->field_44 = (int32_t)resdata;                      /* +0x44 */
    this->field_48 = 0;                                     /* +0x48 */

do_redraw:
    /* Zero the reserved work block if requested */
    if (resetPos != 0) {
        this->reserved_work[0] = 0;                         /* +0x68 */
        this->reserved_work[1] = 0;                         /* +0x6C */
        this->reserved_work[2] = 0;                         /* +0x70 */
        this->reserved_work[3] = 0;                         /* +0x74 */
        this->reserved_work[4] = 0;                         /* +0x78 */
        this->reserved_work[5] = 0;                         /* +0x7C */
        this->reserved_work[6] = 0;                         /* +0x80 */
        this->reserved_work[7] = 0;                         /* +0x84 */
    }

    /* Force a redraw cycle if requested and window is not visible yet */
    if (forceRedraw != 0 && this->visible == 0) {           /* +0x58 */
        DDRAW_UnlockPrimary(this->hWnd);
        Cursor_UpdateDirtyRect(this, resetPos == 0);
        Cursor_UnlockAllSurfaces(this);
        if (this->captureFlag != 0) {                       /* +0x88 */
            Cursor_RenderWithViewport(this, resetPos);
        }
    }
}

/* ================================================================== */
/* GameWindow::set_position                                             */
/* Address: 0x413D90                                                    */
/*                                                                     */
/* Repositions the window at (x, y) using the cached client area        */
/* dimensions (+0x10C/+0x110 as width/height) to compute the window    */
/* size for SetWindowPos. Updates the cached window rect.               */
/*                                                                     */
/* Called by: HelpWnd_Show @ 0x450389                                   */
/* ================================================================== */
void GameWindow::set_position(int x, int y)
{
    int newRight  = this->clientWidth  + x;     /* +0x10C + x */
    int newBottom = this->clientHeight + y;     /* +0x110 + y */

    /* Update cached window rectangle */
    this->rectRight  = newRight;                 /* +0x20 */
    this->rectBottom = newBottom;                /* +0x24 */
    this->rectLeft   = x;                        /* +0x18 */
    this->rectTop    = y;                        /* +0x1C */

    /* Physically reposition the HWND */
    SetWindowPos(this->hWnd,                     /* +0x08 */
                 NULL,                           /* hWndInsertAfter = HWND_TOP */
                 x, y,
                 newRight  - x,                  /* width */
                 newBottom - y,                  /* height */
                 0x140);                         /* SWP_NOCOPYBITS | SWP_SHOWWINDOW */
}

/* ================================================================== */
/* GameWindow::init — vtable[6] callback                                 */
/* Address: 0x4140A0 (default impl: Cursor_UpdateClientRect)            */
/*                                                                     */
/* Called after window creation (create() fires this via vtable).       */
/* Gets the client rectangle via GetClientRect, stores it in the        */
/* temp rect (+0xF4) and client rect (+0x104) fields, then computes     */
/* working dimensions (+0xEC/+0xF0). Guarded by createdFlag (+0xDB).   */
/*                                                                     */
/* Overridden by HelpWnd::init (0x451180) and AboutDialog::Init.       */
/* ================================================================== */
void GameWindow::init()
{
    /* Only execute if the window has been created */
    if (this->createdFlag == 0) {                           /* +0xDB */
        return;
    }

    /* Get the client rectangle into the temp rect at +0xF4 */
    GetClientRect(this->hWnd, (RECT*)&this->tempLeft);     /* +0x08, +0xF4 */

    /* Compute window dimensions from temp rect */
    this->windowWidth  = this->tempRight  - this->tempLeft;  /* +0xE4 */
    this->windowHeight = this->tempBottom - this->tempTop;   /* +0xE8 */

    /* Copy temp rect to client rect */
    this->clientLeft   = this->tempLeft;                     /* +0x104 */
    this->clientTop    = this->tempTop;                      /* +0x108 */
    this->clientWidth  = this->tempRight;                    /* +0x10C */
    this->clientHeight = this->tempBottom;                   /* +0x110 */

    /* Compute working dimensions from client rect edges */
    this->workWidth  = this->clientWidth  - this->clientLeft;  /* +0xEC */
    this->workHeight = this->clientHeight - this->clientTop;   /* +0xF0 */
}

/* ================================================================== */
/* GameWindow::create (vtable[5])                                       */
/* Address: 0x413DE0                                                    */
/* Size: 701 bytes                                                      */
/*                                                                     */
/* Full window creation:                                                */
/*   1. Stores layout params (parent HWND, x, y, width, height)         */
/*   2. Registers WNDCLASS with title string as class name              */
/*   3. Creates HWND via CreateWindowExA (style = WS_POPUP |            */
/*      WS_CLIPSIBLINGS | WS_CLIPCHILDREN, passing this as lpParam)    */
/*   4. Sets createdFlag, fires vtable[6] (init callback)               */
/*   5. Calls Cursor_InitSprites for cursor overlay setup               */
/*   6. Creates offscreen DDraw surface (if none exists) via            */
/*      IDirectDraw4::CreateSurface                                     */
/*   7. Caches window rect from layout parameters                       */
/*   8. Shows the window, updates, stores captureFlag                   */
/*                                                                     */
/* Params 11 (dwExStyle compat) and 12 (reserved) are unused — they    */
/* are passed by all callers for virtual-method signature compat.       */
/*                                                                     */
/* Called by: AboutDialog_Create @ 0x40F5A1,                            */
/*            TrainStationWindow_Create @ 0x436D49,                     */
/*            HelpWnd_Create @ 0x450D40                                 */
/* ================================================================== */
int GameWindow::create(int nCmdShow, HWND hWndParent, int x, int y,
                        int nWidth, int nHeight, HMENU hMenu, HICON hIcon,
                        UINT classStyle, int /*unused1*/, int /*unused2*/,
                        uint8_t showCursor)
{
    /* Get parent window caption text for the window title bar */
    char parentTitle[256];                          /* +0x100 bytes on stack */
    GetWindowTextA(hWndParent, parentTitle, sizeof(parentTitle));

    /* Store layout parameters */
    this->windowWidth   = nWidth;                   /* +0xE4 */
    this->hWndParent    = hWndParent;               /* +0x0C */
    this->windowX       = x;                        /* +0xDC */
    this->windowY       = y;                        /* +0xE0 */
    this->windowHeight  = nHeight;                  /* +0xE8 */

    /* ---- Register WNDCLASS ---- */
    WNDCLASSA wc;                                   /* 10 DWORDs = 40 bytes at stack */
    /* Zero-initialize the full struct */
    for (int i = 0; i < 10; i++) {
        ((int32_t*)&wc)[i] = 0;
    }

    wc.style         = CS_HREDRAW | CS_VREDRAW;     /* 3 */
    if (classStyle != 0) {
        wc.style     = classStyle;                  /* caller override */
    }
    wc.hInstance     = this->hInstance;              /* +0x04 */
    wc.lpfnWndProc   = GameWindow_WndProc;            /* 0x415900, shared GameWindow WndProc */
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hIcon         = hIcon;
    wc.hCursor       = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = this->title;                  /* +0xA8 */

    ATOM atom = RegisterClassA(&wc);
    if (atom == 0) {
        DWORD err = GetLastError();
        if (err != 0) {
            char* errBuf;
            FormatMessageA(0x1100, NULL, err, 0x400, (char*)&errBuf, 0, NULL);
            LocalFree(errBuf);
        }
    }

    /* ---- Create the HWND ---- */
    this->hWnd = CreateWindowExA(
        0,                                    /* dwExStyle */
        this->title,                          /* lpClassName (same as registered) */
        parentTitle,                          /* lpWindowName */
        GW_STYLE,                             /* dwStyle (WS_POPUP | WS_CLIPSIBLINGS |
                                                 WS_CLIPCHILDREN) */
        x, y,
        nWidth, nHeight,
        this->hWndParent,                     /* +0x0C */
        hMenu,
        this->hInstance,                      /* +0x04 */
        this                                  /* lpParam = this (sent to WM_NCCREATE) */
    );

    if (this->hWnd == NULL) {
        DWORD err = GetLastError();
        char* errBuf;
        FormatMessageA(0x1100, NULL, err, 0x400, (char*)&errBuf, 0, NULL);
        LocalFree(errBuf);
        return 0;
    }

    /* Mark window as created and fire init callback (vtable[6]) */
    this->createdFlag = 1;                     /* +0xDB */
    this->init();

    /* Set up cursor sprite overlay */
    Cursor_InitSprites(this);

    /* ---- Create offscreen DDraw surface (if not already present) ---- */
    if (this->backbufferSurface == NULL) {     /* +0x38 */
        /* Stack-allocated DDSURFACEDESC2 (0x7C bytes = 31 DWORDs).
         * We use a local struct rather than stubs/ddraw.h's type because
         * the stub definition may not match the binary's exact field layout.
         * All offsets verified against Ghidra disassembly at 0x413DE0:
         * ddsCaps is at +0x68 (0xA8-0x40=0x68 from struct base at ESP+0x40). */
        struct DDSurfaceDesc {
            DWORD dwSize;           /* +0x00 */
            DWORD dwFlags;          /* +0x04 */
            DWORD dwHeight;         /* +0x08 */
            DWORD dwWidth;          /* +0x0C */
            LONG  lPitch;           /* +0x10 */
            DWORD dwBackBufferCount;/* +0x14 */
            DWORD dwMipMapCount;    /* +0x18 */
            DWORD dwAlphaBitDepth;  /* +0x1C */
            DWORD dwReserved;       /* +0x20 */
            void* lpSurface;        /* +0x24 */
            uint8_t _pad_28[0x40];  /* +0x28 padding to reach ddsCaps at +0x68 */
            DWORD ddsCaps;          /* +0x68 — DDSCAPS2.dwCaps */
            uint8_t _pad_6C[0x10];  /* +0x6C trailing padding to 0x7C total */
        };

        DDSurfaceDesc ddsd;

        /* Zero the full descriptor (31 DWORDs = 0x7C bytes) */
        int32_t* pDesc = (int32_t*)&ddsd;
        for (int i = 0; i < 31; i++) {
            pDesc[i] = 0;
        }

        ddsd.dwSize   = DDSD_SIZE;                         /* 0x7C */
        ddsd.dwFlags  = DDSD_CAPS_HEIGHT_WIDTH;            /* 7 = DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH */
        ddsd.dwWidth  = nWidth;
        ddsd.dwHeight = nHeight;
        ddsd.ddsCaps  = 0x840;                             /* DDSCAPS_OFFSCREENPLAIN (+0x68) */

        /* Call IDirectDraw4::CreateSurface (vtable[6]) */
        {
            int result = ((IDirectDraw4*)g_ddraw)->CreateSurface(       /* 0x485440 */
                &ddsd, &this->backbufferSurface, NULL);
            if (result != 0) {
                return 0;
            }
        }

        /* Get actual surface dimensions after creation */
        {
            uint16_t surfWidth, surfHeight;
            DDRAW_GetSurfaceWidthHeight(this->backbufferSurface,
                                        &surfHeight, &surfWidth);
            /* surfWidth/surfHeight are informational only */
        }

        /* Cache window rectangle from layout parameters */
        this->rectLeft    = x;                      /* +0x18 */
        this->rectRight   = x + nWidth;             /* +0x20 */
        this->rectTop     = y;                      /* +0x1C */
        this->width       = nWidth;                 /* +0x30 */
        this->height      = nHeight;                /* +0x34 */
        this->rectBottom  = y + nHeight;            /* +0x24 */
        this->field_28    = 0;                      /* +0x28 */
        this->field_2C    = 0;                      /* +0x2C */

        /* Set surface pixel format and restore */
        DDRAW_SetSurfaceFormat(this->backbufferSurface, &ddsd);
        DDRAW_RestoreSurfaces(this->backbufferSurface, &ddsd);

        this->createdFlag = 1;                     /* +0xDB */
    }

    /* Show the window */
    ShowWindow(this->hWnd, nCmdShow);
    UpdateWindow(this->hWnd);

    /* Store capture flag (byte param: 0=no capture, 1=capture) */
    this->captureFlag = showCursor;                 /* +0x88 */

    return 1;
}
/* ================================================================== */
/* GameWindow::update_anim — vtable[7] callback, fired after window shown */
/* Address: 0x426130                                                    */
/*                                                                      */
/* In the base GameWindow class this is a no-op (RET 4 in the binary). */
/* Subclasses override with real behavior:                              */
/*   - AboutDialog::update_anim (0x40F890): init dialog state & controls */
/*   - HelpWnd::update_anim      (0x450450): update help window animations */
/*                                                                      */
/* @param param  Always 0 when called from GameWindow::show()           */
/* ================================================================== */
void GameWindow::update_anim(int param)
{
    /* Base implementation: no-op (binary stub at 0x426130: RET 4) */
}

/* ================================================================== */
/* GameWindow::cleanup_sprites — vtable[4] callback                     */
/* Address: 0x426130 (shared default stub with update_anim)             */
/*                                                                     */
/* In the base GameWindow class this is a no-op (RET 4 in the binary). */
/* Overridden by HelpWnd::cleanup_sprites (0x451440) which destroys    */
/* 9 ButtonSprite objects.                                            */
/* ================================================================== */
void GameWindow::cleanup_sprites()
{
    /* Base implementation: no-op */
}

#pragma GCC diagnostic pop
