/**
 * UI_ListBox.cpp — List box implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "UI_ListBox.h"
#include "UIEntity.h"
#include "../shared/collections.h"
#include <new>
#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void* operator_new(size_t size);                    /* 0x465CE0 */
void  GLOBAL_free(void* ptr);                       /* 0x465CD0 */

/* ================================================================== */
/* UI_DrawListBox — Allocate + deep-copy 0xA4-byte DrawContext        */
/* Address: 0x424550                                                    */
/* ================================================================== */
void __thiscall UI_DrawListBox(void* self, int param1, int param2)
{
    /* The binary's staged GameObject -> Entity -> UIEntity vtable writes
       are constructor artifacts.  A copy construction installs the final
       UIEntity vtable while preserving the complete 0xA4-byte snapshot. */
    void* storage = operator_new(sizeof(UIEntity));
    const auto* source = reinterpret_cast<const UIEntity*>(
        static_cast<uintptr_t>(static_cast<uint32_t>(param2)));
    UIEntity* ctx = storage != nullptr
        ? new (storage) UIEntity(*source)
        : nullptr;

    /* Slot 10 is the collection's polymorphic insertion operation. */
    static_cast<Collection*>(self)->InsertAt(param1, ctx);

}

/* ================================================================== */
/* UI_ListBox_FindItem — Binary search for item in sorted list         */
/* Address: 0x424820                                                    */
/* ================================================================== */
uint __thiscall UI_ListBox_FindItem(void* self, int target, uint low, uint high)
{
    SortedCollection* collection = static_cast<SortedCollection*>(self);
    Collection* base = static_cast<Collection*>(collection);

    /* If range > 2 elements, binary search */
    if (high - low > 2U) {
        int mid = static_cast<int>((high - low) >> 1) + static_cast<int>(low);
        void** items = base->items;

        void* target_value = reinterpret_cast<void*>(static_cast<intptr_t>(target));
        int cmp = collection->Comparator(target_value, items[mid]);
        if (cmp < 0) {
            return collection->FindItem(target, low, static_cast<uint>(mid));
        }
        /* target >= items[mid] — search upper half (note: uses target as high bound!) */
        /* BUG: param_1 (target) is passed as the high bound instead of high.
           This means the function searches from mid to target value, not mid to high.
           This is a likely decompilation artifact or intentional narrowing. */
        return collection->FindItem(target, static_cast<uint>(mid),
                                    static_cast<uint>(target));
    }

    /* Linear search for small ranges */
    void** items = base->items;
    uint idx = low;
    while (idx <= high) {
        void* target_value = reinterpret_cast<void*>(static_cast<intptr_t>(target));
        int cmp = collection->Comparator(target_value, items[idx]);
        if (cmp > 0) {
            idx++;
            continue;
        }
        /* cmp <= 0 — found match */
        break;
    }

    /* Verify exact match */
    if (idx <= high &&
        static_cast<int32_t>(reinterpret_cast<intptr_t>(items[idx])) == target) {
        return idx;
    }

    return 0xFFFFFFFF;
}

/* ================================================================== */
/* UI_ListBox_Clear — Reset list box TimerList                         */
/* Address: 0x424A00                                                    */
/* ================================================================== */
void __fastcall UI_ListBox_Clear(void* self)
{
    Collection* collection = static_cast<Collection*>(self);
    if (collection->items != NULL) {
        GLOBAL_free(collection->items);
    }

    /* This is the collection base-destructor/unwind path.  Placement
       construction models the binary's base-vtable installation without
       writing a compiler-specific vtable address. */
    new (collection) Collection();
    collection->items = NULL;
    collection->count = 0;
    collection->capacity = 0;
}
