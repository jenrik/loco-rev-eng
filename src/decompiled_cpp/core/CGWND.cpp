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

/* Subsystem globals (referenced by multiple CGWND methods) */
extern void*    g_ui_main;              /* 0x4FD378 — UI_MainMenu instance */
extern void*    g_town;                 /* 0x4FD37C — Town instance */
extern void*    g_postcard;             /* 0x4FD384 — Postcard LOCOBITMAP */
extern void*    g_cursor;               /* 0x4FD380 — Cursor instance */
extern void*    g_postcard_send;        /* 0x4FD388 — PostcardPreviewWindow */
extern void*    g_trainstation_window;  /* 0x485258 — TrainStationWindow */
extern void*    g_audio_mgr;            /* 0x4FD38C — AudioMgr/HelpWnd */
extern void*    g_about;                /* 0x4FD390 — AboutDialog */
extern void*    g_audio;                /* 0x4FD3BC — GameAudio instance */
extern void*    g_netman;               /* 0x4FD3AC — NetMan instance */
extern void*    _g_network_thread;      /* 0x4FD398 — NetworkThread */
extern void*    _g_train;               /* 0x4FD3A4 — Train object */
extern void*    _DAT_004fd3a8;          /* 0x4FD3A8 — unknown subsystem */
extern void*    _g_dplay;               /* 0x4FD3B0 — DirectPlay */
extern void*    _g_dplay_config;        /* 0x4FD3B4 — DirectPlayConfig */
extern void*    _g_train_resources;     /* 0x4FD394 — TrainResources */
extern void*    g_player_config;        /* 0x4AA4A8 — PlayerConfig */
extern void*    _g_audio_config;        /* 0x4FD3D4 — AudioConfig */
extern void*    _g_dsound_object;       /* 0x4FD3D8 — DirectSound object */
extern int*     _g_cursor_surface;      /* 0x4FD3C8 — Cursor surface */
extern void*    g_frame_event;          /* 0x4A990C — Frame-timer event */
extern uint32_t g_timer_event_id;       /* 0x485438 — Multimedia timer ID */
extern void*    g_ddraw;                /* 0x485440 — DirectDraw object */
extern void*    g_main_window;          /* 0x4AA4A0 — CGWND singleton ptr */
extern uint8_t  g_in_build_mode;        /* 0x4FD199 — in-build-mode flag */
extern void*    g_game;                 /* 0x4854C8 — Game object */
extern void*    g_async_task_queue;     /* 0x4A9AD0 — async task queue */

/* InstallPathInit globals */
extern char     g_install_path[256];    /* 0x4A99C8 — install path buffer */
extern char     g_remote_res_path[256]; /* 0x4A97A8 — remote resource path */
extern const char g_empty_string;       /* 0x4851D0 — empty string (0x00) */
extern const char DAT_0047e220[];       /* INI key name for install path */
extern const char DAT_0047e224[];       /* default value for install key */
extern const char DAT_0047e234;         /* single '\' char as string */

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
/* CGWND::SetPause — Toggle active/paused state                        */
/* Address: 0x4061B0  (size: 48 bytes)                                 */
/*                                                                     */
/* Sets the active/visible flag at +0x24 (Entity.visible), calls       */
/* vtable[1] (StopSound) to release current audio, then plays or       */
/* pauses the audio channel at +0x48 depending on new state.           */
/*                                                                     */
/* Called by:                                                          */
/*   UI_EnableWindow (0x4238AB)                                        */
/*   RESDATA_ScriptedObject_Start (0x4496F2) — demo mode pause        */
/* ================================================================== */
void CGWND::SetPause(bool paused)
{
    /* Set the active/visible flag at +0x24 */
    this->field_10 = paused ? 1 : 0;                       /* +0x10 (visible for this context) */

    /* Call vtable[1] — StopSound / release current audio */
    void** vt = (void**)this->vtable;
    ((void(__thiscall*)())vt[0x04 / 4])();

    /* Manage audio channel at +0x48 */
    void* audio_ch = *(void**)((uint8_t*)this + 0x48);     /* audio channel */
    if (audio_ch != nullptr) {
        if (paused) {
            /* Activating — play audio */
            extern void CGWND_AudioChannel_Play(uint32_t ch);
            CGWND_AudioChannel_Play((uint32_t)audio_ch);
        } else {
            /* Pausing — silence audio */
            extern void CGWND_AudioChannel_Pause(int ch);
            CGWND_AudioChannel_Pause((int)audio_ch);
        }
    }
}


/* ================================================================== */
/* CGWND::SetMode — Core game mode state machine (modes 0-10)          */
/* Address: 0x408130  (size: 494 bytes)                                */
/*                                                                     */
/* Central mode dispatcher. Guards against redundant transitions       */
/* (no-op if new == old), then jumps through a 10-entry dispatch       */
/* table indexed by (mode - 1).                                        */
/*                                                                     */
/* Game modes:                                                         */
/*   0  INIT         1  INIT_GAME    2  MAIN_MENU   3  TOWN           */
/*   4  EXIT_BUILD   5  TOWN_SCREEN  6  POSTCARD    7  CURSOR         */
/*   8  SAVE_STATE   9  POSTCARD_SEND  10 QUIT                         */
/* ================================================================== */
void CGWND::SetMode(int new_mode)
{
    extern int g_game_mode;                                 /* 0x4851F4 */
    int old_mode = g_game_mode;

    if (g_game_mode == new_mode) return;  /* already in this mode */

    g_game_mode = new_mode;

    switch (new_mode) {
    case 1:  /* GAME_MODE_INIT_GAME — full init, transitions to mode 3 */
        this->InitMode1();                                  /* 0x408350 */
        return;

    case 2:  /* GAME_MODE_MAIN_MENU */
        {
            extern void Game_SetScreenMode(void* game, int a, int b, int c);
            extern void* g_game;                            /* 0x4854C8 */
            Game_SetScreenMode(g_game, 0, 1, 0);

            extern void* g_ui_main;                         /* 0x4FD378 */
            void** ui_vt = *(void***)g_ui_main;
            ((void(__thiscall*)())ui_vt[0x08 / 4])();      /* vtable[2] */
        }
        return;

    case 3:  /* GAME_MODE_TOWN */
        extern void CGWND_InitMode4(int old);
        CGWND_InitMode4(old_mode);                          /* 0x4086F0 */
        return;

    case 4:  /* GAME_MODE_EXIT_BUILD */
        {
            extern void Game_SelectGameObject(void* game, void* obj);
            extern void* g_game;
            Game_SelectGameObject(g_game, nullptr);         /* deselect */

            extern void BuildingMgr_DestroyAll(void* mgr, int flags);
            extern void* g_building_mgr;                    /* 0x485448 */
            BuildingMgr_DestroyAll(g_building_mgr, 0);

            extern void UI_ResetTooltips(void* mgr, int reset_type);
            extern void* g_tooltip_mgr;
            UI_ResetTooltips(g_tooltip_mgr, 0);

            extern void World_Reset(void* world, int flags);
            World_Reset((void*)0x4A98B0, 0);

            extern uint8_t g_in_build_mode;
            g_in_build_mode = 0;
        }
        return;

    case 5:  /* GAME_MODE_TOWN_SCREEN */
    case 6:  /* GAME_MODE_POSTCARD */
    case 7:  /* GAME_MODE_CURSOR */
    case 9:  /* GAME_MODE_POSTCARD_SEND */
        {
            extern void* g_audio;
            if (g_audio != nullptr) {
                extern void GameAudio_UpdateVolume(void* a, int v);
                GameAudio_UpdateVolume(g_audio, 1);
            }
            extern void Game_SetScreenMode(void* game, int a, int b, int c);
            extern void* g_game;
            Game_SetScreenMode(g_game, 0, 0, 0);

            void* screen_obj = nullptr;
            if (new_mode == 5) {
                extern void* g_town;
                screen_obj = g_town;                        /* 0x4FD37C */
            } else if (new_mode == 6) {
                extern void* g_postcard;
                screen_obj = g_postcard;                    /* 0x4FD384 */
            } else if (new_mode == 7) {
                extern void* g_cursor;
                extern void Cursor_Show(void* c);
                Cursor_Show(g_cursor);                      /* 0x416B80 */
            } else if (new_mode == 9) {
                if (old_mode == 4) {
                    extern void NETMAN_SendMapData(void* net, int flags);
                    extern void* g_netman;
                    NETMAN_SendMapData(g_netman, 0);
                }
                extern void* g_postcard_send;
                screen_obj = g_postcard_send;               /* 0x4FD388 */
            }
            if (screen_obj != nullptr) {
                void** svt = *(void***)screen_obj;
                ((void(__thiscall*)())svt[0x08 / 4])();    /* vtable[2] */
            }
        }
        return;

    case 8:  /* GAME_MODE_SAVE_STATE — store old mode in audio mgr */
        {
            extern void* g_audio_mgr;
            *(int*)((uint8_t*)g_audio_mgr + 0x3074) = old_mode;
        }
        return;

    case 10: /* GAME_MODE_QUIT */
        {
            extern void* g_audio;
            if (g_audio != nullptr) {
                extern int GameAudio_PlayResourceEx(void* a, int id, uint32_t* out);
                uint32_t ch = 0;
                GameAudio_PlayResourceEx(g_audio, 0x5026, &ch);

                /* Spin-wait for quit sound to finish */
                if (ch != 0) {
                    extern int CGWND_AudioChannel_IsActive(uint32_t ch);
                    while (CGWND_AudioChannel_IsActive(ch) && ch != 0) {
                        /* busy-wait */
                    }
                    extern void CGWND_AudioChannel_Release(void* ch);
                    CGWND_AudioChannel_Release((void*)ch);
                }
            }

            /* Shutdown DirectDraw */
            extern void* g_ddraw;
            if (g_ddraw != nullptr) {
                void** dd_vt = *(void***)g_ddraw;
                extern void* g_main_window;
                ((void(__thiscall*)(HWND,int))dd_vt[0x50 / 4])(
                    *(HWND*)((uint8_t*)g_main_window + 8), 8);
            }

            /* Post WM_QUIT */
            extern int PostMessageA(HWND hWnd, uint32_t msg, uint32_t wParam, uint32_t lParam);
            extern void* g_main_window;
            PostMessageA(*(HWND*)((uint8_t*)g_main_window + 8), 0x10, 0, 0);
        }
        return;

    default:
        /* Mode 0 or >10: silent no-op (used for reset during startup) */
        break;
    }
}


/* ================================================================== */
/* CGWND::Cleanup — Full game shutdown                                 */
/* Address: 0x4077A0  (size: 831 bytes)                                */
/*                                                                     */
/* Saves window position and CleanExit=1 to lego.ini, flushes network */
/* messages, saves world state, destroys all subsystems in reverse      */
/* order of InitAllSubsystems, shuts down DirectDraw/DirectSound,      */
/* kills multimedia timer, and posts WM_QUIT.                          */
/* See src/decompiled/cgwnd_cleanup.c for complete subsystem list.     */
/* ================================================================== */
void CGWND::Cleanup()
{
    /* --- PHASE 1: Save window position and CleanExit to lego.ini --- */
    extern void  Config_WriteInt(void* cfg, const char* sec, const char* key, int val);
    extern void* g_config_ini;
    extern int   g_window_left, g_window_top, g_window_right, g_window_bottom;

    Config_WriteInt(g_config_ini, "WINDOW ATTRIBUTES", "RectLeft",   g_window_left);
    Config_WriteInt(g_config_ini, "WINDOW ATTRIBUTES", "RectTop",    g_window_top);
    Config_WriteInt(g_config_ini, "WINDOW ATTRIBUTES", "RectRight",  g_window_right);
    Config_WriteInt(g_config_ini, "WINDOW ATTRIBUTES", "RectBottom", g_window_bottom);
    Config_WriteInt(g_config_ini, "PROCESS", "CleanExit", 1);

    /* --- PHASE 2: Network thread cleanup --- */
    extern void* _g_network_thread;
    if (_g_network_thread != nullptr) {
        extern void* _g_train;
        extern void Train_FlushMessages(void* train);
        Train_FlushMessages(_g_train);

        void** nt_vt = *(void***)_g_network_thread;
        ((void(__thiscall*)(int))nt_vt[0])(1);  /* scalar dtor with free */
        _g_network_thread = nullptr;
    }

    /* Wait for async thread to exit (spin with 100ms sleeps) */
    {
        extern int WIN32_GetThreadResult(void* state);
        extern void WIN32_Sleep(uint32_t ms);
        void* thread_state = (void*)0x4A9AD0;
        while (WIN32_GetThreadResult(thread_state) != 0) {
            WIN32_Sleep(100);
        }
    }

    /* --- PHASE 3: Save world state, unlock sprites --- */
    extern void World_Init(void* world);
    extern void World_Shutdown(int world);
    extern void Sprite_UnlockAll(int mgr);
    World_Init((void*)0x4A98B0);
    World_Shutdown(0x4A98B0);
    Sprite_UnlockAll(0x4AAD08);

    /* --- PHASE 4: Destroy all UI/audio subsystems (vtable[0](1) pattern) --- */
    auto destroyObj = [](void*& ptr) {
        if (ptr != nullptr) {
            void** vt = *(void***)ptr;
            ((void(__thiscall*)(int))vt[0])(1);
            ptr = nullptr;
        }
    };

    extern void* g_ui_main, *g_town, *g_postcard, *g_cursor;
    extern void* g_postcard_send, *g_trainstation_window;
    extern void* g_audio_mgr, *g_about;

    destroyObj(g_ui_main);
    destroyObj(g_town);
    destroyObj(g_postcard);
    destroyObj(g_cursor);
    destroyObj(g_postcard_send);
    destroyObj(g_trainstation_window);
    destroyObj(g_audio_mgr);
    destroyObj(g_about);

    /* --- PHASE 5: Additional subsystems --- */
    extern void* _g_train, *_DAT_004fd3a8, *g_netman, *_g_dplay;
    extern void* _g_dplay_config, *_g_train_resources, *g_player_config;
    extern void* _g_audio_config, *_g_dsound_object;

    destroyObj(_g_train);
    destroyObj(_DAT_004fd3a8);
    destroyObj(g_netman);
    destroyObj(_g_dplay);
    destroyObj(_g_dplay_config);
    destroyObj(_g_train_resources);
    destroyObj(g_player_config);
    destroyObj(g_config_ini);       /* also cleared in dtor */
    destroyObj(_g_audio_config);
    destroyObj(_g_dsound_object);

    /* --- PHASE 6: Cursor surface special case (refcount at +4) --- */
    extern int* _g_cursor_surface;
    if (_g_cursor_surface != nullptr) {
        void** cs_vt = (void**)_g_cursor_surface[0];
        ((void(__thiscall*)())cs_vt[0x08 / 4])();  /* vtable[2] = ReleaseSurface */

        if (_g_cursor_surface[1] == -1) {
            ((void(__thiscall*)(int))cs_vt[0])(1);  /* delete if refcount == -1 */
        }
        _g_cursor_surface = nullptr;
    }

    /* --- PHASE 7: Low-level shutdown --- */
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
    Town_GameView_Cleanup((int*)0x4852A0);

    extern void DDRAW_InvalidateAll(int* ddraw);
    DDRAW_InvalidateAll((int*)0x4A9EF0);

    extern void RESDATA_ScriptedObject_Shutdown(int* obj);
    RESDATA_ScriptedObject_Shutdown((int*)0x4AA5B8);

    extern void UI_FreeMessageBox(int msgbox);
    UI_FreeMessageBox(0x4FD220);

    extern void INPUT_Shutdown(int input);
    INPUT_Shutdown(0x4A99B0);

    extern void INPUT_Cleanup(int* mgr);
    INPUT_Cleanup((int*)0x4A9990);

    extern void Game_Shutdown(int* game);
    Game_Shutdown((int*)0x4854C8);

    extern int RESMGR_Shutdown(int resmgr);
    RESMGR_Shutdown(0x4855E8);

    extern int CRT_0x470650(void);
    CRT_0x470650();
}


/* ================================================================== */
/* CGWND::InstallPathInit — Read install path from registry, load INI  */
/* Address: 0x4068D0  (size: 712 bytes / 0x2C8)                         */
/*                                                                     */
/* Flow:                                                               */
/*   1. Read install path from HKLM\SOFTWARE\Intelligent Games\LEGO    */
/*      Loco default value. Create key with empty value if missing.    */
/*   2. Append "\lego.ini" to form config file path.                   */
/*   3. Create PlayerConfig object wrapping this path.                 */
/*   4. Read [DIRECTORIES] from lego.ini into g_install_path and       */
/*      g_remote_res_path.                                             */
/*   5. If demo mode, clear g_remote_res_path.                        */
/*   6. Ensure g_remote_res_path ends with '\'.                        */
/*   7. Strip trailing '\' from g_install_path.                        */
/*   8. mkdir(g_install_path).                                         */
/*   9. Re-append '\' to g_install_path.                               */
/*                                                                     */
/* Uses SEH (handler @0x474F1E) for registry/heap protection.          */
/*                                                                     */
/* Called by: WinMain @0x462FF1                                        */
/* ================================================================== */
void CGWND::InstallPathInit()
{
    HKEY   hkey;
    LONG   reg_result;
    DWORD  reg_value_type;
    DWORD  reg_value_size;
    char   install_base_buf[1280];    /* stack working path buffer (+ESP+0x40) */
    char   reg_buf[1284];             /* registry value output (+ESP+0x540) */
    int    path_len;
    int    mkdir_result;

    extern LONG  RegOpenKeyExA(HKEY hKey, const char* subKey, DWORD opts,
                               REGSAM samDesired, HKEY* out);
    extern LONG  RegQueryValueExA(HKEY hKey, const char* valueName, DWORD* reserved,
                                  DWORD* type, uint8_t* data, DWORD* size);
    extern LONG  RegCloseKey(HKEY hKey);
    extern LONG  RegCreateKeyExA(HKEY hKey, const char* subKey, DWORD reserved,
                                 char* lpClass, DWORD options, REGSAM samDesired,
                                 void* secAttr, HKEY* result, DWORD* disposition);
    extern LONG  RegSetValueExA(HKEY hKey, const char* valueName, DWORD reserved,
                                DWORD type, const uint8_t* data, DWORD size);

    /* ── Step 1: Read install path from registry ── */
    /* 0x4068FB-0x406966 */
    reg_value_size = 0x504;
    reg_result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Intelligent Games\\LEGO Loco", /* 0x47E238 */
        0,
        KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY, /* 0x20019 */
        &hkey);

    if (reg_result == ERROR_SUCCESS) {
        reg_value_type = 0;
        reg_result = RegQueryValueExA(
            hkey,
            nullptr,                    /* default (nameless) value */
            nullptr,
            &reg_value_type,
            (uint8_t*)reg_buf,
            &reg_value_size);

        RegCloseKey(hkey);

        if (reg_result == ERROR_SUCCESS) {
            /* 0x406940: Copy registry value into working buffer */
            lstrcpyA(install_base_buf, reg_buf);
            goto after_registry;
        }
    }

    /* Key missing or read failed — create it with empty default value */
    /* 0x406942-0x406966 */
    install_base_buf[0] = '\0';
    extern void CRT_ZeroBuffer(char* buf, size_t len);
    CRT_ZeroBuffer(install_base_buf, 0x100);           /* @0x466950 */

    reg_result = RegCreateKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Intelligent Games\\LEGO Loco",     /* 0x47E238 */
        0,
        nullptr,            /* lpClass */
        0,                  /* dwOptions */
        KEY_ALL_ACCESS,     /* 0xF003F */
        nullptr,            /* lpSecurityAttributes */
        &hkey,
        nullptr);           /* lpdwDisposition */

    if (reg_result == ERROR_SUCCESS) {
        RegSetValueExA(
            hkey,
            nullptr,                            /* default value */
            0,                                  /* Reserved */
            REG_SZ,                             /* type = string */
            (const uint8_t*)install_base_buf,
            1);                                 /* length = 1 (just null) */
        RegCloseKey(hkey);
    }

after_registry:
    /* ── Step 2: Build "<install_base>\lego.ini" ── */
    /* 0x40696B-0x406984 */
    /* Inlined strcat: append "\" then "lego.ini" */
    {
        extern const char DAT_0047e234;  /* "\" as null-terminated string */
        lstrcatA(install_base_buf, &DAT_0047e234);
    }
    lstrcatA(install_base_buf, "lego.ini");            /* 0x47E228 */

    /* ── Step 3: Create PlayerConfig object for INI path ── */
    /* 0x40698B-0x4069B0 */
    void* player_config_raw = operator_new(0x10C);
    if (player_config_raw != nullptr) {
        extern void* PlayerConfig_Ctor(void* mem, const char* path); /* @0x452CE0 */
        g_config_ini = PlayerConfig_Ctor(player_config_raw, install_base_buf);
    } else {
        g_config_ini = nullptr;
    }

    /* ── Step 4: Read [DIRECTORIES] section from lego.ini ── */
    /* 0x4069B4-0x406A2B */
    {
        extern void Config_GetIniString(void* config, const char* section,
                                        const char* key, const char* defaultVal,
                                        char* out, uint32_t size); /* @0x452D80 */
        extern const char DAT_0047e220[];   /* INI key name for install path */
        extern const char DAT_0047e224[];   /* default value for that key */

        /* 4a: Install path */
        Config_GetIniString(g_config_ini,
            "DIRECTORIES",                         /* 0x47E214 */
            DAT_0047e220,                          /* key (e.g. "ResFile") */
            DAT_0047e224,                          /* default */
            g_install_path,                        /* 0x4A99C8 — output */
            0x100);

        /* 4b: Remote resource path */
        Config_GetIniString(g_config_ini,
            "DIRECTORIES",                         /* 0x47E214 */
            "RemoteRes",                           /* 0x47E208 */
            &g_empty_string,                       /* default = "" */
            g_remote_res_path,                     /* 0x4A97A8 — output */
            0x100);
    }

    /* ── Step 5: Demo mode clears remote resource path ── */
    /* 0x406A2D-0x406A3E */
    if (g_demo_mode == 1) {
        g_remote_res_path[0] = '\0';
    }

    /* ── Step 6: Ensure g_remote_res_path ends with '\' ── */
    /* 0x406A40-0x406A6A */
    {
        int rlen = lstrlenA(g_remote_res_path);
        extern const char DAT_0047e234;  /* "\" */
        if (rlen > 0 && g_remote_res_path[rlen - 1] != '\\') {
            lstrcatA(g_remote_res_path, &DAT_0047e234);
        }
    }

    /* ── Step 7: Strip trailing '\' from g_install_path ── */
    /* 0x406A6F-0x406A99 */
    {
        int ilen = lstrlenA(g_install_path);
        if (ilen > 0 && g_install_path[ilen - 1] == '\\') {
            g_install_path[ilen - 1] = '\0';
            path_len = ilen - 1;    /* ESI = len after backslash stripped */
        } else {
            path_len = ilen;        /* ESI = original len */
        }
    }

    /* ── Step 8: mkdir(g_install_path) ── */
    /* 0x406A9F-0x406AA6 */
    extern int CRT_mkdir(const char* path);  /* @0x466590 */
    mkdir_result = CRT_mkdir(g_install_path);

    /* ── Step 9: Re-append '\' to g_install_path ── */
    /* 0x406AAC-0x406ABF */
    {
        extern const char DAT_0047e234;  /* "\" */
        lstrcatA(g_install_path, &DAT_0047e234);
    }

    /* ── Step 10: Return success indicator ── */
    /* 0x406AC4-0x406AD3 */
    /* Return TRUE only if mkdir succeeded (returned 0) AND path after
     * backslash-strip has length > 2. This rejects degenerate paths
     * like "", "C", or "C:". */
    if (mkdir_result == 0 && path_len > 2) {
        /* success — data dir exists or was created */
    }
    /* Function returns with EAX already set from the comparison chain.
     * The original uses SETE/SETNZ to produce a BOOL return. For clarity,
     * we return a bool. */
}


/* ================================================================== */
/* CGWND::InitAllSubsystems — Initialize all game subsystems           */
/* Address: 0x406F90                                                    */
/*                                                                     */
/* Creates and initializes all COM-like subsystems in order:           */
/*   UI_MainMenu, Town, PostcardSend, Postcard, Cursor, AudioMgr,     */
/*   AboutDialog, Train, etc.                                          */
/* See src/decompiled/cgwnd_initallsubsystems.c for full details.      */
/* ================================================================== */
void CGWND::InitAllSubsystems()
{
    /* See src/decompiled/cgwnd_initallsubsystems.c (0x406F90) */
}


/* ================================================================== */
/* CGWND::InitMode1 — Initialize game mode 1 subsystems                */
/* Address: 0x408350                                                    */
/*                                                                     */
/* Sets up the game world for initial entry into gameplay mode.        */
/* Transitions to mode 3 (TOWN) when complete.                         */
/* See src/decompiled/cgwnd_initmode1.c for full details.              */
/* ================================================================== */
void CGWND::InitMode1()
{
    /* See src/decompiled/cgwnd_initmode1.c (0x408350) */
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
