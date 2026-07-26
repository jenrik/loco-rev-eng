/**
 * world.c — World structure function implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the World_* free functions that manage the global g_world
 * singleton (0x4A98B0). These functions handle vehicle lifecycle
 * (play/load/save/clear), per-tick collision detection, arrival handling,
 * audio hit-testing, and depth-sorted sub-object collection.
 *
 * Calling conventions:
 *   __fastcall(ECX=World*) — 1 param only (World pointer in ECX)
 *   __thiscall(ECX=World*, stack=args) — World as implicit this + stack args
 *   __cdecl — stack-based, no this
 */

#include "world.h"

/* ================================================================== */
/* External globals                                                    */
/* ================================================================== */

extern int32_t  g_game_mode;            /* 0x004851F4 */
extern void*    g_netman;               /* 0x004FD3AC */
extern void*    g_town_view;            /* town view singleton */
extern void*    g_ddraw_building;       /* DDRAW building singleton */
extern void*    g_tooltip_mgr;          /* tooltip manager */
extern void*    g_input_mgr;            /* input manager */
extern void*    g_building_mgr;         /* building manager */
extern void*    g_tilemap;              /* TileMap singleton */
extern uint8_t  g_click_on_town;        /* click-on-town flag */
extern int32_t  g_player_id;            /* global player ID */

/* Two global objects released during World_Init */
extern void*    DAT_00485268;           /* @ 0x00485268 */
extern void*    DAT_0048526c;           /* @ 0x0048526c */

/* ================================================================== */
/* External CRT helpers                                                */
/* ================================================================== */
extern void* operator_new(size_t size);  /* operator new */
extern void  operator_delete(void* ptr); /* operator delete */
extern void  GLOBAL_free(void* ptr);     /* game's free @ 0x465E10 */
extern uint32_t CRT_rand(void);          /* rand() @ 0x466510 */

/* ==================================================================== */
/* Forward declarations for external functions called by World functions */
/* ==================================================================== */

extern void CDECL Building_RemoveOccupant(int* occupant);       /* @ 0x4336A0 */
extern void VehicleEditor_Update(void* vehicle);                /* @ 0x44C3A0 */
extern void Vehicle_UpdatePosition(void* vehicle, char param);  /* @ 0x44D500 */
extern void Vehicle_SetState(void* vehicle, int state);         /* @ 0x44CF90 */
extern void NETMAN_HandleTimeout(void* netman, void* vehicle);  /* @ 0x43F380 */
extern void NETMAN_ReceiveGameStart(void* netman, int x, int y,
                                     void* vehicle);            /* @ 0x43E560 */
extern void World_RenderAll(void* vehicle);                     /* @ 0x44E630 */
extern uint __thiscall World_SaveToFile(World* world, uint id,
                                        char player, char flag); /* @ 0x44D8A0 */
extern void GameVehicle_RemoveDestination(int* building, uint id,
                                           char player);        /* @ 0x412B50 */
extern void ArrivalQueue_RemoveVehicle(void* building, uint id,
                                        char player);          /* @ 0x44F410 */
extern void Town_SelectBuilding(void* town_view, int building); /* @ 0x42C9C0 */
extern void DDRAW_SelectBuilding(void* ddraw_building, int building); /* @ 0x46AA80 */
extern void EditorState_DetachCompat(void* editor_state); /* C wrapper for EditorState::Detach() */     /* @ 0x40B5A0 */
extern void* Vehicle_Ctor(void* mem, int resource_id, int dir,
                           char flag1, char flag2);             /* @ 0x44BC90 */
extern void Vehicle_InitRoute(void* vehicle, int resource_id,
                               int dir, char param);            /* @ 0x44C060 */
extern void Vehicle_FindPath(void* vehicle, int* route,
                              char flag);                       /* @ 0x44C280 */
extern int  VehicleEditor_GetResourceId(int sub_obj);           /* @ 0x44AD70 */
extern void VehicleEditor_RemoveVehicle(void* sub_obj, int param); /* @ 0x44BDB0 */
extern uint CGWND_MapResourceToDirection(int resource_id);      /* @ 0x413A80 */
extern void CRT_0x467FF0(char* buf, int len, const char* fmt);  /* @ 0x467FF0 sprintf */
extern void* TileMap_GetObjectAt(void* tilemap, short x,
                                  short y, short layer);        /* @ 0x455620 */
extern void* INPUT_FindObjectAt(void* input_mgr, int param);    /* @ 0x41E8B0 */
extern void ArrivalQueue_AddVehicle(void* building, void* vehicle); /* @ 0x44F2D0 */
extern void UI_CreateMessageBox(void* mgr, int res_id, int, char,
                                 int, int, char);               /* @ 0x428A00 */
extern void CDECL PlaySoundAt(int sound_id, int x,
                               int y, int channel);             /* @ 0x463800 */
extern int  Town_BlitViewport(void* viewport, int a, int b,
                               int c, int d, int e, int f);    /* @ 0x42D1C0 */

/* ==================================================================== */
/* World_Init                                                            */
/* Address: 0x44D9B0                                                    */
/* __fastcall (ECX=World*)                                               */
/*                                                                      */
/* For each active vehicle slot: removes all 8 occupants, renders, and  */
/* saves the vehicle out of the world. Then releases two global objects */
/* at 0x485268 and 0x48526c via their vtable[2] methods.                */
/* ==================================================================== */
void __fastcall World_Init(World* world)
{
    int slot_idx;
    void* vehicle;
    int* occupant;
    int i;

    slot_idx = 4;
    /* Iterate 4 vehicle slots starting at world->vehicles[0] (offset +0x08) */
    while (slot_idx != 0) {
        vehicle = world->vehicles[4 - slot_idx];
        if (vehicle != NULL) {
            /* Remove all 8 occupants at vehicle+0x38..+0x54 */
            occupant = (int*)((int)vehicle + 0x38);
            for (i = 8; i != 0; i--) {
                if (*occupant != 0) {
                    Building_RemoveOccupant((int*)*occupant);
                    *occupant = 0;
                }
                occupant++;
            }
            /* Render final state then save */
            World_RenderAll(vehicle);
            World_SaveToFile(world,
                (uint)*(uint16_t*)((int)vehicle + 0x7a),   /* resource_id */
                *(char*)((int)vehicle + 0x78),              /* player_id */
                (char)1);                                    /* mp_flag */
        }
        slot_idx--;
    }

    /* Release global objects via vtable[2] (index 2 = +0x08) */
    if (DAT_00485268 != NULL) {
        (*(void (**)(void*))(*(int*)DAT_00485268 + 8))(DAT_00485268);
        DAT_00485268 = NULL;
    }
    if (DAT_0048526c != NULL) {
        (*(void (**)(void*))(*(int*)DAT_0048526c + 8))(DAT_0048526c);
        DAT_0048526c = NULL;
    }
}

/* ==================================================================== */
/* World_Shutdown                                                        */
/* Address: 0x44D870                                                    */
/* __fastcall (ECX=World*)                                               */
/*                                                                      */
/* Zeroes the entire World struct: counters, vehicle slots, and the     */
/* 16-entry sub-object array.                                           */
/* ==================================================================== */
void __fastcall World_Shutdown(World* world)
{
    int* ptr;
    int i;

    /* Clear first 6 fields: offsets +0x04 through +0x14 */
    *(int16_t*)((int)world + 0x04) = 0;    /* vehicle_count */
    *(int16_t*)((int)world + 0x06) = 0;    /* field_06 */
    *(int32_t*)((int)world + 0x08) = 0;    /* vehicles[0] */
    *(int32_t*)((int)world + 0x0C) = 0;    /* vehicles[1] */
    *(int32_t*)((int)world + 0x10) = 0;    /* vehicles[2] */
    *(int32_t*)((int)world + 0x14) = 0;    /* vehicles[3] */

    /* Clear 16 sub_object slots at +0x18 through +0x54 (64 bytes) */
    ptr = (int*)((int)world + 0x18);
    for (i = 16; i != 0; i--) {
        *ptr = 0;
        ptr++;
    }
}

/* ==================================================================== */
/* World_Reset                                                           */
/* Address: 0x44DBD0                                                    */
/* __thiscall (this=World*, param_1=char flag)                           */
/*                                                                      */
/* For all active vehicles in states 0, 1, 4, or 5, calls               */
/* Vehicle_UpdatePosition with the given flag.                          */
/* ==================================================================== */
void __thiscall World_Reset(World* world, char flag)
{
    void* vehicle;
    int state;
    int i;

    for (i = 0; i < 4; i++) {
        vehicle = world->vehicles[i];
        if (vehicle != NULL) {
            state = *(int*)((int)vehicle + 0x64);  /* +0x64 = state_2 */
            if (state == 0 || state == 4 || state == 5 || state == 1) {
                Vehicle_UpdatePosition(vehicle, flag);
            }
        }
    }
}

/* ==================================================================== */
/* World_CheckActive                                                     */
/* Address: 0x44DBB0                                                    */
/* __fastcall (ECX=World*)                                               */
/*                                                                      */
/* Returns 1 if vehicle_count >= 4 or field_06 >= 3 (world full).       */
/* ==================================================================== */
char __fastcall World_CheckActive(World* world)
{
    if (world->vehicle_count < 4 && world->field_06 < 3) {
        return 0;
    }
    return 1;
}

/* ==================================================================== */
/* World_FinalizeLoad                                                    */
/* Address: 0x44DF40                                                    */
/* __thiscall (this=World*, vehicle, packed_coords, mp_flag)             */
/*                                                                      */
/* Registers a vehicle in the first empty world slot. Sets up            */
/* destination-based arrival linking. For multiplayer scenarios,         */
/* uses TileMap/INPUT to find destination building and links.            */
/* ==================================================================== */
char __thiscall World_FinalizeLoad(World* world, void* vehicle,
                                    int packed_coords, char mp_flag)
{
    uint16_t slot;
    short dest_x, dest_y;
    void* building;
    int input_param;

    if (world->vehicle_count > 3) {
        return 0;
    }

    /* Find first empty slot */
    for (slot = 0; slot < 4; slot++) {
        if (world->vehicles[slot] == NULL) {
            break;
        }
    }
    if (slot > 3) {
        return 0;
    }

    /* Register vehicle */
    world->vehicles[slot] = vehicle;
    *(int32_t*)((int)vehicle + 0x68) = 0;         /* clear net_sync_flag */
    world->vehicle_count++;

    /* Unpack destination coordinates */
    dest_x = (short)(packed_coords & 0xFFFF);
    dest_y = (short)((packed_coords >> 16) & 0xFFFF);

    /* Check network scenario type */
    if (g_netman != NULL && *(int*)((int)g_netman + 0x5C) == 1) {
        /* Multiplayer scenario 1 */
        input_param = 1;
        building = INPUT_FindObjectAt(g_input_mgr, input_param);
    } else if (g_netman != NULL && *(int*)((int)g_netman + 0x5C) == 2) {
        /* Multiplayer scenario 2: adjust destination based on mp_flag */
        if (mp_flag == 1 || mp_flag == 2) {
            dest_y++;
        }
        building = TileMap_GetObjectAt(g_tilemap, dest_x, dest_y, 0);
    } else {
        /* Single player */
        input_param = 0;
        building = INPUT_FindObjectAt(g_input_mgr, input_param);
    }

    if (building == NULL) {
        building = INPUT_FindObjectAt(g_input_mgr, 1);
        if (building == NULL) {
            /* No destination found — set vehicle to arrival state */
            Vehicle_SetState(vehicle, 3);
            return 1;
        }
    }

    /* Link vehicle to destination building */
    ArrivalQueue_AddVehicle(building, vehicle);
    return 1;
}

/* ==================================================================== */
/* World_LoadFromFile                                                    */
/* Address: 0x44DC10                                                    */
/* __thiscall (this=World*, route_data, vehicle_init)                   */
/*                                                                      */
/* Creates a new vehicle in the first empty world slot. If param_2 is   */
/* NULL, creates a random vehicle. Otherwise uses param_2 data to       */
/* set up the vehicle type and route.                                   */
/* ==================================================================== */
int __thiscall World_LoadFromFile(World* world, int* route_data, int* vehicle_init)
{
    void* vehicle_mem;
    void* vehicle;
    uint16_t slot;
    int resource_id;
    uint dir;
    int rand_val;
    int i;
    char rand_flag;

    /* Bounds check */
    if ((uint16_t)world->vehicle_count > 3 ||
        (uint16_t)world->field_06 > 2) {
        return 0;
    }

    /* Find first empty slot */
    slot = 0;
    while (slot < 4) {
        if (world->vehicles[slot] == NULL) {
            break;
        }
        slot++;
    }

    if (vehicle_init == NULL) {
        /* Random vehicle generation */
        vehicle_mem = operator_new(0x94);
        if (vehicle_mem == NULL) {
            vehicle = NULL;
        } else {
            rand_val = CRT_rand() % 3;
            resource_id = rand_val * 2 + 0x1804;  /* 0x1804, 0x1806, 0x1808 */
            vehicle = Vehicle_Ctor(vehicle_mem, resource_id, 0, 0, 0);
        }
        world->vehicles[slot] = vehicle;

        if (vehicle != NULL) {
            /* Check if resource loaded successfully */
            if (*(int*)((int)vehicle + 0x10) != 0 &&
                *(char*)(*(int*)((int)vehicle + 0x10) + 0x18) == 1) {

                /* Assign random name via sprintf */
                CRT_0x467FF0((char*)&slot, 10, "%s__%lu", 0x47F030);

                /* Random route generation */
                rand_val = CRT_rand() % 5;
                for (i = 0; i < rand_val; i++) {
                    int choice = CRT_rand() % 3;
                    if (choice == 0) {
                        dir = 2;
                        rand_flag = 0;
                        resource_id = (CRT_rand() % 3) * 2 + 0x1866;
                    } else if (choice == 1) {
                        dir = 3;
                        rand_flag = 0;
                        resource_id = 0x186C;
                    } else {
                        dir = 4;
                        rand_flag = 0;
                        resource_id = 0x1870;
                    }
                    Vehicle_InitRoute(vehicle, resource_id, dir, rand_flag);
                }
                goto done_with_load;
            }
            /* Resource not loaded — destroy vehicle */
            (*(void (**)(void*, int))(*(int*)vehicle))(vehicle, 1);
            world->vehicles[slot] = NULL;
        }
    } else {
        /* Specific vehicle creation from save data */
        vehicle_mem = operator_new(0x94);
        if (vehicle_mem == NULL) {
            vehicle = NULL;
        } else {
            vehicle = Vehicle_Ctor(vehicle_mem, *vehicle_init, 0, 0, 0);
        }
        world->vehicles[slot] = vehicle;

        if (vehicle != NULL) {
            /* Check resource loaded */
            if (*(int*)((int)vehicle + 0x10) == 0 ||
                *(char*)(*(int*)((int)vehicle + 0x10) + 0x18) != 1) {
                /* Resource not loaded — destroy */
                (*(void (**)(void*, int))(*(int*)vehicle))(vehicle, 1);
                world->vehicles[slot] = NULL;
                goto done;
            }

            /* Set up route from save data */
            CRT_0x467FF0((char*)&slot, 10, "%s__%lu", 0x47F030);

            Vehicle_UpdatePosition(vehicle, 0);

            /* Route has up to 3 pieces */
            for (i = 0; i < 3; i++) {
                vehicle_init++;
                if (*vehicle_init != 0) {
                    dir = CGWND_MapResourceToDirection(*vehicle_init);
                    Vehicle_InitRoute(vehicle, *vehicle_init, dir, 0);
                    Vehicle_UpdatePosition(vehicle, 0);
                }
            }

done_with_load:
            world->vehicle_count++;
            world->field_06++;
            Vehicle_FindPath(vehicle, route_data, 1);
        }
    }

done:
    return (int)world->vehicles[slot];
}

/* ==================================================================== */
/* World_SaveToFile                                                      */
/* Address: 0x44D8A0                                                    */
/* __thiscall (this=World*, resource_id, player_id, mp_flag)             */
/*                                                                      */
/* Finds a vehicle matching resource_id and player_id, deselects from   */
/* town/DDRAW views if active, notifies network manager, deletes the    */
/* vehicle. Returns packed result with success flag in low byte.        */
/* ==================================================================== */
uint __thiscall World_SaveToFile(World* world, uint resource_id,
                                  char player_id, char mp_flag)
{
    uint slot;
    void* vehicle;
    void** vehicle_slot;
    int result;
    int type;

    for (slot = 0; slot < 4; slot++) {
        vehicle = world->vehicles[slot];
        if (vehicle != NULL) {
            uint16_t veh_id = *(uint16_t*)((int)vehicle + 0x7a);
            char veh_player = *(char*)((int)vehicle + 0x78);
            if (veh_id == (uint16_t)resource_id && veh_player == player_id) {
                break;
            }
        }
    }

    if (slot > 3) {
        /* Not found — return 0 with high byte cleared */
        return (uint)vehicle & 0xFFFFFF00;
    }

    vehicle = world->vehicles[slot];
    vehicle_slot = &world->vehicles[slot];

    /* Check if this vehicle is currently selected in town/DDRAW views */
    if (*(int*)(*(int*)((int)vehicle + 0x10) + 0x14) == *(int*)0x485380 /* selected building */) {
        Town_SelectBuilding(g_town_view, 0);
        DDRAW_SelectBuilding(g_ddraw_building, 0);
    }

    /* Notify network manager */
    NETMAN_HandleTimeout(g_netman, vehicle);

    type = *(int*)((int)vehicle + 4);
    if (type == 0) {
        /* Vehicle type 0 — simple delete */
        world->field_06--;
        if (vehicle != NULL) {
            result = (*(int (**)(void*, int))(*(int*)vehicle))(vehicle, 1);
            *vehicle_slot = NULL;
            world->vehicle_count--;
            return (uint)((result & 0xFFFFFF00) | 1);
        }
    } else {
        /* Check multiplayer scenario 2 with flag */
        if (g_netman != NULL &&
            *(int*)((int)g_netman + 0x5C) == 2 &&
            mp_flag != 0) {
            int unused = NETMAN_ReceiveGameStart(g_netman, 0, 0, vehicle);
            *vehicle_slot = NULL;
            world->vehicle_count--;
            return (uint)((unused & 0xFFFFFF00) | 1);
        }

        if (vehicle != NULL) {
            result = (*(int (**)(void*, int))(*(int*)vehicle))(vehicle, 1);
        }
    }

    *vehicle_slot = NULL;
    world->vehicle_count--;
    return (uint)((result & 0xFFFFFF00) | 1);
}

/* ==================================================================== */
/* World_SerializeObject                                                 */
/* Address: 0x44DA50                                                    */
/* __thiscall (this=World*, player_id)                                   */
/*                                                                      */
/* Finds vehicles matching player_id, clears occupants, renders, saves. */
/* ==================================================================== */
void __thiscall World_SerializeObject(World* world, char player_id)
{
    int i, j;
    void* vehicle;
    int* occupant;

    for (i = 0; i < 4; i++) {
        vehicle = world->vehicles[i];
        if (vehicle != NULL && *(char*)((int)vehicle + 0x78) == player_id) {
            /* Remove all 8 occupants */
            occupant = (int*)((int)vehicle + 0x38);
            for (j = 8; j != 0; j--) {
                if (*occupant != 0) {
                    Building_RemoveOccupant((int*)*occupant);
                    *occupant = 0;
                }
                occupant++;
            }
            /* Render and save */
            World_RenderAll(vehicle);
            World_SaveToFile(world,
                (uint)*(uint16_t*)((int)vehicle + 0x7a),
                player_id, 0);
        }
    }
}

/* ==================================================================== */
/* World_SerializeMap                                                    */
/* Address: 0x44DEA0                                                    */
/* __cdecl                                                               */
/*                                                                      */
/* Sets up route data on a building occupant. Removes old route entries */
/* and installs up to 3 new route points from param_2 data array.       */
/* ==================================================================== */
bool __cdecl World_SerializeMap(int* building, int* route_data)
{
    void* vehicle;
    int i;
    int resource_id;
    uint dir;
    bool result;

    if (*(int*)(building + 0x47) == 1 && /* +0x11C = occupancy flag */
        (vehicle = (void*)building[0x48], vehicle != NULL)) { /* +0x120 = tracked_vehicle */

        resource_id = VehicleEditor_GetResourceId(*(int*)((int)vehicle + 0x10));
        result = (resource_id != *route_data);

        if (result) {
            /* Set new resource on sub-object */
            (*(void (**)(int, int))(**(int**)((int)vehicle + 0x10) + 0x3C))
                (*(int*)((int)vehicle + 0x10), *route_data);
        }

        /* Remove old route entries (3x) */
        for (i = 0; i < 3; i++) {
            VehicleEditor_RemoveVehicle(vehicle, 1);
        }

        /* Install new route entries from route_data[1..3] */
        for (i = 0; i < 3; i++) {
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

/* ==================================================================== */
/* World_DeserializeMap                                                  */
/* Address: 0x44DAD0                                                    */
/* __thiscall (this=World*, building)                                    */
/*                                                                      */
/* Checks each vehicle's sub-objects for bounding-box overlap with a    */
/* given building's rect. On overlap, clears occupants, renders, saves. */
/* ==================================================================== */
void __thiscall World_DeserializeMap(World* world, int* building)
{
    int i, j;
    void* vehicle;
    bool overlap;
    RECT overlap_rect;
    int* occupant;

    for (i = 0; i < 4; i++) {
        vehicle = world->vehicles[i];
        if (vehicle == NULL) continue;

        /* Skip if vehicle state_2 == 2 and building state_code != 4 */
        if (*(int*)((int)vehicle + 0x64) == 2 &&
            *(int*)(building + 0x43) != 4) {
            continue;
        }

        /* Check sub-object overlap */
        overlap = false;
        for (j = 0x10; j < 0x20; j += 4) {  /* sub-object array */
            int* sub_obj = *(int**)((int)vehicle + j);
            if (sub_obj != NULL) {
                if (!overlap) {
                    if (IntersectRect(&overlap_rect,
                            (RECT*)(sub_obj + 2),     /* +0x08 = rect */
                            (RECT*)(building + 2)) == 0) {
                        continue;
                    }
                }
                overlap = true;
            }
        }

        if (overlap) {
            /* Overlap found — clear occupants and save */
            occupant = (int*)((int)vehicle + 0x38);
            for (j = 8; j != 0; j--) {
                if (*occupant != 0) {
                    Building_RemoveOccupant((int*)*occupant);
                    *occupant = 0;
                }
                occupant++;
            }
            World_RenderAll(vehicle);
            World_SaveToFile(world,
                (uint)*(uint16_t*)((int)vehicle + 0x7a),
                *(char*)((int)vehicle + 0x78), 1);
        }
    }
}

/* ==================================================================== */
/* World_UpdateTick                                                      */
/* Address: 0x44E020                                                    */
/* __fastcall (ECX=World*)                                               */
/*                                                                      */
/* Main per-tick update. Only runs during game modes 3 and 9.           */
/* For each active vehicle: processes collision events, updates         */
/* vehicle editor, handles arrival (state=3) and network sync.          */
/* ==================================================================== */
void __fastcall World_UpdateTick(World* world)
{
    int i, j;
    void* vehicle;
    int* occupant;

    if (world->vehicle_count == 0) return;
    if (g_game_mode != 3 && g_game_mode != 9) return;

    for (i = 0; i < 4; i++) {
        vehicle = world->vehicles[i];
        if (vehicle == NULL) continue;

        /* Step 1: Process collision/overlap events */
        World_ProcessEvents(world, vehicle);

        /* Step 2: Update vehicle editor positions */
        VehicleEditor_Update(vehicle);

        /* Step 3: Check vehicle state */
        if (*(int*)((int)vehicle + 0x5C) == 3) {  /* vehicle_state == ARRIVED */
            /* Arrival — clear all occupants */
            occupant = (int*)((int)vehicle + 0x38);
            for (j = 8; j != 0; j--) {
                if (*occupant != 0) {
                    Building_RemoveOccupant((int*)*occupant);
                    *occupant = 0;
                }
                occupant++;
            }
            /* Render and save */
            World_RenderAll(vehicle);
            World_SaveToFile(world,
                (uint)*(uint16_t*)((int)vehicle + 0x7a),
                *(char*)((int)vehicle + 0x78), 1);

        } else if (*(int*)((int)vehicle + 0x68) == 1) { /* net_sync_flag */
            /* Network sync needed */
            int packed_coords = *(int*)(
                *(int*)(*(int*)((int)vehicle + 0x20) + 0x14) + 0x88);

            *(int*)((int)vehicle + 0x68) = 2;   /* mark synced */

            /* Clear occupants if type != 0 */
            if (*(int*)((int)vehicle + 4) != 0) {
                occupant = (int*)((int)vehicle + 0x38);
                for (j = 8; j != 0; j--) {
                    if (*occupant != 0) {
                        Building_RemoveOccupant((int*)*occupant);
                        *occupant = 0;
                    }
                    occupant++;
                }
            }

            /* Render, remove from world, notify network */
            World_RenderAll(vehicle);
            NETMAN_ReceiveGameStart(g_netman,
                (int16_t)(packed_coords & 0xFFFF),
                (int16_t)((packed_coords >> 16) & 0xFFFF),
                vehicle);

            world->vehicles[i] = NULL;
            world->vehicle_count--;
        }
    }
}

/* ==================================================================== */
/* World_ProcessEvents                                                   */
/* Address: 0x44E3F0                                                    */
/* __thiscall (this=World*, current_vehicle)                             */
/*                                                                      */
/* Collision/overlap detection. Checks each vehicle's sub-objects       */
/* against other vehicles for bounding-box overlap. On collision:       */
/* stops both vehicles, sets random 1-100 tick wait.                    */
/* ==================================================================== */
uint __thiscall World_ProcessEvents(World* world, void* current_vehicle)
{
    int i, j;
    void* other;
    void* current_sub_obj;

    /* Skip if fewer than 2 vehicles, target is NULL, not moving, etc. */
    if (world->vehicle_count <= 1) return 0;
    if (current_vehicle == NULL) return 0;
    if (*(int*)((int)current_vehicle + 0x5C) != 2) return 0; /* not MOVING */
    if (*(int*)((int)current_vehicle + 0x60) == 2 ||           /* action_state == 2 */
        *(int*)((int)current_vehicle + 0x60) == 3) return 0;   /* action_state == 3 */
    if (*(int*)((int)current_vehicle + 0x64) == 2) return 0;   /* state_2 == 2 */

    current_sub_obj = *(void**)((int)current_vehicle + 0x20);

    for (i = 0; i < 4; i++) {
        other = world->vehicles[i];
        if (other == NULL) continue;
        if (*(int*)((int)other + 0x60) == 2 ||                 /* action_state == 2 */
            *(int*)((int)other + 0x60) == 3) continue;         /* action_state == 3 */
        if (*(int*)((int)other + 0x64) == 2) continue;         /* state_2 == 2 */
        if (*(int*)((int)other + 0x5C) == 4 &&                 /* state == STOPPED */
            *(int*)((int)other + 0x28) == 0) continue;         /* wait expired */
        if (other == current_vehicle) continue;                 /* skip self */

        /* Check sub-object overlap */
        for (j = 0; j <= (int)*(uint16_t*)((int)other + 0x0C); j++) {
            int* other_sub_obj_ptr = *(int**)((int)other + 0x10 + j * 4);
            if (other_sub_obj_ptr == NULL) continue;

            /* Skip if sharing the same parent building */
            if (*(int*)(*(int*)(other_sub_obj_ptr[0x10C]) + 0x14) ==
                    *(int*)(*(int*)((int)current_sub_obj + 0x14) + 0x14)) {
                continue;
            }
            if (*(int*)(*(int*)(other_sub_obj_ptr[0x10D]) + 0x14) ==
                    *(int*)(*(int*)((int)current_sub_obj + 0x14) + 0x14)) {
                continue;
            }

            /* Bounding-box overlap test using sub_obj rects */
            int cur_left   = *(int*)((int)current_sub_obj + 0x08);
            int cur_top    = *(int*)((int)current_sub_obj + 0x0C);
            int cur_right  = *(int*)((int)current_sub_obj + 0x10);
            int cur_bottom = *(int*)((int)current_sub_obj + 0x14);

            int other_left   = other_sub_obj_ptr[2];   /* +0x08 */
            int other_top    = other_sub_obj_ptr[3];   /* +0x0C */
            int other_right  = other_sub_obj_ptr[4];   /* +0x10 */
            int other_bottom = other_sub_obj_ptr[5];   /* +0x14 */

            /* Simple rect overlap check */
            if (other_left < cur_right && cur_right < other_right &&
                other_top < cur_bottom && cur_bottom < other_bottom) {

                /* Check if building type allows collision */
                if (*(int*)(*(int*)((int)current_vehicle + 0x20) + 0x1C) == 2) {
                    continue; /* type 2 buildings don't collide */
                }

                /* Visual overlap check via Town_BlitViewport */
                int viewport_result = Town_BlitViewport(
                    *(void**)(other_sub_obj_ptr[0x10] + 0x10), /* +0x40 resource */
                    other_sub_obj_ptr[0x0C],                    /* +0x30 */
                    other_sub_obj_ptr[0x0D],                    /* +0x34 */
                    other_sub_obj_ptr[0x0E],                    /* +0x38 */
                    other_sub_obj_ptr[0x0F],                    /* +0x3C */
                    ((uint)*(uint16_t*)(other_sub_obj_ptr[0x10] + 0x14) *
                     (uint)*(uint16_t*)(other_sub_obj_ptr + 0x10E) -
                     other_sub_obj_ptr[2]) + cur_top,           /* adjusted X offset */
                    cur_bottom - other_sub_obj_ptr[3]);          /* height diff */

                if (viewport_result == 0) {
                    /* COLLISION DETECTED — stop current vehicle */
                    Vehicle_SetState(current_vehicle, 4);        /* STOPPED */
                    uint wait = CRT_rand() % 100 + 1;
                    *(int*)((int)current_vehicle + 0x28) = wait;

                    /* Show collision message box */
                    UI_CreateMessageBox(g_tooltip_mgr, 0x3861, 0, 'W',
                        *(int*)(*(int*)((int)current_vehicle + 0x20) + 0x0C),
                        *(int*)(*(int*)((int)current_vehicle + 0x20) + 0x10), 1);

                    /* Also stop the other vehicle if active */
                    if (*(int*)((int)world->vehicles[i] + 0x5C) != 0) {
                        Vehicle_SetState(world->vehicles[i], 4);
                        wait = CRT_rand() % 100 + 1;
                        *(int*)((int)world->vehicles[i] + 0x28) = wait;
                    }
                    break;
                }
            }
        }
    }
    return 0;
}

/* ==================================================================== */
/* World_RenderAll                                                       */
/* Address: 0x44E630                                                    */
/* __cdecl (param_1=arriving vehicle pointer)                            */
/*                                                                      */
/* Handles vehicle arrival at destination. Removes from arrival queue,  */
/* clears tracked_vehicle on destination building, detaches editor      */
/* visual states from all sub-objects.                                  */
/* ==================================================================== */
void CDECL World_RenderAll(void* vehicle)
{
    int i, j;
    void* building;
    int* sub_obj_parent;

    if (vehicle == NULL) return;

    /* Step 1: Update position */
    Vehicle_UpdatePosition(vehicle, 0);

    /* Step 2: Check destination tile */
    building = TileMap_GetObjectAt(g_tilemap,
        *(int16_t*)((int)vehicle + 0x2E),    /* tile_x */
        *(int16_t*)((int)vehicle + 0x30) + 1,/* tile_y + 1 */
        0);

    if (building != NULL) {
        switch (*(int*)((int)vehicle + 0x60)) {  /* action_state */
        case 2:
        case 3:
            /* Unloading/Arriving — remove from arrival queue */
            ArrivalQueue_RemoveVehicle(building,
                (uint)*(uint16_t*)((int)vehicle + 0x7A),
                *(char*)((int)vehicle + 0x78));
            /* Fall through to clear tracked vehicle */
        case 0:
        case 1:
        case 4:
        case 5:
            *(int*)((int)building + 0x11C) = 0;  /* clear arrival queue ptr */
            break;
        }
    }

    /* Clear current tile coords */
    *(int16_t*)((int)vehicle + 0x2E) = -1;    /* tile_x */
    *(int16_t*)((int)vehicle + 0x30) = -1;    /* tile_y */

    /* Step 3: Check destination tile (second set of coords) */
    switch (*(int*)((int)vehicle + 0x64)) {     /* state_2 */
    case 1:
    case 2:
    case 4:
    case 5: {
        int* building_ptr = (int*)TileMap_GetObjectAt(g_tilemap,
            *(int16_t*)((int)vehicle + 0x32),    /* dest_x */
            *(int16_t*)((int)vehicle + 0x34) + 1,/* dest_y + 1 */
            0);

        if (building_ptr != NULL) {
            if (*(int*)((int)vehicle + 0x64) == 2) {
                GameVehicle_RemoveDestination(building_ptr,
                    (uint)*(uint16_t*)((int)vehicle + 0x7A),
                    *(char*)((int)vehicle + 0x78));
            }

            /* Clear tracked vehicle on building */
            if (building_ptr[0x48] == (int)vehicle) {
                building_ptr[0x48] = 0;             /* tracked_vehicle = NULL */
                *(char*)(building_ptr + 0x4A) = 0;  /* tracked_vehicle_flag */
                (*(void (**)(int))(*(int*)building_ptr + 0x1C))(0); /* SetAnimState */
                building_ptr[0x47] = 0;              /* arrival queue state */
            }
        }
        break;
    }
    }

    /* Clear dest coords */
    *(int16_t*)((int)vehicle + 0x32) = -1;    /* dest_x */
    *(int16_t*)((int)vehicle + 0x34) = -1;    /* dest_y */

    /* Step 4: Detach editor states from main sub-object's parent */
    sub_obj_parent = *(int**)(*(int*)((int)vehicle + 0x20) + 0x14);
    if (sub_obj_parent != NULL && *(int*)(sub_obj_parent + 0x43) == 7) {
        (*(void (**)(int))(*(int*)sub_obj_parent + 0x1C))(1); /* SetAnimState */
    }

    /* Step 5: Iterate sub-objects and detach editor visual states */
    for (i = 0; i <= (int)*(uint16_t*)((int)vehicle + 0x0C); i++) {
        int* sub_obj = *(int**)((int)vehicle + 0x10 + i * 4);
        if (sub_obj == NULL) continue;

        int* editor1 = *(int**)(*(int*)(sub_obj[0x10C]) + 0x14); /* +0x430 -> +0x14 */
        if (editor1 != NULL && *(int*)(editor1 + 0x43) == 7) {
            (*(void (**)(int))(*(int*)editor1 + 0x1C))(1);
        }

        int* editor2 = *(int**)(*(int*)(sub_obj[0x10D]) + 0x14); /* +0x434 -> +0x14 */
        if (editor2 != NULL && *(int*)(editor2 + 0x43) == 7) {
            (*(void (**)(int))(*(int*)editor2 + 0x1C))(1);
        }
    }

    /* Step 6: Detach via GAMESTATE (second pass) */
    for (i = 0; i <= (int)*(uint16_t*)((int)vehicle + 0x0C); i++) {
        int* sub_obj = *(int**)((int)vehicle + 0x10 + i * 4);
        if (sub_obj != NULL) {
            EditorState_DetachCompat(*(int*)(sub_obj + 0x10C)); /* +0x430 */
            EditorState_DetachCompat(*(int*)(sub_obj + 0x10D)); /* +0x434 */
        }
    }

    /* Step 7: Detach editor state from main sub-object */
    if (*(int*)((int)vehicle + 0x20) != 0) {
        EditorState_DetachCompat(*(int*)(*(int*)((int)vehicle + 0x20) + 0x14));
    }
}

/* ==================================================================== */
/* World_ProcessAudio                                                    */
/* Address: 0x44E830                                                    */
/* __thiscall (this=World*, audio_x, audio_y)                            */
/*                                                                      */
/* Audio hit-test: checks each vehicle's sub-objects against audio      */
/* position. Calls vtable[2] method on sub-object to test hit.          */
/* Selects building in town view on match.                              */
/* ==================================================================== */
char __thiscall World_ProcessAudio(World* world, int audio_x, int audio_y)
{
    int i, j;
    void* vehicle;
    int* sub_obj;
    char result = 0;

    if (g_click_on_town == 0) {
        return 0;
    }

    for (i = 0; i < 4; i++) {
        vehicle = world->vehicles[i];
        if (vehicle == NULL) continue;

        for (j = 0; j <= (int)*(uint16_t*)((int)vehicle + 0x0C); j++) {
            sub_obj = *(int**)((int)vehicle + 0x10 + j * 4);
            if (sub_obj == NULL) continue;

            /* Check if sub-object is active */
            if (*(char*)(sub_obj + 9) == 1) {  /* +0x24 = visible/active flag */
                char hit = (*(char (**)(int, int))(*(int*)sub_obj + 8))
                    (audio_x, audio_y);
                if (hit != 0) {
                    Town_SelectBuilding(g_town_view, *(int*)((int)vehicle + 0x10 + j * 4));
                    result = 1;
                    break;
                }
            }
        }
    }
    return result;
}

/* ==================================================================== */
/* World_InitTimer                                                       */
/* Address: 0x44E160                                                    */
/* __thiscall (this=World*, building_id)                                 */
/*                                                                      */
/* Checks if any active vehicle references the given building ID.       */
/* Returns 1 if found. Used to determine if world timers should run.    */
/* ==================================================================== */
char __thiscall World_InitTimer(World* world, int building_id)
{
    int i, j;
    void* vehicle;
    int* sub_obj;

    for (i = 0; i < 4; i++) {
        vehicle = world->vehicles[i];
        if (vehicle == NULL) continue;

        /* Check main sub-object */
        if (*(int*)((int)vehicle + 0x20) != 0 &&
            *(int*)(*(int*)((int)vehicle + 0x20) + 0x14) == building_id) {
            return 1;
        }

        /* Check array sub-objects */
        for (j = 0; j <= (int)*(uint16_t*)((int)vehicle + 0x0C); j++) {
            sub_obj = *(int**)((int)vehicle + 0x10 + j * 4);
            if (sub_obj == NULL) continue;

            /* Check editor state buildings for matching ID */
            if (*(int*)(*(int*)(sub_obj[0x10C]) + 0x14) == building_id ||  /* +0x430 -> +0x14 */
                *(int*)(*(int*)(sub_obj[0x10D]) + 0x14) == building_id) {  /* +0x434 -> +0x14 */
                return 1;
            }
        }
    }
    return 0;
}

/* ==================================================================== */
/* World_Lock                                                            */
/* Address: 0x44E200                                                    */
/* __fastcall (ECX=World*)                                               */
/*                                                                      */
/* Collects non-excluded sub-objects from active vehicles and           */
/* bubble-sorts by depth priority. Used for z-order during rendering.   */
/* ==================================================================== */
void __fastcall World_Lock(World* world)
{
    int i;
    int slot_count;
    void* vehicle;
    int* sub_obj;
    bool swapped;

    slot_count = 0;

    if (world->vehicle_count == 0) return;

    /* Step 1: Collect non-excluded sub-objects */
    for (i = 0; i < 4; i++) {
        vehicle = world->vehicles[i];
        if (vehicle == NULL) continue;

        int sub_idx = 0;
        int sub_offset = 0x10;
        while (sub_idx <= (int)*(uint16_t*)((int)vehicle + 0x0C) && sub_offset < 0x20) {
            sub_obj = *(int**)((int)vehicle + sub_offset);
            if (sub_obj != NULL) {
                /* Skip excluded types */
                if (*(int*)(sub_obj + 0x111) != 2 &&      /* +0x444 */
                    *(int*)(sub_obj + 0x111) != 5 &&
                    *(int*)(sub_obj + 0x110) != 2) {      /* +0x440 */

                    world->sub_objects[slot_count] = sub_obj;
                    slot_count++;
                }
            }
            sub_idx++;
            sub_offset += 4;
        }
    }

    if (slot_count == 0) return;

    /* Step 2: Bubble-sort by depth order (descending depth => lower depth first) */
    do {
        swapped = false;
        for (i = 0; i < slot_count - 1 && i < 15; i++) {
            int* obj_a = (int*)world->sub_objects[i];
            int* obj_b = (int*)world->sub_objects[i + 1];
            if (obj_a == NULL || obj_b == NULL) break;

            int depth_a = *(int*)(*(int*)((int)obj_a + 0x430) + 0x10);
            int depth_b = *(int*)(*(int*)((int)obj_b + 0x430) + 0x10);
            if (depth_b < depth_a) {
                /* Swap */
                world->sub_objects[i] = obj_b;
                world->sub_objects[i + 1] = obj_a;
                swapped = true;
            }
        }
    } while (swapped);
}

/* ==================================================================== */
/* World_Unlock                                                          */
/* Address: 0x44E2D0                                                    */
/* __fastcall (ECX=World*)                                               */
/*                                                                      */
/* Clears the sub_objects[16] array (resets it to all zeros).           */
/* ==================================================================== */
void __fastcall World_Unlock(World* world)
{
    int i;
    for (i = 0; i < 16; i++) {
        world->sub_objects[i] = NULL;
    }
}

/* ==================================================================== */
/* World_InvalidateRect                                                  */
/* Address: 0x44E2E0                                                    */
/* __thiscall (this=World*, x, y, param3, param4, scroll_stop)          */
/*                                                                      */
/* Iterates the depth-sorted sub_object array. For each matching tile   */
/* at (x,y), blits background via VehicleEditor_BlitBackground.         */
/* On scroll_stop==1, checks adjacent sub-obj with same depth/layer.    */
/* ==================================================================== */
void __thiscall World_InvalidateRect(World* world, int x, int y,
                                      int param3, int param4, short scroll_stop)
{
    int i;
    void* sub_obj;

    for (i = 0; i < 16; i++) {
        sub_obj = world->sub_objects[i];
        if (sub_obj == NULL) return;

        /* Check if sub-object bounds include this tile */
        char in_bounds = (char)VehicleEditor_IsInBounds(sub_obj,
            (short)x, (short)y, scroll_stop);

        if (in_bounds) {
            VehicleEditor_BlitBackground(sub_obj, x, y);

            /* If scroll_stop==1, check adjacent sub-objects with same depth */
            if (scroll_stop == 1) {
                int j = i + 1;
                while (j < 16) {
                    void* next = world->sub_objects[j];
                    if (next == NULL) break;

                    /* Same layer depth and connectivity check */
                    if (*(int*)((int)next + 0x44C) ==
                        *(int*)((int)sub_obj + 0x44C) &&
                        *(int16_t*)((int)next + 0x448) == 0) {

                        char adjacent = (char)VehicleEditor_IsInBounds(next,
                            (short)x, (short)y, 0);
                        if (adjacent) {
                            VehicleEditor_BlitBackground(
                                world->sub_objects[j], x, y);
                        }
                        break;
                    }
                    j++;
                }
            }
        }
    }
}

/* ==================================================================== */
/* World_GetObjectAt                                                     */
/* Address: 0x44E800                                                    */
/* __cdecl                                                               */
/*                                                                      */
/* Simple helper: remove all 8 occupant slots from an object.           */
/* ==================================================================== */
void CDECL World_GetObjectAt(void* obj)
{
    int* occupant;
    int i;

    occupant = (int*)((int)obj + 0x38);
    for (i = 8; i != 0; i--) {
        if (*occupant != 0) {
            Building_RemoveOccupant((int*)*occupant);
            *occupant = 0;
        }
        occupant++;
    }
}
