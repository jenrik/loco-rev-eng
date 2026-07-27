import assert from "node:assert/strict";
import fs from "node:fs/promises";

const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;

async function loadFabricProgram(path, globals) {
  const source = await fs.readFile(path, "utf8");
  const fn = new AsyncFunction("pi", "agents", "print", source);
  return fn(globals.pi, globals.agents, globals.print || (() => {}));
}

function classParams(overrides = {}) {
  return {
    className: "Foo",
    headerPath: "game/Foo.h",
    implPath: "game/Foo.cpp",
    functions: [{ name: "Foo::Run", address: "0x401000" }],
    ghidraDatabase: "testdb",
    targetStatus: "INTEGRATED",
    maxIterations: 5,
    ...overrides,
  };
}

function review(approved) {
  return {
    status: "completed",
    value: {
      approved,
      currentStatus: approved ? "INTEGRATED" : "TRANSCRIBED",
      summary: approved ? "ready" : "work remains",
      issues: approved ? [] : [{
        severity: "BLOCKER",
        category: "test",
        description: "fix it",
        fix: "implement it",
      }],
      compilationStatus: "PASS",
    },
  };
}

function primary(status, summary = status, blocks = []) {
  return { text: JSON.stringify({ status, summary, compilationStatus: "PASS", blocks }) };
}

async function testPartialSkipsReview() {
  const events = [];
  const reviews = [review(false), review(true)];
  const answers = [primary("PARTIAL", "half done"), primary("DONE", "finished")];
  const agents = {
    create: async () => ({ id: "primary-1" }),
    ask: async ({ message }) => { events.push(["primary", message]); return answers.shift(); },
    run: async ({ name }) => { events.push(["review", name]); return reviews.shift(); },
    remove: async () => ({ removed: true }),
  };
  const pi = { bash: async () => ({ ok: true, output: "[OK] all files" }) };
  const { decompileClass } = await loadFabricProgram("tools/decompile-class.ts", { pi, agents });
  const result = await decompileClass(classParams());

  assert.equal(result.status, "approved");
  assert.deepEqual(events.map(([kind]) => kind), ["review", "primary", "primary", "review"]);
  assert.match(events[2][1], /returned PARTIAL/);
}

async function testRejectedBlockReturnsToSamePrimary() {
  const events = [];
  const answers = [
    primary("BLOCKED", "claimed block", [{ what: "name", why: "unsure" }]),
    primary("DONE", "resolved"),
  ];
  let reviewCount = 0;
  const agents = {
    create: async () => ({ id: "primary-2" }),
    ask: async ({ id, message }) => { events.push({ id, message }); return answers.shift(); },
    run: async ({ name }) => {
      if (name.startsWith("block-review")) {
        return { status: "completed", value: { legitimate: false, reason: "Ghidra can answer it", suggestion: "inspect xrefs" } };
      }
      return reviewCount++ === 0 ? review(false) : review(true);
    },
    remove: async () => ({ removed: true }),
  };
  const pi = { bash: async () => ({ ok: true, output: "[OK] all files" }) };
  const { decompileClass } = await loadFabricProgram("tools/decompile-class.ts", { pi, agents });
  const result = await decompileClass(classParams());

  assert.equal(result.status, "approved");
  assert.equal(events.length, 2);
  assert.equal(events[0].id, events[1].id);
  assert.match(events[1].message, /Ghidra can answer it/);
}

async function testLegitimateBlockStopsWithoutSupervisorAsk() {
  const askIds = [];
  const block = { what: "Base class", why: "separate class is missing", address: "0x402000" };
  const agents = {
    create: async () => ({ id: "primary-3" }),
    ask: async ({ id }) => { askIds.push(id); return primary("BLOCKED", "dependency", [block]); },
    run: async ({ name }) => name.startsWith("block-review")
      ? { status: "completed", value: { legitimate: true, reason: "separate dependency" } }
      : review(false),
    remove: async () => ({ removed: true }),
  };
  const pi = { bash: async () => ({ ok: true, output: "[OK] all files" }) };
  const { decompileClass } = await loadFabricProgram("tools/decompile-class.ts", { pi, agents });
  const result = await decompileClass(classParams());

  assert.equal(result.status, "blocked");
  assert.equal(result.blockReview.legitimate, true);
  assert.deepEqual(askIds, ["primary-3"]);
}

async function testBlockReviewFailureIsError() {
  const agents = {
    create: async () => ({ id: "primary-4" }),
    ask: async () => primary("BLOCKED", "dependency", [{ what: "X", why: "missing" }]),
    run: async ({ name }) => name.startsWith("block-review")
      ? { status: "failed", text: "not-json" }
      : review(false),
    remove: async () => ({ removed: true }),
  };
  const pi = { bash: async () => ({ ok: true, output: "[OK] all files" }) };
  const { decompileClass } = await loadFabricProgram("tools/decompile-class.ts", { pi, agents });
  const result = await decompileClass(classParams());

  assert.equal(result.status, "error");
  assert.match(result.error, /Block reviewer/);
}

async function testDirectedDiscoveryPromptAndSnapshot() {
  const pi = { read: async () => "" };
  const agents = {};
  const { buildSupervisorInstructions, buildSchedulingMessage } =
    await loadFabricProgram("tools/supervisor.ts", { pi, agents });
  const direction = "Get the main menu and all of its runtime dependencies working";
  const instructions = buildSupervisorInstructions({
    classes: [], scope: "below-integrated", direction,
    ghidraDatabase: "testdb", maxParallel: 3,
  }, true);
  const snapshot = buildSchedulingMessage({
    turn: 1, direction, running: [], justCompleted: [], knownOutcomes: [],
    availableSlots: 3, notice: null,
  });

  assert.match(instructions, /Discovery direction — primary session objective/);
  assert.match(instructions, /smallest dependency cone/);
  assert.match(instructions, /Get the main menu/);
  assert.match(snapshot, /"direction": "Get the main menu/);
}

async function testSupervisorReceivesValidatedBlock() {
  let askCount = 0;
  let completionSnapshot = "";
  const fakeClassModule = `return { decompileClass: async (config) => ({
    status: "blocked", className: config.className, primaryTurns: 1, reviewPasses: 1,
    blocks: [{ what: "Base", why: "dependency missing" }],
    blockReview: { legitimate: true, reason: "validated dependency" },
    finalReview: { currentStatus: "TRANSCRIBED" }
  })};`;
  const classes = [{
    className: "BlockedClass",
    headerPath: "game/BlockedClass.h",
    implPath: "game/BlockedClass.cpp",
    functions: [{ name: "BlockedClass::Run", address: "0x402000" }],
  }];
  const agents = {
    create: async () => ({ id: "supervisor-block" }),
    ask: async ({ message }) => {
      askCount++;
      if (askCount === 1) {
        return { text: JSON.stringify({ action: "START", summary: "start", reason: "ready", starts: [{ className: "BlockedClass" }] }) };
      }
      completionSnapshot = message;
      return { text: JSON.stringify({ action: "COMPLETE", summary: "blocked dependency recorded", reason: "nothing else safe", starts: [] }) };
    },
    remove: async () => ({ removed: true }),
  };
  const pi = { read: async () => fakeClassModule };
  const { run } = await loadFabricProgram("tools/supervisor.ts", { pi, agents });
  const result = await run({ classes, ghidraDatabase: "testdb", maxParallel: 1, workflowStatePath: false });

  assert.equal(result.blocked, 1);
  assert.match(completionSnapshot, /"status": "blocked"/);
  assert.match(completionSnapshot, /validated dependency/);
}

async function testSupervisorDiscardsStaleLaunchDecision() {
  const launched = [];
  let askCount = 0;
  const fakeClassModule = `return { decompileClass: async (config) => {
    globalThis.__workflowLaunches.push(config.className);
    await new Promise((resolve) => setTimeout(resolve, 5));
    return { status: "approved", className: config.className, primaryTurns: 1,
      reviewPasses: 1, finalReview: { currentStatus: "INTEGRATED" } };
  }};`;
  globalThis.__workflowLaunches = launched;

  const classes = ["A", "B"].map((className, index) => ({
    className,
    headerPath: `game/${className}.h`,
    implPath: `game/${className}.cpp`,
    functions: [{ name: `${className}::Run`, address: `0x40100${index}` }],
  }));

  const agents = {
    create: async () => ({ id: "supervisor-1" }),
    ask: async () => {
      askCount++;
      if (askCount === 1) {
        return { text: JSON.stringify({ action: "START", summary: "start A", reason: "independent", starts: [{ className: "A" }] }) };
      }
      if (askCount === 2) {
        await new Promise((resolve) => setTimeout(resolve, 20));
        return { text: JSON.stringify({ action: "START", summary: "start B", reason: "slot free", starts: [{ className: "B" }] }) };
      }
      return { text: JSON.stringify({ action: "COMPLETE", summary: "done", reason: "scope complete", starts: [] }) };
    },
    remove: async () => ({ removed: true }),
  };
  const pi = { read: async () => fakeClassModule };
  const { run } = await loadFabricProgram("tools/supervisor.ts", { pi, agents });
  const result = await run({ classes, ghidraDatabase: "testdb", maxParallel: 2, workflowStatePath: false });

  assert.deepEqual(launched, ["A"], "B launch from stale supervisor turn must be discarded");
  assert.equal(result.approved, 1);
  assert.equal(askCount, 3);
  delete globalThis.__workflowLaunches;
}

async function testExplicitConfigPreservesCanonicalFiles() {
  const pi = { read: async () => "" };
  const { normalizeConfig } = await loadFabricProgram("tools/supervisor.ts", { pi, agents: {} });
  const canonical = classParams({
    className: "Canonical",
    headerPath: "game/Canonical.h",
    implPath: "game/Canonical.cpp",
  });
  const normalized = normalizeConfig({
    className: "Canonical",
    headerPath: "../../outside.h",
    implPath: "../../outside.cpp",
    functions: [{ name: "Wrong::Function", address: "0x401111" }],
    retry: true,
    guidance: "retry after dependency",
  }, { classes: [canonical], ghidraDatabase: "testdb", maxIterations: 5 }, false);

  assert.ok(normalized.config);
  assert.equal(normalized.config.headerPath, "game/Canonical.h");
  assert.equal(normalized.config.implPath, "game/Canonical.cpp");
  assert.deepEqual(normalized.config.functions, canonical.functions);
  assert.equal(normalized.config.supervisorGuidance, "retry after dependency");

  const unsafeDiscovery = normalizeConfig({
    className: "Unsafe", headerPath: "../Unsafe.h", implPath: "game/Unsafe.cpp",
    functions: [{ name: "Unsafe::Run", address: "0x401000" }],
  }, { ghidraDatabase: "testdb", maxIterations: 5 }, true);
  assert.match(unsafeDiscovery.error, /unsafe/);
}

const tests = [
  testPartialSkipsReview,
  testRejectedBlockReturnsToSamePrimary,
  testLegitimateBlockStopsWithoutSupervisorAsk,
  testBlockReviewFailureIsError,
  testDirectedDiscoveryPromptAndSnapshot,
  testSupervisorReceivesValidatedBlock,
  testSupervisorDiscardsStaleLaunchDecision,
  testExplicitConfigPreservesCanonicalFiles,
];

for (const test of tests) {
  await test();
  console.log(`ok - ${test.name}`);
}
console.log(`${tests.length} workflow tests passed`);
