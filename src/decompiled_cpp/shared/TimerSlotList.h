/**
 * TimerSlotList.h — Timer slot list structure with multiple vtable variants
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * TimerSlotList is a 16-byte structure matching the Collection field layout
 * (vtable + items array + count + capacity). It stores timer slot entries
 * and has multiple vtable variants for different lifecycle states:
 *
 *   VTBL_TIMERLIST_A (0x477BD0): active/running variant
 *   VTBL_TIMERLIST_C (0x477B40): timer list variant C
 *   VTBL_TIMERLIST_B (0x477B78): variant B
 *   VTBL_TIMERLIST_WRAPPER (0x477AE8): wrapper around collection interface
 *
 * The "dead" vtable (0x477798) is a minimal marker used during destruction
 * to prevent further virtual dispatch.
 *
 * Vtable layout (shared with Collection vtable interface):
 *   [0] +0x00: Timer_Resize (matching Collection::Resize)
 *   [1] +0x04: ScalarDeletingDestructor (variant-specific)
 *   ... remaining slots match the Collection vtable layout
 *
 * Size: 0x10 = 16 bytes
 *
 * Field layout:
 *   +0x00: vtable (void**)  — points to variant-specific vtable
 *   +0x04: items (void**)   — dynamic array of timer entry pointers
 *   +0x08: count (int32_t)  — number of populated entries
 *   +0x0C: capacity (int32_t) — allocated capacity
 */

#pragma once

#include "../shared/types.h"

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

struct TimerSlotList {
/* vtable at +0x00 is compiler-managed */
    void**  items;           /* +0x04  dynamic array of slot entries      */
    int32_t count;           /* +0x08  number of populated entries        */
    int32_t capacity;        /* +0x0C  allocated capacity                 */

    /* ================================================================ */
    /* Destructor body — resets to dead vtable and frees items array     */
    /* Address: 0x412410                                                  */
    /*                                                                     */
    /* Called from scalar deleting destructors and EH unwind handlers.    */
    /* Resets vtable to dead marker (0x477798), zeroes count/capacity,    */
    /* frees the items array via GLOBAL_free, and sets items to NULL.     */
    /* ================================================================ */
    void DtorBody();

    /* ================================================================ */
    /* Scalar deleting destructor (dead/expired vtable variant)          */
    /* Address: 0x412580                                                  */
    /*                                                                     */
    /* Used with vtable 0x4777C0 (dead/expired) and invoked via vtable[1] */
    /* during cleanup. Calls DtorBody, then frees this if flags & 1.      */
    /* ================================================================ */
    void* __thiscall scalar_deleting_dtor_dead(byte flags);

    /* ================================================================ */
    /* Scalar deleting destructor (active/running vtable variant)        */
    /* Address: 0x4125C0                                                  */
    /*                                                                     */
    /* Used with vtable 0x477780 (active) and invoked via vtable[1].       */
    /* Calls DtorBody, then frees this if flags & 1.                      */
    /* ================================================================ */
    void* __thiscall scalar_deleting_dtor_active(byte flags);
};
