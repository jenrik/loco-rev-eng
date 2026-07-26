/**
 * run-session.ts — Entry point for a decompilation session
 *
 * Usage (from fabric_exec):
 *   const code = await pi.read('tools/run-session.ts');
 *   const wrapped = '(async () => { ' + code + ' })()';
 *   return await eval(wrapped);
 *
 * This is the ONLY file you need to invoke. It:
 *   1. Opens the Ghidra database (if not already open)
 *   2. Loads the supervisor
 *   3. Dispatches all classes
 *
 * Edit the CLASSES array below to add/remove targets.
 */

const BINARY = "/home/user/projects/v43/jenrik/lego-loco-rev-eng/lego-loco-unpacked/Exe/loco.exe";
const GHIDRA_DB = "loco12";

// ============================================================================
// MODE — choose one:
//   "discover"  → supervisor finds classes needing work (recommended)
//   "explicit"  → specify CLASSES array below
// ============================================================================

const MODE = "discover";

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
  maxParallel: 3,                          // max concurrent decompileClass calls
  maxIterations: 5,                        // max review-primary passes per class
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
  // May already be open — that's fine
  print(`Ghidra: ${e.message || "already open?"}\n`);
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
    scope: "below-integrated",
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
