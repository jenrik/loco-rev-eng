// Status: INTEGRATED
/**
 * Game.h — Top-level gameplay state singleton
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The Game class extends Entity and serves as the primary gameplay state
 * manager. It tracks cursor/mouse state (position, clicks, drags, screen-
 * saver idle), manages selection of buildings/vehicles from the UI palette,
 * and dispatches per-frame updates to all active subsystems via the
 * Game::Update per-frame loop.
 *
 * A single global instance lives at g_game (0x4854C8).
 *
 * Size: ~0x11C bytes (Entity base 0x88 + Game fields 0x94 bytes)
 * Vtable: 0x477718 (extends the Entity 15-slot vtable 0x477488)
 *
 * Class hierarchy:
 *   GameObject (root, type=1, vtable 0x477820)
 *     +-- Entity (type=2, vtable 0x477488)
 *          +-- Game (vtable 0x477718)  <- this class
 *
 * Vtable layout (0x477718 — bytes verified against the binary; only the
 * destructor and Update are overridden, everything else is inherited):
 *   [0]  +0x00: scalar deleting destructor (Game_Dtor, 0x410660)
 *                   -> calls ~Game() body (Game_BaseDtor, 0x410680)
 *   [1]  +0x04: InvalidateRect   (inherited, 0x436AB0)
 *   [2]  +0x08: PtInRect         (inherited, 0x436A10)
 *   [3]  +0x0C: MoveTo/SetWorldPos (inherited, 0x405C00)
 *   [4]  +0x10: InvokeCallback1  (inherited, 0x436AE0)
 *   [5]  +0x14: InvokeCallback2  (inherited, 0x436B00)
 *   [6]  +0x18: InitBase         (inherited, 0x405900)
 *   [7]  +0x1C: StopSound        (inherited, 0x405A20)
 *   [8]  +0x20: SetFrame         (inherited, 0x405DE0)
 *   [9]  +0x24: SetVisible       (inherited, 0x4061B0)
 *   [10] +0x28: Update           (Game_Update, 0x410840) — OVERRIDDEN
 *   [11] +0x2C: Draw             (inherited, 0x405E60)
 *   [12] +0x30: DrawConnected    (inherited, 0x405FD0)
 *   [13] +0x34: SetName          (inherited, 0x405E20)
 *   [14] +0x38: SetAnimState     (inherited, 0x405A50)
 *
 * Sound resource IDs used by the cursor engine:
 *   0x1400 = generic click/feedback sound
 *   0x1402 = edge-of-world / blocked
 *   0x1404 = placement valid / action accepted
 *   0x1405 = tooltip/contextual cursor
 *   0x5015 = left-click action success
 *   0x502C = right-click action success
 *   cursor edge sounds:
 *     0xC1A / 0x3409 = horizontal scroll
 *     0xC1C / 0x3408 = vertical scroll
 *     0xC26 = scroll right edge, 0xC28 = scroll top edge
 *     0xC2A = scroll bottom edge, 0xC2C = scroll left edge
 *     0xC42 = scroll right (drag variant), 0xC44 = scroll left (drag variant)
 *     0xC46 = scroll top (drag variant),  0xC48 = scroll bottom (drag variant)
 */

#pragma once

#include "Entity.h"
#include "../shared/types.h"

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */
struct Timer;        /* Sub-object at +0x10C, collection-inherited */
class Building;      /* game/Building.h — selected object type */

/* ================================================================== */
/* Global singleton address                                             */
/* ================================================================== */
#define ADDR_g_game                     0x004854C8  /* g_game singleton */

/* ================================================================== */
/* Game class -- gameplay state manager (Entity-derived)                 */
/* ================================================================== */
class Game : public Entity {
public:
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    /* ================================================================ */
    /* Game-specific fields (offsets from 'this', Entity ends at +0x88)  */
    /* ================================================================ */

    /* ---- Mouse / cursor state (+0x88..+0xA3) ---- */
    int32_t    cursor_sound_id;         /* +0x88  current cursor hover feedback sound ID, -1=none */
    uint8_t    cursor_disabled;         /* +0x8D  1=using custom busy.ani cursor mode */
    uint8_t    screensaver_active;      /* +0x8E  1=screensaver idle triggered */
    uint8_t    _pad_8F;                 /* +0x8F */
    uint32_t   packed_mouse_pos;        /* +0x90  packed screen coords (Y<<16 | X) */
    int32_t    mouse_screen_x;          /* +0x94  absolute screen X (from ClientToScreen) */
    int32_t    mouse_screen_y;          /* +0x98  absolute screen Y (from ClientToScreen) */
    int32_t    mouse_world_x;           /* +0x9C  current mouse world X (from ScreenToWorld) */
    int32_t    mouse_world_y;           /* +0xA0  current mouse world Y */

    /* ---- Click / input flags (+0xA4..+0xC3) ---- */
    uint8_t    left_click_flag;         /* +0xA4  1=left click pending this frame */
    uint8_t    _pad_A5[3];              /* +0xA5-0xA7, alignment */
    uint32_t   left_click_screen_pos;   /* +0xA8  packed screen when left click occurred */
    int32_t    left_click_world_x;      /* +0xAC  world X from left-click conversion */
    int32_t    left_click_world_y;      /* +0xB0  world Y from left-click conversion */
    uint8_t    right_click_flag;        /* +0xB4  1=right click pending this frame */
    uint8_t    _pad_B5[3];              /* +0xB5-0xB7, alignment */
    uint32_t   right_click_screen_pos;  /* +0xB8  packed screen when right click occurred */
    int32_t    right_click_world_x;     /* +0xBC  world X from right-click conversion */
    int32_t    right_click_world_y;     /* +0xC0  world Y from right-click conversion */

    /* ---- Mouse move / drag state (+0xC4..+0xE7) ---- */
    uint8_t    mouse_move_flag;         /* +0xC4  1=mouse moved this frame */
    uint8_t    _pad_C5[3];              /* +0xC5-0xC7, alignment */
    uint32_t   mouse_move_screen_pos;   /* +0xC8  packed screen pos at mouse move time */
    int32_t    mouse_move_world_x;      /* +0xCC  world X from mouse-move conversion */
    int32_t    mouse_move_world_y;      /* +0xD0  world Y from mouse-move conversion */
    uint8_t    mouse_drag_flag;         /* +0xD4  1=click-drag this frame */
    uint8_t    _pad_D5[3];              /* +0xD5-0xD7, alignment */
    uint32_t   mouse_drag_screen_pos;   /* +0xD8  packed screen pos at drag start */
    int32_t    mouse_drag_world_x;      /* +0xDC  world X from drag conversion */
    int32_t    mouse_drag_world_y;      /* +0xE0  world Y from drag conversion */
    uint8_t    mouse_drag_mode;         /* +0xE4  1=currently in drag/resize operation */
    uint8_t    mouse_drag_handled;      /* +0xE5  1=drag already processed this frame */
    uint8_t    click_on_selected;       /* +0xE6  1=re-clicked on already-selected object */
    uint8_t    _pad_E7;                 /* +0xE7 */

    /* ---- Selection state (+0xE8..+0xEF) ---- */
    Building*  selected_object;         /* +0xE8  currently selected Building (or NULL) */
    uint8_t    selected_visible;        /* +0xEC  1=cursor/outline visible for selected object */
    uint8_t    _pad_ED[3];              /* +0xED-0xEF */

    /* ---- Mouse speed params + cache (+0xF0..+0x10B) ---- */
    int32_t    mouse_spi3[3];           /* +0xF0  SPI_GETMOUSE (setting 3): threshold, speed, accel */
    int32_t    mouse_spi4[3];           /* +0xFC  SPI_GETMOUSE (setting 4): threshold, speed, accel */
    int32_t    busy_cursor_handle;      /* +0x108 cached HCURSOR for "busy.ani" cursor file */

    /* ---- Timer sub-object (+0x10C..+0x11B) ---- */
    /* The binary keeps an inline TimerSlotList here (16 bytes: vtable,
     * items array, count, capacity; see shared/TimerSlotList.h).  The
     * recovered x86 fields are kept flat for offset documentation; the
     * shared TimerSlotList reconstruction does not yet model the running
     * vtable slots Game dispatches (Stop/GetItem/GetCount/Insert). */
    int32_t    timer_sub_ptr;           /* +0x10C inline timer sub-object vtable
                                           (0x477798 init/dead, 0x477758 running) */
    int32_t    timer_array_ptr;         /* +0x110 allocated timer data (0x28 bytes) */
    int32_t    timer_count;             /* +0x114 timer count (10 if alloc succeeded, else 0) */
    int32_t    timer_edit;              /* +0x118 edit/mode counter */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * Game constructor. 0x410510.
     *
     * Calls Entity(-1,-1,0,0), allocates the timer slot-list array
     * (10 entries), sets the compiler-managed Game vtable (binary
     * 0x477718), saves SPI_GETMOUSE settings 3 and 4, calls
     * SetScreenMode(0,1,0), clears all event flags, then runs one
     * initial Game_Update tick.
     */
    Game();

    /**
     * Virtual destructor body. 0x410660 (scalar deleting wrapper) /
     * 0x410680 (body).
     *
     * The body frees the timer array at +0x110, resets the inline timer
     * sub-object to its dead vtable, and runs the Entity base destructor
     * (GameObject_DtorBody).  NOTE: this is NOT Game::Shutdown (0x410700),
     * which is a separate cleanup function.
     */
    virtual ~Game();

    /* ================================================================ */
    /* Per-frame update pipeline                                        */
    /* ================================================================ */

    /**
     * MAIN per-frame update. 0x410840 — vtable [10].
     *
     *   1. Skip everything unless visible (+0x24) is set.
     *   2. Entity::Update (animation state machine).
     *   3. has_event = left_click || mouse_move || right_click ||
     *      screensaver_active.
     *   4. If the parent resource (+0x40) is non-null:
     *        a. screensaver_active: IsScreensaverActive; if the cursor
     *           moved, ClearMouseMode + clear the flag.
     *        b. left_click_flag: HandleLeftClick.
     *        c. right_click_flag: HandleRightClick.
     *        d. selected && selected_visible && !click_on_selected:
     *           selected->CheckPlacementCollision(world_x, world_y);
     *           on hit, DeselectGameObject; when dragging, also clear
     *           the selection.
     *        e. mouse_move_flag: ScreenToWorld, store world pos,
     *           clear the flag + drag_mode, TileMap_ClearInputProcessedFlag.
     *        f. mouse_drag_flag: ScreenToWorld, store drag world pos,
     *           clear right-click/drag flags, TileMap_ClearInputProcessedFlag.
     *   5. If has_event: dispatch cursor mode (UpdateCursorMode), plus a
     *      fallback 0x1400 sound when initialized (+0x18) is not 1.
     *
     * Called from GameLoop_FrameUpdate (0x45C3C0) every frame.
     */
    void Update() override;

    /**
     * Dispatch game-mode-specific cursor feedback. 0x411760.
     *
     * g_game_mode == 3 (town):  UpdateInputState
     * g_game_mode == 4 (build): HandleCursorHover + ClearMouseMode
     * Otherwise:                PlaySound(0x1400)
     * Also plays the fallback 0x1400 when initialized != 1.
     */
    void UpdateCursorMode();

    /**
     * Build-mode cursor hover sound engine. 0x4117B0.
     *
     * Decision tree: placement validity, scripted-object drag,
     * town/second overlay region, build mode, edge-of-world positioning.
     * Selects and plays the correct cursor sound resource.
     */
    void HandleCursorHover();

    /**
     * Town-mode cursor input handler. 0x411AE0.
     *
     * Tests cursor against scripted-object drag, DDRAW drag rect,
     * DDRAW building sprite, and town view. Plays contextual sound.
     */
    void UpdateInputState();

    /**
     * Left-click world dispatch. 0x411000.
     *
     * Priority: town/postcard view -> DDRAW building -> scripted object
     * -> selected-object placement -> BuildingMgr/TileMap chain.
     */
    void HandleLeftClick();

    /**
     * Right-click world dispatch. 0x411230.
     *
     * Type match? Cycle animation. Mismatch? BuildingMgr/World/TileMap.
     * Deselects unless no_deselect flag set.
     */
    void HandleRightClick();

    /**
     * Mouse-button release handler (build mode). 0x410D20.
     *
     * Clears the timer slot-list selection state, dispatches the click
     * to the scripted object or building footprint tiles, and signals a
     * redraw (action_state = 0x400).
     */
    void ClearMouseMode();

    /* ================================================================ */
    /* Cursor / selection management                                    */
    /* ================================================================ */

    /**
     * Set screen capture/cursor mode (state machine). 0x411DC0.
     *
     * capture=0: release capture, show IDC_ARROW.
     * capture=1+custom=0: hide cursor (gameplay).
     * capture=1+custom=1: show busy.ani (build-mode scroll).
     * When capturing, packs the current cursor position into +0x90 and
     * runs one IsScreensaverActive check.
     *
     * @param capture  0=release, 1=capture
     * @param show     0=no force show, 1=ensure cursor visible
     * @param custom   0=standard hidden, 1=use busy.ani
     */
    void SetScreenMode(uint8_t capture, uint8_t show, uint8_t custom);

    /**
     * Draw the game cursor at a screen rectangle. 0x411C50.
     *
     * Checks visible and cursor_disabled flags, draws the selected
     * object sprite (vtable[11]), then GameObject_Draw /
     * GameObject_DrawConnected.
     */
    void SetCursorByResourceId(int left, int top, int right, int bottom,
                               int enable_scroll);

    /**
     * Redraw cursor overlay within the game's own screen rect. 0x411D10.
     *
     * Sister of SetCursorByResourceId — uses the stored RECT at +0x08.
     */
    void ResetCursor();

    /**
     * Select an object (building/vehicle) from the UI palette. 0x4113A0.
     *
     * Removes it from the sorted building/vehicle list, plays the
     * selection sound. Pass NULL to deselect.
     *
     * @param obj  Building to select, or NULL to deselect
     * @return 1 if selected, 0 if deselected
     */
    int SelectGameObject(Building* obj);

    /**
     * Re-insert the selected object into the sorted list. 0x411580.
     *
     * Removes the selection highlight, re-sorts building/vehicle by
     * distance, tracks the tile overlap count (+0x88).
     */
    void DeselectGameObject();

    /* ================================================================ */
    /* Mouse tracking / utility                                         */
    /* ================================================================ */

    /**
     * Screensaver idle check — core mouse tracking. 0x410A40.
     *
     * Reads the packed mouse pos (+0x90), bounds-tests against the
     * client offset, ClientToScreen + WindowFromPoint, converts to world
     * coords, handles drag-scrolling, and notifies the entity under the
     * cursor via vtable[3] (MoveTo). Returns 1 when the cursor is over
     * game content.
     */
    int IsScreensaverActive();

    /**
     * Convert screen pixel to game world coords. 0x412060.
     *
     * Adds the viewport offset, clamps to the playable area, constrains
     * by active resource bounds when the parent is non-null and not
     * type-5, and snaps to the 16px grid when the scripted object does
     * not claim the point.
     *
     * @param out_xy     int32_t[2] — receives (world_x, world_y)
     * @param screen_x   screen X
     * @param screen_y   screen Y
     */
    void ScreenToWorld(int32_t* out_xy, int screen_x, int screen_y);

    /**
     * Play a sound resource by ID. 0x411FB0.
     *
     * Loads via ResourceManager, calls InitBase (vtable[6]) to start
     * playback, then repositions the Game entity for stereo panning:
     * type-5 parents use the mouse world pos minus their offset fields;
     * otherwise cursor_sound_id = parent resource id and the entity is
     * placed at (mouse_world_x, mouse_world_y - parent +0x16D).
     */
    void PlaySound(int sound_id);

    /**
     * Shutdown/cleanup. 0x410700.
     *
     * Restores SPI_SETMOUSE setting 4, stops the timer sub-object
     * (vtable[5]), switches to windowed mode via SetScreenMode(0,1,0),
     * and releases resources via InitBase(0,-1,0). Called from
     * CGWND_Cleanup (0x407AC8) — NOT the C++ destructor.
     */
    void Shutdown();

    /* ================================================================ */
    /* Startup resource loading                                          */
    /* ================================================================ */

    /**
     * Load and cache 4 intro/title-screen sounds. 0x410750.
     *
     * Preloads sound resources 0x5014, 0x5015, 0x501A, 0x501B and sets
     * the keep-alive flag on each so they remain in memory after
     * release.  (The original reuses the +0x110 timer array slot as
     * scratch; the C++ port uses a local instead — final state is
     * identical and the scratch is invisible to other code.)
     *
     * Called once during GameLoop_Setup (0x406D82).
     *
     * @return true if all 4 sounds were found and loaded
     */
    bool LoadIntroSounds();

    /**
     * Check the screensaver timeout and clear it if expired. 0x410A20.
     *
     * Calls IsScreensaverActive(); if the cursor has moved outside the
     * game window, clears the screensaver_active flag at +0x8E and calls
     * ClearMouseMode to release mouse capture.
     */
    void CheckScreensaverTimeout();
};

/* ================================================================== */
/* Free functions associated with Game                                 */
/* ================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Per-frame game loop — top-level heartbeat. 0x45C3C0.
 *
 * Called from WinMain: updates time, netman, world tick, scripted
 * objects, tooltips, buildings, tile cache; calls g_game->Update().
 * Implemented in core/GameLoop.cpp.
 */
void GameLoop_FrameUpdate(void);

/**
 * Render network player slots in the game setup screen. 0x405520.
 *
 * Operates on a GameSetupPanel, accesses g_dplay_config, calls
 * NET_GetPlayerAddress. NOT a Game method — separate class (ui/).
 */
void CGWND_GameSetup_RenderPlayerSlots(void* panel_this);

#ifdef __cplusplus
}

/* Game_CheckTimeInRange / Game_IsPositionBetween are __cdecl C-style
 * helpers but are declared and defined with C++ linkage across the
 * codebase (Building.cpp, BuildingMgrObjectGroup.cpp, InputMgr.cpp);
 * the header mirrors that convention. */

/**
 * Check if time falls within [start, end] window. 0x412710.
 *
 * Supports overnight wrap (end < start). -1 sentinel = inactive.
 * Struct layout: { hour, minute, second } (tm-style: second@0,
 * minute@4, hour@8).
 *
 * @param time   Pointer to current time struct {second, minute, hour}
 * @param start  Pointer to start time struct
 * @param end    Pointer to end time struct
 * @return 1 if within range, 0 otherwise
 */
int Game_CheckTimeInRange(int* time, int* start, int* end);

/**
 * Check if a date/time falls within [start, end] range. 0x412790.
 *
 * Uses a month-day lookup table at 0x47E410 for the day-of-year
 * calculation, then checks the time component with overnight-wrap
 * support.  The three structs each contain:
 * { seconds, minutes, hours, day_of_month, month }. -1 sentinel
 * fields are treated as "match anything".
 *
 * Called by: INPUT_EditSetFocus for scheduled event validation.
 *
 * @param current  Pointer to current date/time struct
 * @param start    Pointer to start date/time struct
 * @param end      Pointer to end date/time struct
 * @return 1 if current falls within [start, end], 0 otherwise
 */
int Game_IsPositionBetween(int* current, int* start, int* end);
#endif
