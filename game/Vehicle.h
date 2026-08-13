/**
 * Vehicle.h — Vehicle base class for moving game objects (cars/trucks/buses)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Vehicle is a standalone 0x94-byte object (vtable VTBL_VEHICLE at 0x47836C)
 * representing a road vehicle that moves along the track network. It manages
 * position, speed, occupant loading/unloading, engine sounds, route navigation,
 * and multiplayer synchronization.
 *
 * Each Vehicle owns up to 4 VehicleEditor sub-objects (0x450 bytes each) that
 * handle per-vehicle movement along the grid. A GAMESTATE_EditorState (0x20
 * bytes) manages the shared editor state for position tracking.
 *
 * Size: 0x94 bytes
 * Vtable: 0x0047836C (VTBL_VEHICLE)
 *
 * Vtable layout (identical to VTBL_SCRIPTED_OBJECT):
 *   [0]  +0x00: scalar deleting destructor (RESDATA_ScriptedObject_DtorList, 0x44C0B0)
 *   [1]  +0x04: method_1 / cleanup
 *   [2]  +0x08: PtInRect / CheckClick
 *   [3]  +0x0C: MoveTo / HitTest dispatch
 *   [4]  +0x10: method_4
 *   [5]  +0x14: method_5
 *   [6]  +0x18: InitBase
 *   [7]  +0x1C: SetAnimState / EnterBuildState
 *   [8]  +0x20: SetFrame / update callback
 *   [9]  +0x24: SetName
 *   [10] +0x28: Draw / Update
 *   [11] +0x2C: DrawConnected / Dispatch
 *   [12] +0x30: OnTimerTick
 *   [13] +0x34: method_13
 *   [14] +0x38: AnimStateSelect
 *
 * Note: Vehicle is NOT derived from GameObject or Entity. It has its own
 * independent struct layout and vtable. The destructor function name
 * (RESDATA_ScriptedObject_DtorList) suggests it is related to ScriptedObject
 * but with a specialized list-based cleanup pattern.
 *
 * State machine (field state at +0x5C):
 *   0 = STOPPED, 1 = APPROACHING, 2 = MOVING, 3 = WAITING, 4 = STOPPING
 *
 * Occupancy modes (field occupancy at +0x64):
 *   0 = EMPTY, 1 = DEPARTING, 2 = FULL, 4 = STOPPING, 5 = ARRIVING
 *
 * Direction modes (field direction at +0x60):
 *   0 = FORWARD, 1 = REVERSE, 2 = EDGE_OF_MAP, 3 = DEPOT, 4 = ALT_FRONT
 */

#pragma once

#include "../shared/types.h"


// Status: TRANSCRIBED
class VehicleEditor;
class EditorState;
class DPlayManager;
#ifndef _WIN32
struct HostNetworkVehicleTag {};
#endif

/* RESDATA_IsRoadTile/IsBuildingTile (0x44BD10/0x44BD30) take int32_t, the
 * original x86 ABI's pointer width -- a real 64-bit host pointer does not
 * survive that round-trip (game/Vehicle.cpp's Vehicle::LoadSounds/IsMoving
 * both hit this). `resource` is also a loco::assets::SpriteResource* on
 * this build, not the real x86 TileMapResource* those functions' raw
 * +0x63A read assumes (the "raw fixed-offset reads against undersized
 * host resource objects" landmine already fixed in ScrollRect/
 * InputMgr.cpp/ResdataGameVehicle.cpp). On host, checks the resolved
 * tile_type byte against their own documented match sets ({1,2,3,4}
 * road, {7,8,9,10} building) directly; under _WIN32, calls the real
 * int32_t-signature functions unchanged. Shared by game/Vehicle.cpp,
 * core/VehicleEditor.cpp, and world/EditorState.cpp -- all three read
 * this same resource classification off a real placed vehicle/editor. */
void ClassifyResourceTile(void* resource, bool* is_road, bool* is_building);

/* ================================================================ */
/* Vehicle — 0x94-byte standalone class with VTBL_VEHICLE           */
/* ================================================================ */
class Vehicle {
public:
    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;

    /* ================================================================ */
    /* Fields (offsets from this pointer)                                 */
    /* ================================================================ */

    /* vtable at +0x00 is compiler-managed via virtual methods */
    int32_t   owner_handle;        // +0x04  owner/creator parameter
    int32_t   active_editor;       // +0x08  0=forward, 1=reverse (direction toggle)
    int16_t   editor_count;        // +0x0C  number of active VehicleEditor refs (0-4)
    /* +0x0E: padding 2 bytes */
    VehicleEditor* editors[4];     // +0x10  VehicleEditor pointers (max 4)
    EditorState* editor_state;      // +0x20  GAMESTATE_EditorState (0x20-byte sub-object)
    int16_t   max_speed;           // +0x24  forward speed limit
    int16_t   reverse_speed;       // +0x26  reverse speed limit
    int32_t   stop_timer;          // +0x28  stop state countdown
    uint8_t   detach_flag;         // +0x2C  detachment flag
    /* +0x2D: padding 1 byte */
    int16_t   tile_x;              // +0x2E  current tile X position
    int16_t   tile_y;              // +0x30  current tile Y position
    int16_t   target_tile_x;       // +0x32  destination tile X
    int16_t   target_tile_y;       // +0x34  destination tile Y
    int16_t   move_timer;          // +0x36  movement timer countdown
    int32_t   occupant_tracks[8];  // +0x38  8 track/slot entries
    int16_t   max_steps;           // +0x58  max movement steps per frame
    uint8_t   sound_guard;         // +0x5A  reentrancy guard for engine sound
    /* +0x5B: padding 1 byte */
    int32_t   state;               // +0x5C  movement state (0=STOPPED..4=STOPPING)
    int32_t   direction;           // +0x60  travel direction (0=forward..4=ALT_FRONT)
    int32_t   occupancy;           // +0x64  occupancy mode (0=empty..5=arriving)
    int32_t   net_sync_flag;       // +0x68  0=no sync, 1=needs sync, 2=synced
    int8_t    msg_box_count;       // +0x6C  message box counter
    /* +0x6D-0x6F: padding 3 bytes */
    union {
        Vehicle* network_next;
        Vehicle* next;
    };                              // +0x70  inbound-list link (x86 pointer)
    uint16_t  tunnel_angle;        // +0x74  inbound tunnel direction/timeout
    uint16_t  field_76;            // +0x76  inbound metadata
    union { uint8_t color_r; uint8_t slot_index; }; // +0x78
    uint8_t   _pad_79;             // +0x79
    union { uint16_t player_id; uint16_t network_id; }; // +0x7A
    union { uint8_t color_g; uint8_t peer_index; }; // +0x7C
    uint8_t   _pad_7D;             // +0x7D
    uint16_t  field_7E;            // +0x7E
    uint16_t  field_80;            // +0x80
    uint8_t   field_82;            // +0x82
    uint8_t   _pad_83;             // +0x83
    uint16_t  field_84;            // +0x84
    uint16_t  field_86;            // +0x86
    union { uint8_t init_flag; uint8_t process_delay; }; // +0x88
    union { uint8_t flag_89; uint8_t ack_counter; }; // +0x89
    uint8_t   flag_8A;             // +0x8A  unknown flag
    uint8_t   _pad_8B;             // +0x8B
    void*     editor_state_2;      // +0x8C  secondary editor state pointer
    uint8_t   active_flag;         // +0x90  active/update flag
    /* Total: 0x94 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                           */
    /* ================================================================ */

    /**
     * Vehicle constructor.
     * Address: 0x44BE50 (598 bytes)
     *
     * Creates a Vehicle object with editor state and initial VehicleEditor
     * sub-object. Sets up position tracking, state machine, engine sounds,
     * and handles local vs. remote multiplayer registration.
     *
     * Allocates:
     *   - GAMESTATE_EditorState (0x20 bytes)
     *   - VehicleEditor (0x450 bytes)
     *
     * Called by:
     *   - NETMAN_SendSignalChange (0x43E780)
     *   - World_LoadFromFile (0x44DCA1, 0x44DDAC)
     *   - Train_HandleConnectionSetup (0x43B2A7)
     *   - Train_HandleTrackBuild (0x43CE90)
     *
     * @param param_1  int32_t  — editor creator parameter
     * @param param_2  int32_t  — object owner handle
     * @param param_3  uint8_t  — multiplayer flag (0=local, 1=remote)
     * @param param_4  uint8_t  — initialization mode
     */
    Vehicle(int32_t param_1, int32_t param_2, uint8_t param_3, uint8_t param_4);
#ifndef _WIN32
    // Native-layout-safe remote vehicle used by the SDL_net 0x3EC path.
    Vehicle(HostNetworkVehicleTag, int32_t resource_id);
    bool AddHostNetworkRoute(const DPlayManager& session);
#endif

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x44C0B0
     *
     * Calls CleanupChildren, then optionally frees memory via GLOBAL_free.
     *
     * Called by: MSVC runtime virtual dispatch through VTBL_VEHICLE[0].
     *
     * @param flags  uint8_t — 1 to free memory, 0 otherwise
     * @return this pointer
     */
    virtual ~Vehicle();

    /* ================================================================ */
    /* Static cleanup helper                                              */
    /* ================================================================ */

    /**
     * CleanupChildren — Clean up child objects (editors and editor state).
     * Address: 0x44C0D0
     *
     * Resets vtable, sends NETMAN ack if not initialized, destroys all
     * VehicleEditor children, destroys editor state object.
     *
     * @param param_1  int* — the Vehicle as int32_t* (used internally by dtor)
     */
    static void __fastcall CleanupChildren(int32_t* param_1);

    /* ================================================================ */
    /* Movement and route management                                      */
    /* ================================================================ */

    /**
     * InitOccupant — Set occupant mode and update position.
     * Address: 0x44C150
     *
     * Sets the occupancy field (+0x64) to the given mode. If mode is 2
     * (waiting), updates position with reverse=0. Otherwise updates with
     * reverse=1 (loading).
     *
     * Called by: GameVehicle::StartMoving (0x4129C0)
     *
     * @param mode  int32_t — occupancy mode to set
     */
    void InitOccupant(int32_t mode);

    /**
     * FindPath — Register this vehicle as a path target occupant.
     * Address: 0x44C170 (174 bytes)
     *
     * Stores the target tile position, registers as occupant of the target
     * building, sets editors to ARRIVING state (5), calls Vehicle_LoadSounds.
     * If the target already has a different vehicle assigned, adds this
     * vehicle to the target's destination queue instead.
     *
     * Called by: GameVehicle::Update (0x412A80)
     *
     * @param target    int32_t* — target building/object pointer
     * @param is_remote uint8_t  — multiplayer flag (0=local, 1=remote)
     */
    void FindPath(int32_t* target, uint8_t is_remote);

    /**
     * InitRoute — Append a new route segment (VehicleEditor).
     * Address: 0x44C220 (232 bytes)
     *
     * Creates a new VehicleEditor sub-object (0x450 bytes) and adds it to
     * the editors array. Max 3 additional segments (allowing up to 4 total
     * editors). Returns 1 on success, 0 on failure.
     *
     * Called by: VehicleEditor_Update or route creation logic.
     *
     * @param param_1  int32_t
     * @param param_2  int32_t
     * @param param_3  uint8_t
     * @return         uint32_t — 1 on success, 0 on failure
     */
    uint32_t InitRoute(int32_t param_1, int32_t param_2, uint8_t param_3);

    /**
     * RemoveEditor — Remove a VehicleEditor by index from the array.
     * Address: 0x44C310
     *
     * Destroys the editor at editors[index], shifts remaining slots left
     * to fill the gap, decrements editorCount.
     *
     * @param index  uint32_t — editor slot index (0-3)
     * @return       int32_t — 1 on success, 0 on failure
     */
    int32_t RemoveEditor(uint32_t index);

    /**
     * GetOccupantCount — Check if any editor sub-object has occupant state 2.
     * Address: 0x44C370
     *
     * Scans editors[1..3] for an editor with a non-null entry whose
     * +0x42C field equals 2. Returns 1 if any match, 0 otherwise.
     *
     * NOTE: Only checks slots 1-3 (skips slot 0 deliberately).
     *
     * @return  uint8_t — 1 if occupant exists, 0 if none
     */
    uint8_t GetOccupantCount();

    /**
     * ClearRoute — Clear the current movement route.
     * Address: 0x44C9B0 (151 bytes)
     *
     * If the wheel editor's state is IDLE (0) and the target building's
     * state is not 4, resets occupancy to 0, clears tracked vehicle on
     * destination, and resets target tile coordinates to -1.
     *
     * The wheel selection depends on active_editor direction:
     *   - Forward (0): uses last editor's rear wheel (+0x434)
     *   - Reverse (1): uses first editor's front wheel (+0x430)
     */
    void ClearRoute();

    /**
     * HandleStop — Handle stop state transitions based on target building.
     * Address: 0x44CA50 (94 bytes)
     *
     * Checks target building's state at +0x110:
     *   - State 1 (loading): plays engine sound, sets state to MOVING (2)
     *   - State 2 (waiting): if not already APPROACHING (1), sets state to APPROACHING
     *   - State 0 (empty): sets state to MOVING (2)
     *
     * @return  uint8_t — 1 if state was changed, 0 if unchanged
     */
    uint8_t HandleStop();

    /**
     * DetachAll — Check if any editor sub-object is in DETACHED state.
     * Address: 0x44CAB0 (62 bytes)
     *
     * Scans all editors for a sub-object whose +0x448 (detach state)
     * equals 1. Returns immediately when found.
     *
     * @return  uint8_t — 1 if detach detected, 0 if none
     */
    uint8_t DetachAll();

    /**
     * ResetState — Reset engine sound with reentrancy guard.
     * Address: 0x44CAF0 (31 bytes)
     *
     * If the reentrancy guard (+0x5A) is not set, sets it, calls
     * UpdateEngineSound, then clears guard. Returns result.
     *
     * @return  uint8_t — result from UpdateEngineSound
     */
    uint8_t ResetState();

    /**
     * UpdateEngineSound — Full engine sound and state machine update.
     * Address: 0x44CB10 (714 bytes)
     *
     * Complex state machine that:
     *   1. Toggles direction between REVERSE (1) and ALT_FRONT (4)
     *   2. Toggles active_editor between 0 and 1
     *   3. Updates wheel edit modes on all editors
     *   4. Manages editor exclusion flags and state codes
     *   5. Copies editor state and runs placement iterations (12 steps)
     *   6. Checks bounds and determines new occupancy mode
     *
     * @return  uint8_t — 0 if no state change needed, else the new occupancy
     */
    uint8_t UpdateEngineSound();

    /**
     * LoadSounds — Configure sound positions for all editor sub-objects.
     * Address: 0x44CE10 (1631 bytes)
     *
     * Determines tile category (road=2, building=1) from target resource.
     * Positions front and rear wheels based on tile type (N/S/W/E facing).
     * Handles both forward and reverse editor positioning.
     *
     * @param target   int32_t* — target building/object
     * @param param_2  uint8_t  — direction/reverse flag
     * @return         uint8_t  — 1 on success
     */
    uint8_t LoadSounds(int32_t* target, uint8_t param_2);

    /**
     * GetNearestTrack — Get the wheel's target building pointer.
     * Address: 0x44D4C0 (53 bytes)
     *
     * Selects the wheel based on active_editor direction, then returns its
     * target building pointer. Only returns non-zero if the target's state
     * (+0x10c) is 7 (track-alike state). Returns 0 otherwise.
     *
     * @return  int32_t — target pointer if state==7, otherwise 0
     */
    int32_t GetNearestTrack();

    /**
     * UpdatePosition — Update vehicle world position and visibility.
     * Address: 0x44D500 (212 bytes)
     *
     * Skips if direction is EDGE_OF_MAP (2) or DEPOT (3), or if net sync
     * is pending. Triggers sound on first editor if not already playing.
     * Toggles visible flag on all editors and invalidates tile rect.
     * Pauses/plays audio channels based on reverse parameter.
     *
     * @param reverse  uint8_t — 1 = make visible/play, 0 = make invisible/pause
     */
    void UpdatePosition(uint8_t reverse);

    /**
     * Stop — Check stop conditions and update engine sound if needed.
     * Address: 0x44D5E0 (66 bytes)
     *
     * If active_editor matches param_1, or sound guard is active, returns
     * immediately. If param_2 is 0, checks IsMoving first. If clear,
     * calls UpdateEngineSound via reentrancy guard.
     *
     * @param param_1  int32_t — editor index to compare against active_editor
     * @param param_2  uint8_t — force flag (0=check IsMoving, 1=always)
     */
    void Stop(int32_t param_1, uint8_t param_2);

    /**
     * IsMoving — Check if the vehicle is currently moving.
     * Address: 0x44D630 (139 bytes)
     *
     * Returns 0 if state is STOPPING (4) or sound guard is active.
     * Otherwise checks whether the wheel's target tile is a road/building
     * type and compares continuity. Returns 1 if moving, 0 if stopped.
     *
     * @return  uint8_t — 1 if moving, 0 if stopped
     */
    uint8_t IsMoving();

    /**
     * CalcSpeed — Calculate and apply speed setting.
     * Address: 0x44D6C0 (82 bytes)
     *
     * If speed matches max_speed or reverse_speed, sets max_steps and
     * triggers animation via editor vtable dispatch.
     *
     * @param speed  int16_t — target speed value
     * @return       int16_t — current max_steps value
     */
    int16_t CalcSpeed(int16_t speed);

    /**
     * UpdateSpeed — Update state if different from current.
     * Address: 0x44D720 (29 bytes)
     *
     * If current state != state param (and not in state 1 when state is 0),
     * calls SetState.
     *
     * @param state  int32_t — new state to set
     */
    void UpdateSpeed(int32_t state);

    /**
     * SetState — Set vehicle movement state.
     * Address: 0x44D740 (186 bytes)
     *
     * States: 0=STOPPED, 1=APPROACHING, 2=MOVING, 3=WAITING, 4=STOPPING.
     * Manages audio channels (pause/play) and timers based on state.
     * Guards against net_sync_pending and edge/depot directions.
     *
     * @param state  int32_t — new state (0-4)
     */
    void SetState(int32_t state);
};

/* ================================================================ */
/* CollisionData — Collision detection data container (0x58 bytes)   */
/* ================================================================ */
#define VTBL_COLLISION_DATA  0x00478370
#define COLLISION_DATA_SIZE  0x58
