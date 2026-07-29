/**
 * TownTiles.cpp — TownTileRenderer implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Core tile rendering primitives for the isometric town view. All functions
 * operate on a TownTileRenderer context object that holds the 8-bit indexed
 * tile pixel data (+0x18), a 16-bit color palette lookup table (+0x14), and
 * the source buffer stride (+0x08).
 *
 * The 16-bit drawing functions remap 8-bit index bytes through the palette
 * and write to a locked DirectDraw 16-bit surface. Palette[0] is used as
 * a transparent/temporary save slot in some modes. Palette[1] is a
 * half-bright shadow color. Higher indices are regular tile colors.
 */

#include "TownTiles.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {
    /* Pixel format globals */
    extern int   g_surface_bpp;             /* 0x485274 — surface bits-per-pixel */
    extern int   g_surface_channel1;        /* 0x485278 — first color channel mask */
    extern int   g_surface_channel2;        /* 0x48527C — second color channel mask */
    extern int   g_surface_bshift;          /* 0x485280 — bit shift for half-bright */
    extern int   g_pixel_format_mask;       /* 0x485248 — computed: g_bshift << 1 */
}

/* Forward declaration: UIPANEL_Blit is the main dispatcher */
extern bool __thiscall UIPANEL_Blit(
    void* tile_map,
    uint32_t src_x, uint32_t src_y, int dest_x, uint32_t dest_y,
    int** dest_surface, uint32_t clip_left, uint32_t clip_top,
    int clip_right, uint32_t clip_bottom, uint32_t flags);  /* 0x42B050 */

/* ================================================================== */
/* InitTileCache                                                       */
/* Address: 0x42B9C0                                                   */
/*                                                                     */
/* Initializes a 16-bit destination surface from 8-bit source tile     */
/* data through palette lookup. Copies each source byte, remaps it     */
/* through this->palette[source_byte], and writes the resulting        */
/* uint16_t to the destination surface.                                */
/*                                                                     */
/* Called by: UIPANEL_Blit (dispatch for flags=0x01, 0x03)             */
/* ================================================================== */
bool TownTileRenderer::InitTileCache(
    int src_x, int src_y, int dest_x, int dest_y,
    uint8_t* dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    /* Guard: must have pixel data and palette loaded */
    if (this->pixels == NULL || this->palette == NULL) {    /* +0x18, +0x14 */
        return false;
    }

    /* Compute clipped tile dimensions */
    uint32_t tile_width   = (uint32_t)(clip_right - clip_left) & 0xFFFF;
    uint32_t half_pitch   = (dest_pitch >> 1) & 0xFFFF;              /* pitch in uint16_t units */
    uint32_t tile_height  = (uint32_t)(clip_bottom - clip_top) & 0xFFFF;

    /* Destination pointer: start of the clipped area on 16-bit surface */
    uint16_t* dest = (uint16_t*)((uint8_t*)dest_surface +
                                  (half_pitch * (uint32_t)src_y + (uint32_t)src_x) * 2);

    /* Source pointer: start of clipped area in 8-bit tile cache */
    uint8_t* src  = this->pixels + clip_top * this->stride + clip_left;   /* +0x18, +0x08 */

    /* Row strides (bytes to advance after each row) */
    int32_t src_stride  = this->stride - (int32_t)tile_width;             /* +0x08 */
    int32_t dest_stride = (int32_t)(half_pitch - tile_width) * 2;

    /* Source row end marker */
    uint8_t* src_row_end = src + (tile_height - 1) * this->stride + tile_width;  /* +0x08 */
    uint8_t* src_end     = src_row_end + (this->stride - tile_width);             /* +0x08 */
    uint8_t* src_limit   = src_row_end - 1;

    if (src >= src_limit) {
        return true;  /* empty region, nothing to draw */
    }

    /* Iterate rows */
    while (src < src_limit) {
        uint8_t* src_col_end = src + tile_width;

        /* Iterate columns within row */
        while (src < src_col_end) {
            uint8_t index = *src;
            *dest = this->palette[index];          /* +0x14 */
            src++;
            dest++;
        }

        /* Advance to next row */
        src  += src_stride;
        dest  = (uint16_t*)((uint8_t*)dest + dest_stride);
    }

    return true;
}

/* ================================================================== */
/* BlitElement                                                         */
/* Address: 0x42B960                                                   */
/*                                                                     */
/* Thin wrapper that extracts a DirectDraw surface pointer from an     */
/* element struct (at element+0x1C) and passes it as the dest_surface  */
/* to UIPANEL_Blit. The element struct is typically a UI sprite or     */
/* panel element that embeds a ddraw surface reference.                */
/*                                                                     */
/* Called by: EditWindow_render, Cursor_InitBackground,                */
/*            CGWND_TrackPiece_Render, UIPANEL_DrawButton              */
/* ================================================================== */
void TownTileRenderer::BlitElement(
    uint32_t src_x, uint32_t src_y, int dest_x, uint32_t dest_y,
    void* element, uint32_t clip_left, uint32_t clip_top,
    int clip_right, uint32_t clip_bottom, uint32_t flags)
{
    /* Extract the DirectDraw surface pointer from element+0x1C */
    int** ddraw_surface = *(int***)((uint8_t*)element + 0x1C);

    /* Delegate to the main blit dispatcher */
    UIPANEL_Blit(this, src_x, src_y, dest_x, dest_y,
                  ddraw_surface,
                  clip_left, clip_top, clip_right, clip_bottom, flags);
}

/* ================================================================== */
/* DrawTile                                                            */
/* Address: 0x42BA90                                                   */
/*                                                                     */
/* Base tile rendering function. For each pixel:
/*   1. Read destination pixel, save it to palette[0]
/*   2. Read source byte (8-bit index from this->pixels)
/*   3. Look up color from this->palette[source_byte]
/*   4. Write remapped color to destination
/*
/* Since palette[0] is overwritten with the destination pixel before
/* the lookup, source index 0 produces the original destination value
/* (transparent pass-through). This avoids a conditional branch per pixel.
/*
/* Called by: UIPANEL_Blit (flags=0x00, and default for unknown flags)
/* ================================================================== */
bool TownTileRenderer::DrawTile(
    int src_x, int src_y, int dest_x, int dest_y,
    int dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    uint8_t* pixels  = this->pixels;          /* +0x18 */
    uint16_t* pal    = this->palette;          /* +0x14 */

    /* Guard: must have pixel data and palette */
    if (pixels == NULL || pal == NULL) {
        return false;
    }

    /* Compute clipped region dimensions */
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;
    int32_t tile_height = (clip_bottom - clip_top) & 0xFFFF;

    /* Early out for empty region */
    if (tile_width == 0 || tile_height == 0) {
        return true;
    }

    /* Destination: 16-bit pointer at (src_x, src_y) within the locked surface */
    uint16_t* dest = (uint16_t*)((uintptr_t)dest_surface + (half_pitch * src_y + src_x) * 2);

    /* Source: 8-bit pointer at (clip_left, clip_top) in tile cache */
    uint8_t* src = pixels + clip_top * this->stride + clip_left;   /* +0x08 */

    int32_t src_advance  = this->stride - tile_width;               /* +0x08 */
    int32_t dest_advance = (half_pitch - tile_width) * 2;

    /* Iterate rows */
    for (int32_t row = 0; row < tile_height; row++) {
        /* Iterate columns within this row */
        for (int32_t col = 0; col < tile_width; col++) {
            uint16_t saved_dest = *dest;      /* save current dest pixel */
            uint8_t  index = *src++;          /* read 8-bit source index */
            *dest = pal[index];               /* write remapped color */
            dest++;
        }

        /* Advance to next row (skip remaining stride) */
        src  += src_advance;
        dest  = (uint16_t*)((uint8_t*)dest + dest_advance);
    }

    return true;
}

/* ================================================================== */
/* FlushTileCache                                                      */
/* Address: 0x42BB90                                                   */
/*                                                                     */
/* Flushes cached tile data by expanding each 8-bit source byte into   */
/* a 2x2 block of identical 16-bit pixels on the destination surface.  */
/* Each source pixel becomes 4 identical destination pixels arranged   */
/* as: [P] [P]    where P = palette[source_byte]                      */
/*      [P] [P]                                                        */
/* Palette index 0 = skip (preserves original destination content).    */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x04, 0x84)                          */
/* ================================================================== */
bool TownTileRenderer::FlushTileCache(
    int src_x, int src_y, int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, uint32_t clip_bottom)
{
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Destination pointer for 16-bit 2x2 blocks */
    uint16_t* dest = (uint16_t*)((uintptr_t)dest_surface +
                                  (half_pitch * (uint32_t)src_y + (uint32_t)src_x) * 2);

    int32_t clip_area_width = (clip_left + tile_width) & 0xFFFF;

    /* Determine initial row for iteration */
    uint32_t row = 0;
    uint32_t num_rows = clip_bottom;

    if (num_rows == 0) {
        return true;
    }

    /* Iterate output rows (each source row produces 2 output rows in 2x2 expansion) */
    do {
        uint32_t col = 0;

        if (clip_area_width != 0) {
            do {
                /* Read source byte from tile cache */
                uint8_t index;
                index = *(this->pixels + tile_width * (row & 0xFFFF) + col);  /* +0x18 */

                if (index != 0) {
                    /* Write 2x2 block of the palette color */
                    uint16_t color = *(this->palette + index);  /* +0x14 */

                    dest[0] = color;           /* top-left */
                    dest[1] = color;           /* top-right */
                    dest[half_pitch] = color;  /* bottom-left */
                    dest[half_pitch + 1] = color; /* bottom-right */
                }

                dest += 2;                     /* advance 2 pixels (one source -> 2 output) */
                col++;
            } while ((col & 0xFFFF) < (uint32_t)clip_area_width);
        }

        row++;
        /* Advance dest by (half_pitch - tile_width) * 2 uint16_t units */
        dest = dest + (half_pitch - tile_width) * 2;

    } while ((row & 0xFFFF) < num_rows);

    return true;
}

/* ================================================================== */
/* DrawCachedTile                                                      */
/* Address: 0x42BC80                                                   */
/*                                                                     */
/* Draws a tile from the cache with 2x2 expansion per source pixel.    */
/* Almost identical to FlushTileCache but always writes all pixels     */
/* (no palette[0] skip check). Used when returning cached tile regions */
/* to the destination surface.                                         */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x05, 0x85)                          */
/* ================================================================== */
bool TownTileRenderer::DrawCachedTile(
    uint32_t src_x, int src_y, int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, uint32_t clip_bottom)
{
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Destination pointer for 16-bit 2x2 blocks */
    uint16_t* dest = (uint16_t*)((uintptr_t)dest_surface +
                                  (half_pitch * src_y + (int)src_x) * 2);

    int32_t clip_area_width = (clip_left + tile_width) & 0xFFFF;

    uint32_t row = 0;
    uint32_t num_rows = clip_bottom;

    if (num_rows == 0) {
        return true;
    }

    /* Same 2x2 expansion pattern as FlushTileCache */
    do {
        uint32_t col = 0;

        if (clip_area_width != 0) {
            do {
                uint8_t index;
                index = *(this->pixels + tile_width * (row & 0xFFFF) + col);  /* +0x18 */
                uint16_t color = *(this->palette + index);                     /* +0x14 */

                /* Always writes 2x2 block (no transparent skip) */
                dest[0] = color;
                dest[1] = color;
                dest[half_pitch] = color;
                dest[half_pitch + 1] = color;

                dest += 2;
                col++;
            } while ((col & 0xFFFF) < (uint32_t)clip_area_width);
        }

        row++;
        dest = dest + (half_pitch - tile_width) * 2;

    } while ((row & 0xFFFF) < num_rows);

    return true;
}

/* ================================================================== */
/* DrawTileEx                                                          */
/* Address: 0x42BD70                                                   */
/*                                                                     */
/* Extended tile drawing with 3x2 pixel expansion. Each 8-bit source   */
/* byte produces a 3-wide, 2-high block on the 16-bit destination:     */
/*                                                                     */
/*   [P] [P] [P]    -- row 0 (3 pixels, 6 bytes)                     */
/*   [P] [P] [P]    -- row 1 (3 pixels, 6 bytes)                     */
/*                                                                     */
/* Palette index 0 = transparent (skip block). Used for isometric      */
/* tile edge rendering where tiles need extra width coverage.          */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags in range 0x10-0x1F)                  */
/* ================================================================== */
bool TownTileRenderer::DrawTileEx(
    int src_x, int src_y, int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    uint32_t clip_left, uint32_t clip_top,
    int clip_right, int clip_bottom)
{
    int32_t tile_width  = (clip_right - (int)clip_left) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;
    int32_t clip_area_width = ((int)clip_left + tile_width) & 0xFFFF;

    /* Destination pointer: each output pixel is 3 wide on dest */
    uint16_t* dest = (uint16_t*)((uintptr_t)dest_surface +
                                  (half_pitch * (uint32_t)src_y + (uint32_t)src_x) * 2);

    /* Height = (clip_bottom & 0xFFFF) -- clipped to 16 bits */
    int32_t out_height = (clip_bottom & 0xFFFF);

    uint32_t row = 0;

    if (out_height == 0) {
        return true;
    }

    /* Iterate rows */
    do {
        uint32_t col = 0;

        if (clip_area_width != 0) {
            uint16_t* dest_row_start = &dest[3];

            do {
                /* Read source byte */
                uint8_t index;
                index = *(this->pixels + tile_width * (row & 0xFFFF) + col);  /* +0x18 */

                if (index != 0) {
                    uint16_t color = *(this->palette + index);  /* +0x14 */

                    /* Write 3x2 block */
                    dest[0] = color;
                    dest[1] = color;
                    dest[2] = color;

                    uint16_t* row1 = &dest[half_pitch - 2];
                    row1[0] = color;
                    row1[1] = color;
                    row1[2] = color;

                    uint16_t* row2 = (uint16_t*)((uint8_t*)&dest[half_pitch - 2] +
                                                  (half_pitch - 2) * 2 + 2);
                    row2[0] = color;
                    row2[1] = color;
                    row2[2] = color;
                }

                dest = dest_row_start;
                dest_row_start += 3;
                col++;
            } while ((col & 0xFFFF) < (uint32_t)clip_area_width);
        }

        row++;
        /* Advance dest by (half_pitch - tile_width) * 3 uint16_t units */
        dest = dest + (half_pitch - tile_width) * 3;

    } while ((row & 0xFFFF) < (uint32_t)out_height);

    return true;
}

/* ================================================================== */
/* DrawTileLine                                                        */
/* Address: 0x42BEC0                                                   */
/*                                                                     */
/* Draws a tile with line-effect alpha blending. Uses pixel format     */
/* globals to compute per-channel bit masks for half-bright blending.  */
/*                                                                     */
/* Saves destination pixels to this->palette (used as a line buffer),  */
/* then blends source with destination by averaging (>> 1) each       */
/* color channel separately. This creates a semi-transparent overlay   */
/* effect used for selection/highlight lines on the isometric grid.    */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x400, 0x402)                        */
/* ================================================================== */
bool TownTileRenderer::DrawTileLine(
    int src_x, int src_y, int dest_x, int dest_y,
    int dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Destination 16-bit buffer pointer */
    uint16_t* dest = (uint16_t*)((uintptr_t)dest_surface + (half_pitch * src_y + src_x) * 2);

    /* Source position in tile cache */
    int src_pos = clip_top * this->stride + clip_left;          /* +0x08 */

    /* Tile height */
    int32_t tile_height = (clip_bottom - clip_top) & 0xFFFF;

    /* Compute alpha blending masks from pixel format globals */
    uint16_t channel_mask1;   /* first color channel mask */
    uint16_t channel_mask2;   /* second color channel mask */
    uint16_t blend_mask;      /* inverse of (channel_mask1 | channel_mask2 | 1) */

    if (g_surface_bpp == 0x235) {
        /* 565 format: channel1 R[4..0] (DEC 1) -> shift by (channel2-1) */
        channel_mask1 = (uint16_t)(1 << ((g_surface_channel2 - 1) & 0x1F));
        channel_mask2 = (uint16_t)(1 << (g_surface_channel1 & 0x1F));
    } else {
        /* 555 format: standard */
        channel_mask1 = (uint16_t)(1 << (g_surface_channel2 & 0x1F));
        channel_mask2 = (uint16_t)(1 << (g_surface_channel1 & 0x1F));
    }

    blend_mask = (uint16_t)~(channel_mask1 | channel_mask2 | 1);

    /* End of destination for this operation */
    uint16_t* dest_end = dest + (tile_height - 1) * half_pitch + tile_width;
    int32_t src_stride_advance = this->stride - tile_width;      /* +0x08 */
    int32_t dest_stride = half_pitch - tile_width;

    /* Iterate rows */
    while (dest < dest_end) {
        uint16_t* dest_row_end = dest + tile_width;

        /* Iterate columns */
        while (dest < dest_row_end) {
            /* Save destination pixel to palette (used as line/color buffer) */
            this->palette[0] = *dest;                           /* +0x14 */

            /* Read source byte and check for transparency */
            uint16_t index = *(this->pixels + src_pos);         /* +0x18 */
            if (index != 0) {
                /* Shadow: save half-bright version of original dest pixel */
                this->palette[1] = (*dest >> 1) & g_surface_bshift;  /* +0x14 */

                /* Blend: source color & palette index, channel-averaged with dest */
                uint16_t src_color = *(this->palette + index);  /* +0x14 */
                *dest = ((src_color & blend_mask) >> 1) +
                        ((*dest & blend_mask) >> 1);
            }

            dest++;
            src_pos++;
        }

        /* Advance to next row */
        src_pos += src_stride_advance;
        dest    += dest_stride;
    }

    return true;
}

/* ================================================================== */
/* DrawTiles16bpp_Strided                                              */
/* Address: 0x42C050                                                   */
/*                                                                     */
/* Standard left-to-right, top-to-bottom 16-bit tile drawing with      */
/* shadow support.                                                     */
/*                                                                     */
/* Palette usage:                                                      */
/*   palette[0] = saved destination pixel (temporary)                 */
/*   palette[1] = half-bright shadow (dest >> 1 & g_surface_bshift)  */
/*   palette[2+] = normal tile colors                                 */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x02)                                */
/* ================================================================== */
bool TownTileRenderer::DrawTiles16bpp_Strided(
    int src_x, int src_y, int dest_x, int dest_y,
    int dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Destination pointer */
    uint16_t* dest = (uint16_t*)((uintptr_t)dest_surface + (half_pitch * src_y + src_x) * 2);
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t tile_height = (clip_bottom - clip_top) & 0xFFFF;

    /* Source position in tile cache */
    uint8_t* src = this->pixels + clip_top * this->stride + clip_left;  /* +0x18, +0x08 */

    /* Precompute the pixel format mask for shadow computation */
    g_pixel_format_mask = g_surface_bshift << 1;

    if (tile_height == 0 || tile_width == 0) {
        return true;
    }

    /* Iterate rows */
    for (int32_t row = 0; row < tile_height; row++) {
        /* Iterate columns */
        for (int32_t col = 0; col < tile_width; col++) {
            /* Save destination pixel to palette[0] */
            uint16_t saved = *dest;
            this->palette[0] = saved;                               /* +0x14 */

            /* Compute half-bright shadow and store in palette[1] */
            this->palette[1] = (uint16_t)((saved & g_pixel_format_mask) >> 1);  /* +0x14 */

            /* Read source byte and write remapped color */
            uint8_t index = *src++;
            *dest = this->palette[index];                           /* +0x14 */
            dest++;
        }

        /* Advance to next row */
        dest  = (uint16_t*)((uint8_t*)dest + half_pitch * 2);
        src  += this->stride;                                       /* +0x08 */
    }

    return true;
}

/* ================================================================== */
/* DrawTiles16bpp_Reversed                                             */
/* Address: 0x42C130                                                   */
/*                                                                     */
/* Horizontal-mirror 16-bit tile drawing. Reads source bytes from      */
/* right to left within each row, producing a horizontally-mirrored    */
/* tile on the destination surface.                                    */
/*                                                                     */
/* Like Strided but the source pointer starts at the right edge of     */
/* the clip rectangle and decrements. This reverses the tile along     */
/* the X axis.                                                         */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x22)                                */
/* ================================================================== */
bool TownTileRenderer::DrawTiles16bpp_Reversed(
    int src_x, int src_y, int dest_x, int dest_y,
    int dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;
    int32_t tile_height = (clip_bottom - clip_top) & 0xFFFF;

    /* Destination pointer */
    uint16_t* dest = (uint16_t*)((uintptr_t)dest_surface + (half_pitch * src_y + src_x) * 2);

    /* Source: starts at RIGHT edge of clip rect and goes left */
    uint8_t* src = this->pixels + clip_top * this->stride + clip_right;  /* +0x18, +0x08 */

    /* Precompute pixel format mask */
    g_pixel_format_mask = g_surface_bshift << 1;

    if (tile_height == 0 || tile_width == 0) {
        return true;
    }

    /* Iterate rows */
    for (int32_t row = 0; row < tile_height; row++) {
        int32_t col = tile_width;

        /* Iterate columns (right-to-left) */
        while (col != 0) {
            col--;

            uint16_t saved = *dest;
            this->palette[0] = saved;                               /* +0x14 */

            /* Half-bright shadow in palette[1] */
            this->palette[1] = g_surface_bshift & (saved >> 1);     /* +0x14 */

            /* Read source byte (right-to-left) and write remapped color */
            uint8_t index = *src;
            src--;
            *dest = this->palette[index];                           /* +0x14 */
            dest++;
        }

        /* Advance to next row */
        src  = src + this->stride + tile_width;                     /* +0x08 */
        dest = dest + (half_pitch - tile_width);
    }

    return true;
}

/* ================================================================== */
/* DrawTiles16bpp_Checker                                              */
/* Address: 0x42C220                                                   */
/*                                                                     */
/* Checkerboard 16-bit tile drawing. Only renders even-numbered rows   */
/* (skipping odd rows) to produce a checkerboard transparency effect.  */
/*                                                                     */
/* When clip_top is odd, adjusts source_y by 1 to maintain row         */
/* alignment (the checkerboard pattern shifts to compensate).          */
/* Source stride is doubled for the interleaved row access pattern.    */
/*                                                                     */
/* Palette[0] = save destination, palette[1] = half-bright shadow.    */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x102)                               */
/* ================================================================== */
bool TownTileRenderer::DrawTiles16bpp_Checker(
    int src_x, int src_y, int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    int clip_left, uint32_t clip_top,
    int clip_right, int clip_bottom)
{
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Adjust starting position when clip_top is odd */
    if ((clip_top & 1) != 0) {
        clip_top++;
        src_y++;
    }

    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t tile_height = (clip_bottom - (int)clip_top) & 0xFFFF;

    /* Destination pointer */
    uint16_t* dest = (uint16_t*)((uintptr_t)dest_surface +
                                  (half_pitch * (uint32_t)src_y + (uint32_t)src_x) * 2);
    int src_pos = (int)clip_top * this->stride + clip_left;         /* +0x08 */

    uint32_t row = 0;

    if (tile_height == 0) {
        return true;
    }

    /* Iterate rows -- step by 2 (checkerboard: every other row) */
    do {
        uint32_t col = 0;

        if (tile_width != 0) {
            uint16_t* col_dest = dest;

            do {
                uint8_t index;
                index = *(this->pixels + src_pos);                  /* +0x18 */

                /* Save destination to palette[0], shadow to palette[1] */
                this->palette[0] = *col_dest;                       /* +0x14 */
                this->palette[1] = *col_dest >> 1 & g_surface_bshift; /* +0x14 */

                /* Write remapped source color */
                *col_dest = this->palette[index];                   /* +0x14 */

                col_dest++;
                src_pos++;
                col++;
            } while ((col & 0xFFFF) < (uint32_t)tile_width);
        }

        /* Advance by 2 rows (checkerboard skip) */
        dest   = dest + (half_pitch * 2 - tile_width);
        src_pos = src_pos + (this->stride * 2 - tile_width);        /* +0x08 */
        row += 2;

    } while ((row & 0xFFFF) < (uint32_t)tile_height);

    return true;
}

/* ================================================================== */
/* DrawTiles16bpp_Staggered — Staggered 16bpp tile dither overlay       */
/* Address: 0x42C470                                                   */
/*                                                                     */
/* Draws a staggered 16bpp tile with semi-transparency checkerboard     */
/* effect. Every other pixel is dimmed (shifted right by 1) based on   */
/* a source mask, creating a dither pattern for isometric tile grid     */
/* rendering.                                                           */
/*                                                                     */
/* The checkerboard flips each pixel so pixel (0,0) is full-bright,     */
/* (0,1) is dimmed, (1,0) is dimmed, (1,1) is full-bright — a classic */
/* 2x2 dither pattern. Palette[0] = saved destination pixel for source */
/* index 0 pass-through. Palette[1] = dimmed (>> 1 & bshift).          */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x202)                                */
/* ================================================================== */
bool TownTileRenderer::DrawTiles16bpp_Staggered(
    int src_x, int src_y, int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    int clip_left, uint32_t clip_top,
    int clip_right, int clip_bottom)
{
    uint32_t half_pitch = (dest_pitch >> 1) & 0xFFFF;
    uint32_t tile_width  = (uint32_t)(clip_right - clip_left) & 0xFFFF;
    uint32_t tile_height = (uint32_t)(clip_bottom - (int)clip_top) & 0xFFFF;

    /* Destination 16-bit pointer at (src_x, src_y) */
    uint16_t* dest = (uint16_t*)((uintptr_t)dest_surface +
                                 (half_pitch * (uint32_t)src_y + (uint32_t)src_x) * 2);

    /* Source position in 8-bit tile cache */
    int src_pos = (int)clip_top * this->stride + clip_left;         /* +0x08 */

    /* Checkerboard toggle — starts based on clip_left parity */
    bool toggle = ((clip_left & 1) != 0);

    if (tile_height == 0 || tile_width == 0) {
        return true;
    }

    /* Iterate rows */
    for (uint32_t row = 0; row < tile_height; row++) {
        uint16_t* dest_row = dest;

        /* Iterate columns */
        for (uint32_t col = 0; col < tile_width; col++) {
            if (toggle) {
                /* Read source byte and draw with dimmed overlay */
                uint8_t index = *(this->pixels + src_pos);           /* +0x18 */

                /* Save destination pixel to palette[0] */
                this->palette[0] = *dest_row;                       /* +0x14 */

                /* Compute dimmed (half-bright) pixel in palette[1] */
                this->palette[1] = (*dest_row >> 1) & g_surface_bshift;  /* +0x14 */

                /* Write source color (index remapped through palette) */
                *dest_row = this->palette[index];                   /* +0x14 */
            }

            toggle = !toggle;
            dest_row++;
            src_pos++;
        }

        /* Advance to next row */
        dest     = (uint16_t*)((uint8_t*)dest + half_pitch * 2);
        src_pos += this->stride - (int)tile_width;                  /* +0x08 */
    }

    return true;
}

/* ================================================================== */
/* CopyTiles8bpp_Transparent                                           */
/* Address: 0x42C330                                                   */
/*                                                                     */
/* Copies 8-bit indexed pixel data from this tile cache to an external */
/* 8bpp destination surface, skipping pixel value 0 (transparent).     */
/*                                                                     */
/* Unlike the 16-bit drawing functions, this operates in 8bpp mode and */
/* copies raw index bytes (no palette lookup). The dest_surface is a   */
/* plain 8-bit pixel buffer.                                           */
/*                                                                     */
/* NOTE: Parameters dest_r and dest_b (destination rect right/bottom)  */
/* are passed by callers but not used by the function internally.      */
/*                                                                     */
/* Called by: RESMGR_AnimateClock (0x447400) -- clock digit sprites     */
/* ================================================================== */
void TownTileRenderer::CopyTiles8bpp_Transparent(
    int dest_x, int dest_y,
    int dest_r, int dest_b,
    uint8_t* dest_surface, int dest_pitch,
    int src_left, int src_top,
    int src_right, int src_bottom)
{
    /* Source clip area dimensions */
    int32_t tile_width  = (src_right - src_left) & 0xFFFF;
    int32_t tile_height = (src_bottom - src_top) & 0xFFFF;

    /* Destination pointer: 8-bit at (dest_x, dest_y) */
    uint8_t* dest = dest_surface + dest_pitch * dest_y + dest_x;

    /* Source start in tile cache */
    int src_pos = src_top * this->stride + src_left;                /* +0x08 */

    /* Iterate rows */
    for (uint32_t row = 0; (row & 0xFFFF) < (uint32_t)tile_height; row++) {
        for (uint32_t col = 0; (col & 0xFFFF) < (uint32_t)tile_width; col++) {
            uint8_t pixel = *(this->pixels + src_pos);              /* +0x18 */

            if (pixel != 0) {
                *dest = pixel;
            }

            dest++;
            src_pos++;
        }

        /* Advance to next row */
        dest    += dest_pitch - tile_width;
        src_pos += this->stride - tile_width;                       /* +0x08 */
    }
}

/* ================================================================== */
/* CopyTiles8bpp_Direct                                                */
/* Address: 0x42C3D0                                                   */
/*                                                                     */
/* Direct copy of 8-bit indexed pixel data from this tile cache to     */
/* an external 8bpp destination surface. Copies all pixels including   */
/* index 0 (no transparency check).                                    */
/*                                                                     */
/* Used by UIPANEL_FillRect for panel background fills where every     */
/* pixel should be copied regardless of value.                         */
/*                                                                     */
/* NOTE: Parameters dest_r and dest_b (destination rect right/bottom)  */
/* are passed by callers but not used by the function internally.      */
/*                                                                     */
/* Called by: UIPANEL_FillRect (0x42A610)                              */
/* ================================================================== */
void TownTileRenderer::CopyTiles8bpp_Direct(
    int dest_x, int dest_y,
    int dest_r, int dest_b,
    uint8_t* dest_surface, int dest_pitch,
    int src_left, int src_top,
    int src_right, int src_bottom)
{
    /* Source clip area dimensions */
    int32_t tile_width  = (src_right - src_left) & 0xFFFF;
    int32_t tile_height = (src_bottom - src_top) & 0xFFFF;

    /* Destination pointer: 8-bit at (dest_x, dest_y) */
    uint8_t* dest = dest_surface + dest_pitch * dest_y + dest_x;

    /* Source start in tile cache */
    int src_pos = src_top * this->stride + src_left;                /* +0x08 */

    /* Iterate rows */
    for (uint32_t row = 0; (row & 0xFFFF) < (uint32_t)tile_height; row++) {
        for (uint32_t col = 0; (col & 0xFFFF) < (uint32_t)tile_width; col++) {
            uint8_t pixel = *(this->pixels + src_pos);              /* +0x18 */
            *dest = pixel;

            dest++;
            src_pos++;
        }

        /* Advance to next row */
        dest    += dest_pitch - tile_width;
        src_pos += this->stride - tile_width;                       /* +0x08 */
    }
}

/* ================================================================== */
/* BlitTileSurface                                                     */
/* Address: 0x42C890                                                   */
/*                                                                     */
/* Right-to-left 16-bit tile blit with transparency. Reads 8-bit       */
/* indexed tile data from the cache in reverse X direction (right-to-  */
/* left) while writing left-to-right to the destination surface.       */
/* Palette index 0 = transparent (skipped, preserves destination).     */
/*                                                                     */
/* Parameter notes: dest_x and dest_y (param_3, param_4) are present   */
/* in the UIPANEL_Blit dispatch calling convention but are NOT used    */
/* by the implementation. The destination position is determined from  */
/* src_x and src_y.                                                    */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x20, dispatch @ 0x42B6C9)          */
/* ================================================================== */
bool TownTileRenderer::BlitTileSurface(
    int src_x, int src_y,
    int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top,
    int clip_right, int clip_bottom)
{
    /* Compute clipped region dimensions */
    uint32_t tile_width  = (uint32_t)(clip_right - clip_left) & 0xFFFF;
    uint32_t tile_height = (uint32_t)(clip_bottom - clip_top) & 0xFFFF;
    uint32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Destination pointer: 16-bit at (src_x, src_y) on surface       */
    uint16_t* dest = (uint16_t*)((uintptr_t)dest_surface +
                                  (half_pitch * (uint32_t)src_y + (uint32_t)src_x) * 2);

    /* Source position: RIGHT EDGE of clip rect, read right-to-left   */
    /* Start at clip_top * stride + (clip_right - 1)                  */
    int src_pos = clip_top * this->stride + clip_right - 1;          /* +0x08 */

    /* Early exit for empty clip region */
    if (tile_height == 0) {
        return true;
    }

    /* Iterate rows */
    for (uint32_t row = 0; row < tile_height; row++) {
        /* Iterate columns -- source reads right-to-left */
        uint32_t col = tile_width;
        while (col != 0) {
            col--;

            /* Read source byte and check for transparency            */
            uint8_t index = *(this->pixels + src_pos);               /* +0x18 */
            if (index != 0) {
                /* Write remapped color through palette               */
                *dest = this->palette[index];                        /* +0x14 */
            }

            dest++;         /* advance destination LEFT-to-RIGHT      */
            src_pos--;       /* advance source RIGHT-to-LEFT          */
        }

        /* Advance to next row */
        /* Dest: skip to next row = (half_pitch - tile_width) * 2 bytes */
        dest = (uint16_t*)((uint8_t*)dest + (half_pitch - tile_width) * 2);

        /* Source: advance to right edge of next row                  */
        src_pos += this->stride + tile_width;                        /* +0x08 */
    }

    return true;
}
