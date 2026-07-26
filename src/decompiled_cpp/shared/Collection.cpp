/**
 * Collection.cpp — Collection and SortedCollection implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the core dynamic array operations used throughout the
 * game: indexed removal (RemoveAt), sorted insertion (SetAt), and
 * recursive quicksort (QuickSortRange).
 *
 * These are NOT virtual methods — they are concrete member functions
 * on the Collection/SortedCollection classes that dispatch sub-
 * operations (Resize, GetAt, Compare, SortRange) through the vtable.
 *
 * The growth formula in SetAt uses a negative double constant at
 * 0x4780a8 (approximately -1.0) to compute: newCapacity = index + 1.
 */

#include "Collection.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

int __cdecl __ftol(void);              /* 0x466D30 — MSVC float-to-int helper */
void* __cdecl operator_new(size_t size); /* 0x465CE0 */
void  __cdecl GLOBAL_free(void* ptr);    /* 0x465CD0 */

/* Growth factor constant at 0x4780a8; see SetAt for usage.
 * Value: approximately -1.0, giving growth formula:
 *   newCapacity = 1 - (int)(index * -1.0) = index + 1
 */
extern const double growth_factor;  /* 0x4780a8 */

/* ================================================================== */
/* Collection::RemoveAt — Remove element by index, NULL the slot       */
/* Address: 0x4356B0                                                   */
/* Size: 43 bytes (19 instructions)                                     */
/*                                                                     */
/* Called indirectly through vtable[0x0C] (slot 3) from:                */
/*   - TimerList vtables (0x477B4C, 0x477BDC, 0x477FEC, 0x47807C)     */
/*   - Other collection-based vtable at 0x4777A4                       */
/*                                                                     */
/* Simple sparse removal: does NOT shift elements or compact the       */
/* array. The NULL-ed slot is a gap that must be handled by the         */
/* caller or by a compaction pass.                                     */
/*                                                                     */
/* @param index  Zero-based index to remove.                           */
/* @return       The element pointer that was in the slot, or NULL     */
/*               if index >= count.                                    */
/* ================================================================== */
void* Collection::RemoveAt(int32_t index)
{
    /* Bounds check: index must be < count */
    if (index >= this->count) {        /* this->count at +0x08 */
        return NULL;
    }

    /* Retrieve the element via virtual GetAt at vtable[0x1C] (slot 7).
     * This dispatches through the vtable so that subclasses (e.g.,
     * Timer) can add instrumentation or locking around element access.
     */
    void* element = this->GetAt(index);

    /* NULL the slot in the items array */
    this->items[index] = NULL;          /* items array at +0x04 */

    return element;
}

/* ================================================================== */
/* SortedCollection::SetAt — Set element at sorted index, grow if     */
/*                           needed, release if occupied               */
/* Address: 0x435A10                                                   */
/* Size: 129 bytes (47 instructions)                                    */
/*                                                                     */
/* Called indirectly through vtable[0x30] (slot 12, offset 0x30 from   */
/* vtable base) from: SortedCollection variant vtables. The base       */
/* Timer vtable at 0x478070 includes this at slot +0x30.               */
/*                                                                     */
/* Growth formula:                                                     */
/*   newCapacity = 1 - (int)(index * growth_factor)                    */
/*   where growth_factor ≈ -1.0, so newCapacity ≈ index + 1            */
/*                                                                     */
/* @param index  Zero-based index to set.                              */
/* @param value  New element pointer to store.                         */
/* @return       The value pointer on success, or NULL if index        */
/*               exceeds capacity (+0x0C).                             */
/* ================================================================== */
void* SortedCollection::SetAt(int32_t index, void* value)
{
    /* Bounds check: index > capacity → fail.
     * Note: index == capacity is ALLOWED (the grow below handles it).
     * The assembly uses JBE (unsigned below-or-equal), so
     * only indices strictly greater than capacity are rejected.
     */
    if (index > this->capacity) {       /* this->capacity at +0x0C */
        return NULL;
    }

    /* If index >= count, grow the array */
    if (index >= this->count) {         /* this->count at +0x08 */
        /*
         * Growth formula:
         *   FILD index (convert to double)
         *   FMUL growth_factor (negative, so result is negative)
         *   __ftol (truncate back to int, negative)
         *   newCap = 1 - (int result) = 1 + |int result|
         *
         * With growth_factor ≈ -1.0:
         *   newCap = 1 + index
         */
        double grown = (double)index * growth_factor;  /* 0x4780a8 ≈ -1.0 */
        int32_t new_capacity = 1 - (int32_t)grown;

        /* Resize via virtual method at vtable[0] (+0x00) */
        this->Resize(new_capacity);
    }

    /* If there is already an element at this index, release it */
    void* old_element = this->items[index];  /* items array at +0x04 */
    if (old_element != NULL) {
        /* The collection owns polymorphic entries. The virtual destructor
         * performs the scalar-deleting-destructor behavior from the binary. */
        struct OwnedElement { virtual ~OwnedElement() = default; };
        delete static_cast<OwnedElement*>(old_element);

        /* Clear the slot (redundant since we assign next, but done) */
        this->items[index] = NULL;
    }

    /* Store the new element pointer */
    this->items[index] = value;

    /* Return the newly stored value (matching MSVC convention) */
    return this->items[index];
}

/* ================================================================== */
/* SortedCollection::QuickSortRange — Hoare quicksort on [left, right] */
/* Address: 0x435AA0                                                   */
/* Size: 177 bytes (84 instructions)                                    */
/*                                                                     */
/* Called indirectly through vtable[0x3C] (slot 15). The slot at       */
/* vtable[0x3C] points to this same function, so it serves as both     */
/* the default SortRange implementation and the recursive dispatch.     */
/*                                                                     */
/* Algorithm: Hoare partition scheme with midpoint pivot.               */
/* Comparator at vtable[0x48] (slot 18) returns:                       */
/*   < 0  if a < pivot                                                  */
/*   0    if a == pivot                                                 */
/*   > 0  if a > pivot                                                  */
/*                                                                     */
/* @param left   Left index (inclusive).                               */
/* @param right  Right index (inclusive).                              */
/* ================================================================== */
void SortedCollection::QuickSortRange(int32_t left, int32_t right)
{
    /*
     * Select pivot as the middle element.
     * Midpoint = (left + right) / 2 using signed arithmetic (SAR for /2).
     */
    int32_t pivot_index = (left + right) / 2;
    void* pivot = this->items[pivot_index];  /* items at +0x04 */

    int32_t j = left;   /* left-side scanning index */
    int32_t i = right;  /* right-side scanning index */

    /* Main Hoare partition loop */
    do {
        /* Scan from the left: find element >= pivot */
        while (this->Compare(this->items[j], pivot) < 0) {
            j++;
        }

        /* Scan from the right: find element <= pivot */
        while (this->Compare(pivot, this->items[i]) < 0) {
            i--;
        }

        /* If pointers crossed, partitioning is complete */
        if (i < j) {
            break;
        }

        /* Swap items[j] and items[i] */
        void* temp = this->items[j];
        this->items[j] = this->items[i];
        this->items[i] = temp;

        /* Advance both pointers toward center */
        j++;
        i--;
    } while (j <= i);

    /* Recurse on left partition */
    if (left < i) {
        this->SortRange(left, i);
    }

    /* Recurse on right partition */
    if (j < right) {
        this->SortRange(j, right);
    }
}
