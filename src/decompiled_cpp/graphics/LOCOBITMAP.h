
// Status: TRANSCRIBED
#ifndef LOCOBITMAP_H
#define LOCOBITMAP_H

/**
 * LOCOBITMAP.h — PostcardAlbum window class (Entity-derived UI window)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * WARNING — NAME AMBIGUITY: The Ghidra database prefixes all functions in
 * the 0x401000-0x402880 range with "LOCOBITMAP_". These actually belong to
 * THREE separate concepts:
 *
 *   A) PostcardAlbum (Entity-derived, vtable 0x4773F0, size 0x254):
 *      The postcard album/browser window. This is the UI that displays
 *      received postcards with navigation arrows, thumbnail grid, and
 *      keyboard controls. Functions: LOCOBITMAP_CreateFromResource,
 *      LOCOBITMAP_DestroyFromResource, LOCOBITMAP_InitFromResource,
 *      LOCOBITMAP_InitWindow, LOCOBITMAP_DestroyWindow,
 *      LOCOBITMAP_PaintWindow, LOCOBITMAP_LoadPalette.
 *
 *   B) UIPANEL_Surface (plain struct, vtable 0x477D28, size 0x20):
 *      A DirectDraw offscreen surface wrapper holding 8-bit indexed pixel
 *      data, an optional RGB565 palette (128 entries), and a DDraw surface.
 *      Managed by the UIPANEL_* functions (0x42A100 range). Its fields are:
 *        +0x00  vtable      (0x477D28, one entry: ~UIPANEL_Surface)
 *        +0x04  mode        (0=software, 1=DDraw)
 *        +0x08  width
 *        +0x0C  height
 *        +0x10  has_palette (byte)
 *        +0x11  flags       (byte)
 *        +0x14  palette_ptr (128 uint32 entries = 0x200 bytes, if has_palette)
 *        +0x18  pixels      (width*height byte buffer)
 *        +0x1C  ddraw_surf  (IDirectDrawSurface*)
 *
 *   C) PixelDataCache (vtable 0x4773E8, size 0x18):
 *      Album pixel data cache — loads/saves .ind files from PostBag/.
 *      METHODS NOW IN PixelDataCache.h/cpp. See those files.
 *
 *   D) DDRAW_PresentRect (free function, 0x401280):
 *      A DDraw blit/present helper. Despite its name it is NOT a
 *      constructor — it blits a RECT from the backbuffer to the primary
 *      surface with client-to-screen coordinate conversion.
 *
 * This header documents concept (A) — the Entity-derived PostcardAlbum
 * window class.
 *
 * Class hierarchy:
 *   GameObject (vtable 0x477820)
 *     └─ Entity  (vtable 0x477488)
 *          └─ UI_WindowBase (vtable 0x477C30)
 *               └─ PostcardAlbum  ← this class (vtable 0x4773F0)
 *
 * Size: 0x254 bytes (allocation in CGWND_InitAllSubsystems)
 * Vtable: 0x004773F0
 *
 * Vtable layout (0x4773F0):
 *   [0] +0x00: scalar deleting destructor (0x401FB0)
 *   [1] +0x04: DestroyWindow (0x402660)
 *   PaintWindow dispatch at 0x00477444
 */

#pragma once

#include "../shared/types.h"
#include "../core/Entity.h"
#include "../ui/ButtonSprite.h"

/* ================================================================ */
/* Forward declarations for dependent types                          */
/* ================================================================ */

struct RESDATA;  /* Resource data descriptor */

/* ================================================================ */
/* PostcardAlbum — Postbag album/browser UI window                   */
/* Extends UI_WindowBase (which extends Entity extends GameObject)   */
/* ================================================================ */

class PostcardAlbum {
public:
    virtual ~PostcardAlbum() {}
    /* ================================================================ */
    /* Fields (inherited from GameObject -> Entity -> UI_WindowBase)     */
    /* ================================================================ */

    /* vtable at +0x00 is compiler-managed via virtual methods */
    HINSTANCE   hInstance;           // +0x04  (from Entity)
    HWND        hWnd;                // +0x08  (from Entity)

    // +0x0C to +0xD3: intermediate Entity/UI_WindowBase fields (TBD)

    /* ================================================================ */
    /* Scroll offsets (UI_WindowBase / Surface layer)                    */
    /* Used by SURFACE_BlitElement, GFX_RenderTileName,                 */
    /* GFX_RenderAllTiles for source/destination blit translation.       */
    /* ================================================================ */

    int32_t     scroll_src_x;        // +0x0D4  src X offset for blit
    int32_t     scroll_src_y;        // +0x0D8  src Y offset for blit
    // +0x0DC to +0x0E3: unknown/other fields
    uint8_t     has_window;          // +0xE4  byte: 1 = child window created

    /* ================================================================ */
    /* PostcardAlbum-specific fields                                     */
    /* ================================================================ */

    void*       field_0E8;           // +0x0E8  Icon handle (HICON) or sprite pointer

    int32_t     scroll_dst_x;        // +0x0EC  dst X offset for blit
    int32_t     scroll_dst_y;        // +0x0F0  dst Y offset for blit
    // +0x0F4 to +0x0FB: unknown/other fields

    uint8_t     background_loaded;   // +0x0FC  1 after GFX_InitWindow loads background surface
    // +0x0FD to +0x10F: unknown/other fields

    uint8_t     destroyed;           // +0x110  destroyed/hidden flag (0=visible, 1=destroyed)
    uint8_t     paint_inited;        // +0x111  flag: 1 after GFX_InitSprites
    uint8_t     window_visible;      // +0x112  flag: window visibility (bitmap ready)
    // +0x113: padding
    int32_t     tile_offset;         // +0x114  current tile scroll offset index
    int32_t     tile_shown_count;    // +0x118  number of tiles currently displayed per page
    int32_t     tile_total_count;    // +0x11C  total tiles fitting in the grid
    int32_t     hit_index;           // +0x120  row index stored by HitTest() (surface_type 7/8/10)
    int32_t     box_count;           // +0x124  box/thumbnail count (always 9)
    int32_t     sprite_state_value;  // +0x128  sprite state index for surface_type 7
    uint8_t     show_debug_text;     // +0x12C  1 = render debug tile names (checked by GFX_RenderAllTiles)
    // +0x12D to +0x12F: padding
    ButtonSprite* extra_sprite;      // +0x130  extra sprite or state pointer (init to nullptr)
    int32_t     high_res;            // +0x134  1 if screen >= 800x600, else 0

    /* ================================================================ */
    /* Resource/surface pointers (loaded by GFX_InitWindow/InitSprites)  */
    /* ================================================================ */

    void*       background_resdata;  // +0x138  RESDATA* for background bitmap
    void*       background_ui_panel; // +0x13C  UIPANEL* surface for background
    void*       paint_resdata;       // +0x140  RESDATA* for paint overlay (resource 0x3CFA)
    void*       paint_surface;       // +0x144  paint overlay surface handle

    /* ================================================================ */
    /* Sprite pointers — 8 individual UI elements                        */
    /* Accessed by SURFACE_* functions as surface_type 1-7, 9:          */
    /*   1 = +0x148  (main action button)                                 */
    /*   2 = +0x14C  (toggle sprite A, flag at +0x1D6)                   */
    /*   3 = +0x150  (aux button B)                                      */
    /*   4 = +0x154  (toggle sprite B, flag at +0x1D7)                   */
    /*   5 = +0x158  (help/question sprite)                              */
    /*   6 = +0x15C  (toggle sprite C, flag at +0x1D4 = scroll_up)      */
    /*   7 = +0x160  (toggle sprite D, flag at +0x1D5 = scroll_down)    */
    /*   8 = +0x164  (indicator/big sprite, used by type 7)              */
    /* ================================================================ */

    ButtonSprite* sprite_main;         // +0x148  surface_type 1: main button
    ButtonSprite* sprite_toggle_a;     // +0x14C  surface_type 2: toggle with flag +0x1D6
    ButtonSprite* sprite_button_b;     // +0x150  surface_type 3: aux button
    ButtonSprite* sprite_toggle_b;     // +0x154  surface_type 4: toggle with flag +0x1D7
    ButtonSprite* sprite_help;         // +0x158  surface_type 9: help/question
    ButtonSprite* sprite_toggle_c;     // +0x15C  surface_type 5: toggle with up-arrow flag +0x1D4
    ButtonSprite* sprite_toggle_d;     // +0x160  surface_type 6: toggle with down-arrow flag +0x1D5
    ButtonSprite* sprite_indicator;    // +0x164  surface_type 7: indicator/big nav sprite

    /* ================================================================ */
    /* Sprite pointers — per-row elements (6 rows, 3 sprites each)       */
    /* Surface_type 8 = tile_left[6]  (at +0x168-0x17C)                 */
    /* Surface_type 10 = tile_right[6] (at +0x198-0x1AC)                */
    /* ================================================================ */

    ButtonSprite* tile_left[6];        // +0x168..+0x17C  left selection rect per row (type 8)
    ButtonSprite* tile_mid[6];         // +0x180..+0x194  mid background per row (used for name text)
    ButtonSprite* tile_right[6];       // +0x198..+0x1AC  right nav rect per row (type 10)

    /* ================================================================ */
    /* Thumbnail/extra sprites (9 sprites for the grid, surface_type 7)  */
    /* ================================================================ */

    ButtonSprite* thumb_sprites[9];    // +0x1B0..+0x1CC  thumbnail sprite pointers

    // +0x1D0 to +0x1D3: padding/unknown

    /* ================================================================ */
    /* Toggle flags — updated by GFX_RenderAllTiles based on scroll pos  */
    /* Checked by SURFACE_UpdateSprite and SURFACE_BlitElement.         */
    /* 1 = enabled/visible; 0 = disabled/hidden.                         */
    /* ================================================================ */

    uint8_t     toggle_up;           // +0x1D4  up-arrow toggle (surface_type 5 flag)
    uint8_t     toggle_down;         // +0x1D5  down-arrow toggle (surface_type 6 flag)
    uint8_t     toggle_2;            // +0x1D6  toggle used by surface_type 2
    uint8_t     toggle_4;            // +0x1D7  toggle used by surface_type 4
    uint8_t     scroll_up_visible;   // +0x1D8  1 = can scroll up (used by PostcardAlbum_PaintWindow)
    uint8_t     scroll_down_visible; // +0x1D9  1 = can scroll down (used by PostcardAlbum_PaintWindow)

    /* ================================================================ */
    /* Tile name text buffer — 6 rows, 0x14 (20) bytes per row          */
    /* Stores the rendered tile name string (null-terminated ASCII).     */
    /* Initialized to 0 in InitFromResource; written by GFX_RenderTileName.*/
    /* ================================================================ */

    char        tile_text_buf[6][20];// +0x1DA..+0x251  text buffer, 20 bytes per tile row

    // Total: 0x252 bytes (0x1DA + 6*20 + padding = 0x254 allocation)

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * PostcardAlbum constructor (factory).
     * Address: 0x401F50
     *
     * Calls UI_WindowBase_Ctor() for base initialization (Entity/GameObject
     * chain), sets vtable to 0x4773F0, then calls InitFromResource() to
     * allocate all sprite objects.
     *
     * Called by: CGWND_InitAllSubsystems (0x4072B9) for g_postcard
     *
     * @param mem       pre-allocated memory (0x254 bytes from operator_new)
     * @param hInst     application HINSTANCE
     * @param resId     resource ID (e.g. 0x1FB = 507 for postcard album)
     * @return          this pointer, or NULL on allocation failure
     */
    static PostcardAlbum* CreateFromResource(void* mem, HINSTANCE hInst, uint32_t resId);

    /**
     * Native C++ constructor corresponding to the factory at 0x401F50.
     * The factory invokes it with placement new after caller allocation.
     */
    PostcardAlbum(HINSTANCE hInst, uint32_t resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x401FB0
     *
     * Calls LoadPalette (which frees all child sprite objects and calls
     * UI_WindowBase_BaseDtor), then optionally frees the heap allocation.
     *
     * @param flags  bit 0: free heap memory (1 = call GLOBAL_free)
     * @return       this pointer
     */
    void* DestroyFromResource(uint8_t flags);

    /* ================================================================ */
    /* Initialization methods                                             */
    /* ================================================================ */

    /**
     * InitFromResource — allocate all sprite objects.
     * Address: 0x401FD0
     *
     * Creates sprite objects for:
     *   - 8 distinct UI sprites (main, toggle A/B, button B, help,
     *     toggle C/D, indicator)
     *   - 6 rows of 3 sprites each (tile_left + tile_mid + tile_right)
     *   - 9 thumbnail sprites
     * Sets toggle flags to 1 and initializes text buffers to 0.
     * Detects high-resolution mode (>= 800x600) via g_screen_width/height.
     *
     * Called by: CreateFromResource (0x401F8E)
     */
    void InitFromResource();

    /**
     * InitWindow — create the Windows window for this album.
     * Address: 0x402520
     *
     * Creates a full-screen child window via UI_CreateFullWindow with
     * the desktop client area dimensions. Loads icon resource 0x65.
     *
     * Called by: CGWND_InitAllSubsystems for both PostcardAlbum and
     *            PostcardPreviewWindow
     *
     * @param hWndParent  parent window handle
     * @return            TRUE on success
     */
    bool InitWindow(HWND hWndParent);

    /* ================================================================ */
    /* Window lifecycle                                                   */
    /* ================================================================ */

    /**
     * DestroyWindow — hide and free child window resources (vtable[1]).
     * Address: 0x402660
     *
     * Hides the window via UI_WindowBase_Hide, clears visibility flag,
     * frees sprite objects via FreeSprites().
     *
     * Called by: cleanup paths (via vtable dispatch)
     */
    virtual void DestroyWindow();

    /**
     * PaintWindow — WndProc for keyboard navigation.
     * Address: 0x402690
     *
     * Handles VK_LEFT (0x25), VK_RIGHT (0x27), VK_RETURN (0x0D),
     * VK_ESCAPE (0x1B). Navigates album pages, selects items, updates
     * sprite states. All other messages routed to DefWindowProcA.
     *
     * Uses global g_pixel_cache (PixelDataCache*) for album data access.
     *
     * @param hWnd    window handle
     * @param msg     window message
     * @param wParam  WPARAM
     * @param lParam  LPARAM
     * @return        0 if handled, DefWindowProcA result otherwise
     */
    LRESULT PaintWindow(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /* ================================================================ */
    /* Destructor helper (misnamed "LoadPalette" in Ghidra)               */
    /* ================================================================ */

    /**
     * FreeAllSprites — free all child sprites and destroy base window.
     * Address: 0x402380
     *
     * Despite the Ghidra name "LOCOBITMAP_LoadPalette", this function
     * actually restores the base vtable (0x4773F0), frees all 8 navigation
     * arrow sprites, 18 row sprites, 9 thumbnail sprites, and 2 extra
     * sprites (nav_big + unk) via their vtable[0] destructors, then calls
     * UI_WindowBase_BaseDtor(). Called exclusively from
     * DestroyFromResource (the scalar dtor).
     */
    void FreeAllSprites();

    /* ================================================================ */
    /* Surface sprite management (SURFACE_* group)                       */
    /* Managed sprite UI with hit-testing, state updates, and rendering.  */
    /* Surface types: 1=main, 2=toggleA, 3=buttonB, 4=toggleB,          */
    /* 5=toggleC(up), 6=toggleD(down), 7=indicator, 8=tile, 9=help,     */
    /* 10=tile_right/nav. Type 0 = no-op.                                */
    /* ================================================================ */

    /**
     * UpdateSprite — update sprite visual states after a click/navigation.
     * Address: 0x403BA0
     *
     * Sets sprite state based on surface_type. Toggle types (2, 4, 5, 6)
     * check their flag byte first:
     *   - flag == 1: set sprite to state 0 (default/visible)
     *   - flag != 1: set sprite to state 2 (highlight/disabled)
     * Non-toggle types (1, 3, 9) always go to state 0.
     * Types 7/8/10 are no-ops (no update needed).
     *
     * Called by: PostcardAlbum_PaintWindow (VK_LEFT/VK_RIGHT handlers),
     *            GFX_RenderAllTiles (after scroll toggle changes),
     *            unnamed callers in 0x404EBB-0x4053B5 range
     *
     * @param surface_type  element type 1-10 (0 = no-op)
     */
    void UpdateSprite(int surface_type);

    /**
     * HitTest — hit-test a point against all UI sprites.
     * Address: 0x403CD0
     *
     * Tests the given (x, y) point against each sprite's rectangle in
     * priority order:
     *   1. main (+0x148)       -> return 1
     *   2. help (+0x158)       -> return 9
     *   3. toggle_b (+0x154)   -> return 4
     *   4. toggle_a (+0x14C)   -> return 2
     *   5. button_b (+0x150)   -> return 3
     *   6. toggle_c (+0x15C)   -> return 5
     *   7. toggle_d (+0x160)   -> return 6
     *   8. tile_left[6]        -> return 8 (stores row index in hit_index)
     *   9. tile_right[6]       -> return 10 (stores row index in hit_index)
     *  10. thumb_sprites[9]    -> return 7 (stores index in hit_index)
     *  11. none matched        -> return 0
     *
     * Called by: PostcardAlbum_PaintWindow (dispatch via UI event handler
     *            at 0x404F88)
     *
     * @param x  screen X coordinate to test
     * @param y  screen Y coordinate to test
     * @return   surface_type 1-10 on hit, 0 on miss
     */
    int HitTest(int x, int y);

    /**
     * BlitElement — render one UI element to screen with click sound.
     * Address: 0x403E80
     *
     * Plays sound resource 0x5015 (click), reads the sprite's rect,
     * applies scroll offsets, blits via UIPANEL_Blit to the primary
     * surface, then sets the sprite to state 1 (selected/pressed).
     *
     * Toggle types (2, 4, 5, 6) check their flag first. If flag != 1,
     * sets sprite to highlight state 2 and returns without sound/blit.
     * Type 7 sets sprite_indicator state from sprite_state_value.
     * Type 6 plays sound AFTER the blit (unique ordering).
     * Types 8/10 are no-ops.
     *
     * Called by: PostcardAlbum_PaintWindow (VK_LEFT/VK_RIGHT handlers),
     *            unnamed callers in 0x404FB6-0x405352 range
     *
     * @param surface_type  element type 1-10 (0 = no-op)
     */
    void BlitElement(int surface_type);

    /* ================================================================ */
    /* GFX tile grid rendering                                            */
    /* ================================================================ */

    /**
     * InitWindowSurface — lazy-load the background window surface.
     * Address: 0x404720
     *
     * If background_loaded (+0xFC) is 0, loads resource 0x3C0A (normal)
     * or 0x3C0B (hi-res) based on high_res flag, stores the RESDATA at
     * +0x138, creates a UIPANEL surface at +0x13C, and sets +0xFC to 1.
     *
     * Called by: CGWND_InitMode1 (0x408457, 0x408595) for both town
     *            view and album view
     */
    void InitWindowSurface();

    /**
     * InitSprites — initialize all tile-grid sprite objects.
     * Address: 0x404770
     *
     * If paint_inited (+0x111) is 0, calls Sprite_Init on each of the 8
     * individual sprites (+0x148-+0x164) and 6 tile_mid sprites (+0x180).
     * Loads paint resource 0x3CFA into paint_resdata (+0x140), creates
     * paint surface at +0x144, and sets paint_inited to 1.
     *
     * Called by: PostcardAlbum_InitWindow via CGWND flow (0x402595)
     */
    void InitSprites();

    /**
     * FreeSprites — tear down all tile-grid sprite objects.
     * Address: 0x404830
     *
     * If paint_inited (+0x111) is set, calls the paint RESDATA destructor
     * (vtable[2]), destroys all sprites (8 individual + 6 tile_mid) via
     * Sprite_Destroy, and clears paint_inited to 0.
     *
     * Called by: PostcardAlbum_DestroyWindow, PostcardAlbum_FreeAllSprites
     */
    void FreeSprites();

    /**
     * RenderTileName — render a single tile's name text.
     * Address: 0x4048E0
     *
     * Looks up the asset in the pixel data cache for this tile's index.
     * If not found, clears the name flag and blits the empty background
     * rect. If found, renders the player name via DPLAY_RenderPlayer,
     * copies the name string to tile_text_buf[tile_index], then sets
     * the tile_mid sprite state to 0 (default/visible).
     *
     * Called by: GFX_RenderAllTiles (0x404AE8)
     *
     * @param tile_index  tile row to render (0-5)
     */
    void RenderTileName(int tile_index);

    /**
     * RenderAllTiles — full tile grid render pass.
     * Address: 0x404AC0
     *
     * For each visible tile:
     *   1. Calls RenderTileName to render tile name text
     *   2. Blits the tile_right background rect via UIPANEL_Blit
     * After all tiles: calls UIPANEL_BeginPaint, renders debug tile names
     * (if show_debug_text is 1), then calls UIPANEL_EndPaintEx.
     *
     * Then updates scroll toggle flags based on current tile_offset and
     * tile_total_count vs pixel data cache size, calling UpdateSprite(5)
     * or UpdateSprite(6) when arrows change state.
     *
     * Called by: PostcardAlbum_PaintWindow, CGWND_GameSetup_RenderPlayerSlots,
     *            and unnamed callers in the 0x404F18-0x4053AC range
     */
    void RenderAllTiles();

    /**
     * BlitToSurface — internal helper to copy a sprite rect through
     * scroll offsets and blit from the background panel to the primary
     * surface. Returns false on blit failure and prints debug message.
     *
     * Used by: BlitElement and RenderTileName/RenderAllTiles.
     * Not a standalone address; extracted from the common blit pattern.
     *
     * @param sprite  sprite pointer with rect at +0x04
     * @return        TRUE on successful blit, FALSE on failure
     */
    bool BlitToSurface(ButtonSprite* sprite);
};

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
    /* vtable at +0x00 is compiler-managed via virtual methods */
    int32_t     mode;            // +0x04  0=software pixel buffer, 1=DDraw surface
    int32_t     width;           // +0x08  surface width in pixels
    int32_t     height;          // +0x0C  surface height in pixels
    uint8_t     has_palette;     // +0x10  if 1, palette_ptr is owned allocation
    uint8_t     flags;           // +0x11  misc flags
    // padding +0x12-0x13
    uint32_t*   palette_ptr;     // +0x14  128 uint32 entries (0x200 bytes), or shared ref
    uint8_t*    pixels;          // +0x18  width*height byte buffer (mode 0 only)
    void*       ddraw_surf;      // +0x1C  IDirectDrawSurface* (mode 1 only)
    void*       palette;         // shared/borrowed palette pointer (distinct from owned palette_ptr)
};

/* ================================================================ */
/* UIPANEL_Surface management functions (0x42A1xx range)            */
/* ================================================================ */
void  UIPANEL_CreateSurface(UIPANEL_Surface* surface);   /* @0x42A110 */
void* UIPANEL_DestroySurface(UIPANEL_Surface* surface, uint8_t flags); /* @0x42A140 */

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
void __cdecl DDRAW_PresentRect(const RECT* rect, HWND hWnd, int32_t offset_xy[2],
                               uint8_t use_color_key);


/* Ghidra naming artifact: LOCOBITMAP = PostcardAlbum */
typedef PostcardAlbum LOCOBITMAP;

#endif /* LOCOBITMAP_H */
