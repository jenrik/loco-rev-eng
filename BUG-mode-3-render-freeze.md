# BUG: Game appears frozen immediately after Accept/OK on the main menu

**Status:** Fixed and verified (2026-08-09). SDL mouse input now reaches
`Game` in modes 3/9. Fixing it exposed a separate, still-open bug in
`Game`'s downstream input-processing consumers — see
`BUG-mode3-input-processing-crashes.md` and "Fix" below.
**Reported:** 2026-08-06, interactive GDB session against `build/lego_loco` at
commit `4eabbbf` ("Fix render-path call-0 landmine cluster masking town
rendering after mode 3").
**Revalidated:** 2026-08-09 against HEAD `185d54b` (~79 commits later, see
"Revalidation" below). Every substantive root-cause claim still holds;
line-number citations below have been refreshed and one piece of supporting
evidence (the call-0 landmine cross-reference) has been corrected.

## Symptom

Click Accept/OK on the main menu (mode 2 → mode 3 transition). The screen
keeps showing the main menu image forever: no town view appears, the cursor
produces no hover feedback, and no further input (mouse move, click) has any
visible effect. Looks exactly like a hang.

It is **not** caught by the automated GUI integration suite
(`meson test -C build --suite integration`, currently 12/12) because that
suite runs headless under a forced Mesa llvmpipe software renderer (see the
`wayland-gui-sandbox` skill) and never exercises the real hardware-accelerated
GLX/X11 present path this bug was diagnosed against.

## Investigation (ruled out first)

The first hypothesis — a genuine hang in the GPU present call — turned out to
be a red herring, and is recorded here so a future session doesn't re-derive
it:

- Ctrl-C into GDB three times over ~15s always landed at the identical PC:
  `ioctl` → `iris_fence_finish` → `dri_flush` →
  `loader_dri3_swap_buffers_msc` → `glXSwapBuffers` → `X11_GL_SwapWindow` →
  `SDL_RenderPresent_REAL` → `SDL3_PresentPrimarySurface`
  (`graphics/sdl3_ddraw.cpp:568`) → `PumpMessages_SDL3`
  (`core/CGWND_sdl3.cpp:185`).
- Sampling the same stack repeatedly looks like a hang but isn't — it's just
  where a healthy vsync-throttled render loop spends nearly all its time.
- Ruled out with a **hit-counting breakpoint**, not more stack samples:
  `break graphics/sdl3_ddraw.cpp:568` + `ignore 1 100000` + `continue` for
  ~5s reported **"breakpoint already hit 9310 times"** — the present call is
  firing at high frequency, so the loop is alive and well.
- Also ruled out: window occlusion (moved the game to its own dedicated,
  confirmed-`visible:true`/on-screen Sway workspace via `swaymsg -t
  get_tree`/`get_workspaces` — freeze persisted) and a kernel-level GPU hang
  (`dmesg`/`journalctl -k` show no i915/DRM reset events).
- `/proc/<pid>/stat` utime/stime and `voluntary_ctxt_switches` were both
  climbing steadily across polls — the main thread is doing real, repeated
  work, not blocked.

So the render loop runs, but the pixels it presents never change, and input
never does anything. That reframes the question: **why does no game code
ever mark anything dirty in response to input once mode 3 is entered?**

## Root cause

`core/CGWND_sdl3.cpp`'s host SDL3 event loop (`PumpMessages_SDL3`) only
forwards SDL input events into the game while in mode 2 (the main menu):

```cpp
// core/CGWND_sdl3.cpp:35-38
static EditWindow* active_host_menu()
{
    return g_game_mode == 2 ? static_cast<EditWindow*>(g_ui_main) : nullptr;
}
```

Every mouse/keyboard case in the event switch (`SDL_EVENT_MOUSE_MOTION` at
line 103-108, `SDL_EVENT_MOUSE_BUTTON_DOWN` at line 109-122, etc.) only does
anything when `active_host_menu()` returns non-null, i.e. only in mode 2.
No mode-3/mode-9 counterpart exists anywhere in the tree today — the only
other definitions of `hostHandlePointer`/`hostHandleKey`/`hostHandleTextInput`
are on `EditWindow` and `GameSetupPanel`, both mode-2-only menu classes.
Once Accept transitions to mode 3, `active_host_menu()` returns `nullptr` and
**every SDL input event is silently dropped** on the floor — there is no
equivalent host-side wiring that feeds mode-3 (Town/gameplay) input into the
game state.

Confirmed by breakpoint at the data level:

- `((Game*)g_game)->Update()` (`core/GameLoop.cpp:412`, called every frame
  from `GameLoop_FrameUpdate`, `core/GameLoop.cpp:352`) **is** being called
  every frame — verified hit at `core/Game.cpp:417`, with `this->visible ==
  1` so it doesn't bail at the `core/Game.cpp:419` guard.
- But `Game::Update()`'s gate at `core/Game.cpp:427`:
  ```cpp
  bool has_event = (this->left_click_flag != 0) ||
                   (this->mouse_move_flag != 0) ||
                   (this->right_click_flag != 0) ||
                   (this->screensaver_active != 0);
  ```
  was observed `false` on every call — `this->mouse_move_flag` and
  `this->left_click_flag` both read `0` at the breakpoint, with mouse motion
  actively happening on the host side at the time.
- Breakpoints on `Game::UpdateInputState()` and `Game::HandleCursorHover()`
  recorded **zero hits** over a 5-second window of active mouse movement,
  confirming `has_event` never goes true and `UpdateCursorMode()`
  (`core/Game.cpp:491`) is never reached.
- `grep -n "mouse_move_flag\s*=" core/Game.cpp` shows `mouse_move_flag` is
  only ever assigned `0` (cleared) anywhere in the current tree — there is no
  code path, host or original, that currently sets it to `1`.
  `left_click_flag = 1` has exactly one site, `core/Game.cpp:882`, deep
  inside a drag-release re-trigger path that itself requires
  `left_click_flag` to already be meaningfully in play — not an external
  input entry point.

Everything else in the per-frame heartbeat keeps running normally in mode 3
— `NETMAN_Update`, `RESMGR_VehicleAnimationTick`, `World_UpdateTick`,
`RESDATA_ScriptedObject_Update`, `Town_TrackBuilding`,
`DDRAW_UpdateBuilding`, `TileMap_InvalidateDirtyRects`
(`core/GameLoop.cpp:352-436`, as of the 2026-08-09 revalidation — was
333-420 at diagnosis; pure line-number drift from unrelated intervening
commits, same call structure) — which is exactly why the process passes every
liveness check (alive, ticking, presenting frames) while looking completely
frozen on screen: nothing ever marks a tile or UI element dirty, because
nothing ever tells `Game` that input happened.

This is likely connected to (but distinct from) the project's already-tracked
`call 0` landmine sweep (PROGRESS.md, "~400-site call 0 landmine sweep").
**Update, 2026-08-09 revalidation:** at diagnosis time, `Town::on_mouse_move`,
`Cursor::handle_toolbar_hover`, `EditWindow::netPanelWndProc`, and
`HelpWnd::handle_mouse_move` all contained unresolved-symbol (`call 0`)
landmines per `objdump -d build/lego_loco | grep -E "call\s+0 <"`. Since
then, the landmine-sweep commits (`dab8fce`/`adbca85` Cursor-family cluster,
`2ffda7b` Town_Draw*/TileCache cluster, and the EditWindow/UIPANEL-family
fixes) have **fully implemented the first three** — they are no longer
landmines (0 `call 0` sites in each today). `HelpWnd::handle_mouse_move`
**remains landmined** (4 `call 0` sites), and the root cause is now
identified precisely rather than just observed: `ui/HelpWnd.h` declares a
same-signature `void set_mode(void* countPtr, void* dataPtr, int modeA, int
modeB)` overload (documented there as vtable[3], "inherited from
GameWindow") that shadows the real inherited `GameWindow::set_mode(...)` via
the class's own `using GameWindow::set_mode;`. `ui/HelpWnd_stubs.cpp`
explicitly states "No separate `HelpWnd::set_mode` stub is needed" — but
`handle_mouse_move` calls exactly this declared-but-never-defined overload,
so every call resolves to `call 0`. This doesn't change the primary root
cause of this bug (the missing host-side SDL→`Game` flag wiring remains the
actual blocker), but it should be folded into the same implementation pass
if `HelpWnd::handle_mouse_move` turns out to sit on the real mode-3/9
dispatch path — either by giving `HelpWnd` a real `set_mode(void*, void*,
int, int)` override or by removing the shadowing declaration and letting it
fall through to the inherited `GameWindow::set_mode`, whichever the assembly
at 0x414340 supports.

## Suggested fix (not yet implemented)

`PumpMessages_SDL3` needs a mode-3 (and likely mode-9) counterpart to
`active_host_menu()`/`EditWindow::hostHandlePointer` that translates SDL
mouse motion/button events into `Game`'s input fields
(`mouse_move_flag`/`mouse_move_screen_pos`, `left_click_flag`/
`left_click_screen_pos`, `right_click_flag`, and the packed-position fields
referenced throughout `core/Game.cpp`), matching whatever the original
WndProc dispatch does for `WM_MOUSEMOVE`/`WM_LBUTTONDOWN`/`WM_RBUTTONDOWN` in
modes 3/9 — this needs verifying against the disassembly before
implementing, per this project's evidence-first convention, rather than
guessed from the host-side field names alone. `Town::on_mouse_move` and
`Cursor::handle_toolbar_hover` were call-0 landmines at diagnosis time and
have since been fully implemented (see "Update, 2026-08-09 revalidation"
above) — they're worth re-reading as reference for what the real dispatch
path looks like when implementing this fix, since they sit on what looks
like the same call chain. `HelpWnd::handle_mouse_move`'s still-landmined
`set_mode` call should be checked as part of the same pass.

A regression test should drive a real mouse-move/click through the SDL
sandbox in mode 3 and assert some observable side effect (e.g. a
`host_test` event, or a dirty-rect/hover-state change) — the current 12/12
integration gate has no such test, which is how this shipped unnoticed.

## Revalidation (2026-08-09)

Re-checked every claim above against current HEAD `185d54b`, ~79 commits
after the original 2026-08-06 diagnosis at `4eabbbf`. Findings:

- **Root cause unchanged.** `active_host_menu()`, the SDL event switch's
  mode-2-only gating, and `Game::Update()`'s `has_event` gate are all
  structurally identical to diagnosis time; only line numbers drifted from
  unrelated intervening commits (STRICT=2 old-style-cast cleanup, an
  allocation-size fix) — none of them touched this control flow. Confirmed
  by repo-wide grep that `mouse_move_flag`/`right_click_flag` are still
  never assigned `1` anywhere, and `left_click_flag = 1`'s only site
  (`core/Game.cpp:882`) is still not an external input entry point.
- **Regression-test gap still real.** `test_singleplayer_accept_reaches_mode3_without_crashing`
  (`tests/integration/test_game_gui.py`) does all its clicking before mode 3
  is entered and asserts nothing about input once inside it;
  `GameSession` (`tests/integration/gui_sandbox.py`) has no mouse-move/hover
  helper at all.
- **Supporting evidence partially corrected** — see the "Update, 2026-08-09
  revalidation" note above the "Suggested fix" section: 3 of the 4
  referenced call-0 landmines are now fixed; `HelpWnd::handle_mouse_move`'s
  is now root-caused precisely (a shadowing `HelpWnd::set_mode` declaration).
- **Housekeeping finding, now fixed:** this bug report and its
  `PROGRESS.md` session-log entry were stuck mid-`git stash pop` — neither
  was present in any commit, and `PROGRESS.md` had literal unresolved
  conflict markers on disk. Resolved by splicing the stashed entry into its
  correct chronological position in `PROGRESS.md` and dropping the stash;
  see the `2026-08-09 (mode3-render-freeze-revalidation)` session-log entry
  there for the full writeup.

Status at the time of revalidation was **root cause found, not yet fixed**
— the suggested fix above was unchanged in substance and still needed
assembly verification before implementation. See "Fix" below for what was
actually implemented, once that verification was done.

## Fix (2026-08-09)

`MainWndProc` (0x4618C0) was decompiled in Ghidra for the first time (it
had never been transcribed anywhere in this tree — `core/CGWND.cpp` only
points a `_WIN32`-only `WNDCLASS.lpfnWndProc` directly at that raw address,
per this project's Wine/binary-patch model, so no C++ reconstruction of it
existed to consult). Its game-mode (non-menu) mouse dispatch was verified
against the disassembly at these exact offsets, resolving the "needs
verifying against the disassembly" open question from the suggested-fix
section above:

- `WM_MOUSEMOVE` (0x4622D8-0x4622DF): `Game::screensaver_active = 1` (+0x8E),
  `Game::packed_mouse_pos = lParam` (+0x90).
- `WM_LBUTTONDOWN`/`WM_LBUTTONDBLCLK` (0x462387-0x46238E):
  `Game::click_on_selected = 1` (+0xE6), `Game::left_click_flag = 1` (+0xA4),
  `Game::left_click_screen_pos = lParam` (+0xA8).
- `WM_LBUTTONUP` (0x4623B2-0x4623C0): `Game::click_on_selected = 0`,
  `Game::mouse_move_flag = 1` (+0xC4), `Game::mouse_move_screen_pos = lParam`
  (+0xC8) — WM_LBUTTONUP, not WM_MOUSEMOVE, is what actually drives the
  `mouse_move_flag` pipeline documented in `core/Game.h`/`Game::Update()`;
  it also clears `mouse_drag_mode`, consistent with ending a drag on
  release.
- `WM_RBUTTONDOWN`/`WM_RBUTTONDBLCLK` (0x4623E4-0x4623EB):
  `Game::right_click_flag = 1` (+0xB4), `Game::right_click_screen_pos =
  lParam` (+0xB8).
- `WM_RBUTTONUP` (0x46240F-0x462416): `Game::mouse_drag_flag = 1` (+0xD4),
  `Game::mouse_drag_screen_pos = lParam` (+0xD8).

These are literal fixed-address writes (e.g. `MOV byte ptr [0x48556C], 1`
for `left_click_flag`) — decisive proof that `g_game` (0x4854C8) is a
statically-addressed object in the original, not merely a runtime pointer
value, and that `Game`'s fields are individually addressable at
`0x4854C8 + offset` at compile time in the original binary. (This also
surfaced that two of those exact addresses are separately claimed by
unrelated globals elsewhere in this host codebase — see the "address
identity" section of `BUG-mode3-input-processing-crashes.md`.)

`core/CGWND_sdl3.cpp` gained `active_host_game()` (mode 3/9 counterpart to
`active_host_menu()`) and `host_pack_game_lparam()` (projects SDL's
display-space float coordinates into the primary-canvas pixel space via
`SDL3_DisplayToPrimaryCanvas`, the same conversion
`EditWindow::hostHandlePointer` uses for mode 2), then wired
`SDL_EVENT_MOUSE_MOTION`/`SDL_EVENT_MOUSE_BUTTON_DOWN`/
`SDL_EVENT_MOUSE_BUTTON_UP` to reproduce the five writes above exactly,
each annotated with its source address. `SetFocus`/`SetForegroundWindow`
(also present in the original `WM_LBUTTONDOWN` case) were intentionally
not reproduced — pure Win32 window-manager focus/z-order with no game-state
effect, out of scope under a Wayland compositor.

A new passive host_test event, `town_input_dispatched`
(`stubs/host_test_events.h`/`.cpp`), fires whenever this translation
writes into `Game`'s fields, giving a direct observable signal that "SDL
input reached the Game object" — the exact thing this bug's root cause
broke. `tests/integration/test_game_gui.py::test_singleplayer_mode3_mouse_input_reaches_game`
drives a real mouse move through the Wayland sandbox after reaching mode 3
and asserts this event fires with `game_mode: 3`, closing the "current
12/12 integration gate has no such test" gap called out above. (A new
`GameSession.move_logical()` helper was added to `gui_sandbox.py` for
this, using the existing `gui-sandbox move` pointer-motion primitive that
`screenshot()` already relied on to park the cursor.)

Verified live: `coredumpctl`/GDB confirmed `Game::UpdateInputState()` —
previously unreachable, per the original investigation's "zero hits" — now
executes on every real mouse move in mode 3, and the
`town_input_dispatched` event was observed firing with `game_mode: 3` in
the GUI sandbox.

Fixing the wiring immediately exposed that `Game`'s downstream input
consumers (`UpdateInputState`, and by extension the rest of the
click/hover pipeline) had never executed in this host build with real
input and crash on several never-before-reached code paths — tracked
separately as `BUG-mode3-input-processing-crashes.md` rather than folded
into this fix, since it turned out to span multiple unrelated subsystems
(dangling-extern globals in `core/Game.cpp`, and a distinct uninitialized-
state bug in `world/scriptengine.cpp`) rather than a small, contained
follow-up. Two of the three crash sites found so far (both dangling
`extern` globals with no defining translation unit anywhere in the tree,
permitted to link by `-Wl,--unresolved-symbols=ignore-all`) were fixed
alongside this change since they sat directly in `Game::UpdateInputState`,
the exact function this fix newly makes reachable: `g_flag_4A9F80`
(`shared/stubs_impl.cpp`) and `g_ddraw_drag_rect` (`core/Game.cpp`, now a
real default-constructed `GameObject`). The regression test above
deliberately stops at the `mouse_move` dispatch assertion rather than also
clicking and asserting the game survives, precisely because survival is
gated on that separate, still-open bug.
