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
- [x] **Build system cleaned** — `CMakeLists.txt` restored, `flake.nix` simplified

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
  - `build/defsym.args`: 926 --defsym entries for unresolved symbols
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

| Date | Summary |
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
| 2026-07-25 | **Unified build system**: Created root Makefile. Single `make` command compiles 94 source files (64 C++, 25 native C, 5 SDL3 shims) and links into 2.0MB ELF binary. 16 C++ files skipped (known-broken), 31 native files skipped. Auto-detects SDL3 on NixOS. Removed stale CMakeLists.txt / SDL2 port artifacts. |
| 2026-07-25 (late) | Replaced 275 defsym=0 entries with real stubs. Fixed CRT_strtok NULL-return crash. Binary runs full main() → PumpMessages → clean exit. Valgrind: 0 errors. Root cause of heap corruption: 32-bit allocation sizes used on 64-bit. |
| 2026-07-25 (game stubs) | Building class completed: decompiled 7 vtable-gap functions (TeleportTo, StepToward, IsActionComplete, PartyModeUpdate, CheckPlacementCollision, PostMoveDispatch, FindNearestConnectionNode). Updated Building.h fields and virtual methods. Replaced 6 TODO stubs. Removed from sdl3 stubs. 70/70 compile. |
| 2026-07-25 (decomp) | Decompiled GameObject_GetBoundingRect→Entity::GetBoundingRect (0x4583C0), BuildingMgr_CompactCollections (0x434870), Town_PostcardUpdateUI (0x42DE70). Fixed PtInRect scope in Building.cpp, blit_element→BlitElement in Town.h. |
| 2026-07-25 (decomp2) | Decompiled Game_CheckTimeInRange (0x412710), BuildingMgr_CompactCollections (0x434870), Entity::GetBoundingRect (0x4583C0). Unblocked 11 files from broken list (Train, ScriptedObject, Panel, TrackPiece, UIPANEL, UIPANEL_Surface, EditWindow, UI_WindowBase, Netman, Netman_ReceiveSignalChange, Cursor_new_impls). 77→66 C++ files compile. |
| 2026-07-25 | Fixed binutils 2.46 incompatibility: old --defsym used demangled names with parentheses (rejected by new ld). Switched to mangled names from nm -u. Generated 926-entry defsym.args. Binary links clean. |
| 2026-07-25 (eve) | Ported 3 native .c files to clean C++: game_loop_setup.c → core/GameLoop.cpp, config_ini.c → game/ConfigIni.cpp, world_enumerate_assets.c → world/World_enumerate.cpp. Fixed world.h include guard. Added 21 globals + 25+ function stubs to stubs_impl.cpp. Ghidra-validated all 7 functions — found and fixed 3 bugs (missing RegisterWindowClass return check, CRT_timeGetTime missing arg, Config_ReadInt wrong signature). Root-caused and fixed segfault: defsym=0 entries clashed with libc symbols during SDL3 init. Replaced defsym with --unresolved-symbols=ignore-all. Binary now runs full launch sequence + clean exit. 25 native C files remain. |
| 2026-07-25 (ifdef) | Completed #ifdef _WIN32 pivot: added guards to GameWindow.cpp, EditWindow.cpp, Cursor_Editor.cpp, Cursor_internal.h. Extended sdl3_window.h with 25+ missing Win32 declarations + stub implementations. Fixed CGWND_sdl3.cpp which was in BROKEN_SRCS (empty stub linked instead → immediate exit). Unbroke network/test_write.cpp (compiles cleanly). Window opens with blue background, ESC quits cleanly. 72/75 C++ files compile. 3 remaining broken: Building.cpp, Town.cpp, sdl3_undecompiled_stubs.cpp. |
| 2026-07-25 (game-loop) | Wired up real game loop: GameLoop_Setup (all 12 init steps), GameLoop_FrameUpdate (per-frame tick), fixed extern "C" linkage mismatches, fixed RegisterWindowClass SDL3 path, fixed g_main_window initialization. Binary runs full init → pump loop → clean exit without crash. Ready for CGWND_InitMode1 decompilation. |
| 2026-07-25 (initmode1-integrated) | Completed Pass 3 integration: CGWND_InitMode1 → CGWND::initMode1() method. Updated CGWND.h with method declaration + #define backward-compat alias. Bridge functions cleaned up: UI_MainMenu_Create and HelpWnd_Create now use typed method calls (EditWindow::create, HelpWnd::create) instead of raw vtable casts. All field offsets cross-referenced consistent. Status: INTEGRATED. Binary links at 2.4M. |
| 2026-07-25 (gamevehicle-validated) | GameVehicle class validated: all 7 functions (ctor/dtor/StartMoving/Update/AddDestination/RemoveDestination/SetOccupantState) reviewed against AGENTS.md anti-pattern checklist. Fixed void*→Vehicle* typing, added virtual keywords to vtable methods (StartMoving, Update), replaced raw offset access in SetOccupantState with named resource_ptr field, added tile_target field at +0x88, improved vtable layout documentation. Status: TRANSCRIBED → VALIDATED. 70/70 files compile. |
| 2026-07-26 (building-validated) | **Building class validated**: Fixed 9 blocker + 7 warning issues from review. Cross-referenced all function addresses against Ghidra (loco_v8). Key fixes: (1) AddOccupant null-dereference — changed entity+0xF0 to this+0xF0 per binary. (2) BaseCleanup rewritten to match 0x433BE0 (parent from +0x90, search +0xA4 5-slot array, decrement +0x8E child_count, call GameObject_DtorBody). (3) BaseDtor now uses Game_SelectGameObject + RemoveOccupant(via binary 0x432740). (4) CRT_wcsstr signature corrected to wchar_t*. (5) RemoveOccupant renamed to no-arg (reads occupant_ptr internally). (6) 13 void* externs → typed (BuildingMgr*, Entity*, InputMgr*, TileMap*). (7) Entity field accesses converted to named fields (blit_flags, source_rect, resource, sound_res_id, audio_channel). (8) Deserialize copies all padding bytes (_pad_8a[2], _pad_e5[3]). (9) BaseCtor return type void (no more return this). 72/72 files compile. Status: TRANSCRIBED → VALIDATED. |
| 2026-07-26 (building-fixes) | **Building blocker fixes**: Fixed all 12 reviewer issues. (1) CheckTimeout: GameObject::Update() → Entity::Update() per binary vtable[10] at 0x405C40. (2) Deserialize: operator_new → new Building(0) for proper vtable init; signature changed to static Building*; removed _pad_1C/_pad_20 scratch fields. (3) HandleAction action==3 overflow: occupation_level += 2 clamped to 7. (4) Protected constructor: zero-inits occupant_ptr in all paths. (5-12) Implemented 8 virtual methods from disassembly: SetName (0x4344A0), Draw (0x4343B0), OnOccupantReady (0x434260), PartyModeUpdate (0x433220), IsActionComplete (0x432FD0), StepToward (0x432AE0), TeleportTo (0x432940), PostMoveDispatch (0x433CA0), CheckPlacementCollision (0x433860), FindNearestConnectionNode (0x4343F0). Added <cstdlib> for abs(). Updated vtable layout comments with verified addresses. Status: VALIDATED → TRANSCRIBED (new implementations need disassembly-line validation). 72/72 files compile, Building.o OK. |
| 2026-07-26 (netman-cleanup) | **Netman anti-pattern purge (25 issues)**: Rewrote Netman.h (~600→~450 lines): removed all __thiscall/__fastcall from class declarations, fixed ctor (Netman_ctor→Netman(), no void* return), fixed dtor (Netman_dtor(uint8_t)→~Netman(), no flags/GLOBAL_free), split extern "C" block (Win32 API only), deleted ~20 NETMAN_* free-function duplicates, replaced Ghidra auto-labels (RESDATA_→ResourceManager_, GAMESTATE_→EditorState_, RESMGR_→ResourceManager_), removed duplicate m_vehicleList_dup field, fixed field ordering. Rewrote Netman.cpp (~1300→~1400 lines): replaced 30+ literal vtable dispatches with vtable_delete() helper, fixed NULL dereference patterns, fixed signedness in ReceiveTrainPosition, added notes for data_ptr reuse and 1-based car loop, added TODO-stub implementations for 12 missing methods (LoadScenario/ResetNetworkState/StopSession/SendFileTransfer/ReceiveAck/RemovePingEntry/ReceivePing/UpdateLatency/CheckTimeout/HandleTimeout/SerializePlayerData/DeserializePlayerData) + 3 standalone functions. Deleted Netman_fixes.cpp (merged fixes into Netman.cpp). 71/71 files compile. Status: TRANSCRIBED (needs Ghidra validation for stub methods). |
| 2026-07-26 (building-review2) | **Building review fixes (12 issues)**: (BLOCKER 1) Fixed address conflict: OnOccupantReady at 0x434260, MoveToTarget at 0x434399 (from CalcMoveTarget caller list). (BLOCKER 2) Removed BaseDtorWrapper (compiler-generated vector deleting dtor at 0x4327A0), documented in vtable comment. (BLOCKER 3) AddOccupant(void*) → AddOccupant(Entity*). (BLOCKER 4) OnOccupantReady(int) → OnOccupantReady(Entity*), removed (Entity*) cast. (WARNING 1) Added DECOMPILER NOTE for inverted PARTY check in BaseCtor and SetName. (WARNING 2) Added TODO: decompile 0x433860 tile-obstacle check. (WARNING 3) Moved extern TileMap_PathDistance/TileMap_FindTileByType/NodeSet_GetConnectionFlag to file scope. (WARNING 4) Added NOTE for PartyModeUpdate void*/Building* parameter. (WARNING 5) Added TODO: decompile 0x433D98 BuildingMgr collection iteration. 72/72 files compile. |
| 2026-07-26 (cursor-review) | **Cursor anti-pattern purge + missing methods (16 issues)**: Fixed all 9 blockers and 7 warnings from review. (1) Implemented 8 missing methods in Cursor_impls.cpp (set_mode@0x414340, on_show@0x426130, handle_window_paint@0x414A80, init_network_player@0x41A0E0, update_network_names@0x416E00, set_capture@0x414290, update_dirty_rect@0x414FB0, render_with_viewport@0x415440). (2) Constructor: UI_WindowBase_Ctor→member initializer list. (3) Destructor: removed explicit UI_WindowBase_BaseDtor call (compiler auto-chains). (4) Base class method calls: UI_WindowBase_Show/Hide→UI_WindowBase::show/hide(). (5) Free function: Cursor_CleanupEditorSprites→this->cleanup_editor_sprites(). (6) Literal vtable dispatch: ButtonSprite objects use delete; DDraw surfaces kept with COM comments. (7) extern "C" block: split into C-linkage (Win32 APIs) and C++-linkage (game helpers); removed RESDATA_CreateSpriteObject. (8) ODR violation: removed 3 duplicate Cursor stubs. (9) Type fixes: cursor_state cast→cursor_sprite_surface; field_73C→LONG_PTR prev_wndproc. (10) Documentation: #define aliases commented; cached_height icon storage comment improved; prev_cursor_rect title[50] clarified; render() doc-comment stub removed. 72/72 files compile. Status: TRANSCRIBED. |
| 2026-07-26 (directplay-cleanup) | **DirectPlay anti-pattern purge + TRANSCRIBED fixes (26 issues)**: Fixed 20 blockers + 5 warnings + 1 info from independent review. Key fixes: (1) Added Status: TRANSCRIBED tags to DirectPlay.h and DirectPlay.cpp. (2) Fixed type mismatches: _g_primary_surface and _g_dsound_object (int32_t* → void*) to match shared/types.h. (3) Removed all __thiscall/__fastcall annotations from declarations per anti-pattern rule 5. (4) Replaced Ghidra auto-labels: PTR_LAB_00478f88→CLSID_DirectPlay (actual GUID: {0AB1C531-4745-11D1-A7A1-0000F803ABFC}), PTR_LAB_0045f2b0→GUID_SessionDesc (actual GUID from 0x479158: {F9CD2546-577F-11D2-9426-00A0244BDA7A}), DAT_00481218→g_device_path_null. (5) Fixed broken truncation logic: *(char**)&buffer[0x80]=0 → const_cast<char*>(buffer)[0x80]=0 (single-byte write, not dword). (6) Documented s[0x498]=0 redundant zero-clear with DECOMPILER NOTE. (7) Replaced DirectPlay_HandleMessages stub with TODO: decompile 0x45F390 marker + detailed operation notes from Ghidra decompiler. (8) Replaced DirectPlay_SessionMgr dead stub with documentation pointing to AssetMgr.cpp:805. (9) Fixed parameter names: param_1→hInstance, param_2→hWnd, etc. (10) Resolved GetStockObject(0) ambiguity → WHITE_BRUSH. (11) Documented g_client_width-as-RECT pattern and unused client_rect. (12) Fixed address mismatch: Train_network.cpp had 0x00460090, corrected to 0x45F390 per Ghidra. (13) Removed duplicate g_dplay_peer extern. Deferred to INTEGRATED pass: vtable dispatch → virtual methods, raw offsets → named fields, free functions → class methods, void* fields → typed pointers. 74/74 files compile. Status: TRANSCRIBED. |
