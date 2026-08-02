from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
import os
import shutil
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
        "--gui-size",
        action="store",
        default=os.environ.get("LEGO_LOCO_GUI_SIZE"),
        metavar="WIDTHxHEIGHT",
        help="force the sandbox output size (also settable via "
             "LEGO_LOCO_GUI_SIZE; especially useful with --gui-visible)",
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
def game_data_dir(gui_artifacts_root: Path) -> Path:
    """Writable per-session copy of the shipped game data.

    The SDL host writes its current-save marker ("curr") and the ".sav"
    companion into g_install_path (the LEGO_LOCO_DATA data dir — the
    documented host deviation mirroring the original's "~curr" write into
    its install dir).  A session pointed at the repository's source
    art-res would write into the repo's untracked assets, so every GUI
    flow runs against a fresh temp copy under build/test-artifacts; the
    source art-res is only ever read (the C++ persistence fixtures follow
    the same rule with their own make_temp_dir()).
    """
    data_dir = gui_artifacts_root / "game-data"
    target = data_dir / "art-res"
    if not target.exists():
        source = ROOT / "lego-loco-unpacked"
        if not (source / "art-res").is_dir():
            raise AssertionError(f"source game assets missing: {source}")
        # The whole data root: the host resource bridge also opens
        # <LEGO_LOCO_DATA>/Exe/loco.exe for the PE string table
        # (ResourceManager_Init), and art-res/ for the RFH/RFD archives.
        # The Ghidra project database is never read by the game and is
        # excluded to keep the isolated data copy lean.
        shutil.copytree(
            source, data_dir,
            ignore=shutil.ignore_patterns("ghidra_projects"),
        )
    print(f"Isolated game data: {data_dir}")
    return data_dir


@pytest.fixture
def game(request: pytest.FixtureRequest, gui_artifacts_root: Path,
          game_data_dir: Path):
    artifact_dir = gui_artifacts_root / request.node.name
    # Most menu regressions start at mode 2. The dedicated intro test opts
    # out so the normal SDL host launch remains fully covered without making
    # every menu scenario wait through three videos.
    environment = {"LEGO_LOCO_SKIP_INTRO": "1"}
    environment.update(getattr(request, "param", None) or {})
    session = GameSession(ROOT, artifact_dir, environment=environment,
                          data_dir=game_data_dir,
                          visible=request.config.getoption("--gui-visible"),
                          screen_size=request.config.getoption("--gui-size"),
                          record=request.config.getoption("--gui-record"))
    try:
        session.start()
        yield session
    except BaseException:
        session.best_effort_failure_screenshot()
        raise
    finally:
        session.close()
