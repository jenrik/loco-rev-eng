/**
 * CGWND.cpp — Main game window class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "CGWND.h"
#include "../platform/ddraw_interfaces.h"
#include "../game/PlayerConfig.h"  /* for sizeof(PlayerConfig) */
#include <cstring>
#include <cstdio>

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
#include "../audio/GameAudio.h"
#include "../game/World.h"

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
static inline void FormatResourceString(void*, unsigned int, char* buf, int sz) { if(buf&&sz>0) buf[0]=0; }
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

/* ================================================================== */
/* Helper: Play a sound resource by ID without blocking                 */
/* ================================================================== */
#ifdef _WIN32
static void PlaySound(uint32_t resId) {
    /* Calls PlaySoundA with SND_ASYNC | SND_NOWAIT | SND_RESOURCE */
    extern BOOL PlaySoundA(const char* pszSound, HMODULE hmod, DWORD fdwSound);
    extern HMODULE GetModuleHandleA(const char* name);
    HMODULE hMod = GetModuleHandleA(nullptr);
    PlaySoundA((const char*)resId, hMod, 0x20001);  /* SND_RESOURCE | SND_ASYNC | SND_NOWAIT */
}
#else
static void PlaySound(uint32_t resId) {
    /* No sound on non-Windows for now */
    (void)resId;
}
#endif


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
        FormatResourceString(&g_resmgr, 0x7A, msg, sizeof(msg));
    } else {
        /* Gate 2: Mouse check */
        int mousePresent = GetSystemMetrics(0x13);  /* SM_MOUSEPRESENT */
        if (mousePresent == 0) {
            FormatResourceString(&g_resmgr, 0x7B, msg, sizeof(msg));
        } else {
            /* Gate 3: Screen width in range 800-1280 */
            if (g_screen_width < 0x501) {
                FormatResourceString(&g_resmgr, 0x7A, msg, sizeof(msg));
            } else if (g_screen_width > 799) {
                return 1;
            } else {
                FormatResourceString(&g_resmgr, 0x7A, msg, sizeof(msg));
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

            if (g_ddraw != nullptr) {
                /* SetCooperativeLevel — real ABI vtable[20] (byte offset
                 * 0x50), dispatched by name (see
                 * native/ddraw_surface_ops.c's DDRAW_ReleaseSurfaces for
                 * the same call, converted 2026-08-14 — this shim is
                 * API- not ABI-compatible, see CLAUDE.md). */
                static_cast<IDirectDraw4*>(g_ddraw)->SetCooperativeLevel(
                    *(HWND*)((uint8_t*)g_main_window + 0x08), 8);
            }
            PostMessageA(*(HWND*)((uint8_t*)g_main_window + 0x08), 0x10, 0, 0);
#else
            // Host-only equivalent of the original 0x5026 exit-sound
            // request.  Queue the exit sweep while the audio device is
            // still open (kept alive by the background-music stream),
            // then stop only the looping music so the drain loop only
            // needs to wait for the short exit sweep.
            bool ok = SDL3_GameAudioPlayResource(0x5026);
            fprintf(stderr, "[audio] CGWND_SetMode(10): PlayResource(0x5026) -> %s\n", ok ? "true" : "FALSE");
            SDL3_GameAudioStopLooping();
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

    extern void RESDATA_ScriptedObject_Shutdown(int* obj);
    RESDATA_ScriptedObject_Shutdown(reinterpret_cast<int*>(static_cast<uintptr_t>(0x4AA5B8)));

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

    extern int RESMGR_Shutdown(int resmgr);
    RESMGR_Shutdown(0x4855E8);

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
    wc.lpfnWndProc   = (WNDPROC)0x4618C0;            /* MainWndProc */
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

#else /* !_WIN32 — SDL3 window */

    /* SDL3: The window was already created by SDL3_WindowInit in main.cpp.
     * Retrieve it via SDL3_GetWindow() and make it visible. */
    SDL_Window* win = SDL3_GetWindow();
    if (win == nullptr) {
        return FALSE;
    }
    this->hWnd = static_cast<HWND>(win);

    SDL_ShowWindow(win);

    /* Query client area size */
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    g_client_rect.left   = 0;
    g_client_rect.top    = 0;
    g_client_rect.right  = w;
    g_client_rect.bottom = h;

    return TRUE;

#endif /* _WIN32 */
}


/* ================================================================== */
int CGWND_InstallPathInit()
{
#ifdef _WIN32
/* CGWND_InstallPathInit — Read install path from registry, load INI   */
/* Address: 0x4068D0 — free function; no this pointer                  */
/* ================================================================== */
int CGWND_InstallPathInit()
{


    return (mkdir_result == 0 && path_len > 2) ? 1 : 0;

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

