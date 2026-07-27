"""Durable single-writer store for the autonomous RE daemon."""

from __future__ import annotations

from contextlib import contextmanager
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import threading
from typing import Any
from uuid import uuid4


AUTONOMOUS_TASK_ROLES = {"investigator", "transcriber", "validator", "integrator", "reviewer"}
TASK_PLAN_KEY = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._:@/-]{0,119}$")
MAX_EXPANSION_TASKS = 24
MAX_EXPANSION_EDGES = 64

# A requires edge imposes execution order, not a successful-outcome contract.
# Once a prerequisite has reached any terminal state, its successor must be
# allowed to inspect that outcome and continue (or report its own block).
# Otherwise one blocked/deferred root strands every nominally ready descendant.
ACTIVE_TASK_STATUSES = ("ready", "in_progress")


class DaemonStore:
    """SQLite-backed state store.

    The daemon is the only writer. WAL allows dashboard readers to proceed while
    short event transactions are committed, but does not make writes concurrent.
    """

    def __init__(self, path: str | Path):
        self.path = Path(path)
        self._lock = threading.RLock()

    @contextmanager
    def _connect(self):
        connection = sqlite3.connect(self.path, timeout=5.0)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA busy_timeout = 5000")
        try:
            yield connection
        finally:
            connection.close()

    def initialize(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self._lock, self._connect() as connection:
            connection.execute("PRAGMA journal_mode = WAL")
            connection.execute("""
                CREATE TABLE IF NOT EXISTS jobs (
                    id TEXT PRIMARY KEY,
                    title TEXT NOT NULL,
                    goal TEXT NOT NULL,
                    status TEXT NOT NULL,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
                )
            """)
            connection.execute("""
                CREATE TABLE IF NOT EXISTS agents (
                    id TEXT PRIMARY KEY,
                    job_id TEXT NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
                    role TEXT NOT NULL,
                    task TEXT NOT NULL,
                    status TEXT NOT NULL,
                    session_dir TEXT NOT NULL,
                    write_scope_json TEXT NOT NULL DEFAULT '[]',
                    pid INTEGER,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
                )
            """)
            connection.execute("""
                CREATE TABLE IF NOT EXISTS events (
                    sequence INTEGER PRIMARY KEY AUTOINCREMENT,
                    agent_id TEXT REFERENCES agents(id) ON DELETE CASCADE,
                    kind TEXT NOT NULL,
                    payload_json TEXT NOT NULL,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
                )
            """)
            columns = {row[1] for row in connection.execute("PRAGMA table_info(agents)")}
            if "write_scope_json" not in columns:
                connection.execute("ALTER TABLE agents ADD COLUMN write_scope_json TEXT NOT NULL DEFAULT '[]'")
            connection.execute("CREATE INDEX IF NOT EXISTS events_agent_sequence ON events(agent_id, sequence)")
            connection.execute("""
                CREATE TABLE IF NOT EXISTS tasks (
                    id TEXT PRIMARY KEY,
                    job_id TEXT NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
                    title TEXT NOT NULL,
                    instructions TEXT NOT NULL,
                    role TEXT NOT NULL,
                    status TEXT NOT NULL,
                    write_scope_json TEXT NOT NULL DEFAULT '[]',
                    assigned_agent_id TEXT REFERENCES agents(id) ON DELETE SET NULL,
                    transition_reason TEXT,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
                )
            """)
            connection.execute("""
                CREATE TABLE IF NOT EXISTS task_edges (
                    task_id TEXT NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
                    dependency_task_id TEXT NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
                    relation TEXT NOT NULL,
                    PRIMARY KEY(task_id, dependency_task_id, relation),
                    CHECK(task_id <> dependency_task_id)
                )
            """)
            connection.execute("""
                CREATE TABLE IF NOT EXISTS task_expansions (
                    id TEXT PRIMARY KEY,
                    job_id TEXT NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
                    source_task_id TEXT NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
                    source_agent_id TEXT NOT NULL REFERENCES agents(id) ON DELETE CASCADE,
                    rationale TEXT NOT NULL,
                    request_digest TEXT NOT NULL,
                    result_json TEXT NOT NULL,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    UNIQUE(source_task_id, request_digest)
                )
            """)
            connection.execute("""
                CREATE TABLE IF NOT EXISTS evidence_revisions (
                    id TEXT PRIMARY KEY,
                    job_id TEXT NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
                    task_id TEXT REFERENCES tasks(id) ON DELETE SET NULL,
                    agent_id TEXT REFERENCES agents(id) ON DELETE SET NULL,
                    source TEXT NOT NULL,
                    operation TEXT NOT NULL,
                    request_json TEXT NOT NULL,
                    evidence_key TEXT NOT NULL,
                    revision INTEGER NOT NULL,
                    digest TEXT NOT NULL,
                    artifact_path TEXT NOT NULL,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    UNIQUE(evidence_key, revision),
                    UNIQUE(evidence_key, digest)
                )
            """)
            connection.execute("""
                CREATE TABLE IF NOT EXISTS write_scope_requests (
                    id TEXT PRIMARY KEY,
                    task_id TEXT REFERENCES tasks(id) ON DELETE CASCADE,
                    agent_id TEXT NOT NULL REFERENCES agents(id) ON DELETE CASCADE,
                    path TEXT NOT NULL,
                    reason TEXT NOT NULL,
                    status TEXT NOT NULL,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    resolved_at TEXT,
                    resolution_reason TEXT
                )
            """)
            scope_columns = {row[1] for row in connection.execute("PRAGMA table_info(write_scope_requests)")}
            if "resolution_reason" not in scope_columns:
                connection.execute("ALTER TABLE write_scope_requests ADD COLUMN resolution_reason TEXT")
            connection.execute("""
                CREATE TABLE IF NOT EXISTS hypotheses (
                    id TEXT PRIMARY KEY,
                    job_id TEXT NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
                    task_id TEXT REFERENCES tasks(id) ON DELETE SET NULL,
                    agent_id TEXT REFERENCES agents(id) ON DELETE SET NULL,
                    subject TEXT NOT NULL,
                    statement TEXT NOT NULL,
                    evidence_ids_json TEXT NOT NULL DEFAULT '[]',
                    status TEXT NOT NULL,
                    hypothesis_key TEXT NOT NULL,
                    revision INTEGER NOT NULL,
                    supersedes_id TEXT REFERENCES hypotheses(id) ON DELETE SET NULL,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    UNIQUE(hypothesis_key, revision)
                )
            """)
            connection.execute("CREATE INDEX IF NOT EXISTS tasks_job_status ON tasks(job_id, status)")
            connection.execute("CREATE INDEX IF NOT EXISTS task_edges_task ON task_edges(task_id, relation)")
            connection.execute("CREATE INDEX IF NOT EXISTS task_expansions_source ON task_expansions(source_task_id, created_at)")
            connection.execute("CREATE INDEX IF NOT EXISTS evidence_task_operation ON evidence_revisions(task_id, operation)")
            connection.execute("CREATE INDEX IF NOT EXISTS hypotheses_job_subject ON hypotheses(job_id, subject, revision)")
            connection.execute("CREATE INDEX IF NOT EXISTS scope_requests_status ON write_scope_requests(status, created_at)")
            connection.commit()

    def create_job(self, title: str, goal: str) -> dict[str, Any]:
        if not title.strip() or not goal.strip():
            raise ValueError("title and goal are required")
        job_id = f"job-{uuid4().hex}"
        with self._lock, self._connect() as connection:
            connection.execute(
                "INSERT INTO jobs(id, title, goal, status) VALUES (?, ?, ?, 'draft')",
                (job_id, title.strip(), goal.strip()),
            )
            connection.commit()
        return self.get_job(job_id)

    def get_job(self, job_id: str) -> dict[str, Any]:
        with self._lock, self._connect() as connection:
            row = connection.execute("SELECT * FROM jobs WHERE id = ?", (job_id,)).fetchone()
        if row is None:
            raise KeyError(job_id)
        return dict(row)

    def _refresh_job_status(self, connection: sqlite3.Connection, job_id: str) -> str:
        statuses = [row["status"] for row in connection.execute("SELECT status FROM tasks WHERE job_id = ?", (job_id,))]
        if not statuses:
            status = "draft"
        elif "in_progress" in statuses:
            status = "running"
        else:
            ready = connection.execute(
                """SELECT 1 FROM tasks AS task WHERE task.job_id = ? AND task.status = 'ready'
                   AND NOT EXISTS (SELECT 1 FROM task_edges AS edge JOIN tasks AS dependency
                                   ON dependency.id = edge.dependency_task_id
                                   WHERE edge.task_id = task.id AND edge.relation = 'requires'
                                   AND dependency.status IN (?, ?)) LIMIT 1""",
                (job_id, *ACTIVE_TASK_STATUSES),
            ).fetchone()
            if ready is not None:
                status = "queued"
            elif all(task_status == "completed" for task_status in statuses):
                status = "completed"
            elif "failed" in statuses:
                status = "failed"
            else:
                status = "blocked"
        connection.execute("UPDATE jobs SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ? AND status != ?", (status, job_id, status))
        return status

    def _refresh_all_job_statuses(self, connection: sqlite3.Connection) -> None:
        for row in connection.execute("SELECT id FROM jobs").fetchall():
            self._refresh_job_status(connection, row["id"])

    def create_agent(self, job_id: str, role: str, task: str, session_dir: str, write_scope: list[str] | None = None) -> dict[str, Any]:
        if not role.strip() or not task.strip() or not session_dir.strip():
            raise ValueError("role, task, and session_dir are required")
        write_scope = list(dict.fromkeys(write_scope or []))
        if any(not isinstance(path, str) or not path or path.startswith("/") or ".." in path.split("/") for path in write_scope):
            raise ValueError("write_scope must contain safe relative paths")
        self.get_job(job_id)
        agent_id = f"agent-{uuid4().hex}"
        with self._lock, self._connect() as connection:
            connection.execute(
                "INSERT INTO agents(id, job_id, role, task, status, session_dir, write_scope_json) VALUES (?, ?, ?, ?, 'queued', ?, ?)",
                (agent_id, job_id, role.strip(), task.strip(), session_dir, json.dumps(write_scope)),
            )
            self._refresh_job_status(connection, job_id)
            connection.commit()
        return self.get_agent(agent_id)

    def get_agent(self, agent_id: str) -> dict[str, Any]:
        with self._lock, self._connect() as connection:
            row = connection.execute("SELECT * FROM agents WHERE id = ?", (agent_id,)).fetchone()
        if row is None:
            raise KeyError(agent_id)
        return self._agent_row(row)

    @staticmethod
    def _agent_row(row: sqlite3.Row) -> dict[str, Any]:
        result = dict(row)
        result["write_scope"] = json.loads(result.pop("write_scope_json", "[]"))
        return result

    def set_agent_status(self, agent_id: str, status: str, pid: int | None = None) -> dict[str, Any]:
        allowed = {"queued", "starting", "running", "settled", "failed", "aborted"}
        if status not in allowed:
            raise ValueError(f"invalid agent status: {status}")
        with self._lock, self._connect() as connection:
            if pid is None:
                result = connection.execute(
                    "UPDATE agents SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?", (status, agent_id)
                )
            else:
                result = connection.execute(
                    "UPDATE agents SET status = ?, pid = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                    (status, pid, agent_id),
                )
            if result.rowcount != 1:
                raise KeyError(agent_id)
            connection.commit()
        return self.get_agent(agent_id)

    def record_event(self, agent_id: str | None, kind: str, payload: dict[str, Any]) -> dict[str, Any]:
        if agent_id is not None:
            self.get_agent(agent_id)
        encoded = json.dumps(payload, separators=(",", ":"), sort_keys=True)
        with self._lock, self._connect() as connection:
            cursor = connection.execute(
                "INSERT INTO events(agent_id, kind, payload_json) VALUES (?, ?, ?)",
                (agent_id, kind, encoded),
            )
            sequence = cursor.lastrowid
            connection.commit()
            row = connection.execute("SELECT * FROM events WHERE sequence = ?", (sequence,)).fetchone()
        return self._event_row(row)

    @staticmethod
    def _event_row(row: sqlite3.Row) -> dict[str, Any]:
        result = dict(row)
        result["payload"] = json.loads(result.pop("payload_json"))
        return result

    def events_after(self, sequence: int = 0, agent_id: str | None = None, limit: int = 200) -> list[dict[str, Any]]:
        limit = max(1, min(limit, 1000))
        query = "SELECT * FROM events WHERE sequence > ?"
        values: list[Any] = [sequence]
        if agent_id is not None:
            query += " AND agent_id = ?"
            values.append(agent_id)
        query += " ORDER BY sequence ASC LIMIT ?"
        values.append(limit)
        with self._lock, self._connect() as connection:
            rows = connection.execute(query, values).fetchall()
        return [self._event_row(row) for row in rows]

    def create_task(
        self,
        job_id: str,
        title: str,
        instructions: str,
        role: str,
        write_scope: list[str] | None = None,
        status: str = "ready",
    ) -> dict[str, Any]:
        if status not in {"ready", "blocked", "deferred"}:
            raise ValueError("new tasks must be ready, blocked, or deferred")
        if not title.strip() or not instructions.strip() or not role.strip():
            raise ValueError("title, instructions, and role are required")
        write_scope = list(dict.fromkeys(write_scope or []))
        if any(not isinstance(path, str) or not path or path.startswith("/") or ".." in path.split("/") for path in write_scope):
            raise ValueError("write_scope must contain safe relative paths")
        self.get_job(job_id)
        task_id = f"task-{uuid4().hex}"
        with self._lock, self._connect() as connection:
            connection.execute(
                "INSERT INTO tasks(id, job_id, title, instructions, role, status, write_scope_json) VALUES (?, ?, ?, ?, ?, ?, ?)",
                (task_id, job_id, title.strip(), instructions.strip(), role.strip(), status, json.dumps(write_scope)),
            )
            self._refresh_job_status(connection, job_id)
            connection.commit()
        return self.get_task(task_id)

    @staticmethod
    def _task_row(row: sqlite3.Row) -> dict[str, Any]:
        result = dict(row)
        result["write_scope"] = json.loads(result.pop("write_scope_json", "[]"))
        return result

    def get_task(self, task_id: str) -> dict[str, Any]:
        with self._lock, self._connect() as connection:
            row = connection.execute("SELECT * FROM tasks WHERE id = ?", (task_id,)).fetchone()
        if row is None:
            raise KeyError(task_id)
        return self._task_row(row)

    def list_tasks(self, job_id: str) -> list[dict[str, Any]]:
        self.get_job(job_id)
        with self._lock, self._connect() as connection:
            rows = connection.execute("SELECT * FROM tasks WHERE job_id = ? ORDER BY created_at ASC", (job_id,)).fetchall()
        return [self._task_row(row) for row in rows]

    def task_for_agent(self, agent_id: str) -> dict[str, Any] | None:
        with self._lock, self._connect() as connection:
            row = connection.execute("SELECT * FROM tasks WHERE assigned_agent_id = ?", (agent_id,)).fetchone()
        return self._task_row(row) if row is not None else None

    def add_task_dependency(self, task_id: str, dependency_task_id: str, relation: str = "requires") -> None:
        if relation not in {"requires", "evidence", "invalidates"}:
            raise ValueError("invalid task edge relation")
        task = self.get_task(task_id)
        dependency = self.get_task(dependency_task_id)
        if task["job_id"] != dependency["job_id"]:
            raise ValueError("task dependencies must stay within one job")
        with self._lock, self._connect() as connection:
            connection.execute(
                "INSERT OR IGNORE INTO task_edges(task_id, dependency_task_id, relation) VALUES (?, ?, ?)",
                (task_id, dependency_task_id, relation),
            )
            connection.commit()

    def list_task_edges(self, job_id: str) -> list[dict[str, Any]]:
        self.get_job(job_id)
        with self._lock, self._connect() as connection:
            rows = connection.execute(
                """SELECT edge.* FROM task_edges AS edge
                   JOIN tasks AS task ON task.id = edge.task_id
                   WHERE task.job_id = ?
                   ORDER BY task.created_at ASC, edge.rowid ASC""",
                (job_id,),
            ).fetchall()
        return [dict(row) for row in rows]

    def task_dependencies(self, task_id: str) -> list[dict[str, Any]]:
        """Return direct prerequisites with their terminal outcome context."""
        self.get_task(task_id)
        with self._lock, self._connect() as connection:
            rows = connection.execute(
                """SELECT dependency.*, edge.relation FROM task_edges AS edge
                   JOIN tasks AS dependency ON dependency.id = edge.dependency_task_id
                   WHERE edge.task_id = ? ORDER BY dependency.created_at ASC""",
                (task_id,),
            ).fetchall()
        return [self._task_row(row) for row in rows]

    def in_progress_tasks(self) -> list[dict[str, Any]]:
        with self._lock, self._connect() as connection:
            rows = connection.execute("SELECT * FROM tasks WHERE status = 'in_progress' ORDER BY updated_at ASC").fetchall()
        return [self._task_row(row) for row in rows]

    def count_in_progress_tasks(self, job_id: str) -> int:
        self.get_job(job_id)
        with self._lock, self._connect() as connection:
            return int(connection.execute(
                "SELECT COUNT(*) FROM tasks WHERE job_id = ? AND status = 'in_progress'", (job_id,)
            ).fetchone()[0])

    def claim_ready_tasks(self, job_id: str, limit: int = 10) -> list[dict[str, Any]]:
        """Atomically claim dependency-ready work and retain resume provenance."""
        limit = max(1, min(limit, 100))
        claimed: list[dict[str, Any]] = []
        with self._lock, self._connect() as connection:
            rows = connection.execute(
                """
                SELECT task.* FROM tasks AS task
                WHERE task.job_id = ? AND task.status = 'ready'
                  AND NOT EXISTS (
                    SELECT 1 FROM task_edges AS edge
                    JOIN tasks AS dependency ON dependency.id = edge.dependency_task_id
                    WHERE edge.task_id = task.id
                      AND edge.relation = 'requires'
                      AND dependency.status IN (?, ?)
                  )
                ORDER BY task.created_at ASC LIMIT ?
                """,
                (job_id, *ACTIVE_TASK_STATUSES, limit),
            ).fetchall()
            for row in rows:
                updated = connection.execute(
                    """UPDATE tasks SET status = 'in_progress', transition_reason = 'claimed by daemon scheduler',
                       updated_at = CURRENT_TIMESTAMP WHERE id = ? AND status = 'ready'""",
                    (row["id"],),
                )
                if updated.rowcount != 1:
                    continue
                task = self._task_row(row)
                task["_claim_previous_reason"] = task.get("transition_reason")
                task["status"] = "in_progress"
                task["transition_reason"] = "claimed by daemon scheduler"
                claimed.append(task)
            self._refresh_job_status(connection, job_id)
            connection.commit()
        return claimed

    def ready_tasks(self, job_id: str, limit: int = 10) -> list[dict[str, Any]]:
        limit = max(1, min(limit, 100))
        with self._lock, self._connect() as connection:
            rows = connection.execute(
                """
                SELECT task.* FROM tasks AS task
                WHERE task.job_id = ? AND task.status = 'ready'
                  AND NOT EXISTS (
                    SELECT 1 FROM task_edges AS edge
                    JOIN tasks AS dependency ON dependency.id = edge.dependency_task_id
                    WHERE edge.task_id = task.id
                      AND edge.relation = 'requires'
                      AND dependency.status IN (?, ?)
                  )
                ORDER BY task.created_at ASC LIMIT ?
                """,
                (job_id, *ACTIVE_TASK_STATUSES, limit),
            ).fetchall()
        return [self._task_row(row) for row in rows]

    def expand_task_graph(
        self,
        agent_id: str,
        rationale: str,
        tasks: list[dict[str, Any]],
        dependencies: list[dict[str, Any]] | None = None,
    ) -> dict[str, Any]:
        """Atomically add a bounded, acyclic follow-on plan from an active task.

        Each prerequisite-most new node automatically requires the source task,
        so an operator scheduling click cannot race the planning agent's final
        evidence and terminal transition. Identical requests are idempotent.
        """
        rationale = rationale.strip()
        dependencies = dependencies or []
        if not rationale:
            raise ValueError("task graph expansion requires a rationale")
        if not 1 <= len(tasks) <= MAX_EXPANSION_TASKS:
            raise ValueError(f"task graph expansion must contain 1-{MAX_EXPANSION_TASKS} tasks")
        if len(dependencies) > MAX_EXPANSION_EDGES:
            raise ValueError(f"task graph expansion may contain at most {MAX_EXPANSION_EDGES} edges")

        normalized_tasks: list[dict[str, Any]] = []
        keys: set[str] = set()
        for specification in tasks:
            key = specification.get("key")
            title = specification.get("title")
            instructions = specification.get("instructions")
            role = specification.get("role")
            scope = specification.get("write_scope", [])
            if not isinstance(key, str) or TASK_PLAN_KEY.fullmatch(key) is None:
                raise ValueError("task keys must be 1-120 safe semantic characters")
            if key in keys:
                raise ValueError(f"duplicate task key: {key}")
            if not isinstance(title, str) or not title.strip() or len(title.strip()) > 200:
                raise ValueError(f"task {key} requires a title of at most 200 characters")
            if not isinstance(instructions, str) or not instructions.strip() or len(instructions.strip()) > 16000:
                raise ValueError(f"task {key} requires concrete instructions of at most 16000 characters")
            if role not in AUTONOMOUS_TASK_ROLES:
                raise ValueError(f"task {key} has unsupported autonomous role: {role}")
            if not isinstance(scope, list):
                raise ValueError(f"task {key} write_scope must be an array")
            scope = list(dict.fromkeys(scope))
            if any(not isinstance(path, str) or not path or path.startswith("/") or ".." in path.split("/") for path in scope):
                raise ValueError(f"task {key} write_scope must contain safe relative paths")
            keys.add(key)
            normalized_tasks.append({
                "key": key, "title": title.strip(), "instructions": instructions.strip(),
                "role": role, "write_scope": scope,
            })

        normalized_dependencies: list[dict[str, str]] = []
        seen_edges: set[tuple[str, str, str]] = set()
        requires: dict[str, set[str]] = {key: set() for key in keys}
        for edge in dependencies:
            task_key = edge.get("task_key")
            dependency_key = edge.get("dependency_key")
            relation = edge.get("relation", "requires")
            if task_key not in keys or dependency_key not in keys:
                raise ValueError("task graph edges may reference only keys in the same expansion")
            if task_key == dependency_key:
                raise ValueError("a task cannot depend on itself")
            if relation not in {"requires", "evidence", "invalidates"}:
                raise ValueError(f"invalid task edge relation: {relation}")
            identity = (task_key, dependency_key, relation)
            if identity in seen_edges:
                continue
            seen_edges.add(identity)
            normalized_dependencies.append({
                "task_key": task_key, "dependency_key": dependency_key, "relation": relation,
            })
            if relation == "requires":
                requires[task_key].add(dependency_key)

        visiting: set[str] = set()
        visited: set[str] = set()
        def visit(key: str) -> None:
            if key in visiting:
                raise ValueError("requires edges must form an acyclic graph")
            if key in visited:
                return
            visiting.add(key)
            for dependency_key in requires[key]:
                visit(dependency_key)
            visiting.remove(key)
            visited.add(key)
        for key in keys:
            visit(key)

        canonical_request = json.dumps(
            {"rationale": rationale, "tasks": normalized_tasks, "dependencies": normalized_dependencies},
            separators=(",", ":"), sort_keys=True,
        )
        request_digest = hashlib.sha256(canonical_request.encode("utf-8")).hexdigest()

        with self._lock, self._connect() as connection:
            source = connection.execute(
                """SELECT task.* FROM tasks AS task JOIN agents AS agent
                   ON agent.id = task.assigned_agent_id
                   WHERE agent.id = ? AND task.status = 'in_progress' AND task.job_id = agent.job_id""",
                (agent_id,),
            ).fetchone()
            if source is None:
                raise ValueError("only the agent assigned to an in-progress task may expand its graph")
            existing = connection.execute(
                "SELECT result_json FROM task_expansions WHERE source_task_id = ? AND request_digest = ?",
                (source["id"], request_digest),
            ).fetchone()
            if existing is not None:
                result = json.loads(existing["result_json"])
                result["idempotentReplay"] = True
                return result

            ids = {specification["key"]: f"task-{uuid4().hex}" for specification in normalized_tasks}
            for specification in normalized_tasks:
                connection.execute(
                    """INSERT INTO tasks(id, job_id, title, instructions, role, status, write_scope_json)
                       VALUES (?, ?, ?, ?, ?, 'ready', ?)""",
                    (
                        ids[specification["key"]], source["job_id"], specification["title"],
                        specification["instructions"], specification["role"], json.dumps(specification["write_scope"]),
                    ),
                )

            result_edges: list[dict[str, Any]] = []
            for edge in normalized_dependencies:
                task_id = ids[edge["task_key"]]
                dependency_id = ids[edge["dependency_key"]]
                connection.execute(
                    "INSERT INTO task_edges(task_id, dependency_task_id, relation) VALUES (?, ?, ?)",
                    (task_id, dependency_id, edge["relation"]),
                )
                result_edges.append({**edge, "task_id": task_id, "dependency_task_id": dependency_id, "automatic": False})

            # Nodes without an internal blocking prerequisite are the executable
            # roots of this expansion. Gate each on the source task automatically.
            for key in sorted(keys):
                if requires[key]:
                    continue
                connection.execute(
                    "INSERT INTO task_edges(task_id, dependency_task_id, relation) VALUES (?, ?, 'requires')",
                    (ids[key], source["id"]),
                )
                result_edges.append({
                    "task_key": key, "dependency_key": "$source", "task_id": ids[key],
                    "dependency_task_id": source["id"], "relation": "requires", "automatic": True,
                })

            expansion_id = f"expansion-{uuid4().hex}"
            result = {
                "id": expansion_id,
                "jobId": source["job_id"],
                "sourceTaskId": source["id"],
                "rationale": rationale,
                "tasks": [{**specification, "id": ids[specification["key"]], "status": "ready"} for specification in normalized_tasks],
                "edges": result_edges,
                "idempotentReplay": False,
            }
            connection.execute(
                """INSERT INTO task_expansions(id, job_id, source_task_id, source_agent_id, rationale, request_digest, result_json)
                   VALUES (?, ?, ?, ?, ?, ?, ?)""",
                (expansion_id, source["job_id"], source["id"], agent_id, rationale, request_digest, json.dumps(result)),
            )
            self._refresh_job_status(connection, source["job_id"])
            connection.commit()
        return result

    def has_task_expansion(self, task_id: str) -> bool:
        with self._lock, self._connect() as connection:
            return connection.execute(
                "SELECT 1 FROM task_expansions WHERE source_task_id = ? LIMIT 1", (task_id,)
            ).fetchone() is not None

    def transition_task(self, task_id: str, status: str, reason: str | None = None, agent_id: str | None = None) -> dict[str, Any]:
        allowed = {"ready", "in_progress", "completed", "blocked", "deferred", "failed"}
        if status not in allowed:
            raise ValueError(f"invalid task status: {status}")
        if status in {"blocked", "deferred", "failed"} and not (reason or "").strip():
            raise ValueError(f"{status} tasks require a reason")
        with self._lock, self._connect() as connection:
            task_row = connection.execute("SELECT job_id FROM tasks WHERE id = ?", (task_id,)).fetchone()
            if task_row is None:
                raise KeyError(task_id)
            if agent_id is None:
                result = connection.execute(
                    "UPDATE tasks SET status = ?, transition_reason = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                    (status, reason, task_id),
                )
            else:
                result = connection.execute(
                    "UPDATE tasks SET status = ?, transition_reason = ?, assigned_agent_id = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                    (status, reason, agent_id, task_id),
                )
            if result.rowcount != 1:
                raise KeyError(task_id)
            self._refresh_job_status(connection, task_row["job_id"])
            connection.commit()
        return self.get_task(task_id)

    def record_evidence(
        self,
        job_id: str,
        task_id: str | None,
        agent_id: str | None,
        source: str,
        operation: str,
        request: dict[str, Any],
        response: dict[str, Any],
    ) -> dict[str, Any]:
        self.get_job(job_id)
        if task_id is not None:
            self.get_task(task_id)
        if agent_id is not None:
            self.get_agent(agent_id)
        request_json = json.dumps(request, separators=(",", ":"), sort_keys=True)
        evidence_key = hashlib.sha256(f"{source}\0{operation}\0{request_json}".encode("utf-8")).hexdigest()
        artifact_json = json.dumps(response, separators=(",", ":"), sort_keys=True).encode("utf-8")
        digest = hashlib.sha256(artifact_json).hexdigest()
        artifacts = self.path.parent / "artifacts"
        artifacts.mkdir(parents=True, exist_ok=True)
        artifact_path = artifacts / f"{digest}.json"
        if not artifact_path.exists():
            temporary = artifact_path.with_suffix(".tmp")
            temporary.write_bytes(artifact_json)
            temporary.replace(artifact_path)
        with self._lock, self._connect() as connection:
            existing = connection.execute(
                "SELECT * FROM evidence_revisions WHERE evidence_key = ? AND digest = ?", (evidence_key, digest)
            ).fetchone()
            if existing is not None:
                return dict(existing)
            revision = connection.execute(
                "SELECT COALESCE(MAX(revision), 0) + 1 FROM evidence_revisions WHERE evidence_key = ?", (evidence_key,)
            ).fetchone()[0]
            evidence_id = f"evidence-{uuid4().hex}"
            connection.execute(
                """
                INSERT INTO evidence_revisions(id, job_id, task_id, agent_id, source, operation, request_json, evidence_key, revision, digest, artifact_path)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (evidence_id, job_id, task_id, agent_id, source, operation, request_json, evidence_key, revision, digest, str(artifact_path)),
            )
            connection.commit()
            row = connection.execute("SELECT * FROM evidence_revisions WHERE id = ?", (evidence_id,)).fetchone()
        return dict(row)

    def find_evidence(self, job_id: str, source: str, operation: str, request: dict[str, Any]) -> dict[str, Any] | None:
        request_json = json.dumps(request, separators=(",", ":"), sort_keys=True)
        evidence_key = hashlib.sha256(f"{source}\0{operation}\0{request_json}".encode("utf-8")).hexdigest()
        with self._lock, self._connect() as connection:
            row = connection.execute(
                "SELECT * FROM evidence_revisions WHERE job_id = ? AND evidence_key = ? ORDER BY revision DESC LIMIT 1", (job_id, evidence_key)
            ).fetchone()
        return self.load_evidence(row["id"]) if row is not None else None

    @staticmethod
    def _hypothesis_row(row: sqlite3.Row) -> dict[str, Any]:
        result = dict(row)
        result["evidence_ids"] = json.loads(result.pop("evidence_ids_json", "[]"))
        return result

    def record_hypothesis(
        self, job_id: str, task_id: str | None, agent_id: str | None, subject: str, statement: str,
        evidence_ids: list[str], status: str = "tentative", supersedes_id: str | None = None,
    ) -> dict[str, Any]:
        if status not in {"tentative", "supported", "rejected", "superseded"}:
            raise ValueError("invalid hypothesis status")
        if not subject.strip() or not statement.strip():
            raise ValueError("hypothesis subject and statement are required")
        self.get_job(job_id)
        if task_id is not None:
            self.get_task(task_id)
        if agent_id is not None:
            self.get_agent(agent_id)
        evidence_ids = list(dict.fromkeys(evidence_ids))
        with self._lock, self._connect() as connection:
            for evidence_id in evidence_ids:
                evidence = connection.execute("SELECT job_id FROM evidence_revisions WHERE id = ?", (evidence_id,)).fetchone()
                if evidence is None or evidence["job_id"] != job_id:
                    raise ValueError("hypothesis evidence must belong to the same job")
            if supersedes_id is not None:
                predecessor = connection.execute("SELECT job_id FROM hypotheses WHERE id = ?", (supersedes_id,)).fetchone()
                if predecessor is None or predecessor["job_id"] != job_id:
                    raise ValueError("superseded hypothesis must belong to the same job")
            normalized_subject = subject.strip()
            hypothesis_key = hashlib.sha256(f"{job_id}\0{normalized_subject}".encode("utf-8")).hexdigest()
            revision = connection.execute(
                "SELECT COALESCE(MAX(revision), 0) + 1 FROM hypotheses WHERE hypothesis_key = ?", (hypothesis_key,)
            ).fetchone()[0]
            hypothesis_id = f"hypothesis-{uuid4().hex}"
            connection.execute(
                """
                INSERT INTO hypotheses(id, job_id, task_id, agent_id, subject, statement, evidence_ids_json, status, hypothesis_key, revision, supersedes_id)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (hypothesis_id, job_id, task_id, agent_id, normalized_subject, statement.strip(), json.dumps(evidence_ids), status, hypothesis_key, revision, supersedes_id),
            )
            connection.commit()
            row = connection.execute("SELECT * FROM hypotheses WHERE id = ?", (hypothesis_id,)).fetchone()
        return self._hypothesis_row(row)

    def list_hypotheses(self, job_id: str) -> list[dict[str, Any]]:
        self.get_job(job_id)
        with self._lock, self._connect() as connection:
            rows = connection.execute("SELECT * FROM hypotheses WHERE job_id = ? ORDER BY created_at DESC, revision DESC", (job_id,)).fetchall()
        return [self._hypothesis_row(row) for row in rows]

    def list_write_scope_requests(self, status: str | None = None) -> list[dict[str, Any]]:
        if status is not None and status not in {"pending", "approved", "rejected"}:
            raise ValueError("invalid write scope status")
        query = "SELECT * FROM write_scope_requests"
        values: tuple[Any, ...] = ()
        if status is not None:
            query += " WHERE status = ?"
            values = (status,)
        query += " ORDER BY created_at ASC"
        with self._lock, self._connect() as connection:
            rows = connection.execute(query, values).fetchall()
        return [dict(row) for row in rows]

    def resolve_write_scope_request(self, request_id: str, decision: str, reason: str | None = None) -> dict[str, Any]:
        if decision not in {"approved", "rejected"}:
            raise ValueError("write scope decision must be approved or rejected")
        if decision == "rejected" and not (reason or "").strip():
            raise ValueError("rejected write scope requests require a reason")
        with self._lock, self._connect() as connection:
            request = connection.execute("SELECT * FROM write_scope_requests WHERE id = ?", (request_id,)).fetchone()
            if request is None:
                raise KeyError(request_id)
            if request["status"] != "pending":
                raise ValueError("write scope request has already been resolved")
            if decision == "approved":
                if request["task_id"] is not None:
                    task = connection.execute("SELECT write_scope_json FROM tasks WHERE id = ?", (request["task_id"],)).fetchone()
                    if task is not None:
                        scope = list(dict.fromkeys(json.loads(task["write_scope_json"]) + [request["path"]]))
                        connection.execute("UPDATE tasks SET write_scope_json = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?", (json.dumps(scope), request["task_id"]))
                agent = connection.execute("SELECT write_scope_json FROM agents WHERE id = ?", (request["agent_id"],)).fetchone()
                if agent is not None:
                    scope = list(dict.fromkeys(json.loads(agent["write_scope_json"]) + [request["path"]]))
                    connection.execute("UPDATE agents SET write_scope_json = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?", (json.dumps(scope), request["agent_id"]))
            connection.execute(
                "UPDATE write_scope_requests SET status = ?, resolution_reason = ?, resolved_at = CURRENT_TIMESTAMP WHERE id = ?",
                (decision, reason.strip() if reason else None, request_id),
            )
            connection.commit()
            row = connection.execute("SELECT * FROM write_scope_requests WHERE id = ?", (request_id,)).fetchone()
        return dict(row)

    def requeue_task(self, task_id: str, reason: str) -> dict[str, Any]:
        if not reason.strip():
            raise ValueError("requeue reason is required")
        with self._lock, self._connect() as connection:
            row = connection.execute("SELECT status, job_id FROM tasks WHERE id = ?", (task_id,)).fetchone()
            if row is None:
                raise KeyError(task_id)
            if row["status"] not in {"blocked", "deferred", "failed"}:
                raise ValueError("only blocked, deferred, or failed tasks can be requeued")
            connection.execute(
                "UPDATE tasks SET status = 'ready', assigned_agent_id = NULL, transition_reason = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                (reason.strip(), task_id),
            )
            self._refresh_job_status(connection, row["job_id"])
            connection.commit()
        return self.get_task(task_id)

    def load_evidence(self, evidence_id: str) -> dict[str, Any]:
        with self._lock, self._connect() as connection:
            row = connection.execute("SELECT * FROM evidence_revisions WHERE id = ?", (evidence_id,)).fetchone()
        if row is None:
            raise KeyError(evidence_id)
        result = dict(row)
        result["request"] = json.loads(result.pop("request_json"))
        result["response"] = json.loads(Path(result["artifact_path"]).read_text(encoding="utf-8"))
        return result

    def request_write_scope(self, agent_id: str, task_id: str | None, path: str, reason: str) -> dict[str, Any]:
        if not path or path.startswith("/") or ".." in path.split("/") or not reason.strip():
            raise ValueError("path must be safe and reason is required")
        self.get_agent(agent_id)
        if task_id is not None:
            self.get_task(task_id)
        request_id = f"scope-{uuid4().hex}"
        with self._lock, self._connect() as connection:
            connection.execute(
                "INSERT INTO write_scope_requests(id, task_id, agent_id, path, reason, status) VALUES (?, ?, ?, ?, ?, 'pending')",
                (request_id, task_id, agent_id, path, reason.strip()),
            )
            connection.commit()
            row = connection.execute("SELECT * FROM write_scope_requests WHERE id = ?", (request_id,)).fetchone()
        return dict(row)


    def snapshot(self) -> dict[str, Any]:
        with self._lock, self._connect() as connection:
            self._refresh_all_job_statuses(connection)
            connection.commit()
            jobs = [dict(row) for row in connection.execute("SELECT * FROM jobs ORDER BY created_at DESC")]
            agents = [self._agent_row(row) for row in connection.execute("""
                SELECT agent.*, COALESCE(MAX(event.sequence), 0) AS last_activity_sequence
                FROM agents AS agent
                LEFT JOIN events AS event ON event.agent_id = agent.id
                GROUP BY agent.id
                ORDER BY last_activity_sequence DESC, agent.created_at DESC
            """)]
            tasks = [self._task_row(row) for row in connection.execute("SELECT * FROM tasks ORDER BY created_at DESC")]
            task_edges = [dict(row) for row in connection.execute("SELECT * FROM task_edges ORDER BY rowid ASC")]
            hypotheses = [self._hypothesis_row(row) for row in connection.execute("SELECT * FROM hypotheses ORDER BY created_at DESC, revision DESC")]
            write_scope_requests = [dict(row) for row in connection.execute("SELECT * FROM write_scope_requests ORDER BY created_at DESC")]
            last_sequence = connection.execute("SELECT COALESCE(MAX(sequence), 0) FROM events").fetchone()[0]
        return {"jobs": jobs, "agents": agents, "tasks": tasks, "taskEdges": task_edges, "hypotheses": hypotheses, "writeScopeRequests": write_scope_requests, "lastSequence": last_sequence}
