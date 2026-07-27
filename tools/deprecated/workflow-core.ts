/**
 * workflow-core.ts — Pi-facing bridge to the durable Python workflow core.
 *
 * Keep Pi/agent/Ghidra behavior in TypeScript. The Python CLI owns only
 * validated, atomic ledger transitions and never calls tools itself.
 */

const WORKFLOW_PROJECT = "/home/user/projects/v43/jenrik/lego-loco-rev-eng";
const DEFAULT_WORKFLOW_STATE = `${WORKFLOW_PROJECT}/.pi/workflow/decompilation-state.json`;
const DEFAULT_WORKFLOW_CORE = `${WORKFLOW_PROJECT}/tools/workflow_core.py`;

function shellQuote(value) {
  return "'" + String(value).replace(/'/g, "'\"'\"'") + "'";
}

function parseCoreOutput(command, commandResult) {
  const raw = String(commandResult.output || "").trim();
  let response;
  try {
    response = JSON.parse(raw);
  } catch (error) {
    throw new Error(`workflow core ${command} returned invalid JSON: ${raw.substring(0, 500)}`);
  }
  if (!commandResult.ok || !response || response.ok !== true) {
    const detail = response && response.error
      ? `${response.error.code || "error"}: ${response.error.message || "unknown core error"}`
      : raw || `exit ${commandResult.exitCode || "unknown"}`;
    throw new Error(`workflow core ${command} failed: ${detail}`);
  }
  return response.result;
}

async function workflowCoreCall(command, payload, options = {}) {
  const statePath = options.statePath || DEFAULT_WORKFLOW_STATE;
  const corePath = options.corePath || DEFAULT_WORKFLOW_CORE;
  const nonce = `${Date.now()}-${Math.random().toString(16).slice(2)}`;
  const requestPath = `${statePath}.request-${nonce}.json`;
  await pi.write({ path: requestPath, content: JSON.stringify(payload) });
  try {
    const commandResult = await pi.bash({
      command: `python3 ${shellQuote(corePath)} ${shellQuote(command)} --state ${shellQuote(statePath)} --input ${shellQuote(requestPath)}`,
      settle: true,
    });
    return parseCoreOutput(command, commandResult);
  } finally {
    await pi.bash({ command: `rm -f ${shellQuote(requestPath)}`, settle: true }).catch(() => {});
  }
}

function workflowTaskId(config) {
  const addresses = (config.functions || []).map((fn) => fn.address).sort().join(",");
  const phase = String(config.targetStatus || "INTEGRATED").toLowerCase();
  return `class:${config.className}:${phase}:${addresses}`;
}

function workflowTaskPayload(config, options = {}) {
  const ownerFiles = [
    `src/decompiled_cpp/${config.headerPath}`,
    `src/decompiled_cpp/${config.implPath}`,
  ];
  return {
    task: {
      id: options.taskId || workflowTaskId(config),
      title: `${config.className} → ${config.targetStatus || "INTEGRATED"}`,
      className: config.className,
      phase: options.phase || "integrate",
      ownerFiles,
      allowedWrites: options.allowedWrites || ownerFiles,
      sharedWrites: options.sharedWrites || ["PROGRESS.md"],
      metadata: {
        functions: (config.functions || []).map((fn) => ({ name: fn.name, address: fn.address })),
        ghidraDatabase: config.ghidraDatabase,
        targetStatus: config.targetStatus || "INTEGRATED",
      },
    },
  };
}

async function sourceFingerprint(projectRoot = WORKFLOW_PROJECT) {
  const result = await pi.bash({
    command: `cd ${shellQuote(projectRoot)} && { find src/decompiled_cpp -type f -not -path '*/build/*' -print0; test -f PROGRESS.md && printf 'PROGRESS.md\\0'; } | sort -z | xargs -0 -r sha256sum`,
    settle: true,
  });
  if (!result.ok) throw new Error(`unable to fingerprint source tree: ${result.output || result.error || "unknown error"}`);
  const fingerprints = {};
  for (const line of String(result.output || "").split("\n")) {
    if (!line) continue;
    const match = line.match(/^([0-9a-f]{64})\s{1,2}(.*)$/i);
    if (!match) throw new Error(`unexpected sha256sum output: ${line}`);
    fingerprints[match[2]] = match[1];
  }
  return fingerprints;
}

return {
  DEFAULT_WORKFLOW_STATE,
  workflowCoreCall,
  workflowTaskId,
  workflowTaskPayload,
  sourceFingerprint,
};
