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
host alternative in a guarded block. Keep SDL3 implementation in
`src/sdl3_shims/` or small guarded adapters in decompiled classes. SDL3 is the
non-Windows compatibility/presentation layer, never evidence for original game
behavior. Guard SDL includes, declarations, fields, and calls when they touch
decompiled code.

## Stubs

Internal functions must be fully decompiled. Exceptions:

1. OS/hardware APIs may be implemented in `stubs/` or `src/sdl3_shims/`.
2. A temporarily deferred internal function must live in a dedicated stub file,
   say `// TODO: decompile 0xADDRESS`, and be tracked in `PROGRESS.md`.
3. Compiler-generated deleting destructors, RTTI, EH, and similar helpers are
   documented but not reimplemented.

Never use linker `--defsym` placeholders as stubs. Every executable temporary
stub must emit a clear warning to the logs and then fail with an explicit
assertion. Never return silent success from incomplete internal logic.

## Build and tests

Run from the repository root:

```bash
make                   # build build/lego_loco
make check             # per-file compilation status
make test              # deterministic component/host-boundary regressions
make test-integration  # isolated Wayland/Sway GUI integration flows
make test-all          # both test layers
```

Run the smallest relevant tests while iterating and the full affected layer
before completion. Changes to SDL3, host adapters, runtime/UI flow, input,
rendering, or audio must run `make test-integration`. It builds and drives the
real binary in an isolated compositor; artifacts are under
`build/test-artifacts/`. See `docs/testing.md`.

For isolated decompiled-file checks:

```bash
cd src/decompiled_cpp
make
make check
make all
```

This is NixOS. Use project/Nix dependencies and compatibility headers; never add
real Windows SDK headers to the native Linux build.

## Code organization

- Each class/struct has one complete canonical header beside its implementation.
- Use typed pointers and real inheritance; let the compiler manage vtables.
- Put cross-cutting declarations in `shared/`, platform APIs in `stubs/`, and
  host SDL3 implementations in `src/sdl3_shims/`.
- Keep original address annotations on every implementation.
