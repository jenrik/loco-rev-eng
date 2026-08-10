/**
 * DirectPlay.cpp — DirectPlay network session wrapper implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 * Status: TRANSCRIBED
 *
 * `DirectPlaySession` wraps Microsoft DirectPlay 4 for multiplayer session
 * management. It manages COM interface pointers, player/session data, and
 * linked lists of sessions/players/groups/connections.
 *
 * Architecture note: Lego Loco's "multiplayer" is unusual — it actually
 * uses file-based PostBag directories shared between players. The
 * DirectPlay wrapper is used for connection setup and player enumeration,
 * but actual game data transfer happens through file I/O.
 *
 * Host note: `Ordinal_1`/`Ordinal_4` (dynamically loaded DirectPlay/
 * DirectPlayLobby ordinal exports) have no implementation anywhere in this
 * tree — there is no DirectPlay service-provider DLL on a non-Windows host.
 * Every call site that touches them is guarded `#ifdef _WIN32`; the host
 * side is the same no-op behavior previously implemented as free-function
 * overloads in network/sdl3_directplay_train_bridge.cpp (now folded into
 * the methods themselves, since callers now call real DirectPlaySession
 * methods instead of overload-resolving to a separate host-only free
 * function — see network/DirectPlay.h and PROGRESS.md's directplay-cluster
 * entry).
 */

#include "DirectPlay.h"
#include "../stubs/ddraw.h"
#include "../core/Entity.h"
#include <cstddef>
#include <cstring>
#include <initializer_list>
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */

/* DirectPlay COM interface GUIDs (from .rdata section of loco.exe) */
/*
 * GUID layout in memory (little-endian): Data1(4) Data2(2) Data3(2) Data4(8)
 * IID_IDirectPlay4A     @ 0x478F88: {0AB1C531-4745-11D1-A7A1-0000F803ABFC}
 * CLSID_DirectPlay      @ 0x478F98: {D1EB6D20-8923-11D0-9D97-00A0C90A43CB}
 *   (real CLSID, unused by the reconstructed code below — only referenced
 *    from the still-deferred body of DirectPlay_HandleMessages)
 * DP Session GUID       @ 0x479158: {F9CD2546-577F-11D2-9426-00A0244BDA7A}
 * IID_IDirectPlayLobby3A @ 0x479048: {2DB72491-652C-11D1-A7A8-0000F803ABFC}
 *   (byte-verified against Microsoft's dplay.h/dplobby.h, republished by
 *    the genuine DirectX 6.0 SDK dplay.h (see NOTE-directx-sdk.md; also
 *    mirrored at https://github.com/Olde-Skuul/directplay). Previously
 *    mislabeled "IID_IDirPlayAddr2" — no such interface exists in classic
 *    DirectPlay; see PROGRESS.md.)
 * DPSPGUID_IPX/TCPIP/SERIAL/MODEM @ 0x478fa8-0x478fe7: byte-verified
 *   service-provider GUIDs (see EnumConnections/FindLocalModemName below).
 *   All 4 match the genuine DirectX 6.0 SDK's dplay.h constants exactly
 *   (read directly from loco.exe's own .rdata, not transcribed from a
 *   secondary source).
 */
/* IID_IDirectPlay4A: QueryInterface argument to obtain the ANSI IDirectPlay4
 * interface (the symbol name was previously CLSID_DirectPlay — wrong, this
 * value is an IID used for QueryInterface, not a CLSID for object creation).
 * Declared as a real GUID (not a raw address literal or uint32_t[4]) so
 * QueryInterface calls reference it by name, the way a human writing
 * against Microsoft's real dplay.h would have. */
static const GUID IID_IDirectPlay4A = {0x0ab1c531, 0x4745, 0x11d1,
                                        {0xa7, 0xa1, 0x00, 0x00, 0xf8, 0x03, 0xab, 0xfc}};
/* IID_IDirectPlayLobby3A: QueryInterface argument to obtain the ANSI
 * IDirectPlayLobby3 interface — see the file-header comment above. */
static const GUID IID_IDirectPlayLobby3A = {0x2db72491, 0x652c, 0x11d1,
                                             {0xa7, 0xa8, 0x00, 0x00, 0xf8, 0x03, 0xab, 0xfc}};
/* GUID_SessionDesc: game's guidApplication, stored into DPSESSIONDESC2 */
static const GUID GUID_SessionDesc = {0xf9cd2546, 0x577f, 0x11d2,
                                       {0x94, 0x26, 0x00, 0xa0, 0x24, 0x4b, 0xda, 0x7a}};
/* DPAID_Modem {F6DCC200-A2FE-11D0-9C4F-00A0C905425E}, byte-verified at
 * 0x4790F8 against the real DirectX 6.0 SDK's dplobby.h ("Chunk is a
 * string containing a modem name registered with TAPI"). Identifies
 * the address-element chunk that DirectPlay_FindModemNameCallback looks
 * for when walking a DirectPlay address via IDirectPlayLobby3A::EnumAddress. */
static const GUID DPAID_Modem = {0xf6dcc200, 0xa2fe, 0x11d0,
                                  {0x9c, 0x4f, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};

/* DPSPGUID_IPX/TCPIP/SERIAL/MODEM — classic DirectPlay 3-6 service-provider
 * GUIDs. Byte-verified 2026-08-10 by reading loco.exe's .rdata directly
 * (file offsets computed from the PE section table: .rdata VMA 0x477000,
 * file offset 0x75c00) rather than transcribed from a secondary source.
 * Used by EnumConnections/FindLocalModemName's Ordinal_1 calls below —
 * see DirectPlaySession::EnumConnections's real disassembly (0x45EB0B,
 * 0x45EBB1, 0x45EDA2) and FindLocalModemName's (0x45EEE3). A prior
 * TRANSCRIBED revision of this file passed all-nullptr arguments to
 * Ordinal_1 here (both the wrong argument count and the wrong values —
 * see the Ordinal_1 declaration below). */
static const GUID DPSPGUID_IPX    = {0x685bc400, 0x9d2c, 0x11cf,
                                      {0xa9, 0xcd, 0x00, 0xaa, 0x00, 0x68, 0x86, 0xe3}};
static const GUID DPSPGUID_TCPIP  = {0x36e95ee0, 0x8577, 0x11cf,
                                      {0x96, 0x0c, 0x00, 0x80, 0xc7, 0x53, 0x4e, 0x82}};
static const GUID DPSPGUID_SERIAL = {0x0f1d6860, 0x88d9, 0x11cf,
                                      {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
static const GUID DPSPGUID_MODEM  = {0x44eaa760, 0xcb68, 0x11cf,
                                      {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* C++ allocation helpers */
void* __cdecl operator_new(size_t size);
void  __cdecl GLOBAL_free(void* ptr);

extern "C" {
    /* CRT memory management */
    void* __cdecl CRT_malloc_zero(size_t size);
    void  __cdecl CRT_free(void* ptr);
    int32_t __cdecl CRT_atoi(const uint8_t* str);
    int32_t __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...);

    /* Win32 API */
    void* __stdcall GetStockObject(int32_t fnObject);
    int32_t __stdcall FillRect(void* hDC, void* lprc, void* hbr);
    void* __stdcall GetProcessHeap(void);
    void* __stdcall HeapAlloc(void* hHeap, uint32_t dwFlags, uint32_t dwBytes);
    int32_t __stdcall HeapFree(void* hHeap, uint32_t dwFlags, void* lpMem);
    int32_t __stdcall DialogBoxParamA(void* hInstance, const char* lpTemplateName,
                                       void* hWndParent, void* lpDialogFunc, int32_t dwInitParam);
    /* CreateFileA, PlaySoundA, GetSystemMetrics, wsprintfA, LoadStringA,
     * MessageBoxA, CloseHandle, Sleep: declared in stubs/windows.h (pulled
     * in transitively via stubs/dplay.h for the GUID/DPID types below),
     * not redeclared here — their real signatures use HANDLE/BOOL/HMODULE,
     * not the int32_t/void* placeholders this block used before. CreateFileA
     * has a genuine host implementation (graphics/sdl3_window.cpp), unlike
     * Ordinal_1/Ordinal_4 below — safe to call unguarded. */
    void*   __stdcall GlobalAlloc(uint32_t uFlags, uint32_t dwBytes);
    void*   __stdcall GlobalLock(void* hMem);
    int32_t __stdcall GlobalUnlock(void* hMem);
    void*   __stdcall GlobalFree(void* hMem);
    void*   __stdcall GlobalHandle(const void* pMem);
    int32_t __stdcall lstrlenA(const char* lpString);
    int32_t __stdcall CoCreateInstance(void* rclsid, void* pUnkOuter, uint32_t dwClsContext,
                                        void* riid, void** ppv);

}

/* Game engine functions (C++ linkage) */
void* GameObject_BaseCtor(void* mem, int32_t a, int32_t b, int32_t c, int32_t d);
void  DDRAW_PresentRect(void* rect, void* hWnd, int32_t* param, uint8_t flag);

/* DirectPlay game functions (C++ linkage) */
uint32_t WIN32_RecvNetworkData(void* session, uint32_t resId, const char* msg);
uint32_t WIN32_GetSystemMetrics(void* session);

/* ================================================================== */
/* DirectPlay/DirectPlayLobby ordinal exports (loaded via GetProcAddress */
/* from dplay.dll/dpmodem.dll on real Windows — see file header for why  */
/* every call site is #ifdef _WIN32-guarded here: neither has any        */
/* implementation on this tree's host build).                           */
/*                                                                       */
/* Ordinal_1's real 3-argument shape ((const GUID*, void**, int32_t)) is */
/* confirmed via disassembly of EnumConnections (0x45EB0B-0x45ED30) and  */
/* FindLocalModemName (0x45EEE3-0x45EF17): CALL 0x4637d8 (itself already */
/* named "Ordinal_1" by Ghidra) with 3 pushed args — GUID pointer, then  */
/* the &this->dplay_create_obj output slot, then a 0 flags dword — not   */
/* the previous (void*,void*,void*,void*) shape this file declared,      */
/* which both had the wrong arity and (being called with 4 nullptrs)     */
/* discarded the real provider GUID entirely. audio/GameAudio.cpp        */
/* separately declares its own 2-arg `Ordinal_1(int32_t, void*)` for an   */
/* unrelated DirectSound ordinal that happens to share this generic       */
/* Ghidra-assigned name; since both are plain (non-`extern "C"`) C++      */
/* declarations they mangle independently and neither is ever defined     */
/* anywhere in this tree (see docs/landmine-sweep-worklist.md's           */
/* "genuinely missing" census) — no ODR conflict either way.              */
/* ================================================================== */

#ifdef _WIN32
extern int32_t __stdcall Ordinal_1(const GUID* provider, void** outObject, int32_t flags);
extern int32_t __stdcall Ordinal_4(void* ptr1, void** ptr2, void* ptr3, void* ptr4, void* ptr5);
#endif

/* ================================================================== */
/* COM interfaces are called through their real typed virtual methods */
/* (IDirectDrawSurface4/IDirectPlay4A/IDirectPlayLobby3A, declared as a */
/* typed adapter in stubs/ddraw.h/dplay.h whose method order/signatures */
/* are verified against the genuine DirectX 6.0 SDK — see the header    */
/* comment there for why the real mingw-w64 headers aren't used         */
/* directly). No raw vtable-offset arithmetic anywhere in this file:    */
/* every interface pointer below is properly typed, so                  */
/* e.g. `dplay->GetPlayerAddress(...)` resolves through the compiler-  */
/* generated vtable call exactly as it would if loco.exe had been      */
/* written against Microsoft's real dplay.h directly (which it was).   */
/* Real IDirectPlay4/4A slot order (0=QueryInterface,1=AddRef,          */
/* 2=Release): 3=AddPlayerToGroup 4=Close 5=CreateGroup 6=CreatePlayer  */
/* 7=DeletePlayerFromGroup 8=DestroyGroup 9=DestroyPlayer               */
/* 10=EnumGroupPlayers 11=EnumGroups 12=EnumPlayers 13=EnumSessions     */
/* 14=GetCaps 15=GetGroupData 16=GetGroupName 17=GetMessageCount        */
/* 18=GetPlayerAddress 19=GetPlayerCaps 20=GetPlayerData 21=GetPlayerName*/
/* 22=GetSessionDesc 23=Initialize 24=Open 25=Receive 26=Send ...       */
/* 51=CancelMessage — cross-checked against the genuine DirectX 6.0 SDK */
/* (see NOTE-directx-sdk.md) and confirmed via disassembly (not         */
/* decompiler pseudocode, which was independently shown to fabricate a  */
/* stale-register argument on one call site — see                      */
/* DirectPlay_FindModemNameCallback's comment).                        */
/*                                                                       */
/* SetSessionDesc (0x45F090, calls the real EnumSessions below with a   */
/* genuine callback at 0x45F2B0) is NOT yet renamed: EnumSessions's      */
/* callback body is substantial, previously entirely unexamined code    */
/* (builds a session-list linked list conditionally on connection type) */
/* and needs its own dedicated decompilation pass before this function's */
/* true higher-level purpose (join-precheck? host-precheck?) can be      */
/* named with confidence — see PROGRESS.md.                              */
/* ================================================================== */

/* LPDPENUMSESSIONSCALLBACK2 — real callback type for EnumSessions.
 * Address: 0x45F2B0, __stdcall (RET 0x10 = 4 params, confirmed via
 * decode_instructions). Builds a session-list entry (operator_new(0xC),
 * appended to the linked list at session+0xD64) when dwFlags's low bit
 * (DPESC_TIMEDOUT) is clear and connection_type == 1; returns immediately
 * otherwise. This is genuinely new territory — no prior revision of this
 * file referenced this address at all (it was passed as a bare GUID
 * pointer instead, a separate bug fixed below) — so the body is left as
 * a documented deferred stub rather than guessed at partial disassembly.
 * TODO: decompile 0x45F2B0 fully (tracked in PROGRESS.md). */
static BOOL STDMETHODCALLTYPE DirectPlay_EnumSessionsCallback(LPCDPSESSIONDESC2 lpThisSD, LPDWORD lpdwTimeOut,
                                                               DWORD dwFlags, LPVOID lpContext) {
    (void)lpThisSD; (void)lpdwTimeOut; (void)dwFlags; (void)lpContext;
    return 1;  /* stub — TODO: decompile 0x45F2B0; continue enumeration */
}

/* ================================================================== */
/* Global state                                                       */
/* ================================================================== */

extern void* _g_primary_surface;   /* 0x4FD3C4 — primary DirectDraw surface */
extern void* _g_dsound_object;     /* 0x4FD398 — shadow GameObject */
extern void* g_main_window;          /* 0x4AA4A0 */
extern int32_t g_client_width;
extern int32_t g_client_height;      /* primary surface width */
extern char g_empty_string;         /* 0x4851D0 */
extern char g_install_path[];       /* 0x4A99C8 */

/* g_device_path_null (0x481218): 4 zero bytes, byte-verified 2026-08-10
 * by reading loco.exe's .data section directly (file offset computed from
 * the PE section table: .data VMA 0x47e000, file offset 0x7c400). Declared
 * extern in network/DirectPlay.h since ~2026-08-05 but never actually
 * defined anywhere — a pre-existing "missing global" landmine, dormant
 * only because DirectPlay_GetConnectionCaps's one real caller
 * (game/Train_network.cpp's DirectPlay_QueryConnection call) was itself
 * bound to the wrong linkage/address until this session's cluster fix made
 * it genuinely reachable for the first time (confirmed via gdb: SIGSEGV on
 * this exact symbol, TrainSubsystem::TrainSubsystem -> netPanelInit,
 * reproducible on ordinary main-menu load, not just multiplayer entry). */
const char g_device_path_null[4] = {0, 0, 0, 0};

/* DirectPlay_SessionMgr (0x45DA70) was a Ghidra auto-analysis mislabel —
 * not a DirectPlay function at all. It's AssetMgr::RecordPath now (see
 * resources/AssetMgr.h/.cpp). Nothing in this file calls it. */

/* ================================================================== */
/* DirectPlay_Init — Initialize DirectPlay/compositing                */
/* Address: 0x45E090                                                   */
/*                                                                     */
/* Called once from CGWND_InitMode1 to set up the initial screen      */
/* compositing: plays a startup sound, fills the primary surface,     */
/* creates a shadow GameObject, positions it center-screen, and       */
/* presents the first frame.                                           */
/* ================================================================== */
void DirectPlay_Init(void)
{
    /* Step 1: Play startup sound (null = just stop/init) */
    PlaySoundA(NULL, NULL, 0);

    /* Step 2: Get primary surface DC, fill with white, release DC.
     * _g_primary_surface is declared void* for this project's established
     * cross-file convention (see shared/types.h), but its real type is
     * IDirectDrawSurface4* (Cursor_internal.h's own comment agrees) —
     * typed here so GetDC/ReleaseDC go through real virtual methods,
     * not manual vtable-offset arithmetic. */
    IDirectDrawSurface4* primarySurface = static_cast<IDirectDrawSurface4*>(_g_primary_surface);
    void* surface_hdc = nullptr;
    primarySurface->GetDC(&surface_hdc);

    void* hBrush = GetStockObject(0); /* WHITE_BRUSH (stock object index 0) */
    // NOTE: binary passes &g_client_width as RECT* — these globals happen
    // to be contiguous (g_client_width, g_client_height, g_window_right,
    // g_window_bottom). We construct a local RECT for correctness.
    RECT fillRect;
    fillRect.left   = 0;
    fillRect.top    = 0;
    fillRect.right  = g_client_width;
    fillRect.bottom = g_client_height;
    FillRect(surface_hdc, &fillRect, hBrush);
    primarySurface->ReleaseDC(surface_hdc);

    /* Step 3: Create shadow GameObject at 0x4FD398 (res 0x402) */
    void* shadow_mem = operator_new(0x88);
    if (shadow_mem == NULL) {
        _g_dsound_object = NULL;
    } else {
        _g_dsound_object = GameObject_BaseCtor(shadow_mem, 0x402, -1, 0, 0);
    }

    /* Step 4: Position shadow at screen center */
    {
        Entity* shadow = static_cast<Entity*>(_g_dsound_object);
        int32_t screen_h = GetSystemMetrics(1);   /* SM_CYSCREEN */
        int32_t y_center = screen_h / 2;
        /* Subtract half of frame height from the sprite resource data */
        void* resource_ptr = shadow->resource;    /* Entity::resource at +0x40 */
        uint16_t frame_h = *reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(resource_ptr) + 0x16);
        y_center -= static_cast<uint16_t>(frame_h >> 1);
        int32_t screen_w = GetSystemMetrics(0);   /* SM_CXSCREEN */
        int32_t x_center = (screen_w + (screen_w >> 0x1F & 3)) >> 2; /* quarter screen */

        shadow->MoveTo(x_center, y_center);
    }

    /* Step 5: Stop any playing sound, draw initial frame */
    Entity* shadow = static_cast<Entity*>(_g_dsound_object);
    shadow->StopSound(0);
    shadow->Draw(shadow->screen_rect, 0, 0);

    /* Step 6: Present to screen */
    // NOTE: binary passes &g_client_width as RECT* and *(g_main_window+8) as HWND
    DDRAW_PresentRect(&fillRect,
                       *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_main_window) + 8),
                       NULL, 0);
}

/* ================================================================== */
/* DirectPlaySession::CreatePeer                                       */
/* Address: 0x45E490                                                   */
/* ================================================================== */
void DirectPlaySession::CreatePeer(uint32_t hInstance, uint32_t hWnd)
{
    CreateAddress(hInstance, hWnd);
}

/* ================================================================== */
/* DirectPlaySession::CreateAddress                                    */
/* Address: 0x45E4B0                                                   */
/* ================================================================== */
void DirectPlaySession::CreateAddress(uint32_t hInstance, uint32_t hWnd)
{
    /* Field bookkeeping — host-independent, runs unconditionally. */
    show_dialogs = 1;
    startup_flag = 1;
    idle_callback = nullptr;
    error_callback = nullptr;
    error_msg_buf[0] = 0;
    last_hresult = 0;
    session_ready = 0;
    session_state = 0;
    is_host = 0;
    session_flags = 0;
    flag_byte_3 = 0;
    flag_byte_4 = 0;
    dplay_interface = nullptr;
    session_name[0] = 0;
    session_desc_ptr = nullptr;
    session_desc_valid = 0;
    player_name[0] = 0;
    player_dpid = 0;
    connection_type = 0;
    hinstance = reinterpret_cast<void*>(static_cast<uintptr_t>(hInstance));
    hwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(hWnd));
    session_data_size = 0;
    session_data_ptr = nullptr;
    session_list = nullptr;
    player_list = nullptr;
    group_list = nullptr;
    connection_list = nullptr;
    max_players = 10;
    dplay_lobby_obj = nullptr;
    dplay_lobby3a = nullptr;

#ifdef _WIN32
    /* Obtain the transient lobby object (Ordinal_4), then QueryInterface it
     * up to IDirectPlayLobby3A — see the file-header comment on why this
     * is not "IDirectPlayAddress" (that interface doesn't exist). */
    int32_t hr = Ordinal_4(nullptr, reinterpret_cast<void**>(&dplay_lobby_obj), nullptr, nullptr, nullptr);
    last_hresult = hr;

    if (hr == 0 && dplay_lobby_obj != nullptr) {
        hr = dplay_lobby_obj->QueryInterface(IID_IDirectPlayLobby3A, reinterpret_cast<void**>(&dplay_lobby3a));
        last_hresult = hr;
    }

    if (dplay_lobby_obj != nullptr) {
        dplay_lobby_obj->Release();
    }
    dplay_lobby_obj = nullptr;
#else
    /* Host: no DirectPlayLobby service-provider DLL to create an object
     * from. Matches network/sdl3_directplay_train_bridge.cpp's prior
     * DirectPlay_CreatePeer no-op (now folded in here — see file header). */
#endif
}

/* ================================================================== */
/* DirectPlaySession::DestroyPeer                                      */
/* Address: 0x45E5A0                                                   */
/* ================================================================== */
void DirectPlaySession::DestroyPeer()
{
    Close();

    if (dplay_lobby3a != nullptr) {
        dplay_lobby3a->Release();
        dplay_lobby3a = nullptr;
    }

    if (session_data_ptr != nullptr) {
        GLOBAL_free(session_data_ptr);
        session_data_ptr = nullptr;
        session_data_size = 0;
    }

    DirectPlayGenericListNode* session_node = session_list;
    while (session_node != nullptr) {
        DirectPlayGenericListNode* next = session_node->next;
        if (session_node->payload != nullptr) {
            GLOBAL_free(session_node->payload);
        }
        GLOBAL_free(session_node);
        session_list = next;
        session_node = next;
    }

    DirectPlayPlayerListNode* player = player_list;
    while (player != nullptr) {
        DirectPlayPlayerListNode* next = player->next;
        if (player->payload != nullptr) {
            GLOBAL_free(player->payload);
        }
        if (player->hglobal_data != nullptr) {
            void* hglb = GlobalHandle(player->hglobal_data);
            GlobalUnlock(hglb);
            hglb = GlobalHandle(player->hglobal_data);
            GlobalFree(hglb);
            player->hglobal_data = nullptr;
        }
        GLOBAL_free(player);
        player_list = next;
        player = next;
    }

    DirectPlayGenericListNode* group = group_list;
    while (group != nullptr) {
        DirectPlayGenericListNode* next = group->next;
        if (group->payload != nullptr) {
            GLOBAL_free(group->payload);
        }
        GLOBAL_free(group);
        group_list = next;
        group = next;
    }

    DirectPlayConnectionNode* conn = connection_list;
    while (conn != nullptr) {
        DirectPlayConnectionNode* next = conn->next;
        GLOBAL_free(conn);
        connection_list = next;
        conn = next;
    }
}

/* ================================================================== */
/* DirectPlaySession::HostSession                                      */
/* Address: 0x45E700                                                   */
/* ================================================================== */
void DirectPlaySession::HostSession(uint8_t sessionState, uint32_t sessionFlags,
                                     uint8_t isHost, uint8_t startupFlag)
{
    session_state = sessionState;
    session_flags = sessionFlags;
    is_host = isHost;
    startup_flag = startupFlag;
}

static void copy_session_name(char* destination, const char* source,
                              std::size_t capacity)
{
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }
    std::size_t index = 0;
    while (index < capacity && source[index] != '\0') {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

/* ================================================================== */
/* DirectPlaySession::ConnectToSession                                 */
/* Address: 0x45E730                                                   */
/* ================================================================== */
uint8_t DirectPlaySession::ConnectToSession(const char* playerName,
                                             const char* sessionName,
                                             const char* password)
{
    copy_session_name(session_name, sessionName, 0x3FF);
    copy_session_name(player_name, playerName, 0x80);
    copy_session_name(session_password, password, 0x80);

    // DECOMPILER NOTE: verify session_password[0] = 0 against disasm at 0x45E840.
    // This clears password[0] after copy; may target a different field.
    session_password[0] = 0;

    /* CreatePlayer's LPDPNAME argument, byte-verified via disassembly
     * (0x45EA0C/0x45EA4C): a real DPNAME{dwSize=0x10, dwFlags=0,
     * lpszShortName=&g_empty_string, lpszLongName=player_name} built on
     * the stack, hEvent/lpData explicitly nullptr, dwDataSize/dwFlags 0.
     * A prior TRANSCRIBED revision of this file mis-split those 4 DPNAME
     * fields across CreatePlayer's own positional arguments instead
     * (passing &desc_size — a lone int — as the whole LPDPNAME, and
     * &g_empty_string/player_name as hEvent/lpData) — same real values,
     * wrong shape, reading past a 4-byte local as if it were the 16-byte
     * struct. Both call sites below (join and host branches) share this
     * fix. */
    auto create_player = [this]() -> uint8_t {
        if (dplay_interface == nullptr) {
            return 0;
        }
        DPNAME name{};
        name.dwSize = sizeof(DPNAME);
        name.dwFlags = 0;
        name.lpszShortName = &g_empty_string;
        name.lpszLongName = player_name;
        int32_t hr = dplay_interface->CreatePlayer(reinterpret_cast<DPID*>(&player_dpid), &name,
                                                     nullptr, nullptr, 0, 0);
        last_hresult = hr;
        if (hr == 0) {
            return 1;
        }
        char err_buf[300];
        char err_msg[300];
        DirectPlay_Open(err_buf, hr);
        wsprintfA(err_msg, "Failed to join member '%s' to session: %s", player_name, err_buf);
        WIN32_RecvNetworkData(this, 0, err_msg);
        return 0;
    };

    uint8_t result = 0;

    if (session_state == 0) {
        /* Not hosting: try to join */
        uint32_t hr = DirectPlay_HandleMessages(0, nullptr, 0);
        if (static_cast<uint8_t>(hr) == 0) {
            Close();
            return 0;
        }

        uint32_t enum_result = WIN32_GetSystemMetrics(this);
        if (static_cast<uint8_t>(enum_result) == 0) {
            Close();
            return 0;
        }

        result = create_player();
        if (!result) {
            Close();
            return 0;
        }
    } else {
        /* Hosting: handle messages then open the session */
        uint32_t hr = DirectPlay_HandleMessages(0, nullptr, 0);
        if (static_cast<uint8_t>(hr) == 0) {
            Close();
            return 0;
        }

        uint32_t enum_result = OpenSession();
        if (static_cast<uint8_t>(enum_result) != 0) {
            result = create_player();
            if (!result) {
                Close();
                return 0;
            }
        } else {
            /* Open failed — check if error is DPERR_PENDING (user cancel) */
            if (is_host != 0 && last_hresult == -0x7788fea2) {
                return 0;
            }
            Close();
            return 0;
        }
    }

    /* On success: query the real DPCAPS */
    if (result != 0 && dplay_interface != nullptr) {
        memset(&session_caps, 0, sizeof(DPCAPS));
        session_caps.dwSize = sizeof(DPCAPS);
        dplay_interface->GetCaps(&session_caps, 0);
    }

    return result;
}

/* ================================================================== */
/* DirectPlaySession::EnumConnections                                  */
/* Address: 0x45EAB0                                                   */
/* ================================================================== */
DirectPlayConnectionNode* DirectPlaySession::EnumConnections()
{
    /* Return existing list if already enumerated */
    if (connection_list != nullptr) {
        return connection_list;
    }
    if (dplay_interface != nullptr) {
        return nullptr;  /* already connected */
    }

    /* Local modem name lookup doubles as a connectivity probe: on success,
     * prepend a type=1 connection entry — matches disassembly (0x45EAE3-
     * 0x45EB05: unconditional on FindLocalModemName succeeding, before any
     * of the three provider attempts below). */
    if (FindLocalModemName()) {
        auto* entry = static_cast<DirectPlayConnectionNode*>(operator_new(sizeof(DirectPlayConnectionNode)));
        entry->next = connection_list;
        entry->type = 1;
        connection_list = entry;
    }

    /* Provider attempt: create a DirectPlay object for `provider` via
     * Ordinal_1, QueryInterface it up to IDirectPlay4A, and on success
     * prepend a connection_list entry tagged `type`. De-duplicates the
     * three near-identical blocks in the original disassembly (0x45EB0B
     * IPX/type=4, 0x45EBB1 TCP-IP/type=2, 0x45EDA2 Serial/type=3 — the
     * "IPX"/"TCP-IP" comments on a prior TRANSCRIBED revision of this
     * function had these two swapped; corrected here against the byte-
     * verified DPSPGUID_* constants). */
    auto try_provider = [this](const GUID& provider, int32_t type) -> bool {
#ifdef _WIN32
        int32_t hr = Ordinal_1(&provider, reinterpret_cast<void**>(&dplay_create_obj), 0);
        last_hresult = hr;
        if (hr != 0) {
            return false;
        }
        hr = dplay_create_obj->QueryInterface(IID_IDirectPlay4A, reinterpret_cast<void**>(&dplay_interface));
        last_hresult = hr;
        if (hr != 0) {
            dplay_create_obj->Release();
            dplay_create_obj = nullptr;
            return false;
        }
        dplay_create_obj->Release();
        dplay_create_obj = nullptr;
        dplay_interface->Release();
        dplay_interface = nullptr;

        auto* entry = static_cast<DirectPlayConnectionNode*>(operator_new(sizeof(DirectPlayConnectionNode)));
        entry->next = connection_list;
        entry->type = type;
        connection_list = entry;
        return true;
#else
        /* Host: no DirectPlay service-provider DLL to create an object
         * from. Matches network/sdl3_directplay_train_bridge.cpp's prior
         * DirectPlay_EnumConnections no-op (returned nullptr unconditionally
         * — reproduced here since neither provider ever succeeds, leaving
         * connection_list untouched). */
        (void)provider;
        (void)type;
        return false;
#endif
    };

    /* IPX (unconditional) */
    try_provider(DPSPGUID_IPX, 4);

    /* TCP/IP (unconditional) */
    try_provider(DPSPGUID_TCPIP, 2);

    /* Device presence check via CreateFileA: try COM1..COM4 in turn. If
     * none exist, skip the Serial provider entirely — matches the real
     * control flow at 0x45EC51-0x45ED96, which jumps straight to the
     * final return (0x45EE47) when no device is found, never calling
     * Ordinal_1 for the Serial provider. Byte-verified against .data at
     * 0x481204-0x481218 ("COMn\0" template overwritten with digits
     * '1'-'4' per attempt). This whole block was silently dropped by a
     * prior TRANSCRIBED pass of this function — reconstructed here from
     * disassembly. CreateFileA has a real host implementation (unlike
     * Ordinal_1/Ordinal_4), so this loop runs unguarded on every platform;
     * it simply never finds a COM device on a non-Windows host. */
    bool device_found = false;
    for (char digit : {'1', '2', '3', '4'}) {
        char path[8] = "COM";
        path[3] = digit;
        path[4] = '\0';
        HANDLE hFile = CreateFileA(path, 0xC0000000, 0, NULL, 3, 0, NULL);
        if (hFile != reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1))) {
            CloseHandle(hFile);
            device_found = true;
            break;
        }
    }
    if (!device_found) {
        return connection_list;
    }

    /* Serial (device-gated) */
    try_provider(DPSPGUID_SERIAL, 3);

    return connection_list;
}

/* ================================================================== */
/* DirectPlay_GetConnectionCaps — Check if connection device exists   */
/* Address: 0x45EE60                                                   */
/*                                                                     */
/* Attempts to open a device file. Returns success/failure.           */
/* ================================================================== */
uint32_t DirectPlay_GetConnectionCaps(uint8_t* devicePath)
{
    /* Build device path from first byte + global string */
    uint8_t local_path[12];
    local_path[0] = devicePath[0];
    /* Copy g_device_path_null (4 zero bytes at 0x481218 — byte-verified;
     * distinct from EnumConnections's "COMn" template at 0x481214, a
     * different .rdata address entirely) into local_path+1..4, terminating
     * the path right after the single caller-supplied character. */
    *reinterpret_cast<uint32_t*>(&local_path[1]) =
        *reinterpret_cast<const uint32_t*>(g_device_path_null);

    HANDLE hFile = CreateFileA(reinterpret_cast<const char*>(local_path), 0xC0000000, 0,
                                NULL, 3, 0, NULL);
    if (hFile == reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1))) {  /* INVALID_HANDLE_VALUE */
        return 0xFFFFFF00;
    }
    CloseHandle(hFile);
    return 0x0100FF00;  /* success with type byte */
}

/* ================================================================== */
/* DirectPlaySession::FindLocalModemName                               */
/* Address: 0x45EEC0                                                   */
/* ================================================================== */
bool DirectPlaySession::FindLocalModemName()
{
    if (dplay_interface != nullptr) {
        return false;  /* already connected */
    }

#ifdef _WIN32
    /* Create a DirectPlay object for the Modem service provider via
     * Ordinal_1 — byte-verified DPSPGUID_MODEM, see file header. */
    int32_t hr = Ordinal_1(&DPSPGUID_MODEM, reinterpret_cast<void**>(&dplay_create_obj), 0);
    last_hresult = hr;
    if (hr != 0) {
        return false;
    }

    /* Query IDirectPlay4 interface via IID_IDirectPlay4A */
    hr = dplay_create_obj->QueryInterface(IID_IDirectPlay4A, reinterpret_cast<void**>(&dplay_interface));
    last_hresult = hr;
    if (hr != 0) {
        dplay_create_obj->Release();
        dplay_create_obj = nullptr;
        return false;
    }

    /* Release the transient create object */
    dplay_create_obj->Release();
    dplay_create_obj = nullptr;
#else
    /* Host: no Modem service-provider DLL to create an object from.
     * Matches network/sdl3_directplay_train_bridge.cpp's prior no-op. */
    return false;
#endif

    /* Query the local player's own address */
    DWORD desc_size = 0;
    dplay_interface->GetPlayerAddress(0, NULL, &desc_size);

    /* Allocate + lock global memory for the address blob */
    void* hMem = GlobalAlloc(0x42, desc_size);
    void* pMem = GlobalLock(hMem);

    int32_t hr2 = dplay_interface->GetPlayerAddress(0, pMem, &desc_size);
    last_hresult = hr2;

    if (hr2 != 0) {
        if (pMem != nullptr) {
            void* hglb = GlobalHandle(pMem);
            GlobalUnlock(hglb);
            hglb = GlobalHandle(pMem);
            GlobalFree(hglb);
        }
        dplay_interface->Release();
        dplay_interface = nullptr;
        return false;
    }

    /* Walk the address for the DPAID_Modem chunk */
    player_name_buf[0] = 0;
    hr2 = dplay_lobby3a->EnumAddress(&DirectPlay_FindModemNameCallback, pMem, desc_size, NULL);
    last_hresult = hr2;

    /* Cleanup */
    if (pMem != nullptr) {
        void* hglb = GlobalHandle(pMem);
        GlobalUnlock(hglb);
        hglb = GlobalHandle(pMem);
        GlobalFree(hglb);
    }
    dplay_interface->Release();
    dplay_interface = nullptr;

    return player_name_buf[0] != 0;  /* true if a modem name was found */
}

/* ================================================================== */
/* DirectPlaySession::SetSessionDesc                                   */
/* Address: 0x45F090                                                   */
/* ================================================================== */
uint32_t DirectPlaySession::SetSessionDesc(const char* password)
{
    copy_session_name(session_password, password, 0x80);

    /* Clear player list if not host */
    if (!is_host) {
        DirectPlayPlayerListNode* player = player_list;
        while (player != nullptr) {
            DirectPlayPlayerListNode* next = player->next;
            if (player->payload != nullptr) {
                GLOBAL_free(player->payload);
            }
            if (player->hglobal_data != nullptr) {
                void* hglb = GlobalHandle(player->hglobal_data);
                GlobalUnlock(hglb);
                hglb = GlobalHandle(player->hglobal_data);
                GlobalFree(hglb);
                player->hglobal_data = nullptr;
            }
            GLOBAL_free(player);
            player_list = next;
            player = next;
        }
    }

    /* Handle messages to create DirectPlay */
    uint32_t hr = DirectPlay_HandleMessages(0, nullptr, 0);
    if (static_cast<uint8_t>(hr) == 0) {
        return 0;
    }

    /* Defensive null check: DirectPlay_HandleMessages is still a deferred
     * stub (see its own declaration comment) that unconditionally reports
     * success without ever establishing dplay_interface — the real x86
     * disassembly's equivalent check (`dplay_interface==0 && HandleMessages
     * failed -> bail`) only stays safe once HandleMessages is genuinely
     * implemented and sets dplay_interface as a side effect of succeeding.
     * Until then, this check (mirroring OpenSession's own unconditional
     * one) is what keeps this reachable on a host build where
     * dplay_interface never gets set at all. */
    if (dplay_interface == nullptr) {
        return 0;
    }

    /* Build a real DPSESSIONDESC2 — see stubs/dplay.h for the layout;
     * loco.exe's own dword-indexed writes matched this real struct
     * exactly. */
    memset(&session_desc, 0, sizeof(DPSESSIONDESC2));
    session_desc.dwSize          = sizeof(DPSESSIONDESC2);
    session_desc.guidApplication = GUID_SessionDesc;

    if (session_password[0] != 0) {
        session_desc.lpszPasswordA = session_password;
    }

    /* Enumerate matching sessions via DirectPlay (NOT "Open"/CreateSession —
     * see the vtable-slot comment above; the callback's own body at
     * 0x45F2B0 is still a deferred stub) */
    int32_t hr2 = dplay_interface->EnumSessions(&session_desc, 0, &DirectPlay_EnumSessionsCallback,
                                                 hwnd, 0x81);
    last_hresult = hr2;

    /* Retry on DPERR_PENDING */
    while (hr2 == -0x7788fea2) {  /* DPERR_PENDING */
        if (is_host != 0) {          /* flag set to abort */
            return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(player_list));
        }
        if (idle_callback != nullptr) {
            idle_callback();  /* idle callback */
        }
        Sleep(1);

        hr2 = dplay_interface->EnumSessions(&session_desc, 0, &DirectPlay_EnumSessionsCallback,
                                             hwnd, 0x81);
        last_hresult = hr2;
    }

    /* Matches the original's return value exactly: the raw player_list
     * head pointer truncated to uint32_t (same truncating-return shape as
     * EnumConnections's shim — see DirectPlay_EnumConnections below). Not
     * a session count despite a prior revision's comment claiming so. */
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(player_list));
}

/* ================================================================== */
/* DirectPlay_HandleMessages — Create DirectPlay, enumerate providers  */
/* Address: 0x45F390                                                   */
/*                                                                     */
/* Main network setup function. Creates DirectPlay object, enumerates */
/* connections, initializes address. Large state machine (~2076 bytes).*/
/* Shows connection dialog for interactive provider selection.        */
/* ================================================================== */
uint32_t DirectPlay_HandleMessages(int32_t protocol, const char* address, int32_t flags)
{
    /* This function is a large state machine that:
     * 1. Checks if DirectPlay interface exists, closes if so
     * 2. Based on connection_type (+0x518), shows dialog or auto-configures
     * 3. Creates DirectPlay via CoCreateInstance
     * 4. Enumerates service providers
     * 5. Creates/initializes DirectPlayAddress
     * 6. Calls IDirectPlay4::InitializeConnection
     *
     * The connection_type values are:
     *   1 = DirectPlay (default, shows provider selection dialog)
     *   2 = TCP/IP
     *   3 = IPX
     *   4 = Modem
     *   5 = Serial
     *
     * See native/DPLAY_* files for the dispatch implementation.
     * This function calls the Ordinal_1 load and CoCreateInstance path.
     */

    /* TODO: decompile 0x45F390 — DirectPlay_HandleMessages
     * ~2076-byte state machine. Creates DirectPlay instances, shows
     * connection dialogs, initializes address. Takes 'this' via global
     * g_dplay_peer. Called by ConnectToSession, SetSessionDesc, and (with a
     * real protocol/address override) Train_ConnectToServer. The 3 args
     * are accepted (signature now matches the real disassembly) but not
     * yet consumed by this still-deferred body. */
    (void)protocol;
    (void)address;
    (void)flags;
    return 1;  /* stub — TODO: decompile 0x45F390 */
}

/* ================================================================== */
/* DirectPlay_FindModemNameCallback — IDirectPlayLobby3A::EnumAddress   */
/* callback.                                                            */
/* Address: 0x45FBD0                                                   */
/*                                                                     */
/* Real signature confirmed via disassembly: RET 0x10 pops 4 stdcall   */
/* dwords, matching Microsoft's LPDPENUMADDRESSCALLBACK exactly         */
/* (REFGUID,DWORD,LPCVOID,LPVOID) — a 3-param (const char*,uint32_t,   */
/* const char*) "player name" callback in earlier revisions of this    */
/* file was wrong on every count: it read a 16-byte GUID (guidDataType)*/
/* as if it were a short ASCII string being compared to "", and never  */
/* declared the 4th parameter (lpContext) the real callback receives.  */
/* guidDataType is compared against DPAID_Modem (byte-verified at      */
/* 0x4790F8); if it matches and the chunk (lpData) is a non-empty      */
/* string, that string — the modem name registered with TAPI for this */
/* address, per the real dplobby.h — is copied into g_dplay_peer's     */
/* player_name_buf. Called from FindLocalModemName (0x45EEC0) via      */
/* IDirectPlayLobby3A::EnumAddress on the local player's own address    */
/* (obtained via IDirectPlay4A::GetPlayerAddress) — i.e. this looks up  */
/* the local modem's name when connected over a modem, not a player's  */
/* display name. dwDataSize/lpContext are read by neither this         */
/* function nor its one real call site (which always passes            */
/* lpContext=NULL) — real DirectPlay convention: return TRUE(1) to      */
/* continue enumeration, FALSE(0) to stop.                              */
/* ================================================================== */
BOOL STDMETHODCALLTYPE DirectPlay_FindModemNameCallback(const GUID& guidDataType, DWORD dwDataSize,
                                                         LPCVOID lpData, LPVOID lpContext)
{
    (void)dwDataSize;
    (void)lpContext;
    if (memcmp(&guidDataType, &DPAID_Modem, sizeof(GUID)) != 0) {
        return 1;  /* not the modem-name chunk: continue enumeration */
    }
    const char* modemName = reinterpret_cast<const char*>(lpData);
    if (lstrlenA(modemName) == 0) {
        return 1;  /* empty modem name: continue enumeration */
    }
    /* Copy modem name into g_dplay_peer's player_name_buf */
    char* dst = g_dplay_peer->player_name_buf;
    int32_t j = 0;
    while (modemName[j] != 0) {
        dst[j] = modemName[j];
        j++;
    }
    dst[j] = 0;
    return 0;  /* found: stop enumeration */
}

/* ================================================================== */
/* DirectPlaySession::Close                                            */
/* Address: 0x45FC30                                                   */
/* ================================================================== */
void DirectPlaySession::Close()
{
    /* Free session desc memory if allocated */
    if (session_desc_ptr != nullptr && session_desc_valid == 0) {
        void* hglb = GlobalHandle(session_desc_ptr);
        GlobalUnlock(hglb);
        hglb = GlobalHandle(session_desc_ptr);
        GlobalFree(hglb);
    }

    session_desc_valid = 0;
    session_desc_ptr = nullptr;

    /* Close player connection and release DirectPlay interface */
    if (dplay_interface != nullptr) {
        /* Cancel outstanding messages (not "close player" — see
         * dplay->CancelMessage's declaration comment) */
        dplay_interface->CancelMessage(0, 0);
        dplay_interface->Close();
        dplay_interface->Release();
        dplay_interface = nullptr;
    }

    /* Free player list */
    DirectPlayPlayerListNode* player = player_list;
    while (player != nullptr) {
        DirectPlayPlayerListNode* next = player->next;
        if (player->payload != nullptr) {
            GLOBAL_free(player->payload);
        }
        if (player->hglobal_data != nullptr) {
            void* hglb = GlobalHandle(player->hglobal_data);
            GlobalUnlock(hglb);
            hglb = GlobalHandle(player->hglobal_data);
            GlobalFree(hglb);
            player->hglobal_data = nullptr;
        }
        GLOBAL_free(player);
        player_list = next;
        player = next;
    }

    /* Free group list */
    DirectPlayGenericListNode* group = group_list;
    while (group != nullptr) {
        DirectPlayGenericListNode* next = group->next;
        if (group->payload != nullptr) {
            GLOBAL_free(group->payload);
        }
        GLOBAL_free(group);
        group_list = next;
        group = next;
    }

    /* Reset session state fields */
    session_name[0] = 0;
    player_name[0] = 0;
    connection_type = 0;
    connection_name[0] = 0;
    session_flags = 0;
    flag_934 = 0;
    session_ready = 0;
}

/* ================================================================== */
/* DirectPlaySession::OpenSession                                      */
/* Address: 0x45FD80                                                   */
/* ================================================================== */
uint32_t DirectPlaySession::OpenSession()
{
    if (dplay_interface == nullptr) {
        return 0;
    }

    /* Build the real DPSESSIONDESC2 */
    memset(&session_desc, 0, sizeof(DPSESSIONDESC2));
    session_desc.dwSize = sizeof(DPSESSIONDESC2);

    /* dwFlags formula, byte-verified via disassembly (0x45FD80): gated on
     * startup_flag (+0x002), NOT is_host (+0x001) — a prior TRANSCRIBED
     * revision of this file read the wrong field AND inverted the
     * condition (was `is_host ? 0 : 4`; real: `startup_flag ? 4 : 0`). */
    session_desc.dwFlags = (startup_flag != 0 ? 4u : 0u) + 0xA040u;

    session_desc.guidApplication = GUID_SessionDesc;
    session_desc.dwMaxPlayers    = session_flags;

    session_desc.lpszSessionNameA = session_name;
    if (session_password[0] != '\0') {
        session_desc.lpszPasswordA = session_password;
    }

    /* Open (host or join) the session */
    int32_t hr = dplay_interface->Open(&session_desc, 0x82);
    last_hresult = hr;

    /* Retry on pending */
    while (hr == -0x7788fea2 && is_host == 0) {
        Sleep(1);
        hr = dplay_interface->Open(&session_desc, 0x82);
        last_hresult = hr;
    }

    hr = last_hresult;
    if (hr == 0) {
        session_ready = 1;
        return 1;
    }

    if (hr == -0x7788fee8) {  /* DP_OK_USERCANCEL */
        return 0x88770100;
    }

    /* Error — format message */
    char err_buf[300];
    DirectPlay_Open(err_buf, hr);
    char msg_buf[300];
    wsprintfA(msg_buf, "Failed to Open new session: %s", err_buf);
    WIN32_RecvNetworkData(this, 0, msg_buf);
    return hr & 0xFFFFFF00;
}

/* ================================================================== */
/* DirectPlay_Open — Convert DPERR_* HRESULT to string                */
/* Address: 0x45FF30                                                   */
/*                                                                     */
/* Large switch over 40+ DirectPlay error codes. Unknown codes        */
/* formatted as "Unknown Error Code: %d".                             */
/* ================================================================== */
void DirectPlay_Open(char* out_buf, int32_t hresult)
{
    const char* msg;

    if (hresult < -0x7FFFBFFE) {
        if (hresult == -0x7FFFBFFF)       msg = "DPERR_UNSUPPORTED";
        else if (hresult == -0x7FFFFFF6)  msg = "DPERR_PENDING";
        else goto unknown_error;
    } else if (hresult < -0x7FFFBFFA) {
        if (hresult == -0x7FFFBFFB)       msg = "DPERR_GENERIC";
        else if (hresult == -0x7FFFBFFE)  msg = "DPERR_NOINTERFACE";
        else goto unknown_error;
    } else if (hresult < -0x7FF8FFF1) {
        if (hresult == -0x7FF8FFF2)       msg = "DPERR_NOMEMORY";
        else if (hresult == -0x7FFBFEF0)  msg = "CLASS_E_NOAGGREGATION";
        else goto unknown_error;
    } else if (hresult < -0x7788FFFA) {
        if (hresult == -0x7788FFFB)       msg = "DPERR_ALREADYINITIALIZED";
        else if (hresult == -0x7FF8FFA9)  msg = "DPERR_INVALIDPARAM";
        else goto unknown_error;
    } else if (hresult < -0x7788FFEB) {
        if (hresult == -0x7788FFEC)       msg = "DPERR_ACTIVEPLAYERS";
        else if (hresult == -0x7788FFF6)  msg = "DPERR_ACCESSDENIED";
        else goto unknown_error;
    } else if (hresult < -0x7788FFD7) {
        if (hresult == -0x7788FFD8)       msg = "DPERR_CANTADDPLAYER";
        else if (hresult == -0x7788FFE2)  msg = "DPERR_BUFFERTOOSMALL";
        else goto unknown_error;
    } else if (hresult < -0x7788FFC3) {
        if (hresult == -0x7788FFC4)       msg = "DPERR_CANTCREATEPLAYER";
        else if (hresult == -0x7788FFCE)  msg = "DPERR_CANTCREATEGROUP";
        else goto unknown_error;
    } else if (hresult < -0x7788FFAF) {
        if (hresult == -0x7788FFB0)       msg = "DPERR_CAPSNOTAVAILABLEYET";
        else if (hresult == -0x7788FFBA)  msg = "DPERR_CANTCREATESESSION";
        else goto unknown_error;
    } else if (hresult < -0x7788FF87) {
        if (hresult == -0x7788FF88)       msg = "DPERR_INVALIDFLAGS";
        else if (hresult == -0x7788FFA6)  msg = "DPERR_EXCEPTION";
        else goto unknown_error;
    } else if (hresult < -0x7788FF69) {
        if (hresult == -0x7788FF6A)       msg = "DPERR_INVALIDPLAYER";
        else if (hresult == -0x7788FF7E)  msg = "DPERR_INVALIDOBJECT";
        else goto unknown_error;
    } else if (hresult < -0x7788FF5F) {
        if (hresult == -0x7788FF60)       msg = "DPERR_NOCAPS";
        else if (hresult == -0x7788FF65)  msg = "DPERR_INVALIDGROUP";
        else goto unknown_error;
    } else if (hresult < -0x7788FF41) {
        if (hresult == -0x7788FF42)       msg = "DPERR_NOMESSAGES";
        else if (hresult == -0x7788FF56)  msg = "DPERR_NOCONNECTION";
        else goto unknown_error;
    } else if (hresult < -0x7788FF2D) {
        if (hresult == -0x7788FF2E)       msg = "DPERR_NOPLAYERS";
        else if (hresult == -0x7788FF38)  msg = "DPERR_NONAMESERVERFOUND";
        else goto unknown_error;
    } else if (hresult < -0x7788FF19) {
        if (hresult == -0x7788FF1A)       msg = "DPERR_SENDTOOBIG";
        else if (hresult == -0x7788FF24)  msg = "DPERR_NOSESSIONS";
        else goto unknown_error;
    } else if (hresult < -0x7788FF05) {
        if (hresult == -0x7788FF06)       msg = "DPERR_UNAVAILABLE";
        else if (hresult == -0x7788FF10)  msg = "DPERR_TIMEOUT";
        else goto unknown_error;
    } else if (hresult < -0x7788FEE7) {
        if (hresult == -0x7788FEE8)       msg = "DPERR_USERCANCEL";
        else if (hresult == -0x7788FEF2)  msg = "DPERR_BUSY";
        else goto unknown_error;
    } else if (hresult < -0x7788FEC9) {
        if (hresult == -0x7788FECA)       msg = "DPERR_SESSIONLOST";
        else if (hresult == -0x7788FED4)  msg = "DPERR_PLAYERLOST";
        else goto unknown_error;
    } else if (hresult < -0x7788FEB5) {
        if (hresult == -0x7788FEB6)       msg = "DPERR_NONEWPLAYERS";
        else if (hresult == -0x7788FEC0)  msg = "DPERR_UNINITIALIZED";
        else goto unknown_error;
    } else if (hresult < -0x7788FE8D) {
        if (hresult == -0x7788FE8E)       msg = "DPERR_UNKNOWNMESSAGE";
        else if (hresult == -0x7788FE98)  msg = "DPERR_CONNECTIONLOST";
        else goto unknown_error;
    } else if (hresult < -0x7788FE79) {
        if (hresult == -0x7788FE7A)       msg = "DPERR_INVALIDPRIORITY";
        else if (hresult == -0x7788FE84)  msg = "DPERR_CANCELFAILED";
        else goto unknown_error;
    } else if (hresult < -0x7788FE65) {
        if (hresult == -0x7788FE66)       msg = "DPERR_CANCELLED";
        else if (hresult == -0x7788FE70)  msg = "DPERR_NOTHANDLED";
        else goto unknown_error;
    } else if (hresult < -0x7788FC17) {
        if (hresult == -0x7788FC18)       msg = "DPERR_BUFFERTOOLARGE";
        else if (hresult == -0x7788FE5C)  msg = "DPERR_ABORTED";
        else goto unknown_error;
    } else if (hresult < -0x7788FC03) {
        if (hresult == -0x7788FC04)       msg = "DPERR_APPNOTSTARTED";
        else if (hresult == -0x7788FC0E)  msg = "DPERR_CANTCREATEPROCESS";
        else goto unknown_error;
    } else if (hresult < -0x7788FBE5) {
        if (hresult == -0x7788FBE6)       msg = "DPERR_UNKNOWNAPPLICATION";
        else if (hresult == -0x7788FBFA)  msg = "DPERR_INVALIDINTERFACE";
        else goto unknown_error;
    } else if (hresult == -0x7788FBD2) {
        msg = "DPERR_NOTLOBBIED";
    } else if (hresult == -0x7788F7EA) {
        msg = "DPERR_NOTLOGGEDIN";
    } else if (hresult == 0) {
        msg = "DP_OK";
    } else {
        goto unknown_error;
    }

    /* Copy message to output buffer */
    {
        int32_t i = 0;
        while (msg[i] != 0) {
            out_buf[i] = msg[i];
            i++;
        }
        out_buf[i] = 0;
    }
    return;

unknown_error:
    wsprintfA(out_buf, "Unknown Error Code: %d", hresult);
}
