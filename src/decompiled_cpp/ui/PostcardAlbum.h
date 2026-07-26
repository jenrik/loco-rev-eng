/**
 * PostcardAlbum.h — Postcard collection album window
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * PostcardAlbum is the postcard collection album window where players view
 * received postcard images, browse tile screenshots by name, scroll through
 * album pages, and manage their collection. It extends UI_WindowBase.
 *
 * Size: 0x254 bytes
 * Vtable: 0x4773F0 (VTBL_POSTCARD_ALBUM)
 *
 * Class hierarchy:
 *   UI_WindowBase (VTBL_UI_WINDOWBASE, size 0xE8)
 *     └─ PostcardAlbum  <- this class (+0xE8..+0x254, total size 0x254)
 *
 * Vtable layout (0x4773F0, extends UI_WindowBase):
 *   [0]  +0x00: scalar deleting destructor (PostcardAlbum_DestroyFromResource, 0x401FB0)
 *   [1]  +0x04: Hide (overridden: calls DestroyWindow)
 *   [2]  +0x08: Show / Init (overridden: calls InitWindow, InitWindowSurface, InitSprites)
 *   [3]  +0x0C: center_viewport (inherited: UI_WindowBase_Reset, 0x425FD0)
 *   [4]  +0x10: virtual method (inherited stub, 0x426020)
 *   [5]  +0x14: virtual method (inherited stub, 0x426130)
 *   [6]  +0x18: CreateFullWindow (inherited: UI_CreateFullWindow, 0x425B70)
 *   [7]  +0x1C: OnCreate (overridden: PostcardAlbum_PaintWindow, 0x402690)
 *   [8]  +0x20: Render / update (PostcardAlbum_RenderAllTiles, 0x404AC0)
 *   [9]  +0x24: MouseWheel / HitTest (PostcardAlbum_HitTest, 0x403CD0)
 *   [10] +0x28: virtual method (inherited stub, 0x426140)
 *   [11] +0x2C: WindowProc (PostcardAlbum_PaintWindow, 0x402690)
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
#include "../ui/UI_WindowBase.h"

/* ================================================================== */
/* PostcardAlbum — postcard collection album window                     */
/* ================================================================== */

class PostcardAlbum : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* --- Inherited from UI_WindowBase (0x00..0xE8) --- */
    /* +0x00: void*      vtable                                       */
    /* +0x04: HINSTANCE  hInstance                                     */
    /* +0x08: HWND       hWnd                                          */
    /* +0x0C..+0xE7: see UI_WindowBase.h                               */
    /* +0xE4: uint8_t    visible                                       */

    /* --- Album-specific fields (+0xE8..+0x254) --- */

    void*      icon_handle;             // +0xE8  HICON for album window (resource 0x65)
    int32_t    blit_dest_x;             // +0xEC  destination X scroll offset for blits
    int32_t    blit_dest_y;             // +0xF0  destination Y scroll offset for blits

    uint8_t    window_surface_inited;   // +0xF0  1 = window surface initialized (flag_FC actually +0xFC
                                        //        but decompiler shows EC-F0-F4 ranges)

    uint8_t    sprites_visible;         // +0x110  1 = sprites visible/ready
    uint8_t    sprites_text_visible;    // +0x111  1 = text labels visible

    /* NOTE: Field offsets from decompiled code:
     * +0xFC = window_surface_inited (flag_FC)
     * +0x110 = sprites_visible (flag_sprites)
     * +0x111 = sprites_text_visible (flag_text)
     * +0x112 = text_rendered flag
     * +0x114 = current_scroll_offset (pixels)
     * +0x118 = current_tile_index
     * +0x11C = tiles_per_page
     * +0x120 = hovered_tile_index
     * +0x124 = available_tile_count (init to 9)
     * +0x128 = scroll_wheel_position
     * +0x12C = scroll_wheel_enabled (1 = enabled)
     * +0x130 = column_count
     * +0x134 = is_high_res   (0=800x600, 1=1024x768+)
     * +0x138 = album_bg_resource (res 0x3C0A or 0x3C0B)
     * +0x13C = album_bg_surface
     * +0x140 = photo_bg_resource (res 0x3CFA)
     * +0x144 = photo_bg_surface
     */

    uint8_t    flag_sprites;            // +0xF4 (actually +0xFC) — window surface init flag
    uint8_t    _pad_F5[0x1B];           // +0xF5 padding

    /* These offsets map from Ghidra decompiler output. Precise layout TBD. */
    uint8_t    flag_FC;                 // +0xFC  window_surface_inited guard

    uint8_t    _pad_FD[0x14];           // +0xFD to +0x110 padding

    uint8_t    sprites_inited;          // +0x111  1 = AlbumSprites initialized
    uint8_t    text_rendered;           // +0x112  1 = tile text labels rendered
    uint8_t    _pad_113;                // +0x113

    int32_t    scroll_pixel_offset;     // +0x114  current scroll pixel offset
    int32_t    tile_index;              // +0x118  current tile selection index
    int32_t    tiles_per_page;          // +0x11C  tiles per album page
    int32_t    hovered_tile;            // +0x120  currently hovered tile index
    int32_t    tile_count_init;         // +0x124  initial tile count (default 9)
    int32_t    scroll_wheel_pos;        // +0x128  scrollwheel position counter
    uint8_t    scroll_wheel_enabled;    // +0x12C  1 = scrollwheel navigation enabled
    uint8_t    _pad_12D[3];             // +0x12D

    int32_t    column_count;            // +0x130  album grid column count

    int32_t    is_high_res;             // +0x134  0=800x600 mode, 1=1024x768+ mode
                                         //          Determines resource 0x3C0A vs 0x3C0B

    void*      album_bg_resource;       // +0x138  album background resource pointer
    void*      album_bg_surface;        // +0x13C  album background surface

    void*      photo_bg_resource;       // +0x140  photo background resource (res 0x3CFA)
    void*      photo_bg_surface;        // +0x144  photo background surface

    /* Button sprites (8 x 0x24-byte ButtonSprite objects) */
    void*      btn_close;               // +0x148  close button sprite (res 0x3C04)
    void*      btn_delete;              // +0x14C  delete/trash sprite (res 0x3C09) — also acts as action 2
    void*      btn_save;                // +0x150  save button sprite (res 0x3C05) — action 3
    void*      btn_rotate;              // +0x154  rotate button sprite (res 0x3C08) — action 4
    void*      btn_print;               // +0x158  print button sprite (res 0x3C0F) — action 9
    void*      btn_prev;                // +0x15C  previous page sprite (res 0x3C06) — action 5
    void*      btn_next;                // +0x160  next page sprite (res 0x3C07) — action 6
    void*      btn_scrollwheel;         // +0x164  scrollwheel sprite (res 0x3C0C/0x3C0D) — action 7

    /* Row sprite groups (6 rows, each with 3 sprites) */
    /* Each row has: icon sprite, tile sprite, name-area sprite */
    /* Row field starts at +0x168. Each row occupies 0x14 bytes (3 sprite ptrs + name buf). */
    struct RowGroup {
        void* icon_sprite;              // +0x00  tile icon sprite (res varies)
        void* tile_sprite;              // +0x04  tile preview sprite (res 0x3C0E)
        void* name_sprite;              // +0x08  name/caption sprite (res varies)
        uint8_t flags;                  // +0x0C  per-row flags
        uint8_t _pad_0D[3];             // +0x0D padding
    };

    /* First row at +0x168:
     *   icon_sprite:  ptr at +0x168 (base) = +0x168
     *   tile_sprite:  ptr at +0x168 + 0x18 = +0x180  (offset D18 = -0x18 from +0x198)
     *   name_sprite:  ptr at +0x168 + 0x30 = +0x198
     * Next row at +0x168 + 0x14:
     *   icon_sprite:  +0x168 + 0x14 = +0x17C
     *   tile_sprite:  +0x180 + 0x14 = +0x194
     *   name_sprite:  +0x198 + 0x14 = +0x1AC
     *   etc.
     */

    /* From Ghidra decompilation:
       Row group structure:
         offset base = +0x168 + row * 4  (first sprite = icon)
         offset +0x18 = tile_sprite (+0x180 base)
         offset +0x24 = name field area +0x1DA
       Reconstructed as pointers at:
         icon_sprite[row]  = *(int*)(this + 0x168 + row*4)      — at +0x168
         name_buf[row]     = (char*)(this + 0x1DA + row*0x14)
       And tile_sprite[row] at:
         *(int*)(this + 0x180 + row*4)   — at +0x180 as offset 6 rows forward from +0x168
         name_sprite[row]  = *(int*)(this + 0x198 + row*4)      — at +0x198
       Row flags array:  *(byte*)(this + 0x1DA[row]) — actually name buffer start
    */

    /* 6 icon sprites at +0x168..+0x17C */
    void*      row_icon[6];             // +0x168  icon sprites for 6 album rows

    int32_t    _pad_start_178;          // +0x178

    /* 6 tile sprites at +0x180..+0x194 */
    void*      row_tile[6];             // +0x180  tile preview sprites for 6 rows

    int32_t    _pad_start_198;          // +0x198 broken in decomp; name sprites at +0x198 + row*4

    /* 9 tile label sprites at +0x1B0..+0x1D4 */
    void*      tile_label_sprites[9];   // +0x1B0  label sprites for individual tiles

    /* Row enabled flags (+0x1D4..+0x1D9) */
    uint8_t    row_enabled_0;           // +0x1D4  1 = row 0 navigation enabled
    uint8_t    row_enabled_1;           // +0x1D5  1 = row 1 navigation enabled
    uint8_t    row_enabled_2;           // +0x1D6  1 = row 2 navigation enabled
    uint8_t    row_enabled_3;           // +0x1D7  1 = row 3 navigation enabled
    uint8_t    row_enabled_4;           // +0x1D8  1 = row 4 navigation enabled
    uint8_t    row_enabled_5;           // +0x1D9  1 = row 5 navigation enabled

    /* Tile name buffers (6 x 0x14 = 0x78 bytes starting at +0x1DA) */
    char       tile_names[6][20];       // +0x1DA  tile name text for each row (null-terminated)

    /* Total size: 0x254 bytes (end = +0x1DA + 0x78 = +0x252 + 2 padding = +0x254) */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * PostcardAlbum constructor (factory).
     * Address: 0x401F50 (PostcardAlbum_CreateFromResource)
     *
     * Calls UI_WindowBase_Ctor(hInstance, resId), sets vtable to
     * VTBL_POSTCARD_ALBUM (0x4773F0), then calls InitFromResource to
     * initialize all album sprites and fields.
     *
     * Called by: CGWND_InitAllSubsystems
     *
     * @param hInstance  Application instance handle
     * @param resId      Window resource ID
     * @return           Pointer to constructed PostcardAlbum object (same as this)
     */
    static PostcardAlbum* Create(HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x401FB0 (PostcardAlbum_DestroyFromResource)
     *
     * Calls FreeAllSprites to release all resources, then optionally
     * frees the heap allocation via GLOBAL_free.
     *
     * @param flags  Delete flag (bit 0 = free heap memory)
     * @return       This pointer (after dtor)
     */
    void* Destroy(uint8_t flags);

    /* ================================================================ */
    /* Initialization                                                    */
    /* ================================================================ */

    /**
     * InitFromResource — Core initialization of all album fields and sprites.
     * Address: 0x401FD0 (__fastcall, ECX = this)
     *
     * Initializes all album-specific fields, creates 8 button sprites
     * (res 0x3C04-0x3C0F), 6 row sprite groups (x3 sprites each), and
     * 9 tile label sprites. Sets row enabled flags to 1. Detects
     * high-res mode (screen > 800x600). SEH-protected.
     *
     * Called from: PostcardAlbum::Create constructor
     */
    void InitFromResource();

    /**
     * InitWindow — Create the album child window.
     * Address: 0x402520 (__thiscall)
     *
     * Creates a full-desktop child window via UI_CreateFullWindow,
     * loads icon resource 0x65. Returns 1 on success.
     *
     * Called by: UI_WindowBase_OnCreate dispatch (vtable[1] / Show)
     *
     * @param hParent  Parent window HWND
     * @return         TRUE if window created successfully
     */
    bool InitWindow(HWND hParent);

    /**
     * InitWindowSurface — One-shot initialization of album background surface.
     * Address: 0x404720 (__fastcall, ECX = this)
     *
     * Loads background resource (0x3C0A for low-res, 0x3C0B for high-res),
     * caches at album_bg_resource + album_bg_surface. Guarded by
     * window_surface_inited flag at +0xFC.
     */
    void InitWindowSurface();

    /**
     * InitSprites — Initialize all album sprites via Sprite_Init.
     * Address: 0x404770 (__fastcall, ECX = this)
     *
     * Calls Sprite_Init on each of the 8 button sprites, 6 row icon sprites,
     * 6 row tile sprites, and 6 name sprites. Loads photo background resource
     * (0x3CFA) and surface. Sets sprites_inited flag. Guarded by flag.
     */
    void InitSprites();

    /* ================================================================ */
    /* Cleanup / teardown                                                */
    /* ================================================================ */

    /**
     * FreeSprites — Free album child sprites.
     * Address: 0x404830 (__fastcall, ECX = this)
     *
     * Destroys photo background resource, calls Sprite_Destroy on each
     * of the 8 button sprites, 6 row icon sprites, 6 row tile sprites,
     * and scrollwheel sprite. Clears sprites_inited flag. Guarded.
     */
    void FreeSprites();

    /**
     * DestroyWindow — Hide and destroy the album window.
     * Address: 0x402660 (__fastcall, ECX = this)
     *
     * Calls UI_WindowBase_Hide to hide, clears sprites_visible flag,
     * calls FreeSprites to release sprite resources.
     */
    void DestroyWindow();

    /**
     * FreeAllSprites — Full destructor body for PostcardAlbum.
     * Address: 0x402380 (__fastcall, ECX = this)
     *
     * Cleans up all album sprites: calls FreeSprites if sprites_inited,
     * destroys album_bg_resource via vtable[2], destroys each of the
     * 8 button sprites via vtable[0], 6 icon/tile/name sprites, 9 tile
     * label sprites, scrollwheel sprite, and album background sprite.
     * Then calls UI_WindowBase_BaseDtor for base cleanup.
     *
     * Called from: Destroy (scalar dtor)
     */
    void FreeAllSprites();

    /* ================================================================ */
    /* Rendering and event handling                                      */
    /* ================================================================ */

    /**
     * PaintWindow — Window message handler and WM_PAINT dispatcher (vtable[7]/[11]).
     * Address: 0x402690 (__thiscall)
     *
     * Handles keyboard shortcuts:
     *   0x0D = ENTER: hide album, set mode 3
     *   0x1B = ESC:   same as ENTER
     *   0x25 = VK_LEFT: scroll left / previous page
     *   0x27 = VK_RIGHT: scroll right / next page
     *
     * Scroll logic adjusts tile_index, scroll_pixel_offset, and page
     * navigation. Calls BlitElement + UpdateSprite for animation,
     * then RenderAllTiles + EndPaintEx to refresh display.
     *
     * For LEFT (0x25):
     *   If row 4 enabled (0x1D8==1): decrement tile_index + page scroll
     *   Else: scroll left by tiles_per_page
     * For RIGHT (0x27):
     *   If row 5 enabled (0x1D9==1): increment tile_index + page scroll
     *   Else: scroll right by tiles_per_page
     *
     * @param hWnd    Window handle
     * @param msg     Window message
     * @param wParam  WPARAM
     * @param lParam  LPARAM
     * @return        LRESULT from DefWindowProcA or 0
     */
    LRESULT PaintWindow(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * BlitElement — Render a specific album button/sprite element.
     * Address: 0x403E80 (__thiscall)
     *
     * Plays sound (0x5015) and blits the sprite region from the album
     * background surface to the primary surface. Element IDs:
     *   1 = btn_close   (action 1: close/exit)
     *   2 = btn_delete  (action 2: delete entry)
     *   3 = btn_save    (action 3: save postcard)
     *   4 = btn_rotate  (action 4: rotate view)
     *   5 = btn_prev    (action 5: previous page)
     *   6 = btn_next    (action 6: next page)
     *   7 = scrollwheel (action 7: scroll)
     *   9 = btn_print   (action 9: print)
     *
     * Each element checks its corresponding row_enabled flag before
     * rendering; if disabled, just sets sprite state to 2 (dimmed).
     *
     * @param element_id  Element action ID (1..9)
     */
    void BlitElement(int element_id);

    /**
     * UpdateSprite — Update sprite visual state for element.
     * Address: 0x403BA0 (__thiscall)
     *
     * Sets sprite to state 0 (normal) or 2 (dimmed) based on
     * corresponding row_enabled flag. Same element IDs as BlitElement.
     *
     * @param element_id  Element action ID (1..9)
     */
    void UpdateSprite(int element_id);

    /**
     * RenderTileName — Render a single tile name onto the album surface.
     * Address: 0x4048E0 (__thiscall)
     *
     * Looks up tile data via PixelDataCache_LookupAsset using scroll
     * offset + index. If data found, calls DPLAY_RenderPlayer to render
     * the player/tile preview into the row's sprite area. Copies player
     * name string into tile_names[row]. Sets row's icon sprite state.
     *
     * If data not found, blits the row's background area from album
     * background surface and clears the row name buffer.
     *
     * @param row_index  Tile row index (0..5)
     * @return           1 if rendered, 0 if cleared
     */
    uint8_t RenderTileName(int row_index);

    /**
     * RenderAllTiles — Render all visible tile names in the album.
     * Address: 0x404AC0 (__fastcall, ECX = this)
     *
     * Iterates tile_count (from tiles_per_page) and calls RenderTileName
     * for each. Then begins paint, sets text background/color, and draws
     * tile name text via DrawTextA for rendered tiles using the global
     * g_font_small. Updates navigation flags (row_enabled_4/5) based on
     * available tile count and scroll position. Calls UpdateSprite for
     * prev/next buttons to reflect scroll availability.
     */
    void RenderAllTiles();

    /**
     * HitTest — Hit-test album sprites for click dispatch (vtable[9]).
     * Address: 0x403CD0 (__thiscall)
     *
     * Tests click point (param_1, param_2) against each sprite rect.
     * Returns sprite/element ID:
     *   1  = btn_close sprite (+0x148)
     *   9  = btn_print sprite (+0x158)
     *   4  = btn_rotate sprite (+0x154)
     *   2  = btn_delete sprite (+0x14C)
     *   3  = btn_save sprite (+0x150)
     *   5  = btn_prev sprite (+0x15C)
     *   6  = btn_next sprite (+0x160)
     *   8  = row icon sprite (one of 6 at +0x168..+0x17C)
     *        sets hovered_tile (+0x120) to the matching index
     *   10 = row tile sprite (one of 6 at +0x180..+0x194)
     *        sets hovered_tile (+0x120) to the matching index
     *   7  = tile label sprite (one of 9 at +0x1B0..+0x1D4)
     *        sets hovered_tile (+0x120) to the matching index
     *   0  = no hit
     *
     * @param x  Screen X coordinate
     * @param y  Screen Y coordinate
     * @return   Hit result: 0 = none, 1..10 = sprite ID
     */
    int HitTest(int x, int y);
};
