// Status: TRANSCRIBED (new vtable method addresses verified, removed _pad_1C/_pad_20)
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
 *     -> Entity    (type=2, vtable 0x477488, +0x00..+0x86)
 *       -> Building (type=7, vtable 0x477EB8, +0x00..+0xF3)
 *         -> Train (shares Building_BaseCtor)
 *
 * loco_v8 does not support the former BuildingComplex-derived-class entry at
 * 0x478008: its functions operate on a pointer collection (+0x04/+0x08), and
 * no code installs that address as a vtable. The legacy BuildingComplex name
 * at 0x434500 is the BuildingMgr core (vtable 0x477F70); see BuildingComplex.h.
 *
 * Vtable layout (Building 0x477EB8, extends Entity 0x477488 [15 slots]):
 *   [0]  +0x00: ~Building() scalar deleting destructor (0x432720)
 *   [1]..[10]: inherited from Entity (InvalidateRect, PtInRect, MoveTo, ...)
 *   [11] +0x2C: Draw(RECT, int, uint32_t) override (0x4343B0)
 *   [12] +0x30: DrawConnected (0x405FD0, inherited from Entity)
 *   [13] +0x34: SetName → SetCustomName (0x4344A0)
 *   [14] +0x38: SetAnimState (0x405A50, inherited from Entity)
 *   [15] +0x3C: Update(void* next_entity) — AI dispatch (0x4327B0)
 *   [16] +0x40: TeleportTo(int x, int y) (0x432940)
 *   [17] +0x44: OnOccupantReady(Entity* entity) (0x434260)
 *   [18] +0x48: StepToward(int x, int y) (0x432AE0)
 *   [19] +0x4C: FindNearestConnectionNode(void*, uint32_t) (0x4343F0)
 *   [20] +0x50: PostMoveDispatch() (0x433CA0)
 *   [21] +0x54: CheckPlacementCollision(int, int) (0x433860)
 *   [22] +0x58: IsActionComplete() -> int (0x432FD0)
 *   [23] +0x5C: PartyModeUpdate(Building* next_building) (0x433220)
 *
 * NOTE: C++ virtual declaration order does not yet match the binary vtable
 * slot order above. The virtual methods are currently interleaved with
 * non-virtual helpers in logical groups (AI, movement, name management).
 * Since no literal vtable dispatch remains in the codebase, this has no
 * runtime effect. Reordering will be completed during the INTEGRATION pass
 * (Pass 3) when the class hierarchy is finalized.
 *
 * Compiler-managed (not in C++):
 *   scalar deleting destructor wrapper at 0x432720
 *   vector deleting destructor wrapper at 0x4327A0 (calls BaseDtor)
 *
 * Dual-vtable pattern (in the binary):
 *   Building_BaseCtor sets intermediate vtable 0x477F18
 *   Building_Ctor overrides with full vtable 0x477EB8
 *   In natural C++, the compiler handles vtable progression automatically
 *   through the constructor chain.
 */

#pragma once

#include "../core/Entity.h"

class Building : public Entity {
public:
    Building(const Building&) = delete;
    Building& operator=(const Building&) = delete;

    /* ================================================================ */
    /* Building-specific fields (start at offset +0x88)                  */
    /* Fields at +0x00..+0x87 are inherited from Entity/GameObject       */
    /* ================================================================ */

    uint8_t   occupation_level;      // +0x88  starts at 4, increments to 7
    uint8_t   disabled;              // +0x89  0=active, non-zero=disabled
    uint8_t   _pad_8a[2];            // +0x8A
    Entity*   occupant_a;            // +0x8C  entity pointer or NULL
    Entity*   occupant_b;            // +0x90  entity pointer or NULL
    uint32_t  create_time;           // +0x94  g_game_time at construction
    int32_t   conn_building_a;       // +0x98  connected building ID, -1=none
    int32_t   conn_building_b;       // +0x9C  connected building ID, -1=none
    uint32_t  next_action_time;      // +0xA0  game tick for next AI action
    uint32_t  field_a4;              // +0xA4  countdown/state
    int32_t   target_x;              // +0xA8  movement target X, -1=none
    int32_t   target_y;              // +0xAC  movement target Y, -1=none
    int32_t   search_x1;             // +0xB0  search range X1, -1=none
    int32_t   search_y1;             // +0xB4  search range Y1, -1=none
    int32_t   track_x;               // +0xB8  current track-follow X position
    int32_t   track_y;               // +0xBC  current track-follow Y position
    int32_t   track_node_id;         // +0xC0  current track connection node ID
    int32_t   prev_target_x;         // +0xC4  previous target X, -1=none
    int32_t   prev_target_y;         // +0xC8  previous target Y, -1=none
    int32_t   dest_x;                // +0xCC  destination X, -1=none
    int32_t   dest_y;                // +0xD0  destination Y, -1=none
    int32_t   waypoint_x1;           // +0xD4  waypoint X1, -1=none
    int32_t   waypoint_y1;           // +0xD8  waypoint Y1, -1=none
    int32_t   field_dc;              // +0xDC  movement distance / action state
    uint32_t  field_e0;              // +0xE0
    uint8_t   field_e4;              // +0xE4  byte flag
    uint8_t   _pad_e5[3];            // +0xE5
    int32_t   last_action;           // +0xE8  action code from DecideAction
    uint32_t  field_ec;              // +0xEC  serialized Building-base field
    Entity*   occupant_ptr;          // +0xF0  bidirectional occupant link

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
     * Calls BaseCtor, sets occupation_level=4, zeros occupant_ptr.
     */
    Building(int resource_id);

    /**
     * Virtual destructor (vtable[0]).
     * Address: 0x432720 (scalar deleting destructor wrapper)
     * Body: calls BaseDtor (0x432740) for cleanup logic.
     */
    ~Building() override;

    /**
     * Shared base constructor.
     * Address: 0x433A20  (size: 407 bytes)
     *
     * Called by both Building() and Train constructor. Calls
     * InitBase() for Entity-level resource init, then initializes
     * all Building-specific fields, generates random name or copies
     * resource name, handles PARTY mode easter egg trigger.
     *
     * @param resource_id  Resource ID to load
     * @param base_only    If true, skip occupant_ptr (+0xF0) init
     *                     (Train subclass is 0xF0 bytes, no occupant_ptr)
     */
    void BaseCtor(int resource_id, bool base_only);

    /**
     * Base destructor body (real cleanup logic).
     * Address: 0x432740
     *
     * Deselects via Game_SelectGameObject if currently selected,
     * removes occupant via RemoveOccupant, then calls BaseCleanup.
     */
    void BaseDtor();

    /**
     * Base cleanup — disconnect from parent, entity-level cleanup.
     * Address: 0x433BE0
     *
     * Reads parent from +0x90 (occupant_b slot used as scene-graph parent during
     * cleanup, NOT the normal +0x40 parent). Searches parent's 5-slot child
     * array at parent+0xA4 for this pointer, clears the slot and decrements
     * child_count at parent+0x8E, then calls GameObject_DtorBody for resource/
     * audio cleanup.
     */
    void BaseCleanup();

    /* ================================================================ */
    /* Per-frame update                                                  */
    /* ================================================================ */

    /**
     * Main Building update tick.
     * Address: 0x4327B0 — Vtable slot [15] (+0x3C), NEW virtual.
     *
     * This does NOT override Entity::Update() (slot [10]) — they occupy
     * distinct vtable slots and have different signatures. The binary
     * calls Entity::Update() internally for animation frame advancement
     * (see CheckTimeout), and Building::Update(void*) for AI dispatch.
     *
     * Skips normal AI when party mode is active (dispatches to
     * PartyModeUpdate, forwarding next_entity). Otherwise runs
     * occupation/action state machine.
     *
     * @param next_entity  Forwarded to PartyModeUpdate for connection
     *                     building lookup; may be null when called from
     *                     contexts without iteration state.
     */
    virtual void Update(void* next_entity);

    // Expose Entity::Update() (vtable slot [10], no params) alongside
    // Building::Update(void*) (vtable slot [15]). Without this using
    // declaration, the parameterless overload is hidden by name hiding.
    using Entity::Update;

    /* ================================================================ */
    /* Occupant management                                               */
    /* ================================================================ */

    /**
     * Add an occupant to this building.
     * Address: 0x433530
     *
     * DECOMPILER NOTE: Searches slots 0-8 (9 slots) in the occupant
     * array at the entity's +0x38. RemoveOccupant only searches slots
     * 0-7 (8 slots) — this appears to be a binary bug where slot 8
     * occupants are never cleaned up. Faithfully reproduced until
     * confirmed intentional.
     */
    void AddOccupant(Entity* entity);

    /**
     * Remove an occupant from this building.
     * Address: 0x4336A0
     *
     * Reads occupant from occupant_ptr (+0xF0), searches the occupant
     * entity's 8-slot tracking array for `this` (the building), clears
     * the slot, calculates exit position based on road type, teleports
     * the building, and sets visible=1.
     *
     * DECOMPILER NOTE: Searches slots 0-7 (8 slots) but AddOccupant
     * searches 0-8 (9 slots). Slot 8 occupants are never found here.
     * This is binary-correct behavior per Ghidra at 0x4336A0.
     */
    void RemoveOccupant();

    /* ================================================================ */
    /* AI / Movement                                                     */
    /* ================================================================ */

    /**
     * Compute stepped movement toward target with Euclidean distance.
     * Address: 0x433DC0
     *
     * @param out_pos   int[2] — output buffer for resulting pixel (x,y)
     * @param target_x  Target X in world coordinates
     * @param target_y  Target Y in world coordinates
     * @param max_step  Maximum pixels to move this step (speed clamp)
     */
    void CalcMoveTarget(int* out_pos, int target_x, int target_y, int max_step);

    /**
     * OnOccupantReady — Vtable slot [17] (+0x44).
     * Address: 0x434260
     *
     * Moves the building toward the given entity. If the entity has a
     * valid bounding rect, picks a random point within it. Otherwise
     * teleports directly to the entity's world position.
     *
     * @param entity  Target entity to move toward, or NULL to cancel
     */
    virtual void OnOccupantReady(Entity* entity);

    /**
     * Check if action timeout has elapsed.
     * Address: 0x433C50
     */
    void CheckTimeout();

    /**
     * Decide next action based on building state.
     * Address: 0x434040
     *
     * Action codes stored in last_action (+0xE8).
     * Returns: 1=occupy, 2=spawn, 3=wander, 0=wait.
     */
    int DecideAction();

    /**
     * Find a nearby object matching criteria.
     * Address: 0x433EC0
     */
    void* FindNearbyObject(int target_type, int x, int y);

    /**
     * Find a path from current position to target.
     * Address: 0x433370
     *
     * Uses this->target_x, this->target_y as destination coordinates.
     * Returns 1 if step stored, 0 if no path.
     */
    int FindPathToTarget();

    /**
     * Move building occupant toward target (non-virtual helper).
     * Address: 0x434399
     *
     * Uses last_action (+0xE8) to determine which occupant to move toward.
     * Called from Update() for occupy/spawn actions.
     */
    void MoveToTarget();

    /**
     * Execute the current action.
     * Address: 0x434100
     *
     * @param action  Action from last_action (+0xE8) to finalize.
     * In party mode, suppresses random wander timer reset for case-3.
     */
    void HandleAction(int action);

    /* ================================================================ */
    /* Serialization                                                     */
    /* ================================================================ */

    /**
     * Deserialize building state from save data.
     * Address: 0x435700
     *
     * Allocates a Building via `new Building(0)`, then overwrites
     * all fields from the flat save image. Returns the constructed
     * Building (or nullptr on allocation failure).
     *
     * In the original binary, this is __thiscall on a manager object
     * which calls manager->vtable[0x28](context, obj) to register.
     * Registration is deferred to the caller.
     */
    static Building* Deserialize(void* data);

    /* ================================================================ */
    /* Animation updates                                                 */
    /* ================================================================ */

    /**
     * Update animation based on building dimensions.
     * Address: 0x4331B0
     */
    void UpdateAnimByDimensions();

    /**
     * Update animation based on occupancy level.
     * Address: 0x433F60
     *
     * Selects animation index (4-7) based on occupation_level (+0x88).
     * Called from Update post-update when idle and visible.
     */
    void UpdateAnimByOccupancy();

    /* ================================================================ */
    /* Name management                                                   */
    /* ================================================================ */

    /**
     * Party mode update handler (vtable slot 23 = +0x5C).
     * Address: 0x433220
     *
     * Called from Update() when g_is_party_mode is active.
     * Increments a counter each tick; when it reaches 3, checks
     * connection buildings for the next party destination.
     *
     * NOTE: Parameter matches the base class vtable signature (void*).
     * BuildingMgr passes a Building* in practice.
     */
    virtual void PartyModeUpdate(void* next_entity);

    /**
     * Check if the current action is complete (vtable slot 22 = +0x58).
     * Address: 0x432FD0
     *
     * Returns 0 if action is done, non-zero if still in progress.
     * Increments a frame counter each tick and checks against the
     * resource's animation duration.
     */
    virtual int IsActionComplete();

    /**
     * Step movement toward a target (vtable slot 18 = +0x48).
     * Address: 0x432AE0
     *
     * Advances dest_x/dest_y one step toward (x, y) following road
     * connectivity. Updates search_x1/search_y1 with tile position.
     */
    virtual void StepToward(int x, int y);

    /**
     * Teleport to position (vtable slot 16 = +0x40).
     * Address: 0x432940
     *
     * Sets target and initiates movement. If occupant exists, calls
     * AddOccupant to transfer; otherwise calls StepToward to begin
     * path-following.
     */
    virtual void TeleportTo(int x, int y);

    /**
     * Vtable slot 20 = +0x50 handler (cleanup/post-move dispatch).
     * Address: 0x433CA0
     *
     * Detaches from scene-graph parent, then attempts to re-attach
     * to a compatible building in the building collection.
     *
     * TODO: decompile BuildingMgr collection iteration at 0x433D98
     * (Step 2 re-attachment loop using BuildingMgr vtable[0x20])
     */
    virtual void PostMoveDispatch();

    /**
     * Collision check for building placement (vtable slot 21 = +0x54).
     * Address: 0x433860
     *
     * Returns 1 if placement at (x,y) is blocked by another building
     * or the placement position is already occupied.
     *
     * TODO: decompile full tile-obstacle check at 0x433860 (Check 4 —
     * frame_width/frame_height footprint validation against map tiles)
     */
    virtual uint8_t CheckPlacementCollision(int x, int y);

    /**
     * Find nearest connection node to target (vtable slot 19 = +0x4C).
     * Address: 0x4343F0
     *
     * Iterates the node set, computing Manhattan distance from each
     * node to the building's target position. Returns the index of
     * the closest reachable node.
     */
    virtual uint32_t FindNearestConnectionNode(void* node_set, uint32_t current_node_id);

    /**
     * Draw override (vtable slot 11).
     * Address: 0x4343B0
     *
     * Forwards to Entity::Draw(RECT, int, uint32_t).
     */
    virtual void Draw(RECT clip_bounds, int enable_scroll, uint32_t extra_flags) override;

    /**
     * SetName override (vtable slot 13).
     * Address: 0x4344A0  (80 bytes)
     *
     * Updates the building's name and performs side effects:
     *   1. Calls Entity::SetName to store the new name
     *   2. If this is a STATION (sub_type == 7), compacts BuildingMgr collections
     *   3. DECOMPILER NOTE: Party mode activates when the name does NOT
     *      contain "PARTY" (inverted check, binary-correct per Ghidra).
     *      Party mode appears to be the default state; naming a building
     *      "PARTY" disables it.
     */
    virtual void SetName(const char* name) override;

    /**
     * Set a custom display name for this building (alias for SetName).
     * Delegates to SetName for the actual implementation.
     */
    void SetCustomName(const char* name);

protected:
    /**
     * Intermediate Building-base construction path used by Train.
     * Address: 0x433A20.  When base_only is true this deliberately does
     * not initialize Building's +0xF0 occupant_ptr tail field.
     * TODO: decompile 0x433A20 with base_only=true path
     */
    Building(int resource_id, bool base_only);
};

#if UINTPTR_MAX == 0xffffffffu
static_assert(offsetof(Building, occupation_level) == 0x88, "Building field base mismatch");
static_assert(offsetof(Building, occupant_a) == 0x8C, "Building occupant_a offset mismatch");
static_assert(offsetof(Building, field_dc) == 0xDC, "Building field_dc offset mismatch");
static_assert(offsetof(Building, occupant_ptr) == 0xF0, "Building occupant_ptr offset mismatch");
static_assert(sizeof(Building) == 0xF4, "Building must match the 32-bit loco.exe layout");
#endif
