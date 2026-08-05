# Lego Loco Audit — Corrected Orchestration Prompt

## What Was Broken

Three fatal assumptions in the original prompt:

| Assumption | Reality |
|-----------|---------|
| Child agents can use `mcp.ghidra.*` | ❌ Child agents have only 7 tools: `read grep find ls bash edit write`. No MCP. |
| Child agents can spawn sub-agents via `agents.spawn` | ❌ Only the main `fabric_exec` block has `agents.*`. Children are leaf workers. |
| `recursive: true` enables deeper tool access | ❌ It crashes agents with a fabric_exec extension conflict. Never use it. |
| `extensions: true` adds MCP/agent tools | ❌ It only loads project extensions; doesn't add MCP, agents, or tools.models(). |

## Correct Architecture

```
┌──────────────────────────────────────────────┐
│  MAIN fabric_exec (orchestrator)              │
│  • agents.spawn() — all agent orchestration   │
│  • agents.status() — DAG advancement          │
│  • mcp.ghidra.* — ALL Ghidra queries          │
│  • pi.write() — artifact persistence          │
│  • tools.models() — model discovery           │
│                                               │
│  ┌─ Child Agent (deepseek) ────────────────┐  │
│  │  read, grep, find, ls, bash             │  │
│  │  Gets Ghidra data embedded in task       │  │
│  │  Does file analysis, pattern matching    │  │
│  │  Returns structured JSON                 │  │
│  └─────────────────────────────────────────┘  │
│  ┌─ Child Agent (terra) ───────────────────┐  │
│  │  read, grep, find, ls, bash             │  │
│  │  Cross-checks DeepSeek findings          │  │
│  │  Returns reviewed dispositions           │  │
│  └─────────────────────────────────────────┘  │
│  ┌─ Child Agent (luna) ────────────────────┐  │
│  │  read, grep, find, ls, bash             │  │
│  │  AGENTS.md compliance audit              │  │
│  │  Returns policy findings                 │  │
│  └─────────────────────────────────────────┘  │
└──────────────────────────────────────────────┘
```

**Ghia queries ALWAYS happen in the main fabric_exec TypeScript, never in child agents.**
Results are passed to children as inline data in their `task` prompt.

---

## Working Example 1: Discover Models + Open Ghidra

```ts
// Run in fabric_exec — this is the orchestrator's TypeScript, NOT a child agent

// 1. Discover available models
const models = await tools.models();
const required = ["deepseek/deepseek-v4-pro", "openai-codex/gpt-5.6-terra", "openai-codex/gpt-5.6-luna"];
for (const key of required) {
  const found = models.find(m => `${m.provider}/${m.id}` === key);
  print(`Model ${key}: ${found ? "✅ " + found.name : "❌ MISSING"}`);
}

// 2. Open Ghidra database (already open, returns existing)
const db = await mcp.ghidra.open_database({
  file_path: "/home/user/projects/v43/jenrik/lego-loco-rev-eng/lego-loco-unpacked/Exe/loco.exe",
  database_id: "locoaudit"
});
// Use the database ID from the response: db.structuredContent.database
const DB_ID = db.structuredContent.database;
print(`Ghidra database: ${DB_ID} (${db.structuredContent.function_count} functions)`);
```

---

## Working Example 2: Query Ghidra, Spawn a DeepSeek Worker

```ts
// Query Ghidra for a function's decompilation
const decomp = await mcp.ghidra.decompile_function({
  database: DB_ID,
  address: "0x405E60"
});

// Read the corresponding source file
const source = await pi.read("src/decompiled_cpp/GameObject.cpp");

// Spawn a DeepSeek agent with Ghidra data EMBEDDED in its task
const handle = await agents.spawn({
  model: "deepseek/deepseek-v4-pro",
  thinking: "xhigh",
  runner: "pi",
  tools: ["read", "grep", "find", "ls"],  // read-only only
  task: `You are a read-only assembly-to-source auditor.

Compare this Ghidra decompilation to the source file.

=== GHIDRA DECOMPILATION (0x405E60) ===
${decomp.structuredContent.decompiled_code}

=== SOURCE FILE (src/decompiled_cpp/GameObject.cpp) ===
${source}

=== TASK ===
1. Check that every basic block in the decompilation is present in the source.
2. Check that the calling convention, return type, and parameter count match.
3. Check that all call targets in the decompilation are resolved in the source.
4. Report any discrepancies as a JSON array of issues.

Return ONLY this exact JSON structure:
{
  "function": "GameObject::Draw",
  "address": "0x405E60",
  "declared_status": "TRANSCRIBED|VALIDATED|INTEGRATED|UNMARKED",
  "basic_blocks_observed": <number>,
  "basic_blocks_in_source": <number>,
  "calls_resolved": <number>,
  "calls_unresolved": <number>,
  "match": true|false,
  "issues": [
    {
      "severity": "low|medium|high|critical",
      "category": "control_flow|data_flow|calling_convention|ownership|layout|virtual_dispatch|other",
      "line": <source line number or null>,
      "description": "<what differs>",
      "assembly_evidence": "<exact instruction addresses and bytes>",
      "confidence": "low|medium|high"
    }
  ]
}`
});

print(`Spawned DeepSeek agent: ${handle.id}`);
```

---

## Working Example 3: Detached DAG Orchestration (Fire and Forget)

```ts
// The correct pattern: spawn all agents, persist manifest, return immediately.
// DO NOT await agents.wait() — let them complete asynchronously.
// Follow-up runs advance the DAG by checking agents.status().

const MANIFEST_DIR = `build/audit/decomp-audit-${new Date().toISOString().replace(/[:.]/g, "-")}`;
await pi.bash({ cmd: `mkdir -p ${MANIFEST_DIR}` });

// Phase 1: Build inventory (read-only agents)
const inventoryTasks = [
  {
    id: "inventory-files",
    model: "deepseek/deepseek-v4-pro",
    thinking: "xhigh",
    tools: ["read", "grep", "find", "ls"],
    task: `Build an inventory of all reconstructed C++ source files.

1. Run: find src/decompiled_cpp -name '*.cpp' -o -name '*.h' | sort
2. Run: grep -rn "Address: 0x" src/decompiled_cpp | wc -l
3. Run: grep -rn "Status: TRANSCRIBED" src/decompiled_cpp | wc -l
4. Run: grep -rn "Status: VALIDATED" src/decompiled_cpp | wc -l
5. Run: grep -rn "Status: INTEGRATED" src/decompiled_cpp | wc -l
6. Read AGENTS.md "Fix anti-patterns on sight" section and grep for each anti-pattern.
   Count occurrences, being careful to exclude comments, docs, and legitimate C ABI.

Return ONLY this exact JSON:
{
  "files": ["<path>", ...],
  "total_functions_with_address": <number>,
  "status_counts": {
    "transcribed": <number>,
    "validated": <number>,
    "integrated": <number>,
    "unmarked": <number>
  },
  "anti_patterns": {
    "<pattern_name>": { "count": <number>, "sample_files": ["<path>", ...] },
    ...
  }
}`
  },
  {
    id: "inventory-stubs",
    model: "deepseek/deepseek-v4-pro",
    thinking: "xhigh",
    tools: ["read", "grep", "find", "ls"],
    task: `Audit stubs and deferred work.

1. Run: grep -rn "TODO: decompile 0x" src/ --include='*.cpp' --include='*.h'
2. Run: grep -rn "#ifndef _WIN32" src/ --include='*.cpp' --include='*.h'
3. Run: grep -rn "stub" src/stubs/ --include='*.cpp' --include='*.h' -l
4. Read PROGRESS.md and list all pending TODOs (lines with "- [ ]")

Return ONLY this exact JSON:
{
  "pending_decompilations": [
    { "address": "0x...", "file": "...", "line": <number> }
  ],
  "host_guard_regions": <count>,
  "stub_files": ["<path>", ...],
  "progress_pending_todos": ["<todo text>", ...]
}`
  }
];

// Spawn ALL phase-1 agents concurrently
const phase1Handles = await Promise.all(
  inventoryTasks.map((t) => agents.spawn(t))
);

// Build manifest
const manifest = {
  started: new Date().toISOString(),
  database: DB_ID,
  phases: {
    inventory: {
      status: "running",
      agents: phase1Handles.map((h) => ({ id: h.id, task_id: h.name?.substring(0, 40), status: "running" }))
    }
  }
};

await pi.write({
  path: `${MANIFEST_DIR}/manifest.json`,
  text: JSON.stringify(manifest, null, 2)
});

print(`Audit DAG started. Manifest: ${MANIFEST_DIR}/manifest.json`);
print(`Spawned ${phase1Handles.length} inventory agents. They run detached.`);
print("On follow-up: read manifest, check agents.status(), advance DAG.");
```

---

## Working Example 4: Follow-Up — Advance the DAG

```ts
// This runs in a FOLLOW-UP fabric_exec invocation, triggered by agent completion.

const manifest = JSON.parse(
  await pi.read("build/audit/decomp-audit-<timestamp>/manifest.json")
);

// Check all running agents
for (const [phaseName, phase] of Object.entries(manifest.phases)) {
  if (phase.status !== "running") continue;

  let allDone = true;
  for (const agent of phase.agents) {
    if (agent.status === "running") {
      const s = await agents.status({ id: agent.id });
      if (s.status === "completed") {
        agent.status = "completed";
        agent.result = s.text;  // parse JSON output
        agent.usage = s.usage;
        agent.turns = s.turns;
      } else if (s.status === "failed") {
        agent.status = "failed";
        agent.error = s.stderr;
      } else {
        allDone = false;  // still running
      }
    }
  }

  if (allDone) {
    phase.status = "completed";

    // Advance to next phase
    if (phaseName === "inventory") {
      const inventory = phase.agents.find(a => a.task_id?.includes("inventory-files"));
      const stubs = phase.agents.find(a => a.task_id?.includes("inventory-stubs"));

      // Parse results and create shards...
      // Then spawn phase-2 agents (raw DeepSeek shards)
      // ... (see next example)
    }
  }
}

await pi.write({
  path: "build/audit/decomp-audit-<timestamp>/manifest.json",
  text: JSON.stringify(manifest, null, 2)
});

print("DAG advanced. Phases:", Object.entries(manifest.phases)
  .map(([n, p]) => `${n}: ${p.status}`)
  .join(", "));
```

---

## Working Example 5: Terra Reviewer with Independent Ghidra Evidence

The Terra reviewer needs Ghidra data. The orchestrator queries Ghidra and embeds it.

```ts
// 1. Query Ghidra for each function in the shard
const shardFunctions = ["0x405E60", "0x405F00", "0x406000"];
const decompilations = {};

for (const addr of shardFunctions) {
  const r = await mcp.ghidra.decompile_function({ database: DB_ID, address: addr });
  const disasm = await mcp.ghidra.disassemble_function({ database: DB_ID, address: addr });
  decompilations[addr] = {
    decompiled: r.structuredContent.decompiled_code,
    disassembly: disasm.structuredContent?.disassembly || disasm.text
  };
}

// 2. Spawn Terra reviewer with all Ghidra evidence embedded
const terraHandle = await agents.spawn({
  model: "openai-codex/gpt-5.6-terra",
  thinking: "max",
  runner: "pi",
  tools: ["read", "grep", "find", "ls"],
  task: `You are an independent correctness reviewer. Do NOT trust the DeepSeek
findings below — reproduce the evidence yourself from disassembly.

=== GHIDRA DATA FOR ALL SHARD FUNCTIONS ===
${JSON.stringify(decompilations, null, 2)}

=== DEEPSEEK CANDIDATE FINDINGS (DO NOT TRUST) ===
Read build/audit/decomp-audit-<timestamp>/shards/shard_01_deepseek.json

=== SOURCE FILES ===
Read each source file listed in the shard scope from src/decompiled_cpp/

=== TASK ===
For every DeepSeek candidate issue:
1. Independently verify from the disassembly above.
2. Require exact instruction addresses as evidence.
3. Require a SECOND evidence class (CFG, xrefs, callers, vtable, etc.).
4. Distinguish compiler-generated code from user-authored code.
5. Reject claims based only on decompiler pseudocode.

Also inspect a NEGATIVE SAMPLE: check at least 20% of shard functions (min 5)
that DeepSeek did NOT flag, to estimate false negatives.

Return ONLY this exact JSON:
{
  "shard_id": "shard_01",
  "reviewer_model": "openai-codex/gpt-5.6-terra",
  "dispositions": [
    {
      "candidate_id": "<from DeepSeek>",
      "disposition": "confirmed|rejected|needs_more_evidence",
      "independent_evidence": {
        "instruction_addresses": ["0x...", ...],
        "second_evidence_class": "cfg|xrefs|callers|vtable|allocation|offsets|globals",
        "second_evidence_detail": "<description>"
      },
      "reasoning": "<why confirmed/rejected>"
    }
  ],
  "negative_sample": {
    "functions_checked": ["0x...", ...],
    "new_issues_found": [
      { "address": "0x...", "issue": "<description>" }
    ]
  },
  "new_independent_issues": [...]
}`
});

print(`Spawned Terra reviewer: ${terraHandle.id}`);
```

---

## Summary of Constraints

| Capability | Main fabric_exec | Child Agent |
|-----------|-----------------|-------------|
| `mcp.ghidra.*` | ✅ Yes | ❌ No |
| `agents.spawn()` | ✅ Yes | ❌ No |
| `agents.status()` | ✅ Yes | ❌ No |
| `agents.wait()` | ✅ Yes | ❌ No |
| `tools.models()` | ✅ Yes | ❌ No |
| `pi.read / pi.write / pi.edit` | ✅ Yes | ✅ read only (if tools allow) |
| `pi.bash / pi.grep / pi.find / pi.ls` | ✅ Yes | ✅ Yes |
| `extensions.*` | ✅ Yes | ❌ No (even with extensions:true) |
| `ask_user` | ✅ Yes | ❌ No |
| `spawn_session` | ✅ Yes | ❌ No |

## Valid agents.spawn() Parameters

```ts
agents.spawn({
  task: string,              // REQUIRED — the prompt
  model?: string,            // "deepseek/deepseek-v4-pro" | "openai-codex/gpt-5.6-terra" | "openai-codex/gpt-5.6-luna"
  thinking?: string,         // "low" | "medium" | "high" | "xhigh" | "max"
  runner?: "pi",             // only "pi" is valid
  tools?: string[],          // subset of: ["read","bash","edit","write","grep","find","ls"]
  extensions?: boolean,      // loads project extensions (NOT MCP/agents)
  // recursive: true         // BROKEN — never use
})
```

## Correct DAG Pattern

1. **Main run**: Open Ghidra, build inventory via child agents, spawn raw DeepSeek shards, persist manifest, return.
2. **Follow-up 1**: Check agent statuses, collect DeepSeek results, query Ghidra for evidence, spawn Terra + Luna reviewers, update manifest, return.
3. **Follow-up 2**: Check reviewer statuses, collect dispositions, spawn final adjudicators, return.
4. **Follow-up 3**: Collect final results, write report, update PROGRESS.md, announce completion.
