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
#include "../game/GameConfig.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Canonical declaration: resources/ResourceManager.h (matching exactly). */
extern void __cdecl MultiplayerLobby_Reload(void);

extern void __cdecl EditWindow_InitNetworkPanel(void* ui_main); /* 0x422820 */
extern void __thiscall NETMAN_SetGameMode(void* netman, int mode); /* 0x43D2B0 */
extern void __stdcall WIN32_ResumeThread(void* thread, int timeout); /* 0x4616C0 */
extern void __cdecl DDRAW_InitAudio(void);                       /* 0x45B7E0 */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern void* g_ui_main;             /* 0x4FD378 — main menu UI (EditWindow) */
extern void* g_netman;              /* 0x4FD3AC — network manager */
extern void* g_network_thread;      /* 0x4FD398 — network thread handle */

/* GameConfig singleton (0x4FD3A8). Previously accessed here as
 * `g_net_host_info` (a bare `void*` alias of the same storage, declared in
 * network/Netman.h) via raw `+7`/`+0x0C` byte offsets. Host GameConfig has
 * no vtable (its destructor isn't virtual), so its host memory layout does
 * NOT match the original x86 offsets used by raw pointer arithmetic —
 * network/Netman.cpp already made this exact substitution (typed
 * `_g_netman_data` + named fields) for the same 0x4FD3A8 object; this file
 * now matches that precedent instead of `g_net_host_info`. */
extern GameConfig* _g_netman_data;  /* 0x4FD3A8 */

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
    /* Step 1: Re-enable UI network flag (GameConfig::m_autoStart, +0x07) */
    _g_netman_data->m_autoStart = 1;

    /* Step 2: Re-initialize the network panel on the main menu UI */
    EditWindow_InitNetworkPanel(g_ui_main);

    /* Step 3: Set network polling to 50ms (0x32). Original offset +0x0C is
     * GameConfig::m_timeout; this call site repurposes it as a poll
     * interval rather than the session timeout documented in
     * GameConfig.h — same dual-use field, different caller's intent. */
    _g_netman_data->m_timeout = 0x32;

    /* Step 4: Set game mode to 1 (hosting/lobby mode) */
    NETMAN_SetGameMode(g_netman, 1);

    /* Step 5: Resume the network thread with no timeout (-1) */
    WIN32_ResumeThread(g_network_thread, -1);

    /* Step 6: Re-initialize audio subsystem */
    DDRAW_InitAudio();
}
