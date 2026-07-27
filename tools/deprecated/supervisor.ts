/**
 * supervisor.ts — Incremental, dependency-aware decompilation scheduler
 *
 * The persistent supervisor never emits a complete work queue. At each settled
 * scheduling boundary it receives the current running set, newly completed work,
 * validated legitimate blocks, and free capacity. It then authorizes only the
 * work that is safe to start now. TypeScript executes those launch directives.
 */

const PROJECT = "/home/user/projects/v43/jenrik/lego-loco-rev-eng";
const DECOMPILED = `${PROJECT}/src/decompiled_cpp`;
const AGENTS_MD = `${PROJECT}/AGENTS.md`;
const PROGRESS_MD = `${PROJECT}/PROGRESS.md`;
const MAX_SUPERVISOR_OUTPUT_RETRIES = 2;

const SUPERVISOR_TOOLS = [
  "read", "grep", "find", "ls", "bash",
  "mcp.ghidra.decompile_function", "mcp.ghidra.disassemble_function",
  "mcp.ghidra.get_xrefs_to", "mcp.ghidra.get_xrefs_from",
  "mcp.ghidra.list_functions", "mcp.ghidra.get_strings",
  "mcp.ghidra.find_code_by_string", "mcp.ghidra.get_structure",
  "mcp.ghidra.list_structures", "mcp.ghidra.list_names",
  "mcp.ghidra.get_database_info", "mcp.ghidra.get_type_info",
];

function buildSupervisorInstructions(params, isDiscovery) {
  const explicitTargets = isDiscovery ? "" : `
## Allowed explicit targets
${JSON.stringify(params.classes || [], null, 2)}
Only schedule classes from this list. A START entry may contain only className,
retry, and guidance; the host will merge the canonical configuration.`;

  const directionInstructions = isDiscovery && params.direction ? `

## Discovery direction — primary session objective
${params.direction}

Treat this direction as the optimization target for discovery and scheduling:
1. Translate it into concrete, observable success criteria.
2. Trace backward from that capability through callers, constructors, globals,
   runtime paths, and class dependencies.
3. Prioritize the smallest dependency cone that advances those criteria.
4. Defer unrelated below-status cleanup, even if it would otherwise be a quick win.
5. Re-evaluate the dependency cone after every completion or legitimate block.
Do not claim COMPLETE until the directed capability is working or no further work
can be identified; explain which condition applies in the COMPLETE reason.` : "";

  const discoveryInstructions = isDiscovery ? `
## Discovery mode
Discover work incrementally within scope "${params.scope}". Scope semantics:
- below-integrated: TRANSCRIBED, VALIDATED, or missing status
- transcribed: only TRANSCRIBED work needing validation
- validated: only VALIDATED work needing integration
- all: include INTEGRATED work when the direction requires re-validation

Read ${PROGRESS_MD} completely, inspect status tags and untagged files, run
\`cd ${DECOMPILED} && make check\`, and use Ghidra to verify every function address.
Do not build a complete queue. At each turn identify only enough safe work to use
the currently available capacity. Never guess an address.${directionInstructions}` : "";

  return `You are the persistent SUPERVISOR and scheduling authority for a multi-class
Lego Loco decompilation session. Read ${AGENTS_MD} completely.

You are an orchestrator, not an implementer. Do not edit code and do not decompile
whole functions as implementation work. Inspect files and Ghidra only to understand
dependencies, validate names and addresses, and decide what can safely run together.

Ghidra database: ${params.ghidraDatabase}
Maximum concurrency: ${params.maxParallel}
${discoveryInstructions}${explicitTargets}

## Scheduling protocol
Every message contains:
- running: class attempts still in flight
- justCompleted: completions not shown in an earlier scheduling turn
- knownOutcomes: latest outcome for every attempted class
- availableSlots: hard launch limit for this turn
- notice: rejected or stale prior decisions, when applicable

A blocked completion appears only after a block reviewer explicitly determined that
the class had a legitimate reason to stop. Use that dependency information for
future scheduling. Do not send advice back into the stopped class loop. You may:
- schedule the missing dependency;
- avoid other work with the same dependency;
- later schedule a fresh retry with retry:true and concrete guidance after the
  prerequisite has completed.

Return ONLY one JSON object:
{
  "action": "START" | "WAIT" | "COMPLETE",
  "summary": "current scheduling assessment",
  "reason": "why this action is safe",
  "starts": [
    {
      "className": "ClassName",
      "headerPath": "game/ClassName.h",
      "implPath": "game/ClassName.cpp",
      "functions": [{"name":"ClassName::Method", "address":"0x...", "vtableSlot":0}],
      "parentClass": "OptionalBase",
      "vtableAddress": "0x...",
      "contextFiles": [],
      "targetStatus": "INTEGRATED",
      "retry": false,
      "guidance": "optional scheduling or dependency guidance"
    }
  ]
}

Rules:
- START requires one or more starts and may not exceed availableSlots.
- WAIT means starts must be empty and is valid only while work is running or a
  concrete prerequisite is expected.
- COMPLETE means starts must be empty and no further work in scope remains.
- Never schedule two attempts of the same class concurrently.
- New discovery targets require headerPath, implPath, and verified functions.
- Respect dependency order and avoid concurrent edits to shared files.
- Do not use markdown fences or text outside the JSON object.`;
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

function validateDirective(value) {
  if (!value || typeof value !== "object") return "directive is not an object";
  if (!["START", "WAIT", "COMPLETE"].includes(value.action)) return "invalid action";
  if (typeof value.summary !== "string") return "summary must be a string";
  if (typeof value.reason !== "string") return "reason must be a string";
  if (!Array.isArray(value.starts)) return "starts must be an array";
  if (value.action === "START" && value.starts.length === 0) return "START requires at least one start";
  if (value.action !== "START" && value.starts.length !== 0) return `${value.action} requires an empty starts array`;
  for (const start of value.starts) {
    if (!start || typeof start.className !== "string" || start.className.length === 0) {
      return "every start requires className";
    }
    if (start.functions !== undefined && !Array.isArray(start.functions)) return "functions must be an array";
    if (start.contextFiles !== undefined && !Array.isArray(start.contextFiles)) return "contextFiles must be an array";
  }
  return null;
}

async function askSupervisorWithRetry(supervisorId, message) {
  let prompt = message;
  let lastError = null;
  for (let attempt = 0; attempt <= MAX_SUPERVISOR_OUTPUT_RETRIES; attempt++) {
    let response;
    try {
      response = await agents.ask({ id: supervisorId, message: prompt });
      const raw = response.value !== undefined ? response.value : response.text;
      const directive = extractJsonObject(raw);
      const validationError = validateDirective(directive);
      if (!validationError) return directive;
      lastError = new Error(validationError);
    } catch (error) {
      lastError = error;
    }
    if (attempt < MAX_SUPERVISOR_OUTPUT_RETRIES) {
      print(`│ [SUPERVISOR] Invalid directive; asking same settled actor to correct it.\n`);
      prompt = `Your previous scheduling response was invalid: ${lastError.message}\n` +
        "Return only a corrected scheduling JSON object. Do not perform additional discovery in this correction turn.";
    }
  }
  throw new Error(`Supervisor failed structured output after ${MAX_SUPERVISOR_OUTPUT_RETRIES + 1} attempts: ${lastError.message}`);
}

function compactResult(result) {
  return {
    className: result.className,
    status: result.status,
    currentStatus: result.finalReview && result.finalReview.currentStatus,
    primaryTurns: result.primaryTurns,
    reviewPasses: result.reviewPasses,
    blocks: result.blocks || [],
    blockReviewReason: result.blockReview && result.blockReview.reason,
    error: result.error,
  };
}

function buildSchedulingMessage(state) {
  return `## Scheduling boundary ${state.turn}

The previous supervisor activation has settled. This is a fresh, coalesced state
snapshot; no message was steered into an ongoing activation.

${JSON.stringify({
    direction: state.direction || null,
    running: state.running,
    justCompleted: state.justCompleted,
    knownOutcomes: state.knownOutcomes,
    availableSlots: state.availableSlots,
    notice: state.notice || null,
  }, null, 2)}

Choose what is safe to start now. Return only the scheduling JSON object.`;
}

async function loadDecompileClass(projectRoot = PROJECT) {
  const code = await pi.read(`${projectRoot}/tools/decompile-class.ts`);
  const wrapped = "(async () => { " + code + " })()";
  const module = await eval(wrapped);
  return module.decompileClass;
}

async function loadWorkflowCore(projectRoot = PROJECT) {
  const code = await pi.read(`${projectRoot}/tools/workflow-core.ts`);
  const wrapped = "(async () => { " + code + " })()";
  return await eval(wrapped);
}

function isSafeDecompiledPath(path) {
  return typeof path === "string" && path.length > 0 &&
    !path.startsWith("/") && !path.split("/").includes("..");
}

function normalizeConfig(start, params, isDiscovery) {
  let config = start;
  if (!isDiscovery) {
    const canonical = (params.classes || []).find((candidate) => candidate.className === start.className);
    if (!canonical) return { error: `unknown explicit class ${start.className}` };
    // The supervisor may select an explicit target and attach retry guidance, but
    // it must not redirect a canonical class to unrelated source files/functions.
    config = { ...canonical, retry: start.retry === true, guidance: start.guidance };
  }

  if (!isSafeDecompiledPath(config.headerPath) || !isSafeDecompiledPath(config.implPath)) {
    return { error: `${config.className} has an unsafe headerPath or implPath` };
  }
  if (!config.headerPath || !config.implPath) {
    return { error: `${config.className} is missing headerPath or implPath` };
  }
  if (!Array.isArray(config.functions) || config.functions.length === 0) {
    return { error: `${config.className} has no verified function targets` };
  }
  for (const fn of config.functions) {
    if (!fn || typeof fn.name !== "string" || typeof fn.address !== "string") {
      return { error: `${config.className} has an invalid function target` };
    }
  }

  return {
    config: {
      className: config.className,
      headerPath: config.headerPath,
      implPath: config.implPath,
      functions: config.functions,
      ghidraDatabase: config.ghidraDatabase || params.ghidraDatabase,
      parentClass: config.parentClass,
      vtableAddress: config.vtableAddress,
      contextFiles: config.contextFiles || [],
      primaryModel: config.primaryModel || params.primaryModel,
      reviewerModel: config.reviewerModel || params.reviewerModel,
      maxIterations: config.maxIterations || params.maxIterations,
      targetStatus: config.targetStatus || "INTEGRATED",
      supervisorGuidance: config.guidance || start.guidance,
    },
  };
}

function summarizeFinalResults(attemptResults) {
  const latest = new Map();
  for (const result of attemptResults) latest.set(result.className, result);
  const results = Array.from(latest.values());
  return {
    total: results.length,
    approved: results.filter((result) => result.status === "approved").length,
    blocked: results.filter((result) => result.status === "blocked").length,
    maxIterationsReached: results.filter((result) => result.status === "max_iterations_reached").length,
    errors: results.filter((result) => result.status === "error").length,
    results,
    attempts: attemptResults,
  };
}

async function run(inputParams) {
  const projectRoot = inputParams.projectRoot || PROJECT;
  const params = {
    classes: inputParams.classes,
    discover: inputParams.discover || false,
    scope: inputParams.scope || "below-integrated",
    direction: typeof inputParams.direction === "string" && inputParams.direction.trim()
      ? inputParams.direction.trim()
      : null,
    ghidraDatabase: inputParams.ghidraDatabase || "loco",
    maxParallel: inputParams.maxParallel || 3,
    maxIterations: inputParams.maxIterations || 5,
    maxSupervisorTurns: inputParams.maxSupervisorTurns || 100,
    maxAttemptsPerClass: inputParams.maxAttemptsPerClass || 3,
    primaryModel: inputParams.primaryModel || "deepseek/deepseek-v4-pro",
    reviewerModel: inputParams.reviewerModel || "deepseek/deepseek-v4-pro",
    supervisorModel: inputParams.supervisorModel || "deepseek/deepseek-v4-pro",
    projectRoot,
    workflowStatePath: inputParams.workflowStatePath === false
      ? null
      : (inputParams.workflowStatePath || `${projectRoot}/.pi/workflow/decompilation-state.json`),
    workflowCorePath: inputParams.workflowCorePath || `${projectRoot}/tools/workflow_core.py`,
    workflowBinary: inputParams.workflowBinary || {},
  };

  const isDiscovery = params.discover && (!params.classes || params.classes.length === 0);
  if (!isDiscovery && (!Array.isArray(params.classes) || params.classes.length === 0)) {
    return { total: 0, approved: 0, blocked: 0, maxIterationsReached: 0, errors: 0,
      results: [], attempts: [] };
  }

  const concurrency = Math.max(1, params.maxParallel);
  params.maxParallel = concurrency;
  const workflow = params.workflowStatePath ? await loadWorkflowCore(params.projectRoot) : null;
  const workflowOptions = workflow ? {
    statePath: params.workflowStatePath,
    corePath: params.workflowCorePath,
  } : null;
  if (workflow) {
    await workflow.workflowCoreCall("init", { binary: params.workflowBinary }, workflowOptions);
  }
  const decompileClass = await loadDecompileClass(params.projectRoot);

  print(`\n╔══════════════════════════════════════════════╗\n`);
  print(`║ SUPERVISOR — incremental scheduler           ║\n`);
  print(`║ Mode: ${(isDiscovery ? "discover" : "explicit").padEnd(12)} | Concurrency: ${String(concurrency).padEnd(3)}      ║\n`);
  print(`╚══════════════════════════════════════════════╝\n`);
  if (params.direction) print(`Direction: ${params.direction}\n`);
  print("\n");

  const supervisor = await agents.create({
    name: "decompilation-supervisor",
    instructions: buildSupervisorInstructions(params, isDiscovery),
    model: params.supervisorModel,
    runner: "pi",
    tools: SUPERVISOR_TOOLS,
    delivery: "mailbox",
    responseMode: "text",
  });
  print(`Supervisor actor: ${supervisor.id}\n`);

  const active = new Map();
  const attemptResults = [];
  const latestOutcome = new Map();
  const attemptsByClass = new Map();
  let pendingCompletions = [];
  let stateRevision = 0;
  let schedulingTurn = 0;
  let notice = null;
  let idleDecisions = 0;
  let nextJobId = 1;

  function drainSettled() {
    for (const [jobId, record] of Array.from(active.entries())) {
      if (!record.settled) continue;
      active.delete(jobId);
      attemptResults.push(record.result);
      latestOutcome.set(record.className, record.result);
      pendingCompletions.push(compactResult(record.result));
      const icon = record.result.status === "approved" ? "✅" :
        record.result.status === "blocked" ? "⏸️" : "⚠️";
      print(`${icon} ${record.className} attempt ${record.attempt}: ${record.result.status}\n`);
    }
  }

  async function recordWorkflowOutcome(taskId, result, before) {
    if (!workflow) return result;
    try {
      const after = await workflow.sourceFingerprint(params.projectRoot);
      const writeAudit = await workflow.workflowCoreCall("validate-write-set", {
        taskId, before, after,
      }, workflowOptions);
      if (writeAudit.unexpected.length > 0) {
        await workflow.workflowCoreCall("defer", {
          taskId,
          status: "deferred",
          reason: `Unexpected source edits: ${writeAudit.unexpected.join(", ")}`,
          nextAction: "Review and split unexpected edits into declared work items.",
          blockedBy: [],
          evidenceRefs: [],
          retryWhen: "The write set is corrected or the shared work is explicitly declared.",
        }, workflowOptions);
        return {
          ...result,
          status: "error",
          implementationStatus: result.status,
          error: `Unexpected write-set changes: ${writeAudit.unexpected.join(", ")}`,
          writeAudit,
          workflowTaskId: taskId,
        };
      }

      if (result.status === "approved") {
        await workflow.workflowCoreCall("transition", {
          taskId, status: "integrated", reason: "Independent reviewer approved the target status.",
        }, workflowOptions);
      } else if (result.status === "blocked") {
        const firstBlock = (result.blocks || [])[0] || {};
        await workflow.workflowCoreCall("defer", {
          taskId,
          status: "blocked",
          reason: result.blockReview && result.blockReview.reason || firstBlock.why || "Validated external dependency.",
          nextAction: firstBlock.suggestion || "Create a concrete prerequisite or investigation task.",
          blockedBy: [],
          evidenceRefs: firstBlock.address ? [firstBlock.address] : [],
          retryWhen: "The recorded prerequisite or investigation produces new evidence.",
        }, workflowOptions);
      } else {
        await workflow.workflowCoreCall("defer", {
          taskId,
          status: "deferred",
          reason: result.error || result.status || "Attempt did not reach approval.",
          nextAction: "Resume from the recorded review and PRIMARY output.",
          blockedBy: [],
          evidenceRefs: [],
          retryWhen: "A subsequent supervised attempt is scheduled.",
        }, workflowOptions);
      }
      return { ...result, writeAudit, workflowTaskId: taskId };
    } catch (error) {
      return {
        ...result,
        status: "error",
        implementationStatus: result.status,
        error: `Workflow ledger failure: ${error.message || String(error)}`,
        workflowTaskId: taskId,
      };
    }
  }

  async function launch(config) {
    const className = config.className;
    const attempt = (attemptsByClass.get(className) || 0) + 1;
    attemptsByClass.set(className, attempt);
    const jobId = nextJobId++;
    const record = { jobId, className, attempt, config, settled: false, result: null, promise: null };
    let taskId = null;
    let before = null;
    if (workflow) {
      taskId = workflow.workflowTaskId(config);
      await workflow.workflowCoreCall("upsert-task", workflow.workflowTaskPayload(config, { taskId }), workflowOptions);
      await workflow.workflowCoreCall("transition", {
        taskId, status: "active", reason: `Supervisor started attempt ${attempt}.`,
      }, workflowOptions);
      before = await workflow.sourceFingerprint(params.projectRoot);
    }
    record.promise = (async () => {
      let result;
      try {
        const value = await decompileClass(config);
        result = { className, attempt, ...value };
      } catch (reason) {
        result = { className, attempt, status: "error", error: reason && reason.message || String(reason) };
      }
      if (workflow) result = await recordWorkflowOutcome(taskId, result, before);
      record.result = result;
      record.settled = true;
      stateRevision++;
      return result;
    })();
    active.set(jobId, record);
    print(`▶ ${className} attempt ${attempt} (${config.functions.length} functions)\n`);
  }

  function runningSnapshot() {
    return Array.from(active.values()).filter((record) => !record.settled).map((record) => ({
      className: record.className,
      attempt: record.attempt,
      targetStatus: record.config.targetStatus,
    }));
  }

  function hasActiveClass(className) {
    return Array.from(active.values()).some((record) => !record.settled && record.className === className);
  }

  try {
    while (schedulingTurn < params.maxSupervisorTurns) {
      drainSettled();

      if (runningSnapshot().length >= concurrency) {
        const promises = Array.from(active.values()).filter((record) => !record.settled).map((record) => record.promise);
        if (promises.length > 0) await Promise.race(promises);
        continue;
      }

      schedulingTurn++;
      const deliveredCompletions = pendingCompletions;
      pendingCompletions = [];
      const revisionAtStart = stateRevision;
      const running = runningSnapshot();
      const schedulingMessage = buildSchedulingMessage({
        turn: schedulingTurn,
        direction: params.direction,
        running,
        justCompleted: deliveredCompletions,
        knownOutcomes: Array.from(latestOutcome.values()).map(compactResult),
        availableSlots: concurrency - running.length,
        notice,
      });
      notice = null;

      print(`│ [SUPERVISOR] Turn ${schedulingTurn}: ${running.length} running, ${deliveredCompletions.length} completed, ${concurrency - running.length} slots\n`);
      const directive = await askSupervisorWithRetry(supervisor.id, schedulingMessage);

      // Completions may have arrived while the supervisor was working. The actor is
      // now settled, so buffer them and send a fresh snapshot before launching from
      // a stale capacity/dependency decision.
      drainSettled();
      if (stateRevision !== revisionAtStart) {
        notice = `The previous ${directive.action} decision was discarded because running work completed during that supervisor turn. Re-evaluate against this fresh state.`;
        print("│ [SUPERVISOR] State changed during turn; decision deferred until fresh snapshot.\n");
        continue;
      }

      print(`│ [SUPERVISOR] ${directive.action}: ${directive.reason}\n`);

      if (directive.action === "COMPLETE") {
        if (runningSnapshot().length === 0) {
          print("│ [SUPERVISOR] Session declared complete.\n");
          break;
        }
        notice = "COMPLETE was ignored because work is still running.";
        continue;
      }

      if (directive.action === "WAIT") {
        const promises = Array.from(active.values()).filter((record) => !record.settled).map((record) => record.promise);
        if (promises.length > 0) {
          idleDecisions = 0;
          await Promise.race(promises);
          continue;
        }
        idleDecisions++;
        notice = "WAIT is not actionable because no class is running. Return START or COMPLETE.";
        if (idleDecisions >= 3) throw new Error("Supervisor repeatedly returned WAIT with no running work");
        continue;
      }

      const availableSlots = concurrency - runningSnapshot().length;
      let launched = 0;
      const rejected = [];
      for (const start of directive.starts.slice(0, availableSlots)) {
        if (hasActiveClass(start.className)) {
          rejected.push(`${start.className}: already running`);
          continue;
        }
        const prior = latestOutcome.get(start.className);
        if (prior && !start.retry) {
          rejected.push(`${start.className}: already completed; retry:true is required`);
          continue;
        }
        const attemptCount = attemptsByClass.get(start.className) || 0;
        if (attemptCount >= params.maxAttemptsPerClass) {
          rejected.push(`${start.className}: maximum ${params.maxAttemptsPerClass} attempts reached`);
          continue;
        }
        const normalized = normalizeConfig(start, params, isDiscovery);
        if (normalized.error) {
          rejected.push(normalized.error);
          continue;
        }
        await launch(normalized.config);
        launched++;
      }

      if (directive.starts.length > availableSlots) {
        rejected.push(`${directive.starts.length - availableSlots} start(s) exceeded available capacity`);
      }
      notice = rejected.length > 0 ? `Rejected launch directives: ${rejected.join("; ")}` : null;

      if (launched === 0 && runningSnapshot().length === 0) {
        idleDecisions++;
        if (idleDecisions >= 3) throw new Error("Supervisor produced no actionable work for three settled turns");
      } else {
        idleDecisions = 0;
      }
    }

    if (schedulingTurn >= params.maxSupervisorTurns) {
      throw new Error(`Supervisor exceeded ${params.maxSupervisorTurns} scheduling turns`);
    }

    drainSettled();
    const summary = summarizeFinalResults(attemptResults);
    summary.direction = params.direction;
    print(`\nSupervisor complete: ${summary.approved} approved, ${summary.blocked} blocked, ` +
      `${summary.maxIterationsReached} maxed, ${summary.errors} errors.\n`);
    return summary;
  } finally {
    // Direct decompileClass calls cannot be abandoned safely because each owns a
    // persistent PRIMARY actor. Let any in-flight calls finish before actor cleanup.
    const outstanding = Array.from(active.values()).map((record) => record.promise);
    if (outstanding.length > 0) await Promise.all(outstanding).catch(() => {});
    await agents.remove({ id: supervisor.id }).catch(() => {});
    print("Supervisor actor cleaned up.\n");
  }
}

return {
  run,
  extractJsonObject,
  validateDirective,
  normalizeConfig,
  compactResult,
  buildSupervisorInstructions,
  buildSchedulingMessage,
  normalizeConfig,
};
