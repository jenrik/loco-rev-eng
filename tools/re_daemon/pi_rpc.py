"""Pi RPC process lifecycle owned by the autonomous RE daemon."""

from __future__ import annotations

import asyncio
import json
import os
from pathlib import Path
import time
from typing import Any

from .broker import EventBroker
from .normalize import normalize_pi_event
from .store import DaemonStore


# Pi serializes each RPC event on one JSONL record. A read result may be 50KiB
# before JSON escaping, exceeding asyncio's 64KiB default line limit.
RPC_STREAM_LIMIT_BYTES = 2 * 1024 * 1024


def role_instructions(role: str) -> str:
    shared = (
        "You are an autonomous Lego Loco reverse-engineering worker. The raw binary and "
        "Ghidra evidence are authoritative. Use re_get_task before acting. Record material "
        "observations with re_record_observation, distinguish observed from tentative claims, "
        "and use re_defer_task for unresolved work. Ghidra opens lazily on the first re_ghidra_query; "
        "a context status of opened=false is normal and is not a reason to skip the query. Respect the approved "
        "write scope; never claim assembly validation without direct disassembly evidence."
    )
    role_specific = {
        "investigator": "Discover narrowly scoped evidence and next investigations; do not make broad code changes.",
        "transcriber": "Implement only behavior supported by supplied evidence; mark transcription status accurately.",
        "validator": "Compare implementation against disassembly instruction-by-instruction and report every mismatch.",
        "integrator": "Perform declared C++ hierarchy/field integration only after validated evidence is available.",
        "reviewer": "Remain read-only and return concrete evidence-backed defects or approval.",
    }.get(role, "Perform the assigned task conservatively and report uncertainty.")
    return f"{shared}\n\nRole: {role}. {role_specific}"


class PiRpcAgent:
    def __init__(
        self,
        store: DaemonStore,
        broker: EventBroker,
        agent: dict[str, Any],
        project_root: Path,
        daemon_url: str,
        daemon_token: str,
        pi_binary: str = "pi",
        rpc_stream_limit: int = RPC_STREAM_LIMIT_BYTES,
        resume: bool = False,
    ):
        self.store = store
        self.broker = broker
        self.agent = agent
        self.project_root = project_root
        self.daemon_url = daemon_url
        self.daemon_token = daemon_token
        self.pi_binary = pi_binary
        self.rpc_stream_limit = rpc_stream_limit
        self.resume = resume
        self._turns_completed = 0
        self.process: asyncio.subprocess.Process | None = None
        self._stdout_task: asyncio.Task[None] | None = None
        self._stderr_task: asyncio.Task[None] | None = None
        self._wait_task: asyncio.Task[None] | None = None
        self._aborted = False
        self._write_lock = asyncio.Lock()
        self._active_tools: dict[str, float] = {}
        self._last_activity_at = time.monotonic()

    async def start(self) -> None:
        session_dir = Path(self.agent["session_dir"])
        session_dir.mkdir(parents=True, exist_ok=True)
        extension = self.project_root / "tools" / "re_agent_extension.ts"
        if not extension.is_file():
            raise FileNotFoundError(f"missing Pi extension: {extension}")
        environment = os.environ.copy()
        environment.update({
            "RE_DAEMON_URL": self.daemon_url,
            "RE_DAEMON_TOKEN": self.daemon_token,
            "RE_AGENT_ID": self.agent["id"],
            "RE_ALLOWED_WRITES": json.dumps(self.agent["write_scope"]),
        })
        command = [
            self.pi_binary, "--mode", "rpc", "--no-extensions", "--session-dir", str(session_dir),
            *( ["--continue"] if self.resume else [] ),
            "--name", self.agent["id"], "--extension", str(extension),
            "--append-system-prompt", role_instructions(self.agent["role"]),
        ]
        self.store.set_agent_status(self.agent["id"], "starting")
        await self.broker.publish(self.agent["id"], "daemon_launching", {"command": command[:-1] + ["<extension>"]})
        self.process = await asyncio.create_subprocess_exec(
            *command,
            cwd=self.project_root,
            env=environment,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            limit=self.rpc_stream_limit,
        )
        self.store.set_agent_status(self.agent["id"], "running", self.process.pid)
        await self.broker.publish(self.agent["id"], "daemon_started", {"pid": self.process.pid})
        self._stdout_task = asyncio.create_task(self._read_stdout())
        self._stderr_task = asyncio.create_task(self._read_stderr())
        self._wait_task = asyncio.create_task(self._wait_for_exit())
        await self.send("prompt", {"id": "initial", "message": "continue" if self.resume else self.agent["task"]})

    async def send(self, command_type: str, payload: dict[str, Any]) -> None:
        if self.process is None or self.process.stdin is None or self.process.returncode is not None:
            raise RuntimeError("agent process is not running")
        command = {"type": command_type, **payload}
        encoded = (json.dumps(command, separators=(",", ":")) + "\n").encode("utf-8")
        async with self._write_lock:
            self.process.stdin.write(encoded)
            await self.process.stdin.drain()

    async def steer(self, message: str) -> None:
        await self.send("steer", {"message": message})

    async def follow_up(self, message: str) -> None:
        await self.send("follow_up", {"message": message})

    async def abort(self) -> None:
        self._aborted = True
        if self.process is not None and self.process.returncode is None:
            await self.send("abort", {})

    async def pause_after_turn(self, timeout_seconds: float = 60.0) -> bool:
        """Let the current agent turn finish, then abort at its next safe boundary."""
        target_turn = self._turns_completed + 1
        deadline = time.monotonic() + timeout_seconds
        while self.process is not None and self.process.returncode is None and time.monotonic() < deadline:
            if self._turns_completed >= target_turn:
                await self.abort()
                return True
            await asyncio.sleep(0.1)
        await self.abort()
        return False

    async def _read_stdout(self) -> None:
        assert self.process is not None and self.process.stdout is not None
        while True:
            try:
                line = await self.process.stdout.readline()
            except ValueError as error:
                await self.broker.publish(self.agent["id"], "rpc_stdout_limit_exceeded", {"limitBytes": self.rpc_stream_limit, "error": str(error)})
                self._aborted = True
                if self.process.returncode is None:
                    self.process.terminate()
                return
            if not line:
                return
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                await self.broker.publish(self.agent["id"], "rpc_protocol_error", {"line": line[:4096].decode("utf-8", errors="replace")})
                continue
            self._last_activity_at = time.monotonic()
            event_type = event.get("type")
            tool_call_id = event.get("toolCallId")
            if event_type == "tool_execution_start" and isinstance(tool_call_id, str):
                self._active_tools[tool_call_id] = self._last_activity_at
            elif event_type == "tool_execution_update" and isinstance(tool_call_id, str) and tool_call_id in self._active_tools:
                self._active_tools[tool_call_id] = self._last_activity_at
            elif event_type == "tool_execution_end" and isinstance(tool_call_id, str):
                self._active_tools.pop(tool_call_id, None)
            normalized = normalize_pi_event(event)
            if normalized is not None:
                kind, payload = normalized
                await self.broker.publish(self.agent["id"], kind, payload)
            if event.get("type") == "turn_end":
                self._turns_completed += 1
            if event.get("type") == "agent_settled":
                self.store.set_agent_status(self.agent["id"], "settled")

    def stalled_tool_calls(self, timeout_seconds: float) -> list[str]:
        now = time.monotonic()
        return sorted(call_id for call_id, last_update in self._active_tools.items() if now - last_update >= timeout_seconds)

    async def _read_stderr(self) -> None:
        assert self.process is not None and self.process.stderr is not None
        while True:
            line = await self.process.stderr.readline()
            if not line:
                return
            await self.broker.publish(self.agent["id"], "agent_stderr", {"line": line[:16384].decode("utf-8", errors="replace")})

    async def _wait_for_exit(self) -> None:
        assert self.process is not None
        exit_code = await self.process.wait()
        await asyncio.gather(*(task for task in (self._stdout_task, self._stderr_task) if task is not None), return_exceptions=True)
        current = self.store.get_agent(self.agent["id"])["status"]
        if current not in {"settled", "failed", "aborted"}:
            self.store.set_agent_status(self.agent["id"], "aborted" if self._aborted else "failed")
        await self.broker.publish(self.agent["id"], "daemon_exited", {"exitCode": exit_code, "aborted": self._aborted})


class AgentManager:
    """Owns live Pi processes. A restart leaves historical sessions intact."""

    def __init__(self, store: DaemonStore, broker: EventBroker, project_root: Path, daemon_url: str, daemon_token: str, pi_binary: str = "pi", tool_timeout_seconds: float = 300.0, watchdog_poll_seconds: float = 5.0):
        self.store = store
        self.broker = broker
        self.project_root = project_root
        self.daemon_url = daemon_url
        self.daemon_token = daemon_token
        self.pi_binary = pi_binary
        self.tool_timeout_seconds = tool_timeout_seconds
        self.watchdog_poll_seconds = watchdog_poll_seconds
        self._agents: dict[str, PiRpcAgent] = {}
        self._reapers: dict[str, asyncio.Task[None]] = {}
        self._watchdogs: dict[str, asyncio.Task[None]] = {}

    async def launch(self, agent_id: str, *, resume: bool = False) -> dict[str, Any]:
        if agent_id in self._agents:
            raise RuntimeError("agent already has a live process")
        agent = self.store.get_agent(agent_id)
        process = PiRpcAgent(self.store, self.broker, agent, self.project_root, self.daemon_url, self.daemon_token, self.pi_binary, resume=resume)
        self._agents[agent_id] = process
        try:
            await process.start()
            self._watchdogs[agent_id] = asyncio.create_task(self._watch_attempt(agent_id, process))
        except Exception:
            self._agents.pop(agent_id, None)
            self.store.set_agent_status(agent_id, "failed")
            raise
        return self.store.get_agent(agent_id)

    async def control(self, agent_id: str, action: str, message: str | None = None) -> None:
        process = self._agents.get(agent_id)
        if process is None:
            raise KeyError(agent_id)
        if action == "abort":
            await process.abort()
            self._schedule_reap(agent_id, process)
        elif action == "steer" and message:
            await process.steer(message)
        elif action == "follow_up" and message:
            await process.follow_up(message)
        else:
            raise ValueError("invalid control action or missing message")

    async def _watch_attempt(self, agent_id: str, process: PiRpcAgent) -> None:
        """Fail an in-progress task when its Pi child exits or tools stop progressing."""
        try:
            while True:
                await asyncio.sleep(self.watchdog_poll_seconds)
                task = self.store.task_for_agent(agent_id)
                if task is None or task["status"] != "in_progress":
                    return
                if process.process is None or process.process.returncode is not None:
                    await self._fail_task_attempt(agent_id, task["id"], "Pi process exited before a terminal task transition")
                    return
                stalled = process.stalled_tool_calls(self.tool_timeout_seconds)
                if stalled:
                    reason = f"tool inactivity timeout after {self.tool_timeout_seconds:.0f}s: {', '.join(stalled)}"
                    await self.broker.publish(agent_id, "agent_tool_timeout", {"taskId": task["id"], "toolCallIds": stalled, "timeoutSeconds": self.tool_timeout_seconds})
                    await process.abort()
                    self._schedule_reap(agent_id, process)
                    await self._fail_task_attempt(agent_id, task["id"], reason)
                    return
        finally:
            self._watchdogs.pop(agent_id, None)

    async def _fail_task_attempt(self, agent_id: str, task_id: str, reason: str) -> None:
        task = self.store.get_task(task_id)
        if task["status"] != "in_progress":
            return
        if agent_id:
            agent = self.store.get_agent(agent_id)
            if agent["status"] in {"queued", "starting", "running"}:
                self.store.set_agent_status(agent_id, "failed")
        failed = self.store.transition_task(task_id, "failed", reason, agent_id or None)
        await self.broker.publish(agent_id or None, "task_attempt_failed", {"task": failed, "reason": reason})

    async def recover_orphaned_tasks(self) -> list[str]:
        """Fail in-progress tasks whose daemon-owned child PID is definitely gone."""
        recovered: list[str] = []
        for task in self.store.in_progress_tasks():
            agent_id = task.get("assigned_agent_id")
            if not agent_id:
                continue
            agent = self.store.get_agent(agent_id)
            pid = agent.get("pid")
            try:
                if pid is not None:
                    os.kill(pid, 0)
                    continue
            except ProcessLookupError:
                pass
            except PermissionError:
                continue
            await self._fail_task_attempt(agent_id, task["id"], "daemon startup recovery: assigned Pi process is not alive")
            recovered.append(task["id"])
        return recovered

    async def recover_task(self, task_id: str, reason: str) -> dict[str, Any]:
        task = self.store.get_task(task_id)
        if task["status"] != "in_progress":
            raise ValueError("only an in-progress task can be recovered")
        agent_id = task.get("assigned_agent_id")
        if agent_id:
            try:
                await self.control(agent_id, "abort")
            except (KeyError, RuntimeError):
                pass
        await self._fail_task_attempt(agent_id or "", task_id, f"operator recovery: {reason}")
        return self.store.get_task(task_id)
    def _schedule_reap(self, agent_id: str, process: PiRpcAgent) -> None:
        if agent_id not in self._reapers:
            self._reapers[agent_id] = asyncio.create_task(self._reap_after_abort(agent_id, process))

    async def _reap_after_abort(self, agent_id: str, process: PiRpcAgent) -> None:
        """Release an idle RPC process after abort has let its tool response flush."""
        try:
            await asyncio.sleep(1)
            if process.process is not None and process.process.returncode is None:
                process.process.terminate()
            if process._wait_task is not None:
                try:
                    await asyncio.wait_for(asyncio.shield(process._wait_task), timeout=5)
                except TimeoutError:
                    if process.process is not None and process.process.returncode is None:
                        process.process.kill()
                    await asyncio.shield(process._wait_task)
        finally:
            if self._agents.get(agent_id) is process:
                self._agents.pop(agent_id, None)
            self._reapers.pop(agent_id, None)
    async def close(self) -> None:
        """Checkpoint active work, then stop daemon-owned Pi children on shutdown."""
        watchdogs = list(self._watchdogs.values())
        for watchdog in watchdogs:
            watchdog.cancel()
        await asyncio.gather(*watchdogs, return_exceptions=True)
        self._watchdogs.clear()
        processes = list(self._agents.values())
        checkpoints = []
        for process in processes:
            task = self.store.task_for_agent(process.agent["id"])
            if task is not None and task["status"] == "in_progress":
                checkpoints.append((process, task))
                await self.broker.publish(process.agent["id"], "daemon_pause_requested", {"taskId": task["id"]})
        await asyncio.gather(*(process.pause_after_turn() for process, _task in checkpoints), return_exceptions=True)
        for process, task in checkpoints:
            current = self.store.get_task(task["id"])
            if current["status"] == "in_progress":
                paused = self.store.transition_task(task["id"], "ready", f"daemon shutdown checkpoint: resume session from agent {process.agent['id']}")
                self.store.set_agent_status(process.agent["id"], "aborted")
                await self.broker.publish(process.agent["id"], "task_checkpointed", {"task": paused})
        await asyncio.gather(*(process.abort() for process in processes), return_exceptions=True)
        waiters = [process._wait_task for process in processes if process._wait_task is not None]
        try:
            await asyncio.wait_for(asyncio.gather(*waiters, return_exceptions=True), timeout=10)
        except TimeoutError:
            for process in processes:
                if process.process is not None and process.process.returncode is None:
                    process.process.kill()
            await asyncio.gather(*waiters, return_exceptions=True)
        await asyncio.gather(*self._reapers.values(), return_exceptions=True)
        self._reapers.clear()
        self._agents.clear()
