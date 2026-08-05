/**
 * math_helpers.c — Euclidean geometry helper free functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These are simple C free functions used for collision detection and
 * UI hit-testing. No object context.
 */

#include <stdint.h>

/* ================================================================== */
/* Math_DistSquared                                                    */
/* Address: 0x45C7A0                                                   */
/* Size: 31 bytes (11 insn)                                            */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Computes squared Euclidean distance between two points:             */
/*   (x1-x2)^2 + (y1-y2)^2                                            */
/*                                                                     */
/* Called by: Building_FindNearbyObject (0x433F89),                    */
/*            VehicleEditor_MoveAlongTrack (0x42DF53),                 */
/*            Town_CheckOccupied (0x42C9A1),                           */
/*            TileMap_FindNearestObject (0x457D3C)                     */
/*                                                                     */
/* @param x1  First point X                                            */
/* @param y1  First point Y                                            */
/* @param x2  Second point X                                           */
/* @param y2  Second point Y                                           */
/* @return    (x1-x2)^2 + (y1-y2)^2 as int32                           */
/* ================================================================== */
int32_t __cdecl Math_DistSquared(int32_t x1, int32_t y1,
                                 int32_t x2, int32_t y2)
{
    int32_t dx = x1 - x2;
    int32_t dy = y1 - y2;
    return dx * dx + dy * dy;
}

/* ================================================================== */
/* Math_PointOnLineSegment                                             */
/* Address: 0x45C7C0                                                   */
/* Size: 88 bytes (41 insn)                                            */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Tests whether point (px, py) lies on the line segment from          */
/* (x1, y1) to (x2, y2). Uses the cross-product method to check       */
/* collinearity and bounding-box test for segment inclusion.           */
/*                                                                     */
/* Called by: Building collision code, UI hit-testing.                 */
/*                                                                     */
/* @param px  Test point X                                             */
/* @param py  Test point Y                                             */
/* @param x1  Segment start X                                          */
/* @param y1  Segment start Y                                          */
/* @param x2  Segment end X                                            */
/* @param y2  Segment end Y                                            */
/* @return    1 if point lies on the segment, 0 otherwise              */
/* ================================================================== */
uint8_t __cdecl Math_PointOnLineSegment(
    int32_t px, int32_t py,
    int32_t x1, int32_t y1,
    int32_t x2, int32_t y2)
{
    /* Determine min/max X for bounding-box check */
    int32_t minX, maxX;

    if (x1 <= x2) {
        minX = x1;
        maxX = x2;
    } else {
        minX = x2;
        maxX = x1;
    }

    /* Check X bounds */
    if (px < minX) return 0;

    /* Recompute min/max (original branches preserved) */
    if (x1 > x2) {
        minX = x2;
        maxX = x1;
    } else {
        minX = x1;
        maxX = x2;
    }

    if (px > maxX) return 0;

    /* Cross-product collinearity test:
     * (x2 - x1) * (py - y1) == (y2 - y1) * (px - x1)
     *
     * The original binary evaluates:
     *   (x2 - x1) * py - ((y2 - y1) * px + x2 * y1 - y2 * x1) == 0
     * which is the cross-product form for collinearity.
     * Rearranged: (x2-x1)*(py-y1) - (y2-y1)*(px-x1) == 0
     */
    int32_t cross = (x2 - x1) * py - ((y2 - y1) * px + x2 * y1 - y2 * x1);
    return (cross == 0) ? 1 : 0;
}
