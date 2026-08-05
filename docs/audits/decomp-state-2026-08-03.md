# Executive Summary

This is a **partial, blocked audit**, not a repository-wide correctness audit. The deterministic inventory observed 143 reconstructed implementation files, 82 canonical/header files, and 2,648 lexical function entries under `src/decompiled_cpp`. Of those lexical entries, 874 were associated with an `Address:` annotation and 1,774 were not. The inventory produced 90 assembly-oriented shards plus 24 explicit provenance-gap shards (114 total).

The root build and per-file check pass, and the separately invoked deterministic unit/host-boundary suite passes. The required aggregate `make test` target fails because the GUI integration layer reports **8 failed / 4 passed**. No assembly-aware worker reached turn 1: every mandatory recursive Pi child aborted while loading duplicate `pi-fabric` extensions that both register `fabric_exec`. Consequently, 0/114 shards, 0/2,648 lexical functions, and 0 negative samples received deep assembly review; no Terra review or final Luna gate was possible.

The strongest confirmed risks are therefore assurance and runtime-test risks, not proven reconstructed-code mismatches:

1. the mandatory DeepSeek/Luna/Terra evidence pipeline is unusable in the current runner configuration; and
2. the current required GUI integration gate is red (8/12 tests fail).

No assembly-correctness issue is reported as confirmed. Existing `INTEGRATED` labels and `PROGRESS.md` completion claims were inventoried as claims but could not be independently revalidated against `loco.exe` in this run.

# State of the Decompilation Project

## Deterministic inventory

Scope: `src/decompiled_cpp` for reconstructed implementations and canonical headers. The function count is a deterministic lexical heuristic, not a C++ AST; false positives are visible in the shard scopes and are not silently treated as real functions.

| Measure | Independently observed |
|---|---:|
| Implementation files | 143 |
| Headers | 82 |
| Lexical function entries | 2,648 |
| Entries associated with an address annotation | 874 |
| Entries without an associated address annotation | 1,774 |
| Total `Address:` tags | 1,111 |
| Assembly-oriented shards | 90 |
| Provenance-gap shards | 24 |
| Total manifest shards | 114 |

Declared file status:

| Kind | INTEGRATED | TRANSCRIBED | VALIDATED | Absent |
|---|---:|---:|---:|---:|
| Implementations (143) | 15 | 6 | 0 | 122 |
| Headers (82) | 11 | 9 | 0 | 62 |

These counts describe labels only. Under `AGENTS.md`, only `INTEGRATED` means validated, typed, and wired into the hierarchy; this audit did not independently prove that any of the 15 implementation labels satisfy that definition.

## Subsystem census and apparent maturity

“Maturity” below means only declared status and annotation coverage, not assembly correctness.

| Subsystem | Impl. | Headers | Lexical functions | Address-associated | Impl. statuses |
|---|---:|---:|---:|---:|---|
| audio | 2 | 2 | 36 | 28 | 2 absent |
| core | 11 | 7 | 211 | 91 | 2 INTEGRATED, 9 absent |
| game | 19 | 17 | 298 | 147 | 1 INTEGRATED, 2 TRANSCRIBED, 16 absent |
| graphics | 3 | 3 | 75 | 44 | 3 absent |
| input | 7 | 4 | 153 | 32 | 5 INTEGRATED, 2 absent |
| native | 51 | 0 | 249 | 107 | 51 absent |
| network | 8 | 7 | 186 | 22 | 1 INTEGRATED, 7 absent |
| resources | 4 | 3 | 108 | 46 | 4 absent |
| shared | 10 | 6 | 712 | 35 | 10 absent |
| town | 2 | 2 | 107 | 56 | 2 INTEGRATED |
| ui | 22 | 22 | 386 | 189 | 2 INTEGRATED, 4 TRANSCRIBED, 16 absent |
| world | 4 | 3 | 127 | 77 | 2 INTEGRATED, 2 absent |

`stubs/` contributes six headers but no implementation file to this reconstructed-source census.

## Build, check, and test status

Commands were run from the repository root through the pinned Nix environment and captured with exit codes rather than aborting the audit:

| Command | Exit | Observation |
|---|---:|---|
| `nix develop -c make` | 0 | Root binary build succeeds. |
| `nix develop -c make check` | 0 | 126/126 enabled objects reported built: 92 C++, 21 native C, 13 shims; 33 native sources are reported broken/skipped. |
| `nix develop -c make test` | 2 | Stops on the integration prerequisite. |
| `nix develop -c make test-unit` | 0 | Separately invoked deterministic component/host-boundary layer passes. |
| `nix develop -c make test-integration` | 2 | 8 failed / 4 passed in the isolated GUI suite. |

Failing GUI cases cover single-player mode-3 entry, several multiplayer host/session/layout flows, Escape exit, and lobby Search/Exit. The audit does not attribute a root cause or mistake host-only behavior for original x86 behavior.

## `PROGRESS.md`: claimed versus independently observed

- **Claimed:** extensive class/subsystem validation and many `INTEGRATED` milestones. **Observed:** labels and prose exist, but 0 functions were independently assembly-checked in this audit; the claims remain unverified here.
- **Claimed historically:** several changing enabled-source totals (69, 133, 134, 135). **Observed now:** `make check` reports 126/126 enabled objects, with 33 native sources skipped. Historical counts are not used as current denominators.
- **Claimed/current:** GUI integration is 4/12. **Observed now:** independently reproduced as 4 passed / 8 failed.
- **Claimed:** prior audit was blocked by duplicate `fabric_exec` loading. **Observed now:** independently reproduced on all 15 registered child runs before turn 1.

## Unresolved stubs and provenance gaps

The inventory records 235 lexical `TODO`/deferred/stub hits and 1,774 lexical function entries without a nearby address annotation. Neither number is a confirmed defect count: comments, host-only helpers, OS shims, declarations, lexical parser errors, and legitimate temporary sites remain mixed in because the mandatory Luna hit-by-hit filtering did not run. The exact hit ledger is preserved in `inventory.json`.

# Individual Confirmed Issues

## AUDIT-INFRA-001 — High — Audit assurance / orchestration

- **Location:** audit runner configuration; source `file:line` and original function address are not applicable.
- **Current behavior:** every mandatory recursive Pi child exits before turn 1 while loading `/home/user/.pi/agent/extensions/pi-fabric-0.28.6/index.ts`; its `fabric_exec` tool conflicts with the recursively injected Nix-store `pi-fabric` extension.
- **Expected behavior:** the requested detached DAG requires `runner: "pi"`, `recursive: true`, and `extensions: true` workers to start and access read-only Ghidra tooling.
- **Exact evidence:** 15/15 registered child runs failed with the same duplicate-tool diagnostic; all show zero useful turns/tool calls. Six raw shards failed both initial and correction attempts, a seventh failed once after the blocker was established, and Luna failed both attempts.
- **Impact:** no raw comparison, compliance baseline, Terra disposition, negative sample, final adjudication, or final Luna compliance gate exists. Repository correctness cannot be characterized from this audit.
- **Provenance/disposition:** confirmed mechanically by Main from terminal notifications and `agents.status`; no model-authored confidence score is used.
- **Confidence/uncertainty:** high confidence in the startup conflict; the audit did not alter user extension configuration and does not assert which configuration change is appropriate.
- **Recommended direction:** remove the duplicate extension registration at the runner boundary, then rerun the same persisted shard inventory and all review gates.

## AUDIT-TEST-001 — Medium — Required test/compliance state

- **Location:** `Makefile:147`, `Makefile:179-180`; original function/address not applicable.
- **Current behavior:** `make test` exits 2 because its integration prerequisite fails; a separate `make test-integration` run reports 8 failed / 4 passed.
- **Policy expectation:** project `AGENTS.md` § “Build and tests” identifies `make test`, `make test-integration`, and `make test-all` as required validation layers, and requires integration coverage for SDL3/runtime/UI/input/rendering/audio changes.
- **Exact evidence:** `build-test-results.json`, `make_test.log`, and `make_test_integration.log`; the isolated GUI suite mapped and executed rather than being reported as a launch skip.
- **Impact:** the current host runtime/UI state is not green and completion claims depending on this layer are unsafe.
- **Provenance/disposition:** confirmed mechanically by Main; not assembly-dependent. The missing Luna final check is an audit limitation, not hidden.
- **Confidence/uncertainty:** high confidence in the command outcome; no claim is made about a common root cause across the eight tests.
- **Recommended direction:** diagnose each failing flow using the preserved `build/test-artifacts/` evidence and rerun the full affected layer.

There are **zero confirmed reconstructed-function correctness issues** because the mandatory assembly and Terra gates produced no results.

# Structural and/or Recurring Problems

## Confirmed audit-infrastructure recurrence

- **Count/denominator:** 15/15 registered child runs failed before turn 1 with the same duplicate `fabric_exec` registration.
- **Affected stages:** DeepSeek raw shards and Luna baseline; by dependency, all Terra and final-compliance stages.
- **Representative issue:** `AUDIT-INFRA-001`.
- **Likely common cause:** simultaneous loading of configured and recursively injected copies of `pi-fabric`.
- **Project-level risk:** prevents independent assembly assurance and encourages reliance on unreviewed labels/progress prose.
- **Disposition:** confirmed process pattern, not a reconstructed-code pattern.

## Possible provenance/status concentration

- **Observed counts:** 1,774/2,648 lexical entries lack an associated address annotation; 122/143 implementation files and 62/82 headers have no declared status.
- **Affected scope:** every major subsystem, especially `shared`, `native`, `network`, and host-heavy files.
- **Likely cause:** mixed reconstructed, host-only, compatibility, generated-stub, and lexical-false-positive content in the same census.
- **Risk:** provenance and maturity are difficult to measure reproducibly.
- **Disposition:** **possible pattern only**. Luna did not inspect every entry/hit, and the lexical function parser visibly produces false positives.

## Possible anti-pattern/stub concentration

The reproducible lexical scan covered 277 source/header files under `src`. Raw occurrence counts include comments and legitimate constructs and therefore are **not confirmed instance counts**:

| Candidate family | Raw occurrences |
|---|---:|
| TODO/deferred/stub | 235 |
| host `#ifndef _WIN32` regions | 213 |
| literal `vtable`/`VTBL_` text | 2,113 |
| raw-this-relative pattern | 301 |
| `void*` pattern | 5,757 |
| `extern "C"` | 122 |
| decompiler-label family | 1,190 |
| `_Ctor`/`_Dtor` names | 243 |
| deleting-destructor family | 3 |
| explicit `__thiscall`/`__fastcall` | 1,337 |
| `field_`/`param_` artifacts | 898 |
| `defsym` reference | 1 |

These are triage populations only. Without Luna’s mandatory hit-by-hit false-positive elimination and Terra review for assembly-dependent claims, none is promoted to a recurring project defect.

# Rejected and Unresolved Candidates

No DeepSeek candidate result was produced, so no candidate was Terra-rejected.

| Candidate | Disposition | Reason/evidence needed |
|---|---|---|
| All potential assembly/source mismatches in 114 shards | Unresolved | DeepSeek raw workers did not start; independent disassembly and Terra disposition are absent. |
| Status/address/provenance census hits | Unresolved | Requires semantic classification of host-only helpers, declarations, parser false positives, and reconstructed methods. |
| Anti-pattern census families | Unresolved | Requires Luna inspection of every hit; assembly-dependent cases also require Terra. |
| Eight GUI integration failures | Unresolved root cause | Failure is confirmed, but each flow needs diagnosis; no common cause is asserted. |
| Existing `INTEGRATED` labels and `PROGRESS.md` assembly claims | Not independently verified | Must be rechecked instruction-by-instruction under the project definition. |

# Coverage and Limitations

- **Files inventoried:** 143 implementations and 82 headers in reconstructed scope.
- **Functions inventoried:** 2,648 lexical entries; this is not an AST-grounded exact function count.
- **Deeply assembly-checked:** 0/2,648 (0%).
- **Manifest shards completed:** 0/114 (0%).
- **Negative samples:** 0.
- **Raw workers:** 13 registrations covering seven shard IDs; all failed before turn 1. Six shard IDs exhausted two attempts; the seventh was not retried after the identical systemic blocker was established.
- **Queued/not launched:** 107/114 shards; six are persistent-failure gaps and one is blocked after one start.
- **Luna:** 0 schema-valid baseline results after two starts.
- **Terra:** 0 shard reviewers and 0 final adjudicators, because no raw candidate artifact existed.
- **Ghidra:** no child reached a Ghidra query. The database and raw binary were not mutated. No statement relies on pseudocode or Ghidra labels.
- **Final validation:** mandatory raw, Luna, Terra, final-adjudication, and final-compliance gates failed or were not reachable.
- **Safe characterization:** inventory labels, lexical counts, build/test outcomes, Git ownership state, and the runner failure are characterized. Original function behavior, signatures, layouts, inheritance, ownership, virtual slots, and correctness are not safe to characterize.
- **Git:** pre-existing changes to `Makefile`, `PROGRESS.md`, `src/decompiled_cpp/core/HostMode3Bootstrap.cpp`, and untracked audit/test files were treated as user-owned. Reconstructed source and Ghidra were not changed.

# Method and Validation Ledger

## DAG and models

- Inventory: Main, deterministic `inventory.py`.
- Raw candidates: `deepseek/deepseek-v4-pro`, `thinking: xhigh`.
- Compliance: `openai-codex/gpt-5.6-luna`, `thinking: xhigh`.
- Intended independent review/final adjudication: `openai-codex/gpt-5.6-terra`, `thinking: max`; not started because prerequisites failed.
- All child requests used `runner: "pi"`, read-only core tools, `recursive: true`, and `extensions: true`; children were forbidden to spawn agents or read secret-bearing files.

## Agent IDs and assignments

| Model/stage | Shard/attempt | Agent ID | Disposition |
|---|---|---|---|
| DeepSeek raw | raw-001 / 1 | `1e8c95d999714df9908eb5fe35efac9e` | startup failure |
| DeepSeek raw | raw-001 / 2 | `5ad409f334d54c718ff31f1ff1b44e00` | startup failure |
| DeepSeek raw | raw-002 / 1 | `2ffba824bffd4d59be8621e38b9f297a` | startup failure |
| DeepSeek raw | raw-002 / 2 | `26dee2ce817e4da5b6157b986441901b` | startup failure |
| DeepSeek raw | raw-003 / 1 | `098e67e20fc14fa4bfc6148bee48e2a8` | startup failure |
| DeepSeek raw | raw-003 / 2 | `cf4cc35757bc4454a82388888c9ea8ff` | startup failure |
| DeepSeek raw | raw-004 / 1 | `398f9400b04346b8925de86e8d32a37d` | startup failure |
| DeepSeek raw | raw-004 / 2 | `438edbc8eb244ee3a1ef5aa76a260459` | startup failure |
| DeepSeek raw | raw-005 / 1 | `972e56197e524d4483c96d344ae9676b` | startup failure |
| DeepSeek raw | raw-005 / 2 | `018aa2e3253a4962a2f19d82993fd8b3` | startup failure |
| DeepSeek raw | raw-006 / 1 | `53ff5f999bc0486b964f525c55a46d08` | startup failure |
| DeepSeek raw | raw-006 / 2 | `77dbb73480a4471c8ec23501c3621957` | startup failure |
| DeepSeek raw | raw-007 / 1 | `ce5f9f44c9294f17ba4ee25bc559074e` | startup failure; no retry after systemic proof |
| Luna baseline | attempt 1 | `b6aa4c760bd24c899bb6851891c946cc` | startup failure |
| Luna baseline | attempt 2 | `9e0575a0744b4bc088c7ac18a1ad62d0` | startup failure |

No raw-to-Terra review relationships exist because no raw artifact passed schema validation.

## Reproducible census/test commands

```bash
python3 build/audit/decomp-audit-20260803T060437Z/inventory.py
python3 build/audit/decomp-audit-20260803T060437Z/split_scopes.py \
  build/audit/decomp-audit-20260803T060437Z
python3 build/audit/decomp-audit-20260803T060437Z/summarize.py \
  build/audit/decomp-audit-20260803T060437Z
nix develop -c make
nix develop -c make check
nix develop -c make test
nix develop -c make test-unit
nix develop -c make test-integration
```

## Disposition totals

| Disposition | Count |
|---|---:|
| Confirmed reconstructed-code correctness findings | 0 |
| Confirmed process/test-state issues | 2 |
| DeepSeek candidates | 0 |
| Terra-confirmed/rejected/needs-more-evidence | 0 / 0 / 0 |
| Schema-valid Luna findings | 0 |
| Completed raw shards | 0/114 |
| Persistent raw shard failures | 6 |
| Raw shard blocked after one attempt | 1 |
| Not launched under systemic blocker | 107 |

Machine-readable evidence is under `build/audit/decomp-audit-20260803T060437Z/`, including `manifest.json`, inventory/shard scopes, failure artifacts, test logs/results, `audit-summary.json`, and `final-gate-results.json`.
