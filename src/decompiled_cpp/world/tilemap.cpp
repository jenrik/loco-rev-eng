// Status: TRANSCRIBED
/**
 * tilemap.cpp — TileMap method implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * TileMap manages the game world's tile grid (82x66 tiles, each 64 bytes for
 * 16 layer slots). It tracks scroll state, dirty regions for rendering, tile
 * occupancy/buildability checks, and viewport scrolling.
 */

#include "tilemap.h"
#include <new>

/* ================================================================== */
/* External references — declared in tilemap.h; re-declared here for   */
/* self-contained compilation. Duplicates are intentional for now.     */
/* ================================================================== */

/* Memory */
void*  operator_new(size_t size);
void   GLOBAL_free(void* ptr);

/* Game globals — addresses from binary */
extern int32_t  g_game_mode;             /* 0x004851F4 */
extern int32_t  g_screen_width;          /* 0x004851D8 */
extern int32_t  g_screen_height;         /* 0x00485214 */
extern int32_t  g_client_offset_x;
extern int32_t  g_client_offset_y;
extern int32_t  g_client_width;
extern int32_t  g_client_height;
extern uint8_t  g_is_fullscreen;
extern int32_t  g_world_width;
extern uint8_t  g_is_town_mode;
extern int32_t  g_town_overlay_rect;
extern int32_t  g_town_overlay_left;     /* 0x485390 */
extern int32_t  g_town_overlay_top;      /* 0x485394 */
extern int32_t  g_town_overlay_right;    /* 0x485398 */
extern uint8_t  g_build_mode;
extern uint8_t  g_disable_input;
extern uint8_t  g_lock_update_flag;      /* 0x4851F0 */
extern uint8_t  g_click_on_building;
extern uint8_t  g_placement_valid;
extern uint8_t  g_placement_blocked;
extern int32_t  g_placement_resource_id;
extern void*    g_input_mgr;
extern void*    g_town_view;
extern void*    g_ddraw_building;
extern void*    g_about;
extern void*    g_netman;
extern uint8_t  g_click_on_town;
extern int32_t  g_selected_building;
extern int32_t  g_town_selection_rect_left;
extern int32_t  g_town_selection_rect_top;
extern int32_t  g_town_selection_rect_right;
extern int32_t  g_town_selection_rect_bottom;
extern uint8_t  g_has_selection;
extern int32_t  g_viewport_x;
extern int32_t  g_viewport_rect_left;
extern uint8_t  g_allow_building_placement;
extern int32_t  g_player_id;
extern uint8_t  g_player_color;
extern void*    g_cursor_surface;
extern void*    g_primary_surface;
extern void*    g_tile_occupied_bitmap;
extern void*    g_resmgr;
extern void*    g_scripted_object;
extern void*    g_building_mgr;
extern TileMap  g_tilemap;              /* at 0x4AAD08 */
extern void*    g_game;
extern void*    g_world;              /* 0x4A98B0 */
extern void*    g_tooltip_mgr;        /* 0x4FD220 */
extern HWND     g_main_window;

/* Bitmask lookup table */
extern uint8_t  ATTR_0047f108[8];       /* bitmask lookup (1<<n) */

/* External functions */
extern int      TileData_IsRoadTile(int ptr);
extern int      TileData_GetTileCategory(void* ptr, short a, ushort b);
extern int      TileData_IsSceneryTile(int ptr);
extern int      TileData_IsWaterTile(int ptr);
extern int      TileData_IsTrackTile(int ptr);
extern int      INPUT_EditCharHandler(int ptr);
extern int      INPUT_PlaceObject(void** mgr, uint resource_id);
extern int      INPUT_RemoveObject(void** mgr, void* obj, uint param);
extern void     PlaySoundAt(int sound_id, int x, int y, int channel);
extern int      Town_SelectBuilding(void** town_view, int building);
extern int      DDRAW_SelectBuilding(void** ddraw_building, int building);
extern void     CGWND_SetMode(void* mode);
extern void     Town_RenderSelection(void* town_view);
extern void     Game_SetCursorByResourceId(void* game, int x, int y,
                                            int w, int h, int flag);
extern void     UI_SetTooltipText(void* mgr, int x, int y, int w, int h);
extern void     UI_UpdateTooltip(void* mgr, int x, int y, int w, int h);
extern void     BuildingMgr_DispatchAll(void* mgr, int packed_type,
                                         int x, int y, int w, int h);
extern void     World_InvalidateRect(void* world, int x, int y,
                                      int w, int h, short type);
extern void     ScriptedObject_Dispatch(void* obj, int x, int y,
                                         int w, int h, int flag);
extern void     DDRAW_DispatchToSubObjects(void* ddraw, int x, int y,
                                            int w, int h, void* flag);
extern void     Game_DeselectGameObject(int param);
extern void     World_Init(void* world);
extern void     UI_CleanupTooltips(void* mgr);
extern void     INPUT_FileDlgProc(void* mgr);
extern void*    DDRAW_SpriteDataCtor(void* obj, int type);
extern void     DDRAW_SpriteDataDtor(void* obj);
extern void     Town_DeselectBuilding(int param);
extern void     Town_UpdateSelection(int param);
extern void     Game_ResetCursor(void* game);
extern int      Math_DistSquared(int x1, int y1, int x2, int y2);
extern void*    Entity_GetSubObjectPosition(void* obj, int* out_xy, int direction);
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
/* Tile Index Helper Macro                                             */
/* ================================================================== */
/* Tile data offset: 0x48 + (x * 65 + y) * 64 + layer * 4 */
#define TILE_OFFSET(x, y, layer) \
    (0x48 + ((int)(x) * 65 + (int)(y)) * 0x40 + (int)(layer) * 4)

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
        ((uint32_t*)ddsurfacedesc_buf)[i] = 0;
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
    Game_DeselectGameObject((int)(intptr_t)g_game);  /* 0x4854C8 */
    World_Init(g_world);  /* 0x4A98B0 */
    UI_CleanupTooltips(g_tooltip_mgr);  /* 0x4FD220 */
    INPUT_FileDlgProc(g_input_mgr);  /* 0x4A9990 */

    /* Clear the trailing header bytes and all named tile storage. */
    for (int i = 2; i < 6; ++i) {
        _pad_42[i] = 0;
    }
    for (size_t i = 0; i < sizeof(tile_data); ++i) {
        tile_data[i] = 0;
    }

    /* Fill occupancy bitmap with 0xFF if allocated */
    if (occupancy_bitmap != NULL) {
        int tile_count = (int)tile_count_y * (int)tile_count_x;
        int bitmap_size = ((tile_count + (tile_count >> 31 & 7)) >> 3) + 1;

        uint32_t* bitmap32 = (uint32_t*)occupancy_bitmap;
        uint32_t dword_count = bitmap_size >> 2;
        for (uint32_t i = 0; i < dword_count; i++) {
            bitmap32[i] = 0xFFFFFFFF;
        }

        uint8_t* bitmap8 = reinterpret_cast<uint8_t*>(&bitmap32[dword_count]);
        for (uint32_t i = 0; i < (bitmap_size & 3); i++) {
            bitmap8[i] = 0xFF;
        }
    }

    /* Reset tile grid active-layer bytes (at +0x80 within each tile entry) */
    for (int y = 0; y < 0x41; y++) {
        for (int x = 0; x < 0x51; x++) {
            int tile_base = (x * 0x41 + y) * 0x40;
            tile_data[tile_base + 0x38] = 0xFF;
            tile_data[tile_base + 0x39] = 0xFF;
        }
    }

    /* Invalidate and update game window if not in game mode 1 */
    if (g_main_window != NULL) {
        HWND child_wnd = g_main_window;
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
/* param use_1024x768: 0=use screen dims (clamped 1024-1280),         */
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
                this->total_width = g_screen_width;
                this->total_height = g_screen_height;
                height = g_screen_height;
            } else {
                width = 0x500;      /* 1280 max */
                this->total_width = 0x500;
                this->total_height = 0x400;
                height = 0x400;
            }
            goto set_center;
        }
        width = 0x400;              /* 1024 */
        this->total_width = 0x400;
    } else {
        width = 0x400;              /* 1024 */
        this->total_width = 0x400;
    }
    this->total_height = 0x300;      /* 768 */
    height = 0x300;

set_center:
    scroll_x = 0;
    scroll_y = 0;
    center_x = width / 2;
    total_width = width;
    total_height = height;
    viewport_x = 0;
    viewport_y = 0;
    center_y = height / 2;
    viewport_center_x =
        (g_client_offset_x - g_client_width) / 2 + g_client_width;
    viewport_center_y =
        (g_client_offset_y - g_client_height) / 2 + g_client_height;

    /* Calculate tile counts (round up) */
    tile_count_x = (short)((width + (width >> 31 & 0xF)) >> 4);
    tile_count_y = (short)((height + (height >> 31 & 0xF)) >> 4);

    /* Allocate occupancy bitmap */
    if (occupancy_bitmap) {
        GLOBAL_free(occupancy_bitmap);
        occupancy_bitmap = NULL;
    }

    int tile_count = (int)tile_count_x * (int)tile_count_y;
    int bitmap_size = ((tile_count + (tile_count >> 31 & 7)) >> 3) + 1;
    occupancy_bitmap = operator_new(bitmap_size);

    if (occupancy_bitmap) {
        /* Fill with 0xFF (all dirty) */
        uint32_t* bitmap32 = (uint32_t*)occupancy_bitmap;
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
/* ================================================================== */
void* TileMap::GetObjectAt(short tile_x, short tile_y, short layer)
{
    if (tile_x < 0 || tile_x > 0x51 ||
        tile_y < 0 || tile_y > 0x41) {
        return 0;
    }

    size_t data_index = TILE_OFFSET(tile_x, tile_y, layer) - 0x48;
    return ReadTilePointer(data_index);
}

/* ================================================================== */
/* TileMap::GetObjectAtEx                                              */
/* Address: 0x455670                                                   */
/*                                                                     */
/* Extended version: scans layers from highest active layer downward   */
/* (using the active-layer count byte within the tile entry)           */
/* and returns the first non-empty object.                             */
/* ================================================================== */
void* TileMap::GetObjectAtEx(short tile_x, short tile_y, short* layer_out)
{
    if (tile_x < 0 || tile_x > 0x51 ||
        tile_y < 0 || tile_y > 0x42) {
        if (layer_out) *layer_out = -1;
        return 0;
    }

    int tile_index = (int)tile_x * 0x41 + (int)tile_y;

    /* Read the active layer count from byte within the tile entry */
    int active_layers = static_cast<int8_t>(
        tile_data[(tile_index + 2) * 0x40 - 0x48]);
    if (active_layers < 0) {
        if (layer_out) *layer_out = -1;
        return 0;
    }

    /* Scan from highest active layer down to 0 */
    for (int l = active_layers; l >= 0; l--) {
        void* obj = ReadTilePointer(
            ((int)l + tile_index * 0x10) * 4 + 100 - 0x48);
        if (obj != 0) {
            *layer_out = (short)l;
            return obj;
        }
    }

    if (layer_out) *layer_out = -1;
    return 0;
}

/* ================================================================== */
/* TileMap::FindObjectByPos                                            */
/* Address: 0x4556F0                                                   */
/* ================================================================== */
void* TileMap::FindObjectByPos(int pixel_x, int pixel_y)
{
    short tile_x = (pixel_x < 0) ? -1 : (short)(pixel_x >> 4);
    short tile_y = (pixel_y < 0) ? -1 : (short)(pixel_y >> 4);

    int tile_index = (int)tile_x * 0x41 + (int)tile_y;
    int tile_base = tile_index * 0x40;

    /* Active layer is within the tile entry */
    int active_layer = static_cast<int8_t>(
        tile_data[tile_base + 0x80 - 0x48]);

    return ReadTilePointer(tile_base + 0x64 + active_layer * 4 - 0x48);
}

/* ================================================================== */
/* TileMap::GetTileOrigin                                              */
/* Address: 0x455740                                                   */
/*                                                                     */
/* Gets the packed tile origin coordinates from TileMapObject +0x88:  */
/* tile_x in low 16 bits, tile_y in high 16 bits. Via out_id pointer.  */
/* Accesses tile data at this+0x64 (offset 0x1C within each tile entry)*/
/* rather than the standard this+0x48 (start of tile_data). Verified   */
/* against disassembly: MOV ECX, [ECX + ESI*4 + 0x64] at 0x455773.    */
/* Output via out_id pointer. Returns out_id for caller convenience.   */
/* Returns -1 via *out_id if empty or out of bounds.                   */
/* ================================================================== */
int* TileMap::GetTileOrigin(int* out_id, short tile_x, short tile_y, short layer)
{
    /* Bounds check before any tile data access */
    if (tile_x < 0 || tile_x > 0x51 ||
        tile_y < 0 || tile_y > 0x41) {
        *out_id = -1;
        return out_id;
    }

    /* Direct tile data access at this + 0x64 + (x*65+y)*64 + layer*4 */
    /* This is tile_data[0x1C + (x*65+y)*0x40 + layer*4] */
    size_t data_index = ((int)tile_x * 0x41 + (int)tile_y) * 0x40 +
                        (int)layer * 4 + 0x1C;
    int obj_val = ReadTileValue(data_index);

    if (obj_val != 0) {
        *out_id = *(int*)(reinterpret_cast<uint8_t*>(
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
/* Gets the packed tile origin coordinates from TileMapObject +0x88:  */
/* tile_x|tile_y<<16. Uses +0x48 offset for tile access (standard     */
/* tile_data start) vs +0x64 in GetTileOrigin. Output via out_packed.  */
/* ================================================================== */
void TileMap::GetTileOriginEx(int* out_packed, short tile_x, short tile_y, short layer)
{
    /* Bounds check before any tile data access */
    if (tile_x < 0 || tile_x > 0x51 ||
        tile_y < 0 || tile_y > 0x41) {
        *out_packed = -1;
        return;
    }

    /* Direct tile data access at this + 0x48 + (x*65+y)*64 + layer*4 */
    size_t data_index = ((int)tile_x * 0x41 + (int)tile_y) * 0x40 + (int)layer * 4;
    int obj_val = ReadTileValue(data_index);

    if (obj_val != 0) {
        *out_packed = *(int*)(reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(obj_val)) + 0x88);
        return;
    }
    *out_packed = -1;
}

/* ================================================================== */
/* TileMap_IsTileOccupied                                              */
/* Address: 0x457B60                                                   */
/*                                                                     */
/* Checks if two tile resources conflict. Uses type byte at +0x08:     */
/* 0x0C (12) = scenery, 0x03 (3) = track/building.                    */
/* Also checks TileData_IsSceneryTile and TileData_IsWaterTile.        */
/* Returns: 50 = building-building conflict,                           */
/*          10 = building-scenery/water conflict,                      */
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
        if (TileData_IsSceneryTile(tile_resource_b) &&
            resource_a->resource_id > 0x3010) {
            result = 10;
        }
        if (TileData_IsWaterTile(tile_resource_b)) {
            return 10;
        }
        return result;
    }

    /* Building/track (0x03) vs scenery (0x0C) */
    if (type_a == 0x03 && type_b == 0x0C) {
        if (TileData_IsSceneryTile(tile_resource_a) &&
            resource_b->resource_id > 0x3010) {
            result = 10;
        }
        if (TileData_IsWaterTile(tile_resource_a)) {
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
/* Checks if tile_b is buildable adjacent to tile_a.                   */
/* Type codes: 3=track/building, 0x0C=scenery, 0x0D=other (road?).    */
/* Returns: 100=valid placement, 0x65=buildable but restricted,        */
/*          -1=blocked.                                                */
/* ================================================================== */
int TileMap_IsTileBuildable(int tile_resource_a, int tile_resource_b)
{
    TileMapResource* resource_a = reinterpret_cast<TileMapResource*>(tile_resource_a);
    TileMapResource* resource_b = reinterpret_cast<TileMapResource*>(tile_resource_b);
    char type_a = resource_a->object_type;
    char type_b = resource_b->object_type;

    if (type_b == 0x03) {
        /* Building on road/path */
        if (TileData_IsTrackTile(tile_resource_b) &&
            type_a != 0x0C && type_a == 0x0D) {
            return 100;
        }
    } else if (type_b == 0x0C) {
        /* Scenery on something */
        if (INPUT_EditCharHandler(tile_resource_b)) {
            if (type_a == 0x0C) {
                int handled_a = INPUT_EditCharHandler(tile_resource_a);
                return handled_a ? 0x65 : -1;
            }
            if (type_a == 0x0D) {
                return 100;
            }
        }
    } else if (type_b == 0x0D) {
        /* Road/path type */
        if (type_a == 0x03) {
            int is_track = TileData_IsTrackTile(tile_resource_a);
            return is_track ? 0x65 : -1;
        }
        if (type_a == 0x0C || type_a == 0x0D) {
            return 100;
        }
    }

    return -1;
}

/* ================================================================== */
/* TileMap::InvalidateRect                                              */
/* Address: 0x455840                                                   */
/* ================================================================== */
void TileMap::InvalidateRect(int left, int top, int right, int bottom)
{
    /* Only process in game modes 3 or 4 */
    if (g_game_mode != 4 && g_game_mode != 3) {
        return;
    }

    /* Convert pixel coords to tile coords */
    short tile_left   = (left < 0)   ? (short)-1 : (short)(left >> 4);
    short tile_top    = (top < 0)    ? (short)-1 : (short)(top >> 4);
    short tile_right  = (right - 1 < 0)  ? (short)-1 : (short)((right - 1) >> 4);
    short tile_bottom = (bottom - 1 < 0) ? (short)-1 : (short)((bottom - 1) >> 4);

    /* Clamp to valid tile range */
    if (tile_left < 0)   tile_left = 0;
    if (tile_right >= tile_count_x)  tile_right = tile_count_x - 1;
    if (tile_top < 0)    tile_top = 0;
    if (tile_bottom >= tile_count_y) tile_bottom = tile_count_y - 1;

    /* Set dirty bits in occupancy bitmap */
    for (short y = tile_top; y <= tile_bottom; y++) {
        for (short x = tile_left; x <= tile_right; x++) {
            uint32_t bit_index = g_player_id * (int)y + (int)x;
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
    int neighbor_def1 = resource->neighbor_def[direction][0];
    int neighbor_def2 = resource->neighbor_def[direction][1];
    if (neighbor_def1 == 0 && neighbor_def2 == 0) {
        return NULL;
    }

    /* Get sub-object position for this direction */
    int pos_xy[2];
    Entity_GetSubObjectPosition(sprite, pos_xy, direction);
    int world_x = pos_xy[0];
    int world_y = pos_xy[1];

    short tile_x = (world_x < 0) ? -1 : (short)(world_x >> 4);
    short tile_y = (world_y < 0) ? -1 : (short)(world_y >> 4);

    /* Direction offsets: 0=up, 1=right, 2=down, 3=left */
    switch (direction) {
    case 0:  tile_x--; break;
    case 1:  tile_y++; break;
    case 2:  tile_x++; break;
    case 3:  tile_y--; break;
    }

    /* Bounds check */
    if (tile_x < 0 || tile_x >= 0x52 ||
        tile_y < 0 || tile_y >= 0x42) {
        return NULL;
    }

    /* Get the object at the target tile (first layer) */
    TileMapObject* neighbor = static_cast<TileMapObject*>(
        ReadTilePointer((tile_x * 0x41 + (int)tile_y) * 0x40));

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
/* For each valid neighbor, checks if buildable. Returns count (0-4)  */
/* or 1 if any neighbor has a station resource (0xC50/0xC52).         */
/* ================================================================== */
char TileMap::SetViewport(TileMapObject* building_sprite)
{
    if (building_sprite == NULL) {
        return 0;
    }

    TileMapResource* resource = building_sprite->resource;
    if (INPUT_EditCharHandler(reinterpret_cast<intptr_t>(resource))) {
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
            while (res != NULL && INPUT_EditCharHandler(reinterpret_cast<intptr_t>(res))) {
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
    if (INPUT_EditCharHandler(reinterpret_cast<intptr_t>(resource))) {
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
            while (res != NULL && INPUT_EditCharHandler(reinterpret_cast<intptr_t>(res))) {
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
        corner[0] = GetObjectAt((short)(tile_x - 1), (short)(tile_y - 1), 0);
        corner[1] = GetObjectAt((short)(tile_x - 1), (short)(tile_y + 1), 0);
        corner[2] = GetObjectAt((short)(tile_x + 1), (short)(tile_y - 1), 0);
        corner[3] = GetObjectAt((short)(tile_x + 1), (short)(tile_y + 1), 0);

        /* Subtract scenery tiles (type 0x0C) from valid count */
        for (int i = 0; i < 4; i++) {
            if (corner[i] != 0) {
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
/* Fills occupancy data for each of the 4 directions. For each         */
/* direction, walks the chain of connected tiles until a non-          */
/* conflicting one is found.                                           */
/* ================================================================== */
void TileMap::GetTileRect(TileMapObject* sprite)
{
    if (sprite == NULL) return;

    TileMapResource* resource = sprite->resource;

    for (int dir = 0; dir < 4; dir++) {
        int32_t* neighbor_ptr = &sprite->occupancy_neighbors[dir];
        int32_t* score_ptr = &sprite->occupancy_scores[dir];

        /* Zero output fields */
        sprite->occupancy_links[dir] = 0;
        *score_ptr = 0;

        TileMapObject* neighbor = GetViewport(sprite, dir);
        if (neighbor != NULL) {
            while (1) {
                TileMapResource* neighbor_res = neighbor->resource;
                int occupancy = TileMap_IsTileOccupied(
                    reinterpret_cast<intptr_t>(resource),
                    reinterpret_cast<intptr_t>(neighbor_res));
                if (occupancy < 0) break;

                if (!INPUT_EditCharHandler(reinterpret_cast<intptr_t>(neighbor_res))) {
                    *neighbor_ptr = reinterpret_cast<intptr_t>(neighbor);
                }
                *score_ptr += occupancy;

                /* Walk to next connected tile if chained */
                if (neighbor->occupancy_scores[0] < 0) break;
                neighbor = GetViewport(neighbor, dir);
                if (neighbor == NULL) break;
            }
        }
    }
}

/* ================================================================== */
/* TileMap::GetTileAt                                                  */
/* Address: 0x457900                                                   */
/*                                                                     */
/* Like GetTileRect but uses IsTileBuildable instead of IsTileOccupied.*/
/* ================================================================== */
void TileMap::GetTileAt(TileMapObject* sprite)
{
    if (sprite == NULL) return;

    TileMapResource* resource = sprite->resource;

    for (int dir = 0; dir < 4; dir++) {
        int32_t* neighbor_ptr = &sprite->build_links[dir];
        int32_t* score_ptr = &sprite->build_scores[dir];

        /* Zero output fields */
        sprite->occupancy_scores[dir + 1] = 0;
        *score_ptr = 0;

        TileMapObject* neighbor = GetViewport(sprite, dir);
        if (neighbor != NULL) {
            while (1) {
                TileMapResource* neighbor_res = neighbor->resource;
                int buildable = TileMap_IsTileBuildable(
                    reinterpret_cast<intptr_t>(resource),
                    reinterpret_cast<intptr_t>(neighbor_res));
                if (buildable < 0) break;

                if (!INPUT_EditCharHandler(reinterpret_cast<intptr_t>(neighbor_res))) {
                    *neighbor_ptr = reinterpret_cast<intptr_t>(neighbor);
                }
                *score_ptr += buildable;

                if (neighbor->build_scores[0] < 0) break;
                neighbor = GetViewport(neighbor, dir);
                if (neighbor == NULL) break;
            }
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
/* 3D occupancy grid (w x h x d) of the target building to verify     */
/* that no blocking objects are in the way.                            */
/* ================================================================== */
char TileMap::ScrollRect(char use_sound, TileMapObject* target_building,
    short delta_x, ushort delta_y, int placement_mode)
{
    TileMapObject* building = target_building;
    short grid_w = building->grid_width;
    short grid_h = building->grid_height;

    /* Bounds check */
    if ((int)grid_w + (int)delta_x > (int)tile_count_x ||
        (int)grid_h + (int)delta_y > (int)tile_count_y) {
        return 0;
    }

    char valid = 1;

    /* Check road tile compatibility */
    if (building->object_type == 0x03) {
        if (TileData_IsRoadTile(reinterpret_cast<intptr_t>(target_building))) {
            uint category = TileData_GetTileCategory(
                target_building, delta_x, delta_y);
            valid = (char)category;
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
                for (short iy = 0; iy < (short)bVar7; iy++) {
                    for (short ix = 0; ix < (short)bVar4; ix++) {
                        int occ_offset = 0x16E + (int)iz * 9 * 7 +
                                         (int)iy * 7 + (int)ix;
                        int8_t occ = building->occupancy_grid[occ_offset - 0x16E];
                        if (occ != 0) {
                            int tile_x = (int)iy + (int)delta_x;
                            int tile_y = (int)ix + (int)delta_y;
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
                                            (int)tile_x << 4,
                                            (int)tile_y << 4, 4);
                                    }
                                    void* ptr = ReadTilePointer(
                                        TILE_OFFSET(
                                            (short)(iy + (short)delta_x),
                                            (short)(ix + (short)delta_y),
                                            iz) - 0x48);
                                    if (ptr) {
                                        ScrollTo(
                                            static_cast<TileMapObject*>(ptr),
                                            placement_mode);
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
/* Finally removes object from INPUT manager. Returns 1 on success.    */
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
    short base_x = *(short*)((uint8_t*)target + 0x22);
    short tile_y = target->tile_y;

    byte grid_span_y = resource->grid_span_y;       /* +0x16B */
    byte original_span = resource->original_span;    /* +0x16C */
    byte grid_width  = resource->grid_width;         /* +0x168 */
    byte grid_height = resource->grid_height;        /* +0x169 */

    /* ---- Phase 1: Clear original tile area ---- */
    /* Iterates y from tile_y to tile_y+original_span, x from base_x to base_x+grid_span_y */
    {
        int y = (int)tile_y;
        int y_end = (int)(ushort)(tile_y + (ushort)original_span);

        for (; y < y_end; y++) {
            int x = (int)base_x;
            int x_end = (int)(ushort)((ushort)grid_span_y + base_x);

            for (; x < x_end; x++) {
                int tile_idx = y + x * 0x41;

                /* Scan layers 6 down to 0, at offset 0x34-0x1C within tile */
                /* Access: this + tile_idx*0x40 + 0x64 + layer*4 */
                int8_t* active_layer_byte =
                    (int8_t*)((uint8_t*)this + (tile_idx + 2) * 0x40);
                int8_t active = *active_layer_byte;

                for (int8_t layer = 6; layer >= 0; layer--) {
                    /* Slot at this + tile_idx*0x40 + 0x64 + layer*4 */
                    int* slot = (int*)((uint8_t*)this + tile_idx * 0x40 +
                                       0x64 + (int)layer * 4);
                    if (reinterpret_cast<void*>(static_cast<uintptr_t>(*slot)) == target) {
                        /* Decrement active layer count if this was the top */
                        if (active == layer) {
                            *active_layer_byte = active - 1;
                        }
                        *slot = 0;

                        /* Set occupancy bit (mark as dirty) */
                        uint32_t bit_idx = (uint32_t)g_player_id * (uint32_t)y +
                                           (uint32_t)x;
                        uint8_t* bitmap_byte =
                            (uint8_t*)g_tile_occupied_bitmap + (bit_idx >> 3);
                        *bitmap_byte |= ATTR_0047f108[bit_idx & 7];
                    }
                }

                /* Compress active layer byte: while topmost slot is empty, decrement */
                active = *active_layer_byte;
                while (active >= 0) {
                    int* slot = (int*)((uint8_t*)this +
                                       ((int)active + tile_idx * 0x10 + 0x19) * 4);
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
    /* Iterates y from new_tile_y to new_tile_y+grid_height, x from base_x to base_x+grid_width */
    {
        tile_y = tile_y + (ushort)(original_span - grid_height);
        int y = (int)tile_y;
        int y_end = (int)(ushort)((ushort)grid_height + tile_y);

        for (; y < y_end; y++) {
            int x = (int)base_x;
            int x_end = (int)(ushort)((ushort)grid_width + base_x);

            for (; x < x_end; x++) {
                int tile_idx = y + x * 0x41;

                /* Active layer byte at offset +0x39 within tile (0x48 + tile_idx*0x40 + 0x39 - 0x48) */
                int8_t* active_layer_byte =
                    (int8_t*)((uint8_t*)this + tile_idx * 0x40 + 0x81);
                int8_t active = *active_layer_byte;

                if (active >= 0) {
                    /* Scan from active layer down to 0 */
                    /* Access: this + (tile_idx*0x10 + layer)*4 + 0x48 = standard tile_data */
                    int* slot = (int*)((uint8_t*)this +
                                       (tile_idx * 0x10 + (int)active) * 4 + 0x48);
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
                    int* slot = (int*)((uint8_t*)this +
                                       ((int)active + tile_idx * 0x10 + 0x12) * 4);
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

    /* Remove object from INPUT manager */
    extern int INPUT_RemoveObject(void** mgr, void* obj, uint param);
    INPUT_RemoveObject((void**)&g_input_mgr, target, (uint)scroll_flag);

    return target;
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
    short tile_x = (screen_x < 0) ? -1 : (short)(screen_x >> 4);
    short tile_y = (screen_y < 0) ? -1 : (short)(screen_y >> 4);

    if (g_game_mode != 3) {
        if (g_game_mode != 4) {
            return 0;
        }
        void* obj = GetObjectAtEx(tile_x, tile_y, (short*)&screen_y);
        if (g_build_mode == 1 && g_click_on_building == 1 && g_placement_valid == 0) {
            if (obj != NULL) {
                if (scroll_drag_active != 0 &&
                    (0xf < screen_x - drag_start_x ||
                     0xf < screen_y - drag_start_y)) {
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
            int* result = FindObject(g_placement_resource_id, tile_x, tile_y, 0, 1);
            if (g_placement_blocked != 0) {
                return 1;
            }
            if (result != NULL) {
                return 1;
            }
            g_placement_blocked = 1;
            return 0;
        }
        return 0;
    }

    /* Game mode 3: town mode — hit-test and building selection */
    /* (additional logic follows in the binary — abbreviated for transcription) */
    return 0;
}

/* ================================================================== */
/* TileMap::InvalidateDirtyRects                                       */
/* Address: 0x456150                                                   */
/*                                                                     */
/* Main render pipeline. Builds dirty RECT list from occupancy bitmap, */
/* blits cursor surface to primary surface for each dirty region,      */
/* merges overlapping rects via ProcessDirtyRects, presents each rect. */
/* ================================================================== */
void TileMap::InvalidateDirtyRects(char force_all)
{
    RECT* rect_list = NULL;
    RECT* prev_rect = NULL;
    RECT* head_rect = NULL;

    /* Only process in game modes 3 or 4, and not while locked */
    if ((g_game_mode != 3 && g_game_mode != 4) || g_lock_update_flag == 1) {
        return;
    }

    World_Lock(g_world);  /* 0x4A98B0 */
    RECT local_20;
    SetRectEmpty(&local_20);

    short start_x, start_y, end_x, end_y;
    if ((g_is_fullscreen == 0 && g_world_width <= g_screen_width) || force_all != 0) {
        start_x = 0;
        start_y = 0;
        end_x = tile_count_x;
        end_y = tile_count_y;
    } else {
        start_x = (viewport_x < 0) ? (short)-1 : (short)(viewport_x >> 4);
        end_x = (viewport_x + g_client_offset_x < 0) ? (short)-1
                : (short)((viewport_x + g_client_offset_x) >> 4);
        start_y = (viewport_y < 0) ? (short)-1 : (short)(viewport_y >> 4);
        end_y = (viewport_y + g_client_offset_y < 0) ? (short)-1
                : (short)((viewport_y + g_client_offset_y) >> 4);
        if (end_x > tile_count_x) end_x = tile_count_x;
        if (end_y > tile_count_y) end_y = tile_count_y;
    }

    /* Scan occupancy bitmap for dirty tiles, build rect list */
    for (short y = start_y; y < end_y; y++) {
        for (short x = start_x; x < end_x; x++) {
            uint bit_idx = (uint)g_player_id * (uint)y + (uint)x;
            uint8_t* bitmap = (uint8_t*)g_tile_occupied_bitmap;
            if ((ATTR_0047f108[bit_idx & 7] & bitmap[bit_idx >> 3]) != 0) {
                /* Dirty tile found — expand dirty rect or create new one */
                RECT tile_rect;
                tile_rect.left = x * 16;
                tile_rect.top = y * 16;
                tile_rect.right = tile_rect.left + 16;
                tile_rect.bottom = tile_rect.top + 16;

                if (rect_list == NULL) {
                    rect_list = (RECT*)operator_new(sizeof(RECT) + 8);
                    CopyRect(rect_list, &tile_rect);
                    rect_list[1].left = 0;
                    head_rect = rect_list;
                } else {
                    UnionRect(&local_20, rect_list, &tile_rect);
                    CopyRect(rect_list, &local_20);
                }

                /* Clear the dirty bit */
                bitmap[bit_idx >> 3] &= ~ATTR_0047f108[bit_idx & 7];
            }
        }
    }

    /* Process each dirty rect: blit cursor surface to primary surface */
    for (RECT* r = head_rect; r != NULL; r = (RECT*)r[1].left) {
        if (g_cursor_surface != NULL && g_primary_surface != NULL) {
            UIPANEL_Blit(g_cursor_surface, r->left, r->top,
                         r->right - r->left, r->bottom - r->top,
                         g_primary_surface, r->left, r->top,
                         r->right - r->left, r->bottom - r->top, 0);
        }
    }

    /* Merge overlapping dirty rects */
    TileMap_ProcessDirtyRects(head_rect);

    /* Clip rects to viewport */
    TileMap_FreeDirtyRects(head_rect);

    /* Present each rect */
    for (RECT* r = head_rect; r != NULL; r = (RECT*)r[1].left) {
        DDRAW_PresentRect(r, g_main_window, &viewport_x, 0);
    }

    /* Free rect list */
    while (head_rect != NULL) {
        RECT* next = (RECT*)head_rect[1].left;
        GLOBAL_free(head_rect);
        head_rect = next;
    }

    World_Unlock(g_world);  /* 0x4A98B0 */
}

/* ================================================================== */
/* TileMap::ProcessRect                                                */
/* Address: 0x456700                                                   */
/*                                                                     */
/* Renders all objects in a dirty rect. Iterates visible tiles, calls  */
/* Draw on each object. In town mode dispatches BuildingMgr,           */
/* World_InvalidateRect, tooltips, and cursors.                        */
/* ================================================================== */
void TileMap::ProcessRect(int left, int top, int right, int bottom)
{
    short tile_left   = (left < 0)   ? -1 : (short)(left >> 4);
    short tile_top    = (top < 0)    ? -1 : (short)(top >> 4);
    short tile_right  = (right < 0)  ? -1 : (short)(right >> 4);
    short tile_bottom = (bottom < 0) ? -1 : (short)(bottom >> 4);

    if (tile_top < tile_bottom) {
        int tile_idx = (int)tile_top + (int)tile_left * 0x41;
        int pixel_y = tile_top << 4;

        for (short y = tile_top; y < tile_bottom; y++) {
            if (tile_left < tile_right) {
                int pixel_x = tile_left << 4;
                int cur_tile_idx = tile_idx;

                for (short x = tile_left; x < tile_right; x++) {
                    uint bit_idx = (uint)g_player_id * (uint)y + (uint)x;
                    uint8_t* bitmap = (uint8_t*)g_tile_occupied_bitmap;
                    if ((ATTR_0047f108[bit_idx & 7] & bitmap[bit_idx >> 3]) != 0) {
                        /* Read active layer count from tile entry */
                        int8_t active = static_cast<int8_t>(
                            tile_data[(cur_tile_idx + 2) * 0x40 - 0x48]);
                        if (active < 3) active = 2;

                        /* Draw objects in this tile (layers 0..active) */
                        for (int8_t layer = 0; layer <= active; layer++) {
                            int32_t obj_val = ReadTileValue(
                                cur_tile_idx * 0x40 + layer * 4);
                            if (obj_val != 0) {
                                /* Call draw on the object */
                                /* (GameObject virtual draw dispatch) */
                            }
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
}

/* ================================================================== */
/* TileMap::ProcessObjectTimer                                         */
/* Address: 0x456D90                                                   */
/*                                                                     */
/* Validates object's tile footprint matches expected resource IDs in  */
/* all 4 directions (up/right/down/left spiral scan). Returns status.  */
/* ================================================================== */
uint TileMap::ProcessObjectTimer(TileMapObject* obj)
{
    if (obj == NULL || obj->resource == NULL) {
        return 0;
    }

    TileMapResource* res = obj->resource;
    char valid = 1;

    /* Get object's world position and tile footprint bounds */
    extern void* GameObject_GetSubObjectWorldPos(void* obj, int* out_xy);
    int out_xy[2];
    int* pos = (int*)GameObject_GetSubObjectWorldPos(obj, out_xy);
    int world_x = pos[0];
    int world_y = pos[1];

    short tile_y_start = (short)(world_y >> 16);
    short tile_x_start = (short)world_x;

    short tile_y = tile_y_start - 1;
    int x_end = (short)((ushort)res->grid_span_y + obj->tile_x - 1) + 1;

    uint idx = 0;
    int cur_x = (int)(short)world_x;

    /* Spiral scan: validate tile footprint in 4 directions */
    if (cur_x <= x_end) {
        do {
            /* Check bounds */
            if (idx >= *(uint*)((uint8_t*)res + 0x560) || valid != 1) break;

            int expected_id = *(int*)(*(int*)((uint8_t*)res + 0x564) + idx * 4);
            if (expected_id != -1) {
                if ((short)world_x < 0 || g_player_id <= (short)world_x ||
                    tile_y < 0 || g_player_color <= tile_y) {
                    valid = 0;
                } else {
                    int tile_val = ReadTileValue(
                        (cur_x * 0x41 + (int)tile_y) * 0x40);
                    if (tile_val == 0) {
                        if (expected_id != 0) {
                            valid = 0;
                        }
                    } else if (expected_id != *(int*)(*(int*)(tile_val + 0x40) + 4)) {
                        valid = 0;
                    }
                }
            }
            idx++;
            world_x++;
            cur_x = (int)(short)world_x;
        } while (cur_x <= x_end);
    }

    /* Continue spiral scan in other directions (abbreviated) */
    return valid;
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
        AssetMgr_LoadFileEx((uint*)asset_load_ptr);
    }
    if (asset_enum_ptr != NULL) {
        AssetMgr_EnumFiles((uint*)asset_enum_ptr);
    }
}

/* ================================================================== */
/* TileMap::FindObject                                                 */
/* Address: 0x4550C0                                                   */
/*                                                                     */
/* Validates and places building at tile position. Calls ScrollRect    */
/* for validation, INPUT_PlaceObject to create, fills tile grid with   */
/* new object pointers in all occupied cells.                          */
/* ================================================================== */
int* TileMap::FindObject(int target_resource_id, short tile_x, short tile_y,
                          char unknown, int mode)
{
    int* result = NULL;

    if (tile_x < 0 || tile_x > tile_count_x ||
        tile_y < 0 || tile_y > tile_count_y) {
        return NULL;
    }

    void* res_data = ResourceManager_GetById((void**)&g_resmgr, (UINT)target_resource_id);
    if (res_data == NULL) {
        return NULL;
    }

    TileMapResource* resource = (TileMapResource*)res_data;
    ushort orig_span = (ushort)resource->original_span;
    byte span_y = resource->grid_span_y;
    int offset = (uint)orig_span - (uint)resource->grid_height;

    short adjusted_y = tile_y;
    if (unknown != 1) {
        adjusted_y = tile_y - (short)offset;
    }

    if (adjusted_y < 0) {
        return NULL;
    }

    if (g_allow_building_placement == 0 ||
        ScrollRect(0, (TileMapObject*)res_data, tile_x,
                   (ushort)(adjusted_y + (short)offset), mode) != 0) {
        if (ScrollRect(1, (TileMapObject*)res_data, tile_x,
                       (ushort)(offset + (int)adjusted_y), mode) != 0) {
            result = (int*)INPUT_PlaceObject((void**)&g_input_mgr, (uint)target_resource_id);
            if (result != NULL) {
                /* Fill tile grid with new object pointer */
                short gy = 0;
                int y_end = offset + (int)adjusted_y;
                if (resource->grid_height != 0) {
                    do {
                        short gx = 0;
                        uint x_start = (uint)tile_x;
                        if (resource->grid_width != 0) {
                            do {
                                /* Place object in each occupied tile cell */
                                for (int8_t iz = 0; iz < resource->grid_depth; iz++) {
                                    int occ_idx = 0x16E + (int)iz * 9 * 7 +
                                                  (int)gy * 7 + (int)gx;
                                    if (((TileMapObject*)result)->occupancy_grid[occ_idx - 0x16E] != 0) {
                                        WriteTileValue(
                                            ((int)(short)(x_start + gx) * 0x41 +
                                             (int)(short)(y_end)) * 0x40 + iz * 4,
                                            (int32_t)(intptr_t)result);
                                    }
                                }
                                gx++;
                            } while (gx < resource->grid_width);
                        }
                        gy++;
                        y_end++;
                    } while (gy < resource->grid_height);
                }
            }
        }
    }

    return result;
}

/* ================================================================== */
/* TileMap::Scroll                                                     */
/* Address: 0x455960                                                   */
/*                                                                     */
/* Vector-based scroll with floating-point interpolation. Steps along  */
/* vector from drag_start to target, calling GetObjectAtEx at each step*/
/* and ScrollTo on found objects.                                      */
/* ================================================================== */
uint TileMap::Scroll(int delta_x, int delta_y, int drag_start_x, int drag_start_y)
{
    int vec_x = delta_x - drag_start_x;
    int vec_y = delta_y - drag_start_y;

    double cur_x = (double)delta_x;
    double cur_y = (double)delta_y;

    /* SQRT operation approximated via FPU */
    double length = (double)(vec_x * vec_x + vec_y * vec_y);
    /* length = sqrt(dx*dx + dy*dy) */
    double norm_x = (double)vec_x / length;
    double norm_y = (double)vec_y / length;

    /* Step along the vector */
    short last_tile_x = 0, last_tile_y = 0;
    for (double dist = 0; dist < length; dist += 1.0) {
        cur_x = cur_x + norm_x;
        cur_y = cur_y + norm_y;

        short tile_x = ((int)cur_x < 0) ? -1 : (short)((int)cur_x >> 4);
        short tile_y = ((int)cur_y < 0) ? -1 : (short)((int)cur_y >> 4);

        if (tile_x != last_tile_x || tile_y != last_tile_y) {
            short layer_out;
            void* obj = GetObjectAtEx(tile_x, tile_y, &layer_out);
            if (obj != NULL) {
                ScrollTo((TileMapObject*)obj, 1);
            }
            last_tile_x = tile_x;
            last_tile_y = tile_y;
        }
    }

    return 0;
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
    for (RECT* r = rect_list; r != NULL; r = (RECT*)r[1].left) {
        RECT* next = (RECT*)r[1].left;
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
            next = (RECT*)next[1].left;
        }
    }
    return merged;
}

/* ====
/* ================================================================== */
/* TileMap_FreeDirtyRects stub                                         */
/* Address: 0x456D10                                                   */
/* TODO: full decompilation                                            */
/* ================================================================== */
void TileMap_FreeDirtyRects(RECT* rect_list)
{
    (void)rect_list;
}

/* ================================================================== */
/* TileMap_FindNearestObject                                           */
/* Address: 0x457CE0                                                   */
/*                                                                     */
/* Spatial search in concentric diamond rings. Finds nearest object    */
/* matching type_filter byte (at TileMapResource+0x08) within          */
/* search_radius, using Math_DistSquared for distance comparison.      */
/* ================================================================== */
void* TileMap_FindNearestObject(TileMap* tilemap, ushort type_filter,
                                 int target_x, int target_y,
                                 int search_radius)
{
    int best_obj = 0;
    int best_dist_sq = 999999999;

    short radius_tiles = (search_radius < 0) ? -1 : (short)(search_radius >> 4);
    short center_x = (target_x < 0) ? -1 : (short)(target_x >> 4);
    short center_y = (target_y < 0) ? -1 : (short)(target_y >> 4);

    for (short ring = 0; ring <= radius_tiles; ring++) {
        if (best_obj != 0) return (void*)best_obj;

        int r = (int)ring;
        int x_start = (int)center_x - r;
        int y_start = (int)center_y - r;
        int x_end = (int)center_x + r;
        int y_end = (int)center_y + r;

        /* Clamp to valid tile range */
        if (x_start < 0) x_start = 0;
        if (y_start < 0) y_start = 0;

        for (int x = x_start; x <= x_end && x < tilemap->tile_count_x; x++) {
            for (int y = y_start; y <= y_end && y < tilemap->tile_count_y; y++) {
                int tile_val = tilemap->ReadTileValue((x * 0x41 + y) * 0x40);
                if (tile_val != 0) {
                    int resource_ptr = *(int*)(tile_val + 0x40);
                    if (resource_ptr != 0) {
                        uint8_t obj_type = *(uint8_t*)(resource_ptr + 8);
                        if (obj_type == (uint8_t)type_filter) {
                            int obj_wx = *(int*)(tile_val + 0x4C);
                            int obj_wy = *(int*)(tile_val + 0x50);
                            int dist_sq = Math_DistSquared(target_x, target_y,
                                                           obj_wx, obj_wy);
                            if (dist_sq < best_dist_sq) {
                                best_dist_sq = dist_sq;
                                best_obj = tile_val;
                            }
                        }
                    }
                }
            }
        }
    }
    return (void*)best_obj;
}

/* ================================================================== */
/* TileMap_CreateOverlay stub                                          */
/* Address: 0x457080                                                   */
/* TODO: full decompilation                                            */
/* ================================================================== */
void* TileMap_CreateOverlay(TileMap* tilemap, int resource_id, byte param_2)
{
    (void)tilemap;
    (void)resource_id;
    (void)param_2;
    return NULL;
}

/* ================================================================== */
/* TileMap_WorldToScreen                                               */
/* Address: 0x458270 (address verification needed; Ghidra shows        */
/* BuildingMgrObjectGroup_DtorBody at this location)                   */
/*                                                                     */
/* Converts world pixel coordinates to screen coordinates by applying  */
/* the current viewport scroll offset.                                 */
/* TODO: verify address against binary symbol table.                   */
/* ================================================================== */
void TileMap_WorldToScreen(void* output_coords)
{
    /* The address 0x458270 may be incorrect in the symbol table.      */
    /* Ghidra decompiles a different function at this address.         */
    /* Placeholder: apply viewport offset to output coordinates.       */
    (void)output_coords;
}
