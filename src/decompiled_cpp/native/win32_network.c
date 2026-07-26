/**
 * win32_network.c — WIN32 network message handling functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions handle DirectPlay network data send/receive and
 * session join logic. They operate on the large DirectPlay session
 * struct (~0x15E8 bytes, defined in network/DirectPlay.h).
 *
 * Functions:
 *   WIN32_SendNetworkData   (0x460D40) — Send data via DirectPlay
 *   WIN32_RecvNetworkData   (0x460EA0) — Display error message dialog
 *   WIN32_GetSystemMetrics  (0x460360) — Join session with metrics/enum
 *   WIN32_PeekMessageLoop   (0x4606D0) — Main network message pump
 *
 * All operate within the DirectPlay message format where every message
 * has a 2-byte type at offset 0 and a 2-byte subtype (always 300 for
 * network messages) at offset 2.
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {
    void* __cdecl operator_new(size_t size);
    void  __cdecl GLOBAL_free(void* ptr);
    void* __cdecl CRT_malloc_zero(size_t size);
    void  __cdecl CRT_free(void* ptr);

    /* Win32 API */
    void*   __stdcall GetProcessHeap(void);
    void*   __stdcall HeapAlloc(void* hHeap, uint32_t dwFlags, uint32_t dwBytes);
    int32_t __stdcall HeapFree(void* hHeap, uint32_t dwFlags, void* lpMem);
    int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);
    int32_t __stdcall LoadStringA(void* hInstance, uint32_t uID, char* lpBuffer,
                                   int32_t cchBufferMax);
    int32_t __stdcall MessageBoxA(void* hWnd, const char* lpText,
                                   const char* lpCaption, uint32_t uType);
    int32_t __stdcall DialogBoxParamA(void* hInstance, const char* lpTemplateName,
                                       void* hWndParent, void* lpDialogFunc,
                                       int32_t dwInitParam);
    void    __stdcall Sleep(uint32_t dwMilliseconds);

    /* DirectPlay functions */
    void __cdecl DirectPlay_Open(char* out_buf, int32_t hresult);
    void __fastcall DirectPlay_Close(int32_t session);

    /* String constants */
    extern char g_empty_string;          /* 0x4851D0 */
    extern const char DAT_004818e0[4];   /* format string separator */
}

/* ================================================================== */
/* WIN32_SendNetworkData — Send data via DirectPlay                   */
/* Address: 0x460D40                                                   */
/* Calling convention: __thiscall (_this = session struct)              */
/*                                                                     */
/* Prepends a 2-byte message header (subtype 300 = 0x12C), sends via  */
/* IDirectPlay4::Send or IDirectPlay4::SendEx depending on flags.     */
/* Uses vtable[0x68/4] = Send (offset 0x68) or vtable[0xC4/4] =       */
/* SendEx (offset 0xC4) depending on session flag at +0x15E8 bitmap.  */
/*                                                                     */
/* @param _this    DirectPlay session struct                            */
/* @param dpId    Target player DirectPlay ID (or 0 for broadcast)     */
/* @param data    Pointer to message data (first 4 bytes = header)    */
/* @param size    Size of data payload                                 */
/* @param flags   Send flags (bit 0 = async, bit 1 = no timeout, etc) */
/* @return        dpId on success, 0 on error                         */
/* ================================================================== */
uint32_t __thiscall WIN32_SendNetworkData(void* _this, uint32_t dpId,
                                           int32_t data, uint32_t size,
                                           uint32_t flags)
{
    uint8_t* s = (uint8_t*)_this;

    if (*(void**)(s + 0x1588) == NULL) {
        return 0;  /* no DirectPlay interface */
    }

    /* Set message type header (always 300 = 0x12C for network msgs) */
    *(uint16_t*)(data + 2) = 300;

    /* If not async and max_players set, acquire send semaphore */
    if ((flags & 1) == 0 && *(int32_t*)(s + 0xD5C) != 0) {
        uint32_t timeout = 1;
        int32_t wait_result = 0;
        ((int32_t (__thiscall*)(void*, uint32_t))
            (*(void***)(*(uint32_t*)(s + 0x1588)))[200])
            (*(void**)(s + 0x1588), 0);  /* Send with 0 timeout */
    }

    void* dplay = *(void**)(s + 0x1588);

    if ((*(uint32_t*)(s + 0x15E8) & 0x10000) == 0) {
        /* Use IDirectPlay4::Send (vtable[0x68/4]) */
        int32_t hr = ((int32_t (__thiscall*)(void*, uint32_t, uint32_t, uint32_t, uint32_t, void*))
            (*(void***)dplay)[0x68 / 4])
            (dplay, dpId, *(uint32_t*)(s + 0x924), flags, size, (void*)data);
        *(int32_t*)(s + 0xD48) = hr;

        if (hr < 0 && hr != -0x7FFFFFF6) {  /* DPERR_PENDING is okay */
            char err_buf[300];
            DirectPlay_Open(err_buf, hr);
            WIN32_RecvNetworkData(_this, 0x7D03, err_buf);
            return 0;
        }
    } else {
        /* Use IDirectPlay4::SendEx (vtable[0xC4/4]) */
        uint32_t send_flags = flags | 0x600;
        int32_t hr = ((int32_t (__thiscall*)(void*, uint32_t, uint32_t, uint32_t, void*))
            (*(void***)dplay)[0xC4 / 4])
            (dplay, *(uint32_t*)(s + 0x924), dpId, send_flags, (void*)data);
        *(int32_t*)(s + 0xD48) = hr;

        if (hr < 0 && hr != -0x7FFFFFF6) {
            char err_buf[300];
            DirectPlay_Open(err_buf, hr);
            WIN32_RecvNetworkData(_this, 0x7D03, err_buf);
            return 0;
        }
    }

    return dpId;  /* return target dpId as success indicator */
}

/* ================================================================== */
/* WIN32_RecvNetworkData — Display error message                       */
/* Address: 0x460EA0                                                   */
/* Calling convention: __thiscall                                      */
/*                                                                     */
/* Shows an error message dialog. Loads string resources for caption   */
/* and format, concatenates the user-provided message with a resource  */
/* string, and presents the result via callback or MessageBoxA.       */
/*                                                                     */
/* @param _this    DirectPlay session struct (has hInstance at +0x93C) */
/* @param resId   String resource ID for error format message          */
/* @param msg     Error message text to display                        */
/* @return        0                                                    */
/* ================================================================== */
int32_t __thiscall WIN32_RecvNetworkData(void* _this, uint32_t resId, const char* msg)
{
    uint8_t* s = (uint8_t*)_this;

    /* Local format buffers (0x200 bytes each) */
    char caption[0x200];
    char fmt_buf[0x200];

    caption[0] = g_empty_string;
    for (int i = 1; i < 0x200; i++) caption[i] = 0;

    fmt_buf[0] = g_empty_string;
    for (int i = 1; i < 0x200; i++) fmt_buf[i] = 0;

    /* Load error format string if resId is set */
    if (resId != 0) {
        LoadStringA(*(void**)(s + 0x93C), resId, fmt_buf, 0x200);
    }

    if (msg != NULL) {
        if (resId != 0) {
            /* Concatenate: append msg to fmt_buf */
            char separator[] = { ' ', '-', ' ', 0 };  /* " - " */
            char* dst = fmt_buf;
            while (*dst) dst++;
            char* src = separator;
            while (*src) { *dst++ = *src++; }
            src = (char*)msg;
            while (*src) { *dst++ = *src++; }
            *dst = 0;
        } else {
            /* Copy msg as the error text */
            char* dst = fmt_buf;
            const char* src = msg;
            while (*src) { *dst++ = *src++; }
            *dst = 0;
        }
    }

    /* Load caption string (resource 0x7D06 = "LEGO LOCO") */
    LoadStringA(*(void**)(s + 0x93C), 0x7D06, caption, 0x200);

    /* Store error in buffer at +0x945 */
    char* err_dst = (char*)(s + 0x945);
    const char* src = fmt_buf;
    while (*src) { *err_dst++ = *src++; }
    *err_dst = 0;

    /* Dispatch via callback or MessageBox */
    if (*(void**)(s + 0x940) != NULL) {
        ((void (*)(char*))(*(uint32_t*)(s + 0x940)))(fmt_buf);
        return 0;
    }

    if (s[0x944] != 0) {
        return MessageBoxA(*(void**)(s + 0x938), fmt_buf, caption, 0x40000);
    }

    return 0;
}

/* ================================================================== */
/* WIN32_GetSystemMetrics — Join a DirectPlay session                  */
/* Address: 0x460360                                                   */
/* Calling convention: __fastcall (ECX = session struct)              */
/*                                                                     */
/* Joins a session by opening the session description, setting up the  */
/* address, and enumerating players. Uses a dialog for interactive     */
/* setup when no session name is provided.                             */
/*                                                                     */
/* Called during multiplayer setup flow after DirectPlay_HandleMessages*/
/*                                                                     */
/* @param param_1  DirectPlay session struct                           */
/* @return         1 on success, 0 on failure                          */
/* ================================================================== */
uint32_t __fastcall WIN32_GetSystemMetrics(void* param_1)
{
    uint8_t* s = (uint8_t*)param_1;

    /* Build session desc structure at +0x158C (0x50 bytes) */
    uint32_t* desc = (uint32_t*)(s + 0x158C);
    for (int i = 0; i < 0x14; i++) {
        desc[i] = 0;
    }
    desc[0] = 0x50;  /* dwSize */

    /* Set GUID fields */
    desc[6] = *(uint32_t*)0x479158;
    desc[7] = *(uint32_t*)0x47915C;
    desc[8] = *(uint32_t*)0x479160;
    desc[9] = *(uint32_t*)0x479164;

    /* Set session name pointer if non-empty */
    if (*(char*)(s + 0x498) != '\0') {
        *(uint32_t*)(s + 0x15C0) = (uint32_t)(s + 0x498);
    }

    /* Check if we have a session name */
    const char* session_name = (const char*)(s + 0x18);
    int32_t nameLen = 0;
    while (session_name[nameLen] != 0) nameLen++;

    if (nameLen == 0) {
        /* No session name — show interactive dialog (ID 0x7D0B) */
        int32_t result = DialogBoxParamA(NULL, (const char*)0x7D0B,
                                          *(void**)(s + 0x938),
                                          (void*)0x4611B0, 0);
        if (result == 0) {
            return 0;
        }
    } else {
        /* Have a session name — proceed with DirectPlay */

        if (*(void**)(s + 0x1588) == NULL) {
            return 0;
        }

        /* If not host, free existing session desc */
        if (s[1] == 0) {
            if (*(void**)(s + 0x92C) != NULL && s[0x930] == 0) {
                void* hglb = GlobalHandle(*(void**)(s + 0x92C));
                GlobalUnlock(hglb);
                hglb = GlobalHandle(*(void**)(s + 0x92C));
                GlobalFree(hglb);
            }
            s[0x930] = 0;
            *(uint32_t*)(s + 0x92C) = 0;
        }

        /* Open session via IDirectPlay4::Open (vtable[0x34/4]) */
        if (*(uint32_t*)(s + 0x92C) == 0) {
            int32_t hr = ((int32_t (__thiscall*)(void*, void*, uint32_t, void*, uint32_t, uint32_t))
                (*(void***)(*(uint32_t*)(s + 0x1588)))[0x34 / 4])
                (*(void**)(s + 0x1588), desc, 0, (void*)0x460620,
                 *(uint32_t*)(s + 0x938), 0x81);
            *(int32_t*)(s + 0xD48) = hr;

            while (hr == -0x7788fea2) {  /* DPERR_PENDING */
                if (s[1] != 0) goto exit_user_cancel;
                if (*(void**)(s + 0xD4C) != NULL) {
                    ((void (*)(void))(*(uint32_t*)(s + 0xD4C)))();
                }
                Sleep(1);
                hr = ((int32_t (__thiscall*)(void*, void*, uint32_t, void*, uint32_t, uint32_t))
                    (*(void***)(*(uint32_t*)(s + 0x1588)))[0x34 / 4])
                    (*(void**)(s + 0x1588), desc, 0, (void*)0x460620,
                     *(uint32_t*)(s + 0x938), 0x81);
                *(int32_t*)(s + 0xD48) = hr;
            }

            if (*(uint32_t*)(s + 0x92C) == 0) {
                return 0;
            }
        }
    }

    /* Copy session desc data from +0x92C into desc structure */
    {
        uint32_t* src = *(uint32_t**)(s + 0x92C);
        desc[1] = src[0];   /* dwFlags */
        desc[2] = src[1];   /* guidInstance */
        desc[3] = src[2];
        desc[4] = src[3];
    }

    /* Enumerate players via IDirectPlay4::EnumPlayers (vtable[0x60/4]) */
    int32_t hr = ((int32_t (__thiscall*)(void*, void*, uint32_t))
        (*(void***)(*(uint32_t*)(s + 0x1588)))[0x60 / 4])
        (*(void**)(s + 0x1588), desc, 0x81);
    *(int32_t*)(s + 0xD48) = hr;

    while (hr == -0x7788fea2) {
        if (s[1] != 0) goto exit_user_cancel;
        Sleep(1);
        hr = ((int32_t (__thiscall*)(void*, void*, uint32_t))
            (*(void***)(*(uint32_t*)(s + 0x1588)))[0x60 / 4])
            (*(void**)(s + 0x1588), desc, 0x81);
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

    /* Error path */
    {
        char err_buf[300];
        char msg_buf[300];
        DirectPlay_Open(err_buf, hr);
        wsprintfA(msg_buf, "Failed to join session: Direct Play Error: %s", err_buf);
        WIN32_RecvNetworkData(param_1, 0x7D05, msg_buf);
    }

exit_user_cancel:
    return hr & 0xFFFFFF00;
}

/* ================================================================== */
/* WIN32_PeekMessageLoop — Main network message pump                   */
/* Address: 0x4606D0                                                   */
/* Calling convention: __fastcall (ECX = session struct)              */
/*                                                                     */
/* Main network message processing loop. Receives DirectPlay messages  */
/* in a loop, dispatches by message type (3=player_info, 5=join,       */
/* 0x101=session_lost, 0x103=player_info, 0x104=name_query). Responds  */
/* with appropriate response packets.                                   */
/*                                                                     */
/* Heap-allocates message buffers (0x800-byte data + 0x400-byte       */
/* secondary). Each message has format: [type:2][subtype:2][data...]  */
/*                                                                     */
/* @param param_1  DirectPlay session struct (fields at +0x1588,       */
/*                 +0x924 = dpId, +0x18 = session_name,               */
/*                 +0x418 = player_name)                               */
/* @return         Pointer to a heap-allocated response struct,        */
/*                 or NULL on no-messages completion                   */
/* ================================================================== */
undefined4* __fastcall WIN32_PeekMessageLoop(uint8_t* param_1)
{
    if (*(void**)(param_1 + 0x1588) == NULL) {
        return NULL;
    }

    /* --- Initialization: allocate heap buffers --- */
    void* heap = GetProcessHeap();
    void* data_buf = HeapAlloc(heap, 0, 0x7FF);    /* 2047 bytes */
    void* name_buf = HeapAlloc(heap, 0, 0x400);     /* 1024 bytes */
    uint8_t* dpId_ptr = param_1 + 0x924;

    /* --- Main message loop --- */
    while (1) {
        /* Allocate receive buffers */
        int32_t buf_size = 0x7FF;
        void* recv_buf = HeapAlloc(GetProcessHeap(), 0, buf_size);
        void* msg_info = HeapAlloc(GetProcessHeap(), 0, 4);  /* 4-byte msg info */
        uint32_t msg_info_value = 0;

        /* Receive message via IDirectPlay4::Receive (vtable[100/4]) */
        int32_t hr = ((int32_t (__thiscall*)(void*, uint32_t, void*, uint32_t, void*, uint32_t*))
            (*(void***)(*(uint32_t*)(param_1 + 0x1588)))[100])
            (*(void**)(param_1 + 0x1588), 4, msg_info, 1, recv_buf, &buf_size);

        *(int32_t*)(param_1 + 0xD48) = hr;

        /* Handle DPERR_BUFFERTOOSMALL (retry with larger buffer) */
        while (hr == -0x7788ffe2) {  /* DPERR_BUFFERTOOSMALL */
            HeapFree(GetProcessHeap(), 0, recv_buf);
            buf_size += 0x7FF;
            recv_buf = HeapAlloc(GetProcessHeap(), 0, buf_size);
            hr = ((int32_t (__thiscall*)(void*, uint32_t, void*, uint32_t, void*, uint32_t*))
                (*(void***)(*(uint32_t*)(param_1 + 0x1588)))[100])
                (*(void**)(param_1 + 0x1588), 4, msg_info, 1, recv_buf, &buf_size);
            *(int32_t*)(param_1 + 0xD48) = hr;
        }

        /* Check for errors */
        if (*(int32_t*)(param_1 + 0xD48) != 0) {
            HeapFree(GetProcessHeap(), 0, recv_buf);
            HeapFree(GetProcessHeap(), 0, msg_info);

            if (*(int32_t*)(param_1 + 0xD48) == -0x7788ff42) {  /* DPERR_NOMESSAGES */
                HeapFree(GetProcessHeap(), 0, data_buf);
                HeapFree(GetProcessHeap(), 0, name_buf);
                return NULL;  /* normal: no more messages */
            }

            /* Error — show message */
            char err_msg[300];
            DirectPlay_Open(name_buf, *(int32_t*)(param_1 + 0xD48));
            wsprintfA(data_buf, "Network Receive failed: %s", name_buf);
            WIN32_RecvNetworkData(param_1, 0, data_buf);
            HeapFree(GetProcessHeap(), 0, data_buf);
            HeapFree(GetProcessHeap(), 0, name_buf);
            return NULL;
        }

        /* Process received message (msg_info[0] = sender dpId) */
        uint32_t sender_dpId = *(uint32_t*)msg_info;
        if (sender_dpId != 0) {
            uint16_t msg_type = *(uint16_t*)((uint8_t*)recv_buf + 2);  /* subtype at +2 */
            uint16_t msg_flags = *(uint16_t*)recv_buf;                 /* type at +0 */

            /* --- Type 0x32 (player info/session data) --- */
            if (msg_flags == 0x32) {
                uint32_t data_size = *(uint32_t*)((uint8_t*)recv_buf + 6) & 0xFFFF;  /* size field */

                /* Free old session data */
                if (*(void**)(param_1 + 0xD58) != NULL) {
                    GLOBAL_free(*(void**)(param_1 + 0xD58));
                    *(uint32_t*)(param_1 + 0xD54) = 0;
                }

                /* Allocate and copy session data */
                uint32_t* data_ptr = (uint32_t*)operator_new(data_size);
                *(uint32_t**)(param_1 + 0xD58) = data_ptr;
                uint16_t* src = (uint16_t*)((uint8_t*)recv_buf + 10);
                for (uint32_t i = 0; i < data_size / 4; i++) {
                    data_ptr[i] = *(uint32_t*)src;
                    src += 2;
                }
                for (uint32_t i = data_size & ~3; i < data_size; i++) {
                    ((uint8_t*)data_ptr)[i] = ((uint8_t*)src)[i - data_size + 2];
                }

                *(uint32_t*)(param_1 + 0xD54) = data_size;

                /* Build ack: 0x28 struct with 300 subtype */
                uint16_t* ack = (uint16_t*)HeapAlloc(GetProcessHeap(), 0, 4);
                ack[0] = 0x28;   /* type = 0x28 (ACK) */
                ack[1] = 300;    /* subtype = 300 */

                uint32_t* result = (uint32_t*)HeapAlloc(GetProcessHeap(), 0, 8);
                result[0] = 0;
                result[1] = (uint32_t)ack;

                HeapFree(GetProcessHeap(), 0, recv_buf);
                HeapFree(GetProcessHeap(), 0, msg_info);
                HeapFree(GetProcessHeap(), 0, data_buf);
                HeapFree(GetProcessHeap(), 0, name_buf);
                return result;
            }

            /* --- Other message types --- */
            {
                uint32_t* response = (uint32_t*)HeapAlloc(GetProcessHeap(), 0, 8);
                response[0] = 0;
                response[1] = (uint32_t)recv_buf;

                /* Build response packet based on type */
                uint16_t* packet = NULL;

                /* Type 3 (0x03) — player join */
                if (msg_flags == 3) {
                    uint32_t pkt_size = 0x88;
                    packet = (uint16_t*)HeapAlloc(GetProcessHeap(), 0, pkt_size);
                    packet[0] = 0x46;     /* type = 0x46 */
                    packet[1] = 300;      /* subtype */
                    packet[2] = (uint16_t)(*(uint32_t*)((uint8_t*)recv_buf + 4) & 0xFFFF); /* flags from msg */
                    packet[4] = 0;        /* padding */
                    /* Copy player name from recv_buf[9*4]=offset 0x24 */
                    const char* player_name = (const char*)recv_buf + 0x24;
                    char* dst = (char*)(packet + 4);
                    /* Copy name (max 0x80 bytes) */
                    int32_t i = 0;
                    while (player_name[i] != 0 && i < 0x7F) {
                        dst[i] = player_name[i];
                        i++;
                    }
                    dst[i] = 0;
                }
                /* Type 0x101 (257) — session lost */
                else if (msg_flags == 0x101) {
                    param_1[0] = 1;
                    *(uint32_t*)(param_1 + 0x920) = 0;
                    param_1[1] = 0;
                    param_1[2] = 0;
                    packet = (uint16_t*)HeapAlloc(GetProcessHeap(), 0, 4);
                    packet[0] = 0x3C;   /* type = 0x3C (session lost ack) */
                    packet[1] = 300;
                }
                /* Type 0x103 (259) — player info/name */
                else if (msg_flags == 0x103) {
                    uint32_t pkt_size = 0x88;
                    packet = (uint16_t*)HeapAlloc(GetProcessHeap(), 0, pkt_size);
                    packet[0] = 0x50;     /* type = 0x50 */
                    packet[1] = 300;
                    uint32_t flags_field = *(uint32_t*)((uint8_t*)recv_buf + 4);
                    packet[2] = (uint16_t)(flags_field & 0xFFFF);
                    packet[4] = 0;  /* padding */

                    /* Copy player name from recv_buf[6*4]=offset 0x18 */
                    const char* player_name = (const char*)recv_buf + 0x18;
                    char* dst = (char*)(packet + 4);
                    int32_t i = 0;
                    while (player_name[i] != 0 && i < 0x7F) {
                        dst[i] = player_name[i];
                        i++;
                    }
                    dst[i] = 0;
                }
                /* Type 0x104 (260) — name query */
                else if (msg_flags == 0x104) {
                    /* Compare query name with our session name */
                    const char* query_name = (const char*)recv_buf + 0x34;  /* offset 0xD*4 */
                    const char* our_name = (const char*)(param_1 + 0x18);   /* session_name */
                    int32_t cmp = 0;
                    /* String comparison loop */
                    {
                        const uint8_t* a = (const uint8_t*)query_name;
                        const uint8_t* b = (const uint8_t*)our_name;
                        while (1) {
                            if (*a != *b) {
                                cmp = (*a < *b) ? -1 : 1;
                                break;
                            }
                            if (*a == 0) break;
                            a++;
                            b++;
                        }
                    }

                    if (cmp != 0) {
                        /* Names don't match — send our session name back */
                        uint32_t pkt_size = 0x404;
                        packet = (uint16_t*)HeapAlloc(GetProcessHeap(), 0, pkt_size);
                        packet[0] = 0x5A;   /* type = 0x5A (name response) */
                        packet[1] = 300;
                        /* Copy our session name */
                        char* dst = (char*)(packet + 2);
                        const char* src = our_name;
                        int32_t i = 0;
                        while (src[i] != 0 && i < 0x3FF) {
                            dst[i] = src[i];
                            i++;
                        }
                        dst[i] = 0;
                    }
                }

                /* Send response packet if one was created */
                if (packet != NULL) {
                    uint32_t* result = (uint32_t*)HeapAlloc(GetProcessHeap(), 0, 8);
                    result[0] = (uint32_t)recv_buf;
                    result[1] = (uint32_t)response;

                    /* Close message info */
                    HeapFree(GetProcessHeap(), 0, msg_info);

                    /* Send response via WIN32_SendNetworkData */
                    WIN32_SendNetworkData(param_1, sender_dpId, (int32_t)packet, 4, 1);

                    HeapFree(GetProcessHeap(), 0, recv_buf);
                    HeapFree(GetProcessHeap(), 0, data_buf);
                    HeapFree(GetProcessHeap(), 0, name_buf);
                    return result;
                }
            }
        }

        /* Free buffers for next iteration */
        if (recv_buf != NULL) {
            HeapFree(GetProcessHeap(), 0, recv_buf);
        }
        HeapFree(GetProcessHeap(), 0, msg_info);
    }
}
