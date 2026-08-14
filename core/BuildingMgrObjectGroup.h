/**
 * ResourceGameObject (legacy file name BuildingMgrObjectGroup).
 *
 * loco_v8: constructor 0x4580a0, vtable 0x4777d0, resource type 3.
 * The class derives directly from Entity.  It is not part of BuildingMgr's
 * hierarchy; it owns up to five Building/Train objects managed by BuildingMgr.
 */
#pragma once
#include "Entity.h"


// Status: TRANSCRIBED
class Building;

class ResourceGameObject : public Entity {
public:
    ResourceGameObject(const ResourceGameObject&) = delete;
    ResourceGameObject& operator=(const ResourceGameObject&) = delete;

    int16_t sub_pos_x;                // +0x88
    int16_t sub_pos_y;                // +0x8a
    uint8_t member_limit;             // +0x8c, random 1..resource[0x522]
    uint8_t created_count;            // +0x8d
    uint8_t group_flag;               // +0x8e
    uint8_t _pad_8f;
    Building* member_objects[5];      // +0x90..+0xa0
    Building* linked_objects[5];      // +0xa4..+0xb4
    int32_t field_b8;                 // +0xb8
    int32_t field_bc;                 // +0xbc
    uint8_t group_active;             // +0xc0
    uint8_t _pad_c1[3];
    int32_t fields_c4_e0[8];          // +0xc4..+0xe0
    int32_t sentinel_e4;              // +0xe4 = -1
    int32_t fields_e8_104[8];         // +0xe8..+0x104
    int32_t sentinel_108;             // +0x108 = -1

    /** Address: 0x4580a0. */
    explicit ResourceGameObject(int resource_id);
    /** Body 0x458270; scalar deleting wrapper 0x4125e0. */
    ~ResourceGameObject() override;

    /** Vtable [11] override, thunk 0x4343b0 -> Entity::Draw. */
    void Draw(RECT clip_bounds, int enable_scroll, uint32_t extra_flags) override;
    /** Vtable [15], address 0x458430. */
    virtual Building* CreateMember(uint32_t resource_id);
    /** Vtable [16], address 0x458800. */
    virtual uint32_t RestartAnimation();
    /** Vtable [17], address 0x458810. */
    virtual bool IsMemberActionActive();
    /** Vtable [18], address 0x458940. */
    virtual void UpdateScheduledAnimation();
};

/* Source-compatibility alias; this was never a distinct class in loco_v8. */
using BuildingMgrObjectGroup = ResourceGameObject;

#if UINTPTR_MAX == 0xffffffffu
static_assert(offsetof(ResourceGameObject, member_objects) == 0x90,
              "ResourceGameObject member layout mismatch");
static_assert(offsetof(ResourceGameObject, linked_objects) == 0xa4,
              "ResourceGameObject link layout mismatch");
static_assert(sizeof(ResourceGameObject) == 0x10c,
              "ResourceGameObject size mismatch");
#endif
