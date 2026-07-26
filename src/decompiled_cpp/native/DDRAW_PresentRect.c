/**
 * DDRAW_PresentRect — Blit a backbuffer rect to the primary surface
 * Address: 0x401280
 * Size: 601 bytes
 * Calling convention: __cdecl
 *
 * Blits a region of the backbuffer to the primary surface for display.
 * Handles client-to-screen coordinate conversion, window clipping, and
 * surface-loss recovery (restores surfaces on DDERR_SURFACELOST).
 * Optionally uses color key (param_4 flag).
 *
 * Called by: UIPANEL_EndPaintEx (dispatch), frame presentation code
 *
 * @param pRect       Source rect in backbuffer coordinates
 * @param hWnd        Window handle for coordinate offset
 * @param viewport_xy Viewport scroll offset (ptr to [x, y]) or NULL
 * @param use_ck      Nonzero = use color key for blit
 */
#include "../shared/types.h"

extern int* g_primary_surface;       /* 0x4FF0D8 */
extern int* g_backbuffer;            /* 0x4FF0DC */
extern int  g_viewport_rect_left;    /* 0x4FD0F0 */
extern int  g_viewport_rect_top;
extern int  g_viewport_rect_right;
extern int  g_viewport_rect_bottom;
extern void* g_tilemap;              /* 0x4AAD08 */

extern void TileMap_InvalidateRect(
    void* tilemap, int left, int top, int right, int bottom);
extern void DDRAW_GetDdrawErrorString(int hresult);

void __cdecl DDRAW_PresentRect(
    RECT* pRect,
    HWND hWnd,
    int* viewport_xy,
    char use_ck)
{
    /* Copy source rect */
    RECT src_rect = *pRect;

    /* Early exit if empty rect */
    if (IsRectEmpty(&src_rect)) {
        return;
    }

    RECT dest_rect = *pRect;

    /* Apply viewport scroll offset if provided */
    if (viewport_xy) {
        OffsetRect(&dest_rect, -viewport_xy[0], -viewport_xy[1]);
    }

    /* Convert client coords to screen coords */
    POINT client_origin = { 0, 0 };
    ClientToScreen(hWnd, &client_origin);
    OffsetRect(&dest_rect, client_origin.x, client_origin.y);

    /* Intersect with window rect */
    RECT window_rect;
    GetWindowRect(hWnd, &window_rect);

    RECT clip_rect;
    if (!IntersectRect(&clip_rect, &dest_rect, &window_rect)) {
        return;  /* Completely clipped away */
    }

    int result;

    if (use_ck == 0) {
        /* Blit without color key (vtable[0x14] = BltFast or Blt) */
        result = ((int (*)(void*, RECT*, void*, RECT*, int, void*))(
            *(void***)g_backbuffer)[0x14])(
            g_backbuffer, &src_rect,
            g_primary_surface, &dest_rect,
            0x1000000, 0);  /* DDBLT_WAIT */

        /* Handle surface loss */
        if (result == -0x7789FE3E) {  /* DDERR_SURFACELOST */
            result = ((int (*)(void*))(*(void***)g_backbuffer)[0x6C])(
                g_backbuffer);  /* Restore */
            if (result != 0) goto error;

            /* Retry blit */
            result = ((int (*)(void*, RECT*, void*, RECT*, int, void*))(
                *(void***)g_backbuffer)[0x14])(
                g_backbuffer, &src_rect,
                g_primary_surface, &dest_rect,
                0x1000000, 0);

            /* Invalidate viewport after surface restore */
            TileMap_InvalidateRect(g_tilemap,
                g_viewport_rect_left, g_viewport_rect_top,
                g_viewport_rect_right, g_viewport_rect_bottom);
        }
    } else {
        /* Blit with color key */
        result = ((int (*)(void*, RECT*, void*, RECT*, int, void*))(
            *(void***)g_backbuffer)[0x14])(
            g_backbuffer, &src_rect,
            g_primary_surface, &dest_rect,
            0x1000000, 0);

        if (result == -0x7789FE3E) {  /* DDERR_SURFACELOST */
            result = ((int (*)(void*))(*(void***)g_backbuffer)[0x6C])(
                g_backbuffer);
            if (result != 0) goto error;

            result = ((int (*)(void*, RECT*, void*, RECT*, int, void*))(
                *(void***)g_backbuffer)[0x14])(
                g_backbuffer, &src_rect,
                g_primary_surface, &dest_rect,
                0x1000000, 0);

            TileMap_InvalidateRect(g_tilemap,
                g_viewport_rect_left, g_viewport_rect_top,
                g_viewport_rect_right, g_viewport_rect_bottom);
        } else if (result == 0) {
            return;
        }

        /* Second retry path with different flags */
        if (result != 0) {
            result = ((int (*)(void*, RECT*, void*, RECT*, int, void*))(
                *(void***)g_backbuffer)[0x14])(
                g_backbuffer, &src_rect,
                g_primary_surface, &dest_rect,
                0x1000000, 0);

            if (result == -0x7789FE3E) {
                result = ((int (*)(void*))(*(void***)g_backbuffer)[0x6C])(
                    g_backbuffer);
                if (result != 0) goto error;

                result = ((int (*)(void*, RECT*, void*, RECT*, int, void*))(
                    *(void***)g_backbuffer)[0x14])(
                    g_backbuffer, &src_rect,
                    g_primary_surface, &dest_rect,
                    0x1000000, 0);

                TileMap_InvalidateRect(g_tilemap,
                    g_viewport_rect_left, g_viewport_rect_top,
                    g_viewport_rect_right, g_viewport_rect_bottom);
            }
        }
    }

    if (result == 0) {
        return;
    }

error:
    DDRAW_GetDdrawErrorString(result);
}
