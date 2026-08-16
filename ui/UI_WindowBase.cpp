/**
 * UI_WindowBase.cpp — UI_WindowBase implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "UI_WindowBase.h"
#include "../graphics/LOCOBITMAP.h"
#include "../platform/ddraw_interfaces.h"   /* IDirectDrawSurface4 — for BeginPaint's GetDC() */
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
extern void  __thiscall FormatResourceString(void* resmgr, int string_id,
                                             char* out_buf, int max_len);  /* 0x447330 */
extern void  WIN32_FatalError(void);
extern void  ExitProcess(UINT);
extern void  Sleep(DWORD);

/* Real def: ui/UIPANEL_Surface.cpp, 105+ callers across the tree — see
 * that file's own doc comment for why this scalar-argument view is
 * correct despite the original's by-value-RECT-shaped stack layout. */
extern bool __thiscall UIPANEL_Blit(void* renderer, uint32_t src_x, uint32_t src_y,
    int32_t dest_x, uint32_t dest_y, void* dest_surface, uint32_t clip_x, uint32_t clip_y,
    int32_t clip_w, uint32_t clip_h, uint32_t flags);                        /* 0x42B050 */

/* CRT/Windows RECT helpers (stubs/windows.h) */
extern "C" {
BOOL IntersectRect(RECT*, const RECT*, const RECT*);
BOOL UnionRect(RECT*, const RECT*, const RECT*);
}

/* DirectDraw globals — void* to avoid IDirectDraw4 dependency at the
 * declaration site (established pattern, see ui/UIPANEL.cpp). */
extern void* g_primary_surface;                      /* 0x4FD3C4 */
extern void* g_backbuffer;                           /* 0x4FD3C0 */

/* ================================================================== */
/* Global state                                                        */
/* ================================================================== */

class ResourceManager;
extern ResourceManager g_resmgr;    /* 0x4855E8 — object, not a pointer (was void*,
                                      * a widespread cross-TU landmine — see
                                      * PROGRESS.md's g_resmgr sweep) */
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
    this->cursorBackSurface = nullptr;                  /* +0x48 */
    this->childCount1  = 0;                             /* +0x68 */
    this->childCount0  = 0;                             /* +0x60 */
    this->childObj2    = NULL;                          /* +0x74 */
    this->captureFlag  = 0;                             /* +0x3C */
    this->renderSurface = nullptr;                      /* +0x14 */
    this->frameCount   = 0;                             /* +0x20 */
    this->tileWidth    = 0;                             /* +0x18 */
    this->tileHeight   = 0;                             /* +0x1C */
    this->currentFrame = 0;                             /* +0x24 */
    this->field_3D     = 0;                             /* +0x3D */
    this->field_40     = 0;                             /* +0x40 */
    this->timerId      = 0;                             /* +0x28 */
    this->originX      = 0;                             /* +0x2C */
    this->originY      = 0;                             /* +0x30 */
    this->windowCreated = 0;                            /* +0xAB */

    this->resourceId   = resId;                         /* +0x10 */

    this->dirtyRect    = RECT{0, 0, 0, 0};               /* +0x50..+0x5C */

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

    /* If this window holds a reference to the shared cursor backbuffer,
     * decrement the (separate) global refcount and free the shared
     * surface once the last owner releases it. */
    if (this->cursorBackSurface != nullptr) {            /* +0x48 */
        g_cursor_refcount--;
        if (g_cursor_refcount == 0 && g_cursor_back != NULL) {
            release_child_object(g_cursor_back);
            g_cursor_back    = NULL;
            g_cursor_refcount = 0;
        }
        this->cursorBackSurface = nullptr;                /* +0x48 */
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
    this->Render(true);
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
    if (this->renderSurface == surface) {
        if (surface == nullptr) return;
    } else {
        this->renderSurface = surface;
        this->frameCount = static_cast<int32_t>(frame_divisor);
        this->currentFrame = 0;
        if (origin == nullptr) {
            this->originX = 0;
            this->originY = 0;
        } else {
            this->originX = origin->x;
            this->originY = origin->y;
        }

        if (surface == nullptr) {
            this->tileWidth = 0;
            this->tileHeight = 0;
        } else if (frame_divisor == 0) {
            this->tileWidth = surface->width;
            this->tileHeight = surface->height;
        } else {
            // 0x426079 uses DIV (unsigned), so retain unsigned division.
            this->tileWidth = static_cast<int32_t>(
                static_cast<uint32_t>(surface->width) / frame_divisor);
            this->tileHeight = surface->height;
        }
    }

    if (reset_dirty_rect != 0) {
        this->dirtyRect = RECT{0, 0, 0, 0};
    }
    if (force_redraw != 0 && this->captureFlag == 0) {
        DDRAW_UnlockPrimary();
        this->Render(reset_dirty_rect == 0);
        DDRAW_UnlockPrimary();
    }

    if (this->renderSurface != nullptr) {
        KillTimer(this->hWnd, this->timerId);
        // Original: `this->field_14 == this->childCount2 ? 0x32 : 0x78` —
        // comparing renderSurface's raw pointer value against childCount2
        // (an unrelated small int/flag at the same base-class offset used
        // by subclasses that store a THIRD child object pair there instead
        // of calling set_render_surface). A real heap-allocated surface
        // pointer can never numerically equal a tiny counter value (this
        // block is only reached with renderSurface != nullptr, so the
        // comparison could only be true if a live heap pointer's value
        // happened to equal childCount2's small int — never happens in
        // practice, on the original 32-bit binary either). Confirmed-dead:
        // the 0x32 branch is REMOVED here, not merely "simplified" — this
        // always evaluates to the 0x78 (120ms) arm (CLAUDE.md: "simplify
        // assembly-shaped expressions whenever behavioral equivalence is
        // proven").
        const UINT interval = 0x78;
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
 *     (the periodic UI tick started by show()) AND renderSurface != nullptr
 *     (a render surface is configured) AND captureFlag == 0 (not mid-drag)
 *     AND field_3D == 0 (idle animation not yet expired). The fast path
 *     samples GetCursorPos, advances the currentFrame animation-tick counter
 *     (wrapping at frameCount, decrementing the field_40 idle-cycle
 *     countdown on each wrap and latching field_3D once field_40 reaches 0),
 *     and re-renders the panel (bracketed by DDRAW_UnlockPrimary()) if the
 *     cursor has not moved since the last tick (lastCursorX/lastCursorY),
 *     then always updates lastCursorX/lastCursorY and returns 0 WITHOUT
 *     dispatching to on_timer(). If the fast-path condition is false, falls
 *     through to the normal on_timer() virtual dispatch.
 *
 *   WM_CAPTURECHANGED (0x215): NOT in the virtual-dispatch table at all —
 *     always returns 0 directly. No-ops when renderSurface == nullptr (no
 *     surface configured), when lParam already equals this window's own HWND (we
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
        if (wParam == 0x43 && this->renderSurface != nullptr &&
            this->captureFlag == 0 && this->field_3D == 0) {
            POINT cursorPos;
            GetCursorPos(&cursorPos);

            if (this->frameCount < 2) {
                return 0;
            }

            int32_t tickCounter = this->currentFrame;
            this->currentFrame = tickCounter + 1;
            if (this->frameCount <= tickCounter + 1) {
                this->currentFrame = 0;
                if (this->field_40 != 0) {
                    this->field_40 -= 1;
                    if (this->field_40 == 0) {
                        this->field_3D = 1;
                    }
                }
            }

            if (cursorPos.x == this->lastCursorX && cursorPos.y == this->lastCursorY) {
                DDRAW_UnlockPrimary();
                this->Render(true);
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
                this->Render(true);
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
            this->Render(true);
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
        this->Render(true);
        DDRAW_UnlockPrimary();
        return hitResult;
    }

    case 0x215: {  // WM_CAPTURECHANGED — not a virtual slot, always returns 0
        if (this->renderSurface == nullptr) {
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
        this->Render(true);
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
        this->Render(true);
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
    self->Render(true);
#else
    // Render() follows the packed x86 UIPANEL layout via renderSurface,
    // which is never wired to a real surface on the host build; the SDL
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

/* ================================================================== */
/* UI_WindowBase::BeginPaint                                           */
/* Address: 0x426B00                                                   */
/*                                                                     */
/* Moved here 2026-08-16 from a stale "UIPANEL_BeginPaint" transcription */
/* in ui/UIPANEL.cpp (see the correction note in that file and           */
/* PROGRESS.md's 2026-08-16 entry for the full get_xrefs_to/address-      */
/* block evidence that this is a UI_WindowBase member, not UIPANEL's).   */
/* The implementation logic below is unchanged from that prior,          */
/* already-verified integration pass:                                    */
/*                                                                       */
/* Begins buffered painting to the primary surface: unlocks the primary  */
/* surface, then calls IDirectDrawSurface4::GetDC() to obtain a GDI      */
/* device context for the sprites/text drawn by callers such as          */
/* BuildingPanel::draw_item, GameSetupPanel::drawGrid/drawTitle/          */
/* drawLayoutList, Cursor's color-bar/scroll-button drawing, and         */
/* NameEntryPanel::on_timer. Retries up to 1000 times at 10ms apiece      */
/* while GetDC keeps failing, then calls WIN32_FatalError()+             */
/* ExitProcess(1) — a deliberate original fatal path, not something to   */
/* soften.                                                                */
/*                                                                       */
/* Evidence for the vtable slot: the original x86 dispatches through     */
/* vtable+0x44 (slot 17), which matches IDirectDrawSurface4::GetDC's     */
/* real COM ABI position (IUnknown's 3 slots, then                       */
/* AddAttachedSurface/AddOverlayDirtyRect/Blt/BltBatch/BltFast/           */
/* DeleteAttachedSurface/EnumAttachedSurfaces/EnumOverlayZOrders/Flip/    */
/* GetAttachedSurface/GetBltStatus/GetCaps/GetClipper/GetColorKey/GetDC   */
/* = slot 17 = byte offset 0x44). platform/ddraw_interfaces.h's own       */
/* C++ declaration order is NOT ABI-accurate (it's grouped by topic, not  */
/* COM slot order) — but that no longer matters here, since this method   */
/* dispatches via an ordinary typed virtual call (surface->GetDC(&hdc)),  */
/* which the compiler slots however it likes for this rebuild.            */
/*                                                                       */
/* The original also spills a stack argument (this->hWnd, pushed by every */
/* caller) across the DDRAW_UnlockPrimary() call for register             */
/* preservation, then discards it — confirmed dead by decompiling         */
/* DDRAW_UnlockPrimary (0x45B940): it is void(void) and never reads any   */
/* incoming parameter. Not reproduced here.                               */
/*                                                                       */
/* The original also reuses a raw x86 scratch slot at this+0x4C (this     */
/* Entity-derived object's world_x storage — see core/GameObject.h) as    */
/* the GetDC() out-parameter buffer. That storage has moved in the host   */
/* layout (world_x is no longer at +0x4C here), so a plain local variable */
/* is used instead; this is a scratch-buffer implementation detail, not   */
/* part of BeginPaint's observable behavior.                             */
/*                                                                       */
/* NOTE: g_primary_surface is still unwired (null) and                   */
/* Sdl3DirectDrawSurface::GetDC is a permanent no-op returning failure    */
/* (see platform/ddraw_interfaces.h's Phase 3 note and PROGRESS.md's      */
/* 2026-08-14 DirectDraw-shim entry) — so this retry-then-ExitProcess(1)  */
/* path is a live self-destruct hazard the moment g_primary_surface is    */
/* wired to a real surface, independent of this integration pass. Wiring  */
/* g_primary_surface and implementing a real GetDC are explicitly out of  */
/* scope for this change.                                                 */
/* ================================================================== */
HDC UI_WindowBase::BeginPaint()
{
    DDRAW_UnlockPrimary();

    auto* surface = static_cast<IDirectDrawSurface4*>(g_primary_surface);

    HDC hdc = nullptr;
    HRESULT hr = surface->GetDC(&hdc);

    int retry = 0;
    while (hr != 0) {
        ++retry;
        if (retry > 1000) {
            WIN32_FatalError();
            ExitProcess(1);
        }
        Sleep(10);
        hr = surface->GetDC(&hdc);
    }

    return hdc;
}

/* Legacy free-function compatibility shim — ~9 files in the tree still
 * call this as a free function with a mix of extern signatures (some
 * correct, some pre-existing call-0-class landmines fixed separately —
 * see docs/landmine-sweep-worklist.md). Retargeted 2026-08-16 to
 * UI_WindowBase pointer (was incorrectly UIPANEL pointer — every real
 * caller passes a GameSetupPanel, Cursor, NameEntryPanel, BuildingPanel,
 * etc. receiver, never a UIPANEL instance, so the prior cast was UB that
 * happened not to crash only because the method body never touched
 * "this"). */
HDC __fastcall UIPANEL_BeginPaint(void* self)
{
    return static_cast<UI_WindowBase*>(self)->BeginPaint();
}

/* ================================================================== */
/* UI_WindowBase::EndPaintEx                                           */
/* Address: 0x426B90 — see the doc comment in UI_WindowBase.h for the   */
/* full evidence trail (ReleaseDC slot, backwards-Blt correction, field  */
/* identities). Integrated 2026-08-16 from a stale "UIPANEL_EndPaintEx" */
/* free-function transcription in ui/UIPANEL.cpp.                       */
/* ================================================================== */
void UI_WindowBase::EndPaintEx(HDC hdc, bool unlockOnly, RECT* restrictRect)
{
    if (hdc != nullptr) {
        static_cast<IDirectDrawSurface4*>(g_primary_surface)->ReleaseDC(hdc);
    }

    if (unlockOnly) {
        DDRAW_UnlockPrimary();
        return;
    }

    DDRAW_UnlockPrimary();

    // Path A: no active render surface, mid-drag, or (host-safety addition,
    // not part of the original's own branch condition) no cursor backbuffer
    // wired up — plain present. cursorBackSurface is permanently nullptr on
    // the host build today (Cursor_SetupSurface never runs there — see that
    // field's doc comment), so this third clause is what actually fires on
    // host; the first two reproduce the original's real decision exactly.
    // NOTE: Path B (below) is the only place lastCursorX/lastCursorY get
    // updated. Folding the host-only cursorBackSurface==nullptr check into
    // this branch means those two fields are never refreshed while it's
    // null — harmless today since renderSurface is also always null on
    // host (Path A already fires from the first clause alone), but worth
    // flagging for whoever eventually wires a real cursorBackSurface: at
    // that point this folded clause would start mattering on its own.
    if (this->renderSurface == nullptr || this->captureFlag != 0 ||
        this->cursorBackSurface == nullptr) {
        RECT* presentRect = (restrictRect != nullptr) ? restrictRect : &this->workRect;
        DDRAW_PresentRect(presentRect, this->hWnd, nullptr, 1);
        DDRAW_UnlockPrimary();
        return;
    }

    // Path B: cursor-relative tile-content present.
    POINT cursor;
    GetCursorPos(&cursor);
    this->lastCursorX = cursor.x;
    this->lastCursorY = cursor.y;

    const int cursorRelX = cursor.x - this->originX;
    const int cursorRelY = cursor.y - this->originY;

    int tileW = this->tileWidth;
    int tileH = this->tileHeight;

    RECT dirty;
    dirty.left   = cursorRelX;
    dirty.top    = cursorRelY;
    dirty.right  = tileW + cursorRelX;
    dirty.bottom = tileH + cursorRelY;

    int dx = 0;
    int dy = 0;
    if (dirty.right > this->workRect.right) {
        tileW = this->workRect.right - cursorRelX;
        dirty.right = this->workRect.right;
    }
    if (dirty.bottom > this->workRect.bottom) {
        tileH = this->workRect.bottom - cursorRelY;
        dirty.bottom = this->workRect.bottom;
    }
    if (cursorRelY < this->workRect.top) {
        dy = this->workRect.top - cursorRelY;
        tileH = dirty.bottom - this->workRect.top;
        dirty.top = this->workRect.top;
    }
    if (cursorRelX < this->workRect.left) {
        dx = this->workRect.left - cursorRelX;
        tileW = dirty.right - this->workRect.left;
        dirty.left = this->workRect.left;
    }

    int scrollOffset = 0;
    if (this->frameCount >= 2) {
        if (this->currentFrame >= this->frameCount) {
            this->currentFrame = 0;
        }
        scrollOffset = this->tileWidth * this->currentFrame;
    }

    // restrictRect handling: if neither the current dirty rect nor the
    // previously-cached one (still holding LAST frame's value at this
    // point — this->dirtyRect is not overwritten until after this block)
    // intersects restrictRect, just present restrictRect directly and
    // skip the tile-content pipeline entirely.
    RECT unionForPresent{};
    bool hasUnionForPresent = false;
    if (restrictRect != nullptr) {
        RECT tmp;
        const bool intersectsDirty  = IntersectRect(&tmp, restrictRect, &dirty) != 0;
        const bool intersectsCached = IntersectRect(&tmp, restrictRect, &this->dirtyRect) != 0;
        if (!intersectsDirty && !intersectsCached) {
            DDRAW_PresentRect(restrictRect, this->hWnd, nullptr, 1);
            DDRAW_UnlockPrimary();
            return;
        }

        UnionRect(&unionForPresent, &dirty, restrictRect);
        // Original additionally unions that result with the OLD cached
        // this->dirtyRect when one existed (0x426D7D-D91), before this
        // function overwrites it below. Reproduced only for that
        // well-defined case (a previous cached rect existed); when none
        // existed, the original's present-rect computation reads
        // uninitialized stack memory in this exact narrow combination —
        // not reproduced (unionForPresent above is a safe, well-defined
        // superset instead). Both this expression and the original's
        // uninitialized-read case are unreachable on host today
        // (cursorBackSurface is always nullptr, see the Path A check
        // above), so this is a documentation-only distinction.
        if (this->dirtyRect.right != 0) {
            RECT withCached = unionForPresent;
            UnionRect(&unionForPresent, &withCached, &this->dirtyRect);
        }
        hasUnionForPresent = true;
    }

    this->dirtyRect = dirty;

    // Save the primary's pixels under the dirty rect into cursorBackSurface
    // before drawing tile content. Confirmed via disassembly: the receiver
    // is cursorBackSurface and the source is g_primary_surface — the prior
    // transcription had this backwards ("copy background from offscreen to
    // primary"); it is actually "save background into offscreen."
    RECT saveDestRect{0, 0, tileW, tileH};
    this->cursorBackSurface->Blt(&saveDestRect,
                                  static_cast<IDirectDrawSurface4*>(g_primary_surface),
                                  &dirty, DDBLT_WAIT, nullptr);

    UIPANEL_Blit(this->renderSurface, dirty.left, dirty.top, dirty.right, dirty.bottom,
                 g_primary_surface,
                 scrollOffset + dx, dy,
                 tileW + scrollOffset + dx, tileH + dy,
                 0);

    if (!hasUnionForPresent) {
        DDRAW_PresentRect(&this->workRect, this->hWnd, nullptr, 1);
    } else {
        DDRAW_PresentRect(&unionForPresent, this->hWnd, nullptr, 1);
    }

    // Restore: copy cursorBackSurface's saved pixels back onto the primary,
    // erasing the tile content just presented so the next frame's
    // save/draw/present cycle starts from a clean background.
    static_cast<IDirectDrawSurface4*>(g_primary_surface)->Blt(
        &dirty, this->cursorBackSurface, &saveDestRect, DDBLT_WAIT, nullptr);

    DDRAW_UnlockPrimary();
}

/* ================================================================== */
/* UI_WindowBase::EndPaint                                             */
/* Address: 0x426B70                                                    */
/* ================================================================== */
void UI_WindowBase::EndPaint()
{
    // BUG (original): the original UIPANEL_EndPaint wrapper passes the
    // address of an uninitialized stack RECT as restrictRect, whose
    // contents are undefined-behavior garbage (not a reproducible value).
    //
    // Correction 2026-08-16 (advisor review caught this): EndPaintEx's
    // Path A branches on `restrictRect != nullptr`, choosing `restrictRect`
    // itself over `&this->workRect` whenever it is non-null:
    //   RECT* presentRect = (restrictRect != nullptr) ? restrictRect
    //                                                  : &this->workRect;
    // An earlier pass here substituted a *zeroed* local RECT for the
    // original's uninitialized one — but a zeroed, non-null RECT is still
    // non-null, so it took the SAME branch as the original (present
    // `*restrictRect`, not workRect) and turned "presents undefined
    // garbage" into "deterministically presents an empty 0x0 rect" —
    // a different observable outcome (a no-op present) from either the
    // original's garbage-driven present *or* from presenting the window's
    // real work area.
    //
    // Passing nullptr instead makes Path A select `&this->workRect` — a
    // well-defined, reproducible full-window present. This is a deliberate
    // deviation from the original's undefined behavior (there is no
    // "correct" garbage value to reproduce), on the reasoning that a plain
    // EndPaint() call (as opposed to callers that pass a real, meaningful
    // restrictRect to EndPaintEx() directly) most plausibly intends "repaint
    // whatever's dirty," for which workRect is the sane default — matching
    // every other EndPaintEx() caller in this codebase that omits a
    // restrictRect. Every real EndPaint() caller (as opposed to EndPaintEx()
    // directly) does so specifically when no render surface is configured,
    // so this only affects Path A's DDRAW_PresentRect argument, not any
    // tile-content logic.
    this->EndPaintEx(nullptr, false, nullptr);
}

/* ================================================================== */
/* UI_WindowBase::Render                                               */
/* Address: 0x426EB0 — see the doc comment in UI_WindowBase.h for the   */
/* full evidence trail. Integrated 2026-08-16 from a stale               */
/* "UIPANEL_Render" free-function transcription in ui/UIPANEL.cpp.      */
/* ================================================================== */
void UI_WindowBase::Render(bool enableTileMap)
{
    if (this->activeFlag == 0) {
        return;
    }

    POINT cursor;
    GetCursorPos(&cursor);
    const int cursorX = cursor.x - this->originX;
    const int cursorY = cursor.y - this->originY;

    int tileW = this->tileWidth;
    int tileH = this->tileHeight;

    RECT dirty;
    dirty.left   = cursorX;
    dirty.top    = cursorY;
    dirty.right  = tileW + cursorX;
    dirty.bottom = tileH + cursorY;

    int dx = 0;
    int dy = 0;
    if (dirty.right > this->workRect.right) {
        tileW = this->workRect.right - cursorX;
        dirty.right = this->workRect.right;
    }
    if (dirty.bottom > this->workRect.bottom) {
        tileH = this->workRect.bottom - cursorY;
        dirty.bottom = this->workRect.bottom;
    }
    if (cursorY < this->workRect.top) {
        dy = this->workRect.top - cursorY;
        tileH = dirty.bottom - this->workRect.top;
        dirty.top = this->workRect.top;
    }
    if (cursorX < this->workRect.left) {
        dx = this->workRect.left - cursorX;
        tileW = dirty.right - this->workRect.left;
        dirty.left = this->workRect.left;
    }

    const bool hadCachedDirtyRect = (this->dirtyRect.right != 0);
    bool inflate = false;
    RECT inflated{};
    if (this->renderSurface != nullptr && hadCachedDirtyRect && enableTileMap &&
        this->captureFlag == 0) {
        RECT unioned;
        UnionRect(&unioned, &this->dirtyRect, &dirty);
        if ((unioned.right - unioned.left) < 0x100 && (unioned.bottom - unioned.top) < 0x100) {
            inflate = true;
            inflated = unioned;
            inflated.left   -= 4;
            inflated.top    -= 4;
            inflated.right  += 4;
            inflated.bottom += 4;
            if (inflated.right  > this->workRect.right)  inflated.right  = this->workRect.right;
            if (inflated.bottom > this->workRect.bottom) inflated.bottom = this->workRect.bottom;
            if (inflated.top    < this->workRect.top)    inflated.top    = this->workRect.top;
            if (inflated.left   < this->workRect.left)   inflated.left   = this->workRect.left;
        }
    }

    if (hadCachedDirtyRect && enableTileMap && !inflate) {
        // Save the primary's pixels for the cached dirty rect into
        // g_backbuffer. Confirmed via disassembly: receiver is
        // g_backbuffer, source is g_primary_surface — the prior
        // transcription had this backwards ("restore background from
        // backbuffer"); it is actually "save background into backbuffer."
        static_cast<IDirectDrawSurface4*>(g_backbuffer)->Blt(
            &this->dirtyRect, static_cast<IDirectDrawSurface4*>(g_primary_surface),
            &this->dirtyRect, DDBLT_WAIT, nullptr);
    }

    this->lastCursorX = -1;
    this->lastCursorY = -1;

    if (this->renderSurface == nullptr || this->captureFlag != 0) {
        return;
    }

    this->dirtyRect = dirty;

    if (this->cursorBackSurface == nullptr) {
        // Host-safety fallback (not part of the original's own branch
        // condition): cursorBackSurface is permanently nullptr on the host
        // build today (Cursor_SetupSurface never runs there — see that
        // field's doc comment) — skip the tile-content blit rather than
        // dereference a null surface.
        return;
    }

    int scrollOffset = 0;
    if (this->frameCount >= 2) {
        if (this->currentFrame >= this->frameCount) {
            this->currentFrame = 0;
        }
        scrollOffset = this->currentFrame * this->tileWidth;
    }

    if (inflate) {
        // Path A: inflated dirty rect (smooth-cursor small-region case).
        const int width  = inflated.right  - inflated.left;
        const int height = inflated.bottom - inflated.top;
        RECT saveDestRect{0, 0, width, height};
        this->cursorBackSurface->Blt(&saveDestRect,
                                      static_cast<IDirectDrawSurface4*>(g_primary_surface),
                                      &inflated, DDBLT_WAIT, nullptr);

        const int srcX = dirty.left - inflated.left;
        const int srcY = dirty.top  - inflated.top;
        UIPANEL_Blit(this->renderSurface, srcX, srcY, srcX + tileW, srcY + tileH,
                     this->cursorBackSurface, scrollOffset + dx, dy,
                     scrollOffset + dx + tileW, dy + tileH, 0);

        static_cast<IDirectDrawSurface4*>(g_backbuffer)->Blt(
            &this->dirtyRect, this->cursorBackSurface, &saveDestRect, DDBLT_WAIT, nullptr);
    } else {
        // Path B: standard (no inflate). Confirmed via full ESP-relative
        // disassembly (0x4271FA-0x4272B9): unlike Path A, the source/dest
        // region passed to UIPANEL_Blit here is NOT derived from `dirty` at
        // all — it is the literal {0, 0, tileW, tileH} rect (the freshly
        // saved surface region always starts at its own origin). An earlier
        // pass wrongly copied Path A's dirty-relative shape here; corrected
        // 2026-08-16 after re-deriving every register (EAX/ECX/EDX/EDI) from
        // 0x427230 through the CALL at 0x4272B9 by hand, since the
        // decompiler's own output for this function is corrupted by
        // unaff_EBX/unaff_EDI register-allocation artifacts.
        RECT saveDestRect{0, 0, tileW, tileH};
        this->cursorBackSurface->Blt(&saveDestRect,
                                      static_cast<IDirectDrawSurface4*>(g_primary_surface),
                                      &dirty, DDBLT_WAIT, nullptr);

        UIPANEL_Blit(this->renderSurface, /*src_x=*/0, /*src_y=*/0,
                     /*dest_x=*/tileW, /*dest_y=*/tileH,
                     this->cursorBackSurface, scrollOffset + dx, dy,
                     tileW + scrollOffset + dx, tileH + dy, 0);

        static_cast<IDirectDrawSurface4*>(g_backbuffer)->Blt(
            &this->dirtyRect, this->cursorBackSurface, &saveDestRect, DDBLT_WAIT, nullptr);
    }
}

/* ==================================================================== */
/* Legacy free-function compatibility shims for EndPaintEx()/EndPaint(). */
/*                                                                       */
/* Blast radius is far larger than the original ~9-file estimate: ~50    */
/* call sites across 15+ files (game/BuildingPanel.cpp, town/Town.cpp,   */
/* ui/GameSetupPanel.cpp, ui/GameSetupPanel_network.cpp,                 */
/* ui/NameEntryPanel.cpp, network/Netman.cpp, network/DPlayManager.cpp,  */
/* network/NetworkPlayerList.cpp, native/NETMAN_SessionSettings.c,       */
/* native/NETMAN_NetworkUI.c, graphics/LOCOBITMAP.cpp, and several       */
/* shared/stubs_link001_batch*.cpp files), all already consistently      */
/* declaring/calling the SAME real signature:                            */
/*   void UIPANEL_EndPaintEx(void* self, int32_t hdc, int32_t            */
/*                           unlockParam, uint8_t unlockFlag,            */
/*                           RECT* restrictRect);                        */
/* with the 2nd positional arg always a `static_cast<int32_t>(            */
/* reinterpret_cast<intptr_t>(this->hWnd))` (the dead value — matches     */
/* this method's dropped first parameter) and the 3rd positional arg      */
/* always the real HDC (0 when none). Kept as thin shims here rather      */
/* than touching every call site, per this task's explicit "leave a thin  */
/* compatibility shim if the blast radius is too large" guidance —        */
/* mirrors UIPANEL_BeginPaint's precedent above.                          */
/* ==================================================================== */
void UIPANEL_EndPaintEx(void* self, int32_t /* deadHwnd */, int32_t hdc,
                        uint8_t unlockFlag, RECT* restrictRect)
{
    // ABI_BOUNDARY: opaque OS HDC round-tripped through the original
    // function's int parameter (matches the established pattern at every
    // real call site above for HWND).
    HDC realHdc = reinterpret_cast<HDC>(static_cast<intptr_t>(hdc));
    static_cast<UI_WindowBase*>(self)->EndPaintEx(realHdc, unlockFlag != 0, restrictRect);
}

void UIPANEL_EndPaint(void* self)
{
    static_cast<UI_WindowBase*>(self)->EndPaint();
}
