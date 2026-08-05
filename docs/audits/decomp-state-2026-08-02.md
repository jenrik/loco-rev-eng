# Lego Loco Decompilation State Audit — 2026-08-02

## Executive Summary

This audit is **partial and mechanical only**. It deterministically inventoried all reconstructed files under `src/decompiled_cpp/`, captured build and test results, and persisted a 90-shard audit plan. It did **not** establish assembly correctness, policy compliance, subsystem maturity, or any new `VALIDATED`/`INTEGRATED` status.

The mandatory detached reviewers could not start. Six DeepSeek raw workers, two explicit correction attempts, and the Luna baseline worker all failed before model turn 1 because Pi loaded two copies of the `fabric_exec` extension. Consequently, no worker attached to Ghidra, no function was deeply checked, no Terra review was eligible to run, and the final Terra/Luna gates could not run.

The strongest independently observed current-state risks are:

- `nix develop -c make test-integration` reported **8 failures and 4 passes**.
- The direct `make test` target failed outside the Nix development shell because Python could not import `pytest`.
- `make check` passed for all **126 enabled objects**, but reported **33 native C files skipped as broken**.
- Declared maturity is sparse: 122/143 implementation files have no recognized file-status marker. This is an unreviewed possible compliance pattern, not an accepted finding.

Exact deep-audit coverage is **0/2,648 heuristic function entries (0%)**, including **0/874 entries associated by the inventory heuristic with an original address**. This report must not be represented as a repository-wide correctness or compliance audit.

## State of the Decompilation Project

### Deterministic inventory

Scope: `src/decompiled_cpp/`.

| Item | Independently observed count |
|---|---:|
| Implementation files (`.cpp`/`.c`) | 143 |
| Canonical/header files (`.h`/`.hpp`) | 82 |
| Total files inventoried | 225 |
| Heuristic function-definition entries | 2,648 |
| Entries associated with an `Address:` annotation | 874 |
| Heuristic entries without an associated address | 1,774 |
| Explicit `Address:` tags | 1,111 |
| Deterministic shards | 90 |

The function census is lexical, not an AST. It visibly includes false positives and duplicate associations in complex bodies, so the 2,648/874/1,774 figures are manifest denominators, not claims about exact C++ semantic function counts. Every explicit `Address:` tag is separately preserved in `build/audit/decomp-audit-20260802T200327Z/inventory.json`.

### Declared status census

These counts describe source markers only; the audit did not validate that any marker satisfies the AGENTS.md definition.

| Declared marker | Implementation files | Headers |
|---|---:|---:|
| `INTEGRATED` | 15 | 11 |
| `TRANSCRIBED` | 6 | 9 |
| `VALIDATED` | 0 | 0 |
| Absent | 122 | 62 |

No new maturity designation is made by this report. In particular, the 15 implementation files declaring `INTEGRATED` remain **declared**, not independently proven.

### Subsystem inventory

| Subsystem | Implementation files | Heuristic functions | Address-associated entries |
|---|---:|---:|---:|
| audio | 2 | 36 | 28 |
| core | 11 | 211 | 91 |
| game | 19 | 298 | 147 |
| graphics | 3 | 75 | 44 |
| input | 7 | 153 | 32 |
| native | 51 | 249 | 107 |
| network | 8 | 186 | 22 |
| resources | 4 | 108 | 46 |
| shared | 10 | 712 | 35 |
| town | 2 | 107 | 56 |
| ui | 22 | 386 | 189 |
| world | 4 | 127 | 77 |

Subsystem maturity is not safe to characterize from these counts. The large shared/helper and native provenance gaps particularly require a corrected semantic inventory and assembly-aware review.

### Build, check, and test state

| Command | Result | Independently observed detail |
|---|---|---|
| `make` | Pass | Exit 0; build was already substantially up to date. The log repeatedly reports missing `pkg-config`, while Makefile fallback paths still allowed success. |
| `make check` | Pass | 126/126 enabled objects built: 92 C++, 21 native C, 13 shims; 33 native C listed as broken/skipped. |
| `make test` | Fail | Exit 2 before completing the suite: `python3: No module named pytest`. The current Makefile wires `test` to integration first. |
| `nix develop -c make test-unit` | Pass | Exit 0; deterministic component/host-boundary layer passed. Logs still contain explicit host skips/deferred behavior. |
| `nix develop -c make test-integration` | Fail | 8 failed, 4 passed in the isolated GUI suite. |

Logs are under `build/audit/decomp-audit-20260802T200327Z/make*.log`.

### PROGRESS.md: claimed versus observed

- `PROGRESS.md:55` records a historical 69-file `make check` milestone. The current check instead reports 126 enabled objects; the historical claim is not used as current evidence.
- `PROGRESS.md:274-275` records later 133/133 and 135/135 checks. The current Makefile/check denominator is 126 enabled objects, so these are historical snapshots rather than reproducible current counts.
- `PROGRESS.md:404` already reports 4 passing and 8 failing GUI flows. The audit independently reproduced the same 4/8 integration outcome.
- Extensive assembly-validation and integration claims throughout PROGRESS.md were not independently rechecked because no assembly-aware worker started. They remain claimed, not audited.

### Unresolved stubs and provenance gaps

The mechanical census found 235 lines matching TODO/deferred/stub vocabulary and 1,774 heuristic entries without a nearby `Address:` annotation. Neither number has been manually false-positive-eliminated. The Makefile independently reports 33 broken/skipped native C files. These are prioritization leads, not confirmed violations or exact internal-stub counts.

## Individual Confirmed Issues

**None.** No candidate passed the mandatory DeepSeek → independent Terra evidence chain, and no pure policy claim passed Luna review. Reporting mechanical grep hits as confirmed issues would violate the audit gates.

## Structural and/or Recurring Problems

No project structural problem met the required confirmation threshold. The following are **possible patterns only**:

### Possible pattern P-STATUS-ABSENT

- Mechanical instances: 122/143 implementation files and 62/82 headers lack a recognized `TRANSCRIBED`, `VALIDATED`, or `INTEGRATED` marker.
- Scope: all 12 inventoried subsystems.
- Risk: maturity and completion claims may be difficult to reproduce file-by-file.
- Limitation: Luna did not start, and the governing status convention was not applied to every source context. This is not a confirmed policy issue.

### Possible pattern P-PROVENANCE-GAP

- Mechanical instances: 1,774/2,648 heuristic function entries lacked an associated nearby `Address:` tag.
- Risk: original provenance may be absent for some real reconstructed functions.
- Limitation: the lexical parser produced known false positives and duplicates; only a semantic inventory plus source inspection can establish the real denominator.

### Possible pattern P-ANTI-PATTERN-CENSUS

The initial untriaged line-hit counts include 2,113 vtable-word hits, 301 raw-this candidates, 5,757 void-pointer candidates, 1,190 decompiler-label candidates, 243 constructor/destructor-name candidates, 3 deleting-destructor candidates, 1,337 calling-convention candidates, and 898 `param_*/field_*` candidates. These counts include comments, declarations, documentation-like source comments, legitimate ABI boundaries, and duplicate line categories. Because Luna could not inspect every hit, none is a confirmed recurring problem.

## Rejected and Unresolved Candidates

No model-authored candidate issue was produced, so there are no rejected correctness candidates.

| ID | Disposition | Reason |
|---|---|---|
| AUDIT-INFRA-001 | Unresolved blocker | Mandatory `recursive:true`/`extensions:true` Pi workers fail before turn 1 because `fabric_exec` is registered from both `~/.pi/agent/extensions/pi-fabric-0.28.6/index.ts` and the injected Nix-store extension. |
| MECH-P-STATUS | Needs review | Status-marker census was not Luna-reviewed. |
| MECH-P-PROVENANCE | Needs review | Lexical function parser has false positives/duplicates and no assembly review. |
| MECH-P-ANTIPATTERNS | Needs review | Raw grep hits were not inspected hit-by-hit to eliminate legitimate/comment-only cases. |

## Coverage and Limitations

| Coverage measure | Result |
|---|---:|
| Files inventoried | 225/225 in scoped `src/decompiled_cpp` inventory |
| Heuristic functions deeply checked | 0/2,648 (0%) |
| Address-associated entries deeply checked | 0/874 (0%) |
| Raw shards successfully completed | 0/90 (0%) |
| Initial raw shards launched | 6/90 |
| Raw correction attempts | 2 |
| Schema-valid raw candidate artifacts | 0 |
| Terra shard reviews | 0 |
| Terra negative-sample functions | 0 |
| Valid Luna reviews | 0 |
| Ghidra databases opened by workers | 0 |

Shard disposition: 2 persistent startup failures after correction, 4 initial startup failures left blocked by the circuit breaker, and 84 not launched after the systemic failure was established. No raw worker reached a model turn or tool call.

The audit did not cover `src/sdl3_shims` as reconstructed-function inventory; it was intended for Luna host-boundary compliance review, which failed to start. Tooling, daemon/frontend, test, and third-party code were not treated as reconstructed game functions.

Areas not safe to characterize include all original behavior, calling conventions, signedness, layouts, vtable relationships, ownership, function completeness, subsystem integration, host-boundary compliance, and false-negative rate.

## Method and Validation Ledger

### Agent ledger

| Agent ID | Model | Assignment | Outcome |
|---|---|---|---|
| `8441e7ed20a7442fa70e81449f6c12ff` | DeepSeek V4 Pro | raw-001-audio attempt 1 | Failed before turn 1 |
| `1e9e8b1be4094f80bfd27c6187ba6ed9` | DeepSeek V4 Pro | raw-002-audio attempt 1 | Failed before turn 1 |
| `8c7f444bd0334bc688e9694f24fba4ec` | DeepSeek V4 Pro | raw-003-core | Failed before turn 1 |
| `2d3ce52bd17c4645bb3f1748ce8624ab` | DeepSeek V4 Pro | raw-004-core | Failed before turn 1 |
| `d297ca4ebf6445eeb30b7aab7466b4a1` | DeepSeek V4 Pro | raw-005-core | Failed before turn 1 |
| `72a0dff1a5d04c8d962bd981514fb67a` | DeepSeek V4 Pro | raw-006-core | Failed before turn 1 |
| `daa328e4cc5542548cba6eb5029ec1ac` | DeepSeek V4 Pro | raw-001-audio correction | Failed before turn 1 on tmux |
| `95e15d3f5c3e4f85b22a0c1741208564` | DeepSeek V4 Pro | raw-002-audio correction | Failed before turn 1 on tmux |
| `6f6ec0ed26e940c0a08e9cc1beb3bbe1` | GPT-5.6 Luna | baseline compliance | Failed before turn 1 |

No Terra agent was eligible to spawn. There are no review relationships or reviewer dispositions.

### Reproducible commands

```bash
python3 build/audit/decomp-audit-20260802T200327Z/inventory.py
make
make check
make test
nix develop -c make test-unit
nix develop -c make test-integration
```

The exact inventory, shard scopes, schemas, failure records, command logs, manifest, summary, and mechanical gate results are preserved under `build/audit/decomp-audit-20260802T200327Z/`.

### Disposition totals

- Raw agent runs: 8 failed before turn 1; 0 successful.
- Raw shards: 0 complete; 6 attempted; 84 not launched.
- Candidate correctness issues: 0 produced, 0 confirmed, 0 rejected.
- Policy issues: 0 accepted.
- Terra dispositions: 0 confirmed, 0 rejected, 0 needs-more-evidence.
- Final mechanical gate: failed because mandatory raw/Terra/Luna validation was unavailable.
