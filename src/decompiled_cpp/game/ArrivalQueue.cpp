#include "ArrivalQueue.h"

/**
 * ArrivalQueue.cpp — Building vehicle arrival queue implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the arrival queue operations for buildings (GameVehicle/
 * BuildingComplex) that maintain a linked list of arriving vehicles
 * at offset +0x124.
 *
 * Node layout (8-byte linked list):
 *   +0x00: vehicle pointer
 *   +0x04: next node pointer (NULL = end of list)
 */

#include "ArrivalQueue.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void* __cdecl operator_new(size_t size);       /* 0x465CE0 */
void  __cdecl GLOBAL_free(void* ptr);           /* 0x465CD0 */
void  __thiscall Vehicle_SetState(void* vehicle, int state); /* 0x44D740 */

/* ================================================================== */
/* ArrivalQueue_AddVehicle — Add vehicle to arrival queue             */
/* Address: 0x44F3A0                                                   */
/* Size: 101 bytes (35 instructions)                                    */
/*                                                                     */
/* Called by: World_FinalizeLoad (0x44DFFE),                            */
/*            VehicleEditor_Update (0x44C736, 0x44C84A)                */
/*                                                                     */
/* Sets vehicle direction to 2 (ARRIVING), copies the building's       */
/* occupation_level to the vehicle's field_2E, sets vehicle state to   */
/* 0 (STOPPED), then appends the vehicle to the linked list at         */
/* this+0x124. Allocates an 8-byte node for the list entry.           */
/*                                                                     */
/* @param this     ECX = pointer to GameVehicle/BuildingComplex.      */
/* @param vehicle  First stack arg — vehicle to add.                  */
/* ================================================================== */
void __thiscall ArrivalQueue_AddVehicle(void* self, void* vehicle)
{
    ArrivalQueueNode* new_node;
    ArrivalQueueNode* tail;
    int occupation_level;

    /* Step 1: Set vehicle direction to 2 (ARRIVING) */
    *(int*)((char*)vehicle + 0x60) = 2;                    /* direction +0x60 */

    /* Step 2: Copy building's occupation_level (+0x88) to vehicle field +0x2E */
    occupation_level = *(int*)((char*)self + 0x88);        /* this->occupation_level */
    *(int*)((char*)vehicle + 0x2E) = occupation_level;     /* vehicle field +0x2E */

    /* Step 3: Set vehicle state to 0 (STOPPED) */
    Vehicle_SetState(vehicle, 0);  /* 0x44D740 */

    /* Step 4: Allocate a new 8-byte queue node */
    new_node = (ArrivalQueueNode*)operator_new(8);  /* 0x465CE0 */
    new_node->vehicle = vehicle;                     /* +0x00 */
    new_node->next = NULL;                           /* +0x04 */

    /* Step 5: Append to end of linked list at this+0x124 */
    tail = *(ArrivalQueueNode**)((char*)self + 0x124);

    if (tail == NULL) {
        /* List was empty — new node becomes the head */
        *(ArrivalQueueNode**)((char*)self + 0x124) = new_node;
    } else {
        /* Walk to the end of the list (follow ->next until NULL) */
        while (tail->next != NULL) {
            tail = tail->next;
        }
        /* Append new node at the end */
        tail->next = new_node;
    }
}

/* ================================================================== */
/* ArrivalQueue_RemoveVehicle — Remove vehicle from queue by ID/color */
/* Address: 0x44F410                                                   */
/* Size: 123 bytes (57 instructions)                                    */
/*                                                                     */
/* Called by: World_RenderAll (0x44E683)                               */
/*                                                                     */
/* Searches the queue for a vehicle matching both player_id (ushort at */
/* vehicle+0x7A) and color (byte at vehicle+0x78). If found:          */
/*   - Unlinks the node from the linked list                          */
/*   - Frees the node memory via GLOBAL_free                          */
/*   - Returns 1 (TRUE)                                                */
/* If not found: returns 0 (FALSE).                                    */
/*                                                                     */
/* @param this      ECX = pointer to GameVehicle/BuildingComplex.     */
/* @param player_id First stack arg — player ID to match (ushort).    */
/* @param color     Second stack arg — vehicle color byte to match.   */
/* @return          BOOL — 1 if a vehicle was removed, 0 if not found. */
/* ================================================================== */
BOOL __thiscall ArrivalQueue_RemoveVehicle(void* self, uint16_t player_id, byte color)
{
    ArrivalQueueNode* current;
    ArrivalQueueNode* prev;
    void* vehicle;
    uint16_t vehicle_player_id;
    byte vehicle_color;

    /* Get list head from this+0x124 */
    current = *(ArrivalQueueNode**)((char*)self + 0x124);

    /* If list is empty, return FALSE */
    if (current == NULL) {
        return 0;
    }

    prev = NULL;

    /* Walk the linked list searching for a match */
    while (current != NULL) {
        vehicle = current->vehicle;  /* +0x00 */

        /* Read vehicle player ID (ushort at +0x7A) and color (byte at +0x78) */
        vehicle_player_id = *(uint16_t*)((char*)vehicle + 0x7A);
        vehicle_color = *(byte*)((char*)vehicle + 0x78);

        /* Check if this vehicle matches */
        if (vehicle_player_id == player_id && vehicle_color == color) {
            /* Found a match — remove this node */
            if (prev == NULL) {
                /* Removing the head node */
                *(ArrivalQueueNode**)((char*)self + 0x124) = current->next;
            } else {
                /* Removing a middle or tail node */
                prev->next = current->next;
            }

            /* Free the node memory */
            GLOBAL_free(current);
            return 1;
        }

        /* Advance to next node */
        prev = current;
        current = current->next;
    }

    /* No matching vehicle found */
    return 0;
}
