// Status: INTEGRATED
/**
 * tilemap.cpp — TileMap method implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * TileMap manages the game world's tile grid (82x66 tiles, each 64 bytes for
 * 16 layer slots). It tracks scroll state, dirty regions for rendering, tile
 * occupancy/buildability checks, and viewport scrolling.
 *
 * Every method was validated instruction-by-instruction against the Ghidra
 * disassembly (database "locon"); the original address is annotated on each
 * implementation.
 */

#include "tilemap.h"

#include "../core/Entity.h"
#include "../input/InputMgr.h"

#include <cmath>
#include <new>

/* ================================================================== */
/* External references — declared in tilemap.h; re-declared here for   */
/* self-contained compilation.                                         */
/* ================================================================== */

/* Memory */
void*  operator_new(size_t size);
void   GLOBAL_free(void* ptr);

/* Game globals — addresses from binary */
extern int32_t  g_game_mode;             /* 0x004851F4 */
extern int32_t  g_screen_width;          /* 0x004851D8 */
extern int32_t  g_screen_height;         /* 0x00485214 */
extern int32_t  g_client_offset_x;       /* 0x00485228 */
extern int32_t  g_client_offset_y;       /* 0x0048522C */
extern int32_t  g_client_width;          /* 0x00485220 */
extern int32_t  g_client_height;         /* 0x00485224 */
extern uint8_t  g_is_fullscreen;         /* 0x00485210 */
extern int32_t  g_world_width;           /* 0x004AAD0C */
extern uint8_t  g_is_town_mode;
extern int32_t  g_town_overlay_rect;     /* 0x48538C */
extern int32_t  g_town_overlay_left;     /* 0x485390 */
extern int32_t  g_town_overlay_top;      /* 0x485394 */
extern int32_t  g_town_overlay_right;    /* 0x485398 */
extern uint8_t  g_build_mode;            /* 0x485234 */
extern uint8_t  g_disable_input;         /* 0x4855AC */
extern uint8_t  g_lock_update_flag;      /* 0x4851F0 */
extern uint8_t  g_click_on_building;     /* 0x48556C */
extern uint8_t  g_placement_valid;       /* 0x4AA648 */
extern uint8_t  g_placement_blocked;     /* 0x48558C */
extern int32_t  g_placement_resource_id; /* 0x485550 */
extern void*    g_town_view;             /* 0x4852A0 */
extern void*    g_ddraw_building;        /* 0x4A9EF0 */
extern void*    g_about;                 /* 0x4FD390 */
extern void*    g_netman;                /* 0x4FD3AC */
extern uint8_t  g_click_on_town;         /* 0x48557C */
extern int32_t  g_selected_building;     /* 0x4855B0 */
extern int32_t  g_town_selection_rect_left;   /* 0x4854D0 */
extern int32_t  g_town_selection_rect_top;    /* 0x4854D4 */
extern int32_t  g_town_selection_rect_right;  /* 0x4854D8 */
extern int32_t  g_town_selection_rect_bottom; /* 0x4854DC */
extern uint8_t  g_has_selection;         /* 0x4854EC */
extern int32_t  g_viewport_x;            /* 0x4AAD24 */
extern int32_t  g_viewport_rect_left;    /* 0x4AAD14 (TileMap.viewport_rect.left) */
extern int32_t  g_viewport_rect_top;     /* 0x4AAD18 */
extern int32_t  g_viewport_rect_right;   /* 0x4AAD1C */
extern int32_t  g_viewport_rect_bottom;  /* 0x4AAD20 */
extern uint8_t  g_allow_building_placement; /* 0x485328 */
extern int32_t  g_player_id;             /* 0x4AAD46 (TileMap.tile_count_x) */
extern int32_t  g_player_color;          /* 0x4AAD48 (TileMap.tile_count_y) —
                                    *   host-declared 32-bit; the binary stores
                                    *   the 16-bit player words adjacently and
                                    *   loads 16 bits everywhere */
extern void*    g_cursor_surface;        /* 0x4FD3C8 */
extern void*    g_primary_surface;       /* 0x4FD3C4 */
extern void*    g_tile_occupied_bitmap;  /* 0x4FD18C */
extern void*    g_resmgr;                /* 0x4855E8 */
extern void*    g_scripted_object;       /* 0x4AA5B8 */
extern void*    g_building_mgr;          /* 0x485448 */
/* g_tilemap declared canonically in tilemap.h (TileMap* singleton) */
extern void*    g_game;                  /* 0x4854C8 */
extern void*    g_world;                 /* 0x4A98B0 */
extern void*    g_tooltip_mgr;           /* 0x4FD220 */
extern HWND     g_main_window;

/* Bitmask lookup table */
extern uint8_t  ATTR_0047f108[8];        /* bitmask lookup (1<<n) */

/* External functions */
extern int      RESDATA_IsRoadTile(int ptr);
extern int      RESDATA_GetTileCategory(void* ptr, short a, unsigned short b);
extern int      RESDATA_IsSceneryTile(int ptr);
extern int      RESDATA_IsWaterTile(int ptr);
extern int      RESDATA_IsTrackTile(int ptr);
extern void*    INPUT_PlaceObject(InputMgr* mgr, unsigned int resource_id); /* 0x41DD80 */
extern uintptr_t INPUT_RemoveObject(InputMgr* mgr, void* obj, unsigned int param); /* 0x41DEF0 */
extern int      GetResourceType(unsigned int resource_id);
extern void     PlaySoundAt(int sound_id, int x, int y, int channel);
extern int      Town_SelectBuilding(void* town_view, int building);
extern int      DDRAW_SelectBuilding(void* ddraw_building, int building);
extern void     CGWND_SetMode(int mode);
extern void     Town_RenderSelection(void* town_view);
extern void     Town_DeselectBuilding(void* town_view);
extern void     Town_UpdateSelection(void* town_view);
extern void     Game_SetCursorByResourceId(void* game, int x, int y,
                                            int w, int h, int flag);
extern void     Game_ResetCursor(void* game);
extern void     UI_SetTooltipText(void* mgr, int x, int y, int w, int h);
extern void     UI_SetTooltipPos(void* mgr, int x, int y, int w, int h, int flag);
extern void     UI_UpdateTooltip(void* mgr, int x, int y, int w, int h, int flag);
extern void     BuildingMgr_DispatchAll(void* mgr, int dispatch_flags,
                                         int x, int y, int w, int h, int flag);
extern void     World_InvalidateRect(void* world, int x, int y,
                                      int w, int h, short type);
extern void     RESDATA_ScriptedObject_Dispatch(void* obj, int x, int y,
                                                 int w, int h, int flag);
extern void     DDRAW_DispatchToSubObjects(void* ddraw, int x, int y,
                                            int w, int h, void* flag);
extern void     Game_DeselectGameObject(int game);
extern void     World_Init(void* world);
extern void     UI_CleanupTooltips(void* mgr);
extern void*    DDRAW_SpriteDataCtor(void* obj, int type);
extern void     DDRAW_SpriteDataDtor(void* obj);
extern int      Math_DistSquared(int x1, int y1, int x2, int y2);
extern void*    Entity_GetSubObjectPosition(void* obj, int* out_xy, int direction);
extern void     GameObject_GetSubObjectWorldPos(void* obj, int* out_packed);
extern void     World_Lock(void* world);
extern void     World_Unlock(void* world);
extern void     UIPANEL_Blit(void* src, int sx, int sy, int sw, int sh,
                              void* dst, int dx, int dy, int dw, int dh, int flags);
extern void     DDRAW_PresentRect(RECT* rect, HWND hwnd,
                                   int32_t* viewport_x, char flag);
extern int      WIN32_GetThreadResult(int param);
extern void     AssetMgr_LoadFileEx(uint* ptr);
extern void     AssetMgr_EnumFiles(uint* ptr);
extern int      IntersectRect(RECT* dst, RECT* a, RECT* b);
extern int      UnionRect(RECT* dst, RECT* a, RECT* b);
extern int      SetRectEmpty(RECT* rect);
extern void     SetRect(RECT* rect, int left, int top, int right, int bottom);
extern void     CopyRect(RECT* dst, RECT* src);
extern int      PtInRect(RECT* rect, int x, int y);
extern void     InflateRect(RECT* rect, int dx, int dy);
extern void     OutputDebugStringA(const char* str);
extern void     Sleep(uint32_t ms);
extern BOOL     InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
extern BOOL     UpdateWindow(HWND hWnd);
extern void*    ResourceManager_GetById(void** resmgr, UINT id);
extern void     UIPANEL_InitSurface(void* surface, int w, int h,
                                     int a, int b, byte c);

/* ================================================================== */
/* TileMapResource::IsEditorSprite — 0x41F430                          */
/* ================================================================== */
bool TileMapResource::IsEditorSprite() const
{
    /* type 0x03 (building/track): state byte +0x63A.  0x44BD50 accepts
     * {0x0E,0x0F}, 0x44BD70 accepts {0x10,0x11}. */
    if (this->object_type == 0x03) {
        const uint8_t st = this->state_63A;
        return st == 0x0E || st == 0x0F || st == 0x10 || st == 0x11;
    }
    /* type 0x0C (scenery): resource id +0x04 is 0x3001 or 0x3002. */
    if (this->object_type == 0x0C) {
        return this->resource_id == 0x3001 || this->resource_id == 0x3002;
    }
    return false;
}

/* ================================================================== */
/* Tile Index Helper Macro                                             */
/* ================================================================== */
/* Tile data offset: 0x48 + (x * 65 + y) * 64 + layer * 4 */
#define TILE_OFFSET(x, y, layer) \
    (0x48 + (static_cast<int>(x) * 65 + static_cast<int>(y)) * 0x40 + \
     static_cast<int>(layer) * 4)

/* ================================================================== */
/* Dirty-rect list node: a RECT (16 bytes) + "next" pointer at +0x10.  */
/* Allocations are 0x14 bytes (TileMap_InvalidateDirtyRects 0x456475). */
/* ================================================================== */
static RECT* TileMap_AllocRectNode()
{
    RECT* node = reinterpret_cast<RECT*>(operator_new(0x14));
    if (node != NULL) {
        node[1].left = 0;
    }
    return node;
}

/* ================================================================== */
/* Primary-surface Lock/Unlock dispatch (DirectDraw surface vtable).   */
/*                                                                     */
/* g_primary_surface (0x4FD3C4) is the DirectDraw primary; slots 25    */
/* (Lock) and 32 (Unlock) of the standard IDirectDrawSurface4 ABI are  */
/* called with the ddsurfacedesc buffer at TileMap +0x52494.           */
/* TODO(integration): type g_primary_surface via sdl3_shims/sdl3_ddraw */
/* and call Lock/Unlock directly.                                      */
/* ================================================================== */
typedef int (__thiscall* DDrawSurfaceLockFn)(void* self, void* rect,
                                              void* desc, unsigned int flags,
                                              void* handle);
typedef int (__thiscall* DDrawSurfaceUnlockFn)(void* self, void* rect);

static int TileMap_LockPrimarySurface(void* desc)
{
    void** vtable = *reinterpret_cast<void***>(g_primary_surface);
    return (*(reinterpret_cast<DDrawSurfaceLockFn>(vtable[25])))(
        g_primary_surface, NULL, desc, 0, 0);
}

static int TileMap_UnlockPrimarySurface()
{
    void** vtable = *reinterpret_cast<void***>(g_primary_surface);
    return (*(reinterpret_cast<DDrawSurfaceUnlockFn>(vtable[32])))(
        g_primary_surface, NULL);
}

/* ================================================================== */
/* Mode-3 tile-object click handlers (vtable[16]/[17]).                */
/*                                                                     */
/* The mode-3 grid objects are ResourceGameObject-family instances     */
/* whose vtable slots [16]/[17] hold per-class click handlers:         */
/*   ResourceGameObject  [16] 0x458800 RestartAnimation                */
/*                       [17] 0x458810 IsMemberActionActive            */
/*   RESDATA_GameVehicle [16] 0x44B0B0 (click horn/footstep sound)     */
/*                       [17] 0x458810                                 */
/*   GameVehicle         [16] 0x412940 (start move)                    */
/*                       [17] 0x4129B0                                 */
/*   HelpPageNode        [16] 0x44B0B0                                 */
/*                       [17] 0x458810                                 */
/* The game subclasses do not declare these overrides yet, so the calls  */
/* are dispatched here through the binary vtable.                      */
/* TODO(integration): declare the overrides on ResourceGameObject      */
/* (core/BuildingMgrObjectGroup.h), RESDATA_GameVehicle                */
/* (game/ResdataGameVehicle.h) and GameVehicle (game/GameVehicle.h),   */
/* then replace these adapters with typed virtual calls.               */
/* ================================================================== */
typedef int (__thiscall* TileMapObjSlot16Fn)(void* self);
typedef int (__thiscall* TileMapObjSlot17Fn)(void* self);

static int TileMap_CallSlot16(void* obj)
{
    void** vtable = *reinterpret_cast<void***>(obj);
    return (*(reinterpret_cast<TileMapObjSlot16Fn>(vtable[16])))(obj);
}

static int TileMap_CallSlot17(void* obj)
{
    void** vtable = *reinterpret_cast<void***>(obj);
    return (*(reinterpret_cast<TileMapObjSlot17Fn>(vtable[17])))(obj);
}

/* g_about (0x4FD390) is the AboutDialog singleton; vtable[2] closes the
 * about window when a resource-0x820 object is clicked in town mode.
 * TODO(integration): expose vtable[2] as a typed method on AboutDialog
 * (ui/AboutDialog.h) and replace this adapter. */
typedef int (__thiscall* TileMapAboutSlot2Fn)(void* self);

static void TileMap_CloseAbout()
{
    void** vtable = *reinterpret_cast<void***>(g_about);
    (*(reinterpret_cast<TileMapAboutSlot2Fn>(vtable[2])))(g_about);
}

/* ================================================================== */
/* TileMap::TileMap — Constructor                                      */
/* Address: 0x454CF0                                                   */
/*                                                                     */
/* Initializes the TileMap: calls FullReset, zeros viewport offsets,   */
/* allocates two DDRAW_SpriteData objects for asset loading/enumeration,*/
/* zeros occupancy bitmap, scroll_drag_active, surface_locked, and the */
/* DDSURFACEDESC buffer.                                               */
/*                                                                     */
/* The vtable is compiler-managed (table at 0x478520).                 */
/* ================================================================== */
TileMap::TileMap()
{
    /* Full TileMap reset (deselect objects, init world, clear grid) */
    FullReset();

    /* Zero viewport offsets */
    viewport_x = 0;
    viewport_y = 0;

    /* Allocate first DDRAW_SpriteData at +0x52488 (asset_load_ptr) */
    void* mem1 = operator_new(0x2C);
    if (mem1 != NULL) {
        asset_load_ptr = DDRAW_SpriteDataCtor(mem1, 7);
    } else {
        asset_load_ptr = NULL;
    }

    /* Allocate second DDRAW_SpriteData at +0x5248C (asset_enum_ptr) */
    void* mem2 = operator_new(0x2C);
    if (mem2 != NULL) {
        asset_enum_ptr = DDRAW_SpriteDataCtor(mem2, 8);
    } else {
        asset_enum_ptr = NULL;
    }

    /* Zero occupancy bitmap pointer */
    occupancy_bitmap = NULL;

    /* Clear flags */
    scroll_drag_active = 0;   /* +0x3C */
    surface_locked = 0;       /* +0x52510 */

    /* Zero the DDSURFACEDESC buffer (31 dwords = 0x7C bytes) */
    for (int i = 0; i < 0x1F; i++) {
        reinterpret_cast<uint32_t*>(ddsurfacedesc_buf)[i] = 0;
    }
}

/* ================================================================== */
/* TileMap::~TileMap — Destructor                                      */
/* Address: 0x454DE0                                                   */
/*                                                                     */
/* Frees TileMap resources: unlocks all, destroys and frees sprite     */
/* data pointers, frees occupancy bitmap.                              */
/* ================================================================== */
TileMap::~TileMap()
{
    /* Unlock all surfaces first */
    FullReset();

    /* Free asset load pointer (+0x52488) */
    if (asset_load_ptr != NULL) {
        DDRAW_SpriteDataDtor(asset_load_ptr);
        GLOBAL_free(asset_load_ptr);
        asset_load_ptr = NULL;
    }

    /* Free asset enum pointer (+0x5248C) */
    if (asset_enum_ptr != NULL) {
        DDRAW_SpriteDataDtor(asset_enum_ptr);
        GLOBAL_free(asset_enum_ptr);
        asset_enum_ptr = NULL;
    }

    /* Free occupancy bitmap (+0x52484) */
    if (occupancy_bitmap != NULL) {
        GLOBAL_free(occupancy_bitmap);
        occupancy_bitmap = NULL;
    }
}

/* ================================================================== */
/* TileMap::ReadTilePointer                                            */
/* ================================================================== */
void* TileMap::ReadTilePointer(size_t data_index) const
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(
        static_cast<uint32_t>(ReadTileValue(data_index))));
}

/* ================================================================== */
/* TileMap::ReadTileValue                                              */
/* ================================================================== */
int32_t TileMap::ReadTileValue(size_t data_index) const
{
    return static_cast<int32_t>(
        static_cast<uint32_t>(tile_data[data_index]) |
        (static_cast<uint32_t>(tile_data[data_index + 1]) << 8) |
        (static_cast<uint32_t>(tile_data[data_index + 2]) << 16) |
        (static_cast<uint32_t>(tile_data[data_index + 3]) << 24));
}

/* ================================================================== */
/* TileMap::WriteTileValue                                             */
/* ================================================================== */
void TileMap::WriteTileValue(size_t data_index, int32_t value)
{
    uint32_t packed = static_cast<uint32_t>(value);
    tile_data[data_index] = static_cast<uint8_t>(packed);
    tile_data[data_index + 1] = static_cast<uint8_t>(packed >> 8);
    tile_data[data_index + 2] = static_cast<uint8_t>(packed >> 16);
    tile_data[data_index + 3] = static_cast<uint8_t>(packed >> 24);
}

/* ================================================================== */
/* TileMap::RecalcViewportCenter                                       */
/* Address: 0x454FA0                                                   */
/*                                                                     */
/* Recalculates viewport_center_x and viewport_center_y from client    */
/* area offsets and the current viewport position.                     */
/* ================================================================== */
void TileMap::RecalcViewportCenter()
{
    viewport_center_x =
        (g_client_offset_x - g_client_width) / 2 + viewport_x + g_client_width;

    viewport_center_y =
        (g_client_offset_y - g_client_height) / 2 + viewport_y + g_client_height;
}

/* ================================================================== */
/* TileMap::FullReset                                                  */
/* Address: 0x454FE0                                                   */
/*                                                                     */
/* Full TileMap reset: deselects game objects, reinitializes world,    */
/* cleans up tooltips, clears all tile data in the grid and occupancy  */
/* bitmap. If windows exist and not in game mode 1, invalidates and    */
/* updates the window.                                                 */
/* ================================================================== */
void TileMap::FullReset()
{
    /* Global reset sequence */
    Game_DeselectGameObject(
        static_cast<int>(reinterpret_cast<intptr_t>(g_game)));  /* 0x4854C8 */
    World_Init(g_world);  /* 0x4A98B0 */
    UI_CleanupTooltips(g_tooltip_mgr);  /* 0x4FD220 */
    /* Original (0x455003): mov ecx,0x4A9990; call 0x41E100 — the body
     * DIRECTLY (the 0x41D310 thunk/vtable[3] dispatch is CGWND_Cleanup's
     * site).  ResetWorldState is virtual, so the qualified call keeps
     * this direct-call shape. */
    g_input_mgr.InputMgr::ResetWorldState();

    /* Clear the trailing header bytes and all named tile storage.
     * The assembly zeroes dwords +0x44..+0x52483 (0x14910 dwords),
     * i.e. _pad_42[2..5] plus the whole tile grid. */
    for (int i = 2; i < 6; ++i) {
        _pad_42[i] = 0;
    }
    for (size_t i = 0; i < sizeof(tile_data); ++i) {
        tile_data[i] = 0;
    }

    /* Fill occupancy bitmap with 0xFF if allocated */
    if (occupancy_bitmap != NULL) {
        int tile_count = static_cast<int>(tile_count_y) * static_cast<int>(tile_count_x);
        int bitmap_size = ((tile_count + (tile_count >> 31 & 7)) >> 3) + 1;

        uint32_t* bitmap32 = reinterpret_cast<uint32_t*>(occupancy_bitmap);
        uint32_t dword_count = bitmap_size >> 2;
        for (uint32_t i = 0; i < dword_count; i++) {
            bitmap32[i] = 0xFFFFFFFF;
        }

        uint8_t* bitmap8 = reinterpret_cast<uint8_t*>(&bitmap32[dword_count]);
        for (uint32_t i = 0; i < (bitmap_size & 3); i++) {
            bitmap8[i] = 0xFF;
        }
    }

    /* Reset tile grid active-layer bytes (at +0x80 and +0x81 within each
     * tile entry) for the 0x41 x 0x51 tile array. */
    for (int y = 0; y < 0x41; y++) {
        for (int x = 0; x < 0x51; x++) {
            int tile_base = (x * 0x41 + y) * 0x40;
            tile_data[tile_base + 0x38] = 0xFF;
            tile_data[tile_base + 0x39] = 0xFF;
        }
    }

    /* Invalidate and update the main window's child handle (CGWND + 0x8)
     * if not in game mode 1. */
    if (g_main_window != NULL) {
        HWND child_wnd = *reinterpret_cast<HWND*>(
            reinterpret_cast<uint8_t*>(g_main_window) + 8);
        if (child_wnd != NULL && g_game_mode != 1) {
            ::InvalidateRect(child_wnd, NULL, FALSE);
            UpdateWindow(child_wnd);
        }
    }
}

/* ================================================================== */
/* TileMap::Init                                                       */
/* Address: 0x454E60                                                   */
/*                                                                     */
/* Initialize TileMap dimensions at startup or on resolution change.   */
/* Sets up tile grid size, viewport center, allocates occupancy bitmap */
/* initialized to all-0xFF.                                            */
/* param use_1024x768: 0=use screen dims (clamped 1024-1280),          */
/*                      1=use fixed 1024x768                           */
/* ================================================================== */
void TileMap::Init(char use_1024x768)
{
    int width;
    int height;

    if (use_1024x768 == 0) {
        /* Use screen dimensions, clamped */
        if (g_screen_width > 0x3FF) {
            if (g_screen_width < 0x501) {
                width = g_screen_width;
                this->width = g_screen_width;
                this->height = g_screen_height;
                height = g_screen_height;
            } else {
                width = 0x500;      /* 1280 max */
                this->width = 0x500;
                this->height = 0x400;
                height = 0x400;
            }
            goto set_center;
        }
        width = 0x400;              /* 1024 */
        this->width = 0x400;
    } else {
        width = 0x400;              /* 1024 */
        this->width = 0x400;
    }
    this->height = 0x300;           /* 768 */
    height = 0x300;

set_center:
    viewport_rect.left = 0;         /* +0x0C */
    viewport_rect.top = 0;          /* +0x10 */
    center_x = width / 2;           /* +0x24 */
    viewport_rect.right = width;    /* +0x14 */
    viewport_rect.bottom = height;  /* +0x18 */
    viewport_x = 0;                 /* +0x1C */
    viewport_y = 0;                 /* +0x20 */
    center_y = height / 2;          /* +0x28 */
    viewport_center_x =
        (g_client_offset_x - g_client_width) / 2 + g_client_width;
    viewport_center_y =
        (g_client_offset_y - g_client_height) / 2 + g_client_height;

    /* Calculate tile counts (round up) */
    tile_count_x = static_cast<short>((width + (width >> 31 & 0xF)) >> 4);
    tile_count_y = static_cast<short>((height + (height >> 31 & 0xF)) >> 4);

    /* Allocate occupancy bitmap */
    if (occupancy_bitmap) {
        GLOBAL_free(occupancy_bitmap);
        occupancy_bitmap = NULL;
    }

    int tile_count = static_cast<int>(tile_count_x) * static_cast<int>(tile_count_y);
    int bitmap_size = ((tile_count + (tile_count >> 31 & 7)) >> 3) + 1;
    occupancy_bitmap = operator_new(bitmap_size);

    if (occupancy_bitmap) {
        /* Fill with 0xFF (all dirty) */
        uint32_t* bitmap32 = reinterpret_cast<uint32_t*>(occupancy_bitmap);
        uint32_t dword_count = bitmap_size >> 2;
        uint32_t remainder = bitmap_size & 3;

        for (uint32_t i = 0; i < dword_count; i++) {
            bitmap32[i] = 0xFFFFFFFF;
        }
        for (uint32_t i = 0; i < remainder; i++) {
            reinterpret_cast<uint8_t*>(&bitmap32[dword_count])[i] = 0xFF;
        }
    }
}

/* ================================================================== */
/* TileMap::GetObjectAt                                                */
/* Address: 0x455620                                                   */
/*                                                                     */
/* Bounds: x in [0, 0x51], y in [0, 0x41]. Returns the layer-0 slot    */
/* value at this+0x48+(x*65+y)*64+layer*4.                             */
/* ================================================================== */
void* TileMap::GetObjectAt(short tile_x, short tile_y, short layer)
{
    if (tile_x < 0 || tile_x >= 0x52 ||
        tile_y < 0 || tile_y >= 0x42) {
        return NULL;
    }

    size_t data_index = TILE_OFFSET(tile_x, tile_y, layer) - 0x48;
    return ReadTilePointer(data_index);
}

/* ================================================================== */
/* TileMap::GetObjectAtEx                                              */
/* Address: 0x455670                                                   */
/*                                                                     */
/* Extended version: scans layers from the active-layer byte down to   */
/* 0 in the ORIGIN region (this+0x64+...) and returns the first        */
/* non-empty object. *layer_out is only written on success (the        */
/* binary leaves it untouched otherwise).                              */
/* ================================================================== */
void* TileMap::GetObjectAtEx(short tile_x, short tile_y, short* layer_out)
{
    void* result = NULL;

    if (tile_x < 0 || tile_x >= 0x52 ||
        tile_y < 0 || tile_y >= 0x42) {
        return NULL;
    }

    int tile_index = static_cast<int>(tile_x) * 0x41 + static_cast<int>(tile_y);

    /* Active-layer byte at this + (tile_index + 2) * 0x40 (= +0x80 within
     * the tile entry). */
    int8_t active = static_cast<int8_t>(
        tile_data[(tile_index + 2) * 0x40 - 0x48]);
    if (active >= 0) {
        for (int8_t layer = active; layer >= 0; layer--) {
            int32_t val = ReadTileValue(
                (static_cast<int>(layer) + tile_index * 0x10) * 4 + 100 - 0x48);
            if (val != 0) {
                *layer_out = layer;
                result = reinterpret_cast<void*>(static_cast<uintptr_t>(
                    static_cast<uint32_t>(val)));
                break;
            }
        }
    }
    return result;
}

/* ================================================================== */
/* TileMap::FindObjectByPos                                            */
/* Address: 0x4556F0                                                   */
/* ================================================================== */
void* TileMap::FindObjectByPos(int pixel_x, int pixel_y)
{
    short tile_x = (pixel_x < 0) ? -1 : static_cast<short>(pixel_x >> 4);
    short tile_y = (pixel_y < 0) ? -1 : static_cast<short>(pixel_y >> 4);

    int tile_index = static_cast<int>(tile_x) * 0x41 + static_cast<int>(tile_y);
    int tile_base = tile_index * 0x40;

    /* Active layer byte at this + tile_base + 0x80 */
    int8_t active_layer = static_cast<int8_t>(
        tile_data[tile_base + 0x80 - 0x48]);

    return ReadTilePointer(tile_base + 0x64 + active_layer * 4 - 0x48);
}

/* ================================================================== */
/* TileMap::GetTileOrigin                                              */
/* Address: 0x455740                                                   */
/*                                                                     */
/* Reads the ORIGIN-region slot (this+0x64 + tile*0x40 + layer*4) and, */
/* when non-empty and in bounds, returns the object's tile origin at   */
/* +0x88 via *out_id. *out_id = -1 when empty or out of bounds.        */
/*                                                                     */
/* The binary reads the slot before checking bounds; for out-of-bounds */
/* coordinates the result is -1 either way, so the bounds check is     */
/* done first here (documented equivalence, avoids an OOB array read). */
/* ================================================================== */
int* TileMap::GetTileOrigin(int* out_id, short tile_x, short tile_y, short layer)
{
    if (tile_x < 0 || tile_x > 0x51 ||
        tile_y < 0 || tile_y > 0x41) {
        *out_id = -1;
        return out_id;
    }

    size_t data_index = (static_cast<int>(tile_x) * 0x41 + static_cast<int>(tile_y)) * 0x40 +
                        static_cast<int>(layer) * 4 + 0x1C;
    int obj_val = ReadTileValue(data_index);

    if (obj_val != 0) {
        *out_id = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(
            static_cast<uintptr_t>(obj_val)) + 0x88);
        return out_id;
    }
    *out_id = -1;
    return out_id;
}

/* ================================================================== */
/* TileMap::GetTileOriginEx                                            */
/* Address: 0x4557C0                                                   */
/*                                                                     */
/* Like GetTileOrigin but reads the standard 0x48 tile slot. Same      */
/* documented equivalence for the bounds check order.                  */
/* ================================================================== */
void TileMap::GetTileOriginEx(int* out_packed, short tile_x, short tile_y, short layer)
{
    if (tile_x < 0 || tile_x > 0x51 ||
        tile_y < 0 || tile_y > 0x41) {
        *out_packed = -1;
        return;
    }

    size_t data_index = (static_cast<int>(tile_x) * 0x41 + static_cast<int>(tile_y)) * 0x40 +
                        static_cast<int>(layer) * 4;
    int obj_val = ReadTileValue(data_index);

    if (obj_val != 0) {
        *out_packed = *reinterpret_cast<int*>(
            reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(obj_val)) + 0x88);
        return;
    }
    *out_packed = -1;
}

/* ================================================================== */
/* TileMap_IsTileOccupied                                              */
/* Address: 0x457B60                                                   */
/*                                                                     */
/* Checks if two tile resources conflict. Uses type byte at +0x08:     */
/* 0x0C (12) = scenery, 0x03 (3) = track/building.                     */
/* Also checks RESDATA_IsSceneryTile and RESDATA_IsWaterTile.          */
/* Returns: 50 = scenery-scenery conflict,                             */
/*          10 = building-building / scenery-water conflict,           */
/*          -1 = no conflict.                                          */
/* ================================================================== */
int TileMap_IsTileOccupied(int tile_resource_a, int tile_resource_b)
{
    int result = -1;
    TileMapResource* resource_a = reinterpret_cast<TileMapResource*>(tile_resource_a);
    TileMapResource* resource_b = reinterpret_cast<TileMapResource*>(tile_resource_b);
    char type_a = resource_a->object_type;
    char type_b = resource_b->object_type;

    /* Scenery vs scenery — always conflict */
    if (type_a == 0x0C && type_b == 0x0C) {
        return 50;
    }

    /* Building (type 3) vs building (type 3) */
    if (type_a == 0x03 && type_b == 0x03) {
        return 10;
    }

    /* Scenery (0x0C) vs building/track (0x03) */
    if (type_a == 0x0C && type_b == 0x03) {
        if (RESDATA_IsSceneryTile(tile_resource_b) &&
            resource_a->resource_id > 0x3010) {
            result = 10;
        }
        if (RESDATA_IsWaterTile(tile_resource_b)) {
            return 10;
        }
        return result;
    }

    /* Building/track (0x03) vs scenery (0x0C) */
    if (type_a == 0x03 && type_b == 0x0C) {
        if (RESDATA_IsSceneryTile(tile_resource_a) &&
            resource_b->resource_id > 0x3010) {
            result = 10;
        }
        if (RESDATA_IsWaterTile(tile_resource_a)) {
            return 10;
        }
        return result;
    }

    return -1;
}

/* ================================================================== */
/* TileMap_IsTileBuildable                                             */
/* Address: 0x457C20                                                   */
/*                                                                     */
/* Checks whether tile_b may be placed on top of tile_a (the base     */
/* tile).  Dispatch is on the BASE tile's object type (+0x08,          */
/* zero-extended byte, unsigned):                                      */
/*   3 (track/building): base must be a track tile (0x44BD70 — state   */
/*       +0x63A in {0x10,0x11}); road (0x0D) on top -> 0x64, all       */
/*       other top types -> -1                                         */
/*   0x0C (scenery):    base must be an editor sprite (0x41F430,       */
/*       IsEditorSprite); scenery on top -> IsEditorSprite(top) ?      */
/*       0x64 : -1; road on top -> 0x64; else -1                      */
/*   0x0D (road):       track (3) on top -> IsTrackTile(top) (0x44BD70)*/
/*       ? 0x64 : -1; scenery/road on top -> 0x64; else -1            */
/*   anything else:     -1                                             */
/*                                                                     */
/* Returns 0x64 (== 100) on every valid placement and -1 when blocked; */
/* there is no third value.  The neg/sbb/and-$0x65/dec idiom at        */
/* 0x457C69/0x457C9E yields 0x64 when its predicate holds and -1       */
/* otherwise (0x65 - 1).                                               */
/* ================================================================== */
int TileMap_IsTileBuildable(int tile_resource_a, int tile_resource_b)
{
    TileMapResource* resource_a = reinterpret_cast<TileMapResource*>(tile_resource_a);
    TileMapResource* resource_b = reinterpret_cast<TileMapResource*>(tile_resource_b);
    const uint8_t type_a = resource_a->object_type;
    const uint8_t type_b = resource_b->object_type;

    if (type_a == 0x03) {
        /* Track/building base (0x457CAB): base must itself be a track
         * tile; only road (0x0D) may be placed on top. */
        if (!RESDATA_IsTrackTile(tile_resource_a)) {    /* 0x44BD70 */
            return -1;
        }
        return type_b == 0x0D ? 0x64 : -1;
    }

    if (type_a == 0x0C) {
        /* Scenery base (0x457C76): the base must be an editor sprite
         * before anything can be placed on it. */
        if (!resource_a->IsEditorSprite()) {            /* 0x41F430 */
            return -1;
        }
        if (type_b == 0x0C) {
            /* Scenery on scenery: both must be editor sprites. */
            return resource_b->IsEditorSprite() ? 0x64 : -1;
        }
        return type_b == 0x0D ? 0x64 : -1;
    }

    if (type_a == 0x0D) {
        /* Road base (0x457C45). */
        if (type_b == 0x03) {
            return RESDATA_IsTrackTile(tile_resource_b) ? 0x64 : -1; /* 0x44BD70 */
        }
        return (type_b == 0x0C || type_b == 0x0D) ? 0x64 : -1;
    }

    return -1;
}

/* ================================================================== */
/* TileMap::InvalidateRect                                             */
/* Address: 0x455840                                                   */
/* ================================================================== */
void TileMap::InvalidateRect(int left, int top, int right, int bottom)
{
    /* Only process in game modes 3 or 4 */
    if (g_game_mode != 4 && g_game_mode != 3) {
        return;
    }

    /* Convert pixel coords to tile coords */
    short tile_left   = (left < 0)   ? static_cast<short>(-1) : static_cast<short>(left >> 4);
    short tile_top    = (top < 0)    ? static_cast<short>(-1) : static_cast<short>(top >> 4);
    short tile_right  = (right - 1 < 0)  ? static_cast<short>(-1) : static_cast<short>((right - 1) >> 4);
    short tile_bottom = (bottom - 1 < 0) ? static_cast<short>(-1) : static_cast<short>((bottom - 1) >> 4);

    /* Clamp to valid tile range */
    if (tile_left < 0)   tile_left = 0;
    if (tile_right >= tile_count_x)  tile_right = tile_count_x - 1;
    if (tile_top < 0)    tile_top = 0;
    if (tile_bottom >= tile_count_y) tile_bottom = tile_count_y - 1;

    /* Set dirty bits in occupancy bitmap */
    for (short y = tile_top; y <= tile_bottom; y++) {
        for (short x = tile_left; x <= tile_right; x++) {
            uint32_t bit_index = g_player_id * static_cast<int>(y) + static_cast<int>(x);
            uint8_t& bitmap_byte =
                static_cast<uint8_t*>(g_tile_occupied_bitmap)[bit_index >> 3];
            bitmap_byte |= ATTR_0047f108[bit_index & 7];
        }
    }
}

/* ================================================================== */
/* TileMap::GetViewport                                                */
/* Address: 0x4579D0                                                   */
/*                                                                     */
/* Gets the neighboring object in a given direction from a sprite.     */
/* Uses the sprite's sub-object position data stored in the resource   */
/* header. If no neighbor defined, returns NULL. After finding a       */
/* candidate, checks distance is <= 17 pixels.                         */
/* ================================================================== */
TileMapObject* TileMap::GetViewport(TileMapObject* sprite, int direction)
{
    TileMapResource* resource = sprite->resource;
    if (resource == NULL) {
        return NULL;
    }

    /* Check if neighbor is defined for this direction */
    if (resource->neighbor_def[direction][0] == 0 &&
        resource->neighbor_def[direction][1] == 0) {
        return NULL;
    }

    /* Get sub-object position for this direction */
    int pos_xy[2];
    Entity_GetSubObjectPosition(sprite, pos_xy, direction);
    int world_x = pos_xy[0];
    int world_y = pos_xy[1];

    short tile_x = (world_x < 0) ? -1 : static_cast<short>(world_x >> 4);
    short tile_y = (world_y < 0) ? -1 : static_cast<short>(world_y >> 4);

    /* Direction offsets: 0=up, 1=right, 2=down, 3=left */
    switch (direction) {
    case 0:  tile_x--; break;
    case 1:  tile_y++; break;
    case 2:  tile_x++; break;
    case 3:  tile_y--; break;
    default: break;
    }

    /* Bounds check */
    if (tile_x < 0 || tile_x > 0x51 ||
        tile_y < 0 || tile_y > 0x41) {
        return NULL;
    }

    /* Get the object at the target tile (layer 0) */
    TileMapObject* neighbor = static_cast<TileMapObject*>(
        ReadTilePointer((tile_x * 0x41 + static_cast<int>(tile_y)) * 0x40));

    /* Distance check: verify neighbor is close enough (<= 17 pixels) */
    if (neighbor != NULL && neighbor->resource != NULL) {
        int neighbor_pos[2];
        Entity_GetSubObjectPosition(neighbor, neighbor_pos,
                                     (direction - 2) & 3);
        int dist_sq = Math_DistSquared(world_x, world_y,
                                        neighbor_pos[0], neighbor_pos[1]);
        if (dist_sq > 0x11) {
            return NULL;
        }
    }

    return neighbor;
}

/* ================================================================== */
/* TileMap::SetViewport                                                */
/* Address: 0x4576B0                                                   */
/*                                                                     */
/* Evaluates 4 adjacent tiles (N/S/E/W) around a building sprite.     */
/* For each valid neighbor, checks if buildable. Returns count (0-4)   */
/* or 1 if any neighbor has a station resource (0xC50/0xC52).          */
/* ================================================================== */
char TileMap::SetViewport(TileMapObject* building_sprite)
{
    if (building_sprite == NULL) {
        return 0;
    }

    TileMapResource* resource = building_sprite->resource;
    if (resource->IsEditorSprite()) {     /* 0x41F430 */
        return 0;
    }

    /* Get viewport neighbors for all 4 directions */
    TileMapObject* neighbors[4] = {NULL, NULL, NULL, NULL};

    for (int dir = 0; dir < 4; dir++) {
        TileMapObject* neighbor = GetViewport(building_sprite, dir);
        neighbors[dir] = neighbor;

        if (neighbor != NULL) {
            TileMapResource* res = neighbor->resource;

            /* Skip sprite-editor objects */
            while (res != NULL && res->IsEditorSprite()) {     /* 0x41F430 */
                neighbor = GetViewport(neighbor, dir);
                neighbors[dir] = neighbor;
                res = neighbor == NULL ? NULL : neighbor->resource;
            }

            /* If neighbor exists but isn't buildable, discard it */
            if (neighbors[dir] != NULL &&
                TileMap_IsTileBuildable(reinterpret_cast<intptr_t>(resource),
                    reinterpret_cast<intptr_t>(res)) < 0) {
                neighbors[dir] = NULL;
            }
        }
    }

    /* Count valid neighbors with diagonal correction */
    char valid_count = 0;
    for (int dir = 0; dir < 4; dir++) {
        if (neighbors[dir] != NULL &&
            (neighbors[(dir + 1) & 3] != NULL || neighbors[(dir - 2) & 3] == NULL)) {
            valid_count++;
        }
    }

    /* Check for station resources (0xC50 = station type, 0xC52 = other) */
    for (int dir = 0; dir < 4; ++dir) {
        if (neighbors[dir] != NULL) {
            TileMapResource* neighbor_resource = neighbors[dir]->resource;
            int res_id = neighbor_resource == NULL ? -1 : neighbor_resource->resource_id;
            int expected = (dir & 1) == 0 ? 0xC50 : 0xC52;
            if (res_id == expected) return 1;
        }
    }

    return valid_count;
}

/* ================================================================== */
/* TileMap::UpdateViewport                                             */
/* Address: 0x4573E0                                                   */
/*                                                                     */
/* Like SetViewport but for generic (non-building) sprites. For type 7*/
/* (multi-track) sprites, checks the 2x2 tile neighborhood and        */
/* subtracts scenery (type 0x0C) tiles from the count.                */
/* ================================================================== */
char TileMap::UpdateViewport(TileMapObject* sprite, short sprite_type)
{
    if (sprite == NULL) {
        return 0;
    }

    TileMapResource* resource = sprite->resource;
    if (resource->IsEditorSprite()) {     /* 0x41F430 */
        return 0;
    }

    /* Get viewport neighbors for all 4 directions */
    TileMapObject* neighbors[4] = {NULL, NULL, NULL, NULL};

    for (int dir = 0; dir < 4; dir++) {
        TileMapObject* neighbor = GetViewport(sprite, dir);
        neighbors[dir] = neighbor;

        if (neighbor != NULL) {
            TileMapResource* res = neighbor->resource;

            /* Skip sprite-editor objects */
            while (res != NULL && res->IsEditorSprite()) {     /* 0x41F430 */
                neighbor = GetViewport(neighbor, dir);
                neighbors[dir] = neighbor;
                res = neighbor == NULL ? NULL : neighbor->resource;
            }

            /* If neighbor exists but is occupied (conflicts), discard it */
            if (neighbors[dir] != NULL &&
                TileMap_IsTileOccupied(reinterpret_cast<intptr_t>(resource),
                    reinterpret_cast<intptr_t>(res)) < 0) {
                neighbors[dir] = NULL;
            }
        }
    }

    /* Count valid neighbors */
    char valid_count = 0;
    for (int dir = 0; dir < 4; dir++) {
        if (neighbors[dir] != NULL &&
            (neighbors[(dir + 1) & 3] != NULL || neighbors[(dir - 2) & 3] == NULL)) {
            valid_count++;
        }
    }

    /* For type 7 (multi-track objects), check 2x2 tile neighborhood */
    if (sprite_type == 7 && valid_count == 4) {
        int tile_x = sprite->tile_x;
        int tile_y = sprite->tile_y;

        void* corner[4];
        corner[0] = GetObjectAt(static_cast<short>(tile_x - 1), static_cast<short>(tile_y - 1), 0);
        corner[1] = GetObjectAt(static_cast<short>(tile_x - 1), static_cast<short>(tile_y + 1), 0);
        corner[2] = GetObjectAt(static_cast<short>(tile_x + 1), static_cast<short>(tile_y - 1), 0);
        corner[3] = GetObjectAt(static_cast<short>(tile_x + 1), static_cast<short>(tile_y + 1), 0);

        /* Subtract scenery tiles (type 0x0C) from valid count */
        for (int i = 0; i < 4; i++) {
            if (corner[i] != NULL) {
                TileMapResource* corner_res =
                    static_cast<TileMapObject*>(corner[i])->resource;
                char corner_type = corner_res == NULL ? 0 : corner_res->object_type;
                if (corner_type == 0x0C) {
                    valid_count--;
                }
            }
        }
    }

    /* Check for station resources (0xC50/0xC52) */
    for (int dir = 0; dir < 4; ++dir) {
        if (neighbors[dir] != NULL) {
            TileMapResource* neighbor_resource = neighbors[dir]->resource;
            int res_id = neighbor_resource == NULL ? -1 : neighbor_resource->resource_id;
            int expected = (dir & 1) == 0 ? 0xC50 : 0xC52;
            if (res_id == expected) return 1;
        }
    }

    return valid_count;
}

/* ================================================================== */
/* TileMap::GetTileRect                                                */
/* Address: 0x457830                                                   */
/*                                                                     */
/* Fills occupancy data for each of the 4 directions. Per direction:   */
/*   +0xC4 (occupancy_links[dir]) = adjacent object pointer (when it   */
/*        is not a sprite-editor object),                              */
/*   +0xD4 (occupancy_scores[dir]) += occupancy score,                 */
/*   walk continues while the neighbour's +0xE4 (occupancy_more) is    */
/*        negative; stops when it is >= 0 or the chain ends.           */
/* ================================================================== */
void TileMap::GetTileRect(TileMapObject* sprite)
{
    if (sprite == NULL) return;

    TileMapResource* resource = sprite->resource;

    for (int dir = 0; dir < 4; dir++) {
        sprite->occupancy_links[dir] = 0;     /* +0xC4 */
        sprite->occupancy_scores[dir] = 0;    /* +0xD4 */

        TileMapObject* neighbor = GetViewport(sprite, dir);
        while (neighbor != NULL) {
            TileMapResource* neighbor_res = neighbor->resource;
            int occupancy = TileMap_IsTileOccupied(
                reinterpret_cast<intptr_t>(resource),
                reinterpret_cast<intptr_t>(neighbor_res));
            if (occupancy < 0) break;

            if (!neighbor_res->IsEditorSprite()) {     /* 0x41F430 */
                sprite->occupancy_links[dir] = reinterpret_cast<intptr_t>(neighbor);
            }
            sprite->occupancy_scores[dir] += occupancy;

            /* Walk to next connected tile only while the neighbour's
             * occupancy_more (+0xE4) is negative. */
            if (neighbor->occupancy_more >= 0) break;
            neighbor = GetViewport(neighbor, dir);
        }
    }
}

/* ================================================================== */
/* TileMap::GetTileAt                                                  */
/* Address: 0x457900                                                   */
/*                                                                     */
/* Like GetTileRect but uses IsTileBuildable instead of IsTileOccupied */
/* and the build chain fields:                                        */
/*   +0xE8 (build_links[dir]) = adjacent object pointer,               */
/*   +0xF8 (build_scores[dir]) += build score,                         */
/*   walk continues while the neighbour's +0x108 (build_more) is       */
/*        negative.                                                    */
/* ================================================================== */
void TileMap::GetTileAt(TileMapObject* sprite)
{
    if (sprite == NULL) return;

    TileMapResource* resource = sprite->resource;

    for (int dir = 0; dir < 4; dir++) {
        sprite->build_links[dir] = 0;         /* +0xE8 */
        sprite->build_scores[dir] = 0;        /* +0xF8 */

        TileMapObject* neighbor = GetViewport(sprite, dir);
        while (neighbor != NULL) {
            TileMapResource* neighbor_res = neighbor->resource;
            int buildable = TileMap_IsTileBuildable(
                reinterpret_cast<intptr_t>(resource),
                reinterpret_cast<intptr_t>(neighbor_res));
            if (buildable < 0) break;

            if (!neighbor_res->IsEditorSprite()) {     /* 0x41F430 */
                sprite->build_links[dir] = reinterpret_cast<intptr_t>(neighbor);
            }
            sprite->build_scores[dir] += buildable;

            if (neighbor->build_more >= 0) break;
            neighbor = GetViewport(neighbor, dir);
        }
    }
}

/* ================================================================== */
/* TileMap::ClearInputProcessedFlag                                    */
/* Address: 0x456140                                                   */
/* ================================================================== */
void TileMap::ClearInputProcessedFlag()
{
    scroll_drag_active = 0;
}

/* ================================================================== */
/* TileMap::ScrollRect                                                 */
/* Address: 0x4553E0                                                   */
/*                                                                     */
/* Validates placement of a target building at a grid offset from its  */
/* current position. Checks road tile category, then iterates the      */
/* 3D occupancy grid (w x h x d, indexed [x][y][z] = x*63 + y*7 + z)   */
/* of the target building to verify that no blocking objects are in    */
/* the way.                                                            */
/* ================================================================== */
char TileMap::ScrollRect(char use_sound, TileMapObject* target_building,
    short delta_x, unsigned short delta_y, int placement_mode)
{
    TileMapObject* building = target_building;
    byte grid_w = building->grid_width;
    byte grid_h = building->grid_height;

    /* Bounds check (unsigned add of the byte dims, matching the binary) */
    if (static_cast<int>(static_cast<unsigned int>(grid_w) +
                         static_cast<unsigned int>(delta_x)) > tile_count_x ||
        static_cast<int>(static_cast<unsigned int>(grid_h) +
                         static_cast<unsigned int>(delta_y)) > tile_count_y) {
        return 0;
    }

    char valid = 1;

    /* Check road tile compatibility */
    if (building->object_type == 0x03) {
        if (RESDATA_IsRoadTile(reinterpret_cast<intptr_t>(target_building))) {
            unsigned int category = RESDATA_GetTileCategory(
                target_building, delta_x, delta_y);
            valid = static_cast<char>(category);
        }
    }

    /* If still valid, scan the occupancy grid */
    if (valid == 1) {
        byte bVar4 = building->grid_width;
        byte bVar7 = building->grid_height;

        if (bVar4 != 0 && bVar7 != 0) {
            valid = 1;
            short depth = building->grid_depth;

            for (short iz = 0; iz < depth; iz++) {
                for (short iy = 0; iy < static_cast<short>(bVar7); iy++) {
                    for (short ix = 0; ix < static_cast<short>(bVar4); ix++) {
                        /* occupancy grid is indexed [x][y][z] = x*63+y*7+z */
                        int8_t occ = building->occupancy_grid[
                            static_cast<int>(ix) * 9 * 7 +
                            static_cast<int>(iy) * 7 +
                            static_cast<int>(iz)];
                        if (occ != 0) {
                            int tile_x = static_cast<int>(iy) + static_cast<int>(delta_x);
                            int tile_y = static_cast<int>(ix) + static_cast<int>(delta_y);
                            int obj = ReadTileValue(
                                TILE_OFFSET(tile_x, tile_y, iz) - 0x48);
                            if (obj != 0) {
                                if (g_allow_building_placement == 1 &&
                                    static_cast<TileMapObject*>(
                                        reinterpret_cast<void*>(obj))->is_moving == 1 &&
                                    (g_disable_input == 0 ||
                                     g_game_mode == 3 ||
                                     g_game_mode == 1)) {
                                    if (use_sound) {
                                        PlaySoundAt(0x5024,
                                            static_cast<int>(delta_x) << 4,
                                            static_cast<int>(delta_y) << 4, 4);
                                        void* ptr = ReadTilePointer(
                                            TILE_OFFSET(
                                                static_cast<short>(ix + delta_x),
                                                static_cast<short>(iy + delta_y),
                                                iz) - 0x48);
                                        if (ptr) {
                                            ScrollTo(
                                                static_cast<TileMapObject*>(ptr),
                                                placement_mode);
                                        }
                                    }
                                } else {
                                    valid = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return valid;
}

/* ================================================================== */
/* TileMap::ScrollTo                                                   */
/* Address: 0x455AB0                                                   */
/*                                                                     */
/* Two-phase clear: removes object from original tile area (Phase 1)   */
/* and destination area (Phase 2) based on resource grid dimensions.   */
/* Updates occupancy bitmap and decrements active layer counts.        */
/* Finally removes object from INPUT manager.                          */
/*                                                                     */
/* Verified against disassembly (215 instructions, 0x455AB0-0x455D5E). */
/* ================================================================== */
void* TileMap::ScrollTo(TileMapObject* target, int scroll_flag)
{
    if (target == NULL) {
        return NULL;
    }

    /* Check is_moving flag at +0xC0 and object_state at +0x06 */
    if (target->is_moving == 0 || target->object_state != 1) {
        return NULL;
    }

    TileMapResource* resource = target->resource;
    short base_x = target->tile_x;   /* +0x88 */
    short tile_y = target->tile_y;   /* +0x8A */

    byte grid_span_y = resource->grid_span_y;       /* +0x16B */
    byte original_span = resource->original_span;    /* +0x16C */
    byte grid_width  = resource->grid_width;         /* +0x168 */
    byte grid_height = resource->grid_height;        /* +0x169 */

    /* ---- Phase 1: Clear original tile area ---- */
    /* Iterates y from tile_y to tile_y+original_span, x from base_x to
     * base_x+grid_span_y. Scans layers 6..0 in the origin region
     * (this + tile*0x40 + 0x64 + layer*4). */
    {
        int y = static_cast<int>(tile_y);
        int y_end = static_cast<int>(static_cast<unsigned short>(
            static_cast<unsigned short>(tile_y) +
            static_cast<unsigned short>(original_span)));

        for (; y < y_end; y++) {
            int x = static_cast<int>(base_x);
            int x_end = static_cast<int>(static_cast<unsigned short>(
                static_cast<unsigned short>(grid_span_y) + base_x));

            for (; x < x_end; x++) {
                int tile_idx = y + x * 0x41;

                /* Active layer byte at this + (tile_idx + 2) * 0x40 */
                int8_t* active_layer_byte =
                    reinterpret_cast<int8_t*>(
                        reinterpret_cast<uint8_t*>(this) + (tile_idx + 2) * 0x40);
                int8_t active = *active_layer_byte;

                for (int8_t layer = 6; layer >= 0; layer--) {
                    /* Slot at this + tile_idx*0x40 + 0x64 + layer*4 */
                    int* slot = reinterpret_cast<int*>(
                        reinterpret_cast<uint8_t*>(this) + tile_idx * 0x40 +
                        0x64 + static_cast<int>(layer) * 4);
                    if (reinterpret_cast<void*>(static_cast<uintptr_t>(*slot)) == target) {
                        /* Decrement active layer count if this was the top */
                        if (active == layer) {
                            *active_layer_byte = active - 1;
                        }
                        *slot = 0;

                        /* Set occupancy bit (mark as dirty) */
                        uint32_t bit_idx = static_cast<uint32_t>(g_player_id) * static_cast<uint32_t>(y) +
                                           static_cast<uint32_t>(x);
                        uint8_t* bitmap_byte =
                            reinterpret_cast<uint8_t*>(g_tile_occupied_bitmap) + (bit_idx >> 3);
                        *bitmap_byte |= ATTR_0047f108[bit_idx & 7];
                    }
                }

                /* Compress active layer byte: while topmost slot is empty, decrement */
                active = *active_layer_byte;
                while (active >= 0) {
                    int* slot = reinterpret_cast<int*>(
                        reinterpret_cast<uint8_t*>(this) +
                        (static_cast<int>(active) + tile_idx * 0x10 + 0x19) * 4);
                    if (*slot == 0) {
                        *active_layer_byte = *active_layer_byte - 1;
                        active = *active_layer_byte;
                    } else {
                        break;
                    }
                }
            }
        }
    }

    /* ---- Phase 2: Clear destination area ---- */
    /* Recompute tile_y: tile_y += original_span - grid_height */
    /* Iterates y from new_tile_y to new_tile_y+grid_height, x from base_x
     * to base_x+grid_width. Uses the standard layer region
     * (this + tile*0x40 + 0x48 + layer*4). */
    {
        tile_y = tile_y + static_cast<unsigned short>(original_span - grid_height);
        int y = static_cast<int>(tile_y);
        int y_end = static_cast<int>(static_cast<unsigned short>(
            static_cast<unsigned short>(grid_height) + tile_y));

        for (; y < y_end; y++) {
            int x = static_cast<int>(base_x);
            int x_end = static_cast<int>(static_cast<unsigned short>(
                static_cast<unsigned short>(grid_width) + base_x));

            for (; x < x_end; x++) {
                int tile_idx = y + x * 0x41;

                /* Active layer byte at this + tile_idx*0x40 + 0x81 */
                int8_t* active_layer_byte =
                    reinterpret_cast<int8_t*>(
                        reinterpret_cast<uint8_t*>(this) + tile_idx * 0x40 + 0x81);
                int8_t active = *active_layer_byte;

                if (active >= 0) {
                    /* Scan from active layer down to 0 in the standard
                     * layer region (this + tile_idx*0x40 + 0x48 + layer*4) */
                    int* slot = reinterpret_cast<int*>(
                        reinterpret_cast<uint8_t*>(this) +
                        (tile_idx * 0x10 + static_cast<int>(active)) * 4 + 0x48);
                    for (int8_t layer = active; layer >= 0; layer--) {
                        if (reinterpret_cast<void*>(static_cast<uintptr_t>(*slot)) == target) {
                            if (*active_layer_byte == layer) {
                                *active_layer_byte = *active_layer_byte - 1;
                            }
                            *slot = 0;
                        }
                        slot--;
                    }
                }

                /* Compress active layer byte */
                active = *active_layer_byte;
                while (active >= 0) {
                    int* slot = reinterpret_cast<int*>(
                        reinterpret_cast<uint8_t*>(this) +
                        (static_cast<int>(active) + tile_idx * 0x10 + 0x12) * 4);
                    if (*slot == 0) {
                        *active_layer_byte = *active_layer_byte - 1;
                        active = *active_layer_byte;
                    } else {
                        break;
                    }
                }
            }
        }
    }

    /* Remove object from INPUT manager; the binary returns EAX with the
     * low byte set to 1 (CONCAT31(uVar8 >> 8, 1)). */
    uintptr_t rem = static_cast<uintptr_t>(
        INPUT_RemoveObject(&g_input_mgr, target,
                           static_cast<unsigned int>(scroll_flag)));
    return reinterpret_cast<void*>((rem & 0xFFFFFF00U) | 1U);
}

/* ================================================================== */
/* TileMap::HandleClick                                                */
/* Address: 0x455D60                                                   */
/*                                                                     */
/* Mouse click handler. Game mode 3: hit-test + building selection +   */
/* special actions. Game mode 4: drag-scroll + building placement.     */
/* Returns 0 if no action, 1 if click was handled.                     */
/* ================================================================== */
char TileMap::HandleClick(int screen_x, int screen_y)
{
    short tile_x = (screen_x < 0) ? static_cast<short>(-1) : static_cast<short>(screen_x >> 4);
    short tile_y = (screen_y < 0) ? static_cast<short>(-1) : static_cast<short>(screen_y >> 4);

    if (g_game_mode == 3) {
        /* ================= Game mode 3: town object selection ========= */
        char result = 0;
        short layer_out = 0;
        void* obj = GetObjectAtEx(tile_x, tile_y, &layer_out);

        if (g_click_on_building != 0) {
            if (obj == NULL) {
                goto mode3_no_object;
            }
            TileMap_CallSlot16(obj);   /* vtable[16] — click feedback */
            result = 1;
        }

        if (obj == NULL) {
mode3_no_object:
            if (g_click_on_town != 1) {
                return result;
            }
            if (Town_SelectBuilding(g_town_view, 0) == 0 &&
                DDRAW_SelectBuilding(g_ddraw_building, 0) == 0) {
                return 0;
            }
            return 1;
        }

        if (g_click_on_town != 1) {
            return result;
        }

        result = static_cast<char>(TileMap_CallSlot17(obj));  /* vtable[17] */

        int res_id = *reinterpret_cast<int*>(
            reinterpret_cast<uint8_t*>(
                static_cast<TileMapObject*>(obj)->resource) + 4);

        switch (res_id) {
        case 0x820:
            /* About dialog */
            TileMap_CloseAbout();
            return result;
        case 0x818:
            CGWND_SetMode(7);
            return result;
        case 0x848:
            CGWND_SetMode(6);
            return result;
        case 0xC5C:
        case 0xC5E:
        case 0xC60:
            CGWND_SetMode(5);
            return result;
        case 0xC42:
        case 0xC44:
        case 0xC46:
        case 0xC48:
        case 0x3011:
        case 0x3013:
        case 0x3015:
        case 0x3017:
        case 0x3019:
        case 0x301B:
            /* Scenario/station objects: enter network setup when a
             * scenario is active (Netman +0x7C4 == 2). */
            if (*reinterpret_cast<int*>(
                    reinterpret_cast<uint8_t*>(g_netman) + 0x7C4) == 2) {
                CGWND_SetMode(9);
                return result;
            }
            goto mode3_town_select;
        default:
            break;
        }

        if (g_click_on_town != 1) {
            return result;
        }
mode3_town_select:
        result = static_cast<char>(Town_SelectBuilding(
            g_town_view, static_cast<int>(reinterpret_cast<intptr_t>(obj))));
        return result;
    }

    if (g_game_mode != 4) {
        return 0;
    }

    /* ================= Game mode 4: drag-scroll + placement =========== */
    short layer_out = 0;
    void* obj = GetObjectAtEx(tile_x, tile_y, &layer_out);

    if (g_build_mode == 1 && g_click_on_building == 1 && g_placement_valid == 0) {
        if (obj != NULL) {
            if (scroll_drag_active != 0 &&
                (screen_x - drag_start_x >= 0x10 ||
                 screen_y - drag_start_y >= 0x10)) {
                Scroll(screen_x, screen_y, drag_start_x, drag_start_y);
                drag_start_x = screen_x;
                scroll_drag_active = 1;
                drag_start_y = screen_y;
                return 1;
            }
            ScrollTo(static_cast<TileMapObject*>(obj), 1);
        }
        drag_start_x = screen_x;
        scroll_drag_active = 1;
        drag_start_y = screen_y;
        return 1;
    }

    if (g_build_mode == 2 && g_placement_resource_id != -1 &&
        g_click_on_building == 1 && g_placement_valid == 0) {
        int* result = FindObject(static_cast<unsigned int>(g_placement_resource_id),
                                 tile_x, tile_y, 0, 1);
        if (g_placement_blocked != 0) {
            return 1;
        }
        if (result == NULL) {
            if (g_disable_input != 0) {
                return 1;
            }
            PlaySoundAt(0x501B, screen_x, screen_y, 4);
            return 1;
        }
        PlaySoundAt(0x501A, screen_x, screen_y, 4);
        return 1;
    }

    if (obj == NULL) {
        return 0;
    }
    return 0;
}

/* ================================================================== */
/* TileMap::InvalidateDirtyRects                                       */
/* Address: 0x456150                                                   */
/*                                                                     */
/* Main render pipeline. Builds dirty RECT list from occupancy bitmap  */
/* (one node per horizontal run of dirty tiles per row), appends the   */
/* selection rects, blits cursor surface, merges rects via             */
/* ProcessDirtyRects (until it reports no merge), clips via            */
/* FreeDirtyRects, then for each rect locks the primary surface,       */
/* renders via ProcessRect, unlocks, presents and frees. Finally the   */
/* whole occupancy bitmap is cleared.                                  */
/* ================================================================== */
void TileMap::InvalidateDirtyRects(char force_all)
{
    RECT* head = NULL;
    RECT* last = NULL;

    /* Only process in game modes 3 or 4, and not while locked */
    if ((g_game_mode != 3 && g_game_mode != 4) || g_lock_update_flag == 1) {
        return;
    }

    World_Lock(g_world);  /* 0x4A98B0 */

    RECT pending;
    bool no_pending = true;    /* bVar3: true = no rect pending */

    short start_x, start_y, end_x, end_y;
    if ((g_is_fullscreen == 0 && g_world_width <= g_screen_width) || force_all != 0) {
        start_x = 0;
        start_y = 0;
        end_x = tile_count_x;
        end_y = tile_count_y;
    } else {
        start_x = (viewport_x < 0) ? static_cast<short>(-1) : static_cast<short>(viewport_x >> 4);
        end_x = (viewport_x + g_client_offset_x < 0) ? static_cast<short>(-1)
                : static_cast<short>((viewport_x + g_client_offset_x) >> 4);
        start_y = (viewport_y < 0) ? static_cast<short>(-1) : static_cast<short>(viewport_y >> 4);
        end_y = (viewport_y + g_client_offset_y < 0) ? static_cast<short>(-1)
                : static_cast<short>((viewport_y + g_client_offset_y) >> 4);
        if (end_x < tile_count_x) end_x = static_cast<short>(end_x + 1);
        if (end_y < tile_count_y) end_y = static_cast<short>(end_y + 1);

        /* Town-mode clamp: restrict the scan to the town overlay rect. */
        if (g_allow_building_placement != 0) {
            int ov;
            int nx;

            ov = g_town_overlay_rect;
            nx = (ov < 0) ? -1 : (ov >> 4);
            if (start_x >= nx) {
                start_x = static_cast<short>((ov < 0) ? 0xFFFF : (ov >> 4));
            }
            if (start_x < 1) start_x = 0;

            ov = g_town_overlay_top;
            nx = (ov < 0) ? -1 : (ov >> 4);
            if (end_x <= nx) {
                end_x = static_cast<short>((ov < 0) ? -1 : (ov >> 4));
            }
            if (end_x >= tile_count_x) end_x = tile_count_x;

            ov = g_town_overlay_left;
            nx = (ov < 0) ? -1 : (ov >> 4);
            if (start_y >= nx) {
                start_y = static_cast<short>((ov < 0) ? 0xFFFF : (ov >> 4));
            }
            if (start_y < 1) start_y = 0;

            ov = g_town_overlay_right;
            nx = (ov < 0) ? -1 : (ov >> 4);
            if (end_y <= nx) {
                end_y = static_cast<short>((ov < 0) ? -1 : (ov >> 4));
            }
            if (end_y >= tile_count_y) end_y = tile_count_y;
        }
    }

    /* Scan occupancy bitmap for dirty tiles, building one rect node per
     * horizontal run of consecutive dirty tiles in a row. */
    for (short y = start_y; y < end_y; y++) {
        for (short x = start_x; x < end_x; x++) {
            uint32_t bit_idx = static_cast<uint32_t>(g_player_id) * static_cast<uint32_t>(y) +
                               static_cast<uint32_t>(x);
            uint8_t* bitmap = reinterpret_cast<uint8_t*>(g_tile_occupied_bitmap);
            if ((ATTR_0047f108[bit_idx & 7] & bitmap[bit_idx >> 3]) != 0) {
                /* Dirty tile found — extend pending rect or start one */
                RECT tile_rect;
                tile_rect.left = x * 16;
                tile_rect.top = y * 16;
                tile_rect.right = tile_rect.left + 16;
                tile_rect.bottom = tile_rect.top + 16;

                if (no_pending) {
                    pending = tile_rect;
                    no_pending = false;
                } else {
                    UnionRect(&pending, &tile_rect, &pending);
                }
            } else if (!no_pending) {
                /* Flush pending rect */
                RECT* node = TileMap_AllocRectNode();
                if (node != NULL) {
                    *node = pending;
                    if (last != NULL) {
                        last[1].left = static_cast<LONG>(reinterpret_cast<intptr_t>(node));
                    } else {
                        head = node;
                    }
                    last = node;
                }
                no_pending = true;
            }
        }
        /* Flush pending rect at end of row */
        if (!no_pending) {
            RECT* node = TileMap_AllocRectNode();
            if (node != NULL) {
                *node = pending;
                if (last != NULL) {
                    last[1].left = static_cast<LONG>(reinterpret_cast<intptr_t>(node));
                } else {
                    head = node;
                }
                last = node;
            }
            no_pending = true;
        }
    }

    /* Blit cursor surface to primary surface for each dirty region.
     * Note the binary passes the rect's right/bottom directly as the
     * width/height arguments. */
    for (RECT* r = head; r != NULL; r = reinterpret_cast<RECT*>(
             static_cast<uintptr_t>(r[1].left))) {
        void* cursor_pixels = *reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(g_cursor_surface) + 0x10);
        UIPANEL_Blit(cursor_pixels, r->left, r->top, r->right, r->bottom,
                     g_primary_surface, r->left, r->top, r->right, r->bottom, 1);
    }

    /* Append selection rects (selected building + town selection rect) */
    if (g_has_selection != 0) {
        RECT* prev = last;
        if (g_selected_building != 0) {
            RECT* node = TileMap_AllocRectNode();
            if (node != NULL) {
                uint8_t* sel = reinterpret_cast<uint8_t*>(
                    static_cast<uintptr_t>(g_selected_building));
                node->left   = *reinterpret_cast<int*>(sel + 8);
                node->top    = *reinterpret_cast<int*>(sel + 0xC);
                node->right  = *reinterpret_cast<int*>(sel + 0x10);
                node->bottom = *reinterpret_cast<int*>(sel + 0x14);
                if (prev != NULL) {
                    prev[1].left = static_cast<LONG>(reinterpret_cast<intptr_t>(node));
                } else {
                    head = node;
                }
                prev = node;
            }
        }
        RECT* node = TileMap_AllocRectNode();
        if (node != NULL) {
            node->left   = g_town_selection_rect_left;
            node->top    = g_town_selection_rect_top;
            node->right  = g_town_selection_rect_right;
            node->bottom = g_town_selection_rect_bottom;
            if (prev != NULL) {
                prev[1].left = static_cast<LONG>(reinterpret_cast<intptr_t>(node));
            } else {
                head = node;
            }
            prev = node;
        }
        (void)prev;
    }

    /* Merge overlapping dirty rects until no merge occurs */
    if (head != NULL) {
        while (TileMap_ProcessDirtyRects(head) != 0) {
        }
        TileMap_FreeDirtyRects(head);
    }

    /* Present each rect */
    while (head != NULL) {
        /* Lock primary surface if not already locked */
        if (surface_locked == 0) {
            for (int i = 0; i < 0x1F; i++) {
                reinterpret_cast<uint32_t*>(ddsurfacedesc_buf)[i] = 0;
            }
            reinterpret_cast<uint32_t*>(ddsurfacedesc_buf)[0] = 0x7C;
            if (TileMap_LockPrimarySurface(ddsurfacedesc_buf) == 0) {
                surface_locked = 1;
            }
        }

        ProcessRect(head->left, head->top, head->right, head->bottom);

        /* Unlock primary surface */
        if (surface_locked != 0 && TileMap_UnlockPrimarySurface() == 0) {
            surface_locked = 0;
        }

        HWND child_wnd = *reinterpret_cast<HWND*>(
            reinterpret_cast<uint8_t*>(g_main_window) + 8);
        DDRAW_PresentRect(head, child_wnd, &viewport_x, 1);

        RECT* next = reinterpret_cast<RECT*>(static_cast<uintptr_t>(head[1].left));
        GLOBAL_free(head);
        head = next;
    }

    /* Clear the whole occupancy bitmap (all dirty bits consumed) */
    if (occupancy_bitmap != NULL) {
        int tile_count = static_cast<int>(tile_count_x) * static_cast<int>(tile_count_y);
        int bitmap_size = ((tile_count + (tile_count >> 31 & 7)) >> 3) + 1;

        uint32_t* bitmap32 = reinterpret_cast<uint32_t*>(occupancy_bitmap);
        uint32_t dword_count = bitmap_size >> 2;
        for (uint32_t i = 0; i < dword_count; i++) {
            bitmap32[i] = 0;
        }
        uint8_t* bitmap8 = reinterpret_cast<uint8_t*>(&bitmap32[dword_count]);
        for (uint32_t i = 0; i < (bitmap_size & 3); i++) {
            bitmap8[i] = 0;
        }
    }

    World_Unlock(g_world);  /* 0x4A98B0 */
}

/* ================================================================== */
/* TileMap::ProcessRect                                                */
/* Address: 0x456700                                                   */
/*                                                                     */
/* Renders all objects in a dirty rect. Iterates visible tiles, calls  */
/* Draw / DrawConnected on each object. In town mode (mode 3) also     */
/* dispatches BuildingMgr, World_InvalidateRect, tooltips, cursors and */
/* the town overlay lock/unlock dance.                                 */
/* ================================================================== */
void TileMap::ProcessRect(int left, int top, int right, int bottom)
{
    short tile_left   = (left < 0)   ? -1 : static_cast<short>(left >> 4);
    short tile_top    = (top < 0)    ? -1 : static_cast<short>(top >> 4);
    short tile_right  = (right < 0)  ? -1 : static_cast<short>(right >> 4);
    short tile_bottom = (bottom < 0) ? -1 : static_cast<short>(bottom >> 4);

    if (tile_top < tile_bottom) {
        int tile_idx = static_cast<int>(tile_top) + static_cast<int>(tile_left) * 0x41;
        int pixel_y = tile_top << 4;

        for (short y = tile_top; y < tile_bottom; y++) {
            if (tile_left < tile_right) {
                int pixel_x = tile_left << 4;
                int cur_tile_idx = tile_idx;

                for (short x = tile_left; x < tile_right; x++) {
                    uint32_t bit_idx = static_cast<uint32_t>(g_player_id) * static_cast<uint32_t>(y) +
                                       static_cast<uint32_t>(x);
                    uint8_t* bitmap = reinterpret_cast<uint8_t*>(g_tile_occupied_bitmap);
                    if ((ATTR_0047f108[bit_idx & 7] & bitmap[bit_idx >> 3]) != 0) {
                        /* Read active layer count from tile entry */
                        int8_t active = static_cast<int8_t>(
                            tile_data[(cur_tile_idx + 2) * 0x40 - 0x48]);
                        if (active < 3) active = 2;

                        /* Draw objects in this tile (layers 0..active) */
                        for (int8_t layer = 0; layer <= active; layer++) {
                            void* obj = NULL;
                            if (x < 0 || x > 0x51 ||
                                y < 0 || y > 0x41) {
                                obj = NULL;
                            } else {
                                /* Objects are read from the origin region
                                 * (this + 0x64 + tile*0x40 + layer*4). */
                                obj = ReadTilePointer(
                                    cur_tile_idx * 0x40 + layer * 4 + 0x1C);
                            }
                            if (obj != NULL) {
                                /* vtable[11] = Draw */
                                static_cast<Entity*>(obj)->Draw(
                                    RECT{pixel_x, pixel_y,
                                         pixel_x + 16, pixel_y + 16}, 0, 0);
                            }

                            /* Town-mode (mode 3) layer dispatch */
                            if (g_game_mode == 3) {
                                if (layer == 0) {
                                    unsigned int res_type = 0;
                                    TileMapObject* tobj = static_cast<TileMapObject*>(obj);
                                    if (obj != NULL) {
                                        res_type = GetResourceType(static_cast<unsigned int>(
                                            *reinterpret_cast<int*>(
                                                reinterpret_cast<uint8_t*>(tobj->resource) + 4)));
                                    }
                                    if (static_cast<char>(res_type) != 3) {
                                        /* obj == NULL or non-type-3: BuildingMgr
                                         * dispatch first, then world invalidation. */
                                        BuildingMgr_DispatchAll(g_building_mgr, layer,
                                                                pixel_x, pixel_y,
                                                                pixel_x + 16, pixel_y + 16, 0);
                                        World_InvalidateRect(g_world, pixel_x, pixel_y,
                                                             pixel_x + 16, pixel_y + 16, layer);
                                    } else {
                                        /* obj != NULL && type 3: world invalidation
                                         * first, then BuildingMgr dispatch. */
                                        World_InvalidateRect(g_world, pixel_x, pixel_y,
                                                             pixel_x + 16, pixel_y + 16, layer);
                                        BuildingMgr_DispatchAll(g_building_mgr, layer,
                                                                pixel_x, pixel_y,
                                                                pixel_x + 16, pixel_y + 16, 0);
                                    }
                                } else if (layer == 1) {
                                    World_InvalidateRect(g_world, pixel_x, pixel_y,
                                                         pixel_x + 16, pixel_y + 16, layer);
                                    UI_SetTooltipPos(g_tooltip_mgr, pixel_x, pixel_y,
                                                     pixel_x + 16, pixel_y + 16, 1);
                                }
                            }

                            /* vtable[12] = DrawConnected when the object's
                             * frame data marks it as connected. */
                            if (obj != NULL) {
                                TileMapObject* tobj = static_cast<TileMapObject*>(obj);
                                uint8_t* frame_table = *reinterpret_cast<uint8_t**>(
                                    reinterpret_cast<uint8_t*>(tobj->resource) + 0x20);
                                int anim_index =
                                    static_cast<Entity*>(obj)->anim_index;
                                if (frame_table[anim_index * 3 * 8 + 0x17] == 1) {
                                    static_cast<Entity*>(obj)->DrawConnected(
                                        RECT{pixel_x, pixel_y,
                                             pixel_x + 16, pixel_y + 16}, 0, 0);
                                }
                            }
                        }

                        /* Per-tile UI/scripted dispatch */
                        UI_SetTooltipText(g_tooltip_mgr, pixel_x, pixel_y,
                                          pixel_x + 16, pixel_y + 16);
                        UI_UpdateTooltip(g_tooltip_mgr, pixel_x, pixel_y,
                                         pixel_x + 16, pixel_y + 16, 1);
                        RESDATA_ScriptedObject_Dispatch(g_scripted_object, pixel_x, pixel_y,
                                                        pixel_x + 16, pixel_y + 16, 1);
                        DDRAW_DispatchToSubObjects(g_ddraw_building, pixel_x, pixel_y,
                                                   pixel_x + 16, pixel_y + 16,
                                                   reinterpret_cast<void*>(static_cast<uintptr_t>(1)));
                        Town_RenderSelection(g_town_view);
                        if (g_allow_building_placement == 0) {
                            Game_SetCursorByResourceId(g_game, pixel_x, pixel_y,
                                                       pixel_x + 16, pixel_y + 16, 1);
                        }
                    }
                    cur_tile_idx += 0x41;
                    pixel_x += 16;
                }
            }
            tile_idx++;
            pixel_y += 16;
        }
    }

    /* Town overlay surface lock / unlock around the town selection
     * rendering. */
    RECT local_10;
    RECT param_rect;
    param_rect.left = left;
    param_rect.top = top;
    param_rect.right = right;
    param_rect.bottom = bottom;

    /* Binary view: the town overlay RECT spans the contiguous globals at
     * 0x48538C..0x485398. Build an explicit RECT so the host globals do not
     * need to be contiguous. */
    RECT town_overlay = { g_town_overlay_rect, g_town_overlay_left,
                          g_town_overlay_top, g_town_overlay_right };
    if (IntersectRect(&local_10, &town_overlay, &param_rect)) {
        if (g_allow_building_placement != 0) {
            if (surface_locked != 0 &&
                TileMap_UnlockPrimarySurface() == 0) {
                surface_locked = 0;
            }
            Town_DeselectBuilding(g_town_view);
            Town_UpdateSelection(g_town_view);
            if (surface_locked == 0) {
                for (int i = 0; i < 0x1F; i++) {
                    reinterpret_cast<uint32_t*>(ddsurfacedesc_buf)[i] = 0;
                }
                reinterpret_cast<uint32_t*>(ddsurfacedesc_buf)[0] = 0x7C;
                if (TileMap_LockPrimarySurface(ddsurfacedesc_buf) == 0) {
                    surface_locked = 1;
                }
            }
        }
    }

    if (g_allow_building_placement != 0) {
        RECT town_selection = { g_town_selection_rect_left,
                               g_town_selection_rect_top,
                               g_town_selection_rect_right,
                               g_town_selection_rect_bottom };
        bool intersects = IntersectRect(&local_10, &town_selection, &param_rect);
        if (!intersects) {
            RECT sel_rect;
            if (g_selected_building != 0) {
                sel_rect = *reinterpret_cast<RECT*>(
                    reinterpret_cast<uint8_t*>(
                        static_cast<uintptr_t>(g_selected_building)) + 8);
                intersects = IntersectRect(&local_10, &sel_rect, &param_rect);
            }
        }
        if (intersects) {
            Game_ResetCursor(g_game);
        }
    }
}

/* ================================================================== */
/* TileMap::ProcessObjectTimer                                         */
/* Address: 0x456D90                                                   */
/*                                                                     */
/* Validates that the object's tile footprint matches the expected     */
/* resource-id layout (resource +0x564, count at +0x560). The          */
/* footprint is walked in a clockwise spiral starting from the top     */
/* row (y = world_y - 1). Returns the validity flag (low byte).        */
/* ================================================================== */
uint TileMap::ProcessObjectTimer(TileMapObject* obj)
{
    if (obj == NULL) {
        return 0;
    }

    TileMapResource* res = obj->resource;
    int packed_world;
    GameObject_GetSubObjectWorldPos(obj, &packed_world);

    short world_y = static_cast<short>(
        static_cast<unsigned int>(packed_world) >> 16);
    short world_x = static_cast<short>(packed_world);
    int orig_world_x = static_cast<int>(world_x);
    short row_y = static_cast<short>(world_y - 1);

    int x_end = static_cast<short>(
        static_cast<unsigned short>(res->grid_span_y) +
        static_cast<unsigned short>(obj->tile_x) - 1) + 1;

    uint idx = 0;
    char valid = 1;

    /* Expected-id value at this index; -1 = no check for this tile. */
    /* ---- Loop 1: right along the top row (y = world_y - 1) ---- */
    int cur_x = static_cast<int>(world_x);
    while (cur_x <= x_end) {
        if (idx >= res->expected_count || valid != 1) break;
        int expected = res->expected_ids[idx];
        if (expected != -1) {
            if (world_x < 0 || g_player_id <= world_x ||
                row_y < 0 || g_player_color <= row_y) {
                valid = 0;
            } else {
                int tile_val = ReadTileValue((cur_x * 0x41 + row_y) * 0x40);
                if (tile_val == 0) {
                    if (expected != 0) valid = 0;
                } else {
                    int tile_res = *reinterpret_cast<int*>(
                        reinterpret_cast<uint8_t*>(
                            static_cast<uintptr_t>(tile_val)) + 0x40);
                    if (expected != *reinterpret_cast<int*>(tile_res + 4)) {
                        valid = 0;
                    }
                }
            }
        }
        idx++;
        world_x++;
        cur_x = static_cast<int>(world_x);
    }

    /* ---- Loop 2: down the right column (x = world_x - 1) ---- */
    short right_x = static_cast<short>(world_x - 1);
    short col_y = world_y;
    int y_end_2 = static_cast<short>(
        static_cast<unsigned short>(res->original_span) +
        static_cast<unsigned short>(obj->tile_y) - 1) + 1;
    while (static_cast<int>(col_y) <= y_end_2) {
        if (idx >= res->expected_count || valid != 1) break;
        int expected = res->expected_ids[idx];
        if (expected != -1) {
            if (right_x < 0 || g_player_id <= right_x ||
                col_y < 0 || g_player_color <= col_y) {
                valid = 0;
            } else {
                int tile_val = ReadTileValue((col_y + right_x * 0x41) * 0x40);
                if (tile_val == 0) {
                    if (expected != 0) valid = 0;
                } else {
                    int tile_res = *reinterpret_cast<int*>(
                        reinterpret_cast<uint8_t*>(
                            static_cast<uintptr_t>(tile_val)) + 0x40);
                    if (expected != *reinterpret_cast<int*>(tile_res + 4)) {
                        valid = 0;
                    }
                }
            }
        }
        idx++;
        col_y++;
    }

    /* ---- Loop 3: left along the bottom row ---- */
    col_y = static_cast<short>(col_y - 1);
    int x3 = static_cast<int>(world_x) - 2;
    while (orig_world_x - 1 <= x3) {
        if (idx >= res->expected_count || valid != 1) break;
        int expected = res->expected_ids[idx];
        if (expected != -1) {
            if (x3 < 0 || g_player_id <= x3 ||
                col_y < 0 || g_player_color <= col_y) {
                valid = 0;
            } else {
                int tile_val = ReadTileValue((x3 * 0x41 + col_y) * 0x40);
                if (tile_val == 0) {
                    if (expected != 0) valid = 0;
                } else {
                    int tile_res = *reinterpret_cast<int*>(
                        reinterpret_cast<uint8_t*>(
                            static_cast<uintptr_t>(tile_val)) + 0x40);
                    if (expected != *reinterpret_cast<int*>(tile_res + 4)) {
                        valid = 0;
                    }
                }
            }
        }
        idx++;
        x3--;
    }

    /* ---- Loop 4: up the left column ---- */
    x3++;
    while (true) {
        if (static_cast<int>(col_y) < static_cast<int>(world_y) - 1 ||
            idx >= res->expected_count || valid != 1) {
            return valid;
        }
        int expected = res->expected_ids[idx];
        if (expected != -1) {
            if (x3 < 0 || g_player_id <= x3 ||
                col_y < 0 || g_player_color <= col_y) {
                valid = 0;
            } else {
                int tile_val = ReadTileValue((col_y + x3 * 0x41) * 0x40);
                if (tile_val == 0) {
                    if (expected != 0) valid = 0;
                } else {
                    int tile_res = *reinterpret_cast<int*>(
                        reinterpret_cast<uint8_t*>(
                            static_cast<uintptr_t>(tile_val)) + 0x40);
                    if (expected != *reinterpret_cast<int*>(tile_res + 4)) {
                        valid = 0;
                    }
                }
            }
        }
        idx++;
        col_y--;
    }
}

/* ================================================================== */
/* TileMap::UpdateAll                                                  */
/* Address: 0x457320                                                   */
/*                                                                     */
/* Waits for async tile asset loading thread to finish, then loads and */
/* enumerates asset files into the tile grid.                          */
/* ================================================================== */
void TileMap::UpdateAll()
{
    update_complete = 0;

    /* Wait for async thread result */
    int thread_result = WIN32_GetThreadResult(0x4A9AD0);
    while (thread_result != 0) {
        Sleep(50);
        thread_result = WIN32_GetThreadResult(0x4A9AD0);
    }

    /* Load and enumerate asset files */
    if (asset_load_ptr != NULL) {
        AssetMgr_LoadFileEx(reinterpret_cast<uint*>(asset_load_ptr));
    }
    if (asset_enum_ptr != NULL) {
        AssetMgr_EnumFiles(reinterpret_cast<uint*>(asset_enum_ptr));
    }
}

/* ================================================================== */
/* TileMap::FindObject                                                 */
/* Address: 0x4550C0                                                   */
/*                                                                     */
/* Validates and places building at tile position. Calls ScrollRect    */
/* for validation, INPUT_PlaceObject to create, fills tile grid with   */
/* new object pointers in all occupied cells (occupancy grid indexed   */
/* [x][y][z] = x*63+y*7+z), then writes the object's span map (at      */
/* resource +0x4A1) into the origin region, sets tile_x/tile_y and     */
/* calls vtable[3] (SetWorldPos) with pixel coordinates.               */
/* ================================================================== */
int* TileMap::FindObject(unsigned int target_resource_id, short tile_x, short tile_y,
                          char unknown, unsigned int mode)
{
    int* result = NULL;

    if (tile_x < 0 || tile_x > tile_count_x ||
        tile_y < 0 || tile_y > tile_count_y) {
        return NULL;
    }

    void* res_data = ResourceManager_GetById(&g_resmgr,
                                             static_cast<UINT>(target_resource_id));
    if (res_data == NULL) {
        return NULL;
    }

    TileMapResource* resource = reinterpret_cast<TileMapResource*>(res_data);
    unsigned short orig_span = static_cast<unsigned short>(resource->original_span);
    byte span_y = resource->grid_span_y;
    int offset = static_cast<unsigned int>(orig_span) -
                 static_cast<unsigned int>(resource->grid_height);

    short adjusted_y = tile_y;
    if (unknown != 1) {
        adjusted_y = tile_y - static_cast<short>(offset);
    }

    if (adjusted_y < 0) {
        return NULL;
    }

    if (g_allow_building_placement == 0 ||
        ScrollRect(0, reinterpret_cast<TileMapObject*>(res_data), tile_x,
                   static_cast<unsigned short>(adjusted_y + static_cast<short>(offset)),
                   static_cast<int>(mode)) != 0) {
        if (ScrollRect(1, reinterpret_cast<TileMapObject*>(res_data), tile_x,
                       static_cast<unsigned short>(offset + static_cast<int>(adjusted_y)),
                       static_cast<int>(mode)) != 0) {
            result = reinterpret_cast<int*>(
                INPUT_PlaceObject(&g_input_mgr, target_resource_id));
            if (result != NULL) {
                /* Fill the standard layer region of each occupied tile */
                short gy = 0;
                int y_start = offset + static_cast<int>(adjusted_y);
                if (resource->grid_height != 0) {
                    do {
                        short gx = 0;
                        unsigned int x_start = static_cast<unsigned int>(tile_x);
                        if (resource->grid_width != 0) {
                            do {
                                /* Occupancy grid indexed [x][y][z] */
                                for (int8_t iz = 0; iz < resource->grid_depth; iz++) {
                                    if (reinterpret_cast<TileMapObject*>(result)
                                            ->occupancy_grid[
                                                static_cast<int>(gx) * 9 * 7 +
                                                static_cast<int>(gy) * 7 +
                                                static_cast<int>(iz)] == 1) {
                                        int tile_index =
                                            (y_start + static_cast<int>(gy)) +
                                            (static_cast<int>(x_start) +
                                             static_cast<int>(gx)) * 0x41;
                                        /* standard layer slot at +0x48 */
                                        WriteTileValue(
                                            tile_index * 0x40 + static_cast<int>(iz) * 4,
                                            static_cast<int32_t>(
                                                reinterpret_cast<intptr_t>(result)));
                                        /* active byte = max(active, iz) */
                                        uint8_t& active = tile_data[
                                            (tile_index + 2) * 0x40 - 0x48];
                                        if (active <= static_cast<uint8_t>(iz)) {
                                            active = static_cast<uint8_t>(iz);
                                        }
                                    }
                                }
                                gx++;
                            } while (gx < resource->grid_width);
                        }
                        gy++;
                        y_start++;
                    } while (gy < resource->grid_height);
                }

                /* Write the span map (resource +0x4A1, stride 9) into the
                 * origin region (this + tile*0x40 + 0x64 + (span-1)*4) and
                 * mark the occupied-bitmap bits. */
                if (orig_span != 0) {
                    int y = static_cast<int>(adjusted_y);
                    int x = static_cast<int>(tile_x);
                    for (int sy = 0; sy < static_cast<int>(orig_span); sy++, y++) {
                        uint8_t* span_row = reinterpret_cast<uint8_t*>(res_data) +
                                            0x4A1 + sy;
                        if (span_y != 0) {
                            int sx = 0;
                            for (; sx < static_cast<int>(span_y); sx++) {
                                uint8_t span = span_row[sx * 9];
                                if (span != 0) {
                                    int tile_index = y + (x + sx) * 0x41;
                                    int span_slot = static_cast<int>(span) - 1;
                                    WriteTileValue(
                                        tile_index * 0x40 + span_slot * 4 + 0x1C,
                                        static_cast<int32_t>(
                                            reinterpret_cast<intptr_t>(result)));
                                    /* active byte = max(active, span-1) */
                                    uint8_t& active = tile_data[
                                        (tile_index + 2) * 0x40 - 0x48];
                                    if (active <= static_cast<uint8_t>(span_slot)) {
                                        active = static_cast<uint8_t>(span_slot);
                                    }
                                    /* occupancy dirty bit */
                                    uint32_t bit_idx =
                                        static_cast<uint32_t>(g_player_id) *
                                            static_cast<uint32_t>(y) +
                                        static_cast<uint32_t>(x + sx);
                                    uint8_t* bitmap_byte =
                                        reinterpret_cast<uint8_t*>(
                                            g_tile_occupied_bitmap) + (bit_idx >> 3);
                                    *bitmap_byte |= ATTR_0047f108[bit_idx & 7];
                                }
                            }
                        }
                    }
                }

                /* Record tile origin and move the object to pixel coords
                 * via vtable[3] (SetWorldPos / MoveTo). */
                TileMapObject* obj = reinterpret_cast<TileMapObject*>(result);
                obj->tile_y = adjusted_y;
                obj->tile_x = tile_x;
                reinterpret_cast<Entity*>(obj)->MoveTo(
                    static_cast<int>(tile_x) << 4,
                    static_cast<int>(adjusted_y) << 4);
            }
        }
    }

    return result;
}

/* ================================================================== */
/* TileMap::Scroll                                                     */
/* Address: 0x455960                                                   */
/*                                                                     */
/* Vector-based scroll: steps from (delta_x, delta_y) along the unit   */
/* vector toward the drag start for sqrt(dx^2+dy^2) steps, calling     */
/* GetObjectAtEx at each step and ScrollTo on found objects. Returns   */
/* 1 if any object was scrolled.                                       */
/* ================================================================== */
uint TileMap::Scroll(int delta_x, int delta_y, int drag_start_x, int drag_start_y)
{
    int vec_x = delta_x - drag_start_x;
    int vec_y = delta_y - drag_start_y;

    double length = std::sqrt(static_cast<double>(vec_x) * static_cast<double>(vec_x) +
                              static_cast<double>(vec_y) * static_cast<double>(vec_y));
    double norm_x = static_cast<double>(vec_x) / length;
    double norm_y = static_cast<double>(vec_y) / length;

    double cur_x = static_cast<double>(delta_x);
    double cur_y = static_cast<double>(delta_y);

    uint result = 0;
    /* The binary loops while length > 0 (decremented by 1 each step). */
    while (length > 0.0) {
        cur_x = cur_x + norm_x;
        cur_y = cur_y + norm_y;

        /* __ftol truncates toward zero */
        long xl = static_cast<long>(cur_x);
        short tile_x = (xl < 0) ? static_cast<short>(-1) : static_cast<short>(xl >> 4);
        long yl = static_cast<long>(cur_y);
        short tile_y = (yl < 0) ? static_cast<short>(-1) : static_cast<short>(yl >> 4);

        short layer_out;
        void* obj = GetObjectAtEx(tile_x, tile_y, &layer_out);
        if (obj != NULL) {
            ScrollTo(reinterpret_cast<TileMapObject*>(obj), 1);
            result = 1;
        }
        length = length - 1.0;
    }

    return result;
}

/* ================================================================== */
/* TileMap_ProcessDirtyRects                                           */
/* Address: 0x456C60                                                   */
/*                                                                     */
/* Merges overlapping dirty rects in the linked list. Multi-pass:      */
/* inflates each rect by 1, checks intersection via IntersectRect,     */
/* merges via UnionRect, frees the merged-away rect. Returns 1 if any  */
/* merge occurred (caller loops until 0).                              */
/* ================================================================== */
char TileMap_ProcessDirtyRects(RECT* rect_list)
{
    char merged = 0;
    for (RECT* r = rect_list; r != NULL;
         r = reinterpret_cast<RECT*>(static_cast<uintptr_t>(r[1].left))) {
        RECT* next = reinterpret_cast<RECT*>(static_cast<uintptr_t>(r[1].left));
        RECT* prev = r;
        while (next != NULL) {
            RECT tmp;
            InflateRect(next, 1, 1);
            if (IntersectRect(&tmp, r, next)) {
                InflateRect(next, -1, -1);
                UnionRect(&tmp, next, r);
                merged = 1;
                r->left = tmp.left;
                r->top = tmp.top;
                r->right = tmp.right;
                r->bottom = tmp.bottom;
                prev[1].left = next[1].left;
                GLOBAL_free(next);
                next = prev;
            } else {
                InflateRect(next, -1, -1);
            }
            prev = next;
            next = reinterpret_cast<RECT*>(static_cast<uintptr_t>(next[1].left));
        }
    }
    return merged;
}

/* ================================================================== */
/* TileMap_FreeDirtyRects                                              */
/* Address: 0x456D10                                                   */
/*                                                                     */
/* Clips each dirty rect to the viewport rect (g_viewport_rect_left).  */
/* Rects that do not intersect the viewport are unlinked and freed     */
/* with GLOBAL_free; intersecting rects are clipped in place.          */
/* ================================================================== */
void TileMap_FreeDirtyRects(RECT* rect_list)
{
    RECT* prev = NULL;

    while (rect_list != NULL) {
        RECT local_10;
        /* Binary view: the viewport RECT spans the contiguous globals at
         * 0x4AAD14..0x4AAD20 (TileMap.viewport_rect). Build an explicit
         * RECT so the host globals do not need to be contiguous. */
        RECT viewport = { g_viewport_rect_left, g_viewport_rect_top,
                          g_viewport_rect_right, g_viewport_rect_bottom };
        if (IntersectRect(&local_10, rect_list, &viewport)) {
            rect_list->left = local_10.left;
            rect_list->top = local_10.top;
            rect_list->right = local_10.right;
            rect_list->bottom = local_10.bottom;
            prev = rect_list;
            rect_list = reinterpret_cast<RECT*>(
                static_cast<uintptr_t>(rect_list[1].left));
        } else {
            OutputDebugStringA("Invalid Rect found in world draw chain");
            if (prev != NULL) {
                prev[1].left = rect_list[1].left;
            }
            RECT* next = reinterpret_cast<RECT*>(
                static_cast<uintptr_t>(rect_list[1].left));
            GLOBAL_free(rect_list);
            rect_list = next;
        }
    }
}

/* ================================================================== */
/* TileMap::CreateOverlay                                              */
/* Address: 0x457080                                                   */
/*                                                                     */
/* Creates the build-placement preview overlay: initializes the target */
/* UIPANEL surface at the tilemap resolution, then fills its pixel     */
/* buffer with a per-tile overlay code derived from the tile content:  */
/*   2=water -> 3, 4 -> 2, 0x0C scenery -> 7, 0x0D road -> 6,          */
/*   type 3 buildings -> 5 except the four station-corner pieces       */
/*   (0xC1E/0xC20/0xC22/0xC24) which leave their own corner tile       */
/*   unwritten, 5..0xB -> untouched. Returns 1 (low byte).             */
/* ================================================================== */
void* TileMap::CreateOverlay(void* surface, byte fill_byte)
{
    UIPANEL_InitSurface(surface, tile_count_x, tile_count_y, 0, 0, fill_byte);

    /* Surface pixel buffer at +0x18 */
    uint8_t* pixel = *reinterpret_cast<uint8_t**>(
        reinterpret_cast<uint8_t*>(surface) + 0x18);

    for (int y = 0; y < tile_count_y; y++) {
        for (int x = 0; x < tile_count_x; x++) {
            int tile_val = ReadTileValue((x * 0x41 + y) * 0x40);
            if (tile_val == 0) {
                pixel++;
                continue;
            }

            TileMapObject* obj;
            if (x < 0 || x > 0x51 || y < 0 || y > 0x41) {
                obj = NULL;
            } else {
                obj = static_cast<TileMapObject*>(
                    ReadTilePointer((x * 0x41 + y) * 0x40));
            }

            /* The binary re-reads the slot with a bounds check and, for the
             * (unreachable) out-of-range case, dereferences a null object;
             * the null-guard here is the safe host equivalent. */
            TileMapResource* res = (obj == NULL) ? NULL : obj->resource;
            uint8_t type = (res == NULL) ? 0 : res->object_type;

            switch (type) {
            case 2:                              /* water -> 3 */
                *pixel = 3;
                break;
            case 4:                              /* -> 2 */
                *pixel = 2;
                break;
            case 0x0C:                           /* scenery -> 7 */
                *pixel = 7;
                break;
            case 0x0D:                           /* road -> 6 */
                *pixel = 6;
                break;
            case 3: {                            /* building / station pieces */
                int res_id = (res == NULL) ? -1 : res->resource_id;
                int delta = res_id - 0xC1E;
                if (delta < 0 || delta > 6) {
                    *pixel = 5;
                    break;
                }
                /* The four station corner pieces leave their own corner
                 * tile unwritten and mark the rest of the footprint 5. */
                bool skip = false;
                switch (delta) {
                case 0: /* 0xC1E */
                    skip = (obj->tile_x == x && obj->tile_y == y);
                    break;
                case 2: /* 0xC20 */
                    skip = (obj->tile_x + 2 == x && obj->tile_y == y);
                    break;
                case 4: /* 0xC22 */
                    skip = (obj->tile_x == x && obj->tile_y + 2 == y);
                    break;
                case 6: /* 0xC24 */
                    skip = (obj->tile_x + 2 == x && obj->tile_y + 2 == y);
                    break;
                default: /* 0xC1F/0xC21/0xC23 -> default overlay 5 */
                    skip = false;
                    break;
                }
                if (!skip) {
                    *pixel = 5;
                }
                break;
            }
            default:                             /* types 5..0xB: no write */
                break;
            }
            pixel++;
        }
    }

    /* Return value: EAX low byte = 1 (upper bytes are loop residue). */
    return reinterpret_cast<void*>(static_cast<uintptr_t>(1));
}

/* ================================================================== */
/* TileMap_CreateOverlay — free-function entry used by network/Netman. */
/* Address: 0x457080 (__thiscall in the binary; this wrapper keeps the */
/* free-function symbol network/Netman.h declares).                    */
/* ================================================================== */
void TileMap_CreateOverlay(void* tilemap, void* surface, int32_t flags)
{
    static_cast<TileMap*>(tilemap)->CreateOverlay(surface, static_cast<byte>(flags));
}

/* ================================================================== */
/* TileMap::FindNearestObject                                          */
/* Address: 0x457CE0                                                   */
/*                                                                     */
/* Spatial search in concentric square rings. Each ring scans the      */
/* perimeter in four sweeps (top edge, right edge, bottom edge, left   */
/* edge), returning the first object matching type_filter (resource    */
/* type byte at +0x08) found at the smallest ring, using               */
/* Math_DistSquared for tie-breaking within the ring.                  */
/* ================================================================== */
void* TileMap::FindNearestObject(unsigned short type_filter,
                                 int target_x, int target_y, int search_radius)
{
    int best_obj = 0;
    int best_dist_sq = 999999999;

    short radius_tiles = (search_radius < 0) ? -1 : static_cast<short>(search_radius >> 4);
    short center_x = (target_x < 0) ? -1 : static_cast<short>(target_x >> 4);
    short center_y = (target_y < 0) ? -1 : static_cast<short>(target_y >> 4);

    for (short ring = 0; ring <= radius_tiles; ring++) {
        if (best_obj != 0) {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(best_obj));
        }

        int r = static_cast<int>(ring);
        int left = static_cast<int>(center_x) - r;
        int top = static_cast<int>(center_y) - r;
        int right = static_cast<int>(center_x) + r;
        int bottom = static_cast<int>(center_y) + r;

        int x0 = (left < 1) ? 0 : left;          /* max(left, 0) */
        int y0 = (top < 1) ? 0 : top;            /* max(top, 0) */

        /* Sweep 1: top edge (y = top), x from left..right */
        for (int x = x0; x <= right && x < tile_count_x; x++) {
            int tile_val = 0;
            if (x < 0 || x > 0x51 || y0 < 0 || y0 > 0x41) {
                tile_val = 0;
            } else {
                tile_val = ReadTileValue((x * 0x41 + y0) * 0x40);
            }
            if (tile_val != 0) {
                uint8_t* tile_object = reinterpret_cast<uint8_t*>(
                    static_cast<uintptr_t>(tile_val));
                int resource_ptr = *reinterpret_cast<int*>(tile_object + 0x40);
                uint8_t obj_type = 0;
                if (resource_ptr != 0) {
                    obj_type = *reinterpret_cast<uint8_t*>(resource_ptr + 8);
                }
                if (obj_type == static_cast<uint8_t>(type_filter)) {
                    Entity* entity = reinterpret_cast<Entity*>(tile_object);
                    int dist_sq = Math_DistSquared(target_x, target_y,
                                                   entity->world_x,
                                                   entity->world_y);
                    if (dist_sq < best_dist_sq) {
                        best_dist_sq = dist_sq;
                        best_obj = tile_val;
                    }
                }
            }
        }

        /* Sweep 2: right edge (x = right), y from top+1..bottom */
        int rx = right;
        if (rx >= tile_count_x) rx = tile_count_x;
        int y1 = (top + 1 < 1) ? 0 : top + 1;
        for (int y = y1; y <= bottom && y < tile_count_y; y++) {
            int tile_val = 0;
            if (rx < 0 || rx > 0x51 || y < 0 || y > 0x41) {
                tile_val = 0;
            } else {
                tile_val = ReadTileValue((y + rx * 0x41) * 0x40);
            }
            if (tile_val != 0) {
                uint8_t* tile_object = reinterpret_cast<uint8_t*>(
                    static_cast<uintptr_t>(tile_val));
                int resource_ptr = *reinterpret_cast<int*>(tile_object + 0x40);
                uint8_t obj_type = 0;
                if (resource_ptr != 0) {
                    obj_type = *reinterpret_cast<uint8_t*>(resource_ptr + 8);
                }
                if (obj_type == static_cast<uint8_t>(type_filter)) {
                    Entity* entity = reinterpret_cast<Entity*>(tile_object);
                    int dist_sq = Math_DistSquared(target_x, target_y,
                                                   entity->world_x,
                                                   entity->world_y);
                    if (dist_sq < best_dist_sq) {
                        best_dist_sq = dist_sq;
                        best_obj = tile_val;
                    }
                }
            }
        }

        /* Sweep 3: bottom edge (y = bottom), x from right-1 down..left */
        int bx = right - 1;
        if (bx >= tile_count_x) bx = tile_count_x;
        int by = bottom;
        if (by >= tile_count_y) by = tile_count_y;
        for (; left <= bx && bx >= 0; bx--) {
            int tile_val = 0;
            if (bx < 0x52 && by >= 0 && by < 0x42) {
                tile_val = ReadTileValue((bx * 0x41 + by) * 0x40);
            }
            if (tile_val != 0) {
                uint8_t* tile_object = reinterpret_cast<uint8_t*>(
                    static_cast<uintptr_t>(tile_val));
                int resource_ptr = *reinterpret_cast<int*>(tile_object + 0x40);
                uint8_t obj_type = 0;
                if (resource_ptr != 0) {
                    obj_type = *reinterpret_cast<uint8_t*>(resource_ptr + 8);
                }
                if (obj_type == static_cast<uint8_t>(type_filter)) {
                    Entity* entity = reinterpret_cast<Entity*>(tile_object);
                    int dist_sq = Math_DistSquared(target_x, target_y,
                                                   entity->world_x,
                                                   entity->world_y);
                    if (dist_sq < best_dist_sq) {
                        best_dist_sq = dist_sq;
                        best_obj = tile_val;
                    }
                }
            }
        }

        /* Sweep 4: left edge (x = left), y from bottom-1 down..top+1 */
        int lx = (left < 1) ? 0 : left;
        int ly = bottom - 1;
        if (ly >= tile_count_y) ly = tile_count_y;
        for (; top < ly && ly >= 0; ly--) {
            int tile_val = 0;
            if (lx >= 0 && lx <= 0x51 && ly >= 0 && ly <= 0x41) {
                tile_val = ReadTileValue((ly + lx * 0x41) * 0x40);
            }
            if (tile_val != 0) {
                uint8_t* tile_object = reinterpret_cast<uint8_t*>(
                    static_cast<uintptr_t>(tile_val));
                int resource_ptr = *reinterpret_cast<int*>(tile_object + 0x40);
                uint8_t obj_type = 0;
                if (resource_ptr != 0) {
                    obj_type = *reinterpret_cast<uint8_t*>(resource_ptr + 8);
                }
                if (obj_type == static_cast<uint8_t>(type_filter)) {
                    Entity* entity = reinterpret_cast<Entity*>(tile_object);
                    int dist_sq = Math_DistSquared(target_x, target_y,
                                                   entity->world_x,
                                                   entity->world_y);
                    if (dist_sq < best_dist_sq) {
                        best_dist_sq = dist_sq;
                        best_obj = tile_val;
                    }
                }
            }
        }
    }

    return reinterpret_cast<void*>(static_cast<uintptr_t>(best_obj));
}
