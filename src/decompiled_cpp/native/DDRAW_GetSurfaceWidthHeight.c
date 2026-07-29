/**
 * DDRAW_GetSurfaceWidthHeight — Get surface dimensions via GetSurfaceDesc
 * Address: 0x4014E0
 * Size: 82 bytes
 * Calling convention: __cdecl
 *
 * Fills a DDSURFACEDESC struct for a DirectDraw surface and extracts
 * the width and height as 16-bit values. Returns width in param_1
 * (low word) and height via the return address high word.
 *
 * Called by: DDRAW_DimSurfaceRect, UIPANEL surface query code
 *
 * @param surface  IDirectDrawSurface7*
 *                 On return: Low word = surface width
 *                            Return value high word = surface height
 */
#include "../shared/types.h"

void __cdecl DDRAW_GetSurfaceWidthHeight(int* surface)
{
    int ddsd_buf[31];        /* DDSURFACEDESC buffer */
    for (int i = 0; i < 31; i++) {
        ddsd_buf[i] = 0;
    }
    ddsd_buf[0] = 0x7C;     /* dwSize = sizeof(DDSURFACEDESC) */

    if (surface != 0) {
        /* Call IDirectDrawSurface7::GetSurfaceDesc (vtable[0x58]) */
        ((int (*)(void*, int*))(*(void***)surface)[0x58])(
            surface, ddsd_buf);

        /* Write height (low word of ddsd_buf[3]) through return address */
        *(unsigned short*)((uint8_t*)__builtin_return_address(0) + 2) =
            (unsigned short)ddsd_buf[3];  /* dwHeight */

        /* Write width to the surface pointer's low word */
        *(unsigned short*)surface = (unsigned short)ddsd_buf[2];  /* dwWidth */
    }
}
