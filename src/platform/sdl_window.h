/*
 * Lego Loco (1998) - Native Linux Port
 * src/platform/sdl_window.h — SDL2 window management stub header
 *
 * Replaces: Win32 RegisterClassExA, CreateWindowExA, WndProc, message loop
 * See: src/platform/win32_platform.c for the original Win32 code documented
 */

#ifndef LOCO_SDL_WINDOW_H
#define LOCO_SDL_WINDOW_H

#include <SDL2/SDL.h>
#include "../core/core.h"

/* WIN32: CreateWindowExA("LEGO LOCO", ..., WS_POPUP, fullscreen)
 * LINUX: SDL_CreateWindow + SDL_CreateRenderer */
int  Platform_CreateWindow(CGWND *game, const char *title, int w, int h);

/* WIN32: DestroyWindow(hwnd) + UnregisterClassA
 * LINUX: SDL_DestroyRenderer + SDL_DestroyWindow */
void Platform_DestroyWindow(CGWND *game);

/* WIN32: WM_QUIT posted from WndProc on ALT+F4 / close button
 * LINUX: SDL_QUIT event from SDL_PollEvent */
int  Platform_ProcessEvents(CGWND *game);

/* WIN32: IDirectDrawSurface::Flip / Blt to primary surface
 * LINUX: SDL_RenderPresent(renderer) */
void Platform_Present(CGWND *game);

/* SDL2 window and renderer handles (global for simplicity) */
extern SDL_Window   *g_sdlWindow;
extern SDL_Renderer *g_sdlRenderer;

#endif /* LOCO_SDL_WINDOW_H */
