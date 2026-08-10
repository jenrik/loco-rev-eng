/**
 * BuildingDescriptorEditor.h — Building/tile placement .dat descriptor loader
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * NAME NOTE: no original symbol/string names this class. "BuildingDescriptorEditor"
 * is a best-evidence placeholder describing its actual verified behavior — it is
 * NOT an evidenced original name. Ghidra's auto-generated names for its methods
 * (INPUT_ShowFileDialog, INPUT_EditWndProc, INPUT_CreateEditControl, etc.) were
 * verified by direct decompilation to be misleading holdovers from an incorrect
 * "text-edit-box UI widget" guess; none of that framing survived inspection.
 * The class actually loads and parses a building/tile placement descriptor
 * (a ".dat" file) with occupancy grids, entry/exit points, employee/minifig
 * capacity, visit tracking, and clickable-region metadata for the in-editor
 * placement tool.
 *
 * Evidence trail:
 *   - Constructor (0x41E570, Ghidra label "INPUT_ExitGame" — misnomer) calls
 *     ChildWindow's base constructor, re-inits the two embedded TrackPos
 *     sub-objects at +0x534/+0x548, sets the vtable at 0x4779E0, zeroes the
 *     3 KeySequenceRecord key-id array pointers, then calls
 *     handle_edit_message(resId, nameParam).
 *   - Destructor body (0x41E620, Ghidra label "INPUT_CreateEditControl") frees
 *     the 3 heap buffers at +0x564/+0x598/+0x5CC and re-inits the TrackPos
 *     objects. The scalar-deleting-destructor wrapper at 0x41E600 (Ghidra label
 *     "INPUT_DtorWrapper") becomes the compiler-generated vtable[0].
 *   - Vtable 0x4779E0 slot [1]=0x425670 (OnMouseMove) and slot [2]=0x4257F0
 *     (OnMouseLeave) are identical addresses to ChildWindow's base slots,
 *     confirming they are inherited verbatim, not overridden. Slot [0]=0x41E600
 *     (this class's destructor thunk) and slot [3]=0x41E9F0 (Render override).
 *   - resources/ResourceManager.cpp allocates exactly operator_new(0x630) before
 *     calling BuildingDescriptorEditor_Ctor, confirming this class's total size.
 *   - game/ScriptedObject.cpp's RESDATA_ScriptedObject_HandleEvent (0x44B290)
 *     is near-identical to handle_edit_message (same %s%s.dat/%s%s.bmp path
 *     build, same AssetMgr-then-disk fallback, same +0x162 loaded flag) and
 *     calls the Render function (0x41E9F0) directly (not through any vtable)
 *     against its OWN `this`. This means the .dat-directive parser and its 3
 *     helper functions are reused across at least two different host objects
 *     that happen to lay out the same descriptor fields at the same absolute
 *     offsets — evidence of a shared data-layout convention, not of inheritance.
 *   - ui/CursorEditWindow.h documents a sibling class (a ChildWindow-family
 *     .dat/.bmp loader for CURSOR data) with its own "loaded" flag at the same
 *     +0x162 offset, confirming +0x162 is a shared ChildWindow-family convention.
 *
 * Class hierarchy:
 *   ChildWindow (ui/UI_ChildWindow.h, vtable 0x477C18)
 *     └─ BuildingDescriptorEditor (this class, vtable 0x4779E0)
 *
 * Size: 0x630 bytes (confirmed: resources/ResourceManager.cpp allocates
 *       exactly operator_new(0x630) before calling the constructor bridge).
 *
 * Status: INTEGRATED
 */

#pragma once

#include "../shared/types.h"
#include "../ui/UI_ChildWindow.h"
#include "../game/TrackPos.h"

/* ================================================================== */
/* KeySequenceRecord — InsertSeq/MobileSeq/TotalVisits sub-record       */
/* Address of parser: INPUT_EditKeyHandler, 0x41F2B0                    */
/*                                                                      */
/* Size: 0x34 = 52 bytes. Three of these are embedded in                */
/* BuildingDescriptorEditor at +0x55C (InsertSeq), +0x590 (MobileSeq),  */
/* and +0x5C4 (TotalVisits). Field names beyond key_count/key_ids/      */
/* resource_id_0/resource_id_1 are unconfirmed placeholders — the       */
/* parser never reads dword [0], and several stream calls (fields at   */
/* +0x10/+0x18/+0x24, read as int16 via WNDPROC_StreamReadLine) do not  */
/* have an evidenced semantic name yet.                                 */
/* ================================================================== */
struct KeySequenceRecord {
    int32_t  _unused_00;    // +0x00  zeroed by the owning reset; never read by 0x41F2B0
    int32_t  key_count;     // +0x04  count of key_ids entries (must be 0 < n < 0x2D = 45)
    int32_t* key_ids;       // +0x08  heap array of key_count resource IDs (freed by the owner's dtor)
    int32_t  field_0C;      // +0x0C  dword written via WNDPROC_StreamWrite
    int16_t  field_10;      // +0x10  int16 read via WNDPROC_StreamReadLine (padded to a dword slot)
    int16_t  _pad_12;
    int32_t  resource_id_0; // +0x14  primary resource id; expected type 2/4/12/13, else resource_id_1 is cleared to -1
    int16_t  field_18;      // +0x18  int16 read via WNDPROC_StreamReadLine (padded to a dword slot)
    int16_t  _pad_1A;
    int32_t  field_1C;      // +0x1C  dword written via WNDPROC_StreamWrite
    int32_t  resource_id_1; // +0x20  secondary resource id; expected type 7, else cleared to -1 (also cleared if resource_id_0 fails)
    int16_t  field_24;      // +0x24  int16 read via WNDPROC_StreamReadLine (padded to a dword slot)
    int16_t  _pad_26;
    int32_t  field_28;      // +0x28  set from a byte produced by a call decompiled as CRT_fmod — identity
                             //        unresolved in this pass (TODO: verify against disassembly)
    int32_t  field_2C;      // +0x2C  dword written via WNDPROC_StreamWrite
    int32_t  field_30;      // +0x30  dword written via WNDPROC_StreamWrite
};
/* No x86-layout-parity static_assert here: `key_ids` is a native pointer,
 * 8 bytes on this 64-bit host vs. the documented x86 4 bytes, so the real
 * host sizeof() can never equal the original 0x34. Per CLAUDE.md's host-
 * deviation policy, exact x86 struct size/offsets are a documentation
 * concern, not a host-build goal — do not assert host objects into x86
 * layout parity. */

/* ================================================================== */
/* BuildingDescriptorEditor class                                      */
/* ================================================================== */

class BuildingDescriptorEditor : public ChildWindow {
public:
    /* ---- +0x168..+0x62F: this class's own descriptor fields --------- */
    uint8_t  border_width;                            // +0x168  physical-occupancy grid width  ([physical_occupancy] section)
    uint8_t  border_height;                           // +0x169  physical-occupancy grid height
    uint8_t  border_depth;                            // +0x16A  physical-occupancy grid depth
    uint8_t  bitmap_occupancy_width;                  // +0x16B  [bitmap_occupancy] grid width
    uint8_t  bitmap_occupancy_height;                 // +0x16C  [bitmap_occupancy] grid height
    uint8_t  border_scale_byte;                        // +0x16D  computed: (border_height*15 + bitmap_occupancy_height) * 16

    /* Physical-occupancy 3D grid, dims border_width x border_height x
     * border_depth. draw_border_grid() (0x41EFA0) writes cells with an
     * irregular stride (inner +0x3F, middle +7, outer +1) with NO bounds
     * checking against this fixed-size buffer — an original-game
     * buffer-overflow-prone pattern, preserved faithfully rather than
     * silently hardened (matches this project's documented policy for
     * other original bugs, e.g. WIN32_PeekMessageLoop). Shipped .dat
     * files apparently never produce large enough dimensions to overflow
     * it in practice. */
    uint8_t  physical_occupancy_grid[0x333];          // +0x16E

    /* Bitmap-occupancy grid, dims bitmap_occupancy_width x
     * bitmap_occupancy_height, cell stride 9 bytes, same no-bounds-check
     * caveat as physical_occupancy_grid above. */
    uint8_t  bitmap_occupancy_grid[0x75];             // +0x4A1

    uint8_t  max_employees;                           // +0x516  [MaxEmployees], clamped to <= 5
    uint8_t  _pad_517;                                 // +0x517
    int16_t  possible_employees[5];                   // +0x518  [PossibleEmployees], 5 resource ids (type 7 expected; else -1)
    uint8_t  ee_replay_delay;                          // +0x522  [EEReplayDelay] terminator value, clamped to <= 5
    uint8_t  _pad_523;                                  // +0x523
    int16_t  possible_minifigs[5];                     // +0x524  [PossibleMinifigs], 5 resource ids (type 7 expected; else -1)
    int16_t  rmb_seq;                                   // +0x52E  [RMBSeq]
    int16_t  closed_fs;                                 // +0x530  [ClosedFS], sentinel -1
    int16_t  field_532;                                 // +0x532  paired with closed_fs in the reset; no further evidence

    TrackPos track_pos_a;                               // +0x534  re-inited by ctor/dtor via TrackPos_Init/TrackPos_BaseInit
    TrackPos track_pos_b;                               // +0x548  re-inited by ctor/dtor via TrackPos_Init/TrackPos_BaseInit

    KeySequenceRecord insert_seq;                       // +0x55C  [InsertSeq]
    KeySequenceRecord mobile_seq;                       // +0x590  [MobileSeq]
    KeySequenceRecord total_visits;                     // +0x5C4  [TotalVisits]

    int32_t  ee_replay_delay_data;                      // +0x5F8  default per-line data sink while inside the [EEReplayDelay] section

    /* Clickable sub-region rectangles for the edit control, computed by
     * paint_edit_regions() (0x41F0C0) from 4 margin values read from the
     * stream. Value 2 = fill-to-edge, 1-3 = proportional, >= 4 = absolute
     * pixel coordinates; see the .cpp for the exact per-field formula. */
    int32_t  edit_region[8];                            // +0x5FC

    RECT     free_to_roam_rect;                         // +0x61C  [FreeToRoam]
    uint8_t  leisure_destination;                       // +0x62C  [LeisureDestination]
    uint8_t  _pad_62D[3];                                 // +0x62D

    /* Total: 0x630 bytes (verified: ResourceManager::AddString allocates
     * exactly operator_new(0x630) before calling the constructor bridge). */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * Constructor. Address: 0x41E570 (Ghidra label "INPUT_ExitGame" — a
     * legacy misnomer; the body has nothing to do with exiting a game).
     *
     * Initializes the ChildWindow base via member-initializer-list, re-inits
     * the two embedded TrackPos sub-objects, installs the vtable, zeroes the
     * 3 KeySequenceRecord key-array pointers, then calls handle_edit_message
     * to load the .dat descriptor.
     *
     * @param resId      Resource ID for this descriptor
     * @param nameParam  Non-zero = load descriptor data from disk/asset mgr
     */
    BuildingDescriptorEditor(uint32_t resId, int32_t nameParam);

    /**
     * Virtual destructor (vtable[0]). Address: 0x41E600
     * (Ghidra label "INPUT_DtorWrapper" — scalar-deleting-destructor thunk).
     * Body at 0x41E620 (Ghidra label "INPUT_CreateEditControl" — misnomer).
     *
     * Frees the 3 KeySequenceRecord key-array heap buffers and re-inits the
     * TrackPos objects. Base class ~ChildWindow() runs automatically as part
     * of the compiler-generated destructor chain.
     */
    virtual ~BuildingDescriptorEditor();

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * handle_edit_message — Load and parse the .dat descriptor.
     * Address: 0x41E6E0 (Ghidra label "INPUT_HandleEditMessage").
     *
     * Resets all descriptor fields to their sentinel defaults, then (if
     * nameParam != 0) builds "%s%s.dat"/"%s%s.bmp" paths, tries the
     * AssetMgr archive first, falls back to WIN32_StreamOpenPath, and
     * dispatches to Render() (vtable slot [3]) for both the archive and
     * disk-fallback cases. Called by the constructor.
     *
     * @param resId     Resource ID (used to build the descriptor filename)
     * @param nameParam Non-zero to actually load; zero for no-op reset only
     */
    void handle_edit_message(uint32_t resId, int32_t nameParam);

    /**
     * Render — vtable slot [3]. Address: 0x41E9F0
     * (Ghidra label "INPUT_EditWndProc" — not a window procedure at all).
     *
     * Reads one directive line from the stream and dispatches on a
     * cascading sequence of keyword substring checks: physical_occupancy,
     * bitmap_occupancy, entry_exit, RMBSeq, ClosedFS, EEReplayDelay,
     * LeisureDestination, MaxEmployees, PossibleEmployees,
     * PossibleMinifigs, shifts, FreeToRoam, ButtonVisible, InsertSeq,
     * MobileSeq, TotalVisits. Returns true while more directives remain,
     * false once the section's terminator line is reached.
     *
     * Also called directly (not through any vtable) by
     * game/ScriptedObject.cpp's RESDATA_ScriptedObject_HandleEvent
     * (0x44B290) against its own, differently-typed `this` — the shared
     * data-layout convention described in the class-level doc comment above.
     *
     * Overrides ChildWindow::Render (vtable slot [3]).
     *
     * @param stream  WNDPROC stream object positioned at the next line
     * @return        Non-zero (1) = keep processing this section,
     *                zero (0) = section complete
     */
    virtual uint8_t Render(void* stream) override;

    /**
     * draw_border_grid — Address: 0x41EFA0 (Ghidra label
     * "INPUT_DrawEditBorder"). Reads border_width/height/depth from the
     * stream and (re)fills physical_occupancy_grid. Called from
     * Render() when the current line is not the physical_occupancy terminator.
     */
    bool draw_border_grid(void* stream);

    /**
     * paint_edit_regions — Address: 0x41F0C0 (Ghidra label
     * "INPUT_PaintEdit"). Reads 4 margin values from the stream and
     * computes edit_region[8]. Called from Render() when the current
     * line is not the entry_exit terminator.
     */
    bool paint_edit_regions(void* stream);
};

/* No x86-layout-parity static_assert here: this class embeds three
 * KeySequenceRecords (each with a native pointer field) plus TrackPos
 * sub-objects, so its real host sizeof() is inflated past the documented
 * 0x630 x86 allocation by the 4-vs-8-byte pointer-width delta, compounded.
 * The 0x630 figure remains correct documentation of the original allocation
 * size (see the class header comment) — just not a host-buildable invariant. */

#if UINTPTR_MAX == 0xffffffffu

static_assert(sizeof(BuildingDescriptorEditor) == 0x630,
    "BuildingDescriptorEditor must match the 32-bit loco.exe layout (0x630 bytes)");

#endif

/* ================================================================== */
/* Free functions                                                       */
/* ================================================================== */

/**
 * edit_key_handler_parse — Address: 0x41F2B0 (Ghidra label
 * "INPUT_EditKeyHandler"). NOT a member: takes the stream and a pointer
 * to one of the owner's KeySequenceRecord sub-objects directly (the
 * decompiled signature has no `this`/owner parameter at all — only the
 * stream and the sub-record pointer). Called from Render() for the
 * InsertSeq/MobileSeq/TotalVisits keywords, passing &insert_seq /
 * &mobile_seq / &total_visits.
 *
 * Allocates key_count key_ids entries, reads the record's remaining
 * fields from the stream, then validates resource_id_0 (expected type
 * 2/4/12/13) and resource_id_1 (expected type 7), clearing resource_id_1
 * to -1 on either validation failure.
 *
 * @param stream  WNDPROC stream object
 * @param record  KeySequenceRecord* to populate (may be nullptr — returns immediately)
 */
uint32_t edit_key_handler_parse(void* stream, KeySequenceRecord* record);

/**
 * BuildingDescriptorEditor_Ctor — placement-new compatibility bridge.
 *
 * resources/ResourceManager.cpp's AddString (0x446840) drives a C-style
 * resource-type dispatch table that allocates raw memory with
 * operator_new(size) and then calls a `void* Ctor(void* mem, resId,
 * strPtr)` function for each resource-type family (see e.g.
 * CGWND_CursorEditWindow_Ctor, TrainStation_Ctor for the sibling
 * classes). This bridge follows that exact established convention
 * rather than reshaping ResourceManager.cpp's own dispatch style.
 *
 * The original Ghidra label for this exact function is "INPUT_ExitGame"
 * — renamed here since it is this class's real constructor, not
 * something related to exiting the game.
 */
void* BuildingDescriptorEditor_Ctor(void* memory, int32_t resId, int32_t strPtr);
