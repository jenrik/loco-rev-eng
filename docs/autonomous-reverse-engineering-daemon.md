# Autonomous Reverse-Engineering Daemon — Design Intent

**Status:** Active design. This supersedes `evidence-guided-decompilation-workflow.md` as the runtime architecture. The earlier Python CLI is retained only as a data-model prototype until migration is complete.

## Goal

Run targeted, autonomous reverse-engineering work while preserving the project’s assembly-first rules, giving an operator a live browser view of every agent, and retaining enough history to understand, replay, or correct prior work.

The system must not assume that the complete dependency graph is known in advance. It learns incrementally from Ghidra evidence and records both observations and revisable hypotheses.

## Process ownership

```text
Browser dashboard
  └─ REST + WebSocket
       └─ Python daemon (single authority)
            ├─ SQLite event/task/evidence store
            ├─ scheduler and lifecycle controller
            ├─ one serialized Ghidra adapter
            ├─ Pi RPC process manager
            └─ agent event normalizer / log retention
                 └─ pi --mode rpc, one process per agent
                      └─ reverse-engineering Pi extension
                           ├─ task/evidence/transition tools
                           ├─ write-scope guard
                           └─ daemon-routed Ghidra tool
```

### Python daemon

The daemon owns all durable state and all process control. An exclusive lock on
the SQLite state path prevents multiple daemon processes from controlling the
same work queue. It starts and stops Pi processes, sends RPC `prompt`, `steer`,
`follow_up`, and `abort` commands, and normalizes the JSONL event stream from
each process. A terminal task transition
requests abort, then reaps the idle RPC child after a brief response-flush grace
period. A watchdog fails an attempt after bounded tool inactivity or a definitely
dead child PID; startup performs the same dead-PID recovery for stale tasks.
Its subprocess JSONL reader accepts 2MiB records because Pi read results can
exceed asyncio's 64KiB default after JSON escaping. Its direct
`re-mcp-ghidra proxy` client uses the proxy's unprefixed operation names
(`open_database`, `decompile_function`, etc.), rather than Pi's decorated
`mcp.ghidra.*` names. It is the only writer to the database, avoiding races
between agents.

A local SQLite database in WAL mode is appropriate for this single-host,
read-heavy dashboard. WAL permits concurrent readers with one writer but does
not permit multiple writers; therefore agent processes never write the database
directly. The daemon serializes short write transactions and runs bounded
checkpoints. The database must remain on a local filesystem, not NFS/SMB.

### Pi agents

Each agent is a separate `pi --mode rpc` process with:

- an isolated session directory and stable daemon-generated agent ID;
- a role prompt (`investigator`, `transcriber`, `validator`, `integrator`, or
  `reviewer`);
- the project `AGENTS.md` context;
- a custom extension loaded explicitly with `--extension`.

RPC is the correct integration boundary because the core is Python. It exposes
accepted prompts and the full lifecycle/event stream without embedding Node in
the daemon. The daemon retains the Pi session JSONL for full recall and stores
a normalized event stream for efficient live display.

### Custom Pi extension

The extension is an adapter, not the scheduler. It exposes narrowly scoped
agent tools:

- `re_get_task` and `re_get_evidence`;
- `re_record_evidence` and `re_record_hypothesis`;
- `re_transition_task` and `re_defer_task`;
- `re_ghidra_query` routed through the daemon’s Ghidra adapter;
- `re_request_write_scope` for discovered cross-file work.

A `tool_call` hook blocks writes outside the daemon-approved task write scope.
Agents can discover that a shared file is necessary, but must request an
escalation; they cannot silently broaden their task.

## Ghidra access and evidence

Agents retain interactive Ghidra access through `re_ghidra_query`. The daemon
owns one Ghidra adapter/worker per binary so that calls are serialized safely,
raw results are cached, and all evidence has provenance.

The adapter returns bounded raw results with the binary identity, database ID,
operation, parameters, capture time, and content digest. The agent may ask new
questions at any time. Cache entries are append-only revisions; observations
are `observed` only when directly supported by bytes/disassembly, while names,
class ownership, and inferred semantics remain `tentative` hypotheses until
validated.

The daemon now owns an MCP stdio child configured with an explicit command (for
this environment, `re-mcp-ghidra proxy`) and connects it to a fresh raw-binary
database ID. It waits for analysis before accepting calls, serializes all calls,
and exposes only the read-only allowlist. The integration is real: the adapter
has opened `loco.exe` through the configured service and completed an allowlisted
function-list query. Every response is persisted as a content-addressed evidence
revision. Ghidra mutation tools remain unavailable.

## Task graph and deferred work

The graph is partial and evidence-led, not a mandatory upfront plan.

- A node represents a concrete investigation or `function × pass` task.
- A `requires` edge points from a waiting node to a known prerequisite.
- `evidence` and `invalidates` edges document reasoning but do not block work.
- `blocked` means a concrete prerequisite exists.
- `deferred` means the next investigation is named but the prerequisite is not
  yet known.

The scheduler starts only ready, in-scope work. Submitting a job automatically
creates and dispatches one read-only `Initial evidence triage` investigator task;
existing empty drafts can be bootstrapped once from the operator UI. It may create
an investigation node when new uncertainty is encountered. It never treats a
missing edge as proof of independence.

## Dashboard and operator controls

The local dashboard is served by the daemon. It provides:

1. **Overview:** goal, queue, active agents, task counts, Ghidra adapter health.
2. **Live agent view:** current role/task/model/turn/tool, streaming text and
   tool events, plus steer, follow-up, pause, abort, and retry controls.
3. **History:** cursor-paginated replay of completed agent events and links to
   the original Pi session JSONL.
4. **Evidence/task view:** raw evidence revisions, hypotheses, graph edges,
   blocks, deferrals, and write-scope escalations.

The dashboard consumes a WebSocket event stream and can reconnect using the
last durable event sequence. REST endpoints serve snapshots and historical
pages. The server binds to loopback by default; any non-loopback mode requires
an explicit authentication configuration.

## Log retention and disk safety

The daemon records semantic events, not repeated snapshots:

- persist assistant text/thinking **deltas**, never the accumulated
  `message_update.message` object;
- persist tool start/end and a bounded final result;
- treat tool-progress snapshots as replaceable live state, not append-only
  history;
- cap each persisted payload, record truncation metadata, and retain full Pi
  session JSONL separately with per-agent retention limits;
- expose byte/event counters and retention status in the dashboard.

This is necessary for tailing and recall without recreating the previous
message-update log amplification failure.

## Data model

The daemon database has durable IDs and append-only event sequence numbers.
Core tables are:

- `jobs` — operator objectives and lifecycle;
- `tasks` and `task_edges` — partial evidence graph;
- `agents` — role, process/session metadata, status, task ownership;
- `events` — normalized agent and daemon events;
- `evidence_revisions` — raw content-addressed artifacts and their request provenance;
- `hypotheses` — append-only, evidence-linked interpretation revisions;
- `write_scope_requests` — pending/approved/rejected cross-file changes, with the approval atomically extending the assigned agent and task scopes.

Raw large artifacts are content-addressed files under the daemon state root;
the database stores digests and metadata. Database writes are performed only by
the daemon.

## Security boundary

- Bind the web server to `127.0.0.1` by default.
- Use a daemon-generated capability token for extension-to-daemon calls; never
  put it in a task, transcript, UI response, or event payload.
- Treat role prompts and project extensions as trusted project code.
- The extension enforces path scope before built-in write/edit/bash calls.
- Ghidra mutation operations are disabled by default. The initial adapter is
  read-only (`decompile`, `disassemble`, xrefs, structures, names, strings).

## Delivery status

Implemented foundations:

1. **Daemon vertical slice:** SQLite store, normalized event recorder, FastAPI
   status/history API, WebSocket broadcast, static dashboard, and tests.
2. **Pi process manager:** launch a mock/Pi RPC agent, persist live events,
   and implement stop/steer/follow-up transport.
3. **Pi extension:** task/evidence/Ghidra tools and enforced initial write scope.
4. **Ghidra adapter:** daemon-owned, read-only project MCP calls with raw-binary
   open/analysis, serialized calls, and content-addressed evidence revisions.
5. **Autonomous scheduler:** dependency-gated launch of role agents and durable
   completion/block/defer transitions.
6. **Ghidra lifecycle:** non-secret project configuration, job-scoped evidence
   cache reuse, binary identity health, database shutdown cleanup, and one
   bounded worker restart.
7. **Operator workflow:** browser job/task/edge/schedule/agent controls,
   append-only hypotheses, and dynamically enforced write-scope approvals.

Remaining delivery work:

1. **Scheduler recovery:** retry/timeout/cancellation semantics and automatic
   re-queueing of interrupted task attempts.
2. **Safe history and retention:** bounded Pi-session transcript access without
   serving secrets, plus artifact retention/eviction and dashboard cache refresh.
3. **Selective worktrees:** use isolated branches only for tasks declared
   independent; keep shared integration serial.

Every foundation has focused tests; each remaining step must add integration
coverage before it is enabled.
