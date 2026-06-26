/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * src/core/loco_types.h — Common types, structs, and constants
 *
 * This file documents the core data structures recovered from loco.exe.
 * All addresses refer to the original PE32 image base at 0x00400000.
 */

#ifndef LOCO_TYPES_H
#define LOCO_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* =========================================================================
 * Platform abstraction
 * ========================================================================= */

#ifdef LOCO_LINUX
  /* SDL2 replaces Win32 window/surface types */
  #include <SDL2/SDL.h>
  typedef SDL_Window     *HWND;
  typedef SDL_Renderer   *HDC;       /* approximate mapping */
  typedef void           *HINSTANCE;
  typedef void           *HANDLE;
  typedef uint32_t        DWORD;
  typedef uint16_t        WORD;
  typedef uint8_t         BYTE;
  typedef int             BOOL;
  typedef char           *LPSTR;
  typedef const char     *LPCSTR;
  typedef struct { int left, top, right, bottom; } RECT;
  #define TRUE  1
  #define FALSE 0
#else
  #include <windows.h>
  #include <ddraw.h>
  #include <dsound.h>
#endif

/* =========================================================================
 * Game state machine values (DAT_004851f4)
 * ========================================================================= */

typedef enum {
    GAME_STATE_INIT      = 1,   /* Initialising / resetting world */
    GAME_STATE_LOADING   = 2,   /* Async asset load in progress */
    GAME_STATE_RUNNING   = 3,   /* Normal gameplay */
    GAME_STATE_PAUSED    = 4,   /* Game paused */
    GAME_STATE_MENU_A    = 5,   /* Main menu (DirectSound path) */
    GAME_STATE_MENU_B    = 6,   /* Main menu (Input path) */
    GAME_STATE_MOVIE     = 7,   /* FMV playback */
    GAME_STATE_SAVE      = 8,   /* Save-game screen */
    GAME_STATE_CREDITS   = 9,   /* Credits / end sequence */
    GAME_STATE_QUIT      = 10,  /* Posts SDL_QUIT, triggers teardown */
} GameState;

/* =========================================================================
 * CGWND — Main engine root object (0x28 bytes)
 * Constructed at 0x004061e0, singleton at DAT_004aa4a0
 *
 * All Win32 HWND/HINSTANCE fields become SDL2 equivalents on Linux.
 * ========================================================================= */

typedef struct CGWND {
    void        *vtable;            /* +0x00  vtable at PTR_FUN_004774c4 */
    HWND         hwndDesktop;       /* +0x04  WIN32: GetDesktopWindow()   LINUX: NULL */
    HWND         hwndGame;          /* +0x08  WIN32: main WS_POPUP window  LINUX: SDL_Window* */
    HINSTANCE    hInstance;         /* +0x0C  WIN32: HINSTANCE             LINUX: NULL */
    uint8_t      stateFlag;         /* +0x10  internal init flag */
    uint8_t      minVehicleFPS;     /* +0x11  INI BALANCING/MinVehicleFPS  default 20 */
    uint8_t      minBuildingFPS;    /* +0x12  INI BALANCING/MinBuildingFPS default 18 */
    uint8_t      minMinifigFPS;     /* +0x13  INI BALANCING/MinMinifigFPS  default 16 */
    uint8_t      minFlyingFPS;      /* +0x14  INI BALANCING/MinFlyingFPS   default 14 */
    uint8_t      _pad[3];
    uint32_t     versionMajor;      /* +0x18 */
    uint32_t     versionMinor;      /* +0x1C */
    uint32_t     versionBuild;      /* +0x20 */
    uint32_t     versionRevision;   /* +0x24 */
} CGWND;

/* =========================================================================
 * LOCOBITMAP — DirectDraw offscreen surface wrapper
 * Surfaces are identified by 16-bit resource IDs via the resource manager.
 *
 * WIN32: IDirectDrawSurface* (from DDRAW.DLL)
 * LINUX: SDL_Surface* + SDL_Texture*
 * ========================================================================= */

typedef struct LOCOBITMAP {
    void        *vtable;            /* +0x00  C++ vtable */
    void        *pDDSurface;        /* +0x04  WIN32: IDirectDrawSurface*  LINUX: SDL_Texture* */
    int32_t      width;             /* +0x08  surface width in pixels */
    int32_t      height;            /* +0x0C  surface height in pixels */
    uint32_t     colorKey;          /* +0x10  magenta: RGB(255,0,255) */
    uint32_t     flags;             /* +0x14  creation flags (DDSCAPS_*) */
    /* Additional fields undetermined */
} LOCOBITMAP;

/* Magenta color key used throughout for sprite transparency */
#define LOCO_COLOR_KEY_R  255
#define LOCO_COLOR_KEY_G  0
#define LOCO_COLOR_KEY_B  255

/* =========================================================================
 * Resource IDs (16-bit logical IDs used in FUN_00446ea0 lookups)
 * ========================================================================= */

#define RESOURCE_ID_CURSOR_SURFACE   0x1400   /* 256x256 animated cursor */
#define RESOURCE_ID_CURSOR_HOVER     0x1402
#define RESOURCE_ID_CURSOR_GRAB      0x1403

/* Resource type ranges (see resources.c for full table) */
#define RESOURCE_TYPE_GENERIC_START  0x0400
#define RESOURCE_TYPE_SURFACE_START  0x0800
#define RESOURCE_TYPE_ANIM_START     0x1C00
#define RESOURCE_TYPE_BUTTON_START   0x5000

/* =========================================================================
 * RFH Resource file entry (in-memory node, 16 bytes)
 * ========================================================================= */

/* Verified from RFHMGR_Load decompile at 0x0045caa0 */
typedef struct RFHEntry {
    char            *filename;      /* +0x00  heap copy of backslash path e.g. "roads\half.dat" */
    uint32_t         flags;         /* +0x04  0x00=normal, 0x01=packed/custom compressed */
    uint32_t         rfdSize;       /* +0x08  byte size of asset in .RFD file */
    struct RFHEntry *next;          /* +0x0C  linked list; NULL = end */
} RFHEntry;
/* Note: RFD offset is NOT in RFHEntry. Computed by accumulating rfdSize values. */

typedef struct CRFHFile {
    RFHEntry    *entryHead;         /* +0x00  linked list head */
    void        *rfdHandle;         /* +0x04  FILE* to the .RFD data file */
    uint32_t     entryCount;        /* +0x08  number of entries loaded */
} CRFHFile;

/* =========================================================================
 * DirectPlay / NETMAN player slot (approximate, ~76 bytes each)
 * 9 slots at NETMAN+0x518, stride 0x4C
 * ========================================================================= */

typedef struct NetPlayerSlot {
    int32_t     playerId;           /* +0x00  DirectPlay player ID */
    char        username[32];       /* +0x04  player display name */
    uint8_t     isConnected;        /* +0x24  1 = active player */
    uint8_t     _pad[0x27];
} NetPlayerSlot;

/* =========================================================================
 * Global engine singletons (for Linux port: direct variables instead of
 * Win32 global segment addresses)
 * ========================================================================= */

/* Original address -> Linux global name */
/* DAT_004aa4a0 */ extern CGWND        *g_pGameWnd;
/* DAT_004851f4 */ extern int           g_gameState;
/* DAT_00485444 */ extern uint8_t       g_timerFired;
/* DAT_004a990c */ extern void         *g_hGameLoopEvent;   /* WIN32: HANDLE, LINUX: sem_t */
/* DAT_00485438 */ extern uint32_t      g_timerID;          /* WIN32: UINT (multimedia timer) */
/* DAT_004855e8 */ extern void         *g_pResourceMgr;     /* CResourceMgr */
/* DAT_00485440 */ extern void         *g_pDirectDraw;      /* WIN32: IDirectDraw*, LINUX: SDL_Renderer* */
/* DAT_004fd3bc */ extern void         *g_pDirectSound;     /* WIN32: IDirectSound*, LINUX: Mix context */
/* DAT_004aa4a8 */ extern void         *g_pAudioMgr;        /* CAudioMgr, 0x124 bytes */
/* DAT_004fd3ac */ extern void         *g_pNetworkMgr;      /* CNetworkManager */
/* DAT_004fd394 */ extern void         *g_pInputMgr;
/* DAT_004a9eec */ extern void         *g_pIniFile;         /* CIniFile */
/* DAT_004a9918 */ extern int           g_debugMode;        /* 0=normal, 1=developer */

/* Screen dimensions at 0x004851d8 / 0x00485214 */
extern int g_screenWidth;   /* default 640 */
extern int g_screenHeight;  /* default 480 */

#endif /* LOCO_TYPES_H */
