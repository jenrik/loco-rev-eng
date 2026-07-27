/**
 * HelpPageNode.h — Help page sequencing node
 * Status: TRANSCRIBED
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * HelpPageNode is a small (0x128 byte) class that manages a queue of
 * help-page events. It extends RESDATA_GameVehicle and is used by
 * HelpWnd to sequence tutorial/help page transitions that involve
 * game object animations (e.g., vehicles moving to a station).
 *
 * Vtable: 0x4783D8 (VTBL_HELPPAGE_NODE)
 * Size: 0x128 bytes (RESDATA_GameVehicle 0x11C + 0xC)
 *
 * Vtable layout (extends RESDATA_GameVehicle's 15-slot vtable):
 *   [0]  +0x00: scalar deleting destructor  (0x44F2A0) — OVERRIDDEN
 *   [1]  +0x04: base destructor             (0x44F2C0)
 *   [10] +0x28: Update                      (0x44F340) — OVERRIDDEN
 *   (remaining slots inherited from RESDATA_GameVehicle)
 *
 * Class hierarchy:
 *   GameObject → Entity → ResourceGameObject
 *     → RESDATA_GameVehicle (0x11C bytes, vtable 0x478308)
 *       → HelpPageNode (0x128 bytes, vtable 0x4783D8)  ← this class
 */

#pragma once

#include "../game/ResdataGameVehicle.h"

class Vehicle;

class HelpPageNode : public RESDATA_GameVehicle {
public:
    /* ================================================================ */
    /* Fields (+0x11C..+0x127, extending RESDATA_GameVehicle's 0x11C)   */
    /* ================================================================ */

    int32_t    update_flag;        // +0x11C  0 = idle, 1 = update pending
    int32_t    overlay_flag;       // +0x120  1 = overlay effect active
    int32_t    dest_list_head;     // +0x124  singly-linked list head
                                   //          (each node: Vehicle* at +0x00,
                                   //           next ptr at +0x04)

    /* Total object size: 0x128 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * HelpPageNode constructor. 0x44F210.
     *
     * Chains to RESDATA_GameVehicle(resource_id) for base init.
     * Sets update_flag=0, overlay_flag based on resourceId (1 for
     * 0xC42/0xC44/0xC46/0xC48, 0 otherwise), dest_list_head=0.
     * Also overrides resource_kind to 5 at +0x04 and vehicle_kind to 3
     * at +0x10C (overriding the base RESDATA_GameVehicle values).
     *
     * @param resource_id  Resource ID for the page node (0xC42-0xC48 range)
     */
    explicit HelpPageNode(int resource_id);

    /**
     * Destructor. 0x44F2C0.
     *
     * Frees the destination linked list at dest_list_head (+0x124),
     * then chains to ~RESDATA_GameVehicle() for base cleanup.
     */
    ~HelpPageNode() override;

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Update — Per-frame page event processor (vtable[10] override).
     * Address: 0x44F340.
     *
     * Overrides RESDATA_GameVehicle::Update. Calls Entity::Update for
     * base animation, then checks dest_list_head for pending page data.
     * If a node is queued and update_flag is 0:
     *   1. Sets update_flag = 1
     *   2. Dequeues the vehicle from the front of the list
     *   3. Frees the dequeued node
     *   4. Calls vehicle->LoadSounds, vehicle->SetState(2),
     *      vehicle->field_60 = 4
     */
    void Update() override;
};

/* Compile-time size verification */
#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(HelpPageNode) == 0x128,
              "HelpPageNode size mismatch (expected 0x128)");
static_assert(offsetof(HelpPageNode, update_flag) == 0x11C,
              "HelpPageNode::update_flag offset mismatch");
static_assert(offsetof(HelpPageNode, overlay_flag) == 0x120,
              "HelpPageNode::overlay_flag offset mismatch");
static_assert(offsetof(HelpPageNode, dest_list_head) == 0x124,
              "HelpPageNode::dest_list_head offset mismatch");
#endif
