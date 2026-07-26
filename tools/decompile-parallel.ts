/**
 * decompile-parallel.ts — Concurrency pool for multiple decompileClass() calls
 *
 * Usage (from fabric_exec):
 *   const code = await pi.read('tools/decompile-parallel.ts');
 *   const wrapped = '(async () => { ' + code + ' })()';
 *   const { decompileParallel } = await eval(wrapped);
 *   return await decompileParallel([ClassA, ClassB, ...], { maxParallel: 3 });
 *
 * maxParallel is the max number in-flight via a custom concurrency pool.
 * When one finishes, the next starts immediately — no batch waiting.
 * Failures are captured per job (not via Promise.allSettled).
 */

async function loadDecompileClass() {
  const code = await pi.read('tools/decompile-class.ts');
  const wrapped = '(async () => { ' + code + ' })()';
  const { decompileClass } = await eval(wrapped);
  return decompileClass;
}

async function decompileParallel(classes, opts = {}) {
  const {
    maxParallel = 4,
    baseGhidraDb = 12,
    primaryModel = "deepseek/deepseek-v4-pro",
    reviewerModel = "deepseek/deepseek-v4-pro",
    maxIterations = 5,
    supervisorId = null,
  } = opts;

  // Validate
  if (!Array.isArray(classes) || classes.length === 0) {
    print("decompileParallel: no classes to process.\n");
    return { total: 0, approved: 0, blocked: 0, maxIterationsReached: 0, errors: 0, results: [] };
  }

  const concurrency = Math.max(1, Math.min(maxParallel, classes.length));

  const decompileClass = await loadDecompileClass();

  print(`\n═══ DECOMPILE-PARALLEL: ${classes.length} class(es) ═══\n`);
  print(`Concurrency: ${concurrency}  |  Max passes: ${maxIterations}\n`);
  print(`Primary: ${primaryModel}  |  Reviewer: ${reviewerModel}\n\n`);

  // Assign Ghidra DB IDs
  const jobs = classes.map((cls, i) => ({
    ...cls,
    ghidraDatabase: cls.ghidraDatabase || `loco${baseGhidraDb + i}`,
    primaryModel: cls.primaryModel || primaryModel,
    reviewerModel: cls.reviewerModel || reviewerModel,
    maxIterations: cls.maxIterations || maxIterations,
    supervisorId: cls.supervisorId || supervisorId,
  }));

  // Concurrency pool
  const total = jobs.length;
  const results = new Array(total);
  let nextIndex = 0;
  let activeCount = 0;

  await new Promise((resolve) => {
    function startNext() {
      while (activeCount < concurrency && nextIndex < total) {
        const idx = nextIndex++;
        const job = jobs[idx];
        activeCount++;

        print(`  ▶ ${job.className} (DB: ${job.ghidraDatabase}, ${job.functions.length} fn(s))\n`);

        decompileClass(job).then(
          (value) => {
            const icon = value.status === "approved" ? "✅" : value.status === "blocked" ? "⏸️" : "⚠️";
            print(`${icon} ${job.className}: ${value.status}` +
              (value.iterations !== undefined ? ` — ${value.iterations} pass(es)` : "") +
              (value.finalReview ? ` — ${value.finalReview.currentStatus}` : "") + `\n`);
            results[idx] = { className: job.className, ...value };
          },
          (reason) => {
            print(`❌ ${job.className}: ${reason?.message || reason}\n`);
            results[idx] = { className: job.className, status: "error", error: reason?.message || String(reason) };
          }
        ).finally(() => {
          activeCount--;
          if (nextIndex < total) {
            startNext();
          } else if (activeCount === 0) {
            resolve();
          }
        });
      }
    }

    startNext();
  });

  // Summary
  const finished = results.filter(r => r);
  const approved = finished.filter(r => r.status === "approved");
  const blocked = finished.filter(r => r.status === "blocked");
  const maxed = finished.filter(r => r.status === "max_iterations_reached");
  const errors = finished.filter(r => r.status === "error");

  print(`\n═══ DONE ═══\n`);
  print(`  Approved: ${approved.length}  |  Blocked: ${blocked.length}  |  Maxed: ${maxed.length}  |  Errors: ${errors.length}\n`);
  if (approved.length > 0) print(`  Approved: ${approved.map(r => r.className).join(", ")}\n`);
  if (blocked.length > 0) print(`  Blocked: ${blocked.map(r => r.className).join(", ")}\n`);
  if (maxed.length > 0) print(`  Incomplete: ${maxed.map(r => `${r.className} (${r.finalReview?.currentStatus || "?"})`).join(", ")}\n`);
  if (errors.length > 0) print(`  Errors: ${errors.map(r => r.className).join(", ")}\n`);

  return {
    total,
    approved: approved.length,
    blocked: blocked.length,
    maxIterationsReached: maxed.length,
    errors: errors.length,
    results: finished,
  };
}

return { decompileParallel };
