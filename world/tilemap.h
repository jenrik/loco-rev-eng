// Status: INTEGRATED
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
 * pixel cell storing up to 16 overlapped layer slots (base terrain, track,
 * scenery, vehicles/buildings).
 *
 * Tile addressing (verified against disassembly):
 *   layer-0 slot:  this + 0x48 + (tile_x * 65 + tile_y) * 0x40
 *   layer L slot:  + L * 4
 *   origin region: this + 0x64 + (tile_x * 65 + tile_y) * 0x40 + L * 4
 *   active byte:   this + 0x80 + (tile_x * 65 + tile_y) * 0x40
 *   Values: 0 = empty, non-zero = pointer to object occupying the tile.
 *
 * Field layout at +0x04..+0x30 (Init 0x454E60 / disassembly):
 *   +0x04 width  (aliases g_world_width, 0x4AAD0C)
 *   +0x08 height (aliases g_world_height)
 *   +0x0C..+0x18 viewport RECT in world pixels (left/top = scroll offset,
 *                right/bottom = map extents; aliases the g_viewport_rect_*
 *                globals at 0x4AAD14..0x4AAD20 and is maintained by CGWND)
 *   +0x1C/+0x20 viewport_x / viewport_y (screen scroll offset; aliases
 *                g_viewport_x / g_viewport_y at 0x4AAD24/0x4AAD28)
 *   +0x24/+0x28 center_x / center_y (map dims / 2)
 *   +0x2C/+0x30 viewport_center_x / viewport_center_y
 *
 * Size: ~0x52514 bytes (static global)
 * Global: g_tilemap at 0x4AAD08
 * Vtable: 0x478520 ([0] scalar-deleting dtor wrapper 0x454DB0 → body 0x454DE0)
 */

#pragma once

#include "../core/BuildingMgrObjectGroup.h"
#include "../shared/types.h"
#include <cstddef>
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
    uint8_t grid_span_y;                 /* +0x16B  bitmap_occupancy grid
                                          *   width (BuildingDescriptorEditor::
                                          *   bitmap_occupancy_width, same
                                          *   offset -- confirmed same real
                                          *   object: RESDATA_ScriptedObject_
                                          *   AddChild constructs via
                                          *   BuildingDescriptorEditor__Ctor,
                                          *   see PROGRESS.md) */
    uint8_t original_span;               /* +0x16C  bitmap_occupancy grid
                                          *   height (bitmap_occupancy_height) */
    uint8_t _pad_16D;                    /* +0x16D  border_scale_byte,
                                          *   unused by TileMap */

    /* Physical-occupancy 3D grid, dims grid_width x grid_height x
     * grid_depth, indexed [x][y][z] = x*63+y*7+z (ScrollRect 0x4553E0 /
     * FindObject 0x4550C0). Real size confirmed via
     * input/BuildingDescriptorEditor.h's physical_occupancy_grid[0x333]
     * at the same +0x16E offset -- the two headers model the same real
     * x86 object (see the AddChild comment above); this file's earlier
     * occupancy_grid[9*7] was undersized (63 bytes only covers x==0). */
    int8_t occupancy_grid[0x333];        /* +0x16E */

    /* bitmap_occupancy grid cell values (span map), dims grid_span_y x
     * original_span, stored row-major with a 9-byte column stride in the
     * original layout (BuildingDescriptorEditor__parse_dat_directive_line,
     * 0x41E9F0) -- FindObject (0x4550C0) reads this with the matching
     * stride to write the origin-region layer for each nonzero cell
     * (value - 1 = layer index). Same object/offset identity as
     * occupancy_grid above; real size from BuildingDescriptorEditor.h's
     * bitmap_occupancy_grid[0x75]. */
    uint8_t span_map[0x75];              /* +0x4A1 */

    uint8_t _pad_516[0x4A];
    uint32_t expected_count;             /* +0x560  footprint id list length
                                          *   (ProcessObjectTimer) */
    int32_t* expected_ids;               /* +0x564  expected resource-id array
                                          *   (ProcessObjectTimer) */
    uint8_t _pad_568[0x94];
    int32_t neighbor_def[4][2];          /* +0x5FC  per-direction neighbor
                                          *   definitions (GetViewport) */
    uint8_t _pad_61C[0x1E];              /* +0x61C .. +0x639 (unmapped) */
    uint8_t state_63A;                   /* +0x63A  object state byte read by
                                          *   the type-3 predicates 0x44BD50 /
                                          *   0x44BD70 / 0x44BD90 and by
                                          *   IsEditorSprite (0x41F430) */

    /** Sprite-editor object predicate. Address: 0x41F430 (thiscall).
     *
     *  True when the resource is a sprite-editor/creator object that the
     *  TileMap viewport/placement walks must skip:
     *    object_type (+0x08) == 0x03: state (+0x63A) ∈ {0x0E,0x0F,0x10,0x11}
     *                                 (0x44BD50 → {0x0E,0x0F},
     *                                  0x44BD70 → {0x10,0x11})
     *    object_type (+0x08) == 0x0C: resource_id (+0x04) ∈ {0x3001,0x3002}
     *  Verified against objdump 0x41F430 (the legacy "INPUT_EditCharHandler"
     *  label was fabricated; the function is a resource predicate). */
    bool IsEditorSprite() const;
};

/* TileMapObject removed 2026-08-14 (see PROGRESS.md's "TileMapObject
 * mirror struct" entry). It was a hand-written x86-offset mirror struct
 * that every TileMap placement/occupancy routine `reinterpret_cast`ed
 * onto whatever real polymorphic object a tile-grid slot held. On this
 * 64-bit host its own `resource` member (declared assuming a 4-byte x86
 * pointer at +0x40) is actually 8 bytes, silently shifting every field
 * declared after it by 4 real bytes relative to the struct's own offset
 * comments -- e.g. `is_moving` (documented +0xC0) landed on real offset
 * +0xC4, which happens to be the first byte of the *next* object's own
 * occupancy_links[0], not a movement flag at all. A live memory-
 * corruption bug, not just an unreliable read.
 *
 * Every object TileMap::FindObject can place into the tile grid (via
 * INPUT_PlaceObject, 0x41DD80) is provably ResourceGameObject or a class
 * derived from it (RESDATA_GameVehicle, GameVehicle, HelpPageNode) --
 * see core/BuildingMgrObjectGroup.h's class doc comment for the ctor
 * chain evidence. All TileMap methods that used to take/return
 * TileMapObject* now use ResourceGameObject* directly. */

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
struct AssetMgr;

/* ================================================================== */
/* Callback type for FindObject                                       */
/* ================================================================== */
typedef int (*TileMap_FindObjCallback)(int tile_resource_id, int target_resource_id);

/* ================================================================== */
/* TileMap class — tile grid + scroll state + all operations          */
/* ================================================================== */
class TileMap {
public:
    TileMap(const TileMap&) = delete;
    TileMap& operator=(const TileMap&) = delete;

    /** Constructor — Address: 0x454CF0 */
    TileMap();

    /** Destructor — Address: 0x454DE0; virtual slot [0], table at 0x478520 */
    virtual ~TileMap();

    /* ---- Typed accessors for 32-bit tile slots ---- */
    void*   ReadTilePointer(size_t data_index) const;
    int32_t ReadTileValue(size_t data_index) const;
    void    WriteTileValue(size_t data_index, int32_t value);

    /**
     * Converts a placed-object pointer into the value a tile slot should
     * store. On _WIN32 this is the original x86 pointer-to-int32_t cast
     * (lossless there). On host it is a small monotonic handle from a
     * side-table registry, NOT the pointer's low 32 bits -- confirmed by
     * a round-trip measurement that this host's heap addresses do not
     * survive a uint32_t truncate/widen cycle (unlike a 32-bit ABI, this
     * is not a rare ASLR edge case; it fires on ordinary operator_new
     * addresses every time). Every write site that used to compute
     * `static_cast<int32_t>(reinterpret_cast<intptr_t>(ptr))` before
     * calling WriteTileValue must call this first instead; ReadTilePointer
     * is the matching read-side translation. Returns 0 for a null ptr,
     * matching the tile grid's own "0 = empty" convention.
     */
    int32_t StoreTilePointer(void* ptr);

#ifndef _WIN32
    /** Host-only diagnostic, no original counterpart: number of tile-grid
     * cells the most recent FindObject() call actually wrote via
     * WriteTileValue. A non-null FindObject() return is not proof of
     * occupancy by itself -- a resource with no real physical/bitmap
     * footprint (grid_width/height == 0 and orig_span == 0) still
     * constructs and returns an entity but writes zero cells. Callers
     * that need to know whether a placement really landed in the grid
     * (see INPUT_LoadSaveFile's placed-vs-occupied measurement) must
     * check this after each FindObject() call, not just the return
     * value. */
    int32_t last_find_object_cells_written = 0;

    /* Host-only diagnostic: number of occupancy-bitmap tiles found dirty
     * (ATTR_0047f108-gated) during the most recent InvalidateDirtyRects()
     * call. Reset at function entry, incremented once per dirty tile
     * found by the scan loop. Lets callers measure "dirty-tile detection
     * went from always-0 to real" rather than inferring it from whether
     * ProcessRect/DDRAW_PresentRect happened to run without crashing. */
    int32_t last_dirty_tile_count = 0;
#endif

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

    /**
     * Get tile origin resource ID — Address: 0x455740.
     * Reads the ORIGIN region slot (this+0x64+...) and returns the object's
     * tile origin at +0x88. *out_id = -1 when the slot is empty. Returns
     * out_id for the compat wrapper (the binary returns void; the out
     * pointer is returned only so the TileMap_GetTileOrigin wrapper can
     * serve EditorState.cpp).
     */
    int* GetTileOrigin(int* out_id, short tile_x, short tile_y, short layer);

    /** Get tile origin (standard 0x48 slot) — Address: 0x4557C0 */
    void GetTileOriginEx(int* out_packed, short tile_x, short tile_y, short layer);

    /** Validate and place a building at tile position — Address: 0x4550C0 */
    int* FindObject(unsigned int target_resource_id, short tile_x, short tile_y,
                    char unknown, unsigned int mode);

    /** Find object by tile position — Address: 0x4556F0 */
    void* FindObjectByPos(int pixel_x, int pixel_y);

    /* ---- Viewport / scroll ---- */

    /** Get neighboring object in a direction — Address: 0x4579D0 */
    ResourceGameObject* GetViewport(ResourceGameObject* sprite, int direction);

    /** Count valid adjacent tiles for building — Address: 0x4576B0 */
    char SetViewport(ResourceGameObject* building_sprite);

    /** Count valid adjacent tiles (generic sprite) — Address: 0x4573E0 */
    char UpdateViewport(ResourceGameObject* sprite, short sprite_type);

    /** Scroll by delta (float-stepped hit test) — Address: 0x455960 */
    uint Scroll(int delta_x, int delta_y, int drag_start_x, int drag_start_y);

    /** Scroll to target object position — Address: 0x455AB0 */
    void* ScrollTo(ResourceGameObject* target, int scroll_flag);

    /**
     * Validate placement rect scroll — Address: 0x4553E0.
     * target_building is the raw resource (what ResourceManager_GetById
     * returns), not a placed ResourceGameObject -- confirmed via
     * disassembly: every +0x168/+0x169/+0x16a/+0x16e read in this
     * function and in FindObject's fill loop is against that raw
     * resource. The parameter was previously mistyped TileMapObject*
     * (the now-removed mirror struct).
     */
    char ScrollRect(char use_sound, TileMapResource* target_building,
                    short delta_x, unsigned short delta_y, int placement_mode);

    /* ---- Tile occupancy / buildability queries ---- */

    /** Get tile occupancy for all 4 directions — Address: 0x457830 */
    void GetTileRect(ResourceGameObject* sprite);

    /** Get tile buildability for all 4 directions — Address: 0x457900 */
    void GetTileAt(ResourceGameObject* sprite);

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

    /** Render a single dirty rect — Address: 0x456700 */
    void ProcessRect(int left, int top, int right, int bottom);

    /** Validate object footprint against expected id layout — Address: 0x456D90 */
    uint ProcessObjectTimer(ResourceGameObject* param_object);

    /** Build placement-preview overlay surface — Address: 0x457080 */
    void* CreateOverlay(void* surface, byte fill_byte);

    /** Spatial search in concentric rings — Address: 0x457CE0 */
    void* FindNearestObject(unsigned short type_filter,
                            int target_x, int target_y, int search_radius);

    /* ---- Header fields (0x00..0x47) ---- */
    /* +0x00 compiler-managed vptr */
    int32_t     width;                  /* +0x04  map width in pixels
                                         *   (aliases g_world_width) */
    int32_t     height;                 /* +0x08  map height in pixels
                                         *   (aliases g_world_height) */
    RECT        viewport_rect;          /* +0x0C..+0x18 visible world region;
                                         *   left/top = scroll offset,
                                         *   right/bottom = map extents.
                                         *   Aliases the g_viewport_rect_*
                                         *   globals (0x4AAD14..0x4AAD20),
                                         *   maintained by CGWND scrolling. */
    int32_t     viewport_x;             /* +0x1C  current scroll offset X
                                         *   (aliases g_viewport_x 0x4AAD24) */
    int32_t     viewport_y;             /* +0x20  current scroll offset Y
                                         *   (aliases g_viewport_y 0x4AAD28) */
    int32_t     center_x;               /* +0x24  center X (width/2) */
    int32_t     center_y;               /* +0x28  center Y (height/2) */
    int32_t     viewport_center_x;      /* +0x2C  viewport center X */
    int32_t     viewport_center_y;      /* +0x30  viewport center Y */
    int32_t     drag_start_x;           /* +0x34  drag scroll start X */
    int32_t     drag_start_y;           /* +0x38  drag scroll start Y */
    uint8_t     scroll_drag_active;     /* +0x3C  1=currently scrolling via drag */
    uint8_t     _pad_3D;                /* +0x3D */
    int16_t     tile_count_x;           /* +0x3E  number of tile columns
                                         *   (aliases g_player_id 0x4AAD46) */
    int16_t     tile_count_y;           /* +0x40  number of tile rows
                                         *   (aliases g_player_color 0x4AAD48) */
    uint8_t     _pad_42[6];             /* +0x42..+0x47 */

    /* ---- Tile data array (+0x48..+0x52483) ---- */
    /* Each tile is 0x40 (64) bytes, organized as:
     *   tile[tile_x * 65 + tile_y] at (0x48 + (tile_x*65+tile_y) * 0x40)
     *   layer[layer] within tile at +layer*4
     *   Value: 0 = empty, non-zero = object pointer
     * Max tiles: 82 columns (x) x 66 rows (y) = 5412 tiles
     */
    uint8_t     tile_data[0x5243C];     /* +0x48  tile grid */

    /* ---- Misc pointers at end ---- */
    void*       occupancy_bitmap;       /* +0x52484  bitmap: 1 bit per tile */
    /* Genuine AssetMgr* (resources/AssetMgr.h), not a SpriteData* — that
     * type name/identity had zero independent evidence anywhere and was
     * removed 2026-08-14. Forward-declared above rather than #include-ing
     * AssetMgr.h directly: its extern "C" operator_new/GLOBAL_free block
     * conflicts with this file's own locally-declared, differently-typed
     * copies (same reason AssetMgr_LoadFileEx/EnumFiles below stay
     * free-function shims instead of direct method calls). */
    AssetMgr*   asset_load_ptr;         /* +0x52488  asset loading context */
    AssetMgr*   asset_enum_ptr;         /* +0x5248C  asset enumeration context */
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
extern TileMap* g_tilemap;              /* 0x4AAD08 — host-constructed singleton pointer */


/* ================================================================== */
/* Free functions (no TileMap* this pointer)                           */
/* ================================================================== */

/*
 * NOTE: the previously transcribed "TileMap_WorldToScreen" free function
 * does not exist in the binary. The claimed address 0x458270 belongs to
 * BuildingMgrObjectGroup_DtorBody, and no world-to-screen conversion
 * function exists under any TileMap name (list_functions returns no such
 * symbol, and no callers reference it). The binary's coordinate helpers
 * are Game_ScreenToWorld (0x412060, screen + viewport -> world) and the
 * DDRAW_Building inline conversion at 0x45A500 (world - viewport + sprite
 * offset); both belong to other subsystems. The declaration and stub were
 * removed on integration.
 */

/** Check if two tile resources conflict — Address: 0x457B60 */
int TileMap_IsTileOccupied(int tile_resource_a, int tile_resource_b);

/** Check if tile_b may be placed on top of tile_a.  Dispatch is on
 *  tile_a's object type (0x457C20); returns 0x64 (100) when valid,
 *  -1 when blocked. */
int TileMap_IsTileBuildable(int tile_resource_a, int tile_resource_b);

/** Dirty-rect list node used by TileMap::InvalidateDirtyRects/ProcessRect's
 *  presentation pipeline. The original x86 layout stores `next` as a 4-byte
 *  LONG immediately after the RECT (allocated 0x14 = 16 + 4 bytes) and
 *  every site round-trips it through `static_cast<LONG>(reinterpret_cast<
 *  intptr_t>(ptr))` -- lossless on the original 32-bit ABI, but a real
 *  pointer-truncation bug on this 64-bit host (same landmine class as the
 *  TileMap tile-pointer registry fix). A typed `next` member removes the
 *  round-trip entirely rather than widening the truncating cast. */
struct DirtyRectNode {
    RECT rect;
    DirtyRectNode* next;
};

/** Merge overlapping dirty rects in the linked list — Address: 0x456C60.
 *  Returns 1 if any merge occurred (caller loops until 0). */
char TileMap_ProcessDirtyRects(DirtyRectNode* rect_list);

/** Clip dirty rects to the viewport rect; free out-of-view rects.
 *  Address: 0x456D10 */
void TileMap_FreeDirtyRects(DirtyRectNode* rect_list);

/**
 * CreateOverlay — build placement-preview overlay. Address: 0x457080.
 * __thiscall on TileMap in the binary; exposed as a free function with
 * the signature declared by network/Netman.h (the only remaining caller),
 * which fixes the first parameter as void*.
 */
void TileMap_CreateOverlay(void* tilemap, void* surface, int32_t flags);

/* RESDATA tile-state predicates (canonical declarations).  Binary ABI:
 * __thiscall with ECX = the resource pointer (0x41D7BB / 0x41E35A call
 * 0x44BD30 with ECX = entity->resource); the __fastcall annotation keeps
 * that convention on 32-bit Windows and expands to the native ABI on
 * hosts (compat.h).  Implemented in world/tilemap.cpp.  (The sibling
 * predicates 0x44BD50/0x44BD70 are declared below as the legacy int
 * IsWaterTile/IsTrackTile forms — checked 2026-08-06 during the
 * RESDATA_IsBuildingTile/IsRoadTile cluster fix: both are consistently
 * `(int)`, C++ linkage, defined once in shared/stubs_impl.cpp with no
 * caller-side mismatch anywhere in the tree, so they do NOT share the
 * extern-"C"/wrong-param-type defect the other two had.) */
uint8_t __fastcall RESDATA_IsBuildingTile(int32_t tile_obj);  /* 0x44BD30 */

/* ================================================================== */
/* External globals referenced by TileMap                              */
/* ================================================================== */
extern int32_t  g_game_mode;            /* 0x004851F4 */
extern int32_t  g_screen_width;         /* 0x004851D8 */
extern int32_t  g_screen_height;        /* 0x00485214 */
extern int32_t  g_client_offset_x;      /* 0x00485228 */
extern int32_t  g_client_offset_y;      /* 0x0048522C */
extern int32_t  g_client_width;         /* 0x00485220 */
extern int32_t  g_client_height;        /* 0x00485224 */
extern uint8_t  g_is_fullscreen;        /* 0x00485210 */
extern int32_t  g_world_width;          /* 0x004AAD0C (TileMap.width) */
extern uint8_t  g_is_town_mode;         /* 0x00485328  1 = in-game town mode
                                           (tilemap/town flag — DISTINCT from
                                           g_allow_building_placement 0x4FD3DC;
                                           the two were once conflated under one
                                           C++ symbol) */
extern int32_t  g_town_overlay_rect;    /* 0x48538C */
extern int32_t  g_town_overlay_left;    /* 0x485390 */
extern int32_t  g_town_overlay_top;     /* 0x485394 */
extern int32_t  g_town_overlay_right;   /* 0x485398 */
extern uint8_t  g_build_mode;           /* 0x485234 */
extern uint8_t  g_click_on_building;    /* 0x48556C */
extern uint8_t  g_placement_valid;      /* 0x4AA648 */
extern uint8_t  g_placement_blocked;    /* 0x48558C */
extern int32_t  g_placement_resource_id;/* 0x485550 */
extern uint8_t  g_disable_input;        /* 0x4855AC */
extern uint8_t  g_click_on_town;        /* 0x48557C */
extern void*    g_cursor_surface;       /* 0x4FD3C8 */
/* NOTE: g_primary_surface, g_town_view, g_ddraw_building, g_tilemap are
 * declared in their respective module headers (graphics/DDRAW.h, etc) to
 * avoid conflicting type declarations. */
class InputMgr;
extern InputMgr  g_input_mgr;            /* 0x4A9990 — static InputMgr object (input/InputMgr.h) */
extern int32_t  g_town_selection_rect_left;   /* 0x4854D0 */
extern int32_t  g_town_selection_rect_top;    /* 0x4854D4 */
extern int32_t  g_town_selection_rect_right;  /* 0x4854D8 */
extern int32_t  g_town_selection_rect_bottom; /* 0x4854DC */
extern uint8_t  g_has_selection;        /* 0x4854EC */
/* g_selected_building is Entity* (real definition: shared/stubs_impl.cpp
 * `Entity* g_selected_building = nullptr;`, matching game/Building.cpp /
 * game/BuildingMgr.cpp / game/World.cpp's own extern declarations) — this
 * header previously declared it int32_t, a cross-TU type mismatch
 * (landmine class: wrong-width global, not just wrong-signature function)
 * that silently truncates the pointer on 64-bit hosts. Not previously a
 * compile-time conflict only because no single TU included both this
 * header and an Entity*-typed declaration at once; fixed rather than
 * left latent once game/Building.cpp needed to include both. */
class Entity;
extern Entity*  g_selected_building;    /* 0x4855B0 */
extern void*    g_town_view;            /* 0x4852A0 */
extern void*    g_ddraw_building;       /* 0x4A9EF0 */
extern void*    g_about;                /* 0x4FD390 */
extern void*    g_netman;               /* 0x4FD3AC */
/* NOTE: g_tile_occupied_bitmap removed — was misidentified global.
 * The occupancy bitmap is a member field of TileMap at +0x52484. */
extern int32_t  g_player_id;            /* 0x4AAD46 (TileMap.tile_count_x) */
extern int32_t  g_viewport_x;           /* 0x4AAD24 (TileMap.viewport_x) */
extern uint8_t  g_lock_update_flag;     /* 0x4851F0 */
extern uint8_t  g_allow_building_placement;  /* 0x4FD3DC — loader/building
                                           placement flag (Game, InputMgr,
                                           ScriptedObject, TileMap find/scroll
                                           gates).  DISTINCT from g_is_town_mode
                                           (0x485328); the two were once
                                           conflated under this name. */
extern int32_t  g_player_color;          /* 0x4AAD48 — host-declared 32-bit for
                                          *   uniform declarations; the binary
                                          *   stores the 16-bit player words
                                          *   adjacently (id 0x4AAD46) and every
                                          *   use loads 16 bits */
extern HWND     g_main_window;          /* from types.h */

/* ================================================================== */
/* External function declarations (cross-module calls)                 */
/* ================================================================== */
extern int      WIN32_GetThreadResult(int param);
extern void     AssetMgr_LoadFileEx(uint* ptr);
extern void     AssetMgr_EnumFiles(uint* ptr);
extern void     World_Lock(void* world);
extern void     World_Unlock(void* world);
/* Real def: ui/UIPANEL_Surface.cpp, bool(void*,uint32_t,uint32_t,int32_t,
 * uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t). This header's
 * declaration used to shadow the corrected one in world/tilemap.cpp with a
 * plain-`int` overload that won C++ overload resolution over RECT's `int`
 * fields (an exact match beats the real signature's int->uint32_t implicit
 * conversion) — the call still bound to the wrong shape even after the .cpp
 * file's own declaration was fixed (call-0 landmine). */
extern bool     UIPANEL_Blit(void* src, uint32_t sx, uint32_t sy, int32_t sw, uint32_t sh,
                              void* dst, uint32_t dx, uint32_t dy, int32_t dw, uint32_t dh, uint32_t flags);
#ifdef _WIN32
extern void     DDRAW_PresentRect(RECT* rect, HWND hwnd,
                                   int32_t* viewport_x, char flag);
#else
/* Host builds route present through the SDL3 path (graphics/sdl3_ddraw.cpp);
 * that symbol's real signature uses plain int and void pointers rather
 * than char, RECT, and HWND, so the declaration must match exactly or the
 * linker silently binds this call to nothing under this project's
 * unresolved-symbols=ignore-all flag. */
extern void     DDRAW_PresentRect(void* rect, void* hwnd, int* viewport_x, int flag);
#endif
extern void     GLOBAL_free(void* ptr);
extern void*    operator_new(size_t size);
extern void*      INPUT_PlaceObject(InputMgr* mgr, unsigned int resource_id); /* 0x41DD80 */
extern uintptr_t  INPUT_RemoveObject(InputMgr* mgr, void* obj, unsigned int param); /* 0x41DEF0 */
extern int      RESDATA_IsSceneryTile(int ptr);    /* 0x44BD90 */
extern int      RESDATA_IsWaterTile(int ptr);      /* 0x44BD50 */
extern int      RESDATA_IsTrackTile(int ptr);      /* 0x44BD70 */
extern int      RESDATA_IsRoadTile(int ptr);       /* 0x44BD10 */
extern int      RESDATA_GetTileCategory(void* ptr, short a, unsigned short b); /* 0x44BDB0 */
extern "C" int  IntersectRect(RECT* dst, const RECT* a, const RECT* b);
extern "C" int  UnionRect(RECT* dst, const RECT* a, const RECT* b);
extern int      SetRectEmpty(RECT* rect);
extern void     SetRect(RECT* rect, int left, int top, int right, int bottom);
extern void     CopyRect(RECT* dst, RECT* src);
extern int      PtInRect(RECT* rect, int x, int y);
extern void     InflateRect(RECT* rect, int dx, int dy);
extern int      Town_BlitViewport(void* res, int src_x, int src_y,
                                   int src_w, int src_h,
                                   int dst_x, int dst_y);
extern void     PlaySoundAt(int sound_id, int x, int y, int channel); /* 0x4479D0 */
extern int      Town_SelectBuilding(void* town_view, int building);   /* 0x42D040 */
extern int      DDRAW_SelectBuilding(void* ddraw_building, int building); /* 0x459180 */
extern void     CGWND_SetMode(int mode);            /* 0x408130 */
/* Typed method wrappers — implemented beside the class each wraps
 * (Town.cpp, Game.cpp, UI_Utils.cpp, BuildingMgr.cpp, World.cpp,
 * scriptengine.cpp, DDRAW.cpp), not in tilemap.cpp, since those headers'
 * own g_* global declarations conflict with tilemap.cpp's local ones. */
extern void Town_RenderSelection(int x1, int y1, int x2, int y2, int extra);
extern void Town_DeselectBuilding(void);
extern void Town_UpdateSelection(void);
extern void Game_SetCursorByResourceId(int left, int top, int right, int bottom, int enable_scroll);
extern void Game_ResetCursor(void);
extern void UI_SetTooltipText(int x, int y, int w, int h);
extern void UI_SetTooltipPos(int x, int y, int w, int h, int flag);
extern void UI_UpdateTooltip(int x, int y, int w, int h, int flag);
extern void BuildingMgr_DispatchAll(int dispatch_flags, int left, int top, int right, int bottom);
extern void World_InvalidateRect(int x, int y, int w, int h, short type);
extern void RESDATA_ScriptedObject_Dispatch(int x, int y, int w, int h, int flag);
extern void DDRAW_DispatchToSubObjects(int x, int y, int w, int h, void* flag);
extern void     Game_DeselectGameObject(int game);  /* 0x411580 */
extern void     World_Init(void* world);
extern void     UI_CleanupTooltips(void* mgr);
extern int      Math_DistSquared(int x1, int y1, int x2, int y2);
extern void*    Entity_GetSubObjectPosition(void* obj, int* out_xy, int direction);
extern void     GameObject_GetSubObjectWorldPos(void* obj, int* out_packed);
extern void*    ResourceManager_GetById(void** resmgr, UINT id);
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
inline void     TileMap_GetTileRect(TileMap* tm, ResourceGameObject* s)  { tm->GetTileRect(s); }
inline void     TileMap_GetTileAt(TileMap* tm, ResourceGameObject* s)    { tm->GetTileAt(s); }
inline void*    TileMap_GetViewport(TileMap* tm, ResourceGameObject* s, int d) { return tm->GetViewport(s, d); }
inline char     TileMap_SetViewport(TileMap* tm, ResourceGameObject* bs) { return tm->SetViewport(bs); }
inline char     TileMap_UpdateViewport(TileMap* tm, ResourceGameObject* s, short t) { return tm->UpdateViewport(s, t); }
inline void*    TileMap_ScrollTo(TileMap* tm, ResourceGameObject* t, int f) { return tm->ScrollTo(t, f); }
inline char     TileMap_ScrollRect(TileMap* tm, char snd, TileMapResource* b, short dx, unsigned short dy, int pm) { return tm->ScrollRect(snd, b, dx, dy, pm); }
inline char     TileMap_HandleClick(TileMap* tm, int sx, int sy)    { return tm->HandleClick(sx, sy); }
inline void     TileMap_ClearInputProcessedFlag(TileMap* tm)        { tm->ClearInputProcessedFlag(); }
inline void     TileMap_InvalidateRect(TileMap* tm, int l, int t, int r, int b) { tm->InvalidateRect(l, t, r, b); }
inline void     TileMap_InvalidateDirtyRects(TileMap* tm, char fa)  { tm->InvalidateDirtyRects(fa); }
inline void     TileMap_ProcessRect(TileMap* tm, int l, int t, int r, int b) { tm->ProcessRect(l, t, r, b); }
inline uint     TileMap_ProcessObjectTimer(TileMap* tm, ResourceGameObject* obj) { return tm->ProcessObjectTimer(obj); }
inline int*     TileMap_FindObject(TileMap* tm, int id, short x, short y, char u, int m) { return tm->FindObject(static_cast<unsigned int>(id), x, y, u, static_cast<unsigned int>(m)); }
inline void*    TileMap_FindObjectByPos(TileMap* tm, int px, int py) { return tm->FindObjectByPos(px, py); }
inline void     TileMap_Scroll(TileMap* tm, int dx, int dy, int dsx, int dsy) { tm->Scroll(dx, dy, dsx, dsy); }
inline void*    TileMap_FindNearestObject(TileMap* tm, unsigned short type_filter,
                                           int tx, int ty, int radius) {
    return tm->FindNearestObject(type_filter, tx, ty, radius);
}

/* Sprite_* backward compat (callers should use TileMap ctor/dtor/methods) */
inline void*    Sprite_Create(TileMap* tm)    { return new (tm) TileMap(); }
inline void     Sprite_Shutdown(TileMap* tm)  { tm->~TileMap(); }
inline void     Sprite_LockAll(TileMap* tm)   { tm->RecalcViewportCenter(); }
inline void     Sprite_UnlockAll(TileMap* tm) { tm->FullReset(); }

#endif /* TILEMAP_H */
