/**
 * CGWND_sdl3.cpp — SDL3-specific implementations for CGWND
 */
#ifndef _WIN32

#include "CGWND.h"
#include "../ui/EditWindow.h"
#include "../../sdl3_shims/sdl3_ddraw.h"
#include "../../sdl3_shims/sdl3_window.h"
#include "../../sdl3_shims/sdl3_game_audio.h"
#include "../../sdl3_shims/sdl3_intro_video.h"
#include "../../sdl3_shims/host_test_events.h"
#include <SDL3/SDL.h>
#include <cstdio>

extern "C" {
    SDL_Renderer* SDL3_GetRenderer(void);
    void GameLoop_FrameUpdate(void);
}

namespace loco { namespace host {
    bool HasPendingAsyncTask();
    void RunPendingAsyncTask();
} }

static SDL_Renderer* g_renderer = nullptr;

extern void* g_ui_main;
extern int g_game_mode;

void CGWND_PumpMessages(void* cgwnd_ptr, uint8_t filter);

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
        // Preserve ownership across the event pass: an Escape click can end
        // the final intro immediately, in which case mode 2 must still be
        // entered before GameLoop_FrameUpdate sees startup mode 0.
        const bool introWasActive = loco::intro::isActive();

        /* Process all pending events */
        while (SDL_PollEvent(&event)) {
        bool isMouseEvent = false;
        switch (event.type) {
        case SDL_EVENT_QUIT:
            stop_text_input();
            return;  /* window close button */
        case SDL_EVENT_KEY_DOWN:
            if (loco::intro::isActive()) {
                // MCI child subclass 0x4207C0 accepts every WM_KEYDOWN and
                // posts 0x40A; parent handler 0x420F6F enters state 7.
                loco::intro::skipAll();
                break;
            }
            if (EditWindow* menu = active_host_menu()) {
                if (menu->hostHandleKey(static_cast<int32_t>(event.key.key))) break;
            }
            if (event.key.key == SDLK_ESCAPE) {
                stop_text_input();
                return;  /* Escape to quit */
            }
            break;
        case SDL_EVENT_TEXT_INPUT:
            // 0x4207C0 also consumes WM_CHAR while the MCI child is active.
            if (loco::intro::isActive()) {
                loco::intro::skipAll();
                break;
            }
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
            if (loco::intro::isActive()) {
                // 0x4207C0 maps the left/right/middle button-down messages
                // to the same parent 0x40A immediate-menu transition.
                loco::intro::skipAll();
                break;
            }
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

        /* Pump-once semantics: CGWND_PumpMessages(1) (loading progress,
         * filter != 0) processes the pending event batch and returns, like
         * the original PeekMessage(PM_REMOVE) single pass.  The main loop
         * uses filter == 0 and never hits this. */
        if (filter != 0) {
            return;
        }

        /* Check for quit */
        if (event.type == SDL_EVENT_QUIT) {
            break;
        }

        // CGWND_SetMode(10) has queued the exit sweep 0x5026 and
        // stopped background music.  Return immediately so the caller can
        // tear down the window, then drain audio headlessly — the original
        // posts WM_CLOSE right after starting playback and lets DirectSound
        // hardware buffers outlive the window.
        if (g_game_mode == 10) {
            stop_text_input();
            return;
        }

        // Host-only replacement for original MCIWnd playback. Do not present
        // the primary menu canvas until all launch videos have ended/skipped.
        if (introWasActive) {
            if (loco::intro::isActive()) {
                if (!loco::intro::pumpAndRender()) CGWND_SetMode(2);
            } else {
                CGWND_SetMode(2);
            }
            SDL_Delay(1);
            continue;
        }

        /* Host async task (original worker thread): run one pending
         * loading/post-load task before the per-frame tick. */
        if (loco::host::HasPendingAsyncTask()) {
            loco::host::RunPendingAsyncTask();
        }

        /* Game logic tick */
        GameLoop_FrameUpdate();
        SDL3_GameAudioPump();

        // Host-only replacement for the x86 UIPANEL/offscreen-surface path:
        // compose original EditWindow resources in logical canvas coordinates.
        EditWindow* const menu = active_host_menu();
        if (menu != nullptr) menu->hostRenderFrame();

        // The primary DirectDraw target is now the sole frame source. The
        // fallback preserves the launch screen until any target exists.
        bool presented = SDL3_PresentPrimarySurface();
        if (!presented && g_renderer) {
            SDL_SetRenderDrawColor(g_renderer, 0, 40, 80, 255);
            SDL_RenderClear(g_renderer);
            SDL_RenderPresent(g_renderer);
            presented = true;
        }

        // Emit readiness only after SDL has presented the corresponding frame.
        // This is passive test observability and cannot drive the state machine.
        if (presented && menu != nullptr) {
            if (menu->dialogState == 0 || menu->dialogState == 7) {
                loco::host_test::emit_screen_presented("main_menu", menu->dialogState);
            } else if (menu->dialogState == 3 || menu->dialogState == 4 ||
                       menu->dialogState == 5) {
                loco::host_test::emit_screen_presented(
                    menu->dialogState == 5 ? "multiplayer_lobby" : "game_setup",
                    menu->dialogState);
            }
        }
    }
}

void CGWND_PumpMessages(void* cgwnd_ptr, uint8_t filter)
{
    (void)cgwnd_ptr;
    PumpMessages_SDL3(filter);
}

#endif
