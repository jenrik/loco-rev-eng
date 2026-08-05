/**
 * DDRAW.h — DirectDraw rendering and sprite management for Lego Loco
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file covers four groups:
 *
 *   A) DDRAW_Building class  (vtable 0x478548, size ~0x5B0):
 *      Building/station/vehicle sprite manager. Manages selection popups,
 *      name labels, colored state dots, station lists, track arrows, and
 *      day/night animation. Associated with global g_ddraw_building at
 *      0x4A9EF0. Extends RESDATA (vtable 0x478274).
 *
 *   B) SpriteData struct  (size 0x10):
 *      Lightweight sprite resource descriptor. Holds a 4-ary tree pointer
 *      plus an allocated pixel buffer. Resource ID stored at +0x0C.
 *
 *   C) FileData struct  (size 0x10):
 *      Loaded game resource file descriptor. Holds file stream handle,
 *      list of decompressed sub-blocks, total size, and alloc'd filename.
 *
 *   D) C free functions:
 *      Surface management (create, unlock, release, restore),
 *      audio initialization (DirectSound),
 *      error string lookup tables,
 *      clipper management,
 *      file/resource loading helpers.
 *
 * Class hierarchy:
 *   RESDATA (vtable 0x478274)
 *     └─ DDRAW_Building  ← this class (vtable 0x478548)
 *
 * Vtable layout (0x478548):
 *   [0]  +0x00: scalar deleting destructor (DDRAW_SpriteScalarDtor, 0x458AD0)
 *   [1]  +0x04: Refresh/Hide/Show            (DDRAW_SelectBuilding dispatches)
 *   [2]  +0x08: HitTest dispatch              (used by tilemap/town)
 *   [3]  +0x0C: SetPosition                   (move sprite to x,y)
 *   [4]  +0x10: (unknown)
 *   [5]  +0x14: (unknown)
 *   [6]  +0x18: LoadChildResource             (resId, unknown, flags)
 *   [7]  +0x1C: HandleOneArgAction            (single int arg)
 *   [8]  +0x20: Refresh                       (re-render sprite)
 *   [9]  +0x24: SetName / SetResource         (text or resource)
 *   [10] +0x28: AnimUpdate                    (per-frame animation tick)
 *   [11] +0x2C: DispatchRender/DrawConnected  (render/clip subtree)
 *   [12] +0x30: DispatchRenderConnected       (render connected tiles)
 *   [13]+: More RESDATA-inherited slots
 */

#pragma once

#include "../shared/types.h"
#include "../core/GameObject.h"


// Status: TRANSCRIBED
/* ================================================================== */
/* Forward declarations                                               */
/* ================================================================== */

struct GameAudio;
struct UIPANEL_Surface;
class  Building;

/* ================================================================== */
/* SpriteData — lightweight sprite resource descriptor                */
/* Size: 0x10 bytes                                                   */
/* Constructed by DDRAW_SpriteDataCtor (0x45CDF0)                     */
/* Destroyed by DDRAW_SpriteDataDtor   (0x45CE10)                     */
/* ================================================================== */

struct SpriteData {
    void*     tree_node;         /* +0x00  AssetMgr 4-ary tree node      */
    void*     pixel_buffer;      /* +0x04  allocated pixel data buffer   */
    uint32_t  file_size;         /* +0x08  total file size in bytes      */
    uint16_t  resource_id;       /* +0x0C  numeric resource identifier   */
    /* total: 0x0E bytes + alignment padding to 0x10 */
};

/* ================================================================== */
/* FileData — loaded game resource file descriptor                    */
/* Size: 0x10 bytes                                                   */
/* Destroyed by DDRAW_FileData_Dtor (0x45CA20)                        */
/* ================================================================== */

struct FileData {
    void*     file_handle;       /* +0x00  opened file stream handle     */
    void*     block_list;        /* +0x04  linked list of decompressed   */
                                 /*        sub-blocks (+0x00=name, +0x04= */
                                 /*        size, +0x08=offset, +0x0C=next)*/
    int32_t   total_size;        /* +0x08  total decompressed size       */
    char*     file_name;         /* +0x0C  allocated file path string    */
};

/* ================================================================== */
/* DDRAW_Building — Building/station/vehicle sprite manager            */
/* Extends RESDATA (vtable 0x478274). Vtable: 0x478548.               */
/* Size: ~0x5B0 bytes.                                                 */
/* Global singleton at 0x4A9EF0 (g_ddraw_building).                    */
/* ================================================================== */

class DDRAW_Building {
public:
    /* ================================================================ */
    /* Fields                                                           */
    /* ================================================================ */

    /* vtable at +0x00 is compiler-managed via virtual methods */
    int32_t     type;                   // +0x04  = 0xD (13) for DDRAW_Building

    // +0x08 to +0xDF: RESDATA base fields (sprite tree, resource data,
    // position, scroll offsets, etc.)
    // +0x08: x position (dword, used by ShowTooltip owner-relative anchor)
    // +0x0C: y position (dword)

    void*       sub_object_1;           // +0xE0  Embedded GameObject for child
                                        //        entity hit-test / tooltip dispatch
                                        //        Read first dword = vtable pointer

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

    // +0x3E4 to +0x427: more fields

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
     *   1. self via RESDATA_DispatchEvent (draw + dim child rects)
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

/* ================================================================== */
/* Global singleton                                                    */
/* ================================================================== */

/**
 * g_ddraw_building — global DDRAW_Building singleton.
 * Address: 0x4A9EF0
 *
 * Created in CGWND_InitMode1. Used as the active building selection
 * popup manager. Session to g_active_panel.
 */
extern DDRAW_Building* g_ddraw_building;  /* 0x4A9EF0 */

/* ================================================================== */
/* Global state                                                        */
/* ================================================================== */

extern int g_viewport_x;            /* 0x4AAD24 — viewport scroll X */
extern int g_viewport_y;            /* 0x4AAD28 — viewport scroll Y */

extern void* g_tooltip_mgr;         /* 0x4FD220 — tooltip manager object */
extern void* g_world_state;         /* 0x4A98B0 — World state struct */
extern void* g_tilemap;             /* 0x4AAD08 — tilemap global */

/* ================================================================== */
/* C free functions — Surface management                               */
/* ================================================================== */

/**
 * DDRAW_GetSurface — master DirectDraw surface initialization.
 * Address: 0x45B500, __cdecl
 *
 * Creates DirectDraw object, sets cooperative level, creates primary
 * surface + backbuffer, detects 16-bit pixel format (555 vs 565),
 * sets color key, attaches clipper to HWND. Sets global pixel-format
 * vars at 0x485274-0x485290.
 *
 * Called by: CGWND_InitMode1 (init sequence)
 *
 * @return  1 on success, error code on failure
 */
uint32_t __cdecl DDRAW_GetSurface(void);

/**
 * DDRAW_UnlockPrimary — unlock and flip the primary surface.
 * Address: 0x45B940, __cdecl
 *
 * Unlocks the backbuffer, performs flip/blit to primary surface.
 * Handles surface loss (DDERR_SURFACELOST) by restoring surfaces.
 *
 * Called by: GameLoop_FrameUpdate (end of frame)
 */
void __cdecl DDRAW_UnlockPrimary(void);

/**
 * DDRAW_SetSurfaceFormat — detect and set surface pixel format.
 * Address: 0x45B9B0, __cdecl
 *
 * Calls IDirectDrawSurface::GetPixelFormat on the surface and
 * detects 5-5-5 RGB (0x7C00) vs 5-6-5 RGB (0xF800) format.
 * Sets global pixel-format variables.
 *
 * Called by: DDRAW_GetSurface
 *
 * @param surface  IDirectDrawSurface7*
 * @param fmt      DDPIXELFORMAT structure to fill
 */
void __cdecl DDRAW_SetSurfaceFormat(int* surface, int* fmt);

/**
 * DDRAW_RestoreSurfaces — restore lost DirectDraw surfaces.
 * Address: 0x45BA50, __cdecl
 *
 * Calls IDirectDrawSurface7::Restore on the primary surface and
 * re-sets pixel format. Called after display mode change or Alt+Tab.
 *
 * Called by: DDRAW_UnlockPrimary (on surface loss)
 *
 * @param surface  IDirectDrawSurface7* for primary surface
 * @param param2   (undefined)
 */
void __cdecl DDRAW_RestoreSurfaces(int* surface, uint32_t param2);

/**
 * DDRAW_ReleaseSurfaces — release all DirectDraw surfaces and object.
 * Address: 0x45BAA0, __cdecl
 *
 * Releases backbuffer, primary surface, clippers, DirectDraw object,
 * and the DDRAW DLL handle. Counterpart to DDRAW_GetSurface.
 *
 * Called by: CGWND_InitMode1 (cleanup path)
 */
void __cdecl DDRAW_ReleaseSurfaces(void);

/* ================================================================== */
/* C free functions — Audio management                                 */
/* ================================================================== */

/**
 * DDRAW_InitAudio — initialize DirectSound.
 * Address: 0x45B7E0, __cdecl
 *
 * Creates GameAudio object, initializes DirectSound with cooperative
 * level, creates primary buffer, sets format to 22050Hz 16-bit mono.
 * Configures volume bounds from LOCO.INI or defaults (0x4B/0x4E).
 *
 * Called by: CGWND_InitMode1 (init sequence)
 *
 * @return  1 on success, 0 on failure
 */
uint32_t __cdecl DDRAW_InitAudio(void);

/**
 * DDRAW_DestroyAudio — shut down DirectSound.
 * Address: 0x45BB20, __cdecl
 *
 * Saves volume levels to LOCO.INI, calls GameAudio_Cleanup, destroys
 * GameAudio object.
 *
 * Called by: CGWND_Cleanup
 */
void __cdecl DDRAW_DestroyAudio(void);

/* ================================================================== */
/* C free functions — Error strings                                    */
/* ================================================================== */

/**
 * DDRAW_GetDdrawErrorString — DirectDraw error code to string.
 * Address: 0x45BBC0, __cdecl
 *
 * Large switch over common DDERR_* and D3DERR_* error codes.
 * Returns human-readable string. Unknown codes return generic message.
 *
 * @param hresult  DirectDraw/D3D HRESULT error code
 * @return         pointer to error string literal
 */
char* __cdecl DDRAW_GetDdrawErrorString(int32_t hresult);

/**
 * DDRAW_GetDsoundErrorString — DirectSound error code to string.
 * Address: 0x45C2E0, __cdecl
 *
 * Smaller switch over DSERR_* error codes.
 *
 * @param hresult  DirectSound HRESULT error code
 * @return         pointer to error string literal
 */
char* __cdecl DDRAW_GetDsoundErrorString(int32_t hresult);

/* ================================================================== */
/* C free functions — Clipper management                               */
/* ================================================================== */

/**
 * DDRAW_Init — high-level DDRAW initialization.
 * Address: 0x45C8A0, __cdecl
 *
 * Creates thumbpal bitmap via UIPANEL_StretchBlit from resource
 * "smisc/thumbpal.bmp". Returns success/failure.
 *
 * Called by: CGWND_InitAllSubsystems (init sequence)
 *
 * @return  TRUE if thumbpal bitmap loaded successfully
 */
bool __cdecl DDRAW_Init(void);

/**
 * DDRAW_ReleaseClippers — release all DirectDraw clipper objects.
 * Address: 0x45C970, __cdecl
 *
 * Releases 7 clipper objects stored at 0x4FF0F8-0x4FF110
 * (including UIPANEL_Surface at 0x4FF110).
 *
 * Called by: DDRAW_ReleaseSurfaces
 */
void __cdecl DDRAW_ReleaseClippers(void);

/**
 * DDRAW_FreeClipper — zero-initialize a clipper struct entry.
 * Address: 0x45CA10, __fastcall
 *
 * Sets vtable and reference fields to 0. Thin clear helper.
 *
 * @param clipper  pointer to clipper struct (4 dwords)
 */
void __fastcall DDRAW_FreeClipper(void* clipper);

/* ================================================================== */
/* C functions — File/resource data helpers                            */
/* ================================================================== */

/**
 * DDRAW_FileData_Dtor — destructor for FileData struct.
 * Address: 0x45CA20, __fastcall
 *
 * Frees file buffer and decompressed block linked list.
 *
 * @param data  FileData* to destroy
 */
void __fastcall DDRAW_FileData_Dtor(FileData* data);

/**
 * DDRAW_LoadFile — load a game resource file into FileData struct.
 * Address: 0x45CAA0, __thiscall
 *
 * Opens file via WIN32_StreamOpen or AssetMgr, reads into allocated
 * buffer, handling compressed (Huffman) and uncompressed files.
 * Stores decompressed sub-blocks in a linked list. Replaces extension
 * to load accompanying data file.
 *
 * @param path  file path to load
 * @return      1 on success, 0 on failure
 */
uint32_t __thiscall DDRAW_LoadFile(FileData* self, const char* path);

/**
 * DDRAW_SpriteDataCtor — constructor for SpriteData struct.
 * Address: 0x45CDF0, __thiscall
 *
 * Zeroes fields +0x00/+0x04/+0x08, stores resource_id at +0x0C.
 *
 * @param res_id  numeric resource identifier
 */
void __thiscall DDRAW_SpriteDataCtor(SpriteData* self, uint16_t res_id);

/**
 * DDRAW_SpriteDataDtor — destructor for SpriteData struct.
 * Address: 0x45CE10, __fastcall
 *
 * Frees tree node and pixel_buffer.
 *
 * @param data  SpriteData* to destroy
 */
void __fastcall DDRAW_SpriteDataDtor(SpriteData* data);

/* ================================================================== */
/* Global state — DirectDraw globals                                   */
/* ================================================================== */

extern int*    g_ddraw;               /* 0x4A9908  IDirectDraw7*            */
extern int*    g_primary_surface;     /* 0x4FF0D8  IDirectDrawSurface7*     */
extern int*    g_backbuffer;          /* 0x4FF0DC  IDirectDrawSurface7*     */
extern int16_t g_surface_bpp;         /* 0x485274  0x22B=555, 0x235=565    */
extern int16_t g_surface_bshift;      /* 0x48527A  bit-shift mask           */

/* Game mode / difficulty globals */
extern uint8_t  g_is_town_mode;       /* 0x485328  1 = in-game town mode    */
extern uint8_t  g_ddraw_active;       /* 0x4A9F78  1 = DDraw active         */
extern uint16_t g_game_difficulty;    /* 0x4AA288  1=easy, 2=normal, 3=hard*/

extern void* g_active_panel;          /* 0x4852A0  active UI panel          */
extern void* g_tooltip_mgr;           /* tooltip manager context            */
extern void* g_town_view;             /* 0x4A99C8  town view object         */
extern int    g_demo_mode;            /* demo mode flag                     */
