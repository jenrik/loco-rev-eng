/**
 * ui_position.c — Window positioning and visibility helper functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These are C free functions (__cdecl, __thiscall) that operate on
 * RECT structures and UIPANEL visibility.
 */

#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void  __fastcall DDRAW_UnlockPrimary(void);          /* 0x4014CD */
extern void  __fastcall UIPANEL_Render(void* panel, byte enable_tile); /* 0x426EB0 */
extern int   __stdcall SetCapture(void* hwnd);              /* Win32 API */
extern int   __stdcall ReleaseCapture(void);                 /* Win32 API */
extern int   __stdcall ShowCursor(int show);                 /* Win32 API */

/* ================================================================== */
/* UI_CenterWindow — Center one RECT within another                   */
/* Address: 0x425A50                                                   */
/*                                                                     */
/* Modifies pInnerRect in-place to be centered within pOuterRect.      */
/* __cdecl calling convention (both params on stack).                  */
/*                                                                     */
/* @param pOuterRect  Outer bounding rectangle (unmodified)            */
/* @param pInnerRect  Inner rectangle (modified to centered position)  */
/* ================================================================== */
void __cdecl UI_CenterWindow(int* pOuterRect, int* pInnerRect)
{
    int outer_left   = pOuterRect[0];
    int outer_right  = pOuterRect[2];
    int outer_top    = pOuterRect[1];
    int outer_bottom = pOuterRect[3];

    int inner_left   = pInnerRect[0];
    int inner_right  = pInnerRect[2];
    int inner_top    = pInnerRect[1];
    int inner_bottom = pInnerRect[3];

    int inner_width  = inner_right - inner_left;
    int inner_height = inner_bottom - inner_top;

    int outer_width  = outer_right - outer_left;
    int outer_height = outer_bottom - outer_top;

    /* Center horizontally: new_left = outer_left + (outer_width - inner_width) / 2 */
    int new_left = outer_left + (outer_width - inner_width) / 2;
    pInnerRect[0] = new_left;
    pInnerRect[2] = new_left + inner_width;

    /* Center vertically: new_top = outer_top + (outer_height - inner_height) / 2 */
    int new_top = outer_top + (outer_height - inner_height) / 2;
    pInnerRect[1] = new_top;
    pInnerRect[3] = new_top + inner_height;
}

/* ================================================================== */
/* UI_CalcDialogCoords — Map coordinates between rect spaces           */
/* Address: 0x425AC0                                                   */
/*                                                                     */
/* Maps a point (px,py) from pSrcRect coordinate space to pDstRect     */
/* space using fixed-point scaling (*1000). Both px and py are         */
/* modified in place. Similar to Win32 MapDialogRect.                  */
/*                                                                     */
/* __cdecl calling convention.                                         */
/* ================================================================== */
void __cdecl UI_CalcDialogCoords(int* px, int* py,
                                  int* pSrcRect, int* pDstRect)
{
    int src_left   = pSrcRect[0];
    int src_top    = pSrcRect[1];
    int src_right  = pSrcRect[2];
    int src_bottom = pSrcRect[3];
    int src_width  = src_right - src_left;
    int src_height = src_bottom - src_top;

    int dst_left   = pDstRect[0];
    int dst_top    = pDstRect[1];
    int dst_right  = pDstRect[2];
    int dst_bottom = pDstRect[3];
    int dst_width  = dst_right - dst_left;
    int dst_height = dst_bottom - dst_top;

    /* Offset relative to source rect */
    *px = *px - src_left;
    *py = *py - src_top;

    /* Scale: fixed-point multiply by 1000 */
    *px = (*px * (dst_width * 1000 / src_width)) / 1000;
    *py = (*py * (dst_height * 1000 / src_height)) / 1000;

    /* Offset to destination rect */
    *px = *px + dst_left;
    *py = *py + dst_top;
}

/* ================================================================== */
/* UI_SetWindowVisible — Toggle UIPANEL visibility                      */
/* Address: 0x425F20                                                   */
/*                                                                     */
/* show=0: Hide — capture mouse, hide OS cursor, set visible=0.        */
/* show=1: Show — release capture, show OS cursor, set visible=1.      */
/* Always renders the panel to refresh display after vis change.       */
/* ================================================================== */
void __thiscall UI_SetWindowVisible(void* panel, char show)
{
    if (show == 0) {
        /* Hide */
        *(char*)((int)panel + 0x3C) = 0;    /* capture flag = 0 */

        SetCapture(*(void**)((int)panel + 8));  /* get hwnd */
        int vis = ShowCursor(0);
        while (vis >= 0) {
            vis = ShowCursor(0);
        }

        DDRAW_UnlockPrimary();
        UIPANEL_Render(panel, 1);
        DDRAW_UnlockPrimary();
    } else {
        /* Show */
        *(char*)((int)panel + 0x3C) = 1;    /* capture flag = 1 */

        ReleaseCapture();
        int vis = ShowCursor(1);
        while (vis < 0) {
            vis = ShowCursor(1);
        }

        DDRAW_UnlockPrimary();
        UIPANEL_Render(panel, 1);
        DDRAW_UnlockPrimary();
    }
}
