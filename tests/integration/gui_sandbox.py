"""Isolated Wayland driver for Lego Loco GUI integration tests."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import signal
import struct
import subprocess
import time
from typing import Any, Mapping


CANVAS_WIDTH = 1280
CANVAS_HEIGHT = 1024


class GameSession:
    """Own one game process and one throwaway gui-sandbox compositor.

    data_dir points at the shared unpacked game-data root. The fixture sets
    LEGO_LOCO_SAVE_DIR to an artifact-local directory, so the host-only curr
    and curr.sav writes stay isolated while assets remain shared and read-only.
    """

    def __init__(
        self, root: Path, artifact_dir: Path, timeout: float = 20.0,
        environment: Mapping[str, str] | None = None,
        data_dir: Path | None = None,
        visible: bool = False,
        record: bool = False,
    ):
        self.root = root
        self.artifact_dir = artifact_dir
        self.timeout = timeout
        self.environment = dict(environment or {})
        self.data_dir = data_dir
        self.visible = visible
        self.record = record
        self.events_path = artifact_dir / "events.jsonl"
        self.stdout_path = artifact_dir / "stdout.log"
        self.stderr_path = artifact_dir / "stderr.log"
        self.interactions_path = artifact_dir / "interactions.jsonl"
        self.wrapper_path = artifact_dir / "launch-game.sh"
        self.recording_path = artifact_dir / "recording.mp4"
        self.tag: str | None = None
        self.pid: int | None = None
        self.screenshot_size: tuple[int, int] | None = None
        self.content_rect: tuple[int, int, int, int] | None = None
        self._recording_process: subprocess.Popen | None = None
        self._started_at = time.monotonic()
        artifact_dir.mkdir(parents=True, exist_ok=True)

    def start(self) -> "GameSession":
        binary = self.root / "build" / "lego_loco"
        assets = self.root / "lego-loco-unpacked" / "art-res"
        if not binary.is_file():
            raise AssertionError(f"game binary missing: {binary}")
        if not assets.is_dir():
            raise AssertionError(f"game assets missing: {assets}")
        if self.data_dir is not None and not (self.data_dir / "art-res").is_dir():
            raise AssertionError(f"isolated game data missing: {self.data_dir}")

        started = self._run(
            ["gui-sandbox", "start", "--size", f"{CANVAS_WIDTH}x{CANVAS_HEIGHT}"]
            + (["--visible"] if self.visible else []),
            timeout=30,
        )
        (self.artifact_dir / "sandbox-start.log").write_text(
            started.stdout + started.stderr, encoding="utf-8"
        )
        self.tag = self._parse_assignment(started.stdout, "TAG")

        for key, value in self.environment.items():
            if not key.isidentifier() or not isinstance(value, str):
                raise AssertionError(f"invalid game environment override: {key!r}")

        script = "\n".join(
            [
                "#!/bin/sh",
                "set -eu",
                f"cd {shlex.quote(str(self.root))}",
                *[
                    f"export {key}={shlex.quote(value)}"
                    for key, value in self.environment.items()
                ],
                # Shared read-only asset root. The fixture separately sets
                # LEGO_LOCO_SAVE_DIR for host-only curr/curr.sav writes.
                f"export LEGO_LOCO_DATA={shlex.quote(str(self.data_dir or (self.root / 'lego-loco-unpacked')))}",
                f"export LEGO_LOCO_TEST_EVENTS={shlex.quote(str(self.events_path))}",
                f"exec {shlex.quote(str(binary))} >>{shlex.quote(str(self.stdout_path))} 2>>{shlex.quote(str(self.stderr_path))}",
                "",
            ]
        )
        self.wrapper_path.write_text(script, encoding="utf-8")
        self.wrapper_path.chmod(0o700)

        launched = self._run(
            ["gui-sandbox", "launch", self.tag, str(self.wrapper_path)], timeout=30
        )
        (self.artifact_dir / "sandbox-launch.log").write_text(
            launched.stdout + launched.stderr, encoding="utf-8"
        )
        self.pid = int(self._parse_assignment(launched.stdout, "PID"))
        self.content_rect = self._query_content_rect()
        self._record(
            "launch", pid=self.pid, tag=self.tag, content_rect=self.content_rect
        )
        if self.record:
            self._start_recording()
        return self

    def close(self) -> None:
        if self.tag is None:
            return
        self._stop_recording()
        if self.is_alive() and self.pid is not None:
            try:
                os.kill(self.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        stopped = subprocess.run(
            ["gui-sandbox", "stop", self.tag],
            text=True,
            capture_output=True,
            timeout=30,
            check=False,
        )
        (self.artifact_dir / "sandbox-stop.log").write_text(
            stopped.stdout + stopped.stderr, encoding="utf-8"
        )
        self._record("sandbox_stop", returncode=stopped.returncode)
        self.tag = None

    def is_alive(self) -> bool:
        if self.pid is None:
            return False
        stat = Path(f"/proc/{self.pid}/stat")
        try:
            fields = stat.read_text(encoding="utf-8").split()
        except (FileNotFoundError, ProcessLookupError):
            # procfs may remove the PID after pathname lookup but before read(2).
            return False
        return len(fields) > 2 and fields[2] != "Z"

    def assert_alive(self, context: str) -> None:
        if not self.is_alive():
            raise AssertionError(self._failure(f"game exited during {context}"))

    def events(self) -> list[dict[str, Any]]:
        if not self.events_path.exists():
            return []
        parsed: list[dict[str, Any]] = []
        for line in self.events_path.read_text(encoding="utf-8").splitlines():
            if line.strip():
                parsed.append(json.loads(line))
        return parsed

    def wait_for_event(
        self, event: str, timeout: float | None = None, *,
        after_sequence: int | None = None, **fields: Any
    ) -> dict[str, Any]:
        """Wait for an event, optionally requiring a strictly later sequence.

        Button tests use this to distinguish the Go/Back WAV from an earlier
        selector click that happens to queue the same original resource.
        """
        deadline = time.monotonic() + (timeout or self.timeout)
        while time.monotonic() < deadline:
            for item in self.events():
                if (item.get("event") == event
                    and (after_sequence is None
                         or int(item.get("sequence", -1)) > after_sequence)
                    and all(item.get(key) == value for key, value in fields.items())):
                    self._record("event_observed", event=event, fields=fields)
                    return item
            if not self.is_alive():
                raise AssertionError(
                    self._failure(f"game exited while waiting for {event} {fields}")
                )
            time.sleep(0.05)
        raise AssertionError(self._failure(f"timed out waiting for {event} {fields}"))

    def wait_for_clean_exit(self, timeout: float | None = None) -> None:
        self.wait_for_event("clean_shutdown", timeout=timeout)
        deadline = time.monotonic() + (timeout or self.timeout)
        while time.monotonic() < deadline:
            if not self.is_alive():
                self._record("clean_exit")
                return
            time.sleep(0.05)
        raise AssertionError(
            self._failure("clean_shutdown was emitted but process remained alive")
        )

    def screenshot(self, name: str) -> Path:
        self.assert_alive(f"screenshot {name}")
        assert self.tag is not None
        path = self.artifact_dir / f"{name}.png"

        # Screenshots are diagnostic artifacts, not pixel or golden assertions.
        # Park the compositor cursor in a corner to avoid hiding central controls.
        self._run(["gui-sandbox", "move", self.tag, "1", "1"])
        time.sleep(0.1)
        # The game window is borderless under test (LEGO_LOCO_TEST_EVENTS,
        # see graphics/sdl3_window.cpp), so the raw compositor capture is
        # already pixel-comparable against reference screenshots
        # (tests/reference/*.png) with no decoration to crop out.
        self._run(["gui-sandbox", "shot", self.tag, str(path)], timeout=20)
        self.screenshot_size = self._png_size(path)
        self._record("screenshot", name=name, path=str(path), size=self.screenshot_size)
        return path

    def click_logical(self, x: float, y: float, label: str) -> None:
        # Sway may initially report the undecorated placement before its title
        # bar layout settles. Refresh geometry for every action instead of
        # trusting the launch-time tree snapshot.
        self.content_rect = self._query_content_rect()
        assert self.tag is not None
        display_x, display_y = self._logical_to_display(
            x, y, *self.content_rect
        )
        self._record(
            "click", label=label, logical=[x, y], display=[display_x, display_y],
            content_rect=self.content_rect
        )
        self._run(
            ["gui-sandbox", "click", self.tag, str(display_x), str(display_y)]
        )
        self.assert_alive(f"click {label}")

    def move_logical(self, x: float, y: float, label: str) -> None:
        # See click_logical: refresh geometry every action rather than
        # trusting the launch-time tree snapshot.
        self.content_rect = self._query_content_rect()
        assert self.tag is not None
        display_x, display_y = self._logical_to_display(
            x, y, *self.content_rect
        )
        self._record(
            "move", label=label, logical=[x, y], display=[display_x, display_y],
            content_rect=self.content_rect
        )
        self._run(
            ["gui-sandbox", "move", self.tag, str(display_x), str(display_y)]
        )
        self.assert_alive(f"move {label}")

    def type_text(self, text: str) -> None:
        assert self.tag is not None
        self._record("type", byte_count=len(text.encode("utf-8")))
        self._run(["gui-sandbox", "type", self.tag, text])
        self.assert_alive("text input")

    def press_key(self, key: str) -> None:
        assert self.tag is not None
        self._record("key", key=key)
        self._run(["gui-sandbox", "key", self.tag, key])

    def clear_text(self, count: int = 12) -> None:
        assert self.tag is not None
        for _ in range(count):
            self._run(["gui-sandbox", "key", self.tag, "BackSpace"])
        self._record("clear_text", keypresses=count)
        self.assert_alive("clearing player name")

    def best_effort_failure_screenshot(self) -> None:
        if not self.is_alive() or self.tag is None:
            return
        try:
            self.screenshot("failure")
        except Exception as exc:  # diagnostic collection must not hide the failure
            (self.artifact_dir / "failure-screenshot-error.txt").write_text(
                f"{type(exc).__name__}: {exc}\n", encoding="utf-8"
            )

    def _start_recording(self) -> None:
        # wf-recorder is an optional evidence artifact, not part of the game's
        # observable behavior: a missing binary or capture failure must never
        # fail the test, only leave a note explaining the gap.
        assert self.tag is not None
        display_file = self._runtime_dir() / f"{self.tag}.display"
        try:
            display = display_file.read_text(encoding="utf-8").strip()
        except FileNotFoundError:
            self._note_recording_failure(f"missing sandbox display file: {display_file}")
            return

        log_path = self.artifact_dir / "recording.log"
        try:
            self._recording_process = subprocess.Popen(
                ["wf-recorder", "-f", str(self.recording_path)],
                env={
                    **os.environ,
                    "WAYLAND_DISPLAY": display,
                    "XDG_RUNTIME_DIR": str(self._runtime_dir()),
                },
                stdout=log_path.open("w", encoding="utf-8"),
                stderr=subprocess.STDOUT,
            )
        except FileNotFoundError:
            self._note_recording_failure("wf-recorder is not installed")
            return

        time.sleep(0.3)
        if self._recording_process.poll() is not None:
            self._note_recording_failure(f"wf-recorder exited immediately (see {log_path})")
            self._recording_process = None
            return
        self._record("recording_started", path=str(self.recording_path))

    def _stop_recording(self) -> None:
        process = self._recording_process
        self._recording_process = None
        if process is None or process.poll() is not None:
            return
        # SIGINT lets wf-recorder finalize the container (moov atom etc.)
        # instead of leaving a truncated, unplayable file; escalate only if
        # it doesn't exit on its own.
        for sig, timeout in (
            (signal.SIGINT, 5), (signal.SIGTERM, 5), (signal.SIGKILL, 5)
        ):
            process.send_signal(sig)
            try:
                process.wait(timeout=timeout)
                break
            except subprocess.TimeoutExpired:
                continue
        self._record("recording_stopped", path=str(self.recording_path))

    def _note_recording_failure(self, reason: str) -> None:
        (self.artifact_dir / "recording-error.txt").write_text(
            reason + "\n", encoding="utf-8"
        )
        self._record("recording_failed", reason=reason)

    def _run(
        self, argv: list[str], timeout: float = 15.0
    ) -> subprocess.CompletedProcess[str]:
        result = subprocess.run(
            argv, text=True, capture_output=True, timeout=timeout, check=False
        )
        if result.returncode != 0:
            raise AssertionError(
                f"command failed ({result.returncode}): {shlex.join(argv)}\n"
                f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
            )
        return result

    def _failure(self, reason: str) -> str:
        if self.stderr_path.exists():
            stderr = self.stderr_path.read_text(
                encoding="utf-8", errors="replace"
            )
        else:
            stderr = ""
        tail = "\n".join(stderr.splitlines()[-80:])
        return (
            f"{reason}\nartifacts: {self.artifact_dir}\n"
            f"stderr tail:\n{tail}"
        )

    def _record(self, action: str, **details: Any) -> None:
        item = {
            "elapsed_ms": int((time.monotonic() - self._started_at) * 1000),
            "action": action,
            **details,
        }
        with self.interactions_path.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(item, sort_keys=True) + "\n")

    @staticmethod
    def _parse_assignment(output: str, key: str) -> str:
        prefix = key + "="
        for line in output.splitlines():
            if line.startswith(prefix):
                return line[len(prefix):].strip()
        raise AssertionError(f"missing {key}=... in output:\n{output}")

    @staticmethod
    def _runtime_dir() -> Path:
        return Path(
            os.environ.get("XDG_RUNTIME_DIR") or f"/tmp/gsbx-runtime-{os.getuid()}"
        )

    def _query_content_rect(self) -> tuple[int, int, int, int]:
        assert self.tag is not None and self.pid is not None
        socket = self._runtime_dir() / f"{self.tag}.sock"
        result = self._run(
            ["swaymsg", "-s", str(socket), "-t", "get_tree", "-r"], timeout=10
        )
        root = json.loads(result.stdout)
        stack = [root]
        while stack:
            node = stack.pop()
            if node.get("pid") == self.pid:
                rect = node["rect"]
                window = node["window_rect"]
                decoration = node.get("deco_rect", {})
                content_y = rect["y"] + window["y"]
                # During initial tiling Sway can briefly leave rect.y at zero
                # while already reporting the eventual title-bar height.
                decoration_bottom = decoration.get("y", 0) + decoration.get(
                    "height", 0
                )
                content_y = max(content_y, decoration_bottom + window["y"])
                return (
                    rect["x"] + window["x"],
                    content_y,
                    window["width"],
                    window["height"],
                )
            stack.extend(node.get("nodes", []))
            stack.extend(node.get("floating_nodes", []))
        raise AssertionError(f"mapped pid {self.pid} is absent from Sway tree")

    @staticmethod
    def _png_size(path: Path) -> tuple[int, int]:
        data = path.read_bytes()[:24]
        if len(data) != 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
            raise AssertionError(f"not a valid PNG screenshot: {path}")
        return struct.unpack(">II", data[16:24])

    @staticmethod
    def _logical_to_display(
        x: float, y: float, content_x: int, content_y: int,
        content_width: int, content_height: int
    ) -> tuple[int, int]:
        scale = min(
            content_width / CANVAS_WIDTH, content_height / CANVAS_HEIGHT
        )
        origin_x = content_x + (content_width - CANVAS_WIDTH * scale) / 2
        origin_y = content_y + (content_height - CANVAS_HEIGHT * scale) / 2
        return round(origin_x + scale * x), round(origin_y + scale * y)
