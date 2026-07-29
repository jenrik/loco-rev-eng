/**
 * NETMAN_SessionSettings — Network settings persistence (NetSettings.dat)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Functions for loading and saving network session configuration to
 * NetSettings.dat in the install directory. Operates on a packet/struct
 * (~0xB0 bytes) that is part of _g_netman_data or a DPLAY config struct.
 *
 * Contains:
 *   NETMAN_DestroySession (0x441F80) — Session window proc (EDIT/ESC handling)
 *   NETMAN_FreePacket     (0x440D00) — Load NetSettings.dat (MISNAMED)
 *   NETMAN_SendPacket     (0x440EA0) — Save NetSettings.dat
 *   NETMAN_AllocPacket    (0x440CC0) — Destructor: free linked list + optional free
 *
 * === Settings packet struct (at packetPtr) ===
 *   +0x00: vtable (VTBL 0x4781CC for AllocPacket)
 *   +0x04: header/magic word (0x6A = 'j')
 *   +0x06: loaded flag
 *   +0x07: flag
 *   +0x08: flag
 *   +0x0C: timeout setting (default 0x1E = 30)
 *   +0x10: linked list head (provider list)
 *   +0x18..+0xAC: various settings
 */
#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern char g_install_path[];           /* 0x4A99C8 */
extern char g_empty_string;             /* 0x4851D0 */

extern "C" {

extern int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);
extern void*   __stdcall CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess,
                                      uint32_t dwShareMode, void* lpSecurityAttributes,
                                      uint32_t dwCreationDisposition,
                                      uint32_t dwFlagsAndAttributes, void* hTemplateFile);
extern int32_t __stdcall ReadFile(void* hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead,
                                   uint32_t* lpNumberOfBytesRead, void* lpOverlapped);
extern int32_t __stdcall WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
                                    uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
extern int32_t __stdcall CloseHandle(void* hObject);
extern void*   __stdcall GetProcessHeap(void);
extern int32_t __stdcall HeapFree(void* hHeap, uint32_t dwFlags, void* lpMem);
extern int32_t __stdcall IsWindowVisible(void* hWnd);
extern void    __stdcall Sleep(uint32_t dwMilliseconds);
extern LRESULT __stdcall DefWindowProcA(void* hWnd, uint32_t Msg, uint32_t wParam, uint32_t lParam);
extern int32_t __stdcall GetWindowTextA(void* hWnd, char* lpString, int32_t nMaxCount);
extern void    __stdcall ShowWindow(void* hWnd, int32_t nCmdShow);

extern void* __cdecl    GLOBAL_free(void* ptr);
extern void* __cdecl    operator_new(size_t size);
extern void  __cdecl    UI_MainMenu_SetState(void* ui_main, int32_t state);
extern void  __cdecl    FormatResourceString(void* resmgr, uint32_t id, char* buf, int32_t bufsize);
extern void  __cdecl    PlaySound(int32_t soundId);
extern void  __cdecl    Sprite_SetState(void* sprite, int32_t state, int32_t* unk);
extern void  __thiscall UIPANEL_EndPaint(void* panel);
extern void  __thiscall UIPANEL_EndPaintEx(void* panel, void* hwnd, int32_t hdc,
                                            uint8_t repaint, void* updateRect);
extern void  __fastcall NETMAN_GetSessionInfo(int32_t panel);

extern void* _g_netman_data;           /* 0x4FD3A8 */
extern void* g_ui_main;                /* 0x4A8860 */
extern void* g_resmgr;                  /* resource manager */
}

/* Format: "%s\\%s" */
#define FMT_FILE_PATH "%s\\%s"          /* 0x47E8A0 */

#define STR_NET_SETTINGS "NetSettings.dat"  /* 0x47EB74 */

/* ================================================================== */
/* NETMAN_FreePacket — 0x440D00                                        */
/* MISNAMED: _this function LOADS network settings from NetSettings.dat.*/
/* ================================================================== */
void __fastcall NETMAN_FreePacket(uint8_t* packetPtr)
{
    char filepath[0x504];
    uint32_t bytesRead;

    /* Save linked list head before overwriting */
    void* savedList = *(void**)(packetPtr + 0x10);
    *(void**)(packetPtr + 0x10) = NULL;

    /* Clear loaded flag */
    *(uint8_t*)(packetPtr + 6) = 0;

    /* Build path */
    {
        int32_t i;
        filepath[0] = g_empty_string;
        for (i = 0; i < 0x140; i++) {
            ((uint32_t*)&filepath[1])[i] = 0;
        }
        filepath[0x501] = 0;
        filepath[0x502] = 0;
    }

    wsprintfA(filepath, FMT_FILE_PATH, g_install_path, STR_NET_SETTINGS);

    /* Try to open and read existing settings */
    void* hFile = CreateFileA(filepath, 0x80000000, 1, NULL, 3, 0x8000000, NULL);
    if (hFile == (void*)-1) {
        *(void**)(packetPtr + 0x10) = savedList;
        return;
    }

    bytesRead = 0;
    if (!ReadFile(hFile, packetPtr + 4, 0xAC, &bytesRead, NULL)) {
        CloseHandle(hFile);
        *(void**)(packetPtr + 0x10) = savedList;
        return;
    }
    CloseHandle(hFile);

    /* Check magic marker */
    if (*(uint16_t*)(packetPtr + 4) == 0x6A) {
        /* Already initialized — mark loaded */
        *(uint8_t*)(packetPtr + 6) = 1;
    } else {
        /* First run — initialize defaults */
        *(uint16_t*)(packetPtr + 4) = 0x6A;      /* magic */
        *(uint8_t*)(packetPtr + 0x18) = 0;        /* flag */
        *(int32_t*)(packetPtr + 0x1C) = 4;        /* default: 2-player mode */
        *(int32_t*)(packetPtr + 0x20) = 2;        /* default: 4-player mode */
        *(uint8_t*)(packetPtr + 0x24) = 0;
        *(int32_t*)(packetPtr + 0x28) = 4;
        *(uint8_t*)(packetPtr + 0x2C) = 0;
        *(uint8_t*)(packetPtr + 0x6C) = 0;
        *(int32_t*)(packetPtr + 0xAC) = 2;
        *(uint8_t*)(packetPtr + 7) = 1;
        *(uint8_t*)(packetPtr + 8) = 0;
        *(int32_t*)(packetPtr + 0x0C) = 0x1E;    /* 30-second timeout */
        *(uint8_t*)(packetPtr + 6) = 0;

        /* Save defaults to file */
        {
            uint32_t bytesWritten;
            void* hOut = CreateFileA(filepath, 0x40000000, 1, NULL, 2, 0x8000000, NULL);
            if (hOut != (void*)-1) {
                WriteFile(hOut, packetPtr + 4, 0xAC, &bytesWritten, NULL);
                CloseHandle(hOut);
            }
        }
    }

    *(void**)(packetPtr + 0x10) = savedList;
}

/* ================================================================== */
/* NETMAN_SendPacket — 0x440EA0                                        */
/* Save network settings to NetSettings.dat.                           */
/* ================================================================== */
void __fastcall NETMAN_SendPacket(uint8_t* packetPtr)
{
    char filepath[0x504];
    uint32_t bytesWritten;

    wsprintfA(filepath, FMT_FILE_PATH, g_install_path, STR_NET_SETTINGS);

    void* hFile = CreateFileA(filepath, 0x40000000, 1, NULL, 2, 0x8000000, NULL);
    if (hFile != (void*)-1) {
        WriteFile(hFile, packetPtr + 4, 0xAC, &bytesWritten, NULL);
        CloseHandle(hFile);
    }
}

/* ================================================================== */
/* NETMAN_AllocPacket — 0x440CC0                                       */
/* MISNAMED: _this is a destructor that frees a linked list at +0x10.   */
/* ================================================================== */
void* __thiscall NETMAN_AllocPacket(void* _this, uint8_t flags)
{
    /* Free linked list at +0x10 */
    void* list = *(void**)((uint8_t*)_this + 0x10);
    *(void***)_this = (void**)0x4781CC;  /* Restore vtable */

    while (list != NULL) {
        *(void**)((uint8_t*)_this + 0x10) = *(void**)list;
        GLOBAL_free(list);
        list = *(void**)((uint8_t*)_this + 0x10);
    }
    return _this;
}

/* ================================================================== */
/* NETMAN_DestroySession — 0x441F80                                    */
/* Session window proc: handles ENTER (0xD) and ESC (0x1B) keys.      */
/* ================================================================== */
LRESULT __thiscall NETMAN_DestroySession(void* panel, void* hWnd, uint32_t msg,
                                          uint32_t wParam, uint32_t lParam)
{
    if (*(uint8_t*)((uint8_t*)panel + 0x148) == 0) {
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    if (wParam != 0x0D && wParam != 0x1B) {
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    if (wParam == 0x1B) {
        /* ESC pressed — cancel/back */
        Sprite_SetState(*(void**)((uint8_t*)panel + 0x1B4), 1, NULL);
        UIPANEL_EndPaintEx(panel, *(void**)((uint8_t*)panel + 8), 0, 0, NULL);
        Sleep(0x96);
        (**(void(**)(int32_t, int32_t, int32_t, int32_t, int32_t))((uint8_t*)*(void**)panel + 0x10))
            (0, 0, 0, 0, 1);
        UI_MainMenu_SetState(g_ui_main, 7);
        return 0;
    }

    if (wParam == 0x0D) {
        /* ENTER pressed — confirm/join */
        Sprite_SetState(*(void**)((uint8_t*)panel + 0x1B0), 1, NULL);
        UIPANEL_EndPaintEx(panel, *(void**)((uint8_t*)panel + 8), 0, 0, NULL);
        Sleep(0x96);
        (**(void(**)(int32_t, int32_t, int32_t, int32_t, int32_t))((uint8_t*)*(void**)panel + 0x10))
            (0, 0, 0, 0, 1);

        GetWindowTextA(*(void**)((uint8_t*)panel + 0x1D8),
                       (char*)((uint8_t*)_g_netman_data + 0x6C), 0x40);
        if (*(char*)((uint8_t*)_g_netman_data + 8) == '\0') {
            *(uint8_t*)((uint8_t*)_g_netman_data + 0x24) = 1;
        } else {
            *(uint8_t*)((uint8_t*)_g_netman_data + 0x18) = 1;
        }
        NETMAN_SendPacket((uint8_t*)_g_netman_data);
        UI_MainMenu_SetState(g_ui_main, 3);
        return 0;
    }

    return 0;
}
