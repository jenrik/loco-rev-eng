/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: Core / Entry / Game Loop
 * Original: loco.exe (Windows 95/98, DirectX 5 era)
 * Developer: Intelligent Games for LEGO Media
 *
 * This file was produced by reverse engineering the original binary.
 * Windows API calls are marked with WIN32: comments.
 * Linux/SDL2 replacement suggestions are marked with LINUX: comments.
 */

/*
 * loco_core.c — Lego Loco (1998) Core / Entry / Game Loop subsystem
 * Reverse-engineered from loco.exe.  Addresses refer to the original PE image.
 *
 * Subsystem overview
 * ------------------
 * The engine follows a classic early-2000s Win32 game shell pattern:
 *
 *   CRT_Startup (0x4689e0)
 *     └─► WinMain (0x462e90)
 *           ├─► splash dialog (resource 0x71)
 *           ├─► CGWND_Constructor (0x4061e0)   — creates the engine root object
 *           ├─► CoInitializeEx                  — COM for DirectPlay
 *           ├─► CGWND_LoadConfig (0x4068d0)    — registry → lego.ini path
 *           ├─► CGWND_ParseCommandLine (0x406790)
 *           ├─► CGWND_CheckDisplayCaps (0x406680)
 *           ├─► GameLoop_Setup (0x406ba0)       — creates all subsystems + timer
 *           └─► message loop
 *                 ├─ loading phase  (state==2): GetMessageA drain
 *                 └─ running phase  (state==3): PeekMessage + MsgWaitForMultipleObjects
 *                       └─► GameFrame_Update (0x45c3c0)  triggered by 28ms multimedia timer
 *
 * The global DAT_004851f4 is the engine state machine:
 *   1 = INIT / reset
 *   2 = LOADING (async asset load in progress)
 *   3 = RUNNING (normal gameplay)
 *   4 = PAUSED
 *   5 = MAIN_MENU variant A
 *   6 = MAIN_MENU variant B
 *   7 = MOVIE playback
 *   8 = SAVE screen
 *   9 = CREDITS
 *  10 = QUIT transition
 *
 * WIN32:  All window management is done through Win32 GDI/User APIs.
 * LINUX:  Replace with SDL2; see inline LINUX comments for specifics.
 */

#include "core.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* WIN32 */
#include <windows.h>
#include <mmsystem.h>   /* timeSetEvent, timeKillEvent */
#include <ole2.h>       /* CoInitializeEx, CoUninitialize */

/* =========================================================================
 * Global engine singleton pointers and state variables
 * =========================================================================
 *
 * These are the canonical definitions; extern declarations live in core.h.
 */

/* Engine root object (DAT_004aa4a0) */
CGWND        *g_pGameWnd       = NULL;

/* Engine state machine (DAT_004851f4) */
int           g_gameState      = 0;

/* Timer-fired flag (DAT_00485444) set by multimedia timer callback */
BYTE          g_timerFired     = 0;

/* Manual-reset "GameLoop" event handle (DAT_004a990c) */
HANDLE        g_hGameLoopEvent = NULL;

/* Multimedia timer ID (DAT_00485438) */
UINT          g_timerID        = 0;

/* Season / theme override (DAT_00485230) */
int           g_seasonOverride = 0;

/* Debug / help mode flag (DAT_004a9918) */
int           g_debugMode      = 0;

/* Screen dimensions (DAT_004851d8, DAT_00485214) */
int           g_screenWidth    = 0;
int           g_screenHeight   = 0;

/* Loaded CIniFile object (DAT_004a9eec) */
void         *g_pIniFile       = NULL;

/* Subsystem singleton pointers */
void         *g_pDirectDraw    = NULL;  /* DAT_004fd378 */
void         *g_pDirectSound   = NULL;  /* DAT_004fd37c */
void         *g_pInputMgr      = NULL;  /* DAT_004fd384 */
void         *g_pMovieMgr      = NULL;  /* DAT_004fd380 */
void         *g_pNetworkMgr    = NULL;  /* DAT_004fd388 */
void         *g_pSceneMgr      = NULL;  /* DAT_004fd38c */
void         *g_pWorldMgr      = NULL;  /* DAT_004fd390 */
void         *g_pAnimMgr       = NULL;  /* DAT_00485258 */
void         *g_pTimerSvc      = NULL;  /* DAT_004fd394 */
void         *g_pConfigMgr     = NULL;  /* DAT_004fd3a8 */
void         *g_pEventQueue    = NULL;  /* DAT_004fd3ac */
void         *g_pStringTable   = NULL;  /* DAT_004fd3b0 */
void         *g_pDebugLog      = NULL;  /* DAT_004fd3b4 */
void         *g_pSaveGameMgr   = NULL;  /* DAT_004aa4a8 */
void         *g_pThumbnailMgr  = NULL;  /* DAT_004ff110 */

/* =========================================================================
 * Internal helpers — operator new / delete wrappers
 * =========================================================================
 *
 * FUN_00465ce0 — operator new with SEH, wraps HeapAlloc.
 * FUN_00465cd0 — operator delete wrapper.
 * LINUX: Replace with plain malloc / free.
 */

/* Forward declarations of reconstructed internal helpers */
static void *loco_malloc(size_t n);
static void  loco_free(void *p);

static void *loco_malloc(size_t n)
{
    /* WIN32: FUN_00465ce0 — operator new with structured exception handling */
    /* LINUX: plain malloc(n) */
    return malloc(n);
}

static void loco_free(void *p)
{
    /* WIN32: FUN_00465cd0 — operator delete wrapper */
    /* LINUX: plain free(p) */
    free(p);
}

/* =========================================================================
 * Forward declarations of subsystem factory / init functions
 * (addresses from original binary; bodies live in other subsystem files)
 * =========================================================================
 */

/* Multimedia timer callback (0x45c520) — sets g_timerFired = 1 */
extern void CALLBACK TimerCallback_0x45c520(UINT uTimerID, UINT uMsg,
                                             DWORD_PTR dwUser,
                                             DWORD_PTR dw1, DWORD_PTR dw2);

/* Subsystem constructors */
extern void *FUN_004202f0(void *buf, HINSTANCE hInst, UINT resId); /* CDirectDrawManager   */
extern void *FUN_0042e900(void *buf, HINSTANCE hInst, UINT resId); /* CDirectSoundManager  */
extern void *FUN_00430a90(void *buf, HINSTANCE hInst, UINT resId); /* CNetworkManager      */
extern void *FUN_00436b20(void *buf, HINSTANCE hInst, UINT resId); /* CAnimManager         */
extern void *FUN_00401f50(void *buf, HINSTANCE hInst, UINT resId); /* CInputManager        */
extern void *FUN_00415980(void *buf, HINSTANCE hInst, UINT resId); /* CMoviePlayer         */
extern void *FUN_0044f490(void *buf, HINSTANCE hInst, UINT resId); /* CSceneManager        */
extern void *FUN_0040f1c0(void *buf, HINSTANCE hInst, UINT resId); /* CWorldManager        */
extern void *FUN_004493a0(void *buf);                               /* CTimerService        */
extern void *FUN_00440c60(void *buf);                               /* CConfigManager       */
extern void *FUN_0043d0a0(void *buf);                               /* CEventQueue          */
extern void *FUN_00443000(void *buf);                               /* CStringTable         */
extern void *FUN_00452e10(void *buf);                               /* CSaveGameManager     */
extern void *FUN_00401620(void *buf);                               /* CDebugLog            */

/* Subsystem init functions (take HWND of game window) */
extern BOOL  FUN_004204d0(void *pObj, HWND hwnd); /* CDirectDrawManager::Init  */
extern BOOL  FUN_0042edb0(void *pObj, HWND hwnd); /* CDirectSoundManager::Init */
extern BOOL  FUN_00402520(void *pObj, HWND hwnd); /* CNetworkManager::Init     */
extern BOOL  FUN_00436c50(void *pObj, HWND hwnd); /* CAnimManager::Init        */
extern char  FUN_004169e0(void *pObj, HWND hwnd); /* CMoviePlayer::Init        */
extern BOOL  FUN_00450ca0(void *pObj, HWND hwnd); /* CSceneManager::Init       */
extern BOOL  FUN_0040f510(void *pObj, HWND hwnd); /* CWorldManager::Init       */

/* Misc helpers */
extern BOOL  FUN_00446050(int unused);             /* Load string table resource */
extern void *FUN_0042a110(void *buf);              /* CThumbnailMgr constructor  */
extern void  FUN_00429ef0(void *mapData, void *resDir); /* Load map-select data */

/* State handlers */
extern void  FUN_00408350(void);                   /* State 1: reset/clear world */
extern void  FUN_004086f0(int prevState);          /* State 3: resume gameplay   */

/* Screen-rect globals declared in binary (accessed via SetRect in constructor) */
extern int   DAT_004851e0;
extern int   DAT_00485200;
extern int   DAT_00485220;

/* Main window procedure (address only; body reconstructed separately) */
extern LRESULT CALLBACK DAT_004618c0;

/* Child-window subsystem WndProc */
extern LRESULT CALLBACK LAB_00415900;

/* Vtable base for CGWND */
extern void *PTR_FUN_004774c4;

/* Display and misc function wrappers */
extern BOOL  FUN_004480c0(int unused);             /* SetupDisplay + CheckDisplayCaps */


/* =========================================================================
 * CRT_Startup — 0x004689e0
 * =========================================================================
 *
 * MSVC CRT entry point generated for GUI (/SUBSYSTEM:WINDOWS) executables.
 * Equivalent to the CRT's WinMainCRTStartup().
 *
 * What it does
 * ------------
 *  1. Reads Windows version via GetVersion() and stashes major/minor/build
 *     in CRT-private globals (_osver, _winmajor, _winminor).
 *  2. Initialises CRT heap (FUN_0046c4e0) and locale (FUN_0046a7f0);
 *     calls FUN_00468bc0(fatal_code) on failure.
 *  3. Calls FUN_00469330 (atexit table init) and FUN_0046ecd0 (global
 *     C++ constructors).
 *  4. Retrieves the process command line with GetCommandLineA().
 *  5. Skips the executable-name token: if the string starts with '"' it
 *     scans for the closing '"'; otherwise it advances past non-space chars.
 *     Then skips leading spaces to reach the first real argument.
 *  6. Calls GetStartupInfoA to read the inherited STARTUPINFO (dwFlags is
 *     checked to decide the initial nCmdShow value, though here nCmdShow is
 *     always passed as 0).
 *  7. Calls WinMain(hInstance=GetModuleHandleA(NULL), hPrevInstance=0,
 *                   lpCmdLine=<pointer into cmd string after exe name>).
 *  8. Passes the WinMain return value to FUN_004684d0 which calls
 *     ExitProcess(exitCode).
 *
 * WIN32: GetVersion, GetCommandLineA, GetStartupInfoA, GetModuleHandleA,
 *        ExitProcess — all Win32.
 * LINUX: Replace with standard main(int argc, char **argv).  Use argv[0]
 *        for the exe name and pass &argv[1] as the command tail.
 *        SDL_main handles the SDL_main/main renaming on all platforms.
 */
void entry(void)
{
    DWORD         dwVersion;
    HMODULE       hModule;
    BYTE         *pbCmdLine;
    STARTUPINFOA  startupInfo;
    WPARAM        exitCode;

    /* WIN32: Read OS version into CRT globals (_osver etc.) */
    dwVersion = GetVersion();
    (void)dwVersion;
    /* _DAT_004ff1b8 = minor, _DAT_004ff1b4 = major, etc. */

    /* Initialise CRT heap; abort with code 0x1c on failure */
    /* FUN_0046c4e0() ... */
    /* Initialise locale/stdio; abort with code 0x10 on failure */
    /* FUN_0046a7f0() ... */

    /* FUN_00469330() — register atexit handlers */
    /* FUN_0046ecd0() — run global C++ constructors (_initterm) */

    /* WIN32: Obtain raw command line (includes exe name).
     * LINUX:  Use argv[0..] from main() directly. */
    pbCmdLine = (BYTE *)GetCommandLineA();

    /* Skip executable name token */
    if (*pbCmdLine == '"') {
        /* Quoted path — scan to closing '"' */
        do { pbCmdLine++; } while (*pbCmdLine != '"' && *pbCmdLine != '\0');
        if (*pbCmdLine == '"') pbCmdLine++;
    } else {
        /* Unquoted — advance past non-space chars */
        while (*pbCmdLine > 0x20) pbCmdLine++;
    }
    /* Skip whitespace between exe name and first argument */
    while (*pbCmdLine != '\0' && *pbCmdLine <= 0x20) pbCmdLine++;

    /* WIN32: GetStartupInfoA — reads inherited console/window show state.
     * LINUX:  Not needed; nCmdShow is always SW_SHOWNORMAL for SDL. */
    startupInfo.dwFlags = 0;
    GetStartupInfoA(&startupInfo);

    hModule = GetModuleHandleA(NULL);

    /* Call WinMain — the real game entry point */
    exitCode = WinMain(hModule, 0, (LPSTR)pbCmdLine);

    /* WIN32: ExitProcess wrapper.
     * LINUX:  return exitCode from main(). */
    /* FUN_004684d0(exitCode); */
    ExitProcess((UINT)exitCode);
}


/* =========================================================================
 * WinMain — 0x00462e90
 * =========================================================================
 *
 * The real application entry point after CRT startup.
 *
 * Parameters
 *   hInstance   — module handle
 *   unused      — hPrevInstance (always 0 on Win32, ignored)
 *   lpCmdLine   — pointer into the raw command-line string, exe-name stripped
 *
 * Returns WM_QUIT wParam (0=clean exit, non-zero=error code).
 *
 * WIN32 APIs used
 *   CreateDialogParamA, LoadStringA, GetSystemMetrics, SetWindowPos,
 *   UpdateWindow, ShowWindow, SetWindowTextA, DestroyWindow,
 *   CoInitializeEx, CoUninitialize, FindWindowA, SetForegroundWindow,
 *   MessageBoxA, PostMessageA, GetMessageA, PeekMessageA, TranslateMessage,
 *   DispatchMessageA, MsgWaitForMultipleObjects, ResetEvent.
 *
 * LINUX equivalents
 *   SDL_CreateWindow + SDL_ShowWindow for the splash.
 *   Remove CoInitializeEx/CoUninitialize.
 *   Replace FindWindowA duplicate-check with a lock file or /tmp socket.
 *   Replace the Win32 message loop with SDL_PollEvent / SDL_WaitEventTimeout.
 *   Replace MsgWaitForMultipleObjects with sem_timedwait / poll().
 */
WPARAM WinMain(HINSTANCE hInstance, UINT unused, BYTE *lpCmdLine)
{
    HWND        hwndSplash   = NULL;
    HWND        hwndExisting = NULL;
    CGWND      *pGameWnd     = NULL;
    CHAR        szTitle[100];
    MSG         msg;
    int         screenCX, screenCY;
    int         splashX,  splashY;
    WPARAM      wExitCode;
    int         framesThisSec  = 0;
    int         peekThrottle   = 0;  /* countdown: flush queue every 14 frames */
    BOOL        bOk;

    (void)unused;
    (void)framesThisSec;

    memset(&msg, 0, sizeof(msg));

    /* ------------------------------------------------------------------
     * 1. Splash / loading dialog
     *    Resource 0x71 (113) is a borderless bitmap dialog shown while
     *    the engine initialises.  It is centred on screen.
     *
     * WIN32: CreateDialogParamA with a dialog-resource ID.
     * LINUX: SDL_CreateWindow + SDL_RenderCopy of a splash BMP.
     * ------------------------------------------------------------------ */
    hwndSplash = CreateDialogParamA(hInstance, (LPCSTR)(ULONG_PTR)0x71,
                                    NULL, NULL, 0);
    if (hwndSplash != NULL) {
        LoadStringA(hInstance, 1, szTitle, 100);

        /* Centre the 675×450 splash on screen */
        screenCY = GetSystemMetrics(SM_CYSCREEN);
        screenCX = GetSystemMetrics(SM_CXSCREEN);
        splashY  = (screenCY - 0x1c2) / 2;   /* (height - 450) / 2 */
        splashX  = (screenCX - 0x2a3) / 2;   /* (width  - 675) / 2 */

        /* WIN32: position without z-order change (uFlags=0) */
        SetWindowPos(hwndSplash, NULL, splashX, splashY,
                     0x2a3, 0x1c2, 0);
        UpdateWindow(hwndSplash);
        ShowWindow(hwndSplash, SW_SHOWNORMAL);
        SetWindowTextA(hwndSplash, szTitle);
    }

    /* ------------------------------------------------------------------
     * 2. Construct CGWND — the engine root object
     * ------------------------------------------------------------------ */

    /* WIN32: g_hInstance stored globally for window-class registration */
    /* DAT_004a991c = hInstance; */

    /* Allocate 0x28 (40) bytes; FUN_00465ce0 is operator new with SEH */
    pGameWnd = (CGWND *)loco_malloc(0x28);
    if (pGameWnd == NULL) {
        g_pGameWnd = NULL;
    } else {
        g_pGameWnd = CGWND_Constructor(pGameWnd, hInstance);
    }

    if (g_pGameWnd == NULL) {
        /* Show localised "cannot initialise" error box */
        /* FUN_00447330(&g_stringTable, 0x14a, errorMsg, 0x200); */
        MessageBoxA(NULL, "", "LEGO LOCO", MB_ICONEXCLAMATION);
        return (WPARAM)-2;
    }

    /* ------------------------------------------------------------------
     * 3. COM initialisation (needed by DirectPlay)
     *
     * WIN32: CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)
     * LINUX: Remove entirely; rewrite networking with BSD sockets.
     * ------------------------------------------------------------------ */
    CoInitializeEx(NULL, 0 /* COINIT_APARTMENTTHREADED */);

    /* ------------------------------------------------------------------
     * 4. Load configuration (registry → lego.ini)
     *    Returns 0 if the data directory is not accessible.
     * ------------------------------------------------------------------ */
    if (!CGWND_LoadConfig()) {
        /* Destroy CGWND via vtable destructor slot 0, flag=1 */
        if (g_pGameWnd) {
            (*(void(**)(int))g_pGameWnd->vtable[0])(1);
            g_pGameWnd = NULL;
        }
        CoUninitialize();
        MessageBoxA(NULL, "", "LEGO LOCO", MB_ICONEXCLAMATION);
        return (WPARAM)-1;
    }

    /* ------------------------------------------------------------------
     * 5. Parse command-line flags (seasons, debug/help)
     * ------------------------------------------------------------------ */
    CGWND_ParseCommandLine(lpCmdLine);

    /* ------------------------------------------------------------------
     * 6. Validate display (colour depth, resolution)
     * ------------------------------------------------------------------ */
    /* FUN_004480c0 wraps CGWND_SetupDisplay + CGWND_CheckDisplayCaps */
    if (!FUN_004480c0(0)) {
        if (g_pGameWnd) {
            (*(void(**)(int))g_pGameWnd->vtable[0])(1);
            g_pGameWnd = NULL;
        }
        CoUninitialize();
        return 0;
    }

    /* ------------------------------------------------------------------
     * 7. Duplicate-instance check
     *    Unless in debug/help mode, look for an existing "LEGO LOCO"
     *    window and bring it to the foreground if found.
     *
     * WIN32: FindWindowA + SetForegroundWindow
     * LINUX: Use a lock file (/tmp/lego-loco.lock) or a named Unix socket.
     * ------------------------------------------------------------------ */
    if (g_debugMode == 0) {
        hwndExisting = FindWindowA("LEGO LOCO", "LEGO LOCO");
        if (hwndExisting != NULL) {
            SetForegroundWindow(hwndExisting);
            if (g_pGameWnd) {
                (*(void(**)(int))g_pGameWnd->vtable[0])(1);
                g_pGameWnd = NULL;
            }
            CoUninitialize();
            return 1;   /* another instance is already running */
        }
    }

    /* Update splash while we initialise the subsystems */
    if (hwndSplash) UpdateWindow(hwndSplash);

    /* ------------------------------------------------------------------
     * 8. GameLoop_Setup — allocate all subsystems, create window, start timer
     * ------------------------------------------------------------------ */
    wExitCode = (WPARAM)GameLoop_Setup(g_pGameWnd);
    if (wExitCode != 0) {
        /* Init failed — show error, clean up */
        /* FUN_00447330(&g_stringTable, 0x14a, errorMsg, 0x200); */
        MessageBoxA(NULL, "", "LEGO LOCO", MB_ICONEXCLAMATION);
        CGWND_Shutdown();
        if (g_pGameWnd) {
            (*(void(**)(int))g_pGameWnd->vtable[0])(1);
            g_pGameWnd = NULL;
        }
        CoUninitialize();
        return wExitCode;
    }

    /* ------------------------------------------------------------------
     * 9. Transition to loading state and show the game window
     *
     * g_debugMode==0 → fullscreen (state 2 = LOADING)
     * g_debugMode==1 → windowed  (state 1 = debug startup)
     * ------------------------------------------------------------------ */
    if (hwndSplash) UpdateWindow(hwndSplash);
    SetGameState((g_debugMode == 0) ? 2 : 1);

    /* Show main game window (HWND stored at g_pGameWnd->hwndGame) */
    ShowWindow(g_pGameWnd->hwndGame, SW_SHOWNORMAL);

    /* Close the splash dialog */
    PostMessageA(hwndSplash, WM_CLOSE, 0, 0);
    DestroyWindow(hwndSplash);
    hwndSplash = NULL;

    /* ------------------------------------------------------------------
     * 10. Loading-phase message loop (state == 2)
     *
     * While the background loading thread is active (g_gameState==2) the
     * engine uses a blocking GetMessageA pump.  The event queue subsystem
     * (DAT_004fd3ac) is ticked manually after each message.
     *
     * WIN32: GetMessageA, TranslateMessage, DispatchMessageA
     * LINUX: SDL_WaitEvent loop; tick subsystems in the event handler.
     * ------------------------------------------------------------------ */
    while (msg.message != WM_QUIT && g_gameState == 2) {
        bOk = GetMessageA(&msg, NULL, 0, 0);
        if (bOk > 0) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        /* Tick event-queue subsystem after each OS message */
        /* if (g_pEventQueue) FUN_0043f0c0(g_pEventQueue); */
    }

    /* ------------------------------------------------------------------
     * 11. Running-phase message loop (state == 3)
     *
     * Uses PeekMessageA + MsgWaitForMultipleObjects.  The multimedia timer
     * fires every 28ms (~35.7fps) and sets g_timerFired; the main loop
     * calls GameFrame_Update then ResetEvent to acknowledge.
     *
     * peekThrottle: every 14 iterations the loop does a full GetMessage
     * drain; otherwise it does a non-blocking PeekMessage(PM_REMOVE) drain.
     * This reduces latency spikes from heavy message queues.
     *
     * WIN32: PeekMessageA, MsgWaitForMultipleObjects, ResetEvent
     * LINUX: SDL_PollEvent + sem_timedwait(28ms) or SDL_WaitEventTimeout.
     * ------------------------------------------------------------------ */
    while (msg.message != WM_QUIT) {
        /* Reset per-frame performance counters */
        /* _DAT_004ff12c = _DAT_004ff118 = _DAT_004ff128 = 0; */

        if (peekThrottle < 1) {
            /* Full queue flush every 14 frames */
            peekThrottle = 14;
            if (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE)) {
                while (GetMessageA(&msg, NULL, 0, 0) > 0) {
                    TranslateMessage(&msg);
                    DispatchMessageA(&msg);
                    if (!PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE)) break;
                }
            }
        } else {
            peekThrottle--;
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }

        /* Tick background loader if active (DAT_004aa4a4) */
        /* if (g_bgLoadActive) FUN_0045e1e0(0); */

        /* WIN32: Wait for multimedia timer OR any window message (QS_ALLINPUT).
         * LINUX: sem_timedwait(&g_frameSem, &deadline) where deadline=now+28ms. */
        MsgWaitForMultipleObjects(0, NULL, FALSE, 3, QS_ALLINPUT /* 0xbf */);

        /* Per-frame update when timer fires */
        if (g_timerFired) {
            framesThisSec++;
            GameFrame_Update();
            /* WIN32: ResetEvent on the manual-reset "GameLoop" event.
             * LINUX: sem_trywait(&g_frameSem) or pthread_cond_signal pattern. */
            ResetEvent(g_hGameLoopEvent);
        }

        /* FPS measurement every DAT_00481914 frames */
        /* ... (see original code for counter logic using QueryPerformanceCounter) */
    }

    /* ------------------------------------------------------------------
     * 12. Cleanup
     * ------------------------------------------------------------------ */
    if (g_pGameWnd) {
        (*(void(**)(int))g_pGameWnd->vtable[0])(1);
        g_pGameWnd = NULL;
    }
    CoUninitialize();   /* LINUX: remove */
    return msg.wParam;
}


/* =========================================================================
 * CGWND_Constructor — 0x004061e0
 * =========================================================================
 *
 * C++ thiscall constructor for the CGWND engine root object.
 *
 * Parameters
 *   self      — pre-allocated 0x28-byte buffer
 *   hInstance — Win32 module handle stored at self+0x0C
 *
 * Returns self pointer (C++ convention).
 *
 * WIN32: GetDesktopWindow, SetRect
 * LINUX: SDL_GetDisplayBounds(0, &bounds) for the desktop rect equivalent.
 */
CGWND *CGWND_Constructor(CGWND *self, HINSTANCE hInstance)
{
    /* Install vtable pointer (generated by the C++ compiler) */
    self->vtable = (void **)&PTR_FUN_004774c4;

    /* Initialise all state fields to zero */
    self->stateFlag = 0;
    /* DAT_004a97a4 = 0; */   /* auxiliary engine flag */
    self->hwndGame  = NULL;
    self->hInstance = hInstance;

    /* WIN32: Get the desktop window for display-caps queries.
     * LINUX: Not needed; use SDL_GetCurrentDisplayMode instead. */
    self->hwndDesktop = GetDesktopWindow();

    /* Reset global display/state variables */
    /* DAT_00485234 guard: if set, clear it and related display globals */
    g_gameState = GAME_STATE_INIT;   /* DAT_004851f4 = INIT */
    /* DAT_004851f0 = DAT_00485238 = DAT_00485210 = 0; */

    /* Zero all screen RECTs via SetRect(rect, 0, 0, 0, 0) */
    /* WIN32: SetRect is a USER32 helper to fill RECT fields.
     * LINUX: memset(&rect, 0, sizeof(RECT)) or SDL_Rect = {0}. */
    SetRect((LPRECT)&DAT_004851e0, 0, 0, 0, 0);
    SetRect((LPRECT)&DAT_00485200, 0, 0, 0, 0);
    SetRect((LPRECT)&DAT_00485220, 0, 0, 0, 0);

    /* Zero version fields */
    self->versionMajor    = 0;
    self->versionMinor    = 0;
    self->versionBuild    = 0;
    self->versionRevision = 0;

    /* Read FileVersion resource and populate the four version fields above */
    CGWND_ReadVersionInfo(self);

    return self;
}


/* =========================================================================
 * CGWND_SetVisible — 0x004061b0
 * =========================================================================
 *
 * Shows or hides an object that has an embedded CGWND-like vtable.
 * Called with self=CGWND* to toggle game-window visibility.
 *
 * WIN32: virtual dispatch via vtable slot 1 (offset +4).
 * LINUX: SDL_ShowWindow / SDL_HideWindow.
 */
void CGWND_SetVisible(CGWND *self, char bVisible)
{
    /* Store visibility flag at self+0x24 */
    *(char *)((char *)self + 0x24) = bVisible;

    /* Call virtual OnShow/OnHide — vtable[1] (offset +4) */
    (*(void(**)(void))self->vtable[1])();

    /* If a secondary surface handle is attached (self+0x48), show/hide it */
    /* FUN_0040ee20 = show, FUN_0040ee00 = hide */
}


/* =========================================================================
 * CGWND_ReadVersionInfo — 0x004062e0
 * =========================================================================
 *
 * Reads the "FileVersion" string from the running EXE's VS_VERSIONINFO
 * resource block and parses it into four integer fields stored on the
 * CGWND object (versionMajor..versionRevision at offsets +0x18..+0x24).
 *
 * Note: Ghidra shows __cdecl calling convention but 'self' arrives in ECX
 * (extraout_ECX) because the caller is a __thiscall constructor.  The struct
 * writes via extraout_ECX confirm this is effectively a method on CGWND.
 *
 * WIN32: GetModuleHandleA, GetModuleFileNameA,
 *        GetFileVersionInfoSizeA, GetFileVersionInfoA, VerQueryValueA.
 * LINUX: Read version from a companion VERSION text file or a compile-time
 *        constant.  Use readlink("/proc/self/exe") to get the exe path.
 */
void CGWND_ReadVersionInfo(CGWND *self)
{
    HMODULE     hMod;
    char        exePath[0x504];
    DWORD       dwInfoSize;
    void       *pVersionData = NULL;
    char       *pVersionStr;
    UINT        uLen;
    char        versionBuf[0x400];
    BOOL        bOk;

    (void)self;
    (void)versionBuf;

    /* WIN32: Get exe path for GetFileVersionInfoA */
    hMod = GetModuleHandleA(NULL);
    GetModuleFileNameA(hMod, exePath, sizeof(exePath));

    /* Query size of version info block */
    dwInfoSize = GetFileVersionInfoSizeA(exePath, NULL);
    if (dwInfoSize != 0) {
        pVersionData = loco_malloc(dwInfoSize);
    }

    if (pVersionData != NULL) {
        /* WIN32: Load the version info block */
        bOk = GetFileVersionInfoA(exePath, 0, dwInfoSize, pVersionData);
        if (bOk) {
            /* Query the FileVersion string from codepage 080904B0 (English/Latin-1) */
            bOk = VerQueryValueA(pVersionData,
                                 "\\StringFileInfo\\080904B0\\FileVersion",
                                 (LPVOID *)&pVersionStr,
                                 &uLen);
            if (bOk && uLen != 0) {
                /* Copy version string into local buffer */
                memcpy(versionBuf, pVersionStr, uLen);
            }
        }
        loco_free(pVersionData);
    }

    /*
     * Tokenise "major.minor.build.revision" on '.' and convert each
     * field to an integer via FUN_00466390 (atoi).  Results are written
     * into self->versionMajor .. self->versionRevision (offsets +0x18..+0x24).
     *
     * LINUX: sscanf(versionBuf, "%d.%d.%d.%d",
     *               &self->versionMajor, &self->versionMinor,
     *               &self->versionBuild, &self->versionRevision);
     */
    /* FUN_004663a0 = strtok, FUN_00466390 = atoi */
}


/* =========================================================================
 * CGWND_SetupDisplay — 0x00406480
 * =========================================================================
 *
 * Reads screen metrics and INI settings to configure the game window
 * geometry and per-object FPS thresholds.  Called once during startup
 * (from GameLoop_Setup indirectly).
 *
 * WIN32: GetDesktopWindow, GetSystemMetrics, SetRect.
 * LINUX: SDL_GetCurrentDisplayMode for screen width/height.
 *        Load INI settings with a plain INI parser (SDL_RWops + hand-rolled).
 */
void CGWND_SetupDisplay(CGWND *self)
{
    RECT screenRect;

    /* Refresh desktop handle */
    self->hwndDesktop = GetDesktopWindow();

    /* WIN32: Primary display dimensions.
     * LINUX: SDL_GetCurrentDisplayMode(0, &mode); w=mode.w; h=mode.h; */
    g_screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    g_screenHeight = GetSystemMetrics(SM_CYSCREEN);

    /* Compute screen-centre coordinates */
    /* g_screenCentreX = g_screenWidth  / 2; */
    /* g_screenCentreY = g_screenHeight / 2; */

    /* Full-screen rectangle */
    SetRect(&screenRect, 0, 0, g_screenWidth, g_screenHeight);

    /*
     * Read window sub-rect from INI section [WINDOW_ATTRIBUTES].
     * Keys: RectLeft (default 50), RectTop (50), RectRight (w-50), RectBottom (h-50).
     * Clamp each edge so the window stays >= 10 pixels inside screen bounds.
     *
     * LINUX: ini_gets("WINDOW_ATTRIBUTES", "RectLeft", "50", buf, sizeof(buf), "lego.ini")
     *        using the minIni or inih library.
     */
    /* g_windowRect.left   = clamp(ini_read(WINDOW_ATTRIBUTES, RectLeft,   50), ...); */
    /* g_windowRect.top    = clamp(ini_read(WINDOW_ATTRIBUTES, RectTop,    50), ...); */
    /* g_windowRect.right  = clamp(ini_read(WINDOW_ATTRIBUTES, RectRight,  w-50), ...); */
    /* g_windowRect.bottom = clamp(ini_read(WINDOW_ATTRIBUTES, RectBottom, h-50), ...); */

    /*
     * Per-object FPS thresholds from [BALANCING] section.
     * Stored at self+0x11..0x14.  Lower threshold → object is skipped
     * when frame rate falls below the limit.
     */
    self->minVehicleFPS  = /* ini_read(BALANCING, MinVehicleFPS,  20) */ 20;
    self->minBuildingFPS = /* ini_read(BALANCING, MinBuildingFPS, 18) */ 18;
    self->minMinifigFPS  = /* ini_read(BALANCING, MinMinifigFPS,  16) */ 16;
    self->minFlyingFPS   = /* ini_read(BALANCING, MinFlyingFPS,   14) */ 14;

    /*
     * Read PROCESS/CleanExit flag.  If it is 0 on startup the previous
     * session crashed (no orderly shutdown).  Then write 0 back to mark
     * the new session as potentially unclean.
     *
     * LINUX: Same logic; write the flag to the INI file with ini_puts().
     */
    /* g_cleanExitFlag = ini_read(PROCESS, CleanExit, 1); */
    /* ini_write(PROCESS, CleanExit, 0); */
}


/* =========================================================================
 * CGWND_CheckDisplayCaps — 0x00406680
 * =========================================================================
 *
 * Validates that the display supports the game's requirements:
 *   • True-colour (not palettised high-colour) mode.
 *   • Screen width >= 800 pixels.
 *
 * Returns 0 in the high 24 bits on success (low byte = 1 = OK).
 * Non-zero = fatal display error; shows a MessageBoxA.
 *
 * WIN32: GetDC, GetDeviceCaps (BITSPIXEL 0x18, PLANES 0x0C), ReleaseDC,
 *        GetSystemMetrics (SM_SAMEDISPLAYFORMAT 0x13), MessageBoxA.
 * LINUX: SDL_GetCurrentDisplayMode; check SDL_BYTESPERPIXEL(mode.format).
 *        Show error with fprintf(stderr,...) or SDL_ShowSimpleMessageBox.
 */
UINT CGWND_CheckDisplayCaps(CGWND *self)
{
    HDC  hdc;
    UINT uBitsPerPixel;
    int  nPlanes;
    UINT uErrorStringId;

    /* WIN32: Obtain a DC for the desktop window.
     * LINUX: SDL_GetCurrentDisplayMode gives bpp without a DC. */
    hdc = GetDC(self->hwndDesktop);
    uBitsPerPixel = (UINT)GetDeviceCaps(hdc, BITSPIXEL);
    nPlanes       = GetDeviceCaps(hdc, PLANES);
    ReleaseDC(self->hwndDesktop, hdc);

    /*
     * Palettised or unusual display: the check for colour depth < minimum
     * is combined with a PLANES > 16 guard to filter out EGA/VGA-style
     * planar modes.
     *
     * LINUX: if (SDL_BYTESPERPIXEL(mode.format) < 2) → unsupported.
     */
    if ((uBitsPerPixel < 0x80000000u) || (nPlanes > 16)) {
        uErrorStringId = 0x7a;  /* "requires 16-bit colour or higher" */
    } else {
        /* Check all monitors use the same display format (multi-monitor) */
        /* WIN32: SM_SAMEDISPLAYFORMAT (0x13) */
        if (!GetSystemMetrics(0x13)) {
            uErrorStringId = 0x7b;  /* "inconsistent display format" */
        } else {
            /* Resolution check: must be at least 800 wide */
            if (g_screenWidth > 800) {
                /* 1024×768 or higher — OK, return success */
                return ((UINT)(g_screenWidth >> 8) << 8) | 1;
            }
            /* 640×480 or 800×600 — show resolution warning */
            /* FUN_00447330(&g_stringTable, 0x7a, local_100, 0x100); */
            goto show_error;
        }
    }
    (void)uErrorStringId;
    /* Load and display the appropriate error string */
    /* FUN_00447330(&g_stringTable, uErrorStringId, local_100, 0x100); */
show_error:
    MessageBoxA(NULL, "", "LEGO LOCO", 0);
    return 0;   /* failure */
}


/* =========================================================================
 * CGWND_ParseCommandLine — 0x00406790
 * =========================================================================
 *
 * Tokenises the command-line tail on spaces and recognises the following
 * flags (all case-insensitive):
 *
 *   -h, -?, -help     → g_debugMode = 1  (windowed / help mode)
 *   Easter            → g_seasonOverride = 1
 *   Desert            → g_seasonOverride = 2
 *   Halloween         → g_seasonOverride = 3
 *   Winter            → g_seasonOverride = 4
 *   <unknown>         → g_seasonOverride = 5
 *
 * WIN32: No Win32 APIs — pure string processing.
 * LINUX: Same; replace the internal strtok (FUN_004663a0) with strtok_r.
 */
void CGWND_ParseCommandLine(BYTE *lpCmdLine)
{
    BYTE *token;

    g_seasonOverride = 0;

    /* FUN_004663a0(s, delim) = first call of strtok */
    token = (BYTE *)strtok((char *)lpCmdLine, " " /* DAT_0047e204 */);

    while (token != NULL) {
        /* Help/debug flags */
        if (_stricmp((char *)token, "-h")    == 0 ||
            _stricmp((char *)token, "-?")    == 0 ||
            _stricmp((char *)token, "-help") == 0) {
            g_debugMode = 1;
        }
        /* Season overrides */
        else if (_stricmp((char *)token, "Easter")    == 0) g_seasonOverride = 1;
        else if (_stricmp((char *)token, "Desert")    == 0) g_seasonOverride = 2;
        else if (_stricmp((char *)token, "Halloween") == 0) g_seasonOverride = 3;
        else if (_stricmp((char *)token, "Winter")    == 0) g_seasonOverride = 4;
        else                                                  g_seasonOverride = 5;

        /* Next token */
        token = (BYTE *)strtok(NULL, " ");
    }
}


/* =========================================================================
 * CGWND_LoadConfig — 0x004068d0
 * =========================================================================
 *
 * Locates lego.ini via the Windows registry and loads it into a CIniFile
 * object (g_pIniFile / DAT_004a9eec).  Also reads directory paths from
 * the INI file and validates the local resource directory.
 *
 * Flow
 *   1. Open HKLM\SOFTWARE\Intelligent Games\LEGO LOCO.
 *   2. Read the default value (install dir) with RegQueryValueExA.
 *   3. If the key doesn't exist, call FUN_00466950 to search common
 *      locations (program files, Windows dir, etc.), then write the found
 *      path back to the registry with RegCreateKeyExA + RegSetValueExA.
 *   4. Append '\' if missing, then append 'lego.ini'.
 *   5. Construct a CIniFile object wrapping the path.
 *   6. Read DIRECTORIES/LocalRes  → local resource path (DAT_004a99c8).
 *   7. Read DIRECTORIES/RemoteRes → remote resource path (DAT_004a97a8).
 *      If g_debugMode==1, clear the remote path.
 *   8. Strip trailing '\' from the local path.
 *   9. Call FUN_00466590 (access() equivalent) on the local path.
 *  10. Return 1 if local path exists and is longer than 2 chars; else 0.
 *
 * WIN32: RegOpenKeyExA, RegQueryValueExA, RegCloseKey,
 *        RegCreateKeyExA, RegSetValueExA.
 * LINUX: Skip registry entirely.  Use:
 *          • $LEGO_LOCO_HOME or XDG_DATA_DIRS for install path discovery.
 *          • Or hard-code path relative to exe (readlink /proc/self/exe).
 *        Then load the INI file directly with a plain fopen + line parser.
 */
BOOL CGWND_LoadConfig(void)
{
    HKEY    hKey;
    LSTATUS lstatus;
    char    installDir[0x504];
    char    queryBuf[0x504];
    DWORD   dwBufSize = 0x504;
    void   *pIniObj;

    /* ------------------------------------------------------------------
     * Try to read the install directory from the registry.
     *
     * WIN32: HKEY_LOCAL_MACHINE = 0x80000002
     * LINUX: Skip.  Compute iniPath from readlink("/proc/self/exe").
     * ------------------------------------------------------------------ */
    lstatus = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                            "SOFTWARE\\Intelligent Games\\LEGO LOCO",
                            0, KEY_READ, &hKey);
    if (lstatus == ERROR_SUCCESS) {
        DWORD dwType = 0;
        lstatus = RegQueryValueExA(hKey, NULL, NULL,
                                   &dwType, (LPBYTE)queryBuf, &dwBufSize);
        RegCloseKey(hKey);
        if (lstatus == ERROR_SUCCESS) {
            /* Copy registry value to installDir */
            memcpy(installDir, queryBuf, strlen(queryBuf) + 1);
        } else {
            installDir[0] = '\0';
        }
    } else {
        /* Registry key missing — search filesystem (FUN_00466950) */
        installDir[0] = '\0';
        /* FUN_00466950(installDir, 0x100); */

        /* Write discovered path back to registry */
        lstatus = RegCreateKeyExA(HKEY_LOCAL_MACHINE,
                                  "SOFTWARE\\Intelligent Games\\LEGO LOCO",
                                  0, NULL, 0, KEY_ALL_ACCESS,
                                  NULL, &hKey, NULL);
        if (lstatus == ERROR_SUCCESS) {
            RegSetValueExA(hKey, NULL, 0, REG_SZ,
                           (BYTE *)installDir,
                           (DWORD)(strlen(installDir) + 1));
            RegCloseKey(hKey);
        }
    }

    /* Ensure path ends with '\' then append "lego.ini" */
    if (installDir[0] != '\0' &&
        installDir[strlen(installDir) - 1] != '\\') {
        strcat(installDir, "\\");
    }
    strcat(installDir, "lego.ini");

    /* ------------------------------------------------------------------
     * Construct CIniFile object (0x10C bytes) wrapping the path.
     * LINUX: Replace with a plain fopen + hand-rolled INI parser.
     * ------------------------------------------------------------------ */
    pIniObj = loco_malloc(0x10c);
    if (pIniObj == NULL) {
        g_pIniFile = NULL;
    } else {
        g_pIniFile = /* FUN_00452ce0(pIniObj, installDir) */ NULL;
        if (g_pIniFile == NULL) {
            loco_free(pIniObj);
        }
    }

    /*
     * Read directory paths:
     *   DIRECTORIES/LocalRes  → DAT_004a99c8 (must end without '\')
     *   DIRECTORIES/RemoteRes → DAT_004a97a8 (CD-ROM or network path)
     */
    /* FUN_00452d80(g_pIniFile, "DIRECTORIES", "LocalRes",  ..., g_localResDir,  256); */
    /* FUN_00452d80(g_pIniFile, "DIRECTORIES", "RemoteRes", ..., g_remoteResDir, 256); */

    if (g_debugMode == 1) {
        /* In debug/help mode, suppress any remote resource path */
        /* g_remoteResDir[0] = '\0'; */
    }

    /* Strip trailing backslash from local path, then stat it */
    /* LINUX: access(g_localResDir, F_OK) or stat() */
    /* return (FUN_00466590(g_localResDir, ...) == 0 && strlen(g_localResDir) > 2); */
    return TRUE; /* placeholder */
}


/* =========================================================================
 * CGWND_RegisterAndCreateWindow — 0x00406ed0
 * =========================================================================
 *
 * Registers the "LEGO LOCO" window class and creates the main game window.
 *
 * Class style   : CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS (0x0B)
 * WndProc       : DAT_004618c0 (the main window procedure)
 * Window style  : WS_POPUP | WS_VISIBLE (0x82000000) — no caption bar
 * Extended style: WS_EX_TOPMOST (0x08) in fullscreen;
 *                 WS_EX_APPWINDOW (0x08) in windowed (g_debugMode==1)
 *
 * On success stores the HWND at self->hwndGame (self+0x08) and records
 * the client rect in DAT_00485220.
 *
 * WIN32: RegisterClassA, CreateWindowExA, LoadIconA, GetClientRect.
 * LINUX: SDL_CreateWindow(title, x, y, w, h, SDL_WINDOW_FULLSCREEN_DESKTOP
 *                                              | SDL_WINDOW_BORDERLESS).
 *        Store SDL_Window* in self->hwndGame.
 */
BOOL CGWND_RegisterAndCreateWindow(CGWND *self)
{
    WNDCLASSA wc;

    memset(&wc, 0, sizeof(wc));

    /* WIN32: Register window class.
     * LINUX: Not needed — SDL handles window management. */
    wc.hInstance    = self->hInstance;
    wc.style        = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc  = (WNDPROC)&DAT_004618c0;   /* main WndProc */
    wc.cbClsExtra   = 0;
    wc.cbWndExtra   = 0;
    /* WIN32: Load icon resource 0x65 (101) */
    wc.hIcon        = LoadIconA(self->hInstance, (LPCSTR)(ULONG_PTR)0x65);
    wc.hCursor      = NULL;
    wc.hbrBackground= NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName= "LEGO LOCO";
    RegisterClassA(&wc);

    /*
     * Create the main game window at (0,0), full-screen dimensions.
     * Extended style: 0x08 for either WS_EX_TOPMOST or WS_EX_APPWINDOW.
     * Style: WS_POPUP (0x80000000) | WS_VISIBLE (0x10000000) = 0x82000000.
     *
     * WIN32: CreateWindowExA
     * LINUX: SDL_CreateWindow("LEGO LOCO", 0, 0, g_screenWidth, g_screenHeight,
     *                          SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS)
     */
    self->hwndGame = CreateWindowExA(
        8,                  /* dwExStyle: WS_EX_TOPMOST / WS_EX_APPWINDOW */
        "LEGO LOCO",        /* lpClassName */
        "LEGO LOCO",        /* lpWindowName */
        0x82000000,         /* dwStyle: WS_POPUP | WS_VISIBLE */
        0, 0,               /* x, y */
        g_screenWidth,      /* nWidth */
        g_screenHeight,     /* nHeight */
        NULL,               /* hWndParent */
        NULL,               /* hMenu */
        self->hInstance,
        NULL);

    if (self->hwndGame == NULL) return FALSE;

    /* Record client area rect — used by the renderer */
    /* WIN32: GetClientRect — LINUX: SDL_GetWindowSize */
    GetClientRect(self->hwndGame, (LPRECT)&DAT_00485220);

    return TRUE;
}


/* =========================================================================
 * CGWND_InitGraphicsSubsystems — 0x00406f90
 * =========================================================================
 *
 * Allocates and constructs eight DirectX-layer subsystem objects in
 * dependency order.  If any allocation or initialisation fails, all
 * already-created objects are destroyed in reverse order before returning
 * a negative error code.
 *
 * All constructors follow the pattern:
 *   FUN_xxxxxxxx(buffer, hInstance, resourceId) → object*
 * All init functions follow:
 *   FUN_xxxxxxxx(object, hwndGame) → BOOL
 * All destructors are at vtable slot 0:
 *   (*vtable[0])(1)  ← flag=1 means "delete this"
 *
 * WIN32: Indirectly uses DirectDraw, DirectSound, DirectInput, DirectPlay.
 * LINUX: Replace with SDL2 / OpenAL / POSIX equivalents of each subsystem.
 *
 * Returns 0 on full success; negative error codes on partial failure.
 */
int CGWND_InitGraphicsSubsystems(CGWND *self)
{
    void *p;

    /* 1. DirectDraw manager (0x224 bytes, resource id 0x1f8) */
    p = loco_malloc(0x224);
    g_pDirectDraw = p ? FUN_004202f0(p, self->hInstance, 0x1f8) : NULL;
    if (!g_pDirectDraw) return -2;
    if (!FUN_004204d0(g_pDirectDraw, self->hwndGame)) {
        (*(void(**)(int))g_pDirectDraw)(1);
        return -3;
    }

    /* 2. DirectSound manager (0x6e0 bytes, resource id 0x1f5) */
    p = loco_malloc(0x6e0);
    g_pDirectSound = p ? FUN_0042e900(p, self->hInstance, 0x1f5) : NULL;
    if (!g_pDirectSound) { /* destroy #1 */ return -4; }
    if (!FUN_0042edb0(g_pDirectSound, self->hwndGame)) {
        /* destroy #1, #2 */ return -5;
    }

    /* 3. Network manager (0x2c4 bytes, resource id 0x1f7) */
    p = loco_malloc(0x2c4);
    g_pNetworkMgr = p ? FUN_00430a90(p, self->hInstance, 0x1f7) : NULL;
    if (!g_pNetworkMgr) { return -6; }
    if (!FUN_00402520(g_pNetworkMgr, self->hwndGame)) { return -7; }

    /* 4. Animation manager (0x1d4 bytes, resource id 0x1fc) */
    p = loco_malloc(0x1d4);
    g_pAnimMgr = p ? FUN_00436b20(p, self->hInstance, 0x1fc) : NULL;
    if (!g_pAnimMgr) { return -8; }
    if (!FUN_00436c50(g_pAnimMgr, self->hwndGame)) { return -9; }

    /* 5. Input manager (0x254 bytes, resource id 0x1fb) */
    p = loco_malloc(0x254);
    g_pInputMgr = p ? FUN_00401f50(p, self->hInstance, 0x1fb) : NULL;
    if (!g_pInputMgr) { return -10; }
    if (!FUN_00402520(g_pInputMgr, self->hwndGame)) { return -11; }

    /* 6. Movie player (0x740 bytes, resource id 0x1fa) */
    p = loco_malloc(0x740);
    g_pMovieMgr = p ? FUN_00415980(p, self->hInstance, 0x1fa) : NULL;
    if (!g_pMovieMgr) { return -12; }
    if (!(char)FUN_004169e0(g_pMovieMgr, self->hwndGame)) { return -13; }

    /* 7. Scene manager (0x3078 bytes, resource id 0x1fe) */
    p = loco_malloc(0x3078);
    g_pSceneMgr = p ? FUN_0044f490(p, self->hInstance, 0x1fe) : NULL;
    if (!g_pSceneMgr) { return -14; }
    if (!FUN_00450ca0(g_pSceneMgr, self->hwndGame)) { return -15; }

    /* 8. World manager (0x1184 bytes, resource id 0x1fd) */
    p = loco_malloc(0x1184);
    g_pWorldMgr = p ? FUN_0040f1c0(p, self->hInstance, 0x1fd) : NULL;
    if (!g_pWorldMgr) { return -16; }
    if (!FUN_0040f510(g_pWorldMgr, self->hwndGame)) { return -17; }

    return 0;   /* success */
}


/* =========================================================================
 * GameLoop_Setup — 0x00406ba0
 * =========================================================================
 *
 * Initialises the complete game infrastructure before the main loop starts.
 *
 * Steps (in order)
 *  1. Reset all subsystem global pointers to NULL.
 *  2. Create six utility subsystems (timer service, config manager, event
 *     queue, string table, save-game manager, debug log).
 *  3. Read mouse sensitivity settings from INI [MOUSE] section.
 *  4. Register+create the main game window (CGWND_RegisterAndCreateWindow).
 *  5. Load the string table resource file (FUN_00446050).
 *  6. Create the eight DirectX subsystems (CGWND_InitGraphicsSubsystems).
 *  7. Load map-selection data (FUN_00429ef0).
 *  8. Initialise thumbnail palette loader (Thumbnails_Init).
 *  9. Create a named manual-reset Win32 event "GameLoop".
 * 10. Set multimedia timer: 14ms resolution, 28ms period (~35.7fps).
 *
 * WIN32: CreateEventA, timeBeginPeriod, timeSetEvent.
 * LINUX:
 *   Step 9: sem_init(&g_frameSem, 0, 0) or pthread_cond_init.
 *   Step 10: timer_create(CLOCK_MONOTONIC, ...) with SIGEV_THREAD, or
 *            a dedicated timer thread calling clock_nanosleep every 28ms.
 *
 * Returns 0 on success, -1 on any failure.
 */
int GameLoop_Setup(CGWND *self)
{
    void    *p;
    MMRESULT mmRes;

    /* Clear subsystem pointers */
    g_pTimerSvc    = NULL;  g_pConfigMgr   = NULL;
    g_pEventQueue  = NULL;  g_pStringTable = NULL;
    g_pSaveGameMgr = NULL;  g_pDebugLog    = NULL;
    g_pDirectDraw  = NULL;  g_pDirectSound = NULL;
    g_pNetworkMgr  = NULL;  g_pSceneMgr    = NULL;
    g_pWorldMgr    = NULL;  g_pInputMgr    = NULL;
    g_pMovieMgr    = NULL;  g_pAnimMgr     = NULL;

    /* ---- Utility subsystems ---- */

    /* Timer service (0x1c bytes) — wraps timeGetTime/QueryPerformanceCounter */
    p = loco_malloc(0x1c);
    g_pTimerSvc = p ? FUN_004493a0(p) : NULL;

    /* Config manager (0xb0 bytes) — wraps registry/INI key-value access */
    p = loco_malloc(0xb0);
    g_pConfigMgr = p ? FUN_00440c60(p) : NULL;

    /* Event queue (0x804 bytes) — in-game event dispatch bus */
    p = loco_malloc(0x804);
    g_pEventQueue = p ? FUN_0043d0a0(p) : NULL;

    /* String table (0xbe4 bytes) — localised string resource loader */
    p = loco_malloc(0xbe4);
    g_pStringTable = p ? FUN_00443000(p) : NULL;

    /* Save-game manager (0x124 bytes) */
    p = loco_malloc(0x124);
    g_pSaveGameMgr = p ? FUN_00452e10(p) : NULL;

    /* Debug log (0x18 bytes) */
    p = loco_malloc(0x18);
    g_pDebugLog = p ? FUN_00401620(p) : NULL;

    /* ---- Mouse settings from INI [MOUSE] section ---- */
    /* LINUX: ini_getl("MOUSE", "Setting1", 0, "lego.ini") */
    /* g_mouseSetting1 = FUN_00452d60(g_pIniFile, "MOUSE", "Setting1", 0); */
    /* g_mouseSetting2 = FUN_00452d60(g_pIniFile, "MOUSE", "Setting2", 0); */
    /* g_mouseSetting3 = FUN_00452d60(g_pIniFile, "MOUSE", "Setting3", 0); */

    /* ---- Register window class and create the game window ---- */
    if (!CGWND_RegisterAndCreateWindow(self))
        return -1;

    /* Load the string table resource (for localised error/UI strings) */
    if (!FUN_00446050(0))
        return -1;

    /* ---- DirectX subsystems ---- */
    if (CGWND_InitGraphicsSubsystems(self) != 0)
        return -1;

    /* ---- Map data and thumbnails ---- */
    /* FUN_00429ef0(&g_mapSelectData, &g_localResDir); */
    if (!Thumbnails_Init())
        return -1;

    /* ---- Synchronisation: named manual-reset event "GameLoop" ----
     *
     * The multimedia timer callback (at 0x45c520) will set this event.
     * The main loop calls ResetEvent after processing each frame.
     *
     * WIN32: CreateEventA(NULL, bManualReset=TRUE, bInitial=FALSE, "GameLoop")
     * LINUX: sem_init(&g_frameSem, 0, 0);
     *        timer thread: sem_post(&g_frameSem) every 28ms.
     */
    g_hGameLoopEvent = CreateEventA(NULL, TRUE, FALSE, "GameLoop");
    if (g_hGameLoopEvent == NULL)
        return -1;

    /* ---- Multimedia timer: 28ms period = ~35.7 fps ----
     *
     * timeBeginPeriod(14) sets the system timer resolution to 14ms.
     * timeSetEvent(28, 14, callback, 0, TIME_PERIODIC=1) fires the
     * callback (at 0x45c520, which sets g_timerFired=1) every 28ms.
     *
     * WIN32: timeBeginPeriod, timeSetEvent — from winmm.lib
     * LINUX: timer_create(CLOCK_MONOTONIC, &sev, &timerid);
     *        itimerspec = {.it_interval={0,28000000}, .it_value={0,28000000}};
     *        timer_settime(timerid, 0, &itimerspec, NULL);
     *        Timer signal handler posts to g_frameSem.
     */
    mmRes = timeBeginPeriod(14);
    if (mmRes == TIMERR_NOERROR) {
        g_timerID = timeSetEvent(28,    /* uDelay ms */
                                  14,   /* uResolution ms */
                                  (LPTIMECALLBACK)&TimerCallback_0x45c520,
                                  0,
                                  TIME_PERIODIC);
    }

    return 0;   /* success */
}


/* =========================================================================
 * SetGameState — 0x00408130
 * =========================================================================
 *
 * Central state machine for the game engine.  Only performs a transition
 * if newState != current g_gameState.  The previous state (prevState) is
 * passed to state-3 handler FUN_004086f0 so it can distinguish where to
 * resume.
 *
 * State 10 (QUIT) posts WM_CLOSE (0x10) to the game window via PostMessageA.
 *
 * WIN32: PostMessageA (state 10 only).
 * LINUX: SDL_PushEvent with SDL_QUIT type.
 */
void SetGameState(int newState)
{
    int prevState = g_gameState;

    if (g_gameState == newState) return;
    g_gameState = newState;

    switch (newState) {
    case GAME_STATE_INIT:   /* 1: INIT / world reset */
        FUN_00408350();
        break;
    case GAME_STATE_LOADING:  /* 2: LOADING — start the loading-screen DirectDraw mode */
        /* FUN_00411dc0(&g_sceneData, 0, 1, 0); */
        /* (*g_pDirectDraw->vtable[2])(); */
        break;
    case GAME_STATE_RUNNING:  /* 3: RUNNING — resume gameplay from previous state */
        FUN_004086f0(prevState);
        break;
    case GAME_STATE_PAUSED:   /* 4: PAUSED */
        /* FUN_004113a0(&g_sceneData, NULL);  FUN_00434800(...); etc. */
        break;
    case GAME_STATE_MENU_A:   /* 5: MAIN_MENU A — DirectSound mode */
        /* FUN_00411dc0(...); (*g_pDirectSound->vtable[2])(); */
        break;
    case GAME_STATE_MENU_B:   /* 6: MAIN_MENU B — Input mode */
        /* FUN_00411dc0(...); (*g_pInputMgr->vtable[2])(); */
        break;
    case GAME_STATE_MOVIE:    /* 7: MOVIE PLAYBACK */
        /* FUN_00416b80(g_pMovieMgr); */
        break;
    case GAME_STATE_SAVE:     /* 8: SAVE SCREEN — stash prev state in SceneManager */
        /* *(int*)((char*)g_pSceneMgr + 0x3074) = prevState; */
        break;
    case GAME_STATE_CREDITS:  /* 9: CREDITS / special state — same rendering as RUNNING */
        /* (*g_pDirectSound->vtable[2])(); */
        break;
    case GAME_STATE_QUIT:     /* 10: QUIT — send WM_CLOSE to game window
                               * WIN32: PostMessageA
                               * LINUX: SDL_PushEvent with SDL_QUIT */
        if (g_pDirectDraw && g_pGameWnd)
            PostMessageA(g_pGameWnd->hwndGame, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
}


/* =========================================================================
 * GameFrame_Update — 0x0045c3c0
 * =========================================================================
 *
 * Per-frame update called from the main message loop when the multimedia
 * timer fires (~35.7fps).  Ticks every active subsystem in a defined order.
 *
 * The function is gated by g_gameState: states 3 (RUNNING) and 9 (CREDITS)
 * receive the full update including AI and animation; other states (1, 2, 10)
 * receive only the essential subsystem ticks.
 *
 * WIN32: No direct Win32 API calls (all work is done through internal
 *        subsystem methods).
 * LINUX: The same logic applies; replace the DirectDraw flip call inside
 *        FUN_0044e020 with SDL_RenderPresent.
 */
void GameFrame_Update(void)
{
    /* Acknowledge the timer event */
    g_timerFired = 0;

    /* Record frame timestamp (QueryPerformanceCounter wrapper) */
    /* FUN_00466af0(&g_frameTimestamp); */

    /* Tick the event-dispatch queue */
    /* if (g_pEventQueue) FUN_0043f0c0(g_pEventQueue); */

    /* Network heartbeat */
    /* FUN_00448120(g_networkState); */

    /* States with full rendering updates: RUNNING (3) and CREDITS (9) */
    if (g_gameState == GAME_STATE_RUNNING || g_gameState == GAME_STATE_CREDITS) {
        /* Palette animation: if the palette-animation flag is set,
         * apply palette entries to the DirectDraw surface and flip.
         * LINUX: SDL_SetPaletteColors + SDL_RenderPresent. */
        /* FUN_0044e020(&g_palAnimData); */

        /* Reset per-frame palette-change flag */
        /* DAT_004ff11c = 0; */
    }

    /* Always: render world geometry */
    /* FUN_00423d70(g_pWorldData); */

    /* Always: update scene objects */
    /* FUN_00410840(&g_sceneData); */

    /* Always: tick audio */
    /* FUN_004497a0(&g_audioData); */

    /* Full gameplay states only */
    if (g_gameState == GAME_STATE_RUNNING || g_gameState == GAME_STATE_CREDITS) {
        /* AI: minifig pathfinding */
        /* FUN_0042d1a0(&g_minifigData); */

        /* AI: vehicle movement */
        /* FUN_00459da0(&g_vehicleData); */

        /* Animation tick */
        /* FUN_0041dd40(g_animState); */

        /* Building/track update */
        /* FUN_00434720(g_buildingState); */
    }

    /* Present frame (DirectDraw blit / SDL_RenderPresent) */
    /* FUN_00456150(&g_displayData, 0); */
}


/* =========================================================================
 * Thumbnails_Init — 0x0045c8a0
 * =========================================================================
 *
 * Allocates a thumbnail-palette manager (0x20 bytes) and loads the
 * thumbnail palette BMP from the resource directory.
 *
 * The BMP path is: <LocalResDir>\2\smisc\thumbpal.bmp
 * ('2' is a sub-directory prefix from the resource pack layout)
 *
 * WIN32: No Win32 APIs — uses internal file I/O wrappers.
 * LINUX: Use SDL_LoadBMP(path) to load the palette source image.
 *
 * Returns true if the manager object was successfully allocated.
 */
bool Thumbnails_Init(void)
{
    void *pThumbMgr;

    pThumbMgr = loco_malloc(0x20);
    g_pThumbnailMgr = pThumbMgr ? FUN_0042a110(pThumbMgr) : NULL;

    /* Build path: resourceDir + "2\smisc\thumbpal.bmp" */
    /* LINUX: snprintf(thumbPath, sizeof(thumbPath), "%s/2/smisc/thumbpal.bmp",
     *                g_localResDir); */
    /* FUN_00466d60(thumbPath, "2\\smisc\\thumbpal.bmp"); */

    /* Load palette BMP into thumbnail manager */
    /* FUN_0042ab10(g_pThumbnailMgr, thumbPath, 1, 0, 0); */

    return g_pThumbnailMgr != NULL;
}


/* =========================================================================
 * CGWND_Shutdown — 0x004077a0
 * =========================================================================
 *
 * Orderly engine shutdown called at WM_QUIT or on fatal init failure.
 *
 * Steps
 *  1. Persist current window geometry back to lego.ini.
 *  2. Write PROCESS/CleanExit=1 to INI (marks that this session ended cleanly).
 *  3. Wait for any background loading/streaming thread to finish
 *     (polls a thread-active flag with Sleep(100) between retries).
 *  4. Save sound data (FUN_004394e0) then destroy the sound-data object.
 *  5. Destroy ALL subsystem objects via vtable slot 0 (flag=1 = delete this).
 *  6. Kill the multimedia timer: timeKillEvent(g_timerID), timeEndPeriod(14).
 *  7. Close the "GameLoop" event handle: CloseHandle(g_hGameLoopEvent).
 *  8. Run additional cleanup for world, bitmap, font, and misc subsystems.
 *
 * WIN32: Sleep, CloseHandle, timeKillEvent, timeEndPeriod.
 * LINUX: usleep(100000) for Sleep(100).
 *        pthread_cancel / pthread_join for the background thread.
 *        timer_delete(g_timerId) instead of timeKillEvent.
 *        sem_destroy(&g_frameSem) instead of CloseHandle.
 */
void CGWND_Shutdown(void)
{
    /* 1. Persist window rect to INI */
    /* FUN_00452db0(g_pIniFile, "WINDOW_ATTRIBUTES", "RectLeft",   g_windowRect.left);   */
    /* FUN_00452db0(g_pIniFile, "WINDOW_ATTRIBUTES", "RectTop",    g_windowRect.top);    */
    /* FUN_00452db0(g_pIniFile, "WINDOW_ATTRIBUTES", "RectRight",  g_windowRect.right);  */
    /* FUN_00452db0(g_pIniFile, "WINDOW_ATTRIBUTES", "RectBottom", g_windowRect.bottom); */

    /* 2. Mark clean exit in INI */
    /* FUN_00452db0(g_pIniFile, "PROCESS", "CleanExit", 1); */

    /* 3. Wait for background thread to exit.
     * WIN32: Sleep(100).  LINUX: usleep(100000). */
    while (/* FUN_00461710(g_bgThreadState) != 0 */ 0) {
        Sleep(100);
    }

    /* 4. Save state and destroy sound data object */
    /* if (g_pSoundData) { FUN_004394e0(g_pSoundDataAux); (*g_pSoundData->vtable[0])(1); } */

/* 5. Destroy all subsystems in reverse creation order */
#define DESTROY(ptr) \
    do { \
        if (ptr) { \
            (*(void(**)(int))(ptr))(1); \
            (ptr) = NULL; \
        } \
    } while (0)

    DESTROY(g_pDirectDraw);
    DESTROY(g_pDirectSound);
    DESTROY(g_pInputMgr);
    DESTROY(g_pMovieMgr);
    DESTROY(g_pNetworkMgr);
    DESTROY(g_pAnimMgr);
    DESTROY(g_pSceneMgr);
    DESTROY(g_pWorldMgr);
    DESTROY(g_pTimerSvc);
    DESTROY(g_pConfigMgr);
    DESTROY(g_pEventQueue);
    DESTROY(g_pStringTable);
    DESTROY(g_pDebugLog);
    DESTROY(g_pSaveGameMgr);
    DESTROY(g_pIniFile);
#undef DESTROY

    /* 6. Kill multimedia timer
     * WIN32: timeKillEvent, timeEndPeriod
     * LINUX: timer_delete(g_timerId) */
    if (g_timerID != 0) {
        timeKillEvent(g_timerID);
        g_timerID = 0;
    }
    timeEndPeriod(14);

    /* 7. Close "GameLoop" event
     * WIN32: CloseHandle
     * LINUX: sem_destroy(&g_frameSem) */
    if (g_hGameLoopEvent) {
        CloseHandle(g_hGameLoopEvent);
        g_hGameLoopEvent = NULL;
    }

    /* 8. Additional cleanup (world, bitmap cache, fonts, misc) */
    /* FUN_0042a5f0();  FUN_0042cdd0(&g_worldData);  etc. */
}


/* =========================================================================
 * CWnd_CreateChildWindow — 0x00413de0
 * =========================================================================
 *
 * Generic sub-window creation helper.  Used by subsystem UI objects
 * (e.g. the scene renderer, the UI overlay) to create child windows
 * that host DirectDraw surfaces or receive input events.
 *
 * Each subsystem object that calls this has a per-instance class name
 * stored at self+0xA8 (a unique string like "LegoScene0") so that
 * multiple instances can coexist.
 *
 * WIN32: GetWindowTextA, RegisterClassA, GetLastError, FormatMessageA,
 *        LocalFree, CreateWindowExA, ShowWindow, UpdateWindow.
 * LINUX: Replace with SDL_CreateWindow or an SDL sub-surface / texture.
 *        Per-subsystem "windows" may collapse into SDL_Rect viewports.
 */
UINT CWnd_CreateChildWindow(void *self, int nCmdShow, HWND hwndParent,
                             int x, int y, int cx, int cy,
                             HMENU hMenu, HICON hIcon, UINT classStyle)
{
    WNDCLASSA   wc;
    HWND        hwndNew;
    CHAR        szParentTitle[256];
    ATOM        atom;
    DWORD       dwErr;
    HLOCAL      hErrMsg;

    memset(&wc, 0, sizeof(wc));

    /* Read parent window title for debugging / class-name generation */
    /* WIN32: GetWindowTextA.  LINUX: SDL_GetWindowTitle. */
    GetWindowTextA(hwndParent, szParentTitle, sizeof(szParentTitle));

    /* Store parent HWND and geometry in the object */
    *(HWND *)((char *)self + 0x0C) = hwndParent;
    *(int  *)((char *)self + 0xDC) = x;
    *(int  *)((char *)self + 0xE0) = y;
    *(int  *)((char *)self + 0xE4) = cx;
    *(int  *)((char *)self + 0xE8) = cy;

    /* Register per-instance window class */
    wc.style        = classStyle ? classStyle : CS_OWNDC | CS_DBLCLKS;
    wc.hInstance    = *(HINSTANCE *)((char *)self + 4);
    wc.lpfnWndProc  = (WNDPROC)&LAB_00415900;  /* subsystem WndProc */
    wc.hIcon        = hIcon;
    wc.lpszClassName= (LPCSTR)((char *)self + 0xA8); /* per-instance name */

    atom = RegisterClassA(&wc);
    if (atom == 0) {
        /* Log error — non-fatal (class may already be registered) */
        dwErr = GetLastError();
        if (dwErr != 0) {
            /* WIN32: FormatMessageA + LocalFree for diagnostic string.
             * LINUX: strerror(errno) or SDL_GetError(). */
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                           FORMAT_MESSAGE_FROM_SYSTEM,
                           NULL, dwErr,
                           MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                           (LPSTR)&hErrMsg, 0, NULL);
            LocalFree(hErrMsg);
        }
    }

    /*
     * Create child window.
     * Style 0x86000000 = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_CHILD | WS_VISIBLE.
     * WIN32: CreateWindowExA.
     * LINUX: This concept maps to an SDL_Rect viewport, not a real child window.
     *        Store the rect and render into it with SDL_RenderSetViewport.
     */
    hwndNew = CreateWindowExA(0,
                              (LPCSTR)((char *)self + 0xA8),
                              szParentTitle,
                              0x86000000,
                              x, y, cx, cy,
                              hwndParent,
                              hMenu,
                              *(HINSTANCE *)((char *)self + 4),
                              self);    /* pass object as lpCreateParam */

    *(HWND *)((char *)self + 8) = hwndNew;

    if (hwndNew == NULL) {
        /* Log creation error */
        dwErr = GetLastError();
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL, dwErr,
                       MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                       (LPSTR)&hErrMsg, 0, NULL);
        LocalFree(hErrMsg);
        return 0;   /* failure */
    }

    /* Mark window-created flag at self+0xDB */
    *(BYTE *)((char *)self + 0xDB) = 1;

    /* Call virtual OnCreate (vtable slot 6, offset +0x18) */
    (*(void(**)(void))(*(void ***)self)[6])();

    /* Post-create layout setup */
    /* FUN_00414130(self); */

    /* If no DirectDraw surface yet, allocate one via DirectDraw manager */
    if (*(int *)((char *)self + 0x38) == 0) {
        /* Request a surface from the DD manager at g_pDirectDraw->vtable[6]
         * specifying width=cx, height=cy, format=0x7c, pitch=0x840, depth=7 */
        /* (*g_pDirectDraw->vtable[6])(g_pDirectDraw, surfaceParams, &self->surface, 0); */

        /* Store surface geometry */
        *(int *)((char *)self + 0x18) = x;
        *(int *)((char *)self + 0x1C) = y;
        *(int *)((char *)self + 0x20) = x + cx;
        *(int *)((char *)self + 0x24) = y + cy;
        *(int *)((char *)self + 0x30) = cx;
        *(int *)((char *)self + 0x34) = cy;
    }

    ShowWindow(hwndNew, nCmdShow);
    UpdateWindow(hwndNew);

    *(BYTE *)((char *)self + 0x88) = 0;  /* reset a state flag */
    return 1;   /* success */
}
