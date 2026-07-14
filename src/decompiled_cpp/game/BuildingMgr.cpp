/**
 * BuildingMgr.cpp — Building Manager implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * See src/decompiled/buildingmgr_*.c for full C decompilations of each method.
 */

#include "BuildingMgr.h"
#include "Building.h"

extern void GLOBAL_free(void* ptr);
extern void* operator_new(size_t size);
extern void Building_RemoveFromParent(void* building);     /* BaseCleanup helper */


/* ================================================================== */
/* BuildingMgr::CreateFromResource — Factory method                    */
/* Address: 0x4348F0                                                   */
/*                                                                     */
/* Allocates a 0xF4-byte Building object, calls the constructor with   */
/* the given resource_id, adds it to the building list.                */
/* See src/decompiled/buildingmgr_createfromresource.c for full details*/
/* ================================================================== */
Building* BuildingMgr::CreateFromResource(int resource_id)
{
    /* Allocate 0xF4 (244) bytes for a new Building */
    Building* bldg = (Building*)operator_new(0xF4);

    if (bldg != nullptr) {
        /* Call full constructor */
        bldg->Building::Building(resource_id);
    }

    /* Add to managed collection (list append) */
    /* ... collection management ... */

    return bldg;
}


/* ================================================================== */
/* BuildingMgr::CompactCollections — Compact after removals            */
/* Address: 0x434870                                                   */
/*                                                                     */
/* Locks the compact-resource guard at +0x04, calls Compact() on       */
/* the building collection at +0x4C (vtable[0x50]), then unlocks.      */
/* Only runs when the threshold count (+0x3C) exceeds 1.               */
/* ================================================================== */
void BuildingMgr::CompactCollections()
{
    int* threshold = (int*)((uint8_t*)this + 0x3C);
    if (*threshold <= 1) return;

    /* Lock collection guard at +0x04 */
    extern void RESDATA_Lock(void*);
    RESDATA_Lock((uint8_t*)this + 0x04);

    /* Collection at +0x4C: call Compact() at vtable[0x50] */
    void* coll = *(void**)((uint8_t*)this + 0x4C);
    void** vt = *(void***)coll;
    ((void(__thiscall*)())vt[0x50 / 4])();

    /* Unlock */
    extern void RESDATA_Unlock(void*);
    RESDATA_Unlock((uint8_t*)this + 0x04);
}


/* ================================================================== */
/* BuildingMgr::DestroyAll — Destroy all buildings                     */
/* Address: 0x434800 (called as 0x434E50 wrapper)                      */
/*                                                                     */
/* Destroys every building/train in both collections by calling        */
/* each object's destructor (vtable[1] = cleanup without free).        */
/* The _unused parameter is accepted but ignored.                      */
/* ================================================================== */
void BuildingMgr::DestroyAll()
{
    auto destroyCollection = [](void* coll) {
        void** vt = *(void***)coll;
        auto getCount = (int(__thiscall*)())vt[0x2C / 4];
        auto getItem  = (void*(__thiscall*)(int))vt[0x20 / 4];

        int count = getCount();
        for (int i = 0; i < count; i++) {
            void* entity = getItem(i);
            /* vtable[1] = cleanup destructor (no free) */
            void** evt = *(void***)entity;
            ((void(__thiscall*)())evt[0x04 / 4])();
            count = getCount();  /* re-query — destruction may modify collection */
        }
    };

    /* Active buildings at +0x4C */
    destroyCollection(*(void**)((uint8_t*)this + 0x4C));

    /* Special/train collection at +0x64 */
    destroyCollection(*(void**)((uint8_t*)this + 0x64));
}


/* ================================================================== */
/* BuildingMgr::DispatchAll — Dispatch event to all entities           */
/* Address: 0x4348A0                                                   */
/*                                                                     */
/* Calls vtable[0x2C] on each building and train. Gated by phase:      */
/* only dispatches when the low word of flags is 0. Buildings are      */
/* z-sorted — iteration stops early when screen_rect.top > bottom.     */
/* ================================================================== */
void BuildingMgr::DispatchAll()
{
    /* Gate: only dispatch during phase 0 */
    /* (simplified — see src/decompiled/buildingmgr_dispatchall.c for
     *  full flag-based gating and the per-entity dispatch with
     *  left/top/right/bottom rect parameters) */
}


/* ================================================================== */
/* BuildingMgr::BlitOverlaps — Render overlap indicators               */
/* Address: 0x435200 (via wrapper at 0x434AB0)                         */
/*                                                                     */
/* Tests whether a target entity's sprite overlaps any other entity.   */
/* Tier 1 (buildings): tile occupancy check → result 7.               */
/* Tier 2 (trains): pixel-level sprite AND test → result 8.           */
/* Returns 0 if no overlap.                                            */
/* See src/decompiled/buildingmgr_blitoverlaps.c for full details.     */
/* ================================================================== */
void BuildingMgr::BlitOverlaps()
{
    /* See src/decompiled/buildingmgr_blitoverlaps.c (0x435200) */
}


/* ================================================================== */
/* BuildingMgr::HandleClick — Handle mouse click on building           */
/* Address: 0x435580  (size: 299 bytes)                                */
/*                                                                     */
/* Iterates the building collection, testing each building against     */
/* a click rectangle and type filter. On match, dispatches the click   */
/* command (animation change or action dispatch). Plays click sound    */
/* and highlights the building when not in town mode.                  */
/* ================================================================== */
void BuildingMgr::HandleClick(int rect_left, int rect_top,
                               int rect_right, int rect_bottom,
                               void* cmd_ptr)
{
    /* Build click RECT */
    RECT click_rect;
    click_rect.left   = rect_left;
    click_rect.top    = rect_top;
    click_rect.right  = rect_right;
    click_rect.bottom = rect_bottom;

    /* Pre-load type filter from click command (+0x08) */
    struct ClickCmd {
        int field_00, field_04;
        int* sprite_type_ptr;   /* +0x08 */
        int field_0C, field_10;
        int command;            /* +0x14: 0=anim, non-zero=action */
        int16_t anim_index;     /* +0x18 */
        int delay;              /* +0x1C: cooldown ticks */
    };
    ClickCmd* cmd = (ClickCmd*)cmd_ptr;
    int filter_type = *cmd->sprite_type_ptr;  /* BUG: no NULL guard */

    /* Collection at +0x4C */
    void* coll = *(void**)((uint8_t*)this + 0x4C);
    void** vt = *(void***)coll;
    auto getCount = (int(__thiscall*)())vt[0x2C / 4];
    auto getItem  = (void*(__thiscall*)(int))vt[0x20 / 4];

    int count = getCount();
    for (int i = 0; i < count; i++) {
        void* building = getItem(i);

        /* Check 1: Type filter */
        void* sprite_info = *(void**)((uint8_t*)building + 0x40);
        int bldg_type = *(int*)((uint8_t*)sprite_info + 4);
        if (bldg_type != filter_type && filter_type != -1) continue;

        /* Check 2: Point-in-rect */
        POINT pt;
        pt.x = *(int*)((uint8_t*)building + 0x08);
        pt.y = *(int*)((uint8_t*)building + 0x0C);
        extern BOOL PtInRect(const RECT*, POINT);
        if (!PtInRect(&click_rect, pt)) continue;

        /* Check 3: Cooldown timer at +0x68 must be 0 */
        if (*(int*)((uint8_t*)building + 0x68) != 0) continue;

        /* Check 4: Valid command */
        if (cmd->command == -1) continue;

        /* Execute command */
        void** bvt = *(void***)building;
        if (cmd->command == 0) {
            /* Mode 0: Animation change via vtable[0x1C] */
            ((void(__thiscall*)(int16_t))bvt[0x1C / 4])(cmd->anim_index);
            *(int*)((uint8_t*)building + 0x68) = cmd->delay + g_game_time;
        } else {
            /* Mode non-zero: Action dispatch via vtable[0x18] */
            ((void(__thiscall*)(int,int,int))bvt[0x18 / 4])(
                cmd->command, (int)cmd->anim_index, 0);

            /* Set cooldown or cancel previous action based on type */
            if (*(uint8_t*)((uint8_t*)building + 0x18) == 1) {
                *(int*)((uint8_t*)building + 0x68) = cmd->delay + g_game_time;
            } else {
                /* Vehicle: cancel previous action */
                int prev = *(int*)((uint8_t*)building + 0x64);
                ((void(__thiscall*)(int,int,int))bvt[0x18 / 4])(prev, -1, 0);
            }
        }

        /* Post-dispatch sound + visual feedback */
        extern uint8_t g_is_town_mode;
        extern uint8_t g_ddraw_active;
        extern uint16_t g_game_difficulty;
        if (!g_is_town_mode && (g_ddraw_active != 1 || g_game_difficulty != 3)) {
            extern void RESMGR_PlaySound(int id);
            RESMGR_PlaySound(0x571E);

            extern void DDRAW_SelectBuilding(void* ddraw, int building);
            extern void* g_ddraw_building;
            DDRAW_SelectBuilding(g_ddraw_building, (int)building);
        }
    }
}


/* ================================================================== */
/* BuildingMgr::FindAndNotify — Find building at world position        */
/* Address: 0x434C50  (size: 278 bytes)                                */
/*                                                                     */
/* Iterates both collections, calling vtable[0x08] (match-at-position) */
/* on each object. On first match, optionally selects in game UI and   */
/* town view. Only runs in game mode 3. Returns TRUE if found.         */
/* ================================================================== */
bool BuildingMgr::FindAndNotify(int world_x, int world_y)
{
    extern int g_game_mode;
    if (g_game_mode != 3) return false;

    auto searchCollection = [&](void* coll) -> bool {
        void** vt = *(void***)coll;
        auto getCount = (int(__thiscall*)())vt[0x2C / 4];
        auto getItem  = (void*(__thiscall*)(int))vt[0x20 / 4];

        int count = getCount();
        for (int i = 0; i < count; i++) {
            void* obj = getItem(i);
            if (obj == nullptr) continue;

            /* Only consider fully initialized objects (visible == 1) */
            if (*(uint8_t*)((uint8_t*)obj + 0x24) != 1) continue;

            /* vtable[0x08]: match-at-position check */
            void** ovt = *(void***)obj;
            if (((bool(__thiscall*)(int,int))ovt[0x08 / 4])(world_x, world_y)) {
                extern uint8_t g_click_on_building;
                extern uint8_t g_click_on_town;
                extern uint8_t g_disable_input;

                if (g_click_on_building) {
                    extern void Game_SelectGameObject(void* game, void* obj);
                    extern void* g_game;
                    Game_SelectGameObject(g_game, obj);
                }
                if (g_click_on_town) {
                    extern void Town_SelectBuilding(void* town, int building);
                    extern void* g_town_view;
                    Town_SelectBuilding(g_town_view, (int)obj);
                }
                return true;
            }
        }
        return false;
    };

    /* Search buildings at +0x4C */
    if (searchCollection(*(void**)((uint8_t*)this + 0x4C))) return true;

    /* Search trains at +0x64 */
    return searchCollection(*(void**)((uint8_t*)this + 0x64));
}


/* ================================================================== */
/* BuildingMgr::InvalidateRects — Check tile occupancy in clip rect    */
/* Address: 0x435020  (size: 469 bytes)                                */
/*                                                                     */
/* Iterates both collections. For each visible entity whose bounding   */
/* box intersects the clip rect, checks tile occupancy. Returns:       */
/*   7 = building occupies tiles, 8 = train occupies tiles, 0 = none.  */
/* Building check has priority — if it returns 7, trains are skipped.  */
/* ================================================================== */
int BuildingMgr::InvalidateRects(RECT clipRect)
{
    extern BOOL IntersectRect(RECT* out, const RECT* a, const RECT* b);
    extern BOOL OffsetRect(RECT* r, int dx, int dy);
    extern int Town_CheckOccupied(void* surface, int x1, int y1, int x2, int y2);

    auto checkCollection = [&](void** coll_base, int result_value) -> int {
        void* coll = *coll_base;
        void** vt = *(void***)coll;
        auto getCount = (int(__thiscall*)())vt[0x2C / 4];
        auto getItem  = (void*(__thiscall*)(int))vt[0x20 / 4];

        int count = getCount();
        for (int i = 0; i < count && *coll_base != nullptr; i++) {
            void* entity = getItem(i);

            if (*(uint8_t*)((uint8_t*)entity + 0x24) == 0) continue;  /* not visible */

            RECT* entityRect = (RECT*)((uint8_t*)entity + 0x08);
            RECT intersect;
            if (!IntersectRect(&intersect, &clipRect, entityRect)) continue;

            RECT localRect = intersect;
            OffsetRect(&localRect, -entityRect->left, -entityRect->top);
            OffsetRect(&localRect, *(int*)((uint8_t*)entity + 0x30), 0);

            void* objRef = *(void**)((uint8_t*)entity + 0x40);
            void* surface = *(void**)((uint8_t*)objRef + 0x10);

            if (Town_CheckOccupied(surface, localRect.left, localRect.top,
                                   localRect.right, localRect.bottom)) {
                return result_value;
            }
        }
        return 0;
    };

    /* Buildings at +0x4C — returns 7 if occupied */
    int result = checkCollection((void**)((uint8_t*)this + 0x4C), 7);
    if (result != 0) return result;

    /* Trains at +0x64 — returns 8 if occupied */
    return checkCollection((void**)((uint8_t*)this + 0x64), 8);
}


/* ================================================================== */
/* BuildingMgr::RemoveEmpty — Remove buildings with zero occupants     */
/* Address: 0x434970  (size: 90 bytes)                                 */
/*                                                                     */
/* Only runs in game mode 3. Iterates building collection, destroying  */
/* buildings whose occupant count (+0x90) is 0 and whose resource      */
/* data (+0x40->+0x16C) allows removal.                                */
/* ================================================================== */
void BuildingMgr::RemoveEmpty()
{
    extern int g_game_mode;
    if (g_game_mode != 3) return;

    void* coll = *(void**)((uint8_t*)this + 0x4C);
    void** vt = *(void***)coll;
    auto getCount = (int(__thiscall*)())vt[0x2C / 4];
    auto getItem  = (void*(__thiscall*)(int))vt[0x20 / 4];

    int count = getCount();
    for (int i = 0; i < count; i++) {
        void* building = getItem(i);

        /* Check occupant count at +0x90 */
        if (*(int*)((uint8_t*)building + 0x90) == 0) {
            /* Check resource-removable flag at resource+0x16C */
            void* resource = *(void**)((uint8_t*)building + 0x40);
            if (*(uint8_t*)((uint8_t*)resource + 0x16C) != 0) {
                /* Destroy via vtable[0x50] = Release */
                void** bvt = *(void***)building;
                ((void(__thiscall*)())bvt[0x50 / 4])();
            }
        }
    }
}


/* ================================================================== */
/* BuildingMgr::RemoveObject — Remove a specific building/train        */
/* Address: 0x434B60  (size: 229 bytes)                                */
/*                                                                     */
/* Removes a single building (type 7) or train (type 8) from the       */
/* manager. Locks the appropriate collection's critical section,       */
/* removes the object, optionally shows a UI message.                  */
/* ================================================================== */
void BuildingMgr::RemoveObject(void* obj, bool show_message)
{
    if (obj == nullptr) return;

    /* Determine type from resource byte at +0x08 */
    void* parent = *(void**)((uint8_t*)obj + 0x40);
    int type_code = (parent != nullptr) ? *(uint8_t*)((uint8_t*)parent + 8) : 0;

    void* coll;
    int* count_ptr;
    int lock_offset;

    if (type_code == 7) {
        coll       = *(void**)((uint8_t*)this + 0x4C);
        count_ptr  = (int*)((uint8_t*)this + 0x3C);
        lock_offset = 0x04;
    } else if (type_code == 8) {
        coll       = *(void**)((uint8_t*)this + 0x64);
        count_ptr  = (int*)((uint8_t*)this + 0x40);
        lock_offset = 0x20;
    } else {
        return;  /* unknown type */
    }

    /* Lock critical section */
    extern void RESDATA_Lock(void*);
    RESDATA_Lock((uint8_t*)this + lock_offset);

    /* Find index and verify */
    void** cvt = *(void***)coll;
    int idx = ((int(__thiscall*)(void*))cvt[0x38 / 4])(obj);
    void* verify = ((void*(__thiscall*)(int))cvt[0x0C / 4])(idx);

    if (verify == obj) {
        (*count_ptr)--;

        /* Optionally show removal message in game mode */
        if (g_game_mode == 3 && show_message) {
            int wx = *(int*)((uint8_t*)obj + 0x4C);
            int wy = *(int*)((uint8_t*)obj + 0x50);
            extern void UI_CreateMessageBox(void* mgr, int strId, int param,
                                             char icon, int x, int y, int show);
            extern void* g_tooltip_mgr;
            UI_CreateMessageBox(g_tooltip_mgr, 0x3860, 0, 'W', wx, wy, 1);
        }

        /* Call scalar deleting destructor (vtable[0]) with flags=1 */
        void** ovt = *(void***)obj;
        ((void(__thiscall*)(int))ovt[0])(1);
    }

    /* Unlock critical section */
    extern void RESDATA_Unlock(void*);
    RESDATA_Unlock((uint8_t*)this + lock_offset);
}


/* ================================================================== */
/* BuildingMgr::UpdateAll — Chain-update all buildings                 */
/* Address: 0x434720  (size: 212 bytes)                                */
/*                                                                     */
/* Chain-updates both collections: building[i-1]->Update(building[i])  */
/* for i = 1..N-1. The last building in each collection is never used  */
/* as the "this" of an Update call — a known bug in normal gameplay.   */
/* After updates, compacts special collection if threshold > 1.        */
/* ================================================================== */
void BuildingMgr::UpdateAll()
{
    /* Helper: chain-update a collection */
    auto chainUpdate = [](void* coll_base) {
        void* coll = *(void**)coll_base;
        void** vt = *(void***)coll;
        auto getItem  = (void*(__thiscall*)(int))vt[0x20 / 4];
        auto getCount = (int(__thiscall*)())vt[0x2C / 4];

        int count = getCount();
        if (count <= 0) return;

        void* prev = getItem(0);
        for (int i = 1; i < count; i++) {
            void* curr = getItem(i);
            /* vtable[0x3C] = Update(entity* next) */
            void** pvt = *(void***)prev;
            ((void(__thiscall*)(void*))pvt[0x3C / 4])(curr);
            prev = curr;
            count = getCount();  /* re-read — may have changed */
        }
    };

    /* Active buildings at +0x4C */
    chainUpdate((uint8_t*)this + 0x4C);

    /* Special buildings at +0x64 */
    chainUpdate((uint8_t*)this + 0x64);

    /* Compact special collection if threshold exceeded */
    if (*(int*)((uint8_t*)this + 0x40) > 1) {
        extern void RESDATA_Lock(void*);
        extern void RESDATA_Unlock(void*);
        RESDATA_Lock((uint8_t*)this + 0x20);

        void* coll = *(void**)((uint8_t*)this + 0x64);
        void** cvt = *(void***)coll;
        ((void(__thiscall*)())cvt[0x50 / 4])();  /* Compact() */

        RESDATA_Unlock((uint8_t*)this + 0x20);
    }
}
