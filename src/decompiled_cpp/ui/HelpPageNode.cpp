/**
 * HelpPageNode.cpp — Help page sequencing node implementation
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

namespace {
struct DestinationNode32 {
    void* vehicle;
    uint32_t next;
};
}

    extern void* __cdecl operator_new(size_t size);         /* 0x465CE0 */
    extern void  __cdecl GLOBAL_free(void* ptr);            /* 0x465CD0 */

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
        this->dest_list_head = 0;                /* +0x124 */
        break;
    default:
        this->overlay_flag = 0;
        this->dest_list_head = 0;
        break;
    }
}

/** HelpPageNode::~HelpPageNode
 *  Address: 0x44F2C0 */
HelpPageNode::~HelpPageNode()
{
    /* Free linked list nodes at dest_list_head (+0x124).
     * Each node is { Vehicle* vehicle; DestNode* next; }. */
    int next = this->dest_list_head;
    while (next != 0) {
        const auto* node = reinterpret_cast<const DestinationNode32*>(
            static_cast<uintptr_t>(static_cast<uint32_t>(next)));
        int tmp = static_cast<int32_t>(node->next);    /* next->next */
        GLOBAL_free(reinterpret_cast<void*>(
            static_cast<uintptr_t>(static_cast<uint32_t>(next))));
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
    int pageData = this->dest_list_head;
    if (pageData != 0 && this->update_flag == 0) {
        this->update_flag = 1;

        /* Pop first node from list */
        const auto* node = reinterpret_cast<const DestinationNode32*>(
            static_cast<uintptr_t>(static_cast<uint32_t>(pageData)));
        void* vehicle = node->vehicle;                         /* node->vehicle */
        this->dest_list_head = static_cast<int32_t>(node->next); /* node->next */
        GLOBAL_free(reinterpret_cast<void*>(
            static_cast<uintptr_t>(static_cast<uint32_t>(pageData))));

        /* Load sounds and set vehicle state */
        Vehicle_LoadSounds(vehicle, reinterpret_cast<int*>(this), 0);
        Vehicle_SetState(vehicle, 2);
        *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(vehicle) + 0x60) = 4;
                                                                  /* action_state = 4 */
    }
}
