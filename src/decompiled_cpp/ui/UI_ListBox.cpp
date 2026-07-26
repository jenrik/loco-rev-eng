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
    UIEntity* ctx = storage != NULL
        ? new (storage) UIEntity(*(UIEntity*)(uintptr_t)param2)
        : NULL;

    /* Slot 10 is the collection's polymorphic insertion operation. */
    ((Collection*)self)->InsertAt(param1, ctx);

}

/* ================================================================== */
/* UI_ListBox_FindItem — Binary search for item in sorted list         */
/* Address: 0x424820                                                    */
/* ================================================================== */
uint __thiscall UI_ListBox_FindItem(void* self, int target, uint low, uint high)
{
    SortedCollection* collection = (SortedCollection*)self;

    /* If range > 2 elements, binary search */
    if ((uint)(high - low) > 2) {
        int mid = ((int)(high - low) >> 1) + (int)low;
        int** items = *(int***)((uint8_t*)self + 0x04);

        int cmp = collection->Comparator((void*)(intptr_t)target, (void*)items[mid]);
        if (cmp < 0) {
            return collection->FindItem(target, low, (uint)mid);
        }
        /* target >= items[mid] — search upper half (note: uses target as high bound!) */
        /* BUG: param_1 (target) is passed as the high bound instead of high.
           This means the function searches from mid to target value, not mid to high.
           This is a likely decompilation artifact or intentional narrowing. */
        return collection->FindItem(target, (uint)mid, (uint)target);
    }

    /* Linear search for small ranges */
    int** items = *(int***)((uint8_t*)self + 0x04);
    uint idx = low;
    while (idx <= high) {
        int cmp = collection->Comparator((void*)(intptr_t)target, (void*)items[idx]);
        if (cmp > 0) {
            idx++;
            continue;
        }
        /* cmp <= 0 — found match */
        break;
    }

    /* Verify exact match */
    if (idx <= high && (int)(intptr_t)items[idx] == target) {
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
    Collection* collection = (Collection*)self;
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
