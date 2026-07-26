/**
 * tilemap.cpp — TileMap function implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * TileMap manages the game world's tile grid (82x66 tiles, each 64 bytes for
 * 16 layer slots). It tracks scroll state, dirty regions for rendering, tile
 * occupancy/buildability checks, and viewport scrolling.
 *
 * All TileMap_* functions are C-linkage free functions operating on the
 * global g_tilemap at 0x4AAD08.
 */

#include "tilemap.h"
#include <new>
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Memory */
    void*  operator_new(size_t size);               /* 0x465CE0 */
    void   GLOBAL_free(void* ptr);

    /* Game globals */
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
    extern int32_t  g_town_overlay_rect;     /* 0x485394 — packed town overlay rect */
    extern int32_t  DAT_00485390;            /* 0x485390 — town overlay rect field */
    extern int32_t  DAT_00485394;            /* 0x485394 — town overlay rect field */
    extern int32_t  DAT_00485398;            /* 0x485398 — town overlay rect field */
    extern uint8_t  g_build_mode;
    extern uint8_t  g_disable_input;
    extern uint8_t  DAT_004851f0;            /* 0x4851F0 — lock/update flag */
    extern uint8_t  g_click_on_building;
    extern uint8_t  g_placement_valid;
    extern uint8_t  g_placement_blocked;
    extern int32_t  g_placement_resource_id;
    extern void*    g_input_mgr;
    extern void*    g_town_view;
    extern void*    g_ddraw_building;
    extern void*    g_about;                 /* 0x4FD428 */
    extern void*    g_netman;                /* 0x4FD3B8 */
    extern uint8_t  g_click_on_town;
    extern int32_t  g_selected_building;
    extern int32_t  g_town_selection_rect_left;
    extern int32_t  g_town_selection_rect_top;
    extern int32_t  g_town_selection_rect_right;
    extern int32_t  g_town_selection_rect_bottom;
    extern uint8_t  g_has_selection;
    extern int32_t  g_viewport_x;
    extern int32_t  g_viewport_rect_left;    /* viewport clipping rect left */
    extern uint8_t  g_allow_building_placement;  /* 0x485328 */
    extern int32_t  g_player_id;
    extern uint8_t  g_player_color;
    extern void*    g_cursor_surface;        /* _g_cursor_surface */
    extern void*    g_primary_surface;       /* _g_primary_surface */
    extern void*    g_tile_occupied_bitmap;  /* world tile occupancy bitmap */
    extern void*    g_resmgr;               /* 0x4855E8 */
    extern void*    g_scripted_object;      /* 0x4A98E4 */
    extern void*    g_building_mgr;         /* building manager singleton */
    extern void*    g_tilemap;              /* TileMap at 0x4AAD08 */
    extern void*    g_game;                 /* Game singleton */

    /* Bitmask lookup table */
    extern uint8_t  ATTR_0047f108[8];       /* bitmask lookup (1<<n) */

    /* External functions */
    int   __thiscall RESDATA_IsRoadTile(int ptr);           /* 0x45AAF0 */
    int   __thiscall RESDATA_GetTileCategory(void* ptr, short a, ushort b); /* 0x45xxx */
    /* RESDATA_IsSceneryTile, _IsWaterTile, _IsTrackTile, INPUT_EditCharHandler — declared in tilemap.h */
    int   __thiscall INPUT_PlaceObject(void** mgr, uint resource_id); /* 0x420AX */
    int   __thiscall INPUT_RemoveObject(void** mgr, void* obj, uint param); /* 0x420BX */
    void  __cdecl PlaySoundAt(int sound_id, int x, int y, int channel); /* 0x463800 */
    int   __thiscall Town_SelectBuilding(void** town_view, int building); /* 0x42C9C0 */
    int   __thiscall DDRAW_SelectBuilding(void** ddraw_building, int building); /* 0x46AA80 */
    void  __cdecl CGWND_SetMode(void* mode);                  /* 0x407AF0 */
    void  __thiscall Town_RenderSelection(void* town_view);    /* 0x42F1E0 */
    void  __thiscall Game_SetCursorByResourceId(void* game, int x, int y,
                                                 int w, int h, int flag); /* 0x410Y0 */
    void  __thiscall UI_SetTooltipText(void* mgr, int x, int y,
                                        int w, int h);        /* 0x423ZX */
    void  __thiscall UI_UpdateTooltip(void* mgr, int x, int y,
                                       int w, int h);         /* 0x423YX */
    void  __thiscall BuildingMgr_DispatchAll(void* mgr, int packed_type,
                                              int x, int y, int w, int h); /* 0x44EX */
    void  __thiscall World_InvalidateRect(void* world, int x, int y,
                                           int w, int h, short type); /* 0x44DX */
    void  __thiscall UI_SetTooltipPos(void* mgr, int x, int y,
                                       int w, int h);         /* 0x423ZX */
    void  __thiscall RESDATA_ScriptedObject_Dispatch(void* obj, int x, int y,
                                                      int w, int h, int flag); /* 0x45CX */
    void  __thiscall DDRAW_DispatchToSubObjects(void* ddraw, int x, int y,
                                                 int w, int h, void* flag); /* 0x46XX */
    void  __thiscall Game_DeselectGameObject(int param);       /* 0x410XX */
    void  __thiscall World_Init(void* world);                  /* 0x44D30 */
    void  __thiscall UI_CleanupTooltips(void* mgr);            /* 0x423XX */
    void  __thiscall INPUT_FileDlgProc(void* mgr);             /* 0x420XX */
    void* __fastcall DDRAW_SpriteDataCtor(void* obj, int type); /* 0x45CDF0 */
    void  __fastcall DDRAW_SpriteDataDtor(void* obj);          /* 0x45CXX */
    void  __thiscall Town_DeselectBuilding(int param);         /* 0x42FXX */
    void  __thiscall Town_UpdateSelection(int param);          /* 0x42FXX */
    void  __thiscall Game_ResetCursor(void* game);             /* 0x410XX */
    int   __thiscall GetResourceType(uint resource_id);        /* 0x45BXX */
    int   __thiscall Math_DistSquared(int x1, int y1,
                                       int x2, int y2);       /* 0x466X0 */
    void* __thiscall Entity_GetSubObjectPosition(void* obj, int* out_xy,
                                                  int direction); /* 0x458X0 */
    uint  __thiscall TileMap_Scroll(void* tilemap, int delta_x, int delta_y,
                                      int drag_start_x, int drag_start_y); /* 0x455960 */
    void  __thiscall TileMap_ScrollTo(void* tilemap, void* target, int flag); /* 0x455AB0 */
    void  TileMap_FreeDirtyRects(RECT* rect_list);            /* 0x456D10 */
    char  TileMap_ProcessDirtyRects(RECT* rect_list);          /* 0x456C60 */
    void  WIN32_GetThreadResult(int param);                    /* 0x466570 */
    void  AssetMgr_LoadFileEx(uint* ptr);                      /* 0x457110 */
    void  AssetMgr_EnumFiles(uint* ptr);                       /* 0x457170 */
    void  World_Lock(void* world);                             /* 0x44E200 */
    void  World_Unlock(void* world);                           /* 0x44E2D0 */
    void  UIPANEL_Blit(void* src, int sx, int sy, int sw, int sh,
                        void* dst, int dx, int dy, int dw, int dh, int flags); /* 0x421740 */
    void  DDRAW_PresentRect(RECT* rect, HWND hwnd,
                             int32_t* viewport_x, char flag);  /* 0x462150 */
    int   IntersectRect(RECT* dst, RECT* a, RECT* b);
    int   UnionRect(RECT* dst, RECT* a, RECT* b);
    int   SetRectEmpty(RECT* rect);
    void  SetRect(RECT* rect, int left, int top, int right, int bottom);
    void  CopyRect(RECT* dst, RECT* src);
    int   PtInRect(RECT* rect, int x, int y);
    void  InflateRect(RECT* rect, int dx, int dy);
    void  OutputDebugStringA(const char* str);
    void  Sleep(uint32_t ms);
    BOOL  InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
    BOOL  UpdateWindow(HWND hWnd);

    /* TileMap internal helpers */
    void* __thiscall ResourceManager_GetById(void** resmgr, UINT id); /* 0x4472B0 */
    void* __thiscall UIPANEL_InitSurface(void* surface, int w, int h,
                                          int a, int b, byte c);  /* 0x42XX */

/* ================================================================== */
/* Global TileMap singleton                                            */
/* ================================================================== */

/* g_tilemap at 0x4AAD08 — declared in tilemap.h */

/* ================================================================== */
/* Tile Index Helper Macro                                             */
/* ================================================================== */
/* Tile data offset in tilemap: 0x48 + (x * 65 + y) * 64 + layer * 4 */
#define TILE_OFFSET(x, y, layer) \
    (0x48 + ((int)(x) * 65 + (int)(y)) * 0x40 + (int)(layer) * 4)

/* ================================================================== */
/* Sprite_Create (TileMap init)                                        */
/* Address: 0x454CF0                                                   */
/*                                                                     */
/* Initializes TileMap sprite/rendering system. Sets vtable to         */
/* 0x478520, calls Sprite_UnlockAll (full reset), allocates two        */
/* DDRAW_SpriteData objects at +0x52488 and +0x5248C, zeros           */
/* occupancy bitmap at +0x52484, scroll_drag_active at +0x3C,          */
/* surface_locked at +0x52510, and the DDSURFACEDESC buffer.          */
/* Returns this pointer.                                               */
/*                                                                     */
/* NOTE: Despite the name "Sprite_Create", this function operates on   */
/* the TileMap global data structure. The vtable 0x478520 is for a     */
/* TileMap wrapper/manager class, NOT ButtonSprite.                    */
/* ================================================================== */
/** TileMap constructor body — Address: 0x454CF0. */
TileMap::TileMap()
{
}

void* TileMap::ReadTilePointer(size_t data_index) const
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(
        static_cast<uint32_t>(ReadTileValue(data_index))));
}

int32_t TileMap::ReadTileValue(size_t data_index) const
{
    return static_cast<int32_t>(
        static_cast<uint32_t>(tile_data[data_index]) |
        (static_cast<uint32_t>(tile_data[data_index + 1]) << 8) |
        (static_cast<uint32_t>(tile_data[data_index + 2]) << 16) |
        (static_cast<uint32_t>(tile_data[data_index + 3]) << 24));
}

void TileMap::WriteTileValue(size_t data_index, int32_t value)
{
    uint32_t packed = static_cast<uint32_t>(value);
    tile_data[data_index] = static_cast<uint8_t>(packed);
    tile_data[data_index + 1] = static_cast<uint8_t>(packed >> 8);
    tile_data[data_index + 2] = static_cast<uint8_t>(packed >> 16);
    tile_data[data_index + 3] = static_cast<uint8_t>(packed >> 24);
}

void* __fastcall Sprite_Create(void* tilemap)
{
    /* Placement construction lets C++ install the table at 0x478520. */
    TileMap* tm = new (tilemap) TileMap;

    /* Full TileMap reset */
    Sprite_UnlockAll(tm);

    /* Zero viewport_x (+0x1C) and viewport_y (+0x20) */
    tm->viewport_x = 0;
    tm->viewport_y = 0;

    /* Allocate first DDRAW_SpriteData at +0x52488 (asset_load_ptr) */
    void* mem1 = operator_new(0x2C);
    if (mem1 != NULL) {
        tm->asset_load_ptr = DDRAW_SpriteDataCtor(mem1, 7);
    } else {
        tm->asset_load_ptr = NULL;
    }

    /* Allocate second DDRAW_SpriteData at +0x5248C (asset_enum_ptr) */
    void* mem2 = operator_new(0x2C);
    if (mem2 != NULL) {
        tm->asset_enum_ptr = DDRAW_SpriteDataCtor(mem2, 8);
    } else {
        tm->asset_enum_ptr = NULL;
    }

    /* Zero occupancy bitmap */
    tm->occupancy_bitmap = NULL;

    /* Clear flags */
    tm->scroll_drag_active = 0;   /* +0x3C */
    tm->surface_locked = 0;       /* +0x52510 */

    /* Zero the DDSURFACEDESC buffer (31 dwords = 0x7C bytes) */
    /* Note: ddsurfacedesc_buf is at +0x52494 */
    for (int i = 0; i < 0x1F; i++) {
        ((uint32_t*)tm->ddsurfacedesc_buf)[i] = 0;
    }

    return tm;
}

/* ================================================================== */
/* Sprite_Shutdown (TileMap shutdown)                                  */
/* Address: 0x454DE0                                                   */
/*                                                                     */
/* Frees TileMap sprite resources: unlocks all, destroys and frees     */
/* sprite data at +0x52488 and +0x5248C, frees occupancy bitmap.      */
/*                                                                     */
/* NOTE: Despite "Sprite" prefix, operates on TileMap data.            */
/* ================================================================== */
/** TileMap destructor body — Address: 0x454DE0. */
TileMap::~TileMap()
{
    /* Unlock all surfaces first */
    Sprite_UnlockAll(this);

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

void __fastcall Sprite_Shutdown(void* tilemap)
{
    static_cast<TileMap*>(tilemap)->~TileMap();
}

/* ================================================================== */
/* Sprite_LockAll (TileMap viewport center recalculation)              */
/* Address: 0x454FA0                                                   */
/*                                                                     */
/* Recalculates viewport_center_x and viewport_center_y from client    */
/* area offsets and the current viewport position.                     */
/*                                                                     */
/* NOTE: Despite "Sprite" prefix, operates on TileMap data.            */
/* ================================================================== */
void __fastcall Sprite_LockAll(void* tilemap)
{
    TileMap* tm = (TileMap*)tilemap;

    /* viewport_center_x = (g_client_offset_x - g_client_width) / 2 + viewport_x + g_client_width */
    tm->viewport_center_x =
        (g_client_offset_x - g_client_width) / 2 + tm->viewport_x + g_client_width;

    /* viewport_center_y = (g_client_offset_y - g_client_height) / 2 + viewport_y + g_client_height */
    tm->viewport_center_y =
        (g_client_offset_y - g_client_height) / 2 + tm->viewport_y + g_client_height;
}

/* ================================================================== */
/* Sprite_UnlockAll (Full TileMap reset)                               */
/* Address: 0x454FE0                                                   */
/*                                                                     */
/* Full TileMap reset: deselects game objects, reinitializes world,    */
/* cleans up tooltips, clears all tile data in the grid and occupancy  */
/* bitmap. If windows exist and not in game mode 1, invalidates and    */
/* updates the window.                                                 */
/*                                                                     */
/* NOTE: Despite "Sprite" prefix, operates on TileMap data.            */
/* ================================================================== */
void __fastcall Sprite_UnlockAll(void* tilemap)
{
    TileMap* tm = (TileMap*)tilemap;

    /* Global reset sequence */
    Game_DeselectGameObject(0x4854C8);
    World_Init((void*)0x4A98B0);
    UI_CleanupTooltips((void*)0x4FD220);
    INPUT_FileDlgProc((void*)0x4A9990);

    /* Clear the trailing header bytes and all named tile storage. */
    for (int i = 2; i < 6; ++i) {
        tm->_pad_42[i] = 0;
    }
    for (size_t i = 0; i < sizeof(tm->tile_data); ++i) {
        tm->tile_data[i] = 0;
    }

    /* Fill occupancy bitmap with 0xFF if allocated */
    if (tm->occupancy_bitmap != NULL) {
        int tile_count = (int)tm->tile_count_y * (int)tm->tile_count_x;
        int bitmap_size = ((tile_count + (tile_count >> 31 & 7)) >> 3) + 1;

        uint32_t* bitmap32 = (uint32_t*)tm->occupancy_bitmap;
        uint32_t dword_count = bitmap_size >> 2;
        for (uint32_t i = 0; i < dword_count; i++) {
            bitmap32[i] = 0xFFFFFFFF;
        }

        uint8_t* bitmap8 = reinterpret_cast<uint8_t*>(&bitmap32[dword_count]);
        for (uint32_t i = 0; i < (bitmap_size & 3); i++) {
            bitmap8[i] = 0xFF;
        }
    }

    /* Reset tile grid active-layer bytes (the byte at +0x80 in each tile entry) */
    /* Iterates tile_count_y rows x tile_count_x columns */
    for (int y = 0; y < 0x41; y++) {
        for (int x = 0; x < 0x51; x++) {
            int tile_base = (x * 0x41 + y) * 0x40;  /* 0x40 = 64 bytes per tile */
            /* The active-layer bytes are named storage relative to tile_data. */
            tm->tile_data[tile_base + 0x38] = 0xFF;
            tm->tile_data[tile_base + 0x39] = 0xFF;
        }
    }

    /* Invalidate and update game window if not in game mode 1 */
    if (g_main_window != NULL) {
        HWND child_wnd = g_main_window;
        if (child_wnd != NULL && g_game_mode != 1) {
            InvalidateRect(child_wnd, NULL, FALSE);
            UpdateWindow(child_wnd);
        }
    }
}

/* ================================================================== */
/* TileMap_Init                                                        */
/* Address: 0x454E60                                                   */
/* ================================================================== */
void __thiscall TileMap_Init(TileMap* tilemap, char use_1024x768)
{
    int width;
    int height;

    if (use_1024x768 == 0) {
        /* Use screen dimensions, clamped */
        if (g_screen_width > 0x3FF) {
            if (g_screen_width < 0x501) {
                tilemap->width = g_screen_width;
                tilemap->total_width = g_screen_width;
                width = g_screen_width;
            } else {
                tilemap->width = 0x500;      /* 1280 max */
                width = 0x500;
            }
            tilemap->total_height = g_screen_height;
            height = g_screen_height;
            goto set_center;
        }
        tilemap->width = 0x400;              /* 1024 */
        width = 0x400;
    } else {
        tilemap->width = 0x400;              /* 1024 */
        width = 0x400;
    }
    tilemap->total_height = 0x300;            /* 768 */
    height = 0x300;

set_center:
    tilemap->scroll_x = 0;
    tilemap->scroll_y = 0;
    tilemap->center_x = width / 2;
    tilemap->total_width = width;
    tilemap->total_height = height;
    tilemap->viewport_x = 0;
    tilemap->viewport_y = 0;
    tilemap->center_y = height / 2;
    tilemap->viewport_center_x =
        (g_client_offset_x - g_client_width) / 2 + g_client_width;
    tilemap->viewport_center_y =
        (g_client_offset_y - g_client_height) / 2 + g_client_height;

    /* Calculate tile counts (round up) */
    tilemap->tile_count_x = (short)((width + (width >> 31 & 0xF)) >> 4);
    tilemap->tile_count_y = (short)((height + (height >> 31 & 0xF)) >> 4);

    /* Allocate occupancy bitmap */
    if (tilemap->occupancy_bitmap) {
        GLOBAL_free(tilemap->occupancy_bitmap);
        tilemap->occupancy_bitmap = NULL;
    }

    int tile_count = (int)tilemap->tile_count_x * (int)tilemap->tile_count_y;
    int bitmap_size = ((tile_count + (tile_count >> 31 & 7)) >> 3) + 1;
    tilemap->occupancy_bitmap = operator_new(bitmap_size);

    if (tilemap->occupancy_bitmap) {
        /* Fill with 0xFF (all dirty) */
        uint32_t* bitmap32 = (uint32_t*)tilemap->occupancy_bitmap;
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
/* TileMap_GetObjectAt                                                 */
/* Address: 0x455620                                                   */
/* ================================================================== */
void* __thiscall TileMap_GetObjectAt(TileMap* tilemap,
                                      short tile_x, short tile_y, short layer)
{
    if (tile_x < 0 || tile_x > 0x51 ||
        tile_y < 0 || tile_y > 0x41) {
        return 0;
    }

    size_t data_index = TILE_OFFSET(tile_x, tile_y, layer) - 0x48;
    return tilemap->ReadTilePointer(data_index);
}

/* ================================================================== */
/* TileMap_GetObjectAtEx                                               */
/* Address: 0x455670                                                   */
/*                                                                     */
/* Extended version: scans layers from highest active layer downward    */
/* (using the active-layer count byte at +0x80 within the tile entry)  */
/* and returns the first non-empty object.                             */
/* ================================================================== */
void* __thiscall TileMap_GetObjectAtEx(TileMap* tilemap,
                                        short tile_x, short tile_y,
                                        short* layer_out)
{
    if (tile_x < 0 || tile_x > 0x51 ||
        tile_y < 0 || tile_y > 0x42) {
        if (layer_out) *layer_out = -1;
        return 0;
    }

    int tile_index = (int)tile_x * 0x41 + (int)tile_y;

    /* Read the active layer count from byte at +0x80 within the tile entry */
    /* This byte tracks how many layers are in use; we scan downward from it */
    int active_layers = static_cast<int8_t>(
        tilemap->tile_data[(tile_index + 2) * 0x40 - 0x48]);
    if (active_layers < 0) {
        if (layer_out) *layer_out = -1;
        return 0;
    }

    /* Scan from highest active layer down to 0 */
    /* NOTE: The tile indexing here uses (layer + tile_index * 0x10) * 4 + 0x64
       which is a slightly different formula from TILE_OFFSET */
    for (int l = active_layers; l >= 0; l--) {
        void* obj = tilemap->ReadTilePointer(
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
/* TileMap_FindObjectByPos                                             */
/* Address: 0x4556F0                                                   */
/* ================================================================== */
void* __thiscall TileMap_FindObjectByPos(TileMap* tilemap,
                                          int pixel_x, int pixel_y)
{
    short tile_x = (pixel_x < 0) ? -1 : (short)(pixel_x >> 4);
    short tile_y = (pixel_y < 0) ? -1 : (short)(pixel_y >> 4);

    int tile_index = (int)tile_x * 0x41 + (int)tile_y;
    int tile_base = tile_index * 0x40;

    /* Active layer is at +0x80 within the tile entry */
    int active_layer = static_cast<int8_t>(
        tilemap->tile_data[tile_base + 0x80 - 0x48]);

    return tilemap->ReadTilePointer(tile_base + 0x64 + active_layer * 4 - 0x48);
}

/* ================================================================== */
/* TileMap_GetTileOrigin                                               */
/* Address: 0x455740                                                   */
/* ================================================================== */
void __thiscall TileMap_GetTileOrigin(TileMap* tilemap, int* out_id,
                                       short tile_x, short tile_y, short layer)
{
    /* Bounds check */
    if (tile_x < 0 || tile_x > 0x51 ||
        tile_y < 0 || tile_y > 0x41) {
        *out_id = -1;
        return;
    }

    /* Get object at tile */
    void* obj = TileMap_GetObjectAt(tilemap, tile_x, tile_y, layer);
    if (obj != 0) {
        /* Origin/resource ID is at +0x88 in the object struct */
        *out_id = static_cast<TileMapObject*>(obj)->tile_x;
        return;
    }

    *out_id = -1;
}

/* ================================================================== */
/* TileMap_IsTileOccupied                                              */
/* Address: 0x457B60 (__cdecl)                                         */
/*                                                                     */
/* Checks if two tile resources conflict. Uses type byte at +0x08:     */
/* 0x0C (12) = scenery, 0x03 (3) = track/building.                   */
/* Also checks RESDATA_IsSceneryTile and RESDATA_IsWaterTile.         */
/* Returns: 0x32 (50) = building-building conflict,                    */
/*          10 = building-scenery/water conflict,                      */
/*          -1 = no conflict.                                          */
/* ================================================================== */
int __cdecl TileMap_IsTileOccupied(int tile_resource_a, int tile_resource_b)
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
/* Address: 0x457C20 (__cdecl)                                         */
/*                                                                     */
/* Checks if tile_b is buildable adjacent to tile_a.                   */
/* Type codes: 3=track/building, 0x0C=scenery, 0x0D=other (road?).   */
/* Returns: 100=valid placement, 0x65=buildable but restricted,       */
/*          -1=blocked.                                                */
/* ================================================================== */
int __cdecl TileMap_IsTileBuildable(int tile_resource_a, int tile_resource_b)
{
    TileMapResource* resource_a = reinterpret_cast<TileMapResource*>(tile_resource_a);
    TileMapResource* resource_b = reinterpret_cast<TileMapResource*>(tile_resource_b);
    char type_a = resource_a->object_type;
    char type_b = resource_b->object_type;

    if (type_b == 0x03) {
        /* Building on road/path */
        if (RESDATA_IsTrackTile(tile_resource_b) &&
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
            int is_track = RESDATA_IsTrackTile(tile_resource_a);
            return is_track ? 0x65 : -1;
        }
        if (type_a == 0x0C || type_a == 0x0D) {
            return 100;
        }
    }

    return -1;
}

/* ================================================================== */
/* TileMap_InvalidateRect                                              */
/* Address: 0x455840                                                   */
/* ================================================================== */
void __thiscall TileMap_InvalidateRect(TileMap* tilemap, int left, int top,
                                        int right, int bottom)
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
    if (tile_right >= tilemap->tile_count_x)  tile_right = tilemap->tile_count_x - 1;
    if (tile_top < 0)    tile_top = 0;
    if (tile_bottom >= tilemap->tile_count_y) tile_bottom = tilemap->tile_count_y - 1;

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
/* TileMap_GetViewport                                                 */
/* Address: 0x4579D0                                                   */
/*                                                                     */
/* Gets the neighboring object in a given direction from a sprite.     */
/* Uses the sprite's sub-object position data stored in the resource   */
/* header at offsets +0x5FC/+0x600 (param_2-dependent).               */
/* If no neighbor defined, returns NULL. After finding a candidate,    */
/* checks distance is <= 17 pixels using Entity_GetSubObjectPosition   */
/* and Math_DistSquared.                                               */
/* ================================================================== */
void* __thiscall TileMap_GetViewport(TileMap* tilemap,
                                      void* sprite, int direction)
{
    TileMapResource* resource = static_cast<TileMapObject*>(sprite)->resource;
    if (resource == NULL) {
        return NULL;
    }

    /* Check if neighbor is defined for this direction. */
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

    /* Direction offsets:
       0 = up/north    (tile_y--)
       1 = right/east  (tile_x++)
       2 = down/south  (tile_y++)
       3 = left/west   (tile_x--)   */
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
    void* neighbor = tilemap->ReadTilePointer(
        (tile_x * 0x41 + (int)tile_y) * 0x40);

    /* Distance check: verify neighbor is close enough (<= 17 pixels) */
    if (neighbor != NULL && static_cast<TileMapObject*>(neighbor)->resource != NULL) {
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
/* TileMap_SetViewport                                                 */
/* Address: 0x4576B0                                                   */
/*                                                                     */
/* Evaluates 4 adjacent tiles (N/S/E/W) around a building sprite.     */
/* For each valid neighbor (via TileMap_GetViewport), checks if the   */
/* neighbor is buildable using TileMap_IsTileBuildable. Filters out    */
/* sprite-editor objects (INPUT_EditCharHandler).                     */
/* Counts valid neighbors with diagonal correction (opposite+adjacent  */
/* check). Returns count (0-4) or 1 if any neighbor has a station      */
/* resource (0xC50/0xC52).                                             */
/* ================================================================== */
char __thiscall TileMap_SetViewport(TileMap* tilemap, void* building_sprite)
{
    if (building_sprite == NULL) {
        return 0;
    }

    TileMapResource* resource = static_cast<TileMapObject*>(building_sprite)->resource;
    if (INPUT_EditCharHandler(reinterpret_cast<intptr_t>(resource))) {
        return 0;
    }

    /* Get viewport neighbors for all 4 directions */
    int neighbors[4] = {0, 0, 0, 0};

    for (int dir = 0; dir < 4; dir++) {
        void* neighbor = TileMap_GetViewport(tilemap, building_sprite, dir);
        neighbors[dir] = (int)neighbor;

        if (neighbor != NULL) {
            TileMapResource* res = static_cast<TileMapObject*>(neighbor)->resource;

            /* Skip sprite-editor objects */
            while (res != NULL && INPUT_EditCharHandler(reinterpret_cast<intptr_t>(res))) {
                neighbor = TileMap_GetViewport(tilemap, (void*)neighbors[dir], dir);
                neighbors[dir] = (int)neighbor;
                res = neighbor == NULL ? NULL : static_cast<TileMapObject*>(neighbor)->resource;
            }

            /* If neighbor exists but isn't buildable, discard it */
            if (neighbors[dir] != 0 &&
                TileMap_IsTileBuildable(reinterpret_cast<intptr_t>(resource),
                    reinterpret_cast<intptr_t>(res)) < 0) {
                neighbors[dir] = 0;
            }
        }
    }

    /* Count valid neighbors with diagonal correction:
       A neighbor counts if it exists AND either:
       - The adjacent neighbor (dir+1) also exists, OR
       - The opposite neighbor (dir-2) does NOT exist */
    char valid_count = 0;
    for (int dir = 0; dir < 4; dir++) {
        if (neighbors[dir] != 0 &&
            (neighbors[(dir + 1) & 3] != 0 || neighbors[(dir - 2) & 3] == 0)) {
            valid_count++;
        }
    }

    /* Check for station resources (0xC50 = station type, 0xC52 = other station type). */
    for (int dir = 0; dir < 4; ++dir) {
        if (neighbors[dir] != 0) {
            TileMapResource* neighbor_resource =
                reinterpret_cast<TileMapObject*>(neighbors[dir])->resource;
            int res_id = neighbor_resource == NULL ? -1 : neighbor_resource->resource_id;
            int expected = (dir & 1) == 0 ? 0xC50 : 0xC52;
            if (res_id == expected) return 1;
        }
    }

    return valid_count;
}

/* ================================================================== */
/* TileMap_UpdateViewport                                              */
/* Address: 0x4573E0                                                   */
/*                                                                     */
/* Like TileMap_SetViewport but for generic (non-building) sprites.    */
/* For type 7 (multi-track) sprites, checks the 2x2 tile neighborhood  */
/* and subtracts scenery (type 0x0C) tiles from the count.            */
/* ================================================================== */
char __thiscall TileMap_UpdateViewport(TileMap* tilemap,
                                        void* sprite, short sprite_type)
{
    if (sprite == NULL) {
        return 0;
    }

    TileMapResource* resource = static_cast<TileMapObject*>(sprite)->resource;
    if (INPUT_EditCharHandler(reinterpret_cast<intptr_t>(resource))) {
        return 0;
    }

    /* Get viewport neighbors for all 4 directions */
    int neighbors[4] = {0, 0, 0, 0};

    for (int dir = 0; dir < 4; dir++) {
        void* neighbor = TileMap_GetViewport(tilemap, sprite, dir);
        neighbors[dir] = (int)neighbor;

        if (neighbor != NULL) {
            TileMapResource* res = static_cast<TileMapObject*>(neighbor)->resource;

            /* Skip sprite-editor objects */
            while (res != NULL && INPUT_EditCharHandler(reinterpret_cast<intptr_t>(res))) {
                neighbor = TileMap_GetViewport(tilemap, (void*)neighbors[dir], dir);
                neighbors[dir] = (int)neighbor;
                res = neighbor == NULL ? NULL : static_cast<TileMapObject*>(neighbor)->resource;
            }

            /* If neighbor exists but is occupied (conflicts), discard it */
            if (neighbors[dir] != 0 &&
                TileMap_IsTileOccupied(reinterpret_cast<intptr_t>(resource),
                    reinterpret_cast<intptr_t>(res)) < 0) {
                neighbors[dir] = 0;
            }
        }
    }

    /* Count valid neighbors */
    char valid_count = 0;
    for (int dir = 0; dir < 4; dir++) {
        if (neighbors[dir] != 0 &&
            (neighbors[(dir + 1) & 3] != 0 || neighbors[(dir - 2) & 3] == 0)) {
            valid_count++;
        }
    }

    /* For type 7 (multi-track objects), check 2x2 tile neighborhood */
    if (sprite_type == 7 && valid_count == 4) {
        int tile_x = static_cast<TileMapObject*>(sprite)->tile_x;
        int tile_y = static_cast<TileMapObject*>(sprite)->tile_y;

        int corner[4];
        corner[0] = TileMap_GetObjectAt(tilemap,
            (short)(tile_x - 1), (short)(tile_y - 1), 0);
        corner[1] = TileMap_GetObjectAt(tilemap,
            (short)(tile_x - 1), (short)(tile_y + 1), 0);
        corner[2] = TileMap_GetObjectAt(tilemap,
            (short)(tile_x + 1), (short)(tile_y - 1), 0);
        corner[3] = TileMap_GetObjectAt(tilemap,
            (short)(tile_x + 1), (short)(tile_y + 1), 0);

        /* Subtract scenery tiles (type 0x0C) from valid count */
        for (int i = 0; i < 4; i++) {
            if (corner[i] != 0) {
                TileMapResource* corner_res =
                    reinterpret_cast<TileMapObject*>(corner[i])->resource;
                char corner_type = corner_res == NULL ? 0 : corner_res->object_type;
                if (corner_type == 0x0C) {
                    valid_count--;
                }
            }
        }
    }

    /* Check for station resources (0xC50/0xC52). */
    for (int dir = 0; dir < 4; ++dir) {
        if (neighbors[dir] != 0) {
            TileMapResource* neighbor_resource =
                reinterpret_cast<TileMapObject*>(neighbors[dir])->resource;
            int res_id = neighbor_resource == NULL ? -1 : neighbor_resource->resource_id;
            int expected = (dir & 1) == 0 ? 0xC50 : 0xC52;
            if (res_id == expected) return 1;
        }
    }

    return valid_count;
}

/* ================================================================== */
/* TileMap_GetTileRect                                                 */
/* Address: 0x457830                                                   */
/*                                                                     */
/* Fills fields at +0xD4..+0xE4 in the sprite with tile occupancy      */
/* data for each of the 4 directions. For each direction, it walks     */
/* the chain of connected tiles until a non-conflicting one is found.  */
/* Stores both the neighbor pointer at +0xD4+dir*4 and the occupancy   */
/* score at +0xE4+dir*4.                                              */
/* ================================================================== */
void __thiscall TileMap_GetTileRect(TileMap* tilemap, void* sprite)
{
    if (sprite == NULL) return;

    TileMapObject* object = static_cast<TileMapObject*>(sprite);
    TileMapResource* resource = object->resource;

    for (int dir = 0; dir < 4; dir++) {
        int32_t* neighbor_ptr = &object->occupancy_neighbors[dir];
        int32_t* score_ptr = &object->occupancy_scores[dir];

        /* Zero output fields */
        object->occupancy_links[dir] = 0;
        *score_ptr = 0;

        void* neighbor = TileMap_GetViewport(tilemap, sprite, dir);
        if (neighbor != NULL) {
            while (1) {
                TileMapResource* neighbor_res = static_cast<TileMapObject*>(neighbor)->resource;
                int occupancy = TileMap_IsTileOccupied(
                    reinterpret_cast<intptr_t>(resource), reinterpret_cast<intptr_t>(neighbor_res));
                if (occupancy < 0) break;

                if (!INPUT_EditCharHandler(neighbor_res)) {
                    *neighbor_ptr = (int)neighbor;
                }
                *score_ptr += occupancy;

                /* Walk to next connected tile if the current one has a chain */
                if (static_cast<TileMapObject*>(neighbor)->occupancy_scores[0] < 0) break;
                neighbor = TileMap_GetViewport(tilemap, neighbor, dir);
                if (neighbor == NULL) break;
            }
        }
    }
}

/* ================================================================== */
/* TileMap_GetTileAt                                                   */
/* Address: 0x457900                                                   */
/*                                                                     */
/* Like GetTileRect but uses TileMap_IsTileBuildable instead of        */
/* IsTileOccupied. Fills fields at +0xF8..+0x108 with buildability     */
/* data for each direction.                                            */
/* ================================================================== */
void __thiscall TileMap_GetTileAt(TileMap* tilemap, void* sprite)
{
    if (sprite == NULL) return;

    TileMapObject* object = static_cast<TileMapObject*>(sprite);
    TileMapResource* resource = object->resource;

    for (int dir = 0; dir < 4; dir++) {
        int32_t* neighbor_ptr = &object->build_links[dir];
        int32_t* score_ptr = &object->build_scores[dir];

        /* Zero output fields */
        object->occupancy_scores[dir + 1] = 0;
        *score_ptr = 0;

        void* neighbor = TileMap_GetViewport(tilemap, sprite, dir);
        if (neighbor != NULL) {
            while (1) {
                TileMapResource* neighbor_res = static_cast<TileMapObject*>(neighbor)->resource;
                int buildable = TileMap_IsTileBuildable(
                    reinterpret_cast<intptr_t>(resource), reinterpret_cast<intptr_t>(neighbor_res));
                if (buildable < 0) break;

                if (!INPUT_EditCharHandler(neighbor_res)) {
                    *neighbor_ptr = (int)neighbor;
                }
                *score_ptr += buildable;

                if (static_cast<TileMapObject*>(neighbor)->build_scores[0] < 0) break;
                neighbor = TileMap_GetViewport(tilemap, neighbor, dir);
                if (neighbor == NULL) break;
            }
        }
    }
}

/* ================================================================== */
/* TileMap_ClearInputProcessedFlag                                     */
/* Address: 0x456140                                                   */
/* ================================================================== */
void __thiscall TileMap_ClearInputProcessedFlag(TileMap* tilemap)
{
    tilemap->scroll_drag_active = 0;
}

/* ================================================================== */
/* TileMap_ScrollRect                                                  */
/* Address: 0x4553E0                                                   */
/*                                                                     */
/* Validates placement of a target building at a grid offset from its  */
/* current position. Checks road tile category, then iterates the      */
/* 3D occupancy grid (w x h x d) of the target building to verify     */
/* that no blocking objects are in the way. Plays placement sound and  */
/* calls TileMap_ScrollTo if valid and use_sound is set.              */
/* Returns 1 if valid, 0 if out of bounds or blocked.                 */
/* ================================================================== */
char __thiscall TileMap_ScrollRect(TileMap* tilemap,
    char use_sound, void* target_building, short delta_x, ushort delta_y,
    int unknown_param)
{
    TileMapObject* building = static_cast<TileMapObject*>(target_building);
    short grid_w = building->grid_width;
    short grid_h = building->grid_height;

    /* Bounds check: target position + grid size must be within tilemap */
    if ((int)grid_w + (int)delta_x > (int)tilemap->tile_count_x ||
        (int)grid_h + (int)delta_y > (int)tilemap->tile_count_y) {
        return 0;
    }

    char valid = 1;

    /* Check road tile compatibility */
    if (building->object_type == 0x03) {
        if (RESDATA_IsRoadTile((int)target_building)) {
            uint category = RESDATA_GetTileCategory(
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
                            int obj = tilemap->ReadTileValue(
                                TILE_OFFSET(tile_x, tile_y, iz) - 0x48);
                            if (obj != 0) {
                                if (g_allow_building_placement == 1 &&
                                    reinterpret_cast<TileMapObject*>(obj)->is_moving == 1 &&
                                    (g_disable_input == 0 ||
                                     g_game_mode == 3 ||
                                     g_game_mode == 1)) {
                                    if (use_sound) {
                                        PlaySoundAt(0x5024,
                                            (int)tile_x << 4,
                                            (int)tile_y << 4, 4);
                                    }
                                    void* ptr = tilemap->ReadTilePointer(
                                        TILE_OFFSET(
                                            (short)(iy + (short)delta_x),
                                            (short)(ix + (short)delta_y),
                                            iz) - 0x48);
                                    if (ptr) {
                                        TileMap_ScrollTo(tilemap, ptr,
                                                          unknown_param);
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
/* TileMap_ScrollTo                                                    */
/* Address: 0x455AB0                                                   */
/*                                                                     */
/* Removes object from the tile grid. For each tile occupied by the    */
/* object (based on its resource grid dimensions), sets the tile data  */
/* entry to 0 and updates the occupancy bitmap. The function handles   */
/* two phases: first clears the "original" tile area (based on the     */
/* object's tile positions + resource dimensions), then clears the     */
/* "destination" area. Finally removes the object from INPUT manager.  */
/* Returns 1 on success.                                              */
/* ================================================================== */
void* __thiscall TileMap_ScrollTo(TileMap* tilemap, void* target, int scroll_flag)
{
    if (target == NULL) {
        return NULL;
    }

    TileMapObject* object = static_cast<TileMapObject*>(target);

    /* Check the "is_moving" flag at +0xC0 */
    if (object->is_moving == 0) {
        return NULL;
    }

    /* Check type at +0x06 */
    if (object->object_state != 1) {
        return NULL;
    }

    TileMapResource* resource = object->resource;
    short tile_x = object->tile_x;
    short tile_y = object->tile_y;

    byte grid_h = resource->grid_span_y;
    byte grid_w = resource->grid_width;

    /* Phase 1: Clear original tile area (scrolling outward) */
    int end_x = (int)tile_y + (int)(ushort)(tile_y + (ushort)resource->original_span);
    if ((int)tile_y < end_x) {
        for (short cur_y = tile_y; cur_y < (short)end_x; cur_y++) {
            int end_y = (int)tile_x + (int)(ushort)((ushort)grid_h + tile_x);
            for (short cur_x = tile_x; cur_x < (short)end_y; cur_x++) {
                int tile_idx = (int)cur_y + (int)cur_x * 0x41;

                /* Scan layers 6 down to -1 */
                for (int layer = 6; layer >= 0; layer--) {
                    size_t tile_base = (tile_idx + 2) * 0x40 - 0x48;
                    if (tilemap->ReadTileValue(tile_base + layer * 4) ==
                        static_cast<int32_t>(reinterpret_cast<uintptr_t>(target))) {
                        int8_t& active_count =
                            reinterpret_cast<int8_t&>(tilemap->tile_data[tile_base]);
                        if (active_count == static_cast<int8_t>(layer)) {
                            --active_count;
                        }
                    }
                }
            }
        }
    }
}