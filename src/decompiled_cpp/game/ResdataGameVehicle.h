/**
 * ResdataGameVehicle.h — RESDATA_GameVehicle class definition
 * Status: INTEGRATED
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * RESDATA_GameVehicle is a base class (type=4) extending ResourceGameObject.
 * It represents a game entity that can move along tracks/roads (car, train,
 * pedestrian, etc.). The vtable is at 0x00478308 and the struct is 0x11C bytes.
 *
 * The vehicle_kind at +0x10C identifies the type:
 *   1 = train, 2 = pedestrian, 3 = road vehicle, 5 = fuel pump,
 *   6 = crossing signal, 7 = signal, 8 = special
 *
 * Size: 0x11C bytes (ResourceGameObject 0x10C + 0x10)
 * Vtable: 0x00478308 (VTBL_RESDATA_GAMEVEHICLE)
 *
 * Vtable layout (15 slots, matching Entity):
 *   [0]  +0x00: scalar deleting destructor (0x44B030)
 *   [1]  +0x04: InvalidateRect (0x436AB0) — Entity/GameObject
 *   [2]  +0x08: PtInRect (0x436A10) — Entity/GameObject
 *   [3]  +0x0C: MoveTo (0x405C00) — Entity
 *   [4]  +0x10: InvokeCallback1 (0x436AE0) — Entity/GameObject
 *   [5]  +0x14: InvokeCallback2 (0x436B00) — Entity/GameObject
 *   [6]  +0x18: InitBase (0x405900) — Entity
 *   [7]  +0x1C: StopSound (0x44B130) — OVERRIDDEN
 *   [8]  +0x20: SetFrame (0x405DE0) — Entity
 *   [9]  +0x24: SetVisible (0x4061B0) — Entity
 *   [10] +0x28: Update (0x405C40) — Entity
 *   [11] +0x2C: Draw (0x4343B0) — ResourceGameObject thunk → Entity::Draw
 *   [12] +0x30: DrawConnected (0x405FD0) — Entity
 *   [13] +0x34: SetName (0x405E20) — Entity
 *   [14] +0x38: SetAnimState (0x405A50) — Entity
 *
 * Class hierarchy:
 *   GameObject
 *     → Entity
 *       → ResourceGameObject (type=3, vtable 0x4777D0)
 *         → RESDATA_GameVehicle (type=4, vtable 0x478308)
 *           → GameVehicle (vtable 0x477848)
 */

#pragma once

#include "../shared/types.h"
#include "../core/BuildingMgrObjectGroup.h"

/**
 * RESDATA_GameVehicle — Base class for vehicle-hosting track/buildings.
 *
 * Extends ResourceGameObject with vehicle kind classification and
 * occupant/target state management. Overrides StopSound at vtable[7]
 * to handle pedestrian/signal/train state transitions.
 */
class RESDATA_GameVehicle : public ResourceGameObject {
public:
    /* ================================================================ */
    /* Fields (+0x10C..+0x11B, extending ResourceGameObject's 0x10C)    */
    /* ================================================================ */

    int32_t vehicle_kind;       // +0x10C  vehicle type (1=train, 2=ped, 3=road,
                                //          5=fuel, 6=crossing, 7=signal, 8=special)
    int32_t init_state;         // +0x110  animation init / occupant state
    int16_t counter_timer;      // +0x114  counter/timer field
    uint8_t _pad_116[2];        // +0x116  alignment padding
    int32_t reserved;           // +0x118  unused/reserved

    /* ================================================================ */
    /* Convenience accessor for tile_target at +0x88                     */
    /*                                                                  */
    /* The binary reads/writes a 32-bit dword at +0x88, which spans     */
    /* ResourceGameObject's sub_pos_x (+0x88, int16_t) and sub_pos_y    */
    /* (+0x8A, int16_t). Used by GameVehicle::StartMoving to copy the   */
    /* combined tile coordinate into a Vehicle's target_tile fields.    */
    /* ================================================================ */

    int32_t tile_target() const {
        int32_t val;
        // memcpy avoids strict-aliasing UB from type-punning two int16_t
        // fields as a single int32_t (matches the binary's dword load).
        __builtin_memcpy(&val, &this->sub_pos_x, sizeof(int32_t));
        return val;
    }

    void set_tile_target(int32_t val) {
        __builtin_memcpy(&this->sub_pos_x, &val, sizeof(int32_t));
    }

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * RESDATA_GameVehicle constructor. 0x44AE80.
     *
     * Chains to ResourceGameObject(resource_id). Overrides type to 4,
     * vtable to 0x478308. Reads tile type byte at resource+0x63A to
     * determine vehicle_kind and init_state.
     *
     * @param resource_id  Resource ID for the vehicle appearance/data
     */
    explicit RESDATA_GameVehicle(int resource_id);

    /**
     * Destructor body. 0x44B050.
     *
     * Calls World_DeserializeMap to remove from world grid, then
     * chains to ~ResourceGameObject() for tile-map cleanup.
     */
    ~RESDATA_GameVehicle() override;

    /* ================================================================ */
    /* Virtual methods                                                   */
    /* ================================================================ */

    /**
     * StopSound — State machine transition at vtable[7]. 0x44B130.
     *
     * Overrides Entity::StopSound. Dispatches based on tile type and
     * vehicle_kind:
     *   - Pedestrian tile (0x0B) or signal vehicle (kind=7):
     *       state=0 → init_state=5, Entity::StopSound(0), return
     *       state=1 → init_state=4, fall through to Entity::StopSound(state)
     *   - Train (kind=1):
     *       init_state=state, Entity::StopSound(state), return
     *   - Default: Entity::StopSound(state)
     *
     * NOTE: resource (+0x40) is dereferenced WITHOUT null check.
     */
    void StopSound(int state) override;
};

/* Compile-time size verification */
#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(RESDATA_GameVehicle) == 0x11C,
              "RESDATA_GameVehicle size mismatch (expected 0x11C)");
static_assert(offsetof(RESDATA_GameVehicle, vehicle_kind) == 0x10C,
              "RESDATA_GameVehicle::vehicle_kind offset mismatch");
static_assert(offsetof(RESDATA_GameVehicle, init_state) == 0x110,
              "RESDATA_GameVehicle::init_state offset mismatch");
static_assert(offsetof(RESDATA_GameVehicle, reserved) == 0x118,
              "RESDATA_GameVehicle::reserved offset mismatch");
#endif
