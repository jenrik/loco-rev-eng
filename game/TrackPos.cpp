/**
 * TrackPos.cpp — TrackPos struct implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Provides initialization and 1D overlap checking for the 12-segment
 * circular track position system used by INPUT_* edit functions.
 */

// Status: TRANSCRIBED

#include "TrackPos.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Track segment offset lookup table at 0x47E410.
 * 12 entries (uint16_t), one per segment index (0..0xB), giving the
 * cumulative offset for each segment around the circular track.
 * NOTE: This table is shared with Game_IsPositionBetween (0x412790)
 * which uses it as a month-days lookup. Both use 12 entries. */
extern uint16_t g_trackSegmentOffsets[];   /* DAT_0047e410 */

/* ================================================================== */
/* TrackPos_Init — Full 20-byte initializer                            */
/* Address: 0x412620                                                    */
/*                                                                      */
/* Called by: INPUT_ExitGame (0x41E5A8), INPUT_EditPaintSelection       */
/*            (0x41FB5A), INPUT_EditTimerHandler (0x41FC1E)            */
/*                                                                      */
/* Sets vtable to VTBL_TRACKPOS (0x477840) and all 4 data fields to    */
/* -1 (uninitialized sentinel).                                         */
/* ================================================================== */
void __fastcall TrackPos_Init(TrackPos* pos)
{
    pos->vtable = static_cast<int32_t>(0x00477840); /* +0x00 = TRACKPOS descriptor */
    pos->field_04 = -1;                          /* +0x04 = -1 */
    pos->field_08 = -1;                          /* +0x08 = -1 */
    pos->coordinate = -1;                        /* +0x0C = -1 */
    pos->segment_index = -1;                     /* +0x10 = -1 */
}

/* ================================================================== */
/* TrackPos_BaseInit — Fast vtable-only initializer                    */
/* Address: 0x412660                                                    */
/*                                                                      */
/* Called by: INPUT_CreateEditControl (0x41E6A4), INPUT_FreeEditControl */
/*            (0x41F563), INPUT_AllocEditControl (0x41F5B3), and 11    */
/*            SEH unwind stubs (0x475581..0x4756CE).                    */
/*                                                                      */
/* Lightweight re-init: only sets vtable; does NOT reset data fields.   */
/* Used after TrackPos_Init to "finalize" the initialization, or in     */
/* exception-handler cleanup paths that only need to reset the vtable.   */
/* ================================================================== */
void __fastcall TrackPos_BaseInit(TrackPos* pos)
{
    pos->vtable = static_cast<int32_t>(0x00477840); /* +0x00 = TRACKPOS descriptor */
}

/* ================================================================== */
/* TrackPos_IsObjectBetween — 1D overlap check on circular 12-segment  */
/* Address: 0x412670                                                    */
/*                                                                      */
/* Called by: INPUT_SetMouse (0x41FA07) during mouse-move linked-list  */
/*            iteration, to check if an object's track position falls   */
/*            within the cursor's track position range.                 */
/*                                                                      */
/* Tests if current's total position (segment_offset + coordinate)     */
/* lies BETWEEN start and end's total positions on the circular track.  */
/*                                                                      */
/* Algorithm:                                                           */
/*   1. Validate all three positions have valid segment_index (0..0xB)  */
/*      and coordinate (!= -1). Return 0 (false) if any is invalid.    */
/*   2. Convert each segment_index to a cumulative offset using the     */
/*      12-entry lookup table at 0x47E410.                              */
/*   3. Compute total = segment_offset + coordinate for each.           */
/*   4. Check if current_total is within [start_total, end_total]       */
/*      with wrap-around support (end_total < start_total means the     */
/*      range spans the 0 boundary).                                    */
/*                                                                      */
/* BUG: When segment_index passes the bounds check but coordinate is   */
/*      -1 (uninitialized), the total computed will be offset_ - 1,    */
/*      producing an incorrect position that may cause false positives */
/*      or negatives in the overlap test.                               */
/* ================================================================== */
int __cdecl TrackPos_IsObjectBetween(
    const TrackPos* current,
    const TrackPos* start,
    const TrackPos* end)
{
    /* ---- Validate all three segment indices ---- */

    /* Validate start segment index */
    int start_seg = start->segment_index;              /* +0x10 */
    if (start_seg < 0 || start_seg > 0xB) {
        return 0;
    }

    /* Validate end segment index */
    int end_seg = end->segment_index;                  /* +0x10 */
    if (end_seg < 0 || end_seg > 0xB) {
        return 0;
    }

    /* Validate start and end coordinates (must not be -1) */
    if (start->coordinate == -1 || end->coordinate == -1) {  /* +0x0C */
        return 0;
    }

    /* ---- Compute absolute positions from segment offsets + coordinates ---- */

    /* Start total = segment_offset[start_seg] + start->coordinate */
    int start_total = static_cast<int>(g_trackSegmentOffsets[start_seg]) + start->coordinate;

    /* Current total = segment_offset[current_seg] + current->coordinate */
    int cur_seg = current->segment_index;              /* +0x10 */
    int cur_total = static_cast<int>(g_trackSegmentOffsets[cur_seg]) + current->coordinate; /* +0x0C */

    /* End total = segment_offset[end_seg] + end->coordinate */
    int end_total = static_cast<int>(g_trackSegmentOffsets[end_seg]) + end->coordinate;

    /* ---- Range check with wrap-around ---- */
    if (end_total < start_total) {
        /* Wrap-around: range crosses the 0 boundary.
           Current is valid if it's NOT strictly outside:
           (cur < start AND end < cur) would mean cur is in the gap. */
        if (cur_total >= start_total || cur_total <= end_total) {
            return 1;
        }
    } else {
        /* Normal range: current is between start and end (inclusive) */
        if (cur_total >= start_total && cur_total <= end_total) {
            return 1;
        }
    }

    return 0;
}
