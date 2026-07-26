/**
 * Collection.h — Templated dynamic array base class and sorted variant
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The game uses a simple dynamic array (Collection) and its sorted variant
 * (SortedCollection) as its primary container types. These are NOT STL
 * containers — they are hand-rolled MSVC-era template classes with a
 * common 16-byte layout.
 *
 * Both classes are used pervasively throughout the codebase via vtables,
 * with Timer, TimerList, and other subsystems deriving from them.
 *
 * Layout (both Collection and SortedCollection, 16 bytes):
 *   +0x00: void*  vtable      — pointer to virtual function table
 *   +0x04: void** items       — dynamic array of void* element pointers
 *   +0x08: int32_t count      — number of elements currently in use
 *   +0x0C: int32_t capacity   — total allocated slot count
 *
 * Key virtual methods (vtable offsets):
 *   [0]  +0x00: Resize         — grow/shrink the items array (e.g., Timer_Resize @ 0x435D10)
 *   [7]  +0x1C: GetAt          — retrieve element pointer by index
 *   [15] +0x3C: SortRange      — sort a subrange (SortedCollection only)
 *   [18] +0x48: Compare        — comparator returning <0 / 0 / >0 (SortedCollection only)
 *
 * Known vtable addresses:
 *   Collection:     0x477B40 (VTBL_TIMERLIST_C), 0x477BD0 (VTBL_TIMERLIST_A),
 *                   0x4777A4, 0x477B4C, 0x477BDC, 0x477FEC
 *   SortedCollection: entries at 0x478040, 0x478054 (within VTBL_TIMER_BASE region),
 *                     0x477BB4, 0x477B24, 0x477FC4
 *   SortedCollection2: 0x477FB0 (duplicate of SortedCollection via different template instantiation)
 */

#pragma once

#include <stdint.h>

/* ================================================================== */
/* Collection — Simple dynamic array of void* pointers                 */
/* Size: 16 bytes                                                      */
/* ================================================================== */

class Collection {
public:
    /* ---- Fields ---- */
    /* vtable at +0x00 is compiler-managed via virtual methods */
    void**   items;           /* +0x04 — dynamic array of void* pointers  */
    int32_t  count;           /* +0x08 — number of elements in use        */
    int32_t  capacity;        /* +0x0C — total allocated slots            */

    /* ---- Virtual dispatch helpers (called through vtable) ---- */

    /**
     * Resize the internal items array to a new capacity.
     * Virtual method at vtable[0] (+0x00).
     *
     * Called when SetAt needs to grow beyond current capacity.
     * Shrinks automatically if trailing elements are NULL.
     *
     * @param new_capacity  Desired number of slots (may be trimmed).
     */
    void Resize(int32_t new_capacity);

    /**
     * Retrieve element pointer at the given index.
     * Virtual method at vtable[7] (+0x1C).
     *
     * Used by RemoveAt and callers to retrieve the element before
     * clearing the slot.
     *
     * @param index  Zero-based element index.
     * @return       Element pointer, or NULL if out of bounds.
     */
    void* GetAt(int32_t index);

    /* ---- Non-virtual methods ---- */

    /**
     * Remove an element by index — sets the slot to NULL.
     * Address: 0x4356B0
     *
     * Does NOT shift remaining elements or free the object memory.
     * Returns the element pointer that was in the slot (caller is
     * responsible for cleanup).
     *
     * @param index  Zero-based index to remove.
     * @return       Previous element pointer, or NULL if index >= count.
     */
    void* RemoveAt(int32_t index);
};

/* ================================================================== */
/* SortedCollection — Collection with sorting and ordered insertion    */
/* Size: 16 bytes (same layout as Collection)                          */
/* ================================================================== */

class SortedCollection : public Collection {
public:
    /* ---- Virtual dispatch helpers (SortedCollection-specific) ---- */

    /**
     * Sort a subrange of the items array using quicksort.
     * Virtual method at vtable[15] (+0x3C).
     *
     * Dispatches to the actual SortedCollection_QuickSortRange
     * implementation. Used for recursive sorting and external calls.
     *
     * @param left   Left index (inclusive).
     * @param right  Right index (inclusive).
     */
    void SortRange(int32_t left, int32_t right);

    /**
     * Compare two element pointers.
     * Virtual method at vtable[18] (+0x48).
     *
     * Abstract comparator. Returns:
     *   < 0  if a < b
     *   0    if a == b
     *   > 0  if a > b
     *
     * @param a  First element pointer.
     * @param b  Second element pointer.
     * @return   Negative, zero, or positive comparison result.
     */
    int32_t Compare(void* a, void* b);

    /* ---- Non-virtual methods ---- */

    /**
     * Set element at a given sorted index.
     * Address: 0x435A10
     *
     * If index >= capacity, returns NULL (failure).
     * If index >= count, grows the array first.
     * If an element already exists at that index, deletes it via
     * its own vtable[0] before replacing.
     *
     * @param index  Zero-based index to set.
     * @param value  New element pointer.
     * @return       The value pointer (or NULL if out of capacity bounds).
     */
    void* SetAt(int32_t index, void* value);

    /**
     * QuickSort a subrange of elements [left, right] inclusive.
     * Address: 0x435AA0
     *
     * Implements Hoare partition quicksort using virtual Compare
     * at vtable[0x48]. Recursion is dispatched through virtual
     * SortRange at vtable[0x3C] (which normally points back to
     * this function, but could be overridden).
     *
     * @param left   Left index (inclusive).
     * @param right  Right index (inclusive).
     */
    void QuickSortRange(int32_t left, int32_t right);
};
