/**
 * DDRAW_BlitHBITMAPToSurface — Blit an HBITMAP onto a DirectDraw surface
 * Address: 0x401170
 * Size: 269 bytes (0x401170 — 0x40127B)
 * Calling convention: __cdecl
 *
 * Uses GDI to blit a loaded HBITMAP onto a DirectDraw surface:
 * 1. Restore() — unconditional vtable[27] (byte offset 0x6C), no-arg call
 * 2. CreateCompatibleDC(NULL) + SelectObject(hdc, hBitmap)
 * 3. GetObjectA(hBitmap, sizeof(BITMAP), &bmp) for native dimensions
 * 4. GetSurfaceDesc via vtable[22] (byte offset 0x58)
 * 5. GetDC via vtable[17] (byte offset 0x44) — 2 args, returns HRESULT
 * 6. Direct GDI StretchBlt (11 args, import at 0x477054)
 * 7. ReleaseDC via vtable[26] (byte offset 0x68)
 * 8. DeleteDC(mem_dc)
 * 9. Returns GetDC HRESULT (not StretchBlt result)
 *
 * Vtable slots used (IDirectDrawSurface7):
 *   [17] +0x44: GetDC(this, &hdc) → HRESULT
 *   [22] +0x58: GetSurfaceDesc(this, &ddsd) → HRESULT
 *   [26] +0x68: ReleaseDC(this, hdc) → HRESULT
 *   [27] +0x6C: Restore() → HRESULT (no args — called as ()(this))
 *
 * Called by: DDRAW_LoadBmpToSurface, CGWND texture loading
 *
 * @param ddraw_surf  IDirectDrawSurface7* target surface
 * @param hBitmap     HBITMAP handle
 * @param src_x       int16_t — source x-offset in bitmap (MOVSX at 0x401221)
 * @param src_y       int16_t — source y-offset in bitmap (MOVSX at 0x401219)
 * @param override_w  int — override dest width (0 = use bitmap width)
 * @param override_h  int — override dest height (0 = use bitmap height)
 * @return            HRESULT from GetDC (0 on success), or -0x7FFFBFFB on null args
 */
#include "../shared/types.h"

int __cdecl DDRAW_BlitHBITMAPToSurface(
    void* ddraw_surf,
    void* hBitmap,
    int16_t src_x,
    int16_t src_y,
    int override_w,
    int override_h)
{
    BITMAP bmp;
    int ddsd_buf[31];       /* DDSURFACEDESC2 (0x7C bytes) */
    HDC surf_dc;
    HDC mem_dc;
    int ddsd_width, ddsd_height;
    int result;

    if (hBitmap == 0 || ddraw_surf == 0) {
        return -0x7FFFBFFB;  /* Generic error (E_FAIL variant) */
    }

    /* Step 1: Restore surface — vtable[27] at byte offset 0x6C, no arguments.
     * Original: CALL [EAX + 0x6C] with no pushed args (Restore takes none). */
    ((HRESULT (*)(void*))(*(void***)ddraw_surf)[0x6C / 4])(ddraw_surf);

    /* Step 2: Create compatible memory DC, select bitmap into it */
    mem_dc = CreateCompatibleDC(NULL);
    if (mem_dc == NULL) {
        OutputDebugStringA("CreateCompatibleDC failed");
    }
    SelectObject(mem_dc, hBitmap);

    /* Step 3: Get native bitmap dimensions */
    GetObjectA(hBitmap, sizeof(BITMAP), &bmp);
    if (override_w == 0) {
        override_w = bmp.bmWidth;
    }
    if (override_h == 0) {
        override_h = bmp.bmHeight;
    }

    /* Step 4: GetSurfaceDesc — vtable[22] at byte offset 0x58.
     * Original: CALL [EDX + 0x58] with (this, &ddsd_buf). */
    memset(ddsd_buf, 0, sizeof(ddsd_buf));
    ddsd_buf[0] = 0x7C;  /* dwSize = sizeof(DDSURFACEDESC2) */
    ((HRESULT (*)(void*, int*))(*(void***)ddraw_surf)[0x58 / 4])(ddraw_surf, ddsd_buf);

    /* Extract surface width/height from DDSURFACEDESC2 */
    ddsd_width  = *(int*)((uint8_t*)ddsd_buf + 0x10);  /* +0x10 from ddsd_buf start */
    ddsd_height = *(int*)((uint8_t*)ddsd_buf + 0x0C);  /* +0x0C from ddsd_buf start */

    /* Step 5: GetDC — vtable[17] at byte offset 0x44.
     * Original: CALL [ECX + 0x44] with (this, &surf_dc). Returns HRESULT. */
    result = ((HRESULT (*)(void*, HDC*))(*(void***)ddraw_surf)[0x44 / 4])(ddraw_surf, &surf_dc);

    /* Step 6: StretchBlt — direct GDI call, only if GetDC succeeded.
     * Original: CALL [0x477054] (import table) with 11 arguments.
     * Source coords use MOVSX for signed 16-bit → 32-bit promotion. */
    if (result == 0) {
        StretchBlt(
            surf_dc,            /* hdcDest */
            0, 0,               /* xDest, yDest */
            ddsd_width,         /* wDest */
            ddsd_height,        /* hDest */
            mem_dc,             /* hdcSrc */
            (int)src_x,         /* xSrc — MOVSX from stack at 0x401221 */
            (int)src_y,         /* ySrc — MOVSX from stack at 0x401219 */
            override_w,         /* wSrc */
            override_h,         /* hSrc */
            0xCC0020);          /* SRCCOPY */

        /* Step 7: ReleaseDC — vtable[26] at byte offset 0x68.
         * Original: CALL [EDX + 0x68] with (this, surf_dc). */
        ((HRESULT (*)(void*, HDC))(*(void***)ddraw_surf)[0x68 / 4])(ddraw_surf, surf_dc);
    }

    /* Step 8: Cleanup memory DC */
    DeleteDC(mem_dc);

    /* Step 9: Return GetDC HRESULT (original: MOV EAX, [ESP + 0x14] at 0x40125E) */
    return result;
}
