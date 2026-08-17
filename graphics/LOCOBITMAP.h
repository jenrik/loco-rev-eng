// Status: VALIDATED
#ifndef LOCOBITMAP_H
#define LOCOBITMAP_H

/**
 * LOCOBITMAP.h — UIPANEL_Surface / tile-occupancy / DDraw-present helpers
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * WARNING — NAME AMBIGUITY: the Ghidra database prefixes several unrelated
 * functions/concepts with "LOCOBITMAP_"/"LOCOBITMAP". These actually belong
 * to THREE separate concepts, only two of which are documented here:
 *
 *   A) PostcardAlbum (the postcard album/browser window, Entity-derived,
 *      vtable 0x4773F0, size 0x254) — RESOLVED 2026-08-17. This header used
 *      to define a second, competing, flat (non-inheriting) `class
 *      PostcardAlbum` alongside the real one in ui/PostcardAlbum.h/.cpp
 *      (which properly derives from UI_WindowBase). Both definitions
 *      mangled identically for several shared method names, which is a
 *      duplicate-symbol / silent-misbind hazard once both translation units
 *      are linked together. The flat class and all of its method bodies
 *      have been deleted from this header/its .cpp; ui/PostcardAlbum.h is
 *      now the SOLE definition of PostcardAlbum. See that header for the
 *      full class documentation (fields, vtable layout, method addresses).
 *
 *   B) UIPANEL_Surface (plain struct, vtable 0x477D28, size 0x20):
 *      A DirectDraw offscreen surface wrapper holding 8-bit indexed pixel
 *      data, an optional RGB565 palette (128 entries), and a DDraw surface.
 *      Managed by the UIPANEL_* functions (0x42A100 range). Documented below.
 *
 *   C) PixelDataCache (vtable 0x4773E8, size 0x18):
 *      Album pixel data cache — loads/saves .ind files from PostBag/.
 *      METHODS NOW IN PixelDataCache.h/cpp. See those files.
 *
 *   D) DDRAW_PresentRect (free function, 0x401280):
 *      A DDraw blit/present helper. Despite its name it is NOT a
 *      constructor — it blits a RECT from the backbuffer to the primary
 *      surface with client-to-screen coordinate conversion. Documented below.
 *
 * This header documents concepts (B), (C's forward declarations only), and
 * (D). PostcardAlbum (concept A) lives entirely in ui/PostcardAlbum.h/.cpp.
 */

#pragma once

#include "../shared/types.h"

/* ================================================================ */
/* Forward declarations for dependent types                          */
/* ================================================================ */

struct RESDATA;  /* Resource data descriptor */
struct IDirectDrawSurface4;  /* DirectDraw surface interface (see graphics/DDRAW.h,
                               * graphics/sdl3_ddraw.h / stubs/ddraw.h for the real
                               * declaration) — used by UIPANEL_Surface::CalcScrollRect. */

/* ================================================================ */
/* UIPANEL_Surface — DirectDraw surface wrapper (separate concept)   */
/* vtable 0x477D28, size 0x20 bytes                                  */
/* ================================================================ */

/**
 * UIPANEL_Surface is a lightweight wrapper around a DirectDraw offscreen
 * surface. It can operate in two modes:
 *   mode=0: software pixel buffer (width*height bytes of 8-bit indexed data)
 *   mode=1: DirectDraw surface (IDirectDrawSurface*)
 *
 * The struct has a vtable at offset 0 (pointing to 0x477D28) with only
 * one entry at [0]: the scalar deleting destructor (UIPANEL_DestroySurface).
 *
 * NOT derived from Entity. This is a plain C-compatible struct managed
 * by the UIPANEL_* functions (0x42A1xx range).
 */
struct UIPANEL_Surface {
    /** UIPANEL_Surface — zero-init every field (0x42A110).
     *  Address: 0x42A110 */
    UIPANEL_Surface();

    /** UIPANEL_Surface::~UIPANEL_Surface — scalar dtor, user cleanup only
     *  (0x42A140): frees palette_ptr when owned, frees pixels, releases
     *  ddraw_surf. The scalar-deleting-destructor flag and its conditional
     *  operator-delete call are compiler-generated, not reproduced here.
     *  Address: 0x42A140 */
    virtual ~UIPANEL_Surface();

    /** UIPANEL_Surface::UIPANEL_Surface(const&) — deep-copy ctor, a.k.a.
     *  UIPANEL_CopySurface (0x42A1C0, confirmed by its own
     *  OutputDebugStringA literal: "LOCOBITMAP COPY CONSTRUCTOR - failed to
     *  create surface"). Copies palette (new 0x200-byte allocation when
     *  owned, shared pointer otherwise), pixels (width*height bytes), and
     *  -- when present -- duplicates ddraw_surf via IDirectDraw4::
     *  CreateSurface (one retry on failure; the retry re-sets ddsCaps
     *  because the failed attempt clobbers it) followed by a full-surface
     *  Blt from the source. Address: 0x42A1C0 */
    UIPANEL_Surface(const UIPANEL_Surface& other);

    /* No assignment xref exists anywhere in the binary -- deleted rather
     * than left to an implicit member-wise copy that would double-free
     * palette_ptr/pixels/ddraw_surf. */
    UIPANEL_Surface& operator=(const UIPANEL_Surface&) = delete;

    /* vtable at +0x00 is compiler-managed via virtual methods */
    int32_t     mode;            // +0x04  0=software pixel buffer, 1=DDraw surface
    int32_t     width;           // +0x08  surface width in pixels (also read as the
                                  //        8bpp tile-cache row stride by the tile
                                  //        rendering methods below)
    int32_t     height;          // +0x0C  surface height in pixels
    uint8_t     has_palette;     // +0x10  if 1, palette_ptr is owned allocation
    uint8_t     flags;           // +0x11  misc flags
    // padding +0x12-0x13
    uint16_t*   palette_ptr;     // +0x14  256 uint16 entries (0x200 bytes), or shared
                                  //        ref. Evidenced uint16_t* (not uint32_t*) by
                                  //        Town_DrawTile (0x42BA90): byte-indexed,
                                  //        2-byte-stride lookup — see UIPANEL_Surface::
                                  //        DrawTile below.
    uint8_t*    pixels;          // +0x18  width*height byte buffer (mode 0 only)
    void*       ddraw_surf;      // +0x1C  IDirectDrawSurface* (mode 1 only)
    /* total: 0x20 bytes on x86 (vtable+mode+width+height+has_palette+flags+
     * palette_ptr+pixels+ddraw_surf = 4+4+4+4+1+1+2+4+4+4 = 0x20 exactly) —
     * confirmed no room for a further field in the original layout. A
     * trailing `void* palette;` here (no offset comment, zero readers/
     * writers tree-wide per a 2026-08-11 grep) was dead weight, likely a
     * stale duplicate of palette_ptr left over from the TownTileRenderer
     * merge documented above; removed rather than kept as an unexplained
     * 8-byte host-only widening. */

    /* ================================================================ */
    /* Tile rendering methods (all __thiscall, RET 0x28 in the original) */
    /*                                                                    */
    /* These implement the low-level tile pixel-pusher layer dispatched  */
    /* from UIPANEL_Blit (0x42B050) based on its flags parameter. They    */
    /* were originally reverse engineered as a standalone "TownTileRenderer" */
    /* class (town/TownTiles.h) before it was discovered that class was a */
    /* duplicate view of this exact struct: every field TownTileRenderer  */
    /* documented (mode@+0x04, stride@+0x08, palette@+0x14, pixels@+0x18, */
    /* surface_ref@+0x1C) is this struct's mode/width/palette_ptr/pixels/ */
    /* ddraw_surf at the identical offsets — confirmed by UIPANEL_Blit    */
    /* passing its own `this` directly as the receiver of these calls.   */
    /* Implementations live in town/TownTiles.cpp (Status: INTEGRATED).  */
    /*                                                                    */
    /* Rendering modes are dispatched from UIPANEL_Blit based on a flags */
    /* parameter:                                                        */
    /*   0x00  = DrawTile (base tile drawing)                            */
    /*   0x01  = InitTileCache (palette-initialize tile cache)           */
    /*   0x02  = DrawTiles16bpp_Strided (standard LTR 16bpp with shadow)  */
    /*   0x04  = FlushTileCache (2x2 block-expand palette cache)         */
    /*   0x05  = DrawCachedTile (2x2 block from cache)                   */
    /*   0x0F  = (default: DrawTile fallback)                            */
    /*   0x10  = DrawTileEx (3x2 block expansion)                        */
    /*   0x20  = BlitTileSurface (right-to-left blit)                    */
    /*   0x22  = DrawTiles16bpp_Reversed (H-mirror 16bpp)                */
    /*   0x84  = (alias for FlushTileCache)                              */
    /*   0x85  = (alias for DrawCachedTile)                              */
    /*   0x102 = DrawTiles16bpp_Checker (checkerboard 16bpp)             */
    /*   0x202 = DrawTiles16bpp_Staggered (staggered 16bpp)              */
    /*   0x400 = DrawTileLine (alpha-blended tile line)                  */
    /*   0x402 = (alias for DrawTileLine)                                */
    /*   0x40  = scroll rect adjustment (CalcScrollRect/CalcScrollRect_Reversed) */
    /* ================================================================ */

    /**
     * InitTileCache — Initialize tile cache via palette lookup.
     * Address: 0x42B9C0. Called by: UIPANEL_Blit (flags=0x01, 0x03).
     */
    bool InitTileCache(int src_x, int src_y, int dest_x, int dest_y,
                       uint8_t* dest_surface, uint32_t dest_pitch,
                       int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * BlitElement — Extract surface from element and blit.
     * Address: 0x42B960. Called by: EditWindow_render, Cursor_InitBackground,
     * CGWND_TrackPiece_Render, UIPANEL_DrawButton.
     */
    void BlitElement(uint32_t src_x, uint32_t src_y, int dest_x, uint32_t dest_y,
                     void* element, uint32_t clip_left, uint32_t clip_top,
                     int clip_right, uint32_t clip_bottom, uint32_t flags);

    /**
     * DrawTile — Draw an 8-bit indexed tile with palette remapping.
     * Address: 0x42BA90. Called by: UIPANEL_Blit (flags=0x00, default/0x0F).
     */
    bool DrawTile(int src_x, int src_y, int dest_x, int dest_y,
                  int dest_surface, uint32_t dest_pitch,
                  int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * FlushTileCache — Flush tile cache by expanding each pixel to 2x2 block.
     * Address: 0x42BB90. Called by: UIPANEL_Blit (flags=0x04, 0x84).
     */
    bool FlushTileCache(int src_x, int src_y, int dest_x, int dest_y,
                        uint32_t dest_surface, uint32_t dest_pitch,
                        int clip_left, int clip_top, int clip_right, uint32_t clip_bottom);

    /**
     * DrawCachedTile — Draw tile from cache with 2x2 expansion.
     * Address: 0x42BC80. Called by: UIPANEL_Blit (flags=0x05, 0x85).
     */
    bool DrawCachedTile(uint32_t src_x, int src_y, int dest_x, int dest_y,
                        uint32_t dest_surface, uint32_t dest_pitch,
                        int clip_left, int clip_top, int clip_right, uint32_t clip_bottom);

    /**
     * DrawTileEx — Extended tile drawing with 3x2 pixel expansion.
     * Address: 0x42BD70. Called by: UIPANEL_Blit (flags 0x10-0x1F).
     */
    bool DrawTileEx(int src_x, int src_y, int dest_x, int dest_y,
                    uint32_t dest_surface, uint32_t dest_pitch,
                    uint32_t clip_left, uint32_t clip_top,
                    int clip_right, int clip_bottom);

    /**
     * DrawTileLine — Alpha-blended tile line drawing.
     * Address: 0x42BEC0. Called by: UIPANEL_Blit (flags=0x400, 0x402).
     */
    bool DrawTileLine(int src_x, int src_y, int dest_x, int dest_y,
                      int dest_surface, uint32_t dest_pitch,
                      int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * DrawTiles16bpp_Strided — Standard left-to-right 16bpp tile with shadow.
     * Address: 0x42C050. Called by: UIPANEL_Blit (flags=0x02).
     */
    bool DrawTiles16bpp_Strided(int src_x, int src_y, int dest_x, int dest_y,
                                int dest_surface, uint32_t dest_pitch,
                                int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * DrawTiles16bpp_Reversed — Horizontal mirror 16bpp tile.
     * Address: 0x42C130. Called by: UIPANEL_Blit (flags=0x22).
     */
    bool DrawTiles16bpp_Reversed(int src_x, int src_y, int dest_x, int dest_y,
                                 int dest_surface, uint32_t dest_pitch,
                                 int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * DrawTiles16bpp_Checker — Checkerboard 16bpp tile (every other row).
     * Address: 0x42C220. Called by: UIPANEL_Blit (flags=0x102).
     */
    bool DrawTiles16bpp_Checker(int src_x, int src_y, int dest_x, int dest_y,
                                uint32_t dest_surface, uint32_t dest_pitch,
                                int clip_left, uint32_t clip_top,
                                int clip_right, int clip_bottom);

    /**
     * DrawTiles16bpp_Staggered — Staggered 16bpp tile dither overlay.
     * Address: 0x42C470. Called by: UIPANEL_Blit (flags=0x202).
     */
    bool DrawTiles16bpp_Staggered(int src_x, int src_y, int dest_x, int dest_y,
                                  uint32_t dest_surface, uint32_t dest_pitch,
                                  int clip_left, uint32_t clip_top,
                                  int clip_right, int clip_bottom);

    /**
     * CopyTiles8bpp_Transparent — Copy 8bpp tile data skipping zero pixels.
     * Address: 0x42C330. Called by: the clock-digit animation helper (0x447400).
     * NOTE: 3rd/4th args (dest rect right/bottom) are passed by callers but
     * unused internally; dest_surface is an 8bpp buffer (not 16bpp).
     */
    void CopyTiles8bpp_Transparent(int dest_x, int dest_y,
                                   int dest_r, int dest_b,
                                   uint8_t* dest_surface, int dest_pitch,
                                   int src_left, int src_top,
                                   int src_right, int src_bottom);

    /**
     * CopyTiles8bpp_Direct — Copy 8bpp tile data directly (no transparency).
     * Address: 0x42C3D0. Called by: UIPANEL_FillRect (0x42A610).
     * NOTE: same unused-argument layout as CopyTiles8bpp_Transparent.
     */
    void CopyTiles8bpp_Direct(int dest_x, int dest_y,
                              int dest_r, int dest_b,
                              uint8_t* dest_surface, int dest_pitch,
                              int src_left, int src_top,
                              int src_right, int src_bottom);

    /**
     * BlitTileSurface — Right-to-left 16-bit tile blit with transparency.
     * Address: 0x42C890. Called by: UIPANEL_Blit (flags=0x20).
     * NOTE: dest_x/dest_y args are present but unused (src_x/src_y serve as
     * the destination surface origin too).
     */
    bool BlitTileSurface(int src_x, int src_y,
                         int dest_x, int dest_y,
                         uint32_t dest_surface, uint32_t dest_pitch,
                         int clip_left, int clip_top,
                         int clip_right, int clip_bottom);

    /**
     * CalcScrollRect / CalcScrollRect_Reversed — Compute the visible tile
     * rect from a scroll position, clamped against the surface bounds
     * (queried via `surface_obj->GetSurfaceDesc()`, vtable+0x58 in the
     * original) and an input clip rect. Rewrites BOTH `rect` and
     * `clip_rect` in place. Address: 0x42C590 (forward) / 0x42C700
     * (reversed). Called by: UIPANEL_Blit (flags=0x40), selecting forward
     * vs. reversed based on this->mode (1=forward/DDraw surface,
     * 0=reversed/software buffer).
     *
     * RESOLVED (previously deferred — see PROGRESS.md history): the
     * original x86 callee pops 0x10 (4 stack dwords) via its RET
     * instruction because it genuinely takes 4 stack arguments, not 2.
     * Ghidra's own signature guess (2 args) was wrong and caused the
     * decompiler to lose track of two real parameters, surfacing them as
     * unresolved "unaff_EBX"/"unaff_EBP"/"ptStack_4" artifacts. Re-derived
     * from raw disassembly of both this function and its sole caller
     * (UIPANEL_Blit @0x42B050, the `this->+4 == 1` / `== 0` dispatch
     * around 0x42B0EF-0x42B111): the caller pushes, in argument order,
     * `&param_1` (a RECT-shaped view over its own src_x/src_y/dest_x/
     * dest_y stack slots), `param_5` (dest_surface), `&param_6` (a second
     * RECT-shaped view over clip_left/clip_top/clip_right/clip_bottom),
     * and `param_10` (its own flags value) — i.e. exactly 4 stack args,
     * matching RET 0x10.
     *
     * `flags` is pushed by the caller to match the real stack frame but is
     * never read by either function body (checked exhaustively against
     * every stack-relative read in the disassembly) — kept in the
     * signature for original-ABI fidelity, unused internally.
     *
     * The two functions are NOT simple mirror images:
     *   - CalcScrollRect (forward) validates `clip_rect` only against
     *     itself (IntersectRect(tmp, clip_rect, clip_rect), a normalize/
     *     empty-check idiom), clips the shifted view rect against the
     *     surface's *reported* bounds (GetSurfaceDesc), and ALWAYS
     *     returns false — the original's shared epilogue does an
     *     unconditional `XOR AL,AL` on every path, a likely original bug
     *     with no observable effect because the sole caller never checks
     *     the return value. It also computes a `{0,0,this->width,
     *     this->height}` panel-bounds rect via SetRect that is written
     *     but never read again — dead code in the original, omitted here.
     *   - CalcScrollRect_Reversed additionally clips `clip_rect` against
     *     the panel's *logical* bounds (`{0,0,this->width,this->height}`)
     *     BEFORE clipping against the surface's reported bounds, and
     *     returns true on success / false only on the two early-empty-
     *     rect bail paths.
     */
    bool CalcScrollRect(RECT* rect, IDirectDrawSurface4* surface_obj,
                        RECT* clip_rect, uint32_t flags);
    bool CalcScrollRect_Reversed(RECT* rect, IDirectDrawSurface4* surface_obj,
                                 RECT* clip_rect, uint32_t flags);
};

/* Construction/destruction/copy are the real UIPANEL_Surface() /
 * ~UIPANEL_Surface() / UIPANEL_Surface(const&) members above (0x42A110 /
 * 0x42A140 / 0x42A1C0) -- callers use `new UIPANEL_Surface()`, `delete`,
 * and `new UIPANEL_Surface(*src)` respectively, not free functions. */

/* Canonical global: the one persistent UIPANEL_Surface created by
 * DDRAW_Init (0x45C8A0) to hold the "2__smisc_thumbpal_bmp" thumbnail
 * palette bitmap; lives for the process lifetime (no destroy call exists
 * anywhere in the binary). Address: 0x004FF110 */
extern UIPANEL_Surface* g_thumbpal_surface;

/* Factory for translation units that cannot include this header directly.
 * Equivalent to `new UIPANEL_Surface()`; only usable through a forward
 * declaration of UIPANEL_Surface. */
UIPANEL_Surface* UIPANEL_Surface_New();

/* ================================================================ */
/* Tile-occupancy / viewport collision checks (0x42C950-0x42CB10)   */
/*                                                                    */
/* Free functions (not UIPANEL_Surface methods) operating on a        */
/* UIPANEL_Surface* receiver. Ghidra's own function names carry a     */
/* "Town_" prefix — MISLEADING, these are NOT Town methods, despite    */
/* living at addresses adjacent to Town.cpp's own tile logic and       */
/* having been transcribed there historically. Verified via Ghidra    */
/* xrefs: the only callers are BuildingMgr::InvalidateRects/           */
/* BlitOverlaps (game/BuildingMgr.cpp) and World::ProcessEvents        */
/* (game/World.cpp), both of which pass a UIPANEL_Surface* pulled from */
/* the RESDATA-embedded "ui_panel" alias documented in shared/types.h  */
/* (RESDATA::flags, +0x10) / game/BuildingMgr.cpp's entity_surface().  */
/* Implemented in town/TownTiles.cpp beside UIPANEL_Surface's other    */
/* address-adjacent methods.                                           */
/* ================================================================ */

/**
 * Town_CheckOccupied — Scan tile buffer or DDraw surface for non-empty
 * (occupied) tiles in rect [x1..x2, y1..y2).
 * Address: 0x42C950 (__thiscall on a UIPANEL_Surface* receiver).
 *
 * mode==0 (self->mode): scans the byte array at self->pixels (stride
 * self->width). mode==1: delegates to Town_CheckOccupiedEx.
 * @return 1 if any tile in the rect is occupied, 0 otherwise.
 */
uint8_t Town_CheckOccupied(UIPANEL_Surface* self, int x1, int y1, int x2, int y2);

/**
 * Town_CheckOccupiedEx — Extended tile occupancy via primary-surface lock.
 * Address: 0x42C9F0 (__stdcall, 4 stack args). No receiver: operates on
 * the global primary DirectDraw surface (g_primary_surface) directly.
 * Called from Town_CheckOccupied when self->mode == 1.
 *
 * Locks the primary surface, scans 16-bit pixels in [x1..x2, y1..y2). A
 * pixel is occupied when ((pixel & red_mask) >> red_shift) != 0x1f AND
 * (pixel & blue_mask) != 0x1f (non-water).
 */
uint8_t Town_CheckOccupiedEx(int x1, int y1, int x2, int y2);

/**
 * Town_BlitViewport — Passability test for one point (x, y) against a
 * viewport occupancy buffer/surface.
 * Address: 0x42CB10 (__thiscall on a UIPANEL_Surface* receiver, 6 stack
 * args, RET 0x18).
 *
 * NOTE: faithful to the binary, the bound tests use the parameters as
 * (x < x1 || y2 < x || y < x2 || x < y) -> passable; parameter y1 is
 * never read (documented quirk of the original).
 * mode==0 (self->mode): byte index buffer at self->pixels (stride
 * self->width); byte==0 -> passable. mode==1: locks self->ddraw_surf,
 * water check (both channels == 0x1f -> passable).
 * @return 1 = passable, 0 = occupied.
 */
uint32_t Town_BlitViewport(UIPANEL_Surface* self, int x1, int y1, int x2, int y2,
                          int x, int y);

/* ================================================================ */
/* DDRAW_PresentRect — DDraw present/blit helper (free function)     */
/* Address: 0x401280, __cdecl                                       */
/* ================================================================ */

/**
 * DDRAW_PresentRect — blit a RECT from backbuffer to primary surface.
 * Address: 0x401280
 *
 * Despite the original "LOCOBITMAP_Create" Ghidra name, this is NOT a
 * constructor. It performs a DirectDraw Blt operation to present a region
 * of the backbuffer onto the primary surface, handling surface-loss recovery.
 *
 * Called by: UIPANEL_EndPaintEx (3 call sites), TileMap_InvalidateDirtyRects,
 *            DirectPlay_Init
 *
 * @param rect          source rectangle (client coords) to blit
 * @param hWnd          window handle (for client->screen coord conversion)
 * @param offset_xy     optional [x,y] offset (or NULL), used to negate
 *                      before client->screen conversion
 * @param use_color_key 0=no color key (0x1000000 flag), 1=use color key
 *                      (0x200 flag)
 */
#ifdef _WIN32
void __cdecl DDRAW_PresentRect(const RECT* rect, HWND hWnd, int32_t offset_xy[2],
                               uint8_t use_color_key);
#else
/* Host builds route present through the SDL3 path (graphics/sdl3_ddraw.cpp);
 * that symbol's real signature uses plain int and void pointers rather than
 * RECT/HWND/uint8_t — matching the same guard already used in
 * world/tilemap.h. Declaring the RECT/HWND shape unconditionally here (as
 * this header used to) mangles to a different, unlinked symbol on host
 * builds and makes every caller that also declares the host shape
 * ambiguous, since both declarations are visible at once. */
void DDRAW_PresentRect(void* rect, void* hwnd, int* offset_xy, int use_color_key);
#endif

#endif /* LOCOBITMAP_H */
