/**
 * DDRAW_Building.h — Building/station/vehicle sprite manager class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Split out of graphics/DDRAW.h so translation units that cannot include
 * that header (its g_tilemap/g_ddraw_building/g_primary_surface globals
 * conflict in type with world/tilemap.h's — see the NOTE in that header)
 * can still see the real DDRAW_Building class and call its methods
 * directly, instead of going through a void*-taking bridge function.
 * core/Game.cpp is the motivating case: BUG-mode3-input-processing-
 * crashes.md's Game::UpdateInputState/HandleLeftClick call
 * DDRAW_Building::HitTest/HitTestWithDrag on g_ddraw_building.
 */
#pragma once

#include "../shared/types.h"
#include "../core/GameObject.h"
#include "../game/Panel.h"

// Status: TRANSCRIBED
class Building;

/* ================================================================== */
/* DDRAW_Building — Building/station/vehicle sprite manager            */
/* Extends RESDATA (vtable 0x478274). Vtable: 0x478548.               */
/* Size: ~0x5B0 bytes.                                                 */
/* Global singleton at 0x4A9EF0 (g_ddraw_building).                    */
/* ================================================================== */

/* DDRAW_Building really extends Panel (which extends GameObject) in the
 * original binary, not a flat/duplicated layout: its constructor
 * (0x4589B0, DDRAW_InitSprites) calls RESDATA_BaseInit(this) (0x4544E0),
 * which itself calls GameObject_BaseCtor(this, -1, -1, 0, 0) directly on
 * `this`'s own base address, then overwrites the vtable to Panel's
 * (0x4784C8, "VTBL_PANEL" per Ghidra's own decompiler comment on
 * RESDATA_BaseInit) — textbook MSVC base-then-derived constructor
 * vtable installation, confirmed by decompiling 0x458AA6 and 0x4544E0.
 * Exactly the same pattern core/GameView.h/.cpp already uses correctly
 * (Status: INTEGRATED) for the sibling Panel-family class GameView —
 * this class follows that established precedent instead of a fresh
 * GameObject-only guess. HitTest (0x459D60) calls GameObject_PtInRect
 * (this, x, y) — a direct, non-virtual call reusing the inherited
 * screen_rect (+0x08 x/left, +0x0C y/top, +0x10 right, +0x14 bottom in
 * the original; host-native offsets via real C++ inheritance here)
 * rather than a disconnected local x/y pair. HitTestWithDrag (0x45A740)
 * calls RESDATA_HitTestChildren(this, x, y) directly — Panel's own,
 * already-implemented HitTestChildren(int, int) method (0x4549E0,
 * game/Panel.h/.cpp), not the disconnected free-function stub several
 * other files (shared/stubs_impl.cpp, town/Town.cpp,
 * world/scriptengine.cpp) separately and inconsistently declare under
 * the same name. See BUG-mode3-input-processing-crashes.md for the
 * crash this fixes and world/scriptengine.h's identical finding for
 * RESDATA_ScriptedObject (which should get the same treatment). */
class DDRAW_Building : public Panel {
public:
    DDRAW_Building(const DDRAW_Building&) = delete;
    DDRAW_Building& operator=(const DDRAW_Building&) = delete;

    /* ================================================================ */
    /* Fields                                                           */
    /* ================================================================ */

    /* vtable, type (=0xD for DDRAW_Building), and screen_rect (x/y/right/
     * bottom) are inherited from GameObject — see the class comment
     * above. */

    // +0x18 to +0xDF: further RESDATA base fields (sprite tree, resource
    // data, scroll offsets, etc.) not yet named/used by any ported method.

    /* +0xE0 Embedded GameObject for child entity hit-test / tooltip
     * dispatch. Really embedded (its own address, not its value, is the
     * object base — every caller takes &this->sub_object_1, never
     * dereferences it as a plain pointer): a real GameObject member gets
     * a valid vtable for free from the compiler, instead of the
     * previous void* staying null forever on this host build (nothing
     * in the #ifndef _WIN32 constructor path ever set it), which
     * crashed the instant HitTest's DDRAW_SpriteView::hit_test(x, y)
     * dispatch (slot 2, aliases GameObject::PtInRect at the same slot)
     * was ever actually reached — see
     * BUG-mode3-input-processing-crashes.md. */
    GameObject  sub_object_1;           // +0xE0

    // +0xE4 to +0x167: more RESDATA fields
    // +0xE8..+0xF4: invalidate rect (left/top/right/bottom, 4 dwords)
    // +0x108: hover state (dword, 0=off, 1=on)

    /* --- Pattern sprite array (inline GameObjects, each 0x88 bytes) --- */
    /* These are NOT pointers — they are embedded GameObject instances.    */
    /* Initialized via CRT_memset_pattern from a 0x88-byte template.      */
    /* Used for station/vehicle track-piece sprites.                      */
    uint8_t     pattern_sprites[4 * 0x88]; // +0x168  to  +0x168+0x220
                                           // [0]: +0x168, [1]: +0x1F0,
                                           // [2]: +0x278, [3]: +0x300

    // Each pattern sprite (0x88 bytes) has:
    //   +0x00: vtable, +0x40: pointer to animation state object
    //   The animation state object has a word at +0x1A for random countdown

    // +0x388 to +0x397: more RESDATA fields

    /* NOTE: building_type and panel_state are at +0x398 and +0x39c below.
       The earlier offsets in comments (0xE6/0xE7) were placeholders. */

    void*       popup_panel;            // +0x3A0  Embedded GameObject for
                                        //         popup window (station/vehicle
                                        //         name display)
    // +0x3B8: popup_active flag (byte, checked in DispatchToSubObjects)
    // +0x3C8: popup scroll state (dword, station list scroll tracking)

    int32_t     popup_visible;          // +0x3E0  non-zero = popup panel visible

    // +0x3E4: more fields
    void*       popup_audio_channel;    // +0x3E8  AudioChannel* — ambient/name-
                                        //         edit sound channel; released
                                        //         in SelectBuilding (0x459180)
                                        //         via AudioChannel_Release (0x40ECA0)
                                        //         when non-null
    // +0x3EC to +0x427: more fields

    void*       pattern_container;      // +0x428  Embedded GameObject*
                                        //         pattern container
    uint8_t     pattern_visible;        // +0x44C  non-zero = pattern container visible

    // +0x450 to +0x4AF: more fields
    // +0x47C: pattern_update_flag (dword, checked nonzero in UpdateBuilding)

    void*       track_sprite;           // +0x4B0  Embedded GameObject*
                                        //         track sprite entity
    uint8_t     track_visible;          // +0x4D4  non-zero = track sprite visible
    void*       track_entity;           // +0x4F0  entity pointer for vehicle track

    // +0x4F4 to +0x52F: more fields

    // Day/night animation fields at +0x530..+0x548

    /* --- Selection and sprite slot fields --- */
    void*       selected_entity;        // +0x538  Building* currently selected
    int32_t     station_list_offset;    // +0x53C  scroll offset in station list
    void*       tooltip_text_input;     // +0x540  RESDATA SoundObject for name editing

    /* Station/vehicle name sprites — 8 sprite pointers                   */
    void*       station_name_sprites[8];// +0x544..+0x560  row label sprites
                                        // Used by DDRAW_RefreshStationList,
                                        // DDRAW_UpdateStationSprites,
                                        // DDRAW_UpdateVehicleSprites

    /* UI element sprite pointers (loaded by DDRAW_InitBuildingSprites)   */
    void*       wheel_left_sprite;      // +0x564  track wheel sprite (res 0x3802)
    void*       wheel_right_sprite;     // +0x568  track wheel sprite (res 0x3803)
    void*       exit_sign_sprite;       // +0x56C  station exit sign (res 0x2C07)
    void*       entry_sign_sprite;      // +0x570  station entry sign (res 0x2C08)
    void*       up_arrow_sprite;        // +0x574  scroll up/left turn (res 0x3864)
    void*       down_arrow_sprite;      // +0x578  scroll down/right turn (res 0x3865)
    void*       left_turn_sprite;       // +0x57C  track left turn (res 0x3867)
    void*       right_turn_sprite;      // +0x580  track right turn (res 0x3866)

    /* Colored dot sprites (occupancy indicators)                         */
    void*       colored_dots_A[4];      // +0x584..+0x590  occupancy dots (res 0x3868)
    void*       colored_dots_B[4];      // +0x594..+0x5A0  occupancy dots (res 0x3869)

    /* Track arrow sprites                                               */
    void*       track_arrow_left;       // +0x5A4  vehicle direction arrow (res 0x380E)
    void*       track_arrow_mid;        // +0x5A8  vehicle direction arrow (res 0x380F)
    void*       track_arrow_right;      // +0x5AC  vehicle direction arrow (res 0x3810)

    /* Note: Total struct size ~0x5B0 bytes */

    /* ================================================================ */
    /* Key fields accessed via raw offset (confirmed by disassembly)     */
    /* ================================================================ */

    // building_type: word at +0x398 (not +0xE6 as previously guessed)
    //   2=road, 3=station, 4=track, 6=vehicle, 7=station-list,
    //   8=pattern, 0xC=other
    // panel_state/front_back: byte at +0x39c (0=sprite back, 1=sprite front)
    // sprites_visible: byte at +0x39d (visibility flag for colored dots etc.)

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * DDRAW_Building constructor (factory).
     * Address: 0x4589B0  (as DDRAW_InitSprites)
     *
     * Calls RESDATA_BaseInit for base class, creates 4 pattern GameObjects
     * via memset_pattern, creates 5 sub-object GameObjects (child entity,
     * popup panel, pattern container, track sprite +1), clears all sprite
     * slot fields, sets vtable to 0x478548 and type to 0xD.
     *
     * Called by: CGWND_InitMode1 via init sequence (0x45C6E5)
     *
     * @param mem  pre-allocated memory (~0x5B0 bytes from operator_new)
     * @return     this pointer
     */
    static DDRAW_Building* Create(void* mem);

    /** Native C++ constructor for placement construction by Create. */
    DDRAW_Building();

    /**
     * Virtual destructor corresponding to the binary's destructor body at
     * 0x458B00. The scalar-deleting wrapper remains Destroy().
     */
    virtual ~DDRAW_Building();

    /* vtable[0] = scalar deleting destructor (0x458AD0):
     * compiler-generated; wraps ~DDRAW_Building and conditionally
     * calls operator delete when flags & 1. */

    /* ================================================================ */
    /* Cleanup methods                                                   */
    /* ================================================================ */

    /**
     * CleanupSprites — full destructor body (not vtable-dispatched).
     * Address: 0x458B00
     *
     * Reverse-order cleanup of sub-objects (GameObject_DtorBody) and
     * pattern objects (CRT_free_pattern), then base class cleanup via
     * RESDATA_DtorBody. Protected by SEH try-level tracking.
     *
     * Called by: Destroy (scalar dtor), init failure path
     */
    void CleanupSprites();

    /**
     * InvalidateAll — release all sub-object resources.
     * Address: 0x458BB0
     *
     * Calls vtable[6] (LoadChildResource with -1 to release) on self
     * and all sub-objects (sub_object_1, popup_panel, pattern_container,
     * track_sprite, 4 pattern sprites). Then calls RESDATA_DtorBase.
     * Clears 13 sprite slot fields (+0x538..+0x5AC) to 0.
     *
     * Called by: CleanupSprites
     */
    void InvalidateAll();

    /* ================================================================ */
    /* Initialization                                                    */
    /* ================================================================ */

    /**
     * InitBuildingSprites — load building/station UI sprites from resources.
     * Address: 0x458C90
     *
     * Loads name sprite (0x2C09), 8 row label sprites, colored dot sprites
     * (0x3868, 0x3869), wheel sprites (0x3802/0x3803), exit/entry signs
     * (0x2C07/0x2C08), 4 directional sprites (0x3864-0x3867), and 3 track
     * arrow sprites (0x380E-0x3810). Sets sprite positions via vtable[3].
     * Loads 5 sub-object resources (0x3809, 0x2402, 0x386A-0x386C).
     *
     * Called by: GameLoop setup path
     *
     * @return  1 if all resources loaded, 0 on failure
     */
    uint8_t InitBuildingSprites();

    /* ================================================================ */
    /* Building selection                                                */
    /* ================================================================ */

    /**
     * SelectBuilding — select/deselect a building entity.
     * Address: 0x459180
     *
     * Per-building-type selection handler:
     *   type 2/4 (road/track): updates station sprites, positions popup
     *   type 3 (station):     sets popup visible, activates name editing
     *   type 6 (vehicle):     updates vehicle sprites, positions popup
     *   type 7 (station-list): scrolls station list, refreshes sprites
     *   type 8 (pattern):     sets zoom on all 8 pattern pieces
     *   type 0xC (other):     updates station sprites
     * Deselection: clears active flag, resets panel, releases audio.
     *
     * Called by: Game_HandleLeftClick (via dispatch), DDRAW_BuildingClickHandler,
     *            Town_SelectBuilding, DDRAW_UpdateBuilding
     *
     * @param entity  Building* to select, or NULL/0 to deselect
     * @return        active flag byte at +0x88
     */
    uint8_t SelectBuilding(Building* entity);

    /**
     * ClampToViewport — clamp sprite position to viewport bounds.
     * Address: 0x459720
     *
     * Checks intersection with town_view and town_overlay rects.
     * If intersecting, repositions sprite relative to client offset.
     * Then clamps X/Y separately to prevent sprite from leaving viewport.
     *
     * Called by: SelectBuilding (at end)
     */
    void ClampToViewport();

    /* ================================================================ */
    /* Per-frame update                                                  */
    /* ================================================================ */

    /**
     * UpdateBuildingSprites — per-frame refresh of all popup sprites.
     * Address: 0x4597E0
     *
     * Updates visibility and zoom for:
     *   - building name sprite (with text from selected entity)
     *   - wheel sprites (always visible)
     *   - exit/entry signs (based on building_type and panel_state)
     *   - 8 row label sprites (based on building_type and panel_state)
     *   - up/down arrows + left/right turn sprites (station occupancy)
     *   - 4 colored dot pairs (visibility based on sprites_visible flag)
     *   - 4 pattern sprites (visibility based on sprites_visible flag)
     *   - track arrows (zoom managed per vehicle state)
     *
     * Fields accessed: +0x398 (building_type word), +0x39c (panel_state byte),
     * +0x39d (sprites_visible byte), +0x538 (selected_entity),
     * +0x540 (tooltip_text_input), +0x544-0x560 (station_name_sprites[8]),
     * +0x564-0x580 (wheel/arrow sprites), +0x584-0x5A0 (colored dots),
     * +0x5A4-0x5AC (track arrows), +0x44C (pattern_visible),
     * +0x4D4 (track_visible)
     *
     * Called by: SelectBuilding, UpdateBuilding, various station/vehicle
     *            update functions
     */
    void UpdateBuildingSprites();

    /**
     * UpdateSubObject — per-frame update for a single sub-object.
     * Address: 0x459D40
     *
     * Thin wrapper: calls RESDATA_UpdateChild then sub-object 1's
     * vtable[1] (Show/Refresh). Used by frame update loops.
     */
    void UpdateSubObject();

    /**
     * UpdateBuilding — per-frame building selection update.
     * Address: 0x459DA0
     *
     * Handles cursor hover highlight, randomized pattern animation,
     * drag movement, child entity traversal, station list scrolling,
     * vehicle track-piece zoom management, edge-of-track detection.
     * For stations (type 3): plays periodic ambient audio, triggers
     * sprite update when timer counter is below game_time.
     * For vehicles (type 6): manages track arrow zoom (1-3) and
     * detects vehicle departure/arrival via track state fields.
     *
     * Called by: GameLoop_FrameUpdate (0x45C4E6) via vtable dispatch
     */
    void UpdateBuilding();

    /* ================================================================ */
    /* Hit testing and input                                             */
    /* ================================================================ */

    /**
     * HitTest — test if point hits this building sprite manager.
     * Address: 0x459D60
     *
     * Two-stage test: first GameObject_PtInRect on self, then
     * child entity at +0xE0 vtable[2]. Returns 1 if either hits.
     *
     * Called by: Game_HandleLeftClick (dispatch via vtable[3]),
     *            Game_UpdateInputState
     *
     * @param x  world X coordinate
     * @param y  world Y coordinate
     * @return   1 = hit, 0 = miss
     */
    int32_t HitTest(int32_t x, int32_t y);

    /**
     * HitTestWithDrag — extended hit-test for drag operations.
     * Address: 0x45A740
     *
     * Tests child entity (+0xE0) and pattern container (+0x428) for
     * drag selection. Manages drag offset tracking (+0x94/+0x98).
     * Tests track sprite (+0x4B0) for left/right half click zones.
     *
     * Called by: Game_UpdateInputState (via dispatch)
     *
     * @param x  drag start X
     * @param y  drag start Y
     * @return   1 = drag-hit, 0 = miss
     */
    uint8_t HitTestWithDrag(int32_t x, int32_t y);

    /**
     * HandleKeyPress — keyboard input for building selection popup.
     * Address: 0x45B3A0
     *
     * Handles VK_UP (0x26) and VK_DOWN (0x28) for station list scroll.
     * Text input routed to RESDATA_TextInput_HandleChar for name editing.
     * Unhandled keys delegated to town view. Post-keypress actions:
     *   - building_type 6 (vehicle): update vehicle animation state
     *   - building_type 7 (station-list): compact collections, refresh
     *   - other types: forward to entity vtable[0xD]
     *
     * Called by: Town key dispatch
     *
     * @param key_code  virtual key code or character
     * @return          double-byte result: low = handled flag, high = vtable result
     */
    uint32_t HandleKeyPress(uint32_t key_code);

    /**
     * BuildingClickHandler — dispatch building click to active handler.
     * Address: 0x458820
     *
     * Reads action index (+0x0C) and target ID (+0x10) from the click
     * descriptor. If index is 0, calls vtable[7] with the ID. Otherwise
     * calls vtable[6] with (index, id, 0). Plays click sound and selects
     * building if not in town mode and difficulty != hard.
     *
     * Called by: RESDATA_ScriptedObject_HandleToolClick (dispatch)
     *
     * @param desc  click descriptor struct (+0x0C=action, +0x10=target_id)
     */
    void BuildingClickHandler(void* desc);

    /**
     * BuildingShowTooltip — show tooltip for a building UI element.
     * Address: 0x4588B0
     *
     * Reads element descriptor:
     *   +0x20 = tooltip resource ID (must be > 0)
     *   +0x24 = tooltip type/size (short)
     *   +0x28 = anchor type character:
     *     'S' (0x53) = viewport-relative  — x/y added to viewport scroll
     *     'W' (0x57) = window-absolute    — x/y used directly
     *     'U' (0x55)/'D' (0x44) = owner-relative  — x/y added to this->position,
     *          anchor char preserved in call
     *     other = owner-relative, anchor overridden to 'W'
     *   +0x2C = x_offset (int)
     *   +0x30 = y_offset (int)
     *
     * Calls UI_CreateMessageBox(g_tooltip_mgr, res_id, type, anchor, x, y, 1).
     *
     * Called by: RESDATA_ScriptedObject_HandleToolClick (dispatch via vtable)
     *
     * @param desc  UI element descriptor struct (7+ fields)
     */
    void BuildingShowTooltip(void* desc);

    /**
     * BuildingTimeUpdate — day/night animation switch.
     * Address: 0x458940
     *
     * If building has night animation mode (flag at +0x18 check), reads
     * localtime and checks time range [+0x534, +0x548]. When time enters
     * range, switches to night animation. When time exits, switches to
     * default animation. Uses Game_CheckTimeInRange for comparison.
     *
     * Called by: GameLoop_FrameUpdate (0x45C4E6)
     */
    void BuildingTimeUpdate();

    /* ================================================================ */
    /* Station / Vehicle sprite management                              */
    /* ================================================================ */

    /**
     * DispatchToSubObjects — dispatch DrawConnected to all sub-objects.
     * Address: 0x45A1A0
     *
     * Only dispatches when active flag (+0x88) is set. Routes vtable[11]
     * (DrawConnected) to:
     *   1. self via Panel::Draw (draw + dim child rects; the DDRAW_Building
     *      hierarchy's own vtable[11], an override of Entity::Draw)
     *   2. sub_object_1 at +0xE0
     *   3. popup_panel at +0x3A0 (if popup_visible and popup_active byte set)
     *      — called twice: once with vtable[11], once with vtable[12]
     *   4. pattern_container at +0x428 + 4 pattern sprites at +0x168..+0x300
     *      (if pattern_visible byte set)
     *   5. track_sprite at +0x4B0
     *
     * Each sub-object call passes the clip rect (left, top, right, bottom)
     * as a stack RECT and param5/0 as trailing arguments.
     *
     * Called by: TileMap_ProcessRect (dispatch to all map entities)
     *
     * @param left    clip rect left coordinate
     * @param top     clip rect top coordinate
     * @param right   clip rect right coordinate
     * @param bottom  clip rect bottom coordinate
     * @param param5  dispatch parameter (surface/clip context)
     */
    void DispatchToSubObjects(int32_t left, int32_t top, int32_t right,
                              int32_t bottom, void* param5);

    /**
     * RefreshStationList — refresh 8 station name sprites at a scroll offset.
     * Address: 0x45A330
     *
     * Locks building manager collection, walks entries starting at param
     * offset, fills sprite names and sets zoom (2=selected/highlighted,
     * 1=normal). Updates scroll arrow sprites based on list position.
     * Stores updated scroll offset at +0x53C.
     *
     * Called by: SelectBuilding (type 7), HandleKeyPress (up/down)
     *
     * @param start_offset  building list index to start at
     */
    void RefreshStationList(int32_t start_offset);

    /**
     * UpdateStationSprites — refresh station name and occupancy dots.
     * Address: 0x45A400
     *
     * Reads building occupancy array (+0x90 on selected_entity) and sets
     * sprite zoom: 1=occupied (show name), 3=empty (hide). Hides scroll
     * arrows. Called when hovering over a station.
     *
     * Called by: SelectBuilding (types 2, 4, 0xC)
     */
    void UpdateStationSprites();

    /**
     * UpdateVehicleSprites — refresh vehicle sprite row labels.
     * Address: 0x45A480
     *
     * Reads vehicle passenger array and sets zoom for each entry:
     * 3=empty/no passenger, 1=occupied. Hides scroll arrows.
     *
     * Called by: SelectBuilding (type 6), UpdateBuilding
     */
    void UpdateVehicleSprites();
};
