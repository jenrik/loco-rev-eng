"""FastAPI application for the local autonomous RE dashboard."""

from __future__ import annotations

from contextlib import asynccontextmanager
from pathlib import Path
from typing import Any
from uuid import uuid4

from fastapi import FastAPI, Header, HTTPException, Query, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

from .broker import EventBroker
from .mcp import GhidraAdapter, GhidraConfig, McpError
from .pi_rpc import AgentManager
from .scheduler import AutonomousScheduler
from .store import DaemonStore


class CreateJob(BaseModel):
    title: str = Field(min_length=1, max_length=200)
    goal: str = Field(min_length=1, max_length=8000)


class CreateAgent(BaseModel):
    role: str = Field(min_length=1, max_length=80)
    task: str = Field(min_length=1, max_length=16000)
    write_scope: list[str] = Field(default_factory=list)


class CreateTask(BaseModel):
    title: str = Field(min_length=1, max_length=200)
    instructions: str = Field(min_length=1, max_length=16000)
    role: str = Field(min_length=1, max_length=80)
    write_scope: list[str] = Field(default_factory=list)
    status: str = Field(default="ready", pattern="^(ready|blocked|deferred)$")


class CreateTaskDependency(BaseModel):
    dependency_task_id: str = Field(min_length=1)
    relation: str = Field(default="requires", pattern="^(requires|evidence|invalidates)$")


class AgentControl(BaseModel):
    action: str = Field(pattern="^(steer|follow_up|abort)$")
    message: str | None = Field(default=None, max_length=16000)


class RecordEvent(BaseModel):
    kind: str = Field(min_length=1, max_length=100)
    payload: dict[str, Any] = Field(default_factory=dict)


class GhidraQuery(BaseModel):
    operation: str = Field(min_length=1, max_length=100)
    arguments: dict[str, Any] = Field(default_factory=dict)
    use_cache: bool = True


class RecordHypothesis(BaseModel):
    subject: str = Field(min_length=1, max_length=500)
    statement: str = Field(min_length=1, max_length=16000)
    evidence_ids: list[str] = Field(default_factory=list)
    status: str = Field(default="tentative", pattern="^(tentative|supported|rejected|superseded)$")
    supersedes_id: str | None = Field(default=None, min_length=1)


class ResolveWriteScopeRequest(BaseModel):
    decision: str = Field(pattern="^(approved|rejected)$")
    reason: str | None = Field(default=None, max_length=16000)


class RequeueTask(BaseModel):
    reason: str = Field(min_length=1, max_length=16000)


class RecoverTask(BaseModel):
    reason: str = Field(min_length=1, max_length=16000)


class TaskTransition(BaseModel):
    status: str = Field(pattern="^(completed|blocked|deferred|failed)$")
    reason: str | None = Field(default=None, max_length=16000)


class TaskGraphTask(BaseModel):
    key: str = Field(min_length=1, max_length=120)
    title: str = Field(min_length=1, max_length=200)
    instructions: str = Field(min_length=1, max_length=16000)
    role: str = Field(pattern="^(investigator|transcriber|validator|integrator|reviewer)$")
    write_scope: list[str] = Field(default_factory=list)


class TaskGraphDependency(BaseModel):
    task_key: str = Field(min_length=1, max_length=120)
    dependency_key: str = Field(min_length=1, max_length=120)
    relation: str = Field(default="requires", pattern="^(requires|evidence|invalidates)$")


class ExpandTaskGraph(BaseModel):
    rationale: str = Field(min_length=1, max_length=16000)
    tasks: list[TaskGraphTask] = Field(min_length=1, max_length=24)
    dependencies: list[TaskGraphDependency] = Field(default_factory=list, max_length=64)


class WriteScopeRequest(BaseModel):
    path: str = Field(min_length=1)
    reason: str = Field(min_length=1, max_length=16000)


def create_app(
    state_path: str | Path,
    project_root: str | Path | None = None,
    daemon_url: str = "http://127.0.0.1:8765",
    daemon_token: str = "",
    pi_binary: str = "pi",
    ghidra_config: GhidraConfig | None = None,
) -> FastAPI:
    state_path = Path(state_path)
    project_root = Path(project_root or Path.cwd())
    store = DaemonStore(state_path)
    store.initialize()
    broker = EventBroker(store)
    ghidra = GhidraAdapter(ghidra_config)
    manager = AgentManager(store, broker, project_root, daemon_url, daemon_token, pi_binary)
    scheduler = AutonomousScheduler(store, broker, manager, state_path)
    static_dir = Path(__file__).with_name("static")

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        await manager.recover_orphaned_tasks()
        # A restart must resume a queued autonomous graph without waiting for an
        # operator to press Schedule. Capacity remains serial by default.
        for job in store.snapshot()["jobs"]:
            if job["status"] == "queued":
                await scheduler.drive(job["id"])
        try:
            yield
        finally:
            await manager.close()
            await ghidra.close()

    app = FastAPI(title="Lego Loco Autonomous RE", lifespan=lifespan)
    app.state.store = store
    app.state.broker = broker
    app.state.daemon_token = daemon_token
    app.state.ghidra = ghidra
    app.state.agent_manager = manager
    app.state.scheduler = scheduler
    app.mount("/static", StaticFiles(directory=static_dir), name="static")

    def require_internal(token: str | None) -> None:
        if app.state.daemon_token and token != app.state.daemon_token:
            raise HTTPException(status_code=403, detail="invalid daemon capability")

    @app.get("/")
    def dashboard() -> FileResponse:
        return FileResponse(static_dir / "index.html")

    @app.get("/api/status")
    def status() -> dict[str, Any]:
        return {**store.snapshot(), "ghidra": ghidra.status()}

    @app.get("/api/ghidra/status")
    def ghidra_status() -> dict[str, Any]:
        return ghidra.status()

    async def bootstrap_job(job: dict[str, Any]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
        """Create and dispatch the one read-only evidence task for a new objective."""
        initial_task = store.create_task(
            job["id"],
            "Initial evidence triage",
            (
                f"Establish an evidence-led starting point for this job objective:\n{job['goal']}\n\n"
                "Use re_get_task first. Work read-only and issue one tool call at a time. "
                "Inspect the most relevant existing source and project records, then make up to six targeted "
                "re_ghidra_query calls against the highest-risk claims. Record direct binary facts with "
                "re_record_observation and nontrivial interpretations with re_record_hypothesis citing evidence IDs. "
                "Do not edit files. Before marking this triage completed, call re_expand_task_graph exactly once with "
                "a small executable follow-on DAG: concrete address/pass tasks, explicit requires edges, evidence-backed "
                "instructions, and the narrowest safe write scopes. A prose plan in the completion reason is not enough. "
                "Then call re_transition_task completed. If Ghidra or required evidence is unavailable, mark blocked "
                "with the concrete cause instead of inventing graph nodes."
            ),
            "investigator",
        )
        await broker.publish(None, "task_created", {"task": initial_task, "automatic": True})
        return initial_task, await scheduler.schedule(job["id"], limit=1)

    @app.post("/api/jobs")
    async def create_job(request: CreateJob) -> dict[str, Any]:
        job = store.create_job(request.title, request.goal)
        await broker.publish(None, "job_created", {"job": job})
        initial_task, launched = await bootstrap_job(job)
        return {**job, "initialTask": initial_task, "launched": launched}

    @app.post("/api/jobs/{job_id}/bootstrap")
    async def bootstrap_draft_job(job_id: str) -> dict[str, Any]:
        try:
            job = store.get_job(job_id)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown job") from None
        if store.list_tasks(job_id):
            raise HTTPException(status_code=409, detail="job already has tasks; schedule or manage those tasks directly")
        initial_task, launched = await bootstrap_job(job)
        return {"job": store.get_job(job_id), "initialTask": initial_task, "launched": launched}

    @app.post("/api/jobs/{job_id}/tasks")
    async def create_task(job_id: str, request: CreateTask) -> dict[str, Any]:
        try:
            task = store.create_task(job_id, request.title, request.instructions, request.role, request.write_scope, request.status)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown job") from None
        except ValueError as error:
            raise HTTPException(status_code=422, detail=str(error)) from error
        await broker.publish(None, "task_created", {"task": task})
        return task

    @app.get("/api/jobs/{job_id}/tasks")
    def list_tasks(job_id: str) -> dict[str, Any]:
        try:
            return {"tasks": store.list_tasks(job_id)}
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown job") from None

    @app.post("/api/tasks/{task_id}/dependencies")
    async def create_task_dependency(task_id: str, request: CreateTaskDependency) -> dict[str, Any]:
        try:
            store.add_task_dependency(task_id, request.dependency_task_id, request.relation)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown task") from None
        except ValueError as error:
            raise HTTPException(status_code=422, detail=str(error)) from error
        await broker.publish(None, "task_dependency_created", {"taskId": task_id, **request.model_dump()})
        return {"ok": True}

    @app.get("/api/jobs/{job_id}/hypotheses")
    def list_hypotheses(job_id: str) -> dict[str, Any]:
        try:
            return {"hypotheses": store.list_hypotheses(job_id)}
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown job") from None

    @app.get("/api/write-scope-requests")
    def list_write_scope_requests(status: str | None = Query(default=None, pattern="^(pending|approved|rejected)$")) -> dict[str, Any]:
        return {"requests": store.list_write_scope_requests(status)}

    @app.post("/api/write-scope-requests/{request_id}/resolve")
    async def resolve_write_scope_request(request_id: str, request: ResolveWriteScopeRequest) -> dict[str, Any]:
        try:
            scope_request = store.resolve_write_scope_request(request_id, request.decision, request.reason)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown write scope request") from None
        except ValueError as error:
            raise HTTPException(status_code=422, detail=str(error)) from error
        await broker.publish(scope_request["agent_id"], "write_scope_resolved", {"request": scope_request})
        return scope_request

    @app.post("/api/tasks/{task_id}/recover")
    async def recover_task(task_id: str, request: RecoverTask) -> dict[str, Any]:
        try:
            return await manager.recover_task(task_id, request.reason)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown task") from None
        except ValueError as error:
            raise HTTPException(status_code=422, detail=str(error)) from error

    @app.post("/api/tasks/{task_id}/retry")
    async def retry_task(task_id: str, request: RequeueTask) -> dict[str, Any]:
        try:
            task = store.requeue_task(task_id, request.reason)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown task") from None
        except ValueError as error:
            raise HTTPException(status_code=422, detail=str(error)) from error
        await broker.publish(None, "task_requeued", {"task": task})
        return task

    @app.post("/api/jobs/{job_id}/schedule")
    async def schedule(job_id: str, limit: int = Query(default=1, ge=1, le=8)) -> dict[str, Any]:
        try:
            launched = await scheduler.schedule(job_id, limit)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown job") from None
        return {"launched": launched}

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
            return await manager.launch(agent_id)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown agent") from None
        except Exception as error:
            raise HTTPException(status_code=409, detail=str(error)) from error

    @app.post("/api/agents/{agent_id}/control")
    async def control_agent(agent_id: str, request: AgentControl) -> dict[str, Any]:
        try:
            await manager.control(agent_id, request.action, request.message)
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

    @app.get("/internal/agents/{agent_id}/context")
    async def agent_context(agent_id: str, x_re_daemon_token: str | None = Header(default=None)) -> dict[str, Any]:
        require_internal(x_re_daemon_token)
        try:
            agent = store.get_agent(agent_id)
            job = store.get_job(agent["job_id"])
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown agent") from None
        task = store.task_for_agent(agent_id)
        dependencies = store.task_dependencies(task["id"]) if task is not None else []
        ghidra_context = {**ghidra.status(), "queryInstruction": "The database opens lazily when re_ghidra_query is called. opened=false means idle, not unavailable."}
        return {"agent": agent, "job": job, "task": task, "dependencies": dependencies, "ghidra": ghidra_context}

    @app.post("/internal/agents/{agent_id}/events")
    async def record_agent_event(agent_id: str, request: RecordEvent, x_re_daemon_token: str | None = Header(default=None)) -> dict[str, Any]:
        require_internal(x_re_daemon_token)
        try:
            store.get_agent(agent_id)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown agent") from None
        return await broker.publish(agent_id, request.kind, request.payload)

    @app.post("/internal/agents/{agent_id}/ghidra")
    async def query_ghidra(agent_id: str, request: GhidraQuery, x_re_daemon_token: str | None = Header(default=None)) -> dict[str, Any]:
        require_internal(x_re_daemon_token)
        try:
            agent = store.get_agent(agent_id)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown agent") from None
        task = store.task_for_agent(agent_id)
        if request.use_cache:
            cached = store.find_evidence(agent["job_id"], "ghidra", request.operation, request.arguments)
            if cached is not None:
                evidence = {key: value for key, value in cached.items() if key != "response"}
                await broker.publish(agent_id, "ghidra_cache_hit", {"operation": request.operation, "evidence": evidence})
                return {"evidence": evidence, "response": cached["response"], "cacheHit": True}
        try:
            response = await ghidra.query(request.operation, request.arguments)
        except McpError as error:
            await broker.publish(agent_id, "ghidra_query_failed", {"operation": request.operation, "error": str(error)})
            raise HTTPException(status_code=503, detail=str(error)) from error
        evidence = store.record_evidence(agent["job_id"], task["id"] if task else None, agent_id, "ghidra", request.operation, request.arguments, response)
        await broker.publish(agent_id, "ghidra_evidence_recorded", {"operation": request.operation, "evidence": evidence})
        return {"evidence": evidence, "response": response, "cacheHit": False}

    @app.post("/internal/agents/{agent_id}/hypotheses")
    async def record_agent_hypothesis(agent_id: str, request: RecordHypothesis, x_re_daemon_token: str | None = Header(default=None)) -> dict[str, Any]:
        require_internal(x_re_daemon_token)
        try:
            agent = store.get_agent(agent_id)
        except KeyError:
            raise HTTPException(status_code=404, detail="unknown agent") from None
        task = store.task_for_agent(agent_id)
        try:
            hypothesis = store.record_hypothesis(
                agent["job_id"], task["id"] if task else None, agent_id, request.subject, request.statement,
                request.evidence_ids, request.status, request.supersedes_id,
            )
        except ValueError as error:
            raise HTTPException(status_code=422, detail=str(error)) from error
        await broker.publish(agent_id, "hypothesis_recorded", {"hypothesis": hypothesis})
        return hypothesis

    @app.post("/internal/agents/{agent_id}/task/expand")
    async def expand_agent_task_graph(agent_id: str, request: ExpandTaskGraph, x_re_daemon_token: str | None = Header(default=None)) -> dict[str, Any]:
        require_internal(x_re_daemon_token)
        try:
            expansion = store.expand_task_graph(
                agent_id, request.rationale,
                [task.model_dump() for task in request.tasks],
                [edge.model_dump() for edge in request.dependencies],
            )
        except (KeyError, ValueError) as error:
            raise HTTPException(status_code=422, detail=str(error)) from error
        await broker.publish(agent_id, "task_graph_expanded", {
            "expansionId": expansion["id"], "sourceTaskId": expansion["sourceTaskId"],
            "tasks": [{key: task[key] for key in ("id", "key", "title", "role")} for task in expansion["tasks"]],
            "edgeCount": len(expansion["edges"]), "idempotentReplay": expansion["idempotentReplay"],
        })
        return expansion

    @app.post("/internal/agents/{agent_id}/task/transition")
    async def transition_agent_task(agent_id: str, request: TaskTransition, x_re_daemon_token: str | None = Header(default=None)) -> dict[str, Any]:
        require_internal(x_re_daemon_token)
        task = store.task_for_agent(agent_id)
        if task is None:
            raise HTTPException(status_code=409, detail="agent has no scheduler-assigned task")
        if task["status"] != "in_progress":
            raise HTTPException(status_code=409, detail="assigned task is no longer in progress")
        if request.status == "completed" and task["title"] == "Initial evidence triage" and not store.has_task_expansion(task["id"]):
            raise HTTPException(status_code=409, detail="initial triage must persist a follow-on graph with re_expand_task_graph before completion")
        try:
            transitioned = store.transition_task(task["id"], request.status, request.reason, agent_id)
        except ValueError as error:
            raise HTTPException(status_code=422, detail=str(error)) from error
        await broker.publish(agent_id, "task_transitioned", {"task": transitioned})
        # A terminal task outcome ends the agent attempt. Without this explicit
        # lifecycle signal Pi may continue autonomous turns after it has already
        # reported success, growing the transcript and consuming model budget.
        try:
            await manager.control(agent_id, "abort")
        except (KeyError, RuntimeError) as error:
            await broker.publish(agent_id, "terminal_abort_unavailable", {"taskId": task["id"], "error": str(error)})
        else:
            await broker.publish(agent_id, "terminal_abort_requested", {"taskId": task["id"], "status": request.status})
        launched = await scheduler.drive(task["job_id"])
        await broker.publish(None, "task_graph_advanced", {
            "jobId": task["job_id"], "completedTaskId": task["id"],
            "launchedTaskIds": [item["task"]["id"] for item in launched],
        })
        return transitioned

    @app.post("/internal/agents/{agent_id}/write-scope-requests")
    async def request_agent_write_scope(agent_id: str, request: WriteScopeRequest, x_re_daemon_token: str | None = Header(default=None)) -> dict[str, Any]:
        require_internal(x_re_daemon_token)
        try:
            scope_request = store.request_write_scope(agent_id, (store.task_for_agent(agent_id) or {}).get("id"), request.path, request.reason)
        except (KeyError, ValueError) as error:
            raise HTTPException(status_code=422, detail=str(error)) from error
        await broker.publish(agent_id, "write_scope_requested", {"request": scope_request})
        return scope_request

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
