from __future__ import annotations

import pytest


pytestmark = [pytest.mark.integration, pytest.mark.gui]


@pytest.mark.parametrize(
    "game", [{"SDL_AUDIODRIVER": "dummy"}], indirect=True,
)
@pytest.mark.parametrize("terminal_input", ["exit-control", "escape-focused"])
def test_main_menu_mode_10_terminal_inputs_drain_exit_audio_and_shut_down(
    game, terminal_input,
):
    """Mode-2 terminal inputs must not spin forever while the mode-10 WAV is queued."""
    game.wait_for_event("process_started")
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.screenshot(f"main-menu-before-{terminal_input}")

    if terminal_input == "exit-control":
        # Original +0x14C / resource-0x405 control → CGWND_SetMode(10).
        game.click_logical(430, 700, "main-menu exit")
    else:
        # The focused native EDIT Escape branch also reaches CGWND_SetMode(10).
        game.click_logical(600, 720, "player-name field")
        game.press_key("Escape")

    game.wait_for_event("mode_changed", timeout=5, new_mode=10)
    game.wait_for_clean_exit(timeout=5)


def test_game_setup_lobby_search_and_exit(game):
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.screenshot("main-menu-before-accept")

    game.click_logical(600, 720, "player-name field")
    game.clear_text()
    game.type_text("Agent")
    game.click_logical(925, 700, "main-menu accept")

    game.wait_for_event("screen_presented", screen="game_setup", dialog_state=4)
    game.screenshot("game-setup-lobby")

    game.click_logical(900, 620, "lobby search")
    game.wait_for_event("search_completed", sessions=0)
    game.screenshot("game-setup-search-empty")

    game.click_logical(850, 720, "lobby exit")
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=7)
    game.screenshot("main-menu-after-lobby-exit")

    game.click_logical(430, 700, "main-menu quit")
    game.wait_for_event("mode_changed", new_mode=10)
    game.wait_for_clean_exit()
