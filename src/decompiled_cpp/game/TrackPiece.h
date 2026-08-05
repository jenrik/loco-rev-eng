/**
 * TrackPiece.h — Track piece sprite for route editing visualization
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * TrackPiece is a lightweight GameObject subclass used to render and
 * manage a single track piece sprite during route editing. It handles
 * zoom-level rendering, frame animation, hit-test positioning, and
 * coordinate recalculation for track placement.
 *
 * Size: ~0x58 bytes (88 bytes)
 * Vtable: 0x477568
 *
 * Vtable layout:
 *   [0] +0x00: scalar deleting destructor  (0x40D020)
 *   [1] +0x04: InvalidateRect              (inherited: 0x436AB0)
 *   [2] +0x08: PtInRect                    (inherited: 0x436A10)
 *   [3] +0x0C: HitTest dispatch            (TrackPiece_HitTest, 0x43E9A0)
 *   [4] +0x10: (unknown — inherited)
 *   [5] +0x14: (unknown — inherited)
 *   [6] +0x18: SetFrame                    (TrackPiece_SetFrame, 0x40D2A0)
 *   [7] +0x1C: UpdateAnim                  (TrackPiece_UpdateAnim, 0x40D2F0)
 *   [8] +0x20: Render                      (TrackPiece_Render, 0x40D340)
 *
 * Note: Init() at 0x40D0B0 is NOT a virtual method. It is called
 * directly from the constructor only.
 *
 * Class hierarchy:
 *   GameObject (root, type=1)
 *     └─ TrackPiece  ← this class (type field +0x04 = 7)
 */

#pragma once

#include "../core/GameObject.h"


// Status: TRANSCRIBED
class TrackPiece : public GameObject {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* Inherited from GameObject — see GameObject.h */
    /* +0x00: vtable (0x477568)                                         */
    /* +0x04: type (set to 7 in constructor)                            */
    /* +0x08: screen_rect (RECT, 16 bytes)                              */
    /* +0x18: initialized flag                                          */

    void*     town_ptr;            // +0x24  pointer to Town/GameView manager
    int32_t   sub_resource;        // +0x28  child/helper object (released in _Dtor)
    uint16_t  flags;               // +0x2C  flags (param_3 from Init)
    uint16_t  _pad_2E;             // +0x2E
    int32_t   field_30;            // +0x30  (initially 0, unused)
    RECT      source_rect;         // +0x34  source pixel rect into sprite sheet
                                   //   left   = +0x34
                                   //   top    = +0x38
                                   //   right  = +0x3C
                                   //   bottom = +0x40
    RESDATA*  resource;            // +0x44  pointer to RESDATA (param_2 from Init)
    int16_t   zoom_level;          // +0x48  zoom level (1=normal, 2/3/4=zoomed)
    uint16_t  _pad_4A;             // +0x4A
    int32_t   current_frame;       // +0x4C  current animation frame index (set by SetFrame)
    int32_t   anim_tick;           // +0x50  animation tick counter (incremented by UpdateAnim,
                                   //          reset to 0 by SetFrame)
    uint16_t  prev_frame;          // +0x54  previous frame index, initialized to 0xFFFF
    uint8_t   render_enabled;      // +0x56  1 = render this piece (default, set to 1)
    uint8_t   _pad_57;             // +0x57

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * TrackPiece constructor.
     * Address: 0x40CFA0
     *
     * Calls GameObject() (0x4369D0), then initializes TrackPiece fields:
     * sets vtable to 0x477568, zeroes TrackPiece-specific fields, sets
     * type=7, initializes geometry via non-virtual Init().
     *
     * Called by: RESDATA_CreateChildSprite (0x45476C, 0x4547DE),
     *            RESMGR_SoundObject_Ctor (0x448F5E)
     *
     * @param town    Pointer to Town/GameView manager (+0x24)
     * @param res     Pointer to RESDATA resource (+0x44)
     * @param flags   Initial flags (+0x2C)
     */
    TrackPiece(void* town, RESDATA* res, uint16_t flags);

    /**
     * Virtual destructor (body at 0x40D040).
     *
     * Marks object dead, releases sub_resource (+0x28) via its scalar
     * deleting destructor, then marks dead again. The scalar deleting
     * destructor wrapper (0x40D020) and conditional operator delete
     * are compiler-generated.
     */
    virtual ~TrackPiece();

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Init — Initialize the track piece with geometry (NON-VIRTUAL).
     * Address: 0x40D0B0
     *
     * Called only from the constructor (direct call, not vtable dispatch).
     * Sets town_ptr and resource, computes screen_rect and source_rect
     * from resource data, sets zoom_level=1 and flags. If flags & 2
     * (edge piece), adjusts camera X offset in town manager.
     *
     * @param town    Pointer to Town/GameView manager
     * @param res     Pointer to RESDATA resource
     * @param flags   Initial flags
     */
    void Init(void* town, RESDATA* res, uint16_t flags);

    /**
     * SetZoom — Change display zoom level.
     * Address: 0x40D170
     *
     * Validates the zoom level and applies it:
     *   Zoom 1: calls SetFrame(0)
     *   Zoom 2: calls SetFrame(1) if frame_count<3, else SetFrame(count-2)
     *   Zoom 3: calls SetFrame(count-1) — last frame
     *   Zoom 4: recalcs coordinates with pixel offset and recursively
     *           calls SetZoom on a sub-object to zoom level 2
     *
     * @param zoom  New zoom level (1-4)
     */
    void SetZoom(short zoom);

    /**
     * SetFrame (vtable[6]) — Set the current animation frame.
     * Address: 0x40D2A0
     */
    virtual void SetFrame(int frame);

    /**
     * UpdateAnim (vtable[7]) — Advance animation tick and change frame.
     * Address: 0x40D2F0
     */
    virtual uint8_t UpdateAnim();

    /**
     * Render (vtable[8]) — Draw track piece sprite to screen.
     * Address: 0x40D340
     */
    virtual void Render();

    /**
     * RecalcRect — Recalculate screen rect from resource coordinates.
     * Address: 0x40D470
     *
     * Converts resource's grid-relative coordinates (+0x2e, +0x30)
     * into pixel positions (multiplied by 0x39 = 57 pixels per tile),
     * applies offsets for resource alignment, and updates screen_rect.
     * Also adjusts manager X offset if flags & 2.
     */
    void RecalcRect();
};
