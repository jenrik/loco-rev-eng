// Status: INTEGRATED
/**
 * PostcardPreviewWindow.h — Postcard preview/send dialog
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * PostcardPreviewWindow is the postcard preview/send dialog shown when
 * sending a postcard to another player. It extends UI_WindowBase and
 * manages 11 sprite objects (close button, options button, 9 status
 * sprites) plus background and overlay resources. It is created once
 * during CGWND_InitAllSubsystems and shown/hidden via vtable dispatch
 * (CGWND_SetMode(9) @ 0x408294 dispatches vtable[2]).
 *
 * Size: 0x2C4 bytes (708 bytes, allocated by CGWND_InitAllSubsystems)
 * Vtable: 0x477E20 (VTBL_POSTCARD_PREVIEW_WINDOW)
 *
 * Class hierarchy:
 *   UI_WindowBase (VTBL_UI_WINDOWBASE, size 0xE8)
 *     └─ PostcardPreviewWindow  <- this class (+0xE8..+0x2C4, total size 0x2C4)
 *
 * Vtable layout (0x477E20, extends UI_WindowBase — verified via DATA xrefs):
 *   [0]  +0x00: scalar deleting destructor (PostcardPreviewWindow_Dtor, 0x430AF0)
 *               -> ~PostcardPreviewWindow -> destroy() (DtorBody 0x430C60)
 *   [1]  +0x04: Hide / Cleanup (0x430E00)               — overridden: hide()
 *   [2]  +0x08: Show (0x430D70)                         — overridden: show()
 *               (dispatched by CGWND_SetMode(9) @ 0x408294)
 *   [3]  +0x0C: set_mode (inherited, 0x425FD0)
 *   [4]  +0x10: set_render_surface (inherited, 0x426020)
 *   [5]  +0x14: on_async_task_failure (inherited, 0x426130)
 *   [6]  +0x18: create_full_window (inherited, 0x425B70)
 *   [7]  +0x1C: (0x430FE0 — NOT yet decompiled; invoked by show())
 *   [8]  +0x20: (0x431310 — NOT yet decompiled)
 *   [9]  +0x24: (inherited no-op, 0x4661A0)
 *   [10] +0x28: (inherited, 0x426140)
 *   [11] +0x2C: WindowProc (BuildingPanel_WndProc, 0x4324F0)
 *   [12] +0x30: (0x4323E0 — BuildingPanel region, not yet decompiled)
 *
 * NOTE: the previously documented "[2] Show 0x430C40" was WRONG.
 * 0x430C40 sits inside PostcardPreviewWindow_InitBackground
 * (0x430C20..0x430C55) and has no xrefs; the verified vtable[2] target
 * is 0x430D70. The documented "[1] Hide 0x430B40" was also wrong:
 * vtable[1] is PostcardPreviewWindow_Cleanup at 0x430E00.
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
#include "UI_WindowBase.h"
#include "ButtonSprite.h"

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

class Netman;

/* ================================================================== */
/* PostcardPreviewWindow class                                          */
/* ================================================================== */

class PostcardPreviewWindow : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */
    /* --- Inherited from UI_WindowBase (0x00..0xE8) --- */
    /* +0x00: vtable (compiler-managed)                                */
    /* +0x04: hInstance, +0x08: hWnd, +0x0C..+0xE7: see UI_WindowBase.h */
    /* +0xE4: visible                                                   */

    /* --- Preview-window-specific fields (+0xE8..+0x2C4) --- */

    void*      icon_handle;             // +0xE8  HICON written by the shared InitWindow
                                        //         (PostcardAlbum::InitWindow 0x402520, called
                                        //         on this object by CGWND_InitAllSubsystems);
                                        //         zeroed by draw_sprites()
    UINT_PTR   timerId;                 // +0xEC  timer ID for the preview dialog timer
                                        //         (SetTimer(hWnd, 0x4D, 120ms) in show())
    uint8_t    _pad_F0[0x180];          // +0xF0..+0x26F

    int32_t    unknown_270;             // +0x270  state field (init to -1 in draw_sprites)
    uint8_t    _pad_274[4];             // +0x274
    int32_t    unknown_278;             // +0x278  state field (init to 0 in draw_sprites)
    uint8_t    _pad_27C;                // +0x27C
    uint8_t    flag_27D;                // +0x27D  flag (cleared by show()/hide())
    uint8_t    sprites_created;         // +0x27E  1 = sprites created+initialized
    uint8_t    _pad_27F;                // +0x27F

    void*      background_surface;      // +0x280  background surface (Lock(0,0) of 0x3d8a)
    void*      background_resource;     // +0x284  background resource (res 0x3d8a)

    void*      overlay_surface_2;       // +0x288  overlay surface 2 (res 0x3d87)
    void*      overlay_resource_2;      // +0x28C  overlay resource 2
    void*      overlay_surface_3;       // +0x290  overlay surface 3 (res 0x3d88)
    void*      overlay_resource_3;      // +0x294  overlay resource 3

    /* 11 ButtonSprite objects (0x24 bytes each) */
    ButtonSprite* sprite_close;         // +0x298  close button sprite (res 0x3d89)
    ButtonSprite* sprite_status[9];     // +0x29C  9 status/indicator sprites (res 0x3da4..0x3dac)
    ButtonSprite* sprite_options;       // +0x2C0  options button sprite (res 0x3d8b)

    /* Total x86 size: 0x2C4 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * PostcardPreviewWindow constructor.
     * Address: 0x430A90 (__thiscall)
     *
     * Chains to UI_WindowBase(hInstance, resId), then calls draw_sprites
     * to create all 11 preview dialog sprites.
     *
     * Called by: CGWND_InitAllSubsystems @ 0x407116
     *
     * @param hInstance  Application instance handle
     * @param resId      Window resource ID (0x1F7 in the game)
     */
    PostcardPreviewWindow(HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x430AF0 (__thiscall)
     *
     * Calls destroy() (DtorBody) to release all resources; the
     * compiler-emitted deleting destructor then releases the heap
     * allocation (GLOBAL_free) when delete is used.
     */
    virtual ~PostcardPreviewWindow();

    /**
     * Full destructor body — releases all resources.
     * Address: 0x430C60 (__fastcall, ECX = this, PostcardPreviewWindow_DtorBody)
     *
     * Releases background resource, destroys all 11 sprites
     * (Sprite_Destroy + scalar-deleting dtor for each), then base
     * cleanup runs through the C++ destructor chain.
     */
    void destroy();

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * draw_sprites — Create all 11 preview dialog sprite objects.
     * Address: 0x430B10 (__fastcall, ECX = this)
     *
     * Initializes state fields at +0xE8, +0x270, +0x278, +0x27E, +0x280,
     * +0x284. Creates 11 ButtonSprite objects:
     *   +0x298: resource 0x3d89 (close button)
     *   +0x2C0: resource 0x3d8b (option button)
     *   +0x29C[0..8]: resources 0x3da4..0x3dac (9 status sprites)
     *
     * Called from: constructor
     */
    void draw_sprites();

    /**
     * init_sprites — Initialize the 11 sprite surfaces and overlays.
     * Address: 0x431270 (__fastcall, ECX = this; Ghidra labels it
     * BuildingPanel_InitSprites but it operates on this class's fields)
     *
     * Guarded by sprites_created (+0x27E). Loads overlay resources
     * 0x3d87/0x3d88 (+0x28C/+0x294), then calls Sprite_Init
     * (ButtonSprite::init) on the close sprite (+0x298), the options
     * sprite (+0x2C0) and all 9 status sprites (+0x29C). Sets
     * sprites_created = 1.
     *
     * Called from: show() (0x430D70)
     */
    void init_sprites();

    /**
     * init_background — Lazy-initialize the preview background resource.
     * Address: 0x430C20 (__fastcall, ECX = this)
     *
     * Loads resource 0x3d8a via g_resmgr.GetById, stores at +0x284
     * (background_resource), calls resource vtable[1] (Lock(0,0)) to get
     * the surface at +0x280 (background_surface). Guarded by
     * background_resource != 0 check.
     *
     * Called from: CGWND_InitMode1 @ 0x408481, 0x4085A8
     */
    void init_background();

    /**
     * show — Show the preview dialog (vtable[2]).
     * Address: 0x430D70 (__thiscall; dispatched by CGWND_SetMode(9))
     *
     * Verified from raw x86 bytes. Sequence:
     *   if (g_netman->m_gameMode (+0x7C4) != 2) { CGWND_SetMode(3); return; }
     *   init_sprites() (0x431270) -> vtable[7] (0x430FE0, not yet
     *   decompiled; UI_WindowBase has no C++ virtual for slot [7]) ->
     *   flag_27D = 0 -> UI_WindowBase::show() ->
     *   NETMAN_ReceiveFileTransfer(g_netman) (0x440310) ->
     *   ShowWindow(hWnd, SW_MAXIMIZE) -> hide OS cursor ->
     *   SetFocus(hWnd) -> timerId = SetTimer(hWnd, 0x4D, 120ms, NULL).
     */
    void show() override;

    /**
     * hide — Hide the dialog and destroy sprite surfaces (vtable[1]).
     * Address: 0x430E00 (__fastcall, ECX = this, PostcardPreviewWindow_Cleanup)
     *
     * Clears flag_27D, hides the window, sends a network ack when
     * g_netman->m_gameMode (+0x7C4) == 2, restores focus to the main
     * window, invalidates the viewport rect, kills the timer, then
     * destroys the surface side of all 11 sprites (Sprite_Destroy) and
     * clears sprites_created.
     */
    void hide() override;
};

/* ================================================================== */
/* Global instances                                                     */
/* ================================================================== */

/* Global postcard preview window instance — created by
 * CGWND_InitAllSubsystems as `new PostcardPreviewWindow(hInst, 0x1F7)`
 * and stored in the void* global g_postcard_send (0x4FD388; also
 * declared untyped in input/Cursor_internal.h — global mangling is by
 * name, so the typed view below aliases it). */
extern PostcardPreviewWindow* g_postcard_send;  /* 0x4FD388 */
