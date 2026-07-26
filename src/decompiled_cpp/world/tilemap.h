#ifndef TILEMAP_H
#define TILEMAP_H

/**
 * tilemap.h — TileMap structure and associated free functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The TileMap (g_tilemap, 0x4AAD08) is a large global struct (~0x52514
 * bytes) that manages the game world's tile grid. Each tile is a 16x16
 * pixel cell storing up to 4 overlapped layers (base terrain, track,
 * scenery, vehicles/buildings).
 *
 * Tile addressing:
 *   g_tilemap + 0x48 + (tile_x * 65 + tile_y) * 0x40 + layer * 4
 *   Values: -1 = empty, non-negative = pointer to object at tile
 *
 * The struct also includes a scroll/offset state for rendering, a
 * dirty-rect tracking bitmap, DDraw surface lock buffer, and asset
 * loading state.
 *
 * Size: ~0x52514 bytes (static global)
 * Global: g_tilemap at 0x4AAD08
 */

#pragma once

#include "../shared/types.h"

/* ================================================================== */
/* Global address                                                      */
/* ================================================================== */
#define ADDR_g_tilemap                    0x004AAD08  /* TileMap singleton */

/* ================================================================== */
/* TileMap struct — tile grid + scroll state                           */
/* ================================================================== */
class TileMap {
public:
    /** Constructor body: 0x454CF0. */
    TileMap();

    /** Destructor body: 0x454DE0; virtual slot [0], table at 0x478520. */
    virtual ~TileMap();

    /* Typed accessors for the binary's 32-bit tile slots. The argument is
       an index within tile_data, never an offset from this. */
    void* ReadTilePointer(size_t data_index) const;
    int32_t ReadTileValue(size_t data_index) const;
    void WriteTileValue(size_t data_index, int32_t value);

    /* ---- Header (0x00..0x47) ---- */
    /* +0x00 compiler-managed vptr */
    int32_t     _pad_01;                /* +0x04 */
    int32_t     width;                  /* +0x08  world width in pixels (1024 or screen) */
    int32_t     scroll_x;               /* +0x0C  current scroll offset X */
    int32_t     scroll_y;               /* +0x10  current scroll offset Y */
    int32_t     total_width;            /* +0x14  total map width (same as width) */
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

    /* ---- Misc pointers at end of struct ---- */
    void*       occupancy_bitmap;       /* +0x52484  bitmap: 1 bit per tile (allocated) */
    void*       asset_load_ptr;         /* +0x52488  asset loading context */
    void*       asset_enum_ptr;         /* +0x5248C  asset enumeration context */
    uint8_t     update_complete;        /* +0x52490  flag for async asset loading */

    /* ---- DDraw surface lock buffer ---- */
    uint8_t     ddsurfacedesc_buf[0x7C];/* +0x52494  DDSURFACEDESC for Lock/Unlock */

    uint8_t     surface_locked;         /* +0x52510  1=surface currently locked */
    uint8_t     _pad_52511[3];          /* +0x52511 */

    /* Total ~0x52514 bytes */
};

/* Resource and object views used by TileMap routines. Every member is
 * named from an observed binary offset; these replace decompiler-style
 * byte-pointer arithmetic. */
struct TileMapResource {
    uint8_t _pad_00[4];
    int32_t resource_id;                 /* +0x04 */
    uint8_t object_type;                 /* +0x08 */
    uint8_t _pad_09[0x15F];
    uint8_t grid_width;                  /* +0x168 */
    uint8_t grid_height;                 /* +0x169 */
    int8_t grid_depth;                   /* +0x16A */
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
    int8_t grid_depth;                   /* +0x16A */
    uint8_t _pad_16B[3];
    int8_t occupancy_grid[9 * 7];        /* +0x16E */
};

/* ================================================================== */
/* Tile-entry structure (0x40 bytes each, in tile_data array)          */
/* ================================================================== */
typedef struct TileEntry {
    /* Array of 16 int32_t layers (-1 = empty, otherwise pointer to object) */
    int32_t     layers[16];             /* +0x00  up to 16 layer slots */

    /* Total: 0x40 bytes */
} TileEntry;

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */
struct RESDATA;

/* ================================================================== */
/* External globals referenced by TileMap functions                    */
/* ================================================================== */
extern int32_t  g_game_mode;            /* 0x004851F4 */
extern int32_t  g_screen_width;         /* 0x004851D8 */
extern int32_t  g_screen_height;        /* 0x00485214 */
extern int32_t  g_client_offset_x;      /* game window client offset X */
extern int32_t  g_client_offset_y;      /* game window client offset Y */
extern int32_t  g_client_width;         /* game window client width */
extern int32_t  g_client_height;        /* game window client height */
extern uint8_t  g_is_fullscreen;        /* fullscreen flag */
extern int32_t  g_world_width;          /* world width in pixels */
extern uint8_t  g_is_town_mode;         /* town mode flag */
extern int32_t  g_town_overlay_rect;    /* town overlay rect (packed) */
extern uint8_t  g_build_mode;           /* build mode (1=normal, 2=placement) */
extern uint8_t  g_click_on_building;    /* click-on-building flag */
extern uint8_t  g_placement_valid;      /* placement valid flag */
extern uint8_t  g_placement_blocked;    /* placement blocked flag */
extern int32_t  g_placement_resource_id;/* build placement resource ID */
extern uint8_t  g_disable_input;        /* input disabled flag */
extern uint8_t  g_click_on_town;        /* click-on-town flag */
extern void*    g_cursor_surface;       /* cursor DDraw surface */
extern void*    g_primary_surface;      /* primary DDraw surface */
extern void*    g_input_mgr;            /* INPUT manager */
extern int32_t  g_town_selection_rect_left;
extern int32_t  g_town_selection_rect_top;
extern int32_t  g_town_selection_rect_right;
extern int32_t  g_town_selection_rect_bottom;
extern uint8_t  g_has_selection;        /* selection flag */
extern int32_t  g_selected_building;    /* currently selected building ptr */
extern void*    g_town_view;            /* town view singleton */
extern void*    g_ddraw_building;       /* DDRAW building singleton */
extern void*    g_about;                /* AboutDialog singleton */
extern void*    g_netman;               /* NetMan singleton */
extern void*    g_tile_occupied_bitmap; /* world tile occupancy bitmap */
extern int32_t  g_player_id;            /* current player ID */
extern int32_t  g_viewport_x;           /* viewport X */
extern uint8_t  DAT_004851f0;           /* lock/update flag at 0x4851F0 */
extern uint8_t  g_allow_building_placement;  /* 0x485328 */
extern uint8_t  g_player_color;

/* ================================================================== */
/* External function declarations                                      */
/* ================================================================== */
extern void     WIN32_GetThreadResult(int param);        /* @ 0x466570 */
extern void     AssetMgr_LoadFileEx(uint* ptr);          /* @ 0x457110 */
extern void     AssetMgr_EnumFiles(uint* ptr);           /* @ 0x457170 */
extern void     CDECL World_Lock(void* world);           /* @ 0x44E200 */
extern void     CDECL World_Unlock(void* world);         /* @ 0x44E2D0 */
extern void     UIPANEL_Blit(void* src, int sx, int sy, int sw, int sh,
                              void* dst, int dx, int dy, int dw, int dh, int flags); /* @ 0x421740 */
extern void     DDRAW_PresentRect(RECT* rect, HWND hwnd,
                                   int32_t* viewport_x, char flag); /* @ 0x462150 */
extern void     GLOBAL_free(void* ptr);                  /* @ 0x465E10 */
extern void*    operator_new(size_t size);               /* operator new */
extern int      INPUT_EditCharHandler(int ptr);          /* @ 0x41E4B0 */
extern int      RESDATA_IsSceneryTile(int ptr);          /* @ 0x45AAF0 */
extern int      RESDATA_IsWaterTile(int ptr);            /* @ 0x45AB70 */
extern int      RESDATA_IsTrackTile(int ptr);            /* @ 0x45AB30 */
extern int      IntersectRect(RECT* dst, RECT* a, RECT* b);
extern int      UnionRect(RECT* dst, RECT* a, RECT* b);
extern int      SetRectEmpty(RECT* rect);
extern void     SetRect(RECT* rect, int left, int top, int right, int bottom);
extern int      CDECL Town_BlitViewport(void* res, int src_x, int src_y,
                                         int src_w, int src_h,
                                         int dst_x, int dst_y); /* @ 0x42F470 */
extern void     CDECL PlaySoundAt(int sound_id, int x,
                                   int y, int channel); /* @ 0x463800 */
extern void     Town_SelectBuilding(void* town_view, int building); /* @ 0x42C9C0 */
extern void     DDRAW_SelectBuilding(void* ddraw_building, int building); /* @ 0x46AA80 */

/* Callback function type for TileMap_FindObject */
typedef int (*TileMap_FindObjCallback)(int tile_resource_id, int target_resource_id);

/* ================================================================== */
/* TileMap lifecycle functions                                        */
/* ================================================================== */

/**
 * Initialize TileMap dimensions at startup or on resolution change.
 * Sets up tile grid size, viewport center, allocates occupancy bitmap
 * initialized to all-0xFF.
 * Address: 0x454E60
 * __thiscall (this=TileMap*, param_1: 0=use screen dims, 1=use 1024x768)
 */
void __thiscall TileMap_Init(TileMap* tilemap, char use_1024x768);

/**
 * Update all tile assets — waits for async thread to finish, then loads
 * asset files into the tile grid.
 * Address: 0x457320
 * __fastcall (ECX=TileMap*)
 */
void __fastcall TileMap_UpdateAll(TileMap* tilemap);

/* ================================================================== */
/* TileMap query functions                                            */
/* ================================================================== */

/**
 * Get object at tile coordinates in a specific layer.
 * Returns pointer or 0 if out of bounds or empty.
 * Address: 0x455620
 * __thiscall (this=TileMap*, param_1=tile_x, param_2=tile_y, param_3=layer)
 * Tile indexing: this + 0x48 + x * 0x1040 + y * 0x40 + layer * 4
 * Valid ranges: x: 0..81 (0x51), y: 0..65 (0x41)
 */
void* __thiscall TileMap_GetObjectAt(TileMap* tilemap,
                                      short tile_x, short tile_y, short layer);

/**
 * Extended version with additional bounds checking.
 * Scans layers from highest active layer downward using the layer-count
 * byte at +0x80 within the tile entry.
 * Address: 0x455670
 */
void* __thiscall TileMap_GetObjectAtEx(TileMap* tilemap,
                                        short tile_x, short tile_y, short* layer_out);

/**
 * Get the tile origin (top-left pixel) for a given position.
 * Address: 0x455740
 */
int* __thiscall TileMap_GetTileOrigin(TileMap* tilemap, int* out_id,
                                       short tile_x, short tile_y, short layer);

/**
 * Get the tile origin (explicit layer version).
 * Address: 0x4557C0
 */
void __thiscall TileMap_GetTileOriginEx(TileMap* tilemap, int* out_id,
                                         short tile_x, short tile_y, short layer);

/**
 * Find an object with the given resource ID near the given tile.
 * Address: 0x4550C0
 */
int* TileMap_FindObject(TileMap* tilemap, int target_resource_id,
                         short tile_x, short tile_y, char unknown, int mode);

/**
 * Find an object by position in the tile grid.
 * Address: 0x4556F0
 */
void* TileMap_FindObjectByPos(TileMap* tilemap, short tile_x, short tile_y);

/**
 * Find the nearest object of a given type to a position.
 * Address: 0x457CE0
 */
void* TileMap_FindNearestObject(void* tilemap, ushort type_filter,
                                 int target_x, int target_y,
                                 int search_radius);

/**
 * Check if a tile is occupied (building overlap collision test).
 * Address: 0x457B60
 * __cdecl (param_1 = tile1 resource, param_2 = tile2 resource)
 * Returns 0x32 (50) for building-building overlap,
 *         10 for building-scenery/water,
 *         -1 for no conflict.
 */
int TileMap_IsTileOccupied(int tile_resource_a, int tile_resource_b);

/**
 * Check if a tile is buildable at a given position.
 * Address: 0x457C20
 * __cdecl (param_1 = tile1 resource, param_2 = tile2 resource)
 * Returns 100 for valid, 0x64 for buildable, -1 for blocked.
 */
int TileMap_IsTileBuildable(int tile_resource_a, int tile_resource_b);

/**
 * Get viewport tile information for a sprite in all 4 directions.
 * Fills fields at +0xD4..0xE4 with neighbor tile occupancy data.
 * Address: 0x457830
 * __thiscall (this=TileMap*, param_1=sprite with tile data at +0x40)
 */
void __thiscall TileMap_GetTileRect(TileMap* tilemap, void* sprite);

/**
 * Get tile buildability info for a sprite in all 4 directions.
 * Fills fields at +0xF8..0x108 with neighbor tile buildability.
 * Address: 0x457900
 */
void __thiscall TileMap_GetTileAt(TileMap* tilemap, void* sprite);

/**
 * Convert world pixel coordinates to screen coordinates.
 * Address: 0x458270
 */
void __fastcall TileMap_WorldToScreen(void* output_coords);

/* ================================================================== */
/* TileMap viewport/scroll functions                                  */
/* ================================================================== */

/**
 * Get the viewport tile entry for a given sprite in a direction.
 * Returns tile data pointer or NULL.
 * Address: 0x4579D0
 * __thiscall (this=TileMap*, param_1=sprite, param_2=direction 0-3)
 */
void* __thiscall TileMap_GetViewport(TileMap* tilemap,
                                      void* sprite, int direction);

/**
 * Set viewport for a building sprite — checks all 4 directions for
 * adjacent tile suitability, counts valid neighbors, returns count.
 * Address: 0x4576B0
 * __thiscall (this=TileMap*, param_1=building sprite)
 * Returns count of valid adjacent tiles (0-4), or 1 if neighbor is station.
 */
char __thiscall TileMap_SetViewport(TileMap* tilemap, void* building_sprite);

/**
 * Update viewport TileMap data for a sprite.
 * Similar to SetViewport but handles type 7 (multi-track) objects.
 * Address: 0x4573E0
 */
char __thiscall TileMap_UpdateViewport(TileMap* tilemap,
                                        void* sprite, short sprite_type);

/**
 * Scroll the tilemap by a delta offset. Updates scroll_x/scroll_y.
 * Address: 0x455960
 */
uint TileMap_Scroll(void* tilemap, int delta_x, int delta_y,
                     int drag_start_x, int drag_start_y);

/**
 * Scroll the tilemap to a specific object position.
 * Removes the object from the INPUT manager on scroll.
 * Address: 0x455AB0
 */
void* __thiscall TileMap_ScrollTo(TileMap* tilemap,
                                   void* target, int scroll_flag);

/**
 * Scroll a rectangle region (used for drag-select).
 * Address: 0x4553E0
 */
char __thiscall TileMap_ScrollRect(TileMap* tilemap,
    char use_sound, void* target_building, short delta_x, ushort delta_y,
    int unknown_param);

/* ================================================================== */
/* TileMap input functions                                            */
/* ================================================================== */

/**
 * Handle a mouse click on the tilemap. Dispatches to build mode
 * (placement, drag-scroll) or town mode (object selection).
 * Address: 0x455D60
 * __thiscall (this=TileMap*, param_1=screen_x, param_2=screen_y)
 */
char __thiscall TileMap_HandleClick(TileMap* tilemap,
                                     int screen_x, int screen_y);

/**
 * Clear the "input processed" flag byte (at tilemap+0x3C).
 * Address: 0x456140
 */
void __thiscall TileMap_ClearInputProcessedFlag(TileMap* tilemap);

/* ================================================================== */
/* TileMap dirty-rect / rendering functions                           */
/* ================================================================== */

/**
 * Invalidate a rect on the tilemap — marks tiles in the area as dirty
 * for re-rendering. Core of the rendering pipeline.
 * Address: 0x455840
 */
void __thiscall TileMap_InvalidateRect(TileMap* tilemap, int left, int top,
                                        int right, int bottom);

/**
 * Main dirty-rect processing pipeline. Scans visible tiles, builds
 * a linked list of dirty RECTs (non-occupied tiles only), then blits
 * each rect from cursor surface to primary surface. Also handles
 * selection overlay rendering.
 * Address: 0x456150
 * __thiscall (this=TileMap*, param_1=force_all flag)
 */
void __thiscall TileMap_InvalidateDirtyRects(TileMap* tilemap, char force_all);

/**
 * Process a single dirty rect: blits the backbuffer region.
 * Address: 0x456700
 * __thiscall (this=TileMap*, left, top, right, bottom)
 */
void __thiscall TileMap_ProcessRect(TileMap* tilemap,
                                     int left, int top, int right, int bottom);

/**
 * Process all dirty rects in the linked list — merges overlapping rects
 * via UnionRect until no more merges possible (multi-pass).
 * Address: 0x456C60
 */
char TileMap_ProcessDirtyRects(RECT* rect_list);

/**
 * Clip dirty rects to viewport, removing those outside the visible area.
 * Address: 0x456D10
 */
void TileMap_FreeDirtyRects(RECT* rect_list);

/**
 * Process object timer events on the tilemap.
 * Validates tile footprint matches expected resource IDs in all directions.
 * Address: 0x456D90
 */
uint __thiscall TileMap_ProcessObjectTimer(TileMap* tilemap, void* param_object);

/**
 * Create an overlay on the tilemap (used for build placement preview).
 * Scans all tiles and assigns overlay types (2=water, 3=building,
 * 5=road, 6=track, 7=scenery) to a tile-sized surface buffer.
 * Address: 0x457080
 */
void* TileMap_CreateOverlay(void* tilemap, int resource_id, byte param_2);

/* ================================================================== */
/* TileMap Manager functions (Sprite_* naming, but operate on TileMap) */
/* ================================================================== */

/**
 * Initialize TileMap sprite/rendering system. Sets vtable to 0x478520
 * (compiler-managed table at 0x478520), allocates DDRAW_SpriteData for asset loading
 * and enumeration, zeros occupancy bitmap and flags.
 * Address: 0x454CF0
 * __fastcall (ECX=TileMap*)
 */
void* __fastcall Sprite_Create(void* tilemap);

/**
 * Shutdown TileMap sprite system: frees asset load/enum data and
 * occupancy bitmap.
 * Address: 0x454DE0
 * __fastcall (ECX=TileMap*)
 */
void __fastcall Sprite_Shutdown(void* tilemap);

/**
 * Recalculate TileMap viewport center from client area offsets.
 * Address: 0x454FA0
 * __fastcall (ECX=TileMap*)
 */
void __fastcall Sprite_LockAll(void* tilemap);

/**
 * Full TileMap reset: deselects game objects, reinitializes world,
 * clears tile grid data and occupancy bitmap.
 * Address: 0x454FE0
 * __fastcall (ECX=TileMap*)
 */
void __fastcall Sprite_UnlockAll(void* tilemap);

#endif /* TILEMAP_H */
