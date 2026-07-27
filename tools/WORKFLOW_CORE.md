# Workflow core

`workflow_core.py` is the durable state layer for the Fabric decompilation
scheduler. It is intentionally a small, lock-protected JSON CLI rather than a
daemon: the current scheduler has one orchestrator, and atomic state commands
are easier to inspect, test, resume, and recover after a stopped Pi session.

The core stores an *evolving evidence graph*. It does not replace live Ghidra:
PRIMARY agents still query Ghidra directly. The cache records raw evidence and
provenance already observed so later workers and reviewers need not rediscover
it. Interpretations belong in `hypotheses` and remain revisable.

## Commands

All commands take `--state STATE.json --input REQUEST.json` and print exactly
one JSON response:

- `init` — bind the ledger to a binary fingerprint.
- `upsert-task`, `transition`, `defer` — maintain task/pass state.
- `add-edge`, `ready` — maintain incremental prerequisite edges.
- `record-evidence`, `get-evidence` — revisioned raw Ghidra evidence.
- `validate-write-set` — compare pre/post content fingerprints against a
  task's declared allowed and shared write paths.
- `snapshot` — compact state for a scheduler or session report.

A `requires` edge points **from the waiting task to its prerequisite**. A
ready task has every such prerequisite in `integrated` state. Deferred and
blocked tasks stay in the same ledger with their evidence references and retry
conditions; they are not discarded.
