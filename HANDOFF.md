# Handoff: reconcile local `main` with `devbox/main`, then finish the DirectDraw shim

Written 2026-08-14 at the end of a session that built a real DirectDraw↔SDL3
shim. Read this before touching anything — the two branches below diverged
from the same commit and independently touched several of the same files.

## 1. The two histories to reconcile

Local `main` (this checkout, HEAD `cc9ec62`) and `devbox/main` (fetch first:
`git fetch devbox` — it was stale and needed an explicit fetch this session)
both descend from `9fb81420b39f31a27434c172dd2bef1eabe1def2` and have
**diverged 12 vs. 19 commits** since (`git rev-list --count
$(git merge-base main devbox/main)..main` / `..devbox/main`). Neither is a
superset of the other. Confirm the exact state before doing anything else —
the counts above may have shifted if either side gained commits since this
was written:

```bash
git fetch devbox
git merge-base main devbox/main   # confirm it still prints 9fb8142...
git log --oneline $(git merge-base main devbox/main)..main       # this session's work
git log --oneline $(git merge-base main devbox/main)..devbox/main # the parallel RE agent's work
```

**Local `main`** (this session): a full DirectDraw 6.0→SDL3 shim —
`platform/ddraw_interfaces.h` (real pure-virtual `IDirectDraw4`/
`IDirectDrawSurface4`/`IDirectDrawPalette`/`IDirectDrawClipper`), concrete
`Sdl3DirectDraw4`/`Sdl3DirectDrawSurface`/etc. (`graphics/sdl3_ddraw.*`), a
Lock/Unlock-boundary RGB565↔XRGB8888 pixel-format adapter, `g_ddraw` wired to
a real device, and two files (`ui/UIPANEL_Surface.cpp`,
`graphics/LOCOBITMAP.cpp`) migrated off local shadow DirectDraw types onto
the canonical interface. Full detail: PROGRESS.md's DirectDraw-shim
Priority-1 note and its 2026-08-14 session-log entries; the approved plan at
`.claude/plans/when-advancing-to-gameplay-sunny-tome.md`.

**`devbox/main`** (a parallel agent, same day): 20 commits of facade-wiring
fixes — real functions were reconstructed but callers still routed through
long-dead stub facades. Covers DirectSound buffer release
(`ReleaseSoundResource`), `ResourceManager_GetStringById`, `RESMGR_PlaySound`
→ real `PlaySound(UINT)`, `DPLAY_SetPlayerData`/`PlayerConfig_SaveToFile`
facades, `GameAudio_StopAll`/`CGWND_Cleanup` shutdown wiring, and one
independent fix to `platform/sdl3_types.h`'s `DDSURFACEDESC` field order.

## 2. Files touched on BOTH sides — check each one by hand

```bash
comm -12 \
  <(git diff --name-only $(git merge-base main devbox/main) devbox/main | sort) \
  <(git diff --name-only $(git merge-base main devbox/main) main | sort)
```

```
core/CGWND.cpp
graphics/LOCOBITMAP.cpp
platform/sdl3_types.h
PROGRESS.md
shared/defsym_stubs.cpp
shared/link_stubs.cpp
shared/stubs_impl.cpp
```

Do not assume these merge cleanly just because a 3-way merge doesn't flag a
textual conflict — a couple of these are *semantic* overlaps that a mechanical
merge won't catch:

- **`platform/sdl3_types.h` — near-certain real overlap, bigger than it looks
  from the file name alone.** Local commit `da0d62f` (Phase 1) fixed
  `DDSURFACEDESC`'s `dwWidth`/`dwHeight` field order and added
  `DDPIXELFORMAT`/completed `DDBLTFX`, still inside `platform/sdl3_types.h`
  at that point. `devbox/main`'s commit `97feceb` fixes the *exact same*
  field-order bug independently, patching a constructor initializer list
  (`DDSURFACEDESC() : dwSize(0), dwFlags(0), dwWidth(0), dwHeight(0), ...`)
  against that same pre-Phase-1 shape. But a **later** local commit
  (Phase 2+3, `628b1fa`) moved `DDSURFACEDESC` (and `DDBLTFX`/`DDPIXELFORMAT`/
  `DDCOLORKEY`/`DDSCAPS2`) out of `platform/sdl3_types.h` entirely, into the
  new `platform/ddraw_interfaces.h`, and dropped the explicit constructor in
  favor of default member initializers (`uint32_t dwHeight = 0;` inline, no
  `DDSURFACEDESC() : ...` initializer list at all — confirmed by reading the
  current file, there is none). `platform/sdl3_types.h` today just
  `#include`s `ddraw_interfaces.h` for these types. So `97feceb`'s patch
  target — a constructor initializer list, in that file — **no longer
  exists** on the local side. A mechanical merge/rebase of `97feceb` here
  will either conflict outright or, worse, silently apply cleanly against
  unrelated context and do nothing useful. Skip applying `97feceb` as a
  patch; instead just confirm the merged `platform/ddraw_interfaces.h` has
  `dwHeight` before `dwWidth` (it already does) and delete `97feceb`'s intent
  as fully subsumed.
- **`graphics/LOCOBITMAP.cpp` — likely non-overlapping hunks, verify anyway.**
  This session's commit `137d50e` removed the file's local
  `DirectDrawSurfaceView` shadow interface and converted `DDRAW_PresentRect`/
  `UIPANEL_DestroySurface`'s `.blt()`/`.restore()`/`.release()` calls to
  `.Blt()`/`.Restore()`/`.Release()`. `devbox/main`'s commit `9b81c20`
  touches a *different* function in the same file
  (`PostcardAlbum::BlitElement`, removing a fabricated `RESMGR_PlaySound`
  facade in favor of the real `PlaySound(UINT)`). Different regions, but
  confirm the merge didn't silently drop either change — diff the merged
  result against both parents' hunks.
- **`core/CGWND.cpp`** — local `328d1ec` fixed `CGWND_SetMode`'s call-0/
  silent-stub cluster (9 lines); devbox's `9ca7dab` wired
  `GameAudio_StopAll`/`CGWND_Cleanup`'s ResourceManager shutdown. Different
  functions, but this file has been a recurring source of "the original
  bring-up chain is dead on host" surprises this session (see §3) — read the
  merged file's control flow, don't just trust a clean textual merge.
- **`shared/defsym_stubs.cpp` / `shared/link_stubs.cpp` / `shared/stubs_impl.cpp`**
  — both sides removed different now-dead stubs from these files as their
  respective facade-wiring/shadow-type fixes landed. Likely fine, but these
  files are exactly the place a bad merge silently resurrects a stub that one
  side already deleted, or drops one that's still needed by the other side's
  new callers. Run `nm -u` over the built binary afterward and confirm zero
  unexpected undefined symbols, same as both sessions did per-commit.
- **`PROGRESS.md`** — expect a straightforward textual conflict (both sides
  appended to the Priority lists and the session log independently). Resolve
  by keeping both sides' content; do not let one session's log entries
  silently disappear.

Recommended approach: don't do a blind `git merge`. Rebase or merge, then for
each of the 7 files above, read the actual resulting diff against both
parents before trusting it — this is exactly the kind of "assembly is
evidence, not a source-code template" situation CLAUDE.md already asks for:
verify the merged behavior against both sides' original intent, not just
whether the merge tool exited 0.

After merging: `meson setup build && meson compile -C build && meson test -C
build && meson test -C build --suite integration`, and the MinGW typecheck
(`ninja -C build-mingw -k 0`) diffed against both parents' baselines.

## 3. Stale, do-not-merge-as-is branches (fetched, not integrated)

Also present on `devbox` (already fetched into local remote-tracking refs):
a family of `pi-fabric/{impl,repair}-{enter-mode3,mode3-host-bootstrap,mode3-tests}-*`
branches, all forked from a single commit dated **2026-07-31** —
`3d50c5e5`, ~80 commits behind current `main`. These look like an earlier,
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
Phase 5(b) of this session's shim work (commit `fb5680d`) **deliberately
rejected** hooking `CGWND_InitAllSubsystems` as a DirectDraw bring-up point,
because direct investigation found the original bring-up chain
(`ResourceManager::Init()` → `DDRAW_GetSurface`) has zero real callers on the
host build today — inserting a call there would fabricate a call site with
no evidence in the original control flow. Before reviving anything from
these branches, check whether they made that same mistake (fabricating a
bring-up path) — they predate that finding by two weeks.

`impl-mode3-host-bootstrap`/`repair-host-mode3` add a brand-new
`sdl3_mode3_bootstrap.cpp` that appears to drive real mode-3 rendering. If
this ever gets revived, it **must** be checked against the `UIPANEL_BeginPaint`
finding in §4 first — driving real paint calls with a wired `g_primary_surface`
and no working `GetDC` will make the process `ExitProcess(1)` itself after
~10 seconds.

Given the age and the duplicate impl/repair structure, treat these as
**reference material for what was already tried and abandoned**, not as
work to merge. Confirm with the user before reviving any of them — don't
assume they're the "reverse engineering agent's work" the user means: these
remote-tracking refs were already present locally *before* `git fetch devbox`
was run this session (the user's own cue — "needs to be fetched" — pointed
at something not yet visible locally). The one thing the fetch actually
changed was `devbox/main` moving from `9fb8142` to its current tip; the
19-commit divergence in §1 is the current, active work.

## 4. Critical blocker: do not wire `g_primary_surface`/`g_backbuffer` yet

Found this session while migrating `ui/UIPANEL.cpp` (not committed — reverted
after this was found, see PROGRESS.md's 2026-08-14 entry for full detail):

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
the stale mode3 branches in §3) wire these globals without addressing this
first — it would turn a currently-safe no-op into a reproducible crash days
or weeks later when someone least expects it.

`ui/UIPANEL.cpp` still has 8 more raw vtable-slot dispatch sites beyond the
blocked `GetDC` one (`Unlock`×1 in `UIPANEL_EndPaintEx`, `Blt`×7 in
`UIPANEL_Render`) — clean, low-risk, evidenced conversions, deliberately left
alone so the file doesn't end up half-migrated again. Convert all of them
together once `GetDC` has a real answer. `world/tilemap.cpp` also has raw
vtable dispatch on `g_primary_surface`, not yet examined at all.

## 5. Where to read more

- `PROGRESS.md`, Priority-1, the "DirectDraw shim" bullet and its
  2026-08-14 session-log entries — the full evidence trail for everything
  above.
- `.claude/plans/when-advancing-to-gameplay-sunny-tome.md` — the original
  6-phase shim plan (Phases 1-4 done, Phase 5 partially done/blocked, Phase 6
  partially done).
- Project memory `project_directdraw_shim.md` (this Claude session's
  persistent memory) — a shorter cross-session summary of the same state.
- `NOTE-directx-sdk.md` — sourcing guardrail for DirectDraw SDK evidence
  (headers only, never the real `ddraw.dll` implementation or SDK sample
  code).

## 6. Suggested next steps, in order

1. `git fetch devbox` (confirm it pulls something — it was stale once
   already this session).
2. Reconcile local `main` and `devbox/main` per §1-2. Verify, don't trust a
   clean merge tool exit code, especially on `platform/sdl3_types.h` and
   `graphics/LOCOBITMAP.cpp`.
3. Full build + test (`meson test -C build`, `--suite integration`) and
   MinGW typecheck diff against both parents' baselines.
4. Do **not** attempt to wire `g_primary_surface`/`g_backbuffer` — that
   needs the `GetDC` prerequisite in §4 resolved first, which is real,
   separately-scoped GDI work.
5. If there's still appetite for `ui/UIPANEL.cpp`'s remaining raw-dispatch
   cleanup, the `Unlock`/`Blt` sites in §4 are safe to convert whenever
   convenient — they don't depend on `GetDC`.
6. Leave the branches in §3 alone unless the user specifically asks to
   revive one — confirm which problem they're meant to solve first, since
   the codebase has moved substantially since they were written.
