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
#include "../network/DirectPlay.h"
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
void* operator_new(size_t size);
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
void GLOBAL_free(void* ptr);
void GLOBAL_free(void* ptr) { free(ptr); }
void* CRT_malloc_zero(size_t size);
void* CRT_malloc_zero(size_t size) { return operator_new(size); }
void CRT_free(void* ptr);
void CRT_free(void* ptr) { free(ptr); }

/* ---- Math ---- */
int CRT_rand(void);
int CRT_rand(void) { return rand(); }
void CRT_srand(unsigned int s);
void CRT_srand(unsigned int s) { srand(s); }
void OutputDebugStringA(const char* s);
void OutputDebugStringA(const char* s) { if (s) fprintf(stderr, "DEBUG: %s\n", s); }

/* ---- String ---- */
int CRT_strlen(const char* s);
int CRT_strlen(const char* s) { return s ? static_cast<int>(strlen(s)) : 0; }
int CRT_memmove(void* d, const void* s, size_t n);
int CRT_memmove(void* d, const void* s, size_t n) { memmove(d, s, n); return 0; }
int CRT_wcsstr(const char* a, const char* b);
int CRT_wcsstr(const char* a, const char* b) { return (a && b && strstr(a, b)) ? 1 : 0; }
int CRT_sprintf_buf(char* b, const char* f, ...);
int CRT_sprintf_buf(char* b, const char* f, ...) { return 0; }

/* ---- Time ---- */
unsigned int CRT_timeGetTime(void);
unsigned int CRT_timeGetTime(void) { return 0; }
unsigned int CRT_time(unsigned int* t);
unsigned int CRT_time(unsigned int* t) {
    return static_cast<unsigned int>(time(reinterpret_cast<time_t*>(t)));
}

/* ---- Game globals ---- */
uint32_t g_game_time = 0;
uint8_t  g_lock_update_flag = 0;  /* 0x4851F0: guards TileMap::InvalidateDirtyRects reentrancy */
int32_t  g_game_mode = 0;  /* 0x4851F4: dword, read/written by CGWND_SetMode */
char     g_empty_string = 0;
/* Town selection/overlay globals — declared extern in world/tilemap.h but
 * never defined anywhere; reachable only once TileMap::ProcessRect and
 * TileMap::InvalidateDirtyRects stopped being dormant (2026-08-06 render
 * path fix), at which point every one of these reads address 0. */
int32_t  g_town_overlay_rect = 0;          /* 0x48538C */
int32_t  g_town_overlay_left = 0;          /* 0x485390 */
int32_t  g_town_overlay_top = 0;           /* 0x485394 */
int32_t  g_town_overlay_right = 0;         /* 0x485398 */
void*    g_cursor_surface = nullptr;       /* 0x4FD3C8 */
int32_t  g_town_selection_rect_left = 0;   /* 0x4854D0 */
int32_t  g_town_selection_rect_top = 0;    /* 0x4854D4 */
int32_t  g_town_selection_rect_right = 0;  /* 0x4854D8 */
int32_t  g_town_selection_rect_bottom = 0; /* 0x4854DC */
uint8_t  g_has_selection = 0;              /* 0x4854EC */
uint8_t  g_placement_valid = 0;            /* 0x4AA648 */
/* g_flag_4A9F80 — 0x4A9F80, read once by Game::UpdateInputState
 * (core/Game.cpp:643). Declared extern in Game.cpp but never defined
 * anywhere: a link-time dangling reference resolved to a null GOT slot,
 * so the first real Game_UpdateInputState call (i.e. the first mouse
 * click ever routed into mode-3 Town gameplay) segfaulted dereferencing
 * it — undiscovered until the BUG-mode-3-render-freeze.md fix made SDL
 * input reach Game for the first time. In the original binary this
 * address has exactly one xref (that same read); nothing ever writes
 * it, so it is always zero in retail — a vestigial/dev-only flag. */
uint8_t  g_flag_4A9F80 = 0;                /* 0x4A9F80 */
uint8_t  g_placement_blocked = 0;          /* 0x48558C */
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
void*    g_netman = nullptr;
/* g_input_mgr: canonical typed static object defined in input/InputMgr.cpp
 * (InputMgr g_input_mgr; — 0x4A9990).  Removed the old void* placeholder.
 * g_resmgr/g_tilemap: same pattern — canonical typed objects/pointers are
 * `ResourceManager g_resmgr;` (resources/ResourceManager.cpp, 0x4855E8)
 * and `void* g_tilemap;` (graphics/DDRAW.cpp, 0x4AAD08); these void*
 * placeholders were a real type mismatch masked by LINK-001. */
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
void Timer_Resize(void*, unsigned int);
void Timer_Resize(void*, unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Timer_Resize(void*, int);
void Timer_Resize(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Collection_Sort(void*);
void Collection_Sort(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESMGR_PlaySound(int);
void RESMGR_PlaySound(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
/** ScriptEngine_constructor — ABI bridge (address: 0x4493A0)
 *  Sets vtable to 0x4782A4, calls InitializeCriticalSection at +0x04.
 *  Called by: BuildingComplex_Ctor (0x434523) on an embedded
 *  BuildingCollectionLock (game/BuildingMgr.h, 0x1C bytes) — genuinely
 *  narrower than a full host ScriptEngine (sizeof(ScriptEngine) == 64 on
 *  this 64-bit host), so placement-constructing a real ScriptEngine there
 *  would overflow; this bridge intentionally replicates only the +0x00
 *  vtable write and +0x04 critical-section init that ScriptEngine::
 *  ScriptEngine() itself does, matching the original 0x1C-byte object's
 *  real footprint. GameLoop_Setup's own, unrelated, full-sized
 *  ScriptEngine allocation (g_train_resources) now placement-constructs
 *  the real class directly instead of calling through here — see
 *  core/GameLoop.cpp. */
void* ScriptEngine_constructor(void* self);
void* ScriptEngine_constructor(void* self) {
    extern void InitializeCriticalSection(void*);
    extern const void* PTR_RESDATA_ScriptEngine_Cleanup_004782a4;
    *static_cast<const void**>(self) = &PTR_RESDATA_ScriptEngine_Cleanup_004782a4;
    InitializeCriticalSection(static_cast<char*>(self) + 4);
    return self;
}
/** RESDATA_ScriptEngine_Dtor — ABI bridge to ScriptEngine destructor.
 *  TODO: decompile full cleanup at 0x4493D0 */
void RESDATA_ScriptEngine_Dtor(void* self);
void RESDATA_ScriptEngine_Dtor(void* self) {
    (void)self;
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — RESDATA_ScriptEngine_Dtor needs decompilation");
}
int  Vehicle_SetState(void*, int);
int  Vehicle_SetState(void*, int) { return 0; }
/** UI_CenterWindow — Center inner rect within outer rect
 *  Address: 0x425A50
 *  
 *  Takes two RECT pointers (int[4] arrays: left, top, right, bottom).
 *  Modifies inner rect in place to be centered within outer rect.
 *  Preserves inner rect width and height; only adjusts left/top.
 *  __cdecl, 49 instructions. */
void UI_CenterWindow(int* outer, int* inner);
void UI_CenterWindow(int* outer, int* inner)
{
    int inner_w = inner[2] - inner[0];   /* inner width = right - left */
    int inner_h = inner[3] - inner[1];   /* inner height = bottom - top */
    int outer_w = outer[2] - outer[0];   /* outer width */
    int outer_h = outer[3] - outer[1];   /* outer height */

    /* Center horizontally: outer.left + (outer_w - inner_w) / 2 */
    inner[0] = outer[0] + (outer_w - inner_w) / 2;
    inner[2] = inner[0] + inner_w;

    /* Center vertically: outer.top + (outer_h - inner_h) / 2 */
    inner[1] = outer[1] + (outer_h - inner_h) / 2;
    inner[3] = inner[1] + inner_h;
}
void Sprite_Destroy(void*);
void Sprite_Destroy(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* ButtonSprite_Ctor(void*, int, int, int);
void* ButtonSprite_Ctor(void*, int, int, int) { return nullptr; }
void NETMAN_QueueMessage(void*, int, void*);
void NETMAN_QueueMessage(void*, int, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Sprite_SetState(void*, int);
void Sprite_SetState(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CopyRect(RECT* d, const RECT* s);
void CopyRect(RECT* d, const RECT* s) { if(d&&s) *d=*s; }
void OffsetRect(RECT* r, int dx, int dy);
void OffsetRect(RECT* r, int dx, int dy) { if(r){r->left+=dx;r->top+=dy;r->right+=dx;r->bottom+=dy;} }
int  IsRectEmpty(const RECT* r);
int  IsRectEmpty(const RECT* r) { return !r || r->left>=r->right || r->top>=r->bottom; }


/* ---- More globals ---- */
/* g_primary_surface/g_network_thread/g_network_queue/g_train_resources/
 * g_game_config/g_world/g_timer_event_id: duplicates of the canonical
 * definitions in shared/link_stubs.cpp. g_scripted_object: duplicate of
 * shared/defsym_stubs.cpp. g_ddraw_building/g_tooltip_mgr: duplicates of
 * the real typed globals in graphics/DDRAW.cpp. All removed (LINK-001). */
void* g_player_config = nullptr;
void* g_config_ini = nullptr;
void* g_trainstation_window = nullptr;
DirectPlaySession* g_dplay_peer = nullptr;
int   g_world_width = 0;
int   g_world_height = 0;
void* g_second_overlay = nullptr;
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
int wsprintfA(char* buf, const char* fmt, ...);
int wsprintfA(char* buf, const char* fmt, ...) {
    if (!buf || !fmt) return 0;
    va_list args; va_start(args, fmt);
    int ret = vsnprintf(buf, 1024, fmt, args);
    va_end(args); return ret;
}

int CRT_atoi(const char* s);
int CRT_atoi(const char* s) { return s ? atoi(s) : 0; }
    void CRT_sprintf(char* buf, const char* fmt, ...);
    void CRT_sprintf(char* buf, const char* fmt, ...) {}

void Cursor_Render(void*, int, int, char);
void Cursor_Render(void*, int, int, char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_TrackPiece_SetZoom(void*, int);
void CGWND_TrackPiece_SetZoom(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
/* WNDPROC_CriticalSectionLock(int*, char*) is no longer stubbed here —
 * real definition (adapter over WNDPROC_Stream::ExtractToken) lives in
 * resources/WndProcStream.cpp. See PROGRESS.md "WNDPROC_Stream facade
 * recovery" (2026-08-10). */

/* Stream I/O stubs — called from BuildingDescriptorEditor and wave_io; may be unreachable on host in normal paths */
void* WNDPROC_StreamPrintf(void*, void*);
void* WNDPROC_StreamPrintf(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void  WNDPROC_StreamReadLine(void*, void*);
void  WNDPROC_StreamReadLine(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* WNDPROC_StreamWrite(void*, void*);
void* WNDPROC_StreamWrite(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
int   WNDPROC_StreamSeekForward(void*, int, int, int);
int   WNDPROC_StreamSeekForward(void*, int, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void  Stream_BeginEnum(void*);
void  Stream_BeginEnum(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void  Stream_BeginRead(void*, int, int);
void  Stream_BeginRead(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }

/* Math/CRT stubs — signatures inferred from usage, likely misidentified by decompiler */
void* CRT_fabs(void*, void*);
void* CRT_fabs(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void  CRT_fmod(void*, void*);
void  CRT_fmod(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }

int  Config_GetIniInt(void*, const char*, const char*, int def);
int  Config_GetIniInt(void*, const char*, const char*, int def) { return def; }

/* Win32 stubs */
void* GetProcessHeap(void);
void* GetProcessHeap(void) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(1));
}
int   CloseHandle(void*);
int   CloseHandle(void*) { return 1; }
void  Sleep(unsigned int ms);
void  Sleep(unsigned int ms) { usleep(ms * 1000); }
void  SetPixel(void*, int, int, unsigned int);
void  SetPixel(void*, int, int, unsigned int) {}


/* ---- Bulk stubs for remaining symbols ---- */
/* g_ddraw's defining declaration moved to platform/ddraw_globals.cpp
 * (2026-08-14) — that file has no dependencies beyond shared/types.h, so
 * small standalone unit tests that link graphics/sdl3_ddraw.cpp (which
 * now assigns to g_ddraw, see PROGRESS.md's DirectDraw-shim Phase 5 note)
 * can link it too without pulling in the rest of this file's graph. */
void* g_backbuffer = nullptr;
void* g_game = nullptr;
void* g_audio_mgr = nullptr;
NetworkPlayerList* g_dplay = nullptr;
void* g_dplay_config = nullptr;
void* g_active_panel = nullptr;
void* g_font_small = nullptr;
/* Single-use font global read only by HelpWnd::render_scroll_down
 * (ui/HelpWnd_stubs.cpp, 0x4526B0 -> global at 0x4855EC). No write site
 * found anywhere in the binary; storage defined here alongside
 * g_font_small for the same reason (host-side backing for a recovered
 * global with no recovered initializer). */
void* g_font_scroll_down_hint = nullptr;
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
/* TODO(tilemap-drawing-pipeline): declared here as a scalar but
 * world/tilemap.h declares `extern uint8_t ATTR_0047f108[8]` (real 8-byte
 * bitmask-table type, confirmed via disassembly at 0x455342: `MOV AL, byte
 * ptr [EAX + 0x47f108]` with EAX pre-masked to 0-7). Real byte values
 * confirmed via direct read of the original binary at 0x47f108: `80 40 20
 * 10 08 04 02 01` -- MSB-first bit ordering (`1 << (7-n)`, not the naive
 * `1 << n`), consistent with bit 0 of a row addressing the most-
 * significant bit of its byte (same convention as bitmap_occupancy/
 * span_map cell parsing elsewhere in this codebase). Fixing the type
 * wakes TileMap::InvalidateDirtyRects/ProcessRect's DirectDraw
 * presentation path (dirty-tile bits go from always-0 to real).
 *
 * Third revert (2026-08-13): the previous blocker (null `g_tooltip_mgr`
 * inside ProcessRect's UI_SetTooltipPos call) is fixed for real --
 * `g_tooltip_mgr` is now a real `UI_Manager` singleton (PROGRESS.md's
 * "Wire the real UI_Manager singleton" milestone) -- but flipping this
 * type past that point reaches TWO more, larger, genuinely separate
 * gaps in the same previously-100%-dead rendering path:
 *   1. Entity::Draw/DrawConnected (core/GameObject.cpp) call
 *      UIPANEL_Blit(resource+0x10, ...) -- confirmed via UIPANEL_Blit's
 *      own body (ui/UIPANEL_Surface.cpp, 0x42B050) that this is a real
 *      `UIPANEL_Surface*` sub-object pointer, not a bitmap. No host
 *      resource carries one (a host SpriteResource* is a small,
 *      unrelated struct), and FrameData::flip_horizontal has no
 *      .dat-derived host mapping either (only ::is_connected is mapped
 *      so far). Guarded both methods against this (warn-once, skip the
 *      blit) plus a null-`resource` case (most host entities never get a
 *      real resource -- see PersistenceAdapter.h's documented 0/497
 *      placement-coverage gap) -- both guards kept, they're correct and
 *      needed regardless of when this is next attempted.
 *   2. RESOLVED (2026-08-13, GameView-misattribution session):
 *      render_selection (0x42D400) was never a Town:: method at all --
 *      TileMap_ProcessRect's own call site loads ECX with the bare
 *      immediate 0x4852A0 (GameView's global instance), never a Town
 *      pointer-variable dereference. GameView really is Entity-derived
 *      (GameObject -> Entity -> Panel -> GameView, see core/GameView.h),
 *      so the original's raw `CALL 0x405E60` is just
 *      `game_view->Draw(rect, extra, 0)` -- Entity::Draw, inherited,
 *      no cast needed. Moved to GameView::render_selection
 *      (core/GameView.cpp), which now calls `this->Draw(...)` for real
 *      instead of the stale GameObject_Draw(void*) stub. The `Town :
 *      public UI_WindowBase` layout-relationship blocker this bullet
 *      used to describe never existed; it was a wrong-class assumption.
 * Bullet 1's UIPANEL_Surface/FrameData gap is unrelated and still open;
 * `ATTR_0047f108` stays reverted until that one is resolved too. */
int ATTR_0047f108 = 0;
int DAT_00481170 = 0;
int DAT_0048118c = 0;
int DAT_00481190 = 0;
int DAT_00481194 = 0;
int s_AW_Blit_failure_reported_0047e0d8 = 0;

/* Win32 stubs */
void GetWindowTextA(void*, char*, int);
void GetWindowTextA(void*, char*, int) {}
int GetLastError(void);
int GetLastError(void) { return 0; }
int FormatMessageA(int, void*, int, int, char*, int, void*);
int FormatMessageA(int, void*, int, int, char*, int, void*) { return 0; }
void* LocalFree(void*);
void* LocalFree(void*) { return nullptr; }
void* FindFirstFileA(const char*, void*);
void* FindFirstFileA(const char*, void*) { return nullptr; }
int FindNextFileA(void*, void*);
int FindNextFileA(void*, void*) { return 0; }
int FindClose(void*);
int FindClose(void*) { return 0; }
int CreateDirectoryA(const char*, void*);
int CreateDirectoryA(const char*, void*) { return 0; }
int DeleteFileA(const char*);
int DeleteFileA(const char*) { return 0; }
int GetFileAttributesA(const char*);
int GetFileAttributesA(const char*) { return -1; }
int ClientToScreen(void*, void*);
int ClientToScreen(void*, void*) { return 0; }
int SetCursorPos(int, int);
int SetCursorPos(int, int) { return 0; }

/* Critical section stubs */
void InitializeCriticalSection(void*) {}
void EnterCriticalSection(void*);
void EnterCriticalSection(void*) {}
void LeaveCriticalSection(void*);
void LeaveCriticalSection(void*) {}
void DeleteCriticalSection(void*);
void DeleteCriticalSection(void*) {}

/* CRT stubs */
void CRT_strncpy(void*, void*, int);
void CRT_strncpy(void*, void*, int) {}
void CRT_0x4681D0(int);
void CRT_0x4681D0(int) {}
void CRT_0x468480(char*, void*);
void CRT_0x468480(char*, void*) {}
void CRT_0x468610(void*, unsigned int, unsigned int, int);
void CRT_0x468610(void*, unsigned int, unsigned int, int) {}
void* CRT_malloc_zero(unsigned int sz);
void* CRT_malloc_zero(unsigned int sz) { return operator_new(static_cast<size_t>(sz)); }
void* operator_new(unsigned int sz);
void* operator_new(unsigned int sz) { return operator_new(static_cast<size_t>(sz)); }

/* vtable globals (needed for some UI classes) */
void* vtable_for_UIEntity = nullptr;
void* vtable_for_Collection = nullptr;
int growth_factor = 2;

/* DDRAW stubs */
void Cursor_SetCapture(void*, unsigned char);
void Cursor_SetCapture(void*, unsigned char) {}
void DDRAW_UnlockPrimary();
void DDRAW_UnlockPrimary() {}
void Cursor_InitSprites(void*);
void Cursor_InitSprites(void*) {}
void Cursor_UnlockAllSurfaces(void*);
void Cursor_UnlockAllSurfaces(void*) {}
/* DDRAW_GetSurfaceWidthHeight/DDRAW_RestoreSurfaces: real, Ghidra-verified
 * implementations now canonical in native/DDRAW_GetSurfaceWidthHeight.c
 * (0x4014E0, see PROGRESS.md raw-073) and native/ddraw_surface_ops.c;
 * these no-op duplicates removed (LINK-001). */
void DDRAW_SetSurfaceFormat(void*, int);
void DDRAW_SetSurfaceFormat(void*, int) {}
/* DDRAW_SpriteDataCtor/Dtor(void*, int) no-op stubs removed — real
 * implementation now AssetMgr::AssetMgr/~AssetMgr (resources/AssetMgr.h/
 * .cpp; previously misattributed as SpriteData::SpriteData/~SpriteData in
 * graphics/DDRAW.h, removed 2026-08-14 — see AssetMgr.h for the evidence).
 * world/tilemap.cpp's mismatched free-function declarations (which bound
 * here instead of the real constructor/destructor) were fixed to use
 * placement-new/explicit-dtor-call against the real type. */
/* DDRAW_SelectBuilding(void*, int) — real implementation now in
 * graphics/DDRAW.cpp (0x459180), as a bridge to
 * DDRAW_Building::SelectBuilding. The wrong-signature `void`-returning
 * no-op that used to live here caused world/tilemap.cpp's
 * `DDRAW_SelectBuilding(...) == 0` comparison to read whatever garbage
 * happened to be in eax on every deselect click. */

/* RESOURCE stubs (RESMGR_IsSaveHeader/LoadResource/ReleaseResource/
 * ResourceData_Init are real code in resources/ResDataSave.cpp) */
void RESDATA_SetPosition(void*, int, int);
void RESDATA_SetPosition(void*, int, int) {}
void RESDATA_BaseInit(void*);
void RESDATA_BaseInit(void*) {}
void RESDATA_DtorBase(void*);
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
#include "../resources/resource_manager_sdl3.h"
#endif

namespace {

/* Shared tile-state byte fetch for the RESDATA_Is*Tile family below.
 * Host deviation: `tile_obj`/`ptr` may be a loco::assets::SpriteResource*
 * (undersized-object landmine, see PROGRESS.md) rather than a real
 * RESDATA/TileMapResource -- source the byte from the already-verified
 * SpriteMetadata::tile_type instead of reading past the real allocation.
 * Returns false (no category can match) for a host resource with no
 * tile_type directive, matching a real non-track resource's behavior. */
inline bool ResolveTileStateByte(const void* resource, uint8_t* out_byte)
{
#ifndef _WIN32
    if (loco::assets::is_host_sprite_resource(resource)) {
        return loco::assets::sprite_tile_type_byte(resource, out_byte);
    }
#endif
    *out_byte = *reinterpret_cast<const uint8_t*>(
        static_cast<const char*>(resource) + 0x63A);
    return true;
}

}  // namespace

/* RESDATA_IsBuildingTile — identical Ghidra-verified implementation
 * (0x44BD30) now canonical in world/tilemap.cpp; this was a duplicate
 * (LINK-001). */

uint8_t RESDATA_IsRoadTile(int32_t tile_obj);
uint8_t RESDATA_IsRoadTile(int32_t tile_obj)
{
    /* 0x44BD10: check byte at +0x63A for {0x01,0x02,0x03,0x04} */
    if (tile_obj == 0) return 0;
    uint8_t b = 0;
    if (!ResolveTileStateByte(
            reinterpret_cast<const void*>(static_cast<intptr_t>(tile_obj)), &b)) {
        return 0;
    }
    return (b == 0x01 || b == 0x02 || b == 0x03 || b == 0x04) ? 1 : 0;
}

uint8_t RESDATA_IsWaterTile(int32_t tile_obj);
uint8_t RESDATA_IsWaterTile(int32_t tile_obj)
{
    /* 0x44BD50: check byte at +0x63A for {0x0E,0x0F} */
    if (tile_obj == 0) return 0;
    uint8_t b = 0;
    if (!ResolveTileStateByte(
            reinterpret_cast<const void*>(static_cast<intptr_t>(tile_obj)), &b)) {
        return 0;
    }
    return (b == 0x0E || b == 0x0F) ? 1 : 0;
}

uint8_t RESDATA_IsTrackTile(int32_t tile_obj);
uint8_t RESDATA_IsTrackTile(int32_t tile_obj)
{
    /* 0x44BD70: check byte at +0x63A for {0x10,0x11} */
    if (tile_obj == 0) return 0;
    uint8_t b = 0;
    if (!ResolveTileStateByte(
            reinterpret_cast<const void*>(static_cast<intptr_t>(tile_obj)), &b)) {
        return 0;
    }
    return (b == 0x10 || b == 0x11) ? 1 : 0;
}

uint8_t RESDATA_IsSceneryTile(int32_t tile_obj);
uint8_t RESDATA_IsSceneryTile(int32_t tile_obj)
{
    /* 0x44BD90: check byte at +0x63A for {0x12,0x13} */
    if (tile_obj == 0) return 0;
    uint8_t b = 0;
    if (!ResolveTileStateByte(
            reinterpret_cast<const void*>(static_cast<intptr_t>(tile_obj)), &b)) {
        return 0;
    }
    return (b == 0x12 || b == 0x13) ? 1 : 0;
}

uint32_t RESDATA_GetTileCategory(void* ptr, int16_t a, uint16_t b);
uint32_t RESDATA_GetTileCategory(void* ptr, int16_t a, uint16_t b)
{
    /* 0x44BDB0: dispatches on type byte at +0x63A.
     * Returns 0x100 | something on match, 0x??00 on no-match.
     * Host stub: return 0 (no category match) — the original logic
     * requires full RESDATA resource objects with player/color fields. */
    if (ptr == nullptr) return 0;
    /* This stub asserts unconditionally below regardless of typeByte's
     * value (the player/color-dependent path this function needs was
     * never implemented) -- ResolveTileStateByte here exists only to
     * avoid the undersized-host-object OOB read on the way to that
     * assert, not to change whether it fires. A host resource with no
     * tile_type falls through to the same unconditional assert as any
     * other value, matching the original's "always crashes" contract. */
    uint8_t typeByte = 0;
    ResolveTileStateByte(ptr, &typeByte);
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
/* Real signature returns void* (0x4546D0; see graphics/DDRAW.cpp and
 * shared/defsym_stubs.cpp for the sibling overload). Corrected from
 * `void`, which was an ODR mismatch against world/scriptengine.cpp's
 * (void*, int32_t, int32_t, int32_t) declaration of this same overload.
 * Already loud (unlike the sibling overload was) and already
 * unreachable today (RESDATA_ScriptedObject::Start has zero callers). */
void* RESDATA_CreateChildSprite(void*, int, int, int);
void* RESDATA_CreateChildSprite(void*, int, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); return nullptr; }
void RESDATA_HitTestChildren(void*, int, int);
void RESDATA_HitTestChildren(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Panel_DtorBody(void*);
void Panel_DtorBody(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }

/* Game stubs */
void Game_SetScreenMode(void*, char, char, char);
void Game_SetScreenMode(void*, char, char, char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }

/**
 * NET_FindPlayer / NET_UploadAsset / PlaySoundFile — Cursor::upload_custom_content()
 * (0x419B10) network-upload helpers. Genuinely missing (no real
 * implementation exists anywhere in the tree under any signature) but
 * confirmed unreachable on the host build: the whole custom-content upload
 * flow is gated by GetOpenFileNameA (graphics/sdl3_window.cpp), which
 * always returns FALSE in headless/SDL3 mode ("no file dialog"), and
 * Cursor::show() unconditionally zeroes obj_184->upload_id whenever a
 * player record is attached — so upload_id can never be non-zero when
 * NET_FindPlayer's gate checks it either. Loud stubs per CLAUDE.md's
 * stub policy (never a silent no-op for missing internal logic).
 */
int NET_FindPlayer(int, int);
int NET_FindPlayer(int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached (upload_custom_content should be unreachable on host)"); return 0; }
uint16_t NET_UploadAsset(int, char*);
uint16_t NET_UploadAsset(int, char*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached (upload_custom_content should be unreachable on host)"); return 0; }
void PlaySoundFile(char*, int, int, int);
void PlaySoundFile(char*, int, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached (upload_custom_content should be unreachable on host)"); }
void Game_CheckScreensaverTimeout(int*);
void Game_CheckScreensaverTimeout(int*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
/** Game_DeselectGameObject — Host no-op (no selection state to clear).
 *  Binary sets selected building to null. */
void Game_DeselectGameObject(int);
void Game_DeselectGameObject(int) { /* host no-op */ }
void Game_SelectGameObject(void*, void*);
void Game_SelectGameObject(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_StopSound(void*, int);
void GameObject_StopSound(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_Update(void*);
void GameObject_Update(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_Draw(void*);
void GameObject_Draw(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_PtInRect(void*, int, int);
void GameObject_PtInRect(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_DtorBody(void*);
void GameObject_DtorBody(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_BaseCtor(void*, int, int, int, int);
void GameObject_BaseCtor(void*, int, int, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Entity_GetSubObjectPosition(void*, int*, int);
void Entity_GetSubObjectPosition(void*, int*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }

/* UI stubs */
void UI_WindowBase_Ctor(void*, void*, unsigned int);
void UI_WindowBase_Ctor(void*, void*, unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_WindowBase_BaseDtor(void*);
void UI_WindowBase_BaseDtor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_WindowBase_Hide(void*);
void UI_WindowBase_Hide(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_CreateFullWindow(void*, int, void*, int, int, int, int, void*, void*, unsigned int);
void UI_CreateFullWindow(void*, int, void*, int, int, int, int, void*, void*, unsigned int) { /* host no-op */ }
void UI_CreateChildWindow(void*, int, int);
void UI_CreateChildWindow(void*, int, int) { /* host no-op */ }
/* UI_CleanupTooltips(void*) / UI_DestroyTooltip(void*, int) / UI_CreateTooltip
 * (void*, int, int, int, int): removed. UI_CleanupTooltips and
 * UI_DestroyTooltip now have real definitions in ui/UI_Utils.cpp routing
 * to UI_Manager::cleanupTooltips/destroyTooltip. The (void*, int, int,
 * int, int) UI_CreateTooltip overload was a redundant duplicate of the
 * canonical (void*, int, short, int, int) overload (also now in
 * ui/UI_Utils.cpp) — its only two callers (game/ScriptedObject.cpp,
 * world/scriptengine.cpp) were retyped to the canonical short-param
 * signature so there is exactly one UI_CreateTooltip symbol. */
void UI_IsBitmapReady(int);
void UI_IsBitmapReady(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
/* UI_Window_UpdateScroll (0x423560): implemented for real as
 * UIEntity::UpdateScroll() (ui/UIEntity.h/.cpp, 2026-08-14) — the two
 * previously-missing dependencies this stub's own comment named
 * (UI_ShowWindow/UI_HideWindow, since renamed StopSound/SetVisible) were
 * already resolved by an earlier pass. UI_Utils.cpp's hideTooltip() now
 * calls the typed method directly; this free-function facade has zero
 * remaining callers. */
void UIEntity_Ctor(void);
void UIEntity_Ctor(void) { /* host no-op */ }
/* UIPANEL_Blit(void*,int...int) and UIPANEL_BeginPaint(void*): duplicates
 * of shared/link_stubs.cpp / ui/UIPANEL.cpp (LINK-001); removed here. */
/* UIPANEL_EndPaintEx(void*, void*, int, unsigned char, RECT*) — this wrong
 * (2nd-param void*) overload's host no-op was the silent-wrong-stub every
 * mis-declared caller in the tree bound to (docs/landmine-sweep-worklist.md).
 * Removed 2026-08-13 after fixing all known callers; confirmed via `nm`
 * that no remaining .o (native or mingw-typecheck) has an undefined
 * reference to its mangled name (_Z18UIPANEL_EndPaintExPvS_ihP4RECT). */
/* Returns `self` (2026-08-08, network/NetworkPlayerList.cpp STRICT=2
 * cluster): the real definition (0x42A110, graphics/LOCOBITMAP.cpp
 * UIPANEL_CreateSurface(UIPANEL_Surface*)) is a placement-style constructor
 * that initializes *self in place; this narrower void*-param overload is
 * the one every other caller in the tree (EditWindow.cpp, Town.cpp,
 * UI_ChildWindow.cpp, BuildingPanel.cpp, Cursor.cpp, Netman.cpp,
 * NetworkPlayerList.cpp) actually links against, and every one of them
 * chains the result back into the same surface variable (`surface =
 * UIPANEL_CreateSurface(surface)`), expecting identity to be preserved —
 * previously `void`-returning, which is UB read by every caller (Itanium
 * mangling ignores return type, so it linked clean). Still a host no-op
 * (init logic itself not ported), but now well-defined. */
void* UIPANEL_CreateSurface(void* self);
void* UIPANEL_CreateSurface(void* self) { return self; }
void UIPANEL_StretchBlit(void*, void*, int, int, int);
void UIPANEL_StretchBlit(void*, void*, int, int, int) { /* host no-op */ }
void UIPANEL_SetClipRect(void* self, int a, int b);
void UIPANEL_SetClipRect(void* self, int a, int b) { (void)self; (void)a; (void)b; }
void UIPANEL_ScrollPanel_HandleDrag(void*, int, int);
void UIPANEL_ScrollPanel_HandleDrag(void*, int, int) { /* host no-op */ }
void UIPANEL_ScrollPanel_Dtor(void* self);
void UIPANEL_ScrollPanel_Dtor(void* self) { (void)self; }
void UIPANEL_InitScrollPanel(void* self);
void UIPANEL_InitScrollPanel(void* self) { (void)self; }
void UIPANEL_FillRect(void* self, int a, int b);
void UIPANEL_FillRect(void* self, int a, int b) { (void)self; (void)a; (void)b; }

/* Various other stubs */
void Sprite_Init(void* self);
void Sprite_Init(void* self) { (void)self; }
void Sprite_SetState(void*, int, int*);
void Sprite_SetState(void*, int, int*) { /* host no-op */ }
void Sprite_Destroy(void);
void Sprite_Destroy(void) { }
/* ButtonSprite_Ctor(void*,int) / TileMap_InvalidateRect(void*,int,int,int,int) /
 * CGWND_SetMode(void*): duplicates of shared/link_stubs.cpp (LINK-001);
 * removed here. */
void HelpWnd_PlayNarration(void*, int, int);
void HelpWnd_PlayNarration(void*, int, int) { /* host no-op */ }
void Town_BlitElement(void*, unsigned int, unsigned int, int, unsigned int, void*, unsigned int, unsigned int, int, unsigned int, unsigned int);
void Town_BlitElement(void*, unsigned int, unsigned int, int, unsigned int, void*, unsigned int, unsigned int, int, unsigned int, unsigned int) { /* host no-op */ }
/* Town_SelectBuilding(void*, int) — real implementation now in
 * town/Town.cpp (calls GameView::select_building). Removed the no-op
 * stub here per CLAUDE.md's "no --defsym-style placeholders" rule; a
 * duplicate definition here would also be a link error against the
 * real one. */
void TileMap_InvalidateDirtyRects(void*, char);
void TileMap_InvalidateDirtyRects(void*, char) { /* host no-op */ }
void ScriptEngine_Call(void* self);
void ScriptEngine_Call(void* self) { (void)self; }
void ScriptEngine_Init(void* self);
void ScriptEngine_Init(void* self) { (void)self; }
void CGWND_SetPause(void* self, char c);
void CGWND_SetPause(void* self, char c) { (void)self; (void)c; }
void CGWND_SetBuildMode(int i);
void CGWND_SetBuildMode(int i) { (void)i; }
void World_Init(void* self);
void World_Init(void* self) { (void)self; /* host no-op */ }
void PixelDataCache_LookupAsset(void* self, int a, int b);
void PixelDataCache_LookupAsset(void* self, int a, int b) { (void)self; (void)a; (void)b; }
void PixelDataCache_GetEntryCount(void* self);
void PixelDataCache_GetEntryCount(void* self) { (void)self; }
void PixelDataCache_Unlock(void* self, int i);
void PixelDataCache_Unlock(void* self, int i) { (void)self; (void)i; }
void DPLAY_RenderPlayer(void*, void*, int, void*, int, int, unsigned int, RECT*);
void DPLAY_RenderPlayer(void*, void*, int, void*, int, int, unsigned int, RECT*) { /* host no-op */ }
void PlaySoundAt(int, int, int, int);
void PlaySoundAt(int, int, int, int) { /* host no-op */ }
void Collection_Resize(int);
void Collection_Resize(int) { /* host no-op */ }
void Collection_GetAt(int);
void Collection_GetAt(int) { /* host no-op */ }
void SortedCollection_Compare(void*, void*);
void SortedCollection_Compare(void*, void*) { /* host no-op */ }
void SortedCollection_SortRange(int, int);
void SortedCollection_SortRange(int, int) { /* host no-op */ }

/* ================================================================== */
/* GameLoop.cpp dependency stubs (added 2026-07-25)                    */
/* ================================================================== */

/* Subsystem constructors */
void* GameConfig_constructor(void* memory);
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
void* NETMAN_constructor(void*);
void* NETMAN_constructor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); return nullptr; }
void* PlayerRecord_constructor(void*);
void* PlayerRecord_constructor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); return nullptr; }
/* Subsystem init */
/* DDRAW_Init — real implementation is native/ddraw_init.c (0x45C8A0);
 * this and link_stubs.cpp's no-op copy were the flagship LINK-001
 * nondeterminism bug (see PROGRESS.md). Removed. */
void  UIPANEL_Hide(void*, void*);
void  UIPANEL_Hide(void*, void*) { /* host no-op */ }

/* Per-frame updates */
void  NETMAN_Update(void* self);
void  NETMAN_Update(void* self) { (void)self; }
void  RESMGR_VehicleAnimationTick(void* self);
void  RESMGR_VehicleAnimationTick(void* self) { (void)self; }
void  World_UpdateTick(void* self);
void  World_UpdateTick(void* self) { (void)self; }
/* UI_HideTooltip(void*): removed — real definition now in
 * ui/UI_Utils.cpp, routing to UI_Manager::hideTooltip. */
void  RESDATA_ScriptedObject_Update(void* self);
void  RESDATA_ScriptedObject_Update(void* self) { (void)self; }
/* Town_TrackBuilding (0x42D1A0) and DDRAW_UpdateBuilding (0x459DA0) are
 * implemented in src/sdl3_shims/sdl3_town_mode3.cpp for the host build.
 * The Win32 build links the original binary implementations. */
extern void Town_TrackBuilding(void*);
extern void DDRAW_UpdateBuilding(void*);
/* INPUT_GetSaveFileName / INPUT_SaveCurrentWorld / INPUT_FindObjectAt /
 * INPUT_PlaceObject / INPUT_RemoveObject: canonical definitions moved to
 * input/InputMgr.cpp (0x41DD40 real, the rest loud deferred stubs). */
void  BuildingMgr_UpdateAll(void* self);
void  BuildingMgr_UpdateAll(void* self) { (void)self; }

/* Timer callback */
extern "C" void LAB_0045c520(void);
extern "C" void LAB_0045c520(void) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached — LAB_0045c520"); }

/* Windows API extras */
extern "C" {
void* CreateEventA(void*, int, int, const char*);
void* CreateEventA(void*, int, int, const char*) { return (void*)1; }
/* timeBeginPeriod: duplicate of shared/link_stubs.cpp (LINK-001); removed. */
int   timeSetEvent(unsigned int, unsigned int, void*, unsigned int, unsigned int);
int   timeSetEvent(unsigned int, unsigned int, void*, unsigned int, unsigned int) { return 1; }
}
