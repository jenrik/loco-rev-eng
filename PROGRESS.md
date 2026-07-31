# PROGRESS.md — Lego Loco Reverse Engineering Progress

> Durable cross-session record. Update after each significant milestone.
> See AGENTS.md § "PROGRESS.md" for usage conventions.

## Project overview

Reverse-engineering Lego Loco (loco.exe, 1998, MSVC x86) into idiomatic C++.
The Ghidra database is the single source of truth; all code derives from assembly.

**Binary**: `lego-loco-unpacked/Exe/loco.exe` (PE32, ~2,001 functions)

---

## Completed

### Phase 1: Decompilation foundation (2026-07-23)

- [x] Ghidra database created and analyzed
- [x] Directory structure: `src/decompiled_cpp/{core,game,graphics,audio,input,network,resources,town,ui,world,shared,stubs}`
- [x] Shared headers: `types.h`, `vtable_addrs.h`, `Collection.h`, `TimerSlotList.h`
- [x] Stub headers for platform types: `compat.h`, `windows.h`, `ddraw.h`, `dsound.h`, `dplay.h`
- [x] Makefile with stub-based compilation on Linux (GCC + `-fpermissive`)
- [x] MinGW cross-compilation support (`make MINGW=1`)

### Phase 2: Core class decompilation (2026-07-23)

- [x] **GameObject** — root base class (vtable 0x477820, 6 slots)
- [x] **Entity** — intermediate base (vtable 0x477488, 15 slots)
- [x] **Building** — central game entity (vtable 0x477EB8, size 0xF4)
- [x] **BuildingComplex** — BuildingMgr core (vtable 0x477F70, size 0x7C)
- [x] **BuildingMgr** — factory class (0x434500)
- [x] **Train** — extends Building (size 0xF0)
- [x] **CGWND** — main window/application class
- [x] **Game** — game mode manager
- [x] Supporting classes: Vehicle, BuildingPanel, ScriptedObject, World, etc.

### Phase 3: Assembly validation (2026-07-24)

- [x] Ghidra database reopened, all function addresses cross-referenced
- [x] **Building** validated: field offsets, control flow, all 18 methods checked
  - Fixed: `CalcMoveTarget` (`__ftol` semantics), `AddOccupant` return type, `RemoveOccupant` signature
  - Confirmed: Deserialize address 0x435700 (was incorrectly reported as missing)
  - Added: missing `field_ec` at +0xEC, compile-time layout assertions
- [x] **BuildingComplex** reclassified: 0x477F70 is BuildingMgr vtable, not timer coordinator
  - Fixed: embedded lock objects (0x1C bytes), keyed collections (0x18 bytes)
  - Removed: false 0x478008 vtable claim
- [x] **BuildingMgr** canonicalized: factory at 0x4349D0, UpdateAll at 0x434720
  - Reclassified BuildingMgrObjectGroup as ResourceGameObject
- [x] **Train** validated: allocation 0xF0 (not 0xF4), TrainSubsystem implemented
  - Fixed: network offsets, message ownership, upload state machine
- [x] **CGWND** validated: field offsets corrected, non-member functions reclassified
  - Fixed: ResetState buffer size (0x1000), retail/demo styles, AudioChannel polarity
- [x] **Entity/GameObject** validated: vtable layouts, field placement, virtual slots
- [x] **All 69 C++ files**: `make check` → 69 good, 0 need work

### Phase 4: Code quality cleanup (2026-07-24)

- [x] **Literal vtable access removed** — 0 VTBL_ references in code (all remaining in comments/docs only)
- [x] **Raw pointer arithmetic removed** — 0 `(uint8_t*)this +` patterns in C++ code
- [x] **`this->vtable` access removed** — 0 direct vtable member access in code
- [x] **C++ symbols in `extern "C"` fixed** — 56 files cleaned, all `operator_new`/`GLOBAL_free`/`GameObject_*` moved to C++ linkage
- [x] **SDL3 port artifacts removed** — `src/decompiled_cpp/port/` and `src/port/sdl3/` deleted
- [x] **`.bak` files removed** — deleted stale backup files
- [x] **Build system cleaned** — Root Makefile established; `flake.nix` simplified

### Phase 5: SDL3 compatibility shims (2026-07-24)

- [x] **`src/sdl3_shims/`** — isolated directory, zero modifications to decompiled code
- [x] **`sdl3_types.h`** — shared DirectX struct definitions (DDSURFACEDESC, WAVEFORMATEX, etc.)
- [x] **`sdl3_ddraw.h/cpp`** — IDirectDraw4 + IDirectDrawSurface4 via SDL3 Renderer (419 lines)
  - GPU blits via `SDL_Texture`, CPU access via `SDL_Surface` Lock/Unlock
  - Color key support via `SDL_SetTextureColorMod`
  - BMP loading with automatic palette→RGBA expansion at load time
- [x] **`sdl3_dsound.h/cpp`** — IDirectSound + IDirectSoundBuffer via SDL3 Audio (307 lines)
  - WAV parsing, playback via `SDL_AudioStream`, volume conversion (DirectSound→SDL scale)
- [x] **`sdl3_dplay.h`** — DirectPlay 4 stub (all methods return success, no networking)
- [x] **Build system** — `Makefile` (auto-detects Nix SDL3 paths) + `CMakeLists.txt` (`find_package`)
- [x] **`sdl3_window.h/cpp`** — Win32 windowing → SDL3 (838 lines)
  - Window creation: CreateWindowExA, RegisterClassA, ShowWindow, SetWindowPos
  - Message loop: PeekMessageA, GetMessageA, DispatchMessageA (SDL events → Win32 messages)
  - GDI stubs: GetDC, BeginPaint, BitBlt (drawing done via DirectDraw)
  - Timers: SetTimer/KillTimer via SDL_AddTimer
  - MessageBox, registry, version info, COM: stubs
- [x] **README.md** — purpose, architecture, palette strategy, limitations
- [x] All 3 shims compile clean against SDL3 3.4.10
- [x] **CRT/helper link stubs** — host allocation/PRNG wrappers, no-op subsystem helpers, and weak executable globals in `shared/crt_stubs.cpp`
- [x] **RFD/RFH archive reader + startup renderer** — `sdl3_shims/resource_archive.{h,cpp}` parses all 2,522 shipped archive entries and decodes real Huffman-compressed BMPs exactly as `Huf_Decode` (0x45C830); `pe_string_table.{h,cpp}` resolves original `LoadStringA` resource IDs; `make test-menu-sprite-viewer` renders a real startup-menu frame through SDL textures.
- [x] **ResourceManager SDL3 asset bridge** — Replaced null `ResourceManager_Init`/`ResourceManager_GetById` stubs with `resource_manager_sdl3.cpp`: actual PE-ID → RFH/RFD asset lookup, decoded SDL BMP surface cache, paired DAT animation metadata, DirectDraw-compatible magenta color key, and verified 0x421500/0x4216F0 object ABI. Typed archive loading covers WAV, BUT, ANI, LAY, SAV, and INI blobs; `make test-resource-manager-sdl3` validates BMP/DAT/WAV/BUT/ANI assets.
- [x] **Menu sprite typed cleanup** — `EditWindow` and `menu_sprite_viewer` now use typed `SpriteResource`/`SpriteBitmap` accessors: no raw sprite offsets or direct resource-vtable calls remain in those paths. The only remaining resource vtable table is a documented bridge for untranslated callers.
- [x] **SDL primary-surface presentation** — `CGWND_sdl3` now presents the SDL DirectDraw primary render target instead of clearing an unrelated blue frame. `sdl3_ddraw` owns lazily-created primary/backbuffer targets and `make test-sdl3-primary-present` verifies their pixels reach the SDL window.
- [x] **Mode-2 EditWindow bootstrap** — Host `CGWND::InitAllSubsystems` constructs the real first binary subsystem (`EditWindow`, resource 0x1F8) and mode 2 calls its typed `show()` method. The unsupported Town/Cursor/etc. chain remains Windows-only rather than fabricating raw objects.
- [x] **Enabled-source build restored** — Fixed stale class labels, host-only x86 layout assertion, DirectDraw stub linkage/signature corruption, and remaining declaration mismatches. Root `make` now compiles and links every enabled source into `build/lego_loco`.
- [x] **DPlayConfig initialization** — Reconstructed `GameConfig_constructor` (0x440C60) from Ghidra as the 0xB0-byte DPlayConfig defaults at DAT_004FD3A8. The legacy menu alias now points at initialized storage; `make test-dplay-config` verifies all recovered defaults.
- [x] **Typed TrainSubsystem bootstrap** — Replaced EditWindow’s null `TrainSubsystem_Ctor` pointer call with the existing typed `TrainSubsystem` constructor (0x438BC0). SDL’s DirectPlay boundary now reports an empty provider list, preserving the original constructor flow without a Win32 DirectPlay runtime.
- [x] **SDL UIPANEL composition boundary** — Grounded the original `UIPANEL_Render` control flow (0x426EB0) and replaced its unsafe x86-vtable host call with typed composition of decoded EditWindow sprites into the SDL primary target. The primary-surface regression now verifies a source surface blit reaches the presented window.
- [x] **Mode-2 Enter-to-lobby crash** — Core `lego_loco.core` showed `GameSetupPanel::hostRenderFrame` calling `__stack_chk_fail` after the player grid. The cause was an unresolved C++-mangled `SDL3_GetRenderer` declaration under the permissive linker; corrected it to the shim’s C linkage and added `make test-host-menu-renderer-linkage`.
- [x] **Mode-2 multiplayer lobby artwork** — Recovered `GameSetupPanel::show` (0x408F70), `render` (0x409280), and `drawGrid` (0x409980): SDL composes the archive-backed `startup\apback.bmp` background (0x439), frame-0 Exit/Search/Options controls (0x42C/0x429/0x42B), and frame-1 crops from all nine `aplayer0..8` slots (0x43A..0x442) at original-derived geometry.
- [x] **Mode-2 multiplayer lobby controls** — Recovered `GAMESTATE_HandleClick` (0x40A4E0) and routed SDL pointer events to a guarded `GameSetupPanel` adapter: Exit returns through state 7, Options preserves its state-2 return then uses the available host main-menu compositor, and Search visibly reports the SDL DirectPlay boundary’s empty session result. `make test-host-multiplayer-menu-input` guards routing and platform isolation.
- [x] **Mode-2 multiplayer entry selection** — Preserved the original `EditWindow_drawButtons` (0x422010) state-7/provider-list gates, including its known-good right-hand `0x409` path, while recording the selected destination separately on the SDL host. PE resource evidence identifies `0x407/0x408` as singleup/singledown and `0x409/0x40A` as multipleup/multipledown; the right-hand selection enters state 5 after name acceptance. The GameSetup compositor labels state 4 as scenario setup and state 5 as Network Game; isolated-Wayland coverage captures selection → state-5 lobby → empty search → exit.
- [x] **Mode-2 main-loop handoff** — Removed EditWindow’s raw slot-4 vtable dispatch. Assembly at `0x426020` proves its first-show `(nullptr, 0, nullptr, false, true)` invocation returns immediately from the constructor-established null render target; mode 2 now reaches `CGWND_PumpMessages` without a fault.
- [x] **Mode-2 backdrop canvas and blits** — Recovered `EditWindow::render` (0x4216F0) from Ghidra and host-composed resources `0x413`, `0x444`, `0x445`, `0x446`, and `0x443` at their verified native coordinates. The SDL primary is now a fixed 1280×1024 logical canvas projected pixel-perfect when it fits or aspect-fitted when it does not; regressions prove real archive pixels reach an 800×600 display.
- [x] **Mode-2 menu controls** — Host-only `#ifndef _WIN32` composition now follows the state-0/state-7 draw sequence at `0x421C31`/`0x422010`, renders default menu controls and recovered option-selected frames, and maps SDL pointer events through the exact canvas projection before applying recovered `0x422C60..0x422D2E` state transitions.
- [x] **Mode-2 quit control** — Ghidra confirms the `+0x14C` control’s click path (`0x422AC3..0x422C5D`) calls `CGWND_SetMode(10)`. The host routes that scaled hit to the real reconstructed mode-10 transition and terminates the SDL pump, matching the original WM_CLOSE handoff.
- [x] **Mode-2 player-name input** — Recovered native EDIT creation (`0x4204D0`), its unlabelled subclass (`0x420B20`), and Enter/Escape parent paths (`0x420D57`/`0x420C19`). SDL now renders the exact `+0x15C` logical rectangle, receives text/backspace/Enter/Escape through host-only code, preserves the 11-byte limit and original validation, and copies accepted names to PlayerConfig `+0x06`.
- [x] **Mode-2 main-menu accept control** — Disassembly at `0x422AB2` confirms the pressed resource-`0x403` control directly calls `EditWindow::onPlayerNameChanged` (`0x422660`). Its guarded SDL handler now shares the Enter-equivalent validation and state-3 lobby handoff; `make test-host-main-menu-accept` protects the convergence.
- [x] **Mode-2 lobby press feedback** — Disassembly of every actionable `GAMESTATE_HandleClick` branch (`0x40A548`, `0x40A672`, `0x40A722`, `0x40A889`, plus valid list/grid selections) confirms `PlaySound(0x5015)`, `Sprite_SetState(..., 1)`, and `Sleep(0x96)` before the action. The guarded SDL adapter now plays the same archive WAV, renders the exact horizontal frame-1 crop for 150 ms, then applies Exit/Search/Options; `make test-all` passes.
- [x] **Mode-2 main-menu button feedback** — Assembly in the unlabelled callback block `0x422930..0x422D6F` was checked against the Ghidra database: resource names prove left `0x407`/`0x408` are `singleup`/`singledown`, so its click sets DPlayConfig `+0x07` (mode 3); enabled right `0x409`/`0x40A` are the multiplayer pair and clear it (mode 0); lower controls toggle `+0x08`; each changed selection plays `0x5015` without delay. Accept/Quit restore the background, draw `0x404`/`0x406`, play `0x5015`, wait `0x96`, then commit/enter mode 10. The guarded SDL adapter routes selected single player to state 4 rather than the multiplayer lobby and routes the cleared multiplayer selection to state 5; the isolated regression proves this; `make test-all` passes.
- [x] **SDL game-testing protocol** — Added agent skill `lego-loco-game-testing` defining isolated Wayland lifecycle, logical-canvas click conversion, main-menu/lobby flows, screenshot evidence, and renderer-gate reporting; the headless Pixman/swrast sandbox now maps the SDL host successfully.
- [x] **Pytest GUI integration gate** — Added two crash-sensitive isolated-Wayland flows covering launch/main-menu/quit and name-entry/setup-lobby/Search/Exit/quit, with passive host-only JSONL events, Sway client-geometry input conversion, and persistent screenshots/logs under `build/test-artifacts/`.
- [x] **Mode-2 requested interaction scenarios** — Expanded isolated-Wayland coverage to prove: Back/Exit queues original WAV `0x5015` and exits; single-player + `test` + Go queues `0x5015` and leaves the main menu; untouched multiplayer entry retains the `PlayerRecord_constructor` (`0x452E10`) POSIX-account fallback before entering panel B; and Multiple → Host (`0x40B`) → Go → panel-B Back → main-menu Back exits cleanly. A stale `void*` `PlayerRecord_constructor` declaration had selected a no-op overload; GameLoop now calls the typed constructor and the POSIX `GetUserNameA` compatibility boundary implements its recovered size contract. `make test-all` passes.
- [x] **Mode-2/10 host sound boundary** — Preserved the documented `EditWindow::show` preload at `0x420780` (resource `0x5015`) and `CGWND_SetMode(10)` exit sound at `0x40824C` (resource `0x5026`) behind `_WIN32` guards. The SDL bridge resolves the same PE string-table/RFH WAVs (`sounds\toybox\clstray1`, `sounds\toybox\sweep1`), retains the exit stream until drained, and has an archive-backed regression.
- [x] **SDL launch intro sequence** — Host-only MCIWnd replacement decodes and aspect-fits the three shipped 640×480 Cinepak AVIs in the original `legoSpin → IgSpin → locointr` order: state-zero render `0x421EB0` starts `legoSpin`; `MM_MCINOTIFY` handling at `0x420F7F` starts `IgSpin`, then `[Video]/Dir` (`locointr`). The original video-child subclass `0x4207C0` posts `0x40A` for every keyboard/button-down input, and parent handler `0x420F6F` enters menu state 7; the guarded host therefore abandons all remaining clips on any key/button. Deterministic order and isolated-Wayland ordinary-key regressions cover both contracts.
- [x] **Legacy portable-port removal** — Deleted the unused alternate source tree, CMake configuration, and historical port guide. The root Makefile and `src/sdl3_shims/` are now the sole host runtime path.

### Phase 6: Native C compilation (2026-07-24)

- [x] **Native compatibility header** — added Win32/DirectDraw type scaffolding for raw Ghidra C output
- [x] **Native compile rate** — improved from 23/56 to 48/56 files under C++17
- [x] **C++ keyword cleanup** — renamed decompiler `this` parameters in affected native files

### Phase 7: Native-to-C++ porting (2026-07-25)

- [x] **`game_loop_setup.c` → `core/GameLoop.cpp`** — GameLoop_Setup (0x406BA0) and GameLoop_FrameUpdate (0x45C3C0), the two lifecycle backbone functions
- [x] **`config_ini.c` → `game/ConfigIni.cpp`** — Config_GetIniInt/GetIniString/WriteInt/ReadInt wrappers
- [x] **`world_enumerate_assets.c` → `world/World_enumerate.cpp`** — World_EnumeratePostLoadAssets (0x457380)
- [x] **21 new global definitions** in `stubs_impl.cpp` — g_network_thread, g_world_vehicles, g_mouse_spi3/4/5, DAT_* timer/pause globals
- [x] **`world.h` include guard fix** — added missing `#ifndef WORLD_H`
- [x] **`defsym.args` regenerated** — 1119 entries covering all undefined symbols from 109 object files
- [x] **Binary links at 2.3M** — up from 2.0M with new code

### 🎉 FIRST PIXEL — 2026-07-25

The SDL3 window opens with a dark blue background (Lego Loco menu color),
rendering continuously at ~60fps. The full launch sequence executes cleanly:

```
main()
  ├─ SDL3_WindowInit()          ✅ SDL window + renderer created
  ├─ CGWND::CGWND(nullptr)       ✅ Constructor
  ├─ CoInitializeEx()            ✅
  ├─ CGWND_InstallPathInit()     ✅ POSIX config via getenv
  │   └─ PlayerConfig_Ctor()     ✅ Real C++ constructor
  ├─ CGWND::ShowMainMenu()       ✅ SDL3 screen detection
  ├─ DDRAW_Init()                ✅ Returns true
  ├─ CGWND_PumpMessages()        ✅ SDL3 event loop + render
  │   └─ [LOOP] Clear → Present  ✅ Dark blue frame each iteration (ESC to quit)
  └─ CGWND::~CGWND()             ✅ Clean exit
```

### Major milestone: Full launch sequence executes

The binary now runs through the entire main() function:
```
main()
  ├─ SDL3_WindowInit()          ✅
  ├─ CGWND::CGWND(nullptr)       ✅
  ├─ CoInitializeEx()            ✅
  ├─ CGWND_InstallPathInit()     ✅ POSIX + PlayerConfig
  ├─ CGWND::ShowMainMenu()       ✅ SDL3 screen detection
  ├─ DDRAW_Init()                ✅ Returns true
  ├─ CGWND_PumpMessages()        ✅ SDL3 event loop (real loop, not empty stub)
  ├─ CGWND::~CGWND()             ✅ Clean exit
  └─ SDL3_WindowQuit()           ✅
```

### Build system

- **Root Makefile** (`make`): Compiles all sources, links binary
  - 73 C++ files (auto-discovered, 3 known-broken filtered)
  - 25 native C files (31 broken filtered)
  - 5 SDL3 shim files
  - Output: `build/lego_loco` (2.4M ELF)
  - Targets: `make`, `make run`, `make clean`, `make check`
  - Legacy defsym placeholders are historical; new unresolved internals require source stubs that log and assert
  - SDL3 auto-detection via `ls /nix/store/*sdl3-*-dev/include`

### Key files cleaned this session

| File | Status | Changes |
|------|--------|---------|
| `core/CGWND.cpp` | ✅ Compiles | #ifdef fix, non-Win32 stub block, hardcoded-addr fix |
| `core/CGWND_sdl3.cpp` | ✅ Compiles | SDL3 PumpMessages, C++ linkage |
| `game/PlayerConfig.cpp` | ✅ Compiles | Extern "C" split, MSVC cleanup, Ctor bridge |
| `shared/link_stubs.cpp` | ✅ Compiles | DDRAW_Init stub, POSIX file I/O |
| `shared/weak_stubs.c` | ✅ Compiles | g_install_path, g_remote_res_path |
| `docs/ghidra-cleanup-workflow.md` | NEW | 11-tag cleanup program |

### Decompilation workflow scheduler (2026-07-26)

- [x] **Incremental supervisor scheduling** — replaced upfront queue discovery with settled-boundary START/WAIT/COMPLETE decisions
- [x] **Persistent PRIMARY actors** — PARTIAL now nudges the same actor without an intermediate reviewer run
- [x] **Strict block gate** — only explicit block-reviewer legitimacy stops a class; review failure returns an error
- [x] **Settled completion buffering** — stale supervisor launch decisions are discarded when jobs finish during a scheduling turn
- [x] **Parallel pool consolidation** — removed superseded `tools/decompile-parallel.ts`
- [x] **Directed discovery** — optional capability objective focuses scheduling on the smallest relevant runtime dependency cone
- [x] **Workflow regression suite** — covers PARTIAL, rejected/legitimate blocks, review failure, directed discovery, and stale scheduling decisions
- [x] **Evidence-guided workflow foundation** — documented design intent; added lock-protected Python JSON core, Pi TypeScript bridge, durable attempt outcome ledger, source write-set audit, and CLI/integration regression tests

### Autonomous reverse-engineering daemon pivot (2026-07-27)

- [x] **Daemon vertical slice** — active design, SQLite WAL event/task store, normalized Pi RPC event recorder, local FastAPI/WebSocket dashboard, capability-protected internal API, Pi RPC lifecycle manager, and custom Pi task/write-scope extension
- [x] **Read-only Ghidra adapter** — daemon-owned MCP stdio client opens raw `loco.exe`, waits for analysis, serializes an allowlisted query set with verified direct proxy names, and stores content-addressed evidence revisions
- [x] **Evidence-led task scheduler** — SQLite tasks/requirement edges/write-scope requests, automatic initial evidence triage on job submission, dependency-gated launch of role agents, durable agent task transitions, and task-derived job states (`draft`/`queued`/`running`/terminal)
- [x] **Ghidra lifecycle and cache policy** — non-secret project-local command discovery, job-scoped evidence cache reuse, binary identity health, one-time worker restart, and database close on shutdown
- [x] **Hypothesis and approval workflow** — append-only evidence-linked hypothesis revisions, operator scope approval/rejection, and dynamic Pi write-scope enforcement
- [x] **Browser operator controls** — local dashboard forms for jobs/tasks/edges/scheduling/agent controls, task requeue, write-scope decisions, bounded replay, activity-sorted agent selector, formatted/coalesced streamed-message timeline, and declared Uvicorn WebSocket runtime
- [x] **Native HTML input dialogs** — replaced all blocking `window.prompt` flows with one reusable React `<dialog>` form supporting text areas, constrained selects, validation, cancellation, and themed modal presentation
- [x] **React graph dashboard** — replaced monolithic Cytoscape HTML with React 19/TypeScript, React Flow custom nodes/edges, lazy ELK layered layout, renderer-neutral graph contracts/registry, reproducible Vite/Nix builds, and frontend/backend regression tests
- [x] **Terminal task lifecycle** — every agent-reported terminal outcome requests Pi abort and boundedly reaps the idle RPC process; validated against a real Pi→Ghidra trial
- [x] **Stalled-attempt recovery** — watchdog fails/reaps stuck tool calls after bounded inactivity, marks stale agent records failed, startup recovers in-progress tasks whose assigned PID is gone, and the RPC reader accepts escaped multi-KiB tool events
- [x] **Autonomous task-graph driving** — evidence agents atomically persist bounded acyclic follow-on DAGs, initial triage cannot complete with only a prose plan, ready-task claims are race-safe, terminal transitions dispatch the next serial successor, queued graphs resume on startup, and dashboard snapshots render durable edges
- [x] **Settled dependency advancement** — `requires` edges now gate only active prerequisites; blocked/deferred/failed roots release ready successors, expose their outcomes to successor agents, and keep the root job queued while executable work remains

### EditWindow inherited virtual slots (2026-07-28)

- [x] **EditWindow inherited virtual dispatch** — Ghidra-defined and decompiled `UI_WindowBase::set_mode` (0x425FD0), `set_render_surface` (0x426020), and `on_async_task_failure` (0x426130); replaced all EditWindow self-vtable calls with typed virtual methods.

## Remaining work

### Autonomous reverse-engineering daemon

- [ ] **Scheduler retry policy** — bounded automatic re-queueing/backoff for failed attempts (the watchdog currently fails safely for explicit operator requeue)
- [ ] **Transcript history** — expose bounded historical Pi-session transcript links without serving secret-bearing files
- [ ] **Evidence lifecycle** — retention/eviction policy for content-addressed artifacts and explicit cache refresh controls in the dashboard

### Priority 1: #ifdef pivot — first wave

- [x] **CGWND.cpp**: Replace Win32 window creation, message loop, timers, registry with SDL3 + POSIX
- [x] **File I/O sweep** (~15 files): Updated `CreateFileA`/`ReadFile`/`WriteFile`/`CloseHandle`/`GetFileSize` in link_stubs.cpp to use POSIX (`fopen`/`fread`/`fwrite`/`fclose`) under `#ifndef _WIN32`. No per-file changes needed.
- [x] **CGWND.cpp** (18 calls): Full #ifdef conversion with SDL3 window, POSIX registry replacement
- [x] **Window mgmt remaining** (GameWindow.cpp, EditWindow.cpp, Cursor_Editor.cpp): all three files now have #ifdef _WIN32 guards with SDL3-backed alternatives via sdl3_window.h + inline stubs
- [x] **Registry elimination** (2 files): CGWND.cpp done (#ifdef + getenv); Game.cpp only has import table docs, no actual calls
- [x] **Simple GDI utils** → stubs in sdl3_window.cpp (CopyRect, OffsetRect, SetRect, SetRectEmpty, PtInRect, IntersectRect, UnionRect)
- [x] **Cursor_internal.h #ifdef guards**: split extern "C" block into Win32-only (#ifdef _WIN32) and game-specific (unconditional) sections
- [x] **sdl3_window.h extended**: added 25+ missing Win32 declarations (DestroyWindow, ClientToScreen, GetCursorPos, OutputDebugStringA, CopyRect, OffsetRect, UnionRect, IntersectRect, GetStockObject, DrawEdge, SetBkColor, CreateFileA, GetFileSize, CloseHandle, GetLastError, FormatMessageA, LocalFree, GetOpenFileNameA, Sleep, ExitProcess, PostQuitMessage, SetFocus, HLOCAL typedef, PtInRect fixed signature)

### Priority 2: First launch

- [x] **CGWND_sdl3.cpp unbroken**: removed from BROKEN_SRCS, now compiles and provides real SDL3 message pump (was previously linked as empty stub → immediate exit). Window opens with blue background, ESC quits cleanly.
- [x] **Game loop wired up**: `GameLoop_Setup` (0x406BA0) called from main.cpp — all 12 init steps complete. `GameLoop_FrameUpdate` (0x45C3C0) called each frame from SDL3 pump. Mode 1 early-returns correctly.
- [x] **Linkage fixes**: `GameLoop_Setup`/`GameLoop_FrameUpdate` are `extern "C"` — fixed declarations in main.cpp and CGWND_sdl3.cpp. `ResourceManager_Init` stub fixed (`void`→`int` return).
- [x] **SDK wiring fixes**: `RegisterWindowClass` SDL3 path now uses `SDL3_GetWindow()`. `g_main_window` set to CGWND instance in main.cpp.
- [x] **Decompile `CGWND_InitMode1`** (0x408350) — full implementation from Ghidra decompilation. Two code paths: first-time loading screen with incremental subsystem init + progress pump, and return-to-menu world loading. All stubs in place, game transitions to mode 1 and enters main loop cleanly.
- [ ] **Implement real subsystem constructors** — `UI_MainMenu_Ctor`, `Town_Ctor`, `Cursor_Ctor`, etc. currently return raw memory (no vtable). Need proper class implementations with virtual methods.
- [ ] **Unbreak remaining 3 C++ files**:
  - `game/Building.cpp` — uses `this->vtable`, MSVC `scalar_deleting_destructor` pattern
  - `town/Town.cpp` — signature mismatches, `void*` arithmetic
  - `stubs/sdl3_undecompiled_stubs.cpp` — stale SDL3 stubs, likely obsolete
- [ ] **Asset path configuration** — Point `g_install_path` at unpacked game data

### HelpWnd / HelpPageNode extraction

- [x] **Extract HelpPageNode class** — find_page (0x44F210), find_page_scalar_dtor (0x44F2A0), find_page_base_dtor (0x44F2C0) extracted to ui/HelpPageNode.h and ui/HelpPageNode.cpp. HelpPageNode extends RESDATA_GameVehicle (0x11C base + 0xC fields: update_flag, overlay_flag, dest_list_head = 0x128 total). Vtable 0x4783D8. Constructor (0x44F210) chains to RESDATA_GameVehicle_Ctor. Destructor (0x44F2C0) frees linked list. Update override (0x44F340, vtable[10]) processes page linkage events. Ghidra anti-patterns fixed: removed literal vtable assignment, scalar deleting destructor, void* return type, and raw RESDATA_ prefixes.

- [ ] **Decompile HelpWnd stub methods** — 6 rendering methods are stubbed in ui/HelpWnd_stubs.cpp: set_mode (0x414340), render_page (0x452230), render_scroll_up (0x452570), render_scroll_down (0x4526B0), draw_scroll_indicator (0x452B00), update_anim_sprite (0x452C00).

- [ ] **Add named fields to pixel data structure** — ButtonSprite pixel data is accessed via raw offsets: +0x1E (default animation index), +0x20 (frame table pointer). Used in show(), load_page(), update_anim().

- [ ] **Add named fields to AudioChannel** — sound resource ID at +0x38 accessed in hide() and play_page_audio_common().

### Priority 3: Rendering completeness

- [ ] **Palette cycling support** — Currently palettes are baked at load time; palette animations (water, sky) need runtime palette swaps
- [ ] **GDI DC shim** — `GetDC`/`ReleaseDC` currently stubbed; needed for some UI rendering paths
- [ ] **Complete ResourceManager consumers** — connect cached WAV data to `GameAudio`, parse BUT descriptors and ANI frames for their actual UI/cursor call sites, and map tile DAT semantics into the decompiled world objects; lookup, DAT animation metadata, color keys, and generic archive blobs are now live.
- [ ] **DDRAW sprite data** — `DDRAW_SpriteData` loading/management (native .c files)

### Priority 4: Audio completeness

- [ ] **Streaming audio** — `IDirectSoundBuffer::Lock`/`Unlock` currently stubbed
- [ ] **3D positioning** — `SetPan` currently stubbed
- [ ] **Frequency shifting** — `SetFrequency` currently stubbed

### Priority 5: Networking

- [ ] **DirectPlay reimplementation** — Currently fully stubbed; needs UDP-based replacement
- [ ] **Lobby protocol** — `DPLAY_SendMessages`/`ReceiveMessage` message format

### Priority 6: Polish

- [ ] **Port remaining native `.c` files to C++** — 28 usable from 56 total in `native/` (25 already compile, 31 broken). 3 ported so far (game_loop_setup→GameLoop, config_ini→ConfigIni, world_enumerate→World_enumerate)
- [ ] **Eliminate `-fpermissive`** — Fix remaining MSVC-isms so strict C++17 compiles
- [ ] **MinGW cross-build** — Verify SDL3 shims work under `i686-w64-mingw32-g++`

---

## Architecture notes

### Class hierarchy

```
GameObject (0x477820, type=1)
  └── Entity (0x477488, type=2)
        ├── Building (0x477EB8, type=7, size 0xF4)
        │     └── Train (size 0xF0)
        ├── ResourceGameObject (was BuildingMgrObjectGroup)
        ├── Vehicle
        └── ... (Town, ScriptedObject, etc.)
BuildingMgr (0x477F70, size 0x7C) — separate class, not in GameObject tree
CGWND — application window class
```

### Key addresses

| Symbol | Address | Notes |
|--------|---------|-------|
| `VTBL_GAMEOBJECT` | 0x477820 | 6-slot root vtable |
| `VTBL_ENTITY` | 0x477488 | 15-slot intermediate vtable |
| `VTBL_BUILDING` | 0x477EB8 | Full Building vtable |
| `VTBL_BUILDINGMGR` | 0x477F70 | BuildingMgr core vtable |
| `Building::BaseCtor` | 0x433A20 | Shared ctor (Building + Train) |
| `Building::Deserialize` | 0x435700 | Save file loader |
| `operator_new` | 0x465CE0 | Global heap allocator |
| `GLOBAL_free` | 0x465CD0 | Global heap deallocator |
| `g_game_time` | 0x4A99B4 | Game tick counter |
| `g_main_window` | 0x4AA4A0 | Main HWND |

### Dual-vtable construction pattern

The binary uses intermediate vtables during construction:
- Building_BaseCtor sets vtable 0x477F18, then Building_Ctor overrides to 0x477EB8
- In natural C++, the compiler handles vtable progression through the ctor chain
- We do not emulate this pattern — we let C++ virtual dispatch work naturally

### C/C++ linkage in the original binary

The original `loco.exe` (MSVC 1998, x86) used C linkage for:
- All Win32 API imports (kernel32.dll, user32.dll, gdi32.dll) — inherently C-linkage
- Internal helpers: `operator_new`, `GLOBAL_free`, CRT wrappers
- Free functions compiled as `__cdecl` or `__fastcall` (e.g., `ButtonSprite_Ctor`, `Sprite_Init`)

57 decompiled files preserve these `extern "C"` declarations correctly.
The hybrid #ifdef approach eliminates the resulting ABI mismatch for simple
APIs while keeping the shim for complex subsystems (DDRAW, DSOUND, DPLAY).

### Key files

| File | Purpose |
|------|---------|
| `src/decompiled_cpp/shared/link_stubs.cpp` | C + C++ stub definitions (~400 symbols) |
| `src/decompiled_cpp/shared/stubs_impl.cpp` | Runtime stubs + globals |
| `src/decompiled_cpp/shared/crt_stubs.cpp` | CRT compatibility implementations |
| `src/decompiled_cpp/stubs/compat.h` | MSVC calling convention macros, type stubs |
| `src/sdl3_shims/sdl3_window.cpp` | Win32 windowing → SDL3 (to be deprecated by #ifdef) |
| `src/sdl3_shims/sdl3_ddraw.cpp` | IDirectDraw4 → SDL3 Renderer (keep) |
| `src/sdl3_shims/sdl3_dsound.cpp` | IDirectSound → SDL3 Audio (keep) |
| `src/sdl3_shims/sdl3_dplay.h` | DirectPlay stub (keep) |
| `tools/re_daemon/frontend/src/graph/contracts.ts` | Renderer-neutral dashboard graph and layout contracts |
| `tools/re_daemon/frontend/src/graph/GraphCanvas.tsx` | Swappable graph-renderer registry (React Flow initially) |

---

### Priority: GameSetupPanel extended vtable stubs

- [ ] **GameSetupPanel::HandleMapClick** (0x40ABA0) — vtable[12]. Stub in stubs/gamesetuppanel_network_stubs.cpp.
- [ ] **GameSetupPanel::SelectLayoutEntry** (0x40AAF0) — vtable[13]. Stub in stubs/gamesetuppanel_network_stubs.cpp.
- [ ] **GameSetupPanel::SendScenarioSelect** (0x40AC50) — vtable[14]. Stub in stubs/gamesetuppanel_network_stubs.cpp.
- [ ] **GameSetupPanel::ConnectToNetworkGame** (0x40AA20) — vtable[15]. Stub in stubs/gamesetuppanel_network_stubs.cpp.

---

## Session log

| 2026-07-30 (mode10-headless-drain) | Restructured exit-sound drain per user feedback: PumpMessages now returns immediately on mode 10 (no drain loop blocking the pump); main() hides the SDL window instantly, then runs a headless audio drain loop before SDL_Quit tears down the audio device. This matches the original behavior where the window closes right away but DirectSound hardware buffers outlive the process. |
| 2026-07-30 (mode10-exit-sound-drain) | Fixed exit sound missing on quit: the PumpMessages mode-10 path returned immediately without draining the SDL audio stream. Unlike DirectSound (hardware-owned buffers), SDL3 streams are process-owned and need explicit drain before SDL_Quit tears down the audio subsystem. Added drain loop with 3s timeout + 150ms post-drain delay in CGWND_sdl3, and SDL3_GameAudioStopAll before playing the exit sweep in CGWND_SetMode so background music doesn't extend the drain window. |
| 2026-07-30 (quit-button-sound-fix) | Traced the original assembly at the EditWindow WindowProc (0x422900-0x422D80): the quit button handler (0x422AC3) does NOT call PlaySound(0x5015) — only the accept button handler (0x422A72) does. The quit path goes directly to CGWND_SetMode(10) which plays the exit sweep 0x5026. Fixed the host code to match: SDL3_GameAudioPlayResource(0x5015) is now only played for the accept button, not the quit button. |
| 2026-07-30 (host-audio-background-music) | Implemented background music support: added `SDL3_GameAudioPlayFile` for disk WAV playback with optional looping, rewired `PlaySoundA` in `sdl3_window.cpp` to route through SDL3 audio (SND_ASYNC, SND_LOOP, SND_PURGE), added path-normalization for Windows→Linux slash conversion, and added a host fallback in `CGWND_InstallPathInit` that resolves `g_install_path` from `LEGO_LOCO_DATA` when the INI-derived path is non-existent (e.g. `d:\loco\art-res`). All 10 component tests pass. |
| 2026-07-29 (sdl3-launch-intro) | Diagnosed that the obsolete GStreamer player was neither built nor invoked by the SDL3 host; added a guarded appsink renderer for all three shipped Cinepak AVIs, Nix decoder dependencies, skippable launch sequencing, and isolated-Wayland screenshot/event coverage. `make test-all` passes. |

| 2026-07-29 (panel-b-selector-typography) | Validated the missing `CGWND_GameSetup_Show` block at `0x408F70` against its raw x86 instructions: `0x40902E..0x4090F5` places the list right of the grid and `0x409635..0x409642` applies its 12px padding. `ResourceManager_Init` (`0x44611A..0x44613A`) proves `g_font_normal` is 14px, weight-700 Arial. The guarded SDL renderer now uses a 14px emboldened FreeType fallback at those exact list coordinates; updated isolated-Wayland clicks and `make test`/`make test-integration` pass. |
| 2026-07-29 (panel-b-geometry-regression) | Corrected the guarded panel-B geometry regression: raw x86 `0x409034` loads work top and `0x409046` adds `0x27`, so grid/list/buttons must use `work.top + 0x27`, not the unrelated horizontal `0x1B`. Updated row-click evidence; `make build`, focused host regressions, and all 8 isolated-Wayland GUI tests pass. |
| 2026-07-30 (panel-b-selector-text) | Recovered `GameSetupPanel::drawLayoutList` (0x4094B0) instruction-by-instruction: it uses normal 14px Arial, 12px list padding, measured-height + 4 row cadence, unselected COLORREF `0x00FF5C00` and selected COLORREF `0x002525DC`. Replaced non-compositing `SDL_RenderDebugText` calls with a guarded direct-primary 14px glyph fallback in the recovered colours; isolated Wayland captures contain exact `#005CFF`/`#DC2525` glyph pixels and layout-selection events. |
| 2026-07-30 (panel-b-layout-selector) | Grounded the SDL panel-B layout chooser in NETMAN_SyncGameState (0x43FC50) and GameSetupPanel::drawGrid (0x409980): the host now exposes 3x3, 2x2, 2x1, 3x1, and 3x2 and projects their slot count/row/column values into the original Netman inputs when that optional host object exists (otherwise the guarded provider profile is consumed directly); static plus isolated-Wayland layout-selection coverage added. |
| 2026-07-29 (timer-use-after-free) | Fixed SDL timer use-after-free crash when transitioning from multiplayer grid back to main menu. Root cause: KillTimer called g_timers.erase() after SDL_RemoveTimer, freeing the TimerInfo while in-flight callbacks on the SDL timer thread still held a pointer to it. Fix: null out callback before erase in KillTimer/SDL3_WindowQuit, check for null callback in sdl_timer_callback, and populate map entry before passing its address to SDL_AddTimer. Added timer stress test (500 rounds × 8 rapid create/kill cycles at 1-4ms intervals) to catch future regressions; make test passes. |
| 2026-07-29 (legacy-port-removal) | Removed the unused alternate portable-runtime tree, CMake configuration, stale dependencies, and port guide; the root Makefile/Sdl3 shim path is now canonical. |

| 2026-07-29 (intro-order-and-input) | Validated the MCI launch state machine in `loco.exe`: 0x421EB0/0x420F7F yields legoSpin → IgSpin → locointr, while video subclass 0x4207C0 routes any keyboard/button-down input through 0x40A to the immediate state-7 menu transition. Corrected the SDL host and added deterministic plus isolated-Wayland regressions. |

| 2026-07-29 (main-menu-exit-tdd) | Replaced the false-positive main-menu Exit check with an isolated-Wayland regression that launches with SDL dummy audio, drives both original +0x14C/resource-0x405 Exit and focused-Escape mode-10 paths, and fails after 5 seconds awaiting clean shutdown; this deterministically reproduces the queued-audio exit stall. |

| 2026-07-29 (pytest-gui-integration) | Introduced the Nix-managed pytest test layer and `make test{,-integration,-all}` gates; added guarded host JSONL observability, crash/timeout detection, Sway-decoration-aware logical input, and persistent screenshot/log artifacts; deterministic regressions pass and the two headless GUI flows passed twice consecutively. |

| 2026-07-28 (editwindow-ghidra-validation) | Cross-validated EditWindow menu methods against Ghidra: corrected 0x422D80 hit-test gates and 0x422010/0x422440 resource branches/character source rect; replaced verified raw field access and all self-vtable dispatches with typed UI_WindowBase calls after recovering 0x425FD0/0x426020/0x426130; corrected the canonical opaque `g_main_window` global use after checking `UI_MainMenu_Hide` (0x420860); gated the recovered packed-x86 `UIPANEL_Render` slot paths under `_WIN32` after a mode-2 core proved they are incompatible with the SDL host layout; added generated Makefile header dependencies after a stale NameEntryPanel vtable crashed the Enter/state-3 transition, then clean-rebuilt and GDB-exercised that transition; `make build`, `make check` (74/74), and `make test-mode2-menu-backdrop` pass. |

| 2026-07-28 (mode2-multiplayer-transition) | Wired Enter-key to onPlayerNameChanged flow with hostEditText bridge; added GameSetupPanel::hostRenderFrame (SDL3 primary-canvas composition with title/list/grid placeholders); extended hostRenderFrame dispatch to states 3/4/5 (GameSetupPanel visible); all 120 objects compile and link clean. |

| 2026-07-28 (mode2-multiplayer-artwork) | Grounded the GameSetupPanel host compositor in Ghidra 0x408F70/0x409280/0x409980; replaced its teal placeholder with archive-backed apback, recovered Exit/Search/Options controls, and all nine cropped empty-player frames at original-derived positions, behind non-Windows guards; added and passed the multiplayer-artwork regression plus the full host build. |

| 2026-07-28 (mode2-multiplayer-input) | Recovered GAMESTATE_HandleClick (0x40A4E0) and sent SDL mode-2 pointer events to a non-Windows GameSetupPanel adapter; Exit and Options use the original parent state transitions, while Search reports the intentionally empty DirectPlay boundary. Added the guarded routing regression; build, check, lobby artwork, and renderer-linkage tests pass. |

| 2026-07-28 (mode2-main-menu-accept) | Verified the resource-0x403 pressed branch at 0x422AB2 calls EditWindow_OnPlayerNameChanged (0x422660); routed its SDL hit through the shared guarded Enter validation/state-3 handoff, with an accept-control regression plus build, check, lobby-artwork, and renderer-linkage tests passing. |

| 2026-07-28 (game-test-protocol) | Added the project-specific isolated-Wayland testing skill and executed its build/static gates; documented that this headless Pixman sandbox has no compatible SDL window renderer, so interactive screenshot/input evidence awaits a Wayland-capable test host. |

| 2026-07-28 (mode2-enter-crash) | Analyzed `lego_loco.core`: the post-Enter host render called the `__stack_chk_fail` PLT slot because `SDL3_GetRenderer` had C++ linkage while the shim exports C linkage. Corrected the guarded declaration, added an executable-linkage regression, and preserved the original `GetWindowTextA(..., 13)` contract with a safe 13-byte host buffer. |

| Date | Summary |
| 2026-07-28 (mode2-host-audio) | Added guarded SDL3 playback for the original mode-2 `0x5015` preload and mode-10 `0x5026` exit sound; archive/audio/menu regressions and the Nix-shell build pass. |
|------|---------|
| 2026-07-27 (settled-dependency-scheduling) | Fixed autonomous scheduling so terminal blocked/deferred/failed prerequisites release ready successors; root jobs remain queued while executable descendants exist, with prerequisite context, store/RPC regressions, and live-state-copy verification. |
| 2026-07-27 (html-input-dialog) | Replaced dashboard `window.prompt` input with a reusable native `<dialog>` form across job, task, dependency, recovery, agent-control, and write-scope workflows; rebuilt static assets and passed frontend/web tests. |
| 2026-07-27 (react-flow-dashboard) | Migrated the autonomous RE dashboard from monolithic Cytoscape HTML to React Flow + lazy ELK behind renderer/layout interfaces; added custom graph elements, operator UI parity, Vite/Nix builds, static assets, and regression coverage. |
| 2026-07-27 (autonomous-task-graph) | Added agent-driven bounded DAG expansion with idempotency/cycle checks/source gating, atomic scheduler claims, automatic terminal/startup advancement, graph rendering, and end-to-end regression coverage. |
| 2026-07-27 (automatic-job-kickoff) | Job submission now creates and dispatches a read-only initial evidence triage task; added one-click bootstrap for existing empty drafts. |
| 2026-07-27 (job-status-singleton) | Reconciled dashboard job state from runnable/in-progress/terminal tasks and added an exclusive state-file lock after stale queued/running jobs and three competing daemon processes were observed. |
| 2026-07-27 (ghidra-proxy-names) | Found the adapter sent Pi-decorated `ghidra_*` names to the direct re-mcp-ghidra proxy, which exposes unprefixed names; corrected open/wait/close/query mappings and verified a live decompile. |
| 2026-07-27 (rpc-framing) | Found large escaped `read` result JSONL records exceeded asyncio’s 64KiB reader limit, silently halting stdout consumption and stalling sibling parallel reads; raised the bounded limit to 2MiB with regression coverage. |
| 2026-07-27 (websocket-runtime) | Fixed live dashboard upgrades by declaring Uvicorn’s WebSocket runtime in Nix and enforcing it at startup; verified HTTP 101 handshake. |
| 2026-07-27 (stalled-attempt-recovery) | Found a live agent stalled after 3 of 4 parallel reads never completed; added tool-inactivity/dead-PID watchdog, safe operator recovery, and startup stale-task recovery. |
| 2026-07-27 (dashboard-timeline) | Reworked the live event tail into an activity-sorted agent-filterable timeline with formatted event cards and consolidated streaming deltas. |
| 2026-07-27 (daemon-reaper-trial) | Re-ran a real no-write Pi→daemon→Ghidra task after the terminal lifecycle fix: task completed, evidence persisted, and the Pi child was reaped; 224K temporary state removed. |
| 2026-07-27 (daemon-trial) | Ran one live no-write Pi→daemon→Ghidra trial: evidence and a hypothesis persisted, task completed, abort control worked, and 340K temporary state was removed. |
| 2026-07-27 (autonomous-re-daemon) | Added Ghidra worker recovery, project-local non-secret config, job-scoped evidence caching, revisioned hypotheses, operator scope approvals with dynamic enforcement, and browser scheduling/agent controls. |
| 2026-07-27 (autonomous-re-daemon) | Added daemon-owned read-only Ghidra MCP adapter, content-addressed evidence revisions, dependency-gated task scheduler, durable task outcomes/scope requests, and real loco.exe MCP integration probe. |
| 2026-07-27 (autonomous-re-daemon) | Pivoted to active daemon design; implemented SQLite event store, Pi RPC manager, custom Pi extension, local FastAPI/WebSocket dashboard, and 6 daemon tests. |
| 2026-07-27 (evidence-workflow-core) | Added the evidence-guided workflow design, atomic Python ledger/cache CLI, Pi bridge, supervisor attempt persistence and write-audit integration, plus 13 workflow/core tests. |
| 2026-07-28 (mode2-player-name-input) | Recovered the native EDIT creation/subclass and its Enter/Escape paths; added a host-only logical-canvas input field with SDL text event routing, native length/validation rules, and PlayerConfig name-copy behavior. |
| 2026-07-28 (mode2-quit-control) | Confirmed in Ghidra that the +0x14C control invokes CGWND_SetMode(10); wired its host hit path to the reconstructed quit transition and SDL-pump termination. |
| 2026-07-28 (mode2-menu-controls) | Recovered default menu-control draws and click state transitions; added host-only frame composition and inverse scaled-pointer routing. |
| 2026-07-28 (mode2-backdrop-canvas) | Recovered EditWindow::render (0x4216F0), rendered its five assets on a fixed 1280×1024 canvas, and added pixel-perfect/letterboxed 800×600 presentation coverage. |
| 2026-07-27 (mode2-main-loop) | Replaced EditWindow’s raw slot-4 dispatch with its assembly-proven null-target no-op; SDL mode 2 reaches the message pump without crashing. |
| 2026-07-27 (uipanel-sdl-composition) | Recovered UIPANEL_Render (0x426EB0), added typed SDL primary surface clear/blit operations, and rendered decoded menu sprites without raw x86 vtable access. |
| 2026-07-27 (train-subsystem-bootstrap) | Decompiled TrainSubsystem_Ctor (0x438BC0) and worker setup, replaced the null ctor pointer with typed construction, and added an empty-provider SDL DirectPlay boundary. |
| 2026-07-27 (dplay-config) | Decompiled GameConfig_constructor (0x440C60) and replaced its null host stub with the DPlayConfig defaults; mode-2 startup now advances to the unimplemented TrainSubsystem function pointer. |
| 2026-07-27 (enabled-build-restored) | Repaired the remaining enabled-source compilation failures; root make now links the 3.6M host binary. |
| 2026-07-27 (mode2-menu-bootstrap) | Host startup now constructs the real EditWindow-only subsystem cone and enters mode 2 through typed show dispatch; unavailable world subsystems are explicitly gated rather than stub-allocated. |
| 2026-07-27 (primary-surface-present) | Added SDL primary/backbuffer target ownership and made CGWND present the primary texture; dummy-driver regression verifies pixel-accurate primary-to-window composition. |
| 2026-07-27 (editwindow-typed-panels) | Replaced EditWindow child-panel construction/destruction and state dispatch with NameEntryPanel/GameSetupPanel C++ methods; modeled the popup lifetime as a virtual C++ object. |
| 2026-07-26 (resmgr-typed-cleanup) | Replaced raw menu sprite offsets and resource vtable calls in EditWindow/viewer with typed SpriteResource/SpriteBitmap accessors; legacy ABI table is confined and documented in the bridge. |
| 2026-07-26 (resmgr-dat-nonbmp) | Added paired DAT button/animation parsing, RGB magenta source color-key handling, and typed RFH/RFD asset blobs; regression validates startup metadata plus WAV, compressed BUT, and ANI resources. |
| 2026-07-26 (resmgr-sprite-bridge) | Replaced ResourceManager_Init/GetById null stubs with validated PE RT_STRING → RFH/RFD BMP SDL-surface caching; test verifies 0x407 dimensions and vtable[4]/[8] ABI. |
| 2026-07-26 (sprite-renderer) | Added PE RT_STRING resolver (validates 0x407 → startup\\singleup) and SDL startup-menu viewer composed at the original 0x4216F0/0x422010 coordinates; dummy-driver test renders a real frame. |
| 2026-07-26 (sprite-archive) | Removed the uncommitted fake sprite/subsystem layer. Added a validated RFD/RFH archive reader from the actual asset format and Huf_Decode (0x45C830); regression test decodes real startup BMP pixels. |
| 2026-07-26 (gamesetuppanel-review2) | **GameSetupPanel review fixes (6 issues)**: (BLOCKER 1) Removed duplicate class definition from vtable_stubs.cpp (ODR violation). (BLOCKER 2) Created stubs/gamesetuppanel_network_stubs.cpp with 4 extended vtable method stubs (HandleMapClick 0x40ABA0, SelectLayoutEntry 0x40AAF0, SendScenarioSelect 0x40AC50, ConnectToNetworkGame 0x40AA20) using assert(!"stub") + TODO annotations, tracked in PROGRESS.md. (BLOCKER 3) void* currentList → LayoutListNode* currentList; drawLayoutList(void*) → drawLayoutList(LayoutListNode*). (BLOCKER 4) Renamed RESMGR_ReleaseSoundResource → ReleaseSoundResource (Ghidra prefix removal) in ResourceManager.h and call site. (BLOCKER 5) Added NOTE comment for int32_t-to-ResourceEntry* cast. (WARNING) ResourceManager.h GetById return type documented. Compilation: PASS. |
| 2026-07-26 (workflow-scheduler) | Replaced upfront work queues with incremental supervisor decisions; added persistent PARTIAL-aware primaries, strict block validation, settled completion buffering, directed discovery objectives, and 7 regression tests. |
| 2026-07-26 (decompile-class) | GameVehicle: 7 function(s) — 3 pass(es), achieved INTEGRATED |
| 2026-07-26 (workflow) | Refactored `tools/decompile-class.ts` schema retry handling into a shared helper; primary, reviewer, and block-reviewer agents now retry invalid structured output with schema-specific corrective feedback. |
| 2026-07-26 (decompile-class) | GameVehicle: 7 function(s) — 1 pass(es), achieved VALIDATED |
| 2026-07-25 (agents.md) | Defined correctness/completeness standards, 3-tier status tags (TRANSCRIBED/VALIDATED/INTEGRATED), multi-pass pipeline, stub exceptions, and 12 Ghidra anti-patterns with before/after examples. AGENTS.md grew from 280 to 834 lines. |
| 2026-07-23 | Initial decompilation: GameObject, Entity, Building, CGWND, directories, stubs, Makefile |
| 2026-07-24 (early) | Previous agent added SDL3 port (removed). Commits 3a4e5d4..531fa7b. |
| 2026-07-24 (mid) | Ghidra validation: opened loco_v8, dispatched 7 agents. All 69 files validated and corrected. |
| 2026-07-24 (late) | Code quality cleanup: 4 agents removed VTBL_/raw-ptr/vtable/extern-C anti-patterns. 56 files fixed. |
| 2026-07-24 (eve) | SDL3 shims: created isolated `src/sdl3_shims/`, 1117 lines, palette-at-load-time strategy. Both compile. |
| 2026-07-24 (night) | SDL3 window shim: `sdl3_window.h/cpp` (838 lines), 60+ Win32 APIs on SDL3. All 3 compile standalone + with decompiled headers. Created `main.cpp` entry point. Type conflicts between sdl3_types.h and types.h resolved. |
| 2026-07-24 (CRT stubs) | Added weak CRT/helper compatibility implementations and executable globals; verified `build/shared/crt_stubs.o` compiles. |
| 2026-07-24 (final link) | Removed duplicate inline stubs/globals from SDL3 `main.cpp`; final link attempted with 23 native objects, leaving 779 unique undefined symbols. |
| 2026-07-24 (native compile) | Added `native_compat.h`, fixed native C type/keyword/declaration issues, and raised native compilation from 23/56 to 48/56. |
| 2026-07-25 | Created link_stubs.cpp (479 lines, ~400 symbols). Identified fundamental shim limitation: 57 files with `extern "C"` declarations vs C++-linkage shim implementations. Pivoted to hybrid #ifdef + shim strategy. ~284 remain. |
| 2026-07-25 | **Unified build system**: Created root Makefile. Single `make` command compiles 94 source files (64 C++, 25 native C, 5 SDL3 shims) and links into 2.0MB ELF binary. 16 C++ files skipped (known-broken), 31 native files skipped. Auto-detects SDL3 on NixOS. Removed stale CMakeLists.txt / legacy portable-port artifacts. |
| 2026-07-25 (late) | Replaced 275 defsym=0 entries with real stubs. Fixed CRT_strtok NULL-return crash. Binary runs full main() → PumpMessages → clean exit. Valgrind: 0 errors. Root cause of heap corruption: 32-bit allocation sizes used on 64-bit. |
| 2026-07-25 (game stubs) | Building class completed: decompiled 7 vtable-gap functions (TeleportTo, StepToward, IsActionComplete, PartyModeUpdate, CheckPlacementCollision, PostMoveDispatch, FindNearestConnectionNode). Updated Building.h fields and virtual methods. Replaced 6 TODO stubs. Removed from sdl3 stubs. 70/70 compile. |
| 2026-07-25 (decomp) | Decompiled GameObject_GetBoundingRect→Entity::GetBoundingRect (0x4583C0), BuildingMgr_CompactCollections (0x434870), Town_PostcardUpdateUI (0x42DE70). Fixed PtInRect scope in Building.cpp, blit_element→BlitElement in Town.h. |
| 2026-07-25 (decomp2) | Decompiled Game_CheckTimeInRange (0x412710), BuildingMgr_CompactCollections (0x434870), Entity::GetBoundingRect (0x4583C0). Unblocked 11 files from broken list (Train, ScriptedObject, Panel, TrackPiece, UIPANEL, UIPANEL_Surface, EditWindow, UI_WindowBase, Netman, Netman_ReceiveSignalChange, Cursor_new_impls). 77→66 C++ files compile. |
| 2026-07-25 | Fixed binutils 2.46 incompatibility: old --defsym used demangled names with parentheses (rejected by new ld). Switched to mangled names from nm -u. Generated 926-entry defsym.args. Binary links clean. |
| 2026-07-30 (64bit-cast-cleanup) | Fixed all -Werror=int-to-pointer-cast and -Werror=pointer-arith errors across 25 files (17 C++, 8 C). Used (uintptr_t) intermediates for int↔pointer casts and (uint8_t*) for void* arithmetic. 0 errors, all component tests pass. |
| 2026-07-31 (ui-cast-warning-gate) | UI translation units now pass the STRICT old-style-cast diagnostic gate. |

[Showing lines 1-447 of 466 (50.0KB limit). Use offset=448 to continue.]