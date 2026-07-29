from __future__ import annotations

import getpass
import time

import pytest


pytestmark = [pytest.mark.integration, pytest.mark.gui]


@pytest.mark.parametrize(
    "game", [{"SDL_AUDIODRIVER": "dummy", "LEGO_LOCO_SKIP_INTRO": "0"}], indirect=True,
)
def test_launch_intro_any_key_skips_remaining_videos_and_opens_main_menu(game):
    """0x4207C0 posts 0x40A for every key: no later intro may begin."""
    game.wait_for_event("process_started")
    started = game.wait_for_event(
        "intro_video_started", index=0, path="art-res/video/legospin.avi"
    )
    frame = game.wait_for_event("intro_video_frame", index=0, timeout=15)
    assert (frame["width"], frame["height"]) == (640, 480)
    # Avoid recording the black decoder preroll frame as visual evidence.
    time.sleep(1)
    game.screenshot("intro-legospin-before-any-key-skip")

    # The original MCI child subclass handles every WM_KEYDOWN, rather than
    # special-casing Escape. The parent immediately enters menu state 7.
    game.press_key("A")
    game.wait_for_event("intro_video_finished", after_sequence=started["sequence"],
                        index=0, skipped=True)
    game.wait_for_event("intro_sequence_complete")
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)

    started_indices = [
        item["index"] for item in game.events()
        if item.get("event") == "intro_video_started"
    ]
    assert started_indices == [0]



def latest_sequence(game) -> int:
    return max((int(item.get("sequence", 0)) for item in game.events()), default=0)


def select_multiplayer(game) -> None:
    # 0x407/0x408 are singleup/singledown. Only after the left click does
    # 0x422C60 enable 0x409 multipleup; that second click renders 0x40A.
    game.click_logical(600, 550, "select single player")
    game.click_logical(780, 550, "select multiplayer")


@pytest.mark.parametrize(
    "game", [{"SDL_AUDIODRIVER": "dummy"}], indirect=True,
)
def test_main_menu_exit_plays_click_sound_and_exits_cleanly(game):
    """Scenario 1: original 0x405 Back/Exit queues 0x5015 then exits."""
    game.wait_for_event("process_started")
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.screenshot("main-menu-before-exit")

    before_exit = latest_sequence(game)
    game.click_logical(430, 700, "main-menu back/exit")
    game.wait_for_event("audio_queued", after_sequence=before_exit, resource_id=0x5015)
    game.wait_for_event("mode_changed", timeout=5, new_mode=10)
    game.wait_for_clean_exit(timeout=5)


@pytest.mark.parametrize(
    "game", [{"SDL_AUDIODRIVER": "dummy"}], indirect=True,
)
def test_singleplayer_go_plays_click_sound_and_leaves_main_menu(game):
    """Scenario 2: single selection + test + Go reaches the local panel."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.click_logical(600, 550, "select single player")
    game.screenshot("main-menu-singleplayer-selected")
    game.click_logical(600, 720, "player-name field")
    game.clear_text()
    game.type_text("test")

    before_go = latest_sequence(game)
    game.click_logical(925, 700, "main-menu go")
    game.wait_for_event("audio_queued", after_sequence=before_go, resource_id=0x5015)
    game.wait_for_event("screen_presented", screen="game_setup", dialog_state=4)
    assert not any(
        item.get("event") == "screen_presented"
        and item.get("screen") == "multiplayer_lobby"
        and item.get("dialog_state") == 5
        for item in game.events()
    )


@pytest.mark.parametrize(
    "game", [{"SDL_AUDIODRIVER": "dummy"}], indirect=True,
)
def test_multiplayer_empty_entry_defaults_os_name_and_opens_grid(game):
    """Scenario 3: untouched entry keeps PlayerRecord's OS-name fallback."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    select_multiplayer(game)
    game.screenshot("main-menu-multiplayer-selected-default-name")

    # Do not focus, clear, or type into the field: UI_MainMenu_Show copies the
    # PlayerRecord value initialized by 0x452E10's GetUserNameA fallback.
    before_go = latest_sequence(game)
    game.click_logical(925, 700, "main-menu go with default name")
    game.wait_for_event("audio_queued", after_sequence=before_go, resource_id=0x5015)
    committed = game.wait_for_event("player_name_committed")
    assert committed["name"] == getpass.getuser()[:11]
    game.wait_for_event("screen_presented", screen="multiplayer_lobby", dialog_state=5)
    game.screenshot("multiplayer-grid-default-name")


def test_multiplayer_host_game_go_back_back_exits_cleanly(game):
    """Scenario 4: host selection → grid → Back → menu Back → clean exit."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    select_multiplayer(game)

    # Resource 0x40B is hostup. Its state byte is consumed by the subsequent
    # Go path; 0x40C is the visibly selected hostdown frame.
    game.click_logical(950, 500, "select host game")
    game.screenshot("main-menu-host-game-selected")
    game.click_logical(925, 700, "main-menu go")
    game.wait_for_event("screen_presented", screen="multiplayer_lobby", dialog_state=5)
    game.screenshot("multiplayer-grid-host-game")

    game.click_logical(850, 720, "multiplayer back")
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=7)
    game.screenshot("main-menu-after-grid-back")

    game.click_logical(430, 700, "main-menu back/exit")
    game.wait_for_event("mode_changed", new_mode=10)
    game.wait_for_clean_exit()


@pytest.mark.parametrize(
    "game", [{"SDL_AUDIODRIVER": "dummy"}], indirect=True,
)
def test_multiplayer_layout_choices_update_grid_geometry(game):
    """Host-only provider projects choices into the original 0x43FC50 inputs."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    select_multiplayer(game)
    game.click_logical(925, 700, "main-menu go")
    game.wait_for_event("screen_presented", screen="multiplayer_lobby", dialog_state=5)
    game.screenshot("multiplayer-layout-3x3-default")

    # drawLayoutList (0x4094B0) uses 12px padding and a measured-font-height
    # plus four pixel row step. The host uses its recovered 18px line step.
    for index, columns, rows in ((1, 2, 2), (2, 2, 1), (3, 3, 1), (4, 3, 2)):
        before = latest_sequence(game)
        game.click_logical(100, 125 + index * 18, f"select {columns}x{rows}")
        selected = game.wait_for_event(
            "layout_selected", after_sequence=before,
            columns=columns, rows=rows, slots=columns * rows,
        )
        assert selected["slots"] == columns * rows
        game.screenshot(f"multiplayer-layout-{columns}x{rows}")


def test_main_menu_escape_exits_cleanly(game):
    """Keep coverage for the focused EDIT Escape branch at 0x420C19."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.click_logical(600, 720, "player-name field")
    game.press_key("Escape")
    game.wait_for_event("mode_changed", timeout=5, new_mode=10)
    game.wait_for_clean_exit(timeout=5)


def test_game_setup_lobby_search_and_exit(game):
    """Retain the empty DirectPlay search boundary regression."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    select_multiplayer(game)
    game.click_logical(600, 720, "player-name field")
    game.clear_text()
    game.type_text("Agent")
    game.click_logical(925, 700, "main-menu go")

    game.wait_for_event("screen_presented", screen="multiplayer_lobby", dialog_state=5)
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
