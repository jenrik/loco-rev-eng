---
name: decompile-class
description: >-
  Reusable Fabric programs for automated reverse-engineering decompilation.
  Orchestrates reviewer→primary→block-reviewer loops for single classes,
  concurrency pools for parallel multi-class runs, and a supervisor for
  cross-cutting architectural decisions.
---

# Decompilation Tools

Three Fabric programs for automated decompilation of loco.exe classes.

## Files

| File | Purpose |
|------|---------|
| `tools/decompile-class.ts` | Single-class decompilation loop |
| `tools/decompile-parallel.ts` | Concurrency pool for multiple classes |
| `tools/supervisor.ts` | Top-level orchestrator with persistent supervisor actor |

## Architecture

```
supervisor.ts
  ├─ Creates SUPERVISOR ACTOR (persistent — discovers work + answers blocked primaries)
  ├─ [discover mode] Discovery phase — supervisor analyzes codebase, builds work queue
  │     • Reads PROGRESS.md for remaining work context
  │     • Greps for Status: tags (TRANSCRIBED/VALIDATED/INTEGRATED)
  │     • Runs make check for compilation status
  │     • Uses Ghidra to identify functions needing decompilation
  │     • Returns prioritized JSON work queue
  ├─ [explicit mode] Initial deep review — assesses dependencies, sets priority order
  └─ Dispatches → decompileClass() × N via concurrency pool

       decompileClass()  (one instance per class)
         while pass < maxIter:
           orchestrator runs make check → passes output to reviewer
           REVIEWER (one-shot, JSON Schema)
             → if INTEGRATED + zero BLOCKERs → return approved
           PRIMARY (one-shot, JSON Schema)
             → status: DONE | BLOCKED | PARTIAL
             if BLOCKED:
               BLOCK REVIEWER (one-shot, JSON Schema)
                 → validates legitimacy
                 if LEGITIMATE → route to supervisor, return blocked
                 if NOT → tell primary to continue

       decompile-parallel.ts
         Concurrency pool — maxParallel in-flight, not batched.
         When one finishes, the next starts immediately.
         Failures captured per job.
```

## Usage

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
  ghidraDatabase: "loco12",
  vtableAddress: "0x477848",
  parentClass: "RESDATA_GameVehicle",
  contextFiles: ["shared/types.h", "core/Entity.h"],
  targetStatus: "INTEGRATED",
  maxIterations: 5,
});
```

### Parallel (standalone)

```typescript
const code = await pi.read('tools/decompile-parallel.ts');
const wrapped = '(async () => { ' + code + ' })()';
const { decompileParallel } = await eval(wrapped);

return await decompileParallel([
  { className: "Building", headerPath: "game/Building.h", implPath: "game/Building.cpp",
    functions: [{ name: "Building::Foo", address: "0x412345" }] },
  { className: "Train", headerPath: "game/Train.h", implPath: "game/Train.cpp",
    functions: [{ name: "Train::Bar", address: "0x423456" }] },
], { maxParallel: 3 });
```

### Supervisor (multi-class with dependency management)

**Auto-discovery mode** (recommended — supervisor finds classes needing work):

```typescript
const code = await pi.read('tools/supervisor.ts');
const wrapped = '(async () => { ' + code + ' })()';
const { run } = await eval(wrapped);

return await run({
  discover: true,
  scope: "below-integrated",  // "below-integrated" | "transcribed" | "validated" | "all"
  ghidraDatabase: "loco12",
  maxParallel: 3,
  maxIterations: 5,
});
```

**Explicit mode** (backward compatible — you specify the class list):

```typescript
const code = await pi.read('tools/supervisor.ts');
const wrapped = '(async () => { ' + code + ' })()';
const { run } = await eval(wrapped);

return await run({
  classes: [
    { className: "Building", headerPath: "game/Building.h", implPath: "game/Building.cpp",
      functions: [{ name: "Building::Foo", address: "0x412345" }] },
    { className: "Train", headerPath: "game/Train.h", implPath: "game/Train.cpp",
      functions: [{ name: "Train::Bar", address: "0x423456" }] },
  ],
  ghidraDatabase: "loco12",
  maxParallel: 3,
  maxIterations: 5,
});
```

## Parameters — decompileClass

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| className | string | required | C++ class name |
| headerPath | string | required | Path to .h relative to src/decompiled_cpp |
| implPath | string | required | Path to .cpp relative to src/decompiled_cpp |
| functions | FunctionTarget[] | required | Functions with name, address, vtableSlot?, description? |
| ghidraDatabase | string | required | Ghidra DB ID (must be open already) |
| parentClass | string | — | Parent class for inheritance context |
| vtableAddress | string | — | Vtable address for documentation |
| contextFiles | string[] | [] | Extra files agents should read |
| targetStatus | string | "INTEGRATED" | Required status for approval |
| maxIterations | number | 5 | Max review-primary passes |
| primaryModel | string | deepseek/deepseek-v4-pro | Model for primary agent |
| reviewerModel | string | deepseek/deepseek-v4-pro | Model for reviewer + block reviewer |
| supervisorId | string | — | Supervisor actor ID for routing blocked decisions |

## Parameters — decompileParallel

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| classes | ClassConfig[] | required | Array of decompileClass params |
| maxParallel | number | 4 | Max concurrent decompileClass calls |
| maxIterations | number | 5 | Passed to each decompileClass |
| supervisorId | string | — | Passed to each decompileClass |

## Parameters — supervisor.run

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| discover | boolean | false | Auto-discovery: supervisor finds classes needing work |
| scope | string | "below-integrated" | Discovery scope: "below-integrated", "transcribed", "validated", "all" |
| classes | ClassConfig[] | — | Explicit class list (ignored when discover: true) |
| ghidraDatabase | string | "loco12" | Ghidra DB ID |
| maxParallel | number | 3 | Max concurrent decompileClass calls |
| maxIterations | number | 5 | Max review-primary passes per class |
| primaryModel | string | deepseek/deepseek-v4-pro | Model for primary agents |
| reviewerModel | string | deepseek/deepseek-v4-pro | Model for reviewer agents |
| supervisorModel | string | deepseek/deepseek-v4-pro | Model for supervisor actor |

### Discovery scopes

| Scope | Description |
|-------|-------------|
| `"below-integrated"` | Files at TRANSCRIBED, VALIDATED, or missing status tag |
| `"transcribed"` | Only files at TRANSCRIBED status (need validation) |
| `"validated"` | Only files at VALIDATED status (need integration) |
| `"all"` | All files including INTEGRATED (for re-validation) |

### Discovery process

When `discover: true`, the supervisor actor:
1. Reads PROGRESS.md for remaining work context and priorities
2. Greps for `Status:` tags in all .cpp/.h files
3. Runs `make check` to verify compilation state
4. Reads files to extract function lists and address annotations
5. Uses Ghidra for new decompilation targets (no existing .cpp)
6. Returns a prioritized JSON work queue

The TypeScript orchestrator parses the JSON and dispatches classes through
the concurrency pool, just as with explicit mode.

## Return values

### decompileClass

```typescript
{ status: "approved", className, iterations, finalReview }
{ status: "blocked", className, iterations, finalReview, blocks, supervisorDecisions?, blockReview }
{ status: "max_iterations_reached", className, iterations, finalReview }
{ status: "error", className, pass, error }
```

### decompileParallel / supervisor.run

```typescript
{ total, approved, blocked, maxIterationsReached, errors, results }
```

## Schemas

### PRIMARY_SCHEMA

```json
{
  "status": "DONE" | "BLOCKED" | "PARTIAL",
  "summary": "...",
  "compilationStatus": "PASS" | "FAIL" | "UNKNOWN",
  "blocks": [{ "what": "...", "why": "...", "suggestion?": "...", "address?": "..." }]
}
```

### REVIEW_SCHEMA

```json
{
  "approved": true | false,
  "currentStatus": "PRE_TRANSCRIBED" | "TRANSCRIBED" | "VALIDATED" | "INTEGRATED",
  "summary": "...",
  "issues": [{ "severity": "BLOCKER"|"WARNING"|"INFO", "category": "...", "description": "...", "fix": "..." }],
  "compilationStatus": "PASS" | "FAIL" | "UNKNOWN"
}
```

### BLOCK_REVIEW_SCHEMA

```json
{
  "legitimate": true | false,
  "reason": "...",
  "suggestion?": "..."
}
```

## Design decisions

- **One-shot agents with full context**: Primary and reviewer use `agents.run()` with JSON Schema for structured output. Every prompt includes full context (class, files, functions, Ghidra DB, AGENTS.md path) via `taskPrefix()`. No persistent state needed.
- **Orchestrator-owned build**: `make check` is run by the TypeScript orchestrator, not the agents. Output is passed to the reviewer.
- **Block reviewer gate**: Primary cannot unilaterally stop the loop. A separate block reviewer validates that the block is legitimate before the loop ends.
- **Approval enforced in TypeScript**: The orchestrator checks `approved`, `currentStatus`, `compilationStatus`, blocker count, and build output — not just trusting the model's `approved` field.
- **Ghidra lifecycle external**: Agents don't open/close databases. The caller ensures the database is open before dispatching.
