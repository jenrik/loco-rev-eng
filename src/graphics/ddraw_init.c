/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: Graphics / DirectDraw Init / Surface Management
 * Original: loco.exe (Windows 95/98, DirectX 5 era)
 * Developer: Intelligent Games for LEGO Media
 *
 * This file was produced by reverse engineering the original binary.
 * Windows API calls are marked with WIN32: comments.
 * Linux/SDL2 replacement suggestions are marked with LINUX: comments.
 */

/*
 * ddraw_init.c — DirectDraw Init / Display Setup subsystem
 * Lego Loco (1998) — loco.exe reverse-engineered source
 *
 * This file covers the full DirectDraw 5 initialisation and teardown pipeline,
 * pixel-format detection, surface creation and management, clipper setup,
 * GDI-to-surface bitmap copying, back-buffer-to-primary blitting, and the
 * frame-trigger mechanism.  DirectSound initialisation is also included here
 * because it is torn down in the same shutdown sequence.
 *
 * Global COM interface pointers
 * ─────────────────────────────
 *   IDirectDraw  *g_pDD          @ 0x485440   — upgraded IDirectDraw interface
 *   IDirectDraw  *g_pDDRaw       @ 0x4A9908   — raw ptr from DirectDrawCreate()
 *   DWORD         g_fullscreen   @ 0x4A9918   — 1=exclusive, 0=windowed
 *   IDirectDrawSurface *g_pDDSPrimary @ 0x4FD3C0  — front/primary surface
 *   IDirectDrawSurface *g_pDDSBack   @ 0x4FD3C4  — back-buffer composite surface
 *   IDirectDrawSurface *g_pDDSSplash @ 0x4FD3D8  — loading-screen surface
 *   IDirectSound *g_pDS           @ 0x4FD3BC  — DirectSound device
 *   // Sprite/texture cache — released by DD_ReleaseAuxSurfaces():
 *   IDirectDrawSurface *g_pSurf[6] @ 0x4FF0F8–0x4FF10C
 *   void *g_pPalOrThumb           @ 0x4FF110
 *
 * Pixel-format block (populated once during DD_Init)
 * ───────────────────────────────────────────────────
 *   DWORD g_pixFmtId    @ 0x485274  // 0x22B = RGB555, 0x235 = RGB565
 *   DWORD g_rShift      @ 0x485278  // red   shift: 10 (555) or 11 (565)
 *   DWORD g_gBits       @ 0x48527C  // green bits : 5  (555) or  6 (565)
 *   DWORD g_whitePixel  @ 0x485280  // max white  : 0x3DEF (555) or 0x7BEF (565)
 *   DWORD g_whiteAlt    @ 0x485284  // mirror of g_whitePixel
 *   DWORD g_rMask       @ 0x485288  // red   mask from DDSURFACEDESC
 *   DWORD g_gMask       @ 0x48528C  // green mask from DDSURFACEDESC
 *   DWORD g_bMask       @ 0x485290  // blue  mask from DDSURFACEDESC
 *
 * LINUX: Replace DirectDraw with SDL2.  See detailed mapping at end of file.
 */

#include "graphics.h"

/* ── forward declarations of internal helpers ── */
static char *DD_HResultToString(HRESULT hr);   /* 0x0045BBC0 */
static char *DS_HResultToString(HRESULT hr);   /* 0x0045C2E0 */
static void  DD_ReleaseAuxSurfaces(void);       /* 0x0045C970 */

/*
 * External globals referenced in this file.
 * In the original binary these are at fixed addresses; in a port they
 * become ordinary global variables declared in the appropriate modules.
 */
extern IDirectDraw         *g_pDD;          /* 0x485440 */
extern IDirectDraw         *g_pDDRaw;       /* 0x4A9908 */
extern DWORD                g_fullscreen;   /* 0x4A9918 */
extern IDirectDrawSurface  *g_pDDSPrimary;  /* 0x4FD3C0 */
extern IDirectDrawSurface  *g_pDDSBack;     /* 0x4FD3C4 */
extern IDirectDrawSurface  *g_pDDSSplash;   /* 0x4FD3D8 */
extern IDirectSound        *g_pDS;          /* 0x4FD3BC */
extern IDirectDrawSurface  *g_pSurf0;       /* 0x4FF0F8 */
extern IDirectDrawSurface  *g_pSurf1;       /* 0x4FF0FC */
extern IDirectDrawSurface  *g_pSurf2;       /* 0x4FF100 */
extern IDirectDrawSurface  *g_pSurf3;       /* 0x4FF104 */
extern IDirectDrawSurface  *g_pSurf4;       /* 0x4FF108 */
extern IDirectDrawSurface  *g_pSurf5;       /* 0x4FF10C */
extern void                *g_pPalOrThumb;  /* 0x4FF110 */

/* Pixel-format globals — DD_PixelFormat block @ 0x485274 */
extern DWORD  g_pixFmtId;    /* 0x485274 */
extern DWORD  g_rShift;      /* 0x485278 */
extern DWORD  g_gBits;       /* 0x48527C */
extern DWORD  g_whitePixel;  /* 0x485280 */
extern DWORD  g_whiteAlt;    /* 0x485284 */
extern DWORD  g_rMask;       /* 0x485288 */
extern DWORD  g_gMask;       /* 0x48528C */
extern DWORD  g_bMask;       /* 0x485290 */

/* Application object and window globals */
extern void  *g_appObj;      /* main application object */
extern HWND   hWndMain;      /* main window handle */
extern RECT   g_screenRect;  /* 0x485220 — full-screen rect */

/* Config store and audio manager globals */
extern void  *g_configStore;
extern void  *g_aadm0c;
extern void  *g_aadm10;

/* Default system font handle */
extern HANDLE g_defaultFont; /* DAT_004855F8 */

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_Init                                                       0x0045B500
 * ═══════════════════════════════════════════════════════════════════════════
 * Master DirectDraw initialisation.
 *
 * Parameters : none
 * Returns    : 1 on full success, 0 on any failure (low byte significant).
 *
 * Windows APIs used
 *   DirectDrawCreate, IDirectDraw::QueryInterface,
 *   IDirectDraw::SetCooperativeLevel, IDirectDraw::CreateSurface,
 *   IDirectDrawSurface::GetSurfaceDesc, IDirectDrawSurface::SetColorKey,
 *   IDirectDrawSurface::GetClipper, IDirectDraw::CreateClipper,
 *   IDirectDrawClipper::SetHWnd, IDirectDrawSurface::SetClipper
 *
 * LINUX: Replace with SDL_CreateWindow + SDL_CreateRenderer.
 *        Pixel-format block is unnecessary — SDL2 uses 32-bit RGBA.
 *        Colour-key transparency → SDL_SetColorKey.
 *        Clipper → SDL_RenderSetClipRect / SDL_Rect* on blit calls.
 */
UINT DD_Init(void)
{
    HRESULT hr;
    char   *errStr;

    /* ── 1. Create raw IDirectDraw interface ──────────────────────────── */
    /* WIN32: DirectDrawCreate(NULL, &g_pDDRaw, NULL)                      */
    /* LINUX: SDL_CreateWindow("Lego Loco", ..., 640, 480, SDL_WINDOW_SHOWN) */
    hr = DirectDrawCreate(NULL, (LPDIRECTDRAW *)&g_pDDRaw, NULL);
    if (hr != DD_OK) {
        errStr = DD_HResultToString(hr);
        (void)errStr;
        return 0; /* failure */
    }

    /* ── 2. QueryInterface to upgrade to a later IDirectDraw version ─── */
    /* Stored in g_pDD (0x485440).                                         */
    /* WIN32: g_pDDRaw->QueryInterface(IID_IDirectDraw4, (void**)&g_pDD)  */
    hr = IDirectDraw_QueryInterface((IDirectDraw *)g_pDDRaw,
                                    &IID_IDirectDraw4,
                                    (void **)&g_pDD);
    if (hr != DD_OK) {
        errStr = DD_HResultToString(hr);
        (void)errStr;
        return 0;
    }

    /* ── 3. Set cooperative level ─────────────────────────────────────── */
    /* WIN32: g_pDD->SetCooperativeLevel(hWndMain, flags)                  */
    /*   g_fullscreen==1 → DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN               */
    /*   g_fullscreen==0 → DDSCL_NORMAL (windowed)                         */
    /* LINUX: SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN) or 0     */
    if (g_fullscreen == 1)
        hr = IDirectDraw4_SetCooperativeLevel((IDirectDraw4 *)g_pDD,
                                              hWndMain,
                                              DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
    else
        hr = IDirectDraw4_SetCooperativeLevel((IDirectDraw4 *)g_pDD,
                                              hWndMain,
                                              DDSCL_NORMAL);
    if (hr != DD_OK) {
        errStr = DD_HResultToString(hr);
        (void)errStr;
        return 0;
    }

    /* ── 4. Build DDSURFACEDESC for primary+flip surface ─────────────── */
    DDSURFACEDESC ddsd;
    memset(&ddsd, 0, sizeof(ddsd));         /* 0x7C bytes = 0x1F dwords   */
    ddsd.dwSize  = sizeof(ddsd);            /* must be 0x7C               */
    ddsd.dwFlags = DDSD_CAPS;               /* 0x01 — only CAPS needed    */
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP; /* 0x4040 */
    /* For a flipping chain dwBackBufferCount would also be set;            *
     * the decompiler lost those args — the surface type is inferred from   *
     * the caps flags and the back-buffer retrieval that follows.            */

    /* ── 5. Create primary surface ────────────────────────────────────── */
    /* WIN32: g_pDD->CreateSurface(&ddsd, &g_pDDSPrimary, NULL)            */
    /* LINUX: g_pDDSPrimary ~ SDL_GetWindowSurface(win) or SDL_Renderer    */
    hr = IDirectDraw4_CreateSurface((IDirectDraw4 *)g_pDD,
                                    &ddsd,
                                    (LPDIRECTDRAWSURFACE4 *)&g_pDDSPrimary,
                                    NULL);
    if (hr != DD_OK) {
        DD_HResultToString(hr);
        return 0;
    }

    /* ── 6. Query surface pixel format ───────────────────────────────── */
    /* WIN32: g_pDDSPrimary->GetSurfaceDesc(&ddsd)                         */
    /* LINUX: SDL_GetWindowSurface returns a surface with .format->Rmask   */
    {
        DDSURFACEDESC desc;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        IDirectDrawSurface4_GetSurfaceDesc((IDirectDrawSurface4 *)g_pDDSPrimary,
                                           &desc);
        /* desc.ddpfPixelFormat.dwRBitMask is at desc+0x58 in the struct   */
        DD_SetPixelFormatGlobals(g_pDDSPrimary, &desc);
    }

    /* ── 7. Create back-buffer offscreen surface ──────────────────────── */
    /* WIN32: second CreateSurface call with DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT */
    /* LINUX: SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,           *
     *            SDL_TEXTUREACCESS_TARGET, 640, 480)                       */
    hr = IDirectDraw4_CreateSurface((IDirectDraw4 *)g_pDD,
                                    &ddsd,
                                    (LPDIRECTDRAWSURFACE4 *)&g_pDDSBack,
                                    NULL);
    if (hr != DD_OK) {
        DD_HResultToString(hr);
        /* fall through to mode detection even on back-buffer fail          *
         * (game tolerates this on some hardware)                           */
    }

    /* ── 8. Detect display mode from surface pitch ────────────────────── */
    /* Surface pitch returned in GetSurfaceDesc.lPitch.                     *
     * pitch == 0x7C00 (31744) means hardware is in RGB555 15-bit mode;    *
     * any other pitch means RGB565 16-bit mode.                            *
     * This also records raw pitch/width/height in further globals.         */
    /* (See DD_SetPixelFormatGlobals for full logic.)                       */

    /* ── 9. Set transparent colour key (magenta) on surfaces ─────────── */
    /* WIN32: IDirectDrawSurface::SetColorKey(DDCKEY_SRCBLT, &ck)          */
    /* LINUX: SDL_SetColorKey(surf, SDL_TRUE,                               *
     *            SDL_MapRGB(fmt, 255, 0, 255))                            */
    {
        DDCOLORKEY ck;
        /* Magenta: R=31 G=0 B=31 encoded for the active pixel format      */
        ck.dwColorSpaceLowValue  = (g_pixFmtId == 0x22B) ? 0x7C1F : 0xF81F;
        ck.dwColorSpaceHighValue = ck.dwColorSpaceLowValue;
        IDirectDrawSurface4_SetColorKey((IDirectDrawSurface4 *)g_pDDSBack,
                                        DDCKEY_SRCBLT, &ck);
    }

    /* ── 10. Attach clipper ────────────────────────────────────────────── */
    /* LINUX: Not needed — use SDL_Rect* on SDL_BlitSurface / RenderCopy   */
    {
        IDirectDrawClipper *pClipper = NULL;
        HRESULT hrClip = IDirectDrawSurface4_GetClipper(
                              (IDirectDrawSurface4 *)g_pDDSPrimary,
                              &pClipper);
        if (hrClip == DDERR_NOCLIPPERATTACHED) {
            /* No clipper yet — create one */
            /* WIN32: g_pDD->CreateClipper(0, &pClipper, NULL) */
            IDirectDraw4_CreateClipper((IDirectDraw4 *)g_pDD,
                                       0, &pClipper, NULL);
        } else {
            /* Detach existing clipper before reconfiguring */
            IDirectDrawSurface4_SetClipper(
                (IDirectDrawSurface4 *)g_pDDSPrimary, NULL);
        }
        /* Bind clipper to the main window */
        IDirectDrawClipper_SetHWnd(pClipper, 0, hWndMain);
        /* Attach clipper to primary surface */
        IDirectDrawSurface4_SetClipper(
            (IDirectDrawSurface4 *)g_pDDSPrimary, pClipper);
    }

    return 1; /* success */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_SetPixelFormatGlobals                                      0x0045B9B0
 * ═══════════════════════════════════════════════════════════════════════════
 * Reads the pixel format from a DDSURFACEDESC and populates the global
 * pixel-format block at 0x485274.
 *
 * Parameters
 *   pSurf  — DirectDraw surface (used to call GetSurfaceDesc)
 *   pDesc  — DDSURFACEDESC whose pixel-format fields are at +0x58/5C/60
 * Returns : void
 *
 * Windows APIs: IDirectDrawSurface::GetSurfaceDesc (vtable offset 0x58)
 * LINUX: Eliminate entirely — SDL2 provides SDL_PixelFormat* directly.
 */
void DD_SetPixelFormatGlobals(IDirectDrawSurface *pSurf, DDSURFACEDESC *pDesc)
{
    DWORD rMask;

    if (pSurf == NULL || pDesc == NULL)
        return;

    /* Call GetSurfaceDesc to refresh the descriptor */
    /* WIN32: pSurf->GetSurfaceDesc(pDesc)                                  */
    IDirectDrawSurface4_GetSurfaceDesc((IDirectDrawSurface4 *)pSurf, pDesc);

    /* Red-channel bitmask lives at desc+0x58 (ddpfPixelFormat.dwRBitMask) */
    rMask = pDesc->ddpfPixelFormat.dwRBitMask;

    if (rMask == 0x7C00) {
        /* ── RGB555 (15-bit) ────────────────────────────────────────────  */
        /* Red bits 14-10, shift right 10 to extract.                       */
        g_pixFmtId   = 0x22B;   /* "555" decimal as format ID              */
        g_rShift     = 10;      /* red   shift                             */
        g_gBits      = 5;       /* green bit-count                         */
        g_whitePixel = 0x3DEF;  /* near-white pixel for this format        */
    } else {
        /* ── RGB565 (16-bit) ────────────────────────────────────────────  */
        g_pixFmtId   = 0x235;   /* "565" decimal as format ID              */
        g_rShift     = 11;      /* red   shift                             */
        g_gBits      = 6;       /* green bit-count                         */
        g_whitePixel = 0x7BEF;  /* near-white pixel for this format        */
    }

    /* Store raw masks from the surface descriptor */
    g_rMask = pDesc->ddpfPixelFormat.dwRBitMask; /* dwRBitMask            */
    g_gMask = pDesc->ddpfPixelFormat.dwGBitMask; /* dwGBitMask            */
    g_bMask = pDesc->ddpfPixelFormat.dwBBitMask; /* dwBBitMask            */

    g_whiteAlt = g_whitePixel; /* mirror copy written immediately after     */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_SetTransparentColorKey                                     0x0045BA50
 * ═══════════════════════════════════════════════════════════════════════════
 * Installs a magenta (R=31, G=0, B=31) source colour key on a surface so
 * that colour-keyed Blt calls treat magenta as fully transparent.
 *
 * Parameters
 *   pSurf  — target IDirectDrawSurface*
 *   unused — historical second parameter, ignored
 *
 * Windows APIs: IDirectDrawSurface::GetSurfaceDesc (vtable 0x58),
 *               IDirectDrawSurface::SetColorKey    (vtable 0x74)
 * LINUX: SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(fmt, 255, 0, 255))
 */
void DD_SetTransparentColorKey(IDirectDrawSurface *pSurf, DWORD unused)
{
    DDSURFACEDESC desc;
    DDCOLORKEY    ck;

    (void)unused;

    if (pSurf == NULL)
        return;

    /* Refresh descriptor to confirm surface is still valid */
    /* WIN32: pSurf->GetSurfaceDesc(&desc)                                  */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    IDirectDrawSurface4_GetSurfaceDesc((IDirectDrawSurface4 *)pSurf, &desc);

    /* Choose the correct encoding of magenta for the active pixel format.  *
     * RGB555: R=11111(10), G=00000(5), B=11111(0) = 0x7C1F               *
     * RGB565: R=11111(11), G=000000(5), B=11111(0) = 0xF81F              */
    if (g_pixFmtId == 0x22B)
        ck.dwColorSpaceLowValue = 0x7C1F; /* RGB555 magenta               */
    else /* g_pixFmtId == 0x235 — RGB565 */
        ck.dwColorSpaceLowValue = 0xF81F; /* RGB565 magenta               */
    ck.dwColorSpaceHighValue = ck.dwColorSpaceLowValue;

    /* WIN32: pSurf->SetColorKey(DDCKEY_SRCBLT, &ck)                       */
    /* LINUX: SDL_SetColorKey(sdlSurf, SDL_TRUE, magentaKey)               */
    IDirectDrawSurface4_SetColorKey((IDirectDrawSurface4 *)pSurf,
                                    DDCKEY_SRCBLT /*8*/, &ck);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_Shutdown                                                   0x0045BAA0
 * ═══════════════════════════════════════════════════════════════════════════
 * Releases all DirectDraw and auxiliary resources in safe dependency order.
 * Idempotent: guarded by 'if g_pDDRaw == NULL return'.
 *
 * Windows APIs: IDirectDrawSurface::Release (vtable 0x08),
 *               IDirectDraw::SetCooperativeLevel (vtable 0x50),
 *               IDirectDraw::Release (vtable 0x08)
 * LINUX: SDL_DestroyRenderer, SDL_DestroyWindow, SDL_Quit
 */
void DD_Shutdown(void)
{
    if (g_pDDRaw == NULL)
        return; /* already shut down */

    /* Release primary surface */
    if (g_pDDSPrimary != NULL) {
        IDirectDrawSurface4_Release((IDirectDrawSurface4 *)g_pDDSPrimary);
        g_pDDSPrimary = NULL;
    }

    /* Release back buffer */
    if (g_pDDSBack != NULL) {
        IDirectDrawSurface4_Release((IDirectDrawSurface4 *)g_pDDSBack);
        g_pDDSBack = NULL;
    }

    /* Release all auxiliary sprite/texture surfaces */
    DD_ReleaseAuxSurfaces(); /* FUN_0045C970 */

    /* Restore GDI before releasing the interface */
    /* WIN32: g_pDD->SetCooperativeLevel(NULL, DDSCL_NORMAL)               */
    /* LINUX: SDL_SetWindowFullscreen(win, 0)                               */
    IDirectDraw4_SetCooperativeLevel((IDirectDraw4 *)g_pDD, NULL,
                                     DDSCL_NORMAL /*8*/);

    /* Release upgraded interface */
    IDirectDraw4_Release((IDirectDraw4 *)g_pDD);
    g_pDD = NULL;

    /* Release raw interface */
    IDirectDraw_Release((IDirectDraw *)g_pDDRaw);
    g_pDDRaw = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_ReattachClipper                                            0x0045B940
 * ═══════════════════════════════════════════════════════════════════════════
 * Re-binds the window clipper to the primary surface after a cooperative-
 * level or display-mode change.  Creates a new clipper if none exists.
 *
 * Windows APIs: IDirectDrawSurface::GetClipper (vtable 0x3C),
 *               IDirectDraw::CreateClipper (vtable 0x10),
 *               IDirectDrawSurface::SetClipper (vtable 0x70),
 *               IDirectDrawSurface::Release (vtable 0x08)
 * LINUX: Not needed — SDL_RenderSetClipRect handles clipping per-frame.
 */
void DD_ReattachClipper(void)
{
    IDirectDrawClipper *pClipper = NULL;
    HRESULT hr;

    /* Try to get existing clipper from primary surface */
    /* WIN32: g_pDDSPrimary->GetClipper(&pClipper)                         */
    hr = IDirectDrawSurface4_GetClipper((IDirectDrawSurface4 *)g_pDDSPrimary,
                                        &pClipper);

    if (hr == DDERR_NOCLIPPERATTACHED) {
        /* No clipper — create a fresh one */
        /* WIN32: g_pDD->CreateClipper(0, &pClipper, NULL)                 */
        IDirectDraw4_CreateClipper((IDirectDraw4 *)g_pDD, 0, &pClipper, NULL);
    } else {
        /* Detach the existing clipper before reconfiguring */
        IDirectDrawSurface4_SetClipper((IDirectDrawSurface4 *)g_pDDSPrimary,
                                       NULL);
    }

    /* Re-attach clipper to the main window HWND */
    /* WIN32: pClipper->SetHWnd(0, hWndMain)  (vtable[8] = offset 0x20)   */
    IDirectDrawClipper_SetHWnd(pClipper, 0, hWndMain);

    /* Attach clipper back to surface */
    /* WIN32: g_pDDSPrimary->SetClipper(pClipper)                          */
    IDirectDrawSurface4_SetClipper((IDirectDrawSurface4 *)g_pDDSPrimary,
                                   pClipper);

    /* Release working surface reference */
    IDirectDrawSurface4_Release((IDirectDrawSurface4 *)g_pDDSPrimary);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_ReleaseAuxSurfaces                                         0x0045C970
 * ═══════════════════════════════════════════════════════════════════════════
 * Releases the six auxiliary DirectDraw surface pointers used for sprites
 * and textures (0x4FF0F8–0x4FF10C) and the palette/thumbnail object
 * at 0x4FF110.  All pointers are NULLed after release.
 *
 * Windows APIs: IDirectDrawSurface::Release (vtable 0x08)
 * LINUX: SDL_DestroyTexture for each SDL_Texture* equivalent
 */
static void DD_ReleaseAuxSurfaces(void)
{
    int i;

    /* Six standard IDirectDrawSurface pointers — released via vtable[2]   */
    IDirectDrawSurface **surfPtrs[6];
    surfPtrs[0] = &g_pSurf0; /* 0x4FF0F8 */
    surfPtrs[1] = &g_pSurf1; /* 0x4FF0FC */
    surfPtrs[2] = &g_pSurf2; /* 0x4FF100 */
    surfPtrs[3] = &g_pSurf3; /* 0x4FF104 */
    surfPtrs[4] = &g_pSurf4; /* 0x4FF108 */
    surfPtrs[5] = &g_pSurf5; /* 0x4FF10C */

    for (i = 0; i < 6; i++) {
        if (*surfPtrs[i] != NULL) {
            IDirectDrawSurface4_Release((IDirectDrawSurface4 *)*surfPtrs[i]);
            *surfPtrs[i] = NULL;
        }
    }

    /* Palette/thumbnail object — custom COM-like wrapper; released with   *
     * vtable[0] and argument 1 rather than standard Release()             */
    if (g_pPalOrThumb != NULL) {
        typedef void (*PalReleaseFn)(void *self, int flag);
        PalReleaseFn fn = *(PalReleaseFn *)g_pPalOrThumb;
        fn(g_pPalOrThumb, 1);
        g_pPalOrThumb = NULL;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_LoadBitmap                                                 0x00401000
 * ═══════════════════════════════════════════════════════════════════════════
 * Loads a BMP file from disk into a new DirectDraw offscreen surface.
 * Tries video memory first when useVideoMem==1; falls back to system
 * memory on DDERR_OUTOFVIDEOMEMORY (or the explicit system-memory path).
 *
 * Parameters
 *   path        — filesystem path to a BMP file
 *   unused      — reserved (was a param_2 in original; ignored)
 *   width       — desired width (0 = use bitmap's natural width)
 *   height      — desired height (0 = use bitmap's natural height)
 *   useVideoMem — 1 = prefer DDSCAPS_VIDEOMEMORY, else DDSCAPS_SYSTEMMEMORY
 * Returns
 *   IDirectDrawSurface* on success, NULL on failure.
 *   (Ghidra typed it as HDC — this is a decompiler artefact.)
 *
 * Windows APIs: GetFileAttributesA, LoadImageA, GetObjectA,
 *               IDirectDraw::CreateSurface, OutputDebugStringA, DeleteObject
 * LINUX: SDL_LoadBMP(path) → SDL_ConvertSurface → SDL_CreateTextureFromSurface
 */
IDirectDrawSurface * DD_LoadBitmap(
    LPCSTR path, DWORD unused, int width, int height, BOOL useVideoMem)
{
    HRESULT hr;
    HANDLE  hBitmap;
    BITMAP  bm;
    DDSURFACEDESC ddsd;
    IDirectDrawSurface *pSurf = NULL;

    (void)unused;

    /* ── 1. Check file exists ────────────────────────────────────────────*/
    /* WIN32: GetFileAttributesA                                            */
    /* LINUX: access(path, F_OK)                                           */
    if (GetFileAttributesA(path) == 0xFFFFFFFF)
        return NULL;

    /* ── 2. Load BMP via GDI ─────────────────────────────────────────── */
    /* WIN32: LoadImageA(hInstance, path, IMAGE_BITMAP, w, h, LR_LOADFROMFILE) */
    /* LINUX: SDL_LoadBMP(path)                                            */
    {
        HINSTANCE hInst = *(HINSTANCE *)((char *)g_appObj + 0x0C);
        hBitmap = LoadImageA(hInst,
                             path, IMAGE_BITMAP,
                             width, height,
                             LR_LOADFROMFILE);
    }
    if (hBitmap == NULL)
        return NULL;

    /* ── 3. Read bitmap dimensions ───────────────────────────────────── */
    /* WIN32: GetObjectA(hBitmap, sizeof(BITMAP), &bm)                     */
    GetObject(hBitmap, sizeof(BITMAP), &bm);
    if (width  == 0) width  = bm.bmWidth;
    if (height == 0) height = bm.bmHeight;

    /* ── 4. Build DDSURFACEDESC ──────────────────────────────────────── */
    memset(&ddsd, 0, sizeof(ddsd));   /* 0x7C bytes                        */
    ddsd.dwSize  = sizeof(ddsd);      /* 0x7C                              */
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; /* 7             */
    ddsd.dwWidth  = (DWORD)width;
    ddsd.dwHeight = (DWORD)height;

    /* ── 5. Compute DDSCAPS flags based on memory preference ────────── */
    /* 0x4040 = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY              *
     * 0x0840 = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY             */
    ddsd.ddsCaps.dwCaps = useVideoMem
        ? (DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY)   /* 0x4040 */
        : (DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY); /* 0x0840 */

    /* ── 6. Create DirectDraw surface ───────────────────────────────── */
    /* WIN32: g_pDD->CreateSurface(&ddsd, &pSurf, NULL)                   */
    /* LINUX: SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,          *
     *            SDL_TEXTUREACCESS_STATIC, width, height)                 */
    hr = IDirectDraw4_CreateSurface((IDirectDraw4 *)g_pDD,
                                    &ddsd,
                                    (LPDIRECTDRAWSURFACE4 *)&pSurf,
                                    NULL);

    if (hr != DD_OK && useVideoMem) {
        /* Video memory refused — log and retry in system memory           */
        /* WIN32: OutputDebugStringA                                        */
        /* LINUX: fprintf(stderr, ...)                                     */
        OutputDebugStringA("DDINIT - failed to create surface");
        ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
        hr = IDirectDraw4_CreateSurface((IDirectDraw4 *)g_pDD,
                                        &ddsd,
                                        (LPDIRECTDRAWSURFACE4 *)&pSurf,
                                        NULL);
        if (hr != DD_OK) {
            DeleteObject(hBitmap);
            return NULL;
        }
    }

    /* ── 7. Set magenta colour key ──────────────────────────────────── */
    /* WIN32: pSurf->SetColorKey(DDCKEY_SRCBLT, &ck)  via DD_SetTransparentColorKey */
    /* LINUX: SDL_SetColorKey(surf, SDL_TRUE, magentaKey)                  */
    DD_SetTransparentColorKey(pSurf, 0);

    /* ── 8. Copy GDI bitmap pixels into the surface ─────────────────── */
    /* WIN32: GDI StretchBlt path — see DD_CopyBitmapToSurface            */
    /* LINUX: SDL_BlitSurface / SDL_UpdateTexture                          */
    DD_CopyBitmapToSurface(pSurf, hBitmap, 0, 0, 0);

    /* ── 9. Release GDI bitmap handle ──────────────────────────────── */
    /* WIN32: DeleteObject                                                  */
    DeleteObject(hBitmap);

    return pSurf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_CopyBitmapToSurface                                        0x00401170
 * ═══════════════════════════════════════════════════════════════════════════
 * Copies a GDI HBITMAP into an IDirectDrawSurface using the GDI
 * compatibility DC path (GetDC/StretchBlt/ReleaseDC).
 *
 * Parameters
 *   pSurf   — destination IDirectDrawSurface*
 *   hBitmap — source GDI bitmap handle
 *   unused  — ignored
 *   srcW    — source width  (0 = use bitmap's natural width)
 *   srcH    — source height (0 = use bitmap's natural height)
 * Returns
 *   HRESULT from IDirectDrawSurface::GetDC (0 = success)
 *
 * Windows APIs: CreateCompatibleDC, SelectObject, GetObjectA,
 *               IDirectDrawSurface::Restore, IDirectDrawSurface::GetSurfaceDesc,
 *               IDirectDrawSurface::GetDC, StretchBlt,
 *               IDirectDrawSurface::ReleaseDC, DeleteDC, OutputDebugStringA
 * LINUX: SDL_BlitScaled(srcSurface, NULL, dstSurface, &dstRect)
 *        or SDL_RenderCopy(renderer, tex, &src, &dst)
 */
int DD_CopyBitmapToSurface(
    IDirectDrawSurface *pSurf, HANDLE hBitmap, DWORD unused, int srcW, int srcH)
{
    HDC  hdcSrc, hdcDst;
    int  destW, destH;
    BITMAP bm;
    DDSURFACEDESC desc;
    HRESULT hr;

    (void)unused;

    if (hBitmap == NULL || pSurf == NULL)
        return DDERR_GENERIC;

    /* ── Restore surface if lost ────────────────────────────────────── */
    /* WIN32: pSurf->Restore()  (vtable offset 0x6C = vtable[27])         */
    /* LINUX: SDL surfaces never go "lost"                                 */
    IDirectDrawSurface4_Restore((IDirectDrawSurface4 *)pSurf);

    /* ── Create GDI memory DC and select bitmap ─────────────────────── */
    /* WIN32: CreateCompatibleDC(NULL), SelectObject                       */
    hdcSrc = CreateCompatibleDC(NULL);
    if (hdcSrc == NULL)
        OutputDebugStringA("createcompatible dc failed");
    SelectObject(hdcSrc, hBitmap);

    /* ── Read bitmap dimensions ─────────────────────────────────────── */
    GetObject(hBitmap, sizeof(BITMAP), &bm);
    if (srcW == 0) srcW = bm.bmWidth;
    if (srcH == 0) srcH = bm.bmHeight;

    /* ── Query destination surface dimensions via GetSurfaceDesc ─────── */
    /* WIN32: pSurf->GetSurfaceDesc(&desc)  (vtable offset 0x58)           */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc); /* 0x7C */
    IDirectDrawSurface4_GetSurfaceDesc((IDirectDrawSurface4 *)pSurf, &desc);
    destW = (int)desc.dwWidth;
    destH = (int)desc.dwHeight;

    /* ── Obtain GDI DC for the DirectDraw surface ────────────────────── */
    /* WIN32: pSurf->GetDC(&hdcDst)  (vtable offset 0x44)                 */
    /* LINUX: SDL_LockSurface + direct pixel write, or SDL_UpdateTexture   */
    hr = IDirectDrawSurface4_GetDC((IDirectDrawSurface4 *)pSurf, &hdcDst);
    if (hr == DD_OK) {
        /* ── StretchBlt bitmap into surface DC ─────────────────────────  */
        /* WIN32: StretchBlt(ddHdc, 0,0, destW, destH,                     *
         *                   srcHdc, 0, 0, srcW, srcH, SRCCOPY)            */
        /* LINUX: SDL_BlitScaled(src, NULL, dst, &dstRect)                 */
        StretchBlt(hdcDst, 0, 0, destW, destH,
                   hdcSrc, 0, 0, srcW, srcH,
                   SRCCOPY);

        /* ── Release the surface DC ─────────────────────────────────── */
        /* WIN32: pSurf->ReleaseDC(hdcDst)  (vtable offset 0x68)           */
        IDirectDrawSurface4_ReleaseDC((IDirectDrawSurface4 *)pSurf, hdcDst);
    }

    DeleteDC(hdcSrc);
    return hr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_GetSurfaceDesc                                             0x004014E0
 * ═══════════════════════════════════════════════════════════════════════════
 * Retrieves the DDSURFACEDESC for a surface and writes it back into the
 * caller's struct.  Zero-inits 0x7C bytes then sets dwSize before calling.
 *
 * Parameters
 *   pSurf — IDirectDrawSurface* to query
 * Returns : void (desc returned via the surface vtable write-back)
 *
 * Windows APIs: IDirectDrawSurface::GetSurfaceDesc (vtable offset 0x58)
 * LINUX: SDL_QueryTexture / SDL_GetWindowSize
 */
void DD_GetSurfaceDesc(IDirectDrawSurface *pSurf)
{
    DDSURFACEDESC ddsd;

    /* Zero-fill — 31 DWORDs = 0x7C bytes */
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd); /* 0x7C — required by DirectDraw contract   */

    if (pSurf != NULL) {
        /* WIN32: pSurf->GetSurfaceDesc(&ddsd)  (vtable offset 0x58)       */
        IDirectDrawSurface4_GetSurfaceDesc((IDirectDrawSurface4 *)pSurf, &ddsd);
        /* Caller receives the result written into ddsd on the stack        *
         * (Ghidra write-back artefact via unaff_retaddr pattern)           */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_BlitToScreen                                               0x00401280
 * ═══════════════════════════════════════════════════════════════════════════
 * Blits a rectangle from the back buffer (g_pDDSBack) to the primary
 * surface (g_pDDSPrimary) with correct window-relative coordinate mapping.
 * Handles DDERR_SURFACELOST by calling Restore() and retrying.
 *
 * Parameters
 *   pSrcRect      — source rect in back-buffer coordinates
 *   hWnd          — window to map coordinates against
 *   pScrollOffset — optional (x,y) scroll origin to subtract first
 *   forceBlt      — if non-zero, forces a second DDBLT_WAIT blit
 * Returns : void
 *
 * Windows APIs: IsRectEmpty, OffsetRect, ClientToScreen, GetWindowRect,
 *               IntersectRect, IDirectDrawSurface::Blt,
 *               IDirectDrawSurface::Restore
 * LINUX: SDL_RenderCopy(renderer, backTex, &srcRect, &dstRect)
 *        — no surface-lost concept in SDL2
 */
void DD_BlitToScreen(
    RECT *pSrcRect, HWND hWnd, int *pScrollOffset, BOOL forceBlt)
{
    RECT  srcCopy, windowRect, clippedRect;
    POINT origin;
    HRESULT hr;

    /* ── Early-out on empty rect ─────────────────────────────────────── */
    /* WIN32: IsRectEmpty                                                   */
    if (IsRectEmpty(pSrcRect))
        return;

    /* ── Copy and apply optional scroll offset ──────────────────────── */
    srcCopy = *pSrcRect;
    if (pScrollOffset != NULL)
        OffsetRect(&srcCopy, -pScrollOffset[0], -pScrollOffset[1]);

    /* ── Convert to screen coordinates ─────────────────────────────── */
    /* WIN32: ClientToScreen, OffsetRect                                   */
    origin.x = 0;
    origin.y = 0;
    ClientToScreen(hWnd, &origin);
    OffsetRect(&srcCopy, origin.x, origin.y);

    /* ── Clip to window bounds ──────────────────────────────────────── */
    /* WIN32: GetWindowRect, IntersectRect                                  */
    GetWindowRect(hWnd, &windowRect);
    if (!IntersectRect(&clippedRect, &srcCopy, &windowRect))
        return;

    /* ── Primary Blt from back buffer ──────────────────────────────── */
    /* WIN32: g_pDDSPrimary->Blt(&dstRect, g_pDDSBack, &srcRect, flags, NULL) */
    /* LINUX: SDL_RenderCopy(renderer, backTex, &srcSDLRect, &dstSDLRect)  */
    if (!forceBlt) {
        hr = IDirectDrawSurface4_Blt(
                  (IDirectDrawSurface4 *)g_pDDSPrimary,
                  &srcCopy,
                  (IDirectDrawSurface4 *)g_pDDSBack,
                  pSrcRect, 0, NULL);
        if (hr == DDERR_SURFACELOST) {
            /* Restore surface and retry with DDBLT_WAIT */
            hr = IDirectDrawSurface4_Restore(
                      (IDirectDrawSurface4 *)g_pDDSPrimary);
            if (hr == DD_OK) {
                hr = IDirectDrawSurface4_Blt(
                          (IDirectDrawSurface4 *)g_pDDSPrimary,
                          &srcCopy,
                          (IDirectDrawSurface4 *)g_pDDSBack,
                          &origin,
                          DDBLT_WAIT, NULL);
            }
        }
    } else {
        /* forceBlt path: always uses DDBLT_WAIT */
        hr = IDirectDrawSurface4_Blt(
                  (IDirectDrawSurface4 *)g_pDDSPrimary,
                  &srcCopy,
                  (IDirectDrawSurface4 *)g_pDDSBack,
                  pSrcRect, 0, NULL);
        if (hr == DDERR_SURFACELOST) {
            hr = IDirectDrawSurface4_Restore(
                      (IDirectDrawSurface4 *)g_pDDSPrimary);
            if (hr == DD_OK)
                hr = IDirectDrawSurface4_Blt(
                          (IDirectDrawSurface4 *)g_pDDSPrimary,
                          &srcCopy,
                          (IDirectDrawSurface4 *)g_pDDSBack,
                          &origin,
                          DDBLT_WAIT, NULL);
        }
        if (hr == DD_OK)
            hr = IDirectDrawSurface4_Blt(
                      (IDirectDrawSurface4 *)g_pDDSPrimary,
                      &srcCopy,
                      (IDirectDrawSurface4 *)g_pDDSBack,
                      &origin,
                      DDBLT_WAIT, NULL);
    }

    if (hr != DD_OK)
        DD_HResultToString(hr); /* log error; also notifies scrolling system */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_SendFrameMessage                                           0x0045E1E0
 * ═══════════════════════════════════════════════════════════════════════════
 * Triggers a rendered frame by posting WM_USER+7 (0x407) to the main
 * window.  The window procedure performs the actual surface Flip or
 * present inside its WM_USER+7 handler.
 *
 * Parameters
 *   frameParam — low byte passed as wParam of the message
 * Returns : void
 *
 * Called from the game loop when g_renderActive (0x4AA4A4) != 0.
 *
 * Windows APIs: SendMessageA
 * LINUX: SDL_RenderPresent(renderer)  — call directly in the game loop;
 *        the message-pump indirection is eliminated entirely.
 */
void DD_SendFrameMessage(UINT frameParam)
{
    /* g_appObj is the main application object; +8 = hWndMain              */
    HWND hWnd = *(HWND *)((char *)g_appObj + 8);

    /* WIN32: Post WM_USER+7 to trigger the window procedure render path   */
    /* LINUX: SDL_RenderPresent(renderer)                                   */
    SendMessageA(hWnd,
                 0x407,                      /* WM_USER + 7                */
                 (WPARAM)(frameParam & 0xFF),/* low byte as wParam         */
                 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_ShowSplashScreen                                           0x0045E090
 * ═══════════════════════════════════════════════════════════════════════════
 * Clears the back buffer to black, constructs the splash/loading-screen
 * sprite object, centres it, and pushes it to the primary surface.
 *
 * Windows APIs: PlaySoundA, IDirectDrawSurface::Lock (vtable 0x44),
 *               GetStockObject, FillRect, IDirectDrawSurface::ReleaseDC (vtable 0x68),
 *               GetSystemMetrics, SendMessageA (via DD_BlitToScreen)
 * LINUX: Mix_HaltMusic(); SDL_SetRenderDrawColor+SDL_RenderClear;
 *        IMG_LoadTexture; SDL_RenderCopy centred; SDL_RenderPresent.
 */
void DD_ShowSplashScreen(void)
{
    HDC    hdcSurf;
    HBRUSH hBlack;
    int    screenH, screenW, splashH, posY, posX;

    /* ── Stop any currently playing sound ───────────────────────────── */
    /* WIN32: PlaySoundA(NULL, NULL, 0)                                    */
    /* LINUX: Mix_HaltMusic() / Mix_HaltChannel(-1)                       */
    PlaySoundA(NULL, NULL, 0);

    /* ── Lock back buffer and clear to black ────────────────────────── */
    /* WIN32: g_pDDSBack->GetDC(&hdcSurf)  (vtable offset 0x44)            */
    IDirectDrawSurface4_GetDC((IDirectDrawSurface4 *)g_pDDSBack, &hdcSurf);

    /* WIN32: GetStockObject(BLACK_BRUSH), FillRect                        */
    /* LINUX: SDL_SetRenderDrawColor(r,0,0,0,255); SDL_RenderClear(r)     */
    hBlack = (HBRUSH)GetStockObject(BLACK_BRUSH);
    FillRect(hdcSurf, &g_screenRect, hBlack);

    /* WIN32: g_pDDSBack->ReleaseDC(hdcSurf)  (vtable offset 0x68)         */
    IDirectDrawSurface4_ReleaseDC((IDirectDrawSurface4 *)g_pDDSBack, hdcSurf);

    /* ── Construct splash sprite object ─────────────────────────────── */
    /* Allocates 0x88 bytes, calls FUN_00405790 constructor with           *
     * resource ID 0x402 and stores result in g_pDDSSplash (0x4FD3D8)     *
     * FUN_00465ce0 is the game's memory allocator (malloc wrapper).       */
    {
        void *pSplashMem = FUN_00465ce0(0x88);
        if (pSplashMem != NULL)
            g_pDDSSplash = (IDirectDrawSurface *)FUN_00405790(
                               pSplashMem, 0x402, -1, 0, 0);
    }

    /* ── Centre the splash horizontally/vertically ──────────────────── */
    /* WIN32: GetSystemMetrics(SM_CYSCREEN/SM_CXSCREEN)                    */
    /* LINUX: SDL_GetRendererOutputSize / SDL_GetWindowSize                */
    screenH = GetSystemMetrics(SM_CYSCREEN);
    screenW = GetSystemMetrics(SM_CXSCREEN);

    if (g_pDDSSplash != NULL) {
        /*
         * Read the sprite height word from the object at internal offset.
         * The expression mirrors the original: half-height centred vertically,
         * quarter-width centred horizontally (game uses a quarter-screen sprite).
         */
        splashH = (int)(*(unsigned short *)((char *)g_pDDSSplash + 0x26) >> 1);
        posY    = screenH / 2 - splashH;
        posX    = (screenW + (screenW >> 31 & 3)) >> 2;

        /* Position and show the sprite via its vtable methods */
        typedef void (*SetPosFn)(int x, int y);
        typedef void (*SetVisFn)(int vis);
        typedef void (*BlitFn)(int a, int b, int c, int d, int e, int f);

        {
            void **vtbl = *(void ***)g_pDDSSplash;
            ((SetPosFn)(vtbl[3]))(posX, posY);       /* vtable[3] = SetPos  */
            ((SetVisFn)(vtbl[7]))(0);                 /* vtable[7] = SetVisible */
            ((BlitFn)  (vtbl[11]))(                   /* vtable[11] = Blit    */
                ((int *)g_pDDSSplash)[2],
                ((int *)g_pDDSSplash)[3],
                ((int *)g_pDDSSplash)[4],
                ((int *)g_pDDSSplash)[5],
                0, 0);
        }
    }

    /* ── Blit back buffer to screen ─────────────────────────────────── */
    DD_BlitToScreen(&g_screenRect, hWndMain, NULL, FALSE);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DS_Init                                                       0x0045B7E0
 * ═══════════════════════════════════════════════════════════════════════════
 * Creates and configures the DirectSound device.  Reads volume settings
 * from the config store (INI/registry) and applies them.
 *
 * Returns 1 on success, 0 on failure.
 *
 * Windows APIs: DirectSound COM interface (via vtable); wrapped in SEH.
 * LINUX: Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
 *        Mix_AllocateChannels(n); Mix_Volume(-1, savedVolume);
 */
UINT DS_Init(void)
{
    DWORD volLow, volMed, volHigh;

    /* Guard: don't double-initialise */
    if (g_pDS != NULL)
        return 1; /* already initialised */

    /* ── Allocate and construct DirectSound device object ───────────── */
    {
        void *pMem = FUN_00465ce0(0xB8); /* 0xB8-byte allocation          */
        if (pMem == NULL) {
            g_pDS = NULL;
            return 0;
        }
        g_pDS = (IDirectSound *)FUN_00412bd0(pMem); /* DS wrapper ctor    */
    }
    if (g_pDS == NULL)
        return 0;

    /* ── Verify device initialised correctly ────────────────────────── */
    if (!FUN_00412c50(g_pDS)) {
        typedef void (*DSRelFn)(void *self, int flag);
        DSRelFn relFn = *(DSRelFn *)g_pDS;
        relFn((void *)g_pDS, 1);
        g_pDS = NULL;
        return 0;
    }

    /* ── Bind to primary window ─────────────────────────────────────── */
    FUN_004130a0(g_pDS, g_aadm0c, g_aadm10);

    /* ── Load and apply volume settings ─────────────────────────────── */
    /* Defaults: Low=0x4B, Med=0x4B, High=0x4E                            */
    /* LINUX: Mix_Volume(-1, savedVolume * MIX_MAX_VOLUME / 100)           */
    if (g_configStore == NULL) {
        volLow  = 0x4B;
        volMed  = 0x4B;
        volHigh = 0x4E;
    } else {
        volLow  = FUN_00452d60(g_configStore, "Sound", "VolumeLow",  0x4B);
        volMed  = FUN_00452d60(g_configStore, "Sound", "VolumeMed",  0x4B);
        volHigh = FUN_00452d60(g_configStore, "Sound", "VolumeHigh", 0x4E);
    }
    FUN_00413630(g_pDS, volLow, volMed, volHigh, volHigh);

    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DS_SaveAndShutdown                                            0x0045BB20
 * ═══════════════════════════════════════════════════════════════════════════
 * Saves current volume settings to the config store then releases the
 * DirectSound device.  Companion to DS_Init.
 *
 * Windows APIs: DirectSound vtable; config store helpers.
 * LINUX: Mix_CloseAudio(); save volume to SDL-app config file.
 */
void DS_SaveAndShutdown(void)
{
    if (g_pDS == NULL)
        return;

    /* Persist current volume levels to the config store */
    if (g_configStore != NULL) {
        DWORD *dsFields = (DWORD *)g_pDS;
        FUN_00452db0(g_configStore, "Sound", "VolumeLow",  dsFields[4]);
        FUN_00452db0(g_configStore, "Sound", "VolumeMed",  dsFields[3]);
        FUN_00452db0(g_configStore, "Sound", "VolumeHigh", dsFields[2]);
    }

    /* Detach from window and release */
    FUN_00412ee0(g_pDS, *(HWND *)((char *)g_appObj + 8));
    if (g_pDS != NULL) {
        typedef void (*DSRelFn)(void *self, int flag);
        DSRelFn relFn = *(DSRelFn *)g_pDS;
        relFn((void *)g_pDS, 1); /* vtable[0](1) = Release/shutdown */
    }
    g_pDS = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * APP_InitWindow                                                0x004204D0
 * ═══════════════════════════════════════════════════════════════════════════
 * Creates the main application window (full-screen sized) and initialises
 * the two manager sub-objects that depend on it.  Wrapped in SEH.
 *
 * Parameters
 *   this      — application object pointer (fields at known offsets)
 *   hWndParent — parent window (may be NULL for a top-level window)
 * Returns 1 on success, 0 if main window creation failed.
 *
 * Notable offsets in 'this':
 *   +0x04  HINSTANCE hInstance
 *   +0x08  HWND hWndMain  (filled by FUN_00425b70)
 *   +0xF8  HICON hIcon    (loaded here)
 *   +0x20C HWND hWndChild (child render-area window created here)
 *   +0x21C void *pManager0 (sub-object for input or graphics manager)
 *   +0x220 void *pManager1 (sub-object for a second manager)
 *   +0x214 WNDPROC origProc (saved original child WNDPROC for subclassing)
 *   +0x15C–0x168 client rect coordinates for child window placement
 *
 * Windows APIs: GetDesktopWindow, GetClientRect, LoadIconA,
 *               CreateWindowExA, PostMessageA, SetWindowLongA, SetFocus
 * LINUX: SDL_CreateWindow + SDL_SetWindowFullscreen; sub-objects remain C++.
 */
int APP_InitWindow(void *self, HWND hWndParent)
{
    HWND    hDesktop;
    HICON   hIcon;
    RECT    desktopRect;
    HINSTANCE hInst;
    HWND    hWndChild;
    BOOL    ok;
    void   *pMgr0Mem, *pMgr0;
    void   *pMgr1Mem, *pMgr1;
    LONG    oldProc;

    /* ── Get full desktop dimensions ─────────────────────────────────── */
    /* WIN32: GetDesktopWindow, GetClientRect                              */
    /* LINUX: SDL_GetDesktopDisplayMode(0, &dm) → dm.w, dm.h              */
    hDesktop = GetDesktopWindow();
    GetClientRect(hDesktop, &desktopRect);

    hInst = *(HINSTANCE *)((char *)self + 4);

    /* ── Load game icon (resource 0x65 = 101) ───────────────────────── */
    /* WIN32: LoadIconA(hInstance, MAKEINTRESOURCE(101))                   */
    hIcon = LoadIconA(hInst, MAKEINTRESOURCEA(0x65));
    *(HICON *)((char *)self + 0xF8) = hIcon;

    /* ── Register window class ──────────────────────────────────────── */
    FUN_00421500((int)self);

    /* ── Create main window at full desktop size ────────────────────── */
    /* WIN32: calls FUN_00425b70 which wraps CreateWindowExA              */
    /* LINUX: SDL_CreateWindow("Lego Loco", 0, 0, desktopW, desktopH, ...) */
    ok = (BOOL)FUN_00425b70(self, 0, hWndParent,
                            desktopRect.left, desktopRect.top,
                            desktopRect.right  - desktopRect.left,
                            desktopRect.bottom - desktopRect.top,
                            NULL, hIcon, 0);
    if (!ok)
        return 0;

    /* ── Allocate and initialise manager sub-object 0 (0x1E4 bytes) ─── */
    pMgr0Mem = (void *)FUN_00465ce0(0x1E4);
    pMgr0    = (pMgr0Mem != NULL)
               ? FUN_00440F20(pMgr0Mem, *(DWORD *)((char *)self + 4), 0x1F6)
               : NULL;
    *(void **)((char *)self + 0x21C) = pMgr0;
    FUN_004412F0(pMgr0, *(HWND *)((char *)self + 8));

    /* ── Allocate and initialise manager sub-object 1 (0x260 bytes) ─── */
    pMgr1Mem = (void *)FUN_00465ce0(0x260);
    pMgr1    = (pMgr1Mem != NULL)
               ? FUN_00408AA0(pMgr1Mem, *(DWORD *)((char *)self + 4), 0x1F9)
               : NULL;
    *(void **)((char *)self + 0x220) = pMgr1;
    FUN_00408F00(pMgr1, *(HWND *)((char *)self + 8));

    /* ── Create child render-area window ────────────────────────────── */
    /* WIN32: CreateWindowExA(WS_EX_CLIENTEDGE, className, title,         *
     *            WS_CHILD, x, y, w, h, hWndMain, (HMENU)0x411, hInst, 0) */
    /* LINUX: No separate child window needed; SDL renders to the main     *
     *        window directly.                                              */
    hWndChild = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        (LPCSTR)&DAT_0047e464,     /* registered class name               */
        (LPCSTR)&DAT_004851d0,     /* window title                        */
        WS_CHILD,
        *(int *)((char *)self + 0x15C),
        *(int *)((char *)self + 0x160),
        *(int *)((char *)self + 0x164) - *(int *)((char *)self + 0x15C),
        *(int *)((char *)self + 0x168) - *(int *)((char *)self + 0x160),
        *(HWND *)((char *)self + 8),   /* parent = hWndMain               */
        (HMENU)(ULONG_PTR)0x411,
        *(HINSTANCE *)((char *)self + 4),
        NULL);
    *(HWND *)((char *)self + 0x20C) = hWndChild;

    /* ── Set child window font (WM_SETFONT) ─────────────────────────── */
    PostMessageA(hWndChild, WM_SETFONT,
                 (WPARAM)g_defaultFont, 1);

    /* ── Set redraw count (WM_SETREDRAW custom) ─────────────────────── */
    PostMessageA(*(HWND *)((char *)self + 0x20C), 0xC5, 0x0B, 0);

    /* ── Subclass the child window ──────────────────────────────────── */
    /* WIN32: SetWindowLongA(hChild, GWL_WNDPROC=-4, newProc=0x420B20)    */
    oldProc = SetWindowLongA(*(HWND *)((char *)self + 0x20C),
                             GWL_WNDPROC,
                             0x420B20 /* child WNDPROC address            */);
    *(LONG *)((char *)self + 0x214) = oldProc;

    /* ── Give focus to child render window ──────────────────────────── */
    /* WIN32: SetFocus                                                     */
    /* LINUX: SDL_RaiseWindow / SDL_SetWindowInputFocus                   */
    SetFocus(*(HWND *)((char *)self + 0x20C));

    return 1; /* success */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DD_HResultToString                                            0x0045BBC0
 * ═══════════════════════════════════════════════════════════════════════════
 * Translates a DirectDraw / Direct3D / D3DRM HRESULT error code to a
 * static English string.  Covers the complete DDraw5 / D3D5 error table.
 * Returns "Unrecognized error value." for unknown codes.
 *
 * Parameters  hr — HRESULT from any DirectDraw API call
 * Returns     pointer to a static string in .rdata
 *
 * No Windows API calls.
 * LINUX: Replace with SDL_GetError() / a custom mapping table if
 *        the error strings are needed in the SDL2 port.
 */
static char * DD_HResultToString(HRESULT hr)
{
    switch (hr) {
    /* ── Common HRESULT ─────────────────────────────────────────────── */
    case 0:                   return "No error.";
    case (HRESULT)0x80004005: return "Generic failure.";
    case (HRESULT)0x80004001: return "Action not supported.";
    /* ── DDERR codes (selected; full table has ~80 entries) ─────────── */
    case (HRESULT)0x88760057: return "One or more of the parameters passed to the function are incorrect.";
    case (HRESULT)0x8876000E: return "DirectDraw does not have enough memory to perform the operation.";
    case (HRESULT)0x88760028: return "Support is currently not available.";
    case (HRESULT)0x8876000A: return "This surface can not be attached.";
    case (HRESULT)0x88760005: return "This object is already initialized.";
    case (HRESULT)0x8876005A: return "Height of rectangle provided is not a multiple of the required alignment.";
    case (HRESULT)0x88760037: return "An exception was encountered while performing the requested operation.";
    case (HRESULT)0x88760064: return "One or more of the caps bits passed to the function are incorrect.";
    case (HRESULT)0x8876005F: return "Unable to match primary surface creation request with existing primary surface.";
    case (HRESULT)0x88760078: return "DirectDraw does not support the requested mode.";
    case (HRESULT)0x8876006E: return "DirectDraw does not support the provided pixel format.";
    case (HRESULT)0x88760091: return "The pixel format was invalid as specified.";
    case (HRESULT)0x88760082: return "DirectDraw received a pointer that was an invalid DIRECTDRAW object.";
    case (HRESULT)0x880760A0: return "Operation could not be carried out because there is no GDI present.";
    case (HRESULT)0x88760096: return "Rectangle provided was invalid.";
    case (HRESULT)0x880760B4: return "Operation could not be carried out because there is no overlay present.";
    case (HRESULT)0x880760AA: return "There is no 3D present.";
    case (HRESULT)0x880760D2: return "Operation could not be carried out because the source and destination rectangles are on the same surface.";
    case (HRESULT)0x880760CD: return "No cliplist available.";
    case (HRESULT)0x880760D7: return "Surface doesn't currently have a color key.";
    case (HRESULT)0x880760D4: return "Create function called without DirectDraw being initialized.";
    case (HRESULT)0x880760E1: return "Operation requires the application to have exclusive mode but the application does not have exclusive mode.";
    case (HRESULT)0x880760DC: return "Operation could not be carried out because there is no hardware support of the dest color key.";
    case (HRESULT)0x880760F0: return "There is no GDI present.";
    case (HRESULT)0x880760E6: return "Flipping visible surfaces is not supported.";
    case (HRESULT)0x880760FF: return "Requested item was not found.";
    case (HRESULT)0x880760FA: return "Operation could not be carried out because there is no hardware present.";
    case DDERR_NOCLIPPERATTACHED: return "No clipper object attached to surface.";
    case DDERR_SURFACELOST:       return "Access to this surface is being refused because the surface is already locked by another thread.";
    /* ── D3DERR codes (abbreviated) ──────────────────────────────────── */
    case (HRESULT)0x887602BC: return "D3DERR_BADMAJORVERSION";
    case (HRESULT)0x887602BD: return "D3DERR_BADMINORVERSION";
    case (HRESULT)0x887602C6: return "D3DERR_EXECUTE_CREATE_FAILED";
    /* ── D3DRMERR codes (abbreviated) ────────────────────────────────── */
    case (HRESULT)0x8876030D: return "D3DRMERR_BADOBJECT";
    case (HRESULT)0x88760313: return "The file was not found.";
    default:                  return "Unrecognized error value.";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DS_HResultToString                                            0x0045C2E0
 * ═══════════════════════════════════════════════════════════════════════════
 * Translates a DirectSound HRESULT (DSERR_*) to a static English string.
 * Prefixes strings with "Channel error" to distinguish from DD errors.
 *
 * Parameters  hr — HRESULT from any DirectSound API call
 * Returns     pointer to a static string in .rdata
 *
 * No Windows API calls.
 * LINUX: Replace with SDL_mixer error strings or custom table.
 */
static char * DS_HResultToString(HRESULT hr)
{
    switch (hr) {
    case 0:                   return "No error.";
    case (HRESULT)0x8876000E: return "Channel error: DSERR_OUTOFMEMORY";
    case (HRESULT)0x80004001: return "Channel error: DSERR_UNSUPPORTED";
    case (HRESULT)0x80004002: return "Channel error: DSERR_NOINTERFACE";
    case (HRESULT)0x80004005: return "Channel error: DSERR_GENERIC";
    case (HRESULT)0x8878000A: return "Channel error: DSERR_ALLOCATED";
    case (HRESULT)0x88760057: return "Channel error: DSERR_INVALIDPARAM";
    case (HRESULT)0x88780032: return "Channel error: DSERR_INVALIDCALL";
    case (HRESULT)0x8878001E: return "Channel error: DSERR_CONTROLUNAVAIL";
    case (HRESULT)0x88780064: return "Channel error: DSERR_BADFORMAT";
    case (HRESULT)0x88780046: return "Channel error: DSERR_PRIOLEVELNEEDED";
    case (HRESULT)0x88780082: return "Channel error: DSERR_ALREADYINITIALIZED";
    case (HRESULT)0x88780078: return "Channel error: DSERR_NODRIVER";
    case (HRESULT)0x880780A0: return "Channel error: DSERR_OTHERAPPHASPRIO";
    case (HRESULT)0x880780AA: return "Channel error: DSERR_UNINITIALIZED";
    default:                  return "Unrecognized error value.";
    }
}

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * IDirectDraw vtable reference (byte offsets used in this file)
 * ═══════════════════════════════════════════════════════════════════════════
 *  0x08 = vtable[2]  = Release
 *  0x10 = vtable[4]  = CreateClipper
 *  0x18 = vtable[6]  = CreateSurface
 *  0x20 = vtable[8]  = EnumDisplayModes
 *  0x50 = vtable[20] = SetCooperativeLevel
 *
 * IDirectDrawSurface vtable reference (byte offsets used in this file)
 * ═══════════════════════════════════════════════════════════════════════════
 *  0x08 = vtable[2]  = Release
 *  0x14 = vtable[5]  = Blt
 *  0x3C = vtable[15] = GetClipper
 *  0x44 = vtable[17] = GetDC
 *  0x58 = vtable[22] = GetSurfaceDesc
 *  0x68 = vtable[26] = ReleaseDC
 *  0x6C = vtable[27] = Restore
 *  0x70 = vtable[28] = SetClipper
 *  0x74 = vtable[29] = SetColorKey
 *
 * IDirectDrawClipper vtable reference
 * ═══════════════════════════════════════════════════════════════════════════
 *  0x08 = vtable[2]  = Release
 *  0x20 = vtable[8]  = SetHWnd
 *
 * LINUX / SDL2 replacement summary
 * ═══════════════════════════════════════════════════════════════════════════
 * Surface model:
 *   g_pDDSPrimary → SDL_Window* / SDL_Renderer*
 *   g_pDDSBack    → SDL_Texture* (SDL_TEXTUREACCESS_TARGET) or SDL_Surface*
 *   Sprite surfaces (DD_LoadBitmap) → SDL_Surface* from SDL_LoadBMP /
 *       IMG_Load, converted to SDL_Texture* for hardware rendering.
 *
 * Pixel format:
 *   DD_SetPixelFormatGlobals eliminated; SDL2 uses 32-bit RGBA internally.
 *   Any 16-bit arithmetic on g_pixFmtId/g_rShift must be ported to 8-bit
 *   per-channel operations.
 *
 * Blitting:
 *   IDirectDrawSurface::Blt → SDL_RenderCopy (textures) or SDL_BlitSurface.
 *   DDERR_SURFACELOST retry loop → unnecessary; SDL handles device loss.
 *   Colour-key transparency → SDL_SetColorKey(surf, SDL_TRUE,
 *       SDL_MapRGB(surf->format, 255, 0, 255))
 *
 * Clipper:
 *   Dropped; use SDL_RenderSetClipRect or SDL_Rect* on blit calls.
 *
 * Flip / frame trigger:
 *   DD_SendFrameMessage (SendMessageA WM_USER+7) → SDL_RenderPresent(renderer)
 *   called directly in the game loop.
 *
 * Cooperative level / window:
 *   DDSCL_EXCLUSIVE fullscreen → SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN)
 *   DDSCL_NORMAL windowed      → SDL_SetWindowFullscreen(win, 0)
 *   APP_InitWindow             → SDL_CreateWindow + SDL_CreateRenderer
 *
 * Audio:
 *   DS_Init / DS_SaveAndShutdown → Mix_OpenAudio / Mix_CloseAudio
 *   Volume: Mix_Volume(-1, vol * MIX_MAX_VOLUME / 100)
 *
 * Key headers / libs:
 *   #include <SDL2/SDL.h>
 *   #include <SDL2/SDL_image.h>
 *   #include <SDL2/SDL_mixer.h>
 *   Link: -lSDL2 -lSDL2_image -lSDL2_mixer
 */
