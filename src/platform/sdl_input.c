/*
 * Lego Loco (1998) - Native Linux Port
 * src/platform/sdl_input.c — SDL2 replacement for Win32 cursor and mouse input
 *
 * Replaces the following Win32 subsystems documented in src/input/input.c:
 *   USER32.DLL  ShowCursor         — hide/show OS hardware cursor
 *   USER32.DLL  GetCursorPos       — read global screen-space mouse position
 *   USER32.DLL  SetCursorPos       — warp mouse to screen position
 *   USER32.DLL  SetCapture         — lock mouse events to one window
 *   USER32.DLL  GetCapture         — query which window holds mouse capture
 *   USER32.DLL  ReleaseCapture     — release mouse event capture
 *   USER32.DLL  GetKeyState        — read instantaneous button state
 *   USER32.DLL  ClientToScreen     — convert client coords to screen coords
 *   USER32.DLL  GetClientRect      — query window client area dimensions
 *   USER32.DLL  LoadCursorFromFileA — load .ANI animated cursor from disk
 *   GDI32.DLL   SetCursor          — set active OS cursor shape
 *
 * WIN32 → LINUX API mapping table:
 *   ShowCursor(FALSE)  [loop until counter < 0]
 *     -> SDL_ShowCursor(SDL_DISABLE)    [single call; SDL has no counter]
 *
 *   ShowCursor(TRUE)
 *     -> SDL_ShowCursor(SDL_ENABLE)
 *
 *   GetCursorPos(&pt)
 *     -> SDL_GetGlobalMouseState(&pt.x, &pt.y)
 *        (global = screen-space, same coordinate frame as GetCursorPos)
 *
 *   SetCursorPos(x, y)
 *     -> SDL_WarpMouseGlobal(x, y)
 *        or SDL_WarpMouseInWindow(window, x - winX, y - winY)
 *        after SDL_GetWindowPosition(window, &winX, &winY)
 *
 *   SetCapture(hwnd)
 *     -> SDL_CaptureMouse(SDL_TRUE)
 *        SDL_SetWindowGrab((SDL_Window*)hwnd, SDL_TRUE)
 *
 *   GetCapture() == hwnd  [ownership check before release]
 *     -> SDL has no equivalent; call SDL_CaptureMouse(SDL_FALSE) unconditionally
 *
 *   ReleaseCapture()
 *     -> SDL_CaptureMouse(SDL_FALSE)
 *        SDL_SetWindowGrab(window, SDL_FALSE)
 *
 *   GetKeyState(VK_LBUTTON) & 0x8000   [button held]
 *     -> SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK
 *
 *   GetKeyState(VK_RBUTTON) & 0x8000
 *     -> SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_RMASK
 *
 *   LOWORD(lParam) from WM_MOUSEMOVE   [client-space X]
 *     -> SDL_MouseMotionEvent.x
 *
 *   HIWORD(lParam) from WM_MOUSEMOVE   [client-space Y]
 *     -> SDL_MouseMotionEvent.y
 *
 *   ClientToScreen(hwnd, &pt)
 *     -> SDL_GetWindowPosition(window, &wx, &wy); pt.x += wx; pt.y += wy
 *
 *   GetClientRect(hwnd, &rect)
 *     -> SDL_GetWindowSize(window, &w, &h)
 *        rect = {0, 0, w, h}
 *
 *   LoadCursorFromFileA("CURSORS\\busy_ani.ani")
 *     -> Parse RIFF ACON .ani file; see SDL_Input_LoadAniCursor below.
 *        Decompress first if flags byte at header+0x?? equals 0x1 (huffman).
 *
 *   SetCursor(hCursor)   [set OS cursor during loading screens]
 *     -> SDL_SetCursor(SDL_CreateColorCursor(surface, hotX, hotY))
 *
 *   IDirectDrawSurface::BltFast  [cursor sprite blit each frame]
 *     -> SDL_RenderCopy(renderer, cursorTexture, &srcRect, &dstRect)
 *        with srcRect.x = frameIndex * frameWidth
 *
 * Cursor states (mapped from input.c):
 *   CURSOR_STATE_NONE    0  — no cursor drawn (loading)
 *   CURSOR_STATE_IDLE    1  — default pointer (resource 0x1400)
 *   CURSOR_STATE_HOVER   2  — object hover highlight (resource 0x1401)
 *   CURSOR_STATE_GRAB    3  — object grabbed/drag (resource 0x1403)
 *   CURSOR_STATE_BUSY    4  — busy spinner (busy_ani.ani)
 *
 * .ANI cursor file format (RIFF ACON):
 *   RIFF chunk: "RIFF" + size + "ACON"
 *   anih chunk: 36-byte ANIHEADER with frame count, step count, frame dimensions
 *   seq  chunk: optional sequence table (frame order)
 *   LIST chunk containing icon subchunks with actual pixel data
 *   If anih.bfAttributes bit 0x1 is set: frames are compressed (huffman.c needed)
 *   If bit 0x1 is clear: frames are raw ICON resources
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../core/loco_types.h"

/* =========================================================================
 * Cursor state constants
 * ========================================================================= */
#define CURSOR_STATE_NONE    0
#define CURSOR_STATE_IDLE    1
#define CURSOR_STATE_HOVER   2
#define CURSOR_STATE_GRAB    3
#define CURSOR_STATE_BUSY    4

/* Number of frames in the sprite-strip cursor animations (from resource data) */
#define CURSOR_ANIM_FRAMES_IDLE   8
#define CURSOR_ANIM_FRAMES_GRAB   6
#define CURSOR_ANIM_FRAMES_HOVER  4
#define CURSOR_ANIM_FRAMES_BUSY  16

/* Cursor sprite dimensions per frame (from CursorAnimData parsed at init) */
#define CURSOR_FRAME_WIDTH   64
#define CURSOR_FRAME_HEIGHT  64

/* =========================================================================
 * Module-level state
 * ========================================================================= */

/* WIN32: hWndMain stored in CGWND::hwndGame
 * LINUX: cached SDL_Window* for warp/grab calls */
static SDL_Window   *g_inputWindow   = NULL;
static SDL_Renderer *g_inputRenderer = NULL;

/* Software cursor textures loaded from game cursor resources.
 * WIN32: IDirectDrawSurface* (sprite strip from LOCOBITMAP)
 * LINUX: SDL_Texture* per cursor state, horizontal sprite strip */
static SDL_Texture  *g_cursorTextures[5] = { NULL, NULL, NULL, NULL, NULL };

/* Current cursor state and animation frame */
static int  g_cursorState       = CURSOR_STATE_IDLE;
static int  g_cursorAnimFrame   = 0;
static int  g_cursorAnimTick    = 0;    /* tick counter for frame timing */
static int  g_cursorAnimDelay   = 4;    /* ticks per frame (game runs ~28ms/tick) */

/* Last rendered cursor position (screen space).
 * Used to erase the previous frame before drawing the new position.
 * WIN32: stored in CursorManager::destLeft/destTop (this+0x68/0x6c) */
static int  g_cursorLastX = 0;
static int  g_cursorLastY = 0;

/* Mouse button state cache.
 * WIN32: read from WM_LBUTTONDOWN/WM_LBUTTONUP message fields
 * LINUX: polled via SDL_GetMouseState each frame */
static int  g_mouseButtons = 0;

/* Capture active flag.
 * WIN32: tracked implicitly by SetCapture/ReleaseCapture ownership
 * LINUX: must be tracked explicitly */
static int  g_captureActive = 0;

/* =========================================================================
 * SDL_Input_Init  —  replaces cursor subsystem initialisation
 *
 * Called after the SDL window is created.  Stores the window reference,
 * hides the OS cursor, and prepares the cursor animation state machine.
 *
 * WIN32: ShowCursor(FALSE) loop in SetCursorCapture (0x00414290)
 *   The game loops ShowCursor(FALSE) until the return value < 0 because
 *   Win32 maintains a per-thread show-cursor reference counter.
 * LINUX: SDL_ShowCursor(SDL_DISABLE) — single call; SDL uses a simple flag.
 * ========================================================================= */
int SDL_Input_Init(SDL_Window *window, SDL_Renderer *renderer)
{
    if (window == NULL || renderer == NULL) {
        fprintf(stderr, "SDL_Input_Init: NULL window or renderer\n");
        return 0;
    }

    g_inputWindow   = window;
    g_inputRenderer = renderer;

    /* WIN32: WNDCLASS.hCursor = NULL in the window class registration
     *        followed by ShowCursor(FALSE) looped until counter < 0
     *        (see SetCursorCapture at 0x00414290 in input.c)
     * LINUX: Single call is sufficient; SDL cursor visibility is a boolean */
    SDL_ShowCursor(SDL_DISABLE);

    /* WIN32: SetCapture is called explicitly per-frame when the game window
     *        first receives focus.  On Linux we do not grab immediately;
     *        grab is activated on demand via SDL_Input_SetCapture. */

    g_cursorState     = CURSOR_STATE_IDLE;
    g_cursorAnimFrame = 0;
    g_cursorAnimTick  = 0;

    fprintf(stderr, "SDL_Input_Init: cursor subsystem ready\n");
    return 1;
}

/* =========================================================================
 * SDL_Input_Shutdown  —  cursor subsystem teardown
 *
 * Releases all cursor textures and restores the OS cursor.
 *
 * WIN32: ShowCursor(TRUE) once (counter goes from -1 to 0 = visible)
 * LINUX: SDL_ShowCursor(SDL_ENABLE)
 * ========================================================================= */
void SDL_Input_Shutdown(void)
{
    int i;

    /* Release all cursor textures */
    for (i = 0; i < 5; i++) {
        if (g_cursorTextures[i] != NULL) {
            /* WIN32: IDirectDrawSurface::Release (COM vtable[2]) */
            /* LINUX: SDL_DestroyTexture */
            SDL_DestroyTexture(g_cursorTextures[i]);
            g_cursorTextures[i] = NULL;
        }
    }

    /* Restore OS cursor on exit */
    /* WIN32: ShowCursor(TRUE) */
    /* LINUX: SDL_ShowCursor(SDL_ENABLE) */
    SDL_ShowCursor(SDL_ENABLE);

    /* Release any active mouse capture */
    if (g_captureActive) {
        /* WIN32: ReleaseCapture() */
        /* LINUX: SDL_CaptureMouse(SDL_FALSE) */
        SDL_CaptureMouse(SDL_FALSE);
        if (g_inputWindow)
            SDL_SetWindowGrab(g_inputWindow, SDL_FALSE);
        g_captureActive = 0;
    }

    g_inputWindow   = NULL;
    g_inputRenderer = NULL;
}

/* =========================================================================
 * SDL_Input_SetCapture  —  replaces SetCapture / ReleaseCapture
 *
 * Activates or deactivates OS mouse capture so all mouse events are routed
 * to the game window even when the cursor moves outside it.
 *
 * WIN32 (activate):
 *   SetCapture(hwnd)           — locks all mouse events to this HWND
 *   ShowCursor(FALSE) loop     — hides OS cursor (counter loop)
 *
 * WIN32 (deactivate):
 *   if (GetCapture() == hwnd) ReleaseCapture()  — conditional release
 *   ShowCursor(TRUE)           — restore OS cursor (not done in original; see note)
 *
 * LINUX (activate):
 *   SDL_CaptureMouse(SDL_TRUE)         — receive events outside window
 *   SDL_SetWindowGrab(win, SDL_TRUE)   — confine cursor to window bounds
 *   SDL_ShowCursor(SDL_DISABLE)        — hide OS cursor (idempotent)
 *
 * LINUX (deactivate):
 *   SDL_CaptureMouse(SDL_FALSE)
 *   SDL_SetWindowGrab(win, SDL_FALSE)
 *   SDL_ShowCursor(SDL_ENABLE)
 *
 * Note from input.c analysis:
 *   The Win32 deactivation path (SetCursorCapture with deactivate != 0)
 *   does NOT call ShowCursor(TRUE) directly; it relies on FUN_0045b940,
 *   FUN_00414fb0, and FUN_00414ef0 to restore the cursor.
 *   On Linux, SDL_ShowCursor(SDL_ENABLE) should be called explicitly here.
 * ========================================================================= */
void SDL_Input_SetCapture(int activate)
{
    if (activate) {
        if (g_captureActive) return; /* already active */

        /* WIN32: SetCapture((HWND)mgr->hwnd) */
        /* LINUX: SDL_CaptureMouse allows events outside window boundaries */
        SDL_CaptureMouse(SDL_TRUE);

        /* WIN32: SDL_SetWindowGrab confines cursor to window (optional in Win32,
         *        SetCapture already does this implicitly for mouse events) */
        if (g_inputWindow)
            SDL_SetWindowGrab(g_inputWindow, SDL_TRUE);

        /* WIN32: loop ShowCursor(FALSE) until return value < 0
         * LINUX: single call; SDL cursor visibility is a boolean, no counter */
        SDL_ShowCursor(SDL_DISABLE);

        g_captureActive = 1;

    } else {
        if (!g_captureActive) return; /* already released */

        /* WIN32: if (GetCapture() == mgr->hwnd) ReleaseCapture() */
        /* LINUX: no ownership check needed — just release unconditionally */
        SDL_CaptureMouse(SDL_FALSE);
        if (g_inputWindow)
            SDL_SetWindowGrab(g_inputWindow, SDL_FALSE);

        /* WIN32: OS cursor is restored by FUN_00414ef0 / FUN_0045b940 */
        /* LINUX: restore OS cursor explicitly (Win32 path defers this) */
        SDL_ShowCursor(SDL_ENABLE);

        g_captureActive = 0;
    }
}

/* =========================================================================
 * SDL_Input_GetMousePosition  —  replaces GetCursorPos
 *
 * Reads the current mouse position in global screen coordinates.
 *
 * WIN32: GetCursorPos(&pt)
 *   Fills a POINT with {x, y} in screen (global, not client) coordinates.
 *   These are the same coords used by ClientToScreen to convert to window-local.
 *
 * LINUX: SDL_GetGlobalMouseState(&x, &y)
 *   Returns coordinates in the same screen-global space as GetCursorPos.
 *   Note: SDL_GetMouseState returns window-relative coords instead.
 * ========================================================================= */
void SDL_Input_GetMousePosition(int *outX, int *outY)
{
    /* WIN32: GetCursorPos(&pt) — screen-space coords */
    /* LINUX: SDL_GetGlobalMouseState returns screen-space coords */
    SDL_GetGlobalMouseState(outX, outY);
}

/* =========================================================================
 * SDL_Input_GetMousePositionInWindow  —  replaces WM_MOUSEMOVE lParam decode
 *
 * Reads the mouse position relative to the game window client area.
 * This is the equivalent of LOWORD(lParam) / HIWORD(lParam) from
 * WM_MOUSEMOVE messages in the Win32 window procedure.
 *
 * WIN32: int x = LOWORD(lParam); int y = HIWORD(lParam);
 *   (extracted from the message parameter in WndProc)
 *
 * LINUX: SDL_GetMouseState(&x, &y)
 *   Returns window-relative (client-space) coordinates.
 *   Matches the LOWORD/HIWORD values from WM_MOUSEMOVE exactly.
 * ========================================================================= */
void SDL_Input_GetMousePositionInWindow(int *outX, int *outY)
{
    /* WIN32: int x = LOWORD(lParam); int y = HIWORD(lParam); from WM_MOUSEMOVE */
    /* LINUX: SDL_GetMouseState(&x, &y) — window-relative (client-space) */
    SDL_GetMouseState(outX, outY);
}

/* =========================================================================
 * SDL_Input_WarpMouse  —  replaces SetCursorPos
 *
 * Moves the mouse cursor to a specific window-relative position.
 * Called by InputCursor_ProcessWarpedPosition when ic->warpPending is set.
 *
 * WIN32: SetCursorPos(x, y)
 *   Takes screen-space (global) coordinates.
 *   Client-space coords must be converted first via ClientToScreen.
 *
 * LINUX: SDL_WarpMouseInWindow(window, x, y)
 *   Takes window-relative (client-space) coordinates directly.
 *   No ClientToScreen conversion needed.
 *
 * Parameters: x, y in window-relative (client) coordinate space.
 * ========================================================================= */
void SDL_Input_WarpMouse(int clientX, int clientY)
{
    /* WIN32: POINT pt = {clientX, clientY}; ClientToScreen(hwnd, &pt);
     *        SetCursorPos(pt.x, pt.y); */
    /* LINUX: SDL_WarpMouseInWindow takes client-space directly */
    if (g_inputWindow != NULL) {
        SDL_WarpMouseInWindow(g_inputWindow, clientX, clientY);
    }
}

/* =========================================================================
 * SDL_Input_ClientToScreen  —  replaces ClientToScreen
 *
 * Converts a window-relative (client-space) point to global screen coordinates.
 * Used in RenderCursor (0x00414c20) step 8 to position the cursor blit.
 *
 * WIN32: ClientToScreen(hwnd, &pt)
 *   Adds the window's screen-space origin to the client-relative point.
 *   The origin is the top-left corner of the client area in screen coordinates.
 *
 * LINUX: SDL_GetWindowPosition(window, &wx, &wy)
 *   Returns the window top-left in screen coordinates.
 *   Then: screenX = clientX + wx;  screenY = clientY + wy;
 *
 * Both in/out pointers are updated in place (same semantics as POINT*).
 * ========================================================================= */
void SDL_Input_ClientToScreen(int *inOutX, int *inOutY)
{
    int wx = 0, wy = 0;

    /* WIN32: ClientToScreen((HWND)mgr->hwnd, &pt)
     *   Fills pt with screen-space coords from client-space input. */
    /* LINUX: SDL_GetWindowPosition gives the window origin in screen space */
    if (g_inputWindow != NULL)
        SDL_GetWindowPosition(g_inputWindow, &wx, &wy);

    *inOutX += wx;
    *inOutY += wy;
}

/* =========================================================================
 * SDL_Input_GetClientRect  —  replaces GetClientRect
 *
 * Returns the dimensions of the game window's client area.
 * Called by UpdateClientRect (0x004140a0) to refresh CursorManager fields.
 *
 * WIN32: GetClientRect(hwnd, &rect)
 *   Fills rect with {left=0, top=0, right=clientW, bottom=clientH}.
 *   Always left=top=0 because client-space origin is always (0,0).
 *
 * LINUX: SDL_GetWindowSize(window, &w, &h)
 *   Returns the drawable area size.  No title-bar offset.
 *   Caller builds {0, 0, w, h} equivalent.
 * ========================================================================= */
void SDL_Input_GetClientRect(int *outW, int *outH)
{
    /* WIN32: RECT r; GetClientRect(hwnd, &r);
     *        w = r.right - r.left;  h = r.bottom - r.top; */
    /* LINUX: SDL_GetWindowSize(window, &w, &h) */
    if (g_inputWindow != NULL)
        SDL_GetWindowSize(g_inputWindow, outW, outH);
    else {
        *outW = 640;
        *outH = 480;
    }
}

/* =========================================================================
 * SDL_Input_GetButtonState  —  replaces GetKeyState(VK_LBUTTON / VK_RBUTTON)
 *
 * Returns the current instantaneous state of mouse buttons.
 *
 * WIN32: GetKeyState(VK_LBUTTON) & 0x8000   — left button held
 *        GetKeyState(VK_RBUTTON) & 0x8000   — right button held
 *        The high bit is set when the key/button is currently held down.
 *
 * LINUX: SDL_GetMouseState(NULL, NULL) returns a bitmask
 *        SDL_BUTTON_LMASK = left button
 *        SDL_BUTTON_RMASK = right button
 *        SDL_BUTTON_MMASK = middle button
 *
 * Returns SDL mouse button bitmask (use SDL_BUTTON_LMASK etc. to test).
 * ========================================================================= */
uint32_t SDL_Input_GetButtonState(void)
{
    /* WIN32: GetKeyState(VK_LBUTTON) & 0x8000 for left;
     *        GetKeyState(VK_RBUTTON) & 0x8000 for right */
    /* LINUX: SDL_GetMouseState returns all button states as a bitmask */
    return (uint32_t)SDL_GetMouseState(NULL, NULL);
}

/* =========================================================================
 * SDL_Input_IsLeftButtonHeld  —  convenience wrapper
 *
 * WIN32: GetKeyState(VK_LBUTTON) & 0x8000  (high bit = pressed)
 * LINUX: SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK
 * ========================================================================= */
int SDL_Input_IsLeftButtonHeld(void)
{
    /* WIN32: GetKeyState(VK_LBUTTON) & 0x8000 */
    /* LINUX: SDL_GetMouseState returns SDL_BUTTON_LMASK when left is held */
    return (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0;
}

/* =========================================================================
 * SDL_Input_IsRightButtonHeld  —  convenience wrapper
 *
 * WIN32: GetKeyState(VK_RBUTTON) & 0x8000
 * LINUX: SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_RMASK
 * ========================================================================= */
int SDL_Input_IsRightButtonHeld(void)
{
    /* WIN32: GetKeyState(VK_RBUTTON) & 0x8000 */
    /* LINUX: SDL_BUTTON_RMASK is set while the right button is held */
    return (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_RMASK) != 0;
}

/* =========================================================================
 * SDL_Input_LoadCursorTexture  —  replaces InitCursorResources (0x00414130)
 *
 * Loads a cursor sprite-strip SDL_Surface from a BMP/PNG file and uploads
 * it to a SDL_Texture for GPU-accelerated blitting each frame.
 *
 * WIN32: FUN_00446ea0(g_ResourceManager, CURSOR_RES_IDLE)
 *   Returns a CResourceBase* from the game's resource cache.
 *   vtable[1](resource) returns the LOCOBITMAP* (IDirectDrawSurface* wrapper).
 *   FUN_0042a3d0 locks/prepares the surface for pixel access.
 *   The surface is a horizontal sprite strip (all frames side-by-side).
 *   IDirectDraw4::CreateSurface with DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY
 *   creates the 256x256 shared cursor staging surface (g_CursorSurface).
 *
 * LINUX: SDL_LoadBMP(path) or IMG_Load(path) for the sprite strip.
 *   SDL_SetColorKey enables magenta (255, 0, 255) as transparent color.
 *   SDL_CreateTextureFromSurface uploads to GPU texture.
 *   The shared staging surface is replaced by rendering directly with
 *   SDL_RenderCopy using a source SDL_Rect to select the animation frame.
 *
 * Parameters:
 *   state    — CURSOR_STATE_* constant to assign the texture to
 *   bmpPath  — path to the sprite-strip BMP/PNG file on disk
 *
 * Returns 1 on success, 0 on failure.
 * ========================================================================= */
int SDL_Input_LoadCursorTexture(int state, const char *bmpPath)
{
    SDL_Surface *surface;
    SDL_Surface *converted;
    Uint32       magenta;

    if (state < 0 || state >= 5) return 0;
    if (g_inputRenderer == NULL)  return 0;

    /* WIN32: FUN_00446ea0(g_ResourceManager, cursorResId)
     *        Returns a LOCOBITMAP* wrapping an IDirectDrawSurface*
     * LINUX: SDL_LoadBMP loads the sprite strip directly from disk */
    surface = SDL_LoadBMP(bmpPath);
    if (surface == NULL) {
        fprintf(stderr, "SDL_Input_LoadCursorTexture: SDL_LoadBMP('%s'): %s\n",
                bmpPath, SDL_GetError());
        return 0;
    }

    /* Convert to a known pixel format for consistent color-key encoding.
     * WIN32: DirectDraw surfaces use the display pixel format (RGB555/RGB565).
     * LINUX: convert to ARGB8888 so SDL_MapRGB gives a predictable result. */
    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(surface);
    if (converted == NULL) {
        fprintf(stderr, "SDL_Input_LoadCursorTexture: SDL_ConvertSurfaceFormat: %s\n",
                SDL_GetError());
        return 0;
    }

    /* Set magenta as transparent color key.
     * WIN32: IDirectDrawSurface::SetColorKey(DDCKEY_SRCBLT, &ck)
     *   ck.dwColorSpaceLowValue = 0xF81F (RGB565 magenta) or 0x7C1F (RGB555)
     * LINUX: SDL_SetColorKey with SDL_MapRGB(fmt, 255, 0, 255) */
    magenta = SDL_MapRGB(converted->format, 255, 0, 255);

    /* WIN32: IDirectDrawSurface::SetColorKey(DDCKEY_SRCBLT, &ck) */
    /* LINUX: SDL_SetColorKey(surface, SDL_TRUE, key) */
    SDL_SetColorKey(converted, SDL_TRUE, magenta);

    /* Free previous texture for this state if any */
    if (g_cursorTextures[state] != NULL) {
        /* WIN32: IDirectDrawSurface::Release() */
        /* LINUX: SDL_DestroyTexture */
        SDL_DestroyTexture(g_cursorTextures[state]);
        g_cursorTextures[state] = NULL;
    }

    /* Upload to GPU texture.
     * WIN32: The sprite strip remains as a system-memory IDirectDrawSurface;
     *        BltFast copies each frame to the 256x256 staging surface then
     *        to the primary surface.
     * LINUX: SDL_CreateTextureFromSurface uploads to VRAM.
     *        SDL_SetTextureBlendMode enables alpha/color-key transparency. */
    g_cursorTextures[state] = SDL_CreateTextureFromSurface(g_inputRenderer,
                                                            converted);
    SDL_FreeSurface(converted);

    if (g_cursorTextures[state] == NULL) {
        fprintf(stderr, "SDL_Input_LoadCursorTexture: SDL_CreateTextureFromSurface: %s\n",
                SDL_GetError());
        return 0;
    }

    /* Enable blending so color-key pixels render as transparent.
     * WIN32: DDBLT_KEYSRC flag in the BltFast call handles this.
     * LINUX: SDL_SetTextureBlendMode with SDL_BLENDMODE_BLEND */
    SDL_SetTextureBlendMode(g_cursorTextures[state], SDL_BLENDMODE_BLEND);

    return 1;
}

/* =========================================================================
 * SDL_Input_SetCursorState  —  replaces SetCursorState (0x00414340)
 *
 * Changes the active cursor animation state and resets the frame counter.
 *
 * WIN32: Pure state-machine bookkeeping in SetCursorState (0x00414340).
 *        mgr->cursorStateId = stateId
 *        mgr->currentAnimFrame = 0
 *        Optionally zeros dirty-rect fields (resetPos flag).
 * LINUX: Same state-machine logic; no Win32 calls required.
 * ========================================================================= */
void SDL_Input_SetCursorState(int state)
{
    if (state < CURSOR_STATE_NONE || state > CURSOR_STATE_BUSY) return;

    /* WIN32: mgr->cursorStateId = stateId  (this+0x14) */
    g_cursorState = state;

    /* WIN32: mgr->currentAnimFrame = 0  (this+0x48) */
    g_cursorAnimFrame = 0;
    g_cursorAnimTick  = 0;

    /* WIN32: if resetPos, zero dirty-rect block at this+0x68..0x84 */
    g_cursorLastX = -1;
    g_cursorLastY = -1;
}

/* =========================================================================
 * SDL_Input_RenderCursor  —  replaces RenderCursor (0x00414c20)
 *
 * Renders the software cursor sprite at the current mouse position.
 * Called once per game frame when the cursor capture is active.
 *
 * WIN32 two-stage pipeline:
 *   Stage 1: BltFast frame strip → 256x256 g_CursorSurface
 *     IDirectDrawSurface::BltFast(x, y, animSurf, &srcRect, DDBLTFAST_SRCCOLORKEY)
 *   Stage 2: g_CursorSurface → primary surface (g_pDDSPrimary)
 *     IDirectDrawSurface::Blt(&dstRect, g_CursorSurface, NULL, DDBLT_WAIT, NULL)
 *
 * LINUX single-stage pipeline:
 *   SDL_RenderCopy(renderer, cursorTexture, &srcRect, &dstRect)
 *   srcRect.x = frameIndex * CURSOR_FRAME_WIDTH  (column in sprite strip)
 *   srcRect.y = 0                                (always row 0)
 *   srcRect.w = CURSOR_FRAME_WIDTH
 *   srcRect.h = CURSOR_FRAME_HEIGHT
 *   dstRect.x = mouseX - hotspotX
 *   dstRect.y = mouseY - hotspotY
 *
 * Hotspot offsets come from CursorAnimData+0x32/+0x34 in the original;
 * they are hardcoded here until the resource parser provides them.
 * ========================================================================= */
void SDL_Input_RenderCursor(void)
{
    SDL_Texture *tex;
    SDL_Rect     srcRect;
    SDL_Rect     dstRect;
    int          mouseX, mouseY;
    int          frameCount;
    /* Hotspot: logical "tip" of the cursor within the sprite frame.
     * WIN32: stored at CursorAnimData+0x32 (hotspotX), +0x34 (hotspotY)
     * LINUX: placeholder values — replace with parsed resource data */
    static const int hotspotX = 4;
    static const int hotspotY = 4;

    /* Do not render if state is NONE (loading) or no capture active */
    if (g_cursorState == CURSOR_STATE_NONE) return;
    if (g_inputRenderer == NULL) return;

    tex = g_cursorTextures[g_cursorState];
    if (tex == NULL) return;

    /* Determine frame count for this cursor state */
    switch (g_cursorState) {
    case CURSOR_STATE_IDLE:  frameCount = CURSOR_ANIM_FRAMES_IDLE;  break;
    case CURSOR_STATE_HOVER: frameCount = CURSOR_ANIM_FRAMES_HOVER; break;
    case CURSOR_STATE_GRAB:  frameCount = CURSOR_ANIM_FRAMES_GRAB;  break;
    case CURSOR_STATE_BUSY:  frameCount = CURSOR_ANIM_FRAMES_BUSY;  break;
    default:                 frameCount = 1;                         break;
    }

    /* Advance animation frame counter.
     * WIN32: mgr->currentAnimFrame advances in RenderCursor, wraps at frameCount
     *   see CursorFrameSet_AdvanceFrame in input.c */
    g_cursorAnimTick++;
    if (g_cursorAnimTick >= g_cursorAnimDelay) {
        g_cursorAnimTick = 0;
        g_cursorAnimFrame = (g_cursorAnimFrame + 1) % frameCount;
    }

    /* Read current mouse position in window-relative (client) space.
     * WIN32: GetCursorPos (screen-space) then subtract window origin.
     *        In RenderCursor this is GetCursorPos + ClientToScreen for the
     *        forward direction; the blit dest uses screen coords directly.
     * LINUX: SDL_GetMouseState returns client-space coords immediately. */
    /* WIN32: GetCursorPos(&screenPt) */
    /* LINUX: SDL_GetMouseState(&mouseX, &mouseY) */
    SDL_GetMouseState(&mouseX, &mouseY);

    /* Build source rect: select the correct frame column from the sprite strip.
     * WIN32: srcFrameX = frameWidth * currentAnimFrame
     *        srcRect = {srcFrameX, 0, srcFrameX + frameWidth - 1, frameHeight - 1}
     *        (these are DirectDraw-style inclusive RECT coordinates)
     * LINUX: SDL_Rect uses {x, y, w, h} exclusive-end style */
    srcRect.x = g_cursorAnimFrame * CURSOR_FRAME_WIDTH;
    srcRect.y = 0;
    srcRect.w = CURSOR_FRAME_WIDTH;
    srcRect.h = CURSOR_FRAME_HEIGHT;

    /* Build destination rect: position cursor sprite relative to mouse pos.
     * WIN32: spritePt.x = screenPt.x - hotspotX; (after ClientToScreen)
     *        dstRect is in primary-surface (screen) coordinates.
     * LINUX: SDL_RenderCopy uses window-relative coords; no offset needed. */
    dstRect.x = mouseX - hotspotX;
    dstRect.y = mouseY - hotspotY;
    dstRect.w = CURSOR_FRAME_WIDTH;
    dstRect.h = CURSOR_FRAME_HEIGHT;

    /* Clamp to window bounds (mirrors the viewport clipping in RenderCursor) */
    {
        int winW, winH;
        SDL_Input_GetClientRect(&winW, &winH);
        if (dstRect.x < 0) dstRect.x = 0;
        if (dstRect.y < 0) dstRect.y = 0;
        if (dstRect.x + dstRect.w > winW) dstRect.x = winW - dstRect.w;
        if (dstRect.y + dstRect.h > winH) dstRect.y = winH - dstRect.h;
    }

    /* Stage 1 + 2 combined:
     * WIN32: BltFast(animSurf → stagingSurf) then Blt(stagingSurf → primarySurf)
     * LINUX: SDL_RenderCopy renders directly to the renderer target (back-buffer) */
    SDL_RenderCopy(g_inputRenderer, tex, &srcRect, &dstRect);

    /* Cache position for next-frame erase.
     * WIN32: mgr->destLeft = spritePt.x  (this+0x68)
     *        mgr->destTop  = spritePt.y  (this+0x6c) */
    g_cursorLastX = dstRect.x;
    g_cursorLastY = dstRect.y;
}

/* =========================================================================
 * SDL_Input_LoadAniCursor  —  replaces LoadCursorFromFileA
 *
 * Parses a Windows RIFF ACON animated cursor file and returns the first
 * frame as an SDL_Surface.  Used for the "busy" loading cursor
 * (CURSORS\busy_ani.ani) shown during long asset loads.
 *
 * WIN32: LoadCursorFromFileA("CURSORS\\busy_ani.ani")
 *   Returns an HCURSOR; SetCursor(hCursor) installs it as the OS cursor.
 *   The OS cursor is temporarily restored (ShowCursor(TRUE)) for this path.
 *
 * LINUX: Parse RIFF ACON manually:
 *   1. Read "RIFF" FourCC + chunk size + "ACON" FourCC
 *   2. Read "anih" chunk: ANIHEADER (36 bytes)
 *      anih.cbSiz         = 36
 *      anih.cFrames       = number of frames
 *      anih.cSteps        = number of steps in animation sequence
 *      anih.cx/cy         = frame width/height (may be 0; fallback to icon size)
 *      anih.cBitCount     = color depth (may be 0)
 *      anih.cPlanes       = planes (may be 0)
 *      anih.JifRate       = jiffies per frame (1/60 sec units)
 *      anih.bfAttributes  = flags:
 *         0x01 = AF_ICON: frames are ICON resources (not raw DIB)
 *         0x02 = AF_SEQUENCE: seq chunk present
 *      IMPORTANT: bit 0x1 in the original comment in the task refers to
 *      compression; in standard ACON files AF_ICON=0x1 means ICON format
 *      (not compressed). The game's huffman.c is for the RFD asset archive,
 *      not ACON files.
 *   3. Read optional "seq " chunk for frame sequence table
 *   4. Read "LIST" chunk with "fram" type containing icon subchunks
 *   5. For each "icon" subchunk: parse as ICONDIR → ICONDIRENTRY → DIB bits
 *
 * This is a stub implementation that returns a placeholder surface.
 * Full ACON parsing requires an ICO/DIB reader (see ico_load in platform libs).
 *
 * Returns SDL_Surface* for the first frame, or NULL on failure.
 * Caller must SDL_FreeSurface the result.
 * ========================================================================= */
SDL_Surface *SDL_Input_LoadAniCursor(const char *aniPath)
{
    FILE    *fp;
    uint8_t  header[12];
    uint32_t magic;

    /* WIN32: LoadCursorFromFileA(aniPath)
     *   HCURSOR is an opaque handle managed by the OS.
     * LINUX: Open and parse the RIFF ACON file manually. */
    fp = fopen(aniPath, "rb");
    if (fp == NULL) {
        fprintf(stderr, "SDL_Input_LoadAniCursor: cannot open '%s'\n", aniPath);
        return NULL;
    }

    /* Verify RIFF "ACON" magic */
    if (fread(header, 1, 12, fp) != 12) {
        fclose(fp);
        fprintf(stderr, "SDL_Input_LoadAniCursor: short read on '%s'\n", aniPath);
        return NULL;
    }
    fclose(fp);

    /* Check "RIFF" FourCC at offset 0 */
    magic = (uint32_t)header[0] | ((uint32_t)header[1] << 8)
          | ((uint32_t)header[2] << 16) | ((uint32_t)header[3] << 24);
    if (magic != 0x46464952 /* 'RIFF' */) {
        fprintf(stderr, "SDL_Input_LoadAniCursor: '%s' is not a RIFF file\n",
                aniPath);
        return NULL;
    }

    /* Check "ACON" FourCC at offset 8 */
    magic = (uint32_t)header[8] | ((uint32_t)header[9] << 8)
          | ((uint32_t)header[10] << 16) | ((uint32_t)header[11] << 24);
    if (magic != 0x4E4F4341 /* 'ACON' */) {
        fprintf(stderr, "SDL_Input_LoadAniCursor: '%s' is not an ACON file\n",
                aniPath);
        return NULL;
    }

    /* STUB: Full ACON/ICON parsing not yet implemented.
     * Return a placeholder 32x32 magenta surface so the cursor slot is non-NULL.
     *
     * TODO: Parse "anih" chunk to read frame count and dimensions.
     *       Parse "LIST"/"fram"/"icon" subchunks for each frame.
     *       Convert each ICON to SDL_Surface via DIB header parsing.
     *       If anih.bfAttributes & 0x02 (AF_SEQUENCE): read "seq " chunk
     *       to get frame display order.
     *
     * WIN32: LoadCursorFromFileA handles all of the above internally.
     * LINUX: Must parse RIFF manually; no OS support for .ani files. */
    fprintf(stderr, "SDL_Input_LoadAniCursor: STUB — '%s' detected but not parsed\n",
            aniPath);

    /* Return a placeholder surface so callers don't crash */
    return SDL_CreateRGBSurface(0, 32, 32, 32, 0, 0, 0, 0);
}

/* =========================================================================
 * SDL_Input_ProcessEvent  —  replaces Win32 WndProc mouse message handlers
 *
 * Processes a single SDL_Event for mouse input.  Called from the main event
 * loop (Platform_ProcessEvents in sdl_window.c) for each polled event.
 *
 * WIN32 equivalents handled here:
 *   SDL_MOUSEMOTION       <- WM_MOUSEMOVE      (LOWORD/HIWORD lParam)
 *   SDL_MOUSEBUTTONDOWN   <- WM_LBUTTONDOWN / WM_RBUTTONDOWN
 *   SDL_MOUSEBUTTONUP     <- WM_LBUTTONUP  / WM_RBUTTONUP
 *   SDL_WINDOWEVENT_FOCUS_LOST  <- WM_ACTIVATE (wParam = WA_INACTIVE)
 *   SDL_WINDOWEVENT_FOCUS_GAINED <- WM_ACTIVATE (wParam = WA_ACTIVE)
 * ========================================================================= */
void SDL_Input_ProcessEvent(const SDL_Event *ev)
{
    if (ev == NULL) return;

    switch (ev->type) {

    /* WIN32: WM_MOUSEMOVE — lParam encodes {LOWORD=x, HIWORD=y} in client space */
    case SDL_MOUSEMOTION:
        /* ev->motion.x and ev->motion.y are window-relative (client-space)
         * equivalents of LOWORD(lParam) and HIWORD(lParam).
         * Pass to game input handler: InputCursor_Tick / Mouse_ScreenToIso */
        /* TODO: call game input subsystem:
         *   Input_OnMouseMove(ev->motion.x, ev->motion.y); */
        break;

    /* WIN32: WM_LBUTTONDOWN / WM_RBUTTONDOWN */
    case SDL_MOUSEBUTTONDOWN:
        if (ev->button.button == SDL_BUTTON_LEFT) {
            /* WIN32: WM_LBUTTONDOWN; ic->leftClickFlag = 1 */
            /* TODO: Input_OnLeftButtonDown(ev->button.x, ev->button.y); */
        } else if (ev->button.button == SDL_BUTTON_RIGHT) {
            /* WIN32: WM_RBUTTONDOWN; ic->rightClickFlag = 1 */
            /* TODO: Input_OnRightButtonDown(ev->button.x, ev->button.y); */
        }
        break;

    /* WIN32: WM_LBUTTONUP / WM_RBUTTONUP */
    case SDL_MOUSEBUTTONUP:
        if (ev->button.button == SDL_BUTTON_LEFT) {
            /* WIN32: WM_LBUTTONUP */
            /* TODO: Input_OnLeftButtonUp(ev->button.x, ev->button.y); */
        } else if (ev->button.button == SDL_BUTTON_RIGHT) {
            /* WIN32: WM_RBUTTONUP */
            /* TODO: Input_OnRightButtonUp(ev->button.x, ev->button.y); */
        }
        break;

    /* WIN32: WM_ACTIVATE with wParam=WA_INACTIVE — window lost focus */
    case SDL_WINDOWEVENT:
        if (ev->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            /* Mirrors OnWindowActivate (0x00414a80) in input.c.
             * Releases cursor capture when the game window loses focus. */
            SDL_Input_SetCapture(0 /* deactivate */);
        } else if (ev->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
            /* WIN32: WM_ACTIVATE wParam=WA_ACTIVE — window regained focus */
            SDL_Input_SetCapture(1 /* activate */);
        }
        break;

    default:
        break;
    }
}
