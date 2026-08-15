/**
 * sdl3_window.cpp — Win32 windowing → SDL3 shim implementation
 *
 * Implements the critical Win32 USER32/GDI32 APIs on top of SDL3.
 * This is the bridge that allows the decompiled Lego Loco code
 * (which expects Win32 windowing) to run on Linux via SDL3.
 *
 * NOT part of the Lego Loco reverse-engineering project.
 */

#include "sdl3_window.h"
#include <SDL3/SDL.h>
#include "sdl3_game_audio.h"
#include "sdl3_ddraw.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <map>
#include <string>
#include <vector>

#ifndef _WIN32

/* =========================================================================
 * Internal state
 * ========================================================================= */

static SDL_Window*   g_sdl_window   = nullptr;
static SDL_Renderer* g_sdl_renderer = nullptr;
static bool          g_sdl_initialized = false;

/* Message queue for PeekMessage/GetMessage */
struct QueuedMsg {
    MSG msg;
};
static std::vector<QueuedMsg> g_msg_queue;

/* Timer tracking */
struct TimerInfo {
    HWND       hwnd;
    uintptr_t  id;
    UINT       elapse;
    TIMERPROC  callback;
    SDL_TimerID sdl_timer_id;
};
static std::map<uintptr_t, TimerInfo> g_timers;

/* Window class registry — maps class name to WNDCLASSA */
static std::map<std::string, WNDCLASSA> g_window_classes;

/* Window objects — each HWND is a pointer to one of these */
struct WindowData {
    std::string  class_name;
    std::string  title;
    void*        wndproc;
    LONG         userdata;
    SDL_Window*  sdl_win;    /* NULL if not a top-level window */
    int          x, y, width, height;
    bool         visible;
};
static std::map<HWND, WindowData*> g_windows;
static uintptr_t g_next_hwnd = 1;   /* HWNDs are opaque pointers */

/* The real main CGWND window's HWND — the first top-level window ever
 * created (CGWND::RegisterWindowClass's CreateWindowExA call, always the
 * very first window construction in the real program order per
 * GameLoop_Setup). Captured explicitly rather than assuming callers know
 * it happens to be HWND(1); every SDL-event-to-WM_* translation below
 * targets this window specifically, since SDL has exactly one real
 * window/event stream to attribute events to. */
static HWND g_main_hwnd = nullptr;

/* =========================================================================
 * SDL timer callback
 * ========================================================================= */

static Uint32 sdl_timer_callback(void* userdata, SDL_TimerID timer_id, Uint32 interval)
{
    (void)timer_id; (void)interval;
    TimerInfo* ti = reinterpret_cast<TimerInfo*>(userdata);
    if (ti && ti->callback) {
        ti->callback(ti->hwnd, WM_TIMER, ti->id, SDL_GetTicks());
        return ti->elapse; /* re-trigger every elapse ms */
    }
    return 0; /* callback was nulled (timer killed); stop re-triggering */
}

/* =========================================================================
 * Message posting (internal)
 * ========================================================================= */

static void post_message(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    QueuedMsg qm = { { hwnd, msg, wParam, lParam, 0, {0, 0} } };
    g_msg_queue.push_back(qm);
}

/* =========================================================================
 * SDL3 lifecycle
 * ========================================================================= */

int SDL3_WindowInit(const char* title, int width, int height)
{
    if (g_sdl_initialized) return 0;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL3_WindowInit: SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    /* Host-only test deviation: under the GUI sandbox (LEGO_LOCO_TEST_EVENTS
     * set -- stubs/host_test_events.h's existing "are we under test" signal),
     * request true fullscreen instead of a decorated window. Sway (like
     * other Wayland compositors) draws its title-bar decoration
     * server-side for ordinary toplevels regardless of the client's
     * SDL_WINDOW_BORDERLESS hint (confirmed empirically -- borderless alone
     * left the bar in place), but gives a fullscreen surface the whole
     * output with no chrome at all. That decoration would otherwise show
     * up in the sandbox's raw "gui-sandbox shot" capture, corrupting pixel
     * comparisons against reference screenshots (tests/reference/*.png)
     * that never have one. A real desktop run keeps the normal decorated,
     * resizable window. */
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE;
    if (std::getenv("LEGO_LOCO_TEST_EVENTS") != nullptr) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }
    g_sdl_window = SDL_CreateWindow(title, width, height, window_flags);
    if (!g_sdl_window) {
        fprintf(stderr, "SDL3_WindowInit: SDL_CreateWindow failed: %s\n", SDL_GetError());
        fprintf(stderr, "[TRACE] Calling SDL_Quit...\n");
    SDL_Quit();
        return -1;
    }

    g_sdl_renderer = SDL_CreateRenderer(g_sdl_window, nullptr);
    if (!g_sdl_renderer) {
        fprintf(stderr, "SDL3_WindowInit: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    g_sdl_initialized = true;
    return 0;
}

void SDL3_WindowQuit(void)
{
    if (!g_sdl_initialized) return;

    /* Kill all timers */
    for (auto& pair : g_timers) {
        SDL_RemoveTimer(pair.second.sdl_timer_id);
        pair.second.callback = nullptr;  /* guard in-flight callbacks */
    }
    g_timers.clear();

    /* Clean up windows */
    for (auto& pair : g_windows) {
        delete pair.second;
    }
    g_windows.clear();

    if (g_sdl_renderer) { SDL_DestroyRenderer(g_sdl_renderer); g_sdl_renderer = nullptr; }
    if (g_sdl_window)   { SDL_DestroyWindow(g_sdl_window);     g_sdl_window   = nullptr; }

    SDL_Quit();
    g_sdl_initialized = false;
}

SDL_Renderer* SDL3_GetRenderer(void) { return g_sdl_renderer; }
SDL_Window*   SDL3_GetWindow(void)   { return g_sdl_window; }

/* =========================================================================
 * Window class registration
 * ========================================================================= */

ATOM RegisterClassA(const WNDCLASSA* lpWndClass)
{
    if (!lpWndClass || !lpWndClass->lpszClassName) return 0;
    g_window_classes[lpWndClass->lpszClassName] = *lpWndClass;
    return 1; /* success */
}

ATOM RegisterClassExA(const WNDCLASSEXA* lpWndClassEx)
{
    if (!lpWndClassEx || !lpWndClassEx->lpszClassName) return 0;
    WNDCLASSA wc;
    wc.style         = lpWndClassEx->style;
    wc.lpfnWndProc   = lpWndClassEx->lpfnWndProc;
    wc.cbClsExtra    = lpWndClassEx->cbClsExtra;
    wc.cbWndExtra    = lpWndClassEx->cbWndExtra;
    wc.hInstance     = lpWndClassEx->hInstance;
    wc.hIcon         = lpWndClassEx->hIcon;
    wc.hCursor       = lpWndClassEx->hCursor;
    wc.hbrBackground = lpWndClassEx->hbrBackground;
    wc.lpszMenuName  = lpWndClassEx->lpszMenuName;
    wc.lpszClassName = lpWndClassEx->lpszClassName;
    return RegisterClassA(&wc);
}

/* =========================================================================
 * Window creation
 * ========================================================================= */

HWND CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
                      DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                      HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
                      void* lpParam)
{
    (void)dwExStyle; (void)hMenu; (void)lpParam; (void)hInstance;

    auto it = g_window_classes.find(lpClassName ? lpClassName : "");
    if (it == g_window_classes.end()) {
        fprintf(stderr, "SDL3: CreateWindowExA: unregistered class '%s'\n",
                lpClassName ? lpClassName : "(null)");
        return nullptr;
    }

    WindowData* wd = new WindowData();
    wd->class_name = lpClassName ? lpClassName : "";
    wd->title      = lpWindowName ? lpWindowName : "";
    wd->wndproc    = reinterpret_cast<void*>(it->second.lpfnWndProc);
    wd->userdata   = 0;
    wd->sdl_win    = nullptr;
    wd->x = X; wd->y = Y; wd->width = nWidth; wd->height = nHeight;
    wd->visible = (dwStyle & WS_VISIBLE) != 0;

    HWND hwnd = reinterpret_cast<HWND>(g_next_hwnd++);

    /* Only the first top-level window gets the real SDL window */
    if (hWndParent == nullptr && g_sdl_window && !g_windows.empty()) {
        /* Child windows are virtual — no SDL window */
        wd->sdl_win = nullptr;
    }

    if (g_windows.empty()) {
        g_main_hwnd = hwnd;   /* the real main CGWND window — see declaration comment */
    }

    g_windows[hwnd] = wd;

    if (wd->visible) {
        ShowWindow(hwnd, SW_SHOW);
    }

    /* Send WM_CREATE */
    post_message(hwnd, 0x0001, 0, 0); /* WM_CREATE */

    return hwnd;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow)
{
    auto it = g_windows.find(hWnd);
    if (it == g_windows.end()) return false;

    WindowData* wd = it->second;
    if (nCmdShow == SW_HIDE) {
        wd->visible = false;
        if (wd->sdl_win) SDL_HideWindow(wd->sdl_win);
    } else {
        wd->visible = true;
        if (wd->sdl_win) SDL_ShowWindow(wd->sdl_win);
    }
    /* For the main game window: show the SDL window */
    if (g_sdl_window && (nCmdShow != SW_HIDE)) {
        SDL_ShowWindow(g_sdl_window);
    }
    return true;
}

BOOL UpdateWindow(HWND hWnd)
{
    auto it = g_windows.find(hWnd);
    if (it == g_windows.end()) return false;
    /* Invalidate to trigger WM_PAINT */
    InvalidateRect(hWnd, nullptr, false);
    return true;
}

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y,
                   int cx, int cy, UINT uFlags)
{
    (void)hWndInsertAfter;
    auto it = g_windows.find(hWnd);
    if (it == g_windows.end()) return false;

    WindowData* wd = it->second;
    if (!(uFlags & SWP_NOMOVE)) { wd->x = X; wd->y = Y; }
    if (!(uFlags & SWP_NOSIZE)) { wd->width = cx; wd->height = cy; }

    if (g_sdl_window && hWnd == g_main_hwnd) {
        SDL_SetWindowSize(g_sdl_window, wd->width, wd->height);
        SDL_SetWindowPosition(g_sdl_window, wd->x, wd->y);
    }
    return true;
}

BOOL GetClientRect(HWND hWnd, RECT* lpRect)
{
    auto it = g_windows.find(hWnd);
    if (it == g_windows.end() || !lpRect) return false;

    WindowData* wd = it->second;
    lpRect->left   = 0;
    lpRect->top    = 0;
    lpRect->right  = wd->width;
    lpRect->bottom = wd->height;
    return true;
}

int GetSystemMetrics(int nIndex)
{
    /* Use SDL3 to get actual display bounds */
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);

    switch (nIndex) {
        case SM_CXSCREEN:
        case SM_CXFULLSCREEN:
            return mode ? mode->w : 800;
        case SM_CYSCREEN:
        case SM_CYFULLSCREEN:
            return mode ? mode->h : 600;
        default:
            return 0;
    }
}

BOOL EnableWindow(HWND hWnd, BOOL bEnable)
{
    (void)hWnd; (void)bEnable;
    return true;
}

BOOL SetForegroundWindow(HWND hWnd)
{
    auto it = g_windows.find(hWnd);
    if (it != g_windows.end() && g_sdl_window) {
        SDL_RaiseWindow(g_sdl_window);
    }
    return true;
}

BOOL InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase)
{
    (void)lpRect; (void)bErase;
    /* Post WM_PAINT to trigger redraw */
    post_message(hWnd, WM_PAINT, 0, 0);
    return true;
}

BOOL SetWindowTextA(HWND hWnd, LPCSTR lpString)
{
    auto it = g_windows.find(hWnd);
    if (it == g_windows.end()) return false;
    it->second->title = lpString ? lpString : "";
    if (g_sdl_window) SDL_SetWindowTitle(g_sdl_window, it->second->title.c_str());
    return true;
}

HWND GetDesktopWindow(void)
{
    return g_main_hwnd; /* return first window as "desktop" */
}

HWND FindWindowA(LPCSTR lpClassName, LPCSTR lpWindowName)
{
    (void)lpClassName;
    /* Lego Loco uses this to prevent multiple instances.
     * For single-player, always report not found. */
    if (lpWindowName) {
        for (auto& pair : g_windows) {
            if (pair.second->title == lpWindowName) {
                return pair.first;
            }
        }
    }
    return nullptr;
}

LONG SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
{
    auto it = g_windows.find(hWnd);
    if (it == g_windows.end()) return 0;

    switch (nIndex) {
        case GWL_USERDATA: {
            LONG old = it->second->userdata;
            it->second->userdata = dwNewLong;
            return old;
        }
        default:
            return 0;
    }
}

LONG GetWindowLongA(HWND hWnd, int nIndex)
{
    auto it = g_windows.find(hWnd);
    if (it == g_windows.end()) return 0;

    switch (nIndex) {
        case GWL_USERDATA: return it->second->userdata;
        default: return 0;
    }
}

/* Scroll bar stubs */
BOOL SetScrollRange(HWND hWnd, int nBar, int nMinPos, int nMaxPos, BOOL bRedraw)
{ (void)hWnd; (void)nBar; (void)nMinPos; (void)nMaxPos; (void)bRedraw; return true; }

int SetScrollPos(HWND hWnd, int nBar, int nPos, BOOL bRedraw)
{ (void)hWnd; (void)nBar; (void)nPos; (void)bRedraw; return 0; }

BOOL ShowScrollBar(HWND hWnd, int wBar, BOOL bShow)
{ (void)hWnd; (void)wBar; (void)bShow; return true; }

BOOL AdjustWindowRect(RECT* lpRect, DWORD dwStyle, BOOL bMenu)
{ (void)lpRect; (void)dwStyle; (void)bMenu; return true; }

/* =========================================================================
 * Message loop
 * ========================================================================= */

/* Packs an already-projected canvas position the same way Win32's real
 * MAKELPARAM does (X in the low word, Y in the high word), matching
 * core/CGWND_sdl3.cpp's own host_pack_game_lparam{,_clamped} helpers —
 * each component truncated to 16 bits *before* combining, not just OR'd
 * in raw (a wide X could otherwise bleed into Y's bits). */
static LPARAM pack_lparam_xy(float canvas_x, float canvas_y)
{
    const uint16_t x = static_cast<uint16_t>(static_cast<int32_t>(canvas_x));
    const uint16_t y = static_cast<uint16_t>(static_cast<int32_t>(canvas_y));
    return static_cast<LPARAM>((static_cast<uint32_t>(y) << 16) | x);
}

/* Translates one raw SDL event into the matching WM_* message(s) and posts
 * it to the real main window's queue. Shared by PeekMessageA and
 * GetMessageA so a message picked up by SDL_WaitEvent (GetMessageA) gets
 * the same treatment as one picked up by SDL_PollEvent (PeekMessageA)
 * instead of being silently discarded.
 *
 * Mouse coordinates are projected through the same logical-canvas mapping
 * CGWND_sdl3.cpp's host input dispatch already uses, not raw SDL window
 * coordinates — WM_MOUSEMOVE uses the strict (rejecting) projection since
 * the original only generates that message while the pointer is actually
 * over the client area; button transitions use the clamped projection
 * since Win32 mouse capture can deliver an out-of-client-rect button
 * message unconditionally (see SDL3_DisplayToPrimaryCanvasClamped's own
 * doc comment). */
static void translate_and_post_sdl_event(const SDL_Event& sdl_ev)
{
    switch (sdl_ev.type) {
    case SDL_EVENT_QUIT:
        post_message(g_main_hwnd, WM_CLOSE, 0, 0);
        break;
    case SDL_EVENT_KEY_DOWN:
        post_message(g_main_hwnd, WM_KEYDOWN, sdl_ev.key.key, 0);
        break;
    case SDL_EVENT_KEY_UP:
        post_message(g_main_hwnd, WM_KEYUP, sdl_ev.key.key, 0);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        UINT msg = sdl_ev.button.button == SDL_BUTTON_LEFT  ? WM_LBUTTONDOWN :
                   sdl_ev.button.button == SDL_BUTTON_RIGHT ? WM_RBUTTONDOWN : 0;
        float cx = 0.0f, cy = 0.0f;
        if (msg != 0 &&
            SDL3_DisplayToPrimaryCanvasClamped(sdl_ev.button.x, sdl_ev.button.y, &cx, &cy)) {
            post_message(g_main_hwnd, msg, 0, pack_lparam_xy(cx, cy));
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        UINT msg = sdl_ev.button.button == SDL_BUTTON_LEFT  ? WM_LBUTTONUP :
                   sdl_ev.button.button == SDL_BUTTON_RIGHT ? WM_RBUTTONUP : 0;
        float cx = 0.0f, cy = 0.0f;
        if (msg != 0 &&
            SDL3_DisplayToPrimaryCanvasClamped(sdl_ev.button.x, sdl_ev.button.y, &cx, &cy)) {
            post_message(g_main_hwnd, msg, 0, pack_lparam_xy(cx, cy));
        }
        break;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        float cx = 0.0f, cy = 0.0f;
        if (SDL3_DisplayToPrimaryCanvas(sdl_ev.motion.x, sdl_ev.motion.y, &cx, &cy)) {
            post_message(g_main_hwnd, WM_MOUSEMOVE, 0, pack_lparam_xy(cx, cy));
        }
        break;
    }
    default:
        break;
    }
}

BOOL PeekMessageA(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin,
                   UINT wMsgFilterMax, UINT wRemoveMsg)
{
    (void)hWnd; (void)wMsgFilterMin; (void)wMsgFilterMax;

    /* Pump SDL events into our queue */
    SDL_Event sdl_ev;
    while (SDL_PollEvent(&sdl_ev)) {
        translate_and_post_sdl_event(sdl_ev);
    }

    if (g_msg_queue.empty()) return false;

    *lpMsg = g_msg_queue.front().msg;
    if (wRemoveMsg & PM_REMOVE) {
        g_msg_queue.erase(g_msg_queue.begin());
    }
    return true;
}

BOOL GetMessageA(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
    (void)hWnd; (void)wMsgFilterMin; (void)wMsgFilterMax;

    /* Block until a message is available */
    while (g_msg_queue.empty()) {
        SDL_Event sdl_ev;
        if (SDL_WaitEvent(&sdl_ev)) {
            translate_and_post_sdl_event(sdl_ev);
        }
    }

    *lpMsg = g_msg_queue.front().msg;
    g_msg_queue.erase(g_msg_queue.begin());
    if (lpMsg->message == WM_QUIT) return false;
    return true;
}

BOOL TranslateMessage(const MSG* lpMsg)
{
    (void)lpMsg;
    /* No translation needed for SDL3 */
    return true;
}

LONG DispatchMessageA(const MSG* lpMsg)
{
    if (!lpMsg || !lpMsg->hwnd) return 0;

    auto it = g_windows.find(lpMsg->hwnd);
    if (it == g_windows.end()) return DefWindowProcA(lpMsg->hwnd, lpMsg->message,
                                                      lpMsg->wParam, lpMsg->lParam);

    void* wndproc = it->second->wndproc;
    if (!wndproc) return 0;

    /* Call the window procedure: LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM) */
    typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
    return (reinterpret_cast<WNDPROC>(wndproc))(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
}

BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    post_message(hWnd, Msg, wParam, lParam);
    return true;
}

LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd; (void)wParam; (void)lParam;
    switch (Msg) {
        case WM_CLOSE:
            SDL_Event ev;
            ev.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&ev);
            return 0;
        default:
            return 0;
    }
}

/* =========================================================================
 * GDI — stubs (drawing done via DirectDraw/SDL renderer)
 * ========================================================================= */

HDC GetDC(HWND hWnd)
{
    (void)hWnd;
    return reinterpret_cast<HDC>(1); /* non-null sentinel */
}

int ReleaseDC(HWND hWnd, HDC hDC)
{
    (void)hWnd; (void)hDC;
    return 1;
}

HDC BeginPaint(HWND hWnd, PAINTSTRUCT* lpPaint)
{
    (void)hWnd;
    if (lpPaint) {
        memset(lpPaint, 0, sizeof(*lpPaint));
        lpPaint->hdc = reinterpret_cast<HDC>(1);
        auto it = g_windows.find(hWnd);
        if (it != g_windows.end()) {
            lpPaint->rcPaint.right  = it->second->width;
            lpPaint->rcPaint.bottom = it->second->height;
        }
    }
    return reinterpret_cast<HDC>(1);
}

BOOL EndPaint(HWND hWnd, const PAINTSTRUCT* lpPaint)
{
    (void)hWnd; (void)lpPaint;
    return true;
}

BOOL BitBlt(HDC hdc, int x, int y, int cx, int cy,
            HDC hdcSrc, int x1, int y1, DWORD rop)
{
    (void)hdc; (void)x; (void)y; (void)cx; (void)cy;
    (void)hdcSrc; (void)x1; (void)y1; (void)rop;
    return true;
}

BOOL StretchBlt(HDC hdcDest, int xDest, int yDest, int wDest, int hDest,
                HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop)
{
    (void)hdcDest; (void)xDest; (void)yDest; (void)wDest; (void)hDest;
    (void)hdcSrc; (void)xSrc; (void)ySrc; (void)wSrc; (void)hSrc; (void)rop;
    return true;
}

HDC CreateCompatibleDC(HDC hdc)
{
    (void)hdc;
    return reinterpret_cast<HDC>(2);
}

BOOL DeleteDC(HDC hdc)
{
    (void)hdc;
    return true;
}

void* SelectObject(HDC hdc, void* hgdiobj)
{
    (void)hdc;
    return hgdiobj;
}

BOOL DeleteObject(void* ho)
{
    (void)ho;
    return true;
}

HBRUSH CreateSolidBrush(COLORREF color)
{
    (void)color;
    return reinterpret_cast<HBRUSH>(1);
}

COLORREF SetTextColor(HDC hdc, COLORREF color)
{
    (void)hdc;
    return color;
}

int SetBkMode(HDC hdc, int mode)
{
    (void)hdc;
    return mode;
}

int DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, RECT* lprc, UINT format)
{
    (void)hdc; (void)lpchText; (void)cchText; (void)lprc; (void)format;
    /* Text rendering not needed — the game renders text via DirectDraw */
    return 0;
}

int FillRect(HDC hdc, const RECT* lprc, HBRUSH hbr)
{
    (void)hdc; (void)lprc; (void)hbr;
    return 1;
}

void SetRect(RECT* lprc, int left, int top, int right, int bottom)
{
    if (lprc) {
        lprc->left = left;
        lprc->top = top;
        lprc->right = right;
        lprc->bottom = bottom;
    }
}

void SetRectEmpty(RECT* lprc)
{
    if (lprc) memset(lprc, 0, sizeof(*lprc));
}

BOOL PtInRect(const RECT* lprc, POINT pt)
{
    if (!lprc) return false;
    return pt.x >= lprc->left && pt.x < lprc->right &&
           pt.y >= lprc->top  && pt.y < lprc->bottom;
}

/* =========================================================================
 * Cursor
 * ========================================================================= */

HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    (void)hInstance; (void)lpCursorName;
    return reinterpret_cast<HCURSOR>(1);
}

HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName)
{
    (void)hInstance; (void)lpIconName;
    return reinterpret_cast<HICON>(1);
}

void SetCursor(HCURSOR hCursor)
{
    (void)hCursor;
    /* Cursor managed by SDL — show/hide as needed */
}

/* =========================================================================
 * Timers
 * ========================================================================= */

uintptr_t SetTimer(HWND hWnd, uintptr_t nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc)
{
    /* Populate the entry in the map first, so the pointer we pass to
     * SDL_AddTimer points to valid data.  SDL's timer thread may fire
     * the callback before SDL_AddTimer returns for very short intervals. */
    TimerInfo& ti = g_timers[nIDEvent];
    ti.hwnd     = hWnd;
    ti.id       = nIDEvent;
    ti.elapse   = uElapse;
    ti.callback = lpTimerFunc;
    ti.sdl_timer_id = SDL_AddTimer(uElapse, sdl_timer_callback, &ti);

    return nIDEvent;
}

BOOL KillTimer(HWND hWnd, uintptr_t uIDEvent)
{
    (void)hWnd;
    auto it = g_timers.find(uIDEvent);
    if (it != g_timers.end()) {
        SDL_RemoveTimer(it->second.sdl_timer_id);
        /* Null the callback but keep the entry in the map.
         * SDL_RemoveTimer prevents future callbacks, but an in-flight
         * callback may still hold a pointer to this TimerInfo.  If we
         * erase, that callback reads freed memory.  Nulling callback
         * ensures the in-flight callback returns 0 (no re-trigger)
         * instead of calling through a dangling pointer. */
        it->second.callback = nullptr;
    }
    return true;
}

/* =========================================================================
 * MessageBox
 * ========================================================================= */

int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    (void)hWnd; (void)uType;
    fprintf(stderr, "SDL3 MessageBox: [%s] %s\n",
            lpCaption ? lpCaption : "Message", lpText ? lpText : "");
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                              lpCaption ? lpCaption : "Lego Loco",
                              lpText ? lpText : "", nullptr);
    return 1; /* IDOK */
}

/* =========================================================================
 * Modal dialogs with child controls (no host equivalent yet — see the
 * sdl3_window.h doc comment above these declarations)
 * ========================================================================= */

int32_t DialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName,
                         HWND hWndParent, void* lpDialogFunc, LPARAM dwInitParam)
{
    (void)hInstance; (void)hWndParent; (void)lpDialogFunc; (void)dwInitParam;
    fprintf(stderr,
            "WARNING: DialogBoxParamA(template=%p) has no SDL3 host implementation "
            "(no native dialog/control-widget system) -- returning -1 (failure)\n",
            lpTemplateName);
    return -1;
}

LRESULT SendDlgItemMessageA(HWND hDlg, int nIDDlgItem, UINT Msg,
                            WPARAM wParam, LPARAM lParam)
{
    (void)hDlg; (void)nIDDlgItem; (void)Msg; (void)wParam; (void)lParam;
    fprintf(stderr,
            "WARNING: SendDlgItemMessageA(item=%d, msg=0x%x) has no SDL3 host "
            "implementation -- returning 0\n", nIDDlgItem, Msg);
    return 0;
}

BOOL EndDialog(HWND hDlg, int32_t nResult)
{
    (void)hDlg;
    fprintf(stderr,
            "WARNING: EndDialog(result=%d) has no SDL3 host implementation "
            "-- returning TRUE\n", nResult);
    return TRUE;
}

HWND GetDlgItem(HWND hDlg, int nIDDlgItem)
{
    (void)hDlg;
    fprintf(stderr,
            "WARNING: GetDlgItem(item=%d) has no SDL3 host implementation "
            "-- returning nullptr\n", nIDDlgItem);
    return nullptr;
}

/* =========================================================================
 * String helpers
 * ========================================================================= */

int wsprintfA(char* buf, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, 1024, fmt, args);
    va_end(args);
    return ret;
}

int lstrlenA(LPCSTR lpString)
{
    return lpString ? static_cast<int>(strlen(lpString)) : 0;
}

char* lstrcpyA(char* dst, LPCSTR src)
{
    if (dst && src) strcpy(dst, src);
    return dst;
}

char* lstrcatA(char* dst, LPCSTR src)
{
    if (dst && src) strcat(dst, src);
    return dst;
}

int LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax)
{
    (void)hInstance;
    /* The original loads from the executable's string table resource.
     * We return a placeholder. Real implementation needs to parse
     * the .exe's resource section. */
    if (lpBuffer && cchBufferMax > 0) {
        snprintf(lpBuffer, cchBufferMax, "STR_%u", uID);
        return static_cast<int>(strlen(lpBuffer));
    }
    return 0;
}

/* =========================================================================
 * System — stubs
 * ========================================================================= */

HRESULT CoInitializeEx(void* pvReserved, DWORD dwCoInit)
{
    (void)pvReserved; (void)dwCoInit;
    return 0; /* S_OK */
}

void CoUninitialize(void) {}

DWORD GetFileVersionInfoSizeA(LPCSTR file, DWORD* handle)
{
    (void)file;
    if (handle) *handle = 0;
    return 0;
}

BOOL GetFileVersionInfoA(LPCSTR file, DWORD handle, DWORD len, void* data)
{
    (void)file; (void)handle; (void)len; (void)data;
    return false;
}

BOOL VerQueryValueA(void* block, LPCSTR subBlock, void** buffer, UINT* len)
{
    (void)block; (void)subBlock; (void)buffer; (void)len;
    return false;
}

BOOL PlaySoundA(LPCSTR pszSound, HMODULE hmod, DWORD fdwSound)
{
    (void)hmod;

    // SND_PURGE (0x40): stop all playback.
    if (fdwSound & 0x40) {
        SDL3_GameAudioStopAll();
        return true;
    }

    // NULL sound with no purge → stop looping file only.
    if (!pszSound) {
        SDL3_GameAudioStopAll();
        return true;
    }

    // SND_FILENAME is implied when a path is given and SND_RESOURCE
    // (0x40004) is not set.  SND_ASYNC (1) and SND_LOOP (8) control
    // playback mode.
    const bool async = (fdwSound & 1) != 0;
    const bool loop  = (fdwSound & 8) != 0;
    (void)async;  // All host playback is async.

    return SDL3_GameAudioPlayFile(pszSound, loop);
}

/* =========================================================================
 * Registry — stubs
 * ========================================================================= */

LONG RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions,
                    DWORD samDesired, HKEY* phkResult)
{
    (void)hKey; (void)lpSubKey; (void)ulOptions; (void)samDesired;
    if (phkResult) *phkResult = reinterpret_cast<HKEY>(0x1000);
    return ERROR_SUCCESS;
}

LONG RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, DWORD* lpReserved,
                       DWORD* lpType, uint8_t* lpData, DWORD* lpcbData)
{
    (void)hKey; (void)lpValueName; (void)lpReserved;
    /* Return empty data */
    if (lpType)  *lpType  = REG_SZ;
    if (lpData && lpcbData && *lpcbData > 0) lpData[0] = 0;
    if (lpcbData) *lpcbData = 0;
    return ERROR_SUCCESS;
}

LONG RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved,
                      LPCSTR lpClass, DWORD dwOptions, DWORD samDesired,
                      void* lpSecurityAttributes, HKEY* phkResult,
                      DWORD* lpdwDisposition)
{
    (void)hKey; (void)lpSubKey; (void)Reserved; (void)lpClass;
    (void)dwOptions; (void)samDesired; (void)lpSecurityAttributes;
    if (phkResult) *phkResult = reinterpret_cast<HKEY>(0x1000);
    if (lpdwDisposition) *lpdwDisposition = 1; /* REG_CREATED_NEW_KEY */
    return ERROR_SUCCESS;
}

LONG RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved,
                     DWORD dwType, const uint8_t* lpData, DWORD cbData)
{
    (void)hKey; (void)lpValueName; (void)Reserved;
    (void)dwType; (void)lpData; (void)cbData;
    return ERROR_SUCCESS;
}

LONG RegCloseKey(HKEY hKey)
{
    (void)hKey;
    return ERROR_SUCCESS;
}


/* =========================================================================
 * Additional window/message stubs — added for Cursor file compatibility
 * ========================================================================= */

BOOL DestroyWindow(HWND hWnd)
{
    /* DestroyWindow: close the SDL window if it matches */
    (void)hWnd;
    return TRUE;
}

void PostQuitMessage(int nExitCode)
{
    (void)nExitCode;
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&ev);
}

BOOL ClientToScreen(HWND hWnd, POINT* lpPoint)
{
    /* ClientToScreen: convert client coords to screen coords.
     * For SDL3 single-window mode, client == screen offset by window pos. */
    (void)hWnd;
    if (lpPoint && g_sdl_window) {
        int wx, wy;
        SDL_GetWindowPosition(g_sdl_window, &wx, &wy);
        lpPoint->x += wx;
        lpPoint->y += wy;
    }
    return TRUE;
}

void GetCursorPos(POINT* lpPoint)
{
    if (lpPoint) {
        float fx, fy;
        SDL_GetGlobalMouseState(&fx, &fy);
        lpPoint->x = static_cast<int>(fx);
        lpPoint->y = static_cast<int>(fy);
    }
}

void OutputDebugStringA(const char* lpOutputString)
{
    if (lpOutputString) {
        fprintf(stderr, "[DEBUG] %s\n", lpOutputString);
    }
}

BOOL CopyRect(RECT* lprcDst, const RECT* lprcSrc)
{
    if (lprcDst && lprcSrc) {
        *lprcDst = *lprcSrc;
        return TRUE;
    }
    return FALSE;
}

BOOL OffsetRect(RECT* lprc, int dx, int dy)
{
    if (lprc) {
        lprc->left   += dx;
        lprc->right  += dx;
        lprc->top    += dy;
        lprc->bottom += dy;
        return TRUE;
    }
    return FALSE;
}

int UnionRect(RECT* lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2)
{
    if (!lprcDst || !lprcSrc1 || !lprcSrc2) return 0;
    if (lprcSrc1->left >= lprcSrc1->right || lprcSrc1->top >= lprcSrc1->bottom) {
        if (lprcSrc2->left >= lprcSrc2->right || lprcSrc2->top >= lprcSrc2->bottom) {
            lprcDst->left = lprcDst->right = lprcDst->top = lprcDst->bottom = 0;
            return 0;
        }
        *lprcDst = *lprcSrc2;
        return 1;
    }
    if (lprcSrc2->left >= lprcSrc2->right || lprcSrc2->top >= lprcSrc2->bottom) {
        *lprcDst = *lprcSrc1;
        return 1;
    }
    lprcDst->left   = (lprcSrc1->left < lprcSrc2->left)   ? lprcSrc1->left   : lprcSrc2->left;
    lprcDst->top    = (lprcSrc1->top  < lprcSrc2->top)    ? lprcSrc1->top    : lprcSrc2->top;
    lprcDst->right  = (lprcSrc1->right > lprcSrc2->right) ? lprcSrc1->right  : lprcSrc2->right;
    lprcDst->bottom = (lprcSrc1->bottom > lprcSrc2->bottom) ? lprcSrc1->bottom : lprcSrc2->bottom;
    return 1;
}

BOOL IntersectRect(RECT* lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2)
{
    if (!lprcDst || !lprcSrc1 || !lprcSrc2) return FALSE;
    lprcDst->left   = (lprcSrc1->left > lprcSrc2->left)   ? lprcSrc1->left   : lprcSrc2->left;
    lprcDst->top    = (lprcSrc1->top  > lprcSrc2->top)    ? lprcSrc1->top    : lprcSrc2->top;
    lprcDst->right  = (lprcSrc1->right < lprcSrc2->right) ? lprcSrc1->right  : lprcSrc2->right;
    lprcDst->bottom = (lprcSrc1->bottom < lprcSrc2->bottom) ? lprcSrc1->bottom : lprcSrc2->bottom;
    if (lprcDst->left >= lprcDst->right || lprcDst->top >= lprcDst->bottom) {
        lprcDst->left = lprcDst->right = lprcDst->top = lprcDst->bottom = 0;
        return FALSE;
    }
    return TRUE;
}

/* =========================================================================
 * GDI drawing extension stubs
 * ========================================================================= */

HGDIOBJ GetStockObject(int fnObject)
{
    (void)fnObject;
    return reinterpret_cast<HGDIOBJ>(1);  /* non-NULL dummy */
}

BOOL DrawEdge(HDC hdc, RECT* qrc, UINT edge, UINT grfFlags)
{
    (void)hdc; (void)qrc; (void)edge; (void)grfFlags;
    return TRUE;
}

COLORREF SetBkColor(HDC hdc, COLORREF color)
{
    (void)hdc;
    return color;
}

/* =========================================================================
 * File I/O stubs (thin wrappers, real I/O in link_stubs.cpp POSIX path)
 * ========================================================================= */

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   void* lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    /* Delegate to POSIX via link_stubs.cpp — this stub should not be called */
    (void)lpFileName; (void)dwDesiredAccess; (void)dwShareMode;
    (void)lpSecurityAttributes; (void)dwCreationDisposition;
    (void)dwFlagsAndAttributes; (void)hTemplateFile;
    return reinterpret_cast<HANDLE>(-1);  /* INVALID_HANDLE_VALUE */
}

DWORD GetFileSize(HANDLE hFile, DWORD* lpFileSizeHigh)
{
    (void)hFile;
    if (lpFileSizeHigh) *lpFileSizeHigh = 0;
    return 0;
}

BOOL CloseHandle(HANDLE hObject)
{
    (void)hObject;
    return TRUE;
}

DWORD GetLastError(void)
{
    return 0;
}

DWORD FormatMessageA(DWORD dwFlags, const void* lpSource, DWORD dwMessageId,
                      DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, void* Arguments)
{
    (void)dwFlags; (void)lpSource; (void)dwMessageId;
    (void)dwLanguageId; (void)Arguments;
    if (lpBuffer && nSize > 0) lpBuffer[0] = 0;
    return 0;
}

void* LocalFree(void* hMem)
{
    (void)hMem;
    return nullptr;
}

BOOL GetOpenFileNameA(void* lpofn)
{
    (void)lpofn;
    return FALSE;  /* no file dialog in headless/SDL3 mode */
}

/* =========================================================================
 * Process / thread stubs
 * ========================================================================= */

void Sleep(DWORD dwMilliseconds)
{
    SDL_Delay(dwMilliseconds);
}

void ExitProcess(UINT uExitCode)
{
    exit(static_cast<int>(uExitCode));
}

#endif /* _WIN32 */

