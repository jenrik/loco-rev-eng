/**
 * VehicleEditor.h — Track-placement editor for vehicle route editing
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: TRANSCRIBED
 *
 * VehicleEditor manages the user interface for placing and editing train
 * track routes during gameplay. It owns two EditorState objects
 * (one for each end of the track segment being placed) and coordinates
 * their movement, collision detection, bridge checks, and rendering.
 *
 * The class also contains DPLAY network data for multiplayer track
 * synchronization (via GetDPlayData/SetDPlayData).
 *
 * Size: ~0x450 bytes (1092 bytes)
 * Vtable: 0x477590
 *
 * Vtable layout:
 *   [0] +0x00: scalar deleting destructor (0x40D660)
 *   [1] +0x04: StopSound / invalidate rect (inherited: 0x405A20)
 *   [2] +0x08: (release resource? — inherited)
 *   [3] +0x0C: SetRenderOffset / update position (0x40D8E0)
 *   [4] +0x10: (unknown — inherited)
 *   [5] +0x14: (unknown — inherited)
 *   [6] +0x18: SetAnimState (inherited: 0x405A50)
 *   [7] +0x1C: StopSound (inherited: 0x405A20) — NOT overridden by
 *              VehicleEditor. Confirmed by a raw read of the vtable data
 *              at 0x477590+0x1C (= 0x405A20 = Entity::StopSound); the
 *              previous "GetResourceId" label here was wrong — GetResourceId
 *              (0x40E0D0) is a plain non-virtual member, not in this vtable.
 *   [8] +0x20: SetFrame (inherited: 0x405DE0)
 *   [9] +0x24: IsInBounds (0x40E250)
 *
 * Class hierarchy:
 *   GameObject (root, type=1)
 *     └─ Entity (type=2)
 *          └─ VehicleEditor  ← this class
 */

#pragma once

#include "Entity.h"

/* EditorState — per-end track editor state machine
   Defined in world/EditorState.h. Size: 0x20 bytes. */
#include "../world/EditorState.h"

class Vehicle;  // forward — see game/Vehicle.h
class DPlayManager;
#ifndef _WIN32
struct HostNetworkEditorTag {};
#endif

class VehicleEditor : public Entity {
public:
    /* Fields at +0x00..+0x87 are inherited from Entity/GameObject —
       see Entity.h and GameObject.h for the base layout. */

    uint8_t   dplay_data[0x398];  // +0x88  DPLAY network sync data (0x398 bytes)
    uint32_t  dplay_trailer;      // +0x420  final 4 bytes from DPLAY data payload
    uint8_t   dplay_initialized;  // +0x424  1 = DPLAY data is valid
#ifndef _WIN32
    DPlayManager* host_dplay_data = nullptr; // native-width owned copy
#endif
    uint8_t   _pad_425[3];        // +0x425
    int32_t   res_id;             // +0x428  copy of resource ID from constructor
    int32_t   res_id_2;           // +0x42C  secondary resource ID (for track? from ctor param_2)
    EditorState* end_a;  // +0x430  editor state for track end A (first placeholder)
    EditorState* end_b;  // +0x434  editor state for track end B (second placeholder)
    uint16_t  angle_frame;        // +0x438  current angle / sprite frame index (computed by CalcAngle)
    uint16_t  second_angle;       // +0x43A  secondary angle component
    uint8_t   unknown_flag;       // +0x43C
    uint8_t   _pad_43D[3];        // +0x43D
    int32_t   edge_dir_a;         // +0x440  edge direction for end A (0=forward, 1=backward, 2=out-of-bounds)
    int32_t   edge_dir_b;         // +0x444  edge direction for end B (0=forward, 1=backward, 2..5=edit edges)
    uint16_t  bound_check_flag;   // +0x448  flag for bound-checking (0/1)
    uint8_t   _pad_44A[2];        // +0x44A
    void*     target_building;    // +0x44C  building being built/modified (or NULL)

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * VehicleEditor constructor (Entity-level constructor with addition).
     * Address: 0x40D500
     *
     * @param res_id   Resource ID for the track/building being placed
     * @param param_2  Secondary resource ID (track variant?)
     * @param flag     Direction flag (0 = forward-first, non-zero = backward-first)
     */
    VehicleEditor(int res_id, int param_2, char flag);
#ifndef _WIN32
    VehicleEditor(HostNetworkEditorTag, int res_id, int param_2, char flag);
#endif

    /**
     * Destructor — releases EditorState objects and DPLAY data.
     * Address: 0x40D680 (body), 0x40D660 (scalar deleting wrapper, vtable[0])
     */
    virtual ~VehicleEditor();

    /* ================================================================ */
    /* VehicleEditor methods                                            */
    /* ================================================================ */

    /**
     * InitTracks — Initialize both editor ends at a given position.
     * Address: 0x40D890
     *
     * @param x  World X position
     * @param y  World Y position
     * @return  1 on success, 0 on failure
     */
    uint32_t InitTracks(int x, int y);

    /**
     * SetRenderOffset (vtable[3]) — Recompute screen rect from end A's
     * track position and resource tile offsets. After updating the rect,
     * dispatches vtable[3] (HitTest/position update) with new left/top.
     *
     * NOTE: Resource is loaded from the repurposed Entity parent slot
     * at +0x40 (set during GameObject_BaseCtor).
     * Address: 0x40D8E0
     */
    void SetRenderOffset();

    /**
     * ProcessMove — Per-frame update for vehicle dragging on track.
     * Address: 0x40D940
     *
     * Called every frame while editing. Updates both EditorState positions,
     * handles track-follow, bounds checking, bridge/edge detection, and
     * road-to-building transitions.
     *
     * @param vehicle  Vehicle being dragged
     */
    void ProcessMove(Vehicle* vehicle);

    /**
     * MoveAlongTrack — Advance both editor ends along track one step.
     * Address: 0x40DC20
     *
     * @param vehicle  Vehicle being moved
     * @return  non-zero on success
     */
    uint32_t MoveAlongTrack(Vehicle* vehicle);

    /**
     * CheckBridge — Bridge edge collision detection dispatch.
     * Address: 0x40DB90
     *
     * @param vehicle  Vehicle to check
     */
    void CheckBridge(Vehicle* vehicle);

    /* ================================================================ */
    /* Rendering / frame management                                      */
    /* ================================================================ */

    /**
     * CalcAngle — Compute sprite angle from the two editor end positions.
     * Address: 0x40DF80
     *
     * Uses atan2 to compute the angle of the vector from end_b to end_a
     * in screen coordinates, then maps the result to 128 sprite frame
     * indices (0-127, wrapping at 128) for a full rotation.
     *
     * The original x87 FPU code uses FPATAN (partial arctangent) with
     * absolute deltas and manual quadrant adjustment. The calling
     * convention passes an unused stack argument from vehicle[+0x08]
     * (RET 4 at function exit), but the parameter is not used by
     * the function body.
     */
    void CalcAngle();

    /**
     * TriggerSound — Play the sound associated with the current frame.
     * Address: 0x40E130
     */
    void TriggerSound();

    /**
     * BlitBackground — Blit the track sprite background to the primary surface.
     * Address: 0x40E160
     *
     * @param clip_x  Screen clip X offset
     * @param clip_y  Screen clip Y offset
     * @return  1 if blitted, 0 if fully clipped
     */
    uint32_t BlitBackground(int clip_x, int clip_y);

    /**
     * IsInBounds (vtable[9]) — Test if a 16x16 point intersects the editor rect.
     * Address: 0x40E250
     *
     * @param x     Grid X coordinate
     * @param y     Grid Y coordinate
     * @param flag  Bound check flag to match
     * @return  1 if in bounds, 0 otherwise
     */
    uint32_t IsInBounds(short x, short y, short flag);

    /**
     * CheckEdgeBounds — Detect when vehicle reaches world edge.
     * Address: 0x40E2A0
     *
     * @param vehicle  Vehicle being edited
     * @return  0
     */
    uint32_t CheckEdgeBounds(Vehicle* vehicle);

    /**
     * CheckVehicleAttach — Auto-attach logic for road/vehicle alignment.
     * Address: 0x40E340
     *
     * @param vehicle  Vehicle to check
     */
    uint32_t CheckVehicleAttach(Vehicle* vehicle);

    /**
     * CheckEditBounds1 — First-stage edit bounds check (bridge approach).
     * Address: 0x40E440
     *
     * @param vehicle  Vehicle being edited
     * @return  non-zero if callback was triggered
     */
    uint32_t CheckEditBounds1(Vehicle* vehicle);

    /**
     * CheckEditBounds2 — Second-stage edit bounds check (bridge retreat).
     * Address: 0x40E520
     *
     * @param vehicle  Vehicle being edited
     * @return  non-zero if callback was triggered
     */
    uint32_t CheckEditBounds2(Vehicle* vehicle);

    /**
     * GetResourceId (non-virtual — see the vtable[7] correction above) —
     * Returns the resource ID if track resource loaded.
     * Address: 0x40E0D0
     */
    uint32_t GetResourceId();

    /**
     * GetDPlayData — Get pointer to DPLAY network data if initialized.
     * Address: 0x40D750
     *
     * @return  Pointer to +0x88 if flag at +0x424 is set, or NULL
     */
    DPlayManager* GetDPlayData();

    /**
     * SetDPlayData — Copy network data into the editor's DPLAY buffer.
     * Address: 0x40D770
     *
     * @param data  Incoming DPLAY network data buffer
     * @return  non-zero on success
     */
    int SetDPlayData(const DPlayManager* data);
};

