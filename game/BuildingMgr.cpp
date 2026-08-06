/** BuildingMgr non-vtable operations, validated against Ghidra locon. */

// Status: TRANSCRIBED
#include "BuildingMgr.h"
#include "Building.h"
#include "Train.h"
#include <cstring>
#include <new>

extern void* operator_new(size_t);                              // 0x465ce0
extern void* ResourceManager_GetById(void*, int);               // 0x446ea0
extern unsigned int GetResourceType(unsigned int);              // 0x446030
extern void RESDATA_Lock(void*);                                // 0x449410
extern void RESDATA_Unlock(void*);                              // 0x449420
extern void __thiscall UI_CreateMessageBox(void* self, int, int, char, int, int, int); // 0x423ab0, thiscall + 6 stack
extern int __thiscall Town_CheckOccupied(void* self, int, int, int, int);       // 0x42c950, thiscall + 4 stack
extern int UIPANEL_BlitSurface(void*, int, int, void*, int, int); // 0x42a540
extern void Game_SelectGameObject(void*, void*);                // 0x4113a0
extern void Town_SelectBuilding(void*, void*);                  // 0x42d040
extern void PlaySound(int);                                     // 0x447930
extern void DDRAW_SelectBuilding(void*, void*);                  // 0x459180
extern BOOL (__stdcall* g_IntersectRect)(RECT*, const RECT*, const RECT*);
extern BOOL (__stdcall* g_OffsetRect)(RECT*, int, int);
extern BOOL (__stdcall* g_PtInRect)(const RECT*, int, int);

extern void* g_resmgr;          // 0x4855e8
extern int g_game_mode;         // 0x4851f4
extern void* g_tooltip_mgr;     // 0x4fd220
extern void* g_game;            // 0x4854c8
extern void* g_town_view;       // 0x4852a0
extern uint8_t g_click_on_building; // 0x48556c
extern uint8_t g_click_on_town;     // 0x48557c
extern uint8_t g_disable_input;     // 0x4855ac
extern uint32_t g_game_time;        // 0x4a99b4
extern uint8_t g_is_town_mode;      // 0x485328
extern uint8_t g_ddraw_active;      // 0x4a9f78
extern uint16_t g_game_difficulty;  // 0x4aa288
extern void* g_ddraw_building;      // 0x4a9ef0

namespace {

/* RESDATA factory fields read by CreateFromResource (0x4349D0):
 * dependency_id +0x40, exclusion_id +0x44, enabled (uint16) +0x158. */
struct ResourceFactoryFields {
    uint8_t prefix_00_3f[0x40];     // 0x00..0x3F
    int32_t dependency_id;          // 0x40
    int32_t exclusion_id;           // 0x44
    uint8_t prefix_48_157[0x110];   // 0x48..0x157
    uint16_t enabled;               // 0x158
};

/* RESDATA (resource) layout viewed as a building's parent. Offsets verified:
 * resource_id +0x04 (0x435580), type +0x08 (0x434B60), surface +0x10
 * (0x435020/0x435200), z_limit +0x16A (0x43524A), removable +0x16C
 * (0x4349AA). */
struct BuildingParentFields {
    uint8_t prefix_00_03[4];        // 0x00..0x03
    int32_t resource_id;            // 0x04
    uint8_t type;                   // 0x08
    uint8_t prefix_09_0f[7];        // 0x09..0x0F
    void* surface;                  // 0x10
    uint8_t prefix_14_169[0x156];   // 0x14..0x169
    uint8_t z_limit;                // 0x16A
    uint8_t prefix_16b[1];          // 0x16B
    uint8_t removable;              // 0x16C
};

static const ResourceFactoryFields* resource_factory_fields(const void* resource)
{
    return reinterpret_cast<const ResourceFactoryFields*>(resource);
}

static const BuildingParentFields* building_parent_fields(const Entity* parent)
{
    return reinterpret_cast<const BuildingParentFields*>(parent);
}

/* Surface pointer at parent+0x10 (0x435020, 0x435200). */
void* entity_surface(const Building* object)
{
    return object->parent == nullptr
        ? nullptr : building_parent_fields(object->parent)->surface;
}

/* Shared body of InvalidateRects (0x435020): intersect clip with each visible
 * item's screen rect, translate to item-local coordinates, then test tile
 * occupancy. Returns result_code on first occupied tile. */
int collection_occupancy(BuildingCollection& collection, const RECT& clip,
                         int result_code)
{
    for (uint32_t i = 0; i < collection.GetCount(); ++i) {
        Building* object = collection.GetItem(i);
        if (object == nullptr || object->visible == 0) continue;
        RECT intersection;
        if (!g_IntersectRect(&intersection, &clip, &object->screen_rect)) continue;
        g_OffsetRect(&intersection, -object->screen_rect.left,
                     -object->screen_rect.top);
        g_OffsetRect(&intersection, object->source_rect.left, 0);
        if (Town_CheckOccupied(entity_surface(object), intersection.left,
                               intersection.top, intersection.right,
                               intersection.bottom)) return result_code;
    }
    return 0;
}
}

/** Compact the primary keyed collection. Address: 0x434870. */
void BuildingMgr::CompactCollections()
{
    /* Binary uses carry-based (unsigned) branch, not signed comparison.
     * Cast to unsigned to match raw semantics for 0x80000000..0xFFFFFFFF. */
    if (static_cast<uint32_t>(building_count) <= 1) return;
    RESDATA_Lock(&building_lock);
    buildings.Sort();                // collection vtable[20] = 0x4244D0
    RESDATA_Unlock(&building_lock);
}

/**
 * Factory body. Address: 0x4349d0.
 * 0x434af7 is the call to Building::Building, not the function entry.
 * Validates dependency/exclusion resources, allocates Building (type 7,
 * 0xF4) or TrainEntity (type 8, 0xF0), inserts via collection vtable[13]
 * (InsertSorted, 0x4362B0) and bumps the manager's own count (+0x3C/+0x40).
 */
Building* BuildingMgr::CreateFromResource(int resource_id, int owner_slot,
                                          int world_x, int world_y)
{
    void* resource = ResourceManager_GetById(g_resmgr, resource_id);
    /* Host-safety: the binary dereferences the resource unconditionally
     * (iVar1 + 0x40); a null primary lookup would crash the original. */
    if (resource == nullptr) return nullptr;

    const ResourceFactoryFields* resource_fields = resource_factory_fields(resource);
    int dependency_id = resource_fields->dependency_id;
    void* dependency = ResourceManager_GetById(g_resmgr, dependency_id);
    if (dependency_id != -1 &&
        (dependency == nullptr || resource_factory_fields(dependency)->enabled == 0))
        return nullptr;

    int exclusion_id = resource_fields->exclusion_id;
    void* exclusion = ResourceManager_GetById(g_resmgr, exclusion_id);
    if (exclusion != nullptr && resource_factory_fields(exclusion)->enabled != 0)
        return nullptr;

    Building* object = nullptr;
    uint8_t type = GetResourceType(resource_id);
    if (type == 7) {
        void* memory = operator_new(0xf4);
        if (memory != nullptr) object = ::new (memory) Building(resource_id);
    } else if (type == 8) {
        void* memory = operator_new(0xf0);
        if (memory != nullptr)
            object = static_cast<Building*>(::new (memory) TrainEntity(resource_id));
    }
    if (object == nullptr) return nullptr;
    if (object->initialized != 1) {
        delete object;
        return nullptr;
    }

    object->occupant_a = reinterpret_cast<Entity*>(static_cast<intptr_t>(owner_slot)); // +0x8c
    object->MoveTo(world_x, world_y);                               // vtable [3]
    if (type == 7) {
        buildings.InsertSorted(object);                             // +0x4C, vtable[13]
        ++building_count;                                           // +0x3C
    } else {
        secondary_buildings.InsertSorted(object);                   // +0x64, vtable[13]
        ++secondary_count;                                          // +0x40
    }
    return object;
}

/**
 * Re-parent removable, empty buildings. Address: 0x434970.
 * For each building with occupant_b == 0 and the parent's removable flag
 * (+0x16C) set, the binary calls Building vtable[20] = PostMoveDispatch
 * (0x433CA0), which detaches from the scene-graph parent and re-attaches to
 * a compatible host — it does NOT delete the object.
 */
void BuildingMgr::RemoveEmpty()
{
    if (g_game_mode != 3) return;
    for (uint32_t i = 0; i < buildings.GetCount(); ++i) {
        Building* object = buildings.GetItem(i);
        if (object != nullptr && object->occupant_b == nullptr &&
            building_parent_fields(object->parent)->removable != 0) {
            object->PostMoveDispatch();            // vtable [20] = 0x433CA0
        }
    }
}

/**
 * Remove one managed Building/Train. Address: 0x434b60.
 * Finds the object via collection vtable[14] (FindIndex, 0x4244B0), removes
 * it via vtable[3] (RemoveElement, 0x4241E0 — shift + --count), then deletes
 * it only if the removed slot was the requested object.
 */
void BuildingMgr::RemoveObject(Building* object, bool show_message)
{
    if (object == nullptr || object->parent == nullptr) return;
    uint8_t type = building_parent_fields(object->parent)->type;   // +0x08
    BuildingCollection* collection;
    BuildingCollectionLock* lock;
    int32_t* managed_count;
    if (type == 7) {
        collection = &buildings; lock = &building_lock; managed_count = &building_count;
    } else if (type == 8) {
        collection = &secondary_buildings; lock = &secondary_lock; managed_count = &secondary_count;
    } else return;

    RESDATA_Lock(lock);
    uint32_t index = collection->FindIndex(object);          // vtable[14] = 0x4244B0
    Building* found = collection->RemoveElement(index);      // vtable[3]  = 0x4241E0
    if (found == object) {
        --*managed_count;
        if (g_game_mode == 3 && show_message)
            UI_CreateMessageBox(&g_tooltip_mgr, 0x3860, 0, 'W',
                                object->world_x, object->world_y, 1);
        delete object;                                       // vtable[0](1)
    }
    RESDATA_Unlock(lock);
}

/**
 * Select the first object hit at a world position. Address: 0x434c50.
 * Buildings require visible == 1 and are not gated by g_disable_input;
 * trains have no visibility requirement but honour g_disable_input.
 */
bool BuildingMgr::FindAndNotify(int world_x, int world_y)
{
    if (g_game_mode != 3) return false;
    auto find = [&](BuildingCollection& collection, bool require_visible,
                    bool respect_disable) {
        for (uint32_t i = 0; i < collection.GetCount(); ++i) {
            Building* object = collection.GetItem(i);
            if (object == nullptr || (require_visible && object->visible != 1)) continue;
            if (!object->PtInRect(world_x, world_y)) continue;   // vtable [2]
            if (g_click_on_building && (!respect_disable || !g_disable_input))
                Game_SelectGameObject(g_game, object);
            if (g_click_on_town) Town_SelectBuilding(g_town_view, object);
            return true;
        }
        return false;
    };
    return find(buildings, true, false) ||
           find(secondary_buildings, false, true);
}

/**
 * Tile occupancy query. Address: 0x435020.
 * Returns 7 when a building tile is occupied, 8 for a train tile, else 0.
 * The binary accumulates the result at a stack slot (7 at 0x4350F1, 8 at
 * 0x4351CF) and short-circuits the second collection.
 */
int BuildingMgr::InvalidateRects(RECT rect)
{
    int result = collection_occupancy(buildings, rect, 7);
    return result != 0 ? result : collection_occupancy(secondary_buildings, rect, 8);
}

/**
 * Overlap query. Address: 0x435200.
 * With a null target, forwards to InvalidateRects. Otherwise tests every
 * visible object (except the target) within the parent's z_limit (+0x16A):
 * buildings via Town_CheckOccupied on both rects (returns 7), trains via
 * UIPANEL_BlitSurface pixel overlap (returns 8).
 *
 * NOTE: the binary's UIPANEL_BlitSurface call passes a by-value target RECT
 * plus the object surface as a __thiscall this-argument (0x43550B..0x435538);
 * the host shim is declared as a 6-arg free function (shared stub), so the
 * call below passes the same meaningful values in that form.
 */
int BuildingMgr::BlitOverlaps(int left, int top, int right, int bottom,
                              Building* target)
{
    RECT clip = {left, top, right, bottom};
    if (target == nullptr) return InvalidateRects(clip);

    uint8_t z_limit = building_parent_fields(target->parent)->z_limit;
    auto local_intersection = [&](Building* object, RECT& object_local,
                                  RECT& target_local) {
        if (object == nullptr || object == target || object->visible == 0) return false;
        int dz = object->world_y - target->world_y;
        if (dz < 0) dz = -dz;
        if (dz >= z_limit) return false;
        RECT intersection;
        if (!g_IntersectRect(&intersection, &clip, &object->screen_rect)) return false;
        object_local = intersection;
        g_OffsetRect(&object_local, -object->screen_rect.left,
                     -object->screen_rect.top);
        g_OffsetRect(&object_local, object->source_rect.left, 0);
        target_local = intersection;
        g_OffsetRect(&target_local, -target->screen_rect.left,
                     -target->screen_rect.top);
        g_OffsetRect(&target_local, target->source_rect.left, 0);
        return true;
    };

    for (uint32_t i = 0; i < buildings.GetCount(); ++i) {
        Building* object = buildings.GetItem(i);
        RECT object_local, target_local;
        if (!local_intersection(object, object_local, target_local)) continue;
        if (Town_CheckOccupied(entity_surface(object), object_local.left,
                               object_local.top, object_local.right,
                               object_local.bottom) &&
            Town_CheckOccupied(entity_surface(target), target_local.left,
                               target_local.top, target_local.right,
                               target_local.bottom)) return 7;
    }
    for (uint32_t i = 0; i < secondary_buildings.GetCount(); ++i) {
        Building* object = secondary_buildings.GetItem(i);
        RECT object_local, target_local;
        if (!local_intersection(object, object_local, target_local)) continue;
        if (UIPANEL_BlitSurface(entity_surface(target), target_local.left,
                                target_local.top, entity_surface(object),
                                object_local.left, object_local.top)) return 8;
    }
    return 0;
}

/**
 * Command hit-test loop. Address: 0x435580.
 * Each building whose parent resource id matches the command filter (or
 * filter == -1) and whose screen_rect top-left is inside the click rect is
 * dispatched: action 0 plays a sound via StopSound (vtable[7], 0x405A20);
 * other actions reload the animation via InitBase (vtable[6], 0x405900).
 * action_cooldown_time (+0x68) suppresses re-dispatch until delay ticks pass.
 */
void BuildingMgr::HandleClick(const BuildingClickCommand* command, int left,
                              int top, int right, int bottom)
{
    if (command == nullptr) return;
    RECT hit = {left, top, right, bottom};
    const int filter = *command->filter;                 // +0x08, dereferenced
    const int action = command->action;                  // +0x14
    const int argument = command->argument;              // +0x18, sign-extended
    const uint32_t delay = static_cast<uint32_t>(command->delay);  // +0x1C
    for (uint32_t i = 0; i < buildings.GetCount(); ++i) {
        Building* object = buildings.GetItem(i);
        if (object == nullptr) continue;
        int resource_id = building_parent_fields(object->parent)->resource_id;
        if ((filter != -1 && filter != resource_id) ||
            !g_PtInRect(&hit, object->screen_rect.left,
                        object->screen_rect.top) ||
            object->action_cooldown_time != 0 || action == -1) continue;

        if (action == 0) {
            object->StopSound(argument);                 // vtable [7] = 0x405A20
            object->action_cooldown_time = delay + g_game_time;
        } else {
            object->InitBase(action, argument, false);   // vtable [6] = 0x405900
            if (object->initialized == 1)
                object->action_cooldown_time = delay + g_game_time;
            else
                object->InitBase(static_cast<int>(object->stored_resource_id), -1, false);
        }
        if (!g_is_town_mode &&
            !(g_ddraw_active == 1 && g_game_difficulty == 3)) {
            PlaySound(0x571e);
            DDRAW_SelectBuilding(&g_ddraw_building, object);
        }
    }
}

/* ================================================================== */
/* Typed wrapper for TileMap::ProcessRect's BuildingMgr dispatch call.  */
/* Declared in world/tilemap.h; implemented here (not in tilemap.cpp)  */
/* to avoid pulling this file's own headers into that one. Real         */
/* signature verified against DispatchAll's own RET immediate (0x18 —  */
/* 6 stack dwords; the 6th is always 0 from ProcessRect and never read, */
/* per the header's own doc comment, so it's dropped here). */
/* ================================================================== */
extern void* g_building_mgr; /* 0x485448 */

void BuildingMgr_DispatchAll(int dispatch_flags, int left, int top,
                             int right, int bottom)
{
    static_cast<BuildingMgr*>(g_building_mgr)->DispatchAll(dispatch_flags, left,
                                                            top, right, bottom);
}
