/*
 * sdl3_tilemap.h — Tile map data and rendering for SDL3
 *
 * Replaces: TileMap struct at 0x4AAD08 (0x52514 bytes)
 *           TownTileRenderer at 0x42B9C0-0x42Cxxx
 *
 * The original game uses a fixed-size tile grid:
 *   - Each tile is 16x16 pixels
 *   - Up to 4 layers per tile (terrain, track, scenery, building)
 *   - 65 tiles wide (viewport-relative addressing)
 *
 * For the SDL3 port, we simplify: a dynamic 2D array of tile indices
 * rendered as colored quads for now, with real tile graphics loaded
 * from resources later.
 */

#ifndef _WIN32
#ifndef LOCO_SDL3_TILEMAP_H
#define LOCO_SDL3_TILEMAP_H

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Tile types (simplified from original game's resource IDs)
 * ========================================================================= */

typedef enum {
    TILE_EMPTY    = 0,   /* No tile / void                   */
    TILE_GRASS    = 1,   /* Green grass                      */
    TILE_WATER    = 2,   /* Blue water                       */
    TILE_ROAD     = 3,   /* Gray road                        */
    TILE_TRACK    = 4,   /* Brown railway track              */
    TILE_SAND     = 5,   /* Yellow sand / beach              */
    TILE_ROCK     = 6,   /* Dark gray rock / mountain        */
    TILE_BUILDING = 7,   /* Building footprint               */
    TILE_COUNT    = 8,
} TileType;

/* =========================================================================
 * TileMap
 * ========================================================================= */

typedef struct TileMap {
    int      map_width;       /* World width in tiles              */
    int      map_height;      /* World height in tiles             */
    int      tile_size;       /* Tile size in pixels (16)          */
    int      pixel_width;     /* map_width * tile_size             */
    int      pixel_height;    /* map_height * tile_size            */

    /* Viewport / camera */
    int      view_x;          /* Scroll offset X (pixels)          */
    int      view_y;          /* Scroll offset Y (pixels)          */
    int      view_w;          /* Visible width (pixels)            */
    int      view_h;          /* Visible height (pixels)           */

    /* Tile data — flat array, row-major */
    uint8_t *tiles;           /* map_width * map_height entries    */

    /* Rendering cache */
    SDL_Texture *texture;     /* Pre-rendered world texture        */
    bool         dirty;       /* Needs re-render                   */
} TileMap;

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

/**
 * TileMap_Create — Allocate a tile map.
 *
 * @param map_w    World width in tiles.
 * @param map_h    World height in tiles.
 * @param view_w   Viewport width in pixels (e.g., 640).
 * @param view_h   Viewport height in pixels (e.g., 480).
 * @return         New TileMap (caller owns).
 */
TileMap *TileMap_Create(int map_w, int map_h, int view_w, int view_h);

/**
 * TileMap_Destroy — Free tile map resources.
 */
void TileMap_Destroy(TileMap *tm);

/**
 * TileMap_GetTile — Get tile type at (tx, ty).
 */
TileType TileMap_GetTile(const TileMap *tm, int tx, int ty);

/**
 * TileMap_SetTile — Set tile type at (tx, ty).
 */
void TileMap_SetTile(TileMap *tm, int tx, int ty, TileType type);

/**
 * TileMap_Fill — Fill entire map with a tile type.
 */
void TileMap_Fill(TileMap *tm, TileType type);

/**
 * TileMap_GenerateTestWorld — Create a test pattern world.
 *
 * Generates grass base with water border, roads, and scattered buildings.
 */
void TileMap_GenerateTestWorld(TileMap *tm);

/* =========================================================================
 * Viewport
 * ========================================================================= */

/**
 * TileMap_ScrollTo — Set viewport scroll position.
 */
void TileMap_ScrollTo(TileMap *tm, int x, int y);

/**
 * TileMap_ScrollBy — Scroll viewport by delta.
 */
void TileMap_ScrollBy(TileMap *tm, int dx, int dy);

/**
 * TileMap_ClampView — Clamp viewport to world bounds.
 */
void TileMap_ClampView(TileMap *tm);

/* =========================================================================
 * Rendering
 * ========================================================================= */

/**
 * TileMap_Render — Render the tile map to an SDL texture.
 *
 * Converts the tile grid into colored rectangles on a GPU texture.
 * Only re-renders if the map is marked dirty.
 *
 * @param tm        Tile map to render.
 * @param renderer  SDL3 renderer.
 */
void TileMap_Render(TileMap *tm, SDL_Renderer *renderer);

/**
 * TileMap_Draw — Draw the visible portion of the tile map.
 *
 * Renders the viewport region of the world to the current render target.
 * Handles scrolling — only visible tiles are drawn.
 *
 * @param tm        Tile map.
 * @param renderer  SDL3 renderer.
 */
void TileMap_Draw(const TileMap *tm, SDL_Renderer *renderer);

#ifdef __cplusplus
}
#endif

#endif /* LOCO_SDL3_TILEMAP_H */
#endif /* _WIN32 */
