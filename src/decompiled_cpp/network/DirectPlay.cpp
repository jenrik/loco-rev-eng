/**
 * DirectPlay.cpp — DirectPlay network session wrapper implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
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
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */

/* DirectPlay COM interface GUIDs (from .rdata section of loco.exe) */
/* These are the binary GUID values used for IID_IDirectPlay4A and session descriptor */
static const uint32_t PTR_LAB_00478f88[4] = {0,0,0,0};  /* placeholder for IID_IDirectPlay4A */
static const uint32_t PTR_LAB_0045f2b0[4] = {0,0,0,0};  /* placeholder for DPSESSIONDESC2 GUID */

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
void* __thiscall GameObject_BaseCtor(void* mem, int32_t a, int32_t b, int32_t c, int32_t d);
void  __cdecl    DDRAW_PresentRect(void* rect, void* hWnd, int32_t* param, uint8_t flag);

/* DirectPlay game functions (C++ linkage) */
uint32_t __cdecl WIN32_RecvNetworkData(void* session, uint32_t resId, const char* msg);
void     __thiscall WIN32_SendNetworkData(void* session, uint32_t dpId,
                                           int32_t data, uint32_t size, uint32_t flags);
uint32_t __thiscall WIN32_GetSystemMetrics(void* session);

/* ================================================================== */
/* DirectPlay Ordinal exports (loaded via GetProcAddress from dplay.dll/dpmodem.dll) */
/* ================================================================== */

extern int32_t __stdcall Ordinal_1(void* ptr1, void* ptr2, void* ptr3, void* ptr4);
extern int32_t __stdcall Ordinal_4(void* ptr1, void** ptr2, void* ptr3, void* ptr4, void* ptr5);

/* ================================================================== */
/* Global state                                                       */
/* ================================================================== */

extern void* g_dplay_peer;          /* DirectPlay peer struct pointer */
extern int32_t* _g_primary_surface;  /* 0x4FD3C4 — primary DDraw surface */
extern int32_t* _g_dsound_object;    /* 0x4FD398 — shadow GameObject (treated as byte-accessible via cast) */
extern void* g_main_window;          /* 0x4AA4A0 */
extern int32_t g_client_width;      /* primary surface width */
extern char g_empty_string;         /* 0x4851D0 */
extern char g_install_path[];       /* 0x4A99C8 */

/* String constants */
extern const char DAT_00481218[4];  /* Device path string */

/* ================================================================== */
/* DirectPlay_SessionMgr — Walk tree storing IDs and directions       */
/* Address: 0x45DA70                                                   */
/*                                                                     */
/* Delegate to AssetMgr's implementation in AssetMgr.cpp              */
/* ================================================================== */
/* (Implementation defined in AssetMgr.cpp) */

/* ================================================================== */
/* DirectPlay_Init — Initialize DirectPlay/compositing                */
/* Address: 0x45E090                                                   */
/*                                                                     */
/* Called once from CGWND_InitMode1 to set up the initial screen      */
/* compositing: plays a startup sound, fills the primary surface,     */
/* creates a shadow GameObject, positions it center-screen, and       */
/* presents the first frame.                                           */
/* ================================================================== */
void __cdecl DirectPlay_Init(void)
{
    /* Step 1: Play startup sound (null = just stop/init) */
    PlaySoundA(NULL, NULL, 0);

    /* Step 2: Fill primary surface with black background */
    int32_t client_rect[4];  /* receives client width/height */
    ((int32_t (__thiscall*)(void*, int32_t*))(*(void***)_g_primary_surface)[0x44 / 4])
        (_g_primary_surface, client_rect);

    void* hBrush = GetStockObject(0); /* NULL_BRUSH or WHITE_BRUSH? */
    FillRect((void*)-1, (void*)&g_client_width, hBrush); /* full surface */
    ((int32_t (__thiscall*)(void*, void*))(*(void***)_g_primary_surface)[0x68 / 4])
        (_g_primary_surface, (void*)-1); /* ReleaseDC */

    /* Step 3: Create shadow GameObject at 0x4FD398 */
    void* shadow_mem = operator_new(0x88);
    if (shadow_mem == NULL) {
        _g_dsound_object = NULL;
    } else {
        _g_dsound_object = GameObject_BaseCtor(shadow_mem, 0x402, -1, 0, 0);
    }

    /* Step 4: Position shadow at screen center */
    {
        Entity* shadow = (Entity*)_g_dsound_object;
        int32_t screen_h = GetSystemMetrics(1);   /* SM_CYSCREEN */
        int32_t y_center = screen_h / 2;
        /* Subtract half of frame height (frame data at g_dsound_object[0x10] + 0x16) */
        y_center -= (uint16_t)(*(uint16_t*)(*(int32_t*)((uint8_t*)_g_dsound_object + 0x40) + 0x16) >> 1);
        int32_t screen_w = GetSystemMetrics(0);   /* SM_CXSCREEN */
        int32_t x_center = (screen_w + (screen_w >> 0x1F & 3)) >> 2; /* quarter screen */

        shadow->MoveTo(x_center, y_center);
    }

    Entity* shadow = (Entity*)_g_dsound_object;
    shadow->StopSound(0);
    shadow->Draw(shadow->screen_rect, 0, 0);

    /* Step 7: Present to screen */
    DDRAW_PresentRect((void*)&g_client_width,
                       *(void**)((uint8_t*)g_main_window + 8),
                       NULL, 0);
}

/* ================================================================== */
/* DirectPlay_CreatePeer — Create DirectPlay peer                     */
/* Address: 0x45E490                                                   */
/*                                                                     */
/* Wrapper: calls CreateAddress and returns this.                     */
/* ================================================================== */
void* __thiscall DirectPlay_CreatePeer(void* self, uint32_t param_1, uint32_t param_2)
{
    DirectPlay_CreateAddress(self, param_1, param_2);
    return self;
}

/* ================================================================== */
/* DirectPlay_CreateAddress — Initialize DirectPlay address           */
/* Address: 0x45E4B0                                                   */
/*                                                                     */
/* Sets session fields, creates DirectPlayAddress COM object,         */
/* queries IDirectPlayAddress interface.                               */
/* ================================================================== */
void __thiscall DirectPlay_CreateAddress(void* self, uint32_t param_1, uint32_t param_2)
{
    uint8_t* s = (uint8_t*)self;

    /* Initialize fields */
    s[0x944] = 1;        /* show_dialogs = true */
    s[2] = 1;            /* flag_byte_2 = 1 (startup) */
    *(uint32_t*)(s + 0xD4C) = 0;      /* idle_callback */
    *(uint32_t*)(s + 0x940) = 0;       /* error_callback */
    s[0x945] = 0;         /* error_msg_buf[0] = 0 */
    *(uint32_t*)(s + 0xD48) = 0;       /* last_hresult */
    s[0xD50] = 0;         /* session_ready = 0 */
    s[0] = 0;             /* session_state = 0 (inactive) */
    s[1] = 0;             /* flag_byte_1 = 0 */
    *(uint32_t*)(s + 0x920) = 0;       /* session_flags */
    s[3] = 0;             /* flag_byte_3 */
    s[4] = 0;             /* flag_byte_4 */
    *(uint32_t*)(s + 0x1588) = 0;      /* dplay_interface */
    s[0x18] = 0;          /* session_name[0] = 0 */
    *(uint32_t*)(s + 0x92C) = 0;       /* session_desc_ptr */
    s[0x930] = 0;         /* session_desc_valid */
    s[0x418] = 0;         /* player_name[0] = 0 */
    *(uint32_t*)(s + 0x924) = 0;       /* player_dpid */
    *(uint32_t*)(s + 0x518) = 0;       /* connection_type */
    *(uint32_t*)(s + 0x93C) = param_1; /* hinstance */
    *(uint32_t*)(s + 0x938) = param_2; /* hwnd */
    *(uint32_t*)(s + 0xD54) = 0;       /* session_data_size */
    *(uint32_t*)(s + 0xD58) = 0;       /* session_data_ptr */
    *(uint32_t*)(s + 0xD60) = 0;       /* session_list */
    *(uint32_t*)(s + 0xD64) = 0;       /* player_list */
    *(uint32_t*)(s + 0xD68) = 0;       /* group_list */
    *(uint32_t*)(s + 0xD6C) = 0;       /* connection_list */
    *(uint32_t*)(s + 0xD5C) = 10;      /* max_players = 10 */

    /* Create DirectPlayAddress */
    uint32_t* address_ptr = (uint32_t*)(s + 0x15DC);
    *address_ptr = 0;
    *(uint32_t*)(s + 0x15E0) = 0;      /* dplay_address2 */

    int32_t hr = Ordinal_4(0, (void**)(s + 0x15DC), NULL, NULL, NULL);
    *(uint32_t*)(s + 0xD48) = hr;

    if (hr == 0 && *(void**)(s + 0x15DC) != NULL) {
        /* Query for IDirectPlayAddress2 */
        hr = ((int32_t (__thiscall*)(void*, void*, void**))
            **(void***)(*(uint32_t*)(s + 0x15DC)))
            (*(void**)(s + 0x15DC),
             (void*)0x479048,        /* IID_IDirectPlayAddress2 */
             (void**)(s + 0x15E0));
        *(uint32_t*)(s + 0xD48) = hr;
    }

    /* Release the initial DirectPlayAddress object */
    if (*(void**)(s + 0x15DC) != NULL) {
        ((void (__thiscall*)(void*))(*(void***)(*(uint32_t*)(s + 0x15DC)))[2])
            (*(void**)(s + 0x15DC));
    }
    *(uint32_t*)(s + 0x15DC) = 0;
}

/* ================================================================== */
/* DirectPlay_DestroyPeer — Full DirectPlay peer teardown             */
/* Address: 0x45E5A0                                                   */
/*                                                                     */
/* Closes session, releases peer, and frees all linked lists.         */
/* ================================================================== */
void __fastcall DirectPlay_DestroyPeer(int32_t session)
{
    uint8_t* s = (uint8_t*)session;

    /* Close session */
    DirectPlay_Close(session);

    /* Release dplay_address2 */
    if (*(void**)(s + 0x15E0) != NULL) {
        ((void (__thiscall*)(void*))(*(void***)(*(uint32_t*)(s + 0x15E0)))[2])
            (*(void**)(s + 0x15E0));
        *(uint32_t*)(s + 0x15E0) = 0;
    }

    /* Free session data */
    if (*(void**)(s + 0xD58) != NULL) {
        GLOBAL_free(*(void**)(s + 0xD58));
        *(uint32_t*)(s + 0xD58) = 0;
        *(uint32_t*)(s + 0xD54) = 0;
    }

    /* Free session list (2 linked fields per entry: next_ptr + data_ptr) */
    int32_t* list_item = *(int32_t**)(s + 0xD60);
    while (list_item != NULL) {
        int32_t* next = (int32_t*)*list_item;
        if ((void*)list_item[2] != NULL) {
            GLOBAL_free((void*)list_item[2]);
        }
        GLOBAL_free(list_item);
        *(int32_t**)(s + 0xD60) = next;
        list_item = next;
    }

    /* Free player list (linked list with next + data + hGlobal mem) */
    list_item = *(int32_t**)(s + 0xD64);
    while (list_item != NULL) {
        int32_t* next = (int32_t*)*list_item;
        if ((void*)list_item[2] != NULL) {
            GLOBAL_free((void*)list_item[2]);
        }
        if ((void*)list_item[1] != NULL) {
            void* hglb = GlobalHandle((void*)list_item[1]);
            GlobalUnlock(hglb);
            hglb = GlobalHandle((void*)list_item[1]);
            GlobalFree(hglb);
            list_item[1] = 0;
        }
        GLOBAL_free(list_item);
        *(int32_t**)(s + 0xD64) = next;
        list_item = next;
    }

    /* Free group list */
    list_item = *(int32_t**)(s + 0xD68);
    while (list_item != NULL) {
        int32_t* next = (int32_t*)*list_item;
        if ((void*)list_item[2] != NULL) {
            GLOBAL_free((void*)list_item[2]);
        }
        GLOBAL_free(list_item);
        *(int32_t**)(s + 0xD68) = next;
        list_item = next;
    }

    /* Free message list */
    list_item = *(int32_t**)(s + 0xD6C);
    while (list_item != NULL) {
        int32_t* next = (int32_t*)*list_item;
        GLOBAL_free(list_item);
        *(int32_t**)(s + 0xD6C) = next;
        list_item = next;
    }
}

/* ================================================================== */
/* DirectPlay_HostSession — Store host configuration                  */
/* Address: 0x45E700                                                   */
/*                                                                     */
/* Simple setter: stores 4 configuration parameters.                  */
/* ================================================================== */
void __thiscall DirectPlay_HostSession(void* self, uint8_t param_1, uint32_t param_2,
                                        uint8_t param_3, uint8_t param_4)
{
    uint8_t* s = (uint8_t*)self;
    s[0] = param_1;             /* session_state */
    *(uint32_t*)(s + 0x920) = param_2;  /* session_flags */
    s[1] = param_3;             /* flag_byte_1 (is_host) */
    s[2] = param_4;             /* flag_byte_2 */
}

/* ================================================================== */
/* DirectPlay_ConnectToSession — Join a DirectPlay session            */
/* Address: 0x45E730                                                   */
/*                                                                     */
/* Stores player name, session name, password. Creates DirectPlay     */
/* player, sends session desc, enumerates players. Returns 1 on       */
/* success. Called from Train connection code.                        */
/* ================================================================== */
uint8_t __thiscall DirectPlay_ConnectToSession(void* self, const char* playerName,
                                                const char* sessionName,
                                                const char* password)
{
    uint8_t* s = (uint8_t*)self;

    /* Clear session name, copy if provided */
    s[0x18] = 0;
    if (sessionName != NULL) {
        int32_t i = 0;
        while (sessionName[i] != 0 && i < 0x3FF) {
            s[0x18 + i] = (uint8_t)sessionName[i];
            i++;
        }
        s[0x18 + i] = 0;
    }

    /* Copy player name (max 0x80 bytes) */
    char* name_dst = (char*)(s + 0x418);
    *name_dst = 0;
    if (playerName != NULL) {
        int32_t len = 0;
        while (playerName[len] != 0) len++;
        if (len < 0x80) {
            for (int32_t i = 0; i <= len; i++) {
                name_dst[i] = playerName[i];
            }
        } else {
            /* Truncate at 0x80 */
            char saved = playerName[0x80];
            *(char**)&playerName[0x80] = 0; /* force null terminator */
            int32_t i = 0;
            while (playerName[i] != 0 && i < 0x80) {
                name_dst[i] = playerName[i];
                i++;
            }
            name_dst[i] = 0;
            *(char**)&playerName[0x80] = (char*)saved; /* restore */
        }
    }

    /* Copy password (max 0x80 bytes, same logic) */
    char* pwd_dst = (char*)(s + 0x498);
    *pwd_dst = 0;
    if (password != NULL) {
        int32_t len = 0;
        while (password[len] != 0) len++;
        if (len < 0x80) {
            for (int32_t i = 0; i <= len; i++) {
                pwd_dst[i] = password[i];
            }
        } else {
            char saved = password[0x80];
            *(char**)&password[0x80] = 0;
            int32_t i = 0;
            while (password[i] != 0 && i < 0x80) {
                pwd_dst[i] = password[i];
                i++;
            }
            pwd_dst[i] = 0;
            *(char**)&password[0x80] = (char*)saved;
        }
    }

    s[0x498] = 0;  /* DEBUG: redundant zero */

    /* --- Connection logic --- */
    uint8_t result = 0;

    if (s[0] == 0) {
        /* Not hosting: try to join */
        /* Handle messages to create DirectPlay instance */
        uint32_t hr = DirectPlay_HandleMessages();
        if ((uint8_t)hr == 0) {
            DirectPlay_Close((int32_t)self);
            return 0;
        }

        /* Get system metrics / open session */
        uint32_t enum_result = WIN32_GetSystemMetrics(self);
        if ((uint8_t)enum_result == 0) {
            DirectPlay_Close((int32_t)self);
            return 0;
        }

        /* Join session via DirectPlay CreatePlayer */
        if (*(void**)(s + 0x1588) != NULL) {
            int32_t desc_size = 0x10;
            void* player_dpid_ptr = (void*)(s + 0x924);
            int32_t hr2 = ((int32_t (__thiscall*)(void*, void*, void*, void*, void*, void*, void*))
                (*(void***)(*(uint32_t*)(s + 0x1588)))[0x18 / 4])
                (*(void**)(s + 0x1588), s + 0x924, &desc_size,
                 (void*)(uintptr_t)&g_empty_string, name_dst, NULL, NULL);
            *(int32_t*)(s + 0xD48) = hr2;

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
            DirectPlay_Close((int32_t)self);
            return 0;
        }
    } else {
        /* Hosting: handle messages then enumerate players */
        uint32_t hr = DirectPlay_HandleMessages();
        if ((uint8_t)hr == 0) {
            DirectPlay_Close((int32_t)self);
            return 0;
        }

        uint32_t enum_result = DirectPlay_EnumPlayers(self);
        if ((uint8_t)enum_result != 0) {
            /* Enumerate succeeded */
            if (*(void**)(s + 0x1588) != NULL) {
                int32_t desc_size = 0x10;
                int32_t hr2 = ((int32_t (__thiscall*)(void*, void*, void*, void*, void*, void*, void*))
                    (*(void***)(*(uint32_t*)(s + 0x1588)))[0x18 / 4])
                    (*(void**)(s + 0x1588), s + 0x924, &desc_size,
                     (void*)(uintptr_t)&g_empty_string, name_dst, NULL, NULL);
                *(int32_t*)(s + 0xD48) = hr2;

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
                DirectPlay_Close((int32_t)self);
                return 0;
            }
        } else {
            /* Enumeration failed — check if error is DPERR_USERCANCEL */
            if (s[1] != 0 && *(int32_t*)(s + 0xD48) == -0x7788fea2) {
                /* DPERR_PENDING — user cancelled */
                return 0;
            }
            DirectPlay_Close((int32_t)self);
            return 0;
        }
    }

    /* On success: initialize address struct (0x28 bytes) */
    if (result != 0 && *(void**)(s + 0x1588) != NULL) {
        uint32_t* addr_buf = (uint32_t*)(s + 0x15E4);
        for (int i = 0; i < 10; i++) {
            addr_buf[i] = 0;
        }
        addr_buf[0] = 0x28;  /* dwSize */
        ((void (__thiscall*)(void*, void*, int32_t))
            (*(void***)(*(uint32_t*)(s + 0x1588)))[0x38 / 4])
            (*(void**)(s + 0x1588), addr_buf, 0);
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
int32_t __fastcall DirectPlay_EnumConnections(int32_t session)
{
    uint8_t* s = (uint8_t*)session;

    /* Return existing list if already enumerated */
    if (*(int32_t*)(s + 0xD6C) != 0) {
        return *(int32_t*)(s + 0xD6C);
    }

    void** dplay_ptr = (void**)(s + 0x1588);
    if (*dplay_ptr != NULL) {
        return 0;  /* already connected */
    }

    /* Get session desc if possible */
    bool haveSession = false;
    if (DirectPlay_GetSessionDesc(session)) {
        /* Store connection result */
        uint32_t* entry = (uint32_t*)operator_new(8);
        entry[0] = *(uint32_t*)(s + 0xD6C);
        entry[1] = 1;  /* type = TCP/IP? */
        *(uint32_t**)(s + 0xD6C) = entry;
        haveSession = true;
    }

    /* Try provider 1 (TCP/IP typically) via Ordinal_1 */
    void* dplay_dll[2];
    int32_t hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *(int32_t*)(s + 0xD48) = hr;

    if (hr == 0) {
        /* Query for IDirectPlay4 interface */
        hr = ((int32_t (__thiscall*)(void*, void*, void**))
            **(void***)dplay_dll[0])
            ((void*)dplay_dll[0], (void*)(uintptr_t)&PTR_LAB_00478f88, dplay_ptr);
        *(int32_t*)(s + 0xD48) = hr;

        if (hr == 0) {
            /* Success — release DLL handle, store connection */
            ((void (__thiscall*)(void*))(*(void***)dplay_dll[0])[2])((void*)dplay_dll[0]);
            dplay_dll[0] = 0;

            /* Release interface */
            ((void (__thiscall*)(void*))(*(void***)(*dplay_ptr))[2])(*dplay_ptr);
            *dplay_ptr = 0;

            uint32_t* entry = (uint32_t*)operator_new(8);
            entry[0] = *(uint32_t*)(s + 0xD6C);
            entry[1] = 4;  /* type */
            *(uint32_t**)(s + 0xD6C) = entry;
            goto check_provider_2;
        }
        /* Release DLL handle */
        ((void (__thiscall*)(void*))(*(void***)dplay_dll[0])[2])((void*)dplay_dll[0]);
        dplay_dll[0] = 0;
    }

check_provider_2:
    /* Try provider 2 (IPX) */
    hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *(int32_t*)(s + 0xD48) = hr;

    if (hr == 0) {
        hr = ((int32_t (__thiscall*)(void*, void*, void**))
            **(void***)dplay_dll[0])
            ((void*)dplay_dll[0], (void*)(uintptr_t)&PTR_LAB_00478f88, dplay_ptr);
        *(int32_t*)(s + 0xD48) = hr;

        if (hr == 0) {
            ((void (__thiscall*)(void*))(*(void***)dplay_dll[0])[2])((void*)dplay_dll[0]);
            dplay_dll[0] = 0;
            ((void (__thiscall*)(void*))(*(void***)(*dplay_ptr))[2])(*dplay_ptr);
            *dplay_ptr = 0;

            uint32_t* entry = (uint32_t*)operator_new(8);
            entry[0] = *(uint32_t*)(s + 0xD6C);
            entry[1] = 2;
            *(uint32_t**)(s + 0xD6C) = entry;
        } else {
            ((void (__thiscall*)(void*))(*(void***)dplay_dll[0])[2])((void*)dplay_dll[0]);
            dplay_dll[0] = 0;
        }
    }

    /* Check device presence via CreateFile (for Modem/Serial devices) */

    /* Try provider 3 */
    hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *(int32_t*)(s + 0xD48) = hr;

    if (hr == 0) {
        hr = ((int32_t (__thiscall*)(void*, void*, void**))
            **(void***)dplay_dll[0])
            ((void*)dplay_dll[0], (void*)(uintptr_t)&PTR_LAB_00478f88, dplay_ptr);
        *(int32_t*)(s + 0xD48) = hr;

        if (hr != 0) {
            ((void (__thiscall*)(void*))(*(void***)dplay_dll[0])[2])((void*)dplay_dll[0]);
            dplay_dll[0] = 0;
            return *(int32_t*)(s + 0xD6C);
        }

        ((void (__thiscall*)(void*))(*(void***)dplay_dll[0])[2])((void*)dplay_dll[0]);
        dplay_dll[0] = 0;
        ((void (__thiscall*)(void*))(*(void***)(*dplay_ptr))[2])(*dplay_ptr);
        *dplay_ptr = 0;

        uint32_t* entry = (uint32_t*)operator_new(8);
        entry[0] = *(uint32_t*)(s + 0xD6C);
        entry[1] = 3;
        *(uint32_t**)(s + 0xD6C) = entry;
    }

    return *(int32_t*)(s + 0xD6C);
}

/* ================================================================== */
/* DirectPlay_GetConnectionCaps — Check if connection device exists   */
/* Address: 0x45EE60                                                   */
/*                                                                     */
/* Attempts to open a device file. Returns success/failure.           */
/* ================================================================== */
uint32_t __cdecl DirectPlay_GetConnectionCaps(uint8_t* devicePath)
{
    /* Build device path from first byte + global string */
    uint8_t local_path[12];
    local_path[0] = devicePath[0];
    /* Copy DAT_00481214 (3 bytes) to local_path+1..3 */
    *(uint32_t*)&local_path[1] = *(uint32_t*)&DAT_00481218;

    int32_t hFile = CreateFileA((const char*)local_path, 0xC0000000, 0,
                                 NULL, 3, 0, NULL);
    if (hFile == -1) {
        return 0xFFFFFF00;
    }
    CloseHandle((void*)hFile);
    return 0x0100FF00;  /* success with type byte */
}

/* ================================================================== */
/* DirectPlay_GetSessionDesc — Query session description               */
/* Address: 0x45EEC0                                                   */
/*                                                                     */
/* Creates DirectPlay, queries session desc, creates player.           */
/* ================================================================== */
bool __fastcall DirectPlay_GetSessionDesc(int32_t session)
{
    uint8_t* s = (uint8_t*)session;

    if (*(void**)(s + 0x1588) != NULL) {
        return false;  /* already connected */
    }

    /* Create DirectPlay via Ordinal_1 */
    void** dplay_dll = (void**)(s + 0x1584);
    int32_t hr = Ordinal_1(nullptr, nullptr, nullptr, nullptr);
    *(int32_t*)(s + 0xD48) = hr;
    if (hr != 0) return false;

    /* Query IDirectPlay4 interface */
    hr = ((int32_t (__thiscall*)(void*, void*, void**))
        **(void***)dplay_dll[0])
        ((void*)dplay_dll[0], (void*)(uintptr_t)&PTR_LAB_00478f88,
         (void**)(s + 0x1588));
    *(int32_t*)(s + 0xD48) = hr;
    if (hr != 0) {
        ((void (__thiscall*)(void*))(*(void***)dplay_dll[0])[2])((void*)dplay_dll[0]);
        dplay_dll[0] = 0;
        return false;
    }

    /* Release DLL handle */
    ((void (__thiscall*)(void*))(*(void***)dplay_dll[0])[2])((void*)dplay_dll[0]);
    dplay_dll[0] = 0;

    /* Query session desc size */
    void* dplay = *(void**)(s + 0x1588);
    int32_t desc_size = 0;
    ((int32_t (__thiscall*)(void*, int32_t, void*, void*))
        (*(void***)dplay)[0x48 / 4])
        (dplay, 0, NULL, &desc_size);

    /* Allocate + lock global memory for session desc */
    void* hMem = GlobalAlloc(0x42, (uint32_t)desc_size);
    void* pMem = GlobalLock(hMem);

    hr = ((int32_t (__thiscall*)(void*, int32_t, void*, int32_t*))
        (*(void***)dplay)[0x48 / 4])
        (dplay, 0, pMem, &desc_size);
    *(int32_t*)(s + 0xD48) = hr;

    if (hr != 0) {
        if (pMem != NULL) {
            void* hglb = GlobalHandle(pMem);
            GlobalUnlock(hglb);
            hglb = GlobalHandle(pMem);
            GlobalFree(hglb);
        }
        ((void (__thiscall*)(void*))(*(void***)dplay)[2])(dplay);
        *(void**)(s + 0x1588) = NULL;
        return false;
    }

    /* Create player enumeration */
    s[0xD70] = 0;  /* Clear player name buffer */
    hr = ((int32_t (__thiscall*)(void*, void*, void*, int32_t, uint32_t))
        (*(void***)(*(uint32_t*)(s + 0x15E0)))[0x14 / 4])
        (*(void**)(s + 0x15E0), &DirectPlay_CreatePlayer, pMem, desc_size, 0);
    *(int32_t*)(s + 0xD48) = hr;

    /* Cleanup */
    if (pMem != NULL) {
        void* hglb = GlobalHandle(pMem);
        GlobalUnlock(hglb);
        hglb = GlobalHandle(pMem);
        GlobalFree(hglb);
    }
    ((void (__thiscall*)(void*))(*(void***)dplay)[2])(dplay);
    *(void**)(s + 0x1588) = NULL;

    return s[0xD70] != 0;  /* true if a player was found */
}

/* ================================================================== */
/* DirectPlay_SetSessionDesc — Set session description and host        */
/* Address: 0x45F090                                                   */
/*                                                                     */
/* Stores password, clears old player list, creates session via        */
/* DirectPlay CreateSession, retries on pending.                      */
/* ================================================================== */
uint32_t __thiscall DirectPlay_SetSessionDesc(void* self, const char* password)
{
    uint8_t* s = (uint8_t*)self;

    /* Copy password to buffer at +0x498 (max 0x80 bytes, same pattern) */
    char* pwd_dst = (char*)(s + 0x498);
    *pwd_dst = 0;
    if (password != NULL) {
        int32_t len = 0;
        while (password[len] != 0) len++;
        if (len < 0x80) {
            for (int32_t i = 0; i <= len; i++) {
                pwd_dst[i] = password[i];
            }
        } else {
            char saved = password[0x80];
            *(char**)&password[0x80] = 0;
            int32_t i = 0;
            while (password[i] != 0 && i < 0x80) {
                pwd_dst[i] = password[i];
                i++;
            }
            pwd_dst[i] = 0;
            *(char**)&password[0x80] = (char*)saved;
        }
    }

    /* Clear player list if not host */
    if (s[1] == 0) {
        int32_t* player = *(int32_t**)(s + 0xD64);
        while (player != NULL) {
            int32_t* next = (int32_t*)*player;
            if ((void*)player[2] != NULL) {
                GLOBAL_free((void*)player[2]);
            }
            if ((void*)player[1] != NULL) {
                void* hglb = GlobalHandle((void*)player[1]);
                GlobalUnlock(hglb);
                hglb = GlobalHandle((void*)player[1]);
                GlobalFree(hglb);
                player[1] = 0;
            }
            GLOBAL_free(player);
            *(int32_t**)(s + 0xD64) = next;
            player = next;
        }
    }

    /* Handle messages to create DirectPlay */
    uint32_t hr = DirectPlay_HandleMessages();
    if ((uint8_t)hr == 0) {
        return 0;
    }

    /* Create session desc packet at +0x158C (0x50 bytes) */
    uint32_t* desc = (uint32_t*)(s + 0x158C);
    for (int i = 0; i < 0x14; i++) {
        desc[i] = 0;
    }
    desc[0] = 0x50;                         /* dwSize */
    desc[6] = *(uint32_t*)0x479158;         /* dwSession? */
    desc[7] = *(uint32_t*)0x47915C;
    desc[8] = *(uint32_t*)0x479160;
    desc[9] = *(uint32_t*)0x479164;

    if (*(char*)(s + 0x498) != 0) {
        *(uint32_t*)(s + 0x15C0) = (uint32_t)(s + 0x498);  /* lpszPassword */
    }

    /* Create session via DirectPlay */
    int32_t hr2 = ((int32_t (__thiscall*)(void*, void*, uint32_t, void*, uint32_t, uint32_t))
        (*(void***)(*(uint32_t*)(s + 0x1588)))[0x34 / 4])
        (*(void**)(s + 0x1588), desc, 0, (void*)(uintptr_t)&PTR_LAB_0045f2b0,
         *(uint32_t*)(s + 0x938), 0x81);
    *(int32_t*)(s + 0xD48) = hr2;

    /* Retry on DPERR_PENDING */
    while (hr2 == -0x7788fea2) {  /* DPERR_PENDING */
        if (s[1] != 0) {          /* flag set to abort */
            return *(uint32_t*)(s + 0xD64);
        }
        if (*(void**)(s + 0xD4C) != NULL) {
            ((void (*)(void))(*(uint32_t*)(s + 0xD4C)))();  /* idle callback */
        }
        Sleep(1);

        hr2 = ((int32_t (__thiscall*)(void*, void*, uint32_t, void*, uint32_t, uint32_t))
            (*(void***)(*(uint32_t*)(s + 0x1588)))[0x34 / 4])
            (*(void**)(s + 0x1588), desc, 0, (void*)(uintptr_t)&PTR_LAB_0045f2b0,
             *(uint32_t*)(s + 0x938), 0x81);
        *(int32_t*)(s + 0xD48) = hr2;
    }

    return *(uint32_t*)(s + 0xD64);  /* return session count pointer */
}

/* ================================================================== */
/* DirectPlay_HandleMessages — Create DirectPlay instance              */
/* Address: 0x45F390                                                   */
/*                                                                     */
/* Main network setup function. Creates DirectPlay object, enumerates */
/* connections, initializes address. Large state machine (~2076 bytes).*/
/* Shows connection dialog for interactive provider selection.        */
/* ================================================================== */
uint32_t __cdecl DirectPlay_HandleMessages(void)
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

    /* Full implementation is in DPLAY_SendMessages.c and related files.
     * This stub documents the flow. */
    return 1;  /* success */
}

/* ================================================================== */
/* DirectPlay_CreatePlayer — DirectPlay enum callback                 */
/* Address: 0x45FBD0                                                   */
/*                                                                     */
/* Called by DirectPlay when enumerating session players.             */
/* ================================================================== */
uint32_t __cdecl DirectPlay_CreatePlayer(const char* playerName, uint32_t param_2,
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
                char* dst = (char*)((uint8_t*)g_dplay_peer + 0xD70);
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
void __fastcall DirectPlay_Close(int32_t session)
{
    uint8_t* s = (uint8_t*)session;

    /* Free session desc memory if allocated */
    if (*(void**)(s + 0x92C) != NULL && s[0x930] == 0) {
        void* hglb = GlobalHandle(*(void**)(s + 0x92C));
        GlobalUnlock(hglb);
        hglb = GlobalHandle(*(void**)(s + 0x92C));
        GlobalFree(hglb);
    }

    s[0x930] = 0;  /* session_desc_valid */
    *(uint32_t*)(s + 0x92C) = 0;

    /* Close player connection and release DirectPlay interface */
    if (*(void**)(s + 0x1588) != NULL) {
        /* Close player connection */
        ((void (__thiscall*)(void*, uint32_t, uint32_t))
            (*(void***)(*(uint32_t*)(s + 0x1588)))[0xCC / 4])
            (*(void**)(s + 0x1588), 0, 0);

        /* Close session */
        ((void (__thiscall*)(void*))
            (*(void***)(*(uint32_t*)(s + 0x1588)))[0x10 / 4])
            (*(void**)(s + 0x1588));

        /* Release interface */
        ((void (__thiscall*)(void*))
            (*(void***)(*(uint32_t*)(s + 0x1588)))[2])
            (*(void**)(s + 0x1588));

        *(void**)(s + 0x1588) = NULL;
    }

    /* Free player list */
    int32_t* player = *(int32_t**)(s + 0xD64);
    while (player != NULL) {
        int32_t* next = (int32_t*)*player;
        if ((void*)player[2] != NULL) {
            GLOBAL_free((void*)player[2]);
        }
        if ((void*)player[1] != NULL) {
            void* hglb = GlobalHandle((void*)player[1]);
            GlobalUnlock(hglb);
            hglb = GlobalHandle((void*)player[1]);
            GlobalFree(hglb);
            player[1] = 0;
        }
        GLOBAL_free(player);
        *(int32_t**)(s + 0xD64) = next;
        player = next;
    }

    /* Free group list */
    player = *(int32_t**)(s + 0xD68);
    while (player != NULL) {
        int32_t* next = (int32_t*)*player;
        if ((void*)player[2] != NULL) {
            GLOBAL_free((void*)player[2]);
        }
        GLOBAL_free(player);
        *(int32_t**)(s + 0xD68) = next;
        player = next;
    }

    /* Reset session state fields */
    s[0x18] = 0;           /* session_name */
    s[0x418] = 0;          /* player_name */
    *(uint32_t*)(s + 0x518) = 0;    /* connection_type */
    s[0x51C] = 0;         /* connection_name */
    *(uint32_t*)(s + 0x920) = 0;    /* session_flags */
    *(uint32_t*)(s + 0x934) = 0;    /* flag_934 */
    s[0xD50] = 0;          /* session_ready */
}

/* ================================================================== */
/* DirectPlay_EnumPlayers — Enumerate session players                  */
/* Address: 0x45FD80                                                   */
/*                                                                     */
/* Builds a session desc, calls IDirectPlay4::EnumPlayers, retries    */
/* on pending. Returns 1 on success.                                  */
/* ================================================================== */
uint32_t __fastcall DirectPlay_EnumPlayers(void* param_1)
{
    uint8_t* s = (uint8_t*)param_1;

    if (*(void**)(s + 0x1588) == NULL) {
        return 0;
    }

    /* Build session desc structure at +0x158C */
    uint32_t* desc = (uint32_t*)(s + 0x158C);
    for (int i = 0; i < 0x14; i++) {
        desc[i] = 0;
    }
    desc[0] = 0x50;

    uint8_t is_host = s[2];
    desc[1] = (is_host ? 0 : 4) + 0xA040;  /* dwFlags */

    desc[6] = *(uint32_t*)0x479158;   /* guid fields */
    desc[7] = *(uint32_t*)0x47915C;
    desc[8] = *(uint32_t*)0x479160;
    desc[9] = *(uint32_t*)0x479164;
    desc[10] = *(uint32_t*)(s + 0x920);  /* dwMaxPlayers */

    desc[12] = (uint32_t)(s + 0x18);    /* lpszSessionName */
    if (*(char*)(s + 0x498) != '\0') {
        desc[13] = (uint32_t)(s + 0x498);  /* lpszPassword */
    }

    /* Call EnumPlayers */
    int32_t hr = ((int32_t (__thiscall*)(void*, void*, uint32_t))
        (*(void***)(*(uint32_t*)(s + 0x1588)))[0x60 / 4])
        (*(void**)(s + 0x1588), desc, 0x82);
    *(int32_t*)(s + 0xD48) = hr;

    /* Retry on pending */
    while (hr == -0x7788fea2 && s[1] == 0) {
        Sleep(1);
        hr = ((int32_t (__thiscall*)(void*, void*, uint32_t))
            (*(void***)(*(uint32_t*)(s + 0x1588)))[0x60 / 4])
            (*(void**)(s + 0x1588), desc, 0x82);
        *(int32_t*)(s + 0xD48) = hr;
    }

    hr = *(int32_t*)(s + 0xD48);
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
    WIN32_RecvNetworkData(param_1, 0, msg_buf);
    return hr & 0xFFFFFF00;
}

/* ================================================================== */
/* DirectPlay_Open — Convert DPERR_* HRESULT to string                */
/* Address: 0x45FF30                                                   */
/*                                                                     */
/* Large switch over 40+ DirectPlay error codes. Unknown codes        */
/* formatted as "Unknown Error Code: %d".                             */
/* ================================================================== */
void __cdecl DirectPlay_Open(char* out_buf, int32_t hresult)
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
