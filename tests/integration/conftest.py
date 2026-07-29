from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
import os
import subprocess

import pytest

from gui_sandbox import GameSession


ROOT = Path(__file__).resolve().parents[2]


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--gui-artifacts-dir",
        action="store",
        default=None,
        help="directory for persistent GUI screenshots, logs, and event streams",
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


@pytest.fixture
def game(request: pytest.FixtureRequest, gui_artifacts_root: Path):
    artifact_dir = gui_artifacts_root / request.node.name
    environment = getattr(request, "param", None)
    session = GameSession(ROOT, artifact_dir, environment=environment)
    try:
        session.start()
        yield session
    except BaseException:
        session.best_effort_failure_screenshot()
        raise
    finally:
        session.close()
