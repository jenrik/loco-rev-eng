/**
 * input_place.c — Object placement, removal, and random object finding
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Functions:
 *   INPUT_PlaceObject    — Create and place a game object by resource ID  (0x41DD80, 368 bytes)
 *   INPUT_RemoveObject   — Remove a game object and optionally show msg   (0x41DEF0, 516 bytes)
 *   INPUT_FindObjectAt   — Find a random game object matching criteria    (0x41E1F0, 863 bytes)
 *   INPUT_ExitGame       — Edit control constructor (misnamed)            (0x41E570, 141 bytes)
 *
 * Calling convention: __thiscall
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                */
/* ================================================================== */

extern void* __cdecl operator_new(size_t size);             /* 0x465CE0 */
extern void  __thiscall UI_CreateChildWindow(void* self, uint id, int param); /* UI helper */
extern void  __thiscall TrackPos_Init(int* trackPos);       /* 0x412620 */
extern void  __thiscall INPUT_HandleEditMessage(void* self, uint param_1, int param_2); /* 0x41E6E0 */
extern void  __thiscall UI_CreateMessageBox(void* mgr, int resId, ushort type, char icon,
                                             int x, int y, byte flag); /* 0x423AB0 */
extern void* __thiscall TileMap_ScrollTo(void* tilemap, void* obj, uint param); /* 0x455AB0 */
extern void* __thiscall TileMap_GetViewport(void* tilemap, void* obj, uint idx); /* 0x4579D0 */
extern int*  __thiscall TileMap_FindObject(void* tilemap, uint resId,
                                            int x, int y, byte flag, int count); /* 0x4550C0 */
extern int   __cdecl GetResourceType(uint resId);           /* 0x446030 */
extern uint  __cdecl ResourceManager_GetById(void* resmgr, uint resId); /* 0x446EA0 */
extern char  g_build_mode;                                  /* building mode flag */
extern void* g_tooltip_mgr;                                 /* 0x4FD220 */
extern int*  g_tilemap;                                     /* tilemap pointer */
extern void* g_resmgr;                                      /* 0x4855E8 */
extern int   g_input_mgr;                                   /* 0x4A9990 */
extern int   CRT_rand(void);                                /* 0x466150 */
extern byte __fastcall RESDATA_IsBuildingTile(int tileObj); /* 0x44BD30 */
extern byte __fastcall RESDATA_IsRoadTile(int tileObj);     /* 0x44BD10 */
extern byte __fastcall RESDATA_IsSceneryTile(int tileObj);  /* 0x44BD90 */
extern void* __thiscall RESDATA_GameVehicle_Ctor(void* self, uint resId);  /* 0x44AE80 */
extern void* __thiscall RESDATA_GameObject_Ctor(void* self, uint resId);   /* 0x4580A0 */
extern void* __thiscall GameVehicle_Ctor(void* self, uint resId);          /* 0x412870 */
extern void* __thiscall HelpWnd_FindPage(void* self, uint resId);          /* 0x44F210 */

/* ================================================================== */
/* INPUT_PlaceObject — Create and place a game object                  */
/* Address: 0x41DD80                                                    */
/*                                                                      */
/* MISNAMED in Ghidra (was INPUT_SaveGameDialog). This creates a game   */
/* object from a resource ID and adds it to the entity manager.         */
/*                                                                      */
/* Dispatches on resource type:                                         */
/*   type 3 (scenery): check if building tile or road tile              */
/*     - Building: GameVehicle_Ctor (0x11c bytes)                       */
/*     - Road: HelpWnd_FindPage (0x128 bytes)                           */
/*     - Other scenery: RESDATA_GameVehicle_Ctor (300 bytes)            */
/*   other types: RESDATA_GameObject_Ctor (0x10c bytes)                 */
/*                                                                      */
/* If the object initializes successfully (+0x06 byte == 1), adds it    */
/* to the entity collection and increments counters.                    */
/*                                                                      */
/* Called from TileMap_FindObject during object placement.              */
/* Returns: created object pointer, or NULL on failure.                 */
/* ================================================================== */
undefined4* __thiscall INPUT_PlaceObject(void* _this, uint param_1)
{
    void* newObj;
    undefined4* result;

    uint resType = GetResourceType(param_1);

    if ((char)resType == 3) {
        /* Scenery type — determine specific category */
        int resData = ResourceManager_GetById(g_resmgr, param_1);
        if (RESDATA_IsBuildingTile(resData)) {
            /* Building tile: create GameVehicle */
            newObj = operator_new(0x11C);
            if (newObj != NULL) {
                result = GameVehicle_Ctor(newObj, param_1);
                goto add_entity;
            }
        } else if (RESDATA_IsRoadTile(resData)) {
            /* Road tile: create via HelpWnd_FindPage */
            newObj = operator_new(0x128);
            if (newObj != NULL) {
                result = HelpWnd_FindPage(newObj, param_1);
                goto add_entity;
            }
        } else {
            /* Other scenery: create via RESDATA_GameVehicle_Ctor */
            newObj = operator_new(300);
            if (newObj != NULL) {
                result = RESDATA_GameVehicle_Ctor(newObj, param_1);
                goto add_entity;
            }
        }
    } else {
        /* Non-scenery type: create generic game object */
        newObj = operator_new(0x10C);
        if (newObj != NULL) {
            result = RESDATA_GameObject_Ctor(newObj, param_1);
            goto add_entity;
        }
    }

    /* Allocation failed */
    return NULL;

add_entity:
    /* Check if object initialized successfully (byte at +0x06 == 1) */
    if (*(char*)((char*)result + 6) != 1) {
        /* Failed to init — destroy and return NULL */
        typedef void (__thiscall* DtorFn)(void* self, byte flags);
        DtorFn dtor = (DtorFn)(*(void**)result)[0];
        dtor(result, 1);
        return NULL;
    }

    /* Add to entity collection via vtable[0x34] */
    (**(void (__thiscall**)(void*, void*))
        (*(int*)(*(int*)((char*)_this + 4) + 0x34)))(*(void**)((char*)_this + 4), result);

    /* Increment scenery counter if resource has scenery flag at +0x62C */
    if (*(char*)(result[0x10] + 0x62C) != 0) {
        *(int*)((char*)_this + 0x18) += 1;  /* scenery count */
    }

    /* Increment total entity count */
    *(int*)((char*)_this + 0x14) += 1;  /* total count */

    return result;
}

/* ================================================================== */
/* INPUT_RemoveObject — Remove a game object from the world            */
/* Address: 0x41DEF0                                                    */
/*                                                                      */
/* Removes the given object from the entity manager's collection.      */
/* Decrements total and scenery counters. Searches adjacent viewports  */
/* for a tile to scroll to afterwards. Shows a removal message box if  */
/* param_2 is non-zero.                                                */
/*                                                                      */
/* @param _this     Input manager                                         */
/* @param param_1  Object to remove                                      */
/* @param param_2  Show message box flag (0 = silent)                   */
/* ================================================================== */
void __thiscall INPUT_RemoveObject(void* _this, undefined4* param_1, undefined4 param_2)
{
    if (param_1 == NULL) {
        return;
    }

    /* Step 1: Find and remove from entity collection */
    int* collection = (int*)((char*)_this + 4);
    uint idx = 0;
    int count = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);

    if (count != 0) {
        do {
            undefined4* entity = (undefined4*)(**(int (__thiscall**)(int, uint))
                                                  (collection[0] + 0x20))((int)collection, idx);
            if (entity == param_1) {
                /* Remove via vtable[0x0C] (InternalRemoveAt) */
                (**(void (__thiscall**)(int, uint))(collection[0] + 0x0C))((int)collection, idx);

                /* Decrement counters */
                *(int*)((char*)_this + 0x14) -= 1;  /* Total count - 0x14 */

                if (*(char*)(param_1[0x10] + 0x62C) != 0) {
                    *(int*)((char*)_this + 0x18) -= 1;  /* Scenery count - 0x18 */
                }
                break;
            }
            idx++;
            count = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);
        } while (idx < (uint)count);
    }

    /* Step 2: Handle tile type / resource ID specific logic */
    int resPtr = param_1[0x10];  /* +0x40 = resource pointer */
    byte resType = (resPtr != 0) ? *(byte*)(resPtr + 8) : 0;

    if (resType == 0x0C) {  /* Vehicle type */
        int resId = (resPtr != 0) ? *(int*)(resPtr + 4) : -1;

        if (resId > 0x3010) {  /* Specific vehicle resource threshold */
            if ((resId & 1) != 0) {
                *(byte*)((char*)param_1 + 6) = 0;  /* Clear init flag */

                /* Search 4 adjacent viewports for a scenery tile to scroll to */
                for (uint vpIdx = 0; vpIdx < 4; vpIdx++) {
                    undefined4* viewport = (undefined4*)TileMap_GetViewport(
                        g_tilemap, param_1, vpIdx);
                    if (viewport != NULL && *(char*)((char*)viewport + 6) == 1) {
                        int vpRes = viewport[0x10];
                        byte vpType = (vpRes != 0) ? *(byte*)(vpRes + 8) : 0;
                        if (vpType == 3 && RESDATA_IsSceneryTile(vpRes)) {
                            goto scroll_and_msg;
                        }
                    }
                }
                goto show_message;
            }
        }
    }

    if (resType == 3 && RESDATA_IsSceneryTile(resPtr)) {
        *(byte*)((char*)param_1 + 6) = 0;  /* Clear init flag */

        /* Search 4 adjacent viewports for a vehicle to scroll to */
        for (uint vpIdx = 0; vpIdx < 4; vpIdx++) {
            undefined4* viewport = (undefined4*)TileMap_GetViewport(
                g_tilemap, param_1, vpIdx);
            if (viewport != NULL && *(char*)((char*)viewport + 6) == 1) {
                int vpRes = viewport[0x10];
                byte vpType = (vpRes != 0) ? *(byte*)(vpRes + 8) : 0;
                if (vpType == 0x0C) {
                    int vpResId = (vpRes != 0) ? *(int*)(vpRes + 4) : -1;
                    if (vpResId > 0x3010 && (vpResId & 1) != 0) {
                        goto scroll_and_msg;
                    }
                }
            }
        }
    }

show_message:
    /* Step 3: Show removal message box (if not silent) */
    if ((char)param_2 != 0) {
        int x, y;
        int resPtr2 = param_1[0x10];  /* +0x40 */
        if (g_build_mode != 1) {
            /* Normal camera: use entity world position */
            y = param_1[3] + (uint)(*(ushort*)(resPtr2 + 0x16) >> 1);  /* +0x0C */
            x = param_1[2] + (uint)(*(ushort*)(resPtr2 + 0x14) >> 1);  /* +0x08 */
        } else {
            /* Build mode: same calculation */
            y = param_1[3] + (uint)(*(ushort*)(resPtr2 + 0x16) >> 1);  /* +0x0C */
            x = param_1[2] + (uint)(*(ushort*)(resPtr2 + 0x14) >> 1);  /* +0x08 */
        }

        UI_CreateMessageBox(g_tooltip_mgr, 0x3860,
                            (ushort)(g_build_mode != 1), 'W', x, y, 1);
    }

    /* Step 4: Destroy the object */
    typedef void (__thiscall* DtorFn)(void* self, byte flags);
    DtorFn dtor = (DtorFn)(*(void**)param_1)[0];
    dtor(param_1, 1);
    return;

scroll_and_msg:
    /* Fallback: if we found a matching viewport, scroll to it first */
    viewport_scroll[0x31] = 0;
    viewport_scroll[0x32] = 0;
    viewport_scroll[0x33] = 0;
    viewport_scroll[0x34] = 0;
    TileMap_ScrollTo(g_tilemap, viewport_scroll, param_2);
    goto show_message;
}

/* ================================================================== */
/* INPUT_FindObjectAt — Find a random game object matching criteria    */
/* Address: 0x41E1F0                                                    */
/*                                                                      */
/* Finds a game object in the entity collection based on param_1:      */
/*   -1 = random object (from total count)                              */
/*    0/1/4 = building with specific sub-type and field values          */
/*    2 = scenery objects with flag at +0x62C                           */
/*    3 = any building tile                                              */
/*    other = by resource ID (matches +0x04 in resource data)           */
/*                                                                      */
/* Uses CRT_rand() for randomization. Returns object pointer or 0.     */
/* ================================================================== */
undefined4 __thiscall INPUT_FindObjectAt(void* _this, int param_1)
{
    int foundCount = 0;
    int* collection = (int*)((char*)_this + 4);
    int count;
    uint idx;

    switch (param_1) {
    case 0:
    case 1:
    case 4:
        /* Count buildings with type 3, subtype field at +0x10C == 3,
           and +0x120 matching param_1 (if not 4) */
        idx = 0;
        count = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);
        if (count != 0) {
            do {
                int entity = (**(int (__thiscall**)(int, uint))(collection[0] + 0x20))
                               ((int)collection, idx);
                byte resType = 0;
                if (*(int*)(entity + 0x40) != 0) {
                    resType = *(byte*)(*(int*)(entity + 0x40) + 8);
                }
                if (resType == 3 && *(int*)(entity + 0x10C) == 3 &&
                    (param_1 == 4 || *(int*)(entity + 0x120) == param_1)) {
                    foundCount++;
                }
                idx++;
                count = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);
            } while (idx < (uint)count);
        }
        break;

    case 2:
        /* Count objects with scenery flag at resource +0x62C */
        foundCount = *(int*)((char*)_this + 0x18);  /* scenery count cache */
        break;

    case 3:
        /* Count building tiles */
        idx = 0;
        count = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);
        if (count != 0) {
            do {
                int entity = (**(int (__thiscall**)(int, uint))(collection[0] + 0x20))
                               ((int)collection, idx);
                int resPtr = *(int*)(entity + 0x40);
                byte resType = (resPtr != 0) ? *(byte*)(resPtr + 8) : 0;
                if (resType == 3 && RESDATA_IsBuildingTile(resPtr)) {
                    foundCount++;
                }
                idx++;
                count = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);
            } while (idx < (uint)count);
        }
        break;

    case -1:
        /* Random: pick any object from total count */
        if (*(int*)((char*)_this + 0x14) != 0) {
            uint randIdx = (uint)CRT_rand() % *(uint*)((char*)_this + 0x14);
            return (**(int (__thiscall**)(int, uint))(collection[0] + 0x20))
                      ((int)collection, randIdx);
        }
        return 0;

    default:
        /* By resource ID: count/resource objects then pick random one */
        int resData = ResourceManager_GetById(g_resmgr, (uint)param_1);
        if (resData != 0) {
            ushort countField = *(ushort*)(resData + 0x158);
            if (countField > 0) {
                int target = ((uint)CRT_rand() % (uint)countField) + 1;
                int seen = 0;
                idx = 0;
                count = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);
                if (count != 0) {
                    do {
                        if (seen == target) return 0;
                        int entity = (**(int (__thiscall**)(int, uint))(collection[0] + 0x20))
                                       ((int)collection, idx);
                        if (*(int*)(*(int*)(entity + 0x40) + 4) == param_1) {
                            seen++;
                        }
                        idx++;
                        count = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);
                    } while (idx < (uint)count);
                }
            }
        }
        return 0;
    }

    /* Choose random item from matching count */
    if (foundCount == 0) return 0;

    int target;
    if (foundCount < 0) {
        int adjusted = 2 - foundCount;
        target = (adjusted != 0) ? (CRT_rand() % adjusted) + foundCount : foundCount;
    } else {
        target = (CRT_rand() % foundCount) + 1;
    }

    /* Find the Nth matching entity */
    int seen = 0;
    idx = 0;
    int maxCount = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);

    if (maxCount != 0) {
        while (seen != target) {
            int entity = (**(int (__thiscall**)(int, uint))(collection[0] + 0x20))
                           ((int)collection, idx);

            int resPtr = (param_1 == 2) ? *(int*)(entity + 0x40) : *(int*)(entity + 0x40);
            byte resType = (resPtr != 0) ? *(byte*)(resPtr + 8) : 0;

            BOOL matches = FALSE;
            switch (param_1) {
            case 0: case 1: case 4:
                matches = (resType == 3 && *(int*)(entity + 0x10C) == 3 &&
                          (param_1 == 4 || *(int*)(entity + 0x120) == param_1));
                break;
            case 2:
                matches = (*(char*)(*(int*)(entity + 0x40) + 0x62C) != 0);
                break;
            case 3:
                matches = (resType == 3 && RESDATA_IsBuildingTile(resPtr));
                break;
            }

            if (matches) seen++;
            idx++;
            maxCount = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);
            if ((uint)maxCount <= idx) return 0;
        }

        return (**(int (__thiscall**)(int, uint))(collection[0] + 0x20))((int)collection, idx - 1);
    }

    return 0;
}
