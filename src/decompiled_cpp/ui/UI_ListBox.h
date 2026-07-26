/**
 * UI_ListBox.h — List box drawing and item management functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions manage list box UI elements that display sorted
 * collections of items. They handle:
 *   - Drawing list boxes (deep-copy 0xA4-byte DrawContext, dispatch to vtable[10])
 *   - Binary search for items in sorted lists
 *   - Clearing/resetting the list box item collection
 *
 * The list box items are stored in a TimerList-like structure at +0x04
 * with items array, and use vtable[18] (offset 0x48) as comparator.
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* ================================================================== */
/* UI_DrawListBox — Render list box items                              */
/* Address: 0x424550                                                    */
/*                                                                     */
/* Allocates 0xA4-byte DrawContext, deep-copies 0xA2 bytes from param2, */
/* sets up vtable chain (GameObject -> Entity -> UIEntity@0x477A90),   */
/* then calls the virtual InsertAt method. Same pattern as             */
/* UI_DrawScrollBar but larger context (0xA4 vs 0x88 bytes).          */
/*                                                                     */
/* @param this     UI object (vtable dispatch target)                   */
/* @param param1   First parameter passed to vtable[10]                */
/* @param param2   Source DrawContext (0xA4 bytes)                     */
/* ================================================================== */
void __thiscall UI_DrawListBox(void* self, int param1, int param2);

/* ================================================================== */
/* UI_ListBox_FindItem — Binary search for item in sorted list         */
/* Address: 0x424820                                                    */
/*                                                                     */
/* Recursive binary search on sorted items[low..high] using            */
/* vtable[18] comparator. Base case: linear search for ranges <= 2.   */
/* Returns matching index, or 0xFFFFFFFF (-1) if not found.           */
/*                                                                     */
/* @param this    ListBox collection object                            */
/* @param target  Target item pointer to find                          */
/* @param low     Low bound (inclusive)                                */
/* @param high    High bound (inclusive)                              */
/* @return        Index of matching item, or 0xFFFFFFFF                */
/* ================================================================== */
uint __thiscall UI_ListBox_FindItem(void* self, int target, uint low, uint high);

/* ================================================================== */
/* UI_ListBox_Clear — Clear/reset list box item collection             */
/* Address: 0x424A00                                                    */
/*                                                                     */
/* Resets the TimerList sub-object: vtable=VTBL_TIMERLIST_C, count=0,  */
/* capacity=0, frees items array, sets items=NULL. Used from ListBox   */
/* destructor and SEH unwind handlers.                                 */
/* ================================================================== */
void __fastcall UI_ListBox_Clear(void* self);

/* #endif removed — header uses #pragma once */
