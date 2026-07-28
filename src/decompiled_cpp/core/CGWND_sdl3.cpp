/**
 * CGWND_sdl3.cpp — SDL3-specific implementations for CGWND
 */
#ifndef _WIN32

#include "CGWND.h"
#include "../ui/EditWindow.h"
#include "../../sdl3_shims/sdl3_ddraw.h"
#include "../../sdl3_shims/sdl3_window.h"
#include "../../sdl3_shims/sdl3_game_audio.h"
#include <SDL3/SDL.h>
#include <cstdio>

extern "C" {
    SDL_Renderer* SDL3_GetRenderer(void);
    void GameLoop_FrameUpdate(void);
}

static SDL_Renderer* g_renderer = nullptr;

extern void* g_ui_main;
extern int g_game_mode;

static EditWindow* active_host_menu()
{
    return g_game_mode == 2 ? static_cast<EditWindow*>(g_ui_main) : nullptr;
}

static void PumpMessages_SDL3(uint8_t filter)
{
    if (!g_renderer) {
        g_renderer = SDL3_GetRenderer();
    }

    SDL_Event event;
    SDL_Window* const text_input_window = SDL3_GetWindow();
    if (text_input_window) SDL_StartTextInput(text_input_window);
    const auto stop_text_input = [text_input_window]() {
        if (text_input_window) SDL_StopTextInput(text_input_window);
    };
    
    /* Main render loop — runs until quit */
    while (true) {
        /* Process all pending events */
        while (SDL_PollEvent(&event)) {
        bool isMouseEvent = false;
        switch (event.type) {
        case SDL_EVENT_QUIT:
            stop_text_input();
            return;  /* window close button */
        case SDL_EVENT_KEY_DOWN:
            if (EditWindow* menu = active_host_menu()) {
                if (menu->hostHandleKey(static_cast<int32_t>(event.key.key))) break;
            }
            if (event.key.key == SDLK_ESCAPE) {
                stop_text_input();
                return;  /* Escape to quit */
            }
            break;
        case SDL_EVENT_TEXT_INPUT:
            if (EditWindow* menu = active_host_menu()) {
                menu->hostHandleTextInput(event.text.text);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (EditWindow* menu = active_host_menu()) {
                menu->hostHandlePointer(event.motion.x, event.motion.y, false);
            }
            if (filter != 0) { isMouseEvent = true; }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (EditWindow* menu = active_host_menu()) {
                    menu->hostHandlePointer(event.button.x, event.button.y, true);
                }
            }
            if (filter != 0) { isMouseEvent = true; }
            break;
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

        // The Win32 path at 0x40824C posts its terminal message only after
        // requesting sound 0x5026.  The host has no Win32 message queue, so
        // retain its SDL stream until the one-shot drains before exiting.
        if (g_game_mode == 10) {
            if (SDL3_GameAudioPump()) {
                SDL_Delay(1);
                continue;
            }
            stop_text_input();
            return;
        }

        /* Game logic tick */
        GameLoop_FrameUpdate();
        SDL3_GameAudioPump();

        // Host-only replacement for the x86 UIPANEL/offscreen-surface path:
        // compose original EditWindow resources in logical canvas coordinates.
        if (EditWindow* menu = active_host_menu()) {
            menu->hostRenderFrame();
        }

        // The primary DirectDraw target is now the sole frame source. The
        // fallback preserves the launch screen until any target exists.
        if (!SDL3_PresentPrimarySurface() && g_renderer) {
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
