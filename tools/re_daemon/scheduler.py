"""Deterministic, evidence-led task launcher for the daemon."""

from __future__ import annotations

import asyncio
from pathlib import Path
from typing import Any
from uuid import uuid4

from .broker import EventBroker
from .pi_rpc import AgentManager
from .store import DaemonStore


class AutonomousScheduler:
    """Launch ready tasks only; agents create evidence and report transitions."""

    def __init__(self, store: DaemonStore, broker: EventBroker, manager: AgentManager, state_path: Path):
        self.store = store
        self.broker = broker
        self.manager = manager
        self.state_path = state_path
        self._schedule_lock = asyncio.Lock()

    async def schedule(self, job_id: str, limit: int = 1) -> list[dict[str, Any]]:
        async with self._schedule_lock:
            return await self._schedule(job_id, limit)

    async def drive(self, job_id: str, max_active: int = 1) -> list[dict[str, Any]]:
        """Fill bounded autonomous capacity after a graph transition."""
        max_active = max(1, min(max_active, 8))
        async with self._schedule_lock:
            available = max_active - self.store.count_in_progress_tasks(job_id)
            if available <= 0:
                return []
            return await self._schedule(job_id, available)

    async def _schedule(self, job_id: str, limit: int) -> list[dict[str, Any]]:
        launched: list[dict[str, Any]] = []
        for task in self.store.claim_ready_tasks(job_id, limit):
            # A clean daemon shutdown preserves the prior session directory so
            # Pi can restore it with --continue and receive the automatic
            # continuation prompt.
            resume = False
            session_dir = str(self.state_path.parent / "sessions" / f"session-{uuid4().hex}")
            previous_reason = task.get("_claim_previous_reason") or ""
            if previous_reason.startswith("daemon shutdown checkpoint:") and task.get("assigned_agent_id"):
                try:
                    previous = self.store.get_agent(task["assigned_agent_id"])
                    session_dir = previous["session_dir"]
                    resume = True
                except KeyError:
                    pass
            agent = self.store.create_agent(
                task["job_id"], task["role"], task["instructions"], session_dir, task["write_scope"]
            )
            self.store.transition_task(task["id"], "in_progress", "launched", agent["id"])
            await self.broker.publish(agent["id"], "task_claimed", {"task": self.store.get_task(task["id"])})
            try:
                await self.manager.launch(agent["id"], resume=resume)
            except Exception as error:
                self.store.transition_task(task["id"], "ready", f"launch failed: {error}")
                await self.broker.publish(agent["id"], "task_launch_failed", {"taskId": task["id"], "error": str(error)})
                continue
            launched.append({"agent": agent, "task": self.store.get_task(task["id"])})
        return launched
