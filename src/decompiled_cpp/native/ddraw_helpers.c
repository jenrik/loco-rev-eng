/**
 * ddraw_helpers.c — DirectDraw helper free functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These are C free functions (not C++ methods) that wrap DirectDraw
 * operations: loading BMP files to surfaces, blitting HBITMAPs,
 * querying surface dimensions, and dimming surface rectangles.
 *
 * Original Ghidra names:
 *   DDRAW_LoadBmpToSurface      (0x401000) — __cdecl, 368 bytes
 *   DDRAW_BlitHBITMAPToSurface  (0x401170) — __cdecl, 269 bytes
 *   DDRAW_GetSurfaceWidthHeight (0x4014E0) — __cdecl, 82 bytes
 *   DDRAW_DimSurfaceRect        (0x401540) — __cdecl, 213 bytes
 *
 * See LOCOBITMAP.cpp for DDRAW_PresentRect (0x401280).
 */

#include <stdint.h>

/* ================================================================== */
/* Type shims — matching MSVC 32-bit Windows types                     */
/* ================================================================== */
typedef uint8_t   BYTE;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;
typedef int32_t   BOOL;
typedef int32_t   LONG;
typedef void*     HANDLE;
typedef void*     HDC;
typedef void*     HINSTANCE;
typedef void*     HWND;
typedef void*     HBITMAP;
typedef void*     HGDIOBJ;

/* DDSURFACEDESC header size */
#define DDSD_SIZE   0x7C

/* Surface type / creation flags */
#define DDSD_CAPS           0x0001
#define DDSD_HEIGHT         0x0002
#define DDSD_WIDTH          0x0004
#define DDSD_CAPS_HEIGHT_WIDTH (DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH)  /* 0x7 */

/* Blit flags */
#define DDBLT_WAIT          0x1000000
#define DDBLT_KEYSRC        0x200

/* ================================================================== */
/* Global variables (defined in other compilation units)                */
/* ================================================================== */

extern void*  g_ddraw;             /* 0x004FD190 — IDirectDraw4* */
extern void*  g_primary_surface;   /* 0x004FD3C4 — primary surface */
extern HDC    g_backbuffer;        /* 0x004FD3C0 — backbuffer surface (HDC overlay) */
extern HWND   g_main_window;       /* 0x004AA4A0 — main window handle */

/* Static DDSURFACEDESC reused across surface operations (at 0x4FD19C) */
extern DWORD  g_surface_desc_size; /* 0x4FD19C — dwSize */
extern LONG   g_surface_pitch;    /* 0x4FD1AC — lPitch (bytes/row) */
extern void*  g_surface_data;     /* 0x4FD1C0 — lpSurface */
extern BYTE   g_surface_lost;     /* 0x4FD218 — 0=surface ready, 1=locked/lost */

/* Dimming mask (halving mask for 16-bit RGB pixels, at 0x485280) */
extern WORD   g_surface_bshift;

/* ================================================================== */
/* Windows API imports (via IAT)                                       */
/* ================================================================== */

extern DWORD __stdcall GetFileAttributesA(const char* lpFileName);
extern HANDLE __stdcall LoadImageA(HINSTANCE hInst, const char* name,
                                   uint32_t type, int32_t cx, int32_t cy,
                                   uint32_t fuLoad);
extern BOOL  __stdcall GetObjectA(HANDLE hgdiobj, int32_t cbBuffer, void* lpvObject);
extern void  __stdcall OutputDebugStringA(const char* lpOutputString);
extern HDC   __stdcall CreateCompatibleDC(HDC hdc);
extern BOOL  __stdcall DeleteDC(HDC hdc);
extern HGDIOBJ __stdcall SelectObject(HDC hdc, HGDIOBJ hgdiobj);
extern BOOL  __stdcall StretchBlt(HDC hdcDest, int32_t xDest, int32_t yDest,
                                  int32_t wDest, int32_t hDest,
                                  HDC hdcSrc, int32_t xSrc, int32_t ySrc,
                                  int32_t wSrc, int32_t hSrc, DWORD dwRop);
extern BOOL  __stdcall DeleteObject(HANDLE hObject);

/* DDraw surface helpers */
extern int32_t __cdecl DDRAW_RestoreSurfaces(void* backbuffer, void* desc);
extern void    __cdecl DDRAW_GetDdrawErrorString(int32_t error);

/* ================================================================== */
/* DDRAW_LoadBmpToSurface — Load a BMP file to a DDraw surface         */
/* Address: 0x401000                                                   */
/* Size: 368 bytes                                                     */
/* Calling convention: __cdecl (caller pops stack)                     */
/*                                                                     */
/* Loads a BMP file from disk via LoadImageA, creates a DDraw          */
/* offscreen surface matching the bitmap dimensions (or override_*),   */
/* blits the HBITMAP to the surface via DDRAW_BlitHBITMAPToSurface,   */
/* and returns the DDraw surface pointer.                              */
/*                                                                     */
/* If initial surface creation fails and retry_fscreen is true,        */
/* retries with DDSD_SIZE | 0x4040 flags.                              */
/*                                                                     */
/* Called by: UIPANEL_StretchBlit (0x42AE37)                           */
/*                                                                     */
/* @param filename       path to BMP file to load                      */
/* @param unused         ignored parameter (reserved)                  */
/* @param override_w     override surface width (0 = use BMP width)    */
/* @param override_h     override surface height (0 = use BMP height)  */
/* @param retry_fscreen  if 1, retry CreateSurface on first failure    */
/* @return               IDirectDrawSurface pointer, or NULL on failure*/
/* ================================================================== */
void* __cdecl DDRAW_LoadBmpToSurface(const char* filename,
                                     int32_t unused,
                                     int32_t override_w,
                                     int32_t override_h,
                                     BYTE retry_fscreen)
{
    HBITMAP hBitmap;
    DDSURFACEDESC ddsd;    /* 0x7c bytes on stack */
    void* ddraw_surf;      /* receives created surface */
    int32_t result;

    /* Verify file exists */
    DWORD attr = GetFileAttributesA(filename);
    if (attr == 0xFFFFFFFF) {
        hBitmap = NULL;
    } else {
        /* Load bitmap from file */
        HINSTANCE hInst = *(HINSTANCE*)((BYTE*)g_main_window + 0xC); /* g_main_window->hInstance */
        hBitmap = (HBITMAP)LoadImageA(hInst, filename, 0,   /* IMAGE_BITMAP */
                                       override_w, override_h, 0x10); /* LR_DEFAULTCOLOR */
    }

    if (hBitmap == NULL) {
        return NULL;
    }

    /* Get BITMAP info (0x18-byte struct) */
    BITMAP bmp;
    GetObjectA(hBitmap, 0x18, &bmp);

    /* Prepare DDSURFACEDESC */
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize  = DDSD_SIZE;              /* 0x7C */
    ddsd.dwFlags = DDSD_CAPS_HEIGHT_WIDTH; /* 7 */

    if (override_w != 0) {
        ddsd.dwWidth  = override_w;
        ddsd.dwHeight = override_h;
    } else {
        ddsd.dwWidth  = bmp.bmWidth;       /* +0x08 from BITMAP base */
        ddsd.dwHeight = bmp.bmHeight;      /* +0x0C from BITMAP base */
    }

    /* Set DDSCAPS (dwCaps in DDSCAPS2 structure within DDSURFACEDESC) */
    ddsd.ddsCaps.dwCaps = 0x4040;  /* DDSCAPS_OFFSCREENPLAIN | ... */

    /* Create surface via g_ddraw->CreateSurface (vtable[6]) */
    result = ((int32_t (*)(void*, DDSURFACEDESC*, void**, void*))
              (*(void***)g_ddraw)[6])(g_ddraw, &ddsd, &ddraw_surf, NULL);

    /* Retry on failure if requested */
    if ((result != 0) && (retry_fscreen == 1)) {
        DDRAW_GetDdrawErrorString(result);

        /* Retry with different surface type (0x4040 instead of 0x7 | 0x4040) */
        DDSURFACEDESC ddsd2;
        memset(&ddsd2, 0, sizeof(ddsd2));
        ddsd2.dwSize  = DDSD_SIZE;
        ddsd2.dwFlags = 0x4040;   /* different creation flags */
        // width/height already in ddsd fields from first attempt

        result = ((int32_t (*)(void*, DDSURFACEDESC*, void**, void*))
                  (*(void***)g_ddraw)[6])(g_ddraw, &ddsd2, &ddraw_surf, NULL);

        if (result != 0) {
            OutputDebugStringA("DDINIT -- failed to create surface");
            return NULL;
        }
    }

    /* Restore surfaces and blit bitmap to surface */
    DDRAW_RestoreSurfaces(g_backbuffer, &ddsd);
    DDRAW_BlitHBITMAPToSurface(ddraw_surf, hBitmap, 0, 0, 0);
    DeleteObject(hBitmap);

    return ddraw_surf;
}

/* ================================================================== */
/* DDRAW_BlitHBITMAPToSurface — Blit HBITMAP to DDraw surface via GDI */
/* Address: 0x401170                                                   */
/* Size: 269 bytes                                                     */
/* Calling convention: __cdecl (caller pops stack)                     */
/*                                                                     */
/* Gets the DDraw surface's DC, creates a memory DC, selects the      */
/* HBITMAP, and performs StretchBlt to copy pixels. Uses StretchBlt   */
/* with SRCCOPY (0xCC0020). DDraw surface's GetDC (vtable[22]) and    */
/* ReleaseDC (vtable[26]) are used.                                    */
/*                                                                     */
/* Called by: DDRAW_LoadBmpToSurface (0x401152)                        */
/*                                                                     */
/* @param ddraw_surf    IDirectDrawSurface* to blit to                */
/* @param hBitmap       GDI HBITMAP source                             */
/* @param unused        ignored parameter (reserved)                   */
/* @param override_w    override source width (0 = use bitmap width)   */
/* @param override_h    override source height (0 = use bitmap height) */
/* @return               0 on success, 0x80004005 (E_FAIL) on NULL args*/
/*                      or DDraw error code from Lock()                */
/* ================================================================== */
int32_t __cdecl DDRAW_BlitHBITMAPToSurface(void* ddraw_surf,
                                           HBITMAP hBitmap,
                                           int32_t unused,
                                           int32_t override_w,
                                           int32_t override_h)
{
    BITMAP bmp;
    DDSURFACEDESC ddsd;  /* GetDC descriptor on stack */
    int32_t result;

    if ((hBitmap == NULL) || (ddraw_surf == NULL)) {
        return 0x80004005;  /* E_FAIL */
    }

    /* Restore surface (vtable[0x6C/4 = 27]) */
    ((int32_t (*)(void*))(*(void***)ddraw_surf)[27])(ddraw_surf);

    /* Create GDI memory DC compatible with screen */
    HDC hdcMem = CreateCompatibleDC(NULL);
    if (hdcMem == NULL) {
        OutputDebugStringA("createcompatible dc failed");
    }

    /* Select HBITMAP into memory DC */
    SelectObject(hdcMem, hBitmap);

    /* Read BITMAP struct (0x18 bytes) for dimensions */
    GetObjectA(hBitmap, 0x18, &bmp);

    /* Use override dimensions if specified */
    if (override_w == 0) {
        override_w = bmp.bmWidth;
    }
    if (override_h == 0) {
        override_h = bmp.bmHeight;
    }

    /* Prepare DDSURFACEDESC for GetDC */
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize  = DDSD_SIZE;
    ddsd.dwFlags = 6;  /* DDSD_HEIGHT | DDSD_WIDTH */

    /* Get DDraw surface DC (vtable[0x58/4 = 22]) */
    ((int32_t (*)(void*, DDSURFACEDESC*))(*(void***)ddraw_surf)[22])(ddraw_surf, &ddsd);

    /* Lock surface / get DC (vtable[0x44/4 = 17]) */
    result = ((int32_t (*)(void*, DDSURFACEDESC*))(*(void***)ddraw_surf)[17])(ddraw_surf, &ddsd);

    if (result == 0) {
        /* StretchBlt: memory DC (bitmap) → surface DC */
        StretchBlt(ddsd.lpSurface,  /* surface DC */
                   0, 0,
                   ddsd.dwWidth, ddsd.dwHeight,
                   hdcMem,
                   (int16_t)bmp.bmWidth, (int16_t)bmp.bmHeight,
                   override_w, override_h,
                   0xCC0020);  /* SRCCOPY */

        /* Release DC (vtable[0x68/4 = 26]) */
        ((int32_t (*)(void*, HDC))(*(void***)ddraw_surf)[26])(ddraw_surf, ddsd.lpSurface);
    }

    DeleteDC(hdcMem);
    return result;
}

/* ================================================================== */
/* DDRAW_GetSurfaceWidthHeight — Get DDraw surface dimensions          */
/* Address: 0x4014E0                                                   */
/* Size: 82 bytes                                                      */
/* Calling convention: __cdecl (caller pops stack)                     */
/*                                                                     */
/* Calls IDirectDrawSurface::GetSurfaceDesc (vtable[0x58/4 = 22]) and */
/* copies the dwWidth/dwHeight fields as 16-bit values to the caller's */
/* output pointers.                                                    */
/*                                                                     */
/* Called by: GameWindow_Create, Cursor_InitSprites,                   */
/*            Cursor_SetupSurface, DDRAW_GetSurface, UIPANEL_StretchBlit*/
/*                                                                     */
/* @param ddraw_surf   IDirectDrawSurface* to query                    */
/* @param out_width    receives low WORD of dwWidth (can be NULL)      */
/* @param out_height   receives low WORD of dwHeight (can be NULL)     */
/* ================================================================== */
void __cdecl DDRAW_GetSurfaceWidthHeight(void* ddraw_surf,
                                         uint16_t* out_width,
                                         uint16_t* out_height)
{
    DDSURFACEDESC ddsd;

    /* Zero-local and set dwSize */
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = DDSD_SIZE;  /* 0x7C */

    if (ddraw_surf != NULL) {
        /* Get surface desc via vtable[22] */
        ((int32_t (*)(void*, DDSURFACEDESC*))(*(void***)ddraw_surf)[22])(ddraw_surf, &ddsd);

        /* Write width and height (low WORDs only) */
        if (out_width != NULL) {
            *out_width  = (uint16_t)ddsd.dwWidth;
        }
        if (out_height != NULL) {
            *out_height = (uint16_t)ddsd.dwHeight;
        }
    }
}

/* ================================================================== */
/* DDRAW_DimSurfaceRect — Dim (halve) pixels in a surface rectangle    */
/* Address: 0x401540                                                   */
/* Size: 213 bytes                                                     */
/* Calling convention: __cdecl (caller pops stack)                     */
/*                                                                     */
/* Dims a rectangular region on the primary surface by locking it and  */
/* halving each 16-bit pixel: pixel = (pixel >> 1) & g_surface_bshift. */
/*                                                                     */
/* Uses a STATIC DDSURFACEDESC at 0x4FD19C (g_surface_desc/g_pitch/    */
/* g_surface_data/g_surface_lost) rather than a stack-allocated one.   */
/*                                                                     */
/* Called by: RESDATA_DispatchEvent (2 call sites at 0x454983,         */
/*            0x4549CC), for dimming entity regions when menus show.   */
/*                                                                     */
/* @param left    left edge of dim rectangle                           */
/* @param top     top edge of dim rectangle                            */
/* @param right   right edge of dim rectangle                          */
/* @param bottom  bottom edge of dim rectangle                         */
/* @return        TRUE (1) — always returns success                    */
/* ================================================================== */
BYTE __cdecl DDRAW_DimSurfaceRect(int32_t left, int32_t top,
                                  int32_t right, int32_t bottom)
{
    int32_t width, height;
    WORD* pixel_ptr;
    int32_t pitch_div2;   /* surface pitch / 2 (WORD units) */

    /* Lock primary surface if not already locked */
    if (g_surface_lost == 0) {
        /* Zero the static DDSURFACEDESC (0x4FD19C, 0x7C bytes = 31 DWORDS) */
        DWORD* desc_words = (DWORD*)&g_surface_desc_size;
        for (int i = 0; i < 31; i++) {
            desc_words[i] = 0;
        }
        g_surface_desc_size = DDSD_SIZE;  /* 0x4FD19C = 0x7C */

        /* Lock primary surface via vtable[0x64/4 = 25] */
        int32_t lock_result = ((int32_t (*)(void*, void*, DWORD*, DWORD, void*))
            (*(void***)g_primary_surface)[25])(g_primary_surface, NULL,
                                                (DWORD*)&g_surface_desc_size, 0, NULL);
        if (lock_result == 0) {
            g_surface_lost = 1;  /* locked */
        }
    }

    /* Compute pitch in WORD units (pitch >> 1) */
    pitch_div2 = (g_surface_pitch >> 1) & 0xFFFF;

    /* Calculate starting pixel address in the locked surface */
    pixel_ptr = (WORD*)((BYTE*)g_surface_data + top * g_surface_pitch + left * 2);

    /* Rect dimensions */
    width  = (right - left) & 0xFFFF;
    height = (bottom - top) & 0xFFFF;

    /* Process each row in the rectangle */
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            /* Dim: halve the pixel value and mask with bs (bitshift mask) */
            *pixel_ptr = (*pixel_ptr >> 1) & g_surface_bshift;
            pixel_ptr++;
        }
        /* Move to next row (surface pitch may be wider than rect) */
        pixel_ptr += (pitch_div2 - width);
    }

    /* Unlock surface if it was previously locked */
    if (g_surface_lost != 0) {
        int32_t unlock_result = ((int32_t (*)(void*, void*))
            (*(void***)g_primary_surface)[32])(g_primary_surface, NULL);
        if (unlock_result == 0) {
            g_surface_lost = 0;
        }
    }

    return 1;  /* TRUE */
}
