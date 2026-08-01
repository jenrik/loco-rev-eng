// Status: INTEGRATED
#ifndef WORLD_H
#define WORLD_H

/**
 * World.h — Top-level game world manager class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * World is the top-level singleton that manages all in-game vehicles.
 * It owns the active vehicle array (max 4), handles serialization/
 * deserialization, updates all subsystems each tick, manages depth-sorted
 * sub-object arrays for rendering, and coordinates collision detection.
 *
 * All methods are non-virtual (no vtable). The World singleton lives
 * at g_world (0x4A98B0). The World struct is passed via ECX (this) for
 * all member functions, using MSVC __thiscall convention.
 *
 * Size: 0x58 bytes
 * No vtable (no virtual methods)
 * No inheritance
 *
 * Global singleton: g_world at 0x4A98B0
 *
 * NOTE: CollisionData (vtable 0x478370, 0x58 bytes) shares the exact same
 * layout; CollisionData_Ctor (0x44D800) zeroes the identical field set and
 * CollisionData_Dtor (0x44D830) forwards to World_Init. CollisionData is
 * never constructed in the shipped binary (its only ctor caller at 0x45C595
 * is unreferenced code), so it is documented but not represented here.
 */

#pragma once

#include "../shared/types.h"

class Vehicle;
class VehicleEditor;
class Building;
class GameVehicle;
class RESDATA_GameVehicle;

/* ================================================================== */
/* Global address                                                      */
/* ================================================================== */
#define ADDR_g_world                     0x004A98B0  /* World singleton */

/* ================================================================== */
/* External global objects referenced by World methods                 */
/* ================================================================== */
#define ADDR_g_town_view                 0x004852A0  /* Town view singleton */
#define ADDR_g_ddraw_building            0x004A9EF0  /* DDRAW building singleton */
#define ADDR_g_netman                    0x004FD3AC  /* NetMan singleton */
#define ADDR_g_input_mgr                 0x004A9990  /* Input manager */
#define ADDR_g_tilemap                   0x004AAD08  /* TileMap singleton */
#define ADDR_g_tooltip_mgr               0x004FD220  /* UI/tooltip manager */
#define ADDR_g_click_on_town             0x0048557C  /* Click-on-town flag */
#define ADDR_g_selected_building          0x00485380  /* never-written slot read by World::SaveToFile; distinct from the selection global at 0x4855B0 */
#define ADDR_g_game_mode                 0x004851F4  /* Game mode (3=town, 9=network) */

/* ================================================================== */
/* World class                                                         */
/* ================================================================== */
class World {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    int32_t     pad_00;             /* +0x00  unused (never read/written by World)  */
    int16_t     vehicle_count;      /* +0x04  active vehicle count (max 4) */
    int16_t     local_vehicle_count;/* +0x06  vehicles with owner_handle==0 (max 3) */
    Vehicle*    vehicles[4];        /* +0x08  active vehicle pointers */
    VehicleEditor* sub_objects[16]; /* +0x18  depth-sorted sub-objects (from Lock)  */
    /* Total: 0x58 bytes */

    /* ================================================================ */
    /* Constructor / Destructor (none — singleton, not dynamically alloc) */
    /* ================================================================ */

    /* ================================================================ */
    /* Lifecycle Methods                                                 */
    /* ================================================================ */

    /**
     * Init: Remove all vehicles from the world.
     * Address: 0x44D9B0
     *
     * Iterates all 4 vehicle slots. For each active vehicle:
     * removes all 8 occupant entries (vehicle->occupant_tracks at +0x38)
     * via Building::RemoveOccupant, calls World_RenderAll, calls
     * SaveToFile with mp_flag=1.
     * Then releases two global object pointers (0x485268 / 0x48526C)
     * via their vtable[2] (+0x08) release methods and nulls them.
     * Both globals are never assigned anywhere in loco.exe, so this
     * cleanup never fires in the shipped binary.
     *
     * Called by: CGWND_Cleanup (0x40787E), Sprite_UnlockAll (0x454FF4),
     *            CollisionData_Dtor (0x44D839), INPUT_NewWorld (0x41E148)
     */
    void Init(void);

    /**
     * Shutdown: Clear/zero all World struct fields.
     * Address: 0x44D870
     *
     * Zeros vehicle_count, local_vehicle_count, all 4 vehicle slots,
     * and all 16 sub_object entries.
     *
     * Called by: CGWND_Cleanup (0x407888), CGWND_QuitToMenu (0x406EB8)
     */
    void Shutdown(void);

    /**
     * CheckActive: Return 1 if world has reached capacity.
     * Address: 0x44DBB0
     *
     * Returns 1 if vehicle_count >= 4 OR local_vehicle_count >= 3.
     * Used by DDRAW_UpdateBuildingSprites to decide
     * whether to allow new vehicle placement.
     *
     * Called by: DDRAW_UpdateBuildingSprites (0x459B31)
     *
     * @return 1 if world is full/active, 0 otherwise
     */
    char CheckActive(void);

    /**
     * Reset: Reset vehicles in specific occupancy states.
     * Address: 0x44DBD0
     *
     * For all active vehicles where occupancy (at +0x64) is
     * in {0, 1, 4, 5}, calls Vehicle::UpdatePosition with flag.
     *
     * @param flag  Parameter passed to Vehicle_UpdatePosition
     */
    void Reset(char flag);

    /* ================================================================ */
    /* Serialization Methods                                             */
    /* ================================================================ */

    /**
     * LoadFromFile: Create a vehicle in the first empty world slot.
     * Address: 0x44DC10
     *
     * If vehicle_init is NULL, generates a random vehicle with:
     *   - Random resource ID from {0x1804, 0x1806, 0x1808}
     *   - Name via CRT_sprintf("%s %lu", g_player_config->name, id)
     *   - Random route (0-4 segments) from destination pool
     *     {0x1866/0x1868/0x186A, 0x186C/0x186E, 0x1870}
     * If vehicle_init is non-NULL, creates a vehicle from save data
     * and sets up route from vehicle_init[1..3] entries.
     *
     * On resource load failure (editors[0] uninitialized), destroys the
     * vehicle and returns NULL.
     *
     * @param route_data   Pointer to route data buffer for Vehicle::FindPath
     * @param vehicle_init Pointer to save data {resource_id, route[3]}, or NULL for random
     * @return             Pointer to created vehicle, or NULL on failure
     */
    Vehicle* LoadFromFile(int* route_data, int* vehicle_init);

    /**
     * SaveToFile: Remove/delete a vehicle by resource_id + player_id.
     * Address: 0x44D8A0
     *
     * Finds matching vehicle, deselects from town/DDRAW views if the
     * never-written selection slot (0x485380, always 0) equals
     * editors[0] (branch never fires), notifies the network
     * manager, then deletes the vehicle via its destructor. Decrements
     * local_vehicle_count when the vehicle is locally owned
     * (owner_handle == 0).
     *
     * @param resource_id  Numeric resource ID of the target vehicle
     * @param player_id    Player identifier byte
     * @param mp_flag      Multiplayer flag (1=save, used in scenario 2 dispatch)
     * @return             true on success, false if not found
     */
    bool SaveToFile(uint resource_id, char player_id, char mp_flag);

    /**
     * SerializeObject: Find and serialize vehicles matching player_id.
     * Address: 0x44DA50
     *
     * Iterates vehicles. For those matching the given player_id:
     * clears all 8 occupant slots, calls World_RenderAll, calls
     * SaveToFile with mp_flag=0.
     *
     * @param player_id  Player ID byte to match against vehicle owner
     */
    void SerializeObject(char player_id);

    /**
     * DeserializeMap: Check game-vehicle rect overlap with vehicle sub-objects.
     * Address: 0x44DAD0
     *
     * For each active vehicle: checks if any sub-object's rect (+0x08..+0x14)
     * overlaps with the game vehicle's rect. On overlap, clears occupants,
     * renders, and saves the vehicle.
     *
     * Skips vehicles where occupancy==2 unless vehicle_kind==4 (GameVehicle).
     *
     * @param game_vehicle  RESDATA_GameVehicle to test overlap against
     */
    void DeserializeMap(RESDATA_GameVehicle* game_vehicle);

    /**
     * FinalizeLoad: Register a fully-loaded vehicle in the world.
     * Address: 0x44DF40
     *
     * Finds first empty vehicle slot, stores vehicle pointer, clears
     * net_sync_flag (+0x68), increments vehicle_count.
     * Unpacks destination coordinates from packed_coords.
     * Finds destination building via INPUT_FindObjectAt or
     * TileMap_GetObjectAt (depending on netman scenario).
     * Links vehicle to building via ArrivalQueue_AddVehicle.
     *
     * @param vehicle        Initialized vehicle to register
     * @param packed_coords  Packed destination coordinates (x in low 16 bits, y in high 16)
     * @param mp_flag        Multiplayer flag (affects y offset in scenario 2)
     * @return               1 on success, 0 if world is full or no empty slot
     */
    char FinalizeLoad(Vehicle* vehicle, int packed_coords, char mp_flag);

    /* ================================================================ */
    /* Per-Frame Update Methods                                          */
    /* ================================================================ */

    /**
     * UpdateTick: Main per-tick world update.
     * Address: 0x44E020
     *
     * Guards: only runs if vehicle_count > 0 and game_mode is 3 or 9.
     * For each active vehicle:
     *   1. Calls ProcessEvents (collision detection)
     *   2. Calls VehicleEditor_Update
     *   3. If vehicle state (+0x5C) == 3 (ARRIVED):
     *        Clears all occupants, calls World_RenderAll, calls SaveToFile
     *   4. Else if net_sync_flag (+0x68) == 1:
     *        Reads packed destination coords from editor_state->building+0x88,
     *        sets flag to 2, clears occupants (if owner_handle != 0),
     *        renders, calls Netman::ReceiveGameStart, removes from world
     *
     * Called by: GameLoop_FrameUpdate (0x45C462, 0x45C49C)
     */
    void UpdateTick(void);

    /**
     * ProcessEvents: Detect and handle vehicle collisions.
     * Address: 0x44E3F0
     *
     * For current_vehicle's editor state against each other vehicle's
     * sub-objects:
     *   Only tests sub-objects whose end_a/end_b editor states reference
     *   the same building as current_vehicle's editor state.
     *   Checks bounding-box overlap of the sub-object rect with the
     *   current editor-state position (pos_x/pos_y).
     *   Guards on current editor_state->edit_state != 2.
     *   On visual overlap (Town_BlitViewport returns 0):
     *     Stops both vehicles (state=4), sets random 1-100 tick wait,
     *     shows collision message box (resource 0x3861).
     *
     * Skips: vehicles with direction 2 or 3, occupancy == 2,
     * stopped vehicles with stop_timer==0.
     *
     * @param current_vehicle  Vehicle to check for collisions
     * @return                 0 (always — return value not meaningful)
     */
    uint ProcessEvents(Vehicle* current_vehicle);

    /**
     * ProcessAudio: Audio hit-test for sub-objects at position.
     * Address: 0x44E830
     *
     * Checks each active vehicle's sub-objects against (audio_x, audio_y)
     * via GameObject::PtInRect (vtable[2]). On match, selects the object
     * in the town view. Guarded by g_click_on_town flag.
     *
     * @param audio_x  X position to test
     * @param audio_y  Y position to test
     * @return         1 if a matching sub-object was found and selected
     */
    char ProcessAudio(int audio_x, int audio_y);

    /* ================================================================ */
    /* Rendering Support Methods                                         */
    /* ================================================================ */

    /**
     * InitTimer: Check if any vehicle editor state references the given object.
     * Address: 0x44E160
     *
     * Iterates all vehicles and their sub-objects, checking if
     * editor_state(+0x20)->building(+0x14) matches the given pointer,
     * or any sub-object's end_a/end_b (+0x430/+0x434)->building(+0x14)
     * matches. Returns 1 on first match. The parameter is a pointer
     * value (caller RESDATA_GameVehicle_InitSounds passes a
     * RESDATA_GameVehicle*); the original source compares it as an int.
     *
     * @param object_ptr  Object pointer to search for (passed as int)
     * @return            1 if the object is referenced by any vehicle
     */
    char InitTimer(int object_ptr);

    /**
     * Lock: Collect non-excluded sub-objects, sorted by depth (z-order).
     * Address: 0x44E200
     *
     * Iterates all active vehicles. For each sub-object with
     * edge_dir_b (+0x444) != 2 and != 5 AND edge_dir_a (+0x440) != 2:
     * copies to sub_objects[] array.
     * Then bubble-sorts by depth order (via end_a editor state +0x10,
     * EditorState::pos_y).
     *
     * Called by: TileMap_InvalidateDirtyRects (0x45619A)
     */
    void Lock(void);

    /**
     * Unlock: Clear the sub_objects array.
     * Address: 0x44E2D0
     *
     * Zeros all 16 entries in sub_objects[].
     *
     * Called by: TileMap_InvalidateDirtyRects (0x4566E9)
     */
    void Unlock(void);

    /**
     * InvalidateRect: Blit background for sub-objects at given tile.
     * Address: 0x44E2E0
     *
     * Iterates depth-sorted sub_objects[]. For each sub-object within
     * bounds at (x,y) via VehicleEditor::IsInBounds: calls
     * VehicleEditor::BlitBackground.
     *
     * If scroll_stop==1, also checks adjacent sub-objects with the
     * same target_building (+0x44C) and zero bound_check_flag (+0x448==0).
     *
     * @param x           X tile position
     * @param y           Y tile position
     * @param param3      Unused parameter (present on the stack but never read)
     * @param param4      Unused parameter (present on the stack but never read)
     * @param scroll_stop Scroll-stop mode (0=normal, 1=check adjacent)
     */
    void InvalidateRect(int x, int y, int param3, int param4, short scroll_stop);
};

/* ================================================================== */
/* Free functions with World_ prefix (not class methods)               */
/* ================================================================== */

/**
 * SerializeMap: Transfer route data onto a game vehicle occupant.
 * Address: 0x44DEA0
 *
 * If game_vehicle has occupant_state (+0x11C == 1) and current_vehicle
 * (+0x120) is non-NULL: sets new resource on the sub-object via its
 * vtable[15] (VehicleEditor resource setter, 0x40E0F0), removes
 * 3 old route entries, installs up to 3 new route points from
 * route_data[1..3], then calls Vehicle::FindPath.
 *
 * @param game_vehicle  GameVehicle with occupant data
 * @param route_data    Route data array: [0]=resource_id, [1..3]=route entries
 * @return              true if any route entries were installed
 */
bool __stdcall World_SerializeMap(GameVehicle* game_vehicle, int* route_data);

/**
 * RenderAll: Handle vehicle arrival — cleanup, detach, unlink.
 * Address: 0x44E630
 *
 * Misnamed: this is NOT rendering. It handles vehicle arrival
 * at destination: removes from arrival queue, clears current_vehicle,
 * detaches editor visual states from all sub-objects.
 *
 * Steps:
 *   1. Vehicle::UpdatePosition(0)
 *   2. Check destination tile, remove from ArrivalQueue if direction 2-3
 *   3. Clear current_vehicle on destination GameVehicle
 *   4. Detach editor states via StopSound (vtable[7]) on building refs
 *   5. Detach via EditorState::Detach on all sub-obj editors
 *
 * @param vehicle  Vehicle that has arrived at destination
 */
void __stdcall World_RenderAll(Vehicle* vehicle);

/**
 * GetObjectAt: Remove all occupant slots from an object.
 * Address: 0x44E800
 *
 * Simple helper: clears all 8 occupant slots at vehicle->occupant_tracks
 * (+0x38..+0x54) via Building::RemoveOccupant.
 *
 * @param vehicle  Vehicle with occupant slots to clear
 */
void __stdcall World_GetObjectAt(Vehicle* vehicle);

#endif /* WORLD_H */
