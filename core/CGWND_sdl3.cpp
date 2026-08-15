/**
 * CGWND_sdl3.cpp — SDL3-specific implementations for CGWND
 */

// Status: TRANSCRIBED
#ifndef _WIN32

#include "CGWND.h"
#include "Game.h"
#include "../ui/EditWindow.h"
#include "sdl3_ddraw.h"
#include "sdl3_window.h"
#include "sdl3_game_audio.h"
#include "sdl3_intro_video.h"
#include "host_test_events.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdint>

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
extern void* g_game;
extern void* g_main_window;
extern int g_game_mode;

void CGWND_PumpMessages(void* cgwnd_ptr, uint8_t filter);

static EditWindow* active_host_menu()
{
    return g_game_mode == 2 ? static_cast<EditWindow*>(g_ui_main) : nullptr;
}

/* The real main CGWND window's HWND, for calling CGWND_MainWndProc
 * directly — CGWND::WndProc's own entry guard requires the exact
 * `this->hWnd` it was constructed with, not any other window handle. */
static HWND main_wnd_hwnd()
{
    return (g_main_window != nullptr) ? static_cast<CGWND*>(g_main_window)->hWnd : nullptr;
}

/**
 * active_host_game — mode-3/9 (Town/gameplay) counterpart to
 * active_host_menu(). GameLoop_FrameUpdate (0x45C3C0, core/GameLoop.cpp)
 * treats modes 3 and 9 identically for the per-frame world tick, and
 * Game::Update()'s has_event dispatch reaches Game::UpdateInputState() for
 * mode 3 / falls through to the generic PlaySound(0x1400) path for mode 9
 * (core/Game.cpp:504-513) — both need live input, neither is a menu mode.
 */
static Game* active_host_game()
{
    return (g_game_mode == 3 || g_game_mode == 9)
               ? static_cast<Game*>(g_game)
               : nullptr;
}

/**
 * Host translation of MainWndProc's (0x4618C0) game-mode mouse dispatch
 * into Game's per-frame input fields (Game.h). Verified against the
 * disassembly (lego-loco-unpacked/Exe/loco.exe):
 *
 *   WM_MOUSEMOVE   0x4622D8-0x4622DF  screensaver_active=1 (+0x8E),
 *                                     packed_mouse_pos=lParam (+0x90)
 *   WM_LBUTTONDOWN 0x462387-0x46238E  click_on_selected=1 (+0xE6),
 *   / DBLCLK                         left_click_flag=1 (+0xA4),
 *                                     left_click_screen_pos=lParam (+0xA8)
 *   WM_LBUTTONUP   0x4623B2-0x4623C0  click_on_selected=0 (+0xE6),
 *                                     mouse_move_flag=1 (+0xC4),
 *                                     mouse_move_screen_pos=lParam (+0xC8)
 *   WM_RBUTTONDOWN 0x4623E4-0x4623EB  right_click_flag=1 (+0xB4),
 *   / DBLCLK                         right_click_screen_pos=lParam (+0xB8)
 *   WM_RBUTTONUP   0x46240F-0x462416  mouse_drag_flag=1 (+0xD4),
 *                                     mouse_drag_screen_pos=lParam (+0xD8)
 *
 * lParam packs X in the low word, Y in the high word (Win32 MAKELPARAM);
 * Game::ScreenToWorld and friends unpack the same way (core/Game.cpp).
 * The original additionally calls SetFocus/SetForegroundWindow on
 * WM_LBUTTONDOWN — pure Win32 window-manager z-order/focus with no
 * game-state effect, and out of scope under a Wayland compositor.
 *
 * SDL delivers window/display-space float coordinates; project them into
 * the same primary-canvas pixel space DDRAW/Town render into (the same
 * conversion EditWindow::hostHandlePointer uses for its mode-2 path) so
 * lParam packing matches what the original client-rect coordinates would
 * have been.
 *
 * All five original writes above are unconditional — Windows has no
 * "outside the client area" concept for WM_MOUSEMOVE/BUTTONDOWN/BUTTONUP
 * delivery, and happily hands a mouse-captured drag negative/oversized
 * lParam coordinates. Only WM_MOUSEMOVE is dropped here when the pointer
 * projects outside the letterboxed canvas (host_pack_game_lparam,
 * strict) — defensible because without real capture/grab semantics the
 * host wouldn't have generated that motion event over the game window at
 * all. BUTTONDOWN/BUTTONUP use host_pack_game_lparam_clamped, which never
 * rejects (clamps into the canvas instead), so a press or release that
 * lands in a letterbox bar still writes real, if edge-clamped, position
 * data instead of silently dropping the whole event — dropping a button
 * transition (as opposed to a move) would desync flags like
 * click_on_selected that only ever get one paired set/clear per
 * press/release.
 */
static bool host_pack_game_lparam(float display_x, float display_y,
                                   uint32_t* out_packed)
{
    float canvas_x = 0.0f;
    float canvas_y = 0.0f;
    if (!SDL3_DisplayToPrimaryCanvas(display_x, display_y, &canvas_x, &canvas_y)) {
        return false;
    }
    const uint16_t x = static_cast<uint16_t>(static_cast<int32_t>(canvas_x));
    const uint16_t y = static_cast<uint16_t>(static_cast<int32_t>(canvas_y));
    *out_packed = (static_cast<uint32_t>(y) << 16) | x;
    return true;
}

static bool host_pack_game_lparam_clamped(float display_x, float display_y,
                                           uint32_t* out_packed)
{
    float canvas_x = 0.0f;
    float canvas_y = 0.0f;
    if (!SDL3_DisplayToPrimaryCanvasClamped(display_x, display_y, &canvas_x, &canvas_y)) {
        return false;
    }
    const uint16_t x = static_cast<uint16_t>(static_cast<int32_t>(canvas_x));
    const uint16_t y = static_cast<uint16_t>(static_cast<int32_t>(canvas_y));
    *out_packed = (static_cast<uint32_t>(y) << 16) | x;
    return true;
}

/* Unpacks the same way Game::ScreenToWorld and friends do (low word X,
 * high word Y) so the emitted event reflects the actual projected
 * position, not just that some dispatch happened. */
static void emit_town_input(const char* kind, uint32_t packed)
{
    const int x = static_cast<int>(packed & 0xFFFF);
    const int y = static_cast<int>(packed >> 16);
    loco::host_test::emit_town_input_dispatched(kind, g_game_mode, x, y);
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
            /* Real dispatch: CGWND::WndProc's WM_CLOSE case
             * (HandleStartupModeMessage/HandleGameplayMessage) defers to
             * the real quit state machine (CGWND_ShutdownOrDeferToMode10)
             * instead of tearing down immediately here — matching the
             * original's WM_CLOSE contract. The loop notices real
             * completion via WM_QUIT in the post-event drain below. */
            CGWND_MainWndProc(main_wnd_hwnd(), WM_CLOSE, 0, 0);
            break;
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
                /* Host-only event-stream consistency: this raw ESC exit
                 * satisfies the same contract as the focused-edit 0x420C19
                 * branch (EditWindow::hostHandleKey), which already calls
                 * the real CGWND_SetMode(10) when Escape is pressed with
                 * the field focused — this is the fallback for when the
                 * edit field didn't have focus (pre-existing Sway
                 * content-rect y-offset race). CGWND_SetMode(10) now posts
                 * a real WM_CLOSE (core/CGWND.cpp), so this goes through
                 * the same deferred quit machinery as every other close
                 * path instead of returning immediately.
                 *
                 * Deliberately NOT routed through HandleGameplayMessage's
                 * WM_KEYDOWN case for gameplay modes (3+): that dispatch
                 * also reaches g_active_panel->HandleKey() (unverified
                 * receiver) and the 'W' fullscreen-toggle case
                 * (CGWND_ToggleFullscreen has no definition anywhere in
                 * this tree — an unresolved-symbol call under this
                 * project's --unresolved-symbols=ignore-all link setting,
                 * i.e. a real crash risk, not just a missing feature).
                 * Full gameplay keyboard dispatch needs its own pass once
                 * those are resolved. */
                extern int g_game_mode;
                loco::host_test::emit_mode_changed(g_game_mode, 10);
                CGWND_SetMode(10);
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
            if (active_host_game()) {
                uint32_t packed = 0;
                if (host_pack_game_lparam(event.motion.x, event.motion.y, &packed)) {
                    /* Real dispatch: CGWND::WndProc's WM_MOUSEMOVE case
                     * (HandleGameplayMessage, 0x200) writes the identical
                     * screensaver_active/packed_mouse_pos fields this used
                     * to write by hand — see its own doc comment for the
                     * exact original addresses. Going through real dispatch
                     * also picks up the mode-8/mode-4 sub-cases this direct
                     * write never had. */
                    CGWND_MainWndProc(main_wnd_hwnd(), WM_MOUSEMOVE, 0,
                                       static_cast<LPARAM>(packed));
                    emit_town_input("mouse_move", packed);
                }
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
                if (active_host_game()) {
                    uint32_t packed = 0;
                    host_pack_game_lparam_clamped(event.button.x, event.button.y, &packed);
                    /* Real dispatch: WM_LBUTTONDOWN (HandleGameplayMessage,
                     * 0x201) writes click_on_selected/left_click_flag/
                     * left_click_screen_pos and additionally does the real
                     * SetFocus/SetForegroundWindow this manual write never
                     * did. */
                    CGWND_MainWndProc(main_wnd_hwnd(), WM_LBUTTONDOWN, 0,
                                       static_cast<LPARAM>(packed));
                    emit_town_input("left_click", packed);
                }
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                if (active_host_game()) {
                    uint32_t packed = 0;
                    host_pack_game_lparam_clamped(event.button.x, event.button.y, &packed);
                    /* Real dispatch: WM_RBUTTONDOWN (0x204) writes
                     * right_click_flag/right_click_screen_pos. */
                    CGWND_MainWndProc(main_wnd_hwnd(), WM_RBUTTONDOWN, 0,
                                       static_cast<LPARAM>(packed));
                    emit_town_input("right_click", packed);
                }
            }
            if (filter != 0) { isMouseEvent = true; }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (active_host_game()) {
                    uint32_t packed = 0;
                    host_pack_game_lparam_clamped(event.button.x, event.button.y, &packed);
                    /* Real dispatch: WM_LBUTTONUP (0x202) writes
                     * click_on_selected=0/mouse_move_flag/
                     * mouse_move_screen_pos. */
                    CGWND_MainWndProc(main_wnd_hwnd(), WM_LBUTTONUP, 0,
                                       static_cast<LPARAM>(packed));
                    emit_town_input("left_release", packed);
                }
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                if (active_host_game()) {
                    uint32_t packed = 0;
                    host_pack_game_lparam_clamped(event.button.x, event.button.y, &packed);
                    /* Real dispatch: WM_RBUTTONUP (0x205) writes
                     * mouse_drag_flag/mouse_drag_screen_pos. */
                    CGWND_MainWndProc(main_wnd_hwnd(), WM_RBUTTONUP, 0,
                                       static_cast<LPARAM>(packed));
                    emit_town_input("right_release", packed);
                }
            }
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

        /* Drain posted (non-SDL-sourced) messages: WM_TIMER from the real
         * present-timer (SetTimer(hWnd, 0x47, 150, nullptr), started by
         * CGWND::initMode1 — sets g_present_due via CGWND::WndProc's real
         * WM_TIMER case), and WM_CLOSE re-posted by CGWND_SetMode(10)'s
         * deferred quit path. A real WM_QUIT means CGWND_ShutdownOrDefer-
         * ToMode10 completed real teardown (PostQuitMessage) — matching
         * WinMain's own `msg.message != WM_QUIT` loop-exit test, this is
         * the one real signal that ends the pump. */
        {
            MSG queued;
            bool quit_pending = false;
            int drain_count = 0;
            while (PeekMessageA(&queued, nullptr, 0, 0, PM_REMOVE)) {
                if (queued.message == WM_QUIT) { quit_pending = true; break; }
                TranslateMessage(&queued);
                DispatchMessageA(&queued);
                /* Defensive bound: a real message loop drains until the
                 * queue is empty, but a WndProc bug that re-posts a message
                 * to itself every dispatch (a real failure mode this pass
                 * hit once already) would otherwise spin here forever. */
                if (++drain_count > 1000) {
                    fprintf(stderr, "[WARN] PumpMessages_SDL3: message drain "
                                    "exceeded 1000 in one iteration, deferring "
                                    "the rest to next iteration\n");
                    break;
                }
            }
            if (quit_pending) {
                stop_text_input();
                return;
            }
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

        /* Game logic tick. Real WinMain gates this on a second, independent
         * timer flag (DAT_00485444, a 28ms winmm timeSetEvent callback,
         * LAB_0045c520/0x45C520) — deliberately NOT reproduced here:
         * timeSetEvent is currently a total no-op stub (shared/stubs_impl.cpp)
         * and LAB_0045c520 itself is an unimplemented assert-stub, so gating
         * on that flag would freeze gameplay entirely rather than throttle
         * it. Ticking every host loop iteration (as before) until that
         * dependency chain is reconstructed for real. */
        GameLoop_FrameUpdate();
        SDL3_GameAudioPump();

        // Host-only replacement for the x86 UIPANEL/offscreen-surface path:
        // compose original EditWindow resources in logical canvas coordinates.
        EditWindow* const menu = active_host_menu();
        if (menu != nullptr) menu->hostRenderFrame();

        /* Presentation cadence: the real WinMain only gates CGWND_Present
         * on g_present_due (WM_TIMER id 0x47) in its gameplay-phase loop —
         * the menu-phase (mode 2) blocking GetMessageA loop has no present
         * call in the evidenced original at all, so mode 2 keeps presenting
         * every host iteration here (also avoids a blank first-launch menu
         * screen before mode 1 has ever started the 0x47 timer — it isn't
         * running yet the very first time mode 2 is shown). Mode 1 (loading
         * spinner) and every gameplay-adjacent mode share the same real
         * 0x47 timer, so they're gated the same way. */
        const bool should_present = (g_game_mode == 2) || (g_present_due != 0);
        if (should_present) {
            g_present_due = 0;

            // The primary DirectDraw target is now the sole frame source. The
            // fallback preserves the launch screen until any target exists.
            bool presented = SDL3_PresentPrimarySurface();
            if (!presented && g_renderer) {
                SDL_SetRenderDrawColor(g_renderer, 0, 40, 80, 255);
                SDL_RenderClear(g_renderer);
                SDL_RenderPresent(g_renderer);
                presented = true;
            }

            // Emit readiness only after SDL has presented the corresponding
            // frame. This is passive test observability and cannot drive
            // the state machine.
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
        } else {
            // Bounded, message-interruptible-in-spirit wait, matching the
            // real loop's MsgWaitForMultipleObjects(0, NULL, 0, 3, 0xBF) —
            // avoids spinning at full CPU while waiting for the next
            // present/tick.
            SDL_Delay(3);
        }
    }
}

void CGWND_PumpMessages(void* cgwnd_ptr, uint8_t filter)
{
    (void)cgwnd_ptr;
    PumpMessages_SDL3(filter);
}

#endif
