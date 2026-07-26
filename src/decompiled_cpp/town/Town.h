/**
 * Town.h — Main gameplay view (isometric town, building selection, postcard UI)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Town is the primary in-game view. It manages:
 *   - Isometric tile map viewport scrolling and rendering
 *   - Building selection/highlight overlay
 *   - Building placement and tracking
 *   - Postcard creation, sending, receiving, and album management
 *   - Network session management for multiplayer postcard exchange
 *   - Cursor indicator sprites for valid/invalid placement feedback
 *
 * Town extends UI_WindowBase (not GameWindow/Entity). It is instantiated
 * as the global g_town during CGWND_InitAllSubsystems (allocated 0x6E0 bytes).
 * The postcard preview dialog is a separate class (PostcardPreviewWindow,
 * vtable 0x4778F8/0x477E20, allocated 0x2C4 bytes).
 *
 * Town also contains a nested GameView/ScrollView sub-object (vtable 0x477D30)
 * at global g_town_view that manages viewport scrolling and camera position.
 *
 * Size: 0x6E0 bytes (Town-specific fields from +0xE8 to +0x6D8)
 * Vtable: 0x477D88 (Town vtable, extends VTBL_UI_WINDOWBASE)
 *
 * Class hierarchy:
 *   UI_WindowBase (VTBL_UI_WINDOWBASE, +0x000..+0xE8, size 0xE8)
 *     └─ Town  <- this class (+0xE8..+0x6E0, total size 0x6E0)
 *
 * NOTE: The region +0xE9..+0x5ED serves double duty as the postcard data/filename
 * buffer (0x504 bytes) during postcard save/load operations, overlapping with
 * viewport, overlay, and panel fields. Postcard operations (save_postcard_as,
 * save_received_postcard) use this buffer via ReadFile/OPENFILENAME and will
 * corrupt viewport state — this is safe because these operations only occur
 * during postcard UI transitions when the viewport is not being rendered.
 *
 * Vtable layout (0x477D88 inherits UI_WindowBase slots [0]..[11], overrides [0]):
 *   [0]  +0x00: scalar deleting destructor (Town_Dtor, 0x42E960)
 *   [1]  +0x04: Hide                    (inherited: UI_WindowBase_Hide,    0x425990)
 *   [2]  +0x08: Show                    (inherited: UI_WindowBase_Show,    0x4259C0)
 *   [3]  +0x0C: center_viewport         (inherited: UI_WindowBase v3,      0x425FD0)
 *   [4]  +0x10: virtual method          (inherited: UI_WindowBase v4,      0x426020)
 *   [5]  +0x14: virtual method          (inherited: UI_WindowBase v5,      0x426130)
 *   [6]  +0x18: CreateFullWindow        (inherited: UI_CreateFullWindow,   0x425B70)
 *   [7]  +0x1C: OnCreate                (inherited: UI_WindowBase_OnCreate,0x425D30)
 *   [8]  +0x20: PostcardInitList        (overridden: Town_PostcardInitList,0x42E420)
 *   [9]  +0x24: PostcardHitTest         (overridden: Town_HitTest,         0x42FFF0)
 *   [10] +0x28: PreviewDlgProc          (overridden: Town_PostcardDrawPreview, 0x42F810)
 *   [11] +0x2C: LoadBackground / WndProc (overridden: Town_LoadBackground, 0x42EE20)
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
#include "../ui/UI_WindowBase.h"

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

class Town;

/* ================================================================== */
/* Town class                                                          */
/* ================================================================== */

class Town : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */
    /* --- Inherited from UI_WindowBase (see UI_WindowBase.h for full list) --- */
    /* +0x00: vtable is compiler-managed */
    /* +0x04: HINSTANCE            hInstance                            */
    /* +0x08: HWND                 hWnd                                 */
    /* ... see UI_WindowBase.h for +0x0C..+0xE7 ...                    */
    /* +0xE4: uint8_t              visible (base class)                  */

    /* --- Selection and building tracking (+0x88..+0x194) --- */
    /* +0x88 is repurposed from unused bytes in base class title buffer */
    uint8_t    selection_active;        // +0x88  1 = building is selected
    uint8_t    _pad_89[7];              // +0x89  padding to +0x90

    uint8_t    postcard_click_flag;     // +0x90  click suppression flag (PostcardClickHandler)
    uint8_t    _pad_91[63];             // +0x91  padding

    void*      children_list;           // +0xD0  linked list head of child GameObject objects

    /* Cursor indicator sprite duplicates (stored by handle_tile_click alongside +0x170/+0x174) */
    void*      cursor_valid_dup;        // +0xD8  dup of cursor_valid_sprite (res 0x3807)
    void*      cursor_invalid_dup;      // +0xDC  dup of cursor_invalid_sprite (res 0x3808)

    void*      selected_building;       // +0xE0  pointer to currently selected building
    void*      child_panel;             // +0xE4  child panel/GameObject (also serves as postcard_init flag)

    /* flag_E8 + POSTCARD DATA BUFFER OVERLAY: +0xE9..+0x5ED (0x504 bytes) */
    uint8_t    flag_E8;                 // +0xE8  dialog guard flag (set=1 during GetSaveFileNameA)
    /* +0xE9..+0x5ED: Overlaps with viewport/overlay/panel fields below.
       During postcard save operations (save_postcard_as, save_received_postcard),
       this 0x504-byte region serves as the postcard data buffer and OPENFILENAME
       filename buffer. The overlapping fields are invalid during save operations. */

    /* Viewport inset rect — defines visible area within the Town window */
    int32_t    viewport_inset_left;     // +0xEC  viewport clip inset from window left
    int32_t    viewport_inset_top;      // +0xF0  viewport clip inset from window top
    int32_t    viewport_inset_right;    // +0xF4  viewport clip inset from window right
    int32_t    viewport_inset_bottom;   // +0xF8  viewport clip inset from window bottom

    /* OVERLAY: These are overlapped by the postcard data buffer during save operations */

    uint8_t    _pad_FC[24];             // +0xFC  padding

    /* Overlay destination rect — selection highlight on screen */
    int32_t    overlay_dest_left;       // +0x114  destination screen X for overlay
    int32_t    overlay_dest_top;        // +0x118  destination screen Y for overlay
    int32_t    overlay_dest_right;      // +0x11C  destination screen right for overlay
    int32_t    overlay_dest_bottom;     // +0x120  destination screen bottom for overlay

    void*      panel_graphics;          // +0x124  panel graphics/surface object
    uint8_t    _pad_128[68];            // +0x128  padding

    uint16_t   selected_building_type;  // +0x16C  type of selected building (6=depot, 7=remove)

    /* Cursor indicator sprites (stored by handle_tile_click, overloads padding) */
    void*      cursor_valid_sprite;     // +0x170  valid-placement cursor sprite (res 0x3807)
    void*      cursor_invalid_sprite;   // +0x174  invalid-placement cursor sprite (res 0x3808)

    void*      track_piece;             // +0x178  CGWND_TrackPiece zoom control
                                        //         (also cursor_hover_sprite res 0x3806 in handle_tile_click)
    void*      overlay_panel;           // +0x17C  UIPANEL_Surface for selection overlay

    /* Backup surface data — used for restoring background on deselect */
    uint32_t   backup_surface;          // +0x180  backup surface pointer
    uint32_t   backup_x;                // +0x184  backup region source X
    int32_t    backup_y;                // +0x188  backup region source Y
    uint32_t   backup_width;            // +0x18C  backup region width
    int32_t    building_center_x;       // +0x190  cached building center X (for move detection)
    int32_t    building_center_y;       // +0x194  cached building center Y (for move detection)

    uint8_t    _pad_198[0x5ED - 0x198]; // +0x198  padding

    /* --- Postcard UI fields (+0x5ED..+0x6D6) --- */
    uint8_t    audio_playing;           // +0x5ED  flag: 1 = postcard open/close sound playing
    uint8_t    _pad_5EE[2];            // +0x5EE  padding

    int32_t    timer_counter;           // +0x5F0  paint-throttle frame counter
    void*      icon_handle;             // +0x5F4  HICON for window (resource 0x65)

    uint8_t    postcard_active;         // +0x5F8  1 = postcard UI is open/active
    uint8_t    sprites_initialized;     // +0x5F9  1 = 8 postcard sprites + 3 child windows created
    uint8_t    overlay_initialized;     // +0x5FA  1 = overlay sprite (res 0x3cf7) initialized
    uint8_t    _pad_5FB;               // +0x5FB  padding

    int32_t    timer_active;            // +0x5FC  timer ID for 0x4D (postcard refresh timer)
    int32_t    frame_counter;           // +0x600  postcard paint-throttle frame count

    uint8_t    net_update_flag;         // +0x604  1 = need to update network state
    uint8_t    repaint_requested;       // +0x605  paint throttle flag (1 = repaint available)
    uint8_t    is_host;                 // +0x606  1 = this player is the game host
    uint8_t    has_remote_players;      // +0x607  1 = remote players are connected
    uint8_t    _pad_608[4];             // +0x608 padding

    void*      selected_player;         // +0x60C  current selected player/crd entry pointer
    void*      postcard_data;           // +0x610  postcard receive data / player info

    uint8_t    player_count_flag;       // +0x614  flag from player count (init to 1)

    /* Postcard overlay geometry */
    int32_t    postcard_origin_x;       // +0x618  postcard overlay origin X on screen
    int32_t    postcard_origin_y;       // +0x61C  postcard overlay origin Y on screen
    int32_t    postcard_width;          // +0x620  postcard overlay width
    int32_t    postcard_height;         // +0x624  postcard overlay height

    int32_t    render_param_x;          // +0x628  postcard player render position X
    int32_t    render_param_y;          // +0x62C  postcard player render position Y
    uint32_t   render_extra;            // +0x630  postcard render extra parameter
    void*      player_rect;             // +0x634  pointer to player render RECT

    uint8_t    _pad_638[12];            // +0x638  padding

    /* Postcard overlay resources */
    void*      overlay_resource;        // +0x644  overlay background resource (res 0x3cf7)
    void*      overlay_surface;         // +0x648  overlay surface handle

    void*      background_resource;     // +0x64C  postcard background resource (res 0x3cf8)
    void*      background_surface;      // +0x650  postcard background surface

    /* Send animation area */
    int32_t    send_rect_left;          // +0x654  send animation source rect left
    int32_t    send_rect_top;           // +0x658  send animation source rect top
    int32_t    send_rect_right;         // +0x65C  send animation source rect right
    int32_t    send_rect_bottom;        // +0x660  send animation source rect bottom

    /* Button strip */
    void*      button_strip_resource;   // +0x664  button strip resource (res 0x3cfb)
    void*      button_strip_surface;    // +0x668  button strip surface

    int32_t    button_src_left;         // +0x66C  button strip source rect left
    int32_t    button_src_top;          // +0x670  button strip source rect top
    int32_t    button_src_right;        // +0x674  button strip source rect right
    int32_t    button_src_bottom;       // +0x678  button strip source rect bottom

    RECT       button_hit_rect_send;    // +0x67C  hit-test rect for send button (button ID 9)

    /* Postcard preview area */
    RECT       preview_rect;            // +0x68C  postcard image area (source for blit)

    /* Send confirm */
    void*      send_confirm_resource;   // +0x69C  send confirmation resource (res 0x3cfa)
    void*      send_confirm_surface;    // +0x6A0  send confirmation surface

    /* Postcard sprites (8 UISprite pointers, res 0x3cf0-0x3cf9) */
    void*      sprite_btn_close;        // +0x6A4  button 1 sprite (res 0x3cf0)
    void*      sprite_btn_options;      // +0x6A8  button 2 sprite (res 0x3cf1)
    void*      sprite_btn_rotate;       // +0x6AC  button 3 sprite (res 0x3cf2)
    void*      sprite_btn_save;         // +0x6B0  button 4 sprite (res 0x3cf3)
    void*      sprite_inbox;            // +0x6B4  inbox preview sprite (res 0x3cac)
    void*      sprite_inbox_counter;    // +0x6B8  inbox message count sprite (res 0x3cf5)
    void*      sprite_outbox_counter;   // +0x6BC  outbox message count sprite (res 0x3cf6)
    void*      sprite_send;             // +0x6C0  send button sprite (res 0x3cf9)

    /* Sprite state look-up tables (ushort arrays, indexed by message count) */
    uint16_t   inbox_state_lut[5];      // +0x6C4  sprite state IDs for inbox counter (capped at 4)
    uint16_t   outbox_state_lut[5];     // +0x6CE  sprite state IDs for outbox counter (capped at 4)

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * Town constructor.
     * Address: 0x42E900
     *
     * Calls UI_WindowBase::UI_WindowBase(hInstance, 0x1F5) to initialize
     * base fields, sets vtable to VTBL_TOWN (0x477D88), then calls
     * Town_BaseCtor to initialize Town-specific fields and create
     * 8 postcard sprites (res 0x3cf0..0x3cf9).
     *
     * Called by: CGWND_InitAllSubsystems @ 0x407054
     *
     * @param hInstance  Application instance handle (from CGWND)
     * @param resId      Window resource ID (0x1F5)
     */
    Town(HINSTANCE hInstance, UINT resId);

    /**
     * Base constructor body — initializes all Town-specific fields.
     * Address: 0x42E980
     *
     * Called from Town::Town after base class init. Zeroes all fields,
     * creates 8 UISprite objects via RESDATA_CreateSpriteObject, and
     * initializes the sprite state look-up tables at +0x6C4.
     */
    void base_ctor();

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x42E960
     *
     * Calls Town_Destroy, then optionally frees via GLOBAL_free.
     *
     * @param flags  Delete flag (bit 0 = free heap memory)
     * @return       This pointer (after dtor)
     */
    virtual ~Town();

    /**
     * Full destructor body — releases all Town resources.
     * Address: 0x42EC10
     *
     * Resets vtable, destroys overlay notification window, destroys
     * all 8 postcard sprites and 3 child resources (background,
     * button strip, send confirm), then calls UI_WindowBase::base_destructor().
     */
    void destroy();

    /* ================================================================ */
    /* Window lifecycle methods                                          */
    /* ================================================================ */

    /**
     * InitSprites — Create the full-desktop window for town view.
     * Address: 0x42EDB0
     *
     * Creates the Town child window via UI_CreateFullWindow, sized
     * to the full desktop. Loads icon 0x65. Called once from
     * CGWND_InitAllSubsystems after construction.
     *
     * @param hParent  Parent window HWND (the CGWND window)
     * @return         1 on success, 0 on failure
     */
    bool init_sprites(HWND hParent);

    /**
     * InitTileClickSprites — Create placement cursor indicator sprites.
     * Address: 0x42CE10
     *
     * MISNAMED: This is NOT a click handler. Creates 3 cursor indicator
     * sprites (valid=0x3807, invalid=0x3808, hover=0x3806) and an overlay
     * UIPANEL surface for placement feedback. Loads animation resources
     * 0x3805 and 0x3804. Returns 1 on success.
     *
     * Field overloading: Stores sprites at +0x170 (cursor_valid_sprite),
     * +0x174 (cursor_invalid_sprite), +0x178 (cursor_hover_sprite, overloading
     * track_piece), with duplicates at +0xD8/+0xDC. Also stores the overlay
     * panel at +0x17C and initializes backup_surface rect at +0x180 using
     * overlay_panel dimensions via SetRect.
     *
     * @return  1 if all resources loaded successfully, 0 on failure
     */
    char handle_tile_click();

    /* ================================================================ */
    /* Tile rendering primitives (used by UIPANEL_Blit dispatch)        */
    /* ================================================================ */

    /**
     * BlitElement — Extract surface from element and forward to UIPANEL_Blit.
     * Address: 0x42B960
     *
     * Thin wrapper called by UI element renderers (EditWindow, Cursor, etc.).
     * Reads the DirectDraw surface pointer from element+0x1C and passes it
     * as the dest_surface parameter to UIPANEL_Blit. The remaining parameters
     * pass through unchanged.
     *
     * NOTE: `this` in this method is the source UIPANEL surface context
     * (not the Town instance itself). The same calling convention applies
     * to all tile rendering methods: ECX = tile cache context.
     *
     * Called by: EditWindow_render, Cursor_InitBackground,
     *            CGWND_TrackPiece_Render, UIPANEL_DrawButton
     *
     * @param dest_x         int — destination X on 16-bit surface
     * @param dest_y         int — destination Y on 16-bit surface
     * @param dest_w         int — destination width (unused by wrapper)
     * @param dest_h         int — destination height (unused by wrapper)
     * @param element        void* — element struct with surface at +0x1C
     * @param clip_left      int — clip rect left
     * @param clip_top       int — clip rect top
     * @param clip_right     int — clip rect right
     * @param clip_bottom    int — clip rect bottom
     * @param flags          uint32_t — blit flags passed to UIPANEL_Blit
     */
    void BlitElement(
        int dst_l, int dst_t, int dst_r, int dst_b,
        void* element, int src_x, int src_y,
        int src_w, int src_h, unsigned int flags);

    /**
     * InitTileCache — Copy 8bpp indexed source to 16bpp dest through palette.
     * Address: 0x42B9C0
     *
     * The tile cache context (`this`) holds:
     *   +0x08: stride — bytes per row in 8bpp source buffer
     *   +0x14: palette — uint16_t[256] color lookup table
     *   +0x18: pixels — uint8_t* 8-bit indexed tile pixel data
     *
     * Reads source byte at each pixel position, looks up the 16-bit color
     * in this->palette[source_byte], and writes it to the destination surface.
     * Unlike DrawTile, this function does NOT implement transparency —
     * palette index 0 is treated as a normal color.
     *
     * Called by: UIPANEL_Blit (flags=0x01, 0x03)
     *
     * @param dest_x         int — destination X on 16-bit surface (pixels)
     * @param dest_y         int — destination Y on 16-bit surface (pixels)
     * @param dest_w         int — (unused, rect width from dispatcher)
     * @param dest_h         int — (unused, rect height from dispatcher)
     * @param dest_base      uintptr_t — locked 16-bit surface base address
     * @param dest_pitch     uint32_t — destination surface pitch (bytes per row)
     * @param clip_left      int — source rect left (index into tile cache)
     * @param clip_top       int — source rect top
     * @param clip_right     int — source rect right (exclusive)
     * @param clip_bottom    int — source rect bottom (exclusive)
     * @return               1 on success, 0 if cache has no pixels/palette
     */
    uint8_t init_tile_cache(
        int dest_x, int dest_y, int dest_w, int dest_h,
        uintptr_t dest_base, uint32_t dest_pitch,
        int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * DrawTile — Draw 8bpp indexed tile with palette remap + transparency.
     * Address: 0x42BA90
     *
     * Core tile drawing function. For each pixel:
     *   1. Save the destination pixel value temporarily to palette[0]
     *   2. Read the source byte (8-bit index from tile cache)
     *   3. Write palette[source_byte] to the destination
     *
     * Since palette[0] is overwritten with the original destination pixel
     * before the palette lookup, source index 0 reads back the original
     * destination value = transparent pass-through. This avoids a per-pixel
     * conditional branch.
     *
     * Parameters match the UIPANEL_Blit dispatch convention (RET 0x28).
     *
     * Called by: UIPANEL_Blit (flags=0x00, and default/0x0F fallback)
     *
     * @param dest_x         int — destination X on 16-bit surface (pixels)
     * @param dest_y         int — destination Y on 16-bit surface (pixels)
     * @param dest_w         int — (unused, rect width from dispatcher)
     * @param dest_h         int — (unused, rect height from dispatcher)
     * @param dest_base      intptr_t — locked 16-bit surface base address
     * @param dest_pitch     uint32_t — destination surface pitch (bytes per row)
     * @param clip_left      int — source rect left (index into tile cache)
     * @param clip_top       int — source rect top
     * @param clip_right     int — source rect right (exclusive)
     * @param clip_bottom    int — source rect bottom (exclusive)
     * @return               1 on success, 0 if cache uninitialized
     */
    uint8_t draw_tile(
        int dest_x, int dest_y, int dest_w, int dest_h,
        intptr_t dest_base, uint32_t dest_pitch,
        int clip_left, int clip_top, int clip_right, int clip_bottom);

    /**
     * FlushTileCache — Expand each source pixel to 2x2 block on dest.
     * Address: 0x42BB90
     *
     * Each 8-bit source byte produces 4 identical 16-bit pixels in a 2x2
     * block on the destination surface:
     *   [P] [P]    where P = palette[source_byte]
     *   [P] [P]
     *
     * Source byte value 0 = skip (transparent — preserves destination).
     * The source buffer is read with stride = (clip_right - clip_left)
     * and the inner loop runs `clip_right` columns wide.
     *
     * Called by: UIPANEL_Blit (flags=0x04, 0x84)
     *
     * @param dest_x         int — destination X on 16-bit surface (pixels)
     * @param dest_y         int — destination Y on 16-bit surface (pixels)
     * @param dest_w         int — (unused, rect width)
     * @param dest_h         int — (unused, rect height)
     * @param dest_base      uintptr_t — locked 16-bit surface base address
     * @param dest_pitch     uint32_t — destination surface pitch (bytes per row)
     * @param clip_left      int — clip left margin (added to width for inner loop)
     * @param clip_top       int — (unused as offset, row starts at 0)
     * @param clip_right     int — clip right (width = clip_right - clip_left)
     * @param clip_bottom    uint32_t — row count (directly, not rect bottom)
     * @return               1 on success
     */
    uint8_t flush_tile_cache(
        int dest_x, int dest_y, int dest_w, int dest_h,
        uintptr_t dest_base, uint32_t dest_pitch,
        int clip_left, int clip_top, int clip_right, uint32_t clip_bottom);

    /**
     * DrawCachedTile — Draw from cache with 2x2 expansion, no transparency.
     * Address: 0x42BC80
     *
     * Same 2x2 block expansion as FlushTileCache but WITHOUT the transparent
     * skip on palette index 0. Every source byte is rendered as a 2x2 block.
     * Used for regions that were pre-cleared or have no transparency.
     *
     * Source buffer layout: stride = (clip_right - clip_left), inner loop
     * runs `clip_right` columns per row.
     *
     * Called by: UIPANEL_Blit (flags=0x05, 0x85)
     *
     * @param dest_x         int — destination X on 16-bit surface (pixels)
     * @param dest_y         int — destination Y on 16-bit surface (pixels)
     * @param dest_w         int — (unused, rect width)
     * @param dest_h         int — (unused, rect height)
     * @param dest_base      uintptr_t — locked 16-bit surface base address
     * @param dest_pitch     uint32_t — destination surface pitch (bytes per row)
     * @param clip_left      int — clip left margin (added to width for inner loop)
     * @param clip_top       int — (unused as offset, row starts at 0)
     * @param clip_right     int — clip right (width = clip_right - clip_left)
     * @param clip_bottom    uint32_t — row count (directly, not rect bottom)
     * @return               1 on success
     */
    uint8_t draw_cached_tile(
        int dest_x, int dest_y, int dest_w, int dest_h,
        uintptr_t dest_base, uint32_t dest_pitch,
        int clip_left, int clip_top, int clip_right, uint32_t clip_bottom);

    /* ================================================================ */
    /* Building selection and tracking                                   */
    /* ================================================================ */

    /**
     * SelectBuilding — Select/focus a building in the town view.
     * Address: 0x42D040
     *
     * Validates the building entity, centers the viewport on it, sets
     * zoom level (1 for depot/type 6, 3 for others), invalidates tile
     * rect, notifies DDRAW. Called with NULL to deselect (clears flag,
     * resets panels, hides UI).
     *
     * Called by: Town_TrackBuilding (auto-deselect on invisible depot),
     *            UI building click handlers, Town_SendPostcard lifecycle
     *
     * @param building  Building entity pointer, or NULL to deselect
     * @return          this->selection_active (1 if selected, 0 if deselected)
     */
    byte select_building(void* building);

    /**
     * DeselectBuilding — Remove building selection overlay.
     * Address: 0x42D280
     *
     * Computes a clip rect from viewport_inset and overlay dimensions,
     * intersected with the viewport rect. Blits cached background pixels
     * from the backup surface back to the primary surface, then re-blits
     * the UI panel overlay. Handles type 7 (remove tool) differently
     * from other building types.
     */
    void deselect_building();

    /**
     * TrackBuilding — Per-frame tracking of selected building.
     * Address: 0x42D1A0
     *
     * Called every frame from GameLoop_FrameUpdate when selection is
     * active. Auto-deselects invisible depot (type 6). Checks if building
     * center moved and re-centers viewport. Iterates children list and
     * updates child panel.
     */
    void track_building();

    /**
     * UpdateSelection — Blit the selection overlay panel onto the primary surface.
     * Address: 0x42D3A0
     *
     * Copies from overlay_panel (source at viewport_inset) to the
     * primary surface (dest at overlay_dest rect) with scroll-aware flag 0x40.
     * Called from TileMap_ProcessRect after overlay rect intersects dirty region.
     */
    void update_selection();

    /**
     * RenderSelection — Draw selection highlight for a single tile.
     * Address: 0x42D400
     *
     * If selection_active is set, calls GameObject_Draw on this
     * (through vtable dispatch) with the tile rect.
     * Called from TileMap_ProcessRect for each occupied tile in a dirty rect.
     */
    void render_selection();

    /**
     * IsValidPlacement — Check if a building/track can be placed.
     * Address: 0x42CF90 (__cdecl, not a method)
     *
     * Validates entity non-NULL + active, checks tile type byte:
     *   0x07 = always valid
     *   0x02/0x06/0x08 = visible
     *   0x04 = connected
     *   0x03 = building tile
     *   0x0C = large ID (> 0x300F)
     *
     * @param entity  Entity to check
     * @return        true if valid placement location
     */
    static bool is_valid_placement(void* entity);

    /* ================================================================ */
    /* Viewport / tile occupancy checks                                  */
    /* ================================================================ */

    /**
     * CheckOccupied — Scan tile buffer for occupied tiles.
     * Address: 0x42C950 (__thiscall)
     *
     * Two modes depending on this+0x04:
     *   Mode 0 (this+0x04 == 0): Scans byte array at this+0x18 with stride
     *     this+0x08, scanning rect [x1..x2, y1..y2) for any non-zero byte.
     *     Returns 1 if any tile is non-zero (occupied), 0 otherwise.
     *   Mode 1 (this+0x04 != 0): Delegates to Town_CheckOccupiedEx for
     *     DDraw surface pixel-level check.
     *
     * Called by: BuildingMgr_InvalidateRects, BuildingMgr_BlitOverlaps
     *
     * @param x1  Left coordinate of scan rect
     * @param y1  Top coordinate of scan rect
     * @param x2  Right coordinate of scan rect (exclusive)
     * @param y2  Bottom coordinate of scan rect (exclusive)
     * @return    1 if any tile in rect is occupied, 0 if all empty
     */
    uint8_t check_occupied(int x1, int y1, int x2, int y2);

    /**
     * CheckOccupiedEx — Extended tile occupancy via DDraw surface lock.
     * Address: 0x42C9F0 (__stdcall)
     *
     * NOT a Town method — called from CheckOccupied when mode=1.
     * Locks the global primary DirectDraw surface, scans 16-bit pixels
     * in rect [x1..x2, y1..y2). A pixel is considered "occupied" when:
     *   ((pixel & g_mask1) >> g_shift) != 0x1f AND (pixel & g_mask2) != 0x1f
     * This detects non-water pixels (water = 0x1f in both channels).
     *
     * Manages surface lock/unlock with loss recovery via g_surface_lost flag.
     *
     * @param x1  Left coordinate
     * @param y1  Top coordinate
     * @param x2  Right coordinate (exclusive)
     * @param y2  Bottom coordinate (exclusive)
     * @return    1 if occupied (any pixel is non-water), 0 if empty
     */
    static uint8_t check_occupied_ex(int x1, int y1, int x2, int y2);

    /**
     * BlitViewport — Viewport occupancy check for collision detection.
     * Address: 0x42CB10 (__thiscall)
     *
     * Tests pixel (x,y) against viewport bounds [x1..x2, y1..y2].
     * Outside bounds = always passable (returns 1).
     * Mode 0 (this+0x04 != 1): checks byte index buffer at this+0x18 with stride this+0x08.
     *   Returns 1 if byte is zero (passable), 0 if non-zero (occupied).
     * Mode 1 (this+0x04 == 1): locks DDraw surface from this+0x1C, reads 16-bit
     *   pixel, checks water/dirt bit pattern. Returns 1 = passable (water),
     *   0 = occupied (non-water).
     *
     * @param x1  Viewport left bound
     * @param y1  Viewport top bound
     * @param x2  Viewport right bound
     * @param y2  Viewport bottom bound
     * @param x   Test point X
     * @param y   Test point Y
     * @return    1 if position is passable/empty, 0 if occupied
     */
    uint32_t blit_viewport(int x1, int y1, int x2, int y2, int x, int y);

    /**
     * CalcScrollRect — Calculate visible tile rect from scroll position.
     * Address: 0x42C590 (__thiscall)
     *
     * Called by UIPANEL rendering pipeline. Uses vtable[0x58] on param_2
     * (a surface pointer) to get surface dimensions via DDSURFACEDESC.
     * Compensates for negative clip offsets by shifting source rect.
     * Always returns FALSE (0) — possible BUG where return value
     * should indicate non-empty status but always returns 0.
     *
     * @param pClipRect  Source clip rect (Town+0x08/+0x0C = width/height bounds)
     * @param surface    Surface object with vtable[0x58] = GetSurfaceDesc
     * @return           Always 0 (false) — BUG: should return non-empty status
     */
    uint32_t calc_scroll_rect(RECT* pClipRect, void* surface);

    /**
     * CalcScrollRect_Reversed — Reversed scroll direction rect calculation.
     * Address: 0x42C700 (__thiscall)
     *
     * Key difference from CalcScrollRect: first IntersectRect clips
     * viewportRect vs pClipRect, giving reversed scroll mapping.
     * Used for multiplayer/mirrored views. Returns TRUE on success.
     *
     * @param pClipRect  Source clip rect (viewport bounds)
     * @param surface    Surface object with vtable[0x58] = GetSurfaceDesc
     * @return           TRUE if non-empty rect was calculated
     */
    uint32_t calc_scroll_rect_reversed(RECT* pClipRect, void* surface);

    /* ================================================================ */
    /* Postcard UI management                                            */
    /* ================================================================ */

    /**
     * InitPostcardUI — De-initialize/reset the postcard UI (MISNAMED: actually cleanup).
     * Address: 0x42F6C0
     *
     * Hides the window and cursor, invalidates tilemap, destroys player
     * info, destroys all 8 sprites + 3 child sub-windows, kills the 0x4D
     * timer, resets flags. This is the cleanup/teardown function, not
     * initialization.
     *
     * Called from: Town_Destroy during full cleanup
     */
    void init_postcard_ui();

    /**
     * InitOverlaySprite — One-shot initialization of postcard overlay sprite.
     * Address: 0x42FDF0
     *
     * Loads resource 0x3cf7, calls vtable[1] to get surface, caches at
     * overlay_resource/overlay_surface. Guarded by overlay_initialized flag.
     *
     * Called from: CGWND_InitMode1 during mode 1 setup
     */
    void init_overlay_sprite();

    /**
     * InitPostcardSprites — Initialize 8 postcard overlay sprites.
     * Address: 0x42FE30
     *
     * Sprite_Init on all 8 sprites created in base_ctor. Loads 3 child
     * window resources: background (0x3cf8), button strip (0x3cfb),
     * send confirm (0x3cfa). Sets sprites_initialized flag.
     */
    void init_postcard_sprites();

    /**
     * ClearPostcardUI — Clear and reset the postcard UI after closing.
     * Address: 0x42E760
     *
     * If a current player exists: restores background (postcard area +
     * send area) and re-renders the player via DPLAY_RenderPlayer.
     * If no player: falls back to Town_PostcardSendHandler + sprite updates.
     * Entry guard: postcard_active flag must be set.
     */
    void clear_postcard_ui();

    /**
     * PostcardSendHandler — Render postcard overlay to primary surface.
     * Address: 0x42E5E0
     *
     * Two modes:
     *   mode=0: draws only send-button sprite
     *   mode=1: draws postcard image then send animation strip
     *
     * Guarded by sprites_initialized and postcard_active flags.
     *
     * @param full_render  0 = send button only, non-0 = full postcard
     */
    void postcard_send_handler(char full_render);

    /**
     * PostcardUpdateUI — Update postcard sprites and UI state (idle handler).
     * Address: 0x42DE70
     *
     * Called after opening (PostcardInitList) and after clearing
     * (ClearPostcardUI) to reset sprites to default idle state. Also
     * called from the idle loop to check for pending network messages.
     *
     * @param action_id  Sprite/panel action to update:
     *   2..5 = individual button sprites
     *   6    = inbox preview panel (with message count check)
     *   7    = inbox counter sprite (uses inbox_state_lut)
     *   8    = outbox counter sprite (uses outbox_state_lut)
     *   9    = send button sprite
     */
    void postcard_update_ui(int action_id);

    /**
     * PostcardDlgProc — Postcard UI press/click handler.
     * Address: 0x42E150
     *
     * Complementary to PostcardUpdateUI. Plays sound (0x5015) and sets
     * sprites to pressed state (1) for actions 2..9. Action 6 blits the
     * inbox preview panel. Actions 7-8 update inbox/outbox sprites
     * based on DPLAY message count.
     *
     * @param action_id  Same as PostcardUpdateUI (2..9)
     */
    void postcard_dlg_proc(int action_id);

    /**
     * PostcardInitList — Initialize postcard list on dialog open (vtable[8] override).
     * Address: 0x42E420
     *
     * Sets postcard_active flag, resets all button sprites to idle,
     * renders send button via PostcardSendHandler, clears overlay,
     * sets focus, plays open audio sound. Called when the postcard
     * overlay dialog is shown.
     */
    void postcard_init_list();

    /**
     * PostcardUpdateButtons — Blit postcard button strip to primary surface.
     * Address: 0x42E4E0
     *
     * If repaint_requested flag is set, offsets destination by strip width
     * (pressed state column in two-column sprite sheet). Called from
     * Town_HitTest paint throttle.
     */
    void postcard_update_buttons();

    /**
     * HitTestButtons — Hit-test postcard overlay buttons by screen position.
     * Address: 0x430090
     *
     * Tests 7 UISprite rects (sprite_btn_close through sprite_send) plus
     * the inline button_hit_rect_send. Returns button ID (2-9) or 0.
     *
     * @param x  Screen X coordinate
     * @param y  Screen Y coordinate
     * @return   Button ID (2..9) or 0 if no button hit
     */
    byte hit_test_buttons(int32_t x, int32_t y);

    /**
     * HitTest — Postcard paint-throttle message handler (vtable[9] override).
     * Address: 0x42FFF0
     *
     * MISNAMED: Not a hit-test despite the name. Manages postcard paint
     * throttling by counting frames and triggering repaint via
     * UIPANEL_EndPaintEx every 20+ frames or on demand. All messages
     * forwarded to DefWindowProcA.
     */
    void hit_test(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * PostcardClickHandler — Left-click handler for postcard overlay.
     * Address: 0x42D670
     *
     * Checks selection_active flag and delegates to RESDATA_HitTestChildren.
     * If click_flag is set, clears it and returns 1 (suppress double-click).
     *
     * @param x  Click X
     * @param y  Click Y
     * @return   1 if click was handled, 0 otherwise
     */
    char postcard_click_handler(int x, int y);

    /**
     * PostcardCommandHandler — WM_COMMAND handler for postcard child controls.
     * Address: 0x42D6B0
     *
     * Dispatches resource IDs 0x3806/0x3807/0x3808 on a CGWND_TrackPiece
     * control. Handles zoom change and building deselection.
     *
     * @param control  TrackPiece control object
     * @param wParam   WPARAM from WM_COMMAND
     * @param lParam   LPARAM from WM_COMMAND
     * @return         1 if handled, 0 if not
     */
    int postcard_command_handler(void* control, uint32_t wParam, uint32_t lParam);

    /**
     * SendPostcard — Message handler for postcard sending lifecycle.
     * Address: 0x42D770
     *
     * Dispatches MSG_POSTCARD_SEND (0x3806), MSG_POSTCARD_ZOOM (0x3807),
     * MSG_POSTCARD_DESELECT (0x3808) on a CGWND_TrackPiece control.
     * Manages countdown timer at +0x54; fires zoom/animation when
     * timer reaches 0 and zoom level is 2.
     *
     * @param track_piece  CGWND_TrackPiece control to manipulate
     * @return             Status byte (1 = success, 0 = no-op)
     */
    byte send_postcard(void* track_piece);

    /**
     * UploadPostcard — Upload postcard data to remote players.
     * Address: 0x4309B0
     *
     * Iterates hostname list from NET_GetHostName, matches player IDs
     * against selected_player, writes 0x398 bytes of postcard payload
     * to a file per matching player via CreateFile/WriteFile.
     */
    void upload_postcard();

    /**
     * ReceivePostcard — Process an incoming postcard from the network.
     * Address: 0x42D8A0
     *
     * Walks the Sort_In (host) or Sort_Out (client) .crd files to
     * unregister stale sessions. If the postcard has the need_connect
     * flag (+0x3A), deletes the Att_In temp handshake file. Registers
     * sender in Album, releases old session, re-lists postcards.
     */
    void receive_postcard();

    /**
     * SavePostcard — Client-side session re-registration after sending postcard.
     * Address: 0x42DA10
     *
     * Enumerates hosts (type 2), unregisters stale players, registers
     * self as client (type 1), re-lists postcards, polls network messages.
     */
    void save_postcard();

    /**
     * LoadPostcard — Host-side session re-registration after receiving postcard.
     * Address: 0x42DB30
     *
     * Same structure as SavePostcard but registers as host (type 2).
     * Called after loading a postcard from the Album.
     */
    void load_postcard();

    /**
     * DeletePostcard — Delete a player's .crd postcard file.
     * Address: 0x42DC50
     *
     * Removes the .crd file from Sort_In (host) or Sort_Out (client)
     * PostBag directory, matching by player_id. Releases the selected_player
     * struct and resets has_remote_players flag in host mode.
     */
    void delete_postcard();

    /**
     * ListPostcards — Cycle selected_player to next .crd entry.
     * Address: 0x42DD50
     *
     * Walks the Sort_In/Sort_Out directory, advances the selected_player
     * pointer to the next entry, wrapping around to the first entry.
     */
    void list_postcards();

    /**
     * SavePostcardAs — Show "Save As" dialog for received postcard.
     * Address: 0x42EEA0 (__thiscall, 1 stack param: default_filename)
     *
     * Handles file-exists check, overwrite confirmation, and directory
     * creation. Uses GetSaveFileNameA with BMP filter. Returns:
     *   0 = cancelled/error
     *   1 = OK to save (file already exists OR ready)
     *   2 = create directory needed
     *
     * Implementation steps:
     *   1. Downloads postcard attachment via NET_DownloadAsset
     *   2. Sets up OPENFILENAME struct with dialog title (res 0x6a)
     *   3. Posts WM_USER+0x1F5 to hWnd, sets flag_E8=1
     *   4. Calls SetCursorPos and center_viewport(vtable[3])
     *   5. Shows GetSaveFileNameA dialog; flag_E8 cleared after
     *   6. On file selected: checks existing file via CreateFile(OPEN_EXISTING)
     *   7. If exists: prompts overwrite via MessageBox (res 0x6b)
     *   8. ERROR_FILE_NOT_FOUND: return 1 (OK to save to new file)
     *   9. ERROR_PATH_NOT_FOUND: prompts create dir via MessageBox (res 0x6d)
     *   10. Other errors: shows error dialog (res 0x6d, MB_ICONSTOP)
     *
     * @return  Result code: 0=cancelled, 1=ready, 2=create dir needed
     */
    byte save_postcard_as();

    /**
     * SaveReceivedPostcard — Download and save a received postcard.
     * Address: 0x42F250 (__thiscall, 1 stack param: player_id)
     *
     * Downloads postcard from network cache, shows Save As dialog via
     * SavePostcardAs, copies to user-chosen path, cleans up temp files,
     * and calls UploadPostcard to notify peers.
     *
     * Implementation steps:
     *   1. Gets cached file path via NET_GetFilePath(player_id, 5)
     *   2. Opens cache file with ReadFile (0x504 bytes into our +0xE9 buffer)
     *   3. On open/read failure: shows error via FormatMessageA, clears
     *      need_connect, clears postcard UI, paints
     *   4. On success: closes handle, calls SavePostcardAs()
     *   5. If result==2 (dir needed): extracts directory from path via
     *      _strrchr('\\'), calls CreateDirectoryA, error on failure
     *   6. Copies from attachment cache path via CopyFileA
     *   7. Deletes cache files via DeleteFileA on both paths
     *   8. Clears need_connect, calls UploadPostcard
     *   9. Clears postcard UI, paints
     *
     * @param player_id  ID of the sending player (used for cache path)
     */
    void save_received_postcard(uint32_t player_id);

    /**
     * LoadBackground — Window message / WM_SYSCOMMAND handler (vtable[11] override).
     * Address: 0x42EE20
     *
     * MISNAMED: Not a background loader. Handles WM_SYSCOMMAND/
     * SC_SCREENSAVE (prevents screen saver, posts quit) and
     * WM_USER+0x1F5 (re-enables window). All other messages
     * forwarded to DefWindowProcA.
     */
    void load_background(HWND hWnd, UINT msg, uint32_t wParam, LPARAM lParam);

    /**
     * PostcardDrawPreview — Preview overlay dialog proc (vtable[10] override).
     * Address: 0x42F810
     *
     * Handles ESC/Q to dismiss preview with sound and mode switch.
     * Forwards other messages to DefWindowProcA.
     *
     * @return  LRESULT from DefWindowProcA or 0
     */
    int32_t postcard_draw_preview(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

/* ================================================================== */
/* Train_HandleTrackBuild — remote track-build message handler         */
/* ================================================================== */

/**
 * Train_HandleTrackBuild — Process remote track-build network message.
 * Address: 0x43CE10
 *
 * Creates a local vehicle with random type, inits routes for each track
 * piece, copies local player info, downloads missing assets, pops one car
 * from the pending list, updates player info.
 *
 * Called when message type 0x3EC (track build) is received from another
 * player over DirectPlay.
 *
 * @param msg  Message buffer containing track build data
 */
void Train_HandleTrackBuild(void* msg);
