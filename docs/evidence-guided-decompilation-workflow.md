# Evidence-Guided Decompilation Workflow — Design Intent

**Status:** Superseded as the runtime architecture by `autonomous-reverse-engineering-daemon.md`. Its evidence/task model remains useful migration input.

## 1. Purpose

This document defines the long-lived design for the Fabric-assisted Lego Loco reverse-engineering workflow. It exists to keep later changes aligned with the project's assembly-first standard rather than to prescribe a one-off scheduler implementation.

The workflow must improve continuity, share verified observations, and make unresolved work actionable **without pretending that reverse engineering is a fully knowable planning problem**. Every semantic conclusion remains subject to new disassembly evidence.

## 2. Non-negotiable principles

1. **Ghidra and the raw binary remain the source of truth.** Cached material, agent summaries, task status, and graph edges are working records only.
2. **PRIMARY agents retain live Ghidra access.** A cache never limits an agent from following an unexpected xref, inspecting bytes, or disproving a prior conclusion.
3. **Observed facts and hypotheses are different data.** An instruction-level field access is an observation. A proposed C++ class name, ownership model, or vtable interpretation is a hypothesis with evidence and confidence.
4. **The dependency graph is discovered incrementally.** It is not an upfront, complete DAG and must never block useful investigation merely because an edge is unknown.
5. **Work is reversible and auditable.** State transitions, evidence revisions, deferred work, and source edits need enough provenance to explain why a later decision was made.
6. **C++ integration remains the completion standard.** A workflow node is not complete merely because a model says so; the existing transcription, validation, integration, address-annotation, and build requirements apply.

## 3. Problem being solved

The existing supervisor has useful local controls—persistent PRIMARY actors, independent review, block validation, and settled scheduling—but its knowledge lives only inside one run. After a restart it must rediscover previous blocks, partial evidence, and retry reasons from prose. It also has no machine-readable way to distinguish a real prerequisite from an unresolved investigation.

The design adds a durable *evidence graph* alongside the existing C++ source and `PROGRESS.md`. It is a scheduling and memory aid, not a replacement for project documentation or Ghidra.

## 4. Architecture

```text
PRIMARY / REVIEWER
  ├─ query live Ghidra as needed
  ├─ read source and edit C++
  └─ submit observations, hypotheses, task outcomes
             │
             ▼
TypeScript Fabric orchestration
  ├─ owns agent lifecycle and Ghidra session lifecycle
  ├─ takes source-tree fingerprints before/after an attempt
  ├─ requests cache/ledger operations
  └─ decides whether work may be launched
             │ structured JSON CLI requests
             ▼
Python workflow core
  ├─ validated, atomic evidence/task/edge transitions
  ├─ file lock for concurrent CLI invocations
  └─ durable JSON state
```

The TypeScript layer remains the Pi-facing implementation: it calls agents, uses `pi.*`, and owns the active session. The Python core is deliberately not allowed to call Pi, Ghidra, or agents. It is a deterministic state engine with a small JSON protocol.

### Why a CLI, not a daemon

The first implementation uses a lock-protected CLI invocation per state transition.

- The scheduler has a single TypeScript orchestrator today.
- Atomic files make state inspectable and easy to repair after a stopped Pi session.
- A daemon would introduce another process, protocol, ownership, restart, shutdown, and data-corruption surface before it provides a demonstrated benefit.

A daemon becomes appropriate only if profiling shows CLI startup or file-lock contention materially limits many concurrent *independent* orchestrators. The JSON command schema should remain stable if that transport is later replaced.

## 5. Evidence cache

### 5.1 What is cached

Evidence is recorded lazily per stable key, normally including the binary identity and an address, for example:

```text
loco.exe:<sha256>:function:0x4343B0
```

A revision may contain:

- raw decompiler output;
- raw disassembly;
- xrefs, type information, vtable/structure excerpts, and source excerpts;
- capture source and timestamp;
- instruction- or address-linked observations;
- hypotheses, confidence, and reasons.

The artifact is retained with a digest. An identical re-read deduplicates; a changed capture becomes a new revision. Earlier revisions are retained so that a corrected interpretation has an explanation rather than silently rewriting history.

### 5.2 What is not cached

The cache is not a replacement for:

- opening the real binary and checking a surprising result;
- `PROGRESS.md` as the durable human session summary;
- C++ source, Ghidra annotations, or a code-review record;
- a claim that an unobserved path, caller, or field does not exist.

PRIMARY may query Ghidra at any time. When a query produced material evidence, its result should be appended to the cache for the next worker/reviewer.

### 5.3 Evidence confidence

Use these labels consistently:

| Label | Meaning |
|---|---|
| `observed` | Directly supported by bytes, disassembly, or an exact Ghidra result. |
| `tentative` | A useful interpretation inferred from observed evidence; must remain revisable. |

A hypothesis must point to either a cache revision, an address, or a source location. A reviewer can invalidate a hypothesis; it must not delete the prior record.

## 6. Incremental evidence graph

The graph is an evolving partial graph. Nodes represent a specific unit of work, normally an address plus a pass:

```text
fn:0x4343B0:transcribe
fn:0x4343B0:validate
fn:0x4343B0:integrate
investigate:tilemap-obstacle-predicate
```

Class-level nodes may be used as an integration milestone, but must reference the functions that justify their status. This avoids marking a whole class integrated based on only a convenient subset.

Edges have kind, confidence, provenance, and direction:

```text
waiting task --requires--> prerequisite task
claim/task   --evidence--> evidence or investigation task
new evidence --invalidates--> prior hypothesis/task
```

Only `requires` edges constrain the ready set. Tentative edges inform the scheduler but do not become hard gates until supported by evidence. The graph must accept new nodes and edges at any time; absence of an edge means unknown, not independent.

## 7. Task and deferred-work ledger

Deferred work belongs in the graph. It must not become an unsearchable TODO in an agent transcript.

A task records at least:

```json
{
  "id": "fn:0x433860:validate",
  "phase": "validate",
  "status": "deferred",
  "ownerFiles": ["src/decompiled_cpp/game/Building.cpp"],
  "allowedWrites": ["src/decompiled_cpp/game/Building.cpp"],
  "sharedWrites": ["PROGRESS.md"],
  "deferred": {
    "reason": "TileMap obstacle predicate semantics are not established",
    "nextAction": "Inspect predicate callers and tile flag reads",
    "blockedBy": ["investigate:tilemap-obstacle-predicate"],
    "evidenceRefs": ["loco.exe:<sha256>:function:0x433860"],
    "retryWhen": "The predicate interpretation has observed support"
  }
}
```

### Status meanings

| Status | Meaning |
|---|---|
| `open` | Ready to assess; may have no known hard prerequisites. |
| `active` | Owned by a current attempt. |
| `transcribed`, `validated`, `integrated` | Passed the corresponding project pipeline stage. |
| `blocked` | A concrete prerequisite is known and prevents this work now. |
| `deferred` | More investigation is needed; the next investigation is named. |
| `invalidated` | A prior task interpretation was disproved; retain it for provenance. |

`blocked` and `deferred` are not failure states. They are scheduler inputs. A scheduler may launch a prerequisite, an investigation node, or a later retry; it may not silently treat either as completion.

## 8. Source-edit isolation and write sets

### 8.1 Write declarations

Each task declares:

- `ownerFiles`: files expected to embody the result;
- `allowedWrites`: normal task-local edits;
- `sharedWrites`: known cross-cutting edits requiring serial integration or explicit coordination.

Before and after an attempt, the orchestrator fingerprints relevant source files. The Python core compares those fingerprints and records changed, allowed, shared, and unexpected paths.

Unexpected writes are a failure until reviewed. Shared writes are not necessarily wrong—the reverse-engineering process often discovers a base-class field or common type—but they are coordination events and should create or update dependency records.

### 8.2 Isolation levels

1. **Serial shared-tree work** — default for work touching common headers, `PROGRESS.md`, base classes, or unclear ownership.
2. **Write-set-enforced shared-tree work** — suitable for clearly local implementation changes; post-attempt validation detects scope escape.
3. **Git worktree isolation** — use for truly independent changes. Each PRIMARY gets a branch/worktree; an integration task builds, reviews, and applies the resulting patch.

Worktrees prevent collisions but do not solve semantic integration. They should not be used to hide required shared-header coordination.

## 9. Scheduler policy

The scheduler remains incremental and agent-assisted. It should:

1. load the compact ledger snapshot at every settled boundary;
2. prioritize ready tasks with observed prerequisite edges;
3. expose deferred/blocked nodes and retry conditions to the supervisor;
4. request live-Ghidra verification for new target addresses before launch;
5. declare session completion only when no in-scope open, active, blocked, or deferred node remains, or each remaining node is explicitly deferred with a recorded human-approved reason;
6. preserve the existing independent code/assembly review gates.

The scheduler must not require a fully populated graph before starting work. Discovery is allowed to create investigation nodes and tentative edges. The host validates the shape of every node and path; a model is not trusted to write arbitrary task metadata or source paths.

## 10. Migration plan

1. **Foundation:** add the Python JSON CLI, tests, and TypeScript bridge.
2. **Observation:** have the supervisor persist class-attempt tasks and outcomes without changing scheduling policy.
3. **Evidence:** add explicit agent-facing commands/prompts for recording raw Ghidra evidence and hypotheses.
4. **Deferrals:** represent validated blocks as graph nodes/edges with concrete retry conditions.
5. **Enforcement:** apply write-set checks, then selectively use worktrees for independent work.
6. **Scheduling:** allow the supervisor to consume ready/deferred graph state; retain human-readable `PROGRESS.md` updates.

Each stage must be usable on its own and backed by tests. No migration stage may weaken the assembly-validation rules in `AGENTS.md`.

## 11. Acceptance criteria

The architecture is successful when a later session can answer, from durable state and source evidence:

- what was attempted, what pass it reached, and why it stopped;
- what raw evidence supports the current interpretation;
- which hypotheses were changed or invalidated and why;
- which concrete prerequisite or investigation is next;
- which files a task changed, including unexpected shared changes;
- whether a task is truly ready to retry.

It is not successful merely because it schedules more agents or produces more status records.
