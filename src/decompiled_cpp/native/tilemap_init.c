/**
 * TileMap_Init — Initialise tilemap dimensions and occupancy bitmap
 * Address: 0x454E60
 * Size: 315 bytes
 * Calling convention: __thiscall (ECX = TileMap*, 1 stack param)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * NOTE: This function was originally mislabelled "DSOUND_Init" in the Ghidra
 * database. It does NOT initialise DirectSound — it initialises the global
 * TileMap struct (g_tilemap at 0x4AAD08), setting its dimensions based on
 * screen/window size and allocating a bit-occupancy buffer.
 *
 * The occupancy buffer at TileMap+0x52484 is a bitmap where each bit
 * represents one 16x16-pixel tile. All bits are initialised to 1 (occupied).
 *
 * Called by:
 *   GameLoop_Setup     (0x406DA3) — param_1 = 0: use screen dimensions
 *   UI_Panel_HitTest   (0x422734, 0x4227E1) — param_1 = 0 or 1: menu mode
 *
 * @param tilemap  TileMap* to initialise (passed in ECX)
 * @param mode     0 = use g_screen_width/g_screen_height;
 *                 1 = force 1024x768 fallback (menu mode)
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern int32_t  g_screen_width;         /* 0x4851D8 */
extern int32_t  g_screen_height;        /* 0x485214 */
extern int32_t  g_client_offset_x;      /* 0x485228 */
extern int32_t  g_client_width;         /* 0x485220 */
extern int32_t  g_client_offset_y;      /* 0x48522C */
extern int32_t  g_client_height;        /* 0x485224 */

void*  __cdecl operator_new(size_t size);       /* 0x465CE0 */
void   __cdecl GLOBAL_free(void* ptr);           /* 0x465CD0 */

/* ================================================================== */
/* TileMap struct — partial definition based on fields written here    */
/* Size is very large (~0x53000+ bytes). Full definition is TBD.      */
/* ================================================================== */

struct TileMap {
    /* Fields initialised by this function */
    int32_t   width;              /* +0x04  tilemap pixel width */
    int32_t   height;             /* +0x08  tilemap pixel height */
    int32_t   unk_0c;             /* +0x0c  zeroed */
    int32_t   unk_10;             /* +0x10  zeroed */
    int32_t   width_copy;         /* +0x14  copy of width */
    int32_t   height_copy;        /* +0x18  copy of height */
    int32_t   unk_1c;             /* +0x1c  zeroed */
    int32_t   unk_20;             /* +0x20  zeroed */
    int32_t   half_width;         /* +0x24  width / 2 */
    int32_t   half_height;        /* +0x28  height / 2 */
    int32_t   center_x;           /* +0x2c  client area center X */
    int32_t   center_y;           /* +0x30  client area center Y */
    int16_t   tiles_x;            /* +0x3e  (width + 15) / 16 */
    int16_t   tiles_y;            /* +0x40  (height + 15) / 16 */
    uint8_t   pad_42[0x52484 - 0x42];  /* padding up to occupancy bitmap */
    uint8_t*  occupancy;          /* +0x52484  bit-occupancy buffer */
};

/* ================================================================== */
/* TileMap_Init                                                        */
/* ================================================================== */

void __thiscall TileMap_Init(struct TileMap* tilemap, char mode)
{
    int32_t w, h;

    /* --- Determine tilemap dimensions --- */
    if (mode == 0) {
        if (g_screen_width > 0x3FF) {        /* > 1023 */
            if (g_screen_width < 0x501) {    /* < 1281 */
                /* Use screen dimensions */
                tilemap->width  = g_screen_width;   /* +0x04 */
                tilemap->height = g_screen_height;  /* +0x08 */
            } else {
                /* Clamp to 1280x1024 */
                tilemap->width  = 0x500;            /* +0x04 = 1280 */
                tilemap->height = 0x400;            /* +0x08 = 1024 */
            }
        } else {
            /* Screen too narrow — default to 1024 */
            tilemap->width  = 0x400;                /* +0x04 = 1024 */
            tilemap->height = 0x300;                /* +0x08 = 768 */
        }
    } else {
        /* Menu mode — fixed 1024x768 */
        tilemap->width  = 0x400;                    /* +0x04 = 1024 */
        tilemap->height = 0x300;                    /* +0x08 = 768 */
    }

    w = tilemap->width;                             /* +0x04 */
    h = tilemap->height;                            /* +0x08 */

    /* --- Initialise derived fields --- */
    tilemap->unk_0c       = 0;                      /* +0x0c */
    tilemap->unk_10       = 0;                      /* +0x10 */
    tilemap->half_width   = w / 2;                  /* +0x24 (arithmetic shift) */
    tilemap->width_copy   = w;                      /* +0x14 */
    tilemap->height_copy  = h;                      /* +0x18 */
    tilemap->unk_1c       = 0;                      /* +0x1c */
    tilemap->unk_20       = 0;                      /* +0x20 */
    tilemap->half_height  = h / 2;                  /* +0x28 (arithmetic shift) */

    /* Compute client area center X */
    tilemap->center_x = ((g_client_offset_x - g_client_width) / 2) + g_client_width;  /* +0x2c */

    /* Compute client area center Y */
    tilemap->center_y = ((g_client_offset_y - g_client_height) / 2) + g_client_height; /* +0x30 */

    /* Compute tile grid dimensions (16-pixel tiles) */
    tilemap->tiles_x = (int16_t)((w + 15) >> 4);   /* +0x3e */
    tilemap->tiles_y = (int16_t)((h + 15) >> 4);   /* +0x40 */

    /* --- Allocate occupancy bitmap --- */
    /* Free any existing occupancy buffer */
    if (tilemap->occupancy != NULL) {                /* +0x52484 */
        GLOBAL_free(tilemap->occupancy);
        tilemap->occupancy = NULL;                  /* +0x52484 */
    }

    /* Calculate bitmap size: tiles_x * tiles_y bits, rounded up to bytes + 1 */
    int32_t total_tiles = (int32_t)tilemap->tiles_x * (int32_t)tilemap->tiles_y;
    int32_t bitmap_bytes = ((total_tiles + 7) >> 3) + 1;

    /* Allocate */
    uint8_t* bitmap = (uint8_t*)operator_new(bitmap_bytes);

    /* BUG: The original code allocates with operator_new (not GLOBAL_malloc)
       but frees with GLOBAL_free. This mismatch is intentional — the CRT
       operator_new may internally call the same heap allocator, or this
       may reflect an MSVC6 operator new / delete mismatch. */

    tilemap->occupancy = bitmap;                    /* +0x52484 */

    if (bitmap != NULL) {
        /* Initialise all bits to 1 (all tiles occupied / blocked) */
        /* Use REP STOSD for the bulk fill */
        uint32_t* dword_ptr = (uint32_t*)bitmap;
        uint32_t dword_count = bitmap_bytes >> 2;
        uint32_t byte_remainder = bitmap_bytes & 3;

        /* Fill with 0xFFFFFFFF (all bits set) */
        for (uint32_t i = 0; i < dword_count; i++) {
            *dword_ptr++ = 0xFFFFFFFF;
        }
        /* Fill remaining bytes */
        for (uint32_t i = 0; i < byte_remainder; i++) {
            *(uint8_t*)dword_ptr = 0xFF;
            dword_ptr = (uint32_t*)((uint8_t*)dword_ptr + 1);
        }
    }
}
