/**
 * input_manager.c — Input manager and entity container management
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The "input manager" (g_input_mgr at 0x4A9990) is the central entity
 * registry that holds a collection of all placed GameObjects.
 * It provides iteration, save/load lifecycle, and timer management.
 * Overall structure is about 0x5E8 bytes (but primarily the entity
 * collection grows dynamically).
 *
 * Also contains the direction-to-offset helpers used by NETMAN and
 * train placement code to compute neighbour-tile offsets.
 *
 * Layout (g_input_mgr at 0x4A9990):
 *   +0x00: vtable (INPUT_DtorVtable @ 0x4779C8)
 *   +0x04: timer-list structure A (vtable + items + cap + field)
 *   +0x14: total entity count
 *   +0x18: scenery/vehicle count
 *   +0x1C..+0x5E8: entity collection
 *
 * Timer-list structure (TODO: name this struct):
 *   +0x00: vtable pointer
 *   +0x04: item array pointer
 *   +0x08: capacity
 *   +0x0C: (extra field, used by variant B)
 *
 * Direction-to-offset helpers pack a 2D tile coordinate as:
 *   packed = (Y << 16) | X
 * where X and Y are 16-bit signed offsets relative to the
 * current player's position on the tile map grid.
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* __cdecl operator_new(size_t size);   /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);        /* 0x465CD0 */
extern void  __thiscall INPUT_FreeEditControl(void* ctrl);   /* 0x41F540 */
extern void  __thiscall INPUT_AllocEditControl(void* ctrl);  /* 0x41F590 */

/* ================================================================== */
/* Global variables                                                    */
/* ================================================================== */

extern int   g_input_mgr;                  /* 0x4A9990 — main input manager object */
extern int   g_player_id;                  /* 0x4AAD46 — player tilemap X position */
extern int   g_player_color;               /* 0x4AAD48 — player tilemap Y position */
extern void* g_resmgr;                     /* 0x4855E8 — resource manager */

/* ================================================================== */
/* Vtable pointers for the input manager class                          */
/* ================================================================== */

/* 0x004779C8 — Main InputManager vtable */
/* 0x004779E0 — EditControlWrapper vtable (subclass: INPUT_DtorWrapper @ 0x41E600) */

/* Timer-list vtables used in initialization */
#define VTBL_TIMER_RESIZE_A  0x00477798   /* timer-list vtable variant A */
#define VTBL_TIMER_RESIZE_B  0x00477758   /* timer-list vtable variant B */


/* ================================================================== */
/* INPUT_Init — Construct input manager                                 */
/* Address: 0x41D250                                                    */
/*                                                                      */
/* Allocates a 40-byte timer buffer (10 timer slots x 4 bytes),        */
/* initializes two timer lists (timer vtable + buffer pointers),       */
/* sets the main vtable to INPUT_Dtor (0x4779C8), zeroes field at      */
/* +0x14 (count). Called from GameLoop_Setup.                          */
/*                                                                      */
/* __fastcall (ECX = this)                                              */
/* Returns: this pointer                                                */
/* ================================================================== */
void* __fastcall INPUT_Init(void* self)
{
    int* selfInt = (int*)self;

    /* Initialize first timer list (vtable + buffer) */
    selfInt[1] = VTBL_TIMER_RESIZE_A;   /* +0x04 timer list vtable */
    selfInt[3] = 0;                      /* +0x0C capacity = 0 */
    selfInt[2] = 0;                      /* +0x08 items ptr = NULL */

    /* Allocate 40-byte timer buffer (10 uint32 slots) */
    uint32_t* timerBuf = (uint32_t*)operator_new(40);

    selfInt[2] = (int)(uintptr_t)timerBuf;         /* +0x08 store buffer */

    /* Zero the timer buffer (10 dwords) */
    for (int i = 0; i < 10; i++) {
        timerBuf[i] = 0;
    }

    /* Set capacity: 10 if buffer allocated, else 0 */
    if (selfInt[2] != 0) {
        selfInt[3] = 10;                 /* +0x0C capacity = 10 */
    } else {
        selfInt[3] = 0;
        selfInt[2] = 0;
    }

    /* Initialize second timer list (different vtable variant) */
    selfInt[1] = VTBL_TIMER_RESIZE_B;   /* +0x04 second timer list vtable */
    selfInt[4] = 0;                      /* +0x10 timer list extra field */

    /* Set main vtable */
    selfInt[0] = 0x004779C8;             /* INPUT_Dtor vtable */

    selfInt[5] = 0;                      /* +0x14 entity count = 0 */

    return self;
}


/* ================================================================== */
/* INPUT_Dtor — Scalar deleting destructor (vtable[0])                  */
/* Address: 0x41D2B0                                                    */
/*                                                                      */
/* __thiscall (ECX = this, byte param on stack)                         */
/* Calls INPUT_BaseDtor, then frees memory if param_1 & 1.             */
/*                                                                      */
/* MSVC scalar deleting destructor pattern:                             */
/*   RET 4 — pops the flags byte from the caller's stack                */
/* ================================================================== */
extern void __thiscall INPUT_BaseDtor(void* self);

void* __thiscall INPUT_Dtor(void* self, uint8_t flags)
{
    INPUT_BaseDtor(self);

    if (flags & 1) {
        GLOBAL_free(self);
    }

    return self;
}


/* ================================================================== */
/* INPUT_BaseDtor — Base destructor body                                */
/* Address: 0x41D2D0                                                    */
/*                                                                      */
/* __fastcall (ECX = this)                                              */
/*                                                                      */
/* Restores vtable to 0x4779C8, resets timer lists, frees the timer    */
/* buffer at +0x08 if allocated, and calls INPUT_Cleanup (which is a   */
/* virtual dispatch thunk to vtable[3]).                                */
/* ================================================================== */
void __fastcall INPUT_BaseDtor(void* self)
{
    int* selfInt = (int*)self;

    /* Restore main vtable */
    selfInt[0] = 0x004779C8;             /* INPUT_Dtor vtable */

    /* Reset second timer list */
    selfInt[4] = 0;                      /* +0x10 */

    /* Restore first timer list vtable variant A */
    selfInt[1] = VTBL_TIMER_RESIZE_A;    /* +0x04 */
    selfInt[3] = 0;                      /* +0x0C capacity = 0 */

    /* Free timer buffer */
    if (selfInt[2] != 0) {               /* +0x08 items ptr */
        GLOBAL_free((void*)(uintptr_t)selfInt[2]);
    }
    selfInt[2] = 0;                      /* +0x08 = NULL */
}


/* ================================================================== */
/* INPUT_Cleanup — Virtual dispatch thunk to vtable[3]                 */
/* Address: 0x41D310                                                    */
/*                                                                      */
/* This is NOT a cleanup body — it is a 5-byte JUMP thunk that         */
/* tail-calls to vtable[3] of the current object.                      */
/*                                                                      */
/* Equivalent to: this->vtable[3](this)                                 */
/* ================================================================== */
void __fastcall INPUT_Cleanup(void* self)
{
    /* Load vtable and jump to slot 3 (offset 0x0C) */
    void** vtbl = *(void***)self;
    void (*method3)(void*) = (void (*)(void*))vtbl[3];
    method3(self);
}


/* ================================================================== */
/* INPUT_Shutdown — Free edit-control linked lists                      */
/* Address: 0x41F4E0                                                    */
/*                                                                      */
/* Frees two singly-linked lists from the input manager:
/*   free_list at +0x08 (each node has next at +0x30 = [12])            */
/*   alloc_list at +0x0C (each node has next at +0x44 = [17])          */
/* For each node: calls INPUT_FreeEditControl/INPUT_AllocEditControl    */
/* then GLOBAL_free on the node. Called from CGWND_Cleanup.             */
/* ================================================================== */
void __fastcall INPUT_Shutdown(void* self)
{
    int* selfInt = (int*)self;
    void* node;

    /* Free the 'free' list (each node: next pointer at offset +0x30) */
    node = (void*)(uintptr_t)selfInt[2];   /* +0x08 free_list head */
    while (node != NULL) {
        selfInt[2] = *(int*)((char*)node + 0x30);  /* node->next */
        if (node != NULL) {
            INPUT_FreeEditControl(node);
            GLOBAL_free(node);
        }
        node = (void*)(uintptr_t)selfInt[2];
    }

    /* Free the 'alloc' list (each node: next pointer at offset +0x44) */
    node = (void*)(uintptr_t)selfInt[3];   /* +0x0C alloc_list head */
    while (node != NULL) {
        selfInt[3] = *(int*)((char*)node + 0x44);  /* node->next */
        if (node != NULL) {
            INPUT_AllocEditControl(node);
            GLOBAL_free(node);
        }
        node = (void*)(uintptr_t)selfInt[3];
    }
}


/* ================================================================== */
/* INPUT_GetSaveFileName — Call PrepareSave on all entities             */
/* Address: 0x41DD40                                                    */
/*                                                                      */
/* Iterates all entities in the collection at this+0x04, calling       */
/* vtable[0x28] (PrepareSave) on each. Called every frame in           */
/* game modes 3 and 9 from GameLoop_FrameUpdate.                       */
/* ================================================================== */
void __fastcall INPUT_GetSaveFileName(void* self)
{
    int* collection = (int*)((char*)self + 4);   /* +0x04 entity collection */

    uint32_t idx = 0;
    int count = (**(int (__thiscall**)(int*))((uintptr_t)collection[0] + 0x2C))(collection);

    if (count != 0) {
        do {
            int* entity = (int*)(uintptr_t)(**(int (__thiscall**)(int*, uint32_t))
                                    ((uintptr_t)collection[0] + 0x20))(collection, idx);
            if (entity != NULL) {
                /* Call vtable[0x28] — PrepareSave */
                (**(void (__thiscall**)(int*))((uintptr_t)entity[0] + 0x28))(entity);
            }
            idx++;
            count = (**(int (__thiscall**)(int*))((uintptr_t)collection[0] + 0x2C))(collection);
        } while (idx < (uint32_t)count);
    }
}


/* ================================================================== */
/* INPUT_DirToOffset_Up — Compute tile offset for "Up" direction       */
/* Address: 0x41D8F0                                                    */
/*                                                                      */
/* Computes the tilemap offset for the "up" direction relative to the   */
/* current player position. Sets packed value (Y<<16)|X in *output.    */
/*                                                                      */
/* X = g_player_id - 3                                                  */
/* Y = (g_player_color >> 1) - 1                                        */
/*                                                                      */
/* Called by: NETMAN send operations, Train_AddTrainCar.                */
/*                                                                      */
/* @param output  int* — destination for packed (Y<<16)|X offset       */
/* ================================================================== */
void __stdcall INPUT_DirToOffset_Up(int* output)
{
    int32_t x = g_player_id - 3;
    int16_t y = (int16_t)((g_player_color >> 1) - 1);
    *output = ((int)y << 16) | (uint16_t)x;
}


/* ================================================================== */
/* INPUT_DirToOffset_Left — Compute tile offset for "Left" direction   */
/* Address: 0x41D920                                                    */
/*                                                                      */
/* X = 0                                                                */
/* Y = (g_player_color >> 1) - 1                                        */
/*                                                                      */
/* @param output  int* — destination for packed (Y<<16)|X offset       */
/* ================================================================== */
void __stdcall INPUT_DirToOffset_Left(int* output)
{
    int16_t y = (int16_t)((g_player_color >> 1) - 1);
    *output = ((int)y << 16) | 0;
}


/* ================================================================== */
/* INPUT_DirToOffset_Down — Compute tile offset for "Down" direction   */
/* Address: 0x41D950                                                    */
/*                                                                      */
/* X = (g_player_id >> 1) - 1                                           */
/* Y = g_player_color - 2                                               */
/*                                                                      */
/* @param output  int* — destination for packed (Y<<16)|X offset       */
/* ================================================================== */
void __stdcall INPUT_DirToOffset_Down(int* output)
{
    int16_t x = (int16_t)((g_player_id >> 1) - 1);
    int16_t y = (int16_t)(g_player_color - 2);
    *output = ((int)y << 16) | (uint16_t)x;
}


/* ================================================================== */
/* INPUT_DirToOffset_Right — Compute tile offset for "Right" direction */
/* Address: 0x41D980                                                    */
/*                                                                      */
/* X = (g_player_id >> 1) - 1                                           */
/* Y = 0                                                                */
/*                                                                      */
/* @param output  int* — destination for packed (Y<<16)|X offset       */
/* ================================================================== */
void __stdcall INPUT_DirToOffset_Right(int* output)
{
    int16_t x = (int16_t)((g_player_id >> 1) - 1);
    *output = 0 | (uint16_t)x;
}
