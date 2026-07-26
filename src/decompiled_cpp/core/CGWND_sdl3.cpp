/**
 * CGWND_sdl3.cpp — SDL3-specific implementations for CGWND
 */
#ifndef _WIN32

#include "CGWND.h"
#include <SDL3/SDL.h>
#include <cstdio>

extern "C" {
    SDL_Renderer* SDL3_GetRenderer(void);
    void GameLoop_FrameUpdate(void);
}

static SDL_Renderer* g_renderer = nullptr;

static void PumpMessages_SDL3(uint8_t filter)
{
    if (!g_renderer) {
        g_renderer = SDL3_GetRenderer();
    }

    SDL_Event event;
    
    /* Main render loop — runs until quit */
    while (true) {
        /* Process all pending events */
        while (SDL_PollEvent(&event)) {
        bool isMouseEvent = false;
        switch (event.type) {
        case SDL_EVENT_QUIT:
            return;  /* window close button */
        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_ESCAPE)
                return;  /* Escape to quit */
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (filter != 0) { isMouseEvent = true; }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (filter != 0) { isMouseEvent = true; }
            break;
        default:
            if (filter != 0) { return; }
            break;
        }
        if (isMouseEvent) { continue; }
    }
        
        /* Check for quit */
        if (event.type == SDL_EVENT_QUIT) {
            break;
        }

        /* Game logic tick */
        GameLoop_FrameUpdate();

        /* Render a frame */
        if (g_renderer) {
            SDL_SetRenderDrawColor(g_renderer, 0, 40, 80, 255);
            SDL_RenderClear(g_renderer);
            SDL_RenderPresent(g_renderer);
        }
    }
}

void CGWND_PumpMessages(void* cgwnd_ptr, uint8_t filter)
{
    (void)cgwnd_ptr;
    PumpMessages_SDL3(filter);
}

#endif
