# Lego Loco — Linux Port

A native Linux reconstruction of Lego Loco (1998), built by reverse-engineering
the original Windows binary (`loco.exe`) and replacing DirectX/Win32 with SDL3.
See `AGENTS.md` for the reverse-engineering methodology and code conventions,
and `PROGRESS.md` for current status.

## Setup

This project targets NixOS/Nix. Enter the dev shell before doing anything else:

```bash
nix develop
```

(or `direnv allow`, if you use direnv — see `.envrc`)

## Build

```bash
meson setup build
meson compile -C build
```

Re-run `meson setup --reconfigure build` after adding a new source file to any
subsystem directory (source lists are auto-discovered at configure time).

## Run

```bash
LEGO_LOCO_DATA="$(pwd)/lego-loco-unpacked" build/lego_loco
```

`lego-loco-unpacked/` must contain the original game's unpacked assets
(`art-res/`, `Exe/loco.exe`, etc.).

## Test

```bash
meson test -C build                        # unit/component/host-boundary tests
meson test -C build --suite integration    # isolated Wayland/Sway GUI tests
```

## MinGW typecheck build

Compiles the decompiled classes against real Windows headers for validation
(not linked, not a shippable binary):

```bash
meson setup build-mingw --cross-file cross/mingw32-typecheck.txt
ninja -C build-mingw -k 0
```
