/**
 * TrackPos.h — Track position struct (20 bytes, vtable 0x477840)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * TrackPos represents a grid-cell position on the game's circular track
 * system. It stores a vtable pointer, a segment index (0-11 for the 12
 * track segments), and an offset coordinate within that segment. The
 * sentinel value -1 marks uninitialized fields.
 *
 * The struct is used by INPUT_* edit/cursor functions to store track-
 * based cursor positions. TrackPos_IsObjectBetween performs a 1D overlap
 * check on the circular track layout using a 12-entry offset lookup table
 * at 0x47E410 (shared with Game_IsPositionBetween's month-days table).
 *
 * Size: 0x14 = 20 bytes
 * Vtable: 0x477840
 *
 * Field layout:
 *   +0x00: vtable pointer (0x477840)
 *   +0x04: field_04 (int32_t, initialized to -1)
 *   +0x08: field_08 (int32_t, initialized to -1)
 *   +0x0C: coordinate (int32_t, initialized to -1)
 *   +0x10: segment_index (int32_t, initialized to -1, valid range 0..0xB)
 *
 * Functions:
 *   TrackPos_Init (0x412620)           — Full init: vtable + all 4 fields to -1
 *   TrackPos_BaseInit (0x412660)       — Fast init: vtable only (placement-new unwind)
 *   TrackPos_IsObjectBetween (0x412670) — 1D overlap check on circular 12-segment track
 */

#pragma once

#include <stdint.h>

/* ================================================================== */
/* TrackPos struct — 20-byte track grid position                        */
/* ================================================================== */

struct TrackPos {
    int32_t vtable;             /* +0x00  vtable -> 0x477840                */
    int32_t field_04;           /* +0x04  sentinel -1 when uninitialized     */
    int32_t field_08;           /* +0x08  sentinel -1 when uninitialized     */
    int32_t coordinate;         /* +0x0C  offset within segment, -1 = invalid */
    int32_t segment_index;      /* +0x10  track segment index (0..0xB), -1 = invalid */
};

/* ================================================================== */
/* Functions                                                           */
/* ================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TrackPos_Init — Full 20-byte initializer for TrackPos struct.
 * Address: 0x412620
 * Calling convention: __fastcall (ECX = this ptr, no stack args)
 *
 * Sets vtable to 0x477840 and all 4 data fields to -1 (uninitialized
 * sentinel). Called by INPUT_* edit/cursor functions to reset grid-cell
 * track-position structs.
 */
void __fastcall TrackPos_Init(TrackPos* pos);

/**
 * TrackPos_BaseInit — Fast vtable-only initializer for TrackPos struct.
 * Address: 0x412660
 * Calling convention: __fastcall (ECX = this ptr, no stack args)
 *
 * Only sets vtable = 0x477840; does NOT reset the 4 data fields (unlike
 * TrackPos_Init). Used for lightweight re-init (placement-new unwind
 * cleanup). 14 callers including INPUT_* edit functions and exception-
 * handler stubs.
 */
void __fastcall TrackPos_BaseInit(TrackPos* pos);

/**
 * TrackPos_IsObjectBetween — 1D overlap check on a circular 12-segment track.
 * Address: 0x412670
 * Calling convention: __cdecl (3 stack args)
 *
 * Tests if current's total position (segment_offset + coordinate) lies
 * BETWEEN start and end's total positions on the circular track layout.
 * Uses the offset lookup table at 0x47E410 (12 entries) to convert
 * segment indices to global offsets. Handles wrap-around (end < start).
 *
 * BUG: when segment_index is valid but coordinate is -1, the bounds
 * check passes but the coordinate addition produces incorrect results.
 *
 * Called by: INPUT_SetMouse during mouse-move linked-list iteration.
 *
 * @param current  TrackPos* — the position to test
 * @param start    TrackPos* — range start
 * @param end      TrackPos* — range end
 * @return         1 if current is within [start, end] (inclusive), 0 otherwise
 */
int __cdecl TrackPos_IsObjectBetween(
    const TrackPos* current,
    const TrackPos* start,
    const TrackPos* end);

#ifdef __cplusplus
}
#endif
