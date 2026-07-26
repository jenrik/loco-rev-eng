/**
 * DirectPlay.h — DirectPlay network session wrapper for Lego Loco
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These C free functions wrap Microsoft DirectPlay for multiplayer.
 * They operate on a large session struct (~0x15E8+ bytes) allocated
 * during DirectPlay_constructor (0x443000).
 *
 * === Struct Layout (DirectPlay Session) ===
 *   +0x000: session_state (byte) - 0=inactive, 1=host, 2=joined
 *   +0x001: flag_byte_1 (byte) - is_host flag
 *   +0x002: flag_byte_2 (byte) - startup flag
 *   +0x003: flag_byte_3 (byte)
 *   +0x004: flag_byte_4 (byte)
 *   +0x018: session_name[0x400] (char) - session name string
 *   +0x418: player_name[0x80] (char) - local player name
 *   +0x498: session_password[0x80] (char) - session password
 *   +0x518: connection_type (int) - 0=IPX, 1=TCP/IP, 2=Modem, 3=Serial, 4=DirectModem
 *   +0x51C: connection_name[0x400] (char) - provider name buffer
 *   +0x920: session_flags (int)
 *   +0x924: player_dpid (int) - local player's DirectPlay ID
 *   +0x92C: session_desc_ptr (void*) - session description data
 *   +0x930: session_desc_valid (byte) - 0=allocated, 1=pending
 *   +0x934: flag_934 (byte)
 *   +0x938: hwnd (void*) - parent window handle
 *   +0x93C: hinstance (void*) - application instance
 *   +0x940: error_callback (void*) - error display callback
 *   +0x944: show_dialogs (byte) - 1=show MessageBox dialogs
 *   +0x945: error_msg_buf[512] (char) - error message buffer
 *   +0xD48: last_hresult (int) - last DirectPlay HRESULT
 *   +0xD4C: idle_callback (void(*)()) - periodic callback during waits
 *   +0xD50: session_ready (byte) - 1=enumeration complete
 *   +0xD54: session_data_size (int) - received session data size
 *   +0xD58: session_data_ptr (void*) - received session data
 *   +0xD5C: max_players (int) - maximum players setting
 *   +0xD60: session_list (void*) - linked list of session entries
 *   +0xD64: player_list (void*) - linked list of player entries
 *   +0xD68: group_list (void*) - linked list of group entries
 *   +0xD6C: connection_list (void*) - linked list of connections
 *   +0xD70: player_name_buf[0x100] (char) - enumerated player name
 *   +0x1570: modem_settings[5] (int) - modem configuration
 *   +0x1584: dplay_dll_handle (void*) - DirectPlay DLL handle
 *   +0x1588: dplay_interface (void*) - IDirectPlay4* interface pointer
 *   +0x158C: session_desc (0x50 bytes) - DP Session desc structure
 *   +0x15DC: dplay_address (void*) - IDirectPlayAddress*
 *   +0x15E0: dplay_address2 (void*) - IDirectPlayAddress2*
 *   +0x15E4: addr_struct (0x28 bytes) - DP Address structure
 *
 * === DP Session Desc Structure (0x50 bytes at +0x158C) ===
 *   +0x00: dwSize (int) - size of structure (0x50)
 *   +0x04: dwFlags (int) - session flags
 *   +0x08: guidInstance (GUID) - session instance GUID (16 bytes)
 *   +0x18: guidApplication (GUID) - application GUID (16 bytes)
 *   +0x28: dwMaxPlayers (int) - max players
 *   +0x2C: dwCurrentPlayers (int) - current players
 *   +0x30: dwSession (int) - session type flags
 *   +0x34: dwReserved1 (int) - reserved
 *   +0x38: lpszSessionName (char*) - session name ptr
 *   +0x3C: lpszPassword (char*) - password ptr
 *   +0x40: lpszReserved (char*) - reserved
 *   +0x44: reserved2 (int)
 *   +0x48: lpszPlayerName (char*) - player name ptr (reserved)
 *
 * All functions are C-linkage wrappers around DirectPlay COM interface.
 */

#pragma once

#include "../shared/types.h"

/* ================================================================== */
/* DirectPlay functions                                                */
/* ================================================================== */

/**
 * DirectPlay_SessionMgr — Walk 4-ary tree and store path.
 * Address: 0x45DA70, __thiscall
 *
 * Walks the 4-ary tree from startNode to targetId, storing IDs and
 * direction bytes into the AssetMgr's path/direction buffers.
 *
 * Called by: AssetMgr_GetFileInfo (0x45DD27)
 */
void __thiscall DirectPlay_SessionMgr(void* self, int32_t* startNode, int32_t targetId);

/**
 * DirectPlay_Init — Initialize DirectPlay/compositing.
 * Address: 0x45E090, __cdecl
 *
 * Plays a startup sound, fills the primary surface rect, creates a
 * shadow GameObject (res 0x402), positions it, and presents the first
 * frame. Called once from CGWND_InitMode1.
 *
 * Called by: CGWND_InitMode1 (0x4083DC)
 */
void __cdecl DirectPlay_Init(void);

/**
 * DirectPlay_CreatePeer — Create DirectPlay peer, setting address.
 * Address: 0x45E490, __thiscall
 *
 * Wrapper: calls CreateAddress with the two parameters, returns this.
 *
 * Called by: Train_InitNetwork (0x439205), Train_HandleJoinMultiplayer (0x43C550)
 */
void* __thiscall DirectPlay_CreatePeer(void* self, uint32_t param_1, uint32_t param_2);

/**
 * DirectPlay_CreateAddress — Initialize DirectPlay address and interface.
 * Address: 0x45E4B0, __thiscall
 *
 * Sets session fields, creates DirectPlayAddress via Ordinal_4,
 * queries IDirectPlayAddress interface, releases initial object.
 */
void __thiscall DirectPlay_CreateAddress(void* self, uint32_t param_1, uint32_t param_2);

/**
 * DirectPlay_DestroyPeer — Full DirectPlay peer teardown.
 * Address: 0x45E5A0, __fastcall
 *
 * Calls DirectPlay_Close, releases DirectPlay peer interface, frees
 * session data, and empties session/player/group/message linked lists.
 */
void __fastcall DirectPlay_DestroyPeer(int32_t param_1);

/**
 * DirectPlay_HostSession — Store host session configuration.
 * Address: 0x45E700, __thiscall
 *
 * Stores 4 configuration parameters for hosting a DirectPlay session.
 */
void __thiscall DirectPlay_HostSession(void* self, uint8_t param_1, uint32_t param_2,
                                        uint8_t param_3, uint8_t param_4);

/**
 * DirectPlay_ConnectToSession — Join a DirectPlay session.
 * Address: 0x45E730, __thiscall
 *
 * Stores player name and session name/password, creates session on
 * DirectPlay server, enumerates players. Returns 1 on success, 0 on failure.
 */
uint8_t __thiscall DirectPlay_ConnectToSession(void* self, const char* playerName,
                                                const char* sessionName,
                                                const char* password);

/**
 * DirectPlay_EnumConnections — Enumerate DirectPlay service providers.
 * Address: 0x45EAB0, __fastcall
 *
 * Returns the connection list head pointer. Creates DirectPlay object,
 * enumerates service providers (TCP/IP, IPX, Modem, Serial), checks
 * device availability via CreateFile, returns combined list.
 */
int32_t __fastcall DirectPlay_EnumConnections(int32_t param_1);

/**
 * DirectPlay_GetConnectionCaps — Check if connection device exists.
 * Address: 0x45EE60, __cdecl
 *
 * Attempts to open a device file and checks if the handle is valid.
 * Returns 0xFFFFFF00 on failure, 0x0100XX00 on success.
 */
uint32_t __cdecl DirectPlay_GetConnectionCaps(uint8_t* devicePath);

/**
 * DirectPlay_GetSessionDesc — Query session description from DirectPlay.
 * Address: 0x45EEC0, __fastcall
 *
 * Creates DirectPlay, queries session desc, allocates + locks global memory,
 * creates player enumeration callback, stores player name at +0xD70.
 * Returns true if a session was found.
 */
bool __fastcall DirectPlay_GetSessionDesc(int32_t param_1);

/**
 * DirectPlay_SetSessionDesc — Set session description and host.
 * Address: 0x45F090, __thiscall
 *
 * Stores session password, clears previous session/player lists, creates
 * session via DirectPlay CreateSession, retries on DPERR_PENDING status.
 * Returns session data pointer.
 */
uint32_t __thiscall DirectPlay_SetSessionDesc(void* self, const char* password);

/**
 * DirectPlay_HandleMessages — Create DirectPlay, enumerate providers.
 * Address: 0x45F390, __cdecl
 *
 * Main network setup state machine. Creates DirectPlay instance, enumerates
 * connections, initializes address. Large function (2076 bytes). Shows
 * connection dialog for interactive provider selection.
 *
 * Return value: 1 on success, 0 on failure
 */
uint32_t __cdecl DirectPlay_HandleMessages(void);

/**
 * DirectPlay_CreatePlayer — DirectPlay enum callback.
 * Address: 0x45FBD0, __cdecl (callback from DirectPlay EnumPlayers)
 *
 * Called by DirectPlay when enumerating session players. If player name
 * is empty ("") and param_3 is non-empty, copies param_3 into the
 * global player name buffer.
 *
 * @return 0 to continue enumeration, 1 to stop
 */
uint32_t __cdecl DirectPlay_CreatePlayer(const char* playerName, uint32_t param_2,
                                          const char* displayName);

/**
 * DirectPlay_Close — Close DirectPlay session and free resources.
 * Address: 0x45FC30, __fastcall
 *
 * Closes the DirectPlay session: releases session desc, closes player
 * handle, releases interface, frees session/group/message lists.
 */
void __fastcall DirectPlay_Close(int32_t param_1);

/**
 * DirectPlay_EnumPlayers — Enumerate players in a session.
 * Address: 0x45FD80, __fastcall
 *
 * Builds a session desc structure, calls IDirectPlay4::EnumPlayers,
 * retries on DPERR_PENDING. Returns 1 on success, 0x88770100 on
 * DP_OK_USERCANCEL, or error code on failure.
 */
uint32_t __fastcall DirectPlay_EnumPlayers(void* param_1);

/**
 * DirectPlay_Open — Convert DPERR_* HRESULT to string.
 * Address: 0x45FF30, __cdecl
 *
 * Converts DirectPlay error codes to human-readable strings via
 * wsprintfA. Large switch table over 40+ DPERR_* error codes.
 * Unknown codes format as "Unknown Error Code: %d".
 *
 * @param out_buf  Output string buffer
 * @param hresult  DirectPlay HRESULT error code
 */
void __cdecl DirectPlay_Open(char* out_buf, int32_t hresult);

/* ================================================================== */
/* Global state                                                       */
/* ================================================================== */

/* Forward declaration used by DirectPlay_CreatePlayer */
extern void* g_dplay_peer;  /* DirectPlay peer struct pointer */
