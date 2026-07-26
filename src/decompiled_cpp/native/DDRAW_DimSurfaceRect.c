/**
 * DDRAW_DimSurfaceRect — Dim (halve brightness) a rect on the primary surface
 * Address: 0x401540
 * Size: 213 bytes
 * Calling convention: __cdecl
 *
 * Locks the primary DirectDraw surface and divides every pixel in the
 * specified rect by 2 (right-shift by 1) with bitmasking, producing
 * a dimmed/half-bright region. Used for selection highlight feedback
 * and disabled-area overlays. Always returns 1.
 *
 * Called by: Town_DeselectBuilding, UIPANEL drawing code
 *
 * @param left    Rect left (in surface pixels)
 * @param top     Rect top
 * @param right   Rect right
 * @param bottom  Rect bottom
 * @return        Always 1
 */
#include "../shared/types.h"

/* Surface/pixel format globals */
extern int    g_surface_lost;         /* 0x4FD218 */
extern int*   g_primary_surface;      /* 0x4FF0D8 */
extern uint32_t DAT_004fd19c;         /* 0x4FD19C — DDSURFACEDESC */
extern uint32_t DAT_004fd1ac;         /* 0x4FD1AC — surface pitch */
extern uint32_t DAT_004fd1c0;         /* 0x4FD1C0 — surface pixel data ptr */
extern uint16_t g_surface_bshift;     /* 0x48527A — half-bright mask */

int __cdecl DDRAW_DimSurfaceRect(int left, int top, int right, int bottom)
{
    /* Lock the primary surface if not already locked */
    if (g_surface_lost == 0) {
        int* ddsd = &DAT_004fd19c;
        for (int i = 0x1F; i != 0; i--) {
            *ddsd++ = 0;
        }
        DAT_004fd19c = 0x7C;           /* dwSize = sizeof(DDSURFACEDESC) */

        int lock_result = ((int (*)(void*, int, void*, int, int))(
            *(void***)g_primary_surface)[0x64])(  /* Lock */
            g_primary_surface, 0, &DAT_004fd19c, 0, 0);

        if (lock_result != 0) {
            g_surface_lost = 1;
        }
    }

    /* Dim pixels in the specified rect */
    uint32_t pitch_half = (DAT_004fd1ac >> 1) & 0xFFFF;
    uint16_t* pixels = (uint16_t*)(DAT_004fd1c0 +
        ((uint32_t)top * pitch_half + (uint32_t)left) * 2);

    uint32_t width = (uint32_t)(right - left) & 0xFFFF;
    uint32_t height = (uint32_t)(bottom - top) & 0xFFFF;

    if (height != 0) {
        do {
            uint32_t w = width;
            for (; w != 0; w--) {
                *pixels = (*pixels >> 1) & g_surface_bshift;
                pixels++;
            }
            height--;
            pixels += pitch_half - width;
        } while (height != 0);
    }

    /* Unlock the surface if it was locked */
    if (g_surface_lost != 0) {
        int unlock_result = ((int (*)(void*, int))(
            *(void***)g_primary_surface)[0x80])(  /* Unlock */
            g_primary_surface, 0);

        if (unlock_result == 0) {
            g_surface_lost = 0;
        }
    }

    return 1;
}
