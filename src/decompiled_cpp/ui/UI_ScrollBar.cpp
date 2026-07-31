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

namespace {
/* UI scrollbar objects extend Collection with the two state words used by
 * 0x424490 at original offsets +0x10 and +0x14. */
struct ScrollCollection : Collection {
    int32_t stored_param_10;
    int32_t stored_param_14;
};
}

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
    const auto* source = reinterpret_cast<const Entity*>(
        static_cast<uintptr_t>(static_cast<uint32_t>(param2)));
    Entity* ctx = storage != nullptr
        ? new (storage) Entity(*source)
        : nullptr;

    /* Slot 10 is the collection's polymorphic insertion operation. */
    static_cast<Collection*>(self)->InsertAt(param1, ctx);

}

/* ================================================================== */
/* UI_HandleScrollMessage — Remove item at index, shift remaining      */
/* Address: 0x4241E0                                                    */
/* ================================================================== */
int __thiscall UI_HandleScrollMessage(void* self, uint param1)
{
    /* Virtual dispatch to InternalExtract (vtable[7]).
     * Returns void* — non-null means item was handled. */
    Collection* collection = static_cast<Collection*>(self);
    int handled = collection->InternalExtract(param1) ? 1 : 0;

    if (handled != 0) {
        /* Get count at +0x0C */
        int count = collection->count;

        /* If not the last element, shift items left */
        if (param1 < static_cast<uint>(count) - 1U) {
            void** items = collection->items;
            /* memmove items[param1..count-2] = items[param1+1..count-1] */
            int bytesToShift = (count - 1 - static_cast<int>(param1)) * 4;
            CRT_strncpy(items + param1, items + param1 + 1, bytesToShift);
        }

        /* Zero the last slot and decrement count */
        void** items = collection->items;
        int count_after_shift = collection->count;
        items[count_after_shift - 1] = nullptr;
        collection->count = count_after_shift - 1;
    }

    return handled;
}

/* ================================================================== */
/* UI_GetScrollPos — Drain items from tail via vtable[3]               */
/* Address: 0x424250                                                    */
/* ================================================================== */
void __fastcall UI_GetScrollPos(void* self)
{
    Collection* collection = static_cast<Collection*>(self);

    /* Get count at +0x0C */
    int count = collection->count;

    while (count != 0) {
        /* Virtual dispatch to RemoveAt (vtable[3]) */
        collection->RemoveAt(count - 1);

        /* Re-read count (may have changed) */
        count = collection->count;
    }
}

/* ================================================================== */
/* UI_SetScrollPos — Drain items from tail via vtable[4]               */
/* Address: 0x424270                                                    */
/* ================================================================== */
void __fastcall UI_SetScrollPos(void* self)
{
    Collection* collection = static_cast<Collection*>(self);

    /* Get count at +0x0C */
    int count = collection->count;

    while (count != 0) {
        /* Virtual dispatch to RemoveElement (vtable[4]) */
        collection->RemoveElement(count - 1);

        /* Re-read count */
        count = collection->count;
    }
}

/* ================================================================== */
/* UI_InitScrollBar — Initialize/reset TimerList                       */
/* Address: 0x424460                                                    */
/* ================================================================== */
void __fastcall UI_InitScrollBar(void* self)
{
    Collection* collection = static_cast<Collection*>(self);
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
    ScrollCollection* collection = static_cast<ScrollCollection*>(self);
    collection->stored_param_10 = param1;
    collection->stored_param_14 = param2;

    /* Virtual dispatch to Compact (vtable[20]) */
    collection->Compact();

    return 0;
}

/* ================================================================== */
/* UI_EnableScrollBar — Iterate items calling vtable[4] on each       */
/* Address: 0x424510                                                    */
/* ================================================================== */
void __fastcall UI_EnableScrollBar(void* self)
{
    Collection* collection = static_cast<Collection*>(self);

    /* Get count at +0x08 */
    int count = collection->count;
    uint32_t idx = 0;

    if (count == 0) {
        return;
    }

    do {
        /* Virtual dispatch to RemoveElement (vtable[4]) */
        collection->RemoveElement(static_cast<int32_t>(idx));

        idx++;
    } while (idx < static_cast<uint32_t>(collection->count));   /* compare against updated count at +0x08 */
}
