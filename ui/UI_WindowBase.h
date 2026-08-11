/**
 * UI_WindowBase.h — Base class for all game UI windows
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * UI_WindowBase is the root of the UI window hierarchy. It provides:
 *   - HWND lifecycle: creation (RegisterClass + CreateWindowEx via CreateFullWindow),
 *     destruction (ReleaseCapture, DestroyWindow), Show/Hide
 *   - Timer management (SetTimer/KillTimer in Show/Hide)
 *   - Client-RECT caching (GetClientRect dispatched from OnCreate)
 *   - Shared cursor backbuffer reference-counting (global cursor surface
 *     shared among all UI windows, released when last owner decrements)
 *   - Three child sub-object slots for sub-windows (released in base dtor)
 *   - Title string loading from resource strings (FormatResourceString)
 *   - A default WndProc that passes through to DefWindowProcA
 *
 * Subclasses: Cursor (0x477930), EditWindow / UI_MainMenu (0x4779F8),
 *   PostcardAlbum (0x4773F0), PostcardPreviewWindow, Town dialog,
 *   NameEntryPanel (0x4781D0), GameSetupPanel (0x4774D0), and others.
 *
 * Size: 0xE8 bytes (232 bytes, base class footprint)
 * Vtable: 0x477C30 (VTBL_UI_WINDOWBASE)
 *
 * Class hierarchy:
 *   UI_WindowBase  ← this class (root)
 *     ├─ Cursor                    (vtable 0x477930)
 *     ├─ EditWindow (UI_MainMenu)  (vtable 0x4779F8)
 *     ├─ PostcardAlbum             (vtable 0x4773F0)
 *     ├─ PostcardPreviewWindow     (vtable 0x4778F8)
 *     ├─ Town                      (vtable 0x477D88)
 *     ├─ NameEntryPanel            (vtable 0x4781D0, name-entry/multiplayer lobby)
 *     └─ GameSetupPanel            (vtable 0x4774D0, city-selection/game-setup lobby)
 *
 * Vtable layout (0x477C30, 37 slots - full binary layout):
 *   [0]  +0x00: scalar deleting destructor  (UI_WindowBase_Dtor, 0x4258F0)
 *   [1]  +0x04: Hide                        (UI_WindowBase_Hide, 0x425990)
 *   [2]  +0x08: Show                        (UI_WindowBase_Show, 0x4259C0)
 *   [3]  +0x0C: SetMode                     (UI_WindowBase_SetMode, 0x425FD0)
 *   [4]  +0x10: SetRenderSurface            (UI_WindowBase_SetRenderSurface, 0x426020)
 *   [5]  +0x14: OnAsyncTaskFailure          (0x426130, RET 4 no-op)
 *   [6]  +0x18: CreateFullWindow            (UI_CreateFullWindow, 0x425B70)
 *   [7]  +0x1C: OnCreate                    (UI_WindowBase_OnCreate, 0x425D30)
 *   [8]  +0x20: on_update (1 stack arg)     (default 0x426130, RET 4 no-op)
 *   [9]  +0x24: on_noop                     (default 0x4661A0, bare RET)
 *   [10] +0x28: dispatch_message            (0x426140 - WM_* router)
 *   [11] +0x2C: window_proc                 (UI_DefWndProc, 0x422EA0)
 *   [12] +0x30: on_timer   (WM_TIMER 0x113)  (default 0x422EA0)
 *   [13] +0x34: on_create_msg (WM_CREATE 1)  (default 0x422EA0)
 *   [14] +0x38: on_lbutton_down (WM_LBUTTONDOWN 0x201)  (default 0x422EA0)
 *   [15] +0x3C: on_lbutton_up   (WM_LBUTTONUP 0x202)    (default 0x422EA0)
 *   [16] +0x40: on_rbutton_down (WM_RBUTTONDOWN 0x204)  (default 0x422EA0)
 *   [17] +0x44: on_rbutton_up   (WM_RBUTTONUP 0x205)    (default 0x422EA0)
 *   [18] +0x48: on_lbutton_dblclk (WM_LBUTTONDBLCLK 0x203) (default 0x422EA0)
 *   [19] +0x4C: on_rbutton_dblclk (WM_RBUTTONDBLCLK 0x206) (default 0x422EA0)
 *   [20] +0x50: on_mouse_move (WM_MOUSEMOVE 0x200)  (default 0x426900 UIPANEL_WindowProc)
 *   [21] +0x54: on_key_down  (WM_KEYDOWN 0x100)      (default 0x422EA0)
 *   [22] +0x58: on_key_up    (WM_KEYUP 0x101)        (default 0x422EA0)
 *   [23] +0x5C: on_mouse_activate (WM_MOUSEACTIVATE 0x21) (default 0x426950, returns 0)
 *   [24] +0x60: on_set_focus (WM_SETFOCUS 7)         (default 0x422EA0)
 *   [25] +0x64: on_kill_focus (WM_KILLFOCUS 8)       (default 0x422EA0)
 *   [26] +0x68: on_size     (WM_SIZE 5)              (default 0x426960, re-runs on_create)
 *   [27] +0x6C: on_paint    (WM_PAINT 0xF)           (default 0x426980)
 *   [28] +0x70: on_set_cursor (WM_SETCURSOR 0x20)    (default 0x426A60)
 *   [29] +0x74: on_show_window (WM_SHOWWINDOW 0x18)  (default 0x422EA0)
 *   [30] +0x78: on_erase_bkgnd (WM_ERASEBKGND 0x14)  (default 0x426AC0, returns 1)
 *   [31] +0x7C: on_destroy  (WM_DESTROY 2)           (default 0x426AD0)
 *   [32] +0x80: on_close    (WM_CLOSE 0x10)          (default 0x426A90 UIPANEL_OnDestroy)
 *   [33] +0x84: on_notify   (WM_NOTIFY 0x4E)         (default 0x422EA0)
 *   [34] +0x88: on_command  (WM_COMMAND 0x111)       (default 0x422EA0)
 *   [35] +0x8C: on_msg_312  (0x312)                  (default 0x422EA0)
 *   [36] +0x90: on_activate_app (WM_ACTIVATEAPP 0x1C)(default 0x422EA0)
 *
 * For subclasses, vtable slots [3] through [36] are inherited; subclasses
 * may override any of them. For instance, Cursor overrides [1], [7], [8],
 * [11] and adds Cursor-specific slots beyond [11]; EditWindow overrides
 * [1], [2], [7], [8], [9], [11] and adds its own slots beyond [11];
 * GameSetupPanel overrides [8] (Render).
 */

#pragma once

#include <cstddef>
#include "../shared/types.h"

// Status: TRANSCRIBED
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

/* ================================================================== */
/* Typed UI animation metadata used by UI_WindowBase::set_mode          */
/* ================================================================== */

/**
 * The default slot-3 implementation at 0x425FD0 reads only these three
 * fields from a resource descriptor.  This narrow view is intentionally
 * separate from the broader, multi-purpose RESDATA layout.
 */
struct UIAnimationMetadata {
    uint8_t _pad_00[0x32];
    int16_t hotspot_x;       // +0x32
    int16_t hotspot_y;       // +0x34
    uint8_t _pad_36[0x12A];
    uint16_t frame_count;    // +0x160
};
static_assert(offsetof(UIAnimationMetadata, hotspot_x) == 0x32);
static_assert(offsetof(UIAnimationMetadata, frame_count) == 0x160);

struct UIAnimationOrigin {
    int32_t x;
    int32_t y;
};

struct UIPANEL_Surface;

/* ================================================================== */
/* UI_WindowBase class                                                  */
/* ================================================================== */

class UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (vtable pointer at +0x00 is compiler-managed)               */
    /* ================================================================ */

    HINSTANCE  hInstance;              // +0x04  application instance handle (parent HINSTANCE)
    HWND       hWnd;                   // +0x08  this window's HWND (set by CreateFullWindow)
    HWND       hWndParent;             // +0x0C  parent window HWND (stored during creation)
    UINT       resourceId;             // +0x10  window/dialog resource ID

    /* Zeroed by constructor, usage varies by subclass */
    int32_t    field_14;               // +0x14  set by set_render_surface() to the active surface
                                        //         address/identity; dispatch_message's WM_TIMER
                                        //         (0x113) and WM_CAPTURECHANGED (0x215) handlers
                                        //         gate on `field_14 != 0` ("a render surface is
                                        //         configured"). See 0x426140.
    int32_t    field_18;               // +0x18  (unknown, zeroed by ctor)
    int32_t    field_1C;               // +0x1C  (unknown, zeroed by ctor)
    int32_t    field_20;               // +0x20  frame divisor / animation-tick threshold, set by
                                        //         set_render_surface(); compared against field_24
                                        //         in dispatch_message's WM_TIMER fast path (0x426140)
    int32_t    field_24;               // +0x24  animation-tick counter, wraps at field_20, reset by
                                        //         set_render_surface(); incremented by dispatch_message's
                                        //         WM_TIMER fast path (0x426140)

    UINT_PTR   timerId;                // +0x28  window timer ID (set by Show via SetTimer)
    int32_t    field_2C;               // +0x2C  (unknown, zeroed by ctor)
    int32_t    field_30;               // +0x30  (unknown, zeroed by ctor)

    /** Last observed cursor position (screen coords), used by dispatch_message's
     *  WM_TIMER (0x113) fast path to detect "cursor has not moved since the
     *  last tick" and re-render the idle-hover animation. Reset to -1
     *  (sentinel: "unknown/invalidate") by UIPANEL_Render (0x426EB0) after
     *  every render pass. NOT zeroed by the constructor — set by subclass /
     *  first WM_TIMER tick. Confirmed at 0x426140 (dispatch_message) and
     *  0x426EB0 (UIPANEL_Render). */
    int32_t    lastCursorX;            // +0x34  (was field_34)
    int32_t    lastCursorY;            // +0x38  (was field_38)

    uint8_t    captureFlag;            // +0x3C  mouse capture flag (byte, zeroed by ctor, set by show/hide)
                                        //         0 = SetCapture'd / OS cursor hidden (dragging over
                                        //         this window); 1 = ReleaseCapture'd / OS cursor shown.
                                        //         Toggled by dispatch_message's WM_NCHITTEST (0x84),
                                        //         WM_MOUSEMOVE (0x200) and WM_CAPTURECHANGED (0x215).
    uint8_t    field_3D;               // +0x3D  one-shot "idle animation expired" flag, zeroed by ctor;
                                        //         set permanently by dispatch_message's WM_TIMER (0x113)
                                        //         fast path once field_40 counts down to 0; while set,
                                        //         the WM_TIMER fast path is skipped entirely (0x426140).
    uint8_t    _pad_3E[2];             // +0x3E  padding
    int32_t    field_40;               // +0x40  idle-animation cycle countdown, zeroed by ctor;
                                        //         decremented once per field_20 animation-tick wrap by
                                        //         dispatch_message's WM_TIMER fast path (0x426140); at 0
                                        //         it sets field_3D and stops decrementing further.

    uint8_t    activeFlag;             // +0x44  active/hidden flag (byte, cleared by Hide)
    uint8_t    _pad_45[3];             // +0x45  padding

    int32_t    cursorRefCount;         // +0x48  shared cursor backbuffer reference count
                                       //         Incremented by init_sprites, decremented by base dtor.
                                       //         When all owners release, the global cursor backbuffer
                                       //         (_g_cursor_back @ 0x4FD3CC) is freed.

    int32_t    field_4C;               // +0x4C  (unknown, NOT zeroed by ctor — set by subclass)

    int32_t    field_50;               // +0x50  (unknown, zeroed by ctor)
    int32_t    field_54;               // +0x54  (unknown, zeroed by ctor)
    int32_t    field_58;               // +0x58  (unknown, zeroed by ctor)
    int32_t    field_5C;               // +0x5C  (unknown, zeroed by ctor)

    /** Three child sub-objects, released by the base destructor.
     *  Each pair consists of: count/flag at the first offset, and the
     *  vtable-based object pointer at the second offset.
     *  Objects are released via vtable[2] (index 2 = vtable+0x08). */
    int32_t    childCount0;            // +0x60  child 0 count/flag
    void*      childObj0;              // +0x64  child 0 object pointer (vtable-based)
    int32_t    childCount1;            // +0x68  child 1 count/flag
    void*      childObj1;              // +0x6C  child 1 object pointer (vtable-based)
    int32_t    childCount2;            // +0x70  child 2 count/flag
    void*      childObj2;              // +0x74  child 2 object pointer (vtable-based)

    char       title[50];              // +0x78  window title string (loaded from FormatResourceString)

    uint8_t    windowCreated;          // +0xAB  flag: 1 = window has been created (set by CreateFullWindow)
    /* +0xAC..+0xB3: gap, used by subclasses for window position */

    int32_t    windowX;                // +0xAC  window left X position
    int32_t    windowY;                // +0xB0  window top Y position

    int32_t    windowWidth;            // +0xB4  window width (right - left from client rect)
    int32_t    windowHeight;           // +0xB8  window height (bottom - top from client rect)

    int32_t    workWidth;              // +0xBC  working rect width (workRight - workLeft)
    int32_t    workHeight;             // +0xC0  working rect height (workBottom - workTop)

    RECT       clientRect;             // +0xC4  client rectangle {left, top, right, bottom}
                                       //         (populated by GetClientRect in OnCreate)

    RECT       workRect;               // +0xD4  working rectangle {left, top, right, bottom}
                                       //         (copy of clientRect, used as target for layout calc)

    uint8_t    visible;                // +0xE4  visible flag (byte, set by Show, cleared by Hide)
    uint8_t    _pad_E5[3];             // +0xE5  padding to align 0xE8
    /* Total base size: 0xE8 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * UI_WindowBase constructor.
     * Address: 0x425870
     *
     * Initializes the base window object: stores hInstance at +0x04,
     * resourceId at +0x10, zeroes all other fields, loads window title
     * from string resources via FormatResourceString into +0x78 (max 50
     * chars). In the binary: sets vtable to 0x477C30.
     *
     * Called by: ALL subconstructors — Cursor_Ctor @ 0x4159A9,
     *            UI_MainMenu_Ctor @ 0x42031A, PostcardAlbum_Create @ 0x401F79,
     *            GameSetupPanel_Ctor @ 0x408AC9, Town_Ctor @ 0x42E929,
     *            PostcardPreviewWindow_Ctor @ 0x430AB9,
     *            NameEntryPanel_Ctor @ 0x440F49
     *
     * @param hInstance    Application instance handle
     * @param resId        Window resource ID (used to load title string)
     */
    UI_WindowBase(HINSTANCE hInstance, UINT resId);

    /**
     * Virtual destructor (vtable[0]).
     * In the binary: MSVC scalar deleting destructor at 0x4258F0
     * calls base_destructor, then GLOBAL_free if flags & 1.
     */
    virtual ~UI_WindowBase();

    /**
     * Base destructor body (called by destructor).
     * Address: 0x425910
     *
     * Release sequence:
     *   1. Resets vtable to VTBL_UI_WINDOWBASE (in the binary; compiler-managed here)
     *   2. Releases three child sub-objects via vtable[2] if non-null
     *      (pairs at +0x60/+0x64, +0x68/+0x6C, +0x70/+0x74)
     *   3. Decrements global cursor backbuffer refcount (+0x48).
     *      If refcount reaches 0, releases the global cursor backbuffer
     *      (_g_cursor_back @ 0x4FD3CC) via vtable[2]
     *   4. Clears visible flag (+0xE4)
     */
    void base_destructor();

    /**
     * Show the window (pseudo-modal).
     * Address: 0x4259C0 (vtable[2])
     *
     * Creates a 120ms timer (ID 0x43), captures mouse input,
     * hides the OS cursor, renders the window via UIPANEL_Render,
     * disables the window (EnableWindow(FALSE)), and calls
     * ShowWindow(SW_SHOW). Sets visible flag.
     */
    virtual void hide();

    /**
     * Show the window (pseudo-modal).
     * Address: 0x4259C0 (vtable[2])
     *
     * Creates a 120ms timer (ID 0x43), captures mouse input,
     * hides the OS cursor, renders the window via UIPANEL_Render,
     * disables the window (EnableWindow(FALSE)), and calls
     * ShowWindow(SW_SHOW). Sets visible flag.
     */
    virtual void show();

    /**
     * Set the active UI animation mode (vtable[3]).
     * Address: 0x425FD0
     *
     * The base implementation adapts the resource hotspot/frame count into
     * the five-argument render-surface hook below. Cursor overrides this
     * slot with its own animation-state implementation.
     */
    virtual void set_mode(int32_t surface_address, void* animation_metadata,
                          uint8_t reset_position, uint8_t force_redraw);

    /**
     * Configure the active render surface (vtable[4]).
     * Address: 0x426020
     */
    virtual void set_render_surface(UIPANEL_Surface* surface, uint32_t frame_divisor,
                                    const UIAnimationOrigin* origin,
                                    uint8_t reset_dirty_rect, uint8_t force_redraw);

    /**
     * Base asynchronous-task failure hook (vtable[5]).
     * Address: 0x426130 — a three-byte `ret 4` no-op in the original.
     */
    virtual void on_async_task_failure(int32_t reason);

    /**
     * Client rect update callback (vtable[7]).
     * Address: 0x425D30
     *
     * Called after window creation or on resize. Synchronizes the
     * cached client rect (+0xC4) from the actual HWND via GetClientRect.
     * Computes window width/height and working rect. Only executes
     * when windowCreated flag (+0xAB) is non-zero.
     */
    virtual void on_create();

    /* ================================================================ */
    /* Static / Non-member helper                                         */
    /* ================================================================ */

    /**
     * Create the full-screen window (vtable[6]).
     * Address: 0x425B70
     *
     * Registers a WNDCLASS with the window title string and default
     * WndProc stubs; creates a full-screen WS_POPUP window via
     * CreateWindowExA; sets windowCreated flag; dispatches vtable[7]
     * (on_create); calls Cursor_SetupSurface; shows and updates window.
     *
     * Called by: Cursor_Create @ 0x416A0A, UI_MainMenu_Create @ 0x420539,
     *            and all subclass window-creation routines.
     *
     * @param nCmdShow    Initial show command (SW_* flags)
     * @param hParent     Parent window HWND
     * @param x           Window X position
     * @param y           Window Y position
     * @param nWidth      Window width
     * @param nHeight     Window height
     * @param hMenu       Menu handle (or NULL)
     * @param hIcon       Window icon handle
     * @param classStyle  WNDCLASS.style override (0 = default CS_HREDRAW|CS_VREDRAW)
     * @return            BOOL: 1 on success, 0 on failure
     */
    virtual int create_full_window(int nCmdShow, HWND hParent,
                                   int x, int y, int nWidth, int nHeight,
                                   HMENU hMenu, HICON hIcon, UINT classStyle);

    /* ================================================================ */
    /* Message-handler slots [8]-[36] - binary vtable 0x477C30          */
    /* ================================================================ */

    /**
     * Update/render callback (vtable[8]).
     * Address: 0x426130 - a three-byte RET 4 no-op in the original.
     * Subclasses override this slot for their per-frame update/render.
     */
    virtual void on_update(int32_t param);

    /**
     * No-op callback (vtable[9]).
     * Address: 0x4661A0 - bare RET in the original.
     */
    virtual void on_noop();

    /**
     * Message dispatcher (vtable[10]).
     * Address: 0x426140 - routes WM_* messages to slots [12]-[36].
     * Base implementation is the recovered dispatcher.
     */
    virtual LRESULT dispatch_message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * Default window proc (vtable[11]).
     * Address: 0x422EA0 (UI_DefWndProc) - passthrough to DefWindowProcA.
     */
    virtual LRESULT window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_TIMER handler (vtable[12]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_timer(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_CREATE handler (vtable[13]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_create_msg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_LBUTTONDOWN handler (vtable[14]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_lbutton_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_LBUTTONUP handler (vtable[15]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_lbutton_up(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_RBUTTONDOWN handler (vtable[16]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_rbutton_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_RBUTTONUP handler (vtable[17]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_rbutton_up(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_LBUTTONDBLCLK handler (vtable[18]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_lbutton_dblclk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_RBUTTONDBLCLK handler (vtable[19]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_rbutton_dblclk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_MOUSEMOVE handler (vtable[20]).
     * Default: 0x426900 (UIPANEL_WindowProc - render on this window).
     */
    virtual LRESULT on_mouse_move(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_KEYDOWN handler (vtable[21]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_key_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_KEYUP handler (vtable[22]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_key_up(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_MOUSEACTIVATE handler (vtable[23]).
     * Default: 0x426950 (returns 0).
     */
    virtual LRESULT on_mouse_activate(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_SETFOCUS handler (vtable[24]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_set_focus(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_KILLFOCUS handler (vtable[25]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_kill_focus(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_SIZE handler (vtable[26]).
     * Default: 0x426960 - re-runs on_create when windowCreated (+0xAB).
     */
    virtual LRESULT on_size(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_PAINT handler (vtable[27]).
     * Default: 0x426980 (unlock, update rect, begin/end paint).
     */
    virtual LRESULT on_paint(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_SETCURSOR handler (vtable[28]).
     * Default: 0x426A60 (returns 1 when hwnd matches this->hWnd).
     */
    virtual LRESULT on_set_cursor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_SHOWWINDOW handler (vtable[29]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_show_window(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_ERASEBKGND handler (vtable[30]).
     * Default: 0x426AC0 (returns 1).
     */
    virtual LRESULT on_erase_bkgnd(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_DESTROY handler (vtable[31]).
     * Default: 0x426AD0 (clears hWnd, DefWindowProcA).
     */
    virtual LRESULT on_destroy(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_CLOSE handler (vtable[32]).
     * Default: 0x426A90 (UIPANEL_OnDestroy).
     */
    virtual LRESULT on_close(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_NOTIFY handler (vtable[33]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_notify(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_COMMAND handler (vtable[34]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_command(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * Custom message 0x312 handler (vtable[35]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_msg_312(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * WM_ACTIVATEAPP handler (vtable[36]).
     * Default: 0x422EA0 (UI_DefWndProc passthrough).
     */
    virtual LRESULT on_activate_app(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

/* ================================================================== */
/* Global state references                                              */
/* ================================================================== */

/* Global cursor backbuffer surface — shared among all UI windows.
   Released when the last cursorRefCount holder calls base_destructor. */
extern void* g_cursor_back;     /* 0x4FD3CC */
extern int   g_cursor_refcount;  /* 0x4FD3D0 */

/* ================================================================== */
/* UI_SetWindowVisible — free function, NOT a vtable slot.              */
/* Address: 0x425F20. See UI_WindowBase.cpp for full documentation.     */
/*                                                                     */
/* One canonical `char` overload. ui/HelpWnd.cpp keeps its own separate */
/* (identical) local `char` declaration rather than including this      */
/* header. A second `unsigned char` overload was tried and removed:      */
/* every real call site passes an int literal (0 or 1), which is        */
/* equally-good-ranked against both `char` and `unsigned char`, so       */
/* having both simultaneously visible (as happens once ui/EditWindow.h  */
/* pulls this header in) makes every such call ambiguous.               */
/* ================================================================== */
void UI_SetWindowVisible(void* self, char visible);
