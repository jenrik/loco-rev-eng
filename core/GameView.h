// Status: INTEGRATED
/**
 * GameView.h — Town viewport scrolling helper class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * GameView (historically "TownGameView / ScrollView") is the viewport
 * scrolling / camera helper embedded at the g_town_view global
 * (0x4852A0).  It is created once during startup and is shared with the
 * town integration (town/Town.h owns the postcard handlers that live in
 * this vtable's town-family slots).
 *
 * Class hierarchy (binary):
 *   GameObject (vtable 0x477820)
 *     └─ Panel (vtable 0x4784C8)
 *          └─ GameView  <- this class (vtable 0x477D30)
 *
 * Vtable layout (0x477D30 — 20 slots, bytes verified against the
 * binary; the compiler-managed C++ vtable is a host-model
 * approximation, the binary slot numbers are authoritative):
 *   [0]  +0x00: scalar deleting destructor (0x42CD60; body 0x42CD80)
 *   [1]  +0x04: Panel::UpdateChild          (0x454890)
 *   [2]  +0x08: GameObject::PtInRect        (0x436A10, inherited)
 *   [3]  +0x0C: GameView/Town slot          (0x42D440)
 *   [4]  +0x10: Town postcard click handler (0x42D670)
 *   [5]  +0x14: Panel slot                  (0x454A60)
 *   [6]  +0x18: Panel::Init                 (0x454680, inherited)
 *   [7]  +0x1C: GameObject::StopSound       (0x405A20, inherited)
 *   [8]  +0x20: GameObject::SetFrame        (0x405DE0, inherited)
 *   [9]  +0x24: GameObject::SetVisible      (0x4061B0, inherited)
 *   [10] +0x28: GameView::Update            (0x42D1A0 — Ghidra label
 *               "Town_TrackBuilding"; per-frame tracking)
 *   [11] +0x2C: Panel::DispatchEvent        (0x454900, inherited)
 *   [12] +0x30: GameObject::DrawConnected   (0x405FD0, inherited)
 *   [13] +0x34: GameObject::SetName         (0x405E20, inherited)
 *   [14] +0x38: GameObject::SetAnimState    (0x405A50, inherited)
 *   [15] +0x3C: GameView::cleanup           (0x42CDD0)
 *   [16] +0x40: Panel::HandleKey            (0x454AE0, inherited)
 *   [17] +0x44: Town postcard command handler (0x42D6B0)
 *   [18] +0x48: Town-family slot            (0x44EF00)
 *   [19] +0x4C: Town-family slot            (0x42D760)
 *
 * NOTE: earlier headers claimed overrides at "SetAnimState (slot 7)",
 * "Draw 0x42F900" and "method_13 0x42D840" — none of those addresses
 * are referenced anywhere; the verified layout above replaces them.
 */

#pragma once

#include "../game/Panel.h"
#include "../core/Entity.h"
#include "../resources/ResourceManager.h"   /* ResourceObject */

class ResourceObject;

/* ================================================================== */
/* GameView class                                                       */
/* ================================================================== */

class GameView : public Panel {
public:
    /* ================================================================ */
    /* GameView-specific fields                                          */
    /*                                                                   */
    /* The binary layout: Panel fields to +0xDF, then:                   */
    /*   scroll_x            +0xE0                                       */
    /*   embedded Entity     +0xE4..+0x16B (constructed by 0x405790)     */
    /*   +0x16C..+0x17B      unmodeled tail (town-side fields)           */
    /*   child_resource      +0x17C                                      */
    /* Total size: 0x180 bytes.  Natural host layout is compiler-managed */
    /* (x86 layout parity is a documentation concern only).              */
    /* ================================================================ */

    int32_t  scroll_x;               /* +0xE0  scroll/camera position */

    /* Embedded Entity sub-object — constructed with Entity(-1,-1,0,0)
     * (0x405790) in the binary; modeled as a real typed member so the
     * compiler manages its lifecycle and virtual dispatch. */
    Entity   game_object_sub;        /* +0xE4 */

    /* +0x16C..+0x17B: town-side fields (not modeled here). */

    ResourceObject* child_resource;  /* +0x17C, released by cleanup() */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * GameView constructor — Address: 0x42CCE0.
     *
     * Binary sequence: RESDATA_BaseInit (Panel base init, 0x4544E0),
     * embedded Entity(-1,-1,0,0) at +0xE4, GameView vtable, type=0x0E,
     * scroll_x=0, +0xAD active flag=1, child_resource=nullptr.  In
     * natural C++ the Panel base and the Entity member are constructed
     * first and the compiler emits the GameView vtable.
     */
    GameView();

    /**
     * GameView destructor — body Address: 0x42CD80.
     *
     * The binary destroys the embedded Entity then runs Panel_DtorBody
     * (0x4545A0).  In natural C++ the Entity member is destroyed after
     * the base chain, so the destructor body itself is empty and the
     * order swap is behavior-neutral (independent resources).
     */
    ~GameView() override;

    /**
     * Cleanup — Address: 0x42CDD0 (vtable[15]).
     *
     * Destroys the child resource (vtable[0] with flag 1), resets the
     * embedded Entity (vtable[6]) and self (Panel::Init, 0x454680),
     * then runs RESDATA_DtorBase (0x454630).
     */
    void cleanup();
};

/* ================================================================== */
/* Global instance                                                     */
/*                                                                     */
/* The town view is an object EMBEDDED at the g_town_view global       */
/* (binary 0x4852A0); the canonical declaration lives in               */
/* world/tilemap.h (extern void* g_town_view) and the host definition  */
/* in shared/stubs_impl.cpp.  The Game methods pass the object address */
/* (&g_town_view) to GameObject::PtInRect / town handlers.             */
/* ================================================================== */
