// Status: INTEGRATED
/**
 * GameView.h — Town viewport scrolling / building-selection helper class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * GameView (historically "TownGameView / ScrollView") is the viewport
 * scrolling / camera / building-selection helper embedded at the
 * g_town_view global (0x4852A0). It is created once during startup.
 *
 * Class hierarchy (binary, corrected — Panel's real base is Entity, not
 * GameObject directly; see game/Panel.h):
 *   GameObject (vtable 0x477820)
 *     └─ Entity (vtable 0x477488)
 *          └─ Panel (vtable 0x4784C8)
 *               └─ GameView  <- this class (vtable 0x477D30)
 *
 * Vtable layout (0x477D30 — 22 slots, read directly from the raw vtable
 * bytes 0x477D30..0x477D88; 0x477D88 is Town's own unrelated vtable and
 * is the hard ceiling). All addresses below were confirmed by
 * disassembling their sole call sites — every one of TileMap_ProcessRect
 * (0x456700) and GameLoop_FrameUpdate (0x45C3EA)'s calls into this family
 * load ECX with the bare immediate 0x4852A0, not a pointer-variable
 * dereference, proving the receiver is this single global object, not
 * the heap-allocated Town (town/Town.h):
 *   [0]  +0x00: scalar deleting destructor (0x42CD60; body 0x42CD80)
 *   [1]  +0x04: Panel::UpdateChild          (0x454890, inherited)
 *   [2]  +0x08: GameObject::PtInRect        (0x436A10, inherited)
 *   [3]  +0x0C: center_on_point               (0x42D440 — scroll-deadzone
 *               viewport recenter; was misattributed to Town::set_mode.
 *               NOTE: this slot is GameView's own override of
 *               Entity::MoveTo's slot [3] — see method doc for why every
 *               reposition of `this` inside it uses a hardcoded, explicitly
 *               scoped Entity::MoveTo call instead of virtual dispatch)
 *   [4]  +0x10: GameView-family slot        (0x42D670, not covered here —
 *               overrides Panel::HitTestChildren (its base-class slot [4]);
 *               gates on selection_active/postcard_click_flag then calls
 *               Panel::HitTestChildren directly (non-virtually), whose own
 *               per-child loop dispatches slot [17] below. Currently still
 *               misattributed to Town::postcard_click_handler — flagged as
 *               a further follow-up, out of scope for this pass.)
 *   [5]  +0x14: Panel slot                  (0x454A60, inherited)
 *   [6]  +0x18: Panel::Init                 (0x454680, inherited)
 *   [7]  +0x1C: Entity::StopSound           (0x405A20, inherited)
 *   [8]  +0x20: Entity::SetFrame            (0x405DE0, inherited)
 *   [9]  +0x24: Entity::SetVisible          (0x4061B0, inherited)
 *   [10] +0x28: track_building                (0x42D1A0 — per-frame
 *               selection tracking; formerly misattributed to
 *               Town::track_building)
 *   [11] +0x2C: Panel::Draw                 (0x454900, override of Entity::Draw)
 *   [12] +0x30: Entity::DrawConnected       (0x405FD0, inherited)
 *   [13] +0x34: Entity::SetName             (0x405E20, inherited)
 *   [14] +0x38: Entity::SetAnimState        (0x405A50, inherited)
 *   [15] +0x3C: GameView::cleanup           (0x42CDD0)
 *   [16] +0x40: Panel::HandleKey            (0x454AE0, inherited)
 *   [17] +0x44: HitTestChild                  (0x42D6B0 — per-child hit-test/
 *               zoom callback, overriding Panel::HitTestChild; dispatched
 *               ONLY from Panel::HitTestChildren's own +0xD0 child loop
 *               (game/Panel.cpp) — confirmed via its sole xref being this
 *               DATA vtable slot, with zero direct call sites of its own.
 *               Formerly misattributed to Town::postcard_command_handler
 *               (a doubly wrong name: no postcards, no WM_COMMAND))
 *   [18] +0x48: GameView-family slot        (0x44EF00, not covered here)
 *   [19] +0x4C: GameView-family slot        (0x42D760, not covered here)
 *   [20] +0x50: update_cursor_child            (0x42D770 — per-child cursor-
 *               sprite animation tick; formerly misattributed to
 *               Town::send_postcard, whose only xref is track_building's
 *               own child loop, not any postcard call site)
 *   [21] +0x54: render_selection              (0x42D400 — formerly
 *               misattributed to Town::render_selection)
 *
 * NOTE: earlier headers claimed overrides at "SetAnimState (slot 7)",
 * "Draw 0x42F900" and "method_13 0x42D840" — none of those addresses
 * are referenced anywhere; the verified layout above replaces them.
 *
 * handle_tile_click (0x42CE10) and HitTestChild (0x42D6B0, formerly
 * "Town::postcard_command_handler") were moved here from town/Town.h in a
 * later pass: handle_tile_click's sole caller (GameLoop_PostSetupBootstrap,
 * 0x45DF32) loads ECX with the bare immediate 0x4852A0 before calling it,
 * and HitTestChild's sole xref is the DATA reference at this class's own
 * vtable slot [17] (0x477D74) — stronger evidence still, since it has no
 * direct call sites of its own at all. town/Town.h's own field model for
 * +0x88..+0x1B8 was built from these same two functions under the wrong-
 * class assumption and has been corrected here; see town/Town.h's note on
 * what remains there.
 */

#pragma once

#include "../game/Panel.h"
#include "../core/Entity.h"
#include "../game/TrackPiece.h"
#include "../resources/ResourceManager.h"   /* ResourceObject */

class Building;

/* ================================================================== */
/* GameView class                                                       */
/* ================================================================== */

class GameView : public Panel {
public:
    /* ================================================================ */
    /* GameView-specific fields                                          */
    /*                                                                   */
    /* The binary layout: Panel fields to +0xDF, then:                   */
    /*   selected_building   +0xE0                                       */
    /*   embedded Entity     +0xE4..+0x16B (constructed by 0x405790)     */
    /*   selected_building_type +0x16C                                   */
    /*   cursor_valid_sprite   +0x170  (handle_tile_click, res 0x3807)   */
    /*   cursor_invalid_sprite +0x174  (handle_tile_click, res 0x3808)   */
    /*   track_piece         +0x178                                      */
    /*   overlay_panel       +0x17C                                      */
    /*   backup_surface/x/y/width +0x180..+0x18C                        */
    /*   building_center_x/y +0x190/+0x194                              */
    /* Running size is at least 0x198 bytes (both track_building and      */
    /* deselect_building read +0x190/+0x194); this is a global instance,  */
    /* so there is no allocation size to appeal to for an exact total.   */
    /*                                                                   */
    /* IMPORTANT for reviewers: +0xEC/+0x114/+0x124 below are NOT        */
    /* separate GameView fields — they are `game_object_sub`'s own       */
    /* inherited GameObject::screen_rect / Entity::source_rect /         */
    /* Entity::resource, reached through the embedded Entity at +0xE4    */
    /* (+0xE4 + 0x08/0x30/0x40). The "panel index" field track_building   */
    /* and deselect_building read at absolute +0x28 is a DIFFERENT thing:*/
    /* it is Entity::anim_index INHERITED DIRECTLY on `this` (via        */
    /* Panel : Entity : GameObject), not any field of game_object_sub.  */
    /* Do not conflate the two +0x28-shaped-looking accesses.            */
    /* ================================================================ */

    Building* selected_building;     /* +0xE0  currently selected/tracked building */

    /* Embedded Entity sub-object — constructed with Entity(-1,-1,0,0)
     * (0x405790) in the binary; modeled as a real typed member so the
     * compiler manages its lifecycle and virtual dispatch. */
    Entity   game_object_sub;        /* +0xE4 */

    uint16_t selected_building_type; /* +0x16C  tile type of selected_building */

    /* Placement cursor indicator sprites, created by handle_tile_click
     * (0x42CE10). Also duplicated into the inherited Panel members
     * enter_zoom_child/escape_zoom_child (+0xD8/+0xDC, game/Panel.h) for
     * Panel::HandleKey's Enter/Escape keyboard shortcuts — same TrackPiece
     * objects, two independent consumers (mouse-driven HitTestChild here,
     * keyboard-driven HandleKey there). */
    TrackPiece* cursor_valid_sprite;    /* +0x170  valid-placement cursor (res 0x3807) */
    TrackPiece* cursor_invalid_sprite;  /* +0x174  invalid-placement cursor (res 0x3808) */

    TrackPiece* track_piece;         /* +0x178  cursor/zoom control for the
                                         selected building (set_building) */

    /* UIPANEL_Surface pointer by evidence (deselect_building/
     * update_selection dereference its own +0x1C field directly and pass
     * it to UIPANEL_Blit), but kept as ResourceObject* to match the
     * already-integrated cleanup() below and the same generic
     * resource-handle idiom used throughout this codebase for
     * resource-shaped objects (see resources/ResourceManager.cpp). Cast
     * at each Blit-shaped/construction use site via this file's own
     * UIPANEL_SurfaceView mirror (pre-existing, documented LOCOBITMAP.h
     * PostcardAlbum-conflict workaround — see the forward-declaration
     * comment on core/GameView.cpp's own `struct UIPANEL_Surface;`). */
    ResourceObject* overlay_panel;   /* +0x17C, released by cleanup() */

    /* Backup surface data — background restore on deselect. */
    uint32_t backup_surface;         /* +0x180 */
    uint32_t backup_x;                /* +0x184 */
    int32_t  backup_y;                /* +0x188 */
    uint32_t backup_width;            /* +0x18C */

    int32_t  building_center_x;      /* +0x190  cached selected-building center X */
    int32_t  building_center_y;      /* +0x194  cached selected-building center Y */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * GameView constructor — Address: 0x42CCE0.
     *
     * Binary sequence: RESDATA_BaseInit (Panel base init, 0x4544E0),
     * embedded Entity(-1,-1,0,0) at +0xE4, GameView vtable, type=0x0E,
     * selected_building=nullptr, +0xAD active flag=1, overlay_panel=nullptr.
     * In natural C++ the Panel base and the Entity member are constructed
     * first and the compiler emits the GameView vtable.
     */
    GameView();

    /**
     * GameView destructor — body Address: 0x42CD80.
     *
     * The binary destroys the embedded Entity then runs Panel_DtorBody
     * (0x4545A0).  In natural C++ the Entity member is destroyed after
     * the base chain, so the destructor body itself is empty and the
     * order swap is behavior-neutral (independent resources).
     */
    ~GameView() override;

    /**
     * Cleanup — Address: 0x42CDD0 (vtable[15]).
     *
     * Destroys the overlay panel (vtable[0] with flag 1), resets the
     * embedded Entity (vtable[6]) and self (Panel::Init, 0x454680),
     * then runs RESDATA_DtorBase (0x454630).
     */
    void cleanup();

    /**
     * center_on_point — vtable[3] (+0x0C). Address: 0x42D440.
     *
     * Scroll-deadzone viewport recenter. Reads the client rect
     * ({g_client_width, g_client_height, g_client_offset_x,
     * g_client_offset_y} consumed as a RECT and offset by
     * {g_viewport_x, g_viewport_y}), compares the (clamped-to-0) target
     * X against a deadzone derived from this->resource->frame_width and
     * game_object_sub.resource->frame_width, flips the +0xAD dim_flag
     * side when the target crosses the deadzone, repositions/re-renders
     * every child TrackPiece in the +0xD0 list from its own resource's
     * frame_w/world_x, notifies via StopSound(dim_flag) on both `this`
     * and game_object_sub (vtable slot 7 — a generic "state changed"
     * callback here, not audio), and finally repositions `this` and
     * game_object_sub via MoveTo.
     *
     * IMPORTANT: GameView overrides Entity::MoveTo's own vtable slot
     * ([3]) with this method, so a virtual `this->MoveTo(...)` (or the
     * non-virtual `SetWorldPos` alias, which just forwards to the
     * virtual MoveTo) called from inside this function would recurse
     * back into center_on_point. The original's disassembly confirms
     * this: both repositions of `this` are a hardcoded
     * `CALL 0x00405C00` (Entity::MoveTo) rather than a vtable dispatch —
     * reconstructed here as an explicitly-scoped `this->Entity::MoveTo(...)`
     * call, the same bypass pattern already used by render_selection for
     * Entity::Draw vs. Panel::DispatchEvent (see that method's doc). The
     * paired reposition of game_object_sub *is* genuine vtable dispatch in
     * the original (through the embedded Entity's own vtable pointer),
     * and unambiguously resolves to Entity::MoveTo since game_object_sub's
     * dynamic type is exactly Entity — written as an ordinary
     * `game_object_sub.MoveTo(...)` call.
     *
     * select_building and track_building dispatch to it through this
     * slot (previously miscoded as a 4-arg call to the unrelated
     * Town::set_mode/UI_WindowBase::set_mode).
     */
    virtual void center_on_point(int center_x, int center_y);

    /**
     * track_building — vtable[10] (+0x28). Address: 0x42D1A0.
     *
     * Per-frame tracking of the selected building. Auto-deselects an
     * invisible depot (type 6 with selected_building->visible == 0),
     * re-centers the viewport via center_on_point when the building's
     * screen-space center moved, updates each child TrackPiece in the
     * +0xD0 (Panel::child_surface) list via update_cursor_child, and
     * invalidates the embedded Entity.
     *
     * Called by: GameLoop_FrameUpdate (0x45C3EA), ECX = bare immediate
     * 0x4852A0.
     */
    void track_building();

    /**
     * is_valid_placement — Static buildable-tile check (__cdecl, not a
     * method — no receiver). Address: 0x42CF90.
     *
     * Validates entity initialized (+0x18) and tile type byte at
     * resource+8: 0x07 always valid; 0x08/0x02/0x06 must be visible;
     * 0x04 must be connected (+0x62C); 0x03 must be a building tile;
     * 0x0C valid when resource id > 0x300F.
     *
     * Called only by select_building (0x42D06A) — moved alongside it.
     */
    static bool is_valid_placement(Building* entity);

    /**
     * select_building — vtable slot NOT used here (dispatched only via
     * direct calls in the binary). Address: 0x42D040.
     *
     * Select/focus a building (or nullptr to deselect). Validates via
     * is_valid_placement (g_game_mode == 3, demo mode off), stores the
     * building, recenters the viewport via center_on_point, sets the
     * cursor zoom (1 for depot/type 6, else 3), renders the track piece
     * cursor, invalidates the tile rect, and notifies
     * DDRAW_Building::SelectBuilding. The nullptr path clears the
     * selection and hides the embedded Entity.
     *
     * Called by: track_building (0x42D1D1, ECX unchanged from `this`),
     * TileMap_HandleClick (0x456051/0x456072, ECX = bare immediate
     * 0x4852A0), and externally via the Town_SelectBuilding free-function
     * wrapper (town/Town.cpp).
     *
     * @return  selection_active (Panel::update_child_flags, +0x88 —
     *          reused by this class as the selection-active gate)
     */
    uint8_t select_building(Building* building);

    /**
     * deselect_building — Address: 0x42D280 (RET 0x10 — 4 stack args; the
     * body reads only `this`-relative fields, never the incoming stack
     * values).
     *
     * Removes the building-selection overlay: computes a clip rect from
     * game_object_sub.screen_rect and game_object_sub.source_rect
     * dimensions, intersects it with the viewport rect, blits the cached
     * background back to the primary surface via overlay_panel's own
     * Blt slot, then re-blits the panel overlay via UIPANEL_Blit.
     */
    void deselect_building(int32_t unused1, int32_t unused2,
                           int32_t unused3, int32_t unused4);

    /**
     * update_selection — Address: 0x42D3A0 (RET 0x10 — 4 stack args, same
     * unused-trailing pattern as deselect_building above).
     *
     * Blits the selection overlay panel to the primary surface: source
     * from game_object_sub.screen_rect, dest from game_object_sub.source_rect,
     * flag 0x40.
     */
    void update_selection(int32_t unused1, int32_t unused2,
                          int32_t unused3, int32_t unused4);

    /**
     * render_selection — vtable[21] (+0x54). Address: 0x42D400.
     *
     * If selection_active (Panel::update_child_flags, +0x88), draws the
     * embedded Entity's base-class Entity::Draw (0x405E60, called
     * directly on `this`, not game_object_sub — ECX is never reloaded
     * between the +0x88 check and the CALL) with the caller's tile rect.
     */
    void render_selection(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                          int32_t extra);

    /**
     * update_cursor_child — vtable[20] (+0x50). Address: 0x42D770.
     *
     * Per-child cursor-sprite animation tick, dispatched ONLY from
     * track_building's own child loop (its sole xref in the entire
     * binary is the vtable[20] DATA slot at 0x477D80 — it has zero call
     * sites of its own). Ghidra's prior label ("Town_SendPostcard") was
     * a misnomer inherited from Town's unrelated postcard subsystem;
     * the body only recognizes the three handle_tile_click cursor
     * resource IDs (0x3806/0x3807/0x3808), never anything postcard-shaped.
     * Counts down child->prev_frame and fires zoom/deselect transitions.
     */
    uint8_t update_cursor_child(TrackPiece* child);

    /**
     * handle_tile_click — Create placement cursor indicator sprites.
     * Address: 0x42CE10 (MISNAMED: not a click handler).
     *
     * Creates 3 TrackPiece child sprites via the inherited
     * Panel::CreateChildSprite (0x4546D0): valid=res 0x3807
     * (-> cursor_valid_sprite +0x170, duplicated into enter_zoom_child
     * +0xD8), invalid=res 0x3808 (-> cursor_invalid_sprite +0x174,
     * duplicated into escape_zoom_child +0xDC), hover=res 0x3806
     * (-> track_piece +0x178, no duplicate). Loads animation resource
     * 0x3805 on `this` via the inherited Panel::Init (vtable[6]) and 0x3804
     * on the embedded game_object_sub via Entity::InitBase (its own
     * vtable[6] — confirmed embedded-object idiom: the original disassembly
     * loads ECX with `LEA ECX,[this+0xE4]`, the sub-object's own address,
     * and EDX with `MOV EDX,[this+0xE4]`, its vtable pointer — a normal
     * virtual call on the embedded Entity, not a pointer-field dereference).
     * Creates the overlay UIPANEL_Surface at overlay_panel (+0x17C) sized
     * from game_object_sub.resource's own UIPANEL_Surface (+0x124, i.e.
     * game_object_sub's Entity::resource) and initializes the backup rect
     * (+0x180) from it. Returns 1 on success, 0 if any resource/animation
     * load fails.
     *
     * Called by: GameLoop_PostSetupBootstrap (0x45DF32), ECX = bare
     * immediate 0x4852A0 — its sole xref, and currently unimplemented in
     * this tree (see core/GameView.cpp's own note on why this makes the
     * function's real-world callers zero today).
     */
    char handle_tile_click();

    /**
     * HitTestChild — vtable[17] (+0x44). Address: 0x42D6B0. Overrides
     * Panel::HitTestChild.
     *
     * Per-child hit-test/zoom callback dispatched from
     * Panel::HitTestChildren's own +0xD0 child loop (game/Panel.cpp),
     * itself reached (non-virtually) from this class's own vtable[4]
     * override (0x42D670, not yet reconstructed). Tests `child->PtInRect
     * (x, y)`; if hit, dispatches on child->resource->resource_id: 0x3806
     * (hover) and 0x3808 (invalid) share the same "zoom 1 -> SetZoom(2),
     * prev_frame=6" transition; 0x3807 (valid) instead toggles the DDraw
     * building selection (DDRAW_Building::SelectBuilding) based on
     * child->zoom_level, bypassing the zoom transition entirely. Returns 0
     * if `control` is null, not render_enabled (+0x56), or not hit; 1
     * otherwise. Formerly misattributed to Town::postcard_command_handler
     * — wrong on both halves of that name (no postcards, no WM_COMMAND;
     * wParam/lParam are screen x/y, not message params).
     */
    uint8_t HitTestChild(TrackPiece* control, int x, int y) override;
};

/* ================================================================== */
/* Global instance                                                     */
/*                                                                     */
/* The GameView instance lives at the fixed address g_town_view names  */
/* (binary 0x4852A0); the canonical declaration lives in               */
/* world/tilemap.h (extern void* g_town_view) and the host definition  */
/* in shared/stubs_impl.cpp. Callers pass the pointer VALUE, cast to   */
/* GameView*, e.g. town/Town.cpp's Town_RenderSelection/               */
/* Town_DeselectBuilding/Town_UpdateSelection wrappers:                */
/*   static_cast<GameView*>(g_town_view)->render_selection(...)        */
/* — not its address (&g_town_view) — matching the disassembly's bare  */
/* `mov ecx, 0x4852A0` receiver load.                                  */
/* ================================================================== */
