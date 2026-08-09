/**
 * HelpPageNode.cpp — Road-tile vehicle queue / help-page node implementation
 * Status: TRANSCRIBED
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "HelpPageNode.h"
#include "../game/Vehicle.h"
#include <cstdint>

/* ================================================================== */
/* External references                                                  */
/* ================================================================== */

extern void* __cdecl operator_new(size_t size);         /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);             /* 0x465CD0 */

/* Base class helpers — these are the Ghidra-named wrappers for
 * RESDATA_GameVehicle constructor and base destructor. In the binary
 * they are member functions; once RESDATA_GameVehicle.cpp is fully
 * integrated, these will become proper C++ constructor/destructor calls.
 *
 * TODO: Replace with direct C++ constructor/destructor calls when
 * RESDATA_GameVehicle::RESDATA_GameVehicle(int) and
 * RESDATA_GameVehicle::~RESDATA_GameVehicle() are fully integrated.
 */
/* Vehicle methods */
extern void  Vehicle_LoadSounds(void* vehicle, int* pageData, char flag);
extern void  Vehicle_SetState(void* vehicle, int state);

/* GameObject update */
extern void  GameObject_Update(void* obj);                /* 0x436AE0 */

/* ================================================================== */
/* HelpPageNode constructor — 0x44F210                                 */
/* ================================================================== */
HelpPageNode::HelpPageNode(int resource_id)
    : RESDATA_GameVehicle(resource_id)
{
    /* The binary manually sets vtable to 0x4783D8 here.
     * In natural C++, the compiler manages the vtable. */

    /* Configure page-specific fields.
     * Override base-class defaults set by RESDATA_GameVehicle::RESDATA_GameVehicle.
     * RESDATA_GameVehicle sets this->type (offset +0x04) to 4;
     * HelpPageNode overrides it to 5. */
    this->type = 5;                             /* override base type from 4 → 5 */

    this->vehicle_kind = 3;                 /* +0x10C — override base default (0) */

    this->update_flag = 0;                       /* +0x11C */

    /* Set overlay_flag based on resource ID range */
    switch (resource_id) {
    case 0xC42:
    case 0xC44:
    case 0xC46:
    case 0xC48:
        this->overlay_flag = 1;                  /* +0x120 */
        this->dest_list_head = nullptr;          /* +0x124 */
        break;
    default:
        this->overlay_flag = 0;
        this->dest_list_head = nullptr;
        break;
    }
}

/** HelpPageNode::~HelpPageNode
 *  Address: 0x44F2C0 */
HelpPageNode::~HelpPageNode()
{
    /* Free linked list nodes at dest_list_head (+0x124).
     * Each node is { Vehicle* vehicle; DestNode* next; }. */
    DestNode* next = this->dest_list_head;
    while (next != nullptr) {
        DestNode* tmp = next->next;
        GLOBAL_free(next);
        next = tmp;
    }

    /* Compiler auto-calls ~RESDATA_GameVehicle() after destructor body.
     * Explicit base destructor call removed — the binary's call is modeled
     * by C++'s automatic base-class destruction. */
}

/** HelpPageNode::Update — vtable[10]
 *  Address: 0x44F340 */
void HelpPageNode::Update()
{
    /* Update the base GameObject state */
    GameObject_Update(this);

    /* Check if there is pending page data and update flag is clear */
    DestNode* pageData = this->dest_list_head;
    if (pageData != nullptr && this->update_flag == 0) {
        this->update_flag = 1;

        /* Pop first node from list */
        void* vehicle = pageData->vehicle;
        this->dest_list_head = pageData->next;
        GLOBAL_free(pageData);

        /* Load sounds and set vehicle state */
        Vehicle_LoadSounds(vehicle, reinterpret_cast<int*>(this), 0);
        Vehicle_SetState(vehicle, 2);
        *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(vehicle) + 0x60) = 4;
                                                                  /* action_state = 4 */
    }
}

/* ================================================================== */
/* HelpPageNode::AddVehicle                                            */
/* Address: 0x44F3A0 (35 instructions, non-virtual, direct call)       */
/*                                                                      */
/* Called by: World::FinalizeLoad (0x44DFFE),                          */
/*            VehicleEditor_Update (0x44C736, 0x44C84A, not yet ported)*/
/*                                                                      */
/* Formerly transcribed as the free function                           */
/* ArrivalQueue_AddVehicle(void* self, void* vehicle) — see             */
/* docs/landmine-sweep-worklist.md's ArrivalQueue section for the       */
/* evidence chain that identified `self` as HelpPageNode*.              */
/* ================================================================== */
void HelpPageNode::AddVehicle(Vehicle* vehicle)
{
    /* Step 1: read this node's packed tile position (dword at +0x88,
     * ResourceGameObject::sub_pos_x/sub_pos_y) before mutating the
     * vehicle, matching the original instruction order
     * (MOV EAX,[EDI+0x88] precedes both vehicle writes). */
    int32_t tile_pos = this->tile_target();

    /* Step 2: Set vehicle direction to 2 (EDGE_OF_MAP/ARRIVING) */
    vehicle->direction = 2;                                    /* +0x60 */

    /* Step 3: Copy this node's tile position into the vehicle's own
     * tile_x/tile_y (+0x2E/+0x30). The original does a single 32-bit
     * store spanning both fields; they are adjacent int16_t with no
     * padding, so two int16 stores are byte-identical (same idiom as
     * Vehicle::LoadSounds's analogous GameVehicle::sub_pos_x/y copy). */
    vehicle->tile_x = static_cast<int16_t>(tile_pos & 0xFFFF);
    vehicle->tile_y = static_cast<int16_t>((tile_pos >> 16) & 0xFFFF);

    /* Step 4: Set vehicle state to 0 (STOPPED) */
    vehicle->SetState(0);

    /* Step 5: Allocate a new queue node and append to the tail of
     * dest_list_head (+0x124). */
    DestNode* node = static_cast<DestNode*>(operator_new(sizeof(DestNode)));
    node->vehicle = vehicle;
    node->next = nullptr;

    DestNode* tail = this->dest_list_head;
    if (tail == nullptr) {
        this->dest_list_head = node;
    } else {
        while (tail->next != nullptr) {
            tail = tail->next;
        }
        tail->next = node;
    }
}

/* ================================================================== */
/* HelpPageNode::RemoveVehicle                                         */
/* Address: 0x44F410 (57 instructions, non-virtual, direct call)       */
/*                                                                      */
/* Called by: World_RenderAll (0x44E683)                                */
/*                                                                      */
/* Formerly transcribed as the free function                           */
/* ArrivalQueue_RemoveVehicle(void* self, uint16_t player_id,           */
/* byte color) — see docs/landmine-sweep-worklist.md's ArrivalQueue     */
/* section for the evidence chain.                                      */
/* ================================================================== */
uint8_t HelpPageNode::RemoveVehicle(uint16_t player_id, uint8_t color)
{
    DestNode* prev = nullptr;
    DestNode* curr = this->dest_list_head;

    if (curr == nullptr) {
        return 0;
    }

    do {
        Vehicle* veh = curr->vehicle;

        if (veh->player_id == player_id && veh->color_r == color) {
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
