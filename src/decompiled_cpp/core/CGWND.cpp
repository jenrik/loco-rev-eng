/**
 * CGWND.cpp — Main game window class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "CGWND.h"
#include "../shared/vtable_addrs.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {
    /* Windows API */
    HWND  GetDesktopWindow(void);
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
    void*  operator_new(size_t size);

    /* CRT */
    uint32_t CRT_time(void);
    int      CRT_atoi(const char* str);
    char*    CRT_strtok(char* str, const char* delim);

    /* Resource Manager */
    void FormatResourceString(void* resmgr, int id, char* out, int maxLen);

    /* Config/INI */
    int  Config_GetIniInt(void* config, const char* section, const char* key, int defaultVal);
    void Config_WriteInt(void* config, const char* section, const char* key, uint32_t value);

    /* Memory management */
    void GLOBAL_free(void* ptr);
    void* operator_new(size_t size);
}

/* ================================================================== */
/* Global variables                                                    */
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
extern int      g_client_width;         /* 0x485210 area */
extern uint8_t  g_is_fullscreen;        /* 0x485210 */
extern uint8_t  g_show_scrollbars;      /* 0x485238 */
extern uint8_t  g_clean_exit;           /* 0x485218 */
extern uint8_t  g_window_mode;          /* 0x4851F0 */
extern uint8_t  g_build_mode;           /* build/placement mode flag */
extern uint8_t  g_road_build_mode;      /* road building mode */
extern int      g_placement_resource_id;/* resource ID being placed */
extern int      g_game_mode;            /* 1 = main menu, 2 = in-game */
extern int      g_timer_id;             /* Windows timer ID */
extern void*    g_resmgr;               /* global resource manager */
extern int      g_demo_mode;            /* 0x4A9918 — demo mode flag */
extern int      g_easter_egg;           /* 0x485230 — easter egg theme */
extern uint32_t g_screen_bpp;           /* 0x48521C — screen color depth */

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
extern const char DAT_0047e0f4[];  /* "." delimiter for strtok */


/* ================================================================== */
/* CGWND::CGWND — Constructor                                          */
/* Address: 0x4061E0                                                   */
/* ================================================================== */
CGWND::CGWND(HINSTANCE hInstance)
{
    /* Set vtable to 0x4774C4 */
    this->vtable = (void*)VTBL_CGWND;

    /* Init flags */
    this->field_10 = 0;
    g_timer_id = 0;

    /* Store fields */
    this->field_08  = 0;
    this->hInstance = hInstance;
    this->hWnd      = GetDesktopWindow();

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
    SetRect((RECT*)&g_window_left,  0, 0, 0, 0);
    SetRect((RECT*)&g_client_width, 0, 0, 0, 0);

    /* Zero version fields */
    this->versionMajor    = 0;
    this->versionMinor    = 0;
    this->versionBuild    = 0;
    this->versionRevision = 0;

    /* Read EXE VERSIONINFO and populate version fields */
    this->ResetState();
}


/* ================================================================== */
/* CGWND::scalar_deleting_destructor — Vtable slot [0]                 */
/* Address: 0x4062A0                                                   */
/* ================================================================== */
void* CGWND::scalar_deleting_destructor(byte flags)
{
    /* Restore vtable to CGWND vtable */
    this->vtable = (void*)VTBL_CGWND;

    /* Release g_config_ini if it exists */
    if (g_config_ini != nullptr) {
        /* Call its scalar deleting destructor with flags=1 */
        void** ini_vtbl = *(void***)g_config_ini;
        ((void(*)(void*,byte))ini_vtbl[0])(g_config_ini, 1);
        g_config_ini = nullptr;
    }

    /* MSVC scalar-delete: if flags & 1, free memory */
    if (flags & 1) {
        GLOBAL_free(this);
    }

    return this;
}


/* ================================================================== */
/* CGWND::ShowMainMenu — Initialize display for main menu              */
/* Address: 0x406480                                                   */
/* ================================================================== */
void CGWND::ShowMainMenu()
{
    /* Store desktop window handle */
    this->hWnd = GetDesktopWindow();

    /* Query screen dimensions */
    g_screen_width  = GetSystemMetrics(0);  /* SM_CXSCREEN */
    g_screen_height = GetSystemMetrics(1);  /* SM_CYSCREEN */

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
    this->minVehicleFPS  = (uint8_t)Config_GetIniInt(g_config_ini,
                               s_BALANCING_0047e164,
                               s_MinVehicleFPS_0047e170, 20);
    this->minBuildingFPS = (uint8_t)Config_GetIniInt(g_config_ini,
                               s_BALANCING_0047e164,
                               s_MinBuildingFPS_0047e154, 18);
    this->minMinifigFPS  = (uint8_t)Config_GetIniInt(g_config_ini,
                               s_BALANCING_0047e164,
                               s_MinMinifigFPS_0047e144, 16);
    this->minFlyingFPS   = (uint8_t)Config_GetIniInt(g_config_ini,
                               s_BALANCING_0047e164,
                               s_MinFlyingFPS_0047e134, 14);

    /* Read and reset CleanExit flag */
    g_clean_exit = (uint8_t)Config_GetIniInt(g_config_ini,
                              s_PROCESS_0047e120,
                              s_CleanExit_0047e128, 1);
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

    HDC hdc = GetDC(this->hWnd);
    uint32_t colorDepth = GetDeviceCaps(hdc, 0x18);  /* BITSPIXEL */
    g_screen_bpp = GetDeviceCaps(hdc, 0x0C);          /* PLANES -> stored */

    ReleaseDC(this->hWnd, hdc);

    /* Gate 1: Color depth check (8-bit paletted = 8 bpp) */
    if (colorDepth < 0x80000000 || g_screen_bpp > 0x10) {
        /* Color depth failure — show message resource 0x7A */
        FormatResourceString(&g_resmgr, 0x7A, msg, sizeof(msg));
    } else {
        /* Gate 2: Mouse check */
        int mousePresent = GetSystemMetrics(0x13);  /* SM_MOUSEPRESENT */
        if (mousePresent == 0) {
            /* No mouse — show message resource 0x7B */
            FormatResourceString(&g_resmgr, 0x7B, msg, sizeof(msg));
        } else {
            /* Gate 3: Screen width in range 800-1280 */
            if (g_screen_width < 0x501) {
                /* Too narrow */
                FormatResourceString(&g_resmgr, 0x7A, msg, sizeof(msg));
            } else if (g_screen_width > 799) {
                /* All checks passed! */
                return 1;
            } else {
                FormatResourceString(&g_resmgr, 0x7A, msg, sizeof(msg));
            }
        }
    }

    /* Show error message */
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
    char     versionStr[0x400];
    uint32_t dummy;
    void*    verData   = nullptr;
    DWORD    verSize;

    /* Timestamp (unused result) */
    CRT_time();

    /* Zero the version string buffer */
    for (int i = 0; i < 0x3FF; i++) {
        ((uint32_t*)versionStr)[i] = 0;
    }
    *(uint16_t*)(versionStr + 0x3FC) = 0;
    versionStr[0x3FE] = 0;

    /* Get path to loco.exe */
    HMODULE hMod = GetModuleHandleA(nullptr);
    GetModuleFileNameA(hMod, filePath, 0x504);

    /* Get version info block size */
    verSize = GetFileVersionInfoSizeA(filePath, &dummy);
    if (verSize != 0) {
        verData = operator_new(verSize);
    }

    if (verData != nullptr) {
        /* Read version info from file */
        if (GetFileVersionInfoA(filePath, 0, verSize, verData)) {
            char*   verStrPtr = nullptr;
            uint32_t verStrLen = 0;

            /* Query the FileVersion string from StringFileInfo block */
            if (VerQueryValueA(verData,
                    s_StringFileInfo_080904B0_FileVer_0047e0f8,
                    (void**)&verStrPtr, &verStrLen) && verStrLen != 0)
            {
                /* Copy version string to local buffer */
                const char* src = verStrPtr;
                char* dst = versionStr;
                size_t len = 0;
                while (*src) { src++; len++; }
                len++;  /* include null */
                src = verStrPtr;

                /* 4-byte aligned copy */
                size_t words = len >> 2;
                for (size_t i = 0; i < words; i++) {
                    *(uint32_t*)dst = *(const uint32_t*)src;
                    src += 4;
                    dst += 4;
                }
                for (size_t i = 0; i < (len & 3); i++) {
                    *dst++ = *src++;
                }
            }
        }
        GLOBAL_free(verData);
    }

    /* Check if version string is empty */
    const char* p = versionStr;
    int len = -1;
    while (*p) { len--; p++; }
    /* If the string is non-empty (len != -2 means not just null terminator) */

    if (len != -2) {
        /* Parse "major.minor.patch.build" via strtok(".") */
        char* token = CRT_strtok(versionStr, DAT_0047e0f4);
        this->versionMajor = CRT_atoi(token);

        /* Advance past token */
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
/* CGWND::SetPause — Toggle pause state                                */
/* Address: 0x4061B0                                                   */
/* ================================================================== */
void CGWND::SetPause()
{
    /* Implementation TBD — toggles active/paused on UI window,
     * manages audio channel at +0x48. Refer to
     * src/decompiled/cgwnd_setpause_004061b0.c for details.            */
}


/* ================================================================== */
/* CGWND::SetMode — Set video mode                                     */
/* ================================================================== */
void CGWND::SetMode()
{
    /* Implementation TBD — refer to src/decompiled/cgwnd_setmode.c */
}


/* ================================================================== */
/* CGWND::Cleanup — Release all subsystems                             */
/* ================================================================== */
void CGWND::Cleanup()
{
    /* Implementation TBD — refer to src/decompiled/cgwnd_cleanup.c */
}


/* ================================================================== */
/* CGWND::InstallPathInit — Read registry install path                 */
/* Address: 0x4068D0                                                   */
/* ================================================================== */
void CGWND::InstallPathInit()
{
    /* Implementation TBD — reads HKLM\SOFTWARE\Intelligent Games\LEGO Loco,
     * loads lego.ini, creates data dir.
     * Refer to src/decompiled/cgwnd_004068d0.c for details.            */
}


/* ================================================================== */
/* CGWND::InitAllSubsystems — Initialize audio/video/input/network     */
/* ================================================================== */
void CGWND::InitAllSubsystems()
{
    /* Implementation TBD — refer to src/decompiled/cgwnd_initallsubsystems.c */
}


/* ================================================================== */
/* CGWND::InitMode1 — Initialize game mode subsystems                  */
/* ================================================================== */
void CGWND::InitMode1()
{
    /* Implementation TBD — refer to src/decompiled/cgwnd_initmode1.c */
}


/* ================================================================ */
/* Free functions in core range                                      */
/* ================================================================ */

/* ================================================================== */
/* CGWND_ParseCmdLine — Parse command line for demo/easter eggs        */
/* Address: 0x406790                                                   */
/*                                                                     */
/* Called by: WinMain @ 0x46308C                                       */
/* ================================================================== */
void CGWND_ParseCmdLine(char* lpCmdLine)
{
    int   demoTokenSeen = 0;
    g_easter_egg = 0;

    /* Tokenize by space — custom CRT_strtok at 0x4663A0 */
    char* token = CRT_strtok(lpCmdLine, (char*)&" ");

    while (token != nullptr) {
        /*
         * Demo-mode blacklist gate. CRT_wcsstr returns 0 for exact match.
         * If token EXACTLY EQUALS any of the three blacklist strings,
         * flag demo mode.
         */
        /* Check: token == "/s"? */
        if (CRT_wcsstr(token, "/s") == 0) {
            demoTokenSeen = 1;
        }
        /* Check: token == "-s"? */
        else if (CRT_wcsstr(token, "-s") == 0) {
            demoTokenSeen = 1;
        }
        /* Check: token == "s"? */
        else if (CRT_wcsstr(token, "s") == 0) {
            demoTokenSeen = 1;
        }
        else {
            /* Token passed the blacklist gate. Check for easter egg themes. */
            if (CRT_wcsstr(token, "Easter") == 0) {
                g_easter_egg = 1;    /* April Fools / Easter */
            }
            else if (CRT_wcsstr(token, "Desert") == 0) {
                g_easter_egg = 2;    /* Desert / Summer */
            }
            else if (CRT_wcsstr(token, "Halloween") == 0) {
                g_easter_egg = 3;    /* Halloween */
            }
            else if (CRT_wcsstr(token, "Winter") == 0) {
                g_easter_egg = 4;    /* Winter */
            }
            else if (CRT_wcsstr(token, "/XMas") == 0) {
                g_easter_egg = 5;    /* Christmas */
            }
            /* Unrecognized tokens are silently ignored */
        }

        /* Next token */
        token = CRT_strtok(nullptr, (char*)&" ");
    }

    /* If any token matched a blacklist sentinel, set demo mode */
    if (demoTokenSeen) {
        g_demo_mode = 1;
    }
}


/**
 * CRT_wcsstr — case-insensitive string comparison
 * Address: 0x471480
 *
 * Despite the name, this is NOT a substring search. It returns 0 when
 * the two strings are equal (case-insensitively), and non-zero otherwise.
 * Used throughout the codebase for equality checks.
 */
extern "C" int CRT_wcsstr(const char* a, const char* b) {
    /* External — defined in CRT thunk region */
    return 0;
}
