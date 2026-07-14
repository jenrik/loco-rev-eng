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
/* Building::Update — Per-frame AI dispatch                            */
/* Address: 0x4327B0  (size: 395 bytes)                                */
/*                                                                     */
/* Called by:                                                          */
/*   BuildingMgr_UpdateAll (0x434777) — via vtable[0x3C]              */
/*                                                                     */
/* Called functions:                                                   */
/*   Building::CheckTimeout (0x433C50)                                 */
/*   Building::DecideAction (0x434040)                                 */
/*   Building::MoveToTarget (0x434260)                                 */
/*   Building::HandleAction (0x434100)                                 */
/*   Building::UpdateAnimByOccupancy (0x433160)                        */
/*   INPUT_FindObjectAt (0x41E1F0)                                     */
/*                                                                     */
/* Per-frame AI dispatch for all buildings. Each tick:                 */
/*   1. Skip if disabled (+0x89) or anim_index > 7                    */
/*   2. Check occupant timeout                                         */
/*   3. PARTY mode: delegate to vtable[0x5C]                          */
/*   4. Normal mode: decide action if idle, poll completion if active  */
/*   5. Post-update: refresh animation if idle and visible             */
/* ================================================================== */
void Building::Update()
{
    /* Step 1: Early exit if disabled */
    if (this->disabled != 0) {                              /* +0x89 */
        return;
    }

    /* Always run timeout checker */
    this->CheckTimeout();                                   /* +0x433C50 */

    /* Skip AI if anim_index > 7 (terminal state) */
    if (this->anim_index > 7) {                             /* +0x28 */
        return;
    }

    /* Step 2: Party mode fast-path.
     * In party mode, bypass normal AI and delegate to vtable[0x5C].
     * (next_entity param is passed from BuildingMgr_UpdateAll iteration) */
    extern uint8_t g_is_party_mode;                         /* 0x48548C */
    if (g_is_party_mode != 0) {
        /* vtable[0x5C] = party-mode update handler */
        void** vt = (void**)this->vtable;
        ((void(__thiscall*)(void*))vt[0x5C / 4])(this);
        goto post_update;
    }

    /* Step 3: Separate idle vs. action-in-progress */
    if (this->field_dc == 0) {                              /* +0xDC: action_state */
        /* ---- Branch A: Building is idle — decide next action ---- */
        if (this->next_action_time < g_game_time) {         /* +0xA0 */
            int action = this->DecideAction();              /* +0x434040 */
            this->last_action = action;                     /* +0xE8 */

            if (action == 1) {
                /* Action 1: Occupy — move toward occupant_a */
                this->MoveToTarget();                        /* +0x434260, uses occupant_a */
            } else if (action == 2) {
                /* Action 2: Spawn — move toward occupant_b */
                this->MoveToTarget();                        /* +0x434260, uses occupant_b */
            } else if (action == 3) {
                /* Action 3: Idle/wander.
                 * Look for an object under the input hotspot.
                 * If found at a new position, teleport there. */
                extern void* g_input_mgr;                   /* 0x4A9990 */
                extern void* INPUT_FindObjectAt(void* mgr, int mode);
                void* found = INPUT_FindObjectAt(g_input_mgr, 2);

                if (found != nullptr) {
                    int fx = *(int*)((uint8_t*)found + 0x4C);
                    int fy = *(int*)((uint8_t*)found + 0x50);

                    if (fx != this->prev_target_x ||         /* +0xC4 */
                        fy != this->prev_target_y)           /* +0xC8 */
                    {
                        /* vtable[0x40] = SetPosition — teleport */
                        void** vt = (void**)this->vtable;
                        ((void(__thiscall*)(int,int))vt[0x40 / 4])(fx, fy);
                    }
                }

                /* If no destination, schedule random idle timer: 10..30 ticks */
                if (this->dest_x == -1 && this->dest_y == -1) {  /* +0xCC, +0xD0 */
                    extern uint32_t CRT_rand(void);         /* 0x466150 */
                    uint32_t r = CRT_rand();
                    this->next_action_time = (r % 21 + 10)  /* +0xA0 */
                                            + g_game_time;
                }
            }
        }
    } else {
        /* ---- Branch B: Action in progress — poll completion ---- */
        /* vtable[0x58] = IsActionComplete() */
        void** vt = (void**)this->vtable;
        if (((int(__thiscall*)())vt[0x58 / 4])() == 0) {
            /* Action done. Check if we've arrived at target. */
            int cur_x = *(int*)((uint8_t*)this + 0x4C);    /* world_x */
            int cur_y = *(int*)((uint8_t*)this + 0x50);    /* world_y */

            if (this->target_x == cur_x && this->target_y == cur_y) {
                /* At target — finalize the action */
                this->HandleAction();                        /* +0x434100 */
            } else {
                /* Not at target — advance one step via vtable[0x48] */
                ((void(__thiscall*)(int,int))vt[0x48 / 4])(cur_x, cur_y);
            }
        }
    }

post_update:
    /* Only refresh animation when idle and visible */
    if (this->field_dc == 0 && this->visible == 1) {       /* +0xDC, +0x24 */
        this->UpdateAnimByOccupancy();                      /* +0x433160 */
    }
}


/* ================================================================== */
/* Building::AddOccupant — Register entity as building occupant        */
/* Address: 0x433530  (size: 358 bytes)                                */
/*                                                                     */
/* Called by:                                                          */
/*   Building::FindPathToTarget (0x4333F6, 0x43349C)                  */
/*   Occupant state machine (0x432A91)                                 */
/*                                                                     */
/* Tries to claim a slot in the building's occupant array[9] at       */
/* building+0x38. On success: hides occupant, increments occupancy     */
/* level, sets pathfinding target to entry coords, validates road.     */
/* ================================================================== */
void Building::AddOccupant(void* entity)
{
    if (entity == nullptr) {
        /* NULL building: exit current building if inside one */
        void* bldg = *(void**)((uint8_t*)entity + 0xF0);    /* occupied_building */
        if (bldg != nullptr) {
            Building::RemoveOccupant(entity);                /* recursively calls RemoveOccupant on entity */
        }
        return;
    }

    /* Find a free slot in building's 9-slot occupant array at +0x38 */
    void** occupant_array = (void**)((uint8_t*)entity + 0x38);

    int slot;
    for (slot = 0; slot < 9; slot++) {
        if (occupant_array[slot] == nullptr || occupant_array[slot] == this) {
            occupant_array[slot] = this;
            *(void**)((uint8_t*)this + 0xF0) = entity;      /* +0xF0: occupied_building = building */
            break;
        }
    }

    /* Check if we successfully claimed a slot */
    if (*(void**)((uint8_t*)this + 0xF0) == nullptr) {
        /* Building full */
        uint8_t* occ_level = (uint8_t*)this + 0x88;
        if (*occ_level != 0) {
            (*occ_level)--;
        }
        return;
    }

    /* Increment occupancy level (cap at 7) */
    uint8_t* occ_level = (uint8_t*)this + 0x88;
    if (*occ_level < 7) {
        (*occ_level)++;
    }

    /* Hide occupant inside building */
    this->visible = 0;                                      /* +0x24 */

    /* Read building model data for entry coordinates */
    void* model_data = *(void**)((uint8_t*)entity + 0x20);
    if (model_data == nullptr) return;

    void* entry_data = *(void**)((uint8_t*)model_data + 0x14);
    if (entry_data == nullptr) return;

    int entry_x = *(int*)((uint8_t*)entry_data + 0x4C);
    int entry_y = *(int*)((uint8_t*)entry_data + 0x50);
    int* target_x = (int*)((uint8_t*)this + 0xCC);
    int* target_y = (int*)((uint8_t*)this + 0xD0);
    *target_x = entry_x;
    *target_y = entry_y;

    /* Road validation loop: check entry tile is on a road (type 3) */
    extern void* TileMap_GetObjectAt(void* tilemap, int tx, int ty, int flags);
    extern void* g_tilemap;

    auto worldToTile = [](int coord) -> int {
        return (coord < 0) ? -1 : (coord >> 4);
    };

    int prev_tx = -1, prev_ty = -1;
    int tile_type = 3;

    while (tile_type == 3) {
        if (*target_x == prev_tx && *target_y == prev_ty) break;
        prev_tx = *target_x;
        prev_ty = *target_y;

        /* Call vtable[0x48] = SetTarget */
        void** vt = (void**)this->vtable;
        ((void(__thiscall*)(int,int))vt[0x48 / 4])(*target_x, *target_y);

        int tile_x = worldToTile(*target_x);
        int tile_y = worldToTile(*target_y);

        void* tile = TileMap_GetObjectAt(&g_tilemap, tile_x, tile_y, 0);
        if (tile != nullptr) {
            void* parent = *(void**)((uint8_t*)tile + 0x40);
            tile_type = (parent != nullptr) ? *(uint8_t*)((uint8_t*)parent + 0x08) : 0;
        } else {
            break;
        }
    }
}


/* ================================================================== */
/* Building::RemoveOccupant — Remove occupant from building            */
/* Address: 0x4336A0  (size: 435 bytes)                                */
/*                                                                     */
/* Removes occupant from building slot array and calculates exit       */
/* position. Exit depends on building tile road type:                  */
/*   - Horizontal road (0x12): random offset along road               */
/*   - Vertical road (0x13): random offset perpendicular to road      */
/*   - Other: simple offset subtraction                                */
/*                                                                     */
/* BUG: Searches slots 0-7 (8 slots) but AddOccupant searches 0-8     */
/* (9 slots). Slot 8 occupants are never found here.                   */
/* ================================================================== */
void Building::RemoveOccupant(void* entity)
{
    void* building = *(void**)((uint8_t*)this + 0xF0);      /* +0xF0 */
    if (building == nullptr) return;

    /* Find and clear this occupant in building's slot array[8] at +0x38 */
    for (int idx = 0; idx < 8; idx++) {
        if (*(void**)((uint8_t*)building + 0x38 + idx * 4) == this) {
            *(void**)((uint8_t*)building + 0x38 + idx * 4) = nullptr;
            break;
        }
    }

    /* Determine road class from building's tile resource data */
    void* type_info = *(void**)((uint8_t*)building + 0x20);
    void* resdata = (type_info != nullptr) ? *(void**)((uint8_t*)type_info + 0x14) : nullptr;
    uint8_t road_class = 0xFF;

    if (resdata != nullptr) {
        void* tile_res = *(void**)((uint8_t*)resdata + 0x40);
        road_class = *(uint8_t*)((uint8_t*)tile_res + 0x63A);
    }

    /* Read occupant's frame data for sprite offset */
    void* frame_data = *(void**)((uint8_t*)this + 0x40);
    int16_t offset_x = *(int16_t*)((uint8_t*)frame_data + 0x32);
    int16_t offset_y = *(int16_t*)((uint8_t*)frame_data + 0x34);

    int exit_x, exit_y;
    int move_tx = *(int*)((uint8_t*)this + 0xCC);          /* +0xCC: move_target_x */
    int move_ty = *(int*)((uint8_t*)this + 0xD0);          /* +0xD0: move_target_y */

    extern uint32_t CRT_rand(void);

    if (road_class == 0x12) {
        /* Horizontal road: random offset along road */
        uint32_t r1 = CRT_rand();
        int sign = (r1 & 1) ? -1 : 1;
        uint32_t r2 = CRT_rand();
        exit_x = move_tx + sign * ((int)(r2 % 11) + 15) - offset_x;
        exit_y = move_ty - offset_y;
    } else if (road_class == 0x13) {
        /* Vertical road: random offset perpendicular to road */
        exit_x = move_tx - offset_x;
        uint32_t r = CRT_rand();
        int sign = (r & 1) ? -1 : 1;
        exit_y = move_ty - offset_y - 5 + sign;
    } else {
        /* Default: simple offset subtraction */
        exit_x = move_tx - offset_x;
        exit_y = move_ty - offset_y;
    }

    /* Convert to tile coordinates */
    auto worldToTile = [](int coord) -> int {
        return (coord < 0) ? -1 : (coord >> 4);
    };
    int tile_x = worldToTile(exit_x);
    int tile_y = worldToTile(exit_y);

    /* Validate exit tile (must be type 0x0C = walkable surface) */
    extern void* TileMap_GetObjectAt(void* tilemap, int tx, int ty, int flags);
    extern void* g_tilemap;
    void* tile_obj = TileMap_GetObjectAt(&g_tilemap, tile_x, tile_y, 0);

    bool use_fallback = true;
    if (tile_obj != nullptr) {
        void* tile_frame = *(void**)((uint8_t*)tile_obj + 0x40);
        uint8_t tile_type = (tile_frame != nullptr) ? *(uint8_t*)((uint8_t*)tile_frame + 8) : 0;
        if (tile_type == 0x0C) {
            use_fallback = false;
        }
    }

    if (use_fallback) {
        /* Fallback: use follow-target position */
        void* follow = *(void**)((uint8_t*)this + 0x8C);    /* +0x8C */
        if (follow != nullptr) {
            exit_x = *(int*)((uint8_t*)follow + 0x4C);
            exit_y = *(int*)((uint8_t*)follow + 0x50);
        }
    }

    /* Teleport to exit position via vtable[3] (+0x0C) */
    void** vt = (void**)this->vtable;
    ((void(__thiscall*)(int,int))vt[0x0C / 4])(exit_x, exit_y);

    /* Dissociate from building */
    *(void**)((uint8_t*)this + 0xF0) = nullptr;

    /* Trigger display refresh via vtable[0x48] */
    ((void(__thiscall*)(int,int))vt[0x48 / 4])(move_tx, move_ty);

    /* Re-enable per-frame update */
    this->visible = 1;                                      /* +0x24 */
}


/* ================================================================== */
/* Building::CalcMoveTarget — Compute stepped movement toward target   */
/* Address: 0x433DC0  (size: 256 bytes)                                */
/*                                                                     */
/* Computes delta (dx, dy) and Euclidean distance from (world_x,       */
/* world_y) to (target_x, target_y), then produces a new pixel         */
/* position by moving at most max_step pixels toward the target        */
/* from (screen_rect.left, screen_rect.top).                           */
/*                                                                     */
/* Intermediate values stored in Building's movement fields:           */
/*   +0xD4 = delta_x   +0xD8 = delta_y                                */
/*   +0xDC = floor(sqrt(delta_x^2 + delta_y^2))                       */
/*                                                                     */
/* Special case: (-1, -1) zeros movement fields, outputs (world_x,     */
/* world_y) — cancelling movement.                                    */
/*                                                                     */
/* @param out_pos   int[2] — output buffer for resulting pixel (x,y)  */
/* @param target_x  Target X in world coordinates                     */
/* @param target_y  Target Y in world coordinates                     */
/* @param max_step  Maximum pixels to move this step (speed clamp)    */
/*                                                                     */
/* Called by:                                                          */
/*   Building::MoveToTarget  (0x434399)                                */
/*   Building::Update        (0x432AC8, 0x432FBF, 0x433033)           */
/*   Train functions         (0x453776, 0x453D5C, 0x453F14)           */
/* ================================================================== */
void Building::CalcMoveTarget(int* out_pos, int target_x, int target_y, int max_step)
{
    /* Cancel movement: (-1, -1) zeros fields, returns current position */
    if (target_x == -1 && target_y == -1) {
        this->waypoint_x1 = 0;                              /* +0xD4 */
        this->waypoint_y1 = 0;                              /* +0xD8 */
        this->field_dc    = 0;                              /* +0xDC */
        out_pos[0] = this->world_x;                         /* +0x4C */
        out_pos[1] = this->world_y;                         /* +0x50 */
        return;
    }

    /* Signed delta from current world position to target */
    int dx = target_x - this->world_x;                      /* +0x4C */
    int dy = target_y - this->world_y;                      /* +0x50 */

    /* Persist movement vector */
    this->waypoint_x1 = dx;                                 /* +0xD4 */
    this->waypoint_y1 = dy;                                 /* +0xD8 */

    /* Euclidean (crow-flight) distance */
    this->field_dc = (uint32_t)sqrt((double)(dx * dx + dy * dy));  /* +0xDC */

    /* X axis: clamp step to max_step, apply from screen_rect.left */
    int abs_dx = (dx >= 0) ? dx : -dx;
    int step_x = (max_step < abs_dx) ? max_step : abs_dx;
    if (dx < 0) {
        step_x = this->screen_rect.left - step_x;           /* +0x08: move left */
    } else {
        step_x = this->screen_rect.left + step_x;           /* +0x08: move right */
    }

    /* Y axis: clamp step to max_step, apply from screen_rect.top */
    int abs_dy = (dy >= 0) ? dy : -dy;
    int step_y = (max_step < abs_dy) ? max_step : abs_dy;
    if (dy < 0) {
        step_y = this->screen_rect.top - step_y;            /* +0x0C: move up */
    } else {
        step_y = this->screen_rect.top + step_y;            /* +0x0C: move down */
    }

    /* Output resulting screen-space pixel position */
    out_pos[0] = step_x;
    out_pos[1] = step_y;
}


/* ================================================================== */
/* Building::CheckTimeout — Check occupant timeout                    */
/* Address: 0x433C50                                                   */
/*                                                                     */
/* Called by Building::Update each tick. Runs base GameObject::Update */
/* then checks if the occupant timeout timer (+0xA4) has expired.     */
/* After 180 ticks, decrements occupation level, resets action timer, */
/* makes building visible, and calls vtable[0x44].                     */
/* ================================================================== */
void Building::CheckTimeout()
{
    /* Base-class update (animation, audio, frame stepping) */
    this->GameObject::Update();                             /* 0x405C40 */

    /* Check active occupant timeout */
    if (this->field_a4 != 0) {                              /* +0xA4: timeout_start */
        extern uint32_t g_game_time;                        /* 0x4A99B4 */

        /* Has 180 ticks elapsed since timeout started? */
        if (this->field_a4 + 180 < g_game_time) {
            /* Decrement occupant count if any */
            if (this->occupation_level != 0) {              /* +0x88 */
                this->occupation_level--;
            }

            /* Reset action timer and mark visible */
            this->next_action_time = 0;                     /* +0xA0 */
            this->visible = 1;                              /* +0x24 */

            /* vtable[0x44] = OnOccupantReady callback */
            void** vt = (void**)this->vtable;
            ((void(__thiscall*)(int))vt[0x44 / 4])(0);
        }
    }
}


/* ================================================================== */
/* Building::DecideAction — Select next AI action                      */
/* Address: 0x434040                                                   */
/*                                                                     */
/* Decides what this building should do next based on game time and    */
/* schedule. Returns: 1=occupy, 2=spawn, 3=wander, 0=wait.           */
/*                                                                     */
/* Uses schedule slots in RESDATA at +0x534 and +0x548.               */
/* Occupant goes to spawn_building when time is INSIDE its active      */
/* window, and to occupy_building when time is OUTSIDE its window.     */
/* ================================================================== */
int Building::DecideAction()
{
    extern uint32_t g_game_time;
    int game_time = (int)g_game_time;

    /* Not ready to decide yet */
    if (game_time < (int)this->next_action_time) {          /* +0xA0 */
        return 0;
    }

    /* If this building is selected and animating, defer */
    extern void* g_selected_building;
    extern uint8_t g_building_animating;
    if (g_selected_building == this && g_building_animating != 0) {
        return 0;
    }

    /* Mark visible if hidden */
    if (this->visible == 0) {                               /* +0x24 */
        this->visible = 1;
    }

    /* Convert game tick to local time */
    extern void* CRT_localtime(const time_t* timer);
    void* time_info = CRT_localtime((const time_t*)&game_time);

    extern int Game_CheckTimeInRange(void* tm, int* start, int* end);

    /* Check spawn_building schedule (+0x90 = occupant_b).
     * If time is within its active window, go there. */
    if (this->occupant_b != nullptr) {                     /* +0x90 */
        void* resource = *(void**)((uint8_t*)this->occupant_b + 0x40);
        if (Game_CheckTimeInRange(time_info,
                (int*)((uint8_t*)resource + 0x534),
                (int*)((uint8_t*)resource + 0x548))) {
            return 2;  /* spawn */
        }
    }

    /* Check occupy_building schedule (+0x8C = occupant_a).
     * If time is OUTSIDE its active window, go there. */
    if (this->occupant_a != nullptr) {                     /* +0x8C */
        void* resource = *(void**)((uint8_t*)this->occupant_a + 0x40);
        if (!Game_CheckTimeInRange(time_info,
                (int*)((uint8_t*)resource + 0x534),
                (int*)((uint8_t*)resource + 0x548))) {
            return 1;  /* occupy */
        }
    }

    /* Default: wander */
    return 3;
}


/* ================================================================== */
/* Building::FindNearbyObject — Search for entity of target type       */
/* Address: 0x433EC0                                                   */
/*                                                                     */
/* Searches three tile positions around the given coordinates for an   */
/* entity whose resource type byte matches target_type.               */
/* ================================================================== */
void* Building::FindNearbyObject(int target_type, int x, int y)
{
    extern void* TileMap_GetObjectAt(void* tilemap, int tx, int ty, int flags);
    extern void* g_tilemap;

    RESDATA* res_ptr = *(RESDATA**)((uint8_t*)this + 0x40);

    /* Check 1: tile at (x, y) only if this is road type 8 */
    uint8_t res_type = (res_ptr != nullptr) ? *(uint8_t*)((uint8_t*)res_ptr + 0x08) : 0;
    if (res_type == 8) {
        int tile_y = (y < 0) ? -1 : (y >> 4);
        int tile_x = ((x + 4) < 0) ? -1 : ((x + 4) >> 4);
        void* tile = TileMap_GetObjectAt(g_tilemap, tile_x, tile_y, 0);
        if (tile != nullptr) {
            RESDATA* tres = *(RESDATA**)((uint8_t*)tile + 0x40);
            if (tres != nullptr && *(uint8_t*)((uint8_t*)tres + 0x08) == target_type) {
                return tile;
            }
        }
    }

    /* Check 2: (x + 4, y + frame_height / 2) */
    if (res_ptr != nullptr) {
        uint16_t fh = *(uint16_t*)((uint8_t*)res_ptr + 0x16);
        int tile_y = ((fh / 2 + y) < 0) ? -1 : ((fh / 2 + y) >> 4);
        int tile_x = ((x + 4) < 0) ? -1 : ((x + 4) >> 4);
        void* tile = TileMap_GetObjectAt(g_tilemap, tile_x, tile_y, 0);
        if (tile != nullptr) {
            RESDATA* tres = *(RESDATA**)((uint8_t*)tile + 0x40);
            if (tres != nullptr && *(uint8_t*)((uint8_t*)tres + 0x08) == target_type) {
                return tile;
            }
        }
    }

    /* Check 3: (x + frame_width - 4, y + frame_height) */
    if (res_ptr != nullptr) {
        uint16_t fw = *(uint16_t*)((uint8_t*)res_ptr + 0x14);
        uint16_t fh = *(uint16_t*)((uint8_t*)res_ptr + 0x16);
        int tile_y = ((fh + y) < 0) ? -1 : ((fh + y) >> 4);
        int tile_x = ((fw - 4 + x) < 0) ? -1 : ((fw - 4 + x) >> 4);
        void* tile = TileMap_GetObjectAt(g_tilemap, tile_x, tile_y, 0);
        if (tile != nullptr) {
            RESDATA* tres = *(RESDATA**)((uint8_t*)tile + 0x40);
            if (tres != nullptr && *(uint8_t*)((uint8_t*)tres + 0x08) == target_type) {
                return tile;
            }
        }
    }

    return nullptr;
}


/* ================================================================== */
/* Building::FindPathToTarget — Calculate one step toward target tile  */
/* Address: 0x433370                                                   */
/*                                                                     */
/* Finds walkable tile (type 3) at target coords, computes a 4-pixel  */
/* step toward it. Handles horizontal/vertical road tiles with random   */
/* road-edge positioning. Stores step destination in dest_x/dest_y.    */
/* Returns 1 if step stored, 0 if no path.                             */
/* ================================================================== */
int Building::FindPathToTarget()
{
    /* Find tile type 3 (road/walkable) at target position */
    void* tile_obj = this->FindNearbyObject(3, this->target_x, this->target_y);

    if (tile_obj == nullptr) return 0;

    RESDATA* resdata = *(RESDATA**)((uint8_t*)tile_obj + 0x40);
    int resource_id = (resdata != nullptr) ? resdata->resource_id : -1;

    /* Rail tile check: IDs 0xC6C (straight), 0xC6E (crossing) — block passage */
    if (resource_id == 0xC6C || resource_id == 0xC6E) {
        return 0;
    }

    uint8_t road_class = *(uint8_t*)((uint8_t*)resdata + 0x63A);

    /* Horizontal road (class 0x12) */
    if (road_class == 0x12) {
        /* Check for vehicle boarding */
        void* vehicle = *(void**)((uint8_t*)tile_obj + 0x118);
        if (vehicle != nullptr) {
            int vs = *(int*)((uint8_t*)vehicle + 0x5C);
            if (vs == 0 || vs == 1) {  /* idle or boarding */
                extern int Vehicle_GetOccupantCount(void* v);
                if (Vehicle_GetOccupantCount(vehicle) != 0) {
                    this->AddOccupant(vehicle);
                    return 1;
                }
            }
        }

        int tile_x = *(int*)((uint8_t*)tile_obj + 0x4C);
        if (this->world_x == tile_x) {
            extern uint32_t CRT_rand(void);
            uint32_t r = CRT_rand();
            int sign = (r & 1) ? -1 : 1;
            r = CRT_rand();
            this->dest_x = tile_x + sign * ((int)(r % 11) + 15);
        } else {
            this->dest_x = tile_x;
        }

        int tile_y = *(int*)((uint8_t*)tile_obj + 0x50);
        this->dest_y = (tile_y > this->world_y) ? this->world_y - 4 : this->world_y + 4;
        return 1;
    }

    /* Vertical road (class 0x13) */
    if (road_class == 0x13) {
        void* vehicle = *(void**)((uint8_t*)tile_obj + 0x118);
        if (vehicle != nullptr) {
            int vs = *(int*)((uint8_t*)vehicle + 0x5C);
            if (vs == 0 || vs == 1) {
                extern int Vehicle_GetOccupantCount(void* v);
                if (Vehicle_GetOccupantCount(vehicle) != 0) {
                    this->AddOccupant(vehicle);
                    return 1;
                }
            }
        }

        int tile_y = *(int*)((uint8_t*)tile_obj + 0x50);
        if (this->world_y == tile_y) {
            extern uint32_t CRT_rand(void);
            uint32_t r = CRT_rand();
            int sign = (r & 1) ? -1 : 1;
            this->dest_y = tile_y + sign * 18 - 5;
        } else {
            this->dest_y = tile_y;
        }

        int tile_x = *(int*)((uint8_t*)tile_obj + 0x4C);
        this->dest_x = (tile_x > this->world_x) ? this->world_x - 4 : this->world_x + 4;
        return 1;
    }

    return 0;
}


/* ================================================================== */
/* Building::MoveToTarget — Move toward target entity                  */
/* Address: 0x434260                                                   */
/*                                                                     */
/* If target is valid and active, moves toward it with optional        */
/* random arrival point within bounding rect.                          */
/* ================================================================== */
void Building::MoveToTarget()
{
    /* Determine target entity from last_action */
    void* target = nullptr;
    if (this->last_action == 1) target = this->occupant_a;
    else if (this->last_action == 2) target = this->occupant_b;

    extern uint8_t g_is_game_active;
    if (target == nullptr || *(uint8_t*)((uint8_t*)target + 0x18) != 1 || !g_is_game_active) {
        this->dest_x = -1; this->dest_y = -1;
        if (this->visible) { int out[2]; this->CalcMoveTarget(out, -1, -1, 0); }
        return;
    }

    /* Check for random arrival rect */
    extern int GameObject_GetBoundingRect(void* obj, RECT* out);
    RECT bounds;
    void** vt = (void**)this->vtable;
    if (!GameObject_GetBoundingRect(target, &bounds) ||
        *(uint8_t*)(*(uint32_t*)((uint8_t*)target + 0x40) + 0x62C) == 0) {
        int tx = *(int*)((uint8_t*)target + 0x4C);
        int ty = *(int*)((uint8_t*)target + 0x50);
        ((void(__thiscall*)(int,int))vt[0x40 / 4])(tx, ty);
        return;
    }

    /* Random point within bounding rect */
    extern uint32_t CRT_rand(void);
    auto randInRange = [](int lo, int hi) -> int {
        if (hi >= lo) { int w = hi - lo + 1; return (w == 1) ? lo : (int)(CRT_rand() % w) + lo; }
        else { int w = lo - hi + 1; return (w == 1) ? hi : (int)(CRT_rand() % w) + hi; }
    };
    ((void(__thiscall*)(int,int))vt[0x40 / 4])(randInRange(bounds.left, bounds.right),
                                                 randInRange(bounds.top, bounds.bottom));
}

/* ================================================================== */
/* Building::HandleAction — Finalize current action at target          */
/* Address: 0x434100                                                   */
/* ================================================================== */
void Building::HandleAction()
{
    if (!this->visible) return;
    extern void* g_selected_building; extern uint8_t g_building_animating;
    if (g_selected_building == this && g_building_animating) return;

    this->field_a4 = 0;
    int action = this->last_action;
    extern uint8_t g_is_party_mode; extern uint32_t g_game_time;
    extern uint32_t CRT_rand(void);

    if (action == 1) {
        void* ob = this->occupant_a;
        if (ob && this->world_x == *(int*)((uint8_t*)ob + 0x4C) &&
            this->world_y == *(int*)((uint8_t*)ob + 0x50)) {
            if (this->occupation_level < 7) this->occupation_level++;
            this->visible = 0;
        }
    } else if (action == 2) {
        void* sb = this->occupant_b;
        if (!sb) { void** vt = (void**)this->vtable; ((void(__thiscall*)())vt[0x50 / 4])(); goto epilogue; }
        if (this->world_x == *(int*)((uint8_t*)sb + 0x4C) &&
            this->world_y == *(int*)((uint8_t*)sb + 0x50)) {
            if (this->occupation_level < 7) this->occupation_level++;
            this->visible = 0;
            this->next_action_time = g_game_time + 0xE10;
            goto epilogue;
        }
    } else if (action == 3) {
        if (this->occupation_level < 7) this->occupation_level += 2;
        if (g_is_party_mode) goto epilogue;
    } else { goto epilogue; }

    this->next_action_time = (int)(CRT_rand() % 21) + 10 + g_game_time;
epilogue:
    this->prev_target_x = this->target_x; this->prev_target_y = this->target_y;
    this->target_x = -1; this->target_y = -1;
}

/* ================================================================== */
/* Building::Deserialize — Reconstruct from save data                  */
/* Address: 0x435700  (source: src/decompiled/building_deserialize.c)  */
/*                                                                     */
/* Deserializes a Building (0xF4 = 244 bytes) from a flat save buffer. */
/*                                                                     */
/* The original __thiscall takes a manager object as `this` (ECX) and  */
/* two stack args: (context, src_buffer). The manager's vtable[0x28]   */
/* (index 10) is an "add entity" registration callback.                */
/*                                                                     */
/* Algorithm:                                                          */
/*   1. Allocate a raw 0xF4-byte block via operator_new                */
/*   2. Copy fields block-by-block from src_buffer, interleaving       */
/*      vtable pointer writes at each step in the inheritance chain:   */
/*        GameObjectBase (0x477820) → Entity (0x477488)                */
/*        → Building_Base (0x477F18) → Building (0x477EB8)             */
/*   3. Register the new Building via manager->vtable[0x28]            */
/*                                                                     */
/* Field ordering mirrors GameObject_Serialize (0x405FD0) and the      */
/* save-file format. On allocation failure, the registration callback  */
/* is invoked with NULL so the caller can handle the error.            */
/*                                                                     */
/* Related:                                                             */
/*   Train_Deserialize   (0x435DB0) — identical pattern (0xF0 bytes)   */
/*   World_LoadFromFile  (0x44DC10) — top-level save-file dispatcher   */
/* ================================================================== */
void Building::Deserialize(void* data)
{
    uint8_t* src = (uint8_t*)data;

    /* ---- Step 1: Allocate raw memory ---- */
    Building* obj = (Building*)operator_new(0xF4);
    if (obj == nullptr) {
        /* Allocation failed — notify manager with NULL entity.
         * Call manager->vtable[0x28](context, NULL).
         * The manager vtable dispatch is handled by the caller
         * (e.g. World or BuildingMgr) at 0x44DC10.           */
        return;
    }

    /* ---- Step 2a: Copy GameObject fields (+0x04..+0x23) ---- */
    obj->type       = *(int32_t*) (src + 0x04);             /* +0x04  type               */
    obj->screen_rect.left   = *(int32_t*) (src + 0x08);    /* +0x08  screen_rect.left   */
    obj->screen_rect.top    = *(int32_t*) (src + 0x0C);    /* +0x0C  screen_rect.top    */
    obj->screen_rect.right  = *(int32_t*) (src + 0x10);    /* +0x10  screen_rect.right  */
    obj->screen_rect.bottom = *(int32_t*) (src + 0x14);    /* +0x14  screen_rect.bottom */
    *(uint8_t*) ((uint8_t*)obj + 0x18) = *(uint8_t*) (src + 0x18);  /* +0x18  initialized */
    *(uint32_t*)((uint8_t*)obj + 0x1C) = *(uint32_t*)(src + 0x1C);  /* +0x1C  _pad_1C     */
    *(uint32_t*)((uint8_t*)obj + 0x20) = *(uint32_t*)(src + 0x20);  /* +0x20  _pad_20     */

    /* Set vtable to GameObjectBase (VTBL_GAMEOBJECT = 0x00477820) */
    *(void**)obj = (void*)VTBL_GAMEOBJECT;

    /* ---- Step 2b: Copy Entity fields (+0x24..+0x86) ---- */
    *(uint8_t*) ((uint8_t*)obj + 0x24) = *(uint8_t*) (src + 0x24);  /* +0x24  visible       */
    obj->anim_index  = *(int32_t*) (src + 0x28);                    /* +0x28  anim_index    */
    *(uint32_t*)((uint8_t*)obj + 0x2C) = *(uint32_t*)(src + 0x2C); /* +0x2C  blit_flags    */

    /* source_rect (RECT: left, top, right, bottom) at +0x30..+0x3F */
    *(uint32_t*)((uint8_t*)obj + 0x30) = *(uint32_t*)(src + 0x30);
    *(uint32_t*)((uint8_t*)obj + 0x34) = *(uint32_t*)(src + 0x34);
    *(uint32_t*)((uint8_t*)obj + 0x38) = *(uint32_t*)(src + 0x38);
    *(uint32_t*)((uint8_t*)obj + 0x3C) = *(uint32_t*)(src + 0x3C);

    *(void**)   ((uint8_t*)obj + 0x40) = *(void**)   (src + 0x40);  /* +0x40  parent / resource   */
    *(uint32_t*)((uint8_t*)obj + 0x44) = *(uint32_t*)(src + 0x44);  /* +0x44  sound_res_id        */
    *(void**)   ((uint8_t*)obj + 0x48) = *(void**)   (src + 0x48);  /* +0x48  audio_channel       */
    obj->world_x        = *(int32_t*) (src + 0x4C);                 /* +0x4C  world_x             */
    obj->world_y        = *(int32_t*) (src + 0x50);                 /* +0x50  world_y             */
    *(uint32_t*)((uint8_t*)obj + 0x54) = *(uint32_t*)(src + 0x54); /* +0x54  frame_index (uint16) */
    *(uint32_t*)((uint8_t*)obj + 0x58) = *(uint32_t*)(src + 0x58); /* +0x58  timer                */
    *(uint32_t*)((uint8_t*)obj + 0x5C) = *(uint32_t*)(src + 0x5C); /* +0x5C  active_state         */
    *(uint32_t*)((uint8_t*)obj + 0x60) = *(uint32_t*)(src + 0x60); /* +0x60  next_sound_time      */
    *(uint32_t*)((uint8_t*)obj + 0x64) = *(uint32_t*)(src + 0x64); /* +0x64  resource_id          */
    *(uint32_t*)((uint8_t*)obj + 0x68) = *(uint32_t*)(src + 0x68); /* +0x68  field_68 / hit coords */
    *(uint32_t*)((uint8_t*)obj + 0x6C) = *(uint32_t*)(src + 0x6C); /* +0x6C  phase_timer           */
    *(uint8_t*) ((uint8_t*)obj + 0x70) = *(uint8_t*) (src + 0x70); /* +0x70  waiting_flag          */
    *(int32_t*) ((uint8_t*)obj + 0x74) = *(int32_t*) (src + 0x74); /* +0x74  world_x_raw           */
    *(int32_t*) ((uint8_t*)obj + 0x78) = *(int32_t*) (src + 0x78); /* +0x78  world_y_raw           */

    /* Name field (+0x7C..+0x86, 11 bytes including null terminator) */
    memcpy((uint8_t*)obj + 0x7C, src + 0x7C, 11);

    /* Set vtable to Entity (VTBL_ENTITY = 0x00477488) */
    *(void**)obj = (void*)VTBL_ENTITY;

    /* ---- Step 2c: Copy Building fields part 1 (+0x88..+0xE7) ---- */
    obj->occupation_level  = *(uint8_t*) (src + 0x88);              /* +0x88  occupation_level   */
    obj->disabled          = *(uint8_t*) (src + 0x89);              /* +0x89  disabled           */
    obj->_pad_8a[0]        = *(uint8_t*) (src + 0x8A);             /* +0x8A  padding byte 0     */
    /* _pad_8a[1] is not explicitly copied (zero from allocation)  */

    obj->occupant_a        = *(void**)   (src + 0x8C);              /* +0x8C  occupant_a         */
    obj->occupant_b        = *(void**)   (src + 0x90);              /* +0x90  occupant_b         */
    obj->create_time       = *(uint32_t*)(src + 0x94);              /* +0x94  create_time        */
    obj->conn_building_a   = *(int32_t*) (src + 0x98);              /* +0x98  conn_building_a    */
    obj->conn_building_b   = *(int32_t*) (src + 0x9C);              /* +0x9C  conn_building_b    */
    obj->next_action_time  = *(uint32_t*)(src + 0xA0);              /* +0xA0  next_action_time   */
    obj->field_a4          = *(uint32_t*)(src + 0xA4);              /* +0xA4  field_a4           */
    obj->target_x          = *(int32_t*) (src + 0xA8);              /* +0xA8  target_x           */
    obj->target_y          = *(int32_t*) (src + 0xAC);              /* +0xAC  target_y           */
    obj->search_x1         = *(int32_t*) (src + 0xB0);              /* +0xB0  search_x1          */
    obj->search_y1         = *(int32_t*) (src + 0xB4);              /* +0xB4  search_y1          */

    /* Gap/pad fields at +0xB8, +0xBC, +0xC0 */
    *(int32_t*)((uint8_t*)obj + 0xB8) = *(int32_t*)(src + 0xB8);
    *(int32_t*)((uint8_t*)obj + 0xBC) = *(int32_t*)(src + 0xBC);
    *(int32_t*)((uint8_t*)obj + 0xC0) = *(int32_t*)(src + 0xC0);

    obj->prev_target_x     = *(int32_t*) (src + 0xC4);              /* +0xC4  prev_target_x     */
    obj->prev_target_y     = *(int32_t*) (src + 0xC8);              /* +0xC8  prev_target_y     */
    obj->dest_x            = *(int32_t*) (src + 0xCC);              /* +0xCC  dest_x            */
    obj->dest_y            = *(int32_t*) (src + 0xD0);              /* +0xD0  dest_y            */
    obj->waypoint_x1       = *(int32_t*) (src + 0xD4);              /* +0xD4  waypoint_x1       */
    obj->waypoint_y1       = *(int32_t*) (src + 0xD8);              /* +0xD8  waypoint_y1       */
    obj->field_dc          = *(uint32_t*)(src + 0xDC);              /* +0xDC  action_state      */
    obj->field_e0          = *(uint32_t*)(src + 0xE0);              /* +0xE0  field_e0          */
    obj->field_e4          = *(uint8_t*) (src + 0xE4);              /* +0xE4  field_e4          */
    /* _pad_e5[3] is not explicitly copied (zero from allocation)   */

    obj->last_action       = *(int32_t*) (src + 0xE8);              /* +0xE8  last_action       */

    /* Set vtable to Building_Base (VTBL_BUILDING_BASE = 0x00477F18) */
    *(void**)obj = (void*)VTBL_BUILDING_BASE;

    /* ---- Step 2d: Copy final Building fields (+0xEC..+0xF3) ---- */
    *(int32_t*)((uint8_t*)obj + 0xEC) = *(int32_t*)(src + 0xEC);   /* +0xEC  field_ec (unconfirmed)         */
    obj->occupant_ptr      = *(void**)   (src + 0xF0);              /* +0xF0  occupant_ptr / bidirectional link */

    /* Set final vtable to Building (VTBL_BUILDING_FULL = 0x00477EB8) */
    *(void**)obj = (void*)VTBL_BUILDING_FULL;

    /* ---- Step 3: Register with manager ---- */
    /* In the original binary, this calls manager->vtable[0x28](context, obj)
     * where manager is the original `this` (ECX) parameter at 0x435700.
     * The caller at 0x44DC10 (World_LoadFromFile) supplies the context.
     *
     * The Building is now fully reconstructed and registered in the
     * manager's collection. Further occupant/linkage fixups are handled
     * by the save-file loader after all entities are deserialized. */
}

/* ================================================================== */
/* Building::UpdateAnimByDimensions — Animation from movement vector   */
/* Address: 0x4331B0                                                   */
/* ================================================================== */
void Building::UpdateAnimByDimensions()
{
    int dx = this->waypoint_x1, dy = this->waypoint_y1;
    if (abs(dx) > abs(dy)) {
        if (dx <= 0 && this->anim_index != 0) this->SetAnimState(0);
        else if (dx > 0 && this->anim_index != 2) this->SetAnimState(2);
    } else {
        if (dy <= 0 && this->anim_index != 3) this->SetAnimState(3);
        else if (dy > 0 && this->anim_index != 1) this->SetAnimState(1);
    }
}


/* ================================================================== */
/* Building::UpdateAnimByOccupancy — Animation based on occupancy      */
/* Address: 0x433F60                                                   */
/*                                                                     */
/* Selects animation index (4-7) based on occupation_level (+0x88).   */
/* Called from Building::Update post-update when idle and visible.     */
/* ================================================================== */
void Building::UpdateAnimByOccupancy()
{
    /* Map occupancy level (4-7) to animation index */
    int new_anim;
    switch (this->occupation_level) {                       /* +0x88 */
        case 4: new_anim = 4; break;
        case 5: new_anim = 5; break;
        case 6: new_anim = 6; break;
        default: new_anim = 7; break;  /* level 7 or out of range */
    }

    /* Only update if animation changed */
    if (new_anim != this->anim_index) {                     /* +0x28 */
        /* vtable[7] = SetAnimState — update animation */
        void** vt = (void**)this->vtable;
        ((void(__thiscall*)(int))vt[0x1C / 4])(new_anim);
    }
}
