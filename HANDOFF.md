# Handoff: DirectDraw shim — GetDC blocker, remaining UIPANEL.cpp cleanup

Originally written 2026-08-14 to reconcile local `main` with `devbox/main`
after they diverged 13 vs. 19 commits from a shared base. **That
reconciliation is done** — merged in commit `a81b483` (2026-08-14), see
PROGRESS.md's `reconcile-devbox-main-merge` session-log entry for the full
verification trail (conflict resolution, both-sides-touched-file review,
build/test/MinGW-typecheck diffing against both parents, `nm -u` check).
Do not re-attempt that merge.

What's left from that session is the DirectDraw shim's remaining blocker and
cleanup, below.

## Note on the remote: `devbox/main` is now stale

`devbox` (the remote) still points at `5526b16`, the tip *before* this
merge. Local `main` now contains both histories, but the remote does not.
If another agent/session on `devbox` starts from `devbox/main` as-is, it
re-diverges from a stale base and this same reconciliation has to happen
again. Updating `devbox/main` to match is a push to a shared remote —
confirm with the user before doing it, don't push unilaterally.

## Critical blocker: do not wire `g_primary_surface`/`g_backbuffer` yet

Found while migrating `ui/UIPANEL.cpp` (not committed — reverted after this
was found; see PROGRESS.md's 2026-08-14 DirectDraw-shim entries for full
detail):

**`UIPANEL_BeginPaint` (0x426B00) will call `ExitProcess(1)` the moment
`g_primary_surface` is non-null and any real caller runs.** It retries a raw
`GetDC` dispatch 1000× at 10ms, then calls `WIN32_FatalError()`/
`ExitProcess(1)` on failure. `Sdl3DirectDrawSurface::GetDC` is a permanent,
deliberately-scoped no-op (real GDI device-context interop was explicitly
out of scope in the shim plan) that always fails. `game/BuildingPanel.cpp`
has live, real calls into `UIPANEL_BeginPaint`.

This inverts the pattern used everywhere else in this shim: elsewhere, a
null-check-and-fall-through guard is correct because the underlying object
becoming real *fixes* the path. Here, the object becoming real *arms a timed
self-destruct*. **A real `GetDC` implementation (or a different, non-fatal
path for `UIPANEL_BeginPaint`'s callers) is a hard prerequisite for wiring
`g_primary_surface`/`g_backbuffer`, on top of the already-resolved
pixel-format fix.** Do not let any integration work (from `devbox/main` or
the stale mode3 branches below) wire these globals without addressing this
first — it would turn a currently-safe no-op into a reproducible crash days
or weeks later when someone least expects it.

**Update (2026-08-16)**: `UIPANEL::BeginPaint` itself is now integrated as a
real class method (typed `IDirectDrawSurface4::GetDC()` call, real HRESULT/
out-param handling — see PROGRESS.md's 2026-08-16 entry) — the raw
vtable-slot dispatch for `GetDC` is gone. This does **not** lift the
blocker above: `g_primary_surface` is still unwired and
`Sdl3DirectDrawSurface::GetDC` is still a permanent no-op, so the retry-
then-`ExitProcess(1)` path is exactly as fatal as before the moment the
global is wired. A thin free-function shim (`UIPANEL_BeginPaint(void*)`)
was kept over the new method since ~9 other files still call it as a free
function; two genuine call-0-class landmines among those callers
(`game/BuildingPanel.cpp`, `network/NetworkPlayerList.cpp`) were fixed
alongside it.

`ui/UIPANEL.cpp` still has 8 more raw vtable-slot dispatch sites (`Unlock`×1
in `UIPANEL_EndPaintEx`, `Blt`×7 in `UIPANEL_Render`) — clean, low-risk,
evidenced conversions, deliberately left alone so the file doesn't end up
half-migrated again. Convert all of them together once `GetDC` has a real
answer. `world/tilemap.cpp` also has raw vtable dispatch on
`g_primary_surface`, not yet examined at all.

## Stale, do-not-merge-as-is branches (fetched, not integrated)

Present on `devbox` (already fetched into local remote-tracking refs): a
family of `pi-fabric/{impl,repair}-{enter-mode3,mode3-host-bootstrap,mode3-tests}-*`
branches, all forked from a single commit dated **2026-07-31** —
`3d50c5e5`, well behind current `main`. These look like an earlier,
abandoned parallel-attempt cycle at the same "mode 3 presents nothing"
problem (`impl-*` and `repair-*` pairs with the `repair-*` branch built as a
**fresh 2-commit branch off the same old base**, not stacked on `impl-*` —
confirmed via `git merge-base --is-ancestor`, it returns false):

```
devbox/pi-fabric/impl-enter-mode3-9a2987ed         (1 commit)
devbox/pi-fabric/repair-enter-mode3-b7155aed       (2 commits, same base)
devbox/pi-fabric/impl-mode3-host-bootstrap-495375c9 (1 commit)
devbox/pi-fabric/repair-host-mode3-d39f065b        (2 commits, same base)
devbox/pi-fabric/impl-mode3-tests-eb9e125a         (1 commit)
devbox/pi-fabric/repair-mode3-tests-b3a146f2       (2 commits, same base)
devbox/wip/directplay-investigation                (2 commits, based ~08-06)
devbox/feature/intro-video-player                  (1 commit, based ~07-29)
```

`impl-enter-mode3`/`repair-enter-mode3` both touch `core/CGWND.cpp` in ways
that directly overlap the DirectDraw-shim's own investigation of that file:
this session's Phase 5(b) work (commit `fb5680d`) **deliberately rejected**
hooking `CGWND_InitAllSubsystems` as a DirectDraw bring-up point, because
direct investigation found the original bring-up chain
(`ResourceManager::Init()` → `DDRAW_GetSurface`) has zero real callers on the
host build today — inserting a call there would fabricate a call site with
no evidence in the original control flow. Before reviving anything from
these branches, check whether they made that same mistake (fabricating a
bring-up path) — they predate that finding by weeks.

`impl-mode3-host-bootstrap`/`repair-host-mode3` add a brand-new
`sdl3_mode3_bootstrap.cpp` that appears to drive real mode-3 rendering. If
this ever gets revived, it **must** be checked against the
`UIPANEL_BeginPaint` finding above first — driving real paint calls with a
wired `g_primary_surface` and no working `GetDC` will make the process
`ExitProcess(1)` itself after ~10 seconds.

Given the age and the duplicate impl/repair structure, treat these as
**reference material for what was already tried and abandoned**, not as
work to merge. Confirm with the user before reviving any of them.

## Where to read more

- `PROGRESS.md`, Priority-1, the "DirectDraw shim" bullet and its
  2026-08-14 session-log entries — the full evidence trail for everything
  above, including the `reconcile-devbox-main-merge` entry documenting the
  merge this file used to be about.
- `.claude/plans/when-advancing-to-gameplay-sunny-tome.md` — the original
  6-phase shim plan (Phases 1-4 done, Phase 5 partially done/blocked, Phase 6
  partially done).
- Project memory `project_directdraw_shim.md` (persistent Claude memory) —
  a shorter cross-session summary of the same state.
- `NOTE-directx-sdk.md` — sourcing guardrail for DirectDraw SDK evidence
  (headers only, never the real `ddraw.dll` implementation or SDK sample
  code).

## Suggested next steps, in order

1. Real `GetDC` implementation (or a non-fatal path for
   `UIPANEL_BeginPaint`'s callers) — required before wiring
   `g_primary_surface`/`g_backbuffer`. This is real, separately-scoped GDI
   work; do not shortcut it by wiring the globals anyway.
2. Once `GetDC` has a real answer, convert `ui/UIPANEL.cpp`'s remaining
   raw-dispatch sites (`Unlock`×1, `Blt`×7) together, plus examine
   `world/tilemap.cpp`'s raw vtable dispatch on `g_primary_surface`.
3. Leave the branches above alone unless the user specifically asks to
   revive one — confirm which problem they're meant to solve first, since
   the codebase has moved substantially since they were written.
4. If reviving any of the `pi-fabric` work, first update `devbox/main` to
   the merged tip (with user confirmation, since it's a push) so future
   parallel sessions don't re-diverge from the stale `5526b16`.
