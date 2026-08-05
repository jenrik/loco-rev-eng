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
 *     └─ UIEntity  ← this class
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
     * Virtual destructor stub (vtable[0]).
     * Address: 0x4234A8 (jump table), actual dtor at 0x4234C0
     *
     * Note: The vtable at 0x477A90 has only a scalar deleting destructor
     * and likely no overridden virtual methods (defaults to GameObject).
     */
    virtual ~UIEntity();
};
