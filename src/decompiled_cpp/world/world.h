#ifndef WORLD_H
#define WORLD_H
/**
 * world.h — World structure and associated free functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The World struct (at g_world, 0x4A98B0) manages the in-game active
 * vehicles. It stores up to 4 active vehicle pointers, two short counters,
 * and a depth-sorted array of sub-objects used for z-order rendering.
 *
 * The World_* functions are C free functions (not C++ methods) that operate
 * on the global g_world singleton. They handle vehicle lifecycle
 * (add/remove/save/load), collision/overlap detection, per-tick updates,
 * audio hit-testing, and depth-sorted sub-object collection.
 *
 * Layout:
 *   +0x00: Start of struct (4 bytes unused/unknown)
 *   +0x04: short vehicle_count (max 4)
 *   +0x06: short field_06 (destination count?)
 *   +0x08: void* vehicles[4] (up to 4 active vehicles)
 *   +0x18: void* sub_objects[16] (depth-sorted sub-obj ptrs, via World_Lock)
 *   Total: 0x58 bytes
 *
 * Global: g_world at 0x4A98B0
 */

#pragma once

#include "../shared/types.h"

/* ================================================================== */
/* Global address                                                      */
/* ================================================================== */
#define ADDR_g_world                     0x004A98B0  /* World singleton */

/* ================================================================== */
/* World struct — manages active vehicles                              */
/* ================================================================== */
typedef struct World {
    /* Unknown/first field */
    int32_t     _pad_00;                /* +0x00 */

    /* Counters */
    int16_t     vehicle_count;          /* +0x04  active vehicles (max 4) */
    int16_t     field_06;               /* +0x06  (destination count?) */

    /* Vehicle array */
    void*       vehicles[4];            /* +0x08  up to 4 active vehicle pointers */

    /* Depth-sorted sub-objects (populated by World_Lock) */
    void*       sub_objects[16];        /* +0x18  z-order sorted for rendering */

    /* Total: 0x58 bytes */
} World;

/* ================================================================== */
/* Vehicle subtypes — field offsets used in World functions            */
/* ================================================================== */
/* Vehicle fields (generic Building-derived object):
 *   +0x00: vtable pointer
 *   +0x04: type/id
 *   +0x0C: short sub_object_count
 *   +0x10: void* sub_objects[] (array of pointers to sub-object structs)
 *   +0x20: resource/data pointer (also used for InitBase dispatch)
 *   +0x28: timer/wait value
 *   +0x2E: short tile_x (current tile position X)
 *   +0x30: short tile_y (current tile position Y)
 *   +0x32: short dest_x (destination tile X)
 *   +0x34: short dest_y (destination tile Y)
 *   +0x38: void* occupant_slots[8] (8 occupant pointers)
 *   +0x5C: int vehicle_state (0=idle, 2=moving, 3=arrived, 4=stopped)
 *   +0x60: int action_state (0-5, 2=unloading, 3=arriving, etc)
 *   +0x64: int state_2 (0-5, secondary state)
 *   +0x68: int net_sync_flag (1=needs network sync)
 *   +0x78: char owner/player ID
 *   +0x7A: short resource/station ID
 *   +0x78-0x7A: identifiers matched in serialization
 *   +0x0C (sub_object struct): short sub_obj_count
 *   +0x10 (sub_object struct): void* sub_objects[] (varies)
 */

/* Sub-object fields (referenced as *(type*)(sub_obj + offset)):
 *   +0x00: vtable
 *   +0x04: int resource_id
 *   +0x08: char type_code (3=track, 0xC=scenery, 0xD=???)
 *   +0x10: int depth_order (from *(*ptr+0x430)+0x10)
 *   +0x14: void* building_ptr (parent building/vehicle)
 *   +0x1C: int anim_state (used for vtable[7] dispatch)
 *   +0x40: int resource_data
 *   +0x88: short tile_x-ish
 *   +0x8A: short tile_y-ish
 *   +0xD4: int tile_rect_unpacked[4] (from TileMap_GetTileRect)
 *   +0xE4: int stored_tile_index
 *   +0xF8: int tile_buildable_unpacked[4] (from TileMap_GetTileAt)
 *   +0x108: int tile_buildable_index
 *   +0x10C: int state_code (4=normal, visited by World_DeserializeMap)
 *   +0x11C: int field_11c (occupant/arrival queue pointer, cleared on arrival)
 *   +0x120: void* tracked_vehicle (the vehicle tracking this building)
 *   +0x128: char tracked_vehicle_flag
 *   +0x430: void* editor_state (VehicleEditor state object)
 *   +0x434: void* editor_state_2 (secondary editor state)
 *   +0x438: short field_438 (viewport sub-obj index)
 *   +0x440: int exclusion_flag (2=excluded)
 *   +0x444: int exclusion_flag_2 (2 or 5 = excluded)
 *   +0x448: short field_448 (connectivity)
 *   +0x44C: int field_44C (depth/layer identifier)
 */

/* ================================================================== */
/* External globals referenced by World functions                      */
/* ================================================================== */
extern int32_t  g_game_mode;            /* 0x004851F4 — 3=town, 9=network */
extern void*    g_netman;               /* 0x004FD3AC — NetMan singleton */
extern int32_t  g_world;                /* 0x004A98B0 — World singleton */
extern void*    g_town_view;            /* town view singleton */
extern void*    g_ddraw_building;       /* DDRAW building singleton */
extern void*    g_tooltip_mgr;          /* tooltip manager */
extern int32_t  g_click_on_town;        /* click-on-town flag */
extern int32_t  g_player_id;            /* global player ID */
extern void*    g_tilemap;              /* TileMap singleton */
extern void*    g_input_mgr;            /* input manager */

/* ================================================================== */
/* External function declarations                                      */
/* ================================================================== */
extern void     Building_RemoveOccupant(int* occupant);    /* @ 0x4336A0 */
extern void     VehicleEditor_Update(void* vehicle);       /* @ 0x44C3A0 */
extern void     Vehicle_UpdatePosition(void* vehicle, char param); /* @ 0x44D500 */
extern void     Vehicle_SetState(void* vehicle, int state);/* @ 0x44CF90 */
extern void     VehicleEditor_RemoveVehicle(void* sub_obj, int param); /* @ 0x44BDB0 */
extern void     Vehicle_InitRoute(void* vehicle, int resource_id,
                                  int direction, char param); /* @ 0x44C060 */
extern void     Vehicle_FindPath(void* vehicle, int* route, char flag); /* @ 0x44C280 */
extern void     GameVehicle_RemoveDestination(int* building, uint id, char player); /* @ 0x412B50 */
extern void     ArrivalQueue_RemoveVehicle(void* building, uint id, char player); /* @ 0x44F410 */
extern void     VehicleEditor_IsInBounds(void* sub_obj, short x, short y, short param); /* @ 0x44ABD0 */
extern void     VehicleEditor_BlitBackground(void* sub_obj, int x, int y); /* @ 0x44ADD0 */
/* EditorState::Detach in world/EditorState.h */ /* @ 0x40B5A0 */
extern void*    TileMap_GetObjectAt(void* tilemap, short x, short y, short layer); /* @ 0x455620 */
extern void     Town_SelectBuilding(void* town_view, int building); /* @ 0x42C9C0 */
extern void     DDRAW_SelectBuilding(void* ddraw_building, int building); /* @ 0x46AA80 */
extern void     NETMAN_HandleTimeout(void* netman, void* vehicle); /* @ 0x43F380 */
extern void     NETMAN_ReceiveGameStart(void* netman, int x, int y, void* vehicle); /* @ 0x43E560 */
extern void*    INPUT_FindObjectAt(void* input_mgr, int param); /* @ 0x41E8B0 */
extern void     ArrivalQueue_AddVehicle(void* building, void* vehicle); /* @ 0x44F2D0 */
extern void     UI_CreateMessageBox(void* mgr, int res_id, int, char, int, int, char); /* @ 0x428A00 */
extern void     CDECL PlaySoundAt(int sound_id, int x, int y, int channel); /* @ 0x463800 */
extern int      Town_BlitViewport(void* viewport, int a, int b, int c, int d, int e, int f); /* @ 0x42D1C0 */
extern int      VehicleEditor_GetResourceId(int sub_obj);  /* @ 0x44AD70 */
extern uint     CGWND_MapResourceToDirection(int resource_id); /* @ 0x413A80 */
extern int      IntersectRect(void* dst, void* src1, void* src2); /* Win32 API */

/* ================================================================== */
/* World lifecycle functions                                           */
/* ================================================================== */

/**
 * Remove all vehicles from the world.
 * Address: 0x44D9B0
 * __fastcall (ECX=World*)
 */
void __fastcall World_Init(World* world);

/**
 * Clear/zero the entire World struct.
 * Address: 0x44D870
 * __fastcall (ECX=World*)
 */
void __fastcall World_Shutdown(World* world);

/**
 * Reset all vehicles in states 0, 1, 4, or 5.
 * Address: 0x44DBD0
 * __thiscall (this=World*, param_1=char flag)
 */
void __thiscall World_Reset(World* world, char flag);

/**
 * Check if the world has reached maximum capacity.
 * Address: 0x44DBB0
 * __fastcall (ECX=World*)
 */
char __fastcall World_CheckActive(World* world);

/**
 * Finalize vehicle loading — registers vehicle in empty slot, links arrival.
 * Address: 0x44DF40
 * __thiscall (this=World*, vehicle, packed_coords, mp_flag)
 */
char __thiscall World_FinalizeLoad(World* world, void* vehicle, int packed_coords, char mp_flag);

/* ================================================================== */
/* World serialization functions                                       */
/* ================================================================== */

/**
 * Load vehicles from a save file slot.
 * Address: 0x44DC10
 * __thiscall (this=World*, route_data, vehicle_init)
 */
int __thiscall World_LoadFromFile(World* world, int* route_data, int* vehicle_init);

/**
 * Save/remove a vehicle matching the given resource ID and player char.
 * Address: 0x44D8A0
 * __thiscall (this=World*, resource_id, player_id, mp_flag)
 */
uint __thiscall World_SaveToFile(World* world, uint resource_id, char player_id, char mp_flag);

/**
 * Find+clear+save vehicle matching player char.
 * Address: 0x44DA50
 * __thiscall (this=World*, player_id)
 */
void __thiscall World_SerializeObject(World* world, char player_id);

/**
 * Serialize route data back onto a building occupant.
 * Address: 0x44DEA0
 * __cdecl
 */
bool __cdecl World_SerializeMap(int* building, int* route_data);

/**
 * Deserialize map: check overlap with building rect, clear occupants.
 * Address: 0x44DAD0
 * __thiscall (this=World*, building)
 */
void __thiscall World_DeserializeMap(World* world, int* building);

/* ================================================================== */
/* World per-frame update functions                                    */
/* ================================================================== */

/**
 * Main per-tick update. Runs during game modes 3 and 9.
 * Address: 0x44E020
 * __fastcall (ECX=World*)
 */
void __fastcall World_UpdateTick(World* world);

/**
 * Collision/overlap detection for vehicles.
 * Address: 0x44E3F0
 * __thiscall (this=World*, vehicle)
 */
uint __thiscall World_ProcessEvents(World* world, void* vehicle);

/**
 * Handle vehicle arrival at destination (misnamed — not rendering).
 * Address: 0x44E630
 * __cdecl
 */
void CDECL World_RenderAll(void* vehicle);

/**
 * Audio hit-test: check sub-objects at position.
 * Address: 0x44E830
 * __thiscall (this=World*, audio_x, audio_y)
 */
char __thiscall World_ProcessAudio(World* world, int audio_x, int audio_y);

/* ================================================================== */
/* World query/timer functions                                         */
/* ================================================================== */

/**
 * Check if any vehicle references the given building ID.
 * Address: 0x44E160
 * __thiscall (this=World*, building_id)
 */
char __thiscall World_InitTimer(World* world, int building_id);

/**
 * Collect non-excluded sub-objects and bubble-sort by depth.
 * Address: 0x44E200
 * __fastcall (ECX=World*)
 */
void __fastcall World_Lock(World* world);

/**
 * Unlock — clears sub_objects array.
 * Address: 0x44E2D0
 * __fastcall (ECX=World*)
 */
void __fastcall World_Unlock(World* world);

/**
 * Invalidate rectangle — blit background for matching tiles.
 * Address: 0x44E2E0
 * __thiscall (this=World*, x, y, param3, param4, scroll_stop)
 */
void __thiscall World_InvalidateRect(World* world, int x, int y,
                                     int param3, int param4, short scroll_stop);

/**
 * Remove all 8 occupant slots from the given object.
 * Address: 0x44E800
 * __cdecl
 */
void CDECL World_GetObjectAt(void* obj);

#endif /* WORLD_H */
