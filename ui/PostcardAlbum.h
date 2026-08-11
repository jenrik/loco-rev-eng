// Status: INTEGRATED
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
 * Size: 0x254 bytes (allocated by CGWND_InitAllSubsystems @ 0x4072C8)
 * Vtable: 0x4773F0 (VTBL_POSTCARD_ALBUM)
 *
 * Class hierarchy:
 *   UI_WindowBase (VTBL_UI_WINDOWBASE, size 0xE8)
 *     └─ PostcardAlbum  <- this class (+0xE8..+0x254, total size 0x254)
 *
 * Vtable layout (0x4773F0, extends UI_WindowBase — verified via DATA xrefs):
 *   [0]  +0x00: scalar deleting destructor (PostcardAlbum_DestroyFromResource, 0x401FB0)
 *               -> ~PostcardAlbum -> FreeAllSprites
 *   [1]  +0x04: Hide / DestroyWindow (0x402660)            — overridden: hide()
 *   [2]  +0x08: Show (0x402590)                            — overridden: show()
 *               (dispatched by CGWND_SetMode(6) @ 0x408216)
 *   [3]  +0x0C: set_mode (inherited, 0x425FD0)
 *   [4]  +0x10: set_render_surface (inherited, 0x426020)
 *   [5]  +0x14: on_async_task_failure (inherited, 0x426130)
 *   [6]  +0x18: create_full_window (inherited, 0x425B70)
 *   [7]  +0x1C: on_create (overridden, 0x4028B0 — NOT yet decompiled;
 *               UI_WindowBase does not expose this slot as a C++ virtual)
 *   [8]  +0x20: render/update (overridden, 0x404DB0 — NOT yet decompiled;
 *               sets +0x112=1, blits the work rect, updates all element
 *               sprites, calls RenderAllTiles/EndPaintEx)
 *   [9]  +0x24: mousewheel slot (inherited no-op, 0x4661A0)
 *   [10] +0x28: (inherited, 0x426140)
 *   [11] +0x2C: WindowProc (overridden: GAMESTATE_WndProc, 0x40B4C0)
 *   [12] +0x30: UI_DefWndProc (0x422EA0)
 *   [13] +0x34: UI_DefWndProc (0x422EA0)
 *   [14] +0x38: click/key dispatch (0x404F60 — NOT yet decompiled;
 *               calls HitTest 0x403CD0 and dispatches element actions)
 *   [15] +0x3C: UI_DefWndProc (0x422EA0)
 *   [16] +0x40: OnTimerTick (0x4055E0 — operates on +0x130 sprite slot)
 *   [17] +0x44: UI_DefWndProc (0x422EA0)
 *   [18] +0x48: UI_DefWndProc (0x422EA0)
 *   [19] +0x4C: UI_DefWndProc (0x422EA0)
 *   [20] +0x50: HitTest (0x405680 — button rect tests, gated by +0x110)
 *   [21] +0x54: PaintWindow (0x402690)                      — this class
 *   [22] +0x58: UI_DefWndProc (0x422EA0)
 *   [23] +0x5C: (0x426950 — UIPANEL region, not yet decompiled)
 *   [24] +0x60: (0x405620 — not yet decompiled)
 *   [25] +0x64: UI_DefWndProc (0x422EA0)
 *   [26] +0x68: (0x426960 — UIPANEL region, not yet decompiled)
 *   [27] +0x6C: (0x426980 — UIPANEL region, not yet decompiled)
 *   [28] +0x70: (0x426A60 — UIPANEL region, not yet decompiled)
 *   [29] +0x74: UI_DefWndProc (0x422EA0)
 *   [30] +0x78: (0x426AC0 — UIPANEL region, not yet decompiled)
 *   [31] +0x7C: (0x426AD0 — UIPANEL region, not yet decompiled)
 *   [32] +0x80: (0x419A10 — Cursor region, not yet decompiled)
 *   [33]..[36]: UI_DefWndProc (0x422EA0)
 *
 * Note: the previously documented "[8] HitTest 0x403CD0 / [11] PaintWindow
 * 0x402690" slot mapping was WRONG. Verified vtable contents (0x4773F0
 * region): [8]=0x404DB0, [11]=GAMESTATE_WndProc 0x40B4C0, and
 * HitTest (0x403CD0) is a NON-virtual helper called by the slot-[14]
 * dispatch function; PaintWindow (0x402690) sits at slot [21].
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
#include "../ui/UI_WindowBase.h"
#include "../ui/ButtonSprite.h"

/* ================================================================== */
/* PostcardAlbum — postcard collection album window                     */
/* ================================================================== */

class PostcardAlbum : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* --- Inherited from UI_WindowBase (0x00..0xE8) --- */
    /* +0x00: vtable (compiler-managed)                                */
    /* +0x04: hInstance, +0x08: hWnd, +0x0C..+0xE7: see UI_WindowBase.h */
    /* +0xE4: visible                                                   */

    /* --- Album-specific fields (+0xE8..+0x254) --- */

    void*      icon_handle;             // +0xE8  HICON loaded by InitWindow (resource 0x65)
    int32_t    blit_dest_x;             // +0xEC  destination X offset for blits
    int32_t    blit_dest_y;             // +0xF0  destination Y offset for blits
    uint8_t    _pad_F4[8];              // +0xF4..+0xFB
    uint8_t    window_surface_inited;   // +0xFC  1 = album background surface created (one-shot guard)
    uint8_t    _pad_FD[19];             // +0xFD..+0x10F
    uint8_t    active_flag;             // +0x110  1 = album busy/active (set by the slot-[8] render
                                        //         when narration plays; PaintWindow/HitTest ignore
                                        //         input while set). Cleared by InitFromResource,
                                        //         show() and the HelpWnd hide path.
    uint8_t    sprites_inited;          // +0x111  1 = AlbumSprites initialized (InitSprites)
    uint8_t    text_rendered;           // +0x112  1 = tile text labels rendered (set by slot-[8],
                                        //         cleared by hide())
    uint8_t    _pad_113;                // +0x113
    int32_t    scroll_pixel_offset;     // +0x114  current scroll pixel offset
    int32_t    tile_index;              // +0x118  current tile selection index
    int32_t    tiles_per_page;          // +0x11C  tiles per album page
    int32_t    hovered_tile;            // +0x120  currently hovered tile index
    int32_t    tile_count_init;         // +0x124  initial tile count (default 9)
    int32_t    scroll_wheel_pos;        // +0x128  scrollwheel position counter
    uint8_t    scroll_wheel_enabled;    // +0x12C  1 = scrollwheel navigation enabled
    uint8_t    _pad_12D[3];             // +0x12D..+0x12F
    void*      tile_preview_sprite;     // +0x130  lazily-created sprite object, created outside
                                        //         this class's decompiled set (likely the
                                        //         selected-tile preview), freed by show() and
                                        //         FreeAllSprites via scalar-deleting dtor.
                                        //         NULL unless a tile selection created it.
    int32_t    is_high_res;             // +0x134  0=800x600 mode, 1=1024x768+ mode
                                        //          Determines resource 0x3C0A vs 0x3C0B
    void*      album_bg_resource;       // +0x138  album background resource (0x3C0A/0x3C0B)
    void*      album_bg_surface;        // +0x13C  album background surface (Lock(0,0) result)
    void*      photo_bg_resource;       // +0x140  photo background resource (0x3CFA)
    void*      photo_bg_surface;        // +0x144  photo background surface

    /* Button sprites (8 x 0x24-byte ButtonSprite objects) */
    ButtonSprite* btn_close;            // +0x148  close button sprite (res 0x3C04)
    ButtonSprite* btn_delete;           // +0x14C  delete/trash sprite (res 0x3C09) — element 2
    ButtonSprite* btn_save;             // +0x150  save button sprite (res 0x3C05) — element 3
    ButtonSprite* btn_rotate;           // +0x154  rotate button sprite (res 0x3C08) — element 4
    ButtonSprite* btn_print;            // +0x158  print button sprite (res 0x3C0F) — element 9
    ButtonSprite* btn_prev;             // +0x15C  previous page sprite (res 0x3C06) — element 5
    ButtonSprite* btn_next;             // +0x160  next page sprite (res 0x3C07) — element 6
    ButtonSprite* btn_scrollwheel;      // +0x164  scrollwheel sprite (res 0x3C0C/0x3C0D) — element 7

    /* Row sprite groups (three separate 6-element arrays created by
     * InitFromResource: icons at +0x168, tiles at +0x180, names at +0x198) */
    ButtonSprite* row_icon[6];          // +0x168  icon sprites for 6 album rows (res 0)
    ButtonSprite* row_tile[6];          // +0x180  tile preview sprites (res 0x3C0E)
    ButtonSprite* row_name[6];          // +0x198  tile-name sprites (res 0) — rect used for
                                        //         DrawTextA in RenderAllTiles; hit-test target 10
    ButtonSprite* tile_label_sprites[9];// +0x1B0  label sprites for individual tiles (res 0)

    /* Element enable flags (+0x1D4..+0x1D9) — byte per element; named by
     * element role, not by row:
     *   [0] +0x1D4 prev button (element 5), [1] +0x1D5 next button (element 6),
     *   [2] +0x1D6 delete button (element 2), [3] +0x1D7 rotate button (element 4),
     *   [4] +0x1D8 page-up scroll,          [5] +0x1D9 page-down scroll */
    uint8_t    row_enabled[6];          // +0x1D4..+0x1D9

    /* Tile name buffers (6 x 0x14 = 0x78 bytes starting at +0x1DA) */
    char       tile_names[6][20];       // +0x1DA  tile name text for each row (null-terminated)

    /* Total x86 size: 0x254 bytes (end = +0x1DA + 0x78 = +0x252 + 2 pad) */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * PostcardAlbum constructor.
     * (part of the factory PostcardAlbum_CreateFromResource, 0x401F50)
     *
     * Chains to UI_WindowBase(hInstance, resId), then calls
     * InitFromResource to initialize all album fields and sprites.
     *
     * @param hInstance  Application instance handle
     * @param resId      Window resource ID (0x1FB in the game)
     */
    PostcardAlbum(HINSTANCE hInstance, UINT resId);

    /**
     * Factory — construct a PostcardAlbum in pre-allocated memory.
     * Address: 0x401F50 (PostcardAlbum_CreateFromResource, __thiscall)
     *
     * The binary version calls UI_WindowBase_Ctor + InitFromResource on
     * memory supplied by the caller (CGWND_InitAllSubsystems allocates
     * the original x86's 0x254 bytes via operator_new; the host build
     * allocates operator_new(sizeof(PostcardAlbum)) == 0x328 instead, since
     * PostcardAlbum's pointer-bearing base fields widen on a 64-bit host).
     * This is placement-new construction.
     *
     * @param mem         Pre-allocated memory (operator_new(sizeof(PostcardAlbum)))
     * @param hInstance   Application instance handle
     * @param resId       Window resource ID
     * @return            Pointer to constructed PostcardAlbum (same as mem)
     */
    static PostcardAlbum* CreateFromResource(void* mem, HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x401FB0 (PostcardAlbum_DestroyFromResource)
     *
     * Calls FreeAllSprites; the compiler-emitted deleting destructor then
     * releases the heap allocation (GLOBAL_free) when delete is used.
     */
    virtual ~PostcardAlbum();

    /**
     * Hide / destroy the album window (vtable[1]).
     * Address: 0x402660 (PostcardAlbum_DestroyWindow, __fastcall)
     *
     * If the window is visible: calls UI_WindowBase::hide(), clears
     * text_rendered (+0x112) and calls FreeSprites.
     */
    void hide() override;

    /**
     * Show the album (vtable[2]).
     * Address: 0x402590 (dispatched by CGWND_SetMode(6) @ 0x408216)
     *
     * Sequence (verified from raw x86 bytes):
     *   InitSprites -> vtable[7] (0x4028B0, not yet decompiled) ->
     *   UI_WindowBase::show() -> ShowWindow(hWnd, SW_MAXIMIZE) ->
     *   SetFocus(hWnd) -> free +0x130 sprite if any -> set_mode(+0x60,
     *   +0x64, 0, 1) -> clear +0x110 -> restore scroll position from the
     *   PixelDataCache temp fields (insert_index/saved_album_index) and
     *   re-render all tiles -> reset the cache temp fields to -1.
     */
    void show() override;

    /* ================================================================ */
    /* Initialization                                                    */
    /* ================================================================ */

    /**
     * InitFromResource — Core initialization of all album fields and sprites.
     * Address: 0x401FD0 (__fastcall, ECX = this)
     *
     * Zeroes all album state, detects high-res mode, creates 8 button
     * sprites (res 0x3C04-0x3C0F), 6 row groups (icon/tile/name sprites),
     * 9 tile label sprites, and sets all element enable flags to 1.
     */
    void InitFromResource();

    /**
     * InitWindow — Create the album child window.
     * Address: 0x402520 (__thiscall)
     *
     * Creates a full-desktop child window via UI_CreateFullWindow,
     * loads icon resource 0x65 into +0xE8. Returns 1 on success.
     *
     * Shared: the binary also calls this function on the
     * PostcardPreviewWindow object (CGWND_InitAllSubsystems @ 0x407159).
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
     * caches at album_bg_resource + album_bg_surface (Lock(0,0)).
     * Guarded by window_surface_inited flag at +0xFC.
     */
    void InitWindowSurface();

    /**
     * InitSprites — Initialize all album sprites via Sprite_Init.
     * Address: 0x404770 (__fastcall, ECX = this)
     *
     * Calls Sprite_Init (ButtonSprite::init) on each of the 8 button
     * sprites and the 6 row TILE sprites (+0x180). Loads photo background
     * resource (0x3CFA) and surface. Sets sprites_inited flag.
     */
    void InitSprites();

    /* ================================================================ */
    /* Cleanup / teardown                                                */
    /* ================================================================ */

    /**
     * FreeSprites — Free album child sprites.
     * Address: 0x404830 (__fastcall, ECX = this)
     *
     * Releases photo background resource (Lock/Unlock pairing), calls
     * Sprite_Destroy (ButtonSprite::destroy) on each of the 8 button
     * sprites and the 6 row tile sprites. Clears sprites_inited flag.
     */
    void FreeSprites();

    /**
     * FreeAllSprites — Full destructor body for PostcardAlbum.
     * Address: 0x402380 (__fastcall, ECX = this)
     *
     * Calls FreeSprites if sprites_inited, releases album_bg_resource,
     * scalar-deletes all 8 button sprites, 6 icon/tile/name sprites,
     * 9 tile label sprites, the +0x130 sprite slot. Base cleanup runs
     * through the C++ destructor chain (UI_WindowBase::~UI_WindowBase).
     *
     * Called from: ~PostcardAlbum
     */
    void FreeAllSprites();

    /* ================================================================ */
    /* Rendering and event handling                                      */
    /* ================================================================ */

    /**
     * PaintWindow — Key handler for the album (vtable[21], 0x477444).
     * Address: 0x402690 (__thiscall)
     *
     * Guards on +0x110 (returns 0 while the album is busy). Handles:
     *   0x0D ENTER / 0x1B ESC: hide() + CGWND_SetMode(3)
     *   0x25 VK_LEFT:  allowed when row_enabled[0] is set; page-up when
     *                  row_enabled[4] set, else scroll left one page
     *   0x27 VK_RIGHT: allowed when row_enabled[1] is set; page-down when
     *                  row_enabled[5] set, else scroll right one page
     * Other messages fall through to DefWindowProcA. After any scroll the
     * tile grid is re-rendered (RenderAllTiles + UIPANEL_EndPaintEx).
     */
    /** Binary slot [21] 0x402690 (PaintWindow). */
    LRESULT on_key_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    /**
     * BlitElement — Render a specific album button/sprite element.
     * Address: 0x403E80 (__thiscall)
     *
     * Plays sound (0x5015) and blits the sprite region from the album
     * background surface to the primary surface. Element IDs:
     *   1 = btn_close, 2 = btn_delete, 3 = btn_save, 4 = btn_rotate,
     *   5 = btn_prev, 6 = btn_next, 7 = scrollwheel, 9 = btn_print.
     * Disabled elements (flag cleared) are dimmed via setState(2) and
     * skipped. Element 6 plays its sound AFTER the blit; element 7 uses
     * scroll_wheel_pos (+0x128) as the sprite state.
     */
    void BlitElement(int element_id);

    /**
     * UpdateSprite — Update sprite visual state for element.
     * Address: 0x403BA0 (__thiscall)
     *
     * Sets sprite to state 0 (normal) or 2 (dimmed) based on the
     * corresponding element enable flag:
     *   element 2 -> row_enabled[2], 4 -> row_enabled[3],
     *   5 -> row_enabled[0], 6 -> row_enabled[1]
     * The callee pops a second, unused stack argument (ret $8).
     */
    void UpdateSprite(int element_id);

    /**
     * RenderTileName — Render a single tile name onto the album surface.
     * Address: 0x4048E0 (__thiscall)
     *
     * Looks up tile data via PixelDataCache::LookupAsset using scroll
     * offset + index. If data found, calls DPLAY_RenderPlayer to render
     * the player/tile preview into the row's icon sprite area, copies
     * player name into tile_names[row], scalar-deletes the tile entry,
     * and sets row tile sprite state to 0. If data not found, clears the
     * row name buffer and blits the row's background area from the album
     * background surface.
     *
     * @param row_index  Tile row index (0..5)
     * @return           1 if rendered, 0 if cleared
     */
    uint8_t RenderTileName(int row_index);

    /**
     * RenderAllTiles — Render all visible tile names in the album.
     * Address: 0x404AC0 (__fastcall, ECX = this)
     *
     * Phase 1: blits each row's name-sprite area (+0x198) from the album
     * background. Phase 2: if scroll_wheel_enabled (+0x12C), draws the
     * tile names via DrawTextA with DT_SINGLELINE|DT_VCENTER|DT_CENTER
     * into the name-sprite rects using g_font_small. Phase 3: updates the
     * prev/next enable flags and sprite states from scroll position and
     * entry count.
     */
    void RenderAllTiles();

    /**
     * HitTest — Hit-test album sprites for click dispatch.
     * Address: 0x403CD0 (__thiscall, NON-virtual helper)
     *
     * Tests click point (x, y) against each sprite rect. Returns:
     *   1  = btn_close (+0x148), 9 = btn_print (+0x158),
     *   4  = btn_rotate (+0x154), 2 = btn_delete (+0x14C),
     *   3  = btn_save (+0x150), 5 = btn_prev (+0x15C),
     *   6  = btn_next (+0x160),
     *   8  = row icon sprite (+0x168..+0x17C) -> hovered_tile = index,
     *   10 = row name sprite (+0x198..+0x1AC) -> hovered_tile = index,
     *   7  = tile label sprite (+0x1B0..+0x1D4) -> hovered_tile = index,
     *   0  = no hit
     *
     * @param x  Screen X coordinate
     * @param y  Screen Y coordinate
     * @return   Hit result: 0 = none, 1..10 = sprite ID
     */
    int HitTest(int x, int y);
};
