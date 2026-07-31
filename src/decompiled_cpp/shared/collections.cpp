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

#include "collections.h"
#include <cstring>          /* memset, memcpy */

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

void Collection::RemoveElement(int32_t index) {
    /* Base: no-op. Subclasses may override for element cleanup. */
}

void Collection::Compact() {
    /* Base: no-op. Subclasses override to reclaim sparse slots. */
}


/* ================================================================== */
/* Collection::RemoveAt                                                */
/* Address: 0x4356B0                                                   */
/*                                                                     */
/* Sparse removal by index. Dispatches to InternalExtract() (virtual)   */
/* for polymorphic element extraction, then NULLs the slot.             */
/* This was vtable slot 3 (offset 0x0C) in the original binary.        */
/* ================================================================== */
void* Collection::RemoveAt(int32_t index)
{
    if (static_cast<uint32_t>(index) >= static_cast<uint32_t>(this->count)) {
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
/* Address: 0x435A10                                                   */
/*                                                                     */
/* Sets element pointer at the given sorted index. Grows the array     */
/* via Resize() if index >= count. Releases old element via its         */
/* vtable[0] destructor before storing the new pointer.                */
/* ================================================================== */
void* SortedCollection::SetAt(int32_t index, void* element)
{
    if (static_cast<uint32_t>(index) > static_cast<uint32_t>(this->capacity)) {
        return nullptr;
    }

    /* Grow array if index is at or beyond current count */
    if (static_cast<uint32_t>(index) >= static_cast<uint32_t>(this->count)) {
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
/* ================================================================== */
uint32_t SortedCollection::FindItem(int32_t target, uint32_t low, uint32_t high)
{
    if (high - low > 2) {
        uint32_t mid = ((high - low) >> 1) + low;
        int32_t cmp = this->Comparator(reinterpret_cast<void*>(static_cast<intptr_t>(target)), this->items[mid]);
        return cmp < 0 ? this->FindItem(target, low, mid)
                       : this->FindItem(target, mid, high);
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
/* ================================================================== */
bool Timer::IsSorted()
{
    int32_t limit = this->capacity - 1;

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
/* Address: 0x4360B0                                                   */
/*                                                                     */
/* Alternate SortedCollection variant — byte-identical to              */
/* SortedCollection::SetAt (0x435A10) but uses independent vtable.     */
/* ================================================================== */
void* SortedCollection2::SetAt(int32_t index, void* element)
{
    if (static_cast<uint32_t>(index) > static_cast<uint32_t>(this->capacity)) {
        return nullptr;
    }

    if (static_cast<uint32_t>(index) >= static_cast<uint32_t>(this->count)) {
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
