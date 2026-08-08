# Lego Loco Reverse Engineering

## Source of truth

**Assembly first.** Original game behavior derives from `loco.exe`; the Ghidra
database is canonical. Never invent behavior, names, signatures, inheritance,
or original x86 layouts/offsets. When unsure, inspect the bytes.

Every reconstructed method must include its original address:

```cpp
/** GameObject::Draw — vtable[11]
 *  Address: 0x405E60 */
void GameObject::Draw()
```

## Session state

Read `PROGRESS.md` at the start of every session. Update it after a significant
milestone: completed subsystem, major cross-file fix, new tool/test, new runtime
capability, or phase change.

- Move completed TODOs to **Completed** with `- [x]`.
- Keep remaining work prioritized with `- [ ]`.
- Update metrics/architecture notes when they change.
- Add one concise date-stamped session-log line.
- Do not record speculation, transient notes, or stack traces.

## Ghidra

Open the raw binary, never the `.gpr` file:

```text
mcp.ghidra.open_database({
  file_path: "/home/user/projects/v43/jenrik/lego-loco-rev-eng/lego-loco-unpacked/Exe/loco.exe",
  database_id: "locoN"
})
mcp.ghidra.wait_for_analysis({ database: "locoN" })
```

Never use `force_new: true`. The binary is 32-bit x86 PE built with MSVC 1998.
Validate with decompilation,
disassembly, xrefs, vtables, structures, globals, and strings—not decompiler
output alone.

Evidence rules:

- Function address + calling convention + callers determine signatures.
- Vtable membership/slots determine methods and hierarchy.
- `this`-relative accesses determine fields and offsets.
- Branch opcodes determine signedness (`JA/JB` vs `JG/JL`).
- Replace Ghidra labels only when evidence supports a semantic name.
- `shared/vtable_addrs.h` is documentation only; C++ manages vtables.

## Correctness and completion

Behavior must match the assembly for all inputs:

- Preserve control flow, data flow, widths, signedness, calling conventions,
  return values, side effects, ownership, and error paths.
- Account for every basic block and identify every call target.
- Document original x86 field offsets in canonical headers; put every global in
  its canonical declaration.
- Verify original callers/callees, virtual slots, layouts, and allocations agree.
- Do not simplify assembly unless equivalence is proven and documented.

File status:

- `// Status: TRANSCRIBED` — cleaned decompiler output; compiles; not validated.
- `// Status: VALIDATED` — checked instruction-by-instruction against assembly.
- `// Status: INTEGRATED` — validated, typed, and wired into C++ hierarchy.

Only **INTEGRATED** is done. Progress in order: transcribe and clean; validate
against disassembly; integrate named fields/types and virtual dispatch.

## Fix anti-patterns on sight

The patterns below are defects, not tolerated legacy style. **Whenever you see
one in a file you inspect or edit, fix it before declaring the work complete**,
even if it predates your change. Do not copy, preserve, or introduce one. Check
the assembly before choosing the replacement.

- `_Ctor`/`_Dtor` free functions → real C++ constructors/destructors.
- Constructor `void*` returns or explicit vtable writes → remove; C++ emits them.
- Scalar/vector deleting-destructor flags, conditional `operator delete`, or
  `return this` → keep only user cleanup; document compiler-generated slots.
- `Class_Method(self, ...)`, explicit `this`, or C++ `__thiscall`/`__fastcall`
  declarations → typed class methods with implicit `this`.
- Literal vtable assignment/access/dispatch → typed virtual methods/calls.
- Raw `this + offset`, cast-based field access, or expanded scalar arrays →
  named fields/arrays in the one canonical class header.
- Flat inherited structs or duplicate/partial layouts → actual inheritance and
  one canonical definition.
- Known objects stored or passed as `void*`/`void**` → strongest evidenced type.
- `FUN_`, `DAT_`, `PTR_LAB_`, `RESDATA_`, `GAMESTATE_`, `LOCOBITMAP_`, or
  `RESMGR_` artifacts → evidence-backed semantic names.
- `param_1`/`field_XX` when use proves meaning → descriptive names.
- C++ methods, constructors, destructors, operators, or C++ globals inside
  `extern "C"` → C++ linkage; reserve C linkage for actual C ABI symbols.
- MSVC-only cast-to-lvalue syntax or direct base-constructor calls → valid C++.
- Internal no-op/null-return stubs → decompile the function.

Do not manually read or write `VTBL_*` in executable code. Avoid raw pointer
arithmetic; a temporary cross-cast offset is allowed only when evidence is
recorded with a precise TODO and it is removed during integration.

## Host deviations and SDL3

The original Windows/game path remains assembly-derived. **Every deviation from
original game code**—portability behavior, host rendering/input/audio/networking,
test instrumentation, diagnostics, host-only fields, or altered control
flow—must be inside the exact guard:

```cpp
#ifndef _WIN32
// Host-only deviation, usually backed by SDL3.
#endif
```

Exact original x86 struct layouts and field offsets are a documentation and
Windows-reconstruction concern, but a **non-goal for host-only builds**. Do not
pack, pad, cast, or assert host objects into x86 layout parity; use safe native
layouts and typed adapters inside `#ifndef _WIN32`.

Use this exact boundary, not `LOCO_SDL3` or another feature macro. Never leak
host behavior into the original path. Preserve the recovered code and put its
host alternative in a guarded block. Every host-only file (no original
Windows counterpart at all — network discovery, resource archive loading,
SDL3 window/audio/DirectDraw shims, etc.) lives beside the decompiled classes
of the subsystem it belongs to (e.g. `graphics/sdl3_ddraw.cpp` next to
`graphics/DDRAW.cpp`) with its **entire body** wrapped in `#ifndef _WIN32`,
rather than in a separate directory. There is one unified source tree rooted
at the repository root — no more `decompiled_cpp/` vs `sdl3_shims/` split.
SDL3 is the non-Windows compatibility/presentation layer, never evidence for
original game behavior. Guard SDL includes, declarations, fields, and calls
when they touch decompiled code.

Palette handling: Lego Loco's 8-bit palettized BMPs are converted to 32-bit
RGBA at load time (`SDL_LoadBMP_IO` → `SDL_ConvertSurface` to XRGB8888) rather
than via a runtime palette-lookup shader — simpler pipeline, no palette state
to manage, at the cost of not supporting palette-cycling animations (water/
sky), since the palette is baked in at load.

## Stubs

Internal functions must be fully decompiled. Exceptions:

1. OS/hardware APIs may be implemented in `stubs/` or as a host-only,
   `#ifndef _WIN32`-guarded file beside the subsystem it belongs to. This
   applies only to functions that are genuinely thin wrappers — the body
   consists essentially of marshaling arguments into 1-2 real Win32/
   DirectDraw/DirectSound/DirectPlay calls, with no other original logic.
   For those, document the real signature (so the project knows what to
   link against) and delegate/stub rather than fully transcribing.
   A function merely *calling* an OS API is not automatically exempt: if it
   has real game-specific data structures, algorithms, or state beyond the
   API call, it is original game logic and still needs full decompilation,
   regardless of naming (`DDRAW_*`/`NETMAN_*`-style names in this codebase
   usually mean "game code that touches that subsystem," not "Windows
   plumbing" — verify by reading the body, don't infer from the name).
   The same file can be mixed: split the bookkeeping (must be fully
   reconstructed, host-independent) from the actual API marshaling calls
   (stub/delegate) rather than treating the whole file one way.
2. A temporarily deferred internal function must live in a dedicated stub file,
   say `// TODO: decompile 0xADDRESS`, and be tracked in `PROGRESS.md`.
3. Compiler-generated deleting destructors, RTTI, EH, and similar helpers are
   documented but not reimplemented.

Never use linker `--defsym` placeholders as stubs. Every executable temporary
stub must emit a clear warning to the logs and then fail with an explicit
assertion. Never return silent success from incomplete internal logic. Never
maintain a build-exclusion list (a "known broken, skip it" source-file list)
as a substitute for this — every file in the tree must compile.

## Build and tests

Run from the repository root:

```bash
meson setup build && meson compile -C build       # build build/lego_loco
meson test -C build                                # deterministic component/host-boundary regressions
meson test -C build --suite integration            # isolated Wayland/Sway GUI integration flows
meson test -C build && meson test -C build --suite integration  # both test layers
```

Run the smallest relevant tests while iterating and the full affected layer
before completion. Changes to SDL3, host adapters, runtime/UI flow, input,
rendering, or audio must run `meson test -C build --suite integration`. It
builds and drives the real binary in an isolated compositor; artifacts are
under `build/test-artifacts/`.

After adding a new source file to any subsystem directory, reconfigure so the
auto-discovered source list picks it up:

```bash
meson setup --reconfigure build
```

For the MinGW cross-compile typecheck build (compiles the decompiled classes —
not the host-only shim files, which compile down to empty translation units
under `_WIN32` — against real Windows headers; not linked, not a shippable
target):

```bash
meson setup build-mingw --cross-file cross/mingw32-typecheck.txt
ninja -C build-mingw -k 0   # keep-going: reports a full per-file census, expect failures
```

This is NixOS. Use project/Nix dependencies and compatibility headers; never add
real Windows SDK headers to the native Linux build.

## Code organization

- Each class/struct has one complete canonical header beside its implementation.
- Use typed pointers and real inheritance; let the compiler manage vtables.
- Put cross-cutting declarations in `shared/`, platform APIs in `stubs/`, and
  host SDL3 implementations beside the subsystem directory they belong to
  (`graphics/`, `audio/`, `network/`, `resources/`, `town/`, `ui/`), each
  wrapped in `#ifndef _WIN32`.
- Keep original address annotations on every implementation.
