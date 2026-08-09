// Status: INTEGRATED
/**
 * World.cpp — Top-level game world manager implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the World singleton class. All methods are non-virtual
 * __thiscall (ECX = this = World*). Three related free functions
 * (World_SerializeMap, World_RenderAll, World_GetObjectAt) operate
 * on game-vehicle/vehicle objects rather than the World singleton.
 *
 * The World singleton lives at g_world (0x4A98B0). It is NOT
 * dynamically allocated — it's a global struct in the .data section.
 *
 * All addresses validated against loco.exe via Ghidra (locon).
 */

#include "World.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include "../shared/types.h"
#include "../network/Netman.h"
#include "../core/VehicleEditor.h"
#include "../game/Vehicle.h"
#include "../game/Building.h"
#include "../game/GameVehicle.h"
#include "../game/PlayerConfig.h"
#include "../ui/HelpPageNode.h"

#include <cstdint>
#include <new>

/* NOTE: deliberately NOT #include "../graphics/LOCOBITMAP.h" here — it
 * pulls in ui/ButtonSprite.h's `extern "C" void Sprite_Destroy(void*)`,
 * which conflicts with network/Netman.h's own (incorrectly C++-linked)
 * declaration of the same real extern-"C" symbol; Netman.h is already
 * included above (via network/Netman.h) and fixing that separate,
 * pre-existing linkage landmine is out of scope for this file. Declare
 * Town_BlitViewport's real signature locally instead (matching this
 * file's existing style of many parallel per-TU extern declarations). */
struct UIPANEL_Surface;
extern uint32_t __thiscall Town_BlitViewport(UIPANEL_Surface* self, int x1,
                                             int y1, int x2, int y2,
                                             int x, int y);            /* @ 0x42CB10 —
                                             canonical decl: graphics/LOCOBITMAP.h */

/* ================================================================== */
/* External function declarations                                      */
/* ================================================================== */

void* __cdecl operator_new(size_t size);          /* @ 0x465CE0 */
void GLOBAL_free(void* pointer);                   /* @ 0x465CD0 */

extern "C" {
    /* CRT / memory management */
    int     __cdecl CRT_sprintf(char* buf, int max_len,
                                const char* fmt, ...);    /* @ 0x467FF0  sprintf wrapper */

    /* Windows API */
    int     __stdcall IntersectRect(RECT* dst, RECT* src1, RECT* src2); /* @ 0x47726C via IAT */
}

/* Game functions (free functions with no typed C++ equivalent yet) */
class InputMgr;
extern void   __thiscall VehicleEditor_Update(Vehicle* vehicle);      /* @ 0x44C3A0 */
extern uint   __cdecl   CGWND_MapResourceToDirection(int resource_id);/* @ 0x40EB60 */
extern void*  __thiscall TileMap_GetObjectAt(void* tilemap, short x,
                                             short y, short layer);    /* @ 0x455620 */
extern void*  INPUT_FindObjectAt(InputMgr* input_mgr, int param); /* @ 0x41E1F0 */
/* ArrivalQueue_AddVehicle/RemoveVehicle (formerly free functions taking
 * an untyped void* self) are now HelpPageNode::AddVehicle/RemoveVehicle
 * (ui/HelpPageNode.h, 0x44F3A0/0x44F410) — see
 * docs/landmine-sweep-worklist.md's ArrivalQueue section for the
 * evidence chain that identified self's real type. */
extern void*  __thiscall UI_CreateMessageBox(void* mgr, int res_id, short type,
                                             char anchor, int x, int y,
                                             char flags);             /* @ 0x423AB0 */
extern void   __thiscall Town_SelectBuilding(void* town_view,
                                             int building);           /* @ 0x42D040 */
extern int    __thiscall DDRAW_SelectBuilding(void* ddraw_building,
                                              int building);          /* @ 0x459180 —
                                              returns uint8_t; corrected from a
                                              `void` decl (ODR mismatch against
                                              graphics/DDRAW.cpp's real definition) */

/* Forward-declare free functions implemented below */
extern void __stdcall World_RenderAll(Vehicle* vehicle);  /* @ 0x44E630 */
extern void __stdcall World_GetObjectAt(Vehicle* vehicle);/* @ 0x44E800 */

/* ================================================================== */
/* Global variables referenced by World methods                        */
/* ================================================================== */

extern int32_t    g_game_mode;                /* 0x004851F4 */
extern Netman*    g_netman;                   /* 0x004FD3AC */
extern void*      g_town_view;                /* 0x004852A0 */
extern void*      g_ddraw_building;           /* 0x004A9EF0 */
extern InputMgr  g_input_mgr;                /* 0x004A9990 — static InputMgr object (input/InputMgr.h) */
class TileMap;
extern TileMap*     g_tilemap;                  /* 0x004AAD08 */
extern void*      g_tooltip_mgr;              /* 0x004FD220  UI/tooltip manager */
extern uint8_t    g_click_on_town;            /* 0x0048557C */
extern Entity*    g_selected_building;        /* binary slot 0x00485380 — never written in
                                                 loco.exe (distinct from the town/building
                                                 selection global at 0x004855B0 used by
                                                 Building/Train/TileMap) */
extern PlayerConfig* g_player_config;         /* 0x004AA4A8 */
extern void*      g_world_release_a;          /* 0x00485268  released+nulled by World::Init */
extern void*      g_world_release_b;          /* 0x0048526C  released+nulled by World::Init */

/* ================================================================== */
/* World_Shutdown                                                       */
/* Address: 0x44D870                                                    */
/* __thiscall (ECX = this)                                              */
/*                                                                      */
/* Zeroes the entire World struct: counters, vehicle slots, and the     */
/* 16-entry sub-object array. Called during cleanup and quit-to-menu.   */
/*                                                                      */
/* Called by: CGWND_Cleanup (0x407888), CGWND_QuitToMenu (0x406EB8)    */
/* ================================================================== */
void World::Shutdown(void)
{
    /* Clear counters and vehicle slots (+0x04 through +0x14) */
    this->vehicle_count = 0;           /* +0x04 */
    this->local_vehicle_count = 0;     /* +0x06 */
    this->vehicles[0] = NULL;
    this->vehicles[1] = NULL;
    this->vehicles[2] = NULL;
    this->vehicles[3] = NULL;

    /* Clear all 16 sub_object slots (+0x18 through +0x54) */
    for (int i = 0; i < 16; i++) {
        this->sub_objects[i] = NULL;   /* +0x18 + i*4 */
    }
}

/* ================================================================== */
/* World_Init                                                           */
/* Address: 0x44D9B0                                                    */
/* __thiscall (ECX = this)                                              */
/*                                                                      */
/* Removes all vehicles from the world. For each active vehicle:        */
/* clears all 8 occupant slots, renders final state, and saves.         */
/* Then releases two global object pointers (0x485268 / 0x48526C) via   */
/* their vtable[2] (+0x08) release methods.                             */
/*                                                                      */
/* Called by: CGWND_Cleanup (0x40787E), Sprite_UnlockAll (0x454FF4),   */
/*            CollisionData_Dtor (0x44D839), INPUT_NewWorld (0x41E148) */
/* ================================================================== */
void World::Init(void)
{
    for (int slot_idx = 0; slot_idx < 4; slot_idx++) {
        Vehicle* vehicle = this->vehicles[slot_idx];  /* +0x08 + slot_idx * 4 */
        if (vehicle == NULL) {
            continue;
        }

        /* Remove all 8 occupant slots at vehicle->occupant_tracks (+0x38..+0x54) */
        for (int i = 0; i < 8; i++) {
            if (vehicle->occupant_tracks[i] != 0) {
                reinterpret_cast<Building*>(vehicle->occupant_tracks[i])->RemoveOccupant();
                vehicle->occupant_tracks[i] = 0;
            }
        }

        /* Render final state then save */
        World_RenderAll(vehicle);
        this->SaveToFile(static_cast<uint>(vehicle->network_id),   /* +0x7A */
                         static_cast<char>(vehicle->slot_index),   /* +0x78 */
                         static_cast<char>(1));                    /* mp_flag */
    }

    /* Release global object pointers at 0x485268 and 0x48526C.
       Both are released via vtable slot 2 (+0x08) when non-NULL and then
       nulled. get_xrefs_to() on both addresses (Ghidra) shows World::Init
       is their ONLY reader or writer anywhere in the binary — neither is
       ever assigned, so both are always NULL at runtime and this cleanup
       never fires in the shipped binary.
       Their concrete class remains unidentified: a disassembly of the call
       itself (0x44DA15-0x44DA24) shows a single-argument call (self only,
       no extra ints), which rules out this being GameObject/Entity's own
       slot [2] = PtInRect(int,int) (that overload needs two more pushed
       args) — so whatever real class this is, its vtable diverges from
       the GameObject/Entity convention at slot 2, or predates it entirely.
       With no writer to type-narrow from and no matching known class,
       this stays an untyped function-pointer dispatch (no header to add a
       virtual method to) — only the C-style casts are replaced here.

       IMPORTANT: the retained `+ 8` is the ORIGINAL x86 binary's byte
       offset into a 4-byte-pointer vtable (slot 2 = +0x08 there). On this
       64-bit host, `*reinterpret_cast<void**>(...)` reads the object's
       real (GCC-emitted) host vtable pointer, whose entries are 8 bytes
       each — so `+ 8` here actually lands on host vtable slot 1, not
       slot 2, and does not correspond to "PtInRect" or anything else in
       the GameObject/Entity convention on this host. This is the exact
       bug class fixed as a live bug in World_SerializeMap's
       VehicleEditor::SetResourceId call below (misaligned/mis-slotted
       host vtable arithmetic derived from an x86 byte offset). It is
       NOT fixed here because it is genuinely dead (no writer anywhere;
       always NULL) — do not revive this path without first retyping
       g_world_release_a/b to a real class and replacing this with a
       named virtual call, the same way that other call was fixed.
       TODO: revisit if a writer for either global is ever found. */
    typedef void (*ReleaseFn)(void*);
    if (g_world_release_a != nullptr) {
        ReleaseFn release = *reinterpret_cast<ReleaseFn*>(
            reinterpret_cast<uintptr_t>(*reinterpret_cast<void**>(g_world_release_a)) + 8);
        release(g_world_release_a);
        g_world_release_a = nullptr;
    }
    if (g_world_release_b != nullptr) {
        ReleaseFn release = *reinterpret_cast<ReleaseFn*>(
            reinterpret_cast<uintptr_t>(*reinterpret_cast<void**>(g_world_release_b)) + 8);
        release(g_world_release_b);
        g_world_release_b = nullptr;
    }
}

/* ================================================================== */
/* World_CheckActive                                                    */
/* Address: 0x44DBB0                                                    */
/* __thiscall (ECX = this)                                              */
/*                                                                      */
/* Returns 1 if vehicle_count >= 4 or local_vehicle_count >= 3          */
/* (world full).                                                        */
/*                                                                      */
/* Called by: DDRAW_UpdateBuildingSprites (0x459B31)                   */
/* ================================================================== */
char World::CheckActive(void)
{
    if (this->vehicle_count < 4 && this->local_vehicle_count < 3) {
        return 0;
    }
    return 1;
}

/* ================================================================== */
/* World_Reset                                                          */
/* Address: 0x44DBD0                                                    */
/* __thiscall (ECX = this, char flag on stack)                          */
/*                                                                      */
/* For all active vehicles in occupancy states 0, 1, 4, or 5 (at        */
/* +0x64), calls Vehicle::UpdatePosition with the given flag.           */
/* ================================================================== */
void World::Reset(char flag)
{
    for (int i = 0; i < 4; i++) {
        Vehicle* vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        int32_t occupancy = vehicle->occupancy;          /* +0x64 */
        if (occupancy == 0 || occupancy == 4 || occupancy == 5 || occupancy == 1) {
            vehicle->UpdatePosition(static_cast<uint8_t>(flag));
        }
    }
}

/* ================================================================== */
/* World_SaveToFile                                                     */
/* Address: 0x44D8A0                                                    */
/* __thiscall (ECX = this, resource_id, player_id, mp_flag)             */
/*                                                                      */
/* Finds a vehicle matching resource_id and player_id in the active     */
/* vehicle array. Deselects from town/DDRAW views if the selected-      */
/* building slot matches editors[0], notifies the network manager,      */
/* then deletes the vehicle.                                            */
/*                                                                      */
/* On owner_handle==0 (locally owned), decrements local_vehicle_count   */
/* first. In multiplayer scenario 2 with mp_flag set, dispatches         */
/* Netman::ReceiveGameStart instead.                                    */
/*                                                                      */
/* Returns true on success, false when no vehicle matched.              */
/* ================================================================== */
bool World::SaveToFile(uint resource_id, char player_id, char mp_flag)
{
    uint slot = 0;
    for (; slot < 4; ++slot) {
        Vehicle* candidate = this->vehicles[slot];
        if (candidate != nullptr && candidate->network_id == resource_id &&
            candidate->slot_index == static_cast<uint8_t>(player_id)) {
            break;
        }
    }
    if (slot >= 4) return false;

    Vehicle*& vehicle_slot = this->vehicles[slot];
    Vehicle* vehicle = vehicle_slot;

    /* Assembly compares the selected-building slot (0x485380, never written
       in loco.exe — always 0) against editors[0] as raw pointer values:
       0x44D8FA MOV ECX,[0x00485380] ; 0x44D900 CMP [ESI+0x10],ECX
       The deselect branch never fires in the shipped binary. The host
       g_selected_building symbol (Entity*, nullptr) carries the same name. */
    if (reinterpret_cast<Entity*>(vehicle->editors[0]) == g_selected_building) {
        Town_SelectBuilding(g_town_view, 0);
        DDRAW_SelectBuilding(g_ddraw_building, 0);
    }

    g_netman->HandleTimeout(vehicle);
    if (vehicle->owner_handle == 0) {
        --this->local_vehicle_count;                 /* +0x06 */
        vehicle->~Vehicle();
        GLOBAL_free(vehicle);
    } else if (g_netman != nullptr && g_netman->m_gameMode == 2 && mp_flag != 0) {
        g_netman->ReceiveGameStart(0, 0, vehicle);
    } else {
        vehicle->~Vehicle();
        GLOBAL_free(vehicle);
    }

    vehicle_slot = nullptr;
    --this->vehicle_count;
    return true;
}

/* ================================================================== */
/* World_SerializeObject                                                */
/* Address: 0x44DA50                                                    */
/* __thiscall (ECX = this, char player_id)                              */
/*                                                                      */
/* Finds vehicles owned by player_id. For each match: clears all 8      */
/* occupant slots, renders final state, and saves with mp_flag=0.       */
/* ================================================================== */
void World::SerializeObject(char player_id)
{
    for (int i = 0; i < 4; i++) {
        Vehicle* vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        /* Check player ID match */
        if (vehicle->slot_index != static_cast<uint8_t>(player_id)) {  /* +0x78 */
            continue;
        }

        /* Remove all 8 occupant slots */
        for (int j = 0; j < 8; j++) {
            if (vehicle->occupant_tracks[j] != 0) {
                reinterpret_cast<Building*>(vehicle->occupant_tracks[j])->RemoveOccupant();
                vehicle->occupant_tracks[j] = 0;
            }
        }

        /* Render final state and save */
        World_RenderAll(vehicle);
        this->SaveToFile(static_cast<uint>(vehicle->network_id),  /* +0x7A */
                         player_id,
                         0);                         /* mp_flag=0 */
    }
}

/* ================================================================== */
/* World_DeserializeMap                                                 */
/* Address: 0x44DAD0                                                    */
/* __thiscall (ECX = this, RESDATA_GameVehicle*)                        */
/*                                                                      */
/* For each active vehicle: checks any sub-object's bounding rect       */
/* against the game vehicle's rect. On overlap, clears occupants,       */
/* renders final state, and saves with mp_flag=1.                       */
/*                                                                      */
/* Skips vehicles where occupancy==2 unless vehicle_kind==4 (GameVehicle). */
/*                                                                      */
/* Called by: RESDATA_GameVehicle_BaseDtor (0x44B081) — passes `this`.  */
/* ================================================================== */
void World::DeserializeMap(RESDATA_GameVehicle* game_vehicle)
{
    for (int i = 0; i < 4; i++) {
        Vehicle* vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        /* Skip if vehicle occupancy == 2 and vehicle_kind != 4 (not a GameVehicle) */
        if (vehicle->occupancy == 2 && game_vehicle->vehicle_kind != 4) {
            continue;
        }

        /* Check for bounding-box overlap with any sub-object */
        bool overlap = false;
        RECT overlap_rect;
        for (int j = 0; j < 4; j++) {
            VehicleEditor* sub = vehicle->editors[j];
            if (sub == NULL) {
                continue;
            }

            if (!overlap) {
                /* Test rect overlap using Win32 IntersectRect.
                   Sub-object rect at +0x08, game vehicle rect at +0x08. */
                if (IntersectRect(&overlap_rect, &sub->screen_rect,
                                  &game_vehicle->screen_rect) == 0) {
                    continue;   /* No overlap — skip this sub-object */
                }
            }
            overlap = true;
        }

        if (overlap) {
            /* Clear all 8 occupant slots */
            for (int j = 0; j < 8; j++) {
                if (vehicle->occupant_tracks[j] != 0) {
                    reinterpret_cast<Building*>(vehicle->occupant_tracks[j])->RemoveOccupant();
                    vehicle->occupant_tracks[j] = 0;
                }
            }

            /* Render and save */
            World_RenderAll(vehicle);
            this->SaveToFile(static_cast<uint>(vehicle->network_id),   /* +0x7A */
                             static_cast<char>(vehicle->slot_index),   /* +0x78 */
                             1);                          /* mp_flag=1 */
        }
    }
}

/* ================================================================== */
/* World_LoadFromFile                                                   */
/* Address: 0x44DC10                                                    */
/* __thiscall (ECX = this, int* route_data, int* vehicle_init)          */
/*                                                                      */
/* Creates a new vehicle in the first empty world slot.                 */
/*                                                                      */
/* If vehicle_init is NULL: generates a random vehicle with a random    */
/* resource ID (0x1804, 0x1806, or 0x1808), a name built from the       */
/* player config name ("%s %lu"), and a random route (0-4 segments      */
/* pointing at 0x1866/8/A, 0x186C/0x186E, 0x1870).                      */
/*                                                                      */
/* If vehicle_init is non-NULL: creates a vehicle with resource ID      */
/* from vehicle_init[0], sets up route from vehicle_init[1..3], and     */
/* calls Vehicle::FindPath.                                             */
/*                                                                      */
/* On resource load failure (editors[0] == NULL or uninitialized):      */
/* destroys the vehicle and returns NULL.                               */
/*                                                                      */
/* Uses SEH (__try/__except) for operator_new calls in the original.    */
/* ================================================================== */
Vehicle* World::LoadFromFile(int* route_data, int* vehicle_init)
{
    uint slot;
    Vehicle* vehicle;
    bool do_register;

    /* Bounds check — world must not be full */
    if (static_cast<uint16_t>(this->vehicle_count) > 3 ||
        static_cast<uint16_t>(this->local_vehicle_count) > 2) {
        return NULL;
    }

    /* Find first empty vehicle slot. The entry guard guarantees one
       exists (<= 3 vehicles), so the original's fall-through read of
       vehicles[4] is unreachable; guard defensively anyway. */
    for (slot = 0; slot < 4; slot++) {
        if (this->vehicles[slot] == NULL) {
            break;
        }
    }
    if (slot >= 4) {
        return NULL;
    }

    do_register = false;

    if (vehicle_init == NULL) {
        /* ---- Random vehicle generation ---- */
        void* vehicle_mem = operator_new(0x94);
        if (vehicle_mem == NULL) {
            vehicle = NULL;
        } else {
            /* Random resource from {0x1804, 0x1806, 0x1808} */
            int resource_id = static_cast<int>(CRT_rand() % 3) * 2 + 0x1804;
            vehicle = new (vehicle_mem) Vehicle(resource_id, 0, 0, 0);
        }
        this->vehicles[slot] = vehicle;

        if (vehicle != NULL) {
            /* Verify the editor resource loaded (editors[0] initialized) */
            if (vehicle->editors[0] != NULL && vehicle->editors[0]->initialized == 1) {

                /* Assign random vehicle name:
                   CRT_sprintf(buf, 10, "%s %lu", g_player_config->name, id)
                   (format string at 0x47F030, player name at g_player_config+0x06) */
                char name_buf[12];
                CRT_sprintf(name_buf, 10, "%s %lu", g_player_config->name,
                            static_cast<unsigned long>(vehicle->network_id));
                vehicle->editors[0]->SetName(name_buf);

                /* Generate random route (0 to 4 segments) */
                int segments = static_cast<int>(CRT_rand() % 5);
                for (int i = 0; i < segments; i++) {
                    int choice = static_cast<int>(CRT_rand() % 3);
                    if (choice == 0) {
                        /* Destination type A: random from {0x1866, 0x1868, 0x186A} */
                        vehicle->InitRoute(static_cast<int>(CRT_rand() % 3) * 2 + 0x1866, 2, 0);
                    } else if (choice == 1) {
                        /* Destination type B: {0x186C, 0x186E} (assembly: (rand%2)*2+0x186C) */
                        vehicle->InitRoute(static_cast<int>(CRT_rand() % 2) * 2 + 0x186C, 3, 0);
                    } else {
                        /* Destination type C: 0x1870 */
                        vehicle->InitRoute(0x1870, 4, 0);
                    }
                }
                do_register = true;
            } else {
                /* Resource not loaded — destroy vehicle */
                vehicle->~Vehicle();
                GLOBAL_free(vehicle);
                this->vehicles[slot] = NULL;
            }
        }
    } else {
        /* ---- Specific vehicle from save data ---- */
        void* vehicle_mem = operator_new(0x94);
        if (vehicle_mem == NULL) {
            vehicle = NULL;
        } else {
            vehicle = new (vehicle_mem) Vehicle(*vehicle_init, 0, 0, 0);
        }
        this->vehicles[slot] = vehicle;

        if (vehicle != NULL) {
            /* Verify the editor resource loaded */
            if (vehicle->editors[0] == NULL || vehicle->editors[0]->initialized != 1) {
                /* Resource not loaded — destroy */
                vehicle->~Vehicle();
                GLOBAL_free(vehicle);
                this->vehicles[slot] = NULL;
            } else {
                /* Assign name */
                char name_buf[12];
                CRT_sprintf(name_buf, 10, "%s %lu", g_player_config->name,
                            static_cast<unsigned long>(vehicle->network_id));
                vehicle->editors[0]->SetName(name_buf);

                /* Set up initial position */
                vehicle->UpdatePosition(0);

                /* Set up route from save data (up to 3 route entries) */
                for (int i = 0; i < 3; i++) {
                    vehicle_init++;
                    if (*vehicle_init != 0) {
                        uint dir = CGWND_MapResourceToDirection(*vehicle_init);
                        vehicle->InitRoute(*vehicle_init, dir, 0);
                        vehicle->UpdatePosition(0);
                    }
                }
                do_register = true;
            }
        }
    }

    if (do_register) {
        /* Increment counters and find path */
        this->vehicle_count++;
        this->local_vehicle_count++;
        vehicle->FindPath(route_data, 1);
    }

    return this->vehicles[slot];
}

/* ================================================================== */
/* World_FinalizeLoad                                                   */
/* Address: 0x44DF40                                                    */
/* __thiscall (ECX = this, Vehicle*, int packed_coords, char mp_flag)   */
/*                                                                      */
/* Registers a fully-loaded vehicle in the first empty world slot.      */
/* Unpacks destination coordinates and links to a destination building  */
/* via ArrivalQueue_AddVehicle.                                         */
/*                                                                      */
/* In single player (netman scenario 0), finds destination via          */
/* INPUT_FindObjectAt(param=0). In multiplayer scenario 1, uses         */
/* INPUT_FindObjectAt(param=1). In multiplayer scenario 2, uses         */
/* TileMap_GetObjectAt with optional y-coord adjustment based on        */
/* mp_flag. Note: the original dereferences g_netman (no NULL check).   */
/*                                                                      */
/* Returns 1 on success, 0 if world is full or no empty slot.           */
/* ================================================================== */
char World::FinalizeLoad(Vehicle* vehicle, int packed_coords, char mp_flag)
{
    uint slot;
    short dest_x, dest_y;
    /* HelpPageNode* — see docs/landmine-sweep-worklist.md's ArrivalQueue
     * section. The INPUT_FindObjectAt(mode 0/1) branches are proven
     * HelpPageNode by the vehicle_kind==3 filter (only HelpPageNode's
     * constructor ever sets that value). The mp_gameMode==2 branch below
     * uses TileMap_GetObjectAt directly, which applies no kind filter at
     * all — weaker evidence, but the original binary funnels all three
     * paths into the same ArrivalQueue_AddVehicle call on `building`
     * unconditionally, so the type is kept uniform here rather than
     * introduced as a new special case. */
    HelpPageNode* building;

    /* Bounds check */
    if (this->vehicle_count > 3) {
        return 0;
    }

    /* Find first empty slot */
    for (slot = 0; slot < 4; slot++) {
        if (this->vehicles[slot] == NULL) {
            break;
        }
    }
    if (slot > 3) {
        return 0;
    }

    /* Register vehicle in slot */
    this->vehicles[slot] = vehicle;
    vehicle->net_sync_flag = 0;                       /* clear +0x68 */
    this->vehicle_count++;

    /* Unpack destination coordinates */
    dest_x = static_cast<short>(packed_coords & 0xFFFF);
    dest_y = static_cast<short>((packed_coords >> 16) & 0xFFFF);

    /* Determine destination based on network scenario.
       Assembly: MOV ECX,[0x004fd3ac]; MOV EAX,[ECX+0x7C4] — no NULL check. */
    if (g_netman->m_gameMode == 1) {
        /* Multiplayer scenario 1 */
        building = static_cast<HelpPageNode*>(INPUT_FindObjectAt(&g_input_mgr, 1));
    } else if (g_netman->m_gameMode == 2) {
        /* Multiplayer scenario 2: adjust destination Y based on mp_flag */
        if (mp_flag == 1 || mp_flag == 2) {
            dest_y++;
        }
        building = static_cast<HelpPageNode*>(TileMap_GetObjectAt(g_tilemap, dest_x, dest_y, 0));
    } else {
        /* Single player */
        building = static_cast<HelpPageNode*>(INPUT_FindObjectAt(&g_input_mgr, 0));
    }

    /* Fallback: try INPUT_FindObjectAt with param=1 */
    if (building == NULL) {
        building = static_cast<HelpPageNode*>(INPUT_FindObjectAt(&g_input_mgr, 1));
    }

    if (building == NULL) {
        /* No destination found — set vehicle to WAITING state */
        vehicle->SetState(3);
        return 1;
    }

    /* Link vehicle to destination building */
    building->AddVehicle(vehicle);
    return 1;
}

/* ================================================================== */
/* World_UpdateTick                                                     */
/* Address: 0x44E020                                                    */
/* __thiscall (ECX = this)                                              */
/*                                                                      */
/* Main per-tick update. Guards: vehicle_count > 0 AND game_mode in     */
/* {3, 9}. For each active vehicle:                                     */
/*   1. ProcessEvents — collision/overlap detection                     */
/*   2. VehicleEditor_Update — update editor positions                  */
/*   3. If vehicle state == 3:                                          */
/*        Clear all occupants, render, save                             */
/*   4. Else if net_sync_flag == 1:                                     */
/*        Pack destination coords (from editor_state->building+0x88),   */
/*        flag=2, clear occupants (owner_handle!=0), render,            */
/*        send network start, remove from world                         */
/* ================================================================== */
void World::UpdateTick(void)
{
    /* Early out if no vehicles or wrong game mode */
    if (this->vehicle_count == 0) {
        return;
    }
    if (g_game_mode != 3 && g_game_mode != 9) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        Vehicle* vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        /* Step 1: Collision/overlap detection */
        this->ProcessEvents(vehicle);

        /* Step 2: Update vehicle editor (position, animation) */
        VehicleEditor_Update(vehicle);

        /* Step 3: Check vehicle state */
        if (vehicle->state == 3) {
            /* Arrived — clear all occupants */
            for (int j = 0; j < 8; j++) {
                if (vehicle->occupant_tracks[j] != 0) {
                    reinterpret_cast<Building*>(vehicle->occupant_tracks[j])->RemoveOccupant();
                    vehicle->occupant_tracks[j] = 0;
                }
            }

            /* Render and save */
            World_RenderAll(vehicle);
            this->SaveToFile(static_cast<uint>(vehicle->network_id),
                             static_cast<char>(vehicle->slot_index),
                             1);

        } else if (vehicle->net_sync_flag == 1) {
            /* Network sync pending.
               Assembly chain: vehicle+0x20 (editor_state) -> +0x14
               (building) -> +0x88 (packed tile coords). EditorState::building
               is GameVehicle* (world/EditorState.h) — the three-time-deferred
               retype this file's own comment used to flag is now resolved.
               +0x88 is RESDATA_GameVehicle's inherited sub_pos_x/sub_pos_y
               pair, exposed directly by its own tile_target() accessor
               (game/ResdataGameVehicle.h). */
            int packed_coords = vehicle->editor_state->building->tile_target();

            /* Mark as synced */
            vehicle->net_sync_flag = 2;

            /* Clear occupants if locally-owned check (owner_handle != 0) */
            if (vehicle->owner_handle != 0) {
                for (int j = 0; j < 8; j++) {
                    if (vehicle->occupant_tracks[j] != 0) {
                        reinterpret_cast<Building*>(vehicle->occupant_tracks[j])->RemoveOccupant();
                        vehicle->occupant_tracks[j] = 0;
                    }
                }
            }

            /* Render final state */
            World_RenderAll(vehicle);

            /* Remove vehicle from world */
            this->vehicles[i] = NULL;
            this->vehicle_count--;

            /* Notify network manager with unpacked coords */
            g_netman->ReceiveGameStart(
                static_cast<int16_t>(packed_coords & 0xFFFF),
                static_cast<int16_t>((packed_coords >> 16) & 0xFFFF),
                vehicle);
        }
    }
}

/* ================================================================== */
/* World_ProcessEvents                                                  */
/* Address: 0x44E3F0                                                    */
/* __thiscall (ECX = this, Vehicle* current_vehicle)                    */
/*                                                                      */
/* Collision detection between current_vehicle and all other vehicles.  */
/*                                                                      */
/* For each other vehicle's sub-objects: only sub-objects whose         */
/* end_a/end_b editor states reference the same building as the         */
/* current vehicle's editor state are tested. The sub-object rect is    */
/* tested against the current editor-state position (pos_x/pos_y),      */
/* guarded by editor_state->edit_state != 2, then Town_BlitViewport     */
/* checks visual overlap.                                               */
/*                                                                      */
/* On collision: stops both vehicles (state=4), sets random 1-100 tick  */
/* wait timer, shows collision message box (resource 0x3861).           */
/*                                                                      */
/* Guard conditions (skip collision test if):                           */
/*   - Fewer than 2 vehicles in world                                   */
/*   - current_vehicle is NULL or not MOVING (state != 2)               */
/*   - direction is 2 (edge) or 3 (depot)                               */
/*   - occupancy is 2 (full)                                            */
/*   - Other vehicle direction 2/3, occupancy 2                         */
/*   - Other vehicle stopped (state==4) with stop_timer==0              */
/*   - Other vehicle is current_vehicle                                 */
/* ================================================================== */
uint World::ProcessEvents(Vehicle* current_vehicle)
{
    /* Early outs */
    if (this->vehicle_count <= 1)                        return 0;
    if (current_vehicle == NULL)                         return 0;
    if (current_vehicle->state != 2)                     return 0;  /* not MOVING */
    if (current_vehicle->direction == 2 ||
        current_vehicle->direction == 3)                 return 0;  /* edge/depot */
    if (current_vehicle->occupancy == 2)                 return 0;  /* full */

    EditorState* current_es = current_vehicle->editor_state;        /* +0x20 */

    for (int i = 0; i < 4; i++) {
        Vehicle* other = this->vehicles[i];
        if (other == NULL)                               continue;
        if (other->direction == 2 || other->direction == 3)  continue;
        if (other->occupancy == 2)                       continue;
        if (other->state == 4 && other->stop_timer == 0) continue;  /* stopped+idle */
        if (other == current_vehicle)                    continue;  /* skip self */

        /* Check all sub-objects of the other vehicle */
        for (int j = 0; j <= static_cast<int>(other->editor_count); j++) {
            VehicleEditor* sub = other->editors[j];                /* +0x10 + j*4 */

            /* Only test sub-objects whose editor states reference the
               SAME building as the current vehicle's editor state
               (assembly: JZ into overlap test when equal). */
            if (sub->end_a->building != current_es->building &&
                sub->end_b->building != current_es->building) {
                continue;
            }

            /* Bounding-box overlap: sub-object rect vs current
               editor-state position (assembly compares +0x08..+0x14 of
               the sub against current_es->pos_x/pos_y at +0x0C/+0x10). */
            if (!(sub->screen_rect.left < current_es->pos_x &&
                  current_es->pos_x < sub->screen_rect.right &&
                  sub->screen_rect.top < current_es->pos_y &&
                  current_es->pos_y < sub->screen_rect.bottom)) {
                continue;
            }

            /* Assembly: CMP [current_vehicle+0x20]+0x1C, 2 — the current
               editor state's edit_state must not be 2 (wrap mode). */
            if (current_es->edit_state == 2) {
                continue;
            }

            /* Visual overlap check via Town_BlitViewport.
               sub->resource (Entity::resource, +0x40) is a RESDATA*
               (shared/types.h) here: +0x14 is RESDATA::frame_width, a
               named uint16_t field used directly below — a clean,
               width-correct fit.
               +0x10 is RESDATA::flags, documented there as "also aliased
               as ui_panel (UIPANEL*) in some contexts": the ORIGINAL
               32-bit binary packs a full pointer into that one 4-byte
               dword. This host read is NOT equivalent to the binary's:
               `*reinterpret_cast<UIPANEL_Surface**>(...)` reads a native
               8-byte pointer starting at +0x10, spanning `flags` (+0x10..+0x13)
               AND `frame_width`/`frame_height` (+0x14..+0x17) — three
               unrelated fields reinterpreted as one pointer. Left
               unchanged (pre-existing before this cast-style pass, and
               this collision path isn't exercised by any current test),
               but flagged as a real pointer-width landmine candidate if
               this branch is ever live with a genuine cached surface
               pointer — see input/Cursor_internal.h's RESDATA_GetSurface
               for the sanctioned typed-vtable-bridge pattern to use
               instead of a raw offset if this needs a real accessor. */
            RESDATA* sub_res = static_cast<RESDATA*>(sub->resource);
            uint32_t viewport_result = Town_BlitViewport(
                *reinterpret_cast<UIPANEL_Surface**>(reinterpret_cast<uint8_t*>(sub_res) + 0x10),
                sub->source_rect.left,                            /* +0x30 */
                sub->source_rect.top,                             /* +0x34 */
                sub->source_rect.right,                           /* +0x38 */
                sub->source_rect.bottom,                          /* +0x3C */
                (static_cast<uint>(sub_res->frame_width) *
                     static_cast<uint>(sub->angle_frame) -         /* +0x438 */
                 sub->screen_rect.left) + current_es->pos_x,      /* +0x0C */
                current_es->pos_y - sub->screen_rect.top);        /* +0x10 - +0x0C */

            if (viewport_result == 0) {
                /* COLLISION DETECTED */

                /* Stop current vehicle */
                current_vehicle->SetState(4);                     /* STOPPED */
                current_vehicle->stop_timer = static_cast<int32_t>(CRT_rand() % 100) + 1;

                /* Show collision message box */
                UI_CreateMessageBox(g_tooltip_mgr, 0x3861, 0, 'W',
                                    current_es->pos_x, current_es->pos_y, 1);

                /* Also stop the other vehicle if it's active */
                if (other->state != 0) {
                    other->SetState(4);
                    other->stop_timer = static_cast<int32_t>(CRT_rand() % 100) + 1;
                }
                break;
            }
        }
    }
    return 0;
}

/* ================================================================== */
/* World_ProcessAudio                                                  */
/* Address: 0x44E830                                                    */
/* __thiscall (ECX = this, int audio_x, int audio_y)                   */
/*                                                                      */
/* Audio hit-test: checks each active vehicle's sub-objects against     */
/* the given (audio_x, audio_y) position via GameObject::PtInRect       */
/* (vtable[2], thiscall). On match, selects the object in the town      */
/* view. Guarded by g_click_on_town flag.                               */
/* ================================================================== */
char World::ProcessAudio(int audio_x, int audio_y)
{
    char result = 0;

    if (g_click_on_town == 0) {
        return 0;
    }

    for (int i = 0; i < 4; i++) {
        Vehicle* vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        for (int j = 0; j <= static_cast<int>(vehicle->editor_count); j++) {
            VehicleEditor* sub = vehicle->editors[j];
            if (sub == NULL) {
                continue;
            }

            /* Check if sub-object is visible (+0x24) */
            if (sub->visible == 1) {
                /* vtable[2] = GameObject::PtInRect(thiscall) */
                if (sub->PtInRect(audio_x, audio_y) != 0) {
                    Town_SelectBuilding(g_town_view,
                        static_cast<int>(reinterpret_cast<uintptr_t>(vehicle->editors[j])));
                    result = 1;
                    break;
                }
            }
        }
    }
    return result;
}

/* ================================================================== */
/* World_InitTimer                                                      */
/* Address: 0x44E160                                                    */
/* __thiscall (ECX = this, int object_ptr)                              */
/*                                                                      */
/* Checks if any active vehicle editor state references the given      */
/* object pointer. Returns 1 if found. Used to determine if world       */
/* timers should remain active.                                         */
/*                                                                      */
/* Note: the parameter is a pointer value (the caller passes a          */
/* RESDATA_GameVehicle*); the original source compares it as an int.    */
/* ================================================================== */
char World::InitTimer(int object_ptr)
{
    for (int i = 0; i < 4; i++) {
        Vehicle* vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        /* Check main editor state's building pointer.
           object_ptr is a truncating int comparison against a pointer, per
           World.h's own doc: the real caller (RESDATA_GameVehicle_InitSounds,
           per Ghidra — not yet present in this C++ tree) passes a
           RESDATA_GameVehicle*, consistent with EditorState::building now
           being declared GameVehicle* (world/EditorState.h). The truncation
           itself is a pre-existing signature-level property of this function
           (not introduced here) — preserved as-is; only the cast style
           changes. */
        if (vehicle->editor_state != NULL &&
            static_cast<int32_t>(reinterpret_cast<intptr_t>(vehicle->editor_state->building)) == object_ptr) {
            return 1;
        }

        /* Check array sub-objects' editor states */
        for (int j = 0; j <= static_cast<int>(vehicle->editor_count); j++) {
            VehicleEditor* sub = vehicle->editors[j];

            /* Check editor states for matching building pointer.
               (Assembly dereferences end_a/end_b without null checks.) */
            if (static_cast<int32_t>(reinterpret_cast<intptr_t>(sub->end_a->building)) == object_ptr ||
                static_cast<int32_t>(reinterpret_cast<intptr_t>(sub->end_b->building)) == object_ptr) {
                return 1;
            }
        }
    }
    return 0;
}

/* ================================================================== */
/* World_Lock                                                           */
/* Address: 0x44E200                                                    */
/* __thiscall (ECX = this)                                              */
/*                                                                      */
/* Collects non-excluded sub-objects from all active vehicles and       */
/* bubble-sorts them by depth priority (z-order).                       */
/*                                                                      */
/* Exclusion conditions (skip sub-object if):                           */
/*   - edge_dir_b (+0x444) == 2 or == 5                                */
/*   - edge_dir_a (+0x440) == 2                                         */
/*                                                                      */
/* Sorting is bubble-sort by depth (via end_a editor state +0x10).      */
/*                                                                      */
/* Called by: TileMap_InvalidateDirtyRects (0x45619A)                  */
/* ================================================================== */
void World::Lock(void)
{
    int slot_count = 0;

    if (this->vehicle_count == 0) {
        return;
    }

    /* Step 1: Collect non-excluded sub-objects */
    for (int i = 0; i < 4; i++) {
        Vehicle* vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        for (int sub_idx = 0; sub_idx <= static_cast<int>(vehicle->editor_count); sub_idx++) {
            VehicleEditor* sub = vehicle->editors[sub_idx];

            /* Skip excluded types (assembly dereferences sub directly) */
            if (sub->edge_dir_b != 2 && sub->edge_dir_b != 5 &&
                sub->edge_dir_a != 2) {
                this->sub_objects[slot_count] = sub;
                slot_count++;
            }
        }
    }

    if (slot_count == 0) {
        return;
    }

    /* Step 2: Bubble-sort by depth order (ascending = lower depth first).
       Assembly iterates at most 15 adjacent pairs per pass and stops at
       the first NULL entry; only obj_b is null-checked. */
    bool swapped;
    do {
        swapped = false;
        for (int i = 0; i < 15; i++) {
            VehicleEditor* obj_b = this->sub_objects[i + 1];
            if (obj_b == NULL) {
                break;
            }
            VehicleEditor* obj_a = this->sub_objects[i];

            /* Depth via end_a editor state pos_y (+0x430 -> +0x10) */
            int depth_a = obj_a->end_a->pos_y;
            int depth_b = obj_b->end_a->pos_y;
            if (depth_b < depth_a) {
                /* Swap */
                this->sub_objects[i]     = obj_b;
                this->sub_objects[i + 1] = obj_a;
                swapped = true;
            }
        }
    } while (swapped);
}

/* ================================================================== */
/* World_Unlock                                                         */
/* Address: 0x44E2D0                                                    */
/* __thiscall (ECX = this)                                              */
/*                                                                      */
/* Clears all 16 entries in the sub_objects[] array (resets to NULL).   */
/*                                                                      */
/* Called by: TileMap_InvalidateDirtyRects (0x4566E9)                  */
/* ================================================================== */
void World::Unlock(void)
{
    for (int i = 0; i < 16; i++) {
        this->sub_objects[i] = NULL;
    }
}

/* ================================================================== */
/* World_InvalidateRect                                                 */
/* Address: 0x44E2E0                                                    */
/* __thiscall (ECX = this, int x, int y, int param3, int param4,       */
/*            short scroll_stop)                                        */
/*                                                                      */
/* Iterates the depth-sorted sub_objects[] array. For each sub-object   */
/* within bounds at (x, y): blits background via                       */
/* VehicleEditor::BlitBackground.                                      */
/*                                                                      */
/* When scroll_stop == 1: also checks the next adjacent sub-object      */
/* with the same target_building (+0x44C) and zero bound_check_flag     */
/* (+0x448). If within bounds, blits its background as well.            */
/*                                                                      */
/* Note: param3 and param4 are present on the stack but never read.     */
/* ================================================================== */
void World::InvalidateRect(int x, int y, int param3, int param4, short scroll_stop)
{
    /* UNUSED: param3 and param4 are stack padding/unused parameters */
    (void)param3;
    (void)param4;

    for (int i = 0; i < 16; i++) {
        VehicleEditor* sub_obj = this->sub_objects[i];
        if (sub_obj == NULL) {
            return;
        }

        /* Check if sub-object bounds include this tile.
           (Binary passes x, y, scroll_stop plus an unused padding arg.) */
        if (sub_obj->IsInBounds(static_cast<short>(x), static_cast<short>(y), scroll_stop) != 0) {
            sub_obj->BlitBackground(x, y);

            /* If scroll_stop == 1, also check adjacent sub-objects with
               the same target_building */
            if (scroll_stop == 1) {
                int j = i + 1;
                while (j < 16) {
                    VehicleEditor* next = this->sub_objects[j];
                    if (next == NULL) {
                        break;
                    }

                    /* Check same target_building (+0x44C) and zero
                       bound_check_flag (+0x448) */
                    if (next->target_building == sub_obj->target_building &&
                        next->bound_check_flag == 0) {
                        if (next->IsInBounds(static_cast<short>(x), static_cast<short>(y), 0) != 0) {
                            next->BlitBackground(x, y);
                        }
                        break;
                    }
                    j++;
                }
            }
        }
    }
}

/* ================================================================== */
/* World_SerializeMap (free function — NOT a World method)             */
/* Address: 0x44DEA0                                                    */
/* __stdcall (GameVehicle* game_vehicle, int* route_data)               */
/*                                                                      */
/* Configures route data on a game-vehicle occupant. If the             */
/* game vehicle's occupant_state (+0x11C) is 1 and current_vehicle      */
/* (+0x120) exists:                                                     */
/*   1. If resource_id doesn't match, sets new resource on sub-obj     */
/*      via vtable[15] (VehicleEditor resource setter, 0x40E0F0)       */
/*   2. Removes 3 old route entries (Vehicle::RemoveEditor)            */
/*   3. Installs up to 3 new route points from route_data[1..3]        */
/*   4. Calls Vehicle::FindPath                                        */
/*                                                                      */
/* Called from: town building-purchase path (0x45B01A)                  */
/* ================================================================== */
bool __stdcall World_SerializeMap(GameVehicle* game_vehicle, int* route_data)
{
    if (game_vehicle->occupant_state != 1) {                 /* +0x11C */
        return false;
    }
    Vehicle* vehicle = game_vehicle->current_vehicle;        /* +0x120 */
    if (vehicle == NULL) {
        return false;
    }

    int initial_resource = *route_data;
    int current_resource = static_cast<int>(vehicle->editors[0]->GetResourceId());

    bool result = (current_resource != initial_resource);

    if (result) {
        /* VehicleEditor::SetResourceId — vtable[15] (+0x3C), 0x40E0F0.
           See core/VehicleEditor.h's vtable layout comment: this is the
           sole call site (confirmed via get_xrefs_to(0x40E0F0) — only the
           vtable data itself references it, i.e. it was never called
           directly).

           BUG FIX, not just a cast-style change: the previous code read
           the byte offset +0x3C off *this TU's own host GCC vtable
           pointer* (`*(void**)vehicle->editors[0]`) — the x86 binary's
           vtable-slot byte offset applied to an unrelated, differently-
           laid-out host vtable of 8-byte entries. 0x3C/8 = 7.5, i.e. the
           computed address was misaligned, straddling two real host
           vtable entries — calling through it would read half of one
           function pointer and half of the next as if they were one
           pointer, then jump there. This path is reachable from real
           gameplay (called from the town building-purchase path per the
           function doc above), so it was a live, always-wrong host bug,
           not dead code. Fixed by dispatching through the named C++
           virtual method instead of any raw vtable arithmetic. */
        vehicle->editors[0]->SetResourceId(initial_resource, -1);
    }

    /* Remove 3 old route entries */
    for (int i = 0; i < 3; i++) {
        vehicle->RemoveEditor(1);
    }

    /* Install new route entries from route_data[1..3] */
    for (int i = 0; i < 3; i++) {
        route_data++;
        if (*route_data != 0) {
            uint dir = CGWND_MapResourceToDirection(*route_data);
            vehicle->InitRoute(*route_data, dir, 0);
            result = true;
        }
    }

    /* Re-path toward the game vehicle */
    vehicle->FindPath(reinterpret_cast<int32_t*>(game_vehicle), 1);
    return result;
}

/* ================================================================== */
/* World_RenderAll (free function — NOT a World method)                */
/* Address: 0x44E630                                                    */
/* __stdcall (Vehicle* vehicle)                                         */
/*                                                                      */
/* MISNAMED: This does NOT render anything. It handles vehicle          */
/* arrival at its destination.                                          */
/*                                                                      */
/* Steps:                                                              */
/*   1. Vehicle::UpdatePosition(0)                                     */
/*   2. Find destination HelpPageNode at current tile (tile_y+1)       */
/*   3. If found:                                                      */
/*      - direction 2 or 3: HelpPageNode::RemoveVehicle, fall thru    */
/*      - direction 0,1,2,3,4,5: clear occupant_state (+0x11C)        */
/*   4. Clear current tile coords                                      */
/*   5. If occupancy in {1,2,4,5}:                                     */
/*      - Find destination GameVehicle at target tile (target_y+1)    */
/*      - If occupancy == 2: RemoveDestination                        */
/*      - If current_vehicle matches: clear tracking, StopSound(0)    */
/*   6. Clear destination coords                                       */
/*   7. StopSound(1) on editor-state buildings with vehicle_kind==7    */
/*   8. EditorState::Detach on all sub-obj editor states               */
/* ================================================================== */
void __stdcall World_RenderAll(Vehicle* vehicle)
{
    if (vehicle == NULL) {
        return;
    }

    /* Step 1: Update position */
    vehicle->UpdatePosition(0);

    /* Step 2: Find destination node at current tile.
     * HelpPageNode* — see docs/landmine-sweep-worklist.md's ArrivalQueue
     * section. TileMap::GetObjectAt applies no kind filter here; this
     * rests on direction==2 being set exclusively by
     * HelpPageNode::AddVehicle, not on a runtime type check. A bare
     * RESDATA_GameVehicle (0x11C bytes) reaching this call would read
     * +0x124 out of its own allocation — an original-binary hazard the
     * assembly does not guard against. */
    HelpPageNode* building = static_cast<HelpPageNode*>(TileMap_GetObjectAt(
        g_tilemap, vehicle->tile_x, static_cast<short>(vehicle->tile_y + 1), 0));

    if (building != NULL) {
        switch (vehicle->direction) {                 /* +0x60 */
        case 2:   /* EDGE_OF_MAP */
        case 3:   /* DEPOT */
            building->RemoveVehicle(
                vehicle->network_id, vehicle->slot_index);
            /* Fall through to clear occupancy state */
        case 0:   /* FORWARD */
        case 1:   /* REVERSE */
        case 4:   /* ALT_FRONT */
        case 5:
            building->update_flag = 0;                /* +0x11C */
            break;
        }
    }

    /* Step 4: Clear current tile coords */
    vehicle->tile_x = -1;
    vehicle->tile_y = -1;

    /* Step 5: Check destination tile (second set of coords) */
    switch (vehicle->occupancy) {                     /* +0x64 */
    case 1:   /* DEPARTING */
    case 2:   /* FULL */
    case 4:   /* STOPPING */
    case 5:   /* ARRIVING */
    {
        GameVehicle* dest_building = static_cast<GameVehicle*>(TileMap_GetObjectAt(
            g_tilemap, vehicle->target_tile_x, static_cast<short>(vehicle->target_tile_y + 1), 0));

        if (dest_building != NULL) {
            if (vehicle->occupancy == 2) {
                dest_building->RemoveDestination(
                    vehicle->network_id, vehicle->slot_index);
            }

            /* Clear tracked vehicle on destination building */
            if (dest_building->current_vehicle == vehicle) {
                dest_building->current_vehicle = nullptr;   /* +0x120 */
                dest_building->busy_flag = 0;               /* +0x128 */
                dest_building->StopSound(0);
                dest_building->occupant_state = 0;          /* +0x11C */
            }
        }
        break;
    }
    }

    /* Clear destination coords */
    vehicle->target_tile_x = -1;
    vehicle->target_tile_y = -1;

    /* Step 7: StopSound(1) on editor-state buildings with vehicle_kind==7.
       vehicle->editor_state->building is GameVehicle* (world/EditorState.h). */
    {
        GameVehicle* main_building = vehicle->editor_state->building;
        if (main_building != NULL && main_building->vehicle_kind == 7) {
            main_building->StopSound(1);
        }
    }

    /* Iterate sub-objects and detach editor visual states */
    for (int i = 0; i <= static_cast<int>(vehicle->editor_count); i++) {
        VehicleEditor* sub = vehicle->editors[i];

        /* Detach via editor state 1's building */
        GameVehicle* editor_1 = sub->end_a->building;
        if (editor_1 != NULL && editor_1->vehicle_kind == 7) {
            editor_1->StopSound(1);
        }

        /* Detach via editor state 2's building */
        GameVehicle* editor_2 = sub->end_b->building;
        if (editor_2 != NULL && editor_2->vehicle_kind == 7) {
            editor_2->StopSound(1);
        }
    }

    /* Step 8: Detach via EditorState::Detach (second pass) */
    for (int i = 0; i <= static_cast<int>(vehicle->editor_count); i++) {
        VehicleEditor* sub = vehicle->editors[i];
        if (sub != NULL) {
            sub->end_a->Detach();
            sub->end_b->Detach();
        }
    }

    /* Detach the vehicle's own editor state (+0x20) */
    if (vehicle->editor_state != NULL) {
        vehicle->editor_state->Detach();
    }
}

/* ================================================================== */
/* World_GetObjectAt (free function — NOT a World method)              */
/* Address: 0x44E800                                                    */
/* __stdcall (Vehicle* vehicle)                                         */
/*                                                                      */
/* Simple helper: removes all 8 occupant slots at                       */
/* vehicle->occupant_tracks (+0x38..+0x54) via Building::RemoveOccupant. */
/*                                                                      */
/* Called by: NETMAN_ReceiveGameStart (0x43E56E), Town_SendPostcard     */
/*            (0x42D86F), and two town paths (0x45AE2E, 0x462C16).     */
/* ================================================================== */
void __stdcall World_GetObjectAt(Vehicle* vehicle)
{
    for (int i = 0; i < 8; i++) {
        if (vehicle->occupant_tracks[i] != 0) {
            reinterpret_cast<Building*>(vehicle->occupant_tracks[i])->RemoveOccupant();
            vehicle->occupant_tracks[i] = 0;
        }
    }
}

/* ================================================================== */
/* Typed wrapper for TileMap::ProcessRect's World dispatch call.       */
/* Declared in world/tilemap.h; implemented here (not in tilemap.cpp) */
/* to avoid pulling this file's own headers into that one. Real        */
/* signature verified against InvalidateRect's own RET immediate.     */
/* ================================================================== */
extern void* g_world; /* 0x4A98B0 */

/* Forward declarations matching world/tilemap.h's canonical extern
   declarations exactly (not including that header here — it declares its
   own conflicting g_* globals; see the file comment above). Required by
   -Werror=missing-declarations since these definitions are the first
   thing in this TU to mention them. */
void World_InvalidateRect(int x, int y, int w, int h, short type);
void World_Lock(void* world);
void World_Unlock(void* world);

void World_InvalidateRect(int x, int y, int w, int h, short type)
{
    static_cast<World*>(g_world)->InvalidateRect(x, y, w, h, type);
}

void World_Lock(void* world)
{
    static_cast<World*>(world)->Lock();
}

void World_Unlock(void* world)
{
    static_cast<World*>(world)->Unlock();
}
