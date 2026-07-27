/**
 * decompile-class.ts — Single-class decompilation workflow
 *
 * A persistent PRIMARY actor retains context across work turns. Reviewers remain
 * schema-backed one-shot agents. PARTIAL responses are nudged directly back to
 * the same PRIMARY without paying for an intermediate review.
 *
 * State machine:
 *   review -> approved: stop
 *          -> primary
 *               PARTIAL -> nudge same primary
 *               DONE    -> review
 *               BLOCKED -> block reviewer
 *                            false -> give reason to same primary
 *                            true  -> stop and report validated block
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
// Agent tools
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
const GHIDRA_RO = GHIDRA_RW.filter((tool) => !tool.includes("set_decompiler_comment"));
const MAX_STRUCTURED_OUTPUT_RETRIES = 2;

// ============================================================================
// Prompt construction
// ============================================================================

function taskPrefix(params) {
  return `## Class: ${params.className}` +
    (params.parentClass ? ` (extends ${params.parentClass})` : "") + "\n" +
    (params.vtableAddress ? `Vtable: ${params.vtableAddress}\n` : "") +
    `Ghidra DB: ${params.ghidraDatabase}\n` +
    `AGENTS.md: ${AGENTS_MD}\n` +
    `Build: cd ${DECOMPILED} && make check\n\n` +
    "## Files\n" +
    `- ${DECOMPILED}/${params.headerPath}\n` +
    `- ${DECOMPILED}/${params.implPath}\n` +
    (params.contextFiles || []).map((file) => `- ${DECOMPILED}/${file}\n`).join("") +
    "\n## Functions\n" +
    params.functions.map((fn, index) => {
      let line = `${index + 1}. ${fn.name} @ ${fn.address}`;
      if (fn.vtableSlot !== undefined) line += ` (vtable[${fn.vtableSlot}])`;
      if (fn.description) line += ` — ${fn.description}`;
      return line;
    }).join("\n") + "\n" +
    (params.supervisorGuidance
      ? `\n## Supervisor guidance from a previous scheduling decision\n${params.supervisorGuidance}\n`
      : "");
}

function primaryActorInstructions(params) {
  return `${taskPrefix(params)}
You are the persistent PRIMARY implementer for this one class. Retain context
between messages and continue editing the same files until the work is complete.

Read ${AGENTS_MD} completely before editing. The Ghidra database is already open;
do not open or close it. Verify every target with both decompiler and disassembly.
Follow the complete correctness, completeness, status-tag, and anti-pattern rules.
Run \`cd ${DECOMPILED} && make check\` when you believe the class is ready.

Every response must be ONLY one JSON object with this shape:
{
  "status": "DONE" | "PARTIAL" | "BLOCKED",
  "summary": "work performed and what remains",
  "compilationStatus": "PASS" | "FAIL" | "UNKNOWN",
  "blocks": [{"what":"...", "why":"...", "suggestion":"...", "address":"..."}]
}

Status meanings:
- DONE: you believe the requested target status is ready for independent review.
- PARTIAL: you know what remains and can continue without outside help.
- BLOCKED: progress is impossible without an external dependency or cross-class decision.
  Include at least one precise block. Uncertainty, naming difficulty, or more work is PARTIAL.
No markdown fences or prose outside the JSON object.`;
}

function templateReviewFeedback(review) {
  if (!review || review.approved) return "";
  let text = `## Reviewer found ${(review.issues || []).length} issue(s)\n`;
  text += `Current status: ${review.currentStatus || "UNKNOWN"}.\n\n`;
  for (const severity of ["BLOCKER", "WARNING", "INFO"]) {
    const issues = (review.issues || []).filter((issue) => issue.severity === severity);
    if (issues.length === 0) continue;
    text += `### ${severity} (${issues.length})\n`;
    for (const issue of issues) {
      text += `- **${issue.category}**${issue.file ? ` in \`${issue.file}\`` : ""}` +
        `${issue.line ? ` line ${issue.line}` : ""}\n` +
        `  Problem: ${issue.description}\n  Fix: ${issue.fix}\n`;
    }
    text += "\n";
  }
  text += `### Summary\n${review.summary || "N/A"}\n`;
  return text;
}

function buildPrimaryReviewTask(review) {
  return `Fix every issue from the independent review below. Continue using Ghidra
and the project rules. Return DONE only when ready for another independent review;
return PARTIAL when you can continue yourself.

${templateReviewFeedback(review)}`;
}

function buildPartialNudge(primaryOutput) {
  return `Continue the work. You returned PARTIAL, so no independent review has been
started. Complete the remaining work you already identified, then return the required
JSON object again. Do not repeat the previous summary without making progress.

Previous summary:
${primaryOutput.summary}`;
}

function buildEmptyBlockNudge(primaryOutput) {
  return `Your BLOCKED response contained no concrete block reasons, so it is treated
as PARTIAL. Continue working and return the required JSON object after making progress.
Use BLOCKED only with at least one precise, externally dependent reason.

Previous summary:
${primaryOutput.summary}`;
}

function buildRejectedBlockTask(blockReview, primaryOutput) {
  return `Your BLOCKED claim was independently reviewed and found NOT legitimate.
Continue working; do not stop for this reason again.

Block-reviewer reason: ${blockReview.reason}
${blockReview.suggestion ? `Suggestion: ${blockReview.suggestion}\n` : ""}
Your previous summary: ${primaryOutput.summary}`;
}

function buildReviewerTask(params, buildOutput) {
  let task = `You are a STRICT independent reviewer. Find every defect in ${params.className}.

Read ${AGENTS_MD} completely first. Use read-only Ghidra decompilation and disassembly
to compare every listed function against the original binary.

${taskPrefix(params)}
## Required review
- Enforce all twelve Ghidra anti-pattern rules as blockers.
- Verify control flow, data flow, widths, signedness, calls, side effects, offsets,
  declarations, class hierarchy, address annotations, and status tags.
- approved may be true only when currentStatus exactly reaches ${params.targetStatus || "INTEGRATED"},
  compilation passes, and there are zero blockers.

## Orchestrator build output
\`\`\`
${buildOutput === undefined ? "Build not available" : buildOutput}
\`\`\`
`;
  if (buildOutput === undefined) task += "Mark compilationStatus UNKNOWN.\n";
  return task + "Return structured JSON matching the supplied schema.";
}

function buildBlockReviewerTask(primaryOutput, className) {
  return `The PRIMARY for ${className} claims it cannot continue. Determine only
whether the stated reason is a legitimate reason to STOP THIS CLASS LOOP.

Blocks:
${JSON.stringify(primaryOutput.blocks || [], null, 2)}

Primary summary:
${primaryOutput.summary}

LEGITIMATE means progress actually depends on unavailable external work, a separate
class dependency outside this task, or a cross-cutting architectural decision.
NOT LEGITIMATE means the PRIMARY can use Ghidra, inspect nearby code, choose and
document a reasonable name, or simply perform more work itself.

Return structured JSON matching the supplied schema.`;
}

// ============================================================================
// Structured output helpers
// ============================================================================

const SCHEMA_REQUIREMENTS = {
  REVIEW_SCHEMA: [
    "approved: boolean",
    "currentStatus: PRE_TRANSCRIBED | TRANSCRIBED | VALIDATED | INTEGRATED",
    "summary: string",
    "issues: array of {severity, category, description, fix}",
    "compilationStatus: PASS | FAIL | UNKNOWN",
  ],
  BLOCK_REVIEW_SCHEMA: [
    "legitimate: boolean",
    "reason: string",
    "suggestion?: string",
  ],
};

function describeStructuredRunError(runError) {
  if (runError instanceof Error) return `Error: ${runError.message}`;
  if (runError && runError.error) {
    return `Status: ${runError.status || "unknown"}\nError: ` +
      (typeof runError.error === "string" ? runError.error : JSON.stringify(runError.error));
  }
  return `Status: ${(runError && runError.status) || "unknown"}\n` +
    `Raw text: ${String((runError && runError.text) || "").substring(0, 500)}`;
}

function buildRetryFeedback(runError, schemaName) {
  return `Your previous response did not satisfy ${schemaName}.
${describeStructuredRunError(runError)}
Required fields:
- ${SCHEMA_REQUIREMENTS[schemaName].join("\n- ")}
Return only valid JSON matching the schema, with empty arrays where appropriate.`;
}

async function runStructuredAgentWithRetry(options) {
  const maxRetries = options.maxRetries ?? MAX_STRUCTURED_OUTPUT_RETRIES;
  let lastError = null;
  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    const task = attempt === 0
      ? options.task
      : `${options.task}\n\n${buildRetryFeedback(lastError, options.schemaName)}`;
    let result;
    try {
      result = await agents.run({
        name: `${options.name}-a${attempt}`,
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
    if (attempt < maxRetries) {
      print(`│ [${options.logLabel}] Invalid structured output; retrying (${attempt + 1}/${maxRetries}).\n`);
    }
  }
  throw new Error(`${options.logLabel} failed after ${maxRetries + 1} attempts: ` +
    describeStructuredRunError(lastError));
}

function extractJsonObject(text) {
  if (text && typeof text === "object") return text;
  const raw = String(text || "").trim();
  const fenced = raw.match(/```(?:json)?\s*([\s\S]*?)```/i);
  const candidate = fenced ? fenced[1].trim() : raw;
  try { return JSON.parse(candidate); } catch (_) {}

  const start = candidate.indexOf("{");
  if (start < 0) throw new Error("response contains no JSON object");
  let depth = 0;
  let quoted = false;
  let escaped = false;
  for (let i = start; i < candidate.length; i++) {
    const char = candidate[i];
    if (quoted) {
      if (escaped) escaped = false;
      else if (char === "\\") escaped = true;
      else if (char === '"') quoted = false;
      continue;
    }
    if (char === '"') quoted = true;
    else if (char === "{") depth++;
    else if (char === "}" && --depth === 0) return JSON.parse(candidate.slice(start, i + 1));
  }
  throw new Error("response contains an unterminated JSON object");
}

function validatePrimaryOutput(value) {
  if (!value || typeof value !== "object") return "output is not an object";
  if (!["DONE", "BLOCKED", "PARTIAL"].includes(value.status)) return "invalid status";
  if (typeof value.summary !== "string") return "summary must be a string";
  if (!["PASS", "FAIL", "UNKNOWN"].includes(value.compilationStatus)) return "invalid compilationStatus";
  if (!Array.isArray(value.blocks)) return "blocks must be an array";
  for (const block of value.blocks) {
    if (!block || typeof block.what !== "string" || typeof block.why !== "string") {
      return "every block requires string fields what and why";
    }
  }
  return null;
}

async function askPrimaryWithRetry(primaryId, message, logLabel) {
  let prompt = message;
  let lastError = null;
  for (let attempt = 0; attempt <= MAX_STRUCTURED_OUTPUT_RETRIES; attempt++) {
    let response;
    try {
      response = await agents.ask({ id: primaryId, message: prompt });
      const raw = response.value !== undefined ? response.value : response.text;
      const parsed = extractJsonObject(raw);
      const validationError = validatePrimaryOutput(parsed);
      if (!validationError) return parsed;
      lastError = new Error(validationError);
    } catch (error) {
      lastError = error;
    }
    if (attempt < MAX_STRUCTURED_OUTPUT_RETRIES) {
      print(`│ [${logLabel}] Invalid PRIMARY output; asking same actor to correct it.\n`);
      prompt = `Your previous response was invalid: ${lastError.message}\n` +
        "Return ONLY the required JSON object. Do not perform additional implementation work in this correction turn.";
    }
  }
  throw new Error(`PRIMARY failed structured output after ${MAX_STRUCTURED_OUTPUT_RETRIES + 1} attempts: ${lastError.message}`);
}

async function runReviewer(params, buildOutput, pass) {
  return runStructuredAgentWithRetry({
    name: `review-${params.className.toLowerCase()}-p${pass}`,
    task: buildReviewerTask(params, buildOutput),
    model: params.reviewerModel || "deepseek/deepseek-v4-pro",
    tools: ["read", "grep", "find", "ls", ...GHIDRA_RO],
    schema: REVIEW_SCHEMA,
    schemaName: "REVIEW_SCHEMA",
    logLabel: "REVIEWER",
  });
}

async function runBlockReviewer(primaryOutput, params, primaryTurn) {
  return runStructuredAgentWithRetry({
    name: `block-review-${params.className.toLowerCase()}-t${primaryTurn}`,
    task: buildBlockReviewerTask(primaryOutput, params.className),
    model: params.reviewerModel || "deepseek/deepseek-v4-pro",
    tools: ["read", "grep", "find", "ls", ...GHIDRA_RO],
    schema: BLOCK_REVIEW_SCHEMA,
    schemaName: "BLOCK_REVIEW_SCHEMA",
    logLabel: "BLOCK-REVIEW",
  });
}

function buildPassed(buildOutput) {
  return Boolean(buildOutput && buildOutput.includes("[OK]") && !buildOutput.includes("need work"));
}

// ============================================================================
// Main orchestrator
// ============================================================================

async function decompileClass(params) {
  const maxPrimaryTurns = params.maxIterations || 5;
  const targetStatus = params.targetStatus || "INTEGRATED";
  const primaryModel = params.primaryModel || "deepseek/deepseek-v4-pro";

  print(`\n═══ DECOMPILE: ${params.className} ═══\n`);
  print(`Functions: ${params.functions.length} | Primary turns: ${maxPrimaryTurns} | Target: ${targetStatus} | DB: ${params.ghidraDatabase}\n\n`);

  let primary = null;
  let review = null;
  let reviewPasses = 0;
  let primaryTurns = 0;
  let nextPrimaryMessage = null;
  let needsReview = true;

  try {
    while (primaryTurns < maxPrimaryTurns || needsReview) {
      if (needsReview) {
        reviewPasses++;
        print(`┌─ REVIEW ${reviewPasses} ─────────────────────────────────┐\n`);
        let buildOutput;
        try {
          const build = await pi.bash({ command: `cd ${DECOMPILED} && make check 2>&1`, settle: true });
          buildOutput = build.output || "";
        } catch (error) {
          buildOutput = `Build command failed: ${error.message}`;
        }

        let reviewResult;
        try {
          reviewResult = await runReviewer({ ...params, targetStatus }, buildOutput, reviewPasses);
        } catch (error) {
          return { status: "error", className: params.className, reviewPasses, primaryTurns,
            error: `Reviewer: ${error.message}` };
        }

        review = reviewResult.value;
        const blockers = (review.issues || []).filter((issue) => issue.severity === "BLOCKER");
        const buildOk = buildPassed(buildOutput);
        const actuallyApproved = review.approved === true &&
          review.currentStatus === targetStatus &&
          review.compilationStatus === "PASS" && blockers.length === 0 && buildOk;

        print(`│ Status: ${review.currentStatus} | Model approved: ${review.approved} | Enforced: ${actuallyApproved} | ${blockers.length}B | Build: ${buildOk ? "PASS" : "FAIL"}\n`);
        print("└────────────────────────────────────────────────┘\n");

        if (actuallyApproved) {
          return { status: "approved", className: params.className,
            reviewPasses, primaryTurns, finalReview: review };
        }
        if (primaryTurns >= maxPrimaryTurns) break;

        if (!primary) {
          primary = await agents.create({
            name: `primary-${params.className.toLowerCase()}`,
            instructions: primaryActorInstructions({ ...params, targetStatus }),
            model: primaryModel,
            runner: "pi",
            tools: ["read", "grep", "find", "ls", "edit", "write", "bash", ...GHIDRA_RW],
            delivery: "mailbox",
            responseMode: "text",
          });
          print(`│ [PRIMARY] Persistent actor: ${primary.id}\n`);
        }

        nextPrimaryMessage = buildPrimaryReviewTask(review);
        needsReview = false;
      }

      if (primaryTurns >= maxPrimaryTurns) break;
      primaryTurns++;
      print(`│ [PRIMARY] Turn ${primaryTurns}/${maxPrimaryTurns}\n`);

      let primaryOutput;
      try {
        primaryOutput = await askPrimaryWithRetry(primary.id, nextPrimaryMessage, "PRIMARY");
      } catch (error) {
        return { status: "error", className: params.className, reviewPasses, primaryTurns,
          finalReview: review, error: error.message };
      }
      print(`│ [PRIMARY] ${primaryOutput.status} | Compiles: ${primaryOutput.compilationStatus}\n`);

      if (primaryOutput.status === "DONE") {
        needsReview = true;
        continue;
      }

      if (primaryOutput.status === "PARTIAL") {
        nextPrimaryMessage = buildPartialNudge(primaryOutput);
        continue;
      }

      const blocks = primaryOutput.blocks || [];
      if (blocks.length === 0) {
        print("│ [BLOCKED] Empty block list; treating as PARTIAL.\n");
        nextPrimaryMessage = buildEmptyBlockNudge(primaryOutput);
        continue;
      }

      print("│ [BLOCKED] Validating stop reason...\n");
      let blockReviewResult;
      try {
        blockReviewResult = await runBlockReviewer(primaryOutput, params, primaryTurns);
      } catch (error) {
        // Failure to review is neither legitimate nor illegitimate. Fail closed:
        // never inform the supervisor that an unvalidated block is legitimate.
        return { status: "error", className: params.className, reviewPasses, primaryTurns,
          finalReview: review, error: `Block reviewer: ${error.message}` };
      }

      const blockReview = blockReviewResult.value;
      print(`│ [BLOCK-REVIEW] Legitimate: ${blockReview.legitimate} | ${blockReview.reason.substring(0, 100)}\n`);
      if (blockReview.legitimate) {
        return {
          status: "blocked",
          className: params.className,
          reviewPasses,
          primaryTurns,
          finalReview: review,
          blocks,
          blockReview,
        };
      }

      nextPrimaryMessage = buildRejectedBlockTask(blockReview, primaryOutput);
    }

    return { status: "max_iterations_reached", className: params.className,
      reviewPasses, primaryTurns, finalReview: review };
  } finally {
    if (primary) {
      await agents.remove({ id: primary.id }).catch(() => {});
      print(`Primary actor for ${params.className} cleaned up.\n`);
    }
  }
}

return { decompileClass, extractJsonObject, validatePrimaryOutput };
