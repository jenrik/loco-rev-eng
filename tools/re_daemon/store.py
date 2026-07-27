"""Durable single-writer store for the autonomous RE daemon."""

from __future__ import annotations

from contextlib import contextmanager
import json
from pathlib import Path
import sqlite3
import threading
from typing import Any
from uuid import uuid4


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
            connection.commit()

    def create_job(self, title: str, goal: str) -> dict[str, Any]:
        if not title.strip() or not goal.strip():
            raise ValueError("title and goal are required")
        job_id = f"job-{uuid4().hex}"
        with self._lock, self._connect() as connection:
            connection.execute(
                "INSERT INTO jobs(id, title, goal, status) VALUES (?, ?, ?, 'queued')",
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
            connection.execute("UPDATE jobs SET status = 'running', updated_at = CURRENT_TIMESTAMP WHERE id = ?", (job_id,))
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

    def snapshot(self) -> dict[str, Any]:
        with self._lock, self._connect() as connection:
            jobs = [dict(row) for row in connection.execute("SELECT * FROM jobs ORDER BY created_at DESC")]
            agents = [self._agent_row(row) for row in connection.execute("SELECT * FROM agents ORDER BY created_at DESC")]
            last_sequence = connection.execute("SELECT COALESCE(MAX(sequence), 0) FROM events").fetchone()[0]
        return {"jobs": jobs, "agents": agents, "lastSequence": last_sequence}
