/**
 * TrainStationWindow.h — Train station dispatch dialog
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * TrainStationWindow is the train station dispatch dialog shown when
 * clicking on a train station building. It extends GameWindow and
 * manages train dispatch sprites, tooltips, and sound resources.
 *
 * Size: 0x1D4 bytes (468 bytes)
 * Vtable: 0x478130
 *
 * Class hierarchy:
 *   GameWindow  (base, vtable 0x477898, size 0x118)
 *     └─ TrainStationWindow  ← this class (vtable 0x478130)
 *
 * Vtable layout (8 entries, extends GameWindow):
 *   [0] +0x00: scalar deleting destructor  (TrainStationWindow_Dtor,    0x436B40)
 *   [1] +0x04: Hide                        (TrainStationWindow_Hide,    0x436F70)
 *   [2] +0x08: Show                        (TrainStationWindow_Show,    0x436EC0)
 *   [3] +0x0C: set_mode                    (inherited: Cursor_SetMode,  0x414340)
 *   [4] +0x10: method_4                    (inherited: stub,            0x426130)
 *   [5] +0x14: Create                      (TrainStationWindow_Create,  0x436D00)
 *   [6] +0x18: update_client_rect / Init   (inherited,                  0x4140A0)
 *   [7] +0x1C: on_show                     (inherited: stub,            0x426130)
 */

#pragma once

#include "../shared/types.h"
#include "GameWindow.h"

/* ================================================================== */
/* TrainStationWindow class                                             */
/* ================================================================== */

class TrainStationWindow : public GameWindow {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */
    /* --- Inherited from GameWindow (0x00..0x117) --- */

    /* --- TrainStationWindow-specific fields (+0x118..+0x1D4) --- */

    /* +0x118..+0x11B: padding / first custom field area */

    int32_t    field_11C;               // +0x11C  set from Show param_2 (context param)

    uint8_t    field_120;               // +0x120  byte flag (cleared by ctor and Hide)
    uint8_t    _pad_121[3];             // +0x121  padding

    int32_t    train_type;              // +0x124  train type ID (set from Show param_1)

    HICON      hIcon;                   // +0x128  window icon (loaded in Create)

    /* +0x12C..+0x16B: unknown / padding */

    uint8_t    sprites_loaded;          // +0x16C  flag: 1 = sprites have been loaded
    uint8_t    sound_loaded;            // +0x16D  flag: 1 = sound resource loaded
    uint8_t    _pad_16E[2];             // +0x16E  padding

    void*      sprite_ptr_1;            // +0x170  sprite/resource pointer 1 (RESDATA*)
    void*      sprite_ptr_2;            // +0x178  sprite/resource pointer 2 (RESDATA*)
    void*      sprite_ptr_3;            // +0x180  sprite/resource pointer 3 (RESDATA*)
    void*      dest_data_res;           // +0x188  destination data resource (for window sizing)
                                         //         width at +0x14, height at +0x16

    /* +0x18C..+0x18F: padding */

    int32_t    anim_state;              // +0x190  animation state (-1 = inactive)

    /* +0x194..+0x19B: padding */

    UINT_PTR   timer_id;                // +0x19C  window timer ID (200ms, set by Show)

    int16_t    frame_index;             // +0x1A0  sprite frame index (-1 = inactive)
    uint8_t    _pad_1A2[0x16];          // +0x1A2  padding

    int32_t    field_1B8;               // +0x1B8  field (set to 0 in ctor)

    uint8_t    tooltip_active;          // +0x1BC  flag: 1 = tooltip is active
    uint8_t    _pad_1BD[3];             // +0x1BD  padding

    void*      tooltip_ptr;             // +0x1C0  tooltip object pointer (for destroy)
                                        //         also: rect stored at +0x08 from tooltip_ptr

    /* Total size: 0x1D4 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * TrainStationWindow constructor.
     * Address: 0x436B20
     *
     * Calls GameWindow base constructor, zeros all subclass fields
     * (+0x120..+0x1C0), sets vtable to 0x478130, initializes
     * frame_index and anim_state to -1 (inactive).
     *
     * Called by: CGWND_InitAllSubsystems @ 0x4071D0
     *
     * @param hInstance  Application instance handle
     * @param resId      Window resource ID (0x1FC = 508)
     */
    TrainStationWindow(HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x436B40
     *
     * Calls base_destructor(), then optionally frees memory if flags & 1.
     */
    virtual ~TrainStationWindow();

    /**
     * Base destructor body.
     * Address: 0x436B80
     *
     * Restores vtable, calls Hide to release sprites/tooltip/sound,
     * then chains to GameWindow::base_destructor().
     */
    void base_destructor();

    /* ================================================================ */
    /* Virtual methods (overrides)                                       */
    /* ================================================================ */

    /**
     * Create — Load sprites and create the train station window.
     * Address: 0x436D00 (vtable[5])
     *
     * Loads icon resource 0x65, calls Train_LoadSprites, reads window
     * size from dest_data_res (+0x14 = width, +0x16 = height). If
     * sprites_loaded flag is set (reload case), releases old sprites
     * at +0x170, +0x178, +0x180, +0x188 via vtable[2] (Destroy).
     * Centers window on screen via UI_CenterWindow, then calls
     * GameWindow::create with WS_EX_TOPMOST|WS_POPUP (0x86000000) style.
     *
     * @param hWndParent  Parent window HWND
     * @return            true on success, false on failure
     */
    bool Create(HWND hWndParent);

    /**
     * Show — Display the train station dialog.
     * Address: 0x436EC0 (vtable[2])
     *
     * Chains to GameWindow::show(), sets train_type from param_1,
     * calls Train_LoadSprites, fires vtable[6] (update_client_rect),
     * shows HWND + sets focus, creates a 200ms timer (ID 1),
     * activates tooltip via TrainStationWindow_UpdateTooltip.
     * If sound not loaded, loads+plays sound resource 0x50F8.
     *
     * @param train_type   Type of train to dispatch
     * @param context      Context parameter
     */
    virtual void show(int train_type, int context);

    // Override base show() to call the two-param version
    // (the binary uses a different signature for this vtable slot)
    using GameWindow::show;

    /**
     * Hide — Dismiss the train station dialog.
     * Address: 0x436F70 (vtable[1])
     *
     * Chains to GameWindow::hide(), releases sprites if loaded, kills
     * the 200ms timer, resets anim_state/frame_index to -1, deactivates
     * tooltip, destroys tooltip with tilemap invalidation, releases
     * sound resource 0x50F8 if loaded.
     */
    virtual void hide();
};
