/**
 * ui_scroll_list.c — ScrollBar and ListBox functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions operate on TimerList/ScrollBar structures — 0x18-byte
 * dynamic arrays with vtable dispatch for resize, access, and iteration.
 *
 * TimerList layout (0x18 bytes):
 *   +0x00: vtable (VTBL_TIMERLIST_A/B/C variants)
 *   +0x04: data_ptr (allocated array of int-sized entries)
 *   +0x08: capacity (max entries)
 *   +0x0C: count (current entry count)
 *   +0x10: (unknown)
 *   +0x14: (unknown)
 *
 * ListBox uses a 0xA4-byte descriptor (deep-copied from stack) with
 * three vtables set in sequence (0x477820 -> 0x477488 -> 0x477A90).
 */

#include <stdint.h>
#include "../shared/types.h"
#include "../shared/collections.h"
#include "../shared/vtable_addrs.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void  __cdecl GLOBAL_free(void* ptr);                    /* 0x465CD0 */
extern void* __cdecl operator_new(size_t size);                 /* 0x465CE0 */
extern int   __fastcall Timer_Resize(void* timer, int count);   /* 0x435D10 */
extern void* __fastcall CRT_strncpy(void* dst, void* src, int n); /* CRT helper */

/* ================================================================== */
/* UI_InitScrollBar — Initialize/reset a TimerList (ScrollBar variant)  */
/* Address: 0x424460                                                   */
/*                                                                     */
/* Sets vtable to VTBL_TIMERLIST_A, zeros count and capacity,          */
/* frees existing items array. Used as SEH unwind handler and from     */
/* ListBox constructor wrappers.                                       */
/* ================================================================== */
void __fastcall UI_InitScrollBar(void* timerList)
{
    int** ptr = (int**)timerList;

    ptr[3] = 0;                              /* +0x0C = count */
    *ptr = (int*)VTBL_TIMERLIST_A;           /* +0x00 = vtable */

    if (ptr[1] != NULL) {                    /* +0x04 = data_ptr */
        GLOBAL_free(ptr[1]);
    }
    ptr[1] = NULL;                           /* +0x04 = data_ptr = NULL */
    ptr[2] = 0;                              /* +0x08 = capacity = 0 */
}

/* ================================================================== */
/* UI_FreeScrollBar — Store params and delegate to vtable[20]          */
/* Address: 0x424490                                                   */
/*                                                                     */
/* Stores two int parameters at +0x10/+0x14 of the object, then        */
/* calls vtable[20] (0x50/4) cleanup method. Always returns 0.        */
/* Called from UI_DisableWindow and BuildingComplex cleanup.           */
/* ================================================================== */
int __thiscall UI_FreeScrollBar(void* obj, int param1, int param2)
{
    int* objInt = (int*)obj;

    objInt[0x10 / 4] = param1;                /* +0x10 */
    objInt[0x14 / 4] = param2;                /* +0x14 */

    ((Collection*)obj)->Compact();

    return 0;
}

/* ================================================================== */
/* UI_EnableScrollBar — Iterate items calling vtable[4] on each       */
/* Address: 0x424510                                                   */
/*                                                                     */
/* Calls vtable[4] (0x10/4) on every entry index from 0 to count-1.   */
/* Used to enable/disable each scrollbar entry.                        */
/* ================================================================== */
void __fastcall UI_EnableScrollBar(int* timerList)
{
    unsigned int i;
    unsigned int count = (unsigned int)timerList[3];   /* +0x0C = count */

    for (i = 0; i < count; i++) {
        ((Collection*)timerList)->RemoveElement(i);
    }
}

/* ================================================================== */
/* UI_GetScrollPos — Drain items from tail using vtable[3]            */
/* Address: 0x424250                                                   */
/*                                                                     */
/* Repeatedly calls vtable[3] (0x0C/4) on (count-1) until count       */
/* reaches 0. Drains all items from the tail of the array.             */
/* ================================================================== */
void __fastcall UI_GetScrollPos(int* timerList)
{
    int count = timerList[3];   /* +0x0C */

    while (count != 0) {
        ((Collection*)timerList)->RemoveAt(count - 1);
        count = timerList[3];   /* re-read count (may have changed) */
    }
}

/* ================================================================== */
/* UI_SetScrollPos — Drain items from tail using vtable[4]           */
/* Address: 0x424270                                                   */
/*                                                                     */
/* Same pattern as UI_GetScrollPos but dispatches vtable[4] (0x10/4).  */
/* ================================================================== */
void __fastcall UI_SetScrollPos(int* timerList)
{
    int count = timerList[3];   /* +0x0C */

    while (count != 0) {
        ((Collection*)timerList)->RemoveElement(count - 1);
        count = timerList[3];   /* re-read count */
    }
}

/* ================================================================== */
/* UI_HandleScrollMessage — Handle scroll message for a list          */
/* Address: 0x4241E0                                                   */
/*                                                                     */
/* Calls vtable[7] (0x1C/4) to handle the message. If handled and     */
/* msg_index < count-1, shifts array elements up to fill the gap.     */
/* Decrements count. Returns handled flag.                             */
/* ================================================================== */
int __thiscall UI_HandleScrollMessage(void* list, unsigned int msg_index)
{
    int* listInt = (int*)list;
    int handled = ((Collection*)list)->InternalExtract(msg_index) != NULL;

    if (handled != 0) {
        if (msg_index < (unsigned int)(listInt[3] - 1)) {  /* +0x0C = count */
            /* Shift elements forward: memcpy-like copy */
            int* src = (int*)(listInt[1] + (msg_index + 1) * 4);   /* +0x04 = data_ptr */
            int* dst = (int*)(listInt[1] + msg_index * 4);
            int count = listInt[3];
            /* Copy (count - msg_index - 1) ints */
            for (unsigned int j = 0; j < count - msg_index - 1; j++) {
                dst[j] = src[j];
            }
        }
        /* Clear the last element */
        int count = listInt[3];
        *(int*)(listInt[1] + (count - 1) * 4) = 0;

        /* Decrement count */
        listInt[3] = count - 1;
    }

    return handled;
}

/* ================================================================== */
/* UI_ListBox_Clear — Reset a TimerList/ListBox                        */
/* Address: 0x424A00                                                   */
/*                                                                     */
/* Resets to VTBL_TIMERLIST_C base vtable, frees data array.           */
/* Same pattern as UI_InitScrollBar but with different base vtable.    */
/* ================================================================== */
void __fastcall UI_ListBox_Clear(int* timerList)
{
    timerList[3] = 0;                               /* +0x0C = count */
    *(void**)timerList = (void*)VTBL_TIMERLIST_C;    /* +0x00 = vtable */
    timerList[2] = 0;                                /* +0x08 = capacity */

    if ((void*)timerList[1] != NULL) {               /* +0x04 = data_ptr */
        GLOBAL_free((void*)timerList[1]);
    }
    timerList[1] = 0;                                /* +0x04 = data_ptr = NULL */
}

/* ================================================================== */
/* UI_ListData_Dtor_477bd0 — Destructor for TimerList variant A       */
/* Address: 0x424A30                                                   */
/*                                                                     */
/* vtable[0] for VTBL_TIMERLIST_A (0x477BD0).                          */
/* Resets to base vtable, frees data array, optionally frees self.     */
/* ================================================================== */
void* __thiscall UI_ListData_Dtor_477bd0(void* timerList, byte flags)
{
    int* tl = (int*)timerList;

    *(void**)timerList = (void*)VTBL_TIMERLIST_A;    /* +0x00 */

    tl[2] = 0;                                        /* +0x08 */
    if ((void*)tl[1] != NULL) {                       /* +0x04 */
        GLOBAL_free((void*)tl[1]);
    }
    tl[1] = 0;                                        /* +0x04 */

    if ((flags & 1) != 0) {
        GLOBAL_free(timerList);
    }

    return timerList;
}

/* ================================================================== */
/* UI_ListData_Dtor_477b78 — Destructor for TimerList variant B       */
/* Address: 0x424A70                                                   */
/*                                                                     */
/* vtable[1] for VTBL_TIMERLIST_B (0x477B78).                          */
/* Delegates reset to UI_InitScrollBar, optionally frees self.         */
/* ================================================================== */
void* __thiscall UI_ListData_Dtor_477b78(void* timerList, byte flags)
{
    UI_InitScrollBar(timerList);

    if ((flags & 1) != 0) {
        GLOBAL_free(timerList);
    }

    return timerList;
}

/* ================================================================== */
/* UI_ListData_Dtor_477b40 — Destructor for TimerList variant C       */
/* Address: 0x424A90                                                   */
/*                                                                     */
/* vtable[1] for VTBL_TIMERLIST_C (0x477B40).                          */
/* Same pattern as variant A but with different base vtable.           */
/* ================================================================== */
void* __thiscall UI_ListData_Dtor_477b40(void* timerList, byte flags)
{
    int* tl = (int*)timerList;

    *(void**)timerList = (void*)VTBL_TIMERLIST_C;    /* +0x00 */

    tl[2] = 0;                                        /* +0x08 */
    if ((void*)tl[1] != NULL) {                       /* +0x04 */
        GLOBAL_free((void*)tl[1]);
    }
    tl[1] = 0;                                        /* +0x04 */

    if ((flags & 1) != 0) {
        GLOBAL_free(timerList);
    }

    return timerList;
}

/* ================================================================== */
/* UI_ListBox_Dtor — ListBox scalar deleting destructor                */
/* Address: 0x424AD0                                                   */
/*                                                                     */
/* Calls UI_ListBox_Clear, then optionally frees self.                 */
/* vtable[0] at VTBL_TIMERLIST_WRAPPER2 (0x477AEC).                   */
/* ================================================================== */
void* __thiscall UI_ListBox_Dtor(void* listBox, byte flags)
{
    UI_ListBox_Clear((int*)listBox);

    if ((flags & 1) != 0) {
        GLOBAL_free(listBox);
    }

    return listBox;
}

/* ================================================================== */
/* UI_ListBox_FindItem — Binary search through sorted listbox items    */
/* Address: 0x424820                                                   */
/*                                                                     */
/* Performs binary search through items[low..high] using vtable[18]    */
/* (0x48/4) as the comparator function:                                */
/*   comparator(item, list_item) -> <0, =0, >0                        */
/*                                                                     */
/* If direct comparison finds an exact match, returns index.           */
/* Otherwise returns -1 (0xFFFFFFFF).                                  */
/* ================================================================== */
unsigned int __thiscall UI_ListBox_FindItem(void* list, int item_to_find,
                                            unsigned int low, unsigned int high)
{
    int* listInt = (int*)list;
    SortedCollection* collection = (SortedCollection*)list;
    int* data_ptr = (int*)listInt[1];   /* +0x04 */

    /* Binary search */
    if (high - low > 2) {
        unsigned int mid = ((high - low) >> 1) + low;
        int* mid_item = (int*)((char*)data_ptr + mid * 4);

        int cmp = collection->Comparator(
            (void*)(intptr_t)item_to_find, (void*)(intptr_t)*mid_item);

        if (cmp < 0) {
            return collection->FindItem(item_to_find, low, mid);
        }

        return collection->FindItem(item_to_find, mid, high);
    }

    /* Linear search for small range */
    for (; low <= high; low++) {
        int* current_item = (int*)((char*)data_ptr + low * 4);

        int cmp = collection->Comparator(
            (void*)(intptr_t)item_to_find, (void*)(intptr_t)*current_item);

        if (cmp <= 0) {
            if (item_to_find == *current_item) {
                return low;   /* Exact match */
            }
            break;
        }
    }

    return 0xFFFFFFFF;   /* Not found */
}

/* ================================================================== */
/* UI_DrawScrollBar — Create temp descriptor and dispatch render       */
/* Address: 0x424040                                                   */
/*                                                                     */
/* Allocates a 0x88-byte temporary scrollbar descriptor, deep-copies   */
/* from stack source param_2, sets up a 2-phase vtable (0x477820       */
/* base GameObject, then 0x477488 Entity), then invokes the virtual    */
/* collection insertion operation (binary slot 10).                    */
/* ================================================================== */
void __thiscall UI_DrawScrollBar(void* self, int param1, int* param2)
{
    int* temp = (int*)operator_new(0x88);

    if (temp != NULL) {
        /* Deep-copy 0x88 bytes from param2 stack descriptor */
        for (int i = 1; i < 0x88 / 4; i++) {
            temp[i] = param2[i];
        }
        /* The copy omits element 0 (vtable), which is set below */

        /* Phase 1: set GameObject base vtable */
        temp[0] = (int)VTBL_GAMEOBJECT;          /* 0x477820 */

        /* Set byte at +0x86 from source */
        *(char*)((char*)temp + 0x86) = *(char*)((char*)param2 + 0x86);

        /* Phase 2: switch to Entity vtable */
        temp[0] = (int)VTBL_ENTITY;              /* 0x477488 */

        /* Set byte at +0x86 again */
        *(char*)((char*)temp + 0x86) = *(char*)((char*)param2 + 0x86);

        ((Collection*)self)->InsertAt(param1, temp);
        return;
    }

    /* Allocation failed — preserve the original null insertion. */
    ((Collection*)self)->InsertAt(param1, NULL);
}

/* ================================================================== */
/* UI_DrawListBox — Create temp descriptor and dispatch render        */
/* Address: 0x424550                                                   */
/*                                                                     */
/* Same pattern as UI_DrawScrollBar but with 0xA4-byte descriptor and  */
/* a 3-phase vtable chain (0x477820 -> 0x477488 -> 0x477A90).         */
/* ================================================================== */
void __thiscall UI_DrawListBox(void* self, int param1, char* param2)
{
    int* temp = (int*)operator_new(0xA4);

    if (temp != NULL) {
        /* Deep-copy 0xA4 bytes from stack descriptor */
        for (int i = 1; i < 0xA4 / 4; i++) {
            temp[i] = ((int*)param2)[i];
        }
        /* Copy the additional fields beyond 0xA4/4 int boundaries */
        /* Bytes at +0x88, +0x8A, +0x8C, +0x90, +0x94, +0x98, +0x9C, +0xA0 */
        /* These extend beyond 0xA0 bytes into the struct */

        /* Phase 1: GameObject base vtable */
        temp[0] = (int)VTBL_GAMEOBJECT;          /* 0x477820 */

        /* Copy byte at +0x86 */
        *(char*)((char*)temp + 0x86) = *(char*)((char*)param2 + 0x86);

        /* Phase 2: Entity vtable */
        temp[0] = (int)VTBL_ENTITY;              /* 0x477488 */

        /* Copy additional fields from source */
        *(char*)((char*)temp + 0x86) = *(char*)((char*)param2 + 0x86);
        *(char*)((char*)temp + 0x88) = *(char*)((char*)param2 + 0x88);
        *(short*)((char*)temp + 0x8A) = *(short*)((char*)param2 + 0x8A);
        temp[0x23] = ((int*)param2)[0x23];       /* +0x8C */
        temp[0x24] = ((int*)param2)[0x24];       /* +0x90 */
        *(char*)((char*)temp + 0x94) = *(char*)((char*)param2 + 0x94);
        temp[0x26] = ((int*)param2)[0x26];       /* +0x98 */
        temp[0x27] = ((int*)param2)[0x27];       /* +0x9C */
        temp[0x28] = ((int*)param2)[0x28];       /* +0xA0 */

        /* Phase 3: UIEntity vtable (final) */
        temp[0] = (int)VTBL_UIENTITY;            /* 0x477A90 */

        ((Collection*)self)->InsertAt(param1, temp);
        return;
    }

    /* Allocation failed — preserve the original null insertion. */
    ((Collection*)self)->InsertAt(param1, NULL);
}
