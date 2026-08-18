/**
 * CursorEditWindow.h — Cursor editing/tool window (derived from ChildWindow)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * CursorEditWindow is a child window that loads cursor data (a .dat file
 * containing cursor metrics + a .bmp file containing cursor pixel data)
 * from the game installation directory or AssetMgr archive. It is created
 * as a resource-manager child window (via ResourceManager::AddString) and
 * renders the loaded cursor via Render().
 *
 * The cursor filename is derived from the resource string parameter.
 * For example, resource ID 0x1863 might correspond to "ArrowCursor",
 * yielding "ArrowCursor.dat" and "ArrowCursor.bmp".
 *
 * Inheritance:
 *   ChildWindow (0x168 bytes, vtable 0x477C18)
 *     +-- CursorEditWindow (+0x644 bytes subclass data)
 *
 * Size: 0x7AC bytes
 * Vtable: 0x477610 (6 slots)
 *
 * Vtable layout (inherits ChildWindow, overrides slot [3]):
 *   [0] +0x00: scalar deleting destructor (compiler-generated @ 0x40E660)
 *   [1] +0x04: OnMouseMove (inherited from ChildWindow @ 0x425670)
 *   [2] +0x08: OnMouseLeave (inherited from ChildWindow @ 0x4257F0)
 *   [3] +0x0C: Render — CursorEditWindow override (@ 0x40E8D0)
 *   [4] +0x10: (reserved)
 *   [5] +0x14: NULL
 *
 * Called by: ResourceManager::AddString @ 0x446840 (alloc 0x7AC, ctor)
 */

#pragma once

#include "UI_ChildWindow.h"

// Status: INTEGRATED

class CursorEditWindow : public ChildWindow {
public:
    /* ================================================================ */
    /* Fields (derived-specific; base fields are inherited)             */
    /* ================================================================ */

    /* +0x00..+0x167: inherited from ChildWindow (vtable, resourceId,
       resourceType, streamData, renderSurface, etc.) — see UI_ChildWindow.h */

    /* +0x168..+0x7A7: reserved/padding for derived-specific state */
    uint8_t    _reserved[0x7A8 - sizeof(ChildWindow)];  // +0x168

    /* +0x7A8..+0x7AB: derived-specific fields */
    int16_t    field_7A8;                 // +0x7A8  (short, init 0 by init())
    int16_t    field_7AA;                 // +0x7AA  (short, init 0 by init())

    /* Total: 0x7AC bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * CursorEditWindow constructor.
     * Address: 0x40E600
     *
     * Chains to ChildWindow constructor (with nameParam=0 to defer loading),
     * then calls init() to load cursor data.
     * Allocation size: 0x7AC bytes.
     *
     * Called by: ResourceManager::AddString @ 0x446840
     *
     * @param resourceId  Resource ID (unique ID for this cursor)
     * @param name        Cursor name (e.g., "ArrowCursor"); non-null to load
     *                    cursor data from disk/asset mgr. Widened from the
     *                    original's int32_t ABI slot to a real `const char*`
     *                    (see ui/UI_ChildWindow.h's ChildWindow constructor
     *                    doc for why the int32-pointer-handle round trip is
     *                    unsafe on this host).
     */
    CursorEditWindow(uint32_t resourceId, const char* name);

    /**
     * Virtual destructor (vtable[0]).
     * Address: 0x40E660 (scalar-deleting-destructor thunk)
     * Base dtor at: 0x40E680
     *
     * Compiler-managed; no user-defined cleanup needed beyond base dtor.
     */
    virtual ~CursorEditWindow();

    /* ================================================================ */
    /* Virtual Methods (overrides)                                      */
    /* ================================================================ */

    /**
     * Render — Load cursor metrics from a .dat stream.
     * Address: 0x40E8D0
     * Vtable slot: [3] +0x0C
     *
     * Reads cursor dimensions (width, height) and hotspot coordinates (X, Y)
     * from the .dat stream into field_7A8 and field_7AA. The stream data
     * is first parsed for validity (checking stream error flags), then
     * cursor-specific values are extracted via helper functions. A final
     * validation/palette data load is performed via CGWND_ValidatePaletteData.
     *
     * @param stream  Open .dat stream pointer
     * @return        1 if load succeeds and no errors occurred; 0 on failure
     */
    virtual uint8_t Render(void* stream) override;

    /* ================================================================ */
    /* Non-Virtual Methods                                              */
    /* ================================================================ */

    /**
     * init — Initialize and load cursor data.
     * Address: 0x40E690
     *
     * Builds filenames from the resource name parameter:
     *   sprintf(bmpPath, "%s\\%s.bmp", install_path, nameParam)
     *   sprintf(datPath, "%s\\%s.dat", install_path, nameParam)
     *
     * Attempts to load the .dat file via AssetMgr first, then falls
     * back to direct file open. Validates the data via Render() (virtual
     * dispatch), then calls ChildWindow::Render() directly, combining
     * results. Stores success/failure in loaded flag (+0x162).
     *
     * Called by: Constructor
     *
     * @param resourceId  Resource ID
     * @param name        Cursor name; non-null to load
     */
    void init(uint32_t resourceId, const char* name);

    /* NOTE: 0x40E8B0 ("cleanup") was previously declared here as a public
     * method ("Destroys the stream at +0x0C... Called externally..."),
     * but investigated and removed this session: `get_xrefs_to(0x40E8B0)`
     * shows its ONLY 9 callers are all `Unwind@...` SEH funclets (part of
     * `__except_handler4`-style scope tables, identifiable via their
     * `0x19930520` magic cookie at 0x47A76C+), never a real call site
     * anywhere in the binary or in this codebase. Each funclet computes
     * its `this`-equivalent argument as `EBP + <offset>` — a raw local
     * stack address in whatever function each funclet belongs to (three
     * distinct offsets across the 9 funclets: -0x278 ×7, -0xC8 ×1,
     * -0x170 ×1) — NOT a `CursorEditWindow*`. This is a shared,
     * compiler/linker-folded exception-safety helper
     * ("release the stream object embedded at [pointer]+0xC") reused by
     * several unrelated functions' exception-unwind paths, not a genuine
     * per-class CursorEditWindow method — matches CLAUDE.md's stub
     * exemption for compiler-generated EH helpers ("documented but not
     * reimplemented"). It has zero normal-control-flow callers, so it
     * was never reachable as a real API even in the original binary. */
};

/* ================================================================== */
/* Bridge Constructor (C linkage for ResourceManager)                 */
/* ================================================================== */

extern "C" {

/**
 * CursorEditWindow_Ctor — Placement-new constructor bridge.
 * Address: (this file, new definition)
 *
 * Bridges from C-style allocation in ResourceManager::AddString
 * (which uses operator_new) to C++ construction via placement-new.
 * Replaced a dangling extern declaration (`CGWND_CursorEditWindow_Ctor`)
 * that had no body — renamed to this class's own convention (matching
 * `TrainStation_Ctor`/`BuildingDescriptorEditor_Ctor`'s sibling naming)
 * once resources/ResourceManager.cpp was wired to call it directly via
 * this header instead of a mismatched local forward declaration.
 *
 * @param memory      Pre-allocated 0x7AC-byte buffer (from operator_new)
 * @param resId       Resource ID
 * @param name        Cursor name string (real `const char*`, not an int32
 *                    ABI handle — see CursorEditWindow's own constructor doc)
 * @return            Pointer to constructed CursorEditWindow (== memory)
 */
void* CursorEditWindow_Ctor(void* memory, int32_t resId, const char* name);

} // extern "C"

/* ================================================================== */
/* Layout verification (x86 32-bit only)                              */
/* ================================================================== */

#if UINTPTR_MAX == 0xffffffffu

static_assert(offsetof(CursorEditWindow, field_7A8) == 0x7A8,
    "CursorEditWindow::field_7A8 offset mismatch");
static_assert(offsetof(CursorEditWindow, field_7AA) == 0x7AA,
    "CursorEditWindow::field_7AA offset mismatch");
static_assert(sizeof(CursorEditWindow) == 0x7AC,
    "CursorEditWindow must match the 32-bit loco.exe layout (0x7AC bytes)");

#endif
