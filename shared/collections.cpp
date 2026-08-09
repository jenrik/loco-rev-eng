/**
 * collections.cpp — Collection, SortedCollection, and Timer implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These collection classes provide a dynamic array of void* pointers with
 * virtual methods for element access, counting, removal, sorting, and
 * resizing. They are used by BuildingMgr, BuildingComplex, and other game
 * managers to maintain ordered lists of game objects and timer entries.
 *
 * Uses idiomatic C++ virtual dispatch instead of calling function pointers
 * from compiler-managed dispatch tables.
 */

// Status: INTEGRATED (Collection/SortedCollection/SortedCollection2/Timer/
// Timer2 core; ScrollCollection new 2026-08-09). Collection::InsertAt's
// body is unverified against a specific address (see its header comment) —
// do not treat that one method as VALIDATED.

#include "collections.h"
#include "../core/Entity.h"
#include "../ui/UIEntity.h"
#include <cstring>          /* memset, memcpy */
#include <new>

/* ================================================================== */
/* External functions and globals                                      */
/* ================================================================== */

void* operator_new(size_t size);                    /* 0x465CE0 — malloc wrapper */
void  GLOBAL_free(void* ptr);                       /* 0x465CD0 — free wrapper   */
long  __ftol(double val);                           /* 0x466D30 — float-to-long  */

/* Double constant: -1.0 used in SetAt growth formula @ 0x4780A8 */
static const double GROWTH_NEG_ONE = -1.0;


/* ================================================================== */
/* Collection default virtual methods                                   */
/* ================================================================== */

Collection::~Collection() {}

void Collection::Resize(int32_t newCapacity) {
    /* Base: simple no-op. Timer overrides with full implementation. */
}

void* Collection::InternalExtract(int32_t index) {
    /* Base: return items[index].
     * Subclasses may override for custom extraction logic. */
    if (static_cast<uint32_t>(index) >= static_cast<uint32_t>(this->count)) return nullptr;
    return this->items[index];
}

int32_t Collection::InsertAt(int32_t index, void* item) {
    if (index < 0) return 0;
    if (index >= this->capacity) this->Resize(index + 1);
    if (index >= this->capacity || this->items == nullptr) return 0;
    this->items[index] = item;
    if (index >= this->count) this->count = index + 1;
    return 1;
}

uint32_t Collection::FindItem(int32_t, uint32_t, uint32_t) {
    return UINT32_MAX;
}

int32_t Collection::Comparator(void* a, void* b) {
    /* Base: identity comparison by pointer. Subclasses override. */
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

void Collection::QuickSortRangeImpl(int32_t left, int32_t right) {
    /* Base: no-op. SortedCollection overrides. */
}

/* ================================================================== */
/* Collection::RemoveElement                                           */
/* Address: 0x4356E0                                                   */
/*                                                                     */
/* Remove-and-destroy: extracts the element at index via RemoveAt()    */
/* (slot 3), then if non-null, destroys it via its own virtual scalar- */
/* deleting destructor (slot 0, arg 1 = "also free memory"). Uniform   */
/* across all 10 concrete vtables sampled 2026-08-09 — no overrides    */
/* exist. Was a silent no-op stub; implemented for real here.          */
/* ================================================================== */
void Collection::RemoveElement(int32_t index) {
    void* element = this->RemoveAt(index);
    if (element != nullptr) {
        delete static_cast<CollectionElement*>(element);
    }
}

/* ================================================================== */
/* Collection::RemoveAll                                               */
/* Address: 0x4244F0                                                   */
/*                                                                     */
/* Base-stage default for vtable slot 5 (offset +0x14). NULLs          */
/* items[0..capacity) directly — no InternalExtract/RemoveAt dispatch, */
/* no cleanup, and count is left untouched. ScrollCollection::RemoveAll */
/* overrides this with a different, count-bounded, RemoveAt-driven     */
/* mechanism (was UI_GetScrollPos).                                    */
/* ================================================================== */
void Collection::RemoveAll() {
    for (int32_t i = 0; i < this->capacity; ++i) {
        this->items[i] = nullptr;
    }
}

/* ================================================================== */
/* Collection::DestroyAll                                              */
/* Address: 0x424510 (was UI_EnableScrollBar in ui/UI_ScrollBar.cpp —  */
/* dispositively not an "enable" function: void return, no enable      */
/* flag, no position parameter anywhere in the body.)                  */
/*                                                                     */
/* Base-stage default for vtable slot 6 (offset +0x18). Forward sweep  */
/* of index 0..capacity-1 (capacity re-read from `this` after each     */
/* iteration, matching the original's re-load of [ESI+8] inside the    */
/* loop), calling this->RemoveElement(index) — extract-then-destroy —  */
/* on every slot. ScrollCollection::DestroyAll overrides this with a   */
/* different, count-bounded, tail-draining mechanism (was               */
/* UI_SetScrollPos).                                                    */
/* ================================================================== */
void Collection::DestroyAll() {
    if (this->capacity == 0) {
        return;
    }
    uint32_t idx = 0;
    do {
        this->RemoveElement(static_cast<int32_t>(idx));
        idx++;
    } while (idx < static_cast<uint32_t>(this->capacity));
}

/* ================================================================== */
/* Collection::Compact                                                 */
/* Address: 0x4244D0                                                   */
/*                                                                     */
/* Confirmed via a full non-spillover slot-by-slot vtable dump (see    */
/* collections.h's Compact doc comment for the four independent tables */
/* that agree). Mechanism: if count > 1, re-sorts the whole populated  */
/* range via this->QuickSortRangeImpl(0, count-1) (slot 15). Name is   */
/* intentionally NOT changed to something sort-related despite the     */
/* mechanism not matching "compact" — see the header comment for why.  */
/* ================================================================== */
void Collection::Compact() {
    if (this->count > 1) {
        this->QuickSortRangeImpl(0, this->count - 1);
    }
}


/* ================================================================== */
/* Collection::RemoveAt                                                */
/* Address: 0x4356B0                                                   */
/*                                                                     */
/* Sparse removal by index. Dispatches to InternalExtract() (virtual)   */
/* for polymorphic element extraction, then NULLs the slot.             */
/* This was vtable slot 3 (offset 0x0C) in the original binary.        */
/*                                                                     */
/* Bound is CAPACITY, not count — confirmed via disassembly:           */
/*   CMP EDI, [ESI+0x8]; JC ...  (real x86 offset +0x08 = capacity;    */
/* see Collection's FIELD OFFSET NOTE in collections.h). A prior pass   */
/* of this file checked `this->count` here, which loosens the bound     */
/* whenever count < capacity — fixed 2026-08-09.                       */
/* ================================================================== */
void* Collection::RemoveAt(int32_t index)
{
    if (static_cast<uint32_t>(index) >= static_cast<uint32_t>(this->capacity)) {
        return nullptr;
    }

    /* Virtual dispatch to InternalExtract (original vtable[7]). */
    void* element = this->InternalExtract(index);

    /* Null the slot — sparse removal, no shift */
    this->items[index] = nullptr;

    return element;
}


/* ================================================================== */
/* SortedCollection::SetAt                                             */
/* Address: 0x435A10 (original vtable slot 10, offset +0x28 — NOT slot */
/* 12/+0x30; see collections.h's comment on this method for the        */
/* correction).                                                        */
/*                                                                     */
/* Sets element pointer at the given sorted index. Grows the array     */
/* via Resize() if index >= capacity. Rejects (returns null) if        */
/* index > count. Releases old element via its vtable[0] destructor     */
/* before storing the new pointer.                                     */
/*                                                                     */
/* The two bound checks below were SWAPPED in a prior pass of this      */
/* file (checked count for the outer reject, capacity for the growth   */
/* trigger) — confirmed backwards via disassembly:                     */
/*   CMP [this+0xC], param_1; JB reject   -> outer bound is COUNT      */
/*     (real x86 offset +0x0C)                                          */
/*   CMP param_1, [this+0x8]; JAE grow    -> growth trigger is CAPACITY */
/*     (real x86 offset +0x08)                                          */
/* Fixed 2026-08-09. See Collection's FIELD OFFSET NOTE in              */
/* collections.h for why the swap doesn't change host-build behavior    */
/* until a call site's ARGUMENT ordering is what breaks — the swap in    */
/* the DECLARATION order is harmless, but swapping the bound each        */
/* check reads against IS a real behavioral bug (now fixed).            */
/* ================================================================== */
void* SortedCollection::SetAt(int32_t index, void* element)
{
    if (static_cast<uint32_t>(index) > static_cast<uint32_t>(this->count)) {
        return nullptr;
    }

    /* Grow array if index is at or beyond current capacity */
    if (static_cast<uint32_t>(index) >= static_cast<uint32_t>(this->capacity)) {
        /* Growth formula: newCapacity = 1 - (int)(index * -1.0)
           The double -1.0 at 0x4780A8 inverts the sign, then __ftol
           truncates to int. Result: newCapacity = index + 1. */
        long fltResult = __ftol(static_cast<double>(index) * GROWTH_NEG_ONE);
        int32_t newCap = 1 - static_cast<int>(fltResult);   /* = 1 - (-index) = index + 1 */

        /* Virtual dispatch to Resize (original vtable[0]). */
        this->Resize(newCap);
    }

    /* Existing entries are polymorphic C++ objects; virtual destruction
     * replaces the binary's scalar-deleting-destructor dispatch. */
    void* oldElem = this->items[index];
    if (oldElem != nullptr) {
        delete static_cast<CollectionElement*>(oldElem);
        this->items[index] = nullptr;
    }

    /* Store the new element */
    this->items[index] = element;
    return element;
}


/* ================================================================== */
/* SortedCollection::FindItem                                          */
/* Address: 0x424820                                                   */
/*                                                                     */
/* The cmp>=0 recursive branch passes `target` itself as the new high  */
/* bound, NOT `high` — confirmed via disassembly:                      */
/*   (**(code**)(*(int*)this + 0x40))(param_1,iVar1,param_1)           */
/*                                    ^target,^mid,      ^target       */
/* This looks like a bug (and is flagged as such by the same-address    */
/* free-function transcription this replaces, ui/UI_ListBox.cpp's       */
/* UI_ListBox_FindItem, which reproduced it faithfully with a BUG       */
/* comment) but per CLAUDE.md "do not simplify assembly unless          */
/* equivalence is proven" it is preserved as-is. A prior pass of this   */
/* file had silently "fixed" it to `high` — reverted 2026-08-09 so the  */
/* one surviving integration matches the original instead of the        */
/* now-deleted duplicate.                                               */
/* ================================================================== */
uint32_t SortedCollection::FindItem(int32_t target, uint32_t low, uint32_t high)
{
    if (high - low > 2) {
        uint32_t mid = ((high - low) >> 1) + low;
        int32_t cmp = this->Comparator(reinterpret_cast<void*>(static_cast<intptr_t>(target)), this->items[mid]);
        if (cmp < 0) {
            return this->FindItem(target, low, mid);
        }
        /* Reproduces the original's high bound (target itself, not
         * `high`) verbatim — see comment above. */
        return this->FindItem(target, mid, static_cast<uint32_t>(target));
    }

    for (; low <= high; ++low) {
        int32_t cmp = this->Comparator(reinterpret_cast<void*>(static_cast<intptr_t>(target)), this->items[low]);
        if (cmp <= 0) return cmp == 0 ? low : UINT32_MAX;
    }
    return UINT32_MAX;
}

/* ================================================================== */
/* SortedCollection::QuickSortRange                                    */
/* Address: 0x435AA0                                                   */
/*                                                                     */
/* Recursive Hoare quicksort. Uses Comparator() (virtual) for           */
/* comparisons. Recurses via QuickSortRangeImpl() (virtual).           */
/* ================================================================== */
void SortedCollection::QuickSortRange(int32_t left, int32_t right)
{
    /* Pick pivot: midpoint element using signed arithmetic mean */
    int32_t pivotIndex = (left + right) / 2;
    void* pivot = this->items[pivotIndex];

    int32_t i = left;
    int32_t j = right;

    /* Hoare partition scheme */
    do {
        /* Scan left-to-right: find element >= pivot.
         * Uses Comparator (original vtable[18]). */
        while (this->Comparator(this->items[i], pivot) < 0) {
            i++;
        }

        /* Scan right-to-left: find element <= pivot */
        while (this->Comparator(pivot, this->items[j]) < 0) {
            j--;
        }

        /* If indices crossed, we're done partitioning */
        if (j < i) break;

        /* Swap items[i] and items[j].
           NOTE: Assembly at 0x435B11 does INC EDI before the
           [EAX+EDI*4-4] access, making the effective index i, not i-1:
             MOV EDX, [EAX + EBX*4]           ; items[j]
             INC EDI                           ; i++
             MOV ECX, [EAX + EDI*4 - 0x4]     ; items[i] (EDI was already incremented)
             MOV [EAX + EDI*4 - 0x4], EDX     ; items[i] = items[j]
             MOV [EAX + EBX*4], ECX           ; items[j] = items[i] */
        void* tmp = this->items[i];
        this->items[i] = this->items[j];
        this->items[j] = tmp;

        i++;
        j--;
    } while (i <= j);

    /* Recursively sort left and right partitions.
     * Via QuickSortRangeImpl (original vtable[15] self-call). */
    if (left < j) {
        this->QuickSortRangeImpl(left, j);
    }
    if (i < right) {
        this->QuickSortRangeImpl(i, right);
    }
}


/* ================================================================== */
/* Timer::Resize                                                       */
/* Address: 0x435D10                                                   */
/*                                                                     */
/* Algorithm:                                                          */
/*   1. When shrinking, trim trailing NULLs from the end.              */
/*   2. Allocate new items array via operator_new. Zero-fill.          */
/*   3. Copy old items into new array.                                 */
/*   4. Free old array via GLOBAL_free.                                */
/*   5. Update capacity field.                                         */
/* ================================================================== */
void Timer::Resize(int32_t newCapacity)
{
    int32_t cap = this->capacity;

    /* Step 1: When shrinking, trim trailing NULL entries */
    if (newCapacity < cap) {
        void** p = &this->items[cap - 1];
        while (newCapacity < cap) {
            if (*p != nullptr) break;
            cap--;
            p--;
        }
    }

    void* oldItems = this->items;

    /* Step 2: Allocate new array */
    if (cap > 0) {
        void* newItems = operator_new(cap * 4);      /* cap * sizeof(void*) */
        this->items = static_cast<void**>(newItems);
        memset(newItems, 0, cap * 4);
    }

    /* Step 3+4: Copy old items and free old array */
    if (oldItems != nullptr) {
        if (cap > 0) {
            int32_t copyCount = this->capacity;
            if (cap < this->capacity) {
                copyCount = cap;
            }
            if (copyCount > 0) {
                memcpy(this->items, oldItems, copyCount * 4);
            }
        }
        GLOBAL_free(oldItems);
    }

    /* Step 5: Update capacity */
    if (this->items != nullptr) {
        this->capacity = cap;
    } else {
        this->capacity = 0;
        this->items = nullptr;
    }
}


/* ================================================================== */
/* Timer::IsSorted                                                     */
/* Address: 0x435CD0                                                   */
/*                                                                     */
/* Checks ascending order using Comparator(). Returns true for 0/1     */
/* elements.                                                           */
/*                                                                     */
/* Limit is COUNT, not capacity — confirmed via disassembly:            */
/* `iVar1 = param_1[3];` i.e. real x86 offset +0x0C (see Collection's   */
/* FIELD OFFSET NOTE in collections.h). A prior pass of this file used  */
/* `this->capacity` here — fixed 2026-08-09.                            */
/* ================================================================== */
bool Timer::IsSorted()
{
    int32_t limit = this->count - 1;

    if (limit <= 0) return true;

    int32_t cmpResult = -1;
    int32_t i = 0;

    while (cmpResult < 0 && i < limit) {
        /* Virtual dispatch to Comparator (original vtable[18]). */
        cmpResult = this->Comparator(
            this->items[i],
            this->items[i + 1]);
        i++;
    }

    return cmpResult < 0;
}


/* ================================================================== */
/* Timer::Destructor                                                   */
/* Address: 0x435CA0                                                   */
/*                                                                     */
/* The original binary narrows the vtable during destruction; C++ emits  */
/* the compiler-managed destructor slots and requires no manual vtable write. */
/* ================================================================== */
void Timer::Destructor()
{
    this->capacity = 0;
    this->count = 0;

    if (this->items != nullptr) {
        GLOBAL_free(this->items);
    }

    this->items = nullptr;
}


/* ================================================================== */
/* Timer::~Timer — Scalar deleting destructor                          */
/* Address: 0x436360 (compiler-generated Timer destructor slot)       */
/* ================================================================== */
Timer::~Timer()
{
    this->Destructor();
}


/* ================================================================== */
/* SortedCollection2::SetAt                                            */
/* Address: 0x4360B0 (original vtable slot 10, offset +0x28 — same      */
/* correction as SortedCollection::SetAt above).                        */
/*                                                                     */
/* Alternate SortedCollection variant — byte-identical to              */
/* SortedCollection::SetAt (0x435A10) but uses independent vtable.     */
/* Same count/capacity bound swap fixed here as in SortedCollection::  */
/* SetAt above — see that method's comment for the disassembly          */
/* evidence (identical instruction pattern at this address).            */
/* ================================================================== */
void* SortedCollection2::SetAt(int32_t index, void* element)
{
    if (static_cast<uint32_t>(index) > static_cast<uint32_t>(this->count)) {
        return nullptr;
    }

    if (static_cast<uint32_t>(index) >= static_cast<uint32_t>(this->capacity)) {
        long fltResult = __ftol(static_cast<double>(index) * GROWTH_NEG_ONE);
        int32_t newCap = 1 - static_cast<int>(fltResult);   /* = index + 1 */
        this->Resize(newCap);
    }

    void* oldElem = this->items[index];
    if (oldElem != nullptr) {
        delete static_cast<CollectionElement*>(oldElem);
        this->items[index] = nullptr;
    }

    this->items[index] = element;
    return element;
}


/* ================================================================== */
/* Timer2::Destructor                                                  */
/* Address: 0x436280                                                   */
/* ================================================================== */
void Timer2::Destructor()
{
    this->capacity = 0;
    this->count = 0;

    if (this->items != nullptr) {
        GLOBAL_free(this->items);
    }

    this->items = nullptr;
}

Timer2::~Timer2()
{
    this->Destructor();
}


/* Timer and Timer2 destruction is emitted by their C++ destructors above.
 * The original scalar-deleting slots are compiler-generated ABI helpers and
 * are intentionally not reimplemented here. */


/* ================================================================== */
/* ScrollCollection — see collections.h for the full evidence trail    */
/* (vtable slot dump, UI_Ctor/BuildingComplex construction evidence,    */
/* cross-reference with ui/UI_Utils.h's UITimerList).                   */
/* ================================================================== */

/* ================================================================== */
/* ScrollCollection::RemoveAt                                          */
/* Address: 0x4241E0 (was UI_HandleScrollMessage in                    */
/* ui/UI_ScrollBar.{h,cpp})                                             */
/*                                                                     */
/* Shift-and-decrement removal. Dispatches to the virtual               */
/* InternalExtract() (slot 7) for the element at `index`; if handled     */
/* and index is not the last populated slot, shifts every later          */
/* element left by one, then NULLs the last slot and decrements count.  */
/* ================================================================== */
void* ScrollCollection::RemoveAt(int32_t index)
{
    void* handled = this->InternalExtract(index);
    if (handled != nullptr) {
        uint32_t idx = static_cast<uint32_t>(index);
        uint32_t countBefore = static_cast<uint32_t>(this->count);

        if (idx < countBefore - 1U) {
            /* Shift items[idx+1 .. count-1] left by one slot. */
            std::memmove(&this->items[idx], &this->items[idx + 1],
                         static_cast<size_t>(countBefore - 1U - idx) * sizeof(void*));
        }

        uint32_t countAfterShift = static_cast<uint32_t>(this->count);
        this->items[countAfterShift - 1U] = nullptr;
        this->count = static_cast<int32_t>(countAfterShift - 1U);
    }
    return handled;
}

/* ================================================================== */
/* ScrollCollection::RemoveAll                                         */
/* Address: 0x424250 (was UI_GetScrollPos in ui/UI_ScrollBar.{h,cpp})  */
/*                                                                     */
/* Count-bounded tail drain via the virtual RemoveAt (this class's own  */
/* override above, which decrements count — unlike the base            */
/* Collection::RemoveAt).                                               */
/* ================================================================== */
void ScrollCollection::RemoveAll()
{
    int32_t remaining = this->count;
    while (remaining != 0) {
        this->RemoveAt(remaining - 1);
        remaining = this->count;
    }
}

/* ================================================================== */
/* ScrollCollection::DestroyAll                                        */
/* Address: 0x424270 (was UI_SetScrollPos in ui/UI_ScrollBar.{h,cpp})  */
/*                                                                     */
/* Count-bounded tail drain via RemoveElement (slot 4) — destroys each  */
/* removed element (RemoveElement itself deletes it).                   */
/* ================================================================== */
void ScrollCollection::DestroyAll()
{
    int32_t remaining = this->count;
    while (remaining != 0) {
        this->RemoveElement(remaining - 1);
        remaining = this->count;
    }
}

/* ================================================================== */
/* ScrollCollection::SetKey                                            */
/* Address: 0x424490 (was UI_FreeScrollBar in ui/UI_ScrollBar.{h,cpp}) */
/*                                                                     */
/* Stores two configuration words, then delegates to this->Compact()    */
/* (slot 20). See collections.h's SetKey doc comment for the observed   */
/* call-site values cross-referenced against ui/UI_Utils.cpp (read-only, */
/* not modified by this change).                                        */
/* ================================================================== */
void ScrollCollection::SetKey(int32_t param1, int32_t param2)
{
    this->key_offset = param1;
    this->key_size = param2;
    this->Compact();
}

/* ================================================================== */
/* ScrollCollection::Destructor                                        */
/* Addresses: 0x424460 (was UI_InitScrollBar) and 0x424A00 (was         */
/* UI_ListBox_Clear) — see collections.h's Destructor doc comment for   */
/* why the two original functions collapse into this one body.         */
/* ================================================================== */
void ScrollCollection::Destructor()
{
    if (this->items != nullptr) {
        GLOBAL_free(this->items);
    }
    this->items = nullptr;
    this->capacity = 0;
    this->count = 0;
}

ScrollCollection::~ScrollCollection()
{
    this->Destructor();
}

/* ================================================================== */
/* ScrollCollection::DrawScrollBar                                     */
/* Address: 0x424040 (was UI_DrawScrollBar in ui/UI_ScrollBar.{h,cpp}) */
/*                                                                     */
/* Allocates a 0x88-byte Entity, deep-copies srcContext into it via     */
/* copy construction (installing Entity's real, compiler-managed        */
/* vtable — the original's staged GameObject->Entity raw vtable writes  */
/* were construction artifacts), then inserts it through the virtual    */
/* InsertAt (slot 10).                                                  */
/* ================================================================== */
void ScrollCollection::DrawScrollBar(int32_t param1, const void* srcContext)
{
    void* storage = operator_new(sizeof(Entity));
    const auto* source = static_cast<const Entity*>(srcContext);
    Entity* ctx = storage != nullptr
        ? new (storage) Entity(*source)
        : nullptr;

    this->InsertAt(param1, ctx);
}

/* ================================================================== */
/* ScrollCollection::DrawListBox                                       */
/* Address: 0x424550 (was UI_DrawListBox in ui/UI_ListBox.{h,cpp})     */
/*                                                                     */
/* Same pattern as DrawScrollBar, but a 0xA4-byte UIEntity context      */
/* instead of a 0x88-byte Entity.                                       */
/* ================================================================== */
void ScrollCollection::DrawListBox(int32_t param1, const void* srcContext)
{
    void* storage = operator_new(sizeof(UIEntity));
    const auto* source = static_cast<const UIEntity*>(srcContext);
    UIEntity* ctx = storage != nullptr
        ? new (storage) UIEntity(*source)
        : nullptr;

    this->InsertAt(param1, ctx);
}
