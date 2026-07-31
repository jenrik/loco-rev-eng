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
 * Game_Update per-frame loop.
 *
 * A single global instance lives at g_game (0x4A98D8).
 *
 * Size: ~0x11C bytes (Entity base 0x88 + Game fields 0x94 bytes)
 * Vtable: 0x477718 (extends Entity vtable 0x477488)
 *
 * Class hierarchy:
 *   GameObject (root, type=1, vtable 0x477820)
 *     +-- Entity (type=2, vtable 0x477488)
 *          +-- Game (vtable 0x477718)  <- this class
 *
 * Destructor chain:
 *   vtable[0] = Game_Dtor (0x410660) — scalar deleting destructor
 *     calls ~Game() (Game_BaseDtor, 0x410680) — SEH-protected body
 *       frees timer array, calls GameObject_DtorBody
 *
 * Vtable layout (inherits from GameObject/Entity):
 *   [0]  +0x00: scalar deleting destructor (Game_Dtor, 0x410660)
 *                   → calls ~Game() base destructor (Game_BaseDtor, 0x410680)
 *   [1]  +0x04: StopSound (inherited)
 *   [2]  +0x08: (release resource?)
 *   [3]  +0x0C: HitTest dispatch (inherited)
 *   [4]  +0x10: (unknown)
 *   [5]  +0x14: (unknown)
 *   [6]  +0x18: InitBase (resource loading + setup)
 *   [7]  +0x1C: SetAnimState (animation dispatch)
 *   [8]  +0x20: SetFrame (update source rect)
 *   [9]  +0x24: SetName
 *   [10] +0x28: Draw (single-frame)
 *   [11] +0x2C: DrawConnected (multi-frame)
 *   [12] +0x30: OnTimerTick
 *   [13] +0x34: (unknown)
 *   [14] +0x38: AnimStateSelect
 *   -- All Game-specific methods below are non-virtual (direct calls) --
 *
 * Sound resource IDs used by cursor hover engine:
 *   0x1400 = generic click/feedback sound
 *   0x1402 = edge-of-world / blocked
 *   0x1404 = placement valid / action accepted
 *   0x1405 = tooltip/contextual cursor
 *   0x5015 = left-click action success
 *   0x502C = right-click action success
 *   cursor edge sounds:
 *     0xC1A / 0x3409 = horizontal scroll
 *     0xC1C / 0x3408 = vertical scroll
 *     0xC26 = scroll right edge
 *     0xC28 = scroll top edge
 *     0xC2A = scroll bottom edge
 *     0xC2C = scroll left edge
 *     0xC42 = scroll right (drag variant)
 *     0xC44 = scroll left (drag variant)
 *     0xC46 = scroll top (drag variant)
 *     0xC48 = scroll bottom (drag variant)
 */

#pragma once

#include "Entity.h"
#include "../shared/types.h"

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */
struct Timer;        /* Sub-object at +0x10C, collection-inherited */

/* ================================================================== */
/* Global singleton address                                             */
/* ================================================================== */
#define ADDR_g_game                     0x004A98D8  /* g_game singleton */

/* ================================================================== */
/* Game class -- gameplay state manager (Entity-derived)                 */
/* ================================================================== */
class Game : public Entity {
public:
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
    int32_t    selected_object_ptr;     /* +0xE8  pointer to currently selected GameObject */
    uint8_t    selected_visible;        /* +0xEC  1=cursor/outline visible for selected object */
    uint8_t    _pad_ED[3];              /* +0xED-0xEF */

    /* ---- Mouse speed params + cache (+0xF0..+0x10B) ---- */
    int32_t    mouse_spi3[3];           /* +0xF0  SPI_GETMOUSE (setting 3): threshold, speed, accel */
    int32_t    mouse_spi4[3];           /* +0xFC  SPI_GETMOUSE (setting 4): threshold, speed, accel */
    int32_t    busy_cursor_handle;      /* +0x108 cached HCURSOR for "busy.ani" cursor file */

    /* ---- Timer sub-object (+0x10C..+0x118) ---- */
    int32_t    timer_sub_ptr;           /* +0x10C Timer sub-object pointer (vtable 0x477758) */
    int32_t    timer_array_ptr;         /* +0x110 pointer to allocated timer data (0x28 bytes) */
    int32_t    timer_count;             /* +0x114 timer count (10 if alloc succeeded, else 0) */
    int32_t    timer_edit;              /* +0x118 edit/mode counter */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * Game constructor. 0x410510.
     *
     * Calls Entity(-1,-1,0,0), allocates timer array (10 entries), sets
     * the compiler-managed Game vtable, clears all event flags, saves SPI_GETMOUSE
     * (settings 3 and 4), calls SetScreenMode(0,1,0), then runs one
     * initial Game_Update tick.
     */
    Game();

    /**
     * Virtual destructor. 0x410660 / 0x410680.
     *
     * MSVC scalar deleting destructor (vtable[0]) and base object destructor.
     * Uses the compiler-managed Game vtable, frees timer array buffer at +0x110,
     * resets timer sub-object, calls Entity base destructor.
     */
    virtual ~Game();

    /* ================================================================ */
    /* Per-frame update pipeline                                        */
    /* ================================================================ */

    /**
     * MAIN per-frame update (7-step pipeline). 0x410840.
     *
     *   1. GameObject_Update (animation state machine)
     *   2. Poll input flags (left-click, right-click, mouse-move, screensaver)
     *   3. If screensaver_active: check IsScreensaverActive timeout, clear if expired
     *   4. If left_click_flag: HandleLeftClick
     *   5. If right_click_flag: HandleRightClick
     *   6. If selected_object && visible && !click_on_selected:
     *        hit-test selected object -> deselect on re-click
     *   7. If mouse_move_flag: ScreenToWorld, update world pos, clear flag
     *   8. If mouse_drag_flag: ScreenToWorld, update drag, clear flags
     *   9. After events: dispatch UpdateCursorMode based on g_game_mode
     *
     * Called from: GameLoop_FrameUpdate (0x45C3C0) every frame.
     */
    void Update() override;

    /**
     * Dispatch game-mode-specific cursor feedback. 0x411760.
     *
     * g_game_mode == 3 (town):  UpdateInputState
     * g_game_mode == 4 (build): HandleCursorHover + ClearMouseMode
     * Otherwise:                PlaySound(0x1400)
     * Also plays fallback if initialized != 1.
     */
    void UpdateCursorMode();

    /**
     * Build-mode cursor hover sound engine. 0x4117B0.
     *
     * Complex decision tree: placement validity, scripted-object drag,
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
     * Clears selection state in Timer, dispatches click to scripted
     * object or building footprint, signals redraw (action_state=0x400).
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
     *
     * @param capture  0=release, 1=capture
     * @param show     0=no force show, 1=ensure cursor visible
     * @param custom   0=standard hidden, 1=use busy.ani
     */
    void SetScreenMode(uint8_t capture, uint8_t show, uint8_t custom);

    /**
     * Draw game cursor at given screen rectangle. 0x411C50.
     *
     * Checks visible and cursor_disabled flags, draws selected object
     * sprite, GameObject_Draw, and GameObject_DrawConnected.
     */
    void SetCursorByResourceId(RECT* rect, int enable_scroll);

    /**
     * Redraw cursor overlay within game's own rect. 0x411D10.
     *
     * Sister of SetCursorByResourceId -- uses stored RECT at +0x08.
     */
    void ResetCursor();

    /**
     * Select an object (building/vehicle) from list. 0x4113A0.
     *
     * Removes from sorted list, plays selection sound.
     * Pass NULL to deselect.
     *
     * @param obj  GameObject to select, or NULL to deselect
     * @return 1 if selected, 0 if deselected
     */
    int SelectGameObject(GameObject* obj);

    /**
     * Re-insert selected object into sorted list on deselect. 0x411580.
     *
     * Removes selection highlight, re-sorts building/vehicle by distance,
     * tracks tile overlap count.
     */
    void DeselectGameObject();

    /* ================================================================ */
    /* Mouse tracking / utility                                         */
    /* ================================================================ */

    /**
     * Screensaver idle check -- core mouse tracking. 0x410A40.
     *
     * Reads packed mouse pos, tests bounds, WindowFromPoint, converts
     * to world coords, handles drag-scrolling, notifies entity under
     * cursor via vtable[3]. Returns 1 if mouse moved over game content.
     */
    int IsScreensaverActive();

    /**
     * Convert screen pixel to game world coords. 0x412060.
     *
     * Adds viewport offset, clamps to playable area, constrains by
     * active resource bounds if parent is non-null and not type-5.
     * Snaps to 16px grid on scripted-object miss.
     */
    void ScreenToWorld(int screen_x, int screen_y,
                       int32_t* out_x, int32_t* out_y);

    /**
     * Play a sound resource by ID. 0x411FB0.
     *
     * Loads via ResourceManager, calls vtable[6] (InitBase), repositions
     * Game entity for stereo audio panning.
     */
    void PlaySound(int sound_id);

    /**
     * Shutdown/cleanup. 0x410700.
     *
     * Restores SPI_SETMOUSE, stops timer, switches to windowed mode,
     * releases resources. Counterpart to Game_Ctor.
     */
    void Shutdown();

    /* ================================================================ */
    /* Startup resource loading                                          */
    /* ================================================================ */

    /**
     * Load and cache 4 intro/title-screen sounds. 0x410750.
     *
     * Preloads sound resources 0x5014, 0x5015, 0x501A, 0x501B and sets
     * the keep-alive flag on each so they remain in memory after release.
     * The 4 sound IDs correspond to intro, click-select, and two other
     * title-screen sound effects.
     *
     * Called once during GameLoop_Setup (0x406D82).
     *
     * @return true if all 4 sounds were found and loaded
     */
    bool LoadIntroSounds();

    /**
     * Check screensaver timeout and clear if expired. 0x410A20.
     *
     * Calls IsScreensaverActive(); if the cursor has moved outside the
     * game window, clears the screensaver_active flag at +0x8E and
     * calls ClearMouseMode to release mouse capture.
     *
     * Called from Game::Update() when screensaver_active flag is set.
     */
    void CheckScreensaverTimeout();
};

/* ================================================================== */
/* C-linkage (free) functions associated with Game                     */
/* ================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Per-frame game loop -- top-level heartbeat. 0x45C3C0.
 *
 * Called from WinMain: updates time, netman, world tick, scripted
 * objects, tooltips, buildings, tile cache; calls g_game.Update().
 */
void GameLoop_FrameUpdate(void);

/**
 * Check if time falls within [start, end] window. 0x412710.
 *
 * Supports overnight wrap (end < start). -1 sentinel = inactive.
 * Struct layout: { hour, minute, second }.
 *
 * @param time   Pointer to current time struct {hour, minute, second}
 * @param start  Pointer to start time struct
 * @param end    Pointer to end time struct
 * @return 1 if within range, 0 otherwise
 */
int Game_CheckTimeInRange(int* time, int* start, int* end);

/**
 * Check if a date/time falls within [start, end] range. 0x412790.
 *
 * Uses a month-day lookup table at 0x47E410 for day-of-year calculation,
 * then checks time component with overnight-wrap support. The three
 * structs each contain: { seconds, minutes, hours, day_of_month, month }.
 * -1 sentinel fields are treated as "match anything".
 *
 * Called by: INPUT_EditSetFocus for scheduled event validation.
 *
 * @param current  Pointer to current date/time struct
 * @param start    Pointer to start date/time struct
 * @param end      Pointer to end date/time struct
 * @return 1 if current falls within [start, end], 0 otherwise
 */
int Game_IsPositionBetween(int* current, int* start, int* end);

/**
 * Render network player slots in game setup screen. 0x405520.
 *
 * Operates on a GameSetupPanel, accesses g_dplay_config, calls
 * NET_GetPlayerAddress. NOT a Game method -- separate class.
 */
void CGWND_GameSetup_RenderPlayerSlots(void* panel_this);

#ifdef __cplusplus
}
#endif
