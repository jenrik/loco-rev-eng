/**
 * UI_ScrollBar.h — Scroll bar and list management functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions operate on TimerList/Collection-like structures to
 * manage scrollable content in UI panels. They handle:
 *   - Scroll bar rendering (deep-copy draw context, delegate to vtable[10])
 *   - Item removal dispatch (HandleScrollMessage)
 *   - Tail-based item draining (GetScrollPos, SetScrollPos)
 *   - TimerList initialization and cleanup (InitScrollBar, FreeScrollBar)
 *   - Iteration over items (EnableScrollBar)
 *
 * Each function operates on an object with a vtable and TimerList-like
 * field layout: vtable(+0x00), items(+0x04), count(+0x08), capacity(+0x0C).
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* ================================================================== */
/* UI DrawScrollBar — Render scrollbar with thumb proportional to       */
/* content/visible range.                                               */
/* Address: 0x424040                                                    */
/*                                                                     */
/* Allocates 0x88-byte DrawContext, deep-copies 0x86 bytes from param2, */
/* constructs an Entity descriptor, then inserts it through the        */
/* collection's virtual InsertAt operation (binary slot 10).           */
/*                                                                     */
/* @param this     UI object (vtable dispatch target)                   */
/* @param param1   First parameter passed to vtable[10]                */
/* @param param2   Source DrawContext (0x88 bytes of frame data)       */
/* ================================================================== */
void __thiscall UI_DrawScrollBar(void* self, int param1, int param2);

/* ================================================================== */
/* UI HandleScrollMessage — Dispatch scroll removal and shift items     */
/* Address: 0x4241E0                                                    */
/*                                                                     */
/* Calls the virtual InternalExtract method on param1. If handled and   */
/* param1 < count-1, shifts subsequent items left by copying memory.    */
/* Sets last slot to NULL and decrements count. Returns handled flag.  */
/*                                                                     */
/* @param this    Collection-like object                               */
/* @param param1  Index to remove                                      */
/* @return        1 if handled, 0 otherwise                            */
/* ================================================================== */
int __thiscall UI_HandleScrollMessage(void* self, uint param1);

/* ================================================================== */
/* UI GetScrollPos — Drain items from tail (vtable[3] loop)            */
/* Address: 0x424250                                                    */
/*                                                                     */
/* Processes items from end of collection via vtable[3](count-1)        */
/* repeatedly until count reaches 0. Drains all items from tail.       */
/* ================================================================== */
void __fastcall UI_GetScrollPos(void* self);

/* ================================================================== */
/* UI SetScrollPos — Drain items from tail (vtable[4] loop)            */
/* Address: 0x424270                                                   */
/*                                                                     */
/* Same drain pattern as GetScrollPos but dispatches via vtable[4]     */
/* instead of vtable[3].                                               */
/* ================================================================== */
void __fastcall UI_SetScrollPos(void* self);

/* ================================================================== */
/* UI InitScrollBar — Initialize/reset TimerList                       */
/* Address: 0x424460                                                    */
/*                                                                     */
/* Sets vtable to VTBL_TIMERLIST_A (0x477BD0), count=0, capacity=0,    */
/* frees old items array, sets items=NULL. Used as SEH unwind handler  */
/* and from ListBox constructors.                                      */
/* ================================================================== */
void __fastcall UI_InitScrollBar(void* self);

/* ================================================================== */
/* UI FreeScrollBar — Store params and delegate to vtable[20]          */
/* Address: 0x424490                                                    */
/*                                                                     */
/* Stores two parameters at +0x10 and +0x14, then calls vtable[20]     */
/* (offset 0x50 — Compact/cleanup). Always returns 0.                  */
/* ================================================================== */
int __thiscall UI_FreeScrollBar(void* self, int param1, int param2);

/* ================================================================== */
/* UI EnableScrollBar — Iterate items calling vtable[4] on each        */
/* Address: 0x424510                                                    */
/*                                                                     */
/* Iterates from 0 to count-1, calling vtable[4] on each index.        */
/* Used to enable/disable all scrollbar items.                         */
/* ================================================================== */
void __fastcall UI_EnableScrollBar(void* self);

/* #endif removed — header uses #pragma once */
