/**
 * UI_ChildWindow.h — ChildWindow base class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * ChildWindow is a lightweight base class providing resource loading,
 * rendering, and event handling for UI elements (cursors, tool icons,
 * animated overlays). It manages a resource ID, bitmap rendering surface,
 * and per-frame animation state.
 *
 * Size: 0x168 bytes
 * Vtable: 0x477C18 (6 slots)
 *
 * Class hierarchy:
 *   ChildWindow (base class, no parent)
 *     └─ CursorEditWindow (derived in ui/CursorEditWindow.h)
 *     └─ TrainStation (derived in game/TrainStation.h)
 *     └─ BuildingDescriptorEditor (derived in input/BuildingDescriptorEditor.h)
 *
 * Vtable layout (ChildWindow 0x477C18):
 *   [0] +0x00: scalar deleting destructor (compiler-generated thunk @ 0x424B40)
 *              Real cleanup body @ 0x424BA0 — use only this in ~ChildWindow()
 *   [1] +0x04: OnMouseMove(int32_t x, int32_t y) → void* (0x425670)
 *   [2] +0x08: OnMouseLeave() (0x4257F0)
 *   [3] +0x0C: Render(void* stream) → uint8_t (0x424E00) — TODO: decompile
 *   [4] +0x10: Constructor init body (0x424BF0) — NOT a virtual method, reachable
 *              during base construction via vtable dispatch to slot[3]; compiler
 *              manages vtable progression in real C++.
 *   [5] +0x14: NULL (reserved)
 *
 * Evidence trail: UI_CreateChildWindow (0x424AF0) and UI_ChildWindow_Create
 * (0x424BF0) write fields at absolute this-relative offsets (e.g.
 * *(int32_t*)(this+0x10)=0), not offsets past a fixed-size subobject. All
 * offsets are unconditional for every caller. Four sources confirm ChildWindow
 * is a genuine base class, not an overlay across unrelated objects:
 * 1. All field writes are absolute (not subobject-relative).
 * 2. Total size is fixed at operator_new(0x168) across all current callers.
 * 3. Derived constructors call the base ctor, then immediately overwrite the
 *    vptr with their own vtable — the classic C++ base-subobject construction
 *    pattern, confirming the base is a real type, not a field-layout alias.
 * 4. The base vtable is staged (0x477C18) only to enable dispatch during
 *    constructor initialization at 0x424D54 (a Render call through slot[3]);
 *    real derived subclasses override it afterward, so progression is normal
 *    C++ semantics, not an ABI anomaly.
 *
 * Prior revert context: An earlier attempt modeled this as `class ChildWindow
 * : public UI_WindowBase`. That derivation was correctly rejected — UI_WindowBase
 * writes differ semantically (subobject-relative offsets). However, the revert's
 * *broader* conclusion ("ChildWindow is not a base class at all") was wrong,
 * and this evidence corrects it. The revert also noted, correctly, that this
 * session's fresh decompilation of 0x424BF0 recovered the nameParam!=0 branch
 * and confirmed bmpPath layout via disassembly, and that disassembly of 0x424AF0
 * confirmed the overlayRefCount write at +0x158 (word ptr [ESI+0x158], AX).
 *
 * Host layout note: In the x86 ABI, pointers are 4 bytes, so the documented
 * offsets are precise on Windows (_WIN32). On a 64-bit host without packing,
 * the compiler's layout differs (e.g., vptr is 8 bytes, so members shift).
 * CLAUDE.md forbids packing host objects into x86 byte-parity. Named member
 * access via the class achieves the correct behavior on any layout — the
 * compiler enforces type safety and alignment automatically. Exact x86 offsets
 * remain a documentation artifact and a Windows-build concern only (verified
 * by static_assert, below).
 */

#pragma once

#include "../shared/types.h"

// Status: INTEGRATED

class ChildWindow {
public:
    ChildWindow(const ChildWindow&) = delete;
    ChildWindow& operator=(const ChildWindow&) = delete;

    /* ================================================================ */
    /* Fields (offsets from this; vtable at +0x00 is compiler-managed)   */
    /* ================================================================ */

    uint32_t   resourceId;         // +0x04  resource ID for this child window
    uint8_t    resourceType;       // +0x08  type byte from GetResourceType(resourceId)
    uint8_t    _pad_09[3];         // +0x09  padding to align streamData
    void*      streamData;         // +0x0C  stream buffer pointer (zeroed)
    void*      renderSurface;      // +0x10  render surface / child object (released in dtor)
    int16_t    field_14;           // +0x14  zeroed; purpose not evidenced
    int16_t    field_16;           // +0x16  zeroed; purpose not evidenced
    uint8_t    sticky;             // +0x18  sticky flag; OnMouseLeave checks (!= 1) before releasing
    uint8_t    _pad_19;            // +0x19  padding
    uint16_t   subWindowCount;     // +0x1A  number of sub-windows (zeroed)
    int16_t    field_1C;           // +0x1C  zeroed; purpose not evidenced
    int16_t    field_1E;           // +0x1E  zeroed; purpose not evidenced
    void*      heapBuffer;         // +0x20  GLOBAL_free'd in dtor; also entry-table pointer when
                                     //        subWindowCount != 0 (dual role confirmed in Paint/OnMouseLeave)
    void*      subObject;          // +0x24  sub-object pointer (released in dtor via vtable slot 0, flags=1)
    int16_t    field_28;           // +0x28  zeroed; purpose not evidenced
    int16_t    field_2A;           // +0x2A  zeroed; purpose not evidenced
    int16_t    frameCount;         // +0x2C  zeroed; read by IsBitmapReady()
    int16_t    _reserved_2E[2];    // +0x2E  4 bytes, no writer evidenced
    int16_t    roadOffsetX;        // +0x32  zeroed; x-offset for road overlay
    int16_t    roadOffsetY;        // +0x34  zeroed; y-offset for road overlay
    int16_t    _reserved_36;       // +0x36  2 bytes, no writer evidenced
    int32_t    field_38;           // +0x38  zeroed; purpose not evidenced
    int32_t    field_3C;           // +0x3C  zeroed; purpose not evidenced
    int32_t    depResourceId1;     // +0x40  dependent resource ID #1 (default -1)
    int32_t    depResourceId2;     // +0x44  dependent resource ID #2 (default -1)
    char       bmpPath[0x105];     // +0x48  261-byte buffer for bitmap path (sprintf target)
    uint8_t    field_14D;          // +0x14D zeroed before sprintf; purpose not evidenced
    uint8_t    _reserved_14E[10];  // +0x14E no writer evidenced
    int16_t    overlayRefCount;    // +0x158 incremented in OnMouseMove, decremented in OnMouseLeave;
                                     //        renderSurface released when == 0 && !sticky
    uint8_t    _reserved_15A[2];   // +0x15A no writer evidenced
    int32_t    field_15C;          // +0x15C default -1; purpose not evidenced
    int16_t    field_160;          // +0x160 default 1; used as divisor in OnMouseMove
    uint8_t    loaded;             // +0x162 zero until resource loads via Render()
    uint8_t    ready;              // +0x163 default 1; "ready" / "easter-egg" flag
    int32_t    animFlags;          // +0x164 animation metadata flags (zeroed)

    /* Total: 0x168 bytes (including compiler-managed vtable at +0x00) */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * ChildWindow constructor.
     * Address: 0x424AF0 (wrapper) + 0x424BF0 (init body)
     *
     * Initializes all fields to zero/defaults, and (if nameParam != 0)
     * loads associated .dat/.bmp resources via resource manager or disk.
     *
     * Called by: CursorEditWindow::CursorEditWindow (0x40E600),
     *            TrainStation_Ctor (0x436400),
     *            BuildingDescriptorEditor constructor.
     *
     * @param resourceId  Resource ID to load for this window
     * @param nameParam   Non-zero to load resources immediately (currently
     *                    deferred — no caller uses this yet)
     */
    ChildWindow(uint32_t resourceId, int32_t nameParam);

    /**
     * Virtual destructor (vtable[0]).
     * Address: 0x424B40 (scalar-deleting-destructor thunk, compiler-generated)
     * Body: 0x424BA0 (UI_ChildWindow_Dtor — real cleanup logic)
     *
     * Clears the loaded flag (+0x162), releases renderSurface and subObject
     * sub-resources (each via its own vtable slot 0, flags=1), and frees
     * heapBuffer.
     */
    virtual ~ChildWindow();

    /* ================================================================ */
    /* Virtual Methods                                                   */
    /* ================================================================ */

    /**
     * OnMouseMove — Handle mouse motion over this window.
     * Address: 0x425670 (UI_PaintWindow)
     * Vtable slot: [1] +0x04
     *
     * Creates the render surface on first call (stretch-blits the bitmap at
     * +0x48 bmpPath), then recomputes per-frame width/height, increments
     * the overlay refcount (+0x158), loads frame-set sounds, and (for
     * resource 0x842, the clock) advances the animated-clock resource.
     *
     * @param x      Mouse X coordinate (passed to UIPANEL_StretchBlit)
     * @param y      Mouse Y coordinate (passed to UIPANEL_StretchBlit)
     * @return       The render-surface pointer (+0x10), or null if unavailable.
     */
    virtual void* OnMouseMove(int32_t x, int32_t y);

    /**
     * OnMouseLeave — Handle mouse leaving this window.
     * Address: 0x4257F0 (UI_OnMouseLeave)
     * Vtable slot: [2] +0x08
     *
     * Decrements the overlay refcount (+0x158); when it reaches 0 and the
     * window is not "sticky" (+0x18 != 1), releases the render surface
     * (+0x10, via its own vtable slot 0) and the frame-set's dependent
     * sound resources.
     */
    virtual void OnMouseLeave();

    /**
     * Render — Parse .dat descriptor stream and load bitmap resource.
     * Address: 0x424E00 (UI_ChildWindow_Render)
     * Vtable slot: [3] +0x0C
     *
     * TODO: decompile 0x424E00. Ghidra's decompilation (2032 bytes, ~150
     * lines, deeply nested string-keyword dispatch with internal gotos) is
     * available but depends on three stream helpers (WNDPROC_StreamReadLine,
     * WNDPROC_StreamPrintf, WNDPROC_StreamWrite) that have no other callers
     * to evidence their real signatures. Tracked in PROGRESS.md. Not
     * currently reachable (no current caller uses nameParam != 0 in
     * constructor).
     *
     * @param stream  Open .dat stream pointer
     * @return        Non-zero if load succeeds (set to loaded flag +0x162)
     */
    virtual uint8_t Render(void* stream);

    /* ================================================================ */
    /* Non-Virtual Methods                                               */
    /* ================================================================ */

    /**
     * IsBitmapReady — Check whether this window's bitmap is ready to render.
     * Address: 0x4255F0 (UI_IsBitmapReady)
     * NOT a virtual method (plain member, non-virtual).
     *
     * Checks: ready flag (+0x163), subObject pointer (+0x24), frameCount
     * (+0x2C), and two dependent resource IDs (+0x40/+0x44) via
     * ResourceManager_GetById, with a scenario-mode special case for
     * resource 0xC42.
     *
     * @return  Non-zero when ready to render, zero otherwise.
     */
    bool IsBitmapReady() const;

    /**
     * InitFields — Initialize all ChildWindow fields from resource ID.
     * Address: 0x424BF0 (UI_ChildWindow_Create body)
     *
     * Public to model the binary's init flow: UI_CreateChildWindow
     * (0x424AF0) zeroes fields, writes the vtable, then calls the init
     * body at 0x424BF0 (factored here as InitFields). The extern "C" init
     * shim and the real constructor both call this to populate member
     * variables.
     *
     * @param resourceId  Resource ID
     * @param nameParam   Non-zero to load resources immediately (deferred)
     */
    void InitFields(uint32_t resourceId, int32_t nameParam);
};

/* ================================================================== */
/* Extern "C" Compatibility Shims                                     */
/* ================================================================== */

extern "C" {

/**
 * UI_CreateChildWindow — ChildWindow init shim @ 0x424AF0
 */
void* UI_CreateChildWindow(void* self, uint32_t resourceId, int32_t nameParam);

/**
 * UI_ChildWindow_Create — Init-body shim @ 0x424BF0
 */
void UI_ChildWindow_Create(void* self, uint32_t resourceId, int32_t nameParam);

/**
 * UI_ChildWindow_Dtor — Destructor shim @ 0x424BA0
 */
void UI_ChildWindow_Dtor(void* self);

/**
 * UI_ChildWindow_Render — Render shim @ 0x424E00
 */
uint8_t UI_ChildWindow_Render(void* self, void* stream);

/**
 * UI_IsBitmapReady — Bitmap-ready check shim @ 0x4255F0
 */
int32_t UI_IsBitmapReady(int32_t self);

/**
 * UI_PaintWindow — OnMouseMove shim @ 0x425670
 */
void* UI_PaintWindow(void* self, int32_t param1, int32_t param2);

/**
 * UI_OnMouseLeave — OnMouseLeave shim @ 0x4257F0
 */
void UI_OnMouseLeave(void* self);

} // extern "C"

/* ================================================================== */
/* Layout verification (x86 32-bit only)                              */
/* ================================================================== */

#if UINTPTR_MAX == 0xffffffffu

static_assert(offsetof(ChildWindow, resourceId) == 0x04,
    "ChildWindow::resourceId offset mismatch");
static_assert(offsetof(ChildWindow, resourceType) == 0x08,
    "ChildWindow::resourceType offset mismatch");
static_assert(offsetof(ChildWindow, streamData) == 0x0C,
    "ChildWindow::streamData offset mismatch");
static_assert(offsetof(ChildWindow, renderSurface) == 0x10,
    "ChildWindow::renderSurface offset mismatch");
static_assert(offsetof(ChildWindow, sticky) == 0x18,
    "ChildWindow::sticky offset mismatch");
static_assert(offsetof(ChildWindow, subWindowCount) == 0x1A,
    "ChildWindow::subWindowCount offset mismatch");
static_assert(offsetof(ChildWindow, heapBuffer) == 0x20,
    "ChildWindow::heapBuffer offset mismatch");
static_assert(offsetof(ChildWindow, subObject) == 0x24,
    "ChildWindow::subObject offset mismatch");
static_assert(offsetof(ChildWindow, frameCount) == 0x2C,
    "ChildWindow::frameCount offset mismatch");
static_assert(offsetof(ChildWindow, roadOffsetX) == 0x32,
    "ChildWindow::roadOffsetX offset mismatch");
static_assert(offsetof(ChildWindow, roadOffsetY) == 0x34,
    "ChildWindow::roadOffsetY offset mismatch");
static_assert(offsetof(ChildWindow, field_38) == 0x38,
    "ChildWindow::field_38 offset mismatch");
static_assert(offsetof(ChildWindow, field_3C) == 0x3C,
    "ChildWindow::field_3C offset mismatch");
static_assert(offsetof(ChildWindow, depResourceId1) == 0x40,
    "ChildWindow::depResourceId1 offset mismatch");
static_assert(offsetof(ChildWindow, depResourceId2) == 0x44,
    "ChildWindow::depResourceId2 offset mismatch");
static_assert(offsetof(ChildWindow, bmpPath) == 0x48,
    "ChildWindow::bmpPath offset mismatch");
static_assert(offsetof(ChildWindow, field_14D) == 0x14D,
    "ChildWindow::field_14D offset mismatch");
static_assert(offsetof(ChildWindow, overlayRefCount) == 0x158,
    "ChildWindow::overlayRefCount offset mismatch");
static_assert(offsetof(ChildWindow, field_15C) == 0x15C,
    "ChildWindow::field_15C offset mismatch");
static_assert(offsetof(ChildWindow, field_160) == 0x160,
    "ChildWindow::field_160 offset mismatch");
static_assert(offsetof(ChildWindow, loaded) == 0x162,
    "ChildWindow::loaded offset mismatch");
static_assert(offsetof(ChildWindow, ready) == 0x163,
    "ChildWindow::ready offset mismatch");
static_assert(offsetof(ChildWindow, animFlags) == 0x164,
    "ChildWindow::animFlags offset mismatch");
static_assert(sizeof(ChildWindow) == 0x168,
    "ChildWindow must match the 32-bit loco.exe layout (0x168 bytes)");

#endif
