/*
 * sdl3_tilemap.c — Tile map implementation
 *
 * Grounded in: original TileMap at 0x4AAD08, TownTileRenderer at 0x42B9C0
 */

#include "sdl3_tilemap.h"
#include <stdlib.h>
#include <string.h>

/* Tile colors in ARGB8888 */
static const uint32_t tile_colors[TILE_COUNT] = {
    0x00000000,  /* EMPTY    — transparent */
    0xFF3C8C3C,  /* GRASS    — green       */
    0xFF2848C8,  /* WATER    — blue        */
    0xFF888888,  /* ROAD     — gray        */
    0xFF8B6914,  /* TRACK    — brown       */
    0xFFD4C878,  /* SAND     — sand        */
    0xFF606060,  /* ROCK     — dark gray   */
    0xFFC83030,  /* BUILDING — red         */
};

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

TileMap *TileMap_Create(int map_w, int map_h, int view_w, int view_h)
{
    TileMap *tm = (TileMap*)calloc(1, sizeof(TileMap));
    if (!tm) return NULL;

    tm->map_width    = map_w;
    tm->map_height   = map_h;
    tm->tile_size    = 16;
    tm->pixel_width  = map_w * tm->tile_size;
    tm->pixel_height = map_h * tm->tile_size;
    tm->view_x       = 0;
    tm->view_y       = 0;
    tm->view_w       = view_w;
    tm->view_h       = view_h;
    tm->dirty        = true;

    tm->tiles = (uint8_t*)calloc((size_t)map_w * map_h, 1);
    if (!tm->tiles) {
        free(tm);
        return NULL;
    }

    return tm;
}

void TileMap_Destroy(TileMap *tm)
{
    if (tm) {
        if (tm->texture) SDL_DestroyTexture(tm->texture);
        free(tm->tiles);
        free(tm);
    }
}

TileType TileMap_GetTile(const TileMap *tm, int tx, int ty)
{
    if (!tm || tx < 0 || tx >= tm->map_width || ty < 0 || ty >= tm->map_height)
        return TILE_EMPTY;
    return (TileType)tm->tiles[ty * tm->map_width + tx];
}

void TileMap_SetTile(TileMap *tm, int tx, int ty, TileType type)
{
    if (!tm || tx < 0 || tx >= tm->map_width || ty < 0 || ty >= tm->map_height)
        return;
    tm->tiles[ty * tm->map_width + tx] = (uint8_t)type;
    tm->dirty = true;
}

void TileMap_Fill(TileMap *tm, TileType type)
{
    if (!tm) return;
    memset(tm->tiles, (uint8_t)type, (size_t)tm->map_width * tm->map_height);
    tm->dirty = true;
}

/* =========================================================================
 * Test world generation — creates an interesting looking map
 * ========================================================================= */

void TileMap_GenerateTestWorld(TileMap *tm)
{
    if (!tm) return;

    /* Fill with grass */
    TileMap_Fill(tm, TILE_GRASS);

    int mw = tm->map_width;
    int mh = tm->map_height;

    /* Water border (2 tiles wide) */
    for (int y = 0; y < mh; y++) {
        for (int x = 0; x < mw; x++) {
            if (x < 2 || x >= mw - 2 || y < 2 || y >= mh - 2) {
                TileMap_SetTile(tm, x, y, TILE_WATER);
            }
        }
    }

    /* Main road — horizontal */
    int road_y = mh / 2;
    for (int x = 4; x < mw - 4; x++) {
        TileMap_SetTile(tm, x, road_y, TILE_ROAD);
    }

    /* Vertical road */
    int road_x = mw / 2;
    for (int y = 4; y < mh - 4; y++) {
        TileMap_SetTile(tm, road_x, y, TILE_ROAD);
    }

    /* Crossroads */
    TileMap_SetTile(tm, road_x, road_y, TILE_SAND);

    /* Railway track — parallel to horizontal road */
    for (int x = 4; x < mw - 4; x++) {
        TileMap_SetTile(tm, x, road_y - 4, TILE_TRACK);
    }

    /* Scattered buildings */
    for (int i = 0; i < 15; i++) {
        int bx = 4 + (rand() % (mw - 8));
        int by = 4 + (rand() % (mh - 8));
        /* Don't overwrite roads */
        TileType existing = TileMap_GetTile(tm, bx, by);
        if (existing == TILE_GRASS) {
            TileMap_SetTile(tm, bx, by, TILE_BUILDING);
        }
    }

    /* Beach/sand area */
    for (int y = 2; y < 5; y++) {
        for (int x = 8; x < 20; x++) {
            if (TileMap_GetTile(tm, x, y) == TILE_GRASS) {
                TileMap_SetTile(tm, x, y, TILE_SAND);
            }
        }
    }
    for (int x = 8; x < 20; x++) {
        for (int y = mh - 5; y < mh - 2; y++) {
            if (TileMap_GetTile(tm, x, y) == TILE_GRASS) {
                TileMap_SetTile(tm, x, y, TILE_SAND);
            }
        }
    }

    /* Rock formations */
    for (int i = 0; i < 10; i++) {
        int rx = 4 + (rand() % (mw - 8));
        int ry = 4 + (rand() % (mh - 8));
        if (TileMap_GetTile(tm, rx, ry) == TILE_GRASS) {
            TileMap_SetTile(tm, rx, ry, TILE_ROCK);
        }
    }
}

/* =========================================================================
 * Viewport
 * ========================================================================= */

void TileMap_ScrollTo(TileMap *tm, int x, int y)
{
    if (!tm) return;
    tm->view_x = x;
    tm->view_y = y;
    TileMap_ClampView(tm);
}

void TileMap_ScrollBy(TileMap *tm, int dx, int dy)
{
    if (!tm) return;
    tm->view_x += dx;
    tm->view_y += dy;
    TileMap_ClampView(tm);
}

void TileMap_ClampView(TileMap *tm)
{
    if (!tm) return;
    int max_x = tm->pixel_width  - tm->view_w;
    int max_y = tm->pixel_height - tm->view_h;
    if (tm->view_x < 0) tm->view_x = 0;
    if (tm->view_y < 0) tm->view_y = 0;
    if (tm->view_x > max_x) tm->view_x = max_x;
    if (tm->view_y > max_y) tm->view_y = max_y;
}

/* =========================================================================
 * Rendering
 * ========================================================================= */

void TileMap_Render(TileMap *tm, SDL_Renderer *renderer)
{
    if (!tm || !renderer) return;
    if (!tm->dirty && tm->texture) return;

    /* Create or recreate the world texture */
    if (tm->texture) SDL_DestroyTexture(tm->texture);

    tm->texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_TARGET,
        tm->pixel_width, tm->pixel_height);
    if (!tm->texture) return;

    SDL_Texture *prev = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, tm->texture);

    /* Clear to dark green (grass default) */
    SDL_SetRenderDrawColor(renderer, 24, 80, 24, 255);
    SDL_RenderClear(renderer);

    int ts = tm->tile_size;

    /* Draw each tile */
    for (int ty = 0; ty < tm->map_height; ty++) {
        for (int tx = 0; tx < tm->map_width; tx++) {
            TileType type = TileMap_GetTile(tm, tx, ty);
            if (type == TILE_EMPTY || type == TILE_GRASS) continue;

            /* Slightly randomize color for visual variety */
            int seed = (tx * 7 + ty * 13) % 16;
            int r = (int)((tile_colors[type] >> 16) & 0xFF);
            int g = (int)((tile_colors[type] >> 8)  & 0xFF);
            int b = (int)(tile_colors[type] & 0xFF);
            r = r + seed - 8; if (r < 0) r = 0; if (r > 255) r = 255;
            g = g + seed - 8; if (g < 0) g = 0; if (g > 255) g = 255;
            b = b + seed - 8; if (b < 0) b = 0; if (b > 255) b = 255;

            SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, 255);

            SDL_FRect rect = {
                (float)(tx * ts), (float)(ty * ts),
                (float)ts, (float)ts
            };
            SDL_RenderFillRect(renderer, &rect);

            /* Outer border for non-grass tiles */
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 40);
            SDL_RenderRect(renderer, &rect);

            /* Inner highlight */
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 15);
            SDL_FRect inner = {
                (float)(tx * ts + 1), (float)(ty * ts + 1),
                (float)(ts - 2), (float)(ts - 2)
            };
            SDL_RenderRect(renderer, &inner);
        }
    }

    SDL_SetRenderTarget(renderer, prev);
    tm->dirty = false;
}

void TileMap_Draw(const TileMap *tm, SDL_Renderer *renderer)
{
    if (!tm || !renderer) return;

    /* Ensure the world texture is up to date */
    TileMap_Render((TileMap*)tm, renderer);

    if (!tm->texture) return;

    /* Draw the visible portion */
    SDL_FRect src = {
        (float)tm->view_x, (float)tm->view_y,
        (float)tm->view_w, (float)tm->view_h
    };
    SDL_FRect dst = { 0, 0, (float)tm->view_w, (float)tm->view_h };

    SDL_RenderTexture(renderer, tm->texture, &src, &dst);
}
