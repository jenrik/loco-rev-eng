/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: CURSOR_INPUT — Custom software cursor compositing and input capture
 * Original: loco.exe (Windows 95/98, DirectX 5 era)
 * Developer: Intelligent Games for LEGO Media
 *
 * This file was produced by reverse engineering the original binary with Ghidra.
 * Windows API calls are marked:  // WIN32: FunctionName(args) -> description
 * Linux/SDL2 replacements are:   // LINUX: equivalent SDL2/POSIX call
 *
 * Analyzed functions:
 *   0x004140a0  UpdateClientRect   — cache client rect + clip dimensions
 *   0x00414130  InitCursorResources — load cursor .ani resources and staging surface
 *   0x00414290  SetCursorCapture   — activate/deactivate OS cursor capture
 *   0x00414340  SetCursorState     — change active cursor animation state
 *   0x00414a80  OnWindowActivate   — handle window focus loss
 *   0x00414b80  OnWindowDestroy    — handle WM_DESTROY; post quit message
 *   0x00414bb0  WaitForSurface     — blocking poll until primary surface ready
 *   0x00414c20  RenderCursor       — per-frame cursor compositing and blit
 *
 * Porting strategy summary:
 *   - DirectDraw surface operations -> SDL_Surface / SDL_BlitSurface
 *   - Win32 SetCapture/ReleaseCapture -> SDL_CaptureMouse + SDL_SetWindowGrab
 *   - ShowCursor(FALSE) loop -> single SDL_ShowCursor(SDL_DISABLE) call
 *   - GetCursorPos (screen coords) -> SDL_GetGlobalMouseState
 *   - ClientToScreen -> SDL_GetWindowPosition + manual add
 *   - GetClientRect -> SDL_GetWindowSize
 *   - DDSURFACEDESC2 surface create -> SDL_CreateRGBSurface(0, 256, 256, 32, ...)
 *   - WM_DESTROY + PostQuitMessage -> SDL_DestroyWindow + SDL_PushEvent(SDL_QUIT)
 *   - Sleep(10) + ExitProcess(1) -> SDL_Delay(10) + exit(1) (WaitForSurface only)
 *   - OutputDebugStringA -> SDL_Log / fprintf(stderr)
 */

#include "input.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * ARCHITECTURE OVERVIEW
 * =========================================================================
 *
 * Lego Loco replaces the Windows system cursor with an animated DirectDraw
 * sprite.  The replacement has two layers:
 *
 *   1. A per-frame software cursor renderer (CursorManager / CursorRenderer):
 *        - Reads hardware mouse position via GetCursorPos every frame
 *        - Erases old cursor by re-blitting the background store surface
 *        - Blits the current animation frame from the horizontal sprite sheet
 *          onto the back-buffer via IDirectDrawSurface::BltFast
 *
 *   2. A state machine (InputCursor) that selects which sprite resource ID
 *      to display based on the current g_GameMode and what the cursor hovers:
 *        - LoadingMode  (1): skip cursor update
 *        - PlaceMode    (3): InputCursor_PlaceModeCursor
 *        - BuildMode    (4): InputCursor_BuildModeCursor
 *        - Other modes      : force CURSOR_RES_IDLE (0x1400)
 *
 * The OS cursor is hidden with a ShowCursor(0) loop at startup.
 * It is temporarily restored only during long loading operations via
 * LoadCursorFromFileA("CURSORS\busy_ani.ani") + SetCursor.
 *
 * -----------------------------------------------------------------------
 * CURSOR SPRITE SHEET FORMAT
 * -----------------------------------------------------------------------
 * Each cursor resource (e.g. 0x1400) is a single horizontal strip stored
 * in a DirectDraw offscreen surface (DDSCAPS_OFFSCREENPLAIN):
 *
 *   [ frame 0 | frame 1 | frame 2 | ... | frame N-1 ]
 *   <-fw-----><-fw-----><-fw----->     <-fw--------->
 *
 * where fw = CursorAnimData.frameWidth.  The frame height is always
 * CursorAnimData.frameHeight and all frames start at row 0 (src_y = 0).
 *
 * Animation frame selection in the hot path:
 *   uint16_t src_x = anim_frame * frameWidth;   // column of current frame
 *   uint16_t src_y = 0;                          // always top row
 *
 * LINUX: DirectDraw surface -> SDL_Texture; BltFast -> SDL_RenderCopy.
 * =========================================================================
 *
 * Global Variable Definitions  (PE addresses from loco.exe)
 * ========================================================================= */

/*
 * g_ResourceManager  (DAT_004855e8)
 *
 * Global resource manager vtable object.  Passed to FUN_00446ea0 in
 * InitCursorResources to look up cursor animation resources by resource ID.
 * The cursor animation resource IDs fall in the 0x1400 range (type 5),
 * which are always marked persistent so they survive cache flushes.
 *
 * WIN32: opaque C++ vtable object with method at vtable[1] = getLocoBitmap()
 * LINUX: replaced by custom resource-cache struct; no vtable dispatch needed
 */
void *g_ResourceManager = NULL;       /* 0x004855e8 — global resource manager */

/*
 * g_CursorSurface  (DAT_004fd3cc)
 *
 * Shared 256x256 offscreen surface used as staging buffer for cursor
 * compositing.  All CursorManager instances share one backing surface;
 * ownership is tracked via g_CursorSurfaceRefCount.
 *
 * Two-stage blit pipeline:
 *   [cursor .ani frame strip] --DDBLT_KEYSRC--> [this 256x256 surface]
 *   [this 256x256 surface]   --DDBLT_WAIT---> [primary DirectDraw surface]
 *
 * WIN32: IDirectDrawSurface4*  created with DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY
 * LINUX: SDL_Surface*  created with SDL_CreateRGBSurface(0, 256, 256, 32, ...)
 */
void *g_CursorSurface = NULL;         /* 0x004fd3cc — shared cursor staging surface */

/*
 * g_CursorSurfaceRefCount  (DAT_004fd3d0)
 *
 * Reference count for g_CursorSurface.  Incremented once per CursorManager
 * that attaches to the shared surface in InitCursorResources.  The surface
 * must only be freed when this reaches zero.  Preserve this model in the
 * Linux port.
 */
int   g_CursorSurfaceRefCount = 0;   /* 0x004fd3d0 — g_CursorSurface refcount */

/*
 * g_DirectDraw  (DAT_00485440)
 *
 * Global DirectDraw factory object used in InitCursorResources to create the
 * 256x256 staging surface.  Vtable slot +0x18 = CreateSurface.
 *
 * WIN32: IDirectDraw4*
 * LINUX: not needed — SDL_CreateRGBSurface is a direct function call.
 */
void *g_DirectDraw = NULL;            /* 0x00485440 — global IDirectDraw4* factory */

/*
 * g_PrimaryDDrawSurface  (DAT_004fd3c0)
 *
 * Global primary or back-buffer DirectDraw surface.  Used in RenderCursor
 * as the final blit destination after the cursor is composited into the
 * 256x256 staging surface.  Vtable slot +0x14 = Blt.
 *
 * WIN32: IDirectDrawSurface4*
 * LINUX: SDL_Surface* representing the backbuffer, or SDL_Texture* if GPU
 */
void *g_PrimaryDDrawSurface = NULL;   /* 0x004fd3c0 — global primary/backbuffer surface */

/*
 * g_InputDispatcher  (DAT_004854c8)
 *
 * Unknown subsystem object passed to FUN_00411dc0 during cursor deactivation
 * in SetCursorCapture.  Likely the input manager or window message dispatcher.
 * Notified when custom cursor capture is released so it can update its state.
 */
void *g_InputDispatcher = NULL;       /* 0x004854c8 — input manager / msg dispatcher */

/* =========================================================================
 * UpdateClientRect  (0x004140a0)
 * =========================================================================
 *
 * Recomputes and caches the window client-area rectangle and derived
 * offset/dimension fields in the CursorManager object.
 *
 * Only executes when the active flag at this+0xdb is non-zero.  This guard
 * prevents UpdateClientRect from running after OnWindowDestroy has cleared
 * the active flag, avoiding use-after-free on the HWND.
 *
 * Two parallel rect copies are maintained:
 *   +0xf4  raw GetClientRect output (live)
 *   +0x104 working copy used by the renderer in RenderCursor (0x00414c20)
 * The width/height at +0xe4/+0xe8 and +0xec/+0xf0 appear to serve
 * different clipping contexts (e.g. scroll-adjusted vs raw window).
 *
 * On Win32, GetClientRect always returns {left=0, top=0, right=w, bottom=h}
 * so the subtraction (right-left, bottom-top) is defensive programming.
 * On Linux, SDL_GetWindowSize similarly fills width and height directly.
 *
 * Called whenever the viewport or window geometry changes (e.g. on resize,
 * on scroll, or at startup after the window is created).
 *
 * WIN32: GetClientRect(this->hwnd, &rawRect)
 *          — fills RECT with {0, 0, clientWidth, clientHeight}
 * LINUX: SDL_GetWindowSize(window, &w, &h)
 *          — returns drawable area size; no title-bar offset needed
 */
void UpdateClientRect(CursorManager *mgr)
{
    RECT rawRect;

    /*
     * Guard: skip entirely if the CursorManager is not active.
     * The active flag (this+0xdb) is set during initialization and cleared
     * by OnWindowDestroy before DestroyWindow is called.
     */
    if (!mgr->active) {
        return;
    }

    /*
     * Obtain the current client-area rectangle.
     *
     * WIN32: GetClientRect(mgr->hwnd, &rawRect)
     *   Fills rawRect with {left=0, top=0, right=clientW, bottom=clientH}.
     *   The HWND is stored at this+0x08.
     *
     * LINUX: SDL_GetWindowSize((SDL_Window*)mgr->hwnd, &w, &h)
     *   Then: rawRect = {0, 0, w, h}
     *   SDL client area equals the window drawable area; no title-bar offset.
     */
    /* WIN32: GetClientRect(mgr->hwnd, &rawRect); */
    /* LINUX: { int w,h; SDL_GetWindowSize((SDL_Window*)mgr->hwnd,&w,&h);
     *           rawRect.left=0; rawRect.top=0; rawRect.right=w; rawRect.bottom=h; } */

    /*
     * Cache the raw rectangle at this+0xf4 (live GetClientRect output).
     * This is the authoritative copy of the window client rect.
     */
    mgr->clientRect = rawRect;  /* writes to this+0xf4 */

    /*
     * Compute pixel dimensions and store in the primary clip fields.
     *   clientWidth  (this+0xe4) = rawRect.right  - rawRect.left
     *   clientHeight (this+0xe8) = rawRect.bottom - rawRect.top
     * On Win32 these are always rawRect.right and rawRect.bottom since
     * left=top=0, but the subtraction is kept for correctness.
     */
    mgr->clientWidth  = rawRect.right  - rawRect.left;  /* this+0xe4 */
    mgr->clientHeight = rawRect.bottom - rawRect.top;   /* this+0xe8 */

    /*
     * Copy the same coordinates to the working clip rect block at this+0x104.
     * This second copy is consumed by RenderCursor (0x00414c20) when it
     * performs the ClientToScreen conversion and builds the destination rect
     * for the final blit to the primary surface.
     */
    mgr->clipLeft   = rawRect.left;    /* this+0x104 */
    mgr->clipTop    = rawRect.top;     /* this+0x108 */
    mgr->clipRight  = rawRect.right;   /* this+0x10c */
    mgr->clipBottom = rawRect.bottom;  /* this+0x110 */

    /*
     * Recompute clipping dimensions into the secondary width/height fields.
     * These appear to serve a different clipping context from the primary
     * pair at +0xe4/+0xe8 (e.g. used when scroll offsets are applied).
     *   clipWidth  (this+0xec) = rawRect.right  - rawRect.left
     *   clipHeight (this+0xf0) = rawRect.bottom - rawRect.top
     */
    mgr->clipWidth  = rawRect.right  - rawRect.left;  /* this+0xec */
    mgr->clipHeight = rawRect.bottom - rawRect.top;   /* this+0xf0 */
}

/* =========================================================================
 * InitCursorResources  (0x00414130)
 * =========================================================================
 *
 * One-time initialization of the cursor subsystem.
 *
 * Steps performed:
 *   1. Load cursor animation resource 0x1400 (idle cursor) from the global
 *      resource manager at g_ResourceManager via FUN_00446ea0.
 *   2. Load cursor animation resource 0x1403 (grab/busy cursor) the same way.
 *   3. For each resource, call vtable slot +4 (getLocoBitmap) to obtain the
 *      associated LOCOBITMAP surface pointer.
 *   4. Call FUN_0042a3d0 to lock/prepare each surface for pixel access.
 *   5. Cache frame width (resource+0x14) and frame height (resource+0x16)
 *      as uint32 into this+0x3c and this+0x40.
 *   6. If the shared 256x256 cursor staging surface (g_CursorSurface) has
 *      not yet been allocated, create it using the DirectDraw factory.
 *   7. Initialize the new surface via FUN_004014e0 and FUN_0045b9b0/FUN_0045ba50.
 *   8. Store the surface pointer in this+0x5c and increment g_CursorSurfaceRefCount.
 *
 * Resource ID notes:
 *   0x1400 = idle/default cursor
 *   0x1401 = hover cursor (inferred; not loaded here)
 *   0x1402 = unknown (inferred; not loaded here)
 *   0x1403 = grab/busy cursor (offset +3 from idle)
 *   All fall in the type-5 range (0x1400..0x17FF) which is always persistent.
 *
 * Reference-count model:
 *   g_CursorSurfaceRefCount is incremented once per CursorManager instance
 *   that attaches to g_CursorSurface.  The surface must only be destroyed
 *   when the count reaches zero.  Preserve this invariant in the Linux port.
 *
 * WIN32: IDirectDraw4::CreateSurface called via vtable slot +0x18 on
 *        g_DirectDraw, with a DDSURFACEDESC2 on the stack:
 *          dwSize   = 0x7c (DDSURFACEDESC2_SIZE)
 *          dwFlags  = 0x07 (DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH)
 *          dwHeight = 256, dwWidth = 256
 *          ddsCaps.dwCaps = 0x840 (DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY)
 *
 * LINUX: SDL_CreateRGBSurface(0, 256, 256, 32, Rmask, Gmask, Bmask, Amask)
 *        for the shared staging surface.
 *        Custom resource loader replaces FUN_00446ea0 — load .ani file from
 *        art-res/cursors/ and parse frame strips.
 *        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
 *                          SDL_TEXTUREACCESS_STREAMING, 256, 256)
 *        if GPU-side compositing is preferred over CPU SDL_Surface blits.
 */
void InitCursorResources(CursorManager *mgr)
{
    void *idleRes;          /* resource object for 0x1400 (idle cursor) */
    void *grabRes;          /* resource object for 0x1403 (grab/busy cursor) */
    void *idleSurface;      /* LOCOBITMAP surface for the idle animation */
    void *grabSurface;      /* LOCOBITMAP surface for the grab animation */

    /*
     * Step 1: Load the idle cursor animation resource (ID 0x1400).
     *
     * FUN_00446ea0 is the resource manager's lazy-load lookup:
     *   if the slot is unloaded, it triggers a batch load for the type group,
     *   then returns the loaded CResourceBase-derived object pointer.
     * Resource 0x1400 is type 5 (bits[13:10] = 5); type-5 slots are always
     * marked persistent so they survive cache flushes.
     *
     * WIN32: opaque C++ vtable call to g_ResourceManager
     * LINUX: loco_GetResource(g_ResourceManager, CURSOR_RES_IDLE)
     */
    idleRes = FUN_00446ea0(g_ResourceManager, CURSOR_RES_IDLE); /* 0x1400 */
    mgr->pIdleCursorRes = (CursorAnimResource *)idleRes; /* this+0x98 */

    /*
     * Step 2: Load the grab/busy cursor animation resource (ID 0x1403).
     * Resource IDs 0x1401..0x1402 likely exist for hover and other states
     * but are not loaded here.
     */
    grabRes = FUN_00446ea0(g_ResourceManager, CURSOR_RES_GRAB); /* 0x1403 */
    mgr->pGrabCursorRes = (CursorAnimResource *)grabRes; /* this+0xa4 */

    /*
     * Step 3: For each resource, obtain the backing LOCOBITMAP surface
     * by calling vtable slot +4 (index 1, getLocoBitmap).
     *
     * WIN32: ((int (**)(void*))(**(int***)idleRes + 1))(idleRes)
     *   — C++ thiscall through the vtable
     * LINUX: loco_CursorRes_GetSurface(idleRes)
     *   — direct function call; no vtable dispatch needed
     */
    if (idleRes != NULL) {
        /* WIN32: idleSurface = (*(int(**)(void*))(*(int*)idleRes + 4))(idleRes); */
        /* LINUX: idleSurface = loco_CursorRes_GetSurface(idleRes); */
        idleSurface = NULL; /* placeholder: result of vtable[1](idleRes) */
        mgr->pIdleSurface = idleSurface; /* this+0x94 */

        /*
         * Step 4: Lock/prepare the surface for pixel access.
         * FUN_0042a3d0 internally calls the DirectDraw Lock() method and
         * stores the surface pitch and pixel pointer.
         *
         * WIN32: IDirectDrawSurface4::Lock via vtable
         * LINUX: SDL_LockSurface(idleSurface); pitch at surface->pitch
         */
        FUN_0042a3d0(idleSurface);

        /*
         * Step 5: Cache frame dimensions from the resource object.
         * The idle cursor's frame size drives the frame-column calculation
         * in RenderCursor (frameWidth * frameIndex = column start pixel).
         *
         *   frameWidth  at resource+0x14  -> this+0x3c
         *   frameHeight at resource+0x16  -> this+0x40
         */
        if (idleRes != NULL) {
            CursorAnimResource *res = (CursorAnimResource *)idleRes;
            mgr->frameWidth  = (uint32_t)res->frameWidth;  /* this+0x3c */
            mgr->frameHeight = (uint32_t)res->frameHeight; /* this+0x40 */
        }

        /* Cache the surface pitch from the locked surface object (+0x1c) */
        if (idleSurface != NULL) {
            mgr->idleSurfacePitch = *(DWORD *)((char *)idleSurface + 0x1c);
            /* this+0x90 */
        }
    }

    if (grabRes != NULL) {
        /* WIN32: grabSurface = (*(int(**)(void*))(*(int*)grabRes + 4))(grabRes); */
        /* LINUX: grabSurface = loco_CursorRes_GetSurface(grabRes); */
        grabSurface = NULL; /* placeholder: result of vtable[1](grabRes) */
        mgr->pGrabSurface = grabSurface; /* this+0xa0 */

        FUN_0042a3d0(grabSurface);

        if (grabSurface != NULL) {
            mgr->grabSurfacePitch = *(DWORD *)((char *)grabSurface + 0x1c);
            /* this+0x9c */
        }
    }

    /*
     * Step 6: Create the shared 256x256 cursor staging surface if not yet
     * allocated.  The NULL-check on g_CursorSurface means only the first
     * CursorManager to be initialized pays the allocation cost; all
     * subsequent instances share the same surface.
     */
    if (g_CursorSurface == NULL) {

        /*
         * Build a DDSURFACEDESC2 on the stack to describe the surface.
         * The structure must be exactly 0x7c (124) bytes with dwSize=0x7c.
         *
         * WIN32 fields:
         *   dwSize   = 0x7c (DDSURFACEDESC2_SIZE) — identifies structure version
         *   dwFlags  = 0x07 (DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH)
         *   dwHeight = 0x100 (256)
         *   dwWidth  = 0x100 (256)
         *   ddsCaps.dwCaps = 0x840
         *     DDSCAPS_OFFSCREENPLAIN (0x040) — not the primary/back surface
         *     DDSCAPS_SYSTEMMEMORY   (0x800) — in system RAM for CPU pixel access
         *
         * LINUX: replace the entire DirectDraw creation path with:
         *   g_CursorSurface = SDL_CreateRGBSurface(
         *       0,              // flags (always 0 for SDL2)
         *       CURSOR_SURFACE_DIM, CURSOR_SURFACE_DIM,
         *       32,             // bits per pixel
         *       0x00FF0000,     // Rmask (ARGB8888)
         *       0x0000FF00,     // Gmask
         *       0x000000FF,     // Bmask
         *       0xFF000000      // Amask
         *   );
         *   if (g_CursorSurface == NULL) {
         *       SDL_Log("Failed to create cursor surface: %s", SDL_GetError());
         *       exit(1);
         *   }
         */
        DDSURFACEDESC2_Cursor desc;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize         = DDSURFACEDESC2_SIZE;    /* 0x7c */
        desc.dwFlags        = DDSD_FLAGS_CURSOR;      /* 0x07 */
        desc.dwHeight       = CURSOR_SURFACE_DIM;     /* 256 */
        desc.dwWidth        = CURSOR_SURFACE_DIM;     /* 256 */
        desc.ddsCaps_dwCaps = DDSCAPS_CURSOR_SURFACE; /* 0x840 */

        /*
         * WIN32: call g_DirectDraw vtable slot +0x18 = CreateSurface.
         *   typedef HRESULT (*CreateSurfaceFn)(void*, DDSURFACEDESC2*, void**, void*);
         *   CreateSurfaceFn createSurf = ((CreateSurfaceFn**)g_DirectDraw)[0][0x18/4];
         *   void *newSurface = NULL;
         *   createSurf(g_DirectDraw, &desc, &newSurface, NULL);
         *   g_CursorSurface = newSurface;
         *
         * LINUX: (see SDL_CreateRGBSurface comment above)
         */
        /* [DirectDraw vtable dispatch or SDL_CreateRGBSurface here] */

        /*
         * Step 7: Initialize the newly created surface.
         * FUN_004014e0 sets up internal bookkeeping on the LOCOBITMAP wrapper.
         * FUN_0045b9b0 and FUN_0045ba50 perform additional surface setup
         * (likely palette assignment or color-key configuration).
         *
         * LINUX: SDL surfaces created via SDL_CreateRGBSurface need no extra
         * init; the mask arguments in the creation call handle color layout.
         * If a color key is needed: SDL_SetColorKey(surface, SDL_TRUE, colorKey).
         */
        if (g_CursorSurface != NULL) {
            FUN_004014e0(g_CursorSurface);
            FUN_0045b9b0(g_CursorSurface);
            FUN_0045ba50(g_CursorSurface);
        }
    }

    /*
     * Step 8: Cache the shared surface pointer in this CursorManager and
     * increment the reference count.  Every CursorManager that uses the
     * surface must increment the count; the surface is only freed when
     * the count reaches zero (on shutdown).
     */
    mgr->pCursorSurface = g_CursorSurface; /* this+0x5c */
    g_CursorSurfaceRefCount += 1;          /* DAT_004fd3d0 */
}

/* =========================================================================
 * SetCursorCapture  (0x00414290)
 * =========================================================================
 *
 * Activates or deactivates the game's custom cursor mode.
 *
 * ACTIVATION path (deactivate == 0):
 *   - If not already active (deactivated flag at this+0x58 is already 0), no-op.
 *   - Otherwise:
 *       1. Call SetCapture(mgr->hwnd) to lock all mouse input to this window.
 *       2. Loop ShowCursor(FALSE) until the Win32 per-thread show-cursor
 *          reference counter drops below zero.  This guarantees the OS hardware
 *          cursor is fully hidden regardless of how many times other code has
 *          called ShowCursor(TRUE).
 *       3. The deactivated flag at this+0x58 is NOT set here (it stays 0),
 *          indicating the cursor is now active.
 *
 * DEACTIVATION path (deactivate != 0):
 *   - If not currently deactivated (flag at this+0x58 is already 1), no-op.
 *   - Otherwise:
 *       1. Set this+0x58 = 1 (mark as deactivated).
 *       2. If GetCapture() returns our HWND, call ReleaseCapture().
 *       3. Call FUN_0045b940 (flush cursor surface).
 *       4. Call FUN_00414fb0(mgr, 1) (cursor blit with clear flag).
 *       5. Call FUN_00414ef0(mgr) (position restore / cleanup).
 *       6. Call FUN_00411dc0(g_InputDispatcher) (notify input subsystem).
 *       7. If this+0x88 (hasOverlayCursor) is set, also call FUN_00415440(mgr, 1).
 *
 * ShowCursor loop note:
 *   Win32 maintains a per-thread reference counter for cursor visibility.
 *   Each ShowCursor(TRUE) increments it; ShowCursor(FALSE) decrements it.
 *   The cursor is hidden only when the counter is < 0.  The game loops
 *   ShowCursor(FALSE) until the return value goes negative to counteract
 *   any previous ShowCursor(TRUE) calls from other subsystems.
 *
 *   SDL has no counter — SDL_ShowCursor is a simple boolean.  The loop
 *   MUST be removed; replace with a single SDL_ShowCursor(SDL_DISABLE).
 *   On deactivation, restore with SDL_ShowCursor(SDL_ENABLE).
 *
 * WIN32: GetCapture()       — returns HWND that currently holds mouse capture
 *        SetCapture(hwnd)   — locks mouse events to this window
 *        ReleaseCapture()   — releases mouse capture
 *        ShowCursor(FALSE)  — decrements per-thread show-cursor counter;
 *                             loop until return value < 0
 * LINUX: SDL_CaptureMouse(SDL_TRUE/SDL_FALSE)  — replaces SetCapture/ReleaseCapture
 *        SDL_SetWindowGrab(window, SDL_TRUE/SDL_FALSE) — confines events to window
 *        SDL_ShowCursor(SDL_DISABLE/SDL_ENABLE) — single call; no loop needed
 */
void SetCursorCapture(CursorManager *mgr, int deactivate)
{
    if (deactivate == 0) {
        /*
         * Activate: take OS mouse capture and hide the hardware cursor.
         * Only proceed if the cursor is currently deactivated (flag == 1).
         * If the flag is already 0, we already own capture — nothing to do.
         */
        if (mgr->deactivated == 0) {
            return; /* already active */
        }

        /*
         * Lock mouse input to the game window.
         *
         * WIN32: SetCapture(mgr->hwnd)
         *   Routes all mouse messages to this HWND even when the cursor
         *   moves outside the window.
         *
         * LINUX: SDL_CaptureMouse(SDL_TRUE)
         *   Allows the application to receive mouse events globally while
         *   the button is held (or always, until SDL_CaptureMouse(SDL_FALSE)).
         *   For full window-confinement:
         *     SDL_SetWindowGrab((SDL_Window*)mgr->hwnd, SDL_TRUE)
         */
        /* WIN32: SetCapture((HWND)mgr->hwnd); */
        /* LINUX: SDL_CaptureMouse(SDL_TRUE);
         *        SDL_SetWindowGrab((SDL_Window*)mgr->hwnd, SDL_TRUE); */

        /*
         * Hide the OS hardware cursor.
         *
         * WIN32: Loop ShowCursor(FALSE) until the return value < 0.
         *   The Win32 show-cursor counter starts at 0 on a fresh thread.
         *   If other code has called ShowCursor(TRUE) N times, we must
         *   call ShowCursor(FALSE) N+1 times to reach -1 (hidden).
         *   The loop handles any arbitrary initial counter value.
         *
         * LINUX: Single SDL_ShowCursor(SDL_DISABLE) call.
         *   SDL uses a flag, not a counter.  No loop needed or correct.
         *   Calling SDL_ShowCursor multiple times is idempotent.
         */
        /* WIN32: { int r; do { r = ShowCursor(FALSE); } while (r >= 0); } */
        /* LINUX: SDL_ShowCursor(SDL_DISABLE); */

    } else {
        /*
         * Deactivate: release capture and restore the OS hardware cursor.
         * Only proceed if the cursor is currently active (flag == 0).
         * If the flag is already 1, we've already deactivated — nothing to do.
         */
        if (mgr->deactivated != 0) {
            return; /* already deactivated */
        }

        /*
         * Mark as deactivated.  This flag (this+0x58) is checked by
         * RenderCursor (0x00414c20) to decide whether to composite and blit.
         * Value 1 = cursor capture released, OS cursor visible.
         * Value 0 = game cursor active, OS cursor hidden.
         */
        mgr->deactivated = 1; /* this+0x58 */

        /*
         * Release OS mouse capture if our window currently holds it.
         *
         * WIN32: if (GetCapture() == mgr->hwnd) ReleaseCapture();
         *   GetCapture() returns the HWND currently holding capture,
         *   or NULL if no window holds it.  Only release if it's ours
         *   to avoid releasing another window's capture accidentally.
         *
         * LINUX: SDL_CaptureMouse(SDL_FALSE);
         *        SDL_SetWindowGrab((SDL_Window*)mgr->hwnd, SDL_FALSE);
         *   SDL_CaptureMouse has no concept of "who holds it" — just release.
         */
        /* WIN32: if (GetCapture() == (HWND)mgr->hwnd) ReleaseCapture(); */
        /* LINUX: SDL_CaptureMouse(SDL_FALSE);
         *        SDL_SetWindowGrab((SDL_Window*)mgr->hwnd, SDL_FALSE); */

        /*
         * Restore the OS hardware cursor.
         *
         * WIN32: ShowCursor(TRUE)  (single call; counter increments to 0 = visible)
         * LINUX: SDL_ShowCursor(SDL_ENABLE)  (single call)
         *
         * Note: the original deactivation path does NOT call ShowCursor(TRUE).
         * The OS cursor reappears via the subordinate cleanup functions below.
         * On Linux, SDL_ShowCursor(SDL_ENABLE) should be called here explicitly.
         */
        /* LINUX: SDL_ShowCursor(SDL_ENABLE); */

        /* Flush the cursor compositing surface */
        FUN_0045b940();

        /* Blit with clear flag = 1 to erase the previous cursor position */
        FUN_00414fb0(mgr, 1);

        /* Restore cursor position state / cleanup */
        FUN_00414ef0(mgr);

        /* Notify the input dispatcher that custom capture has been released */
        FUN_00411dc0(g_InputDispatcher);

        /*
         * If a secondary overlay cursor is active, clear it too.
         * The overlay flag at this+0x88 is set when a second cursor
         * animation layer is composited over the primary cursor.
         */
        if (mgr->hasOverlayCursor) {
            FUN_00415440(mgr, 1);
        }
    }
}

/* =========================================================================
 * SetCursorState  (0x00414340)
 * =========================================================================
 *
 * Changes the active cursor animation state.
 *
 * The cursor subsystem uses a state machine to select which .ani strip is
 * displayed.  State 0 means the cursor is hidden (no compositing occurs in
 * RenderCursor).  Non-zero state IDs correspond to named cursor modes
 * (e.g. idle, hover, grab) each with their own CursorAnimData.
 *
 * Frame counter:
 *   this+0x48 (currentAnimFrame) counts the current frame within the strip.
 *   It is reset to 0 on every state change and advances each render tick in
 *   RenderCursor, wrapping when it reaches CursorAnimData+0x160 (frameCount).
 *
 * Early-exit condition:
 *   If the new state ID equals the current state ID AND the new ID is 0
 *   (cursor hidden -> cursor hidden), the function returns immediately.
 *   This avoids redundant clears when the cursor is already hidden.
 *
 * Dirty-rect reset (resetPos flag):
 *   If param_3 is set, zeroes the eight fields at this+0x68..this+0x84:
 *     this+0x68 (destLeft), this+0x6c (destTop),
 *     this+0x70 (destRight), this+0x74 (destBottom),
 *     this+0x78..this+0x84 (auxiliary dirty-rect / prev-frame tracking).
 *   This forces RenderCursor to treat the next blit as the first blit
 *   at a fresh position.
 *
 * Force-redraw (forceRedraw flag):
 *   If param_4 is set and the cursor is currently active (this+0x58 == 0),
 *   triggers an immediate redraw:
 *     FUN_0045b940()        — flush cursor compositing surface
 *     FUN_00414fb0(mgr, 0) — blit with normal (non-clear) flag
 *     FUN_00414ef0(mgr)    — position restore
 *   And if hasOverlayCursor:
 *     FUN_00415440(mgr, 0) — redraw overlay
 *
 * WIN32: no Win32 calls; pure state-machine bookkeeping
 * LINUX: fully portable as-is; no SDL or POSIX calls required
 *
 * @param mgr        CursorManager instance
 * @param stateId    new cursor state ID (0 = hidden)
 * @param pAnimData  CursorAnimData pointer for the new state
 * @param resetPos   non-zero: zero the eight dirty-rect / position cache fields
 * @param forceRedraw non-zero: if cursor is active, trigger immediate redraw
 */
void SetCursorState(CursorManager *mgr,
                    int             stateId,
                    CursorAnimData *pAnimData,
                    int             resetPos,
                    int             forceRedraw)
{
    /*
     * Early-exit: if the requested state is already the current state AND
     * both are 0 (cursor hidden), nothing needs to change.
     * This does NOT early-exit when transitioning between two non-zero states
     * with the same ID; the frame counter is always reset in that case.
     */
    if (stateId == mgr->cursorStateId && stateId == 0) {
        return;
    }

    /*
     * Store the new state ID and animation data pointer.
     * The animation data pointer at this+0x44 is used by RenderCursor to
     * read hotspot offsets (+0x32, +0x34) and frame count (+0x160).
     */
    mgr->cursorStateId  = stateId;   /* this+0x14 */
    mgr->pCurrentAnim   = pAnimData; /* this+0x44 */

    /*
     * Reset the animation frame counter to 0.
     * On the next RenderCursor tick, frame 0 of the new animation is displayed.
     * The counter advances each tick and wraps at pAnimData->frameCount.
     */
    mgr->currentAnimFrame = 0; /* this+0x48 */

    /*
     * Optional dirty-rect / position cache clear (resetPos flag).
     * Zeroes the destination rect fields and the auxiliary dirty-rect block:
     *   this+0x68 = destLeft     this+0x6c = destTop
     *   this+0x70 = destRight    this+0x74 = destBottom
     *   this+0x78..this+0x84    = aux dirty-rect (4 x LONG)
     * After this, RenderCursor treats the next blit as if no previous blit
     * exists (e.g. no erase-old-position step is needed).
     */
    if (resetPos) {
        mgr->destLeft   = 0; /* this+0x68 */
        mgr->destTop    = 0; /* this+0x6c */
        mgr->destRight  = 0; /* this+0x70 */
        mgr->destBottom = 0; /* this+0x74 */
        /* auxiliary dirty-rect block (this+0x78..+0x84) */
        memset(mgr->auxRect, 0, sizeof(mgr->auxRect));
    }

    /*
     * Optional immediate redraw (forceRedraw flag).
     * Only triggered when the cursor is currently active (deactivated == 0).
     * This allows callers to change cursor appearance mid-frame without
     * waiting for the next RenderCursor call.
     */
    if (forceRedraw && mgr->deactivated == 0) {
        FUN_0045b940();
        FUN_00414fb0(mgr, 0); /* normal blit, not a clear */
        FUN_00414ef0(mgr);

        if (mgr->hasOverlayCursor) {
            FUN_00415440(mgr, 0);
        }
    }
}

/* =========================================================================
 * OnWindowActivate  (0x00414a80)
 * =========================================================================
 *
 * Window focus-change handler.  Called when the game window loses activation.
 *
 * Receives a window handle parameter (param_1 in the original).  Compares
 * it to the stored HWND at this+0x08; if they match, the game window has
 * just lost focus, and the custom cursor surface is flushed and capture
 * is released.
 *
 * The HWND comparison guards against stale or cross-window events: if a
 * different window loses activation, this CursorManager does nothing.
 *
 * Cleanup sequence (same as the deactivation path in SetCursorCapture):
 *   1. FUN_0045b940()      — flush cursor compositing surface
 *   2. FUN_00414fb0(this, 1) — blit with clear flag
 *   3. FUN_00414ef0(this)  — position restore / cleanup
 *   4. FUN_00415440(this, 1) — if hasOverlayCursor: clear overlay
 *
 * Paired with SetCursorCapture:
 *   - Activation re-enables capture (SetCursorCapture(mgr, 0))
 *   - Deactivation (here or via SetCursorCapture(mgr, 1)) releases it
 *
 * Always returns 0.
 *
 * WIN32: driven by WM_ACTIVATE or WM_KILLFOCUS window message
 * LINUX: wire to SDL event loop:
 *   if (event.type == SDL_WINDOWEVENT &&
 *       event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
 *       OnWindowActivate(mgr, (void*)SDL_GetWindowFromID(event.window.windowID));
 *
 * @param mgr   CursorManager instance
 * @param hwnd  HWND that lost focus (compared against mgr->hwnd)
 * @return      always 0
 */
int OnWindowActivate(CursorManager *mgr, void *hwnd)
{
    /*
     * Only act when the event concerns our game window.
     * On Win32, multiple top-level windows can lose activation independently.
     * On Linux, SDL_WINDOWEVENT_FOCUS_LOST includes a windowID; map it to
     * SDL_GetWindowFromID and compare to (SDL_Window*)mgr->hwnd.
     */
    if (hwnd != mgr->hwnd) {
        return 0; /* not our window — ignore */
    }

    /*
     * Our game window just lost focus.
     * Flush the cursor surface so no stale cursor graphic remains on screen.
     */
    FUN_0045b940();

    /*
     * Blit with clear flag = 1 to erase the cursor at its last position.
     * param_1 = 1 causes the blit to clear rather than composite.
     */
    FUN_00414fb0(mgr, 1);

    /* Restore cursor position state / perform post-blit cleanup */
    FUN_00414ef0(mgr);

    /*
     * If a secondary overlay cursor is active, clear it as well.
     * The overlay cursor renders a separate animation layer on top of the
     * primary cursor (e.g. a busy spinner over the grab cursor).
     */
    if (mgr->hasOverlayCursor) {
        FUN_00415440(mgr, 1);
    }

    return 0; /* always 0 */
}

/* =========================================================================
 * OnWindowDestroy  (0x00414b80)
 * =========================================================================
 *
 * WM_DESTROY equivalent handler.  Performs final window teardown:
 *
 *   1. Clear the active flag at param_1+0xdb to 0.
 *      This prevents UpdateClientRect (FUN_004140a0) and RenderCursor from
 *      running after the window handle becomes invalid.
 *   2. Call DestroyWindow on the HWND at param_1+0x08.
 *   3. If the parent-window field at param_1+0x0c is NULL, this is the root
 *      (top-level) window.  Post WM_QUIT via PostQuitMessage(0) to terminate
 *      the message loop cleanly.
 *
 * Note: param_1 in the original decompiled code is a raw pointer to the
 * CursorManager; this is documented as mgr throughout.
 *
 * WIN32: DestroyWindow(hwnd)     — sends WM_DESTROY to the window
 *        PostQuitMessage(0)      — posts WM_QUIT; terminates GetMessage loop
 * LINUX: SDL_DestroyWindow(window)
 *        { SDL_Event ev = {.type=SDL_QUIT}; SDL_PushEvent(&ev); }
 *
 * @param mgr   CursorManager instance (original: param_1 raw pointer)
 * @return      always 0
 */
int OnWindowDestroy(CursorManager *mgr)
{
    /*
     * Clear the active flag.
     * This is the master enable for the entire CursorManager; all hot-path
     * functions (UpdateClientRect, RenderCursor) check this flag first and
     * return immediately when it is 0.
     *
     * Must be cleared BEFORE calling DestroyWindow so that any re-entrant
     * messages triggered by DestroyWindow (e.g. WM_NCPAINT) do not attempt
     * to use the now-invalid window handle.
     */
    mgr->active = 0; /* this+0xdb = 0 */

    /*
     * Destroy the window.
     *
     * WIN32: DestroyWindow((HWND)mgr->hwnd)
     *   Sends WM_DESTROY and WM_NCDESTROY to the window and its children.
     *   The window handle becomes invalid after this call returns.
     *
     * LINUX: SDL_DestroyWindow((SDL_Window*)mgr->hwnd)
     *   Frees the SDL window object.  Triggers SDL_WINDOWEVENT_CLOSE.
     */
    /* WIN32: DestroyWindow((HWND)mgr->hwnd); */
    /* LINUX: SDL_DestroyWindow((SDL_Window*)mgr->hwnd); */

    /*
     * If this is the top-level window (parentHwnd == NULL), signal the
     * application to quit.  Child or popup windows do NOT post WM_QUIT;
     * only the root window's destruction should terminate the message loop.
     *
     * WIN32: PostQuitMessage(0)
     *   Posts WM_QUIT to the thread's message queue.
     *   GetMessage() returns FALSE on WM_QUIT, breaking the message loop.
     *
     * LINUX: Push an SDL_QUIT event:
     *   SDL_Event quitEvent;
     *   memset(&quitEvent, 0, sizeof(quitEvent));
     *   quitEvent.type = SDL_QUIT;
     *   SDL_PushEvent(&quitEvent);
     *   The SDL event loop sees SDL_QUIT and exits.
     */
    if (mgr->parentHwnd == NULL) {
        /* WIN32: PostQuitMessage(0); */
        /* LINUX: { SDL_Event ev; ev.type = SDL_QUIT; SDL_PushEvent(&ev); } */
    }

    return 0; /* always 0 */
}

/* =========================================================================
 * WaitForSurface  (0x00414bb0)
 * =========================================================================
 *
 * Blocking poll that waits for the game's primary rendering surface to
 * become ready before any rendering can proceed.
 *
 * Mechanism:
 *   Calls vtable method at (**(int**)(mgr+0x38))[0x44/4] in a loop — a
 *   'query status' or 'is-ready' call on the DirectDraw surface object
 *   stored at mgr->pGameSurface.  On each iteration, if the surface is not
 *   ready, sleeps SURFACE_WAIT_INTERVAL_MS (10) milliseconds then retries.
 *
 *   After SURFACE_WAIT_MAX_RETRIES (1000) attempts without success (total
 *   timeout: ~10 seconds), the function calls FUN_00463600 (likely an
 *   error-logging assert) then ExitProcess(1) to terminate the process.
 *
 *   On success, returns the surface-status value (non-zero = ready).
 *
 * Porting note:
 *   SDL_CreateRGBSurface is synchronous — it returns NULL on failure or a
 *   valid pointer on success.  There is no async surface readiness concept
 *   in SDL.  Replace the entire polling loop with a NULL-check:
 *
 *   if (g_PrimaryDDrawSurface == NULL) {
 *       SDL_Log("Primary surface not ready: %s", SDL_GetError());
 *       SDL_Quit();
 *       exit(1);
 *   }
 *   return 1;
 *
 *   If the vtable call is querying a different async resource (e.g. a
 *   background texture load), replace Sleep(10) with SDL_Delay(10) and
 *   keep the iteration logic but remove the vtable dispatch.
 *
 * WIN32: Sleep(10)        — suspends the calling thread for ~10 ms
 *        ExitProcess(1)   — terminates the process unconditionally
 *        vtable query     — IDirectDrawSurface4 DDSCAPS/status check
 * LINUX: SDL_Delay(10)    — replaces Sleep(10)
 *        exit(1)          — replaces ExitProcess(1); call SDL_Quit() first
 *        NULL-check after SDL_CreateRGBSurface — replaces vtable query
 *
 * @param mgr  CursorManager whose pGameSurface vtable is polled
 * @return     surface status value from the vtable query on success
 */
int WaitForSurface(CursorManager *mgr)
{
    int retries = 0;
    int surfaceStatus = 0;

    /*
     * LINUX simplification: SDL surfaces are always immediately available.
     * Replace this entire function with:
     *   if (g_PrimaryDDrawSurface == NULL) {
     *       SDL_Log("Error: primary surface is NULL: %s", SDL_GetError());
     *       SDL_Quit();
     *       exit(1);
     *   }
     *   return 1;
     */

    while (retries < SURFACE_WAIT_MAX_RETRIES) {
        /*
         * Query surface readiness via vtable dispatch.
         * vtable method at offset 0x44 in the vtable (index 0x44/4 = 17).
         *
         * WIN32: typedef int (*QueryFn)(void*);
         *        QueryFn query = ((QueryFn**)mgr->pGameSurface)[0][0x44/4];
         *        surfaceStatus = query(mgr->pGameSurface);
         *        if (surfaceStatus != 0) return surfaceStatus; // ready
         *
         * LINUX: SDL surfaces are always ready; this query is unnecessary.
         *        Return 1 immediately after a NULL-check.
         */
        /* WIN32: vtable query goes here */
        /* surfaceStatus = (*(int(**)(void*))((*(int**)mgr->pGameSurface)[0x44/4]))(mgr->pGameSurface); */

        if (surfaceStatus != 0) {
            /* Surface is ready — return the status value */
            return surfaceStatus;
        }

        /*
         * Surface not yet ready — wait and retry.
         *
         * WIN32: Sleep(SURFACE_WAIT_INTERVAL_MS)
         *   Suspends the current thread for approximately 10 ms.
         *   The actual sleep may be longer due to OS scheduler granularity.
         *
         * LINUX: SDL_Delay(SURFACE_WAIT_INTERVAL_MS)
         *   SDL_Delay suspends for at least the specified number of ms.
         *   Same caveats about scheduler granularity apply.
         */
        /* WIN32: Sleep(SURFACE_WAIT_INTERVAL_MS); */
        /* LINUX: SDL_Delay(SURFACE_WAIT_INTERVAL_MS); */

        retries += 1;
    }

    /*
     * Timeout: surface never became ready within 10 seconds.
     * This is a fatal error — the game cannot render without the surface.
     *
     * WIN32: FUN_00463600() — error handler / assert / log
     *        ExitProcess(1) — terminate the process unconditionally
     *
     * LINUX: SDL_Log("Fatal: primary surface never became ready");
     *        SDL_Quit();   -- release all SDL resources cleanly before exit
     *        exit(1);      -- replaces ExitProcess(1)
     */
    FUN_00463600(); /* WIN32: error logger / assert */
    /* WIN32: ExitProcess(1); */
    /* LINUX: SDL_Quit(); exit(1); */

    return 0; /* unreachable; silences compiler warning */
}

/* =========================================================================
 * RenderCursor  (0x00414c20)
 * =========================================================================
 *
 * Main per-frame cursor compositing and blit function.
 *
 * This function implements the two-stage blit pipeline that renders the game's
 * software cursor each frame:
 *
 *   Stage 1 (cursor frame -> 256x256 staging surface):
 *     Source: the horizontal .ani frame strip in the cursor animation resource.
 *     Blit flags: DDBLT_WAIT | DDBLT_KEYSRC (0x1008000) — color-key transparency.
 *     The source rect selects the correct frame column (frameWidth * frameIndex).
 *
 *   Stage 2 (256x256 staging surface -> primary DirectDraw surface):
 *     Blit flags: DDBLT_WAIT (0x1000000) — no transparency, opaque composite.
 *     The destination rect is in screen coordinates (after ClientToScreen).
 *
 * Hot-path decision tree:
 *
 *   if (beginFrame)
 *     call 'begin frame' vtable method
 *
 *   if (clearOnly OR cursorStateId == 0 OR deactivated == 1):
 *     -> clear path: blit a blank rect over the old cursor position
 *   else:
 *     -> normal path: composite and blit the current cursor frame
 *
 * Normal path steps:
 *   1. WIN32: GetCursorPos(&screenPt) — screen (global) mouse coordinates
 *      LINUX: SDL_GetGlobalMouseState(&screenPt.x, &screenPt.y)
 *   2. Subtract hotspot (CursorAnimData+0x32, +0x34) from screen position
 *      to get the cursor sprite top-left corner.
 *   3. Clip the sprite rect against viewport bounds (this+0x20, this+0x24).
 *   4. Store the clipped destination rect in this+0x68..0x74.
 *   5. Select animation frame column from the sprite strip:
 *        if frameCount >= 2 (multi-frame):
 *          srcX = frameWidth * currentAnimFrame  (wraps at frameCount)
 *        else:
 *          srcX = 0
 *   6. CopyRect + OffsetRect to compute viewport-relative source coordinates
 *      (subtracting scroll offsets this+0x18 / this+0x1c).
 *   7. Blit cursor frame from the animation surface into the 256x256 staging
 *      surface using DirectDraw Blt with flags DDBLT_SRC_FLAGS (0x1008000).
 *   8. WIN32: ClientToScreen(mgr->hwnd, &windowOriginPt)
 *      LINUX: SDL_GetWindowPosition(window, &wx, &wy); pt.x+=wx; pt.y+=wy;
 *   9. Blit the 256x256 staging surface onto the primary DirectDraw surface
 *      using flags DDBLT_DST_FLAGS (0x1000000).
 *  10. On blit failure, log with OutputDebugStringA.
 *      LINUX: SDL_Log / fprintf(stderr)
 *
 * Animation frame selection:
 *   Cursor sprites are horizontal strips: frame N starts at pixel column
 *   (frameWidth * N).  The frame counter at this+0x48 wraps when it reaches
 *   CursorAnimData+0x160 (frameCount).  This is pure C arithmetic and is
 *   fully portable to Linux without change.
 *
 * Blit flag details:
 *   0x1008000 = DDBLT_WAIT(0x1000000) | DDBLT_KEYSRC(0x8000)
 *     Source blit: color-key transparency (the background color of the .ani
 *     frame is treated as transparent when blitting into the staging surface).
 *   0x1000000 = DDBLT_WAIT only
 *     Destination blit: opaque — the entire 256x256 staging surface is copied
 *     to the screen, composited by the prior color-key blit.
 *
 * WIN32: GetCursorPos, ClientToScreen, CopyRect, OffsetRect, SetRect,
 *        IDirectDrawSurface4::Blt (vtable +0x14) with flags 0x1008000 / 0x1000000,
 *        OutputDebugStringA
 * LINUX: SDL_GetGlobalMouseState(&x, &y)     — replaces GetCursorPos
 *        SDL_GetWindowPosition(win, &wx, &wy) — replaces ClientToScreen
 *        SDL_SetColorKey(srcSurf, SDL_TRUE, key) then SDL_BlitSurface
 *                                            — replaces Blt with 0x1008000
 *        SDL_BlitSurface                     — replaces Blt with 0x1000000
 *        SDL_Rect arithmetic (inline)        — replaces CopyRect, OffsetRect, SetRect
 *        SDL_Log / fprintf(stderr, ...)      — replaces OutputDebugStringA
 *
 * @param mgr         CursorManager instance
 * @param beginFrame  if non-zero, call 'begin frame' vtable method first
 * @param clearOnly   if non-zero, perform clear-only path (erase last pos)
 */
void RenderCursor(CursorManager *mgr, int beginFrame, int clearOnly)
{
    POINT    screenPt;        /* current screen-space mouse position */
    POINT    spritePt;        /* cursor sprite top-left (after hotspot subtraction) */
    RECT     srcRect;         /* source rect in the .ani frame strip */
    RECT     destRect;        /* destination rect on the screen */
    RECT     viewportRelRect; /* viewport-relative source rect (after scroll offset) */
    POINT    windowOriginPt;  /* window origin in screen coordinates */
    int      srcFrameX;       /* X pixel offset of the selected animation frame */
    int      blitResult;      /* DirectDraw Blt return value (0 = success) */
    char     dbgBuf[128];     /* buffer for OutputDebugStringA error message */
    CursorAnimData *anim;     /* current animation data */

    /*
     * Step: optional 'begin frame' notification.
     * If beginFrame is non-zero, call a vtable method to signal the start
     * of a new rendered frame.  This allows the rendering subsystem to
     * prepare the primary surface for writing.
     *
     * WIN32: (*((void(**)(void*))(*((int*)mgr->pGameSurface) + beginFrameOffset)))(mgr->pGameSurface);
     * LINUX: SDL equivalent (e.g. SDL_RenderClear or surface lock) goes here.
     */
    if (beginFrame) {
        /* WIN32: vtable call for 'begin frame' on mgr->pGameSurface */
        /* LINUX: SDL_RenderClear(renderer); or SDL_LockSurface(surface); */
    }

    /*
     * Determine whether to take the normal compositing path or the clear-only
     * path.  The clear-only path erases the cursor's previous screen position
     * without drawing a new cursor.
     *
     * Conditions for the normal path (all must be true):
     *   - clearOnly == 0
     *   - cursorStateId != 0 (a cursor animation is active)
     *   - deactivated == 0  (cursor capture is active; OS cursor is hidden)
     */
    if (clearOnly || mgr->cursorStateId == 0 || mgr->deactivated != 0) {
        /*
         * Clear-only path: blit a blank rect to erase the last cursor position.
         * Uses the last known destination rect (this+0x68..0x74).
         *
         * WIN32: g_PrimaryDDrawSurface Blt (vtable+0x14) with flags DDBLT_WAIT
         * LINUX: SDL_FillRect(primarySurface, &lastDestRect, clearColor)
         *        or SDL_BlitSurface(background, &lastDestRect, primary, &lastDestRect)
         */
        /* [clear blit of previous cursor rect goes here] */
        return;
    }

    /*
     * Normal compositing path.
     */

    anim = mgr->pCurrentAnim; /* CursorAnimData* at this+0x44 */

    /*
     * Step 1: Read the current screen-space mouse position.
     *
     * WIN32: GetCursorPos(&screenPt)
     *   Returns global screen coordinates regardless of window position.
     *   Stored in a POINT (two LONGs: x, y) on the stack at tStack_60.
     *
     * LINUX: SDL_GetGlobalMouseState(&screenPt.x, &screenPt.y)
     *   Returns screen coordinates in the same global coordinate space as
     *   GetCursorPos.  No ClientToScreen offset is needed at this step
     *   because we're already in screen space.
     *
     *   Alternative: SDL_GetMouseState(&screenPt.x, &screenPt.y)
     *   Returns window-relative coordinates.  If using this form, skip the
     *   ClientToScreen step in step 8 (window origin is already applied).
     */
    /* WIN32: GetCursorPos(&screenPt); */
    /* LINUX: SDL_GetGlobalMouseState(&screenPt.x, &screenPt.y); */

    /*
     * Step 2: Subtract the hotspot offset to get the sprite top-left corner.
     * The hotspot is the logical "tip" of the cursor within the sprite image.
     * CursorAnimData+0x32 = hotspotX, CursorAnimData+0x34 = hotspotY.
     * Subtracting places the sprite so the hotspot aligns with the mouse pos.
     *
     * This arithmetic is coordinate-system-neutral and ports unchanged.
     */
    if (anim != NULL) {
        spritePt.x = screenPt.x - (LONG)anim->hotspotX; /* CursorAnimData+0x32 */
        spritePt.y = screenPt.y - (LONG)anim->hotspotY; /* CursorAnimData+0x34 */
    } else {
        spritePt.x = screenPt.x;
        spritePt.y = screenPt.y;
    }

    /*
     * Step 3: Clip the sprite rect against the viewport bounds.
     * Viewport right boundary: this+0x20 (viewportRight)
     * Viewport bottom boundary: this+0x24 (viewportBottom)
     *
     * If the sprite's top-left is beyond the viewport edges, the cursor
     * is off-screen and should not be drawn (or should be clipped to the edge).
     */
    /* [clipping logic against viewportRight / viewportBottom goes here] */

    /*
     * Step 4: Store the clipped destination rect in the CursorManager.
     * Fields at this+0x68..0x74 record where the cursor was last drawn.
     * These are used on the next frame for the clear-only path to erase
     * the previous cursor position.
     */
    mgr->destLeft   = spritePt.x;  /* this+0x68 */
    mgr->destTop    = spritePt.y;  /* this+0x6c */
    /* destRight and destBottom set after frame dimensions are known */

    /*
     * Step 5: Select the animation frame column from the horizontal sprite strip.
     * The strip layout: frame 0 at pixel column 0, frame 1 at column frameWidth,
     * frame N at column (frameWidth * N).
     *
     * If frameCount >= 2 (multi-frame animation), multiply the frame width by
     * the current frame index.  The frame index (this+0x48) is advanced by
     * the caller each tick and wraps at frameCount (CursorAnimData+0x160).
     *
     * If frameCount == 1 (static cursor), srcFrameX = 0.
     *
     * This logic is fully portable to Linux; feed the result into an SDL_Rect.
     */
    srcFrameX = 0;
    if (anim != NULL && anim->frameCount >= 2) {
        srcFrameX = (int)anim->frameWidth * mgr->currentAnimFrame; /* this+0x48 */

        /* Advance frame counter; wrap at frameCount */
        mgr->currentAnimFrame += 1;
        if (mgr->currentAnimFrame >= (int)anim->frameCount) {
            mgr->currentAnimFrame = 0;
        }
    }

    /*
     * Step 6: CopyRect + OffsetRect to compute viewport-relative source coordinates.
     * The source rect in the animation strip uses pixel coordinates relative to
     * the strip image itself.  The scroll offsets (this+0x18 / this+0x1c) are
     * subtracted so the rect aligns with the visible viewport area.
     *
     * WIN32: CopyRect(&viewportRelRect, &srcRect);
     *        OffsetRect(&viewportRelRect, -mgr->scrollX, -mgr->scrollY);
     * LINUX: viewportRelRect = srcRect; (struct assignment)
     *        viewportRelRect.left  -= mgr->scrollX;
     *        viewportRelRect.right -= mgr->scrollX;
     *        viewportRelRect.top   -= mgr->scrollY;
     *        viewportRelRect.bottom-= mgr->scrollY;
     */
    if (anim != NULL) {
        SetRect(&srcRect,
                srcFrameX,
                0,
                srcFrameX + (int)anim->frameWidth  - 1,
                (int)anim->frameHeight - 1);
    }
    CopyRect(&viewportRelRect, &srcRect);
    OffsetRect(&viewportRelRect, -mgr->scrollX, -mgr->scrollY);

    /*
     * Step 7: Blit cursor frame from the animation surface into the 256x256
     * staging surface.
     *
     * WIN32: typedef HRESULT (*BltFn)(void*, LPRECT, void*, LPRECT, DWORD, void*);
     *        BltFn blt = ((BltFn**)g_CursorSurface)[0][0x14/4];
     *        blitResult = blt(g_CursorSurface, &destRect, animSurf, &srcRect,
     *                         DDBLT_SRC_FLAGS, NULL);
     *        Flags 0x1008000 = DDBLT_WAIT | DDBLT_KEYSRC:
     *          DDBLT_KEYSRC enables color-key transparency so the .ani
     *          background color is treated as transparent in the composite.
     *          DDBLT_WAIT means block until the blit hardware is ready.
     *
     * LINUX (two-step SDL replacement):
     *   SDL_Surface *animSurf = ... ; // the .ani frame strip surface
     *   SDL_SetColorKey(animSurf, SDL_TRUE, colorKey); // enable color-key
     *   SDL_Rect sdlSrcRect = { srcFrameX, 0, anim->frameWidth, anim->frameHeight };
     *   SDL_Rect sdlDstRect = { 0, 0, anim->frameWidth, anim->frameHeight };
     *   blitResult = SDL_BlitSurface(animSurf, &sdlSrcRect,
     *                                (SDL_Surface*)g_CursorSurface, &sdlDstRect);
     */
    blitResult = 0; /* placeholder; actual blit call goes here */

    if (blitResult != 0) {
        /*
         * Blit failure: log the error.
         * WIN32: OutputDebugStringA("RenderCursor: stage-1 blit failed\n");
         * LINUX: SDL_Log("RenderCursor: stage-1 blit failed: %s", SDL_GetError());
         *        fprintf(stderr, "RenderCursor: stage-1 blit failed\n");
         */
        snprintf(dbgBuf, sizeof(dbgBuf),
                 "RenderCursor: stage-1 blit failed (result=%d)\n", blitResult);
        /* WIN32: OutputDebugStringA(dbgBuf); */
        /* LINUX: SDL_Log("%s", dbgBuf); */
    }

    /*
     * Step 8: Convert the game window origin from client to screen coordinates.
     * This gives the screen-space position of the window's top-left corner,
     * which is needed to position the 256x256 staging surface correctly on
     * the primary surface.
     *
     * WIN32: POINT winOrigin = {0, 0};
     *        ClientToScreen((HWND)mgr->hwnd, &winOrigin);
     *        The result is the window origin in global screen coordinates.
     *
     * LINUX: int wx, wy;
     *        SDL_GetWindowPosition((SDL_Window*)mgr->hwnd, &wx, &wy);
     *        windowOriginPt.x = wx; windowOriginPt.y = wy;
     *        Add wx, wy to the destination rect manually.
     */
    windowOriginPt.x = 0;
    windowOriginPt.y = 0;
    /* WIN32: ClientToScreen((HWND)mgr->hwnd, &windowOriginPt); */
    /* LINUX: SDL_GetWindowPosition((SDL_Window*)mgr->hwnd,
     *                               &windowOriginPt.x, &windowOriginPt.y); */

    /*
     * Build the screen-space destination rect for the final blit.
     * Uses SetRect to construct the rect from the window origin and the
     * cursor surface dimensions.
     *
     * WIN32: SetRect(&destRect,
     *               windowOriginPt.x,
     *               windowOriginPt.y,
     *               windowOriginPt.x + CURSOR_SURFACE_DIM,
     *               windowOriginPt.y + CURSOR_SURFACE_DIM);
     * LINUX: SDL_Rect sdlDestRect = {
     *            windowOriginPt.x, windowOriginPt.y,
     *            CURSOR_SURFACE_DIM, CURSOR_SURFACE_DIM };
     */
    SetRect(&destRect,
            windowOriginPt.x,
            windowOriginPt.y,
            windowOriginPt.x + CURSOR_SURFACE_DIM,
            windowOriginPt.y + CURSOR_SURFACE_DIM);

    /* Update cached dest rect fields */
    mgr->destRight  = destRect.right;  /* this+0x70 */
    mgr->destBottom = destRect.bottom; /* this+0x74 */

    /*
     * Step 9: Blit the 256x256 staging surface onto the primary DirectDraw
     * surface (the screen).
     *
     * WIN32: typedef HRESULT (*BltFn)(void*, LPRECT, void*, LPRECT, DWORD, void*);
     *        BltFn blt = ((BltFn**)g_PrimaryDDrawSurface)[0][0x14/4];
     *        blitResult = blt(g_PrimaryDDrawSurface, &destRect,
     *                         g_CursorSurface, NULL, DDBLT_DST_FLAGS, NULL);
     *        Flags 0x1000000 = DDBLT_WAIT only (opaque blit, no color key).
     *
     * LINUX: SDL_Rect sdlSrcRect = {0, 0, CURSOR_SURFACE_DIM, CURSOR_SURFACE_DIM};
     *        SDL_Rect sdlDstRect = {windowOriginPt.x, windowOriginPt.y,
     *                               CURSOR_SURFACE_DIM, CURSOR_SURFACE_DIM};
     *        blitResult = SDL_BlitSurface(
     *            (SDL_Surface*)g_CursorSurface, &sdlSrcRect,
     *            (SDL_Surface*)g_PrimaryDDrawSurface, &sdlDstRect);
     */
    blitResult = 0; /* placeholder; actual blit call goes here */

    if (blitResult != 0) {
        /*
         * Step 10: Log final blit failure.
         *
         * WIN32: OutputDebugStringA("RenderCursor: stage-2 blit to primary failed\n")
         * LINUX: SDL_Log("RenderCursor: stage-2 blit failed: %s", SDL_GetError());
         *        fprintf(stderr, "RenderCursor: stage-2 blit to primary failed\n");
         */
        snprintf(dbgBuf, sizeof(dbgBuf),
                 "RenderCursor: stage-2 blit to primary failed (result=%d)\n",
                 blitResult);
        /* WIN32: OutputDebugStringA(dbgBuf); */
        /* LINUX: SDL_Log("%s", dbgBuf); */
    }
}

/* =========================================================================
 * Additional Global Variable Definitions
 * ========================================================================= */

int   g_GameMode     = 0;   /* 0x004851F4 — current game mode (GAME_MODE_*) */
int   g_BulldozeFlag = 0;   /* 0x004AA648 — bulldoze/delete tool active     */
int   g_FreePlaceFlag= 0;   /* 0x004A9F80 — object being freely dragged      */
int   g_RegionOverlay= 0;   /* 0x004A9F78 — map-region overlay active        */
int   g_ViewPanX     = 0;   /* 0x004AAD24 — viewport pan X offset (pixels)  */
int   g_ViewPanY     = 0;   /* 0x004AAD28 — viewport pan Y offset (pixels)  */
int   g_GridTileW    = 0;   /* 0x004AAD46 — world grid width in tiles        */
int   g_GridTileH    = 0;   /* 0x004AAD48 — world grid height in tiles       */
int   g_SubMode      = 0;   /* 0x00485234 — sub-mode within current game mode*/
int   g_TickBase     = 0;   /* 0x004A99B4 — base tick counter for anim delay */

/* =========================================================================
 * CursorFrameSet_AdvanceFrame  (helper — not a named EXE symbol)
 * =========================================================================
 *
 * Implements the animation frame-advance step shared by every cursor
 * renderer tick: CursorManager (RenderCursor), CursorWindow_Tick, and
 * the CursorRenderer dirty-repaint paths.
 *
 * The hot-path observed in the decompile uses only the flat frame_count
 * and does NOT index into individual CursorFrameSet entries per tick.
 * Frame-set transitions happen at state-change time (SetCursorState /
 * Cursor_SetType), not during the advance.
 *
 * HOT PATH (single active set, no per-set timing — most cursor states):
 *
 *   if (anim->frameCount > 1) {
 *       *frame_index = (*frame_index + 1) % anim->frameCount;
 *   }
 *   return (*frame_index) * anim->frameWidth;   // src_x column offset
 *
 * FULL FRAME-SET AWARE PATH (multi-set cursors with per-set duration):
 *
 *   CursorFrameSet *fs = &anim->frame_sets[active_set_index];
 *   // Check if frame is outside this set's range (e.g. after set change)
 *   if (*frame_index < fs->start_frame || *frame_index > fs->end_frame) {
 *       *frame_index = fs->start_frame;
 *       *tick_counter = 0;
 *   }
 *   // Advance when duration ticks have elapsed
 *   (*tick_counter)++;
 *   if (*tick_counter >= fs->duration) {
 *       *tick_counter = 0;
 *       (*frame_index)++;
 *       if (*frame_index > fs->end_frame) {
 *           *frame_index = fs->loop ? fs->start_frame : fs->end_frame;
 *       }
 *   }
 *   return (*frame_index) * anim->frameWidth;
 *
 * @param anim         pointer to the active CursorAnimData sprite resource
 * @param frame_index  pointer to the renderer's current frame index (updated)
 * @return             src_x — horizontal pixel offset into the sprite sheet
 *                     for the current frame; pass as the left edge of the
 *                     source rectangle to BltFast / SDL_RenderCopy
 *
 * LINUX example:
 *   int src_x = CursorFrameSet_AdvanceFrame(anim, &renderer->anim_frame);
 *   SDL_Rect src = { src_x, 0, anim->frameWidth, anim->frameHeight };
 *   SDL_Rect dst = { cursor_x - anim->hotspotX,
 *                    cursor_y - anim->hotspotY,
 *                    anim->frameWidth, anim->frameHeight };
 *   SDL_RenderCopy(sdl_renderer, cursor_texture, &src, &dst);
 */
int CursorFrameSet_AdvanceFrame(const CursorAnimData *anim, int *frame_index)
{
    if (anim == NULL || frame_index == NULL)
        return 0;

    /*
     * Hot path: flat modulo wrap across all frames.
     * Mirrors the decompile from CursorWindow_Tick (0x00426EB0) and
     * Cursor_Draw (0x00414FB0):
     *
     *   if (sprite->frame_count > 1)
     *       renderer->frame_index = (renderer->frame_index + 1) % sprite->frame_count;
     *   src_x = renderer->frame_index * sprite->frame_width;
     */
    if (anim->frameCount > 1) {
        *frame_index = (*frame_index + 1) % (int)anim->frameCount;
    }
    /* For frameCount == 1 (static cursor): frame_index stays at 0; src_x = 0 */

    return (*frame_index) * (int)anim->frameWidth;
}

/* =========================================================================
 * InputCursor_Tick  (0x00410840)
 * =========================================================================
 *
 * Main per-frame input processor.  Called from the game loop when the dirty
 * flag at ic+0x24 is set.  Flushes all pending mouse events in priority order
 * then delegates to InputCursor_UpdateCursorState to select the correct sprite.
 *
 * Event dispatch order (each flag is cleared after the corresponding handler):
 *   1. FUN_00405C40          — pull latest raw mouse position from OS
 *   2. warpPending (ic+0x8E) — deferred ClientToScreen warp
 *   3. leftClickFlag         — button-down world action
 *   4. rightClickFlag        — button-down context action / redirect
 *   5. dragLocked flag       — drag-drop release
 *   6. mouseMovePacked0/1    — queued WM_MOUSEMOVE packed coords
 *
 * After all events: call InputCursor_UpdateCursorState.
 * If gameCursorEnabled != 0x01 at any point: force CURSOR_RES_IDLE.
 *
 * WIN32: packed LPARAM coords lo16=X, hi16=Y stored in mouseMovePacked fields
 * LINUX: SDL_MOUSEMOTION.x / .y fill these fields in the SDL event handler
 */
void InputCursor_Tick(InputCursor *ic)
{
    if (ic == NULL) return;

    /* Guard: only process if dirty flag is set */
    if (!ic->dirtyFlag) return;

    /* 1. Pull latest mouse position from OS */
    /* WIN32: FUN_00405C40(ic) — calls GetCursorPos internally */
    /* LINUX: SDL_GetMouseState(&px, &py) and pack into warpPackedPos */

    /* 2. Deferred mouse warp */
    if (ic->warpPending) {
        ic->warpPending = 0;
        InputCursor_ProcessWarpedPosition(ic);
    }

    /* 3. Left-click event */
    if (ic->leftClickFlag) {
        ic->leftClickFlag = 0;
        InputCursor_LeftButtonDown(ic);
    }

    /* 4. Right-click event */
    if (ic->rightClickFlag) {
        ic->rightClickFlag = 0;
        InputCursor_RightButtonDown(ic);
    }

    /* 5. Drag-drop release */
    if (ic->dragLocked && ic->draggedObj) {
        InputCursor_DragDrop(ic);
        /* flag cleared inside DragDrop */
    }

    /* 6. Queued WM_MOUSEMOVE (two slots at +0xC4 and +0xD4) */
    if (ic->mouseMovePacked0) {
        /* WIN32: unpack lo16=X, hi16=Y; convert via Mouse_ScreenToIso */
        /* LINUX: SDL_MOUSEMOTION already provides game-space coords */
        ic->mouseMovePacked0 = 0;
    }
    if (ic->mouseMovePacked1) {
        ic->mouseMovePacked1 = 0;
    }

    /* Select cursor sprite for this frame */
    InputCursor_UpdateCursorState(ic);

    /* Fallback: if DDraw cursor is not enabled, force default sprite */
    if (ic->gameCursorEnabled != 0x01) {
        InputCursor_SetCursorSprite(ic, CURSOR_RES_IDLE);
    }

    ic->dirtyFlag = 0;
}

/* =========================================================================
 * InputCursor_UpdateCursorState  (0x00411760)
 * =========================================================================
 *
 * Top-level cursor sprite selector.  Dispatches by g_GameMode each frame.
 *
 * Mode dispatch:
 *   GAME_MODE_LOADING  (1) -> return immediately (no cursor change)
 *   GAME_MODE_PLACE    (3) -> InputCursor_PlaceModeCursor
 *   GAME_MODE_BUILD    (4) -> InputCursor_BuildModeCursor;
 *                             then InputCursor_ProcessPlacement
 *   all other modes        -> InputCursor_SetCursorSprite(CURSOR_RES_IDLE)
 *
 * Post-dispatch: if gameCursorEnabled != 0x01, force CURSOR_RES_IDLE.
 */
void InputCursor_UpdateCursorState(InputCursor *ic)
{
    if (ic == NULL) return;

    switch (g_GameMode) {
    case GAME_MODE_LOADING:
        /* No cursor change during loading/splash screens */
        return;

    case GAME_MODE_PLACE:
        InputCursor_PlaceModeCursor(ic);
        break;

    case GAME_MODE_BUILD:
        InputCursor_BuildModeCursor(ic);
        InputCursor_ProcessPlacement(ic);
        break;

    default:
        /* All other modes use the default cursor */
        InputCursor_SetCursorSprite(ic, CURSOR_RES_IDLE);
        break;
    }

    /* If DDraw sprite cursor is not active, force idle regardless of mode */
    if (ic->gameCursorEnabled != 0x01) {
        InputCursor_SetCursorSprite(ic, CURSOR_RES_IDLE);
    }
}

/* =========================================================================
 * InputCursor_BuildModeCursor  (0x004117B0)
 * =========================================================================
 *
 * Cursor-sprite selection for build/scroll mode.  Called every frame from
 * InputCursor_UpdateCursorState when g_GameMode == GAME_MODE_BUILD.
 *
 * Priority (first matching condition wins):
 *   1. Bulldoze tool active         -> CURSOR_RES_PLACE (grab cursor)
 *   2. Cursor outside world map     -> check edge-scroll zones
 *   3. Hoverable zone               -> FUN_00449CE0 / FUN_00436A10
 *   4. Near left edge  (< SCROLL_EDGE_LRT_PX)    -> CURSOR_RES_SCROLL_LEFT
 *      Near right edge (> width - LRT)            -> CURSOR_RES_SCROLL_RIGHT
 *      Near top edge   (< SCROLL_EDGE_LRT_PX)    -> CURSOR_RES_SCROLL_UP
 *      Near bot edge   (> height - BOT_PX)        -> CURSOR_RES_SCROLL_DOWN
 *      Diagonal corners                           -> 0x0C42..0x0C48
 *   5. Fall back to ic->activeCursorId, or CURSOR_RES_IDLE
 *
 * The scroll cursor is only substituted when ic->activeCursorId is non-zero
 * (i.e. the player has an item selected in the toolbar).
 */
void InputCursor_BuildModeCursor(InputCursor *ic)
{
    if (ic == NULL) return;

    /* Priority 1: bulldoze overrides everything */
    if (g_BulldozeFlag) {
        InputCursor_SetCursorSprite(ic, CURSOR_RES_PLACE);
        return;
    }

    /* Priorities 2-4: world map + edge-scroll detection.
     * Original calls FUN_00449D80 (is-cursor-in-world-map?).
     * Edge-scroll substitution compares cursor pos to viewport edges.
     * Commented stubs indicate where those calls go in the real binary. */

    /* Priority 5: fall back to item cursor or default */
    if (ic->activeCursorId != 0) {
        /* Use the cursor ID stored by the toolbar/item selection */
        /* InputCursor_SetCursorSprite(ic, ic->activeCursorId); */
    } else {
        InputCursor_SetCursorSprite(ic, CURSOR_RES_IDLE);
    }
}

/* =========================================================================
 * InputCursor_PlaceModeCursor  (0x00411AE0)
 * =========================================================================
 *
 * Cursor-sprite selection for place/train mode.  Called every frame from
 * InputCursor_UpdateCursorState when g_GameMode == GAME_MODE_PLACE.
 *
 * Forces CURSOR_RES_PLACE (grab cursor) when:
 *   - draggedObj != NULL AND dragLocked != 0  (item locked on tile)
 *   - g_BulldozeFlag set
 *   - g_FreePlaceFlag set (item being freely dragged, not grid-snapped)
 *
 * Inside world map:
 *   - g_RegionOverlay active AND cursor in highlight zone -> CURSOR_RES_PLACE
 *   - In boundary area (FUN_00459D60)                    -> CURSOR_RES_IDLE
 *   - Panel area (FUN_00485328) hit                      -> CURSOR_RES_IDLE
 *   - draggedObj != NULL AND dragLocked == 0             -> CURSOR_RES_OPEN_HAND
 *     (carrying an unlocked item over valid tile)
 */
void InputCursor_PlaceModeCursor(InputCursor *ic)
{
    if (ic == NULL) return;

    /* Force grab cursor for locked drag, bulldoze, or free-place */
    if ((ic->draggedObj != NULL && ic->dragLocked != 0) ||
        g_BulldozeFlag ||
        g_FreePlaceFlag) {
        InputCursor_SetCursorSprite(ic, CURSOR_RES_PLACE);
        return;
    }

    /* Check world-map region (FUN_00449D80 / FUN_00436A10 in original).
     * The stubs below show where those hit-tests are inserted. */

    /* Region overlay highlight zone */
    if (g_RegionOverlay) {
        /* if (FUN_00436A10(cursor_x, cursor_y)) { */
        InputCursor_SetCursorSprite(ic, CURSOR_RES_PLACE);
        return;
        /* } */
    }

    /* Open-hand hover: carrying an item over a valid, unlocked tile */
    if (ic->draggedObj != NULL && ic->dragLocked == 0) {
        InputCursor_SetCursorSprite(ic, CURSOR_RES_OPEN_HAND);
        return;
    }

    /* Default */
    InputCursor_SetCursorSprite(ic, CURSOR_RES_IDLE);
}

/* =========================================================================
 * InputCursor_ProcessWarpedPosition  (0x00410A40)
 * =========================================================================
 *
 * Handles a deferred mouse-warp event.  warpPending is set by
 * InputCursor_SetSystemCursorMode when the busy cursor is activated and
 * the cursor position needs to be re-evaluated after SetCapture.
 *
 * Steps:
 *   1. Unpack warpPackedPos (lo16=X, hi16=Y) into client-space POINT
 *   2. ClientToScreen -> screen-space position
 *   3. WindowFromPoint -> verify cursor is still over our window
 *   4. Mouse_ScreenToIso -> tile-space cursorGameX/Y
 *   5. Rail-scroll axis clamp:
 *        activeCursorId == CURSOR_RES_RAIL_CLAMP_Y1 or _Y2 -> clamp Y
 *        activeCursorId == CURSOR_RES_RAIL_CLAMP_X1 or _X2 -> clamp X
 *   6. If draggedObj != NULL: call vtable to update item screen position
 *   7. Dispatch to InputCursor_SetSystemCursorMode for focus-loss handling
 *
 * WIN32: ClientToScreen, WindowFromPoint
 * LINUX: SDL_GetWindowPosition + SDL_GetMouseFocus
 */
void InputCursor_ProcessWarpedPosition(InputCursor *ic)
{
    if (ic == NULL) return;

    /* Unpack warpPackedPos: lo16=X, hi16=Y */
    int client_x = (int)(ic->warpPackedPos & 0xFFFF);
    int client_y = (int)((ic->warpPackedPos >> 16) & 0xFFFF);

    /* WIN32: POINT pt = { client_x, client_y };
     *        ClientToScreen((HWND)ic->hwnd, &pt);
     *        HWND hit = WindowFromPoint(pt);
     *        if (hit != (HWND)ic->hwnd) { handle focus loss; return; }
     * LINUX: SDL_GetWindowPosition + SDL_GetMouseFocus check */

    /* Convert to tile coordinates */
    int out[2] = {0, 0};
    Mouse_ScreenToIso(ic, client_x, client_y, out);

    /* Rail-scroll axis clamp */
    switch (ic->activeCursorId) {
    case CURSOR_RES_RAIL_CLAMP_Y1:
    case CURSOR_RES_RAIL_CLAMP_Y2:
        /* clamp Y to previous value — horizontal rail */
        out[1] = ic->cursorGameY;
        break;
    case CURSOR_RES_RAIL_CLAMP_X1:
    case CURSOR_RES_RAIL_CLAMP_X2:
        /* clamp X to previous value — vertical rail */
        out[0] = ic->cursorGameX;
        break;
    default:
        break;
    }

    ic->cursorGameX = out[0];
    ic->cursorGameY = out[1];

    /* Update dragged object screen position */
    if (ic->draggedObj != NULL) {
        /* WIN32/LINUX: call dragged_obj vtable+0xC(tile_x, tile_y) */
    }
}

/* =========================================================================
 * InputCursor_ProcessPlacement  (0x00410D20)
 * =========================================================================
 *
 * Left-button-release and area-placement handler for build mode.
 * Iterates ic->tileContainer children looking for a tile at cursorGameX/Y.
 *
 * Sub-mode 1 (single tile):
 *   FUN_00455670(tileContainer, cursorGameX, cursorGameY) -> tile ptr
 *   If found: entity->event = 0x400; call entity vtable+0x34 (add-to-queue)
 *
 * Sub-mode 2 (stamp brush):
 *   For each row r in [0, brushRows) and col c in [0, brushCols):
 *     FUN_00455620(tileContainer, cursorGameX+c, cursorGameY+r)
 *     If found and brush cell is non-zero: fire placement
 *
 * Snap-back on invalid target:
 *   Sets leftClickFlag = 1 and copies warpPackedPos (last valid pos)
 *   into clickPackedL so the item returns to its last valid position.
 */
void InputCursor_ProcessPlacement(InputCursor *ic)
{
    if (ic == NULL || ic->tileContainer == NULL) return;
    if (g_GameMode != GAME_MODE_BUILD) return;

    if (g_SubMode == 1) {
        /* Single-tile click via FUN_00455670 */
        /* void *tile = FUN_00455670(ic->tileContainer, ic->cursorGameX, ic->cursorGameY);
         * if (tile) { fire event 0x400 on tile entity } */
    } else if (g_SubMode == 2) {
        /* Stamp-brush area via FUN_00455620 for each brush cell */
        for (int r = 0; r < (int)ic->brushRows; r++) {
            for (int c = 0; c < (int)ic->brushCols; c++) {
                /* void *tile = FUN_00455620(ic->tileContainer,
                 *     ic->cursorGameX + c, ic->cursorGameY + r);
                 * if (tile && brush_cell[r][c] != 0) { fire placement } */
            }
        }
    }
}

/* =========================================================================
 * InputCursor_LeftButtonDown  (0x00411000)
 * =========================================================================
 *
 * Left-mouse-button-down event.  Unpacks clickPackedL into lClickGameX/Y
 * then performs hit-tests in priority order.
 *
 * Special case: CURSOR_RES_ROTATE (0x1402) cursor with a new default frame-set
 * fires vtable+0x1C on the active item to advance its frame animation.
 *
 * Hit-test order (first match wins):
 *   1. Mini-train-world overlay (FUN_00459D60)
 *   2. Main world map           (FUN_00449D00)
 *   3. Toolbar items            (FUN_00434C50)
 *   4. Train tile grid          (FUN_004556F0)
 *
 * On grid hit with draggedObj != NULL:
 *   - draggedObj->load_count++ (capped at DRAG_ITEMS_MAX = 7)
 *   - call draggedObj vtable+0x44 to attach to tile
 *   - fire event 0x386D
 *
 * WIN32/LINUX: no platform-specific calls in this function.
 */
void InputCursor_LeftButtonDown(InputCursor *ic)
{
    if (ic == NULL) return;

    /* Unpack left-click coords and convert to tile space */
    int screen_x = (int)(ic->clickPackedL & 0xFFFF);
    int screen_y = (int)((ic->clickPackedL >> 16) & 0xFFFF);
    int out[2]   = {0, 0};
    Mouse_ScreenToIso(ic, screen_x, screen_y, out);
    ic->lClickGameX = out[0];
    ic->lClickGameY = out[1];

    /* Rotate cursor: frame-advance the active item */
    if (ic->activeCursorId == CURSOR_RES_ROTATE) {
        /* if (active_item has new default frame-set)
         *     call active_item vtable+0x1C (frame advance) */
    }

    /* Hit-test 1: mini-train-world overlay */
    /* if (FUN_00459D60(ic->lClickGameX, ic->lClickGameY)) return; */

    /* Hit-test 2: main world map */
    /* if (FUN_00449D00(ic, ic->lClickGameX, ic->lClickGameY)) return; */

    /* Hit-test 3: toolbar */
    /* if (FUN_00434C50(ic, ic->lClickGameX, ic->lClickGameY)) return; */

    /* Hit-test 4: train tile grid */
    if (ic->draggedObj != NULL) {
        /* Grid hit confirmed: attach dragged object to tile.
         * - Clamp load counter at DRAG_ITEMS_MAX */
        int *load_cnt = (int *)((char *)ic->draggedObj + 0x88);
        if (*load_cnt < DRAG_ITEMS_MAX)
            (*load_cnt)++;
        /* - call draggedObj vtable+0x44 (attach to tile) */
        /* - fire event 0x386D */
    }
}

/* =========================================================================
 * InputCursor_RightButtonDown  (0x00411230)
 * =========================================================================
 *
 * Right-mouse-button-down event.  Unpacks clickPackedR into rClickGameX/Y.
 *
 * Cursor redirect:
 *   If activeCursorId matches the current item's ID AND
 *   the item's sprite has frameSetRedirect (+0x52E) != 0:
 *     mutate ic->activeCursorId to frameSetRedirect value.
 * Otherwise: standard hit-tests against toolbar, minimap, tile grid.
 *   On tile grid hit: fire event 0x502C or 0x5015.
 * On button-up: call FUN_004113A0 to clear draggedObj.
 *
 * WIN32/LINUX: no platform-specific calls in this function.
 */
void InputCursor_RightButtonDown(InputCursor *ic)
{
    if (ic == NULL) return;

    /* Unpack right-click coords */
    int screen_x = (int)(ic->clickPackedR & 0xFFFF);
    int screen_y = (int)((ic->clickPackedR >> 16) & 0xFFFF);
    int out[2]   = {0, 0};
    Mouse_ScreenToIso(ic, screen_x, screen_y, out);
    ic->rClickGameX = out[0];
    ic->rClickGameY = out[1];

    /* Cursor ID redirect via frameSetRedirect at sprite+0x52E */
    /* CursorAnimData *sprite = RESMGR_GetResource(g_ResourceManager, ic->activeCursorId);
     * if (sprite && sprite->frameSetRedirect != 0 &&
     *     ic->activeCursorId == current_item_id) {
     *     ic->activeCursorId = sprite->frameSetRedirect;
     *     return;
     * } */

    /* Standard hit-tests: toolbar, minimap, tile grid */
    /* FUN_00434C50 / FUN_0044E830 / FUN_00455D60 calls here */
    /* Events 0x502C or 0x5015 fired on tile grid hit */

    /* Clear draggedObj on button-up */
    /* FUN_004113A0(ic); */
}

/* =========================================================================
 * InputCursor_DragDrop  (0x00411580)
 * =========================================================================
 *
 * Drag-release / drop for the held train-locomotive.
 * draggedObj pointer is at ic+0xE8.
 *
 * Placement mode 0x07 (first train slot array):
 *   1. Search g_TrainSlotArray0 for a compatible slot at drop position.
 *   2. Fallback: iterate all slots for best available
 *      via vtable+0x1C (query) and vtable+0x48 (accept).
 *   3. On success: call vtable+0x0 to commit placement.
 *
 * Placement mode 0x08 (second train slot array):
 *   Same logic on the second slot array.
 *
 * After placement: clamp load counter at draggedObj+0x88 to [0, DRAG_ITEMS_MAX].
 * Clear dragLocked flag at ic+0xEC.
 *
 * WIN32/LINUX: no platform-specific calls in this function.
 */
void InputCursor_DragDrop(InputCursor *ic)
{
    if (ic == NULL || ic->draggedObj == NULL) return;

    /* Read placement mode byte from the dragged object's cursor info */
    /* uint8_t placement_mode = *(uint8_t*)((char*)draggedObj_cursor + 0x07); */

    /* Slot search (mode 0x07 — first slot array) */
    /* if (placement_mode == 0x07) {
     *   search g_TrainSlotArray0 via vtable+0x1C / vtable+0x48;
     *   if found: vtable+0x0(slot, draggedObj) to commit;
     * }
     * else if (placement_mode == 0x08) {
     *   search second slot array similarly;
     * } */

    /* Clamp load counter */
    int *load_cnt = (int *)((char *)ic->draggedObj + 0x88);
    if (*load_cnt < 0) *load_cnt = 0;
    if (*load_cnt > DRAG_ITEMS_MAX) *load_cnt = DRAG_ITEMS_MAX;

    /* Clear drag-locked flag */
    ic->dragLocked = 0;
}

/* =========================================================================
 * InputCursor_SetSystemCursorMode  (0x00411DC0)
 * =========================================================================
 *
 * Controls the OS system cursor and mouse capture for non-DDraw states.
 *
 * Deactivation (active == 0):
 *   ReleaseCapture();
 *   if (showSysCursor) ShowCursor loop to 1
 *
 * Activation without busy cursor (useBusyCursor == 0):
 *   ShowCursor loop to -1  (hide OS cursor)
 *
 * Activation with busy cursor (useBusyCursor != 0):
 *   Load "CURSORS\busy_ani.ani" from game dir via LoadCursorFromFileA
 *   (cached in ic->busyCursor after first load).
 *   SetCursor(ic->busyCursor);
 *   SetCapture((HWND)ic->hwnd);
 *   GetCursorPos + ScreenToClient -> pack into ic->warpPackedPos;
 *   Set ic->warpPending = 1.
 *
 * WIN32: ShowCursor, SetCapture, ReleaseCapture, LoadCursorFromFileA, SetCursor,
 *        GetCursorPos, ScreenToClient
 * LINUX: SDL_ShowCursor, SDL_CaptureMouse, SDL_GetMouseState; custom ANI loader
 */
void InputCursor_SetSystemCursorMode(InputCursor *ic,
                                     int active,
                                     int showSysCursor,
                                     int useBusyCursor)
{
    if (ic == NULL) return;

    if (active == 0) {
        /* Deactivation: release capture and optionally restore OS cursor */
        /* WIN32: ReleaseCapture(); */
        /* LINUX: SDL_CaptureMouse(SDL_FALSE); */

        if (showSysCursor) {
            /* WIN32: do { } while (ShowCursor(TRUE) < 1); */
            /* LINUX: SDL_ShowCursor(SDL_ENABLE); */
        }
    } else if (!useBusyCursor) {
        /* Activation: hide OS cursor (no busy animation) */
        /* WIN32: do { } while (ShowCursor(FALSE) >= 0); */
        /* LINUX: SDL_ShowCursor(SDL_DISABLE); */
    } else {
        /* Busy-cursor activation: load .ANI file and SetCursor */

        if (ic->busyCursor == NULL) {
            /* First call: load and cache the animated busy cursor.
             * Path: <game_data_dir>\CURSORS\busy_ani.ani
             * WIN32: ic->busyCursor = LoadCursorFromFileA(path);
             * LINUX: parse ANI format -> SDL_CreateColorCursor / SDL_SetCursor */
        }
        /* WIN32: SetCursor((HCURSOR)ic->busyCursor); */
        /* LINUX: SDL_SetCursor((SDL_Cursor*)ic->busyCursor); */

        /* Capture mouse to game window */
        /* WIN32: SetCapture((HWND)ic->hwnd); */
        /* LINUX: SDL_CaptureMouse(SDL_TRUE); */

        /* Read current cursor position and store packed for deferred warp */
        /* WIN32: POINT pt; GetCursorPos(&pt);
         *        ScreenToClient((HWND)ic->hwnd, &pt);
         *        ic->warpPackedPos = (uint32_t)pt.x | ((uint32_t)pt.y << 16);
         * LINUX: int x,y; SDL_GetMouseState(&x,&y);
         *        ic->warpPackedPos = (uint32_t)x | ((uint32_t)y << 16); */
        ic->warpPending = 1;
    }
}

/* =========================================================================
 * InputCursor_SetCursorSprite  (0x00411FB0)
 * =========================================================================
 *
 * Core cursor sprite switch.  No-ops if resource_id already matches the
 * currently displayed cursor ID (from *(ic->cursorRenderer + 4)).
 *
 * Steps:
 *   1. Look up resource_id in g_ResourceManager via FUN_00446EA0.
 *   2. Call ic vtable+0x18 (cursor-renderer method) with:
 *        (resource_id, hotspot_x, 0)
 *      to push the new sprite to the renderer sub-object.
 *   3. Compute hotspot:
 *        if sprite->cursorType == 5:
 *          hotspot from ic field at +0x9C/0xA0 minus sprite->hotspotX/Y
 *        else:
 *          Y adjusted by subtracting sprite->heightOffset (+0x16D)
 *   4. Store resource_id in ic->activeCursorId.
 */
void InputCursor_SetCursorSprite(InputCursor *ic, uint32_t resource_id)
{
    if (ic == NULL) return;

    /* Early-out: already displaying this sprite */
    /* uint32_t current_id = *(uint32_t*)((char*)ic->cursorRenderer + 4);
     * if (current_id == resource_id) return; */

    /* Look up the resource */
    /* CursorAnimData *sprite = (CursorAnimData*)FUN_00446EA0(g_ResourceManager, resource_id);
     * if (!sprite) return; */

    /* Push to cursor renderer via vtable+0x18 */
    /* ((void(*)(void*,uint32_t,int,int))ic->vtable[0x18/4])(ic, resource_id, hotspot_x, 0); */

    /* Store the newly active resource ID */
    ic->activeCursorId = resource_id;
}

/* =========================================================================
 * InputCursor_GameModeSet  (0x00408130)
 * =========================================================================
 *
 * Sets g_GameMode and performs mode-transition side-effects.
 *
 * Modes 5/6/7/9 (UI overlays): call SetSystemCursorMode(ic, 0, 0, 0)
 *   to hide the DDraw cursor before showing the relevant window.
 * Mode 2 (transition): hide DDraw cursor, show OS cursor, focus inventory.
 * Mode 3 (place): intermediate step via FUN_004086F0 (sets mode=2 first).
 * Mode 4 (build): clear draggedObj, reset toolbar, clear world-event queue.
 * Mode 10 (shutdown): drain message queue, post WM_QUIT / SDL_QUIT.
 *
 * WIN32: PostQuitMessage(0) for mode 10
 * LINUX: SDL_PushEvent({SDL_QUIT}) for mode 10
 */
void InputCursor_GameModeSet(int new_mode)
{
    g_GameMode = new_mode;

    switch (new_mode) {
    case GAME_MODE_LOADING:
        /* FUN_00408350(ic) — splash/loading setup */
        break;

    case GAME_MODE_TRANSITION:
        /* Hide DDraw cursor, show OS cursor, focus inventory window */
        /* InputCursor_SetSystemCursorMode(ic, 0, 1, 0); */
        break;

    case GAME_MODE_PLACE:
        /* Intermediate: sets mode=2 first via FUN_004086F0 */
        break;

    case GAME_MODE_BUILD:
        /* Clear dragged object, reset toolbar, clear world-event queue */
        break;

    case GAME_MODE_CREDITS:
    case GAME_MODE_OPTIONS:
    case GAME_MODE_HELP:
    case GAME_MODE_SETTINGS:
        /* Hide DDraw cursor before showing the UI overlay */
        /* InputCursor_SetSystemCursorMode(ic, 0, 0, 0); */
        break;

    case GAME_MODE_SHUTDOWN:
        /* Drain message queue then post quit */
        /* WIN32: { MSG m; while (PeekMessage(&m, NULL, 0, 0, PM_REMOVE)) {} PostQuitMessage(0); } */
        /* LINUX: { SDL_Event e; while (SDL_PollEvent(&e)) {}
         *          SDL_Event q = {0}; q.type = SDL_QUIT; SDL_PushEvent(&q); } */
        break;

    default:
        break;
    }
}

/* =========================================================================
 * Mouse_ScreenToIso  (0x00412060)
 * =========================================================================
 *
 * Converts screen pixel coordinates to isometric tile coordinates.
 *
 *   tile_x = clamp((screen_x + g_ViewPanX) / TILE_SIZE_PX, 0, g_GridTileW-1)
 *   tile_y = clamp((screen_y + g_ViewPanY) / TILE_SIZE_PX, 0, g_GridTileH-1)
 *
 * When the active cursor sprite's cursorType != 5 (not an isometric-drag
 * cursor), additional clamping is applied using the sprite's own grid
 * boundary fields:
 *   col max: sprite->frameWidth
 *   row min/max: sprite->heightOffset (+0x16D), sprite->brushCols (+0x169)
 *
 * Snaps to TILE_SIZE_PX grid via integer floor division.
 *
 * @param ic        InputCursor instance (for sprite lookup)
 * @param screen_x  screen pixel X (before viewport pan)
 * @param screen_y  screen pixel Y (before viewport pan)
 * @param out       out[0] = tile_x, out[1] = tile_y
 *
 * WIN32/LINUX: pure arithmetic; fully portable.
 */
void Mouse_ScreenToIso(const InputCursor *ic,
                       int screen_x, int screen_y,
                       int out[2])
{
    int world_x = screen_x + g_ViewPanX;
    int world_y = screen_y + g_ViewPanY;

    /* Clamp to world grid before dividing */
    int max_x = g_GridTileW * TILE_SIZE_PX;
    int max_y = g_GridTileH * TILE_SIZE_PX;
    if (world_x < 0) world_x = 0;
    if (world_y < 0) world_y = 0;
    if (world_x > max_x) world_x = max_x;
    if (world_y > max_y) world_y = max_y;

    /* Floor division to tile grid */
    out[0] = world_x / TILE_SIZE_PX;
    out[1] = world_y / TILE_SIZE_PX;

    /* Additional sprite-specific clamping when cursorType != 5.
     * The sprite's own boundary is smaller than the full world grid
     * (e.g. toolbar-area cursors are constrained to their panel). */
    (void)ic;  /* ic used for sprite lookup in original; elided in stub */
}

/*
 * Mouse_ProcessMove  (0x00410A40)
 *
 * Thin wrapper — in the original binary this is the same function as
 * InputCursor_ProcessWarpedPosition.  Reads warpPackedPos and delegates.
 *
 * WIN32: ClientToScreen, WindowFromPoint
 * LINUX: SDL_GetWindowPosition + SDL_GetMouseFocus
 */
void Mouse_ProcessMove(InputCursor *ic)
{
    InputCursor_ProcessWarpedPosition(ic);
}
