/**
 * UI_WindowBase.cpp — UI_WindowBase implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "UI_WindowBase.h"
#include "../graphics/LOCOBITMAP.h"
#ifndef _WIN32
#include "sdl3_ddraw.h"
#endif

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* CRT/Windows wrappers */
    extern void __cdecl GLOBAL_free(void* ptr);     /* 0x465CD0 */

extern "C" {
    /* Win32 API imports */
    extern HWND  __stdcall CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName,
                                           LPCSTR lpWindowName, DWORD dwStyle,
                                           int X, int Y, int nWidth, int nHeight,
                                           HWND hWndParent, HMENU hMenu,
                                           HINSTANCE hInstance, void* lpParam);
    extern HWND  __stdcall SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse,
                                     void* lpTimerFunc);
    extern BOOL  __stdcall KillTimer(HWND hWnd, UINT_PTR uIDEvent);
    extern HWND  __stdcall SetCapture(HWND hWnd);
    extern BOOL  __stdcall ReleaseCapture(void);
    extern int   __stdcall ShowCursor(BOOL bShow);
    extern BOOL  __stdcall GetCursorPos(POINT* lpPoint);
    extern HWND  __stdcall WindowFromPoint(POINT pt);
    extern BOOL  __stdcall ShowWindow(HWND hWnd, int nCmdShow);
    extern BOOL  __stdcall EnableWindow(HWND hWnd, BOOL bEnable);
    extern ATOM  __stdcall RegisterClassA(const void* lpWndClass);
    extern BOOL  __stdcall GetClientRect(HWND hWnd, void* lpRect);
    extern BOOL  __stdcall UpdateWindow(HWND hWnd);
    extern DWORD __stdcall GetLastError(void);
    extern DWORD __stdcall FormatMessageA(DWORD dwFlags, const void* lpSource,
                                          DWORD dwMessageId, DWORD dwLanguageId,
                                          char* lpBuffer, DWORD nSize,
                                          void* Arguments);
    extern void* __stdcall LocalFree(void* hMem);
    extern LRESULT __stdcall DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam,
                                           LPARAM lParam);
    extern int   __stdcall GetWindowTextA(HWND hWnd, char* lpString, int nMaxCount);
    extern BOOL  __stdcall DestroyWindow(HWND hWnd);
    extern void  __stdcall PostQuitMessage(int nExitCode);
}

/* UI module function declarations */
extern void  __cdecl DDRAW_UnlockPrimary(void);             /* 0x45B940 */
extern void  __fastcall Cursor_SetupSurface(int this_ptr);  /* 0x425DC0 */
extern void  __fastcall UIPANEL_Render(void* panel, byte flag);  /* 0x426EB0 */
extern void  __thiscall FormatResourceString(void* resmgr, int string_id,
                                             char* out_buf, int max_len);  /* 0x447330 */

/* ================================================================== */
/* Global state                                                        */
/* ================================================================== */

extern void* g_resmgr;          /* 0x4855E8 — ResourceManager singleton */
extern void* g_main_window;     /* 0x4AA4A0 — main CGWND window ptr */

/* Global cursor backbuffer surface + refcount (shared among all UI windows) */
extern void* g_cursor_back;     /* 0x4FD3CC */
extern int   g_cursor_refcount;  /* 0x4FD3D0 */

/* Forward declaration of the default WindowProc stub — serves as
   vtable[11] and as the base for the WNDCLASS registration. */
extern LRESULT __stdcall UI_DefWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
/* 0x422EA0 — just calls DefWindowProcA */

namespace {
using ChildReleaseFunction = void (__fastcall *)(void*);

/* The three child slots are heterogeneous binary UI objects, but their
 * recovered cleanup path uses the common vtable slot 2. */
void release_child_object(void* object)
{
    ChildReleaseFunction* vtable =
        *reinterpret_cast<ChildReleaseFunction**>(object);
    vtable[2](object);
}
}

/* ================================================================== */
/* UI_WindowBase Constructor                                           */
/* Address: 0x425870                                                    */
/*                                                                     */
/* Sets the vtable, stores instance handle and resource ID, zeroes all */
/* fields, and loads the window title string from string resources.    */
/*                                                                     */
/* Called by: ALL subconstructors (Cursor, EditWindow, PostcardAlbum,   */
/*            Town, PostcardPreviewWindow, NameEntryPanel,              */
/*            GameSetupPanel, etc.)                                     */
/* ================================================================== */
UI_WindowBase::UI_WindowBase(HINSTANCE hInstance, UINT resId)
{
    this->hInstance  = hInstance;                       /* +0x04 */
    /* In the binary: sets vtable to 0x477C30 (VTBL_UI_WINDOWBASE).
     * Natural C++ handles this via the compiler. */

    /* Zero all working fields */
    this->hWnd         = NULL;                          /* +0x08 */
    this->hWndParent   = NULL;                          /* +0x0C */
    this->cursorRefCount = 0;                           /* +0x48 */
    this->childCount1  = 0;                             /* +0x68 */
    this->childCount0  = 0;                             /* +0x60 */
    this->childObj2    = NULL;                          /* +0x74 */
    this->captureFlag  = 0;                             /* +0x3C */
    this->field_14     = 0;                             /* +0x14 */
    this->field_20     = 0;                             /* +0x20 */
    this->field_18     = 0;                             /* +0x18 */
    this->field_1C     = 0;                             /* +0x1C */
    this->field_24     = 0;                             /* +0x24 */
    this->field_3D     = 0;                             /* +0x3D */
    this->field_40     = 0;                             /* +0x40 */
    this->timerId      = 0;                             /* +0x28 */
    this->field_2C     = 0;                             /* +0x2C */
    this->field_30     = 0;                             /* +0x30 */
    this->windowCreated = 0;                            /* +0xAB */

    this->resourceId   = resId;                         /* +0x10 */

    this->field_50     = 0;                             /* +0x50 */
    this->field_58     = 0;                             /* +0x58 */
    this->field_54     = 0;                             /* +0x54 */
    this->field_5C     = 0;                             /* +0x5C */

    /* Load window title from string resources */
    FormatResourceString(&g_resmgr, resId, this->title, sizeof(this->title));  /* +0x78, 50 bytes */

    this->visible      = 0;                             /* +0xE4 */
    this->activeFlag   = 0;                             /* +0x44 */
}

/* ================================================================== */
/* UI_WindowBase::~UI_WindowBase — Virtual destructor (vtable[0])      */
/*                                                                     */
/* In the binary: MSVC scalar deleting destructor at 0x4258F0 calls    */
/* base_destructor, then GLOBAL_free if flags & 1.                     */
/* ================================================================== */
UI_WindowBase::~UI_WindowBase()
{
    this->base_destructor();
}

/* ================================================================== */
/* UI_WindowBase::base_destructor                                      */
/* Address: 0x425910                                                    */
/*                                                                     */
/* Release sequence:                                                    */
/*   1. Reset vtable to base (for correct dispatch during dtor chain)   */
/*   2. Release three child sub-objects via vtable[2] if non-null       */
/*   3. Decrement global cursor backbuffer refcount,                    */
/*      free the shared surface when reaching 0                         */
/*   4. Clear visible flag                                              */
/* ================================================================== */
void UI_WindowBase::base_destructor()
{
    /* In the binary: resets vtable to 0x477C30. Natural C++ handles
     * vtable during destruction automatically. */

    /* Release child sub-object pairs via vtable[2] */
    if (this->childCount0 != 0) {                        /* +0x60 */
        void* obj = this->childObj0;                     /* +0x64 */
        release_child_object(obj);
        this->childObj0   = NULL;                        /* +0x64 */
        this->childCount0 = 0;                           /* +0x60 */
    }

    if (this->childCount1 != 0) {                        /* +0x68 */
        void* obj = this->childObj1;                     /* +0x6C */
        release_child_object(obj);
        this->childObj1   = NULL;                        /* +0x6C */
        this->childCount1 = 0;                           /* +0x68 */
    }

    if (this->childObj2 != NULL) {                       /* +0x74 */
        void* obj = this->childObj2;
        release_child_object(obj);
        this->childObj2    = NULL;                       /* +0x74 */
        this->childCount2  = 0;                          /* +0x70 */
    }

    /* Decrement global cursor backbuffer refcount */
    if (this->cursorRefCount != 0) {                     /* +0x48 */
        g_cursor_refcount--;
        if (g_cursor_refcount == 0 && g_cursor_back != NULL) {
            release_child_object(g_cursor_back);
            g_cursor_back    = NULL;
            g_cursor_refcount = 0;
        }
        this->cursorRefCount = 0;                        /* +0x48 */
    }

    this->visible = 0;                                   /* +0xE4 */
}

/* ================================================================== */
/* UI_WindowBase::hide (vtable[1])                                     */
/* Address: 0x425990                                                    */
/*                                                                     */
/* Hides the window and stops its timer. Called by subclass Hide()     */
/* overrides (e.g., Cursor_Hide chains to UI_WindowBase_Hide).         */
/* ================================================================== */
void UI_WindowBase::hide()
{
    ShowWindow(this->hWnd, SW_HIDE);        /* SW_HIDE = 0 */
    if (this->timerId != 0) {
        KillTimer(this->hWnd, this->timerId);
    }
    this->visible    = 0;                   /* +0xE4 */
    this->activeFlag = 0;                   /* +0x44 */
}

/* ================================================================== */
/* UI_WindowBase::show (vtable[2])                                     */
/* Address: 0x4259C0                                                    */
/*                                                                     */
/* Shows the window as pseudo-modal:                                    */
/*   1. SetTimer(120ms, ID 0x43) — periodic tick timer                 */
/*   2. SetCapture — all mouse input goes to this window                */
/*   3. Hide OS cursor via ShowCursor(FALSE) loop                      */
/*   4. Unlock primary surface, render UI, unlock again                 */
/*   5. Disable window (no input acceptance)                           */
/*   6. ShowWindow(SW_SHOW)                                            */
/*   7. Set visible flag                                               */
/* ================================================================== */
void UI_WindowBase::show()
{
    this->timerId = reinterpret_cast<UINT_PTR>(
        SetTimer(this->hWnd, 0x43, 120, NULL));

    this->captureFlag = 0;                  /* +0x3C */

#ifdef _WIN32
    SetCapture(this->hWnd);
#else
    // SDL routes input through its event queue; there is no Win32 capture
    // import to call on the host.
#endif

    /* Hide the OS cursor — loop until ShowCursor returns < 0. */
#ifdef _WIN32
    int cursorVis = ShowCursor(FALSE);
    while (cursorVis >= 0) {
        cursorVis = ShowCursor(FALSE);
    }
#else
    // SDL cursor ownership is handled by the window shim. Do not call the
    // unresolved Win32 import or emulate its counter in a busy loop.
#endif

    std::fprintf(stderr, "[TRACE] UI_WindowBase::show: unlock primary\n");
    DDRAW_UnlockPrimary();
    std::fprintf(stderr, "[TRACE] UI_WindowBase::show: render panel\n");
#ifdef _WIN32
    UIPANEL_Render(this, 1);
#else
    // The SDL compositor receives the decoded EditWindow sprites directly;
    // do not reinterpret this 64-bit UI_WindowBase as the x86 UIPANEL layout.
    SDL3_PresentPrimarySurface();
#endif
    std::fprintf(stderr, "[TRACE] UI_WindowBase::show: unlock primary complete\n");
    DDRAW_UnlockPrimary();

    std::fprintf(stderr, "[TRACE] UI_WindowBase::show: show window\n");
    EnableWindow(this->hWnd, FALSE);
    ShowWindow(this->hWnd, SW_SHOW);        /* SW_SHOW = 5 */

    this->visible = 1;                      /* +0xE4 */
}

/* ================================================================== */
/* UI_WindowBase::set_mode (vtable[3])                                */
/* Address: 0x425FD0                                                   */
/* ================================================================== */
void UI_WindowBase::set_mode(int32_t surface_address, void* animation_metadata,
                             uint8_t reset_position, uint8_t force_redraw)
{
#ifndef _WIN32
    // The SDL host uses EditWindow::hostRenderFrame rather than the x86
    // UIPANEL renderer. Its widened object layout cannot safely interpret
    // the binary-only surface/metadata overlay at +0x60/+0x64.
    (void)surface_address;
    (void)animation_metadata;
    (void)reset_position;
    (void)force_redraw;
    return;
#else
    const auto* const metadata = static_cast<const UIAnimationMetadata*>(animation_metadata);
    const UIAnimationOrigin origin = {metadata->hotspot_x, metadata->hotspot_y};

    // The original x86 value at +0x60 is an address when this base
    // implementation is selected. Cursor's override interprets the same
    // argument as an animation state instead.
    const auto* const surface = reinterpret_cast<UIPANEL_Surface*>(
        static_cast<uintptr_t>(static_cast<uint32_t>(surface_address)));
    this->set_render_surface(surface, metadata->frame_count, &origin,
                             reset_position, force_redraw);
#endif
}

/* ================================================================== */
/* UI_WindowBase::set_render_surface (vtable[4])                      */
/* Address: 0x426020                                                   */
/* ================================================================== */
void UI_WindowBase::set_render_surface(UIPANEL_Surface* surface, uint32_t frame_divisor,
                                       const UIAnimationOrigin* origin,
                                       uint8_t reset_dirty_rect, uint8_t force_redraw)
{
#ifndef _WIN32
    // UIPANEL_Render (0x426EB0) follows the packed x86 UIPANEL layout and
    // is replaced on the host by EditWindow::hostRenderFrame. Retaining
    // this native-only callback would dereference incompatible host fields.
    (void)surface;
    (void)frame_divisor;
    (void)origin;
    (void)reset_dirty_rect;
    (void)force_redraw;
    return;
#else
    const int32_t surface_address = static_cast<int32_t>(reinterpret_cast<uintptr_t>(surface));
    if (this->field_14 == surface_address) {
        if (surface == nullptr) return;
    } else {
        this->field_14 = surface_address;
        this->field_20 = static_cast<int32_t>(frame_divisor);
        this->field_24 = 0;
        if (origin == nullptr) {
            this->field_2C = 0;
            this->field_30 = 0;
        } else {
            this->field_2C = origin->x;
            this->field_30 = origin->y;
        }

        if (surface == nullptr) {
            this->field_18 = 0;
            this->field_1C = 0;
        } else if (frame_divisor == 0) {
            this->field_18 = surface->width;
            this->field_1C = surface->height;
        } else {
            // 0x426079 uses DIV (unsigned), so retain unsigned division.
            this->field_18 = static_cast<int32_t>(
                static_cast<uint32_t>(surface->width) / frame_divisor);
            this->field_1C = surface->height;
        }
    }

    if (reset_dirty_rect != 0) {
        this->field_50 = 0;
        this->field_54 = 0;
        this->field_58 = 0;
        this->field_5C = 0;
    }
    if (force_redraw != 0 && this->captureFlag == 0) {
        DDRAW_UnlockPrimary();
        UIPANEL_Render(this, reset_dirty_rect == 0);
        DDRAW_UnlockPrimary();
    }

    if (this->field_14 != 0) {
        KillTimer(this->hWnd, this->timerId);
        const UINT interval = this->field_14 == this->childCount2 ? 0x32 : 0x78;
        this->timerId = reinterpret_cast<UINT_PTR>(SetTimer(this->hWnd, 0x43, interval, NULL));
    }
#endif
}

/* ================================================================== */
/* UI_WindowBase::on_async_task_failure (vtable[5])                   */
/* Address: 0x426130                                                   */
/* ================================================================== */
void UI_WindowBase::on_async_task_failure(int32_t /* reason */)
{
    // The original is a three-byte RET 4 no-op callback.
}

/* ================================================================== */
/* UI_WindowBase::on_create (vtable[7])                                 */
/* Address: 0x425D30                                                    */
/*                                                                     */
/* Called after window creation (from CreateFullWindow) or on resize.  */
/* Synchronizes the client rect cache and computes window/working       */
/* dimensions. Only executes when windowCreated flag (+0xAB) is set.   */
/* ================================================================== */
void UI_WindowBase::on_create()
{
    if (!this->windowCreated) {              /* +0xAB */
        return;
    }

    /* Get current client rectangle from the HWND */
    GetClientRect(this->hWnd, &this->clientRect);      /* +0xC4 (RECT) */

    /* Compute window dimensions */
    this->windowWidth  = this->clientRect.right  - this->clientRect.left;   /* +0xB4 */
    this->windowHeight = this->clientRect.bottom - this->clientRect.top;    /* +0xB8 */

    /* Copy client rect to working rect */
    this->workRect = this->clientRect;                   /* +0xD4 <- +0xC4 */

    /* Compute working dimensions */
    this->workWidth  = this->workRect.right  - this->workRect.left;         /* +0xBC */
    this->workHeight = this->workRect.bottom - this->workRect.top;          /* +0xC0 */
}

/* ================================================================== */
/* UI_CreateFullWindow (vtable[6])                                      */
/* Address: 0x425B70                                                    */
/*                                                                     */
/* Registers a WNDCLASS for this window (using title string as class    */
/* name) and creates a full-screen WS_POPUP window via CreateWindowExA. */
/* Posts OnCreate (vtable[7]), calls Cursor_SetupSurface, shows window. */
/*                                                                     */
/* Returns: 1 on success, 0 on failure.                                */
/* ================================================================== */
int UI_WindowBase::create_full_window(int nCmdShow,
                                       HWND hParent, int x, int y,
                                       int nWidth, int nHeight,
                                       HMENU hMenu, HICON hIcon, UINT classStyle)
{
    /* Clear existing HWND before creating new one */
    this->hWnd = NULL;

    /* Get parent window caption text for the window title */
    char parentTitle[256];
    GetWindowTextA(hParent, parentTitle, sizeof(parentTitle));

    /* Store layout parameters */
    this->windowX      = x;
    this->windowY      = y;
    this->windowWidth  = nWidth;
    this->windowHeight = nHeight;
    this->hWndParent   = hParent;

    /* Register WNDCLASS */
    // The original zeros 40 bytes for x86 WNDCLASS. Value initialization is
    // layout-safe on the 64-bit SDL host as well.
    WNDCLASSA wc{};
    wc.style       = CS_HREDRAW | CS_VREDRAW;  /* 3 = CS_HREDRAW | CS_VREDRAW */
    if (classStyle != 0) {
        wc.style   = classStyle;
    }
    wc.hInstance   = this->hInstance;            /* +0x04 */
    wc.lpfnWndProc = &DefWindowProcA;            /* NOTE: actual base WndProc at 0x4272F0 */
    wc.cbClsExtra  = 0;
    wc.cbWndExtra  = 0;
    wc.hIcon       = hIcon;
    wc.hCursor     = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = this->title;              /* +0x78 — title string as class name */

    ATOM atom = RegisterClassA(&wc);
    if (atom == 0) {
        DWORD err = GetLastError();
        if (err != 0) {
            char* buf;
            FormatMessageA(0x1100, NULL, err, 0x400,
                           reinterpret_cast<char*>(&buf), 0, NULL);
            LocalFree(buf);
        }
    }

    /* Create the actual window */
    this->hWnd = CreateWindowExA(
        0,                                    /* dwExStyle */
        this->title,                          /* lpClassName (same as reg'd class) */
        parentTitle,                          /* lpWindowName */
        0x87000000,                           /* dwStyle: WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS |
                                                 WS_CLIPCHILDREN | WS_SYSMENU */
        x, y,
        this->windowWidth,  this->windowHeight,
        this->hWndParent,
        hMenu,
        this->hInstance,
        this                                /* lpParam = this */
    );

    if (this->hWnd == NULL) {
        DWORD err = GetLastError();
        char* buf;
        FormatMessageA(0x1100, NULL, err, 0x400,
                       reinterpret_cast<char*>(&buf), 0, NULL);
        LocalFree(buf);
        return 0;
    }

    /* Mark window as created and fire OnCreate (vtable[7]) */
    this->windowCreated = 1;
    this->on_create();

#ifdef _WIN32
    Cursor_SetupSurface((int)self);  /* set up cursor surface for this window */
#else
    // Cursor_SetupSurface has not yet been reconstructed for the host and its
    // original int pointer ABI truncates this on 64-bit. SDL owns the cursor.
#endif
    ShowWindow(this->hWnd, nCmdShow);
    UpdateWindow(this->hWnd);

    return 1;
}


/* ================================================================== */
/* Message-handler slots [8]-[36] base defaults (binary vtable 0x477C30) */
/*                                                                     */
/* Each matches the binary default at the corresponding slot:          */
/*   [8]  0x426130 RET 4 no-op  |  [9]  0x4661A0 bare RET              */
/*   [10] 0x426140 WM_* router  |  [11] 0x422EA0 UI_DefWndProc         */
/*   [12]-[19],[21]-[22],[24]-[25],[29],[33]-[36] -> 0x422EA0          */
/*   [20] 0x426900 UIPANEL_WindowProc | [23] 0x426950 returns 0        */
/*   [26] 0x426960 re-runs on_create  | [27] 0x426980 WM_PAINT         */
/*   [28] 0x426A60 returns 1 on match | [30] 0x426AC0 returns 1        */
/*   [31] 0x426AD0 clears hWnd        | [32] 0x426A90 UIPANEL_OnDestroy*/
/* ================================================================== */

/* vtable[8] - RET 4 no-op in the binary */
void UI_WindowBase::on_update(int32_t /* param */)
{
}

/* vtable[9] - bare RET in the binary */
void UI_WindowBase::on_noop()
{
}

/* vtable[10] - WM_* router 0x426140
 *
 * Most messages are a pure "dispatch to typed virtual slot" router (see the
 * table below). Four messages are NOT pure dispatch — they contain real
 * inline logic (capture/cursor-visibility state machine and an idle-hover
 * animation throttle) before falling through to (or instead of) a virtual
 * slot call. This was previously mis-transcribed as pure dispatch for all
 * four; re-validated instruction-by-instruction against the disassembly at
 * 0x426140 (see git history for the prior, incorrect version):
 *
 *   WM_NCHITTEST (0x84): NOT a virtual dispatch at all. Calls DefWindowProcA
 *     directly. If the result is HTCLIENT (1), captures the mouse and hides
 *     the OS cursor (spin-looping ShowCursor(FALSE) until it reports hidden);
 *     otherwise releases capture and restores the OS cursor (spin-looping
 *     ShowCursor(TRUE) until it reports visible). Either way, re-renders the
 *     panel bracketed by DDRAW_UnlockPrimary(), then returns the
 *     DefWindowProcA hit-test result.
 *
 *   WM_MOUSEMOVE (0x200): only acts when `hWnd` is this window's own HWND
 *     (else returns 0 without dispatching). Resolves the HWND under the
 *     cursor via WindowFromPoint and compares it against captureFlag's
 *     current state to decide whether the cursor just left or re-entered
 *     this window, toggling capture/cursor-visibility and re-rendering on
 *     the transition (see captureFlag's doc comment in the header for the
 *     0/1 meaning). Always falls through to the real on_mouse_move()
 *     virtual slot afterward EXCEPT when the cursor is leaving the window
 *     (captureFlag was already 1 and the point is off-window: returns 0
 *     immediately) or has just left it (captureFlag transitions 0 -> 1:
 *     returns 0 immediately after the render, without dispatching).
 *
 *   WM_TIMER (0x113): a fast path runs BEFORE on_timer() when wParam == 0x43
 *     (the periodic UI tick started by show()) AND field_14 != 0 (a render
 *     surface is configured) AND captureFlag == 0 (not mid-drag) AND
 *     field_3D == 0 (idle animation not yet expired). The fast path samples
 *     GetCursorPos, advances the field_24 animation-tick counter (wrapping
 *     at field_20, decrementing the field_40 idle-cycle countdown on each
 *     wrap and latching field_3D once field_40 reaches 0), and re-renders
 *     the panel (bracketed by DDRAW_UnlockPrimary()) if the cursor has not
 *     moved since the last tick (lastCursorX/lastCursorY), then always
 *     updates lastCursorX/lastCursorY and returns 0 WITHOUT dispatching to
 *     on_timer(). If the fast-path condition is false, falls through to the
 *     normal on_timer() virtual dispatch.
 *
 *   WM_CAPTURECHANGED (0x215): NOT in the virtual-dispatch table at all —
 *     always returns 0 directly. No-ops when field_14 == 0 (no surface
 *     configured), when lParam already equals this window's own HWND (we
 *     are the window gaining capture), or when lParam == 0. Otherwise
 *     another window is taking capture away from us: marks captureFlag = 1,
 *     releases capture, restores the OS cursor (spin-loop), and re-renders
 *     the panel bracketed by DDRAW_UnlockPrimary().
 *
 * All other messages remain pure dispatch:
 *   WM_ERASEBKGND(0x14) -> on_erase_bkgnd  [30]
 *   WM_CREATE(1)        -> on_create_msg   [13]
 *   WM_DESTROY(2)       -> on_destroy      [31]
 *   WM_SIZE(5)          -> on_size         [26]
 *   WM_SETFOCUS(7)      -> on_set_focus    [24]
 *   WM_KILLFOCUS(8)     -> on_kill_focus   [25]
 *   WM_PAINT(0xF)       -> on_paint        [27]
 *   WM_CLOSE(0x10)      -> on_close        [32]
 *   WM_SHOWWINDOW(0x18) -> on_show_window  [29]
 *   WM_ACTIVATEAPP(0x1C)-> on_activate_app [36]
 *   WM_SETCURSOR(0x20)  -> on_set_cursor   [28]
 *   WM_MOUSEACTIVATE(0x21)-> on_mouse_activate [23]
 *   WM_NOTIFY(0x4E)     -> on_notify       [33]
 *   WM_KEYDOWN(0x100)   -> on_key_down     [21]
 *   WM_KEYUP(0x101)     -> on_key_up       [22]
 *   WM_COMMAND(0x111)   -> on_command      [34]
 *   WM_MOUSEMOVE(0x200) -> on_mouse_move   [20]  (see fast-path note above)
 *   WM_LBUTTONDOWN(0x201)-> on_lbutton_down [14]
 *   WM_LBUTTONUP(0x202) -> on_lbutton_up   [15]
 *   WM_LBUTTONDBLCLK(0x203)-> on_lbutton_dblclk [18]
 *   WM_RBUTTONDOWN(0x204)-> on_rbutton_down [16]
 *   WM_RBUTTONUP(0x205) -> on_rbutton_up   [17]
 *   WM_RBUTTONDBLCLK(0x206)-> on_rbutton_dblclk [19]
 *   0x312               -> on_msg_312      [35]
 *   default             -> window_proc     [11]
 *
 * NOTE: the real disassembly also shows WM_LBUTTONDOWN/UP/DBLCLK and
 * WM_RBUTTONDOWN/DBLCLK (0x201/0x203/0x204/0x206) calling
 * SetForegroundWindow(this->hWnd) before their virtual dispatch. That is a
 * pre-existing simplification in this file, out of scope for this pass
 * (which only re-validates WM_NCHITTEST/WM_MOUSEMOVE/WM_TIMER/
 * WM_CAPTURECHANGED) — left untouched here; flagged for a future pass.
 */
LRESULT UI_WindowBase::dispatch_message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case 0x14:  return on_erase_bkgnd(hWnd, msg, wParam, lParam);
    case 1:     return on_create_msg(hWnd, msg, wParam, lParam);
    case 2:     return on_destroy(hWnd, msg, wParam, lParam);
    case 5:     return on_size(hWnd, msg, wParam, lParam);
    case 7:     return on_set_focus(hWnd, msg, wParam, lParam);
    case 8:     return on_kill_focus(hWnd, msg, wParam, lParam);
    case 0xF:   return on_paint(hWnd, msg, wParam, lParam);
    case 0x10:  return on_close(hWnd, msg, wParam, lParam);
    case 0x18:  return on_show_window(hWnd, msg, wParam, lParam);
    case 0x1C:  return on_activate_app(hWnd, msg, wParam, lParam);
    case 0x20:  return on_set_cursor(hWnd, msg, wParam, lParam);
    case 0x21:  return on_mouse_activate(hWnd, msg, wParam, lParam);
    case 0x4E:  return on_notify(hWnd, msg, wParam, lParam);
    case 0x100: return on_key_down(hWnd, msg, wParam, lParam);
    case 0x101: return on_key_up(hWnd, msg, wParam, lParam);
    case 0x111: return on_command(hWnd, msg, wParam, lParam);

    case 0x113: {  // WM_TIMER — fast idle-hover-animation path, else on_timer() [12]
        if (wParam == 0x43 && this->field_14 != 0 &&
            this->captureFlag == 0 && this->field_3D == 0) {
            POINT cursorPos;
            GetCursorPos(&cursorPos);

            if (this->field_20 < 2) {
                return 0;
            }

            int32_t tickCounter = this->field_24;
            this->field_24 = tickCounter + 1;
            if (this->field_20 <= tickCounter + 1) {
                this->field_24 = 0;
                if (this->field_40 != 0) {
                    this->field_40 -= 1;
                    if (this->field_40 == 0) {
                        this->field_3D = 1;
                    }
                }
            }

            if (cursorPos.x == this->lastCursorX && cursorPos.y == this->lastCursorY) {
                DDRAW_UnlockPrimary();
                UIPANEL_Render(this, 1);
                DDRAW_UnlockPrimary();
            }
            this->lastCursorX = cursorPos.x;
            this->lastCursorY = cursorPos.y;
            return 0;
        }
        return on_timer(hWnd, msg, wParam, lParam);
    }

    case 0x200: {  // WM_MOUSEMOVE — capture/cursor state machine, then on_mouse_move() [20]
        if (hWnd != this->hWnd) {
            return 0;
        }

        POINT pt;
        pt.x = static_cast<short>(lParam & 0xFFFF);
        pt.y = static_cast<short>(static_cast<uint32_t>(lParam) >> 16);
        HWND hitWnd = WindowFromPoint(pt);

        if (this->captureFlag == 0) {
            if (hitWnd != this->hWnd) {
                // Cursor just left this window: release capture, restore
                // the OS cursor, re-render, and skip the virtual dispatch.
                this->captureFlag = 1;
                ReleaseCapture();
                int cursorVis = ShowCursor(TRUE);
                while (cursorVis < 0) {
                    cursorVis = ShowCursor(TRUE);
                }
                DDRAW_UnlockPrimary();
                UIPANEL_Render(this, 1);
                DDRAW_UnlockPrimary();
                return 0;
            }
            // Still hovering: no state change, fall through to dispatch.
        } else {
            if (hitWnd != this->hWnd) {
                // Already released and still off-window: nothing to do.
                return 0;
            }
            // Cursor (re-)entered this window: capture it, hide the OS
            // cursor, and re-render before dispatching.
            this->captureFlag = 0;
            SetCapture(this->hWnd);
            int cursorVis = ShowCursor(FALSE);
            while (cursorVis >= 0) {
                cursorVis = ShowCursor(FALSE);
            }
            DDRAW_UnlockPrimary();
            UIPANEL_Render(this, 1);
            DDRAW_UnlockPrimary();
        }
        return on_mouse_move(hWnd, msg, wParam, lParam);
    }

    case 0x201: return on_lbutton_down(hWnd, msg, wParam, lParam);
    case 0x202: return on_lbutton_up(hWnd, msg, wParam, lParam);
    case 0x203: return on_lbutton_dblclk(hWnd, msg, wParam, lParam);
    case 0x204: return on_rbutton_down(hWnd, msg, wParam, lParam);
    case 0x205: return on_rbutton_up(hWnd, msg, wParam, lParam);
    case 0x206: return on_rbutton_dblclk(hWnd, msg, wParam, lParam);
    case 0x312: return on_msg_312(hWnd, msg, wParam, lParam);

    case 0x84: {  // WM_NCHITTEST — direct DefWindowProcA, not a virtual slot
        LRESULT hitResult = DefWindowProcA(hWnd, msg, wParam, lParam);
        if (hitResult == 1) {  // HTCLIENT
            this->captureFlag = 0;
            SetCapture(this->hWnd);
            int cursorVis = ShowCursor(FALSE);
            while (cursorVis >= 0) {
                cursorVis = ShowCursor(FALSE);
            }
        } else {
            this->captureFlag = 1;
            ReleaseCapture();
            int cursorVis = ShowCursor(TRUE);
            while (cursorVis < 0) {
                cursorVis = ShowCursor(TRUE);
            }
        }
        DDRAW_UnlockPrimary();
        UIPANEL_Render(this, 1);
        DDRAW_UnlockPrimary();
        return hitResult;
    }

    case 0x215: {  // WM_CAPTURECHANGED — not a virtual slot, always returns 0
        if (this->field_14 == 0) {
            return 0;
        }
        if (reinterpret_cast<HWND>(static_cast<uintptr_t>(static_cast<uint32_t>(lParam))) == this->hWnd) {
            return 0;
        }
        if (lParam == 0) {
            return 0;
        }
        this->captureFlag = 1;
        ReleaseCapture();
        int cursorVis = ShowCursor(TRUE);
        while (cursorVis < 0) {
            cursorVis = ShowCursor(TRUE);
        }
        DDRAW_UnlockPrimary();
        UIPANEL_Render(this, 1);
        DDRAW_UnlockPrimary();
        return 0;
    }

    default:    return window_proc(hWnd, msg, wParam, lParam);
    }
}

/* vtable[11] - UI_DefWndProc 0x422EA0 passthrough */
LRESULT UI_WindowBase::window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[12] */
LRESULT UI_WindowBase::on_timer(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[13] */
LRESULT UI_WindowBase::on_create_msg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[14] */
LRESULT UI_WindowBase::on_lbutton_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[15] */
LRESULT UI_WindowBase::on_lbutton_up(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[16] */
LRESULT UI_WindowBase::on_rbutton_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[17] */
LRESULT UI_WindowBase::on_rbutton_up(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[18] */
LRESULT UI_WindowBase::on_lbutton_dblclk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[19] */
LRESULT UI_WindowBase::on_rbutton_dblclk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[20] - UIPANEL_WindowProc 0x426900 */
LRESULT UI_WindowBase::on_mouse_move(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (hWnd == this->hWnd) {
        DDRAW_UnlockPrimary();
        UIPANEL_Render(this, 1);
        DDRAW_UnlockPrimary();
    }
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[21] */
LRESULT UI_WindowBase::on_key_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[22] */
LRESULT UI_WindowBase::on_key_up(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[23] - 0x426950 returns 0 */
LRESULT UI_WindowBase::on_mouse_activate(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd; (void)msg; (void)wParam; (void)lParam;
    return 0;
}

/* vtable[24] */
LRESULT UI_WindowBase::on_set_focus(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[25] */
LRESULT UI_WindowBase::on_kill_focus(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[26] - 0x426960 re-runs on_create when windowCreated */
LRESULT UI_WindowBase::on_size(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (this->windowCreated != 0) {
        this->on_create();
    }
    (void)hWnd; (void)msg; (void)wParam; (void)lParam;
    return 0;
}

/* vtable[27] - 0x426980 WM_PAINT handler */
LRESULT UI_WindowBase::on_paint(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DDRAW_UnlockPrimary();
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[28] - 0x426A60 returns 1 when hwnd matches */
LRESULT UI_WindowBase::on_set_cursor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (hWnd != this->hWnd) {
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 1;
}

/* vtable[29] */
LRESULT UI_WindowBase::on_show_window(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[30] - 0x426AC0 returns 1 */
LRESULT UI_WindowBase::on_erase_bkgnd(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd; (void)msg; (void)wParam; (void)lParam;
    return 1;
}

/* vtable[31] - 0x426AD0 clears hWnd, DefWindowProcA */
LRESULT UI_WindowBase::on_destroy(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    this->hWnd = NULL;
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[32] - 0x426A90 UIPANEL_OnDestroy */
LRESULT UI_WindowBase::on_close(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    this->windowCreated = 0;
    if (hWnd != NULL) {
        DestroyWindow(hWnd);
    }
    if (this->hWndParent == NULL) {
        PostQuitMessage(0);
    }
    (void)msg; (void)wParam; (void)lParam;
    return 0;
}

/* vtable[33] */
LRESULT UI_WindowBase::on_notify(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[34] */
LRESULT UI_WindowBase::on_command(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[35] */
LRESULT UI_WindowBase::on_msg_312(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* vtable[36] */
LRESULT UI_WindowBase::on_activate_app(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
    return 0;
}

/* ================================================================== */
/* UI_SetWindowVisible                                                  */
/* Address: 0x425F20                                                    */
/*                                                                     */
/* NOT a UI_WindowBase vtable slot — a free function called directly on */
/* a UI_WindowBase-derived receiver by ui/EditWindow.cpp (this->show()) */
/* and ui/HelpWnd.cpp (g_town/g_postcard/g_cursor, all UI_WindowBase    */
/* subclasses: Town, PostcardAlbum, Cursor). Confirmed by disassembly:  */
/* the receiver's +0x08 (SetCapture argument) and +0x3C (stored flag)   */
/* match UI_WindowBase::hWnd and UI_WindowBase::captureFlag exactly.    */
/*                                                                     */
/* visible == 0: captures the mouse and hides the OS cursor (the same  */
/*   capture step UI_WindowBase::show() performs).                     */
/* visible != 0: releases capture and restores the OS cursor.          */
/* Either way, re-renders the panel.                                    */
/*                                                                     */
/* One canonical `char` overload — see UI_WindowBase.h for why a second */
/* `unsigned char` overload cannot coexist with it.                     */
/* ================================================================== */
namespace {
void SetWindowVisibleImpl(UI_WindowBase* self, bool visible)
{
    self->captureFlag = visible ? 1 : 0;   /* +0x3C */

#ifdef _WIN32
    if (!visible) {
        SetCapture(self->hWnd);
        int cursorVis = ShowCursor(FALSE);
        while (cursorVis >= 0) {
            cursorVis = ShowCursor(FALSE);
        }
    } else {
        ReleaseCapture();
        int cursorVis = ShowCursor(TRUE);
        while (cursorVis < 0) {
            cursorVis = ShowCursor(TRUE);
        }
    }
#else
    // SDL routes input capture and cursor visibility through its own
    // event queue and window API; there is no Win32 capture/ShowCursor
    // import to call on the host (see UI_WindowBase::show() above).
#endif

    DDRAW_UnlockPrimary();
#ifdef _WIN32
    UIPANEL_Render(self, 1);
#else
    // UIPANEL_Render (0x426EB0) follows the packed x86 UIPANEL layout,
    // which does not apply to this widened UI_WindowBase object; the SDL
    // compositor presents the already-composed primary surface instead.
    SDL3_PresentPrimarySurface();
#endif
    DDRAW_UnlockPrimary();
}
} // namespace

void UI_SetWindowVisible(void* self, char visible)
{
    SetWindowVisibleImpl(static_cast<UI_WindowBase*>(self), visible != 0);
}
