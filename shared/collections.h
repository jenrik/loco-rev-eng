/**
 * collections.h — Collection, SortedCollection, and Timer base classes
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These classes provide a dynamic array of void* pointers with virtual methods
 * for element access, counting, removal, sorting, and resizing. They are used
 * by BuildingMgr, BuildingComplex, UI_Manager's tooltip lists, and the
 * ScrollBar/ListBox item lists (ui/UI_ScrollBar.cpp, ui/UI_ListBox.cpp) to
 * maintain ordered lists of game objects and timer entries.
 *
 * Class hierarchy:
 *   Collection                  — dynamic array of void*, 16 bytes
 *     ├─ SortedCollection       — sorted-element insertion support
 *     ├─ Timer                  — timer event queue with sorted-ordering check
 *     │  └─ Timer2              — alternate vtable variant
 *     └─ ScrollCollection       — scroll-bar/list-box item list, 0x18 bytes
 *   SortedCollection2           — parallel hierarchy, same layout
 *
 * Size: 0x10 = 16 bytes (Collection/SortedCollection/Timer/Timer2);
 *       0x18 = 24 bytes (ScrollCollection, adds key_offset/key_size).
 *
 * IMPORTANT: These classes use a NON-STANDARD vtable layout in the original
 * binary where slot 0 is Resize, NOT the scalar deleting destructor. In this
 * C++ idiomatic version, we use standard virtual dispatch — the compiler
 * manages vtable ordering. Method names correspond to the original vtable
 * slot functions.
 *
 * FIELD OFFSET NOTE (evidenced 2026-08-09 via disassembly of Timer::Resize
 * @0x435D10, Collection::RemoveAt @0x4356B0, SortedCollection::SetAt
 * @0x435A10, SortedCollection2::SetAt @0x4360B0, UI_Ctor @0x4238C0's
 * sub-object construction, and BuildingComplex::BuildingComplex @0x434500's
 * sub-object construction — all independently agree): the REAL x86 layout is
 * vtable(+0x00), items(+0x04), capacity(+0x08), count(+0x0C). This is
 * OPPOSITE the declaration order below (count declared 2nd, capacity 3rd).
 * The swap is intentionally NOT corrected by reordering the fields: every
 * access in this class hierarchy is symbolic (this->count / this->capacity),
 * never a raw offset, so the physical byte offset the compiler assigns is
 * irrelevant to host-build correctness — only the SEMANTIC ROLE matters, and
 * every method below has been checked against the real semantic role (capacity
 * = written only by Resize(), used only as a growth-trigger bound; count =
 * the populated-element highwater mark, decremented on removal). Exact x86
 * offset parity is a documentation/Windows-reconstruction concern and a
 * non-goal for host builds per CLAUDE.md. Do not reorder the declarations —
 * that would just be churn; instead each field below documents its real x86
 * offset so future RE work isn't misled.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>


// Status: INTEGRATED (see collections.cpp for per-method caveats)
/* Opaque collection entries are known from the binary to have a virtual
 * scalar-deleting destructor in their first dispatch slot. */
struct CollectionElement {
    virtual ~CollectionElement() = default;
};

/* ================================================================== */
/* Collection — dynamic array of void* pointers                        */
/* ================================================================== */

struct Collection {
    void**  items;           /* +0x04  dynamic array of element pointers.
                                 Real x86 offset: +0x04 (agrees with decl order). */
    int32_t count;           /* Real x86 offset: +0x0C (NOT +0x08 — see the
                                 FIELD OFFSET NOTE above). Populated-element
                                 highwater mark; decremented on removal. */
    int32_t capacity;        /* Real x86 offset: +0x08 (NOT +0x0C — see the
                                 FIELD OFFSET NOTE above). Allocated capacity
                                 (slots in items); written only by Resize(). */

    /* ================================================================ */
    /* Virtual methods (standard C++ vtable dispatch)                     */
    /* ================================================================ */

    /**
     * Resize — Grow/shrink the items array (original vtable[0]).
     * Address: 0x435D10 (Timer::Resize)
     */
    virtual void Resize(int32_t newCapacity);

    /**
     * Scalar deleting destructor (original vtable[1]).
     * Frees the items array then optionally frees this memory.
     */
    virtual ~Collection();

    /**
     * InternalExtract — Extract element at index (original vtable[7]).
     * Returns items[index] by default; subclasses may override.
     */
    virtual void* InternalExtract(int32_t index);

    /**
     * InsertAt — Insert/replace an item (original vtable slot 10, offset
     * +0x28 — NOT slot 12/+0x30 as a prior pass of this file claimed).
     * Address: UNIDENTIFIED for a generic/base Collection body — every one
     * of the 7 concrete vtables sampled 2026-08-09 (via a full slot-by-slot
     * get_xrefs_from dump, offsets 0x00-0x54) has its OWN distinct override
     * at +0x28: 0x424170, 0x424290, 0x4246F0, 0x424790, 0x4359A0, and — for
     * the two SortedCollection variants — 0x435A10 / 0x4360B0 (already
     * integrated below as SortedCollection::SetAt / SortedCollection2::SetAt).
     * DrawScrollBar/DrawListBox both dispatch through +0x28 on `this`,
     * confirming the slot, not any specific default body.
     * The addresses a prior pass cited here (0x424010, 0x424760) are real
     * functions but occupy DIFFERENT slots (11 and 12, +0x2C/+0x30) that
     * are uniform-ish across every table sampled — a distinct, out-of-scope
     * pair of virtual methods, not InsertAt. Not conflating them here.
     * This body is a plausible generic default (grow-then-store, consistent
     * with the confirmed capacity/count semantic roles) but is NOT
     * instruction-validated against any specific original address. Do not
     * treat as VALIDATED; do not "fix" it by pattern-matching a sibling.
     */
    virtual int32_t InsertAt(int32_t index, void* item);

    /**
     * FindItem — Search a sorted range (original vtable[16]).
     * Address: 0x424820 for the UI list specialization.
     */
    virtual uint32_t FindItem(int32_t target, uint32_t low, uint32_t high);

    /**
     * Comparator — Compare two elements (original vtable[18]).
     * Returns < 0 if a < b. Subclasses override for custom ordering.
     */
    virtual int32_t Comparator(void* a, void* b);

    /**
     * QuickSortRangeImpl — Recursive quicksort on [left, right] (original vtable[15]).
     * Recurse via this->QuickSortRangeImpl (vtable self-call in original).
     */
    virtual void QuickSortRangeImpl(int32_t left, int32_t right);

    /* ================================================================ */
    /* Removal / bulk-clear virtuals (slots 3/4/5/6)                     */
    /* ================================================================ */

    /**
     * RemoveAt — Sparse removal by index (original vtable slot 3, offset
     * +0x0C = 0x4356B0). Bound-checks index against capacity (real x86
     * offset +0x08 — see FIELD OFFSET NOTE), extracts the element via the
     * virtual InternalExtract() (slot 7), then NULLs the slot. Does not
     * shift or free. Returns the removed element (or nullptr if in-bounds
     * of capacity but the slot was already empty, or out of bounds).
     *
     * Confirmed 2026-08-09 via disassembly of 0x4356B0:
     *   CMP EDI, [ESI+0x8]; JC ...   -> bound is capacity, NOT count.
     * A prior pass of this file had this body checking `this->count`,
     * which is the WRONG semantic field (loosens the bound whenever
     * count < capacity) — fixed in collections.cpp.
     */
    virtual void* RemoveAt(int32_t index);

    /**
     * RemoveElement — Remove-and-destroy the element at index (original
     * vtable slot 4, offset +0x10 = 0x4356E0). Confirmed 2026-08-09 via
     * decode_instructions (no Function object existed at this address —
     * it was reachable only through the vtable, never a direct CALL) and
     * cross-checked as the UNIFORM slot-4 target across all 10 concrete
     * vtables sampled (A/B/C/WRAPPER/TIMER_BASE/TIMER2_BASE and
     * BuildingComplex's two final-stage tables) — no overrides exist
     * anywhere, so this base body is the only body. Real mechanism:
     * calls this->RemoveAt(index) (slot 3), and if the extracted element
     * is non-null, destroys it via its own virtual scalar-deleting
     * destructor with the "also free memory" flag (slot 0, arg 1).
     * Was previously a silent no-op stub; now implemented for real.
     */
    virtual void RemoveElement(int32_t index);

    /**
     * RemoveAll — Bulk-clear without destroying elements (original vtable
     * slot 5, offset +0x14). This is the BASE default body, confirmed
     * 2026-08-09 via decode_instructions at 0x4244F0 (uniform across the
     * 4 base-stage tables sampled: A/C/TIMER_BASE/TIMER2_BASE — no Function
     * object existed there either). Mechanism: NULLs items[0..capacity)
     * directly, without invoking InternalExtract/RemoveAt/any per-element
     * cleanup, and without touching count. ScrollCollection overrides this
     * with a different (count-bounded, RemoveAt-driven) mechanism — see
     * ScrollCollection::RemoveAll below. Original semantic *purpose* of
     * this slot within the class's lifecycle is not confirmed — only the
     * mechanism is; named for the confirmed behavior, not a recovered
     * original identifier (none exists — this address had no prior name
     * anywhere in this codebase).
     */
    virtual void RemoveAll();

    /**
     * DestroyAll — Bulk-clear WITH destruction (original vtable slot 6,
     * offset +0x18). This is the BASE default body: was previously named
     * UI_EnableScrollBar (0x424510) in ui/UI_ScrollBar.cpp, a dispositively
     * wrong name — the function returns void, takes no position/enable
     * parameter, and reads no "enabled" flag anywhere. Real mechanism
     * (confirmed via disassembly): forward sweep of index 0..capacity-1
     * (capacity re-read from `this` after each iteration), calling
     * this->RemoveElement(index) — i.e. RemoveAt(index)-then-destroy —
     * on every slot. ScrollCollection overrides this with a different
     * (count-bounded, tail-draining) mechanism — see
     * ScrollCollection::DestroyAll below.
     */
    virtual void DestroyAll();

    /**
     * Compact — original vtable slot 20, offset +0x50 = 0x4244D0.
     * Confirmed 2026-08-09 via a full non-spillover slot-by-slot dump of
     * BuildingComplex's two final-stage vtables (0x478018, 0x477F88; both
     * verified valid through slot 21) AND the UI ScrollBar/ListBox final
     * vtables (B=0x477B78, WRAPPER=0x477AE8) — all four independently
     * agree on 0x4244D0 at slot 20. Real mechanism: if count > 1, calls
     * this->QuickSortRangeImpl(0, count-1) (slot 15) — i.e. re-sorts the
     * entire populated range. This is NOT a compaction (no gap removal,
     * no shrink) — "Compact" is a pre-existing, dispositively-wrong name
     * by the same standard applied elsewhere in this file, and a more
     * accurate name would be something like Resort(). Kept as `Compact()`
     * here anyway: this specific address was flagged as needing caution
     * (a shorter, non-representative probe of a different table family
     * produced spillover garbage at this same offset earlier in the same
     * investigation), and this file's owner should have final say on the
     * rename rather than it happening as a side effect of an unrelated
     * task. Body is implemented for real; name is intentionally NOT
     * changed — see docs/landmine-sweep-worklist.md for the full trail.
     */
    virtual void Compact();
};

/* ================================================================== */
/* SortedCollection — Collection with sorted ordering                   */
/* ================================================================== */

struct SortedCollection : public Collection {
    uint32_t FindItem(int32_t target, uint32_t low, uint32_t high) override;

    /**
     * SetAt — Set element at sorted index (original vtable slot 10,
     * offset +0x28 = 0x435A10 — NOT slot 12/+0x30; corrected 2026-08-09
     * via a full non-spillover slot dump of BuildingComplex's final-stage
     * vtable at 0x478018, which shows 0x435A10 at +0x28 and a different,
     * uniform function (0x424760) at +0x30). Grows via Resize() if
     * index >= capacity (real offset +0x08 — see FIELD OFFSET NOTE);
     * rejects (returns null) if index > count (real offset +0x0C).
     * A prior pass of this file had these two bound checks swapped
     * (capacity/count reversed) — fixed in collections.cpp.
     */
    void* SetAt(int32_t index, void* element);

    /**
     * QuickSortRange — Recursive Hoare quicksort (original vtable[15] = 0x435AA0).
     * Uses Comparator() for comparisons, recurses via QuickSortRangeImpl().
     */
    void QuickSortRange(int32_t left, int32_t right);
};

/* ================================================================== */
/* SortedCollection2 — Byte-identical twin of SortedCollection          */
/* Uses parallel vtable (original VTBL_SORTED_COLLECTION2 = 0x477FB0).  */
/* ================================================================== */

struct SortedCollection2 : public Collection {
    void* SetAt(int32_t index, void* element);
};

/* ================================================================== */
/* Timer — Timer event queue collection                                */
/* Inherits Collection's vtable interface, adds IsSorted.               */
/* ================================================================== */

struct Timer : public Collection {
    /**
     * Resize — Override for Timer-specific growth logic (0x435D10).
     */
    void Resize(int32_t newCapacity) override;

    /**
     * IsSorted — Check ascending order (0x435CD0).
     * Uses Comparator() to check adjacent pairs, for indices
     * [0, count-2] (real x86 offset +0x0C — see FIELD OFFSET NOTE;
     * confirmed via disassembly: `param_1[3]`, i.e. offset +0x0C, is the
     * loop limit). A prior pass of this file used `this->capacity` here,
     * the wrong semantic field — fixed in collections.cpp.
     */
    bool IsSorted();

    /**
     * Destructor body — Resets to minimal state, frees items.
     * Called by ~Timer() and EH unwind handlers.
     */
    void Destructor();

    ~Timer() override;
};

/* ================================================================== */
/* Timer2 — Timer variant with different base vtable (0x477FE0).        */
/* Identical behavior, separate vtable in original binary.              */
/* ================================================================== */

struct Timer2 : public Collection {
    void Destructor();
    ~Timer2() override;
};

/* ================================================================== */
/* ScrollCollection — item list shared by scroll bars, list boxes, and  */
/* BuildingComplex's timer collections. 0x18 bytes (Collection's 0x10   */
/* plus two int32 fields at +0x10/+0x14).                               */
/*                                                                       */
/* Discovered/confirmed 2026-08-09 while converting the free-function-  */
/* with-explicit-self anti-pattern out of ui/UI_ScrollBar.{h,cpp} and   */
/* ui/UI_ListBox.{h,cpp} (see docs/landmine-sweep-worklist.md).         */
/*                                                                       */
/* Evidence this is a genuine, non-UI-specific shared base (not a       */
/* UI-only helper):                                                     */
/*   - Its slot 3/5/6 overrides (RemoveAt/RemoveAll/DestroyAll below)   */
/*     were found, via a full non-spillover slot-by-slot vtable dump,   */
/*     at IDENTICAL offsets in FOUR unrelated concrete vtables: the two */
/*     UI_Manager sub-object finals (B=0x477B78 "text", WRAPPER=        */
/*     0x477AE8 "pos"/"update", per UI_Ctor @0x4238C0) AND              */
/*     BuildingComplex's two TimerCollection finals (0x478018, 0x477F88,*/
/*     per BuildingComplex::BuildingComplex @0x434500) — NOT touching   */
/*     game/BuildingComplex.cpp itself; read-only cross-check.          */
/*   - The two extra fields are confirmed by construction: in all four  */
/*     ctors, the word immediately after the FINAL vtable install is    */
/*     zeroed twice more (the +0x10/+0x14 pair) before construction     */
/*     continues, matching exactly what SetKey (was UI_FreeScrollBar)   */
/*     writes.                                                          */
/*   - ui/UI_Utils.h's pre-existing (undiscovered-duplicate) UITimerList*/
/*     class independently derived the SAME field layout (items+0x04,   */
/*     capacity+0x08, count+0x0C) and even the SAME two extra fields    */
/*     (named key_offset/key_size there), with UI_Manager's own ctor    */
/*     comment corroborating vtable 0x477BD0->0x477B78 and              */
/*     0x477B40->0x477AE8 exactly as re-derived here independently.     */
/*     UITimerList is NOT unified with ScrollCollection in this change  */
/*     (out of scope — a future cleanup); see the worklist doc.         */
/* ================================================================== */

struct ScrollCollection : public Collection {
    int32_t key_offset;   /* +0x10. Name adopted from ui/UI_Utils.h's
                              UITimerList (same fields, independently
                              named there with corroborating evidence —
                              see class comment above). Purpose beyond
                              "opaque config written by SetKey" not
                              re-confirmed in this pass. */
    int32_t key_size;      /* +0x14. See key_offset. */

    /**
     * RemoveAt override — shift-and-decrement removal (original vtable
     * slot 3 for this class's final-stage vtable). Was UI_HandleScrollMessage
     * (0x4241E0) in ui/UI_ScrollBar.{h,cpp}. Dispatches to the virtual
     * InternalExtract() (slot 7) for the element at `index`; if handled and
     * `index` is not the last populated slot, shifts every later element
     * left by one (memmove-equivalent), then NULLs the last slot and
     * decrements count (real x86 offset +0x0C — see Collection's FIELD
     * OFFSET NOTE). Returns non-null (as a void*, matching Collection's
     * RemoveAt signature) when handled.
     *
     * Renamed from UI_HandleScrollMessage: that name described a
     * plausible-sounding "message handler," but the function takes an
     * index and returns a handled flag with no message/event structure
     * anywhere in its body — it IS the class's RemoveAt, confirmed by
     * vtable slot identity (same slot as Collection::RemoveAt in the
     * sibling base-stage tables A/C/TIMER_BASE/TIMER2_BASE).
     */
    void* RemoveAt(int32_t index) override;

    /**
     * RemoveAll override — count-bounded tail drain via the virtual
     * RemoveAt (slot 3, i.e. THIS class's own override above). Was
     * UI_GetScrollPos (0x424250). Repeatedly calls this->RemoveAt(count-1)
     * until count reaches 0, re-reading count after each call (relies on
     * this class's RemoveAt override to actually decrement it — the base
     * Collection::RemoveAt does NOT, so this override only terminates
     * correctly on a ScrollCollection).
     *
     * Renamed from UI_GetScrollPos: dispositively not a getter — void
     * return, no output parameter, no "position" concept read or written
     * anywhere in the body. Confirmed via disassembly (0x424250) and via
     * a full vtable dump showing it uniformly at slot 5 in all 5 final-
     * stage tables checked, and Collection::RemoveAll's DIFFERENT,
     * non-destructive mechanism at the same slot in the base-stage tables.
     */
    void RemoveAll() override;

    /**
     * DestroyAll override — count-bounded tail drain via RemoveElement
     * (slot 4). Was UI_SetScrollPos (0x424270). Repeatedly calls
     * this->RemoveElement(count-1) until count reaches 0 — unlike
     * RemoveAll above, this destroys each removed element (RemoveElement
     * itself deletes it via its virtual destructor).
     *
     * Renamed from UI_SetScrollPos: dispositively not a setter — void
     * return, no input value parameter (only `this`), no "position"
     * concept anywhere in the body. Confirmed the same way as RemoveAll:
     * disassembly plus uniform slot-6 placement across the same 5
     * final-stage tables where Collection::DestroyAll's DIFFERENT
     * (capacity-bounded, forward-sweep) mechanism sits in the base-stage
     * tables at the identical slot.
     */
    void DestroyAll() override;

    /**
     * SetKey — Store two configuration words, then call this->Compact()
     * (slot 20). Was UI_FreeScrollBar (0x424490). Confirmed via
     * disassembly: writes param1/param2 to key_offset/key_size, then a
     * single virtual call, nothing else — no GLOBAL_free, no delete, no
     * items-array access at all.
     *
     * Renamed from UI_FreeScrollBar: dispositively not a free/cleanup
     * function — it performs zero deallocation. Observed call-site values
     * (read-only cross-check against game/BuildingComplex.cpp and
     * ui/UI_Utils.cpp, neither modified by this change): UI_Manager's
     * update_list passes (0x0C, -4); BuildingComplex's second
     * TimerCollection passes the same (0x0C, -4); BuildingComplex's first
     * passes (0x7C, 10). ui/UI_Utils.cpp's UITimerList sets its own
     * key_offset/key_size to (0x0C, -4) at exactly this call site,
     * corroborating both the field identity and these values independently.
     */
    void SetKey(int32_t param1, int32_t param2);

    /**
     * Destructor body — resets to unconstructed state: frees items if
     * non-null, zeros items/capacity/count. Was UI_InitScrollBar
     * (0x424460) and UI_ListBox_Clear (0x424A00) in the two ui/ files —
     * confirmed via decompilation to be byte-identical EXCEPT for which
     * sibling vtable each one reinstalled (VTBL_TIMERLIST_A vs
     * VTBL_TIMERLIST_C), which is exactly the raw-vtable-write EH-unwind
     * artifact CLAUDE.md requires dropping (C++ manages vtables). With
     * that dropped, both original functions collapse into this one body;
     * both original addresses are cited here rather than picking one,
     * since neither is individually "the" original — they were two call
     * sites of what is, modulo the vtable write, identical logic. Matches
     * the existing Timer::Destructor()/Timer2::Destructor() pattern in
     * this same file.
     */
    void Destructor();

    ~ScrollCollection() override;

    /**
     * DrawScrollBar — Allocate a 0x88-byte Entity, deep-copy from
     * srcContext, install Entity's real (compiler-managed) vtable, then
     * insert it via the virtual InsertAt (slot 10). Was UI_DrawScrollBar
     * (0x424040) in ui/UI_ScrollBar.{h,cpp}.
     *
     * Confirmed (2026-08-09, non-spillover slot dump) to sit at vtable
     * slot 9 in BOTH stages (A=0x477BD0, B=0x477B78) of the "text"
     * sub-object's class. DrawListBox below sits at the SAME slot 9 in
     * the sibling "pos"/"update" sub-object's class (C=0x477B40,
     * WRAPPER=0x477AE8) — i.e. slot 9 is a real shared virtual ("Draw")
     * with two different per-leaf-class bodies. Modeled here as two
     * plainly-named, non-virtual methods on the one ScrollCollection
     * class rather than as competing overrides of a shared virtual,
     * since that would require reconstructing two further leaf
     * subclasses this task's scope does not call for (BuildingComplex's
     * two TimerCollection finals do NOT have Draw at slot 9 at all —
     * confirming Draw is not part of ScrollCollection's own interface,
     * only its UI-side leaf classes').
     */
    void DrawScrollBar(int32_t param1, const void* srcContext);

    /**
     * DrawListBox — Same pattern as DrawScrollBar, but a 0xA4-byte
     * UIEntity context instead of a 0x88-byte Entity. Was UI_DrawListBox
     * (0x424550) in ui/UI_ListBox.{h,cpp}. See DrawScrollBar's comment
     * for the shared-slot-9 evidence.
     */
    void DrawListBox(int32_t param1, const void* srcContext);
};

/* Scalar/vector deleting-destructor slots are compiler-generated from the
 * virtual destructors above; no hand-written wrappers are required. */
