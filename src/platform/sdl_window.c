/*
 * Lego Loco (1998) - Native Linux Port
 * src/platform/sdl_window.c — SDL2 window management
 *
 * Replaces Win32 functions documented in win32_platform.c:
 *   FUN_00406680 (0x406680): RegisterClassExA + CreateWindowExA -> SDL_CreateWindow
 *   FUN_00406480 (0x406480): Pre-window DirectDraw setup -> SDL_CreateRenderer
 *   FUN_004077a0 (0x4077a0): Window teardown -> SDL_DestroyWindow
 *   FUN_0045e1e0 (0x45e1e0): Frame present -> SDL_RenderPresent
 *   WndProc:                 Message handlers -> SDL_PollEvent
 *
 * WIN32 WndProc messages -> SDL2 event equivalents:
 *   WM_QUIT          -> SDL_QUIT
 *   WM_MOUSEMOVE     -> SDL_MOUSEMOTION
 *   WM_LBUTTONDOWN   -> SDL_MOUSEBUTTONDOWN (button = SDL_BUTTON_LEFT)
 *   WM_RBUTTONDOWN   -> SDL_MOUSEBUTTONDOWN (button = SDL_BUTTON_RIGHT)
 *   WM_LBUTTONUP     -> SDL_MOUSEBUTTONUP
 *   WM_KEYDOWN       -> SDL_KEYDOWN
 *   WM_KEYUP         -> SDL_KEYUP
 *   WM_ACTIVATE      -> SDL_WINDOWEVENT_FOCUS_GAINED/LOST
 *   WM_SIZE          -> SDL_WINDOWEVENT_RESIZED
 *   WM_PAINT         -> (handled by renderer — no equivalent needed)
 *   WM_DESTROY       -> SDL_QUIT
 */

#include "sdl_window.h"
#include "../core/core.h"
#include "../core/loco_types.h"
#include <stdio.h>

SDL_Window   *g_sdlWindow   = NULL;
SDL_Renderer *g_sdlRenderer = NULL;

/*
 * Platform_CreateWindow
 *
 * Replaces FUN_00406680: RegisterClassExA + CreateWindowExA
 *
 * Original creates a WS_POPUP fullscreen window at screen center with
 * title "LEGO LOCO" and no cursor (hCursor = NULL in WNDCLASS).
 *
 * WIN32: RegisterClassExA + CreateWindowExA + SetWindowPos
 * LINUX: SDL_CreateWindow + SDL_CreateRenderer
 */
int Platform_CreateWindow(CGWND *game, const char *title, int w, int h) {
    /* WIN32: GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN) to center window */
    /* LINUX: SDL handles centering with SDL_WINDOWPOS_CENTERED */

    g_sdlWindow = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS
    );

    if (!g_sdlWindow) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }

    /* WIN32: DirectDraw creates surfaces separately; IDirectDraw::SetCooperativeLevel
     *        with DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN
     * LINUX: SDL_CreateRenderer handles GPU-accelerated compositing */
    g_sdlRenderer = SDL_CreateRenderer(
        g_sdlWindow, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!g_sdlRenderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_sdlWindow);
        g_sdlWindow = NULL;
        return 0;
    }

    /* WIN32: DirectDraw SetCooperativeLevel + SetDisplayMode(640, 480, 16)
     * LINUX: Scale to logical 640x480 regardless of actual display resolution */
    SDL_RenderSetLogicalSize(g_sdlRenderer, w, h);

    /* WIN32: WNDCLASS.hCursor = NULL + ShowCursor(FALSE) to hide OS cursor
     * LINUX: SDL_ShowCursor(SDL_DISABLE) — game draws its own cursor sprite */
    SDL_ShowCursor(SDL_DISABLE);

    /* Store SDL window handle in CGWND for subsystem access */
    if (game) {
        game->hwndGame = (HWND)g_sdlWindow;
    }

    fprintf(stderr, "Window created: %s (%dx%d)\n", title, w, h);
    return 1;
}

/*
 * Platform_DestroyWindow
 *
 * Replaces FUN_004077a0: DestroyWindow + UnregisterClassA shutdown
 */
void Platform_DestroyWindow(CGWND *game) {
    if (g_sdlRenderer) {
        SDL_DestroyRenderer(g_sdlRenderer);
        g_sdlRenderer = NULL;
    }
    if (g_sdlWindow) {
        SDL_DestroyWindow(g_sdlWindow);
        g_sdlWindow = NULL;
    }
    if (game) {
        game->hwndGame = NULL;
    }
}

/*
 * Platform_ProcessEvents
 *
 * Replaces the Win32 PeekMessage / GetMessage / TranslateMessage / DispatchMessage loop.
 *
 * Original loop at WinMain (0x462e90):
 *   while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
 *     TranslateMessage(&msg);
 *     DispatchMessageA(&msg);
 *   }
 *
 * Returns 0 when SDL_QUIT is received (game should exit).
 */
int Platform_ProcessEvents(CGWND *game) {
    SDL_Event ev;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {

        /* WIN32: WM_QUIT / WM_DESTROY -> PostQuitMessage(0) */
        case SDL_QUIT:
            return 0;

        /* WIN32: WM_KEYDOWN */
        case SDL_KEYDOWN:
            if (ev.key.keysym.sym == SDLK_ESCAPE ||
                ev.key.keysym.sym == SDLK_F4) {
                /* F4 or Escape quits — mirrors ALT+F4 WM_CLOSE handling */
                return 0;
            }
            /* Route to game input handler */
            /* CGWND_OnKeyDown(game, ev.key.keysym.sym); */
            break;

        /* WIN32: WM_KEYUP */
        case SDL_KEYUP:
            /* CGWND_OnKeyUp(game, ev.key.keysym.sym); */
            break;

        /* WIN32: WM_MOUSEMOVE — (LOWORD(lParam), HIWORD(lParam)) = cursor pos */
        case SDL_MOUSEMOTION:
            /* Input_OnMouseMove(ev.motion.x, ev.motion.y); */
            break;

        /* WIN32: WM_LBUTTONDOWN / WM_RBUTTONDOWN */
        case SDL_MOUSEBUTTONDOWN:
            /* Input_OnMouseButton(ev.button.button, 1, ev.button.x, ev.button.y); */
            break;

        /* WIN32: WM_LBUTTONUP / WM_RBUTTONUP */
        case SDL_MOUSEBUTTONUP:
            /* Input_OnMouseButton(ev.button.button, 0, ev.button.x, ev.button.y); */
            break;

        /* WIN32: WM_ACTIVATE — game pauses when window loses focus */
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                /* CGWND_OnDeactivate(game); */
            } else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                /* CGWND_OnActivate(game); */
            }
            break;

        default:
            break;
        }
    }

    return 1; /* continue running */
}

/*
 * Platform_Present
 *
 * Replaces FUN_0045e1e0: IDirectDrawSurface::Flip or Blt to primary surface.
 *
 * WIN32: g_pDDSPrimary->Blt(NULL, g_pDDSBack, NULL, DDBLT_WAIT, NULL)
 *        or g_pDDSPrimary->Flip(NULL, DDFLIP_WAIT)
 * LINUX: SDL_RenderPresent(g_sdlRenderer)
 *
 * Called when the dirty flag (g_timerFired) is set by the 28ms timer.
 */
void Platform_Present(CGWND *game) {
    SDL_RenderPresent(g_sdlRenderer);

    /* WIN32: ResetEvent(g_hGameLoopEvent) after presenting */
    /* LINUX: g_timerFired is reset by the SDL timer callback */
}
