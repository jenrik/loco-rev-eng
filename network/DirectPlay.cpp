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
#include "../core/Entity.h"
#include <cstddef>
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */

/* DirectPlay COM interface GUIDs (from .rdata section of loco.exe) */
/*
 * GUID layout in memory (little-endian): Data1(4) Data2(2) Data3(2) Data4(8)
 * CLSID_DirectPlay @ 0x478F88: {0AB1C531-4745-11D1-A7A1-0000F803ABFC}
 * Session GUID     @ 0x479158: {F9CD2546-577F-11D2-9426-00A0244BDA7A}
 * IID_IDirPlayAddr2 @ 0x479048: {2DB72491-652C-11D1-A7A8-0000F803ABFC}
 */
/* CLSID_DirectPlay: used to query IDirectPlay4 interface via CoCreateInstance/Ordinal_1 */
static const uint32_t CLSID_DirectPlay[4] = {0x0ab1c531, 0x11d14745, 0x0000a1a7, 0xfcab03f8};
/* GUID for DPSESSIONDESC2: used in IDirectPlay4::Open / CreateSession calls */
static const uint32_t GUID_SessionDesc[4]  = {0xf9cd2546, 0x11d2577f, 0xa0002694, 0x7ada4b24};

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
    void*    dplay_dll_handle;        // +0x1584: DLL handle
    void*    dplay_interface;         // +0x1588: IDirectPlay4*
    uint8_t  session_desc_buf[0x50];  // +0x158C: DP session desc (0x50 bytes)
    void*    dplay_address;           // +0x15DC: IDirectPlayAddress*
    void*    dplay_address2;          // +0x15E0: IDirectPlayAddress2*
    uint8_t  addr_struct[0x28];       // +0x15E4: DP Address (0x28 bytes)
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
    int32_t __stdcall PlaySoundA(const char* pszSound, void* hmod, uint32_t fdwSound);
    int32_t __stdcall GetSystemMetrics(int32_t nIndex);
    int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);
    int32_t __stdcall LoadStringA(void* hInstance, uint32_t uID, char* lpBuffer, int32_t cchBufferMax);
    int32_t __stdcall MessageBoxA(void* hWnd, const char* lpText, const char* lpCaption, uint32_t uType);
    int32_t __stdcall DialogBoxParamA(void* hInstance, const char* lpTemplateName,
                                       void* hWndParent, void* lpDialogFunc, int32_t dwInitParam);
    int32_t __stdcall CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess,
                                   uint32_t dwShareMode, void* lpSecurityAttributes,
                                   uint32_t dwCreationDisposition,
                                   uint32_t dwFlagsAndAttributes, void* hTemplateFile);
    int32_t __stdcall CloseHandle(void* hObject);
    void    __stdcall Sleep(uint32_t dwMilliseconds);
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
/* COM interface vtable helpers — inline wrappers for TRANSCRIBED     */
/* (will be replaced by proper IDirectDrawSurface/IDirectPlay classes */
/*  during the INTEGRATED pass)                                        */
/* ================================================================== */

/* Fetch slot `index` from a COM vtable (an array of void* thunks) and
 * reinterpret it as a callable of type Fn. Every dplay_ and ddraw_surface_
 * helper below is this same operation with a different Fn/index — kept as
 * separate named wrappers (not collapsed into one template call site) so
 * each vtable slot number/name stays independently documented and grep-able. */
template <typename Fn>
static inline Fn vtable_slot(void* iface, std::size_t index) {
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(iface))[index]);
}

/* IDirectDrawSurface7 vtable helpers (surface stored in _g_primary_surface) */
static inline int32_t ddraw_surface_GetDC(void* surface, int32_t* hdc) {
    /* vtable[17] = GetDC */
    return vtable_slot<int32_t (*)(void*, int32_t*)>(surface, 17)(surface, hdc);
}
static inline void ddraw_surface_ReleaseDC(void* surface, void* hdc) {
    /* vtable[26] = ReleaseDC */
    vtable_slot<void (*)(void*, void*)>(surface, 26)(surface, hdc);
}

/* IDirectPlay4 vtable helpers (dplay_interface at session+0x1588) */
static inline int32_t dplay_Release(void* iface) {
    /* vtable[2] = Release */
    return vtable_slot<int32_t (*)(void*)>(iface, 2)(iface);
}
static inline int32_t dplay_Close(void* iface) {
    /* vtable[4] = Close */
    return vtable_slot<int32_t (*)(void*)>(iface, 0x10 / 4)(iface);
}
static inline int32_t dplay_CreatePlayer(void* iface, void* pid, void* pidSize,
                                          void* friendlyName, void* formalName,
                                          void* data, void* dataSize) {
    /* vtable[6] = CreatePlayer */
    return vtable_slot<int32_t (*)(void*, void*, void*, void*, void*, void*, void*)>
               (iface, 0x18 / 4)
               (iface, pid, pidSize, friendlyName, formalName, data, dataSize);
}
static inline int32_t dplay_Open(void* iface, void* desc, uint32_t flags,
                                  void* guid, uint32_t hWnd, uint32_t unk) {
    /* vtable[13] = Open */
    return vtable_slot<int32_t (*)(void*, void*, uint32_t, void*, uint32_t, uint32_t)>
               (iface, 0x34 / 4)
               (iface, desc, flags, guid, hWnd, unk);
}
static inline int32_t dplay_GetCaps(void* iface, void* caps, void* capsSize) {
    /* vtable[15] = GetCaps */
    return vtable_slot<int32_t (*)(void*, void*, void*)>(iface, 0x3C / 4)
               (iface, caps, capsSize);
}
static inline int32_t dplay_EnumSessions(void* iface, int32_t timeout,
                                           void* guid, int32_t* descSize,
                                           uint32_t flags) {
    /* vtable[17] = EnumSessions */
    return vtable_slot<int32_t (*)(void*, int32_t, void*, int32_t*, uint32_t)>
               (iface, 0x44 / 4)
               (iface, timeout, guid, descSize, flags);
}
static inline int32_t dplay_GetSessionDesc(void* iface, int32_t timeout,
                                             void* desc, int32_t* descSize) {
    /* vtable[18] = GetSessionDesc */
    return vtable_slot<int32_t (*)(void*, int32_t, void*, int32_t*)>
               (iface, 0x48 / 4)
               (iface, timeout, desc, descSize);
}
static inline int32_t dplay_EnumPlayers(void* iface, void* desc, uint32_t flags) {
    /* vtable[24] = EnumPlayers */
    return vtable_slot<int32_t (*)(void*, void*, uint32_t)>(iface, 0x60 / 4)
               (iface, desc, flags);
}
static inline void dplay_ClosePlayer(void* iface, uint32_t pid, uint32_t flags) {
    /* vtable[51] = Close (player handle) — actually this is IDirectPlay4::Close at slot 4,
       but the 0xCC call might be a different version. Verify during VALIDATED pass. */
    vtable_slot<void (*)(void*, uint32_t, uint32_t)>(iface, 0xCC / 4)
        (iface, pid, flags);
}

/* Generic COM QueryInterface helper */
static inline int32_t com_QueryInterface(void* iface, void* iid, void** ppv) {
    /* vtable[0] = QueryInterface */
    return vtable_slot<int32_t (*)(void*, void*, void**)>(iface, 0)(iface, iid, ppv);
}

/* IDirectPlayAddress2 vtable helpers */
static inline int32_t dpaddr2_Release(void* iface) {
    return vtable_slot<int32_t (*)(void*)>(iface, 2)(iface);
}
static inline int32_t dpaddr2_EnumPlayers(void* iface, void* callback,
                                            void* desc, int32_t descSize,
                                            uint32_t flags) {
    /* vtable[5] = Enum (player enumeration via address) */
    return vtable_slot<int32_t (*)(void*, void*, void*, int32_t, uint32_t)>
               (iface, 0x14 / 4)
               (iface, callback, desc, descSize, flags);
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

    /* Step 2: Get primary surface DC, fill with white, release DC */
    int32_t surface_hdc = 0;
    ddraw_surface_GetDC(_g_primary_surface, &surface_hdc);

    void* hBrush = GetStockObject(0); /* WHITE_BRUSH (stock object index 0) */
    // NOTE: binary passes &g_client_width as RECT* — these globals happen
    // to be contiguous (g_client_width, g_client_height, g_window_right,
    // g_window_bottom). We construct a local RECT for correctness.
    RECT fillRect;
    fillRect.left   = 0;
    fillRect.top    = 0;
    fillRect.right  = g_client_width;
    fillRect.bottom = g_client_height;
    FillRect(reinterpret_cast<void*>(static_cast<intptr_t>(surface_hdc)), &fillRect, hBrush);
    ddraw_surface_ReleaseDC(_g_primary_surface,
                             reinterpret_cast<void*>(static_cast<intptr_t>(surface_hdc)));

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

    /* Create DirectPlayAddress */
    uint32_t* address_ptr = reinterpret_cast<uint32_t*>(s + 0x15DC);
    *address_ptr = 0;
    *reinterpret_cast<uint32_t*>(s + 0x15E0) = 0;      /* dplay_address2 */

    int32_t hr = Ordinal_4(nullptr, reinterpret_cast<void**>(s + 0x15DC), nullptr, nullptr, nullptr);
    *reinterpret_cast<uint32_t*>(s + 0xD48) = hr;

    if (hr == 0 && *reinterpret_cast<void**>(s + 0x15DC) != nullptr) {
        /* Query for IDirectPlayAddress2 — IID @ 0x479048 */
        hr = com_QueryInterface(*reinterpret_cast<void**>(s + 0x15DC),
                                 reinterpret_cast<void*>(0x479048),
                                 reinterpret_cast<void**>(s + 0x15E0));
        *reinterpret_cast<uint32_t*>(s + 0xD48) = hr;
    }

    /* Release the initial DirectPlayAddress object */
    if (*reinterpret_cast<void**>(s + 0x15DC) != nullptr) {
        dpaddr2_Release(*reinterpret_cast<void**>(s + 0x15DC));
    }
    *reinterpret_cast<uint32_t*>(s + 0x15DC) = 0;
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

    /* Release dplay_address2 */
    if (*reinterpret_cast<void**>(s + 0x15E0) != nullptr) {
        dpaddr2_Release(*reinterpret_cast<void**>(s + 0x15E0));
        *reinterpret_cast<uint32_t*>(s + 0x15E0) = 0;
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
            int32_t hr2 = dplay_CreatePlayer(*reinterpret_cast<void**>(s + 0x1588),
                                              s + 0x924, &desc_size,
                                              reinterpret_cast<void*>(
                                                  reinterpret_cast<uintptr_t>(&g_empty_string)),
                                              name_dst, nullptr, nullptr);
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

        uint32_t enum_result = DirectPlay_EnumPlayers(self);
        if (static_cast<uint8_t>(enum_result) != 0) {
            /* Enumerate succeeded */
            if (*reinterpret_cast<void**>(s + 0x1588) != nullptr) {
                int32_t desc_size = 0x10;
                int32_t hr2 = dplay_CreatePlayer(*reinterpret_cast<void**>(s + 0x1588),
                                                  s + 0x924, &desc_size,
                                                  reinterpret_cast<void*>(
                                                      reinterpret_cast<uintptr_t>(&g_empty_string)),
                                                  name_dst, nullptr, nullptr);
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

    /* On success: initialize address struct (0x28 bytes) */
    if (result != 0 && *reinterpret_cast<void**>(s + 0x1588) != nullptr) {
        uint32_t* addr_buf = reinterpret_cast<uint32_t*>(s + 0x15E4);
        for (int i = 0; i < 10; i++) {
            addr_buf[i] = 0;
        }
        addr_buf[0] = 0x28;  /* dwSize */
        dplay_GetCaps(*reinterpret_cast<void**>(s + 0x1588), addr_buf, nullptr);
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

    void** dplay_ptr = reinterpret_cast<void**>(s + 0x1588);
    if (*dplay_ptr != nullptr) {
        return 0;  /* already connected */
    }

    /* Get session desc if possible */
    bool haveSession = false;
    if (DirectPlay_GetSessionDesc(session)) {
        /* Store connection result */
        uint32_t* entry = static_cast<uint32_t*>(operator_new(8));
        entry[0] = *reinterpret_cast<uint32_t*>(s + 0xD6C);
        entry[1] = 1;  /* type = TCP/IP? */
        *reinterpret_cast<uint32_t**>(s + 0xD6C) = entry;
        haveSession = true;
    }

    /* Try provider 1 (TCP/IP typically) via Ordinal_1 */
    void* dplay_dll[2];
    int32_t hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    if (hr == 0) {
        /* Query for IDirectPlay4 interface via CLSID_DirectPlay */
        hr = com_QueryInterface(dplay_dll[0],
                                 reinterpret_cast<void*>(
                                     reinterpret_cast<uintptr_t>(&CLSID_DirectPlay)),
                                 dplay_ptr);
        *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

        if (hr == 0) {
            /* Success — release DLL handle, store connection */
            dplay_Release(dplay_dll[0]);
            dplay_dll[0] = nullptr;

            /* Release interface */
            dplay_Release(*dplay_ptr);
            *dplay_ptr = nullptr;

            uint32_t* entry = static_cast<uint32_t*>(operator_new(8));
            entry[0] = *reinterpret_cast<uint32_t*>(s + 0xD6C);
            entry[1] = 4;  /* type */
            *reinterpret_cast<uint32_t**>(s + 0xD6C) = entry;
            goto check_provider_2;
        }
        /* Release DLL handle */
        dplay_Release(dplay_dll[0]);
        dplay_dll[0] = nullptr;
    }

check_provider_2:
    /* Try provider 2 (IPX) */
    hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    if (hr == 0) {
        hr = com_QueryInterface(dplay_dll[0],
                                 reinterpret_cast<void*>(
                                     reinterpret_cast<uintptr_t>(&CLSID_DirectPlay)),
                                 dplay_ptr);
        *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

        if (hr == 0) {
            dplay_Release(dplay_dll[0]);
            dplay_dll[0] = nullptr;
            dplay_Release(*dplay_ptr);
            *dplay_ptr = nullptr;

            uint32_t* entry = static_cast<uint32_t*>(operator_new(8));
            entry[0] = *reinterpret_cast<uint32_t*>(s + 0xD6C);
            entry[1] = 2;
            *reinterpret_cast<uint32_t**>(s + 0xD6C) = entry;
        } else {
            dplay_Release(dplay_dll[0]);
            dplay_dll[0] = nullptr;
        }
    }

    /* Check device presence via CreateFile (for Modem/Serial devices) */

    /* Try provider 3 */
    hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    if (hr == 0) {
        hr = com_QueryInterface(dplay_dll[0],
                                 reinterpret_cast<void*>(
                                     reinterpret_cast<uintptr_t>(&CLSID_DirectPlay)),
                                 dplay_ptr);
        *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

        if (hr != 0) {
            dplay_Release(dplay_dll[0]);
            dplay_dll[0] = nullptr;
            return *reinterpret_cast<int32_t*>(s + 0xD6C);
        }

        dplay_Release(dplay_dll[0]);
        dplay_dll[0] = nullptr;
        dplay_Release(*dplay_ptr);
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

    int32_t hFile = CreateFileA(reinterpret_cast<const char*>(local_path), 0xC0000000, 0,
                                 nullptr, 3, 0, nullptr);
    if (hFile == -1) {
        return 0xFFFFFF00;
    }
    CloseHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(hFile)));
    return 0x0100FF00;  /* success with type byte */
}

/* ================================================================== */
/* DirectPlay_GetSessionDesc — Query session description               */
/* Address: 0x45EEC0                                                   */
/*                                                                     */
/* Creates DirectPlay, queries session desc, creates player.           */
/* ================================================================== */
bool DirectPlay_GetSessionDesc(int32_t session)
{
    uint8_t* s = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(session));

    if (*reinterpret_cast<void**>(s + 0x1588) != nullptr) {
        return false;  /* already connected */
    }

    /* Create DirectPlay via Ordinal_1 */
    void** dplay_dll = reinterpret_cast<void**>(s + 0x1584);
    int32_t hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;
    if (hr != 0) return false;

    /* Query IDirectPlay4 interface via CLSID_DirectPlay */
    hr = com_QueryInterface(dplay_dll[0],
                             reinterpret_cast<void*>(
                                 reinterpret_cast<uintptr_t>(&CLSID_DirectPlay)),
                             reinterpret_cast<void**>(s + 0x1588));
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;
    if (hr != 0) {
        dplay_Release(dplay_dll[0]);
        dplay_dll[0] = nullptr;
        return false;
    }

    /* Release DLL handle */
    dplay_Release(dplay_dll[0]);
    dplay_dll[0] = nullptr;

    /* Query session desc size */
    void* dplay = *reinterpret_cast<void**>(s + 0x1588);
    int32_t desc_size = 0;
    dplay_GetSessionDesc(dplay, 0, nullptr, &desc_size);

    /* Allocate + lock global memory for session desc */
    void* hMem = GlobalAlloc(0x42, static_cast<uint32_t>(desc_size));
    void* pMem = GlobalLock(hMem);

    hr = dplay_GetSessionDesc(dplay, 0, pMem, &desc_size);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    if (hr != 0) {
        if (pMem != nullptr) {
            void* hglb = GlobalHandle(pMem);
            GlobalUnlock(hglb);
            hglb = GlobalHandle(pMem);
            GlobalFree(hglb);
        }
        dplay_Release(dplay);
        *reinterpret_cast<void**>(s + 0x1588) = nullptr;
        return false;
    }

    /* Create player enumeration */
    s[0xD70] = 0;  /* Clear player name buffer */
    hr = dpaddr2_EnumPlayers(*reinterpret_cast<void**>(s + 0x15E0),
                              reinterpret_cast<void*>(&DirectPlay_CreatePlayer),
                              pMem, desc_size, 0);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    /* Cleanup */
    if (pMem != nullptr) {
        void* hglb = GlobalHandle(pMem);
        GlobalUnlock(hglb);
        hglb = GlobalHandle(pMem);
        GlobalFree(hglb);
    }
    dplay_Release(dplay);
    *reinterpret_cast<void**>(s + 0x1588) = nullptr;

    return s[0xD70] != 0;  /* true if a player was found */
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

    /* Create session desc packet at +0x158C (0x50 bytes) */
    uint32_t* desc = reinterpret_cast<uint32_t*>(s + 0x158C);
    for (int i = 0; i < 0x14; i++) {
        desc[i] = 0;
    }
    desc[0] = 0x50;                         /* dwSize */
    /* NOTE (2026-08-08, STRICT=2 cast pass): these four reads dereference
     * fixed addresses (0x479158.. — the original binary's .rdata GUID
     * bytes) directly, unguarded by _WIN32. On host this is a genuine
     * absolute-address read of unmapped memory if ever reached — same
     * shape as the DirectPlay_* pointer-truncation landmine but a distinct
     * defect (fixed-address read, not int32_t/pointer width). Preserved
     * as-is: DirectPlay_SetSessionDesc is call-0/unreachable on host today
     * (see docs/landmine-sweep-worklist.md's DirectPlay_* cluster entry);
     * fixing it is out of scope for this cast-respelling pass. */
    desc[6] = *reinterpret_cast<uint32_t*>(0x479158);         /* dwSession? */
    desc[7] = *reinterpret_cast<uint32_t*>(0x47915C);
    desc[8] = *reinterpret_cast<uint32_t*>(0x479160);
    desc[9] = *reinterpret_cast<uint32_t*>(0x479164);

    if (*reinterpret_cast<char*>(s + 0x498) != 0) {
        *reinterpret_cast<uint32_t*>(s + 0x15C0) =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s + 0x498));  /* lpszPassword */
    }

    /* Create session via DirectPlay */
    void* dplay = *reinterpret_cast<void**>(s + 0x1588);
    int32_t hr2 = dplay_Open(dplay, desc, 0,
                              reinterpret_cast<void*>(
                                  reinterpret_cast<uintptr_t>(&GUID_SessionDesc)),
                              *reinterpret_cast<uint32_t*>(s + 0x938), 0x81);
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

        hr2 = dplay_Open(dplay, desc, 0,
                          reinterpret_cast<void*>(
                              reinterpret_cast<uintptr_t>(&GUID_SessionDesc)),
                          *reinterpret_cast<uint32_t*>(s + 0x938), 0x81);
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
/* DirectPlay_CreatePlayer — DirectPlay enum callback                 */
/* Address: 0x45FBD0                                                   */
/*                                                                     */
/* Called by DirectPlay when enumerating session players.             */
/* ================================================================== */
uint32_t DirectPlay_CreatePlayer(const char* playerName, uint32_t flags,
                                          const char* displayName)
{
    /* Compare playerName with empty string */
    const char* empty = "";
    int32_t i = 0;
    while (i < 0x10) {
        if (playerName[i] != empty[i]) break;
        if (playerName[i] == 0) {
            /* playerName is empty string */
            int32_t nameLen = lstrlenA(displayName);
            if (nameLen != 0) {
                /* Copy displayName into g_dplay_peer + 0xD70 */
                char* dst = reinterpret_cast<char*>(
                    static_cast<uint8_t*>(g_dplay_peer) + 0xD70);
                int32_t j = 0;
                while (displayName[j] != 0) {
                    dst[j] = displayName[j];
                    j++;
                }
                dst[j] = 0;
                return 0;  /* continue enumeration */
            }
            break;
        }
        i++;
    }
    return 1;  /* stop enumeration */
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
        void* dplay = *reinterpret_cast<void**>(s + 0x1588);

        /* Close player connection */
        dplay_ClosePlayer(dplay, 0, 0);

        /* Close session */
        dplay_Close(dplay);

        /* Release interface */
        dplay_Release(dplay);

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
/* DirectPlay_EnumPlayers — Enumerate session players                  */
/* Address: 0x45FD80                                                   */
/*                                                                     */
/* Builds a session desc, calls IDirectPlay4::EnumPlayers, retries    */
/* on pending. Returns 1 on success.                                  */
/* ================================================================== */
uint32_t DirectPlay_EnumPlayers(void* self)
{
    uint8_t* s = static_cast<uint8_t*>(self);

    if (*reinterpret_cast<void**>(s + 0x1588) == nullptr) {
        return 0;
    }

    /* Build session desc structure at +0x158C */
    uint32_t* desc = reinterpret_cast<uint32_t*>(s + 0x158C);
    for (int i = 0; i < 0x14; i++) {
        desc[i] = 0;
    }
    desc[0] = 0x50;

    uint8_t is_host = s[2];
    desc[1] = (is_host ? 0 : 4) + 0xA040;  /* dwFlags */

    /* See the identical fixed-address-read note in DirectPlay_SetSessionDesc
     * above — preserved as-is, out of scope for this cast-respelling pass. */
    desc[6] = *reinterpret_cast<uint32_t*>(0x479158);   /* guid fields */
    desc[7] = *reinterpret_cast<uint32_t*>(0x47915C);
    desc[8] = *reinterpret_cast<uint32_t*>(0x479160);
    desc[9] = *reinterpret_cast<uint32_t*>(0x479164);
    desc[10] = *reinterpret_cast<uint32_t*>(s + 0x920);  /* dwMaxPlayers */

    desc[12] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s + 0x18));    /* lpszSessionName */
    if (*reinterpret_cast<char*>(s + 0x498) != '\0') {
        desc[13] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s + 0x498));  /* lpszPassword */
    }

    /* Call EnumPlayers */
    void* dplay = *reinterpret_cast<void**>(s + 0x1588);
    int32_t hr = dplay_EnumPlayers(dplay, desc, 0x82);
    *reinterpret_cast<int32_t*>(s + 0xD48) = hr;

    /* Retry on pending */
    while (hr == -0x7788fea2 && s[1] == 0) {
        Sleep(1);
        hr = dplay_EnumPlayers(dplay, desc, 0x82);
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
