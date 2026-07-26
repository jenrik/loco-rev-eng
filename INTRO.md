# Lego Loco — Reverse Engineering Introduction

## What is this?

This is a complete reverse engineering of **Lego Loco** (1998, Intelligent Games), a train-themed city-building game. The binary `loco.exe` (~460 KB, x86 32-bit PE) has been fully decompiled and annotated in a Ghidra database. Every function in game code (0x401000–0x465000) now has a meaningful name and decompiler comment.

## Opening the Ghidra Database

The Ghidra project lives at `./lego-loco-unpacked/Exe/ghidra_projects/`. The `.gpr` project file is broken ("No load spec found"), so always open the raw binary:

```
open_database:
  file_path: "/home/user/projects/v43/jenrik/lego-loco-rev-eng/lego-loco-unpacked/Exe/loco.exe"
  database_id: "loco2"   // use a fresh ID each session
```

Then wait for auto-analysis. Never use `force_new: true`.

## Calling Convention Quick Reference

The game was compiled with MSVC 6.0 (Visual C++ 98). Three conventions appear:

| Convention | How to spot it | Example |
|---|---|---|
| **`__thiscall`** | ECX = `this`, callee pops stack args (`RET N`) | Instance methods on classes |
| **`__fastcall`** | ECX = `this`, no stack args, plain `RET` | Methods with zero stack parameters |
| **`__cdecl`** | No ECX usage, caller cleans stack (`ADD ESP, N` after `CALL`) | Global/static functions |
| **`__stdcall`** | No ECX usage, callee pops stack (`RET N`) | Win32 API callbacks, some COM wrappers |

Ghidra conflates `__thiscall` with `__fastcall` when there are zero stack args — both use ECX with plain `RET`, so the ABI is identical. If you see `RET N` (N > 0), it's `__thiscall`.

**MSVC scalar deleting destructor pattern:** Virtually every class has this at vtable[0]:
```c
void * __thiscall Class_ScalarDtor(Class *this, int flags) {
    Class_BaseDtor(this);
    if (flags & 1) GLOBAL_free(this);
    return this;
}
```

## Memory Map

| Address Range | Subsystem | What it handles |
|---|---|---|
| `0x401000–0x404fff` | **Graphics** | LOCOBITMAP (bitmap loading, blitting, PostBag album persistence), SURFACE (UI sprite management), GFX (tile grid rendering) |
| `0x406000–0x413fff` | **Core** | CGWND (main game window, mode state machine, subsystem init, cleanup), GAME (game loop, input handling, screen mode, audio) |
| `0x414000–0x42bfff` | **Input/UI** | CURSOR (custom cursor rendering, editor), INPUT (keyboard/mouse mapping, easter eggs, save/load UI), UI (main menu, window base class), UIPANEL (panel rendering, surface management, scroll panels) |
| `0x42c000–0x43cfff` | **Game World** | Town (tile rendering, viewport, postcards, buildings), BuildingMgr, Trains (network trains, multiplayer train movement) |
| `0x43d000–0x445fff` | **Network** | NETMAN (DirectPlay networking, player slots, message queue, game state sync) |
| `0x446000–0x44ffff` | **Resources** | RESMGR/RESDATA (resource manager, file loading, sound, scripted objects, game vehicles) |
| `0x450000–0x45ffff` | **DDraw/Audio** | DDRAW (DirectDraw surface init, building sprites, selection popup), GameAudio (DirectSound, channel management), GameLoop_FrameUpdate |
| `0x460000–0x465fff` | **Win32 Platform** | WndProc, StreamFile, StreamMem, critical section wrappers |
| `0x466000+` | **CRT** | MSVC C runtime (printf, malloc, math, locale, exceptions) — not game code |

## Architecture Overview

### The Central State Machine

`CGWND_SetMode` (0x408130) is the heart of the game. It has a 10-entry jump table:

| Mode | Name | What happens |
|---|---|---|
| 0 | Init | Silent accept (set at startup) |
| 1 | InitGame | Two paths: fresh start (progressive loading screen), or world reload (load .sav → mode 3) |
| 2 | MainMenu | Show main menu UI overlay and cursor |
| 3 | EnterMode3 | Transition to gameplay from any mode; handles per-mode cleanup |
| 4 | ExitBuild | Deselect game object, destroy buildings, reset tooltips |
| 5 | Town | Show town view |
| 6 | Postcard | Show postcard view |
| 7 | Cursor | Show cursor/vehicle editor |
| 8 | SaveState | Records previous mode for audio manager (no visual change) |
| 9 | PostcardSend | Send postcard (network) |
| 10 | Quit | Play quit sound, shutdown DirectDraw, post WM_QUIT |

### Startup Sequence (WinMain area, ~0x462000)

```
CGWND_Ctor → CGWND_ResetState (read EXE version) → CGWND_ParseCmdLine (easter eggs)
→ CGWND_ShowMainMenu (read screen res, INI config, FPS limits)
→ CGWND_InitGame (validate display: color depth, mouse, screen width)
→ CGWND_InstallPathInit (registry → lego.ini → create data dir)
→ GameLoop_Setup:
    CGWND_SetMode(0)
    CGWND_SetMode(1) → CGWND_InitMode1 (loading screen, progressive subsystem init)
    CGWND_RegisterWindowClass ("LEGO_LOCO" window)
    CGWND_InitAllSubsystems (creates 8 subsystems with SEH)
    GameLoop_FrameUpdate (main loop: NETMAN tick → World tick → Game_Update → render)
```

### The 8 Subsystems (created by CGWND_InitAllSubsystems at 0x406F90)

| # | Global | Size | Subsystem |
|---|---|---|---|
| 1 | `g_ui_main` | 0x224 | UI Main Menu |
| 2 | `g_town` | 0x6E0 | Town view |
| 3 | `g_postcard_send` | 0x2C4 | Postcard send window |
| 4 | `g_trainstation_window` | 0x1D4 | Train station window |
| 5 | `g_postcard` | 0x254 | Postcard receive window |
| 6 | `g_cursor` | 0x740 | Cursor/vehicle editor |
| 7 | `g_audio_mgr` | 0x3078 | Audio manager (DirectSound) |
| 8 | `g_about` | 0x1184 | About/help dialog |

Each is created with a two-phase pattern: `operator_new(size)` → `Ctor(this, hInstance, resourceId)` → `Create(this, hWndParent)`.

### The Main Game Loop

`GameLoop_FrameUpdate` (0x45C3C0) runs every frame (~14ms timer):
1. Poll network (`NETMAN_Update`)
2. Prepare resource manager
3. World tick (`World_UpdateTick`) — vehicles, collisions, network sync
4. Hide tooltips
5. `Game_Update` — animation, mouse input dispatch, cursor handling
6. Scripted object update
7. Build-mode specific: track building, building sprites, autosave, building AI
8. Dirty rect invalidation (`TileMap_InvalidateDirtyRects`)

`Game_Update` (0x410840) is the per-frame input dispatch:
1. Animation update (`GameObject_Update`)
2. Aggregate mouse activity (left click, right click, move, screensaver)
3. Screensaver dismissal
4. Left click dispatch (`Game_HandleLeftClick`) — priority: town → DDRAW → scripted → selected → building → tile
5. Right click dispatch (`Game_HandleRightClick`) — type match (cycle animation) or type mismatch (building → audio → tile)
6. Selected-object click dispatch
7. Mouse-move hover (screen-to-world conversion, entity routing)
8. Game-mode-specific cursor handling

### The Object Hierarchy

Game objects use an MSVC virtual inheritance chain:

```
GameObject (0x88 bytes, vtable 0x477820)
├── Entity (extends by ~0x60 bytes, vtable 0x477488)
│   ├── RESDATA (resource descriptor, extends to ~0x638 bytes)
│   │   ├── RESDATA_ScriptedObject (scripted UI objects, type 10)
│   │   ├── RESDATA_GameVehicle (vehicle placement)
│   │   └── RESDATA_GameObject (generic game object)
│   ├── Game (singleton game state, extends to ~0x118 bytes)
│   ├── Building (building AI, occupant management)
│   │   └── Train (network train, extends Building)
│   └── UI_WindowBase (base UI window, ~0xE8 bytes)
│       ├── UI_MainMenu (main menu, 0x224 bytes)
│       ├── CGWND_GameSetup (lobby, 0x260 bytes)
│       ├── Cursor (cursor editor, 0x740 bytes)
│       └── TrainStationWindow (0x1D4 bytes)
└── (standalone: Town ~0x6E0, GameAudio ~0xB8, NetMan ~0x804)
```

Key struct offsets (Entity base):
- `+0x00` vtable pointer
- `+0x04` type ID
- `+0x18` initialized flag
- `+0x24` visible/mode flag
- `+0x28` animation index
- `+0x2C` blit flags / action state
- `+0x40` parent/resource descriptor pointer (`RESDATA*`)
- `+0x48` audio channel handle
- `+0x4C`/`+0x50` world position (x, y)

### The Rendering Pipeline

1. **Tile rendering:** `Town_DrawTiles16bpp_*` functions convert 8bpp palette-indexed tiles to 16-bit DirectDraw surfaces with transparency. Multiple variants handle strided, reversed, checkerboard, and staggered (isometric) layouts.

2. **UI compositing:** `UIPANEL_Blit` (0x42B050) is the central dispatcher — 105+ callers, 11 tile drawing modes controlled by a flags parameter. `UIPANEL_EndPaintEx` handles cursor overlay compositing with viewport clipping.

3. **Cursor rendering:** `Cursor_Render` (0x414C20) performs a 3-blit sequence: draw cursor sprite to primary with color key, save region to backbuffer, composite onto primary. `Cursor_RenderWithViewport` adds viewport clipping.

4. **Dirty rect system:** `Cursor_UpdateDirtyRect` unions old and new cursor rects, then `TileMap_InvalidateDirtyRects` triggers redraws only for changed regions.

### Networking (DirectPlay)

NETMAN wraps Microsoft DirectPlay for peer-to-peer multiplayer. Key concepts:

- **9 PlayerSlot entries** (0x4C bytes each) at `g_netman + 0x518`
- **Two message queue systems:** Train messages (type 6, via `Train_QueueMessage`) for network data, and internal control messages (types 0x05–0x1B) in NETMAN's own queue
- **Message dispatch:** `NETMAN_ProcessMessage` (0x43F2B0) handles 20+ message types via a switch statement
- **Game state sync:** `NETMAN_SyncGameState` updates all 9 player slots from incoming packets; `NETMAN_SendMapData` serializes the tilemap for transmission
- **File transfer sub-protocol:** Messages 0x12 (angle/position), 0x15 (player data + ping), 0x17 (forward ping)

### The PostBag / Album System

A persistent pixel-data cache stored in `<install>/PostBag/AlbIndex_<player>_<category>.ind` files. Player-created custom content (cursors, postcards) is persisted through 9 alphabetically-binned album files (A-C, D-F, G-J, etc.). Managed by `LOCOBITMAP_PixelData_*` functions at 0x401600–0x401FF0.

### Audio

`GameAudio_Init` (0x412C50) sets up DirectSound: enumerates devices, creates primary buffer, allocates 16 channels at PCM 22050Hz 16-bit stereo. `GameAudio_AllocChannel` (0x413210) implements a 5-pass channel allocation algorithm: audit → constraint check → reuse → steal active → priority-based steal.

## Reading the Codebase

### Key Global Variables

| Address | Name | Description |
|---|---|---|
| `0x4FD378` | `g_ui_main` | Main menu UI object |
| `0x4FD37C` | `g_town` | Town view object |
| `0x4FD380` | `g_cursor` | Cursor/vehicle editor |
| `0x4FD3A8` | `g_net_config` | Network game configuration |
| `0x4FD3AC` | `g_netman` | Network manager (NETMAN) |
| `0x4FD3B4` | `g_pixel_data` | PostBag album pixel data |
| `0x4851F4` | `g_game_mode` | Current game mode (0–10) |
| `0x485210` | `g_is_fullscreen` | Fullscreen flag |
| `0x4A98B0` | `g_world` | World object |
| `0x4AAD08` | `g_tilemap` | TileMap object |
| `0x4A9990` | `g_input_mgr` | Input manager |
| `0x4855E8` | `g_resmgr` | Resource manager struct |

### Decompiled C Files

`src/decompiled/` contains 479 cleaned C files, one per function (or small function group). Naming convention: `[subsystem]_[functionname].c`. These are not compilable — they're annotated decompilation for reading. Every struct field access uses `/* +0xNN */` offset comments.

### Entity Header

`src/decompiled/entity.h` defines the base `GameObject` and `Entity` structs with known field offsets. This is the starting point for understanding any game object method.

### FUNCTION_MAP.md

A complete function table mapping every address to its subsystem and purpose. Updated during the reverse engineering process but may lag behind Ghidra — the Ghidra database is authoritative for function names.

## Best Starting Points

If you're new to the codebase, read these functions first:

1. **CGWND_SetMode** (0x408130) — understand the 10-mode state machine
2. **CGWND_InitAllSubsystems** (0x406F90) — see all 8 subsystems created
3. **Game_Update** (0x410840) — the per-frame input dispatch pipeline
4. **GameLoop_FrameUpdate** (0x45C3C0) — the main game loop
5. **UI_MainMenu_SetState** (0x4208F0) — the main menu's 7-state UI machine
6. **NETMAN_ProcessMessage** (0x43F2B0) — how 20+ network message types are dispatched
7. **UIPANEL_Blit** (0x42B050) — the central rendering dispatcher
8. **Town_BlitViewport** (0x42CB10) — how the tile viewport is rendered
9. **Building_Update** (0x4327B0) — building AI per-frame logic
10. **GameAudio_AllocChannel** (0x413210) — audio channel allocation algorithm

## Known Bugs Found During RE

- **CGWND_InitGame** (0x406680): Inverted color-depth check — rejects all valid display modes
- **Building_RemoveOccupant** (0x4336A0): Searches 0–7 slots but AddOccupant uses 0–8 — slot 8 occupants leak
- **Game_LoadWaveFile** (0x413660): Double-release of file stream after filesystem fallback
- **Cursor_WaitForBlit** (0x414BB0): Hard `ExitProcess` on blit timeout (~10s) — no recovery
- **NETMAN_ReceivePing** division: Potential modulo-by-zero when `range == 2`
- **BuildingMgr_UpdateAll** (0x434720): Last building in the update chain is never updated (only passed as argument)
- **UIPANEL_ReadPaletteFromBMP** (0x42AF30): Falls back to freeing shared palette unconditionally on private allocation failure
- **Town_CalcScrollRect** (0x42C590): Always returns FALSE regardless of result

## Tools

- **Ghidra** via MCP server — decompilation, disassembly, cross-references, type annotation
- **radare2** via MCP server — quick triage, string search, hex dumps
- Decompiled C files in `src/decompiled/` — cleaned pseudocode for reading

## Running the Game

See `RUNNING_THE_GAME.md`. The game runs under Wine + Xephyr with two binary patches applied to fix window creation issues.
