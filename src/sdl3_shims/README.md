# SDL3 Shims for DirectX

## Purpose

This directory contains **SDL3-based compatibility shims** for the DirectX APIs
used by Lego Loco (loco.exe, 1998). They allow the decompiled C++ code in
`src/decompiled_cpp/` to compile and run on Linux without Windows or the
original DirectX runtime.

These shims are **not part of the reverse-engineering effort**. They are a
separate portability layer. The decompiled code is the single source of truth
derived from the Ghidra database; these shims merely provide the platform
abstraction the decompiled code expects.

## What's shimmed

| Original API       | Shim file          | Backend    | Status       |
|--------------------|--------------------|------------|--------------|
| DirectDraw 4       | `sdl3_ddraw.h/cpp` | SDL3 Renderer | Partial      |
| DirectSound        | `sdl3_dsound.h/cpp`| SDL3 Audio   | Partial      |
| DirectPlay 4       | `sdl3_dplay.h/cpp` | Stub        | Stub only    |

## Usage

Compile the shims alongside the decompiled C++ code:

```
g++ -c sdl3_shims/sdl3_ddraw.cpp -I src/decompiled_cpp $(pkg-config --cflags sdl3)
g++ -c sdl3_shims/sdl3_dsound.cpp -I src/decompiled_cpp $(pkg-config --cflags sdl3)
```

Then link the shim objects instead of the stub objects.

## Architecture

Each shim header mirrors the corresponding stub in `src/decompiled_cpp/stubs/`
but provides actual SDL3-backed implementations rather than empty type
definitions.

The shims are **completely isolated** from the decompiled code:
- No decompiled files are modified
- No build system changes are needed in the decompiled tree
- Simply compile with `-I src/sdl3_shims` before `-I src/decompiled_cpp/stubs`

## Palette handling strategy

Lego Loco uses 8-bit palettized BMPs extensively. Rather than implementing
a fragment shader for runtime palette lookup (complex, GPU-specific), we
convert palettized images to 32-bit RGBA **at load time**.

### How it works

1. `SDL_LoadBMP_IO` loads the BMP as an indexed surface with its embedded palette
2. `SDL_ConvertSurface(surface, SDL_PIXELFORMAT_XRGB8888)` expands each palette
   index to a full 32-bit RGBA color using the BMP's palette
3. The resulting 32-bit texture is uploaded to the GPU via `SDL_CreateTextureFromSurface`
4. All rendering operates on standard 32-bit textures — no shader required

### Trade-offs

- **Pro**: No GPU shader needed; works on any SDL3-supported platform
- **Pro**: Simpler rendering pipeline; no palette state to manage at runtime
- **Con**: Runtime palette swaps (palette cycling for water/sky animations) are not
  supported — the palette is baked at load time
- **Con**: Slightly higher VRAM usage (32-bit vs 8-bit textures)

## Limitations

- DirectDraw: Palettized surfaces are baked to 32-bit at load time (see above).
  IDirectDrawSurface4::Lock provides raw pixel access but performance
  will differ from hardware-accelerated DirectDraw.
- DirectSound: Streaming buffer Lock/Unlock is stubbed; audio must be
  pre-loaded. 3D positioning is not implemented.
- DirectPlay: Fully stubbed. Lego Loco networking uses IPX and serial
  protocols that have no SDL3 equivalent. This shim returns success
  for initialization but does not pass network traffic.

## Related

- `src/decompiled_cpp/stubs/ddraw.h` — type-only stub for compilation checks
- `src/decompiled_cpp/stubs/dsound.h` — type-only stub for compilation checks
- `src/decompiled_cpp/stubs/dplay.h` — type-only stub for compilation checks
