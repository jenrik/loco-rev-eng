/*
 * locobitmap.c  -  LOCOBITMAP DirectDraw surface subsystem
 * Lego Loco (1998), reconstructed from Ghidra decompilation.
 *
 * LOCOBITMAP wraps IDirectDrawSurface for offscreen bitmap management.
 *   g_pDirectDraw         @ 0x00485440   global IDirectDraw*
 *   g_pPrimarySurface     @ 0x004fd3c0   screen / primary surface
 *   g_pBackSurface        @ 0x004fd3c4   offscreen back-buffer
 *   DDSURFACEDESC.dwSize  = 0x7c (124 bytes)
 *   DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY = 0x4040
 *   DDERR_SURFACELOST     = 0x887601C2
 *   DDBLT_ASYNC           = 0x1000000
 *
 * PORTING NOTES (Win32 -> Linux/SDL2):
 *   IDirectDraw4 / IDirectDrawSurface*  ->  SDL_Surface* + SDL_Texture*
 *   GDI HBITMAP + GetDC/StretchBlt      ->  SDL_LoadBMP + SDL_BlitScaled
 *   CreateFileA / WriteFile             ->  fopen / fwrite / fclose
 *   DeleteFileA                         ->  unlink()
 *   operator new  (FUN_00465ce0)        ->  malloc()
 *   operator delete (FUN_00465cd0)      ->  free()
 *   GetFileAttributesA                  ->  access(path, F_OK)
 *   Surface lost / Restore()            ->  SDL_RENDER_TARGETS_RESET event
 */

#include "locobitmap.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════════════════════════════
 * GLOBAL VARIABLE DEFINITIONS
 *
 * Original PE virtual addresses noted for cross-reference with Ghidra.
 * On Linux these become plain C globals initialised during engine startup.
 * ══════════════════════════════════════════════════════════════════════ */

/*
 * 0x00485440  g_pDirectDraw / g_pRenderer
 * Global IDirectDraw4* used to create all surfaces via vtable[0x18].
 * Linux: SDL_Renderer*; all texture/surface creation goes through it.
 */
#ifdef LOCO_LINUX
SDL_Renderer  *g_pRenderer      = NULL;
#else
void          *g_pDirectDraw    = NULL;
#endif

/*
 * 0x004fd3c0  g_pPrimarySurface / g_pPrimaryTexture
 * Primary (front-buffer / screen) DirectDraw surface.
 * Linux: the SDL_Renderer default render target; SDL_RenderPresent pushes
 * the back-buffer to the screen.
 */
#ifdef LOCO_LINUX
SDL_Texture   *g_pPrimaryTexture = NULL;
#else
void          *g_pPrimarySurface = NULL;
#endif

/*
 * 0x004fd3c4  g_pBackBuffer / g_pBackSurface + g_pBackTexture
 * Offscreen back-buffer.  Source in BlitToScreen; locked in DarkenRect.
 * Linux: SDL_Texture (SDL_TEXTUREACCESS_TARGET) paired with an SDL_Surface
 * for CPU-side pixel access.
 */
#ifdef LOCO_LINUX
SDL_Surface   *g_pBackSurface = NULL;
SDL_Texture   *g_pBackTexture = NULL;
#else
void          *g_pBackBuffer  = NULL;
#endif

/* 0x004fd1c0  Pointer to locked surface pixel data (uint16_t*, 16-bit RGB565). */
uint16_t      *g_pSurfaceBits    = NULL;

/* 0x004fd1ac  Surface byte stride per row after Lock. */
uint32_t       g_nSurfacePitch   = 0;

/* 0x004fd218  1 = back-buffer is currently locked; prevents redundant Locks. */
char           g_bSurfaceLocked  = 0;

/* 0x00485280  Per-channel isolation mask: 0x7BEF (RGB565) or 0x3DEF (RGB555). */
uint16_t       g_nPixelChannelMask = PIXEL_CHANNEL_MASK_RGB565;

/* 0x004855f4  Font handle used by LocoBitmapChain_RefreshGrid for DrawTextA. */
HANDLE         g_labelFont       = NULL;

/* 0x004aa4a0  Application context struct. */
AppContext     *g_pAppInstance   = NULL;

/* 0x004aad08  Dirty-rectangle tracking state. */
DirtyRectState *g_pDirtyRectState = NULL;

/* ══════════════════════════════════════════════════════════════════════
 * EXTERNAL HELPERS FROM OTHER SUBSYSTEMS
 *
 * These are called by the functions below but implemented elsewhere.
 * ══════════════════════════════════════════════════════════════════════ */

/* Error logger / reporter (logs HRESULT or error code). */
extern void FUN_0045bbc0(int errorCode);

/* Dirty-rect tracker called after each successful blit. */
extern void FUN_00455840(DirtyRectState *pState, int x, int y, int w, int h);

/* malloc / free wrappers (operator new / delete from MSVC runtime). */
extern void *FUN_00465ce0(unsigned int size);   /* malloc equivalent */
extern void  FUN_00465cd0(void *ptr);           /* free  equivalent  */

/* LocoBitmapChain base-class ctor / dtor. */
extern void FUN_00425870(void *self);  /* base ctor */
extern void FUN_00425910(void *self);  /* base dtor */

/* Present / flip the back-buffer to the screen. */
extern void FUN_00425990(void *self);

/* Create the window sized to the desktop. */
extern void FUN_00425b70(void *self, int flags, HWND hWndParent,
                          int x, int y, int w, int h,
                          void *reserved, HICON hIcon, int extra);

/* LOCOBITMAP destructor / free wrapper. */
extern void FUN_00454bc0(LOCOBITMAP *bm);

/* Set frame / slot visual state: mode 0=normal, 1=visible, 2=grayed. */
extern void FUN_00454c30(LOCOBITMAP *bm, int mode, void *param);

/* Acquire a draw resource token before DrawSlot. */
extern void FUN_00447930(int resId);

/* AW blit: blit srcRect from pSrc to pDst at dstRect. */
extern void FUN_0042b050(void *pSrc, void *pDst,
                          RECT *srcRect, RECT *dstRect);

/* Blit the rendered carousel to the screen. */
extern void FUN_00426b90(LocoBitmapChain *self);

/* AlbIndex resource lookup helpers. */
extern void FUN_00445930(int id, char *pathBuf);  /* build resource path */
extern void *FUN_00444c70(const char *path);       /* resource lookup     */

/* _strupr replacement (FUN_00474c70 in original). */
extern char *FUN_00474c70(char *str);

/* Draw item bitmap into a slot rect. */
extern void FUN_004437c0(void *itemData, LOCOBITMAP *slot, RECT *r);

/* ══════════════════════════════════════════════════════════════════════
 * Batch 1 - LOCOBITMAP core functions        0x00401000 to 0x004016ff
 * ══════════════════════════════════════════════════════════════════════ */

/* ===========================================================================
 * LOCOBITMAP_LoadFromFile                                       0x00401000
 *
 * Load a BMP file and create a DirectDraw offscreen surface populated with
 * the image data.  Returns the IDirectDrawSurface* (Win32) or SDL surface/
 * texture (Linux), or NULL on failure.
 *
 * DDSCAPS selection:
 *   useVideoMem == 1  ->  0x4040  (DDSCAPS_VIDEOMEMORY | DDSCAPS_OFFSCREENPLAIN)
 *   useVideoMem != 1  ->  0x0840  (DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN)
 *   On video-mem failure:  fall back to 0x0840 and log via FUN_0045bbc0.
 * =========================================================================== */
void *LOCOBITMAP_LoadFromFile(void *param_1,
                               const char *filePath,
                               int desiredWidth,
                               int desiredHeight,
                               int useVideoMem)
{
    (void)param_1;

    /*
     * STEP 1: Verify file exists.
     * WIN32: GetFileAttributesA(filePath) == 0xFFFFFFFF  ->  missing
     * LINUX: access(filePath, F_OK) != 0  ->  missing
     */
#ifdef LOCO_LINUX
    if (access(filePath, F_OK) != 0) {
        return NULL;
    }
#else
    if (GetFileAttributesA(filePath) == 0xFFFFFFFF) {
        return NULL;
    }
#endif

    /*
     * STEP 2: Load the BMP into a platform bitmap object.
     * WIN32: LoadImageA(hModule, filePath, IMAGE_BITMAP=0, 0, 0, LR_LOADFROMFILE=0x10)
     * LINUX: SDL_LoadBMP(filePath)
     */
#ifdef LOCO_LINUX
    SDL_Surface *pSrcSurface = SDL_LoadBMP(filePath);
    if (!pSrcSurface) {
        return NULL;
    }
    int bitmapWidth  = pSrcSurface->w;
    int bitmapHeight = pSrcSurface->h;
#else
    HMODULE hModule = *(HMODULE *)((char *)g_pAppInstance + 0x0c);
    HBITMAP hBitmap = (HBITMAP)LoadImageA(hModule, filePath,
                                          0 /* IMAGE_BITMAP */,
                                          0, 0,
                                          0x10 /* LR_LOADFROMFILE */);
    if (!hBitmap) {
        return NULL;
    }
    /*
     * WIN32: GetObjectA(hBitmap, sizeof(BITMAP)=0x18, &bm)
     * BITMAP: bmWidth at +0x04, bmHeight at +0x08.
     */
    BITMAP bm;
    GetObjectA(hBitmap, BITMAP_STRUCT_SIZE /* 0x18 */, &bm);
    int bitmapWidth  = bm.bmWidth;
    int bitmapHeight = bm.bmHeight;
#endif

    /* STEP 3: Determine final surface dimensions. */
    int surfaceWidth  = (desiredWidth  != 0) ? desiredWidth  : bitmapWidth;
    int surfaceHeight = (desiredHeight != 0) ? desiredHeight : bitmapHeight;

    /*
     * STEP 4: Create the destination surface.
     *
     * WIN32: zero-init a 124-byte DDSURFACEDESC2, set dwSize=0x7c,
     * dwFlags=7, dwHeight, dwWidth, ddsCaps.dwCaps, then call
     * IDirectDraw4::CreateSurface via vtable[6] (offset 0x18).
     *
     * LINUX: SDL_LoadBMP already gave us an SDL_Surface.  Apply the
     * magenta colour key for transparency then optionally upload to GPU.
     */
#ifdef LOCO_LINUX
    SDL_SetColorKey(pSrcSurface, SDL_TRUE,
                    SDL_MapRGB(pSrcSurface->format, 255, 0, 255));

    void *pResult = NULL;
    if (useVideoMem) {
        /* Video memory -> GPU texture. */
        SDL_Texture *pTex = SDL_CreateTextureFromSurface(g_pRenderer, pSrcSurface);
        if (!pTex) {
            FUN_0045bbc0(0); /* log fallback */
            pResult = (void *)pSrcSurface; /* keep CPU surface */
        } else {
            SDL_FreeSurface(pSrcSurface);
            pResult = (void *)pTex;
        }
    } else {
        /* System memory -> keep as SDL_Surface. */
        pResult = (void *)pSrcSurface;
    }
    return pResult;

#else
    /* WIN32 path */
    DDSURFACEDESC2 desc;
    memset(&desc, 0, DDSURFACEDESC2_SIZE /* 0x7c */);
    desc.dwSize   = DDSURFACEDESC2_SIZE;
    desc.dwFlags  = DDSD_CAPS_HEIGHT_WIDTH;   /* 7 */
    desc.dwHeight = (DWORD)surfaceHeight;
    desc.dwWidth  = (DWORD)surfaceWidth;
    desc.ddsCaps.dwCaps = (useVideoMem == 1) ? DDSCAPS_OFFSCREENPLAIN_VIDEOMEM
                                             : DDSCAPS_OFFSCREENPLAIN_SYSMEM;

    typedef HRESULT (__stdcall *CreateSurfaceFn)(void *, DDSURFACEDESC2 *,
                                                  void **, void *);
    CreateSurfaceFn pfnCreate =
        (CreateSurfaceFn)(*(void ***)g_pDirectDraw)[VTBL_IDD4_CREATESURFACE];

    void *pNewSurface = NULL;
    HRESULT hr = pfnCreate(g_pDirectDraw, &desc, &pNewSurface, NULL);

    if (FAILED(hr) && useVideoMem == 1) {
        /* Video-mem failure: fall back to system memory. */
        FUN_0045bbc0((int)hr);
        desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN_SYSMEM;
        hr = pfnCreate(g_pDirectDraw, &desc, &pNewSurface, NULL);
        if (FAILED(hr)) {
            DeleteObject(hBitmap);
            return NULL;
        }
    }
    if (!pNewSurface) {
        DeleteObject(hBitmap);
        return NULL;
    }

    /* STEP 5: Blit GDI HBITMAP into the new DirectDraw surface. */
    LOCOBITMAP_BlitHBitmapToSurface(pNewSurface, (void *)hBitmap);

    /* STEP 6: Free the intermediate GDI bitmap. */
    DeleteObject(hBitmap);

    return pNewSurface;
#endif
}

/* ===========================================================================
 * LOCOBITMAP_BlitHBitmapToSurface                               0x00401170
 *
 * Copy a GDI HBITMAP (hBitmap) into an IDirectDrawSurface (pSurface) using
 * GetDC / StretchBlt / ReleaseDC.  Restores a lost surface first.
 * Returns HRESULT from GetDC (0 = success).
 * =========================================================================== */
int LOCOBITMAP_BlitHBitmapToSurface(void *pSurface, void *hBitmap)
{
    if (!pSurface || !hBitmap) {
        return 0x80040005; /* E_INVALIDARG sentinel */
    }

#ifdef LOCO_LINUX
    SDL_Surface *pDst = (SDL_Surface *)pSurface;
    SDL_Surface *pSrc = (SDL_Surface *)hBitmap;
    SDL_Rect dstRect  = { 0, 0, pDst->w, pDst->h };
    return SDL_BlitScaled(pSrc, NULL, pDst, &dstRect);

#else
    /*
     * WIN32: IDirectDrawSurface::Restore via vtable[0x6c]
     * Recovers a lost surface before any GDI operation on it.
     */
    typedef HRESULT (__stdcall *RestoreFn)(void *);
    RestoreFn pfnRestore =
        (RestoreFn)(*(void ***)pSurface)[VTBL_IDS_RESTORE];
    pfnRestore(pSurface);

    /* WIN32: CreateCompatibleDC(NULL) */
    HDC hMemDC = CreateCompatibleDC(NULL);
    if (!hMemDC) {
        OutputDebugStringA("LOCOBITMAP: CreateCompatibleDC failed\n");
        return -1;
    }

    /* WIN32: SelectObject(hMemDC, hBitmap) */
    HGDIOBJ hOldObj = SelectObject(hMemDC, (HBITMAP)hBitmap);

    /* WIN32: GetObjectA(hBitmap, 0x18, &bm) -> bmWidth @ +0x04, bmHeight @ +0x08 */
    BITMAP bm;
    memset(&bm, 0, BITMAP_STRUCT_SIZE);
    GetObjectA(hBitmap, BITMAP_STRUCT_SIZE, &bm);
    int srcWidth  = bm.bmWidth;
    int srcHeight = bm.bmHeight;

    /* WIN32: IDirectDrawSurface::GetSurfaceDesc via vtable[0x58] */
    DDSURFACEDESC2 desc;
    memset(&desc, 0, DDSURFACEDESC2_SIZE);
    desc.dwSize = DDSURFACEDESC2_SIZE;
    typedef HRESULT (__stdcall *GetSurfaceDescFn)(void *, DDSURFACEDESC2 *);
    GetSurfaceDescFn pfnGetDesc =
        (GetSurfaceDescFn)(*(void ***)pSurface)[VTBL_IDS_GETSURFACEDESC];
    pfnGetDesc(pSurface, &desc);
    int dstWidth  = (int)desc.dwWidth;
    int dstHeight = (int)desc.dwHeight;

    /* WIN32: IDirectDrawSurface::GetDC via vtable[0x44] */
    HDC hSurfDC = NULL;
    typedef HRESULT (__stdcall *GetDCFn)(void *, HDC *);
    GetDCFn pfnGetDC =
        (GetDCFn)(*(void ***)pSurface)[VTBL_IDS_GETDC];
    HRESULT hr = pfnGetDC(pSurface, &hSurfDC);

    if (hr == 0) {
        /* WIN32: StretchBlt(SRCCOPY = 0xcc0020) */
        StretchBlt(hSurfDC, 0, 0, dstWidth, dstHeight,
                   hMemDC,  0, 0, srcWidth, srcHeight,
                   SRCCOPY);

        /* WIN32: IDirectDrawSurface::ReleaseDC via vtable[0x68] */
        typedef HRESULT (__stdcall *ReleaseDCFn)(void *, HDC);
        ReleaseDCFn pfnReleaseDC =
            (ReleaseDCFn)(*(void ***)pSurface)[VTBL_IDS_RELEASEDC];
        pfnReleaseDC(pSurface, hSurfDC);
    }

    SelectObject(hMemDC, hOldObj);
    DeleteDC(hMemDC);
    return (int)hr;
#endif
}

/* ===========================================================================
 * LOCOBITMAP_BlitToScreen                                       0x00401280
 *
 * Clip and blit a region from the offscreen back-buffer to the primary
 * (screen) surface.  Handles DDERR_SURFACELOST by calling Restore + retry.
 *
 * Coordinate flow (Win32):
 *   client rect -> OffsetRect by scroll origin -> ClientToScreen ->
 *   OffsetRect to screen space -> IntersectRect with window bounds ->
 *   submitted to IDirectDrawSurface::Blt on primary surface.
 * =========================================================================== */
void LOCOBITMAP_BlitToScreen(LocoRect *pSrcRect, void *hWnd,
                              int *pScrollOrigin, int bAsync)
{
#ifdef LOCO_LINUX
    if (!pSrcRect ||
        pSrcRect->right  <= pSrcRect->left ||
        pSrcRect->bottom <= pSrcRect->top) {
        return;
    }

    SDL_Rect srcRect = {
        pSrcRect->left,
        pSrcRect->top,
        pSrcRect->right  - pSrcRect->left,
        pSrcRect->bottom - pSrcRect->top
    };
    SDL_Rect dstRect = srcRect;

    if (pScrollOrigin) {
        dstRect.x += pScrollOrigin[0];
        dstRect.y += pScrollOrigin[1];
    }

    /* Clip against window dimensions (replaces GetWindowRect + IntersectRect). */
    int wW = 0, wH = 0;
    /* SDL_GetWindowSize(g_pWindow, &wW, &wH); */
    if (wW > 0 && wH > 0) {
        int x2 = (dstRect.x + dstRect.w < wW) ? dstRect.x + dstRect.w : wW;
        int y2 = (dstRect.y + dstRect.h < wH) ? dstRect.y + dstRect.h : wH;
        int x1 = (dstRect.x > 0) ? dstRect.x : 0;
        int y1 = (dstRect.y > 0) ? dstRect.y : 0;
        if (x2 <= x1 || y2 <= y1) return;
        dstRect.x = x1; dstRect.y = y1;
        dstRect.w = x2 - x1; dstRect.h = y2 - y1;
    }

    SDL_RenderCopy(g_pRenderer, g_pBackTexture, &srcRect, &dstRect);

    if (bAsync) {
        /* SDL_RenderFlush(g_pRenderer); */
    }

    if (g_pDirtyRectState) {
        FUN_00455840(g_pDirtyRectState,
                     dstRect.x, dstRect.y, dstRect.w, dstRect.h);
    }

#else
    /* WIN32 path */
    if (!pSrcRect || IsRectEmpty((RECT *)pSrcRect)) {
        return;
    }

    RECT srcRectW32 = { pSrcRect->left,  pSrcRect->top,
                        pSrcRect->right, pSrcRect->bottom };
    RECT dstRectW32 = srcRectW32;

    if (pScrollOrigin) {
        OffsetRect(&dstRectW32, pScrollOrigin[0], pScrollOrigin[1]);
    }

    POINT ptOrigin = { 0, 0 };
    ClientToScreen((HWND)hWnd, &ptOrigin);
    OffsetRect(&dstRectW32, ptOrigin.x, ptOrigin.y);

    RECT windowRect;
    GetWindowRect((HWND)hWnd, &windowRect);
    RECT clippedRect;
    if (!IntersectRect(&clippedRect, &dstRectW32, &windowRect)) {
        return;
    }

    typedef HRESULT (__stdcall *BltFn)(void *, RECT *, void *, RECT *,
                                        DWORD, void *);
    BltFn pfnBlt =
        (BltFn)(*(void ***)g_pPrimarySurface)[VTBL_IDS_BLT];

retry_blt:
    {
        HRESULT hr = pfnBlt(g_pPrimarySurface,
                            &clippedRect,
                            g_pBackBuffer,
                            &srcRectW32,
                            0, NULL);

        if (hr == (HRESULT)DDERR_SURFACELOST_APPROX) {
            /* WIN32: IDirectDrawSurface::Restore via vtable[0x6c] */
            typedef HRESULT (__stdcall *RestoreFn)(void *);
            RestoreFn pfnRestore =
                (RestoreFn)(*(void ***)g_pPrimarySurface)[VTBL_IDS_RESTORE];
            pfnRestore(g_pPrimarySurface);
            goto retry_blt;
        } else if (FAILED(hr)) {
            FUN_0045bbc0((int)hr);
            return;
        }
    }

    if (bAsync) {
        pfnBlt(g_pPrimarySurface, &clippedRect,
               g_pBackBuffer, &srcRectW32,
               DDBLT_ASYNC_FLAG, NULL);
    }

    if (g_pDirtyRectState) {
        FUN_00455840(g_pDirtyRectState,
                     clippedRect.left,
                     clippedRect.top,
                     clippedRect.right  - clippedRect.left,
                     clippedRect.bottom - clippedRect.top);
    }
#endif
}

/* ===========================================================================
 * LOCOBITMAP_GetSurfaceDesc                                     0x004014e0
 *
 * Query pixel dimensions of a DirectDraw surface.  Sets DDSURFACEDESC2.dwSize
 * before the call as required by the DirectDraw ABI.
 * =========================================================================== */
void LOCOBITMAP_GetSurfaceDesc(void *pSurface, int *outW, int *outH)
{
    if (!pSurface || !outW || !outH) return;

#ifdef LOCO_LINUX
    SDL_Surface *surf = (SDL_Surface *)pSurface;
    *outW = surf->w;
    *outH = surf->h;
    /* For SDL_Texture: SDL_QueryTexture(tex, NULL, NULL, outW, outH); */
#else
    DDSURFACEDESC2 desc;
    memset(&desc, 0, DDSURFACEDESC2_SIZE);
    desc.dwSize = DDSURFACEDESC2_SIZE;

    typedef HRESULT (__stdcall *GetSurfaceDescFn)(void *, DDSURFACEDESC2 *);
    GetSurfaceDescFn pfnGetDesc =
        (GetSurfaceDescFn)(*(void ***)pSurface)[VTBL_IDS_GETSURFACEDESC];
    pfnGetDesc(pSurface, &desc);

    *outW = (int)desc.dwWidth;
    *outH = (int)desc.dwHeight;
#endif
}

/* ===========================================================================
 * LOCOBITMAP_DarkenRect                                         0x00401540
 *
 * Darken a rectangle of 16-bit pixels on the back-buffer by 50%.
 * Per pixel: pixel = (pixel >> 1) & g_nPixelChannelMask (0x7BEF for RGB565).
 * Lazily locks the surface on the first call.
 *
 * Note: the original code keeps the surface locked across multiple consecutive
 * DarkenRect calls and only unlocks at the end.  For simplicity this
 * implementation always unlocks after each call.
 * =========================================================================== */
void LOCOBITMAP_DarkenRect(int x1, int y1, int x2, int y2)
{
    /* Lazy lock */
    if (!g_bSurfaceLocked) {
#ifdef LOCO_LINUX
        if (SDL_LockSurface(g_pBackSurface) != 0) return;
        g_pSurfaceBits   = (uint16_t *)g_pBackSurface->pixels;
        g_nSurfacePitch  = (uint32_t)g_pBackSurface->pitch;
#else
        DDSURFACEDESC2 lockDesc;
        memset(&lockDesc, 0, DDSURFACEDESC2_SIZE);
        lockDesc.dwSize = DDSURFACEDESC2_SIZE;

        typedef HRESULT (__stdcall *LockFn)(void *, RECT *, DDSURFACEDESC2 *,
                                             DWORD, HANDLE);
        LockFn pfnLock =
            (LockFn)(*(void ***)g_pBackBuffer)[VTBL_IDS_LOCK];
        HRESULT hr = pfnLock(g_pBackBuffer, NULL, &lockDesc,
                             1 /* DDLOCK_WAIT */, NULL);
        if (FAILED(hr)) return;

        g_pSurfaceBits  = (uint16_t *)lockDesc.lpSurface;
        g_nSurfacePitch = (uint32_t)lockDesc.lPitch;
#endif
        g_bSurfaceLocked = 1;
    }

    if (!g_pSurfaceBits) return;

    /*
     * Pixel loop: right-shift by 1 then mask to prevent inter-channel bleed.
     * pitch >> 1 converts byte-stride to 16-bit pixel stride.
     */
    uint32_t pixPerRow = g_nSurfacePitch >> 1;
    for (int y = y1; y < y2; y++) {
        uint16_t *pRow = g_pSurfaceBits + (uint32_t)y * pixPerRow;
        for (int x = x1; x < x2; x++) {
            pRow[x] = (uint16_t)((pRow[x] >> 1) & g_nPixelChannelMask);
        }
    }

    /* Unlock */
#ifdef LOCO_LINUX
    SDL_UnlockSurface(g_pBackSurface);
#else
    typedef HRESULT (__stdcall *UnlockFn)(void *, RECT *);
    UnlockFn pfnUnlock =
        (UnlockFn)(*(void ***)g_pBackBuffer)[VTBL_IDS_UNLOCK];
    pfnUnlock(g_pBackBuffer, NULL);
#endif
    g_pSurfaceBits   = NULL;
    g_bSurfaceLocked = 0;
}

/* ===========================================================================
 * LOCOBITMAP_Construct                                          0x00401620
 *
 * __fastcall constructor.  Seats the vtable and initialises all fields to
 * their "uninitialised / empty" sentinel values.
 * =========================================================================== */
void __fastcall LOCOBITMAP_Construct(LOCOBITMAP *self)
{
    if (!self) return;

    /* pThis->vtable = PTR_FUN_004773e8  (set by the C++ runtime / caller) */

    self->width       = LOCOBITMAP_UNINIT; /* -1: uninitialised sentinel     */
    self->frameBufPtr = 0;                 /* no frame-record buffer yet     */
    self->capacity    = 0;
    self->frameCount  = 0;
    self->savedCount  = LOCOBITMAP_UNINIT;
}

/* ===========================================================================
 * LOCOBITMAP_Destructor                                         0x00401650
 *
 * __thiscall destructor.  Releases the DirectDraw surface via vtable dispatch,
 * frees the frame-record heap buffer, and optionally frees the object itself.
 * =========================================================================== */
void LOCOBITMAP_Destructor(LOCOBITMAP *self, int deleteBit)
{
    if (!self) return;

    /* Re-seat vtable (MSVC re-entry guard pattern). */
    /* self->vtable = PTR_FUN_004773e8_base; */

    /*
     * Release the embedded IDirectDrawSurface via vtable dispatch.
     * Linux: SDL_DestroyTexture / SDL_FreeSurface stored in the sub-object.
     */
    if (self->vtable) {
        typedef void (__stdcall *ReleaseFn)(void *);
        /* vtable[2] = Release (IUnknown::Release index) */
        ((ReleaseFn)(self->vtable[2]))(self);
    }

    /* Free the frame-record heap buffer. */
    if (self->frameBufPtr) {
#ifdef LOCO_LINUX
        free((void *)(uintptr_t)self->frameBufPtr);
#else
        FUN_00465cd0((void *)(uintptr_t)self->frameBufPtr);
#endif
        self->frameBufPtr = 0;
        self->capacity    = 0;
        self->frameCount  = 0;
    }

    /* Optionally free the object itself. */
    if (deleteBit & 1) {
#ifdef LOCO_LINUX
        free(self);
#else
        FUN_00465cd0(self);
#endif
    }
}

/* ===========================================================================
 * LOCOBITMAP_InsertFrameRecord                                  0x00401690
 *
 * Insert a 24-byte frame record at position index in the LOCOBITMAP's
 * dynamic frame array.  Grows the buffer by one slot; shifts records up.
 *
 * Also called as AlbIndex_InsertAt: both classes share the same buffer-
 * management fields at +0x08/+0x0c/+0x10/+0x14 and the same record size.
 * =========================================================================== */
void LOCOBITMAP_InsertFrameRecord(LOCOBITMAP *self, int index,
                                   const FrameRecord *record)
{
    if (!self || !record) return;

    /* Save snapshot of frameCount before insert (savedCount field). */
    self->savedCount = self->frameCount;

    uint32_t oldCap   = (uint32_t)self->capacity;
    uint32_t newCap   = oldCap + FRAME_RECORD_SIZE;
    uint32_t oldCount = oldCap / FRAME_RECORD_SIZE;

#ifdef LOCO_LINUX
    FrameRecord *pNew = (FrameRecord *)malloc(newCap);
#else
    FrameRecord *pNew = (FrameRecord *)FUN_00465ce0(newCap);
#endif
    if (!pNew) return;

    FrameRecord *pOld = (FrameRecord *)(uintptr_t)self->frameBufPtr;

    /* Copy entries before insertion point. */
    if (pOld && (uint32_t)index > 0) {
        memcpy(pNew, pOld, (uint32_t)index * FRAME_RECORD_SIZE);
    }

    /* Insert new record at index. */
    memcpy(&pNew[index], record, FRAME_RECORD_SIZE);

    /* Shift remaining entries right by one slot. */
    if (pOld && (uint32_t)index < oldCount) {
        memcpy(&pNew[index + 1], &pOld[index],
               (oldCount - (uint32_t)index) * FRAME_RECORD_SIZE);
    }

    /* Free old buffer. */
    if (pOld) {
#ifdef LOCO_LINUX
        free(pOld);
#else
        FUN_00465cd0(pOld);
#endif
    }

    self->frameBufPtr = (int)(uintptr_t)pNew;
    self->capacity    = (int)newCap;
    self->frameCount  = (int)((uint32_t)index + 1);
}

/* ══════════════════════════════════════════════════════════════════════
 * Batch 2 - AlbIndex functions                0x00401760 to 0x00401ef0
 * ══════════════════════════════════════════════════════════════════════ */

/* ===========================================================================
 * AlbIndex_RemoveAt                                             0x00401760
 *
 * Remove the 24-byte entry at index from the in-memory section buffer.
 * Frees the buffer entirely when only one entry remains; otherwise copies
 * surviving entries into a new smaller buffer.
 * =========================================================================== */
void AlbIndex_RemoveAt(AlbIndex *self, int index)
{
    if (!self || !self->buffer || self->bufferBytes == 0) return;

    if (self->bufferBytes == FRAME_RECORD_SIZE) {
        /* Only one entry: free the buffer entirely. */
#ifdef LOCO_LINUX
        free(self->buffer);
#else
        FUN_00465cd0(self->buffer);
#endif
        self->buffer      = NULL;
        self->bufferBytes = 0;
        return;
    }

    uint32_t newBytes = self->bufferBytes - FRAME_RECORD_SIZE;
    uint32_t count    = self->bufferBytes / FRAME_RECORD_SIZE;

#ifdef LOCO_LINUX
    uint8_t *pNew = (uint8_t *)malloc(newBytes);
#else
    uint8_t *pNew = (uint8_t *)FUN_00465ce0(newBytes);
#endif
    if (!pNew) return;

    uint8_t *pOld = self->buffer;

    /* Copy entries before index. */
    if (index > 0) {
        memcpy(pNew, pOld, (uint32_t)index * FRAME_RECORD_SIZE);
    }

    /* Copy entries after index (shifted left by one). */
    if ((uint32_t)index < count - 1) {
        memcpy(pNew + (uint32_t)index * FRAME_RECORD_SIZE,
               pOld + ((uint32_t)index + 1) * FRAME_RECORD_SIZE,
               (count - 1 - (uint32_t)index) * FRAME_RECORD_SIZE);
    }

#ifdef LOCO_LINUX
    free(pOld);
#else
    FUN_00465cd0(pOld);
#endif

    self->buffer      = pNew;
    self->bufferBytes = newBytes;
}

/* ===========================================================================
 * AlbIndex_GetCount                                             0x00401810
 *
 * __fastcall, non-method.  Returns entry count from bufferBytes at +0x0C.
 * param1 is treated as the AlbIndex* directly (bufferBytes at offset 0x0C).
 * =========================================================================== */
int __fastcall AlbIndex_GetCount(const void *param1)
{
    if (!param1) return 0;
    uint32_t bufBytes = *(const uint32_t *)((const uint8_t *)param1 + 0x0C);
    return (int)(bufBytes / FRAME_RECORD_SIZE);
}

/* ===========================================================================
 * AlbIndex_EnsurePageGetCount                                   0x00401820
 *
 * Demand-load section if it differs from currentSection, then return the
 * active page entry count.
 * =========================================================================== */
int AlbIndex_EnsurePageGetCount(AlbIndex *self, int section)
{
    if (!self) return 0;
    if (self->currentSection != section) {
        AlbIndex_LoadPage(self, section);
    }
    return (int)(self->bufferBytes / FRAME_RECORD_SIZE);
}

/* ===========================================================================
 * AlbIndex_InsertSorted                                         0x00401850
 *
 * Insert a record into the alphabetically-partitioned PostBag index.
 * Selects the target section based on the first character of the uppercased
 * name field at param1+0x25, then finds the sorted insertion position and
 * calls LOCOBITMAP_InsertFrameRecord.
 * =========================================================================== */

/* Internal: map first char of uppercased name to AlbIndex section number. */
static int AlbIndex_CharToSection(unsigned char c)
{
    if (c >= 'A' && c <= 'C') return ALBINDEX_SECTION_AC;
    if (c >= 'D' && c <= 'F') return ALBINDEX_SECTION_DF;
    if (c >= 'G' && c <= 'J') return ALBINDEX_SECTION_GJ;
    if (c >= 'K' && c <= 'M') return ALBINDEX_SECTION_KM;
    if (c >= 'N' && c <= 'Q') return ALBINDEX_SECTION_NQ;
    if (c >= 'R' && c <= 'T') return ALBINDEX_SECTION_RT;
    if (c >= 'U' && c <= 'W') return ALBINDEX_SECTION_UW;
    if (c >= 'X' && c <= 'Z') return ALBINDEX_SECTION_XZ;
    return ALBINDEX_SECTION_OTHER;
}

void AlbIndex_InsertSorted(AlbIndex *self, void *param1)
{
    if (!self || !param1) return;

    /*
     * Copy the name string from param1+0x25 and uppercase it.
     * WIN32: FUN_00474c70 = _strupr
     * LINUX: toupper loop
     */
    char nameBuf[256];
    const char *srcName = (const char *)param1 + 0x25;
    strncpy(nameBuf, srcName, sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';

#ifdef LOCO_LINUX
    for (char *p = nameBuf; *p; ++p) {
        *p = (char)toupper((unsigned char)*p);
    }
#else
    FUN_00474c70(nameBuf); /* _strupr */
#endif

    /* Map first character to section index. */
    int section = AlbIndex_CharToSection((unsigned char)nameBuf[0]);

    /* Load the target section. */
    AlbIndex_LoadPage(self, section);

    /*
     * Find sorted insertion position by linearly scanning the name fields.
     * Name field layout within each 24-byte entry is not fully specified;
     * we assume the name bytes start at entry offset 0 for comparison.
     * Adjust the comparison offset if analysis reveals a different layout.
     */
    uint32_t count       = self->bufferBytes / FRAME_RECORD_SIZE;
    int      insertIndex = (int)count; /* default: append at end */

    for (uint32_t i = 0; i < count; i++) {
        const char *entryName = (const char *)(self->buffer +
                                               i * FRAME_RECORD_SIZE);
        if (strcmp(nameBuf, entryName) <= 0) {
            insertIndex = (int)i;
            break;
        }
    }

    /*
     * Reuse LOCOBITMAP_InsertFrameRecord (FUN_00401690) for the actual
     * buffer insert - both AlbIndex and LOCOBITMAP use identical 24-byte
     * records and compatible buffer-management fields.
     */
    LOCOBITMAP_InsertFrameRecord((LOCOBITMAP *)self, insertIndex,
                                  (const FrameRecord *)param1);
}

/* ===========================================================================
 * AlbIndex_RemoveByID                                           0x00401aa0
 *
 * Remove the entry whose ID field (at entry+0x14) matches the id at
 * param1+0x0C.  Returns 1 on removal, 0 if not found.
 * =========================================================================== */
int AlbIndex_RemoveByID(AlbIndex *self, void *param1)
{
    if (!self || !param1) return 0;

    /* Determine section from the name field (param1+0x25). */
    char nameBuf[256];
    const char *srcName = (const char *)param1 + 0x25;
    strncpy(nameBuf, srcName, sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
#ifdef LOCO_LINUX
    for (char *p = nameBuf; *p; ++p) {
        *p = (char)toupper((unsigned char)*p);
    }
#else
    FUN_00474c70(nameBuf);
#endif
    int section = AlbIndex_CharToSection((unsigned char)nameBuf[0]);

    AlbIndex_LoadPage(self, section);

    uint32_t  targetId = *(const uint32_t *)((const uint8_t *)param1 + 0x0C);
    uint32_t  count    = self->bufferBytes / FRAME_RECORD_SIZE;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t entryId = *(uint32_t *)(self->buffer +
                                         i * FRAME_RECORD_SIZE +
                                         ALBINDEX_ENTRY_ID_OFFSET);
        if (entryId == targetId) {
            AlbIndex_RemoveAt(self, (int)i);
            return 1;
        }
    }
    return 0;
}

/* ===========================================================================
 * AlbIndex_FindResource                                         0x00401c10
 *
 * Load section, iterate entries from startIndex, build a resource path for
 * each entry's ID, and return the first non-NULL resource pointer.
 * =========================================================================== */
void *AlbIndex_FindResource(AlbIndex *self, int startIndex, int section)
{
    if (!self) return NULL;

    AlbIndex_LoadPage(self, section);

    uint32_t count      = self->bufferBytes / FRAME_RECORD_SIZE;
    char     pathBuf[1284];

    for (uint32_t i = (uint32_t)startIndex; i < count; i++) {
        uint32_t id = *(uint32_t *)(self->buffer +
                                    i * FRAME_RECORD_SIZE +
                                    ALBINDEX_ENTRY_ID_OFFSET);
        FUN_00445930((int)id, pathBuf);
        void *pRes = FUN_00444c70(pathBuf);
        if (pRes) return pRes;
    }
    return NULL;
}

/* ===========================================================================
 * Internal helper: build the .ind file path.
 * Path: <DataPath>\PostBag\AlbIndex\<album%03d>\<section%04d>.ind
 * =========================================================================== */
static void AlbIndex_BuildPath(AlbIndex *self, int section, char *outPath,
                                size_t pathSize)
{
    const char *base = "";
    int albumNum     = self ? self->albumNum : 0;

    snprintf(outPath, pathSize,
             "%sPostBag\\AlbIndex\\%03d\\%04d.ind",
             base, albumNum, section);
}

/* ===========================================================================
 * AlbIndex_FlushPage                                            0x00401c90
 *
 * Write the resident section buffer to disk and evict it.
 * =========================================================================== */
void AlbIndex_FlushPage(AlbIndex *self)
{
    if (!self || !self->buffer || self->bufferBytes == 0 ||
        self->currentSection == ALBINDEX_SECTION_NONE) {
        return;
    }

    char filePath[512];
    AlbIndex_BuildPath(self, self->currentSection, filePath, sizeof(filePath));

#ifdef LOCO_LINUX
    unlink(filePath);

    FILE *fp = fopen(filePath, "wb");
    if (!fp) {
        fprintf(stderr, "AlbIndex_FlushPage: fopen failed: %s: %s\n",
                filePath, strerror(errno));
        return;
    }
    fwrite(self->buffer, 1, self->bufferBytes, fp);
    fclose(fp);
#else
    /* WIN32: DeleteFileA + CreateFileA + WriteFile + CloseHandle */
    DeleteFileA(filePath);

    HANDLE hFile = CreateFileA(filePath,
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ,
                               NULL, OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        char *pMsg = NULL;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL, err, 0, (LPSTR)&pMsg, 0, NULL);
        if (pMsg) LocalFree(pMsg);
        return;
    }
    DWORD written = 0;
    WriteFile(hFile, self->buffer, self->bufferBytes, &written, NULL);
    CloseHandle(hFile);
#endif

    /* Evict the buffer. */
#ifdef LOCO_LINUX
    free(self->buffer);
#else
    FUN_00465cd0(self->buffer);
#endif
    self->buffer          = NULL;
    self->bufferBytes     = 0;
    self->currentSection  = ALBINDEX_SECTION_NONE;
}

/* ===========================================================================
 * AlbIndex_LoadPage                                             0x00401df0
 *
 * Flush the current page then load section param_section from disk.
 * =========================================================================== */
void AlbIndex_LoadPage(AlbIndex *self, int section)
{
    if (!self) return;

    /* Flush current page if one is loaded. */
    if (self->currentSection != ALBINDEX_SECTION_NONE) {
        AlbIndex_FlushPage(self);
    }

    char filePath[512];
    AlbIndex_BuildPath(self, section, filePath, sizeof(filePath));

#ifdef LOCO_LINUX
    FILE *fp = fopen(filePath, "rb");
    if (!fp) {
        self->buffer         = NULL;
        self->bufferBytes    = 0;
        self->currentSection = section;
        return;
    }

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize <= 0) {
        fclose(fp);
        self->buffer         = NULL;
        self->bufferBytes    = 0;
        self->currentSection = section;
        return;
    }

    uint8_t *pBuf = (uint8_t *)malloc((size_t)fileSize);
    if (!pBuf) {
        fclose(fp);
        return;
    }
    fread(pBuf, 1, (size_t)fileSize, fp);
    fclose(fp);

    self->buffer         = pBuf;
    self->bufferBytes    = (uint32_t)fileSize;
    self->currentSection = section;

#else
    /* WIN32: CreateFileA + GetFileSize + ReadFile + CloseHandle */
    HANDLE hFile = CreateFileA(filePath,
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ,
                               NULL, OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        self->buffer         = NULL;
        self->bufferBytes    = 0;
        self->currentSection = section;
        return;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        self->buffer         = NULL;
        self->bufferBytes    = 0;
        self->currentSection = section;
        return;
    }

    uint8_t *pBuf = (uint8_t *)FUN_00465ce0(fileSize);
    if (!pBuf) {
        CloseHandle(hFile);
        return;
    }
    DWORD bytesRead = 0;
    ReadFile(hFile, pBuf, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    self->buffer         = pBuf;
    self->bufferBytes    = fileSize;
    self->currentSection = section;
#endif
}

/* ══════════════════════════════════════════════════════════════════════
 * Batch 3 - LocoBitmapChain functions        0x00401f50 to 0x00404ac0
 * ══════════════════════════════════════════════════════════════════════ */

/* ===========================================================================
 * LocoBitmapChain_Ctor                                          0x00401f50
 * =========================================================================== */
void LocoBitmapChain_Ctor(LocoBitmapChain *self)
{
    if (!self) return;
    FUN_00425870(self); /* base class constructor */
    /* self->vtable = PTR_FUN_004773f0; (set by C++ runtime) */
    LocoBitmapChain_Init(self);
}

/* ===========================================================================
 * LocoBitmapChain_Dtor                                          0x00401fb0
 * =========================================================================== */
void LocoBitmapChain_Dtor(LocoBitmapChain *self, int deleteBit)
{
    if (!self) return;
    LocoBitmapChain_Release(self);
    if (deleteBit & 1) {
#ifdef LOCO_LINUX
        free(self);
#else
        FUN_00465cd0(self);
#endif
    }
}

/* ===========================================================================
 * LocoBitmapChain_Init                                          0x00401fd0
 *
 * Initialise all fields, detect resolution, allocate all LOCOBITMAP slots.
 * =========================================================================== */
void LocoBitmapChain_Init(LocoBitmapChain *self)
{
    if (!self) return;

    /* Zero all state fields from srcOffsetX onwards. */
    memset(&self->srcOffsetX, 0,
           (size_t)((char *)self->labels - (char *)&self->srcOffsetX)
           + sizeof(self->labels));

    /* Detect display resolution. */
#ifdef LOCO_LINUX
    SDL_DisplayMode dm;
    self->resolutionFlag = 0;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
        self->resolutionFlag = (dm.w >= 801 && dm.h >= 601) ? 1 : 0;
    }
#else
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    self->resolutionFlag = (sw >= 801 && sh >= 601) ? 1 : 0;
#endif

    /*
     * Allocate fixed LOCOBITMAP slots (IDs 0x3c04..0x3c0f).
     * Exact allocation (FUN_00454xxx) not fully specified; stub here.
     */
    for (int i = 0; i < LBC_FIXED_SLOT_COUNT; i++) {
        self->slots[i] = NULL; /* filled by higher-level init */
    }
    for (int i = 0; i < LBC_THUMBNAIL_COUNT; i++) {
        self->thumbnails[i] = NULL;
    }
    for (int i = 0; i < LBC_LABELBG_COUNT; i++) {
        self->labelBg[i] = NULL;
    }
    for (int i = 0; i < LBC_TEXTRECT_COUNT; i++) {
        self->textRects[i] = NULL;
    }
    for (int i = 0; i < LBC_SCROLLDEST_COUNT; i++) {
        self->scrollDest[i] = NULL;
    }

    /* Zero all label text buffers. */
    memset(self->labels, 0, sizeof(self->labels));

    /* Set all enable flags to 1 (enabled). */
    for (int i = 0; i < LBC_ENABLE_FLAG_COUNT; i++) {
        self->enableFlags[i] = 1;
    }
}

/* ===========================================================================
 * LocoBitmapChain_Release                                       0x00402380
 *
 * Release all DirectDraw and LOCOBITMAP resources.
 * =========================================================================== */
void LocoBitmapChain_Release(LocoBitmapChain *self)
{
    if (!self) return;

    /* Release the DirectDraw object at +0x140 via vtable[2]. */
    if (self->pDDObject) {
        typedef void (__stdcall *RelFn)(void *);
        void **vt = *(void ***)self->pDDObject;
        ((RelFn)vt[2])(self->pDDObject);
        self->pDDObject = NULL;
    }

    /* Free all 8 fixed slots. */
    for (int i = 0; i < LBC_FIXED_SLOT_COUNT; i++) {
        if (self->slots[i]) {
            FUN_00454bc0(self->slots[i]);
            self->slots[i] = NULL;
        }
    }

    /* Free thumbnail and label-bg slots (combined loop, 6 each). */
    for (int i = 0; i < LBC_THUMBNAIL_COUNT; i++) {
        if (self->thumbnails[i]) {
            FUN_00454bc0(self->thumbnails[i]);
            self->thumbnails[i] = NULL;
        }
        if (self->labelBg[i]) {
            FUN_00454bc0(self->labelBg[i]);
            self->labelBg[i] = NULL;
        }
    }

    /* Free scroll-dest slots (9). */
    for (int i = 0; i < LBC_SCROLLDEST_COUNT; i++) {
        if (self->scrollDest[i]) {
            FUN_00454bc0(self->scrollDest[i]);
            self->scrollDest[i] = NULL;
        }
    }

    FUN_00425910(self); /* base class destructor */
}

/* ===========================================================================
 * LocoBitmapChain_CreateWindow                                  0x00402520
 * =========================================================================== */
BOOL LocoBitmapChain_CreateWindow(LocoBitmapChain *self)
{
    if (!self) return FALSE;

#ifdef LOCO_LINUX
    /* LINUX: SDL_CreateWindow(SDL_WINDOW_FULLSCREEN_DESKTOP) */
    return FALSE; /* TODO: implement SDL2 window creation */
#else
    HWND hDesktop = GetDesktopWindow();
    RECT desktopRect;
    GetClientRect(hDesktop, &desktopRect);

    HICON hIcon = LoadIconA(self->hInst,
                            MAKEINTRESOURCEA(LBC_WINDOW_ICON_ID));
    self->hIcon = hIcon;

    FUN_00425b70(self,
                 WS_OVERLAPPEDWINDOW,
                 NULL,
                 desktopRect.left, desktopRect.top,
                 desktopRect.right  - desktopRect.left,
                 desktopRect.bottom - desktopRect.top,
                 NULL, hIcon, 0);

    return (self->hParent != NULL) ? TRUE : FALSE;
#endif
}

/* ===========================================================================
 * LocoBitmapChain_FlushDrawList                                 0x00402660
 * =========================================================================== */
void LocoBitmapChain_FlushDrawList(LocoBitmapChain *self)
{
    if (!self) return;
    if (self->drawActive) {
        FUN_00425990(self);      /* present / flip */
        self->drawListActive = 0;
        LocoBitmapChain_FreeSlotData(self);
    }
}

/* ===========================================================================
 * LocoBitmapChain_WndProc                                       0x00402690
 *
 * Window procedure for keyboard navigation.
 *
 * self is retrieved from the window's GWLP_USERDATA (set by the window
 * creation helper FUN_00425b70).
 * =========================================================================== */
LRESULT CALLBACK LocoBitmapChain_WndProc(HWND hWnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam)
{
#ifndef LOCO_LINUX
    LocoBitmapChain *self =
        (LocoBitmapChain *)GetWindowLongPtrA(hWnd, GWLP_USERDATA);
    if (!self || self->shuttingDown) {
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    if (msg == WM_KEYDOWN) {
        switch ((int)wParam) {
        case 0x1B: /* VK_ESCAPE */
        case 0x0D: /* VK_RETURN */
            PostQuitMessage(0);
            break;

        case VK_LEFT: /* 0x25 */
            /* Un-draw left-arrow highlight, pause, re-draw. */
            LocoBitmapChain_DrawSlot(self, LBC_SLOTID_LEFT_ARROW);
            Sleep(LBC_ARROW_BLINK_MS);
            LocoBitmapChain_SetSlotState(self, LBC_SLOTID_LEFT_ARROW);
            /* Scroll left: decrement currentCol or currentRow. */
            if (self->currentCol > 0) {
                self->currentCol--;
            } else if (self->currentRow > 0) {
                self->currentRow--;
                self->currentCol = self->pageSize - 1;
            }
            LocoBitmapChain_RefreshGrid(self);
            FUN_00426b90(self);
            break;

        case VK_RIGHT: /* 0x27 */
            LocoBitmapChain_DrawSlot(self, LBC_SLOTID_RIGHT_ARROW);
            Sleep(LBC_ARROW_BLINK_MS);
            LocoBitmapChain_SetSlotState(self, LBC_SLOTID_RIGHT_ARROW);
            self->currentCol++;
            LocoBitmapChain_RefreshGrid(self);
            FUN_00426b90(self);
            break;

        default:
            break;
        }
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
#else
    /* LINUX: keyboard navigation handled via SDL_PollEvent in the main loop. */
    (void)hWnd; (void)msg; (void)wParam; (void)lParam;
    return 0;
#endif
}

/* ===========================================================================
 * LocoBitmapChain_SetSlotState                                  0x00403ba0
 *
 * Set visual state (normal/grayed) of one fixed UI slot.
 * =========================================================================== */
void LocoBitmapChain_SetSlotState(LocoBitmapChain *self, int slotId)
{
    if (!self) return;

    LOCOBITMAP *slot    = NULL;
    int         enabled = 1;

    switch (slotId) {
    case LBC_SLOTID_1: slot = self->slots[0]; break;
    case LBC_SLOTID_2: slot = self->slots[1]; break;
    case LBC_SLOTID_3: slot = self->slots[2]; break;
    case LBC_SLOTID_4: slot = self->slots[3]; break;
    case LBC_SLOTID_9:
        slot    = self->slots[4];
        enabled = self->enableFlags[2];
        break;
    case LBC_SLOTID_LEFT_ARROW:
        slot    = self->slots[5];
        enabled = self->enableFlags[0];
        break;
    case LBC_SLOTID_RIGHT_ARROW:
        slot    = self->slots[6];
        enabled = self->enableFlags[1];
        break;
    default:
        return;
    }

    if (!slot) return;

    /* mode 0 = normal/idle frame; mode 2 = grayed/disabled frame */
    FUN_00454c30(slot, enabled ? 0 : 2, NULL);
}

/* ===========================================================================
 * LocoBitmapChain_HitTest                                       0x00403cd0
 *
 * Hit-test screen point (x,y) against all slot RECTs.
 * The LOCOBITMAP fields width/frameBufPtr/capacity/frameCount serve as
 * RECT left/top/right/bottom (see LBC_SLOT_* macros).
 * =========================================================================== */
int LocoBitmapChain_HitTest(LocoBitmapChain *self, int x, int y)
{
    if (!self) return 0;

    /* Fixed button slots (IDs 1,2,3,4,9). */
    static const struct { int slotIdx; int zoneId; } fixed[] = {
        {0, LBC_SLOTID_1}, {1, LBC_SLOTID_2},
        {2, LBC_SLOTID_3}, {3, LBC_SLOTID_4},
        {4, LBC_SLOTID_9},
    };
    for (int i = 0; i < 5; i++) {
        LOCOBITMAP *s = self->slots[fixed[i].slotIdx];
        if (s && LBC_SLOT_HIT(s, x, y)) return fixed[i].zoneId;
    }

    /* Left and right arrow slots (IDs 5, 6). */
    if (self->slots[5] && LBC_SLOT_HIT(self->slots[5], x, y))
        return LBC_SLOTID_LEFT_ARROW;
    if (self->slots[6] && LBC_SLOT_HIT(self->slots[6], x, y))
        return LBC_SLOTID_RIGHT_ARROW;

    /* Scroll-dest slots (zone ID 7). */
    for (int i = 0; i < LBC_SCROLLDEST_COUNT; i++) {
        LOCOBITMAP *s = self->scrollDest[i];
        if (s && LBC_SLOT_HIT(s, x, y)) {
            self->hitIndex = i;
            return 7;
        }
    }

    /* Thumbnail slots (zone ID 8). */
    for (int i = 0; i < LBC_THUMBNAIL_COUNT; i++) {
        LOCOBITMAP *s = self->thumbnails[i];
        if (s && LBC_SLOT_HIT(s, x, y)) {
            self->hitIndex = i;
            return LBC_SLOTID_THUMBNAIL;
        }
    }

    /* Text-rect slots (zone ID 10). */
    for (int i = 0; i < LBC_TEXTRECT_COUNT; i++) {
        LOCOBITMAP *s = self->textRects[i];
        if (s && LBC_SLOT_HIT(s, x, y)) {
            self->hitIndex = i;
            return LBC_SLOTID_TEXT_RECT;
        }
    }

    return 0; /* miss */
}

/* ===========================================================================
 * LocoBitmapChain_DrawSlot                                      0x00403e80
 *
 * Draw one LOCOBITMAP slot to the back-buffer.  Applies src/dst offsets and
 * calls the AW blit helper.
 * =========================================================================== */
void LocoBitmapChain_DrawSlot(LocoBitmapChain *self, int slotId)
{
    if (!self) return;
    if (!self->initialized || !self->drawListActive) return;

    /* Acquire draw resource token. */
    FUN_00447930(LBC_DRAW_RESOURCE_TOKEN);

    LOCOBITMAP *slot     = NULL;
    int         frameIdx = 0;
    int         useEnable = 0;
    int         enableIdx = 0;

    switch (slotId) {
    case LBC_SLOTID_1:            slot = self->slots[0]; break;
    case LBC_SLOTID_2:            slot = self->slots[1]; break;
    case LBC_SLOTID_3:            slot = self->slots[2]; break;
    case LBC_SLOTID_4:            slot = self->slots[3]; break;
    case LBC_SLOTID_9:            slot = self->slots[4]; useEnable=1; enableIdx=2; break;
    case LBC_SLOTID_LEFT_ARROW:   slot = self->slots[5]; useEnable=1; enableIdx=0; break;
    case LBC_SLOTID_RIGHT_ARROW:  slot = self->slots[6]; useEnable=1; enableIdx=1; break;
    case LBC_SLOTID_TRACK:
        slot     = self->slots[7];
        frameIdx = self->trackPageIndex;
        break;
    default:
        return;
    }

    if (!slot) return;

    /* Check enabled state for slots that have an enable flag. */
    if (useEnable && !self->enableFlags[enableIdx]) {
        FUN_00454c30(slot, 2, NULL); /* grayed frame */
        return;
    }

    /* Read slot RECT from the LOCOBITMAP fields (+0x04..+0x10). */
    RECT srcRect = {
        LBC_SLOT_LEFT(slot)   + self->srcOffsetX,
        LBC_SLOT_TOP(slot)    + self->srcOffsetY,
        LBC_SLOT_RIGHT(slot)  + self->srcOffsetX,
        LBC_SLOT_BOTTOM(slot) + self->srcOffsetY
    };
    RECT dstRect = {
        LBC_SLOT_LEFT(slot)   + self->dstOffsetX,
        LBC_SLOT_TOP(slot)    + self->dstOffsetY,
        LBC_SLOT_RIGHT(slot)  + self->dstOffsetX,
        LBC_SLOT_BOTTOM(slot) + self->dstOffsetY
    };

    (void)frameIdx; /* frame index used by caller to select animation frame */

    FUN_0042b050(g_pBackBuffer, self->pBlitSurface, &srcRect, &dstRect);

    /* Mark slot visible. */
    FUN_00454c30(slot, 1, NULL);
}

/* ===========================================================================
 * LocoBitmapChain_FreeSlotData                                  0x00404830
 * =========================================================================== */
void LocoBitmapChain_FreeSlotData(LocoBitmapChain *self)
{
    if (!self || !self->initialized) return;

    /* Release DirectDraw object at +0x140 via vtable[2]. */
    if (self->pDDObject) {
        typedef void (__stdcall *RelFn)(void *);
        void **vt = *(void ***)self->pDDObject;
        ((RelFn)vt[2])(self->pDDObject);
        self->pDDObject = NULL;
    }

    for (int i = 0; i < LBC_FIXED_SLOT_COUNT; i++) {
        if (self->slots[i]) {
            FUN_00454bc0(self->slots[i]);
            self->slots[i] = NULL;
        }
    }

    /* Combined loop for thumbnails (6) and labelBg (6). */
    for (int i = 0; i < LBC_THUMBNAIL_COUNT; i++) {
        if (self->thumbnails[i]) {
            FUN_00454bc0(self->thumbnails[i]);
            self->thumbnails[i] = NULL;
        }
        if (self->labelBg[i]) {
            FUN_00454bc0(self->labelBg[i]);
            self->labelBg[i] = NULL;
        }
    }

    for (int i = 0; i < LBC_SCROLLDEST_COUNT; i++) {
        if (self->scrollDest[i]) {
            FUN_00454bc0(self->scrollDest[i]);
            self->scrollDest[i] = NULL;
        }
    }

    self->initialized = 0;
}

/* ===========================================================================
 * LocoBitmapChain_DrawItem                                      0x004048e0
 *
 * Draw one scrollable grid cell (column col, row currentRow).
 * =========================================================================== */
void LocoBitmapChain_DrawItem(LocoBitmapChain *self, int col)
{
    if (!self) return;

    /* Look up the world-data entry via AlbIndex. */
    void *pItemData = AlbIndex_FindResource(NULL /* index ptr, not self */,
                                            self->currentCol + col,
                                            self->currentRow);

    if (!pItemData) {
        /* No data: blit background thumbnail. */
        if (self->thumbnails[col]) {
            RECT r = {
                LBC_SLOT_LEFT(self->thumbnails[col]),
                LBC_SLOT_TOP(self->thumbnails[col]),
                LBC_SLOT_RIGHT(self->thumbnails[col]),
                LBC_SLOT_BOTTOM(self->thumbnails[col])
            };
            FUN_0042b050(g_pBackBuffer, self->pBlitSurface,
                          &r, &r);
        }
        self->labels[col][0] = '\0';
    } else {
        /* Item found: render bitmap and update label. */
        if (self->thumbnails[col]) {
            RECT r = {
                LBC_SLOT_LEFT(self->thumbnails[col]),
                LBC_SLOT_TOP(self->thumbnails[col]),
                LBC_SLOT_RIGHT(self->thumbnails[col]),
                LBC_SLOT_BOTTOM(self->thumbnails[col])
            };
            FUN_004437c0(pItemData, self->thumbnails[col], &r);
        }

        /* Copy item name (at data+0x25) into the label buffer. */
        const char *name = (const char *)pItemData + 0x25;
        strncpy(self->labels[col], name, LBC_LABEL_BUF_SIZE - 1);
        self->labels[col][LBC_LABEL_BUF_SIZE - 1] = '\0';

        /* Show label overlay. */
        if (self->labelBg[col]) {
            FUN_00454c30(self->labelBg[col], 0, NULL);
        }
    }
}

/* ===========================================================================
 * LocoBitmapChain_RefreshGrid                                   0x00404ac0
 *
 * Redraw all 6 visible grid columns, render text labels, update enable
 * flags, and blit to screen.
 * =========================================================================== */
void LocoBitmapChain_RefreshGrid(LocoBitmapChain *self)
{
    if (!self) return;

    /* Draw each of the 6 visible item cells. */
    for (int i = 0; i < LBC_LABEL_COUNT; i++) {
        LocoBitmapChain_DrawItem(self, i);
    }

#ifndef LOCO_LINUX
    /* Render text labels with DrawTextA. */
    if (self->textFlag) {
        for (int i = 0; i < LBC_TEXTRECT_COUNT; i++) {
            LOCOBITMAP *tr = self->textRects[i];
            if (!tr || !self->labels[i][0]) continue;

            RECT textRect = {
                LBC_SLOT_LEFT(tr), LBC_SLOT_TOP(tr),
                LBC_SLOT_RIGHT(tr), LBC_SLOT_BOTTOM(tr)
            };

            HDC hDC = NULL;
            typedef HRESULT (__stdcall *GetDCFn)(void *, HDC *);
            if (self->pBlitSurface) {
                GetDCFn pfnGetDC =
                    (GetDCFn)(*(void ***)self->pBlitSurface)[VTBL_IDS_GETDC];
                pfnGetDC(self->pBlitSurface, &hDC);
            }
            if (hDC && g_labelFont) {
                HGDIOBJ oldFont = SelectObject(hDC, g_labelFont);
                DrawTextA(hDC, self->labels[i], -1, &textRect,
                          LBC_DRAWTEXT_FLAGS);
                SelectObject(hDC, oldFont);
            }
            if (hDC && self->pBlitSurface) {
                typedef HRESULT (__stdcall *ReleaseDCFn)(void *, HDC);
                ReleaseDCFn pfnRelDC =
                    (ReleaseDCFn)(*(void ***)self->pBlitSurface)[VTBL_IDS_RELEASEDC];
                pfnRelDC(self->pBlitSurface, hDC);
            }
        }
    }
#endif

    /* Blit result to screen. */
    FUN_00426b90(self);

    /* Update prev-page enable flags. */
    int prevEnabled = (self->currentCol == 0 && self->currentRow == 0) ? 0 : 1;
    self->enableFlags[0] = (uint8_t)prevEnabled;
    self->enableFlags[4] = (uint8_t)prevEnabled;

    /* Update next-page enable flags.
     * totalItems not tracked here; caller must set enableFlags[1]/[5]
     * based on (currentCol + pageSize) vs total world-data entry count.
     */

    /* Refresh arrow visual states. */
    LocoBitmapChain_SetSlotState(self, LBC_SLOTID_LEFT_ARROW);
    LocoBitmapChain_SetSlotState(self, LBC_SLOTID_RIGHT_ARROW);
}

/* end of locobitmap.c */
