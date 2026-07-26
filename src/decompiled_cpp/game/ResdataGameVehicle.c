/**
 * ResdataGameVehicle.c — RESDATA_GameVehicle implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the RESDATA_GameVehicle class (0x11C bytes, vtable at 0x478308).
 * This is the base class for all moving game entities (vehicles, trains,
 * pedestrians) that follow tracks through the town.
 *
 * Functions:
 *   RESDATA_GameVehicle_InitState  (0x44ADF0, 134b) — State machine entry
 *   RESDATA_GameVehicle_Ctor       (0x44AE80, 426b) — Constructor
 *   RESDATA_GameVehicle_Dtor       (0x44B030, 30b)  — Scalar deleting destructor
 *   RESDATA_GameVehicle_BaseDtor   (0x44B050, 93b)  — Base destructor
 *   RESDATA_GameVehicle_InitSounds (0x44B0B0, 250b) — Sound initialization
 */

#include "ResdataGameVehicle.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void  GLOBAL_free(void* ptr);
extern void* __thiscall RESDATA_GameObject_Ctor(void* this, int32_t param_1);
extern void  __fastcall GameObject_StopSound(void* obj, int32_t soundIdx);
extern void  __fastcall World_DeserializeMap(void* world, int32_t param_1);
extern void  __fastcall BuildingMgr_DestroyObjectGroup(void* param_1);
extern uint8_t __fastcall RESDATA_IsRoadTile(int32_t tileObj);

extern uint8_t __fastcall RESDATA_ScriptedObject_EnterBuildMode(void* this, uint8_t param_1);
extern void  __fastcall CGWND_SetBuildMode(uint8_t mode);
extern void  __fastcall CGWND_TrackPiece_SetZoom(void* trackPiece, int16_t zoom);
extern void  __fastcall Game_UpdateCursorMode(int32_t* game);
extern uint8_t __fastcall World_InitTimer(void* world, int32_t param_1);
extern void  __fastcall RESDATA_GameVehicle_InitSounds(int32_t* param_1);

/* ================================================================== */
/* Global references                                                    */
/* ================================================================== */
extern void* g_game;          /* Game singleton */
extern int32_t g_game_mode;   /* Current game mode */

/* ================================================================== */
/* RESDATA_GameVehicle_InitState — State machine transitions            */
/* Address: 0x44ADF0                                                   */
/* ================================================================== */
uint8_t __thiscall RESDATA_GameVehicle_InitState(void* this, int32_t param_1)
{
    uint8_t* self = (uint8_t*)this;

    /* Only process when sub-state is 3 */
    if (*(int16_t*)(self + 0x740) != 3) {
        return 0;
    }

    if (param_1 == 0x1b) {
        /* Enter build mode */
        RESDATA_ScriptedObject_EnterBuildMode(this, 0);
        return 1;
    }

    if (param_1 != 8 && param_1 != 0x2e) {
        return 0;  /* Unknown state code */
    }

    /* State 8 or 0x2e: Place vehicle on track and adjust zoom */
    if (g_game_mode == 4) {
        if (*(int16_t*)(*(int32_t*)(self + 0x744) + 0x48) == 1) {
            CGWND_SetBuildMode(1);
            int16_t zoom = 2;
            CGWND_TrackPiece_SetZoom(*(void**)(self + 0x744), zoom);
        } else {
            CGWND_SetBuildMode(0);
            int16_t zoom = 1;
            CGWND_TrackPiece_SetZoom(*(void**)(self + 0x744), zoom);
        }
        Game_UpdateCursorMode((int32_t*)g_game);
        return 1;
    }

    return 0;
}

/* ================================================================== */
/* RESDATA_GameVehicle_Ctor — Constructor                               */
/* Address: 0x44AE80                                                   */
/* ================================================================== */
void* __thiscall RESDATA_GameVehicle_Ctor(void* this, int32_t param_1)
{
    uint8_t* self = (uint8_t*)this;
    int32_t iVar1;

    /* Call base constructor */
    RESDATA_GameObject_Ctor(this, param_1);

    int32_t resData = *(int32_t*)(self + 0x40);

    /* Set vtable, type, and defaults */
    *(void***)this = (void*)VTBL_RESDATA_GAMEVEHICLE;
    *(int32_t*)(self + 0x04) = 4;          /* type = 4 */
    *(int32_t*)(self + 0x10C) = 0;         /* vehicle_kind = 0 (unset) */
    *(int32_t*)(self + 0x110) = 3;         /* init_state = 3 */

    /* Determine vehicle_kind based on tile type byte at RESDATA+0x63a */
    uint8_t tileType = *(uint8_t*)(resData + 0x63A);

    if (tileType == 0x0C) {
        /* Track tile — this is a train */
        *(int32_t*)(self + 0x10C) = 1;     /* vehicle_kind = TRAIN */

        int32_t defaultAnim = (int32_t)*(int16_t*)(resData + 0x1E);  /* default animation index */

        /* Check for subtype 0x0B inside 0x0C branch (code path documented) */
        if (*(uint8_t*)(resData + 0x63A) == 0x0B) {
            /* Walker subtype within track tile */
            if (defaultAnim == 0) {
                *(int32_t*)(self + 0x110) = 5;
                GameObject_StopSound(this, 0);
            } else {
                if (defaultAnim == 1) {
                    *(int32_t*)(self + 0x110) = 4;
                }
                GameObject_StopSound(this, defaultAnim);
            }
        } else {
            /* Normal train — init_state = default animation index */
            *(int32_t*)(self + 0x110) = defaultAnim;
            GameObject_StopSound(this, defaultAnim);
        }
    } else if (tileType == 0x0B) {
        /* Pedestrian tile */
        *(int32_t*)(self + 0x10C) = 2;     /* vehicle_kind = PEDESTRIAN */
        *(int32_t*)(self + 0x110) = 5;     /* init_state = 5 */
    } else if (RESDATA_IsRoadTile(resData)) {
        /* Road tile — road vehicle */
        *(int32_t*)(self + 0x10C) = 3;     /* vehicle_kind = ROAD_VEHICLE */
    } else if (tileType == 0x0D) {
        /* Crossing signal */
        *(int32_t*)(self + 0x10C) = 6;     /* vehicle_kind = CROSSING */
    } else if (tileType == 0x05 || tileType == 0x06) {
        /* Fuel pump */
        *(int32_t*)(self + 0x10C) = 5;     /* vehicle_kind = FUEL */
    } else if (tileType == 0x0E || tileType == 0x0F ||
               tileType == 0x10 || tileType == 0x11) {
        /* Water/Track signal tile */
        *(int32_t*)(self + 0x110) = 4;
        *(int32_t*)(self + 0x10C) = 7;     /* vehicle_kind = SIGNAL */
        *(int32_t*)(self + 0x110) = 4;     /* redundant in original */
        GameObject_StopSound(this, 1);
    } else {
        /* Check resource ID for special kinds */
        int32_t resId = (resData == 0) ? -1 : *(int32_t*)(resData + 4);
        if (resId == 0xC68 || resId == 0xC66 || resId == 0xC6A || resId == 0xC64) {
            *(int32_t*)(self + 0x10C) = 8;  /* vehicle_kind = SPECIAL */
        }
    }

    /* Finalize initialization */
    *(int32_t*)(self + 0x118) = 0;          /* field_118 = 0 */
    *(int16_t*)(self + 0x114) = 0;          /* field_114 = 0 */

    return this;
}

/* ================================================================== */
/* RESDATA_GameVehicle_Dtor — Scalar deleting destructor                */
/* Address: 0x44B030                                                   */
/* ================================================================== */
void* __thiscall RESDATA_GameVehicle_Dtor(void* this, uint8_t param_1)
{
    RESDATA_GameVehicle_BaseDtor(this);

    if ((param_1 & 1) != 0) {
        GLOBAL_free(this);
    }

    return this;
}

/* ================================================================== */
/* RESDATA_GameVehicle_BaseDtor — Base destructor                       */
/* Address: 0x44B050                                                   */
/* ================================================================== */
void __fastcall RESDATA_GameVehicle_BaseDtor(void* param_1)
{
    /* Reset vtable */
    *(void**)param_1 = (void*)VTBL_RESDATA_GAMEVEHICLE;

    /* Remove from world grid tracking */
    World_DeserializeMap(&g_world, (int32_t)param_1);
    /* Note: &g_world above refers to the global World at 0x4A98B0 */

    /* Destroy object's group in building manager */
    BuildingMgr_DestroyObjectGroup(param_1);
}

/* ================================================================== */
/* RESDATA_GameVehicle_InitSounds — Play vehicle sounds                 */
/* Address: 0x44B0B0                                                   */
/* ================================================================== */
uint8_t __fastcall RESDATA_GameVehicle_InitSounds(int32_t* param_1)
{
    int32_t resData = param_1[0x10];  /* +0x40 = RESDATA ptr */
    uint8_t tileType = *(uint8_t*)(resData + 0x63A);

    /* --- Track tile (0x0C): play horn --- */
    if (tileType == 0x0C) {
        int32_t subtype = param_1[0x44];  /* +0x110 = init_state/param */
        if (subtype == 0) {
            (**(void (__thiscall**)(int32_t*, int32_t))param_1)(param_1, 1);  /* vtable[7] horn 1 */
        } else if (subtype == 1) {
            (**(void (__thiscall**)(int32_t*, int32_t))param_1)(param_1, 2);  /* vtable[7] horn 2 */
        } else if (subtype == 2) {
            (**(void (__thiscall**)(int32_t*, int32_t))param_1)(param_1, 0);  /* vtable[7] horn 0 */
        }
        /* subtype >= 3: no sound */
    }

    /* --- Walker tile (0x0B): play footstep --- */
    resData = param_1[0x10];
    tileType = *(uint8_t*)(resData + 0x63A);

    if (tileType == 0x0B) {
        uint8_t timerResult = World_InitTimer(&g_world, (int32_t)param_1);
        if (timerResult == 0) {
            /* Timer not running — play footstep or idle sound */
            if (param_1[0x44] == 4) {
                (**(void (__thiscall**)(int32_t*, int32_t))param_1)(param_1, 0);  /* footstep */
            } else {
                int32_t footstepIdx = param_1[0x44] - 5;
                if (footstepIdx == 0) {
                    (**(void (__thiscall**)(int32_t*, int32_t))param_1)(param_1, 1);  /* footstep 1 */
                    return 1;
                }
            }
        }
    }

    return 1;
}
