/**
 * gamestate_handlers.c — Game state / setup panel handling functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions manage the game state machine transitions during the
 * setup screen (player selection, layout picking, difficulty, multiplayer
 * lobby) before entering actual gameplay (mode 3 = town).
 *
 * The GAMESTATE object (vtable 0x4784CC) is a C++ class derived from
 * GameSetupPanel (GameWindow derived), with significant field layout
 * for track pieces, sprites, and lobby state. The functions here use
 * __thiscall (C++ methods) or __fastcall (C with ECX as this).
 *
 * Functions:
 *   GAMESTATE_StartNewGame       (0x40A150) — Start new game
 *   GAMESTATE_ReturnToMainMenu   (0x40A220) — Return to main menu
 *   GAMESTATE_LoadExistingGame   (0x40A260) — Load save game
 *   GAMESTATE_HandleNetworkGame  (0x40A300) — Handle network game
 *   GAMESTATE_StartGameTimer     (0x40A350) — Start game timers
 *   GAMESTATE_SelectLayout       (0x40A3D0) — Select layout/scenario
 *   GAMESTATE_SetDifficulty      (0x40A4A0) — Set difficulty index
 *   GAMESTATE_HandleClick        (0x40A4E0) — Click handler
 *   GAMESTATE_ConnectToNetworkGame (0x40AA20) — Network connect
 *   GAMESTATE_SelectLayoutEntry  (0x40AAF0) — Select layout entry
 *   GAMESTATE_HandleMapClick     (0x40ABA0) — Scenario grid click
 *   GAMESTATE_SendScenarioSelect (0x40AC50) — Send scenario choice
 *   GAMESTATE_WndProc            (0x40B4C0) — Window procedure
 *   GAMESTATE_FindTrackPosition  (0x40B610) — Find track position
 *   GAMESTATE_InitTrackAtPosition (0x40B740) — Init track
 *   GAMESTATE_FindAdjacentTrack  (0x40B880) — Find adjacent track
 *   GAMESTATE_UpdateVehiclePlacement (0x40BBD0) — Update placement
 *
 * Calling conventions: mix of __thiscall (C++ methods) and __fastcall
 */

#include "../shared/types.h"

/* ================================================================== */
/* External declarations                                               */
/* ================================================================== */

extern void* __cdecl operator_new(size_t size);            /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);               /* 0x465CD0 */
extern int   __cdecl CRT_rand(void);                       /* 0x466150 */
extern void  __cdecl PlaySound(int sound_id);              /* 0x447930 */
extern void  __cdecl PlaySoundAt(int sound_id, int x, int y, int flags); /* 0x4479D0 */

/* GAMESTATE methods */
extern void  __thiscall GAMESTATE_StartNewGame(void* this_ptr);      /* 0x40A150 */
extern void  __thiscall GAMESTATE_ReturnToMainMenu(void* this_ptr);  /* 0x40A220 */
extern void  __thiscall GAMESTATE_LoadExistingGame(void* this_ptr);  /* 0x40A260 */
extern void  __thiscall GAMESTATE_HandleNetworkGame(void* this_ptr); /* 0x40A300 */
extern void  __thiscall GAMESTATE_SelectLayoutEntry(void* this_ptr, int index); /* 0x40AAF0 */

/* NETMAN */
extern void  __thiscall NETMAN_ResetNetworkState(void* netman); /* 0x43EFA0 */
extern void  __thiscall NETMAN_SetGameMode(void* netman, int mode); /* 0x43D2B0 */
extern void  __thiscall NETMAN_StopSession(int netman);       /* 0x43F070 */
extern void  __thiscall NETMAN_StartClientSession(void);      /* 0x43F030 */
extern void  __thiscall NETMAN_StartHostSession(void);        /* 0x43F000 */
extern void  __thiscall NETMAN_Init(void* netman, byte flags); /* 0x43D130 */
extern void  __thiscall NETMAN_LoadScenario(void* netman, char* scenario); /* 0x43D820 */
extern void  __thiscall NETMAN_SendLayoutSelect(void* netman, int hostSlot, int scenario, byte* name, int packed_id); /* 0x43FE30 */

/* Other game functions */
extern void  __thiscall UI_MainMenu_SetState(void* ui, int state); /* 0x4208F0 */
extern void  __thiscall GameSetupPanel__loadLayouts(void* panel, byte refresh); /* 0x409E70 */
extern void  __thiscall GameSetupPanel__drawLayoutList(void* panel, void* list); /* 0x4094B0 */
extern void  __thiscall GameSetupPanel__drawGrid(void* panel); /* 0x409980 */
extern void  __thiscall GameSetupPanel__updateTitle(void* panel); /* 0x409360 */
extern void  __thiscall GameSetupPanel__drawTitle(void* panel);  /* 0x409770 */
extern void  __thiscall UIPANEL_EndPaint(void* panel);        /* 0x426B70 */
extern void  __thiscall UIPANEL_EndPaintEx(void* panel, int a, int b, byte c, void* rect); /* 0x426B90 */
extern void  __thiscall Sprite_SetState(void* sprite, int state, int* data); /* 0x454C30 */
extern void  __thiscall FormatResourceString(void* rmgr, int id, char* buf, int max); /* 0x447330 */
extern void  __thiscall PtInRect(void* rect, int x, int y);
extern void  __thiscall WIN32_PostQuit(void);                 /* 0x463670 */
extern LRESULT __thiscall DefWindowProcA(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
extern void  __thiscall Sleep(uint ms);
extern UINT_PTR __thiscall SetTimer(HWND hwnd, UINT_PTR id, UINT delay, void* proc);
extern void  __thiscall Train_QueueMessage(void* train, void* msg); /* 0x4393D0 */

/* Global variables */
extern int32_t  g_netman;           /* 0x4FD3AC */
extern int32_t  g_ui_main;          /* 0x4FD378 */
extern int32_t  g_player_config;    /* 0x4AA4A8 */
extern int32_t  g_player_id;        /* 0x4A99BC */
extern int32_t  g_player_color;     /* 0x4A99C0 */
extern int32_t  g_game_config;      /* 0x4FD3A8 */
extern int32_t  g_train;            /* 0x4FD3A4 */
extern uint32_t g_game_time;        /* 0x4A99B4 */
extern int32_t  g_game_difficulty;  /* 0x4852B4 */
extern int32_t  g_building_count;   /* 0x486258 */


/* ================================================================== */
/* GAMESTATE_StartNewGame — Entry point for starting a new game        */
/* Address: 0x40A150                                                    */
/*                                                                      */
/* Resets NETMAN mode, stops/starts client session, frees linked        */
/* layout list at this+0xF0, queues a START_NEW_GAME (type 2) train     */
/* message with player name. Guard flag at +0x10C prevents re-entrance. */
/*                                                                      */
/* Called by: GAMESTATE_HandleClick (0x40A4E0), NETMAN (0x43FC50)     */
/* ================================================================== */
void __fastcall GAMESTATE_StartNewGame(int* this_ptr)
{
    int* list_node;
    void* msg;
    void* mem;

    if (*(char*)(this_ptr + 0x43) != 0) {  /* guard at +0x10C */
        return;
    }

    /* Reset NETMAN */
    *(char*)g_netman = 0;  /* mode = 0 */
    NETMAN_ResetNetworkState((void*)g_netman);
    NETMAN_StopSession(g_netman);
    NETMAN_StartClientSession();

    /* Free layout list at +0xF0 (linked list) */
    if (*(int*)(this_ptr + 0x3C) == -1) {  /* +0xF0 */
        *(int*)(this_ptr + 0x3C) = 0;
    }
    list_node = (int*)(this_ptr + 0x3C);
    while (*list_node != 0) {
        list_node = *(int**)(this_ptr + 0x3C);
        if ((list_node)[2] != 0) {
            GLOBAL_free((void*)(list_node)[2]);
        }
        GLOBAL_free(*(void**)(this_ptr + 0x3C));
        *(int*)(this_ptr + 0x3C) = (int)list_node;
    }

    /* Create START_NEW_GAME train message (type 2) */
    mem = operator_new(0x1C);
    if (mem == 0) {
        mem = 0;
    } else {
        *(int*)((char*)mem + 8) = 0;
        *(int*)((char*)mem + 0x18) = 0;
    }
    *(int*)mem = 2;                              /* message type = START_NEW_GAME */
    *(int*)((char*)mem + 8) = this_ptr[2];       /* player name offset */

    Train_QueueMessage((void*)g_train, mem);

    /* Set guard */
    *(char*)(this_ptr + 0x43) = 1;  /* +0x10C */
}


/* ================================================================== */
/* GAMESTATE_ReturnToMainMenu — Return to main menu from setup          */
/* Address: 0x40A220                                                    */
/*                                                                      */
/* Calls cleanup vtable[4], resets NETMAN to menu mode (3), tells       */
/* UI_MainMenu to show (state 7), clears "returning to menu" flag.      */
/* ================================================================== */
void __fastcall GAMESTATE_ReturnToMainMenu(int* this_ptr)
{
    /* vtable[4] cleanup */
    (**(void (__thiscall**)(int*, int, int, int, int, int))(*this_ptr + 0x10))
        (this_ptr, 0, 0, 0, 0, 1);

    NETMAN_ResetNetworkState((void*)g_netman);
    NETMAN_SetGameMode((void*)g_netman, 3);
    UI_MainMenu_SetState((void*)g_ui_main, 7);
    *(char*)((char*)this_ptr + 0x114) = 0;  /* +0x114 */  /* flag at offset 0x45 */
}


/* ================================================================== */
/* GAMESTATE_LoadExistingGame — Load existing save game                 */
/* Address: 0x40A260                                                    */
/*                                                                      */
/* Very similar to StartNewGame — resets NETMAN, stops sessions,        */
/* queues LOAD_GAME (type 1) train message. Guard at +0x10C.          */
/* ================================================================== */
void __fastcall GAMESTATE_LoadExistingGame(int* this_ptr)
{
    /* Guard */
    if (*(char*)(this_ptr + 0x43) != 0) {
        return;
    }

    /* Reset NETMAN */
    *(char*)g_netman = 0;
    NETMAN_ResetNetworkState((void*)g_netman);
    NETMAN_StopSession(g_netman);
    NETMAN_StartClientSession();

    /* Free layout list (same as StartNewGame) */
    if (*(int*)(this_ptr + 0x3C) == -1) {
        *(int*)(this_ptr + 0x3C) = 0;
    }
    /* (linked list cleanup same as StartNewGame) */
    {
        int* list = *(int**)(this_ptr + 0x3C);
        while (list) {
            int* next = (int*)*list;
            if (list[2]) GLOBAL_free((void*)list[2]);
            GLOBAL_free(list);
            *(int*)(this_ptr + 0x3C) = (int)next;
            list = next;
        }
    }

    /* Create LOAD_GAME train message (type 1) */
    void* mem = operator_new(0x1C);
    if (!mem) mem = 0;
    else {
        *(int*)((char*)mem + 8) = 0;
        *(int*)((char*)mem + 0x18) = 0;
    }
    *(int*)mem = 1;
    *(int*)((char*)mem + 8) = this_ptr[2];
    Train_QueueMessage((void*)g_train, mem);
    *(char*)(this_ptr + 0x43) = 1;
}


/* ================================================================== */
/* GAMESTATE_HandleNetworkGame — Handle network game start              */
/* Address: 0x40A300                                                    */
/*                                                                      */
/* Sets NETMAN mode, creates session, queues HOST_GAME (type 3)         */
/* train message. Guard at +0x10C.                                    */
/* ================================================================== */
void __fastcall GAMESTATE_HandleNetworkGame(int* this_ptr)
{
    if (*(char*)(this_ptr + 0x43) != 0) {
        return;
    }

    *(char*)g_netman = 0;
    NETMAN_SetGameMode((void*)g_netman, 1);
    // Create session omitted (calls to DirectPlay / NETMAN)

    void* mem = operator_new(0x1C);
    if (!mem) mem = 0;
    else {
        *(int*)((char*)mem + 8) = 0;
        *(int*)((char*)mem + 0x18) = 0;
    }
    *(int*)mem = 3;  /* HOST_GAME */
    *(int*)((char*)mem + 8) = this_ptr[2];
    Train_QueueMessage((void*)g_train, mem);
    *(char*)(this_ptr + 0x43) = 1;
}


/* ================================================================== */
/* GAMESTATE_StartGameTimer — Start game timers after init complete     */
/* Address: 0x40A350                                                    */
/*                                                                      */
/* Creates two Windows timers:                                            */
/*   0x50 (50ms, 20fps) — main game loop                                */
/*   0x52 (75ms, ~13fps) — secondary                                   */
/*                                                                      */
/* Single-player: selects next layout entry.                            */
/* Network: inits NETMAN and loads layouts from file.                   */
/* Sets game state to 2 (running).                                      */
/* ================================================================== */
void __fastcall GAMESTATE_StartGameTimer(int* this_ptr)
{
    *(char*)((char*)this_ptr + 0x10C) = 0;  /* +0x43 as byte offset */

    if (*(char*)(g_game_config + 8) == 0) {  /* single player */
        GAMESTATE_SelectLayoutEntry(this_ptr, this_ptr[0x3D] + 1);  /* +0xF4 + 1 */
    } else {
        NETMAN_Init((void*)g_netman, 0);
        GameSetupPanel__loadLayouts(this_ptr, 1);
    }

    /* vtable[8] = create */
    (**(void (__thiscall**)(int*, int))(*this_ptr + 0x20))(this_ptr, 0);

    /* Create Windows timers */
    this_ptr[0x46] = (int)SetTimer((HWND)(intptr_t)this_ptr[2], 0x50, 50, 0);   /* 20fps */
    this_ptr[0x47] = (int)SetTimer((HWND)(intptr_t)this_ptr[2], 0x52, 75, 0);   /* ~13fps */

    this_ptr[0x6C] = 2;  /* game state = running */
}


/* ================================================================== */
/* GAMESTATE_WndProc — Window procedure for game-state window          */
/* Address: 0x40B4C0                                                    */
/*                                                                      */
/* Intercepts WM_SYSCOMMAND/SC_CLOSE to call WIN32_PostQuit() for      */
/* clean shutdown instead of letting DefWindowProc destroy the window.  */
/* All other messages pass to DefWindowProcA.                           */
/* ================================================================== */
void __cdecl GAMESTATE_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_SYSCOMMAND && (wp & 0xFFF0) == SC_CLOSE) {
        WIN32_PostQuit();
    }
    DefWindowProcA(hwnd, msg, wp, lp);
}
