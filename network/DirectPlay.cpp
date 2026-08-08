/**
 * DirectPlay.cpp — DirectPlay network session wrapper implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 * Status: TRANSCRIBED
 *
 * These C free functions wrap Microsoft DirectPlay 4 for multiplayer
 * session management. They operate on a large session struct (~0x15E8
 * bytes) managing COM interface pointers, player/session data, and
 * linked lists of sessions/players/groups/messages.
 *
 * Architecture note: Lego Loco's "multiplayer" is unusual — it actually
 * uses file-based PostBag directories shared between players. The
 * DirectPlay wrapper is used for connection setup and player enumeration,
 * but actual game data transfer happens through file I/O.
 */

#include "DirectPlay.h"
#include "../stubs/ddraw.h"
#include "../core/Entity.h"
#include <cstddef>
#include <cstring>
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

/* ================================================================== */
/* DirectPlay session struct (layout documented in DirectPlay.h)       */
/* ================================================================== */
struct DirectPlaySession {
    uint8_t  session_state;           // +0x000: 0=inactive, 1=host, 2=joined
    uint8_t  flag_byte_1;             // +0x001: is_host flag
    uint8_t  flag_byte_2;             // +0x002: startup flag
    uint8_t  flag_byte_3;             // +0x003
    uint8_t  flag_byte_4;             // +0x004
    uint8_t  _pad_05[0x13];           // +0x005
    char     session_name[0x400];     // +0x018: session name
    char     player_name[0x80];       // +0x418: local player name
    char     session_password[0x80];  // +0x498: session password
    int32_t  connection_type;         // +0x518: 0=IPX, 1=TCP/IP, 2=Modem, 3=Serial
    char     connection_name[0x400];  // +0x51C: provider name buffer
    int32_t  modem_baud;              // +0x91C: modem baud rate (int16_t)
    uint8_t  _pad_91E[2];             // +0x91E
    int32_t  session_flags;           // +0x920
    int32_t  player_dpid;             // +0x924: local player DPID
    uint8_t  _pad_928[4];             // +0x928
    void*    session_desc_ptr;        // +0x92C: session description data
    uint8_t  session_desc_valid;      // +0x930: 0=allocated, 1=pending
    uint8_t  _pad_931[3];             // +0x931
    int32_t  flag_934;                // +0x934
    void*    hwnd;                    // +0x938: parent window
    void*    hinstance;               // +0x93C: application instance
    void*    error_callback;          // +0x940: error display callback
    uint8_t  show_dialogs;            // +0x944: 1=show dialogs
    char     error_msg_buf[512];      // +0x945: error message buffer
    uint8_t  _pad_B45[0x203];         // +0xB45
    int32_t  last_hresult;            // +0xD48: last HRESULT
    void*    idle_callback;           // +0xD4C: periodic callback
    uint8_t  session_ready;           // +0xD50: enumeration complete
    uint8_t  _pad_D51[3];             // +0xD51
    int32_t  session_data_size;       // +0xD54
    void*    session_data_ptr;        // +0xD58
    int32_t  max_players;             // +0xD5C
    void*    session_list;            // +0xD60: linked list head
    void*    player_list;             // +0xD64: linked list head
    void*    group_list;              // +0xD68: linked list head
    void*    connection_list;         // +0xD6C: linked list head
    char     player_name_buf[0x100];  // +0xD70: enumerated player name
    uint8_t  _pad_E70[0x8A0];         // +0xE70
    int32_t  modem_settings[5];       // +0x1570: modem config
    void*    dplay_create_obj;        // +0x1584: transient IDirectPlay-family
                                       //   object from Ordinal_1 (NOT a DLL handle)
    void*    dplay_interface;         // +0x1588: IDirectPlay4A*
    uint8_t  session_desc_buf[0x50];  // +0x158C: DPSESSIONDESC2 (ANSI layout)
    void*    dplay_lobby_obj;         // +0x15DC: transient object from Ordinal_4
                                       //   (NOT "IDirectPlayAddress" — that
                                       //   interface does not exist)
    void*    dplay_lobby3a;           // +0x15E0: IDirectPlayLobby3A*
    uint8_t  session_caps[0x28];      // +0x15E4: DPCAPS (10 dwords, NOT an
                                       //   "address structure")
};

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
     * not the int32_t/void* placeholders this block used before. */
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
/* DirectPlay Ordinal exports (loaded via GetProcAddress from dplay.dll/dpmodem.dll) */
/* ================================================================== */

extern int32_t __stdcall Ordinal_1(void* ptr1, void* ptr2, void* ptr3, void* ptr4);
extern int32_t __stdcall Ordinal_4(void* ptr1, void** ptr2, void* ptr3, void* ptr4, void* ptr5);

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
/* DirectPlay_SetSessionDesc (0x45F090, calls the real EnumSessions     */
/* below with a genuine callback at 0x45F2B0) is NOT yet renamed:       */
/* EnumSessions's callback body is substantial, previously entirely     */
/* unexamined code (builds a session-list linked list conditionally on */
/* connection type) and needs its own dedicated decompilation pass      */
/* before this function's true higher-level purpose (join-precheck?    */
/* host-precheck?) can be named with confidence — see PROGRESS.md.      */
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

/* String constants */
/* 0x481218: byte is 0x00 (null terminator before "Direct Play Initiali..." string) */
extern const char g_device_path_null[4];  /* 4 zero bytes at 0x481218 */

/* ================================================================== */
/* DirectPlay_SessionMgr — Walk tree storing IDs and directions       */
/* Address: 0x45DA70                                                   */
/*                                                                     */
/* Delegate to AssetMgr's implementation in AssetMgr.cpp              */
/* ================================================================== */
/* NOTE: Implemented in AssetMgr.cpp:805.
 * This function logically belongs to AssetMgr (tree walk).
 * Declaration kept here for cross-reference purposes only. */

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
/* DirectPlay_CreatePeer — Create DirectPlay peer                     */
/* Address: 0x45E490                                                   */
/*                                                                     */
/* Wrapper: calls CreateAddress and returns this.                     */
/* ================================================================== */
void* DirectPlay_CreatePeer(void* self, uint32_t hInstance, uint32_t hWnd)
{
    DirectPlay_CreateAddress(self, hInstance, hWnd);
    return self;
}

/* ================================================================== */
/* DirectPlay_CreateAddress — Initialize DirectPlay address           */
/* Address: 0x45E4B0                                                   */
/*                                                                     */
/* Sets session fields, creates DirectPlayAddress COM object,         */
/* queries IDirectPlayAddress interface.                               */
/* ================================================================== */
void DirectPlay_CreateAddress(void* self, uint32_t hInstance, uint32_t hWnd)
{
    uint8_t* s = static_cast<uint8_t*>(self);

    /* Initialize fields */
    s[0x944] = 1;        /* show_dialogs = true */
    s[2] = 1;            /* flag_byte_2 = 1 (startup) */
    *reinterpret_cast<uint32_t*>(s + 0xD4C) = 0;      /* idle_callback */
    *reinterpret_cast<uint32_t*>(s + 0x940) = 0;       /* error_callback */
    s[0x945] = 0;         /* error_msg_buf[0] = 0 */
    *reinterpret_cast<uint32_t*>(s + 0xD48) = 0;       /* last_hresult */
    s[0xD50] = 0;         /* session_ready = 0 */
    s[0] = 0;             /* session_state = 0 (inactive) */
    s[1] = 0;             /* flag_byte_1 = 0 */
    *reinterpret_cast<uint32_t*>(s + 0x920) = 0;       /* session_flags */
    s[3] = 0;             /* flag_byte_3 */
    s[4] = 0;             /* flag_byte_4 */
    *reinterpret_cast<uint32_t*>(s + 0x1588) = 0;      /* dplay_interface */
    s[0x18] = 0;          /* session_name[0] = 0 */
    *reinterpret_cast<uint32_t*>(s + 0x92C) = 0;       /* session_desc_ptr */
    s[0x930] = 0;         /* session_desc_valid */
    s[0x418] = 0;         /* player_name[0] = 0 */
    *reinterpret_cast<uint32_t*>(s + 0x924) = 0;       /* player_dpid */
    *reinterpret_cast<uint32_t*>(s + 0x518) = 0;       /* connection_type */
    *reinterpret_cast<uint32_t*>(s + 0x93C) = hInstance; /* hinstance */
    *reinterpret_cast<uint32_t*>(s + 0x938) = hWnd; /* hwnd */
    *reinterpret_cast<uint32_t*>(s + 0xD54) = 0;       /* session_data_size */
    *reinterpret_cast<uint32_t*>(s + 0xD58) = 0;       /* session_data_ptr */
    *reinterpret_cast<uint32_t*>(s + 0xD60) = 0;       /* session_list */
    *reinterpret_cast<uint32_t*>(s + 0xD64) = 0;       /* player_list */
    *reinterpret_cast<uint32_t*>(s + 0xD68) = 0;       /* group_list */
    *reinterpret_cast<uint32_t*>(s + 0xD6C) = 0;       /* connection_list */
    *reinterpret_cast<uint32_t*>(s + 0xD5C) = 10;      /* max_players = 10 */

    /* Obtain the transient lobby object (Ordinal_4), then QueryInterface it
     * up to IDirectPlayLobby3A — see the file-header comment on why this
     * is not "IDirectPlayAddress" (that interface doesn't exist). */
    *reinterpret_cast<void**>(s + 0x15DC) = nullptr;
    *reinterpret_cast<void**>(s + 0x15E0) = nullptr;   /* dplay_lobby3a */

    int32_t hr = Ordinal_4(nullptr, reinterpret_cast<void**>(s + 0x15DC), nullptr, nullptr, nullptr);
    *reinterpret_cast<uint32_t*>(s + 0xD48) = hr;

    IDirectPlayLobby3A* lobbyObj = *reinterpret_cast<IDirectPlayLobby3A**>(s + 0x15DC);
    if (hr == 0 && lobbyObj != NULL) {
        hr = lobbyObj->QueryInterface(IID_IDirectPlayLobby3A, reinterpret_cast<void**>(s + 0x15E0));
        *reinterpret_cast<uint32_t*>(s + 0xD48) = hr;
    }

    /* Release the initial lobby object */
    if (lobbyObj != NULL) {
        lobbyObj->Release();
    }
    *reinterpret_cast<void**>(s + 0x15DC) = nullptr;
}

/* ================================================================== */
/* DirectPlay_DestroyPeer — Full DirectPlay peer teardown             */
/* Address: 0x45E5A0                                                   */
/*                                                                     */
/* Closes session, releases peer, and frees all linked lists.         */
/* ================================================================== */
void DirectPlay_DestroyPeer(int32_t session)
{
    uint8_t* s = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(session));

    /* Close session */
    DirectPlay_Close(session);

    /* Release dplay_lobby3a */
    IDirectPlayLobby3A* lobby3a = *reinterpret_cast<IDirectPlayLobby3A**>(s + 0x15E0);
    if (lobby3a != NULL) {
        lobby3a->Release();
        *reinterpret_cast<void**>(s + 0x15E0) = nullptr;
    }

    /* Free session data */
    if (*reinterpret_cast<void**>(s + 0xD58) != nullptr) {
        GLOBAL_free(*reinterpret_cast<void**>(s + 0xD58));
        *reinterpret_cast<uint32_t*>(s + 0xD58) = 0;
        *reinterpret_cast<uint32_t*>(s + 0xD54) = 0;
    }

    /* Free session list (2 linked fields per entry: next_ptr + data_ptr) */
    int32_t* list_item = *reinterpret_cast<int32_t**>(s + 0xD60);
    while (list_item != nullptr) {
        int32_t* next = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(*list_item));
        if (reinterpret_cast<void*>(static_cast<uintptr_t>(list_item[2])) != nullptr) {
            GLOBAL_free(reinterpret_cast<void*>(static_cast<uintptr_t>(list_item[2])));
        }
        GLOBAL_free(list_item);
        *reinterpret_cast<int32_t**>(s + 0xD60) = next;
        list_item = next;
    }

    /* Free player list (linked list with next + data + hGlobal mem) */
    list_item = *reinterpret_cast<int32_t**>(s + 0xD64);
    while (list_item != nullptr) {
        int32_t* next = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(*list_item));
        if (reinterpret_cast<void*>(static_cast<uintptr_t>(list_item[2])) != nullptr) {
            GLOBAL_free(reinterpret_cast<void*>(static_cast<uintptr_t>(list_item[2])));
        }
        if (reinterpret_cast<void*>(static_cast<uintptr_t>(list_item[1])) != nullptr) {
            void* hglb = GlobalHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(list_item[1])));
            GlobalUnlock(hglb);
            hglb = GlobalHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(list_item[1])));
            GlobalFree(hglb);
            list_item[1] = 0;
        }
        GLOBAL_free(list_item);
        *reinterpret_cast<int32_t**>(s + 0xD64) = next;
        list_item = next;
    }

    /* Free group list */
    list_item = *reinterpret_cast<int32_t**>(s + 0xD68);
    while (list_item != nullptr) {
        int32_t* next = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(*list_item));
        if (reinterpret_cast<void*>(static_cast<uintptr_t>(list_item[2])) != nullptr) {
            GLOBAL_free(reinterpret_cast<void*>(static_cast<uintptr_t>(list_item[2])));
        }
        GLOBAL_free(list_item);
        *reinterpret_cast<int32_t**>(s + 0xD68) = next;
        list_item = next;
    }

    /* Free message list */
    list_item = *reinterpret_cast<int32_t**>(s + 0xD6C);
    while (list_item != nullptr) {
        int32_t* next = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(*list_item));
        GLOBAL_free(list_item);
        *reinterpret_cast<int32_t**>(s + 0xD6C) = next;
        list_item = next;
    }
}

/* ================================================================== */
/* DirectPlay_HostSession — Store host configuration                  */
/* Address: 0x45E700                                                   */
/*                                                                     */
/* Simple setter: stores 4 configuration parameters.                  */
/* ================================================================== */
void DirectPlay_HostSession(void* self, uint8_t sessionState, uint32_t sessionFlags,
                                uint8_t isHost, uint8_t startupFlag)
{
    uint8_t* s = static_cast<uint8_t*>(self);
    s[0] = sessionState;             /* session_state */
    *reinterpret_cast<uint32_t*>(s + 0x920) = sessionFlags;  /* session_flags */
    s[1] = isHost;             /* flag_byte_1 (is_host) */
    s[2] = startupFlag;             /* flag_byte_2 */
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
/* DirectPlay_ConnectToSession — Join a DirectPlay session            */
/* Address: 0x45E730                                                   */
/*                                                                     */
/* Stores player name, session name, password. Creates DirectPlay     */
/* player, sends session desc, enumerates players. Returns 1 on       */
/* success. Called from Train connection code.                        */
/* ================================================================== */
uint8_t DirectPlay_ConnectToSession(void* self, const char* playerName,
                                        const char* sessionName,
                                        const char* password)
{
    uint8_t* s = static_cast<uint8_t*>(self);

    /* Clear session name, copy if provided */
    s[0x18] = 0;
    if (sessionName != nullptr) {
        int32_t i = 0;
        while (sessionName[i] != 0 && i < 0x3FF) {
            s[0x18 + i] = static_cast<uint8_t>(sessionName[i]);
            i++;
        }
        s[0x18 + i] = 0;
    }

    /* Copy player name (max 0x80 bytes). The original temporarily
     * terminates the source at +0x80 and restores it; copying into the
     * fixed destination directly has the same observable result without
     * casting away constness from the caller's buffer. */
    char* name_dst = reinterpret_cast<char*>(s + 0x418);
    copy_session_name(name_dst, playerName, 0x80);

    /* Copy password (max 0x80 bytes, same logic) */
    char* pwd_dst = reinterpret_cast<char*>(s + 0x498);
    copy_session_name(pwd_dst, password, 0x80);

    // DECOMPILER NOTE: verify s[0x498] = 0 against disasm at 0x45E840.
    // This clears password[0] after copy; may target a different field.
    s[0x498] = 0;

    /* --- Connection logic --- */
    uint8_t result = 0;

    if (s[0] == 0) {
        /* Not hosting: try to join */
        /* Handle messages to create DirectPlay instance */
        uint32_t hr = DirectPlay_HandleMessages(0, nullptr, 0);
        if (static_cast<uint8_t>(hr) == 0) {
            DirectPlay_Close(static_cast<int32_t>(reinterpret_cast<intptr_t>(self)));
            return 0;
        }

        /* Get system metrics / open session */
        uint32_t enum_result = WIN32_GetSystemMetrics(self);
        if (static_cast<uint8_t>(enum_result) == 0) {
            DirectPlay_Close(static_cast<int32_t>(reinterpret_cast<intptr_t>(self)));
            return 0;
        }

        /* Join session via DirectPlay CreatePlayer */
        if (*reinterpret_cast<void**>(s + 0x1588) != nullptr) {
            int32_t desc_size = 0x10;
            /* Cast preserves the exact pre-existing argument values;
             * whether desc_size/&g_empty_string genuinely match
             * CreatePlayer's real (LPDPNAME,HANDLE,LPVOID) parameter
             * types is a pre-existing question unrelated to this pass
             * (which only replaces manual vtable dispatch with the real
             * typed virtual call). */
            IDirectPlay4A* dplayIface = *reinterpret_cast<IDirectPlay4A**>(s + 0x1588);
            int32_t hr2 = dplayIface->CreatePlayer(reinterpret_cast<DPID*>(s + 0x924),
                                                    reinterpret_cast<LPDPNAME>(&desc_size),
                                                    reinterpret_cast<HANDLE>(
                                                        reinterpret_cast<uintptr_t>(&g_empty_string)),
                                                    name_dst, 0, 0);
            *reinterpret_cast<int32_t*>(s + 0xD48) = hr2;

            if (hr2 == 0) {
                result = 1;  /* joined successfully */
            } else {
                char err_buf[300];
                char err_msg[300];
                DirectPlay_Open(err_buf, hr2);
                wsprintfA(err_msg, "Failed to join member '%s' to session: %s",
                          name_dst, err_buf);
                WIN32_RecvNetworkData(self, 0, err_msg);
                result = 0;
            }
        }

        if (!result) {
            DirectPlay_Close(static_cast<int32_t>(reinterpret_cast<intptr_t>(self)));
            return 0;
        }
    } else {
        /* Hosting: handle messages then enumerate players */
        uint32_t hr = DirectPlay_HandleMessages(0, nullptr, 0);
        if (static_cast<uint8_t>(hr) == 0) {
            DirectPlay_Close(static_cast<int32_t>(reinterpret_cast<intptr_t>(self)));
            return 0;
        }

        uint32_t enum_result = DirectPlay_OpenSession(self);
        if (static_cast<uint8_t>(enum_result) != 0) {
            /* Enumerate succeeded */
            if (*reinterpret_cast<void**>(s + 0x1588) != nullptr) {
                int32_t desc_size = 0x10;
                IDirectPlay4A* dplayIface = *reinterpret_cast<IDirectPlay4A**>(s + 0x1588);
                int32_t hr2 = dplayIface->CreatePlayer(reinterpret_cast<DPID*>(s + 0x924),
                                                        reinterpret_cast<LPDPNAME>(&desc_size),
                                                        reinterpret_cast<HANDLE>(
                                                            reinterpret_cast<uintptr_t>(&g_empty_string)),
                                                        name_dst, 0, 0);
                *reinterpret_cast<int32_t*>(s + 0xD48) = hr2;

                if (hr2 == 0) {
                    result = 1;
                } else {
                    char err_buf[300];
                    char err_msg[300];
                    DirectPlay_Open(err_buf, hr2);
                    wsprintfA(err_msg, "Failed to join member '%s' to session: %s",
                              name_dst, err_buf);
                    WIN32_RecvNetworkData(self, 0, err_msg);
                    result = 0;
                }
            }

            if (!result) {
                DirectPlay_Close(static_cast<int32_t>(reinterpret_cast<intptr_t>(self)));
                return 0;
            }
        } else {
            /* Enumeration failed — check if error is DPERR_USERCANCEL */
            if (s[1] != 0 && *reinterpret_cast<int32_t*>(s + 0xD48) == -0x7788fea2) {
                /* DPERR_PENDING — user cancelled */
                return 0;
            }
            DirectPlay_Close(static_cast<int32_t>(reinterpret_cast<intptr_t>(self)));
            return 0;
        }
    }

    /* On success: query the real DPCAPS at +0x15E4 (0x28 bytes) */
    if (result != 0 && *reinterpret_cast<void**>(s + 0x1588) != nullptr) {
        DPCAPS* caps = reinterpret_cast<DPCAPS*>(s + 0x15E4);
        memset(caps, 0, sizeof(DPCAPS));
        caps->dwSize = sizeof(DPCAPS);
        IDirectPlay4A* dplayIface = *reinterpret_cast<IDirectPlay4A**>(s + 0x1588);
        dplayIface->GetCaps(caps, 0);
    }

    return result;
}

/* ================================================================== */
/* DirectPlay_EnumConnections — Enumerate DirectPlay providers        */
/* Address: 0x45EAB0                                                   */
/*                                                                     */
/* Tries 4 service providers (TCP/IP, IPX, Modem, Serial). Creates    */
/* DirectPlay object for each, queries interface, stores connections   */
/* in linked list. Checks device availability via CreateFile.         */
/* ================================================================== */
int32_t DirectPlay_EnumConnections(int32_t session)
{
    uint8_t* s = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(session));

    /* Return existing list if already enumerated */
    if (*reinterpret_cast<int32_t*>(s + 0xD6C) != 0) {
        return *reinterpret_cast<int32_t*>(s + 0xD6C);
    }

    IDirectPlay4A** dplay_ptr = reinterpret_cast<IDirectPlay4A**>(s + 0x1588);
    if (*dplay_ptr != NULL) {
        return 0;  /* already connected */
    }

    /* Get session desc if possible */
    bool haveSession = false;
    if (DirectPlay_FindLocalModemName(session)) {
        /* Store connection result */
        uint32_t* entry = static_cast<uint32_t*>(operator_new(8));
        entry[0] = *reinterpret_cast<uint32_t*>(s + 0xD6C);
        entry[1] = 1;  /* type = TCP/IP? */
        *reinterpret_cast<uint32_t**>(s + 0xD6C) = entry;
        haveSession = true;
    }

    /* Try provider 1 (TCP/IP typically) via Ordinal_1 */
    IDirectPlay4A* dplay_dll[2];
    int32_t hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    if (hr == 0) {
        /* Query for IDirectPlay4 interface via IID_IDirectPlay4A */
        hr = dplay_dll[0]->QueryInterface(IID_IDirectPlay4A, reinterpret_cast<void**>(dplay_ptr));
        *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

        if (hr == 0) {
            /* Success — release DLL handle, store connection */
            dplay_dll[0]->Release();
            dplay_dll[0] = nullptr;

            /* Release interface */
            (*dplay_ptr)->Release();
            *dplay_ptr = nullptr;

            uint32_t* entry = static_cast<uint32_t*>(operator_new(8));
            entry[0] = *reinterpret_cast<uint32_t*>(s + 0xD6C);
            entry[1] = 4;  /* type */
            *reinterpret_cast<uint32_t**>(s + 0xD6C) = entry;
            goto check_provider_2;
        }
        /* Release DLL handle */
        dplay_dll[0]->Release();
        dplay_dll[0] = nullptr;
    }

check_provider_2:
    /* Try provider 2 (IPX) */
    hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    if (hr == 0) {
        hr = dplay_dll[0]->QueryInterface(IID_IDirectPlay4A, reinterpret_cast<void**>(dplay_ptr));
        *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

        if (hr == 0) {
            dplay_dll[0]->Release();
            dplay_dll[0] = nullptr;
            (*dplay_ptr)->Release();
            *dplay_ptr = nullptr;

            uint32_t* entry = static_cast<uint32_t*>(operator_new(8));
            entry[0] = *reinterpret_cast<uint32_t*>(s + 0xD6C);
            entry[1] = 2;
            *reinterpret_cast<uint32_t**>(s + 0xD6C) = entry;
        } else {
            dplay_dll[0]->Release();
            dplay_dll[0] = nullptr;
        }
    }

    /* Check device presence via CreateFile (for Modem/Serial devices) */

    /* Try provider 3 */
    hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    if (hr == 0) {
        hr = dplay_dll[0]->QueryInterface(IID_IDirectPlay4A, reinterpret_cast<void**>(dplay_ptr));
        *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

        if (hr != 0) {
            dplay_dll[0]->Release();
            dplay_dll[0] = nullptr;
            return *reinterpret_cast<int32_t*>(s + 0xD6C);
        }

        dplay_dll[0]->Release();
        dplay_dll[0] = nullptr;
        (*dplay_ptr)->Release();
        *dplay_ptr = nullptr;

        uint32_t* entry = static_cast<uint32_t*>(operator_new(8));
        entry[0] = *reinterpret_cast<uint32_t*>(s + 0xD6C);
        entry[1] = 3;
        *reinterpret_cast<uint32_t**>(s + 0xD6C) = entry;
    }

    return *reinterpret_cast<int32_t*>(s + 0xD6C);
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
    /* Copy DAT_00481214 (3 bytes) to local_path+1..3 */
    // Copy dword from 0x481214 ("COMn" string fragment) into path buffer
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
/* DirectPlay_FindLocalModemName — look up the local modem's name       */
/* Address: 0x45EEC0                                                   */
/*                                                                     */
/* GetPlayerAddress + EnumAddress(DPAID_Modem) — see the header comment */
/* and DirectPlay_FindModemNameCallback for the full evidence trail.    */
/* Renamed from "DirectPlay_GetSessionDesc" (wrong: no session desc is  */
/* queried anywhere in this function).                                  */
/* ================================================================== */
bool DirectPlay_FindLocalModemName(int32_t session)
{
    uint8_t* s = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(session));

    if (*reinterpret_cast<void**>(s + 0x1588) != nullptr) {
        return false;  /* already connected */
    }

    /* Create DirectPlay via Ordinal_1 */
    IDirectPlay4A** dplay_dll = reinterpret_cast<IDirectPlay4A**>(s + 0x1584);
    int32_t hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;
    if (hr != 0) return false;

    /* Query IDirectPlay4 interface via IID_IDirectPlay4A */
    hr = dplay_dll[0]->QueryInterface(IID_IDirectPlay4A, reinterpret_cast<void**>(s + 0x1588));
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;
    if (hr != 0) {
        dplay_dll[0]->Release();
        dplay_dll[0] = nullptr;
        return false;
    }

    /* Release DLL handle */
    dplay_dll[0]->Release();
    dplay_dll[0] = nullptr;

    /* Query the local player's own address */
    IDirectPlay4A* dplay = *reinterpret_cast<IDirectPlay4A**>(s + 0x1588);
    DWORD desc_size = 0;
    dplay->GetPlayerAddress(0, NULL, &desc_size);

    /* Allocate + lock global memory for the address blob */
    void* hMem = GlobalAlloc(0x42, desc_size);
    void* pMem = GlobalLock(hMem);

    hr = dplay->GetPlayerAddress(0, pMem, &desc_size);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    if (hr != 0) {
        if (pMem != nullptr) {
            void* hglb = GlobalHandle(pMem);
            GlobalUnlock(hglb);
            hglb = GlobalHandle(pMem);
            GlobalFree(hglb);
        }
        dplay->Release();
        *reinterpret_cast<void**>(s + 0x1588) = nullptr;
        return false;
    }

    /* Walk the address for the DPAID_Modem chunk */
    s[0xD70] = 0;  /* Clear player name buffer */
    IDirectPlayLobby3A* lobby3a = *reinterpret_cast<IDirectPlayLobby3A**>(s + 0x15E0);
    hr = lobby3a->EnumAddress(&DirectPlay_FindModemNameCallback, pMem, desc_size, NULL);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    /* Cleanup */
    if (pMem != nullptr) {
        void* hglb = GlobalHandle(pMem);
        GlobalUnlock(hglb);
        hglb = GlobalHandle(pMem);
        GlobalFree(hglb);
    }
    dplay->Release();
    *reinterpret_cast<void**>(s + 0x1588) = nullptr;

    return s[0xD70] != 0;  /* true if a modem name was found */
}

/* ================================================================== */
/* DirectPlay_SetSessionDesc — Set session description and host        */
/* Address: 0x45F090                                                   */
/*                                                                     */
/* Stores password, clears old player list, creates session via        */
/* DirectPlay CreateSession, retries on pending.                      */
/* ================================================================== */
uint32_t DirectPlay_SetSessionDesc(void* self, const char* password)
{
    uint8_t* s = static_cast<uint8_t*>(self);

    /* Copy password to buffer at +0x498 (max 0x80 bytes, same pattern) */
    char* pwd_dst = reinterpret_cast<char*>(s + 0x498);
    *pwd_dst = 0;
    if (password != nullptr) {
        int32_t len = 0;
        while (password[len] != 0) len++;
        if (len < 0x80) {
            for (int32_t i = 0; i <= len; i++) {
                pwd_dst[i] = password[i];
            }
        } else {
            char saved = password[0x80];
            const_cast<char*>(password)[0x80] = 0;
            int32_t i = 0;
            while (password[i] != 0 && i < 0x80) {
                pwd_dst[i] = password[i];
                i++;
            }
            pwd_dst[i] = 0;
            const_cast<char*>(password)[0x80] = saved;
        }
    }

    /* Clear player list if not host */
    if (s[1] == 0) {
        int32_t* player = *reinterpret_cast<int32_t**>(s + 0xD64);
        while (player != nullptr) {
            int32_t* next = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(*player));
            if (reinterpret_cast<void*>(static_cast<uintptr_t>(player[2])) != nullptr) {
                GLOBAL_free(reinterpret_cast<void*>(static_cast<uintptr_t>(player[2])));
            }
            if (reinterpret_cast<void*>(static_cast<uintptr_t>(player[1])) != nullptr) {
                void* hglb = GlobalHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(player[1])));
                GlobalUnlock(hglb);
                hglb = GlobalHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(player[1])));
                GlobalFree(hglb);
                player[1] = 0;
            }
            GLOBAL_free(player);
            *reinterpret_cast<int32_t**>(s + 0xD64) = next;
            player = next;
        }
    }

    /* Handle messages to create DirectPlay */
    uint32_t hr = DirectPlay_HandleMessages(0, nullptr, 0);
    if (static_cast<uint8_t>(hr) == 0) {
        return 0;
    }

    /* Build a real DPSESSIONDESC2 at +0x158C (0x50 bytes) — see
     * stubs/dplay.h for the layout; loco.exe's own dword-indexed writes
     * (fixed via a raw pointer cast rather than field access in earlier
     * revisions of this file) matched this real struct exactly. */
    LPDPSESSIONDESC2 desc = reinterpret_cast<LPDPSESSIONDESC2>(s + 0x158C);
    memset(desc, 0, sizeof(DPSESSIONDESC2));
    desc->dwSize          = sizeof(DPSESSIONDESC2);
    desc->guidApplication = GUID_SessionDesc;

    if (*reinterpret_cast<char*>(s + 0x498) != 0) {
        desc->lpszPasswordA = reinterpret_cast<LPSTR>(s + 0x498);
    }

    /* Enumerate matching sessions via DirectPlay (NOT "Open"/CreateSession —
     * see the vtable-slot comment above; the callback's own body at
     * 0x45F2B0 is still a deferred stub) */
    IDirectPlay4A* dplay = *reinterpret_cast<IDirectPlay4A**>(s + 0x1588);
    int32_t hr2 = dplay->EnumSessions(desc, 0, &DirectPlay_EnumSessionsCallback,
                                       reinterpret_cast<LPVOID>(static_cast<uintptr_t>(
                                           *reinterpret_cast<uint32_t*>(s + 0x938))), 0x81);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr2;

    /* Retry on DPERR_PENDING */
    while (hr2 == -0x7788fea2) {  /* DPERR_PENDING */
        if (s[1] != 0) {          /* flag set to abort */
            return *reinterpret_cast<uint32_t*>(s + 0xD64);
        }
        if (*reinterpret_cast<void**>(s + 0xD4C) != nullptr) {
            reinterpret_cast<void (*)(void)>(
                static_cast<uintptr_t>(*reinterpret_cast<uint32_t*>(s + 0xD4C)))();  /* idle callback */
        }
        Sleep(1);

        hr2 = dplay->EnumSessions(desc, 0, &DirectPlay_EnumSessionsCallback,
                                   reinterpret_cast<LPVOID>(static_cast<uintptr_t>(
                                       *reinterpret_cast<uint32_t*>(s + 0x938))), 0x81);
        *reinterpret_cast<int32_t*>(s + 0xD48) = hr2;
    }

    return *reinterpret_cast<uint32_t*>(s + 0xD64);  /* return session count pointer */
}

/* ================================================================== */
/* DirectPlay_HandleMessages — Create DirectPlay instance              */
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
/* address, per the real dplobby.h — is copied into g_dplay_peer+0xD70.*/
/* Called from DirectPlay_FindLocalModemName (0x45EEC0) via                */
/* IDirectPlayLobby3A::EnumAddress on the local player's own address    */
/* (obtained via IDirectPlay4A::GetPlayerAddress) — i.e. this looks up  */
/* the local modem's name when connected over a modem, not a player's  */
/* display name. dwDataSize/lpContext are read by neither this         */
/* function nor its one real call site (which always passes            */
/* lpContext=NULL) — real DirectPlay convention: return TRUE(1) to      */
/* continue enumeration, FALSE(0) to stop (previous revisions had the   */
/* return-value comments backwards, though the numeric values          */
/* themselves were already correct).                                   */
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
    /* Copy modem name into g_dplay_peer + 0xD70 */
    char* dst = reinterpret_cast<char*>(static_cast<uint8_t*>(g_dplay_peer) + 0xD70);
    int32_t j = 0;
    while (modemName[j] != 0) {
        dst[j] = modemName[j];
        j++;
    }
    dst[j] = 0;
    return 0;  /* found: stop enumeration */
}

/* ================================================================== */
/* DirectPlay_Close — Close session and free resources                 */
/* Address: 0x45FC30                                                   */
/*                                                                     */
/* Releases session desc, player handle, interface, frees lists.      */
/* ================================================================== */
void DirectPlay_Close(int32_t session)
{
    uint8_t* s = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(session));

    /* Free session desc memory if allocated */
    if (*reinterpret_cast<void**>(s + 0x92C) != nullptr && s[0x930] == 0) {
        void* hglb = GlobalHandle(*reinterpret_cast<void**>(s + 0x92C));
        GlobalUnlock(hglb);
        hglb = GlobalHandle(*reinterpret_cast<void**>(s + 0x92C));
        GlobalFree(hglb);
    }

    s[0x930] = 0;  /* session_desc_valid */
    *reinterpret_cast<uint32_t*>(s + 0x92C) = 0;

    /* Close player connection and release DirectPlay interface */
    if (*reinterpret_cast<void**>(s + 0x1588) != nullptr) {
        IDirectPlay4A* dplay = *reinterpret_cast<IDirectPlay4A**>(s + 0x1588);

        /* Cancel outstanding messages (not "close player" — see
         * dplay->CancelMessage's declaration comment) */
        dplay->CancelMessage(0, 0);

        /* Close session */
        dplay->Close();

        /* Release interface */
        dplay->Release();

        *reinterpret_cast<void**>(s + 0x1588) = nullptr;
    }

    /* Free player list */
    int32_t* player = *reinterpret_cast<int32_t**>(s + 0xD64);
    while (player != nullptr) {
        int32_t* next = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(*player));
        if (reinterpret_cast<void*>(static_cast<uintptr_t>(player[2])) != nullptr) {
            GLOBAL_free(reinterpret_cast<void*>(static_cast<uintptr_t>(player[2])));
        }
        if (reinterpret_cast<void*>(static_cast<uintptr_t>(player[1])) != nullptr) {
            void* hglb = GlobalHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(player[1])));
            GlobalUnlock(hglb);
            hglb = GlobalHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(player[1])));
            GlobalFree(hglb);
            player[1] = 0;
        }
        GLOBAL_free(player);
        *reinterpret_cast<int32_t**>(s + 0xD64) = next;
        player = next;
    }

    /* Free group list */
    player = *reinterpret_cast<int32_t**>(s + 0xD68);
    while (player != nullptr) {
        int32_t* next = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(*player));
        if (reinterpret_cast<void*>(static_cast<uintptr_t>(player[2])) != nullptr) {
            GLOBAL_free(reinterpret_cast<void*>(static_cast<uintptr_t>(player[2])));
        }
        GLOBAL_free(player);
        *reinterpret_cast<int32_t**>(s + 0xD68) = next;
        player = next;
    }

    /* Reset session state fields */
    s[0x18] = 0;           /* session_name */
    s[0x418] = 0;          /* player_name */
    *reinterpret_cast<uint32_t*>(s + 0x518) = 0;    /* connection_type */
    s[0x51C] = 0;         /* connection_name */
    *reinterpret_cast<uint32_t*>(s + 0x920) = 0;    /* session_flags */
    *reinterpret_cast<uint32_t*>(s + 0x934) = 0;    /* flag_934 */
    s[0xD50] = 0;          /* session_ready */
}

/* ================================================================== */
/* DirectPlay_OpenSession — open (host or join) a DirectPlay session   */
/* Address: 0x45FD80                                                   */
/*                                                                     */
/* Builds a session desc, calls IDirectPlay4A::Open (real vtable       */
/* offset 0x60 — confirmed via disassembly), retries on pending.        */
/* Renamed from "DirectPlay_EnumPlayers" (wrong: real EnumPlayers is a  */
/* different, unused vtable slot). Returns 1 on success.               */
/* ================================================================== */
uint32_t DirectPlay_OpenSession(void* self)
{
    uint8_t* s = static_cast<uint8_t*>(self);

    if (*reinterpret_cast<void**>(s + 0x1588) == nullptr) {
        return 0;
    }

    /* Build the real DPSESSIONDESC2 at +0x158C */
    LPDPSESSIONDESC2 desc = reinterpret_cast<LPDPSESSIONDESC2>(s + 0x158C);
    memset(desc, 0, sizeof(DPSESSIONDESC2));
    desc->dwSize = sizeof(DPSESSIONDESC2);

    uint8_t is_host = s[2];
    desc->dwFlags = (is_host ? 0 : 4) + 0xA040;

    desc->guidApplication = GUID_SessionDesc;
    desc->dwMaxPlayers    = *reinterpret_cast<uint32_t*>(s + 0x920);

    desc->lpszSessionNameA = reinterpret_cast<LPSTR>(s + 0x18);
    if (*reinterpret_cast<char*>(s + 0x498) != '\0') {
        desc->lpszPasswordA = reinterpret_cast<LPSTR>(s + 0x498);
    }

    /* Open (host or join) the session */
    IDirectPlay4A* dplay = *reinterpret_cast<IDirectPlay4A**>(s + 0x1588);
    int32_t hr = dplay->Open(desc, 0x82);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    /* Retry on pending */
    while (hr == -0x7788fea2 && s[1] == 0) {
        Sleep(1);
        hr = dplay->Open(desc, 0x82);
        *reinterpret_cast<int32_t*>(s + 0xD48) = hr;
    }

    hr = *reinterpret_cast<int32_t*>(s + 0xD48);
    if (hr == 0) {
        s[0xD50] = 1;  /* session_ready */
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
    WIN32_RecvNetworkData(self, 0, msg_buf);
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
