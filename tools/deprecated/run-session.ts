/**
 * run-session.ts — Entry point for a decompilation session
 *
 * Usage (from fabric_exec):
 *   const code = await pi.read('tools/run-session.ts');
 *   const wrapped = '(async () => { ' + code + ' })()';
 *   return await eval(wrapped);
 *
 * This is the ONLY file you need to invoke. It:
 *   1. Opens the raw binary with a fresh Ghidra database ID
 *   2. Loads the supervisor
 *   3. Runs the incremental supervisor scheduler
 *
 * Edit the CLASSES array below to add/remove targets.
 */

const BINARY = "/home/user/projects/v43/jenrik/lego-loco-rev-eng/lego-loco-unpacked/Exe/loco.exe";
const GHIDRA_DB = `loco${Date.now()}`; // fresh ID; stale IDs cannot be reused

// ============================================================================
// MODE — choose one:
//   "discover"  → supervisor finds classes needing work (recommended)
//   "explicit"  → specify CLASSES array below
// ============================================================================

const MODE = "discover";

// Optional discovery objective. Set to null for broad status-based discovery.
const DISCOVERY_DIRECTION =
  "Get the main menu and all of its runtime dependencies working";

// Only used when MODE = "explicit"
const CLASSES = [
  // {
  //   className: "GameVehicle",
  //   headerPath: "game/GameVehicle.h",
  //   implPath: "game/GameVehicle.cpp",
  //   functions: [
  //     { name: "GameVehicle::GameVehicle", address: "0x412870" },
  //   ],
  //   parentClass: "RESDATA_GameVehicle",
  //   vtableAddress: "0x477848",
  // },
];

// ============================================================================
// Session config
// ============================================================================

const CONFIG = {
  direction: MODE === "discover" ? DISCOVERY_DIRECTION : null,
  workflowBinary: { filePath: BINARY },
  maxParallel: 3,                          // hard cap; supervisor decides safe occupancy
  maxIterations: 5,                        // maximum PRIMARY work turns per attempt
  primaryModel: "deepseek/deepseek-v4-pro",
  reviewerModel: "deepseek/deepseek-v4-pro",
  supervisorModel: "deepseek/deepseek-v4-pro",
};

// ============================================================================
// Bootstrap
// ============================================================================

// 1. Open Ghidra
print("Opening Ghidra...\n");
try {
  const open = await mcp.ghidra.open_database({
    file_path: BINARY,
    database_id: GHIDRA_DB,
  });
  if (open.structuredContent?.status === "opening") {
    print("Waiting for analysis...\n");
    await mcp.ghidra.wait_for_analysis({ database: GHIDRA_DB });
  }
  print(`Ghidra ready: ${open.structuredContent?.database || GHIDRA_DB}\n`);
} catch (e) {
  print(`Ghidra open failed: ${e.message}\n`);
  throw e;
}

// 2. Load and run supervisor
print("Loading supervisor...\n");
const code = await pi.read('tools/supervisor.ts');
const wrapped = '(async () => { ' + code + ' })()';
const { run } = await eval(wrapped);

// 3. Go
if (MODE === "discover") {
  return await run({
    discover: true,
    scope: DISCOVERY_DIRECTION ? "all" : "below-integrated",
    ghidraDatabase: GHIDRA_DB,
    ...CONFIG,
  });
} else {
  return await run({
    classes: CLASSES,
    ghidraDatabase: GHIDRA_DB,
    ...CONFIG,
  });
}
