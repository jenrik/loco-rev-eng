/**
 * DDRAW_BlitHBITMAPToSurface — Blit an HBITMAP onto a DirectDraw surface
 * Address: 0x401170
 * Size: 269 bytes
 * Calling convention: __cdecl
 *
 * Uses GDI to blit a loaded HBITMAP onto a DirectDraw surface.
 * Gets a DC from the surface (IDirectDrawSurface7::GetDC), creates
 * a compatible memory DC, selects the bitmap into it, queries surface
 * dimensions, then StretchBlts from the memory DC to the surface DC.
 * Returns 0 on success, negative error code on failure.
 *
 * Called by: DDRAW_LoadBmpToSurface, CGWND texture loading
 *
 * @param ddraw_surf  IDirectDrawSurface7* target surface
 * @param hBitmap     HBITMAP handle
 * @param unused      Unused parameter
 * @param override_w  Override width (0 = use bitmap width)
 * @param override_h  Override height (0 = use bitmap height)
 * @return            0 on success, or negative HRESULT on failure
 */
#include "../shared/types.h"

int __cdecl DDRAW_BlitHBITMAPToSurface(
    void* ddraw_surf,
    void* hBitmap,
    int unused,
    int override_w,
    int override_h)
{
    BITMAP bmp;
    int ddsd_buf[31];
    int result;

    if (hBitmap == 0 || ddraw_surf == 0) {
        return -0x7FFFBFFB;  /* Generic error */
    }

    /* Get DC from the DirectDraw surface (vtable[0x6C] = GetDC) */
    HDC surf_dc;
    result = ((int (*)(void*, HDC*))(*(void***)ddraw_surf)[0x6C])(
        ddraw_surf, &surf_dc);
    if (result != 0) {
        return result;
    }

    /* Create compatible memory DC */
    HDC mem_dc = CreateCompatibleDC(0);
    if (mem_dc == 0) {
        OutputDebugStringA("createcompatible dc failed\n");
        ((int (*)(void*, HDC))(*(void***)ddraw_surf)[0x68])(
            ddraw_surf, surf_dc);  /* ReleaseDC */
        return -1;
    }

    /* Select bitmap into memory DC */
    SelectObject(mem_dc, hBitmap);

    /* Get bitmap dimensions */
    GetObjectA(hBitmap, sizeof(BITMAP), &bmp);
    int bmp_w = (override_w != 0) ? override_w : bmp.bmWidth;
    int bmp_h = (override_h != 0) ? override_h : bmp.bmHeight;

    /* Get surface dimensions via GetSurfaceDesc (vtable[0x58]) */
    for (int i = 0; i < 31; i++) ddsd_buf[i] = 0;
    ddsd_buf[0] = 0x7C;
    ddsd_buf[1] = 6;  /* DDSD_HEIGHT | DDSD_WIDTH */
    ((int (*)(void*, int*))(*(void***)ddraw_surf)[0x58])(
        ddraw_surf, ddsd_buf);

    int surf_w = ddsd_buf[2];
    int surf_h = ddsd_buf[3];

    /* Blit via StretchBlt (SRCCOPY = 0xCC0020) */
    result = ((int (*)(void*, int, int, int, int, HDC, int, int, int, int, int))(
        *(void***)ddraw_surf)[0x44])(  /* vtable[0x44] = StretchBlt-like or Blt */
        ddraw_surf, 0, 0, surf_w, surf_h,
        mem_dc, 0, 0, bmp_w, bmp_h,
        0xCC0020);

    /* Release DCs */
    if (result == 0) {
        ((int (*)(void*, HDC))(*(void***)ddraw_surf)[0x68])(
            ddraw_surf, surf_dc);  /* ReleaseDC */
    }

    DeleteDC(mem_dc);
    return result;
}
