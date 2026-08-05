# Lego Loco Audit — v2 Prompt (pyghidra bridge)

## Architecture

Child agents get Ghidra access through a bash-callable Python bridge.
**No `mcp.ghidra.*` needed in the orchestrator. No `recursive: true`.** 

```
┌─────────────────────────────────────────────────┐
│  MAIN fabric_exec (orchestrator)                 │
│  • agents.spawn() — orchestration only           │
│  • agents.status() — DAG advancement             │
│  • pi.write() — manifest + report persistence    │
│  • tools.models() — model discovery              │
│  • NEVER calls mcp.ghidra.*                      │
│                                                  │
│  ┌─ DeepSeek child agent ─────────────────────┐  │
│  │  tools: [read, grep, find, ls, bash]        │  │
│  │  bash: python3 -W ignore \                  │  │
│  │    build/audit/ghidra_bridge.py decompile X │  │
│  │  → Gets real decompilation, disassembly,    │  │
│  │    callers, callees, basic blocks            │  │
│  └────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

---

## Phase 0: Preliminaries (single fabric_exec invocation)

```ts
// 1. Read mandatory files
const rootAgents = await pi.read("/home/user/.pi/agent/AGENTS.md");
const projectAgents = await pi.read("AGENTS.md");
const progress = await pi.read("PROGRESS.md");

// 2. Discover models
const models = await tools.models();
const required = ["deepseek/deepseek-v4-pro", "openai-codex/gpt-5.6-terra", "openai-codex/gpt-5.6-luna"];
for (const key of required) {
  const [provider, id] = key.split("/");
  const found = models.find(m => m.provider === provider && m.id === id);
  print(`${found ? "✅" : "❌"} ${key}`);
}

// 3. Git status (observe only)
const gitStatus = await pi.bash({ cmd: "git status --short", settle: true });
print("Git status:", gitStatus.output);

// 4. Verify bridge works
const bridgeTest = await pi.bash({
  cmd: "python3 -W ignore build/audit/ghidra_bridge.py decompile 0x401000 2>&1 | head -5",
  settle: true
});
print("Bridge test:", bridgeTest.output?.substring(0, 200));

// 5. Create manifest directory
const TS = new Date().toISOString().replace(/[:.]/g, "-");
const MANIFEST_DIR = `build/audit/decomp-audit-${TS}`;
await pi.bash({ cmd: `mkdir -p ${MANIFEST_DIR}` });
```

---

## Phase 1: Build Inventory (detached child agents)

Spawn inventory agents that use `bash` + `ghidra_bridge.py` and `grep`/shell commands.
Return handles immediately — do NOT `agents.wait()`.

```ts
// === Inventory Agent: Source Code Census ===
const invSource = await agents.spawn({
  model: "deepseek/deepseek-v4-pro",
  thinking: "xhigh",
  tools: ["read", "bash", "grep", "find", "ls"],
  task: `Build a complete source code inventory. Run these commands:

# File listing
find src/decompiled_cpp -name '*.cpp' -o -name '*.h' | sort

# Address annotations
grep -rn "Address: 0x" src/decompiled_cpp | wc -l

# Status counts
grep -rn "Status: TRANSCRIBED" src/decompiled_cpp | wc -l
grep -rn "Status: VALIDATED" src/decompiled_cpp | wc -l
grep -rn "Status: INTEGRATED" src/decompiled_cpp | wc -l

# Ghidra function count (for comparison)
python3 -W ignore build/audit/ghidra_bridge.py functions 2>/dev/null | wc -l

# Gap: functions without address annotations
# (grep for function definitions, then check which lack "Address: 0x" above)

Return ONLY this exact JSON:
{
  "cpp_files": ["<path>", ...],
  "header_files": ["<path>", ...],
  "total_functions_with_address": <number>,
  "ghidra_total_functions": <number>,
  "status_counts": {
    "transcribed": <number>, "validated": <number>,
    "integrated": <number>, "unmarked": <number>
  },
  "functions_without_address": ["<path:line>", ...]
}`
});

// === Inventory Agent: Anti-Pattern Census ===
const invAntiPatterns = await agents.spawn({
  model: "deepseek/deepseek-v4-pro",
  thinking: "xhigh",
  tools: ["read", "bash", "grep", "find", "ls"],
  task: `Read AGENTS.md "Fix anti-patterns on sight" section completely.
For EACH anti-pattern listed there, run a grep command to count occurrences
in src/decompiled_cpp/ (exclude comments, docs, tests, and legitimate C ABI).

Patterns to check (from AGENTS.md):
1. _Ctor/_Dtor free functions → grep for _Ctor|_Dtor
2. Constructor void* returns or explicit vtable writes
3. Scalar/vector deleting-destructor flags
4. Class_Method(self, ...) or explicit this
5. Literal vtable assignment/access
6. Raw this+offset or cast-based field access
7. Flat inherited structs
8. void*/void** for known objects
9. FUN_/DAT_/PTR_LAB_ artifacts
10. param_1/field_XX when use proves meaning
11. C++ inside extern "C"
12. MSVC-only cast-to-lvalue
13. Internal no-op stubs

For each: provide count, sample file:line, and whether it's in a comment.
Be careful to eliminate false positives.

Return ONLY this exact JSON:
{
  "patterns": {
    "<pattern_name>": {
      "count": <number>,
      "samples": ["<file:line>", ...],
      "false_positive_rate": "<none|low|medium|high>"
    }
  },
  "total_hits": <number>
}`
});

// === Inventory Agent: Stubs, TODOs, Host Guards ===
const invStubs = await agents.spawn({
  model: "deepseek/deepseek-v4-pro",
  thinking: "xhigh",
  tools: ["read", "bash", "grep", "find", "ls"],
  task: `Audit deferred work and host boundaries:

1. grep -rn "TODO: decompile 0x" src/ --include='*.cpp' --include='*.h'
2. grep -rn "#ifndef _WIN32" src/ --include='*.cpp' --include='*.h' | wc -l
3. grep -rn "#ifndef _WIN32" src/ --include='*.cpp' --include='*.h' | head -30
4. ls src/stubs/ 2>/dev/null
5. grep -rn "// TODO" src/decompiled_cpp/ --include='*.cpp' --include='*.h' | head -40
6. Read PROGRESS.md, list all "- [ ]" pending items

Return ONLY this exact JSON:
{
  "pending_decompilations": [
    {"address": "0x...", "file": "...", "line": <number>}
  ],
  "host_guard_count": <number>,
  "host_guard_samples": ["<file:line>", ...],
  "stub_files": ["<path>", ...],
  "progress_pending": ["<todo text>", ...]
}`
});

// === Persist manifest, return immediately ===
const manifest = {
  started: new Date().toISOString(),
  manifest_dir: MANIFEST_DIR,
  phases: {
    inventory: {
      status: "running",
      agents: [
        { id: invSource.id, label: "source_census", status: "running" },
        { id: invAntiPatterns.id, label: "anti_pattern_census", status: "running" },
        { id: invStubs.id, label: "stubs_and_todos", status: "running" },
      ]
    },
    raw_shards: { status: "pending", agents: [] },
    reviews: { status: "pending", agents: [] },
    final: { status: "pending", agents: [] },
  }
};

await pi.write({
  path: `${MANIFEST_DIR}/manifest.json`,
  text: JSON.stringify(manifest, null, 2)
});

print(`Inventory phase spawned. Manifest: ${MANIFEST_DIR}/manifest.json`);
return { phase: "inventory", agents: manifest.phases.inventory.agents.map(a => a.id) };
```

---

## Phase 2: Raw DeepSeek Shards (follow-up invocation)

After inventory agents complete, spawn shard workers. Each shard worker uses the
bridge to independently query Ghidra for its assigned functions.

```ts
// Read manifest, check which inventory agents completed
const manifest = JSON.parse(await pi.read(`${MANIFEST_DIR}/manifest.json`));

// ... (DAG advancement logic — check agents.status(), collect results) ...

// Example: spawn a raw DeepSeek shard worker
const shard = await agents.spawn({
  model: "deepseek/deepseek-v4-pro",
  thinking: "xhigh",
  tools: ["read", "bash", "grep", "find", "ls"],
  task: `You are a read-only assembly-to-source auditor.
Your shard covers these functions: 0x405E60, 0x405F00, 0x406000

FOR EACH function address, run ALL of these commands and capture output:

  python3 -W ignore build/audit/ghidra_bridge.py decompile ADDR 2>/dev/null
  python3 -W ignore build/audit/ghidra_bridge.py disassemble ADDR 2>/dev/null
  python3 -W ignore build/audit/ghidra_bridge.py callers ADDR 2>/dev/null
  python3 -W ignore build/audit/ghidra_bridge.py callees ADDR 2>/dev/null
  python3 -W ignore build/audit/ghidra_bridge.py basic_blocks ADDR 2>/dev/null

Then read the corresponding source files from src/decompiled_cpp/.
Compare control flow, data flow, calling conventions, return values,
ownership, vtable usage, and struct layouts.

For EVERY discrepancy found, cite:
- Exact Ghidra instruction addresses and bytes
- Source file path and line
- What the assembly does vs what the source does
- Why it matters (correctness, compatibility, etc.)

Return ONLY this exact JSON:
{
  "shard_id": "<unique>",
  "functions_examined": [
    {
      "address": "0x...",
      "source_file": "<path>",
      "declared_status": "TRANSCRIBED|VALIDATED|INTEGRATED|UNMARKED",
      "basic_blocks_ghidra": <number>,
      "basic_blocks_source": <number>,
      "calls_resolved": <number>,
      "calls_unresolved": <number>,
      "match": true|false
    }
  ],
  "candidate_issues": [
    {
      "id": "DEEPSEEK-<shard>-<seq>",
      "severity": "low|medium|high|critical",
      "category": "control_flow|data_flow|calling_convention|ownership|layout|virtual_dispatch|other",
      "function": "0x...",
      "source_file": "<path>",
      "source_line": <number>,
      "current_behavior": "<what source does>",
      "assembly_behavior": "<what assembly does>",
      "evidence_instructions": ["0x...: <bytes> <mnemonic>", ...],
      "evidence_secondary": "<xrefs|callers|CFG|vtable|...>",
      "impact": "<description>",
      "confidence": "low|medium|high"
    }
  ],
  "gaps": ["<description>", ...],
  "tool_failures": ["<description>", ...]
}`
});
```

---

## Phase 3: Terra Reviewers (follow-up invocation)

After a raw shard completes, spawn an independent Terra reviewer. The Terra
reviewer queries Ghidra ITSELF via the bridge — it does NOT trust DeepSeek.

```ts
const terraReview = await agents.spawn({
  model: "openai-codex/gpt-5.6-terra",
  thinking: "max",
  tools: ["read", "bash", "grep", "find", "ls"],
  task: `You are an independent correctness reviewer.
DO NOT trust the DeepSeek findings. Reproduce all evidence yourself.

FIRST, for every function in the shard, query Ghidra independently:

  python3 -W ignore build/audit/ghidra_bridge.py decompile ADDR 2>/dev/null
  python3 -W ignore build/audit/ghidra_bridge.py disassemble ADDR 2>/dev/null
  python3 -W ignore build/audit/ghidra_bridge.py callers ADDR 2>/dev/null
  python3 -W ignore build/audit/ghidra_bridge.py callees ADDR 2>/dev/null
  python3 -W ignore build/audit/ghidra_bridge.py basic_blocks ADDR 2>/dev/null

SECOND, read the DeepSeek candidate findings from:
  build/audit/decomp-audit-<TS>/shards/<shard_id>_deepseek.json

THIRD, for each DeepSeek candidate:
- Disposition as confirmed / rejected / needs_more_evidence
- For CONFIRMED: provide YOUR OWN independent instruction addresses
  and a SECOND evidence class (CFG, xrefs, callers, vtable, alloc size, etc.)
- Reject claims based only on decompiler pseudocode
- Distinguish compiler-generated code (thunks, EH, RTTI) from user-authored code

FOURTH, negative sample: independently check at least 20% of the shard's
functions that DeepSeek did NOT flag (minimum 5). Report any NEW issues found.

Return ONLY this exact JSON:
{
  "shard_id": "<same as DeepSeek>",
  "reviewer": "openai-codex/gpt-5.6-terra",
  "dispositions": [
    {
      "candidate_id": "DEEPSEEK-...",
      "disposition": "confirmed|rejected|needs_more_evidence",
      "independent_evidence_instructions": ["0x...: <bytes>", ...],
      "independent_evidence_secondary": "<description>",
      "reasoning": "<why>"
    }
  ],
  "negative_sample": {
    "functions_checked": ["0x...", ...],
    "new_issues_found": [...]
  },
  "independent_issues": [...]
}`
});
```

---

## Phase 4: Luna Compliance (parallel with inventory)

```ts
const lunaBaseline = await agents.spawn({
  model: "openai-codex/gpt-5.6-luna",
  thinking: "xhigh",
  tools: ["read", "bash", "grep", "find", "ls"],
  task: `Read BOTH AGENTS.md files completely:
  1. /home/user/.pi/agent/AGENTS.md
  2. ./AGENTS.md

Audit the ENTIRE repository for compliance. Use grep/census commands first,
then inspect hits to eliminate false positives. Check:

- assembly-first source of truth
- original address annotations on every reconstructed function
- TRANSCRIBED/VALIDATED/INTEGRATED status rules
- canonical class/header ownership (one definition per class)
- real C++ inheritance and typed virtual dispatch
- prohibited: vtable ops, raw this, void*, extern "C" for C++, 
  decompiler labels, deleting-destructor patterns
- constructor/destructor rules
- exact #ifndef _WIN32 host boundary (no other guards)
- SDL3 organization in src/sdl3_shims/
- stub policy (loud failure, no silent success)
- original x86 layout documentation vs host-native
- PROGRESS.md accuracy
- NixOS build constraints

Every issue MUST cite:
- Exact AGENTS.md rule
- Source path and exact line
- Source excerpt
- Severity and impact

Return ONLY this exact JSON:
{
  "reviewer": "openai-codex/gpt-5.6-luna",
  "phase": "baseline",
  "issues": [
    {
      "id": "LUNA-<seq>",
      "rule": "<AGENTS.md section/line>",
      "source_file": "<path>",
      "source_line": <number>,
      "excerpt": "<offending code>",
      "severity": "low|medium|high|critical",
      "impact": "<description>",
      "needs_assembly_verification": true|false
    }
  ],
  "census": {
    "total_files_checked": <number>,
    "total_issues": <number>,
    "by_severity": {"low": N, "medium": N, "high": N, "critical": N}
  }
}`
});
```

---

## Ghidra Bridge Reference

Child agents use these bash commands (no `mcp.ghidra.*` needed):

| Command | Output |
|---------|--------|
| `python3 -W ignore build/audit/ghidra_bridge.py decompile 0x401000 2>/dev/null` | Decompiled C |
| `python3 -W ignore build/audit/ghidra_bridge.py disassemble 0x401000 2>/dev/null` | Full disassembly |
| `python3 -W ignore build/audit/ghidra_bridge.py callers 0x401000 2>/dev/null` | Who calls this |
| `python3 -W ignore build/audit/ghidra_bridge.py callees 0x401000 2>/dev/null` | What this calls |
| `python3 -W ignore build/audit/ghidra_bridge.py basic_blocks 0x401000 2>/dev/null` | BB count + edges |
| `python3 -W ignore build/audit/ghidra_bridge.py functions 2>/dev/null` | All 1,748 functions |
| `python3 -W ignore build/audit/ghidra_bridge.py search Pattern 2>/dev/null` | Search function names |
| `python3 -W ignore build/audit/ghidra_bridge.py env 2>/dev/null` | Print env vars |

Note: `-W ignore` suppresses deprecation warnings. `2>/dev/null` silences
stderr noise. Function names are Ghidra defaults (`FUN_00401000`) — the
semantic names live in the reconstructed source, linked by address.

---

## Key Rules (preserved from original prompt)

1. Never use `agents.run()`, `agents.wait()`, or polling in the main invocation.
2. Spawn with `Promise.all`, return handles, advance DAG on follow-ups.
3. No `recursive: true` — it crashes agents.
4. No `extensions: true` — it doesn't add MCP or agents.
5. Child agents get: `["read", "bash", "grep", "find", "ls"]` (omit edit/write).
6. Main writes artifacts (`pi.write`), children return structured JSON.
7. All child agents use `runner: "pi"`.
8. Ghidra-aware agents use DeepSeek (`xhigh`) or Terra (`max`).
9. Luna uses `xhigh` for policy compliance only.
10. Every assembly claim needs two independent confirmations for high/critical.
11. Report "claimed" vs "observed" separately.
12. Never claim a repo-wide audit if any shard failed.
