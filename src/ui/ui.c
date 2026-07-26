/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: UI (dialogs, main menu, settings, multiplayer lobby)
 * WIN32: CreateDialogParamA -> LINUX: SDL2 custom rendering
 *
 * Original binary: loco.exe (Win32, x86, MSVC-compiled)
 * Analysis basis: Ghidra decompilation of relevant address range
 *
 * This file documents two distinct UI subsystems:
 *   Section 1 (lines ~30-): UIPanel — building-picker panel, drag-and-drop
 *     cursor tracking, DirectDraw blit pipeline, panel lifecycle
 *   Section 2 (appended):   EditWindow / PanelA / PanelB / MainMenu —
 *     fullscreen dialog controller, name-entry, city selection, multiplayer
 *     lobby, screensaver, startup sequence
 *
 * Porting target: Linux + SDL2 (see PORTING NOTES in ui.h)
 *
 * Original addresses are noted per function as 0x00XXXXXX offsets into loco.exe.
 * All WIN32 API calls are annotated with replacement SDL2/POSIX equivalents.
 */

#include "ui.h"

#include <stdint.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Global variable declarations (original PE addresses noted)
 *
 * In the Win32 binary these are absolute data addresses in the .data section.
 * In the Linux port these become normal C globals or are folded into a
 * GameState struct as appropriate.
 *--------------------------------------------------------------------------*/

/*
 * 0x004fd3c4  DAT_004fd3c4
 * Global primary DirectDraw surface (IDirectDrawSurface**).
 * Destination for all final screen blits.  GetBltStatus is polled on this
 * surface by Panel_WaitBltSync.
 * LINUX: replaced by the global SDL_Renderer* or SDL_Texture* for the
 *        primary render target.
 */
#ifdef LOCO_LINUX
SDL_Renderer *g_primary_renderer = NULL;   /* replaces DAT_004fd3c4 */
SDL_Texture  *g_primary_texture  = NULL;
#else
void *DAT_004fd3c4 = NULL;   /* IDirectDrawSurface** */
#endif

/*
 * 0x004fd3c0  DAT_004fd3c0
 * Global secondary DirectDraw surface (back buffer / offscreen buffer).
 * Used by Panel_DragUpdate to restore background pixels under the old
 * cursor position before drawing at the new position.
 * LINUX: replaced by a saved SDL_Texture snapshot of the background region.
 */
#ifdef LOCO_LINUX
SDL_Texture *g_secondary_texture = NULL;   /* replaces DAT_004fd3c0 */
#else
void *DAT_004fd3c0 = NULL;   /* IDirectDrawSurface** */
#endif

/*
 * 0x004fd3e0  DAT_004fd3e0
 * Global pointer to the currently active/focused UIPanel object.
 * Set to `this` when a panel becomes active (modes 1-5).
 * Reset to &DAT_004aa5b8 (null-panel sentinel) when panel mode is 0 (hidden).
 * LINUX: plain C pointer; no Win32 equivalent needed.
 */
UIPanel *g_active_panel = NULL;   /* DAT_004fd3e0 */

/*
 * 0x004aa5b8  DAT_004aa5b8
 * Static null-panel / default-panel sentinel block.
 * g_active_panel points here when no panel is active (mode 0).
 * LINUX: static storage acts as sentinel; pointer comparison still works.
 */
static UIPanel g_null_panel_sentinel;   /* DAT_004aa5b8 */

/*
 * 0x00477cc8  PTR_FUN_00477cc8
 * UIPanel virtual dispatch table pointer.
 * Set as the first dword of every UIPanel object in the constructor.
 * Re-set at the start of the destructor body before virtual cleanup.
 * LINUX: replaced by a C++ vtable or a function-pointer struct.
 */
extern UIPanelVtable g_uipanel_vtable;   /* PTR_FUN_00477cc8 */

/*
 * 0x004855e8  DAT_004855e8
 * Resource registry / resource table.
 * Searched by UIPanel_LoadContent (FUN_00427580) via FUN_00446ea0 to look up
 * panel content elements by type-ID in the 0x2c00-0x2c0f range.
 * LINUX: pointer into the game's internal resource system; needs own porting.
 */
extern void *g_resource_registry;   /* DAT_004855e8 */

/*
 * 0x004aad0c  DAT_004aad0c
 * Game timer or frame counter.
 * Used in FUN_004277d0 to calculate elapsed time relative to panel+0x38 in
 * order to set the panel_visible_flag2 at panel+0xad.
 * LINUX: replace with SDL_GetTicks() or a monotonic clock value.
 */
int g_game_timer = 0;   /* DAT_004aad0c */

/*
 * 0x004aa5c8  DAT_004aa5c8
 * Visibility threshold constant.
 * Compared against the elapsed-time difference inside FUN_004277d0.
 * LINUX: plain constant; no Win32 dependency.
 */
int g_visibility_threshold = 0;   /* DAT_004aa5c8 */

/*
 * Internal render mutex handle.
 * Original: Win32 CRITICAL_SECTION or DirectDraw Lock/Unlock pair hidden
 *           inside FUN_0045b940 (lock) and its matching release call.
 * LINUX: SDL_mutex* protected by SDL_LockMutex / SDL_UnlockMutex.
 *        If rendering is strictly single-threaded, remove entirely.
 */
#ifdef LOCO_LINUX
static SDL_mutex *s_render_mutex = NULL;
#endif

/*---------------------------------------------------------------------------
 * Internal helper: acquire / release render mutex
 * Original: FUN_0045b940 called as paired lock/unlock around render ops.
 *--------------------------------------------------------------------------*/
static void RenderMutex_Lock(void)
{
#ifdef LOCO_LINUX
    /* LINUX: SDL_LockMutex(s_render_mutex) replaces EnterCriticalSection or
     *        DirectDraw Lock.  If rendering is single-threaded, this is a
     *        no-op. */
    if (s_render_mutex)
        SDL_LockMutex(s_render_mutex);
#else
    /* WIN32: FUN_0045b940() — wraps EnterCriticalSection or DDraw Lock */
#endif
}

static void RenderMutex_Unlock(void)
{
#ifdef LOCO_LINUX
    /* LINUX: SDL_UnlockMutex(s_render_mutex) replaces LeaveCriticalSection or
     *        DirectDraw Unlock. */
    if (s_render_mutex)
        SDL_UnlockMutex(s_render_mutex);
#else
    /* WIN32: matching release call to FUN_0045b940 */
#endif
}

/*---------------------------------------------------------------------------
 * Internal helper: low-level bitmap blitter
 * Original: FUN_0042b050 — blits a LOCOBITMAP region to an offscreen surface
 *           with computed source offsets (used by Panel_Render and
 *           Panel_DragUpdate).
 * LINUX: equivalent to SDL_BlitSurface / SDL_RenderCopy on the bitmap texture.
 *--------------------------------------------------------------------------*/
static void LocoBitmap_Blit(void *bitmap, int src_x, int src_y,
                             int dst_x, int dst_y, int w, int h)
{
#ifdef LOCO_LINUX
    /* LINUX: SDL_BlitSurface(src_surface, &src_rect, dst_surface, &dst_rect)
     *        or SDL_RenderCopy(renderer, src_texture, &src_rect, &dst_rect) */
    (void)bitmap; (void)src_x; (void)src_y;
    (void)dst_x; (void)dst_y; (void)w; (void)h;
    /* TODO: implement using SDL2 blit primitives */
#else
    /* WIN32: FUN_0042b050(bitmap, src_x, src_y, dst_x, dst_y, w, h) */
    (void)bitmap; (void)src_x; (void)src_y;
    (void)dst_x; (void)dst_y; (void)w; (void)h;
#endif
}

/*---------------------------------------------------------------------------
 * Internal helper: screen rect invalidation
 * Original: FUN_00401280 — invalidates a merged screen rectangle so Win32
 *           will repaint it.
 * LINUX: no-op in SDL2 (renderer handles invalidation automatically).
 *--------------------------------------------------------------------------*/
static void Screen_InvalidateRect(const LOCO_RECT *r)
{
#ifdef LOCO_LINUX
    /* LINUX: SDL2 does not need explicit rect invalidation.  The renderer
     *        redraws whatever SDL_RenderCopy has written on SDL_RenderPresent.
     *        This call can be removed or replaced with a no-op marker. */
    (void)r;
#else
    /* WIN32: FUN_00401280(&rect) — calls InvalidateRect on the panel HWND */
    (void)r;
#endif
}

/*---------------------------------------------------------------------------
 * Internal helper: fatal error log + process exit
 * Original: FUN_00463600 — writes to an error log then calls ExitProcess.
 * LINUX: write to stderr then call exit(1) after SDL_Quit().
 *--------------------------------------------------------------------------*/
static void FatalError_Log(const char *msg)
{
#ifdef LOCO_LINUX
    /* LINUX: fprintf(stderr, ...) then SDL_Quit(); exit(1); */
    (void)msg;
    /* TODO: wire up game-specific error logging here */
    SDL_Quit();
    exit(1);
#else
    /* WIN32: FUN_00463600(msg); ExitProcess(1); */
    (void)msg;
#endif
}

/*===========================================================================
 * Panel_WndProc
 * Original address: 0x00426900
 *
 * Window procedure / message handler for the panel HWND.
 *
 * Behaviour:
 *   Checks whether the incoming HWND matches the panel's stored HWND at
 *   this+0x08.  If it matches:
 *     1. Acquires the render mutex (FUN_0045b940).
 *     2. Calls Panel_DragUpdate (FUN_00426eb0) with flag=1 to repaint at
 *        the current cursor position.
 *     3. Releases the render mutex.
 *   Then falls through to DefWindowProcA for default Win32 handling.
 *
 *   Acts as the WM_PAINT or WM_TIMER callback that keeps the panel bitmap
 *   refreshed on screen.
 *
 * Note: `this` is a window-container object, not a raw HWND.  The two
 * FUN_0045b940 calls bracketing FUN_00426eb0 indicate a custom render mutex
 * (likely wrapping EnterCriticalSection/LeaveCriticalSection or DirectDraw
 * Lock/Unlock).  flag=1 to Panel_DragUpdate means a full forced repaint.
 *
 * WIN32: DefWindowProcA
 * LINUX: No WndProc in SDL2.  Replace with SDL_RenderPresent triggered from
 *        the SDL event loop on SDL_WINDOWEVENT_EXPOSED or from the main game
 *        loop render tick.  DefWindowProcA is simply dropped.
 *===========================================================================*/
#ifndef LOCO_LINUX
LRESULT Panel_WndProc(UIPanel *self, HWND hwnd, UINT msg,
                       WPARAM wparam, LPARAM lparam)
{
    /*
     * Check whether the incoming HWND matches the panel's stored HWND.
     * The HWND is stored at object offset +0x08.
     */
    if (hwnd == self->hwnd)   /* this+0x08 */
    {
        /* Acquire render mutex — WIN32: FUN_0045b940() */
        RenderMutex_Lock();

        /* Force a full repaint at the current cursor position.
         * flag=1 means unconditional repaint regardless of dirty-rect state. */
        Panel_DragUpdate(self, /*flag=*/1);   /* FUN_00426eb0 */

        /* Release render mutex — WIN32: matching FUN_0045b940 release */
        RenderMutex_Unlock();
    }

    /*
     * Fall through to default Win32 window procedure.
     * WIN32: DefWindowProcA(hwnd, msg, wparam, lparam)
     * LINUX: Dropped — SDL2 event loop handles all window events without a
     *        WndProc callback.
     */
    /* WIN32: return DefWindowProcA(hwnd, msg, wparam, lparam); */
    return 0;
}
#else /* LOCO_LINUX */
/*
 * LINUX SDL2 replacement for Panel_WndProc.
 *
 * Instead of a WndProc, the SDL event loop calls this function when an
 * SDL_WINDOWEVENT_EXPOSED event is received, or it is called directly from
 * the main game-loop render tick.
 *
 * SDL_RenderPresent(renderer) is the equivalent of the DefWindowProcA
 * WM_PAINT handling that causes the OS to composite the window.
 */
void Panel_OnExposed(UIPanel *self)
{
    if (!self) return;

    /* Acquire render mutex — LINUX: SDL_LockMutex */
    RenderMutex_Lock();

    /* Repaint at current cursor position — flag=1 forces full repaint */
    Panel_DragUpdate(self, /*flag=*/1);

    /* Release render mutex — LINUX: SDL_UnlockMutex */
    RenderMutex_Unlock();

    /* LINUX: SDL_RenderPresent(g_primary_renderer) presents the frame.
     *        Called here if this is the final compositing step, otherwise
     *        deferred to the main render tick. */
    SDL_RenderPresent(g_primary_renderer);
}
#endif /* LOCO_LINUX */

/*===========================================================================
 * Panel_Destroy
 * Original address: 0x00426a90
 *
 * Panel shutdown sequence:
 *   1. Clears the is_alive flag at object+0xab (prevents re-entrant destroy).
 *   2. Destroys the Win32 window via DestroyWindow on the HWND at object+0x08.
 *   3. Checks the sibling/parent count at object+0x0c.
 *      If it is zero (this is the last panel), posts WM_QUIT to end the
 *      Win32 message loop via PostQuitMessage(0).
 *
 * WIN32: DestroyWindow, PostQuitMessage
 * LINUX: SDL_DestroyWindow(panel->sdl_window) replaces DestroyWindow.
 *        SDL_PushEvent with type SDL_QUIT replaces PostQuitMessage(0).
 *        sibling_count at +0x0c maps to an SDL2 window-count tracker or
 *        application-level flag.
 *===========================================================================*/
void Panel_Destroy(UIPanel *self)
{
    if (!self) return;

    /*
     * Guard against re-entrant destroy.
     * is_alive is at object+0xab; clearing it prevents a second call from
     * doing any work.
     */
    self->is_alive = 0;   /* +0xab */

    /*
     * Destroy the OS window.
     * WIN32: DestroyWindow(self->hwnd)  — self->hwnd is at object+0x08.
     * LINUX: SDL_DestroyWindow(self->sdl_window)
     */
#ifndef LOCO_LINUX
    /* WIN32: DestroyWindow(self->hwnd); */
    (void)self;   /* silence unused-variable warning in stub */
#else
    /* LINUX: SDL_DestroyWindow replaces DestroyWindow */
    if (self->sdl_window)
    {
        SDL_DestroyWindow(self->sdl_window);
        self->sdl_window = NULL;
    }
#endif

    /*
     * If this is the last panel (sibling_count == 0 at object+0x0c), signal
     * the application to quit.
     *
     * WIN32: PostQuitMessage(0) posts WM_QUIT to the message loop.
     * LINUX: SDL_PushEvent with SDL_QUIT type terminates the SDL event loop.
     */
    if (self->sibling_count == 0)   /* +0x0c */
    {
#ifndef LOCO_LINUX
        /* WIN32: PostQuitMessage(0); */
#else
        /* LINUX: push an SDL_QUIT event so the main event loop exits cleanly */
        SDL_Event quit_event;
        quit_event.type = SDL_QUIT;
        SDL_PushEvent(&quit_event);
#endif
    }
}

/*===========================================================================
 * Panel_WaitBltSync
 * Original address: 0x00426b00
 *
 * Synchronisation wait for a pending DirectDraw blit to complete.
 *
 * Behaviour:
 *   1. Acquires the render lock.
 *   2. Polls IDirectDrawSurface::GetBltStatus (vtable offset 0x44) on the
 *      global primary surface at DAT_004fd3c4.
 *   3. Stores the result at object+0x4c (blit_sync_result).
 *   4. If the status is non-zero (blit still in progress):
 *      - Sleeps BLIT_SYNC_SLEEP_MS (10 ms).
 *      - Retries up to BLIT_SYNC_TIMEOUT_ITERS (1000) times.
 *      - On timeout calls FUN_00463600 (fatal error log) then ExitProcess(1).
 *   5. Returns the final sync result value.
 *
 * Constants:
 *   BLIT_SYNC_TIMEOUT_ITERS = 1000  (10-second hard timeout at 10ms/iter)
 *   BLIT_SYNC_SLEEP_MS      = 10
 *   DDRAW_VTBL_GETBLTSTATUS = 0x44
 *
 * WIN32: Sleep, ExitProcess, IDirectDrawSurface::GetBltStatus (vtable+0x44
 *        on DAT_004fd3c4)
 * LINUX: SDL2 blits (SDL_RenderCopy) are synchronous on the calling thread,
 *        so this polling loop is a no-op.  If GPU-side completion is still
 *        required, call SDL_RenderFlush() once.
 *        Sleep(10) -> SDL_Delay(10) if any retry logic is kept.
 *        ExitProcess(1) -> exit(1) after SDL_Quit().
 *===========================================================================*/
int Panel_WaitBltSync(UIPanel *self)
{
#ifdef LOCO_LINUX
    /*
     * LINUX: SDL2 rendering is synchronous on the calling thread.
     * GetBltStatus polling is unnecessary.  A single SDL_RenderFlush()
     * provides a GPU synchronisation point if needed.
     */
    (void)self;
    SDL_RenderFlush(g_primary_renderer);   /* LINUX: no-op equivalent */
    return 0;
#else
    /*
     * WIN32 implementation:
     *
     * Acquire render lock — FUN_0045b940()
     */
    RenderMutex_Lock();

    {
        int iter;
        int status = 0;

        for (iter = 0; iter < BLIT_SYNC_TIMEOUT_ITERS; ++iter)
        {
            /*
             * Poll GetBltStatus via the COM vtable.
             * Vtable layout: QI=0x00, AddRef=0x04, Release=0x08, ...,
             *   GetBltStatus=0x44.
             *
             * C equivalent of the original MSVC-compiled indirect call:
             *   status = (**(code **)(*(int *)DAT_004fd3c4 + DDRAW_VTBL_GETBLTSTATUS))
             *              (DAT_004fd3c4);
             *
             * WIN32: IDirectDrawSurface::GetBltStatus returns 0 when idle.
             */
            /* WIN32: status = ddraw_GetBltStatus(DAT_004fd3c4); */
            status = 0;   /* stub: assume idle in documentation build */

            /* Store result at object+0x4c */
            self->blit_sync_result = status;   /* +0x4c */

            if (status == 0)
            {
                /* Blit complete — exit polling loop */
                break;
            }

            /*
             * Blit still in progress — sleep and retry.
             * WIN32: Sleep(BLIT_SYNC_SLEEP_MS)
             * LINUX: SDL_Delay(BLIT_SYNC_SLEEP_MS)
             */
            /* WIN32: Sleep(BLIT_SYNC_SLEEP_MS); */

            if (iter == BLIT_SYNC_TIMEOUT_ITERS - 1)
            {
                /*
                 * Timeout after 1000 * 10ms = 10 seconds.
                 * WIN32: FUN_00463600("blit timeout"); ExitProcess(1);
                 * LINUX: FatalError_Log("blit timeout"); exit(1);
                 */
                RenderMutex_Unlock();
                FatalError_Log("Panel_WaitBltSync: DirectDraw blit timeout");
                /* not reached */
                return -1;
            }
        }

        RenderMutex_Unlock();
        return status;
    }
#endif /* LOCO_LINUX */
}

/*===========================================================================
 * Panel_UpdateWrapper
 * Original address: 0x00426b70
 *
 * Thin convenience wrapper that triggers a full panel repaint.
 *
 * Behaviour:
 *   Extracts the window handle from param_1+0x08 and calls Panel_Render
 *   (FUN_00426b90) with:
 *     param_2 = 0    (no clip rect hint — do not call SetClipper)
 *     param_3 = NUL  (normal update path)
 *     param_4 = NULL (null RECT* means repaint the entire panel bounds)
 *
 * Note: The stack RECT* argument resolves to an uninitialized on-stack address,
 *       but Panel_Render treats a null param_4 as "repaint full bounds", so the
 *       actual value is ignored on the null-fallthrough path.
 *
 * WIN32: none (all Win32 calls are inside Panel_Render)
 * LINUX: Direct call to the SDL2 equivalent of Panel_Render with a null
 *        SDL_Rect* to repaint the full surface.
 *===========================================================================*/
void Panel_UpdateWrapper(UIPanel *self, void *unused_param)
{
    (void)unused_param;

    if (!self) return;

    /*
     * Call Panel_Render with:
     *   use_clipper = 0    — do not set a DirectDraw clipper / SDL clip rect
     *   special_mode = 0   — normal render path (not NUL special mode)
     *   dirty_rect = NULL  — repaint entire panel bounds
     *
     * The HWND/window handle is at self+0x08; Panel_Render uses it internally.
     */
    Panel_Render(self,
                 /*use_clipper=*/0,
                 /*special_mode=*/0,
                 /*dirty_rect=*/NULL);
}

/*===========================================================================
 * Panel_Render
 * Original address: 0x00426b90
 *
 * Core panel blit and dirty-rect engine.
 *
 * Behaviour (param_3 == 0, normal mode):
 *   1. If param_2 non-zero: calls IDirectDrawSurface::SetClipper (vtable+0x68)
 *      to restrict subsequent blits to a sub-region.
 *   2. Acquires the render lock.
 *   3. Reads the current cursor via GetCursorPos and converts to panel-local
 *      coordinates by subtracting the panel screen origin (this+0x2c/0x30).
 *   4. Clips the result to the panel viewport rectangle at this+0xd4..0xe0.
 *   5. Handles sprite animation: if frame_count (this+0x20) >= 2, offsets the
 *      source X by (current_frame * frame_width) — this+0x18 * this+0x24.
 *   6. Uses IntersectRect against param_4 and the stored dirty rect
 *      (this+0x50) to decide whether to skip the blit entirely.
 *   7. Calls UnionRect to merge old and new dirty rects.
 *   8. Blits the LOCOBITMAP via IDirectDrawSurface::Blt (vtable+0x14 on the
 *      surface pointer at this+0x48) and the internal FUN_0042b050 blitter.
 *   9. Calls FUN_00401280 to invalidate the merged screen rect.
 *  10. Calls a second Blt on DAT_004fd3c4 to push pixels to the primary
 *      surface (front buffer flip).
 *  11. Updates the stored dirty rect at this+0x50..0x5c.
 *
 * Constants:
 *   DDBLT_WAIT            = 0x1000000  (5th arg to every Blt call)
 *   DDRAW_VTBL_BLT        = 0x14
 *   DDRAW_VTBL_SETCLIPPER = 0x68
 *
 * WIN32: GetCursorPos, IntersectRect, UnionRect,
 *        IDirectDrawSurface::Blt (vtable+0x14),
 *        IDirectDrawSurface::SetClipper (vtable+0x68)
 * LINUX: GetCursorPos  -> SDL_GetGlobalMouseState(&x, &y)
 *        IntersectRect -> SDL_IntersectRect (SDL 2.0.4+) or manual clip
 *        UnionRect     -> SDL_UnionRect (SDL 2.0.4+) or manual union
 *        Blt           -> SDL_RenderCopy(renderer, texture, &src, &dst)
 *        SetClipper    -> SDL_RenderSetClipRect(renderer, &clip_rect)
 *===========================================================================*/
void Panel_Render(UIPanel *self, int use_clipper, int special_mode,
                  const LOCO_RECT *dirty_rect)
{
    int cursor_x, cursor_y;
    int local_cursor_x, local_cursor_y;
    int src_x_offset;
    LOCO_RECT combined_rect;
    LOCO_RECT intersection;

    if (!self) return;

    /*
     * Step 1: optional clipper setup.
     * If use_clipper is non-zero, restrict the blit region.
     *
     * WIN32: IDirectDrawSurface::SetClipper (vtable offset 0x68) called on
     *        the global primary surface DAT_004fd3c4.
     *        Vtable call: (**(code **)(*(int *)DAT_004fd3c4 + DDRAW_VTBL_SETCLIPPER))
     *                      (DAT_004fd3c4, clipper_object);
     * LINUX: SDL_RenderSetClipRect(g_primary_renderer, &clip_rect)
     */
    if (use_clipper)
    {
#ifndef LOCO_LINUX
        /* WIN32: ddraw_SetClipper(DAT_004fd3c4, clipper); */
#else
        /* LINUX: SDL_RenderSetClipRect(g_primary_renderer, &some_clip_rect); */
        /* clip rect would be derived from the panel viewport at +0xd4..0xe0 */
        SDL_Rect sdl_clip;
        sdl_clip.x = self->clip_left;    /* +0xd4 */
        sdl_clip.y = self->clip_top;     /* +0xd8 */
        sdl_clip.w = self->clip_right  - self->clip_left;
        sdl_clip.h = self->clip_bottom - self->clip_top;
        SDL_RenderSetClipRect(g_primary_renderer, &sdl_clip);
#endif
    }

    /*
     * special_mode != 0 takes an alternative early-exit path (not fully
     * documented here; the NUL/0 path is the normal render path).
     */
    if (special_mode != 0)
    {
        /* Alternative path: details not fully decompiled in this analysis. */
        return;
    }

    /* Step 2: acquire render lock — FUN_0045b940 */
    RenderMutex_Lock();

    /*
     * Step 3: get current cursor position and convert to panel-local coords.
     *
     * WIN32: POINT pt; GetCursorPos(&pt);
     *        local_x = pt.x - self->screen_origin_x;   // this+0x2c
     *        local_y = pt.y - self->screen_origin_y;   // this+0x30
     * LINUX: SDL_GetGlobalMouseState(&cursor_x, &cursor_y)
     *        (SDL 2.0.4+ gives global screen coordinates matching GetCursorPos)
     */
#ifndef LOCO_LINUX
    /* WIN32: POINT pt; GetCursorPos(&pt); cursor_x = pt.x; cursor_y = pt.y; */
    cursor_x = 0; cursor_y = 0;   /* stub */
#else
    /* LINUX: SDL_GetGlobalMouseState — global screen coordinates */
    SDL_GetGlobalMouseState(&cursor_x, &cursor_y);
#endif

    local_cursor_x = cursor_x - self->screen_origin_x;   /* this+0x2c */
    local_cursor_y = cursor_y - self->screen_origin_y;   /* this+0x30 */

    /*
     * Step 4: clip local cursor to the panel viewport rectangle.
     * Viewport is at this+0xd4 (clip_left), +0xd8 (clip_top),
     *              +0xdc (clip_right), +0xe0 (clip_bottom).
     */
    if (local_cursor_x < self->clip_left)   local_cursor_x = self->clip_left;
    if (local_cursor_x > self->clip_right)  local_cursor_x = self->clip_right;
    if (local_cursor_y < self->clip_top)    local_cursor_y = self->clip_top;
    if (local_cursor_y > self->clip_bottom) local_cursor_y = self->clip_bottom;

    /*
     * Step 5: sprite animation source-rect offset.
     * If frame_count (this+0x20) >= 2 the bitmap is an animated sprite strip.
     * Source X is offset by: current_frame (this+0x24) * panel_width (this+0x18).
     *
     * LINUX: SDL_Rect src; src.x = self->current_frame * self->panel_width;
     */
    src_x_offset = 0;
    if (self->frame_count >= 2)   /* +0x20 */
    {
        src_x_offset = self->current_frame * self->panel_width;
        /* current_frame is at +0x24, panel_width is at +0x18 */
    }

    /*
     * Step 6: dirty-rect intersection test.
     * Use IntersectRect to check whether param_4 (dirty_rect) overlaps the
     * stored dirty rect at this+0x50..0x5c.
     * If there is no overlap, skip the blit entirely.
     *
     * WIN32: IntersectRect(&intersection, dirty_rect, &self->dirty_rect)
     * LINUX: SDL_IntersectRect(&sdl_dirty, &sdl_param4, &sdl_result)
     */
    if (dirty_rect != NULL)
    {
        LOCO_RECT self_dirty;
        self_dirty.left   = self->dirty_left;    /* +0x50 */
        self_dirty.top    = self->dirty_top;     /* +0x54 */
        self_dirty.right  = self->dirty_right;   /* +0x58 */
        self_dirty.bottom = self->dirty_bottom;  /* +0x5c */

#ifndef LOCO_LINUX
        /* WIN32: if (!IntersectRect(&intersection, dirty_rect, &self_dirty)) return; */
        (void)intersection;
#else
        {
            SDL_Rect sdl_a, sdl_b, sdl_result;
            sdl_a.x = dirty_rect->left;
            sdl_a.y = dirty_rect->top;
            sdl_a.w = dirty_rect->right  - dirty_rect->left;
            sdl_a.h = dirty_rect->bottom - dirty_rect->top;
            sdl_b.x = self_dirty.left;
            sdl_b.y = self_dirty.top;
            sdl_b.w = self_dirty.right  - self_dirty.left;
            sdl_b.h = self_dirty.bottom - self_dirty.top;
            /* LINUX: SDL_IntersectRect — returns SDL_TRUE if rects overlap */
            if (!SDL_IntersectRect(&sdl_a, &sdl_b, &sdl_result))
            {
                RenderMutex_Unlock();
                return;   /* no overlap — skip blit */
            }
        }
#endif
    }

    /*
     * Step 7a: merge dirty rects.
     * WIN32: UnionRect(&combined_rect, &old_dirty, &new_dirty)
     * LINUX: SDL_UnionRect
     */
    {
        LOCO_RECT old_dirty;
        old_dirty.left   = self->dirty_left;
        old_dirty.top    = self->dirty_top;
        old_dirty.right  = self->dirty_right;
        old_dirty.bottom = self->dirty_bottom;

        /* Build the "new" dirty rect from cursor position and panel size */
        LOCO_RECT new_dirty;
        new_dirty.left   = local_cursor_x;
        new_dirty.top    = local_cursor_y;
        new_dirty.right  = local_cursor_x + self->panel_width;
        new_dirty.bottom = local_cursor_y + self->panel_height;

#ifndef LOCO_LINUX
        /* WIN32: UnionRect(&combined_rect, &old_dirty, &new_dirty); */
        combined_rect = old_dirty;   /* stub */
        (void)new_dirty;
#else
        {
            SDL_Rect sdl_old, sdl_new, sdl_union;
            sdl_old.x = old_dirty.left;  sdl_old.y = old_dirty.top;
            sdl_old.w = old_dirty.right  - old_dirty.left;
            sdl_old.h = old_dirty.bottom - old_dirty.top;
            sdl_new.x = new_dirty.left;  sdl_new.y = new_dirty.top;
            sdl_new.w = new_dirty.right  - new_dirty.left;
            sdl_new.h = new_dirty.bottom - new_dirty.top;
            /* LINUX: SDL_UnionRect */
            SDL_UnionRect(&sdl_old, &sdl_new, &sdl_union);
            combined_rect.left   = sdl_union.x;
            combined_rect.top    = sdl_union.y;
            combined_rect.right  = sdl_union.x + sdl_union.w;
            combined_rect.bottom = sdl_union.y + sdl_union.h;
        }
#endif
    }

    /*
     * Step 7b: blit the LOCOBITMAP to the offscreen surface.
     *
     * WIN32: IDirectDrawSurface::Blt (vtable+0x14) on this+0x48 with
     *        DDBLT_WAIT (0x1000000) as the 5th argument.
     *        Indirect call: (**(code **)(*(int *)self->dd_surface + DDRAW_VTBL_BLT))
     *                        (self->dd_surface, &dst_rect, src, &src_rect, DDBLT_WAIT, NULL)
     * LINUX: SDL_RenderCopy(renderer, bitmap_texture, &src_rect, &dst_rect)
     */
    {
#ifndef LOCO_LINUX
        /* WIN32: ddraw_Blt(self->dd_surface, ..., DDBLT_WAIT); */
#else
        SDL_Rect sdl_src, sdl_dst;
        sdl_src.x = src_x_offset;
        sdl_src.y = 0;
        sdl_src.w = self->panel_width;    /* +0x18 */
        sdl_src.h = self->panel_height;   /* +0x1c */
        sdl_dst.x = local_cursor_x + self->screen_origin_x;
        sdl_dst.y = local_cursor_y + self->screen_origin_y;
        sdl_dst.w = self->panel_width;
        sdl_dst.h = self->panel_height;
        /* LINUX: SDL_RenderCopy — replaces IDirectDrawSurface::Blt + DDBLT_WAIT */
        /* SDL_RenderCopy(g_primary_renderer, bitmap_texture, &sdl_src, &sdl_dst); */
        (void)sdl_src; (void)sdl_dst;   /* TODO: supply actual texture */
#endif

        /* Also call the internal LOCOBITMAP blitter (FUN_0042b050) */
        LocoBitmap_Blit(self->content_bitmap,   /* +0x14 */
                        src_x_offset, 0,
                        local_cursor_x, local_cursor_y,
                        self->panel_width, self->panel_height);
    }

    /*
     * Step 8: invalidate the merged screen rect.
     * WIN32: FUN_00401280(&combined_rect) — calls InvalidateRect on panel HWND.
     * LINUX: no-op; SDL2 renderer handles rect tracking automatically.
     */
    Screen_InvalidateRect(&combined_rect);

    /*
     * Step 9: push pixels to the primary surface (front buffer).
     * WIN32: second IDirectDrawSurface::Blt call on DAT_004fd3c4.
     *        Passes DDBLT_WAIT (0x1000000) again.
     * LINUX: SDL_RenderPresent(g_primary_renderer) or SDL_RenderCopy to
     *        the window render target.
     */
#ifndef LOCO_LINUX
    /* WIN32: ddraw_Blt(DAT_004fd3c4, &dst, src, &src_rect, DDBLT_WAIT, NULL); */
#else
    /* LINUX: SDL_RenderPresent(g_primary_renderer); */
    /* Typically deferred to the main render tick rather than called per-panel */
#endif

    /*
     * Step 11: update stored dirty rect.
     * Overwrite this+0x50..0x5c with the combined rect so the next call
     * knows what was last blitted.
     */
    self->dirty_left   = combined_rect.left;    /* +0x50 */
    self->dirty_top    = combined_rect.top;     /* +0x54 */
    self->dirty_right  = combined_rect.right;   /* +0x58 */
    self->dirty_bottom = combined_rect.bottom;  /* +0x5c */

    /* Release render lock */
    RenderMutex_Unlock();

    /*
     * LINUX: reset clip rect after blit if SetClipRect was called above.
     * SDL_RenderSetClipRect(g_primary_renderer, NULL);
     */
#ifdef LOCO_LINUX
    if (use_clipper)
    {
        SDL_RenderSetClipRect(g_primary_renderer, NULL);
    }
#endif
}

/*===========================================================================
 * Panel_DragUpdate
 * Original address: 0x00426eb0
 *
 * Drag-and-drop cursor tracking with incremental dirty-rect repaint.
 *
 * Behaviour:
 *   Guards on drag_active flag at this+0x44.  If not active, returns early.
 *
 *   1. Gets current cursor position; converts to panel-local coords by
 *      subtracting screen_origin_x/y (this+0x2c/0x30).
 *   2. Clips to panel viewport (this+0xd4..0xe0).
 *   3. Dirty-rect coalescing optimisation:
 *      If the UnionRect of the previous blit rect (this+0x50..0x5c) and the
 *      new-position rect fits within DIRTY_RECT_MAX_DIM x DIRTY_RECT_MAX_DIM
 *      (256x256 px), a 4-pixel-padded combined region is rendered in one pass.
 *      The CONCAT13 flag (internal) triggers this merged path.
 *   4. When param_1 is non-zero and no coalescing optimisation is active:
 *      - Restores the old cursor position to the background by blitting from
 *        DAT_004fd3c0 (secondary surface) to the old rect.
 *      - Resets last_cursor_x/y to CURSOR_INVALID_SENTINEL (0xffffffff).
 *   5. Updates this+0x50..0x5c with the new dirty rect.
 *   6. Blits the dragged bitmap via FUN_0042b050.
 *   7. Pushes to screen via Blt on DAT_004fd3c0.
 *
 * Constants:
 *   DIRTY_RECT_MAX_DIM      = 0x100 (256 px) — coalescing threshold
 *   DIRTY_RECT_PADDING      = 4 px — border added to coalesced rect
 *   CURSOR_INVALID_SENTINEL = 0xffffffff (-1) — "no previous position"
 *   DDBLT_WAIT              = 0x1000000
 *
 * WIN32: GetCursorPos, UnionRect,
 *        IDirectDrawSurface::Blt (vtable+0x14 on DAT_004fd3c0)
 * LINUX: GetCursorPos  -> SDL_GetGlobalMouseState(&x, &y)
 *        UnionRect     -> SDL_UnionRect
 *        Blt on DAT_004fd3c0 -> SDL_RenderCopy of saved background texture
 *===========================================================================*/
void Panel_DragUpdate(UIPanel *self, int flag)
{
    int cursor_x, cursor_y;
    int local_x, local_y;
    int use_coalesce;
    LOCO_RECT new_rect;
    LOCO_RECT union_rect;

    if (!self) return;

    /*
     * Guard: only run when drag is active.
     * drag_active flag is at object+0x44.
     */
    if (!self->drag_active)   /* +0x44 */
        return;

    /*
     * Step 1: get cursor in panel-local coordinates.
     *
     * WIN32: POINT pt; GetCursorPos(&pt);
     *        local_x = pt.x - self->screen_origin_x;
     *        local_y = pt.y - self->screen_origin_y;
     * LINUX: SDL_GetGlobalMouseState(&cursor_x, &cursor_y)
     */
#ifndef LOCO_LINUX
    /* WIN32: POINT pt; GetCursorPos(&pt); cursor_x = pt.x; cursor_y = pt.y; */
    cursor_x = 0; cursor_y = 0;   /* stub */
#else
    /* LINUX: SDL_GetGlobalMouseState — matches GetCursorPos global coords */
    SDL_GetGlobalMouseState(&cursor_x, &cursor_y);
#endif

    local_x = cursor_x - self->screen_origin_x;   /* this+0x2c */
    local_y = cursor_y - self->screen_origin_y;   /* this+0x30 */

    /*
     * Step 2: clip to panel viewport.
     * Viewport bounds at this+0xd4 (clip_left), +0xd8 (clip_top),
     *                    +0xdc (clip_right), +0xe0 (clip_bottom).
     */
    if (local_x < self->clip_left)    local_x = self->clip_left;
    if (local_x > self->clip_right)   local_x = self->clip_right;
    if (local_y < self->clip_top)     local_y = self->clip_top;
    if (local_y > self->clip_bottom)  local_y = self->clip_bottom;

    /*
     * Build the new-position dirty rect from cursor local coords and bitmap
     * dimensions (panel_width at +0x18, panel_height at +0x1c).
     */
    new_rect.left   = local_x;
    new_rect.top    = local_y;
    new_rect.right  = local_x + self->panel_width;
    new_rect.bottom = local_y + self->panel_height;

    /*
     * Step 3: dirty-rect coalescing optimisation.
     *
     * Compute the union of the old dirty rect (this+0x50..0x5c) and the new
     * position rect.  If both width and height of the union are < 256 px,
     * a single padded combined blit is cheaper than two separate blits.
     *
     * WIN32: UnionRect(&union_rect, &self->dirty_rect, &new_rect)
     * LINUX: SDL_UnionRect
     */
    use_coalesce = 0;
    {
        LOCO_RECT old_rect;
        old_rect.left   = self->dirty_left;    /* +0x50 */
        old_rect.top    = self->dirty_top;     /* +0x54 */
        old_rect.right  = self->dirty_right;   /* +0x58 */
        old_rect.bottom = self->dirty_bottom;  /* +0x5c */

#ifndef LOCO_LINUX
        /* WIN32: UnionRect(&union_rect, &old_rect, &new_rect); */
        union_rect = old_rect;   /* stub */
#else
        {
            SDL_Rect sdl_old, sdl_new, sdl_union;
            sdl_old.x = old_rect.left; sdl_old.y = old_rect.top;
            sdl_old.w = old_rect.right  - old_rect.left;
            sdl_old.h = old_rect.bottom - old_rect.top;
            sdl_new.x = new_rect.left; sdl_new.y = new_rect.top;
            sdl_new.w = new_rect.right  - new_rect.left;
            sdl_new.h = new_rect.bottom - new_rect.top;
            /* LINUX: SDL_UnionRect */
            SDL_UnionRect(&sdl_old, &sdl_new, &sdl_union);
            union_rect.left   = sdl_union.x;
            union_rect.top    = sdl_union.y;
            union_rect.right  = sdl_union.x + sdl_union.w;
            union_rect.bottom = sdl_union.y + sdl_union.h;
        }
#endif

        /*
         * Coalesce if the union fits within DIRTY_RECT_MAX_DIM x DIRTY_RECT_MAX_DIM.
         * The 4-pixel (DIRTY_RECT_PADDING) border expansion prevents edge tearing.
         */
        if ((union_rect.right  - union_rect.left) < DIRTY_RECT_MAX_DIM &&
            (union_rect.bottom - union_rect.top)  < DIRTY_RECT_MAX_DIM)
        {
            use_coalesce = 1;
            /* Expand by DIRTY_RECT_PADDING pixels on each edge */
            union_rect.left   -= DIRTY_RECT_PADDING;
            union_rect.top    -= DIRTY_RECT_PADDING;
            union_rect.right  += DIRTY_RECT_PADDING;
            union_rect.bottom += DIRTY_RECT_PADDING;
        }
    }

    if (use_coalesce)
    {
        /*
         * Coalesced path: render the combined (old+new) region in a single
         * blit pass, padded by DIRTY_RECT_PADDING to avoid tearing.
         *
         * LINUX: SDL_RenderCopy(g_primary_renderer, background_texture,
         *                       &sdl_union, &sdl_union)
         *        then SDL_RenderCopy for the dragged sprite at new_rect.
         */
#ifndef LOCO_LINUX
        /* WIN32: ddraw_Blt(DAT_004fd3c0, &union_rect, ..., DDBLT_WAIT); */
#else
        {
            SDL_Rect sdl_union_rect;
            sdl_union_rect.x = union_rect.left;
            sdl_union_rect.y = union_rect.top;
            sdl_union_rect.w = union_rect.right  - union_rect.left;
            sdl_union_rect.h = union_rect.bottom - union_rect.top;
            /* LINUX: restore background over the combined region */
            /* SDL_RenderCopy(g_primary_renderer, g_secondary_texture,
             *                &sdl_union_rect, &sdl_union_rect); */
            (void)sdl_union_rect;   /* TODO: supply background texture */
        }
#endif
    }
    else
    {
        /*
         * Non-coalesced path with param_1 (flag) != 0:
         * Restore background at the OLD cursor position from the secondary
         * DirectDraw surface (DAT_004fd3c0) before drawing at the new position.
         *
         * WIN32: IDirectDrawSurface::Blt (vtable+0x14) on DAT_004fd3c0 with
         *        DDBLT_WAIT to restore old background pixels.
         * LINUX: SDL_RenderCopy of the pre-saved background snapshot texture
         *        to the old rect position.
         */
        if (flag)
        {
            /* Only restore if last_cursor is valid (not CURSOR_INVALID_SENTINEL) */
            if (self->last_cursor_x != (int)CURSOR_INVALID_SENTINEL)
            {
#ifndef LOCO_LINUX
                /* WIN32: ddraw_Blt(DAT_004fd3c0, &old_dst, src, &old_src,
                 *                  DDBLT_WAIT, NULL); */
#else
                /* LINUX: restore background texture to the old cursor region */
                SDL_Rect old_dst;
                old_dst.x = self->last_cursor_x + self->screen_origin_x;
                old_dst.y = self->last_cursor_y + self->screen_origin_y;
                old_dst.w = self->panel_width;
                old_dst.h = self->panel_height;
                /* SDL_RenderCopy(g_primary_renderer, g_secondary_texture,
                 *                &old_dst, &old_dst); */
                (void)old_dst;
#endif
            }

            /*
             * Reset last cursor position to sentinel: (0xffffffff, 0xffffffff).
             * Sentinel value CURSOR_INVALID_SENTINEL = -1 as int.
             */
            self->last_cursor_x = (int)CURSOR_INVALID_SENTINEL;   /* +0x34 */
            self->last_cursor_y = (int)CURSOR_INVALID_SENTINEL;   /* +0x38 */
        }
    }

    /*
     * Step 5: update stored dirty rect to the new position.
     * this+0x50..0x5c
     */
    self->dirty_left   = new_rect.left;
    self->dirty_top    = new_rect.top;
    self->dirty_right  = new_rect.right;
    self->dirty_bottom = new_rect.bottom;

    /*
     * Step 6: blit the dragged bitmap at the new cursor position.
     * Uses the internal LOCOBITMAP blitter (FUN_0042b050).
     */
    LocoBitmap_Blit(self->content_bitmap,   /* +0x14 */
                    0, 0,
                    local_x, local_y,
                    self->panel_width, self->panel_height);

    /*
     * Step 7: push the updated region to the screen surface.
     *
     * WIN32: IDirectDrawSurface::Blt (vtable+0x14) on DAT_004fd3c0 with
     *        DDBLT_WAIT (0x1000000).
     * LINUX: SDL_RenderCopy(g_primary_renderer, ...) or SDL_RenderPresent.
     */
#ifndef LOCO_LINUX
    /* WIN32: ddraw_Blt(DAT_004fd3c0, &new_dst, src, &src_rect, DDBLT_WAIT, NULL); */
#else
    {
        SDL_Rect sdl_dst;
        sdl_dst.x = local_x + self->screen_origin_x;
        sdl_dst.y = local_y + self->screen_origin_y;
        sdl_dst.w = self->panel_width;
        sdl_dst.h = self->panel_height;
        /* LINUX: SDL_RenderCopy to push new dragged sprite to screen */
        /* SDL_RenderCopy(g_primary_renderer, dragged_texture, &sdl_src, &sdl_dst); */
        (void)sdl_dst;   /* TODO: supply dragged sprite texture */
    }
#endif
}

/*===========================================================================
 * UIPanel_Ctor
 * Original address: 0x00427370
 *
 * Constructor for the UIPanel object.
 *
 * Sets up MSVC SEH frame via ExceptionList manipulation (Windows-only).
 * Calls the base-class constructor FUN_004544e0.
 * Initialises sub-objects and zeroes all bookkeeping fields.
 *
 * Field initialisation summary:
 *   +0x3f0: UIScrollPos sub-object — scroll_x=-1, scroll_y=-1, w=0, h=0
 *             via FUN_00405790(-1, -1, 0, 0)
 *   +0x478: UIHotspotList sub-object — via FUN_0042a110
 *   +0x00:  vtable pointer = PTR_FUN_00477cc8
 *   +0x04:  type = PANEL_TYPE (0xc)
 *   +0x49c: panel_mode = 0 (hidden)
 *   +0x4a0..+0x4b8: UIButton* tab pointers zeroed (4 slots)
 *   +0x4bc: label_text pointer zeroed
 *   +0x4c0..+0x4d4: UIBuildTile*[6] building tile array zeroed
 *   +0x4d4: list_ptr zeroed
 *   +0x4d8: content_list_head zeroed
 *   +0x4dc: aux_list zeroed
 *   +0xe0:  aux flag byte zeroed
 *   +0x2ea: aux_flag zeroed
 *
 * WIN32: Windows SEH via ExceptionList TIB field.
 * LINUX: MSVC SEH has no POSIX equivalent.  Remove the __try/__except pattern.
 *        Use plain constructor body or C++ try/catch if exception safety is
 *        needed.  ExceptionList is a Windows TIB field with no POSIX analogue.
 *
 * Object size: at least 0x4e0 (1248) bytes.
 * Type constant: PANEL_TYPE = 0xc identifies the object for runtime checks.
 * Sub-object at +0x3f0 default (-1,-1,0,0) = "no scroll" sentinel values.
 * The 6 zeroed dwords at +0x4c0 are building-picker tile slots populated
 * later by FUN_00427580 (UIPanel_LoadContent).
 *===========================================================================*/
UIPanel *UIPanel_Ctor(UIPanel *self)
{
    if (!self) return NULL;

    /*
     * WIN32 note: here the original code sets up an MSVC SEH frame by writing
     * to the ExceptionList field of the Thread Information Block (TIB) via
     * FS:[0].  This is Windows-specific and has no POSIX equivalent.
     *
     * LINUX: simply omit the SEH setup.  If the constructor can throw,
     *        use C++ RAII or try/catch instead.
     */

    /*
     * Call the base-class constructor.
     * FUN_004544e0(self) — initialises the root object fields before the
     * UIPanel-specific fields are filled.
     * LINUX: call the equivalent C++ base constructor or initialise common
     *        fields here.
     */
    /* base_ctor(self); */   /* FUN_004544e0 */

    /*
     * Initialise the UIScrollPos sub-object at object+0x3f0.
     * FUN_00405790(sub_obj, -1, -1, 0, 0) sets:
     *   scroll_x = -1 (no scroll, sentinel)
     *   scroll_y = -1
     *   width    = 0
     *   height   = 0
     */
    {
        UIScrollPos *scroll = (UIScrollPos *)((char *)self + 0x3f0);
        scroll->scroll_x = -1;
        scroll->scroll_y = -1;
        scroll->width    = 0;
        scroll->height   = 0;
    }

    /*
     * Initialise the UIHotspotList sub-object at object+0x478.
     * FUN_0042a110 initialises the dynamic button/hotspot array.
     * LINUX: call the C++ UIHotspotList constructor or memset the region.
     */
    /* hotspot_list_ctor((UIHotspotList *)((char *)self + 0x478)); */
    memset((char *)self + 0x478, 0, sizeof(UIHotspotList));

    /* Set the vtable pointer — UIPanel virtual dispatch table */
    self->vtable = &g_uipanel_vtable;   /* +0x00  PTR_FUN_00477cc8 */

    /* Set the type identifier for runtime type checking */
    self->type = PANEL_TYPE;   /* +0x04  0xc */

    /* Initialise all panel-specific fields to zero / null */
    self->panel_mode    = PANEL_MODE_HIDDEN;   /* +0x49c  uint16 = 0 */
    self->btn_tab1      = NULL;                /* +0x4a0 */
    self->btn_tab2      = NULL;                /* +0x4a4 */
    self->btn_tab3      = NULL;                /* +0x4a8 */
    self->btn_tab4      = NULL;                /* +0x4ac */
    self->btn_content   = NULL;                /* +0x4b0 */
    self->label_text    = NULL;                /* +0x4bc */

    /* Zero the 6-element building-tile array (+0x4c0 through +0x4d4) */
    memset(&self->building_tiles[0], 0, BUILDING_TILE_COUNT * sizeof(void *));
    /* building_tiles is at +0x4c0; 6 pointers, each 4 bytes = 24 bytes */

    self->list_ptr          = NULL;   /* +0x4d4 */
    self->content_list_head = NULL;   /* +0x4d8 */
    self->aux_list          = NULL;   /* +0x4dc */

    /* Zero auxiliary flag bytes */
    *((char *)self + 0xe0)  = 0;   /* clip_bottom / aux flag at +0xe0 */
    self->aux_flag          = 0;   /* +0x2ea */

    /*
     * WIN32 note: the original code tears down the SEH frame here by restoring
     * the previous ExceptionList value.  Omit on Linux.
     */

    return self;
}

/*===========================================================================
 * UIPanel_Dtor
 * Original address: 0x00427440
 *
 * MSVC-pattern destructor for UIPanel.
 *
 * The destructor body (FUN_00427460) performs:
 *   1. Walks the linked list of child elements at object+0x4d8, following
 *      next-pointers at element+0x22c (element+0x8b*4).
 *      Calls each child's vtable[0] destructor with delete_flag=1.
 *   2. Resets the vtable to PTR_FUN_00477cc8 (base vtable).
 *   3. Calls vtable method +0x18 (release/close) on the main object and on
 *      the sub-object at object+0xfc.
 *   4. Calls FUN_00454630 (base destructor).
 *   5. Calls FUN_0042a370 (UIHotspotList destructor at +0x11e, i.e. +0x478).
 *   6. Calls FUN_00405870 (UIScrollPos destructor at +0xfc, i.e. +0x3f0).
 *   7. Calls FUN_004545a0 (root base destructor).
 *   If param_1 bit 0 is set, calls FUN_00465cd0 (operator delete) to free
 *   the object memory.
 *
 * bit-0 convention (MSVC virtual destructor):
 *   bit 0 set   -> destruct + free memory (heap object)
 *   bit 0 clear -> destruct in place (stack or embedded object)
 *
 * WIN32: none (no Win32 API calls in the destructor itself)
 * LINUX: FUN_00465cd0 (operator delete) maps directly to free().
 *        MSVC SEH cleanup in FUN_00427460 (ExceptionList restore) is removed.
 *===========================================================================*/
void UIPanel_Dtor(UIPanel *self, int delete_flag)
{
    UIElement *child;
    UIElement *next_child;

    if (!self) return;

    /*
     * Step 1: walk the linked list of child elements at object+0x4d8 and
     * destroy each one.
     *
     * Next-pointer is at element+0x22c (element+0x8b*4 = element+0x22c).
     * Each child's vtable[0] is its destructor; call with delete_flag=1 to
     * free child memory.
     */
    child = (UIElement *)self->content_list_head;   /* +0x4d8 */
    while (child != NULL)
    {
        /* Read next pointer before destroying child */
        next_child = (UIElement *)*(void **)((char *)child + 0x22c);

        /*
         * Call the child's virtual destructor.
         * vtable[0](child, 1) — destructor + free.
         * LINUX: equivalent to calling the C++ destructor and then free().
         */
        if (child->vtable && child->vtable[0])
        {
            ((void (*)(UIElement *, int))child->vtable[0])(child, 1);
        }

        child = next_child;
    }

    /*
     * Step 2: reset vtable to the UIPanel base vtable before further cleanup.
     * This matches the MSVC pattern of re-setting the vptr at the start of
     * the destructor body to prevent pure-virtual calls during teardown.
     */
    self->vtable = &g_uipanel_vtable;   /* PTR_FUN_00477cc8 */

    /*
     * Step 3: call vtable[6] (method at vtable offset +0x18, 0-indexed as
     * the 7th pointer) — release/close on the main object and the sub-object
     * at object+0xfc.
     * LINUX: call the equivalent C++ release method.
     */
    if (self->vtable && self->vtable[6])
    {
        ((void (*)(UIPanel *))self->vtable[6])(self);
    }
    /* Also call on sub-object at +0xfc if it has the same interface */
    /* ((void (*)(void*))sub_vtable_release)((char*)self + 0xfc); */

    /*
     * Steps 4-7: call base and sub-object destructors in reverse construction
     * order.
     *
     * FUN_00454630 — base destructor
     * FUN_0042a370 — UIHotspotList destructor (+0x478 / field index +0x11e)
     * FUN_00405870 — UIScrollPos destructor (+0x3f0 / field +0xfc relative)
     * FUN_004545a0 — root base destructor
     *
     * LINUX: call matching C++ destructors or cleanup functions.
     */
    /* base_dtor(self);                              FUN_00454630 */
    /* hotspot_list_dtor((char*)self + 0x478);       FUN_0042a370 */
    /* scroll_pos_dtor((char*)self + 0x3f0);         FUN_00405870 */
    /* root_base_dtor(self);                         FUN_004545a0 */

    /*
     * If the low bit of delete_flag is set, free the object memory.
     *
     * WIN32: FUN_00465cd0(self) — operator delete
     * LINUX: free(self)
     */
    if (delete_flag & 1)
    {
#ifndef LOCO_LINUX
        /* WIN32: FUN_00465cd0(self); */
#else
        /* LINUX: free(self) */
        free(self);
#endif
    }
}

/*===========================================================================
 * UIPanel_Init (synthesis helper — not a named original function)
 *
 * Allocates and constructs a new UIPanel on the heap.
 * Equivalent to the `new UIPanel()` call sites in the original binary.
 *
 * LINUX: malloc + UIPanel_Ctor.
 *===========================================================================*/
UIPanel *UIPanel_New(void)
{
    UIPanel *panel = (UIPanel *)calloc(1, sizeof(UIPanel));
    if (!panel) return NULL;
    return UIPanel_Ctor(panel);
}

/*===========================================================================
 * UIPanel_Delete (synthesis helper)
 *
 * Destructs and frees a heap-allocated UIPanel.
 * Equivalent to `delete panel` in the original binary.
 *
 * LINUX: UIPanel_Dtor with delete_flag=1.
 *===========================================================================*/
void UIPanel_Delete(UIPanel *panel)
{
    UIPanel_Dtor(panel, /*delete_flag=*/1);
}

/*===========================================================================
 * Section 2: EditWindow / PanelA / PanelB / Main-Menu System
 *
 * Decompiled from loco.exe.  All addresses refer to the original PE32 image.
 *
 * ARCHITECTURE OVERVIEW
 * ---------------------
 * The dialog system is a hand-rolled C++ hierarchy built on Win32.
 * Each class stores its vtable pointer at offset +0 (MSVC layout).
 * Objects are heap-allocated via FUN_00465ce0 (custom new) and freed via
 * FUN_00465cd0 (custom delete).
 *
 * STARTUP SEQUENCE  (FUN_00462e90 = WinMain)
 * ------------------------------------------
 *  1. CreateDialogParamA(resource 0x71)    675x450 centered splash dialog
 *  2. InstallPath_and_INI_init (0x4068d0)  registry -> lego.ini; resource paths
 *  3. CmdLine_parse (0x406790)             holiday themes; screensaver flag
 *  4. MainMenu_showAfterSetup (0x4480c0)   password.cpl; plays music.wav
 *  5. Window_and_INI_setup (0x406480)      screen dims; [BALANCING] FPS limits
 *  6. Display_capability_check (0x406680)  palette/depth/resolution guards
 *  7. FindWindowA("LEGO LOCO")             single-instance guard
 *  8. GameSubsystems_init (0x406ba0)       all subsystems + window + 28ms timer
 *  9. if g_screensaverMode==0:
 *         MultiplayerLobby_show() -> GameState_machine(2)  [normal menu]
 *     else:
 *         GameState_machine(1)                              [screensaver]
 * 10. ShowWindow, destroy splash, Win32 message loop
 *
 * DIALOG STATE MACHINE  (EditWindow_setState / IntroMenu_animation_state)
 * -----------------------------------------------------------------------
 *   0  initial
 *   1  hidden: PlaySoundA(NULL); ShowWindow(edit, SW_HIDE)
 *   2  deactivated: hide edit; stop PanelA; stop PanelB if prev was 4/5
 *   3  show panels: show PanelA; resolve to 4/5; show PanelB
 *   4  Panel B singleplayer: hide edit; show PanelB
 *   5  Panel B multiplayer:  hide edit; show PanelB
 *   6  shutdown: hide panels; GameState_machine(1) -> gameplay
 *   7  close child dialog: restore wndproc; destroy dialog; play music.wav
 *
 * LOCALIZATION
 * ------------
 * ResourceManager_lookup (0x00446ea0) and LocalizedString_load (0x00447330)
 * apply per-language string-table offsets for resource/string IDs in [100,500]:
 *   Lang 1 (ENGLISH)    -> +0x6cfc
 *   Lang 2 (DANISH)     -> +0x652c
 *   Lang 4 (DUTCH)      -> +0x6338
 *   Lang 5 (SPANISH)    -> +0x6144
 *   Lang 6 (FRENCH)     -> +0x6914
 *   Lang 7 (GERMAN)     -> +0x6720
 *   Lang 8 (ITALIAN)    -> +0x6ef0
 *   Lang 9 (PORTUGUESE) -> +0x6b08
 * English fallback is used when LoadStringA returns 0 for the translated ID.
 *
 * TILE BITMAP CACHE  (TileBitmap_cache_get, 0x004442b0)
 * -----------------------------------------------------
 * 256-entry LRU cache keyed on (type:byte, row:byte, col:byte).
 * Keys: this+0x808, timestamps: this+0x408+i*4.
 * Filename patterns by tile type:
 *   0-15: [install]\Clipart\{type:01x}_{row-1:01d}_{col:03d}.bmp
 *   16-25: char(type+0x58)_{...}.bmp
 *   26-29: char(type+0x5a)_{...}.bmp
 *   30 'R': R_{row-1:01d}_{col:03d}.bmp
 *   31:     S0_{col:03d}.bmp
 *   default: 0_{row-1:01d}_{col:03d}.bmp
 *===========================================================================*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Internal allocator shims
 * WIN32: FUN_00465ce0 / FUN_00465cd0 (custom slab allocator)
 * LINUX: malloc / free
 * ---------------------------------------------------------------------- */
static void *loco_alloc(size_t n) { return malloc(n); }
static void  loco_free(void *p)   { free(p); }

/* -------------------------------------------------------------------------
 * Global singletons  (defined here; extern declarations in ui.h)
 * ---------------------------------------------------------------------- */
EditWindow *g_pEditWindow        = NULL;   /* DAT_00485240 */
PanelA     *g_pPanelA            = NULL;   /* DAT_00485260 */
void       *g_pConfigMgr         = NULL;   /* DAT_004fd3a8 */
void       *g_pNpcMgr            = NULL;   /* DAT_004fd3ac */
void       *g_pDDrawRenderer     = NULL;   /* DAT_004fd378 */
void       *g_pUserProfile       = NULL;   /* DAT_004aa4a8 */
int         g_screensaverMode    = 0;      /* DAT_004a9918 */
int         g_seasonOverride     = 0;      /* DAT_00485230 */
int32_t     g_locobitmapRefCount = 0;      /* DAT_00485254 */

/* Actual screen dimensions, set once when the display mode is selected.
 * The main menu is authored in MAINMENU_SURFACE_WIDTH x MAINMENU_SURFACE_HEIGHT
 * (1280x1024) "design space" and centered onto these. */
int         g_screenWidth        = 640;    /* DAT_004851d8 */
int         g_screenHeight       = 480;    /* DAT_00485214 */

/* -------------------------------------------------------------------------
 * Small rect helpers — direct ports of the GDI32 SetRect / OffsetRect calls
 * the original code uses all over the menu layout/hit-test paths.
 * ---------------------------------------------------------------------- */
static void LOCO_RECT_set(LOCO_RECT *r, int l, int t, int rt, int b)
{
    r->left = l; r->top = t; r->right = rt; r->bottom = b;   /* WIN32: SetRect */
}
static void LOCO_RECT_offset(LOCO_RECT *r, int dx, int dy)
{
    r->left += dx; r->right += dx; r->top += dy; r->bottom += dy; /* WIN32: OffsetRect */
}
static int LOCO_RECT_contains(const LOCO_RECT *r, int x, int y)
{
    /* WIN32: PtInRect — note right/bottom are exclusive */
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}

/* WIN32: FUN_00425d30 — WindowBase (CGWnd) base layout recompute */
extern void WindowBase_recalc(EditWindow *self);

/* Forward decl: helper extracted from MainMenu_recalcLayout (defined below). */
static void MenuButton_placeRect(EditWindow *self, LOCO_RECT *rc,
                                 int x, int y, int spriteIdx);

/* =========================================================================
 * ButtonSprite_ctor  (0x00454b50)
 *
 * Minimal constructor.  Sets vtable to PTR_FUN_0047851c, zeros posX/posY,
 * stores resourceId, clears stateFlags.
 *
 * WIN32: none   LINUX: none
 * ======================================================================= */
ButtonSprite *ButtonSprite_ctor(ButtonSprite *self, int32_t resourceId)
{
    /* WIN32: self->vtable = &PTR_FUN_0047851c */
    self->vtable     = NULL;    /* LINUX: populate with function-pointer table */
    self->posX       = 0;
    self->posY       = 0;
    self->resourceId = resourceId;
    self->stateFlags = 0;
    return self;
}

/* =========================================================================
 * LOCOBITMAP_copy_ctor  (0x0042a1c0)
 *
 * Deep-copies a LOCOBITMAP.  Increments g_locobitmapRefCount.
 *
 * Palette copy: if flags0==1 and palettePtr!=NULL, allocates 0x200 bytes
 *   and copies 0x80 uint32 entries (128 RGB565 values).
 * Pixel copy: if rawpixelPtr!=NULL, allocates width*height bytes.
 * Surface copy: if pDDSurface!=NULL, creates a new SYSTEMMEMORY DirectDraw
 *   surface (caps type=7, flags=0x840) via IDirectDraw vtable+0x18, blits
 *   via FUN_0045ba50.
 *
 * WIN32: IDirectDraw::CreateSurface (vtable index 6 = vtable+0x18)
 * LINUX: SDL_CreateTexture + SDL_RenderCopy
 * ======================================================================= */
LOCOBITMAP *LOCOBITMAP_copy_ctor(LOCOBITMAP *dst, const LOCOBITMAP *src)
{
    g_locobitmapRefCount++;
    *dst = *src;

    if (src->flags0 == 1 && src->palettePtr != NULL) {
        /* WIN32: FUN_00465ce0(0x200)  LINUX: malloc(0x200) */
        dst->palettePtr = (uint16_t *)loco_alloc(0x200);
        if (dst->palettePtr) {
            memcpy(dst->palettePtr, src->palettePtr, 0x80 * sizeof(uint32_t));
        }
    } else {
        dst->palettePtr = NULL;
    }

    if (src->rawpixelPtr != NULL) {
        size_t pixelBytes = (size_t)src->width * (size_t)src->height;
        dst->rawpixelPtr = (uint8_t *)loco_alloc(pixelBytes);
        if (dst->rawpixelPtr) {
            memcpy(dst->rawpixelPtr, src->rawpixelPtr, pixelBytes);
        }
    } else {
        dst->rawpixelPtr = NULL;
    }

    if (src->pDDSurface != NULL) {
        /*
         * WIN32: IDirectDraw::CreateSurface(DDSURFACEDESC {size=0x7c, caps=7,
         *        flags=0x840}), then FUN_0045ba50 to blit from src.
         * LINUX: SDL_CreateTexture + SDL_RenderCopy
         */
        dst->pDDSurface = NULL;  /* LINUX: platform-specific surface clone */
    }

    return dst;
}

/* =========================================================================
 * LOCOBITMAP_DDraw_upload  (0x0042a3d0)
 *
 * Lazy-converts LOCOBITMAP 8-bit indexed pixels to a DirectDraw SYSTEMMEMORY
 * surface.  Guards via convertedFlag; no-op if already converted.
 *
 * Steps:
 *  1. Allocate DDSURFACEDESC (0x7c bytes on stack).
 *  2. IDirectDraw::CreateSurface via vtable+0x18 (caps type=7, flags=0x840).
 *  3. FUN_0045ba50 to Lock surface, obtain descriptor.
 *  4. LOCOBITMAP_indexed_to_16bit_blit to convert pixels.
 *  5. IDirectDrawSurface::Unlock (vtable+0x80).
 *  6. Set convertedFlag=1; free palettePtr and rawpixelPtr.
 *
 * WIN32: IDirectDraw::CreateSurface, IDirectDrawSurface::Lock/Unlock
 * LINUX: SDL_CreateTexture(SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC,
 *        width, height); convert pixels locally; SDL_UpdateTexture; free bufs
 * ======================================================================= */
void LOCOBITMAP_DDraw_upload(LOCOBITMAP *self, void *pDirectDraw)
{
    (void)pDirectDraw;

    if (self->convertedFlag) {
        return;  /* already uploaded */
    }

    /*
     * WIN32: allocate DDSURFACEDESC, CreateSurface, Lock, blit, Unlock.
     * LINUX: SDL_CreateTexture equivalent + SDL_UpdateTexture.
     */
    self->pDDSurface = NULL;  /* LINUX: fill in with SDL_CreateTexture(...) */

    if (self->pDDSurface == NULL) {
        return;
    }

    /* Mark converted and free CPU-side buffers */
    self->convertedFlag = 1;

    if (self->palettePtr) {
        loco_free(self->palettePtr);
        self->palettePtr = NULL;
    }
    if (self->rawpixelPtr) {
        loco_free(self->rawpixelPtr);
        self->rawpixelPtr = NULL;
    }
}

/* =========================================================================
 * LOCOBITMAP_indexed_to_16bit_blit  (0x0042b9c0)
 *
 * Row-by-row conversion of 8-bit indexed pixels to 16-bit RGB565.
 * Reads rawpixelPtr indexed by palettePtr; writes into the locked DDraw surface.
 *
 * WIN32: pure pixel arithmetic; no direct API calls
 * LINUX: same; replace IDirectDrawSurface lPitch with SDL_Surface->pitch
 * ======================================================================= */
void LOCOBITMAP_indexed_to_16bit_blit(LOCOBITMAP *self,
                                       int dstX, int dstY,
                                       void *pDstDesc,
                                       int stride,
                                       int regionW, int regionH,
                                       int srcX, int srcY)
{
    int row, col;
    (void)pDstDesc;

    if (!self->rawpixelPtr || !self->palettePtr || !self->pDDSurface) {
        return;
    }

    for (row = 0; row < regionH; row++) {
        const uint8_t  *src = self->rawpixelPtr
                              + (srcY + row) * self->width + srcX;
        uint16_t *dst = (uint16_t *)((uint8_t *)self->pDDSurface
                                    + (dstY + row) * stride
                                    + dstX * (int)sizeof(uint16_t));
        for (col = 0; col < regionW; col++) {
            /* 8-bit index -> 16-bit RGB565 palette lookup */
            dst[col] = self->palettePtr[src[col]];
        }
    }
}

/* =========================================================================
 * PanelA_ctor  (0x00440f20)
 *
 * Sets vtable PTR_FUN_004781d0, calls base ctor FUN_00425870, then PanelA_init.
 * ======================================================================= */
PanelA *PanelA_ctor(PanelA *self)
{
    memset(self, 0, sizeof(PanelA));
    self->vtable = NULL;    /* WIN32: &PTR_FUN_004781d0 */
    /* WIN32: FUN_00425870(self)  — base class WindowBase ctor */
    PanelA_init(self);
    return self;
}

/* =========================================================================
 * PanelA_init  (0x00440fa0)
 *
 * Creates light-blue background brush (0xa8c4d8) and 7 ButtonSprite objects.
 *
 * COLORREF 0xa8c4d8 in Win32 BGR format = RGB(0xd8, 0xc4, 0xa8).
 *
 * WIN32: CreateSolidBrush(0xa8c4d8); FUN_00465ce0(0x24) x7
 * LINUX: SDL_Color { R=0xD8, G=0xC4, B=0xA8, A=0xFF }; malloc x7
 * ======================================================================= */
void PanelA_init(PanelA *self)
{
    static const int32_t spriteIds[7] = {
        RES_PANEL_A_BTN_0,  /* 0x419 */
        RES_PANEL_A_BTN_1,  /* 0x41a */
        RES_PANEL_A_BTN_2,  /* 0x417 */
        RES_PANEL_A_BTN_3,  /* 0x418 */
        RES_PANEL_A_BTN_4,  /* 0x41f */
        RES_PANEL_A_BTN_5,  /* 0x420 */
        RES_PANEL_A_BTN_6,  /* 0x421 */
    };
    int i;

    /* WIN32: self->hbrLightBlue = CreateSolidBrush(0xa8c4d8)
     * LINUX: self->hbrLightBlue = NULL;  (SDL_Color stored separately) */
    self->hbrLightBlue = NULL;

    for (i = 0; i < 7; i++) {
        ButtonSprite *pSprite =
            (ButtonSprite *)loco_alloc(sizeof(ButtonSprite));
        if (pSprite) {
            ButtonSprite_ctor(pSprite, spriteIds[i]);
        }
        self->pSprite[i] = pSprite;
    }

    g_pPanelA = self;               /* store global self-ptr DAT_00485260 */
    self->languageSlotCount = 3;
}

/* =========================================================================
 * PanelA_createWindow  (0x004412f0)
 *
 * Gets desktop rect, loads icon (resource 0x65), calls WindowBase_create.
 *
 * WIN32: GetDesktopWindow, GetClientRect, LoadIconA, WindowBase_create
 *        (RegisterClassA + CreateWindowExA + ShowWindow + UpdateWindow)
 * LINUX: SDL_GetDisplayBounds; WindowBase_create -> viewport assignment
 * ======================================================================= */
int PanelA_createWindow(PanelA *self)
{
    /* WIN32: RECT r; GetClientRect(GetDesktopWindow(), &r);
     *        self->hIcon = LoadIconA(NULL, MAKEINTRESOURCEA(RES_APP_ICON));
     *        return WindowBase_create(self, NULL, 0, 0, r.right, r.bottom);
     * LINUX: SDL_Rect bounds; SDL_GetDisplayBounds(0, &bounds);
     *        return WindowBase_create(self, NULL, bounds.x, bounds.y,
     *                                 bounds.w, bounds.h); */
    self->hIcon = NULL;
    return WindowBase_create(self, NULL, 0, 0, 0, 0);
}

/* =========================================================================
 * PanelB_ctor  (0x00408aa0)
 * ======================================================================= */
PanelB *PanelB_ctor(PanelB *self)
{
    memset(self, 0, sizeof(PanelB));
    self->vtable = NULL;    /* WIN32: &PTR_FUN_004774d0 */
    /* WIN32: FUN_00425870(self) */
    PanelB_init(self);
    return self;
}

/* =========================================================================
 * PanelB_init  (0x00408b20)
 *
 * Allocates 5 named button sprites (resource IDs 0x42a,0x42c,0x429,0x42b,0x42f)
 * and 9 city-selection sprites (contiguous IDs 0x43a..0x0442).
 *
 * WIN32: FUN_00465ce0(0x24) x14   LINUX: malloc x14
 * ======================================================================= */
void PanelB_init(PanelB *self)
{
    static const int32_t namedIds[5] = {
        RES_PANEL_B_BTN_0,  /* 0x42a */
        RES_PANEL_B_BTN_1,  /* 0x42c */
        RES_PANEL_B_BTN_2,  /* 0x429 */
        RES_PANEL_B_BTN_3,  /* 0x42b */
        RES_PANEL_B_BTN_4,  /* 0x42f */
    };
    int i;

    for (i = 0; i < 5; i++) {
        ButtonSprite *p = (ButtonSprite *)loco_alloc(sizeof(ButtonSprite));
        if (p) {
            ButtonSprite_ctor(p, namedIds[i]);
        }
        self->pNamedBtn[i] = p;
    }

    for (i = 0; i < RES_PANEL_B_CITY_COUNT; i++) {
        ButtonSprite *p = (ButtonSprite *)loco_alloc(sizeof(ButtonSprite));
        if (p) {
            ButtonSprite_ctor(p, RES_PANEL_B_CITY_FIRST + i);
        }
        self->pCitySprite[i] = p;
    }

    self->languageSlotCount = 3;
}

/* =========================================================================
 * PanelB_createWindow  (0x00408f00)
 *
 * Identical pattern to PanelA_createWindow; stores icon at hIcon (+0x1b4).
 * ======================================================================= */
int PanelB_createWindow(PanelB *self)
{
    self->hIcon = NULL;
    return WindowBase_create(self, NULL, 0, 0, 0, 0);
}

/* =========================================================================
 * WindowBase_create  (0x00425b70)
 *
 * Shared base method for all UI panel classes.
 *
 * Registers WNDCLASSA with class name from self+0x78 and shared wndproc
 * LAB_004272f0.  Creates window with style 0x87000000 (WS_POPUP|WS_VISIBLE|
 * WS_CLIPSIBLINGS|WS_CLIPCHILDREN), passing 'self' as lpParam.
 * On success:
 *   self+0x08 = new HWND
 *   self+0x0c = hwndParent
 *   self+0xac..+0xb8 = x, y, w, h
 *   Calls vtable[7] callback and FUN_00425dc0.
 *   ShowWindow(hwnd, SW_SHOW); UpdateWindow(hwnd).
 *
 * WIN32: RegisterClassA, CreateWindowExA, ShowWindow, UpdateWindow
 * LINUX: SDL_CreateWindow(SDL_WINDOW_BORDERLESS|SDL_WINDOW_FULLSCREEN_DESKTOP)
 *        OR SDL_RenderSetViewport for panel sub-regions
 * ======================================================================= */
int WindowBase_create(void *self, void *hwndParent,
                      int x, int y, int w, int h)
{
    (void)self; (void)hwndParent;
    (void)x; (void)y; (void)w; (void)h;

    /*
     * WIN32:
     *   WNDCLASSA wc = {0};
     *   wc.lpfnWndProc   = (WNDPROC)LAB_004272f0;
     *   wc.hInstance     = g_hInstance;
     *   wc.lpszClassName = (char*)self + 0x78;
     *   RegisterClassA(&wc);
     *   HWND hwnd = CreateWindowExA(0, className, NULL, 0x87000000,
     *       x, y, w, h, hwndParent, NULL, g_hInstance, self);
     *   if (!hwnd) return 0;
     *   *(HWND*)((char*)self + 0x08) = hwnd;
     *   // invoke vtable[7]; FUN_00425dc0(self);
     *   ShowWindow(hwnd, SW_SHOW); UpdateWindow(hwnd);
     *   return 1;
     *
     * LINUX:
     *   SDL_Window *win = SDL_CreateWindow("", x, y, w, h,
     *       SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN_DESKTOP);
     *   *(SDL_Window**)((char*)self + 0x08) = win;
     *   return win != NULL ? 1 : 0;
     */
    return 0;
}

/* =========================================================================
 * EditWindow_ctor  (0x004202f0)
 *
 * Zeros the 0x230-byte block, installs vtable, creates two GDI brushes,
 * stores global self-ptr at DAT_00485240.
 *
 * Brush colours:
 *   0x5252e7 = BGR(0x52,0x52,0xe7) = RGB(0xe7,0x52,0x52) blue-purple solid
 *   0x000a5c0a = dark olive-green hatched (HS_DIAGCROSS)
 *
 * WIN32: CreateSolidBrush(0x5252e7), CreateHatchBrush(HS_DIAGCROSS,0x000a5c0a)
 * LINUX: no GDI; store SDL_Color values; hbrSolid/hbrHatch = NULL
 * ======================================================================= */
EditWindow *EditWindow_ctor(EditWindow *self)
{
    memset(self, 0, sizeof(EditWindow));
    self->vtable            = NULL;             /* WIN32: &PTR_FUN_004779f8 */
    self->dialogState       = UI_STATE_INITIAL;
    self->prevState         = 0;
    self->spritesInitialized = 0;
    self->pChildDialog      = NULL;
    self->pPanelA           = NULL;
    self->pPanelB           = NULL;
    self->hwndEdit          = NULL;
    self->savedEditWndProc      = NULL;
    self->pMainSurface      = NULL;
    self->hbrSolid          = NULL;  /* WIN32: CreateSolidBrush(0x5252e7)    */
    self->hbrHatch          = NULL;  /* WIN32: CreateHatchBrush(5, 0xa5c0a)  */
    g_pEditWindow = self;            /* DAT_00485240 */
    return self;
}

/* =========================================================================
 * EditWindow_dtor  (0x004203a0)
 *
 * MSVC scalar destructor pattern: call cleanup body then optionally free.
 * flags & 1 -> free heap block.
 * ======================================================================= */
void EditWindow_dtor(EditWindow *self, int flags)
{
    EditWindow_cleanup(self);
    if (flags & 1) {
        loco_free(self);    /* WIN32: FUN_00465cd0(self) */
    }
}

/* =========================================================================
 * EditWindow_cleanup  (0x004203c0)
 *
 * Shared cleanup body used by dtor and destroy.
 *   1. If pChildDialog: restore wndproc (SetWindowLongA GWL_WNDPROC),
 *      call FUN_004544a0 to destroy it, clear pointer.
 *   2. If spritesInitialized: MainMenu_freeSprites.
 *   3. DeleteObject on both GDI brushes.
 *   4. FUN_00425910 (base class cleanup).
 *
 * WIN32: SetWindowLongA, DeleteObject
 * LINUX: no-op for GDI; SDL_StopTextInput / SDL_DestroyTexture as needed
 * ======================================================================= */
void EditWindow_cleanup(EditWindow *self)
{
    if (self->pChildDialog) {
        /* WIN32: SetWindowLongA(hwndEdit, GWL_WNDPROC, (LONG)savedWndProc)
         *        FUN_004544a0(pChildDialog)  // CMciVideoPlayer_Destruct
         * LINUX: GStreamer / libVLC stop */
        self->pChildDialog = NULL;
    }

    if (self->spritesInitialized) {
        MainMenu_freeSprites(self);
    }

    /* WIN32: DeleteObject(self->hbrSolid); DeleteObject(self->hbrHatch) */
    self->hbrSolid = NULL;
    self->hbrHatch = NULL;

    /* WIN32: FUN_00425910(self)  — base class cleanup */
}

/* =========================================================================
 * EditWindow_WM_INITDIALOG  (0x004204d0)
 *
 * Handles WM_INITDIALOG.  Steps:
 *  1. GetClientRect(GetDesktopWindow()) -> fullscreen rect.
 *  2. LoadIconA -> hAppIcon (resource 0x65).
 *  3. WindowBase_create -> fullscreen HWND.
 *  4. Alloc PanelA (0x1e4 b): PanelA_ctor + PanelA_createWindow -> pPanelA.
 *     Additional init via FUN_004412f0 (= PanelA_createWindow).
 *  5. Alloc PanelB (0x260 b): PanelB_ctor + PanelB_createWindow -> pPanelB.
 *  6. CreateWindowExA(class DAT_0047e464, WS_CHILD|ES_AUTOHSCROLL|0x40000000,
 *     ID 0x411) -> hwndEdit.
 *  7. EM_SETLIMITTEXT(hwndEdit, 11).
 *  8. Set font (WM_SETFONT).
 *  9. Subclass: savedWndProc = GetWindowLongA(GWL_WNDPROC);
 *               SetWindowLongA(GWL_WNDPROC, 0x420b20).
 * 10. SetFocus(hwndEdit).
 *
 * WIN32: GetDesktopWindow, GetClientRect, LoadIconA, CreateWindowExA,
 *        SendMessageA, GetWindowLongA, SetWindowLongA, SetFocus
 * LINUX: SDL_GetDisplayBounds; SDL_StartTextInput; panel viewports
 * ======================================================================= */
int EditWindow_WM_INITDIALOG(EditWindow *self)
{
    /* WIN32: GetClientRect(GetDesktopWindow(), &desktopRect) */
    /* LINUX: SDL_GetDisplayBounds(0, &sdlBounds) */

    /* Load application icon */
    self->hAppIcon = NULL;  /* WIN32: LoadIconA(hInst, MAKEINTRESOURCEA(0x65)) */

    /* Create fullscreen base window */
    WindowBase_create(self, NULL, 0, 0, 0, 0);

    /* Allocate and construct PanelA */
    {
        PanelA *pA = (PanelA *)loco_alloc(sizeof(PanelA));
        if (pA) {
            PanelA_ctor(pA);
            PanelA_createWindow(pA);
        }
        self->pPanelA = pA;
    }

    /* Allocate and construct PanelB */
    {
        PanelB *pB = (PanelB *)loco_alloc(sizeof(PanelB));
        if (pB) {
            PanelB_ctor(pB);
            PanelB_createWindow(pB);
        }
        self->pPanelB = pB;
    }

    /*
     * Create child edit control.
     * WIN32: self->hwndEdit = CreateWindowExA(0, (LPCSTR)DAT_0047e464, NULL,
     *            WS_CHILD|ES_AUTOHSCROLL|0x40000000, x,y,w,h,
     *            parentHWND, (HMENU)EDIT_CONTROL_ID, hInst, NULL);
     *        SendMessageA(hwndEdit, EM_SETLIMITTEXT, EDIT_CONTROL_MAX_CHARS, 0);
     *        // WM_SETFONT
     *        savedWndProc = (WNDPROC)GetWindowLongA(hwndEdit, GWL_WNDPROC);
     *        SetWindowLongA(hwndEdit, GWL_WNDPROC, (LONG)0x420b20);
     *        SetFocus(hwndEdit);
     * LINUX: SDL_StartTextInput(); SDL_SetTextInputRect(&textRect)
     */
    self->hwndEdit     = NULL;
    self->savedEditWndProc = NULL;

    return 1;
}

/* =========================================================================
 * EditWindow_activate  (0x004206b0)
 *
 * Activates name-entry UI:
 *   1. Reset flags.
 *   2. MainMenu_loadSprites.
 *   3. FUN_00425f20 (layout recalc).
 *   4. Lobby_create(g_pDDrawRenderer).
 *   5. while(ShowCursor(FALSE)>=0){} — hide OS cursor.
 *   6. SetFocus + WM_SETTEXT (player name from DAT_004aa4a8+6) + EM_SETSEL 0,-1.
 *   7. If multiplayer: LobbyState_machine(g_pNpcMgr,1) + FUN_0043d2b0.
 *      Else: EditWindow_setState(self, UI_STATE_SHOW_PANELS).
 *   8. Check password DLL (resource 0x5015).
 *
 * WIN32: ShowCursor, SetFocus, SendMessageA (WM_SETTEXT, EM_SETSEL)
 * LINUX: SDL_ShowCursor(SDL_DISABLE); SDL_StartTextInput; pre-fill text buf
 * ======================================================================= */
void EditWindow_activate(EditWindow *self)
{
    self->flag0 = 0;
    self->flag1 = 0;

    MainMenu_loadSprites(self);
    /* FUN_00425f20(self) — recalc layout */
    Lobby_create(g_pDDrawRenderer);

    /* WIN32: while (ShowCursor(FALSE) >= 0) {}
     * LINUX: SDL_ShowCursor(SDL_DISABLE) */

    /* WIN32: SetFocus(hwndEdit);
     *        SendMessageA(hwndEdit, WM_SETTEXT, 0, playerName);
     *        SendMessageA(hwndEdit, EM_SETSEL, 0, -1);
     * LINUX: SDL_StartTextInput(); populate text buffer */

    {
        /* multiplayer flag at g_pConfigMgr+7 */
        uint8_t mpActive = g_pConfigMgr
                           ? *((uint8_t *)g_pConfigMgr + 7)
                           : 0;
        if (mpActive) {
            LobbyState_machine(g_pNpcMgr, 1);
            /* WIN32: FUN_0043d2b0(DAT_004fd3ac, 1) */
        } else {
            EditWindow_setState(self, UI_STATE_SHOW_PANELS);
        }
    }
}

/* =========================================================================
 * EditWindow_setState  (0x004208f0)
 *
 * Central state machine.  See UI_STATE_* constants and architecture comment.
 *
 * WIN32: PlaySoundA, ShowWindow, SetWindowLongA (state 7)
 * LINUX: Mix_HaltMusic / Mix_PlayMusic; SDL_HideWindow / SDL_ShowWindow
 * ======================================================================= */
void EditWindow_setState(EditWindow *self, int newState)
{
    int prevState = self->dialogState;
    self->dialogState = newState;

    switch (newState) {

    case UI_STATE_HIDDEN:   /* 1 */
        /* WIN32: PlaySoundA(NULL, NULL, SND_ASYNC)  LINUX: Mix_HaltMusic() */
        /* WIN32: ShowWindow(hwndEdit, SW_HIDE)       LINUX: hide text field  */
        break;

    case UI_STATE_DEACTIVATED:  /* 2 */
        /* hide edit; stop PanelA via vtable[2] */
        if (self->pPanelA && self->pPanelA->vtable) {
            /* (*(self->pPanelA->vtable[2]))(self->pPanelA) */
        }
        /* stop PanelB if prior state was 4 or 5 */
        if ((prevState == UI_STATE_PANEL_B_SINGLE ||
             prevState == UI_STATE_PANEL_B_MULTI) &&
            self->pPanelB && self->pPanelB->vtable) {
            /* (*(self->pPanelB->vtable[2]))(self->pPanelB) */
        }
        break;

    case UI_STATE_SHOW_PANELS:  /* 3 */
        /* show PanelA via vtable[1] */
        if (self->pPanelA && self->pPanelA->vtable) {
            /* (*(self->pPanelA->vtable[1]))(self->pPanelA) */
        }
        /* resolve follow-up state from DAT_004fd3a8+0x18 */
        {
            uint8_t mpLobby = g_pConfigMgr
                              ? *((uint8_t *)g_pConfigMgr + 0x18)
                              : 0;
            self->dialogState = mpLobby
                                ? UI_STATE_PANEL_B_MULTI
                                : UI_STATE_PANEL_B_SINGLE;
        }
        /* show PanelB via vtable[2] */
        if (self->pPanelB && self->pPanelB->vtable) {
            /* (*(self->pPanelB->vtable[2]))(self->pPanelB) */
        }
        break;

    case UI_STATE_PANEL_B_SINGLE:  /* 4 */
    case UI_STATE_PANEL_B_MULTI:   /* 5 */
        /* hide edit; show PanelB via vtable[2] */
        if (self->pPanelB && self->pPanelB->vtable) {
            /* (*(self->pPanelB->vtable[2]))(self->pPanelB) */
        }
        break;

    case UI_STATE_SHUTDOWN:  /* 6 */
        /* hide both panels */
        /* conditionally play music via LobbyState_machine */
        {
            uint8_t mpActive = g_pConfigMgr
                               ? *((uint8_t *)g_pConfigMgr + 7)
                               : 0;
            if (mpActive) {
                LobbyState_machine(g_pNpcMgr, 1);
            }
        }
        /* enter gameplay */
        GameState_machine(1);
        break;

    case UI_STATE_CLOSE_CHILD:  /* 7 */
        /* restore subclassed wndproc */
        if (self->hwndEdit && self->savedEditWndProc) {
            /* WIN32: SetWindowLongA(hwndEdit, GWL_WNDPROC, (LONG)savedWndProc)
             * LINUX: no-op */
        }
        /* destroy child dialog */
        if (self->pChildDialog) {
            /* WIN32: FUN_004544a0(pChildDialog) */
            self->pChildDialog = NULL;
        }
        /* play svideo/music.wav if prior state was 0 or 1 */
        if (prevState == UI_STATE_INITIAL || prevState == UI_STATE_HIDDEN) {
            /* WIN32: PlaySoundA("svideo\\music.wav", NULL, SND_ASYNC|SND_FILENAME)
             * LINUX: Mix_LoadMUS(path); Mix_PlayMusic(music, -1) */
            Audio_init();
        }
        /* deactivate panels */
        if (self->pPanelA && self->pPanelA->vtable) {
            /* stop vtable call */
        }
        if (self->pPanelB && self->pPanelB->vtable) {
            /* stop vtable call */
        }
        break;

    default:
        break;
    }
}

/* =========================================================================
 * EditWindow_destroy  (0x00420860)
 *
 * Destroys EditWindow: calls base dtor FUN_00425990, clears flag1, destroys
 * child dialog, calls EditWindow_setState(1), releases sprites, restores
 * focus to main game window (DAT_004aa4a0+8), InvalidateRect.
 *
 * WIN32: SetWindowLongA, DestroyWindow, SetFocus, InvalidateRect
 * LINUX: SDL_RenderPresent to force repaint
 * ======================================================================= */
void EditWindow_destroy(EditWindow *self)
{
    /* WIN32: FUN_00425990(self) — base destructor */
    self->flag1 = 0;

    if (self->pChildDialog) {
        if (self->hwndEdit && self->savedEditWndProc) {
            /* WIN32: SetWindowLongA(hwndEdit, GWL_WNDPROC, ...) */
        }
        /* WIN32: DestroyWindow or FUN_004544a0 */
        self->pChildDialog = NULL;
    }

    EditWindow_setState(self, UI_STATE_HIDDEN);

    if (self->spritesInitialized) {
        MainMenu_freeSprites(self);
    }

    /* WIN32: SetFocus(mainHwnd); InvalidateRect(mainHwnd, NULL, FALSE)
     * LINUX: SDL_RenderPresent(g_pRenderer) */
}

/* =========================================================================
 * MainMenu_loadSprites  (0x00421500)
 *
 * Loads 12 main-menu button sprites into menuSprite[].
 * Resource IDs (skipping 0x40d):
 *   0x403, 0x404, 0x405, 0x406, 0x407, 0x408,
 *   0x409, 0x40a, 0x40b, 0x40c, 0x40e, 0x40f
 *
 * For each ID:
 *   pObj    = ResourceManager_lookup(pResMgr, id)
 *   pSurface = (*(pObj->vtable[1]))(pObj)     // create surface handle
 * Then MainMenu_layoutSprites.  Sets spritesInitialized=1.
 *
 * WIN32: virtual dispatch; IDirectDrawSurface creation
 * LINUX: SDL_CreateTexture per sprite
 * ======================================================================= */
void MainMenu_loadSprites(EditWindow *self)
{
    static const int spriteIds[RES_BTN_MAINMENU_COUNT] = {
        0x403, 0x404, 0x405, 0x406, 0x407, 0x408,
        0x409, 0x40a, 0x40b, 0x40c, 0x40e, 0x40f,
        /* 0x40d deliberately skipped */
    };
    int i;
    void *pResMgr = NULL;   /* LINUX: &g_ResMgr from resources.h */

    if (self->spritesInitialized) {
        return;
    }

    for (i = 0; i < RES_BTN_MAINMENU_COUNT; i++) {
        void *pObj  = ResourceManager_lookup(pResMgr, spriteIds[i]);
        void *pSurf = NULL;

        if (pObj) {
            /* WIN32: pSurf = (*(((void***)pObj)[0][1]))(pObj) */
            /* LINUX: SDL_CreateTexture wrapper */
        }

        self->menuSprite[i].pResource = pObj;
        self->menuSprite[i].pSurface  = pSurf;
    }

    MainMenu_layoutSprites(self);
    self->spritesInitialized = 1;
}

/* =========================================================================
 * MainMenu_layoutSprites  (0x004216f0)
 *
 * Creates the 1280x1024 main DirectDraw surface (FUN_0042a850) and blits
 * 5 background sprites at hardcoded positions.  Each resource is released
 * via vtable[2] after blitting.
 *
 * Blit table:
 *   RES_LAYOUT_BACKGROUND (0x413) -> (0,     0)
 *   RES_LAYOUT_PANEL_1    (0x444) -> (0xf4,  0x1d6)
 *   RES_LAYOUT_PANEL_2    (0x445) -> (0x204, 0xf9)
 *   RES_LAYOUT_PANEL_3    (0x446) -> (0x11a, 0xf0)
 *   RES_LAYOUT_PANEL_4    (0x443) -> (0x20b, 0x2a8)
 *
 * WIN32: FUN_0042a850(renderer, 1280, 1024) -> IDirectDrawSurface*
 * LINUX: SDL_CreateTexture(SDL_TEXTUREACCESS_TARGET, 1280, 1024);
 *        SDL_SetRenderTarget(renderer, tex); blit each sprite; reset target
 * ======================================================================= */
void MainMenu_layoutSprites(EditWindow *self)
{
    static const struct { int id, x, y; } layout[5] = {
        { RES_LAYOUT_BACKGROUND, 0,      0      },
        { RES_LAYOUT_PANEL_1,    0xf4,   0x1d6  },
        { RES_LAYOUT_PANEL_2,    0x204,  0xf9   },
        { RES_LAYOUT_PANEL_3,    0x11a,  0xf0   },
        { RES_LAYOUT_PANEL_4,    0x20b,  0x2a8  },
    };
    int i;
    void *pResMgr = NULL;   /* LINUX: &g_ResMgr */

    /* Create 1280x1024 main surface.
     * WIN32: self->pMainSurface = FUN_0042a850(g_pDDrawRenderer, 1280, 1024)
     * LINUX: SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
     *            SDL_TEXTUREACCESS_TARGET, 1280, 1024) */
    self->pMainSurface = NULL;

    for (i = 0; i < 5; i++) {
        void *pRes = ResourceManager_lookup(pResMgr, layout[i].id);
        if (pRes) {
            Surface_blitSprite(self->pMainSurface, pRes,
                               layout[i].x, layout[i].y);
            /* WIN32: (*(((void***)pRes)[0][2]))(pRes, 0)  // vtable[2] release */
        }
    }
}

/* =========================================================================
 * MainMenu_recalcLayout  (0x00421200)
 *
 * Recomputes 8 button hit-rects in screen space.  If spritesInitialized:
 * computes centering delta from (surface size - screen size) / 2, then
 * SetRect + OffsetRect for each rect stored at +0x13c, +0x14c, +0xfc,
 * +0x10c, +0x11c, +0x12c, +0x17c, +0x15c.
 *
 * Sample hardcoded base positions:
 *   Slot 0: (0x387, 0x2a5)   Slot 1: (0x18b, 0x2a5)   Slot 2: (0x212, 0x1ea)
 *
 * WIN32: SetRect, OffsetRect (GDI32)
 * LINUX: plain LOCO_RECT assignment and offset arithmetic; no GDI
 * ======================================================================= */
void MainMenu_recalcLayout(EditWindow *self)
{
    /*
     * Recovered verbatim from FUN_00421200.  The menu is authored in a fixed
     * 1280x1024 "design space" (MAINMENU_SURFACE_*).  Each button is placed at
     * its design coordinate, sized from its OWN sprite's pixel dimensions
     * (sprite header: width at +0x14, height at +0x16), then the whole layout
     * is shifted by (-centerOffsetX, -centerOffsetY) so the design canvas is
     * centered on the player's actual screen (g_screenWidth/g_screenHeight,
     * 0x4851d8 / 0x485214).  Because every rect uses the SAME offset, the menu
     * moves as a rigid body across resolutions.
     *
     * Helper that mirrors the WIN32 SetRect+(read sprite size)+OffsetRect idiom:
     *   left/top  = design coordinate
     *   right     = left + sprite.width
     *   bottom    = top  + sprite.height
     *   then OffsetRect(-dx, -dy)
     */
    WindowBase_recalc(self);                       /* WIN32: FUN_00425d30(self) */

    if (!self->spritesInitialized) {
        return;
    }

    /* Centering deltas — half the difference between the composite surface size
     * and the real screen size.  Surface w/h are LOCOBITMAP fields at +8/+0xc. */
    self->centerOffsetX =
        ((int)((LOCOBITMAP *)self->pMainSurface)->width  - g_screenWidth ) >> 1;  /* +0x16c */
    self->centerOffsetY =
        ((int)((LOCOBITMAP *)self->pMainSurface)->height - g_screenHeight) >> 1;  /* +0x170 */
    self->originX = g_screenWidth  + self->centerOffsetX;                         /* +0x174 */
    self->originY = g_screenHeight + self->centerOffsetY;                         /* +0x178 */

    /* menuSprite[] index map: [0]=0x407 [1]=0x408 [2]=0x409 [3]=0x40a
     *                         [4]=0x403(Go) [5]=0x404 [6]=0x405(Back) [7]=0x406
     *                         [8]=0x40b [9]=0x40c [10]=0x40e [11]=0x40f          */
    MenuButton_placeRect(self, &self->rcBtnGo,   0x387, 0x2a5, 4);  /* sprite 0x403 */
    MenuButton_placeRect(self, &self->rcBtnBack, 0x18b, 0x2a5, 6);  /* sprite 0x405 */
    MenuButton_placeRect(self, &self->rcBtn407,  0x212, 0x1ea, 0);  /* sprite 0x407 */
    MenuButton_placeRect(self, &self->rcBtn409,  0x2c9, 0x1ea, 2);  /* sprite 0x409 */
    MenuButton_placeRect(self, &self->rcBtn40b,  0x387, 0x1bd, 8);  /* sprite 0x40b */
    MenuButton_placeRect(self, &self->rcBtn40e,  0x387, 0x231, 10); /* sprite 0x40e */

    /* Two fixed (non-sprite-sized) regions, then centered the same way. */
    LOCO_RECT_set(&self->rcFixedA, 300,   0xac,  0x3d4, 0x354);
    LOCO_RECT_offset(&self->rcFixedA, -self->centerOffsetX, -self->centerOffsetY);
    LOCO_RECT_set(&self->rcFixedB, 0x232, 0x2cc, 0x34d, 0x2ed);
    LOCO_RECT_offset(&self->rcFixedB, -self->centerOffsetX, -self->centerOffsetY);
}

/* =========================================================================
 * MenuButton_placeRect  (helper extracted from FUN_00421200)
 *
 * Places one button hit-rect: top-left at the design coordinate (x,y),
 * sizes it from menuSprite[spriteIdx]'s pixel dimensions, then offsets it by
 * the centering delta so it maps into screen space.
 *
 * WIN32 original (per button):
 *   SetRect(rc, x, y, 0, 0);
 *   rc->right  = rc->left + *(uint16_t*)(sprite->pResource + 0x14);
 *   rc->bottom = rc->top  + *(uint16_t*)(sprite->pResource + 0x16);
 *   OffsetRect(rc, -centerOffsetX, -centerOffsetY);
 * ======================================================================= */
static void MenuButton_placeRect(EditWindow *self, LOCO_RECT *rc,
                                 int x, int y, int spriteIdx)
{
    const uint8_t *spr = (const uint8_t *)self->menuSprite[spriteIdx].pResource;
    uint16_t w = spr ? *(const uint16_t *)(spr + 0x14) : 0;  /* sprite header width  */
    uint16_t h = spr ? *(const uint16_t *)(spr + 0x16) : 0;  /* sprite header height */

    rc->left   = x;
    rc->top    = y;
    rc->right  = x + w;
    rc->bottom = y + h;
    LOCO_RECT_offset(rc, -self->centerOffsetX, -self->centerOffsetY);
}

/* =========================================================================
 * MainMenu_freeSprites  (0x00421ae0)
 *
 * Releases all 12 sprite slots via vtable[0](1) then nulls them.
 * Destroys pMainSurface via vtable[0](1).  Clears spritesInitialized.
 *
 * WIN32: virtual destructor dispatch; IDirectDrawSurface::Release
 * LINUX: SDL_DestroyTexture
 * ======================================================================= */
void MainMenu_freeSprites(EditWindow *self)
{
    int i;

    for (i = 0; i < RES_BTN_MAINMENU_COUNT; i++) {
        if (self->menuSprite[i].pResource) {
            /* WIN32: (*(((void***)pResource)[0][0]))(pResource, 1)
             * LINUX: SDL_DestroyTexture(pSurface); free(pResource) */
            self->menuSprite[i].pResource = NULL;
            self->menuSprite[i].pSurface  = NULL;
        }
    }

    if (self->pMainSurface) {
        /* WIN32: vtable[0](pMainSurface, 1)
         * LINUX: SDL_DestroyTexture((SDL_Texture*)pMainSurface) */
        self->pMainSurface = NULL;
    }

    self->spritesInitialized = 0;
}

/* =========================================================================
 * ResourceManager_lookup  (0x00446ea0)
 *
 * Central resource factory for all UI widgets.
 * For IDs in [LOCALIZED_STR_ID_FIRST, LOCALIZED_STR_ID_LAST] (100-500):
 *   Reads language code from CResourceMgr+0x241b8.
 *   Applies LANG_STRTBL_* offset; calls LoadStringA.
 *   Falls back to English if LoadStringA returns 0 for the translated ID.
 * Calls FUN_00446840 (RESMGR_LoadResource) and returns cached pointer.
 *
 * WIN32: GetModuleHandleA(NULL), LoadStringA
 * LINUX: g_StringTable_Lookup(translatedId, buf, 256) from resources.c
 * ======================================================================= */
void *ResourceManager_lookup(void *pResMgr, int resourceId)
{
    (void)pResMgr;
    (void)resourceId;
    /* LINUX: RESMGR_GetResource(&g_ResMgr, resourceId) from resources.c */
    return NULL;
}

/* =========================================================================
 * Surface_blitSprite  (0x0042b960)
 *
 * Extracts the DDraw surface pointer at pTargetSurface+0x1c and calls
 * FUN_0042b050 to perform the blit.
 *
 * WIN32: IDirectDrawSurface::BltFast (via FUN_0042b050)
 * LINUX: SDL_RenderCopy(renderer, (SDL_Texture*)pSprite, NULL, &dstRect)
 * ======================================================================= */
void Surface_blitSprite(void *pTargetSurface, void *pSprite, int x, int y)
{
    (void)pTargetSurface; (void)pSprite; (void)x; (void)y;
    /* WIN32: void *ddSurf = *(void**)((char*)pTargetSurface + 0x1c);
     *        FUN_0042b050(ddSurf, pSprite, x, y);
     * LINUX: SDL_Rect dst = {x, y, spriteW, spriteH};
     *        SDL_RenderCopy(g_pRenderer, (SDL_Texture*)pSprite, NULL, &dst); */
}

/* =========================================================================
 * Lobby_create  (0x00422820)
 *
 * Creates the SessionScheduler (0x38 bytes) and NetworkManager (0x41c bytes)
 * if not already present.  Guards via DAT_004fd3a4 non-null check.
 *
 * SessionScheduler (FUN_00438bc0):
 *   vtable PTR_FUN_004781c4; enumerates up to 4 DirectPlay sessions
 *   (FUN_0045eab0 for each slot 0-3).  Builds session list at g_pConfigMgr+0x10.
 *
 * NetworkManager (FUN_00461610):
 *   FUN_00461790 starts dispatch thread with callback LAB_00439240.
 *   On failure: vtable[5] shutdown.
 *
 * WIN32: DirectPlay session enumeration; CreateThread (indirect)
 * LINUX: BSD sockets; pthread_create
 * ======================================================================= */
void Lobby_create(void *pDDrawRenderer)
{
    static int created = 0;
    if (created) { return; }
    (void)pDDrawRenderer;

    /* Set slot count from multiplayer flag */
    if (g_pConfigMgr) {
        uint8_t mpActive = *((uint8_t *)g_pConfigMgr + 7);
        *((int *)((char *)g_pConfigMgr + 0xc)) =
            mpActive ? MP_SLOT_COUNT_ACTIVE : MP_SLOT_COUNT_LOBBY;
    }

    /* WIN32: allocate + FUN_00438bc0(SessionScheduler, 0x38 bytes)
     *        allocate + FUN_00461610(NetworkManager, 0x41c bytes)
     *        FUN_00461790 to start dispatch thread
     * LINUX: malloc; pthread_create(&tid, NULL, dispatchThread, pNet) */
    created = 1;
}

/* =========================================================================
 * MultiplayerLobby_show  (0x00448350)
 * ======================================================================= */
void MultiplayerLobby_show(void)
{
    /* set multiplayer active flag at g_pConfigMgr+7 */
    if (g_pConfigMgr) {
        *((uint8_t *)g_pConfigMgr + 7) = 1;
    }

    Lobby_create(g_pDDrawRenderer);

    /* set slot count to 50 */
    if (g_pConfigMgr) {
        *((int *)((char *)g_pConfigMgr + 0xc)) = MP_SLOT_COUNT_ACTIVE;
    }

    LobbyState_machine(g_pNpcMgr, 1);

    /* WIN32: FUN_004616c0(DAT_004fd398, -1) — raise thread priority
     * LINUX: pthread_setschedparam or nice(-1) */

    Audio_init();
}

/* =========================================================================
 * LobbyState_machine  (0x0043d2b0)
 * ======================================================================= */
void LobbyState_machine(void *pNpcMgr, int state)
{
    (void)pNpcMgr;

    switch (state) {
    case 1:
        /* WIN32: *(int*)((char*)pNpcMgr + 0x7f0) = MP_POLL_TIMER_MS_IDLE; */
        break;

    case 2:
        /* Scan session list at pNpcMgr+0x518 (9 entries, stride 0x4c).
         * Find entry matching *(int*)((char*)pNpcMgr + 0x7d4).
         * Store matched index -> +0x7d0; pointer -> +0x7cc.
         * Set fast poll timer: +0x7f0 = MP_POLL_TIMER_MS_MATCH.
         * Allocate type-6 message (payload 0x3fa); post to scheduler. */
        break;

    default:
        break;
    }
}

/* =========================================================================
 * GameState_machine  (0x00408130)
 *
 * UI-relevant states:
 *   1 -> GameStart_state1: 150ms timer; enable audio; render UI overlay
 *   2 -> deactivate DDraw renderer; activate intro/title
 *   3 -> GameState3_transition: cleanup per prior state
 *  10 -> exit: SetCooperativeLevel + PostMessageA(WM_QUIT)
 *
 * WIN32: PostMessageA(WM_QUIT on state 10)
 * LINUX: SDL_PushEvent with SDL_QUIT type
 * ======================================================================= */
void GameState_machine(int newState)
{
    (void)newState;
    /* WIN32: switch (newState) { case 1: FUN_00408350(); ...
     *        case 10: PostMessageA(hwnd, WM_QUIT, 0, 0); break; }
     * LINUX: SDL_PushEvent(&(SDL_Event){.type=SDL_QUIT}) on state 10 */
}

/* =========================================================================
 * MainMenu_showAfterSetup  (0x004480c0)
 * ======================================================================= */
int MainMenu_showAfterSetup(int *pSetupPhaseCounter)
{
    (void)pSetupPhaseCounter;
    /* ScreenSaverPassword_loadDLL(ctx); */
    /* WIN32: int soundEn = INI_GetInt(g_pIniFile,"ScreenSaver","Sound",0);
     *        if (!soundEn) PlaySoundA(musicPath, NULL, SND_ASYNC|SND_FILENAME);
     * LINUX: Mix_PlayMusic(g_pBgMusic, -1) */
    return 1;
}

/* =========================================================================
 * ScreenSaverPassword_loadDLL  (0x004487f0)
 *
 * Windows-only.  Loads password.cpl; resolves GetPasswordStatus and
 * VerifyScreenSavePwd via GetProcAddress.
 * LINUX: no-op.
 * ======================================================================= */
void ScreenSaverPassword_loadDLL(ScreenSaverCtx *ctx)
{
    (void)ctx;
    /* WIN32: GetSystemDirectoryA; RegOpenKeyExA/RegQueryValueExA;
     *        SetErrorMode(SEM_FAILCRITICALERRORS); LoadLibraryA(path);
     *        GetProcAddress x2. */
}

/* =========================================================================
 * ScreenSaverDialog_init  (0x00448040)
 *
 * WIN32: CreateSolidBrush(0xa8c4d8)   COLORREF BGR -> RGB(0xd8,0xc4,0xa8)
 * LINUX: no GDI; SDL_Color { R=0xD8, G=0xC4, B=0xA8, A=0xFF }
 * ======================================================================= */
void *ScreenSaverDialog_init(ScreenSaverCtx *ctx)
{
    memset(ctx, 0, sizeof(ScreenSaverCtx));
    ctx->capacity    = 0x400;
    ctx->hwnd        = NULL;
    ctx->hbrLightBlue = NULL;   /* WIN32: CreateSolidBrush(0xa8c4d8) */
    return ctx;
}

/* =========================================================================
 * ScreenSaverDialog_destroy  (0x00448080)
 *
 * WIN32: DeleteObject(hbrLightBlue); DestroyWindow(hwnd); FreeLibrary(dll)
 * LINUX: SDL_DestroyWindow if hwnd; dlclose(hPasswordDll)
 * ======================================================================= */
void ScreenSaverDialog_destroy(ScreenSaverCtx *ctx)
{
    if (ctx->hbrLightBlue) {
        /* WIN32: DeleteObject(ctx->hbrLightBlue) */
        ctx->hbrLightBlue = NULL;
    }
    if (ctx->hwnd) {
        /* WIN32: DestroyWindow(ctx->hwnd)
         * LINUX: SDL_DestroyWindow((SDL_Window*)ctx->hwnd) */
        ctx->hwnd = NULL;
    }
    if (ctx->hPasswordDll) {
        /* WIN32: FreeLibrary(ctx->hPasswordDll)
         * LINUX: dlclose(ctx->hPasswordDll) */
        ctx->hPasswordDll = NULL;
    }
}

/* =========================================================================
 * ScreenSaverTimer_tick  (0x00448120)
 *
 * Cycles screensaver animation frames every 0x7ff (2047) type-1 ticks.
 * 3 slots at DAT_004a98b8.  Frame selection: FUN_00466150()/0xfff < 3 =>
 * frame0 (3/4 probability) else frame1 (1/4 probability).
 * Calls FUN_0044d6c0(pObj, frame) to display selected frame.
 *
 * WIN32: driven by WM_TIMER messages
 * LINUX: SDL_AddTimer callback or main-loop tick counter
 * ======================================================================= */
void ScreenSaverTimer_tick(void *pMsgParam, int tickType)
{
    static int tickCounter = 0;
    (void)pMsgParam;

    if (tickType != 1) { return; }
    if (((++tickCounter) & 0x7ff) != 0) { return; }
    tickCounter = 0;

    /* iterate 3 screensaver slots at DAT_004a98b8:
     *   for each non-null pObj:
     *     int rng = FUN_00466150() & 0xfff;
     *     uint16_t frame = (rng < (0xfff * 3 / 4))
     *                      ? slot->frame0 : slot->frame1;
     *     FUN_0044d6c0(pObj, frame);
     * LINUX: SDL_RenderCopy with frame rect offset */
}

/* =========================================================================
 * SaveFileEnum_list  (0x00448390)
 *
 * Enumerates *.sav (fileType==0) or *.scr (fileType==1) files.
 * Builds a singly linked list of SaveFileNode (0x508 bytes each).
 *
 * WIN32: wsprintfA(pattern,...); FUN_00467a20 (FindFirstFileA wrapper);
 *        FUN_00467b50 (FindNextFileA); FUN_00467c70 (FindClose)
 * LINUX: snprintf + opendir/readdir/closedir (POSIX dirent.h)
 * ======================================================================= */
SaveFileNode *SaveFileEnum_list(int fileType)
{
    SaveFileNode *head = NULL;
    (void)fileType;

#ifndef _WIN32
    /* LINUX: const char *ext = (fileType==0) ? ".sav" : ".scr";
     *        DIR *d = opendir(resourceDir);
     *        struct dirent *e;
     *        while ((e=readdir(d))) {
     *            if (suffix_matches(e->d_name, ext)) {
     *                SaveFileNode *n = malloc(sizeof(SaveFileNode));
     *                strncpy(n->filename, e->d_name, 0x503);
     *                n->next = head; head = n;
     *            }
     *        }
     *        closedir(d); */
#endif
    return head;
}

/* =========================================================================
 * ScreenSaver_layout_picker  (0x004481b0)
 *
 * Reads [ScreenSaver]/Random from INI.  If set: enumerate .scr files,
 * seed RNG from GetTickCount, pick random entry.  Else: read Layout key
 * (default 'ScrSaver.saver.sav').  Write 'ScrSaver\[name]' into outBuf.
 *
 * WIN32: GetTickCount (for RNG seed)
 * LINUX: SDL_GetTicks() or clock_gettime(CLOCK_MONOTONIC)
 * ======================================================================= */
void ScreenSaver_layout_picker(char *outBuf)
{
    if (outBuf) { outBuf[0] = '\0'; }
    /* WIN32: int isRandom = INI_GetInt(g_pIniFile,"ScreenSaver","Random",0);
     *        if (isRandom) { SaveFileNode *list = SaveFileEnum_list(1); ...
     *                        srand(GetTickCount()); idx = rand() % count; ...
     *                        wsprintfA(outBuf,"ScrSaver\\%s",node->filename); }
     *        else { INI_GetString(...,"Layout","ScrSaver.saver.sav",...);
     *               wsprintfA(outBuf,"ScrSaver\\%s",layout); }
     * LINUX: SDL_GetTicks() for seed; snprintf instead of wsprintfA */
}

/* =========================================================================
 * LocalizedString_load  (0x00447330)
 *
 * WIN32: GetModuleHandleA(NULL) + LoadStringA with language offset.
 *        Fallback: LoadStringA with original (English) ID.
 * LINUX: g_StringTable_Lookup(translatedId, outBuf, bufLen)
 *        from resources.c RESMGR_LoadLocalizedString
 * ======================================================================= */
void LocalizedString_load(void *pResMgr, unsigned int baseId,
                           char *outBuf, int bufLen)
{
    (void)pResMgr; (void)baseId;
    if (outBuf && bufLen > 0) { outBuf[0] = '\0'; }
    /* static const int offsets[] = {
     *     0, LANG_STRTBL_ENGLISH, LANG_STRTBL_DANISH, 0,
     *     LANG_STRTBL_DUTCH, LANG_STRTBL_SPANISH, LANG_STRTBL_FRENCH,
     *     LANG_STRTBL_GERMAN, LANG_STRTBL_ITALIAN, LANG_STRTBL_PORTUGUESE,
     * };
     * LINUX: g_StringTable_Lookup(baseId + offsets[langCode], outBuf, bufLen) */
}

/* =========================================================================
 * Audio_init  (0x0045b7e0)
 *
 * Allocates 0xb8-byte audio manager; opens DirectSound device; reads volume
 * levels from LOCO.INI [Sound] (defaults 75/75/78).
 *
 * WIN32: FUN_00412bd0 (ctor), FUN_00412c50 (open device), FUN_00413630 (set vols)
 * LINUX: Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
 *        Mix_Volume(-1, volLevel); Mix_VolumeMusic(volHigh)
 * ======================================================================= */
int Audio_init(void)
{
    /* volLow = INI_GetInt(g_pIniFile, "Sound", "VolumeLow",  0x4b); // 75 */
    /* volMed = INI_GetInt(g_pIniFile, "Sound", "VolumeMed",  0x4b); // 75 */
    /* volHigh= INI_GetInt(g_pIniFile, "Sound", "VolumeHigh", 0x4e); // 78 */
    /* WIN32: FUN_00413630(pAudio, volLow, volMed, volHigh, volHigh)  */
    /* LINUX: Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)        */
    return 1;
}

/*===========================================================================
 * SECTION 3:  INPUT DISPATCH, BUTTON HIT-TESTING & RENDERING  (recovered)
 *
 * These functions answer "how do the menu buttons work".  They were missing
 * from the earlier decompilation pass; recovered here from the binary and
 * mirrored into the Ghidra project (loco.exe.rep).
 *
 * Message flow:
 *   Win32 msg --> CGWnd_StaticWndProc (0x4272f0) recovers 'this' from the HWND
 *             --> this->vtable[10] CGWnd_dispatchMessage (0x426140)
 *             --> specific virtual: OnLButtonDown / OnKeyDown / OnMouseMove ...
 *
 * A "button" is the triple (normal sprite, pressed sprite, hit-rect).  There is
 * no button object: the controller owns flat sprite[] and rect fields and drives
 * them directly.  Pressed sprite id = normal id + 1 (e.g. Go 0x403 / 0x404).
 *=========================================================================*/

/* Win32 message ids used below (avoid pulling in <windows.h> for the port). */
#define LOCO_WM_KEYDOWN      0x0100
#define LOCO_WM_LBUTTONDOWN  0x0201
#define LOCO_WM_MOUSEMOVE    0x0200
#define LOCO_VK_RETURN       0x0d
#define LOCO_VK_ESCAPE       0x1b
#define MAINMENU_PRESS_MS    0x96    /* Sleep(150) so a press is visible */

/* menuSprite[] index constants (see MainMenu_recalcLayout map). */
enum {
    SPR_407 = 0, SPR_408, SPR_409, SPR_40A,
    SPR_GO,  SPR_GO_PRESSED,            /* 0x403 / 0x404 */
    SPR_BACK, SPR_BACK_PRESSED,         /* 0x405 / 0x406 */
    SPR_40B, SPR_40C, SPR_40E, SPR_40F
};

/* Forward declarations for this section (definitions follow). */
intptr_t CGWnd_dispatchMessage(EditWindow *self, void *hwnd, unsigned int msg,
                               uintptr_t wParam, intptr_t lParam);
intptr_t EditWnd_onLButtonDown(EditWindow *self, void *hwnd, unsigned int msg,
                               uintptr_t wParam, intptr_t lParam);
intptr_t EditWnd_onMouseMove_hitTest(EditWindow *self, void *hwnd, unsigned int msg,
                                     uintptr_t wParam, intptr_t lParam);
intptr_t EditWnd_onKeyDown(EditWindow *self, void *hwnd, unsigned int msg,
                           uintptr_t wParam, intptr_t lParam);
void     EditWnd_submitPlayerName(EditWindow *self);
static void MenuButton_drawPressed(EditWindow *self, const LOCO_RECT *rc, int spriteIdx);

/* =========================================================================
 * CGWnd_StaticWndProc  (0x004272f0)
 *
 * The single WNDPROC registered for EVERY CGWnd window class
 * (RegisterClassA in CGWnd_createWindow / WindowBase_create, 0x425b70).
 * It recovers the C++ object stored in GWL_USERDATA and forwards to the
 * object's virtual message dispatcher (vtable[10]).  On the first message it
 * instead latches 'this' from the CREATESTRUCT (classic MFC-style thunk).
 *
 * WIN32: GetWindowLongA(hwnd, GWL_USERDATA=-21); SetWindowLongA(...); GetParent;
 *        DefWindowProcA
 * LINUX: SDL has no per-window WNDPROC; route SDL_Event to the matching
 *        CGWnd* and call its dispatch() directly.
 * ======================================================================= */
intptr_t CGWnd_StaticWndProc(void *hwnd, unsigned int msg,
                             uintptr_t wParam, intptr_t lParam)
{
    EditWindow *self = /* WIN32: GetWindowLongA(hwnd, GWL_USERDATA) */ g_pEditWindow;

    if (self != NULL) {
        /* vtable[10] == CGWnd_dispatchMessage (0x426140) */
        return CGWnd_dispatchMessage(self, hwnd, msg, wParam, lParam);
    }
    /* First message: WIN32 stores lpCreateParams into GWL_USERDATA here. */
    /* WIN32: hParent = GetParent(hwnd); ... DefWindowProcA(hwnd,msg,wParam,lParam) */
    return 0;
}

/* =========================================================================
 * CGWnd_dispatchMessage  (0x00426140, vtable index 10)
 *
 * Translates each Win32 message into a call to a specific virtual slot, and
 * manages mouse capture + cursor visibility for the borderless fullscreen
 * window.  Only the slots the main menu actually overrides are shown; the
 * full table is documented in docs/MAIN_MENU_ARCHITECTURE.md §2.3.
 *
 *   WM_LBUTTONDOWN 0x201 -> vtable+0x38 (idx14) EditWnd_onLButtonDown
 *   WM_MOUSEMOVE   0x200 -> vtable+0x50 (idx20) EditWnd_onMouseMove_hitTest
 *   WM_KEYDOWN     0x100 -> vtable+0x54 (idx21) EditWnd_onKeyDown
 *
 * WIN32: SetCapture/ReleaseCapture/ShowCursor/WindowFromPoint
 * ======================================================================= */
intptr_t CGWnd_dispatchMessage(EditWindow *self, void *hwnd, unsigned int msg,
                               uintptr_t wParam, intptr_t lParam)
{
    switch (msg) {
    case LOCO_WM_LBUTTONDOWN:
        /* WIN32: SetForegroundWindow(self->hwnd) then vtable+0x38 */
        return EditWnd_onLButtonDown(self, hwnd, msg, wParam, lParam);
    case LOCO_WM_MOUSEMOVE:
        return EditWnd_onMouseMove_hitTest(self, hwnd, msg, wParam, lParam);
    case LOCO_WM_KEYDOWN:
        return EditWnd_onKeyDown(self, hwnd, msg, wParam, lParam);
    default:
        /* WIN32: many more slots; otherwise DefWindowProcA */
        return 0;
    }
}

/* =========================================================================
 * EditWnd_onLButtonDown  (0x00422930, vtable index 14)
 *
 * Mouse-press handler.  If the screen is idle (state 0) a click anywhere
 * dismisses any child screen (setState 7).  Otherwise the click point is
 * unpacked from lParam and tested against the visible button rects; on a hit
 * the PRESSED sprite is blitted as feedback, the screen is redrawn, we sleep
 * briefly so the depression is visible, then the button's action runs.
 *
 *   x = lParam & 0xffff ;  y = lParam >> 16          (GET_X/Y_LPARAM)
 *   PtInRect(rcBtnGo)   -> press sprGoPressed   -> EditWnd_submitPlayerName
 *   PtInRect(rcBtnBack) -> press sprBackPressed -> back/cancel action
 *
 * WIN32: PtInRect, Sleep, vtable[3] redraw, FUN_0042b050 sprite blit
 * LINUX: SDL_Point + SDL_PointInRect; SDL_Delay; re-render
 * ======================================================================= */
intptr_t EditWnd_onLButtonDown(EditWindow *self, void *hwnd, unsigned int msg,
                               uintptr_t wParam, intptr_t lParam)
{
    int x, y;
    (void)hwnd; (void)msg; (void)wParam;

    if (self->dialogState == UI_STATE_INITIAL) {     /* +0xe8 == 0 */
        EditWindow_setState(self, UI_STATE_CLOSE_CHILD);
        return 0;
    }
    if (!self->flag1 /* +0xf4 buttonsVisible */) {
        return 0;
    }

    x = (int)(lParam & 0xffff);
    y = (int)((lParam >> 16) & 0xffff);

    /*
     * Six buttons are hit-tested in the original (0x422930); actions verbatim:
     *   rcBtnGo  (+0x13c, 0x403/0x404): submit name -> proceed
     *   rcBtnBack(+0x14c, 0x405/0x406): play sound, screensaver-pw probe, FUN_00408130(0xa)
     *   rcBtn407 (+0xfc):  enable multiplayer  g_pConfigMgr[7]=1, FUN_0043d2b0(npc,3)
     *   rcBtn409 (+0x10c): disable multiplayer g_pConfigMgr[7]=0, FUN_0043d2b0(npc,0)
     *   rcBtn40b (+0x11c): set   sub-option g_pConfigMgr[8]=1
     *   rcBtn40e (+0x12c): clear sub-option g_pConfigMgr[8]=0
     * Each blits its PRESSED sprite for feedback and repaints (FUN_00422010 + vtable[3]).
     */
    if (LOCO_RECT_contains(&self->rcBtnGo, x, y)) {
        /* visual feedback: blit pressed "Go" sprite (id 0x404) to the screen */
        MenuButton_drawPressed(self, &self->rcBtnGo, SPR_GO_PRESSED);
        /* WIN32: screensaver-pw check (0x5015); vtable[3] redraw; Sleep(0x96) */
        EditWnd_submitPlayerName(self);              /* 0x422660 — the Go action */
        return 0;
    }
    if (LOCO_RECT_contains(&self->rcBtnBack, x, y)) {
        MenuButton_drawPressed(self, &self->rcBtnBack, SPR_BACK_PRESSED);
        /* WIN32: PlaySoundA; screensaver-pw probe (res 0x5015); vtable[0x10];
         *        FUN_00408130(0xa)  — leave the screen via the secondary path */
        /* GameState_machine(0xa);  // FUN_00408130(0xa) */
        return 0;
    }
    /* rcBtn407/409/40b/40e: mode-select toggles — flip g_pConfigMgr flags and repaint.
     * (Omitted here; see the action map above and EditWnd_onLButtonDown's Ghidra comment.) */
    return 0;
}

/* =========================================================================
 * EditWnd_onMouseMove_hitTest  (0x00422d80, vtable index 20)
 *
 * Hover handler.  While the pointer is active (+0x14) it tests all eight
 * button/region rects in order and, on the first that contains the cursor,
 * triggers a redraw (the common tail at 0x422e78 -> vtable[3]).  This is the
 * engine's hover/refresh feedback path.
 * ======================================================================= */
intptr_t EditWnd_onMouseMove_hitTest(EditWindow *self, void *hwnd, unsigned int msg,
                                     uintptr_t wParam, intptr_t lParam)
{
    int x = (int)(lParam & 0xffff);
    int y = (int)((lParam >> 16) & 0xffff);
    const LOCO_RECT *order[8] = {
        &self->rcBtnGo, &self->rcBtnBack, &self->rcBtn407, &self->rcBtn409,
        &self->rcBtn40b, &self->rcBtn40e, &self->rcFixedA, &self->rcFixedB
    };
    int i;
    (void)hwnd; (void)msg; (void)wParam;

    for (i = 0; i < 8; i++) {
        if (LOCO_RECT_contains(order[i], x, y)) {
            /* WIN32: redraw via vtable[3] (CGWnd_onDraw, 0x425fd0) */
            return 0;
        }
    }
    return 0;
}

/* =========================================================================
 * EditWnd_onKeyDown  (0x00420bb0, vtable index 21 AND the EDIT subclass body)
 *
 * The child EDIT control is subclassed (init: SetWindowLongA GWL_WNDPROC ->
 * 0x420b20).  That thunk maps Enter/Esc/Tab to app messages and passes all
 * other keys to the original EDIT proc (so normal typing still works).  This
 * body activates the default/cancel button exactly like a mouse press:
 * Enter == clicking Go, Escape == clicking Back.
 *
 * WIN32: CallWindowProcA(savedEditWndProc, ...) for normal keys
 * LINUX: SDL_TEXTINPUT for typing; SDLK_RETURN/SDLK_ESCAPE for accelerators
 * ======================================================================= */
intptr_t EditWnd_onKeyDown(EditWindow *self, void *hwnd, unsigned int msg,
                           uintptr_t wParam, intptr_t lParam)
{
    (void)hwnd; (void)msg; (void)lParam;

    if (self->dialogState == UI_STATE_INITIAL) {
        /* WIN32: PostMessageA(self->hwnd, 0x40a, 0, 0) */
        return 0;
    }
    if (wParam == LOCO_VK_RETURN && self->flag1 /* buttonsVisible */) {
        MenuButton_drawPressed(self, &self->rcBtnGo, SPR_GO_PRESSED);
        /* WIN32: Sleep(0x96); redraw normal; */
        EditWnd_submitPlayerName(self);              /* same action as Go click */
        return 0;
    }
    if (wParam == LOCO_VK_ESCAPE && self->flag1) {
        /* Escape == clicking the +0x14c button (rcBtnBack): leaves via FUN_00408130(0xa) */
        MenuButton_drawPressed(self, &self->rcBtnBack, SPR_BACK_PRESSED);
        /* GameState_machine(0xa);  // FUN_00408130(0xa) */
        return 0;
    }
    /* Normal keystroke: WIN32 CallWindowProcA(self->savedEditWndProc, ...) */
    return 0;
}

/* =========================================================================
 * MenuButton_drawPressed  (helper for the press-feedback blit)
 *
 * Blits a button's PRESSED sprite into its rect on the primary DirectDraw
 * surface, then (in the original) sleeps MAINMENU_PRESS_MS and redraws the
 * normal sprite.  Mirrors the inline blit sequences in 0x422930 / 0x420bb0.
 * ======================================================================= */
static void MenuButton_drawPressed(EditWindow *self, const LOCO_RECT *rc, int spriteIdx)
{
    void *pressedSurface = self->menuSprite[spriteIdx].pSurface;
    (void)pressedSurface; (void)rc;
    /* WIN32: blit pressedSurface to g_pPrimarySurface (DAT_004fd3c4) at rc,
     *        then vtable[3] redraw, Sleep(MAINMENU_PRESS_MS).
     * LINUX: SDL_RenderCopy(renderer, pressedTexture, NULL, &sdlRect);
     *        SDL_RenderPresent(renderer); SDL_Delay(MAINMENU_PRESS_MS). */
}

/* =========================================================================
 * EditWnd_submitPlayerName  (0x00422660)  -- the "Go" / Enter action
 *
 * Reads the EDIT control text (<=11 chars), validates it (must NOT contain any
 * char from a reject set at 0x47e77c, and MUST contain at least one [A-Za-z]
 * from 0x47e744), stores it into the player record (g_pUserProfile+6) and
 * persists it to ee.ini [USER] Name=, then advances the screen's state machine
 * (single player) or kicks off the multiplayer game.  Transitions are recovered
 * verbatim from FUN_00422660.
 *
 * WIN32: GetWindowTextA, WritePrivateProfileStringA, SetWindowTextA
 * LINUX: read SDL text buffer; INI_SetString(g_pIniFile,"USER","Name",buf)
 * ======================================================================= */
void EditWnd_submitPlayerName(EditWindow *self)
{
    char buf[13];
    (void)buf;

    /* WIN32: vtable[4](self) refresh; GetWindowTextA(self->hwndEdit, buf, 13) */
    /* if (!strpbrk(buf, rejectSet) && strpbrk(buf, "A-Za-z")) {  // FUN_004676d0 */
    /*     PlayerRecord_setName(g_pUserProfile, buf);                            */
    /*     WritePrivateProfileStringA("USER","Name",buf,"ee.ini");              */
    /* }                                                                         */
    /* WIN32: SetWindowTextA(self->hwndEdit, g_pUserProfile + 6);               */

    {
        /* g_pConfigMgr (DAT_004fd3a8) flags drive the transition. */
        uint8_t mpActive = g_pConfigMgr ? *((uint8_t *)g_pConfigMgr + 7)    : 0;
        uint8_t subOpt   = g_pConfigMgr ? *((uint8_t *)g_pConfigMgr + 8)    : 0;

        if (!mpActive) {
            /* Single-player.  Pick a panel state from sub-option flags + which
             * PanelA item is enabled (panelA+0x1e0 / +0x1e1); fall back to 2. */
            if (!subOpt /* && config+0x24 set && (config+0x28==4 && panelA[0x1e0]) ||
                                                  (config+0x28==2 && panelA[0x1e1]) */) {
                EditWindow_setState(self, UI_STATE_PANEL_B_SINGLE);   /* 4 */
            } else if (subOpt /* && config+0x18 set && similar PanelA check */) {
                EditWindow_setState(self, UI_STATE_PANEL_B_MULTI);    /* 5 */
            } else {
                EditWindow_setState(self, UI_STATE_DEACTIVATED);      /* 2 */
            }
        } else {
            /* Multiplayer: start lobby + game world directly. */
            /* WIN32: FUN_0043d2b0(g_pNpcMgr,1); FUN_004616c0(g_pNet,1);
             *        EditWnd_startGameWorld(self); FUN_00408130(1) */
            EditWindow_setState(self, UI_STATE_SHUTDOWN);             /* 6 -> game start path */
        }
    }
}
