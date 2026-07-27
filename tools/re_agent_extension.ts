import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { StringEnum } from "@earendil-works/pi-ai";
import { Type } from "typebox";
import path from "node:path";

const daemonUrl = process.env.RE_DAEMON_URL;
const daemonToken = process.env.RE_DAEMON_TOKEN;
const agentId = process.env.RE_AGENT_ID;
const allowedWrites: string[] = (() => {
  try { return JSON.parse(process.env.RE_ALLOWED_WRITES || "[]"); } catch { return []; }
})();

function daemonReady(): string | null {
  if (!daemonUrl || !agentId) return "Reverse-engineering daemon configuration is missing.";
  return null;
}

async function daemonRequest(endpoint: string, init: RequestInit = {}) {
  const error = daemonReady();
  if (error) throw new Error(error);
  const headers = new Headers(init.headers);
  headers.set("content-type", "application/json");
  if (daemonToken) headers.set("x-re-daemon-token", daemonToken);
  const response = await fetch(`${daemonUrl}${endpoint}`, { ...init, headers });
  if (!response.ok) throw new Error(`Daemon request failed (${response.status}): ${await response.text()}`);
  return await response.json();
}

function normalizedProjectPath(candidate: unknown): string | null {
  if (typeof candidate !== "string" || !candidate) return null;
  const absolute = path.resolve(candidate);
  const relative = path.relative(process.cwd(), absolute);
  if (!relative || relative.startsWith("..") || path.isAbsolute(relative)) return null;
  return relative.split(path.sep).join("/");
}

function isAllowedWrite(candidate: unknown): boolean {
  const relative = normalizedProjectPath(candidate);
  return relative !== null && allowedWrites.includes(relative);
}

export default function (pi: ExtensionAPI) {
  pi.on("tool_call", async (event) => {
    if (event.toolName === "write" || event.toolName === "edit") {
      const file = event.input.path;
      if (!isAllowedWrite(file)) {
        return { block: true, reason: `Write outside approved scope. Use re_request_write_scope before editing ${String(file)}.` };
      }
    }
    if (event.toolName === "bash") {
      const command = String(event.input.command || "");
      const mutatingShell = /(^|[;&|]\s*)(rm|mv|cp|install)\b|\btee\b|\bsed\s+-i\b|\bperl\s+-i\b|(?<!\d)>{1,2}(?!&)/;
      if (mutatingShell.test(command)) {
        return { block: true, reason: "Potential shell write blocked. Use the edit/write tools within scope, or request a scope escalation." };
      }
    }
    return undefined;
  });

  pi.registerTool({
    name: "re_get_task",
    label: "Get RE Task",
    description: "Read the daemon-assigned reverse-engineering task, goal, and approved write scope.",
    parameters: Type.Object({}),
    async execute() {
      const context = await daemonRequest(`/internal/agents/${agentId}/context`);
      return { content: [{ type: "text", text: JSON.stringify(context, null, 2) }], details: context };
    },
  });

  pi.registerTool({
    name: "re_ghidra_query",
    label: "Query Ghidra",
    description: "Run an allowlisted read-only Ghidra operation through the daemon and record immutable evidence.",
    parameters: Type.Object({
      operation: StringEnum([
        "decompile_function", "disassemble_function", "list_functions", "get_xrefs_to", "get_xrefs_from",
        "list_structures", "get_structure", "list_names", "get_strings", "find_code_by_string",
      ] as const),
      arguments: Type.Object({}, { additionalProperties: true }),
    }),
    async execute(_id, params) {
      const result = await daemonRequest(`/internal/agents/${agentId}/ghidra`, {
        method: "POST", body: JSON.stringify(params),
      });
      return {
        content: [{ type: "text", text: JSON.stringify(result.response, null, 2) }],
        details: { evidence: result.evidence },
      };
    },
  });

  pi.registerTool({
    name: "re_record_observation",
    label: "Record RE Observation",
    description: "Record a Ghidra-backed observation or tentative hypothesis in the daemon event history.",
    parameters: Type.Object({
      address: Type.String({ description: "Binary address or evidence key" }),
      statement: Type.String({ description: "Observation or hypothesis" }),
      confidence: StringEnum(["observed", "tentative"] as const, { description: "observed requires direct binary support" }),
    }),
    async execute(_id, params) {
      const payload = { address: params.address, statement: params.statement, confidence: params.confidence };
      const event = await daemonRequest(`/internal/agents/${agentId}/events`, {
        method: "POST", body: JSON.stringify({ kind: "observation_recorded", payload }),
      });
      return { content: [{ type: "text", text: `Recorded ${params.confidence} observation for ${params.address}.` }], details: event };
    },
  });

  pi.registerTool({
    name: "re_transition_task",
    label: "Transition RE Task",
    description: "Report a scheduler-assigned task as completed, blocked, deferred, or failed.",
    parameters: Type.Object({
      status: StringEnum(["completed", "blocked", "deferred", "failed"] as const),
      reason: Type.Optional(Type.String()),
    }),
    async execute(_id, params) {
      const task = await daemonRequest(`/internal/agents/${agentId}/task/transition`, {
        method: "POST", body: JSON.stringify(params),
      });
      return { content: [{ type: "text", text: `Task transitioned to ${params.status}.` }], details: task };
    },
  });

  pi.registerTool({
    name: "re_defer_task",
    label: "Defer RE Task",
    description: "Defer the assigned task with a concrete reason and next investigation step.",
    parameters: Type.Object({ reason: Type.String(), nextAction: Type.String() }),
    async execute(_id, params) {
      const task = await daemonRequest(`/internal/agents/${agentId}/task/transition`, {
        method: "POST", body: JSON.stringify({ status: "deferred", reason: `${params.reason}\nNext action: ${params.nextAction}` }),
      });
      return { content: [{ type: "text", text: "Recorded deferred task." }], details: task };
    },
  });

  pi.registerTool({
    name: "re_request_write_scope",
    label: "Request Write Scope",
    description: "Request a new source-file write scope instead of editing an undeclared file.",
    parameters: Type.Object({ path: Type.String(), reason: Type.String() }),
    async execute(_id, params) {
      const request = await daemonRequest(`/internal/agents/${agentId}/write-scope-requests`, {
        method: "POST", body: JSON.stringify(params),
      });
      return { content: [{ type: "text", text: `Requested write scope for ${params.path}; do not edit it until approved.` }], details: request };
    },
  });
}
