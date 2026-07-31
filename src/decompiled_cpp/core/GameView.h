/**
 * GameView.h — Per-scene viewport scrolling helper class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * GameView (also known as TownGameView / ScrollView) is a helper
 * sub-object that handles viewport scrolling and camera position
 * within a Town scene. It is created once per scene during startup
 * and stored as the global g_town_view. It extends the Panel class
 * (which extends GameObject) and embeds a GameObject sub-object for
 * sprite management.
 *
 * Size: ~0x180 bytes (inherits from Panel/GameObject/RESDATA chain)
 * Vtable address in loco.exe: 0x477D30
 *
 * Class hierarchy:
 *   RESDATA (vtable 0x478274)
 *     └─ GameObject (vtable 0x477820)
 *          └─ Panel (vtable 0x477C88)
 *               └─ GameView  <- this class
 *
 * Vtable layout (0x477D30, extends Panel 15-slot GameObject vtable):
 *   [0]  +0x00: scalar deleting destructor (compiler-generated wrapper, 0x42CD60)
 *   [1]  +0x04: StopSound                (inherited from GameObject)
 *   [2]  +0x08: (inherited stub)
 *   [3]  +0x0C: HitTest dispatch         (inherited stub)
 *   [4]  +0x10: (inherited)
 *   [5]  +0x14: (inherited)
 *   [6]  +0x18: Init / InvalidateRect    (inherited: GameObject::InitBase, 0x405900)
 *   [7]  +0x1C: SetAnimState             (overridden)
 *   [8]  +0x20: SetFrame                 (inherited stub)
 *   [9]  +0x24: SetName                  (inherited stub)
 *   [10] +0x28: Draw                     (overridden: TownGameView_Draw, 0x42F900)
 *   [11] +0x2C: DrawConnected            (inherited stub)
 *   [12] +0x30: OnTimerTick              (inherited stub)
 *   [13] +0x34: method_13                (overridden: TownGameView_Method13, 0x42D840)
 *   [14] +0x38: AnimStateSelect          (inherited stub)
 *   Beyond [14]: GameView-specific cleanup slot at vtable[15] = +0x3C
 *       GameView::cleanup body (0x42CDD0)
 */

#pragma once

#include "../shared/types.h"

class ResourceObject;

/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* ================================================================== */
/* GameView class                                                       */
/* ================================================================== */

class GameView {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* The raw object has a compiler-managed vptr at +0x00.  The unnamed
     * RESDATA/Panel prefix occupies +0x04..+0xDF.  Keep that prefix named
     * and opaque: the embedded object and child pointer are the fields this
     * class actually owns at the recovered offsets. */
    int32_t    type;                   // +0x04  object type (0x0E)
    uint8_t    _panel_prefix[0xA5];    // +0x08..+0xAC
    uint8_t    scroll_active_flag;     // +0xAD  active/drag flag
    uint8_t    _panel_suffix[0x32];    // +0xAE..+0xDF
    int32_t    scroll_x;               // +0xE0

    /* Entity constructed by the original call to 0x405790 at +0xE4.
     * The storage is deliberately opaque because the recovered inline
     * object is an x86 ABI object, not a separately allocated pointer. */
    alignas(uint32_t) uint8_t game_object_sub[0x98]; // +0xE4..+0x17B

    ResourceObject* child_resource;    // +0x17C, released by Cleanup()

    /* Total size: 0x180 bytes on the recovered x86 layout. */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /** GameView constructor — Address: 0x42CCE0 (__fastcall). */
    GameView();

    /** GameView destructor body — Address: 0x42CD80 (__fastcall). */
    ~GameView();

    /** Cleanup — Address: 0x42CDD0 (__fastcall). */
    void cleanup();
};

/* ================================================================== */
/* Global instances                                                     */
/* ================================================================== */

/* Global GameView instance — created during CGWND_InitAllSubsystems */
extern GameView* g_town_view;  /* 0x4FD374 — global town view object */
