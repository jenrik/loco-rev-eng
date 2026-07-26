/**
 * World.cpp — Top-level game world manager implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the World singleton class. All methods are non-virtual
 * __thiscall (ECX = this = World*). Three related free functions
 * (World_SerializeMap, World_RenderAll, World_GetObjectAt) operate
 * on building/vehicle objects rather than the World singleton.
 *
 * The World singleton lives at g_world (0x4A98B0). It is NOT
 * dynamically allocated — it's a global struct in the .data section.
 */

#include "World.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include "../shared/types.h"

#include <cstdint>

/* ================================================================== */
/* External function declarations                                      */
/* ================================================================== */

void*   __cdecl operator_new(size_t size);          /* @ 0x465CE0  operator new */

extern "C" {
    /* CRT / memory management */
    int     __cdecl CRT_rand(void);                      /* @ 0x466150  rand() */
    int     __cdecl CRT_sprintf(char* buf, int max_len,
                                const char* fmt, ...);    /* @ 0x467FF0  sprintf wrapper */

    /* Windows API */
    int     __stdcall IntersectRect(RECT* dst, RECT* src1, RECT* src2); /* @ 0x47726C via IAT */
}

/* Game functions */
extern void   __cdecl Building_RemoveOccupant(int* occupant);             /* @ 0x4336A0 */
extern void   __thiscall VehicleEditor_Update(void* vehicle);             /* @ 0x44C3A0 */
extern void   __thiscall Vehicle_UpdatePosition(void* vehicle, char flag);/* @ 0x44D500 */
extern void   __thiscall Vehicle_SetState(void* vehicle, int state);      /* @ 0x44CF90 */
extern void   __thiscall NETMAN_HandleTimeout(void* netman, void* vehicle); /* @ 0x4408B0 */
extern uint32_t __cdecl NETMAN_ReceiveGameStart(void* netman, int x,
                                                       int y, void* vehicle);     /* @ 0x43E560 */
extern void   __thiscall Town_SelectBuilding(void* town_view,
                                             int building);               /* @ 0x42C040 */
extern void   __thiscall DDRAW_SelectBuilding(void* ddraw_building,
                                              int building);              /* @ 0x459180 */
extern void*  __thiscall Vehicle_Ctor(void* mem, int resource_id,
                                       int dir, char flag1, char flag2);   /* @ 0x44BE50 */
extern void   __thiscall Vehicle_InitRoute(void* vehicle, int resource_id,
                                           uint dir, char flag);          /* @ 0x44C220 */
extern void   __thiscall Vehicle_FindPath(void* vehicle,
                                          int* route, char flag);         /* @ 0x44C170 */
extern int    __thiscall VehicleEditor_GetResourceId(int sub_obj);        /* @ 0x40E0D0 */
extern void   __thiscall VehicleEditor_RemoveVehicle(void* vehicle,
                                                     int param);          /* @ 0x44C310 */
extern uint   __stdcall CGWND_MapResourceToDirection(int resource_id);    /* @ 0x40EB60 */
extern void*  __thiscall TileMap_GetObjectAt(void* tilemap, short x,
                                             short y, short layer);       /* @ 0x455620 */
extern void*  __thiscall INPUT_FindObjectAt(void* input_mgr, int param);  /* @ 0x41E1F0 */
extern void   __thiscall ArrivalQueue_AddVehicle(void* building,
                                                 void* vehicle);          /* @ 0x44F3A0 */
extern void   __thiscall ArrivalQueue_RemoveVehicle(void* building,
                                                     uint resource_id,
                                                     char player_id);     /* @ 0x44F410 */
extern void   __thiscall GameVehicle_RemoveDestination(void* building,
                                                        uint resource_id,
                                                        char player_id);  /* @ 0x412B50 */
extern void   __cdecl UI_CreateMessageBox(void* mgr, int res_id, int a,
                                          char b, int c, int d, char e); /* @ 0x428A00 */
extern int    __cdecl Town_BlitViewport(void* viewport, int a, int b,
                                        int c, int d, int e, int f);     /* @ 0x42D1C0 */
extern char   __cdecl GAMESTATE_EditorState_Detach(int editor_state);     /* @ 0x40B5A0 */
extern char   __stdcall VehicleEditor_IsInBounds(void* sub_obj, short x,
                                                  short y, short param);  /* @ 0x44ABD0 */
extern void   __stdcall VehicleEditor_BlitBackground(void* sub_obj,
                                                      int x, int y);      /* @ 0x44ADD0 */

/* Forward-declare free functions implemented below */
extern void __stdcall World_RenderAll(void* vehicle);    /* @ 0x44E630 */
extern void __stdcall World_GetObjectAt(void* obj);      /* @ 0x44E800 */

/* ================================================================== */
/* Global variables referenced by World methods                        */
/* ================================================================== */

extern int32_t    g_game_mode;                /* 0x004851F4 */
extern void*      g_netman;                   /* 0x004FD3AC */
extern void*      g_town_view;                /* 0x004852A0 */
extern void*      g_ddraw_building;           /* 0x004A9EF0 */
extern void*      g_input_mgr;                /* 0x004A9990 */
extern void*      g_tilemap;                  /* 0x004AAD08 */
extern void*      g_tooltip_mgr;              /* 0x0048526C */
extern uint8_t    g_click_on_town;            /* 0x0048557C */
extern int32_t    g_selected_building;        /* 0x00485380 */
extern void*      DAT_00485268;               /* 0x00485268 — global object released in Init */
extern void*      DAT_0048526c;               /* 0x0048526c — global object released in Init */

/* ================================================================== */
/* Vehicle field offset constants (shared knowledge)                   */
/* ================================================================== */
#define VEHICLE_OFF_TYPE           0x04  /* int32   vehicle sub-type */
#define VEHICLE_OFF_SUB_OBJ_COUNT  0x0C  /* uint16  number of sub-objects */
#define VEHICLE_OFF_SUB_OBJ_ARRAY  0x10  /* void**  sub-object pointer array */
#define VEHICLE_OFF_RESOURCE_DATA  0x10  /* void*   resource data pointer (overlaps) */
#define VEHICLE_OFF_SUB_OBJ_MAIN   0x20  /* void*   main sub-object pointer */
#define VEHICLE_OFF_TIMER_WAIT     0x28  /* int32   collision wait timer */
#define VEHICLE_OFF_TILE_X         0x2E  /* int16   current tile X */
#define VEHICLE_OFF_TILE_Y         0x30  /* int16   current tile Y */
#define VEHICLE_OFF_DEST_X         0x32  /* int16   destination tile X */
#define VEHICLE_OFF_DEST_Y         0x34  /* int16   destination tile Y */
#define VEHICLE_OFF_OCCUPANTS      0x38  /* int*[8] occupant slots (8 x 4 bytes) */
#define VEHICLE_OFF_VEHICLE_STATE  0x5C  /* int32   state (0=idle, 2=moving, 3=arrived, 4=stopped) */
#define VEHICLE_OFF_ACTION_STATE   0x60  /* int32   action state (0-5) */
#define VEHICLE_OFF_STATE_2        0x64  /* int32   secondary state (0-5) */
#define VEHICLE_OFF_NET_SYNC_FLAG  0x68  /* int32   0=none, 1=pending, 2=synced */
#define VEHICLE_OFF_PLAYER_ID      0x78  /* char    owner/player ID */
#define VEHICLE_OFF_RESOURCE_ID    0x7A  /* uint16  vehicle resource ID */

/* Sub-object field offset constants */
#define SUB_OBJ_OFF_VTABLE         0x00  /* void*   vtable pointer */
#define SUB_OBJ_OFF_RECT           0x08  /* RECT    bounding rectangle (16 bytes) */
#define SUB_OBJ_OFF_ACTIVE_FLAG    0x24  /* char    visible/active flag */
#define SUB_OBJ_OFF_EDITOR_STATE_1 0x430 /* void*   first editor state */
#define SUB_OBJ_OFF_EDITOR_STATE_2 0x434 /* void*   second editor state */
#define SUB_OBJ_OFF_DEPTH          0x44C /* int32   depth/layer identifier */
#define SUB_OBJ_OFF_CONNECTIVITY   0x448 /* int16   connectivity flag */
#define SUB_OBJ_OFF_EXCLUSION_FLAG 0x440 /* int32   exclusion flag (2=excluded) */
#define SUB_OBJ_OFF_EXCLUSION_FLAG2 0x444 /* int32   secondary exclusion flag (2 or 5=excluded) */

/* Building field offset constants */
#define BUILDING_OFF_OCCUPANCY     0x11C /* int32   occupancy/arrival queue state */
#define BUILDING_OFF_TRACKED_VEH   0x120 /* void*   tracked vehicle pointer */
#define BUILDING_OFF_TRACKED_FLAG  0x128 /* char    tracked vehicle flag */
#define BUILDING_OFF_EDITOR_1      0x430 /* void*   first editor state */
#define BUILDING_OFF_STATE_CODE    0x10C /* int32   state code (4=normal) */

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
    int i;

    /* Clear counters and vehicle slots (+0x04 through +0x14) */
    this->vehicle_count = 0;           /* +0x04 */
    this->field_06 = 0;                /* +0x06 */
    this->vehicles[0] = NULL;          /* +0x08 */
    this->vehicles[1] = NULL;          /* +0x0C */
    this->vehicles[2] = NULL;          /* +0x10 */
    this->vehicles[3] = NULL;          /* +0x14 */

    /* Clear all 16 sub_object slots (+0x18 through +0x54) */
    for (i = 0; i < 16; i++) {
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
/* Then releases two global objects via vtable[2] destructors.          */
/*                                                                      */
/* Called by: CGWND_Cleanup (0x40787E), Sprite_UnlockAll (0x454FF4),   */
/*            CollisionData_Dtor (0x44D839), INPUT_NewWorld (0x41E148) */
/* ================================================================== */
void World::Init(void)
{
    int slot_idx;
    int i;
    void* vehicle;
    int* occupant;

    for (slot_idx = 0; slot_idx < 4; slot_idx++) {
        vehicle = this->vehicles[slot_idx];  /* +0x08 + slot_idx * 4 */
        if (vehicle == NULL) {
            continue;
        }

        /* Remove all 8 occupant slots at vehicle+0x38..+0x54 */
        occupant = (int*)((uint8_t*)vehicle + VEHICLE_OFF_OCCUPANTS);
        for (i = 8; i != 0; i--) {
            if (*occupant != 0) {
                Building_RemoveOccupant((int*)*occupant);
                *occupant = 0;
            }
            occupant++;
        }

        /* Render final state then save */
        World_RenderAll(vehicle);
        this->SaveToFile(
            (uint)*(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_ID),   /* +0x7A */
            *(char*)((uint8_t*)vehicle + VEHICLE_OFF_PLAYER_ID),               /* +0x78 */
            (char)1);                                                          /* mp_flag */
    }

    /* Release global objects at 0x485268 and 0x48526c */
    /* These are called via vtable index 2 (+0x08) which is a release/destroy method */
    if (DAT_00485268 != NULL) {
        (*(void (**)(void*))(*(int*)DAT_00485268 + 8))(DAT_00485268);
        DAT_00485268 = NULL;
    }
    if (DAT_0048526c != NULL) {
        (*(void (**)(void*))(*(int*)DAT_0048526c + 8))(DAT_0048526c);
        DAT_0048526c = NULL;
    }
}

/* ================================================================== */
/* World_CheckActive                                                    */
/* Address: 0x44DBB0                                                    */
/* __thiscall (ECX = this)                                              */
/*                                                                      */
/* Returns 1 if vehicle_count >= 4 or field_06 >= 3 (world full).      */
/*                                                                      */
/* Called by: DDRAW_UpdateBuildingSprites (0x459B31)                   */
/* ================================================================== */
char World::CheckActive(void)
{
    if (this->vehicle_count < 4 && this->field_06 < 3) {
        return 0;
    }
    return 1;
}

/* ================================================================== */
/* World_Reset                                                          */
/* Address: 0x44DBD0                                                    */
/* __thiscall (ECX = this, char flag on stack)                          */
/*                                                                      */
/* For all active vehicles in states 0, 1, 4, or 5 (at +0x64), calls   */
/* Vehicle_UpdatePosition with the given flag.                          */
/* ================================================================== */
void World::Reset(char flag)
{
    int i;
    void* vehicle;
    int state_2;

    for (i = 0; i < 4; i++) {
        vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        state_2 = *(int*)((uint8_t*)vehicle + VEHICLE_OFF_STATE_2);  /* +0x64 */
        if (state_2 == 0 || state_2 == 4 || state_2 == 5 || state_2 == 1) {
            Vehicle_UpdatePosition(vehicle, flag);
        }
    }
}

/* ================================================================== */
/* World_SaveToFile                                                     */
/* Address: 0x44D8A0                                                    */
/* __thiscall (ECX = this, resource_id, player_id, mp_flag)             */
/*                                                                      */
/* Finds a vehicle matching resource_id and player_id in the active     */
/* vehicle array. Deselects from town/DDRAW views if selected,          */
/* notifies the network manager, then deletes the vehicle.              */
/*                                                                      */
/* On type==0, decrements field_06 first. In multiplayer scenario 2    */
/* with mp_flag set, dispatches NETMAN_ReceiveGameStart instead.        */
/*                                                                      */
/* Returns packed result with success flag (1) in low byte.             */
/* ================================================================== */
uint World::SaveToFile(uint resource_id, char player_id, char mp_flag)
{
    uint slot;
    void* vehicle;
    void** vehicle_slot;
    int type;
    uint result;
    void* resource_data;

    /* Find matching vehicle by resource ID + player ID */
    for (slot = 0; slot < 4; slot++) {
        vehicle = this->vehicles[slot];
        if (vehicle == NULL) {
            continue;
        }

        if (*(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_ID) == (uint16_t)resource_id &&
            *(char*)((uint8_t*)vehicle + VEHICLE_OFF_PLAYER_ID) == player_id) {
            break;
        }
    }

    if (slot > 3) {
        /* Not found — return 0 with high byte cleared/sign-extended */
        return (uint)vehicle & 0xFFFFFF00;
    }

    vehicle = this->vehicles[slot];
    vehicle_slot = &this->vehicles[slot];
    resource_data = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_DATA);  /* +0x10 */

    /* Check if this vehicle is currently selected in town/DDRAW views */
    if (resource_data != NULL &&
        *(int*)((uint8_t*)resource_data + 0x14) == g_selected_building) {
        Town_SelectBuilding(g_town_view, 0);
        DDRAW_SelectBuilding(g_ddraw_building, 0);
    }

    /* Notify network manager */
    NETMAN_HandleTimeout(g_netman, vehicle);

    type = *(int*)((uint8_t*)vehicle + VEHICLE_OFF_TYPE);  /* +0x04 */
    if (type == 0) {
        /* Type 0 — decrement field_06, then delete */
        this->field_06--;                                    /* +0x06 */

        if (vehicle != NULL) {
            /* Call scalar-deleting destructor vtable[0] */
            result = (*(uint (**)(void*, int))(*(int*)vehicle))(vehicle, 1);
            *vehicle_slot = NULL;
            this->vehicle_count--;
            return (result & 0xFFFFFF00) | 1;
        }
    } else {
        /* Check multiplayer scenario 2 with mp_flag */
        if (g_netman != NULL &&
            *(int*)((uint8_t*)g_netman + 0x7C4) == 2 &&    /* scenarioId field */
            mp_flag != 0) {
            result = NETMAN_ReceiveGameStart(g_netman, 0, 0, vehicle);
            *vehicle_slot = NULL;
            this->vehicle_count--;
            return (result & 0xFFFFFF00) | 1;
        }

        if (vehicle != NULL) {
            /* Call scalar-deleting destructor vtable[0] */
            result = (*(uint (**)(void*, int))(*(int*)vehicle))(vehicle, 1);
        }
    }

    *vehicle_slot = NULL;
    this->vehicle_count--;
    return (result & 0xFFFFFF00) | 1;
}

/* ================================================================== */
/* World_SerializeObject                                                */
/* Address: 0x44DA50                                                    */
/* __thiscall (ECX = this, char player_id)                              */
/*                                                                      */
/* Finds vehicles owned by player_id. For each match: clears all 8      */
/* occupant slots, renders final state, and saves with mp_flag=0.      */
/* ================================================================== */
void World::SerializeObject(char player_id)
{
    int i, j;
    void* vehicle;
    int* occupant;

    for (i = 0; i < 4; i++) {
        vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        /* Check player ID match */
        if (*(char*)((uint8_t*)vehicle + VEHICLE_OFF_PLAYER_ID) != player_id) {  /* +0x78 */
            continue;
        }

        /* Remove all 8 occupant slots */
        occupant = (int*)((uint8_t*)vehicle + VEHICLE_OFF_OCCUPANTS);  /* +0x38 */
        for (j = 8; j != 0; j--) {
            if (*occupant != 0) {
                Building_RemoveOccupant((int*)*occupant);
                *occupant = 0;
            }
            occupant++;
        }

        /* Render final state and save */
        World_RenderAll(vehicle);
        this->SaveToFile(
            (uint)*(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_ID),  /* +0x7A */
            player_id,
            0);                                                               /* mp_flag=0 */
    }
}

/* ================================================================== */
/* World_DeserializeMap                                                 */
/* Address: 0x44DAD0                                                    */
/* __thiscall (ECX = this, void* building)                               */
/*                                                                      */
/* For each active vehicle: checks any sub-object's bounding rect       */
/* against the building's rect. On overlap, clears occupants, renders   */
/* final state, and saves with mp_flag=1.                               */
/*                                                                      */
/* Skips vehicles where state_2==2 unless building state_code==4.       */
/* ================================================================== */
void World::DeserializeMap(void* building)
{
    int i, j;
    void* vehicle;
    bool overlap;
    RECT overlap_rect;
    int* occupant;
    void* sub_obj;

    for (i = 0; i < 4; i++) {
        vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        /* Skip if vehicle state_2 == 2 and building state_code != 4 */
        if (*(int*)((uint8_t*)vehicle + VEHICLE_OFF_STATE_2) == 2 &&           /* +0x64 */
            *(int*)((uint8_t*)building + BUILDING_OFF_STATE_CODE) != 4) {       /* +0x10C */
            continue;
        }

        /* Check for bounding-box overlap with any sub-object */
        overlap = false;
        for (j = 0; j < 4; j++) {
            sub_obj = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_ARRAY + j * 4);  /* +0x10 + j*4 */
            if (sub_obj == NULL) {
                continue;
            }

            if (!overlap) {
                /* Test rect overlap using Win32 IntersectRect */
                /* Sub-object rect at +0x08, building rect at +0x08 */
                if (IntersectRect(&overlap_rect,
                        (RECT*)((uint8_t*)sub_obj + SUB_OBJ_OFF_RECT),
                        (RECT*)((uint8_t*)building + SUB_OBJ_OFF_RECT)) == 0) {
                    /* No overlap — skip this sub-object */
                    continue;
                }
            }
            overlap = true;
        }

        if (overlap) {
            /* Clear all 8 occupant slots */
            occupant = (int*)((uint8_t*)vehicle + VEHICLE_OFF_OCCUPANTS);  /* +0x38 */
            for (j = 8; j != 0; j--) {
                if (*occupant != 0) {
                    Building_RemoveOccupant((int*)*occupant);
                    *occupant = 0;
                }
                occupant++;
            }

            /* Render and save */
            World_RenderAll(vehicle);
            this->SaveToFile(
                (uint)*(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_ID),  /* +0x7A */
                *(char*)((uint8_t*)vehicle + VEHICLE_OFF_PLAYER_ID),              /* +0x78 */
                1);                                                               /* mp_flag=1 */
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
/* resource ID (0x1804, 0x1806, or 0x1808), random name, and random    */
/* route (0-4 segments pointing to buildings at 0x1866/8/A, 0x186C,    */
/* 0x1870).                                                             */
/*                                                                      */
/* If vehicle_init is non-NULL: creates a vehicle with resource ID      */
/* from vehicle_init[0], sets up route from vehicle_init[1..3], and     */
/* calls Vehicle_FindPath.                                              */
/*                                                                      */
/* On resource load failure: destroys the vehicle via scalar-deleting   */
/* destructor and returns NULL.                                         */
/*                                                                      */
/* Uses SEH (__try/__except) for operator_new calls.                    */
/* ================================================================== */
void* World::LoadFromFile(int* route_data, int* vehicle_init)
{
    uint slot;
    void* vehicle_mem;
    void* vehicle;
    int resource_id;
    uint dir;
    int rand_val;
    int i;
    char rand_flag;
    char name_buf[12];
    bool do_register;

    /* Bounds check — world must not be full */
    if ((uint16_t)this->vehicle_count > 3 || (uint16_t)this->field_06 > 2) {
        return NULL;
    }

    /* Find first empty vehicle slot */
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
        vehicle_mem = operator_new(0x94);
        if (vehicle_mem == NULL) {
            vehicle = NULL;
        } else {
            /* Random resource from {0x1804, 0x1806, 0x1808} */
            rand_val = CRT_rand() % 3;
            resource_id = rand_val * 2 + 0x1804;
            vehicle = Vehicle_Ctor(vehicle_mem, resource_id, 0, 0, 0);
        }
        this->vehicles[slot] = vehicle;

        if (vehicle != NULL) {
            /* Verify resource loaded successfully */
            void* res_data = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_DATA);  /* +0x10 */
            if (res_data != NULL &&
                *(char*)((uint8_t*)res_data + 0x18) == 1) {

                /* Assign random vehicle name via sprintf */
                CRT_sprintf(name_buf, 10, "%s__%lu", (const char*)0x47F030,
                    *(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_ID));

                /* Set name on vehicle's sub-object via vtable[13] (+0x34) */
                {
                    void* res_data = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_DATA);
                    ((void (*)(void*, char*))((void**)res_data)[13])(res_data, name_buf);
                }

                /* Generate random route (0 to 4 segments) */
                rand_val = CRT_rand() % 5;
                for (i = 0; i < rand_val; i++) {
                    int choice = CRT_rand() % 3;
                    if (choice == 0) {
                        /* Destination building type A: random from {0x1866, 0x1868, 0x186A} */
                        dir = 2;
                        rand_flag = 0;
                        resource_id = (CRT_rand() % 3) * 2 + 0x1866;
                    } else if (choice == 1) {
                        /* Destination building type B: 0x186C */
                        dir = 3;
                        rand_flag = 0;
                        resource_id = 0x186C;
                    } else {
                        /* Destination building type C: 0x1870 */
                        dir = 4;
                        rand_flag = 0;
                        resource_id = 0x1870;
                    }
                    Vehicle_InitRoute(vehicle, resource_id, dir, rand_flag);
                }
                do_register = true;
            } else {
                /* Resource not loaded — destroy vehicle */
                (*(void (**)(void*, int))(*(int*)vehicle))(vehicle, 1);
                this->vehicles[slot] = NULL;
            }
        }
    } else {
        /* ---- Specific vehicle from save data ---- */
        vehicle_mem = operator_new(0x94);
        if (vehicle_mem == NULL) {
            vehicle = NULL;
        } else {
            vehicle = Vehicle_Ctor(vehicle_mem, *vehicle_init, 0, 0, 0);
        }
        this->vehicles[slot] = vehicle;

        if (vehicle != NULL) {
            /* Verify resource loaded */
            void* res_data = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_DATA);  /* +0x10 */
            if (res_data == NULL || *(char*)((uint8_t*)res_data + 0x18) != 1) {
                /* Resource not loaded — destroy */
                (*(void (**)(void*, int))(*(int*)vehicle))(vehicle, 1);
                this->vehicles[slot] = NULL;
            } else {
                /* Assign name */
                CRT_sprintf(name_buf, 10, "%s__%lu", (const char*)0x47F030,
                    *(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_ID));

                /* Set name on vehicle's sub-object */
                {
                    void* res_data = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_DATA);
                    ((void (*)(void*, char*))((void**)res_data)[13])(res_data, name_buf);
                }

                /* Set up initial position */
                Vehicle_UpdatePosition(vehicle, 0);

                /* Set up route from save data (up to 3 route entries) */
                for (i = 0; i < 3; i++) {
                    vehicle_init++;
                    if (*vehicle_init != 0) {
                        dir = CGWND_MapResourceToDirection(*vehicle_init);
                        Vehicle_InitRoute(vehicle, *vehicle_init, dir, 0);
                        Vehicle_UpdatePosition(vehicle, 0);
                    }
                }
                do_register = true;
            }
        }
    }

    if (do_register) {
        /* Increment counters and find path */
        this->vehicle_count++;
        this->field_06++;
        Vehicle_FindPath(vehicle, route_data, 1);
    }

    return this->vehicles[slot];
}

/* ================================================================== */
/* World_FinalizeLoad                                                   */
/* Address: 0x44DF40                                                    */
/* __thiscall (ECX = this, void* vehicle, int packed_coords,           */
/*            char mp_flag)                                             */
/*                                                                      */
/* Registers a fully-loaded vehicle in the first empty world slot.      */
/* Unpacks destination coordinates and links to a destination building  */
/* via ArrivalQueue_AddVehicle.                                         */
/*                                                                      */
/* In single player, finds destination via INPUT_FindObjectAt(param=0). */
/* In multiplayer scenario 1, uses INPUT_FindObjectAt(param=1).         */
/* In multiplayer scenario 2, uses TileMap_GetObjectAt with optional    */
/* y-coord adjustment based on mp_flag.                                 */
/*                                                                      */
/* Returns 1 on success, 0 if world is full or no empty slot.           */
/* ================================================================== */
char World::FinalizeLoad(void* vehicle, int packed_coords, char mp_flag)
{
    uint slot;
    short dest_x, dest_y;
    void* building;
    int input_param;

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
    *(int*)((uint8_t*)vehicle + VEHICLE_OFF_NET_SYNC_FLAG) = 0;  /* clear net_sync_flag +0x68 */
    this->vehicle_count++;

    /* Unpack destination coordinates */
    dest_x = (short)(packed_coords & 0xFFFF);
    dest_y = (short)((packed_coords >> 16) & 0xFFFF);

    /* Determine destination based on network scenario */
    if (g_netman != NULL && *(int*)((uint8_t*)g_netman + 0x7C4) == 1) {
        /* Multiplayer scenario 1 */
        input_param = 1;
        building = INPUT_FindObjectAt(g_input_mgr, input_param);
    } else if (g_netman != NULL && *(int*)((uint8_t*)g_netman + 0x7C4) == 2) {
        /* Multiplayer scenario 2: adjust destination Y based on mp_flag */
        if (mp_flag == 1 || mp_flag == 2) {
            dest_y++;
        }
        building = TileMap_GetObjectAt(g_tilemap, dest_x, dest_y, 0);
    } else {
        /* Single player */
        input_param = 0;
        building = INPUT_FindObjectAt(g_input_mgr, input_param);
    }

    /* Fallback: try INPUT_FindObjectAt with param=1 */
    if (building == NULL) {
        building = INPUT_FindObjectAt(g_input_mgr, 1);
    }

    if (building == NULL) {
        /* No destination found — set vehicle to arrival state */
        Vehicle_SetState(vehicle, 3);  /* ARRIVED */
        return 1;
    }

    /* Link vehicle to destination building */
    ArrivalQueue_AddVehicle(building, vehicle);
    return 1;
}

/* ================================================================== */
/* World_UpdateTick                                                     */
/* Address: 0x44E020                                                    */
/* __thiscall (ECX = this)                                              */
/*                                                                      */
/* Main per-tick update. Guards: vehicle_count > 0 AND game_mode in     */
/* {3, 9}. For each active vehicle:                                     */
/*   1. World_ProcessEvents — collision/overlap detection               */
/*   2. VehicleEditor_Update — update editor positions                  */
/*   3. If vehicle_state == 3 (ARRIVED):                                */
/*        Clear all occupants, render, save                             */
/*   4. Else if net_sync_flag == 1:                                     */
/*        Pack destination coords, flag=2, clear occupants (type!=0),   */
/*        render, send network start, remove from world                 */
/* ================================================================== */
void World::UpdateTick(void)
{
    int i, j;
    void* vehicle;
    int* occupant;
    int packed_coords;

    /* Early out if no vehicles or wrong game mode */
    if (this->vehicle_count == 0) {
        return;
    }
    if (g_game_mode != 3 && g_game_mode != 9) {
        return;
    }

    for (i = 0; i < 4; i++) {
        vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        /* Step 1: Collision/overlap detection */
        this->ProcessEvents(vehicle);

        /* Step 2: Update vehicle editor (position, animation) */
        VehicleEditor_Update(vehicle);

        /* Step 3: Check vehicle state */
        if (*(int*)((uint8_t*)vehicle + VEHICLE_OFF_VEHICLE_STATE) == 3) {
            /* ARRIVED — clear all occupants */
            occupant = (int*)((uint8_t*)vehicle + VEHICLE_OFF_OCCUPANTS);  /* +0x38 */
            for (j = 8; j != 0; j--) {
                if (*occupant != 0) {
                    Building_RemoveOccupant((int*)*occupant);
                    *occupant = 0;
                }
                occupant++;
            }

            /* Render and save */
            World_RenderAll(vehicle);
            this->SaveToFile(
                (uint)*(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_ID),
                *(char*)((uint8_t*)vehicle + VEHICLE_OFF_PLAYER_ID),
                1);

        } else if (*(int*)((uint8_t*)vehicle + VEHICLE_OFF_NET_SYNC_FLAG) == 1) {
            /* Network sync pending */
            void* resource_data = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_DATA);  /* +0x10 */
            void* editor_state = *(void**)((uint8_t*)resource_data + 0x14);

            /* Pack destination coordinates from editor_state */
            packed_coords = *(int*)((uint8_t*)editor_state + 0x88);

            /* Mark as synced */
            *(int*)((uint8_t*)vehicle + VEHICLE_OFF_NET_SYNC_FLAG) = 2;

            /* Clear occupants if vehicle type != 0 */
            if (*(int*)((uint8_t*)vehicle + VEHICLE_OFF_TYPE) != 0) {  /* +0x04 */
                occupant = (int*)((uint8_t*)vehicle + VEHICLE_OFF_OCCUPANTS);  /* +0x38 */
                for (j = 8; j != 0; j--) {
                    if (*occupant != 0) {
                        Building_RemoveOccupant((int*)*occupant);
                        *occupant = 0;
                    }
                    occupant++;
                }
            }

            /* Render final state */
            World_RenderAll(vehicle);

            /* Notify network manager with unpacked coords */
            NETMAN_ReceiveGameStart(g_netman,
                (int)(int16_t)(packed_coords & 0xFFFF),
                (int)(int16_t)((packed_coords >> 16) & 0xFFFF),
                vehicle);

            /* Remove vehicle from world */
            this->vehicles[i] = NULL;
            this->vehicle_count--;
        }
    }
}

/* ================================================================== */
/* World_ProcessEvents                                                  */
/* Address: 0x44E3F0                                                    */
/* __thiscall (ECX = this, void* current_vehicle)                       */
/*                                                                      */
/* Collision detection between current_vehicle and all other vehicles.  */
/*                                                                      */
/* Checks each sub-object of current_vehicle against sub-objects of     */
/* other vehicles for bounding-box overlap. Requires both visual        */
/* overlap (via Town_BlitViewport) and rect overlap.                    */
/*                                                                      */
/* On collision: stops both vehicles (state=4), sets random 1-100      */
/* tick wait timer, shows collision message box (resource 0x3861).     */
/*                                                                      */
/* Guard conditions (skip collision test if):                           */
/*   - Fewer than 2 vehicles in world                                   */
/*   - current_vehicle is NULL or not MOVING (state != 2)               */
/*   - action_state is 2 (unloading) or 3 (arriving)                    */
/*   - state_2 is 2                                                     */
/*   - Other vehicle is unloading/arriving                               */
/*   - Other vehicle is stopped with wait=0                             */
/*   - Both vehicles share the same parent building                     */
/*   - Current vehicle building type == 2 (non-colliding type)          */
/* ================================================================== */
uint World::ProcessEvents(void* current_vehicle)
{
    int i, j;
    void* other;
    void* current_sub_obj;
    int cur_left, cur_top, cur_right, cur_bottom;
    int other_left, other_top, other_right, other_bottom;
    int viewport_result;
    uint wait;
    int* other_sub_obj_ptr;

    /* Early outs */
    if (this->vehicle_count <= 1)                       return 0;
    if (current_vehicle == NULL)                        return 0;
    if (*(int*)((uint8_t*)current_vehicle + VEHICLE_OFF_VEHICLE_STATE) != 2) return 0;  /* not MOVING */
    if (*(int*)((uint8_t*)current_vehicle + VEHICLE_OFF_ACTION_STATE) == 2 ||
        *(int*)((uint8_t*)current_vehicle + VEHICLE_OFF_ACTION_STATE) == 3)  return 0;
    if (*(int*)((uint8_t*)current_vehicle + VEHICLE_OFF_STATE_2) == 2)       return 0;

    current_sub_obj = *(void**)((uint8_t*)current_vehicle + VEHICLE_OFF_SUB_OBJ_MAIN);  /* +0x20 */

    for (i = 0; i < 4; i++) {
        other = this->vehicles[i];
        if (other == NULL)                            continue;
        if (*(int*)((uint8_t*)other + VEHICLE_OFF_ACTION_STATE) == 2 ||
            *(int*)((uint8_t*)other + VEHICLE_OFF_ACTION_STATE) == 3)  continue;
        if (*(int*)((uint8_t*)other + VEHICLE_OFF_STATE_2) == 2)       continue;
        if (*(int*)((uint8_t*)other + VEHICLE_OFF_VEHICLE_STATE) == 4 &&  /* STOPPED */
            *(int*)((uint8_t*)other + VEHICLE_OFF_TIMER_WAIT) == 0)    continue;
        if (other == current_vehicle)                 continue;  /* skip self */

        /* Check all sub-objects of the other vehicle */
        uint16_t other_sub_count = *(uint16_t*)((uint8_t*)other + VEHICLE_OFF_SUB_OBJ_COUNT);  /* +0x0C */
        for (j = 0; j <= (int)other_sub_count; j++) {
            other_sub_obj_ptr = *(int**)((uint8_t*)other + VEHICLE_OFF_SUB_OBJ_ARRAY + j * 4);  /* +0x10 + j*4 */
            if (other_sub_obj_ptr == NULL) continue;

            /* Skip if sharing the same parent building (editor state) */
            int* editor_state_1 = *(int**)((uint8_t*)other_sub_obj_ptr + SUB_OBJ_OFF_EDITOR_STATE_1);  /* +0x430 */
            if (*(int*)((uint8_t*)editor_state_1 + 0x14) ==
                *(int*)((uint8_t*)current_sub_obj + 0x14)) {
                continue;
            }

            int* editor_state_2 = *(int**)((uint8_t*)other_sub_obj_ptr + SUB_OBJ_OFF_EDITOR_STATE_2);  /* +0x434 */
            if (*(int*)((uint8_t*)editor_state_2 + 0x14) ==
                *(int*)((uint8_t*)current_sub_obj + 0x14)) {
                continue;
            }

            /* Bounding-box overlap test using sub-object rects */
            cur_left   = *(int*)((uint8_t*)current_sub_obj + SUB_OBJ_OFF_RECT + 0x00);      /* +0x08 */
            cur_top    = *(int*)((uint8_t*)current_sub_obj + SUB_OBJ_OFF_RECT + 0x04);      /* +0x0C */
            cur_right  = *(int*)((uint8_t*)current_sub_obj + SUB_OBJ_OFF_RECT + 0x08);      /* +0x10 */
            cur_bottom = *(int*)((uint8_t*)current_sub_obj + SUB_OBJ_OFF_RECT + 0x0C);      /* +0x14 */

            other_left   = other_sub_obj_ptr[2];   /* +0x08 */
            other_top    = other_sub_obj_ptr[3];   /* +0x0C */
            other_right  = other_sub_obj_ptr[4];   /* +0x10 */
            other_bottom = other_sub_obj_ptr[5];   /* +0x14 */

            /* Simple AABB overlap test */
            if (other_left < cur_right && cur_right < other_right &&
                other_top < cur_bottom && cur_bottom < other_bottom) {

                /* Check if building type allows collision */
                void* current_res_data = *(void**)((uint8_t*)current_vehicle + VEHICLE_OFF_RESOURCE_DATA);
                if (*(int*)((uint8_t*)current_res_data + 0x1C) == 2) {
                    continue;  /* type 2 buildings don't collide */
                }

                /* Visual overlap check via Town_BlitViewport */
                void* other_sub_obj = *(void**)((uint8_t*)other + VEHICLE_OFF_SUB_OBJ_ARRAY + j * 4);
                int* other_sub = (int*)other_sub_obj;

                viewport_result = Town_BlitViewport(
                    *(void**)(other_sub[0x10] + 0x10),     /* +0x40 resource -> +0x10 */
                    other_sub[0x0C],                       /* +0x30 */
                    other_sub[0x0D],                       /* +0x34 */
                    other_sub[0x0E],                       /* +0x38 */
                    other_sub[0x0F],                       /* +0x3C */
                    ((uint)*(uint16_t*)(other_sub[0x10] + 0x14) *
                     (uint)*(uint16_t*)(other_sub + 0x10E) -  /* +0x438 */
                     other_sub[2]) + cur_top,               /* adjusted X */
                    cur_bottom - other_sub[3]);             /* height diff */

                if (viewport_result == 0) {
                    /* COLLISION DETECTED */

                    /* Stop current vehicle */
                    Vehicle_SetState(current_vehicle, 4);  /* STOPPED */
                    wait = (uint)(CRT_rand() % 100) + 1;
                    *(int*)((uint8_t*)current_vehicle + VEHICLE_OFF_TIMER_WAIT) = wait;  /* +0x28 */

                    /* Show collision message box */
                    void* cur_res_data = *(void**)((uint8_t*)current_vehicle + VEHICLE_OFF_RESOURCE_DATA);
                    UI_CreateMessageBox(g_tooltip_mgr, 0x3861, 0, 'W',
                        *(int*)((uint8_t*)cur_res_data + 0x0C),
                        *(int*)((uint8_t*)cur_res_data + 0x10),
                        1);

                    /* Also stop the other vehicle if it's active */
                    if (*(int*)((uint8_t*)this->vehicles[i] + VEHICLE_OFF_VEHICLE_STATE) != 0) {
                        Vehicle_SetState(this->vehicles[i], 4);
                        wait = (uint)(CRT_rand() % 100) + 1;
                        *(int*)((uint8_t*)this->vehicles[i] + VEHICLE_OFF_TIMER_WAIT) = wait;
                    }
                    break;
                }
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
/* the given (audio_x, audio_y) position. Calls vtable[2] (HitTest)     */
/* on each sub-object. On match, selects the building in town view.     */
/* Guarded by g_click_on_town flag.                                     */
/* ================================================================== */
char World::ProcessAudio(int audio_x, int audio_y)
{
    int i, j;
    void* vehicle;
    void* sub_obj;
    char result;

    result = 0;

    if (g_click_on_town == 0) {
        return 0;
    }

    for (i = 0; i < 4; i++) {
        vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        uint16_t sub_count = *(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_COUNT);  /* +0x0C */
        for (j = 0; j <= (int)sub_count; j++) {
            sub_obj = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_ARRAY + j * 4);  /* +0x10 + j*4 */
            if (sub_obj == NULL) {
                continue;
            }

            /* Check if sub-object is active/visible */
            if (*(char*)((uint8_t*)sub_obj + SUB_OBJ_OFF_ACTIVE_FLAG) == 1) {  /* +0x24 */
                /* Call vtable[2] = HitTest or PtInRect */
                char hit = (*(char (**)(int, int))(*(int*)sub_obj + 8))(audio_x, audio_y);
                if (hit != 0) {
                    Town_SelectBuilding(g_town_view,
                        (int)*(void**)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_ARRAY + j * 4));
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
/* __thiscall (ECX = this, int building_id)                             */
/*                                                                      */
/* Checks if any active vehicle references the given building ID        */
/* through its editor states. Returns 1 if found. Used to determine     */
/* if world timers should remain active.                                */
/* ================================================================== */
char World::InitTimer(int building_id)
{
    int i, j;
    void* vehicle;
    void* sub_obj;

    for (i = 0; i < 4; i++) {
        vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        /* Check main sub-object's editor state parent */
        void* main_sub = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_MAIN);  /* +0x20 */
        if (main_sub != NULL &&
            *(int*)((uint8_t*)main_sub + 0x14) == building_id) {
            return 1;
        }

        /* Check array sub-objects' editor states */
        uint16_t sub_count = *(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_COUNT);  /* +0x0C */
        for (j = 0; j <= (int)sub_count; j++) {
            sub_obj = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_ARRAY + j * 4);  /* +0x10 + j*4 */
            if (sub_obj == NULL) {
                continue;
            }

            /* Check editor states for matching building ID */
            void* editor_1 = *(void**)((uint8_t*)sub_obj + SUB_OBJ_OFF_EDITOR_STATE_1);  /* +0x430 */
            void* editor_2 = *(void**)((uint8_t*)sub_obj + SUB_OBJ_OFF_EDITOR_STATE_2);  /* +0x434 */

            if ((editor_1 != NULL && *(int*)((uint8_t*)editor_1 + 0x14) == building_id) ||
                (editor_2 != NULL && *(int*)((uint8_t*)editor_2 + 0x14) == building_id)) {
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
/* bubble-sorts them by depth priority (z-order).                      */
/*                                                                      */
/* Exclusion conditions (skip sub-object if):                           */
/*   - exclusion_flag_2 (+0x444) == 2 or == 5                          */
/*   - exclusion_flag (+0x440) == 2                                    */
/*                                                                      */
/* Sorting is bubble-sort by depth order (via editor_state+0x10).       */
/* Sorted in ascending depth (lower depth = higher priority).           */
/*                                                                      */
/* Called by: TileMap_InvalidateDirtyRects (0x45619A)                  */
/* ================================================================== */
void World::Lock(void)
{
    int i;
    int slot_count;
    void* vehicle;
    void* sub_obj;
    bool swapped;

    slot_count = 0;

    if (this->vehicle_count == 0) {
        return;
    }

    /* Step 1: Collect non-excluded sub-objects */
    for (i = 0; i < 4; i++) {
        vehicle = this->vehicles[i];
        if (vehicle == NULL) {
            continue;
        }

        uint16_t sub_count = *(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_COUNT);  /* +0x0C */
        int sub_idx;
        for (sub_idx = 0; sub_idx <= (int)sub_count && sub_idx < 4; sub_idx++) {
            sub_obj = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_ARRAY + sub_idx * 4);  /* +0x10 + sub_idx*4 */
            if (sub_obj == NULL) {
                continue;
            }

            /* Skip excluded types */
            if (*(int*)((uint8_t*)sub_obj + SUB_OBJ_OFF_EXCLUSION_FLAG2) != 2 &&  /* +0x444 */
                *(int*)((uint8_t*)sub_obj + SUB_OBJ_OFF_EXCLUSION_FLAG2) != 5 &&
                *(int*)((uint8_t*)sub_obj + SUB_OBJ_OFF_EXCLUSION_FLAG) != 2) {    /* +0x440 */
                if (slot_count < 16) {
                    this->sub_objects[slot_count] = sub_obj;
                    slot_count++;
                }
            }
        }
    }

    if (slot_count == 0) {
        return;
    }

    /* Step 2: Bubble-sort by depth order (ascending = lower depth first) */
    do {
        swapped = false;
        for (i = 0; i < slot_count - 1 && i < 15; i++) {
            void* obj_a = this->sub_objects[i];
            void* obj_b = this->sub_objects[i + 1];
            if (obj_a == NULL || obj_b == NULL) {
                break;
            }

            int depth_a = *(int*)(*(int*)((uint8_t*)obj_a + SUB_OBJ_OFF_EDITOR_STATE_1) + 0x10);  /* +0x430 -> +0x10 */
            int depth_b = *(int*)(*(int*)((uint8_t*)obj_b + SUB_OBJ_OFF_EDITOR_STATE_1) + 0x10);

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
    int i;
    for (i = 0; i < 16; i++) {
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
/* VehicleEditor_BlitBackground.                                        */
/*                                                                      */
/* When scroll_stop == 1: also checks the next adjacent sub-object      */
/* with the same depth/layer (+0x44C) and zero connectivity (+0x448).   */
/* If within bounds, blits its background as well.                      */
/*                                                                      */
/* Note: param3 and param4 are present on the stack but never read.     */
/* ================================================================== */
void World::InvalidateRect(int x, int y, int param3, int param4, short scroll_stop)
{
    int i;
    void* sub_obj;
    char in_bounds;

    /* UNUSED: param3 and param4 are stack padding/unused parameters */
    (void)param3;
    (void)param4;

    for (i = 0; i < 16; i++) {
        sub_obj = this->sub_objects[i];
        if (sub_obj == NULL) {
            return;
        }

        /* Check if sub-object bounds include this tile */
        in_bounds = (char)VehicleEditor_IsInBounds(sub_obj,
            (short)x, (short)y, scroll_stop);

        if (in_bounds != 0) {
            VehicleEditor_BlitBackground(sub_obj, x, y);

            /* If scroll_stop == 1, also check adjacent sub-objects with same depth */
            if (scroll_stop == 1) {
                int j = i + 1;
                while (j < 16) {
                    void* next = this->sub_objects[j];
                    if (next == NULL) {
                        break;
                    }

                    /* Check same layer depth and zero connectivity */
                    if (*(int*)((uint8_t*)next + SUB_OBJ_OFF_DEPTH) ==       /* +0x44C */
                        *(int*)((uint8_t*)sub_obj + SUB_OBJ_OFF_DEPTH) &&
                        *(int16_t*)((uint8_t*)next + SUB_OBJ_OFF_CONNECTIVITY) == 0) {  /* +0x448 */

                        char adjacent = (char)VehicleEditor_IsInBounds(next,
                            (short)x, (short)y, 0);
                        if (adjacent != 0) {
                            VehicleEditor_BlitBackground(
                                this->sub_objects[j], x, y);
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
/* __stdcall (stack args only, no ECX)                                  */
/*                                                                      */
/* Configures route data on a building occupant. If the building's      */
/* occupancy flag (+0x11C) is 1 and tracked vehicle (+0x120) exists:    */
/*   1. If resource_id doesn't match, sets new resource on sub-obj     */
/*   2. Removes 3 old route entries                                    */
/*   3. Installs up to 3 new route points from route_data[1..3]        */
/*   4. Calls Vehicle_FindPath                                        */
/*                                                                      */
/* Called from: Town_BuyBuilding (0x45B01A)                             */
/* ================================================================== */
bool __stdcall World_SerializeMap(int* building, int* route_data)
{
    void* vehicle;
    int initial_resource;
    int current_resource;
    bool result;
    uint dir;

    /* Check occupancy flag and tracked vehicle */
    if (building[0x47] == 1 &&                                   /* +0x11C */
        (vehicle = (void*)building[0x48], vehicle != NULL)) {    /* +0x120 */

        initial_resource = *route_data;
        current_resource = VehicleEditor_GetResourceId(*(int*)((uint8_t*)vehicle + 0x10));

        result = (current_resource != initial_resource);

        if (result) {
            /* Set new resource on sub-object via vtable[15] (+0x3C) */
            (*(void (**)(int, int))(**(int**)((uint8_t*)vehicle + 0x10) + 0x3C))
                (*(int*)((uint8_t*)vehicle + 0x10), initial_resource);
        }

        /* Remove 3 old route entries */
        for (int i = 0; i < 3; i++) {
            VehicleEditor_RemoveVehicle(vehicle, 1);
        }

        /* Install new route entries from route_data[1..3] */
        for (int i = 0; i < 3; i++) {
            route_data++;
            if (*route_data != 0) {
                dir = CGWND_MapResourceToDirection(*route_data);
                Vehicle_InitRoute(vehicle, *route_data, dir, 0);
                result = true;
            }
        }

        Vehicle_FindPath(vehicle, building, 1);
        return result;
    }
    return false;
}

/* ================================================================== */
/* World_RenderAll (free function — NOT a World method)                */
/* Address: 0x44E630                                                    */
/* __stdcall (void* vehicle)                                            */
/*                                                                      */
/* MISNAMED: This does NOT render anything. It handles vehicle          */
/* arrival at its destination.                                          */
/*                                                                      */
/* Steps:                                                              */
/*   1. Vehicle_UpdatePosition                                         */
/*   2. Find destination building at current tile (+0x2E/0x30, y+1)    */
/*   3. If building found:                                             */
/*      - action_state 2 or 3: ArrivalQueue_RemoveVehicle, fall thru  */
/*      - action_state 0,1,2,3,4,5: clear arrival queue ptr (+0x11C)  */
/*   4. Clear current tile coords                                      */
/*   5. If state_2 in {1,2,4,5}:                                      */
/*      - Find destination building at dest tile (+0x32/0x34, y+1)    */
/*      - If state_2 == 2: GameVehicle_RemoveDestination              */
/*      - If tracked_vehicle matches: clear tracking, SetAnimState(0) */
/*   6. Clear destination coords                                       */
/*   7. SetAnimState(1) on all sub-object editor states at +0x430/434 */
/*   8. GAMESTATE_EditorState_Detach on all sub-obj editor states      */
/* ================================================================== */
void __stdcall World_RenderAll(void* vehicle)
{
    int i;
    void* building;
    void* sub_obj_parent;

    if (vehicle == NULL) {
        return;
    }

    /* Step 1: Update position */
    Vehicle_UpdatePosition(vehicle, 0);

    /* Step 2: Find destination building at current tile */
    building = TileMap_GetObjectAt(g_tilemap,
        *(int16_t*)((uint8_t*)vehicle + VEHICLE_OFF_TILE_X),       /* +0x2E */
        *(int16_t*)((uint8_t*)vehicle + VEHICLE_OFF_TILE_Y) + 1,   /* +0x30 + 1 */
        0);

    if (building != NULL) {
        switch (*(int*)((uint8_t*)vehicle + VEHICLE_OFF_ACTION_STATE)) {  /* +0x60 */
        case 2:  /* UNLOADING */
        case 3:  /* ARRIVING */
            ArrivalQueue_RemoveVehicle(building,
                (uint)*(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_ID),
                *(char*)((uint8_t*)vehicle + VEHICLE_OFF_PLAYER_ID));
            /* Fall through to clear arrival queue */
        case 0:  /* IDLE */
        case 1:  /* APPROACHING */
        case 4:  /* DEPARTING */
        case 5:  /* WAITING */
            *(int*)((uint8_t*)building + BUILDING_OFF_OCCUPANCY) = 0;  /* +0x11C — clear arrival queue ptr */
            break;
        }
    }

    /* Step 4: Clear current tile coords */
    *(int16_t*)((uint8_t*)vehicle + VEHICLE_OFF_TILE_X) = -1;   /* +0x2E */
    *(int16_t*)((uint8_t*)vehicle + VEHICLE_OFF_TILE_Y) = -1;   /* +0x30 */

    /* Step 5: Check destination tile (second set of coords) */
    switch (*(int*)((uint8_t*)vehicle + VEHICLE_OFF_STATE_2)) {  /* +0x64 */
    case 1:
    case 2:
    case 4:
    case 5: {
        void* dest_building = TileMap_GetObjectAt(g_tilemap,
            *(int16_t*)((uint8_t*)vehicle + VEHICLE_OFF_DEST_X),        /* +0x32 */
            *(int16_t*)((uint8_t*)vehicle + VEHICLE_OFF_DEST_Y) + 1,    /* +0x34 + 1 */
            0);

        if (dest_building != NULL) {
            if (*(int*)((uint8_t*)vehicle + VEHICLE_OFF_STATE_2) == 2) {
                GameVehicle_RemoveDestination(
                    dest_building,
                    (uint)*(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_RESOURCE_ID),
                    *(char*)((uint8_t*)vehicle + VEHICLE_OFF_PLAYER_ID));
            }

            /* Clear tracked vehicle on destination building */
            if ((void*)*(int*)((uint8_t*)dest_building + BUILDING_OFF_TRACKED_VEH) == vehicle) {
                *(int*)((uint8_t*)dest_building + BUILDING_OFF_TRACKED_VEH) = 0;   /* +0x120 */
                *(char*)((uint8_t*)dest_building + BUILDING_OFF_TRACKED_FLAG) = 0; /* +0x128 */
                (*(void (**)(int))(*(int*)dest_building + 0x1C))(0);               /* SetAnimState(0) */
                *(int*)((uint8_t*)dest_building + BUILDING_OFF_OCCUPANCY) = 0;     /* +0x11C */
            }
        }
        break;
    }
    }

    /* Clear destination coords */
    *(int16_t*)((uint8_t*)vehicle + VEHICLE_OFF_DEST_X) = -1;   /* +0x32 */
    *(int16_t*)((uint8_t*)vehicle + VEHICLE_OFF_DEST_Y) = -1;   /* +0x34 */

    /* Step 7: Detach editor states — SetAnimState(1) on state_code == 7 */
    void* main_sub = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_MAIN);  /* +0x20 */
    if (main_sub != NULL) {
        sub_obj_parent = *(void**)((uint8_t*)main_sub + 0x14);
        if (sub_obj_parent != NULL && *(int*)((uint8_t*)sub_obj_parent + 0x10C) == 7) {
            (*(void (**)(int))(*(int*)sub_obj_parent + 0x1C))(1);  /* SetAnimState(1) */
        }
    }

    /* Iterate sub-objects and detach editor visual states */
    uint16_t sub_count = *(uint16_t*)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_COUNT);  /* +0x0C */
    for (i = 0; i <= (int)sub_count; i++) {
        void* sub_obj = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_ARRAY + i * 4);  /* +0x10 + i*4 */
        if (sub_obj == NULL) continue;

        /* Detach editor state 1 */
        void* editor_1 = *(void**)(*(int*)((uint8_t*)sub_obj + SUB_OBJ_OFF_EDITOR_STATE_1) + 0x14);
        if (editor_1 != NULL && *(int*)((uint8_t*)editor_1 + 0x10C) == 7) {
            (*(void (**)(int))(*(int*)editor_1 + 0x1C))(1);
        }

        /* Detach editor state 2 */
        void* editor_2 = *(void**)(*(int*)((uint8_t*)sub_obj + SUB_OBJ_OFF_EDITOR_STATE_2) + 0x14);
        if (editor_2 != NULL && *(int*)((uint8_t*)editor_2 + 0x10C) == 7) {
            (*(void (**)(int))(*(int*)editor_2 + 0x1C))(1);
        }
    }

    /* Step 8: Detach via GAMESTATE (second pass) */
    for (i = 0; i <= (int)sub_count; i++) {
        void* sub_obj = *(void**)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_ARRAY + i * 4);
        if (sub_obj != NULL) {
            GAMESTATE_EditorState_Detach(*(int*)((uint8_t*)sub_obj + SUB_OBJ_OFF_EDITOR_STATE_1));  /* +0x430 */
            GAMESTATE_EditorState_Detach(*(int*)((uint8_t*)sub_obj + SUB_OBJ_OFF_EDITOR_STATE_2));  /* +0x434 */
        }
    }

    /* Detach editor state from main sub-object parent */
    if (*(void**)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_MAIN) != NULL) {
        void* main_editor = *(void**)(*(int*)((uint8_t*)vehicle + VEHICLE_OFF_SUB_OBJ_MAIN) + 0x14);
        GAMESTATE_EditorState_Detach((int)main_editor);
    }
}

/* ================================================================== */
/* World_GetObjectAt (free function — NOT a World method)              */
/* Address: 0x44E800                                                    */
/* __stdcall (void* obj)                                                */
/*                                                                      */
/* Simple helper: removes all 8 occupant slots at +0x38..+0x54 from    */
/* the given object via Building_RemoveOccupant.                        */
/* ================================================================== */
void __stdcall World_GetObjectAt(void* obj)
{
    int* occupant;
    int i;

    occupant = (int*)((uint8_t*)obj + VEHICLE_OFF_OCCUPANTS);  /* +0x38 */
    for (i = 8; i != 0; i--) {
        if (*occupant != 0) {
            Building_RemoveOccupant((int*)*occupant);
            *occupant = 0;
        }
        occupant++;
    }
}
