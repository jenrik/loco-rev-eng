# Running Lego Loco

Lego Loco (1998) runs under Wine on a virtual framebuffer (Xvfb) with software rendering. No GPU is required. Two binary patches to `loco.exe` bypass a colour-depth check that prevents the game from starting under Wine.

---

## Quick start

### First-time setup

Unpack the ISO and run setup once:

```bash
# 1. Unpack the ISO (if not already done)
7z x lego-loco.iso -o lego-loco-unpacked/

# 2. Install game files and apply binary patches
./setup-game.sh
```

`setup-game.sh` copies game files into `wine-prefix/drive_c/loco/`, writes `LEGO.INI`, and applies both binary patches to `loco.exe`.

---

## Running for a human player

### On the host (NixOS)

Enter the Nix shell for all required tools, then launch the game. If `DISPLAY` is not set the script starts its own Xvfb and tears it down on exit. If `DISPLAY` is already set (e.g. `DISPLAY=:0`) the game appears on that screen directly.

```bash
nix-shell

# Headless — game runs in the background on :99
DISPLAY=:99 ./run-game.sh &

# Attach VNC so you can see and interact with it (port 5900)
env -u WAYLAND_DISPLAY XDG_SESSION_TYPE=x11 \
  x11vnc -display :99 -rfbport 5900 -forever -noxdamage -nopw -bg
# Connect with any VNC viewer: localhost:5900
```

The `WAYLAND_DISPLAY` and `XDG_SESSION_TYPE` overrides are required because x11vnc 0.9.17 exits immediately when it detects a Wayland host session. The Xvfb display at `:99` is pure X11 regardless; those env vars only affect how x11vnc decides whether to start.

### In a container (Docker / Podman)

```bash
# Build once
docker build -t lego-loco .

# Display mode — VNC on localhost:5900
docker compose --profile display up loco-display

# Connect with any VNC viewer: localhost:5900
```

Set `VNC_PASSWORD` in the environment if you want a password on the VNC port.

---

## Running for an agent

The agent interacts with the game headlessly via `screenshot.sh` and `send-input.sh`. Both scripts default to `DISPLAY=:99` and can be overridden.

### On the host

```bash
nix-shell

# Start the game
DISPLAY=:99 ./run-game.sh &
sleep 10   # wait for the title screen

# Take a screenshot
DISPLAY=:99 ./screenshot.sh /tmp/screen.png

# Send input
DISPLAY=:99 ./send-input.sh key Return          # press Enter
DISPLAY=:99 ./send-input.sh click 512 400       # left-click at (512, 400)
DISPLAY=:99 ./send-input.sh rclick 200 300      # right-click
DISPLAY=:99 ./send-input.sh move 512 400        # move mouse without clicking
DISPLAY=:99 ./send-input.sh type "player1"      # type a string
```

### In a container

```bash
# Headless mode (default)
docker compose up loco

# Interact from another terminal
docker compose exec loco screenshot.sh /tmp/screen.png
docker compose exec loco send-input.sh key Return
docker compose exec loco send-input.sh click 512 400
```

### Display resolution and coordinates

The virtual display is 1024×768. The game window fills the display. Pixel coordinates passed to `send-input.sh click` map directly to screen pixels.

---

## Environment variables

| Variable | Default | Effect |
|---|---|---|
| `DISPLAY` | `:99` | X display to use |
| `WINEPREFIX` | `./wine-prefix` | Wine prefix directory |
| `WINEARCH` | `win32` | Must stay `win32` |
| `WINEDLLOVERRIDES` | `ddraw=b` | Force Wine builtin ddraw (see below) |
| `VNC_ENABLED` | `0` | Container only: start x11vnc on port 5900 |
| `VNC_PASSWORD` | _(unset)_ | Container only: VNC password |
| `LOCO_AUTOSTART` | `1` | Container only: start game automatically |

---

## Project structure

```
run-game.sh          Host launcher (headless or on existing DISPLAY)
setup-game.sh        First-time install + patch
screenshot.sh        Capture display :99 → PNG
send-input.sh        Inject keyboard/mouse via xdotool
patches/
  skip-color-depth-check.sh   Apply both binary patches
Dockerfile           Container image (Debian + Wine + Xvfb + x11vnc)
docker-compose.yml   headless (loco) and display (loco-display) services
entrypoint.sh        Container entrypoint
shell.nix            Nix dev shell with all host tools
wine-prefix/         Wine installation + game files (gitignored)
```

---

## Technical notes

### Why the game requires binary patches

Lego Loco performs two independent colour-depth checks at startup. Both fail under Wine even when Xvfb is set to 16-bit depth, and each had to be patched separately.

#### The display stack

```
loco.exe (Win32, 32-bit)
  └── Wine (32-bit, WINEARCH=win32)
        └── Xvfb :99 -screen 0 1024x768x16  (virtual framebuffer, 16-bit)
```

Wine uses software rendering via `llvmpipe` (Mesa). No real GPU is involved. The `-screen 0 1024x768x16` flag sets the X11 display depth to 16 bits per pixel, which is what the game was designed to require — but Wine's GDI layer reports 32-bit depth to the game anyway (it converts internally), which is what triggers the checks.

#### Why DDrawCompat doesn't help

The game ships `DDrawCompat.dll` (v0.7.1) in the `Exe/` directory alongside `loco.exe`. DDrawCompat is a compatibility shim that intercepts DirectDraw calls and emulates 16-bit mode regardless of the real display depth. However:

- Wine's default DLL load order for `ddraw` is `builtin,native`, so Wine loads its own `ddraw.dll` first and never touches DDrawCompat.
- Forcing `WINEDLLOVERRIDES=ddraw=n` (native first) makes Wine attempt to load DDrawCompat, but its `DllMain` returns `FALSE` and Wine aborts with `STATUS_DLL_INIT_FAILED`.

The `WINEDLLOVERRIDES=ddraw=b` in all scripts forces Wine's builtin DirectDraw unconditionally, side-stepping the DDrawCompat crash entirely.

---

### Patch 1 — DirectDraw init bypass

**Location:** `fcn.00446050`, virtual address `0x446063`, file offset `0x45463`

**Change:** `75 0B` (`jne +11`) → `EB 0B` (`jmp +11`)

`fcn.00446050` is a mid-level initialisation wrapper. It calls `fcn.0045b500`, which sets up DirectDraw surfaces and detects the pixel format (RGB 5-5-5 vs 5-6-5). When `fcn.0045b500` returns 0 (failure — Wine's DirectDraw can't satisfy the 16-bit surface request), the original `jne` skips over the failure handler only if the return value is nonzero. Changing it to an unconditional `jmp` makes `fcn.00446050` always continue regardless of what DirectDraw returned.

```asm
; Before
0x446061  test  al, al
0x446063  jne   0x446070   ; 75 0B — skip cleanup only if DD succeeded
0x446065  ...              ; early-exit path

; After
0x446063  jmp   0x446070   ; EB 0B — always skip cleanup (DD failure ignored)
```

---

### Patch 2 — GDI colour check bypass

**Location:** `fcn.00406680`, virtual address `0x4066CA`, file offset `0x5ACB` (displacement byte only)

**Change:** displacement byte `0x83` → `0x01` (redirects `jmp 0x406752` to `jmp 0x4066D0`)

This is the main colour-depth gate. `fcn.00406680` is called from `main` after DirectDraw init and returns a boolean: `1` = display is acceptable, `0` = show error dialog and quit.

The function logic as shipped:

```c
// Pseudocode of fcn.00406680
bool check_display(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    int numColors  = GetDeviceCaps(hdc, NUMCOLORS);   // -1 for TrueColor
    int bitsPixel  = GetDeviceCaps(hdc, BITSPIXEL);   // 16, 24, or 32
    ReleaseDC(hwnd, hdc);

    if (numColors > -1)          goto error;  // palette mode (8-bit)
    // TrueColor path:
    compare(bitsPixel, 16);      // result computed but never used
    goto error;                  // UNCONDITIONAL — always fails
    // ...
    // Resolution checks (800–1280 wide) — never reached from above
    // ...
    return true;   // success path — unreachable via colour checks

error:
    MessageBoxA(NULL, load_string(122), "LEGO LOCO", 0);
    return false;
}
```

The `goto error` after the `bitsPixel` comparison is unconditional in the shipped binary — it is an `E9` (near `jmp`) not a conditional branch. This means `fcn.00406680` **always** shows the error dialog and returns `false`, regardless of the actual colour depth. This appears to be a shipping bug: the conditional jump was never wired up.

The patch changes a single byte in the jump's displacement operand, redirecting the jump from the error path (`0x406752`) to the mouse-presence check (`0x4066D0`), which is the correct "TrueColor accepted" continuation:

```asm
; Before (file offset 0x5ACA)
E9 83 00 00 00   jmp 0x406752   ; → error dialog

; After (only byte 0x5ACB changes)
E9 01 00 00 00   jmp 0x4066D0   ; → GetSystemMetrics(SM_MOUSEPRESENT)
```

After the redirect, execution falls through:

1. **Mouse check** (`0x4066D0`): `GetSystemMetrics(SM_MOUSEPRESENT)` — Wine reports a mouse present; check passes.
2. **Resolution check** (`0x4066EA`): screen width must be between 800 and 1280 pixels. At 1024×768 this passes.
3. **Return true** (`0x406745`): `fcn.00406680` returns 1 and `main` continues.

#### Disassembly context

```asm
; fcn.00406680 — colour and display check
0x40669E  push  0x18              ; NUMCOLORS
0x4066A0  push  esi               ; hDC
0x4066A1  call  GetDeviceCaps
0x4066A6  mov   ebp, eax          ; ebp = NUMCOLORS (-1 for TrueColor)

0x4066A3  push  0x0C              ; BITSPIXEL
0x4066A5  push  esi
0x4066A8  call  GetDeviceCaps
0x4066AA  mov   [0x48521C], eax   ; store BITSPIXEL globally

0x4066BA  cmp   ebp, 0xFFFFFFFF   ; NUMCOLORS vs -1
0x4066BD  jg    0x406752           ; palette mode → error
                                   ; TrueColor falls through:
0x4066C3  cmp   [0x48521C], 0x10  ; BITSPIXEL vs 16 (result unused)
0x4066CA  jmp   0x406752           ; ← PATCHED: displacement 0x83→0x01
                                   ;   now jumps to 0x4066D0 (mouse check)

0x4066D0  push  0x13              ; SM_MOUSEPRESENT
0x4066D2  call  GetSystemMetrics
0x4066D8  test  eax, eax
0x4066DA  jne   0x4066EA          ; mouse present → resolution check
; ...
0x406745  mov   al, 1
0x406747  ret                      ; return true — display accepted
```

#### Address arithmetic

The `.text` section maps from file offset `0x400` to virtual address `0x401000` (a constant delta of `0x400 - 0x1000 = -0xC00`). File offset = `VA - 0x401000 + 0x400`.

| Location | VA | File offset |
|---|---|---|
| Patch 1 (jne→jmp) | `0x446063` | `0x45463` |
| Patch 2 (jmp displacement) | `0x4066CB` | `0x5ACB` |

The patch script (`patches/skip-color-depth-check.sh`) verifies the expected original bytes before writing, and is idempotent (safe to re-run on an already-patched binary).

---

### Original installer

The game's InstallShield installer is a 16-bit Windows executable. It crashes under Wine's `winevdm` (16-bit emulation layer) and cannot be used. `setup-game.sh` manually copies files from the unpacked ISO instead.

### LEGO.INI

The game reads `LEGO.INI` from the same directory as `loco.exe` at startup:

```ini
[DIRECTORIES]
Res=c:\loco\art-res
ResFile=c:\loco\art-res\resource.rfh
exe=loco.exe

[Video]
Dir=c:\loco\art-res\video\locoIntr.avi
```

`setup-game.sh` writes this file with the correct Windows-style paths for the Wine prefix layout.
