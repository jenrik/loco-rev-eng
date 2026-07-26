/**
 * UI_ScrollBar.cpp — Scroll bar and list management implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "UI_ScrollBar.h"
#include "../core/Entity.h"
#include "../shared/collections.h"
#include <new>
#include <stdint.h>
#include <cstring>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void* operator_new(size_t size);                    /* 0x465CE0 */
void  GLOBAL_free(void* ptr);                       /* 0x465CD0 */
void  __cdecl CRT_strncpy(void* dst, void* src, int n); /* 0x4660A0 */

/* ================================================================== */
/* UI_DrawScrollBar — Allocate + deep-copy DrawContext, dispatch       */
/* Address: 0x424040                                                    */
/* ================================================================== */
void __thiscall UI_DrawScrollBar(void* self, int param1, int param2)
{
    /* The staged GameObject -> Entity vtable writes in the binary are
       constructor artifacts.  Copy construction preserves the complete
       0x88-byte descriptor and installs Entity's compiler-managed vtable. */
    void* storage = operator_new(sizeof(Entity));
    Entity* ctx = storage != NULL
        ? new (storage) Entity(*(Entity*)(uintptr_t)param2)
        : NULL;

    /* Slot 10 is the collection's polymorphic insertion operation. */
    ((Collection*)self)->InsertAt(param1, ctx);

}

/* ================================================================== */
/* UI_HandleScrollMessage — Remove item at index, shift remaining      */
/* Address: 0x4241E0                                                    */
/* ================================================================== */
int __thiscall UI_HandleScrollMessage(void* self, uint param1)
{
    /* Virtual dispatch to InternalExtract (vtable[7]).
     * Returns void* — non-null means item was handled. */
    int handled = ((Collection*)self)->InternalExtract(param1) ? 1 : 0;

    if (handled != 0) {
        /* Get count at +0x0C */
        int count = *(int*)((uint8_t*)self + 0x0C);

        /* If not the last element, shift items left */
        if (param1 < (uint)count - 1) {
            void** items = *(void***)((uint8_t*)self + 0x04);
            /* memmove items[param1..count-2] = items[param1+1..count-1] */
            int bytesToShift = (count - 1 - (int)param1) * 4;
            CRT_strncpy(
                (void*)((uint8_t*)items + param1 * 4),
                (void*)((uint8_t*)items + (param1 + 1) * 4),
                bytesToShift);
        }

        /* Zero the last slot and decrement count */
        void** items2 = *(void***)((uint8_t*)self + 0x04);
        int count2 = *(int*)((uint8_t*)self + 0x0C);
        items2[count2 - 1] = NULL;
        *(int*)((uint8_t*)self + 0x0C) = count2 - 1;
    }

    return handled;
}

/* ================================================================== */
/* UI_GetScrollPos — Drain items from tail via vtable[3]               */
/* Address: 0x424250                                                    */
/* ================================================================== */
void __fastcall UI_GetScrollPos(void* self)
{
    int* selfInt = (int*)self;

    /* Get count at +0x0C */
    int count = selfInt[3];  /* *(int*)(self + 0x0C) */

    while (count != 0) {
        /* Virtual dispatch to RemoveAt (vtable[3]) */
        ((Collection*)self)->RemoveAt(count - 1);

        /* Re-read count (may have changed) */
        count = selfInt[3];
    }
}

/* ================================================================== */
/* UI_SetScrollPos — Drain items from tail via vtable[4]               */
/* Address: 0x424270                                                    */
/* ================================================================== */
void __fastcall UI_SetScrollPos(void* self)
{
    int* selfInt = (int*)self;

    /* Get count at +0x0C */
    int count = selfInt[3];

    while (count != 0) {
        /* Virtual dispatch to RemoveElement (vtable[4]) */
        ((Collection*)self)->RemoveElement(count - 1);

        /* Re-read count */
        count = selfInt[3];
    }
}

/* ================================================================== */
/* UI_InitScrollBar — Initialize/reset TimerList                       */
/* Address: 0x424460                                                    */
/* ================================================================== */
void __fastcall UI_InitScrollBar(void* self)
{
    Collection* collection = (Collection*)self;
    if (collection->items != NULL) {
        GLOBAL_free(collection->items);
    }

    /* Model the unwind path's base construction without writing a raw
       compiler-specific vtable address. */
    new (collection) Collection();
    collection->items = NULL;
    collection->count = 0;
    collection->capacity = 0;
}

/* ================================================================== */
/* UI_FreeScrollBar — Store params and delegate to vtable[20]          */
/* Address: 0x424490                                                    */
/* ================================================================== */
int __thiscall UI_FreeScrollBar(void* self, int param1, int param2)
{
    /* Store params at +0x10 and +0x14 */
    *(int*)((uint8_t*)self + 0x10) = param1;
    *(int*)((uint8_t*)self + 0x14) = param2;

    /* Virtual dispatch to Compact (vtable[20]) */
    ((Collection*)self)->Compact();

    return 0;
}

/* ================================================================== */
/* UI_EnableScrollBar — Iterate items calling vtable[4] on each       */
/* Address: 0x424510                                                    */
/* ================================================================== */
void __fastcall UI_EnableScrollBar(void* self)
{
    int* selfInt = (int*)self;

    /* Get count at +0x08 */
    int count = selfInt[2];
    uint32_t idx = 0;

    if (count == 0) {
        return;
    }

    do {
        /* Virtual dispatch to RemoveElement (vtable[4]) */
        ((Collection*)self)->RemoveElement(idx);

        idx++;
    } while (idx < (uint32_t)selfInt[2]);   /* compare against updated count at +0x08 */
}
