/**
 * TrackTileDescriptor.h — Scripted-object child descriptor with track-tile
 * classification (extends BuildingDescriptorEditor)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * NAME NOTE: no original symbol/string names this class. "TrackTileDescriptor"
 * is a best-evidence placeholder describing its actual verified behavior —
 * NOT an evidenced original name. The prior in-tree transcription attempted
 * this under the name "RESDATA_ScriptedObject_AddChild" and wired its logic
 * onto game/ScriptedObject.h/.cpp (the *parent* object) — that attribution
 * was wrong (see Evidence trail below) and has been corrected: the +0x630
 * "child_script_ptr" / +0x63A "unk_flag" / +0x63B "script_bitmap_path"
 * fields and the AddChild/RemoveChild/HandleEvent/LoadFromStream methods
 * that used to live on ScriptedObject actually describe *this* class.
 *
 * What this class is: a `BuildingDescriptorEditor` (0x630-byte ChildWindow-
 * family layout) with 0xC extra trailing bytes that classify a track/tunnel/
 * bridge/switch/crosstrack/levelcrossing/station tile-type keyword out of a
 * dedicated ".dat" section, on top of the inherited physical/bitmap-occupancy
 * parsing. It is constructed for `ResourceManager::AddString`'s odd-resId
 * "type 3" resource family (the scripted-object child family) and is what
 * `ScriptedObject`'s .dat script loader creates via `AddChild`.
 *
 * Size: 0x63C bytes on the original x86 layout (0x630 base + 0xC own fields;
 * see resources/ResourceManager.cpp's AddString, which allocates exactly
 * operator_new(0x63C) before calling the constructor bridge below).
 *
 * Vtable: 0x478358 (see below for the exact evidence on its bounds).
 *
 * Class hierarchy:
 *   ChildWindow (ui/UI_ChildWindow.h, vtable 0x477C18)
 *     └─ BuildingDescriptorEditor (input/BuildingDescriptorEditor.h, vtable 0x4779E0)
 *          └─ TrackTileDescriptor (this class, vtable 0x478358)
 *
 * Evidence trail:
 *   - AddChild bridge (0x44B190, Ghidra label "RESDATA_ScriptedObject_
 *     AddChild" — misnomer, renamed): disassembly shows a direct (non-
 *     virtual) `CALL 0x0041e570` (BuildingDescriptorEditor's real
 *     constructor) on the same `this`, with nameParam hardcoded to 0 (the
 *     base does only its own field reset, no disk load yet), THEN
 *     `MOV [this], 0x478358` (vtable install — this class's real, derived
 *     vtable, distinct from BDE's own 0x4779E0), THEN
 *     `MOV [this+0x630], 0` (zero the new heap-array field), THEN a direct
 *     `CALL 0x0044b290` (this class's own HandleEvent, the real .dat
 *     loader) — exactly the "base ctor, then derived ctor body, then a
 *     factory finishing step" shape already established for
 *     BuildingDescriptorEditor_Ctor/TrainStation_Ctor in this tree.
 *   - Destructor chain: DtorChain (0x44B200, vtable[0], scalar deleting
 *     destructor) calls RemoveChild (0x44B220) then optionally frees.
 *     RemoveChild's disassembly: resets `[this]` back to the same 0x478358
 *     vtable (the standard MSVC "reset vtable during unwind" idiom), frees
 *     `this[0x18C]` (dword index 0x18C == byte offset 0x630 ==
 *     tile_type_entries), THEN calls `BuildingDescriptorEditor__DtorBody`
 *     (0x41E620, BDE's own real destructor body) directly on the same
 *     `this` — i.e. RemoveChild literally IS this class's own destructor
 *     body, chaining to the base destructor exactly like real C++
 *     compiler-generated destructor chaining.
 *   - Vtable 0x478358 real bounds: `get_xrefs_to` on every dword-aligned
 *     offset from 0x478358 up to 0x478378 shows the NEXT address anything
 *     installs as a vtable pointer (`*this = &...`) is 0x47836C, exactly
 *     0x14 bytes (5 slots) later — the same 5-slot size as ChildWindow's
 *     own vtable (dtor/OnMouseMove/OnMouseLeave/Render/ctor-init-body) and
 *     BuildingDescriptorEditor's. 0x47836C onward belongs to an entirely
 *     different, unrelated class (installed by `RESDATA_ScriptedObject_
 *     CleanupChildren` and `Vehicle_Ctor`, operating on a receiver with
 *     small offsets — +0x10..+0x20 child array, +0x7A, +0x88 — matching
 *     neither this class nor ScriptedObject's own documented 0x4782A8
 *     vtable). That third class/relationship is out of scope for this
 *     class and is NOT modeled here.
 *   - Vtable 0x478358 slots, confirmed via `read_bytes` + decompiling every
 *     slot target:
 *       [0] +0x00: RESDATA_ScriptedObject_DtorChain (0x44B200) — scalar
 *                  deleting destructor
 *       [1] +0x04: 0x00425670 — byte-identical to ChildWindow::OnMouseMove;
 *                  inherited unchanged, not overridden
 *       [2] +0x08: 0x004257F0 — byte-identical to ChildWindow::OnMouseLeave;
 *                  inherited unchanged, not overridden
 *       [3] +0x0C: RESDATA_ScriptedObject_ClassifyTileType (0x44B4F0) —
 *                  Render() override (this class's own tile-type classifier)
 *       [4] +0x10: RESDATA_ScriptedObject_HandleEvent (0x44B290) — occupies
 *                  the same slot position as BuildingDescriptorEditor's own
 *                  handle_edit_message (0x41E6E0) at ChildWindow's "ctor
 *                  init body" slot; per ui/UI_ChildWindow.h's own
 *                  established precedent this is MSVC construction-time
 *                  indirect-dispatch plumbing, not a real declared C++
 *                  virtual — modeled as a plain (non-virtual) method here,
 *                  matching BuildingDescriptorEditor::handle_edit_message's
 *                  precedent exactly.
 *   - HandleEvent's (0x44B290) own disassembly resolves the "unknown self-
 *     dispatch" call chain precisely: for both the archive-load and disk-
 *     fallback branches it calls, in order: `CALL 0x0041e9f0`
 *     (BuildingDescriptorEditor::Render, qualified/direct — the physical_
 *     occupancy/bitmap_occupancy/etc. directive cascade), then, if that
 *     succeeded, `CALL 0x00424e00` (ChildWindow::Render, qualified/direct —
 *     the button/Name/hotspot/etc. base directive cascade), then, if THAT
 *     succeeded, `CALL [this+0xC]` (this class's OWN Render — virtual self-
 *     dispatch, resolving to ClassifyTileType since the vtable was already
 *     patched to 0x478358 by AddChild before HandleEvent runs). Each level
 *     parses a distinct portion of the same .dat stream; this is NOT one
 *     override delegating to the next via an internal call, but three
 *     explicit, sequential, gated parse phases invoked directly by
 *     HandleEvent.
 *   - HandleEvent's string-building call shape (`CRT_sprintf_buf(buf,
 *     "%s%s.dat", g_scene_name, name_suffix)` at 0x47e368, `"%s%s.bmp"` at
 *     0x47e35c writing to `this+0x48` == inherited ChildWindow::bmpPath —
 *     NOT +0x63B as the prior mis-attribution assumed — and `"%s.dat"` at
 *     0x47e354 for the archive path) exactly matches game/ScriptedObject.
 *     cpp's ALREADY-reverse-engineered (but wrongly-attributed) logic; that
 *     logic is moved here verbatim rather than re-derived, since it was
 *     already correct in substance, just attached to the wrong class.
 *   - ClassifyTileType (0x44B4F0) confirms the extra 0xC bytes: frees/
 *     nulls a heap array at +0x630 on entry (idempotent re-entry guard,
 *     since HandleEvent may invoke Render twice — once per archive/disk
 *     branch — if the first attempt fails outright), reads two counts via
 *     WNDPROC_Stream::ExtractInt, writes count1-1 to +0x636 and either 0 or
 *     count2-1+count1 to +0x638 UNCONDITIONALLY, conditionally allocates
 *     and fills a (count1+count2)-entry int16-pair array at +0x630, then —
 *     only if the following terminator line reads exactly "-9" (matching
 *     ui/UI_ChildWindow.cpp's s_terminator convention) — enters a directive-
 *     keyword classification loop writing one of 20 distinct codes (0x1-
 *     0x13) to +0x63A based on tunnel/depot/bridge/points/switch/
 *     crosstrack/levelcrossing/station keyword matches (see the TileType
 *     enum below for the exact keyword→code map, confirmed via
 *     `read_bytes` on every one of the 20 keyword-string globals in
 *     0x47ef68-0x47f028).
 *
 * BUG (preserved): the original never explicitly initializes +0x636/+0x638
 * at construction (AddChild only zeroes +0x630); if HandleEvent's Render
 * cascade never reaches ClassifyTileType (either earlier stage fails), the
 * original leaves these two fields as raw operator_new heap garbage. This
 * reconstruction zero-initializes them in the constructor for host
 * determinism — a deliberate, documented deviation from the original's
 * uninitialized-read bug, not a silent behavior change.
 */

#pragma once

#include "BuildingDescriptorEditor.h"

class TrackTileDescriptor : public BuildingDescriptorEditor {
public:
    /**
     * Track-tile classification code, written to `tile_type` (+0x63A) by
     * Render()/ClassifyTileType (0x44B4F0). Values confirmed against the 20
     * keyword-string globals at 0x47ef68-0x47f028 (tunnel/depot/bridge/
     * points/switch/crosstrack/levelcrossing/station family).
     */
    enum TileType : uint8_t {
        kTileType_None               = 0x00,  // default; classification section absent or malformed
        kTileType_TunnelLeft         = 0x01,
        kTileType_TunnelRight        = 0x02,
        kTileType_TunnelTop          = 0x03,
        kTileType_TunnelBottom       = 0x04,
        kTileType_BridgeHorizontal   = 0x05,
        kTileType_BridgeVertical     = 0x06,
        kTileType_DepotLeft          = 0x07,
        kTileType_DepotRight         = 0x08,
        kTileType_DepotTop           = 0x09,
        kTileType_DepotBottom        = 0x0A,
        kTileType_Points             = 0x0B,
        kTileType_Switch             = 0x0C,
        kTileType_Crosstrack         = 0x0D,
        kTileType_LevelCrossingPathH = 0x0E,
        kTileType_LevelCrossingPathV = 0x0F,
        kTileType_LevelCrossingRoadH = 0x10,
        kTileType_LevelCrossingRoadV = 0x11,
        kTileType_StationHorizontal  = 0x12,
        kTileType_StationVertical    = 0x13,
    };

    /* ================================================================ */
    /* Fields (+0x630..+0x63B: this class's own 0xC bytes beyond BDE's    */
    /* 0x630-byte base layout)                                            */
    /* ================================================================ */

    /** +0x630: heap array of (count1+count2) int16 pairs (4 bytes/entry),
     *  read line-by-line from the stream by ClassifyTileType. Freed and
     *  re-nulled at the top of every ClassifyTileType call (idempotent),
     *  and freed once more in the destructor. Null until the tile-type
     *  section is actually parsed. */
    int16_t* tile_type_entries;

    /** +0x634: 2 bytes between tile_type_entries and count1_minus1 that
     *  ClassifyTileType never reads or writes — no evidence of a semantic
     *  purpose in this pass. */
    uint8_t  unknown_0x634[2];

    /** +0x636: (first ExtractInt count) - 1. Written unconditionally on
     *  every ClassifyTileType call. */
    int16_t  count1_minus1;

    /** +0x638: 0 if the second ExtractInt count is 0, else
     *  (second count - 1 + first count). Written unconditionally on every
     *  ClassifyTileType call. */
    int16_t  count2_field;

    /** +0x63A: TileType classification code; see enum above. */
    uint8_t  tile_type;

    /** +0x63B: alignment padding; no evidence of use. */
    uint8_t  _pad_63B;

    /* Total: 0x63C bytes on the original x86 layout (verified:
     * ResourceManager::AddString allocates exactly operator_new(0x63C)
     * before calling the constructor bridge below). */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * Constructor. Not itself decompiled at a distinct address — this is
     * the derived-class portion of AddChild's (0x44B190) inlined
     * construction sequence: calls BuildingDescriptorEditor(resId, 0)
     * (nameParam hardcoded to 0 — no disk load in the base), then zeroes
     * this class's own fields. The real .dat load happens afterward via
     * an explicit HandleEvent() call (see the bridge function below), not
     * from within this constructor — matching the disassembly, where
     * HandleEvent is called by AddChild, not by the constructor call it
     * makes.
     *
     * @param resId  Resource ID for this descriptor
     */
    explicit TrackTileDescriptor(uint32_t resId);

    /**
     * Virtual destructor (vtable[0] via DtorChain, 0x44B200). Body:
     * RemoveChild, 0x44B220. Frees tile_type_entries. Base class
     * ~BuildingDescriptorEditor() runs automatically as part of the
     * compiler-generated destructor chain.
     */
    virtual ~TrackTileDescriptor();

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Render — vtable slot [3]. Address: 0x44B4F0 (Ghidra label
     * "RESDATA_ScriptedObject_ClassifyTileType").
     *
     * Overrides BuildingDescriptorEditor::Render (itself an override of
     * ChildWindow::Render). Reads two entry counts, records them (and an
     * array of that many int16 pairs) at tile_type_entries/count1_minus1/
     * count2_field, then — only if the section's terminator line reads
     * "-9" — classifies a following tunnel/depot/bridge/points/switch/
     * crosstrack/levelcrossing/station keyword sequence into `tile_type`.
     *
     * Also invoked directly (not through the vtable) via HandleEvent's own
     * qualified `BuildingDescriptorEditor::Render`/`ChildWindow::Render`
     * calls for the earlier two parse phases — see the class-level doc
     * comment for the exact three-phase call sequence.
     *
     * @param stream  WNDPROC stream object positioned at the next line
     * @return        Non-zero (1) on success, zero (0) on stream/parse
     *                failure (see 0x44B4F0's body for the exact conditions)
     */
    virtual uint8_t Render(void* stream) override;

    /**
     * HandleEvent — Load and parse this child's .dat script.
     * Address: 0x44B290 (Ghidra label "RESDATA_ScriptedObject_HandleEvent"
     * — misleading, since this is not ScriptedObject's own method).
     *
     * Resets tile_type and the inherited `loaded` flag, then (if
     * name_suffix is non-null) builds "<g_scene_name><name_suffix>.dat" /
     * ".bmp" paths, tries the AssetMgr archive first (building
     * "<name_suffix>.dat" for the archive lookup), falls back to
     * WIN32_Stream::OpenPath, and for whichever stream actually opens,
     * runs the three-phase Render cascade described in the class-level
     * doc comment. Called by the AddChild bridge below, not by the
     * constructor.
     *
     * Occupies the same vtable slot position as
     * BuildingDescriptorEditor::handle_edit_message (see the class-level
     * doc comment on vtable slot [4]); modeled as non-virtual to match
     * that established precedent.
     *
     * @param resId       Resource ID (used only for the archive/disk path
     *                    strings' shared "g_scene_name" prefix; not itself
     *                    part of the path)
     * @param name_suffix Resource-name string used to build the .dat/.bmp
     *                    paths; null is a no-op (matches the original's
     *                    `if (name_suffix == 0) return` early-out)
     */
    void HandleEvent(uint32_t resId, const char* name_suffix);
};

/* No x86-layout-parity static_assert here: tile_type_entries is a native
 * pointer, 8 bytes on this 64-bit host vs. the documented x86 4 bytes, so
 * the real host sizeof() can never equal the original 0x63C. Per CLAUDE.md's
 * host-deviation policy, exact x86 struct size/offsets are a documentation
 * concern, not a host-build goal. */

/* ================================================================== */
/* Free functions                                                       */
/* ================================================================== */

/**
 * TrackTileDescriptor_Ctor — placement-new + load compatibility bridge.
 * Address: 0x44B190 (Ghidra label "RESDATA_ScriptedObject_AddChild" —
 * misnomer; this constructs/loads a CHILD descriptor object, it is not a
 * ScriptedObject instance method).
 *
 * resources/ResourceManager.cpp's AddString (0x446840) drives a C-style
 * resource-type dispatch table that allocates raw memory with
 * operator_new(size) and then calls a `void* Ctor(void* mem, resId,
 * strPtr)` function for each resource-type family — this bridge follows
 * that exact established convention (see e.g. BuildingDescriptorEditor_Ctor,
 * TrainStation_Ctor for the sibling classes) rather than reshaping
 * ResourceManager.cpp's own dispatch style.
 *
 * Places the new object, constructs it (BuildingDescriptorEditor base with
 * nameParam=0, then this class's own field reset), then explicitly calls
 * HandleEvent(resId, name_suffix) to perform the real .dat load — matching
 * the disassembly's post-construction direct call, not a constructor-time
 * call.
 */
void* TrackTileDescriptor_Ctor(void* memory, int32_t resId, int32_t strPtr);
