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

/**
 * Typed 0x18-byte collection embedded by UI_Manager.  The original binary
 * uses several template-instantiation vtables for this same field layout;
 * natural C++ construction replaces those staged raw-vtable writes.
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
    virtual void* GetItem(uint32_t index) const; /* original vtable[8] */
    virtual uint32_t GetCount() const;           /* original vtable[11] */
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
     * vtable[3], sets flag bit 0x02 at +0x2C, adds to text_list at +0x04.
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
     * Searches text_list (+0x04) for the given tooltip pointer via vtable[8],
     * removes it via vtable[4] (RemoveAt).
     *
     * @param tooltipPtr  Pointer to the tooltip GameObject to destroy
     */
    void destroyTooltip(int* tooltipPtr);

    /**
     * Cleanup tooltips (partial) — clears pos_list and update_list.
     * Address: 0x423D00
     *
     * Calls vtable+0x18 on pos_list (+0x1C) and update_list (+0x34).
     * Does NOT touch text_list (+0x04). Used during sprite resets.
     */
    void cleanupTooltips();

    /**
     * Free all tooltip timers — cleanup all three timer lists.
     * Address: 0x423A90
     *
     * Calls vtable+0x18 on all three timers in order: pos_list,
     * update_list, text_list. Called during CGWND_Cleanup.
     */
    void freeTooltipManager();

    /**
     * Per-frame tooltip maintenance — iterate update_list and pos_list,
     * update scroll/anim, remove completed items.
     * Address: 0x423D70
     *
     * NOT actually "hiding" — this is the animation/scroll tick function.
     * Iterates both update_list and pos_list, calls UI_Window_UpdateScroll
     * on each item, removes those that return 1 (completed).
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
     * Reset all tooltips — iterate update_list and pos_list,
     * call vtable[9] on each non-null item.
     * Address: 0x423F80
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
