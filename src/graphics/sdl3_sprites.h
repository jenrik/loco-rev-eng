/*
 * sdl3_sprites.h — Sprite/texture cache for game graphics
 *
 * Loads BMP/PNG images and manages GPU textures for tiles,
 * buildings, vehicles, and UI elements.
 */

#ifndef LOCO_SDL3_SPRITES_H
#define LOCO_SDL3_SPRITES_H

#include <SDL3/SDL.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of cached textures. */
#define SPRITE_CACHE_MAX  256

/** One cached sprite texture. */
typedef struct {
    char         name[128];    /* Asset filename / key              */
    SDL_Texture *texture;      /* GPU texture (ARGB8888)            */
    int          width;        /* Texture width in pixels           */
    int          height;       /* Texture height in pixels          */
} SpriteEntry;

/** Sprite cache manager. */
typedef struct {
    SpriteEntry entries[SPRITE_CACHE_MAX];
    int         count;
} SpriteCache;

/**
 * SpriteCache_Init — Initialize the sprite cache.
 */
void SpriteCache_Init(SpriteCache *cache);

/**
 * SpriteCache_Destroy — Free all cached textures.
 */
void SpriteCache_Destroy(SpriteCache *cache);

/**
 * SpriteCache_Load — Load a BMP/PNG from disk into the cache.
 *
 * @param cache     Sprite cache.
 * @param renderer  SDL3 renderer.
 * @param path      Path to the image file.
 * @return          Index in cache, or -1 on failure.
 */
int SpriteCache_Load(SpriteCache *cache, SDL_Renderer *renderer,
                     const char *path);

/**
 * SpriteCache_LoadFromMemory — Load from memory buffer (e.g., from resources).
 *
 * @param cache     Sprite cache.
 * @param renderer  SDL3 renderer.
 * @param name      Key/name for the sprite.
 * @param data      Raw image data (BMP/PNG bytes).
 * @param size      Size of data in bytes.
 * @return          Index in cache, or -1 on failure.
 */
int SpriteCache_LoadFromMemory(SpriteCache *cache, SDL_Renderer *renderer,
                               const char *name, const void *data, size_t size);

/**
 * SpriteCache_Find — Find a cached sprite by name.
 *
 * @return  Index in cache, or -1 if not found.
 */
int SpriteCache_Find(const SpriteCache *cache, const char *name);

/**
 * SpriteCache_Get — Get a sprite entry by index.
 */
const SpriteEntry *SpriteCache_Get(const SpriteCache *cache, int index);

/**
 * Sprite_Draw — Draw a sprite at the given position.
 *
 * @param renderer  SDL3 renderer.
 * @param sprite    Sprite to draw.
 * @param x         Destination X.
 * @param y         Destination Y.
 * @param scale     Scale factor (1.0 = native size).
 */
void Sprite_Draw(SDL_Renderer *renderer, const SpriteEntry *sprite,
                 float x, float y, float scale);

/**
 * Sprite_DrawRect — Draw a portion of a sprite (tile from a tileset).
 *
 * @param renderer  SDL3 renderer.
 * @param sprite    Source sprite (tileset).
 * @param src_x     Source X in sprite.
 * @param src_y     Source Y in sprite.
 * @param src_w     Source width.
 * @param src_h     Source height.
 * @param dst_x     Destination X.
 * @param dst_y     Destination Y.
 * @param scale     Scale factor.
 */
void Sprite_DrawRect(SDL_Renderer *renderer, const SpriteEntry *sprite,
                     int src_x, int src_y, int src_w, int src_h,
                     float dst_x, float dst_y, float scale);

#ifdef __cplusplus
}
#endif

#endif /* LOCO_SDL3_SPRITES_H */
