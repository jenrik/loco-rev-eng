// Status: TRANSCRIBED
#ifndef TILEMAP_H
#define TILEMAP_H

/**
 * tilemap.h — TileMap class and associated types
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The TileMap (g_tilemap, 0x4AAD08) is a large global object (~0x52514
 * bytes) that manages the game world's tile grid. Each tile is a 16x16
 * pixel cell storing up to 4 overlapped layers (base terrain, track,
 * scenery, vehicles/buildings).
 *
 * Tile addressing:
 *   this + 0x48 + (tile_x * 65 + tile_y) * 0x40 + layer * 4
 *   Values: -1 = empty, non-negative = pointer to object at tile
 *
 * The class also includes scroll/offset state for rendering, a
 * dirty-rect tracking bitmap, DDraw surface lock buffer, and asset
 * loading state.
 *
 * Size: ~0x52514 bytes (static global)
 * Global: g_tilemap at 0x4AAD08
 */

#pragma once

#include "../shared/types.h"
#include <new>

/* ================================================================== */
/* Global address                                                      */
/* ================================================================== */
#define ADDR_g_tilemap                    0x004AAD08  /* TileMap singleton */

/* ================================================================== */
/* Resource and object views used by TileMap routines. Every member is */
/* named from an observed binary offset; these replace decompiler-style*/
/* byte-pointer arithmetic.                                            */
/* ================================================================== */
struct TileMapResource {
    uint8_t _pad_00[4];
    int32_t resource_id;                 /* +0x04 */
    uint8_t object_type;                 /* +0x08 */
    uint8_t _pad_09[0x15F];
    uint8_t grid_width;                  /* +0x168 */
    uint8_t grid_height;                 /* +0x169 */
    int8_t  grid_depth;                  /* +0x16A */
    uint8_t grid_span_y;                 /* +0x16B */
    uint8_t original_span;               /* +0x16C */
    uint8_t _pad_16D[0x48F];
    int32_t neighbor_def[4][2];          /* +0x5FC */
};

struct TileMapObject {
    uint8_t _pad_00[6];
    uint8_t object_state;                /* +0x06 */
    uint8_t _pad_07;
    uint8_t object_type;                 /* +0x08 */
    uint8_t _pad_09[0x37];
    TileMapResource* resource;           /* +0x40 */
    uint8_t _pad_44[0x44];
    int16_t tile_x;                      /* +0x88 */
    int16_t tile_y;                      /* +0x8A */
    uint8_t _pad_8C[0x34];
    uint8_t is_moving;                   /* +0xC0 */
    uint8_t _pad_C1[3];
    int32_t occupancy_links[4];          /* +0xC4 */
    int32_t occupancy_neighbors[4];      /* +0xD4 */
    int32_t occupancy_scores[5];         /* +0xE4 */
    int32_t build_links[4];              /* +0xF8 */
    int32_t build_scores[5];             /* +0x108 */
    uint8_t _pad_11C[0x4C];
    uint8_t grid_width;                  /* +0x168 */
    uint8_t grid_height;                 /* +0x169 */
    int8_t  grid_depth;                  /* +0x16A */
    uint8_t _pad_16B[3];
    int8_t  occupancy_grid[9 * 7];       /* +0x16E */
};

/* ================================================================== */
/* Tile-entry structure (0x40 bytes each, in tile_data array)          */
/* ================================================================== */
typedef struct TileEntry {
    int32_t     layers[16];             /* +0x00  up to 16 layer slots */
} TileEntry;

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */
struct RESDATA;

/* ================================================================== */
/* Callback type for FindObject                                       */
/* ================================================================== */
typedef int (*TileMap_FindObjCallback)(int tile_resource_id, int target_resource_id);

/* ================================================================== */
/* TileMap class — tile grid + scroll state + all operations          */
/* ================================================================== */
class TileMap {
public:
    /** Constructor — Address: 0x454CF0 */
    TileMap();

    /** Destructor — Address: 0x454DE0; virtual slot [0], table at 0x478520 */
    virtual ~TileMap();

    /* ---- Typed accessors for 32-bit tile slots ---- */
    void*   ReadTilePointer(size_t data_index) const;
    int32_t ReadTileValue(size_t data_index) const;
    void    WriteTileValue(size_t data_index, int32_t value);

    /* ---- Lifecycle ---- */

    /** Initialize dimensions and occupancy bitmap — Address: 0x454E60 */
    void Init(char use_1024x768);

    /** Update all tile assets (async load) — Address: 0x457320 */
    void UpdateAll();

    /** Full reset: deselect, world init, clear grid — Address: 0x454FE0 */
    void FullReset();

    /** Recalculate viewport center from client area — Address: 0x454FA0 */
    void RecalcViewportCenter();

    /* ---- Tile queries ---- */

    /** Get object at tile (x,y,layer) — Address: 0x455620 */
    void* GetObjectAt(short tile_x, short tile_y, short layer);

    /** Get object at tile with active-layer scan — Address: 0x455670 */
    void* GetObjectAtEx(short tile_x, short tile_y, short* layer_out);

    /** Get tile origin resource ID — Address: 0x455740 */
    int* GetTileOrigin(int* out_id, short tile_x, short tile_y, short layer);

    /** Get tile origin (explicit layer, returns via out parameter) — Address: 0x4557C0 */
    void GetTileOriginEx(int* out_packed, short tile_x, short tile_y, short layer);

    /** Find object by resource ID near tile — Address: 0x4550C0 */
    int* FindObject(int target_resource_id, short tile_x, short tile_y,
                    char unknown, int mode);

    /** Find object by tile position — Address: 0x4556F0 */
    void* FindObjectByPos(int pixel_x, int pixel_y);

    /* ---- Viewport / scroll ---- */

    /** Get neighboring object in a direction — Address: 0x4579D0 */
    TileMapObject* GetViewport(TileMapObject* sprite, int direction);

    /** Count valid adjacent tiles for building — Address: 0x4576B0 */
    char SetViewport(TileMapObject* building_sprite);

    /** Count valid adjacent tiles (generic sprite) — Address: 0x4573E0 */
    char UpdateViewport(TileMapObject* sprite, short sprite_type);

    /** Scroll by delta — Address: 0x455960 */
    uint Scroll(int delta_x, int delta_y, int drag_start_x, int drag_start_y);

    /** Scroll to target object position — Address: 0x455AB0 */
    void* ScrollTo(TileMapObject* target, int scroll_flag);

    /** Validate placement rect scroll — Address: 0x4553E0 */
    char ScrollRect(char use_sound, TileMapObject* target_building,
                    short delta_x, ushort delta_y, int placement_mode);

    /* ---- Tile occupancy / buildability queries ---- */

    /** Get tile occupancy for all 4 directions — Address: 0x457830 */
    void GetTileRect(TileMapObject* sprite);

    /** Get tile buildability for all 4 directions — Address: 0x457900 */
    void GetTileAt(TileMapObject* sprite);

    /* ---- Input handling ---- */

    /** Handle mouse click — Address: 0x455D60 */
    char HandleClick(int screen_x, int screen_y);

    /** Clear input-processed flag — Address: 0x456140 */
    void ClearInputProcessedFlag();

    /* ---- Dirty-rect / rendering ---- */

    /** Mark tiles in rect as dirty — Address: 0x455840 */
    void InvalidateRect(int left, int top, int right, int bottom);

    /** Process all dirty rects for rendering — Address: 0x456150 */
    void InvalidateDirtyRects(char force_all);

    /** Blit a single dirty rect — Address: 0x456700 */
    void ProcessRect(int left, int top, int right, int bottom);

    /** Process object timer events — Address: 0x456D90 */
    uint ProcessObjectTimer(TileMapObject* param_object);

    /* ---- Header fields (0x00..0x47) ---- */
    /* +0x00 compiler-managed vptr */
    int32_t     _pad_01;                /* +0x04 */
    int32_t     width;                  /* +0x08  world width in pixels */
    int32_t     scroll_x;               /* +0x0C  current scroll offset X */
    int32_t     scroll_y;               /* +0x10  current scroll offset Y */
    int32_t     total_width;            /* +0x14  total map width */
    int32_t     total_height;           /* +0x18  total map height */
    int32_t     viewport_x;             /* +0x1C  viewport offset X (world space) */
    int32_t     viewport_y;             /* +0x20  viewport offset Y (world space) */
    int32_t     center_x;               /* +0x24  center X (width/2) */
    int32_t     center_y;               /* +0x28  center Y (height/2) */
    int32_t     viewport_center_x;      /* +0x2C  viewport center X */
    int32_t     viewport_center_y;      /* +0x30  viewport center Y */
    int32_t     drag_start_x;           /* +0x34  drag scroll start X */
    int32_t     drag_start_y;           /* +0x38  drag scroll start Y */
    uint8_t     scroll_drag_active;     /* +0x3C  1=currently scrolling via drag */
    uint8_t     _pad_3D;                /* +0x3D */
    int16_t     tile_count_x;           /* +0x3E  number of tile columns (width/16) */
    int16_t     tile_count_y;           /* +0x40  number of tile rows (height/16) */
    uint8_t     _pad_42[6];             /* +0x42..+0x47 */

    /* ---- Tile data array (+0x48..+0x52483) ---- */
    /* Each tile is 0x40 (64) bytes, organized as:
     *   tile[tile_x * 65 + tile_y] at (0x48 + (tile_x*65+tile_y) * 0x40)
     *   layer[layer] within tile at +layer*4
     *   Value: -1 = empty, non-negative = object pointer
     * Max tiles: 82 columns (x) x 66 rows (y) = 5412 tiles
     */
    uint8_t     tile_data[0x5243C];     /* +0x48  tile grid */

    /* ---- Misc pointers at end ---- */
    void*       occupancy_bitmap;       /* +0x52484  bitmap: 1 bit per tile */
    void*       asset_load_ptr;         /* +0x52488  asset loading context */
    void*       asset_enum_ptr;         /* +0x5248C  asset enumeration context */
    uint8_t     update_complete;        /* +0x52490  async asset loading flag */

    /* ---- DDraw surface lock buffer ---- */
    uint8_t     ddsurfacedesc_buf[0x7C];/* +0x52494  DDSURFACEDESC for Lock/Unlock */

    uint8_t     surface_locked;         /* +0x52510  1=surface currently locked */
    uint8_t     _pad_52511[3];          /* +0x52511 */

    /* Total ~0x52514 bytes */
};

/* ================================================================== */
/* Global TileMap instance                                             */
/* ================================================================== */
extern TileMap g_tilemap;               /* at 0x4AAD08 */

/* ================================================================== */
/* Static / free functions (no TileMap* this pointer)                 */
/* ================================================================== */

/** Check if two tile resources conflict — Address: 0x457B60 */
int TileMap_IsTileOccupied(int tile_resource_a, int tile_resource_b);

/** Check if tile_b is buildable next to tile_a — Address: 0x457C20 */
int TileMap_IsTileBuildable(int tile_resource_a, int tile_resource_b);

/** Process dirty rect linked list (merge overlapping) — Address: 0x456C60 */
char TileMap_ProcessDirtyRects(RECT* rect_list);

/** Clip dirty rects to viewport — Address: 0x456D10 */
void TileMap_FreeDirtyRects(RECT* rect_list);

/** Find nearest object of a type — Address: 0x457CE0 */
void* TileMap_FindNearestObject(TileMap* tilemap, ushort type_filter,
                                 int target_x, int target_y,
                                 int search_radius);

/** Create overlay for build placement preview — Address: 0x457080 */
void* TileMap_CreateOverlay(TileMap* tilemap, int resource_id, byte param_2);

/** Convert world pixel coordinates to screen coordinates — Address: 0x458270 */
void TileMap_WorldToScreen(void* output_coords);

/* ================================================================== */
/* External globals referenced by TileMap                              */
/* ================================================================== */
extern int32_t  g_game_mode;            /* 0x004851F4 */
extern int32_t  g_screen_width;         /* 0x004851D8 */
extern int32_t  g_screen_height;        /* 0x00485214 */
extern int32_t  g_client_offset_x;
extern int32_t  g_client_offset_y;
extern int32_t  g_client_width;
extern int32_t  g_client_height;
extern uint8_t  g_is_fullscreen;
extern int32_t  g_world_width;
extern uint8_t  g_is_town_mode;
extern int32_t  g_town_overlay_rect;
extern int32_t  g_town_overlay_left;    /* 0x485390 */
extern int32_t  g_town_overlay_top;     /* 0x485394 */
extern int32_t  g_town_overlay_right;   /* 0x485398 */
extern uint8_t  g_build_mode;
extern uint8_t  g_click_on_building;
extern uint8_t  g_placement_valid;
extern uint8_t  g_placement_blocked;
extern int32_t  g_placement_resource_id;
extern uint8_t  g_disable_input;
extern uint8_t  g_click_on_town;
extern void*    g_cursor_surface;
extern void*    g_primary_surface;
extern void*    g_input_mgr;
extern int32_t  g_town_selection_rect_left;
extern int32_t  g_town_selection_rect_top;
extern int32_t  g_town_selection_rect_right;
extern int32_t  g_town_selection_rect_bottom;
extern uint8_t  g_has_selection;
extern int32_t  g_selected_building;
extern void*    g_town_view;
extern void*    g_ddraw_building;
extern void*    g_about;
extern void*    g_netman;
extern void*    g_tile_occupied_bitmap;
extern int32_t  g_player_id;
extern int32_t  g_viewport_x;
extern uint8_t  g_lock_update_flag;     /* 0x4851F0 */
extern uint8_t  g_allow_building_placement;  /* 0x485328 */
extern uint8_t  g_player_color;
extern HWND     g_main_window;          /* from types.h */

/* ================================================================== */
/* External function declarations (cross-module calls)                 */
/* ================================================================== */
extern int      WIN32_GetThreadResult(int param);
extern void     AssetMgr_LoadFileEx(uint* ptr);
extern void     AssetMgr_EnumFiles(uint* ptr);
extern void     World_Lock(void* world);
extern void     World_Unlock(void* world);
extern void     UIPANEL_Blit(void* src, int sx, int sy, int sw, int sh,
                              void* dst, int dx, int dy, int dw, int dh, int flags);
extern void     DDRAW_PresentRect(RECT* rect, HWND hwnd,
                                   int32_t* viewport_x, char flag);
extern void     GLOBAL_free(void* ptr);
extern void*    operator_new(size_t size);
extern int      INPUT_EditCharHandler(int ptr);
extern int      TileData_IsSceneryTile(int ptr);    /* @ 0x45AAF0 */
extern int      TileData_IsWaterTile(int ptr);      /* @ 0x45AB70 */
extern int      TileData_IsTrackTile(int ptr);      /* @ 0x45AB30 */
extern int      TileData_IsRoadTile(int ptr);       /* @ 0x45AAF0 */
extern int      TileData_GetTileCategory(void* ptr, short a, ushort b);
extern int      IntersectRect(RECT* dst, RECT* a, RECT* b);
extern int      UnionRect(RECT* dst, RECT* a, RECT* b);
extern int      SetRectEmpty(RECT* rect);
extern void     SetRect(RECT* rect, int left, int top, int right, int bottom);
extern void     CopyRect(RECT* dst, RECT* src);
extern int      PtInRect(RECT* rect, int x, int y);
extern void     InflateRect(RECT* rect, int dx, int dy);
extern int      Town_BlitViewport(void* res, int src_x, int src_y,
                                   int src_w, int src_h,
                                   int dst_x, int dst_y);
extern void     PlaySoundAt(int sound_id, int x, int y, int channel);
extern void     Town_SelectBuilding(void* town_view, int building);
extern void     DDRAW_SelectBuilding(void* ddraw_building, int building);
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
extern void*    ResourceManager_GetById(void** resmgr, UINT id);
extern void     UIPANEL_InitSurface(void* surface, int w, int h,
                                     int a, int b, byte c);
extern void     OutputDebugStringA(const char* str);
extern void     Sleep(uint32_t ms);
extern BOOL     InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
extern BOOL     UpdateWindow(HWND hWnd);

/* Bitmask lookup table */
extern uint8_t  ATTR_0047f108[8];       /* bitmask lookup (1<<n) */

/* ================================================================== */
/* Backward-compatible inline wrappers (callers not yet converted)     */
/* These delegate to the TileMap methods above. Remove after all       */
/* callers have been updated.                                          */
/* ================================================================== */
inline void     TileMap_Init(TileMap* tm, char use_1024x768)        { tm->Init(use_1024x768); }
inline void     TileMap_UpdateAll(TileMap* tm)                      { tm->UpdateAll(); }
inline void*    TileMap_GetObjectAt(TileMap* tm, short x, short y, short l) { return tm->GetObjectAt(x, y, l); }
inline void*    TileMap_GetObjectAtEx(TileMap* tm, short x, short y, short* lo) { return tm->GetObjectAtEx(x, y, lo); }
inline int*     TileMap_GetTileOrigin(TileMap* tm, int* out_id, short x, short y, short l) { return tm->GetTileOrigin(out_id, x, y, l); }
inline void     TileMap_GetTileOriginEx(TileMap* tm, int* out_p, short x, short y, short l) { tm->GetTileOriginEx(out_p, x, y, l); }
inline void     TileMap_GetTileRect(TileMap* tm, TileMapObject* s)  { tm->GetTileRect(s); }
inline void     TileMap_GetTileAt(TileMap* tm, TileMapObject* s)    { tm->GetTileAt(s); }
inline void*    TileMap_GetViewport(TileMap* tm, TileMapObject* s, int d) { return tm->GetViewport(s, d); }
inline char     TileMap_SetViewport(TileMap* tm, TileMapObject* bs) { return tm->SetViewport(bs); }
inline char     TileMap_UpdateViewport(TileMap* tm, TileMapObject* s, short t) { return tm->UpdateViewport(s, t); }
inline void*    TileMap_ScrollTo(TileMap* tm, TileMapObject* t, int f) { return tm->ScrollTo(t, f); }
inline char     TileMap_ScrollRect(TileMap* tm, char snd, TileMapObject* b, short dx, ushort dy, int pm) { return tm->ScrollRect(snd, b, dx, dy, pm); }
inline char     TileMap_HandleClick(TileMap* tm, int sx, int sy)    { return tm->HandleClick(sx, sy); }
inline void     TileMap_ClearInputProcessedFlag(TileMap* tm)        { tm->ClearInputProcessedFlag(); }
inline void     TileMap_InvalidateRect(TileMap* tm, int l, int t, int r, int b) { tm->InvalidateRect(l, t, r, b); }
inline void     TileMap_InvalidateDirtyRects(TileMap* tm, char fa)  { tm->InvalidateDirtyRects(fa); }
inline void     TileMap_ProcessRect(TileMap* tm, int l, int t, int r, int b) { tm->ProcessRect(l, t, r, b); }
inline uint     TileMap_ProcessObjectTimer(TileMap* tm, TileMapObject* obj) { return tm->ProcessObjectTimer(obj); }
inline int*     TileMap_FindObject(TileMap* tm, int id, short x, short y, char u, int m) { return tm->FindObject(id, x, y, u, m); }
inline void*    TileMap_FindObjectByPos(TileMap* tm, int px, int py) { return tm->FindObjectByPos(px, py); }
inline void     TileMap_Scroll(TileMap* tm, int dx, int dy, int dsx, int dsy) { tm->Scroll(dx, dy, dsx, dsy); }

/* Sprite_* backward compat (callers should use TileMap ctor/dtor/methods) */
inline void*    Sprite_Create(TileMap* tm)    { return new (tm) TileMap(); }
inline void     Sprite_Shutdown(TileMap* tm)  { tm->~TileMap(); }
inline void     Sprite_LockAll(TileMap* tm)   { tm->RecalcViewportCenter(); }
inline void     Sprite_UnlockAll(TileMap* tm) { tm->FullReset(); }

#endif /* TILEMAP_H */
