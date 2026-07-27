"""FastAPI application for the local autonomous RE dashboard."""

from __future__ import annotations

import asyncio
from pathlib import Path
from typing import Any
from uuid import uuid4

from fastapi import FastAPI, Header, HTTPException, Query, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

from .broker import EventBroker
from .pi_rpc import AgentManager
from .store import DaemonStore


class CreateJob(BaseModel):
    title: str = Field(min_length=1, max_length=200)
    goal: str = Field(min_length=1, max_length=8000)


class CreateAgent(BaseModel):
    role: str = Field(min_length=1, max_length=80)
    task: str = Field(min_length=1, max_length=16000)
    write_scope: list[str] = Field(default_factory=list)


class AgentControl(BaseModel):
    action: str = Field(pattern="^(steer|follow_up|abort)$")
    message: str | None = Field(default=None, max_length=16000)


class RecordEvent(BaseModel):
    kind: str = Field(min_length=1, max_length=100)
    payload: dict[str, Any] = Field(default_factory=dict)


def create_app(
    state_path: str | Path,
    project_root: str | Path | None = None,
    daemon_url: str = "http://127.0.0.1:8765",
    daemon_token: str = "",
    pi_binary: str = "pi",
) -> FastAPI:
    state_path = Path(state_path)
    project_root = Path(project_root or Path.cwd())
    store = DaemonStore(state_path)
    store.initialize()
    broker = EventBroker(store)
    static_dir = Path(__file__).with_name("static")

    app = FastAPI(title="Lego Loco Autonomous RE")
    app.state.store = store
    app.state.broker = broker
    app.state.daemon_token = daemon_token
    app.state.agent_manager = AgentManager(store, broker, project_root, daemon_url, daemon_token, pi_binary)
    app.mount("/static", StaticFiles(directory=static_dir), name="static")

    @app.get("/")
    def dashboard() -> FileResponse:
        return FileResponse(static_dir / "index.html")

    @app.get("/api/status")
    def status() -> dict[str, Any]:
        return store.snapshot()

    @app.post("/api/jobs")
    async def create_job(request: CreateJob) -> dict[str, Any]:
        job = store.create_job(request.title, request.goal)
        await broker.publish(None, "job_created", {"job": job})
        return job

    @app.post("/api/jobs/{job_id}/agents")
    async def create_agent(job_id: str, request: CreateAgent) -> dict[str, Any]:
        session_dir = str(state_path.parent / "sessions" / f"session-{uuid4().hex}")
        try:
            agent = store.create_agent(job_id, request.role, request.task, session_dir, request.write_scope)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown job") from None
        await broker.publish(agent["id"], "agent_created", {"agent": agent})
        return agent

    @app.post("/api/agents/{agent_id}/start")
    async def start_agent(agent_id: str) -> dict[str, Any]:
        try:
            return await app.state.agent_manager.launch(agent_id)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown agent") from None
        except Exception as error:
            raise HTTPException(status_code=409, detail=str(error)) from error

    @app.post("/api/agents/{agent_id}/control")
    async def control_agent(agent_id: str, request: AgentControl) -> dict[str, Any]:
        try:
            await app.state.agent_manager.control(agent_id, request.action, request.message)
        except KeyError:
            raise HTTPException(status_code=404, detail="agent is not live") from None
        except ValueError as error:
            raise HTTPException(status_code=422, detail=str(error)) from error
        return {"ok": True}

    @app.get("/api/agents/{agent_id}/events")
    def agent_events(agent_id: str, after: int = Query(default=0, ge=0), limit: int = Query(default=200, ge=1, le=1000)) -> dict[str, Any]:
        try:
            store.get_agent(agent_id)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown agent") from None
        return {"events": store.events_after(after, agent_id, limit)}

    def require_internal(token: str | None) -> None:
        if app.state.daemon_token and token != app.state.daemon_token:
            raise HTTPException(status_code=403, detail="invalid daemon capability")

    @app.get("/internal/agents/{agent_id}/context")
    async def agent_context(agent_id: str, x_re_daemon_token: str | None = Header(default=None)) -> dict[str, Any]:
        require_internal(x_re_daemon_token)
        try:
            agent = store.get_agent(agent_id)
            job = store.get_job(agent["job_id"])
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown agent") from None
        return {"agent": agent, "job": job}

    @app.post("/internal/agents/{agent_id}/events")
    async def record_agent_event(agent_id: str, request: RecordEvent, x_re_daemon_token: str | None = Header(default=None)) -> dict[str, Any]:
        require_internal(x_re_daemon_token)
        try:
            store.get_agent(agent_id)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown agent") from None
        return await broker.publish(agent_id, request.kind, request.payload)

    @app.websocket("/ws")
    async def websocket_events(websocket: WebSocket) -> None:
        await websocket.accept()
        queue = broker.subscribe()
        try:
            while True:
                event = await queue.get()
                await websocket.send_json(event)
        except WebSocketDisconnect:
            pass
        finally:
            broker.unsubscribe(queue)

    return app
