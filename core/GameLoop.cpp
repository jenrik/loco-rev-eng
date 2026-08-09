/**
 * GameLoop.cpp - Game loop initialization and per-frame update
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Two functions form the lifecycle backbone of the game:
 *   GameLoop_Setup     (0x406BA0) - Called once from WinMain after CGWND construction.
 *                                    Allocates and initializes all major subsystems.
 *   GameLoop_FrameUpdate (0x45C3C0) - Called every frame from WinMain's message loop.
 *                                    Drives the per-frame tick.
 */

// Status: TRANSCRIBED

#include "CGWND.h"
#include "Game.h"
#include "../game/PlayerConfig.h"
#include "../network/NetworkPlayerList.h"
#include "../shared/types.h"
#include "../world/tilemap.h"
#include "../graphics/PixelDataCache.h"
#ifndef _WIN32
#include "sdl3_net_game_bridge.h"
#include "sdl3_town_mode3.h"
namespace loco { namespace host { void BootstrapMode3Core(); } }
#endif
#include <cstdio>
#include <cstdlib>
#include <new>

/* ================================================================== */
/* External declarations (not yet in headers)                          */
/* ================================================================== */

/* CRT helpers (C linkage) */
/* CRT helpers (C++ linkage - match stubs_impl.cpp) */
void  CRT_srand(unsigned int seed);
unsigned int CRT_timeGetTime(void);
void* operator_new(size_t size);

/* Subsystem constructors (C++ linkage) */
/* Narrow factory pair for a full-sized ScriptEngine (world/scriptengine.
 * cpp) -- declared narrowly rather than #include-ing scriptengine.h,
 * which would pull in that header's own, separately-tracked
 * g_scripted_object redeclaration conflict. */
size_t ScriptEngine_HostSize();
void*  ScriptEngine_HostConstruct(void* mem);
void* GameConfig_constructor(void* mem);         /* 0x440C60 */
void* NETMAN_constructor(void* mem);             /* 0x43D0A0 */
PlayerConfig* PlayerRecord_constructor(PlayerConfig* config); /* 0x452E10 */

/* Subsystem init/update (C++ linkage) */
int   Config_GetIniInt(void* cfg, const char* section, const char* key, int def); /* 0x452D60 */
int   ResourceManager_Init(void* rmgr);          /* 0x446050 */
class InputMgr;
void  INPUT_GetSaveFileName(InputMgr* self);      /* 0x41DD40 — per-frame entity tick */
void  UIPANEL_Hide(void* panel, void* str);      /* 0x429EF0 */
int   DDRAW_Init(void);                          /* 0x45C8A0 */
void  NETMAN_Update(void* netman);               /* 0x43F0C0 */
void  RESMGR_VehicleAnimationTick(void*);        /* 0x448120 */
void  World_UpdateTick(void* world);             /* 0x44E020 */
void  UI_HideTooltip(void* mgr);                 /* 0x423D70 */
void  RESDATA_ScriptedObject_Update(void* obj);  /* 0x4497A0 */
void  Town_TrackBuilding(void* view);            /* 0x42D1A0 */
void  DDRAW_UpdateBuilding(void* ddraw);         /* 0x459DA0 */
void  BuildingMgr_UpdateAll(void* mgr);          /* 0x434720 */
int   Vehicle_SetState(void* veh, int state);    /* 0x44D740 */

/* Windows API (C linkage) */
extern "C" {
void* CreateEventA(void* attr, int manual, int initial, const char* name);
int   timeBeginPeriod(unsigned int period);
int   timeSetEvent(unsigned int delay, unsigned int res, void* callback, unsigned int user, unsigned int flags);
void  LAB_0045c520(void);     /* 0x45C520 - timer callback */
}

/* Global singletons (declared in stubs_impl.cpp but not yet in shared headers) */
extern void*    g_ui_main;           /* 0x4FD378 */
extern void*    g_town;              /* 0x4FD37C */
extern void*    g_postcard_send;     /* 0x4FD380 */
extern void*    g_cursor;            /* 0x4FD384 */
extern void*    g_postcard;          /* 0x4FD388 */
extern void*    g_network_thread;    /* 0x4FD398 */
extern void*    g_network_queue;     /* 0x4FD39C */
extern void*    g_train;             /* 0x4FD3A4 */
extern void*    g_train_resources;   /* 0x4FD394 */
extern void*    g_game_config;       /* 0x4FD3A8 */
extern void*    g_netman;            /* 0x4FD3AC */
#ifndef _WIN32
extern Netman*  _g_netman;           /* stale translated alias of 0x4FD3AC */
#endif
extern PlayerConfig* g_player_config; /* 0x4AA4A8 */
extern void*    g_dplay_config;      /* 0x4FD3B4 */
extern void*    g_resmgr;            /* 0x4855E8 */
extern void*    g_scripted_object;   /* 0x4AA9B0 */
extern void*    g_town_view;         /* 0x4AA818 */
extern void*    g_ddraw_building;    /* 0x4851D0 */
extern void*    g_building_mgr;      /* 0x485448 */
extern void*    g_tooltip_mgr;       /* 0x4FD220 */
extern void*    g_second_overlay;    /* 0x4851D0 */
extern void*    g_world;             /* 0x4A98B0 */
/* g_game_mode declared int32_t in world/tilemap.h (included above) —
 * matches the real definition in shared/stubs_impl.cpp; the old local
 * `uint8_t` extern here was itself a type mismatch on the same global. */
extern void*    g_game;              /* 0x4854C8 */
extern char     g_empty_string;      /* empty string singleton */

/* Mouse settings */
extern int g_mouse_spi3[3];          /* 0x4855C4 */
extern int g_mouse_spi4[3];          /* 0x4855C8 */
extern int g_mouse_spi5[3];          /* 0x4855CC */

/* Misc globals */
extern int   DAT_004fd3a0;           /* 0x4FD3A0 */
extern int   DAT_004a990c;           /* 0x4A990C */
extern void* g_timer_event_id;       /* 0x485438 */
extern int   DAT_00485444;           /* 0x485444 */
class InputMgr;
extern InputMgr g_input_mgr;        /* 0x4A9990 — static InputMgr object */
extern uint8_t g_input_events[];   /* 0x4A99B0 — event-list window object */
extern int   DAT_004ff124;           /* 0x4FF124 */
extern int   DAT_004ff11c;           /* 0x4FF11C */
extern int   DAT_004a98b4;           /* 0x4A98B4 */
extern void* g_world_vehicles[4];    /* 0x4A98B8 */

/* Note: g_config_ini, g_game_time, g_main_window declared in types.h */

/* String constants */
static const char S_MOUSE[]    = "MOUSE";
static const char S_SETTING1[] = "Setting1";
static const char S_SETTING2[] = "Setting2";
static const char S_SETTING3[] = "Setting3";
static const char S_GAMELOOP[] = "GameLoop";

extern "C" int GameLoop_Setup(void* cgwnd);

static void trace_setup_stage(const char* stage)
{
    std::fprintf(stderr, "[TRACE] GameLoop_Setup: %s\n", stage);
    std::fflush(stderr);
}


/* ================================================================== */
/* GameLoop_Setup - One-time game initialization                        */
/* Address: 0x406BA0                                                    */
/*                                                                      */
/* Called by: WinMain (0x4630FD) after CGWND constructor                */
/*                                                                      */
/* Allocates all singletons, reads config, creates window,              */
/* initializes subsystems, starts 28ms multimedia timer.                */
/*                                                                      */
/* @param cgwnd  CGWND instance pointer                                 */
/* @return 0 on success, -1 on failure                                  */
/* ================================================================== */
extern "C" int GameLoop_Setup(void* cgwnd)
{
    unsigned int seed;
    void* mem;

    /* Step 1: Apply display mode */
    trace_setup_stage("step 1: mode reset");
    CGWND_SetMode(0);

    /* Step 2: Seed RNG */
    seed = CRT_timeGetTime();
    CRT_srand(seed);

    /* Step 3: Zero all global singleton pointers */
    g_ui_main        = nullptr;
    g_town           = nullptr;
    g_postcard_send  = nullptr;
    g_cursor         = nullptr;
    g_postcard       = nullptr;
    g_network_thread = nullptr;
    g_network_queue  = nullptr;
    DAT_004fd3a0     = 0;
    g_train          = nullptr;

    /* Allocate ScriptEngine. Was operator_new(0x1C) (the original x86
     * sizeof(ScriptEngine)) placement-constructed via the ScriptEngine_
     * constructor ABI bridge — an undersized-allocation landmine on this
     * 64-bit host, where sizeof(ScriptEngine) is 64 bytes, not 0x1C
     * (pointer-widened members past the +0x04 critical-section block).
     * Fixed to allocate and construct the real class directly. */
    trace_setup_stage("step 3a: ScriptEngine");
    mem = operator_new(ScriptEngine_HostSize());
    g_train_resources = mem ? ScriptEngine_HostConstruct(mem) : nullptr;

    /* Allocate GameConfig (0xB0 bytes) */
    trace_setup_stage("step 3b: GameConfig");
    mem = operator_new(0xB0);
    g_game_config = mem ? GameConfig_constructor(mem) : nullptr;

    /* Allocate NETMAN (0x804 bytes) */
    trace_setup_stage("step 3c: Netman");
#ifdef _WIN32
    mem = operator_new(0x804);
    g_netman = mem ? NETMAN_constructor(mem) : nullptr;
#else
    g_netman = lego_loco::network::CreateHostNetman();
    _g_netman = static_cast<Netman*>(g_netman);
#endif

    /* Allocate NetworkPlayerList (0xBE4 bytes in the original x86 layout).
     * Placement construction preserves the recovered allocator/null path while
     * letting C++ establish the dispatch pointer and native host layout. */
    trace_setup_stage("step 3d: DirectPlay");
    mem = operator_new(sizeof(NetworkPlayerList));
    g_dplay = mem ? ::new (mem) NetworkPlayerList() : nullptr;

    /* Allocate PlayerRecord (0x124 bytes) */
    trace_setup_stage("step 3e: PlayerRecord");
    mem = operator_new(0x124);
    g_player_config = mem ? PlayerRecord_constructor(static_cast<PlayerConfig*>(mem))
                          : nullptr;

    /* Allocate PixelDataCache. Was operator_new(0x18) (the original x86
     * sizeof(PixelDataCache)) -- another undersized-allocation landmine on
     * this 64-bit host, where sizeof(PixelDataCache) is 32 bytes, not 0x18
     * (pointer-widened pixel_buffer member plus alignment padding). The old
     * manual-poke stub never wrote past +0x14 so it fit in 24 bytes
     * harmlessly; PixelDataCache::Create writes saved_album_index at +0x20,
     * 8 bytes past the old allocation -- confirmed via a live repro
     * (coredumpctl backtrace: SIGSEGV inside vsnprintf during the
     * subsequent Load(1) call, a classic corrupted-heap-surfaces-later
     * symptom, not a fault at the allocation site itself). */
    trace_setup_stage("step 3f: PixelDataCache");
    mem = operator_new(sizeof(PixelDataCache));
    g_dplay_config = mem ? PixelDataCache::Create(mem) : nullptr;

    trace_setup_stage("step 4: config");
    /* Step 4: Read mouse settings from lego.ini */
    g_mouse_spi3[0] = Config_GetIniInt(g_config_ini, S_MOUSE, S_SETTING1, 0);
    g_mouse_spi4[0] = Config_GetIniInt(g_config_ini, S_MOUSE, S_SETTING2, 0);
    g_mouse_spi5[0] = Config_GetIniInt(g_config_ini, S_MOUSE, S_SETTING3, 0);

    trace_setup_stage("step 5: main window");
    /* Step 5: Create main game window */
    if (!((CGWND*)cgwnd)->RegisterWindowClass()) {
        std::fprintf(stderr, "[TRACE] GameLoop_Setup FAILED at step 5\n"); std::fflush(stderr);
        return -1;
    }

    trace_setup_stage("step 6: tilemap");
    /* Step 6: Initialize tilemap */
#ifdef _WIN32
    TileMap_Init(g_tilemap, 0);
#else
    /* On host, g_tilemap is a lazily-constructed pointer (see
     * HostMode3Bootstrap.cpp) rather than the static object the original
     * binary has at this point; it doesn't exist yet here and gets its own
     * Init() call once constructed. */
    if (g_tilemap != nullptr) {
        TileMap_Init(g_tilemap, 0);
    }
#endif

    trace_setup_stage("step 7: input config");
    /* Step 7: Load events — original (GameLoop_Setup 0x406DA8):
     *   push 0x47E29C; mov ecx,0x4A99B0; call 0x41F5E0
     * 0x41F5E0 loads the [LoadEvents] section (string 0x47E608) from
     * <ResDir>EE.INI — the file name is built as "%s%s.ini" (0x47E61C)
     * over the Res-dir buffer 0x4A99C8 and the "ee" suffix pushed
     * here (0x47E29C), with "%03ld" keys (0x47E614).  The 0x4A99B0
     * event-list object is NOT reconstructed yet (event-list
     * reconstruction belongs to the persistence milestone); the host
     * path is an explicit guarded adapter — it logs loudly instead of
     * silently no-op'ing, and the original path is preserved under
     * _WIN32.  The legacy "INPUT_LoadConfig" label was a misnomer. */
#ifndef _WIN32
    std::fprintf(stderr,
        "[HOST] INPUT_LoadEvents (0x41F5E0) deferred: 0x4A99B0 event-list "
        "object not reconstructed\n");
    std::fflush(stderr);
#else
    /* Original thiscall: ECX = &g_input_events (0x4A99B0), one stack arg =
     * the "ee" suffix at 0x47E29C.  Declared here so the original path
     * stays expressed; the definition arrives with the reconstruction. */
    extern void INPUT_LoadEvents(void* self, const char* suffix);  /* 0x41F5E0 */
    INPUT_LoadEvents(&g_input_events, "ee");
#endif

    trace_setup_stage("step 8: resources");
    /* Step 8: Initialize resource manager */
    if (!ResourceManager_Init(g_resmgr)) {
        std::fprintf(stderr, "[TRACE] GameLoop_Setup FAILED at step 8\n"); std::fflush(stderr);
        return -1;
    }

#ifndef _WIN32
    /* Host mode-3 cone: the original constructs these embedded singletons
     * via CRT static-init thunks (0x45C560..0x45C650) before WinMain. The
     * SDL host does it here, after ResourceManager_Init, in the same order
     * (Game, World, BuildingMgr, ScriptedObject, TileMap, GameAudio). */
    loco::host::BootstrapMode3Core();
    loco::host::BootstrapTownMode3Objects();
    if (!loco::host::Mode3FrameDependenciesReady()) {
        std::fputs("FATAL: mode-3 frame dependencies were not initialized\n", stderr);
        std::abort();
    }
#endif

    trace_setup_stage("step 9: UI subsystems");
    std::fprintf(stderr, "[TRACE] GameLoop_Setup: calling InitAllSubsystems...\n"); std::fflush(stderr);
    /* Step 9: Initialize all subsystems */
    if (((CGWND*)cgwnd)->InitAllSubsystems() != 0) {
        std::fprintf(stderr, "[TRACE] GameLoop_Setup FAILED at step 9\n"); std::fflush(stderr);
        return -1;
    }
    std::fprintf(stderr, "[TRACE] GameLoop_Setup: InitAllSubsystems done\n"); std::fflush(stderr);

    /* Step 10: Hide second overlay, init DDRAW */
    std::fprintf(stderr, "[TRACE] GameLoop_Setup: calling DDRAW_Init...\n"); std::fflush(stderr);
    UIPANEL_Hide(g_second_overlay, &g_empty_string);

    if (!DDRAW_Init()) {
        std::fprintf(stderr, "[TRACE] GameLoop_Setup FAILED at step 10 (DDRAW_Init)\n"); std::fflush(stderr);
        return -1;
    }
    std::fprintf(stderr, "[TRACE] GameLoop_Setup: DDRAW_Init done\n"); std::fflush(stderr);

    /* Step 11: Create named event */
    std::fprintf(stderr, "[TRACE] GameLoop_Setup: creating named event...\n"); std::fflush(stderr);
    void* hEvent = CreateEventA(nullptr, 1, 0, S_GAMELOOP);
    DAT_004a990c = (int)(intptr_t)hEvent;
    if (!hEvent) {
        std::fprintf(stderr, "[TRACE] GameLoop_Setup FAILED at step 11\n"); std::fflush(stderr);
        return -1;
    }

    /* Step 12: Start multimedia timer (28ms period, 14ms resolution) */
    int period_result = timeBeginPeriod(14);
    if (period_result == 0) {
        g_timer_event_id = (void*)(intptr_t)timeSetEvent(
            28, 14, (void*)&LAB_0045c520, 0, 1);
    }

    return 0;
}


/* ================================================================== */
/* GameLoop_FrameUpdate - Per-frame game loop heartbeat                 */
/* Address: 0x45C3C0                                                    */
/*                                                                      */
/* Called by: WinMain message loop at 0x46322B, every frame             */
/*                                                                      */
/* Drives per-frame tick: netman, world, objects, buildings, tile cache */
/* ================================================================== */
extern "C" void GameLoop_FrameUpdate(void)
{
    /* Step 1: Clear timer-handled flag */
    DAT_00485444 = 0;

    /* Step 2: Update game time */
    CRT_timeGetTime();

    /* Step 3: Network update */
    if (g_netman) {
#ifndef _WIN32
        if (g_train && g_player_config) {
            lego_loco::network::PumpTransportIntoGame(
                static_cast<Netman*>(g_netman),
                static_cast<TrainSubsystem*>(g_train),
                g_player_config->name);
        }
#endif
        NETMAN_Update(g_netman);
    }

    /* Step 4: Vehicle animation tick */
    RESMGR_VehicleAnimationTick((void*)0x4A9910);

    /* Step 5: Mode check - skip menu modes (1,2) and screensaver (10) */
    int game_mode = g_game_mode;
    if (game_mode >= 1 && (game_mode <= 2 || game_mode == 10)) {
        return;
    }

    /* Step 6: Town/gameplay mode (3 or 9) - world tick */
    if (game_mode == 3 || game_mode == 9) {
        if (DAT_004ff124 == 1) {
            /* Pause transition: stop vehicles, tick, resume */
            if (DAT_004ff11c == 1 && DAT_004a98b4 != 0) {
                for (int i = 0; i < 4; i++) {
                    if (g_world_vehicles[i]) {
                        Vehicle_SetState(g_world_vehicles[i], 2);
                    }
                }
            }
            World_UpdateTick(g_world);
            DAT_004ff11c = 0;

            if (DAT_004a98b4 != 0) {
                for (int i = 0; i < 4; i++) {
                    if (g_world_vehicles[i]) {
                        Vehicle_SetState(g_world_vehicles[i], 0);
                    }
                }
            }
        } else {
            World_UpdateTick(g_world);
        }
    }

    /* Step 7: Hide tooltip */
    UI_HideTooltip(g_tooltip_mgr);

    /* Step 8: Game update (input, animation, selection) */
    ((Game*)g_game)->Update();

    /* Step 9: Scripted object update */
    RESDATA_ScriptedObject_Update(g_scripted_object);

    /* Step 10: Town mode updates */
    if (game_mode == 3 || game_mode == 9) {
        /* Host mode-3 cone: g_town_view and g_ddraw_building are now
         * constructed by BootstrapTownMode3Objects() during GameLoop_Setup.
         * Both the _WIN32 and non-_WIN32 paths call the real implementations. */
        Town_TrackBuilding(g_town_view);
        DDRAW_UpdateBuilding(g_ddraw_building);
        INPUT_GetSaveFileName(&g_input_mgr);
        BuildingMgr_UpdateAll(g_building_mgr);
    }

    /* Step 11: Flush dirty tile rects to screen */
#ifdef _WIN32
    TileMap_InvalidateDirtyRects(g_tilemap, 0);
#else
    /* g_tilemap is null until HostMode3Bootstrap constructs it on first
     * entry to mode 3; frames rendered before that (e.g. main menu) must
     * skip this instead of dereferencing a null TileMap*. */
    if (g_tilemap != nullptr) {
        TileMap_InvalidateDirtyRects(g_tilemap, 0);
    }
#endif
}
