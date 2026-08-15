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
 *   [3] +0x0C: Render(void* stream) → uint8_t (0x424E00)
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
 * 2. Total size was fixed at the original x86 sizeof(ChildWindow), 0x168,
 *    across all callers (now operator_new(sizeof(ChildWindow)) == 0x180 on
 *    this 64-bit host, since pointer-bearing fields widen).
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

struct UIPANEL_Surface; /* graphics/LOCOBITMAP.h — DDraw surface wrapper */

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
    uint8_t    _pad_09[3];         // +0x09  padding to align shadowId
    int32_t    shadowId;           // +0x0C  "ShadowId" .dat directive (zeroed; was misnamed
                                     //        `streamData` — a Render() cross-check this session
                                     //        showed it's a WNDPROC_StreamWrite(stream, &this+0xC)
                                     //        target for the "ShadowId" keyword, a plain int32, not
                                     //        a stream/void* pointer)
    UIPANEL_Surface* renderSurface; // +0x10  render surface / child object (released in dtor)
    int16_t    field_14;           // +0x14  zeroed; purpose not evidenced
    int16_t    field_16;           // +0x16  zeroed; purpose not evidenced
    uint8_t    sticky;             // +0x18  sticky flag; OnMouseLeave checks (!= 1) before releasing
    uint8_t    _pad_19;            // +0x19  padding
    uint16_t   frameSetCount;      // +0x1A  "number_of_frame_sets" .dat directive (zeroed; was
                                     //        misnamed `subWindowCount`). Drives the heapBuffer
                                     //        entry-table allocation size (frameSetCount * 0x18)
                                     //        in Render().
    int16_t    cursorFrameSetIndex;   // +0x1C  "cursor_frame_set" directive's frame-set index
                                        //        (zeroed; was `field_1C`), validated against
                                        //        frameSetCount in Render().
    int16_t    defaultFrameSetIndex;  // +0x1E  "cursor_default_frame_set" directive's frame-set
                                        //        index (zeroed; was `field_1E`), same validation.
    void*      heapBuffer;         // +0x20  GLOBAL_free'd in dtor; frame-set entry-table pointer
                                     //        (frameSetCount entries, 0x18 bytes each — populated
                                     //        by Render()'s cursor_frame_set branch, read by
                                     //        OnMouseMove/OnMouseLeave via entry+0x0E "stringId")
    UIPANEL_Surface* bitmapSurface; // +0x24  static bitmap surface for this window's own frame
                                     //        image (was misnamed `subObject`; created in Render()
                                     //        via `new UIPANEL_Surface()`, the same construction
                                     //        idiom as OnMouseMove's overlay `renderSurface` at
                                     //        +0x10 but for a *different*, statically-loaded
                                     //        surface — released in dtor via `delete`, same as
                                     //        renderSurface).
    int16_t    field_28;           // +0x28  computed in Render()'s tail as
                                     //        bitmapSurface[0x08](dword)/frameCount — same
                                     //        "per-frame width" role as OnMouseMove's field_14 for
                                     //        the overlay surface, but for bitmapSurface.
    int16_t    field_2A;           // +0x2A  copied from bitmapSurface+0xC in Render()'s tail —
                                     //        same role as OnMouseMove's field_16 for the overlay
                                     //        surface, but for bitmapSurface.
    int16_t    frameCount;         // +0x2C  "button" directive's 3rd value (default 3 — a button's
                                     //        state count: up/hover/down); read by IsBitmapReady()
                                     //        and used as bitmapSurface's per-frame-width divisor.
    int16_t    buttonParam1;       // +0x2E  "button" directive's 1st value (zeroed; was part of
                                     //        an undocumented `_reserved_2E[2]` gap — exact
                                     //        semantic beyond "part of a button line" unresolved).
    int16_t    buttonParam2;       // +0x30  "button" directive's 2nd value (zeroed; same gap).
    int16_t    hotspotX;           // +0x32  "hotspot" .dat directive, x (zeroed; was misnamed
                                     //        `roadOffsetX` from TrainStation's own, DIFFERENT
                                     //        reinterpretation of this same storage as a road-
                                     //        connection offset — ChildWindow's own Render() is
                                     //        the base-level authority: this is a cursor/bitmap
                                     //        hotspot coordinate, read via WNDPROC_StreamReadLine
                                     //        for the "hotspot" keyword. TrainStation's reuse is a
                                     //        legitimate derived-class reinterpretation of the same
                                     //        bytes, not a contradiction — see game/TrainStation.cpp).
    int16_t    hotspotY;           // +0x34  "hotspot" .dat directive, y (zeroed; was `roadOffsetY`).
    int16_t    _reserved_36;       // +0x36  2 bytes, no writer evidenced
    int32_t    shadowOffsetX;      // +0x38  "ShadowOffset" directive, x (zeroed; was `field_38`)
    int32_t    shadowOffsetY;      // +0x3C  "ShadowOffset" directive, y (zeroed; was `field_3C`)
    int32_t    depResourceId1;     // +0x40  "must/cant_have" directive, 1st value (default -1)
    int32_t    depResourceId2;     // +0x44  "must/cant_have" directive, 2nd value (default -1)
    char       bmpPath[0x105];     // +0x48  261-byte buffer for bitmap path (sprintf target)
    char       name[10];           // +0x14D "Name" .dat directive (was misnamed `field_14D`, a
                                     //        single byte — Render()'s "Name" branch does
                                     //        `strncpy(this+0x14D, line+1, 10)`, proving it's a
                                     //        10-byte buffer, not one byte). Trailing \r/\n
                                     //        trimmed by Render(); not null-terminated by strncpy
                                     //        alone if the source fills all 10 bytes.
    uint8_t    field_157;          // +0x157 zeroed unconditionally by the "Name" branch right
                                     //        after the strncpy above (was part of an
                                     //        undocumented `_reserved_14E[10]` gap); no other
                                     //        writer or reader evidenced yet.
    int16_t    overlayRefCount;    // +0x158 incremented in OnMouseMove, decremented in OnMouseLeave;
                                     //        renderSurface released when == 0 && !sticky
    uint8_t    _reserved_15A[2];   // +0x15A no writer evidenced
    int32_t    maxInstances;       // +0x15C  "MaxInstances" .dat directive (default -1 = no
                                     //        limit; was misnamed `field_15C`). Populated via a
                                     //        call Ghidra decompiles as `CRT_fabs(stream, &this+
                                     //        0x15C)` — already flagged elsewhere in this codebase
                                     //        (input/BuildingDescriptorEditor.cpp) as a likely-
                                     //        misidentified thunk, since CRT_fabs really takes a
                                     //        double; the exact callee is unresolved, but the
                                     //        keyword-to-offset mapping itself is direct and solid.
    int16_t    totalFrameCount;    // +0x160  "total_number_of_frames" .dat directive (default 1;
                                     //        was misnamed `field_160`); used as OnMouseMove's
                                     //        per-frame-width divisor for the overlay surface.
    uint8_t    loaded;             // +0x162 zero until resource loads via Render()
    uint8_t    ready;              // +0x163 default 1; "ready" / "easter-egg" flag
    int32_t    animFlags;          // +0x164 animation metadata flags (zeroed); Render() sets bit
                                     //        0x400 for "semi-transparent", bit 0x2 for "shadows"

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
     * Clears the loaded flag (+0x162), releases renderSurface and bitmapSurface
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
     * Render — Parse a .dat descriptor stream and load this window's
     * static bitmap resource.
     * Address: 0x424E00 (UI_ChildWindow_Render), 2032 bytes / ~150 lines.
     * Vtable slot: [3] +0x0C
     *
     * Decompiled for real this session — the "no other callers to evidence
     * WNDPROC_StreamReadLine/Printf/Write's signatures" blocker recorded
     * earlier no longer applied: TrainStation::Render and
     * BuildingDescriptorEditor::Render (both decompiled in the same earlier
     * pass that built this class hierarchy) already established real
     * call-site evidence for all three, which this function's own
     * decompilation independently confirmed byte-for-byte against
     * disassembly (the WNDPROC_Stream* calls, the CRT_wcsstr inverted-match
     * convention, and the "-9" terminator sentinel all match the sibling
     * Render overrides exactly).
     *
     * Reads keyword-prefixed directive lines from the stream in a loop
     * until a terminator line ("-9") or the stream's own "ended" bit is
     * hit. Recognized directives: "button" (buttonParam1/buttonParam2/
     * frameCount, default 3), "Name" (10-byte `name` buffer, trailing
     * \r/\n trimmed), "hotspot" (hotspotX/hotspotY), "ShadowId"
     * (shadowId), "ShadowOffset" (shadowOffsetX/shadowOffsetY), and —
     * only on lines that do NOT match the literal keyword "animation"
     * (a recognized-but-otherwise-inert section marker, confirmed via the
     * disassembly's `!= 0` polarity flip at this one branch, unlike every
     * other keyword check in this function) — "semi-transparent"
     * (animFlags |= 0x400), "shadows" (animFlags |= 0x2), "must/cant_have"
     * (depResourceId1/depResourceId2), "MaxInstances" (maxInstances),
     * "total_number_of_frames" (totalFrameCount, default 1),
     * "number_of_frame_sets" (frameSetCount, allocates the heapBuffer
     * entry table sized frameSetCount*0x18 — an early, clean return on
     * allocation failure, matching the original exactly), and finally
     * "cursor_frame_set"/"cursor_default_frame_set", which populate
     * cursorFrameSetIndex/defaultFrameSetIndex and then fill every
     * heapBuffer entry from the stream (a section-terminator line that
     * matches NEITHER of those two keywords breaks the outer loop
     * entirely, per the disassembly's double-negative check).
     *
     * After the loop, skips forward past any leading '/'-prefixed comment
     * lines, then — if bmpPath is non-trivial (length > 2) — composes a
     * scratch copy of bmpPath with its last 2 characters replaced by the
     * literal suffix "ut" (traced precisely via raw disassembly at
     * 0x4254C5-0x425542, since the decompiler's own stack-variable
     * splitting was internally inconsistent here; the net byte-level
     * effect — confirmed algebraically independent of exactly which
     * stack-relative offset the compiler's inlined strcat-style routine
     * started scanning from — is unambiguous), allocates bitmapSurface via
     * UIPANEL_CreateSurface, stretch-blits that composed path into it via
     * UIPANEL_StretchBlit, releases it immediately if empty (matching
     * OnMouseMove's identical "surface[6]==0 && surface[7]==0" dead-
     * surface check), and — if frameCount is non-zero — computes field_28/
     * field_2A from bitmapSurface's header (the same "surface[8]/count"
     * and "surface+0xC" pattern OnMouseMove already uses for the overlay
     * surface, just against bitmapSurface/frameCount instead of
     * renderSurface/totalFrameCount).
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
     * Checks: ready flag (+0x163), bitmapSurface pointer (+0x24), frameCount
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

/* sizeof(ChildWindow) on this host — see ui/UI_ChildWindow.cpp. Plain C++
 * linkage; must NOT be declared inside the extern "C" block below. */
size_t ChildWindow_Size();

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
static_assert(offsetof(ChildWindow, shadowId) == 0x0C,
    "ChildWindow::shadowId offset mismatch");
static_assert(offsetof(ChildWindow, renderSurface) == 0x10,
    "ChildWindow::renderSurface offset mismatch");
static_assert(offsetof(ChildWindow, sticky) == 0x18,
    "ChildWindow::sticky offset mismatch");
static_assert(offsetof(ChildWindow, frameSetCount) == 0x1A,
    "ChildWindow::frameSetCount offset mismatch");
static_assert(offsetof(ChildWindow, cursorFrameSetIndex) == 0x1C,
    "ChildWindow::cursorFrameSetIndex offset mismatch");
static_assert(offsetof(ChildWindow, defaultFrameSetIndex) == 0x1E,
    "ChildWindow::defaultFrameSetIndex offset mismatch");
static_assert(offsetof(ChildWindow, heapBuffer) == 0x20,
    "ChildWindow::heapBuffer offset mismatch");
static_assert(offsetof(ChildWindow, bitmapSurface) == 0x24,
    "ChildWindow::bitmapSurface offset mismatch");
static_assert(offsetof(ChildWindow, frameCount) == 0x2C,
    "ChildWindow::frameCount offset mismatch");
static_assert(offsetof(ChildWindow, buttonParam1) == 0x2E,
    "ChildWindow::buttonParam1 offset mismatch");
static_assert(offsetof(ChildWindow, buttonParam2) == 0x30,
    "ChildWindow::buttonParam2 offset mismatch");
static_assert(offsetof(ChildWindow, hotspotX) == 0x32,
    "ChildWindow::hotspotX offset mismatch");
static_assert(offsetof(ChildWindow, hotspotY) == 0x34,
    "ChildWindow::hotspotY offset mismatch");
static_assert(offsetof(ChildWindow, shadowOffsetX) == 0x38,
    "ChildWindow::shadowOffsetX offset mismatch");
static_assert(offsetof(ChildWindow, shadowOffsetY) == 0x3C,
    "ChildWindow::shadowOffsetY offset mismatch");
static_assert(offsetof(ChildWindow, depResourceId1) == 0x40,
    "ChildWindow::depResourceId1 offset mismatch");
static_assert(offsetof(ChildWindow, depResourceId2) == 0x44,
    "ChildWindow::depResourceId2 offset mismatch");
static_assert(offsetof(ChildWindow, bmpPath) == 0x48,
    "ChildWindow::bmpPath offset mismatch");
static_assert(offsetof(ChildWindow, name) == 0x14D,
    "ChildWindow::name offset mismatch");
static_assert(offsetof(ChildWindow, field_157) == 0x157,
    "ChildWindow::field_157 offset mismatch");
static_assert(offsetof(ChildWindow, overlayRefCount) == 0x158,
    "ChildWindow::overlayRefCount offset mismatch");
static_assert(offsetof(ChildWindow, maxInstances) == 0x15C,
    "ChildWindow::maxInstances offset mismatch");
static_assert(offsetof(ChildWindow, totalFrameCount) == 0x160,
    "ChildWindow::totalFrameCount offset mismatch");
static_assert(offsetof(ChildWindow, loaded) == 0x162,
    "ChildWindow::loaded offset mismatch");
static_assert(offsetof(ChildWindow, ready) == 0x163,
    "ChildWindow::ready offset mismatch");
static_assert(offsetof(ChildWindow, animFlags) == 0x164,
    "ChildWindow::animFlags offset mismatch");
static_assert(sizeof(ChildWindow) == 0x168,
    "ChildWindow must match the 32-bit loco.exe layout (0x168 bytes)");

#endif
