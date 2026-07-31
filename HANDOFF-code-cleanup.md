# HANDOFF — Compiler Flags Cleanup

## Current status (2026-08-01)

- Default Tier 1 is clean: `make` and `make check` compile/link all 134 enabled translation units.
- Forced fresh audit before final Ghidra ABI remediations: `STRICT=1` had 11 failing units / 98 diagnostics; `STRICT=2` had 50 failing units / 5,512 diagnostics. Tier 3 is implemented but not yet clean.
- The final pass additionally Ghidra-verified and repaired TrainMessage/NetworkPlayerList construction, `DPlayManager::EnumerateSessions` static fastcall ABI, and reviewed raw vtable/COM dispatch.
- `make test` currently reaches archive tests then stops because `lego-loco-unpacked/art-res/resource.RFH` is absent.

`PROGRESS.md` is the durable current plan; remaining content below is historical context from the initial handoff.

## Branch

`feature/compiler-flags-tier1-3` in worktree at:
`/home/user/projects/v43/jenrik/lego-loco-rev-eng-compiler-flags`

## Summary

Replaced the old blanket suppression (`-Wno-error -Wno-attributes -Wno-delete-non-virtual-dtor ...`) with a three-tier flag system that catches AGENTS.md anti-patterns at build time.

## Three-Tier Flag System

### Tier 1 — Always-on (default `make`)
Catch real anti-pattern bugs on every build. **STATUS: ✅ Clean.**

| Flag | Anti-pattern caught |
|---|---|
| `-Werror=class-memaccess` | #2 — raw writes on vtable-bearing types |
| `-Werror=cast-function-type` | #5 — vtable dispatch function pointer casts |
| `-Werror=strict-aliasing` | #6 / #8 — type-punned offset access, void* misuse |
| `-Werror=non-virtual-dtor` | #3 — missing virtual destructors |
| `-Werror=delete-non-virtual-dtor` | #3 — deleting through non-virtual base |
| `-Werror=overloaded-virtual` | #7 — hidden virtual methods in flat hierarchies |
| `-Werror=suggest-override` | #7 — missing override annotations |
| `-Werror=return-type` | #13 — silent stub returns |
| `-Werror=reorder` | #7 — initializer-order mismatches |
| `-Werror=narrowing` | Type safety |
| `-Werror=write-strings` | Type safety |
| `-Werror=int-to-pointer-cast` | Pointer truncation |
| `-Werror=pointer-arith` | Void* arithmetic |
| `-Werror=pmf-conversions` | Pointer-to-member safety |
| `-Werror=invalid-offsetof` | Invalid offsetof usage |
| `-Werror=missing-field-initializers` | Incomplete initialization |
| `-Werror=subobject-linkage` | Anonymous namespace hygiene |
| `-Werror=cast-align` | Alignment safety |

### Tier 2 — STRICT=1 (`make STRICT=1`)
Tighten type hygiene. **STATUS: 🔴 391 errors.**

| Flag | Errors | What it catches | Approach |
|---|---|---|---|
| `-Werror=zero-as-null-pointer-constant` | 360 | `0` used as null pointer | Mechanical: replace `= 0;` with `= nullptr;` in pointer contexts |
| `-Werror=cast-qual` | 31 | const-correctness violations | File-by-file: use `const_cast` or fix const correctness |
| `-Wattributes` + `-Werror=ignored-attributes` | 0† | #4 — `__thiscall`/`__fastcall` on methods | 1,311 occurrences exist but `-Wno-attributes` in Tier 1 suppresses the warning before STRICT=1 can override |

† The `-Wno-attributes` in Tier 1 base flags is winning over `-Wattributes` in STRICT=1. To make `-Werror=ignored-attributes` actually fire, either remove `-Wno-attributes` from base flags or use a different override mechanism. When working, this catches `__thiscall`/`__fastcall` annotations on C++ methods (anti-pattern #4) — there are 1,311 such annotations across the codebase.

### Tier 3 — STRICT=2 (`make STRICT=2`)
Full C++ audit. **STATUS: 🔴 ~9,000+ errors (aspirational).**

| Flag | What it catches | Why aspirational |
|---|---|---|
| `-Werror=old-style-cast` | #5 / #6 — all C-style casts | ~9,000 C-style casts from Ghidra decompilation; automated conversion via perl/agents introduced syntax regressions in nested expressions, function pointer casts, and intptr_t/uintptr_t patterns |
| `-Weffc++` | Effective C++ rules | Very noisy; needs per-class enablement |
| `-Werror=missing-declarations` | Functions without prior declarations | #9 — catches FUN_ artifacts added without headers |

## Files modified for Tier 1 fixes (15 source files)

| File | Fix |
|---|---|
| core/Entity.h | override on InitBase and Update |
| core/Game.h | override on Update |
| ui/EditWindow.h | override on show() and hide() |
| ui/PostcardPreviewWindow.h | override on show() |
| input/Cursor.h | override on hide(), using UI_WindowBase::show |
| ui/TrainStationWindow.h | override on hide(), using GameWindow::show |
| ui/HelpWnd.h | using GameWindow::{show,create,set_mode} |
| game/Train.h | using Building::Update in TrainEntity |
| graphics/LOCOBITMAP.h | virtual ~PostcardAlbum() |
| network/DPlayManager.h | virtual ~DPlayManager() |
| network/NetHelpers.h | virtual ~PoolAllocator() |
| network/NetworkPlayerList.h | virtual ~NetworkPlayerList() |
| world/scriptengine.h | virtual ~ScriptEngine(), virtual ~RESDATA_ScriptedObject() |
| game/GameConfig.cpp | return this; in GameConfig_dtor |
| graphics/PixelDataCache.cpp | return this; in DestroyFromResource |
| graphics/LOCOBITMAP.cpp | return this; in DestroyFromResource |
| shared/TimerSlotList.cpp | return this; in two dtor variants |
| resources/ResourceManager.cpp | Fixed misplaced return self; |

## What was tried for old-style-cast and why it failed

1. **4 parallel agents** (GPT-5.6 Luna): Reduced errors from ~9,000 to ~6,600 but introduced ~80 invalid-cast regressions (reinterpret_cast on arithmetic types), had to be reverted.

2. **Perl script pass 1** (safe patterns): (Type*) -> reinterpret_cast<Type*>(), 131 files, reduced to ~3,000. Committed.

3. **Perl script pass 2** (arithmetic types): (int) -> static_cast<int>(), broke nested expressions, reverted.

4. **Python script**: Crashed with regex backreference errors, corrupted files, reverted.

5. **More parallel agents**: Same pattern — introduced syntax errors from mishandling nested parentheses, function pointer types, and intptr_t/uintptr_t ambiguity.

**Root cause**: C-style cast conversion cannot be done safely with regex or LLMs that don't understand C++ operator precedence. The patterns that break:
- Multi-level nested casts: *(Type*)((intptr_t)ptr + offset)
- Function pointer casts: (void (*)(void*, int))expr
- intptr_t/uintptr_t: ambiguous whether source is pointer or integer
- Casts spanning macro expansions

**Recommendation**: Enable -Werror=old-style-cast per-directory as each subsystem reaches INTEGRATED status, with manual conversion using a C++ parser (clang-tidy cppcoreguidelines-pro-type-cstyle-cast can auto-fix with --fix).

## Next steps for STRICT=1 cleanup

### zero-as-null-pointer-constant (360 errors)
Python script using GCC error output to target specific lines. Approach:
- Extract file:line from GCC errors
- On each line, replace `= 0;` with `= nullptr;` and `return 0;` with `return nullptr;`
- Skip lines where 0 is an integer value (not a pointer)

### cast-qual (31 errors)
File-by-file manual fixes. These are const char* -> char* type issues, typically in Win32 API calls. Use const_cast or fix the declarations.

### ignored-attributes (0 errors currently, 1,311 latent)
Fix the -Wno-attributes override issue in the Makefile, then tackle the 1,311 __stdcall/__fastcall/__thiscall annotations. Legitimate ones on Win32 API extern declarations should stay (move to a compat.h macro that expands to nothing on Linux). Ones on C++ methods are anti-pattern #4.

## Makefile locations

Both Makefiles have the tiered flag structure:
- Root: Makefile (lines 23-66)
- Decompiled: src/decompiled_cpp/Makefile (lines 56-94)
