/**
 * Netman_standalone.c — Standalone NETMAN helper functions (session panel + settings)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file contains NETMAN-prefixed C free functions that operate on:
 *   1. DPlayConfig (GameConfig) — settings persistence (FreePacket/SendPacket)
 *   2. Session panel UI (CGWND/NameEntryPanel) — join, leave, enumerate, etc.
 *
 * These are standalone C functions (not Netman class methods) despite the
 * NETMAN_ prefix, following the original code organization.
 *
 * === Functions ===
 *   NETMAN_FreePacket (0x440D00) — Load NetSettings.dat
 *   NETMAN_SendPacket (0x440EA0) — Save NetSettings.dat
 *   NETMAN_EnumerateSessions (0x441720) — Create session name edit control
 *   NETMAN_JoinSession (0x441870) — Initialize and show join panel
 *   NETMAN_CreateSession (0x4419C0) — Set mode flags from provider list
 *   NETMAN_LeaveSession (0x441A00) — Cleanup and hide join panel
 *   NETMAN_UpdateSessionInfo (0x441A90) — Blit surface, update sprites
 *   NETMAN_GetSessionInfo (0x441B40) — Update sprite visibility by mode
 *   NETMAN_SetSessionInfo (0x441C80) — Handle mouse clicks on session panel
 *   NETMAN_DestroySession (0x441F80) — Window proc for session panel
 */

#include "../shared/types.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External declarations (Win32 API + engine globals)                  */
/* ================================================================== */

extern char   g_empty_string;              /* 0x4851D0 */
extern char   g_install_path[];            /* 0x4A99C8 */
extern void*  g_primary_surface;           /* 0x4FD3C4 */
extern void*  g_ui_main;                   /* 0x4A8860 */
extern void*  g_resmgr;                    /* 0x4855E8 */

/* External functions */
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
extern int32_t __stdcall ShowWindow(void* hWnd, int32_t nCmdShow);
extern int32_t __stdcall SetWindowTextA(void* hWnd, const char* lpString);
extern int32_t __stdcall GetWindowTextA(void* hWnd, char* lpString, int32_t nMaxCount);
extern int32_t __stdcall PostMessageA(void* hWnd, uint32_t Msg, void* wParam, uint32_t lParam);
extern void*   __stdcall SetWindowLongA(void* hWnd, int32_t nIndex, void* dwNewLong);
extern void*   __stdcall CreateWindowExA(uint32_t dwExStyle, const char* lpClassName,
                                          const char* lpWindowName, uint32_t dwStyle,
                                          int32_t x, int32_t y, int32_t nWidth, int32_t nHeight,
                                          void* hWndParent, void* hMenu,
                                          void* hInstance, void* lpParam);
extern void*   __stdcall SetTimer(void* hWnd, uint32_t nIDEvent, uint32_t uElapse, void* lpTimerFunc);
extern int32_t __stdcall KillTimer(void* hWnd, void* nIDEvent);
extern void    __stdcall SetFocus(void* hWnd);
extern void    __stdcall Sleep(uint32_t dwMilliseconds);
extern int32_t __stdcall PtInRect(const void* lprc, int32_t pt);  /* packed POINT */
extern void*   __stdcall DefWindowProcA(void* hWnd, uint32_t Msg, void* wParam, void* lParam);

/* Engine functions */
extern void  __cdecl    UIPANEL_Blit(void* surface, uint32_t srcX, uint32_t srcY,
                                      int32_t srcW, uint32_t srcH, void* dstSurface,
                                      uint32_t dstX, uint32_t dstY, int32_t dstW,
                                      uint32_t dstH, int32_t flags);
extern void  __thiscall UIPANEL_EndPaintEx(void* panel, void* hwnd, int32_t hdc,
                                            uint8_t repaint, void* updateRect);
extern void  __fastcall UIPANEL_EndPaint(void* panel);
extern void  __cdecl    Sprite_SetState(void* sprite, int32_t state, int32_t* unk);
extern void  __cdecl    Sprite_Init(void* sprite);
extern void  __cdecl    Sprite_Destroy(int32_t sprite);
extern void  __cdecl    UI_WindowBase_Show(void* window);
extern void  __cdecl    UI_WindowBase_Hide(void* window);
extern void  __cdecl    UI_MainMenu_SetState(void* ui_main, int32_t state);
extern void  __cdecl    PlaySound(int32_t soundId);
extern void  __cdecl    PlaySoundAt(int32_t soundId, int32_t x, int32_t y, int32_t flags);
extern void  __cdecl    FormatResourceString(void* resmgr, uint32_t id, char* buf, int32_t bufsize);
extern int32_t __cdecl  CRT_rand(void);
extern void* __thiscall ResourceManager_GetById(void* resmgr, uint32_t id);
extern int32_t __cdecl  ResourceManager_GetStringById(void* resmgr, uint32_t id);
extern void  __cdecl    RESMGR_LoadSoundResource(int32_t resId);
extern void  __fastcall NETMAN_GetSessionInfo(int32_t panel);
extern void  __fastcall NETMAN_SendPacket(int32_t packetPtr);

/* DPlayManager (for RenderConnectionPanel call) */
extern void  __fastcall DPlayManager_RenderConnectionPanel(void* panel);

/* ================================================================== */
/* String format addresses (from .rdata)                               */
/* ================================================================== */
#define FMT_PATH     ((const char*)0x0047E8A0)   /* "%s\\%s" */
#define STR_SETTINGS ((const char*)0x0047EB74)   /* "NetSettings.dat" */

/* ================================================================== */
/* NETMAN_FreePacket — 0x440D00                                        */
/* MISNAMED: This is LoadSettings, not a free operation.              */
/*                                                                     */
/* Loads network settings from NetSettings.dat. ECX = config ptr.     */
/* Called by: GameConfig_ctor @ 0x440CAC                               */
/* ================================================================== */
void __fastcall NETMAN_FreePacket(int32_t packetPtr)
{
    char buffer[0x504];
    void* saved_provider_list;
    void* hFile;
    uint32_t bytesRead;

    /* Initialize buffer */
    buffer[0] = g_empty_string;
    {
        uint32_t* p = (uint32_t*)(buffer + 1);
        int32_t i;
        for (i = 0x140; i != 0; i--) {
            *p++ = 0;
        }
    }
    *(uint16_t*)(buffer + 0x501) = 0;

    /* Save provider list and reset */
    saved_provider_list = *(void**)(packetPtr + 0x10);
    *(void**)(packetPtr + 0x10) = NULL;
    *(uint8_t*)(packetPtr + 6) = 0;

    /* Build path: "<install>\\NetSettings.dat" */
    wsprintfA(buffer, FMT_PATH, g_install_path, STR_SETTINGS);

    hFile = CreateFileA(buffer, 0x80000000, 1, NULL, 3, 0x8000000, NULL);
    if (hFile == (void*)-1) {
        *(void**)(packetPtr + 0x10) = saved_provider_list;
        return;
    }

    bytesRead = 0;
    if (ReadFile(hFile, (void*)(packetPtr + 4), 0xAC, &bytesRead, NULL) != 0) {
        CloseHandle(hFile);

        if (*(uint16_t*)(packetPtr + 4) != 0x006A) {
            /* Initialize defaults */
            *(uint16_t*)(packetPtr + 4) = 0x006A;
            *(uint8_t*)(packetPtr + 0x18) = 0;
            *(int32_t*)(packetPtr + 0x1C) = 4;
            *(int32_t*)(packetPtr + 0x20) = 2;
            *(uint8_t*)(packetPtr + 0x24) = 0;
            *(int32_t*)(packetPtr + 0x28) = 4;
            *(uint8_t*)(packetPtr + 0x2C) = 0;
            *(uint8_t*)(packetPtr + 0x6C) = 0;
            *(int32_t*)(packetPtr + 0xAC) = 2;
            *(uint8_t*)(packetPtr + 7) = 1;
            *(uint8_t*)(packetPtr + 8) = 0;
            *(int32_t*)(packetPtr + 0x0C) = 0x1E;
            *(uint8_t*)(packetPtr + 6) = 0;

            /* Write defaults to disk */
            {
                uint32_t bytesWritten;
                void* hOutFile;
                char outPath[0x504];

                outPath[0] = g_empty_string;
                {
                    uint32_t* p = (uint32_t*)(outPath + 1);
                    int32_t i;
                    for (i = 0x140; i != 0; i--) *p++ = 0;
                }
                *(uint16_t*)(outPath + 0x501) = 0;

                wsprintfA(outPath, FMT_PATH, g_install_path, STR_SETTINGS);
                hOutFile = CreateFileA(outPath, 0x40000000, 1, NULL, 2, 0x8000000, NULL);
                if (hOutFile != (void*)-1) {
                    WriteFile(hOutFile, (void*)(packetPtr + 4), 0xAC, &bytesWritten, NULL);
                    CloseHandle(hOutFile);
                }
            }
        } else {
            *(uint8_t*)(packetPtr + 6) = 1;
        }
    } else {
        CloseHandle(hFile);
    }

    *(void**)(packetPtr + 0x10) = saved_provider_list;
}

/* ================================================================== */
/* NETMAN_SendPacket — 0x440EA0                                       */
/* MISNAMED: This is SaveSettings.                                    */
/*                                                                     */
/* Writes 0xAC bytes from config+4 to NetSettings.dat.                */
/* Called by: SetSessionInfo, DestroySession, EditWindow_OnPlayerName  */
/* ================================================================== */
void __fastcall NETMAN_SendPacket(int32_t packetPtr)
{
    char buffer[0x504];
    void* hFile;
    uint32_t bytesWritten;

    buffer[0] = g_empty_string;
    {
        uint32_t* p = (uint32_t*)(buffer + 1);
        int32_t i;
        for (i = 0x140; i != 0; i--) *p++ = 0;
    }
    *(uint16_t*)(buffer + 0x501) = 0;

    wsprintfA(buffer, FMT_PATH, g_install_path, STR_SETTINGS);

    hFile = CreateFileA(buffer, 0x40000000, 1, NULL, 2, 0x8000000, NULL);
    if (hFile == (void*)-1) return;

    WriteFile(hFile, (void*)(packetPtr + 4), 0xAC, &bytesWritten, NULL);
    CloseHandle(hFile);
}

/* ================================================================== */
/* NETMAN_EnumerateSessions — 0x441720                                */
/*                                                                     */
/* Creates session name EDIT child control on the join panel.         */
/* __fastcall: ECX = panel pointer                                     */
/* ================================================================== */
void __fastcall NETMAN_EnumerateSessions(int32_t panel)
{
    void* hEditWnd;

    if (*(int32_t*)(panel + 0x1D8) != 0) return;

    hEditWnd = CreateWindowExA(
        0x200, (const char*)0x0047E464, (const char*)0x004851D0,
        0x40000080,
        *(int32_t*)(panel + 0x15C), *(int32_t*)(panel + 0x160),
        *(int32_t*)(panel + 0x164) - *(int32_t*)(panel + 0x15C),
        *(int32_t*)(panel + 0x168) - *(int32_t*)(panel + 0x160),
        *(void**)(panel + 8), (void*)0x41F, *(void**)(panel + 4), NULL);
    *(int32_t*)(panel + 0x1D8) = (int32_t)hEditWnd;

    if (hEditWnd != NULL) {
        PostMessageA(hEditWnd, 0x30, *(void**)0x004855F8, 1);
        PostMessageA(hEditWnd, 0xC5, (void*)0x40, 0);
        SetWindowTextA(hEditWnd, (const char*)(*(int32_t*)0x004FD3A8 + 0x6C));
        *(int32_t*)(panel + 0x1DC) = (int32_t)SetWindowLongA(
            hEditWnd, -4, (void*)0x004417E0);
    }
}

/* ================================================================== */
/* NETMAN_JoinSession — 0x441870                                      */
/*                                                                     */
/* Initialize and show the join-session UI panel. Creates 7 sprites,  */
/* loads sound, starts timer, renders connection text.                */
/* __fastcall: ECX = panel pointer                                     */
/* ================================================================== */
void __fastcall NETMAN_JoinSession(int32_t panel)
{
    *(uint8_t*)(panel + 0x148) = 0;  /* clear dirty flag */

    /* Initialize sprites if not already done */
    if (*(uint8_t*)(panel + 0x1AC) == 0) {
        void* resMgr = ResourceManager_GetById(&g_resmgr, 0x439);
        *(int32_t*)(panel + 0x1CC) = (int32_t)resMgr;

        if (resMgr != NULL) {
            void** vtbl = *(void***)resMgr;
            *(int32_t*)(panel + 0x1D0) =
                (int32_t)((void*(__stdcall*)(int,int))vtbl[1])(0, 0);
        }

        Sprite_Init(*(void**)(panel + 0x1B0));
        Sprite_Init(*(void**)(panel + 0x1B4));
        Sprite_Init(*(void**)(panel + 0x1B8));
        Sprite_Init(*(void**)(panel + 0x1BC));
        Sprite_Init(*(void**)(panel + 0x1C0));
        Sprite_Init(*(void**)(panel + 0x1C4));
        Sprite_Init(*(void**)(panel + 0x1C8));
        *(uint8_t*)(panel + 0x1AC) = 1;
    }

    /* vtable[7] OnCreate */
    (*(void(**)(void))(*(int32_t*)panel + 0x1C))();

    /* Iterate provider list */
    {
        void* prov = *(void**)(*(int32_t*)0x004FD3A8 + 0x10);
        while (prov != NULL) {
            int32_t type = *(int32_t*)((int8_t*)prov + 4);
            if (type == 2)      *(uint8_t*)(panel + 0x1E1) = 1;
            else if (type == 4) *(uint8_t*)(panel + 0x1E0) = 1;
            prov = *(void**)prov;
        }
    }

    UI_WindowBase_Show((void*)panel);
    SetFocus((void*)*(int32_t*)(panel + 8));

    /* vtable[3] set_mode */
    (*(void(**)(int32_t,int32_t,int32_t,int32_t))(*(int32_t*)panel + 0x0C))(
        *(int32_t*)(panel + 0x60), *(int32_t*)(panel + 0x64), 0, 1);

    /* Load sound */
    {
        int32_t snd = ResourceManager_GetStringById(&g_resmgr, 0x5015);
        if (snd != 0) RESMGR_LoadSoundResource(snd);
    }

    *(int32_t*)(panel + 0xEC) = (int32_t)SetTimer(
        (void*)*(int32_t*)(panel + 8), 0x50, 0x32, NULL);

    *(int32_t*)(panel + 0x140) = 2;
    FormatResourceString(&g_resmgr, 0x79, (char*)(panel + 0xF0), 0x40);

    DPlayManager_RenderConnectionPanel((void*)panel);
}

/* ================================================================== */
/* NETMAN_CreateSession — 0x4419C0                                    */
/*                                                                     */
/* Set 2/4 player mode flags from provider list.                       */
/* __fastcall: ECX = panel pointer                                     */
/* ================================================================== */
void __fastcall NETMAN_CreateSession(int32_t panel)
{
    void* prov = *(void**)(*(int32_t*)0x004FD3A8 + 0x10);
    while (prov != NULL) {
        int32_t type = *(int32_t*)((int8_t*)prov + 4);
        if (type == 2)      *(uint8_t*)(panel + 0x1E1) = 1;
        else if (type == 4) *(uint8_t*)(panel + 0x1E0) = 1;
        prov = *(void**)prov;
    }
}

/* ================================================================== */
/* NETMAN_LeaveSession — 0x441A00                                     */
/*                                                                     */
/* Cleanup: kill timer, destroy sprites, hide window.                 */
/* __fastcall: ECX = panel pointer                                     */
/* ================================================================== */
void __fastcall NETMAN_LeaveSession(int32_t panel)
{
    KillTimer((void*)*(int32_t*)(panel + 8), (void*)*(int32_t*)(panel + 0xEC));

    if (*(uint8_t*)(panel + 0x1AC) != 0) {
        void* resMgr = *(void**)(panel + 0x1CC);
        if (resMgr != NULL) {
            void** vtbl = *(void***)resMgr;
            ((void(__stdcall*)(void))vtbl[2])();
        }

        Sprite_Destroy(*(int32_t*)(panel + 0x1B0));
        Sprite_Destroy(*(int32_t*)(panel + 0x1B4));
        Sprite_Destroy(*(int32_t*)(panel + 0x1B8));
        Sprite_Destroy(*(int32_t*)(panel + 0x1BC));
        Sprite_Destroy(*(int32_t*)(panel + 0x1C0));
        Sprite_Destroy(*(int32_t*)(panel + 0x1C4));
        Sprite_Destroy(*(int32_t*)(panel + 0x1C8));

        *(uint8_t*)(panel + 0x1AC) = 0;
    }

    UI_WindowBase_Hide((void*)panel);
}

/* ================================================================== */
/* NETMAN_UpdateSessionInfo — 0x441A90                                */
/*                                                                     */
/* Blit child surface, reset sprites, call GetSessionInfo, end paint. */
/* __fastcall: ECX = panel pointer                                     */
/* ================================================================== */
void __fastcall NETMAN_UpdateSessionInfo(void* panel)
{
    UIPANEL_Blit(
        *(void**)((int32_t)panel + 0x1D0),
        *(uint32_t*)((int32_t)panel + 0xD4),
        *(uint32_t*)((int32_t)panel + 0xD8),
        *(int32_t*)((int32_t)panel + 0xDC),
        *(uint32_t*)((int32_t)panel + 0xE0),
        g_primary_surface,
        *(uint32_t*)((int32_t)panel + 0x14C),
        *(uint32_t*)((int32_t)panel + 0x150),
        *(int32_t*)((int32_t)panel + 0x154),
        *(uint32_t*)((int32_t)panel + 0x158),
        1);

    Sprite_SetState(*(void**)((int32_t)panel + 0x1C8), 0, NULL);
    Sprite_SetState(*(void**)((int32_t)panel + 0x1B0), 0, NULL);
    Sprite_SetState(*(void**)((int32_t)panel + 0x1B4), 0, NULL);

    NETMAN_GetSessionInfo((int32_t)panel);

    UIPANEL_EndPaintEx(panel, *(void**)((int32_t)panel + 8), 0, 0, NULL);
    *(uint8_t*)((int32_t)panel + 0x148) = 1;
}

/* ================================================================== */
/* NETMAN_GetSessionInfo — 0x441B40                                   */
/*                                                                     */
/* Update button sprite visibility based on host/client mode.         */
/* __fastcall: ECX = panel pointer                                     */
/* ================================================================== */
void __fastcall NETMAN_GetSessionInfo(int32_t panel)
{
    int32_t config = *(int32_t*)0x004FD3A8;
    uint8_t isHost = *(uint8_t*)(config + 8);

    Sprite_SetState(*(void**)(panel + 0x1C8), 0, NULL);

    if (isHost == 0) {
        /* Client mode */
        if (*(uint8_t*)(panel + 0x1E0) != 0) {
            int32_t pc = *(int32_t*)(config + 0x28);
            Sprite_SetState(*(void**)(panel + 0x1B8), (pc == 4) ? 1 : 0, NULL);
            ShowWindow((void*)*(int32_t*)(panel + 0x1D8), 0);
        }
        if (*(uint8_t*)(panel + 0x1E1) == 0) return;

        if (*(int32_t*)(config + 0x28) == 2) {
            Sprite_SetState(*(void**)(panel + 0x1C0), 0, NULL);
            Sprite_SetState(*(void**)(panel + 0x1BC), 1, NULL);
            ShowWindow((void*)*(int32_t*)(panel + 0x1D8), 5);
            SetFocus((void*)*(int32_t*)(panel + 0x1D8));
            return;
        }
        ShowWindow((void*)*(int32_t*)(panel + 0x1D8), 0);
    } else {
        /* Host mode */
        ShowWindow((void*)*(int32_t*)(panel + 0x1D8), 0);

        if (*(uint8_t*)(panel + 0x1E0) != 0) {
            int32_t pc = *(int32_t*)(config + 0x1C);
            Sprite_SetState(*(void**)(panel + 0x1B8), (pc == 4) ? 1 : 0, NULL);
        }
        if (*(uint8_t*)(panel + 0x1E1) == 0) return;

        if (*(int32_t*)(config + 0x1C) == 2) {
            Sprite_SetState(*(void**)(panel + 0x1BC), 1, NULL);
            return;
        }
    }

    Sprite_SetState(*(void**)(panel + 0x1BC), 0, NULL);
}

/* ================================================================== */
/* NETMAN_SetSessionInfo — 0x441C80                                   */
/*                                                                     */
/* Handle mouse clicks on session panel sprites. Called from WndProc.  */
/* __thiscall: ECX = panel, stack: lParam (packed x,y) + 3 hidden     */
/* ================================================================== */
static uint32_t __thiscall NETMAN_SetSessionInfo_impl(void* panel, uint32_t lParam)
{
    int32_t mouseX = (int32_t)(lParam & 0xFFFF);
    int32_t mouseY = (int32_t)(lParam >> 16);

    if (*(uint8_t*)((int32_t)panel + 0x148) == 0) return 0;

    /* sprite[0]: Back/Cancel */
    {
        const RECT* r = (const RECT*)(*(int32_t*)((int32_t)panel + 0x1B0) + 4);
        if (PtInRect(r, lParam & 0xFFFF | ((lParam >> 16) << 16)) != 0) {
            Sprite_SetState(*(void**)((int32_t)panel + 0x1B0), 1, NULL);
            PlaySound(0x5015);
            UIPANEL_EndPaint(panel);
            Sleep(0x96);
            (*(void(**)(int,int,int,int,int))(*(int32_t*)panel + 0x10))(0,0,0,0,1);
            GetWindowTextA((void*)*(int32_t*)((int32_t)panel + 0x1D8),
                           (char*)(*(int32_t*)0x004FD3A8 + 0x6C), 0x40);
            if (*(uint8_t*)(*(int32_t*)0x004FD3A8 + 8) == 0)
                *(uint8_t*)(*(int32_t*)0x004FD3A8 + 0x24) = 1;
            else
                *(uint8_t*)(*(int32_t*)0x004FD3A8 + 0x18) = 1;
            NETMAN_SendPacket(*(int32_t*)0x004FD3A8);
            UI_MainMenu_SetState(g_ui_main, 3);
            return 0;
        }
    }

    /* sprite[1]: Cancel/Exit */
    {
        const RECT* r = (const RECT*)(*(int32_t*)((int32_t)panel + 0x1B4) + 4);
        if (PtInRect(r, lParam & 0xFFFF | ((lParam >> 16) << 16)) != 0) {
            Sprite_SetState(*(void**)((int32_t)panel + 0x1B4), 1, NULL);
            PlaySound(0x5015);
            UIPANEL_EndPaint(panel);
            Sleep(0x96);
            (*(void(**)(int,int,int,int,int))(*(int32_t*)panel + 0x10))(0,0,0,0,1);
            UI_MainMenu_SetState(g_ui_main, 7);
            return 0;
        }
    }

    /* sprite[2]: 4-Player mode */
    {
        const RECT* r = (const RECT*)(*(int32_t*)((int32_t)panel + 0x1B8) + 4);
        if (PtInRect(r, lParam & 0xFFFF | ((lParam >> 16) << 16)) != 0) {
            if (*(uint8_t*)((int32_t)panel + 0x1E0) != 0) {
                int32_t cfg = *(int32_t*)0x004FD3A8;
                if (*(uint8_t*)(cfg + 8) == 0)
                    *(int32_t*)(cfg + 0x28) = 4;
                else
                    *(int32_t*)(cfg + 0x1C) = 4;
                NETMAN_GetSessionInfo((int32_t)panel);
                PlaySound(0x5015);
                UIPANEL_EndPaintEx(panel, *(void**)((int32_t)panel + 8), 0, 0, NULL);
            }
            return 0;
        }
    }

    /* sprite[3]: 2-Player mode */
    {
        const RECT* r = (const RECT*)(*(int32_t*)((int32_t)panel + 0x1BC) + 4);
        if (PtInRect(r, lParam & 0xFFFF | ((lParam >> 16) << 16)) != 0) {
            if (*(uint8_t*)((int32_t)panel + 0x1E1) != 0) {
                int32_t cfg = *(int32_t*)0x004FD3A8;
                if (*(uint8_t*)(cfg + 8) == 0)
                    *(int32_t*)(cfg + 0x28) = 2;
                else
                    *(int32_t*)(cfg + 0x1C) = 2;
                NETMAN_GetSessionInfo((int32_t)panel);
                PlaySound(0x5015);
                UIPANEL_EndPaintEx(panel, *(void**)((int32_t)panel + 8), 0, 0, NULL);
            }
            return 0;
        }
    }

    /* Random sound area at panel+0x19C */
    {
        int32_t pt = lParam & 0xFFFF | ((lParam >> 16) << 16);
        if (PtInRect((const RECT*)((int32_t)panel + 0x19C), pt) != 0) {
            int32_t rnd = CRT_rand();
            PlaySoundAt((rnd / 0x1FFF) + 0x500F, mouseX, mouseY, 4);
        }
    }

    return 0;
}

/* ================================================================== */
/* NETMAN_SetSessionInfo — 0x441C80                                   */
/*                                                                     */
/* Thunk that reads lParam from the WindowProc stack and dispatches.   */
/* __thiscall: ECX = panel, 4 stack args from WindowProc (16 bytes)   */
/* ================================================================== */
void __thiscall NETMAN_SetSessionInfo(void* panel)
{
    /* NOTE: This function is called via vtable dispatch with
       WindowProc args on the stack: (HWND, UINT, WPARAM, LPARAM).
       The 4th arg (lParam at [ESP+0x14]) contains packed x,y.
       Since MSVC __thiscall pops only 4 bytes per declared arg,
       and this is declared as __thiscall(void*) with no stack args,
       the actual stack cleanup (RET 0x10) must be handled by the caller.

       The implementation reads lParam directly from the known stack offset.
       In the original MSVC code, this offset is:
         [ESP + 0x14] after prologue push of EBX (saved reg).

       For simplicity, the internal _impl function receives lParam.
       At the call site, the stack args are stripped by RET 0x10. */

    /* Read lParam at [ESP+0x14] — after PUSH EBX, ESP is -4 from entry,
       and entry had 4 WindowProc args above return address. */
    uint32_t lParam;
    __asm mov eax, [esp + 0x14]
    __asm mov lParam, eax

    NETMAN_SetSessionInfo_impl(panel, lParam);
}

/* ================================================================== */
/* NETMAN_DestroySession — 0x441F80                                   */
/*                                                                     */
/* Session panel WindowProc: handles ENTER (confirm) and ESC (cancel). */
/* __thiscall: ECX = panel, args: hWnd, msg, wParam, lParam           */
/* ================================================================== */
void* __thiscall NETMAN_DestroySession(void* panel, void* hWnd, uint32_t msg,
                                        uint32_t wParam, uint32_t lParam)
{
    if (*(uint8_t*)((int32_t)panel + 0x148) == 0) return (void*)0;

    if ((int32_t)wParam == 0x0D) {
        /* ENTER — Confirm */
        Sprite_SetState(*(void**)((int32_t)panel + 0x1B0), 1, NULL);
        UIPANEL_EndPaintEx(panel, *(void**)((int32_t)panel + 8), 0, 0, NULL);
        Sleep(0x96);
        (*(void(**)(int,int,int,int,int))(*(int32_t*)panel + 0x10))(0,0,0,0,1);
        GetWindowTextA((void*)*(int32_t*)((int32_t)panel + 0x1D8),
                       (char*)(*(int32_t*)0x004FD3A8 + 0x6C), 0x40);
        if (*(uint8_t*)(*(int32_t*)0x004FD3A8 + 8) == 0)
            *(uint8_t*)(*(int32_t*)0x004FD3A8 + 0x24) = 1;
        else
            *(uint8_t*)(*(int32_t*)0x004FD3A8 + 0x18) = 1;
        NETMAN_SendPacket(*(int32_t*)0x004FD3A8);
        UI_MainMenu_SetState(g_ui_main, 3);
        return (void*)0;
    }

    if ((int32_t)wParam == 0x1B) {
        /* ESC — Cancel */
        Sprite_SetState(*(void**)((int32_t)panel + 0x1B4), 1, NULL);
        UIPANEL_EndPaintEx(panel, *(void**)((int32_t)panel + 8), 0, 0, NULL);
        Sleep(0x96);
        (*(void(**)(int,int,int,int,int))(*(int32_t*)panel + 0x10))(0,0,0,0,1);
        UI_MainMenu_SetState(g_ui_main, 7);
        return (void*)0;
    }

    return DefWindowProcA(hWnd, msg, (void*)wParam, (void*)lParam);
}
