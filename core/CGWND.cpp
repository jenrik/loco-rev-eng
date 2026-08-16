/**
 * CGWND.cpp — Main game window class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "CGWND.h"
#include "../game/ScriptedObject.h"
#include "../platform/ddraw_interfaces.h"
#include "../game/PlayerConfig.h"  /* for sizeof(PlayerConfig) */
#include <cstring>
#include <cstdio>
#include <cassert>

/* Subsystem class headers — for InitAllSubsystems and typed dispatch */
#include "../ui/EditWindow.h"
/* EditWindow.h only forward-declares NameEntryPanel/GameSetupPanel (its
 * pPanelA/pPanelB field types); WIN32_PostQuit below needs their complete
 * types to read GameSetupPanel::field_E8 / the inherited
 * UI_WindowBase::visible. */
#include "../ui/NameEntryPanel.h"
#include "../ui/GameSetupPanel.h"
// The host menu bootstrap intentionally stops after EditWindow. The remaining
// original startup chain is retained for the Windows/binary-faithful build.
#ifdef _WIN32
#include "../graphics/LOCOBITMAP.h"
#endif

/* Typed subsystem headers — needed by CGWND_EnterMode3 (0x4086F0) and
 * CGWND_SetMode (0x408130) for typed virtual-method dispatch. Included
 * unconditionally because CGWND_EnterMode3 is not behind _WIN32.
 * Netman.h is excluded (conflicting Config_GetIniInt signature);
 * a forward declaration is used instead. */
#include "../town/Town.h"
#include "../ui/PostcardAlbum.h"
#include "../ui/PostcardPreviewWindow.h"
#include "../ui/TrainStationWindow.h"
#include "../input/Cursor.h"
#include "../input/InputMgr.h"
#include "../ui/HelpWnd.h"
#include "../ui/AboutDialog.h"
#include "../core/Game.h"
// Netman: forward-declared below (Netman.h conflicts with Config_GetIniInt)
class Netman;
#include "../game/BuildingMgr.h"
#include "../game/Panel.h"        /* for g_active_panel->HandleKey (WndProc) */
#include "../world/scriptengine.h" /* for g_scripted_object->EnterBuildMode (WndProc) */
#include "../audio/GameAudio.h"
#include "../game/World.h"
#include "../resources/ResourceManager.h"

#ifndef _WIN32
#include <SDL3/SDL.h>
#include "sdl3_window.h"
#include "sdl3_game_audio.h"
#include "host_test_events.h"
extern "C" { SDL_Window* SDL3_GetWindow(void); }
namespace loco { namespace host { void HostPostLoadWorker(void* param); } }
#endif

/* ================================================================== */
/* Win32 API declarations (Windows only)                               */
/* ================================================================== */

#ifdef _WIN32
/* PAINTSTRUCT: not provided by this project's Windows compatibility
 * headers (stubs/windows.h) — mirrors graphics/sdl3_window.h's own
 * definition so the (MinGW typecheck only, never linked) _WIN32 path
 * has a matching layout for BeginPaint/EndPaint below. */
struct PAINTSTRUCT {
    HDC   hdc;
    BOOL  fErase;
    RECT  rcPaint;
    BOOL  fRestore;
    BOOL  fIncUpdate;
    uint8_t rgbReserved[32];
};

extern "C" {
    /* Windows API */
    HWND  GetDesktopWindow(void);
    HWND  CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
                          DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                          HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
                          void* lpParam);
    ATOM  RegisterClassA(const WNDCLASSA* lpWndClass);
    ATOM  RegisterClassExA(const WNDCLASSEXA* lpWndClassEx);
    BOOL  GetClientRect(HWND hWnd, struct RECT* lpRect);
    HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
    BOOL  ShowWindow(HWND hWnd, int nCmdShow);
    int   GetSystemMetrics(int nIndex);
    HDC   GetDC(HWND hWnd);
    int   GetDeviceCaps(HDC hdc, int index);
    int   ReleaseDC(HWND hWnd, HDC hdc);
    int   MessageBoxA(HWND hWnd, const char* text, const char* caption, uint32_t type);
    HMODULE GetModuleHandleA(const char* name);
    DWORD  GetModuleFileNameA(HMODULE hModule, char* buf, DWORD size);
    DWORD  GetFileVersionInfoSizeA(const char* file, DWORD* handle);
    BOOL   GetFileVersionInfoA(const char* file, DWORD handle, DWORD len, void* data);
    BOOL   VerQueryValueA(void* block, const char* subBlock, void** buffer, uint32_t* len);
    BOOL   PeekMessageA(struct tagMSG* lpMsg, HWND hWnd, uint32_t wMsgFilterMin,
                        uint32_t wMsgFilterMax, uint32_t wRemoveMsg);
    BOOL   TranslateMessage(const struct tagMSG* lpMsg);
    LONG   DispatchMessageA(const struct tagMSG* lpMsg);
    void   SetCursor(void* hCursor);
    HWND   SetFocus(HWND hWnd);
    BOOL   SetForegroundWindow(HWND hWnd);
    void   Sleep(DWORD dwMilliseconds);
    BOOL   DestroyWindow(HWND hWnd);
    void   PostQuitMessage(int nExitCode);
    LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    BOOL   OffsetRect(struct RECT* lprc, int dx, int dy);
    LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    BOOL   GetWindowRect(HWND hWnd, struct RECT* lpRect);
    int    KillTimer(HWND hWnd, uintptr_t uIDEvent);
    uintptr_t SetTimer(HWND hWnd, uintptr_t nIDEvent, uint32_t uElapse,
                       void (*lpTimerFunc)(HWND,uint32_t,uintptr_t,DWORD));
    BOOL   PostMessageA(HWND hWnd, uint32_t Msg, uint32_t wParam, uint32_t lParam);
    LONG   SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong);
    LONG   GetWindowLongA(HWND hWnd, int nIndex);
    BOOL   SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y,
                        int cx, int cy, uint32_t uFlags);
    BOOL   ShowScrollBar(HWND hWnd, int wBar, BOOL bShow);
    int    SetScrollRange(HWND hWnd, int nBar, int nMinPos, int nMaxPos, BOOL bRedraw);
    int    SetScrollPos(HWND hWnd, int nBar, int nPos, BOOL bRedraw);
    BOOL   AdjustWindowRect(struct RECT* lpRect, DWORD dwStyle, BOOL bMenu);
    BOOL   EnableWindow(HWND hWnd, BOOL bEnable);
    BOOL   InvalidateRect(HWND hWnd, const struct RECT* lpRect, BOOL bErase);
    BOOL   UpdateWindow(HWND hWnd);
    void   SetRect(struct RECT* lpRect, int left, int top, int right, int bottom);
    void   SetRectEmpty(struct RECT* lpRect);
    BOOL   PlaySoundA(const char* pszSound, HMODULE hmod, DWORD fdwSound);
    BOOL   DrawTextA(HDC hdc, const char* lpchText, int cchText,
                      struct RECT* lprc, UINT format);
    BOOL   GetOpenFileNameA(void* lpofn);
    HDC    BeginPaint(HWND hWnd, struct PAINTSTRUCT* lpPaint);
    BOOL   EndPaint(HWND hWnd, const struct PAINTSTRUCT* lpPaint);

    /* CRT helpers */
    unsigned int CRT_time(unsigned int* t);
    int     CRT_toupper(int c);
    int     CRT_atoi(const char* s);
    char*   CRT_strtok(char* str, const char* delim);
    void*   CRT_localtime(unsigned int* timer);
    void    CRT_mkdir(const char* path);
    char*   CRT_itoa(int value, char* str, int radix);
    void    CRT_memset_pattern(void* dst, int val, int count, void* pattern);
    int     CRT_exit(const char** a, const char** b);
    void    CRT_free_pattern(void* ptr, int val, int count, void* pattern);

    /* INI helpers */
    int   Config_GetIniInt(void* config, const char* section, const char* key, int defaultVal);
    void  Config_WriteInt(void* config, const char* section, const char* key, uint32_t value);
    void  Config_GetIniString(void* config, const char* section, const char* key,
                              const char* def, char* out, uint32_t maxLen);
    int    wsprintfA(char* buf, const char* fmt, ...);
}
#endif /* _WIN32 */

#ifndef _WIN32
/* Non-Windows stubs for helpers NOT covered by sdl3_window.h */
static inline int Config_GetIniInt(void*, const char*, const char*, int d) { return d; }
static inline void Config_WriteInt(void*, const char*, const char*, unsigned int) {}
static inline void Config_GetIniString(void*, const char*, const char*, const char*, char* out, unsigned int) { if(out) out[0]=0; }
static inline unsigned int CRT_time() { return 0; }
static inline int CRT_toupper(int c) { return (c>='a'&&c<='z')?c-32:c; }
static inline int CRT_atoi(const char* s) { return s?atoi(s):0; }
static char _strtok_empty_cgwnd[1] = {0}; static inline char* CRT_strtok(char*, const char*) { return _strtok_empty_cgwnd; }
static inline void* CRT_localtime(unsigned int*) { static int t=0; return &t; }
static inline int CRT_mkdir(const char*) { return 0; }
static inline char* CRT_itoa(int v, char* buf, int) { if(buf)snprintf(buf,32,"%d",v); return buf; }
static inline void CRT_memset_pattern(void*, int, int, void*) {}
static inline int CRT_exit(const char**, const char**) { exit(0); return 0; }
static inline void CRT_free_pattern(void*, int, int, void*) {}
static inline void* GetModuleHandleA(const char*) { return NULL; }
static inline unsigned int GetModuleFileNameA(void*, char*, unsigned int) { return 0; }
/* GetFileVersionInfoSizeA/GetFileVersionInfoA/VerQueryValueA/lstrcpyA/
 * lstrcatA/lstrlenA ARE covered by sdl3_window.h (included above) — the
 * local static duplicates that used to live here had bodies identical to
 * sdl3_window.cpp's, which GCC's identical-code-folding silently promoted
 * into colliding global symbols (LINK-001). Removed; sdl3_window.cpp's
 * definitions are used directly. */
/* operator_new / GLOBAL_free are defined in link_stubs / stubs_impl */
extern void* operator_new(size_t);
extern void  GLOBAL_free(void*);
#endif

/* ================================================================== */
/* Game global variables (all platforms)                               */
/* ================================================================== */
extern void*    g_config_ini;           /* INI config object at 0x4A9EEC */
extern int      g_screen_width;         /* 0x4851D8 */
extern int      g_screen_height;        /* 0x485214 */
extern int      g_screen_center_x;      /* 0x4851F8 */
extern int      g_screen_center_y;      /* 0x4851FC */
extern RECT     g_fullscreen_rect;      /* 0x4851E0 */
extern int      g_window_left;          /* 0x485200 */
extern int      g_window_top;           /* 0x485204 */
extern int      g_window_right;         /* 0x485208 */
extern int      g_window_bottom;        /* 0x48520C */
extern RECT     g_client_rect;          /* 0x485220 */
extern uint8_t  g_is_fullscreen;        /* 0x485210 */
extern uint8_t  g_show_scrollbars;      /* 0x485238 */
extern uint8_t  g_clean_exit;           /* 0x485218 */
extern uint8_t  g_window_mode;          /* 0x4851F0 */
extern uint8_t  g_build_mode;
extern uint8_t  g_road_build_mode;
extern int      g_placement_resource_id;
extern int      g_game_mode;            /* 0x4851F4 — 1=main menu, 2=in-game */
extern int      g_timer_id;
class ResourceManager;
extern ResourceManager g_resmgr;  /* 0x4855E8 — object, not a pointer (was void*,
                                    * a widespread cross-TU landmine — see
                                    * PROGRESS.md's g_resmgr sweep) */
extern int      g_demo_mode;            /* 0x4A9918 */
extern int      g_easter_egg;
extern uint32_t g_screen_bpp;           /* 0x48521C */
extern int      g_world_width;          /* 0x4AAD0C */
extern int      g_world_height;         /* 0x4AAD10 */
extern int      g_client_offset_x;      /* 0x485228 */
extern int      g_client_offset_y;      /* 0x48522C */
extern int      g_viewport_x;           /* 0x4AAD24 */
extern int      g_viewport_y;           /* 0x4AAD28 */
extern int      g_viewport_rect_left;   /* 0x4AAD14 */
extern int      g_viewport_rect_top;    /* 0x4AAD18 */
extern int      g_viewport_rect_right;  /* 0x4AAD1C */
extern int      g_viewport_rect_bottom; /* 0x4AAD20 */
extern void*    g_font_small;

/* Subsystem globals */
extern void*    g_ui_main;              /* 0x4FD378 */
extern void*    g_town;                 /* 0x4FD37C */
extern void*    g_postcard;             /* 0x4FD384 */
extern void*    g_cursor;               /* 0x4FD380 */
extern void*    g_trainstation_window;  /* 0x485258 */
extern void*    g_audio_mgr;            /* 0x4FD38C */
extern void*    g_about;                /* 0x4FD390 */
extern void*    g_ddraw;
extern void*    g_audio;                /* 0x4FD3BC */
extern void*    g_netman;               /* 0x4FD3AC */
extern void*    g_game;                 /* 0x4854C8 */
extern void*    g_main_window;          /* 0x4AA4A0 */
extern void*    g_async_task_queue;     /* 0x4A9AD0 */
class UI_Manager;
extern UI_Manager* g_tooltip_mgr;       /* 0x4FD220 */
extern "C" void UI_ProcessObjectTimers(void);         /* 0x420000, __cdecl */
extern "C" void Game_DispatchCursorFeedback(void* game);  /* 0x411760, __cdecl */
class TileMap;
extern TileMap*   g_tilemap;              /* 0x4AAD08 */
extern void*    g_building_mgr;         /* 0x485448 */
extern World*   g_world;                /* 0x4A98B0 */
extern uint8_t  g_in_build_mode;
extern uint8_t  g_asset_mgr;

/* ROM strings */
extern const char s_WINDOW_ATTRIBUTES_0047e1a0[];
extern const char s_RectLeft_0047e1b4[];
extern const char s_RectTop_0047e198[];
extern const char s_RectRight_0047e18c[];
extern const char s_RectBottom_0047e180[];
extern const char s_BALANCING_0047e164[];
extern const char s_MinVehicleFPS_0047e170[];
extern const char s_MinBuildingFPS_0047e154[];
extern const char s_MinMinifigFPS_0047e144[];
extern const char s_MinFlyingFPS_0047e134[];
extern const char s_PROCESS_0047e120[];
extern const char s_CleanExit_0047e128[];
extern const char s_LEGO_LOCO_0047e1c0[];
extern const char s_StringFileInfo_080904B0_FileVer_0047e0f8[];
extern const char DAT_0047e0f4[];
extern const char s_LEGO_LOCO_CLASS[];
extern const char s_ScreenSaver_0047e2b4[];
extern const char s_Sound_0047e2c0[];
extern const char s__s_s_s_0047e3d0[];
extern const char DAT_0047e3cc[];
extern const char DAT_0047e3c8[];
extern const char DAT_00479190[];
extern const char g_empty_string;
extern char g_install_path[256];
extern char g_remote_res_path[256];

static void destroy_subsystem(void* ptr) {
    if (ptr != nullptr) {
        void** vtable = *reinterpret_cast<void***>(ptr);
        using DeletingDestructor = void (*)(void*, int);
        const auto destructor = reinterpret_cast<DeletingDestructor>(vtable[0]);
        destructor(ptr, 1);
        ptr = nullptr;
    }
}

/* Local `static void PlaySound(uint32_t)` helper removed 2026-08-15 —
 * zero callers anywhere in this file or the tree, no original address
 * citation (unlike every other reconstructed function here), and a
 * duplicate of the real, already-implemented, already-correctly-used
 * PlaySound(UINT) (0x447930, resources/ResourceManager.cpp) —
 * ui/TrainStationWindow.cpp:255 already calls that real function via
 * its own `extern void __cdecl PlaySound(uint32_t)` declaration. Having
 * both a static PlaySound here and the real extern PlaySound in the
 * same binary was a live tripwire: any TU that saw both declarations
 * would silently drop the `static` per C++'s linkage rules ([basic.link]
 * — a name once given external linkage cannot later be redeclared
 * internal), causing a duplicate-symbol link error depending on include
 * order. Discovered while wiring RESMGR_Shutdown below, which needed
 * ResourceManager.h's declaration of the real PlaySound in the same TU. */


/* ================================================================== */
/* CGWND::CGWND — Constructor                                          */
/* Address: 0x4061E0                                                   */
/* ================================================================== */
CGWND::CGWND(HINSTANCE hInstance)
{
    /* Set vtable to 0x4774C4 */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Init flags */
    this->field_10 = 0;
    g_timer_id = 0;

    /* Store fields */
    this->hWnd           = nullptr;     /* +0x08: main game window — set later by RegisterWindowClass */
    this->hWndDesktop    = nullptr;     /* +0x04: will be set below to GetDesktopWindow() */
    this->hInstance      = hInstance;   /* +0x0C */
#ifdef _WIN32
    this->hWndDesktop    = GetDesktopWindow();  /* +0x04 */
#else
    this->hWndDesktop    = nullptr;             /* +0x04: no desktop concept in SDL3 */
#endif
    this->hWnd           = nullptr;     /* +0x08: reset */

    /* Reset build/placement mode globals */
    if (g_build_mode != 0) {
        g_build_mode             = 0;
        g_road_build_mode        = 0;
        g_placement_resource_id  = -1;
    }

    /* Set game mode to main menu */
    g_game_mode       = 1;
    g_window_mode     = 0;
    g_show_scrollbars = 0;
    g_is_fullscreen   = 0;

    /* Clear display state */
    SetRect(&g_fullscreen_rect, 0, 0, 0, 0);
    g_screen_width    = 0;
    g_screen_height   = 0;
    g_screen_center_x = 0;
    g_screen_center_y = 0;
    SetRect(reinterpret_cast<RECT*>(&g_window_left),  0, 0, 0, 0);
    SetRect(&g_client_rect, 0, 0, 0, 0);

    /* Zero version fields */
    this->versionMajor    = 0;
    this->versionMinor    = 0;
    this->versionBuild    = 0;
    this->versionRevision = 0;

    /* Read EXE VERSIONINFO and populate version fields */
    this->ResetState();
}


/* ================================================================== */
/* CGWND::scalar deleting destructor — Vtable slot [0]                 */
/* Address: 0x4062A0                                                   */
/* ================================================================== */
CGWND::~CGWND()
{
    /* Restore vtable to CGWND vtable */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Release g_config_ini if it exists.
     * Original uses vtable[0] scalar-deleting-dtor, but g_config_ini
     * is a PlayerConfig with a descriptor (0x4784BC) at offset 0,
     * not a real vtable. Under SDL3 we use GLOBAL_free directly. */
    if (g_config_ini != nullptr) {
#ifdef _WIN32
        void** ini_vtbl = *(void***)g_config_ini;
        ((void(*)(void*,byte))ini_vtbl[0])(g_config_ini, 1);
#else
        GLOBAL_free(g_config_ini);
#endif
        g_config_ini = nullptr;
    }

    /* MSVC scalar-delete: if flags & 1, free memory */
}


/* ================================================================== */
/* CGWND::ShowMainMenu — Initialize display for main menu              */
/* Address: 0x406480                                                   */
/* ================================================================== */
void CGWND::ShowMainMenu()
{
#ifdef _WIN32
    /* Store desktop window handle */
    this->hWndDesktop = GetDesktopWindow();

    /* Query screen dimensions */
    g_screen_width  = GetSystemMetrics(0);  /* SM_CXSCREEN */
    g_screen_height = GetSystemMetrics(1);  /* SM_CYSCREEN */
#else
    /* SDL3: use the window created by SDL3_WindowInit */
    this->hWndDesktop = nullptr;
    {
        const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
        g_screen_width  = dm ? dm->w : 800;
        g_screen_height = dm ? dm->h : 600;
    }
#endif

    /* Calculate center */
    g_screen_center_x = g_screen_width / 2;
    g_screen_center_y = g_screen_height / 2;

    /* Full screen rect = entire display */
    SetRect(&g_fullscreen_rect, 0, 0, g_screen_width, g_screen_height);

    /* Reset display flags */
    g_window_mode     = 0;
    g_show_scrollbars = 0;
    g_is_fullscreen   = 0;

    /* Read saved window position from INI [WINDOW_ATTRIBUTES] */
    g_window_left   = Config_GetIniInt(g_config_ini,
                        s_WINDOW_ATTRIBUTES_0047e1a0,
                        s_RectLeft_0047e1b4, 50);
    g_window_top    = Config_GetIniInt(g_config_ini,
                        s_WINDOW_ATTRIBUTES_0047e1a0,
                        s_RectTop_0047e198, 50);
    g_window_right  = Config_GetIniInt(g_config_ini,
                        s_WINDOW_ATTRIBUTES_0047e1a0,
                        s_RectRight_0047e18c,
                        g_screen_width - 50);
    g_window_bottom = Config_GetIniInt(g_config_ini,
                        s_WINDOW_ATTRIBUTES_0047e1a0,
                        s_RectBottom_0047e180,
                        g_screen_height - 50);

    /* Clamp window position to visible screen area */
    if (g_window_left < 0 || g_window_left > g_screen_width) {
        g_window_left = 10;
    }
    if (g_window_right - g_window_left > g_screen_width - 10) {
        g_window_right = g_window_left + g_screen_width - 10;
    }
    if (g_window_top < 0 || g_window_top > g_screen_height) {
        g_window_top = 10;
    }
    if (g_window_bottom - g_window_top > g_screen_height - 10) {
        g_window_bottom = g_window_top + g_screen_height - 10;
    }

    /* Read per-type FPS balancing limits from INI [BALANCING] */
    this->minVehicleFPS  = static_cast<uint8_t>(Config_GetIniInt(g_config_ini,
                               s_BALANCING_0047e164,
                               s_MinVehicleFPS_0047e170, 20));
    this->minBuildingFPS = static_cast<uint8_t>(Config_GetIniInt(g_config_ini,
                               s_BALANCING_0047e164,
                               s_MinBuildingFPS_0047e154, 18));
    this->minMinifigFPS  = static_cast<uint8_t>(Config_GetIniInt(g_config_ini,
                               s_BALANCING_0047e164,
                               s_MinMinifigFPS_0047e144, 16));
    this->minFlyingFPS   = static_cast<uint8_t>(Config_GetIniInt(g_config_ini,
                               s_BALANCING_0047e164,
                               s_MinFlyingFPS_0047e134, 14));

    /* Read and reset CleanExit flag */
    g_clean_exit = static_cast<uint8_t>(Config_GetIniInt(g_config_ini,
                              s_PROCESS_0047e120,
                              s_CleanExit_0047e128, 1));
    Config_WriteInt(g_config_ini, s_PROCESS_0047e120,
                    s_CleanExit_0047e128, 0);
}


/* ================================================================== */
/* CGWND::InitGame — Display validation gate                           */
/* Address: 0x406680                                                   */
/* ================================================================== */
int CGWND::InitGame()
{
    char msg[256];

#ifdef _WIN32
    HDC hdc = GetDC(this->hWndDesktop);
    uint32_t colorDepth = GetDeviceCaps(hdc, 0x18);  /* BITSPIXEL */
    g_screen_bpp = GetDeviceCaps(hdc, 0x0C);          /* PLANES */

    ReleaseDC(this->hWndDesktop, hdc);
#else
    /* SDL3: default to 32 bpp; color depth check passes */
    uint32_t colorDepth = 32;
    g_screen_bpp = 32;
#endif
    /* Gate 1: Color depth check (8-bit paletted = 8 bpp) */
    if (colorDepth < 0x80000000 || g_screen_bpp > 0x10) {
        g_resmgr.FormatResourceString(0x7A, msg, sizeof(msg));
    } else {
        /* Gate 2: Mouse check */
        int mousePresent = GetSystemMetrics(0x13);  /* SM_MOUSEPRESENT */
        if (mousePresent == 0) {
            g_resmgr.FormatResourceString(0x7B, msg, sizeof(msg));
        } else {
            /* Gate 3: Screen width in range 800-1280 */
            if (g_screen_width < 0x501) {
                g_resmgr.FormatResourceString(0x7A, msg, sizeof(msg));
            } else if (g_screen_width > 799) {
                return 1;
            } else {
                g_resmgr.FormatResourceString(0x7A, msg, sizeof(msg));
            }
        }
    }

    MessageBoxA(nullptr, msg, s_LEGO_LOCO_0047e1c0, 0);
    return 0;
}


/* ================================================================== */
/* CGWND::ResetState — Read EXE VERSIONINFO                            */
/* Address: 0x4062E0                                                   */
/* ================================================================== */
void CGWND::ResetState()
{
    char     filePath[0x504];
    char     versionStr[0x1000];
    uint32_t dummy;
    void*    verData   = nullptr;
    DWORD    verSize;

    CRT_time();

    /* Zero the version string buffer (the original clears through +0xFFE). */
    std::memset(versionStr, 0, 0xFFF);

    HMODULE hMod = GetModuleHandleA(nullptr);
    GetModuleFileNameA(hMod, filePath, 0x504);

    verSize = GetFileVersionInfoSizeA(filePath, &dummy);
    if (verSize != 0) {
        verData = operator_new(verSize);
    }

    if (verData != nullptr) {
        if (GetFileVersionInfoA(filePath, 0, verSize, verData)) {
            char*   verStrPtr = nullptr;
            uint32_t verStrLen = 0;

            if (VerQueryValueA(verData,
                    s_StringFileInfo_080904B0_FileVer_0047e0f8,
                    reinterpret_cast<void**>(&verStrPtr), &verStrLen) && verStrLen != 0)
            {
                const char* src = verStrPtr;
                char* dst = versionStr;
                size_t len = 0;
                while (*src) { src++; len++; }
                len++;
                src = verStrPtr;

                const size_t words = len >> 2;
                if (words != 0) {
                    std::memcpy(dst, src, words * sizeof(uint32_t));
                    src += words * sizeof(uint32_t);
                    dst += words * sizeof(uint32_t);
                }
                for (size_t i = 0; i < (len & 3); i++) {
                    *dst++ = *src++;
                }
            }
        }
        GLOBAL_free(verData);
    }

    const char* p = versionStr;
    int len = -1;
    while (*p) { len--; p++; }

    if (len != -2) {
        char* token = CRT_strtok(versionStr, DAT_0047e0f4);
        this->versionMajor = CRT_atoi(token);

        size_t tlen = 0;
        while (*token) { token++; tlen++; }
        token = CRT_strtok(token + tlen + 1, DAT_0047e0f4);
        this->versionMinor = CRT_atoi(token);

        tlen = 0;
        while (*token) { token++; tlen++; }
        token = CRT_strtok(token + tlen + 1, DAT_0047e0f4);
        this->versionBuild = CRT_atoi(token);

        tlen = 0;
        while (*token) { token++; tlen++; }
        token = CRT_strtok(token + tlen + 1, DAT_0047e0f4);
        this->versionRevision = CRT_atoi(token);
    }
}


/* ================================================================== */
/* Entity::SetPause — Toggle active/paused state (MIS-LABELED as CGWND) */
/* Address: 0x4061B0 — accesses Entity +0x24 (visible) and +0x48 (audio_channel) */
/* CGWND is only 0x28 bytes; this is actually an Entity/GameObject method.  */
/* ================================================================== */
void __thiscall CGWND_SetPause(void* self, uint8_t paused)
{
    auto* self_bytes = static_cast<uint8_t*>(self);
    *reinterpret_cast<uint8_t*>(self_bytes + 0x24) = paused ? 1 : 0;

    void** vtable = *reinterpret_cast<void***>(self);
    using PauseMethod = void (__thiscall*)();
    const auto pause_method = reinterpret_cast<PauseMethod>(vtable[0x04 / 4]);
    pause_method();

    void* audio_ch = *reinterpret_cast<void**>(self_bytes + 0x48);
    if (audio_ch != nullptr) {
        if (paused) {
            extern void CGWND_AudioChannel_Play(uint32_t ch);
            CGWND_AudioChannel_Play(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(audio_ch)));
        } else {
            extern void CGWND_AudioChannel_Pause(int ch);
            CGWND_AudioChannel_Pause(static_cast<int>(reinterpret_cast<uintptr_t>(audio_ch)));
        }
    }
}


/* ================================================================== */
/* CGWND_SetMode — Core game mode state machine (modes 0-10)           */
/* Address: 0x408130 — free function; no this pointer                  */
/* ================================================================== */
void CGWND_SetMode(int new_mode)
{
    extern int g_game_mode;
    int old_mode = g_game_mode;

    if (g_game_mode == new_mode) return;

    g_game_mode = new_mode;
#ifndef _WIN32
    loco::host_test::emit_mode_changed(old_mode, new_mode);
#endif

    switch (new_mode) {
    case 1:
        static_cast<CGWND*>(g_main_window)->initMode1();
        return;

    case 2:
        {
            extern void Game_SetScreenMode(void* game, int a, int b, int c);
            Game_SetScreenMode(g_game, 0, 1, 0);
            if (g_ui_main) static_cast<EditWindow*>(g_ui_main)->show();
        }
        return;

    case 3:
        CGWND_EnterMode3(old_mode);
        return;

    case 4:
        {
            extern void Game_SelectGameObject(void* game, void* obj);
            Game_SelectGameObject(g_game, nullptr);

            extern void BuildingMgr_DestroyAll(void* mgr, int flags);
            extern void* g_building_mgr;
            BuildingMgr_DestroyAll(g_building_mgr, 0);

            extern void UI_ResetTooltips(void* mgr, int reset_type);
            UI_ResetTooltips(g_tooltip_mgr, 0);

            extern void World_Reset(void* world, int flags);
            World_Reset(reinterpret_cast<void*>(static_cast<uintptr_t>(0x4A98B0)), 0);

            g_in_build_mode = 0;
        }
        return;

    case 5:
    case 6:
    case 7:
    case 9:
        {
            if (g_audio != nullptr) {
                extern void GameAudio_UpdateVolume(void* a, int v);
                GameAudio_UpdateVolume(g_audio, 1);
            }
            extern void Game_SetScreenMode(void* game, int a, int b, int c);
            Game_SetScreenMode(g_game, 0, 0, 0);

            void* screen_obj = nullptr;
            if (new_mode == 5) {
                screen_obj = g_town;
            } else if (new_mode == 6) {
                screen_obj = g_postcard;
            } else if (new_mode == 7) {
                extern void Cursor_Show(void* c);
                Cursor_Show(g_cursor);
            } else if (new_mode == 9) {
                if (old_mode == 4) {
                    extern void NETMAN_SendMapData(void* net, int flags);
                    NETMAN_SendMapData(g_netman, 0);
                }
                screen_obj = g_postcard_send;
            }
            if (screen_obj != nullptr) {
                void** vtable = *reinterpret_cast<void***>(screen_obj);
                using ShowMethod = void (__thiscall*)();
                const auto show_method = reinterpret_cast<ShowMethod>(vtable[0x08 / 4]);
                show_method();
            }
        }
        return;

    case 8:
        {
            auto* audio_mgr_bytes = static_cast<uint8_t*>(g_audio_mgr);
            *reinterpret_cast<int*>(audio_mgr_bytes + 0x3074) = old_mode;
        }
        return;

    case 10:
        {
#ifdef _WIN32
            // Original 0x40824C..0x4082C8: play 0x5026 through GameAudio,
            // restore DirectDraw, then post the terminal Win32 message.
            if (g_audio != nullptr) {
                extern int GameAudio_PlayResourceEx(void* a, int id, uint32_t* out);
                uint32_t ch = 0;
                GameAudio_PlayResourceEx(g_audio, 0x5026, &ch);

                if (ch != 0) {
                    /* ch is a real 32-bit x86 pointer here (this whole block
                     * is _WIN32-only). Call the typed method directly rather
                     * than through CGWND_AudioChannel_IsActive — that
                     * free-function facade was declared int-returning here
                     * but defined void-returning in shared/defsym_stubs.cpp
                     * (return type isn't part of C++ mangling, so every
                     * caller silently read garbage out of EAX), the same
                     * landmine class fixed for AudioChannel_IsActive
                     * elsewhere this session. */
                    AudioChannel* channel =
                        reinterpret_cast<AudioChannel*>(static_cast<uintptr_t>(ch));
                    while (!channel->IsActive()) {}
                    channel->Release();
                }
            }

            HWND main_hwnd = static_cast<CGWND*>(g_main_window)->hWnd;
            if (g_ddraw != nullptr) {
                /* SetCooperativeLevel — real ABI vtable[20] (byte offset
                 * 0x50), dispatched by name (see
                 * native/ddraw_surface_ops.c's DDRAW_ReleaseSurfaces for
                 * the same call, converted 2026-08-14 — this shim is
                 * API- not ABI-compatible, see CLAUDE.md). */
                static_cast<IDirectDraw4*>(g_ddraw)->SetCooperativeLevel(main_hwnd, 8);
            }
            PostMessageA(main_hwnd, 0x10 /* WM_CLOSE */, 0, 0);
#else
            // Host-only equivalent of the original 0x5026 exit-sound
            // request.  Queue the exit sweep while the audio device is
            // still open (kept alive by the background-music stream),
            // then stop only the looping music so the drain loop only
            // needs to wait for the short exit sweep.
            bool ok = SDL3_GameAudioPlayResource(0x5026);
            fprintf(stderr, "[audio] CGWND_SetMode(10): PlayResource(0x5026) -> %s\n", ok ? "true" : "FALSE");
            SDL3_GameAudioStopLooping();

            /* Real contract (matches the _WIN32 branch above): defer the
             * actual quit to whoever next drains the main window's message
             * queue, rather than the host detecting g_game_mode == 10
             * directly and short-circuiting out of its own loop. */
            HWND main_hwnd = static_cast<CGWND*>(g_main_window)->hWnd;
            PostMessageA(main_hwnd, 0x10 /* WM_CLOSE */, 0, 0);
#endif
        }
        return;

    default:
        break;
    }
}


/* ================================================================== */
/* CGWND_EnterMode3 — Transition handler for entering game mode 3      */
/* Address: 0x4086F0 — free function; __cdecl                          */
/*                                                                      */
/* Receives the PREVIOUS mode and cleans up/transitions based on what's */
/* being left. Case 2 (main menu) cancels. Cases 5/6/7/9 hide their    */
/* overlays. All paths fall through to common cleanup: reset buildings, */
/* tooltips, world, build mode, cursor, and audio.                     */
/* ================================================================== */
void CGWND_EnterMode3(int old_mode)
{
    /* ---- free-function adapters (no typed class yet) ---------------- */
    extern void  WIN32_QueueAsyncTask(void* queue, void* callback, int param);
    extern void  UI_ResetTooltips(void* mgr, int reset_type);
    /* UI_ProcessObjectTimers + Game_DispatchCursorFeedback are declared at
     * file scope with C linkage (original __cdecl C functions). */
    extern void  NETMAN_SendMapData(void* net, int flags);    /* 0x43D350 */
    /* TileMap_UpdateAll / TileMap_InvalidateRect bind to out-of-line
     * emissions of the tilemap.h inline wrappers (HostMode3Bootstrap.cpp);
     * g_tilemap is the TileMap* singleton (file-scope decl at line 214).
     * (The old function-local `class TileMap;` was removed — it shadowed
     * the file-scope declaration and broke static_cast below.) */
    extern void TileMap_UpdateAll(TileMap* tm);
    extern void TileMap_InvalidateRect(TileMap* tm, int left, int top,
                                       int right, int bottom);

    switch (old_mode) {

    /* ---------------------------------------------------------------- */
    /* Case 2 — Coming from main menu: just cancel                      */
    /* 0x408726: MOV [0x4851F4], 2; return                             */
    /* ---------------------------------------------------------------- */
    case 2:
        g_game_mode = 2;
        return;

    /* ---------------------------------------------------------------- */
    /* Cases 5, 6, 7 — Coming from town/postcard/cursor overlays       */
    /* 0x408739..0x4087AD: hide each overlay via vtable[1], then        */
    /*                    GameAudio::UpdateVolume, Game::SetScreenMode,  */
    /*                    TileMap::InvalidateRect → common tail          */
    /* ---------------------------------------------------------------- */
    case 5:
    case 6:
    case 7:
        {
            /* Hide each overlay subsystem (vtable slot 1 = hide) */
            if (g_town != nullptr) {
                static_cast<Town*>(g_town)->hide();          /* 0x408739 */
            }
            if (g_postcard != nullptr) {
                static_cast<PostcardAlbum*>(g_postcard)->hide(); /* 0x408759 */
            }
            if (g_cursor != nullptr) {
                static_cast<Cursor*>(g_cursor)->hide();      /* 0x408779 */
            }
            if (g_audio != nullptr) {
                static_cast<GameAudio*>(g_audio)->UpdateVolume(0); /* 0x4135B0 */
            }
            static_cast<Game*>(g_game)->SetScreenMode(1, 1, 0); /* 0x411DC0 */
            TileMap_InvalidateRect(static_cast<TileMap*>(g_tilemap),
                g_viewport_rect_left, g_viewport_rect_top,
                g_viewport_rect_right, g_viewport_rect_bottom);
        }
        break;  /* → common tail */

    /* ---------------------------------------------------------------- */
    /* Case 9 — Coming from postcard-send overlay                       */
    /* 0x4087B2..0x40880E: hide postcard send, same tail as 5/6/7       */
    /* ---------------------------------------------------------------- */
    case 9:
        {
            if (g_postcard_send != nullptr) {
                static_cast<PostcardPreviewWindow*>(g_postcard_send)->hide(); /* 0x4087B2 */
            }
            if (g_audio != nullptr) {
                static_cast<GameAudio*>(g_audio)->UpdateVolume(0);
            }
            static_cast<Game*>(g_game)->SetScreenMode(1, 1, 0);
            TileMap_InvalidateRect(static_cast<TileMap*>(g_tilemap),
                g_viewport_rect_left, g_viewport_rect_top,
                g_viewport_rect_right, g_viewport_rect_bottom);
        }
        break;  /* → common tail */

    /* ---------------------------------------------------------------- */
    /* Case 1 — Coming from loading screen / mode 1                     */
    /* 0x408813..0x4088CD: fullscreen toggle, kill timer, netman send,  */
    /*                    demo audio check, tilemap update, narration,   */
    /*                    then FALLS THROUGH to case 4                   */
    /* ---------------------------------------------------------------- */
    case 1:
        {
            /* JGE: g_world_width >= g_screen_width → go_windowed=1 */
            char go_windowed = (g_world_width >= g_screen_width) ? 1 : 0;
            CGWND_SetFullscreenMode(go_windowed);   /* 0x407D20 */

            HWND hWnd = static_cast<CGWND*>(g_main_window)->hWnd;  /* +0x08 */
            KillTimer(hWnd, static_cast<uintptr_t>(g_timer_id)); /* IAT */
            g_timer_id = 0;

            NETMAN_SendMapData(g_netman, 0);            /* 0x43D350 */
            PostMessageA(hWnd, 0x406, static_cast<uint32_t>(g_game_time), 0);  /* WM_USER+6 */

            if (g_demo_mode == 1 && g_audio != nullptr) {
                extern int Config_GetIniInt(void* config, const char* section,
                                            const char* key, int defaultVal);
                int sound_flag = Config_GetIniInt(g_config_ini,
                    "ScreenSaver", "Sound", 0);     /* 0x452D60 */
                static_cast<GameAudio*>(g_audio)->SetMute(
                    (sound_flag != 0) ? 1 : 0);
            }
            TileMap_UpdateAll(static_cast<TileMap*>(g_tilemap)); /* 0x457320 */
#ifdef _WIN32
            if (g_audio_mgr != nullptr) {
                static_cast<HelpWnd*>(g_audio_mgr)->play_narration(5, 0); /* 0x44F560 */
            }
#else
            /* Host: HelpWnd's presentation layer is not ported. create() is
             * never called for g_audio_mgr on this path (see
             * InitAllSubsystems's #ifndef _WIN32 branch above), six render
             * methods are stubs (ui/HelpWnd_stubs.cpp), and
             * play_narration's chain reaches Cursor_WaitForBlit (0x414BB0),
             * which polls a DirectDraw surface vtable slot the host has no
             * object for. Skip loudly rather than run an unported
             * subsystem's presentation path. */
            std::fprintf(stderr, "[HOST] EnterMode3 case 1: tutorial narration skipped "
                                  "(HelpWnd presentation layer not ported — see PROGRESS.md)\n");
#endif
        }
        /* FALLS THROUGH to case 4 */
        [[fallthrough]];

    /* ---------------------------------------------------------------- */
    /* Case 4 — Coming from scenario / world setup                      */
    /* 0x4088CD..0x40892A: PostMessage, netman send, screen mode,       */
    /*                    UI_ProcessObjectTimers, conditional build-     */
    /*                    mode cleanup → common tail                     */
    /* ---------------------------------------------------------------- */
    case 4:
        {
            HWND hWnd = static_cast<CGWND*>(g_main_window)->hWnd;  /* +0x08 */
            PostMessageA(hWnd, 0x406, static_cast<uint32_t>(g_game_time), 0);
            NETMAN_SendMapData(g_netman, 0);
            static_cast<Game*>(g_game)->SetScreenMode(1, 1, 0);
            UI_ProcessObjectTimers();              /* 0x420000 */

            if (g_in_build_mode != 0) {
                TileMap_UpdateAll(static_cast<TileMap*>(g_tilemap));
                static_cast<BuildingMgr*>(g_building_mgr)->UpdateStoredTargets();
            }
        }
        break;  /* → common tail */

    /* ---------------------------------------------------------------- */
    /* Default / cases 0, 3, 8, 10, >9 → straight to common tail        */
    /* ---------------------------------------------------------------- */
    default:
        break;
    }

    /* ================================================================ */
    /* Common tail — 0x40892A..0x40899E                                  */
    /* ================================================================ */

    static_cast<BuildingMgr*>(g_building_mgr)->InvalidateAll(1); /* 0x434800 */
    UI_ResetTooltips(g_tooltip_mgr, 1);            /* 0x423F80 */
    g_world->Reset(1);                             /* 0x44DBD0 */

    /* Queue async task with callback at 0x42CC60 — the SDL host
     * substitutes its typed post-load worker (HostMode3Bootstrap.cpp). */
    WIN32_QueueAsyncTask(&g_async_task_queue,
        reinterpret_cast<void*>(&loco::host::HostPostLoadWorker), 0);

    /* Build-mode state cleanup — 0x408972..0x408986 */
    if (g_build_mode != 0) {
        g_build_mode = 0;
        g_road_build_mode = 0;
        g_placement_resource_id = -1;
    }

    /* Game cursor feedback dispatch — 0x40898A */
    Game_DispatchCursorFeedback(g_game);           /* 0x411760 */

    /* Audio volume update (silence) — 0x408992..0x40899E */
    if (g_audio != nullptr) {
        static_cast<GameAudio*>(g_audio)->UpdateVolume(0);  /* 0x4135B0 */
    }
}


/* ================================================================== */
/* CGWND_Cleanup — Full game shutdown                                  */
/* Address: 0x4077A0 — free function; no this pointer                  */
/* ================================================================== */
void CGWND_Cleanup()
{
    extern void* g_building_mgr;        /* 0x485448 */

    /* ================================================================ */
    /* PHASE 1: Save window position and CleanExit=1 to lego.ini        */
    /* ================================================================ */
    Config_WriteInt(g_config_ini, "WINDOW ATTRIBUTES", "RectLeft",   g_window_left);
    Config_WriteInt(g_config_ini, "WINDOW ATTRIBUTES", "RectTop",    g_window_top);
    Config_WriteInt(g_config_ini, "WINDOW ATTRIBUTES", "RectRight",  g_window_right);
    Config_WriteInt(g_config_ini, "WINDOW ATTRIBUTES", "RectBottom", g_window_bottom);
    Config_WriteInt(g_config_ini, "PROCESS", "CleanExit", 1);

#ifndef _WIN32
    /* Host-only deviation: PHASE 2 onward is real subsystem teardown that
     * has never been exercised on host until the WM_CLOSE quit
     * state-machine work made it reachable for the first time, and it
     * hits three separate un-reconstructed blockers before it could ever
     * complete cleanly:
     *   - PHASE 2 does raw vtable dispatch on _g_network_thread
     *     (`(*(void***)_g_network_thread)[0]`) — the concrete type behind
     *     that global has never been modeled.
     *   - PHASE 2's thread-join loop casts the literal x86 address
     *     0x4A9AD0 to a live pointer and polls it via
     *     WIN32_GetThreadResult in an unbounded spin — meaningless (and
     *     unbounded-hang-risk) on a 64-bit host where that address isn't
     *     a real object.
     *   - PHASE 4's destroy_subsystem calls reach UI_WindowBase's real
     *     base destructor (UI_WindowBase_BaseDtor, shared/stubs_impl.cpp)
     *     for every UI subsystem (g_ui_main, g_town, g_postcard, g_cursor,
     *     g_postcard_send, g_trainstation_window, g_about) — a loud,
     *     deliberate stub-policy assert, not a bug, because that
     *     destructor's real body has never been reconstructed.
     * CleanExit=1/window-position bookkeeping above (PHASE 1) is real
     * game state (read back by initMode1's PATH B via g_clean_exit) and
     * must still run. main.cpp already performs its own host resource
     * teardown after CGWND_Cleanup returns (~CGWND, GLOBAL_free,
     * CoUninitialize, SDL3_GameAudioStopAll, SDL3_WindowQuit) before
     * process exit, so returning here does not leak the process — it
     * just defers PHASES 2-7's original-game-object teardown until each
     * blocker above is reconstructed. Revisit this guard once all three
     * are real. */
    return;
#endif

    /* ================================================================ */
    /* PHASE 2: Network thread teardown                                  */
    /* ================================================================ */
    extern void* _g_network_thread;
    if (_g_network_thread != nullptr) {
        extern void* _g_train;
        extern void Train_FlushMessages(void* train);
        Train_FlushMessages(_g_train);

        void** vtable = *reinterpret_cast<void***>(_g_network_thread);
        using ShutdownMethod = void (__thiscall*)(int);
        const auto shutdown = reinterpret_cast<ShutdownMethod>(vtable[0]);
        shutdown(1);
        _g_network_thread = nullptr;
    }

    {
        extern int  WIN32_GetThreadResult(void* state);
        extern void WIN32_Sleep(uint32_t ms);
        void* thread_state = reinterpret_cast<void*>(static_cast<uintptr_t>(0x4A9AD0));
        while (WIN32_GetThreadResult(thread_state) != 0) {
            WIN32_Sleep(100);
        }
    }

    /* ================================================================ */
    /* PHASE 3: Save world state and unlock sprites                      */
    /* ================================================================ */
    extern void World_Init(void* world);
    extern void World_Shutdown(int world);
    extern void Sprite_UnlockAll(int mgr);
    World_Init(reinterpret_cast<void*>(static_cast<uintptr_t>(0x4A98B0)));
    World_Shutdown(0x4A98B0);
    Sprite_UnlockAll(0x4AAD08);

    /* ================================================================ */
    /* PHASE 4: Destroy all UI/audio subsystems (reverse init order)     */
    /* ================================================================ */
    destroy_subsystem(g_ui_main);
    destroy_subsystem(g_town);
    destroy_subsystem(g_postcard);
    destroy_subsystem(g_cursor);
    destroy_subsystem(g_postcard_send);
    destroy_subsystem(g_trainstation_window);
    destroy_subsystem(g_audio_mgr);
    destroy_subsystem(g_about);

    /* ================================================================ */
    /* PHASE 5: Destroy additional non-UI subsystems                     */
    /* ================================================================ */
    extern void* _g_train;
    extern void* _DAT_004fd3a8;
    extern void* _g_dplay;
    extern void* _g_dplay_config;
    extern void* _g_train_resources;
    /* g_player_config: declared as PlayerConfig* in PlayerConfig.h */
    extern void* _g_audio_config;
    extern void* _g_dsound_object;

    destroy_subsystem(_g_train);
    destroy_subsystem(_DAT_004fd3a8);
    destroy_subsystem(g_netman);
    destroy_subsystem(_g_dplay);
    destroy_subsystem(_g_dplay_config);
    destroy_subsystem(_g_train_resources);
    destroy_subsystem(g_player_config);
    destroy_subsystem(g_config_ini);
    destroy_subsystem(_g_audio_config);
    destroy_subsystem(_g_dsound_object);

    /* ================================================================ */
    /* PHASE 6: Release cursor surface (refcounted at +4)               */
    /* ================================================================ */
    extern int* _g_cursor_surface;
    if (_g_cursor_surface != nullptr) {
        int*   surf    = _g_cursor_surface;
        void** vtable = reinterpret_cast<void**>(static_cast<uintptr_t>(surf[0]));
        using ReleaseMethod = void (__thiscall*)();
        const auto release = reinterpret_cast<ReleaseMethod>(vtable[0x08 / 4]);
        release();
        if (surf[1] == -1) {
            using DeleteMethod = void (__thiscall*)(int);
            const auto destroy = reinterpret_cast<DeleteMethod>(vtable[0]);
            destroy(1);
        }
        _g_cursor_surface = nullptr;
    }

    /* ================================================================ */
    /* PHASE 7: Low-level system shutdown                               */
    /* ================================================================ */
    extern void UIPANEL_FreeAllSurfaces(void);
    UIPANEL_FreeAllSurfaces();

    extern void* g_frame_event;
    if (g_frame_event != nullptr) {
        extern void WIN32_CloseHandle(void* h);
        WIN32_CloseHandle(g_frame_event);
        g_frame_event = nullptr;
    }

    extern uint32_t g_timer_event_id;
    extern void WIN32_timeKillEvent(uint32_t id);
    extern void WIN32_timeEndPeriod(uint32_t period);
    WIN32_timeKillEvent(g_timer_event_id);
    WIN32_timeEndPeriod(14);

    extern void Sprite_Shutdown(int mgr);
    Sprite_Shutdown(0x4AAD08);

    extern void Town_GameView_Cleanup(int* view);
    Town_GameView_Cleanup(reinterpret_cast<int*>(static_cast<uintptr_t>(0x4852A0)));

    extern void DDRAW_InvalidateAll(int* ddraw);
    DDRAW_InvalidateAll(reinterpret_cast<int*>(static_cast<uintptr_t>(0x4A9EF0)));

    /* Was a hardcoded-literal-address call to a free-function stub
       (RESDATA_ScriptedObject_Shutdown(int*), a real crash-on-touch
       landmine on this host's process layout — reinterpret_cast'ing a
       literal integer to a pointer is never a valid object address here).
       Call the real global's real, typed method directly. */
    g_scripted_object->Shutdown();

    extern void UI_FreeMessageBox(int msgbox);
    UI_FreeMessageBox(0x4FD220);

    /* Original (CGWND_Cleanup 0x407AAF..0x407ABE):
     *   mov ecx,0x4A99B0; call 0x41F4E0  — 0x4A99B0 event-list teardown
     *   mov ecx,0x4A9990; call 0x41D310  — InputMgr cleanup thunk
     *                                    (vtable[3] = ResetWorldState)
     * 0x41F4E0 frees both event lists (LoadEvents head +0x08, TimeEvents
     * head +0x0C), destroying every entry.  The 0x4A99B0 object is not
     * reconstructed yet (its event lists are only populated by the
     * deferred 0x41F5E0/0x41F6E0 INI loaders, so on the host there is
     * nothing to tear down).  The host path is an explicit guarded
     * adapter — it logs loudly instead of silently no-op'ing — and the
     * original path is preserved under _WIN32. */
#ifndef _WIN32
    std::fprintf(stderr,
        "[HOST] CGWND_Cleanup: 0x4A99B0 event-list teardown (0x41F4E0) "
        "deferred\n");
    std::fflush(stderr);
#else
    /* Original thiscall: ECX = &g_input_events (0x4A99B0).  Declared
     * here so the original path stays expressed; the definition arrives
     * with the reconstruction. */
    extern void INPUT_FreeEvents(void* self);   /* 0x41F4E0 */
    INPUT_FreeEvents(&g_input_events);
#endif

    /* Original: mov ecx,0x4A9990; call 0x41D310 (cleanup thunk:
     * mov eax,[ecx]; jmp [eax+0x0C]) — vtable[3] =
     * InputMgr::ResetWorldState (0x41E100).  ResetWorldState is virtual
     * (binary slot[3]), so this typed static-object call emits the same
     * thiscall + vtable[3] dispatch on g_input_mgr for both the _WIN32
     * and the host paths (host pointer model unchanged). */
    g_input_mgr.ResetWorldState();

    extern void Game_Shutdown(int* game);
    Game_Shutdown(reinterpret_cast<int*>(static_cast<uintptr_t>(0x4854C8)));

    g_resmgr.Shutdown();

    extern int CRT_0x470650(void);
    CRT_0x470650();
}


/* ================================================================== */
/* CGWND::RegisterWindowClass — Register window class and create main   */
/*                                window                                */
/* Address: 0x406ED0                                                    */
/*                                                                      */
/* Called by: GameLoop_Setup (0x406D80)                                 */
/*                                                                      */
/* Registers the "LEGO_LOCO" WNDCLASS (style 0xB, WndProc @ 0x4618C0,   */
/* icon resource 101), then creates a top-level popup window sized to   */
/* the full screen dimensions (g_screen_width x g_screen_height).       */
/*                                                                      */
/* In demo mode, uses WS_EX_TOPMOST (0x8); retail uses exStyle 0.       */
/* Window style = WS_POPUP | WS_VISIBLE (0x82000000).                    */
/*                                                                      */
/* Stores the new HWND in this->hWnd (+0x08) and queries client rect   */
/* into the client RECT at 0x485220.                                   */
/*                                                                      */
/* @return TRUE on success, FALSE if CreateWindowEx failed.             */
/* ================================================================== */
BOOL CGWND::RegisterWindowClass()
{
#ifdef _WIN32
    WNDCLASSA wc;

    /* Zero-initialize the WNDCLASSA structure */
    wc.style         = 0;
    wc.lpfnWndProc   = nullptr;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = nullptr;
    wc.hIcon         = nullptr;
    wc.hCursor       = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = nullptr;

    wc.hInstance     = this->hInstance;              /* +0x0C */
    wc.style         = 0xB;                          /* CS_BYTEALIGNCLIENT | CS_HREDRAW | CS_VREDRAW */
    wc.lpfnWndProc   = &CGWND_MainWndProc;           /* real trampoline, was (WNDPROC)0x4618C0 */
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hIcon         = LoadIconA(this->hInstance, (LPCSTR)0x65);  /* IDI_APPLICATION = 101 */
    wc.hCursor       = nullptr;                      /* no cursor — handled by game */
    wc.hbrBackground = nullptr;                      /* no background brush */
    wc.lpszMenuName  = nullptr;                      /* no menu */
    wc.lpszClassName = s_LEGO_LOCO_0047e1c0;         /* "LEGO_LOCO" */

    RegisterClassA(&wc);

    /* Assembly: (-(g_demo_mode != 1) & 0xfffffff8) + 8.
     * The 32-bit addition wraps to 0 for retail and yields
     * WS_EX_TOPMOST (0x8) for demo mode.
     */
    uint32_t exStyle = (g_demo_mode != 1) ? 0 : 0x8;

    this->hWnd = CreateWindowExA(
        exStyle,
        s_LEGO_LOCO_0047e1c0,          /* class name */
        s_LEGO_LOCO_0047e1c0,          /* window title */
        0x82000000,                     /* WS_POPUP | WS_VISIBLE */
        0, 0,                           /* position */
        g_screen_width,                 /* width */
        g_screen_height,                /* height */
        nullptr,                        /* no parent */
        nullptr,                        /* no menu */
        this->hInstance,                /* hInstance */
        nullptr);                       /* lpParam */

    if (this->hWnd == nullptr) {
        return FALSE;
    }

    /* Query client rect at 0x485220. */
    GetClientRect(this->hWnd, &g_client_rect);
    return TRUE;

#else /* !_WIN32 — SDL3, routed through the real Win32 message-pump         */
      /* emulation layer (graphics/sdl3_window.cpp) rather than grabbing    */
      /* SDL3_GetWindow() directly, so DispatchMessageA can actually reach  */
      /* CGWND_MainWndProc once something drives that pump. The real       */
      /* SDL_Window (created by SDL3_WindowInit in main.cpp) stays reachable*/
      /* via SDL3_GetWindow() for the call sites that need it directly     */
      /* (SDL3_GetRenderer, SDL3_DisplayToPrimaryCanvas*, etc.) — nothing   */
      /* in this tree treats CGWND::hWnd as an SDL_Window* (confirmed via   */
      /* grep before this change), so becoming a shim HWND here is safe.   */

    WNDCLASSA wc;
    wc.style         = 0xB;
    wc.lpfnWndProc   = &CGWND_MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = this->hInstance;
    wc.hIcon         = nullptr;
    wc.hCursor       = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = s_LEGO_LOCO_0047e1c0;

    RegisterClassA(&wc);

    this->hWnd = CreateWindowExA(
        0, s_LEGO_LOCO_0047e1c0, s_LEGO_LOCO_0047e1c0,
        0x82000000 /* WS_POPUP | WS_VISIBLE */,
        0, 0, g_screen_width, g_screen_height,
        nullptr, nullptr, this->hInstance, nullptr);

    if (this->hWnd == nullptr) {
        return FALSE;
    }

    /* Make the real SDL window (created earlier by SDL3_WindowInit)
     * visible and size the client rect from it — the shim HWND above is
     * an opaque registry key, not a real window; presentation still goes
     * through the one real SDL_Window. */
    SDL_Window* win = SDL3_GetWindow();
    int w = g_screen_width;
    int h = g_screen_height;
    if (win != nullptr) {
        SDL_ShowWindow(win);
        SDL_GetWindowSize(win, &w, &h);
    }
    g_client_rect.left   = 0;
    g_client_rect.top    = 0;
    g_client_rect.right  = w;
    g_client_rect.bottom = h;

    return TRUE;

#endif /* _WIN32 */
}


/* ================================================================== */
/* CGWND::WndProc and friends — the main window's real WNDPROC          */
/* Address: 0x4618C0 (Ghidra: FUN_004618c0, renamed CGWND_MainWndProc)  */
/*                                                                      */
/* Session findings that correct/extend the original dispatch prompt's */
/* description (see PROGRESS.md for the full writeup):                 */
/*   - WM_CLOSE (0x10) has TWO distinct handling sites, one per mode    */
/*     group, not one: in modes {0,1,2} it always tears down (with an  */
/*     error dialog first when wParam != 0); in every other mode it     */
/*     opens TrainStationWindow unless mode is already 10 or demo mode  */
/*     is active, in which case it defers to the same real shutdown.    */
/*   - The real shutdown routine (0x463430, renamed                     */
/*     CGWND_ShutdownOrDeferToMode10) is NOT unconditional teardown: for */
/*     any mode other than 1 or 10 it just calls CGWND_SetMode(10) and   */
/*     returns immediately, deferring to the async two-step quit         */
/*     machinery (CGWND_SetMode(10) eventually re-posts WM_CLOSE once    */
/*     the mode has settled) — only mode 1/10 do the real teardown.      */
/*   - No live PostMessageA/SendMessageA call site anywhere in this tree */
/*     posts messages 0x401/0x402/0x403/0x404/0x408 with an object       */
/*     argument, so several sub-cases are genuinely dead code today;     */
/*     they are still dispatched faithfully (counters, DefWindowProcA    */
/*     forwarding) but their vtable/receiver-dependent payload is        */
/*     explicitly left as a documented blocked case rather than guessed. */
/* ================================================================== */

/* DDRAW_PresentRect (0x401280) — canonically declared in graphics/
 * LOCOBITMAP.h, already included above under _WIN32 (see this file's own
 * host-menu-bootstrap include guard higher up), so only the host (#else)
 * shape needs re-declaring here. A _WIN32-guarded copy of the other shape
 * would be a pure duplicate of LOCOBITMAP.h's own declaration — harmless
 * today but a needless second place for the signature to drift. */
#ifndef _WIN32
extern void DDRAW_PresentRect(void* rect, void* hwnd, int* scroll, int force);
#endif

extern void WIN32_PostQuit(void);

#ifndef _WIN32
/* extern "C" required: shared/link_stubs.cpp defines both inside its own
 * extern "C" block, so plain C++ (mangled) declarations here would look
 * for a symbol that doesn't exist — silently resolved to a call through
 * address 0 under this project's --unresolved-symbols=ignore-all link
 * setting, rather than a link error (found via a live SIGSEGV, `call
 * 0x0`, the first time CGWND_CloseAllSubwindows was ever actually
 * executed — see PROGRESS.md). */
extern "C" int32_t SendMessageA(void*, uint32_t, uint32_t, int32_t);   /* shared/link_stubs.cpp — host no-op */
extern "C" int32_t GetWindowRect(HWND, RECT*);                          /* shared/link_stubs.cpp — host no-op */
#endif

/* New globals this reconstruction introduces (all previously unnamed
 * Ghidra DAT_* addresses with no prior declaration anywhere in this
 * tree, confirmed via repeated grep before adding). */
uint8_t  g_present_due = 0;                    /* 0x4AA4A4 — set by WM_TIMER
                                                 * id 0x47; the future main-
                                                 * loop rebuild (tracked in
                                                 * PROGRESS.md) is what will
                                                 * consume this to gate
                                                 * CGWND_Present(0). */
static uint8_t  g_wndproc_displaychange_guard = 0;  /* 0x4FF138 — WM_DISPLAYCHANGE reentry guard */
static uint8_t  g_wndproc_sizing_active = 0;         /* 0x4FF13C — WM_ENTERSIZEMOVE/EXITSIZEMOVE guard,
                                                        * also read by WM_NCHITTEST */
static uint32_t g_winmain_frame_counter_402 = 0;     /* 0x4FF12C — WinMain's own per-iteration counter */
static uint32_t g_winmain_frame_counter_403 = 0;     /* 0x4FF118 */
static uint32_t g_winmain_frame_counter_404 = 0;     /* 0x4FF128 */

/* ================================================================== */
/* CGWND_ShutdownOrDeferToMode10                                        */
/* Address: 0x463430 (Ghidra: FUN_00463430)                             */
/*                                                                      */
/* Only actually tears down when g_game_mode is already 1 or 10;        */
/* otherwise defers by requesting the async mode-10 quit transition     */
/* (CGWND_SetMode(10) plays the exit sound and posts WM_CLOSE itself    */
/* once ready — see core/CGWND.cpp's CGWND_SetMode case 10) and returns */
/* without doing anything else.                                        */
/* ================================================================== */
void CGWND_CloseAllSubwindows();   /* 0x4634F0 — forward decl, defined below */

static void CGWND_ShutdownOrDeferToMode10()
{
    CGWND* self = static_cast<CGWND*>(g_main_window);

    if (g_game_mode == 1 || g_game_mode == 10) {
        if (self != nullptr && self->hWnd != nullptr) {
            ShowWindow(self->hWnd, 0 /* SW_HIDE */);
            WIN32_PostQuit();
        }
        /* falls through to the real teardown below */
    } else if (self != nullptr) {
        CGWND_SetMode(10);
        return;
    }

    CGWND_CloseAllSubwindows();

    if (self != nullptr) {
        CGWND_Cleanup();
        if (self->hWnd != nullptr) {
            DestroyWindow(self->hWnd);
            self->hWnd = nullptr;
        }
    }

    extern void WIN32_CloseThreadHandle(void* h);
    extern void* g_async_task_queue;
    WIN32_CloseThreadHandle(&g_async_task_queue);

    if (g_timer_id != 0 && self != nullptr) {
        KillTimer(self->hWnd, static_cast<uintptr_t>(g_timer_id));
        g_timer_id = 0;
    }

    PostQuitMessage(0);
    extern int DAT_00485444;   /* 0x485444 — core/GameLoop.cpp's frame-tick gate */
    DAT_00485444 = 0;
}

/* ================================================================== */
/* CGWND_CloseAllSubwindows                                             */
/* Address: 0x4634F0 (Ghidra: FUN_004634f0)                             */
/*                                                                      */
/* Posts WM_CLOSE to every live subsystem window, and asks the main     */
/* menu's popup (if any) to switch to state 7 first. Called only from   */
/* CGWND_ShutdownOrDeferToMode10's real-teardown path.                  */
/* ================================================================== */
/* network/Netman.h declares this with plain C++ linkage, but its only real
 * callers (native/NETMAN_NetworkUI.c, native/NETMAN_SessionSettings.c) are C
 * files, giving it C linkage there — a pre-existing declared-linkage
 * mismatch (same class of landmine as WNDPROC_CriticalSectionLock, not
 * introduced here). Matching the C files' `extern "C"` linkage gives this
 * call the best chance of resolving to whatever real definition exists; if
 * none does, this is an already-existing gap, not a new one. A linkage-
 * specification like `extern "C"` is only legal at namespace scope, not
 * inside a function body, so this must be file-scope, not local to
 * CGWND_CloseAllSubwindows() below. */
extern "C" void UI_MainMenu_SetState(void* ui_main, int32_t state);

void CGWND_CloseAllSubwindows()
{
    if (g_about != nullptr) {
        SendMessageA(static_cast<AboutDialog*>(g_about)->hWnd, 0x10, 0, 0);
    }
    if (g_audio_mgr != nullptr) {
        SendMessageA(static_cast<HelpWnd*>(g_audio_mgr)->hWnd, 0x10, 0, 0);
    }
    if (g_trainstation_window != nullptr) {
        SendMessageA(static_cast<TrainStationWindow*>(g_trainstation_window)->hWnd, 0x10, 0, 0);
    }
    if (g_cursor != nullptr) {
        SendMessageA(static_cast<Cursor*>(g_cursor)->hWnd, 0x10, 0, 0);
    }
    if (g_town != nullptr) {
        SendMessageA(static_cast<Town*>(g_town)->hWnd, 0x10, 0, 0);
    }
    if (g_postcard != nullptr) {
        SendMessageA(static_cast<PostcardAlbum*>(g_postcard)->hWnd, 0x10, 0, 0);
    }
    if (g_postcard_send != nullptr) {
        SendMessageA(g_postcard_send->hWnd, 0x10, 0, 0);
    }
    if (g_ui_main != nullptr) {
        EditWindow* ui = static_cast<EditWindow*>(g_ui_main);
        if (ui->pPanelB != nullptr) {
            SendMessageA(ui->pPanelB->hWnd, 0x10, 0, 0);
        }
        if (ui->pPanelA != nullptr) {
            SendMessageA(ui->pPanelA->hWnd, 0x10, 0, 0);
        }
        if (ui->pPopupWindow != nullptr) {
            UI_MainMenu_SetState(g_ui_main, 7);
        }
        SendMessageA(ui->hWnd, 0x10, 0, 0);
    }
}

/* ================================================================== */
/* CGWND_EnterGameplayPresentation                                      */
/* Address: 0x45E400 (Ghidra: FUN_0045e400) — WM_USER+5 (0x405) handler */
/*                                                                      */
/* Signals a frame present, switches to mode 3, tears down the splash   */
/* audio config/DirectSound objects, then restores window focus.        */
/* ================================================================== */
static void CGWND_EnterGameplayPresentation()
{
    CGWND* self = static_cast<CGWND*>(g_main_window);
    SendMessageA(self->hWnd, 0x407, 1, 0);
    CGWND_SetMode(3);

    extern void* _g_audio_config;
    if (_g_audio_config != nullptr) {
        destroy_subsystem(_g_audio_config);
        _g_audio_config = nullptr;
    }

    /* Original decompile re-checks the same object this method's sibling
     * (CGWND_PresentLoadingSpinner) draws every WM_USER+7 tick — the
     * loading-screen shadow Entity is torn down here, once real gameplay
     * has actually started (get_xrefs_to on its real address, 0x4FD3D8,
     * showed both handlers reading/writing it). Its real concrete type
     * (Entity*, placement-new-constructed in network/DirectPlay.cpp) is
     * known, so a real `delete` replaces the original's raw
     * vtable[0](1) scalar-deleting-destructor dispatch. */
    extern void* _g_dsound_object;
    if (_g_dsound_object != nullptr) {
        delete static_cast<Entity*>(_g_dsound_object);
        _g_dsound_object = nullptr;
    }

    PlaySoundA(nullptr, nullptr, 0);
    EnableWindow(self->hWnd, TRUE);
    SetFocus(self->hWnd);
}

/* ================================================================== */
/* CGWND_PresentLoadingSpinner                                          */
/* Address: 0x45E210 (Ghidra: FUN_0045e210) — WM_USER+7 (0x407) handler */
/*                                                                      */
/* Clears the "present due" flag, then advances/redraws the loading-    */
/* screen shadow entity (_g_dsound_object) once (flags == 0) or plays a  */
/* short animated 4-frame "spin up" sequence (flags != 0) before         */
/* settling. Entity vtable slots confirmed against core/Entity.h:        */
/* +0x1C StopSound(int), +0x28 Update(), +0x2C Draw(RECT,int,uint32_t). */
/* ================================================================== */
static void CGWND_PresentLoadingSpinner(char flags)
{
    g_present_due = 0;

    extern void* _g_dsound_object;   /* 0x4FD3D8 — shadow GameObject, real
                                       * type Entity* (network/DirectPlay.cpp) */
    if (_g_dsound_object == nullptr) {
        return;
    }
    Entity* shadow = static_cast<Entity*>(_g_dsound_object);
    HWND main_hwnd = static_cast<CGWND*>(g_main_window)->hWnd;

    auto draw_and_present = [&]() {
        shadow->Draw(shadow->screen_rect, 0, 0);
        DDRAW_PresentRect(&shadow->screen_rect, main_hwnd, nullptr, 0);
    };

    if (flags == 0) {
        shadow->Update();
        draw_and_present();
        return;
    }

    shadow->StopSound(1);
    draw_and_present();
    Sleep(0x4B);
    for (int i = 0; i < 2; ++i) {
        shadow->Update();
        draw_and_present();
        Sleep(0x4B);
    }
    shadow->Update();
    draw_and_present();
    Sleep(0xFA);
}

/* ================================================================== */
/* CGWND::HandleStartupModeMessage — modes {0,1,2} curated message set  */
/* Address: 0x4618C0's startup-mode branch                              */
/* ================================================================== */
bool CGWND::HandleStartupModeMessage(HWND hWnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam, LRESULT* out)
{
    switch (msg) {
    case 0xF: {  /* WM_PAINT */
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        DDRAW_PresentRect(&ps.rcPaint, this->hWnd, nullptr, 0);
        EndPaint(hWnd, &ps);
        *out = 0;
        return true;
    }
    case 1:   /* WM_CREATE */
    case 2:   /* WM_DESTROY */
        *out = 0;
        return true;

    case 0x20:   /* WM_SETCURSOR */
    case 0x200:  /* WM_MOUSEMOVE */
        SetCursor(nullptr);
        *out = 0;
        return true;

    case 0x10: {  /* WM_CLOSE — always tears down in these modes */
        if (wParam != 0) {
            char msgbuf[512] = {};
            g_resmgr.FormatResourceString(0x14a, msgbuf, sizeof(msgbuf));
            MessageBoxA(nullptr, msgbuf, s_LEGO_LOCO_0047e1c0, 0x30);
        }
        CGWND_ShutdownOrDeferToMode10();
        *out = 0;
        return true;
    }

    case 0x112:  /* WM_SYSCOMMAND — only SC_CLOSE matters here */
        if ((wParam & 0xFFF0) == 0xF140) {
            WIN32_PostQuit();
            *out = DefWindowProcA(hWnd, msg, wParam, lParam);
            return true;
        }
        return false;

    case 0x100:  /* WM_KEYDOWN — no-op while loading/menu */
        *out = 0;
        return true;

    case 0x113:  /* WM_TIMER — only id 0x47 (loading spinner) matters */
        if (wParam == 0x47) {
            g_present_due = 1;
        }
        *out = 0;
        return true;

    case 0x207:  /* WM_MBUTTONDOWN */
    case 0x201:  /* WM_LBUTTONDOWN */
    case 0x203:  /* WM_LBUTTONDBLCLK */
    case 0x204:  /* WM_RBUTTONDOWN */
        *out = 0;
        return true;

    case 0x405:  /* WM_USER+5 */
        Sleep(0x14);
        CGWND_EnterGameplayPresentation();
        *out = 0;
        return true;

    case 0x407:  /* WM_USER+7 */
        CGWND_PresentLoadingSpinner(static_cast<char>(wParam));
        *out = 0;
        return true;

    default:
        return false;
    }
}

/* ================================================================== */
/* CGWND::HandleSizingMessage — WM_SIZING (0x214)                       */
/* Address: 0x4618C0's WM_SIZING block                                  */
/*                                                                      */
/* Only does real work while a fullscreen window is being resized       */
/* (rare in practice — fullscreen windows aren't user-resizable on      */
/* most platforms). Clamps the drag rect to the world size, re-syncs    */
/* scroll position and dirty rects, and updates the trainstation        */
/* tooltip if one is active.                                            */
/* ================================================================== */
LRESULT CGWND::HandleSizingMessage(LPARAM lParam)
{
    if (g_is_fullscreen == 0) {
        return 1;
    }

    // ABI_BOUNDARY: WM_SIZING's real Win32 contract packs a RECT*
    // directly into LPARAM; this is the message's own wire format,
    // not a modeled game object.
    RECT* rect = reinterpret_cast<RECT*>(static_cast<uintptr_t>(lParam));
    int cx_frame = GetSystemMetrics(2);
    int cy_frame = GetSystemMetrics(3);
    DWORD style = GetWindowLongA(this->hWnd, -16 /* GWL_STYLE */);

    if (g_world_width < rect->right - rect->left) {
        RECT probe = {0, 0, g_world_width, rect->bottom - rect->top};
        AdjustWindowRect(&probe, style, FALSE);
        if ((probe.right + cx_frame) - probe.left < rect->right - rect->left) {
            rect->right = (probe.right + cx_frame - probe.left) + rect->left;
        }
        g_window_left = rect->left; g_window_top = rect->top;
        g_window_right = rect->right; g_window_bottom = rect->bottom;
    }

    {
        RECT probe = {0, 0, 0x80, rect->bottom - rect->top};
        AdjustWindowRect(&probe, style, FALSE);
        if (rect->right - rect->left < (probe.right + cx_frame) - probe.left) {
            rect->right = (probe.right + cx_frame - probe.left) + rect->left;
            g_window_left = rect->left; g_window_top = rect->top;
            g_window_right = rect->right; g_window_bottom = rect->bottom;
        }
    }

    if (g_world_height < rect->bottom - rect->top) {
        RECT probe = {0, 0, rect->right - rect->left, g_world_height};
        AdjustWindowRect(&probe, style, FALSE);
        if ((probe.bottom + cy_frame) - probe.top < rect->bottom - rect->top) {
            rect->bottom = (probe.bottom + cy_frame - probe.top) + rect->top;
        }
        g_window_left = rect->left; g_window_top = rect->top;
        g_window_right = rect->right; g_window_bottom = rect->bottom;
    }

    {
        RECT probe = {0, 0, rect->right - rect->left, 0x80};
        AdjustWindowRect(&probe, style, FALSE);
        int probe_h = (probe.bottom + cy_frame) - probe.top;
        if (rect->bottom - rect->top < probe_h) {
            rect->bottom = probe_h + rect->top;
            g_window_left = rect->left; g_window_top = rect->top;
            g_window_right = rect->right; g_window_bottom = rect->bottom;
        }
    }

    int dx = (g_world_width - g_viewport_x < rect->right - rect->left)
                 ? (g_world_width - g_viewport_x + rect->left - rect->right) : 0;
    CGWND_ScrollHorizontal(dx);
    int dy = (g_world_height - g_viewport_y < rect->bottom - rect->top)
                 ? (g_world_height - g_viewport_y + rect->top - rect->bottom) : 0;
    CGWND_ScrollVertical(dy);

    extern void TileMap_InvalidateRect(TileMap* tm, int l, int t, int r, int b);
    extern void TileMap_InvalidateDirtyRects(TileMap* tm, char flag);

    RECT edge1 = {g_window_left, rect->top, g_window_right, g_window_top};
    OffsetRect(&edge1, g_viewport_x, g_viewport_y);
    if (edge1.top < edge1.bottom) {
        TileMap_InvalidateRect(g_tilemap, edge1.left, edge1.top, edge1.right, edge1.bottom);
    }

    RECT edge2 = {rect->left, g_window_top, g_window_left, g_window_bottom};
    OffsetRect(&edge2, g_viewport_x, g_viewport_y);
    if (edge2.left < edge2.right) {
        TileMap_InvalidateRect(g_tilemap, edge2.left, edge2.top, edge2.right, edge2.bottom);
    }

    RECT edge3 = {g_window_right, g_window_top, rect->right, g_window_bottom};
    OffsetRect(&edge3, g_viewport_x, g_viewport_y);
    if (edge3.left < edge3.right) {
        TileMap_InvalidateRect(g_tilemap, edge3.left, edge3.top, edge3.right, edge3.bottom);
    }

    RECT edge4 = {g_window_left, g_window_bottom, g_window_right, rect->bottom};
    OffsetRect(&edge4, g_viewport_x, g_viewport_y);
    if (edge4.top < edge4.bottom) {
        TileMap_InvalidateRect(g_tilemap, edge4.left, edge4.top, edge4.right, edge4.bottom);
    }

    extern int32_t g_client_width;   /* world/tilemap.h — same underlying
                                       * memory as g_client_rect, see that
                                       * header's own aliasing note */
    extern int32_t g_client_height;
    {
        RECT wr = {};
        GetWindowRect(this->hWnd, &wr);
        g_window_left = wr.left; g_window_top = wr.top;
        g_window_right = wr.right; g_window_bottom = wr.bottom;
    }
    {
        RECT cr = {};
        GetClientRect(this->hWnd, &cr);
        g_client_width = cr.right; g_client_height = cr.bottom;
        g_client_offset_x = cr.left; g_client_offset_y = cr.top;
    }

    extern void Sprite_LockAll(TileMap* tm);
    Sprite_LockAll(g_tilemap);
    TileMap_InvalidateDirtyRects(g_tilemap, 1);

    if (g_trainstation_window != nullptr &&
        static_cast<TrainStationWindow*>(g_trainstation_window)->tooltip_active != 0) {
        /* TrainStationWindow_UpdateTooltip is a pre-existing loud-deferred
         * stub (shared/stubs_link001_batch5_ui_graphics.cpp) — it takes
         * `this` as a plain `int` (a documented pre-existing 32-bit-only
         * pointer-width hazard, not introduced here) and only logs a
         * warning; not fixed as part of this pass. */
        extern void TrainStationWindow_UpdateTooltip(int thisPtr);
        TrainStationWindow_UpdateTooltip(
            static_cast<int>(reinterpret_cast<uintptr_t>(g_trainstation_window)));
    }
    return 1;
}

/* ================================================================== */
/* CGWND::HandleUserCommandMessage — WM_USER+1 (0x401) sub-switch        */
/* Address: 0x4618C0's 0x401 case                                       */
/* ================================================================== */
/* ui/UIPANEL_Draw.cpp declares these inside `extern "C" { }` with their
 * original x86 calling conventions — match linkage AND convention here, or
 * the MinGW typecheck build (which compiles this file under _WIN32, where
 * __fastcall/__thiscall are real GCC attributes, not no-ops) will see
 * conflicting declarations of the same extern "C" symbol. A linkage-
 * specification like `extern "C"` is only legal at namespace scope, not
 * inside a function body, so this must be file-scope, not local to
 * CGWND::HandleUserCommandMessage below. */
extern "C" {
    void __fastcall UIPANEL_InitSprite(void* self);
    void __fastcall UIPANEL_BlitSprite(void* self);
    void __fastcall UIPANEL_BlitSpriteEx(void* self);
    void __thiscall UIPANEL_Hide(void* self, const char* filename);
}

bool CGWND::HandleUserCommandMessage(WPARAM wParam, LPARAM lParam, LRESULT* out)
{
    extern uint8_t g_second_overlay_bounds[];  /* 0x4AA818 (core/Game.cpp) */

    switch (wParam) {
    case 0: {
        g_scripted_object->EnterBuildMode(0);     /* 0x44A9D0, world/scriptengine.h */

        extern void* g_town_view;                /* 0x4852A0 */
        extern void* g_ddraw_building;            /* 0x4A9EF0 */
        extern int   Town_SelectBuilding(void*, int);
        extern int   DDRAW_SelectBuilding(void*, int);
        Town_SelectBuilding(g_town_view, 0);
        DDRAW_SelectBuilding(g_ddraw_building, 0);

        /* Original also checks `g_main_window == 0` here; every reachable
         * call already passed WndProc's own hWnd-vs-g_main_window guard,
         * so that check is provably always false at this point (confirmed
         * via get_xrefs_to: nothing between here and function entry reads
         * 0x4AA4A0 again) — simplified to the unconditional branch. */
        CGWND_QuitToMenu();
        *out = 0;
        return true;
    }

    case 5:
        UIPANEL_InitSprite(g_second_overlay_bounds);
        *out = 0;
        return true;
    case 6:
        UIPANEL_BlitSprite(g_second_overlay_bounds);
        *out = 0;
        return true;
    case 7:
        UIPANEL_BlitSpriteEx(g_second_overlay_bounds);
        *out = 0;
        return true;

    case 8:
        if (lParam == 0) {
            *out = 0;
            return true;
        }
        static_cast<Town*>(g_town)->net_update_flag = 1;
        CGWND_SetMode(5);
        *out = 0;
        return true;

    case 9: {
        extern char INPUT_SaveCurrentWorld(InputMgr*, const char*);
        extern char INPUT_LoadWorld(InputMgr*, const char*);
        INPUT_SaveCurrentWorld(&g_input_mgr, "curr");
        // ABI_BOUNDARY: WM_USER+1 case 9's LPARAM is a raw filename
        // string pointer per the original message's own contract.
        UIPANEL_Hide(g_second_overlay_bounds, reinterpret_cast<const char*>(static_cast<uintptr_t>(lParam)));
        INPUT_LoadWorld(&g_input_mgr, "curr");
        *out = 0;
        return true;
    }

    case 0xA:
        /* Decompile literally passes g_postcard here, despite
         * CGWND_GameSetup_RenderPlayerSlots's own doc comment describing
         * a GameSetupPanel receiver — no live poster of 0x401/0xA exists
         * in this tree to cross-check against; preserved as decompiled. */
        CGWND_GameSetup_RenderPlayerSlots(g_postcard);
        *out = 0;
        return true;

    case 0xB:
        /* BLOCKED — lParam's object type (indexed at +0x120 to reach a
         * Vehicle*, per World_GetObjectAt/World_RenderAll's signatures)
         * is not evidenced anywhere else in this tree, and this exact
         * sub-case has no live poster to cross-check against. Not
         * guessed; see this method's doc comment and PROGRESS.md. */
        *out = 0;
        return true;

    default:
        *out = 0;
        return true;
    }
}

/* ================================================================== */
/* CGWND::HandleGameplayMessage — every mode other than {0,1,2}          */
/* Address: 0x4618C0's gameplay branch                                  */
/* ================================================================== */
bool CGWND::HandleGameplayMessage(HWND hWnd, UINT msg, WPARAM wParam,
                                   LPARAM lParam, LRESULT* out)
{
    extern void Game_SetScreenMode(void* game, int a, int b, int c);
    extern void* g_active_panel;      /* 0x4FD3E0 — active UI panel override */
    extern uint8_t g_has_selection;   /* 0x4854EC (world/tilemap.h) */

    bool quit_to_menu_requested = false;

    switch (msg) {
    case 0x14:  /* WM_ERASEBKGND */
        *out = 1;
        return true;

    case 0x10:  /* WM_CLOSE */
        /* FALLBACK, not the real original behavior, when mode != 10 and
         * not in demo mode: the real path here opens the TrainStationWindow
         * hub instead of quitting. TrainStationWindow::show() (0x436EC0) is
         * real, integrated code, but its sound-loading step unconditionally
         * calls RESMGR_LoadSoundResource (0x448D60) — documented in
         * PROGRESS.md's Priority-2 list as still having no real
         * implementation ("entry is garbage and entry->is_valid is a wild
         * read"). Calling into it from a live WM_CLOSE handler would risk a
         * crash/UB, not just an unimplemented feature, so this deliberately
         * falls back to closing instead of opening the hub in both
         * branches — matching this reconstruction's own documented scope
         * boundary (TrainStationWindow hub explicitly out of scope until
         * RESMGR_LoadSoundResource is real). Revisit once that dependency
         * is implemented. */
        CGWND_ShutdownOrDeferToMode10();
        *out = 0;
        return true;

    case 0x84: {  /* WM_NCHITTEST */
        LRESULT hit = DefWindowProcA(hWnd, msg, wParam, lParam);
        if (g_wndproc_sizing_active == 0) {
            if (hit == 1 /* HTCLIENT */) {
                if (g_has_selection == 0) {
                    Game_SetScreenMode(g_game, 1, 1, 0);
                    *out = 1;
                    return true;
                }
            } else if (g_has_selection != 0) {
                Game_SetScreenMode(g_game, 0, 1, 0);
            }
        }
        *out = hit;
        return true;
    }

    case 0x48:  /* WM_POWER */
        quit_to_menu_requested = (wParam == 1);
        break;

    case 0x112: {  /* WM_SYSCOMMAND */
        uint32_t masked = wParam & 0xFFF0;
        if (masked == 0xF030) {  /* SC_MAXIMIZE */
            if (g_is_fullscreen == 0) { *out = 0; return true; }
            CGWND_SetFullscreenMode(1);
            *out = 0;
            return true;
        }
        if (masked == 0xF060) {  /* SC_MINIMIZE */
            PostMessageA(this->hWnd, 0x10, 0, 0);
            *out = 0;
            return true;
        }
        if (masked == 0xF140) {  /* SC_CLOSE */
            WIN32_PostQuit();
            *out = DefWindowProcA(hWnd, msg, wParam, lParam);
            return true;
        }
        return false;
    }

    case 0x100: {  /* WM_KEYDOWN */
        if (wParam < 0x20 || wParam > 0x5A) {
            if (g_active_panel != nullptr) {
                uint32_t handled = static_cast<Panel*>(g_active_panel)->HandleKey(static_cast<int>(wParam));
                if (handled != 0) { *out = 0; return true; }
            }
            if (wParam == 0x1B) {  /* VK_ESCAPE */
                PostMessageA(this->hWnd, 0x10, 0, 0);
                *out = 0;
                return true;
            }
            /* BLOCKED — VK_RETURN (0xD) build-mode entry: the decompiled
             * call `Panel_Init(&g_scripted_object, 0x2400, 1, 0)` matches
             * Panel::Init's real signature, but world/scriptengine.h's
             * RESDATA_ScriptedObject (the only g_scripted_object typed in
             * this tree) is NOT Panel-derived and already declares its
             * own 0-arg Init() — a genuine receiver/signature conflict,
             * not resolved here. A second condition, `wParam ==
             * 0x564B5F51`, could not be verified against disassembly
             * (out of this session's captured range) and is not encoded.
             */
        }
        return false;
    }

    case 0x102: {  /* WM_CHAR */
        if (wParam > 0x1F && wParam < 0x7F) {
            WPARAM ch = wParam;
            if (ch > 0x60 && ch < 0x7B) {
                ch = static_cast<WPARAM>(CRT_toupper(static_cast<int>(ch)));
            }
            if (g_active_panel == nullptr ||
                static_cast<Panel*>(g_active_panel)->HandleKey(static_cast<int>(ch)) == 0) {
                if (ch == 0x51) {  /* 'Q' */
                    PostMessageA(this->hWnd, 0x10, 0, 0);
                    *out = 0;
                    return true;
                }
                if (ch == 0x57) {  /* 'W' */
                    CGWND_ToggleFullscreen();
                    *out = DefWindowProcA(hWnd, msg, 0x57, lParam);
                    return true;
                }
            }
        }
        return false;
    }

    case 0x200: {  /* WM_MOUSEMOVE */
        if (g_game_mode == 8) {
            PostMessageA(static_cast<HelpWnd*>(g_audio_mgr)->hWnd, 0x200, wParam, lParam);
            *out = DefWindowProcA(hWnd, msg, wParam, lParam);
            return true;
        }
        Game* game = static_cast<Game*>(g_game);
        game->screensaver_active = 1;
        game->packed_mouse_pos = static_cast<uint32_t>(lParam);
        if (g_game_mode == 4 && game->resource != nullptr &&
            GetResourceType(static_cast<RESDATA*>(game->resource)->resource_id) != 5) {
            game->Update();
            *out = DefWindowProcA(hWnd, msg, wParam, lParam);
            return true;
        }
        return false;
    }

    case 0x114: {  /* WM_HSCROLL */
        int16_t pos = static_cast<int16_t>(wParam >> 16);
        switch (wParam & 0xFFFF) {
        case 0: CGWND_ScrollHorizontal(-4); break;
        case 1: CGWND_ScrollHorizontal(4); break;
        case 2: CGWND_ScrollHorizontal(-0x100); break;
        case 3: CGWND_ScrollHorizontal(0x100); break;
        case 4: CGWND_ScrollHorizontal(pos - g_viewport_x); break;
        default: return false;
        }
        *out = DefWindowProcA(hWnd, msg, wParam, lParam);
        return true;
    }

    case 0x115: {  /* WM_VSCROLL */
        int16_t pos = static_cast<int16_t>(wParam >> 16);
        switch (wParam & 0xFFFF) {
        case 0: CGWND_ScrollVertical(-4); break;
        case 1: CGWND_ScrollVertical(4); break;
        case 2: CGWND_ScrollVertical(-0x100); break;
        case 3: CGWND_ScrollVertical(0x100); break;
        case 4: CGWND_ScrollVertical(pos - g_viewport_y); break;
        default: return false;
        }
        *out = DefWindowProcA(hWnd, msg, wParam, lParam);
        return true;
    }

    case 0x214:  /* WM_SIZING */
        *out = this->HandleSizingMessage(lParam);
        return true;

    /* Cases 0x201/0x203/0x202/0x204/0x206/0x205 write directly into
     * Game's own per-frame input fields (core/Game.h) — confirmed
     * against core/CGWND_sdl3.cpp's own doc comment, which already
     * documents this exact assembly address range (0x462380-0x462426)
     * against the same fields for its bespoke SDL mouse-dispatch path. */
    case 0x201:  /* WM_LBUTTONDOWN */
    case 0x203:  /* WM_LBUTTONDBLCLK */
        SetFocus(this->hWnd);
        SetForegroundWindow(this->hWnd);
        static_cast<Game*>(g_game)->click_on_selected = 1;
        static_cast<Game*>(g_game)->left_click_flag = 1;
        static_cast<Game*>(g_game)->left_click_screen_pos = static_cast<uint32_t>(lParam);
        *out = DefWindowProcA(hWnd, msg, wParam, lParam);
        return true;

    case 0x202:  /* WM_LBUTTONUP */
        static_cast<Game*>(g_game)->click_on_selected = 0;
        static_cast<Game*>(g_game)->mouse_move_flag = 1;
        static_cast<Game*>(g_game)->mouse_move_screen_pos = static_cast<uint32_t>(lParam);
        *out = DefWindowProcA(hWnd, msg, wParam, lParam);
        return true;

    case 0x204:  /* WM_RBUTTONDOWN */
    case 0x206:  /* WM_RBUTTONDBLCLK */
        static_cast<Game*>(g_game)->right_click_flag = 1;
        static_cast<Game*>(g_game)->right_click_screen_pos = static_cast<uint32_t>(lParam);
        *out = DefWindowProcA(hWnd, msg, wParam, lParam);
        return true;

    case 0x205:  /* WM_RBUTTONUP */
        static_cast<Game*>(g_game)->mouse_drag_flag = 1;
        static_cast<Game*>(g_game)->mouse_drag_screen_pos = static_cast<uint32_t>(lParam);
        *out = DefWindowProcA(hWnd, msg, wParam, lParam);
        return true;

    case 0x401:  /* WM_USER+1 */
        return this->HandleUserCommandMessage(wParam, lParam, out);

    case 0x232:  /* WM_EXITSIZEMOVE */
        g_wndproc_sizing_active = 0;
        if (g_is_fullscreen == 0) {
            *out = 0;
            return true;
        }
        {
            RECT wr = {};
            GetWindowRect(this->hWnd, &wr);
            g_window_left = wr.left; g_window_top = wr.top;
            g_window_right = wr.right; g_window_bottom = wr.bottom;
            extern int32_t g_client_width;
            extern int32_t g_client_height;
            RECT cr = {};
            GetClientRect(this->hWnd, &cr);
            g_client_width = cr.right; g_client_height = cr.bottom;
            g_client_offset_x = cr.left; g_client_offset_y = cr.top;
        }
        {
            extern void Sprite_LockAll(TileMap* tm);
            extern void TileMap_InvalidateRect(TileMap* tm, int l, int t, int r, int b);
            extern void TileMap_InvalidateDirtyRects(TileMap* tm, char flag);
            Sprite_LockAll(g_tilemap);
            CGWND_ScrollHorizontal(0);
            CGWND_ScrollVertical(0);
            TileMap_InvalidateRect(g_tilemap, g_viewport_rect_left, g_viewport_rect_top,
                                    g_viewport_rect_right, g_viewport_rect_bottom);
            TileMap_InvalidateDirtyRects(g_tilemap, 1);
        }
        *out = 0;
        return true;

    case 0x402:
        if (g_game_mode != 10 && wParam != 0) {
            ++g_winmain_frame_counter_402;
            /* BLOCKED — vtable[+0x3C] dispatch on the wParam-supplied
             * callback object: no live poster of this message exists in
             * this tree, and the vtable slot's meaning differs per
             * concrete subclass (confirmed via core/GameView.h, which
             * documents its own override at this exact slot as
             * GameView::cleanup — a different class overriding it
             * differently is exactly what makes the receiver
             * undeterminable here). See this method's doc comment. */
            *out = DefWindowProcA(hWnd, msg, wParam, lParam);
            return true;
        }
        return false;

    case 0x403:
        if (g_game_mode != 10) {
            ++g_winmain_frame_counter_403;
            /* BLOCKED — calls a named-but-receiver-ambiguous helper
             * (Ghidra: FUN_00434d70, renamed
             * BuildingMgr_CheckTrainProximity): its own decompiled body
             * needs a `this` with a vtable-backed vehicle-list field at
             * +0x4C, which rules out CGWND (only 0x28 bytes) as the
             * receiver; the disassembly range that would show what
             * register is really loaded into ECX here (0x4624FC-
             * 0x462DA5) was unavailable through this session's tooling. */
            *out = DefWindowProcA(hWnd, msg, wParam, lParam);
            return true;
        }
        return false;

    case 0x404:
        if (g_game_mode != 10) {
            ++g_winmain_frame_counter_404;
            /* BLOCKED — same reasoning as 0x403 (Ghidra: FUN_004202b0,
             * renamed UI_DismissExpiredCounter). */
            *out = DefWindowProcA(hWnd, msg, wParam, lParam);
            return true;
        }
        return false;

    case 0x406:
        if (g_game_mode != 10) {
            g_resmgr.AnimateClock(static_cast<int32_t>(wParam));
        }
        return false;

    case 0x408:
        if (g_game_mode != 10 && wParam != 0) {
            /* BLOCKED — vtable[+0x48] dispatch, same reasoning as 0x402. */
            *out = DefWindowProcA(hWnd, msg, wParam, lParam);
            return true;
        }
        return false;

    case 0x231:  /* WM_ENTERSIZEMOVE */
        g_wndproc_sizing_active = 1;
        *out = 0;
        return true;

    case 0x215:  /* WM_CAPTURECHANGED */
        if (g_has_selection == 0) { *out = 0; return true; }
        // ABI_BOUNDARY: WM_CAPTURECHANGED's real Win32 contract packs
        // the HWND losing capture directly into LPARAM.
        if (reinterpret_cast<HWND>(static_cast<uintptr_t>(lParam)) == this->hWnd) { *out = 0; return true; }
        if (reinterpret_cast<HWND>(static_cast<uintptr_t>(lParam)) ==
            static_cast<HelpWnd*>(g_audio_mgr)->hWnd) { *out = 0; return true; }
        Game_SetScreenMode(g_game, 0, 1, 0);
        *out = 0;
        return true;

    case 0x216:  /* WM_MOVING */
        if (g_trainstation_window == nullptr ||
            static_cast<TrainStationWindow*>(g_trainstation_window)->tooltip_active == 0) {
            *out = 0;
            return true;
        }
        if (g_is_fullscreen == 0) { *out = 0; return true; }
        {
            /* See HandleSizingMessage's identical call for why this is a
             * pre-existing loud-deferred stub, not fixed here. */
            extern void TrainStationWindow_UpdateTooltip(int thisPtr);
            TrainStationWindow_UpdateTooltip(
                static_cast<int>(reinterpret_cast<uintptr_t>(g_trainstation_window)));
        }
        *out = 0;
        return true;

    case 0x218:  /* WM_POWERBROADCAST */
        quit_to_menu_requested = (wParam == 4);
        break;

    default:
        return false;
    }

    if (quit_to_menu_requested) {
        /* Original also checks `g_main_window == 0` — provably always
         * false here, same reasoning as HandleUserCommandMessage's case
         * 0. Simplified to the unconditional call. */
        CGWND_QuitToMenu();
        WIN32_PostQuit();
        *out = 1;
        return true;
    }
    return false;
}

/* ================================================================== */
/* CGWND::WndProc                                                       */
/* Address: 0x4618C0                                                    */
/* ================================================================== */
LRESULT CGWND::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* Entry guard: only the main window itself is handled here. */
    if (hWnd != this->hWnd) {
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    /* Demo-mode screensaver "close on any input" gate. */
    if (g_demo_mode == 1) {
        int filtered = g_scrsaver_mod.FilterMessage(msg, wParam);
        if (filtered != 0) {
            if (filtered == 2) return 0;
            if (filtered == 3) return 1;
            return DefWindowProcA(hWnd, msg, wParam, lParam);
        }
    }

    /* WM_DISPLAYCHANGE (0x7E): something else changed the resolution
     * out from under the game. If it matches what the game already
     * expects, ignore it; otherwise show a fatal error and shut down. */
    if (msg == 0x7E) {
        if (g_wndproc_displaychange_guard != 0) {
            return 0;
        }
        uint32_t new_width  = static_cast<uint32_t>(lParam) & 0xFFFF;
        uint32_t new_height = static_cast<uint32_t>(lParam) >> 16;
        if (new_width == static_cast<uint32_t>(g_screen_width) &&
            new_height == static_cast<uint32_t>(g_screen_height) &&
            wParam == g_screen_bpp) {
            return 0;
        }
        ShowWindow(this->hWnd, 0 /* SW_HIDE */);
        char msgbuf[256] = {};
        g_resmgr.FormatResourceString(0x14b, msgbuf, sizeof(msgbuf));
        g_wndproc_displaychange_guard = 1;
        MessageBoxA(nullptr, msgbuf, s_LEGO_LOCO_0047e1c0, 0);
        CGWND_ShutdownOrDeferToMode10();
        g_wndproc_displaychange_guard = 0;
        return 0;
    }

    LRESULT result = 0;
    if (g_game_mode == 0 || g_game_mode == 1 || g_game_mode == 2) {
        if (this->HandleStartupModeMessage(hWnd, msg, wParam, lParam, &result)) {
            return result;
        }
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    if (this->HandleGameplayMessage(hWnd, msg, wParam, lParam, &result)) {
        return result;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ================================================================== */
/* CGWND_MainWndProc — free-function WNDPROC trampoline                 */
/* Address: 0x4618C0                                                    */
/* ================================================================== */
LRESULT CGWND_MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_main_window == nullptr) {
        /* Can't happen in the real binary (the WNDCLASS is registered by
         * CGWND::RegisterWindowClass, itself a CGWND method — g_main_window
         * always exists by the time any message can arrive), but stay
         * defensive rather than dereference a null CGWND*. */
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return static_cast<CGWND*>(g_main_window)->WndProc(hWnd, msg, wParam, lParam);
}


/* ================================================================== */
/* CGWND_InstallPathInit — Read install path from registry, load INI   */
/* Address: 0x4068D0 — free function; no this pointer                  */
/* ================================================================== */
int CGWND_InstallPathInit()
{
#ifdef _WIN32
    /* TODO: decompile 0x4068D0's real Win32 path (HKEY registry read +
     * PlayerConfig_Ctor("lego.ini") init). Never executed: this codebase
     * only ships the #else (SDL3/POSIX) path below; _WIN32 exists solely
     * so the mingw typecheck build can validate surrounding declarations
     * (see CLAUDE.md's cross/mingw32-typecheck.txt docs — "not linked, not
     * a shippable target"). Loud-fail rather than silently return success
     * per this project's stub policy. */
    std::fprintf(stderr,
        "STUB: CGWND_InstallPathInit (0x4068D0) _WIN32 registry-read path "
        "not implemented\n");
    assert(false && "CGWND_InstallPathInit: _WIN32 path not decompiled");
    return 0;

#else /* !_WIN32 — POSIX with environment variable */

    char   install_base_buf[1280];
    int    path_len;
    int    mkdir_result;

    /* Step 1: Read install path from environment or use default */
    const char* env_path = getenv("LEGO_LOCO_PATH");
    if (env_path != nullptr) {
        lstrcpyA(install_base_buf, env_path);
    } else {
        lstrcpyA(install_base_buf, ".");
    }

    /* Step 2: Build "<install_base>/lego.ini" */
    lstrcatA(install_base_buf, "/");
    lstrcatA(install_base_buf, "lego.ini");

    /* Step 3: Create config object */
    void* player_config_raw = operator_new(sizeof(PlayerConfig))  /* was 0x10C (32-bit) */;
    if (player_config_raw != nullptr) {
        extern void* PlayerConfig_Ctor(void* mem, const char* path);
        g_config_ini = PlayerConfig_Ctor(player_config_raw, install_base_buf);
    } else {
        g_config_ini = nullptr;
    }

    /* Step 4: Read [DIRECTORIES] section */
    extern const char DAT_0047e220[];
    extern const char DAT_0047e224[];
    Config_GetIniString(g_config_ini, "DIRECTORIES", DAT_0047e220, DAT_0047e224,
                        g_install_path, 0x100);
    Config_GetIniString(g_config_ini, "DIRECTORIES", "RemoteRes", &g_empty_string,
                        g_remote_res_path, 0x100);

    /* Step 5: Demo mode */
    if (g_demo_mode == 1) g_remote_res_path[0] = '\0';

    /* Step 6-7: Path cleanup with forward slashes */
    {
        const int rlen = lstrlenA(g_remote_res_path);
        if (rlen == 0 || g_remote_res_path[rlen - 1] != '/')
            lstrcatA(g_remote_res_path, "/");
    }
    {
        const int ilen = lstrlenA(g_install_path);
        if (ilen > 0 && g_install_path[ilen - 1] == '/') {
            g_install_path[ilen - 1] = '\0';
            path_len = ilen - 1;
        } else {
            path_len = ilen;
        }
    }

    mkdir_result = CRT_mkdir(g_install_path);
    lstrcatA(g_install_path, "/");

    // If the INI-derived path is a non-existent Windows path (e.g.
    // d:\loco\art-res), fall back to LEGO_LOCO_DATA + /art-res.
    if (path_len < 3 || (mkdir_result != 0 && path_len > 0)) {
        const char* data_root = getenv("LEGO_LOCO_DATA");
        if (!data_root || !*data_root) data_root = "lego-loco-unpacked";
        lstrcpyA(g_install_path, data_root);
        lstrcatA(g_install_path, "/art-res");
        path_len = lstrlenA(g_install_path);
        mkdir_result = CRT_mkdir(g_install_path);
        lstrcatA(g_install_path, "/");
    }

    return (mkdir_result == 0 && path_len > 2) ? 1 : 0;

#endif /* _WIN32 */
}


/* ================================================================== */
/* InitAllSubsystems — Master subsystem initialization                  */
/* Address: 0x406F90                                                    */
/* ================================================================== */
int CGWND::InitAllSubsystems()
{
    HWND      hWndParent = this->hWnd;         /* +0x08: main game HWND */
    HINSTANCE hInst      = this->hInstance;     /* +0x0C */

#ifndef _WIN32
    /* Host path: construct all 8 original subsystems.  The C++
     * constructors initialise fields and create ButtonSprites without
     * HWND dependencies.  HWND-dependent create() / init_sprites() /
     * InitWindow() calls are handled by the incremental initMode1 PATH A
     * sequence (Town::init_overlay_sprite, Cursor::init_background,
     * PostcardAlbum::InitWindowSurface, PostcardPreviewWindow::
     * init_background), which only need ResourceManager_GetById — already
     * live on the host.
     *
     * Construction order and resource IDs match the original
     * CGWND_InitAllSubsystems (0x406F90).  Error codes -2..-10 mirror
     * the Win32 path's -2..-17 range so GameLoop_Setup receives a
     * non-zero failure indicator. */

    /* 1. UI_MainMenu — EditWindow, res 0x1F8. Ctor: 0x420310 */
    g_ui_main = new EditWindow(hInst, 0x1F8);
    if (g_ui_main == nullptr) return -2;
    if (!static_cast<EditWindow*>(g_ui_main)->create(hWndParent))
        { destroy_subsystem(g_ui_main); return -3; }

    /* 2. Town — 0x6E0 bytes, res 0x1F5. Ctor: 0x42E900 */
    g_town = new Town(hInst, 0x1F5);
    if (g_town == nullptr)
        { destroy_subsystem(g_ui_main); return -4; }

    /* 3. PostcardPreviewWindow — 0x2C4 bytes, res 0x1F7. Ctor: 0x430A90 */
    g_postcard_send = new PostcardPreviewWindow(hInst, 0x1F7);
    if (g_postcard_send == nullptr)
        { destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -5; }

    /* 4. TrainStationWindow — 0x1D4 bytes, res 0x1FC. Ctor: 0x436B20 */
    g_trainstation_window = new TrainStationWindow(hInst, 0x1FC);
    if (g_trainstation_window == nullptr)
        { destroy_subsystem(g_postcard_send); destroy_subsystem(g_town);
          destroy_subsystem(g_ui_main); return -6; }

    /* 5. PostcardAlbum — 0x254 bytes, res 0x1FB. Ctor: 0x401F50 */
    g_postcard = new PostcardAlbum(hInst, 0x1FB);
    if (g_postcard == nullptr)
        { destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send);
          destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -7; }

    /* 6. Cursor — 0x740 bytes, res 0x1FA. Ctor: 0x415980
     * Host: Cursor::init() is guarded by #ifdef _WIN32 (the editor/
     * colour-picker UI loads Edit_colour.dat via file-I/O stubs not
     * yet complete for the host path). */
    g_cursor = new Cursor(hInst, 0x1FA);
    if (g_cursor == nullptr)
        { destroy_subsystem(g_postcard); destroy_subsystem(g_trainstation_window);
          destroy_subsystem(g_postcard_send); destroy_subsystem(g_town);
          destroy_subsystem(g_ui_main); return -8; }

    /* 7. AudioMgr (IS HelpWnd) — 0x3078 bytes, res 0x1FE. Ctor: 0x44F490
     * Host: HelpWnd::init() is guarded by #ifdef _WIN32 (tutorial/
     * help-window file I/O not yet complete for the host path). */
    g_audio_mgr = new HelpWnd(hInst, 0x1FE);
    if (g_audio_mgr == nullptr)
        { destroy_subsystem(g_cursor); destroy_subsystem(g_postcard);
          destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send);
          destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -9; }

    /* 8. AboutDialog — 0x1184 bytes, res 0x1FD. Ctor: 0x40F1C0 */
    g_about = new AboutDialog(hInst, 0x1FD);
    if (g_about == nullptr)
        { destroy_subsystem(g_audio_mgr); destroy_subsystem(g_cursor);
          destroy_subsystem(g_postcard); destroy_subsystem(g_trainstation_window);
          destroy_subsystem(g_postcard_send); destroy_subsystem(g_town);
          destroy_subsystem(g_ui_main); return -10; }

    return 0;
#else
    /* 1. UI_MainMenu — EditWindow, 0x224 bytes, res 0x1F8. Ctor: 0x420310 */
    g_ui_main = new EditWindow(hInst, 0x1F8);
    if (g_ui_main == nullptr) return -2;
    if (!((EditWindow*)g_ui_main)->create(hWndParent))   /* 0x4204D0 */
        { destroy_subsystem(g_ui_main); return -3; }

    /* 2. Town — 0x6E0 bytes, res 0x1F5. Ctor: 0x42E900 */
    g_town = new Town(hInst, 0x1F5);
    if (g_town == nullptr) { destroy_subsystem(g_ui_main); return -4; }
    if (!((Town*)g_town)->init_sprites(hWndParent))       /* 0x42EDB0 */
        { destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -5; }

    /* 3. PostcardPreviewWindow — 0x2C4 bytes, res 0x1F7. Ctor: 0x430A90 */
    g_postcard_send = new PostcardPreviewWindow(hInst, 0x1F7);
    if (g_postcard_send == nullptr)
        { destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -6; }
    if (!((PostcardAlbum*)g_postcard_send)->InitWindow(hWndParent))  /* 0x402520 — shared UI_WindowBase path */
        { destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -7; }

    /* 4. TrainStationWindow — 0x1D4 bytes, res 0x1FC. Ctor: 0x436B20 */
    g_trainstation_window = new TrainStationWindow(hInst, 0x1FC);
    if (g_trainstation_window == nullptr)
        { destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -8; }
    if (!((TrainStationWindow*)g_trainstation_window)->Create(hWndParent))  /* 0x436D00 */
        { destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -9; }

    /* 5. PostcardAlbum — 0x254 bytes on the original x86 layout, res 0x1FB.
     * sizeof(PostcardAlbum) is 0x328 on this 64-bit host (pointer fields
     * widen) — use the real size, not the stale x86 literal. */
    g_postcard = PostcardAlbum::CreateFromResource(
        operator_new(sizeof(PostcardAlbum)), hInst, 0x1FB);   /* 0x401F50 */
    if (g_postcard == nullptr)
        { destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -10; }
    if (!((PostcardAlbum*)g_postcard)->InitWindow(hWndParent))  /* 0x402520 */
        { destroy_subsystem(g_postcard); destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -11; }

    /* 6. Cursor — 0x740 bytes, res 0x1FA. Ctor: 0x415980 */
    g_cursor = new Cursor(hInst, 0x1FA);
    if (g_cursor == nullptr)
        { destroy_subsystem(g_postcard); destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -12; }
    /* Cursor has no direct create(HWND); uses UI_WindowBase::OnCreate via vtable[6] */
    {
        void** vt = *(void***)g_cursor;
        BOOL (__thiscall *onCreate)(void*, HWND) = (BOOL(__thiscall*)(void*, HWND))vt[6];
        if (!onCreate(g_cursor, hWndParent))
            { destroy_subsystem(g_cursor); destroy_subsystem(g_postcard); destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -13; }
    }

    /* 7. AudioMgr (IS HelpWnd) — 0x3078 bytes, res 0x1FE. Ctor: 0x44F490 */
    g_audio_mgr = new HelpWnd(hInst, 0x1FE);
    if (g_audio_mgr == nullptr)
        { destroy_subsystem(g_cursor); destroy_subsystem(g_postcard); destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -14; }
    if (!((HelpWnd*)g_audio_mgr)->create(hWndParent))        /* 0x450CA0 */
        { destroy_subsystem(g_audio_mgr); destroy_subsystem(g_cursor); destroy_subsystem(g_postcard); destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -15; }

    /* 8. AboutDialog — 0x1184 bytes, res 0x1FD. Ctor: 0x40F1C0 */
    g_about = new AboutDialog(hInst, 0x1FD);
    if (g_about == nullptr)
        { destroy_subsystem(g_audio_mgr); destroy_subsystem(g_cursor); destroy_subsystem(g_postcard); destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -16; }
    if (!((AboutDialog*)g_about)->Create(hWndParent))         /* 0x40F5A0 */
        { destroy_subsystem(g_about); destroy_subsystem(g_audio_mgr); destroy_subsystem(g_cursor); destroy_subsystem(g_postcard); destroy_subsystem(g_trainstation_window); destroy_subsystem(g_postcard_send); destroy_subsystem(g_town); destroy_subsystem(g_ui_main); return -17; }

    return 0;
#endif
}

/* ================================================================== */
/* WIN32_PostQuit                                                       */
/* Address: 0x463670                                                    */
/*                                                                     */
/* Despite the Ghidra/legacy stub name, this posts no quit message —   */
/* it minimizes (ShowWindow(..., 7) == SW_SHOWMINNOACTIVE) every        */
/* constructed UI subsystem window that is currently shown, then the    */
/* main CGWND window, unconditionally. Called from several WM_SYSCOMMAND/ */
/* SC_CLOSE-style handlers (ui/HelpWnd.cpp, game/BuildingPanel.cpp,      */
/* input/Cursor_new_impls.cpp, and — per Ghidra xrefs not previously     */
/* enumerated — GAMESTATE_WndProc, Town_LoadBackground, and 2 further    */
/* unnamed call sites) right before/after switching game mode, so the   */
/* windows get out of the way without being destroyed. Demo-mode builds  */
/* (g_demo_mode == 1) skip this entirely — verified via the leading      */
/* CMP/JZ against 0x4a9918 in the disassembly, matching g_demo_mode's    */
/* documented address everywhere else in the tree. Return value is a     */
/* bool in AL (true unless the demo-mode early-out is taken); every real  */
/* caller discards it, so this is declared void here, matching all three  */
/* existing call sites' `void WIN32_PostQuit(void);` declarations. */
void WIN32_PostQuit(void);
void WIN32_PostQuit(void)
{
    if (g_demo_mode == 1) {
        return;
    }

    /* g_about/g_audio_mgr are GameWindow-derived (AboutDialog/HelpWnd):
     * gated on GameWindow::visible2 (+0x114), per the disassembly's
     * byte-at-EAX+0x114 check ahead of each ShowWindow call. */
    if (g_about != nullptr) {
        AboutDialog* about = static_cast<AboutDialog*>(g_about);
        if (about->visible2 != 0) {
            ShowWindow(about->hWnd, 7);
        }
    }
    if (g_audio_mgr != nullptr) {
        HelpWnd* audioMgr = static_cast<HelpWnd*>(g_audio_mgr);
        if (audioMgr->visible2 != 0) {
            ShowWindow(audioMgr->hWnd, 7);
        }
    }

    /* g_cursor/g_town/g_postcard/g_postcard_send are UI_WindowBase-derived:
     * gated on UI_WindowBase::visible (+0xE4). */
    if (g_cursor != nullptr) {
        Cursor* cursor = static_cast<Cursor*>(g_cursor);
        if (cursor->visible != 0) {
            ShowWindow(cursor->hWnd, 7);
        }
    }
    if (g_town != nullptr) {
        Town* town = static_cast<Town*>(g_town);
        if (town->visible != 0) {
            ShowWindow(town->hWnd, 7);
        }
    }
    if (g_postcard != nullptr) {
        PostcardAlbum* postcard = static_cast<PostcardAlbum*>(g_postcard);
        if (postcard->visible != 0) {
            ShowWindow(postcard->hWnd, 7);
        }
    }
    if (g_postcard_send != nullptr) {
        PostcardPreviewWindow* postcardSend = static_cast<PostcardPreviewWindow*>(g_postcard_send);
        if (postcardSend->visible != 0) {
            ShowWindow(postcardSend->hWnd, 7);
        }
    }

    /* g_ui_main (EditWindow) minimizes its two child panels first — each
     * independently of g_ui_main's own visible flag — then itself. */
    if (g_ui_main != nullptr) {
        EditWindow* uiMain = static_cast<EditWindow*>(g_ui_main);

        /* pPanelB (GameSetupPanel, +0x220): gated on its own class-specific
         * field at +0xE8 (GameSetupPanel::field_E8 in ui/GameSetupPanel.h —
         * only ever zeroed there today; this is the first evidence it also
         * doubles as a visible-style flag, mirroring the inherited
         * UI_WindowBase::visible at +0xE4 that this same class also has but
         * which the original code does NOT read here). */
        if (uiMain->pPanelB != nullptr && uiMain->pPanelB->field_E8 != 0) {
            ShowWindow(uiMain->pPanelB->hWnd, 7);
        }

        /* pPanelA (NameEntryPanel, +0x21C): gated on inherited
         * UI_WindowBase::visible (+0xE4). */
        if (uiMain->pPanelA != nullptr && uiMain->pPanelA->visible != 0) {
            ShowWindow(uiMain->pPanelA->hWnd, 7);
        }

        if (uiMain->visible != 0) {
            if (uiMain->pPopupWindow != nullptr) {
                uiMain->setState(7);   /* EditWindow::setState, 0x4208F0 */
            }
            ShowWindow(uiMain->hWnd, 7);
        }
    }

    /* The original dereferences [0x4aa4a0] unguarded here (no CMP/TEST before
     * the load), matching this file's own pre-existing unguarded uses of
     * g_main_window elsewhere (e.g. the initMode1()/hWnd reads above) — safe
     * in the real game because g_main_window is always constructed before
     * any of this function's WndProc callers can exist. On this host build,
     * though, `shared/stubs_impl.cpp`/`tests/persistence_fixtures.h` default
     * it to nullptr, and unit tests can plausibly exercise a WndProc (e.g.
     * Cursor's SC_CLOSE handler) without having run the full subsystem-init
     * chain first. Guard defensively — this never changes behavior on the
     * real init path, only avoids a host-only null deref. */
    if (g_main_window != nullptr) {
        ShowWindow(static_cast<CGWND*>(g_main_window)->hWnd, 7);
    }
}

