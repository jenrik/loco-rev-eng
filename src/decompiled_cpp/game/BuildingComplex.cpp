/**
 * BuildingComplex.cpp — BuildingMgr singleton core (legacy Ghidra name)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Validated against Ghidra database loco_v8.
 */

#include "BuildingMgr.h"
#include "Building.h"

/* These are C++ symbols in loco.exe; do not give them C linkage. */
extern void GLOBAL_free(void* ptr);                              // 0x465cd0
extern void ScriptEngine_constructor(void* lock_object);         // 0x4493a0
extern void RESDATA_ScriptEngine_Dtor(void* lock_object);        // 0x4493f0
extern void Timer_Resize(void* collection, unsigned capacity);   // 0x435d10
extern void Collection_Sort(void* collection);                   // 0x4244d0
extern uint32_t g_game_time;                                     // 0x4a99b4
extern int g_game_mode;                                          // 0x4851f4
extern void RESDATA_Lock(void* lock_object);                     // 0x449410
extern void RESDATA_Unlock(void* lock_object);                   // 0x449420

/** Collection resize entry used by all four collection vtables.
 * Address: 0x435d10.
 */
void BuildingCollection::Resize(uint32_t new_capacity)
{
    Timer_Resize(this, new_capacity);
}

/** Configure the key used by the sorted collection.
 * Address: 0x424490.
 *
 * The binary invokes vtable[20] after storing these fields. At construction
 * time the collection is empty, so there is nothing to reorder here.
 */
void BuildingCollection::ConfigureKey(int32_t offset, int32_t size)
{
    key_offset = offset;
    key_size = size;
    Sort();
}

/** Get a managed object by index (vtable[8] wrapper at 0x424030). */
Building* BuildingCollection::GetItem(uint32_t index) const
{
    return static_cast<Building*>(items[index]);
}

/** Return logical item count (vtable[11] implementation at 0x424000). */
uint32_t BuildingCollection::GetCount() const
{
    return count;
}

/** Sort by the configured key (final collection vtable slot 20, 0x4244d0). */
void BuildingCollection::Sort()
{
    Collection_Sort(this);
}

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
    buildings.key_offset = 0;                       // +0x5c
    buildings.key_size = 0;                         // +0x60

    secondary_buildings.items = nullptr;            // +0x68
    secondary_buildings.capacity = 0;               // +0x6c
    secondary_buildings.Resize(100);
    secondary_buildings.count = 0;                  // +0x70
    secondary_buildings.key_offset = 0;             // +0x74
    secondary_buildings.key_size = 0;               // +0x78

    building_count = 0;                             // +0x3c
    secondary_count = 0;                            // +0x40
    delayed_update = 0;                             // +0x44

    /* These are comparison-key descriptors, not timer intervals. */
    buildings.ConfigureKey(0x7c, 10);               // Building::name[10]
    secondary_buildings.ConfigureKey(0x0c, -4);     // descending 32-bit key
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
    secondary_buildings.capacity = 0;               // +0x6c
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
            /* Building vtable slot 16 (0x432940). The existing source names
             * this operation TeleportTo; assembly shows it sets/replans the
             * target held at +0xa8/+0xac. */
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
    if (delayed_update != 0 && delayed_update_start + 300 < g_game_time) {
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
                /* 0x4327b0 receives the next object in the chain. The
                 * ordinary update path does not inspect that argument. */
                current->Update(next);
            }
            current = next;
        } while (index < collection.GetCount());
    };

    update_collection(buildings, false);
    update_collection(secondary_buildings, true);

    if (secondary_count > 1) {
        RESDATA_Lock(&secondary_lock);
        secondary_buildings.Sort();
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
            collection.GetItem(index)->InvalidateRect();
            ++index;
        } while (index < collection.GetCount());
    };

    invalidate_collection(buildings);
    invalidate_collection(secondary_buildings);
}

/**
 * Draw/dispatch managed objects for a dirty rectangle.
 * Address: 0x4348a0 (vtable[4]).
 */
void BuildingMgr::DispatchAll(int dispatch_flags, int left, int top,
                                  int right, int bottom)
{
    if (static_cast<int16_t>(dispatch_flags) > 0) {
        return;
    }

    RECT clip = {left, top, right, bottom};

    for (uint32_t i = 0; i < buildings.GetCount(); ++i) {
        Building* building = buildings.GetItem(i);
        building->Draw(clip, 0, 0);
        if (bottom < building->screen_rect.top) {
            break;
        }
    }

    for (uint32_t i = 0; i < secondary_buildings.GetCount(); ++i) {
        secondary_buildings.GetItem(i)->Draw(clip, 0, 0);
    }
}
