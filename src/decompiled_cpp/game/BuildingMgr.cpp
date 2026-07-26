/** BuildingMgr non-vtable operations, validated against loco_v8. */
#include "BuildingMgr.h"
#include "Building.h"
#include "Train.h"
#include <new>

extern void* operator_new(size_t);                              // 0x465ce0
extern void* ResourceManager_GetById(void*, int);               // 0x446ea0
extern uint8_t GetResourceType(int);                            // 0x446030
extern void RESDATA_Lock(void*);                                // 0x449410
extern void RESDATA_Unlock(void*);                              // 0x449420
extern void UI_CreateMessageBox(void*, int, int, char, int, int, int); // 0x423ab0
extern int Town_CheckOccupied(void*, int, int, int, int);       // 0x42c950
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
void add_to_collection(BuildingCollection& collection, Building* object)
{
    if (collection.count == collection.capacity) {
        collection.Resize(collection.capacity == 0 ? 100 : collection.capacity * 2);
    }
    collection.items[collection.count++] = object;
    collection.Sort();
}

int find_in_collection(const BuildingCollection& collection, const Building* object)
{
    for (uint32_t i = 0; i < collection.count; ++i) {
        if (collection.items[i] == object) return static_cast<int>(i);
    }
    return -1;
}

void* entity_surface(const Building* object)
{
    return object->parent == nullptr
        ? nullptr : *(void**)((uint8_t*)object->parent + 0x10);
}

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
    if (building_count <= 1) return;
    RESDATA_Lock(&building_lock);
    buildings.Sort();
    RESDATA_Unlock(&building_lock);
}

/**
 * Factory body. Address: 0x4349d0.
 * 0x434af7 is the call to Building::Building, not the function entry.
 */
Building* BuildingMgr::CreateFromResource(int resource_id, int owner_slot,
                                          int world_x, int world_y)
{
    void* resource = ResourceManager_GetById(g_resmgr, resource_id);
    /* The binary assumes this primary lookup succeeds. */
    if (resource == nullptr) return nullptr;

    int dependency_id = *(int*)((uint8_t*)resource + 0x40);
    void* dependency = ResourceManager_GetById(g_resmgr, dependency_id);
    if (dependency_id != -1 &&
        (dependency == nullptr || *(uint16_t*)((uint8_t*)dependency + 0x158) == 0))
        return nullptr;

    int exclusion_id = *(int*)((uint8_t*)resource + 0x44);
    void* exclusion = ResourceManager_GetById(g_resmgr, exclusion_id);
    if (exclusion != nullptr &&
        *(uint16_t*)((uint8_t*)exclusion + 0x158) != 0) return nullptr;

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

    object->occupant_a = (void*)(intptr_t)owner_slot;               // +0x8c
    object->MoveTo(world_x, world_y);                               // vtable [3]
    if (type == 7) {
        add_to_collection(buildings, object);                       // +0x4c
        ++building_count;
    } else {
        add_to_collection(secondary_buildings, object);             // +0x64
        ++secondary_count;
    }
    return object;
}

/** Remove unused removable buildings. Address: 0x434970. */
void BuildingMgr::RemoveEmpty()
{
    if (g_game_mode != 3) return;
    for (uint32_t i = 0; i < buildings.GetCount(); ++i) {
        Building* object = buildings.GetItem(i);
        if (object != nullptr && object->occupant_b == nullptr &&
            *(uint8_t*)((uint8_t*)object->parent + 0x16c) != 0) {
            RemoveObject(object, false);
        }
    }
}

/** Remove one managed Building/Train. Address: 0x434b60. */
void BuildingMgr::RemoveObject(Building* object, bool show_message)
{
    if (object == nullptr || object->parent == nullptr) return;
    uint8_t type = *(uint8_t*)((uint8_t*)object->parent + 8);
    BuildingCollection* collection;
    BuildingCollectionLock* lock;
    int32_t* managed_count;
    if (type == 7) {
        collection = &buildings; lock = &building_lock; managed_count = &building_count;
    } else if (type == 8) {
        collection = &secondary_buildings; lock = &secondary_lock; managed_count = &secondary_count;
    } else return;

    RESDATA_Lock(lock);
    int index = find_in_collection(*collection, object);
    if (index >= 0) {
        collection->items[index] = nullptr;
        --*managed_count;
        if (g_game_mode == 3 && show_message)
            UI_CreateMessageBox(&g_tooltip_mgr, 0x3860, 0, 'W',
                                object->world_x, object->world_y, 1);
        delete object;
    }
    RESDATA_Unlock(lock);
}

/** Select the first object hit at a world position. Address: 0x434c50. */
bool BuildingMgr::FindAndNotify(int world_x, int world_y)
{
    if (g_game_mode != 3) return false;
    auto find = [&](BuildingCollection& collection, bool require_visible,
                    bool respect_disable) {
        for (uint32_t i = 0; i < collection.GetCount(); ++i) {
            Building* object = collection.GetItem(i);
            if (object == nullptr || (require_visible && object->visible != 1)) continue;
            if (!object->PtInRect(world_x, world_y)) continue;
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

/** Tile occupancy query. Address: 0x435020. */
int BuildingMgr::InvalidateRects(RECT rect)
{
    int result = collection_occupancy(buildings, rect, 7);
    return result != 0 ? result : collection_occupancy(secondary_buildings, rect, 8);
}

/** Overlap query. Address: 0x435200. */
int BuildingMgr::BlitOverlaps(int left, int top, int right, int bottom,
                              Building* target)
{
    RECT clip = {left, top, right, bottom};
    if (target == nullptr) return InvalidateRects(clip);

    uint8_t z_limit = *(uint8_t*)((uint8_t*)target->parent + 0x16a);
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

/** Command hit-test loop. Address: 0x435580. */
void BuildingMgr::HandleClick(void* command, int left, int top,
                              int right, int bottom)
{
    if (command == nullptr) return;
    RECT hit = {left, top, right, bottom};
    int filter = **(int**)((uint8_t*)command + 8);
    int action = *(int*)((uint8_t*)command + 0x14);
    int16_t argument = *(int16_t*)((uint8_t*)command + 0x18);
    uint32_t delay = *(uint32_t*)((uint8_t*)command + 0x1c);
    for (uint32_t i = 0; i < buildings.GetCount(); ++i) {
        Building* object = buildings.GetItem(i);
        if (object == nullptr) continue;
        int resource_id = *(int*)((uint8_t*)object->parent + 4);
        if ((filter != -1 && filter != resource_id) ||
            !g_PtInRect(&hit, object->screen_rect.left,
                        object->screen_rect.top) ||
            object->field_68 != 0 || action == -1) continue;

        if (action == 0) {
            object->StopSound(argument);                 // binary vtable +0x1c
            object->field_68 = delay + g_game_time;
        } else {
            object->InitBase(action, argument, false);   // binary vtable +0x18
            if (object->initialized == 1)
                object->field_68 = delay + g_game_time;
            else
                object->InitBase((int)object->field_64, -1, false);
        }
        if (!g_is_town_mode &&
            !(g_ddraw_active == 1 && g_game_difficulty == 3)) {
            PlaySound(0x571e);
            DDRAW_SelectBuilding(g_ddraw_building, object);
        }
    }
}


