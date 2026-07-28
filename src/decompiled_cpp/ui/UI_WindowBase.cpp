/**
 * UI_WindowBase.cpp — UI_WindowBase implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

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
    extern int   __stdcall ShowCursor(BOOL bShow);
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
    extern void  __stdcall DefWindowProcA(HWND hWnd, UINT Msg, void* wParam,
                                          void* lParam);
    extern int   __stdcall GetWindowTextA(HWND hWnd, char* lpString, int nMaxCount);
}

/* UI module function declarations */
extern void  __fastcall DDRAW_UnlockPrimary(HWND hWnd);     /* 0x45B940 — takes HWND */
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
extern void __stdcall UI_DefWndProc(HWND hWnd, UINT msg, void* wParam, void* lParam);
/* 0x422EA0 — just calls DefWindowProcA */

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
        void** vtab = *(void***)obj;
        ((void (__fastcall*)(void*))(vtab[2]))(obj);
        this->childObj0   = NULL;                        /* +0x64 */
        this->childCount0 = 0;                           /* +0x60 */
    }

    if (this->childCount1 != 0) {                        /* +0x68 */
        void* obj = this->childObj1;                     /* +0x6C */
        void** vtab = *(void***)obj;
        ((void (__fastcall*)(void*))(vtab[2]))(obj);
        this->childObj1   = NULL;                        /* +0x6C */
        this->childCount1 = 0;                           /* +0x68 */
    }

    if (this->childObj2 != NULL) {                       /* +0x74 */
        void* obj = this->childObj2;
        void** vtab = *(void***)obj;
        ((void (__fastcall*)(void*))(vtab[2]))(obj);
        this->childObj2    = NULL;                       /* +0x74 */
        this->childCount2  = 0;                          /* +0x70 */
    }

    /* Decrement global cursor backbuffer refcount */
    if (this->cursorRefCount != 0) {                     /* +0x48 */
        g_cursor_refcount--;
        if (g_cursor_refcount == 0 && g_cursor_back != NULL) {
            void** vtab = *(void***)g_cursor_back;
            ((void (__fastcall*)(void*))(vtab[2]))(g_cursor_back);
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
    this->timerId = (UINT_PTR)SetTimer(this->hWnd, 0x43, 120, NULL);

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
    DDRAW_UnlockPrimary(this->hWnd);
    std::fprintf(stderr, "[TRACE] UI_WindowBase::show: render panel\n");
#ifdef _WIN32
    UIPANEL_Render(this, 1);
#else
    // The SDL compositor receives the decoded EditWindow sprites directly;
    // do not reinterpret this 64-bit UI_WindowBase as the x86 UIPANEL layout.
    SDL3_PresentPrimarySurface();
#endif
    std::fprintf(stderr, "[TRACE] UI_WindowBase::show: unlock primary complete\n");
    DDRAW_UnlockPrimary(this->hWnd);

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
        DDRAW_UnlockPrimary(this->hWnd);
        UIPANEL_Render(this, reset_dirty_rect == 0);
        DDRAW_UnlockPrimary(this->hWnd);
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
int UI_WindowBase::create_full_window(UI_WindowBase* self, int nCmdShow,
                                       HWND hParent, int x, int y,
                                       int nWidth, int nHeight,
                                       HMENU hMenu, HICON hIcon, UINT classStyle)
{
    /* Clear existing HWND before creating new one */
    self->hWnd = NULL;

    /* Get parent window caption text for the window title */
    char parentTitle[256];
    GetWindowTextA(hParent, parentTitle, sizeof(parentTitle));

    /* Store layout parameters */
    self->windowX      = x;
    self->windowY      = y;
    self->windowWidth  = nWidth;
    self->windowHeight = nHeight;
    self->hWndParent   = hParent;

    /* Register WNDCLASS */
    // The original zeros 40 bytes for x86 WNDCLASS. Value initialization is
    // layout-safe on the 64-bit SDL host as well.
    WNDCLASSA wc{};
    wc.style       = CS_HREDRAW | CS_VREDRAW;  /* 3 = CS_HREDRAW | CS_VREDRAW */
    if (classStyle != 0) {
        wc.style   = classStyle;
    }
    wc.hInstance   = self->hInstance;            /* +0x04 */
    wc.lpfnWndProc = &DefWindowProcA;            /* NOTE: actual base WndProc at 0x4272F0 */
    wc.cbClsExtra  = 0;
    wc.cbWndExtra  = 0;
    wc.hIcon       = hIcon;
    wc.hCursor     = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = self->title;              /* +0x78 — title string as class name */

    ATOM atom = RegisterClassA(&wc);
    if (atom == 0) {
        DWORD err = GetLastError();
        if (err != 0) {
            char* buf;
            FormatMessageA(0x1100, NULL, err, 0x400, (char*)&buf, 0, NULL);
            LocalFree(buf);
        }
    }

    /* Create the actual window */
    self->hWnd = CreateWindowExA(
        0,                                    /* dwExStyle */
        self->title,                          /* lpClassName (same as reg'd class) */
        parentTitle,                          /* lpWindowName */
        0x87000000,                           /* dwStyle: WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS |
                                                 WS_CLIPCHILDREN | WS_SYSMENU */
        x, y,
        self->windowWidth,  self->windowHeight,
        self->hWndParent,
        hMenu,
        self->hInstance,
        self                                 /* lpParam = this */
    );

    if (self->hWnd == NULL) {
        DWORD err = GetLastError();
        char* buf;
        FormatMessageA(0x1100, NULL, err, 0x400, (char*)&buf, 0, NULL);
        LocalFree(buf);
        return 0;
    }

    /* Mark window as created and fire OnCreate (vtable[7]) */
    self->windowCreated = 1;
    self->on_create();

#ifdef _WIN32
    Cursor_SetupSurface((int)self);  /* set up cursor surface for this window */
#else
    // Cursor_SetupSurface has not yet been reconstructed for the host and its
    // original int pointer ABI truncates this on 64-bit. SDL owns the cursor.
#endif
    ShowWindow(self->hWnd, nCmdShow);
    UpdateWindow(self->hWnd);

    return 1;
}
