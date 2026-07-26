/**
 * DDRAW_LoadBmpToSurface — Load a BMP file to a DirectDraw surface
 * Address: 0x401000
 * Size: 368 bytes
 * Calling convention: __cdecl
 *
 * Loads a bitmap file from disk into a DirectDraw surface. Creates a
 * new surface via IDirectDraw7::CreateSurface, blits the HBITMAP onto
 * it via GDI StretchBlt, then returns the surface handle.
 *
 * Called by: DDRAW_GetSurface (surface creation path), CGWND setup
 *
 * @param filename    Path to BMP file
 * @param unused      Unused second parameter
 * @param override_w  Override width (0 = use bitmap dimensions)
 * @param override_h  Override height (0 = use bitmap dimensions)
 * @param retry_fscreen  Retry with fullscreen surface on failure
 * @return            IDirectDrawSurface7*, or NULL on failure
 */
#include "../shared/types.h"

extern int*    g_ddraw;               /* 0x4A9908  IDirectDraw7* */
extern void*  g_main_window;          /* defined by the SDL entry point */
extern char*  DDRAW_GetDdrawErrorString(int hresult);  /* 0x45BBC0 */
extern void   DDRAW_RestoreSurfaces(int* surface, int* desc);
extern int    DDRAW_BlitHBITMAPToSurface(void* ddraw_surf, void* hBitmap,
                                          int unused, int w, int h);

/* 0x465CE0 */
extern void* __cdecl operator_new(size_t size);
extern void  __cdecl GLOBAL_free(void* ptr);

int* __cdecl DDRAW_LoadBmpToSurface(
    const char* filename,
    int unused,
    int override_w,
    int override_h,
    char retry_fscreen)
{
    int ddsd_buf[31];        /* DDSURFACEDESC buffer (0x7C bytes) */
    int* surface = 0;

    /* Check if file exists */
    unsigned long attr = GetFileAttributesA(filename);
    if (attr == 0xFFFFFFFF) {
        return 0;
    }

    /* Load bitmap via LoadImageA */
    void* hBitmap = LoadImageA(
        *(void**)((byte*)g_main_window + 0xC), /* hInstance at +0x0C */
        filename,
        0,           /* IMAGE_BITMAP */
        override_w,
        override_h,
        0x10);       /* LR_LOADFROMFILE */
    if (hBitmap == 0) {
        return 0;
    }

    /* Get bitmap dimensions */
    BITMAP bmp;
    GetObjectA(hBitmap, sizeof(BITMAP), &bmp);

    /* Clear DDSURFACEDESC */
    for (int i = 0; i < 31; i++) {
        ddsd_buf[i] = 0;
    }
    ddsd_buf[0] = 0x7C;                /* dwSize */
    ddsd_buf[1] = 7;                    /* dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH */

    /* Set dimensions */
    if (override_w == 0) {
        ddsd_buf[2] = bmp.bmWidth;     /* dwWidth */
        ddsd_buf[3] = bmp.bmHeight;    /* dwHeight */
    } else {
        ddsd_buf[2] = override_w;
        ddsd_buf[3] = override_h;
    }

    /* Create offscreen surface */
    int retry_flags = 0x4040;  /* DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY */
    int result = ((int (*)(void*, int*, void**, void*))(
        *(void***)g_ddraw)[6])(  /* IDirectDraw7::CreateSurface */
        g_ddraw, ddsd_buf, &surface, 0);

    if (result != 0 && retry_fscreen == 1) {
        DDRAW_GetDdrawErrorString(result);

        /* Retry with video memory surface */
        retry_flags = 0x4040;
        result = ((int (*)(void*, int*, void**, void*))(
            *(void***)g_ddraw)[6])(
            g_ddraw, ddsd_buf, &surface, 0);

        if (result != 0) {
            OutputDebugStringA("DDINIT - failed to create surface\n");
            return 0;
        }
    }

    /* Restore surface and blit bitmap onto it */
    DDRAW_RestoreSurfaces(surface, ddsd_buf);
    DDRAW_BlitHBITMAPToSurface(surface, hBitmap, 0, 0, 0);

    /* Clean up bitmap */
    DeleteObject(hBitmap);

    return surface;
}
