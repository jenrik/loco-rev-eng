#!/usr/bin/env python3
"""Durable evidence graph and workflow ledger for Pi decompilation sessions.

The program deliberately has no daemon. Each invocation takes an exclusive file
lock, performs one validated state transition, atomically replaces the state
file, and returns exactly one structured JSON object on stdout. This gives the
TypeScript Fabric orchestration layer a crash-safe, inspectable boundary while
leaving PRIMARY agents free to interrogate live Ghidra.
"""

from __future__ import annotations

import argparse
import copy
import datetime as dt
import fcntl
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
from typing import Any

SCHEMA_VERSION = 1
TASK_STATUSES = {
    "open", "active", "transcribed", "validated", "integrated",
    "blocked", "deferred", "invalidated",
}
TASK_PHASES = {"transcribe", "validate", "integrate", "investigate"}
EDGE_KINDS = {"requires", "evidence", "invalidates"}
CONFIDENCE = {"observed", "tentative"}
MAX_EVENTS = 500


class CoreError(Exception):
    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code


def now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat().replace("+00:00", "Z")


def require_object(value: Any, name: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise CoreError("invalid_input", f"{name} must be a JSON object")
    return value


def require_string(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value:
        raise CoreError("invalid_input", f"{name} must be a non-empty string")
    return value


def require_string_list(value: Any, name: str) -> list[str]:
    if not isinstance(value, list) or any(not isinstance(item, str) for item in value):
        raise CoreError("invalid_input", f"{name} must be an array of strings")
    return list(dict.fromkeys(value))


def safe_relative_path(path: str, name: str) -> str:
    path = require_string(path, name)
    candidate = Path(path)
    if candidate.is_absolute() or ".." in candidate.parts:
        raise CoreError("invalid_path", f"{name} must be a relative path without '..'")
    return candidate.as_posix()


def default_state() -> dict[str, Any]:
    timestamp = now()
    return {
        "schemaVersion": SCHEMA_VERSION,
        "createdAt": timestamp,
        "updatedAt": timestamp,
        "binary": {},
        "tasks": {},
        "edges": [],
        "evidence": {},
        "events": [],
    }


def validate_state(state: Any) -> dict[str, Any]:
    state = require_object(state, "state")
    if state.get("schemaVersion") != SCHEMA_VERSION:
        raise CoreError("unsupported_state", "unsupported workflow state schema version")
    for key, expected in (("tasks", dict), ("evidence", dict), ("events", list), ("edges", list)):
        if not isinstance(state.get(key), expected):
            raise CoreError("invalid_state", f"state.{key} has an invalid type")
    if not isinstance(state.get("binary"), dict):
        raise CoreError("invalid_state", "state.binary has an invalid type")
    return state


def append_event(state: dict[str, Any], kind: str, details: dict[str, Any]) -> None:
    state["events"].append({"at": now(), "kind": kind, "details": details})
    if len(state["events"]) > MAX_EVENTS:
        del state["events"][:-MAX_EVENTS]


def load_state(path: Path) -> dict[str, Any]:
    if not path.exists():
        return default_state()
    try:
        return validate_state(json.loads(path.read_text(encoding="utf-8")))
    except json.JSONDecodeError as error:
        raise CoreError("invalid_state", f"state is not valid JSON: {error.msg}") from error


def atomic_write(path: Path, state: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(state, indent=2, sort_keys=True) + "\n"
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            handle.write(serialized)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    finally:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass


def task_summary(task: dict[str, Any]) -> dict[str, Any]:
    return {
        "id": task["id"],
        "title": task.get("title", task["id"]),
        "status": task["status"],
        "phase": task["phase"],
        "className": task.get("className"),
        "address": task.get("address"),
        "updatedAt": task["updatedAt"],
    }


def ensure_task(state: dict[str, Any], task_id: str) -> dict[str, Any]:
    task = state["tasks"].get(task_id)
    if task is None:
        raise CoreError("unknown_task", f"unknown task: {task_id}")
    return task


def command_init(state: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any]:
    binary = payload.get("binary", {})
    binary = require_object(binary, "binary")
    if state["binary"] and binary and state["binary"] != binary:
        raise CoreError("binary_mismatch", "state already belongs to a different binary fingerprint")
    if binary:
        state["binary"] = copy.deepcopy(binary)
    append_event(state, "initialized", {"binary": state["binary"]})
    return {"schemaVersion": SCHEMA_VERSION, "binary": state["binary"]}


def command_upsert_task(state: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any]:
    source = require_object(payload.get("task"), "task")
    task_id = require_string(source.get("id"), "task.id")
    existing = state["tasks"].get(task_id)
    if existing is None:
        phase = source.get("phase", "investigate")
        if phase not in TASK_PHASES:
            raise CoreError("invalid_input", "task.phase is invalid")
        status = source.get("status", "open")
        if status not in TASK_STATUSES:
            raise CoreError("invalid_input", "task.status is invalid")
        timestamp = now()
        existing = {
            "id": task_id,
            "title": source.get("title", task_id),
            "phase": phase,
            "status": status,
            "className": source.get("className"),
            "address": source.get("address"),
            "ownerFiles": [],
            "allowedWrites": [],
            "sharedWrites": [],
            "metadata": {},
            "history": [{"at": timestamp, "status": status, "reason": "created"}],
            "writeAudits": [],
            "deferred": None,
            "createdAt": timestamp,
            "updatedAt": timestamp,
        }
        state["tasks"][task_id] = existing
        created = True
    else:
        created = False

    for field in ("title", "className", "address"):
        if field in source:
            value = source[field]
            if value is not None and not isinstance(value, str):
                raise CoreError("invalid_input", f"task.{field} must be a string or null")
            existing[field] = value
    if "phase" in source:
        if source["phase"] not in TASK_PHASES:
            raise CoreError("invalid_input", "task.phase is invalid")
        existing["phase"] = source["phase"]
    for field in ("ownerFiles", "allowedWrites", "sharedWrites"):
        if field in source:
            paths = [safe_relative_path(item, f"task.{field}") for item in require_string_list(source[field], f"task.{field}")]
            existing[field] = paths
    if "metadata" in source:
        metadata = require_object(source["metadata"], "task.metadata")
        existing["metadata"].update(copy.deepcopy(metadata))
    existing["updatedAt"] = now()
    append_event(state, "task_created" if created else "task_updated", {"taskId": task_id})
    return {"created": created, "task": task_summary(existing)}


def command_transition(state: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any]:
    task_id = require_string(payload.get("taskId"), "taskId")
    task = ensure_task(state, task_id)
    status = require_string(payload.get("status"), "status")
    if status not in TASK_STATUSES:
        raise CoreError("invalid_input", "status is invalid")
    reason = payload.get("reason", "")
    if not isinstance(reason, str):
        raise CoreError("invalid_input", "reason must be a string")
    previous = task["status"]
    task["status"] = status
    task["updatedAt"] = now()
    task["history"].append({"at": task["updatedAt"], "status": status, "previous": previous, "reason": reason})
    if status not in {"blocked", "deferred"}:
        task["deferred"] = None
    append_event(state, "task_transitioned", {"taskId": task_id, "from": previous, "to": status})
    return {"task": task_summary(task), "previousStatus": previous}


def command_defer(state: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any]:
    task_id = require_string(payload.get("taskId"), "taskId")
    task = ensure_task(state, task_id)
    status = payload.get("status", "deferred")
    if status not in {"blocked", "deferred"}:
        raise CoreError("invalid_input", "defer status must be blocked or deferred")
    reason = require_string(payload.get("reason"), "reason")
    next_action = require_string(payload.get("nextAction"), "nextAction")
    blocked_by = require_string_list(payload.get("blockedBy", []), "blockedBy")
    evidence_refs = require_string_list(payload.get("evidenceRefs", []), "evidenceRefs")
    retry_when = payload.get("retryWhen")
    if retry_when is not None and not isinstance(retry_when, str):
        raise CoreError("invalid_input", "retryWhen must be a string or null")
    for dependency in blocked_by:
        if dependency not in state["tasks"]:
            raise CoreError("unknown_task", f"blockedBy refers to an unknown task: {dependency}")
    previous = task["status"]
    timestamp = now()
    task["status"] = status
    task["updatedAt"] = timestamp
    task["deferred"] = {
        "reason": reason,
        "nextAction": next_action,
        "blockedBy": blocked_by,
        "evidenceRefs": evidence_refs,
        "retryWhen": retry_when,
        "recordedAt": timestamp,
    }
    task["history"].append({"at": timestamp, "status": status, "previous": previous, "reason": reason})
    append_event(state, "task_deferred", {"taskId": task_id, "status": status, "blockedBy": blocked_by})
    return {"task": task_summary(task), "deferred": task["deferred"]}


def command_add_edge(state: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any]:
    source = require_string(payload.get("from"), "from")
    target = require_string(payload.get("to"), "to")
    ensure_task(state, source)
    ensure_task(state, target)
    if source == target:
        raise CoreError("invalid_input", "an edge cannot refer to the same task")
    kind = payload.get("kind", "requires")
    confidence = payload.get("confidence", "tentative")
    if kind not in EDGE_KINDS:
        raise CoreError("invalid_input", "edge kind is invalid")
    if confidence not in CONFIDENCE:
        raise CoreError("invalid_input", "edge confidence is invalid")
    provenance = require_object(payload.get("provenance", {}), "provenance")
    edge = {"from": source, "to": target, "kind": kind, "confidence": confidence, "provenance": copy.deepcopy(provenance), "createdAt": now()}
    if not any(all(previous.get(key) == edge[key] for key in ("from", "to", "kind")) for previous in state["edges"]):
        state["edges"].append(edge)
        append_event(state, "edge_added", {"from": source, "to": target, "kind": kind})
    return {"edge": edge}


def evidence_digest(artifact: Any) -> str:
    return hashlib.sha256(json.dumps(artifact, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()


def command_record_evidence(state: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any]:
    key = require_string(payload.get("key"), "key")
    artifact = payload.get("artifact")
    if artifact is None:
        raise CoreError("invalid_input", "artifact is required")
    source = require_object(payload.get("source", {}), "source")
    observations = payload.get("observations", [])
    hypotheses = payload.get("hypotheses", [])
    if not isinstance(observations, list) or not isinstance(hypotheses, list):
        raise CoreError("invalid_input", "observations and hypotheses must be arrays")
    entry = state["evidence"].setdefault(key, {"key": key, "revisions": []})
    digest = evidence_digest(artifact)
    if entry["revisions"] and entry["revisions"][-1]["digest"] == digest:
        return {"key": key, "revision": entry["revisions"][-1]["revision"], "deduplicated": True}
    revision = len(entry["revisions"]) + 1
    entry["revisions"].append({
        "revision": revision,
        "capturedAt": now(),
        "digest": digest,
        "source": copy.deepcopy(source),
        "artifact": copy.deepcopy(artifact),
        "observations": copy.deepcopy(observations),
        "hypotheses": copy.deepcopy(hypotheses),
    })
    append_event(state, "evidence_recorded", {"key": key, "revision": revision})
    return {"key": key, "revision": revision, "deduplicated": False}


def command_get_evidence(state: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any]:
    key = require_string(payload.get("key"), "key")
    entry = state["evidence"].get(key)
    if entry is None:
        raise CoreError("unknown_evidence", f"unknown evidence key: {key}")
    return {"evidence": copy.deepcopy(entry)}


def command_ready(state: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any]:
    include_deferred = bool(payload.get("includeDeferred", False))
    ready: list[dict[str, Any]] = []
    waiting: list[dict[str, Any]] = []
    for task in state["tasks"].values():
        if task["status"] not in ({"open"} | ({"deferred"} if include_deferred else set())):
            continue
        prerequisites = [edge for edge in state["edges"] if edge["from"] == task["id"] and edge["kind"] == "requires"]
        unmet = [edge["to"] for edge in prerequisites if state["tasks"][edge["to"]]["status"] != "integrated"]
        if unmet:
            waiting.append({**task_summary(task), "unmetPrerequisites": unmet})
        else:
            ready.append(task_summary(task))
    return {"ready": ready, "waiting": waiting}


def command_validate_write_set(state: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any]:
    task_id = require_string(payload.get("taskId"), "taskId")
    task = ensure_task(state, task_id)
    before = require_object(payload.get("before"), "before")
    after = require_object(payload.get("after"), "after")
    for label, fingerprint in (("before", before), ("after", after)):
        for path, digest in fingerprint.items():
            safe_relative_path(path, f"{label} path")
            if not isinstance(digest, str):
                raise CoreError("invalid_input", f"{label} fingerprint values must be strings")
    changed = sorted(path for path in set(before) | set(after) if before.get(path) != after.get(path))
    allowed = set(task["allowedWrites"])
    shared = set(task["sharedWrites"])
    unexpected = [path for path in changed if path not in allowed and path not in shared]
    shared_changed = [path for path in changed if path in shared]
    audit = {"at": now(), "changed": changed, "allowed": [path for path in changed if path in allowed], "shared": shared_changed, "unexpected": unexpected}
    task["writeAudits"].append(audit)
    task["updatedAt"] = audit["at"]
    append_event(state, "write_set_validated", {"taskId": task_id, "unexpected": unexpected, "shared": shared_changed})
    return audit


def command_snapshot(state: dict[str, Any], _payload: dict[str, Any]) -> dict[str, Any]:
    return {
        "schemaVersion": state["schemaVersion"],
        "binary": copy.deepcopy(state["binary"]),
        "tasks": [task_summary(task) for task in state["tasks"].values()],
        "edges": copy.deepcopy(state["edges"]),
        "evidenceKeys": sorted(state["evidence"]),
        "eventCount": len(state["events"]),
    }


COMMANDS = {
    "init": command_init,
    "upsert-task": command_upsert_task,
    "transition": command_transition,
    "defer": command_defer,
    "add-edge": command_add_edge,
    "record-evidence": command_record_evidence,
    "get-evidence": command_get_evidence,
    "ready": command_ready,
    "validate-write-set": command_validate_write_set,
    "snapshot": command_snapshot,
}


def parse_payload(input_path: str | None) -> dict[str, Any]:
    if input_path is None:
        return {}
    try:
        return require_object(json.loads(Path(input_path).read_text(encoding="utf-8")), "input")
    except FileNotFoundError as error:
        raise CoreError("missing_input", f"input file does not exist: {input_path}") from error
    except json.JSONDecodeError as error:
        raise CoreError("invalid_input", f"input is not valid JSON: {error.msg}") from error


def run(command: str, state_path: Path, payload: dict[str, Any]) -> dict[str, Any]:
    handler = COMMANDS[command]
    state_path.parent.mkdir(parents=True, exist_ok=True)
    lock_path = state_path.with_name(state_path.name + ".lock")
    with lock_path.open("a+", encoding="utf-8") as lock:
        fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
        try:
            state = load_state(state_path)
            result = handler(state, payload)
            state["updatedAt"] = now()
            atomic_write(state_path, state)
            return result
        finally:
            fcntl.flock(lock.fileno(), fcntl.LOCK_UN)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=sorted(COMMANDS))
    parser.add_argument("--state", required=True, help="workflow state JSON path")
    parser.add_argument("--input", help="path to one JSON input object")
    args = parser.parse_args(argv)
    try:
        result = run(args.command, Path(args.state), parse_payload(args.input))
        print(json.dumps({"ok": True, "command": args.command, "result": result}, sort_keys=True))
        return 0
    except CoreError as error:
        print(json.dumps({"ok": False, "error": {"code": error.code, "message": str(error)}}, sort_keys=True))
        return 2
    except Exception as error:  # Never leak a traceback into the protocol stream.
        print(json.dumps({"ok": False, "error": {"code": "internal_error", "message": str(error)}}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
