/**
 * input_world.c — World save/load operations for the input manager
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions manage the .loco save file lifecycle: loading saved
 * worlds, creating new worlds, and saving the current game state.
 * They operate on the g_input_mgr (0x4A9990) entity manager and the
 * g_resmgr (0x4855E8) resource manager.
 *
 * Functions:
 *   INPUT_NewWorld          — Initialize a new game world          (0x41E120, 199 bytes)
 *   INPUT_LoadWorld         — Load saved world from .loco file     (0x41D320, 671 bytes)
 *   INPUT_LoadSaveFile      — Parse .loco file entity data         (0x41D5C0, 816 bytes)
 *   INPUT_SaveCurrentWorld  — Serialize world to .loco file        (0x41D9B0, 910 bytes)
 *
 * Calling convention: __thiscall (INPUT_LoadWorld, INPUT_SaveCurrentWorld)
 *                     or __fastcall (INPUT_NewWorld, INPUT_LoadSaveFile)
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                */
/* ================================================================== */

/* Win32 API */
extern HANDLE __stdcall CreateFileA(const char* path, DWORD access, DWORD share,
                                     void* security, DWORD creation, DWORD flags,
                                     HANDLE template_file);      /* 0x4770B4 */
extern BOOL   __stdcall ReadFile(HANDLE file, void* buffer, DWORD bytes,
                                  DWORD* read_bytes, void* overlapped); /* 0x4770BC */
extern BOOL   __stdcall WriteFile(HANDLE file, const void* buffer, DWORD bytes,
                                   DWORD* written, void* overlapped);   /* 0x4770A4 */
extern BOOL   __stdcall CloseHandle(HANDLE object);                     /* 0x4770A0 */

/* Resource manager */
extern void __thiscall RESMGR_ResourceData_Init(int* data);              /* 0x447B20 */
extern BOOL __thiscall RESMGR_LoadResource(int* data, char* path);       /* 0x447BA0 */
extern void __thiscall RESMGR_RemoveResource(int data);                   /* 0x447FB0 */
extern uint __thiscall RESMGR_ReleaseResource(int* data);                 /* 0x447B90 */
extern void* __thiscall RESMGR_LoadResourceData(int* data, char* path);  /* 0x447E30 */
extern void* __thiscall RESMGR_LockResource(int data);                    /* 0x447DB0 */
extern void* __thiscall RESMGR_UnlockResource(int data);                  /* 0x447DF0 */
extern BOOL  __thiscall RESMGR_IsSaveHeader(int data);                    /* 0x448030 */
extern void  __thiscall RESMGR_WriteSaveRecord(int* table, void* data);  /* 0x447F50 */
extern void  __thiscall RESMGR_WriteTableRecord(int* data, void* entry); /* 0x447F80 */

/* Game systems */
extern char  g_in_build_mode;           /* 0x4A9A00 — building placement mode flag */
extern char  g_allow_building_placement; /* building placement lock flag */
extern int   g_game_mode;               /* 0x4FD2E0 */
extern int   g_player_id;
extern int   g_player_color;
extern char  g_install_path[];           /* 0x4A99C8 */
extern void* g_netman;                   /* 0x4FD3AC */
extern void* g_resmgr;                   /* 0x4855E8 */
extern int   g_input_mgr;                /* 0x4A9990 */
extern int   g_scenario_id;              /* scenario/instance type */
extern int   DAT_004a98b4;               /* some game state counter */
extern int   DAT_004a98b8[16];           /* level/table entries for save */

extern void __thiscall PlaySound(int resId);                    /* 0x447930 */
extern void __thiscall UI_CleanupTooltips(int mgr);             /* 0x423D00 */
extern void __thiscall UI_HideTooltip(int mgr);                 /* 0x423D70 */
extern void __thiscall World_Init(int world);                   /* 0x44D9B0 */
extern void __thiscall World_UpdateTick(int world);             /* 0x44E020 */
extern void __thiscall Game_SetScreenMode(int* game, byte a, byte b, byte c); /* 0x411DC0 */
extern BOOL __thiscall TileMap_ScrollTo(int* tilemap, void* obj, byte anim);  /* 0x455AB0 */
extern void __thiscall TileMap_InvalidateDirtyRects(int* tilemap, byte flag); /* 0x456150 */
extern int* __thiscall TileMap_FindObject(int* tilemap, int resId, int x, int y, byte flag, int count);
extern void* __thiscall TileMap_GetViewport(int* tilemap, void* obj, int idx); /* 0x4579D0 */
extern byte __fastcall RESDATA_IsBuildingTile(int tileObj);     /* 0x44BD30 */
extern byte __fastcall RESDATA_IsRoadTile(int tileObj);         /* 0x44BD10 */
extern byte __fastcall RESDATA_IsSceneryTile(int tileObj);      /* 0x44BD90 */
extern int __cdecl GetResourceType(uint resId);                 /* 0x446030 */
extern void* __cdecl operator_new(size_t size);                 /* 0x465CE0 */
extern void __thiscall Sprite_UnlockAll(int spriteMgr);         /* 0x454FE0 */
extern void __thiscall UIPANEL_Hide(int* panel, char* flag);    /* 0x429EF0 */
extern uint __cdecl CRT_wcsstr(byte* str, byte* sub);           /* 0x471480 */
extern int  __cdecl CRT_itoa(int value, char* str, int radix);  /* 0x467EA0 */
extern uint __cdecl GetResourceType(uint resId);                /* 0x446030 */
extern void __thiscall ResourceManager_RegenerateData(void);    /* 0x4480C0 */
extern uint __cdecl ResourceManager_GetById(void* resmgr, uint resId); /* 0x446EA0 */

/* ================================================================== */
/* INPUT_NewWorld — Initialize a new game world                        */
/* Address: 0x41E120                                                   */
/*                                                                      */
/* Called from HelpWnd_Hide (0x450C2D) and RESDATA_ScriptedObject_      */
/* UpdateToolState (0x44AC9A) to start a fresh game.                   */
/*                                                                      */
/* Flow:                                                                */
/*   1. Set build mode, play new-game sound                             */
/*   2. Cleanup stale tooltips                                          */
/*   3. World_Init to generate fresh terrain                            */
/*   4. Iterate all entities, scroll viewport to each (every 10th       */
/*      entity uses animated scroll, rest instant)                      */
/* ================================================================== */
void __fastcall INPUT_NewWorld(int param_1)
{
    g_in_build_mode = 1;
    PlaySound(0x5026);  /* New game jingle */
    UI_CleanupTooltips(0x4FD220);
    World_Init(0x4A98B0);  /* World at DAT_004a98b0 */

    /* Scroll to each entity in the world */
    int entityCount = *(int*)(param_1 + 0x14);  /* +0x14 = entity count */
    if (entityCount != 0) {
        Game_SetScreenMode(&g_game, 1, 1, 1);  /* Enable screen tracking */

        int idx = 0;
        do {
            void* entity = (void*)(**(int (__thiscall**)(int, int))(*(int*)(param_1 + 4) + 0x20))
                                   (param_1 + 4, idx);
            BOOL success;
            if (*(uint*)(param_1 + 0x14) % 10 == 0) {
                /* Animated scroll (every 10th entity) */
                success = TileMap_ScrollTo(&g_tilemap, entity, 1);
                if (!success) {
                    idx++;
                }
                UI_HideTooltip(0x4FD220);
                TileMap_InvalidateDirtyRects(&g_tilemap, 0);
            } else {
                /* Instant scroll */
                success = TileMap_ScrollTo(&g_tilemap, entity, 0);
                if (!success) {
                    idx++;
                }
            }
        } while (idx != entityCount);

        Game_SetScreenMode(&g_game, 1, 1, 0);  /* Disable screen tracking */
    }
}

/* ================================================================== */
/* INPUT_LoadWorld — Load saved world from .loco file                  */
/* Address: 0x41D320                                                   */
/*                                                                      */
/* Called from tilemap operations to load a saved game world.          */
/* Steps:                                                              */
/*   1. Call INPUT_LoadSaveFile with the save name                     */
/*   2. If success and name contains "curr", save the path             */
/*   3. Also load the .sav backup file                                 */
/*   4. If scenario==2 (multiplayer), scroll to player buildings       */
/*      and run NETMAN edge-checking for neighbor connections          */
/*                                                                      */
/* @param param_1  byte* — save file name (e.g. "curr" or specific)    */
/* @return         char — 1 on success, 0 on failure                   */
/* ================================================================== */
char __thiscall INPUT_LoadWorld(void* _this, byte* param_1)
{
    char savePath[260];  /* local_104 */
    char result;

    /* Step 1: Load main save file */
    result = (char)INPUT_LoadSaveFile(param_1, 1, 1);

    /* Step 2: If success and "curr" in name, save the path */
    if (result != 0) {
        if (CRT_wcsstr(param_1, (byte*)"curr") != 0) {
            /* Copy save name to DAT_004aa8f8 (current save path) */
            int len = 0xFFFFFFFF;
            byte* src = param_1;
            do {
                if (len == 0) break;
                len--;
                src = param_1 + 1;
                if (*param_1 == 0) break;
                param_1 = (byte*)src;
            } while (1);
            len = ~len;
            /* memcpy to DAT_004aa8f8 */
            byte* dst = (byte*)0x4AA8F8;
            src = (byte*)((int)src - (int)(uint)len);
            for (uint i = (uint)len >> 2; i != 0; i--) {
                *(int*)dst = *(int*)src;
                src += 4;
                dst += 4;
            }
            for (uint i = (uint)len & 3; i != 0; i--) {
                *dst = *src;
                src++;
                dst++;
            }
        }
    }

    /* Step 3: Load .sav backup */
    {
        /* Build save path from install path */
        int len1 = 0xFFFFFFFF;
        char* s1 = g_install_path;
        do {
            if (len1 == 0) break;
            len1--;
            s1++;
        } while (*(s1 - 1) != '\0');
        int len2 = 0xFFFFFFFF;
        s1 = (char*)(~len1 + 0x47 + (int)g_resmgr);  /* build path str */
        do {
            if (len2 == 0) break;
            len2--;
            s1++;
        } while (*(s1 - 1) != '\0');
        /* Copy path to savePath buffer */
        byte* psrc = (byte*)(s1 + ~len2);
        byte* pdst = savePath;
        for (uint i = (uint)len2 >> 2; i != 0; i--) {
            *(int*)pdst = *(int*)psrc;
            psrc += 4;
            pdst += 4;
        }
        for (uint i = (uint)len2 & 3; i != 0; i--) {
            *pdst = *psrc;
            psrc++;
            pdst++;
        }

        /* Append ".sav" extension */
        int savLen;
        for (savLen = 0; savePath[savLen] != 0; savLen++);

        /* Find ".sav" string constant and append */
        const char* savStr = &DAT_0047e4f4;  /* ".sav" */
        int extLen = 0xFFFFFFFF;
        s1 = (char*)savStr;
        do {
            if (extLen == 0) break;
            extLen--;
            s1++;
        } while (*(s1 - 1) != '\0');
        extLen = ~extLen;
        /* Copy ".sav" to end of path */
        /* ... (string concatenation) */

        INPUT_LoadSaveFile(savePath, 0, 0);
    }

    /* Step 4: Multiplayer scenario handling */
    char savedFlag = g_allow_building_placement;
    if (*(int*)((char*)g_netman + 0x5C) == 2) {  /* netman[0x17].scenarioId == 2 */
        uint idx = 0;
        int count = (**(int (__thiscall**)(int))(*(int*)((char*)_this + 4) + 0x2C))
                       ((int)_this + 4);
        if (count != 0) {
            do {
                int* entity = (int*)(**(int (__thiscall**)(int, uint))
                                        (*(int*)((char*)_this + 4) + 0x20))((int)_this + 4, idx);
                byte resourceType = 0;
                if (entity[0x10] != 0) {  /* resource pointer at +0x40 */
                    resourceType = *(byte*)(entity[0x10] + 8);  /* type at +0x08 */
                }
                if (resourceType == 3 && *(int*)(entity + 0x43) == 3 &&
                    *(int*)(entity + 0x48) == 1) {
                    TileMap_ScrollTo(&g_tilemap, entity, 1);
                }
                idx++;
                count = (**(int (__thiscall**)(int))(*(int*)((char*)_this + 4) + 0x2C))
                           ((int)_this + 4);
            } while ((uint)idx < (uint)count);
        }

        g_allow_building_placement = 1;

        /* Check network edges for neighbor connections */
        uint (*CheckEdgeFn)(void*) = (uint (*)(void*))0x43DDF0;  /* NETMAN_CheckRightEdge */
        /* Check up, down, right, left edges - scroll to matching buildings */

        if (NETMAN_CheckUpEdge(g_netman)) {
            int* obj = TileMap_FindObject(&g_tilemap, 0xC46,
                                          (g_player_id >> 1) - 1, 0, 0, 1);
            if (obj != NULL) *(int*)(obj + 0x30) = 0;
        }
        if (NETMAN_CheckDownEdge(g_netman)) {
            short colorVal = (short)g_player_color - 2;
            int* obj = TileMap_FindObject(&g_tilemap, 0xC48,
                                          (g_player_id >> 1) - 1, (int)colorVal, 0, 1);
            if (obj != NULL) *(int*)(obj + 0x30) = 0;
        }
        if (NETMAN_CheckRightEdge(g_netman)) {
            short colorVal2 = (g_player_color >> 1) - 1;
            int* obj = TileMap_FindObject(&g_tilemap, 0xC42,
                                          g_player_id - 3, (int)colorVal2, 0, 1);
            if (obj != NULL) *(int*)(obj + 0x30) = 0;
        }
        if (NETMAN_CheckLeftEdge(g_netman)) {
            short colorVal3 = (g_player_color >> 1) - 1;
            int* obj = TileMap_FindObject(&g_tilemap, 0xC44, 0, (int)colorVal3, 0, 1);
            if (obj != NULL) *(int*)(obj + 0x30) = 0;
        }
    }
    g_allow_building_placement = savedFlag;

    return result;
}

/* ================================================================== */
/* INPUT_LoadSaveFile — Parse .loco save file entity data              */
/* Address: 0x41D5C0                                                    */
/*                                                                      */
/* Reads a .loco save file and restores entities/vehicles.             */
/*                                                                      */
/* param_1 = save file name string                                      */
/* param_2 = unlock_sprites flag (1=calls Sprite_UnlockAll)            */
/* param_3 = clear_placement_flag (0=clears +0xC0 placement flag)     */
/*                                                                      */
/* Returns: 1 on success, 0 on failure.                                */
/* SEH-protected for file I/O errors.                                   */
/* ================================================================== */
uint INPUT_LoadSaveFile(byte* param_1, char param_2, char param_3)
{
    int resourceData[44];   /* local_2ec — RESMGR resource data handle */
    char pathBuf[264];       /* local_114 — build path */
    ushort savedPlayerId;    /* local_23a */
    ushort savedPlayerColor; /* uStack_238 */
    int entityCount;         /* local_234 */
    ushort vehicleCount;     /* local_230 */
    void* sehNode;
    int idx;

    sehNode = NULL;

    /* Step 1: Initialize resource manager data */
    RESMGR_ResourceData_Init(resourceData);

    /* Step 2: Build full path from install directory + save name */
    /* Copy g_install_path to pathBuf */
    int pathLen = -1;
    char* src = g_install_path;
    do {
        if (pathLen == 0) break;
        pathLen--;
        src++;
    } while (*(src - 1) != '\0');
    int len1 = ~pathLen;
    char* s2 = (char*)(src - 1 - len1);  /* back to start */
    char* dst = pathBuf;
    for (uint i = (uint)len1 >> 2; i != 0; i--) {
        *(int*)dst = *(int*)s2;
        s2 += 4;
        dst += 4;
    }
    for (uint i = (uint)len1 & 3; i != 0; i--) {
        *dst = *s2;
        s2++;
        dst++;
    }

    /* Append save name to path */
    int nameLen = -1;
    byte* nameSrc = param_1;
    do {
        if (nameLen == 0) break;
        nameLen--;
        nameSrc++;
    } while (*(nameSrc - 1) != 0);
    nameLen = ~nameLen;
    byte* nSrc = (byte*)(nameSrc - 1 - nameLen);
    byte* nDst = (byte*)(dst - 1);  /* overwrite null */
    for (uint i = (uint)nameLen >> 2; i != 0; i--) {
        *(int*)nDst = *(int*)nSrc;
        nSrc += 4;
        nDst += 4;
    }
    for (uint i = (uint)nameLen & 3; i != 0; i--) {
        *nDst = *nSrc;
        nSrc++;
        nDst++;
    }

    /* Step 3: Load resource from the built path */
    RESMGR_LoadResource(resourceData, pathBuf);

    /* Step 4: Check if it's a valid save header */
    if (!RESMGR_IsSaveHeader((int)resourceData)) {
        RESMGR_RemoveResource((int)resourceData);
        RESMGR_ReleaseResource(resourceData);
        return 0;
    }

    /* Step 5: Calculate position offset from saved player data */
    uint offsetX = ((uint)g_player_id - (uint)savedPlayerId) / 2;
    uint offsetY = ((uint)g_player_color - (uint)savedPlayerColor) / 2;

    if (param_2 != 0) {
        Sprite_UnlockAll(0x4AAD08);  /* Unlock sprite surfaces */
    }

    /* Handle "curr" save: hide overlay */
    if (CRT_wcsstr(param_1, (byte*)"curr") != 0) {
        UIPANEL_Hide((int*)resourceData + 0x1C, (char*)((int)&vehicleCount + 2));
    }

    /* Step 6: Restore entities */
    char savedDevMode = g_allow_building_placement;
    g_allow_building_placement = 1;

    idx = 0;
    uint xOff = offsetX;
    uint yOff = offsetY;

    if (entityCount > 0) {
        do {
            ushort* lockedData = (ushort*)RESMGR_LockResource((int)resourceData);
            int* entity;
            if (lockedData == NULL) {
                entity = NULL;
            } else {
                entity = TileMap_FindObject(
                    &g_tilemap,
                    (uint)*lockedData,
                    lockedData[1] + (short)xOff,
                    lockedData[2] + (short)yOff,
                    1, 1);
            }

            if (entity != NULL) {
                if (param_3 == 0) {
                    *(int*)(entity + 0x30) = 0;  /* Clear placement flag at +0xC0 */
                }
                /* Call vtable[0x34] — Deserialize entity data from +8 onwards */
                (**(void (__thiscall**)(int*, ushort*))(entity[0] + 0x34))(entity, lockedData + 8);

                /* For non-scenery items, check if resource type needs animation update */
                if (*lockedData != 0x852 && (uint)*lockedData != 0x852) {
                    uint resType = GetResourceType((uint)*lockedData);
                    if ((char)resType != 3 ||
                        !RESDATA_IsBuildingTile(entity[0x10])) {
                        /* Call vtable[0x1C] — SetAnimState */
                        (**(void (__thiscall**)(int*, int))(entity[0] + 0x1C))
                            (entity, *(int*)(lockedData + 4));
                    }
                }

                /* Set destination/target field */
                entity[0x2F] = *(int*)(lockedData + 6);  /* +0xBC */

                /* Process child occupants (max 5) */
                ushort* childData = lockedData + 0x12;
                for (int childIdx = 5; childIdx != 0; childIdx--) {
                    if (childData[-4] != 0) {  /* child resource ID */
                        int* child = (int*)(**(int (__thiscall**)(int*, ushort))
                                              (entity[0] + 0x3C))(entity, childData[-4]);
                        if (child != NULL) {
                            if (CRT_wcsstr((byte*)childData, (byte*)"PARTY") != 0) {
                                (**(void (__thiscall**)(int*, ushort*))(child[0] + 0x34))
                                    (child, childData);
                            }
                            child[0x25] = *(int*)(childData - 2);  /* +0x94 */
                        }
                    }
                    childData += 10;  /* 10 ushorts per child record */
                }

                xOff = offsetX & 0xFFFF;
            }

            idx++;
            if (entityCount <= idx) break;
            yOff = offsetY & 0xFFFF;
        } while (1);
    }

    /* Step 7: Restore vehicle entries */
    for (int vehIdx = 0; vehIdx < (int)(vehicleCount & 0xFFFF); vehIdx++) {
        int* vehData = (int*)RESMGR_UnlockResource((int)resourceData);
        if (vehData != NULL) {
            int* building = INPUT_FindObjectAt(&g_input_mgr, 3);
            if (building != NULL) {
                void* vehicle = World_LoadFromFile(&DAT_004a98b0, building, vehData);
                if (vehicle != NULL) {
                    Vehicle_UpdatePosition(vehicle, 0);
                    (**(void (__thiscall**)(int*, int*))(*(int*)((int)vehicle + 0x10) + 0x34))
                        (vehicle, vehData + 8);
                }
            }
        }
    }

    /* Step 8: Cleanup */
    RESMGR_RemoveResource((int)resourceData);
    g_in_build_mode = 1;
    g_allow_building_placement = savedDevMode;
    RESMGR_ReleaseResource(resourceData);

    return 1;
}

/* ================================================================== */
/* INPUT_SaveCurrentWorld — Serialize all entities to a .loco file     */
/* Address: 0x41D9B0                                                    */
/*                                                                      */
/* MISNAMED in Ghidra (was INPUT_LoadDefaultWorld). This is the core   */
/* SAVE function. Serializes all placed entities (where +0xC0 == 1)    */
/* and level-table entries into a .loco resource file.                  */
/*                                                                      */
/* Called with "curr" to save the current game state.                  */
/*                                                                      */
/* @param _this     Input manager pointer                                 */
/* @param param_1  Save file name string (e.g. "curr")                  */
/* @return         1 on success, 0 on failure                           */
/* ================================================================== */
uint __thiscall INPUT_SaveCurrentWorld(void* _this, byte* param_1)
{
    int resourceData[44];     /* local_2ec */
    char pathBuf[264];        /* local_114 */
    int levelTableIdx;
    uint result;

    /* Step 1: Init resource data for writing */
    RESMGR_ResourceData_Init(resourceData);

    /* Step 2: Build install path + save name */
    {
        int pathLen = -1;
        char* s = g_install_path;
        do {
            if (pathLen == 0) break;
            pathLen--;
            s++;
        } while (*(s - 1) != '\0');
        int len = ~pathLen;
        char* src = (char*)(s - 1 - len);
        char* dst = pathBuf;
        for (uint i = (uint)len >> 2; i != 0; i--) {
            *(int*)dst = *(int*)src;
            src += 4;
            dst += 4;
        }
        for (uint i = (uint)len & 3; i != 0; i--) {
            *dst = *src;
            src++;
            dst++;
        }

        int nameLen = -1;
        byte* n = param_1;
        do {
            if (nameLen == 0) break;
            nameLen--;
            n++;
        } while (*(n - 1) != 0);
        nameLen = ~nameLen;
        byte* nSrc = (byte*)(n - 1 - nameLen);
        byte* nDst = (byte*)(dst - 1);
        for (uint i = (uint)nameLen >> 2; i != 0; i--) {
            *(int*)nDst = *(int*)nSrc;
            nSrc += 4;
            nDst += 4;
        }
        for (uint i = (uint)nameLen & 3; i != 0; i--) {
            *nDst = *nSrc;
            nSrc++;
            nDst++;
        }
    }

    /* Step 3: Set up save header fields */
    int header[0x2C];  /* Save header buffer */
    memset(header, 0, sizeof(header));

    /* Field byte at +4: size or type marker */
    *(short*)((char*)header + 0) = 8;  /* Header size/type */

    /* Player info */
    *(short*)((char*)header + 4) = g_player_id;       /* +4 */
    *(short*)((char*)header + 6) = g_player_color;    /* +6 */

    /* Scenario/instance type */
    *(short*)((char*)header + 8) = DAT_004a98b4;      /* +8 */

    /* Copy a string constant */
    {
        int strLen = -1;
        char* s = (char*)0x4AA9FD;
        do {
            if (strLen == 0) break;
            strLen--;
            s++;
        } while (*(s - 1) != '\0');
        int len = ~strLen;
        char* src = (char*)(s - 1 - len);
        char* dst = (char*)header + 10;
        for (uint i = (uint)len >> 2; i != 0; i--) {
            *(int*)dst = *(int*)src;
            src += 4;
            dst += 4;
        }
        for (uint i = (uint)len & 3; i != 0; i--) {
            *dst = *src;
            src++;
            dst++;
        }
    }

    /* Entity count field */
    *(int*)((char*)header + 0x20) = *(int*)((char*)_this + 0x14);  /* total count */

    /* Step 4: Load/create the resource file */
    void* loadResult = RESMGR_LoadResourceData(resourceData, pathBuf);
    if (loadResult == NULL) {
        RESMGR_ReleaseResource(resourceData);
        return 0;
    }

    /* Step 5: Serialize each entity where +0xC0 == 1 (placed in world) */
    int* collection = *(int**)((char*)_this + 4);  /* Entity collection */
    uint entityIdx = 0;
    int count = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);

    if (count != 0) {
        do {
            int* entity = (int*)(**(int (__thiscall**)(int, uint))(collection[0] + 0x20))
                                   ((int)collection, entityIdx);

            if (entity != NULL && *(char*)((char*)entity + 0xC0) == 1) {
                /* Build save record for _this entity */
                short saveRecord[0x20];  /* 64-byte record buffer */
                memset(saveRecord, 0, sizeof(saveRecord));

                /* Resource ID from entity's resource data */
                saveRecord[0] = *(short*)(*(int*)(entity[0x10] + 4));  /* resource ID at +0x04 */
                saveRecord[1] = *(short*)((char*)entity + 0x88);       /* position/offset +0x88 */
                saveRecord[2] = *(short*)((char*)entity + 0x28);       /* +0x28 */
                saveRecord[3] = *(short*)((char*)entity + 0xBC);       /* +0xBC */

                /* Copy entity name from +0x7C */
                char* namePtr = (char*)entity + 0x7C;
                int nameLen = -1;
                char* ns = namePtr;
                do {
                    if (nameLen == 0) break;
                    nameLen--;
                    ns++;
                } while (*(ns - 1) != '\0');
                nameLen = ~nameLen;
                char* ns2 = (char*)(ns - 1 - nameLen);
                char* nd = (char*)saveRecord + 8;  /* After header fields */
                for (uint i = (uint)nameLen >> 2; i != 0; i--) {
                    *(int*)nd = *(int*)ns2;
                    ns2 += 4;
                    nd += 4;
                }
                for (uint i = (uint)nameLen & 3; i != 0; i--) {
                    *nd = *ns2;
                    ns2++;
                    nd++;
                }

                /* Process up to 5 child occupants */
                int* childPtr = (int*)((char*)entity + 0x90);  /* +0x90 child list start */
                int childCount = 5;
                char* recordPos = (char*)saveRecord + 8 + ((nameLen + 3) & ~3);  /* aligned */

                while (childCount > 0) {
                    if (*childPtr != 0) {
                        /* Write child resource ID + position */
                        *(short*)(recordPos - 4) = *(short*)(*(int*)(*childPtr + 0x40) + 4);
                        *(int*)recordPos = *(int*)(*childPtr + 0x94);  /* child position */

                        /* Copy child name from +0x7C */
                        char* childName = (char*)*childPtr + 0x7C;
                        int cnLen = -1;
                        char* cns = childName;
                        do {
                            if (cnLen == 0) break;
                            cnLen--;
                            cns++;
                        } while (*(cns - 1) != '\0');
                        cnLen = ~cnLen;
                        /* memcpy childName into record */
                    }
                    childPtr++;
                    recordPos += 0x14;  /* 20 bytes per child record */
                    childCount--;
                }

                /* Write the save record */
                RESMGR_WriteSaveRecord((int*)resourceData, saveRecord);
            }

            entityIdx++;
            count = (**(int (__thiscall**)(int))(collection[0] + 0x2C))((int)collection);
        } while (entityIdx < (uint)count);
    }

    /* Step 6: Serialize level/table entries */
    int* tableEntry = (int*)0x4A98B8;  /* g_level_table_entry array */
    while ((int)tableEntry < 0x4A98C8) {  /* 4 entries */
        if (*tableEntry != 0) {
            int entryData[8];  /* 32-byte table entry */
            memset(entryData, 0, sizeof(entryData));

            for (int i = 4; i != 0; i--) {
                int* obj = (int*)(*(int*)(tableEntry[0x10] + 4)); /* +0x10 */
                if (*(int*)(tableEntry + 0x10) == 0) {
                    entryData[4 - i] = 0;
                } else {
                    entryData[4 - i] = (uint)*(short*)(*(int*)(*(int*)(tableEntry + 0x10) + 0x40) + 4);
                }
                tableEntry++;
            }

            /* Copy name from first entry */
            char* entryName = (char*)(*(int*)(tableEntry + 0x10) + 0x7C);
            /* ... string copy ... */

            RESMGR_WriteTableRecord(resourceData, entryData);
        }
        tableEntry++;
    }

    /* Step 7: Handle "curr" save — copy path to global current save slot */
    if (CRT_wcsstr(param_1, (byte*)"curr") != 0) {
        int nameLen = -1;
        byte* ns = param_1;
        do {
            if (nameLen == 0) break;
            nameLen--;
            ns++;
        } while (*(ns - 1) != 0);
        nameLen = ~nameLen;
        /* Copy name to current save path global */
    }

    /* Step 8: Finalize */
    RESMGR_RemoveResource((int)resourceData);
    result = RESMGR_ReleaseResource(resourceData);
    CONCAT31((int3)((uint)result >> 8), 1);

    return 1;
}

/* Forward declarations for externals not yet defined above */
extern uint __stdcall NETMAN_CheckUpEdge(void* netman);     /* 0x43DE10 */
extern uint __stdcall NETMAN_CheckDownEdge(void* netman);   /* 0x43DE20 */
extern uint __stdcall NETMAN_CheckRightEdge(void* netman);  /* 0x43DDF0 */
extern uint __stdcall NETMAN_CheckLeftEdge(void* netman);   /* 0x43DE00 */
extern void* __thiscall World_LoadFromFile(int* world, void* building, void* data); /* 0x44DC10 */
extern void __thiscall Vehicle_UpdatePosition(void* vehicle, byte flag);    /* 0x44D500 */
extern undefined4 __thiscall INPUT_FindObjectAt(void* _this, int param_1);   /* 0x41E1F0 */
