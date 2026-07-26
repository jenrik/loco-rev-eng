/**
 * UI_ProcessObjectTimers — Main game-loop timer dispatch function
 * Address: 0x420000
 * Size: 677 bytes (0x2A5)
 * Calling convention: __cdecl
 *
 * Called by: CGWND_EnterMode3 (0x408908)
 *
 * This function is called every frame during gameplay (game mode 3).
 * It iterates over all game objects and processes their timers via
 * TileMap_ProcessObjectTimer. When a timer fires (return value = 1),
 * it dispatches to the object's vtable[7] event handler, optionally
 * looking up a target object by type ID and world position.
 *
 * It also creates tooltip/message-box popups for certain resource type
 * 0x0E (message resources) and triggers object lookups for type 0x0E
 * sound effects vs other resource types.
 *
 * Temporarily sets g_allow_building_placement = 1 during execution
 * to allow building placement checks during timer processing.
 * Restores the original value on exit.
 */

#include <stdint.h>

/* ================================================================== */
/* External function declarations                                       */
/* ================================================================== */

/* TileMap system */
int  __thiscall TileMap_ProcessObjectTimer(void* tilemap, int* gameObject);
                                                      /* @ 0x456D90 — returns 0 or 1 */
int* __thiscall TileMap_FindObject(void* tilemap, uint32_t typeId,
                                   int16_t worldX, int16_t worldY,
                                   byte someFlag, byte anotherFlag);
                                                      /* @ 0x4550C0 — find object by type+pos */

/* GameObject helpers */
int* __thiscall GameObject_GetSubObjectWorldPos(int* gameObject,
                                                 void* outPos);
                                                      /* @ 0x458310 — returns world X/Y in packed int */

/* Resource manager */
uint8_t __thiscall RESMGR_GetResourceType(uint32_t resId);
                                                      /* @ 0x446030 — returns resource type byte */

/* UI message/tooltip system */
void  __thiscall UI_CreateMessageBox(void* tooltipMgr, uint32_t resId,
                                     int16_t subId, uint8_t letter,
                                     int32_t x, int32_t y, uint8_t flags);
                                                      /* @ 0x423AB0 — create tooltip popup */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern uint32_t    g_object_count;              /* 0x4A99A0 — total objects in world */
extern uint32_t    g_game_mode;                 /* 0x4851F4 — 3 = gameplay mode */
extern uint8_t     g_allow_building_placement;  /* 0x4FD3DC — building placement flag */
extern uint8_t     g_viewport_x;                /* 0x4AAD24 — viewport X offset (low byte) */
extern uint8_t     g_viewport_y;                /* 0x4AAD28 — viewport Y offset (low byte) */

/* Object manager (implements vtable-based iteration: vtable[8] = getObjectByIndex) */
extern uint8_t     g_object_mgr[];              /* 0x4A9994 — object manager base */

/* TileMap global */
extern uint8_t     g_tilemap[];                 /* 0x4AAD08 — tilemap singleton */

/* Tooltip manager */
extern uint8_t     g_tooltip_mgr[];             /* 0x4FD220 — tooltip/message manager */

/* ================================================================== */
/* Constants                                                            */
/* ================================================================== */

/* Object metadata offsets */
#define OBJ_META_TIMER_ID           0x568  /* +0x568 — timer type ID (-1 = no timer)  */
#define OBJ_META_TIMER_PARAM        0x56C  /* +0x56C — timer callback parameter (short)*/
#define OBJ_META_TARGET_ID          0x570  /* +0x570 — target object ID (-1 = use relative offsets) */
#define OBJ_META_OFFSET_X           0x574  /* +0x574 — X offset from object position   */
#define OBJ_META_OFFSET_Y           0x578  /* +0x578 — Y offset from object position   */
#define OBJ_META_EFFECT_RES         0x57C  /* +0x57C — effect resource ID (0 = none)   */
#define OBJ_META_EFFECT_SUB_ID      0x580  /* +0x580 — effect sub-ID (short)           */
#define OBJ_META_EFFECT_TYPE        0x584  /* +0x584 — effect type code                 */
#define OBJ_META_EFFECT_OFFSET_X    0x588  /* +0x588 — effect X screen offset           */
#define OBJ_META_EFFECT_OFFSET_Y    0x58C  /* +0x58C — effect Y screen offset           */

/* Game object field offsets */
#define OBJ_FLD_UNK_02              0x08   /* +0x08 — offset of first data word        */
#define OBJ_FLD_X                   0x10   /* +0x10 — world X coordinate (saved)        */
#define OBJ_FLD_WORLD_POS           0x08   /* +0x08 — world position (X in low half)    */
#define OBJ_META_PTR                0x40   /* +0x40 — pointer to object metadata        */

/* Menu-related timer type IDs (all 0x3010-0x301B are menu buttons) */
#define TIMERID_MENU_BASE           0x3010

/* ================================================================== */
/* Helper macros                                                        */
/* ================================================================== */

/* Extract low and high 16 bits from a packed coordinate */
#define LOWORD(x)   ((int16_t)((x) & 0xFFFF))
#define HIWORD(x)   ((int16_t)((x) >> 16))

/* ================================================================== */
/* UI_ProcessObjectTimers                                               */
/*                                                                      */
/* Main game-loop timer dispatch. Called once per frame from             */
/* CGWND_EnterMode3 during gameplay (game mode 3).                      */
/* ================================================================== */
void __cdecl UI_ProcessObjectTimers(void)
{
    uint32_t   i;
    int*       gameObject;
    int*       metaData;
    uint8_t    savedAllowBuilding;
    int16_t    timerParam;
    uint32_t   timerId;
    int        x, y;             /* cached world position (x = this[2], y = this[3]) */
    uint32_t   effectResId;
    uint8_t    tileMapResult;
    int        worldX, worldY;
    int*       targetObject;
    int        packedCoord;

    /* Temporarily enable building placement during timer processing */
    savedAllowBuilding = g_allow_building_placement;
    g_allow_building_placement = 1;

    /* Iterate over all game objects */
    for (i = 0; i < g_object_count; i++) {
        /* Only process timers during gameplay mode */
        if (g_game_mode != 3) {
            g_allow_building_placement = savedAllowBuilding;
            return;
        }

        tileMapResult = 0;
        gameObject = (int*)((*(int (__thiscall**)(uint32_t))(*(uint32_t*)&g_object_mgr[0] + 0x20))(i));

        if (gameObject == 0) {
            continue;
        }

        /* Cache world position from gameObject */
        metaData = (int*)gameObject[0x40 / 4];                         /* +0x40 */
        x = gameObject[2];                                              /* +0x08 */
        y = gameObject[3];                                              /* +0x0C */

        /* Check if object has an active timer or effect */
        if (metaData[OBJ_META_TIMER_ID / 4] >= 0    /* +0x568: timer active */
            || metaData[OBJ_META_EFFECT_RES / 4] > 0) {                /* +0x57C: has effect resource */
            tileMapResult = TileMap_ProcessObjectTimer(g_tilemap, gameObject);
        }

        if (tileMapResult != 1) {
            continue;
        }

        /* -- Timer fired -- */
        timerId = (uint32_t)metaData[OBJ_META_TIMER_ID / 4];           /* +0x568 */

        if ((int32_t)timerId < 0) {
            /* Timer ID is -1, which means no timer callback */
            goto check_effect;
        }

        timerParam = *(int16_t*)((uint8_t*)metaData + OBJ_META_TIMER_PARAM);  /* +0x56C */

        if (timerId == 0 || timerId == (uint32_t)metaData[1]) {
            /* Direct dispatch: invoke vtable[7] on the object itself */
            int (*eventHandler)(int) = (int (*)(int))((int**)gameObject)[0][7];
            eventHandler((int)timerParam);
        } else {
            /* Indirect dispatch: find target object and invoke vtable[7] */

            /* Get the object's world position */
            packedCoord = *(int*)GameObject_GetSubObjectWorldPos(gameObject, &packedCoord);
            worldX = LOWORD(packedCoord);
            worldY = HIWORD(packedCoord);

            /* If target ID == -1, use offset-based positioning */
            if (metaData[OBJ_META_TARGET_ID / 4] == -1) {              /* +0x570 */
                worldX += *(int16_t*)((uint8_t*)metaData + OBJ_META_OFFSET_X);  /* +0x574 */
                worldY += *(int16_t*)((uint8_t*)metaData + OBJ_META_OFFSET_Y);  /* +0x578 */
                packedCoord = (worldY << 16) | (worldX & 0xFFFF);
            }

            /* Determine if this is a menu button timer ID */
            int isMenuButton = (timerId >= 0x3010 && timerId <= 0x301B);

            /* Find the target object by type ID and position */
            targetObject = TileMap_FindObject(
                g_tilemap, timerId,
                (int16_t)worldX, (int16_t)worldY,
                0,
                isMenuButton ? 0 : 1);

            if (targetObject != 0) {
                int (*eventHandler)(int) = (int (*)(int))((int**)targetObject)[0][7];
                eventHandler((int)timerParam);
                i = 0xFFFFFFFF;  /* Force re-iteration from beginning? */
                /* BUG: This sets local_28 = 0xFFFFFFFF which gets incremented
                   to 0 at end of loop body, effectively resetting i to 0.
                   This may cause infinite-loop-like behavior or periodic
                   re-scanning of all objects from the start. */
            }
        }

check_effect:
        /* Check for secondary effect resource */
        effectResId = (uint32_t)metaData[OBJ_META_EFFECT_RES / 4];     /* +0x57C */
        if ((int32_t)effectResId <= 0) {
            continue;
        }

        /* Check the resource type */
        uint8_t resType = RESMGR_GetResourceType(effectResId);

        if (resType == 0x0E) {
            /* Resource type 0x0E: create a tooltip/message-box popup */

            int effectType = metaData[OBJ_META_EFFECT_TYPE / 4];       /* +0x584 */
            int effectOffX = metaData[OBJ_META_EFFECT_OFFSET_X / 4];   /* +0x588 */
            int effectOffY = metaData[OBJ_META_EFFECT_OFFSET_Y / 4];   /* +0x58C */

            if (effectType == 0x53) {
                /* Type 0x53: absolute screen position (viewport-relative) */
                effectOffX += g_viewport_x;
                effectOffY += g_viewport_y;
            } else if (effectType == 0x57) {
                /* Type 0x57: absolute screen position, no object offset */
            } else {
                /* Other types: relative to object world position */
                effectOffX += x;
                effectOffY += y;
            }

            int16_t subId = *(int16_t*)((uint8_t*)metaData + OBJ_META_EFFECT_SUB_ID);  /* +0x580 */

            UI_CreateMessageBox(g_tooltip_mgr, effectResId, subId,
                                'W', effectOffX, effectOffY, 1);
        } else {
            /* Non-0x0E resource: find object at effect position */
            packedCoord = *(int*)GameObject_GetSubObjectWorldPos(gameObject, &packedCoord);
            worldX = LOWORD(packedCoord);
            worldY = HIWORD(packedCoord);

            int16_t effOffX = *(int16_t*)((uint8_t*)metaData + OBJ_META_EFFECT_OFFSET_X);  /* +0x588 */
            int16_t effOffY = *(int16_t*)((uint8_t*)metaData + OBJ_META_EFFECT_OFFSET_Y);  /* +0x58C */

            TileMap_FindObject(g_tilemap, effectResId,
                               worldX + effOffX, worldY + effOffY,
                               0, 1);
            /* Result is unused — this is likely a side-effect call
               that triggers object discovery at the effect position */
        }
    }

    /* Restore original building placement flag */
    g_allow_building_placement = savedAllowBuilding;
}
