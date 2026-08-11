/**
 * DirectPlay.h — DirectPlay network session wrapper for Lego Loco
 * Status: TRANSCRIBED
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * `DirectPlaySession` is the canonical class wrapping Microsoft DirectPlay
 * for multiplayer. Ten of its original free-function-with-explicit-`self`
 * methods (the CLAUDE.md free-function-with-self anti-pattern) are real
 * `DirectPlaySession::` methods below. `g_dplay_peer` is typed
 * `DirectPlaySession*` (2026-08-10 — previously `void*`, split-declared
 * across this header, game/Train_network.cpp, town/Town.cpp, and
 * shared/stubs_impl.cpp, all retyped together in the same change) and both
 * of its real callers (game/Train_network.cpp, town/Town.cpp) call these
 * methods directly — see docs/landmine-sweep-worklist.md's "DirectPlay_*
 * cluster" entry for the full history of why this was previously blocked
 * (mangled-name-coincidence risk from retyping a *free function*'s
 * parameter) and how calling real methods instead sidesteps that risk
 * entirely (no free-function declaration is retyped; the old free
 * functions are simply gone).
 *
 * === DirectPlaySession field layout (original x86 offsets, documentation
 *     only — see CLAUDE.md: exact byte-offset/pointer-width parity is a
 *     non-goal off-Windows, so the class below uses natural host field
 *     widths, not manual x86 packing) ===
 *   +0x000: session_state (byte) - 0=inactive, 1=host, 2=joined
 *   +0x001: is_host (byte) - set via HostSession's isHost parameter
 *   +0x002: startup_flag (byte) - set via HostSession's startupFlag parameter
 *   +0x003: flag_byte_3 (byte) - never touched by any of this class's methods
 *   +0x004: flag_byte_4 (byte) - never touched by any of this class's methods
 *   +0x018: session_name[0x400] (char) - session name string
 *   +0x418: player_name[0x80] (char) - local player name
 *   +0x498: session_password[0x80] (char) - session password
 *   +0x518: connection_type (int) - 0=unset/invalid (HandleMessages's
 *           dispatch switch bails without creating a provider), 1=Modem,
 *           2=TCP/IP, 3=Serial, 4=IPX. Confirmed 2026-08-11 by reading the
 *           raw DPSPGUID_* bytes at 0x478fa8-0x478fe7 (IPX/TCPIP/SERIAL/MODEM
 *           in that address order) and cross-referencing DirectPlay_
 *           HandleMessages's (0x45F390) connection_type-1 switch, whose
 *           four cases select GUID sub-fields from 0x478fd8 (case value 1),
 *           0x478fb8 (2), 0x478fc8 (3), 0x478fa8 (4) respectively — see
 *           network/DirectPlay.cpp's DirectPlay_ChooseConnectionDlgProc and
 *           DirectPlay_HandleMessages comments. This replaces a prior,
 *           unverified "0=IPX, 1=TCP/IP, 2=Modem, 3=Serial, 4=DirectModem"
 *           comment that both misordered the values and invented a
 *           "DirectModem" connection type that does not appear anywhere in
 *           the binary.
 *   +0x51C: connection_name[0x400] (char) - provider name buffer
 *   +0x91C: modem_baud (int16_t, stored widened) - never touched by this class's methods
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
 *   +0xD60: session_list (node*) - linked list of session entries
 *   +0xD64: player_list (node*) - linked list of player entries
 *   +0xD68: group_list (node*) - linked list of group entries
 *   +0xD6C: connection_list (node*) - linked list of connections
 *   +0xD70: player_name_buf[0x100] (char) - enumerated player/modem name
 *   +0x1570: modem_settings[5] (int) - modem configuration
 *   +0x1584: dplay_create_obj (IDirectPlay4A*) - transient object returned
 *            by Ordinal_1, QueryInterface'd for IID_IDirectPlay4A into
 *            dplay_interface, then Released.
 *   +0x1588: dplay_interface (IDirectPlay4A*) - the active session interface
 *   +0x158C: session_desc (DPSESSIONDESC2, 0x50 bytes on x86)
 *   +0x15DC: dplay_lobby_obj (IDirectPlayLobby3A*) - transient object from
 *            Ordinal_4, QueryInterface'd for IID_IDirectPlayLobby3A into
 *            dplay_lobby3a, then Released.
 *   +0x15E0: dplay_lobby3a (IDirectPlayLobby3A*) - used for EnumAddress
 *   +0x15E4: session_caps (DPCAPS, 0x28 bytes on x86)
 *
 * === Known GUIDs from binary ===
 *   IID_IDirectPlay4A     @ 0x478F88: {0AB1C531-4745-11D1-A7A1-0000F803ABFC}
 *   CLSID_DirectPlay      @ 0x478F98: {D1EB6D20-8923-11D0-9D97-00A0C90A43CB}
 *     (the real CLSID, referenced only from the still-deferred body of
 *      DirectPlay_HandleMessages — not yet used by any reconstructed C++
 *      in this file. See PROGRESS.md TODO.)
 *   IID_IDirectPlayLobby3A @ 0x479048: {2DB72491-652C-11D1-A7A8-0000F803ABFC}
 *   DP Session GUID (guidApplication) @ 0x479158: {F9CD2546-577F-11D2-9426-00A0244BDA7A}
 *   DPSPGUID_IPX/TCPIP/SERIAL/MODEM @ 0x478fa8-0x478fe7: byte-verified
 *     service-provider GUIDs used by EnumConnections/FindLocalModemName's
 *     Ordinal_1 calls — see network/DirectPlay.cpp.
 *
 * NOTE: Several helper-function names and vtable offsets in DirectPlay.cpp
 * do not match the real IDirectPlay4/IDirectPlay4A vtable order (cross-
 * checked against the genuine DirectX 6.0 SDK (Aug 1998) dplay.h — see
 * NOTE-directx-sdk.md, also mirrored by https://github.com/Olde-Skuul/directplay.
 * See PROGRESS.md for the full corrected slot table and the specific
 * confirmed mismatches — this is flagged but intentionally NOT fixed here
 * pending a dedicated VALIDATED pass, since several of them change which
 * real DirectPlay operation is being performed, not just its name.
 *
 * NOTE (2026-08-10): `DirectPlay_CreatePeer` (0x45E490) is NOT the class's
 * constructor. Ghidra confirms it's a plain __thiscall wrapper (calls
 * CreateAddress, returns `this`, no vtable write) on caller-allocated
 * memory — its two real callers (Train_InitNetwork, Train_HandleJoinMultiplayer)
 * both `operator_new` the session first. A stale comment in a prior
 * revision of this file claimed the real constructor was at 0x443000;
 * Ghidra shows that address is `NetworkPlayerList::NetworkPlayerList_ctor`,
 * an unrelated class — bogus address annotation, corrected here.
 */

#pragma once

#include "../shared/types.h"
#include "../stubs/dplay.h"

/* ================================================================== */
/* Linked-list node shapes                                             */
/*                                                                     */
/* Confirmed via disassembly of DestroyPeer/Close/SetSessionDesc's     */
/* list-traversal loops (network/DirectPlay.cpp). Three distinct node  */
/* shapes are walked by this class; none are packed to their x86 byte  */
/* sizes (0xC/0xC/0x8) since nothing in this file needs pointer-width  */
/* parity with the original allocator on a host build (CLAUDE.md).     */
/* ================================================================== */

/* session_list (+0xD60) / group_list (+0xD68) node shape: next, an
 * unread reserved slot (never accessed by this file's own code — these
 * two lists are populated by the still-deferred EnumSessions callback,
 * 0x45F2B0), and an owned heap payload freed with GLOBAL_free. */
struct DirectPlayGenericListNode {
    DirectPlayGenericListNode* next;
    void* reserved;
    void* payload;
};

/* player_list (+0xD64) node shape: next, a GlobalAlloc'd handle-backed
 * buffer (released via GlobalHandle/GlobalUnlock/GlobalFree), and an
 * owned heap payload freed with GLOBAL_free. */
struct DirectPlayPlayerListNode {
    DirectPlayPlayerListNode* next;
    void* hglobal_data;
    void* payload;
};

/* connection_list (+0xD6C) node shape: allocated by EnumConnections/
 * FindLocalModemName (operator_new(8) on x86 — 2 dwords), freed as a
 * single block (no owned secondary pointer: 'type' is a plain tag, not
 * a pointer). */
struct DirectPlayConnectionNode {
    DirectPlayConnectionNode* next;
    int32_t type;   /* provider/connection tag — see EnumConnections */
};

/* ================================================================== */
/* DirectPlaySession                                                   */
/* ================================================================== */
class DirectPlaySession {
public:
    /**
     * CreatePeer — Create DirectPlay peer, setting address.
     * Address: 0x45E490
     *
     * Plain __thiscall wrapper (not a constructor — see file header):
     * calls CreateAddress with the two parameters.
     *
     * Called by: Train_InitNetwork (0x439205), Train_HandleJoinMultiplayer (0x43C550)
     */
    void CreatePeer(uint32_t hInstance, uint32_t hWnd);

    /**
     * CreateAddress — Initialize DirectPlay address and interface.
     * Address: 0x45E4B0
     *
     * Sets session fields, creates the transient lobby object via
     * Ordinal_4, queries IDirectPlayLobby3A, releases the initial object.
     */
    void CreateAddress(uint32_t hInstance, uint32_t hWnd);

    /**
     * DestroyPeer — Full DirectPlay peer teardown.
     * Address: 0x45E5A0
     *
     * Calls Close, releases dplay_lobby3a, frees session data, and empties
     * session/player/group/message linked lists.
     */
    void DestroyPeer();

    /**
     * HostSession — Store host session configuration.
     * Address: 0x45E700
     *
     * Stores 4 configuration parameters for hosting a DirectPlay session.
     */
    void HostSession(uint8_t sessionState, uint32_t sessionFlags, uint8_t isHost, uint8_t startupFlag);

    /**
     * ConnectToSession — Join a DirectPlay session.
     * Address: 0x45E730
     *
     * Stores player name and session name/password, creates a player on
     * the DirectPlay server, enumerates players. Returns 1 on success, 0
     * on failure.
     */
    uint8_t ConnectToSession(const char* playerName, const char* sessionName, const char* password);

    /**
     * EnumConnections — Enumerate DirectPlay service providers.
     * Address: 0x45EAB0
     *
     * Returns the connection list head. Creates DirectPlay objects for
     * IPX, TCP/IP, and (device-gated) Serial, checks COM-port availability
     * via CreateFileA, returns the combined list.
     */
    DirectPlayConnectionNode* EnumConnections();

    /**
     * FindLocalModemName — Look up the local player's modem name.
     * Address: 0x45EEC0
     *
     * Queries the local player's own DirectPlay address via
     * IDirectPlay4A::GetPlayerAddress(idPlayer=0 [DPID_ALLPLAYERS — the
     * exact reason for using this sentinel here, rather than the local
     * player's own DPID, is not yet determined], buffer, &size), then
     * walks that address via IDirectPlayLobby3A::EnumAddress looking for
     * the DPAID_Modem chunk (see DirectPlay_FindModemNameCallback). If
     * found and non-empty, the modem name ends up in player_name_buf
     * (+0xD70). Renamed from "GetSessionDesc" — its old name and doc were
     * wrong: this queries a player address and modem-name chunk, not a
     * session description at all.
     *
     * @return true if a modem name was found (and stored in player_name_buf)
     */
    bool FindLocalModemName();

    /**
     * SetSessionDesc — Set session description and host.
     * Address: 0x45F090
     *
     * Stores session password, clears previous player list (unless host),
     * enumerates matching sessions via DirectPlay EnumSessions, retries on
     * DPERR_PENDING status.
     */
    uint32_t SetSessionDesc(const char* password);

    /**
     * Close — Close DirectPlay session and free resources.
     * Address: 0x45FC30
     *
     * Closes the DirectPlay session: releases session desc, cancels
     * outstanding messages, closes/releases the interface, frees
     * player/group lists.
     */
    void Close();

    /**
     * OpenSession — Open (host or join) a DirectPlay session.
     * Address: 0x45FD80
     *
     * Builds a DPSESSIONDESC2 (dwMaxPlayers, guidApplication, session
     * name, password) and calls IDirectPlay4A::Open(lpsd, dwFlags),
     * retrying on DPERR_PENDING. Renamed from "EnumPlayers" — confirmed
     * via disassembly (real vtable offset 0x60 is Open, 2-push arity
     * matching Open(LPDPSESSIONDESC2,DWORD), not EnumPlayers's 4-param
     * signature) — this creates/opens the session, it does not enumerate
     * anything.
     *
     * @return 1 on success, 0x88770100 on DP_OK_USERCANCEL, or error code
     */
    uint32_t OpenSession();

    uint8_t  session_state;           // +0x000: 0=inactive, 1=host, 2=joined
    uint8_t  is_host;                 // +0x001
    uint8_t  startup_flag;            // +0x002
    uint8_t  flag_byte_3;             // +0x003
    uint8_t  flag_byte_4;             // +0x004
    char     session_name[0x400];     // +0x018
    char     player_name[0x80];       // +0x418
    char     session_password[0x80];  // +0x498
    int32_t  connection_type;         // +0x518
    char     connection_name[0x400];  // +0x51C
    int32_t  modem_baud;              // +0x91C
    int32_t  session_flags;           // +0x920
    int32_t  player_dpid;             // +0x924
    void*    session_desc_ptr;        // +0x92C
    uint8_t  session_desc_valid;      // +0x930
    int32_t  flag_934;                // +0x934
    void*    hwnd;                    // +0x938
    void*    hinstance;               // +0x93C
    void*    error_callback;          // +0x940
    uint8_t  show_dialogs;            // +0x944
    char     error_msg_buf[512];      // +0x945
    int32_t  last_hresult;            // +0xD48
    void     (*idle_callback)();      // +0xD4C
    uint8_t  session_ready;           // +0xD50
    int32_t  session_data_size;       // +0xD54
    void*    session_data_ptr;        // +0xD58
    int32_t  max_players;             // +0xD5C
    DirectPlayGenericListNode* session_list;     // +0xD60
    DirectPlayPlayerListNode*  player_list;      // +0xD64
    DirectPlayGenericListNode* group_list;       // +0xD68
    DirectPlayConnectionNode*  connection_list;  // +0xD6C
    char     player_name_buf[0x100];  // +0xD70
    int32_t  modem_settings[5];       // +0x1570
    IDirectPlay4A* dplay_create_obj;  // +0x1584
    IDirectPlay4A* dplay_interface;   // +0x1588
    DPSESSIONDESC2 session_desc;      // +0x158C
    IDirectPlayLobby3A* dplay_lobby_obj; // +0x15DC
    IDirectPlayLobby3A* dplay_lobby3a;   // +0x15E0
    DPCAPS   session_caps;            // +0x15E4
};

/* ================================================================== */
/* Free functions with no `self` — never converted to methods          */
/* ================================================================== */

/* DirectPlay_SessionMgr (0x45DA70) is NOT a DirectPlay function — a Ghidra
 * auto-analysis mislabel. It's AssetMgr's own tree-walk helper, now
 * AssetMgr::RecordPath in resources/AssetMgr.{h,cpp} (converted to a real
 * __thiscall method 2026-08-09). This dead cross-reference declaration
 * (never defined here, never called via this signature) has been removed;
 * see AssetMgr.h for the real declaration. */

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
 * DirectPlay_GetConnectionCaps — Check if connection device exists.
 * Address: 0x45EE60
 *
 * Attempts to open a device file and checks if the handle is valid.
 * Returns 0xFFFFFF00 on failure, 0x0100XX00 on success.
 */
uint32_t DirectPlay_GetConnectionCaps(uint8_t* devicePath);

/* 0x481218: 4 zero bytes (byte-verified) — distinct from EnumConnections's
 * "COMn" template at 0x481214. Used only by DirectPlay_GetConnectionCaps
 * to terminate its path buffer right after the caller-supplied byte. */
extern const char g_device_path_null[4];

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
 * ConnectToSession) push (0, 0, 0). game/Train_network.cpp's
 * Train_ConnectToServer (0x43C8EE-0x43C8F2, 0x43C98D-0x43C991) pushes a
 * real (protocol, address-string, 0) override when message 0x3EB directs
 * a reconnect to a specific server. The ~2076-byte body itself remains a
 * deferred TODO; this signature fix only makes both call sites pass the
 * real arguments instead of silently dropping them.
 */
uint32_t DirectPlay_HandleMessages(int32_t protocol, const char* address, int32_t flags);

/**
 * DirectPlay_FindModemNameCallback — IDirectPlayLobby3A::EnumAddress callback.
 * Address: 0x45FBD0, __stdcall (real signature: LPDPENUMADDRESSCALLBACK)
 *
 * Called while walking the local player's own DirectPlay address (obtained
 * via IDirectPlay4A::GetPlayerAddress) for the DPAID_Modem chunk — the
 * modem name registered with TAPI, present only for modem connections.
 * When found and non-empty, copies it into g_dplay_peer's player_name_buf.
 * Confirmed via disassembly (RET 0x10 = 4 stdcall params) and byte-verified
 * against the real DPAID_Modem GUID (0x4790F8) — this is NOT a "player
 * enumeration" callback; a prior revision of this file mistook
 * guidDataType (a GUID*) for a short ASCII player-name string.
 *
 * Declared with the exact LPDPENUMADDRESSCALLBACK signature (REFGUID,
 * DWORD, LPCVOID, LPVOID) so it can be passed to EnumAddress with no
 * cast, matching what a human writing against Microsoft's real dplay.h
 * would have done.
 *
 * @return TRUE (1) to continue enumeration, FALSE (0) to stop
 */
BOOL STDMETHODCALLTYPE DirectPlay_FindModemNameCallback(const GUID& guidDataType, DWORD dwDataSize,
                                                         LPCVOID lpData, LPVOID lpContext);

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

/* The single live DirectPlay peer. Retyped 2026-08-10 (see file header) —
 * declared/defined consistently as DirectPlaySession* in this header,
 * game/Train_network.cpp, town/Town.cpp, and shared/stubs_impl.cpp (its
 * real definition). */
extern DirectPlaySession* g_dplay_peer;
