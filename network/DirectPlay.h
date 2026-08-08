/**
 * DirectPlay.h — DirectPlay network session wrapper for Lego Loco
 * Status: TRANSCRIBED
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
 *   +0x1584: dplay_create_obj (void*) - transient IDirectPlay-family object
 *            returned by Ordinal_1; QueryInterface'd for IID_IDirectPlay4A
 *            into dplay_interface, then Released. NOT a DLL handle (verified
 *            via disassembly of DirectPlay_EnumConnections/GetSessionDesc:
 *            this slot is always dereferenced through a vtable, never
 *            passed to FreeLibrary).
 *   +0x1588: dplay_interface (void*) - IDirectPlay4A* interface pointer
 *   +0x158C: session_desc (0x50 bytes) - DPSESSIONDESC2 structure (ANSI
 *            layout: dwSize,dwFlags,guidInstance,guidApplication,
 *            dwMaxPlayers,dwCurrentPlayers,lpszSessionNameA,lpszPasswordA,
 *            dwReserved1,dwReserved2,dwUser1-4 — see stubs/dplay.h)
 *   +0x15DC: dplay_lobby_obj (void*) - transient object from Ordinal_4,
 *            QueryInterface'd for IID_IDirectPlayLobby3A into
 *            dplay_lobby3a, then Released. NOT an "IDirectPlayAddress"
 *            (that interface does not exist in classic DirectPlay).
 *   +0x15E0: dplay_lobby3a (void*) - IDirectPlayLobby3A* interface pointer,
 *            used for EnumAddress-style calls (vtable slot 5 / +0x14).
 *   +0x15E4: session_caps (0x28 bytes) - DPCAPS structure (dwSize=0x28,
 *            10 dwords; confirmed via disassembly of
 *            DirectPlay_ConnectToSession/0x45EA84-0x45EA90, which zeroes
 *            10 dwords, sets dwSize=0x28, and calls vtable[14]/+0x38 =
 *            IDirectPlay4A::GetCaps). NOT a "DP Address structure".
 *
 * === Known GUIDs from binary ===
 *   IID_IDirectPlay4A     @ 0x478F88: {0AB1C531-4745-11D1-A7A1-0000F803ABFC}
 *     (byte-verified; used as the QueryInterface argument in
 *      DirectPlay_GetSessionDesc/EnumConnections — NOT a CLSID, despite
 *      the misleading `CLSID_DirectPlay` symbol name still used in
 *      DirectPlay.cpp for this value)
 *   CLSID_DirectPlay      @ 0x478F98: {D1EB6D20-8923-11D0-9D97-00A0C90A43CB}
 *     (the real CLSID, immediately following IID_IDirectPlay4A in the
 *      binary's GUID table; referenced only from the still-deferred body
 *      of DirectPlay_HandleMessages — not yet used by any reconstructed
 *      C++ in this file. See PROGRESS.md TODO.)
 *   IID_IDirectPlayLobby3A @ 0x479048: {2DB72491-652C-11D1-A7A8-0000F803ABFC}
 *     (byte-verified; mislabeled "IID_IDirectPlayAddr2" in earlier
 *      revisions of this file — no such interface exists in classic
 *      DirectPlay. Confirmed via disassembly: DirectPlay_CreateAddress
 *      QueryInterfaces an Ordinal_4 object for this IID, matching
 *      IDirectPlayLobby3A exactly.)
 *   DP Session GUID (guidApplication) @ 0x479158: {F9CD2546-577F-11D2-9426-00A0244BDA7A}
 *     (game-specific application GUID, not a standard SDK constant —
 *      correctly documented already)
 *
 * All functions are C-linkage wrappers around DirectPlay COM interfaces.
 *
 * NOTE: Several helper-function names and vtable offsets in DirectPlay.cpp
 * do not match the real IDirectPlay4/IDirectPlay4A vtable order (cross-
 * checked against the genuine DirectX 6.0 SDK (Aug 1998) dplay.h — see
 * NOTE-directx-sdk.md, also mirrored by https://github.com/Olde-Skuul/directplay.
 * See PROGRESS.md for the full
 * corrected slot table and the specific confirmed mismatches — this is
 * flagged but intentionally NOT fixed here pending a dedicated VALIDATED
 * pass, since several of them change which real DirectPlay operation is
 * being performed, not just its name.
 *
 * NOTE: This file uses free functions with explicit 'void* self' parameter
 * per the binary's __thiscall convention. These will be converted to C++ class
 * methods during the INTEGRATED pass. The __thiscall/__fastcall annotations
 * have been removed per AGENTS.md anti-pattern rule 5.
 */

#pragma once

#include "../shared/types.h"

/* ================================================================== */
/* DirectPlay functions                                                */
/* ================================================================== */

/**
 * DirectPlay_SessionMgr — Walk 4-ary tree and store path.
 * Address: 0x45DA70
 *
 * Walks the 4-ary tree from startNode to targetId, storing IDs and
 * direction bytes into the AssetMgr's path/direction buffers.
 *
 * NOTE: Implemented in AssetMgr.cpp:805 per binary layout.
 * Called by: AssetMgr_GetFileInfo (0x45DD27)
 */
void DirectPlay_SessionMgr(void* self, int32_t* startNode, int32_t targetId);

/**
 * DirectPlay_Init — Initialize DirectPlay/compositing.
 * Address: 0x45E090
 *
 * Plays a startup sound, fills the primary surface rect, creates a
 * shadow GameObject (res 0x402), positions it, and presents the first
 * frame. Called once from CGWND_InitMode1.
 *
 * Called by: CGWND_InitMode1 (0x4083DC)
 */
void DirectPlay_Init(void);

/**
 * DirectPlay_CreatePeer — Create DirectPlay peer, setting address.
 * Address: 0x45E490
 *
 * Wrapper: calls CreateAddress with the two parameters, returns this.
 *
 * Called by: Train_InitNetwork (0x439205), Train_HandleJoinMultiplayer (0x43C550)
 */
void* DirectPlay_CreatePeer(void* self, uint32_t hInstance, uint32_t hWnd);

/**
 * DirectPlay_CreateAddress — Initialize DirectPlay address and interface.
 * Address: 0x45E4B0
 *
 * Sets session fields, creates DirectPlayAddress via Ordinal_4,
 * queries IDirectPlayAddress interface, releases initial object.
 */
void DirectPlay_CreateAddress(void* self, uint32_t hInstance, uint32_t hWnd);

/**
 * DirectPlay_DestroyPeer — Full DirectPlay peer teardown.
 * Address: 0x45E5A0
 *
 * Calls DirectPlay_Close, releases DirectPlay peer interface, frees
 * session data, and empties session/player/group/message linked lists.
 */
void DirectPlay_DestroyPeer(int32_t session);

/**
 * DirectPlay_HostSession — Store host session configuration.
 * Address: 0x45E700
 *
 * Stores 4 configuration parameters for hosting a DirectPlay session.
 */
void DirectPlay_HostSession(void* self, uint8_t sessionState, uint32_t sessionFlags,
                             uint8_t isHost, uint8_t startupFlag);

/**
 * DirectPlay_ConnectToSession — Join a DirectPlay session.
 * Address: 0x45E730
 *
 * Stores player name and session name/password, creates session on
 * DirectPlay server, enumerates players. Returns 1 on success, 0 on failure.
 */
uint8_t DirectPlay_ConnectToSession(void* self, const char* playerName,
                                     const char* sessionName,
                                     const char* password);

/**
 * DirectPlay_EnumConnections — Enumerate DirectPlay service providers.
 * Address: 0x45EAB0
 *
 * Returns the connection list head pointer. Creates DirectPlay object,
 * enumerates service providers (TCP/IP, IPX, Modem, Serial), checks
 * device availability via CreateFile, returns combined list.
 */
int32_t DirectPlay_EnumConnections(int32_t session);

/**
 * DirectPlay_GetConnectionCaps — Check if connection device exists.
 * Address: 0x45EE60
 *
 * Attempts to open a device file and checks if the handle is valid.
 * Returns 0xFFFFFF00 on failure, 0x0100XX00 on success.
 */
uint32_t DirectPlay_GetConnectionCaps(uint8_t* devicePath);

/**
 * DirectPlay_GetSessionDesc — Query session description from DirectPlay.
 * Address: 0x45EEC0
 *
 * Creates DirectPlay, queries session desc, allocates + locks global memory,
 * creates player enumeration callback, stores player name at +0xD70.
 * Returns true if a session was found.
 */
bool DirectPlay_GetSessionDesc(int32_t session);

/**
 * DirectPlay_SetSessionDesc — Set session description and host.
 * Address: 0x45F090
 *
 * Stores session password, clears previous session/player lists, creates
 * session via DirectPlay CreateSession, retries on DPERR_PENDING status.
 * Returns session data pointer.
 */
uint32_t DirectPlay_SetSessionDesc(void* self, const char* password);

/**
 * DirectPlay_HandleMessages — Create DirectPlay, enumerate providers.
 * Address: 0x45F390
 *
 * Main network setup state machine. Creates DirectPlay instance, enumerates
 * connections, initializes address. Large function (~2076 bytes). Shows
 * connection dialog for interactive provider selection.
 *
 * Return value: 1 on success, 0 on failure
 *
 * NOTE: This is a __cdecl function in the binary but takes no explicit
 * 'this' — it accesses g_dplay_peer directly as a global.
 *
 * Takes 3 stack args the DB previously hid behind a void(void) signature
 * (the decompiler drops arguments it doesn't believe exist). Its own two
 * internal call sites (0x45E88C/0x45E88E and 0x45E987/0x45E989, inside
 * DirectPlay_ConnectToSession) push (0, 0, 0). game/Train_network.cpp's
 * Train_ConnectToServer (0x43C8EE-0x43C8F2, 0x43C98D-0x43C991) pushes a
 * real (protocol, address-string, 0) override when message 0x3EB directs
 * a reconnect to a specific server. The ~2076-byte body itself remains a
 * deferred TODO; this signature fix only makes both call sites pass the
 * real arguments instead of silently dropping them.
 */
uint32_t DirectPlay_HandleMessages(int32_t protocol, const char* address, int32_t flags);

/**
 * DirectPlay_CreatePlayer — DirectPlay enum callback.
 * Address: 0x45FBD0
 *
 * Called by DirectPlay when enumerating session players. If player name
 * is empty ("") and displayName is non-empty, copies displayName into the
 * global player name buffer at g_dplay_peer+0xD70.
 *
 * @return 0 to continue enumeration, 1 to stop
 */
uint32_t DirectPlay_CreatePlayer(const char* playerName, uint32_t flags,
                                  const char* displayName);

/**
 * DirectPlay_Close — Close DirectPlay session and free resources.
 * Address: 0x45FC30
 *
 * Closes the DirectPlay session: releases session desc, closes player
 * handle, releases interface, frees session/group/message lists.
 */
void DirectPlay_Close(int32_t session);

/**
 * DirectPlay_EnumPlayers — Enumerate players in a session.
 * Address: 0x45FD80
 *
 * Builds a session desc structure, calls IDirectPlay4::EnumPlayers,
 * retries on DPERR_PENDING. Returns 1 on success, 0x88770100 on
 * DP_OK_USERCANCEL, or error code on failure.
 */
uint32_t DirectPlay_EnumPlayers(void* self);

/**
 * DirectPlay_Open — Convert DPERR_* HRESULT to string.
 * Address: 0x45FF30
 *
 * Converts DirectPlay error codes to human-readable strings via
 * wsprintfA. Large switch table over 40+ DPERR_* error codes.
 * Unknown codes format as "Unknown Error Code: %d".
 *
 * @param outBuf   Output string buffer
 * @param hresult  DirectPlay HRESULT error code
 */
void DirectPlay_Open(char* outBuf, int32_t hresult);

/* ================================================================== */
/* Global state                                                       */
/* ================================================================== */

/* Forward declaration used by DirectPlay_CreatePlayer */
extern void* g_dplay_peer;  /* DirectPlay peer struct pointer */