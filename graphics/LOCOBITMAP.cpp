/**
 * LOCOBITMAP.cpp — DDRAW_PresentRect implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * SEE LOCOBITMAP.h for naming disambiguation.
 *
 * RESOLVED 2026-08-17: this file used to also contain a full, flat
 * (non-inheriting) `class PostcardAlbum` implementation that competed with
 * the real, UI_WindowBase-deriving PostcardAlbum in ui/PostcardAlbum.h/.cpp.
 * Both definitions mangled identically for several method names (a
 * duplicate-symbol / silent-misbind hazard). The flat class and every
 * `PostcardAlbum::` method body that lived here have been deleted; the
 * anonymous-namespace helpers that existed only to support them
 * (sprite_rect/sprite_contains/destroy_allocated_sprite/destroy_resource/
 * resource_surface) were removed alongside them since they had no other
 * caller in this file. ui/PostcardAlbum.h/.cpp is now the sole
 * PostcardAlbum implementation.
 *
 * This file now contains only:
 *   DDRAW_PresentRect — DDraw blit/present helper (free function, 0x401280)
 *
 * UIPANEL_Surface's own methods (construction/destruction/copy and the
 * tile-rendering method group) are NOT implemented here — see
 * graphics/UIPANEL_Surface_lifecycle.cpp and town/TownTiles.cpp.
 */

// Status: VALIDATED

#include "LOCOBITMAP.h"
#include "../platform/ddraw_interfaces.h"

/* ================================================================== */
/* External references (DDRAW_PresentRect only)                        */
/* ================================================================== */

extern "C" {
    void  DDRAW_GetDdrawErrorString(int32_t error);            /* @0x45BBC0 */
    BOOL  IsRectEmpty(const RECT* rect);                       /* @0x45B940 */
    void  OffsetRect(RECT* rect, int32_t dx, int32_t dy);     /* @0x45B960 - indirect via 0x477378 */
    void  ClientToScreen(HWND hWnd, POINT* pt);               /* @0x45B980 - indirect via 0x477374 */
    void  GetWindowRect(HWND hWnd, RECT* rect);               /* @0x45B990 - indirect via 0x47737C */
    BOOL  IntersectRect(RECT* dst, const RECT* src1, const RECT* src2); /* @0x45B940 - via 0x47726C */
}

/* TileMap_InvalidateRect: real def is the inline TileMap* overload in
 * world/tilemap.h (`TileMap_InvalidateRect(TileMap*, int, int, int, int)`). */
class TileMap;
void  TileMap_InvalidateRect(TileMap* self, int32_t left, int32_t top,
                              int32_t right, int32_t bottom);           /* @0x455840 */

/* Tilemap & viewport globals for DDRAW_PresentRect */
extern void*   g_tilemap;                                          /* @0x004FD0C8 */
extern int32_t g_viewport_rect_left;    /* 0x004851D0 -- viewport rect */
extern int32_t g_viewport_rect_top;     /* 0x004851D4 */
extern int32_t g_viewport_rect_right;   /* 0x004851DC */
extern int32_t g_viewport_rect_bottom;  /* 0x004851E0 */

extern void*   g_backbuffer;                /* 0x004FD3C0 -- DDraw backbuffer surface */
extern void*   g_primary_surface;           /* 0x004FD3C4 -- DDraw primary surface */

/* ================================================================== */
/* DDRAW_PresentRect — DDraw present/blit helper (free function)       */
/* Address: 0x401280                                                   */
/* ================================================================== */
void __cdecl DDRAW_PresentRect(const RECT* rect, HWND hWnd, int32_t offset_xy[2],
                               uint8_t use_color_key)
{
    BOOL result;
    RECT blit_rect;
    RECT window_rect;
    RECT clipped_rect;
    POINT screen_offset;
    int32_t blit_flags;

    /* Save original rect dimensions for DDraw blit source */
    RECT src_rect;
    src_rect.left   = rect->left;     /* +0x00 */
    src_rect.top    = rect->top;      /* +0x04 */
    src_rect.right  = rect->right;    /* +0x08 */
    src_rect.bottom = rect->bottom;   /* +0x0C */

    /* Return early if rect is empty */
    result = IsRectEmpty(rect);                  /* @0x45B940 (via 0x477268) */
    if (result != 0) {
        return;
    }

    /* Copy the rect for blitting, apply optional offset */
    blit_rect.left   = rect->left;
    blit_rect.top    = rect->top;
    blit_rect.right  = rect->right;
    blit_rect.bottom = rect->bottom;

    if (offset_xy != nullptr) {
        OffsetRect(&blit_rect, -offset_xy[0], -offset_xy[1]);  /* @0x45B960 (via 0x477378) */
    }

    /* Convert client coordinates to screen coordinates */
    screen_offset.x = 0;
    screen_offset.y = 0;
    ClientToScreen(hWnd, &screen_offset);        /* @0x45B980 (via 0x477374) */
    OffsetRect(&blit_rect, screen_offset.x, screen_offset.y);

    /* Get the window rect and clip the blit rect against it */
    GetWindowRect(hWnd, &window_rect);           /* @0x45B990 (via 0x47737C) */
    result = IntersectRect(&clipped_rect, &blit_rect, &window_rect);  /* @0x45B940 (via 0x47726C) */
    if (result == 0) {
        return;  /* Nothing visible — rect is outside window */
    }

    /* Determine blit flags */
    if (use_color_key == 0) {
        blit_flags = 0x1000000;  /* DDBLT_WAIT */
    } else {
        blit_flags = 0x200;      /* DDBLT_KEYSRC */
    }

    /* Perform the Blt from primary surface to backbuffer. */
    IDirectDrawSurface4* backbuffer = static_cast<IDirectDrawSurface4*>(g_backbuffer);
    IDirectDrawSurface4* primary = static_cast<IDirectDrawSurface4*>(g_primary_surface);
    int32_t blt_result = backbuffer->Blt(
        &blit_rect, primary, &src_rect, blit_flags, nullptr);

    if (blt_result == 0x887601C2) {  /* DDERR_SURFACELOST */
        /* Surface was lost — restore it and retry */
        int32_t restore_result = backbuffer->Restore();

        if (restore_result != 0) {
            goto error;  /* Surface restore failed */
        }

        /* Retry the blit with DDBLT_WAIT. */
        blt_result = backbuffer->Blt(
            &blit_rect, primary, &src_rect, 0x1000000, nullptr);

        if (blt_result == 0) {
            /* Success — invalidate viewport for full redraw */
            TileMap_InvalidateRect(static_cast<TileMap*>(g_tilemap),
                g_viewport_rect_left, g_viewport_rect_top,
                g_viewport_rect_right, g_viewport_rect_bottom);  /* @0x455840 */
        }
    } else if (use_color_key != 0 && blt_result != 0) {
        /* With color key and non-SURFACELOST error, retry with DDBLT_WAIT. */
        blt_result = backbuffer->Blt(
            &blit_rect, primary, &src_rect, 0x1000000, nullptr);

        if (blt_result == 0x887601C2) {
            /* Surface lost again on retry */
            int32_t restore_result = backbuffer->Restore();
            if (restore_result != 0) {
                goto error;
            }
            blt_result = backbuffer->Blt(
                &blit_rect, primary, &src_rect, 0x1000000, nullptr);
        }
    }

    if (blt_result == 0) {
        return;  /* Success */
    }

error:
    DDRAW_GetDdrawErrorString(blt_result);       /* @0x45BBC0 */
}

/* ================================================================== */
/* UIPANEL_Surface management functions                                */
/*                                                                      */
/* Construction/destruction/copy (0x42A110/0x42A140/0x42A1C0) and the   */
/* UIPANEL_Surface_New() factory live in                                */
/* graphics/UIPANEL_Surface_lifecycle.cpp -- a separate, minimal-        */
/* dependency translation unit so narrow test executables and           */
/* resources/sprite_uipanel_adapter.cpp don't need this file's much     */
/* larger Win32 dependency graph just to construct one.                 */
/* ================================================================== */
