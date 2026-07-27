import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";

const execFileAsync = promisify(execFile);
const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;
const ROOT = path.resolve(".");

async function loadSupervisor(pi, agents) {
  const source = await fs.readFile("tools/supervisor.ts", "utf8");
  const fn = new AsyncFunction("pi", "agents", "print", source);
  return fn(pi, agents, () => {});
}

async function shell(command) {
  try {
    const { stdout, stderr } = await execFileAsync("bash", ["-lc", command], { cwd: ROOT });
    return { ok: true, output: stdout + stderr };
  } catch (error) {
    return { ok: false, output: String(error.stdout || "") + String(error.stderr || ""), exitCode: error.code };
  }
}

const fakeClassModule = `return { decompileClass: async (config) => ({
  status: "approved", className: config.className, primaryTurns: 1, reviewPasses: 1,
  finalReview: { currentStatus: "INTEGRATED" }
})};`;

async function testSupervisorPersistsApprovedAttempt() {
  const temporary = await fs.mkdtemp(path.join(os.tmpdir(), "lego-loco-workflow-state-"));
  const statePath = path.join(temporary, "state.json");
  let asks = 0;
  const pi = {
    read: async (file) => file.endsWith("decompile-class.ts")
      ? fakeClassModule
      : fs.readFile(file, "utf8"),
    write: async ({ path: file, content }) => {
      await fs.mkdir(path.dirname(file), { recursive: true });
      await fs.writeFile(file, content, "utf8");
      return { ok: true, output: "" };
    },
    bash: async ({ command }) => shell(command),
  };
  const agents = {
    create: async () => ({ id: "supervisor" }),
    ask: async () => {
      asks++;
      return { text: JSON.stringify(asks === 1
        ? { action: "START", summary: "start", reason: "ready", starts: [{ className: "Foo" }] }
        : { action: "COMPLETE", summary: "done", reason: "scope complete", starts: [] }) };
    },
    remove: async () => ({ removed: true }),
  };
  try {
    const { run } = await loadSupervisor(pi, agents);
    const result = await run({
      classes: [{
        className: "Foo",
        headerPath: "game/Foo.h",
        implPath: "game/Foo.cpp",
        functions: [{ name: "Foo::Run", address: "0x401000" }],
      }],
      ghidraDatabase: "testdb",
      maxParallel: 1,
      projectRoot: ROOT,
      workflowStatePath: statePath,
      workflowCorePath: path.join(ROOT, "tools/workflow_core.py"),
      workflowBinary: { sha256: "test-binary" },
    });
    assert.equal(result.approved, 1);
    assert.equal(result.errors, 0);

    const state = JSON.parse(await fs.readFile(statePath, "utf8"));
    const task = Object.values(state.tasks)[0];
    assert.equal(task.status, "integrated");
    assert.equal(task.writeAudits.length, 1);
    assert.deepEqual(task.writeAudits[0].unexpected, []);
  } finally {
    await fs.rm(temporary, { recursive: true, force: true });
  }
}

await testSupervisorPersistsApprovedAttempt();
console.log("workflow supervisor core integration test passed");
