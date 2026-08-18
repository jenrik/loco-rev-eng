from __future__ import annotations

import getpass
import os
import subprocess
import time
from pathlib import Path

import pytest


pytestmark = [pytest.mark.integration, pytest.mark.gui]

# Resolved via LEGO_LOCO_BUILD_ROOT (set automatically by `meson test`) rather
# than a "build/"-relative literal, which only worked because the old
# Makefile happened to invoke pytest without changing directories.
_SDL3_NET_TRANSPORT_TEST = str(Path(os.environ["LEGO_LOCO_BUILD_ROOT"]) / "tests" / "sdl3_net_transport_test")


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
    # The mapping gate can precede Sway adding its title bar by one frame; wait
    # until client geometry has settled before converting logical coordinates.
    time.sleep(0.2)
    # 0x407/0x408 are singleup/singledown. Only after the left click does
    # 0x422C60 enable 0x409 multipleup; that second click renders 0x40A.
    game.click_logical(600, 550, "enable multiplayer selector")
    # The persisted config can already have the left selector active, in which
    # case that click is intentionally ignored. Only the final state matters.
    time.sleep(0.1)
    game.click_logical(780, 550, "select multiplayer")
    game.wait_for_event("menu_mode_selected", multiplayer=True)
    # Present the resulting 0x40A frame before Accept consumes the state.
    game.screenshot("main-menu-multiplayer-selected")


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
    """Scenario 2: single selection + test + Go enters mode-1 loading, then
    the mode-3 cone (original 0x4227DA → CGWND_SetMode(1) → InitMode1 →
    loading task → CGWND_SetMode(3)).  No setup panel is shown for
    single-player, matching the binary."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.click_logical(600, 550, "select single player")
    game.screenshot("main-menu-singleplayer-selected")
    game.click_logical(600, 720, "player-name field")
    game.clear_text()
    game.type_text("test")

    before_go = latest_sequence(game)
    game.click_logical(925, 700, "main-menu go")
    game.wait_for_event("audio_queued", after_sequence=before_go, resource_id=0x5015)
    game.wait_for_event("mode_changed", after_sequence=before_go, new_mode=1)
    # Mode-1 loading screen: Town::init_overlay_sprite/Cursor::init_background/
    # PostcardAlbum::InitWindowSurface now run un-stubbed on host (see
    # core/InitMode1.cpp, PROGRESS.md's RESDATA/ResourceObject unification
    # entry) -- screenshot this transient window, not just the mode-3
    # settle, so a regression that silently no-ops them again would show up
    # as a visual diff here even if it doesn't crash.
    game.screenshot("singleplayer-mode1-loading-screen")
    game.wait_for_event("mode_changed", new_mode=3, timeout=10)
    assert not any(
        item.get("event") == "screen_presented"
        and item.get("screen") == "game_setup"
        for item in game.events()
    )


@pytest.mark.parametrize(
    "game", [{"SDL_AUDIODRIVER": "dummy"}], indirect=True,
)
def test_singleplayer_accept_reaches_mode3_without_crashing(game):
    """Regression for 3d0533a: Game::Update dereferenced the Game object's
    resource field (+0x40, never written by any code) on the first mode-3
    frame (0x405C57). BootstrapMode3Core now clears the host Game's
    initialized flag so Entity::Update's early-return guard applies; hold in
    mode 3 long enough to catch a frame-loop regression, matching the manual
    soak used to verify the original fix."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.click_logical(600, 550, "select single player")
    game.click_logical(600, 720, "player-name field")
    game.clear_text()
    game.type_text("test")
    game.click_logical(925, 700, "main-menu accept/ok")
    game.wait_for_event("mode_changed", new_mode=3, timeout=10)
    time.sleep(2)
    assert game.is_alive(), game._failure("game crashed after entering mode 3")
    game.screenshot("singleplayer-mode3-stable")


@pytest.mark.parametrize(
    "game", [{"SDL_AUDIODRIVER": "dummy"}], indirect=True,
)
def test_singleplayer_enter_key_commit_reaches_mode3_without_crashing(game):
    """Regression for lego_loco.core (coredumpctl PID 607663/613744/614018,
    2026-08-18): SIGSEGV in PostcardAlbum::InitWindowSurface.

    Two coredumps of this exact build converge on the identical crash site
    from two different UI triggers:

        EditWindow::hostHandleKey(key_code=13) -> hostCommitPlayerName
          -> CGWND_SetMode(1) -> CGWND::initMode1
          -> PostcardAlbum::InitWindowSurface  [crash]

        host_complete_lobby_control(Go) -> CGWND_SetMode(1) -> ...same tail

    gdb on the core showed `this->is_high_res == 1`, so `resId = 0x3C0B`;
    `album_bg_resource == 0x0` -- i.e. `g_resmgr.GetById(0x3C0B)` returned 0
    and the very next line unconditionally dereferences it
    (ui/PostcardAlbum.cpp:670, `static_cast<ResourceObject*>(resource)
    ->Lock(0, 0)`).

    This is NOT a missing null check to add at that call site: Ghidra's
    disassembly of the original 0x404720 is the identical 23-instruction,
    no-null-check sequence (PUSH ESI; ...; CALL [EAX+4] on the raw GetById
    result), so the original game made the same unchecked call and relied
    on the resource always being present. The defect is upstream, in
    ResourceManager::GetById (resources/ResourceManager.cpp, `// Status:
    TRANSCRIBED` -- not yet integrated into the canonical object model).
    One live-but-unverified lead worth recording so it isn't lost: the
    lazy-load path there computes `idx = typeEntry - this->resource_ptrs`
    (ResourceManager.cpp:1089) where `typeEntry` points into
    `resource_type_idx`, not `resource_ptrs` -- suspicious, but a from-
    scratch `ResourceManager` probe and the real `g_resmgr` diverged under
    the resulting undefined behavior (out-of-bounds `resource_ptrs[idx]`
    access) in ways this investigation could not fully reconcile, so treat
    it as a lead for the next debugging session, not a confirmed
    mechanism.

    test_singleplayer_accept_reaches_mode3_without_crashing (immediately
    above) already reaches this same crash via a mouse click on
    accept/ok -- both it and this test independently fail on this exact
    SIGSEGV as of this commit (confirmed via coredumpctl during this
    investigation). This test exists separately because it drives the
    *physical Enter key* path (EditWindow::hostHandleKey), matching
    lego_loco.core's primary stack trace exactly, so a future fix that
    only patches the button's render-poll path would not silently leave
    this one uncovered."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.click_logical(600, 550, "select single player")
    game.click_logical(600, 720, "player-name field")
    game.clear_text()
    game.type_text("test")
    game.press_key("Return")
    game.wait_for_event("mode_changed", new_mode=3, timeout=10)
    time.sleep(2)
    assert game.is_alive(), game._failure("game crashed after Enter-key name commit")
    game.screenshot("singleplayer-enter-key-mode3-stable")


def test_singleplayer_mode3_mouse_input_reaches_game(game):
    """Regression for BUG-mode-3-render-freeze.md: PumpMessages_SDL3 only
    forwarded SDL input while active_host_menu() was non-null (mode 2), so
    every mouse event was silently dropped once Accept transitioned to mode
    3 — Game::Update()'s has_event gate (core/Game.cpp:427) never went true
    and the screen looked frozen forever. CGWND_sdl3.cpp now translates SDL
    mouse motion/click/release into Game's input fields for modes 3/9,
    mirroring MainWndProc's (0x4618C0) WM_MOUSEMOVE/WM_LBUTTONDOWN/
    WM_LBUTTONUP/WM_RBUTTONDOWN/WM_RBUTTONUP dispatch. Assert the
    town_input_dispatched host_test event fires for a move, with the
    packed position it decoded matching the logical coordinate moved to
    (within display/canvas round-trip rounding) — not just that some
    dispatch happened, but that host_pack_game_lparam's projection is
    correct, the only genuinely new coordinate-math logic in the fix.

    BUG-mode3-input-processing-crashes.md tracked a chain of crashes on
    this exact path (Game::UpdateInputState -> ... -> ChildWindow
    construction), stopping at UI_CreateChildWindow's host
    assert(false) reached via BuildingDescriptorEditor's constructor.
    That assert is gone now that ChildWindow/CursorEditWindow/
    TrainStation/BuildingDescriptorEditor are real derived classes with
    working host-path constructors (see ui/UI_ChildWindow.h's class
    hierarchy) — this is the regression test for that fix. It only
    checks the immediate aftermath of the click, not sustained survival:
    see test_singleplayer_mode3_click_reaches_wndproc_stream_stub below
    for why a real click still aborts the process a little further down
    the same call chain, past this test's own 1s window."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.click_logical(600, 550, "select single player")
    game.click_logical(600, 720, "player-name field")
    game.clear_text()
    game.type_text("test")
    game.click_logical(925, 700, "main-menu accept/ok")
    game.wait_for_event("mode_changed", new_mode=3, timeout=10)
    time.sleep(1)

    before = latest_sequence(game)
    game.move_logical(400, 300, "town view hover")
    move_event = game.wait_for_event(
        "town_input_dispatched", after_sequence=before, kind="mouse_move"
    )
    assert move_event["game_mode"] == 3
    # Display/canvas round-trip (logical -> display px -> canvas px) can be
    # off by rounding; a couple of pixels is noise, a wrong axis or a
    # broken scale factor is not.
    assert abs(move_event["x"] - 400) <= 2, move_event
    assert abs(move_event["y"] - 300) <= 2, move_event

    game.click_logical(400, 300, "town view click")
    time.sleep(1)
    game.assert_alive("mode-3 click past ChildWindow construction")


def test_singleplayer_mode3_click_reaches_wndproc_stream_stub(game):
    """Longer-soak extension of test_singleplayer_mode3_mouse_input_reaches_game,
    added to reliably reproduce a then-open crash one step further down the
    same call chain that test only samples for 1s.

    Live gdb backtrace (2026-08-11 session, coredumpctl on a manually driven
    repro of this exact click, matching commit 69f7556 HEAD) confirmed the
    full chain from a real click:

        Game::UpdateInputState -> Game::PlaySound(0x1400)
        -> ResourceManager::GetById -> ResourceManager::AddString
        -> BuildingDescriptorEditor_Ctor
        -> BuildingDescriptorEditor::BuildingDescriptorEditor
        -> BuildingDescriptorEditor::handle_edit_message
        -> BuildingDescriptorEditor::Render
        -> WNDPROC_CriticalSectionLock (resources/WndProcStream.cpp:291)
        -> assert(false) -> abort()

    WNDPROC_CriticalSectionLock was a deliberate loud stub (PROGRESS.md,
    "win32_stream.c removed (partial)"): its `stream` argument arrived as a
    raw `int streamHandle[2]` -- not because the WIN32_StreamOpen*
    functions themselves were unimplemented (5 of 6 already were, see
    PROGRESS.md's "win32-stream-cluster-implemented" entry), but because
    input/BuildingDescriptorEditor.cpp's path-load branch called
    WIN32_StreamOpenPath (a plain method call on an already-constructed
    WIN32_Stream, not a constructor) without first calling WIN32_StreamOpen
    to construct one. Root-caused and fixed 2026-08-11 (FUN_-sweep
    session) by disassembling the original 0x41E6E0 directly and
    confirming WIN32_StreamOpen runs unconditionally before
    WIN32_StreamOpenPath there; every other real, reachable caller
    (game/ScriptedObject.cpp, game/TrainStation.cpp, ui/CursorEditWindow.cpp,
    ui/UIPANEL_Surface.cpp, ui/HelpWnd.cpp) was individually audited and
    already did this correctly. WNDPROC_CriticalSectionLock itself was then
    un-stubbed to forward to the real, already-validated
    WNDPROC_Stream::ExtractToken(). See resources/WndProcStream.cpp's
    updated doc comment for the full writeup.

    This test now asserts the game SURVIVES the click (previously asserted
    the opposite while the bug was open) -- confirmed passing on a
    from-scratch build with 0 `call 0` linkage sites."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    game.click_logical(600, 550, "select single player")
    game.click_logical(600, 720, "player-name field")
    game.clear_text()
    game.type_text("test")
    game.click_logical(925, 700, "main-menu accept/ok")
    game.wait_for_event("mode_changed", new_mode=3, timeout=10)
    time.sleep(1)

    game.click_logical(400, 300, "town view click")
    time.sleep(3)
    game.assert_alive(
        "mode-3 click reaching BuildingDescriptorEditor::Render -> "
        "WNDPROC_CriticalSectionLock (resources/WndProcStream.cpp)"
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
    game.wait_for_event("transport_listening", port=23000)
    game.wait_for_event("netman_session_ready", player_id=1, hosting=True)

    admitted = subprocess.run(
        [_SDL3_NET_TRANSPORT_TEST, "--admit-client", "23000"],
        check=True, capture_output=True, text=True, timeout=10,
    )
    assert "ADMITTED id=2" in admitted.stdout
    game.wait_for_event("netman_player_joined", player_id=2)
    for packet_type in range(0x3EA, 0x3F0):
        game.wait_for_event("legacy_service_applied", packet_type=packet_type)
    game.wait_for_event(
        "legacy_attachment_updated", sender=2, train_type=0x55, sequence=2,
        subtype=2, attachment_bytes=4, final_bytes=3, complete=True
    )
    game.wait_for_event("netman_message_processed", type=0x0F)
    game.wait_for_event(
        "netman_vehicle_adopted", editor_count=2, network_id=0, list_depth=1
    )
    game.wait_for_event(
        "legacy_asset_owned", mode=2, type=7, byte_count=3,
        replaced=True, asset_count=1
    )
    game.wait_for_event(
        "legacy_asset_consumed", mode=2, type=7, byte_count=3
    )
    game.wait_for_event(
        "legacy_track_sessions_materialized", session_count=1,
        vehicle_count=1, editor_count=2, config=0x12345678,
        first_entry_count=128, first_signal_type=2
    )
    game.wait_for_event("netman_message_processed", type=0x13, flags=1)
    game.wait_for_event("netman_message_processed", type=0x15)
    game.wait_for_event("legacy_service_applied", packet_type=0x3F7, byte_count=12)
    game.wait_for_event("netman_message_processed", type=0x17, flags=1)
    game.wait_for_event("netman_message_processed", type=0x16, flags=1)
    game.wait_for_event(
        "netman_pixel_data_updated", slot=1, byte_count=4, width=2, height=2
    )
    game.screenshot("multiplayer-grid-host-game-ready")

    game.click_logical(850, 720, "multiplayer back")
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=7)
    game.screenshot("main-menu-after-grid-back")

    game.click_logical(430, 700, "main-menu back/exit")
    game.wait_for_event("mode_changed", new_mode=10)
    game.wait_for_clean_exit()


def test_multiplayer_ready_go_enters_loading_with_adopted_vehicle(game):
    """A real 0x3EC Vehicle remains valid across the recovered Go handoff."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    select_multiplayer(game)
    game.click_logical(950, 500, "select host game")
    game.click_logical(925, 700, "main-menu go")
    game.wait_for_event("transport_listening", port=23000)
    game.wait_for_event("netman_session_ready", player_id=1, hosting=True)

    admitted = subprocess.run(
        [_SDL3_NET_TRANSPORT_TEST, "--admit-client", "23000"],
        check=True, capture_output=True, text=True, timeout=10,
    )
    assert "ADMITTED id=2" in admitted.stdout
    game.wait_for_event(
        "netman_vehicle_adopted", editor_count=2, network_id=0, list_depth=1
    )
    game.wait_for_event(
        "legacy_asset_consumed", mode=2, type=7, byte_count=3
    )

    # Recovered 0x42A Go rectangle is [688,700,832,812).
    game.click_logical(720, 720, "multiplayer ready go")
    game.wait_for_event(
        "netman_route_cloned", source_editors=2, clone_editors=2, list_depth=2
    )
    game.wait_for_event("mode_changed", new_mode=1)
    time.sleep(0.5)
    assert game.is_alive(), game._failure("game exited after adopted-Vehicle Go")
    game.screenshot("multiplayer-adopted-vehicle-loading")


def test_multiplayer_ready_go_is_exposed_after_session_projection(game):
    """Listener projection exposes original 0x42A Go only after Netman readiness."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    select_multiplayer(game)
    game.click_logical(950, 500, "select host game")
    game.screenshot("main-menu-host-selected-ready-go")
    time.sleep(0.2)  # GAMESTATE_HandleClick preserves Sleep(0x96) feedback.
    game.click_logical(925, 700, "main-menu go")
    game.wait_for_event("netman_session_ready", player_id=1, hosting=True)
    game.screenshot("multiplayer-ready-go-visible")

    game.click_logical(850, 720, "multiplayer back")
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=7)
    game.click_logical(430, 700, "main-menu back/exit")
    game.wait_for_event("mode_changed", new_mode=10)
    game.wait_for_clean_exit()


@pytest.mark.parametrize(
    "game", [{"SDL_AUDIODRIVER": "dummy"}], indirect=True,
)
def test_multiplayer_direct_connect_projects_host_into_netman(game):
    """Direct endpoint text -> SDL_net admission -> recovered Netman ready fields."""
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=0)
    select_multiplayer(game)
    game.click_logical(925, 700, "main-menu go")
    game.wait_for_event("screen_presented", screen="multiplayer_lobby", dialog_state=5)

    server = subprocess.Popen(
        [_SDL3_NET_TRANSPORT_TEST, "--admit-server", "24000"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    try:
        assert server.stdout is not None
        assert server.stdout.readline().strip() == "ADMISSION_READY 24000"
        game.click_logical(850, 565, "focus direct connect endpoint")
        game.type_text("127.0.0.1:24000")
        game.press_key("Return")
        game.wait_for_event("netman_session_ready", player_id=2, hosting=False)
        game.wait_for_event("netman_player_joined", player_id=1)
        game.wait_for_event("transport_connected", player_id=2)
        game.wait_for_event("netman_message_processed", type=0x15)
        game.wait_for_event(
            "netman_ping_updated", dp_id=0x1234, slot=1, peer=1, x=7, y=8
        )
        game.screenshot("multiplayer-direct-connected-ready")
        assert server.wait(timeout=5) == 0
    finally:
        if server.poll() is None:
            server.terminate()
            server.wait(timeout=5)

    game.click_logical(850, 720, "multiplayer back")
    game.wait_for_event("screen_presented", screen="main_menu", dialog_state=7)
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

    # CGWND_GameSetup_Show (0x408F70) places the list to the right of the
    # grid. drawLayoutList (0x4094B0) then applies 12px padding and the
    # measured 14px normal-font height plus a four-pixel row step.
    for index, columns, rows in ((1, 2, 2), (2, 2, 1), (3, 3, 1), (4, 3, 2)):
        before = latest_sequence(game)
        # text.top = work.top + 0x27 + 0x0C (0x409046/0x40963F);
        # click five pixels inside each measured 18px row.
        game.click_logical(800, 268 + index * 18, f"select {columns}x{rows}")
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
    """Run asynchronous DNS-SD Search and retain the zero-session UI result."""
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
