/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * src/game/tile_desc.h — Tile descriptor (CTileDesc) data structures
 *
 * Each tile in the game world has a corresponding .dat text descriptor file
 * inside resource.RFD. This header defines the in-memory representation
 * populated by the parser (CTileDesc_ParseDat in game_world.c).
 *
 * .dat format is plain ASCII text, CRLF line endings.
 * All 627 .dat files in resource.RFD follow this format.
 *
 * Original: Intelligent Games for LEGO Media (1998)
 */

#ifndef LOCO_TILE_DESC_H
#define LOCO_TILE_DESC_H

#include <stdint.h>

/* Maximum values (observed in shipped .dat files) */
#define TILE_MAX_PHYS_COLS     8    /* physical_occupancy max columns */
#define TILE_MAX_PHYS_ROWS     8    /* physical_occupancy max rows */
#define TILE_MAX_PHYS_LAYERS   6    /* building\launcher.dat = 6 layers */
#define TILE_MAX_FRAME_SETS    20   /* building\launcher.dat = 20 frame sets */
#define TILE_MAX_EMPLOYEES     5    /* PossibleEmployees list length */
#define TILE_MAX_MINIFIGS      5    /* PossibleMinifigs list length */
#define TILE_COMPASS_DIRS      16   /* vehicle frame sets (16-direction) */

/* entry_exit special values:
 *   0    = not connected on this side
 *   2    = road/track connected (road tiles; value = edge flag)
 *   N>2  = track enters at pixel offset N on this side (buildings with track)
 *
 * Side order: [0]=North, [1]=East, [2]=South, [3]=West
 */
#define TILE_SIDE_N  0
#define TILE_SIDE_E  1
#define TILE_SIDE_S  2
#define TILE_SIDE_W  3

/* =========================================================================
 * AnimFrameSet — one named animation state
 * Populated from lines after the final -9 sentinel in the .dat file:
 *   <name>  <set_idx>  <first_frame>  <speed>  <?>  <delay_ms>  <sound_id>  <?>  <?>  <loop>  <?>
 * Example (van.dat): "W  0  0  1  0  0  -1  0  0  0  0"
 * ========================================================================= */

typedef struct AnimFrameSet {
    char     name[16];         /* state name: "W", "SW", "cursor", "idle" etc. */
    int16_t  set_idx;          /* animation set index (indexes into sprite sheet) */
    int16_t  first_frame;      /* first frame of this animation */
    int16_t  speed;            /* frames per animation step (0=static) */
    int16_t  delay_ms;         /* delay in ms before animation starts */
    int32_t  sound_id;         /* sound effect ID to play (-1=none) */
    int16_t  loop;             /* loop count (0=infinite, -1=play once) */
    int16_t  extras[3];        /* undocumented trailing fields */
} AnimFrameSet;

/* =========================================================================
 * CTileDesc — complete tile descriptor
 *
 * Original C++ class reconstructed from .dat parser and game_world code.
 * The actual in-memory size is unknown; this is the ported equivalent.
 * Original game allocates one CTileDesc per unique tile type at startup.
 * ========================================================================= */

typedef struct CTileDesc {
    /* --- identity --- */
    char     filename[128];    /* relative path in RFD: "roads\half-vwint.dat" */
    uint32_t resource_id;      /* 16-bit logical ID used to look up this entry */

    /* --- physical_occupancy ---
     * 3D grid: phys_layers × phys_rows × phys_cols of 0/1 occupancy flags.
     * 1 = cell blocked by this tile, 0 = cell passable.
     * Most tiles: 1 layer. Tall buildings: up to 6 layers.
     * Original is a heap-allocated uint8 array; size = cols*rows*layers.
     */
    uint8_t  phys_cols;
    uint8_t  phys_rows;
    uint8_t  phys_layers;
    uint8_t  phys_grid[TILE_MAX_PHYS_LAYERS][TILE_MAX_PHYS_ROWS][TILE_MAX_PHYS_COLS];

    /* --- bitmap_occupancy ---
     * Visual footprint in tile units (may differ from physical for tall buildings).
     * Numbers in the grid indicate which sprite "band" covers that visual tile.
     */
    uint8_t  bmp_cols;
    uint8_t  bmp_rows;
    uint8_t  bmp_grid[TILE_MAX_PHYS_ROWS][TILE_MAX_PHYS_COLS];

    /* --- connectivity ---
     * entry_exit[4] = connection point per side (N/E/S/W).
     * 0 = no connection; 2 = road edge; >2 = pixel offset into tile edge.
     */
    int32_t  entry_exit[4];    /* [N, E, S, W] */

    /* --- right-mouse-button ---
     * Animation sequence ID to play on right-click.
     * -1 = no RMB action on this tile type.
     */
    int32_t  rmb_seq_id;

    /* --- leisure and roaming ---
     * leisure_dest: 1 = minifigs can walk here for leisure activities.
     * free_to_roam: pixel bounding box for minifig movement on this tile.
     */
    uint8_t  leisure_dest;
    int16_t  free_roam_x1, free_roam_y1, free_roam_x2, free_roam_y2;

    /* --- employees (workers at this building) ---
     * max_employees: how many workers are employed here simultaneously.
     * possible_employees[i]: minifig type ID (-1 = any type).
     */
    int8_t   max_employees;
    int32_t  possible_employees[TILE_MAX_EMPLOYEES];

    /* --- visitors (minifigs using this resource) ---
     * max_minifig: max simultaneous visitors.
     * possible_minifigs[i]: minifig type ID (-1 = any).
     */
    int8_t   max_minifig;
    int32_t  possible_minifigs[TILE_MAX_MINIFIGS];

    /* --- vehicle-only fields ---
     * walk_speed: movement speed (x, y) for vehicles.
     * sex: 'M' or 'F' for minifig gender restriction.
     * pickup_sound_id: sound when this vehicle is picked up.
     * ground_width: collision width in pixels.
     */
    int8_t   walk_speed_x, walk_speed_y;
    char     sex;              /* 'M', 'F', or 0 = any */
    int32_t  pickup_sound_id;
    int16_t  ground_width;

    /* --- display ---
     * shifts[4]: isometric pixel rendering offsets.
     *   shifts[0,1] = NW corner x,y offset
     *   shifts[2,3] = SE corner x,y offset
     * button_visible: 1 = appears in build-mode toolbar.
     * closed_fs: animation frame set index for night/closed state.
     * button_offset[3]: sprite position in toolbar panel.
     * hotspot[2]: click detection point in pixels (x, y).
     */
    int8_t   shifts[4];
    uint8_t  button_visible;
    int8_t   closed_fs;
    int8_t   button_offset[3];
    int16_t  hotspot_x, hotspot_y;

    /* --- sequence triggers ---
     * insert_seq: (group, id) sound+animation when tile is first placed.
     * mobile_seq: (group, id) animation sequence for moving entities here.
     * easter_egg strings are stored as opaque blobs for the EasterEgg system.
     */
    int32_t  insert_seq_group, insert_seq_id;
    int32_t  mobile_seq_group, mobile_seq_id;
    char     insert_easter_egg[64];
    char     mobile_easter_egg[64];

    /* --- animation ---
     * total_frames:   total sprite frames in the sprite sheet.
     * num_frame_sets: number of named animation states in the table below.
     * cursor_frame_set: (idx, n) which frame set to activate on cursor hover.
     * frame_sets[]:  one entry per named state (loaded from table after -9 sentinel).
     */
    int16_t  total_frames;
    int16_t  num_frame_sets;
    int16_t  cursor_fs_idx;
    int16_t  cursor_fs_n;
    AnimFrameSet frame_sets[TILE_MAX_FRAME_SETS];
} CTileDesc;

/* =========================================================================
 * Function declarations (implemented in game_world.c)
 * ========================================================================= */

/* Parse a .dat text file content into td. Returns 1 on success, 0 on error.
 * WIN32: called by CGameWorld_LoadTileDescriptors during startup.
 * LINUX: same — no platform dependencies in the parser itself.
 */
int CTileDesc_ParseDat(CTileDesc *td, const char *content, const char *filename);

/* Look up the tile descriptor for a given resource ID.
 * Returns NULL if not found.
 * WIN32/LINUX: linear search of in-memory table.
 */
CTileDesc *CTileDesc_Lookup(uint32_t resource_id);

/* Returns 1 if the tile has a road/track connection on the given side. */
int CTileDesc_IsConnected(const CTileDesc *td, int side);

/* =========================================================================
 * Global tile descriptor table
 * Original: heap array, count at DAT_004aa4a0+offset (exact address TBD)
 * ========================================================================= */

extern CTileDesc *g_tileDescs;     /* heap array of all tile descriptors */
extern int        g_tileDescCount; /* number of loaded descriptors */

#endif /* LOCO_TILE_DESC_H */
