// Status: INTEGRATED
/**
 * TownTiles.cpp — UIPANEL_Surface tile-rendering method implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Core tile rendering primitives for the isometric town view. These were
 * originally reverse engineered as a standalone "TownTileRenderer" class
 * before it was discovered that class was a duplicate view of
 * UIPANEL_Surface (graphics/LOCOBITMAP.h) — every field it documented
 * (mode, stride, palette, pixels, surface_ref) is UIPANEL_Surface's own
 * mode/width/palette_ptr/pixels/ddraw_surf at the identical offsets, and
 * UIPANEL_Blit (0x42B050) passes its own `this` directly as the receiver
 * of these calls. See the method docs on UIPANEL_Surface for the per-flag
 * dispatch table. All functions operate on the object's 8-bit indexed tile
 * pixel data (+0x18), 16-bit color palette lookup table (+0x14), and the
 * source buffer stride (width, +0x08).
 *
 * The 16-bit drawing functions remap 8-bit index bytes through the palette
 * and write to a locked DirectDraw 16-bit surface. Palette[0] is used as
 * a transparent/temporary save slot in some modes. Palette[1] is a
 * half-bright shadow color. Higher indices are regular tile colors.
 *
 * Validated instruction-by-instruction against Ghidra (database "locon"):
 *   InitTileCache 0x42B9C0, DrawTile 0x42BA90, FlushTileCache 0x42BB90,
 *   DrawCachedTile 0x42BC80, BlitElement 0x42B960.
 */

#include "../graphics/LOCOBITMAP.h"
#include "sdl3_ddraw.h"   /* typed IDirectDrawSurface4 + DDSURFACEDESC bridge */
#include <cassert>
#include <cstdio>
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {
    /* Pixel format globals */
    extern int   g_surface_bpp;             /* 0x485274 — surface bits-per-pixel */
    extern int   g_surface_channel1;        /* 0x485278 — red channel bit shift (10/11) */
    extern int   g_surface_channel2;        /* 0x48527C — green channel bit shift (5/6) */
    extern int   g_surface_bshift;          /* 0x485280 — bit shift for half-bright */
    extern int   g_pixel_format_mask;       /* 0x485248 — computed: g_bshift << 1 */
}

/* Globals used by Town_CheckOccupied/Town_CheckOccupiedEx/Town_BlitViewport
 * below (see town/Town.cpp for the same globals used by the file's other
 * DirectDraw-surface-locking paths). */
extern void*  g_primary_surface;       /* 0x4FD3C4 — primary DirectDraw surface */
extern char   g_surface_lost;          /* 0x4FD218 — primary surface lost flag */
extern int    g_surface_red_mask;      /* 0x485288 — red channel mask (water check) */
extern int    g_surface_blue_mask;     /* 0x485290 — blue channel mask (water check) */

extern "C" {
    /* Real def: shared/link_stubs.cpp (Section A, extern "C") — matching
     * linkage required here too, or this becomes a call-0 landmine. */
    void* DDRAW_GetDdrawErrorString(int code);                     /* 0x45BBC0 */
}

/* Forward declaration: UIPANEL_Blit is the main dispatcher (ui/UIPANEL_Surface.cpp).
 * Signature must match the real definition exactly (C++ name-mangled — no
 * extern "C" — a mismatched param type here is a call-0 landmine, as it was
 * before this fix: the previous `int** dest_surface` didn't match the real
 * `void* dest_surface`). */
extern bool __thiscall UIPANEL_Blit(
    void* renderer,
    uint32_t src_x, uint32_t src_y, int dest_x, uint32_t dest_y,
    void* dest_surface, uint32_t clip_left, uint32_t clip_top,
    int clip_right, uint32_t clip_bottom, uint32_t flags);  /* 0x42B050 */

/* ================================================================== */
/* InitTileCache                                                       */
/* Address: 0x42B9C0                                                   */
/*                                                                     */
/* Initializes a 16-bit destination surface from 8-bit source tile     */
/* data through palette lookup. Copies each source byte, remaps it     */
/* through this->palette_ptr[source_byte], and writes the resulting        */
/* uint16_t to the destination surface.                                */
/*                                                                     */
/* Called by: UIPANEL_Blit (dispatch for flags=0x01, 0x03)             */
/* ================================================================== */
bool UIPANEL_Surface::InitTileCache(
    int src_x, int src_y, int dest_x, int dest_y,
    uint8_t* dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    /* Guard: must have pixel data and palette loaded */
    if (this->pixels == nullptr || this->palette_ptr == nullptr) { /* +0x18, +0x14 */
        return false;
    }

    /* Compute clipped tile dimensions */
    uint32_t tile_width   = static_cast<uint32_t>(clip_right - clip_left) & 0xFFFF;
    uint32_t half_pitch   = (dest_pitch >> 1) & 0xFFFF;              /* pitch in uint16_t units */
    uint32_t tile_height  = static_cast<uint32_t>(clip_bottom - clip_top) & 0xFFFF;

    /* Destination pointer: start of the clipped area on 16-bit surface */
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        dest_surface + (half_pitch * static_cast<uint32_t>(src_y) +
                        static_cast<uint32_t>(src_x)) * 2);

    /* Source pointer: start of clipped area in 8-bit tile cache */
    uint8_t* src  = this->pixels + clip_top * this->width + clip_left;   /* +0x18, +0x08 */

    /* Row strides (bytes to advance after each row) */
    int32_t src_stride  = this->width - static_cast<int32_t>(tile_width); /* +0x08 */
    int32_t dest_stride = static_cast<int32_t>(half_pitch - tile_width) * 2;

    /* Source row end marker */
    uint8_t* src_row_end = src + (tile_height - 1) * this->width + tile_width;  /* +0x08 */
    uint8_t* src_end     = src_row_end + (this->width - tile_width);             /* +0x08 */
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
            *dest = this->palette_ptr[index];          /* +0x14 */
            src++;
            dest++;
        }

        /* Advance to next row */
        src  += src_stride;
        dest  = reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(dest) + dest_stride);
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
void UIPANEL_Surface::BlitElement(
    uint32_t src_x, uint32_t src_y, int dest_x, uint32_t dest_y,
    void* element, uint32_t clip_left, uint32_t clip_top,
    int clip_right, uint32_t clip_bottom, uint32_t flags)
{
    /* Extract the DirectDraw surface pointer from element+0x1C */
    int** ddraw_surface = *reinterpret_cast<int***>(
        reinterpret_cast<uint8_t*>(element) + 0x1C);

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
/*   3. Look up color from this->palette_ptr[source_byte]
/*   4. Write remapped color to destination
/*
/* Since palette[0] is overwritten with the destination pixel before
/* the lookup, source index 0 produces the original destination value
/* (transparent pass-through). This avoids a conditional branch per pixel.
/*
/* Called by: UIPANEL_Blit (flags=0x00, and default for unknown flags)
/* ================================================================== */
bool UIPANEL_Surface::DrawTile(
    int src_x, int src_y, int dest_x, int dest_y,
    int dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    uint8_t* pixels  = this->pixels;          /* +0x18 */
    uint16_t* pal    = this->palette_ptr;          /* +0x14 */

    /* Guard: must have pixel data and palette */
    if (pixels == nullptr || pal == nullptr) {
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
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        static_cast<uintptr_t>(dest_surface) + (half_pitch * src_y + src_x) * 2);

    /* Source: 8-bit pointer at (clip_left, clip_top) in tile cache */
    uint8_t* src = pixels + clip_top * this->width + clip_left;   /* +0x08 */

    int32_t src_advance  = this->width - tile_width;               /* +0x08 */
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
        dest  = reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(dest) + dest_advance);
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
bool UIPANEL_Surface::FlushTileCache(
    int src_x, int src_y, int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, uint32_t clip_bottom)
{
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Destination pointer for 16-bit 2x2 blocks */
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        static_cast<uintptr_t>(dest_surface) +
        (half_pitch * static_cast<uint32_t>(src_y) +
         static_cast<uint32_t>(src_x)) * 2);

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
                    uint16_t color = *(this->palette_ptr + index);  /* +0x14 */

                    dest[0] = color;           /* top-left */
                    dest[1] = color;           /* top-right */
                    dest[half_pitch] = color;  /* bottom-left */
                    dest[half_pitch + 1] = color; /* bottom-right */
                }

                dest += 2;                     /* advance 2 pixels (one source -> 2 output) */
                col++;
            } while ((col & 0xFFFF) < static_cast<uint32_t>(clip_area_width));
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
bool UIPANEL_Surface::DrawCachedTile(
    uint32_t src_x, int src_y, int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, uint32_t clip_bottom)
{
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Destination pointer for 16-bit 2x2 blocks */
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        static_cast<uintptr_t>(dest_surface) +
        (half_pitch * src_y + static_cast<int>(src_x)) * 2);

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
                uint16_t color = *(this->palette_ptr + index);                     /* +0x14 */

                /* Always writes 2x2 block (no transparent skip) */
                dest[0] = color;
                dest[1] = color;
                dest[half_pitch] = color;
                dest[half_pitch + 1] = color;

                dest += 2;
                col++;
            } while ((col & 0xFFFF) < static_cast<uint32_t>(clip_area_width));
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
bool UIPANEL_Surface::DrawTileEx(
    int src_x, int src_y, int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    uint32_t clip_left, uint32_t clip_top,
    int clip_right, int clip_bottom)
{
    int32_t tile_width  = (clip_right - static_cast<int>(clip_left)) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;
    int32_t clip_area_width = (static_cast<int>(clip_left) + tile_width) & 0xFFFF;

    /* Destination pointer: each output pixel is 3 wide on dest */
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        static_cast<uintptr_t>(dest_surface) +
        (half_pitch * static_cast<uint32_t>(src_y) +
         static_cast<uint32_t>(src_x)) * 2);

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
                    uint16_t color = *(this->palette_ptr + index);  /* +0x14 */

                    /* Write 3x2 block */
                    dest[0] = color;
                    dest[1] = color;
                    dest[2] = color;

                    uint16_t* row1 = &dest[half_pitch - 2];
                    row1[0] = color;
                    row1[1] = color;
                    row1[2] = color;

                    uint16_t* row2 = reinterpret_cast<uint16_t*>(
                        reinterpret_cast<uint8_t*>(&dest[half_pitch - 2]) +
                        (half_pitch - 2) * 2 + 2);
                    row2[0] = color;
                    row2[1] = color;
                    row2[2] = color;
                }

                dest = dest_row_start;
                dest_row_start += 3;
                col++;
            } while ((col & 0xFFFF) < static_cast<uint32_t>(clip_area_width));
        }

        row++;
        /* Advance dest by (half_pitch - tile_width) * 3 uint16_t units */
        dest = dest + (half_pitch - tile_width) * 3;

    } while ((row & 0xFFFF) < static_cast<uint32_t>(out_height));

    return true;
}

/* ================================================================== */
/* DrawTileLine                                                        */
/* Address: 0x42BEC0                                                   */
/*                                                                     */
/* Draws a tile with line-effect alpha blending. Uses pixel format     */
/* globals to compute per-channel bit masks for half-bright blending.  */
/*                                                                     */
/* Saves destination pixels to this->palette_ptr (used as a line buffer),  */
/* then blends source with destination by averaging (>> 1) each       */
/* color channel separately. This creates a semi-transparent overlay   */
/* effect used for selection/highlight lines on the isometric grid.    */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x400, 0x402)                        */
/* ================================================================== */
bool UIPANEL_Surface::DrawTileLine(
    int src_x, int src_y, int dest_x, int dest_y,
    int dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Destination 16-bit buffer pointer */
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        static_cast<uintptr_t>(dest_surface) + (half_pitch * src_y + src_x) * 2);

    /* Source position in tile cache */
    int src_pos = clip_top * this->width + clip_left;          /* +0x08 */

    /* Tile height */
    int32_t tile_height = (clip_bottom - clip_top) & 0xFFFF;

    /* Compute alpha blending masks from pixel format globals */
    uint16_t channel_mask1;   /* first color channel mask */
    uint16_t channel_mask2;   /* second color channel mask */
    uint16_t blend_mask;      /* inverse of (channel_mask1 | channel_mask2 | 1) */

    if (g_surface_bpp == 0x235) {
        /* 565 format: channel1 R[4..0] (DEC 1) -> shift by (channel2-1) */
        channel_mask1 = static_cast<uint16_t>(1 << ((g_surface_channel2 - 1) & 0x1F));
        channel_mask2 = static_cast<uint16_t>(1 << (g_surface_channel1 & 0x1F));
    } else {
        /* 555 format: standard */
        channel_mask1 = static_cast<uint16_t>(1 << (g_surface_channel2 & 0x1F));
        channel_mask2 = static_cast<uint16_t>(1 << (g_surface_channel1 & 0x1F));
    }

    blend_mask = static_cast<uint16_t>(~(channel_mask1 | channel_mask2 | 1));

    /* End of destination for this operation */
    uint16_t* dest_end = dest + (tile_height - 1) * half_pitch + tile_width;
    int32_t src_stride_advance = this->width - tile_width;      /* +0x08 */
    int32_t dest_stride = half_pitch - tile_width;

    /* Iterate rows */
    while (dest < dest_end) {
        uint16_t* dest_row_end = dest + tile_width;

        /* Iterate columns */
        while (dest < dest_row_end) {
            /* Save destination pixel to palette (used as line/color buffer) */
            this->palette_ptr[0] = *dest;                           /* +0x14 */

            /* Read source byte and check for transparency */
            uint16_t index = *(this->pixels + src_pos);         /* +0x18 */
            if (index != 0) {
                /* Shadow: save half-bright version of original dest pixel */
                this->palette_ptr[1] = (*dest >> 1) & g_surface_bshift;  /* +0x14 */

                /* Blend: source color & palette index, channel-averaged with dest */
                uint16_t src_color = *(this->palette_ptr + index);  /* +0x14 */
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
bool UIPANEL_Surface::DrawTiles16bpp_Strided(
    int src_x, int src_y, int dest_x, int dest_y,
    int dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Destination pointer */
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        static_cast<uintptr_t>(dest_surface) + (half_pitch * src_y + src_x) * 2);
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t tile_height = (clip_bottom - clip_top) & 0xFFFF;

    /* Source position in tile cache */
    uint8_t* src = this->pixels + clip_top * this->width + clip_left;  /* +0x18, +0x08 */

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
            this->palette_ptr[0] = saved;                               /* +0x14 */

            /* Compute half-bright shadow and store in palette[1] */
            this->palette_ptr[1] = static_cast<uint16_t>(
                (saved & g_pixel_format_mask) >> 1);  /* +0x14 */

            /* Read source byte and write remapped color */
            uint8_t index = *src++;
            *dest = this->palette_ptr[index];                           /* +0x14 */
            dest++;
        }

        /* Advance to next row */
        dest  = reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(dest) + half_pitch * 2);
        src  += this->width;                                       /* +0x08 */
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
bool UIPANEL_Surface::DrawTiles16bpp_Reversed(
    int src_x, int src_y, int dest_x, int dest_y,
    int dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    int32_t tile_width  = (clip_right - clip_left) & 0xFFFF;
    int32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;
    int32_t tile_height = (clip_bottom - clip_top) & 0xFFFF;

    /* Destination pointer */
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        static_cast<uintptr_t>(dest_surface) + (half_pitch * src_y + src_x) * 2);

    /* Source: starts at RIGHT edge of clip rect and goes left */
    uint8_t* src = this->pixels + clip_top * this->width + clip_right;  /* +0x18, +0x08 */

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
            this->palette_ptr[0] = saved;                               /* +0x14 */

            /* Half-bright shadow in palette[1] */
            this->palette_ptr[1] = g_surface_bshift & (saved >> 1);     /* +0x14 */

            /* Read source byte (right-to-left) and write remapped color */
            uint8_t index = *src;
            src--;
            *dest = this->palette_ptr[index];                           /* +0x14 */
            dest++;
        }

        /* Advance to next row */
        src  = src + this->width + tile_width;                     /* +0x08 */
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
bool UIPANEL_Surface::DrawTiles16bpp_Checker(
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
    int32_t tile_height =
        (clip_bottom - static_cast<int>(clip_top)) & 0xFFFF;

    /* Destination pointer */
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        static_cast<uintptr_t>(dest_surface) +
        (half_pitch * static_cast<uint32_t>(src_y) +
         static_cast<uint32_t>(src_x)) * 2);
    int src_pos = static_cast<int>(clip_top) * this->width + clip_left; /* +0x08 */

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
                this->palette_ptr[0] = *col_dest;                       /* +0x14 */
                this->palette_ptr[1] = *col_dest >> 1 & g_surface_bshift; /* +0x14 */

                /* Write remapped source color */
                *col_dest = this->palette_ptr[index];                   /* +0x14 */

                col_dest++;
                src_pos++;
                col++;
            } while ((col & 0xFFFF) < static_cast<uint32_t>(tile_width));
        }

        /* Advance by 2 rows (checkerboard skip) */
        dest   = dest + (half_pitch * 2 - tile_width);
        src_pos = src_pos + (this->width * 2 - tile_width);        /* +0x08 */
        row += 2;

    } while ((row & 0xFFFF) < static_cast<uint32_t>(tile_height));

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
bool UIPANEL_Surface::DrawTiles16bpp_Staggered(
    int src_x, int src_y, int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    int clip_left, uint32_t clip_top,
    int clip_right, int clip_bottom)
{
    uint32_t half_pitch = (dest_pitch >> 1) & 0xFFFF;
    uint32_t tile_width  = static_cast<uint32_t>(clip_right - clip_left) & 0xFFFF;
    uint32_t tile_height = static_cast<uint32_t>(
        clip_bottom - static_cast<int>(clip_top)) & 0xFFFF;

    /* Destination 16-bit pointer at (src_x, src_y) */
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        static_cast<uintptr_t>(dest_surface) +
        (half_pitch * static_cast<uint32_t>(src_y) +
         static_cast<uint32_t>(src_x)) * 2);

    /* Source position in 8-bit tile cache */
    int src_pos = static_cast<int>(clip_top) * this->width + clip_left; /* +0x08 */

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
                this->palette_ptr[0] = *dest_row;                       /* +0x14 */

                /* Compute dimmed (half-bright) pixel in palette[1] */
                this->palette_ptr[1] = (*dest_row >> 1) & g_surface_bshift;  /* +0x14 */

                /* Write source color (index remapped through palette) */
                *dest_row = this->palette_ptr[index];                   /* +0x14 */
            }

            toggle = !toggle;
            dest_row++;
            src_pos++;
        }

        /* Advance to next row */
        dest     = reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(dest) + half_pitch * 2);
        src_pos += this->width - static_cast<int>(tile_width);    /* +0x08 */
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
/* Called by: the clock-digit animation helper (0x447400) -- clock digit sprites     */
/* ================================================================== */
void UIPANEL_Surface::CopyTiles8bpp_Transparent(
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
    int src_pos = src_top * this->width + src_left;                /* +0x08 */

    /* Iterate rows */
    for (uint32_t row = 0; (row & 0xFFFF) < static_cast<uint32_t>(tile_height); row++) {
        for (uint32_t col = 0; (col & 0xFFFF) < static_cast<uint32_t>(tile_width); col++) {
            uint8_t pixel = *(this->pixels + src_pos);              /* +0x18 */

            if (pixel != 0) {
                *dest = pixel;
            }

            dest++;
            src_pos++;
        }

        /* Advance to next row */
        dest    += dest_pitch - tile_width;
        src_pos += this->width - tile_width;                       /* +0x08 */
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
void UIPANEL_Surface::CopyTiles8bpp_Direct(
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
    int src_pos = src_top * this->width + src_left;                /* +0x08 */

    /* Iterate rows */
    for (uint32_t row = 0; (row & 0xFFFF) < static_cast<uint32_t>(tile_height); row++) {
        for (uint32_t col = 0; (col & 0xFFFF) < static_cast<uint32_t>(tile_width); col++) {
            uint8_t pixel = *(this->pixels + src_pos);              /* +0x18 */
            *dest = pixel;

            dest++;
            src_pos++;
        }

        /* Advance to next row */
        dest    += dest_pitch - tile_width;
        src_pos += this->width - tile_width;                       /* +0x08 */
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
/* Parameter notes: dest_x and dest_y (the 3rd and 4th arguments) are present   */
/* in the UIPANEL_Blit dispatch calling convention but are NOT used    */
/* by the implementation. The destination position is determined from  */
/* src_x and src_y.                                                    */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x20, dispatch @ 0x42B6C9)          */
/* ================================================================== */
bool UIPANEL_Surface::BlitTileSurface(
    int src_x, int src_y,
    int dest_x, int dest_y,
    uint32_t dest_surface, uint32_t dest_pitch,
    int clip_left, int clip_top,
    int clip_right, int clip_bottom)
{
    /* Compute clipped region dimensions */
    uint32_t tile_width  = static_cast<uint32_t>(clip_right - clip_left) & 0xFFFF;
    uint32_t tile_height = static_cast<uint32_t>(clip_bottom - clip_top) & 0xFFFF;
    uint32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Destination pointer: 16-bit at (src_x, src_y) on surface       */
    uint16_t* dest = reinterpret_cast<uint16_t*>(
        static_cast<uintptr_t>(dest_surface) +
        (half_pitch * static_cast<uint32_t>(src_y) +
         static_cast<uint32_t>(src_x)) * 2);

    /* Source position: RIGHT EDGE of clip rect, read right-to-left   */
    /* Start at clip_top * stride + (clip_right - 1)                  */
    int src_pos = clip_top * this->width + clip_right - 1;          /* +0x08 */

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
                *dest = this->palette_ptr[index];                        /* +0x14 */
            }

            dest++;         /* advance destination LEFT-to-RIGHT      */
            src_pos--;       /* advance source RIGHT-to-LEFT          */
        }

        /* Advance to next row */
        /* Dest: skip to next row = (half_pitch - tile_width) * 2 bytes */
        dest = reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(dest) +
            (half_pitch - tile_width) * 2);

        /* Source: advance to right edge of next row                  */
        src_pos += this->width + tile_width;                        /* +0x08 */
    }

    return true;
}

/* ================================================================== */
/* CalcScrollRect / CalcScrollRect_Reversed                            */
/* Addresses: 0x42C590 (forward) / 0x42C700 (reversed)                 */
/*                                                                     */
/* RESOLVED (was a deferred stub — see PROGRESS.md history). Ghidra's   */
/* own signature guess for these functions (2 stack args) was wrong;   */
/* both take 4 stack args, matching their `RET 0x10`. Re-derived from   */
/* raw disassembly of both functions AND their sole caller (UIPANEL_   */
/* Blit @0x42B050, dispatch around 0x42B0EF-0x42B111 for the forward   */
/* call, mirrored for the reversed call): the caller pushes, in        */
/* argument order, `&param_1` (a RECT-shaped view over its own         */
/* src_x/src_y/dest_x/dest_y stack slots), `param_5` (dest_surface),   */
/* `&param_6` (a second RECT-shaped view over clip_left/clip_top/      */
/* clip_right/clip_bottom), and `param_10` (its own flags value).      */
/*                                                                     */
/* `flags` is genuinely unread by either function body — confirmed by  */
/* enumerating every ESP-relative read in both disassemblies and       */
/* finding none at the stack offset corresponding to the 4th argument. */
/* Kept in the signature for original-ABI fidelity (the sole caller    */
/* pushes it and the callee's RET 0x10 pops it); unused internally.    */
/*                                                                     */
/* Both functions clip a "scroll view" rectangle — `clip_rect`'s own   */
/* width/height repositioned to `rect`'s origin, compensating for a    */
/* negative `clip_rect` origin — against the surface's bounds (queried */
/* from `surface_obj->GetSurfaceDesc()`, vtable+0x58 in the original),  */
/* then write the result back into BOTH `rect` (surface-local          */
/* coordinates) and `clip_rect` (shifted destination coordinates).     */
/*                                                                     */
/* They are NOT simple mirror images:                                  */
/*   - CalcScrollRect (forward, this->mode==1 / DDraw surface) checks   */
/*     `clip_rect` only for emptiness against itself, clips solely      */
/*     against the surface's *reported* bounds, and its shared          */
/*     epilogue does an unconditional `XOR AL,AL` — it ALWAYS returns   */
/*     false, on both the success and early-bail paths. The sole caller */
/*     never inspects the return value, so this apparent original bug   */
/*     has no observable effect (documented, not "fixed"). It also      */
/*     computes a `{0,0,this->width,this->height}` panel-bounds rect    */
/*     via SetRect that is written but never read again anywhere in the */
/*     function — dead code in the original, omitted here for the same  */
/*     reason CLAUDE.md permits simplifying proven-equivalent assembly   */
/*     artifacts.                                                       */
/*   - CalcScrollRect_Reversed (this->mode==0 / software pixel buffer)  */
/*     additionally clips `clip_rect` against the panel's *logical*     */
/*     bounds (`{0,0,this->width,this->height}`) BEFORE clipping        */
/*     against the surface's reported bounds, and returns true on       */
/*     success / false only on its two early-empty-rect bail paths.     */
/* ================================================================== */
extern void __stdcall SetRect(RECT* lprc, int left, int top, int right, int bottom);  /* IAT 0x477384 */
extern int  __stdcall IntersectRect(RECT* dst, const RECT* src1, const RECT* src2);   /* IAT 0x47726C */
extern int  __stdcall IsRectEmpty(const RECT* rect);                                  /* IAT 0x477268 */

bool UIPANEL_Surface::CalcScrollRect(RECT* rect, IDirectDrawSurface4* surface_obj,
                                     RECT* clip_rect, uint32_t /*flags*/)
{
    DDSURFACEDESC desc = {};
    desc.dwSize = sizeof(desc);
    surface_obj->GetSurfaceDesc(&desc);   /* vtable+0x58 in the original */

    RECT surface_bounds;
    SetRect(&surface_bounds, 0, 0, static_cast<int>(desc.dwWidth), static_cast<int>(desc.dwHeight));

    /* Shift amount compensating for a negative clip_rect origin. */
    int adj_x = rect->left - ((clip_rect->left < 0) ? clip_rect->left : 0);
    int adj_y = rect->top  - ((clip_rect->top  < 0) ? clip_rect->top  : 0);

    /* Validate clip_rect (empty-rect check via self-intersect, matching
     * the original's IntersectRect(tmp, clip_rect, clip_rect) idiom). */
    RECT clip_norm;
    IntersectRect(&clip_norm, clip_rect, clip_rect);
    if (IsRectEmpty(&clip_norm)) {
        return false;
    }

    /* View rect: same size as clip_norm, repositioned to (adj_x, adj_y). */
    RECT view_rect;
    view_rect.left   = adj_x;
    view_rect.top    = adj_y;
    view_rect.right  = adj_x + (clip_norm.right  - clip_norm.left);
    view_rect.bottom = adj_y + (clip_norm.bottom - clip_norm.top);

    RECT final_rect;
    IntersectRect(&final_rect, &view_rect, &surface_bounds);
    if (IsRectEmpty(&final_rect)) {
        return false;
    }

    int clamp_x = (adj_x > 0) ? 0 : adj_x;
    int clamp_y = (adj_y > 0) ? 0 : adj_y;

    SetRect(rect, final_rect.left, final_rect.top, final_rect.right, final_rect.bottom);

    int dst_x = clip_norm.left - clamp_x;
    int dst_y = clip_norm.top  - clamp_y;
    SetRect(clip_rect, dst_x, dst_y,
            dst_x + (final_rect.right  - final_rect.left),
            dst_y + (final_rect.bottom - final_rect.top));

    /* BUG (original): the shared epilogue always returns false, on every
     * path including this success path. The sole caller (UIPANEL_Blit)
     * never checks the return value, so this has no observable effect. */
    return false;
}

bool UIPANEL_Surface::CalcScrollRect_Reversed(RECT* rect, IDirectDrawSurface4* surface_obj,
                                              RECT* clip_rect, uint32_t /*flags*/)
{
    DDSURFACEDESC desc = {};
    desc.dwSize = sizeof(desc);
    surface_obj->GetSurfaceDesc(&desc);   /* vtable+0x58 in the original */

    RECT surface_bounds;
    SetRect(&surface_bounds, 0, 0, static_cast<int>(desc.dwWidth), static_cast<int>(desc.dwHeight));

    RECT panel_bounds;
    SetRect(&panel_bounds, 0, 0, this->width, this->height);   /* +0x08 / +0x0C */

    /* Shift amount compensating for a negative clip_rect origin. */
    int adj_x = rect->left - ((clip_rect->left < 0) ? clip_rect->left : 0);
    int adj_y = rect->top  - ((clip_rect->top  < 0) ? clip_rect->top  : 0);

    /* Unlike the forward version, clip_rect is first clipped against the
     * panel's logical bounds (not just self-validated). */
    RECT clip_vs_panel;
    IntersectRect(&clip_vs_panel, &panel_bounds, clip_rect);
    if (IsRectEmpty(&clip_vs_panel)) {
        return false;
    }

    /* View rect: same size as clip_vs_panel, repositioned to (adj_x, adj_y). */
    RECT view_rect;
    view_rect.left   = adj_x;
    view_rect.top    = adj_y;
    view_rect.right  = adj_x + (clip_vs_panel.right  - clip_vs_panel.left);
    view_rect.bottom = adj_y + (clip_vs_panel.bottom - clip_vs_panel.top);

    RECT final_rect;
    IntersectRect(&final_rect, &view_rect, &surface_bounds);
    if (IsRectEmpty(&final_rect)) {
        return false;
    }

    int clamp_x = (adj_x > 0) ? 0 : adj_x;
    int clamp_y = (adj_y > 0) ? 0 : adj_y;

    SetRect(rect, final_rect.left, final_rect.top, final_rect.right, final_rect.bottom);

    int dst_x = clip_vs_panel.left - clamp_x;
    int dst_y = clip_vs_panel.top  - clamp_y;
    SetRect(clip_rect, dst_x, dst_y,
            dst_x + (final_rect.right  - final_rect.left),
            dst_y + (final_rect.bottom - final_rect.top));

    return true;
}

/* ================================================================== */
/* Town_CheckOccupied / Town_CheckOccupiedEx / Town_BlitViewport       */
/* Addresses: 0x42C950 / 0x42C9F0 / 0x42CB10                           */
/*                                                                     */
/* MOVED HERE (2026-08-08 town-cpp-strict2 session) from a previous,   */
/* mis-scoped transcription as `Town::check_occupied` / `_ex` /        */
/* `blit_viewport` member functions. Ghidra evidence for the real      */
/* scope:                                                              */
/*   - Ghidra names them "Town_CheckOccupied"/"Town_BlitViewport" as   */
/*     FREE functions (not Town:: methods) — get_xrefs_to confirms     */
/*     their only callers are BuildingMgr::InvalidateRects/            */
/*     BlitOverlaps (0x435020/0x435200) and World::ProcessEvents       */
/*     (0x44E3F0), none of which are Town methods.                     */
/*   - Every one of those call sites passes                            */
/*     `*(void**)(*(int*)(entity+0x40) + 0x10)` as the receiver — the   */
/*     RESDATA-embedded "ui_panel" alias documented in shared/types.h  */
/*     (RESDATA::flags, +0x10) — i.e. a UIPANEL_Surface*, never a Town*.*/
/*   - No caller anywhere in this codebase ever called                */
/*     Town::check_occupied/blit_viewport as a method; they were fully */
/*     dead code, while game/BuildingMgr.cpp and game/World.cpp already*/
/*     declared+called the correctly-shaped free functions against a  */
/*     symbol that didn't exist anywhere — a genuine call-0 landmine.  */
/* This is a DIFFERENT resolution than CalcScrollRect above: that      */
/* pair's Ghidra decompile dereferences an unresolved `ptStack_4`       */
/* distinct from the tracked RECT* (evidence of a genuinely uncertain   */
/* stack-argument count), so it stays a deferred stub. CheckOccupied/   */
/* CheckOccupiedEx/BlitViewport have no such ambiguity — clean          */
/* `this+4`/`this+8`/`this+0x18`/`this+0x1C` field reads throughout,    */
/* matching UIPANEL_Surface::mode/width/pixels/ddraw_surf exactly.      */
/* ================================================================== */

namespace {
/* Persistent primary-surface lock state — the original keeps a global
 * DDSURFACEDESC at 0x4FD19C that persists across calls; reproduced as
 * file-static storage so a failed re-lock still scans the previous
 * lock's data exactly like the original global. Moved here (2026-08-08)
 * from town/Town.cpp along with Town_CheckOccupiedEx, its only user. */
DDSURFACEDESC g_primary_surface_desc = {};
} // namespace

uint8_t Town_CheckOccupied(UIPANEL_Surface* self, int x1, int y1, int x2, int y2)
{
    if (self->mode != 0) {                                        /* +0x04 */
        return Town_CheckOccupiedEx(x1, y1, x2, y2);
    }

    int stride = self->width;                                     /* +0x08 */
    uint8_t* buf = self->pixels;                                   /* +0x18 */
    int width  = x2 - x1;
    int height = y2 - y1;

    uint8_t* row = buf + static_cast<ptrdiff_t>(y1) * stride + x1;
    for (int row_idx = 0; row_idx < height; row_idx++) {
        for (int col = 0; col < width; col++) {
            if (row[col] != 0) {
                return 1;
            }
        }
        row += stride;
    }
    return 0;
}

uint8_t Town_CheckOccupiedEx(int x1, int y1, int x2, int y2)
{
    uint8_t result = 0;
    IDirectDrawSurface4* primary = static_cast<IDirectDrawSurface4*>(g_primary_surface);

#ifndef _WIN32
    /* g_primary_surface is a real virtual IDirectDrawSurface4* now (see
     * platform/ddraw_interfaces.h) but stays null until a later shim pass
     * wires the host DirectDraw device — dispatching Lock/Unlock through a
     * null pointer would segfault on a null vtable read (and the pixel scan
     * below would separately segfault reading through a never-populated
     * g_primary_surface_desc.lpSurface), instead of the old plain-struct
     * stub's silent no-op. This check is purely a host affordance: once
     * that device exists, primary is non-null and this function runs
     * exactly as the original always did. Report "not occupied" rather
     * than crash — the conservative default for an unavailable check. */
    if (!primary) {
        std::fprintf(stderr, "[HOST] Town_CheckOccupiedEx: occupancy scan skipped "
                              "(DirectDraw device not wired on host)\n");
        return 0;
    }
#endif

    if (g_surface_lost == 0) {
        /* NOTE: the binary sets the flag when Lock SUCCEEDS (returns 0)
         * — "lost" is actually the locked state here (0x42CA33..0x42CA37). */
        DDSURFACEDESC& desc = g_primary_surface_desc;
        desc = DDSURFACEDESC();
        desc.dwSize = 0x7C;

        /* Lock(this, NULL, &desc, 0, 0) — COM slot 25 (byte 0x64). */
        if (primary->Lock(nullptr, &desc, 0, nullptr) == 0) {
            g_surface_lost = 1;
        }
    }

    uint32_t pitch = static_cast<uint32_t>(g_primary_surface_desc.lPitch);
    uint32_t height = static_cast<uint32_t>(y2 - y1) & 0xFFFF;
    uint32_t width  = static_cast<uint32_t>(x2 - x1) & 0xFFFF;

    uint16_t* pixels = reinterpret_cast<uint16_t*>(
        static_cast<uint8_t*>(g_primary_surface_desc.lpSurface) +
        ((pitch >> 1) * static_cast<uint32_t>(y1) + static_cast<uint32_t>(x1)) * 2);

    for (uint32_t row = 0; row < height; row++) {
        if (width != 0) {
            for (uint32_t col = 0; col < width; col++) {
                uint16_t pixel = pixels[col];
                int channel1 = (g_surface_red_mask & pixel) >>
                               (static_cast<uint8_t>(g_surface_channel1) & 0x1f);
                int channel2 = g_surface_blue_mask & pixel;

                if (channel1 != 0x1f && channel2 != 0x1f) {
                    result = 1;
                    break;
                }
            }
        }

        pixels += (pitch >> 1) - width;

        if (result != 0) {
            break;
        }
    }

    if (g_surface_lost != 0) {
        /* Unlock(NULL) — COM slot 32 (byte 0x80). */
        if (primary->Unlock(nullptr) == 0) {
            g_surface_lost = 0;
        }
    }

    return result;
}

uint32_t Town_BlitViewport(UIPANEL_Surface* self, int x1, int y1, int x2, int y2,
                           int x, int y)
{
    /* Outside bounds -> passable. Parameter usage matches the binary
     * (0x42CB31..0x42CB61): y1 is never read. */
    if (x < x1 || y2 < x || y < x2 || x < y) {
        return 1;
    }

    if (self->mode != 1) {                                        /* +0x04 */
        uint8_t* buf = self->pixels;                               /* +0x18 */
        int stride = self->width;                                  /* +0x08 */
        uint8_t val = buf[static_cast<ptrdiff_t>(stride) * y + x];
        return (val == 0) ? 1 : 0;
    }

    /* Mode 1: lock the surface at self->ddraw_surf and check the pixel. */
    IDirectDrawSurface4* surface =
        static_cast<IDirectDrawSurface4*>(self->ddraw_surf);       /* +0x1C */
    DDSURFACEDESC desc = {};
    desc.dwSize = 0x7C;

    /* Lock(this, NULL, &desc, 1, 0) — COM slot 25 (byte 0x64). */
    int lock_result = surface->Lock(nullptr, &desc, 1, nullptr);
    if (lock_result != 0) {
        DDRAW_GetDdrawErrorString(1);
        return 0;
    }

    uint32_t pitch = static_cast<uint32_t>(desc.lPitch);
    uint8_t* base = static_cast<uint8_t*>(desc.lpSurface);
    uint16_t pixel = *reinterpret_cast<uint16_t*>(
        base + static_cast<ptrdiff_t>(pitch) * y + static_cast<ptrdiff_t>(x) * 2);

    uint32_t channel1 = (static_cast<uint32_t>(g_surface_red_mask) & pixel) >>
                        (static_cast<uint8_t>(g_surface_channel1) & 0x1f);
    uint32_t channel2 = static_cast<uint32_t>(g_surface_blue_mask) & pixel;

    uint8_t result = 1;
    if (channel1 != 0x1f || channel2 != 0x1f) {
        result = 0;
    }

    /* Unlock(NULL) — COM slot 32 (byte 0x80). */
    surface->Unlock(nullptr);

    return result;
}
