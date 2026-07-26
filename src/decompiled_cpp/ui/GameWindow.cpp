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

#include "GameWindow.h"
#include "../shared/vtable_addrs.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* CRT / memory management */
    extern void __cdecl GLOBAL_free(void* ptr);                     /* 0x465CD0 */

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
/* Non-Windows stubs: include SDL3 window shim for full implementations,
 * plus inline stubs for functions not yet covered by sdl3_window.h */
#include <SDL3/SDL.h>
#include "sdl3_window.h"

/* Inline stubs for functions not yet covered by sdl3_window.h */
static inline HWND GetCapture(void) { return NULL; }
static inline BOOL ReleaseCapture(void) { return TRUE; }
static inline int GetWindowTextA(HWND, char* buf, int max) {
    if (buf && max > 0) buf[0] = 0;
    return 0;
}
#endif /* !_WIN32 */

/* Game window subsystem functions */
extern void __fastcall Cursor_InitSprites(GameWindow* this_);              /* 0x414130 */
extern void __thiscall Cursor_UnlockAllSurfaces(GameWindow* this_);        /* 0x414EF0 */
extern void __thiscall Cursor_SetCapture(GameWindow* this_, byte capture); /* 0x414290 */
extern void __cdecl    DDRAW_UnlockPrimary(HWND hWnd);                     /* 0x45B940 */
extern void __thiscall FormatResourceString(void* resmgr, int string_id,
                                            char* out_buf, int max_len);   /* 0x447330 */

/* DDraw surface helpers (defined in native/ddraw_helpers.c) */
extern void __cdecl DDRAW_GetSurfaceWidthHeight(void* surf,
                                                uint16_t* out_w,
                                                uint16_t* out_h);          /* 0x4014E0 */
extern int  __cdecl DDRAW_SetSurfaceFormat(void* surf, int fmt);           /* 0x45B9B0 */
extern int  __cdecl DDRAW_RestoreSurfaces(void* surf, void* desc);         /* 0x45BA50 */

/* ================================================================== */
/* Global variable references                                           */
/* ================================================================== */

extern void*  g_resmgr;             /* 0x4855E8 — ResourceManager singleton */
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

    /* Zero reserved work fields (8 DWORDs at +0x68..+0x84) */
    /* +0x68 */ this->field_68 = 0;
    /* +0x70 */ this->field_70 = 0;
    /* +0x6C */ this->field_6C = 0;
    /* +0x74 */ this->field_74 = 0;
    /* +0x78 */ this->field_78 = 0;
    /* +0x80 */ this->field_80 = 0;
    /* +0x7C */ this->field_7C = 0;
    /* +0x84 */ this->field_84 = 0;

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
        void* obj = this->ddrawAuxPtr1;                         /* +0x98 */
        void** vtab = *(void***)obj;
        ((void (__fastcall*)(void*))(vtab[2]))(obj);            /* vtable[2] = Release */
        this->ddrawAuxField1 = 0;                               /* +0x94 */
        this->ddrawAuxPtr1   = NULL;                            /* +0x98 */
        this->ddrawAuxCount1 = 0;                               /* +0x90 */
    }

    /* Release auxiliary DDraw object 2 */
    if (this->ddrawAuxCount2 != 0) {                            /* +0x9C */
        void* obj = this->ddrawAuxPtr2;                         /* +0xA4 */
        void** vtab = *(void***)obj;
        ((void (__fastcall*)(void*))(vtab[2]))(obj);            /* vtable[2] = Release */
        this->ddrawAuxField2 = 0;                               /* +0xA0 */
        this->ddrawAuxPtr2   = NULL;                            /* +0xA4 */
        this->ddrawAuxCount2 = 0;                               /* +0x9C */
    }

    /* Decrement global cursor backbuffer refcount */
    if (this->cursorRefcount != 0) {                            /* +0x5C */
        g_cursor_refcount--;
        if (g_cursor_refcount == 0 && g_cursor_back != NULL) {
            void** vtab = *(void***)g_cursor_back;
            ((void (__fastcall*)(void*))(vtab[2]))(g_cursor_back);  /* vtable[2] = Release */
            g_cursor_back     = NULL;
            g_cursor_refcount = 0;
        }
        this->cursorRefcount = 0;                               /* +0x5C */
    }

    /* Release own backbuffer surface */
    void* surf = this->backbufferSurface;                        /* +0x38 */
    if (surf != NULL) {
        void** vtab = *(void***)surf;
        ((void (__fastcall*)(void*))(vtab[2]))(surf);            /* vtable[2] = Release */
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
        void** primVtab = *(void***)g_primary_surface;               /* 0x4FD3C4 */
        int result = ((int (__stdcall*)(void*, void*))(primVtab[0x80/4]))
                        (g_primary_surface, NULL);                  /* vtable[32] = Restore */
        if (result == 0) {
            g_surface_lost = 0;
        }
    }

    /* Blt: save screen content below window from primary to backbuffer */
    void** bbVtab = *(void***)g_backbuffer;                         /* 0x4FD3C0 */
    ((void (__stdcall*)(void*, RECT*, void*, RECT*, DWORD, void*))(bbVtab[0x14/4]))
        (g_backbuffer, &destRect, g_primary_surface, &srcRect,
         DDBLT_WAIT, NULL);                                         /* vtable[5] = Blt */

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

    /* Fire vtable[7] callback (on_show) with arg=0 */
    this->on_show(0);

    /* Unlock primary surface */
    DDRAW_UnlockPrimary(this->hWnd);                /* +0x08 */

    /* Blt: restore window content from its private surface to main backbuffer */
    void** bbVtab = *(void***)g_backbuffer;                         /* 0x4FD3C0 */
    ((void (__stdcall*)(void*, RECT*, void*, RECT*, DWORD, void*))(bbVtab[0x14/4]))
        (g_backbuffer, (RECT*)&this->rectLeft,                       /* +0x18 dest rect */
         this->backbufferSurface, (RECT*)&this->rectLeft,            /* +0x38 src surface, +0x18 src rect */
         DDBLT_WAIT, NULL);                                          /* vtable[5] = Blt */

    Cursor_UnlockAllSurfaces(this);
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
                 0x140);                         /* SWP_NOZORDER | SWP_NOACTIVATE */
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
    wc.lpfnWndProc   = (WNDPROC)0x415900;            /* Shared GameWindow WndProc */
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
    {
    }

    /* Set up cursor sprite overlay */
    Cursor_InitSprites(this);

    /* ---- Create offscreen DDraw surface (if not already present) ---- */
    if (this->backbufferSurface == NULL) {     /* +0x38 */
        /* Stack-allocated DDSURFACEDESC2 (0x7C bytes = 31 DWORDs) */
        struct DDSurfaceDesc {
            DWORD dwSize;       /* +0x00 */
            DWORD dwFlags;      /* +0x04 */
            DWORD dwHeight;     /* +0x08 */
            DWORD dwWidth;      /* +0x0C */
            /* ... omitted fields zeroed ... */
            DWORD caps;         /* +0x18 — DDSCAPS */
        };

        DDSurfaceDesc ddsd;

        /* Zero the full descriptor */
        int32_t* pDesc = (int32_t*)&ddsd;
        for (int i = 0; i < 31; i++) {
            pDesc[i] = 0;
        }

        ddsd.dwSize   = DDSD_SIZE;                         /* 0x7C */
        ddsd.dwFlags  = DDSD_CAPS_HEIGHT_WIDTH;            /* 7 = DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH */
        ddsd.dwWidth  = nWidth;
        ddsd.dwHeight = nHeight;
        ddsd.caps     = 0x840;                             /* DDSCAPS_OFFSCREENPLAIN */

        /* Call IDirectDraw4::CreateSurface (vtable[6]) */
        {
            void** ddrawVtab = *(void***)g_ddraw;          /* 0x485440 */
            int result = ((int (__stdcall*)(void*, DDSurfaceDesc*, void**, void*))
                         (ddrawVtab[0x18/4]))
                         (g_ddraw, &ddsd, &this->backbufferSurface, NULL);
            if (result != 0) {
                return 0;
            }
        }

        /* Get actual surface dimensions after creation */
        {
            uint16_t surfWidth, surfHeight;
            DDRAW_GetSurfaceWidthHeight(this->backbufferSurface,
                                        &surfWidth, &surfHeight);
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
        DDRAW_SetSurfaceFormat(this->backbufferSurface, (int)&ddsd);
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
/* GameWindow::on_show — vtable[7] callback, fired after window shown  */
/* Address: 0x426130                                                    */
/*                                                                      */
/* In the base GameWindow class this is a no-op (RET 4 in the binary). */
/* Subclasses override with real behavior:                              */
/*   - AboutDialog::on_show (0x40F890): init dialog state & controls    */
/*   - HelpWnd::on_show      (0x4528E0): update help window animations  */
/*                                                                      */
/* @param param  Always 0 when called from GameWindow::show()           */
/* ================================================================== */
void GameWindow::on_show(int param)
{
    /* Base implementation: no-op */
}
