// Status: INTEGRATED
/**
 * Town.h — Main gameplay view (isometric town, building selection, postcard UI)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Town is the primary in-game view window. It manages:
 *   - Postcard creation, sending, receiving, and album management
 *   - Network session management for multiplayer postcard exchange
 *
 * (Cursor indicator sprites for valid/invalid placement feedback were
 * previously attributed here via handle_tile_click; confirmed to be
 * GameView's own method and moved to core/GameView.h/.cpp — see the
 * "Building selection and tracking" note below.)
 *
 * Class hierarchy (verified from Town_Ctor @ 0x42E900, which calls
 * UI_WindowBase_Ctor directly):
 *   UI_WindowBase (base vtable @ 0x477C30, 12 slots, size 0xE8)
 *     └─ Town  <- this class (vtable 0x477D88, 37 slots, size 0x6E0)
 *
 * Size: 0x6E0 bytes. Vtable: 0x477D88.
 *
 * NOTE: +0xE9..+0x5ED (0x504 bytes) is the postcard data/filename buffer
 * used during postcard save/load operations (save_postcard_as,
 * save_received_postcard). It used to be documented as double-duty with
 * named viewport/overlay/panel/cursor fields on the theory those fields
 * were genuine Town members sharing the same storage — re-verified while
 * moving handle_tile_click/HitTestChild off this class: every one of
 * those named fields was evidenced only by those two (now-moved,
 * GameView-owned) methods, so there was never a real Town-side union to
 * document — see the field list below.
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
class DPlayManager;
/* Canonical definition: graphics/LOCOBITMAP.h. NOT included from this
 * header/Town.cpp — LOCOBITMAP.h also defines a conflicting, differently
 * shaped `class PostcardAlbum` ("concept A" per its own header comment)
 * that collides with this file's ui/PostcardAlbum.h (a separate,
 * pre-existing landmine, out of scope here). Forward-declared as an
 * opaque pointer target; Town.cpp mirrors the confirmed field offsets
 * locally (see UIPANEL_SurfaceView there) rather than dereferencing the
 * real type. */
struct UIPANEL_Surface;

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

    /* +0xD0..+0xE7: formerly children_list/cursor_valid_dup/cursor_invalid_dup
     * /selected_building/child_panel. Re-verified while moving
     * handle_tile_click and HitTestChild (formerly "postcard_command_
     * handler") to GameView (core/GameView.h/.cpp): a tree-wide grep of
     * this file shows every one of those five fields was read/written
     * ONLY inside those same two now-moved methods (children_list/
     * child_panel had already been flagged suspect even before this pass
     * — this confirms it — and cursor_valid_dup/cursor_invalid_dup/
     * selected_building were the (Panel-derived-class-shaped, not
     * UI_WindowBase-shaped) evidence trail this class's own doc used to
     * cite). No surviving Town:: method touches this range; left as an
     * honest gap rather than reasserting fields this class doesn't use. */
    uint8_t    _pad_D0[0xE8 - 0xD0];    // +0xD0  padding to +0xE8

    uint8_t    flag_E8;                 // +0xE8  dialog guard flag (GetSaveFileNameA)

    /* +0xE9..+0x5EC (0x504 bytes): postcard data/filename buffer — see
     * save_postcard_as/save_received_postcard, which read/write this
     * range directly via `reinterpret_cast<char*>(this) + 0xE9`. This
     * used to be sub-declared as named viewport_inset_(x/y),
     * overlay_dest_(x/y), panel_graphics, selected_building_type,
     * cursor_valid_sprite, cursor_invalid_sprite, track_piece,
     * overlay_panel, backup_(surface/x/y/width), and
     * building_center_(x/y) fields on the theory that they shared this
     * space with the postcard buffer via a double-duty union — all of
     * those sub-fields were, like the ones above, evidenced only by
     * handle_tile_click/HitTestChild, now confirmed to be GameView's own
     * fields at GameView's own (unrelated, Panel-derived) offsets, not a
     * real union with this class's postcard buffer. Collapsed to a single
     * pad matching the real, still-used 0x504-byte buffer size. */
    uint8_t    _pad_E9[0x5ED - 0xE9];   // +0xE9  padding to +0x5ED

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

    /* Real type DPlayManager* (network/DPlayManager.h) — confirmed via
     * DPLAY_CreatePlayer (0x442850): sets vtable=0x478264, size 0x39C,
     * matching DPlayManager.h's own "This IS the DPLAY_PlayerSlot
     * structure" comment. upload_postcard's WriteFile of 0x398 bytes
     * from selected_player+4 covers exactly [0x4, 0x39C) of that layout,
     * and the existing `delete (DPlayManager*)selected_player` sites
     * elsewhere in this file already assumed this type. */
    DPlayManager* selected_player;      // +0x608  current selected player/crd entry
    DPlayManager* postcard_data;        // +0x60C  postcard receive data / player info

    uint8_t    player_count_flag;       // +0x610  player-count byte (init to 1)

    /* Postcard overlay rect (centered by on_create): left/top = screen
     * origin, right/bottom = overlay width/height (used as size in blits). */
    int32_t    postcard_rect_left;      // +0x614
    int32_t    postcard_rect_top;       // +0x618
    int32_t    postcard_rect_right;     // +0x61C
    int32_t    postcard_rect_bottom;    // +0x620

    /* Postcard player render area — passed to NetworkPlayerList::RenderPlayer
     * as its left/top/right/bottom row rect. */
    int32_t    render_param_x;          // +0x624  player render-area rect left
    int32_t    render_param_y;          // +0x628  player render-area rect top
    uint32_t   render_extra;            // +0x62C  player render-area rect right
    /* +0x630: was `void* render_rect_ptr` — a reinterpret_cast<void*>
     * round-trip of a plain coordinate (this->player_rect.top + 200),
     * an anti-pattern inherited from a since-corrected misreading of
     * NetworkPlayerList::RenderPlayer's real 9-arg ABI (its own doc
     * comment has the full resolution). This is genuinely just the
     * render-area rect's bottom coordinate, always was — fixed
     * 2026-08-17 to a plain int32_t, which also removes a real
     * host-size landmine (the same field, as `void*`, was already
     * documented at this file's PtInRect call site as covering only
     * the low 4 bytes of an 8-byte pointer on a 64-bit host when
     * reinterpret_cast to a packed RECT). */
    int32_t    render_bottom;           // +0x630  player render-area rect bottom

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

    /* ================================================================ */
    /* Building selection and tracking — NOT Town methods                */
    /*                                                                    */
    /* is_valid_placement/select_building/track_building/deselect_building/ */
    /* update_selection/render_selection/send_postcard (0x42CF90/0x42D040/  */
    /* 0x42D1A0/0x42D280/0x42D3A0/0x42D400/0x42D770) used to be declared    */
    /* here as Town:: methods. Ghidra disassembly (2026-08-13 session)      */
    /* confirmed every call site loads ECX with the bare immediate          */
    /* 0x4852A0 (or is called from one of these methods with `this`         */
    /* unchanged) — the real receiver is the GameView global instance       */
    /* (core/GameView.h/.cpp), not this class. Moved there (send_postcard   */
    /* renamed update_cursor_child: its sole xref is a GameView vtable      */
    /* slot reached only from track_building's own child loop, never a     */
    /* postcard call site).                                                */
    /*                                                                      */
    /* handle_tile_click (0x42CE10) and HitTestChild (0x42D6B0, formerly    */
    /* "postcard_command_handler" — a doubly wrong name: no postcards, no   */
    /* WM_COMMAND) showed the SAME "ECX = bare immediate 0x4852A0" receiver  */
    /* evidence (handle_tile_click) or an even stronger one (HitTestChild's  */
    /* sole xref is GameView's own vtable slot [17] DATA reference, with no  */
    /* direct call sites at all) — confirmed and moved to GameView in a      */
    /* later pass (core/GameView.h/.cpp). This class's own field model for   */
    /* +0x88..+0x1B8 was built from exactly these two methods and has been   */
    /* corrected accordingly (see the field list above).                    */
    /* ================================================================ */

    /* ================================================================ */
    /* Viewport / tile occupancy checks — NOT Town methods               */
    /*                                                                    */
    /* check_occupied/check_occupied_ex/blit_viewport (0x42C950/0x42C9F0/ */
    /* 0x42CB10) and calc_scroll_rect/calc_scroll_rect_reversed           */
    /* (0x42C590/0x42C700) used to be declared here as Town:: methods,    */
    /* despite this class's own prior doc comments already noting `this` */
    /* in those methods was NOT the Town instance. Ghidra xrefs (2026-08- */
    /* 08, town-cpp-strict2 session) confirmed both groups operate on a   */
    /* UIPANEL_Surface* receiver instead:                                 */
    /*   - Town_CheckOccupied/Town_CheckOccupiedEx/Town_BlitViewport are   */
    /*     free functions (graphics/LOCOBITMAP.h), called only from       */
    /*     game/BuildingMgr.cpp and game/World.cpp — implemented in       */
    /*     town/TownTiles.cpp beside UIPANEL_Surface's other address-     */
    /*     adjacent methods. Town.cpp used to carry a fully-implemented   */
    /*     but entirely uncalled/dead duplicate of these under the wrong  */
    /*     scope; deleted in favor of the real free functions, which had  */
    /*     been silent call-0 landmines (declared and called by their     */
    /*     real callers, but never defined anywhere) until this fix.      */
    /*   - UIPANEL_Surface::CalcScrollRect/CalcScrollRect_Reversed        */
    /*     (graphics/LOCOBITMAP.h) are called only from UIPANEL_Blit and  */
    /*     remain loud deferred stubs in town/TownTiles.cpp — Town.cpp's  */
    /*     dead duplicate body silently assumed two Ghidra-distinct stack */
    /*     pointers (`ptStack_4` vs the tracked RECT* `param_1`) were the */
    /*     same object, which the disassembly does not support; deleted   */
    /*     rather than promoted onto the live UIPANEL_Blit path.          */
    /* ================================================================ */

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
     *
     * FLAGGED (not fixed this pass): this is very likely also a
     * misattributed GameView method, not a Town one — it occupies
     * GameView's own vtable slot [4] (0x42D670, core/GameView.h),
     * overriding Panel::HitTestChildren, and RESDATA_HitTestChildren
     * (which it delegates to) is exactly Panel::HitTestChildren
     * (game/Panel.cpp), whose own per-child loop dispatches
     * GameView::HitTestChild (vtable[17], moved off this class in the
     * same pass that moved handle_tile_click here). Left on Town as a
     * documented follow-up, out of scope for this pass.
     */
    char postcard_click_handler(int x, int y);

    /* postcard_command_handler (0x42D6B0) moved to GameView::HitTestChild
     * — see the "Building selection and tracking" note above. */

    /* send_postcard (0x42D770) moved to GameView::update_cursor_child —
     * see the "Building selection and tracking" note above. */

    /**
     * postcard_mouse_handler — Postcard button mouse dispatch (vtable[20]
     * on THIS class's own vtable, 0x477D88 — unrelated to GameView's
     * vtable[20] at the same byte offset in its own, different vtable;
     * see core/GameView.h's class doc comment for that distinction).
     * Address: 0x430800
     *
     * Called from the postcard window proc path. Guards on
     * audio_playing/flag_E8/postcard_active, hit-tests the buttons and
     * dispatches the send/zoom/deselect actions.
     *
     * NOTE: this doc comment previously (incorrectly) claimed
     * track_building's child-update loop called this method — that
     * loop actually dispatches through GameView's own, unrelated
     * vtable[20] slot (GameView::update_cursor_child, 0x42D770); fixed
     * per the same evidence that moved track_building off this class.
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
     * canonical members of UIPANEL_Surface (graphics/LOCOBITMAP.h,
     * implemented in town/TownTiles.cpp) — the UIPANEL_Blit dispatch
     * context. They were historically duplicated on Town, then on a
     * standalone "TownTileRenderer" class that turned out to be a
     * duplicate view of UIPANEL_Surface itself; the UIPANEL_Surface
     * forms are authoritative. */
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
