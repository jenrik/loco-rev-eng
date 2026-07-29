# Audio System Analysis — Lego Loco

> Evidence-backed from Ghidra decompilation of `loco.exe` (1998, MSVC x86).
> All function addresses verified against the canonical Ghidra database `loco1`.

## Architecture overview

Lego Loco has **two** distinct sound playback layers:

| Layer | Function | Address | Usage |
|-------|----------|---------|-------|
| **Free function** | `PlaySound(uint id)` | `0x447930` | UI buttons, menu clicks, lobby controls |
| **Free function** | `PlaySoundAt(uint id, int x, int y, uint atten)` | `0x4479D0` | Positional audio (dino easter-egg) |
| **Method** | `Game::PlaySound(void* this, int id)` | `0x411FB0` | In-game cursor hover/click (stereo-panned) |
| **Method** | `GameAudio::PlayResource(uint id)` | `0x413180` | Direct playback at listener position |
| **Method** | `GameAudio::PlayResourceEx(uint id, void** out)` | `0x4131C0` | Playback with channel tracking |

## Sound resource IDs

| ID | Path | Purpose |
|----|------|---------|
| `0x5015` | `sounds\toybox\clstray1` | **UI click sound** — used by ALL menu/lobby button presses EXCEPT main-menu quit |
| `0x5026` | `sounds\toybox\sweep1` | **Exit sweep** — played by `CGWND_SetMode(10)` when quitting |
| `0x50F3`–`0x511A` | various toybox sounds | Random sounds for dinosaur easter-egg in lobby |
| `video\music.wav` | disk file (7.6MB, PCM 16-bit stereo 22050Hz) | **Background music** — looped via `PlaySoundA(path, nullptr, SND_ASYNC|SND_LOOP=9)` |

## Main menu button sounds

### Assembly evidence

The EditWindow WindowProc (undefined range `0x42292A`–`0x422D7F`, entry at `0x422940`)
handles all main-menu button clicks. Raw-byte analysis reveals exactly **three**
sound-resource pushes, all `0x5015`:

| Address | Button | Code |
|---------|--------|------|
| `0x422A72` | Accept/Play (`+0x13C`) | `push 0x5015; call PlaySound` — **click sound ✓** |
| `0x422BE2` | (accept path, tracked) | `push 0x5015; call GameAudio_PlayResource` — with channel wait |
| `0x422D57` | Selection change | `push 0x5015; call PlaySound` — **click sound ✓** |

### Quit button (0x422AC3)

The quit button handler at `0x422AC3` does **NOT** push any sound resource ID.
Its full flow:

1. Restores button background from offscreen surface (`pMainSurface` at `+0x1F0`)
2. Draws pressed sprite `0x406` (exit down)
3. Calls `Sleep(0x96)` (150ms)
4. Calls `SetRenderSurface(nullptr, 0, nullptr, 0, 1)` via vtable[4]
5. Calls `CGWND_SetMode(10)` → which plays **`0x5026`** (exit sweep)

This makes the quit button sound distinctly different from other buttons:
only the exit sweep plays, no click sound.

### Accept button (0x422A72)

1. `PlaySound(0x5015)` — click sound
2. Restores background, draws pressed sprite `0x404`
3. `Sleep(0x96)`
4. `EditWindow::OnPlayerNameChanged` (`0x422660`) — validates and commits name
5. Transitions to lobby (state 3 → GameSetupPanel)

## Multiplayer lobby button sounds

Every actionable control in `GAMESTATE_HandleClick` (`0x40A4E0`) plays
`PlaySound(0x5015)`:

- Back/Exit (first PtInRect check at `+0x220`)  
- Search (second check at `+0x224`)  
- Options (third check at `+0x228`)  
- Go (drawn only when session available, at `+0x22C`)  
- Layout list selection (vertical PtInRect at `+0x1DC`)  
- Map click-through (PtInRect at `+0x1EC`)  

The dinosaur easter-egg region (`+0x20C`) plays `PlaySoundAt(rand % 0x1999 + 0x50F3, x, y, 4)`.

## Background music

### Startup

`EditWindow::setState(7)` at `0x4208F0`:
1. If `previous_state == 0`: calls `DDRAW_InitAudio()`
2. If `previous_state == 0 || previous_state == 1`: plays `video\music.wav` via
   `PlaySoundA(path, nullptr, 9)` — flag 9 = `SND_ASYNC | SND_LOOP`

### Shutdown

`EditWindow::setState(1)` calls `PlaySoundA(nullptr, nullptr, 0)` to stop all audio.

### Host implementation

On the SDL3 host:
- `PlaySoundA` routes through `SDL3_GameAudioPlayFile(path, looping)`
- The music file is at `LEGO_LOCO_DATA/art-res/video/music.wav`
- `g_install_path` falls back to `$LEGO_LOCO_DATA/art-res` when the INI-derived
  Windows path (e.g. `d:\loco\art-res`) doesn't exist
- Backslashes in paths are normalized to forward slashes

## Resource loading

### PlaySound (0x447930)
```c
void PlaySound(UINT resource_id) {
    // Valid range: 0x5000–0x605F
    // Lazy-loads via ResourceManager_LoadStringTable
    // Caches loaded resource in global array at 0x49161C
    // Calls GameAudio_AllocChannel(g_audio, res, NULL, g_listener_x, g_listener_y, 4, 0)
}
```

### Game_PlaySound (0x411FB0)
```c
void __thiscall Game_PlaySound(void* this, int resource_id) {
    // Checks game object (+0x40) for building context
    // Loads via ResourceManager_GetById
    // Calls vtable[6] (InitBase) on resource
    // Repositions Game for stereo panning based on building position
}
```

## Key differences from earlier understanding

1. **PlaySound (0x447930) ≠ Game_PlaySound (0x411FB0)** — they are completely
   separate functions with different callers and behavior. The UI uses PlaySound;
   in-game code uses Game_PlaySound.

2. **Quit button has NO click sound** — the original assembly shows the quit
   handler at 0x422AC3 does not call PlaySound. Only CGWND_SetMode(10) produces
   audio (the exit sweep 0x5026).

3. **0x5015 is preloaded, not preplayed** — EditWindow::show loads resource
   0x5015 to warm the cache, but does not play it. Each button press calls
   PlaySound(0x5015) individually.
