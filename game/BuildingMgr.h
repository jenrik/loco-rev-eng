// Status: INTEGRATED
/**
 * BuildingMgr.h — Building manager singleton at 0x485448.
 *
 * Validated instruction-by-instruction against Ghidra database locon
 * (loco.exe, 1998, MSVC x86). The constructor at 0x434500 and vtable
 * 0x477F70 belong to this class; "BuildingComplex" was a legacy decompiler
 * name, not a base or derived class (see BuildingComplex.h).
 *
 * Binary layout (BuildingMgr, 0x7C bytes, vtable 0x477F70):
 *   +0x00 vtable — 5 slots:
 *     [0] ~BuildingMgr        0x4345D0 (scalar wrapper; body 0x4345F0)
 *     [1] UpdateStoredTargets 0x434690
 *     [2] UpdateAll           0x434720
 *     [3] InvalidateAll       0x434800
 *     [4] DispatchAll         0x4348A0
 *   +0x04 building_lock        (BuildingCollectionLock, 0x1C)
 *   +0x20 secondary_lock       (BuildingCollectionLock, 0x1C)
 *   +0x3C building_count       +0x40 secondary_count
 *   +0x44 delayed_update       +0x48 delayed_update_start
 *   +0x4C buildings            (BuildingCollection, 0x18)
 *   +0x64 secondary_buildings  (BuildingCollection, 0x18)
 *
 * Each managed collection is a sorted pointer array. In the binary the two
 * instances carry different vtables — 0x478018 (buildings) and 0x477F88
 * (secondary) — which are installed after the first Resize and differ only in
 * the Comparator slot ([18]). Slot numbers and addresses below are binary
 * documentation; C++ manages the vtables.
 */
#pragma once

#include "../shared/types.h"

class Building;
class TrainEntity;

/**
 * Click command consumed by BuildingMgr::HandleClick (0x435580).
 * Offsets verified against the reads at 0x435580: filter +0x08 (pointer,
 * dereferenced), action +0x14, argument +0x18 (int16, sign-extended),
 * delay +0x1C.
 */
struct BuildingClickCommand {
    uint8_t         prefix_00_07[8];    // +0x00..+0x07 not read by HandleClick
    const int32_t*  filter;             // +0x08 resource-id filter; *filter == -1 matches all
    uint8_t         prefix_0c_13[8];    // +0x0C..+0x13 not read by HandleClick
    int32_t         action;             // +0x14 0 = play sound, otherwise InitBase action
    int16_t         argument;           // +0x18 sound id / animation index
    uint8_t         _pad_1a[2];         // +0x1A
    int32_t         delay;              // +0x1C cooldown ticks added to g_game_time
};

class BuildingCollectionLock {
public:
    uint8_t critical_section[0x18];
    virtual ~BuildingCollectionLock() {}
};

/**
 * Sorted pointer collection (0x18 bytes: vtable, items, capacity, count,
 * key_offset, key_size). Each method documents its binary vtable slot and
 * address; the binary vtable (26 slots) is shared with the UI list classes.
 */
class BuildingCollection {
public:
    void** items;            // +0x04
    uint32_t capacity;       // +0x08
    uint32_t count;          // +0x0c
    int32_t key_offset;      // +0x10
    int32_t key_size;        // +0x14

    virtual ~BuildingCollection() {}

    /** vtable[0] / [22], direct call 0x435D10 (Timer_Resize). */
    virtual void Resize(uint32_t new_capacity);
    /** vtable[3], 0x4241E0 — shift-remove at index, decrements count. */
    virtual Building* RemoveElement(uint32_t index);
    /** vtable[7], 0x424530 — items[index], nullptr if index >= capacity. */
    virtual Building* GetAt(uint32_t index) const;
    /** vtable[8], 0x424030 — GetAt passthrough. */
    virtual Building* GetItem(uint32_t index) const;
    /** vtable[11], 0x424000. */
    virtual uint32_t GetCount() const;
    /** vtable[12], 0x424760 — index in range and slot non-null. */
    virtual bool IsSlotOccupied(uint32_t index) const;
    /** vtable[13], 0x4362B0 — insert preserving sort order. */
    virtual void InsertSorted(Building* object);
    /** vtable[14], 0x4244B0 — count ? FindItem(obj, 0, count-1) : -1. */
    virtual uint32_t FindIndex(Building* object) const;
    /** vtable[15], 0x435AA0 — recursive Hoare quicksort over [left, right]. */
    virtual void QuickSortRange(int32_t left, int32_t right);
    /** vtable[16], 0x424820 — binary search over the sorted range. */
    virtual uint32_t FindItem(Building* target, uint32_t low, uint32_t high) const;
    /** vtable[17], 0x435B60 — insert at index, shift tail, ++count. */
    virtual uint32_t InsertAt(uint32_t index, Building* object);
    /** vtable[18], 0x435C00 (buildings) / 0x4361E0 (secondary) — key compare (a - b). */
    virtual int Comparator(const Building* a, const Building* b) const;
    /** vtable[19], 0x424490 — store key config and re-sort. */
    virtual void ConfigureKey(int32_t offset, int32_t size);
    /** vtable[20], 0x4244D0 — QuickSortRange(0, count-1) when count > 1. */
    virtual void Sort();
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
    /** Vtable [2], address 0x434720. */
    virtual void UpdateAll();
    /** Vtable [3], address 0x434800; the int argument is unused (RET 4). */
    virtual void InvalidateAll(int unused);
    /** Vtable [4], address 0x4348A0. The binary pops a 6th unused dword
     *  (RET 0x18); it is always 0 from TileMap_ProcessRect and never read. */
    virtual void DispatchAll(int dispatch_flags, int left, int top,
                             int right, int bottom);

    /** Address: 0x434870. */
    void CompactCollections();
    /** Start 0x4349d0; 0x434af7 is Building::Building, not the entry. */
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
    void HandleClick(const BuildingClickCommand* command, int left, int top,
                     int right, int bottom);
};

#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(BuildingCollectionLock) == 0x1c, "lock layout mismatch");
static_assert(sizeof(BuildingCollection) == 0x18, "collection layout mismatch");
static_assert(sizeof(BuildingMgr) == 0x7c, "BuildingMgr layout mismatch");
#endif
