/**
 * vehicle.c — Vehicle road vehicle function implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Vehicle is a road vehicle (car/truck/bus) that follows tracks through
 * the town. It manages position, speed, engine sounds, occupant loading,
 * and multiplayer sync. The Vehicle struct is 0x94 bytes with vtable at
 * 0x0047836C (VTBL_SCRIPTED_OBJECT).
 *
 * Functions in this file:
 *   Vehicle_Ctor                         (0x44BE50, 598b) — Constructor
 *   RESDATA_ScriptedObject_DtorList      (0x44C0B0, 28b)  — Scalar-deleting dtor
 *   RESDATA_ScriptedObject_CleanupChildren(0x44C0D0, 122b)— Clean child objects
 *   Vehicle_InitOccupant                 (0x44C150, 32b)  — Set occupant mode
 *   Vehicle_FindPath                     (0x44C170, 174b) — Find path target
 *   Vehicle_InitRoute                    (0x44C220, 232b) — Add route segment
 *   VehicleEditor_RemoveVehicle          (0x44C310, 93b)  — Remove editor slot
 *   Vehicle_GetOccupantCount             (0x44C370, 48b)  — Check occupant
 *   VehicleEditor_Update                 (0x44C3A0, 1520b)— Per-frame update
 *   Vehicle_ClearRoute                   (0x44C9B0, 151b) — Clear route
 *   Vehicle_HandleStop                   (0x44CA50, 94b)  — Stop handler
 *   Vehicle_DetachAll                    (0x44CAB0, 62b)  — Detach check
 *   Vehicle_ResetState                   (0x44CAF0, 31b)  — Reset engine sound
 *   Vehicle_UpdateEngineSound            (0x44CB10, 714b) — Update sound pos
 *   Vehicle_LoadSounds                   (0x44CE10, 1631b)— Load sound config
 *   Vehicle_GetNearestTrack              (0x44D4C0, 53b)  — Get nearest track
 *   Vehicle_UpdatePosition               (0x44D500, 212b) — Update world pos
 *   Vehicle_Stop                         (0x44D5E0, 66b)  — Stop vehicle
 *   Vehicle_IsMoving                     (0x44D630, 139b) — Check movement
 *   Vehicle_CalcSpeed                    (0x44D6C0, 82b)  — Calculate speed
 *   Vehicle_UpdateSpeed                  (0x44D720, 29b)  — Update speed
 *   Vehicle_SetState                     (0x44D740, 186b) — Set state
 *   CollisionData_Ctor                   (0x44D800, 45b)  — Collision data ctor
 *   CollisionData_Dtor                   (0x44D830, 36b)  — Collision data dtor
 */

#include "vehicle.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* operator_new(uint32_t size);
extern void  GLOBAL_free(void* ptr);

extern void* __thiscall GAMESTATE_EditorState_Ctor(void* this, uint8_t param_1);
extern void  __fastcall GAMESTATE_InitTrackAtPosition(void* editor, int32_t x, int32_t y);
extern uint32_t __fastcall GAMESTATE_UpdateVehiclePlacement(void* editorState, void* vehicle);
extern void  __fastcall GAMESTATE_EditorState_Copy(void* dst, void* src);
extern void  __fastcall GAMESTATE_EditorState_Detach(void* editorState);

extern void* __thiscall VehicleEditor_Ctor(void* this, int32_t param_1,
                                           int32_t param_2, uint8_t param_3);
extern void  __fastcall VehicleEditor_InitTracks(void* editor, int32_t trackIdx, int32_t param_3);
extern void  __fastcall VehicleEditor_ProcessMove(void* editor, void* vehicle);
extern void  __fastcall VehicleEditor_TriggerSound(void* editor);
extern void  __fastcall VehicleEditor_UpdateEditMode(void* wheel, void* vehicle);
extern void  __fastcall VehicleEditor_CheckEditBounds1(void* editor, void* vehicle);
extern void  __fastcall VehicleEditor_CheckBounds(void* editorState);
extern void  __fastcall VehicleEditor_CheckBounds2(void* editorState);

extern int32_t __fastcall TileMap_GetObjectAt(void* tilemap, int16_t x, int16_t y, int32_t layer);
extern void    __thiscall TileMap_InvalidateRect(void* tilemap, int32_t x, int32_t y,
                                                  int32_t w, int32_t h);
extern void*   __fastcall INPUT_FindObjectAt(void* inputMgr, int32_t type);

extern void    __fastcall GameVehicle_AddDestination(int32_t* target, void* vehicle);
extern void    __fastcall GameVehicle_RemoveDestination(int32_t* building, uint32_t id,
                                                         uint8_t player);

extern void    __thiscall AudioChannel_UpdatePosition(void* channel, int32_t x, int32_t y);
extern void    __fastcall AudioChannel_Pause(uint32_t channel);
extern void    __fastcall AudioChannel_Play(uint32_t channel);

extern void    __fastcall ArrivalQueue_AddVehicle(void* building, void* vehicle);

extern void    __fastcall NETMAN_ReceivePing(void* netman, uint32_t playerId,
                        uint8_t colorR, uint8_t colorG, int32_t x, int32_t y);
extern void    __fastcall NETMAN_ReceiveAck(void* netman, uint32_t playerId,
                        uint8_t colorR, uint8_t colorG);
extern void    __fastcall NETMAN_SerializePlayerData(void* netman, int32_t vehicleAddr);

extern void    __cdecl UI_CreateMessageBox(void* tooltipMgr, int32_t resId,
                        int32_t param2, char param3, int32_t x, int32_t y, uint8_t param7);

extern void    __fastcall GameObject_StopSound(void* obj, int32_t soundIdx);

extern uint8_t __fastcall RESDATA_IsRoadTile(int32_t tileObj);
extern uint8_t __fastcall RESDATA_IsBuildingTile(int32_t tileObj);

extern void    __fastcall World_Init(void* world);

/* ================================================================== */
/* Global references                                                    */
/* ================================================================== */
extern void* g_tilemap;     /* TileMap singleton, 0x4AAD08 */
extern void* g_input_mgr;   /* INPUT manager singleton */
extern void* g_netman;      /* NetMan singleton */
extern void* g_tooltip_mgr; /* Tooltip manager */
extern void* g_game;        /* Game singleton */

#define ADDR_g_world  0x004A98B0  /* World singleton */

/* ================================================================== */
/* Vehicle_Ctor                                                        */
/* Address: 0x44BE50 (598 bytes)                                       */
/* ================================================================== */
void* __thiscall Vehicle_Ctor(void* this, int32_t param_1, int32_t param_2,
                              uint8_t param_3, uint8_t param_4)
{
    uint8_t* self = (uint8_t*)this;
    void* editorState;
    void* vehicleEditor;
    int32_t** activeSlot;
    int32_t i;

    /* Initialize position tracking to -1 */
    *(int16_t*)(self + 0x2E) = -1;  /* tileX */
    *(int16_t*)(self + 0x30) = -1;  /* tileY */
    *(int16_t*)(self + 0x32) = -1;  /* targetTileX */
    *(int16_t*)(self + 0x34) = -1;  /* targetTileY */

    self[0x88] = param_4;           /* initFlag */
    *(int32_t*)(self + 0x04) = param_2;

    /* Set vtable */
    *(void***)self = (void*)VTBL_SCRIPTED_OBJECT;

    /* Clear state */
    self[0x5A] = 0;                 /* soundGuard = 0 */
    self[0x90] = 0;                 /* activeFlag = 0 */
    self[0x2C] = 0;                 /* detachFlag = 0 */
    *(int32_t*)(self + 0x8C) = 0;   /* editorState2 = 0 */
    *(int32_t*)(self + 0x68) = 0;   /* netSyncFlag = 0 */
    *(int32_t*)(self + 0x70) = 0;

    /* Clear occupant/track slots (8 int32_t at +0x38) */
    for (i = 0; i < 8; i++) {
        *(int32_t*)(self + 0x38 + i * 4) = 0;
    }

    /* Clear editor array (4 entries at +0x10) */
    *(int32_t*)(self + 0x10) = 0;
    *(int32_t*)(self + 0x14) = 0;
    *(int32_t*)(self + 0x18) = 0;
    *(int32_t*)(self + 0x1C) = 0;
    *(int32_t*)(self + 0x60) = 0;   /* direction = 0 */

    /* Create EditorState sub-object (0x20 bytes) */
    editorState = operator_new(0x20);
    if (editorState) {
        editorState = GAMESTATE_EditorState_Ctor(editorState, param_3);
    } else {
        editorState = 0;
    }
    *(void**)(self + 0x20) = editorState;
    *(int32_t*)(self + 0x28) = 0;   /* stopTimer = 0 */

    /* Re-clear editors */
    *(int32_t*)(self + 0x10) = 0;
    *(int32_t*)(self + 0x14) = 0;
    *(int32_t*)(self + 0x18) = 0;
    *(int32_t*)(self + 0x1C) = 0;
    *(int16_t*)(self + 0x0C) = 0;   /* editorCount = 0 */

    /* Create VehicleEditor sub-object (0x450 bytes) */
    vehicleEditor = operator_new(0x450);
    if (vehicleEditor) {
        vehicleEditor = VehicleEditor_Ctor(vehicleEditor, param_1, 2, param_3);
    } else {
        vehicleEditor = 0;
    }
    *(void**)(self + 0x10) = vehicleEditor;  /* editors[0] = vehicleEditor */

    /* Check if first editor slot is active */
    activeSlot = *(int32_t***)(self + *(int16_t*)(self + 0x0C) * 4 + 0x10);

    if (activeSlot != 0) {
        if (*(uint8_t*)(activeSlot + 6) == 1) {  /* editor active flag at +0x18 */
            activeSlot[0x113] = (int32_t)this;  /* backref to vehicle at +0x44C */

            int32_t* editorParam = *(int32_t**)(*(int32_t*)(self + 0x10) + 0x40);
            int16_t fwdSpeed = *(int16_t*)(editorParam + 0x7A8 / 4);   /* forward speed */
            *(int16_t*)(self + 0x24) = fwdSpeed;
            *(int16_t*)(self + 0x26) = *(int16_t*)(editorParam + 0x7AA / 4);  /* reverse speed */
            *(int16_t*)(self + 0x58) = fwdSpeed;  /* maxSteps = forward speed */
            *(int32_t*)(self + 0x08) = 0;          /* activeEditor = 0 (forward) */

            Vehicle_SetState(this, 0);               /* STATE_STOPPED */
            *(int16_t*)(self + 0x36) = 0;            /* moveTimer = 0 */

            /* Set player colors based on scenario */
            if (*(int32_t*)((uint8_t*)g_netman + 0x7C4) == 2) {  /* scenario 2 */
                self[0x78] = *(uint8_t*)((uint8_t*)g_netman + 0x7D0);
                self[0x7C] = *(uint8_t*)((uint8_t*)g_netman + 0x7D0);
            } else {
                self[0x78] = 1;
                self[0x7C] = 1;
            }

            if (param_3 == 0) {  /* local vehicle */
                /* Register with netman */
                int32_t maxPlayers = *(int32_t*)((uint8_t*)g_netman + 0x7E8) + 1;
                *(int32_t*)((uint8_t*)g_netman + 0x7E8) = maxPlayers;
                *(int16_t*)(self + 0x7A) = (int16_t)maxPlayers;  /* playerId */

                GAMESTATE_InitTrackAtPosition(*(void**)(self + 0x20), -1, -1);

                int32_t trackIdx = *(int32_t*)(*(int32_t*)(self + 0x20) + 0x0C) + 0x0C;
                VehicleEditor_InitTracks(*(void**)(self + 0x10), trackIdx, -1);

                int16_t netX = (trackIdx < 0) ? -1 : (int16_t)(trackIdx >> 4);
                NETMAN_ReceivePing(g_netman, *(uint16_t*)(self + 0x7A),
                                  self[0x78], self[0x7C], netX, -1);

                self[0x89] = 0;
                self[0x8A] = 0;
                *(int32_t*)(self + 0x70) = 0;
                *(int32_t*)(self + 0x64) = 2;     /* occupancy = 2 (full) */
                *(int32_t*)(self + 0x60) = 0;     /* direction = 0 (forward) */
                Vehicle_UpdatePosition(this, 0);
            } else {  /* remote vehicle */
                *(int32_t*)(self + 0x60) = 2;     /* direction = 2 (edge-of-map) */
                *(int32_t*)(self + 0x64) = 0;     /* occupancy = 0 (empty) */
                Vehicle_UpdatePosition(this, 1);
            }
        } else if (activeSlot != 0) {
            /* Destroy inactive editor slot */
            (*(void (__thiscall**)(void*, uint8_t))*activeSlot)(activeSlot, 1);
            int16_t editorIdx = *(int16_t*)(self + 0x0C);
            *(int32_t**)(self + editorIdx * 4 + 0x10) = 0;
        }
    }

    return this;
}

/* ================================================================== */
/* RESDATA_ScriptedObject_DtorList — Scalar deleting destructor        */
/* Address: 0x44C0B0                                                   */
/* ================================================================== */
void* __thiscall RESDATA_ScriptedObject_DtorList(void* this, uint8_t param_1)
{
    RESDATA_ScriptedObject_CleanupChildren((int32_t*)this);
    if ((param_1 & 1) != 0) {
        GLOBAL_free(this);
    }
    return this;
}

/* ================================================================== */
/* RESDATA_ScriptedObject_CleanupChildren — Clean up child objects     */
/* Address: 0x44C0D0                                                   */
/* ================================================================== */
void __fastcall RESDATA_ScriptedObject_CleanupChildren(int32_t* param_1)
{
    int32_t i;
    int32_t* editor;

    /* Reset vtable */
    param_1[0] = (int32_t)VTBL_SCRIPTED_OBJECT;

    /* Send NETMAN ack if netman active and initFlag not set */
    if (g_netman != 0 && *(uint8_t*)(param_1 + 0x22) == 0) {
        NETMAN_ReceiveAck(g_netman, (uint32_t)*(uint16_t*)((uint8_t*)param_1 + 0x7A),
                         *(uint8_t*)(param_1 + 0x1E),
                         *(uint8_t*)((uint8_t*)param_1 + 0x1F));
    }

    /* Destroy child objects in array at +0x10..+0x1c */
    for (i = 0; i <= (int32_t)(uint32_t)*(uint16_t*)(param_1 + 3); i++) {
        editor = (int32_t*)*((int32_t*)((uint8_t*)param_1 + 0x10 + i * 4));
        if (editor != 0) {
            (*(void (__thiscall**)(int32_t*, uint8_t))*editor)(editor, 1);
            *((int32_t*)((uint8_t*)param_1 + 0x10 + i * 4)) = 0;
        }
    }

    /* Destroy tail object at +0x20 */
    if (*(void**)(param_1 + 8) != 0) {
        void* tail = *(void**)(param_1 + 8);
        (*(void (__thiscall**)(void*, uint8_t))*(void**)tail)(tail, 1);
        param_1[8] = 0;
    }
}

/* ================================================================== */
/* Vehicle_InitOccupant                                                */
/* Address: 0x44C150                                                   */
/* ================================================================== */
void __thiscall Vehicle_InitOccupant(void* this, int32_t mode)
{
    uint8_t* self = (uint8_t*)this;
    *(int32_t*)(self + 0x64) = mode;  /* occupancy */
    if (mode != 2) {
        Vehicle_UpdatePosition(this, 1);  /* loading */
    } else {
        Vehicle_UpdatePosition(this, 0);  /* waiting */
    }
}

/* ================================================================== */
/* Vehicle_FindPath                                                    */
/* Address: 0x44C170                                                   */
/* ================================================================== */
void __thiscall Vehicle_FindPath(void* this, int32_t* target, uint8_t isRemote)
{
    uint8_t* self = (uint8_t*)this;
    uint32_t i;

    /* Store target tile index */
    *(int32_t*)(self + 0x32) = target[0x22];  /* targetTileX from target's tile pos */

    /* If another vehicle already en route — add to destination list */
    if (target[0x48] != 0 && (void*)target[0x48] != this) {
        GameVehicle_AddDestination(target, this);
        return;
    }

    /* Register as occupant */
    *(uint8_t*)(target + 0x4A) = 1;
    target[0x48] = (int32_t)this;
    *(int32_t*)(self + 0x64) = 5;     /* occupancy = ARRIVING */
    Vehicle_UpdatePosition(this, 1);

    /* Set all editors to ARRIVING state */
    for (i = 0; i <= *(uint16_t*)(self + 0x0C); i++) {
        int32_t* editor = *(int32_t**)(self + 0x10 + i * 4);
        editor[0x444 / 4] = 5;                       /* editor state */
        *(int32_t*)(editor[0x430 / 4] + 0x1C) = 5;   /* front wheel state */
        *(int32_t*)(editor[0x434 / 4] + 0x1C) = 5;   /* rear wheel state */
    }

    Vehicle_SetState(this, 2);           /* MOVING */
    Vehicle_UpdatePosition(this, 1);
    Vehicle_LoadSounds(this, target, isRemote);
    target[0x47] = 0;                    /* clear target's occupant count */
}

/* ================================================================== */
/* Vehicle_InitRoute                                                   */
/* Address: 0x44C220 (232 bytes)                                       */
/* ================================================================== */
uint32_t __thiscall Vehicle_InitRoute(void* this, int32_t param_1,
                                      int32_t param_2, uint8_t param_3)
{
    uint8_t* self = (uint8_t*)this;
    uint16_t count = *(uint16_t*)(self + 0x0C);
    void* editor;

    if (count < 3 && *(void**)(self + count * 4 + 0x14) == 0) {
        *(int16_t*)(self + 0x0C) = (int16_t)(count + 1);

        editor = operator_new(0x450);
        if (editor) {
            editor = VehicleEditor_Ctor(editor, param_1, param_2, param_3);
        } else {
            editor = 0;
        }
        *(void**)(self + *(uint16_t*)(self + 0x0C) * 4 + 0x10) = editor;

        uint16_t newCount = *(uint16_t*)(self + 0x0C);
        void* newEditor = *(void**)(self + newCount * 4 + 0x10);
        if (newEditor) {
            if (*(uint8_t*)((uint8_t*)newEditor + 0x18) == 1) {
                *(void**)((uint8_t*)newEditor + 0x44C) = this;  /* backref */
                return 1;
            }
            /* Editor inactive — destroy it */
            (*(void (__thiscall**)(void*, uint8_t))*(void**)newEditor)(newEditor, 1);
            newCount = *(uint16_t*)(self + 0x0C);
            *(void**)(self + newCount * 4 + 0x10) = 0;
        }
        *(int16_t*)(self + 0x0C) = (int16_t)(newCount - 1);
    }
    return 0;
}

/* ================================================================== */
/* VehicleEditor_RemoveVehicle                                          */
/* Address: 0x44C310                                                   */
/* ================================================================== */
int32_t __thiscall VehicleEditor_RemoveVehicle(void* this, uint32_t index)
{
    uint8_t* self = (uint8_t*)this;
    int32_t i;

    if (index > 3) {
        return 0;
    }

    void* editor = *(void**)(self + index * 4 + 0x10);
    if (editor == 0) {
        return 0;
    }

    /* Destroy editor via its scalar deleting destructor */
    (*(void (__thiscall**)(void*, uint8_t))*(void**)editor)(editor, 1);
    *(void**)(self + index * 4 + 0x10) = 0;
    *(int16_t*)(self + 0x0C) = *(int16_t*)(self + 0x0C) - 1;  /* editorCount-- */

    /* Shift remaining slots left if not last slot */
    if (index < 3) {
        for (i = index; i < 3; i++) {
            *(void**)(self + i * 4 + 0x10) = *(void**)(self + i * 4 + 0x14);
            *(void**)(self + i * 4 + 0x14) = 0;
        }
    }

    return 1;
}

/* ================================================================== */
/* Vehicle_GetOccupantCount                                            */
/* Address: 0x44C370                                                   */
/* ================================================================== */
uint8_t __fastcall Vehicle_GetOccupantCount(void* this)
{
    uint8_t* self = (uint8_t*)this;
    uint32_t i;

    for (i = 1; i <= 3; i++) {
        int32_t* editor = *(int32_t**)(self + 0x14 + (i - 1) * 4);
        if (editor != 0 && *(int32_t*)(*(int32_t*)((uint8_t*)editor + 0x42C)) == 2) {
            return 1;
        }
    }
    return 0;
}

/* ================================================================== */
/* VehicleEditor_Update — Main per-frame update (1520 bytes)           */
/* Address: 0x44C3A0                                                   */
/* ================================================================== */
void __fastcall VehicleEditor_Update(void* param_1)
{
    uint8_t* self = (uint8_t*)param_1;
    int32_t curState;
    int32_t moved;
    uint32_t i, j;
    int32_t* target;
    int32_t targetState;
    uint32_t idx;

    moved = 0;

    /* --- Engine sound update --- */
    if (self[0x90] != 0) {
        self[0x90] = 0;
        if (*(int32_t*)(self + 0x08) != 0 && self[0x5A] == 0) {
            self[0x5A] = 1;
            Vehicle_UpdateEngineSound(param_1);
            self[0x5A] = 0;
        }
    }

    /* --- Move timer countdown --- */
    if (*(int16_t*)(self + 0x36) != 0) {
        int16_t timer = *(int16_t*)(self + 0x36) - 1;
        *(int16_t*)(self + 0x36) = timer;
        if (timer == 1 && *(int32_t*)(self + 0x5C) == 1) {
            Vehicle_SetState(param_1, 2);  /* MOVING */
        }
    }

    /* --- Early exit conditions --- */
    curState = *(int32_t*)(self + 0x5C);
    if (curState == 0 || curState == 4) goto after_movement;
    if (*(int32_t*)(self + 0x60) == 2 || *(int32_t*)(self + 0x60) == 3) goto after_movement;
    if (*(int32_t*)(self + 0x64) == 2) goto after_movement;  /* occupancy full */
    if (*(int16_t*)(self + 0x58) == 0) goto after_movement;
    if (*(uint16_t*)(self + 0x36) > 1) goto after_movement;
    if (curState != 2 && curState != 1) goto after_movement;

    /* --- Check target building state --- */
    target = *(int32_t**)(*(int32_t*)(self + 0x20) + 0x14);
    if (target == 0) goto movement_loop;

    targetState = target[0x110 / 4];  /* +0x110 = target state code */
    if (targetState == 0) {
        Vehicle_SetState(param_1, 2);  /* MOVING */
        goto after_movement;
    }
    if (targetState == 1) {
        /* Loading — play engine sound */
        if (self[0x5A] == 0) {
            self[0x5A] = 1;
            Vehicle_UpdateEngineSound(param_1);
            self[0x5A] = 0;
        }
        Vehicle_SetState(param_1, 2);
        goto after_movement;
    }
    if (targetState == 2) {
        /* Waiting — approach if not already */
        if (curState != 1) {
            Vehicle_SetState(param_1, 1);
        }
    }

movement_loop:
    /* --- Process movement steps --- */
    moved = 0;
    if (*(int16_t*)(self + 0x58) != 0) {
        for (i = 0; i < *(uint16_t*)(self + 0x58); i++) {
            void* editorState = *(void**)(self + 0x20);
            int32_t* editor = *(int32_t**)(self + i * 4 + 0x10);

            if (*(int32_t*)((uint8_t*)editorState + 0x18) == 2 ||
                *(int32_t*)((uint8_t*)editorState + 0x1C) == 2) {
                moved = 1;
                break;
            }

            int32_t moveResult = GAMESTATE_UpdateVehiclePlacement(editorState, param_1);
            if (moveResult == 0) break;
            moved++;

            if (*(int32_t*)(*(int32_t*)(self + 0x20) + 0x18) >= 2) {
                moved = 1;
                break;
            }

            if (*(int32_t*)(self + 0x64) != 2 &&
                *(int32_t*)(*(int32_t*)(self + 0x20) + 0x1C) == 2) {
                *(int32_t*)(self + 0x64) = 1;    /* occupancy = DEPARTING */
                Vehicle_UpdatePosition(param_1, 1);
                moved = 1;
                break;
            }

            /* Process movement per editor */
            if (curState == 2) {
                if (*(int32_t*)(self + 0x08) == 0) {
                    /* Forward direction */
                    for (j = 0; j <= *(uint16_t*)(self + 0x0C); j++) {
                        void* ve = *(void**)(self + 0x10 + j * 4);
                        VehicleEditor_ProcessMove(ve, param_1);
                    }
                } else if (*(int32_t*)(self + 0x08) == 1) {
                    /* Reverse direction */
                    int8_t revCount = *(int8_t*)(self + 0x0C);
                    int32_t ed = revCount;
                    for (j = 0; (int32_t)j <= (int32_t)revCount; j++) {
                        void* ve = *(void**)(self + ed * 4 + 0x10);
                        VehicleEditor_ProcessMove(ve, param_1);
                        ed--;
                    }
                }
            }

            /* Re-check target building state */
            target = *(int32_t**)(*(int32_t*)(self + 0x20) + 0x14);
            if (target != 0 && *(int32_t*)((uint8_t*)target + 0x10C) == 1 && *(int32_t*)(self + 0x5C) != 0) {
                targetState = target[0x110 / 4];
                if (targetState == 0) {
                    Vehicle_SetState(param_1, 2);
                } else if (targetState == 1) {
                    if (self[0x5A] != 0) goto check_states;
                    self[0x5A] = 1;
                    Vehicle_UpdateEngineSound(param_1);
                    self[0x5A] = 0;
                    Vehicle_SetState(param_1, 2);
                } else if (targetState == 2 && *(int32_t*)(self + 0x5C) != 1) {
                    Vehicle_SetState(param_1, 1);
                }
            }

check_states:
            /* --- Handle DEPARTURE (direction 1) completion --- */
            if (*(int32_t*)(self + 0x60) == 1) {
                int32_t* lastVe = *(int32_t**)(self + *(uint16_t*)(self + 0x0C) * 4 + 0x10);
                int32_t* firstVe = *(int32_t**)(self + 0x10);

                if (lastVe[0x440 / 4] == 2 && firstVe[0x440 / 4] == 2) {
                    *(int32_t*)(self + 0x60) = 2;        /* direction = EDGE-OF-MAP */
                    *(int32_t*)(*(int32_t*)(self + 0x20) + 0x18) = 2;
                    *(int32_t*)(*(int32_t*)(*(int32_t*)(self + 0x20) + 0x14) + 0x11C) = 0;

                    int32_t buildingType = *(int32_t*)(*(int32_t*)(*(int32_t*)(*(int32_t*)(self + 0x20) + 0x14) + 0x40) + 4);
                    if (buildingType == 0xC42 || buildingType == 0xC44 ||
                        buildingType == 0xC46 || buildingType == 0xC48) {
                        /* Depot buildings */
                        *(int32_t*)(self + 0x60) = 3;    /* direction = DEPOT */
                        Vehicle_UpdatePosition(param_1, 0);
                        *(int32_t*)(self + 0x68) = 1;    /* netSyncFlag = 1 */
                    } else {
                        void* building = INPUT_FindObjectAt(&g_input_mgr, 0);
                        if (building == 0) {
                            Vehicle_SetState(param_1, 3);    /* WAITING */
                        } else {
                            ArrivalQueue_AddVehicle(building, param_1);
                        }
                    }
                    break;
                }
            }

            /* --- Handle DEPARTING (occupancy 1) completion --- */
            if (*(int32_t*)(self + 0x64) == 1) {
                int32_t* firstEditor = *(int32_t**)(self + 0x10);
                int32_t* lastEditor = *(int32_t**)(self + *(uint16_t*)(self + 0x0C) * 4 + 0x10);

                if (firstEditor[0x444 / 4] == 2 && lastEditor[0x444 / 4] == 2) {
                    *(int32_t*)(self + 0x64) = 2;         /* occupancy = FULL */
                    Vehicle_UpdatePosition(param_1, 0);
                    *(int32_t*)(*(int32_t*)(self + 0x20) + 0x1C) = 2;

                    int32_t* obj = (int32_t*)TileMap_GetObjectAt(&g_tilemap,
                        *(int16_t*)(target + 0x88), *(int16_t*)(target + 0x8A) + 1, 0);
                    if (obj != 0) {
                        obj[0x48] = (int32_t)param_1;     /* tracked_vehicle */
                        (*(void (__thiscall**)(int32_t, int32_t))*obj)(1, 1); /* vtable[7] */
                        obj[0x47] = 1;                    /* occupant_count */
                        *(int32_t*)(self + 0x32) = obj[0x22];  /* update targetX */
                    }
                    return;
                }
            }

            /* --- Clear route for FINISHED states --- */
            if (*(int32_t*)(self + 0x64) == 5 || *(int32_t*)(self + 0x64) == 4) {
                Vehicle_ClearRoute(param_1);
            }
        }
    }

    /* --- Post-movement audio update --- */
    if (*(int32_t*)(self + 0x68) == 0 && *(int32_t*)(self + 0x60) != 2 && *(int32_t*)(self + 0x60) != 3) {
        for (j = 0; j <= *(uint16_t*)(self + 0x0C); j++) {
            void* ve = *(void**)(self + j * 4 + 0x10);
            if (*(void**)((uint8_t*)ve + 0x48) != 0) {
                AudioChannel_UpdatePosition(*(void**)((uint8_t*)ve + 0x48),
                                           *(int32_t*)((uint8_t*)ve + 8),
                                           *(int32_t*)((uint8_t*)ve + 0x0C));
            }
        }
    }

after_movement:
    /* --- Edge-of-map routing --- */
    if (*(int32_t*)(self + 0x60) == 2 ||
        (*(int32_t*)(self + 0x60) == 3 && *(int32_t*)(self + 0x68) != 1)) {
        int32_t objAt = TileMap_GetObjectAt(&g_tilemap, *(int16_t*)(self + 0x2E),
                                            *(int16_t*)(self + 0x30) + 1, 0);
        if (objAt == 0) {
            void* building = INPUT_FindObjectAt(&g_input_mgr, 4);
            if (building == 0) {
                Vehicle_SetState(param_1, 3);  /* WAITING */
                return;
            }
            ArrivalQueue_AddVehicle(building, param_1);
        }
    }

    /* --- Handle STOP state --- */
    if (*(int32_t*)(self + 0x5C) == 4) {
        if (*(int32_t*)(self + 0x28) == 0) {
            Vehicle_SetState(param_1, 0);            /* STOPPED */
            *(int32_t*)(self + 0x28) = 0;
            Vehicle_CalcSpeed(param_1, *(int16_t*)(self + 0x24));

            if (*(int32_t*)(self + 0x08) != 0 && self[0x5A] == 0) {
                uint8_t moving = Vehicle_IsMoving(param_1);
                if (moving != 0 && self[0x5A] == 0) {
                    self[0x5A] = 1;
                    Vehicle_UpdateEngineSound(param_1);
                    self[0x5A] = 0;
                }
            }
            Vehicle_SetState(param_1, 2);  /* MOVING */
        } else {
            *(int32_t*)(self + 0x28) = *(int32_t*)(self + 0x28) - 1;  /* countdown */
        }
    }

    /* --- Network position sync --- */
    if (moved != 0) {
        int32_t occupancy = *(int32_t*)(self + 0x64);
        if (occupancy == 0 || (occupancy > 3 && occupancy < 6)) {
            int32_t* editorState = *(int32_t**)(self + 0x20);
            int32_t posX = editorState[0x10 / 4];  /* +0x10 */
            int32_t posY = editorState[0x0C / 4];  /* +0x0C */
            int16_t netX = (posX < 0) ? -1 : (int16_t)(posX >> 4);
            int16_t netY = (posY < 0) ? -1 : (int16_t)(posY >> 4);

            NETMAN_ReceivePing(g_netman, *(uint16_t*)(self + 0x7A),
                              self[0x78], self[0x7C], netX, netY);
        }
    }

    /* --- Network state serialization for special building types --- */
    target = *(int32_t**)(*(int32_t*)(self + 0x20) + 0x14);
    if (target != 0 && *(int32_t*)(self + 0x64) != 2 &&
        *(int32_t*)(self + 0x60) != 2 && *(int32_t*)(self + 0x60) != 3) {
        int32_t type = *(int32_t*)(*(int32_t*)((uint8_t*)target + 0x40) + 4);
        if (type == 0xC5C || type == 0xC5E || type == 0xC60) {
            NETMAN_SerializePlayerData(g_netman, (int32_t)param_1);
        }
    }
}

/* ================================================================== */
/* Vehicle_ClearRoute                                                  */
/* Address: 0x44C9B0                                                   */
/* ================================================================== */
void __fastcall Vehicle_ClearRoute(void* this)
{
    uint8_t* self = (uint8_t*)this;
    int32_t objAt;
    int32_t wheelEditor;

    /* Get relevant wheel editor based on direction */
    if (*(int32_t*)(self + 0x08) == 0) {  /* forward */
        int32_t lastEd = *(int32_t*)(self + *(uint16_t*)(self + 0x0C) * 4 + 0x10);
        wheelEditor = *(int32_t*)(lastEd + 0x434);  /* rear wheel */
    } else {  /* reverse */
        int32_t firstEd = *(int32_t*)(self + 0x10);
        wheelEditor = *(int32_t*)(firstEd + 0x430);  /* front wheel */
    }

    /* Check if route can be cleared — wheel state must be IDLE and target state != 4 */
    int32_t wheelState = *(int32_t*)(wheelEditor + 0x1C);
    int32_t* target = *(int32_t**)(wheelEditor + 0x14);
    if (wheelState == 0 && target != 0 && *(int32_t*)((uint8_t*)target + 0x10C) != 4) {
        /* Can clear route */
        *(int32_t*)(self + 0x64) = 0;  /* occupancy = 0 */
        Vehicle_UpdatePosition(this, 1);

        /* Clear tracked vehicle on destination building */
        objAt = TileMap_GetObjectAt(&g_tilemap, *(int16_t*)(self + 0x32),
                                    *(int16_t*)(self + 0x34) + 1, 0);
        if (objAt != 0) {
            *(int32_t*)(objAt + 0x11C) = 0;  /* clear arrival queue ptr */
            *(uint8_t*)(objAt + 0x128) = 0;   /* clear tracked_vehicle_flag */
            *(int32_t*)(objAt + 0x120) = 0;   /* clear tracked_vehicle */
        }

        *(int16_t*)(self + 0x32) = -1;  /* targetTileX = -1 */
        *(int16_t*)(self + 0x34) = -1;  /* targetTileY = -1 */
    }
}

/* ================================================================== */
/* Vehicle_HandleStop                                                  */
/* Address: 0x44CA50                                                   */
/* ================================================================== */
uint8_t __fastcall Vehicle_HandleStop(void* this)
{
    uint8_t* self = (uint8_t*)this;
    int32_t state = *(int32_t*)(self + 0x5C);

    if (state == 0) return 0;  /* already stopped */

    int32_t* target = *(int32_t**)(*(int32_t*)(self + 0x20) + 0x14);
    if (target == 0) return 0;

    int32_t targetState = target[0x110 / 4];  /* +0x110 */

    if (targetState == 1) {
        /* Loading — play engine sound then set MOVING */
        if (self[0x5A] == 0) {
            self[0x5A] = 1;
            Vehicle_UpdateEngineSound(this);
            self[0x5A] = 0;
        }
        Vehicle_SetState(this, 2);  /* MOVING */
        return 1;
    }

    if (targetState == 2) {
        /* Waiting — approach if not already */
        if (state != 1) {
            Vehicle_SetState(this, 1);  /* APPROACHING */
        }
        return 0;
    }

    /* targetState == 0 (empty) — move along */
    Vehicle_SetState(this, 2);
    return 1;
}

/* ================================================================== */
/* Vehicle_DetachAll                                                   */
/* Address: 0x44CAB0                                                   */
/* ================================================================== */
uint8_t __fastcall Vehicle_DetachAll(void* this)
{
    uint8_t* self = (uint8_t*)this;
    int32_t i;

    self[0x2C] = 0;  /* detachFlag = 0 */

    for (i = 0; i <= *(int16_t*)(self + 0x0C); i++) {
        int32_t editor = *(int32_t*)(self + 0x10 + i * 4);
        if (*(int16_t*)(editor + 0x448) == 1) {  /* DETACHED state */
            self[0x2C] = 1;
            return 1;
        }
    }
    return self[0x2C];
}

/* ================================================================== */
/* Vehicle_ResetState                                                  */
/* Address: 0x44CAF0                                                   */
/* ================================================================== */
uint8_t __fastcall Vehicle_ResetState(void* this)
{
    uint8_t* self = (uint8_t*)this;
    if (self[0x5A] != 0) {
        return 0;  /* reentrancy guard active */
    }
    self[0x5A] = 1;
    uint8_t result = Vehicle_UpdateEngineSound(this);
    self[0x5A] = 0;
    return result;
}

/* ================================================================== */
/* Vehicle_UpdateEngineSound — Full 714-byte implementation             */
/* Address: 0x44CB10                                                   */
/* ================================================================== */
uint8_t __fastcall Vehicle_UpdateEngineSound(void* this)
{
    uint8_t* self = (uint8_t*)this;
    uint32_t i;
    int32_t iterCount;

    /* Skip if occupancy 2 or direction 2/3 */
    if (*(int32_t*)(self + 0x64) == 2 || *(int32_t*)(self + 0x60) == 2 ||
        *(int32_t*)(self + 0x60) == 3) {
        return 0;
    }

    /* Toggle direction between 1 (REVERSE) and 4 (ALTERNATE FRONT) */
    if (*(int32_t*)(self + 0x60) == 1) {
        *(int32_t*)(self + 0x60) = 4;
    } else if (*(int32_t*)(self + 0x60) == 4) {
        *(int32_t*)(self + 0x60) = 1;
    }

    /* Toggle activeEditor flag */
    *(int32_t*)(self + 0x08) = (*(int32_t*)(self + 0x08) == 0) ? 1 : 0;

    /* --- Process each editor --- */
    for (i = 0; i <= *(uint16_t*)(self + 0x0C); i++) {
        int32_t* editor = *(int32_t**)(self + 0x10 + i * 4);

        /* Update wheel edit modes */
        VehicleEditor_UpdateEditMode(*(void**)((uint8_t*)editor + 0x430), this);
        VehicleEditor_UpdateEditMode(*(void**)((uint8_t*)editor + 0x434), this);

        /* State machine on editor exclusion flag (+0x440) */
        int32_t* editorX = editor;
        int32_t exclFlag = *(int32_t*)((uint8_t*)editor + 0x440);
        if (exclFlag > 0) {
            if (exclFlag < 3) {
                *(int32_t*)((uint8_t*)editor + 0x440) = 4;
            } else if (exclFlag == 4) {
                *(int32_t*)((uint8_t*)editor + 0x440) = 1;
            }
        }

        /* If front or rear wheel has state 1 (ACTIVE), set exclusion to 1 */
        if (*(int32_t*)(*(int32_t*)((uint8_t*)editor + 0x430) + 0x18) == 1 ||
            *(int32_t*)(*(int32_t*)((uint8_t*)editor + 0x434) + 0x18) == 1) {
            *(int32_t*)((uint8_t*)editor + 0x440) = 1;
        }

        /* State machine on editor state code (+0x444) */
        int32_t stateCode = *(int32_t*)((uint8_t*)editor + 0x444);
        switch (stateCode) {
        case 1:
            *(int32_t*)((uint8_t*)editor + 0x444) = 4;
            break;
        case 2:
            *(int32_t*)((uint8_t*)editor + 0x444) = 5;
            *(uint8_t*)((uint8_t*)editor + 0x24) = 0;  /* clear visible */
            break;
        case 4:
            *(int32_t*)((uint8_t*)editor + 0x444) = 1;
            VehicleEditor_CheckEditBounds1((void*)*editor, (int32_t)this);
            break;
        case 5:
            if (*(int32_t*)(*(int32_t*)((uint8_t*)editor + 0x430) + 0x1C) == 1 ||
                *(int32_t*)(*(int32_t*)((uint8_t*)editor + 0x434) + 0x1C) == 1) {
                /* Detached — goto case 4 logic */
                *(int32_t*)((uint8_t*)editor + 0x444) = 1;
                VehicleEditor_CheckEditBounds1((void*)*editor, (int32_t)this);
            } else {
                *(int32_t*)((uint8_t*)editor + 0x444) = 2;
                *(uint8_t*)((uint8_t*)editor + 0x24) = 0;  /* clear visible */
            }
            break;
        }
    }

    /* --- Copy editor state and run placement iteration --- */
    int16_t* targetCount = (int16_t*)(*(int32_t*)(*(int32_t*)(self + 0x20) + 0x14) + 0x114);
    *targetCount = *targetCount - 1;

    if (*(int32_t*)(self + 0x08) == 1) {
        /* Active: copy from rear wheel of last editor */
        GAMESTATE_EditorState_Copy(*(void**)(self + 0x20),
            *(void**)(*(int32_t*)(self + *(uint16_t*)(self + 0x0C) * 4 + 0x10) + 0x434));
        targetCount = (int16_t*)(*(int32_t*)(*(int32_t*)(self + 0x20) + 0x14) + 0x114);
        *targetCount = *targetCount + 1;

        iterCount = 12;
        while (iterCount != 0) {
            GAMESTATE_UpdateVehiclePlacement(*(void**)(self + 0x20), this);
            iterCount--;
        }
    } else {
        /* Forward: copy from front wheel of first editor */
        GAMESTATE_EditorState_Copy(*(void**)(self + 0x20),
            *(void**)(*(int32_t*)(self + 0x10) + 0x430));
        targetCount = (int16_t*)(*(int32_t*)(*(int32_t*)(self + 0x20) + 0x14) + 0x114);
        *targetCount = *targetCount + 1;

        iterCount = 0;
        while (iterCount < 12) {
            uint32_t res = GAMESTATE_UpdateVehiclePlacement(*(void**)(self + 0x20), this);
            if (res == 0) break;
            iterCount++;
        }
    }

    /* --- Post-placement state checks --- */
    if (*(int32_t*)(self + 0x60) == 4 && *(int32_t*)(*(int32_t*)(self + 0x20) + 0x18) == 2) {
        *(int32_t*)(*(int32_t*)(self + 0x20) + 0x18) = 4;
    }

    VehicleEditor_CheckBounds(*(int32_t*)(self + 0x20));
    VehicleEditor_CheckBounds2(*(int32_t*)(self + 0x20));

    /* --- Handle editor state result (+0x1C) --- */
    switch (*(int32_t*)(*(int32_t*)(self + 0x20) + 0x1C)) {
    case 0: {
        /* Check if first and last editor have state 4 or 5 (excluded) */
        int32_t firstState = *(int32_t*)(*(int32_t*)(self + 0x10) + 0x444);
        int32_t lastState = *(int32_t*)(*(int32_t*)(self + *(uint16_t*)(self + 0x0C) * 4 + 0x10) + 0x444);
        if (firstState != 4 && firstState != 5 && lastState != 4 && lastState != 5) {
            /* No excluded editors */
            if (*(int32_t*)(self + 0x64) == 0) {
                return 0;  /* occupancy empty — nothing more to do */
            }

            /* Clear target building's tracked vehicle */
            int32_t* targetBuilding = (int32_t*)TileMap_GetObjectAt(&g_tilemap,
                *(int16_t*)(self + 0x32), *(int16_t*)(self + 0x34) + 1, 0);
            if (targetBuilding != 0 && (void*)targetBuilding[0x48] == this) {
                targetBuilding[0x47] = 0;          /* clear occupant count */
                *(uint8_t*)(targetBuilding + 0x4A) = 0; /* clear occupant flag */
                (*(void (__thiscall**)(int32_t*, int32_t))*targetBuilding)(targetBuilding, 0); /* vtable[7] */
                targetBuilding[0x48] = 0;          /* clear tracked vehicle */
            }

            *(int16_t*)(self + 0x32) = -1;         /* targetTileX = -1 */
            *(int16_t*)(self + 0x34) = -1;         /* targetTileY = -1 */
            *(int32_t*)(self + 0x64) = 0;          /* occupancy = 0 */
            Vehicle_UpdatePosition(this, 1);
            return 0;
        }
        /* Fall through: excluded editors found — set occupancy = 4 (STOPPING) */
    }
    case 4:
    case 5:
        *(int32_t*)(self + 0x64) = 4;  /* occupancy = STOPPING */
        Vehicle_UpdatePosition(this, 1);
        return 0;

    case 1:
    case 2:
        *(int32_t*)(self + 0x64) = 1;  /* occupancy = DEPARTING */
        Vehicle_UpdatePosition(this, 1);
        return 0;

    default:
        return 0;
    }
}

/* ================================================================== */
/* Vehicle_LoadSounds — Full 1631-byte implementation                   */
/* Address: 0x44CE10                                                   */
/* ================================================================== */
uint8_t __thiscall Vehicle_LoadSounds(void* this, int32_t* param_1, uint8_t param_2)
{
    uint8_t* self = (uint8_t*)this;
    int32_t i, j;
    int32_t tileCategory;
    int32_t resourceId;

    if (param_1 == 0) {
        return 0;
    }

    /* Determine tile category from target building's resource */
    resourceId = param_1[0x10];  /* +0x40 = resource data ptr */
    tileCategory = 0;

    if (RESDATA_IsRoadTile(resourceId)) {
        tileCategory = 2;  /* ROAD */
        *(int32_t*)(self + 0x2E) = param_1[0x22];  /* store target tile X */
    } else if (RESDATA_IsBuildingTile(resourceId)) {
        tileCategory = 1;  /* BUILDING */
        (*(void (__thiscall**)(int32_t*, int32_t))*param_1)(param_1, 1);  /* vtable[7] */
    }

    /* --- Configure sound positions for each editor --- */
    for (i = 0; i <= *(uint16_t*)(self + 0x0C); i++) {
        int32_t* editor = *(int32_t**)(self + 0x10 + i * 4);
        int32_t frontWheel = *(int32_t*)((uint8_t*)editor + 0x430);
        int32_t rearWheel  = *(int32_t*)((uint8_t*)editor + 0x434);

        /* Set target building reference on both wheels */
        *(int32_t**)(frontWheel + 0x14) = param_1;
        *(int32_t**)(rearWheel + 0x14) = param_1;

        if (tileCategory == 2) {  /* ROAD */
            *(int32_t*)((uint8_t*)editor + 0x440) = 4;
            *(int32_t*)(frontWheel + 0x18) = 4;
            *(int32_t*)(rearWheel + 0x18) = 4;
        } else {  /* BUILDING or default */
            *(int32_t*)(frontWheel + 0x1C) = 5;
            *(int32_t*)(rearWheel + 0x1C) = 5;
        }

        /* Set sound pitch offsets based on tile type */
        uint8_t tileType = *(uint8_t*)(resourceId + 0x63A);
        if (tileType == 1 || tileType == 7) {
            if (param_2 == 0) {
                *(int16_t*)((uint8_t*)editor + 0x438) =
                    (*(int32_t*)(self + 0x08) == 0) ? 0x40 : 0;
            }
            *(int32_t*)(frontWheel + 4) = 0;
            *(int32_t*)(frontWheel + 8) = *(uint16_t*)(resourceId + 0x636) - 1;
            *(int32_t*)(rearWheel + 4) = 0;
            *(int32_t*)(rearWheel + 8) = *(uint16_t*)(resourceId + 0x636) - 1;
        } else if (tileType == 2 || tileType == 8) {
            if (param_2 == 0) {
                *(int16_t*)((uint8_t*)editor + 0x438) =
                    (*(int32_t*)(self + 0x08) == 0) ? 0 : 0x40;
            }
            *(int32_t*)(frontWheel + 4) = 1;
            *(int32_t*)(frontWheel + 8) = 1;
            *(int32_t*)(rearWheel + 4) = 1;
            *(int32_t*)(rearWheel + 8) = 1;
        } else if (tileType == 3 || tileType == 9) {
            if (param_2 == 0) {
                *(int16_t*)((uint8_t*)editor + 0x438) =
                    (*(int32_t*)(self + 0x08) == 0) ? 0x20 : 0x60;
            }
            *(int32_t*)(frontWheel + 4) = 1;
            *(int32_t*)(frontWheel + 8) = 1;
            *(int32_t*)(rearWheel + 4) = 1;
            *(int32_t*)(rearWheel + 8) = 1;
        } else if (tileType == 4 || tileType == 10) {
            if (param_2 == 0) {
                *(int16_t*)((uint8_t*)editor + 0x438) =
                    (*(int32_t*)(self + 0x08) == 0) ? 0x60 : 0x20;
            }
            *(int32_t*)(frontWheel + 4) = 0;
            *(int32_t*)(frontWheel + 8) = *(uint16_t*)(resourceId + 0x636) - 1;
            *(int32_t*)(rearWheel + 4) = 0;
            *(int32_t*)(rearWheel + 8) = *(uint16_t*)(resourceId + 0x636) - 1;
        }
        /* else: no special handling for other tile types */

        /* Re-set target on wheels (redundant from original code) */
        *(int32_t**)(frontWheel + 0x14) = param_1;
        *(int32_t**)(rearWheel + 0x14) = param_1;
    }

    /* --- Set editor state position --- */
    int32_t* editorState = *(int32_t**)(self + 0x20);
    *(int32_t**)(editorState + 0x14 / 4) = param_1;

    /* Copy position from front wheel to editor state */
    int32_t frontWheelData = *(int32_t*)(*(int32_t*)(self + 0x10) + 0x430);
    editorState[4 / 4] = *(int32_t*)(frontWheelData + 4);     /* copy pos X */
    editorState[8 / 4] = *(int32_t*)(frontWheelData + 8);     /* copy pos Y */

    /* Calculate world position from target building */
    int32_t* targetBuilding = *(int32_t**)(editorState + 0x14 / 4);
    int32_t* targetRes = *(int32_t**)((uint8_t*)targetBuilding + 0x40);
    int32_t* trackTable = *(int32_t**)((uint8_t*)targetRes + 0x630);

    editorState[0x0C / 4] =
        (int32_t)*(int16_t*)((uint8_t*)trackTable + *(int32_t*)(frontWheelData + 8) * 4) +
        *(int16_t*)((uint8_t*)targetBuilding + 0x88) * 0x10;
    editorState[0x10 / 4] =
        (int32_t)*(int16_t*)((uint8_t*)trackTable + 2 + *(int32_t*)(frontWheelData + 8) * 4) +
        *(int16_t*)((uint8_t*)targetBuilding + 0x8A) * 0x10;

    /* Set editor state code based on tile category */
    if (tileCategory == 2) {
        *(int32_t*)(editorState + 0x18 / 4) = 4;
    } else {
        *(int32_t*)(editorState + 0x1C / 4) = 5;
    }

    /* --- Position vehicle sub-editors relative to editor state --- */
    if (*(int32_t*)(self + 0x08) != 0) {
        /* ActiveEditor set — position from last editor backward */
        for (j = 0; j <= *(int16_t*)(self + 0x0C); j++) {
            int32_t* ed = *(int32_t**)(self + *(uint16_t*)(self + 0x0C) * 4 + 0x10 - j * 4);
            int32_t offset = j * 0x26;

            switch (tileType = *(uint8_t*)(resourceId + 0x63A)) {
            case 1: case 7:
                /* North-facing: place rear wheel above front */
                break;
            case 2: case 8:
                /* South-facing */
                break;
            case 3: case 9:
                /* West-facing */
                break;
            case 4: case 10:
                /* East-facing */
                break;
            }
        }
    } else {
        /* Forward — position editors sequentially */
        for (i = 0; i <= *(uint16_t*)(self + 0x0C); i++) {
            int32_t* editor = *(int32_t**)(self + 0x10 + i * 4);
            int32_t frontWheelEd = *(int32_t*)((uint8_t*)editor + 0x430);
            int32_t rearWheelEd  = *(int32_t*)((uint8_t*)editor + 0x434);
            int32_t offset = i * 0x26;

            tileType = *(uint8_t*)(resourceId + 0x63A);

            if (tileType == 1 || tileType == 7) {
                /* North: front wheel above, rear wheel below */
                int32_t frontPos = (i == 0)
                    ? *(int32_t*)(editorState + 0x0C / 4) - 0x0C
                    : *(int32_t*)(*(int32_t*)(*(int32_t*)(self + 0x10) + 0x430) + 0x0C) + offset * -1;
                *(int32_t*)(frontWheelEd + 0x0C) = frontPos;
                *(int32_t*)(frontWheelEd + 0x10) = *(int32_t*)(editorState + 0x10 / 4);
                *(int32_t*)(rearWheelEd + 0x0C) = *(int32_t*)(frontWheelEd + 0x0C) - 0x16;
                *(int32_t*)(rearWheelEd + 0x10) = *(int32_t*)(editorState + 0x10 / 4);
            } else if (tileType == 2 || tileType == 8) {
                /* South: front wheel below, rear wheel above */
                int32_t frontPos = (i == 0)
                    ? *(int32_t*)(editorState + 0x0C / 4) + 0x0C
                    : *(int32_t*)(*(int32_t*)(*(int32_t*)(self + 0x10) + 0x430) + 0x0C) + offset;
                *(int32_t*)(frontWheelEd + 0x0C) = frontPos;
                *(int32_t*)(frontWheelEd + 0x10) = *(int32_t*)(editorState + 0x10 / 4);
                *(int32_t*)(rearWheelEd + 0x0C) = *(int32_t*)(frontWheelEd + 0x0C) + 0x16;
                *(int32_t*)(rearWheelEd + 0x10) = *(int32_t*)(editorState + 0x10 / 4);
            } else if (tileType == 3 || tileType == 9) {
                /* West: front wheel left, rear wheel right */
                int32_t frontPos = (i == 0)
                    ? *(int32_t*)(editorState + 0x10 / 4) - 0x0C
                    : *(int32_t*)(*(int32_t*)(*(int32_t*)(self + 0x10) + 0x430) + 0x10) + offset * -1;
                *(int32_t*)(frontWheelEd + 0x10) = frontPos;
                *(int32_t*)(frontWheelEd + 0x0C) = *(int32_t*)(editorState + 0x0C / 4);
                *(int32_t*)(rearWheelEd + 0x0C) = *(int32_t*)(editorState + 0x0C / 4);
                *(int32_t*)(rearWheelEd + 0x10) = *(int32_t*)(frontWheelEd + 0x10) - 0x16;
            } else if (tileType == 4 || tileType == 10) {
                /* East: front wheel right, rear wheel left */
                int32_t frontPos = (i == 0)
                    ? *(int32_t*)(editorState + 0x10 / 4) + 0x0C
                    : *(int32_t*)(*(int32_t*)(*(int32_t*)(self + 0x10) + 0x430) + 0x10) + offset;
                *(int32_t*)(frontWheelEd + 0x10) = frontPos;
                *(int32_t*)(frontWheelEd + 0x0C) = *(int32_t*)(editorState + 0x0C / 4);
                *(int32_t*)(rearWheelEd + 0x0C) = *(int32_t*)(editorState + 0x0C / 4);
                *(int32_t*)(rearWheelEd + 0x10) = *(int32_t*)(frontWheelEd + 0x10) + 0x16;
            }

            /* Copy position to rear wheel + 0x0C */
            *(int32_t*)(rearWheelEd + 0x0C) = *(int32_t*)(frontWheelEd + 0x0C);
            *(int32_t*)(rearWheelEd + 0x10) = *(int32_t*)(editorState + 0x10 / 4);
        }
    }

    return 1;
}

/* ================================================================== */
/* Vehicle_GetNearestTrack                                             */
/* Address: 0x44D4C0                                                   */
/* ================================================================== */
int32_t __fastcall Vehicle_GetNearestTrack(void* this)
{
    uint8_t* self = (uint8_t*)this;
    int32_t wheelEditor;

    if (*(int32_t*)(self + 0x08) == 0) {  /* forward */
        wheelEditor = *(int32_t*)(self + *(uint16_t*)(self + 0x0C) * 4 + 0x10);
        wheelEditor = *(int32_t*)(wheelEditor + 0x434);  /* rear wheel */
    } else {                               /* reverse */
        wheelEditor = *(int32_t*)(self + 0x10);
        wheelEditor = *(int32_t*)(wheelEditor + 0x430);  /* front wheel */
    }

    int32_t* target = *(int32_t**)(wheelEditor + 0x14);  /* target building */
    if (target != 0 && *(int32_t*)((uint8_t*)target + 0x10C) == 7) {
        return 0;  /* special state */
    }
    return (int32_t)target;
}

/* ================================================================== */
/* Vehicle_UpdatePosition                                              */
/* Address: 0x44D500                                                   */
/* ================================================================== */
void __thiscall Vehicle_UpdatePosition(void* this, uint8_t reverse)
{
    uint8_t* self = (uint8_t*)this;
    uint32_t i;

    /* Skip if in edge-of-map or depot direction, or if net sync pending */
    if (*(int32_t*)(self + 0x60) == 2 || *(int32_t*)(self + 0x60) == 3 ||
        *(int32_t*)(self + 0x68) != 0) {
        return;
    }

    /* Trigger sound on first editor if not already playing */
    if (*(int32_t*)(*(int32_t*)(self + 0x10) + 0x48) == 0) {
        VehicleEditor_TriggerSound(*(void**)(self + 0x10));
    }

    /* Update visible flag on all editors based on reverse param */
    for (i = 0; i <= *(uint16_t*)(self + 0x0C); i++) {
        int32_t* editor = *(int32_t**)(self + 0x10 + i * 4);
        if (editor == 0) continue;

        uint8_t oldVisible = *(uint8_t*)((uint8_t*)editor + 0x24);
        uint8_t newVisible = reverse;

        if (reverse == 0) {
            if (oldVisible == 0) continue;  /* already invisible, skip */
        } else {
            if (oldVisible != 0) continue;  /* already visible, skip */
        }

        *(uint8_t*)((uint8_t*)editor + 0x24) = newVisible;

        /* Invalidate rect for repaint */
        TileMap_InvalidateRect(&g_tilemap,
            *(int32_t*)((uint8_t*)editor + 8),
            *(int32_t*)((uint8_t*)editor + 0x0C),
            *(int32_t*)((uint8_t*)editor + 0x10),
            *(int32_t*)((uint8_t*)editor + 0x14));

        /* Pause or play audio channel */
        uint32_t audioCh = *(uint32_t*)((uint8_t*)editor + 0x48);
        if (audioCh != 0) {
            if (reverse == 0 || *(int32_t*)(self + 0x5C) == 0 ||
                *(int32_t*)(self + 0x5C) == 1 || *(int32_t*)(self + 0x5C) == 4) {
                AudioChannel_Pause(audioCh);
            } else {
                AudioChannel_Play(audioCh);
            }
        }
    }
}

/* ================================================================== */
/* Vehicle_Stop                                                        */
/* Address: 0x44D5E0                                                   */
/* ================================================================== */
void __thiscall Vehicle_Stop(void* this, int32_t param_1, uint8_t param_2)
{
    uint8_t* self = (uint8_t*)this;

    /* If activeEditor matches param_1, skip */
    if (*(int32_t*)(self + 0x08) == param_1) {
        return;
    }

    /* Skip if reentrancy guard active */
    if (self[0x5A] != 0) {
        return;
    }

    /* If param_2 is 0, check if vehicle is actually moving */
    if (param_2 == 0) {
        uint8_t moving = Vehicle_IsMoving(this);
        if (moving == 0) {
            return;
        }
    }

    /* Play engine sound update */
    if (self[0x5A] == 0) {
        self[0x5A] = 1;
        Vehicle_UpdateEngineSound(this);
        self[0x5A] = 0;
    }
}

/* ================================================================== */
/* Vehicle_IsMoving                                                    */
/* Address: 0x44D630                                                   */
/* ================================================================== */
uint8_t __fastcall Vehicle_IsMoving(void* this)
{
    uint8_t* self = (uint8_t*)this;

    /* If state is STOPPING (4), not moving */
    if (*(int32_t*)(self + 0x5C) == 4) {
        return 0;
    }

    /* If sound guard active, return false */
    if (self[0x5A] != 0) {
        return 0;
    }

    /* Get relevant wheel editor based on activeEditor flag */
    int32_t wheelEditor;
    if (*(int32_t*)(self + 0x08) == 0) {  /* forward */
        wheelEditor = *(int32_t*)(self + *(uint16_t*)(self + 0x0C) * 4 + 0x10);
        wheelEditor = *(int32_t*)(wheelEditor + 0x434);  /* rear wheel */
    } else {                               /* reverse */
        int32_t firstEditor = *(int32_t*)(self + 0x10);
        wheelEditor = *(int32_t*)(firstEditor + 0x430);  /* front wheel */
    }

    /* Check if wheel target exists and is road/building tile */
    int32_t* target = *(int32_t**)(*(int32_t*)(self + 0x20) + 0x14);
    if (target == 0) {
        return 1;  /* no target = assume moving */
    }

    int32_t targetRes = *(int32_t*)((uint8_t*)target + 0x40);
    if (!RESDATA_IsRoadTile(targetRes) && !RESDATA_IsBuildingTile(targetRes)) {
        return 1;  /* not road/building = assume moving */
    }

    /* Check wheel's current target */
    int32_t* wheelTarget = *(int32_t**)(wheelEditor + 0x14);
    if (wheelTarget != 0) {
        int32_t wheelRes = *(int32_t*)((uint8_t*)wheelTarget + 0x40);
        if (RESDATA_IsRoadTile(wheelRes) || RESDATA_IsBuildingTile(wheelRes)) {
            /* If wheel target differs from vehicle target, vehicle is moving */
            if (wheelRes != targetRes) {
                return 0;
            }
        }
    }

    return 1;
}

/* ================================================================== */
/* Vehicle_CalcSpeed                                                   */
/* Address: 0x44D6C0                                                   */
/* ================================================================== */
int16_t __thiscall Vehicle_CalcSpeed(void* this, int16_t speed)
{
    uint8_t* self = (uint8_t*)this;

    if (*(void**)(self + 0x10) == 0) {
        return 0;
    }

    if (speed == *(int16_t*)(self + 0x24)) {
        /* Forward speed */
        *(int16_t*)(self + 0x58) = speed;  /* maxSteps = speed */
        int32_t* editor = *(int32_t**)(self + 0x10);
        int32_t animData = editor[0x15];
        int32_t editorVtbl = *editor;
        (*(void (__thiscall**)(int32_t, int32_t))editorVtbl + 0x1C)(0);  /* set anim state 0 */
        (*(void (__thiscall**)(int32_t, int32_t))**(int32_t**)(self + 0x10) + 0x20)(animData, 0);  /* set frame */
    } else if (speed == *(int16_t*)(self + 0x26)) {
        /* Reverse speed */
        int32_t* editor = *(int32_t**)(self + 0x10);
        int32_t animData = editor[0x15];
        *(int16_t*)(self + 0x58) = speed;
        (*(void (__thiscall**)(int32_t, int32_t))*editor)(1);  /* set anim state 1 */
        (*(void (__thiscall**)(int32_t, int32_t))**(int32_t**)(self + 0x10) + 0x20)(animData, 0);  /* set frame */
    }

    return *(int16_t*)(self + 0x58);
}

/* ================================================================== */
/* Vehicle_UpdateSpeed                                                 */
/* Address: 0x44D720                                                   */
/* ================================================================== */
void __thiscall Vehicle_UpdateSpeed(void* this, int32_t state)
{
    int32_t curState = *(int32_t*)((uint8_t*)this + 0x5C);
    if (curState != state) {
        /* Allow transition from state 1 only to state 0 */
        if (curState != 1 || state == 0) {
            Vehicle_SetState(this, state);
        }
    }
}

/* ================================================================== */
/* Vehicle_SetState                                                    */
/* Address: 0x44D740                                                   */
/* ================================================================== */
void __thiscall Vehicle_SetState(void* this, int32_t state)
{
    uint8_t* self = (uint8_t*)this;
    uint32_t i;

    /* Skip if same state or active flag prevents change */
    if (*(int32_t*)(self + 0x5C) == state) return;
    if (self[0x90] != 0) return;

    *(int32_t*)(self + 0x5C) = state;

    if (state == 0) {
        /* STOPPED — clear everything */
        *(int32_t*)(self + 0x28) = 0;   /* stopTimer = 0 */
        *(int16_t*)(self + 0x36) = 0;   /* moveTimer = 0 */
        /* Pause all editor audio channels */
        for (i = 0; i <= *(uint16_t*)(self + 0x0C); i++) {
            if (*(int32_t*)(*(int32_t*)(self + 0x10 + i * 4) + 0x48) != 0) {
                AudioChannel_Pause(*(int32_t*)(*(int32_t*)(self + 0x10 + i * 4) + 0x48));
            }
        }
    } else if (state == 1) {
        /* APPROACHING — pause all editor audio channels */
        *(int16_t*)(self + 0x36) = 0;   /* clear move timer */
        for (i = 0; i <= *(uint16_t*)(self + 0x0C); i++) {
            if (*(int32_t*)(*(int32_t*)(self + 0x10 + i * 4) + 0x48) != 0) {
                AudioChannel_Pause(*(int32_t*)(*(int32_t*)(self + 0x10 + i * 4) + 0x48));
            }
        }
    } else if (state == 4) {
        /* STOPPING — clear move timer only */
        *(int16_t*)(self + 0x36) = 0;
    } else {
        /* MOVING (2) or WAITING (3) — start playing audio */
        if (*(int32_t*)(self + 0x68) != 0) return;  /* net sync pending */
        if (*(int32_t*)(self + 0x60) == 2) return;   /* edge-of-map */
        if (*(int32_t*)(self + 0x60) == 3) return;   /* depot */

        /* Trigger sound on first editor if not already playing */
        if (*(int32_t*)(*(int32_t*)(self + 0x10) + 0x48) == 0) {
            VehicleEditor_TriggerSound(*(void**)(self + 0x10));
        }

        /* Play all editor audio channels */
        for (i = 0; i <= *(uint16_t*)(self + 0x0C); i++) {
            int32_t* editor = *(int32_t**)(self + 0x10 + i * 4);
            if (*(uint32_t*)((uint8_t*)editor + 0x48) != 0) {
                AudioChannel_Play(*(uint32_t*)((uint8_t*)editor + 0x48));
            }
        }
    }
}

/* ================================================================== */
/* CollisionData_Ctor — Constructor for collision data (0x58 bytes)    */
/* Address: 0x44D800                                                   */
/* Previously named Vehicle_CheckCollision                              */
/* ================================================================== */
void* __fastcall CollisionData_Ctor(void* param_1)
{
    uint8_t* data = (uint8_t*)param_1;
    int32_t i;

    /* Set vtable */
    *(void**)data = (void*)VTBL_COLLISION_DATA;

    /* Zero all fields: bytes +0x04 through +0x57 */
    *(int16_t*)(data + 0x04) = 0;  /* word */
    *(int16_t*)(data + 0x06) = 0;  /* word */
    *(int32_t*)(data + 0x08) = 0;  /* dword */
    *(int32_t*)(data + 0x0C) = 0;  /* dword */
    *(int32_t*)(data + 0x10) = 0;  /* dword */
    *(int32_t*)(data + 0x14) = 0;  /* dword */

    /* Zero remaining 16 dwords from +0x18 onwards (16 * 4 = 64 bytes = up to +0x57) */
    for (i = 0; i < 16; i++) {
        *(int32_t*)(data + 0x18 + i * 4) = 0;
    }

    return param_1;
}

/* ================================================================== */
/* CollisionData_Dtor — Scalar deleting destructor for collision data  */
/* Address: 0x44D830                                                   */
/* Previously named Vehicle_ResolveCollision                            */
/* ================================================================== */
void* __thiscall CollisionData_Dtor(void* this, uint8_t param_1)
{
    /* Reset vtable */
    *(void**)this = (void*)VTBL_COLLISION_DATA;

    /* Call World_Init (treats this as World* — questionable casting) */
    World_Init(this);

    if ((param_1 & 1) != 0) {
        GLOBAL_free(this);
    }
    return this;
}
