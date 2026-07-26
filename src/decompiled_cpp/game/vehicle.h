/**
 * vehicle.h — Vehicle road vehicle class for Lego Loco
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Vehicle is a ScriptedObject (0x94-byte struct, vtable 0x47836C)
 * representing a road vehicle (car, truck, bus, etc.) that follows tracks
 * through the town. It manages position, speed, engine sounds, occupant
 * loading, and multiplayer sync.
 *
 * Size: 0x94 bytes
 * Vtable: 0x0047836C (VTBL_SCRIPTED_OBJECT)
 *
 * Field layout:
 *   +0x00: vtable          (void**) — 0x0047836C
 *   +0x04: param_2         (int32_t) — owner/creator param
 *   +0x08: activeEditor    (int32_t) — 0/1 direction toggle; 0=forward, 1=reverse
 *   +0x0C: editorCount     (int16_t) — number of VehicleEditor refs
 *   +0x10: editors[4]      (void*[4]) — VehicleEditor pointers (max 4)
 *   +0x20: editorState     (void*)  — GAMESTATE_EditorState ptr
 *   +0x24: maxSpeed        (int16_t) — forward speed limit
 *   +0x26: reverseSpeed    (int16_t) — reverse speed limit
 *   +0x28: stopTimer       (int32_t) — stop state countdown
 *   +0x2C: detachFlag      (uint8_t) — detachment flag
 *   +0x2E: tileX           (int16_t) — current tile X
 *   +0x30: tileY           (int16_t) — current tile Y
 *   +0x32: targetTileX     (int16_t) — destination tile X
 *   +0x34: targetTileY     (int16_t) — destination tile Y
 *   +0x36: moveTimer       (int16_t) — movement timer countdown
 *   +0x38: occupantTracks  (int32_t[8]) — 8 track/slot entries (init 0)
 *   +0x58: maxSteps        (int16_t) — max movement steps per frame
 *   +0x5A: soundGuard      (uint8_t) — reentrancy guard for engine sound
 *   +0x5C: state           (int32_t) — movement state (0=STOPPED, 1=APPROACHING,
 *                                       2=MOVING, 3=WAITING, 4=STOPPING)
 *   +0x60: direction       (int32_t) — travel direction (0=forward, 1=reverse,
 *                                       2=edge-of-map, 3=depot, 4=alternate)
 *   +0x64: occupancy       (int32_t) — occupancy mode (0=empty, 1=departing,
 *                                       2=full, 4=stopping, 5=arriving)
 *   +0x68: netSyncFlag     (int32_t) — 0=no sync, 1=needs sync, 2=synced
 *   +0x6C: msgBoxCount     (int8_t) — message box counter
 *   +0x70: (unused)        (int32_t) — always 0
 *   +0x78: colorR          (uint8_t) — player color R component
 *   +0x7A: playerId        (int16_t) — owning player ID
 *   +0x7C: colorG          (uint8_t) — player color G component
 *   +0x88: initFlag        (uint8_t) — initialization flag (from ctor param_4)
 *   +0x89: flag_89         (uint8_t) — unknown flag
 *   +0x8A: flag_8A         (uint8_t) — unknown flag
 *   +0x8C: editorState2    (void*)  — secondary editor state ptr
 *   +0x90: activeFlag      (uint8_t) — active/update flag
 */

#pragma once

#include "../shared/types.h"

/* Vtable address: VTBL_SCRIPTED_OBJECT (0x0047836C) */
#define VTBL_VEHICLE  0x0047836C

/* ================================================================ */
/* Function declarations                                             */
/* ================================================================ */

/**
 * Vehicle_Ctor — Constructor for a road vehicle object.
 * Address: 0x44BE50
 * Size: 0x94 bytes allocated. Creates EditorState (0x20) and
 * VehicleEditor (0x450) sub-objects. Initializes position tracking,
 * state to STOPPED, and handles multiplayer registration.
 *
 * @param this     void*    — 0x94-byte vehicle struct
 * @param param_1  int32_t — editor creator param
 * @param param_2  int32_t — object owner handle
 * @param param_3  uint8_t — multiplayer flag (0=local, 1=remote)
 * @param param_4  uint8_t — initialization mode
 * @return void* — this pointer
 */
void* __thiscall Vehicle_Ctor(void* self, int32_t param_1, int32_t param_2,
                              uint8_t param_3, uint8_t param_4);

/**
 * RESDATA_ScriptedObject_DtorList — Scalar-deleting destructor for ScriptedObject.
 * Address: 0x44C0B0
 * Calls RESDATA_ScriptedObject_CleanupChildren, then optionally frees via GLOBAL_free.
 * Vtable entry at 0x47836C[0].
 *
 * @param this    void*
 * @param param_1 uint8_t — 1 to free memory, 0 otherwise
 * @return void* — this
 */
void* __thiscall RESDATA_ScriptedObject_DtorList(void* self, uint8_t param_1);

/**
 * RESDATA_ScriptedObject_CleanupChildren — Cleans up child objects.
 * Address: 0x44C0D0
 * Resets vtable, sends NETMAN ack, destroys child array at +0x10..+0x1c,
 * then destroys tail at +0x20.
 *
 * @param param_1 int* — ScriptedObject
 */
void __fastcall RESDATA_ScriptedObject_CleanupChildren(int32_t* param_1);

/**
 * Vehicle_InitOccupant — Initialize occupant mode.
 * Address: 0x44C150
 * Sets occupancy mode and calls Vehicle_UpdatePosition.
 *
 * @param this     void*
 * @param mode     int32_t — mode (2=waiting, other=loading)
 */
void __thiscall Vehicle_InitOccupant(void* self, int32_t mode);

/**
 * Vehicle_FindPath — Find and register a path target for this vehicle.
 * Address: 0x44C170
 * Sets target tile, registers as occupant with target building,
 * sets state ARRIVING (5), updates editors, calls Vehicle_LoadSounds.
 *
 * @param this     void*
 * @param target   int32_t* — target building/object pointer
 * @param isRemote uint8_t  — multiplayer flag
 */
void __thiscall Vehicle_FindPath(void* self, int32_t* target, uint8_t isRemote);

/**
 * Vehicle_InitRoute — Append a new route segment (VehicleEditor).
 * Address: 0x44C220
 * Creates a new VehicleEditor as child sub-object. Max 4 route segments.
 *
 * @param this    void*
 * @param param_1 int32_t
 * @param param_2 int32_t
 * @param param_3 uint8_t
 * @return uint32_t — 1 on success, 0 on failure
 */
uint32_t __thiscall Vehicle_InitRoute(void* self, int32_t param_1,
                                      int32_t param_2, uint8_t param_3);

/**
 * VehicleEditor_RemoveVehicle — Remove an editor sub-object by index.
 * Address: 0x44C310
 * Destroys the editor at editors[index], shifts remaining slots left.
 *
 * @param this   void* — Vehicle*
 * @param index  uint32_t — editor slot index (0-3)
 * @return int32_t — 1 on success, 0 on failure
 */
int32_t __thiscall VehicleEditor_RemoveVehicle(void* self, uint32_t index);

/**
 * Vehicle_GetOccupantCount — Check if any editor has occupant mode 2.
 * Address: 0x44C370
 *
 * @param this  void* — Vehicle* (via ECX, __fastcall)
 * @return uint8_t — 1 if occupant exists, 0 if none
 */
uint8_t __fastcall Vehicle_GetOccupantCount(void* self);

/**
 * VehicleEditor_Update — Main per-frame vehicle editor update tick (1520 bytes).
 * Address: 0x44C3A0
 * Processes vehicle movement across track grid. Handles edge-of-map
 * routing to neighbor towns, collision resolution, speed calculation,
 * and network position sync.
 *
 * __fastcall (ECX=Vehicle*)
 */
void __fastcall VehicleEditor_Update(void* param_1);

/**
 * Vehicle_ClearRoute — Clear current movement route.
 * Address: 0x44C9B0
 * Resets occupancy to 0 and clears target tile coords on certain states.
 *
 * __fastcall (ECX=Vehicle*)
 */
void __fastcall Vehicle_ClearRoute(void* self);

/**
 * Vehicle_HandleStop — Handle stop state transitions.
 * Address: 0x44CA50
 * Checks target building state to decide transitions.
 *
 * __fastcall (ECX=Vehicle*)
 * @param this  void*
 * @return uint8_t — 0 or 1 depending on state
 */
uint8_t __fastcall Vehicle_HandleStop(void* self);

/**
 * Vehicle_DetachAll — Check if any editor sub-object is in DETACHED state.
 * Address: 0x44CAB0
 * Scans editor list for a sub-object with state 1 at +0x448.
 *
 * __fastcall (ECX=Vehicle*)
 * @param this  void* — Vehicle*
 * @return uint8_t — 1 if detach detected, 0 if none
 */
uint8_t __fastcall Vehicle_DetachAll(void* self);

/**
 * Vehicle_ResetState — Reset vehicle engine sound state.
 * Address: 0x44CAF0
 * Plays engine sound update if reentrancy guard is clear.
 *
 * __fastcall (ECX=Vehicle*)
 * @param this  void*
 * @return uint8_t — result from Vehicle_UpdateEngineSound
 */
uint8_t __fastcall Vehicle_ResetState(void* self);

/**
 * Vehicle_UpdateEngineSound — Update vehicle engine sound and state machine.
 * Address: 0x44CB10 (714 bytes)
 *
 * __fastcall (ECX=Vehicle*)
 * @param this  void*
 * @return uint8_t — success flag
 */
uint8_t __fastcall Vehicle_UpdateEngineSound(void* self);

/**
 * Vehicle_LoadSounds — Configure sound positions for all vehicle sub-objects.
 * Address: 0x44CE10 (1631 bytes)
 * Configures audio channel positions for all VehicleEditor sub-objects
 * based on the tile type category.
 *
 * __thiscall
 * @param this     void*      — Vehicle*
 * @param param_1  int32_t*  — target building/object
 * @param param_2  uint8_t   — direction/reverse flag
 * @return uint8_t — 1 on success
 */
uint8_t __thiscall Vehicle_LoadSounds(void* self, int32_t* param_1, uint8_t param_2);

/**
 * Vehicle_GetNearestTrack — Get nearest track position.
 * Address: 0x44D4C0
 *
 * __fastcall (ECX=Vehicle*)
 * @param this  void*
 * @return int32_t — track tile index or -1
 */
int32_t __fastcall Vehicle_GetNearestTrack(void* self);

/**
 * Vehicle_UpdatePosition — Update vehicle world position.
 * Address: 0x44D500
 * Updates visible flag for all editors and pauses/plays audio channels.
 *
 * __thiscall
 * @param this    void*
 * @param reverse uint8_t — reverse visible flag
 */
void __thiscall Vehicle_UpdatePosition(void* self, uint8_t reverse);

/**
 * Vehicle_Stop — Stop/sound guard check for vehicle.
 * Address: 0x44D5E0
 * Checks activeEditor match and sound guard. If clear and vehicle is
 * moving, plays engine sound update.
 *
 * __thiscall (ECX=this, 2 stack args)
 * @param this    void*
 * @param param_1 int32_t — editor index to compare against activeEditor
 * @param param_2 uint8_t — force flag (0=check isMoving, 1=always)
 */
void __thiscall Vehicle_Stop(void* self, int32_t param_1, uint8_t param_2);

/**
 * Vehicle_IsMoving — Check if vehicle is currently moving.
 * Address: 0x44D630
 * Checks state STOPPING (4), soundGuard, and compares wheel target
 * tile type for road/building continuity.
 *
 * __fastcall (ECX=Vehicle*)
 * @param this  void*
 * @return uint8_t — 1 if moving, 0 if stopped
 */
uint8_t __fastcall Vehicle_IsMoving(void* self);

/**
 * Vehicle_CalcSpeed — Calculate speed based on target speed setting.
 * Address: 0x44D6C0
 * If speed matches maxSpeed or reverseSpeed, sets maxSteps and triggers
 * animation sound via editor vtable.
 *
 * __thiscall
 * @param this  void*  — Vehicle*
 * @param speed int16_t — target speed value
 * @return int16_t — current maxSteps value
 */
int16_t __thiscall Vehicle_CalcSpeed(void* self, int16_t speed);

/**
 * Vehicle_UpdateSpeed — Update vehicle state if different.
 * Address: 0x44D720
 * If current state != param_1 (and not state 1 when param_1==0),
 * calls Vehicle_SetState.
 *
 * __thiscall
 * @param this   void*
 * @param state  int32_t — new state to set
 */
void __thiscall Vehicle_UpdateSpeed(void* self, int32_t state);

/**
 * Vehicle_SetState — Set vehicle movement state.
 * Address: 0x44D740
 * States: 0=STOPPED, 1=APPROACHING, 2=MOVING, 3=WAITING, 4=STOPPING
 * Manages audio channels and timer based on state.
 *
 * __thiscall
 * @param this  void*
 * @param state int32_t — new state (0-4)
 */
void __thiscall Vehicle_SetState(void* self, int32_t state);

/**
 * CollisionData_Ctor — Constructor for collision data container.
 * Address: 0x44D800 (previously named Vehicle_CheckCollision)
 * Sets vtable to 0x00478370 and zeroes all fields (0x58-byte struct).
 *
 * __fastcall (ECX=param_1, this pointer)
 * @param param_1 void* — 0x58-byte collision data struct
 * @return void* — initialized struct
 */
void* __fastcall CollisionData_Ctor(void* param_1);

/**
 * CollisionData_Dtor — Scalar deleting destructor for collision data.
 * Address: 0x44D830 (previously named Vehicle_ResolveCollision)
 * Restores vtable, calls World_Init, optionally frees via GLOBAL_free.
 *
 * __thiscall
 * @param this   void*
 * @param param_1 uint8_t — 1 to free memory, 0 otherwise
 * @return void* — this
 */
void* __thiscall CollisionData_Dtor(void* self, uint8_t param_1);

/* Vehicle struct field offsets for use by other subsystems */
#define VEHICLE_VTABLE            0x00
#define VEHICLE_PARAM_2           0x04
#define VEHICLE_ACTIVE_EDITOR     0x08
#define VEHICLE_EDITOR_COUNT      0x0C
#define VEHICLE_EDITORS           0x10
#define VEHICLE_EDITOR_STATE      0x20
#define VEHICLE_MAX_SPEED         0x24
#define VEHICLE_REVERSE_SPEED     0x26
#define VEHICLE_STOP_TIMER        0x28
#define VEHICLE_DETACH_FLAG       0x2C
#define VEHICLE_TILE_X            0x2E
#define VEHICLE_TILE_Y            0x30
#define VEHICLE_TARGET_X          0x32
#define VEHICLE_TARGET_Y          0x34
#define VEHICLE_MOVE_TIMER        0x36
#define VEHICLE_OCCUPANT_TRACKS   0x38
#define VEHICLE_MAX_STEPS         0x58
#define VEHICLE_SOUND_GUARD       0x5A
#define VEHICLE_STATE             0x5C
#define VEHICLE_DIRECTION         0x60
#define VEHICLE_OCCUPANCY         0x64
#define VEHICLE_NET_SYNC_FLAG     0x68
#define VEHICLE_MSG_BOX_COUNT     0x6C
#define VEHICLE_COLOR_R           0x78
#define VEHICLE_PLAYER_ID         0x7A
#define VEHICLE_COLOR_G           0x7C
#define VEHICLE_INIT_FLAG         0x88
#define VEHICLE_FLAG_89           0x89
#define VEHICLE_FLAG_8A           0x8A
#define VEHICLE_EDITOR_STATE2     0x8C
#define VEHICLE_ACTIVE_FLAG       0x90

/* CollisionData struct field offsets */
#define COLLISION_DATA_VTABLE     0x00
#define COLLISION_DATA_SIZE       0x58
