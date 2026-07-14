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
/* Building::CalcMoveTarget — Calculate movement target coordinates    */
/* Address: 0x432DA0                                                   */
/*                                                                     */
/* Computes the target position for the building's current action.     */
/* See src/decompiled/building_calcmovetarget.c for full details.      */
/* ================================================================== */
void Building::CalcMoveTarget()
{
    /* If no destination set, nothing to calculate */
    if (this->dest_x == -1 && this->dest_y == -1) {         /* +0xCC, +0xD0 */
        return;
    }

    /* Calculate world-space target based on destination and waypoint.
     * Full implementation: src/decompiled/building_calcmovetarget.c */
    int dx = this->dest_x;                                  /* +0xCC */
    int dy = this->dest_y;                                  /* +0xD0 */

    /* Snap to waypoint if set */
    if (this->waypoint_x1 != -1) {                          /* +0xD4 */
        dx = this->waypoint_x1;
    }
    if (this->waypoint_y1 != -1) {                          /* +0xD8 */
        dy = this->waypoint_y1;
    }

    /* Store as target */
    this->target_x = dx;                                    /* +0xA8 */
    this->target_y = dy;                                    /* +0xAC */
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
        if (this->visible) this->CalcMoveTarget();
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
/* Address: 0x435700                                                   */
/* ================================================================== */
void Building::Deserialize(void* data)
{
    /* Allocates a 0xF4-byte Building, copies fields from the save
     * buffer stepping through vtable chain (GameObject → Entity →
     * Building_Base → Building), then registers with manager.
     * See src/decompiled/building_deserialize.c for full ~18K impl. */
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
