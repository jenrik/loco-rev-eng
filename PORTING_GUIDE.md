# Lego Loco (1998) — Linux Port Guide

Developer: Intelligent Games for LEGO Media
Engine: Custom C++ (MSVC, Win32, DirectX 5)
Binary: `loco.exe` — PE32, 1.1 MB, 1,763 functions
Target: SDL2-based native Linux port

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Dependency Map: Win32 to Linux](#2-dependency-map-win32-to-linux)
3. [Port Strategy](#3-port-strategy)
4. [CMake Build System](#4-cmake-build-system)
5. [Major Porting Challenges and Solutions](#5-major-porting-challenges-and-solutions)
6. [Data File Formats: .RFH / .RFD](#6-data-file-formats-rfh--rfd)
7. [Save File and Profile Formats](#7-save-file-and-profile-formats)
8. [Network Protocol](#8-network-protocol)
9. [Step-by-Step Port Roadmap](#9-step-by-step-port-roadmap)

---

## 1. Architecture Overview

### Entry Point and Bootstrap

The game is a Win32 GUI application (`/SUBSYSTEM:WINDOWS`). The real entry is the MSVC CRT stub `entry` at `0x004689e0` which parses the raw command line and calls `WinMain` at `0x00462e90`. For Linux the CRT stub is replaced by a standard `main(int argc, char **argv)`.

`WinMain` performs:

1. Shows a splash dialog (blocking load screen)
2. Calls `CoInitializeEx` to init COM — remove on Linux
3. Constructs the `CGWND` singleton (40 bytes, `0x004061e0`)
4. Calls `CGWND_LoadConfig` — reads `lego.ini` path from the Windows Registry
5. Calls `CGWND_CheckDisplayCaps` — validates colour depth via `GetDeviceCaps`
6. Runs `GameLoop_Setup` to bring up DirectDraw, DirectSound, and all subsystems
7. Runs the two-phase PeekMessage / frame-tick loop until `WM_QUIT`
8. Calls `CGWND_Shutdown` and `CoUninitialize`

### CGWND — Engine Root Object

```
struct CGWND (0x28 bytes)
  +0x00  vtable*         → PTR_FUN_004774c4
  +0x04  HWND hwndDesktop
  +0x08  HWND hwndGame   ← main WS_POPUP fullscreen window
  +0x0C  HINSTANCE
  +0x10  stateFlag
  +0x11  minVehicleFPS   (INI BALANCING/MinVehicleFPS,  default 20)
  +0x12  minBuildingFPS  (INI BALANCING/MinBuildingFPS, default 18)
  +0x13  minMinifigFPS   (INI BALANCING/MinMinifigFPS,  default 16)
  +0x14  minFlyingFPS    (INI BALANCING/MinFlyingFPS,   default 14)
  +0x18  versionMajor / Minor / Build / Revision
```

On Linux `hwndDesktop` and `hwndGame` become `SDL_Window*`. The vtable stays as-is (C++ vtable dispatch).

### Game Loop

The game uses a classic Windows PeekMessage loop driven by a Win32 multimedia timer:

```
timeSetEvent(28ms, 0, TimerCallback, 0, TIME_PERIODIC)
  → sets g_timerFired = 1 (at 0x00485444)

Main loop:
  while PeekMessage:
    TranslateMessage / DispatchMessage
  if g_timerFired:
    g_timerFired = 0
    GameFrame_Update()   ← ticks all subsystems + blits frame
```

Target rate: 28 ms per tick → ~35.7 fps.

A named Win32 event (`CreateEventA("GameLoop")`) at `g_hGameLoopEvent` (`0x004a990c`) acts as a cross-thread synchronisation primitive.

### Subsystem Object Graph

All subsystems are opaque C++ objects stored as global pointers in `.data`. Each object's first field is its vtable; vtable slot 0 is always the destructor (`(*vtable[0])(self, 1)`).

| Global pointer       | Class name              | Size (bytes) |
|----------------------|-------------------------|-------------|
| `g_pDirectDraw`      | `CDirectDrawManager`    | 0x224       |
| `g_pDirectSound`     | `CDirectSoundManager`   | 0x6E0       |
| `g_pInputMgr`        | `CInputManager`         | 0x254       |
| `g_pMovieMgr`        | `CMoviePlayer`          | 0x740       |
| `g_pNetworkMgr`      | `CNetworkManager`       | 0x2C4       |
| `g_pSceneMgr`        | `CSceneManager`         | 0x3078      |
| `g_pWorldMgr`        | `CWorldManager`         | 0x1184      |
| `g_pAnimMgr`         | `CAnimManager`          | 0x1D4       |
| `g_pTimerSvc`        | `CTimerService`         | 0x1C        |
| `g_pConfigMgr`       | `CConfigManager`        | 0xB0        |
| `g_pEventQueue`      | `CEventQueue`           | 0x804       |
| `g_pStringTable`     | `CStringTable`          | 0xBE4       |
| `g_pDebugLog`        | `CDebugLog`             | 0x18        |
| `g_pSaveGameMgr`     | `CSaveGameManager`      | 0x124       |

The resource manager `CResourceMgr` (`g_ResMgr` at `0x004855E8`) is a ~150 KB inline global (not heap-allocated) containing three large inline cache arrays.

### Game State Machine

The integer `g_gameState` (at `0x004851f4`) drives subsystem activation:

| Value | State constant        | Description                          |
|-------|-----------------------|--------------------------------------|
| 1     | `GAME_STATE_INIT`     | Initialising / resetting world       |
| 2     | `GAME_STATE_LOADING`  | Async asset load in progress         |
| 3     | `GAME_STATE_RUNNING`  | Normal gameplay                      |
| 4     | `GAME_STATE_PAUSED`   | Game paused                          |
| 5     | `GAME_STATE_MENU_A`   | Main menu (DirectSound mode)         |
| 6     | `GAME_STATE_MENU_B`   | Main menu (Input mode)               |
| 7     | `GAME_STATE_MOVIE`    | FMV playback                         |
| 8     | `GAME_STATE_SAVE`     | Save-game screen                     |
| 9     | `GAME_STATE_CREDITS`  | Credits / end sequence               |
| 10    | `GAME_STATE_QUIT`     | Posts `WM_CLOSE` → teardown          |

---

## 2. Dependency Map: Win32 to Linux

### 2.1 Windowing and Event Loop

| Win32 API | Purpose | Linux Replacement |
|---|---|---|
| `RegisterClassA` / `CreateWindowExA` | Window creation | `SDL_CreateWindow` |
| `WinMain` + `PeekMessageA` loop | Entry + event pump | `main` + `SDL_PollEvent` loop |
| `SendMessageA` / `PostMessageA` | Intra-process messaging | `SDL_PushEvent` with `SDL_USEREVENT` |
| `DefWindowProcA` | Default window proc | Not needed; SDL handles events |
| `SetTimer` / `KillTimer` | UI repeat timers | `SDL_AddTimer` |
| `ShowWindow` / `EnableWindow` | Window visibility | `SDL_ShowWindow` / no-op (no child windows) |
| `GetClientRect` | Client area size | `SDL_GetWindowSize` |
| `GetDesktopWindow` / `GetSystemMetrics` | Desktop dimensions | `SDL_GetDisplayBounds(0, &rect)` |
| `GetWindowRect` / `ClientToScreen` | Coordinate mapping | SDL viewport math |

### 2.2 Graphics (DirectDraw 5)

| Win32 / DirectDraw API | Purpose | Linux Replacement |
|---|---|---|
| `DirectDrawCreate` + `QueryInterface` | Create DD device | `SDL_CreateRenderer` |
| `IDirectDraw::SetCooperativeLevel` | Exclusive fullscreen | `SDL_SetWindowFullscreen(SDL_WINDOW_FULLSCREEN_DESKTOP)` |
| `IDirectDraw::CreateSurface` (primary) | Front buffer | `SDL_GetRenderTarget` (or window surface) |
| `IDirectDraw::CreateSurface` (back buffer) | Back buffer composite | `SDL_CreateTexture(TEXTUREACCESS_TARGET)` |
| `IDirectDrawSurface::Blt` / `BltFast` | Surface-to-surface blit | `SDL_RenderCopy` / `SDL_BlitSurface` |
| `IDirectDrawSurface::SetColorKey` | Magenta transparency | `SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(fmt, 255, 0, 255))` |
| `IDirectDrawSurface::GetDC` + `StretchBlt` | GDI-path bitmap copy | `SDL_BlitScaled` |
| `IDirectDrawSurface::GetSurfaceDesc` | Query pixel format | `SDL_QueryTexture` |
| `IDirectDrawClipper::SetHWnd` | Window clipping | `SDL_RenderSetClipRect` |
| `IDirectDrawSurface::Lock` / `Unlock` | Direct pixel access | `SDL_LockTexture` / `SDL_UnlockTexture` |
| `LoadImageA` (HBITMAP) | Load BMP into GDI | `SDL_LoadBMP` |
| `GetStockObject` + `FillRect` | Fill rect with colour | `SDL_SetRenderDrawColor` + `SDL_RenderFillRect` |
| Pixel format globals (RGB555/RGB565) | Packed colour math | Use `SDL_PIXELFORMAT_RGB555` / `SDL_PIXELFORMAT_RGB565`; or always 32-bit ARGB8888 |
| `GetSystemMetrics(SM_CXSCREEN)` | Screen size | `SDL_GetCurrentDisplayMode` |

The pixel format detection block (`g_pixFmtId`, `g_rShift`, `g_gBits`, `g_whitePixel`, `g_rMask/gMask/bMask` at `0x485274–0x485290`) can be eliminated entirely on Linux. SDL2 abstracts pixel formats and the porter should convert all surface blits to work in 32-bit ARGB8888 or let SDL handle format conversions automatically.

Magenta (`R=255, G=0, B=255`) is the universal colour key throughout the engine. Every surface uses it for transparency.

### 2.3 Audio (DirectSound 5)

| Win32 / DirectSound API | Purpose | Linux Replacement |
|---|---|---|
| `DirectSoundCreate` (DSOUND.DLL Ordinal 1) | Create DS device | `Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096)` |
| `IDirectSoundBuffer::Play` | Start playback | `Mix_PlayChannel(-1, chunk, 0)` |
| `IDirectSoundBuffer::Stop` | Stop playback | `Mix_HaltChannel(channel)` |
| `IDirectSoundBuffer::Release` | Free buffer | `Mix_FreeChunk(chunk)` |
| `IDirectSoundBuffer::SetVolume` | Set volume | `Mix_Volume(channel, vol)` |
| `IDirectSoundBuffer::SetPan` | Stereo pan | `Mix_SetPanning(channel, left, right)` |
| Looping `IDirectSoundBuffer::Play(DSBPLAY_LOOPING)` | Loop ambient sound | `Mix_PlayChannel(-1, chunk, -1)` |
| `PlaySoundA(SND_FILENAME\|SND_ASYNC)` | Background music.wav | `Mix_LoadMUS(path)` + `Mix_PlayMusic(music, -1)` |
| `PlaySoundA(NULL)` | Stop music | `Mix_HaltMusic()` |
| 3D panning / positional audio | `CSoundEffect` pan/volume | `Mix_SetPanning` computed from tile position |

For higher-quality positional audio (the `CSoundEffect` object has `panLeft`/`panRight`, `posX`/`posY`, and layer indices), OpenAL is a superior replacement. SDL_mixer is sufficient for the simple mono/stereo mix the game actually performs.

Audio state machine (`CAudioStateMachine_SetState` at `0x004208f0`) transitions:

- State 7: play `music.wav` in background — `Mix_LoadMUS` + `Mix_PlayMusic(-1)`
- State 1: stop music — `Mix_HaltMusic`
- State 3: play FMV audio track (video player) — GStreamer or libVLC
- State 0/6: return to menu, stop all sound

### 2.4 Video Playback (MCIWnd / AVI)

The game plays two intro AVI files (`IgSpin.avi`, `legospin.avi`) and uses `music.wav` as background music via the MCIWnd window class (`MSVFW32.DLL`).

| Win32 MCIWnd API | Purpose | Linux Replacement |
|---|---|---|
| `MCIWndRegisterClass` | Register MCIWnd class | GStreamer `gst_init` |
| `CreateWindowExA("MCIWndClass")` | Create video window | `gst_parse_launch("playbin uri=file://...")` |
| `SendMessage(hWnd, MCIWNDM_OPEN, 0, path)` | Open media file | `gst_element_set_state(GST_STATE_READY)` |
| `SendMessage(hWnd, MCIWNDM_PUT_DEST, rect)` | Set display rect | `GstVideoOverlay` / SDL overlay window |
| `SendMessage(hWnd, 0x804 STOP)` | Stop playback | `gst_element_set_state(GST_STATE_NULL)` |
| `mciSendCommandA(MCI_SETAUDIO)` | Set audio volume | `gst_stream_volume_set_volume` |
| `SendMessage(hWnd, WM_CLOSE)` | Destroy video window | `gst_object_unref(pipeline)` |

Alternative: use `libVLC` (`libvlc_media_player_set_xwindow` + `libvlc_media_player_play`), which is simpler to embed than raw GStreamer.

For the port, the simplest approach is to pre-convert the AVI files to a format that SDL2_image or SDL2 can handle, or decode frames using FFmpeg's `libavcodec` directly.

### 2.5 Input

| Win32 API | Purpose | Linux Replacement |
|---|---|---|
| `PeekMessageA(WM_MOUSEMOVE)` | Mouse input | `SDL_PollEvent` / `SDL_MOUSEMOTION` |
| `SetCapture` / `ReleaseCapture` | Mouse capture | `SDL_CaptureMouse(SDL_TRUE/FALSE)` |
| `GetCursorPos` / `SetCursorPos` | Cursor position | `SDL_GetMouseState` / `SDL_WarpMouseInWindow` |
| `ClientToScreen` / `ScreenToClient` | Coord conversion | SDL window-relative coords are already client-space |
| `LoadCursorA(IDC_ARROW)` | System arrow cursor | `SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW)` |
| `LoadCursorFromFileA("busy_ani.ani")` | Busy/animated cursor | `SDL_CreateCursor` from extracted frames |
| `ShowCursor(TRUE/FALSE)` | Hide/show OS cursor | `SDL_ShowCursor(SDL_ENABLE/SDL_DISABLE)` |
| `WM_LBUTTONDOWN/UP`, `WM_RBUTTONDOWN/UP` | Mouse buttons | `SDL_MOUSEBUTTONDOWN/UP` events |

The game uses a custom sprite-based cursor rendered into a 256×256 DirectDraw offscreen surface (`g_cursorDDSurface` at `0x004fd3cc`). The OS cursor is always hidden (`RegisterClassA` sets `hCursor = NULL`). On Linux, call `SDL_ShowCursor(SDL_DISABLE)` at startup and blit the custom cursor sprite each frame using `SDL_RenderCopy`.

### 2.6 Multiplayer (DirectPlay)

| Win32 / DirectPlay API | Purpose | Linux Replacement |
|---|---|---|
| `DirectPlayCreate` | Create DP session | BSD sockets / ENet / SDL_net |
| `IDirectPlay::Open` | Join/host session | `enet_host_connect` |
| `IDirectPlay::Send` | Send packet | `enet_peer_send` |
| `IDirectPlay::Receive` | Receive packet | `enet_host_service` |
| `IDirectPlay::Close` | End session | `enet_peer_disconnect` |
| Service providers (modem, IPX, TCP/IP) | Transport | TCP/IP only (via ENet) |

See Section 8 for protocol details.

### 2.7 System and CRT

| Win32 API | Purpose | Linux Replacement |
|---|---|---|
| `RegOpenKeyExA` / `RegQueryValueExA` | Read `lego.ini` path from registry | Derive from `$LEGO_LOCO_DATA`, XDG dirs, or `./lego.ini` |
| `GetPrivateProfileStringA` / `IntA` | Read INI values | `inih` library (`ini_parse`) |
| `WritePrivateProfileStringA` | Write INI values | `inih` + `fopen`/`fprintf` |
| `GetUserNameA` | Current user's name | `getpwuid(getuid())->pw_name` |
| `GetModuleHandleA` / `LoadStringA` | Load localised strings from EXE | Extract string table to a `.strings` file; `fgets` lookup |
| `GetFileVersionInfoA` / `VerQueryValueA` | Read EXE version | Compile-time `VERSION_STRING` macro |
| `CreateFileA` / `ReadFile` / `WriteFile` | Binary file I/O | `open` / `read` / `write` (POSIX) |
| `CoInitializeEx` / `CoUninitialize` | COM init | Remove entirely |
| `timeSetEvent(28ms)` | Game-loop timer | `timer_create(CLOCK_MONOTONIC)` or `SDL_AddTimer(28, cb, NULL)` |
| `CreateEventA` / `SetEvent` / `ResetEvent` | Thread sync | `sem_t` (POSIX semaphore) |
| `Sleep(ms)` | Delay | `SDL_Delay(ms)` |
| `OutputDebugStringA` | Debug output | `fprintf(stderr, ...)` |
| `FormatMessageA` | Error message | `strerror(errno)` |
| `wsprintfA` | String formatting | `snprintf` |
| `FindWindowA` | Duplicate-instance check | Lock file (`/var/lock/lego-loco.lock`) or skip |
| `GetFileAttributesA` | File existence check | `access(path, F_OK) == 0` |
| `SetWindowLongA(GWL_WNDPROC)` | WndProc subclassing | Remove; SDL has one event loop |

---

## 3. Port Strategy

### Recommended Approach: SDL2 2D with SDL_Renderer

Given that Lego Loco uses **DirectDraw 5 for 2D blitting only** (no 3D geometry, no DirectX transform pipeline), the correct port strategy is:

**SDL2 software-accelerated 2D renderer**

- `SDL_CreateRenderer` with `SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC`
- All surfaces become `SDL_Texture*` objects with `SDL_TEXTUREACCESS_STATIC` (pre-uploaded art) or `SDL_TEXTUREACCESS_TARGET` (render targets / compositing surfaces)
- Blitting is `SDL_RenderCopy` / `SDL_RenderCopyEx`
- No OpenGL/Vulkan needed

This is the lowest-risk approach because:
1. All rendering in the original engine is 2D axis-aligned rectangular blits
2. The isometric tile renderer is a manual blit pipeline, not a 3D engine
3. Colour-key transparency maps directly to `SDL_SetColorKey`
4. There is no palette animation (the game uses RGB555/RGB565, not 8-bit indexed colour)

### Pixel Format Simplification

The original engine detects whether the display is RGB555 or RGB565 at runtime and packs/unpacks 16-bit colours accordingly. On Linux, **use 32-bit ARGB8888 throughout** and let SDL2 handle format conversions:

```c
// On surface creation:
SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
// Magenta key:
SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(surf->format, 255, 0, 255));
// Upload to GPU:
SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
SDL_FreeSurface(surf);
```

### Window Setup

```c
SDL_Window *win = SDL_CreateWindow(
    "LEGO LOCO",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    640, 480,
    SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS);
SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
// Scale to logical resolution:
SDL_RenderSetLogicalSize(ren, 640, 480);
```

`SDL_RenderSetLogicalSize` handles 640×480-to-desktop letterboxing automatically, including 800×600 mode by changing the logical size before creating the renderer.

### Child Windows / Sub-regions

The original engine uses multiple child `HWND` objects (`hWndChild`, `hWndChildRender`, and per-subsystem windows). On Linux **collapse all child windows into SDL viewport regions**:

```c
// Instead of CreateWindowExA for a child, just set:
SDL_RenderSetViewport(renderer, &childRect);
// Render the child's content...
SDL_RenderSetViewport(renderer, NULL);  // restore full view
```

This eliminates the `RegisterClassA` / `CWnd_CreateChildWindow` pattern entirely.

---

## 4. CMake Build System

### Directory Layout

```
lego-loco-port/
├── CMakeLists.txt
├── src/
│   ├── main.c             ← replaces WinMain + entry
│   ├── platform/
│   │   ├── sdl_window.c   ← DD_Init / DD_Shutdown → SDL2
│   │   ├── sdl_audio.c    ← DS_Init / DS_Shutdown → SDL_mixer
│   │   ├── sdl_input.c    ← mouse/keyboard → SDL events
│   │   ├── sdl_video.c    ← MCIWnd → GStreamer / libVLC
│   │   └── ini_config.c   ← INI parsing → inih
│   ├── core/
│   │   ├── core.c         ← game loop, CGWND, state machine
│   │   └── core.h
│   ├── graphics/
│   │   ├── graphics.c     ← rendering subsystem
│   │   ├── graphics.h
│   │   ├── ddraw_init.c   ← DD init wrapper
│   │   └── ddraw_init.h
│   ├── audio/
│   │   ├── audio.c
│   │   └── audio.h
│   ├── network/
│   │   ├── network.c
│   │   └── network.h
│   ├── resources/
│   │   ├── resources.c    ← RFH/RFD loader, cache
│   │   └── resources.h
│   ├── game/
│   │   ├── world.c        ← tile map, objects
│   │   ├── save.c         ← .sav / .usr formats
│   │   └── ui.c           ← cursor, colour picker
│   └── ui/
│       └── ui.c
├── data/                  ← game data files (art-res/, Exe/, etc.)
└── third_party/
    ├── inih/              ← INI parser
    └── enet/              ← network (optional)
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(lego-loco-port C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# Find SDL2 and companions
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)

# Optional: GStreamer for AVI video
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST gstreamer-1.0 gstreamer-video-1.0)

# Optional: ENet for multiplayer
find_library(ENET_LIB enet)

# inih (bundled)
add_library(inih STATIC third_party/inih/ini.c)
target_include_directories(inih PUBLIC third_party/inih)

# Main executable
add_executable(lego-loco
    src/main.c
    src/core/core.c
    src/platform/sdl_window.c
    src/platform/sdl_audio.c
    src/platform/sdl_input.c
    src/platform/sdl_video.c
    src/platform/ini_config.c
    src/graphics/graphics.c
    src/graphics/ddraw_init.c
    src/audio/audio.c
    src/network/network.c
    src/resources/resources.c
    src/game/world.c
    src/game/save.c
    src/ui/ui.c
)

target_compile_definitions(lego-loco PRIVATE LOCO_LINUX=1)

target_include_directories(lego-loco PRIVATE
    src
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_libraries(lego-loco PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    inih
    m
)

if(GST_FOUND)
    target_compile_definitions(lego-loco PRIVATE LOCO_USE_GSTREAMER=1)
    target_include_directories(lego-loco PRIVATE ${GST_INCLUDE_DIRS})
    target_link_libraries(lego-loco PRIVATE ${GST_LIBRARIES})
endif()

if(ENET_LIB)
    target_compile_definitions(lego-loco PRIVATE LOCO_USE_ENET=1)
    target_link_libraries(lego-loco PRIVATE ${ENET_LIB})
endif()

# Install
install(TARGETS lego-loco RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/ DESTINATION share/lego-loco)
```

### Configuration Discovery

On Linux, `CGWND_LoadConfig` should search for `lego.ini` in this priority order:

1. `$LEGO_LOCO_DATA/lego.ini` (environment variable)
2. `~/.config/lego-loco/lego.ini` (XDG config)
3. `/usr/share/lego-loco/lego.ini` (system-wide install)
4. `./data/Exe/LEGO.INI` (development tree)

The `[DIRECTORIES]` keys `Res=` and `ResFile=` in `lego.ini` use Windows drive-letter paths. On Linux, strip the drive prefix and remap separators:

```c
// "d:\loco\art-res"  →  data_root + "/art-res"
// "d:\loco\art-res\resource.rfh"  →  data_root + "/art-res/resource.rfh"
```

---

## 5. Major Porting Challenges and Solutions

### 5.1 C++ Calling Conventions (thiscall / fastcall)

The original binary uses three calling conventions:

| Convention | Usage | Linux C equivalent |
|---|---|---|
| `__cdecl` | Global C functions | Standard C — no change |
| `__thiscall` | C++ member functions (ECX = `this`) | First parameter becomes explicit `void *this` |
| `__fastcall` | Utility functions (ECX=arg0, EDX=arg1) | Standard C with explicit parameters |

When rewriting from decompiled Ghidra output, convert all `__thiscall` functions to plain C functions with an explicit `self` or `this_` first argument:

```c
// Original MSVC: void __thiscall RESMGR_Init(CResourceMgr *this);
// Linux:         int  RESMGR_Init(CResourceMgr *self);
```

### 5.2 COM and vtable Dispatch

The engine uses COM-style vtables for DirectDraw/DirectSound interfaces. On Linux these disappear because SDL2 provides concrete functions. However, the engine's own C++ objects (CGWND, CResourceMgr, etc.) also use vtables and those remain valid C++ objects after the port.

Key COM interfaces and their lifetimes:
- `IDirectDraw` → removed; `SDL_Renderer*` singleton replaces it
- `IDirectDrawSurface` → replaced by `SDL_Texture*` (GPU) or `SDL_Surface*` (CPU)
- `IDirectSound` → removed; `Mix_*` state replaces it
- `IDirectSoundBuffer` → replaced by `Mix_Chunk*` (per-sound) or `Mix_Music*` (background)

### 5.3 Registry-Based Configuration

`CGWND_LoadConfig` uses `RegOpenKeyExA(HKLM, "SOFTWARE\\Intelligent Games\\LEGO LOCO")` to find the install path. On Linux replace with:

```c
static const char *find_data_root(void) {
    const char *env = getenv("LEGO_LOCO_DATA");
    if (env) return env;
    // XDG
    const char *xdg = getenv("XDG_DATA_HOME");
    // ...fallback to ./data
    return "./data";
}
```

### 5.4 String Table (Localisation)

The game loads localised strings from the EXE's `VS_VERSIONINFO` / `RT_STRING` resource table via `LoadStringA(GetModuleHandleA(NULL), id, buf, len)`.

The string table must be extracted from `loco.exe` (using `wrestool -x` from the `icoutils` package or Ghidra's export) and stored as a flat text file or binary blob:

```
# strings.en (example format)
100=Press any key to continue
101=Loading...
200=Save game
```

`RESMGR_LoadLocalizedString` at `0x00447330` applies per-language ID offsets for the four supported languages. The language is selected by `g_ResMgr.languageID` (at `+0x241B8` in `CResourceMgr`):

| languageID | Language |
|------------|----------|
| 0 | English (default) |
| 1–9 | Locale-specific offsets into string table |

Known locale directories in the install: `Dan` (Danish), `Dut` (Dutch), `Spa` (Spanish), `Fre` (French), `Ger` (German), `Ita` (Italian), `Nor` (Norwegian), `Swe` (Swedish).

### 5.5 Child Window Architecture

The engine creates multiple `HWND` child windows, each with its own `WNDPROC`, to create distinct rendering areas (main world, mini-map, UI panels). On Linux:

- Replace each child window with an `SDL_Rect` viewport
- Replace `WM_*` messages with game-internal events
- Replace `SetWindowLongA(GWL_WNDPROC)` subclassing with a direct function pointer

The colour picker sub-window (`ColorPickerSubWnd_WndProc` at `0x00419a60`) handles:
- `WM_CTLCOLOREDIT` — set text colour on edit boxes → SDL_ttf draw
- Custom `0x5F5` — save and close → game callback
- Custom `0x5F6` — re-enable parent → game callback

### 5.6 Multimedia Timer Replacement

`timeSetEvent(28, 0, cb, 0, TIME_PERIODIC)` fires at ~35.7 fps. On Linux:

**Option A — SDL timer (simplest):**
```c
Uint32 timer_callback(Uint32 interval, void *param) {
    SDL_AtomicSet(&g_timer_fired, 1);
    return 28;  // reschedule
}
g_timer_id = SDL_AddTimer(28, timer_callback, NULL);
```

**Option B — POSIX interval timer (most accurate):**
```c
struct sigevent sev = { .sigev_notify = SIGEV_THREAD, .sigev_notify_function = timer_cb };
timer_create(CLOCK_MONOTONIC, &sev, &g_timer_id);
struct itimerspec its = { .it_interval = {0, 28000000}, .it_value = {0, 28000000} };
timer_settime(g_timer_id, 0, &its, NULL);
```

**Option C — Frame-limited game loop (cleanest):**
Remove the timer entirely and cap the main loop using `SDL_GetTicks`:
```c
Uint32 next_frame = SDL_GetTicks();
while (running) {
    Uint32 now = SDL_GetTicks();
    if (now >= next_frame) {
        next_frame += 28;
        GameFrame_Update();
    }
    SDL_PollEvent(&ev);
    // ...
}
```

### 5.7 Screensaver

`LEGO LOCO.scr` is a Windows screensaver using the same engine with a restricted subset. On Linux the screensaver function can be:
- Skipped entirely (no `.scr` format on Linux)
- Repackaged as an XScreenSaver hack
- Or triggered via `xscreensaver-command -activate`

The `SCREENSAVER_Init` / `SCREENSAVER_Cleanup` / `SCREENSAVER_PostQuit` functions are isolated and can be stubbed out.

### 5.8 Duplicate Instance Check

The original `WinMain` calls `FindWindowA("LEGO LOCO", NULL)` to detect a running instance and shows a warning if found. On Linux, use a lock file:

```c
int lock_fd = open("/tmp/lego-loco.lock", O_CREAT | O_RDWR, 0666);
if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
    fprintf(stderr, "Lego Loco is already running.\n");
    return 1;
}
```

### 5.9 Font Rendering

`CResourceMgr` holds five GDI fonts (`CreateFontA("Arial", ...)`) at offsets `+0x0004` to `+0x0014`:
- Arial 12pt, 14pt, 16pt, 24pt, 20pt

On Linux, replace with SDL_ttf:
```c
TTF_Font *fonts[5];
fonts[0] = TTF_OpenFont("LiberationSans.ttf", 12);
fonts[1] = TTF_OpenFont("LiberationSans.ttf", 14);
// etc.
```

Liberation Sans is metric-compatible with Arial and is freely available. Alternatively, ship a bundled font.

---

## 6. Data File Formats: .RFH / .RFD

### Overview

The game's art assets are packed into two companion files:

- `art-res/resource.RFH` — index file, 85,647 bytes
- `art-res/resource.RFD` — data file, 57,305,835 bytes (~55 MB)

The resource system is managed by `CRFHFile` (embedded in `CResourceMgr` at `+0x0018`) and loaded by `RFHMGR_Load` (`0x0045caa0`).

### .RFH On-Disk Format

The RFH file is a sequence of variable-length binary records with no global file header. Each record describes one asset stored in the paired .RFD file:

```
RFH Record (variable length):
  [0..3]   uint32_le  name_len      Length of the filename string INCLUDING
                                    the null terminator
  [4..N-1] char[]     filename      Null-terminated relative path using
                                    backslash separators
                                    (e.g. "roads\half-vwint.dat\0")
  [N..N+3] uint32_le  rfd_size      Size of the asset data in the .RFD file
  [N+4..N+7] uint32_le flags        0x00 = normal asset
                                    0x01 = packed/special asset (e.g. .but, .ini)
```

RFD byte offsets are NOT stored in the RFH. The loader (`RFHMGR_Load`) computes them by accumulating `rfd_size` values sequentially as it parses the RFH. The in-memory `RFHEntry.rfdOffset` field is computed during load.

The RFH ends at EOF; there is no record count or terminator.

### .RFD On-Disk Format

The .RFD file is a flat binary concatenation of all asset blobs in the same order as the RFH records. No padding or alignment is inserted between assets. The offset and size from the corresponding RFH entry are used to `fseek` and `fread` the blob directly.

Asset types found in the archive (by extension):

| Extension | Count | Type | Description |
|---|---|---|---|
| `.bmp` | 1338 | Windows BMP | Sprite sheets, UI graphics (16-bit RGB555/RGB565) |
| `.dat` | 627  | Text tile descriptor | Tile/object definitions — rich text format (see below) |
| `.wav` | 355  | WAV audio | Sound effects (accessed by path, not always archived) |
| `.but` | 173  | Binary button descriptor | Button IDs → sound files + sprite IDs |
| `.sav` | 17   | Binary world save | Scenario/player save files |
| `.lay` | 6    | Text multi-area layout | Multi-save world grid definitions (LAYOUTS/ dir) |
| `.ani` | 3    | RIFF ACON cursor  | Windows animated cursor files; 2 of 3 are Huffman-compressed (flags=0x1) |
| `.but` | 173  | Huffman-compressed BMP | Toolbar button sprites: 156×53 pixels, 8bpp indexed, custom Huffman |
| `.ini` | 1    | INI text | Per-asset config (ee.ini = Easter Egg INI) |

### .dat Tile Descriptor Format (Verified — Text Format)

Every tile in the game (roads, buildings, vehicles, scenery, minifigs) has a `.dat` file
describing its behavior. These are **plain ASCII text files** — easy to parse on Linux.

```
# Example: roads\half-vwint.dat (a vertical+horizontal road intersection)

physical_occupancy           # 3D footprint on the tile grid
  2 1 1                      # cols rows layers
  1 1                        # layer 0: row 0 occupancy (1=blocked, 0=free)
                             # (multiple layers for tall buildings)

bitmap_occupancy             # how many tile cells the sprite visually covers
  2 1                        # cols rows
  1 1                        # grid of band indices (which sprite "row" covers each cell)

entry_exit N E S W           # track/road connection pixel offsets per cardinal side
                             # e.g. "0 2 0 2" = connected N and S
                             # For buildings/vehicles: "88 48 88 0" = pixel position

RMBSeq <id>                  # right-mouse-button animation sequence ID (-1 = none)
                             # plays when player right-clicks this tile

LeisureDestination <0|1>     # 1 = minifigs can walk here for leisure

FreeToRoam <x1> <y1> <x2> <y2>  # pixel bounds for minifig roaming on this tile

MaxEmployees <n>             # max workers at this building (0 = no workers)
PossibleEmployees <id> ...   # minifig type IDs that can work here (-1 = any)

MaxMinifigForResource <n>    # max visitors simultaneously
PossibleMinifigs <id> ...    # visitor minifig type IDs (-1 = any)

Shifts <sh1> <sh2> <sh3> <sh4>  # pixel rendering offsets for isometric display
                                 # "09 30 17 30" = NW-offset, NW-shift, SE-offset, SE-shift

ButtonVisible <0|1>          # 1 = appears in build-mode toolbar

closedfs <n>                 # frame set index to display when building is "closed"
                             # (night-time / out-of-hours state)

InsertSeq <group> <id>       # sound/animation to play when tile is first placed
EasterEgg <seq> <n>  <seq> <count> <delay>  <seq> <n> R <x> <y>
                             # Easter Egg trigger: sequence, minifig type, repeat, coords

MobileSeq <group> <id>       # animation for moving entities on this tile
EasterEgg ...                # (same format as InsertSeq Easter Egg)

-9                           # section separator sentinel

button offset <x> <y> [<z>] # sprite position in the UI toolbar panel

Hotspot <x> <y>             # click detection point in pixels

total_number_of_frames <n>  # sprite animation frame count
number_of_frame_sets <n>    # how many named animation states

cursor_frame_set <idx> <n>  # frame set to activate when cursor hovers

# Animation frame set table (one row per set):
# <state_name>  <set_idx> <first_frame> <speed> <???> <delay_ms> <sound_id> <???> <loop> <???> <extra>
cursor     0  0    0    0   -1      0  0  0  0
noisy1     0  0    0  150   2   21915 -1  1  0

# For vehicles (16-direction sets):
W      0  0  1  0  0  -1  0  0  0  0
WSW    1  1  1  0  0  -1  0  0  0  0
SW     2  2  1  0  0  -1  0  0  0  0
# ... (16 compass headings)
```

Key `.dat` parsing rules:
- Lines starting with `//` are comments (ignored)
- `-9` acts as a section separator / end-of-section sentinel
- All whitespace (spaces, tabs) between tokens is ignored
- Files are ASCII; newlines may be `\r\n` (Windows CRLF)
- Fields not present default to sensible values (e.g. RMBSeq=-1, MaxEmployees=0)

**Linux port strategy**: Parse `.dat` files with a simple line-oriented tokenizer.
No binary parsing needed. `getline` + `strtok` or equivalent is sufficient.
A complete `CTileDesc` struct can be populated from the `.dat` text in one pass.

### In-Memory Structures

After loading, the RFH index is a linked list of heap-allocated `RFHEntry` nodes:

```c
/* Verified from RFHMGR_Load decompile at 0x0045caa0 */
struct RFHEntry {          // 0x10 bytes each (heap-allocated per entry)
  char     *filename;      // +0x00  heap copy of relative path (backslash-separated)
  uint32_t  flags;         // +0x04  0x00 = normal asset; 0x01 = packed/special (e.g. .but, .ani)
  uint32_t  rfdSize;       // +0x08  byte size of this asset in the .RFD file
  RFHEntry *next;          // +0x0C  NULL = end of linked list
};
/* RFD offset is NOT stored in RFHEntry. The loader reads the RFH sequentially
 * until FILE._flag & _IOEOF is set. The RFD offset is tracked by a separate
 * fseek cursor maintained by FUN_00464490 (stream positioned reader).
 * For the Linux port, compute RFD offsets by accumulating rfdSize during load. */
```

Asset lookup (`FUN_0045cd00`) performs a **linear search** through this list by case-insensitive filename comparison (`_stricmp` on Win32; use `strcasecmp` on Linux). When found, `FUN_00464490` seeks the RFD file to the accumulated offset and returns a seekable position for reading.

**Flags handling (Verified)**: When `flags == 0x01`, the asset is Huffman-compressed (see `FUN_0045c830` at 0x45c830). All 173 `.but` files and 2 of 3 `.ani` files use this encoding.

```
Huffman-packed format layout (reverse-engineered from FUN_0045c830):
  [0x000]  uint32_le  uncompressed_size    byte count of decompressed output
  [0x004]  uint32_le  tree_root            starting node index for Huffman traversal
  [0x008]  uint16[]   tree_table           Huffman tree nodes (2040 bytes = 1020 entries)
  [0x800]  uint32[]   bit_stream           payload (32-bit words, LSB-first bit order)

Decompression algorithm:
  1. node = tree_root
  2. While node > 0xFF:  read one bit; node = table[(node*2 + bit)*2]
  3. Output byte = node & 0xFF
  4. Repeat uncompressed_size times

Decompressed content:
  .but → standard Windows BMP (8bpp indexed, 156×53 pixels for road buttons)
  .ani → standard Windows RIFF ACON animated cursor (16×16 frames)

Implementation: tools/unpack_but.py (verified, decompresses all 173 .but + 2 .ani correctly).
Linux port: decompress at load time; use SDL_LoadBMP_RW for .but, libxcursor or manual ACON parse for .ani.
```

### Linux Port Notes for Resource System

- `CRFHFile.rfdHandle` is already `FILE*` (CRT `fopen`). No change needed.
- `RFHMGR_Load` uses `fopen` / `fread` / `fclose` — portable as-is.
- Path separator conversion: replace `\\` with `/` when building filename lookup keys on Linux.
- The `DAT_004a99c8` game data directory prefix is stripped from filenames during archive lookups. Reproduce this with `strncasecmp` or by normalising paths on load.
- `BITMAP_LoadFromArchiveOrFile` first tries the RFD archive; if not found, falls back to a direct filesystem path. The fallback uses `GetFileAttributesA` (replace with `access(path, F_OK)`).

---

## 7. Save File and Profile Formats

### World Save Files (.sav)

Located in `art-res/SAVEGAME/`. Pre-built scenario saves ship with the game (e.g. `Wildwest.sav`). Player saves are written to the same directory.

File sizes vary by world complexity (all 8 shipped saves verified):
  Wildwest.sav = 67,008 bytes  (smallest, arid/western scenario)
  4BRIDGES.SAV = 74,816 bytes
  COW-VILL.SAV = 238,232 bytes (largest)

The file is a raw binary blob with this confirmed structure:

```
.sav Binary Layout (verified from all 8 shipped save files):

=== File Header ===
  [0x00]  uint16_le  flags_0      0x0008 (same in all saves)
  [0x02]  uint16_le  flags_1      0x0040 (same in all saves; NOT 0x4000)
  [0x04]  uint32_le  version      0x00000030 = 48 (same in all saves)
  [0x08]  uint32_le  tile_count   entity record count; e.g. Wildwest=497
  [0x0C]  uint16_le  has_tag      0x0001 if scenario_id is set, 0x0000 otherwise
  [0x0E]  char[6]    scenario_id  null-terminated 5-char theme tag:
                                    "GREEN\0" = 4BRIDGES, COW-VILL, GREENVIL
                                    "ARRID\0" = Wildwest (arid/desert)
                                    ""        = BUSYTOWN, MOONBASE, SNOWBALL
  [0x14]  uint8[]    terrain_map  tile-type grid; values 0x00-0x07 per cell
                                  0x05=track, 0x06=road, 0x07=border, 0x03=grass
                                  encodes the isometric map layout

=== Entity Records (start at 0x0D24, stride 128 bytes) ===
  Exactly (file_size - 0x0D24) / 128 records.

  Per-record layout (128 = 0x80 bytes):
    [+0x00]  char[32]   entity_type  e.g. "Building\0" (null-padded to 32)
    [+0x20]  int16_le   grid_x       isometric grid column
    [+0x22]  int16_le   grid_y       isometric grid row
    [+0x24]  uint16_le  resource_id  maps to .dat/.but asset in resource.RFD
    [+0x26]  uint16_le  orientation  rotation 0-3 (N/E/S/W)
    [+0x28]  uint8[72]  state_data   entity-specific serialized state
                                     (animation frame, occupants, etc.)
    [+0x70]  uint32_le  runtime_ptr  internal linked-list pointer; zero on disk

  Confirmed entity type strings:
    "Building"   — placed building tile (529 records in 4BRIDGES)
    "Vehicle"    — train, car, bus
    "Minifig"    — pedestrian NPC
    Easter egg names: "6Luis", "6Ruggero", "6Mark", "6Dan", "6Simon"
    ("6" prefix = developer names from Intelligent Games hidden in Wildwest)
```

Full entity-record field definitions require reverse engineering of
`CGameWorld_SaveEntity` / `CGameWorld_LoadEntity` in the 0x00452E10 area.
The `CGameWorld_Reset` function initializes `0x14910` uint32 tile slots.

### Multi-Area World Layouts (.LAY — Verified)

Located at `LAYOUTS/` in the RFH archive (6 files). These are **plain ASCII text**
files that define how multiple `.sav` area files are stitched together into a larger
world grid. The player can expand their town across adjacent areas.

```
.LAY format (from LAYOUTS/3 X 3.LAY):

Line 1: <total_area_count>        e.g. 9
Line 2: <cols>                    e.g. 3  (grid width in areas)
Line 3: <rows>                    e.g. 3  (grid height in areas)
Line 4+: <area_filename.sav>      one per area, in row-major order

Example — LAYOUTS/3 X 3.LAY (9 areas, 3×3 grid):
  9
  3
  3
  area1.sav   ← top-left
  area2.sav   ← top-center
  area3.sav   ← top-right
  area4.sav
  area5.sav
  area6.sav
  area7.sav   ← bottom-left
  area8.sav
  area9.sav   ← bottom-right
```

Available layout configurations: 2×1, 2×2, 3×1, 3×2, 3×3 (defined in INDEX.LAY).
`INDEX.LAY` simply lists the layout names, one per line, as available options.

**Linux port**: Parse as plain text; no binary format involved. The world grid is
assembled by loading each area `.sav` in its grid position and joining them on the
shared tile boundaries.

### User Profile Files (.usr)

Located in `art-res/POSTBAG/<theme>/<lang>/`. One profile per username.

Structure from code (`CUserProfile` at `FUN_00452e10`):

```c
struct CUserProfile {  // 0x124 bytes on-disk (0x120 payload + isNewUser flag)
  void*    vtable;          // +0x00  (not written to disk)
  uint16_t magic;           // +0x04  must == 0x0066 ('f') for valid profile
  char     username[12];    // +0x06  null-terminated player name
  uint8_t  pad[6];          // +0x12
  uint32_t reserved;        // +0x14
  uint32_t clientId;        // +0x18  slot number 1..999 (from INI [CLIENT]/NextId)
  uint32_t saveCounter;     // +0x1C  screenshot counter, wraps at 9999
  char     screenshotName[32]; // +0x20  formatted "%03d_%04d"
  // ... remainder unknown up to 0x120 bytes total
  uint8_t  isNewUser;       // +0x120  1 = brand-new account (not written to disk)
};
```

The `.usr` file stores exactly `0x120` bytes (the `isNewUser` flag at `+0x120` is runtime-only, not persisted). Bad-magic files are recovered by re-reading `[CLIENT]/NextId` from `lego.ini` and overwriting.

The POSTBAG `.usr` files (`easter.usr`) appear to be structured differently — they contain plain ASCII text (character names, one per line, CRLF-terminated) and are not binary profiles. These are likely character/minifig name lists shipped as game data.

### INI Configuration (lego.ini)

The main configuration file uses standard Windows INI format:

```ini
[DIRECTORIES]
Res=d:\loco\art-res          ; game data root (remap to Linux path)
ResFile=d:\loco\art-res\resource.rfh  ; primary RFH index file
exe=loco.exe

[Video]
Dir=d:\loco\Art-res\Video\locointr.avi  ; intro AVI (optional)

[MOUSE]
Setting1=PlayerName          ; current username

[CLIENT]
NextId=1                     ; next user slot number

[BALANCING]
MinVehicleFPS=20
MinBuildingFPS=18
MinMinifigFPS=16
MinFlyingFPS=14

[WINDOW_ATTRIBUTES]
; window geometry (written on exit)

[PROCESS]
CleanExit=1                  ; 0 = previous crash detected
```

On Linux, store `lego.ini` in `~/.config/lego-loco/lego.ini`. Use `inih` (https://github.com/benhoyt/inih) as a drop-in for `GetPrivateProfileStringA` / `WritePrivateProfileStringA`.

---

## 8. Network Protocol

### DirectPlay Architecture

The multiplayer subsystem (`CNetworkManager`, `g_pNetworkMgr`, 0x2C4 bytes) wraps DirectPlay. The original game supports the "LEGO International Train Server" over the DirectPlay TCP/IP service provider, accommodating up to 9 simultaneous players sharing a train layout.

DirectPlay sits between the game and the transport. On Linux, DirectPlay is replaced with BSD sockets using a custom or off-the-shelf reliable UDP library.

### Protocol Analysis

DirectPlay uses a session-based model. Observed from the `NETMAN` class:
- Players connect to a named session ("LEGO LOCO" or "LEGO International Train Server")
- The host player controls the train schedule
- Train sharing involves sending tile placement deltas and train position updates

The wire protocol is not fully reverse-engineered. Key observations:

1. **Session discovery**: DirectPlay used UDP broadcast on the LAN or a lobby server. Replace with ENet peer discovery or a simple TCP server at a fixed address.
2. **Packet format**: DirectPlay wraps application data in `DPMSG_*` structs. The actual game payload is the `NETMAN` message defined inside the game. This payload must be extracted from Ghidra analysis of `NETMAN_Send` / `NETMAN_Receive`.
3. **Player count**: Hardcoded limit of 9 players (one host + 8 clients).
4. **Data exchanged**: Tile placement events (tile ID + coordinates), train ownership transfers, player username strings.

### Recommended Network Replacement

**ENet** (http://enet.bespin.org) is the recommended replacement:
- Reliable UDP — matches DirectPlay's TCP/IP provider semantics
- Built-in peer management (up to 32 peers; 9 more than sufficient)
- MIT licensed, C library, Linux-native
- Simple channel model maps to DirectPlay's guaranteed/unreliable delivery modes

Minimal adaptation layer:

```c
// Host:
ENetHost *server = enet_host_create(&addr, 9, 2, 0, 0);
// Client:
ENetHost *client = enet_host_create(NULL, 1, 2, 0, 0);
ENetPeer *peer   = enet_host_connect(client, &serverAddr, 2, 0);
// Send:
ENetPacket *pkt = enet_packet_create(data, len, ENET_PACKET_FLAG_RELIABLE);
enet_peer_send(peer, 0, pkt);
// Receive:
ENetEvent ev;
while (enet_host_service(host, &ev, 0) > 0) { ... }
```

For the "International Train Server" LAN feature, use `enet_address_set_host(&addr, "255.255.255.255")` for broadcast discovery, matching the original DirectPlay lobby behaviour.

---

## 9. Step-by-Step Port Roadmap

The port is structured as eight sequential milestones. Each milestone produces a runnable binary (stubs or full) so progress can be validated continuously.

### Milestone 1 — Skeleton Build (Week 1–2)

**Goal**: A Linux binary that opens a window and exits cleanly.

Tasks:
1. Set up CMake project with `LOCO_LINUX` define
2. Write `src/main.c` with `main()` calling `SDL_Init` / `SDL_CreateWindow` / `SDL_Quit`
3. Port `core.h` type stubs — compile all headers with `LOCO_LINUX`
4. Replace `WinMain` with `main(int argc, char **argv)` calling `CGWND_ParseCommandLine`
5. Stub out `CGWND_LoadConfig` to search for `lego.ini` in local paths
6. Stub all subsystem `Init` functions to return `1` (success)
7. Implement the game loop: `SDL_AddTimer(28, ...)` + `SDL_PollEvent`

Deliverable: Binary opens a black SDL2 window at 640×480 and enters the game loop.

### Milestone 2 — Configuration and Resource Loading (Week 3–4)

**Goal**: Parse `lego.ini` and load the RFH/RFD archive index.

Tasks:
1. Integrate `inih` — implement `INI_GetString` / `INI_GetInt` wrappers
2. Path mapper: convert Windows `d:\loco\...` paths to Linux equivalents
3. Port `RFHMGR_Load` — already uses CRT `fopen`/`fread`, change path separator only
4. Port `RESMGR_Init` — replace `CreateFontA` → `TTF_OpenFont`, `LoadStringA` → stub
5. Extract EXE string table to `strings.en.txt` using `wrestool -x loco.exe -t 6 -o strings/`
6. Implement `RESMGR_LoadLocalizedString` reading from the extracted file

Deliverable: Archive index loads successfully; `RESMGR_GetResource` can resolve filenames.

### Milestone 3 — Graphics Backend (Week 5–7)

**Goal**: Display the splash screen and main menu background.

Tasks:
1. Implement `DD_Init` → `SDL_CreateWindow` + `SDL_CreateRenderer`
2. Implement `DD_LoadBitmap` → `SDL_LoadBMP` + `SDL_SetColorKey` + `SDL_CreateTextureFromSurface`
3. Implement `DD_BlitToScreen` → `SDL_RenderCopy` + `SDL_RenderPresent`
4. Implement `DD_ShowSplashScreen` — clear to black, load `loadfig1.bmp`, centre and blit
5. Port `BITMAP_LoadFromArchiveOrFile` — read blob from RFD, decode BMP from memory buffer using `SDL_RWFromMem`
6. Port `BITMAP_Init` surface constructor
7. Implement `LOCOBITMAP` wrapper as `SDL_Texture*` + metadata struct
8. Port the cursor sprite system — disable OS cursor, blit sprite each frame

Deliverable: Splash screen appears. Main menu background image renders.

### Milestone 4 — Audio Backend (Week 8–9)

**Goal**: Sound effects and background music play.

Tasks:
1. Implement `DS_Init` → `Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096)` + `Mix_AllocateChannels(32)`
2. Port `SOUND_Load` → `Mix_LoadWAV` (from RFD blob via `SDL_RWFromMem`)
3. Port `SOUND_Release` → `Mix_HaltChannel` + `Mix_FreeChunk`
4. Port `RESMGR_PlayBgMusic` → `Mix_PlayChannel(-1, chunk, -1)`
5. Implement `PlaySoundA` stub → `Mix_LoadMUS` + `Mix_PlayMusic` for `music.wav`
6. Port volume persistence via INI config
7. Port `CAudioStateMachine_SetState` state transitions

Deliverable: Button click sounds, ambient sound effects, and background music play correctly.

### Milestone 5 — Input and UI (Week 10–11)

**Goal**: Mouse input drives the game cursor; main menu buttons respond.

Tasks:
1. Port `MouseInput_ProcessMove` — replace `GetCursorPos`/`ScreenToClient` with `SDL_GetMouseState`
2. Port `CursorManager_SetMode` — `SDL_ShowCursor(SDL_DISABLE)` + sprite blit
3. Port `MouseInput_Tick` animation loop
4. Port `CursorSprite_SetType` resource lookup
5. Port `DragMode_Enter` — `SDL_CaptureMouse(SDL_TRUE)`
6. Port `ColorBars_Render` — `SDL_SetRenderDrawColor` + `SDL_RenderFillRect`
7. Port `ColorSwatch_HitTest` — `SDL_PointInRect`
8. Port all `WM_LBUTTONDOWN`/`WM_LBUTTONUP` handlers to `SDL_MOUSEBUTTONDOWN`/`UP`

Deliverable: Cursor sprite animates; clicking main menu buttons triggers responses.

### Milestone 6 — Game World and Tile System (Week 12–15)

**Goal**: The isometric town map renders and tile placement works.

Tasks:
1. Port `CGameWorld_Construct` / `CGameWorld_Reset` — tile grid allocation unchanged
2. Port `CGameWorld_SetResolution` — replace `GetSystemMetrics` with `SDL_GetCurrentDisplayMode`
3. Port `CTileMap_PlaceItem` / `CTileMap_CheckPlacement` — pure game logic, no Win32
4. Port the isometric renderer (tile blit pipeline) — `SDL_RenderCopy` with source rects
5. Port `RESMGR_DrawClock` — `SDL_Rect` arithmetic replacing `SetRect`/`OffsetRect`
6. Port `InvalidateRect` / `UpdateWindow` calls → dirty-flag + SDL repaint queue

Deliverable: Town map renders; tiles can be placed and removed; clock animates.

### Milestone 7 — Video Playback and State Machine (Week 16–17)

**Goal**: Intro AVI files play; all game states transition correctly.

Tasks:
1. Implement `CMciVideoPlayer_PlayInWindow` → GStreamer `playbin` pipeline
   - Target: `SDL_Window` overlay via `GstVideoOverlay` or separate SDL window
   - Alternative: pre-convert AVI to webm/mp4 and use SDL2's built-in decoder
2. Implement `CMciVideoPlayer_Stop` → `gst_element_set_state(NULL)`
3. Port `CAudioStateMachine_SetState` states 3/4/5 (video with audio)
4. Test all `GAME_STATE_*` transitions end-to-end
5. Port `CGWND_Shutdown` — replace `timeKillEvent` → `SDL_RemoveTimer`, `CloseHandle` → `sem_destroy`

Deliverable: Intro video plays on startup; all game-state transitions work.

### Milestone 8 — Network and Save System (Week 18–20)

**Goal**: Save/load works; LAN multiplayer connects between two Linux machines.

Tasks:
1. Port `CUserProfile_LoadFromFile` / `_SaveToFile` — replace `CreateFileA`/`ReadFile`/`WriteFile` → POSIX `open`/`read`/`write`
2. Port `CUserProfile_Construct` — replace `GetUserNameA` → `getpwuid(getuid())->pw_name`
3. Port save-game serialisation for `.sav` files (world tile data)
4. Integrate ENet: implement `CNetworkManager` wrapper over ENet sessions
5. Port session discovery: ENet UDP broadcast replacing DirectPlay lobby
6. Test: two instances on a LAN can share a train layout
7. Port `CGWND_LoadConfig` registry access → lock file + XDG paths

Deliverable: Save/load works. Two machines on LAN can play together.

---

### Quick Reference: Key Addresses

| Address | Symbol | Description |
|---|---|---|
| `0x004689e0` | `entry` | CRT startup stub (replace with `main`) |
| `0x00462e90` | `WinMain` | Application entry point |
| `0x004061e0` | `CGWND_Constructor` | Engine root object ctor |
| `0x004068d0` | `CGWND_LoadConfig` | Registry/INI config load |
| `0x00406ba0` | `GameLoop_Setup` | Subsystem init + timer start |
| `0x0045c3c0` | `GameFrame_Update` | Per-frame tick (35fps) |
| `0x00408130` | `SetGameState` | State machine transition |
| `0x0045b500` | `DD_Init` | DirectDraw init |
| `0x0045baa0` | `DD_Shutdown` | DirectDraw teardown |
| `0x0045b7e0` | `DS_Init` | DirectSound init |
| `0x00446050` | `RESMGR_Init` | Resource manager init |
| `0x0045caa0` | `RFHMGR_Load` | RFH/RFD archive load |
| `0x004208f0` | `CAudioStateMachine_SetState` | Audio state machine |
| `0x00454250` | `CMciVideoPlayer_Construct` | AVI player ctor |
| `0x00452e10` | `CUserProfile_Construct` | User profile ctor |
| `0x004855e8` | `g_ResMgr` | Global resource manager (~150KB) |
| `0x004851f4` | `g_gameState` | Engine state variable |
| `0x00485444` | `g_timerFired` | Timer tick flag |
| `0x004a990c` | `g_hGameLoopEvent` | Game loop sync event |

---

*This guide is based on reverse engineering of `loco.exe` (1998, Intelligent Games / LEGO Media) using Ghidra. All addresses are from the shipping Windows binary. The port does not include the original game assets, which remain proprietary.*
