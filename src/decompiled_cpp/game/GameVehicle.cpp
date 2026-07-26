/**
 * GameVehicle.cpp — Vehicle destination queue management
 * Status: INTEGRATED
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * GameVehicle manages the assignment of road vehicles/trains to building
 * and track targets. It maintains a singly-linked destination queue for
 * vehicles waiting when the target is already occupied.
 *
 * Class hierarchy:
 *   GameObject → Entity → ResourceGameObject (type=3)
 *     → RESDATA_GameVehicle (type=4)
 *       → GameVehicle (vtable 0x477848)
 */

#include "GameVehicle.h"
#include "Vehicle.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void* operator_new(size_t size);    /* 0x465CE0 */
void  GLOBAL_free(void* ptr);       /* 0x465CD0 */


/* ================================================================== */
/* GameVehicle::GameVehicle — Constructor                              */
/* Address: 0x412870                                                   */
/*                                                                      */
/* Chains to RESDATA_GameVehicle(resource_id) which:                   */
/*   1. Calls ResourceGameObject(resource_id) → Entity(res_id,-1,0,0) */
/*   2. Sets type=4, vtable to VTBL_RESDATA_GAMEVEHICLE                */
/*   3. Determines vehicle_kind from tile type                         */
/*                                                                      */
/* GameVehicle then zeroes all extended fields and sets vehicle_kind=4. */
/* The compiler automatically sets the vtable to VTBL_GAMEVEHICLE.     */
/* ================================================================== */
GameVehicle::GameVehicle(int resource_id)
    : RESDATA_GameVehicle(resource_id)
{
    this->occupant_state = 0;
    this->current_vehicle = nullptr;
    this->dest_list_head   = nullptr;
    this->busy_flag        = 0;

    this->vehicle_kind = 4;
}


/* ================================================================== */
/* GameVehicle::~GameVehicle — Destructor                              */
/* Address: 0x4128D0 (base destructor body)                            */
/*                                                                      */
/* NOTE: SEH frame at 0x4128D0 omitted — not portable to GCC.          */
/*                                                                      */
/* Frees the singly-linked destination queue at +0x124.                */
/* Base class destructors (~RESDATA_GameVehicle, ~ResourceGameObject,  */
/* ~Entity, ~GameObject) run automatically after this body.            */
/* ================================================================== */
GameVehicle::~GameVehicle()
{
    /* Free the destination queue linked list */
    DestNode* node = this->dest_list_head;
    while (node != nullptr) {
        DestNode* next = node->next;
        GLOBAL_free(node);
        node = next;
    }
    /* ~RESDATA_GameVehicle() runs automatically:
     *   → World_DeserializeMap
     *   → ~ResourceGameObject() → BuildingMgr_DestroyObjectGroup
     *   → ~Entity()
     *   → ~GameObject()
     */
}


/* ================================================================== */
/* GameVehicle::StartMoving — Assign a vehicle to this target/building */
/* Address: 0x4129C0 (NON-VIRTUAL, not in vtable)                      */
/*                                                                      */
/* Verified against disassembly at 0x4129C0-0x412A78.                 */
/* ================================================================== */
uint32_t GameVehicle::StartMoving(Vehicle* vehicle)
{
    if (g_demo_mode != 1 &&
        vehicle->owner_handle == 0 &&
        this->busy_flag == 0)
    {
        /* Success: take ownership of the vehicle */

        if (this->occupant_state != 0) {
            this->StopSound(0);
        }

        this->busy_flag       = 1;
        this->current_vehicle = vehicle;

        vehicle->InitOccupant(1);

        /* Copy tile_target as a 32-bit dword, spanning both int16_t
         * target_tile_x (+0x32) and target_tile_y (+0x34).
         * Use memcpy to avoid strict-aliasing UB. */
        {
            int32_t val = this->tile_target();
            __builtin_memcpy(&vehicle->target_tile_x, &val, sizeof(int32_t));
        }

        return 1;
    }

    /* Failure: target is busy or vehicle unavailable */

    if (this->occupant_state == 0) {
        this->StopSound(2);
        this->occupant_state = 2;
    }

    if (vehicle->IsMoving()) {
        int32_t timeout = (vehicle->active_editor == 0) ? 1 : 0;
        vehicle->Stop(timeout, 1);
        return 0;
    }

    vehicle->SetState(1);
    vehicle->active_flag = 1;
    vehicle->move_timer  = 2;

    return 0;
}


/* ================================================================== */
/* GameVehicle::Update — Per-frame queue processor                     */
/* Address: 0x412A80 (vtable slot [10])                                */
/*                                                                      */
/* Verified against disassembly at 0x412A80-0x412AE8.                 */
/* ================================================================== */
void GameVehicle::Update()
{
    Entity::Update();

    if (this->current_vehicle == nullptr) {
        /* No active vehicle: drain the destination queue */
        DestNode* head = this->dest_list_head;
        if (head != nullptr && this->busy_flag == 0) {
            head->vehicle->FindPath((int32_t*)this, 1);

            this->dest_list_head = head->next;
            GLOBAL_free(head);
        }
    } else {
        /* Vehicle assigned: check if its occupant count dropped to 0 */
        if (this->current_vehicle->occupancy == 0) {
            this->current_vehicle = nullptr;
            this->busy_flag       = 0;
        }
    }
}


/* ================================================================== */
/* GameVehicle::AddDestination — Enqueue a vehicle for later delivery  */
/* Address: 0x412AF0                                                   */
/*                                                                      */
/* Verified against Ghidra decompile.                                  */
/* ================================================================== */
void GameVehicle::AddDestination(Vehicle* vehicle)
{
    /* Walk to the tail */
    DestNode* tail = nullptr;
    DestNode* curr = this->dest_list_head;
    while (curr != nullptr) {
        tail = curr;
        curr = curr->next;
    }

    DestNode* node    = (DestNode*)operator_new(sizeof(DestNode));
    node->vehicle     = vehicle;
    node->next        = nullptr;

    if (tail == nullptr) {
        this->dest_list_head = node;
    } else {
        tail->next = node;
    }
}


/* ================================================================== */
/* GameVehicle::RemoveDestination — Remove a vehicle from the queue    */
/* Address: 0x412B50                                                   */
/*                                                                      */
/* Verified against Ghidra decompile.                                  */
/* ================================================================== */
uint8_t GameVehicle::RemoveDestination(uint16_t player_id, uint8_t platform)
{
    DestNode* prev = nullptr;
    DestNode* curr = this->dest_list_head;

    if (curr == nullptr) {
        return 0;
    }

    do {
        Vehicle* veh = curr->vehicle;

        if (veh->player_id == player_id && veh->color_r == platform) {
            /* Found: unlink and free */
            if (prev != nullptr) {
                prev->next = curr->next;
            } else {
                this->dest_list_head = curr->next;
            }
            GLOBAL_free(curr);
            return 1;
        }

        prev = curr;
        curr = curr->next;
    } while (curr != nullptr);

    return 0;
}
