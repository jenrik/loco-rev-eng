/**
 * supervisor.ts — Top-level orchestrator for multi-class decompilation
 *
 * Usage (from fabric_exec):
 *   const code = await pi.read('tools/supervisor.ts');
 *   const wrapped = '(async () => { ' + code + ' })()';
 *   const { run } = await eval(wrapped);
 *   return await run({ classes: [...], ghidraDatabase: "loco12", maxParallel: 3 });
 *
 * Architecture:
 *   SUPERVISOR ACTOR (persistent) — answers blocked primaries
 *   ├─ Initial deep review — assesses all files + dependencies
 *   └─ Dispatches → decompileClass() × N via concurrency pool
 *        └─ primary↔reviewer loop with BLOCKED → supervisor routing
 */

const PROJECT = "/home/user/projects/v43/jenrik/lego-loco-rev-eng";
const DECOMPILED = `${PROJECT}/src/decompiled_cpp`;
const AGENTS_MD = `${PROJECT}/AGENTS.md`;
const PROGRESS_MD = `${PROJECT}/PROGRESS.md`;

function buildSupervisorInstructions(classes, ghidraDatabase, isDiscovery) {
  let instructions = `You are the SUPERVISOR for a multi-class decompilation session.

## Your role
You are an ORCHESTRATOR, not a worker. Your job is to:
- Analyze the codebase to discover what needs decompilation work
- Track which classes are running, blocked, or complete
- Answer architectural questions from blocked primary agents
- Resolve naming conflicts and cross-cutting decisions
- Decide what can be dispatched and in what order

You do NOT decompile functions or edit code. You analyze, decide, and direct.

## Your authority
- Rename Ghidra auto-labels (RESDATA_*, FUN_*, DAT_*) when you determine the correct name
- Decide on architectural questions: class hierarchy, file splits, naming conventions
- Resolve naming conflicts across classes
- Determine dispatch order based on dependencies
- Discover classes that need decompilation work

## Tools
- Ghidra (database ${ghidraDatabase}): decompile, disassemble, xrefs — read access
  Use these to UNDERSTAND functions before naming them. Do not modify Ghidra.
- File tools: read, grep, find, ls — to inspect code and inform decisions
- Build: cd ${DECOMPILED} && make check

## How you work
`;

  if (isDiscovery) {
    instructions += `### Discovery mode
You will first analyze the entire codebase to find classes that need work.
Read PROGRESS.md at ${PROGRESS_MD}, check status tags, run make check,
and use Ghidra to identify functions that still need decompilation.

### After discovery
When a primary agent is BLOCKED, you receive a query describing what they need:
1. Use Ghidra and file tools to understand the situation
2. Make a clear decision: provide a name, resolve an architecture question, or
   confirm that the dependency needs to be dispatched as a separate task
3. Respond clearly — your answer will be relayed back when the class is restarted
`;
  } else {
    instructions += `## Target classes
${classes.map(c => `- ${c.className} (${c.headerPath}, ${c.implPath})`).join("\n")}

1. When a primary agent is BLOCKED, you receive a query describing what they need
2. Use Ghidra and file tools to understand the situation
3. Make a clear decision: provide a name, resolve an architecture question, or
   confirm that the dependency needs to be dispatched as a separate task
4. Respond clearly — your answer will be relayed back when the class is restarted
`;
  }

  instructions += `
## Decision guidelines
- Ghidra labels: decompile the function, understand its purpose, name it accordingly
- When in doubt, decompile from Ghidra and describe what it does before naming it
- Prefer descriptive names. If a function checks tile types, name it IsRoadTile or similar.
- If a dependency needs its own decompilation pass, say so — the orchestrator will
  dispatch it as a separate task before restarting the blocked class.

Always read ${AGENTS_MD} before making decisions that affect code standards.`;

  return instructions;
}

// ============================================================================
// Discovery — supervisor finds classes needing work
// ============================================================================

function buildDiscoveryTask(scope) {
  let scopeDesc = "";
  switch (scope) {
    case "below-integrated":
      scopeDesc = "files that are below INTEGRATED status (TRANSCRIBED, VALIDATED, or missing status tag)";
      break;
    case "transcribed":
      scopeDesc = "files at TRANSCRIBED status that need validation";
      break;
    case "validated":
      scopeDesc = "files at VALIDATED status that need integration";
      break;
    case "all":
      scopeDesc = "ALL files including those already INTEGRATED (for re-validation)";
      break;
    default:
      scopeDesc = "files that are below INTEGRATED status (TRANSCRIBED, VALIDATED, or missing status tag)";
  }

  return `## Discovery Task — Find classes that need decompilation work

Analyze the codebase and return a prioritized list of classes that need work.
Focus on: ${scopeDesc}

### Step 1: Read project status
Read ${PROGRESS_MD} completely. Note:
- "Remaining work" section — what's pending
- "Architecture notes" — class hierarchy and key addresses
- "Session log" — recent accomplishments

### Step 2: Find status tags
Run: grep -rn "Status:" ${DECOMPILED} --include="*.cpp" --include="*.h"
This shows which files have explicit status tags (TRANSCRIBED/VALIDATED/INTEGRATED).

Files WITHOUT a status tag predate the tagging system — they need assessment.

### Step 3: Check compilation
Run: cd ${DECOMPILED} && make check
Note which files appear in "KNOWN GOOD" vs "NEEDS WORK".

### Step 4: Read PROGRESS.md priorities
Cross-reference PROGRESS.md "Remaining work" items with files on disk:
- "Implement real subsystem constructors" → which .cpp files have stub constructors?
- "Unbreak remaining C++ files" → which files fail make check?
- "Port remaining native .c files to C++" → which .c files in native/ still need porting?

### Step 5: For each file needing work, determine functions
For files that already have .cpp implementations (needing validation or integration):
- Read the header to find declared methods
- Read the .cpp to find implemented functions and their address annotations
- List ALL functions that need validation/integration

For files that need NEW decompilation (no .cpp or only stubs):
- Use Ghidra to find the class vtable
- Use xrefs to find constructor/destructor and all virtual method implementations
- List functions with their addresses

### Step 6: Prioritize
Order by:
1. Dependencies first (if class A depends on class B being INTEGRATED, do B first)
2. PROGRESS.md priority order (Priority 1 > Priority 2 > ...)
3. Within same priority: TRANSCRIBED → VALIDATED (quick win) before new decompilation

### Step 7: Output structured JSON
Return a JSON object:

\`\`\`json
{
  "summary": "Overall assessment of codebase state",
  "totalFiles": 72,
  "filesNeedingWork": 5,
  "classes": [
    {
      "className": "EditorState",
      "headerPath": "world/EditorState.h",
      "implPath": "world/EditorState.cpp",
      "functions": [
        { "name": "EditorState::EditorState", "address": "0x4XXXXX" },
        { "name": "EditorState::Update", "address": "0x4XXXXX", "vtableSlot": 1 }
      ],
      "parentClass": null,
      "vtableAddress": "0x4XXXXX",
      "contextFiles": ["shared/types.h", "core/Entity.h"],
      "targetStatus": "VALIDATED",
      "priority": 1,
      "reason": "Currently TRANSCRIBED — needs validation",
      "currentStatus": "TRANSCRIBED"
    }
  ]
}
\`\`\`

### Important rules
- Be thorough — check every .cpp and .h file under ${DECOMPILED}
- Only include classes that GENUINELY need work (not already INTEGRATED)
- For class names, use the C++ class name (not the filename)
- For addresses, verify against Ghidra — never guess
- For functions, include the address annotation from the .cpp file's doc comment
- If a file has no address annotations, use Ghidra to find the function address
- Limit to at most 15 classes (focus on highest priority)
- If you cannot determine functions for a class, mark it with an empty functions array and explain why in the reason`;
}

function parseDiscoveryOutput(text) {
  // Try to extract JSON from the response
  let jsonStr = text;

  // Try markdown code block first
  const codeBlock = text.match(/```(?:json)?\s*([\s\S]*?)```/);
  if (codeBlock) {
    jsonStr = codeBlock[1].trim();
  } else {
    // Look for { "summary" or { "classes"
    const jsonMatch = text.match(/\{\s*"(?:summary|classes|totalFiles)"/);
    if (jsonMatch) {
      jsonStr = text.substring(jsonMatch.index);
      // Find matching closing brace
      let depth = 0;
      let end = jsonMatch.index;
      for (let i = jsonMatch.index; i < text.length; i++) {
        if (text[i] === '{') depth++;
        if (text[i] === '}') {
          depth--;
          if (depth === 0) { end = i + 1; break; }
        }
      }
      jsonStr = text.substring(jsonMatch.index, end);
    }
  }

  try {
    return JSON.parse(jsonStr);
  } catch (e) {
    print(`Discovery parse warning: ${e.message}\n`);
    print(`Raw (first 500): ${jsonStr.substring(0, 500)}\n`);
    return null;
  }
}

async function loadModule(filePath) {
  const code = await pi.read(filePath);
  const wrapped = '(async () => { ' + code + ' })()';
  return eval(wrapped);
}

async function run(params) {
  const {
    classes: inputClasses,
    discover = false,
    scope = "below-integrated",
    ghidraDatabase = "loco12",
    maxParallel = 3,
    maxIterations = 5,
    primaryModel = "deepseek/deepseek-v4-pro",
    reviewerModel = "deepseek/deepseek-v4-pro",
    supervisorModel = "deepseek/deepseek-v4-pro",
  } = params;

  // ── Determine mode ──
  const isDiscovery = discover && (!inputClasses || inputClasses.length === 0);

  if (!isDiscovery && (!Array.isArray(inputClasses) || inputClasses.length === 0)) {
    print("supervisor: no classes provided and discover not enabled.\n");
    print("Use { discover: true } for auto-discovery or { classes: [...] } for explicit list.\n");
    return { total: 0, approved: 0, blocked: 0, maxIterationsReached: 0, errors: 0, results: [] };
  }

  const classCount = isDiscovery ? 15 : inputClasses.length;
  const concurrency = Math.max(1, Math.min(maxParallel, classCount));

  print(`\n╔══════════════════════════════════════════════╗\n`);
  if (isDiscovery) {
    print(`║   SUPERVISOR — Auto-discovery mode           ║\n`);
    print(`║   Scope: ${scope.padEnd(33)} ║\n`);
  } else {
    print(`║   SUPERVISOR — ${inputClasses.length} class(es), concurrency ${concurrency}     ║\n`);
  }
  print(`║   DB: ${ghidraDatabase}  |  Max passes: ${maxIterations}                ║\n`);
  print(`╚══════════════════════════════════════════════╝\n\n`);

  // ── Load decompile-class ──
  const { decompileClass } = await loadModule('tools/decompile-class.ts');

  // ── Create supervisor actor ──
  print("Creating supervisor actor...\n");
  const supervisor = await agents.create({
    name: "supervisor",
    instructions: buildSupervisorInstructions(inputClasses || [], ghidraDatabase, isDiscovery),
    model: supervisorModel,
    runner: "pi",
    tools: ["read", "grep", "find", "ls",
            "mcp.ghidra.decompile_function", "mcp.ghidra.disassemble_function",
            "mcp.ghidra.get_xrefs_to", "mcp.ghidra.get_xrefs_from",
            "mcp.ghidra.list_functions", "mcp.ghidra.get_strings",
            "mcp.ghidra.find_code_by_string", "mcp.ghidra.get_structure",
            "mcp.ghidra.list_structures", "mcp.ghidra.list_names",
            "mcp.ghidra.get_database_info", "mcp.ghidra.get_type_info"],
    delivery: "mailbox",
    responseMode: "text",
  });
  print(`Supervisor actor: ${supervisor.id}\n`);

  let orderedClasses = inputClasses || [];

  try {
    if (isDiscovery) {
      // ═══════════════════════════════════════════════
      // DISCOVERY PHASE — supervisor finds classes needing work
      // ═══════════════════════════════════════════════
      print("┌─ DISCOVERY PHASE ────────────────────────────┐\n");
      print("│ Supervisor analyzing codebase...\n");

      const discoveryTask = buildDiscoveryTask(scope);
      const discoveryResponse = await agents.ask({ id: supervisor.id, message: discoveryTask });
      const discoveryText = discoveryResponse.text || discoveryResponse.value || "";

      print(`│ Response: ${discoveryText.length} chars\n`);

      const discovery = parseDiscoveryOutput(discoveryText);

      if (discovery && Array.isArray(discovery.classes) && discovery.classes.length > 0) {
        print(`│ Summary: ${discovery.summary || "N/A"}\n`);
        print(`│ Found ${discovery.classes.length} class(es) needing work\n`);
        print(`│ Total files: ${discovery.totalFiles || "?"}  |  Needing work: ${discovery.filesNeedingWork || "?"}\n`);

        // Sort by priority
        discovery.classes.sort((a, b) => (a.priority || 99) - (b.priority || 99));

        print("│ Priority order:\n");
        for (const c of discovery.classes) {
          print(`│   ${c.priority}. ${c.className} (${c.currentStatus || "no tag"} → ${c.targetStatus || "INTEGRATED"}) — ${c.reason}\n`);
        }

        orderedClasses = discovery.classes;
      } else {
        print("│ WARNING: Could not parse discovery output.\n");
        print("└──────────────────────────────────────────────┘\n\n");
        print("Discovery failed. Returning empty result.\n");
        return { total: 0, approved: 0, blocked: 0, maxIterationsReached: 0, errors: 0,
                 results: [], discoveryRaw: discoveryText.substring(0, 1000) };
      }
      print("└──────────────────────────────────────────────┘\n\n");
    } else {
      // ═══════════════════════════════════════════════
      // REVIEW PHASE — supervisor reviews provided classes
      // ═══════════════════════════════════════════════
      print("┌─ INITIAL REVIEW ─────────────────────────────┐\n");
      const reviewTask = `Review these ${inputClasses.length} classes for cross-cutting concerns:\n\n` +
        inputClasses.map(c => `- ${c.className}: ${c.headerPath}, ${c.implPath} (${c.functions.length} functions)`).join("\n") +
        `\n\nFor each class, read the .h and .cpp. Then identify:\n` +
        `1. Common dependencies: functions/symbols referenced by multiple classes that aren't decompiled yet\n` +
        `2. Naming conflicts: Ghidra auto-labels (RESDATA_*, FUN_*) that appear across classes\n` +
        `3. Priority: which classes should be decompiled first (dependencies first)\n` +
        `Return your analysis as a prioritized work order.`;

      const initialReview = await agents.ask({ id: supervisor.id, message: reviewTask });
      const reviewText = initialReview.text || initialReview.value || "";
      print(`│ Supervisor analyzed ${inputClasses.length} classes`);
      if (reviewText) print(` — ${reviewText.length} chars`);
      print(`\n`);

      // Parse priority from review (best-effort: look for class names in order)
      const priorityOrder = [];
      for (const cls of inputClasses) {
        if (reviewText.includes(cls.className) && !priorityOrder.includes(cls.className)) {
          priorityOrder.push(cls.className);
        }
      }
      for (const cls of inputClasses) {
        if (!priorityOrder.includes(cls.className)) priorityOrder.push(cls.className);
      }

      orderedClasses = priorityOrder.map(name => inputClasses.find(c => c.className === name)).filter(Boolean);

      if (orderedClasses.length > 1 && orderedClasses[0].className !== inputClasses[0].className) {
        print(`│ Priority order: ${orderedClasses.map(c => c.className).join(" → ")}\n`);
      }
      print("└──────────────────────────────────────────────┘\n\n");
    }

    // ── Dispatch concurrency pool ──
    // #32: Supervisor does NOT control whether a loop stops.
    // The block reviewer (in decompileClass) makes that decision.
    // Supervisor only answers questions when asked.
    print(`Dispatching ${orderedClasses.length} class(es) with concurrency ${concurrency}...\n\n`);

    const classConfigs = orderedClasses.map((cls, i) => ({
      className: cls.className,
      headerPath: cls.headerPath,
      implPath: cls.implPath,
      functions: cls.functions,
      ghidraDatabase: cls.ghidraDatabase || `${ghidraDatabase}`,
      parentClass: cls.parentClass,
      vtableAddress: cls.vtableAddress,
      contextFiles: cls.contextFiles || [],
      primaryModel: cls.primaryModel || primaryModel,
      reviewerModel: cls.reviewerModel || reviewerModel,
      maxIterations: cls.maxIterations || maxIterations,
      supervisorId: supervisor.id,
      targetStatus: cls.targetStatus || "INTEGRATED",
    }));

    const total = classConfigs.length;
    const results = new Array(total);
    let nextIndex = 0;
    let activeCount = 0;

    await new Promise((resolve) => {
      function startNext() {
        while (activeCount < concurrency && nextIndex < total) {
          const idx = nextIndex++;
          const cfg = classConfigs[idx];
          activeCount++;

          print(`  ▶ ${cfg.className} (DB: ${cfg.ghidraDatabase}, ${cfg.functions.length} fn(s))\n`);

          decompileClass(cfg).then(
            (value) => {
              const icon = value.status === "approved" ? "✅" : value.status === "blocked" ? "⏸️" : "⚠️";
              print(`${icon} ${cfg.className}: ${value.status}` +
                (value.iterations !== undefined ? ` — ${value.iterations} pass(es)` : "") +
                (value.finalReview ? ` — ${value.finalReview.currentStatus}` : "") + `\n`);
              results[idx] = { className: cfg.className, ...value };
            },
            (reason) => {
              print(`❌ ${cfg.className}: ${reason?.message || reason}\n`);
              results[idx] = { className: cfg.className, status: "error", error: reason?.message || String(reason) };
            }
          ).finally(() => {
            activeCount--;
            if (nextIndex < total) startNext();
            else if (activeCount === 0) resolve();
          });
        }
      }
      startNext();
    });

    // ── Summary ──
    const finished = results.filter(r => r);
    const approved = finished.filter(r => r.status === "approved");
    const blocked = finished.filter(r => r.status === "blocked");
    const maxed = finished.filter(r => r.status === "max_iterations_reached");
    const errors = finished.filter(r => r.status === "error");

    print(`\n╔══════════════════════════════════════════════╗\n`);
    print(`║   SUPERVISOR — Complete                      ║\n`);
    print(`╠══════════════════════════════════════════════╣\n`);
    print(`║ ✅ ${String(approved.length).padStart(2)}  |  ⏸️  ${String(blocked.length).padStart(2)}  |  ⚠️  ${String(maxed.length).padStart(2)}  |  ❌ ${String(errors.length).padStart(2)}  ║\n`);
    print(`╚══════════════════════════════════════════════╝\n`);

    for (const r of approved) print(`  ✅ ${r.className}: ${r.finalReview?.currentStatus || "APPROVED"} (${r.iterations} passes)\n`);
    for (const r of blocked) print(`  ⏸️  ${r.className}: BLOCKED — ${(r.blocks||[]).map(b=>b.what).join("; ")}\n`);
    for (const r of maxed) print(`  ⚠️  ${r.className}: ${r.finalReview?.currentStatus || "?"} — max iterations\n`);
    for (const r of errors) print(`  ❌ ${r.className}: ${r.error}\n`);

    if (isDiscovery && orderedClasses.length === 0) {
      print("  ℹ️  No classes found needing work — codebase may already be fully INTEGRATED.\n");
    }

    return {
      total,
      approved: approved.length,
      blocked: blocked.length,
      maxIterationsReached: maxed.length,
      errors: errors.length,
      results: finished,
    };

  } finally {
    // ── Always clean up the supervisor actor ──
    await agents.remove({ id: supervisor.id }).catch(() => {});
    print("\nSupervisor actor cleaned up.\n");
  }
}

return { run };
