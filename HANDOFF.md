# Handoff: DirectDraw shim — Cursor subsystem is the last blocker before wiring

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

## `GetDC` blocker: RESOLVED — `ui/UIPANEL.cpp`/`world/tilemap.cpp`: RESOLVED

The `UIPANEL_BeginPaint` `ExitProcess(1)` landmine described in the previous
version of this section is fixed (commit `c9c4640`): `Sdl3DirectDrawSurface::
GetDC`/`ReleaseDC` now succeed with a valid non-null opaque handle instead of
permanently failing, and `ui/UIPANEL.cpp`'s `GetDC`/`ReleaseDC`/7×`Blt` raw
dispatch sites are converted to named calls. Ghidra decompile of 0x426B00/
0x426B90 also caught a real transcription bug: the prior code checked
`GetDC`'s HRESULT return as if it were the HDC itself, and slot 0x68 was
mislabeled "Unlock" when it's really `ReleaseDC` (an HDC flows into it, not a
`RECT*`). `world/tilemap.cpp`'s `TileMap_LockPrimarySurface`/
`UnlockPrimarySurface` raw ABI-slot dispatch is also converted (commit
`6eb1645`) — see PROGRESS.md's 2026-08-14 `directdraw-shim-getdc-and-tilemap`
entry for the full trail.

## Current blocker: the Cursor subsystem's own raw dispatch (28 sites)

`input/Cursor_internal.h` declares 5 helper functions —
`Cursor_ComSurfaceRelease`, `Cursor_SurfaceBlt`, `Cursor_SurfaceReleaseDC`,
`Cursor_SurfaceFill`, `Cursor_SurfaceLegacyBlt` — that each dereference a
surface's vtable by raw real-ABI slot number (`Release`=2, `Blt`=5 — three
differently-named helpers all point at this same slot — `ReleaseDC`=0x68/4=26,
matching the `UIPANEL_EndPaintEx` finding above). This shim is
API-compatible, not ABI-compatible (`platform/ddraw_interfaces.h`'s
declaration order doesn't match these numbers), so once `g_primary_surface`/
`g_backbuffer`/`_g_primary_surface`/`_g_backbuffer` are non-null, every one
of these dispatches through the wrong compiler-generated slot on a real
`Sdl3DirectDrawSurface` object — the exact class of bug Phase 5(a) already
fixed once for `g_ddraw`'s own raw-dispatch sites. This is the last
prerequisite before wiring is safe.

The header's own comment (`Cursor_internal.h:301-304`) claims this is a
permitted "opaque COM surface" ABI-boundary exception — that was true before
this session's shim existed, but is stale now that `platform/
ddraw_interfaces.h`'s `IDirectDrawSurface4` is a real typed interface; CLAUDE.md's
opaque-COM exception applies only when no typed interface is available.

28 call sites across 5 files, none yet converted: `input/Cursor_Render.cpp`
(6), `input/Cursor.cpp` (2), `input/Cursor_Editor.cpp` (2), `input/
Cursor_impls.cpp` (14), `input/Cursor_new_impls.cpp` (4) — operating on
`this->primary_surface()`/`this->backbuffer()`/`_g_backbuffer`/
`_g_primary_surface`/`editor_surf_a`/`editor_surf_b`/`target_surf`, all
confirmed sharing addresses with `g_primary_surface`/`g_backbuffer`. The
evidence for each slot's real method is already fully resolved in-file
(comments cite the real interface method by name) — what's left is
mechanical: convert the "get a function pointer, then call it" two-step
pattern (`Cursor_SurfaceBlt(surface)(args...)`) into a direct named virtual
call (`static_cast<IDirectDrawSurface4*>(surface)->Blt(args...)`) at all 28
sites, plus removing the now-dead helpers/typedefs from `Cursor_internal.h`.
Not started — stopped here to report the true scope rather than silently
absorb a second ~30-site sweep without a checkpoint.

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

1. Convert the Cursor subsystem's 28 raw-dispatch call sites (see above) —
   the last prerequisite before wiring. Evidence is fully resolved; this is
   a mechanical sweep across 5 files, build/test after each file, same
   pattern already used for `ui/UIPANEL_Surface.cpp`/`graphics/LOCOBITMAP.cpp`/
   `ui/UIPANEL.cpp`/`world/tilemap.cpp` this session.
2. Once that sweep is done and verified, wire `g_primary_surface =
   g_sdl_primary_surface;` / `g_backbuffer = g_sdl_backbuffer;` inside
   `SDL3_EnsurePrimarySurface()` (`graphics/sdl3_ddraw.cpp`) — mirroring
   exactly how `g_ddraw = g_sdl_ddraw;` was wired in Phase 5(b). Both
   `Sdl3DirectDrawSurface` objects already exist there (constructed via
   `g_ddraw->CreateSurface(...)`, used internally for rendering) — this is a
   2-line change once everything upstream is safe. Run `--suite integration`
   specifically watching `native/DDRAW_DimSurfaceRect.c`/`town/TownTiles.cpp`'s
   `Town_CheckOccupiedEx` — both confirmed live, and this is the first time
   they'll exercise a real Lock/Unlock against a real surface end-to-end.
3. Leave the stale `pi-fabric` branches above alone unless the user
   specifically asks to revive one — confirm which problem they're meant to
   solve first, since the codebase has moved substantially since they were
   written.
4. If reviving any of the `pi-fabric` work, first update `devbox/main` to
   the merged tip (with user confirmation, since it's a push) so future
   parallel sessions don't re-diverge from the stale `5526b16`.
