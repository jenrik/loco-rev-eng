/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: Game World (isometric tile grid, buildings, trains, pathfinding)
 * Platform-independent: no Win32 APIs in core game logic
 *
 * Covers addresses:
 *   0x00455620 – TileMap_GetCell / WorldTileLookup
 *   0x004557c0 – TileMap_GetCoordFromNode
 *   0x00455740 – TileMap_GetCoordFromNodeAlt
 *   0x00456150 – TileMap_ScheduleRender
 *   0x00456700 – TileMap_RenderRect
 *   0x00456d10 – WorldDrawChain_Clip
 *   0x00455840 – MarkDirtyTiles
 *   0x0040b610 – Train_FindWaypointInTile
 *   0x0040b740 – Train_SnapToTile
 *   0x0040b880 – Train_FindNextTileNode
 *   0x0040bbd0 – Train_Update
 *   0x0040c3d0 – Train_CheckStationGap
 *   0x0040c460 – Train_JunctionRouting
 *   0x0040cb10 – Train_HandleTunnelPortal
 *   0x0044ca50 – Train_HandleStationArrival
 *   0x0040d500 – TrainEntity_ctor
 *   0x0040dc20 – TrainMoveAlongPath
 *   0x0040d940 – TrainUpdate
 *   0x0040db90 – TrainCheckStation
 *   0x0040df80 – ComputeHeading
 *   0x0040d8e0 – RepositionFromPathSlot
 *   0x0040e440 – StationApproachCheck
 *   0x0040e520 – StationDepartCheck
 *   0x0040e2a0 – CheckOffscreenBounds
 *   0x0040e340 – CheckSignal
 *   0x0040d0b0 – SpriteEntity_Init
 *   0x0040d2a0 – SpriteEntity_SetFrame
 *   0x0040d2f0 – AnimTick
 *   0x0040d470 – Sprite_AdvanceFrame
 *   0x0040ec70 – InitPathSlot
 *   0x0040eb60 – ClassifyTileType
 *   0x0040ecf0 – FreeLinkedNode
 *   0x0040ed10 – LinkEntity
 *   0x0045ce40 – BuildPathGraph_A
 *   0x0045d1c0 – BuildPathGraph_B
 *   0x0045dad0 – LinkPathNodePairs
 *   0x00406480 – Config_LoadBalancing
 *   0x00429490 – SaveGame_ListSlots
 *   0x00429a10 – SaveGame_Load
 *   0x00429b20 – SaveGame_Save
 *   0x00429dd0 – SaveGame_Delete
 *
 * Win32 surface types (DirectDraw) are not referenced here; see src/graphics/.
 * The LOCO_RECT type provides a platform-independent rectangle for those callers
 * inside game logic that need clipping rectangles.
 */

#ifndef GAME_WORLD_H
#define GAME_WORLD_H

#include <stdint.h>
#include <stddef.h>

#ifdef LOCO_LINUX
#  include <SDL2/SDL.h>
#endif

/* =========================================================================
 * Platform-independent rectangle type
 *
 * Mirrors Win32 RECT: exclusive right/bottom (right = left + width).
 * SDL_Rect uses (x, y, w, h) — different! Use the adapter functions in
 * game_world.c (loco_RECT_to_SDL / loco_SDL_to_RECT) when calling SDL APIs.
 * =========================================================================*/
#ifdef LOCO_WIN32
#  include <windows.h>
   typedef RECT LOCO_RECT;
#else
typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} LOCO_RECT;
#endif

/* =========================================================================
 * Grid constants
 *
 * The tile map is an 82 × 66 × 16-layer grid of entity pointers.
 * Stride formulae (from binary; deduced from x_stride = y_count * z_size):
 *   x stride in bytes = 0x1040  (= 65 * 0x40, or 65 rows × 64 bytes)
 *   y stride in bytes = 0x40    (= 16 z-slots × 4 bytes each)
 *   z stride in bytes = 4       (one TileNode* per layer slot)
 *
 * Note: x_stride covers 65 y-rows not 66; confirmed by DAT_004aad46 = 0x41.
 * The extra y-row (index 65) is valid but the stride does not include it in
 * the x step; bounds checks must still allow y up to 65 inclusive.
 *
 * Pixel ↔ tile conversion: 16 sub-pixels per tile unit (right-shift by 4).
 * =========================================================================*/
#define TILEMAP_GRID_X     82   /* 0x52 — columns (x) */
#define TILEMAP_GRID_Y     66   /* 0x42 — rows    (y) */
#define TILEMAP_GRID_Z     16   /* layers per (x,y) cell */
#define TILEMAP_XSTRIDE    0x1040  /* bytes between adjacent x columns */
#define TILEMAP_YSTRIDE    0x40    /* bytes between adjacent y rows */
#define TILEMAP_ZSTRIDE    4       /* bytes between adjacent z layers */
#define TILEMAP_VIS_XSTRIDE 0x41   /* visibility bitmap x-stride (DAT_004aad46) */
#define TILE_PIXELS        16      /* pixel units per tile (shift-by-4 factor) */

/* Maximum heading index for train sprites.
 * heading_index runs 0..0x7F (128 directions); 0x80 wraps back to 0. */
#define TRAIN_MAX_HEADING  0x80

/* Initial frame counter for a newly created CarSlot (InitPathSlot). */
#define CAR_SLOT_INIT_FRAME  100

/* =========================================================================
 * TileType — track tile categories (from ClassifyTileType, 0x0040eb60)
 *
 * Maps sprite IDs to functional category used by the train system.
 * Non-track tiles return TILE_TYPE_NONE (0).
 * =========================================================================*/
typedef enum TileType {
    TILE_TYPE_NONE      = 0,  /* no track / building / decoration */
    TILE_TYPE_STRAIGHT  = 1,  /* straight rail: sprite IDs 0x1804–0x1808 */
    TILE_TYPE_CURVE     = 2,  /* curved rail:   sprite IDs 0x1866–0x186a */
    TILE_TYPE_JUNCTION  = 3,  /* switch/junction: sprite IDs 0x186c–0x186e */
    TILE_TYPE_STATION   = 4   /* station / signal platform: IDs 0x1870–0x1871 */
} TileType;

/* =========================================================================
 * TileState — state values stored at TileNode+0x110
 *
 * Used by the train scheduler and junction router.
 * =========================================================================*/
typedef enum TileState {
    TILE_STATE_FREE        = 0, /* cell unoccupied */
    TILE_STATE_LOADING     = 1, /* train currently loading at station */
    TILE_STATE_EMPTY       = 2, /* station empty / train may proceed */
    TILE_STATE_BLOCKED     = 3, /* cell blocked (collision check) */
    TILE_STATE_JUNCTION_A  = 4, /* junction: direction state A (default) */
    TILE_STATE_JUNCTION_B  = 5, /* junction: direction state B (toggled) */
    TILE_STATE_TUNNEL      = 5  /* tunnel tile (same value as JUNCTION_B; context-dependent) */
} TileState;

/* =========================================================================
 * PortalDir — direction byte values at TileData+0x63a for tunnel/station tiles
 * =========================================================================*/
typedef enum PortalDir {
    PORTAL_DIR_NONE    = 0,
    PORTAL_DIR_EAST    = 7,   /* train exits tile heading east */
    PORTAL_DIR_WEST    = 8,   /* train exits tile heading west */
    PORTAL_DIR_SOUTH   = 9,   /* train exits tile heading south */
    PORTAL_DIR_NORTH   = 10,  /* train exits tile heading north */
    PORTAL_DIR_STATION_A = 18, /* station tile type A */
    PORTAL_DIR_STATION_B = 19  /* station tile type B */
} PortalDir;

/* =========================================================================
 * TrainMotionState — values at TrainEntity+0x440 (motion_state)
 * =========================================================================*/
typedef enum TrainMotionState {
    MOTION_NORMAL   = 0,  /* train traversing tiles normally */
    MOTION_WAITING  = 1,  /* blocked; signal or switch ahead */
    MOTION_STOPPED  = 2   /* stopped / off-screen / reversing */
} TrainMotionState;

/* =========================================================================
 * TrainStationState — values at TrainEntity+0x444 (station_state)
 *
 * Controls the docking / departure sequence at platform tiles.
 * =========================================================================*/
typedef enum TrainStationState {
    STATION_CLEAR       = 0, /* not near a station */
    STATION_APPROACHING = 1, /* lead car has entered station zone */
    STATION_DOCKED      = 2, /* train stopped at platform */
    STATION_DEPARTING   = 4, /* both cars unblocked; train leaving */
    STATION_TUNNEL_EXIT = 5  /* special: train routed through tunnel portal */
} TrainStationState;

/* =========================================================================
 * SpriteResource — in-memory sprite/tile data blob
 *
 * The original game reads binary sprite data from .RFD resource files into
 * heap memory.  This struct documents the fields accessed by the runtime
 * at known byte offsets.  Fields between documented offsets are raw padding
 * from the binary format and are not used by the decompiled code.
 *
 * A SpriteResource is pointed to by SpriteEntity->sprite_resource (+0x44).
 * For rail tiles this blob also contains pathfinding data (waypoints, etc.)
 * starting at offset +0x630.
 *
 * WIN32: loaded via the resource-file subsystem in src/resources/.
 * LINUX: same loader; no platform dependencies in the data layout.
 * =========================================================================*/
typedef struct SpriteResource {
    /* +0x00..+0x13: internal resource header (RFH/RFD format metadata). */
    uint8_t  _rfd_header[0x14];

    /* +0x14 */ uint16_t sprite_width;      /* pixel width of one animation frame
                                             * (used by RepositionFromPathSlot, +0x14) */
    /* +0x16 */ uint16_t sprite_height;     /* pixel height of one frame (+0x16) */

    /* +0x18..+0x27: more format fields (frame count, palette refs). */
    uint8_t  _res_mid[0x10];

    /* +0x28 */ uint16_t tile_px_width;     /* tile footprint width in pixels;
                                             * used by SpriteEntity_Init for SetRect */
    /* +0x2a */ uint16_t tile_px_height;    /* tile footprint height in pixels */
    /* +0x2c */ uint16_t frame_count;       /* total animation frames in sheet */
    /* +0x2e */ int16_t  hotspot_x;         /* sprite hotspot column (blit anchor) */
    /* +0x30 */ int16_t  hotspot_y;         /* sprite hotspot row (blit anchor) */

    /* +0x32..+0x167: sprite sheet data and additional metadata. */
    uint8_t  _sprite_data[0x136];

    /* +0x168: per-heading hotspot table.
     * Layout: int16_t pairs (hotspot_x, hotspot_y) indexed by heading_index.
     * heading_index runs 0..0x7F (128 directions); each entry is 4 bytes.
     * Access: sprite+0x168 + heading_index * 4
     * Used by RepositionFromPathSlot (0x0040d8e0) to align the train sprite. */
    int16_t  heading_hotspot[128][2]; /* [heading][0]=x, [heading][1]=y */

    /* +0x368..+0x629: additional sprite and collision data (undocumented). */
    uint8_t  _extra[0x2c2];

    /* ---- Pathfinding data (rail tiles only) ---- */

    /* +0x630: waypoint coordinate table.
     * Array of int16_t pairs (x_off, y_off) = pixel offsets from tile origin.
     * Tile origin = (tile_grid_x * 16, tile_grid_y * 16).
     * Each waypoint occupies 4 bytes (two int16_t values).
     * Count is given by waypoint_count below.
     * Used by Train_FindWaypointInTile, TrainMoveAlongPath. */
    int16_t  waypoints[64][2];       /* up to 64 waypoints; [i][0]=x_off, [i][1]=y_off */

    /* +0x830: gap between waypoints array and known count fields. */
    uint8_t  _wp_gap[6];

    /* +0x636 */ int16_t  waypoint_count;   /* number of valid waypoints entries */
    /* +0x638 */ int16_t  junction_count;   /* number of junction-specific waypoints */
    /* +0x63a */ uint8_t  direction_type;   /* PortalDir: 7=E, 8=W, 9=S, 10=N portal;
                                             * 18/19=station; 0=normal track */

    /* +0x63b: tile footprint in tile units (used by Train_HandleTunnelPortal).
     * NOTE: these overlap with the heading_hotspot table layout above because
     * the SpriteResource is a union-like binary blob; rail tiles do not use
     * the heading table, and building tiles do not use the waypoint data. */
    /* +0x16b */ /* uint8_t tile_footprint_w; — documented alias within blob */
    /* +0x16c */ /* uint8_t tile_footprint_h; — documented alias within blob */
} SpriteResource;

/* Accessor macros for tile footprint bytes (stored at different offsets
 * depending on tile type; rail tiles use +0x63b/+0x63c, buildings +0x16b/+0x16c).
 * Use these instead of direct field access to document intent. */
#define SPRITE_TILE_FP_W(sr)  (((uint8_t*)(sr))[0x16b])
#define SPRITE_TILE_FP_H(sr)  (((uint8_t*)(sr))[0x16c])

/* =========================================================================
 * SpriteEntity — base game entity with sprite rendering
 *
 * All visible game objects (tiles, buildings, trains, minifigs) inherit from
 * this base class.  The original Win32 code uses __thiscall on all methods;
 * the Linux port passes 'self' explicitly.
 *
 * vtable dispatch offsets referenced from the decompile:
 *   +0x0c  move callback (called by RepositionFromPathSlot)
 *   +0x18  SetFrame virtual (called by AnimTick -> SpriteEntity_SetFrame)
 *   +0x20  request redraw (called after frame change, entity move)
 *   +0x24  dock/depart callback (called by StationApproachCheck/DepartCheck)
 *   +0x28  (unknown)
 *   +0x2c  draw background tile
 *   +0x30  draw overlay tile
 * =========================================================================*/
typedef struct SpriteEntity {
    /* +0x00 */ void      **vtable;          /* C++ vtable pointer */
    /* +0x04 */ uint8_t    _base[4];         /* base-class ref/flags from FUN_00405790 */

    /* +0x08 */ LOCO_RECT   world_rect;      /* entity bounding rect in screen pixels
                                              * (left/top/right/bottom, exclusive) */

    /* +0x18 */ uint8_t     active;          /* 1 = entity is live / visible */
    /* +0x19 */ uint8_t     _pad19[3];

    /* +0x1c */ uint8_t     _base2[8];       /* additional base-class fields */

    /* +0x24 */ void       *world_ctx;       /* scene/world context pointer
                                              * (TileMap or equivalent container);
                                              * set by SpriteEntity_Init (+0x24) */

    /* +0x28 */ int32_t     tile_frame_idx;  /* animation state index (set by vtable calls) */

    /* +0x2c */ uint16_t    entity_flags;    /* bitfield:
                                              *   bit 0: scrolls with world viewport
                                              *   bit 1: animated (cycling frames) */
    /* +0x2e */ int16_t     sprite_x_offset; /* hotspot column relative to tile origin */
    /* +0x30 */ int16_t     sprite_y_offset; /* hotspot row relative to tile origin */
    /* +0x32 */ uint8_t     _pad32[2];

    /* +0x34 */ int32_t     src_left;        /* source rect in sprite sheet:
                                              * left = current_frame * frame_width */
    /* +0x38 */ int32_t     src_top;
    /* +0x3c */ int32_t     src_right;       /* right = (current_frame+1) * frame_width */
    /* +0x40 */ int32_t     src_bottom;

    /* +0x44 */ SpriteResource *sprite_resource; /* sprite sheet + pathfinding data;
                                              * +0x28=tile_px_width, +0x2a=tile_px_height,
                                              * +0x2c=frame_count, +0x2e/+0x30=hotspot */

    /* +0x48 */ uint16_t    anim_type;       /* 1 = cycling animation enabled */
    /* +0x4a */ uint8_t     _pad4a[2];
    /* +0x4c */ int32_t     current_frame;   /* current animation frame index */
    /* +0x50 */ int32_t     frame_skip_ctr;  /* reset to 0 on SetFrame; counts ticks
                                              * AnimTick fires every 3 ticks */
    /* +0x54 */ uint16_t    unknown_54;      /* initialised to 0xFFFF by SpriteEntity_Init */
} SpriteEntity;

/* =========================================================================
 * TileNode — a tile entity placed on the world grid
 *
 * TileNode extends SpriteEntity.  The grid position (tile_x, tile_y) is
 * packed into two int16_t fields at +0x88/+0x8a.  The tile runtime state
 * (occupied, junction direction, etc.) is at +0x110.
 *
 * For rail tiles, sprite_resource (+0x44) contains the waypoint/direction
 * data at offsets +0x630 onwards (see SpriteResource above).
 * =========================================================================*/
typedef struct TileNode {
    /* SpriteEntity base — +0x00..+0x55 */
    SpriteEntity  base;

    /* +0x56..+0x87: additional entity fields (animation cue, sub-type, etc.)
     * Not fully documented; do not access directly. */
    uint8_t       _mid[0x32];

    /* +0x88 */ int16_t  tile_x;         /* grid column (0..81) of this tile */
    /* +0x8a */ int16_t  tile_y;         /* grid row    (0..65) of this tile */

    /* +0x8c..+0x10f: building/animation state (not fully mapped). */
    uint8_t       _lower[0x84];

    /* +0x110 */ int32_t  tile_state;    /* TileState enum:
                                          * 1=loading, 2=empty, 3=blocked,
                                          * 4=junction_A, 5=junction_B/tunnel */
    /* +0x114 */ int32_t  ref_count;     /* vehicle occupancy ref count;
                                          * incremented on enter, decremented on exit;
                                          * > 0 means tile is occupied */
    /* +0x118 */ void    *vehicle_occupancy; /* pointer to occupying vehicle (if any) */
    /* +0x11c */ int32_t  block_flag;    /* signal block: 1=blocked, 0=clear;
                                          * cleared by CheckSignal when track is free */
} TileNode;

/* =========================================================================
 * TileMap — global isometric tile grid object at DAT_004aad08
 *
 * The tile grid is stored as two parallel pointer arrays (primary and secondary)
 * each holding TileNode* for every (x, y, z) cell.  The struct header at
 * the start of the object contains bookkeeping and scroll state.
 *
 * Both primary_base and secondary_base point to separately heap-allocated
 * arrays of (TileNode*) indexed by:
 *     index = x * (TILEMAP_XSTRIDE / sizeof(TileNode*))
 *           + y * (TILEMAP_YSTRIDE / sizeof(TileNode*))
 *           + z
 *
 * primary  (at map+0x48): building/entity pointers per cell
 * secondary(at map+0x64): used for tile coordinate resolution (train waypoints)
 *
 * The layer_count_base array (at map+0x80) stores a per-(x,y) layer count
 * (minimum 2 enforced by TileMap_RenderRect).
 * =========================================================================*/
typedef struct TileMap {
    /* +0x00..+0x1b: internal header (vtable, ref counts, flags). */
    uint8_t   _header[0x1c];

    /* +0x1c */ int32_t  scroll_x;       /* pixel X scroll position */
    /* +0x20 */ int32_t  scroll_y;       /* pixel Y scroll position */

    /* +0x24..+0x3b: build-mode state, animation counters, etc. */
    uint8_t   _mid[0x18];

    /* +0x3c */ uint8_t  _pad3c[2];
    /* +0x3e */ int16_t  grid_width;     /* tile columns; 82 (0x52) */
    /* +0x40 */ int16_t  grid_height;    /* tile rows;    66 (0x42) */

    /* +0x42..+0x47: padding / unknown fields. */
    uint8_t   _pad42[6];

    /* +0x48 */ TileNode **primary_base;   /* pointer to primary cell array;
                                            * indexed as primary_base[x*0x410 + y*0x10 + z]
                                            * (strides in TileNode* units, not bytes) */

    /* +0x4c..+0x63: additional header fields (build-cursor rect, flags). */
    uint8_t   _gap4c[0x18];

    /* +0x64 */ TileNode **secondary_base; /* pointer to secondary cell array;
                                            * same index formula as primary_base;
                                            * used for coordinate resolution in
                                            * TileMap_GetCoordFromNode variants */

    /* +0x68..+0x7f: further metadata. */
    uint8_t   _gap68[0x18];

    /* +0x80 */ uint8_t  *layer_count_base; /* pointer to per-(x,y) layer count array;
                                             * index = x * 0x410 + y (same x-stride, z=0);
                                             * minimum value enforced = 2 */

    /* +0x84..+0xa7: other fields including MinBuildingFPS accumulator at +0xa8. */
    uint8_t   _gap84[0x24];

    /* +0xa8 */ int32_t  min_building_fps_demand; /* running maximum of animation FPS
                                                    * demand across all active sprites;
                                                    * updated by SpriteEntity_Init and
                                                    * Sprite_AdvanceFrame */
} TileMap;

/* =========================================================================
 * CarSlot — path-tracking slot for one train car end
 *
 * Each TrainEntity has two CarSlots: car_front (+0x430) and car_rear (+0x434).
 * A CarSlot records the car's current position on the tile path graph, its
 * screen coordinates, and docking/signal flags.
 *
 * Allocated by FUN_0040b500 (size approximately 0x3c bytes per slot).
 * Initialised by InitPathSlot (0x0040ec70).
 * =========================================================================*/
typedef struct CarSlot {
    /* +0x00 */ TileNode  *tile_entity;    /* current tile entity (TileNode*) this
                                            * car is on; NULL if not on a tile */

    /* +0x04 */ int32_t    direction;      /* movement direction (initially 2 = neutral;
                                            * updated by train state machine;
                                            * 1=forward, 4=reverse approx.) */
    /* +0x08 */ int32_t    frame_index;    /* frame within current waypoint segment;
                                            * 0..waypoint_count-1;
                                            * init: CAR_SLOT_INIT_FRAME (100) */
    /* +0x0c */ int32_t    screen_x;       /* car screen pixel X position */
    /* +0x10 */ int32_t    screen_y;       /* car screen pixel Y position */

    /* +0x14 */ void      *tile_ptr;       /* secondary tile reference (for signal check) */
    /* +0x18 */ int32_t    direction_active; /* 1 = direction tracking enabled */
    /* +0x1c */ int32_t    blocked;        /* 1 = car is blocked (station or signal) */

    /* +0x20..+0x37: reserved / undocumented fields. */
    uint8_t    _reserved[0x18];

    /* +0x38 */ int32_t    extra;          /* cleared by InitPathSlot; purpose unknown */
} CarSlot;

/* =========================================================================
 * TrainEntity — train agent (extends SpriteEntity)
 *
 * A TrainEntity is a moving game agent.  It owns two CarSlot objects
 * (front and rear) that track position on the tile path graph.  The heading
 * is computed each frame from the vector between the two car endpoints.
 *
 * Constructed by TrainEntity_ctor (0x0040d500) which calls
 * the SpriteEntity base constructor at FUN_00405790 and then sets up the
 * vtable (PTR_FUN_00477590) and the car slots.
 *
 * Layout note: SpriteEntity base fields occupy roughly +0x00..+0x417.
 * The extension fields listed here start at +0x420.
 * =========================================================================*/
typedef struct TrainEntity {
    /* SpriteEntity base — approximately +0x00..+0x417 */
    SpriteEntity  base;

    /* Gap between SpriteEntity fields and TrainEntity extension fields.
     * Contains animation state and additional sprite data. */
    uint8_t       _mid[0x3c2];    /* pad from end of SpriteEntity to +0x420 */

    /* +0x420..+0x427: train-specific sprite metadata. */
    uint8_t       _train_sprite_meta[8];

    /* +0x428 */ int32_t    tile_id;        /* resource ID of the train sprite */

    /* +0x42c */ uint8_t    _pad42c[4];

    /* +0x430 */ CarSlot   *car_front;      /* path slot for the leading car end */
    /* +0x434 */ CarSlot   *car_rear;       /* path slot for the trailing car end */

    /* +0x438 */ int16_t    heading_index;  /* sprite heading index, 0..0x7F;
                                             * computed by ComputeHeading from
                                             * car screen_x/y delta; wraps at 0x80 */
    /* +0x43a */ uint8_t    _pad43a[6];

    /* +0x440 */ int32_t    motion_state;   /* TrainMotionState: 0=normal, 1=waiting,
                                             * 2=stopped */
    /* +0x444 */ int32_t    station_state;  /* TrainStationState: 0=clear, 1=approach,
                                             * 2=docked, 4=depart, 5=tunnel-exit */
    /* +0x448 */ int32_t    speed_state;    /* 0=slow (within 50 frames of boundary),
                                             * 1=full speed */

    /* +0x44c...: additional train-specific fields (signal queue, sounds, etc.) */
    /* +0x5c */ int32_t    stop_request;    /* flag set by station stop logic;
                                             * absolute offset within full struct */
    /* +0x60 */ int32_t    direction_flag;  /* 1=forward, 4=reverse;
                                             * toggled by FUN_0044cb10 on platform swap */
} TrainEntity;

/* =========================================================================
 * PathNode — node in the train path graph
 *
 * Built by BuildPathGraph_A/B (0x0045ce40 / 0x0045d1c0).
 * One PathNode exists per rail TileNode.
 * Size: 0x2c bytes.
 * =========================================================================*/
typedef struct PathNode {
    /* +0x00 */ TileNode  *tile;          /* the TileNode this path node covers */
    /* +0x04 */ int32_t    node_index;    /* index in the global path node array;
                                           * stored back into tile+0xe4 (type A)
                                           * or tile+0x108 (type B) */
    /* +0x08 */ void      *edges;         /* linked list of PathEdge (next ptr at edge+4) */
    /* +0x0c */ int32_t    neighbor_dirs[4]; /* direction to each connected neighbor
                                             * (0=N, 1=E, 2=S, 3=W) */
    /* +0x1c */ uint8_t    _reserved[0x10];
} PathNode;

/* =========================================================================
 * PathEdge — directed edge in the train path graph
 *
 * Each PathEdge connects two PathNodes.  The edge stores a cost (always 1
 * in the current game), a pointer to the next edge in the adjacency list,
 * and the from/to node indices.
 * Size: 0x10 bytes.
 *
 * GetOppositePath direction mapping:
 *   0 (N) <-> 2 (S)
 *   1 (E) <-> 3 (W)
 * =========================================================================*/
typedef struct PathEdge {
    /* +0x00 */ int32_t    cost;          /* edge traversal cost (always 1) */
    /* +0x04 */ void      *next;          /* next PathEdge in adjacency list or NULL */
    /* +0x08 */ int32_t    from_idx;      /* source PathNode index */
    /* +0x0c */ int32_t    to_idx;        /* destination PathNode index */
} PathEdge;

/* =========================================================================
 * SaveSlot — one entry in the save game slot list
 *
 * Allocated by SaveGame_ListSlots (0x00429490) for each .sav file found.
 * Stored in a sorted singly-linked list at world+0x4d8.
 * Size: 0x230 bytes.
 * =========================================================================*/
typedef struct SaveSlot {
    /* +0x000 */ char       slot_name[10];   /* base filename without .sav (≤10 chars) */
    /* +0x00a */ uint8_t    _pad00a[2];
    /* +0x00c */ char       full_path[0x40]; /* complete path to .sav file */
    /* +0x04c */ uint8_t    _meta[0x1e0];    /* additional slot metadata (world name etc.) */
    /* +0x22c */ struct SaveSlot *next;       /* next slot in sorted list (by slot_name) */
} SaveSlot;

/* =========================================================================
 * GameConfig — performance-balancing configuration
 *
 * Loaded by Config_LoadBalancing (0x00406480) from the [BALANCING] section
 * of the game INI file.  Field offsets are relative to the config object;
 * original offsets from the binary are noted in comments.
 * =========================================================================*/
typedef struct GameConfig {
    /* +0x00..+0x0d: other config fields (not part of BALANCING section) */
    uint8_t  _other[0x0e];

    /* +0x0e */ int8_t   min_flying_fps;   /* MinFlyingFPS (default 14) */
    /* +0x0f */ uint8_t  _pad0f;
    /* +0x10 */ int8_t   min_minifig_fps;  /* MinMinifigFPS (default 16) */
    /* +0x11 */ int8_t   min_vehicle_fps;  /* MinVehicleFPS (default 20) */
    /* +0x12 */ int8_t   min_building_fps; /* MinBuildingFPS (default 18) */

    /* +0x13 */ int8_t   min_flying_fps2;  /* MinFlyingFPS second copy (default 14) */

    /* +0x14...: window rect and clean-exit flag from other INI sections */
    uint8_t  _rest[0x20];
} GameConfig;

/* =========================================================================
 * DrawChainNode — node in the world Y-sorted draw chain
 *
 * The draw chain is a singly-linked list of 20-byte nodes used to render
 * entities in Y-sorted order.  Each node stores a screen RECT and a next
 * pointer packed into the RECT's second slot.
 *
 * Layout observed in WorldDrawChain_Clip (0x00456d10):
 *   bytes 0..15:  LOCO_RECT (16 bytes: left, top, right, bottom)
 *   bytes 16..19: next pointer (stored in RECT[1].left field)
 *
 * Invalid rects arise when an entity is freshly constructed but not yet
 * placed, or when a train exits the world without removal from the chain.
 * =========================================================================*/
typedef struct DrawChainNode {
    /* +0x00 */ LOCO_RECT          rect;  /* screen bounding rect of entity */
    /* +0x10 */ struct DrawChainNode *next; /* next node in Y-sorted chain */
} DrawChainNode;

/* =========================================================================
 * Global singletons
 *
 * These are static globals in the original binary at fixed PE addresses.
 * In the Linux port, declare as regular C globals and initialise in main.
 * =========================================================================*/

/* DAT_004aad08 — TileMap object (the complete town grid) */
extern TileMap   *g_tile_map;

/* DAT_004aad0c — maximum valid tile X coordinate (82 columns → max index 81) */
extern int32_t    g_tile_map_max_x;

/* DAT_004aad10 — maximum valid tile Y coordinate (66 rows → max index 65) */
extern int32_t    g_tile_map_max_y;

/* DAT_004aad14 — world viewport RECT (screen coords); used by WorldDrawChain_Clip */
extern LOCO_RECT  g_world_viewport;

/* DAT_004aad46 — x-stride in tile units for the visibility bitmap (= 0x41 = 65) */
extern int32_t    g_vis_x_stride;

/* DAT_004fd18c — visibility bit-array; bit (y*65+x) set means tile is on screen */
extern uint8_t   *g_vis_bitmap;

/* DAT_004a9990 — World/Game object; passed to the save/load serialiser */
extern void      *g_world_obj;

/* DAT_00485228 — viewport width in pixels */
extern int32_t    g_viewport_width;

/* DAT_00485228+0x2c — viewport height in pixels */
extern int32_t    g_viewport_height;

/* DAT_00485328 — build-mode flag; when set, TileMap_ScheduleRender clips to cursor */
extern int32_t    g_build_mode;

/* DAT_0047f108 — bitmask lookup table for MarkDirtyTiles (8 entries, one per bit) */
extern uint8_t    g_bitmask_lut[8];

/* =========================================================================
 * Platform-independent RECT helpers
 *
 * Replace Win32 user32.dll SetRect / IntersectRect / IsRectEmpty.
 * WIN32: macro-delegate to the real Win32 functions (see below).
 * LINUX: implemented in game_world.c directly on LOCO_RECT.
 * =========================================================================*/
#ifdef LOCO_WIN32
#   define loco_SetRect(r, l, t, ri, b)  SetRect((r), (l), (t), (ri), (b))
#   define loco_IntersectRect(d, a, b)   IntersectRect((d), (a), (b))
#   define loco_IsRectEmpty(r)           IsRectEmpty((r))
#else
void loco_SetRect(LOCO_RECT *r, int l, int t, int ri, int b);
int  loco_IntersectRect(LOCO_RECT *dst, const LOCO_RECT *a, const LOCO_RECT *b);
int  loco_IsRectEmpty(const LOCO_RECT *r);
#endif

#ifdef LOCO_LINUX
SDL_Rect  loco_RECT_to_SDL(const LOCO_RECT *r);
LOCO_RECT loco_SDL_to_RECT(const SDL_Rect  *s);
#endif

/* =========================================================================
 * Function declarations — Tile Grid
 * =========================================================================*/

/*
 * TileMap_GetCell  (0x00455620)
 * Returns the TileNode* stored in the primary cell array at (x, y, z).
 * Returns NULL if any coordinate is out of range.
 * Bounds: x 0–81, y 0–65, z 0–15.
 *
 * WIN32/LINUX: pure memory access; no platform dependency.
 */
TileNode *TileMap_GetCell(TileMap *map, int x, int y, int z);

/*
 * TileMap_GetCoordFromNode  (0x004557c0)
 * Reads the TileNode* from the SECONDARY cell array at (x, y, z).
 * On success writes the TileNode's packed grid coord (TileNode+0x88) into
 * *out_coord (a packed int32: high 16 = tile_x, low 16 = tile_y).
 * On miss writes 0xFFFFFFFF into *out_coord.
 *
 * Used by the train waypoint system to verify tile coordinates.
 */
void TileMap_GetCoordFromNode(TileMap *map, int x, int y, int z, int32_t *out_coord);

/*
 * TileMap_GetCoordFromNodeAlt  (0x00455740)
 * Identical to TileMap_GetCoordFromNode.
 * Called from FUN_0040b740 when snapping a train to a track tile.
 */
void TileMap_GetCoordFromNodeAlt(TileMap *map, int x, int y, int z, int32_t *out_coord);

/*
 * MarkDirtyTiles  (0x00455840)
 * Marks the visibility bitmap bits for all tile cells covered by the given
 * screen pixel rectangle.  Converts pixels to tiles (>> 4), clamps to grid
 * bounds, then sets bits in g_vis_bitmap using g_bitmask_lut.
 * Called after any entity movement to schedule those tiles for redraw.
 *
 * WIN32/LINUX: no platform dependency.
 */
void MarkDirtyTiles(TileMap *map, const LOCO_RECT *screen_rect);

/*
 * TileMap_RenderRect  (0x00456700)
 * Renders the visible tiles within a pixel rectangle to the screen.
 * Converts pixel rect to tile coords (>> 4).  For each visible tile
 * (bit set in g_vis_bitmap), iterates layers and dispatches vtable+0x2c
 * (draw background) and vtable+0x30 (draw overlay) on each TileNode.
 *
 * WIN32: vtable calls to DirectDraw blitters in src/graphics/.
 * LINUX: same vtable dispatch; blitters replaced by SDL2 equivalents.
 */
void TileMap_RenderRect(TileMap *map, int left, int top, int right, int bottom);

/*
 * TileMap_ScheduleRender  (0x00456150)
 * Computes the visible tile range from scroll position and viewport size,
 * builds dirty-rect linked list nodes, dispatches TileMap_RenderRect for
 * each dirty region, then zeroes the visibility bitmap.
 *
 * WIN32: reads DirectDraw viewport dimensions from DAT_00485228.
 * LINUX: replace DAT_00485228 reads with SDL_GetWindowSize() equivalents.
 */
void TileMap_ScheduleRender(TileMap *map, char full_redraw);

/*
 * WorldDrawChain_Clip  (0x00456d10)
 * Walks the Y-sorted draw chain.  For each node calls IntersectRect against
 * the world viewport (g_world_viewport).  Invalid rects (no intersection or
 * zero-area) are unlinked and freed; valid rects are clamped to viewport.
 *
 * WIN32: uses IntersectRect (user32.dll), OutputDebugStringA (kernel32.dll).
 * LINUX: use loco_IntersectRect; replace OutputDebugStringA with fprintf(stderr).
 */
void WorldDrawChain_Clip(DrawChainNode **chain_head);

/* =========================================================================
 * Function declarations — Train Navigation
 * =========================================================================*/

/*
 * Train_FindWaypointInTile  (0x0040b610)
 * Finds the waypoint index within a TileNode that matches the given pixel
 * position (px, py).  Reads TileNode+0x88/0x8a scaled by 16 as tile origin,
 * then searches sprite_resource->waypoints[] for a matching (x_off, y_off).
 * Writes train+0x08 (waypoint index) and train+0x10 (y pixel).
 *
 * Used when a train is placed or re-attached to a track segment.
 */
void Train_FindWaypointInTile(TrainEntity *train, TileNode *tile, int px, int py);

/*
 * Train_SnapToTile  (0x0040b740)
 * Snaps a train to the tile at pixel position (px, py).
 * Divides by TILE_PIXELS to get tile (x, y), calls TileMap_GetCell(z=0),
 * verifies tile type == TILE_TYPE_STRAIGHT (via FUN_00446030), iterates
 * waypoints to find closest index, and writes train+0x14 (tile pointer),
 * train+0x08 (waypoint index), train+0x0c/+0x10 (pixel x,y).
 */
void Train_SnapToTile(TrainEntity *train, int px, int py);

/*
 * Train_FindNextTileNode  (0x0040b880)
 * Searches the tile grid for the TileNode a train should enter next.
 * Reads train's current TileNode+0x88/0x8a and sprite_resource->waypoints
 * to compute target pixel position, then calls TileMap_GetCell.
 * Checks direction_type (+0x63a) and waypoint/junction counts (+0x636/+0x638).
 * Updates train+0x14 (tile), +0x04 (direction), +0x08 (waypoint index).
 * Returns 0 on successful transition, old tile pointer on failure.
 */
TileNode *Train_FindNextTileNode(TrainEntity *train, void *param_1);

/*
 * Train_Update  (0x0040bbd0)
 * Main per-tick train movement state machine (__thiscall on train agent).
 * Dispatches on train+0x18 (state) and train+0x1c (state2):
 *   State 0 (normal traverse): advances waypoint via train+0x08, updates
 *     pixel x/y from waypoints table.
 *   State 4 (junction): adjusts pixel coords per direction byte.
 * Checks world bounds (g_tile_map_max_x, g_tile_map_max_y).
 * Calls Train_FindNextTileNode when waypoints exhausted.
 * Updates TileNode+0x114 (ref_count) on tile enter/exit.
 *
 * WIN32: __thiscall — 'self' passed in ECX.
 * LINUX: explicit 'self' parameter.
 */
void Train_Update(TrainEntity *self, void *station_ctx);

/*
 * Train_HandleTunnelPortal  (0x0040cb10)
 * Handles tunnel portal traversal.  Reads direction_type (7-10 for portals).
 * Increments/decrements train+0x0c or train+0x10 one step per tick.
 * Checks tile bounds from TileNode+0x88/0x8a and tile footprint size.
 * On valid traverse calls FUN_0040b610 to snap waypoint and clears state2.
 * Returns 1 if transition succeeded, 0 if still inside tunnel.
 */
int Train_HandleTunnelPortal(TrainEntity *train, TileNode *portal_tile);

/*
 * Train_CheckStationGap  (0x0040c3d0)
 * Tests whether a station tile has a safe gap for train entry.
 * Reads direction_type from sprite_resource+0x63a (18 or 19 = station).
 * Reads train+0x36 (stop state): if 1 clears and returns 0.
 * Calls FUN_0044c370 to check for a gap.  On valid gap at waypoint 1 or
 * (count-1), sets TileNode+0x118 (vehicle_occupancy) and train+0x36=200.
 * Returns 1 if train may enter, 0 if blocked.
 */
int Train_CheckStationGap(TrainEntity *train, TileNode *station_tile);

/*
 * Train_JunctionRouting  (0x0040c460)
 * Junction routing (__thiscall on train).
 * Reads TileNode+0x114 (ref_count): if > 0, stops train (priority 1).
 * Checks all four boundary waypoints against train direction/waypoint.
 * Toggles TileNode+0x110 between TILE_STATE_JUNCTION_A and _B.
 * Calls Train_FindNextTileNode after each toggle.
 * Dispatches vtable+0x1c (trigger event) for switch types 4 or 5.
 */
void Train_JunctionRouting(TrainEntity *train, TileNode *junction_tile);

/*
 * Train_HandleStationArrival  (0x0044ca50)
 * Station arrival handler.  Checks train+0x5c (stop_request flag).
 * Reads station_tile_node+0x110 (tile_state):
 *   1=loading → calls FUN_0044cb10 (platform swap, reverses all cars)
 *   2=empty   → calls FUN_0044d740 (dispatch; priority 2=pass-through / 1=stop)
 *   3=blocked → train waits
 * Sets train+0x36=200 (wait-for-gap state) as needed.
 */
void Train_HandleStationArrival(TrainEntity *train, TileNode *station_tile);

/* =========================================================================
 * Function declarations — Train Entity
 * =========================================================================*/

/*
 * TrainEntity_ctor  (0x0040d500)
 * Train entity constructor.  Calls SpriteEntity base ctor (FUN_00405790),
 * sets vtable PTR_FUN_00477590.  Allocates two CarSlots via FUN_0040b500
 * and initialises them with InitPathSlot.  param_3 controls lead end:
 *   param_3 == 0 → station_state=2, motion_state=0
 *   param_3 != 0 → station_state=0, motion_state=2
 *
 * WIN32: __thiscall; new is operator new from msvcrt.
 * LINUX: allocate with malloc; no C++ runtime dependency.
 */
TrainEntity *TrainEntity_ctor(TrainEntity *self, void *world_ctx,
                               SpriteResource *sprite_res, int param_3);

/*
 * TrainMoveAlongPath  (0x0040dc20)
 * Core per-frame path step.  Selects car_front or car_rear based on param_1[2].
 * At segment boundary (forward: frame == total_frames-1; backward: frame == 1):
 *   loads next screen coords from waypoints table, updates both cars.
 * Otherwise increments/decrements frame_index.
 * Speed modulation: within 50 frames of boundary → speed_state=0 (slow);
 *   after 80 frames → speed_state=1 (full speed).
 * Calls MarkDirtyTiles and recomputes heading.
 */
void TrainMoveAlongPath(TrainEntity *train, void *path_context);

/*
 * TrainUpdate  (0x0040d940)
 * Per-frame train dispatcher.  Checks both cars' tile pointers and tile_state.
 * tile_state == TILE_STATE_TUNNEL → tunnel logic (FUN_0040dc20).
 * Falls through to FUN_0040c580 for each car to advance path.
 * If either car moved: calls MarkDirtyTiles, ComputeHeading, vtable+0x20
 * (request redraw), RepositionFromPathSlot.
 * Dispatches to CheckOffscreenBounds, CheckSignal, TrainCheckStation based
 * on motion_state and station_state.
 */
void TrainUpdate(TrainEntity *train);

/*
 * TrainCheckStation  (0x0040db90)
 * Station docking logic.  Reads station_state:
 *   1 + car blocked == 1 → calls StationApproachCheck
 *   4 + both cars unblocked  → resets station_state = 0
 *   5 → calls StationDepartCheck (tunnel depart)
 */
void TrainCheckStation(TrainEntity *train);

/*
 * ComputeHeading  (0x0040df80)
 * Recomputes train heading sprite index from car screen positions.
 * Reads car_front and car_rear screen_x/y.  Uses atan2(dy, dx) → angle →
 * integer index via __ftol.  Writes to train+0x438 (heading_index, short).
 * Clamps 0x80 back to 0 (half-circle wrap).
 *
 * WIN32: uses FPU atan2 (fpatan instruction).
 * LINUX: replace with atan2f() from <math.h> + (int) cast.
 */
void ComputeHeading(TrainEntity *train);

/*
 * RepositionFromPathSlot  (0x0040d8e0)
 * Places the train entity in screen space.  Reads screen_x from car_front,
 * subtracts hotspot from sprite_resource->heading_hotspot[heading_index].
 * Sets entity bounding rect and calls vtable+0x0c (move callback).
 */
void RepositionFromPathSlot(TrainEntity *train);

/*
 * StationApproachCheck  (0x0040e440)
 * Tests whether the lead car has crossed into a station tile boundary.
 * Switch on direction_type (7=E, 8=W, 9=S, 10=N):
 *   checks corresponding edge of bounding rect.
 * On crossing: sets station_state=STATION_DOCKED, calls vtable+0x24(0).
 */
void StationApproachCheck(TrainEntity *train, TileNode *station_tile);

/*
 * StationDepartCheck  (0x0040e520)
 * Mirror of StationApproachCheck for departure.  Same direction switch.
 * If car has exited station boundary: sets station_state=STATION_DEPARTING,
 * calls vtable+0x24(1) (depart callback).
 */
void StationDepartCheck(TrainEntity *train, TileNode *station_tile);

/*
 * CheckOffscreenBounds  (0x0040e2a0)
 * Called when motion_state == MOTION_WAITING and a signal/switch is ahead.
 * Reads leading car tile direction, checks bounding rect edges against
 * g_tile_map_max_x / g_tile_map_max_y.
 * If out of bounds: sets motion_state=MOTION_STOPPED, calls vtable+0x24(0).
 */
void CheckOffscreenBounds(TrainEntity *train);

/*
 * CheckSignal  (0x0040e340)
 * Signal and switch handler.  If both cars' tile_state==0 (free):
 *   clears motion_state, calls vtable+0x24(1).
 * Looks up next tile via TileMap_GetCell at tile coords (+0x2e, +0x30).
 * If ahead tile has a signal (FUN_0044bd10) and signal is clear:
 *   resets block_flag (+0x11c).
 * If track ahead is empty (iVar4 == 0 or -1): writes -1 to tile coords.
 */
void CheckSignal(TrainEntity *train);

/* =========================================================================
 * Function declarations — Sprite Entity
 * =========================================================================*/

/*
 * SpriteEntity_Init  (0x0040d0b0)
 * Initialises a sprite entity slot (buildings, decorations).
 * Sets world_ctx (+0x24) and sprite_resource (+0x44).
 * Calls SetRect for world_rect from hotspot offsets and tile dimensions.
 * Calls SetRect for source rect (0, 0, tile_px_w, tile_px_h).
 * Sets active=1 (+0x48), entity_flags (+0x2c), unknown_54=0xFFFF (+0x54).
 * If entity_flags & 2 (animated): computes frame demand and updates
 * world_ctx->min_building_fps_demand (+0xa8) if larger than current.
 *
 * WIN32: __thiscall; SetRect from user32.dll.
 * LINUX: explicit self; loco_SetRect replaces Win32 SetRect.
 */
void SpriteEntity_Init(SpriteEntity *self, void *world_ctx,
                        SpriteResource *sprite_res, uint16_t flags);

/*
 * SpriteEntity_SetFrame  (0x0040d2a0)
 * Sets the current animation frame for a sprite entity.
 * Updates current_frame (+0x4c), computes source sprite sheet slice:
 *   src_left = frame * tile_px_width
 *   src_right = (frame+1) * tile_px_width
 * Resets frame_skip_ctr (+0x50) to 0.  Calls vtable+0x20 (request redraw).
 */
void SpriteEntity_SetFrame(SpriteEntity *self, int frame);

/*
 * AnimTick  (0x0040d2f0)
 * Per-tick animation driver for buildings.  Increments frame_skip_ctr (+0x50).
 * Fires every 3 ticks.  Cycles current_frame 0..frame_count-4 (wraps to 0).
 * Calls vtable+0x18 (SetFrame) on frame change.
 * Only active when active (+0x18) and anim_type (+0x48) are both set.
 */
void AnimTick(SpriteEntity *self);

/*
 * Sprite_AdvanceFrame  (0x0040d470)
 * Advances one animation frame.  Reads sprite_resource+0x30 (frame counter);
 * if < 4: applies scaled step x -= x*57 - 50, y -= y*57 - 40.
 * Calls SetRect to update world_rect.
 * If entity_flags & 2 (animated): recomputes frame demand = new_x/57 - 2,
 * updates world_ctx->min_building_fps_demand.
 * The constant 57 (0x39) is the animation sub-pixel scale factor.
 */
void Sprite_AdvanceFrame(SpriteEntity *self);

/* =========================================================================
 * Function declarations — Path Slot & Path Graph
 * =========================================================================*/

/*
 * InitPathSlot  (0x0040ec70)
 * Zero-initialises a CarSlot.
 * Sets direction=2 (neutral), frame_index=100, direction_active=1.
 * Clears screen_x, screen_y, tile_ptr, tile_entity, occupancy, blocked, extra.
 */
void InitPathSlot(CarSlot *slot);

/*
 * ClassifyTileType  (0x0040eb60)
 * Maps a sprite resource ID to a TileType category.
 * Returns TILE_TYPE_NONE (0) for non-track sprite IDs.
 */
TileType ClassifyTileType(uint32_t sprite_id);

/*
 * FreeLinkedNode  (0x0040ecf0)
 * Frees a singly-linked node: if *node_ptr != NULL, poisons the first dword
 * (writes 0) and sets *node_ptr = NULL.
 * Used to unlink PathEdge and similar list nodes.
 */
void FreeLinkedNode(void **node_ptr);

/*
 * LinkEntity  (0x0040ed10)
 * Bidirectionally links two list nodes:
 *   self->first = other; if other != NULL: other[0] = self.
 * Used for inserting entities into draw-chain or signal queues.
 */
void LinkEntity(void *self, void *other);

/*
 * BuildPathGraph_A  (0x0045ce40)
 * Builds the path graph for train type A (freight/cargo).
 * Enumerates rail tiles via DAT_004a9994 vtable + FUN_004573e0 filter.
 * Allocates PathNode (0x2c bytes each) per tile.
 * Reads neighbor pointers at tile+0xc4, allocates PathEdge (0x10 bytes each),
 * writes reverse direction via GetOppositePath (0↔2, 1↔3).
 * Logs 'ERROR: Invalid path in GetOppositePath' if direction >= 4.
 */
void BuildPathGraph_A(void *world_ctx);

/*
 * BuildPathGraph_B  (0x0045d1c0)
 * Builds the path graph for train type B (passenger).
 * Structurally identical to BuildPathGraph_A but reads neighbor tile pointers
 * at tile+0xe8 instead of +0xc4, and uses tile+0x108 for path-node index.
 */
void BuildPathGraph_B(void *world_ctx);

/*
 * LinkPathNodePairs  (0x0045dad0)
 * Post-processes path graph with pairwise bidirectional linking.
 * For each (i, j) pair calls FUN_0045dd80 for connectivity.
 * If result == 0x80 (bidirectional), calls FUN_0045dde0 to write direction
 * bytes and applies GetOppositePath for the reverse j→i link.
 */
void LinkPathNodePairs(PathNode *nodes, int node_count);

/* =========================================================================
 * Function declarations — Config & Save
 * =========================================================================*/

/*
 * Config_LoadBalancing  (0x00406480)
 * Loads performance-balancing config from INI via FUN_00452d60.
 * Section [BALANCING]: MinBuildingFPS (18), MinVehicleFPS (20),
 *   MinMinifigFPS (16), MinFlyingFPS (14).
 * Also reads [WINDOW_ATTRIBUTES]: RectLeft/Top/Right/Bottom.
 * And [PROCESS]: CleanExit flag.
 *
 * WIN32: INI file read via FUN_00452d60 (GetPrivateProfileInt wrapper).
 * LINUX: replace with a simple key=value INI parser.
 */
void Config_LoadBalancing(GameConfig *config, const char *ini_path);

/*
 * SaveGame_ListSlots  (0x00429490)
 * Enumerates .sav files in savegame\ directory.
 * Constructs "savegame\*.sav" (or "backdrop*.bmp" for mode 5).
 * For each file with name length <= 10: allocates a SaveSlot (0x230 bytes),
 * copies slot_name (10 chars) and full_path (0x40 chars).
 * Builds a sorted linked list at world+0x4d8.
 * Returns 1 on success, 0 on failure.
 *
 * WIN32: FindFirstFileA / FindNextFileA / FindClose (kernel32.dll).
 * LINUX: use opendir() / readdir() / closedir() from <dirent.h>.
 */
int SaveGame_ListSlots(void *world_ctx, int mode);

/*
 * SaveGame_Load  (0x00429a10)
 * Loads a save game.  Constructs path <dir>\savegame\<name>.sav.
 * Creates savegame directory if absent (CreateDirectoryA).
 * Calls TileMap_ScheduleRender(&g_tile_map, '\0') to reset tile display.
 * Calls FUN_0041d320(&g_world_obj, path) — the actual deserialiser.
 *
 * WIN32: CreateDirectoryA, full path construction.
 * LINUX: mkdir() + snprintf() path construction.
 */
void SaveGame_Load(void *world_ctx, const char *slot_name);

/*
 * SaveGame_Save  (0x00429b20)
 * Saves the current game.  Creates savegame\ if absent.
 * Constructs filename from current slot name (FUN_004490d0).
 * Frees existing SaveSlot list (dispatches vtable[0] with arg 1 per node).
 * Calls FUN_0041d9b0(&g_world_obj, path) to serialise game state.
 * Calls SaveGame_ListSlots to re-enumerate slots.
 * Performs lexicographic compare to select correct slot after save.
 *
 * WIN32: CreateDirectoryA, path construction.
 * LINUX: mkdir() + snprintf().
 */
void SaveGame_Save(void *world_ctx);

/*
 * SaveGame_Delete  (0x00429dd0)
 * Deletes a save game slot.  Constructs full path into a 260-char buffer.
 * Frees the SaveSlot linked list.  Calls DeleteFileA / unlink().
 * On success: calls FUN_00449070 with slot_name to clear world name record,
 * then FUN_00429850 to update selected slot pointer.
 *
 * WIN32: DeleteFileA (kernel32.dll).
 * LINUX: unlink() from <unistd.h>.
 */
void SaveGame_Delete(void *world_ctx, const char *slot_name);

/* =========================================================================
 * Porting notes
 *
 * TILE GRID
 *   TileMap.primary_base and secondary_base are heap-allocated arrays of
 *   TileNode* pointers.  On Linux allocate with calloc() instead of the
 *   original Win32 HeapAlloc.
 *   Access formula (pointer arithmetic, TileNode** units):
 *     cell = base[x * (TILEMAP_XSTRIDE/4) + y * (TILEMAP_YSTRIDE/4) + z]
 *
 * TRAIN PATHFINDING
 *   Uses fpatan (x87 FPU instruction) for heading computation.
 *   Replace with atan2f() in ComputeHeading on Linux.
 *   The heading_hotspot table in SpriteResource is accessed at runtime;
 *   no platform change needed — it's pure memory access.
 *
 * SAVE GAME
 *   Win32 APIs replaced as follows:
 *     FindFirstFileA / FindNextFileA / FindClose → opendir/readdir/closedir
 *     CreateDirectoryA → mkdir(path, 0755)
 *     DeleteFileA      → unlink()
 *   Path separator: replace '\\' with '/' on Linux throughout.
 *
 * DIRECTDRAW SURFACE TYPES
 *   Not referenced in this header.  The vtable calls to draw background
 *   (+0x2c) and overlay (+0x30) tiles in TileMap_RenderRect dispatch into
 *   src/graphics/ blitter code.  Replace those vtable slots with SDL2
 *   equivalents in the Linux port.
 *
 * WIN32 DEBUG STRINGS
 *   WorldDrawChain_Clip calls OutputDebugStringA.
 *   Replace with fprintf(stderr, ...) on Linux.
 * =========================================================================*/

#endif /* GAME_WORLD_H */
