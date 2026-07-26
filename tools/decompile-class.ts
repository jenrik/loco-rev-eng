/**
 * decompile-class.ts — Single-class decompilation with one-shot agents
 *
 * Primary and reviewer are one-shot agents.run() calls with JSON Schema.
 * Every prompt includes full context (taskPrefix) so no persistence is needed.
 *
 * Usage (from fabric_exec):
 *   const code = await pi.read('tools/decompile-class.ts');
 *   const wrapped = '(async () => { ' + code + ' })()';
 *   const { decompileClass } = await eval(wrapped);
 *   return await decompileClass({ className: "Foo", ... });
 *
 * Architecture:
 *   while pass < maxIter:
 *     orchestrator runs make check → passes output to reviewer
 *     REVIEWER (schema)      → if INTEGRATED + zero BLOCKERs → return approved
 *     PRIMARY  (schema)      → status: DONE | BLOCKED | PARTIAL
 *       if BLOCKED:
 *         BLOCK REVIEWER     → validates legitimacy
 *           if LEGITIMATE    → route to supervisor, return { status: "blocked" }
 *                              (supervisor may restart the loop)
 *           if NOT           → tell primary to continue
 */

const PROJECT = "/home/user/projects/v43/jenrik/lego-loco-rev-eng";
const DECOMPILED = `${PROJECT}/src/decompiled_cpp`;
const AGENTS_MD = `${PROJECT}/AGENTS.md`;

// ============================================================================
// Schemas
// ============================================================================

const PRIMARY_SCHEMA = {
  type: "object",
  properties: {
    status: { type: "string", enum: ["DONE", "BLOCKED", "PARTIAL"] },
    summary: { type: "string" },
    compilationStatus: { type: "string", enum: ["PASS", "FAIL", "UNKNOWN"] },
    blocks: {
      type: "array",
      items: {
        type: "object",
        properties: {
          what: { type: "string" },
          why: { type: "string" },
          suggestion: { type: "string" },
          address: { type: "string" },
        },
        required: ["what", "why"],
      },
    },
  },
  required: ["status", "summary", "blocks", "compilationStatus"],
};

const REVIEW_SCHEMA = {
  type: "object",
  properties: {
    approved: { type: "boolean" },
    currentStatus: { type: "string", enum: ["PRE_TRANSCRIBED", "TRANSCRIBED", "VALIDATED", "INTEGRATED"] },
    summary: { type: "string" },
    issues: {
      type: "array",
      items: {
        type: "object",
        properties: {
          severity: { type: "string", enum: ["BLOCKER", "WARNING", "INFO"] },
          category: { type: "string" },
          file: { type: "string" },
          line: { type: "integer" },
          description: { type: "string" },
          fix: { type: "string" },
        },
        required: ["severity", "category", "description", "fix"],
      },
    },
    compilationStatus: { type: "string", enum: ["PASS", "FAIL", "UNKNOWN"] },
  },
  required: ["approved", "currentStatus", "summary", "issues", "compilationStatus"],
};

const BLOCK_REVIEW_SCHEMA = {
  type: "object",
  properties: {
    legitimate: { type: "boolean" },
    reason: { type: "string" },
    suggestion: { type: "string" },
  },
  required: ["legitimate", "reason"],
};

// ============================================================================
// Ghidra tools (no lifecycle management — agents don't open/close databases)
// ============================================================================

const GHIDRA_RW = [
  "mcp.ghidra.decompile_function", "mcp.ghidra.disassemble_function",
  "mcp.ghidra.get_xrefs_to", "mcp.ghidra.get_xrefs_from",
  "mcp.ghidra.list_functions", "mcp.ghidra.get_strings",
  "mcp.ghidra.find_code_by_string", "mcp.ghidra.get_structure",
  "mcp.ghidra.list_structures", "mcp.ghidra.list_names",
  "mcp.ghidra.get_database_info", "mcp.ghidra.get_type_info",
  "mcp.ghidra.set_decompiler_comment",
];

const GHIDRA_RO = GHIDRA_RW.filter(t => !t.includes('set_decompiler_comment'));

// ============================================================================
// Templates
// ============================================================================

function templateReviewFeedback(review) {
  if (!review || review.approved) return "";
  let text = `## Reviewer found ${(review.issues || []).length} issue(s)\n`;
  text += `Current status: ${review.currentStatus || "UNKNOWN"}.\n\n`;
  const blockers = (review.issues || []).filter(i => i.severity === "BLOCKER");
  const warnings = (review.issues || []).filter(i => i.severity === "WARNING");
  if (blockers.length > 0) {
    text += `### BLOCKERS (${blockers.length}):\n\n`;
    for (const b of blockers) {
      text += `- **${b.category}**${b.file ? ` in \`${b.file}\`` : ""}${b.line ? ` line ${b.line}` : ""}\n`;
      text += `  Problem: ${b.description}\n  Fix: ${b.fix}\n\n`;
    }
  }
  if (warnings.length > 0) {
    text += `### WARNINGS (${warnings.length}):\n\n`;
    for (const w of warnings) text += `- **${w.category}**: ${w.description}\n  Fix: ${w.fix}\n\n`;
  }
  text += `### Summary:\n${review.summary || "N/A"}\n`;
  return text;
}

// ============================================================================
// Shared task prefix — every primary prompt gets full context
// ============================================================================

function taskPrefix(params) {
  return `## Class: ${params.className}` +
    (params.parentClass ? ` (extends ${params.parentClass})` : "") + `\n` +
    (params.vtableAddress ? `Vtable: ${params.vtableAddress}\n` : "") +
    `Ghidra DB: ${params.ghidraDatabase}\n` +
    `AGENTS.md: ${AGENTS_MD}\n` +
    `Build: cd ${DECOMPILED} && make check\n\n` +
    `## Files\n` +
    `- ${DECOMPILED}/${params.headerPath}\n` +
    `- ${DECOMPILED}/${params.implPath}\n` +
    (params.contextFiles || []).map(f => `- ${DECOMPILED}/${f}\n`).join("") +
    `\n## Functions\n` +
    params.functions.map((f, i) => {
      let s = `${i + 1}. ${f.name} @ ${f.address}`;
      if (f.vtableSlot !== undefined) s += ` (vtable[${f.vtableSlot}])`;
      if (f.description) s += ` — ${f.description}`;
      return s;
    }).join("\n") + `\n`;
}

// ============================================================================
// Task builders
// ============================================================================

function buildPrimaryTask(params, reviewFeedback, blockOverride) {
  const prefix = taskPrefix(params);

  if (blockOverride) {
    return `${prefix}
## Task
Your previous BLOCKED status was reviewed and determined NOT legitimate.

Reviewer: ${blockOverride.reason}
${blockOverride.suggestion ? `Suggestion: ${blockOverride.suggestion}\n` : ""}
Continue fixing the issues. Do not declare BLOCKED for this again.

${reviewFeedback ? `## Remaining reviewer issues\n${templateReviewFeedback(reviewFeedback)}` : ""}

## Instructions
- Read AGENTS.md at ${AGENTS_MD}
- Use Ghidra (${params.ghidraDatabase}) to verify against disassembly
- Remove ALL 12 anti-patterns per AGENTS.md § "Ghidra decompiler anti-patterns":
  1. No _Ctor/_Dtor free functions → proper constructors/destructors
  2. No ClassName_MethodName free functions → ClassName::MethodName
  3. No scalar deleting destructor free_memory flag
  4. No literal vtable dispatch
  5. No __thiscall/__fastcall on C++ declarations
  6. No Ghidra auto-labels (FUN_, DAT_, RESDATA_, GAMESTATE_, LOCOBITMAP_, PTR_LAB_)
  7. No extern "C" around C++ methods
  8. No void* fields where type is known
  9. No param_N parameter names
  10. No flat struct for types with vtables → use inheritance
  11. No array fields as separate scalars
  12. No void* return on constructors
- Run \`cd ${DECOMPILED} && make check\` after edits
- Respond DONE when complete`;
  }

  if (reviewFeedback && !reviewFeedback.approved) {
    return `${prefix}
## Task
Fix ALL issues the reviewer found.

${templateReviewFeedback(reviewFeedback)}

## Instructions
- Read AGENTS.md at ${AGENTS_MD}
- Remove ALL 12 anti-patterns
- Every function: doc comment with /* 0xADDRESS */
- Every this-relative offset mapped to a named field in a header
- Virtual method calls — NO literal vtable dispatch: (*(void (**)(...))(*(void**)this + N))(...)
- Run \`cd ${DECOMPILED} && make check\` after edits
- Respond DONE when complete, BLOCKED only if genuinely stuck`;
  }

  return `${prefix}
## Task — Initial decompilation
Decompile this class from loco.exe (MSVC 1998 x86) into clean C++.

## Instructions
1. Read AGENTS.md at ${AGENTS_MD} completely
2. For each function, use mcp.ghidra.decompile_function AND mcp.ghidra.disassemble_function
3. Remove ALL 12 anti-patterns (AGENTS.md § "Ghidra decompiler anti-patterns")
4. Every function: doc comment with /* 0xADDRESS */
5. Named fields instead of raw offsets where possible
6. Virtual method calls instead of literal vtable dispatch
7. Tag with appropriate Status (TRANSCRIBED/VALIDATED/INTEGRATED)
8. Run \`cd ${DECOMPILED} && make check\` — fix ALL errors

Respond DONE when complete, BLOCKED only if genuinely stuck on external dependency.`;
}

function buildReviewerTask(params, buildOutput) {
  let task = `You are a STRICT code reviewer. Find EVERY defect in \`${params.className}\`.

## CRITICAL: Read AGENTS.md first — ${AGENTS_MD}

## Ghidra access
You have read-only Ghidra access. Use decompile_function and disassemble_function
to cross-reference against the original assembly.

## Files
- ${DECOMPILED}/${params.headerPath}
- ${DECOMPILED}/${params.implPath}
${(params.contextFiles || []).map(f => `- ${DECOMPILED}/${f}`).join("\n")}

## Anti-patterns — BLOCKER if found
1. _Ctor/_Dtor free functions  2. ClassName_MethodName free functions
3. Scalar deleting destructor  4. Literal vtable dispatch
5. __thiscall/__fastcall  6. Ghidra auto-labels (FUN_, DAT_, RESDATA_, etc.)
7. extern "C" around C++  8. void* where type known
9. param_N names  10. Flat struct instead of inheritance
11. Array fields as scalars  12. Constructor void* return

## Correctness + Completeness
Control flow matches disassembly, data flow preserved, signedness correct,
all basic blocks present, all offsets named, cross-refs consistent.

## Build verification
`;

  if (buildOutput !== undefined) {
    task += `The orchestrator ran \`make check\` for you:\n\`\`\`\n${buildOutput}\n\`\`\`\n`;
    if (buildOutput.includes('[OK]') && !buildOutput.includes('need work')) {
      task += `Compilation: PASS\n\n`;
    } else if (buildOutput.includes('error:') || buildOutput.includes('FAIL')) {
      task += `Compilation: FAIL\n\n`;
    }
  } else {
    task += `Compilation status was not verified by the orchestrator. Mark as UNKNOWN.\n\n`;
  }

  task += `## Status assignment
PRE_TRANSCRIBED → TRANSCRIBED → VALIDATED → INTEGRATED.
approved: true ONLY at INTEGRATED with zero BLOCKERs and compilation PASS.

Return structured JSON.`;

  return task;
}


// ============================================================================
// Structured-agent retry helper
// ============================================================================

const MAX_STRUCTURED_OUTPUT_RETRIES = 2;

const SCHEMA_REQUIREMENTS = {
  PRIMARY_SCHEMA: [
    "- status: DONE | BLOCKED | PARTIAL",
    "- summary: string",
    "- compilationStatus: PASS | FAIL | UNKNOWN",
    "- blocks: array of {what, why, suggestion?, address?}",
  ],
  REVIEW_SCHEMA: [
    "- approved: boolean",
    "- currentStatus: PRE_TRANSCRIBED | TRANSCRIBED | VALIDATED | INTEGRATED",
    "- summary: string",
    "- issues: array of {severity, category, description, fix}",
    "- compilationStatus: PASS | FAIL | UNKNOWN",
  ],
  BLOCK_REVIEW_SCHEMA: [
    "- legitimate: boolean",
    "- reason: string",
    "- suggestion?: string",
  ],
};

function describeStructuredRunError(runError) {
  if (runError instanceof Error) return "Error: " + runError.message;
  if (runError && runError.error) {
    return "Status: " + (runError.status || "unknown") + "\n" +
      "Error: " + (typeof runError.error === "string"
        ? runError.error
        : JSON.stringify(runError.error));
  }
  return "Status: " + ((runError && runError.status) || "unknown") + "\n" +
    "Raw text (first 500 chars): " + String((runError && runError.text) || "").substring(0, 500);
}

function buildRetryFeedback(runError, schemaName) {
  const requirements = SCHEMA_REQUIREMENTS[schemaName];
  if (!requirements) throw new Error("Unknown structured-output schema: " + schemaName);

  return "## ⚠️ Your previous response was invalid\n\n" +
    "The output did not satisfy the required JSON Schema (" + schemaName + ").\n\n" +
    describeStructuredRunError(runError) + "\n\n" +
    "## Required schema fields\n" + requirements.join("\n") + "\n\n" +
    "## Instructions\n" +
    "Produce ONLY valid JSON that matches the schema. No markdown fences or extra text. " +
    "Do not omit required fields; use empty arrays where appropriate.";
}

/**
 * Runs a schema-backed one-shot agent and retries only malformed/invalid output.
 * The original task is repeated so each retry has full context; the retry feedback
 * explains the prior failure and restates the exact schema contract.
 */
async function runStructuredAgentWithRetry(options) {
  const maxRetries = options.maxRetries ?? MAX_STRUCTURED_OUTPUT_RETRIES;
  let lastError = null;

  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    const task = attempt === 0
      ? options.task
      : options.task + "\n\n" + buildRetryFeedback(lastError, options.schemaName);
    let result;

    try {
      result = await agents.run({
        name: options.name + "-a" + attempt,
        task,
        model: options.model,
        runner: "pi",
        tools: options.tools,
        schema: options.schema,
      });
    } catch (error) {
      lastError = error;
    }

    if (result && result.status === "completed" && result.value !== undefined) return result;

    lastError = result || lastError;
    const detail = describeStructuredRunError(lastError).replace(/\n/g, " ");
    if (attempt < maxRetries) {
      print("│ [" + options.logLabel + "] Attempt " + (attempt + 1) +
        " invalid: " + detail.substring(0, 180) + ". Retrying...\n");
      continue;
    }
  }

  throw new Error(options.logLabel + " failed after " + (maxRetries + 1) +
    " attempts: " + describeStructuredRunError(lastError));
}

async function runReviewerWithRetry(params, buildOutput, pass) {
  return runStructuredAgentWithRetry({
    name: "review-" + params.className.toLowerCase() + "-p" + pass,
    task: buildReviewerTask(params, buildOutput),
    model: params.reviewerModel || "deepseek/deepseek-v4-pro",
    tools: ["read", "grep", "find", "ls"].concat(GHIDRA_RO),
    schema: REVIEW_SCHEMA,
    schemaName: "REVIEW_SCHEMA",
    logLabel: "REVIEWER",
  });
}

async function runPrimaryWithRetry(params, primaryTask, pass, primaryModel) {
  return runStructuredAgentWithRetry({
    name: "primary-" + params.className.toLowerCase() + "-p" + pass,
    task: primaryTask,
    model: primaryModel,
    tools: ["read", "grep", "find", "ls", "edit", "write", "bash", ...GHIDRA_RW],
    schema: PRIMARY_SCHEMA,
    schemaName: "PRIMARY_SCHEMA",
    logLabel: "PRIMARY",
  });
}

async function runBlockReviewerWithRetry(primaryOut, className, pass, reviewerModel) {
  const task = buildBlockReviewerTask(primaryOut, className);
  if (!task) return null;

  try {
    return await runStructuredAgentWithRetry({
      name: "block-review-" + className.toLowerCase() + "-p" + pass,
      task,
      model: reviewerModel,
      tools: ["read", "grep", "find", "ls"].concat(GHIDRA_RO),
      schema: BLOCK_REVIEW_SCHEMA,
      schemaName: "BLOCK_REVIEW_SCHEMA",
      logLabel: "BLOCK-REVIEW",
    });
  } catch (error) {
    print("│ [BLOCK-REVIEW] " + error.message + ". Accepting block.\n");
    return null;
  }
}

function buildBlockReviewerTask(primaryOutput, className) {
  const blocks = primaryOutput.blocks || [];
  if (blocks.length === 0) return null;
  return `The primary agent for ${className} claims to be BLOCKED. Verify if legitimate.

## Primary's blocks:
${JSON.stringify(blocks, null, 2)}

## Primary's summary:
${primaryOutput.summary}

## Criteria
- LEGITIMATE: progress is IMPOSSIBLE because the work depends on something external
  that does not exist yet. This means:
  - A function from another class that hasn't been decompiled yet, AND the primary
    cannot decompile it because it belongs to a different class's scope
  - A cross-cutting architecture decision (class hierarchy, file split, naming convention)
    that must be consistent across multiple classes
  - A type or field whose definition lives in a dependency that isn't decompiled
- NOT LEGITIMATE (primary can and should handle this itself):
  - The primary has Ghidra access and can decompile the blocking function itself
  - The primary can grep the codebase for similar naming patterns and make a choice
  - The primary can make a reasonable decision and document it with a // NOTE: comment
  - The primary is unsure about a name — it should pick one and move on

Return structured JSON: { legitimate, reason, suggestion? }`;
}

// ============================================================================
// Main orchestrator
// ============================================================================

async function decompileClass(params) {
  const maxIter = params.maxIterations || 5;
  const primaryModel = params.primaryModel || "deepseek/deepseek-v4-pro";
  const reviewerModel = params.reviewerModel || "deepseek/deepseek-v4-pro";
  const supervisorId = params.supervisorId || null;
  const targetStatus = params.targetStatus || "INTEGRATED";

  print(`\n═══ DECOMPILE: ${params.className} ═══\n`);
  print(`Functions: ${params.functions.length}  |  Passes: ${maxIter}  |  Target: ${targetStatus}  |  DB: ${params.ghidraDatabase}\n\n`);

  let review = null;
  let blockOverride = null;
  let pass = 0;

  while (pass < maxIter) {
    pass++;
    print(`┌─ PASS ${pass}/${maxIter} ───────────────────────────────┐\n`);

    // ── Run make check (orchestrator-owned, not agent-reported) ──
    let buildOutput = undefined;
    try {
      const build = await pi.bash({ command: `cd ${DECOMPILED} && make check 2>&1`, settle: true });
      buildOutput = build.output || "";
    } catch (e) {
      buildOutput = `Build command failed: ${e.message}`;
    }

    // ── REVIEWER (with retry for schema violations) ──
    print(`│ [REVIEWER] Checking...\n`);
    let revResult;
    try {
      revResult = await runReviewerWithRetry(params, buildOutput, pass);
    } catch (e) {
      return { status: "error", className: params.className, pass, error: `Reviewer: ${e.message}` };
    }

    review = revResult.value;
    const blockers = (review.issues || []).filter(i => i.severity === "BLOCKER");
    const buildOk = buildOutput && buildOutput.includes('[OK]') && !buildOutput.includes('need work');

    // ── Enforce approval invariants in TypeScript ──
    const actuallyApproved =
      review.approved === true &&
      review.currentStatus === targetStatus &&
      review.compilationStatus === "PASS" &&
      blockers.length === 0 &&
      buildOk;

    print(`│ Status: ${review.currentStatus}  |  Approved: ${review.approved}  |  Enforced: ${actuallyApproved}  |  ${blockers.length}B  |  Build: ${buildOk ? "PASS" : "FAIL"}\n`);
    for (const b of blockers.slice(0, 3)) print(`│   BLOCKER: ${b.category} — ${b.description.substring(0, 70)}\n`);
    print(`└──────────────────────────────────────────────┘\n`);

    if (actuallyApproved) {
      print(`\n✅ APPROVED at ${review.currentStatus} after ${pass} pass(es)!\n`);
      return { status: "approved", className: params.className, iterations: pass, finalReview: review };
    }

    // ── PRIMARY ──
    const primaryTask = buildPrimaryTask(params, review, blockOverride);
    blockOverride = null;

    print(`│ [PRIMARY] Working...\n`);
    let primaryResult;
    try {
      primaryResult = await runPrimaryWithRetry(params, primaryTask, pass, primaryModel);
    } catch (e) {
      return { status: "error", className: params.className, pass, error: `Primary: ${e.message}` };
    }

    const primaryOut = primaryResult.value;
    print(`│ [PRIMARY] Status: ${primaryOut.status}  |  Compiles: ${primaryOut.compilationStatus}\n`);

    // ── Handle BLOCKED ──
    if (primaryOut.status === "BLOCKED") {
      const blocks = primaryOut.blocks || [];

      if (blocks.length === 0) {
        print(`│ [BLOCKED] Empty blocks array — treating as PARTIAL, continuing.\n`);
        print(`\n`);
        continue;
      }

      print(`│ [BLOCKED] Primary claims stuck. Running block reviewer...\n`);

      let blockReview;
      try {
        blockReview = await runBlockReviewerWithRetry(primaryOut, params.className, pass, reviewerModel);
      } catch (e) {
        print(`│ [BLOCK-REVIEW] Unexpected throw: ${e.message}. Accepting block.\n`);
        return { status: "blocked", className: params.className, iterations: pass, finalReview: review, blocks };
      }
      if (blockReview && blockReview.status === "completed" && blockReview.value) {
        const br = blockReview.value;
        print(`│ [BLOCK-REVIEW] Legitimate: ${br.legitimate}  |  ${br.reason?.substring(0, 80)}\n`);

        if (br.legitimate) {
          // Route to supervisor for resolution, then end loop.
          // Supervisor may restart with a fresh decompileClass() call.
          let supervisorDecisions = null;
          if (supervisorId) {
            try {
              const query = `Primary for ${params.className} is blocked:\n\n` +
                blocks.map(b => `**What:** ${b.what}\n**Why:** ${b.why}\n` +
                  (b.suggestion ? `**Suggestion:** ${b.suggestion}\n` : "") +
                  (b.address ? `**Address:** ${b.address}\n` : "")).join("\n");
              print(`│ [SUPERVISOR] Routing ${blocks.length} decision(s)...\n`);
              const supResponse = await agents.ask({ id: supervisorId, message: query });
              supervisorDecisions = supResponse.value || supResponse.text;
              print(`│ [SUPERVISOR] Responded — ${String(supervisorDecisions).length} chars\n`);
            } catch (e) {
              print(`│ [SUPERVISOR] Unavailable: ${e.message}\n`);
            }
          }

          print(`│ ⏸️  BLOCKED — loop ended. Supervisor may restart.\n`);
          return {
            status: "blocked",
            className: params.className,
            iterations: pass,
            finalReview: review,
            blocks,
            supervisorDecisions,
            blockReview: br,
          };
        } else {
          print(`│ [BLOCK-REVIEW] Block rejected. Primary must continue.\n`);
          blockOverride = br;
        }
      } else {
        print(`│ [BLOCK-REVIEW] Failed. Accepting block.\n`);
        return { status: "blocked", className: params.className, iterations: pass, finalReview: review, blocks };
      }
    }

    print(`\n`);
  }

  print(`\n❌ MAX ITERATIONS (${maxIter}) reached at ${review?.currentStatus || "?"}.\n`);
  return { status: "max_iterations_reached", className: params.className, iterations: maxIter, finalReview: review };
}

return { decompileClass };
