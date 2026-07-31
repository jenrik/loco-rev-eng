/**
 * collections.h — Collection, SortedCollection, and Timer base classes
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These classes provide a dynamic array of void* pointers with virtual methods
 * for element access, counting, removal, sorting, and resizing. They are used
 * by BuildingMgr, BuildingComplex, and other game managers to maintain ordered
 * lists of game objects and timer entries.
 *
 * Class hierarchy:
 *   Collection                  — dynamic array of void*, 16 bytes
 *     ├─ SortedCollection       — sorted-element insertion support
 *     └─ Timer                  — timer event queue with sorted-ordering check
 *        └─ Timer2              — alternate vtable variant
 *   SortedCollection2           — parallel hierarchy, same layout
 *
 * Size: 0x10 = 16 bytes (all classes).
 *
 * IMPORTANT: These classes use a NON-STANDARD vtable layout in the original
 * binary where slot 0 is Resize, NOT the scalar deleting destructor. In this
 * C++ idiomatic version, we use standard virtual dispatch — the compiler
 * manages vtable ordering. Method names correspond to the original vtable
 * slot functions.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

/* Opaque collection entries are known from the binary to have a virtual
 * scalar-deleting destructor in their first dispatch slot. */
struct CollectionElement {
    virtual ~CollectionElement() = default;
};

/* ================================================================== */
/* Collection — dynamic array of void* pointers                        */
/* ================================================================== */

struct Collection {
    void**  items;           /* +0x04  dynamic array of element pointers         */
    int32_t count;           /* +0x08  number of populated elements              */
    int32_t capacity;        /* +0x0C  allocated capacity (slots in items)       */

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
     * InsertAt — Insert/replace an item (original vtable[10]).
     * Address: variant-specific (for example 0x424010 / 0x424760).
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
    /* Non-virtual methods                                               */
    /* ================================================================ */

    /**
     * RemoveAt — Sparse removal by index (original vtable[3] = 0x4356B0).
     * NULLs the slot, does not shift or free. Returns removed element.
     */
    virtual void* RemoveAt(int32_t index);

    /**
     * RemoveElement — Remove element at index, shifting tail (vtable[4]).
     * Called by scrollbar drain/iteration loops. Differs from RemoveAt
     * in that it processes the element (e.g. cleanup) before removal.
     */
    virtual void RemoveElement(int32_t index);

    /**
     * Compact — Compress/compact the collection after modifications (vtable[20]).
     * Called by scrollbar free/cleanup paths to reclaim sparse slots.
     */
    virtual void Compact();
};

/* ================================================================== */
/* SortedCollection — Collection with sorted ordering                   */
/* ================================================================== */

struct SortedCollection : public Collection {
    uint32_t FindItem(int32_t target, uint32_t low, uint32_t high) override;

    /**
     * SetAt — Set element at sorted index (original vtable[12] = 0x435A10).
     * Grows via Resize() if index >= count. Releases old element.
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
     * Uses Comparator() to check adjacent pairs.
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

/* Scalar/vector deleting-destructor slots are compiler-generated from the
 * virtual destructors above; no hand-written wrappers are required. */
