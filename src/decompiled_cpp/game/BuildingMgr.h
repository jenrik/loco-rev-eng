/**
 * BuildingMgr.h — Building manager singleton at 0x485448.
 *
 * Validated against Ghidra database loco_v8.  The function at 0x434500 and
 * vtable 0x477f70 belong to this class; "BuildingComplex" was a legacy
 * decompiler name, not a base or derived class.
 */
#pragma once

#include "../shared/types.h"

class Building;

class BuildingCollectionLock {
public:
    uint8_t critical_section[0x18];
    virtual ~BuildingCollectionLock() {}
};

class BuildingCollection {
public:
    void** items;            // +0x04
    uint32_t capacity;       // +0x08
    uint32_t count;          // +0x0c
    int32_t key_offset;      // +0x10
    int32_t key_size;        // +0x14

    virtual ~BuildingCollection() {}
    void Resize(uint32_t new_capacity);                 // 0x435d10
    void ConfigureKey(int32_t offset, int32_t size);    // 0x424490
    Building* GetItem(uint32_t index) const;            // vtable [8]
    uint32_t GetCount() const;                          // vtable [11]
    void Sort();                                        // vtable [20]
};

class BuildingMgr {
public:
    BuildingCollectionLock building_lock;       // +0x04
    BuildingCollectionLock secondary_lock;      // +0x20
    int32_t building_count;                     // +0x3c
    int32_t secondary_count;                    // +0x40
    uint8_t delayed_update;                     // +0x44
    uint8_t _pad_45[3];                         // +0x45
    uint32_t delayed_update_start;              // +0x48
    BuildingCollection buildings;               // +0x4c
    BuildingCollection secondary_buildings;     // +0x64

    /** Address: 0x434500. */
    BuildingMgr();
    /** Scalar wrapper 0x4345d0; body 0x4345f0. */
    virtual ~BuildingMgr();
    void BaseDtor();

    /** Vtable [1], address 0x434690. */
    virtual void UpdateStoredTargets();
    /** Vtable [2], start 0x434720 (0x434777 is an internal call). */
    virtual void UpdateAll();
    /** Vtable [3], address 0x434800. */
    virtual void InvalidateAll(int unused);
    /** Vtable [4], address 0x4348a0. */
    virtual void DispatchAll(int dispatch_flags, int left, int top,
                             int right, int bottom);

    /** Address: 0x434870. */
    void CompactCollections();
    /** Start 0x4349d0; 0x434af7 is an internal constructor call site. */
    Building* CreateFromResource(int resource_id, int owner_slot,
                                 int world_x, int world_y);
    /** Address: 0x434970. */
    void RemoveEmpty();
    /** Address: 0x434b60. */
    void RemoveObject(Building* object, bool show_message);
    /** Address: 0x434c50. */
    bool FindAndNotify(int world_x, int world_y);
    /** Address: 0x435020. */
    int InvalidateRects(RECT rect);
    /** Address: 0x435200. */
    int BlitOverlaps(int left, int top, int right, int bottom,
                     Building* target);
    /** Address: 0x435580. */
    void HandleClick(void* command, int left, int top, int right, int bottom);
};

#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(BuildingCollectionLock) == 0x1c, "lock layout mismatch");
static_assert(sizeof(BuildingCollection) == 0x18, "collection layout mismatch");
static_assert(sizeof(BuildingMgr) == 0x7c, "BuildingMgr layout mismatch");
#endif
