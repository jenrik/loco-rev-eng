/**
 * PostcardPreviewWindow.h — Postcard preview/send dialog
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * PostcardPreviewWindow is the postcard preview/send dialog shown when
 * sending a postcard to another player. It extends UI_WindowBase and
 * manages 11 sprite objects (close button, option button, 9 status
 * sprites) plus background and overlay resources. It is created once
 * during CGWND_InitAllSubsystems and shown/hidden via vtable dispatch.
 *
 * Size: 0x2C4 bytes (708 bytes)
 * Vtable: 0x477E20 (VTBL_POSTCARD_PREVIEW_WINDOW)
 *
 * Class hierarchy:
 *   UI_WindowBase (VTBL_UI_WINDOWBASE, size 0xE8)
 *     └─ PostcardPreviewWindow  <- this class (+0xE8..+0x2C4, total size 0x2C4)
 *
 * Vtable layout (0x477E20, extends UI_WindowBase 12 slots):
 *   [0]  +0x00: scalar deleting destructor (PostcardPreviewWindow_Dtor, 0x430AF0)
 *   [1]  +0x04: Hide / Cleanup          (PostcardPreviewWindow_Cleanup, 0x430E00)
 *   [2]  +0x08: Show / Init             (PostcardPreviewWindow_Show, 0x430C40)
 *   [3]  +0x0C: (inherited stub, 0x425FD0)
 *   [4]  +0x10: (inherited stub, 0x426020)
 *   [5]  +0x14: (inherited stub, 0x426130)
 *   [6]  +0x18: CreateFullWindow        (inherited: UI_CreateFullWindow, 0x425B70)
 *   [7]  +0x1C: OnCreate                (inherited: UI_WindowBase_OnCreate, 0x425D30)
 *   [8]  +0x20: (inherited stub, 0x426130)
 *   [9]  +0x24: (inherited no-op, 0x4661A0)
 *   [10] +0x28: (inherited stub, 0x426140)
 *   [11] +0x2C: WindowProc              (inherited: UI_DefWndProc, 0x422EA0)
 *
 * NOTE: Slots [1] (Hide/Cleanup) and [2] (Show/Init) are overridden
 * by the PostcardPreviewWindow class. The rest are inherited.
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
#include "UI_WindowBase.h"

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

class PostcardPreviewWindow;

/* ================================================================== */
/* PostcardPreviewWindow class                                          */
/* ================================================================== */

class PostcardPreviewWindow : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */
    /* --- Inherited from UI_WindowBase (0x00..0xE8) --- */
    /* +0x00: void*      vtable                                        */
    /* +0x04: HINSTANCE  hInstance                                      */
    /* +0x08: HWND       hWnd                                           */
    /* +0x0C..+0xE7: see UI_WindowBase.h                                */
    /* +0xE4: uint8_t    visible                                        */

    /* --- Preview-window-specific fields (+0xE8..+0x2C4) --- */

    int32_t    field_E8;                // +0xE8  unknown field (zeroed in DrawSprites)
    UINT_PTR   timerId;                 // +0xEC  timer ID for preview dialog timer
    int32_t    field_F0;                // +0xF0  (inherited from UI_WindowBase gap)

    /* Pad +0xF4..+0x26F */
    uint8_t    _pad_F4[0x17C];          // +0xF4 to +0x26F padding

    int32_t    state_field_270;         // +0x270  unknown state field (init to -1 in DrawSprites)
    uint8_t    _pad_274[4];            // +0x274  padding

    int32_t    state_field_278;         // +0x278  unknown state field (init to 0 in DrawSprites)
    uint8_t    _pad_27C[1];            // +0x27C  padding

    uint8_t    flag_27D;                // +0x27D  flag (1 = cleanup needed, cleared in Cleanup)
    uint8_t    sprites_created;         // +0x27E  1 = sprites created (DrawSprites done)
    uint8_t    _pad_27F[1];            // +0x27F  padding

    void*      background_surface;      // +0x280  background surface handle
    void*      background_resource;     // +0x284  background resource (res 0x3d8a)

    void*      overlay_surface_2;       // +0x288  overlay surface 2
    void*      overlay_resource_2;      // +0x28C  overlay resource 2

    void*      overlay_surface_3;       // +0x290  overlay surface 3
    void*      overlay_resource_3;      // +0x294  overlay resource 3

    /* 11 UISprite/ButtonSprite objects (0x24 bytes each) */
    void*      sprite_close;            // +0x298  close button sprite (res 0x3d89)
    void*      sprite_status[9];        // +0x29C  9 status/indicator sprites (res 0x3da4..0x3dac)
    void*      sprite_options;          // +0x2C0  options button sprite (res 0x3d8b)

    /* Total size: 0x2C4 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * PostcardPreviewWindow constructor.
     * Address: 0x430A90 (__thiscall)
     *
     * Calls UI_WindowBase_Ctor(hInstance, resId) to initialize base fields,
     * sets vtable to VTBL_POSTCARD_PREVIEW_WINDOW (0x477E20), then calls
     * DrawSprites to create all 11 preview dialog sprites.
     *
     * Called by: CGWND_InitAllSubsystems @ 0x407107
     *
     * @param hInstance  Application instance handle
     * @param resId      Window resource ID
     */
    PostcardPreviewWindow(HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x430AF0 (__thiscall)
     *
     * Calls DtorBody to release all resources (calls Cleanup + frees
     * individual sprite objects), then optionally frees via GLOBAL_free.
     *
     * @param flags  Delete flag (bit 0 = free heap memory)
     * @return       This pointer (after dtor)
     */
    virtual ~PostcardPreviewWindow();

    /**
     * Full destructor body — releases all resources.
     * Address: 0x430C60 (__fastcall, ECX = this)
     *
     * Restores vtable, releases background resource, destroys all 11
     * sprites (Sprite_Destroy + scalar-deleting dtor for each), then
     * calls UI_WindowBase_BaseDtor for base cleanup.
     */
    void destroy();

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * DrawSprites — Create all 11 preview dialog sprites.
     * Address: 0x430B10 (__fastcall, ECX = this)
     *
     * Initializes state fields at +0xE8, +0x270, +0x278, +0x27E, +0x280,
     * +0x284. Creates 11 ButtonSprite objects:
     *   +0x298: resource 0x3d89 (close button)
     *   +0x2C0: resource 0x3d8b (option button)
     *   +0x29C[0..8]: resources 0x3da4..0x3dac (9 status sprites)
     *
     * Called from: constructor @ 0x430ACE
     */
    void draw_sprites();

    /**
     * InitBackground — Lazy-initialize the preview background resource.
     * Address: 0x430C20 (__fastcall, ECX = this)
     *
     * Loads resource 0x3d8a via ResourceManager_GetById, stores at +0x284
     * (background_resource), calls vtable[1] to get surface at +0x280
     * (background_surface). Guarded by background_resource != 0 check.
     *
     * Called from: CGWND_InitMode1 @ 0x408487, 0x4085AE
     */
    void init_background();

    /**
     * Show — Show and initialize the preview dialog.
     * Address: 0x430C40 (__thiscall, vtable[2])
     *
     * Shows the window via UI_WindowBase_Show, then lazy-initializes
     * background resource via InitBackground. Sets sprites_created flag.
     *
     * Called by: vtable[2] dispatch
     */
    void show() override;

    /**
     * Cleanup — Hide, kill timer, destroy sprites (vtable[1]).
     * Address: 0x430E00 (__fastcall, ECX = this)
     *
     * Called via vtable[1] dispatch. Clears flag_27D, hides window,
     * sends network ack if applicable, restores focus to main window,
     * invalidates viewport, kills timer, destroys 11 sprites via
     * Sprite_Destroy (surface destroy only, not full object free),
     * clears sprites_created flag.
     */
    void cleanup();

    /* ================================================================ */
    /* Static / Non-member helpers                                       */
    /* ================================================================ */

    /**
     * Show — Static wrapper for vtable[2] dispatch.
     * Address: 0x430C40
     *
     * Called from CGWND_InitMode1.
     */
    static void Show(PostcardPreviewWindow* self);
};

/* ================================================================== */
/* Global instances                                                     */
/* ================================================================== */

/* Global postcard preview window instance — created by CGWND_InitAllSubsystems */
extern PostcardPreviewWindow* g_postcard_preview;  /* allocated during startup */
