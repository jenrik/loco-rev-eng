"""Live event broker backed by the durable daemon store."""

from __future__ import annotations

import asyncio
from typing import Any

from .store import DaemonStore


class EventBroker:
    def __init__(self, store: DaemonStore):
        self.store = store
        self._subscribers: set[asyncio.Queue[dict[str, Any]]] = set()

    async def publish(self, agent_id: str | None, kind: str, payload: dict[str, Any]) -> dict[str, Any]:
        event = self.store.record_event(agent_id, kind, payload)
        stale: list[asyncio.Queue[dict[str, Any]]] = []
        for queue in self._subscribers:
            try:
                queue.put_nowait(event)
            except asyncio.QueueFull:
                stale.append(queue)
        for queue in stale:
            self._subscribers.discard(queue)
        return event

    def subscribe(self) -> asyncio.Queue[dict[str, Any]]:
        queue: asyncio.Queue[dict[str, Any]] = asyncio.Queue(maxsize=512)
        self._subscribers.add(queue)
        return queue

    def unsubscribe(self, queue: asyncio.Queue[dict[str, Any]]) -> None:
        self._subscribers.discard(queue)
