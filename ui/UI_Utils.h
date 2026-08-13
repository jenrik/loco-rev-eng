/**
 * UI_Utils.h — UI Manager, tooltip, and UIEntity helper functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file contains:
 *   - UI_DefWndProc: default passthrough WindowProc for all UI windows
 *   - UIEntity helper methods: show/hide/enable tooltip child windows
 *   - UI_Manager: singleton manager with three TimerList sub-objects
 *     (text_list, pos_list, update_list) for tooltip lifecycle
 *   - Tooltip creation/destruction/update functions
 *   - MessageBox creation (world-positioned UIEntity popups)
 *
 * UI_Manager size: 0x4C bytes
 * Vtable: 0x477AD0 (VTBL_UI_MANAGER)
 */

#pragma once

#include "../shared/types.h"

// Status: TRANSCRIBED
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

struct Collection;
class GameObject;

/**
 * Typed 0x18-byte collection embedded by UI_Manager.  The original binary
 * uses several template-instantiation vtables for this same field layout;
 * natural C++ construction replaces those staged raw-vtable writes.
 *
 * The original collection vtable (0x477AE8 for pos_list/update_list,
 * 0x477B78 for text_list — both share the same functions at every slot
 * this class implements) has 22 slots. Only the ones this class's own
 * methods and UI_Manager's tooltip-facade call paths actually exercise
 * have been reconstructed here (byte-dumped and decompiled from the
 * live vtable, not inferred from the class's own layout):
 *
 *   slot0  (0x00) Resize        0x435D10
 *   slot3  (0x0C) RemoveAndGet  0x4241E0  (folded into RemoveAt below)
 *   slot4  (0x10) RemoveAt      0x4356E0
 *   slot6  (0x18) RemoveAll     0x424270
 *   slot7  (0x1C) GetItemRaw    0x424530  (folded into GetItem below)
 *   slot8  (0x20) GetItem       0x424030
 *   slot10 (0x28) SetAt         0x424790
 *   slot11 (0x2C) GetCount      0x424000
 *   slot12 (0x30) HasLiveSlot   0x424760  (used only by Add's keyed branch)
 *   slot13 (0x34) Add           0x4362B0
 *   slot17 (0x44) InsertAt      0x4248C0
 *   slot18 (0x48) Compare       0x424960  (keyed-insert only; NOT
 *                                          reconstructed — see Add's doc
 *                                          comment)
 *
 * Every other slot (1/2/5/9/14/15/16/19/20/21 — destructor variants,
 * Clear, and collection-wide helpers not reachable from any tooltip
 * facade) is unused by this class and left unmodeled.
 */
class UITimerList {
public:
    void** items;             /* +0x04 */
    uint32_t capacity;        /* +0x08 */
    uint32_t count;           /* +0x0C */
    int32_t key_offset;       /* +0x10 */
    int32_t key_size;         /* +0x14 */

    virtual ~UITimerList() {}
    /* Resize — vtable[0]. Address: 0x435D10. Grows/shrinks `items` to
     * `new_capacity` slots (trimming trailing NULLs when shrinking,
     * zero-filling new slots, preserving up to min(old,new) entries).
     * Implemented directly in UI_Utils.cpp — see that definition's doc
     * comment for why the original's shared void*-based routine is
     * expressed as a typed method here instead of a shared free
     * function. */
    virtual void Resize(uint32_t new_capacity);

    /* GetItem — vtable[8] (0x424030), itself a one-line forward to
     * vtable[7]/GetItemRaw (0x424530): `if (index >= capacity) return
     * nullptr; return items[index];`. Both folded into one method since
     * nothing else in this class's call graph needs the intermediate
     * virtual hop. */
    virtual void* GetItem(uint32_t index) const;
    virtual uint32_t GetCount() const;           /* original vtable[11] */

    /* RemoveAt — vtable[4] (0x4356E0), which itself calls vtable[3]
     * (0x4241E0, "RemoveAndGet": bounds-checked fetch, then left-compact
     * the array and decrement `count`) and, if that returned non-null,
     * deletes the extracted item through its own virtual destructor —
     * exactly what plain `delete` on a GameObject* does, so that is how
     * it's expressed here instead of a manual vtable[0](1) dispatch. */
    virtual void RemoveAt(uint32_t index);

    /* RemoveAll — vtable[6] (0x424270; Ghidra auto-named this
     * "UI_SetScrollPos", a FLIRT misnomer — it does not touch scroll
     * position). Repeatedly calls RemoveAt(count-1) until the list is
     * empty, deleting every remaining item. */
    virtual void RemoveAll();

    /* SetAt — vtable[10] (0x424790). Bounds-checked slot assignment:
     * fails (returns nullptr) if index > count; grows the array first if
     * index has reached capacity (matching InsertAt's own growth policy,
     * 1.1x — see InsertAt); deletes whatever item currently occupies the
     * slot (through its own virtual destructor) before overwriting it. */
    virtual void* SetAt(uint32_t index, void* item);

    /* InsertAt — vtable[17] (0x4248C0). Grows the backing array (1.1x
     * growth factor — the double constant at the original's 0x477C10 —
     * truncated toward zero) when full, shifts every entry from `index`
     * onward one slot right to open a gap (skipped when appending at
     * `count`), then assigns the new item into the opened slot via SetAt
     * and increments `count`. */
    virtual uint32_t InsertAt(uint32_t index, void* item);

    /* Add — vtable[13] (0x4362B0). When `key_size == 0` (text_list and
     * pos_list — unordered), appends at `count` via InsertAt. When
     * `key_size != 0` (update_list only), the original performs a
     * linear-scan keyed insert (comparator at vtable[18]/0x424960,
     * comparing the field at `key_offset` per `key_size`'s int32/int16/
     * uint16/byte-run encoding). That keyed branch is NOT reconstructed:
     * update_list is only ever populated by UI_CreateMessageBox
     * (0x423AB0), which remains an unimplemented stub returning nullptr
     * on every call site in this tree today, so the branch is
     * unreachable. Asserts loudly instead of silently misbehaving if
     * that ever changes without this branch being implemented first. */
    virtual void Add(void* item);
};

/* ================================================================== */
/* UI_Manager — Tooltip and message box manager                        */
/*                                                                     */
/* The UI Manager singleton manages 3 TimerList sub-objects:           */
/*   - text_list (+0x04): text/string tooltip entries                  */
/*   - pos_list (+0x1C): position-based tooltip entries                */
/*   - update_list (+0x34): update/animation tooltip entries           */
/*                                                                     */
/* Each TimerList is 0x18 bytes with the layout:                      */
/*   +0x00: vtable (Collection/Timer variant)                         */
/*   +0x04: items array (void**)                                       */
/*   +0x08: count / capacity (varies by variant)                       */
/*   +0x0C: capacity / count (varies by variant)                       */
/*   +0x10: extra_field1                                              */
/*   +0x14: extra_field2                                              */
/* ================================================================== */
#define TIMER_LIST_SIZE         0x18

class UI_Manager {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */
/* vtable at +0x00 is compiler-managed */
    /* TimerList sub-objects (each 0x18 bytes) */
    UITimerList text_list;   /* +0x04  text tooltip entries */
    UITimerList pos_list;    /* +0x1C  position tooltip entries */
    UITimerList update_list; /* +0x34  update/animation entries */
    /* Total: 0x4C bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * UI_Manager constructor.
     * Address: 0x4238C0
     *
     * Initializes three TimerList sub-objects:
     *   - text_list at +0x04: resize to 100, vtable 0x477BD0 -> 0x477B78
     *   - pos_list at +0x1C: resize to 100, vtable 0x477B40 -> 0x477AE8
     *   - update_list at +0x34: allocate items array (400 bytes = 100 ptrs),
     *     zero-fill, set count=100, vtable 0x477B40 -> 0x477AE8
     *
     * Sets vtable to VTBL_UI_MANAGER. Calls vtable[0x13] on update_list
     * as final initialization step.
     *
     * NOT called by CGWND_InitAllSubsystems (0x406F90-0x407794) — that
     * range's callers were checked directly and none reach 0x4238C0.
     * The one and only real caller is 0x45C685, inside an anonymous
     * compiler-generated static-initializer thunk at 0x45C680-0x45C697
     * that is itself listed as data (not called) in the MSVC CRT's
     * `.CRT$XCU` static-initializer pointer table at 0x47E020 (part of
     * the array based at 0x47E000). That thunk runs this constructor
     * directly on the global object embedded at 0x4FD220 — i.e. the
     * global `g_tooltip_mgr` storage IS a UI_Manager object in the
     * original binary (not a pointer to one) — then registers a
     * teardown thunk (0x45C6A0, tail-jumps to UI_ResetWindow/reset()
     * at 0x4239E0) with the CRT's atexit-equivalent registrar
     * (0x468170, auto-named `_ungetwc_push_ret` by Ghidra's signature
     * matcher — that name is a FLIRT false-positive; the real behavior,
     * confirmed by decompiling both it and its callee 0x4680E0, is
     * growing an exit-function-pointer table, i.e. this is CRT
     * atexit()/`_onexit()`-style registration). This entire mechanism
     * runs before WinMain, with no explicit call site anywhere in game
     * code. See graphics/DDRAW.cpp's g_tooltip_mgr definition for the
     * host reconstruction (a real global object with automatic static
     * storage duration — the faithful C++ equivalent of this CRT
     * mechanism, requiring no manual host wiring).
     */
    UI_Manager();

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x4239C0
     *
     * Calls UI_ResetWindow to clean up all timer lists and frees memory.
     * Not itself the CRT-registered teardown callback for the global
     * instance — the CRT exit table calls reset() (0x4239E0) directly
     * via a tail-jump thunk (0x45C6A0), skipping this scalar-deleting
     * wrapper, since the global object is static storage (never heap-
     * deleted). See UI_Manager() ctor doc comment above for the full
     * static-initializer evidence chain.
     */
    virtual ~UI_Manager();

    /**
     * Base destructor / reset.
     * Address: 0x4239E0
     *
     * Resets all three TimerList sub-objects: frees items arrays,
     * zeros counts and capacities, resets vtables.
     */
    void reset();

    /* ================================================================ */
    /* Public Methods                                                    */
    /* ================================================================ */

    /**
     * Create a world-positioned message box.
     * Address: 0x423AB0
     *
     * FPS-gated (skips if main_window->fps <= threshold, unless resourceId==0x3861).
     * Validates resource availability (frame count < max frames), then allocates
     * 0xA4 bytes for a UIEntity. Positions it in the update_list (if param_6
     * is set) or pos_list. Returns NULL on failure.
     *
     * @param resourceId  Resource ID for the message box sprite
     * @param param2      Resource-specific short parameter
     * @param direction   Placement direction code
     * @param x           World X coordinate
     * @param y           World Y coordinate
     * @param use_update  1 = add to update_list, 0 = add to pos_list
     * @return            UIEntity pointer, or NULL on failure
     */
    void* createMessageBox(int resourceId, short param2, char direction,
                           int x, int y, char useUpdate);

    /**
     * Create a tooltip (0x88-byte GameObject).
     * Address: 0x423C50
     *
     * Allocates 0x88 bytes, calls GameObject_BaseCtor, sets position via
     * vtable[3], sets flag bit 0x02 at +0x2C, adds to text_list via
     * UITimerList::Add (unordered — text_list's key_size is 0).
     *
     * BLOCKED from the free-function facade (UI_CreateTooltip): the first
     * thing this method does is call GameObject_BaseCtor (0x405790),
     * which remains an unimplemented assert-stub (shared/stubs_impl.cpp).
     * This method itself is fully modeled from the real 0x423C50
     * disassembly; the facade is deliberately left unwired so that real
     * call sites (e.g. ui/UIEntity.cpp's constructor, whenever a
     * resource's childCount > 0) don't start aborting the process. See
     * UI_CreateTooltip's own doc comment in UI_Utils.cpp.
     *
     * @param resourceId  Resource ID for the tooltip sprite
     * @param param2      Resource-specific short parameter
     * @param posX        Tooltip X position
     * @param posY        Tooltip Y position
     * @return            GameObject pointer (0x88 bytes), or NULL on failure
     */
    int* createTooltip(int resourceId, short param2, int posX, int posY);

    /**
     * Destroy a specific tooltip from text_list.
     * Address: 0x423D20
     *
     * Searches text_list (+0x04) via GetItem for the given tooltip
     * pointer, removes (and deletes) it via UITimerList::RemoveAt.
     *
     * @param tooltipPtr  Pointer to the tooltip GameObject to destroy
     */
    void destroyTooltip(int* tooltipPtr);

    /**
     * Cleanup tooltips (partial) — clears pos_list and update_list.
     * Address: 0x423D00
     *
     * Calls UITimerList::RemoveAll on pos_list (+0x1C) and update_list
     * (+0x34), deleting every entry in both. Does NOT touch text_list
     * (+0x04). Used during sprite resets.
     */
    void cleanupTooltips();

    /**
     * Free all tooltip timers — cleanup all three timer lists.
     * Address: 0x423A90
     *
     * Calls UITimerList::RemoveAll on all three timers in order:
     * pos_list, update_list, text_list. Called during CGWND_Cleanup.
     */
    void freeTooltipManager();

    /**
     * Per-frame tooltip maintenance — iterate update_list and pos_list,
     * update scroll/anim, remove completed items.
     * Address: 0x423D70
     *
     * NOT actually "hiding" — this is the animation/scroll tick function.
     * Iterates both update_list and pos_list, calls UI_Window_UpdateScroll
     * on each item, removes (via UITimerList::RemoveAt) those that return
     * 1 (completed). UI_Window_UpdateScroll (0x423560) itself remains an
     * unimplemented assert-stub, but both lists are populated only by
     * UI_CreateMessageBox (0x423AB0, itself an unimplemented stub that
     * always returns nullptr today), so they are always empty and this
     * loop body is unreachable in the current tree — safe to wire the
     * free-function facade despite the downstream stub.
     */
    void hideTooltip();

    /**
     * Set tooltip text — iterate text_list, call vtable[11] on each
     * initialized+visible tooltip with given rect and flags.
     * Address: 0x423E00 (RET 0x14 — a real 5th stack arg; every caller
     * passes literal 1 and the body never reads it).
     */
    void setTooltipText(int a1, int a2, int a3, int a4, int unused5);

    /**
     * Set tooltip position — iterate pos_list, call vtable[11] on each
     * initialized+visible tooltip with given rect and flags.
     * Address: 0x423E80 (RET 0x14 — same unused 5th arg as setTooltipText).
     */
    void setTooltipPos(int a1, int a2, int a3, int a4, int unused5);

    /**
     * Update tooltip — iterate update_list, call vtable[11] on each
     * initialized+visible tooltip. Called from TileMap_ProcessRect.
     * Address: 0x423F00 (RET 0x14 — same unused 5th arg as setTooltipText).
     */
    void updateTooltip(int a1, int a2, int a3, int a4, int unused5);

    /**
     * Reset all tooltips — iterate update_list and pos_list, call
     * vtable[9] on each non-null item.
     * Address: 0x423F80
     *
     * Every item added to update_list/pos_list is a UIEntity (created by
     * UI_CreateMessageBox's UIEntity_Ctor call), and original vtable[9]
     * on UIEntity's vtable is UIEntity::SetVisible (see ui/UIEntity.h's
     * live vtable-slot dump) — expressed here as a typed virtual call
     * through Entity::SetVisible rather than manual vtable indexing.
     */
    void resetTooltips(int param);
};

/* UI_ShowWindow/UI_HideWindow/UI_EnableWindow (0x423840/0x423870/
 * 0x423890) converted 2026-08-09 to real UIEntity virtual overrides —
 * StopSound/Update/SetVisible respectively (vtable[7]/[10]/[9], each
 * confirmed via a live slot-by-slot vtable dump — see ui/UIEntity.h).
 * The free-function names didn't match their real behavior (none of them
 * show/hide/enable anything); see each override's doc comment in
 * ui/UIEntity.h for the evidence and the real per-slot semantics. */

/* ================================================================== */
/* UI_DefWndProc — Default passthrough WindowProc                      */
/* Address: 0x422EA0                                                   */
/*                                                                     */
/* Passes all messages through to DefWindowProcA. Used as the default   */
/* WndProc for many UI vtables (vtable[11] for UI_WindowBase).        */
/* ================================================================== */
void __stdcall UI_DefWndProc(HWND hWnd, UINT msg, void* wParam, void* lParam);

/* #endif removed — header uses #pragma once */
