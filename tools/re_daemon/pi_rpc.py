"""Pi RPC process lifecycle owned by the autonomous RE daemon."""

from __future__ import annotations

import asyncio
import json
import os
from pathlib import Path
from typing import Any

from .broker import EventBroker
from .normalize import normalize_pi_event
from .store import DaemonStore


def role_instructions(role: str) -> str:
    shared = (
        "You are an autonomous Lego Loco reverse-engineering worker. The raw binary and "
        "Ghidra evidence are authoritative. Use re_get_task before acting. Record material "
        "observations with re_record_observation, distinguish observed from tentative claims, "
        "and use re_defer_task for unresolved work. Respect the approved write scope; never "
        "claim assembly validation without direct disassembly evidence."
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
    ):
        self.store = store
        self.broker = broker
        self.agent = agent
        self.project_root = project_root
        self.daemon_url = daemon_url
        self.daemon_token = daemon_token
        self.pi_binary = pi_binary
        self.process: asyncio.subprocess.Process | None = None
        self._stdout_task: asyncio.Task[None] | None = None
        self._stderr_task: asyncio.Task[None] | None = None
        self._wait_task: asyncio.Task[None] | None = None
        self._aborted = False
        self._write_lock = asyncio.Lock()

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
        )
        self.store.set_agent_status(self.agent["id"], "running", self.process.pid)
        await self.broker.publish(self.agent["id"], "daemon_started", {"pid": self.process.pid})
        self._stdout_task = asyncio.create_task(self._read_stdout())
        self._stderr_task = asyncio.create_task(self._read_stderr())
        self._wait_task = asyncio.create_task(self._wait_for_exit())
        await self.send("prompt", {"id": "initial", "message": self.agent["task"]})

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

    async def _read_stdout(self) -> None:
        assert self.process is not None and self.process.stdout is not None
        while True:
            line = await self.process.stdout.readline()
            if not line:
                return
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                await self.broker.publish(self.agent["id"], "rpc_protocol_error", {"line": line[:4096].decode("utf-8", errors="replace")})
                continue
            normalized = normalize_pi_event(event)
            if normalized is not None:
                kind, payload = normalized
                await self.broker.publish(self.agent["id"], kind, payload)
            if event.get("type") == "agent_settled":
                self.store.set_agent_status(self.agent["id"], "settled")

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

    def __init__(self, store: DaemonStore, broker: EventBroker, project_root: Path, daemon_url: str, daemon_token: str, pi_binary: str = "pi"):
        self.store = store
        self.broker = broker
        self.project_root = project_root
        self.daemon_url = daemon_url
        self.daemon_token = daemon_token
        self.pi_binary = pi_binary
        self._agents: dict[str, PiRpcAgent] = {}

    async def launch(self, agent_id: str) -> dict[str, Any]:
        if agent_id in self._agents:
            raise RuntimeError("agent already has a live process")
        agent = self.store.get_agent(agent_id)
        process = PiRpcAgent(self.store, self.broker, agent, self.project_root, self.daemon_url, self.daemon_token, self.pi_binary)
        self._agents[agent_id] = process
        try:
            await process.start()
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
        elif action == "steer" and message:
            await process.steer(message)
        elif action == "follow_up" and message:
            await process.follow_up(message)
        else:
            raise ValueError("invalid control action or missing message")

    async def close(self) -> None:
        """Abort daemon-owned Pi children during application shutdown."""
        processes = list(self._agents.values())
        await asyncio.gather(*(process.abort() for process in processes), return_exceptions=True)
        waiters = [process._wait_task for process in processes if process._wait_task is not None]
        try:
            await asyncio.wait_for(asyncio.gather(*waiters, return_exceptions=True), timeout=10)
        except TimeoutError:
            for process in processes:
                if process.process is not None and process.process.returncode is None:
                    process.process.kill()
            await asyncio.gather(*waiters, return_exceptions=True)
        self._agents.clear()
