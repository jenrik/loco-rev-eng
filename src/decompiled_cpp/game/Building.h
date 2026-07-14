/**
 * Building.h — Core Building game object class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Building is the central game entity representing houses, stations,
 * shops, and other structures placed on the map. It extends Entity
 * with occupation, connection, AI movement, and party mode logic.
 *
 * Size: 0xF4 = 244 bytes (allocated in BuildingMgr_CreateFromResource @ 0x4349D0)
 *
 * Class hierarchy:
 *   GameObject (type=1, vtable 0x477820, +0x00..+0x3F)
 *     → Entity    (type=2, vtable 0x477488, +0x00..+0x86)
 *       → Building (type=7, vtable 0x477EB8, +0x00..+0xF3)
 *         → BuildingComplex (vtable 0x478008)
 *         → Train (shares Building_BaseCtor)
 *
 * Dual-vtable pattern:
 *   Building_BaseCtor sets intermediate vtable 0x477F18
 *   Building_Ctor overrides with full vtable 0x477EB8
 *   This ensures virtual dispatch works correctly during construction.
 */

#pragma once

#include "../core/Entity.h"

class Building : public Entity {
public:
    /* ================================================================ */
    /* Building-specific fields (start at offset +0x88)                  */
    /* Fields at +0x00..+0x87 are inherited from Entity/GameObject       */
    /* ================================================================ */

    uint8_t   occupation_level;      // +0x88  starts at 4, increments to 7
    uint8_t   disabled;              // +0x89  0=active, non-zero=disabled
    uint8_t   _pad_8a[2];            // +0x8A
    void*     occupant_a;            // +0x8C  entity pointer or NULL
    void*     occupant_b;            // +0x90  entity pointer or NULL
    uint32_t  create_time;           // +0x94  g_game_time at construction
    int32_t   conn_building_a;       // +0x98  connected building ID, -1=none
    int32_t   conn_building_b;       // +0x9C  connected building ID, -1=none
    uint32_t  next_action_time;      // +0xA0  game tick for next AI action
    uint32_t  field_a4;              // +0xA4  countdown/state
    int32_t   target_x;              // +0xA8  movement target X, -1=none
    int32_t   target_y;              // +0xAC  movement target Y, -1=none
    int32_t   search_x1;             // +0xB0  search range X1, -1=none
    int32_t   search_y1;             // +0xB4  search range Y1, -1=none
    uint8_t   _pad_b8[12];           // +0xB8  (gap)
    int32_t   prev_target_x;         // +0xC4  previous target X, -1=none
    int32_t   prev_target_y;         // +0xC8  previous target Y, -1=none
    int32_t   dest_x;                // +0xCC  destination X, -1=none
    int32_t   dest_y;                // +0xD0  destination Y, -1=none
    int32_t   waypoint_x1;           // +0xD4  waypoint X1, -1=none
    int32_t   waypoint_y1;           // +0xD8  waypoint Y1, -1=none
    uint32_t  field_dc;              // +0xDC  state flag (movement?)
    uint32_t  field_e0;              // +0xE0
    uint8_t   field_e4;              // +0xE4  byte flag
    uint8_t   _pad_e5[3];            // +0xE5
    int32_t   last_action;           // +0xE8  action code from DecideAction
    void*     occupant_ptr;          // +0xF0  bidirectional occupant link

    /* Resource field offsets (relative to RESDATA*) */
    static const int RESNAME_OFFSET  = 0x14D;  /* custom building name */
    static const int RESCLASS_OFFSET = 0x170;  /* 'M' = residential */

    /* Sub-type constants */
    static const uint8_t SUBTYPE_STATION = 7;

    /* Name generation pools */
    static const int RESIDENTIAL_NAME_COUNT = 0x31;  /* 49+1 = 50 names */
    static const int RESIDENTIAL_NAME_BASE  = 2;
    static const int COMMERCIAL_NAME_COUNT  = 0x0B;  /* 11 names */
    static const int COMMERCIAL_NAME_BASE   = 0x33;  /* 51 decimal */

    /* ================================================================ */
    /* Constructors / Destructors                                        */
    /* ================================================================ */

    /**
     * Full Building constructor.
     * Address: 0x4326F0
     *
     * Calls BaseCtor, overrides vtable to 0x477EB8, sets
     * occupation_level=4, zeros occupant_ptr.
     */
    Building(int resource_id);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x432720
     *
     * Calls BaseDtor for cleanup, frees memory if flags & 1.
     */
    void* scalar_deleting_destructor(byte flags);

    /**
     * Shared base constructor.
     * Address: 0x433A20  (size: 407 bytes)
     *
     * Called by both Building() and Train constructor. Calls Entity(),
     * sets intermediate vtable 0x477F18, initializes all Building
     * fields, generates random name or copies resource name,
     * handles PARTY mode easter egg trigger.
     */
    Building* BaseCtor(int resource_id);

    /**
     * Base destructor body (real cleanup logic).
     * Address: 0x432740
     */
    void BaseDtor();

    /**
     * Base destructor wrapper (restores vtable before cleanup).
     * Address: 0x4327A0
     */
    void BaseDtorWrapper();

    /**
     * Base cleanup — removes from parent, entity-level cleanup.
     * Address: 0x432770
     */
    void BaseCleanup();

    /* ================================================================ */
    /* Per-frame update                                                  */
    /* ================================================================ */

    /**
     * Main Building update tick.
     * Address: 0x4327B0
     *
     * Skips normal AI when party mode is active (dispatches to
     * vtable[0x5C] instead). Otherwise runs occupation/action
     * state machine.
     */
    void Update();

    /* ================================================================ */
    /* Occupant management                                               */
    /* ================================================================ */

    /**
     * Add an occupant to this building.
     * Address: 0x432BB0
     */
    void AddOccupant(void* entity);

    /**
     * Remove an occupant from this building.
     * Address: 0x432D50
     */
    void RemoveOccupant(void* entity);

    /* ================================================================ */
    /* AI / Movement                                                     */
    /* ================================================================ */

    /**
     * Calculate movement target coordinates.
     * Address: 0x432DA0
     */
    void CalcMoveTarget();

    /**
     * Check if action timeout has elapsed.
     * Address: 0x432F90
     */
    int CheckTimeout();

    /**
     * Decide next action based on building state.
     * Address: 0x432FE0
     *
     * Action codes stored in last_action (+0xE8).
     */
    void DecideAction();

    /**
     * Find a nearby object matching criteria.
     * Address: 0x4333B0
     */
    void* FindNearbyObject(int search_type);

    /**
     * Find a path from current position to target.
     * Address: 0x433510
     */
    int FindPathToTarget();

    /**
     * Move building occupant toward target.
     * Address: 0x433950
     */
    void MoveToTarget();

    /**
     * Execute the current action.
     * Address: 0x434100
     *
     * In party mode, suppresses random wander timer reset for case-3.
     */
    void HandleAction();

    /* ================================================================ */
    /* Serialization                                                     */
    /* ================================================================ */

    /**
     * Deserialize building state from save data.
     * Address: 0x435630  (large function: ~18K bytes in source)
     */
    void Deserialize(void* data);

    /* ================================================================ */
    /* Animation updates                                                 */
    /* ================================================================ */

    /**
     * Update animation based on building dimensions.
     * Address: 0x433CD0
     */
    void UpdateAnimByDimensions();

    /**
     * Update animation based on occupancy level.
     * Address: 0x433F60
     */
    void UpdateAnimByOccupancy();
};
