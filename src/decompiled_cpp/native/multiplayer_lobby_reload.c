/**
 * multiplayer_lobby_reload.c — Multiplayer lobby reload on demo exit
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * MultiplayerLobby_Reload is called from WinMain when exiting the demo
 * loop back to the multiplayer lobby (network game setup). It resets the
 * UI network state, re-initializes the network panel, resumes the network
 * thread, and re-initializes audio.
 *
 * C free function (void, no params, no return). Uses __cdecl convention.
 * Despite being called in a C++ context, this is a flat C function.
 */

#include "../shared/types.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void __cdecl EditWindow_InitNetworkPanel(void* ui_main); /* 0x422820 */
extern void __thiscall NETMAN_SetGameMode(void* netman, int mode); /* 0x43D2B0 */
extern void __stdcall WIN32_ResumeThread(void* thread, int timeout); /* 0x4616C0 */
extern void __cdecl DDRAW_InitAudio(void);                       /* 0x45B7E0 */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern void* g_ui_main;             /* 0x4FD378 — main menu UI (EditWindow) */
extern void* g_netman;              /* 0x4FD3AC — network manager */
extern void* g_net_host_info;       /* 0x4FD3A8 — network host info struct */
extern void* g_network_thread;      /* 0x4FD398 — network thread handle */

/* ================================================================== */
/* MultiplayerLobby_Reload                                             */
/* Address: 0x448350                                                    */
/*                                                                      */
/* Called when exiting demo mode back to the multiplayer lobby.         */
/* Re-enables UI network flag, re-inits the network panel, sets         */
/* the network polling interval to 50ms, sets game mode to 1 (hosting), */
/* resumes the network thread, and re-initializes audio.                */
/* ================================================================== */
void __cdecl MultiplayerLobby_Reload(void)
{
    /* Step 1: Re-enable UI network flag at g_net_host_info+0x07 */
    *(char*)((int)g_net_host_info + 7) = 1;

    /* Step 2: Re-initialize the network panel on the main menu UI */
    EditWindow_InitNetworkPanel(g_ui_main);

    /* Step 3: Set network polling to 50ms (0x32) */
    *(int*)((int)g_net_host_info + 0x0C) = 0x32;

    /* Step 4: Set game mode to 1 (hosting/lobby mode) */
    NETMAN_SetGameMode(g_netman, 1);

    /* Step 5: Resume the network thread with no timeout (-1) */
    WIN32_ResumeThread(g_network_thread, -1);

    /* Step 6: Re-initialize audio subsystem */
    DDRAW_InitAudio();
}
