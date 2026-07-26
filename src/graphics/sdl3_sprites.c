/*
 * sdl3_sprites.c — Sprite/texture cache implementation
 */

#include "sdl3_sprites.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void SpriteCache_Init(SpriteCache *cache)
{
    memset(cache, 0, sizeof(*cache));
}

void SpriteCache_Destroy(SpriteCache *cache)
{
    for (int i = 0; i < cache->count; i++) {
        if (cache->entries[i].texture) {
            SDL_DestroyTexture(cache->entries[i].texture);
            cache->entries[i].texture = NULL;
        }
    }
    cache->count = 0;
}

int SpriteCache_Find(const SpriteCache *cache, const char *name)
{
    for (int i = 0; i < cache->count; i++) {
        if (strcmp(cache->entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

const SpriteEntry *SpriteCache_Get(const SpriteCache *cache, int index)
{
    if (!cache || index < 0 || index >= cache->count) return NULL;
    return &cache->entries[index];
}

static int add_entry(SpriteCache *cache, SDL_Renderer *renderer,
                     const char *name, SDL_Surface *surface)
{
    if (cache->count >= SPRITE_CACHE_MAX) return -1;

    SpriteEntry *entry = &cache->entries[cache->count];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = 0;
    entry->width  = surface->w;
    entry->height = surface->h;

    entry->texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!entry->texture) return -1;

    SDL_SetTextureBlendMode(entry->texture, SDL_BLENDMODE_BLEND);
    return cache->count++;
}

int SpriteCache_Load(SpriteCache *cache, SDL_Renderer *renderer,
                     const char *path)
{
    /* Check cache first */
    int idx = SpriteCache_Find(cache, path);
    if (idx >= 0) return idx;

    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) {
        /* Try PNG */
        /* SDL3_image would handle this; for now BMP only */
        return -1;
    }

    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(surface);
    if (!converted) return -1;

    idx = add_entry(cache, renderer, path, converted);
    SDL_DestroySurface(converted);
    return idx;
}

int SpriteCache_LoadFromMemory(SpriteCache *cache, SDL_Renderer *renderer,
                               const char *name, const void *data, size_t size)
{
    int idx = SpriteCache_Find(cache, name);
    if (idx >= 0) return idx;

    /* Create SDL_IOStream from memory */
    SDL_IOStream *io = SDL_IOFromConstMem(data, size);
    if (!io) return -1;

    SDL_Surface *surface = SDL_LoadBMP_IO(io, true); /* closes io */
    if (!surface) return -1;

    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(surface);
    if (!converted) return -1;

    idx = add_entry(cache, renderer, name, converted);
    SDL_DestroySurface(converted);
    return idx;
}

void Sprite_Draw(SDL_Renderer *renderer, const SpriteEntry *sprite,
                 float x, float y, float scale)
{
    if (!sprite || !sprite->texture) return;

    SDL_FRect dst = {
        x, y,
        (float)sprite->width * scale,
        (float)sprite->height * scale
    };
    SDL_RenderTexture(renderer, sprite->texture, NULL, &dst);
}

void Sprite_DrawRect(SDL_Renderer *renderer, const SpriteEntry *sprite,
                     int src_x, int src_y, int src_w, int src_h,
                     float dst_x, float dst_y, float scale)
{
    if (!sprite || !sprite->texture) return;

    SDL_FRect src = {
        (float)src_x, (float)src_y,
        (float)src_w, (float)src_h
    };
    SDL_FRect dst = {
        dst_x, dst_y,
        (float)src_w * scale,
        (float)src_h * scale
    };
    SDL_RenderTexture(renderer, sprite->texture, &src, &dst);
}
