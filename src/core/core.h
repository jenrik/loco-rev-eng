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

#ifndef LOCO_CORE_H
#define LOCO_CORE_H

/*
 * Platform abstraction layer.
 *
 * WIN32: include the real Windows headers.
 * LINUX: provide minimal type stubs so the source can be studied and
 *        incrementally ported without a Windows toolchain.
 *
 * To build the Linux port, define LOCO_LINUX and supply SDL2 headers
 * alongside a loco_platform.h that maps the WIN32 types below to their
 * SDL2 / POSIX counterparts.
 */

#ifndef LOCO_LINUX

/* WIN32 — pull in the real Win32 SDK headers */
#include <windows.h>
#include <mmsystem.h>   /* timeSetEvent / timeKillEvent / MMRESULT */
#include <ole2.h>       /* CoInitializeEx / CoUninitialize           */

#else /* LOCO_LINUX */

/*
 * LINUX — minimal type stubs for cross-compilation study.
 * Replace each type with its SDL2 / POSIX equivalent when porting.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Basic Win32 scalar types */
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned int    DWORD;
typedef int             BOOL;
typedef unsigned int    UINT;
typedef long            LONG;
typedef unsigned long   ULONG;
typedef void           *LPVOID;
typedef char           *LPSTR;
typedef const char     *LPCSTR;
typedef void           *HANDLE;
typedef void           *HINSTANCE;
typedef void           *HMODULE;
typedef void           *HMENU;
typedef void           *HICON;
typedef void           *HBRUSH;
typedef void           *HCURSOR;
typedef void           *HKEY;
typedef void           *HLOCAL;
typedef int             LSTATUS;
typedef uintptr_t       UINT_PTR;
typedef uintptr_t       ULONG_PTR;
typedef uintptr_t       DWORD_PTR;
typedef uintptr_t       WPARAM;
typedef intptr_t        LPARAM;
typedef intptr_t        LRESULT;
typedef unsigned short  ATOM;
typedef unsigned int    MMRESULT;

/*
 * LINUX: HWND maps to SDL_Window*.
 * Defined as void* here so headers compile without SDL2 present.
 */
typedef void           *HWND;

/* Common constants */
#define TRUE    1
#define FALSE   0
#define NULL    ((void*)0)

/* Win32 RECT — map to SDL_Rect for the Linux port */
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *LPRECT;

/* Win32 message structure */
typedef struct tagMSG {
    HWND    hwnd;
    UINT    message;
    WPARAM  wParam;
    LPARAM  lParam;
    DWORD   time;
    /* POINT pt; */
} MSG, *LPMSG;

/* STARTUPINFOA stub */
typedef struct _STARTUPINFOA {
    DWORD  cb;
    LPSTR  lpReserved;
    LPSTR  lpDesktop;
    LPSTR  lpTitle;
    DWORD  dwX;
    DWORD  dwY;
    DWORD  dwXSize;
    DWORD  dwYSize;
    DWORD  dwXCountChars;
    DWORD  dwYCountChars;
    DWORD  dwFillAttribute;
    DWORD  dwFlags;
    WORD   wShowWindow;
    WORD   cbReserved2;
    BYTE  *lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOA;

/* WNDCLASSA stub */
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct tagWNDCLASSA {
    UINT      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR    lpszMenuName;
    LPCSTR    lpszClassName;
} WNDCLASSA;

/* Common Win32 constants */
#define WM_QUIT         0x0012
#define WM_CLOSE        0x0010
#define SW_SHOWNORMAL   1
#define PM_NOREMOVE     0x0000
#define PM_REMOVE       0x0001
#define MB_ICONEXCLAMATION  0x00000030L
#define SM_CXSCREEN     0
#define SM_CYSCREEN     1
#define CS_VREDRAW      0x0001
#define CS_HREDRAW      0x0002
#define CS_DBLCLKS      0x0008
#define CS_OWNDC        0x0020
#define QS_ALLINPUT     0x04FF
#define KEY_READ        0x20019
#define KEY_ALL_ACCESS  0xF003F
#define REG_SZ          1
#define ERROR_SUCCESS   0
#define TIME_PERIODIC   0x0001
#define TIMERR_NOERROR  0
#define BITSPIXEL       12
#define PLANES          14
#define FORMAT_MESSAGE_ALLOCATE_BUFFER  0x00000100
#define FORMAT_MESSAGE_FROM_SYSTEM      0x00001000
#define MAKELANGID(p,s) ((((WORD)(s))<<10)|(WORD)(p))
#define LANG_ENGLISH    0x09
#define SUBLANG_DEFAULT 0x01

/* LINUX: replace with sem_t or pthread_cond_t */
/* #include <semaphore.h> */
/* typedef sem_t LOCO_FRAME_SEM; */

typedef void (CALLBACK *LPTIMECALLBACK)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
#define CALLBACK

#endif /* LOCO_LINUX */

/* =========================================================================
 * Engine state constants  (g_gameState / DAT_004851f4)
 * =========================================================================
 */
#define GAME_STATE_INIT     1   /* initialising / resetting world              */
#define GAME_STATE_LOADING  2   /* async asset load in progress                */
#define GAME_STATE_RUNNING  3   /* normal gameplay                             */
#define GAME_STATE_PAUSED   4   /* game paused                                 */
#define GAME_STATE_MENU_A   5   /* main menu variant A (DirectSound mode)      */
#define GAME_STATE_MENU_B   6   /* main menu variant B (Input mode)            */
#define GAME_STATE_MOVIE    7   /* movie / FMV playback                        */
#define GAME_STATE_SAVE     8   /* save-game screen                            */
#define GAME_STATE_CREDITS  9   /* credits / end sequence                      */
#define GAME_STATE_QUIT    10   /* quit transition — posts WM_CLOSE            */

/* =========================================================================
 * Season / theme override constants  (g_seasonOverride / DAT_00485230)
 * =========================================================================
 */
#define SEASON_DEFAULT    0   /* normal / no override                          */
#define SEASON_EASTER     1   /* -Easter command-line flag                     */
#define SEASON_DESERT     2   /* -Desert command-line flag                     */
#define SEASON_HALLOWEEN  3   /* -Halloween command-line flag                  */
#define SEASON_WINTER     4   /* -Winter command-line flag                     */
#define SEASON_UNKNOWN    5   /* unrecognised seasonal token                   */

/* =========================================================================
 * CGWND — root engine object
 * =========================================================================
 *
 * Allocated as 0x28 (40) bytes on the heap; constructed by
 * CGWND_Constructor (0x004061e0).  Holds the Win32 window handles, the
 * module instance, a set of per-class FPS thresholds, and the four fields
 * of the parsed EXE file-version resource.
 *
 * Layout (all offsets relative to the object base):
 *
 *   +0x00  void**     vtable          points to PTR_FUN_004774c4
 *   +0x04  HWND       hwndDesktop     GetDesktopWindow() result
 *   +0x08  HWND       hwndGame        main WS_POPUP window (set later)
 *   +0x0C  HINSTANCE  hInstance       module handle from WinMain
 *   +0x10  BYTE       stateFlag       cleared in constructor
 *   +0x11  BYTE       minVehicleFPS   INI BALANCING/MinVehicleFPS  (def 20)
 *   +0x12  BYTE       minBuildingFPS  INI BALANCING/MinBuildingFPS (def 18)
 *   +0x13  BYTE       minMinifigFPS   INI BALANCING/MinMinifigFPS  (def 16)
 *   +0x14  BYTE       minFlyingFPS    INI BALANCING/MinFlyingFPS   (def 14)
 *   +0x15..+0x17      (3 bytes padding / alignment)
 *   +0x18  DWORD      versionMajor    "1" from "1.0.0.0"
 *   +0x1C  DWORD      versionMinor
 *   +0x20  DWORD      versionBuild
 *   +0x24  DWORD      versionRevision
 *
 * LINUX: Replace HWND with SDL_Window*; HINSTANCE with void* or omit.
 *        The vtable pattern can be kept as-is or replaced with a plain
 *        struct of function pointers.
 */
typedef struct CGWND {
    void     **vtable;           /* +0x00  PTR_FUN_004774c4                    */
    HWND       hwndDesktop;      /* +0x04  GetDesktopWindow() result           */
    HWND       hwndGame;         /* +0x08  main WS_POPUP game window           */
    HINSTANCE  hInstance;        /* +0x0C  module handle from WinMain          */
    BYTE       stateFlag;        /* +0x10  cleared in constructor              */
    BYTE       minVehicleFPS;    /* +0x11  INI BALANCING/MinVehicleFPS         */
    BYTE       minBuildingFPS;   /* +0x12  INI BALANCING/MinBuildingFPS        */
    BYTE       minMinifigFPS;    /* +0x13  INI BALANCING/MinMinifigFPS         */
    BYTE       minFlyingFPS;     /* +0x14  INI BALANCING/MinFlyingFPS          */
    BYTE       _pad[3];          /* +0x15..+0x17  alignment padding            */
    DWORD      versionMajor;     /* +0x18  first  dot-field of FileVersion     */
    DWORD      versionMinor;     /* +0x1C  second dot-field                    */
    DWORD      versionBuild;     /* +0x20  third  dot-field                    */
    DWORD      versionRevision;  /* +0x24  fourth dot-field                    */
} CGWND;

/* =========================================================================
 * Global engine singletons and state variables
 * =========================================================================
 *
 * All globals are defined in core.c; other translation units use extern.
 */

/*
 * Engine root object singleton.  Set by WinMain after CGWND_Constructor,
 * cleared to NULL before CoUninitialize on every exit path.
 * Original address: DAT_004aa4a0
 */
extern CGWND  *g_pGameWnd;

/*
 * Engine state machine.  Written only through SetGameState().
 * See GAME_STATE_* constants above.
 * Original address: DAT_004851f4
 * LINUX: no platform dependency — plain int.
 */
extern int     g_gameState;

/*
 * Set to 1 by the multimedia timer callback at 0x45c520 each 28ms tick.
 * Cleared at the top of GameFrame_Update.
 * Original address: DAT_00485444
 * LINUX: written by the timer thread; use volatile or atomic_flag.
 */
extern BYTE    g_timerFired;

/*
 * Handle to the named manual-reset event "GameLoop".
 * Created by GameLoop_Setup; the main loop ResetEvent()s it after each frame.
 * Original address: DAT_004a990c
 * LINUX: replace with sem_t or pthread_cond_t + pthread_mutex_t.
 */
extern HANDLE  g_hGameLoopEvent;

/*
 * Multimedia timer ID returned by timeSetEvent.
 * Used by CGWND_Shutdown to call timeKillEvent.
 * Original address: DAT_00485438
 * LINUX: replace with timer_t (POSIX interval timer) or a thread ID.
 */
extern UINT    g_timerID;

/*
 * Season / theme override set by CGWND_ParseCommandLine.
 * 0=default, 1=Easter, 2=Desert, 3=Halloween, 4=Winter, 5=unknown.
 * Original address: DAT_00485230
 */
extern int     g_seasonOverride;

/*
 * Debug / help mode flag.  Set to 1 by -h / -? / -help on the command line.
 * Suppresses the duplicate-instance FindWindow check and creates the window
 * in windowed mode.
 * Original address: DAT_004a9918
 */
extern int     g_debugMode;

/*
 * Primary display width in pixels, populated by CGWND_SetupDisplay.
 * Original address: DAT_004851d8
 * LINUX: populate from SDL_GetCurrentDisplayMode or SDL_GetDisplayBounds.
 */
extern int     g_screenWidth;

/*
 * Primary display height in pixels, populated by CGWND_SetupDisplay.
 * Original address: DAT_00485214
 */
extern int     g_screenHeight;

/*
 * Pointer to the CIniFile object wrapping lego.ini.
 * Set by CGWND_LoadConfig, destroyed by CGWND_Shutdown.
 * Original address: DAT_004a9eec
 * LINUX: replace with a plain key-value map or a minIni/inih context.
 */
extern void   *g_pIniFile;

/*
 * Subsystem singleton pointers.
 * Each pointer is the first DWORD of an object whose first field is a vtable.
 * Vtable slot 0 is the destructor: (*vtable[0])(1) destroys and frees.
 * Vtable slot 2 is an Activate / Init method used by SetGameState.
 *
 * Original addresses listed per field.
 */
extern void   *g_pDirectDraw;    /* DAT_004fd378 — CDirectDrawManager  (0x224 bytes) */
extern void   *g_pDirectSound;   /* DAT_004fd37c — CDirectSoundManager (0x6e0 bytes) */
extern void   *g_pInputMgr;      /* DAT_004fd384 — CInputManager       (0x254 bytes) */
extern void   *g_pMovieMgr;      /* DAT_004fd380 — CMoviePlayer        (0x740 bytes) */
extern void   *g_pNetworkMgr;    /* DAT_004fd388 — CNetworkManager     (0x2c4 bytes) */
extern void   *g_pSceneMgr;      /* DAT_004fd38c — CSceneManager       (0x3078 bytes)*/
extern void   *g_pWorldMgr;      /* DAT_004fd390 — CWorldManager       (0x1184 bytes)*/
extern void   *g_pAnimMgr;       /* DAT_00485258 — CAnimManager        (0x1d4 bytes) */
extern void   *g_pTimerSvc;      /* DAT_004fd394 — CTimerService       (0x1c bytes)  */
extern void   *g_pConfigMgr;     /* DAT_004fd3a8 — CConfigManager      (0xb0 bytes)  */
extern void   *g_pEventQueue;    /* DAT_004fd3ac — CEventQueue         (0x804 bytes) */
extern void   *g_pStringTable;   /* DAT_004fd3b0 — CStringTable        (0xbe4 bytes) */
extern void   *g_pDebugLog;      /* DAT_004fd3b4 — CDebugLog           (0x18 bytes)  */
extern void   *g_pSaveGameMgr;   /* DAT_004aa4a8 — CSaveGameManager    (0x124 bytes) */
extern void   *g_pThumbnailMgr;  /* DAT_004ff110 — CThumbnailMgr       (0x20 bytes)  */

/* =========================================================================
 * Function declarations
 * =========================================================================
 */

/*
 * entry — 0x004689e0
 *
 * MSVC CRT startup stub (WinMainCRTStartup equivalent).  Parses the raw
 * command line, calls WinMain, then calls ExitProcess.
 *
 * WIN32: Entry point for /SUBSYSTEM:WINDOWS executables.
 * LINUX: Replace entirely with standard main(int argc, char **argv).
 */
void entry(void);

/*
 * WinMain — 0x00462e90
 *
 * Real application entry point.  Creates the splash dialog, constructs
 * the CGWND singleton, initialises COM, loads config, validates display,
 * runs the two-phase message loop, and tears everything down on exit.
 *
 * Parameters:
 *   hInstance  — module handle (from GetModuleHandleA in entry())
 *   unused     — hPrevInstance, always 0 on Win32
 *   lpCmdLine  — command-line tail (after exe name); may be empty
 *
 * Returns:
 *   0   = clean exit
 *  -1   = init failure (LoadConfig)
 *  -2   = CGWND construction failed
 *   1   = duplicate instance detected
 *
 * WIN32: Uses Win32 dialog, window, message-loop, and COM APIs.
 * LINUX: Replace with SDL2 event loop; remove COM calls.
 */
WPARAM WinMain(HINSTANCE hInstance, UINT unused, BYTE *lpCmdLine);

/*
 * CGWND_Constructor — 0x004061e0
 *
 * C++ __thiscall constructor for the CGWND engine root object.
 * Installs the vtable, zeroes all fields, obtains the desktop HWND,
 * initialises global state to GAME_STATE_INIT, and reads the EXE
 * file-version resource into the four version fields.
 *
 * Parameters:
 *   self      — pre-allocated 0x28-byte buffer
 *   hInstance — Win32 module handle stored at self->hInstance
 *
 * Returns:
 *   self pointer (C++ constructor convention)
 *
 * WIN32: GetDesktopWindow, SetRect
 * LINUX: SDL_GetDisplayBounds(0, ...) for desktop geometry
 */
CGWND *CGWND_Constructor(CGWND *self, HINSTANCE hInstance);

/*
 * CGWND_SetVisible — 0x004061b0
 *
 * Stores bVisible at self+0x24 and calls the virtual OnShow/OnHide
 * method (vtable slot 1).  Also toggles an optional secondary surface
 * attached at self+0x48.
 *
 * Parameters:
 *   self     — CGWND or compatible object
 *   bVisible — non-zero to show, zero to hide
 *
 * WIN32: Virtual dispatch; no direct Win32 API.
 * LINUX: SDL_ShowWindow / SDL_HideWindow
 */
void CGWND_SetVisible(CGWND *self, char bVisible);

/*
 * CGWND_ReadVersionInfo — 0x004062e0
 *
 * Reads the FileVersion string from the running EXE's VS_VERSIONINFO
 * resource and tokenises it on '.' to populate the four DWORD version
 * fields of self (offsets +0x18 .. +0x24).
 *
 * Parameters:
 *   self — CGWND object; version fields written directly
 *
 * WIN32: GetModuleHandleA, GetModuleFileNameA,
 *        GetFileVersionInfoSizeA, GetFileVersionInfoA, VerQueryValueA
 * LINUX: sscanf from a VERSION file, or a compile-time macro
 */
void CGWND_ReadVersionInfo(CGWND *self);

/*
 * CGWND_SetupDisplay — 0x00406480
 *
 * Queries the primary screen dimensions into g_screenWidth / g_screenHeight,
 * reads window geometry from the INI [WINDOW_ATTRIBUTES] section, and reads
 * per-class FPS thresholds from [BALANCING] into self->minVehicleFPS etc.
 * Also reads the PROCESS/CleanExit flag to detect previous crashes.
 *
 * Parameters:
 *   self — CGWND object
 *
 * WIN32: GetDesktopWindow, GetSystemMetrics, SetRect
 * LINUX: SDL_GetCurrentDisplayMode; hand-rolled INI reader
 */
void CGWND_SetupDisplay(CGWND *self);

/*
 * CGWND_CheckDisplayCaps — 0x00406680
 *
 * Validates the display colour depth (must be true-colour, not palettised)
 * and screen width (must exceed 800px for 1024×768+ mode).
 * Displays a MessageBoxA with a localised error string on failure.
 *
 * Parameters:
 *   self — CGWND object (used for the desktop HWND)
 *
 * Returns:
 *   Non-zero on success (low byte = 1)
 *   0 on failure
 *
 * WIN32: GetDC, GetDeviceCaps, ReleaseDC, GetSystemMetrics, MessageBoxA
 * LINUX: SDL_GetCurrentDisplayMode; SDL_ShowSimpleMessageBox
 */
UINT CGWND_CheckDisplayCaps(CGWND *self);

/*
 * CGWND_ParseCommandLine — 0x00406790
 *
 * Tokenises the raw command-line tail on spaces and recognises season
 * override keywords and help/debug flags.  Sets g_debugMode and
 * g_seasonOverride accordingly.
 *
 * Parameters:
 *   lpCmdLine — pointer to the command-line tail (after exe name);
 *               string is modified in-place by strtok
 *
 * WIN32: No Win32 APIs; pure string processing.
 * LINUX: Use strtok_r to avoid thread-safety issues.
 */
void CGWND_ParseCommandLine(BYTE *lpCmdLine);

/*
 * CGWND_LoadConfig — 0x004068d0
 *
 * Locates lego.ini via HKLM\SOFTWARE\Intelligent Games\LEGO LOCO,
 * constructs a CIniFile object, and reads the local / remote resource
 * directory paths from [DIRECTORIES].
 *
 * Returns:
 *   TRUE  — local resource directory found and accessible
 *   FALSE — directory missing or path too short
 *
 * WIN32: RegOpenKeyExA, RegQueryValueExA, RegCloseKey,
 *        RegCreateKeyExA, RegSetValueExA
 * LINUX: Derive path from $LEGO_LOCO_HOME, XDG_DATA_DIRS, or
 *        readlink("/proc/self/exe"); use plain fopen + line parser
 */
BOOL CGWND_LoadConfig(void);

/*
 * CGWND_RegisterAndCreateWindow — 0x00406ed0
 *
 * Registers the "LEGO LOCO" window class and creates the fullscreen
 * WS_POPUP game window.  Stores the HWND in self->hwndGame and
 * records the client rect in DAT_00485220.
 *
 * Parameters:
 *   self — CGWND object
 *
 * Returns:
 *   TRUE on success, FALSE if CreateWindowExA fails
 *
 * WIN32: RegisterClassA, CreateWindowExA, LoadIconA, GetClientRect
 * LINUX: SDL_CreateWindow(SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS)
 */
BOOL CGWND_RegisterAndCreateWindow(CGWND *self);

/*
 * CGWND_InitGraphicsSubsystems — 0x00406f90
 *
 * Allocates and initialises the eight core DirectX-layer subsystem objects
 * in dependency order:
 *   CDirectDrawManager, CDirectSoundManager, CNetworkManager,
 *   CAnimManager, CInputManager, CMoviePlayer, CSceneManager, CWorldManager.
 * On any failure, already-created objects are destroyed in reverse order.
 *
 * Parameters:
 *   self — CGWND object (provides hInstance and hwndGame)
 *
 * Returns:
 *   0   on full success
 *  <0   negative error code identifying the failed subsystem
 *
 * WIN32: Indirectly uses DirectDraw, DirectSound, DirectInput, DirectPlay.
 * LINUX: Replace with SDL_Renderer, SDL_mixer, SDL input events, BSD sockets.
 */
int CGWND_InitGraphicsSubsystems(CGWND *self);

/*
 * GameLoop_Setup — 0x00406ba0
 *
 * Top-level initialisation: creates all utility and graphics subsystems,
 * registers the game window, loads string tables, starts the multimedia
 * timer (28ms / ~35.7fps), and creates the "GameLoop" synchronisation event.
 *
 * Parameters:
 *   self — CGWND object
 *
 * Returns:
 *   0   on success
 *  -1   on any failure
 *
 * WIN32: CreateEventA, timeBeginPeriod, timeSetEvent
 * LINUX: sem_init + timer_create(CLOCK_MONOTONIC) or SDL_AddTimer
 */
int GameLoop_Setup(CGWND *self);

/*
 * SetGameState — 0x00408130
 *
 * Transitions the engine state machine to newState.  No-ops if newState
 * equals the current state.  Each state triggers specific subsystem
 * activations; state 10 (QUIT) posts WM_CLOSE to the game window.
 *
 * Parameters:
 *   newState — one of the GAME_STATE_* constants
 *
 * WIN32: PostMessageA (state GAME_STATE_QUIT only)
 * LINUX: SDL_PushEvent with SDL_QUIT type
 */
void SetGameState(int newState);

/*
 * GameFrame_Update — 0x0045c3c0
 *
 * Per-frame update, invoked from the main message loop each time the
 * multimedia timer fires.  Clears g_timerFired, then ticks the event
 * queue, network, world renderer, scene, audio, and (for RUNNING/CREDITS
 * states) AI, animation, and building subsystems.  Ends with a frame
 * present / blit call.
 *
 * WIN32: No direct Win32 API calls; all via subsystem vtable methods.
 * LINUX: Replace the DirectDraw flip inside FUN_0044e020 with
 *        SDL_RenderPresent; replace palette calls with SDL_SetPaletteColors.
 */
void GameFrame_Update(void);

/*
 * Thumbnails_Init — 0x0045c8a0
 *
 * Allocates a CThumbnailMgr (0x20 bytes) and loads the thumbnail palette
 * BMP from <LocalResDir>\2\smisc\thumbpal.bmp.
 *
 * Returns:
 *   true  if the manager was allocated successfully
 *   false on allocation failure
 *
 * WIN32: Internal file I/O wrappers (no direct Win32 API).
 * LINUX: SDL_LoadBMP for the palette image.
 */
bool Thumbnails_Init(void);

/*
 * CGWND_Shutdown — 0x004077a0
 *
 * Orderly engine teardown.  Persists window rect and CleanExit flag to
 * lego.ini, waits for background threads, destroys all subsystem objects
 * via their vtable destructors in reverse-creation order, kills the
 * multimedia timer, and closes the GameLoop event handle.
 *
 * WIN32: Sleep, CloseHandle, timeKillEvent, timeEndPeriod
 * LINUX: usleep(100000), pthread_join, timer_delete, sem_destroy
 */
void CGWND_Shutdown(void);

/*
 * CWnd_CreateChildWindow — 0x00413de0
 *
 * Generic child/sub-window creation helper used by subsystem UI objects.
 * Registers a per-instance window class (class name at self+0xA8), creates
 * a WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS window,
 * optionally allocates a DirectDraw surface, then calls ShowWindow /
 * UpdateWindow.
 *
 * Parameters:
 *   self       — subsystem object; stores geometry at fixed offsets
 *   nCmdShow   — initial show state (SW_SHOWNORMAL etc.)
 *   hwndParent — parent window handle
 *   x, y       — window position relative to parent
 *   cx, cy     — window size in pixels
 *   hMenu      — child-window ID or menu handle (may be NULL)
 *   hIcon      — icon for the window class (may be NULL)
 *   classStyle — override for the WNDCLASSA.style field;
 *                0 defaults to CS_OWNDC | CS_DBLCLKS
 *
 * Returns:
 *   1 on success, 0 on CreateWindowExA failure
 *
 * WIN32: GetWindowTextA, RegisterClassA, GetLastError, FormatMessageA,
 *        LocalFree, CreateWindowExA, ShowWindow, UpdateWindow
 * LINUX: Collapse into an SDL_Rect viewport; no real child windows needed.
 *        Use SDL_RenderSetViewport to render subsystems into sub-regions.
 */
UINT CWnd_CreateChildWindow(void *self, int nCmdShow, HWND hwndParent,
                             int x, int y, int cx, int cy,
                             HMENU hMenu, HICON hIcon, UINT classStyle);

#endif /* LOCO_CORE_H */
