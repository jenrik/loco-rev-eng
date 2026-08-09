/**
 * UIEntity.h — World-positioned UI entity with tooltip support
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * UIEntity is a GameObject-derived class that places itself at specific
 * world coordinates based on a direction code. The class supports 7
 * placement modes: Center, Down, Push-right, Random, Spawn-left, Up,
 * and West. It manages an animation variant (1-4, either fixed or
 * random) and creates a tooltip if the associated resource has one.
 *
 * Size: ~0xA4 bytes (extends GameObject base of ~0x84 bytes)
 * Vtable: 0x477A90 (VTBL_00477A90)
 *
 * Class hierarchy:
 *   GameObject (vtable 0x477820)
 *     └─ Entity (vtable 0x477488)
 *          └─ UIEntity  ← this class
 *
 * Direction codes:
 *   0x43 ('C') = Center — place at world center
 *   0x44 ('D') = Down — place below a reference position (scatter downward)
 *   0x50 ('P') = Push right — place pushing right from reference
 *   0x52 ('R') = Random — fully random placement within world bounds
 *   0x53 ('S') = Spawn left — spawn along left edge (scatter leftward)
 *   0x55 ('U') = Up — place above a reference position (scatter upward)
 *   0x57 ('W') = West — place to the west (left) of reference
 *
 * Called from: UI_CreateMessageBox (0x423BA5) to position dialog elements
 */

#pragma once

#include "../core/Entity.h"


// Status: TRANSCRIBED
/* ================================================================== */
/* UIEntity class                                                       */
/* ================================================================== */
class UIEntity : public Entity {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /*                                                                   */
    /* Fields +0x00 through +0x84 are inherited from GameObject.         */
    /* ================================================================ */

    /* --- GameObject inherited fields (+0x00..+0x84) --- */
    /* See GameObject.h for offsets 0x00-0x84 */

    /* --- UIEntity-specific fields (+0x88..+0xA3) --- */
    char        direction;          /* +0x88  placement direction code:
                                              'C'=Center, 'D'=Down, 'P'=Push,
                                              'R'=Random, 'S'=Spawn, 'U'=Up,
                                              'W'=West */
    int16_t     field_8A;           /* +0x8A  short — resource-specific param
                                              (stored from constructor param_2) */
    int32_t     worldX;             /* +0x8C  world X coordinate (spawn param
                                              or computed position) */
    int32_t     worldY;             /* +0x90  world Y coordinate (spawn param
                                              or computed position) */
    uint8_t     animVariant;        /* +0x94  animation variant (1..4):
                                              from resource field when type 8,
                                              else random % 3 + 1 */
    void*       pTooltip;           /* +0x98  UI_CreateTooltip pointer (0=no tooltip) */
    int32_t     tooltipOffsetX;     /* +0x9C  tooltip X offset from entity position */
    int32_t     tooltipOffsetY;     /* +0xA0  tooltip Y offset from entity position */

    /* ================================================================ */
    /* Constructor                                                       */
    /* ================================================================ */

    /**
     * UIEntity constructor.
     * Address: 0x422EC0
     *
     * Calls GameObject_BaseCtor, sets vtable to 0x477A90.
     * Computes world position based on the direction code and resource
     * frame data. Animation variant is selected from resource (type 8)
     * or random (otherwise). Creates tooltip if resource has children.
     *
     * @param resourceId  int32 — Resource/type identifier (stored at +0x04)
     * @param param2      int16 — Resource-specific parameter (stored at +0x8A)
     * @param direction   char  — Placement direction code ('C','D','P','R','S','U','W')
     * @param x           int32 — World X / spawn parameter
     * @param y           int32 — World Y / spawn parameter
     */
    UIEntity(int32_t resourceId, int16_t param2, char direction,
             int32_t x, int32_t y);

    /**
     * Destructor. Destroys the tooltip child (if any) via
     * UI_DestroyTooltip; base-class cleanup (Entity/GameObject) runs
     * automatically through the compiler-generated destructor chain.
     * Address: 0x423500 (body, Ghidra label "UI_Window_Dtor" — a
     * misnomer; this is a destructor, not a drawing function). Scalar
     * deleting destructor wrapper (vtable[0]): 0x4234E0.
     *
     * Correction: the vtable at 0x477A90 is NOT a bare GameObject-shaped
     * table with only a destructor — it is a full 15-slot Entity-shaped
     * table (confirmed via a live slot-by-slot dump) that reuses Entity's
     * function pointers for every slot except three real overrides,
     * reconstructed below as StopSound/SetVisible/Update (2026-08-09;
     * were previously free functions UI_ShowWindow/UI_EnableWindow/
     * UI_HideWindow taking an explicit `self` — see each override's own
     * doc comment for why those names didn't match the real behavior).
     */
    ~UIEntity() override;

    /**
     * StopSound override — vtable[7]. Address: 0x423840 (previously
     * transcribed as the free function UI_ShowWindow — misnamed; this
     * doesn't show anything, it forwards to the tooltip child's own
     * StopSound then calls Entity::StopSound on itself).
     *
     * Propagates to the tooltip child at +0x98 (if any) via its own
     * overridden vtable[7] slot, then calls the base Entity
     * implementation (same address, 0x405A20, as this override's own
     * post-child-handling call in the original binary).
     */
    void StopSound(int param) override;

    /**
     * SetVisible override — vtable[9]. Address: 0x423890 (previously
     * transcribed as the free function UI_EnableWindow — misnamed; it
     * doesn't set a visibility flag).
     *
     * Propagates to the tooltip child's own SetVisible, then pauses/
     * unpauses the game window via CGWND_SetPause — does NOT call
     * Entity::SetVisible (a different address, 0x4061B0; this override
     * replaces the base behavior entirely rather than extending it).
     */
    void SetVisible(bool visible) override;

    /**
     * Update override — vtable[10]. Address: 0x423870 (previously
     * transcribed as the free function UI_HideWindow — misnamed; it
     * doesn't hide anything).
     *
     * Propagates to the tooltip child's own Update, then invalidates
     * this entity's screen rect (GameObject::InvalidateRect, 0x436AB0)
     * — does NOT call Entity::Update (a different address, 0x405C40;
     * this override replaces the base behavior entirely).
     */
    void Update() override;
};
