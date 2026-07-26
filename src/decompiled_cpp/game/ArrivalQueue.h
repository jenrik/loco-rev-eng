/**
 * ArrivalQueue.h — Building vehicle arrival queue (destination queue)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * ArrivalQueue is NOT a standalone class — it is a set of operations
 * that manage a linked list of vehicles arriving at a building (e.g.,
 * a train station or commercial building). The linked list is stored
 * at offset +0x124 in a GameVehicle/BuildingComplex struct.
 *
 * Each queue node is an 8-byte linked list entry:
 *   +0x00: vehicle pointer (ScriptedObject/GameVehicle)
 *   +0x04: next node pointer (or NULL)
 *
 * Functions:
 *   ArrivalQueue_AddVehicle    — Add a vehicle to the arrival queue
 *   ArrivalQueue_RemoveVehicle — Remove a vehicle from the queue
 *
 * Called by: World_RenderAll (for removal) and VehicleEditor_Update
 *            (for addition).
 */

#pragma once

#include "../shared/types.h"

/* ================================================================== */
/* Arrival Queue Node (internal 8-byte linked list entry)              */
/* ================================================================== */

struct ArrivalQueueNode {
    void*              vehicle;  /* +0x00 — pointer to GameVehicle/ScriptedObject */
    ArrivalQueueNode*  next;     /* +0x04 — next node in linked list (NULL = tail) */
};

/* ================================================================== */
/* Vehicle field offsets (relative to vehicle pointer)                  */
/* These are used by the arrival queue functions for matching.         */
/* ================================================================== */

#define VEHICLE_OFFSET_DIRECTION   0x60  /* vehicle direction/state (set to 2 by AddVehicle) */
#define VEHICLE_OFFSET_FIELD_2E    0x2E  /* vehicle field (set from this->occupation_level +0x88) */
#define VEHICLE_OFFSET_COLOR       0x78  /* vehicle color byte (used by RemoveVehicle for matching) */
#define VEHICLE_OFFSET_PLAYER_ID   0x7A  /* vehicle player ID (ushort, used by RemoveVehicle for matching) */

/* ================================================================== */
/* Arrival queue operations                                            */
/* ================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ArrivalQueue_AddVehicle — Add vehicle to building arrival queue.
 * Address: 0x44F3A0
 * Calling convention: __thiscall (ECX = this = building), RET 0x4
 *
 * Appends a vehicle to the arrival queue linked list at this+0x124.
 * The vehicle's direction (+0x60) is set to 2, and its field_2E (+0x2E)
 * is set from this->occupation_level (+0x88).
 *
 * Called by:
 *   - World_FinalizeLoad (0x44DFFE)
 *   - VehicleEditor_Update (0x44C736, 0x44C84A)
 *
 * @param this     ECX = pointer to GameVehicle/BuildingComplex with queue at +0x124.
 * @param vehicle  First stack arg — pointer to vehicle (ScriptedObject/GameVehicle).
 */
void __thiscall ArrivalQueue_AddVehicle(void* self, void* vehicle);

/**
 * ArrivalQueue_RemoveVehicle — Remove vehicle from arrival queue by player ID + color.
 * Address: 0x44F410
 * Calling convention: __thiscall (ECX = this = building), RET 0x8
 *
 * Searches the arrival queue linked list at this+0x124 for a vehicle
 * matching the given player_id (ushort at vehicle+0x7A) and color
 * (byte at vehicle+0x78). Removes the matching node from the linked
 * list and frees the node memory.
 *
 * Called by:
 *   - World_RenderAll (0x44E683)
 *
 * @param this      ECX = pointer to GameVehicle/BuildingComplex with queue at +0x124.
 * @param player_id First stack arg — player ID to match (compared as ushort).
 * @param color     Second stack arg — color byte to match.
 * @return          BOOL — 1 if a vehicle was removed, 0 if not found.
 */
BOOL __thiscall ArrivalQueue_RemoveVehicle(void* self, uint16_t player_id, byte color);

#ifdef __cplusplus
}
#endif
