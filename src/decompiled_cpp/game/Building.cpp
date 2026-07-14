/**
 * Building.cpp — Building game object class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The Building class contains the most thoroughly reverse-engineered
 * code in the project. Key methods below are translated from
 * src/decompiled/building_*.c into idiomatic C++.
 */

#include "Building.h"
#include "../shared/vtable_addrs.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {
    uint32_t CRT_rand(void);                                    /* 0x466150 */
    int      CRT_wcsstr(const char* a, const char* b);          /* 0x471480 */
    int      LoadStringA(HINSTANCE hInst, UINT id, char* buf, int maxLen);
}

/* BuildingMgr singleton helper */
extern void BuildingMgr_CompactCollections(void* bldg_mgr);     /* 0x434870 */
extern void BuildingMgr_RemoveObject(void* bldg_mgr, void* obj);

/* Globals */
extern uint32_t  g_game_time;           /* 0x4A99B4 */
extern HWND      g_main_window;         /* 0x4AA4A0 */
extern uint8_t   g_is_party_mode;       /* 0x48548C */
extern uint32_t  g_party_start_time;    /* 0x485490 */
extern void*     g_building_mgr;        /* 0x485448 — BuildingMgr singleton */
extern void*     g_selected_building;   /* currently selected building */
extern void      GLOBAL_free(void* ptr);

/* ROM string */
static const wchar_t PARTY_STRING[] = L"PARTY";  /* at 0x47E4FC in .rdata */


/* ================================================================== */
/* Building::Building — Full constructor                               */
/* Address: 0x4326F0  (size: 42 bytes)                                 */
/*                                                                     */
/* Called by:                                                          */
/*   BuildingMgr_CreateFromResource (0x434AF7) — factory              */
/* ================================================================== */
Building::Building(int resource_id)
{
    /* Step 1: Initialize base class via shared base constructor.
     * This sets vtable to 0x477F18 (intermediate), calls Entity(),
     * loads resource, initializes all Building-specific fields.      */
    this->BaseCtor(resource_id);                /* +0x433A20 */

    /* Step 2: Override vtable for the complete Building type.
     * 0x477EB8 is the "full" vtable whose destructor slot points to
     * Building::scalar_deleting_destructor (0x432720).              */
    this->vtable = (void**)VTBL_BUILDING_FULL;  /* +0x00 */

    /* Step 3: Set occupation level to 4 (redundant with BaseCtor
     * but ensures correctness).                                     */
    this->occupation_level = 4;                 /* +0x88 */

    /* Step 4: Zero the occupant cross-reference pointer.
     * Offset +0xF0 is a bidirectional link: in a Building it points
     * to the occupying entity; in an occupant entity it points to
     * the building.                                                */
    this->occupant_ptr = nullptr;               /* +0xF0 */
}


/* ================================================================== */
/* Building::scalar_deleting_destructor — Vtable slot [0]              */
/* Address: 0x432720  (size: 30 bytes)                                 */
/* ================================================================== */
void* Building::scalar_deleting_destructor(byte flags)
{
    /* Run the base destructor which:
     *   - Resets vtable to the complete Building vtable (0x477EB8)
     *   - Deselects this building if it's the currently selected one
     *   - Removes any occupant still assigned at +0xF0
     *   - Calls Building_BaseCleanup (removes from parent, entity cleanup) */
    this->BaseDtor();                           /* +0x432740 */

    /* MSVC scalar delete: if flags & 1, free heap memory */
    if (flags & 1) {
        GLOBAL_free(this);                      /* +0x465CD0 */
    }

    return this;
}


/* ================================================================== */
/* Building::BaseCtor — Shared base constructor                        */
/* Address: 0x433A20  (size: 407 bytes)                                */
/*                                                                     */
/* Called by:                                                          */
/*   Building() (0x4326F0)                                             */
/*   Train constructor (0x4533D8)                                     */
/*                                                                     */
/* Summary (see src/decompiled/building_basector.c for full details):  */
/*   1. Calls Entity() (0x405790) — base GameObject/Entity init       */
/*   2. Sets intermediate vtable = 0x477F18                            */
/*   3. Zero/-1 initializes all Building fields                        */
/*   4. If resource has a name, copies it; otherwise generates random  */
/*      name from string table (residential vs commercial pool)        */
/*   5. If sub-type == STATION (7), compacts BuildingMgr collections   */
/*   6. If resource name does NOT contain "PARTY", activates party mode */
/* ================================================================== */
Building* Building::BaseCtor(int resource_id)
{
    /* --- Step 1: Construct base classes ---
     * Entity() (0x405790, __thiscall) calls:
     *   GameObject::GameObject()            — sets vtable, type, zero-fields
     *   if (resource_id > 0):
     *     InitBase(resource_id, anim_idx, false) — loads RESDATA        */
    /* Note: Entity ctor parameters: resource_id, anim_idx=-1, world_x=0, world_y=0 */
    Entity::Entity(resource_id, -1, 0, 0);  /* +0x405790 */

    /* --- Step 2: Override vtable to intermediate Building vtable --- */
    this->vtable = (void**)VTBL_BUILDING_BASE;  /* +0x00, vtable = 0x477F18 */

    /* --- Step 3: Initialize Building-specific byte flags --- */
    this->disabled          = 0;            /* +0x89 — building is active */
    this->occupation_level  = 4;            /* +0x88 — starts at level 4  */
    this->field_e4          = 0;            /* +0xE4 */

    /* --- Step 4: Record creation timestamp --- */
    this->create_time = g_game_time;        /* +0x94 */

    /* --- Step 5: Get resource pointer --- */
    RESDATA* resource = (RESDATA*)this->parent;  /* +0x40 (resource stored in parent slot before overridden) */
    /* NOTE: In the original binary, the resource pointer is stored at +0x40.
     * Our Entity class uses 'parent' at +0x40 for scene graph, but during
     * construction, +0x40 holds the RESDATA* before it's replaced. */

    /* --- Step 6: Store the resource ID --- */
    /* resource_id is stored at +0x64 (offset within Entity extension area) */
    *(int*)((uint8_t*)this + 0x64) = resource_id;

    /* --- Step 7: Zero / -1 initialize all remaining fields --- */
    *(uint32_t*)((uint8_t*)this + 0x68) = 0;
    this->occupant_a         = nullptr;     /* +0x8C */
    this->occupant_b         = nullptr;     /* +0x90 */
    this->field_dc           = 0;           /* +0xDC */
    this->conn_building_a    = -1;          /* +0x98 */
    this->conn_building_b    = -1;          /* +0x9C */
    this->waypoint_x1        = -1;          /* +0xD4 */
    this->waypoint_y1        = -1;          /* +0xD8 */
    this->dest_x             = -1;          /* +0xCC */
    this->dest_y             = -1;          /* +0xD0 */
    this->target_x           = -1;          /* +0xA8 */
    this->target_y           = -1;          /* +0xAC */
    this->prev_target_x      = -1;          /* +0xC4 */
    this->prev_target_y      = -1;          /* +0xC8 */
    this->search_x1          = -1;          /* +0xB0 */
    this->search_y1          = -1;          /* +0xB4 */
    this->field_e0           = 0;           /* +0xE0 */
    this->next_action_time   = 0;           /* +0xA0 */
    this->field_a4           = 0;           /* +0xA4 */
    this->last_action        = 0;           /* +0xE8 */

    /* --- Step 8: Set the building's name --- */
    if (resource != nullptr) {
        char* res_name = (char*)resource + RESNAME_OFFSET;  /* +0x14D */

        if (*res_name == '\0') {
            /* Branch A: No resource-provided name.
             * Generate a random name from the game's String Table.
             * Building class at resource+RESCLASS_OFFSET determines pool:
             *   'M' (0x4D) = Residential → 50 names (IDs 2..51)
             *   otherwise   = Commercial  → 11 names (IDs 51..61)        */
            UINT name_id;
            uint32_t rand_val = CRT_rand();                     /* +0x466150 */

            if (*(uint32_t*)((uint8_t*)resource + RESCLASS_OFFSET) == 0x4D) {
                /* Residential: "M" = Minifigure house */
                name_id = (rand_val % RESIDENTIAL_NAME_COUNT) + RESIDENTIAL_NAME_BASE;
            } else {
                /* Commercial / Industrial */
                name_id = (rand_val % COMMERCIAL_NAME_COUNT) + COMMERCIAL_NAME_BASE;
            }

            /* Load name from executable's string table.
             * hInstance is at g_main_window + 0x0C */
            HINSTANCE hInst = *(HINSTANCE*)((uint8_t*)g_main_window + 0x0C);
            LoadStringA(hInst, name_id, this->name, 10);
        } else {
            /* Branch B: Resource has a custom name.
             * Copy it with validation via CopyName.                      */
            this->CopyName(res_name);                           /* +0x405E20 */

            /* Check building sub-type at resource+0x08.
             * If type == STATION (7), compact BuildingMgr collections.   */
            uint8_t sub_type = *(uint8_t*)((uint8_t*)resource + 0x08);
            if (sub_type == SUBTYPE_STATION) {
                BuildingMgr_CompactCollections(g_building_mgr); /* +0x434870 */
            }

            /* PARTY mode trigger:
             * If the building's resource name does NOT contain "PARTY",
             * activate party mode.                                       */
            if (CRT_wcsstr(res_name, (const char*)PARTY_STRING) == 0) {
                g_is_party_mode    = 1;
                g_party_start_time = g_game_time;
            }
        }
    }

    return this;
}


/* ================================================================== */
/* Building::BaseDtor — Base destructor body                           */
/* Address: 0x432740                                                   */
/*                                                                     */
/* Cleaning logic:                                                     */
/*   1. Reset vtable to 0x477EB8 (full Building vtable)               */
/*   2. If this == g_selected_building, clear selection to NULL        */
/*   3. If occupant_ptr != NULL, remove occupant from parent/self      */
/*   4. Call Building::BaseCleanup() for entity-level cleanup          */
/* ================================================================== */
void Building::BaseDtor()
{
    /* Reset vtable to full Building vtable */
    this->vtable = (void**)VTBL_BUILDING_FULL;

    /* Deselect if this is the currently selected building */
    if ((void*)this == g_selected_building) {
        g_selected_building = nullptr;
    }

    /* Remove any occupant still attached */
    if (this->occupant_ptr != nullptr) {
        /* Remove occupant from building manager */
        BuildingMgr_RemoveObject(g_building_mgr, this->occupant_ptr);
        this->occupant_ptr = nullptr;
    }

    /* Entity-level cleanup */
    this->BaseCleanup();
}


/* ================================================================== */
/* Building::BaseDtorWrapper — Wrapper that resets vtable first        */
/* Address: 0x4327A0                                                   */
/* ================================================================== */
void Building::BaseDtorWrapper()
{
    /* Restore vtable, then delegate to BaseDtor */
    this->vtable = (void**)VTBL_BUILDING_FULL;
    this->BaseDtor();
}


/* ================================================================== */
/* Building::BaseCleanup — Entity-level cleanup                        */
/* Address: 0x432770                                                   */
/*                                                                     */
/* Removes this building from its parent entity's child list and       */
/* calls GameObject::~GameObject() for resource release.              */
/* ================================================================== */
void Building::BaseCleanup()
{
    /* If parent exists, remove this from parent's child list.
     * Entity::parent at +0x40 doubles as scene graph link. */
    if (this->parent != nullptr) {
        /* Parent's child pointer at +0x130 (in LOCOBITMAP extension area)
         * is cleared — this removes the building from the scene graph. */
        *(void**)((uint8_t*)this->parent + 0x130) = nullptr;
    }

    /* Call GameObject destructor body to release resources.
     * This handles audio channel, resource, sound resource cleanup. */
    this->~GameObject();
}


/* ================================================================== */
/* Building::Update — Per-frame update                                 */
/* Address: 0x4327B0                                                   */
/*                                                                     */
/* Full implementation: see src/decompiled/building_update.c           */
/* Key behavior:                                                       */
/*   - In party mode: dispatches to vtable[0x5C] (special handler)    */
/*   - Normal mode: runs occupation timer, checks for occupant events, */
/*     updates animation based on occupancy level                      */
/* ================================================================== */
void Building::Update()
{
    /* See src/decompiled/building_update.c (0x4327B0) for full decompilation */
}


/* ================================================================== */
/* Building::AddOccupant — Add an occupant to this building            */
/* Address: 0x432BB0                                                   */
/*                                                                     */
/* See src/decompiled/building_addoccupant.c for full decompilation    */
/* ================================================================== */
void Building::AddOccupant(void* entity)
{
    /* See src/decompiled/building_addoccupant.c (0x432BB0) */
}


/* ================================================================== */
/* Building::RemoveOccupant — Remove an occupant                       */
/* Address: 0x432D50                                                   */
/*                                                                     */
/* See src/decompiled/building_removeoccupant.c for full decompilation */
/* ================================================================== */
void Building::RemoveOccupant(void* entity)
{
    /* See src/decompiled/building_removeoccupant.c (0x432D50) */
}


/* ================================================================== */
/* Building::CalcMoveTarget — Calculate movement target                */
/* Address: 0x432DA0                                                   */
/* ================================================================== */
void Building::CalcMoveTarget()
{
    /* See src/decompiled/building_calcmovetarget.c (0x432DA0) */
}


/* ================================================================== */
/* Building::CheckTimeout — Check action timeout                       */
/* Address: 0x432F90                                                   */
/* ================================================================== */
int Building::CheckTimeout()
{
    /* See src/decompiled/building_checktimeout.c (0x432F90) */
    return 0;
}


/* ================================================================== */
/* Building::DecideAction — Select next AI action                      */
/* Address: 0x432FE0                                                   */
/* ================================================================== */
void Building::DecideAction()
{
    /* See src/decompiled/building_decideaction.c (0x432FE0) */
}


/* ================================================================== */
/* Building::FindNearbyObject — Find object matching criteria          */
/* Address: 0x4333B0                                                   */
/* ================================================================== */
void* Building::FindNearbyObject(int search_type)
{
    /* See src/decompiled/building_findnearbyobject.c (0x4333B0) */
    return nullptr;
}


/* ================================================================== */
/* Building::FindPathToTarget — Pathfinding to target                  */
/* Address: 0x433510                                                   */
/* ================================================================== */
int Building::FindPathToTarget()
{
    /* See src/decompiled/building_findpathtotarget.c (0x433510) */
    return 0;
}


/* ================================================================== */
/* Building::MoveToTarget — Move occupant toward target                */
/* Address: 0x433950                                                   */
/* ================================================================== */
void Building::MoveToTarget()
{
    /* See src/decompiled/building_movetotarget.c (0x433950) */
}


/* ================================================================== */
/* Building::HandleAction — Execute current action                     */
/* Address: 0x434100                                                   */
/* ================================================================== */
void Building::HandleAction()
{
    /* See src/decompiled/building_handleaction.c (0x434100) */
}


/* ================================================================== */
/* Building::Deserialize — Deserialize from save data                  */
/* Address: 0x435630  (large function: ~18K bytes source)             */
/* ================================================================== */
void Building::Deserialize(void* data)
{
    /* See src/decompiled/building_deserialize.c (0x435630) */
}


/* ================================================================== */
/* Building::UpdateAnimByDimensions — Animation based on dimensions    */
/* Address: 0x433CD0                                                   */
/* ================================================================== */
void Building::UpdateAnimByDimensions()
{
    /* See src/decompiled/building_updateanimbydimensions.c (0x433CD0) */
}


/* ================================================================== */
/* Building::UpdateAnimByOccupancy — Animation based on occupancy      */
/* Address: 0x433F60                                                   */
/* ================================================================== */
void Building::UpdateAnimByOccupancy()
{
    /* See src/decompiled/building_updateanimbyoccupancy.c (0x433F60) */
}
