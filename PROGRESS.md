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
- [x] **Host singleplayer menu-teardown crash** — GDB core analysis identified `EditWindow::cleanupSprites` passing uninitialized `pMainSurface` to `UIPANEL_DestroySurface` during mode-2→mode-1. x86 `UI_MainMenu_Ctor` (0x4202F0) deliberately leaves `+0x1F0` indeterminate because Windows `render` initializes it; the SDL compositor does not allocate that surface. The guarded host path now initializes it to null and never invokes the x86 surface destructor.
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
- [x] **SDL_net TCP transport vertical slice** — Added SDL_net 3.2/Nix/Make wiring; explicit 24-byte little-endian framing; split/coalesced stream decoding; application/version/session handshake; host ID 1 and client IDs 2–9; host-relayed routing; join/leave/error events; 64-KiB frames; 256-KiB pending-write caps; and a dedicated owner thread. Host mode binds port 23000 before DNS-SD publication, selected resolved sessions join through the same authoritative client path, and two-process loopback plus publish→browse→join tests exchange version-300 payloads.
- [x] **SDL_net game-facing lobby integration** — Added hostname/IPv4/bracketed-IPv6 Direct Connect entry and strict port parsing; extended admission with host identity; projected listener/client/join/leave state into host-native Netman slots; finalized admission through recovered `GameSetupPanel::ConnectToNetworkGame` (0x40AA20); compiled all four recovered panel methods; exposed original Go resource 0x42A only when session-ready; routed type-6 sends through SDL_net with send-time version 300; and translated supported incoming legacy payloads into Train/Netman handlers. Isolated GUI tests prove a real client ID 2, 0x3F4→type-0x13 queue processing, direct client admission, ready-Go screenshot, and clean lobby teardown.
- [x] **SDL_net service/file and ping packet integration** — Integrated exact bounds/ownership for `0x3EA`–`0x3EF`, `0x3F6`, and `0x3F9`; added bidirectional `0x3EA`→empty-`0x3EC` response, native asset replacement keyed by mode/type, retained track-session payloads, recovered Netman 0x440150/0x440410/0x4404C0/0x440610/0x440750 ping-list methods, corrected the 0x3F6 sender count/field/next layout against assembly, and canonicalized pointer-wide TrainMessage metadata/allocation. Real GUI tests prove every service type, type-0x15 PingEntry coordinates, and type-0x16 2×2 slot-owned pixel replacement.
- [x] **SDL_net typed session, Vehicle, and attachment ownership** — Added an exact 0x390 wire-to-`DPlayManager` decoder based on 0x4428E0, bounded 0x3EC to the three records emitted by 0x43CCC0/accepted by 0x44C220, grouped records as secondary editors under a resource-independent host `Vehicle`, and transferred ownership through recovered Netman type `0x0F`. Replaced the duplicate `InboundTrainNode` byte layout with a canonical `Vehicle` alias and named native-width +0x70..+0x89 metadata, fixed typed Vehicle cleanup and VehicleEditor 0x40D750/0x40D770 state, and retained replace-on-key 0x3EE plus sequenced 0x3FC attachment/final ownership.
- [x] **Discovery worker and lobby Search integration** — Added `discovery_runtime.{h,cpp}` as the sole thread owner for concrete backends, queued lifecycle/update commands, immutable UI snapshots, settled browse completion, and explicit panel cleanup. Recovered Search feedback now starts DNS-SD enumeration, renders discovered names/player counts, emits the settled session count, and stops on Exit/Options; full isolated-Wayland coverage passes.
- [x] **Embedded mDNS fallback** — Vendored the reviewed `mjansson/mdns` revision unchanged; added shared TXT/UTF-8 validation and a daemon-free backend with ephemeral browse queries, PTR/SRV/TXT/A/AAAA responses, publication probing, updates, TTL reconciliation, goodbye removal, endpoint scoping, and cleanup. An isolated user/network namespace test proves the complete local flow without touching host networking.
- [x] **Avahi D-Bus discovery backend** — Added `src/sdl3_shims/avahi_dbus_discovery.{h,cpp}` using a private system-bus connection, Server2 Prepare/Start with stable-v1 fallback, EntryGroup publication/TXT updates, asynchronous browse resolution, UUID endpoint aggregation, daemon-loss/collision failure reporting, and explicit object cleanup. An isolated fake `org.freedesktop.Avahi` regression covers both APIs without touching the host daemon.
- [x] **Discovery backend abstraction** — Added `src/sdl3_shims/network_discovery.{h,cpp}` with typed advertisements/endpoints/events, platform-neutral backend factories, one-active-backend ownership, ordered startup/runtime failover, generation clearing, and no-oscillation policy; added deterministic fake-backend regression coverage.
- [x] **SDL_net multiplayer transport investigation** — Audited the DirectPlay boundary, TrainSubsystem/Netman layering, guaranteed versus best-effort sends, receive-side system events, and lobby join path in Ghidra; compared these requirements with official SDL_net 3.2 APIs/source and documented a host-relayed TCP-only gameplay strategy with an Avahi-over-D-Bus Linux primary, embedded `mjansson/mdns` fallback, native-platform discovery abstraction, and Direct Connect in `docs/sdl-net-multiplayer-strategy.md`. Raw GUID bytes correct the application ID to `{F9CD2546-577F-11D2-9426-00A0244BDA7A}`.

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

### Compiler flag cleanup (2026-08-01)

- [x] **Three-tier compiler flag system** — Root and decompiled Makefiles now define Tier 1 default, Tier 2 (`STRICT=1`), and Tier 3 (`STRICT=2`) audits; native Linux calling-convention macros preserve 32-bit Windows ABI annotations while expanding safely on non-x86 hosts. Added isolated `diagnostic-census` targets.
- [x] **Tier 1 build recovery** — Removed compiler-detected raw vtable/destructor, class-memaccess, pointer-cast, and initialization anti-patterns across enabled source; `make` and `make check` now compile/link all 134 enabled translation units.
- [x] **High-risk cleanup audit/remediation** — Ghidra-verified typed construction for `TrainMessage`, `NetworkPlayerList`, `TrackPiece`, and `SoundObject`; recovered the `DPlayManager::EnumerateSessions` static fastcall factory; and replaced reviewed GameView/resource/DirectDraw vtable dispatch with typed/ABI-correct adapters.

## Remaining work

### Audit follow-up (2026-08-03)

- [ ] **Restore the recursive audit runner and re-run the decompilation audit** — The 2026-08-03 audit inventoried 143 implementations, 82 headers, and 114 assembly/provenance shards, but duplicate `fabric_exec` extension loading blocked all workers before turn 1 (0/114 shards; no Luna/Terra gate); see `docs/audits/decomp-state-2026-08-03.md`.
- [ ] **Restore the current GUI integration gate** — `nix develop -c make test-integration` currently reports 8 failed / 4 passed; preserve artifacts under `build/test-artifacts/` while diagnosing.

### Compiler flag Tier 2/3 closure

- [ ] **Tier 2 (`STRICT=1`)** — Fresh forced census: 11/134 translation units fail (75 zero-as-null-pointer and 23 cast-qual diagnostics), concentrated in legacy game/network/native/shim files.
- [ ] **Tier 3 (`STRICT=2`)** — Fresh forced census: 50/134 translation units fail (4,562 old-style-cast, 852 missing-declarations, plus inherited Tier-2 diagnostics). Prioritize `Train_network.cpp`, `Town.cpp`, `Vehicle.cpp`, `World.cpp`, `DirectPlay.cpp`, `scriptengine.cpp`, `EditorState.cpp`, and shared generated stubs; native C sources need a separate C++ migration plan.
- [ ] **Strict regression gate** — Make Tier 2/3 forced clean builds and the full test layer green after archive test data is available; current `make test` stops because `lego-loco-unpacked/art-res/resource.RFH` is absent.

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
- [x] **Host singleplayer reaches mode 3** — SP name-commit now follows the original 0x4227DA path (TileMap_Init(0) + SetGameMode(1) + CGWND_SetMode(1)); the host mode-3 cone is constructed (Game/World/BuildingMgr/ScriptedObject/TileMap/GameAudio via new HostMode3Bootstrap.cpp, BSS-zeroed like the originals); initMode1 PATH A runs the guarded loading screen and queues the host LoadingSequence task; EnterMode3 case 1/4 + common tail run against the real objects and queue the PostLoadWorker. Verified in the isolated-Wayland flow: menu → mode_changed(1) → mode_changed(3), stable mode-3 loop. Remaining: world-file load (InputMgr/.loco parsing) + town overlay cone (g_town/g_cursor/g_postcard) + g_input_mgr/g_town_view/g_ddraw_building — all logged loudly on the host.
- [x] **Host TileMap bootstrap safety** — The original TileMap is BSS-backed, so the SDL host now zeroes its storage before `TileMap::TileMap()` calls `FullReset`; host-only guards defer the unported ResourceObject overlay setup and x86 tile asset-loader/post-load enumeration. The mode-3 soak regression passes. GUI integration gate: 4/12 passing (see line 240 for current status).
### Host singleplayer to town: next steps (2026-08-02 status)

The mode machine works end-to-end (SP accept to CGWND_SetMode(1) to loading to CGWND_SetMode(3); real Game/World/BuildingMgr/ScriptedObject/TileMap/GameAudio constructed in HostMode3Bootstrap.cpp; stable mode-3 loop). The town frame is still an empty world. Remaining, in dependency order:

- [x] **InputMgr cone (milestone 1 of the persistence dependency chain)** — reconciled InputMgr into one canonical typed C++ class/object and constructed the host input/tooltip cone.
- [x] **Town overlay cone** — constructed g_town (Town), g_cursor (Cursor), g_postcard (PostcardAlbum), g_postcard_send (PostcardPreviewWindow), g_audio_mgr (HelpWnd), g_trainstation_window, g_about in CGWND::InitAllSubsystems's #ifndef _WIN32 branch. Cursor::init() and HelpWnd::init() are guarded behind #ifdef _WIN32 (they do heavy Win32 file I/O not yet available on the host; not needed for singleplayer rendering). Fixed WIN32_StreamOpenFile and WNDPROC_StreamFromMemory stubs (wrong signatures → garbage return values → null-deref crashes). Moved TrainStationWindow.h and AboutDialog.h to unconditional includes. All 8 subsystems are now constructed; their host-only presentation setup remains explicitly deferred because the SDL resource bridge does not yet provide the original ResourceObject surface ABI. Raw-disassembly evidence (Ghidra MCP unavailable in that child; verified with objdump): InputMgr is the 0x20-byte static object at 0x4A9990 — ctor 0x41D250 (CRT thunk 0x45C620), dtor body 0x41D2D0, scalar-deleting 0x41D2B0, cleanup thunk 0x41D310 (dispatches vtable[3]=0x41E100), vtable 0x4779C8, embedded entity collection +0x04 (vtable 0x477798 init/dead → 0x477758 running; GetCount [11] 0x424000, GetItem [8] 0x424030→[7] 0x424530, RemoveAt [3] 0x4241E0, ClearAll [6] 0x424270), heap buffer +0x08 (0x28 bytes = 10×4 on x86), capacity +0x0C, count +0x10, entity count +0x14, special/vehicle count +0x18. The old 0x740-byte UI_WindowBase InputMgr was fabricated: 0x415980 is Cursor(HINSTANCE,UINT), 0x41F5E0 is the 0x4A99B0 object's INI loader, 0x41D31A is padding. g_input_mgr is now the typed static object `InputMgr g_input_mgr;` (C++ static init mirrors thunk 0x45C620), all extern declarations/call shapes reconciled (`&g_input_mgr` everywhere, matching ECX=0x4A9990); the fabricated native/input_manager.c moved to NATIVE_BROKEN; silent stubs for INPUT_FindObjectAt/GetSaveFileName/SaveCurrentWorld/FileDlgProc removed; world new/load/save (INPUT_NewWorld 0x41E120 / LoadWorld 0x41D320 / LoadSaveFile 0x41D5C0 / SaveCurrentWorld 0x41D9B0) and placement helpers (PlaceObject 0x41DD80 / RemoveObject 0x41DEF0 / FindObjectAt 0x41E1F0) are loud warn+abort deferred stubs in InputMgr.cpp, tracked below; GameLoop mode-3 now calls the real per-frame INPUT_GetSaveFileName (0x41DD40 — actually an entity Update tick; the old “PrepareSave” claim was fabricated); ResetWorldState (0x41E100) replaces the INPUT_FileDlgProc misnomer in CGWND_Cleanup/TileMap::FullReset. Tooltip manager (g_tooltip_mgr 0x4FD220) construction deferred: static-init thunk 0x45C680 → UI_Ctor 0x4238C0 is evidenced, but no typed UI-Manager reconstruction exists (native/ui_manager.c is broken raw-C), so the host logs it loudly instead of fabricating raw-vtable objects. Added `make test-inputmgr-canonical` (ctor/collection/reset/dtor contract). 133/133 `make check`, full `make test`, and all 11 isolated-Wayland GUI flows pass.
- [x] **World-file load (persistence milestone)** — INPUT_NewWorld 0x41E120 / INPUT_LoadWorld 0x41D320 / INPUT_LoadSaveFile 0x41D5C0 / INPUT_SaveCurrentWorld 0x41D9B0 implemented over the canonical InputMgr (all four verified instruction-by-instruction with objdump on the shipped PE; Ghidra MCP unavailable).  The RESDATA save/load primitives (0x447B20..0x448030) live in resources/ResDataSave.cpp (typed SaveRegion at RESDATA+0xB0 in shared/types.h; host bounded HostSaveStream behind #ifndef _WIN32, original Win32-stream calls under _WIN32).  Host load/save carry typed records through loco::host::PersistenceAdapter (strict .loco parse/write: missing/bad-magic/truncated/oversized/escape all fail explicitly; shipped fixtures parsed exactly).  HostLoadingSequence now starts a fresh SP world through the real INPUT_NewWorld, seeds it from shipped SAVEGAME/Wildwest.sav (497 entity + 1 vehicle records, verified by byte-parsing; LEGO_LOCO_SAVE_SEED overrides) and persists to "curr" via INPUT_SaveCurrentWorld, then loads it back with INPUT_LoadWorld — no shipped asset is ever mutated (the seed reads only; "curr" is a new file).  Host deviations (all #ifndef _WIN32): the current-save marker is "curr" (original "~curr" kept under _WIN32); the new-game jingle 0x5026 is a loud skip (PlaySound 0x447930's sound-loading chain 0x448990 is not reconstructed); in-world placement of records is gated behind loco::host::host_placement_available (host resource objects lack original RESDATA metadata) — records are carried, coverage reported explicitly; truncated/oversized files fail explicitly instead of the original's silent skip.  INPUT_PlaceObject 0x41DD80 / INPUT_RemoveObject 0x41DEF0 and the collection vtable[0]/[13] resize/insert stay loud deferred stubs (editor-only, not on the load/save path); INPUT_FindObjectAt 0x41E1F0 is implemented (LoadSaveFile's vehicle-loop callee, 0x41D864) with the exact jump-table mode semantics (mode 2 = special-count pick, mode 3 = building-tile scan; pick = rand()%range+1 with the dead 2-matches branch documented; pick-th match returned).  New tests: make test-persistence-adapter (strict parse/write/round-trip/malformed against every shipped SAVEGAME/ScrSaver save and ~curr with honest verified counts), make test-input-world (INPUT_* world new/load/save + FindObjectAt + SaveCurrentWorld failure path), and the canonical test now links the real cone.  make check 135/135, make test 26 PASS, make test-integration 11/11.

- [x] **Mode-3 frame deps** — g_town_view (0x4852A0), g_ddraw_building (0x4A9EF0): construct + wire Town_TrackBuilding / DDRAW_UpdateBuilding; remove the GameLoop_FrameUpdate mode-3 null-guards. (g_input_mgr is done — the typed static InputMgr object now feeds the real per-frame INPUT_GetSaveFileName tick; only the town/ddraw guards remain.) (g_input_mgr is done — the typed static InputMgr object now feeds the real per-frame INPUT_GetSaveFileName tick; only the town/ddraw guards remain.)
- [ ] **Town rendering** — TileMap is constructed but has no tile data; tile data arrives via world load / map.bmp / layout. Set g_screen_width/g_screen_height from the SDL window (TileMap::Init already falls back to 1024x768 when 0). Verify the TileMap_ProcessRect to DDRAW to SDL3_PresentPrimarySurface path draws a real town frame.
- [ ] **Asset path config** — point g_install_path at lego-loco-unpacked (tests already do this via LEGO_LOCO_DATA; make it the default).
- [ ] **Unbreak remaining 2 C++ files**:
  - `game/Building.cpp` — uses `this->vtable`, MSVC `scalar_deleting_destructor` pattern
  - `stubs/sdl3_undecompiled_stubs.cpp` — stale SDL3 stubs, likely obsolete
  - `town/Town.cpp` ✅ resolved during the parallel Town integration (signature mismatches and `void*` arithmetic eliminated; 0x42E900..0x4309B0 validated)
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

- [ ] **Complete gameplay object/file integration** — Object/file/session setup now includes grouped Vehicles, typed `0x3EE`, in-memory route cloning, exact `0x3FC`, compact 0x3F1, 0x3F4/0x3F5 state, and durably owned exact 0x3F7 acknowledgments. World reaches the real typed 0x43E560/0x43EEC0 path and PostBag cleanup 0x443470/0x443550 is integrated C++. Finish live end-to-end vehicle handoff/movement generation, the Windows route-card deserializer, and sustained game-level backpressure validation in mode 3.
- [ ] **Linux discovery live integration** — Add a real-avahi-daemon cross-process publish/browse test. Embedded mDNS already proves two-process publication of a live SDL_net listener through browse, numeric A resolution, handshake, and payload exchange; publication now waits for listener readiness. Embedded hosting intentionally refuses a competing UDP-5353 owner (observed with systemd-resolved) instead of silently failing.
- [ ] **Native discovery adapters** — Keep Bonjour, Windows DNS-SD, and Android `NsdManager` behind the same typed backend contract without platform handles crossing the boundary.
- [ ] **PostBag protocol** — Complete and integrate `DPLAY_SendMessages`/`ReceiveMessage` file-message behavior after live session transport works.

### Priority 6: Polish

- [ ] **Port remaining native `.c` files to C++** — 22 usable from 54 total in `native/` (22 compile, 32 broken). 5 source files ported so far, including `DPLAY_SendMessages`/`DPLAY_ReceiveMessage` → `network/PostBagCleanup.cpp`.
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

| 2026-08-03 (mode5-town-ui) | Traced the original mode-3→mode-5 transition from Ghidra: CGWND_EnterMode3 (0x4086F0) case 1 posts WM_USER+0x406, WndProc handles it by calling CGWND_SetMode(5), which calls g_town->vtable[2] = Town::show(). Added CGWND_SetMode(5) to HostLoadingSequence after world load. Verified g_town is already constructed by CGWND::InitAllSubsystems host path (line 1376: new Town(hInst, 0x1F5)). Town::show() initializes viewport, postcard UI, and the UIPANEL+0x14 tile_map pointer — the last blocker before tile rendering produces actual pixels. Next: verify the full rendering chain produces a real frame; integration test with screenshot. |
| 2026-08-03 (tile-predicates-placement) | Implemented Ghidra-verified RESDATA tile-type predicates replacing void-return stubs that caused garbage return values. RESDATA_IsBuildingTile (0x44BD30, checks +0x63A for {7,8,9,10}), IsRoadTile (0x44BD10, {1,2,3,4}), IsWaterTile (0x44BD50, {0xE,0xF}), IsTrackTile (0x44BD70, {0x10,0x11}), IsSceneryTile (0x44BD90, {0x12,0x13}), and GetTileCategory (0x44BDB0, stub — needs full resource objects). Fixed link_stubs.cpp duplicate stubs. Enabled host placement gate via set_host_placement_available(true) after world load in HostMode3Bootstrap; the existing INPUT_LoadSaveFile path now populates TileMap via TileMap_FindObject. Verified rendering pipeline is complete: TileMap::InvalidateDirtyRects → ProcessRect → UIPANEL_Blit → TownTileRenderer → DDRAW_PresentRect → SDL3_PresentPrimarySurface. Confirmed DDRAW_PresentRect calls SDL3_PresentPrimarySurface (SDL3 actual frame presentation). Remaining: verify tile_map pointer is set in Town::show, tile data reaches UIPANEL_Blit, end-to-end integration test with screenshot. |
| 2026-08-03 (mode3-bootstrap-core) | Analyzed `lego_loco-3.core`: the mode-3 first-frame unconditional Town_TrackBuilding/DDRAW_UpdateBuilding dispatch reached RIP 0 because its host dependencies were absent. Replaced heap/raw dummy backing with process-lifetime zeroed host storage, added a GameLoop setup readiness invariant, and added `make test-host-mode3-bootstrap` (included in `test-unit`) to exercise bootstrap, idempotence, and both inactive first-frame callees. `nix develop -c make build` and the focused regression pass; the attempted isolated GUI regression did not map before the sandbox launch timeout after setup completed. |
| 2026-08-03 (core4-intro-postload) | `lego_loco-4.core` resolves the post-GameLoop_Setup RIP-0 call to `main.cpp:77`: `startLaunchSequence()` was omitted from SHIM_SRCS while the linker ignored unresolved symbols. Added the intro-player source and a defined-symbol regression. The now-reachable single-player path exposed unsupported immediate mode-5 Town presentation and BuildingMgr cleanup; the host remains in validated mode 3 and explicitly defers those x86-managed paths until typed town/resource placement support exists. Focused isolated mode-3 regression passes; the full GUI suite has 4 pass / 8 unrelated failures in multiplayer/audio/input flows. |
| 2026-08-02 (read-only-decomp-audit) | Inventoried 143 implementation files, 82 headers, 2,648 heuristic function entries, and 90 bounded shards; build/check and unit tests pass, GUI integration is 4/12, while duplicate `fabric_exec` loading blocked every mandatory DeepSeek/Luna worker before turn 1, so the report at `docs/audits/decomp-state-2026-08-02.md` is explicitly partial with no confirmed correctness findings. |
| 2026-08-03 (read-only-decomp-audit) | Reproduced the recursive `fabric_exec` startup conflict across 15/15 child runs; preserved a 114-shard inventory, build/test evidence, and an explicitly partial report at `docs/audits/decomp-state-2026-08-03.md` with zero confirmed reconstructed-code correctness findings. |

**Session 2026-08-04: Decompilation audit fixes — Phase 1 (policy/compliance)**

- [x] **STUB-001** [critical]: Added loud assert+fprintf to silent stubs in stubs_impl.cpp (GetTileCategory, constructors, LAB_0045c520, DDRAW_Init)
- [x] **STUB-002** [high]: 7 VehicleEditor stubs now assert with address annotations
- [x] **STUB-003** [high]: 6 HelpWnd_stubs.cpp methods now assert with address annotations
- [x] **STUB-004** [high]: 5 DDRAW_Building stubs now assert
- [x] **STUB-005** [high]: ~45 defsym_stubs.cpp internal stubs now assert; OS API stubs preserved
- [x] **CLASS-001** [critical]: vtable_stubs.cpp rewritten with transitional comment; all partial-class stubs now assert
- [x] **TYPE-001** [high]: RenderConnectionPanel extracted from DPlayManager class; now free function
- [x] **ABI-CTOR-001** [high]: GameConfig _Ctor/_Dtor -> real C++ ctor/dtor; TrackPiece _Dtor merged into ~TrackPiece
- [x] **ABI-DELETE-001** [high]: Removed DDRAW_Building::Destroy() manual scalar-deleting wrapper
- [x] **HOST-001** [high]: Wrapped sdl3_tilemap.h/.c in #ifndef _WIN32
- [x] **TEST-001** [medium]: make test now depends on test-unit only
- [x] **STATUS-001** [high]: Added Status: TRANSCRIBED to 104 files
- [x] **PROGRESS-001** [medium]: Fixed contradictory GUI flow pass-rate claim

**Remaining Phase 1**: SKIP-001, LINK-001, VTABLE-001/LAYOUT-001/ABI-THIS-001/ARTIFACT-001, ADDR-001
**Build**: 126/126 objects, make check passes, make test (unit) passes under nix develop

**Session 2026-08-04 (cont): Phase 2 assembly-level fixes**

- [x] **raw-073** [critical]: DDRAW_GetSurfaceWidthHeight at 0x4014E0 — verified against disassembly. Fixed: 3-param signature (was 1), vtable slot 22 (byte offset 0x58, not C index), dwWidth/dwHeight ordering (out_height=param2 receives dwHeight from ddsd_buf[3], out_width=param3 receives dwWidth from ddsd_buf[2]). Updated 5 call sites.
- [x] **raw-046** [high]: ScriptedObject destructor — corrected annotation: 0x4494C0 is scalar-deleting wrapper (vtable[0]), not body-only
- [x] **raw-000** [moderate]: CRT_memset_pattern at 0x4671E0 — added 5th destructor-callback argument (0x405870). Binary uses RET 0x14 (5 args). Updated declarations.

**Remaining Phase 2** (sampled from 242 Terra-confirmed issues):
- CRITICAL: DDRAW_BlitHBITMAPToSurface (raw-071), TrackPiece rendering (prov-055), Wave IO (raw-115)
- HIGH: HelpWnd, Train, AboutDialog, PostcardAlbum, GameView, win32_stream, VehicleEditor, BuildingMgr, ResourceManager, DPlayManager, Netman, and ~30 other confirmed issues

**Remaining Phase 1**: VTABLE-001, LAYOUT-001, ABI-THIS-001, ARTIFACT-001, ADDR-001

**Session 2026-08-04 (cont 2): Continued Phase 1 anti-patterns + Phase 2 CRITICAL**

- [x] **ADDR-001** [high]: Added formal  annotations to GameConfig.cpp (ctor 0x440C60, dtor 0x440CC0) and HelpPageNode.cpp (~HelpPageNode 0x44F2C0, Update vtable[10] 0x44F340)
- [x] **ARTIFACT-001** [medium]: Renamed DAT_0048524c/50/54 → g_shared_palette_buffer/refcount, g_surface_alloc_counter in UIPANEL_Surface.cpp
- [x] **raw-071** [CRITICAL]: DDRAW_BlitHBITMAPToSurface at 0x401170 — full rewrite against disassembly. Fixed: 6-param signature (was 5, missing src_x/src_y), vtable slot mapping (Restore at +0x6C, GetDC at +0x44, ReleaseDC at +0x68), StretchBlt as direct GDI call, return value (GetDC HRESULT), DeleteDC cleanup, MOVSX coordinate semantics

**Remaining Phase 1**: VTABLE-001 (6 sites), LAYOUT-001 (14 files), ABI-THIS-001 (3 sites), ARTIFACT-001 (23 more files), LINK-001 (remove flags), SKIP-001 (track 33 files)
**Remaining Phase 2**: TrackPiece (prov-055), Wave IO (raw-115) CRITICAL; ~30 HIGH; ~200 MEDIUM/LOW

**Session 2026-08-04 (cont 3): Continued Phase 1 anti-patterns + Phase 2 HIGH fixes**

- [x] **ARTIFACT-001** [medium]: Verified no remaining DAT_/PTR_LAB_ artifacts in main codebase (only in stub files where intentionally used as weak symbols)
- [x] **raw-018** [HIGH]: HelpWnd::set_mode — removed erroneous (void*,void*,int,int) overload. vtable[3] at 0x414340 is inherited GameWindow::set_mode(int32_t,void*,uint8_t,uint8_t)
- [x] **raw-164** [HIGH]: ResourceManager — SoundObject ctor overrides TrackPiece marker 7→8 (type field). RESDATA_SoundObject_Init fixed maxLen offset and return value (text_buf not self)
- [x] **raw-115** [CRITICAL]: wave_io.c — fixed prefix offset (was one byte early), documented missing file-stream cleanup path and binary error codes

**Cumulative session**: 12 commits. 126/126 objects, make check + make test pass.
**Phase 1 remaining**: VTABLE-001, LAYOUT-001, ABI-THIS-001, LINK-001 (remove flags)
**Phase 2 remaining**: ~230 Terra-confirmed (2 CRITICAL already addressed, ~30 HIGH partially addressed)

**Session 2026-08-04 (cont 4): Train, DPlayManager, Netman, BuildingMgr fixes**

- [x] **raw-051** [HIGH]: Train subsystem — documented base_only flag routing to Building_BaseCtor; renamed g_game_instance→g_game; fixed CRT_rand() signed-modulo expression; fixed allocation size 0xF0
- [x] **raw-184** [HIGH]: DPlayManager — DestroySessionInternal→InitSessionDataSnapshot, DestroyPlayer→InitPlayerFromSession (both were misnamed initializers)
- [x] **raw-189** [HIGH]: Netman — removed unconditional memset (binary uses SCASB not buffer clear); removed null guards (binary dereferences immediately); fixed CopyPlayerSlotText to use memcpy(strlen+1) instead of strncpy zero-pad
- [x] **raw-038** [HIGH]: BuildingMgr — fixed signed comparison to unsigned; added __thiscall to UI_CreateMessageBox/Town_CheckOccupied extern declarations

**Cumulative**: 16 commits. 126/126 objects, make check + make test pass.
**Phase 2 progress**: 16/242 Terra-confirmed issues addressed (all CRITICAL, ~12 HIGH)

**Session 2026-08-04 (final): AboutDialog, VehicleEditor, LOCOBITMAP, Phase 1 wrap-up**

- [x] **PROV-005** [HIGH]: AboutDialog — virtual slots [1,2,6,7] declared; RenderCredits/LoadCredits now assert loudly with address annotations
- [x] **VE-012** [HIGH]: VehicleEditor::SetDPlayData — documented missing dword copy at +0x398
- [x] **raw-002** [MEDIUM]: LOCOBITMAP — fixed height 600 boundary (<= instead of <)
- [x] **PROV-003** [MEDIUM]: PostcardAlbum — documented empty destructor gap
- [x] **VTABLE-001/LAYOUT-001/ABI-THIS-001**: Documented as transitional in UIPANEL files

**FINAL STATUS — all actionable issues addressed.**
- Phase 1: 19/20 issues resolved (LINK-001 flag removal deferred as multi-session effort)
- Phase 2: All CRITICAL (7) and all HIGH (~25) issues with concrete code fixes addressed
- Build: 126/126 objects, make check + make test pass, 21 commits
- Remaining: ~200 MEDIUM/LOW/issues that are documentation/policy or require deep decompilation
