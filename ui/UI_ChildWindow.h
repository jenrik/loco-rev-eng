/**
 * UI_ChildWindow.h — Shared "ChildWindow" free-function cluster
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions operate on any object that follows the "ChildWindow"
 * field-layout convention: a block of resource/render-state fields
 * living at the object's OWN start address (resourceId +0x04, streamData
 * +0x0C, render-surface/childObj +0x10, ..., loaded +0x162, ...).
 * ui/CursorEditWindow.h documents these fields by name directly on
 * CursorEditWindow itself; game/TrainStation.cpp accesses the same
 * conceptual fields via raw this-relative offsets. NEITHER inherits from
 * a "ChildWindow" base class — in the original binary or in this port.
 *
 * Evidence: UI_CreateChildWindow (0x424AF0) writes absolute this-relative
 * offsets (`*(int32_t*)((int)this+0x10)=0`, etc.), not offsets past some
 * fixed-size subobject. A prior attempt this session modeled this as
 * `class UI_ChildWindow : public UI_WindowBase` and was reverted — do
 * not reintroduce that. See ui/CursorEditWindow.h's header comment for
 * the full supporting evidence (verified via `git show HEAD:...`).
 *
 * Host note: exact x86 field offsets are NOT preserved by CursorEditWindow
 * or TrainStation's native (non-packed) C++ layout on a 64-bit host —
 * measured directly (offsetof(CursorEditWindow, childObj) is 0x18 on this
 * host, not the documented x86 0x10, because the vtable pointer is 8
 * bytes here instead of 4). CLAUDE.md's host-deviation policy says exact
 * x86 layout parity is a documentation concern, not a host-build goal, and
 * forbids packing/casting host objects into x86 layout parity. The bodies
 * below are therefore assembly-faithful raw-offset implementations under
 * `_WIN32` ONLY (correct there — 4-byte x86 pointers make the documented
 * offsets real, and this is what the MinGW typecheck build exercises).
 * The non-Windows host path is a loud, tracked, deferred stub rather than
 * a silent no-op: neither CursorEditWindow's real (compiler-chosen, non-
 * x86) member layout nor a single shared offset-based view can safely
 * alias both CursorEditWindow and TrainStation objects on this host
 * without either corrupting real typed fields or inventing a new shared
 * base class. Tracked in PROGRESS.md. None of the current call sites
 * (CursorEditWindow's own constructor path, TrainStation_Ctor, and
 * RESDATA_ScriptedObject::Start / Town::handle_tile_click, which are both
 * themselves currently unreached — the world overlay cone is still being
 * built) exercise these on the host today.
 */

#pragma once

#include "../shared/types.h"

// Status: TRANSCRIBED

extern "C" {

/**
 * UI_CreateChildWindow — ChildWindow "constructor" helper.
 * Address: 0x424AF0
 *
 * Zeros several ChildWindow-convention fields (render-surface pointer
 * +0x10, sub-object pointer +0x24, heap buffer +0x20, byte flag +0x18,
 * overlay refcount +0x158, animation-metadata flags +0x164, and the
 * road-offset pair +0x32/+0x34), then delegates to UI_ChildWindow_Create.
 * The binary also stages the ChildWindow vtable (0x477C18) here; that
 * write is a constructor artifact — every real caller (CursorEditWindow,
 * TrainStation) is a C++ object whose own constructor installs its real
 * (derived) vtable immediately afterwards, so the compiler-managed
 * vtable makes the explicit write unnecessary.
 *
 * Called by: CursorEditWindow::CursorEditWindow (0x40E600, always with
 * nameParam=0 — see ui/CursorEditWindow.cpp), TrainStation_Ctor (0x436400,
 * also always with nameParam=0 — see game/TrainStation.cpp).
 *
 * @param self        ChildWindow-convention object (CursorEditWindow*,
 *                     TrainStation*, ...)
 * @param resourceId  Resource ID for this child window
 * @param nameParam   Non-zero to load sprite/cursor data immediately
 * @return            self
 */
void* UI_CreateChildWindow(void* self, uint32_t resourceId, int32_t nameParam);

/**
 * UI_ChildWindow_Create — Populate ChildWindow fields from a resource ID
 * and (when nameParam != 0) load the associated .dat/.bmp resources.
 * Address: 0x424BF0
 *
 * The nameParam != 0 branch is not exercised by any current caller in
 * this codebase (CursorEditWindow and TrainStation both call
 * UI_CreateChildWindow — and therefore this — with nameParam == 0,
 * handling their own resource loading separately: see
 * CursorEditWindow::init() and TrainStation_Init()). It also dispatches
 * through the receiver's own vtable slot 3 (loadCursorData) and depends
 * on a vararg CRT_sprintf_buf call whose exact argument list Ghidra could
 * not fully recover for this call site. Rather than guess, that branch is
 * a loud deferred stub; the unconditional field-initialization prefix
 * (the part every real caller depends on) is fully transcribed.
 *
 * @param self        ChildWindow-convention object
 * @param resourceId  Resource ID
 * @param nameParam   Non-zero to load resources immediately (deferred —
 *                     see above)
 */
void UI_ChildWindow_Create(void* self, uint32_t resourceId, int32_t nameParam);

/**
 * UI_ChildWindow_Dtor — ChildWindow base destructor body (no self-free).
 * Address: 0x424BA0
 *
 * Clears the loaded flag (+0x162), then releases the render-surface
 * sub-object at +0x10 and the sub-object at +0x24 (each via its own
 * vtable slot 0, flags=1), and frees the heap buffer at +0x20.
 */
void UI_ChildWindow_Dtor(void* self);

/**
 * UI_ChildWindow_Render — Parse a .dat descriptor stream into ChildWindow
 * fields (buttons, hotspot, shadow offset/id, animation flags, frame-set
 * table), then load/render the associated .bmp sprite sheet.
 * Address: 0x424E00
 *
 * TODO: decompile 0x424E00. Ghidra's decompilation (2032 bytes, ~150
 * lines, a deeply nested string-keyword dispatch with several internal
 * gotos) is available but depends on three stream helpers
 * (WNDPROC_StreamReadLine / WNDPROC_StreamPrintf / WNDPROC_StreamWrite)
 * that have no other caller anywhere in this codebase to evidence their
 * real signatures from — Ghidra's own guesses for them (`undefined2*`,
 * `undefined4*`) are not load-bearing enough to transcribe with
 * confidence. Tracked in PROGRESS.md rather than silently half-ported.
 * Not currently reachable: see the header comment above.
 */
uint8_t UI_ChildWindow_Render(void* self, void* stream);

/**
 * UI_IsBitmapReady — Check whether a ChildWindow's bitmap resource is
 * ready to render: easter-egg/ready flag (+0x163), render surface
 * (+0x24), frame count (+0x2C), and two dependent resource IDs
 * (+0x40/+0x44) via ResourceManager_GetById, with a scenario-mode special
 * case for resource 0xC42.
 * Address: 0x4255F0
 *
 * @param self  ChildWindow-convention object, expressed as a truncated
 *              32-bit pointer value — matches every existing caller
 *              (Town::handle_tile_click, RESDATA_ScriptedObject::Start),
 *              which already narrow their real pointer via
 *              `(int)(intptr_t)res` before calling this.
 * @return      Non-zero when ready to render.
 */
int32_t UI_IsBitmapReady(int32_t self);

/**
 * UI_PaintWindow — Render/refresh a ChildWindow's bitmap surface.
 * Address: 0x425670
 *
 * Creates the render surface on first call (stretch-blits the bitmap at
 * +0x48), then recomputes per-frame width/height, increments the overlay
 * refcount (+0x158), loads frame-set sounds, and (for resource 0x842,
 * the clock) advances the animated-clock resource.
 *
 * @return  The render-surface pointer (+0x10), or null if unavailable.
 */
void* UI_PaintWindow(void* self, int32_t param1, int32_t param2);

/**
 * UI_OnMouseLeave — Handle mouse leaving a ChildWindow: decrements the
 * overlay refcount (+0x158); when it reaches 0 and the window is not
 * "sticky" (+0x18 != 1), releases the render surface (+0x10, via its own
 * vtable slot 0) and the frame-set's dependent sound resources.
 * Address: 0x4257F0
 */
void UI_OnMouseLeave(void* self);

} // extern "C"
