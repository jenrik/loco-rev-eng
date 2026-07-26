/**
 * winmain.c — WinMain application entry point
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * WinMain is the application entry point called by _mainCRTStartup.
 * Flow:
 *   1. Create splash dialog, display centered on screen
 *   2. Allocate CGWND object (0x28 bytes), store in g_main_window
 *   3. Initialize COM (CoInitializeEx)
 *   4. Install path initialization & resource manager data init
 *   5. Parse command line
 *   6. ResourceManager_Regenerate (check assets)
 *   7. Show main menu, init game
 *   8. Prevent multiple instances (FindWindowA for "LEGO LOCO")
 *   9. GameLoop_Setup -> main message pump (two phases: lobby+game)
 *  10. Cleanup: destroy CGWND, CoUninitialize, return exit code
 *
 * Address: 0x462E90
 * Size: 1440 bytes
 * Calling convention: __stdcall (ret 0x10)
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Win32 API via IAT */
extern HWND  __stdcall CreateDialogParamA(HINSTANCE hInstance, LPCSTR templateName,
                                           HWND hWndParent, DLGPROC lpDialogFunc,
                                           LPARAM dwInitParam);         /* 0x47728C */
extern int   __stdcall LoadStringA(HINSTANCE hInstance, UINT uID,
                                    LPSTR lpBuffer, int cchBufferMax); /* 0x47738C */
extern int   __stdcall GetSystemMetrics(int nIndex);                    /* 0x477338 */
extern BOOL  __stdcall SetWindowPos(HWND hWnd, HWND hWndInsertAfter,
                                     int X, int Y, int cx, int cy,
                                     UINT uFlags);                     /* 0x477344 */
extern BOOL  __stdcall UpdateWindow(HWND hWnd);                        /* 0x47730C */
extern BOOL  __stdcall ShowWindow(HWND hWnd, int nCmdShow);            /* 0x47735C */
extern BOOL  __stdcall SetWindowTextA(HWND hWnd, LPCSTR lpString);     /* 0x477394 */
extern int   __stdcall MessageBoxA(HWND hWnd, LPCSTR lpText,
                                    LPCSTR lpCaption, UINT uType);     /* 0x477334 */
extern HRESULT __stdcall CoInitializeEx(LPVOID pvReserved,
                                         DWORD dwCoInit);              /* 0x4773DC */
extern void  __stdcall CoUninitialize(void);                           /* 0x4773D8 */
extern HWND  __stdcall FindWindowA(LPCSTR lpClassName,
                                    LPCSTR lpWindowName);              /* 0x477288 */
extern BOOL  __stdcall SetForegroundWindow(HWND hWnd);                  /* 0x4772BC */
extern BOOL  __stdcall GetMessageA(LPMSG lpMsg, HWND hWnd,
                                    UINT wMsgFilterMin,
                                    UINT wMsgFilterMax);               /* 0x4772A0 */
extern BOOL  __stdcall TranslateMessage(const MSG* lpMsg);             /* 0x4772F8 */
extern LRESULT __stdcall DispatchMessageA(const MSG* lpMsg);           /* 0x477294 */
extern BOOL  __stdcall PeekMessageA(LPMSG lpMsg, HWND hWnd,
                                     UINT wMsgFilterMin, UINT wMsgFilterMax,
                                     UINT wRemoveMsg);                 /* 0x4772F4 */
extern DWORD __stdcall MsgWaitForMultipleObjects(DWORD nCount,
                                                  const HANDLE* pHandles,
                                                  BOOL fWaitAll,
                                                  DWORD dwMilliseconds,
                                                  DWORD dwWakeMask);    /* 0x477284 */
extern BOOL  __stdcall PostMessageA(HWND hWnd, UINT Msg,
                                     WPARAM wParam, LPARAM lParam);   /* 0x477340 */
extern BOOL  __stdcall DestroyWindow(HWND hWnd);                       /* 0x4772A8 */
extern HANDLE __stdcall ResetEvent(HANDLE hEvent);                     /* 0x47714C */

/* Game functions */
extern void* __cdecl operator_new(size_t size);             /* 0x465CE0 */
extern void* __thiscall CGWND_constructor(void* self, HINSTANCE hInstance);  /* 0x4061E0 */
extern char  __thiscall CGWND_InstallPathInit(void);                         /* 0x4068D0 */
extern void __thiscall CGWND_ParseCmdLine(byte* cmdLine);                    /* 0x406790 */
extern void __thiscall CGWND_ShowMainMenu(int self);                         /* 0x406480 */
extern char __thiscall CGWND_InitGame(int self);                             /* 0x406680 */
extern void __thiscall CGWND_Cleanup(void);                                  /* 0x4077A0 */
extern void __thiscall CGWND_Present(int param);                             /* 0x45E1E0 */
extern void __thiscall CGWND_SetMode(void* mode);                            /* 0x408130 */
extern void __thiscall ResourceManager_InitData(void* resmgr);               /* 0x4463C0 */
extern char __thiscall ResourceManager_Regenerate(void* resmgr);             /* 0x4480C0 */
extern void __cdecl FormatResourceString(void* resmgr, UINT id,
                                          char* buf, int maxLen);           /* 0x447330 */
extern int  __thiscall GameLoop_Setup(int cgwnd);                            /* 0x406BA0 */
extern void __thiscall GameLoop_FrameUpdate(void);                           /* 0x45C3C0 */
extern void __cdecl NETMAN_Update(void* netman);                             /* 0x43F0C0 */
extern void __cdecl MultiplayerLobby_Reload(void);                           /* 0x448350 */
extern int  __cdecl CRT_timeGetTime(int* param);                             /* 0x466AF0 */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern HINSTANCE g_hInstance;           /* 0x4A991C */
extern void*     g_main_window;         /* 0x4AA4A0 — CGWND pointer */
extern int       g_demo_mode;           /* 0x4A9918 — demo mode flag (0=retail) */
extern int       g_game_mode;           /* 0x4FD2E0 — current game mode */
extern int       g_player_id;           /* global player ID */
extern int       g_player_color;        /* global player color */
extern void*     g_netman;              /* 0x4FD3AC — NetMan manager */
extern void*     g_resmgr;              /* 0x4855E8 — resource manager */
extern char      g_install_path[];      /* 0x4A99C8 — install directory path */
extern double    g_fps_counter;         /* 0x481170 — frames-per-second counter */

/* Internal globals used in the message loop */
extern int  DAT_00485444;               /* 0x485444 — frame update enabled flag */
extern int  DAT_00481914;               /* 0x481914 — FPS calculation rate limit counter */
extern char DAT_004aa4a4;               /* 0x4AA4A4 — present flag */
extern void* DAT_004a990c;              /* 0x4A990C — reset event handle */
extern int  DAT_004ff12c;               /* 0x4FF12C — frame throttle counter */
extern int  DAT_004ff118;               /* 0x4FF118 — frame throttle counter 2 */
extern int  DAT_004ff128;               /* 0x4FF128 — frame throttle counter 3 */

/* ================================================================== */
/* WinMain — Application entry point                                    */
/* Address: 0x462E90                                                   */
/* ================================================================== */
WPARAM __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                          LPSTR lpCmdLine, int nCmdShow)
{
    HWND hSplashWnd;
    int frameCounter;            /* local_29c — frames since last FPS calculation */
    int prevFrameCounter;        /* local_294 — message dispatch throttle counter */
    int dummyCounter;            /* local_290 */
    MSG msg;
    char titleBuf[100];          /* local_70 — window title */
    int initResult;
    void* sehNode;
    char errorBuf[0x200];        /* local_270, local_26f (overlapping) */

    sehNode = NULL;
    initResult = 0;
    dummyCounter = 0;
    frameCounter = 0;

    /* Step 1: Create splash dialog (resource ID 0x71 = 113) */
    hSplashWnd = CreateDialogParamA(hInstance, (LPCSTR)0x71, NULL, NULL, 0);
    if (hSplashWnd != NULL) {
        LoadStringA(hInstance, 1, titleBuf, 100);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);    /* 0x11 */
        int screenWidth  = GetSystemMetrics(SM_CXSCREEN);    /* 0x10 */
        int x = (screenWidth  - 0x2A3) / 2;   /* center: 675-> x */
        int y = (screenHeight - 0x1C2) / 2;   /* center: 450 -> y */
        SetWindowPos(hSplashWnd, HWND_TOP, x, y, 0x2A3, 0x1C2, 0);
        UpdateWindow(hSplashWnd);
        ShowWindow(hSplashWnd, SW_SHOWNORMAL);
        SetWindowTextA(hSplashWnd, titleBuf);
    }

    /* Step 2: Store hInstance globally and allocate CGWND */
    g_hInstance = hInstance;

    void* cgwnd = operator_new(0x28);  /* CGWND is 0x28 bytes */
    if (cgwnd == NULL) {
        g_main_window = NULL;
    } else {
        g_main_window = CGWND_constructor(cgwnd, hInstance);
    }

    /* Step 3: If CGWND allocation failed, show error and exit */
    if (g_main_window == NULL) {
        memset(errorBuf, 0, sizeof(errorBuf));
        errorBuf[0] = '\0';
        FormatResourceString(g_resmgr, 0x14a, errorBuf, 0x200);
        MessageBoxA(NULL, errorBuf, "LEGO LOCO", MB_ICONEXCLAMATION | MB_OK);
        msg.wParam = (WPARAM)-2;  /* 0xFFFFFFFE */
        goto cleanup;
    }

    /* Step 4: Initialize COM, install path, and resource manager */
    CoInitializeEx(NULL, 0);  /* COINIT_APARTMENTTHREADED */

    initResult = CGWND_InstallPathInit();
    ResourceManager_InitData(g_resmgr);

    if (initResult == 0) {
        /* Install path init failed */
        if (g_main_window != NULL) {
            void** vtable = *(void***)g_main_window;
            typedef void (__thiscall* DtorFn)(void* self, byte flags);
            ((DtorFn)vtable[0])(g_main_window, 1);
        }
        g_main_window = NULL;
        CoUninitialize();
        memset(errorBuf, 0, sizeof(errorBuf));
        errorBuf[0] = '\0';
        FormatResourceString(g_resmgr, 0x14a, errorBuf, 0x200);
        MessageBoxA(NULL, errorBuf, "LEGO LOCO", MB_ICONEXCLAMATION | MB_OK);
        msg.wParam = (WPARAM)-1;  /* 0xFFFFFFFF */
        goto cleanup;
    }

    /* Step 5: Parse command line and regenerate resource manager */
    CGWND_ParseCmdLine((byte*)lpCmdLine);

    if (!ResourceManager_Regenerate(g_resmgr)) {
        if (g_main_window != NULL) {
            void** vtable = *(void***)g_main_window;
            typedef void (__thiscall* DtorFn)(void* self, byte flags);
            ((DtorFn)vtable[0])(g_main_window, 1);
        }
        g_main_window = NULL;
        CoUninitialize();
        msg.wParam = 0;  /* Return 0 */
        goto cleanup;
    }

    /* Step 6: Show main menu, init game */
    CGWND_ShowMainMenu((int)g_main_window);

    if (!CGWND_InitGame((int)g_main_window)) {
        if (g_main_window != NULL) {
            void** vtable = *(void***)g_main_window;
            typedef void (__thiscall* DtorFn)(void* self, byte flags);
            ((DtorFn)vtable[0])(g_main_window, 1);
        }
        g_main_window = NULL;
        CoUninitialize();
        msg.wParam = (WPARAM)-3;  /* 0xFFFFFFFD */
        goto cleanup;
    }

    /* Step 7: Prevent multiple instances (retail mode only) */
    if (g_demo_mode == 0) {
        HWND hExisting = FindWindowA("LEGO LOCO", "LEGO LOCO");
        if (hExisting != NULL) {
            SetForegroundWindow(hExisting);
            if (g_main_window != NULL) {
                void** vtable = *(void***)g_main_window;
                typedef void (__thiscall* DtorFn)(void* self, byte flags);
                ((DtorFn)vtable[0])(g_main_window, 1);
            }
            g_main_window = NULL;
            CoUninitialize();
            msg.wParam = 1;  /* Exit code 1: already running */
            goto cleanup;
        }
    }

    /* Step 8: Game loop setup */
    if (hSplashWnd != NULL) {
        UpdateWindow(hSplashWnd);
    }

    int loopResult = GameLoop_Setup((int)g_main_window);
    if (loopResult != 0) {
        /* Game loop setup failed — show error, cleanup */
        memset(errorBuf, 0, sizeof(errorBuf));
        errorBuf[0] = '\0';
        FormatResourceString(g_resmgr, 0x14a, errorBuf, 0x200);
        MessageBoxA(NULL, errorBuf, "LEGO LOCO", MB_ICONEXCLAMATION | MB_OK);
        CGWND_Cleanup();
        if (g_main_window != NULL) {
            void** vtable = *(void***)g_main_window;
            typedef void (__thiscall* DtorFn)(void* self, byte flags);
            ((DtorFn)vtable[0])(g_main_window, 1);
        }
        g_main_window = NULL;
        CoUninitialize();
        msg.wParam = (WPARAM)loopResult;
        goto cleanup;
    }

    /* Step 9: Set mode based on demo/retail */
    if (hSplashWnd != NULL) {
        UpdateWindow(hSplashWnd);
    }

    if (g_demo_mode == 0) {
        CGWND_SetMode((void*)2);  /* Mode 2 = lobby/game mode */
    } else {
        MultiplayerLobby_Reload();
        CGWND_SetMode((void*)1);  /* Mode 1 = multiplayer lobby */
    }

    /* Show the main window and destroy the splash screen */
    {
        void** cgwnd_vtable = *(void***)g_main_window;
        int hWnd = *(int*)((char*)g_main_window + 8);  /* CGWND hWnd at +0x08 */
        ShowWindow((HWND)hWnd, SW_SHOWNORMAL);
    }

    msg.message = 0;
    PostMessageA(hSplashWnd, WM_CLOSE, 0, 0);
    DestroyWindow(hSplashWnd);

    /* Step 10: Main message pump – Phase 1: Lobby (game_mode == 2) */
    while (msg.message != WM_QUIT && g_game_mode == 2) {
        if (GetMessageA(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (g_netman != NULL) {
            NETMAN_Update(g_netman);
        }
    }

    /* Step 11: Main message pump – Phase 2: Game mode */
    while (msg.message != WM_QUIT) {
        /* Reset throttle counters */
        DAT_004ff12c = 0;
        DAT_004ff118 = 0;
        DAT_004ff128 = 0;

        if (prevFrameCounter < 1) {
            /* Batch: process up to 14 pending messages */
            prevFrameCounter = 14;
            while (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE) > 0) {
                GetMessageA(&msg, NULL, 0, 0);
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
                if (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE) <= 0) break;
            }
        } else {
            /* Single: process one pending message */
            prevFrameCounter--;
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
                if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE) <= 0) break;
            }
        }

        /* Present frame if flag is set */
        if (DAT_004aa4a4 != 0) {
            CGWND_Present(0);
        }

        /* Wait for new messages or events (3ms timeout) */
        MsgWaitForMultipleObjects(0, NULL, FALSE, 3, QS_ALLEVENTS);

        /* Process game frame update */
        if (DAT_00485444 != 0) {
            frameCounter++;
            GameLoop_FrameUpdate();
            ResetEvent(DAT_004a990c);
        }

        /* FPS calculation (every DAT_00481914 frames) */
        if (frameCounter >= DAT_00481914) {
            if (DAT_00481914 < 100) {
                DAT_00481914++;
            }
            int currentTime = CRT_timeGetTime(NULL);
            if (dummyCounter != currentTime && g_game_mode == 3) {
                /* Calculate FPS: frames / elapsed_seconds */
                g_fps_counter = (double)frameCounter /
                                ((double)currentTime - (double)dummyCounter);
            }
            frameCounter = 0;
            dummyCounter = currentTime;
        }
    }

    /* Step 12: Cleanup */
    if (g_main_window != NULL) {
        void** vtable = *(void***)g_main_window;
        typedef void (__thiscall* DtorFn)(void* self, byte flags);
        ((DtorFn)vtable[0])(g_main_window, 1);
    }
    g_main_window = NULL;
    CoUninitialize();

cleanup:
    return msg.wParam;
}
