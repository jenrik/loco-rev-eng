# Testing

The root suite has two layers:

- `make test` runs deterministic component and host-boundary regressions.
- `make test-integration` builds and drives `build/lego_loco` through an
  isolated Wayland/Sway sandbox with pytest.
- `make test-all` runs both layers.

## Component regressions

- `make test-cgwnd-entermode3` — CGWND_EnterMode3(2) safe early return and symbol
  ownership. Links against the real `CGWND.o` and provides only the one global
  (`g_game_mode`) that the mode-2 branch touches. All other undefined symbols
  are left unresolved, isolating the early-return contract without pulling in
  the full game-object graph. Proves that CGWND_EnterMode3 is a C++ free
  function (mangled `_Z16CGWND_EnterMode3i`) owned by `core/CGWND.h`, not
  `extern "C"`.

## GUI integration artifacts

Every GUI test gets a fresh compositor and game process. Artifacts are retained
under `build/test-artifacts/gui-*/<test-name>/`:

- screenshots for each important visible state;
- `events.jsonl`, the passive host-only state/presentation event stream;
- `interactions.jsonl`, logical/display coordinates and test actions;
- game stdout/stderr and sandbox lifecycle logs.

Override the destination when an agent or CI job needs a known path:

```bash
python3 -m pytest tests/integration --gui-artifacts-dir /tmp/lego-loco-gui
```

Screenshots are evidence and debugging artifacts, not golden-image assertions.
The driver moves the compositor cursor to the top-left before capture to reduce
obstruction, but no test depends on cursor pixels. State transitions are
asserted through passive JSONL events emitted only when
`LEGO_LOCO_TEST_EVENTS` is set. User input still travels through
`gui-sandbox`, so the tests exercise the real Wayland-to-SDL event path.

A missing sandbox mapping, premature process exit, signal/crash, absent render
state, or timeout fails the test. GUI tests require the unpacked game assets at
`lego-loco-unpacked/art-res` and never fall back to the ambient desktop.
