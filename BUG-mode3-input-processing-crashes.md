# BUG: Mode-3 (Town) mouse input processing crashes on real dispatch

**Status:** A real mouse move + left click in mode 3 now runs the full
`Game::UpdateInputState` → `HandleLeftClick` chain — DDRAW_Building/
RESDATA_ScriptedObject hit-testing, resource lookup, and building-panel
construction — cleanly through **six** separate landmines (all fixed this
pass), stopping only at an already-tracked, deliberately-deferred stub
(`UI_ChildWindow`'s host path — PROGRESS.md, "not yet ported to a
non-Windows receiver type") rather than a crash. Not "fixed" in the sense
of "a real click now fully works end to end" — it stops at a loud, honest
assert, not a working click — but every landmine on the path there is
gone.
**Discovered:** 2026-08-09, while fixing and regression-testing
`BUG-mode-3-render-freeze.md` at commit (pending — same session).
**Continued:** 2026-08-09, same day, second pass (systematic-debugging
re-run) — see "Second pass" below for the additional five fixes.

## Relationship to BUG-mode-3-render-freeze.md

That bug's root cause — `PumpMessages_SDL3` never translated SDL mouse
events into `Game`'s input fields once mode 3 was entered, so
`Game::Update()`'s `has_event` gate never went true — is fixed and verified
(`town_input_dispatched` host_test event observed firing with
`game_mode: 3` for real SDL mouse motion; see the fix's commit and
`tests/integration/test_game_gui.py::test_singleplayer_mode3_mouse_input_reaches_game`).

Fixing it exposed this **separate** bug: `Game::Update()`'s downstream
consumers (`UpdateCursorMode` → `UpdateInputState`/`HandleCursorHover`, and
by extension `HandleLeftClick`/`HandleRightClick`) had never executed with
real input in the SDL3 host build, because nothing ever reached them. They
are marked `Status: INTEGRATED` in `core/Game.cpp`, meaning validated
against the disassembly — but only as *decompilation*, never as a
*live-executed host build*, since no test or manual play session ever
drove mode-3 input before now. Two crash classes surfaced immediately:

1. **Dangling `extern` globals with no defining translation unit**,
   permitted to link by `-Wl,--unresolved-symbols=ignore-all`
   (`meson.build`, tracked as "LINK-001, remaining" since before this
   session) and resolving to a null GOT slot. The *first* runtime read
   dereferences null and segfaults. This is a data-symbol analogue of the
   project's already-tracked "~400-site call 0 landmine sweep"
   (`PROGRESS.md`) — same shape (declared, never defined, silently
   permitted to link, crashes the instant it's actually reached), but for
   data instead of function calls, and **not currently audited/tracked
   anywhere** the way the call-0 sweep is.
2. **Genuinely uninitialized/unconstructed subsystem state** reached for
   the first time (see "Crash 3" below) — not a dangling-extern problem at
   all, a real host-build gap in a different file.

## Confirmed crash sites (this session)

All three were hit in sequence while iterating
`test_singleplayer_mode3_mouse_input_reaches_game` locally (not committed
in that form — see "Regression test scope" below for why the final test
stops before reaching any of them). Each was captured with
`coredumpctl debug <pid> --debugger=gdb -A "-batch -ex bt"` against a local
build; the GUI sandbox's `LEGO_LOCO_TEST_EVENTS` JSONL confirmed the crash
happened after real input had already reached `Game` (i.e. these are true
downstream consumption bugs, not the input-wiring bug).

### Crash 1 (fixed) — `g_flag_4A9F80`

```
#0 Game::UpdateInputState (this=...) at core/Game.cpp:643
#1 Game::UpdateCursorMode
#2 Game::Update
#3 GameLoop_FrameUpdate
```

`core/Game.cpp:124` declared `extern uint8_t g_flag_4A9F80;` (0x4A9F80)
with **no definition anywhere in the tree**. Ghidra confirms exactly one
xref to 0x4A9F80 in the whole binary — `Game_UpdateInputState`'s own read
(0x411B08) — and zero writers, so in retail this byte is permanently zero
(BSS default); it's a vestigial/dev-only check. Fixed by defining
`uint8_t g_flag_4A9F80 = 0;` in `shared/stubs_impl.cpp`, matching the
project's existing convention for this class of always-zero global (see
`g_placement_valid` a few lines above it, and the near-identical precedent
already documented at `shared/stubs_impl.cpp:85-87` for `g_town_overlay_*`
— "never defined anywhere... reachable only once [some other fix] stopped
being dormant, at which point every one of these reads address 0").

### Crash 2 (fixed) — `g_ddraw_drag_rect`

```
#0 (anonymous namespace)::bounds_hit (obj=0x0, x=0, y=0) at core/Game.cpp:321
#1 Game::UpdateInputState at core/Game.cpp:652 (then :667 after Crash 1's fix shifted lines)
```

Same shape: `core/Game.cpp:118` declared `extern uint8_t g_ddraw_drag_rect[];`
(0x4A9FD0) with no definition anywhere. Ghidra: one xref
(`Game_UpdateInputState`, 0x411B4C), a **direct** call into
`GameObject_PtInRect` (not through a vtable — Ghidra shows a resolved
function call, not `(**(vtable+N))(...)`), and no constructor ever runs on
that address. `bounds_hit()`'s host implementation does
`reinterpret_cast<GameObject*>(obj)->PtInRect(x, y)`, which — unlike the
original's direct call — **is** a virtual dispatch in the C++ port, so it
additionally needs a valid vtable, not just non-null storage. Fixed by
replacing the dangling array with a real default-constructed
`static GameObject g_ddraw_drag_rect_obj;` (`core/Game.cpp`) —
`GameObject::GameObject()` (0x4369D0) zeroes `screen_rect`, reproducing
the original's always-empty-rect (thus always-`false`) `PtInRect` result
exactly, while giving the virtual call a real vtable.

### Crash 3 (fixed, second pass) — `RESDATA_ScriptedObject::IsDragging`

```
#0 RESDATA_ScriptedObject::IsDragging (this=0x368b0e00, x=0, y=0)
   at world/scriptengine.cpp:983  -- panel_view(this)->point_in_rect(x, y)
#1 Game::UpdateInputState at core/Game.cpp:672
#2 Game::UpdateCursorMode
#3 Game::Update
#4 GameLoop_FrameUpdate
```

Different in kind from Crashes 1–2: `this` (0x368b0e00) is a **real,
non-null** pointer (this run's `g_scripted_object`), so this is not a
dangling-extern/null-GOT landmine. `panel_view(this)` reinterpret-casts
`this` to a fabricated 22-slot `PanelDispatchView*` and dispatches
virtually; `RESDATA_ScriptedObject` only declares 2 real virtuals
(`scriptengine.h`), so the compiler-generated vtable is far shorter than
`PanelDispatchView` assumes, and slot [2] reads garbage past it.

Ghidra's real decompile of `IsDragging` (0x449CE0) shows a **direct,
non-virtual** call: `GameObject_PtInRect(this, x, y);` — not a vtable
dispatch at all. `GameObject_PtInRect` (0x436A10, independently
decompiled) reads `this+0x08/+0x0C/+0x10/+0x14` as left/top/right/bottom
— which for `RESDATA_ScriptedObject`'s own layout is exactly
`x`/`y`/`right`/`bottom` (renamed from `field_10`/`field_14`,
`world/scriptengine.h`). Fixed by reproducing the same half-open-interval
test directly against those fields instead of resurrecting a virtual
call the original doesn't make:

```cpp
bool RESDATA_ScriptedObject::IsDragging(int32_t x, int32_t y)
{
    return x >= this->x && x < this->right &&
           y >= this->y && y < this->bottom;
}
```

See "Second pass" below for the closely related `DDRAW_Building::HitTest`
fix (same `GameObject_PtInRect` pattern, same session) and the
`RESDATA_ScriptedObject`-family real-inheritance question that fix
answered.

## Second pass (2026-08-09, systematic-debugging re-run)

Continuing past Crash 3 surfaced a cluster of closely related landmines —
all on the same `Game::UpdateInputState`/`HandleLeftClick` call path, all
fixed this pass — plus one new, already-tracked stopping point. Each was
found by literally driving a real click through the Wayland sandbox
(`GameSession.click_logical`) and following the next `coredumpctl`
backtrace, per this project's systematic-debugging process — not guessed.

### Crash 4 — `DDRAW_Building::HitTest`, same `GameObject_PtInRect` problem as Crash 3

```
#0 (anonymous namespace)::bounds_hit  [already fixed, Crash 2]
#0 DDRAW_Building::HitTest at graphics/DDRAW.cpp:673 -- sprite_view(&sub_object_1)->hit_test(x, y)
#1 Game::UpdateInputState
```

`DDRAW_Building::HitTest` (0x459D60) has the exact same shape as Crash 3:
Ghidra shows a direct, non-virtual `GameObject_PtInRect(this, x, y)`
call, not a vtable dispatch. Chasing *why* the class was declared as a
flat, non-inheriting struct (rather than fixing this one call site in
isolation) led to the real, evidence-backed fix: **`DDRAW_Building`
really extends `Panel` (which extends `GameObject`) in the original
binary.** Its constructor (0x4589B0) calls `RESDATA_BaseInit(this)`
(0x4544E0), which itself calls `GameObject_BaseCtor(this, -1, -1, 0, 0)`
directly on `this`'s own base address and then overwrites the vtable to
Panel's (0x4784C8, "VTBL_PANEL" per Ghidra's own decompiler comment) —
textbook MSVC base-then-derived constructor vtable installation. This is
exactly the pattern `core/GameView.h`/`.cpp` (`class GameView : public
Panel`, `Status: INTEGRATED`) already uses correctly for a sibling
Panel-family class — `DDRAW_Building` was just never given the same
treatment.

Fixed:
- `DDRAW_Building` split out of `graphics/DDRAW.h` into a new
  `graphics/DDRAW_Building.h` (so `core/Game.cpp`, which can't include
  the rest of `DDRAW.h` — see the ODR-split note below — can still see
  the real class and call its methods directly instead of through a
  bridge function; an earlier draft of this fix added exactly such a
  bridge and was corrected mid-session per direct feedback that it was
  "abuse" of the pattern) now declares `class DDRAW_Building : public
  Panel`.
- `HitTest` calls the inherited `GameObject::PtInRect(x, y)` directly
  (non-virtual, matching the original) instead of the dangling
  `GameObject_PtInRect` free function.
- `HitTestWithDrag` calls the inherited `Panel::HitTestChildren(x, y)`
  (0x4549E0, already implemented in `game/Panel.cpp`) instead of a
  `RESDATA_HitTestChildren` free function that three different files
  (this one, `town/Town.cpp`, `world/scriptengine.cpp`) declared with
  three different, mutually inconsistent signatures/addresses, only one
  of which (`shared/stubs_impl.cpp`, an `assert(0)` stub) actually
  linked. `HitTestWithDrag`'s own doc comment describes more than a
  single `HitTestChildren` call (child-entity/pattern-container/
  track-sprite drag checks) — it was never fully transcribed beyond this
  one-call delegation; that remains open, tracked below, not attempted
  here.
- The embedded child-entity sub-object (`sub_object_1`, `+0xE0`,
  addressed as `&this->sub_object_1` everywhere it's used, never
  dereferenced as a plain pointer) is now a real embedded `GameObject`
  member instead of an unconstructed `void*` that stayed null forever on
  this host build — gives `sprite_view(&sub_object_1)->hit_test(...)`
  (slot 2, aliases `GameObject::PtInRect` at the same slot) a real vtable
  instead of crashing on a null one.
- Two now-conflicting free-function declarations removed from
  `graphics/DDRAW.cpp` (`GameObject_PtInRect`, unused after the above;
  `GameObject_GetRelPos`, declared there with a different, wrong return
  type/calling convention than `game/Panel.h`'s own already-correct
  declaration — silently coexisting in separate TUs until
  `DDRAW_Building.h`'s new `#include "../game/Panel.h"` made both visible
  in the same TU for the first time, turning a previously-latent mismatch
  into a hard compile error).

### Crash 5 — `g_ddraw_building`/`g_town_view` were never actually constructed

Fixing Crash 4 didn't help until this was *also* found and fixed: neither
singleton was ever constructed via its real C++ constructor.
`town/sdl3_town_mode3.cpp`'s `BootstrapTownMode3Objects()` allocated raw,
zero-filled `std::array<std::byte, sizeof(T)>` storage and
`reinterpret_cast`ed it straight to `GameView*`/`DDRAW_Building*` — for
`DDRAW_Building` additionally poking one field (`type`) at a hardcoded
x86 byte offset into the raw bytes. The comment justifying this
("Constructors cannot be run: their recovered x86 layouts write through
incompatible host offsets") was accurate for the *old*, pre-Panel-
inheritance `DDRAW_Building` constructor's `#else` (`_WIN32`) branch, but
stale for its `#ifndef _WIN32` (host) branch, which only ever wrote its
own plain fields — no x86-offset pokes, always host-safe. Leaving both
objects' vtable pointers null forever (not just their x86-specific
fields) meant *any* virtual call on either — `GameObject::PtInRect` via
`HitTest`, `Panel::HitTestChildren` via `HitTestWithDrag`, anything on
`g_town_view` — crashed the instant it was reached.

Fixed: both are now placement-new constructed for real
(`::new (storage.data()) GameView()` / `DDRAW_Building()`) into the same
static, process-lifetime-owned storage, using each class's already-
verified-host-safe constructor.

This regressed `tests/host_mode3_bootstrap_test` — an isolated-object-
extraction test that previously needed no external symbols for this
function (raw-byte construction has no dependencies) and now needs the
real `GameObject`/`Panel`/`GameView`/`DDRAW_Building` constructors, plus
transitively `SetRect` (`graphics/sdl3_window.cpp`, the real SDL3-host
implementation — not `shared/defsym_stubs.cpp`'s same-named but
different-linkage, silently non-conflicting no-op stub) and
`g_empty_string` (`shared/stubs_impl.cpp`). Fixed by adding
`obj_game_view`/`obj_panel`/`obj_ddraw`/`obj_sdl3_window` to the test's
`objects:` list in `meson.build`/`tests/meson.build`, and defining
`g_empty_string` locally in the test file (matching its own existing
`g_town_view` pattern) rather than linking in all of
`shared/stubs_impl.cpp` for one byte. Verified clean under
`valgrind --leak-check=full`: 2 allocs, 2 frees, 0 leaks.

### Crash 6 — `ResourceManager::AddString`'s `INPUT_ExitGame` was a dangling extern with a real replacement sitting unused

```
#0 UI_CreateChildWindow  [now the correct, already-tracked stopping point — see below]
#0 (before this fix) INPUT_ExitGame — null function pointer call
#1 ResourceManager::AddString at resources/ResourceManager.cpp:681/703/776
#2 ResourceManager::GetById
#3 Game::PlaySound
#4 Game::UpdateInputState
```

Once Crashes 4–5 were fixed, `UpdateInputState`'s `PlaySound(0x1400)`
fallback (reached whenever nothing else hit-tested) walked into
`ResourceManager::GetById` → `AddString`, whose odd-resource-ID branches
for types 0/2/4/12/13 called a `void* INPUT_ExitGame(void*, int32_t,
int32_t)` free function declared in `resources/ResourceManager.cpp` with
**no definition anywhere in the tree** (a stale comment there claimed
"loud deferred stub in InputMgr.cpp" — no such definition exists there
either, just a comment referencing the same address) — another dangling
extern permitted to link by `-Wl,--unresolved-symbols=ignore-all`.

`input/BuildingDescriptorEditor.h` already has the real fix sitting
unused: `BuildingDescriptorEditor_Ctor(void* memory, int32_t resId,
int32_t strPtr)` is an already-implemented "placement-new compatibility
bridge" whose own doc comment explicitly names this exact
`ResourceManager::AddString` call site and the same "INPUT_ExitGame"
Ghidra misnomer — it was written for this, just never wired up. Fixed by
replacing all three `INPUT_ExitGame` call sites with
`BuildingDescriptorEditor_Ctor`.

### Where it stops now — `UI_ChildWindow`, an already-tracked deferred stub

```
#0 UI_CreateChildWindow at ui/UI_ChildWindow.cpp:110 -- assert(false, "host implementation not yet ported")
#1 BuildingDescriptorEditor::BuildingDescriptorEditor
#2 BuildingDescriptorEditor_Ctor
#3 ResourceManager::AddString
#4 ResourceManager::GetById
#5 Game::PlaySound
#6 Game::UpdateInputState
```

This is **not** a landmine: `ui/UI_ChildWindow.cpp`'s host path is a
loud, deliberate `fprintf` + `assert(false, ...)` stub, and PROGRESS.md
already tracks it in detail ("ui_childwindow.c: Windows-path
reconstructed, host path still open", 2026-08-05) — "the ChildWindow
cluster is not yet ported to a non-Windows receiver type." Porting an
entire receiver-type cluster is a real feature-completion task, not a
bug fix; per this session's own scope discipline (stop at large, tracked,
out-of-scope boundaries rather than open a new subsystem mid-crash-chase),
this is where the chase stops. A real mouse click in mode 3 now runs
cleanly through every fixed landmine above and reaches this pre-existing,
already-documented boundary — which is real, verified progress even
though it doesn't make a click fully work yet.

**Verification, this pass**: `meson compile -C build` clean (0 errors) on
a full clean rebuild; `meson test -C build` 28/30 (same 2 pre-existing
environmental failures as baseline — `embedded-mdns-discovery`/
`sdl3-net-discovery-transport`, both "Unknown device type" from the
sandboxed network namespace); `--suite integration` included and green.
Manually driven via a real Wayland-sandbox mouse move + click
(`GameSession.click_logical`), confirmed via `coredumpctl`/GDB at every
step that the crash site moved forward through Crashes 4/5/6 in sequence
and now lands precisely on the `UI_ChildWindow` assert, not a segfault.

## `Game::ScreenToWorld`'s two-word out-parameter is likely out-of-bounds UB

`void Game::ScreenToWorld(int32_t* out_xy, int screen_x, int screen_y)`
(`core/Game.cpp:1371`) writes `out_xy[0]` *and* `out_xy[1]`. Every one of
its five call sites (`core/Game.cpp:481`, `:493`, `:712`, `:795`, `:1278`)
passes `&wx`, where `wx`/`wy` are two separate `int32_t` locals — not an
array — e.g.:

```cpp
int32_t wx, wy;
this->ScreenToWorld(&wx, ...);
this->mouse_move_world_x = wx;
this->mouse_move_world_y = wy;
```

`out_xy[1]` therefore writes one `int32_t` past the single-object bound of
`wx`. Whether that lands on `wy` is compiler-stack-layout-dependent, not
guaranteed — this is UB (out-of-bounds write through a pointer to a
single object), not merely "relies on layout." It happens to work today
(`wy` reads back correctly in practice at `-O0`), but is exactly the kind
of latent memory-corruption bug that a `-O2`/reordered-locals build, a
sanitizer run, or a stack-protector canary could turn into a real crash —
on the same mouse-input path this file's other two entries are about,
now live for the first time. Fixing it needs checking what the original
0x412060 call sites actually pass (two separate output pointers? a
packed/adjacent pair the compiler already guarantees are contiguous? a
different calling convention entirely) rather than guessing; not
attempted here.

## Other dangling globals found in `core/Game.cpp`, not yet crash-confirmed

Enumerated by diffing each `.o`'s undefined symbols against every `.o`'s
defined symbols (`nm -u` / `nm --defined-only` over `build/lego_loco.p/*.o`
and `build/shims/*.o`, set-subtracted) and cross-referencing against
`core/Game.cpp`'s own undefined-symbol list. 8 more `extern`-declared,
never-defined globals exist in `core/Game.cpp` beyond the two fixed above:

- `g_object_array` (`void**`, 0x4A9998) — read in `Game::ClearMouseMode()`
  (mode-agnostic; reached whenever `IsScreensaverActive()` returns true,
  i.e. on real mouse movement in any mode), guarded by `g_object_count`
  (which *is* defined, so the loop may or may not execute depending on its
  runtime value — not verified either way).
- `g_vehicle_list`, `g_building_list` (`BuildingCollection`, 0x4854AC /
  0x485494) — read in `Game::SelectGameObject`/`DeselectGameObject`
  (reached from the left-click path). **Not just missing storage**: Ghidra
  xrefs show no initializer for either in the original binary either
  (`core/Game.cpp:252-254`'s existing TODO already flags this — "no
  initializer was located" — for the *vtable variant*, i.e. which of the
  0x477AE8..0x477BD0 family these two globals actually use is unconfirmed).
  Needs the same vtable-identification work as `BuildingCollection`'s
  existing TODO, not a quick fix.
- `g_town_overlay_bounds`, `g_second_overlay_bounds`, `g_town_overlay_threshold`,
  `g_has_town_overlay`, `g_has_second_overlay`, `g_town_click_valid` — all
  read only in `Game::HandleCursorHover()`, which `Game::UpdateCursorMode()`
  only calls for **mode 4** (build/edit), not mode 3/9 — out of scope for
  the mode-3 freeze fix and not exercised by its regression test.
  `g_second_overlay_bounds` (0x4AA818) is additionally a live instance of
  the address-aliasing problem below (it's the *same original address* as
  the already-real, already-constructed `g_town_view` global
  (`core/GameLoop.cpp:97`) — fixing it means reconciling with that
  existing object, not inventing new disconnected storage; see below).

None of the mode-4-only ones block the mode-3 freeze fix or its test.
`g_object_array`/`g_vehicle_list`/`g_building_list` might be reachable
from a real click in mode 3 (untested — Crash 3 was hit first).

## Separately: address-identity is not preserved between `Game`'s fields and other host globals

While reverse-engineering `MainWndProc` (0x4618C0) to build the
`BUG-mode-3-render-freeze.md` fix, three of `Game`'s fields were confirmed
via disassembly to live at fixed addresses `g_game (0x4854C8) + offset`
in the original binary (WndProc writes them directly by address, e.g.
`MOV byte ptr [0x48556C], 1` for `left_click_flag` at `+0xA4`). Two of
those exact addresses are **already** claimed by unrelated, independently-
defined globals elsewhere in this codebase:

- `0x48558C` = `Game::mouse_move_flag` (+0xC4) **and**
  `world/tilemap.h`'s `g_placement_blocked` (`shared/stubs_impl.cpp:99`,
  read at `world/tilemap.cpp:1566`).
- `0x4855AE` = `Game::click_on_selected` (+0xE6) **and**
  `core/Game.cpp`'s own `g_mouse_capture` (`shared/stubs_impl.cpp:135`,
  written at `core/Game.cpp:979`; DDRAW's drag-hit-test code reads the
  *original* byte at 0x45AC77/0x45AD0B per this session's disassembly).

In the original binary these are the literal same byte; in this host
build they are two unrelated pieces of memory, because `Game`'s host
fields live inside a heap-allocated C++ object, not at a fixed static
address, and `g_placement_blocked`/`g_mouse_capture` were named
independently (likely before `Game`'s true field layout was understood).
This is not a bug the `BUG-mode-3-render-freeze.md` fix introduces or
needs to fix — SDL input now reaches `Game`'s own fields correctly, which
is what that bug was about — but it means any *other* original code path
that was reading the WndProc's writes to 0x48558C/0x4855AE (rather than
going through `Game`'s own consumption in `Game::Update()`) is reading
stale/disconnected host storage instead. `world/tilemap.cpp:1566`'s
`g_placement_blocked` check is the one currently-live example; whether it
depends on ever seeing the WM_LBUTTONUP-driven value is unverified.

## Regression test scope

`test_singleplayer_mode3_mouse_input_reaches_game`
(`tests/integration/test_game_gui.py`) intentionally stops after asserting
the `mouse_move` dispatch — it does not click, and does not assert
`game.is_alive()` afterward, because *any* mode-3 mouse motion (not just a
click) reaches `Game::UpdateInputState()` every frame once `has_event` is
true (`screensaver_active` is set unconditionally on `WM_MOUSEMOVE`).
Empirically, a bare move stays safe today (`Game::PlaySound`'s
`parent_id == sound_id` early-return means the first ever `PlaySound
(0x1400)` — whichever branch reaches it first — resolves without needing
`ResourceManager::AddString`'s heavier path, apparently because 0x1400 is
already cached from startup); a real click reaches a *different* branch
that still needs `AddString` and now lands cleanly on the tracked
`UI_ChildWindow` stub (see "Second pass" above) instead of a crash — real
progress, but not yet "survives a click." Extend this test to click and
assert survival once `UI_ChildWindow`'s host path is real.

## Suggested next steps

Crashes 1–6 above are all fixed. What's left:

1. **Port (or stub more gracefully) the `UI_ChildWindow` host cluster** —
   the new stopping point, already tracked in PROGRESS.md ("ui_childwindow.c:
   Windows-path reconstructed, host path still open"). This is the next
   thing a real click needs; it's a genuine feature-completion task
   (pick a non-Windows receiver type for the ChildWindow cluster), not a
   quick fix.
2. `HitTestWithDrag`'s own doc comment describes more logic than the
   current one-call `HitTestChildren` delegation (child-entity/pattern-
   container/track-sprite drag checks) — never fully transcribed from
   0x45A740's disassembly. Not attempted this pass; low risk today since
   `HitTest` (checked first) already returns true/false correctly and
   `HitTestWithDrag` only runs after a `HitTest` hit.
3. Decide `g_vehicle_list`/`g_building_list`'s real vtable variant (fold
   into the existing `shared/collections.h` TODO already noted in
   `core/Game.cpp:252-254`) before a real click that reaches
   `SelectGameObject`/`DeselectGameObject` can be considered safe —
   not confirmed reachable from the paths fixed this session, but
   untested.
4. Once `UI_ChildWindow`'s host path is real (item 1), extend
   `test_singleplayer_mode3_mouse_input_reaches_game` to click and assert
   `game.is_alive()`, closing the loop this bug's test deliberately left
   open.
5. Consider whether the broader LINK-001 dangling-symbol sweep
   (`meson.build`'s tracked TODO) should be prioritized ahead of further
   ad hoc discovery — this bug's two sessions found 6 real crashes by
   touching exactly one call path (`Game::UpdateInputState`/
   `HandleLeftClick`); the total undefined-symbol count linked via
   `-Wl,--unresolved-symbols=ignore-all` is ~446 across the whole binary
   (functions and data), of unknown severity/reachability.
6. `RESDATA_HitTestChildren`'s three mutually-inconsistent free-function
   declarations (`town/Town.cpp`, `world/scriptengine.cpp`,
   `shared/stubs_impl.cpp`'s stub) are now fully dead in
   `graphics/DDRAW.cpp` (this fix replaced its one call site with the
   real `Panel::HitTestChildren` method) but still exist and still
   conflict in principle for whichever of those other two files a future
   change might put in the same TU as `shared/stubs_impl.cpp` or each
   other — same latent-ODR-split shape as the `g_tilemap`/
   `g_ddraw_building`/`g_primary_surface` note above, not fixed here
   since neither of those two call sites was reached by this pass's
   crash chain.
