/**
 * GameVehicle.h — Vehicle destination management class
 * Status: INTEGRATED
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * GameVehicle manages the assignment of vehicles (road vehicles/trains) to
 * building/track-node targets. It maintains a singly-linked list of pending
 * destination vehicles at +0x124 and tracks the currently assigned vehicle
 * at +0x120. The class provides methods for starting movement, updating
 * the destination queue, adding/removing destinations, and per-frame update.
 *
 * The vtable at +0x00 (0x477848, VTBL_GAMEVEHICLE) has 15 slots matching
 * the Entity layout:
 *   [0]  +0x00: scalar deleting destructor (0x4128B0) — OVERRIDDEN
 *   [1]  +0x04: InvalidateRect (0x436AB0) — inherited (Entity)
 *   [2]  +0x08: PtInRect        (0x436A10) — inherited (Entity)
 *   [3]  +0x0C: MoveTo          (0x405C00) — inherited (Entity)
 *   [4]  +0x10: InvokeCallback1 (0x436AE0) — inherited (Entity)
 *   [5]  +0x14: InvokeCallback2 (0x436B00) — inherited (Entity)
 *   [6]  +0x18: InitBase        (0x405900) — inherited (Entity)
 *   [7]  +0x1C: StopSound       (0x44B130) — inherited (RESDATA_GameVehicle)
 *   [8]  +0x20: SetFrame        (0x405DE0) — inherited (Entity)
 *   [9]  +0x24: SetVisible      (0x4061B0) — inherited (Entity)
 *   [10] +0x28: Update           (0x412A80) — OVERRIDDEN
 *   [11] +0x2C: Draw             (0x4343B0) — inherited (ResourceGameObject
 *               thunk → Entity::Draw at 0x405E60)
 *   [12] +0x30: DrawConnected    (0x405FD0) — inherited (Entity)
 *   [13] +0x34: SetName          (0x405E20) — inherited (Entity)
 *   [14] +0x38: SetAnimState     (0x405A50) — inherited (Entity)
 *
 * Only slots [0] and [10] are overridden by GameVehicle.
 * Slots [7] and [11] originate from RESDATA_GameVehicle overrides.
 * StartMoving is a non-virtual method — it does NOT appear in the vtable.
 *
 * Size: 0x12C bytes (RESDATA_GameVehicle 0x11C + GameVehicle 0x10)
 * Vtable: VTBL_GAMEVEHICLE (0x477848)
 *
 * Class hierarchy (type=4):
 *   GameObject → Entity → ResourceGameObject (type=3)
 *     → RESDATA_GameVehicle (type=4)
 *       → GameVehicle (vtable 0x477848)  ← this class
 *
 * RESDATA_GameVehicle sets vehicle_kind (+0x10C) based on tile type byte at
 * resource+0x63a (1=train, 2=pedestrian, 3=road, 5=fuel, 6=crossing,
 * 7=signal, 8=special). GameVehicle overrides vehicle_kind to 4.
 */

#pragma once

#include "../shared/types.h"
#include "ResdataGameVehicle.h"

class Vehicle;

class GameVehicle : public RESDATA_GameVehicle {
public:
    /* ================================================================ */
    /* Singly-linked list node for destination queue                     */
    /* ================================================================ */

    struct DestNode {
        Vehicle*  vehicle;       // +0x00  Vehicle* pointer
        DestNode* next;          // +0x04  next node (NULL = tail)
    };

    /* ================================================================ */
    /* Fields (offsets from this)                                         */
    /* ================================================================ */

    /* +0x00..+0x11B: RESDATA_GameVehicle fields (inherited), including: */
    /*   +0x40: resource — used for tile type checks                    */
    /*   +0x88: sub_pos_x / sub_pos_y — read as tile_target via method  */
    /*   +0x10C: vehicle_kind — set to 4 by constructor                 */
    /*   +0x110: init_state — animation init state                      */

    /* GameVehicle-specific fields (+0x11C..+0x12B):                      */
    /* occupant_state also takes value 1 in world/EditorState.cpp's
     * UpdateVehiclePlacement (road-tile branch): set to 1 while a vehicle
     * is mid-transition onto this node, checked back against 1 with a
     * direction guard before allowing another vehicle through, and never
     * observed as 2 in that call path. 0/2 (free/occupied) remain the
     * values set by this class's own Update()/World_RenderAll paths; 1 is
     * a third, narrower "claimed by an in-progress placement" state used
     * only by the track editor. */
    int32_t    occupant_state;     // +0x11C 0 = free, 1 = claimed (editor), 2 = occupied
    Vehicle*   current_vehicle;    // +0x120 currently assigned vehicle
    DestNode*  dest_list_head;     // +0x124 head of singly-linked destination queue
    uint8_t    busy_flag;          // +0x128 1 = busy/occupied
    uint8_t    _pad_129[3];        // +0x129 compiler alignment padding
    /* Total object size: 0x12C bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * GameVehicle constructor. 0x412870.
     *
     * Chains to RESDATA_GameVehicle(resource_id) for base initialization.
     * Zeroes all GameVehicle-specific fields, sets vehicle_kind to 4.
     *
     * Called by: INPUT_PlaceObject (0x41DE11).
     *
     * @param resource_id  Resource ID for the vehicle appearance/data
     */
    GameVehicle(int resource_id);

    /**
     * Destructor body. 0x4128D0.
     *
     * NOTE: SEH frame at 0x4128D0 omitted — not portable to GCC.
     *
     * Frees the destination queue linked list at +0x124. Base class
     * destructors (~RESDATA_GameVehicle, ~ResourceGameObject, ~Entity)
     * run automatically after this body.
     */
    ~GameVehicle() override;

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * StartMoving — Assign a vehicle to this target/building (NON-VIRTUAL).
     * Address: 0x4129C0
     *
     * NOT in vtable — called directly.
     *
     * Attempts to assign a vehicle to this building/target. On success:
     *   - Clears occupant state via StopSound(0) if needed
     *   - Marks busy, stores vehicle pointer at +0x120
     *   - Calls vehicle->InitOccupant(1)
     *   - Copies tile_target to vehicle+0x32 (32-bit write spanning
     *     target_tile_x and target_tile_y)
     *   - Returns 1
     *
     * On failure:
     *   - Sets occupant_state = 2 via StopSound(2)
     *   - If vehicle moving: vehicle->Stop(timeout, 1)
     *   - If stationary: vehicle->SetState(1), sets active_flag=1, move_timer=2
     *   - Returns 0
     *
     * @param vehicle  Vehicle* — the road vehicle to assign
     * @return         uint32_t — 1 on success, 0 on failure
     */
    uint32_t StartMoving(Vehicle* vehicle);

    /**
     * Update — Per-frame destination queue processor (vtable[10]).
     * Address: 0x412A80
     *
     * Overrides Entity::Update. Calls Entity::Update() for base animation,
     * then processes the destination queue:
     *   1. If current_vehicle is NULL: dequeues from dest_list_head and
     *      calls vehicle->FindPath to route it here.
     *   2. If current_vehicle is non-NULL but its occupancy is 0: clears
     *      current_vehicle and busy_flag.
     */
    void Update() override;

    /**
     * AddDestination — Enqueue a vehicle for later delivery.
     * Address: 0x412AF0
     *
     * Allocates a DestNode, stores the vehicle pointer, and appends to
     * the tail of the singly-linked list at dest_list_head (+0x124).
     *
     * @param vehicle  Vehicle* — the vehicle to add to the queue
     */
    void AddDestination(Vehicle* vehicle);

    /**
     * RemoveDestination — Remove a vehicle from the queue by player ID.
     * Address: 0x412B50
     *
     * Walks the queue matching vehicle->player_id (+0x7A) and
     * vehicle->color_r (+0x78). Unlinks and frees the matching node.
     * Returns 1 if found, 0 if not.
     *
     * @param player_id  uint16_t — player ID to match
     * @param platform   uint8_t  — platform/color to match
     * @return           uint8_t  — 1 if found and removed, 0 if not
     */
    uint8_t RemoveDestination(uint16_t player_id, uint8_t platform);
};

/* Compile-time size verification */
#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(GameVehicle) == 0x12C,
              "GameVehicle size mismatch (expected 0x12C)");
static_assert(offsetof(GameVehicle, occupant_state) == 0x11C,
              "GameVehicle::occupant_state offset mismatch");
static_assert(offsetof(GameVehicle, current_vehicle) == 0x120,
              "GameVehicle::current_vehicle offset mismatch");
static_assert(offsetof(GameVehicle, dest_list_head) == 0x124,
              "GameVehicle::dest_list_head offset mismatch");
static_assert(offsetof(GameVehicle, busy_flag) == 0x128,
              "GameVehicle::busy_flag offset mismatch");
#endif
