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
extern int g_game_mode;

void CGWND_PumpMessages(void* cgwnd_ptr, uint8_t filter);

static EditWindow* active_host_menu()
{
    return g_game_mode == 2 ? static_cast<EditWindow*>(g_ui_main) : nullptr;
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
                /* Host-only event-stream consistency: this raw ESC exit
                 * IS the mode-10 quit path (the same contract the
                 * focused-edit 0x420C19 branch satisfies via
                 * CGWND_SetMode(10)); the pump just skips the SetMode
                 * switch.  Emit the mode change so the test observer sees
                 * the same quit transition whether or not the edit field
                 * had focus (pre-existing Sway content-rect y-offset race
                 * could land the click 25px above the field and miss
                 * focus — the game still quit through mode 10). */
                extern int g_game_mode;
                loco::host_test::emit_mode_changed(g_game_mode, 10);
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
            if (Game* game = active_host_game()) {
                uint32_t packed = 0;
                if (host_pack_game_lparam(event.motion.x, event.motion.y, &packed)) {
                    game->screensaver_active = 1;      /* +0x8E, 0x4622D8 */
                    game->packed_mouse_pos = packed;   /* +0x90, 0x4622DF */
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
                if (Game* game = active_host_game()) {
                    uint32_t packed = 0;
                    host_pack_game_lparam_clamped(event.button.x, event.button.y, &packed);
                    game->click_on_selected = 1;          /* +0xE6, 0x462380 */
                    game->left_click_flag = 1;            /* +0xA4, 0x462387 */
                    game->left_click_screen_pos = packed; /* +0xA8, 0x46238E */
                    emit_town_input("left_click", packed);
                }
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                if (Game* game = active_host_game()) {
                    uint32_t packed = 0;
                    host_pack_game_lparam_clamped(event.button.x, event.button.y, &packed);
                    game->right_click_flag = 1;            /* +0xB4, 0x4623E4 */
                    game->right_click_screen_pos = packed; /* +0xB8, 0x4623EB */
                    emit_town_input("right_click", packed);
                }
            }
            if (filter != 0) { isMouseEvent = true; }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (Game* game = active_host_game()) {
                    uint32_t packed = 0;
                    host_pack_game_lparam_clamped(event.button.x, event.button.y, &packed);
                    game->click_on_selected = 0;          /* +0xE6, 0x4623B2 */
                    game->mouse_move_flag = 1;            /* +0xC4, 0x4623B9 */
                    game->mouse_move_screen_pos = packed; /* +0xC8, 0x4623C0 */
                    emit_town_input("left_release", packed);
                }
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                if (Game* game = active_host_game()) {
                    uint32_t packed = 0;
                    host_pack_game_lparam_clamped(event.button.x, event.button.y, &packed);
                    game->mouse_drag_flag = 1;            /* +0xD4, 0x46240F */
                    game->mouse_drag_screen_pos = packed; /* +0xD8, 0x462416 */
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
