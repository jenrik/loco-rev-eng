/**
 * ScriptedObject.h — Interactive scripted object class (extends Panel)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: TRANSCRIBED
 *
 * ScriptedObject is the interactive panel object placed in the game world.
 * It manages an embedded Entity sub-object (sprite), a ScriptEngine (scripting),
 * and a ScrollPanel (scrollable UI). It supports drag-based movement,
 * tool click handling, hit testing, Enter/Escape key dispatch, and
 * loading .dat script files for child objects.
 *
 * RECONCILIATION (2026-08-16): This class and world/scriptengine.h's
 * `RESDATA_ScriptedObject` were two independent, unreconciled
 * reconstructions of the SAME original class — identical vtable (0x4782A8),
 * identical size (~0x74C bytes), and an identical method-address list
 * (Ctor 0x449430, Dtor 0x4494C0, Update 0x4497A0, MoveTo 0x449DC0, HitTest
 * 0x44A0C0, HandleToolClick 0x44A250, UpdateToolState 0x44AC20,
 * GetDragOffset 0x449D80, CheckClick 0x449D00, EnterBuildMode 0x44A9D0).
 * This file is now the single canonical definition; world/scriptengine.h's
 * `RESDATA_ScriptedObject` class body has been removed (see that header's
 * own note). The real global singleton — constructed via a CRT static-init
 * thunk near 0x45C6B0, NOT a runtime `new` — is at address 0x4AA5B8
 * (`g_scripted_object`, declared at the bottom of this header). This class
 * was kept as canonical (rather than world/scriptengine.h's flat struct)
 * because it already models real `Panel` inheritance instead of duplicating
 * GameObject/Entity/Panel fields inline, matching this project's
 * anti-duplication rule (see CLAUDE.md).
 *
 * The object has a 4-state mode machine at +0x740, CONFIRMED by disassembling
 * both Update (0x4497A0) and EnterBuildMode (0x44A9D0) directly:
 *   0 = idle/hover (default, waiting for interaction)
 *   1 = in-world (build mode entry, bounds clamped)
 *   2 = exiting build mode (transitioning out)
 *   3 = placed/active (fully placed in world)
 * EnterBuildMode's exit branch (param==0) is the ONLY place in the binary
 * that ever stores 2 into +0x740 (`*(undefined2 *)((int)this + 0x740) = 2;`,
 * right after `Init(0x2400, 2, 0)`, i.e. immediately after the "exit build
 * mode" teardown). Update() never branches on `mode == 2` alone — only on
 * the combined `mode == 1 || mode == 2` condition, which is a single,
 * symmetric "watch the enter/exit animation" block: reaching the anim's
 * start_frame transitions back to state 0 (idle), reaching its end_frame
 * transitions to state 3 (placed). There is no code path anywhere that
 * treats `mode == 2` as "dragging" — dragging is driven by the wholly
 * separate `Panel::drag_active` field (+0x90), checked independently later
 * in the same function. world/scriptengine.h's "2 = dragging" label was a
 * misreading (traceable to Ghidra's own plate comment on 0x4497A0, which
 * itself mislabels state 2 the same way — corrected there too, see
 * set_decompiler_comment on that address).
 *
 * Size: 0x74C bytes (original x86 layout; not binding on non-Windows host
 * builds — see CLAUDE.md's "Host deviations" section).
 * Vtable: 0x4782A8 (VTBL_SCRIPTED_OBJECT)
 *
 * Class hierarchy:
 *   GameObject
 *     └─ Entity
 *          └─ Panel
 *               └─ ScriptedObject  ← this class
 *
 * Vtable layout (0x4782A8, 23 slots — CORRECTED 2026-08-16 by reading the
 * raw vtable bytes directly; the previous table below this line had every
 * slot from [8] onward mislabeled with the address that actually belongs
 * one slot earlier — a documentation-only bug: the per-method doc comments
 * on the class's own virtual declarations already carried the right
 * addresses, only the summary table's slot *numbers* were off by one):
 *   [0]  +0x00: scalar deleting destructor    (0x4494C0)  compiler-generated
 *   [1]  +0x04: OnUpdateChild                 (0x454890)  Panel::UpdateChild
 *   [2]  +0x08: IsDragging                    (0x449CE0)  override
 *   [3]  +0x0C: MoveTo                        (0x449DC0)  override
 *   [4]  +0x10: HitTest                       (0x44A0C0)  override
 *   [5]  +0x14: (unlabeled override, 0x454A60) — not needed by this pass
 *   [6]  +0x18: Init                          (0x454680)  Panel::Init
 *   [7]  +0x1C: StopSound                     (0x405A20)  Entity::StopSound
 *   [8]  +0x20: SetFrame                      (0x405DE0)  Entity::SetFrame
 *   [9]  +0x24: SetVisible                    (0x4061B0)  Entity::SetVisible
 *   [10] +0x28: Update                        (0x4497A0)  override
 *   [11] +0x2C: Draw                          (0x454900)  Panel::Draw (override of Entity::Draw)
 *   [12] +0x30: DrawConnected                 (0x405FD0)  Entity::DrawConnected
 *   [13] +0x34: SetName                       (0x405E20)  Entity::SetName
 *   [14] +0x38: SetAnimState                  (0x405A50)  Entity::SetAnimState
 *   [15] +0x3C: Shutdown                      (0x4495B0)  override
 *   [16] +0x40: InitState                     (0x44ADF0)  override
 *   [17] +0x44: HandleToolClick               (0x44A250)  override
 *   [18] +0x48: (unknown, shared with [19])   (0x44EF00)
 *   [19] +0x4C: (unknown, shared with [18])   (0x44EF00)
 *   [20] +0x50: UpdateToolState               (0x44AC20)  override
 *   [21] +0x54: GetDragOffset                 (0x449D80)  override
 *   [22] +0x58: CheckClick                    (0x449D00)  override
 *
 * `Dispatch` (0x449C00, RET 0x14 — 5 stack args) is NOT in this vtable at
 * all (confirmed: none of the 23 raw dwords above equal 0x449C00) — its
 * one caller (TileMap::ProcessRect, 0x456A40) reaches it via a direct
 * UNCONDITIONAL_CALL, not indirect/vtable dispatch. It is therefore
 * modeled below as an ordinary non-virtual method, not a vtable override.
 */

#pragma once

#include "../game/Panel.h"
#include "../core/Entity.h"

class TrackPiece;
class ScriptedObject : public Panel {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* Panel/Entity/GameObject fields (0x00..0xDF) — see Panel.h, Entity.h, GameObject.h */
    /* +0x08/+0x0C/+0x10/+0x14: screen_rect (GameObject; left/top/right/bottom) */
    /* +0x24: visible       (inherited from Entity) */
    /* +0x28: anim_index    (inherited from Entity) */
    /* +0x3C: source_rect.bottom (inherited from Entity; used as a world-height
     *        clamp reference in MoveTo/Update — NOT screen_rect.bottom, which
     *        is a different field at +0x14. A prior pass here mislabeled this
     *        access as `screen_rect.bottom`; corrected.) */
    /* +0x40: resource      (inherited from Entity) */
    /* +0x48: audio_channel (inherited from Entity, repurposed as path_buf) */
    /* +0x54: frame_index   (inherited from Entity; current animation frame
     *        position — distinct from anim_index at +0x28. Compared against
     *        the resource's per-state start/end frame table in Update().) */
    /* +0x88: update_child_flags (inherited from Panel) */
    /* +0x90: drag_active   (inherited from Panel) */
    /* +0x94: drag_offset_x (inherited from Panel) */
    /* +0x98: drag_offset_y (inherited from Panel) */
    /* +0xA0: tooltip_handle (inherited from Panel) */
    /* +0xAD: dim_flag      (inherited from Panel; ScriptedObject-specific use:
     *        0 = left-direction layout, 1 = right-direction layout for child
     *        sprite placement — see MoveTo/HitTest) */
    /* +0xD0: child_surface (inherited from Panel, repurposed as the head of
     *        this object's own child linked list — walked via each child's
     *        own +0x28 "next" field) */
    /* +0xDC: escape_zoom_child (inherited from Panel, renamed from
     *        `child_ptr`/`panel_state` — see game/Panel.h's own field doc;
     *        this comment's "repurposed as escape_tool" claim has no
     *        actual access site in this file's .cpp today, so it is
     *        unverified — flagged, not fixed, out of scope here) */

    /* ================================================================ */
    /* Embedded Entity sub-object (+0xE0..+0x167, 0x88 bytes)            */
    /* ================================================================ */
    /* Constructed via Entity(-1, -1, 0, 0) — see this class's own ctor.
     * Typed as a real embedded Entity (was a raw uint8_t[0x88] byte array;
     * corrected 2026-08-16 — Entity is already a fully-modeled canonical
     * class and every access below goes through its own real virtual
     * methods/fields, matching CLAUDE.md's "no expanded scalar arrays over
     * known game objects" rule). Host layout size need not match the
     * original x86 0x88 bytes (non-goal for host builds; see CLAUDE.md). */
    Entity   sub_entity;                    // +0xE0  embedded Entity

    /* ================================================================ */
    /* ScriptedObject fields after embedded Entity (+0x168..+0x177)      */
    /* ================================================================ */

    RECT     drag_rect;                     // +0x168  active drag rectangle (computed from screen_rect or sub_entity screen_rect)

    /* ================================================================ */
    /* Embedded ScriptEngine (+0x178..+0x1FF, 0x88 bytes reserved)      */
    /* ================================================================ */
    /* ScriptEngine_Init at 0x44E8D0 initializes the real ScriptEngine
     * fields (~0x3C..0x44 bytes; world/scriptengine.h's ScriptEngine class).
     * NOT embedded here as a typed `ScriptEngine` member: reading
     * ScriptEngine's own full vtable (0x478378) directly shows several
     * slots (e.g. +0x0C, +0x1C) installed with the EXACT SAME addresses as
     * Panel's own vtable (0x454820 Panel::SetPosition, 0x454890
     * Panel::UpdateChild, 0x4549E0 Panel::HitTestChildren, 0x454680
     * Panel::Init, ...) — evidence that ScriptEngine's real inheritance is
     * NOT the plain "RESDATA base" world/scriptengine.h currently models,
     * but something Panel-shaped. Re-deriving ScriptEngine's true hierarchy
     * and full field layout is a separate, substantial investigation, not
     * attempted here — see the BLOCKED note in PROGRESS.md. Kept as an
     * opaque reserved byte range with the pre-existing vtable-dispatch
     * helpers (se_shutdown/se_handle_drag/se_vmove/se_hittest/se_ptinrect/
     * se_draw/se_update below) rather than guessing at a wrong typed
     * member. Key known vtable slots on the CURRENT (incomplete)
     * ScriptEngine model: [0]=Reset(dtor-shaped), [3]=Call(shutdown). */
    uint8_t  script_engine_prefix[0x38];   // +0x178  ScriptEngine fields 0x00..0x37
    int32_t  script_engine_offset;          // +0x1B0  SE child sprite offset (overlays SE::field_38)
    uint8_t  script_engine_suffix[4];       // +0x1B4  remaining SE bytes
    uint8_t  _pad_1B8[0x48];                // +0x1B8..+0x1FF
    uint8_t  script_engine_active;          // +0x200  SE sub-object visible flag
    uint8_t  _pad_201[0x5F];                // +0x201..+0x25F

    /* ================================================================ */
    /* Embedded ScrollPanel (+0x260.., nominally 0x88 bytes reserved)    */
    /* ================================================================ */
    /* UIPANEL_InitScrollPanel at 0x427370 placement-constructs a real
     * UIPANEL (ui/UIPANEL.h) at this address. BLOCKER (2026-08-16, not
     * resolved by this pass): UIPANEL's own documented real size is
     * ~0x4E0 bytes, but this class had previously only reserved 0x88 bytes
     * here (+0x260..+0x2E7) before the "+0x2E8 scrollpanel_visible /
     * +0x630 unknown" fields that follow. 0x260 + 0x4E0 == 0x740, which is
     * EXACTLY where `mode` begins — strongly suggesting the embedded
     * UIPANEL actually occupies the entire +0x260..+0x73F range (i.e. the
     * previously-"unknown_0x630" range is simply part of UIPANEL's own
     * already-modeled fields: tab_sprites/item_sprites/sprite_list_head/
     * sprite_list_tail land in almost exactly that range once UIPANEL's
     * own offsets are added to 0x260), and that the "+0x2E8 scrollpanel_
     * visible" byte this class and world/scriptengine.h both independently
     * documented is actually UIPANEL's OWN inherited `Panel::
     * update_child_flags` (relative +0x88 within a Panel-derived object —
     * exactly the offset both files read there), not a separate
     * ScriptedObject-level flag. This is a plausible, evidenced hypothesis,
     * not a confirmed fact (would require walking UIPANEL::HandleDrag's own
     * disassembly against this base to confirm no other field boundaries
     * are violated) — flagging as BLOCKED rather than committing the
     * larger reflow. Kept as the pre-existing opaque reserved layout below;
     * do not add new casts against it beyond the pre-existing dispatch
     * helpers (sp_shutdown/sp_vmove/sp_hittest/sp_ptinrect/sp_draw/
     * sp_update). */
    uint8_t  scroll_panel_prefix[0x38];    // +0x260  UIPANEL fields 0x00..0x37
    int32_t  scroll_panel_offset;           // +0x298  SP child sprite offset (overlays UIPANEL field)
    uint8_t  scroll_panel_suffix[0x4C];     // +0x29C  remaining UIPANEL bytes
    uint8_t  scroll_panel_active;           // +0x2E8  SP sub-object visible flag (see BLOCKER above)
    uint8_t  _pad_2E9[0x347];               // +0x2E9..+0x62F
    uint8_t  unknown_0x630[0x110];          // +0x630..+0x73F — see BLOCKER above

    /* ================================================================ */
    /* Mode / trailing fields (+0x740..+0x74B)                           */
    /* ================================================================ */

    int16_t  mode;                          // +0x740  0=idle, 1=in-world, 2=exiting, 3=placed (see class doc comment)
    uint8_t  _pad_742[2];                   // +0x742..+0x743
    void*    child_sprite1;                 // +0x744  sprite created for resource type 0x2406 (see Start())
    void*    child_sprite2;                 // +0x748  sprite created for resource type 0x240C (see Start())

    /* Total size: 0x74C bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * ScriptedObject constructor.
     * Address: 0x449430
     */
    ScriptedObject();

    /**
     * Virtual destructor (vtable[0]).
     * Address: 0x4494C0 (body), scalar deleting wrapper compiler-generated.
     */
    virtual ~ScriptedObject();

    /* ================================================================ */
    /* Virtual methods — vtable overrides                                */
    /* ================================================================ */

    /** OnUpdateChild — vtable[1], address 0x454890 (Panel::UpdateChild) */
    virtual void OnUpdateChild();

    /** IsDragging — vtable[2], address 0x449CE0 */
    virtual int IsDragging(int x, int y);

    /** MoveTo — vtable[3], address 0x449DC0 */
    virtual void MoveTo(int x, int y) override;

    /** HitTest — vtable[4], address 0x44A0C0 */
    virtual int HitTest(int x, int y);

    /** Update — vtable[10], address 0x4497A0 */
    virtual void Update() override;

    /** Shutdown — vtable[15], address 0x4495B0 */
    virtual void Shutdown();

    /** InitState — vtable[16], address 0x44ADF0 */
    virtual int InitState();

    /** HandleToolClick — vtable[17], address 0x44A250 */
    virtual uint32_t HandleToolClick(TrackPiece* tool, int x, int y);

    /** UpdateToolState — vtable[20], address 0x44AC20 */
    virtual uint32_t UpdateToolState(TrackPiece* tool);

    /** GetDragOffset — vtable[21], address 0x449D80 */
    virtual int GetDragOffset(int x, int y);

    /** CheckClick — vtable[22], address 0x449D00 */
    virtual bool CheckClick(int x, int y);

    /* ================================================================ */
    /* Non-virtual methods                                               */
    /* ================================================================ */

    /** InitSubObjects — Tear down all sub-objects. Address: 0x4494E0 */
    void InitSubObjects();

    /**
     * Start — activate the scripted object: loads resources 0x2400-0x2413,
     * creates the child GameObject/sprites, creates the tooltip.
     * Address: 0x449600
     * @return non-zero on success.
     */
    uint32_t Start();

    /**
     * Dispatch — draw this object and its visible sub-objects within a clip
     * rect. NOT a vtable member (see class doc comment) — called directly
     * by TileMap::ProcessRect with the tile's screen rect and a trailing
     * flag (always passed as literal 0 to the internal draw calls
     * regardless of that flag, per the original's own disassembly).
     * Address: 0x449C00 (RET 0x14 — 5 real stack args)
     */
    void Dispatch(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t flag);

    /* HandleEvent/LoadFromStream/AddChild/RemoveChild were removed from
     * here — they were a misattribution of TrackTileDescriptor's own
     * methods (0x44B290/0x44B190/0x44B220; TrackTileDescriptor is a
     * BuildingDescriptorEditor subclass, not a ScriptedObject method
     * family). See input/TrackTileDescriptor.h and the +0x630 field
     * comment above for the full evidence trail. `LoadFromStream` in
     * particular never had a decompiled address of its own — it was
     * invented to stand in for TrackTileDescriptor's virtual self-dispatch
     * `Render()` call (vtable slot [3]), which this pass identified via
     * disassembly (0x44B290's `CALL [this+0xC]`). */

    /** EnterBuildMode — Enter/exit build mode. Address: 0x44A9D0 */
    void EnterBuildMode(uint8_t enter);
};

/* ================================================================== */
/* External global — the real, reachable singleton                    */
/* ================================================================== */
/* Constructed via a CRT static-initializer thunk near 0x45C6B0 (same
 * shape as the neighboring CRT_StaticInit_UIManager thunk), NOT via a
 * runtime `new`. Real, reachable, live-wired consumers: core/
 * HostMode3Bootstrap.cpp (host raw-alloc, constructor deliberately
 * skipped — see that file), core/GameLoop.cpp (Update() every frame),
 * core/Game.cpp (GetDragOffset/IsDragging/CheckClick/HitTest for mouse
 * hover/click/drag), world/tilemap.cpp (Dispatch(), per visible tile),
 * ui/UIPANEL.cpp, ui/EditWindow.cpp. */
#define ADDR_g_scripted_object 0x004AA5B8
extern ScriptedObject* g_scripted_object;  /* 0x4AA5B8 */
