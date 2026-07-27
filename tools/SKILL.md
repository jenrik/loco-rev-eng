---
name: decompile-class
description: >-
  Reusable Fabric programs for automated reverse-engineering decompilation.
  Runs persistent-primary class workflows under an incremental, dependency-aware
  supervisor scheduler.
---

# Decompilation Tools

Fabric programs for automated decompilation of `loco.exe` classes.

## Files

| File | Purpose |
|------|---------|
| `tools/run-session.ts` | Session entry point; opens a fresh Ghidra database and starts the supervisor |
| `tools/supervisor.ts` | Persistent incremental scheduler for multi-class runs |
| `tools/decompile-class.ts` | One class: persistent PRIMARY plus one-shot reviewer gates |
| `tools/workflow-core.ts` | Pi-facing TypeScript bridge to durable workflow state |
| `tools/workflow_core.py` | Atomic Python JSON CLI for evidence, task, dependency, deferred-work, and write-audit state |
| `docs/evidence-guided-decompilation-workflow.md` | Long-lived design intent and staged migration plan |

`tools/decompile-parallel.ts` was removed. Its fixed concurrency pool was
superseded by the dependency-aware scheduler in `supervisor.ts`.

## Architecture

```text
run-session.ts
  ├─ opens raw loco.exe with a fresh Ghidra database ID
  └─ supervisor.run()
       ├─ creates persistent SUPERVISOR actor
       ├─ sends settled state snapshot
       │    • currently running attempts
       │    • completions since the previous scheduling turn
       │    • validated legitimate blocks
       │    • latest outcomes
       │    • available capacity
       ├─ supervisor returns START | WAIT | COMPLETE
       ├─ TypeScript launches only supervisor-authorized classes
       └─ repeats after completions until COMPLETE

supervisor-authorized class attempt
  └─ decompileClass()
       ├─ orchestrator runs make check
       ├─ one-shot REVIEWER validates source against Ghidra
       ├─ if not approved, create/reuse persistent PRIMARY actor
       ├─ PRIMARY returns:
       │    • PARTIAL → nudge same actor; no reviewer call
       │    • DONE → run build and reviewer
       │    • BLOCKED → run one-shot block reviewer
       ├─ block reviewer returns:
       │    • legitimate=false → reason goes to same PRIMARY
       │    • legitimate=true → stop class and report blocked
       │    • failed review → class returns error, never blocked
       └─ always removes persistent PRIMARY actor
```

## Scheduling guarantees

- The supervisor does not fill a complete queue upfront.
- Exactly one supervisor activation is in flight.
- Class completions that occur during a supervisor turn are buffered.
- After the supervisor settles, a stale launch decision is discarded and a fresh,
  coalesced state snapshot is sent before any new class starts.
- `maxParallel` is a hard capacity limit, not a target. The supervisor decides how
  much of that capacity is dependency-safe to use.
- TypeScript executes launch directives; the model does not own process lifecycle.
- A class may be retried only with `retry: true`, and attempts are bounded by
  `maxAttemptsPerClass`.

## Directed discovery

Discovery accepts an optional free-form `direction`. When present, the supervisor:

- translates the objective into observable success criteria;
- traces the runtime dependency cone backward from that capability;
- prioritizes only work that advances the objective;
- defers unrelated status cleanup;
- re-evaluates the dependency cone after every completion or legitimate block;
- includes the direction in every settled scheduling snapshot.

Example:

```typescript
return await run({
  discover: true,
  scope: "all", // permit re-validation of dependencies already tagged INTEGRATED
  direction: "Get the main menu and all of its runtime dependencies working",
  ghidraDatabase: "locoUniqueSessionId",
  maxParallel: 3,
});
```

Use `scope: "all"` for runtime-capability objectives that may require revisiting
files already tagged INTEGRATED. The direction still prevents unrelated re-validation.
Omit `direction`, pass `null`, or use an empty string for broad status-based discovery.

## Block semantics

The block reviewer is only a stop gate:

1. PRIMARY returns `BLOCKED` with one or more precise reasons.
2. The one-shot block reviewer determines whether stopping is legitimate.
3. If false, its reason and suggestion are sent to the same persistent PRIMARY,
   which performs another turn.
4. If true, the class loop stops and returns `status: "blocked"`.
5. The supervisor learns about that validated block at the next settled scheduling
   boundary and may schedule dependencies or a later fresh attempt.
6. If block review cannot produce valid structured output after retries, the class
   returns `status: "error"`; an unvalidated block is never reported as legitimate.

The supervisor does not answer the blocked PRIMARY and does not resume the stopped
class loop. Any retry is a fresh `decompileClass()` attempt with optional supervisor
guidance.

## Usage

### Full session

```typescript
const code = await pi.read('tools/run-session.ts');
const wrapped = '(async () => { ' + code + ' })()';
return await eval(wrapped);
```

### Supervisor discovery mode

The caller must open the raw binary and wait for analysis first.

```typescript
const code = await pi.read('tools/supervisor.ts');
const wrapped = '(async () => { ' + code + ' })()';
const { run } = await eval(wrapped);

return await run({
  discover: true,
  scope: "all", // permit re-validation of dependencies already tagged INTEGRATED
  direction: "Get the main menu and all of its runtime dependencies working",
  ghidraDatabase: "locoUniqueSessionId",
  maxParallel: 3,
  maxIterations: 5,
  maxAttemptsPerClass: 3,
});
```

### Supervisor explicit mode

```typescript
return await run({
  classes: [
    {
      className: "GameVehicle",
      headerPath: "game/GameVehicle.h",
      implPath: "game/GameVehicle.cpp",
      functions: [
        { name: "GameVehicle::StartMoving", address: "0x4129C0", vtableSlot: 1 },
      ],
      targetStatus: "INTEGRATED",
    },
  ],
  ghidraDatabase: "locoUniqueSessionId",
  maxParallel: 3,
});
```

### Single class

```typescript
const code = await pi.read('tools/decompile-class.ts');
const wrapped = '(async () => { ' + code + ' })()';
const { decompileClass } = await eval(wrapped);

return await decompileClass({
  className: "GameVehicle",
  headerPath: "game/GameVehicle.h",
  implPath: "game/GameVehicle.cpp",
  functions: [
    { name: "GameVehicle::StartMoving", address: "0x4129C0", vtableSlot: 1 },
  ],
  ghidraDatabase: "locoUniqueSessionId",
  targetStatus: "INTEGRATED",
  maxIterations: 5,
});
```

## Parameters — `decompileClass`

| Parameter | Default | Description |
|-----------|---------|-------------|
| `className` | required | C++ class name |
| `headerPath` | required | Header relative to `src/decompiled_cpp` |
| `implPath` | required | Implementation relative to `src/decompiled_cpp` |
| `functions` | required | Verified names and binary addresses |
| `ghidraDatabase` | required | Already-open Ghidra database |
| `parentClass` | — | Inheritance context |
| `vtableAddress` | — | Original vtable address |
| `contextFiles` | `[]` | Additional files for agents |
| `targetStatus` | `INTEGRATED` | Exact status required for approval |
| `maxIterations` | `5` | Maximum PRIMARY work turns, including PARTIAL nudges |
| `supervisorGuidance` | — | Guidance supplied to a fresh scheduler retry |
| `primaryModel` | `deepseek/deepseek-v4-pro` | Persistent PRIMARY model |
| `reviewerModel` | `deepseek/deepseek-v4-pro` | Reviewer and block-reviewer model |

## Parameters — `supervisor.run`

| Parameter | Default | Description |
|-----------|---------|-------------|
| `discover` | `false` | Incrementally discover work |
| `scope` | `below-integrated` | Discovery status scope |
| `direction` | — | Optional capability objective used to focus dependency discovery and scheduling |
| `classes` | — | Explicit allowed target list |
| `ghidraDatabase` | required in practice | Already-open Ghidra database |
| `maxParallel` | `3` | Hard concurrent-attempt limit |
| `maxIterations` | `5` | PRIMARY turns per class attempt |
| `maxAttemptsPerClass` | `3` | Fresh-attempt limit per class |
| `maxSupervisorTurns` | `100` | Scheduling-turn safety limit |
| `primaryModel` | `deepseek/deepseek-v4-pro` | Default PRIMARY model |
| `reviewerModel` | `deepseek/deepseek-v4-pro` | Default reviewer model |
| `supervisorModel` | `deepseek/deepseek-v4-pro` | Persistent supervisor model |

## Structured outputs

### PRIMARY

```json
{
  "status": "DONE | PARTIAL | BLOCKED",
  "summary": "...",
  "compilationStatus": "PASS | FAIL | UNKNOWN",
  "blocks": [{ "what": "...", "why": "...", "suggestion": "...", "address": "..." }]
}
```

### Reviewer

```json
{
  "approved": false,
  "currentStatus": "PRE_TRANSCRIBED | TRANSCRIBED | VALIDATED | INTEGRATED",
  "summary": "...",
  "issues": [
    { "severity": "BLOCKER | WARNING | INFO", "category": "...", "description": "...", "fix": "..." }
  ],
  "compilationStatus": "PASS | FAIL | UNKNOWN"
}
```

### Block reviewer

```json
{
  "legitimate": true,
  "reason": "...",
  "suggestion": "..."
}
```

### Supervisor directive

```json
{
  "action": "START | WAIT | COMPLETE",
  "summary": "...",
  "reason": "...",
  "starts": [
    {
      "className": "DependencyClass",
      "headerPath": "game/DependencyClass.h",
      "implPath": "game/DependencyClass.cpp",
      "functions": [{ "name": "DependencyClass::Method", "address": "0x..." }],
      "retry": false,
      "guidance": "..."
    }
  ]
}
```

## Return values

### `decompileClass`

```text
{ status: "approved", className, primaryTurns, reviewPasses, finalReview }
{ status: "blocked", className, blocks, blockReview, primaryTurns, reviewPasses, finalReview }
{ status: "max_iterations_reached", className, primaryTurns, reviewPasses, finalReview }
{ status: "error", className, error, primaryTurns, reviewPasses, finalReview? }
```

`blocked` always means the block reviewer explicitly returned `legitimate: true`.

### `supervisor.run`

```text
{
  total, approved, blocked, maxIterationsReached, errors,
  results,   // latest result for each class
  attempts,  // every attempt, including superseded blocked attempts
  direction  // normalized discovery objective, or null
}
```
