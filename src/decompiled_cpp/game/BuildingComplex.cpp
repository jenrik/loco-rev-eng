/**
 * BuildingComplex.cpp — BuildingMgr singleton core (legacy Ghidra name)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Validated against Ghidra database locon.
 */

#include "BuildingMgr.h"
#include "Building.h"
#include <cstring>

/* These are C++ symbols in loco.exe; do not give them C linkage. */
extern void GLOBAL_free(void* ptr);                              // 0x465cd0
extern void* operator_new(size_t);                               // 0x465ce0
extern void ScriptEngine_constructor(void* lock_object);         // 0x4493a0
extern void RESDATA_ScriptEngine_Dtor(void* lock_object);        // 0x4493f0
extern uint32_t g_game_time;                                     // 0x4a99b4
extern int g_game_mode;                                          // 0x4851f4
extern void RESDATA_Lock(void* lock_object);                     // 0x449410
extern void RESDATA_Unlock(void* lock_object);                   // 0x449420

/* ================================================================== */
/* BuildingCollection — binary vtable slots of the managed collections */
/* ================================================================== */

/** Grow/shrink the items array. Address: 0x435d10 (vtable[0]/[22]).
 *  Shrinks trim trailing NULL slots; allocation is operator_new + zero-fill,
 *  copy of the overlap, then GLOBAL_free of the old array. */
void BuildingCollection::Resize(uint32_t new_capacity)
{
    uint32_t old_capacity = capacity;
    uint32_t cap = new_capacity;

    if (new_capacity < old_capacity) {
        void** scan = &items[old_capacity - 1];
        while (new_capacity < old_capacity) {
            if (*scan != nullptr) break;
            --old_capacity;
            --scan;
        }
        cap = old_capacity;
    }

    void** old_items = items;
    if (cap != 0) {
        items = static_cast<void**>(operator_new(cap * 4));
        memset(items, 0, cap * 4);
    } else {
        items = nullptr;
    }

    if (old_items != nullptr) {
        if (cap != 0) {
            uint32_t copy_count = capacity < cap ? capacity : cap;
            if (copy_count != 0) memcpy(items, old_items, copy_count * 4);
        }
        GLOBAL_free(old_items);
    }

    capacity = (items != nullptr) ? cap : 0;
    if (capacity == 0) items = nullptr;
}

/** vtable[7], 0x424530 — bounds-checked element access. */
Building* BuildingCollection::GetAt(uint32_t index) const
{
    if (index >= capacity) return nullptr;
    return static_cast<Building*>(items[index]);
}

/** vtable[8], 0x424030 — GetAt passthrough. */
Building* BuildingCollection::GetItem(uint32_t index) const
{
    return GetAt(index);
}

/** vtable[11], 0x424000. */
uint32_t BuildingCollection::GetCount() const
{
    return count;
}

/** vtable[12], 0x424760 — index in range and slot occupied. */
bool BuildingCollection::IsSlotOccupied(uint32_t index) const
{
    return index < capacity && items[index] != nullptr;
}

/** vtable[3], 0x4241E0 — shift-remove at index, decrements count.
 *  Returns the removed element (or nullptr for an out-of-range index).
 *  The tail is shifted left by one slot and the last slot is nulled. */
Building* BuildingCollection::RemoveElement(uint32_t index)
{
    Building* removed = GetAt(index);
    if (removed != nullptr) {
        if (index < count - 1) {
            memmove(&items[index], &items[index + 1],
                    static_cast<size_t>(count - index - 1) * sizeof(void*));
        }
        items[count - 1] = nullptr;
        --count;
    }
    return removed;
}

/** vtable[16], 0x424820 — binary search over [low, high] via Comparator. */
uint32_t BuildingCollection::FindItem(Building* target, uint32_t low,
                                      uint32_t high) const
{
    if (high - low > 2) {
        uint32_t mid = low + ((high - low) >> 1);
        if (Comparator(target, GetAt(mid)) < 0)
            return FindItem(target, low, mid);
        return FindItem(target, mid, high);
    }
    while (low <= high && Comparator(target, GetAt(low)) > 0) ++low;
    if (low <= high && target == GetAt(low)) return low;
    return 0xFFFFFFFF;
}

/** vtable[14], 0x4244B0 — count ? FindItem(obj, 0, count-1) : 0xFFFFFFFF. */
uint32_t BuildingCollection::FindIndex(Building* object) const
{
    if (count == 0) return 0xFFFFFFFF;
    return FindItem(object, 0, count - 1);
}

/** vtable[18], 0x435C00 (buildings) / 0x4361E0 (secondary).
 *  Compares the key at a+key_offset with b+key_offset. The width is selected
 *  by key_size: -4/-3 = signed dword, -2 = signed 16-bit, -1 = unsigned
 *  16-bit, >= 0 = memcmp. A zero result falls back to the pointer
 *  difference (tie-break at 0x435C78). */
int BuildingCollection::Comparator(const Building* a, const Building* b) const
{
    const uint8_t* key_a = reinterpret_cast<const uint8_t*>(a) + key_offset;
    const uint8_t* key_b = reinterpret_cast<const uint8_t*>(b) + key_offset;
    int result;
    switch (key_size) {
    case -4:
    case -3:
        result = *reinterpret_cast<const int32_t*>(key_a) -
                 *reinterpret_cast<const int32_t*>(key_b);
        break;
    case -2:
        result = static_cast<int>(*reinterpret_cast<const int16_t*>(key_a)) -
                 static_cast<int>(*reinterpret_cast<const int16_t*>(key_b));
        break;
    case -1:
        result = static_cast<int>(*reinterpret_cast<const uint16_t*>(key_a)) -
                 static_cast<int>(*reinterpret_cast<const uint16_t*>(key_b));
        break;
    default:
        result = memcmp(key_a, key_b, static_cast<size_t>(key_size));
        break;
    }
    if (result == 0) {
        result = static_cast<int>(key_a - key_b);
    }
    return result;
}

/** vtable[17], 0x435B60 — insert at index, shifting the tail right.
 *  Grows via Resize(ftol(count * 1.1)) when the slot would overflow
 *  (0x435B88..0x435BA8). Returns the index, or 0xFFFFFFFF when
 *  index > count. */
uint32_t BuildingCollection::InsertAt(uint32_t index, Building* object)
{
    if (index > count) return 0xFFFFFFFF;
    if (count + 1 > capacity) {
        Resize(static_cast<uint32_t>(static_cast<double>(count) * 1.1));
    }
    if (index != count) {
        memmove(&items[index + 1], &items[index],
                static_cast<size_t>(count - index) * sizeof(void*));
        items[index] = nullptr;
    }
    items[index] = object;
    ++count;
    return index;
}

/** vtable[13], 0x4362B0 — insert preserving sort order.
 *  With an unconfigured key (key_size == 0) the object is appended; otherwise
 *  the insert position is found by scanning with Comparator, guarded by
 *  IsSlotOccupied (vtable[12], 0x424760), then InsertAt (vtable[17]). */
void BuildingCollection::InsertSorted(Building* object)
{
    uint32_t index;
    if (key_size == 0) {
        index = count;
    } else {
        index = 0;
        while (index < count) {
            if (!IsSlotOccupied(index)) break;
            if (Comparator(object, GetAt(index)) <= 0) break;
            ++index;
        }
    }
    InsertAt(index, object);
}

/** vtable[15], 0x435AA0 — recursive Hoare quicksort over [left, right]. */
void BuildingCollection::QuickSortRange(int32_t left, int32_t right)
{
    Building* pivot = GetAt(static_cast<uint32_t>((left + right) / 2));
    int32_t i = left;
    int32_t j = right;
    do {
        while (Comparator(GetAt(static_cast<uint32_t>(i)), pivot) < 0) ++i;
        while (Comparator(pivot, GetAt(static_cast<uint32_t>(j))) < 0) --j;
        if (j < i) break;
        void* tmp = items[i];
        items[i] = items[j];
        items[j] = tmp;
        ++i;
        --j;
    } while (i <= j);
    if (left < j) QuickSortRange(left, j);
    if (i < right) QuickSortRange(i, right);
}

/** vtable[20], 0x4244D0 — sort the whole range when count > 1. */
void BuildingCollection::Sort()
{
    if (count > 1) {
        QuickSortRange(0, static_cast<int32_t>(count) - 1);
    }
}

/** vtable[19], 0x424490 — store the comparison key and re-sort.
 *  The binary dispatches this through the collection vtable after storing
 *  key_offset/key_size; it is the final "ConfigureKey" step of the ctor. */
void BuildingCollection::ConfigureKey(int32_t offset, int32_t size)
{
    key_offset = offset;
    key_size = size;
    Sort();
}

/* ================================================================== */
/* BuildingMgr core                                                    */
/* ================================================================== */

/**
 * Construct the BuildingMgr singleton core.
 * Address: 0x434500 (0x434500..0x4345cd).
 */
BuildingMgr::BuildingMgr()
{
    ScriptEngine_constructor(&building_lock);       // +0x04
    ScriptEngine_constructor(&secondary_lock);      // +0x20

    buildings.items = nullptr;                      // +0x50
    buildings.capacity = 0;                         // +0x54
    buildings.Resize(100);
    buildings.count = 0;                            // +0x58
    buildings.key_offset = 0;                       // +0x5C
    buildings.key_size = 0;                         // +0x60

    secondary_buildings.items = nullptr;            // +0x68
    secondary_buildings.capacity = 0;               // +0x6C
    secondary_buildings.Resize(100);
    secondary_buildings.count = 0;                  // +0x70
    secondary_buildings.key_offset = 0;             // +0x74
    secondary_buildings.key_size = 0;               // +0x78

    building_count = 0;                             // +0x3C
    secondary_count = 0;                            // +0x40
    delayed_update = 0;                             // +0x44

    /* Comparison-key descriptors, dispatched via collection vtable[19]
     * (ConfigureKey, 0x424490). Buildings sort by name (10 bytes at +0x7C);
     * trains by the 32-bit key at +0x0C (comparator 0x4361E0, a - b). */
    buildings.ConfigureKey(0x7c, 10);
    secondary_buildings.ConfigureKey(0x0c, -4);
}

/** Scalar-deleting destructor body.
 * Address: 0x4345d0.
 *
 * MSVC's wrapper calls BaseDtor and conditionally GLOBAL_free(this) according
 * to its hidden delete flags. Natural C++ supplies that wrapper itself.
 */
BuildingMgr::~BuildingMgr()
{
    BaseDtor();
}

/** Destroy inline collections and lock objects in reverse order.
 * Address: 0x4345f0 (0x4345f0..0x43468f).
 */
void BuildingMgr::BaseDtor()
{
    secondary_buildings.count = 0;                  // +0x70
    secondary_buildings.capacity = 0;               // +0x6C
    if (secondary_buildings.items != nullptr) {
        GLOBAL_free(secondary_buildings.items);
        secondary_buildings.items = nullptr;
    }

    buildings.count = 0;                            // +0x58
    buildings.capacity = 0;                         // +0x54
    if (buildings.items != nullptr) {
        GLOBAL_free(buildings.items);
        buildings.items = nullptr;
    }

    RESDATA_ScriptEngine_Dtor(&secondary_lock);      // +0x20
    RESDATA_ScriptEngine_Dtor(&building_lock);       // +0x04
}

/**
 * Apply each object's stored target coordinates (+0xa8/+0xac).
 * Address: 0x434690 (vtable[1]).
 */
void BuildingMgr::UpdateStoredTargets()
{
    if (g_game_mode != 3) {
        return;
    }

    auto update_collection = [](BuildingCollection& collection) {
        uint32_t index = 0;
        if (collection.GetCount() == 0) {
            return;
        }
        do {
            Building* building = collection.GetItem(index);
            /* Building vtable slot 16 (0x432940). The binary dispatches
             * vtable[0x40] with the +0xA8/+0xAC fields as arguments. */
            building->TeleportTo(building->target_x, building->target_y);
            ++index;
        } while (index < collection.GetCount());
    };

    update_collection(buildings);
    update_collection(secondary_buildings);
}

/**
 * Per-frame update of both managed collections.
 * Address: 0x434720 (vtable[2]).
 */
void BuildingMgr::UpdateAll()
{
    if (delayed_update != 0 &&
        static_cast<int32_t>(delayed_update_start) + 300 <
            static_cast<int32_t>(g_game_time)) {
        delayed_update = 0;
    }

    auto update_collection = [](BuildingCollection& collection,
                                bool require_initialized) {
        Building* current = collection.GetItem(0);
        uint32_t index = 0;
        if (collection.GetCount() == 0) {
            return;
        }
        do {
            Building* next = collection.GetItem(++index);
            if (!require_initialized || current->initialized == 1) {
                /* Building vtable slot 15 (0x4327B0) receives the next
                 * object in the chain; the ordinary update path does not
                 * inspect that argument. */
                current->Update(next);
            }
            current = next;
        } while (index < collection.GetCount());
    };

    update_collection(buildings, false);
    update_collection(secondary_buildings, true);

    if (secondary_count > 1) {
        RESDATA_Lock(&secondary_lock);
        secondary_buildings.Sort();                 // vtable[20] = 0x4244D0
        RESDATA_Unlock(&secondary_lock);
    }
}

/**
 * Invalidate every managed object's rectangle. The argument is unused.
 * Address: 0x434800 (vtable[3]).
 */
void BuildingMgr::InvalidateAll(int unused)
{
    (void)unused;
    auto invalidate_collection = [](BuildingCollection& collection) {
        uint32_t index = 0;
        if (collection.GetCount() == 0) {
            return;
        }
        do {
            collection.GetItem(index)->InvalidateRect();   // vtable [1]
            ++index;
        } while (index < collection.GetCount());
    };

    invalidate_collection(buildings);
    invalidate_collection(secondary_buildings);
}

/**
 * Draw/dispatch managed objects for a dirty tile rect.
 * Address: 0x4348a0 (vtable[4]).
 *
 * Faithful to the binary: the secondary (train) collection is drawn FIRST
 * with an early exit when an object's screen_rect.top exceeds the bottom
 * clip; the building collection is drawn second without the exit. Each item
 * receives the clip the binary builds at 0x4348E8..0x4348FA — RECT
 * {dispatch_flags, left, top, bottom} — with `bottom` as the enable_scroll
 * argument and extra_flags = 0. dispatch_flags is the layer-packed value from
 * TileMap_ProcessRect (low word = layer byte), which is 0 in practice, so the
 * clip reads {0, left, top, bottom}. The binary also pops a 6th unused dword
 * (RET 0x18) that TileMap_ProcessRect always pushes as 0.
 */
void BuildingMgr::DispatchAll(int dispatch_flags, int left, int top,
                              int right, int bottom)
{
    if (static_cast<int16_t>(dispatch_flags) > 0) {
        return;
    }

    RECT clip;
    clip.left = dispatch_flags;
    clip.top = left;
    clip.right = top;
    clip.bottom = bottom;

    for (uint32_t i = 0; i < secondary_buildings.GetCount(); ++i) {
        Building* object = secondary_buildings.GetItem(i);
        object->Draw(clip, bottom, 0);
        if (bottom < object->screen_rect.top) break;
    }

    for (uint32_t i = 0; i < buildings.GetCount(); ++i) {
        buildings.GetItem(i)->Draw(clip, bottom, 0);
    }
}
