/**
 * Game.cpp — Game class implementation: main loop, cursor engine, config/INI
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Global singleton: g_game at 0x4A98D8.
 * Frame heartbeat: GameLoop_FrameUpdate (0x45C3C0) calls Game_Update (0x410840)
 *                  every frame from WinMain.
 *
 * Also includes:
 *   Game_LoadWaveFile (0x413660) — RIFF/WAV file parser for sound loading
 *   Game_ReadChunk (0x413980) — RIFF chunk header reader
 */

#include "Game.h"
#include "../game/Building.h"
#include "../shared/vtable_addrs.h"  /* [VTBL] temporary */
/* ================================================================== */
/* External declarations — Windows API (imported via IAT)               */
/* ================================================================== */

#pragma comment(linker, "/DEFAULTLIB:user32.lib")
#pragma comment(linker, "/DEFAULTLIB:kernel32.lib")

extern "C" {

/* WinAPI IAT shims — map function names to __imp_* pointers */
#define SystemParametersInfoA __imp_SystemParametersInfoA
#define ReleaseCapture __imp_ReleaseCapture
#define LoadCursorA __imp_LoadCursorA
#define SetCursor __imp_SetCursor
#define ShowCursor __imp_ShowCursor
#define SetCapture __imp_SetCapture
#define GetCursorPos __imp_GetCursorPos
#define ClientToScreen __imp_ClientToScreen
#define LoadCursorFromFileA __imp_LoadCursorFromFileA
#define WindowFromPoint __imp_WindowFromPoint
#define ScreenToClient __imp_ScreenToClient


/* user32.dll (IAT entries for delayed loading or direct call) */
typedef int (__stdcall *FUN_ShowCursor)(int);
typedef int (__stdcall *FUN_SetCursorPos)(int, int);
typedef void* (__stdcall *FUN_SetCapture)(void*);
typedef int (__stdcall *FUN_ReleaseCapture)();
typedef void* (__stdcall *FUN_LoadCursorA)(void*, void*);
typedef void* (__stdcall *FUN_LoadCursorFromFileA)(const char*);
typedef void* (__stdcall *FUN_SetCursor)(void*);
typedef int (__stdcall *FUN_GetCursorPos)(void*);
typedef int (__stdcall *FUN_ClientToScreen)(void*, void*);
typedef int (__stdcall *FUN_ScreenToClient)(void*, void*);
typedef void* (__stdcall *FUN_WindowFromPoint)(int, int);
typedef int (__stdcall *FUN_SystemParametersInfoA)(int, int, void*, int);
typedef int (__stdcall *FUN_GetPrivateProfileStringA)(const char*, const char*, const char*, char*, int, const char*);
typedef int (__stdcall *FUN_GetPrivateProfileIntA)(const char*, const char*, int, const char*);

/* advapi32.dll */
typedef long (__stdcall *FUN_RegOpenKeyExA)(void*, const char*, int, int, void*);
typedef long (__stdcall *FUN_RegQueryValueExA)(void*, const char*, int*, int*, void*, int*);
typedef long (__stdcall *FUN_RegCloseKey)(void*);
typedef long (__stdcall *FUN_RegCreateKeyExA)(void*, const char*, int, char*, int, int, void*, void*, int*);
typedef long (__stdcall *FUN_RegSetValueExA)(void*, const char*, int, int, const char*, int);

/* IAT function pointers */
extern FUN_ShowCursor       __imp_ShowCursor;           /* 0x004772C8 */
extern FUN_SetCursorPos     __imp_SetCursorPos;         /* 0x004772E4 */
extern FUN_SetCapture       __imp_SetCapture;           /* 0x004772D0 */
extern FUN_ReleaseCapture   __imp_ReleaseCapture;       /* 0x004772DC */
extern FUN_LoadCursorA      __imp_LoadCursorA;          /* 0x004772E0 */
extern FUN_LoadCursorFromFileA __imp_LoadCursorFromFileA; /* 0x004772CC */
extern FUN_SetCursor        __imp_SetCursor;            /* 0x004772FC */
extern FUN_GetCursorPos     __imp_GetCursorPos;         /* 0x004772D4 */
extern FUN_ClientToScreen   __imp_ClientToScreen;       /* 0x004772D8 */
extern FUN_ScreenToClient   __imp_ScreenToClient;       /* 0x004772D8 */
extern FUN_WindowFromPoint  __imp_WindowFromPoint;      /* 0x00477338 */
extern FUN_SystemParametersInfoA __imp_SystemParametersInfoA; /* 0x004772EC */
extern FUN_GetPrivateProfileStringA __imp_GetPrivateProfileStringA; /* 0x00477120 */
extern FUN_GetPrivateProfileIntA __imp_GetPrivateProfileIntA;    /* 0x0047711C */
extern FUN_RegOpenKeyExA    __imp_RegOpenKeyExA;        /* 0x00477010 */
extern FUN_RegQueryValueExA __imp_RegQueryValueExA;     /* 0x00477008 */
extern FUN_RegCloseKey      __imp_RegCloseKey;          /* 0x0047700C */
extern FUN_RegCreateKeyExA  __imp_RegCreateKeyExA;      /* 0x00477004 */
extern FUN_RegSetValueExA   __imp_RegSetValueExA;       /* 0x00477000 */

/* CRT helpers */
extern int   __cdecl CRT_mkdir(const char* path, int* err); /* 0x00466590 */
extern void  __cdecl CRT_memset(char* buf, int val, int size); /* 0x00466950 */
extern int   __stdcall wsprintfA(char* buf, const char* fmt, ...);
extern int   __cdecl CRT_strlen(const char* s);           /* 0x00466930 */
extern int   __cdecl CRT_memmove(void* dst, const void* src, size_t n); /* memmove */

} /* extern "C" */

extern void* __cdecl operator_new(size_t size);          /* 0x00465CE0 */

/* Game-internal functions (declared by name) */
extern void   __fastcall GameObject_Update(void* obj);               /* 0x00405C40 */
extern void   __fastcall GameObject_BaseCtor(void* obj, int a, int b, int c, int d); /* 0x00405790 */
extern void   __fastcall GameObject_Draw(void* obj);                 /* 0x00405E60 */
extern void   __fastcall GameObject_DrawConnected(void* obj, int l, int t, char r, int b, int scroll, uint32_t flags); /* 0x00405FD0 */
extern int    __fastcall GameObject_HitTest(void* obj, int x, int y); /* 0x00405680 */
extern void   __fastcall GameObject_SetWorldPos(void* obj, int x, int y); /* 0x00405C00 */
extern int    __thiscall GameObject_PtInRect(void* rect, int x, int y); /* 0x00436A10 */
extern int    __thiscall ResourceManager_GetById(void* rmgr, int res_id); /* TBD */
extern void   __thiscall PlayerConfig_Ctor(void* cfg, const char* path); /* 0x00452CE0 */
extern void   __thiscall Config_GetIniString(void* cfg, const char* section, const char* key, const char* def, char* out, int max); /* 0x00452D80 */
extern void   __thiscall TileMap_InvalidateRect(void* tm, int l, int t, int r, int b); /* 0x00455840 */

/* Timer sub-object vtable methods (collection interface) */
/* Vtable 0x477758 / 0x477798 — Timer specific */

/* Global game state objects */
extern int32_t  g_game_mode;            /* 0x004851F4 */
extern int32_t  g_build_mode;           /* 0x00485234 */
extern uint8_t  g_placement_valid;      /* 0x004AA648 */
extern uint8_t  g_is_town_mode;         /* 0x004AA7B8 */
extern uint8_t  g_town_click_valid;     /* 0x004AA664 */
extern int32_t  g_demo_mode;            /* 0x004A9918 */
extern uint8_t  g_allow_building_placement; /* 0x004FD3DC */
extern uint8_t  g_ddraw_active;         /* DDRAW active flag */
extern int32_t  g_world_width;          /* 0x004AAD0C */
extern int32_t  g_world_height;         /* 0x004AAD10 */
extern int32_t  g_viewport_x;           /* viewport scroll X */
extern int32_t  g_viewport_y;           /* viewport scroll Y */
extern int32_t  g_client_offset_x;      /* client left offset */
extern int32_t  g_client_offset_y;      /* client top offset */
extern int32_t  g_client_width;         /* client width */
extern int32_t  g_client_height;        /* client height */
extern char     g_install_path[256];    /* 0x004A99C8 */
extern char     g_remote_res_path[256]; /* 0x004A97A8 */


/* Game-specific globals */
extern void*    g_tilemap;           /* 0x4AAD08 */
extern int32_t  g_object_count;      /* object array count */
extern void**   g_object_array;      /* object array */
extern void*    g_scripted_object;   /* scripted object singleton */
extern void*    g_resmgr;            /* resource manager */
extern void*    g_building_mgr;      /* building manager */
extern int32_t  g_player_id;         /* player ID */
extern int32_t  g_player_color;      /* player color */
extern void*    g_town_view;         /* town view object */
extern void*    g_ddraw_building;    /* DDRAW building */
extern void*    g_ddraw_drag_rect;   /* DDRAW drag rect */
extern void*    g_town_overlay_bounds; /* town overlay bounds */
extern int32_t  g_town_overlay_threshold; /* town overlay threshold */
extern uint8_t  g_has_second_overlay; /* has second overlay */
extern void*    g_second_overlay_bounds; /* second overlay bounds */
extern void*    g_audio;             /* audio system */
extern int32_t  g_building_count;    /* building count */
extern void**   g_building_list;     /* building list */
extern void**   g_building_data;     /* building data array */
extern int32_t  g_vehicle_count;     /* vehicle count */
extern void**   g_vehicle_list;      /* vehicle list */
extern void**   g_vehicle_data;      /* vehicle data array */
extern void*    g_tooltip_mgr;       /* tooltip manager */
extern void*    g_world;             /* world object */
extern void*    g_town;              /* town object */
extern void*    g_cursor;            /* cursor object */
extern void*    g_postcard;          /* postcard object */
extern void*    g_postcard_send;     /* postcard send object */
extern void*    g_ui_main;           /* UI main object */
extern int32_t  some_variable;       /* temporary variable */

/* Helper functions */
void  TileMap_ClearInputProcessedFlag(void* tm);
void* TileMap_GetObjectAtEx(void* tm, int x, int y, int* out);
void* TileMap_GetObjectAt(void* tm, int x, int y, int layer);
int   TileMap_HandleClick(void* tm, int x, int y);
void  TileMap_SetObject(void* tm, int x, int y, int obj);
int   TileMap_FindObjectByPos(void* tm, int x, int y);
int   BuildingMgr_FindAndNotify(void* mgr, int x);
int   DDRAW_HitTest(void* ddraw, int x, int y);
int   DDRAW_HitTestWithDrag(void* ddraw, int x, int y);
int   RESDATA_ScriptedObject_CheckClick(void* obj, int x, int y);
int   RESDATA_ScriptedObject_GetDragOffset(void* obj, int* ox, int* oy);
int   RESDATA_ScriptedObject_IsDragging(void* obj, int x, int y);
int   RESDATA_ScriptedObject_HitTest(void* obj, int x, int y);
void  UI_CreateMessageBox(void* mgr, int resId, int p2, char p3, int x, int y, int p7);
void  GameAudio_PlayResource(void* audio, int resId);
int   World_ProcessAudio(void* world, int x, int y);
void  PlaySoundAt(int id, int x, int y, int flags);
int   Town_PostcardClickHandler(void* town, int x, int y);

/* Globals also declared in types.h — outside extern "C" to avoid linkage conflict */
/* g_game_time declared in types.h */
/* g_main_window declared in types.h */
/* g_config_ini declared in types.h */

/* ================================================================== */
/* String constants (from .rdata)                                      */
/* ================================================================== */
static const char s_ini_directories[]   = "DIRECTORIES";     /* 0x0047E214 */
static const char s_ini_remote_res[]    = "RemoteRes";       /* 0x0047E208 */
static const char s_ini_install_path[]  = "InstallPath";     /* 0x0047E220 — alias for default */
static const char s_default_path[]      = "";                /* 0x0047E224 — empty default */
static const char s_lego_ini[]          = "lego.ini";        /* 0x0047E228 */
static const char s_backslash[]         = "\\";              /* 0x0047E234 */
static const char s_busy_ani[]          = "busy.ani";        /* 0x0047E400 */
static const char s_cur_fmt[]           = "%sCURSORS\\%s";   /* 0x0047E3F0 — wsprintfA format */
static const char s_reg_key[]           = "SOFTWARE\\Intelligent Games\\LEGO Loco"; /* 0x0047E238 */

/* Timer sub-object vtables (collection-based, used at +0x10C) */
#define VTBL_TIMER_INIT_VTABLE        0x00477798  /* initial vtable for timer sub-object */
#define VTBL_TIMER_RUNNING_VTABLE     0x00477758  /* running vtable after timer init */

/* ================================================================== */
/* Game constructor                                                     */
/* Address: 0x410510                                                    */
/* ================================================================== */
Game::Game()
{
    int i;

    /* Call Entity base constructor: Entity(-1, -1, 0, 0) => GameObject_BaseCtor(this, -1, -1, 0, 0) */
    GameObject_BaseCtor(this, -1, -1, 0, 0);

    /* Initialize timer sub-object */
    this->timer_sub_ptr = (int32_t)VTBL_TIMER_INIT_VTABLE;  /* +0x10C = 0x477798 */
    this->timer_array_ptr = 0;                                      /* +0x110 = NULL */
    this->timer_count = 0;                                          /* +0x114 = 0 */

    /* Allocate timer data array (10 entries * 4 bytes = 0x28 bytes) */
    int* timers = (int*)operator_new(0x28);
    this->timer_array_ptr = (int32_t)timers;

    if (timers) {
        for (i = 10; i != 0; i--) {
            *timers = 0;
            timers++;
        }
        this->timer_count = 10;
    } else {
        this->timer_array_ptr = 0;
        this->timer_count = 0;
    }

    /* Switch to running timer vtable */
    this->timer_sub_ptr = (int32_t)VTBL_TIMER_RUNNING_VTABLE;  /* +0x10C = 0x477758 */
    this->timer_edit = 0;                                             /* +0x118 = 0 */

    /* Set Game vtable */
    *(void**)this = (void*)VTBL_GAME;              /* +0x00 = 0x477718 */

    /* Mark active */
    this->initialized = 1;                          /* +0x18 = 1 — inherited from GameObject */
    this->visible = 1;                              /* +0x24 = 1 — used as "parent active" flag */

    /* Initial screen mode: windowed, cursor visible */
    this->SetScreenMode(0, 1, 0);                   /* capture=0, show=1, custom=0 */

    /* Initialize state fields */
    this->cursor_sound_id = -1;                     /* +0x88 = 0xFFFFFFFF */
    this->selected_object_ptr = 0;                  /* +0xE8 = NULL */
    this->selected_visible = 0;                     /* +0xEC = 0 */

    /* Zero mouse speed params */
    this->mouse_spi3[0] = 0;                                       /* +0xF0 */
    this->mouse_spi3[1] = 0;
    this->mouse_spi3[2] = 0;
    this->mouse_spi4[0] = 0;                                       /* +0xFC */
    this->mouse_spi4[1] = 0;
    this->mouse_spi4[2] = 0;

    /* Save current mouse speed params from system */
    SystemParametersInfoA(3, 0, this->mouse_spi3, 0);              /* SPI_GETMOUSE (setting 3) */
    SystemParametersInfoA(4, 0, this->mouse_spi4, 0);              /* SPI_GETMOUSE (setting 4) */

    /* Clear all event flags */
    this->left_click_flag = 0;      /* +0xA4 */
    this->right_click_flag = 0;     /* +0xB4 */
    this->mouse_move_flag = 0;      /* +0xC0 */
    this->mouse_drag_flag = 0;      /* +0xD0 */
    this->click_on_selected = 0;    /* +0xE2 */
    this->mouse_drag_mode = 0;      /* +0xE0 */
    this->mouse_drag_handled = 0;   /* +0xE1 */

    /* +0xE7 = 0 — unknown pad */
    this->_pad_E7 = 0;                                             /* +0xE7 */

    /* Edit/misc state */
    this->timer_edit = 0;           /* +0x118 */

    /* Cursor disabled = 0 */
    this->cursor_disabled = 0;      /* +0x8D */

    /* Run one initial update tick */
    this->Update();
}

/* ================================================================== */
/* Game destructor body                                                 */
/* Address: 0x4106C0 (scalar deleting destructor at 0x410700)           */
/* ================================================================== */
Game::~Game()
{
    /* Restore original mouse speed params */
    SystemParametersInfoA(4, 0, this->mouse_spi3, 0);              /* SPI_SETMOUSE (setting 4) */

    /* Stop timer sub-object: call vtable[5] (+0x14) — Timer::Stop */
    void* timer_sub = (void*)(intptr_t)this->timer_sub_ptr;
    if (timer_sub) {
        (*(void (__thiscall**)(void*))((int*)timer_sub + 5))(timer_sub); /* vtable[5] on timer */
    }

    /* Switch to windowed cursor mode */
    this->SetScreenMode(0, 1, 0);   /* capture=0, show=1, custom=0 */

    /* Release resources: vtable[6] = InitBase(0, -1, 0) = resource release call */
    (*(void (__thiscall**)(void*, int, int, int))((uintptr_t*)this)[6])(this, 0, -1, 0);
}

/* ================================================================== */
/* Game_Update — MAIN per-frame game loop (7-step pipeline)             */
/* Address: 0x410840                                                    */
/* ================================================================== */
void Game::Update()
{
    bool has_any_event;

    /* Skip if parent resource not active */
    if (!this->initialized) {
        return;
    }

    /* Step 1: Animation state machine update */
    GameObject_Update(this);                                        /* 0x00405C40 */

    /* Step 2: Poll input flags. Determine if any event is pending */
    has_any_event = (this->left_click_flag != 0)         /* +0xA4 */
                 || (this->right_click_flag != 0)        /* +0xB4 */
                 || (this->mouse_move_flag != 0)         /* +0xC0 */
                 || (this->screensaver_active != 0);     /* +0x8E */

    if (this->selected_object_ptr != 0) {
        /* Step 3: Screensaver timeout check */
        if (this->screensaver_active != 0) {

/* ================================================================== */
    }
}
}

/* ================================================================== */
/* Game_UpdateCursorMode                                               */
/* ================================================================== */
void Game::UpdateCursorMode() {}

/* ================================================================== */
/* Game_ClearMouseMode                                                 */
/* ================================================================== */
void Game::ClearMouseMode() {}

/* ================================================================== */
/* Game_SetScreenMode                                                  */
/* ================================================================== */
void Game::SetScreenMode(uint8_t a, uint8_t b, uint8_t c) {}

/* ================================================================== */
/* Game_HandleCursorHover                                              */
/* ================================================================== */
void Game::HandleCursorHover() {}

/* ================================================================== */
/* Game_UpdateInputState                                               */
/* ================================================================== */
void Game::UpdateInputState() {}

/* ================================================================== */
/* Game_PlaySound                                                      */
/* ================================================================== */
void Game::PlaySound(int id) {}

/* ================================================================== */
/* Game_ScreenToWorld                                                  */
/* ================================================================== */
void Game::ScreenToWorld(int a, int b, int32_t* c, int32_t* d) {}

/* ================================================================== */
/* Game_HandleLeftClick                                                */
/* ================================================================== */
void Game::HandleLeftClick() {}

/* ================================================================== */
/* Game_HandleRightClick                                               */
/* ================================================================== */
void Game::HandleRightClick() {}

/* ================================================================== */
/* Game_SelectGameObject                                               */
/* ================================================================== */
int Game::SelectGameObject(GameObject* obj) { return 0; }

/* ================================================================== */
/* Game_DeselectGameObject                                             */
/* ================================================================== */
void Game::DeselectGameObject() {}

/* ================================================================== */
/* Game_IsScreensaverActive                                            */
/* ================================================================== */
int Game::IsScreensaverActive() { return 0; }


/* ================================================================== */
/* Game_CheckTimeInRange — Check if current time falls within range     */
/* Address: 0x412710                                                    */
/*                                                                      */
/* __cdecl free function. Time struct = int[3] (sec, min, hour).        */
/* Compares total minutes with overnight wrap support.                  */
/* Returns 1 if in range, 0 otherwise.                                  */
/* ================================================================== */
int Game_CheckTimeInRange(int* current_time, int* start_time, int* end_time)
{
    int start_m = start_time[1];    /* +0x04 = minutes */
    if (start_m == -1) return 0;
    int end_m = end_time[1];
    if (end_m == -1) return 0;
    int start_h = start_time[2];    /* +0x08 = hours */
    if (start_h == -1) return 0;
    int end_h = end_time[2];
    if (end_h == -1) return 0;

    int s = start_m + start_h * 60;
    int c = current_time[1] + current_time[2] * 60;
    int e = end_m + end_h * 60;

    if (e < s) return (s <= c || c <= e) ? 1 : 0;
    return (s <= c && c <= e) ? 1 : 0;
}
