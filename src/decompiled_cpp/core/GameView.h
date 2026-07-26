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
 *   [0]  +0x00: scalar deleting destructor (GameView_Dtor, 0x42D810)
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
 *       GameView_Cleanup (0x42CDD0)
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* ================================================================== */
/* GameView class                                                       */
/* ================================================================== */

class GameView {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* --- Inherited from RESDATA/GameObject/Panel (partial list of key fields) --- */
/* vtable at +0x00 is compiler-managed */
    int32_t    type;                   // +0x04  object type (14 = 0x0E for GameView)
    /* +0x08..+0xAC: RESDATA/GameObject fields */

    uint8_t    scroll_active_flag;     // +0xAD  1 = scroll/scrollbar active (init to 1)

    /* +0xAE..+0xDF: more RESDATA/GameObject fields */

    int32_t    scroll_x;               // +0xE0  scroll offset X (init to 0)
    /* +0xE4: Embedded GameObject sub-object (created via GameObject_BaseCtor) */
    void*      game_object_sub;        // +0xE4  embedded GameObject for sprite management

    /* +0xE8..+0x17B: more Panel fields */

    int32_t    scroll_y;               // +0x17C  scroll offset Y / child reference (init to 0)

    /* Total size: ~0x180 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * GameView constructor.
     * Address: 0x42CCE0 (__fastcall, ECX = this)
     *
     * Initializes the GameView object:
     *   1. Calls RESDATA_BaseInit to initialize RESDATA base
     *   2. Creates GameObject sub-object at +0xE4 via GameObject_BaseCtor
     *   3. C++ construction installs the GameView vtable (0x477D30)
     *   4. Sets type to 14 (0x0E)
     *   5. Initializes scroll_x (+0xE0) = 0, scroll_y (+0x17C) = 0
     *   6. Sets scroll_active_flag (+0xAD) = 1
     *
     * Called from: game timer callback during scene setup
     *
     * @return  Pointer to the constructed object (same as this)
     */
    void* GameView_Ctor();

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x42CD60 (__thiscall)
     *
     * Calls DtorBody to release resources, then optionally frees
     * the heap allocation via GLOBAL_free if flags & 1.
     *
     * @param flags  Delete flag (bit 0 = free heap memory)
     * @return       This pointer (after dtor)
     */
    virtual ~GameView();

    /**
     * Full destructor body.
     * Address: 0x42CD80 (__fastcall, ECX = this)
     *
     * 1. Enters destruction with the compiler-managed GameView vtable
     * 2. Calls GameObject_DtorBody on sub-object at +0xE4
     * 3. Calls Panel_DtorBody for base class cleanup
     */
    void destroy();

    /**
     * Cleanup — Release child resources and call base cleanup.
     * Address: 0x42CDD0 (__fastcall, ECX = this, vtable[15] at +0x3C)
     *
     * 1. Destroys child reference at +0x17C via vtable[0]
     * 2. Cleanups up GameObject at +0xE4 via vtable[6]
     * 3. Cleanups up self via vtable[6]
     * 4. Calls RESDATA_DtorBase
     *
     * Called from: CGWND_Cleanup directly on g_town_view
     */
    void cleanup();
};

/* ================================================================== */
/* Global instances                                                     */
/* ================================================================== */

/* Global GameView instance — created during CGWND_InitAllSubsystems */
extern GameView* g_town_view;  /* 0x4FD374 — global town view object */
