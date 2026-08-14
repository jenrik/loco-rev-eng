// Status: TRANSCRIBED (new virtual method implementations from disassembly; needs disassembly-line validation)
/**
 * Building.cpp — Building game object class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "Building.h"
#include "BuildingMgr.h"
#include "Vehicle.h"
#include "../core/GameObject.h"
#include "../core/BuildingMgrObjectGroup.h"
#include "ResdataGameVehicle.h"
#include "../world/tilemap.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

/* resources/AssetMgr.h is deliberately NOT included: it re-declares
 * operator_new/GLOBAL_free as extern "C" (this file needs the ordinary
 * C++-linkage forms already declared below) and re-declares CRT_wcsstr
 * with yet another incompatible shape (uint32_t(uint8_t*,uint8_t*) —
 * one more entry in this tree's already-tracked CRT_wcsstr landmine
 * cluster, out of scope to fully resolve here). Only what's needed —
 * the AssetMgr type name and AssetMgr_ReadPairValue's real signature —
 * is forward-declared locally instead. */
struct AssetMgr;

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
        return (w == 1) ? lo : static_cast<int>(CRT_rand() % w) + lo;
    } else {
        int w = lo - hi + 1;
        return (w == 1) ? hi : static_cast<int>(CRT_rand() % w) + hi;
    }
}

/* Forward-declared types used in externs */
class BuildingMgr;
class InputMgr;
/* class TileMap and g_tilemap/TileMap_GetObjectAt/TileMap_FindNearestObject
 * now come from world/tilemap.h (included above). Building.cpp previously
 * declared its own extern void* TileMap_GetObjectAt(TileMap*, int, int, int)
 * and TileMap_FindTileByType(void*, int, int, int, int) — both wrong-
 * signature landmines (docs/landmine-sweep-worklist.md "TileMap_GetObjectAt
 * (cluster A)" / "TileMap_FindTileByType"): the real functions are
 * TileMap_GetObjectAt(TileMap*, short, short, short) and
 * TileMap::FindNearestObject(unsigned short, int, int, int) (0x457CE0,
 * exposed as TileMap_FindNearestObject), both inline in world/tilemap.h.
 * Neither the wrong `int` params nor the wrong name/param-order matched
 * the real mangled symbols, so every call in this file was a silent
 * call-0 (ignore-all unresolved-symbols) prior to this fix. */

/* Heap allocation */
extern void* operator_new(size_t size);                                  /* 0x465CE0 */

/* Globals — declared with C++ linkage; do NOT re-declare with C linkage. */
extern BuildingMgr* g_building_mgr;         /* 0x485448 — BuildingMgr singleton */
extern Entity*      g_selected_building;    /* currently selected building (pointer) */
extern void         GLOBAL_free(void* ptr);
extern InputMgr     g_input_mgr;            /* 0x4A9990 — static object */
extern void*        INPUT_FindObjectAt(InputMgr* mgr, int mode);
extern uint8_t      g_building_animating;
extern void*        CRT_localtime(const time_t* timer);
extern int          Game_CheckTimeInRange(int* current_time, int* start, int* end);  /* 0x40CA40 */
extern uint8_t      g_is_game_active;

/* GameObject destructor body (0x405870) — called by BaseCleanup */
extern void GameObject_DtorBody(void* obj);

/* Game selection helper (0x4113A0) */
extern void Game_SelectGameObject(void* game, void* obj);

/* Game singleton — used for selection/deselection */
extern void* g_game;

/* Math helpers — used by StepToward, FindNearestConnectionNode */
extern int      Math_DistSquared(int x1, int y1, int x2, int y2);                     /* 0x45C7A0 */
extern uint8_t  Math_PointOnLineSegment(int px, int py, int ax, int ay, int bx, int by); /* 0x45C7C0 */

/* Asset manager — used by StepToward, FindNearestConnectionNode.
 * g_asset_mgr is declared void* almost everywhere in this tree (matching
 * the original C-style opaque handle); AssetMgr_ReadPairValue takes a
 * typed AssetMgr* first arg, so callers here static_cast at the call site
 * rather than widen the global's declared type. Building.cpp previously
 * declared this extern with a `void*` first param — a landmine (worklist
 * "AssetMgr_ReadPairValue", callers Building::StepToward /
 * Building::FindNearestConnectionNode): the real symbol takes AssetMgr*,
 * so every call here was silently unresolved (call 0).
 *
 * 2026-08-09: the real implementation is now AssetMgr::ReadPairValue (a
 * genuine __thiscall method, resources/AssetMgr.h/.cpp); the declaration
 * below is a free-function compatibility shim over it, kept specifically
 * so this file doesn't have to include resources/AssetMgr.h (see that
 * header's own comment on the shim for why). */
extern void*    g_asset_mgr;                                                       /* asset manager singleton */
extern uint8_t  AssetMgr_ReadPairValue(AssetMgr* self, uint32_t a, uint32_t b);    /* 0x45DD80 */

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
    /* Delegate all initialization to the shared BaseCtor.
     * base_only=false: full Building initialization including
     * occupant_ptr at +0xF0.                                        */
    this->BaseCtor(resource_id, false);         /* +0x433A20 */

    /* Ensure occupation level (redundant with BaseCtor but matches
     * the binary which also writes it twice).                       */
    this->occupation_level = 4;                 /* +0x88 */
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
    /* Delegate to the single shared BaseCtor at 0x433A20.
     * When base_only=true (Train subclass): skips occupant_ptr at +0xF0
     * since Train is only 0xF0 bytes and doesn't have that field.   */
    this->BaseCtor(resource_id, base_only);
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
void Building::BaseCtor(int resource_id, bool base_only)
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

    /* --- Step 4: Get resource pointer ---
     * this->parent is the Entity*-typed alias of the +0x40 union slot;
     * during construction it actually holds the RESDATA* set by InitBase
     * just above, so this is a same-address reinterpretation of an
     * unrelated pointer type, not a real Entity — reinterpret_cast. */
    RESDATA* resource = reinterpret_cast<RESDATA*>(this->parent);  /* +0x40 (resource stored in parent slot before overridden) */
    /* NOTE: In the original binary, the resource pointer is stored at +0x40.
     * Our Entity class uses 'parent' at +0x40 for scene graph, but during
     * construction, +0x40 holds the RESDATA* before it's replaced. */

    /* --- Step 5: Store the resource ID ---
     * Binary 0x433A20: *(int*)(this+0x64) = param_1 (resource_id). */
    this->stored_resource_id = resource_id;  /* +0x64 */

    /* --- Step 6: Zero / -1 initialize all remaining fields --- */
    this->action_cooldown_time = 0;         /* +0x68 */
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

    /* --- Step 6a: When base_only, skip occupant_ptr init.
     * The Train subclass is only 0xF0 bytes and does not have
     * the occupant_ptr field at +0xF0.                              */
    if (!base_only) {
        this->occupant_ptr = nullptr;       /* +0xF0 */
    }

    /* --- Step 7: Set the building's name --- */
    if (resource != nullptr) {
        /* +0x14D is inside the opaque, undocumented "ChildWindow-family"
         * base region of input/BuildingDescriptorEditor.h (a Building's
         * resource is that 0x630-byte descriptor, confirmed via
         * resources/ResourceManager.cpp's operator_new(0x630)+INPUT_ExitGame
         * allocation for odd-id types 0/2/4 and 12/13, and via
         * resources/ResourceManager.h's own comment "Building resources
         * extend this with schedule data at +0x534/+0x548" — see the
         * +0x534/+0x548 TrackPos use in DecideAction below). No specific
         * field name is evidenced yet for +0x14D within that class, so
         * this stays a raw offset (reinterpret_cast, not a named field). */
        char* res_name = reinterpret_cast<char*>(resource) + RESNAME_OFFSET;  /* +0x14D */

        if (*res_name == '\0') {
            /* Branch A: No resource-provided name.
             * Generate a random name from the game's String Table.
             * Building class at resource+RESCLASS_OFFSET determines pool:
             *   'M' (0x4D) = Residential → 50 names (IDs 2..51)
             *   otherwise   = Commercial  → 11 names (IDs 51..61)
             * Ghidra-confirmed (0x433A20): the binary genuinely reads a
             * 4-byte int at +0x170 and compares to 0x4D, not a single
             * byte — preserved as-is, not a truncation bug. */
            UINT name_id;
            uint32_t rand_val = CRT_rand();                     /* +0x466150 */

            if (*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(resource) + RESCLASS_OFFSET) == 0x4D) {
                /* Residential: "M" = Minifigure house */
                name_id = (rand_val % RESIDENTIAL_NAME_COUNT) + RESIDENTIAL_NAME_BASE;
            } else {
                /* Commercial / Industrial */
                name_id = (rand_val % COMMERCIAL_NAME_COUNT) + COMMERCIAL_NAME_BASE;
            }

            /* Load name from executable's string table.
             * hInstance is at g_main_window + 0x0C */
            HINSTANCE hInst = *reinterpret_cast<HINSTANCE*>(reinterpret_cast<uint8_t*>(g_main_window) + 0x0C);
            LoadStringA(hInst, name_id, this->name, 10);
        } else {
            /* Branch B: Resource has a custom name.
             * Copy it with validation via CopyName.                      */
            this->CopyName(res_name);                           /* +0x405E20 */

            /* Check building sub-type/object_type at resource+0x08
             * (RESDATA::object_type — see shared/types.h).
             * If type == STATION (7), compact BuildingMgr collections.   */
            uint8_t sub_type = resource->object_type;
            if (sub_type == SUBTYPE_STATION) {
                g_building_mgr->CompactCollections();  /* +0x434870 */
            }

            /* PARTY mode trigger — DECOMPILER NOTE:
             * The check is inverted: party mode activates when the
             * resource name does NOT contain "PARTY". This is binary-
             * correct behavior (confirmed: wcsstr returns NULL → jz
             * taken → activate party). Party mode appears to be the
             * default state; naming a building "PARTY" disables it.      */
            if (CRT_wcsstr(reinterpret_cast<const wchar_t*>(res_name), PARTY_STRING) == nullptr) {
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
    /* --- Step 1: Read scene-graph parent from +0x90 (occupant_b slot) ---
     * Ghidra-confirmed (0x433BE0 decompile): the "parent"'s +0xA4 read is
     * a 5-slot pointer array and its +0x8E is a decremented byte counter —
     * this EXACTLY matches core/BuildingMgrObjectGroup.h's ResourceGameObject
     * (linked_objects[5] @ +0xA4, group_flag @ +0x8E), not a generic Entity.
     * occupant_b is declared Entity* because ResourceGameObject IS-A Entity;
     * downcast to reach the group-specific fields. */
    ResourceGameObject* scene_parent = static_cast<ResourceGameObject*>(this->occupant_b);

    if (scene_parent != nullptr) {
        /* --- Step 2: Search parent's 5-slot child array at parent+0xA4 ---
         * Each slot holds a pointer to a child Building. Find and clear ours.*/
        int slot;
        for (slot = 0; slot < 5; slot++) {
            if (scene_parent->linked_objects[slot] == this) {
                scene_parent->linked_objects[slot] = nullptr;
                break;
            }
        }

        /* --- Step 3: Decrement parent's group_flag (child count) at +0x8E ---
         * If our pointer wasn't found in the 5-slot array (slot >= 5),
         * skip the decrement and just null out the parent link. */
        if (slot < 5) {
            scene_parent->group_flag = static_cast<uint8_t>(scene_parent->group_flag - 1);
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
void Building::Update(void* next_entity)
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
     * Binary: if (g_is_party_mode && param_1 != 0) → PartyModeUpdate(param_1).
     * When next_entity is null, falls through to normal AI even in party mode. */
    if (g_is_party_mode != 0 && next_entity != nullptr) {   /* 0x48548C */
        this->PartyModeUpdate(next_entity);
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
                void* found = INPUT_FindObjectAt(&g_input_mgr, 2);

                if (found != nullptr) {
                    GameObject* found_obj = static_cast<GameObject*>(found);
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
                this->HandleAction(this->last_action);      /* +0x434100 */
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
     *     buildings it's currently occupying.
     * NOTE: Entity's own declared layout has source_rect.right at +0x38
     * (core/Entity.h), not an array — this raw offset only makes sense
     * for whatever concrete occupant type is actually passed at each call
     * site (e.g. game/Vehicle.h's Vehicle::occupant_tracks[8] @ +0x38 for
     * the Vehicle occupant passed from FindPathToTarget below). Kept as a
     * raw reinterpret_cast rather than retyping the Entity* parameter,
     * matching the existing duck-typed convention documented throughout
     * this file (Entity* is the common calling-convention type; the real
     * runtime layout varies by caller). --- */
    Entity** occupant_slots = reinterpret_cast<Entity**>(reinterpret_cast<uint8_t*>(entity) + 0x38);

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
    RESDATA* model_data = *reinterpret_cast<RESDATA**>(reinterpret_cast<uint8_t*>(entity) + 0x20);
    if (model_data == nullptr) return;

    void* entry_data = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(model_data) + 0x14);
    if (entry_data == nullptr) return;

    int entry_x = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(entry_data) + 0x4C);
    int entry_y = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(entry_data) + 0x50);
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

        ResourceGameObject* tile = static_cast<ResourceGameObject*>(
            TileMap_GetObjectAt(g_tilemap, static_cast<short>(tile_x), static_cast<short>(tile_y), 0));
        if (tile != nullptr) {
            TileMapResource* tile_parent = static_cast<TileMapResource*>(tile->resource);
            tile_type = (tile_parent != nullptr) ? tile_parent->object_type : 0;
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
     *     array at occupant+0x38 (8 slots). Same duck-typed convention as
     *     AddOccupant (see its comment) — Entity's own +0x38 is unrelated
     *     (source_rect.right); this depends on the real occupant type. --- */
    Entity** occupant_slots = reinterpret_cast<Entity**>(reinterpret_cast<uint8_t*>(occupant) + 0x38);
    for (int idx = 0; idx < 8; idx++) {
        if (occupant_slots[idx] == this) {
            occupant_slots[idx] = nullptr;
            break;
        }
    }

    /* --- Step 3: Determine road class from entity's model/tile data ---
     * Ghidra-confirmed (0x4336A0): `type_info` is occupant+0x20 (same
     * field AddOccupant calls model_data); its own +0x14 holds a pointer
     * whose +0x40 is a TileMapResource* (world/tilemap.h) -- i.e. the
     * same shaped "tile entry" AddOccupant reads via entry_data above,
     * just followed one level further here to reach its resource's
     * object_type (the road/tile-class byte at +0x63A, the same offset
     * already established by the RESDATA_IsBuildingTile/IsRoadTile
     * cluster). The binary does not null-check the +0x40 resource
     * pointer before reading +0x63A; preserved as-is.
     *
     * `tile_entry`'s concrete class is NOT independently verified the
     * way ResourceGameObject's tile-grid placement fields are (see
     * core/BuildingMgrObjectGroup.h) -- it is reached via `occupant`'s
     * own +0x20/+0x14 chain, whose owning type this pass did not trace.
     * Left as an explicit raw offset read rather than asserting a
     * ResourceGameObject* type that isn't backed by the same evidence
     * (out of scope for the TileMapObject-mirror elimination pass; see
     * PROGRESS.md's "TileMapObject mirror struct" entry). */
    void* type_info = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(occupant) + 0x20);
    void* tile_entry = (type_info != nullptr)
        ? *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(type_info) + 0x14)
        : nullptr;
    uint8_t road_class = 0xFF;

    if (tile_entry != nullptr) {
        TileMapResource* tile_res = *reinterpret_cast<TileMapResource**>(
            reinterpret_cast<uint8_t*>(tile_entry) + 0x40);
        road_class = tile_res->object_type;
    }

    /* --- Step 4: Read building's frame data for sprite offset --- */
    RESDATA* frame_data = static_cast<RESDATA*>(this->resource);     /* +0x40 */
    int16_t offset_x = (frame_data != nullptr) ? frame_data->offset_x : 0;  /* RESDATA::offset_x, +0x32 */
    int16_t offset_y = (frame_data != nullptr) ? frame_data->offset_y : 0;  /* RESDATA::offset_y, +0x34 */

    int exit_x, exit_y;
    int move_tx = this->dest_x;                             /* +0xCC */
    int move_ty = this->dest_y;                             /* +0xD0 */

    if (road_class == 0x12) {
        /* Horizontal road: random offset along road */
        uint32_t r1 = CRT_rand();
        int sign = (r1 & 1) ? -1 : 1;
        uint32_t r2 = CRT_rand();
        exit_x = move_tx + sign * (static_cast<int>(r2 % 11) + 15) - offset_x;
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
    ResourceGameObject* tile_obj = static_cast<ResourceGameObject*>(
        TileMap_GetObjectAt(g_tilemap, static_cast<short>(tile_x), static_cast<short>(tile_y), 0));

    bool use_fallback = true;
    if (tile_obj != nullptr) {
        TileMapResource* tile_frame = static_cast<TileMapResource*>(tile_obj->resource);
        uint8_t tile_type = (tile_frame != nullptr) ? tile_frame->object_type : 0;
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
    this->field_dc = static_cast<int32_t>(sqrt(static_cast<double>(dx * dx + dy * dy)));  /* +0xDC */

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
    int game_time = static_cast<int>(g_game_time);

    /* Not ready to decide yet */
    if (game_time < static_cast<int>(this->next_action_time)) {          /* +0xA0 */
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
    void* time_info = CRT_localtime(reinterpret_cast<const time_t*>(&game_time));

    /* Check spawn_building schedule (+0x90 = occupant_b).
     * If time is within its active window, go there.
     * Occupant's resource is at Entity::resource (+0x40); +0x534/+0x548
     * are BuildingDescriptorEditor::track_pos_a/track_pos_b
     * (input/BuildingDescriptorEditor.h) — confirmed by
     * resources/ResourceManager.h's own comment "Building resources
     * extend this with schedule data at +0x534/+0x548" and by
     * core/BuildingMgrObjectGroup.cpp's UpdateScheduledAnimation, which
     * already reads the identical offsets the same way. TrackPos's
     * layout {vtable, field_04, field_08, coordinate, segment_index}
     * happens to line up byte-for-byte with Game_CheckTimeInRange's
     * expected {second, minute, hour} ints (field_04/field_08 double as
     * the minute/hour slots, sharing the same -1-uninitialized sentinel
     * convention) — kept as a raw int* reinterpret rather than
     * introducing TrackPos here, matching that sibling file's style. */
    if (this->occupant_b != nullptr) {                     /* +0x90 */
        RESDATA* resource = static_cast<RESDATA*>(this->occupant_b->resource);
        if (Game_CheckTimeInRange(static_cast<int*>(time_info),
                reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(resource) + 0x534),
                reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(resource) + 0x548))) {
            return 2;  /* spawn */
        }
    }

    /* Check occupy_building schedule (+0x8C = occupant_a).
     * If time is OUTSIDE its active window, go there. */
    if (this->occupant_a != nullptr) {                     /* +0x8C */
        RESDATA* resource = static_cast<RESDATA*>(this->occupant_a->resource);
        if (!Game_CheckTimeInRange(static_cast<int*>(time_info),
                reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(resource) + 0x534),
                reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(resource) + 0x548))) {
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
    RESDATA* res_ptr = static_cast<RESDATA*>(this->resource);           /* +0x40 */

    /* Check 1: tile at (x, y) only if this is road type 8 */
    uint8_t res_type = (res_ptr != nullptr) ? res_ptr->object_type : 0;
    if (res_type == 8) {
        int tile_y = (y < 0) ? -1 : (y >> 4);
        int tile_x = ((x + 4) < 0) ? -1 : ((x + 4) >> 4);
        ResourceGameObject* tile = static_cast<ResourceGameObject*>(
            TileMap_GetObjectAt(g_tilemap, static_cast<short>(tile_x), static_cast<short>(tile_y), 0));
        if (tile != nullptr) {
            TileMapResource* tres = static_cast<TileMapResource*>(tile->resource);
            if (tres != nullptr && tres->object_type == target_type) {
                return tile;
            }
        }
    }

    /* Check 2: (x + 4, y + frame_height / 2) */
    if (res_ptr != nullptr) {
        uint16_t fh = res_ptr->frame_height;               /* +0x16 */
        int tile_y = ((fh / 2 + y) < 0) ? -1 : ((fh / 2 + y) >> 4);
        int tile_x = ((x + 4) < 0) ? -1 : ((x + 4) >> 4);
        ResourceGameObject* tile = static_cast<ResourceGameObject*>(
            TileMap_GetObjectAt(g_tilemap, static_cast<short>(tile_x), static_cast<short>(tile_y), 0));
        if (tile != nullptr) {
            TileMapResource* tres = static_cast<TileMapResource*>(tile->resource);
            if (tres != nullptr && tres->object_type == target_type) {
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
        ResourceGameObject* tile = static_cast<ResourceGameObject*>(
            TileMap_GetObjectAt(g_tilemap, static_cast<short>(tile_x), static_cast<short>(tile_y), 0));
        if (tile != nullptr) {
            TileMapResource* tres = static_cast<TileMapResource*>(tile->resource);
            if (tres != nullptr && tres->object_type == target_type) {
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
    ResourceGameObject* tile_obj = static_cast<ResourceGameObject*>(
        this->FindNearbyObject(3, this->target_x, this->target_y));

    if (tile_obj == nullptr) return 0;

    TileMapResource* resdata = static_cast<TileMapResource*>(tile_obj->resource);
    int resource_id = (resdata != nullptr) ? resdata->resource_id : -1;

    /* Rail tile check: IDs 0xC6C (straight), 0xC6E (crossing) — block passage */
    if (resource_id == 0xC6C || resource_id == 0xC6E) {
        return 0;
    }

    /* Road class stored deep in the tile resource at +0x63A */
    uint8_t road_class = resdata->state_63A;

    /* Horizontal road (class 0x12) */
    if (road_class == 0x12) {
        /* Check for vehicle boarding: RESDATA_GameVehicle::boarding_vehicle
         * (+0x118, game/ResdataGameVehicle.h) -- tile_obj is a road/track
         * tile (matched via FindNearbyObject's target_type==3 resource
         * check above), which is always RESDATA_GameVehicle or a class
         * derived from it (the only class in the ResourceGameObject
         * hierarchy with a field at this offset). Vehicle_GetOccupantCount
         * was a void*-taking free-function landmine (docs/landmine-sweep-
         * worklist.md, callers: Building::FindPathToTarget) that bound
         * to nothing — the real function is the typed method
         * Vehicle::GetOccupantCount() (0x44C370, uint8_t, no args). */
        Vehicle* vehicle = static_cast<RESDATA_GameVehicle*>(tile_obj)->boarding_vehicle;
        if (vehicle != nullptr) {
            int32_t vs = vehicle->state;
            if (vs == 0 || vs == 1) {  /* idle or boarding */
                if (vehicle->GetOccupantCount() != 0) {
                    this->AddOccupant(reinterpret_cast<Entity*>(vehicle));
                    return 1;
                }
            }
        }

        /* tile_obj's own world pixel position -- GameObject::world_x/
         * world_y (core/GameObject.h), inherited by ResourceGameObject;
         * NOT its grid sub_pos_x/sub_pos_y at +0x88/+0x8A (a distinct,
         * smaller int16 pair). */
        int tile_x = tile_obj->world_x;
        if (this->world_x == tile_x) {
            uint32_t r = CRT_rand();
            int sign = (r & 1) ? -1 : 1;
            r = CRT_rand();
            this->dest_x = tile_x + sign * (static_cast<int>(r % 11) + 15);
        } else {
            this->dest_x = tile_x;
        }

        int tile_y = tile_obj->world_y;
        this->dest_y = (tile_y > this->world_y) ? this->world_y - 4 : this->world_y + 4;
        return 1;
    }

    /* Vertical road (class 0x13) */
    if (road_class == 0x13) {
        Vehicle* vehicle = static_cast<RESDATA_GameVehicle*>(tile_obj)->boarding_vehicle;
        if (vehicle != nullptr) {
            int32_t vs = vehicle->state;
            if (vs == 0 || vs == 1) {
                if (vehicle->GetOccupantCount() != 0) {
                    this->AddOccupant(reinterpret_cast<Entity*>(vehicle));
                    return 1;
                }
            }
        }

        int tile_y = tile_obj->world_y;
        if (this->world_y == tile_y) {
            uint32_t r = CRT_rand();
            int sign = (r & 1) ? -1 : 1;
            this->dest_y = tile_y + sign * 18 - 5;
        } else {
            this->dest_y = tile_y;
        }

        int tile_x = tile_obj->world_x;
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

    /* Check for random arrival rect.
     * target->resource is Entity's own named +0x40 field (no raw offset
     * needed for that indirection); +0x62C is
     * BuildingDescriptorEditor::leisure_destination (input/
     * BuildingDescriptorEditor.h) — same flag OnOccupantReady checks
     * below and town/Town.cpp already reads at entity->resource+0x62C. */
    RECT bounds;
    if (!target->GetBoundingRect(&bounds) ||
        *(reinterpret_cast<uint8_t*>(target->resource) + 0x62C) == 0) {
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
void Building::HandleAction(int action)
{
    if (!this->visible) return;
    if (g_selected_building == this && g_building_animating) return;

    this->field_a4 = 0;

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

    this->next_action_time = static_cast<int>(CRT_rand() % 21) + 10 + g_game_time;
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
    uint8_t* src = reinterpret_cast<uint8_t*>(data);

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
    obj->type                = *reinterpret_cast<int32_t*>(src + 0x04);  /* +0x04 */
    obj->screen_rect.left    = *reinterpret_cast<int32_t*>(src + 0x08);  /* +0x08 */
    obj->screen_rect.top     = *reinterpret_cast<int32_t*>(src + 0x0C);  /* +0x0C */
    obj->screen_rect.right   = *reinterpret_cast<int32_t*>(src + 0x10);  /* +0x10 */
    obj->screen_rect.bottom  = *reinterpret_cast<int32_t*>(src + 0x14);  /* +0x14 */
    obj->initialized         = *reinterpret_cast<uint8_t*>(src + 0x18);  /* +0x18 */

    /* Callback slots at +0x1C/+0x20: write as raw uint32_t.
     * These overlap with callback_1/callback_2 function pointers
     * in GameObject; the save format stores them as POD. */
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(obj) + 0x1C) = *reinterpret_cast<uint32_t*>(src + 0x1C);
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(obj) + 0x20) = *reinterpret_cast<uint32_t*>(src + 0x20);

    /* Step 2b: Overwrite Entity fields (+0x24..+0x86) */
    obj->visible            = *reinterpret_cast<uint8_t*>(src + 0x24);  /* +0x24 */
    obj->anim_index         = *reinterpret_cast<int32_t*>(src + 0x28);  /* +0x28 */
    obj->blit_flags         = *reinterpret_cast<uint32_t*>(src + 0x2C);  /* +0x2C */
    obj->source_rect.left   = *reinterpret_cast<int32_t*>(src + 0x30);
    obj->source_rect.top    = *reinterpret_cast<int32_t*>(src + 0x34);
    obj->source_rect.right  = *reinterpret_cast<int32_t*>(src + 0x38);
    obj->source_rect.bottom = *reinterpret_cast<int32_t*>(src + 0x3C);

    obj->resource           = *reinterpret_cast<void**>(src + 0x40);  /* +0x40 */
    obj->sound_res_id       = *reinterpret_cast<uint32_t*>(src + 0x44);  /* +0x44 */
    obj->audio_channel      = *reinterpret_cast<void**>(src + 0x48);  /* +0x48 */
    obj->world_x            = *reinterpret_cast<int32_t*>(src + 0x4C);  /* +0x4C */
    obj->world_y            = *reinterpret_cast<int32_t*>(src + 0x50);  /* +0x50 */
    obj->frame_index        = *reinterpret_cast<int32_t*>(src + 0x54);  /* +0x54 */
    obj->timer              = *reinterpret_cast<uint32_t*>(src + 0x58);  /* +0x58 */
    obj->active_state       = *reinterpret_cast<uint32_t*>(src + 0x5C);  /* +0x5C */
    obj->next_sound_time    = *reinterpret_cast<uint32_t*>(src + 0x60);  /* +0x60 */
    obj->stored_resource_id   = *reinterpret_cast<uint32_t*>(src + 0x64);  /* +0x64 */
    obj->action_cooldown_time = *reinterpret_cast<uint32_t*>(src + 0x68);  /* +0x68 */
    obj->phase_timer        = *reinterpret_cast<uint32_t*>(src + 0x6C);  /* +0x6C */
    obj->waiting_flag       = *reinterpret_cast<uint8_t*>(src + 0x70);  /* +0x70 */
    obj->world_x_raw        = *reinterpret_cast<int32_t*>(src + 0x74);  /* +0x74 */
    obj->world_y_raw        = *reinterpret_cast<int32_t*>(src + 0x78);  /* +0x78 */

    /* Name field (+0x7C..+0x86, 11 bytes) */
    memcpy(obj->name, src + 0x7C, 11);

    /* Step 2c: Overwrite Building fields (+0x88..+0xF3) */
    obj->occupation_level   = *reinterpret_cast<uint8_t*>(src + 0x88);  /* +0x88 */
    obj->disabled           = *reinterpret_cast<uint8_t*>(src + 0x89);  /* +0x89 */
    obj->_pad_8a[0]         = *reinterpret_cast<uint8_t*>(src + 0x8A);
    obj->_pad_8a[1]         = *reinterpret_cast<uint8_t*>(src + 0x8B);
    obj->occupant_a         = *reinterpret_cast<Entity**>(src + 0x8C);  /* +0x8C */
    obj->occupant_b         = *reinterpret_cast<Entity**>(src + 0x90);  /* +0x90 */
    obj->create_time        = *reinterpret_cast<uint32_t*>(src + 0x94);  /* +0x94 */
    obj->conn_building_a    = *reinterpret_cast<int32_t*>(src + 0x98);  /* +0x98 */
    obj->conn_building_b    = *reinterpret_cast<int32_t*>(src + 0x9C);  /* +0x9C */
    obj->next_action_time   = *reinterpret_cast<uint32_t*>(src + 0xA0);  /* +0xA0 */
    obj->field_a4           = *reinterpret_cast<uint32_t*>(src + 0xA4);  /* +0xA4 */
    obj->target_x           = *reinterpret_cast<int32_t*>(src + 0xA8);  /* +0xA8 */
    obj->target_y           = *reinterpret_cast<int32_t*>(src + 0xAC);  /* +0xAC */
    obj->search_x1          = *reinterpret_cast<int32_t*>(src + 0xB0);  /* +0xB0 */
    obj->search_y1          = *reinterpret_cast<int32_t*>(src + 0xB4);  /* +0xB4 */
    obj->track_x            = *reinterpret_cast<int32_t*>(src + 0xB8);  /* +0xB8 */
    obj->track_y            = *reinterpret_cast<int32_t*>(src + 0xBC);  /* +0xBC */
    obj->track_node_id      = *reinterpret_cast<int32_t*>(src + 0xC0);  /* +0xC0 */
    obj->prev_target_x      = *reinterpret_cast<int32_t*>(src + 0xC4);  /* +0xC4 */
    obj->prev_target_y      = *reinterpret_cast<int32_t*>(src + 0xC8);  /* +0xC8 */
    obj->dest_x             = *reinterpret_cast<int32_t*>(src + 0xCC);  /* +0xCC */
    obj->dest_y             = *reinterpret_cast<int32_t*>(src + 0xD0);  /* +0xD0 */
    obj->waypoint_x1        = *reinterpret_cast<int32_t*>(src + 0xD4);  /* +0xD4 */
    obj->waypoint_y1        = *reinterpret_cast<int32_t*>(src + 0xD8);  /* +0xD8 */
    obj->field_dc           = *reinterpret_cast<int32_t*>(src + 0xDC);  /* +0xDC */
    obj->field_e0           = *reinterpret_cast<uint32_t*>(src + 0xE0);  /* +0xE0 */
    obj->field_e4           = *reinterpret_cast<uint8_t*>(src + 0xE4);  /* +0xE4 */
    memcpy(obj->_pad_e5, src + 0xE5, 3);
    obj->last_action        = *reinterpret_cast<int32_t*>(src + 0xE8);  /* +0xE8 */
    obj->field_ec           = *reinterpret_cast<uint32_t*>(src + 0xEC);  /* +0xEC */
    obj->occupant_ptr       = *reinterpret_cast<Entity**>(src + 0xF0);  /* +0xF0 */

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

    /* Check the flag at resource + 0x62C
     * (BuildingDescriptorEditor::leisure_destination — see MoveToTarget
     * above and town/Town.cpp's identical entity->resource+0x62C read). */
    RESDATA* res = static_cast<RESDATA*>(entity->resource);      /* +0x40 */
    uint8_t flag_62c = *(reinterpret_cast<uint8_t*>(res) + 0x62C);

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
    Building* next_bldg = static_cast<Building*>(next_entity);
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
    /* +0x168/+0x169 are BuildingDescriptorEditor::border_width/
     * border_height (input/BuildingDescriptorEditor.h's physical-
     * occupancy grid dimensions) — reused here as a max-step pixel
     * clamp / animation-frame limit respectively. Same shared-offset
     * convention as the +0x168/+0x169 grid_width/grid_height on
     * TileMapResource (world/tilemap.h): both are within the common
     * "ChildWindow-family" descriptor header. */
    if (this->occupant_ptr != nullptr) {
        /* Occupant present — compute step toward dest */
        int out[2];
        this->CalcMoveTarget(out, this->dest_x, this->dest_y,
            *(reinterpret_cast<uint8_t*>(this->resource) + 0x168));

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
    RESDATA* res = static_cast<RESDATA*>(this->resource);        /* +0x40 */
    uint8_t anim_limit = *(reinterpret_cast<uint8_t*>(res) + 0x169);

    if (this->field_e4 < anim_limit) {
        return 1;  /* still counting frames */
    }

    /* Reached the animation limit — compute step and check placement */
    this->field_e4 = 0;                             /* +0xE4 */

    int out[2];
    this->CalcMoveTarget(out, this->dest_x, this->dest_y,
        *(reinterpret_cast<uint8_t*>(res) + 0x168));

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
/*   Math_PointOnLineSegment (0x45C7C0) — line-segment collision test   */
/*   AssetMgr_ReadPairValue  (0x45DD80) — connection graph lookup       */
/*   TileMap_FindTileByType  (0x457ce0) — tile search by type           */
/* ================================================================== */
void Building::StepToward(int x, int y)
{
    /* --- Step 1: Convert target to tile coordinates --- */
    int tile_x = (x < 0) ? -1 : (x >> 4);
    int tile_y = (y < 0) ? -1 : (y >> 4);

    /* --- Step 2: Look up tile at target tile position --- */
    ResourceGameObject* tile = static_cast<ResourceGameObject*>(
        TileMap_GetObjectAt(g_tilemap, static_cast<short>(tile_x), static_cast<short>(tile_y), 0));  /* 0x455620 */

    uint8_t tile_type = 0;
    if (tile != nullptr) {
        TileMapResource* tile_res = static_cast<TileMapResource*>(tile->resource);
        tile_type = (tile_res != nullptr) ? tile_res->object_type : 0;
    }

    /* --- Step 3: Branch on tile type (binary: 0x432B50) ---
     *
     * Branch (a): Type 0x0C (walkable) or Type 3 (road) WITH occupant.
     *   Binary condition: cVar2 == '\f' || (cVar2 == '\x03' && param_1[0x3c] != 0)
     *   param_1[0x3c] = this->occupant_ptr (+0xF0). Confirmed at 0x432B50.
     *
     * Branch (b): All other types → fallback direct step toward target.
     *
     * Within branch (a), the binary has two sub-cases:
     *   (c) Lane-offset: conn_node_id at tile+0xE4 (ResourceGameObject::
     *       occupancy_more) == 0xFFFFFFFF → scans the 4-slot
     *       occupancy_scores[4] array at tile+0xD4 for lane selection.
     *   (d) Connection-node traversal: conn_node_id != 0xFFFFFFFF
     *       → uses AssetMgr_ReadPairValue + Math_PointOnLineSegment. */
    if (tile_type == 0x0C || (tile_type == 3 && this->occupant_ptr != nullptr)) {
        /* --- Step 3a: Determine connection node at tile+0xE4 --- */
        uint32_t conn_node_id = (tile != nullptr)
            ? static_cast<uint32_t>(tile->occupancy_more) : 0xFFFFFFFF;

        if (conn_node_id == 0xFFFFFFFF) {
            /* Branch (c): Lane-offset computation (0x432B80–0x432D30).
             *
             * No connection node → scan the 4 "lane" scores at
             * occupancy_scores[4] (+0xD4..+0xE3), then read the
             * connected tile from occupancy_links[lane] (+0xC4..+0xD3).
             *
             * BUG FIX (Ghidra-confirmed against 0x432AE0/0x432940
             * disassembly, both instances of this lane-scan loop): the
             * count-array base is +0xD4, not +0xD8 — the previous +0xD8
             * base read occupancy_scores[1..3] shifted down by one slot
             * and, for lane 3, read occupancy_more (+0xE4, a completely
             * different field: "keep walking the chain" flag, not a
             * score) instead of occupancy_scores[3]. Fixed to index
             * occupancy_scores[] directly, matching disassembly's
             * `dword ptr [EDI + EAX*0x4 + 0xd4]` / `LEA EBP,[EDI+0xd8]`
             * (the scan pointer starts one slot past the base, at
             * lane 1, because lane 0 is the implicit initial candidate —
             * preserved by scanning lanes 0..3 against a base of +0xD4
             * here, which is behaviorally equivalent to the binary's
             * "start from lane 0, compare candidates 1..3" walk). */
            if (x != this->target_x || y != this->target_y) {
                uint8_t best_lane = 0;
                uint32_t best_count = 0;
                for (int lane = 0; lane < 4; lane++) {
                    uint32_t count = static_cast<uint32_t>(tile->occupancy_scores[lane]);
                    if (count != 0 && (best_count == 0 || count < best_count)) {
                        best_count = count;
                        best_lane = lane;
                    }
                }
                ResourceGameObject* conn = tile->occupancy_links[best_lane];
                if (conn != nullptr) {
                    this->dest_x = conn->world_x;
                    this->dest_y = conn->world_y;
                    return;
                }
            }
        } else {
            /* Branch (d): Connection-node traversal (0x432C50–0x432F80).
             *
             * Valid node ID → follow the connection graph:
             *  1. Get connected tile via occupancy_links[node_id]
             *  2. AssetMgr_ReadPairValue for direction mask
             *  3. Math_PointOnLineSegment for path validation
             *  4. If direction differs from node_id, switch lanes
             *  5. Set track/dest to final tile position              */
            ResourceGameObject* conn = tile->occupancy_links[conn_node_id];
            if (conn != nullptr) {
                uint32_t peer_node = static_cast<uint32_t>(conn->occupancy_more);
                uint8_t dir = AssetMgr_ReadPairValue(
                    static_cast<AssetMgr*>(g_asset_mgr), peer_node, this->track_node_id);

                int conn_x = conn->world_x;
                int conn_y = conn->world_y;
                int tile_x2 = tile->world_x;
                int tile_y2 = tile->world_y;

                uint8_t on_seg = Math_PointOnLineSegment(
                    this->track_x, this->track_y,
                    tile_x2, tile_y2, conn_x, conn_y);

                if (on_seg == 0 &&
                    ((dir - 2) & 3) == conn_node_id &&
                    tile->occupancy_links[dir] != nullptr) {
                    conn_node_id = dir;
                }

                ResourceGameObject* final_tile = tile->occupancy_links[conn_node_id];
                if (final_tile != nullptr) {
                    this->track_x = final_tile->world_x;
                    this->track_y = final_tile->world_y;
                    this->track_node_id = static_cast<uint32_t>(final_tile->occupancy_more);

                    if (this->track_node_id == conn_node_id) {
                        this->dest_x = this->track_x;
                        this->dest_y = this->track_y;
                    } else {
                        if (this->dest_x < this->track_x) this->dest_x += 4;
                        else if (this->dest_x > this->track_x) this->dest_x -= 4;
                        if (this->dest_y < this->track_y) this->dest_y += 4;
                        else if (this->dest_y > this->track_y) this->dest_y -= 4;
                    }
                    return;
                }
            }
        }
    }

    /* --- Step 4: Fallback — advance dest directly toward (x,y) --- */
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

    /* Search for a road tile near the target.
     * TileMap_FindTileByType was a landmine (docs/landmine-sweep-
     * worklist.md, "genuinely missing"): no symbol of that name/shape
     * exists. The address it cites, 0x457CE0, is TileMap::FindNearestObject
     * (world/tilemap.h/.cpp, exposed as TileMap_FindNearestObject) — the
     * worklist's "genuinely missing" classification was itself wrong, not
     * just the caller. Fixed the name AND the argument order/types to
     * match: real signature is (type_filter, tx, ty, radius), but this
     * call passed (tx, ty, 0x0C, 0x900) — type_filter and radius were in
     * the wrong argument positions. Ghidra-confirmed against 0x432940's
     * decompile: `TileMap_FindNearestObject(&g_tilemap,0xc,target_x,
     * target_y,0x900)`. */
    ResourceGameObject* road_tile = static_cast<ResourceGameObject*>(
        TileMap_FindNearestObject(g_tilemap, 0x0C, x, y, 0x900));

    if (road_tile != nullptr) {
        /* Store road tile position in track-follow fields */
        this->track_x      = road_tile->world_x;  /* +0xB8 */
        this->track_y      = road_tile->world_y;  /* +0xBC */

        /* Get connection node ID from the tile's occupancy_more (+0xE4) */
        int32_t conn_node = road_tile->occupancy_more;
        if (conn_node != -1) {
            this->track_node_id = conn_node;        /* +0xC0 */
        } else {
            /* Scan the tile's occupancy_scores[4] (+0xD4) for the lane
             * with the minimum nonzero score — same selection rule as
             * StepToward's lane scan above (see its BUG FIX comment for
             * full Ghidra evidence). This function's own version of the
             * loop additionally had the comparison direction inverted
             * (`count > best_count`, i.e. picking the MAXIMUM) — the
             * decompile of 0x432940 shows the identical min-nonzero
             * selection as StepToward's, so both the base offset and
             * the comparison direction are fixed here to match. */
            uint8_t best_slot = 0;
            uint32_t best_count = 0;
            for (int slot = 0; slot < 4; slot++) {
                uint32_t count = static_cast<uint32_t>(road_tile->occupancy_scores[slot]);
                if (count != 0 && (best_count == 0 || count < best_count)) {
                    best_count = count;
                    best_slot = static_cast<uint8_t>(slot);
                }
            }
            ResourceGameObject* conn_slot_obj = road_tile->occupancy_links[best_slot];
            if (conn_slot_obj != nullptr) {
                this->track_node_id = conn_slot_obj->occupancy_more;
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
    /* Step 1: Detach from current scene-graph parent (occupant_b).
     * Ghidra-confirmed (0x433CA0 decompile) identical to BaseCleanup:
     * the parent is a ResourceGameObject* (linked_objects[5] @ +0xA4,
     * group_flag @ +0x8E), not a generic Entity. */
    if (this->occupant_b != nullptr) {
        ResourceGameObject* parent = static_cast<ResourceGameObject*>(this->occupant_b);  /* +0x90 */

        /* Search parent's 5-slot child array at parent+0xA4 for `this` */
        for (int slot = 0; slot < 5; slot++) {
            if (parent->linked_objects[slot] == this) {
                parent->linked_objects[slot] = nullptr;
                /* Decrement parent's group_flag (child count) at +0x8E */
                parent->group_flag = static_cast<uint8_t>(parent->group_flag - 1);
                break;
            }
        }
        this->occupant_b = nullptr;                 /* +0x90 */
    }

    /* Step 2: Search for a compatible host to re-attach to.
     * Address: 0x433D98
     *
     * KNOWN-WRONG, NOT JUST APPROXIMATED (upgraded from the previous vaguer
     * TODO after Ghidra-decompiling 0x433CA0 in full):
     *
     *   1. The binary does NOT iterate g_building_mgr->buildings. It reads
     *      the global object-list Collection at 0x4A9994 (DAT_004a9994,
     *      the same "all live GameObjects" list Netman::CheckTimeout /
     *      AssetMgr_LoadFileEx / INPUT_PeriodicTickDispatch already use —
     *      see network/Netman.cpp's `extern Collection DAT_004a9994`),
     *      dispatching through ITS vtable slot [8] (byte offset 0x20,
     *      the same slot BuildingCollection::GetItem occupies —
     *      `iVar4 = (**(code**)(DAT_004a9994 + 0x20))(index)`), bounded by
     *      g_object_count, not BuildingCollection::GetCount().
     *   2. The match predicate is NOT a sub_type-byte compare. It is:
     *      `candidate->group_flag < candidate->resource[0x516]` (a
     *      per-resource max-member-count byte — BuildingDescriptorEditor::
     *      max_employees, input/BuildingDescriptorEditor.h) AND
     *      `this->resource->resource_id` appears in the 5×int16 list at
     *      `candidate->resource + 0x518` (BuildingDescriptorEditor::
     *      possible_employees[5]).
     *   3. `candidate` itself is a ResourceGameObject* (its +0xA4/+0x8E
     *      reads are linked_objects[5]/group_flag, exactly Step 1's shape),
     *      NOT a Building*.
     *
     *   The code below still iterates g_building_mgr->buildings and treats
     *   each `candidate` as if it had a 5-slot pointer array at +0xA4 —
     *   but Building's OWN layout at +0xA4..+0xB7 is field_a4/target_x/
     *   target_y/search_x1/search_y1 (game/Building.h), not a pointer
     *   array. If this branch is ever reached with a real Building
     *   candidate, it corrupts that candidate's own movement-state fields
     *   instead of registering a group membership. Reproducing the real
     *   behavior needs: (a) a typed accessor for DAT_004a9994's vtable
     *   slot [8] (Collection, shared/collections.h, doesn't currently
     *   expose that slot), and (b) switching the predicate to the
     *   BuildingDescriptorEditor fields above — both out of scope for a
     *   cast-cleanup pass; left as raw-cast-modernized but behaviorally
     *   unchanged rather than fabricating an unvalidated rewrite. */
    if (g_building_mgr != nullptr && this->resource != nullptr) {
        RESDATA* my_res = static_cast<RESDATA*>(this->resource);     /* +0x40 */
        uint8_t my_sub_type = my_res->object_type;

        BuildingCollection* collection = &g_building_mgr->buildings;
        uint32_t count = collection->GetCount();

        for (uint32_t i = 0; i < count; i++) {
            Building* candidate = collection->GetItem(i);
            if (candidate == nullptr || candidate == this) continue;

            /* Check candidate has no occupant in its occupant_b slot */
            if (candidate->occupant_b != nullptr) continue;

            /* Check sub-type compatibility */
            RESDATA* cand_res = static_cast<RESDATA*>(candidate->resource);
            if (cand_res == nullptr) continue;
            uint8_t cand_sub_type = cand_res->object_type;
            if (cand_sub_type != my_sub_type) continue;

            /* Compatible host found: add `this` to host's child array.
             * Host child array is at host+0xA4 (5 slots) — see the
             * KNOWN-WRONG note above; `candidate` is really a Building*
             * here, not the ResourceGameObject* this layout requires. */
            Entity** child_array = reinterpret_cast<Entity**>(reinterpret_cast<uint8_t*>(candidate) + 0xA4);
            int slot;
            for (slot = 0; slot < 5; slot++) {
                if (child_array[slot] == nullptr) {
                    child_array[slot] = this;
                    break;
                }
            }

            if (slot < 5) {
                /* Increment host's child count at host+0x8E */
                uint8_t* child_count = reinterpret_cast<uint8_t*>(candidate) + 0x8E;
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
    RESDATA* res = static_cast<RESDATA*>(this->resource);        /* +0x40 */
    if (res != nullptr) {
        uint16_t fw = res->frame_width;             /* +0x14 */
        uint16_t fh = res->frame_height;            /* +0x16 */

        int base_tx = worldToTile(x);
        int base_ty = worldToTile(y);

        /* Compute tile footprint: frame dimensions in tiles.
         * The binary divides width/height by 16 (>> 4) to get tile count. */
        int tiles_wide = static_cast<int>(fw) >> 4;
        int tiles_high = static_cast<int>(fh) >> 4;
        if (tiles_wide < 1) tiles_wide = 1;
        if (tiles_high < 1) tiles_high = 1;

        /* Iterate over every tile position in the footprint */
        for (int ty = 0; ty < tiles_high; ty++) {
            for (int tx = 0; tx < tiles_wide; tx++) {
                int tile_x = base_tx + tx;
                int tile_y = base_ty + ty;

                if (tile_x < 0 || tile_y < 0) continue;

                ResourceGameObject* tile_obj = static_cast<ResourceGameObject*>(
                    TileMap_GetObjectAt(g_tilemap, static_cast<short>(tile_x), static_cast<short>(tile_y), 0));
                if (tile_obj != nullptr) {
                    /* Check if tile is occupied by another building or obstacle.
                     * The binary checks tile->resource type and occupancy flags.
                     * A non-null tile with certain type flags means blocked. */
                    TileMapResource* tile_res = static_cast<TileMapResource*>(tile_obj->resource);
                    if (tile_res != nullptr) {
                        uint8_t tile_type = tile_res->object_type;
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
    /* Binary at 0x4343F0 calls Math_DistSquared (0x45C7A0) for
     * squared Euclidean distance computation, then calls
     * AssetMgr_ReadPairValue (0x45DD80) for connection flags.
     *
     * node_set's documented layout here (+0x00 count, +0x08 nodes[])
     * matches resources/AssetMgr.h's AssetMgr struct exactly
     * (entry_count @ +0x00, resource_array @ +0x08) — node_set really
     * is an AssetMgr*-shaped connection matrix (its own +0x04
     * pair_matrix is what AssetMgr_ReadPairValue below reads),
     * corroborated by AssetMgr_ReadPairValue's real signature taking
     * `AssetMgr* self` as the exact object passed as `this=node_set`.
     * AssetMgr is only forward-declared here (see the top-of-file note
     * on why resources/AssetMgr.h isn't included), so field access below
     * stays a raw reinterpret_cast rather than named members; only the
     * pointer's TYPE (needed for AssetMgr_ReadPairValue's correct
     * mangled overload) is fixed. */
    AssetMgr* node_mgr = static_cast<AssetMgr*>(node_set);

    /* Compute baseline squared distance: self world_pos → target */
    int best_dist = Math_DistSquared(
        this->world_x, this->world_y, this->target_x, this->target_y);

    uint32_t count = *reinterpret_cast<uint32_t*>(node_set);          /* +0x00 */
    uint32_t best_idx = current_node_id;
    void** node_array = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(node_set) + 0x08);    /* +0x08 */

    for (uint32_t i = 0; i < count; i++) {
        /* AssetMgr_ReadPairValue(this=node_set, a=current_node_id, b=i)
         * Returns 0-3 = valid connection direction,
         *         0x80 = empty but valid,
         *         0xFF = invalid/nonexistent.
         * AssetMgr_ReadPairValue was a void*-first-param landmine
         * (docs/landmine-sweep-worklist.md, callers: Building::
         * StepToward, Building::FindNearestConnectionNode) — the real
         * function takes AssetMgr*. */
        uint8_t flag = AssetMgr_ReadPairValue(node_mgr, current_node_id, i);

        if (flag == 0x80 || flag == 0xFF) {
            continue;
        }

        Entity* node_entity = static_cast<Entity*>(node_array[i]);
        if (node_entity == nullptr) continue;

        int node_dist = Math_DistSquared(
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
    RESDATA* res = static_cast<RESDATA*>(this->resource);        /* +0x40 */
    if (res != nullptr) {
        uint8_t sub_type = res->object_type;
        if (sub_type == SUBTYPE_STATION) {
            g_building_mgr->CompactCollections();   /* 0x434870 */
        }
    }

    /* Step 3: PARTY mode trigger — DECOMPILER NOTE:
     * Party mode activates when the name does NOT contain "PARTY"
     * (inverted check, binary-correct per Ghidra). Party mode
     * appears to be the default; naming a building "PARTY" disables it. */
    if (CRT_wcsstr(reinterpret_cast<const wchar_t*>(name), PARTY_STRING) == nullptr) {
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
