/**
 * stubs_impl.cpp — Runtime stub implementations for linking
 *
 * Provides real definitions for CRT functions, game globals,
 * and helper functions that the decompiled C++ code expects.
 * These were in the original loco.exe binary; we provide
 * minimal working replacements.
 */

// Status: TRANSCRIBED

#include "types.h"
#include "../network/DPlayConfig.h"
#include "../network/NetworkPlayerList.h"
#include <new>
#include <cstdlib>
#include <cassert>

/* Forward-declare Entity for typed pointer globals. */
class Entity;
#include <cstring>
#include <ctime>
#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <cmath>

/* ---- Allocation ---- */
void* operator_new(size_t size) {
    static int call_count = 0;
    call_count++;
    if (call_count <= 60) {
        fprintf(stderr, "[ALLOC] operator_new(%zu) call #%d\n", size, call_count);
        fflush(stderr);
    }
    void* p = malloc(size);
    if (!p) { fprintf(stderr, "FATAL: operator_new(%zu) failed\n", size); abort(); }
    memset(p, 0, size);
    if (call_count <= 60) {
        fprintf(stderr, "[ALLOC] operator_new(%zu) returned %p\n", size, p);
        fflush(stderr);
    }
    return p;
}
void GLOBAL_free(void* ptr) { free(ptr); }
void* CRT_malloc_zero(size_t size) { return operator_new(size); }
void CRT_free(void* ptr) { free(ptr); }

/* ---- Math ---- */
int CRT_rand(void) { return rand(); }
void CRT_srand(unsigned int s) { srand(s); }
void OutputDebugStringA(const char* s) { if (s) fprintf(stderr, "DEBUG: %s\n", s); }

/* ---- String ---- */
int CRT_strlen(const char* s) { return s ? static_cast<int>(strlen(s)) : 0; }
int CRT_memmove(void* d, const void* s, size_t n) { memmove(d, s, n); return 0; }
int CRT_wcsstr(const char* a, const char* b) { return (a && b && strstr(a, b)) ? 1 : 0; }
int CRT_sprintf_buf(char* b, const char* f, ...) { return 0; }

/* ---- Time ---- */
unsigned int CRT_timeGetTime(void) { return 0; }
unsigned int CRT_time(unsigned int* t) {
    return static_cast<unsigned int>(time(reinterpret_cast<time_t*>(t)));
}

/* ---- Game globals ---- */
uint32_t g_game_time = 0;
int32_t  g_game_mode = 0;  /* 0x4851F4: dword, read/written by CGWND_SetMode */
char     g_empty_string = 0;
char     g_install_path[256] = ".";
char     g_current_save_path[0x108] = "";  /* 0x4AA8F8 — current save name */
/* g_sound_cache — 0x49161C, indexed by sound ID (PlaySound 0x447930
 * bounds-checks to 0x5000..0x605F; sized to 0x6060 so the direct
 * g_sound_cache[soundId] read is always in bounds).  The old
 * link_stubs.cpp definition was a single void* — an OOB read for every
 * sound ID. */
int32_t  g_sound_cache[0x6060] = {0};
void*    g_main_window = nullptr;
void*    g_building_mgr = nullptr;
void*    g_resmgr = nullptr;
void*    g_netman = nullptr;
/* g_input_mgr: canonical typed static object defined in input/InputMgr.cpp
 * (InputMgr g_input_mgr; — 0x4A9990).  Removed the old void* placeholder. */
void*    g_tilemap = nullptr;
void*    g_asset_mgr = nullptr;
void*    g_audio = nullptr;
void*    g_cursor = nullptr;
void*    g_town = nullptr;
void*    g_ui_main = nullptr;
void*    g_postcard = nullptr;
void*    g_postcard_send = nullptr;
Entity*  g_selected_building = nullptr;
uint8_t  g_mouse_capture = 0;           /* 0x4855AE — cursor capture flag */
BOOL (*g_IntersectRect)(RECT*, const RECT*, const RECT*) = nullptr;
BOOL (*g_IsRectEmpty)(const RECT*) = nullptr;
BOOL (*g_PtInRect)(const RECT*, int, int) = nullptr;
BOOL (*g_OffsetRect)(RECT*, int, int) = nullptr;

/* ---- Helper stubs ---- */
void Timer_Resize(void*, unsigned int) {}
void Timer_Resize(void*, int) {}
void Collection_Sort(void*) {}
void RESMGR_PlaySound(int) {}
void ScriptEngine_constructor(void*) {}
void RESDATA_ScriptEngine_Dtor(void*) {}
int  Vehicle_SetState(void*, int) { return 0; }
void UI_CenterWindow(int*, int*) {}
void Sprite_Destroy(void*) {}
void* ButtonSprite_Ctor(void*, int, int, int) { return nullptr; }
void NETMAN_QueueMessage(void*, int, void*) {}
void Sprite_SetState(void*, int) {}
void CopyRect(RECT* d, const RECT* s) { if(d&&s) *d=*s; }
void OffsetRect(RECT* r, int dx, int dy) { if(r){r->left+=dx;r->top+=dy;r->right+=dx;r->bottom+=dy;} }
int  IsRectEmpty(const RECT* r) { return !r || r->left>=r->right || r->top>=r->bottom; }


/* ---- More globals ---- */
void* g_primary_surface = nullptr;
void* g_player_config = nullptr;
void* g_config_ini = nullptr;
void* g_trainstation_window = nullptr;
void* g_dplay_peer = nullptr;
int   g_world_width = 0;
int   g_world_height = 0;
void* g_network_thread = nullptr;
void* g_network_queue = nullptr;
void* g_train_resources = nullptr;
void* g_game_config = nullptr;
void* g_scripted_object = nullptr;
void* g_ddraw_building = nullptr;
void* g_tooltip_mgr = nullptr;
void* g_second_overlay = nullptr;
void* g_world = nullptr;
void* g_timer_event_id = nullptr;
int g_mouse_spi3[3] = {0,0,0};
int g_mouse_spi4[3] = {0,0,0};
int g_mouse_spi5[3] = {0,0,0};
int DAT_004fd3a0 = 0;
int DAT_004a990c = 0;
int DAT_00485444 = 0;
/* 0x4A99B0 — 0x630-byte event-list window object (LoadEvents/
 * TimeEvents/EasterEggs; BSS in the original, constructed by CRT thunk
 * 0x45C650 -> ctor 0x41F480).  Anchor only: the typed reconstruction is
 * deferred (persistence milestone), so nothing dereferences it on the
 * host; the _WIN32 call sites take its address for the original thiscall
 * shapes (see input/InputMgr.h). */
uint8_t g_input_events[0x630] = {0};
int DAT_004ff124 = 0;
int DAT_004ff11c = 0;
int DAT_004a98b4 = 0;
void* g_world_vehicles[4] = {nullptr,nullptr,nullptr,nullptr};


/* ---- More stubs ---- */
int wsprintfA(char* buf, const char* fmt, ...) {
    if (!buf || !fmt) return 0;
    va_list args; va_start(args, fmt);
    int ret = vsnprintf(buf, 1024, fmt, args);
    va_end(args); return ret;
}

int CRT_atoi(const char* s) { return s ? atoi(s) : 0; }
    void CRT_sprintf(char* buf, const char* fmt, ...) {}

void Cursor_Render(void*, int, int, char) {}
void CGWND_TrackPiece_SetZoom(void*, int) {}
void WNDPROC_CriticalSectionLock(int*, char*) {}
int  Config_GetIniInt(void*, const char*, const char*, int def) { return def; }

/* Win32 stubs */
void* GetProcessHeap(void) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(1));
}
int   CloseHandle(void*) { return 1; }
void  Sleep(unsigned int ms) { usleep(ms * 1000); }
void  SetPixel(void*, int, int, unsigned int) {}


/* ---- Bulk stubs for remaining symbols ---- */
void* g_ddraw = nullptr;
void* g_backbuffer = nullptr;
void* g_game = nullptr;
void* g_audio_mgr = nullptr;
NetworkPlayerList* g_dplay = nullptr;
void* g_dplay_config = nullptr;
void* g_active_panel = nullptr;
void* g_font_small = nullptr;
void* g_last_cursor_pos = nullptr;
void* g_thumbpal_surface = nullptr;
extern "C" const char g_thumbpal_bmp_name[] = "";
int g_screen_width = 800;
int g_screen_height = 600;
int g_client_width = 800;
int g_client_height = 600;
int g_client_offset_x = 0;
int g_client_offset_y = 0;
int g_cursor_world_x = 0;
int g_cursor_world_y = 0;
int g_drag_start_x = 0;
int g_drag_start_y = 0;
int g_town_mode = 0;
int g_player_id = 0;
int32_t g_player_color = 0;   /* 0x4AAD48 — host-declared 32-bit for uniformity;
                               *   the binary stores the 16-bit player words
                               *   adjacently (id 0x4AAD46 / color 0x4AAD48) and
                               *   every use loads 16 bits (was void* in
                               *   defsym_stubs.cpp) */
int32_t g_demo_mode = 0;
uint8_t g_ddraw_active = 1;
uint8_t g_disable_input = 0;
uint8_t g_allow_building_placement = 0;   /* 0x4FD3DC — loader/building placement
                                             flag.  The original is a BSS global
                                             (zero-initialized; the gap between
                                             .data and .rsrc); the loader saves
                                             and restores it around its work. */
void* g_town_view = nullptr;
void* g_tile_occupied_bitmap = nullptr;
int ATTR_0047f108 = 0;
int DAT_00481170 = 0;
int DAT_0048118c = 0;
int DAT_00481190 = 0;
int DAT_00481194 = 0;
int s_AW_Blit_failure_reported_0047e0d8 = 0;

/* Win32 stubs */
void GetWindowTextA(void*, char*, int) {}
int GetLastError(void) { return 0; }
int FormatMessageA(int, void*, int, int, char*, int, void*) { return 0; }
void* LocalFree(void*) { return nullptr; }
void* FindFirstFileA(const char*, void*) { return nullptr; }
int FindNextFileA(void*, void*) { return 0; }
int FindClose(void*) { return 0; }
int CreateDirectoryA(const char*, void*) { return 0; }
int DeleteFileA(const char*) { return 0; }
int GetFileAttributesA(const char*) { return -1; }
int ClientToScreen(void*, void*) { return 0; }
int SetCursorPos(int, int) { return 0; }

/* Critical section stubs */
void InitializeCriticalSection(void*) {}
void EnterCriticalSection(void*) {}
void LeaveCriticalSection(void*) {}
void DeleteCriticalSection(void*) {}

/* CRT stubs */
void CRT_strncpy(void*, void*, int) {}
void CRT_0x4681D0(int) {}
void CRT_0x468480(char*, void*) {}
void CRT_0x468610(void*, unsigned int, unsigned int, int) {}
void* CRT_malloc_zero(unsigned int sz) { return operator_new(static_cast<size_t>(sz)); }
void* operator_new(unsigned int sz) { return operator_new(static_cast<size_t>(sz)); }

/* vtable globals (needed for some UI classes) */
void* vtable_for_UIEntity = nullptr;
void* vtable_for_Collection = nullptr;
int growth_factor = 2;

/* DDRAW stubs */
void Cursor_SetCapture(void*, unsigned char) {}
void DDRAW_UnlockPrimary() {}
void Cursor_InitSprites(void*) {}
void Cursor_UnlockAllSurfaces(void*) {}
void DDRAW_GetSurfaceWidthHeight(void*, unsigned short*, unsigned short*) {}
void DDRAW_SetSurfaceFormat(void*, int) {}
void DDRAW_RestoreSurfaces(void*, void*) {}
void DDRAW_SpriteDataCtor(void*, int) {}
void DDRAW_SpriteDataDtor(void*) {}
void DDRAW_SelectBuilding(void*, int) {}

/* RESOURCE stubs (RESMGR_IsSaveHeader/LoadResource/ReleaseResource/
 * ResourceData_Init are real code in resources/ResDataSave.cpp) */
void RESDATA_SetPosition(void*, int, int) {}
void RESDATA_BaseInit(void*) {}
void RESDATA_DtorBase(void*) {}
/* RESDATA tile-type predicates — verified from Ghidra disassembly.
 * Each checks byte at resource+0x63A against known type ranges.
 * Addresses: IsBuildingTile 0x44BD30, IsRoadTile 0x44BD10,
 *            IsWaterTile 0x44BD50, IsTrackTile 0x44BD70,
 *            IsSceneryTile 0x44BD90, GetTileCategory 0x44BDB0.
 *
 * Tile type byte at RESDATA+0x63A values:
 *   0x01-0x04 = road, 0x07-0x0A = building (7,8,9,10),
 *   0x0E-0x0F = water, 0x10-0x11 = track/rail,
 *   0x12-0x13 = scenery.
 *
 * On the host, the resource pointer may be a raw int32_t cast (from
 * ResourceManager_GetById bridge) or a real RESDATA pointer.  Both paths
 * read the same +0x63A byte. */

#ifndef _WIN32
#include <cstdint>
#endif

uint8_t RESDATA_IsBuildingTile(int32_t tile_obj)
{
    /* 0x44BD30: check byte at +0x63A for {0x07,0x08,0x09,0x0A} */
    if (tile_obj == 0) return 0;
    uint8_t b = *reinterpret_cast<const uint8_t*>(
        static_cast<const char*>(reinterpret_cast<const void*>(static_cast<intptr_t>(tile_obj))) + 0x63A);
    return (b == 0x07 || b == 0x08 || b == 0x09 || b == 0x0A) ? 1 : 0;
}

uint8_t RESDATA_IsRoadTile(int32_t tile_obj)
{
    /* 0x44BD10: check byte at +0x63A for {0x01,0x02,0x03,0x04} */
    if (tile_obj == 0) return 0;
    uint8_t b = *reinterpret_cast<const uint8_t*>(
        static_cast<const char*>(reinterpret_cast<const void*>(static_cast<intptr_t>(tile_obj))) + 0x63A);
    return (b == 0x01 || b == 0x02 || b == 0x03 || b == 0x04) ? 1 : 0;
}

uint8_t RESDATA_IsWaterTile(int32_t tile_obj)
{
    /* 0x44BD50: check byte at +0x63A for {0x0E,0x0F} */
    if (tile_obj == 0) return 0;
    uint8_t b = *reinterpret_cast<const uint8_t*>(
        static_cast<const char*>(reinterpret_cast<const void*>(static_cast<intptr_t>(tile_obj))) + 0x63A);
    return (b == 0x0E || b == 0x0F) ? 1 : 0;
}

uint8_t RESDATA_IsTrackTile(int32_t tile_obj)
{
    /* 0x44BD70: check byte at +0x63A for {0x10,0x11} */
    if (tile_obj == 0) return 0;
    uint8_t b = *reinterpret_cast<const uint8_t*>(
        static_cast<const char*>(reinterpret_cast<const void*>(static_cast<intptr_t>(tile_obj))) + 0x63A);
    return (b == 0x10 || b == 0x11) ? 1 : 0;
}

uint8_t RESDATA_IsSceneryTile(int32_t tile_obj)
{
    /* 0x44BD90: check byte at +0x63A for {0x12,0x13} */
    if (tile_obj == 0) return 0;
    uint8_t b = *reinterpret_cast<const uint8_t*>(
        static_cast<const char*>(reinterpret_cast<const void*>(static_cast<intptr_t>(tile_obj))) + 0x63A);
    return (b == 0x12 || b == 0x13) ? 1 : 0;
}

uint32_t RESDATA_GetTileCategory(void* ptr, int16_t a, uint16_t b)
{
    /* 0x44BDB0: dispatches on type byte at +0x63A.
     * Returns 0x100 | something on match, 0x??00 on no-match.
     * Host stub: return 0 (no category match) — the original logic
     * requires full RESDATA resource objects with player/color fields. */
    if (ptr == nullptr) return 0;
    uint8_t typeByte = *reinterpret_cast<const uint8_t*>(
        static_cast<const char*>(ptr) + 0x63A);
    (void)a; (void)b;
    /* For now, return 0 for everything.  The full implementation needs
     * the +0x169, +0x16B, +0x16C fields which are only available on
     * native RESDATA objects, not the SDL bridge's lightweight structs. */
    if (typeByte == 0x01 || typeByte == 0x02 || typeByte == 0x03 || typeByte == 0x04) {
        fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
        assert(0 && "stub reached — GetTileCategory player/color-dependent path");
    }
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — GetTileCategory fallthrough");
}
void RESDATA_CreateChildSprite(void*, int, int, int) {}
void RESDATA_HitTestChildren(void*, int, int) {}
void Panel_DtorBody(void*) {}

/* Game stubs */
void Game_SetScreenMode(void*, char, char, char) {}
void Game_CheckScreensaverTimeout(int*) {}
void Game_DeselectGameObject(int) {}
void Game_SelectGameObject(void*, void*) {}
void GameObject_StopSound(void*, int) {}
void GameObject_Update(void*) {}
void GameObject_Draw(void*) {}
void GameObject_PtInRect(void*, int, int) {}
void GameObject_DtorBody(void*) {}
void GameObject_BaseCtor(void*, int, int, int, int) {}
void Entity_GetSubObjectPosition(void*, int*, int) {}

/* UI stubs */
void UI_WindowBase_Ctor(void*, void*, unsigned int) {}
void UI_WindowBase_BaseDtor(void*) {}
void UI_WindowBase_Hide(void*) {}
void UI_CreateFullWindow(void*, int, void*, int, int, int, int, void*, void*, unsigned int) {}
void UI_CreateChildWindow(void*, int, int) {}
void UI_CleanupTooltips(void*) {}
void UI_DestroyTooltip(void*, int) {}
void UI_CreateTooltip(void*, int, int, int, int) {}
void UI_IsBitmapReady(int) {}
void UI_Window_UpdateScroll(int*) {}
void UIEntity_Ctor(void) {}
void UIPANEL_Blit(void*, int, int, int, int, void*, int, int, int, int, int) {}
void UIPANEL_BeginPaint(void*) {}
void UIPANEL_EndPaintEx(void*, void*, int, unsigned char, RECT*) {}
void UIPANEL_CreateSurface(void*) {}
void UIPANEL_StretchBlit(void*, void*, int, int, int) {}
void UIPANEL_SetClipRect(void*, int, int) {}
void UIPANEL_ScrollPanel_HandleDrag(void*, int, int) {}
void UIPANEL_ScrollPanel_Dtor(void*) {}
void UIPANEL_InitScrollPanel(void*) {}
void UIPANEL_FillRect(void*, int, int) {}

/* Various other stubs */
void Sprite_Init(void*) {}
void Sprite_SetState(void*, int, int*) {}
void Sprite_Destroy(void) {}
void ButtonSprite_Ctor(void*, int) {}
void HelpWnd_PlayNarration(void*, int, int) {}
void Town_BlitElement(void*, unsigned int, unsigned int, int, unsigned int, void*, unsigned int, unsigned int, int, unsigned int, unsigned int) {}
void Town_SelectBuilding(void*, int) {}
void TileMap_InvalidateDirtyRects(void*, char) {}
void TileMap_InvalidateRect(void*, int, int, int, int) {}
void ScriptEngine_Call(void*) {}
void ScriptEngine_Init(void*) {}
void CGWND_SetMode(void*) {}
void CGWND_SetPause(void*, char) {}
void CGWND_SetBuildMode(int) {}
void World_Init(void*) {}
void PixelDataCache_LookupAsset(void*, int, int) {}
void PixelDataCache_GetEntryCount(void*) {}
void PixelDataCache_Unlock(void*, int) {}
void DPLAY_RenderPlayer(void*, void*, int, void*, int, int, unsigned int, RECT*) {}
void PlaySoundAt(int, int, int, int) {}
void Collection_Resize(int) {}
void Collection_GetAt(int) {}
void SortedCollection_Compare(void*, void*) {}
void SortedCollection_SortRange(int, int) {}

/* ================================================================== */
/* GameLoop.cpp dependency stubs (added 2026-07-25)                    */
/* ================================================================== */

/* Subsystem constructors */
void* GameConfig_constructor(void* memory)
{
    // GameConfig_constructor @ 0x440C60 initializes the 0xB0-byte DPlayConfig
    // at DAT_004FD3A8. Keep that binary-facing object available to the menu.
    if (!memory) return nullptr;
    auto* config = new (memory) DPlayConfig();
    extern void* _g_netman_state;
    extern void* g_netSettings;
    _g_netman_state = config->binary_data();
    // g_netSettings is the same binary DAT_004FD3A8 object under a stale
    // translation-unit name used by TrainSubsystem.
    g_netSettings = config->binary_data();
    return config;
}
void* NETMAN_constructor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); return nullptr; }
void* PlayerRecord_constructor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); return nullptr; }
void* PixelDataCache_Ctor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); return nullptr; }

/* Subsystem init */
int   DDRAW_Init(void) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); return 1; }
void  TileMap_Init(void*, char) {}
void  UIPANEL_Hide(void*, void*) {}

/* Per-frame updates */
void  NETMAN_Update(void*) {}
void  RESMGR_VehicleAnimationTick(void*) {}
void  World_UpdateTick(void*) {}
void  UI_HideTooltip(void*) {}
void  RESDATA_ScriptedObject_Update(void*) {}
/* Town_TrackBuilding (0x42D1A0) and DDRAW_UpdateBuilding (0x459DA0) are
 * implemented in src/sdl3_shims/sdl3_town_mode3.cpp for the host build.
 * The Win32 build links the original binary implementations. */
extern void Town_TrackBuilding(void*);
extern void DDRAW_UpdateBuilding(void*);
/* INPUT_GetSaveFileName / INPUT_SaveCurrentWorld / INPUT_FindObjectAt /
 * INPUT_PlaceObject / INPUT_RemoveObject: canonical definitions moved to
 * input/InputMgr.cpp (0x41DD40 real, the rest loud deferred stubs). */
void  BuildingMgr_UpdateAll(void*) {}

/* Asset enumeration */
void  AssetMgr_EnumerateCategory(unsigned int**) {}

/* Timer callback */
extern "C" void LAB_0045c520(void) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached — LAB_0045c520"); }

/* Windows API extras */
extern "C" {
void* CreateEventA(void*, int, int, const char*) { return (void*)1; }
int   timeBeginPeriod(unsigned int) { return 0; }
int   timeSetEvent(unsigned int, unsigned int, void*, unsigned int, unsigned int) { return 1; }
}
