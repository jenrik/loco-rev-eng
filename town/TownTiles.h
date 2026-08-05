// Status: INTEGRATED
/**
 * TownTiles.h — Town tile rendering engine (tile cache surface primitives)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The tile rendering engine operates on a tile cache surface context
 * (TownTileRenderer) that stores rendering parameters: an unused slot at
 * +0x00, the render mode at +0x04, the source stride at +0x08, the
 * palette lookup table at +0x14 and the 8-bit indexed tile pixels at
 * +0x18. All methods were validated instruction-by-instruction against
 * the Ghidra disassembly (database "locon").
 *
 * Rendering modes are dispatched from UIPANEL_Blit (0x42B050) based on
 * a flags parameter. The flags select the drawing algorithms:
 *
 *   0x00  = Town_DrawTile (base tile drawing)
 *   0x01  = Town_InitTileCache (palette-initialize tile cache)
 *   0x02  = Town_DrawTiles16bpp_Strided (standard LTR 16bpp with shadow)
 *   0x04  = Town_FlushTileCache (2x2 block-expand palette cache)
 *   0x05  = Town_DrawCachedTile (2x2 block from cache)
 *   0x0F  = (default: Town_DrawTile fallback)
 *   0x10  = Town_DrawTileEx (3x2 block expansion)
 *   0x20  = Town_BlitTileSurface (right-to-left blit)
 *   0x22  = Town_DrawTiles16bpp_Reversed (H-mirror 16bpp)
 *   0x84  = (alias for FlushTileCache)
 *   0x85  = (alias for DrawCachedTile)
 *   0x102 = Town_DrawTiles16bpp_Checker (checkerboard 16bpp)
 *   0x202 = Town_DrawTiles16bpp_Staggered (staggered 16bpp)
 *   0x400 = Town_DrawTileLine (alpha-blended tile line)
 *   0x402 = (alias for DrawTileLine)
 *
 * Town_BlitElement (0x42B960) and the two 8bpp copy functions
 * (Transparent/Direct, 0x42C330/0x42C3D0) are called from outside the
 * UIPANEL_Blit dispatch table and have slightly different semantics.
 *
 * Fields:
 *   +0x00  (unused — present but never accessed by the tile methods)
 *   +0x04  mode: render direction (0=reversed, 1=normal)
 *   +0x08  stride: source 8bpp tile surface pitch (bytes per row)
 *   +0x0C  (unknown, not accessed by these functions)
 *   +0x10  (unknown, not accessed by these functions)
 *   +0x14  palette: uint16_t[256] color lookup table pointer
 *   +0x18  pixels: uint8_t* 8-bit indexed tile pixel data
 *   +0x1C  surface_ref: (unknown, used by UIPANEL_Blit)
 *
 * Globals referenced (pixel format info):
 *   g_surface_bpp       (0x485274) — DirectDraw surface bits-per-pixel
 *   g_surface_bshift    (0x485280) — bit shift for half-bright averaging
 *   g_pixel_format_mask (0x485248) — computed bitmask (bshift << 1)
 *   g_surface_channel1  (0x485278) — red channel bit shift (10/11)
 *   g_surface_channel2  (0x48527C) — green channel bit shift (5/6)
 */

#pragma once

#include "../shared/types.h"

/* ================================================================== */
/* TownTileRenderer class                                              */
/* ================================================================== */

class TownTileRenderer {
public:
    /* ================================================================ */
    /* Fields                                                            */
    /* ================================================================ */

    /* +0x00 is an unused slot in the original object (not a C++ vtable:
     * the class has no virtual methods). Kept as an explicit member so
     * the documented binary offsets hold. */
    void*      unused_00;          // +0x00  (never accessed by tile methods)
    int32_t    mode;               // +0x04  0=reversed, 1=normal
    int32_t    stride;             // +0x08  source 8bpp surface pitch (bytes)
    int32_t    _pad_0C;            // +0x0C
    int32_t    _pad_10;            // +0x10
    uint16_t*  palette;            // +0x14  color lookup table (256 x uint16_t)
    uint8_t*   pixels;             // +0x18  8-bit indexed tile pixel data
    void*      surface_ref;        // +0x1C  DirectDraw surface reference

    /* ================================================================ */
    /* Tile rendering methods (all __thiscall, RET 0x28)                */
    /* ================================================================ */

    /**
     * InitTileCache — Initialize tile cache via palette lookup.
     * Address: 0x42B9C0
     *
     * Copies 8-bit indexed pixel data from this->pixels through this->palette
     * (palette[source_byte] = uint16_t) to a destination 16-bit DirectDraw
     * surface. Palette index 0 = transparent (passes destination pixel through).
     *
     * Called by: UIPANEL_Blit (flags=0x01, 0x03)
     *
     * @param src_x          int — source X offset in the tile cache
     * @param src_y          int — source Y offset in the tile cache
     * @param dest_x         int — destination X on the 16-bit surface (in pixels)
     * @param dest_y         int — destination Y on the 16-bit surface (in pixels)
     * @param dest_surface   uint8_t* — locked 16-bit surface base pointer
     * @param dest_pitch     uint — destination surface pitch in bytes
     * @param clip_left      int — clip rect left (source X start)
     * @param clip_top       int — clip rect top (source Y start)
     * @param clip_right     int — clip rect right (source X end)
     * @param clip_bottom    int — clip rect bottom (source Y end)
     * @return               1 on success, 0 if cache not initialized
     */
    bool InitTileCache(int src_x, int src_y, int dest_x, int dest_y,
                       uint8_t* dest_surface, uint32_t dest_pitch,
                       int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * BlitElement — Extract surface from element and blit.
     * Address: 0x42B960
     *
     * Thin wrapper that reads a DirectDraw surface pointer from an element
     * struct at element+0x1C and calls UIPANEL_Blit with it. Used by
     * EditWindow_render and Cursor_InitBackground for UI element rendering.
     *
     * Called by: EditWindow_render, Cursor_InitBackground, CGWND_* functions
     *
     * @param src_x          uint — source X in element
     * @param src_y          uint — source Y in element
     * @param dest_x         int — destination X
     * @param dest_y         uint — destination Y
     * @param element        void* — element struct (ddraw_surface at +0x1C)
     * @param clip_left      uint — clip rect left
     * @param clip_top       uint — clip rect top
     * @param clip_right     int — clip rect right
     * @param clip_bottom    uint — clip rect bottom
     * @param flags          uint — blit flags dispatched to UIPANEL_Blit
     */
    void BlitElement(uint32_t src_x, uint32_t src_y, int dest_x, uint32_t dest_y,
                     void* element, uint32_t clip_left, uint32_t clip_top,
                     int clip_right, uint32_t clip_bottom, uint32_t flags);

    /**
     * DrawTile — Draw an 8-bit indexed tile with palette remapping.
     * Address: 0x42BA90
     *
     * Core tile rendering function. Before writing each pixel, saves the
     * current destination pixel to palette[0] (transparent slot), then
     * reads the source byte and writes the remapped color to the destination.
     * Palette index 0 acts as transparent (pass-through) because palette[0]
     * is overwritten with the original destination pixel first.
     *
     * Called by: UIPANEL_Blit (flags=0x00, default/0x0F)
     *
     * @param src_x          int — source X in tile cache
     * @param src_y          int — source Y in tile cache
     * @param dest_x         int — destination X on 16-bit surface
     * @param dest_y         int — destination Y on 16-bit surface
     * @param dest_surface   int — locked 16-bit surface base pointer
     * @param dest_pitch     uint — destination surface pitch in bytes
     * @param clip_left      int — clip rect left
     * @param clip_top       int — clip rect top
     * @param clip_right     int — clip rect right
     * @param clip_bottom    int — clip rect bottom
     * @return               1 on success, 0 if cache not initialized or empty clip
     */
    bool DrawTile(int src_x, int src_y, int dest_x, int dest_y,
                  int dest_surface, uint32_t dest_pitch,
                  int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * FlushTileCache — Flush tile cache by expanding each pixel to 2x2 block.
     * Address: 0x42BB90
     *
     * Reads each source byte once and writes a 2x2 block of identical pixels
     * to the 16-bit destination surface. Palette index 0 = skip (keeps
     * original destination pixel). The 2x2 block writes: pixel, pixel+1,
     * pixel+stride, pixel+stride+1.
     *
     * Called by: UIPANEL_Blit (flags=0x04, 0x84)
     *
     * @param src_x          int — source X
     * @param src_y          int — source Y
     * @param dest_x         int — destination X
     * @param dest_y         int — destination Y
     * @param dest_surface   uint — destination surface base pointer
     * @param dest_pitch     uint — destination pitch (bytes)
     * @param clip_left      int — clip left
     * @param clip_top       int — clip top
     * @param clip_right     int — clip right
     * @param clip_bottom    uint — clip bottom
     * @return               1 on success
     */
    bool FlushTileCache(int src_x, int src_y, int dest_x, int dest_y,
                        uint32_t dest_surface, uint32_t dest_pitch,
                        int clip_left, int clip_top, int clip_right, uint32_t clip_bottom);

    /**
     * DrawCachedTile — Draw tile from cache with 2x2 expansion.
     * Address: 0x42BC80
     *
     * Same 2x2 block expansion as FlushTileCache but reads from the tile
     * cache buffer. Each source byte produces a 2x2 color block on the
     * 16-bit destination. Palette index 0 is NOT skipped (always writes).
     *
     * Called by: UIPANEL_Blit (flags=0x05, 0x85)
     *
     * @param src_x          uint — source X
     * @param src_y          int — source Y
     * @param dest_x         int — destination X
     * @param dest_y         int — destination Y
     * @param dest_surface   uint — destination surface base pointer
     * @param dest_pitch     uint — destination pitch (bytes)
     * @param clip_left      int — clip left
     * @param clip_top       int — clip top
     * @param clip_right     int — clip right
     * @param clip_bottom    uint — clip bottom
     * @return               1 on success
     */
    bool DrawCachedTile(uint32_t src_x, int src_y, int dest_x, int dest_y,
                        uint32_t dest_surface, uint32_t dest_pitch,
                        int clip_left, int clip_top, int clip_right, uint32_t clip_bottom);

    /**
     * DrawTileEx — Extended tile drawing with 3x2 pixel expansion.
     * Address: 0x42BD70
     *
     * Each source byte produces a 3x2 block on the 16-bit destination.
     * Palette index 0 = skip. The 3x2 block is laid out as:
     *   [pixel] [pixel] [pixel]    — row 0 (6 bytes)
     *   [pixel] [pixel] [pixel]    — row 1 (6 bytes)
     * Pattern used for isometric tile edge rendering.
     *
     * Called by: UIPANEL_Blit (flags=0x0F < 0x10, i.e. values 0x10-0x1F)
     *
     * @param src_x          int — source X
     * @param src_y          int — source Y
     * @param dest_x         int — destination X
     * @param dest_y         int — destination Y
     * @param dest_surface   uint — destination surface base pointer
     * @param dest_pitch     uint — destination pitch (bytes)
     * @param clip_left      uint — clip left
     * @param clip_top       uint — clip top
     * @param clip_right     int — clip right
     * @param clip_bottom    int — clip bottom
     * @return               1 on success
     */
    bool DrawTileEx(int src_x, int src_y, int dest_x, int dest_y,
                    uint32_t dest_surface, uint32_t dest_pitch,
                    uint32_t clip_left, uint32_t clip_top,
                    int clip_right, int clip_bottom);

    /**
     * DrawTileLine — Alpha-blended tile line drawing.
     * Address: 0x42BEC0
     *
     * Draws a tile with alpha blending for line effects. Saves destination
     * pixels to this->palette (used as a line buffer), then blends source
     * with destination using bit-shifted color averaging. Uses pixel format
     * globals (g_surface_bpp, g_surface_bshift, g_surface_channel1/2) to
     * compute per-channel masks for half-bright alpha blending.
     *
     * Called by: UIPANEL_Blit (flags=0x400, 0x402)
     *
     * @param src_x          int — source X
     * @param src_y          int — source Y
     * @param dest_x         int — destination X
     * @param dest_y         int — destination Y
     * @param dest_surface   int — destination surface base pointer
     * @param dest_pitch     uint — destination pitch (bytes)
     * @param clip_left      int — clip left
     * @param clip_top       int — clip top
     * @param clip_right     int — clip right
     * @param clip_bottom    int — clip bottom
     * @return               1 on success
     */
    bool DrawTileLine(int src_x, int src_y, int dest_x, int dest_y,
                      int dest_surface, uint32_t dest_pitch,
                      int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * DrawTiles16bpp_Strided — Standard left-to-right 16bpp tile with shadow.
     * Address: 0x42C050
     *
     * 16-bit tile drawing with palette[0] = save destination, palette[1] =
     * half-bright shadow (destination >> 1 & g_surface_bshift). Higher
     * palette indices are normal tile colors. Standard left-to-right,
     * top-to-bottom blitting pattern.
     *
     * Called by: UIPANEL_Blit (flags=0x02)
     *
     * @param src_x          int — source X
     * @param src_y          int — source Y
     * @param dest_x         int — destination X
     * @param dest_y         int — destination Y
     * @param dest_surface   int — destination surface base pointer
     * @param dest_pitch     uint — destination pitch (bytes)
     * @param clip_left      int — clip left
     * @param clip_top       int — clip top
     * @param clip_right     int — clip right
     * @param clip_bottom    int — clip bottom
     * @return               1 on success
     */
    bool DrawTiles16bpp_Strided(int src_x, int src_y, int dest_x, int dest_y,
                                int dest_surface, uint32_t dest_pitch,
                                int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * DrawTiles16bpp_Reversed — Horizontal mirror 16bpp tile.
     * Address: 0x42C130
     *
     * Same as Strided but reads source bytes right-to-left within each row,
     * producing a horizontally-mirrored tile. Used for reversed scrolling.
     *
     * Called by: UIPANEL_Blit (flags=0x22)
     *
     * @param src_x          int — source X
     * @param src_y          int — source Y
     * @param dest_x         int — destination X
     * @param dest_y         int — destination Y
     * @param dest_surface   int — destination surface base pointer
     * @param dest_pitch     uint — destination pitch (bytes)
     * @param clip_left      int — clip left
     * @param clip_top       int — clip top
     * @param clip_right     int — clip right
     * @param clip_bottom    int — clip bottom
     * @return               1 on success
     */
    bool DrawTiles16bpp_Reversed(int src_x, int src_y, int dest_x, int dest_y,
                                 int dest_surface, uint32_t dest_pitch,
                                 int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * DrawTiles16bpp_Checker — Checkerboard 16bpp tile (every other row).
     * Address: 0x42C220
     *
     * Renders only even-numbered rows, skipping odd rows to produce a
     * checkerboard pattern. When clip_top is odd, adjusts source offset
     * by 1 to maintain alignment. Palette[0] = save destination pixel,
     * palette[1] = half-bright shadow.
     *
     * Called by: UIPANEL_Blit (flags=0x102)
     *
     * @param src_x          int — source X
     * @param src_y          int — source Y
     * @param dest_x         int — destination X
     * @param dest_y         int — destination Y
     * @param dest_surface   uint — destination surface base pointer
     * @param dest_pitch     uint — destination pitch (bytes)
     * @param clip_left      int — clip left
     * @param clip_top       uint — clip top
     * @param clip_right     int — clip right
     * @param clip_bottom    int — clip bottom
     * @return               1 on success
     */
    bool DrawTiles16bpp_Checker(int src_x, int src_y, int dest_x, int dest_y,
                                uint32_t dest_surface, uint32_t dest_pitch,
                                int clip_left, uint32_t clip_top,
                                int clip_right, int clip_bottom);

    /**
     * DrawTiles16bpp_Staggered — Staggered 16bpp tile dither overlay.
     * Address: 0x42C560
     */
    bool DrawTiles16bpp_Staggered(int src_x, int src_y, int dest_x, int dest_y,
                                  uint32_t dest_surface, uint32_t dest_pitch,
                                  int clip_left, uint32_t clip_top,
                                  int clip_right, int clip_bottom);

    /**
     * CopyTiles8bpp_Transparent — Copy 8bpp tile data skipping zero pixels.
     * Address: 0x42C330
     *
     * Copies 8-bit indexed pixel data from this tile cache to an external
     * 8bpp destination buffer. Skips pixel value 0 (transparent). Used by
     * ResourceManager clock animation (0x447400) to blit clock digit sprites onto the clock surface.
     *
     * NOTE: Parameters differ from other tile methods — the 3rd and 4th arguments
     * (dest rectangle right/bottom) are passed by callers but not used
     * internally. The dest_surface is an 8bpp buffer (not 16bpp).
     *
     * Called by: the clock-digit animation helper (0x447400)
     *
     * @param dest_x         int — destination X on 8bpp surface
     * @param dest_y         int — destination Y on 8bpp surface
     * @param dest_r         int — (unused, dest rect right from caller)
     * @param dest_b         int — (unused, dest rect bottom from caller)
     * @param dest_surface   uint8_t* — destination 8bpp pixel buffer
     * @param dest_pitch     int — destination surface pitch
     * @param src_left       int — source clip rect left
     * @param src_top        int — source clip rect top
     * @param src_right      int — source clip rect right
     * @param src_bottom     int — source clip rect bottom
     */
    void CopyTiles8bpp_Transparent(int dest_x, int dest_y,
                                   int dest_r, int dest_b,
                                   uint8_t* dest_surface, int dest_pitch,
                                   int src_left, int src_top,
                                   int src_right, int src_bottom);

    /**
     * CopyTiles8bpp_Direct — Copy 8bpp tile data directly (no transparency).
     * Address: 0x42C3D0
     *
     * Copies 8-bit indexed pixel data from this tile cache to an external
     * 8bpp destination buffer. No transparency check — copies all pixels.
     * Used by UIPANEL_FillRect for panel background fills.
     *
     * NOTE: Same parameter layout as CopyTiles8bpp_Transparent. Param_3 and
     * the 4th argument (dest rect right/bottom) is not used internally.
     *
     * Called by: UIPANEL_FillRect (0x42A610)
     *
     * @param dest_x         int — destination X on 8bpp surface
     * @param dest_y         int — destination Y on 8bpp surface
     * @param dest_r         int — (unused, dest rect right from caller)
     * @param dest_b         int — (unused, dest rect bottom from caller)
     * @param dest_surface   uint8_t* — destination 8bpp pixel buffer
     * @param dest_pitch     int — destination surface pitch
     * @param src_left       int — source clip rect left
     * @param src_top        int — source clip rect top
     * @param src_right      int — source clip rect right
     * @param src_bottom     int — source clip rect bottom
     */
    void CopyTiles8bpp_Direct(int dest_x, int dest_y,
                              int dest_r, int dest_b,
                              uint8_t* dest_surface, int dest_pitch,
                              int src_left, int src_top,
                              int src_right, int src_bottom);

    /**
     * BlitTileSurface — Right-to-left 16-bit tile blit with transparency.
     * Address: 0x42C890
     *
     * Reads 8-bit indexed tile data right-to-left (reversed X direction) from
     * the tile cache, writes left-to-right to the 16-bit destination surface
     * through the palette look-up table. Palette index 0 = transparent (pixel
     * is skipped, preserving destination). the 3rd/dest_x and 4th/dest_y arguments
     * are present in the parameter list but NOT used by the implementation
     * (src_x/src_y serve as the destination surface origin).
     *
     * The right-to-left read is achieved by starting the source position at
     * clip_right-1 and decrementing, while writing to dest from left to right.
     * Used for mirrored tile orientation in multiplayer/split-screen views.
     *
     * Called by: UIPANEL_Blit (flags=0x20)
     *
     * @param src_x          int — destination X on 16-bit surface
     * @param src_y          int — destination Y on 16-bit surface
     * @param dest_x         int — (UNUSED) present in parameter list
     * @param dest_y         int — (UNUSED) present in parameter list
     * @param dest_surface   uint — locked 16-bit surface base pointer
     * @param dest_pitch     uint — destination surface pitch in bytes
     * @param clip_left      int — clip rect left (source X start)
     * @param clip_top       int — clip rect top (source Y start)
     * @param clip_right     int — clip rect right (source X end)
     * @param clip_bottom    int — clip rect bottom (source Y end)
     * @return               true (always returns 1)
     */
    bool BlitTileSurface(int src_x, int src_y,
                         int dest_x, int dest_y,
                         uint32_t dest_surface, uint32_t dest_pitch,
                         int clip_left, int clip_top,
                         int clip_right, int clip_bottom);
};
