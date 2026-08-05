// Status: INTEGRATED
/**
 * Town.h — Main gameplay view (isometric town, building selection, postcard UI)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Town is the primary in-game view window. It manages:
 *   - Building selection/highlight overlay (selection_active, overlay panel)
 *   - Building placement and tracking (track_building per-frame)
 *   - Postcard creation, sending, receiving, and album management
 *   - Network session management for multiplayer postcard exchange
 *   - Cursor indicator sprites for valid/invalid placement feedback
 *
 * Class hierarchy (verified from Town_Ctor @ 0x42E900, which calls
 * UI_WindowBase_Ctor directly):
 *   UI_WindowBase (base vtable @ 0x477C30, 12 slots, size 0xE8)
 *     └─ Town  <- this class (vtable 0x477D88, 37 slots, size 0x6E0)
 *
 * Size: 0x6E0 bytes. Vtable: 0x477D88.
 *
 * NOTE: The region +0xE9..+0x5ED serves double duty as the postcard
 * data/filename buffer (0x504 bytes) during postcard save/load
 * operations (save_postcard_as, save_received_postcard), overlapping
 * with viewport/overlay/panel fields. This is safe because those
 * operations only run during postcard UI transitions when the viewport
 * is not being rendered.
 *
 * ================================================================
 * Vtable layout (0x477D88) — verified from raw vtable bytes
 * ================================================================
 * Slots [0]..[11] correspond to the UI_WindowBase base slots; the
 * entries below marked "inherited" point to the same implementations
 * as the base vtable (0x477C30). Slots [12]..[36] are Town-only
 * virtuals (mostly window procs for the child windows Town registers).
 *
 *   [0]  +0x00 0x42E960  ~Town() scalar deleting destructor (override)
 *   [1]  +0x04 0x42F6C0  hide() — deinit_postcard_ui (override)
 *   [2]  +0x08 0x42F5E0  show() — full Town show (override)
 *   [3]  +0x0C 0x425FD0  set_mode (inherited UI_WindowBase)
 *   [4]  +0x10 0x426020  set_render_surface (inherited)
 *   [5]  +0x14 0x426130  on_async_task_failure (inherited)
 *   [6]  +0x18 0x425B70  create_full_window (inherited)
 *   [7]  +0x1C 0x42F8B0  on_create() — postcard geometry layout (override)
 *   [8]  +0x20 0x42E420  postcard_init_list (override)
 *   [9]  +0x24 0x4661A0  (inherited no-op)
 *   [10] +0x28 0x426140  (inherited)
 *   [11] +0x2C 0x42EE20  load_background — window proc (override)
 *   [12] +0x30 0x42FFF0  hit_test — paint-throttle window proc
 *   [13] +0x34 0x422EA0  UI_DefWndProc (default)
 *   [14] +0x38 0x430190  postcard_wnd_proc — overlay mouse handler
 *   [15] +0x3C 0x422EA0  UI_DefWndProc (default)
 *   [16] +0x40 0x4307C0  postcard_deselect_wnd_proc
 *   [17] +0x44 0x422EA0  UI_DefWndProc (default)
 *   [18] +0x48 0x422EA0  UI_DefWndProc (default)
 *   [19] +0x4C 0x422EA0  UI_DefWndProc (default)
 *   [20] +0x50 0x430800  postcard_mouse_handler — button action dispatch
 *   [21] +0x54 0x42F810  postcard_draw_preview — preview dialog proc
 *   [22] +0x58 0x422EA0  UI_DefWndProc (default)
 *   [23] +0x5C 0x426950  (returns 0 — shared no-op)
 *   [24] +0x60 0x42FF20  audio-guard window proc (shared)
 *   [25] +0x64 0x422EA0  UI_DefWndProc (default)
 *   [26] +0x68 0x426960  on-create dispatch window proc (shared)
 *   [27] +0x6C 0x426980  WM_PAINT window proc (shared)
 *   [28] +0x70 0x426A60  hwnd-guard window proc (shared)
 *   [29] +0x74 0x422EA0  UI_DefWndProc (default)
 *   [30] +0x78 0x426AC0  (returns 1 — shared)
 *   [31] +0x7C 0x426AD0  on-destroy window proc (shared)
 *   [32] +0x80 0x42FF80  close/quit window proc (shared)
 *   [33] +0x84 0x422EA0  UI_DefWndProc (default)
 *   [34] +0x88 0x422EA0  UI_DefWndProc (default)
 *   [35] +0x8C 0x422EA0  UI_DefWndProc (default)
 *   [36] +0x90 0x422EA0  UI_DefWndProc (default)
 *   (0x477E1C = 0x00000000 vtable terminator)
 *
 * NOTE: the previous transcription claimed PostcardInitList/HitTest/
 * PreviewDlgProc/LoadBackground overrode slots [8]..[11]. Verified
 * truth: [8] PostcardInitList and [11] LoadBackground override base
 * slots; HitTest is Town-only slot [12]; PostcardDrawPreview is
 * Town-only slot [21]; slots [1] (hide) and [2] (show) and [7]
 * (on_create) are also overridden.
 */

#pragma once

#include "../shared/types.h"
#include "../ui/UI_WindowBase.h"
#include "../ui/ButtonSprite.h"
#include "../game/TrackPiece.h"

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

class Building;

/* ================================================================== */
/* Town class                                                          */
/* ================================================================== */

class Town : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */
    /* --- Inherited from UI_WindowBase (see UI_WindowBase.h) ---       */
    /* +0x00: vtable is compiler-managed                                */
    /* +0x04: HINSTANCE hInstance, +0x08: HWND hWnd, ... +0xE4: visible */
    /* NOTE: base +0x28 (timerId) is repurposed by Town as the building */
    /* panel index (read at 0x42D32D in deselect_building).             */

    /* --- Selection and building tracking (+0x88..+0x1B8) --- */

    uint8_t    selection_active;        // +0x88  1 = building is selected
    uint8_t    _pad_89[7];              // +0x89  padding to +0x90

    uint8_t    postcard_click_flag;     // +0x90  click suppression flag
    uint8_t    _pad_91[0x3F];           // +0x91  padding to +0xD0

    /** Child TrackPiece sprite list (created by RESDATA_CreateChildSprite,
     *  linked via +0x28). Tracked per-frame by track_building. */
    TrackPiece* children_list;          // +0xD0  child sprite list head

    /* Cursor indicator child sprites (duplicates of +0x170/+0x174,
     * stored by handle_tile_click from res 0x3807/0x3808). */
    TrackPiece* cursor_valid_dup;       // +0xD8  dup of cursor_valid_sprite
    TrackPiece* cursor_invalid_dup;     // +0xDC  dup of cursor_invalid_sprite

    Building*  selected_building;       // +0xE0  currently selected building
    void*      child_panel;             // +0xE4  child panel object

    uint8_t    flag_E8;                 // +0xE8  dialog guard flag (GetSaveFileNameA)
    /* +0xE9..+0x5ED: postcard data/filename buffer (0x504 bytes) —
       overlaps the viewport/overlay/panel fields below during save ops. */

    /* Viewport inset rect — visible area within the Town window */
    int32_t    viewport_inset_left;     // +0xEC
    int32_t    viewport_inset_top;      // +0xF0
    int32_t    viewport_inset_right;    // +0xF4
    int32_t    viewport_inset_bottom;   // +0xF8

    uint8_t    _pad_FC[0x18];           // +0xFC  padding to +0x114

    /* Overlay destination rect — selection highlight on screen */
    int32_t    overlay_dest_left;       // +0x114
    int32_t    overlay_dest_top;        // +0x118
    int32_t    overlay_dest_right;      // +0x11C
    int32_t    overlay_dest_bottom;     // +0x120

    void*      panel_graphics;          // +0x124  building panel object:
                                        //   +0x10 = surface ptr (blit source)
                                        //   +0x20 = anim table (0x18-byte entries)
    uint8_t    _pad_128[0x44];          // +0x128  padding to +0x16C

    uint16_t   selected_building_type;  // +0x16C  tile type of selected building
                                        //         (6 = depot, 7 = remove tool)

    /* Cursor indicator sprites (created by handle_tile_click) */
    TrackPiece* cursor_valid_sprite;    // +0x170  valid-placement cursor (res 0x3807)
    TrackPiece* cursor_invalid_sprite;  // +0x174  invalid-placement cursor (res 0x3808)

    TrackPiece* track_piece;            // +0x178  track-piece zoom control
                                        //         (cursor hover sprite res 0x3806)
    void*      overlay_panel;           // +0x17C  UIPANEL surface for selection overlay

    /* Backup surface data — background restore on deselect */
    uint32_t   backup_surface;          // +0x180  backup surface handle
    uint32_t   backup_x;                // +0x184  backup region source X
    int32_t    backup_y;                // +0x188  backup region source Y
    uint32_t   backup_width;            // +0x18C  backup region width
    int32_t    building_center_x;       // +0x190  cached building center X
    int32_t    building_center_y;       // +0x194  cached building center Y

    uint8_t    _pad_198[0x5ED - 0x198]; // +0x198  padding to +0x5ED

    /* --- Postcard UI fields (+0x5ED..+0x6D6) --- */
    uint8_t    audio_playing;           // +0x5ED  1 = postcard open/close sound playing
    uint8_t    _pad_5EE[2];             // +0x5EE  padding

    int32_t    timer_counter;           // +0x5F0  paint-throttle frame counter
    HICON      icon_handle;             // +0x5F4  window icon (resource 0x65)

    uint8_t    postcard_active;         // +0x5F8  1 = postcard UI is open/active
    uint8_t    sprites_initialized;     // +0x5F9  1 = 8 postcard sprites + 3 children ready
    uint8_t    overlay_initialized;     // +0x5FA  1 = overlay sprite (res 0x3cf7) ready
    uint8_t    _pad_5FB;                // +0x5FB  padding

    int32_t    timer_active;            // +0x5FC  timer ID for 0x4D (postcard refresh)
    int32_t    frame_counter;           // +0x600  postcard paint-throttle frame count

    uint8_t    net_update_flag;         // +0x604  1 = need to update network state
    uint8_t    repaint_requested;       // +0x605  paint throttle flag
    uint8_t    is_host;                 // +0x606  1 = this player is the game host
    uint8_t    has_remote_players;      // +0x607  1 = remote players are connected

    void*      selected_player;         // +0x608  current selected player/crd entry
    void*      postcard_data;           // +0x60C  postcard receive data / player info

    uint8_t    player_count_flag;       // +0x610  player-count byte (init to 1)

    /* Postcard overlay rect (centered by on_create): left/top = screen
     * origin, right/bottom = overlay width/height (used as size in blits). */
    int32_t    postcard_rect_left;      // +0x614
    int32_t    postcard_rect_top;       // +0x618
    int32_t    postcard_rect_right;     // +0x61C
    int32_t    postcard_rect_bottom;    // +0x620

    /* Postcard player render area — passed to DPLAY_RenderPlayer as
     * (x, y, extra, rect_ptr). */
    int32_t    render_param_x;          // +0x624
    int32_t    render_param_y;          // +0x628
    uint32_t   render_extra;            // +0x62C
    void*      render_rect_ptr;         // +0x630  RECT* passed as DPLAY_RenderPlayer arg 7

    /* Player render rect {0,0,800,600}, centered by on_create. */
    RECT       player_rect;             // +0x634

    /* Postcard overlay resources (RESDATA children, released via vtable[2]) */
    void*      overlay_resource;        // +0x644  overlay background resource (res 0x3cf7)
    void*      overlay_surface;         // +0x648  overlay surface handle

    void*      background_resource;     // +0x64C  postcard background resource (res 0x3cf8)
    void*      background_surface;      // +0x650  postcard background surface handle

    /* Send animation area rect */
    int32_t    send_rect_left;          // +0x654
    int32_t    send_rect_top;           // +0x658
    int32_t    send_rect_right;         // +0x65C
    int32_t    send_rect_bottom;        // +0x660

    /* Button strip */
    void*      button_strip_resource;   // +0x664  button strip resource (res 0x3cfb)
    void*      button_strip_surface;    // +0x668  button strip surface handle

    int32_t    button_src_left;         // +0x66C  button strip source rect left
    int32_t    button_src_top;          // +0x670  button strip source rect top
    int32_t    button_src_right;        // +0x674  button strip source rect right
    int32_t    button_src_bottom;       // +0x678  button strip source rect bottom

    RECT       button_hit_rect_send;    // +0x67C  hit-test rect for send button (ID 9)

    RECT       preview_rect;            // +0x68C  postcard image area (blit source)

    /* Send confirm */
    void*      send_confirm_resource;   // +0x69C  send confirmation resource (res 0x3cfa)
    void*      send_confirm_surface;    // +0x6A0  send confirmation surface handle

    /* Postcard button sprites (ButtonSprite objects, 0x24 bytes, created
     * by ButtonSprite_Ctor @ 0x454B50). RECT (x, y, w, h) lives at +4. */
    ButtonSprite* sprite_btn_close;     // +0x6A4  button 1 (res 0x3cf0)
    ButtonSprite* sprite_btn_options;   // +0x6A8  button 2 (res 0x3cf1)
    ButtonSprite* sprite_btn_rotate;    // +0x6AC  button 3 (res 0x3cf2)
    ButtonSprite* sprite_btn_save;      // +0x6B0  button 4 (res 0x3cf3)
    ButtonSprite* sprite_inbox;         // +0x6B4  inbox preview sprite (res 0x3cac)
    ButtonSprite* sprite_outbox_counter;// +0x6B8  outbox counter sprite (res 0x3cf6)
    ButtonSprite* sprite_inbox_counter; // +0x6BC  inbox counter sprite (res 0x3cf5)
    ButtonSprite* sprite_send;          // +0x6C0  send button sprite (res 0x3cf9)

    /* Sprite state look-up tables (indexed by message count, capped at 4) */
    uint16_t   inbox_state_lut[5];      // +0x6C4  {0,1,3,5,7}
    uint16_t   outbox_state_lut[5];     // +0x6CE  {0,1,2,3,4}

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * Town constructor.
     * Address: 0x42E900
     *
     * Calls UI_WindowBase(hInstance, 0x1F5), sets the vtable to 0x477D88
     * (0x477D88), then runs base_ctor (field init + 8 ButtonSprites).
     *
     * Called by: CGWND_InitAllSubsystems @ 0x407054 with
     *            operator_new(0x6E0).
     */
    Town(HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x42E960 — calls destroy() then GLOBAL_free if flags & 1.
     */
    virtual ~Town();

    /**
     * Base constructor body — initializes all Town-specific fields.
     * Address: 0x42E980
     *
     * Zeroes +0x5F4..+0x610 and +0x644/+0x648, creates 8 ButtonSprite
     * objects (res 0x3CF0-0x3CF9, with 0x3CAC for the inbox preview),
     * initializes the sprite state look-up tables at +0x6C4/+0x6CE.
     */
    void base_ctor();

    /**
     * Full destructor body — releases all Town resources.
     * Address: 0x42EC10
     *
     * Destroys the overlay resource (+0x5FA guard), destroys the 8
     * postcard sprites + 3 child resources, frees the sprite objects,
     * then calls UI_WindowBase::base_destructor().
     */
    void destroy();

    /* ================================================================ */
    /* Window lifecycle methods                                          */
    /* ================================================================ */

    /**
     * show — Show the Town window and prepare the postcard UI (vtable[2]).
     * Address: 0x42F5E0
     *
     * Hides the OS cursor, runs the base show, clears flag_E8, initializes
     * the postcard sprites, runs on_create geometry, focuses the window,
     * sets is_host=1, counts remote hosts, arms the 0x4D/200ms timer and
     * re-lists postcards.
     */
    void show() override;

    /**
     * hide — Hide the Town window and de-initialize the postcard UI
     * (vtable[1]). Address: 0x42F6C0
     *
     * If visible: base hide, hide OS cursor, invalidate the viewport
     * tilemap, release selected_player/postcard_data, destroy the 8
     * sprites + 3 child resources, kill the 0x4D timer. Always finishes
     * with NET_UpdatePlayerList + NETMAN_CheckTimeout.
     */
    void hide() override;

    /**
     * on_create — Postcard geometry layout (vtable[7]).
     * Address: 0x42F8B0
     *
     * Runs after the sprites are initialized: centers the postcard
     * overlay rect (+0x614..+0x620) on the working rect, positions the
     * player rect (+0x634..+0x640), computes the 8 sprite screen rects
     * from their resource widths, and fills the send/button/preview
     * geometry fields.
     */
    void on_create() override;

    /**
     * layout_postcard_sprite — Position one button sprite.
     * (No original address: this is the repeated sprite-positioning block
     * inside on_create at 0x42F9E0..0x42FD90.)
     *
     * Computes the sprite screen rect from its resource width/height
     * relative to the player rect, then applies the (dx, dy) offset.
     */
    void layout_postcard_sprite(ButtonSprite* sprite, int dx, int dy);

    /**
     * init_sprites — Create the full-desktop Town child window.
     * Address: 0x42EDB0
     *
     * Loads icon 0x65 and calls UI_CreateFullWindow sized to the desktop.
     */
    bool init_sprites(HWND hParent);

    /**
     * handle_tile_click — Create placement cursor indicator sprites.
     * Address: 0x42CE10
     *
     * MISNAMED: this is not a click handler. Creates 3 TrackPiece child
     * sprites (valid=0x3807 -> +0x170/+0xD8, invalid=0x3808 -> +0x174/+0xDC,
     * hover=0x3806 -> +0x178), loads animation resources 0x3805 (self) and
     * 0x3804 (child_panel) via vtable[6], creates the overlay UIPANEL
     * surface at +0x17C and initializes the backup rect at +0x180.
     * Returns 1 on success.
     */
    char handle_tile_click();

    /* ================================================================ */
    /* Building selection and tracking                                   */
    /* ================================================================ */

    /**
     * is_valid_placement — Static placement check (__cdecl, not a method).
     * Address: 0x42CF90
     *
     * Validates entity initialized (+0x18) and tile type byte at
     * resource+8: 0x07 always valid; 0x08/0x02/0x06 must be visible;
     * 0x04 must be connected (+0x62C); 0x03 must be a building tile;
     * 0x0C valid when resource id > 0x300F.
     */
    static bool is_valid_placement(Building* entity);

    /**
     * select_building — Select/focus a building (or NULL to deselect).
     * Address: 0x42D040
     *
     * Validates, centers the viewport via set_mode, sets zoom (1 for
     * type 6, else 3), renders the track piece, invalidates the tile
     * rect and notifies DDRAW_SelectBuilding. The NULL path clears the
     * selection, restores the active panel, and hides self + child_panel.
     * Returns selection_active.
     */
    byte select_building(Building* building);

    /**
     * track_building — Per-frame tracking of the selected building.
     * Address: 0x42D1A0
     *
     * Auto-deselects an invisible depot (type 6), re-centers the
     * viewport when the building center moved, queries cursor position
     * via GameObject_GetRelPos, updates each child sprite and the
     * child panel.
     */
    void track_building();

    /**
     * deselect_building — Remove the building selection overlay.
     * Address: 0x42D280
     *
     * Computes a clip rect from viewport_inset and overlay dimensions,
     * intersects it with the viewport rect, blits the cached background
     * back to the primary surface, then re-blits the panel overlay.
     */
    void deselect_building();

    /**
     * update_selection — Blit the selection overlay panel to primary.
     * Address: 0x42D3A0
     *
     * Source from viewport_inset, dest from overlay_dest, flag 0x40.
     */
    void update_selection();

    /**
     * render_selection — Draw the selection highlight for one tile.
     * Address: 0x42D400
     *
     * If selection_active, calls GameObject_Draw (0x405E60) directly
     * with the caller's tile rect (5 stack args; RET 0x14).
     */
    void render_selection(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                          int32_t extra);

    /* ================================================================ */
    /* Viewport / tile occupancy checks                                  */
    /* ================================================================ */

    /**
     * check_occupied — Scan tile buffer or DDraw surface for occupancy.
     * Address: 0x42C950 (__thiscall)
     *
     * NOTE: `this` is the tile-cache/viewport context object (fields
     * +0x04 mode, +0x08 stride, +0x18 pixels), not the Town instance.
     * Mode 0 (this+0x04 == 0): scans the byte array at +0x18 (stride
     * +0x08) over [x1..x2, y1..y2), returns 1 on any non-zero byte.
     * Mode 1: delegates to check_occupied_ex.
     */
    uint8_t check_occupied(int x1, int y1, int x2, int y2);

    /**
     * check_occupied_ex — Extended tile occupancy via primary-surface lock.
     * Address: 0x42C9F0 (__stdcall, 4 args)
     *
     * Locks the global primary DirectDraw surface (COM slot 25 = byte
     * offset 0x64) with a DDSURFACEDESC, scans 16-bit pixels in
     * [x1..x2, y1..y2). A pixel is occupied when
     * ((pixel & red_mask) >> red_shift) != 0x1f AND (pixel & blue_mask)
     * != 0x1f (non-water). Unlock via COM slot 32 (byte offset 0x80)
     * with loss recovery through g_surface_lost.
     */
    static uint8_t check_occupied_ex(int x1, int y1, int x2, int y2);

    /**
     * blit_viewport — Viewport occupancy check for collision detection.
     * Address: 0x42CB10 (__thiscall, 6 stack args)
     *
     * Passability test for point (x, y). NOTE: the binary uses the
     * parameters as (x < x1 || y2 < x || y < x2 || x < y) -> passable;
     * parameter y1 is never read (documented quirk of the original).
     * Mode 0 (this+0x04 != 1): byte index buffer at +0x18 with stride
     * +0x08; byte==0 -> passable. Mode 1: locks the surface from
     * this+0x1C, water check (both channels == 0x1f -> passable).
     */
    uint32_t blit_viewport(int x1, int y1, int x2, int y2, int x, int y);

    /**
     * calc_scroll_rect — Calculate the visible tile rect from scroll.
     * Address: 0x42C590 (__thiscall)
     *
     * Fills a DDSURFACEDESC via surface vtable[0x58 bytes = slot 22],
     * builds surface + viewport rects, compensates negative clip
     * offsets and intersects with the surface bounds. Returns 0 (the
     * return value is unused by callers; kept faithful).
     */
    uint32_t calc_scroll_rect(RECT* pClipRect, void* surface);

    /**
     * calc_scroll_rect_reversed — Reversed scroll direction rect calc.
     * Address: 0x42C700 (__thiscall)
     *
     * Same DDSURFACEDESC setup, but first IntersectRect clips the
     * viewport rect against pClipRect. Returns TRUE on success.
     */
    uint32_t calc_scroll_rect_reversed(RECT* pClipRect, void* surface);

    /* ================================================================ */
    /* Postcard UI management                                            */
    /* ================================================================ */

    /**
     * postcard_init_list — Initialize the postcard list on dialog open
     * (vtable[8]). Address: 0x42E420
     *
     * Sets postcard_active, renders the send button, resets all button
     * sprites to idle, clears the overlay, ends paint, sets focus and
     * plays the open audio event (HelpWnd_PlayNarration @ 0x44F560).
     */
    /** Binary slot [8] 0x42E420 (postcard_init_list). */
    void on_update(int32_t param) override;

    /**
     * init_overlay_sprite — One-shot initialization of the postcard
     * overlay sprite. Address: 0x42FDF0
     *
     * Loads resource 0x3cf7 into overlay_resource and stores the
     * vtable[1] surface into overlay_surface. Guarded by
     * overlay_initialized.
     */
    void init_overlay_sprite();

    /**
     * init_postcard_sprites — Initialize the 8 postcard sprites.
     * Address: 0x42FE30
     *
     * Calls Sprite_Init on all 8 sprites, then loads the 3 child window
     * resources: background (0x3cf8 -> +0x64C/+0x650), button strip
     * (0x3cfb -> +0x664/+0x668), send confirm (0x3cfa -> +0x69C/+0x6A0).
     * Sets sprites_initialized.
     */
    void init_postcard_sprites();

    /**
     * clear_postcard_ui — Clear/reset the postcard UI after closing.
     * Address: 0x42E760
     *
     * If a selected player exists: restores the preview area, re-renders
     * the player via DPLAY_RenderPlayer (args from +0x610/+0x624..+0x630),
     * restores the send area, and updates the inbox sprite. Otherwise
     * re-renders the postcard and updates outbox/inbox sprites.
     */
    void clear_postcard_ui();

    /**
     * postcard_send_handler — Render the postcard overlay to primary.
     * Address: 0x42E5E0
     *
     * mode=0: draws only the send-button sprite (dest from the postcard
     * overlay rect at +0x614..+0x620, source from the base workRect at
     * +0xD4..+0xE0). mode=1: draws the postcard image from preview_rect
     * then the send animation strip from send_rect.
     */
    void postcard_send_handler(char full_render);

    /**
     * postcard_update_ui — Postcard idle/release UI handler.
     * Address: 0x42DE70
     *
     * action 2..5: reset button sprite to idle; 6: blit inbox preview
     * and check message count; 7: inbox counter sprite via inbox LUT;
     * 8: outbox counter sprite via outbox LUT; 9: send button idle.
     */
    void postcard_update_ui(int action_id);

    /**
     * postcard_dlg_proc — Postcard press/click handler.
     * Address: 0x42E150
     *
     * Plays sound 0x5015 and sets the pressed state (1) for actions
     * 2..9; action 6 additionally blits the inbox preview; actions 7-8
     * update the counter sprites via the state LUTs.
     */
    void postcard_dlg_proc(int action_id);

    /**
     * postcard_update_buttons — Blit the postcard button strip.
     * Address: 0x42E4E0
     *
     * When repaint_requested is set, offsets the destination by the
     * strip width (pressed column of the two-column sprite sheet).
     */
    void postcard_update_buttons();

    /**
     * hit_test_buttons — Hit-test postcard overlay buttons.
     * Address: 0x430090
     *
     * Tests the 8 ButtonSprite rects (sprite+4) plus the inline
     * button_hit_rect_send. Returns button ID 2..9 or 0.
     */
    byte hit_test_buttons(int32_t x, int32_t y);

    /**
     * hit_test — Postcard paint-throttle window proc (vtable[12]).
     * Address: 0x42FFF0
     *
     * MISNAMED: not a hit-test. Counts frames and triggers a repaint
     * via UIPANEL_EndPaintEx every 20+ frames or on demand. All
     * messages forwarded to DefWindowProcA.
     */
    /** Binary slot [12] 0x42FFF0 (hit_test). */
    LRESULT on_timer(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    /**
     * postcard_click_handler — Left-click handler for the postcard overlay.
     * Address: 0x42D670
     *
     * Checks selection_active, delegates to RESDATA_HitTestChildren and
     * suppresses double clicks via postcard_click_flag.
     */
    char postcard_click_handler(int x, int y);

    /**
     * postcard_command_handler — WM_COMMAND handler for postcard controls.
     * Address: 0x42D6B0
     *
     * Dispatches resource IDs 0x3806/0x3807/0x3808 on a TrackPiece
     * control: zoom changes and building (de)selection.
     */
    int postcard_command_handler(TrackPiece* control, uint32_t wParam,
                                 uint32_t lParam);

    /**
     * send_postcard — Postcard sending lifecycle handler.
     * Address: 0x42D770
     *
     * Counts down the TrackPiece +0x54 timer and fires the zoom/
     * animation when it reaches 0 (zoom level 2). For MSG 0x3806 with
     * a world pointer at building+0x44C, saves the world to disk.
     */
    byte send_postcard(TrackPiece* track_piece);

    /**
     * postcard_mouse_handler — Postcard button mouse dispatch (vtable[20]).
     * Address: 0x430800
     *
     * Called from the child-update loop in track_building with the child
     * pointer (interpreted as packed mouse coords) and from the postcard
     * window proc path. Guards on audio_playing/flag_E8/postcard_active,
     * hit-tests the buttons and dispatches the send/zoom/deselect
     * actions.
     */
    /** Binary slot [20] 0x430800 (postcard_mouse_handler); packed x,y in lParam. */
    LRESULT on_mouse_move(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    /**
     * postcard_wnd_proc — Postcard overlay window proc (vtable[14]).
     * Address: 0x430190
     *
     * Handles mouse clicks on the postcard preview/button areas:
     * save-received (0x5464 sound), select player, inbox preview
     * toggle, close (hide + set mode 3), options, rotate, send, etc.
     */
    /** Binary slot [14] 0x430190 (postcard_wnd_proc); packed x,y in lParam. */
    LRESULT on_lbutton_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    /**
     * load_background — Town window proc (vtable[11]).
     * Address: 0x42EE20
     *
     * Handles WM_SYSCOMMAND/SC_SCREENSAVE (prevents screen saver, posts
     * quit, kills the 0x4D timer) and WM_USER+0x1F5 (re-enables the
     * window). Other messages forwarded to DefWindowProcA.
     */
    /** Binary slot [11] 0x42EE20 (load_background). */
    LRESULT window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    /**
     * postcard_draw_preview — Preview overlay dialog proc (vtable[21]).
     * Address: 0x42F810
     *
     * Handles ESC/Q to dismiss the preview with a sound and a mode
     * switch (postcard_dlg_proc(2), hide, CGWND_SetMode(3)). Others
     * forwarded to DefWindowProcA.
     */
    /** Binary slot [21] 0x42F810 (postcard_draw_preview). */
    LRESULT on_key_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    /* ================================================================ */
    /* Postcard network/session management                               */
    /* ================================================================ */

    /**
     * upload_postcard — Write the postcard payload to remote players.
     * Address: 0x4309B0
     *
     * Iterates the hostname list (NET_GetHostName), matches player IDs
     * and writes 0x398 bytes from selected_player+4 to a per-player
     * file via CreateFile/WriteFile.
     */
    void upload_postcard();

    /**
     * receive_postcard — Process an incoming postcard from the network.
     * Address: 0x42D8A0
     *
     * Walks the Sort_In/Sort_Out .crd files to unregister stale
     * sessions, deletes the Att_In handshake file when need_connect
     * (+0x3A) is set, registers the sender, releases the old session
     * and re-lists postcards.
     */
    void receive_postcard();

    /**
     * save_postcard — Client-side session re-registration after sending.
     * Address: 0x42DA10
     *
     * Unregisters stale players, registers self as client (type 1),
     * re-lists postcards and polls network messages.
     */
    void save_postcard();

    /**
     * load_postcard — Host-side session re-registration after receiving.
     * Address: 0x42DB30
     *
     * Same structure as save_postcard but registers self as host
     * (type 2).
     */
    void load_postcard();

    /**
     * delete_postcard — Delete a player's .crd postcard file.
     * Address: 0x42DC50
     *
     * Unregisters the matching hostname, releases selected_player and
     * re-counts remote players.
     */
    void delete_postcard();

    /**
     * list_postcards — Cycle selected_player to the next .crd entry.
     * Address: 0x42DD50
     *
     * Walks the Sort_In/Sort_Out hostname list and advances
     * selected_player, wrapping around to the first entry.
     */
    void list_postcards();

    /**
     * save_postcard_as — Show the "Save As" dialog for a received postcard.
     * Address: 0x42EEA0 (__thiscall)
     *
     * Downloads the attachment via NET_DownloadAsset(player_id, 5, buf),
     * builds the OPENFILENAMEA (lStructSize 0x4C, lpstrFile = this+0xE9,
     * nMaxFile 0x504, lpstrFileTitle buffer, lpstrInitialDir "c:",
     * Flags 0x80024, lpfnHook = SaveAsDlgHook @ 0x419FD0), posts
     * WM_USER+0x1F5, sets flag_E8, repositions the cursor onto
     * sprite_send, runs set_mode, then GetSaveFileNameA. Handles
     * file-exists overwrite (MB_YESNO), ERROR_FILE_NOT_FOUND -> 1,
     * ERROR_PATH_NOT_FOUND -> 2/0, other errors -> 0.
     *
     * @return 0 = cancelled/error, 1 = ready to save, 2 = create dir needed
     */
    byte save_postcard_as();

    /**
     * save_received_postcard — Download and save a received postcard.
     * Address: 0x42F250
     *
     * Reads the cached .dat payload (0x504 bytes) into the +0xE9
     * buffer, shows the Save As dialog, creates the target directory
     * when needed, copies the .att attachment, deletes the cache files
     * and notifies peers via upload_postcard.
     *
     * @param unused_arg  Present in the original signature but never
     *                    read; the player id comes from selected_player
     *                    +0x3A.
     */
    void save_received_postcard(uint32_t unused_arg);

    /* NOTE: the tile-render primitives (BlitElement 0x42B960,
     * init_tile_cache 0x42B9C0, draw_tile 0x42BA90,
     * flush_tile_cache 0x42BB90, draw_cached_tile 0x42BC80) are the
     * canonical members of TownTileRenderer (town/TownTiles.h) — the
     * UIPANEL_Blit dispatch context. They were historically duplicated
     * on Town; the TownTileRenderer forms are authoritative. */
};

/* ================================================================== */
/* Train_HandleTrackBuild — remote track-build network message handler  */
/* ================================================================== */

/**
 * Train_HandleTrackBuild — Process a remote track-build network message.
 * Address: 0x43CE10 (__thiscall: ECX = TrainSubsystem, 1 stack arg)
 *
 * Called from Train_ProcessMessages for message type 0x3EC. Creates a
 * local Vehicle with a random type (3 variants starting at res 0x1804),
 * sets its player data from the session records in the message, inits
 * the route, downloads missing assets, then moves pending vehicle
 * nodes into the network queue (type 0xF TrainMessage).
 *
 * @param subsystem  TrainSubsystem (pending list at +0x14, next +0x70)
 * @param msg        Track-build message (count at +0xC, records at +0x10)
 */
void Train_HandleTrackBuild(void* subsystem, int msg);
