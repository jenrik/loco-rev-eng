// Status: TRANSCRIBED (new virtual method implementations from disassembly; needs disassembly-line validation)
/**
 * Building.cpp — Building game object class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "Building.h"
#include "../core/GameObject.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* CRT imports — C linkage */
extern "C" {
    uint32_t     CRT_rand(void);                                         /* 0x466150 */
    const wchar_t* CRT_wcsstr(const wchar_t* a, const wchar_t* b);      /* 0x471480 */
    int          LoadStringA(HINSTANCE hInst, UINT id, char* buf, int maxLen);
}

/* ================================================================== */
/* File-level helper utilities (extracted from duplicated lambdas)     */
/* ================================================================== */

/** Convert world pixel coordinate to tile index (>> 4, -1 if negative). */
static inline int worldToTile(int coord) {
    return (coord < 0) ? -1 : (coord >> 4);
}

/** Random integer in [lo, hi] inclusive, handling reversed ranges. */
static inline int randInRange(int lo, int hi) {
    if (hi == lo) return lo;
    if (hi > lo) {
        int w = hi - lo + 1;
        return (w == 1) ? lo : (int)(CRT_rand() % w) + lo;
    } else {
        int w = lo - hi + 1;
        return (w == 1) ? hi : (int)(CRT_rand() % w) + hi;
    }
}

/* Forward-declared types used in externs */
class BuildingMgr;
class InputMgr;
class TileMap;

/* BuildingMgr helpers */
extern void BuildingMgr_CompactCollections(BuildingMgr* bldg_mgr);      /* 0x434870 */

/* Heap allocation */
extern void* operator_new(size_t size);                                  /* 0x465CE0 */

/* Globals — declared with C++ linkage; do NOT re-declare with C linkage. */
extern BuildingMgr* g_building_mgr;         /* 0x485448 — BuildingMgr singleton */
extern Entity*      g_selected_building;    /* currently selected building (pointer) */
extern void         GLOBAL_free(void* ptr);
extern InputMgr*    g_input_mgr;            /* 0x4A9990 */
extern void*        INPUT_FindObjectAt(InputMgr* mgr, int mode);
extern TileMap*     g_tilemap;
extern void*        TileMap_GetObjectAt(TileMap* tilemap, int tx, int ty, int flags);
extern uint8_t      g_building_animating;
extern void*        CRT_localtime(const time_t* timer);
extern int          Game_CheckTimeInRange(void* tm, int* start, int* end);
extern int          Vehicle_GetOccupantCount(void* v);
extern uint8_t      g_is_game_active;

/* GameObject destructor body (0x405870) — called by BaseCleanup */
extern void GameObject_DtorBody(void* obj);

/* Game selection helper (0x4113A0) */
extern void Game_SelectGameObject(void* game, void* obj);

/* Game singleton — used for selection/deselection */
extern void* g_game;

/* TileMap helpers — used by StepToward, TeleportTo, FindNearestConnectionNode */
extern int      TileMap_PathDistance(void* tilemap, int x1, int y1, int x2, int y2);  /* 0x45c7a0 */
extern void*    TileMap_FindTileByType(void* tilemap, int x, int y,
                                      int search_flags, int tile_type);               /* 0x457ce0 */

/* NodeSet helper — used by FindNearestConnectionNode */
extern uint8_t  NodeSet_GetConnectionFlag(void* ns, uint32_t idx);                    /* 0x45dd80 */

/* ROM string at 0x47E4FC in .rdata */
static const wchar_t PARTY_STRING[] = L"PARTY";


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
     * This calls InitBase() for Entity-level resource loading,
     * initializes all Building-specific fields.                      */
    this->BaseCtor(resource_id);                /* +0x433A20 */

    /* Step 2: Set occupation level to 4 (redundant with BaseCtor
     * but ensures correctness).                                     */
    this->occupation_level = 4;                 /* +0x88 */

    /* Step 3: Zero the occupant cross-reference pointer.
     * Offset +0xF0 is a bidirectional link: in a Building it points
     * to the occupying entity; in an occupant entity it points to
     * the building.                                                */
    this->occupant_ptr = nullptr;               /* +0xF0 */
}


/* ================================================================== */
/* Building::Building — Protected intermediate constructor (Train)     */
/* Address: 0x433A20  (base_only=true path, size: 407 bytes)           */
/*                                                                     */
/* Called by:                                                          */
/*   TrainEntity::TrainEntity (0x4533D8) — via initializer list       */
/*                                                                     */
/* When base_only is true, this deliberately does NOT initialize       */
/* Building's +0xF0 occupant_ptr tail field because Train is only      */
/* 0xF0 bytes (no occupant_ptr). All other Building fields up to       */
/* +0xEC are initialized identically to the full constructor.          */
/* ================================================================== */
Building::Building(int resource_id, bool base_only)
{
    /* --- Step 1: Initialize Entity-level resource data ---
     * In the binary, Entity::Entity() (0x405790) is called which:
     *   1. Calls GameObject::GameObject() — sets vtable, type, zero-fields
     *   2. If resource_id > 0: calls InitBase(resource_id, anim_idx, false)
     * In C++, Entity is already constructed via the ctor chain, so we
     * call InitBase directly for resource loading.                     */
    this->InitBase(resource_id, -1, false);

    /* --- Step 2: Initialize Building-specific byte flags --- */
    this->disabled          = 0;            /* +0x89 — building is active */
    this->occupation_level  = 4;            /* +0x88 — starts at level 4  */
    this->field_e4          = 0;            /* +0xE4 */

    /* --- Step 3: Record creation timestamp --- */
    this->create_time = g_game_time;        /* +0x94 */

    /* --- Step 4: Get resource pointer --- */
    RESDATA* resource = (RESDATA*)this->parent;  /* +0x40 */

    /* --- Step 5: Store the resource ID --- */
    this->field_64 = resource_id;           /* +0x64 */

    /* --- Step 6: Zero / -1 initialize all remaining fields --- */
    this->field_68 = 0;                     /* +0x68 */
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
    this->field_ec           = 0;           /* +0xEC */

    /* --- Step 7: When base_only, skip occupant_ptr init.
     * The Train subclass is only 0xF0 bytes and does not have
     * the occupant_ptr field at +0xF0.                              */
    if (!base_only) {
        this->occupant_ptr = nullptr;       /* +0xF0 */
    }

    /* --- Step 8: Set the building's name --- */
    if (resource != nullptr) {
        char* res_name = (char*)resource + RESNAME_OFFSET;  /* +0x14D */

        if (*res_name == '\0') {
            /* Branch A: No resource-provided name.
             * Generate a random name from the game's String Table.  */
            UINT name_id;
            uint32_t rand_val = CRT_rand();                     /* +0x466150 */

            if (*(uint32_t*)((uint8_t*)resource + RESCLASS_OFFSET) == 0x4D) {
                /* Residential: "M" = Minifigure house */
                name_id = (rand_val % RESIDENTIAL_NAME_COUNT) + RESIDENTIAL_NAME_BASE;
            } else {
                /* Commercial / Industrial */
                name_id = (rand_val % COMMERCIAL_NAME_COUNT) + COMMERCIAL_NAME_BASE;
            }

            /* Load name from executable's string table. */
            HINSTANCE hInst = *(HINSTANCE*)((uint8_t*)g_main_window + 0x0C);
            LoadStringA(hInst, name_id, this->name, 10);
        } else {
            /* Branch B: Resource has a custom name. */
            this->CopyName(res_name);                           /* +0x405E20 */

            /* Check building sub-type at resource+0x08.
             * If type == STATION (7), compact BuildingMgr collections. */
            uint8_t sub_type = *(uint8_t*)((uint8_t*)resource + 0x08);
            if (sub_type == SUBTYPE_STATION) {
                BuildingMgr_CompactCollections(g_building_mgr); /* +0x434870 */
            }

            /* PARTY mode trigger — DECOMPILER NOTE:
             * The check is inverted: party mode activates when the
             * resource name does NOT contain "PARTY". This is binary-
             * correct behavior per Ghidra.                              */
            if (CRT_wcsstr((const wchar_t*)res_name, PARTY_STRING) == 0) {
                g_is_party_mode    = 1;
                g_party_start_time = g_game_time;
            }
        }
    }
}


/* ================================================================== */
/* Building::~Building — Virtual destructor (vtable[0])                */
/* Address: 0x432720 (scalar deleting destructor wrapper)              */
/* Body: calls BaseDtor (0x432740) for cleanup logic.                  */
/*                                                                     */
/* The MSVC scalar deleting destructor at 0x432720 calls BaseDtor,    */
/* then conditionally calls GLOBAL_free(this) if (flags & 1). In C++,  */
/* the compiler emits the delete call automatically — we only keep     */
/* the cleanup logic.                                                  */
/* ================================================================== */
Building::~Building()
{
    this->BaseDtor();
}


/* ================================================================== */
/* Building::BaseCtor — Shared base constructor                        */
/* Address: 0x433A20  (size: 407 bytes)                                */
/*                                                                     */
/* Called by:                                                          */
/*   Building() (0x4326F0)                                             */
/*   Train constructor (0x4533D8)                                     */
/*                                                                     */
/* Summary:                                                            */
/*   1. Calls InitBase() for Entity-level resource loading             */
/*   2. Zero/-1 initializes all Building fields                        */
/*   3. If resource has a name, copies it; otherwise generates random  */
/*      name from string table (residential vs commercial pool)        */
/*   4. If sub-type == STATION (7), compacts BuildingMgr collections   */
/*   5. If resource name does NOT contain "PARTY", activates party mode */
/* ================================================================== */
void Building::BaseCtor(int resource_id)
{
    /* --- Step 1: Initialize Entity-level resource data ---
     * In the binary, Entity::Entity() (0x405790) is called which:
     *   1. Calls GameObject::GameObject() — sets vtable, type, zero-fields
     *   2. If resource_id > 0: calls InitBase(resource_id, anim_idx, false)
     * In C++, Entity is already constructed via the ctor chain, so we
     * call InitBase directly for resource loading.                     */
    this->InitBase(resource_id, -1, false);

    /* --- Step 2: Initialize Building-specific byte flags --- */
    this->disabled          = 0;            /* +0x89 — building is active */
    this->occupation_level  = 4;            /* +0x88 — starts at level 4  */
    this->field_e4          = 0;            /* +0xE4 */

    /* --- Step 3: Record creation timestamp --- */
    this->create_time = g_game_time;        /* +0x94 */

    /* --- Step 4: Get resource pointer --- */
    RESDATA* resource = (RESDATA*)this->parent;  /* +0x40 (resource stored in parent slot before overridden) */
    /* NOTE: In the original binary, the resource pointer is stored at +0x40.
     * Our Entity class uses 'parent' at +0x40 for scene graph, but during
     * construction, +0x40 holds the RESDATA* before it's replaced. */

    /* --- Step 5: Store the resource ID --- */
    this->field_64 = resource_id;           /* +0x64 */

    /* --- Step 6: Zero / -1 initialize all remaining fields --- */
    this->field_68 = 0;                     /* +0x68 */
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

    /* --- Step 7: Set the building's name --- */
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

            /* PARTY mode trigger — DECOMPILER NOTE:
             * The check is inverted: party mode activates when the
             * resource name does NOT contain "PARTY". This is binary-
             * correct behavior (confirmed: wcsstr returns NULL → jz
             * taken → activate party). Party mode appears to be the
             * default state; naming a building "PARTY" disables it.      */
            if (CRT_wcsstr((const wchar_t*)res_name, PARTY_STRING) == 0) {
                g_is_party_mode    = 1;
                g_party_start_time = g_game_time;
            }
        }
    }

}


/* ================================================================== */
/* Building::BaseDtor — Base destructor body                           */
/* Address: 0x432740                                                   */
/*                                                                     */
/* Cleaning logic:                                                     */
/*   1. Deselect via Game_SelectGameObject if this is selected         */
/*   2. If occupant_ptr != NULL, call RemoveOccupant() to clear link   */
/*   3. Call Building::BaseCleanup() for parent/entity cleanup         */
/*                                                                     */
/* Notes: The binary sets vtable to 0x477EB8 at entry (compiler-generated */
/* in C++). Exception registration is compiler-generated and omitted.  */
/* ================================================================== */
void Building::BaseDtor()
{
    /* Deselect if this is the currently selected building */
    if (g_selected_building == this) {
        Game_SelectGameObject(g_game, nullptr);
    }

    /* Remove any occupant still attached — calls RemoveOccupant(this)
     * which clears the bidirectional occupant link and teleports the
     * building to the exit position. */
    if (this->occupant_ptr != nullptr) {
        this->RemoveOccupant();
    }

    /* Entity-level cleanup (parent disconnection + GameObject_DtorBody) */
    this->BaseCleanup();
}


/* ================================================================== */
/* Building::BaseCleanup — Lowest-level entity cleanup                 */
/* Address: 0x433BE0                                                   */
/*                                                                     */
/* Called by BaseDtor (0x432740) as the final cleanup step.           */
/*                                                                     */
/* Algorithm:                                                          */
/*   1. Read scene-graph parent from +0x90 (occupant_b slot used as   */
/*      parent link during cleanup — NOT from the normal +0x40 parent) */
/*   2. Search parent's 5-slot child pointer array at parent+0xA4     */
/*      for `this`, clear the matching slot                            */
/*   3. Decrement parent's child_count at parent+0x8E                  */
/*   4. Clear this->occupant_b (+0x90)                                */
/*   5. Call GameObject_DtorBody(this) for resource/audio cleanup      */
/*                                                                     */
/* NOTE: The parent is read from +0x90 which overlaps with occupant_b. */
/* During normal operation this holds a different value, but during    */
/* destruction the binary repurposes +0x90 as a scene-graph parent     */
/* link. The normal Entity::parent at +0x40 is NOT used here.          */
/*                                                                     */
/* DECOMPILER NOTE: The binary also resets the vtable to 0x477F18 at   */
/* entry; this is compiler-managed in C++ and omitted.                 */
/* ================================================================== */
void Building::BaseCleanup()
{
    /* --- Step 1: Read scene-graph parent from +0x90 (occupant_b slot) --- */
    Entity* scene_parent = this->occupant_b;

    if (scene_parent != nullptr) {
        /* --- Step 2: Search parent's 5-slot child array at parent+0xA4 ---
         * Each slot holds a pointer to a child entity. Find and clear ours.*/
        Entity** child_array = (Entity**)((uint8_t*)scene_parent + 0xA4);
        int slot;
        for (slot = 0; slot < 5; slot++) {
            if (child_array[slot] == this) {
                child_array[slot] = nullptr;
                break;
            }
        }

        /* --- Step 3: Decrement parent's child_count at parent+0x8E ---
         * If our pointer wasn't found in the 5-slot array (slot >= 5),
         * skip the decrement and just null out the parent link. */
        if (slot < 5) {
            uint8_t* child_count = (uint8_t*)scene_parent + 0x8E;
            *child_count = *child_count - 1;
        }

        /* --- Step 4: Clear the parent link --- */
        this->occupant_b = nullptr;
    }

    /* --- Step 5: Release GameObject-level resources ---
     * Calls the destructor body (0x405870), not the scalar deleting
     * destructor wrapper. This releases audio channel, sound/resource
     * handles without calling `operator delete`. In C++ we call the
     * body directly rather than going through the destructor, avoiding
     * double-destruction when the C++ runtime also destroys the base. */
    GameObject_DtorBody(this);
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
/*   Building::MoveToTarget (0x434399)                                 */
/*   Building::HandleAction (0x434100)                                 */
/*   Building::UpdateAnimByOccupancy (0x433F60)                        */
/*   INPUT_FindObjectAt (0x41E1F0)                                     */
/*                                                                     */
/* Per-frame AI dispatch for all buildings. Each tick:                 */
/*   1. Skip if disabled (+0x89) or anim_index > 7                    */
/*   2. Check occupant timeout                                         */
/*   3. PARTY mode: delegate to PartyModeUpdate                       */
/*   4. Normal mode: decide action if idle, poll completion if active  */
/*   5. Post-update: refresh animation if idle and visible             */
/* ================================================================== */
void Building::Update(void* /*next_entity*/)
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
     * In party mode, bypass normal AI and delegate to PartyModeUpdate. */
    if (g_is_party_mode != 0) {                             /* 0x48548C */
        this->PartyModeUpdate(nullptr);
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
                this->MoveToTarget();                        /* 0x434399, uses occupant_a */
            } else if (action == 2) {
                /* Action 2: Spawn — move toward occupant_b */
                this->MoveToTarget();                        /* 0x434399, uses occupant_b */
            } else if (action == 3) {
                /* Action 3: Idle/wander.
                 * Look for an object under the input hotspot.
                 * If found at a new position, teleport there. */
                void* found = INPUT_FindObjectAt(g_input_mgr, 2);

                if (found != nullptr) {
                    GameObject* found_obj = (GameObject*)found;
                    int fx = found_obj->world_x;            /* +0x4C */
                    int fy = found_obj->world_y;            /* +0x50 */

                    if (fx != this->prev_target_x ||         /* +0xC4 */
                        fy != this->prev_target_y)           /* +0xC8 */
                    {
                        this->TeleportTo(fx, fy);
                    }
                }

                /* If no destination, schedule random idle timer: 10..30 ticks */
                if (this->dest_x == -1 && this->dest_y == -1) {  /* +0xCC, +0xD0 */
                    uint32_t r = CRT_rand();
                    this->next_action_time = (r % 21 + 10)  /* +0xA0 */
                                            + g_game_time;
                }
            }
        }
    } else {
        /* ---- Branch B: Action in progress — poll completion ---- */
        if (this->IsActionComplete() == 0) {
            /* Action done. Check if we've arrived at target. */
            int cur_x = this->world_x;                      /* +0x4C */
            int cur_y = this->world_y;                      /* +0x50 */

            if (this->target_x == cur_x && this->target_y == cur_y) {
                /* At target — finalize the action */
                this->HandleAction();                        /* +0x434100 */
            } else {
                /* Not at target — advance one step */
                this->StepToward(cur_x, cur_y);
            }
        }
    }

post_update:
    /* Only refresh animation when idle and visible */
    if (this->field_dc == 0 && this->visible == 1) {       /* +0xDC, +0x24 */
        this->UpdateAnimByOccupancy();                      /* +0x433F60 */
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
/* Algorithm:                                                          */
/*   If entity is NULL: if we have an occupant (occupant_ptr != 0),    */
/*     call RemoveOccupant() to clear it.                              */
/*   Otherwise: search the ENTITY's 9-slot occupant tracking array at  */
/*     entity+0x38 for a free slot. Store `this` (the building) into   */
/*     the entity's array so the entity knows which building it's in.  */
/*     Set building->occupant_ptr = entity for bidirectional link.     */
/*   On success: increment occupation_level (cap 7), hide building     */
/*     (visible=0), read entry coords from entity's model data, then   */
/*     validate the road path to the entry point via StepToward loop.  */
/*                                                                     */
/* NOTE: The occupant array is at entity+0x38 (the occupant ENTITY's   */
/* slots), not the Building's slots. This is correct — entities track  */
/* which buildings they occupy. The building only stores occupant_ptr  */
/* at +0xF0 for the current active occupant.                          */
/*                                                                     */
/* DECOMPILER NOTE: Searches slots 0-8 (9 slots). RemoveOccupant       */
/* only searches slots 0-7 (8 slots). Slot 8 occupants are never       */
/* cleaned up — this is a binary-correct reproduction.                 */
/* ================================================================== */
void Building::AddOccupant(Entity* entity)
{
    if (entity == nullptr) {
        /* NULL entity: if we currently have an occupant, remove it.
         * The binary reads this->occupant_ptr (+0xF0), NOT from entity. */
        if (this->occupant_ptr != nullptr) {
            /* Remove the building's current occupant.
             * The binary calls Building::RemoveOccupant(this) with `this`
             * as the building — it clears the occupant link bidirectional.*/
            this->RemoveOccupant();
        }
        return;
    }

    /* --- Find a free slot in the entity's 9-slot occupant tracking
     *     array at entity+0x38. The entity uses this to track which
     *     buildings it's currently occupying. --- */
    Entity** occupant_slots = (Entity**)((uint8_t*)entity + 0x38);

    int slot;
    for (slot = 0; slot < 9; slot++) {
        if (occupant_slots[slot] == nullptr || occupant_slots[slot] == this) {
            occupant_slots[slot] = this;
            this->occupant_ptr = entity;
            break;
        }
    }

    /* Check if we successfully claimed a slot */
    if (this->occupant_ptr == nullptr) {
        /* Building full — decrement occupancy level */
        if (this->occupation_level != 0) {
            this->occupation_level--;
        }
        return;
    }

    /* Increment occupancy level (cap at 7) */
    if (this->occupation_level < 7) {
        this->occupation_level++;
    }

    /* Hide the building (building becomes invisible when occupied).
     * In the binary, the building sprite is hidden while an occupant
     * is inside — the occupant entity remains visible elsewhere. */
    this->visible = 0;

    /* Read the entity's model data for entry coordinates */
    RESDATA* model_data = *(RESDATA**)((uint8_t*)entity + 0x20);
    if (model_data == nullptr) return;

    void* entry_data = *(void**)((uint8_t*)model_data + 0x14);
    if (entry_data == nullptr) return;

    int entry_x = *(int*)((uint8_t*)entry_data + 0x4C);
    int entry_y = *(int*)((uint8_t*)entry_data + 0x50);
    this->dest_x = entry_x;
    this->dest_y = entry_y;

    /* Road validation loop: check entry tile is on a road (type 3). */
    int prev_tx = -1, prev_ty = -1;
    int tile_type = 3;

    while (tile_type == 3) {
        if (this->dest_x == prev_tx && this->dest_y == prev_ty) break;
        prev_tx = this->dest_x;
        prev_ty = this->dest_y;

        /* Advance one step toward entry point via vtable[18] */
        this->StepToward(this->dest_x, this->dest_y);

        int tile_x = worldToTile(this->dest_x);
        int tile_y = worldToTile(this->dest_y);

        void* tile = TileMap_GetObjectAt(g_tilemap, tile_x, tile_y, 0);
        if (tile != nullptr) {
            void* tile_parent = *(void**)((uint8_t*)tile + 0x40);
            tile_type = (tile_parent != nullptr) ? *(uint8_t*)((uint8_t*)tile_parent + 0x08) : 0;
        } else {
            break;
        }
    }
}


/* ================================================================== */
/* Building::RemoveOccupant — Remove occupant from building            */
/* Address: 0x4336A0  (size: 435 bytes)                                */
/*                                                                     */
/* Called by BaseDtor (0x432740) when destroying a building that still  */
/* has an occupant. Also called from AddOccupant for NULL-entity path.  */
/*                                                                     */
/* Algorithm:                                                          */
/*   1. Read this->occupant_ptr (+0xF0) to get the occupant entity     */
/*   2. Search entity's 8-slot occupant tracking array at entity+0x38  */
/*      for `this` (the building), clear the slot                      */
/*   3. Calculate exit position based on building's road class:         */
/*      - Horizontal road (0x12): random offset along road            */
/*      - Vertical road (0x13): random offset perpendicular to road   */
/*      - Other: simple offset subtraction                             */
/*   4. Validate exit tile (must be type 0x0C = walkable surface)      */
/*   5. Fallback to occupant_a's position if tile invalid              */
/*   6. Teleport building to exit via MoveTo(), clear occupant_ptr,    */
/*      trigger display refresh via StepToward, set visible = 1        */
/*                                                                     */
/* NOTE: The occupant entity's slot array is searched (entity+0x38),   */
/* NOT the building's slots. Entity tracks which building it's in.     */
/* The building teleports/moves, not the entity — the building's       */
/* position updates to move it away from the departed occupant.        */
/*                                                                     */
/* DECOMPILER NOTE: Searches slots 0-7 (8 slots) but AddOccupant       */
/* searches 0-8 (9 slots). Slot 8 occupants are never found here.      */
/* This is binary-correct behavior per Ghidra at 0x4336A0.             */
/* ================================================================== */
void Building::RemoveOccupant()
{
    /* --- Step 1: Get the occupant entity from occupant_ptr --- */
    Entity* occupant = this->occupant_ptr;                   /* +0xF0 */
    if (occupant == nullptr) return;

    /* --- Step 2: Find and clear this building in occupant's slot
     *     array at occupant+0x38 (8 slots). --- */
    Entity** occupant_slots = (Entity**)((uint8_t*)occupant + 0x38);
    for (int idx = 0; idx < 8; idx++) {
        if (occupant_slots[idx] == this) {
            occupant_slots[idx] = nullptr;
            break;
        }
    }

    /* --- Step 3: Determine road class from entity's model/tile data --- */
    void* type_info = *(void**)((uint8_t*)occupant + 0x20);
    void* resdata = (type_info != nullptr) ? *(void**)((uint8_t*)type_info + 0x14) : nullptr;
    uint8_t road_class = 0xFF;

    if (resdata != nullptr) {
        void* tile_res = *(void**)((uint8_t*)resdata + 0x40);
        road_class = *(uint8_t*)((uint8_t*)tile_res + 0x63A);
    }

    /* --- Step 4: Read building's frame data for sprite offset --- */
    void* frame_data = *(void**)((uint8_t*)this + 0x40);
    int16_t offset_x = *(int16_t*)((uint8_t*)frame_data + 0x32);
    int16_t offset_y = *(int16_t*)((uint8_t*)frame_data + 0x34);

    int exit_x, exit_y;
    int move_tx = this->dest_x;                             /* +0xCC */
    int move_ty = this->dest_y;                             /* +0xD0 */

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

    /* --- Step 5: Convert to tile coordinates --- */
    int tile_x = worldToTile(exit_x);
    int tile_y = worldToTile(exit_y);

    /* Validate exit tile (must be type 0x0C = walkable surface) */
    void* tile_obj = TileMap_GetObjectAt(g_tilemap, tile_x, tile_y, 0);

    bool use_fallback = true;
    if (tile_obj != nullptr) {
        void* tile_frame = *(void**)((uint8_t*)tile_obj + 0x40);
        uint8_t tile_type = (tile_frame != nullptr) ? *(uint8_t*)((uint8_t*)tile_frame + 8) : 0;
        if (tile_type == 0x0C) {
            use_fallback = false;
        }
    }

    if (use_fallback) {
        /* Fallback: use occupant_a's position */
        Entity* fallback = this->occupant_a;                /* +0x8C */
        if (fallback != nullptr) {
            exit_x = fallback->world_x;
            exit_y = fallback->world_y;
        }
    }

    /* --- Step 6: Teleport to exit, clear link, refresh display --- */
    this->MoveTo(exit_x, exit_y);                           /* Entity vtable[3] */
    this->occupant_ptr = nullptr;                           /* +0xF0 */
    this->StepToward(move_tx, move_ty);                     /* vtable[18] */
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
    this->field_dc = (int32_t)sqrt((double)(dx * dx + dy * dy));  /* +0xDC */

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
/* makes building visible, and calls OnOccupantReady.                  */
/* ================================================================== */
void Building::CheckTimeout()
{
    /* Base-class update (animation, audio, frame stepping).
     * Address 0x405C40 is Entity::Update (vtable[10]), which handles
     * frame stepping, animation boundaries, waiting flags, sound
     * playback, and SetFrame calls. Calling GameObject::Update()
     * would be a no-op — the binary dispatches through the vtable
     * to Entity::Update. */
    this->Entity::Update();                                 /* 0x405C40 */

    /* Check active occupant timeout */
    if (this->field_a4 != 0) {                              /* +0xA4: timeout_start */
        /* Has 180 ticks elapsed since timeout started? */
        if (this->field_a4 + 180 < g_game_time) {
            /* Decrement occupant count if any */
            if (this->occupation_level != 0) {              /* +0x88 */
                this->occupation_level--;
            }

            /* Reset action timer and mark visible */
            this->next_action_time = 0;                     /* +0xA0 */
            this->visible = 1;                              /* +0x24 */

            /* OnOccupantReady callback */
            this->OnOccupantReady(nullptr);
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
    int game_time = (int)g_game_time;

    /* Not ready to decide yet */
    if (game_time < (int)this->next_action_time) {          /* +0xA0 */
        return 0;
    }

    /* If this building is selected and animating, defer */
    if (g_selected_building == this && g_building_animating != 0) {
        return 0;
    }

    /* Mark visible if hidden */
    if (this->visible == 0) {                               /* +0x24 */
        this->visible = 1;
    }

    /* Convert game tick to local time */
    void* time_info = CRT_localtime((const time_t*)&game_time);

    /* Check spawn_building schedule (+0x90 = occupant_b).
     * If time is within its active window, go there.
     * Occupant's resource is at Entity::resource (+0x40). */
    if (this->occupant_b != nullptr) {                     /* +0x90 */
        RESDATA* resource = (RESDATA*)this->occupant_b->resource;
        if (Game_CheckTimeInRange(time_info,
                (int*)((uint8_t*)resource + 0x534),
                (int*)((uint8_t*)resource + 0x548))) {
            return 2;  /* spawn */
        }
    }

    /* Check occupy_building schedule (+0x8C = occupant_a).
     * If time is OUTSIDE its active window, go there. */
    if (this->occupant_a != nullptr) {                     /* +0x8C */
        RESDATA* resource = (RESDATA*)this->occupant_a->resource;
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
    RESDATA* res_ptr = (RESDATA*)this->resource;           /* +0x40 */

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
        uint16_t fh = res_ptr->frame_height;               /* +0x16 */
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
        uint16_t fw = res_ptr->frame_width;                /* +0x14 */
        uint16_t fh = res_ptr->frame_height;               /* +0x16 */
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

    /* Road class stored deep in RESDATA at +0x63A */
    uint8_t road_class = *(uint8_t*)((uint8_t*)resdata + 0x63A);

    /* Horizontal road (class 0x12) */
    if (road_class == 0x12) {
        /* Check for vehicle boarding at tile+0x118 */
        void* vehicle = *(void**)((uint8_t*)tile_obj + 0x118);
        if (vehicle != nullptr) {
            int vs = *(int*)((uint8_t*)vehicle + 0x5C);
            if (vs == 0 || vs == 1) {  /* idle or boarding */
                if (Vehicle_GetOccupantCount(vehicle) != 0) {
                    this->AddOccupant(vehicle);
                    return 1;
                }
            }
        }

        int tile_x = *(int*)((uint8_t*)tile_obj + 0x4C);
        if (this->world_x == tile_x) {
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
                if (Vehicle_GetOccupantCount(vehicle) != 0) {
                    this->AddOccupant(vehicle);
                    return 1;
                }
            }
        }

        int tile_y = *(int*)((uint8_t*)tile_obj + 0x50);
        if (this->world_y == tile_y) {
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
/* Building::MoveToTarget — Move toward target entity (non-virtual)    */
/* Address: 0x434399                                                   */
/*                                                                     */
/* If target is valid and active, moves toward it with optional        */
/* random arrival point within bounding rect.                          */
/* ================================================================== */
void Building::MoveToTarget()
{
    /* Determine target entity from last_action */
    Entity* target = nullptr;
    if (this->last_action == 1) target = this->occupant_a;
    else if (this->last_action == 2) target = this->occupant_b;

    if (target == nullptr || target->initialized != 1 || !g_is_game_active) {
        this->dest_x = -1; this->dest_y = -1;
        if (this->visible) { int out[2]; this->CalcMoveTarget(out, -1, -1, 0); }
        return;
    }

    /* Check for random arrival rect */
    RECT bounds;
    if (!target->GetBoundingRect(&bounds) ||
        *(uint8_t*)(*(uintptr_t*)((uint8_t*)target + 0x40) + 0x62C) == 0) {
        int tx = target->world_x;
        int ty = target->world_y;
        this->TeleportTo(tx, ty);
        return;
    }

    /* Random point within bounding rect */
    this->TeleportTo(randInRange(bounds.left, bounds.right),
                     randInRange(bounds.top, bounds.bottom));
}


/* ================================================================== */
/* Building::HandleAction — Finalize current action at target          */
/* Address: 0x434100                                                   */
/* ================================================================== */
void Building::HandleAction()
{
    if (!this->visible) return;
    if (g_selected_building == this && g_building_animating) return;

    this->field_a4 = 0;
    int action = this->last_action;

    if (action == 1) {
        Entity* ob = this->occupant_a;
        if (ob && this->world_x == ob->world_x &&
            this->world_y == ob->world_y) {
            if (this->occupation_level < 7) this->occupation_level++;
            this->visible = 0;
        }
    } else if (action == 2) {
        Entity* sb = this->occupant_b;
        if (!sb) {
            this->PostMoveDispatch();
            goto epilogue;
        }
        if (this->world_x == sb->world_x &&
            this->world_y == sb->world_y) {
            if (this->occupation_level < 7) this->occupation_level++;
            this->visible = 0;
            this->next_action_time = g_game_time + 0xE10;
            goto epilogue;
        }
    } else if (action == 3) {
        /* Cap occupation_level at 7 — level 6 + 2 would overflow. */
        if (this->occupation_level < 6) {
            this->occupation_level += 2;
        } else {
            this->occupation_level = 7;
        }
        if (g_is_party_mode) goto epilogue;
    } else {
        goto epilogue;
    }

    this->next_action_time = (int)(CRT_rand() % 21) + 10 + g_game_time;
epilogue:
    this->prev_target_x = this->target_x;
    this->prev_target_y = this->target_y;
    this->target_x = -1;
    this->target_y = -1;
}


/* ================================================================== */
/* Building::Deserialize — Reconstruct from save data                  */
/* Address: 0x435700                                                     */
/*                                                                      */
/* Allocates a Building via `new Building(0)`, then overwrites all     */
/* fields from the flat save buffer. The constructor chain ensures     */
/* the vtable pointer at +0x00 is properly initialized.                 */
/*                                                                      */
/* The original binary at 0x435700 is __thiscall on a manager object   */
/* with two stack args (context, src_buffer). The manager's vtable     */
/* [0x28] registers the deserialized entity. Registration is deferred  */
/* to the caller.                                                      */
/*                                                                      */
/* Returns: Pointer to the constructed Building, or nullptr on OOM.    */
/* ================================================================== */
Building* Building::Deserialize(void* data)
{
    uint8_t* src = (uint8_t*)data;

    /* Step 1: Allocate via proper C++ constructor chain.
     * `new Building(0)` calls the full constructor chain
     * (GameObject → Entity → Building), setting up the
     * vtable pointer at +0x00 correctly. Fields are then
     * overwritten from the save buffer. */
    Building* obj = new Building(0);
    if (obj == nullptr) {
        return nullptr;
    }

    /* Step 2a: Overwrite GameObject fields (+0x04..+0x23).
     * +0x00 (vtable) is managed by C++ — skip it.
     * +0x1C and +0x20 are callback function pointers in the
     * serialized format; write them as raw bytes. */
    obj->type                = *(int32_t*) (src + 0x04);  /* +0x04 */
    obj->screen_rect.left    = *(int32_t*) (src + 0x08);  /* +0x08 */
    obj->screen_rect.top     = *(int32_t*) (src + 0x0C);  /* +0x0C */
    obj->screen_rect.right   = *(int32_t*) (src + 0x10);  /* +0x10 */
    obj->screen_rect.bottom  = *(int32_t*) (src + 0x14);  /* +0x14 */
    obj->initialized         = *(uint8_t*) (src + 0x18);  /* +0x18 */

    /* Callback slots at +0x1C/+0x20: write as raw uint32_t.
     * These overlap with callback_1/callback_2 function pointers
     * in GameObject; the save format stores them as POD. */
    *(uint32_t*)((uint8_t*)obj + 0x1C) = *(uint32_t*)(src + 0x1C);
    *(uint32_t*)((uint8_t*)obj + 0x20) = *(uint32_t*)(src + 0x20);

    /* Step 2b: Overwrite Entity fields (+0x24..+0x86) */
    obj->visible            = *(uint8_t*) (src + 0x24);  /* +0x24 */
    obj->anim_index         = *(int32_t*) (src + 0x28);  /* +0x28 */
    obj->blit_flags         = *(uint32_t*)(src + 0x2C);  /* +0x2C */
    obj->source_rect.left   = *(int32_t*) (src + 0x30);
    obj->source_rect.top    = *(int32_t*) (src + 0x34);
    obj->source_rect.right  = *(int32_t*) (src + 0x38);
    obj->source_rect.bottom = *(int32_t*) (src + 0x3C);

    obj->resource           = *(void**)   (src + 0x40);  /* +0x40 */
    obj->sound_res_id       = *(uint32_t*)(src + 0x44);  /* +0x44 */
    obj->audio_channel      = *(void**)   (src + 0x48);  /* +0x48 */
    obj->world_x            = *(int32_t*) (src + 0x4C);  /* +0x4C */
    obj->world_y            = *(int32_t*) (src + 0x50);  /* +0x50 */
    obj->frame_index        = *(int32_t*) (src + 0x54);  /* +0x54 */
    obj->timer              = *(uint32_t*)(src + 0x58);  /* +0x58 */
    obj->active_state       = *(uint32_t*)(src + 0x5C);  /* +0x5C */
    obj->next_sound_time    = *(uint32_t*)(src + 0x60);  /* +0x60 */
    obj->field_64           = *(uint32_t*)(src + 0x64);  /* +0x64 */
    obj->field_68           = *(uint32_t*)(src + 0x68);  /* +0x68 */
    obj->phase_timer        = *(uint32_t*)(src + 0x6C);  /* +0x6C */
    obj->waiting_flag       = *(uint8_t*) (src + 0x70);  /* +0x70 */
    obj->world_x_raw        = *(int32_t*) (src + 0x74);  /* +0x74 */
    obj->world_y_raw        = *(int32_t*) (src + 0x78);  /* +0x78 */

    /* Name field (+0x7C..+0x86, 11 bytes) */
    memcpy(obj->name, src + 0x7C, 11);

    /* Step 2c: Overwrite Building fields (+0x88..+0xF3) */
    obj->occupation_level   = *(uint8_t*) (src + 0x88);  /* +0x88 */
    obj->disabled           = *(uint8_t*) (src + 0x89);  /* +0x89 */
    obj->_pad_8a[0]         = *(uint8_t*) (src + 0x8A);
    obj->_pad_8a[1]         = *(uint8_t*) (src + 0x8B);
    obj->occupant_a         = *(Entity**) (src + 0x8C);  /* +0x8C */
    obj->occupant_b         = *(Entity**) (src + 0x90);  /* +0x90 */
    obj->create_time        = *(uint32_t*)(src + 0x94);  /* +0x94 */
    obj->conn_building_a    = *(int32_t*) (src + 0x98);  /* +0x98 */
    obj->conn_building_b    = *(int32_t*) (src + 0x9C);  /* +0x9C */
    obj->next_action_time   = *(uint32_t*)(src + 0xA0);  /* +0xA0 */
    obj->field_a4           = *(uint32_t*)(src + 0xA4);  /* +0xA4 */
    obj->target_x           = *(int32_t*) (src + 0xA8);  /* +0xA8 */
    obj->target_y           = *(int32_t*) (src + 0xAC);  /* +0xAC */
    obj->search_x1          = *(int32_t*) (src + 0xB0);  /* +0xB0 */
    obj->search_y1          = *(int32_t*) (src + 0xB4);  /* +0xB4 */
    obj->track_x            = *(int32_t*) (src + 0xB8);  /* +0xB8 */
    obj->track_y            = *(int32_t*) (src + 0xBC);  /* +0xBC */
    obj->track_node_id      = *(int32_t*) (src + 0xC0);  /* +0xC0 */
    obj->prev_target_x      = *(int32_t*) (src + 0xC4);  /* +0xC4 */
    obj->prev_target_y      = *(int32_t*) (src + 0xC8);  /* +0xC8 */
    obj->dest_x             = *(int32_t*) (src + 0xCC);  /* +0xCC */
    obj->dest_y             = *(int32_t*) (src + 0xD0);  /* +0xD0 */
    obj->waypoint_x1        = *(int32_t*) (src + 0xD4);  /* +0xD4 */
    obj->waypoint_y1        = *(int32_t*) (src + 0xD8);  /* +0xD8 */
    obj->field_dc           = *(int32_t*) (src + 0xDC);  /* +0xDC */
    obj->field_e0           = *(uint32_t*)(src + 0xE0);  /* +0xE0 */
    obj->field_e4           = *(uint8_t*) (src + 0xE4);  /* +0xE4 */
    memcpy(obj->_pad_e5, src + 0xE5, 3);
    obj->last_action        = *(int32_t*) (src + 0xE8);  /* +0xE8 */
    obj->field_ec           = *(uint32_t*)(src + 0xEC);  /* +0xEC */
    obj->occupant_ptr       = *(Entity**) (src + 0xF0);  /* +0xF0 */

    /* Step 3: Registration is deferred to caller.
     * In the original binary, manager->vtable[0x28](context, obj)
     * registers the deserialized building with the manager.
     * The caller at 0x44DC10 (World_LoadFromFile) handles this. */

    return obj;
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
        this->SetAnimState(new_anim);
    }
}


/* ================================================================== */
/* Virtual method implementations — decompiled from binary             */
/* ================================================================== */

/* ================================================================== */
/* Building::OnOccupantReady — Vtable slot [17] (+0x44)               */
/* Address: 0x434260                                                    */
/*                                                                      */
/* Moves the building toward the given entity.                          */
/*                                                                      */
/* Algorithm:                                                           */
/*   1. If entity is NULL or not initialized or game inactive:          */
/*      set dest = -1, optionally call CalcMoveTarget to cancel         */
/*   2. Get entity's bounding rect; if no rect or +0x62C flag is 0:    */
/*      teleport directly to entity's world pos                         */
/*   3. Otherwise: pick random point within bounding rect, teleport     */
/* ================================================================== */
void Building::OnOccupantReady(Entity* entity)
{
    /* Step 1: Validate entity */
    if (entity == nullptr || entity->initialized != 1 || !g_is_game_active) {
        this->dest_x = -1;                        /* +0xCC */
        this->dest_y = -1;                        /* +0xD0 */
        if (this->visible) {
            int out[2];
            this->CalcMoveTarget(out, -1, -1, 0);
        }
        return;
    }

    /* Step 2: Try to get bounding rect */
    RECT bounds;
    bool has_rect = entity->GetBoundingRect(&bounds);  /* 0x4583C0 */

    if (!has_rect) {
        /* Branch A: No bounding rect — teleport directly */
        int tx = entity->world_x;                   /* +0x4C */
        int ty = entity->world_y;                   /* +0x50 */
        this->TeleportTo(tx, ty);                   /* vtable[0x40] */
        return;
    }

    /* Check the flag at resource + 0x62C */
    RESDATA* res = (RESDATA*)entity->resource;      /* +0x40 */
    uint8_t flag_62c = *(uint8_t*)((uint8_t*)res + 0x62C);

    if (flag_62c == 0) {
        /* Branch B: Flag is 0 — teleport directly */
        int tx = entity->world_x;
        int ty = entity->world_y;
        this->TeleportTo(tx, ty);
        return;
    }

    /* Branch C: Pick random point within bounding rect */
    int rand_x = randInRange(bounds.left, bounds.right);
    int rand_y = randInRange(bounds.top, bounds.bottom);
    this->TeleportTo(rand_x, rand_y);
}


/* ================================================================== */
/* Building::PartyModeUpdate — Vtable slot [23] (+0x5C)               */
/* Address: 0x433220  (192 bytes)                                       */
/*                                                                      */
/* Called from Update every tick when g_is_party_mode is active.       */
/* Increments field_e4 each call. When it reaches 3: sets visible=1,   */
/* checks connection buildings for the next party destination.          */
/*                                                                      */
/* NOTE: Parameter is void* to match base class vtable signature.      */
/* BuildingMgr passes a Building* in practice.                          */
/*                                                                      */
/* Algorithm:                                                           */
/*   1. Increment field_e4 (+0xE4)                                      */
/*   2. If < 3 or occupant_ptr != NULL: return (still animating)        */
/*   3. At 3: set visible=1, reset field_e4                             */
/*   4. If conn_building_a and conn_building_b are both -1: disable     */
/*      this building (disabled=1, visible=0)                           */
/*   5. Otherwise: continue party; next update will handle routing      */
/* ================================================================== */
void Building::PartyModeUpdate(void* next_entity)
{
    /* Increment frame counter */
    this->field_e4++;                               /* +0xE4 */

    /* Still counting up — return early if < 3 or we have an occupant */
    if (this->field_e4 < 3 || this->occupant_ptr != nullptr) {
        return;
    }

    /* Reached frame 3: make visible, reset counter */
    this->field_e4 = 0;                             /* +0xE4 */
    this->visible = 1;                              /* +0x24 */

    /* Check connection buildings on the next entity */
    Building* next_bldg = (Building*)next_entity;
    if (next_bldg != nullptr) {
        if (next_bldg->conn_building_a == -1 && next_bldg->conn_building_b == -1) {
            /* No connections — disable this building */
            this->disabled = 1;                     /* +0x89 */
            this->visible = 0;                      /* +0x24 */
        }
    }
}


/* ================================================================== */
/* Building::IsActionComplete — Vtable slot [22] (+0x58)              */
/* Address: 0x432FD0  (400 bytes)                                       */
/*                                                                      */
/* Checks if the current movement action has finished. This is polled   */
/* every tick while field_dc != 0 (action in progress).                 */
/*                                                                      */
/* Algorithm:                                                           */
/*   If occupant_ptr is non-null:                                       */
/*     Calls CalcMoveTarget and CheckPlacementCollision(vtable[0x54])  */
/*     Returns whether collision check passed (1=blocked, 0=clear)     */
/*   Otherwise:                                                         */
/*     Increments field_e4; if it reaches the resource's anim duration */
/*     (at +0x169), resets field_e4, calls CalcMoveTarget, then         */
/*     CheckPlacementCollision. Returns 0 if clear, 1 if blocked.       */
/*     Returns 1 (still in progress) if frame counter is below limit.   */
/* ================================================================== */
int Building::IsActionComplete()
{
    if (this->occupant_ptr != nullptr) {
        /* Occupant present — compute step toward dest */
        int out[2];
        this->CalcMoveTarget(out, this->dest_x, this->dest_y,
            *(uint8_t*)((uint8_t*)this->resource + 0x168));

        int step_x = out[0];
        int step_y = out[1];

        /* Call CheckPlacementCollision via vtable[0x54] */
        uint8_t blocked = this->CheckPlacementCollision(step_x, step_y);
        return (blocked != 1) ? 0 : 1;
    }

    /* No occupant — increment frame counter */
    this->field_e4++;                               /* +0xE4 */

    if (!this->visible) {
        return 1;  /* not visible = action still in progress */
    }

    /* Check against resource's animation duration at +0x169 */
    RESDATA* res = (RESDATA*)this->resource;        /* +0x40 */
    uint8_t anim_limit = *(uint8_t*)((uint8_t*)res + 0x169);

    if (this->field_e4 < anim_limit) {
        return 1;  /* still counting frames */
    }

    /* Reached the animation limit — compute step and check placement */
    this->field_e4 = 0;                             /* +0xE4 */

    int out[2];
    this->CalcMoveTarget(out, this->dest_x, this->dest_y,
        *(uint8_t*)((uint8_t*)res + 0x168));

    uint8_t blocked = this->CheckPlacementCollision(out[0], out[1]);
    return (blocked != 1) ? 0 : 1;
}


/* ================================================================== */
/* Building::StepToward — Vtable slot [18] (+0x48)                    */
/* Address: 0x432AE0  (1209 bytes in binary)                            */
/*                                                                      */
/* Advances dest_x/dest_y one step along the road network toward the   */
/* target position (x, y). This is the core path-following function     */
/* used by AddOccupant (road validation), RemoveOccupant (refresh),     */
/* TeleportTo (initial step), and Update (action progress).             */
/*                                                                      */
/* Summary of control flow (from disassembly at 0x432AE0):              */
/*                                                                      */
/*   1. Convert (x,y) to tile coordinates (>> 4, clamp negative to -1)  */
/*   2. Look up tile at those coords via TileMap_GetObjectAt             */
/*   3. Get tile type from tile's resource at +0x08                     */
/*   4. Multi-way branch on tile type:                                  */
/*      a. Type 0x0C (walkable): path-distance + road-tile search,      */
/*         advance dest toward connecting tile                          */
/*      b. Type 3 (road): if no occupant, same as 0x0C; otherwise       */
/*         different path (occupant-relative stepping)                  */
/*      c. Type 0x12 / 0x13 (horizontal/vertical road): additional      */
/*         lane-offset computation for road-edge positioning            */
/*      d. Various connection/junction tile types: connection node      */
/*         traversal and direction selection                            */
/*      e. Other types: fallback direct advance toward (x,y)            */
/*   5. Always updates dest_x/dest_y by ±4 pixels toward target        */
/*                                                                      */
/* DECOMPILER NOTE: The current C++ implementation covers branches      */
/* (a), (b), and (e). Branches (c) and (d) — the tile-type-specific    */
/* lane offset and connection-node logic accounting for approximately   */
/* 60% of the binary's 1209 bytes — are approximated. Full VALIDATION   */
/* requires decompilation of all branch cases from the disassembly.     */
/*                                                                      */
/* Called helpers:                                                      */
/*   TileMap_PathDistance    (0x45c7a0) — tile-path distance compute    */
/*   TileMap_FindTileByType  (0x457ce0) — tile search by type          */
/* ================================================================== */
void Building::StepToward(int x, int y)
{
    /* --- Step 1: Convert target to tile coordinates --- */
    int tile_x = (x < 0) ? -1 : (x >> 4);
    int tile_y = (y < 0) ? -1 : (y >> 4);

    /* --- Step 2: Look up tile at target tile position --- */
    void* tile = TileMap_GetObjectAt(g_tilemap, tile_x, tile_y, 0);  /* 0x455620 */

    uint16_t tile_type = 0;
    if (tile != nullptr) {
        RESDATA* tile_res = *(RESDATA**)((uint8_t*)tile + 0x40);
        tile_type = (tile_res != nullptr) ? *(uint8_t*)((uint8_t*)tile_res + 0x08) : 0;
    }

    /* --- Step 3: Branch on tile type ---
     *
     * Type 0x0C (walkable surface) or Type 3 (road) with no occupant:
     *   Use path-finding to locate a connecting road tile (type 0x30),
     *   update search position, and step toward it.
     *
     * The binary at 0x432B50–0x432D30 handles additional tile types:
     *   - 0x12 / 0x13 (horizontal/vertical road): lane-offset positioning
     *   - 0xC6C / 0xC6E (rail tiles): block passage
     *   - Connection node tiles: direction selection and node traversal
     *
     * These additional branches are documented above and tracked for
     * full decompilation in PROGRESS.md.                              */
    if (tile_type == 0x0C || (tile_type == 3 && this->occupant_ptr == nullptr)) {
        /* Compute path distance from target to current via world pos.
         * Result is stored but the return value is used in the binary
         * for connection-node ranking in branch (d).                 */
        int path_dist = TileMap_PathDistance(g_tilemap,
            this->target_x, this->target_y, x, y);

        /* Search for a connecting road tile of type 0x30 near target. */
        void* road_tile = TileMap_FindTileByType(g_tilemap, x, y, 0x0C, 0x30);

        if (road_tile != nullptr) {
            /* Found a connecting road tile — update search position.
             * Read the tile's world position at +0x4C/+0x50.         */
            this->search_x1 = *(int32_t*)((uint8_t*)road_tile + 0x4C);  /* +0xB0 */
            this->search_y1 = *(int32_t*)((uint8_t*)road_tile + 0x50);  /* +0xB4 */

            /* Advance dest toward the road tile position by ±4 pixels.
             * This is a fixed-step movement; the binary may use a
             * variable step based on road type for lane positioning. */
            int road_x = this->search_x1;
            int road_y = this->search_y1;
            if (this->dest_x < road_x) this->dest_x += 4;
            else if (this->dest_x > road_x) this->dest_x -= 4;
            if (this->dest_y < road_y) this->dest_y += 4;
            else if (this->dest_y > road_y) this->dest_y -= 4;
            return;
        }
    }

    /* --- Step 4: Fallback — advance dest directly toward (x,y) ---
     * Move ±4 pixels in each axis, clamping toward the target.
     * Used when no road tile is found or tile type doesn't match
     * the path-following branches.                                  */
    if (this->dest_x < x) {
        this->dest_x += 4;                          /* +0xCC */
    } else if (this->dest_x > x) {
        this->dest_x -= 4;
    }
    if (this->dest_y < y) {
        this->dest_y += 4;                          /* +0xD0 */
    } else if (this->dest_y > y) {
        this->dest_y -= 4;
    }
}


/* ================================================================== */
/* Building::TeleportTo — Vtable slot [16] (+0x40)                    */
/* Address: 0x432940  (404 bytes)                                       */
/*                                                                      */
/* Sets the movement target and begins path-following.                   */
/*                                                                      */
/* Algorithm:                                                           */
/*   1. Set action timeout (+0xA4) to g_game_time                       */
/*   2. If (x, y) == (-1, -1): cancel movement via CalcMoveTarget       */
/*   3. Otherwise: save prev_target, set new target                     */
/*   4. Search for road tile near target via TileMap functions          */
/*   5. If occupant_ptr != NULL: call AddOccupant to transfer occupant  */
/*   6. Otherwise: call StepToward(current position) to begin moving    */
/* ================================================================== */
void Building::TeleportTo(int x, int y)
{
    /* Set action timeout to current game time */
    this->field_a4 = g_game_time;                   /* +0xA4 */

    /* Cancel movement if both coords are -1 */
    if (x == -1 && y == -1) {
        if (this->occupant_ptr == nullptr) {
            /* No occupant — just stop moving */
            int out[2];
            this->CalcMoveTarget(out, -1, -1, 0);
        }
        return;
    }

    /* Only proceed if game is active */
    if (!g_is_game_active) {
        return;
    }

    /* Check if target is changing */
    bool target_changed = (this->target_x != x || this->target_y != y);

    /* Save previous target and set new one */
    if (target_changed) {
        this->prev_target_x = this->target_x;       /* +0xC4 */
        this->prev_target_y = this->target_y;       /* +0xC8 */
        this->target_x      = x;                    /* +0xA8 */
        this->target_y      = y;                    /* +0xAC */
    }

    /* Search for a road tile near the target */
    void* road_tile = TileMap_FindTileByType(g_tilemap, x, y, 0x0C, 0x900);

    if (road_tile != nullptr) {
        /* Store road tile position in track-follow fields */
        this->track_x      = *(int32_t*)((uint8_t*)road_tile + 0x4C);  /* +0xB8 */
        this->track_y      = *(int32_t*)((uint8_t*)road_tile + 0x50);  /* +0xBC */

        /* Get connection node ID from the tile's +0xE4 field */
        int32_t conn_node = *(int32_t*)((uint8_t*)road_tile + 0xE4);
        if (conn_node != -1) {
            this->track_node_id = conn_node;        /* +0xC0 */
        } else {
            /* Scan the tile's 4 connection slots at +0xD8 for a valid node */
            int best_slot = 0;
            uint32_t best_count = 0;
            for (int slot = 0; slot < 4; slot++) {
                uint32_t count = *(uint32_t*)((uint8_t*)road_tile + 0xD8 + slot * 4);
                if (count > best_count) {
                    best_count = count;
                    best_slot = slot;
                }
            }
            void* conn_slot_obj = *(void**)((uint8_t*)road_tile + 0xC4 + best_slot * 4);
            if (conn_slot_obj != nullptr) {
                this->track_node_id = *(int32_t*)((uint8_t*)conn_slot_obj + 0xE4);
            } else {
                this->track_node_id = 0xFF;
            }
        }
    }

    /* Transfer occupant or step toward current position */
    if (this->occupant_ptr != nullptr) {
        this->AddOccupant(this->occupant_ptr);
    } else {
        /* Step toward current world position to trigger display update */
        this->StepToward(this->world_x, this->world_y);  /* vtable[0x48] */
    }
}


/* ================================================================== */
/* Building::PostMoveDispatch — Vtable slot [20] (+0x50)              */
/* Address: 0x433CA0  (247 bytes)                                       */
/*                                                                      */
/* Detaches from the scene-graph parent (occupant_b at +0x90), then    */
/* searches the building collection for a compatible building to       */
/* re-attach to.                                                        */
/*                                                                      */
/* Algorithm:                                                           */
/*   1. If occupant_b (+0x90) is set: clear `this` from parent's       */
/*      5-slot child array at parent+0xA4, decrement child_count       */
/*   2. Iterate all buildings in g_building_mgr collection:             */
/*      find one whose occupant_b is NULL and whose sub-type matches    */
/*   3. When found: add `this` to that building's child array,          */
/*      increment its child_count, set this->occupant_b = building      */
/* ================================================================== */
void Building::PostMoveDispatch()
{
    /* Step 1: Detach from current scene-graph parent (occupant_b) */
    if (this->occupant_b != nullptr) {
        Entity* parent = this->occupant_b;          /* +0x90 */

        /* Search parent's 5-slot child array at parent+0xA4 for `this` */
        Entity** child_array = (Entity**)((uint8_t*)parent + 0xA4);
        for (int slot = 0; slot < 5; slot++) {
            if (child_array[slot] == this) {
                child_array[slot] = nullptr;
                /* Decrement parent's child_count at parent+0x8E */
                uint8_t* child_count = (uint8_t*)parent + 0x8E;
                *child_count = *child_count - 1;
                break;
            }
        }
        this->occupant_b = nullptr;                 /* +0x90 */
    }

    /* Step 2: Search building collection for a compatible host.
     * Address: 0x433D98
     *
     * Iterates all buildings in g_building_mgr->buildings collection,
     * looking for one whose occupant_b (+0x90) is NULL and whose
     * resource sub-type matches ours. When found, adds `this` to the
     * matching building's 5-slot child array at host+0xA4, increments
     * host's child_count at host+0x8E, and sets this->occupant_b = host.
     *
     * DECOMPILER NOTE: The binary calls g_building_mgr->vtable[0x20]
     * for iteration; we use the BuildingCollection API directly. */
    if (g_building_mgr != nullptr && this->resource != nullptr) {
        RESDATA* my_res = (RESDATA*)this->resource;     /* +0x40 */
        uint8_t my_sub_type = *(uint8_t*)((uint8_t*)my_res + 0x08);

        BuildingCollection* collection = &g_building_mgr->buildings;
        uint32_t count = collection->GetCount();

        for (uint32_t i = 0; i < count; i++) {
            Building* candidate = collection->GetItem(i);
            if (candidate == nullptr || candidate == this) continue;

            /* Check candidate has no occupant in its occupant_b slot */
            if (candidate->occupant_b != nullptr) continue;

            /* Check sub-type compatibility */
            RESDATA* cand_res = (RESDATA*)candidate->resource;
            if (cand_res == nullptr) continue;
            uint8_t cand_sub_type = *(uint8_t*)((uint8_t*)cand_res + 0x08);
            if (cand_sub_type != my_sub_type) continue;

            /* Compatible host found: add `this` to host's child array.
             * Host child array is at host+0xA4 (5 slots of Entity*). */
            Entity** child_array = (Entity**)((uint8_t*)candidate + 0xA4);
            int slot;
            for (slot = 0; slot < 5; slot++) {
                if (child_array[slot] == nullptr) {
                    child_array[slot] = this;
                    break;
                }
            }

            if (slot < 5) {
                /* Increment host's child count at host+0x8E */
                uint8_t* child_count = (uint8_t*)candidate + 0x8E;
                *child_count = *child_count + 1;

                /* Set our occupant_b to point to the host */
                this->occupant_b = candidate;
            }
            break;
        }
    }
}


/* ================================================================== */
/* Building::CheckPlacementCollision — Vtable slot [21] (+0x54)       */
/* Address: 0x433860  (428 bytes)                                       */
/*                                                                      */
/* Checks whether placing this building at (x, y) would collide with   */
/* something. Returns 1 if blocked, 0 if placement is allowed.          */
/*                                                                      */
/* Algorithm:                                                           */
/*   1. If (x, y) matches (dest_x, dest_y) AND (search_x1, search_y1): */
/*      placement is at current position — allowed                      */
/*   2. If (x, y) matches (target_x, target_y) AND (dest_x, dest_y):   */
/*      placement is at target — allowed                                */
/*   3. If this == g_selected_building and g_building_animating:        */
/*      allow placement                                                 */
/*   4. Otherwise: check building dimensions against known obstacles    */
/* ================================================================== */
uint8_t Building::CheckPlacementCollision(int x, int y)
{
    /* Check 1: Already at destination — no collision */
    if (x == this->dest_x && y == this->dest_y &&
        this->search_x1 == x && this->search_y1 == y) {
        return 0;  /* allowed */
    }

    /* Check 2: At target position — no collision */
    if (x == this->target_x && y == this->target_y &&
        this->dest_x == x && this->dest_y == y) {
        /* At target and dest matches — verify against g_selected_building */
        if (this == g_selected_building && g_building_animating) {
            return 1;  /* blocked — selected & animating */
        }
        return 0;
    }

    /* Check 3: Global collision check against selection state */
    if (this == g_selected_building && g_building_animating) {
        return 0;  /* allow placement while editing/animating */
    }

    /* Check 4: Validate building footprint against map tiles.
     * Address: 0x433890 (tile-obstacle validation loop)
     *
     * Converts the placement position (x, y) to tile coordinates,
     * then iterates over the building's footprint defined by
     * frame_width and frame_height from the RESDATA resource.
     * Each tile within the footprint is checked via TileMap_GetObjectAt
     * for map obstacles. Returns 1 (blocked) if any tile is occupied
     * or blocked; returns 0 if all tiles are clear. */
    RESDATA* res = (RESDATA*)this->resource;        /* +0x40 */
    if (res != nullptr) {
        uint16_t fw = res->frame_width;             /* +0x14 */
        uint16_t fh = res->frame_height;            /* +0x16 */

        int base_tx = worldToTile(x);
        int base_ty = worldToTile(y);

        /* Compute tile footprint: frame dimensions in tiles.
         * The binary divides width/height by 16 (>> 4) to get tile count. */
        int tiles_wide = (int)fw >> 4;
        int tiles_high = (int)fh >> 4;
        if (tiles_wide < 1) tiles_wide = 1;
        if (tiles_high < 1) tiles_high = 1;

        /* Iterate over every tile position in the footprint */
        for (int ty = 0; ty < tiles_high; ty++) {
            for (int tx = 0; tx < tiles_wide; tx++) {
                int tile_x = base_tx + tx;
                int tile_y = base_ty + ty;

                if (tile_x < 0 || tile_y < 0) continue;

                void* tile_obj = TileMap_GetObjectAt(g_tilemap, tile_x, tile_y, 0);
                if (tile_obj != nullptr) {
                    /* Check if tile is occupied by another building or obstacle.
                     * The binary checks tile->resource type and occupancy flags.
                     * A non-null tile with certain type flags means blocked. */
                    RESDATA* tile_res = *(RESDATA**)((uint8_t*)tile_obj + 0x40);
                    if (tile_res != nullptr) {
                        uint8_t tile_type = *(uint8_t*)((uint8_t*)tile_res + 0x08);
                        /* Type 0x01 = building/obstacle, blocks placement */
                        if (tile_type == 0x01) {
                            return 1;  /* blocked */
                        }
                    }
                }
            }
        }

        return 0;  /* all tiles clear */
    }

    return 1;  /* blocked by default */
}


/* ================================================================== */
/* Building::FindNearestConnectionNode — Vtable slot [19] (+0x4C)     */
/* Address: 0x4343F0  (162 bytes)                                       */
/*                                                                      */
/* Iterates the node set, computing Manhattan distance from each node  */
/* to the building's target position. Returns the index of the closest  */
/* reachable node.                                                      */
/*                                                                      */
/* node_set structure:                                                  */
/*   +0x00: uint32_t count                                              */
/*   +0x04: padding?                                                    */
/*   +0x08: Entity** nodes[] (array of entity pointers)                 */
/*                                                                      */
/* The binary calls 0x45c7a0 to compute distance, then calls            */
/* 0x45dd80 to get each node's connection flag. Skips nodes with        */
/* flag 0x80 or 0xFF. Returns index of node with minimum distance.      */
/* ================================================================== */
uint32_t Building::FindNearestConnectionNode(void* node_set, uint32_t current_node_id)
{
    /* Compute baseline distance from target to world position */
    int best_dist = TileMap_PathDistance(g_tilemap,
        this->world_x, this->world_y, this->target_x, this->target_y);

    /* Get node count from node_set */
    uint32_t count = *(uint32_t*)node_set;          /* +0x00 */
    uint32_t best_idx = current_node_id;

    /* node array at node_set + 0x08 */
    void** node_array = (void**)((uint8_t*)node_set + 0x08);

    for (uint32_t i = 0; i < count; i++) {
        /* Get connection flag for this node via 0x45dd80 */
        uint8_t flag = NodeSet_GetConnectionFlag(node_set, i);

        /* Skip blocked nodes (flag 0x80 or 0xFF) */
        if (flag == 0x80 || flag == 0xFF) {
            continue;
        }

        /* Get this node's entity and its position */
        Entity* node_entity = (Entity*)node_array[i];
        if (node_entity == nullptr) continue;

        int node_dist = TileMap_PathDistance(g_tilemap,
            this->target_x, this->target_y,
            node_entity->world_x, node_entity->world_y);

        if (node_dist < best_dist) {
            best_dist = node_dist;
            best_idx = i;
        }
    }

    return best_idx;
}


/* ================================================================== */
/* Building::SetCustomName — Non-virtual name setter                   */
/* Delegates to virtual SetName which contains the full implementation */
/* ================================================================== */
void Building::SetCustomName(const char* name)
{
    this->SetName(name);
}


/* ================================================================== */
/* Building::SetName — Vtable slot [13] override (0x4344A0)           */
/* Address: 0x4344A0  (80 bytes)                                        */
/*                                                                      */
/* Algorithm:                                                           */
/*   1. Call Entity::SetName(name) at 0x405E20                          */
/*   2. If sub-type == STATION (7), compact BuildingMgr collections     */
/*   3. If name does NOT contain "PARTY", activate party mode           */
/* ================================================================== */
void Building::SetName(const char* name)
{
    /* Step 1: Delegate to Entity's SetName */
    Entity::SetName(name);                          /* 0x405E20 */

    /* Step 2: Check if it's a station */
    RESDATA* res = (RESDATA*)this->resource;        /* +0x40 */
    if (res != nullptr) {
        uint8_t sub_type = *(uint8_t*)((uint8_t*)res + 0x08);
        if (sub_type == SUBTYPE_STATION) {
            BuildingMgr_CompactCollections(g_building_mgr);  /* 0x434870 */
        }
    }

    /* Step 3: PARTY mode trigger — DECOMPILER NOTE:
     * Party mode activates when the name does NOT contain "PARTY"
     * (inverted check, binary-correct per Ghidra). Party mode
     * appears to be the default; naming a building "PARTY" disables it. */
    if (CRT_wcsstr((const wchar_t*)name, PARTY_STRING) == 0) {
        g_is_party_mode    = 1;
        g_party_start_time = g_game_time;
    }
}


/* ================================================================== */
/* Building::Draw — Vtable slot [11] override                           */
/* Address: 0x4343B0  (46 bytes)                                        */
/*                                                                      */
/* Forwards to Entity::Draw(RECT, int, uint32_t). The binary unpacks   */
/* the RECT from the stack (16 bytes call-by-value), then calls        */
/* Entity::Draw at 0x405E60.                                            */
/* ================================================================== */
void Building::Draw(RECT clip_bounds, int enable_scroll, uint32_t extra_flags)
{
    Entity::Draw(clip_bounds, enable_scroll, extra_flags);
}
