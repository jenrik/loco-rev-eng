from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
import os
import subprocess

import pytest

from gui_sandbox import GameSession


ROOT = Path(__file__).resolve().parents[2]


def _env_flag(name: str) -> bool:
    return os.environ.get(name, "") not in ("", "0")


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--gui-artifacts-dir",
        action="store",
        default=None,
        help="directory for persistent GUI screenshots, logs, and event streams",
    )
    parser.addoption(
        "--gui-visible",
        action="store_true",
        default=_env_flag("LEGO_LOCO_GUI_VISIBLE"),
        help="run the sandbox compositor in a visible nested window instead of "
             "headless, so a human can watch the run live (also settable via "
             "LEGO_LOCO_GUI_VISIBLE=1, e.g. for `make test-integration`)",
    )
    parser.addoption(
        "--gui-record",
        action="store_true",
        default=_env_flag("LEGO_LOCO_GUI_RECORD"),
        help="record each test's sandbox output to recording.mp4 in its "
             "artifact directory via wf-recorder, best-effort (also settable "
             "via LEGO_LOCO_GUI_RECORD=1, e.g. for `make test-integration`)",
    )


@pytest.fixture(scope="session", autouse=True)
def build_game() -> None:
    subprocess.run(["make", "build"], cwd=ROOT, check=True)


@pytest.fixture(scope="session")
def gui_artifacts_root(request: pytest.FixtureRequest) -> Path:
    configured = request.config.getoption("--gui-artifacts-dir")
    if configured:
        path = Path(configured).resolve()
    else:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        path = ROOT / "build" / "test-artifacts" / f"gui-{stamp}-{os.getpid()}"
    path.mkdir(parents=True, exist_ok=True)
    print(f"GUI test artifacts: {path}")
    return path


@pytest.fixture(scope="session")
def game_data_dir() -> Path:
    """The shared, read-only-in-practice unpacked game-data root.

    Resource lookup needs both art-res/ and Exe/loco.exe. Current-save writes
    are redirected by LEGO_LOCO_SAVE_DIR to the per-session save directory
    below, so GUI runs never copy or modify these shipped assets.
    """
    source = ROOT / "lego-loco-unpacked"
    if not (source / "art-res").is_dir() or not (source / "Exe" / "loco.exe").is_file():
        raise AssertionError(f"source game assets missing or incomplete: {source}")
    print(f"Shared game data: {source}")
    return source


@pytest.fixture(scope="session")
def game_save_dir(gui_artifacts_root: Path) -> Path:
    """Writable host-only current-save location for this GUI test session."""
    path = gui_artifacts_root / "game-saves"
    path.mkdir(parents=True, exist_ok=True)
    print(f"Isolated game saves: {path}")
    return path


@pytest.fixture
def game(request: pytest.FixtureRequest, gui_artifacts_root: Path,
          game_data_dir: Path, game_save_dir: Path):
    artifact_dir = gui_artifacts_root / request.node.name
    # Most menu regressions start at mode 2. The dedicated intro test opts
    # out so the normal SDL host launch remains fully covered without making
    # every menu scenario wait through three videos.
    environment = {"LEGO_LOCO_SKIP_INTRO": "1"}
    environment.update(getattr(request, "param", None) or {})
    environment["LEGO_LOCO_SAVE_DIR"] = str(game_save_dir)
    session = GameSession(ROOT, artifact_dir, environment=environment,
                          data_dir=game_data_dir,
                          visible=request.config.getoption("--gui-visible"),
                          record=request.config.getoption("--gui-record"))
    try:
        session.start()
        yield session
    except BaseException:
        session.best_effort_failure_screenshot()
        raise
    finally:
        session.close()
