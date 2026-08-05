/**
 * TimerSlotList.cpp — Timer slot list structure implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "TimerSlotList.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void GLOBAL_free(void* ptr);    /* 0x465CD0 */

/* ================================================================== */
/* DtorBody — Destructor body                                          */
/* Address: 0x412410                                                   */
/*                                                                     */
/* Called by: scalar deleting destructors and EH unwind handlers       */
/* (0x475251, 0x4765D3, 0x4765F3, 0x4125C3)                           */
/*                                                                     */
/* Resets the structure to a clean dead state: marks vtable to dead    */
/* marker (0x477798 = PTR_Timer_Resize, which is a minimal vtable with */
/* just the Resize function at slot[0]), frees the items array, zeroes */
/* count and capacity.                                                 */
/* ================================================================== */
void TimerSlotList::DtorBody()
{
    /* Reset capacity to 0 */
    this->capacity = 0;                             /* +0x0C */

    /* In the binary: resets vtable to dead marker (0x477798).
     * Natural C++: dead objects have their initialized flag cleared instead. */

    /* Reset count to 0 */
    this->count = 0;                                /* +0x08 */

    /* Free the items array if present */
    if (this->items != nullptr) {                   /* +0x04 */
        GLOBAL_free(this->items);
    }

    /* Null out the items pointer */
    this->items = nullptr;                          /* +0x04 */
}

/* The scalar-deleting destructor slots at 0x412580 and 0x4125C0 are
 * compiler-generated ABI helpers. The user destructor owns only the
 * collection cleanup recovered at 0x412410. */
TimerSlotList::~TimerSlotList()
{
    DtorBody();
}
