from __future__ import annotations

import pytest


pytestmark = [pytest.mark.integration, pytest.mark.gui]


def test_launches_main_menu_and_quits_cleanly(game):
    game.wait_for_event("process_started")
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.screenshot("main-menu")

    game.click_logical(430, 700, "main-menu quit")
    game.wait_for_event("mode_changed", new_mode=10)
    game.wait_for_clean_exit()


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
