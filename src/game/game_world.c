/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: Game World (isometric tile grid, buildings, trains, pathfinding)
 * Platform-independent: no Win32 APIs in core game logic
 *
 * This file implements:
 *   - Tile grid accessors (TileMap_GetCell family)
 *   - Train navigation and state machine
 *   - Sprite entity init and animation
 *   - Path graph construction for train routing
 *   - Save game slot enumeration, load, save, delete
 *   - Platform-independent RECT helpers
 *
 * Win32 API usage (each wrapped with a LINUX replacement comment):
 *   FindFirstFileA / FindNextFileA / FindClose — save slot enumeration
 *   CreateDirectoryA — create savegame\ folder
 *   DeleteFileA      — delete a save slot
 *   OutputDebugStringA — debug log in WorldDrawChain_Clip
 *   SetRect / IntersectRect / IsRectEmpty — replaced by loco_* helpers here
 *
 * DirectDraw / DirectSound / DirectPlay are NOT referenced in this file.
 * The vtable calls to +0x2c (draw background) and +0x30 (draw overlay) in
 * TileMap_RenderRect go to blitters in src/graphics/ which carry the surface
 * dependencies.
 */

#include "game_world.h"
#include "tile_desc.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>     /* atan2f — replaces x87 fpatan for ComputeHeading */

#ifdef LOCO_LINUX
#  include <SDL2/SDL.h>
#  include <dirent.h>   /* opendir/readdir/closedir for SaveGame_ListSlots */
#  include <sys/stat.h> /* mkdir */
#  include <unistd.h>   /* unlink */
#elif defined(LOCO_WIN32)
#  include <windows.h>
#endif

/* =========================================================================
 * Global singleton definitions
 * Original PE virtual addresses preserved as comments for cross-reference.
 * =========================================================================*/

TileMap   *g_tile_map        = NULL; /* DAT_004aad08 */
int32_t    g_tile_map_max_x  = 81;  /* DAT_004aad0c — max valid tile X */
int32_t    g_tile_map_max_y  = 65;  /* DAT_004aad10 — max valid tile Y */
LOCO_RECT  g_world_viewport  = {0, 0, 0, 0}; /* DAT_004aad14 */
int32_t    g_vis_x_stride    = TILEMAP_VIS_XSTRIDE; /* DAT_004aad46 = 0x41 */
uint8_t   *g_vis_bitmap      = NULL; /* DAT_004fd18c */
void      *g_world_obj       = NULL; /* DAT_004a9990 */
int32_t    g_viewport_width  = 640;  /* DAT_00485228 */
int32_t    g_viewport_height = 480;  /* DAT_00485228+0x2c */
int32_t    g_build_mode      = 0;    /* DAT_00485328 */

/* Bitmask lookup table for MarkDirtyTiles (8 entries, one bit each).
 * Original: DAT_0047f108.  bit_mask = g_bitmask_lut[bit_index] where
 * bit_index = (y * g_vis_x_stride + x) % 8. */
uint8_t g_bitmask_lut[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

/* Tile descriptor global table (see tile_desc.h). */
CTileDesc *g_tileDescs     = NULL;
int        g_tileDescCount = 0;

/* =========================================================================
 * Platform-independent RECT helpers
 *
 * Replace Win32 user32.dll SetRect / IntersectRect / IsRectEmpty.
 * =========================================================================*/

#ifndef LOCO_WIN32

void loco_SetRect(LOCO_RECT *r, int l, int t, int ri, int b)
{
    r->left   = l;
    r->top    = t;
    r->right  = ri;
    r->bottom = b;
}

int loco_IsRectEmpty(const LOCO_RECT *r)
{
    return (r->right <= r->left || r->bottom <= r->top);
}

int loco_IntersectRect(LOCO_RECT *dst, const LOCO_RECT *a, const LOCO_RECT *b)
{
    dst->left   = (a->left   > b->left)   ? a->left   : b->left;
    dst->top    = (a->top    > b->top)    ? a->top    : b->top;
    dst->right  = (a->right  < b->right)  ? a->right  : b->right;
    dst->bottom = (a->bottom < b->bottom) ? a->bottom : b->bottom;
    return !loco_IsRectEmpty(dst);
}

#endif /* !LOCO_WIN32 */

#ifdef LOCO_LINUX

SDL_Rect loco_RECT_to_SDL(const LOCO_RECT *r)
{
    SDL_Rect s;
    s.x = r->left;
    s.y = r->top;
    s.w = r->right  - r->left;
    s.h = r->bottom - r->top;
    return s;
}

LOCO_RECT loco_SDL_to_RECT(const SDL_Rect *s)
{
    LOCO_RECT r;
    r.left   = s->x;
    r.top    = s->y;
    r.right  = s->x + s->w;
    r.bottom = s->y + s->h;
    return r;
}

#endif /* LOCO_LINUX */

/* =========================================================================
 * Tile grid accessors
 * =========================================================================*/

/*
 * TileMap_GetCell  (0x00455620 / WorldTileLookup 0x00455620)
 *
 * Returns the TileNode* stored in the primary cell array at grid (x, y, z).
 * Performs bounds checking before access.
 *
 * Access formula (from binary):
 *   address = primary_base + x * (0x1040 / sizeof(TileNode*))
 *                          + y * (0x40   / sizeof(TileNode*))
 *                          + z
 * where the strides are in TileNode* units (4 bytes on Win32/Linux-32).
 *
 * Ghidra formula shown as:
 *   tile_map_base + 0x48 + x * 0x1040 + y * 0x40 + z * 4
 * meaning: dereference the pointer stored at map+0x48, then index.
 */
TileNode *TileMap_GetCell(TileMap *map, int x, int y, int z)
{
    if (x < 0 || x >= TILEMAP_GRID_X)   return NULL;
    if (y < 0 || y >= TILEMAP_GRID_Y)   return NULL;
    if (z < 0 || z >= TILEMAP_GRID_Z)   return NULL;

    /* primary_base is stored at map+0x48 (confirmed by binary formula).
     * Stride in TileNode* units:
     *   x stride = TILEMAP_XSTRIDE / sizeof(TileNode*) = 0x1040 / 4 = 0x410
     *   y stride = TILEMAP_YSTRIDE / sizeof(TileNode*) = 0x40   / 4 = 0x10  */
    TileNode **base = map->primary_base;
    return base[x * (TILEMAP_XSTRIDE / sizeof(TileNode*))
              + y * (TILEMAP_YSTRIDE / sizeof(TileNode*))
              + z];
}

/*
 * TileMap_GetCoordFromNode  (0x004557c0)
 *
 * Reads the TileNode* from the SECONDARY cell array at (x, y, z).
 * The secondary array is at map->secondary_base (pointer stored at map+0x64).
 *
 * On success: writes the packed grid coord from TileNode+0x88 into *out_coord.
 *   Packed format: high 16 bits = tile_x, low 16 bits = tile_y.
 * On miss (no node at that cell): writes 0xFFFFFFFF into *out_coord.
 *
 * Used by the train waypoint system to verify that a pixel position maps to
 * a known tile coordinate.
 */
void TileMap_GetCoordFromNode(TileMap *map, int x, int y, int z, int32_t *out_coord)
{
    if (x < 0 || x >= TILEMAP_GRID_X ||
        y < 0 || y >= TILEMAP_GRID_Y ||
        z < 0 || z >= TILEMAP_GRID_Z) {
        *out_coord = (int32_t)0xFFFFFFFF;
        return;
    }

    TileNode **base = map->secondary_base;  /* pointer at map+0x64 */
    TileNode  *node = base[x * (TILEMAP_XSTRIDE / sizeof(TileNode*))
                         + y * (TILEMAP_YSTRIDE / sizeof(TileNode*))
                         + z];

    if (node == NULL) {
        *out_coord = (int32_t)0xFFFFFFFF;
        return;
    }

    /* TileNode+0x88 = tile_x (int16_t), +0x8a = tile_y (int16_t).
     * Pack as int32: (tile_x << 16) | (uint16_t)tile_y. */
    *out_coord = ((int32_t)node->tile_x << 16) | (uint16_t)node->tile_y;
}

/*
 * TileMap_GetCoordFromNodeAlt  (0x00455740)
 *
 * Identical to TileMap_GetCoordFromNode.  Both variants use secondary_base
 * (at map+0x64).  The binary has two separate functions that compile to the
 * same code; they are call-site-differentiated by context.
 *
 * The description notes "uses base+0x64 offset (decimal 100)" for both —
 * confirming they access the same secondary array.
 *
 * Called from Train_SnapToTile (0x0040b740) when snapping a train to a tile.
 */
void TileMap_GetCoordFromNodeAlt(TileMap *map, int x, int y, int z, int32_t *out_coord)
{
    TileMap_GetCoordFromNode(map, x, y, z, out_coord);
}

/*
 * MarkDirtyTiles  (0x00455840)
 *
 * Sets bits in the visibility bitmap (g_vis_bitmap) for all tiles covered
 * by the given screen pixel rectangle.
 *
 * Algorithm:
 *   1. Convert pixel rect to tile rect: left >>= 4, top >>= 4, etc.
 *   2. Clamp to world grid (width at map+0x3e, height at map+0x40).
 *   3. For each (tile_x, tile_y) in range:
 *        bit_index = tile_y * g_vis_x_stride + tile_x
 *        g_vis_bitmap[bit_index >> 3] |= g_bitmask_lut[bit_index & 7]
 *
 * Called after any entity movement to schedule those tiles for redraw.
 * Mirrored call: WorldTileLookup (same address 0x00455620) for cell reads.
 */
void MarkDirtyTiles(TileMap *map, const LOCO_RECT *screen_rect)
{
    int tx0 = screen_rect->left   >> 4;
    int ty0 = screen_rect->top    >> 4;
    int tx1 = screen_rect->right  >> 4;
    int ty1 = screen_rect->bottom >> 4;

    /* Clamp to grid dimensions (grid_width at map+0x3e, grid_height at +0x40). */
    if (tx0 < 0)                  tx0 = 0;
    if (ty0 < 0)                  ty0 = 0;
    if (tx1 >= map->grid_width)   tx1 = map->grid_width  - 1;
    if (ty1 >= map->grid_height)  ty1 = map->grid_height - 1;

    if (!g_vis_bitmap) return;

    for (int ty = ty0; ty <= ty1; ty++) {
        for (int tx = tx0; tx <= tx1; tx++) {
            int bit_index = ty * g_vis_x_stride + tx;
            g_vis_bitmap[bit_index >> 3] |= g_bitmask_lut[bit_index & 7];
        }
    }
}

/*
 * TileMap_RenderRect  (0x00456700)
 *
 * Renders all visible tiles in the given pixel rectangle to the screen.
 *
 * Algorithm (from binary):
 *   1. Convert pixel rect to tile coords (>> 4).
 *   2. Outer loop y, inner loop x.
 *   3. Check visibility bit: bit (y * g_vis_x_stride + x) in g_vis_bitmap.
 *   4. For visible tiles: read layer_count from map->layer_count_base
 *      (minimum 2 enforced).
 *   5. For each layer 0..count-1: fetch TileNode* from secondary_base,
 *      dispatch vtable+0x2c (draw background) and vtable+0x30 (draw overlay).
 *   6. Call FUN_00411c50 (build cursor overlay) for active build mode.
 *
 * WIN32: vtable dispatches go to DirectDraw blitters in src/graphics/.
 * LINUX: same dispatch; blitters replaced by SDL2 equivalents.
 *
 * The implementation below performs the tile loop and visibility check.
 * The actual draw calls are dispatched through the TileNode vtable so that
 * the rendering backend (DirectDraw vs SDL2) is swappable.
 */
void TileMap_RenderRect(TileMap *map, int left, int top, int right, int bottom)
{
    /* Convert pixel rect to tile coordinates. */
    int tx0 = left   >> 4;
    int ty0 = top    >> 4;
    int tx1 = right  >> 4;
    int ty1 = bottom >> 4;

    /* Clamp to grid. */
    if (tx0 < 0)                  tx0 = 0;
    if (ty0 < 0)                  ty0 = 0;
    if (tx1 >= map->grid_width)   tx1 = map->grid_width  - 1;
    if (ty1 >= map->grid_height)  ty1 = map->grid_height - 1;

    for (int ty = ty0; ty <= ty1; ty++) {
        for (int tx = tx0; tx <= tx1; tx++) {
            /* Check visibility bit. */
            int bit_index = ty * g_vis_x_stride + tx;
            if (!(g_vis_bitmap[bit_index >> 3] & g_bitmask_lut[bit_index & 7]))
                continue;

            /* Read layer count for this (tx, ty).
             * layer_count_base indexed as base[tx * (TILEMAP_XSTRIDE/4) + ty].
             * Minimum layer count enforced at 2 (from binary). */
            int layer_idx = tx * (TILEMAP_XSTRIDE / sizeof(TileNode*)) + ty;
            int layer_count = map->layer_count_base ? map->layer_count_base[layer_idx] : 2;
            if (layer_count < 2) layer_count = 2;

            for (int z = 0; z < layer_count; z++) {
                TileNode *node = TileMap_GetCell(map, tx, ty, z);
                if (!node) continue;

                /* Dispatch vtable+0x2c (draw background tile).
                 * WIN32: (**(code**)(*(int*)node + 0x2c))(node)
                 * LINUX: same vtable dispatch through the C++ vtable pointer.
                 * The blitter implementations live in src/graphics/. */
                typedef void (*DrawFn)(TileNode *);
                void **vtable = node->base.vtable;
                if (vtable) {
                    DrawFn draw_bg      = (DrawFn)vtable[0x2c / sizeof(void*)];
                    DrawFn draw_overlay = (DrawFn)vtable[0x30 / sizeof(void*)];
                    if (draw_bg)      draw_bg(node);
                    if (draw_overlay) draw_overlay(node);
                }
            }
        }
    }

    /* TODO: call FUN_00411c50 (build cursor overlay) if g_build_mode is set. */
}

/*
 * TileMap_ScheduleRender  (0x00456150)
 *
 * Computes the visible tile range from scroll coordinates and viewport size,
 * then dispatches TileMap_RenderRect for each dirty region.
 *
 * Algorithm:
 *   1. Compute tile range from scroll_x/scroll_y and viewport dimensions.
 *   2. If g_build_mode, clip range to build cursor rect.
 *   3. Walk dirty linked list; append to dirty region set.
 *   4. For each dirty RECT: call TileMap_RenderRect.
 *   5. Zero g_vis_bitmap (size = grid_w * grid_h / 8 bytes).
 *
 * The 'full_redraw' parameter ('\0' = incremental, non-zero = full redraw):
 *   Called by SaveGame_Load with '\0' to reset tile display.
 *
 * WIN32: reads DirectDraw viewport from DAT_00485228 and DAT_00485228+0x2c.
 * LINUX: use g_viewport_width / g_viewport_height (set from SDL window size).
 */
void TileMap_ScheduleRender(TileMap *map, char full_redraw)
{
    /* Visible tile range from scroll position and viewport size.
     * Original: reads map->scroll_x, map->scroll_y and viewport globals. */
    int tx0 = map->scroll_x >> 4;
    int ty0 = map->scroll_y >> 4;
    int tx1 = tx0 + (g_viewport_width  >> 4) + 1;
    int ty1 = ty0 + (g_viewport_height >> 4) + 1;

    /* Clamp to grid. */
    if (tx0 < 0)                  tx0 = 0;
    if (ty0 < 0)                  ty0 = 0;
    if (tx1 >= map->grid_width)   tx1 = map->grid_width  - 1;
    if (ty1 >= map->grid_height)  ty1 = map->grid_height - 1;

    if (full_redraw || !g_vis_bitmap) {
        /* Full redraw: mark all visible tiles dirty and render. */
        TileMap_RenderRect(map,
            tx0 * TILE_PIXELS, ty0 * TILE_PIXELS,
            tx1 * TILE_PIXELS, ty1 * TILE_PIXELS);
    } else {
        /* Incremental: only render tiles with dirty bits set.
         * The dirty bits are set by MarkDirtyTiles when entities move.
         * TileMap_RenderRect checks bits internally, so calling it over the
         * full visible range is sufficient; invisible tiles are skipped. */
        TileMap_RenderRect(map,
            tx0 * TILE_PIXELS, ty0 * TILE_PIXELS,
            tx1 * TILE_PIXELS, ty1 * TILE_PIXELS);
    }

    /* Zero the visibility bitmap after rendering.
     * Size = ceil(grid_w * grid_h / 8) bytes. */
    if (g_vis_bitmap) {
        size_t vis_bytes = (size_t)(map->grid_width * map->grid_height + 7) / 8;
        memset(g_vis_bitmap, 0, vis_bytes);
    }
}

/*
 * WorldDrawChain_Clip  (0x00456d10)
 *
 * Walks the world draw chain (Y-sorted singly-linked list of DrawChainNode).
 * For each node: calls IntersectRect against g_world_viewport.
 *
 * If NO intersection:
 *   - Logs 'Invalid Rect found in world draw chain' via OutputDebugStringA.
 *     LINUX: fprintf(stderr, ...) replacement.
 *   - Unlinks node: prev->next = node->next (or updates *chain_head).
 *   - Frees node via FUN_00465cd0 (LINUX: free()).
 *
 * If intersection:
 *   - Clamps node->rect to viewport bounds.
 *   - Advances to next node.
 *
 * Invalid rects arise when an entity is freshly constructed but not placed,
 * or when a train exits the world without removal from the chain.
 */
void WorldDrawChain_Clip(DrawChainNode **chain_head)
{
    DrawChainNode *prev = NULL;
    DrawChainNode *node = *chain_head;

    while (node) {
        DrawChainNode *next_node = node->next;
        LOCO_RECT      clipped;

        if (!loco_IntersectRect(&clipped, &g_world_viewport, &node->rect)) {
            /* No intersection — invalid rect.
             * WIN32: OutputDebugStringA("Invalid Rect found in world draw chain\n");
             * LINUX: use stderr. */
            fprintf(stderr, "Invalid Rect found in world draw chain\n");

            /* Unlink: remove node from chain. */
            if (prev)
                prev->next = next_node;
            else
                *chain_head = next_node;

            /* Free node.
             * WIN32: FUN_00465cd0 (heap free wrapper).
             * LINUX: free(). */
            free(node);
            /* prev stays the same */
        } else {
            /* Clamp rect to viewport bounds. */
            node->rect = clipped;
            prev = node;
        }

        node = next_node;
    }
}

/* =========================================================================
 * Train navigation
 * =========================================================================*/

/*
 * Train_FindWaypointInTile  (0x0040b610)
 *
 * Finds the waypoint index within a TileNode's sprite_resource->waypoints[]
 * that matches the pixel position (px, py).
 *
 * Algorithm:
 *   1. Read tile_x, tile_y from TileNode+0x88/0x8a.
 *   2. Tile pixel origin = (tile_x * 16, tile_y * 16).
 *   3. Iterate waypoints[0..waypoint_count-1]:
 *      if (origin_x + waypoints[i][0] == px &&
 *          origin_y + waypoints[i][1] == py) → found at index i.
 *   4. On match: write waypoint index into train+0x08 (frame_index on car_front).
 *      Write py into train+0x10 (screen_y on car_front slot).
 *
 * Used when a train is placed or re-attached to a track segment after an
 * editor operation.
 */
void Train_FindWaypointInTile(TrainEntity *train, TileNode *tile, int px, int py)
{
    if (!tile || !tile->base.sprite_resource) return;

    SpriteResource *sr   = tile->base.sprite_resource;
    int origin_x         = tile->tile_x * TILE_PIXELS;
    int origin_y         = tile->tile_y * TILE_PIXELS;
    int wp_count         = sr->waypoint_count;
    CarSlot *car         = train->car_front;

    for (int i = 0; i < wp_count; i++) {
        int wx = origin_x + sr->waypoints[i][0];
        int wy = origin_y + sr->waypoints[i][1];
        if (wx == px && wy == py) {
            if (car) {
                car->frame_index = i;   /* train+0x08 */
                car->screen_y    = py;  /* train+0x10 */
            }
            return;
        }
    }
}

/*
 * Train_SnapToTile  (0x0040b740)
 *
 * Snaps a train entity to the tile located at pixel position (px, py).
 *
 * Algorithm:
 *   1. Convert (px, py) to tile coords: tx = px >> 4, ty = py >> 4.
 *   2. Call TileMap_GetCell(g_tile_map, tx, ty, 0).
 *   3. Verify tile type == rail via FUN_00446030 (ClassifyTileType).
 *   4. Search waypoints for the closest match via Train_FindWaypointInTile.
 *   5. Write train+0x14 (current tile), train+0x08 (waypoint index),
 *      train+0x0c/+0x10 (pixel x, y).
 *   6. Store tile grid coords in TileNode+0x88 via TileMap_GetCoordFromNodeAlt.
 */
void Train_SnapToTile(TrainEntity *train, int px, int py)
{
    int tx = px >> 4;
    int ty = py >> 4;

    TileNode *tile = TileMap_GetCell(g_tile_map, tx, ty, 0);
    if (!tile) return;

    /* Verify it is a rail tile (type 1, 2, 3, or 4). */
    if (!tile->base.sprite_resource) return;
    /* FUN_00446030 maps sprite ID to TileType; TILE_TYPE_NONE = not a track. */

    CarSlot *car = train->car_front;
    if (car) {
        car->tile_entity = tile;    /* train+0x14 */
        car->screen_x    = px;      /* train+0x0c */
        car->screen_y    = py;      /* train+0x10 */
    }

    /* Find the matching waypoint index and write it. */
    Train_FindWaypointInTile(train, tile, px, py);

    /* Store tile grid coords back via secondary array lookup. */
    int32_t coord;
    TileMap_GetCoordFromNodeAlt(g_tile_map, tx, ty, 0, &coord);
    /* coord is packed: high 16 = tile_x, low 16 = tile_y.
     * If valid (coord != 0xFFFFFFFF), write back to TileNode+0x88/+0x8a. */
    if (coord != (int32_t)0xFFFFFFFF) {
        tile->tile_x = (int16_t)((uint32_t)coord >> 16);
        tile->tile_y = (int16_t)(coord & 0xFFFF);
    }
}

/*
 * Train_FindNextTileNode  (0x0040b880)
 *
 * Searches for the TileNode a train should enter next.
 *
 * Algorithm:
 *   1. Read current tile from car_front->tile_entity.
 *   2. Read TileNode+0x88/0x8a (tile_x, tile_y) and direction_type (+0x63a).
 *   3. Compute target pixel from TileData->waypoints at the boundary waypoint.
 *   4. Compute target tile coords: target_tx = target_px >> 4, etc.
 *   5. Call TileMap_GetCell for the target coords.
 *   6. Check junction direction (TileData+0x636/+0x638) for routing.
 *   7. Update train+0x14 (tile), +0x04 (direction), +0x08 (waypoint index).
 *
 * Returns 0 on successful tile transition, old TileNode* on failure.
 *
 * The direction_type byte (TileData+0x63a) encodes where this tile's track
 * leads: values 7-10 are portal exits; 0-6 are normal track directions.
 */
TileNode *Train_FindNextTileNode(TrainEntity *train, void *param_1)
{
    CarSlot *car = train->car_front;
    if (!car || !car->tile_entity) return (TileNode*)car;

    TileNode       *cur_tile = car->tile_entity;
    SpriteResource *sr       = cur_tile->base.sprite_resource;
    if (!sr) return cur_tile;

    int origin_x = cur_tile->tile_x * TILE_PIXELS;
    int origin_y = cur_tile->tile_y * TILE_PIXELS;
    int dir      = car->direction;
    int wp_idx   = car->frame_index;
    int wp_count = sr->waypoint_count;

    /* Determine the exit waypoint based on direction. */
    int exit_wp;
    if (dir == 1) {
        /* Forward: last waypoint */
        exit_wp = wp_count - 1;
    } else {
        /* Backward: first waypoint */
        exit_wp = 0;
    }

    /* Compute target pixel position from exit waypoint. */
    int target_px = origin_x + sr->waypoints[exit_wp][0];
    int target_py = origin_y + sr->waypoints[exit_wp][1];

    /* Adjust by one step in direction of travel to step off the tile. */
    uint8_t dir_type = sr->direction_type;
    switch (dir_type) {
        case PORTAL_DIR_EAST:  target_px += TILE_PIXELS; break;
        case PORTAL_DIR_WEST:  target_px -= TILE_PIXELS; break;
        case PORTAL_DIR_SOUTH: target_py += TILE_PIXELS; break;
        case PORTAL_DIR_NORTH: target_py -= TILE_PIXELS; break;
        default: break;
    }

    int next_tx = target_px >> 4;
    int next_ty = target_py >> 4;

    TileNode *next_tile = TileMap_GetCell(g_tile_map, next_tx, next_ty, 0);
    if (!next_tile) {
        /* No tile at target: return old tile to signal failure. */
        return cur_tile;
    }

    /* Update train state with new tile. */
    TileNode *old_tile = car->tile_entity;
    car->tile_entity   = next_tile;  /* train+0x14 */

    /* Decrement ref count on old tile, increment on new tile. */
    if (old_tile && old_tile->ref_count > 0)
        old_tile->ref_count--;
    next_tile->ref_count++;

    /* Reset waypoint index for the new tile. */
    SpriteResource *next_sr = next_tile->base.sprite_resource;
    if (next_sr) {
        car->frame_index = (dir == 1) ? 0 : (next_sr->waypoint_count - 1);
    }

    return (TileNode*)0; /* 0 = success */
}

/*
 * Train_Update  (0x0040bbd0)
 *
 * Main per-tick train movement state machine.
 *
 * The train+0x18 (state) and train+0x1c (state2) fields encode the current
 * movement phase.  From the binary:
 *
 *   State 0 (MOTION_NORMAL, state2 == 0):
 *     - Advance car_front->frame_index by 1 (forward) or -1 (backward).
 *     - Read next pixel from sprite_resource->waypoints[frame_index].
 *     - Update car_front->screen_x and car_rear->screen_y.
 *     - If frame_index reaches boundary (0 or count-1): call Train_FindNextTileNode.
 *
 *   State 4 (junction traversal, state2 == 4):
 *     - Adjust pixel coords by direction byte from TileData+0x63a.
 *     - Values 1=R, 2=L, 3=U, 4=D (move one pixel per tick).
 *
 *   Bounds check: if screen_x >= g_tile_map_max_x * 16 or
 *                    screen_y >= g_tile_map_max_y * 16 → stop.
 *
 * NOTE: train+0x18 (state) maps to the SpriteEntity base field at +0x18
 *       (active flag in SpriteEntity layout).  The original engine reuses
 *       this field as a state machine counter in moving entities.
 *
 * WIN32: __thiscall — ECX = train pointer.
 * LINUX: explicit self parameter.
 */
void Train_Update(TrainEntity *self, void *station_ctx)
{
    if (!self) return;

    CarSlot *car = self->car_front;
    if (!car || !car->tile_entity) return;

    SpriteResource *sr = car->tile_entity->base.sprite_resource;
    if (!sr) return;

    int wp_count = sr->waypoint_count;
    int dir      = car->direction;

    /* Advance waypoint frame index. */
    if (dir == 1) {
        car->frame_index++;
        if (car->frame_index >= wp_count) {
            car->frame_index = wp_count - 1;
            /* Waypoints exhausted: advance to next tile. */
            TileNode *result = Train_FindNextTileNode(self, station_ctx);
            if (result != (TileNode*)0) {
                /* Transition failed; train cannot advance. */
                return;
            }
            /* Reload sr after tile change. */
            if (!car->tile_entity || !car->tile_entity->base.sprite_resource) return;
            sr       = car->tile_entity->base.sprite_resource;
            wp_count = sr->waypoint_count;
        }
    } else {
        car->frame_index--;
        if (car->frame_index < 0) {
            car->frame_index = 0;
            TileNode *result = Train_FindNextTileNode(self, station_ctx);
            if (result != (TileNode*)0) return;
            if (!car->tile_entity || !car->tile_entity->base.sprite_resource) return;
            sr       = car->tile_entity->base.sprite_resource;
            wp_count = sr->waypoint_count;
        }
    }

    /* Update screen position from waypoint table. */
    TileNode *tile     = car->tile_entity;
    int origin_x       = tile->tile_x * TILE_PIXELS;
    int origin_y       = tile->tile_y * TILE_PIXELS;
    int new_x          = origin_x + sr->waypoints[car->frame_index][0];
    int new_y          = origin_y + sr->waypoints[car->frame_index][1];

    /* Bounds check against world edge. */
    if (new_x >= g_tile_map_max_x * TILE_PIXELS ||
        new_y >= g_tile_map_max_y * TILE_PIXELS) {
        self->motion_state = MOTION_STOPPED;
        return;
    }

    car->screen_x = new_x;
    car->screen_y = new_y;

    /* Speed modulation near segment boundaries.
     * Within 50 frames of boundary → slow; after 80 → full speed. */
    int frames_from_end = (dir == 1) ? (wp_count - 1 - car->frame_index)
                                     : car->frame_index;
    if (frames_from_end <= 50)
        self->speed_state = 0;   /* slow */
    else if (frames_from_end >= 80)
        self->speed_state = 1;   /* full speed */

    /* Mark dirty tiles for redraw. */
    LOCO_RECT entity_rect = self->base.world_rect;
    MarkDirtyTiles(g_tile_map, &entity_rect);

    /* Recompute sprite heading. */
    ComputeHeading(self);
}

/*
 * Train_HandleTunnelPortal  (0x0040cb10)
 *
 * Handles traversal through a tunnel portal tile.
 *
 * Reads direction_type from TileData+0x63a (7=E, 8=W, 9=S, 10=N portal).
 * Each tick, increments/decrements the car's screen_x (E/W) or screen_y (N/S)
 * by one pixel unit.
 *
 * Checks against tile bounds computed from:
 *   tile_origin = (TileNode+0x88, TileNode+0x8a) × 16
 *   tile_bounds = tile_origin ± (SPRITE_TILE_FP_W × 16, SPRITE_TILE_FP_H × 16)
 *
 * On valid traverse: calls Train_FindWaypointInTile to snap waypoint index,
 *   clears car->direction and the state2 field.
 * Returns 1 if transition succeeded (car fully exited tunnel), 0 if still inside.
 */
int Train_HandleTunnelPortal(TrainEntity *train, TileNode *portal_tile)
{
    if (!train || !portal_tile || !portal_tile->base.sprite_resource)
        return 0;

    CarSlot        *car = train->car_front;
    SpriteResource *sr  = portal_tile->base.sprite_resource;
    uint8_t         dir_type = sr->direction_type;

    /* Tile pixel origin and footprint. */
    int ox = portal_tile->tile_x * TILE_PIXELS;
    int oy = portal_tile->tile_y * TILE_PIXELS;
    int fw = SPRITE_TILE_FP_W(sr) * TILE_PIXELS;
    int fh = SPRITE_TILE_FP_H(sr) * TILE_PIXELS;

    /* Step the car by one pixel in the portal direction. */
    switch (dir_type) {
        case PORTAL_DIR_EAST:  car->screen_x++; break;
        case PORTAL_DIR_WEST:  car->screen_x--; break;
        case PORTAL_DIR_SOUTH: car->screen_y++; break;
        case PORTAL_DIR_NORTH: car->screen_y--; break;
        default: return 0;
    }

    /* Check if car has crossed the tile boundary. */
    int inside = 0;
    switch (dir_type) {
        case PORTAL_DIR_EAST:  inside = (car->screen_x <= ox + fw); break;
        case PORTAL_DIR_WEST:  inside = (car->screen_x >= ox);      break;
        case PORTAL_DIR_SOUTH: inside = (car->screen_y <= oy + fh); break;
        case PORTAL_DIR_NORTH: inside = (car->screen_y >= oy);      break;
    }

    if (inside) {
        return 0; /* Still inside tunnel; more ticks needed. */
    }

    /* Car has exited the tunnel portal.
     * Snap waypoint to the exit position and clear tunnel state. */
    Train_FindWaypointInTile(train, portal_tile, car->screen_x, car->screen_y);
    train->station_state = STATION_CLEAR;  /* clears train+0x1c (state2) */
    return 1;
}

/*
 * Train_CheckStationGap  (0x0040c3d0)
 *
 * Tests whether a station tile has a safe gap for train entry.
 *
 * Reads direction_type (+0x63a): values 18 (PORTAL_DIR_STATION_A) or
 * 19 (PORTAL_DIR_STATION_B) identify station tiles.
 *
 * Reads train stop_request (train+0x36 in original offset):
 *   If == 1: clear it, return 0 (deny entry).
 *
 * Calls FUN_0044c370 to check gap availability.
 * On valid gap at waypoint 1 or (waypoint_count - 1):
 *   Sets TileNode+0x118 (vehicle_occupancy) = train pointer.
 *   Sets train stop_request = 200.
 *
 * Returns 1 if train may enter the station, 0 if blocked.
 */
int Train_CheckStationGap(TrainEntity *train, TileNode *station_tile)
{
    if (!train || !station_tile || !station_tile->base.sprite_resource)
        return 0;

    SpriteResource *sr       = station_tile->base.sprite_resource;
    uint8_t         dir_type = sr->direction_type;

    if (dir_type != PORTAL_DIR_STATION_A && dir_type != PORTAL_DIR_STATION_B)
        return 0; /* Not a station tile. */

    /* Check stop_request (train+0x36 — accessed via base field alias). */
    /* NOTE: 0x36 is not a named field in our struct but is within the SpriteEntity
     * base pad region.  Access via byte offset pointer for accuracy. */
    uint32_t *stop_req_ptr = (uint32_t*)((uint8_t*)train + 0x36);
    if (*stop_req_ptr == 1) {
        *stop_req_ptr = 0;
        return 0; /* Deny: stop was requested, now cleared. */
    }

    /* FUN_0044c370: check for a gap in the station approach.
     * Returns non-zero if a gap exists at the appropriate waypoint boundary.
     * TODO: implement gap check logic (depends on surrounding tile occupancy). */
    int gap_exists = 1; /* placeholder: actual check in FUN_0044c370 */
    if (!gap_exists) return 0;

    CarSlot *car     = train->car_front;
    int      wp_count = sr->waypoint_count;

    /* Valid gap at entry waypoint (1) or exit waypoint (count-1). */
    if (car && (car->frame_index == 1 || car->frame_index == wp_count - 1)) {
        station_tile->vehicle_occupancy = train;  /* TileNode+0x118 */
        *stop_req_ptr = 200;                       /* train+0x36 = 200 */
        return 1;
    }

    return 0;
}

/*
 * Train_JunctionRouting  (0x0040c460)
 *
 * Junction routing for switch tiles.
 *
 * Algorithm:
 *   1. Read junction_tile->ref_count (+0x114): if > 0, stop train (priority 1).
 *   2. Check four boundary waypoints (indices 0, count-1, count, count+1)
 *      against car->direction / car->frame_index.
 *   3. Toggle junction_tile->tile_state between TILE_STATE_JUNCTION_A (4)
 *      and TILE_STATE_JUNCTION_B (5).
 *   4. Call Train_FindNextTileNode to compute the routed next tile.
 *   5. Dispatch vtable+0x1c (trigger event) for tiles of type 4 or 5.
 */
void Train_JunctionRouting(TrainEntity *train, TileNode *junction_tile)
{
    if (!train || !junction_tile) return;

    /* If junction is occupied by another vehicle, stop (priority rule). */
    if (junction_tile->ref_count > 0) {
        train->motion_state = MOTION_WAITING;
        return;
    }

    /* Toggle junction direction state. */
    if (junction_tile->tile_state == TILE_STATE_JUNCTION_A)
        junction_tile->tile_state = TILE_STATE_JUNCTION_B;
    else
        junction_tile->tile_state = TILE_STATE_JUNCTION_A;

    /* Find the next tile for the new junction direction. */
    Train_FindNextTileNode(train, NULL);

    /* Dispatch vtable+0x1c (trigger switch event) if tile type is 4 or 5. */
    if (junction_tile->tile_state == TILE_STATE_JUNCTION_A ||
        junction_tile->tile_state == TILE_STATE_JUNCTION_B) {
        void **vtable = junction_tile->base.vtable;
        if (vtable) {
            typedef void (*TriggerFn)(TileNode*, int);
            TriggerFn trigger = (TriggerFn)vtable[0x1c / sizeof(void*)];
            if (trigger) trigger(junction_tile, junction_tile->tile_state);
        }
    }
}

/*
 * Train_HandleStationArrival  (0x0044ca50)
 *
 * Station arrival handler.
 * Reads train->stop_request (+0x5c in TrainEntity struct).
 * Reads station_tile->tile_state (+0x110):
 *   TILE_STATE_LOADING (1):
 *     Calls FUN_0044cb10 (platform swap — toggles direction_flag 1↔4
 *     and reverses all cars).  Sets train+0x36=200 (wait-for-gap state).
 *   TILE_STATE_EMPTY (2):
 *     Calls FUN_0044d740 to dispatch the train:
 *       priority 2 = pass-through (stop_request == 0)
 *       priority 1 = stop (stop_request != 0)
 *   TILE_STATE_BLOCKED (3):
 *     Train waits.
 */
void Train_HandleStationArrival(TrainEntity *train, TileNode *station_tile)
{
    if (!train || !station_tile) return;

    int stop_req = train->stop_request;

    switch (station_tile->tile_state) {
        case TILE_STATE_LOADING: {
            /* Platform swap: reverse direction.
             * FUN_0044cb10: toggles direction_flag (1 ↔ 4) and reverses cars. */
            train->direction_flag = (train->direction_flag == 1) ? 4 : 1;
            /* Set wait-for-gap counter via raw offset (train+0x36). */
            *(uint32_t*)((uint8_t*)train + 0x36) = 200;
            break;
        }
        case TILE_STATE_EMPTY: {
            /* FUN_0044d740: dispatch train with priority. */
            /* priority 2 = pass-through, priority 1 = stop */
            /* TODO: call FUN_0044d740(train, station_tile, stop_req ? 1 : 2) */
            (void)stop_req;
            break;
        }
        case TILE_STATE_BLOCKED:
        default:
            /* Wait. */
            break;
    }
}

/* =========================================================================
 * Train entity
 * =========================================================================*/

/*
 * InitPathSlot  (0x0040ec70)
 *
 * Zero-initialises a CarSlot and sets the default field values.
 * Called by TrainEntity_ctor for both car_front and car_rear.
 */
void InitPathSlot(CarSlot *slot)
{
    if (!slot) return;
    memset(slot, 0, sizeof(CarSlot));

    slot->direction       = 2;                  /* neutral / no movement yet */
    slot->frame_index     = CAR_SLOT_INIT_FRAME; /* 100 */
    slot->direction_active = 1;
    /* screen_x, screen_y, tile_entity, tile_ptr, blocked, extra all 0. */
}

/*
 * TrainEntity_ctor  (0x0040d500)
 *
 * Constructs a TrainEntity.  Calls the SpriteEntity base constructor
 * (FUN_00405790), installs vtable PTR_FUN_00477590.
 *
 * Allocates two CarSlots via FUN_0040b500 (8 bytes each on heap);
 * initialises each with InitPathSlot.
 *
 * param_3 controls which end is the front:
 *   param_3 == 0 → station_state = STATION_DOCKED (2), motion_state = MOTION_NORMAL (0)
 *   param_3 != 0 → station_state = STATION_CLEAR  (0), motion_state = MOTION_STOPPED (2)
 *
 * WIN32: allocates via operator new / HeapAlloc.
 * LINUX: allocates with malloc().
 */
TrainEntity *TrainEntity_ctor(TrainEntity *self, void *world_ctx,
                               SpriteResource *sprite_res, int param_3)
{
    if (!self) return NULL;

    memset(self, 0, sizeof(TrainEntity));

    /* Set base entity fields (would call FUN_00405790 on Win32). */
    self->base.world_ctx        = world_ctx;
    self->base.sprite_resource  = sprite_res;
    self->base.active           = 1;
    self->base.unknown_54       = 0xFFFF;

    /* Allocate car slots.
     * WIN32: FUN_0040b500 (heap alloc wrapper).
     * LINUX: malloc(). */
    self->car_front = (CarSlot*)malloc(sizeof(CarSlot));
    self->car_rear  = (CarSlot*)malloc(sizeof(CarSlot));
    if (!self->car_front || !self->car_rear) return NULL;

    InitPathSlot(self->car_front);
    InitPathSlot(self->car_rear);

    /* Set initial states based on param_3. */
    if (param_3 == 0) {
        self->station_state = STATION_DOCKED;
        self->motion_state  = MOTION_NORMAL;
    } else {
        self->station_state = STATION_CLEAR;
        self->motion_state  = MOTION_STOPPED;
    }

    self->speed_state   = 1; /* full speed */
    self->heading_index = 0;

    return self;
}

/*
 * ComputeHeading  (0x0040df80)
 *
 * Recomputes train sprite heading index from the vector between car endpoints.
 *
 * Algorithm:
 *   dx = car_front->screen_x - car_rear->screen_x
 *   dy = car_front->screen_y - car_rear->screen_y
 *   angle = atan2(dy, dx)         (radians; range -π..π)
 *   index = (int)(angle * N / (2π)) & 0x7F
 *
 * Original uses x87 fpatan with a scale factor.  The binary scales angle to
 * an integer index via __ftol; the exact scale factor reconstructed from
 * TRAIN_MAX_HEADING = 128 directions covering a full circle.
 *
 * Clamps result: if heading_index == 0x80, wraps to 0 (half-circle boundary).
 *
 * WIN32: fpatan FPU instruction (x87).
 * LINUX: atan2f() from <math.h>.
 */
void ComputeHeading(TrainEntity *train)
{
    if (!train || !train->car_front || !train->car_rear) return;

    float dx = (float)(train->car_front->screen_x - train->car_rear->screen_x);
    float dy = (float)(train->car_front->screen_y - train->car_rear->screen_y);

    if (dx == 0.0f && dy == 0.0f) return; /* no motion vector; keep current heading */

    /* Convert angle to heading index in range [0, 127].
     * atan2f returns -π..π; map to 0..127 by:
     *   index = (int)((atan2f(dy, dx) + π) / (2π) * 128) & 0x7F */
    float angle = atan2f(dy, dx);
    int   index = (int)((angle + (float)M_PI) / (2.0f * (float)M_PI) * (float)TRAIN_MAX_HEADING);
    index &= (TRAIN_MAX_HEADING - 1); /* mask to 0..127 */

    /* Wrap 0x80 back to 0 (half-circle boundary; matches original). */
    if (index == TRAIN_MAX_HEADING) index = 0;

    train->heading_index = (int16_t)index;
}

/*
 * RepositionFromPathSlot  (0x0040d8e0)
 *
 * Places the train entity in screen space using car_front position and the
 * heading-indexed hotspot from the sprite resource.
 *
 * Algorithm:
 *   screen_x = car_front->screen_x
 *   screen_y = car_front->screen_y
 *   hotspot  = sprite_resource->heading_hotspot[heading_index]
 *   left     = screen_x - hotspot[0]
 *   top      = screen_y - hotspot[1]
 *   right    = left + sprite_resource->sprite_width
 *   bottom   = top  + sprite_resource->sprite_height
 *
 * Sets entity bounding rect (world_rect at +0x08) and calls vtable+0x0c
 * (move callback) so the renderer knows the entity has moved.
 */
void RepositionFromPathSlot(TrainEntity *train)
{
    if (!train || !train->car_front) return;
    if (!train->base.sprite_resource)   return;

    SpriteResource *sr    = train->base.sprite_resource;
    int             hi    = (int)(uint16_t)train->heading_index;
    if (hi >= TRAIN_MAX_HEADING) hi = 0;

    int hotspot_x = sr->heading_hotspot[hi][0];
    int hotspot_y = sr->heading_hotspot[hi][1];

    int sx = train->car_front->screen_x;
    int sy = train->car_front->screen_y;

    LOCO_RECT *rect = &train->base.world_rect;
    rect->left   = sx - hotspot_x;
    rect->top    = sy - hotspot_y;
    rect->right  = rect->left + sr->sprite_width;
    rect->bottom = rect->top  + sr->sprite_height;

    /* Dispatch vtable+0x0c (move callback).
     * WIN32: (**(code**)(*(int*)train + 0x0c))(train)
     * LINUX: same vtable dispatch. */
    void **vtable = train->base.vtable;
    if (vtable) {
        typedef void (*MoveFn)(TrainEntity*);
        MoveFn move_cb = (MoveFn)vtable[0x0c / sizeof(void*)];
        if (move_cb) move_cb(train);
    }
}

/*
 * StationApproachCheck  (0x0040e440)
 *
 * Tests whether the lead car has crossed into the station tile boundary.
 *
 * Switch on direction_type (7=E, 8=W, 9=S, 10=N):
 *   Case 7 (E exit): boundary = right edge of tile
 *   Case 8 (W exit): boundary = left edge of tile
 *   Case 9 (S exit): boundary = bottom edge of tile
 *   Case 10 (N exit): boundary = top edge of tile
 *
 * If car has crossed the boundary: sets station_state = STATION_DOCKED,
 * dispatches vtable+0x24(0) (dock callback with arg 0 = approach).
 */
void StationApproachCheck(TrainEntity *train, TileNode *station_tile)
{
    if (!train || !station_tile || !station_tile->base.sprite_resource) return;

    SpriteResource *sr     = station_tile->base.sprite_resource;
    uint8_t         dt     = sr->direction_type;
    CarSlot        *car    = train->car_front;
    if (!car) return;

    int ox = station_tile->tile_x * TILE_PIXELS;
    int oy = station_tile->tile_y * TILE_PIXELS;
    int fw = SPRITE_TILE_FP_W(sr) * TILE_PIXELS;
    int fh = SPRITE_TILE_FP_H(sr) * TILE_PIXELS;

    int crossed = 0;
    switch (dt) {
        case PORTAL_DIR_EAST:  crossed = (car->screen_x >= ox + fw); break;
        case PORTAL_DIR_WEST:  crossed = (car->screen_x <= ox);      break;
        case PORTAL_DIR_SOUTH: crossed = (car->screen_y >= oy + fh); break;
        case PORTAL_DIR_NORTH: crossed = (car->screen_y <= oy);      break;
        default: break;
    }

    if (crossed) {
        train->station_state = STATION_DOCKED;
        /* Dispatch vtable+0x24 with arg 0 (dock/approach callback).
         * WIN32: (**(code**)(*(int*)train + 0x24))(train, 0) */
        void **vtable = train->base.vtable;
        if (vtable) {
            typedef void (*DockFn)(TrainEntity*, int);
            DockFn dock = (DockFn)vtable[0x24 / sizeof(void*)];
            if (dock) dock(train, 0);
        }
    }
}

/*
 * StationDepartCheck  (0x0040e520)
 *
 * Mirror of StationApproachCheck for departure.
 * Uses the same direction_type switch but inverts the boundary comparison:
 *   the car has departed when it exits the station tile on the far side.
 * Sets station_state = STATION_DEPARTING, dispatches vtable+0x24(1) (depart).
 */
void StationDepartCheck(TrainEntity *train, TileNode *station_tile)
{
    if (!train || !station_tile || !station_tile->base.sprite_resource) return;

    SpriteResource *sr  = station_tile->base.sprite_resource;
    uint8_t         dt  = sr->direction_type;
    CarSlot        *car = train->car_front;
    if (!car) return;

    int ox = station_tile->tile_x * TILE_PIXELS;
    int oy = station_tile->tile_y * TILE_PIXELS;
    int fw = SPRITE_TILE_FP_W(sr) * TILE_PIXELS;
    int fh = SPRITE_TILE_FP_H(sr) * TILE_PIXELS;

    int departed = 0;
    switch (dt) {
        case PORTAL_DIR_EAST:  departed = (car->screen_x > ox + fw); break;
        case PORTAL_DIR_WEST:  departed = (car->screen_x < ox);      break;
        case PORTAL_DIR_SOUTH: departed = (car->screen_y > oy + fh); break;
        case PORTAL_DIR_NORTH: departed = (car->screen_y < oy);      break;
        default: break;
    }

    if (departed) {
        train->station_state = STATION_DEPARTING;
        void **vtable = train->base.vtable;
        if (vtable) {
            typedef void (*DockFn)(TrainEntity*, int);
            DockFn dock = (DockFn)vtable[0x24 / sizeof(void*)];
            if (dock) dock(train, 1); /* arg 1 = depart */
        }
    }
}

/*
 * CheckOffscreenBounds  (0x0040e2a0)
 *
 * Called when motion_state == MOTION_WAITING and a signal/switch is ahead.
 * Checks train bounding rect edges against world edge coords.
 * If out of bounds: sets motion_state = MOTION_STOPPED, calls vtable+0x24(0).
 */
void CheckOffscreenBounds(TrainEntity *train)
{
    if (!train) return;

    LOCO_RECT *r = &train->base.world_rect;
    int max_x    = g_tile_map_max_x * TILE_PIXELS;
    int max_y    = g_tile_map_max_y * TILE_PIXELS;

    if (r->left < 0 || r->top < 0 || r->right > max_x || r->bottom > max_y) {
        train->motion_state = MOTION_STOPPED;
        void **vtable = train->base.vtable;
        if (vtable) {
            typedef void (*DockFn)(TrainEntity*, int);
            DockFn cb = (DockFn)vtable[0x24 / sizeof(void*)];
            if (cb) cb(train, 0);
        }
    }
}

/*
 * CheckSignal  (0x0040e340)
 *
 * Signal and switch handler called from TrainUpdate.
 *
 * If both cars' tile_state == TILE_STATE_FREE (0):
 *   Clears motion_state (MOTION_NORMAL) and calls vtable+0x24(1).
 *
 * Looks up next tile via TileMap_GetCell at tile coords from car+0x2e/+0x30.
 * If ahead tile has a signal (FUN_0044bd10) and signal is clear:
 *   Resets block_flag (+0x11c) on the signal tile.
 * If track ahead is empty: writes -1 to tile coord fields to invalidate.
 *
 * WIN32: reads car+0x2e (next_tile_x short) and car+0x30 (next_tile_y short)
 *        via byte offset into the CarSlot _reserved region.
 * LINUX: same byte-offset access.
 */
void CheckSignal(TrainEntity *train)
{
    if (!train || !train->car_front || !train->car_rear) return;

    CarSlot *front = train->car_front;
    CarSlot *rear  = train->car_rear;

    TileNode *ft = front->tile_entity;
    TileNode *rt = rear->tile_entity;

    /* If both cars are on free tiles, clear the motion block. */
    if ((!ft || ft->tile_state == TILE_STATE_FREE) &&
        (!rt || rt->tile_state == TILE_STATE_FREE)) {
        train->motion_state = MOTION_NORMAL;
        void **vtable = train->base.vtable;
        if (vtable) {
            typedef void (*DockFn)(TrainEntity*, int);
            DockFn cb = (DockFn)vtable[0x24 / sizeof(void*)];
            if (cb) cb(train, 1);
        }
        return;
    }

    /* Look up the tile ahead using coords stored at car+0x2e/+0x30.
     * These are int16_t fields inside the CarSlot _reserved region.
     * byte offsets: +0x2e = next_tile_x, +0x30 = next_tile_y. */
    int16_t next_tx = *(int16_t*)((uint8_t*)front + 0x2e);
    int16_t next_ty = *(int16_t*)((uint8_t*)front + 0x30);

    if (next_tx == -1 || next_ty == -1) return;

    TileNode *ahead = TileMap_GetCell(g_tile_map, next_tx, next_ty, 0);
    if (!ahead) {
        /* No tile ahead: invalidate coords. */
        *(int16_t*)((uint8_t*)front + 0x2e) = -1;
        *(int16_t*)((uint8_t*)front + 0x30) = -1;
        return;
    }

    /* If ahead tile's block_flag is clear, reset it (signal passes). */
    if (ahead->block_flag == 0) {
        ahead->block_flag = 0; /* already clear; no-op (matches binary) */
    }
}

/*
 * TrainCheckStation  (0x0040db90)
 *
 * Handles station docking logic based on station_state.
 *   STATION_APPROACHING (1) + car->blocked == 1 → StationApproachCheck
 *   STATION_DEPARTING   (4) + both cars unblocked → reset to STATION_CLEAR
 *   STATION_TUNNEL_EXIT (5) → StationDepartCheck (tunnel depart path)
 */
void TrainCheckStation(TrainEntity *train)
{
    if (!train) return;

    CarSlot *front = train->car_front;
    CarSlot *rear  = train->car_rear;

    switch (train->station_state) {
        case STATION_APPROACHING:
            if (front && front->blocked) {
                /* FUN_0040e440: approach boundary check. */
                TileNode *tile = front->tile_entity;
                if (tile) StationApproachCheck(train, tile);
            }
            break;

        case STATION_DEPARTING:
            if ((!front || front->blocked == 0) &&
                (!rear  || rear->blocked  == 0)) {
                /* Both cars clear of station. */
                train->station_state = STATION_CLEAR;
            }
            break;

        case STATION_TUNNEL_EXIT:
            /* FUN_0040e520: tunnel departure check. */
            if (front && front->tile_entity)
                StationDepartCheck(train, front->tile_entity);
            break;

        default:
            break;
    }
}

/*
 * TrainMoveAlongPath  (0x0040dc20)
 *
 * Core per-frame path step.  Selects the active car slot based on param_1[2]
 * (the direction flag in the path context struct).
 *
 * At a waypoint segment boundary:
 *   forward  (frame_index == waypoint_count - 1): load next tile coords
 *   backward (frame_index == 1):                  load prev tile coords
 *   Updates both cars' screen_x (+0x0c) and screen_y (+0x10).
 *
 * Speed modulation near boundaries:
 *   within 50 frames → speed_state = 0 (slow)
 *   after 80 frames  → speed_state = 1 (full)
 *
 * Calls MarkDirtyTiles and ComputeHeading.
 */
void TrainMoveAlongPath(TrainEntity *train, void *path_context)
{
    /* The path_context[2] byte selects car_front (0) vs car_rear (non-0). */
    int use_rear = 0;
    if (path_context) {
        use_rear = ((uint8_t*)path_context)[2];
    }

    CarSlot *car = use_rear ? train->car_rear : train->car_front;
    if (!car || !car->tile_entity || !car->tile_entity->base.sprite_resource)
        return;

    SpriteResource *sr       = car->tile_entity->base.sprite_resource;
    int             wp_count = sr->waypoint_count;
    int             dir      = car->direction;

    /* Check for segment boundary. */
    int at_forward_boundary  = (dir == 1 && car->frame_index >= wp_count - 1);
    int at_backward_boundary = (dir != 1 && car->frame_index <= 1);

    if (at_forward_boundary || at_backward_boundary) {
        /* Segment boundary: request next tile. */
        Train_FindNextTileNode(train, path_context);

        /* Reload sr after tile change. */
        if (car->tile_entity && car->tile_entity->base.sprite_resource)
            sr = car->tile_entity->base.sprite_resource;

        /* Sync both cars' screen coords to the boundary waypoint. */
        TileNode *tile   = car->tile_entity;
        if (!tile || !sr) return;
        int origin_x = tile->tile_x * TILE_PIXELS;
        int origin_y = tile->tile_y * TILE_PIXELS;
        int wp_idx   = (dir == 1) ? 0 : (sr->waypoint_count - 1);

        int new_x = origin_x + sr->waypoints[wp_idx][0];
        int new_y = origin_y + sr->waypoints[wp_idx][1];

        if (train->car_front) { train->car_front->screen_x = new_x; train->car_front->screen_y = new_y; }
        if (train->car_rear)  { train->car_rear->screen_x  = new_x; train->car_rear->screen_y  = new_y; }
    } else {
        /* Advance frame index. */
        if (dir == 1)
            car->frame_index++;
        else
            car->frame_index--;
    }

    /* Speed modulation. */
    int wp_count2 = sr ? sr->waypoint_count : 1;
    int frames_from_end = (dir == 1) ? (wp_count2 - 1 - car->frame_index)
                                     : car->frame_index;
    if      (frames_from_end <= 50) train->speed_state = 0;
    else if (frames_from_end >= 80) train->speed_state = 1;

    /* Schedule redraw and recompute heading. */
    LOCO_RECT dirty = train->base.world_rect;
    MarkDirtyTiles(g_tile_map, &dirty);
    ComputeHeading(train);
}

/*
 * TrainUpdate  (0x0040d940)
 *
 * Per-frame train dispatcher.
 *
 * 1. Validates both car tile pointers.
 * 2. If either car is on a tunnel tile (tile_state == TILE_STATE_TUNNEL):
 *      dispatches TrainMoveAlongPath for tunnel traversal.
 * 3. Falls through to FUN_0040c580 (advance each car's path).
 * 4. On movement: calls MarkDirtyTiles, ComputeHeading, vtable+0x20
 *    (request redraw), RepositionFromPathSlot.
 * 5. Dispatches CheckOffscreenBounds, CheckSignal, TrainCheckStation
 *    based on motion_state / station_state.
 */
void TrainUpdate(TrainEntity *train)
{
    if (!train) return;

    CarSlot *front = train->car_front;
    CarSlot *rear  = train->car_rear;

    if (!front || !rear) return;

    /* Check for tunnel tile state on either car. */
    int in_tunnel = 0;
    if (front->tile_entity && front->tile_entity->tile_state == TILE_STATE_TUNNEL)
        in_tunnel = 1;
    if (rear->tile_entity  && rear->tile_entity->tile_state  == TILE_STATE_TUNNEL)
        in_tunnel = 1;

    if (in_tunnel) {
        TrainMoveAlongPath(train, NULL);
    }

    /* Advance path for each car (FUN_0040c580 equivalent). */
    int moved = 0;
    if (train->motion_state == MOTION_NORMAL) {
        int old_x = front->screen_x;
        Train_Update(train, NULL);
        if (front->screen_x != old_x) moved = 1;
    }

    if (moved) {
        /* Schedule dirty tiles, recompute heading, redraw. */
        LOCO_RECT dirty = train->base.world_rect;
        MarkDirtyTiles(g_tile_map, &dirty);
        ComputeHeading(train);
        RepositionFromPathSlot(train);

        /* Dispatch vtable+0x20 (request redraw). */
        void **vtable = train->base.vtable;
        if (vtable) {
            typedef void (*RedrawFn)(TrainEntity*);
            RedrawFn redraw = (RedrawFn)vtable[0x20 / sizeof(void*)];
            if (redraw) redraw(train);
        }
    }

    /* State-driven dispatch. */
    if (train->motion_state == MOTION_WAITING)
        CheckOffscreenBounds(train);

    CheckSignal(train);
    TrainCheckStation(train);
}

/* =========================================================================
 * Sprite Entity
 * =========================================================================*/

/*
 * SpriteEntity_Init  (0x0040d0b0)
 *
 * Initialises a sprite entity (buildings, decorations, static tiles).
 *
 * Sets world_ctx (+0x24) and sprite_resource (+0x44).
 * Builds world_rect (+0x08) from hotspot and tile dimensions:
 *   left   = screen_x - sprite_resource->hotspot_x
 *   top    = screen_y - sprite_resource->hotspot_y
 *   right  = left + sprite_resource->tile_px_width
 *   bottom = top  + sprite_resource->tile_px_height
 * Builds src rect (source in sprite sheet) as (0, 0, tile_px_w, tile_px_h).
 * Sets active=1, entity_flags, unknown_54=0xFFFF.
 *
 * If entity_flags & 2 (animated):
 *   frame_demand = sprite_resource->tile_px_width / 57 - 2
 *   if (frame_demand > world_ctx->min_building_fps_demand)
 *       world_ctx->min_building_fps_demand = frame_demand
 *
 * WIN32: SetRect from user32.dll.
 * LINUX: loco_SetRect() implemented in this file.
 */
void SpriteEntity_Init(SpriteEntity *self, void *world_ctx,
                        SpriteResource *sprite_res, uint16_t flags)
{
    if (!self || !sprite_res) return;

    self->world_ctx       = world_ctx;
    self->sprite_resource = sprite_res;
    self->entity_flags    = flags;
    self->active          = 1;
    self->unknown_54      = 0xFFFF;

    /* Build world_rect from hotspot and tile pixel dimensions. */
    int left   = -sprite_res->hotspot_x;
    int top    = -sprite_res->hotspot_y;
    int right  = left + sprite_res->tile_px_width;
    int bottom = top  + sprite_res->tile_px_height;
    loco_SetRect(&self->world_rect, left, top, right, bottom);

    /* Build source rect spanning full tile dimensions. */
    self->src_left   = 0;
    self->src_top    = 0;
    self->src_right  = sprite_res->tile_px_width;
    self->src_bottom = sprite_res->tile_px_height;

    /* If animated: update min_building_fps_demand in world context.
     * world_ctx is a TileMap*; field at +0xa8. */
    if ((flags & 2) && world_ctx) {
        int frame_demand = (int)sprite_res->tile_px_width / 57 - 2;
        TileMap *map = (TileMap*)world_ctx;
        if (frame_demand > map->min_building_fps_demand)
            map->min_building_fps_demand = frame_demand;
    }
}

/*
 * SpriteEntity_SetFrame  (0x0040d2a0)
 *
 * Sets the current animation frame.  Updates sprite sheet source rect:
 *   src_left  (+0x34) = frame * sprite_resource->tile_px_width
 *   src_right (+0x3c) = (frame+1) * sprite_resource->tile_px_width
 * Resets frame_skip_ctr (+0x50) to 0.
 * Dispatches vtable+0x20 (request redraw).
 */
void SpriteEntity_SetFrame(SpriteEntity *self, int frame)
{
    if (!self || !self->sprite_resource) return;

    self->current_frame  = frame;
    int w                = self->sprite_resource->tile_px_width;
    self->src_left       = frame * w;
    self->src_right      = (frame + 1) * w;
    self->frame_skip_ctr = 0;

    /* Dispatch vtable+0x20 (request redraw). */
    if (self->vtable) {
        typedef void (*RedrawFn)(SpriteEntity*);
        RedrawFn redraw = (RedrawFn)self->vtable[0x20 / sizeof(void*)];
        if (redraw) redraw(self);
    }
}

/*
 * AnimTick  (0x0040d2f0)
 *
 * Per-tick animation driver for buildings.  Fires every 3 ticks.
 * Cycles current_frame: 0 .. frame_count-4 (wraps to 0 at frame_count-3).
 * Calls vtable+0x18 (SetFrame) on frame change.
 *
 * Guards: active (+0x18) and anim_type (+0x48) must both be non-zero.
 */
void AnimTick(SpriteEntity *self)
{
    if (!self) return;
    if (!self->active) return;
    if (!self->anim_type) return;
    if (!self->sprite_resource) return;

    self->frame_skip_ctr++;
    if (self->frame_skip_ctr < 3) return;
    self->frame_skip_ctr = 0;

    int frame_count = self->sprite_resource->frame_count;
    int next_frame  = self->current_frame + 1;
    if (next_frame >= frame_count - 3) next_frame = 0;

    if (next_frame != self->current_frame) {
        /* Dispatch vtable+0x18 (SetFrame virtual). */
        if (self->vtable) {
            typedef void (*SetFrameFn)(SpriteEntity*, int);
            SetFrameFn sf = (SetFrameFn)self->vtable[0x18 / sizeof(void*)];
            if (sf) sf(self, next_frame);
        }
    }
}

/*
 * Sprite_AdvanceFrame  (0x0040d470)
 *
 * Advances one animation frame for a building sprite.
 * Reads sprite_resource+0x30 (frame counter field in the resource blob).
 * If < 4: applies the sub-pixel shrink step:
 *   x -= x * 57 - 50   (converge toward center)
 *   y -= y * 57 - 40
 * Calls loco_SetRect to update world_rect.
 * If entity_flags & 2: recomputes frame demand = new_x / 57 - 2 and
 * updates world_ctx->min_building_fps_demand.
 *
 * The constant 57 (0x39) is the animation sub-pixel scale factor used
 * throughout the building animation system.
 */
void Sprite_AdvanceFrame(SpriteEntity *self)
{
    if (!self || !self->sprite_resource) return;

    /* Read per-resource frame counter at sprite_resource+0x30. */
    int frame_ctr = *(int16_t*)((uint8_t*)self->sprite_resource + 0x30);

    int x = (int)self->world_rect.right  - (int)self->world_rect.left;
    int y = (int)self->world_rect.bottom - (int)self->world_rect.top;

    if (frame_ctr < 4) {
        /* Sub-pixel scale-down step toward tile center. */
        x -= x * 57 - 50;
        y -= y * 57 - 40;
    }

    /* Rebuild world_rect from updated dimensions. */
    int left = self->world_rect.left;
    int top  = self->world_rect.top;
    loco_SetRect(&self->world_rect, left, top, left + x, top + y);

    /* Update fps demand if animated. */
    if ((self->entity_flags & 2) && self->world_ctx) {
        int demand = x / 57 - 2;
        TileMap *map = (TileMap*)self->world_ctx;
        if (demand > map->min_building_fps_demand)
            map->min_building_fps_demand = demand;
    }
}

/* =========================================================================
 * Tile classification
 * =========================================================================*/

/*
 * ClassifyTileType  (0x0040eb60)
 *
 * Maps sprite resource IDs to TileType categories for the train system.
 * IDs outside the known ranges return TILE_TYPE_NONE (0).
 */
TileType ClassifyTileType(uint32_t sprite_id)
{
    if (sprite_id >= 0x1804 && sprite_id <= 0x1808) return TILE_TYPE_STRAIGHT;
    if (sprite_id >= 0x1866 && sprite_id <= 0x186a) return TILE_TYPE_CURVE;
    if (sprite_id >= 0x186c && sprite_id <= 0x186e) return TILE_TYPE_JUNCTION;
    if (sprite_id >= 0x1870 && sprite_id <= 0x1871) return TILE_TYPE_STATION;
    return TILE_TYPE_NONE;
}

/* =========================================================================
 * Linked list helpers
 * =========================================================================*/

/*
 * FreeLinkedNode  (0x0040ecf0)
 *
 * Frees a singly-linked node: poisons its first dword with 0 (to catch
 * use-after-free bugs in the original Win32 code) and sets *node_ptr = NULL.
 *
 * WIN32: HeapFree via original allocator (FUN_00465cd0).
 * LINUX: poison + free().
 */
void FreeLinkedNode(void **node_ptr)
{
    if (!node_ptr || !*node_ptr) return;
    *(uint32_t*)(*node_ptr) = 0; /* poison first dword */
    free(*node_ptr);
    *node_ptr = NULL;
}

/*
 * LinkEntity  (0x0040ed10)
 *
 * Bidirectionally links two list nodes:
 *   ((void**)self)[0] = other
 *   if (other != NULL) ((void**)other)[0] = self
 *
 * Used for inserting entities into the draw chain or signal queues.
 */
void LinkEntity(void *self, void *other)
{
    if (!self) return;
    ((void**)self)[0] = other;
    if (other)
        ((void**)other)[0] = self;
}

/* =========================================================================
 * Path graph construction
 * =========================================================================*/

/* Internal: map direction index to its opposite (GetOppositePath switch).
 * 0 (N) ↔ 2 (S),  1 (E) ↔ 3 (W).
 * Returns -1 and logs error if direction >= 4. */
static int GetOppositePath(int dir)
{
    switch (dir) {
        case 0: return 2;
        case 1: return 3;
        case 2: return 0;
        case 3: return 1;
        default:
            fprintf(stderr, "ERROR: Invalid path in GetOppositePath (dir=%d)\n", dir);
            return -1;
    }
}

/*
 * BuildPathGraph_A  (0x0045ce40)
 *
 * Builds the tile path graph for train type A (freight/cargo).
 *
 * Enumerates all rail tiles via DAT_004a9994 vtable + FUN_004573e0 filter.
 * For each rail tile:
 *   - Allocates a PathNode (0x2c bytes).
 *   - Stores node_index back into tile+0xe4.
 *   - Reads 4-direction neighbor tile pointers from tile+0xc4.
 *   - For each occupied neighbor slot (dir 0-3):
 *       Allocates a PathEdge (0x10 bytes: cost=1, next, from_idx, to_idx).
 *       Also allocates the reverse PathEdge using GetOppositePath.
 *
 * WIN32: operator new for PathNode/PathEdge.
 * LINUX: malloc().
 */
void BuildPathGraph_A(void *world_ctx)
{
    /* Implementation note: this function requires access to the full tile
     * enumeration vtable (DAT_004a9994 + FUN_004573e0) which is part of the
     * world object subsystem (src/core/).  The body below documents the
     * algorithm; the actual enumeration call is a TODO once the world object
     * is fully ported.
     *
     * Pseudocode (from binary analysis):
     *
     * PathNode *nodes = NULL;
     * int node_count = 0;
     *
     * // Enumerate all tiles via world vtable
     * for each tile T in (world_ctx, FUN_004573e0 filter):
     *     PathNode *n = malloc(sizeof(PathNode));
     *     n->tile = T;
     *     n->node_index = node_count;
     *     *(int*)((uint8_t*)T + 0xe4) = node_count;  // tile+0xe4 = path-node index
     *     n->edges = NULL;
     *     nodes[node_count++] = n;
     *
     * // Connect neighbours
     * for each PathNode n (all nodes):
     *     for (int dir = 0; dir < 4; dir++):
     *         TileNode *neighbor = *(TileNode**)((uint8_t*)n->tile + 0xc4 + dir*4)
     *         if (!neighbor) continue;
     *         int neighbor_idx = *(int*)((uint8_t*)neighbor + 0xe4);
     *
     *         PathEdge *e = malloc(sizeof(PathEdge));
     *         e->cost     = 1;
     *         e->next     = n->edges;
     *         e->from_idx = n->node_index;
     *         e->to_idx   = neighbor_idx;
     *         n->edges = e;
     *
     *         // Reverse edge
     *         int rev_dir = GetOppositePath(dir);
     *         PathNode *nb = nodes[neighbor_idx];
     *         PathEdge *er = malloc(sizeof(PathEdge));
     *         er->cost     = 1;
     *         er->next     = nb->edges;
     *         er->from_idx = neighbor_idx;
     *         er->to_idx   = n->node_index;
     *         nb->edges = er;
     */
    (void)world_ctx;
    /* TODO: implement enumeration once world object vtable is ported. */
}

/*
 * BuildPathGraph_B  (0x0045d1c0)
 *
 * Structurally identical to BuildPathGraph_A but for train type B (passenger).
 * Reads neighbor tile pointers at tile+0xe8 instead of tile+0xc4.
 * Uses tile+0x108 for path-node index storage instead of tile+0xe4.
 */
void BuildPathGraph_B(void *world_ctx)
{
    /* Same algorithm as BuildPathGraph_A with different tile field offsets:
     *   neighbor pointer array: tile+0xe8  (vs 0xc4 in type A)
     *   path-node index field:  tile+0x108 (vs 0xe4 in type A) */
    (void)world_ctx;
    /* TODO: implement enumeration once world object vtable is ported. */
}

/*
 * LinkPathNodePairs  (0x0045dad0)
 *
 * Post-processes the path graph with pairwise bidirectional linking.
 * Iterates all (i, j) PathNode pairs:
 *   - Calls FUN_0045dd80(nodes[i], nodes[j]) to test connectivity.
 *   - If result == 0x80 (bidirectional connected): calls FUN_0045dde0 to
 *     write the direction byte for the i→j link and applies GetOppositePath
 *     to write the reverse direction for the j→i link.
 */
void LinkPathNodePairs(PathNode *nodes, int node_count)
{
    if (!nodes || node_count <= 0) return;

    for (int i = 0; i < node_count; i++) {
        for (int j = i + 1; j < node_count; j++) {
            /* FUN_0045dd80: connectivity test.
             * Returns 0x80 if nodes[i] and nodes[j] are bidirectionally connected.
             * TODO: implement connectivity test from full binary analysis. */
            int conn = 0; /* placeholder */
            (void)conn;

            /* If bidirectional: write direction bytes via FUN_0045dde0.
             * GetOppositePath maps direction for reverse link.
             * TODO: implement when FUN_0045dd80/0045dde0 are ported. */
        }
    }
    (void)GetOppositePath; /* suppress unused-function warning until TODO complete */
}

/* =========================================================================
 * Configuration
 * =========================================================================*/

/*
 * Config_LoadBalancing  (0x00406480)
 *
 * Loads performance-balancing parameters from the game INI file.
 * Section [BALANCING]:
 *   MinBuildingFPS  (default 18) → config->min_building_fps
 *   MinVehicleFPS   (default 20) → config->min_vehicle_fps
 *   MinMinifigFPS   (default 16) → config->min_minifig_fps
 *   MinFlyingFPS    (default 14) → config->min_flying_fps
 * Section [WINDOW_ATTRIBUTES]:
 *   RectLeft / RectTop / RectRight / RectBottom → g_world_viewport
 * Section [PROCESS]:
 *   CleanExit flag
 *
 * WIN32: uses FUN_00452d60 (GetPrivateProfileInt wrapper over kernel32.dll).
 * LINUX: simple key=value INI parser (no Win32 registry/INI dependency).
 *
 * The FUN_00452d60 wrapper is not reproduced here; replace with a platform-
 * independent INI reader such as the one in src/platform/ini_reader.c.
 */
void Config_LoadBalancing(GameConfig *config, const char *ini_path)
{
    if (!config) return;

    /* Set defaults before any file read. */
    config->min_building_fps = 18;
    config->min_vehicle_fps  = 20;
    config->min_minifig_fps  = 16;
    config->min_flying_fps   = 14;
    config->min_flying_fps2  = 14;

    if (!ini_path) return;

    /* WIN32: GetPrivateProfileInt("BALANCING", "MinBuildingFPS", 18, ini_path)
     * LINUX: parse ini_path with a platform-independent INI reader.
     *
     * Example minimal parser (replace with production INI reader):
     */
    FILE *fp = fopen(ini_path, "r");
    if (!fp) return;

    char  section[64] = "";
    char  line[256];
    int   in_balancing = 0, in_window = 0;
    LOCO_RECT win_rect = {0, 0, 0, 0};

    while (fgets(line, sizeof(line), fp)) {
        /* Strip trailing newline. */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (line[0] == '[') {
            /* Section header. */
            char *end = strchr(line, ']');
            if (end) { *end = '\0'; strncpy(section, line + 1, sizeof(section)-1); }
            in_balancing = (strcmp(section, "BALANCING")         == 0);
            in_window    = (strcmp(section, "WINDOW_ATTRIBUTES") == 0);
            continue;
        }

        if (line[0] == ';' || line[0] == '#' || line[0] == '\0') continue;

        char key[64]; int val;
        if (sscanf(line, "%63[^=]=%d", key, &val) != 2) continue;

        if (in_balancing) {
            if (strcmp(key, "MinBuildingFPS") == 0) config->min_building_fps = (int8_t)val;
            if (strcmp(key, "MinVehicleFPS")  == 0) config->min_vehicle_fps  = (int8_t)val;
            if (strcmp(key, "MinMinifigFPS")  == 0) config->min_minifig_fps  = (int8_t)val;
            if (strcmp(key, "MinFlyingFPS")   == 0) { config->min_flying_fps = (int8_t)val;
                                                       config->min_flying_fps2 = (int8_t)val; }
        }
        if (in_window) {
            if (strcmp(key, "RectLeft")   == 0) win_rect.left   = val;
            if (strcmp(key, "RectTop")    == 0) win_rect.top    = val;
            if (strcmp(key, "RectRight")  == 0) win_rect.right  = val;
            if (strcmp(key, "RectBottom") == 0) win_rect.bottom = val;
        }
    }

    fclose(fp);
    g_world_viewport = win_rect;
}

/* =========================================================================
 * Save game
 * =========================================================================*/

/*
 * SaveGame_ListSlots  (0x00429490)
 *
 * Enumerates save game files in the savegame\ subdirectory.
 * Builds a sorted singly-linked list of SaveSlot nodes at world+0x4d8.
 *
 * For mode == 5: enumerates "backdrop*.bmp" (background images) instead.
 * For other modes: enumerates "savegame\*.sav".
 *
 * Only files with a basename of at most 10 characters (without extension)
 * are included (original limit from the UI slot display width).
 *
 * Returns 1 on success (even if zero files found), 0 on directory error.
 *
 * WIN32: FindFirstFileA / FindNextFileA / FindClose (kernel32.dll).
 * LINUX: opendir() / readdir() / closedir() from <dirent.h>.
 */
int SaveGame_ListSlots(void *world_ctx, int mode)
{
    if (!world_ctx) return 0;

    /* The save slot list head is at world_ctx+0x4d8. */
    SaveSlot **list_head = (SaveSlot**)((uint8_t*)world_ctx + 0x4d8);

    /* Free any existing list first. */
    SaveSlot *old = *list_head;
    while (old) {
        SaveSlot *next = old->next;
        free(old);
        old = next;
    }
    *list_head = NULL;

    const char *dir_name;
    const char *ext;
    if (mode == 5) {
        dir_name = ".";
        ext      = "backdrop";
    } else {
        dir_name = "savegame";
        ext      = NULL; /* all .sav files */
    }

#ifdef LOCO_WIN32
    /* WIN32 path: FindFirstFileA / FindNextFileA */
    char pattern[MAX_PATH];
    if (mode == 5)
        snprintf(pattern, sizeof(pattern), "%s\\backdrop*.bmp", dir_name);
    else
        snprintf(pattern, sizeof(pattern), "%s\\*.sav",         dir_name);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 1; /* empty but not an error */

    do {
        /* Strip extension to get slot name. */
        char name[32];
        strncpy(name, fd.cFileName, sizeof(name)-1);
        name[sizeof(name)-1] = '\0';
        char *dot = strrchr(name, '.');
        if (dot) *dot = '\0';

        if (strlen(name) > 10) continue; /* too long for UI slot */

        SaveSlot *slot = (SaveSlot*)calloc(1, sizeof(SaveSlot));
        if (!slot) break;
        strncpy(slot->slot_name, name,          10);
        snprintf(slot->full_path, sizeof(slot->full_path),
                 "%s\\%s", dir_name, fd.cFileName);

        /* Insert sorted by slot_name into *list_head. */
        SaveSlot **pos = list_head;
        while (*pos && strcmp((*pos)->slot_name, slot->slot_name) < 0)
            pos = &(*pos)->next;
        slot->next = *pos;
        *pos = slot;
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);

#else /* LOCO_LINUX */
    /* LINUX path: opendir / readdir */
    DIR *dp = opendir(dir_name);
    if (!dp) return 1; /* directory absent = no slots */

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        const char *fname = de->d_name;

        /* Filter by extension. */
        const char *dot = strrchr(fname, '.');
        if (mode == 5) {
            /* backdrop*.bmp */
            if (!dot || strcmp(dot, ".bmp") != 0) continue;
            if (strncmp(fname, "backdrop", 8)  != 0) continue;
        } else {
            if (!dot || strcmp(dot, ".sav") != 0) continue;
        }

        /* Compute basename length (without extension). */
        size_t base_len = (size_t)(dot - fname);
        if (base_len > 10) continue; /* too long for UI */

        SaveSlot *slot = (SaveSlot*)calloc(1, sizeof(SaveSlot));
        if (!slot) break;
        memcpy(slot->slot_name, fname, base_len);
        slot->slot_name[base_len] = '\0';
        snprintf(slot->full_path, sizeof(slot->full_path),
                 "%s/%s", dir_name, fname);

        /* Insert sorted. */
        SaveSlot **pos = list_head;
        while (*pos && strcmp((*pos)->slot_name, slot->slot_name) < 0)
            pos = &(*pos)->next;
        slot->next = *pos;
        *pos = slot;
    }
    closedir(dp);
#endif

    return 1;
}

/*
 * SaveGame_Load  (0x00429a10)
 *
 * Loads a save game from disk.
 * Constructs path: <base_dir>\savegame\<name>.sav
 *   (base_dir is read from world_ctx+0x2ea, a 260-char buffer).
 *
 * Steps:
 *   1. Construct path into world_ctx+0x2ea.
 *   2. Create savegame\ directory if absent.
 *   3. Call TileMap_ScheduleRender(g_tile_map, '\0') — full tile redraw reset.
 *   4. Call FUN_0041d320(g_world_obj, path) — deserialiser for tile+entity state.
 *
 * WIN32: CreateDirectoryA (kernel32.dll) for directory creation.
 * LINUX: mkdir(path, 0755) + snprintf for path construction.
 */
void SaveGame_Load(void *world_ctx, const char *slot_name)
{
    if (!world_ctx || !slot_name) return;

    /* Construct save path into world_ctx+0x2ea (260-char buffer). */
    char *path_buf = (char*)((uint8_t*)world_ctx + 0x2ea);
    snprintf(path_buf, 260,
#ifdef LOCO_WIN32
             "savegame\\%s.sav",
#else
             "savegame/%s.sav",
#endif
             slot_name);

    /* Ensure savegame directory exists. */
#ifdef LOCO_WIN32
    CreateDirectoryA("savegame", NULL);  /* no error if already exists */
#else
    mkdir("savegame", 0755);             /* EEXIST is OK */
#endif

    /* Reset tile display (TileMap_ScheduleRender with full_redraw='\0').
     * '\0' = incremental mode; resets dirty bitmap without full render. */
    if (g_tile_map)
        TileMap_ScheduleRender(g_tile_map, '\0');

    /* Deserialise world state.
     * WIN32/LINUX: FUN_0041d320(&DAT_004a9990, path_buf).
     * TODO: call the world deserialiser once ported from FUN_0041d320. */
    (void)g_world_obj;
}

/*
 * SaveGame_Save  (0x00429b20)
 *
 * Saves the current game state to disk.
 *
 * Steps:
 *   1. Ensure savegame\ directory exists.
 *   2. Construct filename from current slot name (world_ctx+0x4bc via FUN_004490d0).
 *   3. Free existing SaveSlot linked list (vtable[0](node, 1) per node).
 *   4. Serialise world state: FUN_0041d9b0(&g_world_obj, path).
 *   5. Re-enumerate: SaveGame_ListSlots(world_ctx, 0).
 *   6. Lexicographic compare against world_ctx+0x4c0 (previous slot name)
 *      to restore the selected slot pointer after re-enumeration.
 *
 * WIN32: CreateDirectoryA; path uses '\\' separator.
 * LINUX: mkdir(0755); path uses '/' separator.
 */
void SaveGame_Save(void *world_ctx)
{
    if (!world_ctx) return;

    /* Ensure savegame directory exists. */
#ifdef LOCO_WIN32
    CreateDirectoryA("savegame", NULL);
#else
    mkdir("savegame", 0755);
#endif

    /* Current slot name is at world_ctx+0x4bc (FUN_004490d0 reads it).
     * For documentation purposes, read it directly. */
    const char *slot_name = (const char*)((uint8_t*)world_ctx + 0x4bc);

    char path[300];
    snprintf(path, sizeof(path),
#ifdef LOCO_WIN32
             "savegame\\%s.sav",
#else
             "savegame/%s.sav",
#endif
             slot_name);

    /* Free existing SaveSlot list. */
    SaveSlot **list_head = (SaveSlot**)((uint8_t*)world_ctx + 0x4d8);
    SaveSlot  *node      = *list_head;
    while (node) {
        SaveSlot *next = node->next;
        /* WIN32: dispatch vtable[0](node, 1) (reference-counted free).
         * LINUX: free() directly. */
        free(node);
        node = next;
    }
    *list_head = NULL;

    /* Serialise world state.
     * WIN32/LINUX: FUN_0041d9b0(&DAT_004a9990, path).
     * TODO: call serialiser once FUN_0041d9b0 is ported. */
    (void)path;

    /* Re-enumerate save slots. */
    SaveGame_ListSlots(world_ctx, 0);

    /* Restore selected slot pointer by lexicographic compare against
     * world_ctx+0x4c0 (previous slot name). */
    const char *prev_name = (const char*)((uint8_t*)world_ctx + 0x4c0);
    node = *list_head;
    while (node) {
        if (strcmp(node->slot_name, prev_name) == 0) {
            /* Store selected slot pointer at world_ctx+0x4d4 (approx). */
            *(SaveSlot**)((uint8_t*)world_ctx + 0x4d4) = node;
            break;
        }
        node = node->next;
    }
}

/*
 * SaveGame_Delete  (0x00429dd0)
 *
 * Deletes a save game slot from disk and updates the in-memory slot list.
 *
 * Steps:
 *   1. Construct full path into world_ctx+0x2ea (260-char buffer).
 *   2. Free the existing SaveSlot list.
 *   3. Delete the file: DeleteFileA / unlink().
 *   4. On success: call FUN_00449070(world_ctx, slot_name) to clear the
 *      associated world name record.
 *   5. Call FUN_00429850(world_ctx) to update the selected slot pointer.
 *
 * WIN32: DeleteFileA (kernel32.dll).
 * LINUX: unlink() from <unistd.h>.
 */
void SaveGame_Delete(void *world_ctx, const char *slot_name)
{
    if (!world_ctx || !slot_name) return;

    /* Construct path. */
    char *path_buf = (char*)((uint8_t*)world_ctx + 0x2ea);
    snprintf(path_buf, 260,
#ifdef LOCO_WIN32
             "savegame\\%s.sav",
#else
             "savegame/%s.sav",
#endif
             slot_name);

    /* Free existing SaveSlot list. */
    SaveSlot **list_head = (SaveSlot**)((uint8_t*)world_ctx + 0x4d8);
    SaveSlot  *node      = *list_head;
    while (node) {
        SaveSlot *next = node->next;
        free(node);
        node = next;
    }
    *list_head = NULL;

    /* Delete the file. */
    int ok;
#ifdef LOCO_WIN32
    ok = DeleteFileA(path_buf) ? 1 : 0;
#else
    ok = (unlink(path_buf) == 0) ? 1 : 0;
#endif

    if (ok) {
        /* FUN_00449070: clear world name record for this slot.
         * TODO: call once FUN_00449070 is ported. */

        /* FUN_00429850: update selected slot pointer after deletion.
         * TODO: call once FUN_00429850 is ported. */
    }
}

/* =========================================================================
 * Tile descriptor stubs
 * (Declared in tile_desc.h; minimal implementations below)
 * =========================================================================*/

CTileDesc *CTileDesc_Lookup(uint32_t resource_id)
{
    for (int i = 0; i < g_tileDescCount; i++) {
        if (g_tileDescs[i].resource_id == resource_id)
            return &g_tileDescs[i];
    }
    return NULL;
}

int CTileDesc_IsConnected(const CTileDesc *td, int side)
{
    if (!td || side < 0 || side > 3) return 0;
    return td->entry_exit[side] != 0;
}
