#include "../game/Train.h"
#include "../core/VehicleEditor.h"
#include "../shared/Collection.h"
#include "../game/Panel.h"
#include "../core/CGWND.h"
#include <cstdarg>
/*
 * sdl3_undecompiled_stubs.cpp — Stubs for undecompiled loco.exe functions
 *
 * These are functions referenced by the decompiled C++ code that have NOT
 * yet been decompiled from the original binary.  They exist in loco.exe
 * but we provide dummy implementations so the game links and runs.
 *
 * As more functions are decompiled, replace stubs with real implementations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Types from the decompiled code */
#include "shared/types.h"
#include "core/GameObject.h"
#include "game/Building.h"
#include "game/Vehicle.h"
#include "world/tilemap.h"

/* Tag stubs so we can see what's being called */
#define STUB(msg)  /* fprintf(stderr, "STUB: %s\n", msg) */

/* =========================================================================
 * CRT / memory functions (from original MSVC runtime)
 * ========================================================================= */

extern "C++" {
    void* __cdecl operator_new(size_t size) {
        STUB("operator_new");
        return calloc(1, size);
    }
    void __cdecl GLOBAL_free(void* ptr) {
        STUB("GLOBAL_free");
        free(ptr);
    }
    void CRT_free(void* ptr) {
        STUB("CRT_free");
        free(ptr);
    }
    int CRT_rand(void) {
        return rand();
    }
    int CRT_sprintf_buf(char* buf, const char* fmt, ...) {
        STUB("CRT_sprintf_buf");
        va_list va; va_start(va, fmt);
        int r = vsnprintf(buf, 1024, fmt, va);
        va_end(va);
        return r;
    }
    char* CRT_strncpy(char* dst, const char* src, int n) {
        STUB("CRT_strncpy");
        strncpy(dst, src, (size_t)n);
        return dst;
    }
    char* CRT_strupr(char* s) {
        STUB("CRT_strupr");
        for (char* p = s; *p; p++) *p = (char)toupper(*p);
        return s;
    }
    int CRT_toupper(int c) { return toupper(c); }

    /* Win32 stubs */
    void Sleep(int ms) { SDL_Delay((uint32_t)ms); }
    int wsprintfA(char* buf, const char* fmt, ...) {
        va_list va; va_start(va, fmt);
        int r = vsnprintf(buf, 1024, fmt, va);
        va_end(va);
        return r;
    }
    void* GetProcessHeap(void) { return NULL; }
    int HeapFree(void*, int, void* ptr) { free(ptr); return 1; }
    void* __ftol; /* MSVC float-to-long helper */
}
}

/* =========================================================================
 * Global variables
 * ========================================================================= */

/* Graphics globals */
int   g_surface_bpp = 16;
int   g_surface_channel1 = 0;
int   g_surface_channel2 = 0;
int   g_surface_bshift = 0;
int   g_pixel_format_mask = 0;
int   g_screen_width = 640;
int   g_screen_height = 480;
int   g_viewport_x = 0;
int   g_viewport_y = 0;
int   g_viewport_rect_left = 0;
int   g_viewport_rect_top = 0;
int   g_viewport_rect_right = 640;
int   g_viewport_rect_bottom = 480;
int   g_ref_count = 0;
uint8_t g_is_fullscreen = 0;
int   g_world_width = 1024;
int   g_world_height = 768;
uint32_t g_game_time = 0;
int   DAT_004aad34 = 0;
int   DAT_004aad38 = 0;
void* g_primary_surface = NULL; /* alias: _g_primary_surface */

/* Game state globals */
void* g_asset_mgr = NULL;
void* g_audio = NULL;
void* g_audio_mgr = NULL;
void* g_config_ini = NULL;
void* g_dplay = NULL;
void* g_dplay_config = NULL;
void* g_font_small = NULL;
void* g_game = NULL;
void* g_input_mgr = NULL;
void* g_netman = NULL;
void* g_player_config = NULL;
void* g_resmgr = NULL;
void* g_tilemap = NULL;
void* g_tooltip_mgr = NULL;
void* g_main_window = NULL;
int   g_listener_x = 0;
int   g_listener_y = 0;
uint8_t g_allow_building_placement = 0;
int   g_in_build_mode = 0;
int   g_demo_mode = 0;
int   growth_factor = 2;
int   g_last_cursor_pos = 0;
char  g_install_path[256] = ".";
char  g_scene_name[64] = "";
void* _g_network_queue = NULL;
void* _g_train = NULL;
void* _g_train_resources = NULL;
int   g_trackSegmentOffsets = 0;

/* =========================================================================
 * CGWND (window manager, not fully decompiled)
 * ========================================================================= */

CGWND::CGWND(void* hInstance) {
    STUB("CGWND::CGWND");
    memset(this, 0, sizeof(*this));
}
}
CGWND::~CGWND() { STUB("CGWND::~CGWND"); }
int CGWND::InitAllSubsystems() { STUB("CGWND::InitAllSubsystems"); return 1; }
void CGWND::Cleanup() { STUB("CGWND::Cleanup"); }
void CGWND_SetMode(int mode) { STUB("CGWND_SetMode"); }
void CGWND_SetBuildMode(int mode) { STUB("CGWND_SetBuildMode"); }

/* =========================================================================
 * GameObject base — not fully decompiled
 * ========================================================================= */

GameObject::GameObject() { STUB("GameObject::GameObject"); }
GameObject::~GameObject() { STUB("GameObject::~GameObject"); }
int GameObject::HitTest(unsigned int xy) { STUB("GameObject::HitTest"); return 0; }
int GameObject::PtInRect(int x, int y) { STUB("GameObject::PtInRect"); return 1; }
void GameObject::InvalidateRect() { STUB("GameObject::InvalidateRect"); }
void GameObject::MoveTo(int x, int y) { STUB("GameObject::MoveTo"); }
void GameObject::SetName(const char* name) { STUB("GameObject::SetName"); }
void GameObject::SetFrame(int frame, bool trigger) { STUB("GameObject::SetFrame"); }
void GameObject::SetAnimState(int state) { STUB("GameObject::SetAnimState"); }
void GameObject::StopSound(int id) { STUB("GameObject::StopSound"); }
void GameObject::OnTimerTick() { STUB("GameObject::OnTimerTick"); }
void GameObject::DrawConnected(RECT r, int s, unsigned int f) { STUB("GameObject::DrawConnected"); }
void GameObject::Draw(RECT r, int s, unsigned int f) { STUB("GameObject::Draw"); }
void GameObject::InitBase(int x, int y, bool) { STUB("GameObject::InitBase"); }
void GameObject::RegisterEntity(void*, void*) { STUB("GameObject::RegisterEntity"); }
/* Free functions */
void GameObject_BaseCtor(void* obj, int a, int b, int c, int d) { STUB("GameObject_BaseCtor"); }
void GameObject_DtorBody(void* obj) { STUB("GameObject_DtorBody"); }
void GameObject_SetWorldPos(void* o, int x, int y) { STUB("GameObject_SetWorldPos"); }
void GameObject_Update(void* o) { STUB("GameObject_Update"); }
int  GameObject_HitTest(void* o, int x, int y) { STUB("GameObject_HitTest"); return 0; }
void GameObject_SetFrame(void* o, int f, bool t) { STUB("GameObject_SetFrame"); }

/* =========================================================================
 * RESDATA base — not decompiled
 * ========================================================================= */

void  RESDATA_BaseInit(void* self) { STUB("RESDATA_BaseInit"); }
void  RESDATA_DtorBase(void* self) { STUB("RESDATA_DtorBase"); }
void  RESDATA_Lock(void* p) { STUB("RESDATA_Lock"); }
void  RESDATA_Unlock(void* p) { STUB("RESDATA_Unlock"); }
void  RESDATA_SetPosition(void* p, int x, int y) { STUB("RESDATA_SetPosition"); }
void  RESDATA_CreateSpriteObject(void* p, int id) { STUB("RESDATA_CreateSpriteObject"); }
char  RESDATA_IsValidTrackIndex(void* r, short idx) { STUB("RESDATA_IsValidTrackIndex"); return 0; }
char  RESDATA_IsRoadTile(void* r) { STUB("RESDATA_IsRoadTile"); return 0; }
char  RESDATA_IsBuildingTile(void* r) { STUB("RESDATA_IsBuildingTile"); return 0; }
char  Resource_IsValidTrackIndex(void* r, short idx) { STUB("Resource_IsValidTrackIndex"); return 0; }
char  Resource_IsRoadTile(void* r) { STUB("Resource_IsRoadTile"); return 0; }
char  Resource_IsBuildingTile(void* r) { STUB("Resource_IsBuildingTile"); return 0; }
void  RESDATA_GameVehicle_Ctor(void* p, int id) { STUB("RESDATA_GameVehicle_Ctor"); }
void  RESDATA_GameVehicle_BaseDtor(void* p) { STUB("RESDATA_GameVehicle_BaseDtor"); }

/* =========================================================================
 * Building — not fully decompiled
 * ========================================================================= */

Building::Building(int id) { STUB("Building::Building"); }
Building::~Building() { STUB("Building::~Building"); }

/* =========================================================================
 * Panel, Entity, UI base classes
 * ========================================================================= */

void  Panel_DtorBody(void* self) { STUB("Panel_DtorBody"); }
Panel::~Panel() { STUB("Panel::~Panel"); }
Entity::Entity(int id, short anim, int x, int y) { STUB("Entity::Entity"); }

/* =========================================================================
 * Vehicle
 * ========================================================================= */

int   Vehicle_GetOccupantCount(int id) { STUB("Vehicle_GetOccupantCount"); return 0; }
void  Vehicle_SetState(void* v, int s) { STUB("Vehicle_SetState"); }
int   Vehicle_GetNearestTrack(int i) { STUB("Vehicle_GetNearestTrack"); return -1; }
void* Vehicle_FindPath(void*, void*, unsigned char) { STUB("Vehicle_FindPath"); return NULL; }
void  Vehicle_InitOccupant(void*, int) { STUB("Vehicle_InitOccupant"); }
void  Vehicle_Stop(void*, int, unsigned char) { STUB("Vehicle_Stop"); }
int   Vehicle_IsMoving(void*) { STUB("Vehicle_IsMoving"); return 0; }
int   Vehicle_DetachAll(int) { STUB("Vehicle_DetachAll"); return 0; }

/* =========================================================================
 * GAMESTATE helpers  (GAMESTATE_FindAdjacentTrack / FindTrackPosition
 * are implemented as EditorState member methods; see EditorState.cpp.)
 * ========================================================================= */

void  GAMESTATE_EditorState_Ctor(void*, char) { STUB("GAMESTATE_EditorState_Ctor"); }
void  GAMESTATE_InitTrackAtPosition(void*, int, int) { STUB("GAMESTATE_InitTrackAtPosition"); }

/* =========================================================================
 * Train
 * ========================================================================= */

void TrainEntity::BaseDtor() { STUB("TrainEntity::BaseDtor"); }
void Train_QueueMessage() { STUB("Train_QueueMessage"); }

/* =========================================================================
 * Resource manager
 * ========================================================================= */

void* RESMGR_GetById(void*, unsigned int) { STUB("RESMGR_GetById"); return NULL; }
void* RESMGR_LoadSoundResource() { STUB("RESMGR_LoadSoundResource"); return NULL; }
void  RESMGR_PlaySound() { STUB("RESMGR_PlaySound"); }
void  RESMGR_ReleaseSoundResource() { STUB("RESMGR_ReleaseSoundResource"); }
void* ResourceManager_GetById() { STUB("ResourceManager_GetById"); return NULL; }
void* ResourceManager_GetStringById() { STUB("ResourceManager_GetStringById"); return (void*)""; }

/* =========================================================================
 * ScriptEngine
 * ========================================================================= */

void ScriptEngine_constructor() { STUB("ScriptEngine_constructor"); }
void ScriptEngine_destructor() { STUB("ScriptEngine_destructor"); }
void ScriptEngine_Init() { STUB("ScriptEngine_Init"); }
void ScriptEngine_Call() { STUB("ScriptEngine_Call"); }

/* =========================================================================
 * Collections
 * ========================================================================= */

void* Collection::GetAt(int idx) { STUB("Collection::GetAt"); return NULL; }
int   SortedCollection::Compare(void* a, void* b) { STUB("SortedCollection::Compare"); return 0; }
void  SortedCollection::SortRange(int lo, int hi) { STUB("SortedCollection::SortRange"); }

/* =========================================================================
 * Sprite helpers
 * ========================================================================= */

void Sprite_Init() { STUB("Sprite_Init"); }
void Sprite_Destroy() { STUB("Sprite_Destroy"); }
void Sprite_SetState() { STUB("Sprite_SetState"); }

/* =========================================================================
 * TileMap
 * ========================================================================= */

void TileMap_InvalidateRect(TileMap* tm, int l, int t, int r, int b) { STUB("TileMap_InvalidateRect"); }
void TrackPiece_SetZoom(void* tp, short z) { STUB("TrackPiece_SetZoom"); }

/* =========================================================================
 * UI functions
 * ========================================================================= */

void UIPANEL_Blit(void* a, unsigned int b, unsigned int c, int d, unsigned int e, int** f, unsigned int g, unsigned int h, int i, unsigned int j, unsigned int k) { STUB("UIPANEL_Blit"); }
void UIPANEL_BeginPaint(void* p) { STUB("UIPANEL_BeginPaint"); }
void UIPANEL_EndPaintEx() { STUB("UIPANEL_EndPaintEx"); }
void UIPANEL_InitScrollPanel() { STUB("UIPANEL_InitScrollPanel"); }
void UIPANEL_ScrollPanel_Dtor() { STUB("UIPANEL_ScrollPanel_Dtor"); }
void UIPANEL_ScrollPanel_HandleDrag() { STUB("UIPANEL_ScrollPanel_HandleDrag"); }
void UI_CreateChildWindow() { STUB("UI_CreateChildWindow"); }
void UI_CreateFullWindow() { STUB("UI_CreateFullWindow"); }
void UI_CreateTooltip() { STUB("UI_CreateTooltip"); }
void UI_OnMouseLeave() { STUB("UI_OnMouseLeave"); }
void UI_PaintWindow() { STUB("UI_PaintWindow"); }
void UI_ChildWindow_Dtor() { STUB("UI_ChildWindow_Dtor"); }
void UI_ChildWindow_Render() { STUB("UI_ChildWindow_Render"); }
void UI_DestroyTooltip(void*, int) { STUB("UI_DestroyTooltip"); }
void UI_WindowBase_BaseDtor() { STUB("UI_WindowBase_BaseDtor"); }
void UI_WindowBase_Ctor() { STUB("UI_WindowBase_Ctor"); }
void UI_WindowBase_Hide() { STUB("UI_WindowBase_Hide"); }

/* =========================================================================
 * DDRAW / audio helpers
 * ========================================================================= */

const char* DDRAW_GetDdrawErrorString(int code) { STUB("DDRAW_GetDdrawErrorString"); return "DDRAW error"; }
const char* DDRAW_GetDsoundErrorString(int code) { STUB("DDRAW_GetDsoundErrorString"); return "DSOUND error"; }
void GameAudio_StopFinished(void*) { STUB("GameAudio_StopFinished"); }
void GameAudio_UpdateVolume() { STUB("GameAudio_UpdateVolume"); }
void Game_CheckScreensaverTimeout() { STUB("Game_CheckScreensaverTimeout"); }

/* =========================================================================
 * Asset loading
 * ========================================================================= */

void* AssetMgr_LoadFile(void* mgr, void* path, int* size) { STUB("AssetMgr_LoadFile"); return NULL; }

/* =========================================================================
 * Config
 * ========================================================================= */

int Config_GetIniInt(void* ini, const char* sec, const char* key, int def) { STUB("Config_GetIniInt"); return def; }
int Ordinal_1(int a, void* b) { STUB("Ordinal_1"); return 0; }
int Ordinal_2(void* a) { STUB("Ordinal_2"); return 0; }

/* =========================================================================
 * Input
 * ========================================================================= */

void INPUT_EditWndProc() { STUB("INPUT_EditWndProc"); }
void INPUT_ExitGame() { STUB("INPUT_ExitGame"); }
void INPUT_LoadWorld() { STUB("INPUT_LoadWorld"); }
void INPUT_NewWorld() { STUB("INPUT_NewWorld"); }
void INPUT_SaveCurrentWorld() { STUB("INPUT_SaveCurrentWorld"); }

/* =========================================================================
 * Network / DirectPlay stubs
 * ========================================================================= */

void DPLAY_CleanupPlayer(void*) { STUB("DPLAY_CleanupPlayer"); }
void DPLAY_CreatePlayer(void*) { STUB("DPLAY_CreatePlayer"); }
void DPLAY_RenderPlayer() { STUB("DPLAY_RenderPlayer"); }
void NET_CheckAssetExists() { STUB("NET_CheckAssetExists"); }
void NET_ResolveAddress() { STUB("NET_ResolveAddress"); }

/* =========================================================================
 * HelpWnd / narration
 * ========================================================================= */

uint32_t HelpWnd_PlayNarration(void* mgr, int page, uint flags) { STUB("HelpWnd_PlayNarration"); return 0; }

/* =========================================================================
 * Timer
 * ========================================================================= */

void Timer_Resize() { STUB("Timer_Resize"); }

/* =========================================================================
 * vtable globals (for RTTI/typeinfo)
 * ========================================================================= */

/* These are needed for dynamic_cast / typeid */
void* typeinfo_for_Building = NULL;
void* typeinfo_for_GameObject = NULL;
void* vtable_for_Panel = NULL;
void* vtable_for_ScriptedObject = NULL;
void* vtable_for_UIEntity = NULL;

/* =========================================================================
 * Win32 GDI stubs
 * ========================================================================= */

extern "C++" {
    #include <SDL3/SDL.h>
    void* GetDesktopWindow(void) { return NULL; }
    int GetClientRect(void* hwnd, RECT* r) {
        if (r) { r->left = 0; r->top = 0; r->right = 640; r->bottom = 480; }
        return 1;
    }
    int GetWindowRect(void* hwnd, RECT* r) {
        if (r) { r->left = 0; r->top = 0; r->right = 640; r->bottom = 480; }
        return 1;
    }
    int IntersectRect(RECT* out, const RECT* a, const RECT* b) { STUB("IntersectRect"); return 0; }
    int IsRectEmpty(const RECT* r) { STUB("IsRectEmpty"); return 1; }
    int CopyRect(RECT* dst, const RECT* src) { if(dst&&src) *dst=*src; return 1; }
    int OffsetRect(RECT* r, int dx, int dy) { if(r){r->left+=dx;r->top+=dy;r->right+=dx;r->bottom+=dy;} return 1; }
    void SetRect(RECT* r, int l, int t, int ri, int b) { if(r){r->left=l;r->top=t;r->right=ri;r->bottom=b;} return 1; }
    void SetRect(void* r, int l, int t, int ri, int b) { SetRect((RECT*)r, l, t, ri, b); }
    int PtInRect(const RECT* r, int x, int y) { STUB("PtInRect"); return r&&x>=r->left&&x<r->right&&y>=r->top&&y<r->bottom; }
    int DrawTextA(void* hdc, const char* text, int len, RECT* r, unsigned int fmt) { STUB("DrawTextA"); return 0; }
    void* LoadIconA(void* inst, const char* name) { STUB("LoadIconA"); return NULL; }
    int DeleteObject(void* obj) { STUB("DeleteObject"); return 1; }
    int SelectObject(void* hdc, void* obj) { STUB("SelectObject"); return 0; }
    int SetBkMode(void* hdc, int mode) { STUB("SetBkMode"); return 0; }
    int SetTextColor(void* hdc, int color) { STUB("SetTextColor"); return 0; }
    void* DefWindowProcA(void* hwnd, unsigned int msg, unsigned int wp, int lp) { STUB("DefWindowProcA"); return NULL; }
    int PostMessageA(void* hwnd, unsigned int msg, unsigned int wp, int lp) { STUB("PostMessageA"); return 1; }

    /* File I/O */
    void* CreateFileA(const char* path, int access, int share, void* sec, int disp, int flags, void* tmpl) {
        STUB("CreateFileA"); return NULL;
    }
    int CloseHandle(void* h) { STUB("CloseHandle"); return 1; }
    int ReadFile(void* h, void* buf, int size, int* read, void* ov) {
        STUB("ReadFile"); if(read) *read = 0; return 0;
    }
    int WriteFile(void* h, const void* buf, int size, int* written, void* ov) {
        STUB("WriteFile"); if(written) *written = size; return 1;
    }
    int GetFileSize(void* h, int* high) { STUB("GetFileSize"); return 0; }
    int DeleteFileA(const char* path) { STUB("DeleteFileA"); return 0; }
    int GetLastError(void) { return 0; }

    /* Memory / string */
    void* LocalFree(void* ptr) { free(ptr); return NULL; }
    int FormatMessageA(int flags, void* src, int msgId, int lang, char* buf, int size, void* args) {
        STUB("FormatMessageA"); if(buf&&size>0) buf[0]=0; return 0;
    }
}
}

/* =========================================================================
 * Stream helpers
 * ========================================================================= */

void WIN32_StreamDestroy(int s) { STUB("WIN32_StreamDestroy"); }
void WIN32_StreamDestroyImmediate(void* s) { STUB("WIN32_StreamDestroyImmediate"); }
void WIN32_StreamOpen(void* s, int mode) { STUB("WIN32_StreamOpen"); }
int  WIN32_StreamOpenPath(void* s, const char* path, int flags, int unk) { STUB("WIN32_StreamOpenPath"); return -1; }
void WNDPROC_StreamCleanup(void* s) { STUB("WNDPROC_StreamCleanup"); }
void* WNDPROC_StreamFromMemory(void* obj, char* data, int size, int mode) { STUB("WNDPROC_StreamFromMemory"); return NULL; }
