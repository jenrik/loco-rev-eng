/**
 * game_loop_setup.c — Game loop initialization and per-frame update
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These two functions form the lifecycle backbone of the game:
 *   GameLoop_Setup     (0x406BA0) — Called once from WinMain after CGWND construction.
 *                                    Allocates and initializes all major subsystems.
 *   GameLoop_FrameUpdate (0x45C3C0) — Called every frame from WinMain's message loop.
 *                                    Drives the per-frame tick: netman, world, objects,
 *                                    buildings, and tile cache invalidation.
 *
 * Calling convention: __fastcall (ECX = param_1, but effectively used as a C function
 * with the first param in ECX by the calling convention quirk; param_1 is the CGWND
 * instance pointer which is passed through to sub-calls).
 */

#include "../shared/types.h"

/* ================================================================== */
/* External declarations                                               */
/* ================================================================== */

extern void  __fastcall CGWND_SetMode(void* unused);               /* 0x408130 */
extern void  __fastcall CGWND_RegisterWindowClass(void* cgwnd);    /* 0x406ED0 */
extern int   __fastcall CGWND_InitAllSubsystems(void* cgwnd);      /* 0x406F90 */
extern void  __fastcall Game_Update(void* game);                   /* 0x410840 */
extern void  __thiscall INPUT_LoadConfig(void* config);            /* 0x41F5E0 */
extern void  __thiscall TileMap_Init(void* tilemap, byte flags);   /* 0x454E60 */
extern int   __thiscall ResourceManager_Init(void* rmgr);          /* 0x446050 */
extern void  __thiscall UIPANEL_Hide(void* panel, void* str);      /* 0x429EF0 */
extern int   __thiscall DDRAW_Init(void);                          /* 0x45C8A0 */
extern void  __thiscall NETMAN_Update(void* netman);               /* 0x43F0C0 */
extern void  __thiscall RESMGR_VehicleAnimationTick(void*);        /* 0x448120 */
extern void  __thiscall World_UpdateTick(void* world);             /* 0x44E020 */
extern void  __thiscall UI_HideTooltip(void* mgr);                 /* 0x423D70 */
extern void  __thiscall RESDATA_ScriptedObject_Update(void* obj);  /* 0x4497A0 */
extern void  __thiscall Town_TrackBuilding(void* view);            /* 0x42D1A0 */
extern void  __thiscall DDRAW_UpdateBuilding(void* ddraw);         /* 0x459DA0 */
extern void  __thiscall INPUT_GetSaveFileName(void* ptr);          /* 0x41DD40 */
extern void  __thiscall BuildingMgr_UpdateAll(void* mgr);          /* 0x434720 */
extern void  __thiscall TileMap_InvalidateDirtyRects(void* tm, byte); /* 0x456150 */
extern void  __thiscall Vehicle_SetState(void* veh, int state);    /* 0x44D740 */
extern int   __thiscall Config_GetIniInt(void* cfg, const char* section, const char* key, int def); /* 0x452D60 */
extern void  __fastcall ScriptEngine_constructor(void* mem);       /* 0x4493A0 */
extern void  __fastcall GameConfig_constructor(void* mem);         /* 0x440C60 */
extern void  __fastcall NETMAN_constructor(void* mem);             /* 0x43D0A0 */
extern void  __fastcall DirectPlay_constructor(void* mem);         /* 0x443000 */
extern void  __fastcall PlayerRecord_constructor(void* mem);       /* 0x452E10 */
extern void  __fastcall PixelDataCache_Ctor(void* mem);            /* 0x401620 */
extern void* __cdecl operator_new(size_t size);                    /* 0x465CE0 */
extern int   __cdecl CRT_srand(unsigned int seed);                 /* 0x466140 */
extern unsigned int __cdecl CRT_timeGetTime(int* unused);          /* 0x466AF0 */

/* Windows API via IAT */
extern HANDLE  __stdcall CreateEventA(void* attr, int manual, int initial, const char* name);
extern MMRESULT __stdcall timeBeginPeriod(unsigned int period);
extern MMRESULT __stdcall timeSetEvent(unsigned int delay, unsigned int res, void* callback, unsigned int user, unsigned int flags);

/* Global state */
#define ADDR_g_ui_main          0x004FD378
#define ADDR_g_town             0x004FD37C
#define ADDR_g_postcard_send    0x004FD380
#define ADDR_g_cursor           0x004FD384
#define ADDR_g_postcard         0x004FD388
#define ADDR_g_network_thread   0x004FD398
#define ADDR_g_network_queue    0x004FD39C
#define ADDR_g_train            0x004FD3A4
#define ADDR_g_train_resources  0x004FD394
#define ADDR_g_game_config      0x004FD3A8
#define ADDR_g_netman           0x004FD3AC
#define ADDR_g_dplay            0x004FD3B0
#define ADDR_g_player_config    0x004AA4A8
#define ADDR_g_dplay_config     0x004FD3B4
#define ADDR_g_resmgr           0x004855E8
#define ADDR_g_tilemap          0x004AAD08
#define ADDR_g_game             0x004A98D8
#define ADDR_g_scripted_object  0x004AA9B0
#define ADDR_g_town_view        0x004AA818
#define ADDR_g_ddraw_building   0x004851D0
#define ADDR_g_building_mgr     0x00485448
#define ADDR_g_tooltip_mgr      0x004FD220
#define ADDR_g_game_mode        0x004851F4
#define ADDR_g_game_time        0x004A99B4
#define ADDR_g_config_ini       0x004A9EEC
#define ADDR_g_main_window      0x004AA4A0

/* Timer callback address for timeSetEvent */
#define ADDR_TIMER_CALLBACK     0x0045C520

/* String constants (from .rdata) */
static const char S_MOUSE[]       = "MOUSE";        /* 0x0047E27C */
static const char S_SETTING1[]    = "Setting1";     /* 0x0047E284 */
static const char S_SETTING2[]    = "Setting2";     /* 0x0047E270 */
static const char S_SETTING3[]    = "Setting3";     /* 0x0047E264 */
static const char S_GAMELOOP[]    = "GameLoop";     /* 0x0047E290 */

/* Mouse settings globals */
extern int DAT_004855c4;   /* Mouse Setting1 */
extern int DAT_004855c8;   /* Mouse Setting2 */
extern int DAT_004855cc;   /* Mouse Setting3 */

/* ================================================================== */
/* GameLoop_Setup                                                       */
/* Address: 0x406BA0                                                    */
/*                                                                      */
/* Called by: WinMain (0x4630FD) — after CGWND_constructor             */
/*                                                                      */
/* One-time game initialization. Sets up all subsystems:                */
/*   1. Calls CGWND_SetMode(0) to apply display mode                   */
/*   2. Seeds RNG with timeGetTime                                     */
/*   3. Allocates global singleton objects (9 total):                   */
/*      - ScriptEngine (0x1C bytes, at 0x4FD394)                       */
/*      - GameConfig  (0xB0 bytes, at 0x4FD3A8)                       */
/*      - NETMAN      (0x804 bytes, at 0x4FD3AC)                      */
/*      - DirectPlay  (0xBE4 bytes, at 0x4FD3B0)                      */
/*      - PlayerRecord (0x124 bytes, at 0x4AA4A8)                     */
/*      - PixelDataCache (0x18 bytes, at 0x4FD3B4)                    */
/*   4. Reads mouse settings from lego.ini [MOUSE] section             */
/*   5. Creates main game window (CGWND_RegisterWindowClass)           */
/*   6. Initialises tilemap (TileMap_Init)                             */
/*   7. Loads input config                                            */
/*   8. Initialises resource manager (ResourceManager_Init)            */
/*   9. Initialises all core subsystems (CGWND_InitAllSubsystems)      */
/*  10. Hides second overlay, inits DDRAW                             */
/*  11. Creates "GameLoop" named event (for synchronisation)           */
/*  12. Starts 28ms multimedia timer (timeBeginPeriod 14ms,            */
/*      timeSetEvent callback at 0x45C520)                             */
/*                                                                      */
/* On any failure, immediately returns -1. On success, returns 0.       */
/*                                                                      */
/* @param cgwnd  CGWND instance pointer (passed in ECX)                */
/* @return 0 on success, -1 on failure                                 */
/* ================================================================== */
int __fastcall GameLoop_Setup(void* cgwnd)
{
    unsigned int seed;
    void* mem;

    /* Step 1: Apply display mode */
    CGWND_SetMode(0);

    /* Step 2: Seed RNG */
    seed = CRT_timeGetTime(0);
    CRT_srand(seed);

    /* Step 3: Zero all global singleton pointers */
    /* g_ui_main, g_town, g_postcard_send, g_cursor, g_postcard */
    /* g_network_thread, g_network_queue, g_train, DAT_004fd3a0 */
    *(int*)ADDR_g_ui_main = 0;
    *(int*)ADDR_g_town = 0;
    *(int*)ADDR_g_postcard_send = 0;
    *(int*)ADDR_g_cursor = 0;
    *(int*)ADDR_g_postcard = 0;
    *(int*)0x004FD398 = 0;  /* g_network_thread */
    *(int*)0x004FD39C = 0;  /* g_network_queue */
    *(int*)0x004FD3A0 = 0;  /* DAT_004fd3a0 */
    *(int*)ADDR_g_train = 0;

    /* --- Allocate ScriptEngine (0x1C bytes) --- */
    mem = operator_new(0x1C);
    if (mem) {
        ScriptEngine_constructor(mem);
    } else {
        mem = 0;
    }
    *(int*)ADDR_g_train_resources = (int)mem;

    /* --- Allocate GameConfig (0xB0 bytes) --- */
    mem = operator_new(0xB0);
    if (mem) {
        GameConfig_constructor(mem);
    } else {
        mem = 0;
    }
    *(int*)ADDR_g_game_config = (int)mem;

    /* --- Allocate NETMAN (0x804 bytes) --- */
    mem = operator_new(0x804);
    if (mem) {
        NETMAN_constructor(mem);
    } else {
        mem = 0;
    }
    *(int*)ADDR_g_netman = (int)mem;

    /* --- Allocate DirectPlay (0xBE4 bytes) --- */
    mem = operator_new(0xBE4);
    if (mem) {
        DirectPlay_constructor(mem);
    } else {
        mem = 0;
    }
    *(int*)ADDR_g_dplay = (int)mem;

    /* --- Allocate PlayerRecord (0x124 bytes) --- */
    mem = operator_new(0x124);
    if (mem) {
        PlayerRecord_constructor(mem);
    } else {
        mem = 0;
    }
    *(int*)ADDR_g_player_config = (int)mem;

    /* --- Allocate PixelDataCache / DPlayConfig (0x18 bytes) --- */
    mem = operator_new(0x18);
    if (mem) {
        PixelDataCache_Ctor(mem);
    } else {
        mem = 0;
    }
    *(int*)ADDR_g_dplay_config = (int)mem;

    /* Step 4: Read mouse settings from lego.ini */
    DAT_004855c4 = Config_GetIniInt(
        *(void**)ADDR_g_config_ini, S_MOUSE, S_SETTING1, 0);
    DAT_004855c8 = Config_GetIniInt(
        *(void**)ADDR_g_config_ini, S_MOUSE, S_SETTING2, 0);
    DAT_004855cc = Config_GetIniInt(
        *(void**)ADDR_g_config_ini, S_MOUSE, S_SETTING3, 0);

    /* Step 5: Create main game window */
    CGWND_RegisterWindowClass(cgwnd);
    if (*(char*)(ADDR_g_main_window + 8) == 0) {  /* hWnd valid? */
        return -1;
    }

    /* Step 6: Initialize tilemap */
    TileMap_Init((void*)ADDR_g_tilemap, 0);

    /* Step 7: Load input config */
    INPUT_LoadConfig((void*)0x004A99B0);

    /* Step 8: Initialize resource manager */
    if (!ResourceManager_Init((void*)ADDR_g_resmgr)) {
        return -1;
    }

    /* Step 9: Initialize all subsystems */
    if (CGWND_InitAllSubsystems(cgwnd) != 0) {
        return -1;
    }

    /* Step 10: Hide second overlay, init DDRAW */
    UIPANEL_Hide((void*)0x004851D0, (void*)0x0047E288);  /* empty string? */

    if (!DDRAW_Init()) {
        return -1;
    }

    /* Step 11: Create named event */
    HANDLE hEvent = CreateEventA(0, 1, 0, S_GAMELOOP);
    *(int*)0x004A990C = (int)hEvent;
    if (!hEvent) {
        return -1;
    }

    /* Step 12: Start multimedia timer (28ms period, 14ms resolution) */
    MMRESULT period_result = timeBeginPeriod(14);          /* 0x0E = 14ms */
    if (period_result == 0) {                              /* TIMERR_NOERROR */
        *(int*)0x00485438 = timeSetEvent(
            28,               /* 0x1C = 28ms delay */
            14,               /* 0x0E = 14ms resolution */
            (void*)ADDR_TIMER_CALLBACK,  /* callback at 0x45C520 */
            0,                /* user data = 0 */
            1);               /* TIME_ONESHOT (or periodic?) */
    }

    return 0;
}


/* ================================================================== */
/* GameLoop_FrameUpdate — Per-frame game loop heartbeat                 */
/* Address: 0x45C3C0                                                    */
/*                                                                      */
/* Called by: WinMain message loop at 0x46322B, every frame             */
/*                                                                      */
/* This is the main per-frame update function. Execution flow:          */
/*   1. Clear timer-handled flag (DAT_00485444 = 0)                    */
/*   2. Update game time (CRT_timeGetTime)                             */
/*   3. Update NETMAN (network message processing)                     */
/*   4. Tick vehicle animation frame                                   */
/*   5. Mode check: skip if mode <= 2 (menu) or == 10                  */
/*   6. Mode 3 or 9 (town/gameplay):                                   */
/*      a. If DAT_004ff124 set: pause vehicles, tick world, resume     */
/*      b. Else: tick world directly                                   */
/*   7. Hide tooltip                                                   */
/*   8. Game_Update (handle input, animation, selection)               */
/*   9. Update scripted objects (RESDATA_ScriptedObject_Update)        */
/*  10. Town mode (3 or 9): town, DDRAW, input, building updates       */
/*  11. TileMap_InvalidateDirtyRects (flush dirty rects to screen)     */
/* ================================================================== */
void __cdecl GameLoop_FrameUpdate(void)
{
    /* Step 1: Clear timer-handled flag */
    *(int*)0x00485444 = 0;

    /* Step 2: Update game time */
    CRT_timeGetTime((int*)ADDR_g_game_time);

    /* Step 3: Network update */
    if (*(void**)ADDR_g_netman) {
        NETMAN_Update(*(void**)ADDR_g_netman);
    }

    /* Step 4: Vehicle animation tick */
    RESMGR_VehicleAnimationTick((void*)0x004A9910);

    /* Step 5: Mode check — skip if menu/idle */
    int game_mode = *(int*)ADDR_g_game_mode;
    if (game_mode < 1 || (game_mode > 2 && game_mode != 10)) {
        /* Step 6: Town/gameplay mode (3 or 9) */
        if (game_mode == 3 || game_mode == 9) {
            int paused = *(char*)0x004FF124;  /* DAT_004ff124 */

            if (paused == 1) {
                /* Pause transition: stop vehicles, tick, resume */
                if (*(char*)0x004FF11C == 1 && *(int*)0x004A98B4 != 0) {
                    int* vehicles = (int*)0x004A98B8;
                    int i;
                    for (i = 0; i < 4; i++) {
                        if (vehicles[i]) {
                            Vehicle_SetState((void*)vehicles[i], 2);  /* STOP */
                        }
                    }
                }
                World_UpdateTick((void*)0x004A98B0);
                *(char*)0x004FF11C = 0;  /* clear pause flag */

                if (*(int*)0x004A98B4 != 0) {
                    int* vehicles = (int*)0x004A98B8;
                    int i;
                    for (i = 0; i < 4; i++) {
                        if (vehicles[i]) {
                            Vehicle_SetState((void*)vehicles[i], 0);  /* RESUME */
                        }
                    }
                }
            } else {
                World_UpdateTick((void*)0x004A98B0);
            }
        }

        /* Step 7: Hide tooltip */
        UI_HideTooltip((void*)ADDR_g_tooltip_mgr);

        /* Step 8: Game update (input, animation, selection) */
        Game_Update((void*)ADDR_g_game);

        /* Step 9: Scripted object update */
        RESDATA_ScriptedObject_Update((void*)ADDR_g_scripted_object);

        /* Step 10: Town mode updates */
        if (game_mode == 3 || game_mode == 9) {
            Town_TrackBuilding((void*)ADDR_g_town_view);
            DDRAW_UpdateBuilding((void*)ADDR_g_ddraw_building);
            INPUT_GetSaveFileName((void*)0x004A9990);
            BuildingMgr_UpdateAll((void*)ADDR_g_building_mgr);
        }

        /* Step 11: Flush dirty tile rects to screen */
        TileMap_InvalidateDirtyRects((void*)ADDR_g_tilemap, 0);
    }
}
