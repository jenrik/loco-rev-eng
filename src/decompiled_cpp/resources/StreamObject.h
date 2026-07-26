/**
 * StreamObject.h — Stream I/O object with synchronization support
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * StreamObject is a structure containing a CRITICAL_SECTION and a sync
 * flag used for thread-safe streaming operations. The Lock/Unlock helper
 * functions conditionally enter/leave the critical section based on the
 * sync flag at +0x34 (negative = active synchronization).
 *
 * Size: variable (the full stream object is larger; Lock/Unlock only access
 * offsets +0x34 (sync flag, int32) and +0x38 (CRITICAL_SECTION)).
 *
 * This is NOT a C++ class — it's a C struct with free-function helpers.
 *
 * Field layout (relevant offsets):
 *   +0x34: sync_flag (int32) — negative = use CRITICAL_SECTION
 *   +0x38: cs (CRITICAL_SECTION) — Win32 critical section object
 *
 * Used by: CGWND_AboutDialog_LoadCredits, various streaming I/O functions
 */

#pragma once

#include "../shared/types.h"
#include <windows.h>

/* ================================================================== */
/* StreamObject_Lock — Enter CRITICAL_SECTION if sync is active       */
/* Address: 0x410240                                                   */
/*                                                                     */
/* If stream->sync_flag (+0x34) is negative (bit 31 set), enters the   */
/* CRITICAL_SECTION at stream+0x38. Otherwise does nothing.            */
/*                                                                     */
/* Called by: CGWND_AboutDialog_LoadCredits (0x4101AA)                 */
/*                                                                     */
/* @param stream  Pointer to stream object                            */
/* ================================================================== */
void __cdecl StreamObject_Lock(void* stream);

/* ================================================================== */
/* StreamObject_Unlock — Leave CRITICAL_SECTION if sync is active     */
/* Address: 0x410260                                                   */
/*                                                                     */
/* If stream->sync_flag (+0x34) is negative (bit 31 set), leaves the   */
/* CRITICAL_SECTION at stream+0x38. Otherwise does nothing.            */
/*                                                                     */
/* Called by: CGWND_AboutDialog_LoadCredits (0x4101C5)                 */
/*                                                                     */
/* @param stream  Pointer to stream object                            */
/* ================================================================== */
void __cdecl StreamObject_Unlock(void* stream);
