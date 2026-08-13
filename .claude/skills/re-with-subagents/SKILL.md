---
name: re-with-subagents
description: Use when dispatching subagents to reverse engineer functions from Ghidra. Recovers the class model first and produces behavior-equivalent C++ integrated into canonical classes (or evidenced C where appropriate), never checked-in cleaned decompiler output.
---

# RE with Subagents

**REQUIRED BACKGROUND:** The `reverse-engineering` skill covers the environment-level tooling — the `ghidra` and `radare2` MCP servers, `rizin`, `binwalk`, `pwntools`, and the triage→deep-analysis workflow. Read it first to understand the tools. This skill is the *coordination layer*: how to fan that tooling out across parallel subagents. Use `reverse-engineering` for solo/triage work; use this skill when dispatching multiple RE agents.

## Overview

Dispatch subagents using the **`reverse-engineer` agent type** (`.claude/agents/reverse-engineer.md`). This agent comes pre-loaded with the RE process, Ghidra tool knowledge, calling convention detection rules, C-vs-C++ determination logic, class field offset catalog, vtable address constants, and output format conventions. The coordinator's per-function prompt is dramatically simpler — just the address, suspected class/language, and any function-specific context.

**Output:** The agent first recovers the receiver, hierarchy, virtual slots, fields, signatures, and ownership, then updates the canonical header and produces behavior-equivalent idiomatic C++. C (`.c` files in `native/`) is reserved for functions evidenced as originally C. A cleaned decompilation, offset-based pseudo-class, or cast-backed partial layout is rejected output, not an intermediate milestone to merge.

**Core insight:** Analysis and implementation belong in one evidence loop, but modeling must precede source edits. The `reverse-engineer` agent decompiles, checks assembly/xrefs/vtables/constructors, establishes the canonical C++ model, implements through that model, validates behavior, and annotates Ghidra. “One pass” never means “paste pseudocode now and integrate later.”

## When to Use

- You have the "loco" Ghidra database open and need functions reconstructed as integrated C++ (or evidenced C)
- You're coordinating parallel RE agents and need consistent output quality
- You have function-specific context (suspected class, purpose, related functions) to seed agents with

**Don't use for:** radare2 triage, simple function listing, or when you just need a 1-line summary.

## Dispatch Template

Always use `subagent_type: "reverse-engineer"`. The per-function prompt is minimal because the agent already knows the process.

### For C++ Methods

```
Agent:
  description: "RE ClassName::Method"           // short (3-5 words)
  subagent_type: "reverse-engineer"
  prompt: |
    Reverse engineer [METHOD NAME] at [ADDRESS].

    Class: [ClassName] (vtable: [0xVTBL_ADDR], header: <subsystem>/<ClassName>.h)
    Language: C++
    Context: [1-2 sentences about what this method does]

    Database: loco

    Recover the canonical class model before writing implementation.
    Update <subsystem>/<ClassName>.h first, then integrate the method in
    <subsystem>/<ClassName>.cpp. Do not use reinterpret_cast, void*, local
    partial-layout views, literal object offsets, or manual vtable dispatch to
    stand in for unresolved modeling.
```

### For C Functions

```
Agent:
  description: "RE FunctionName"
  subagent_type: "reverse-engineer"
  prompt: |
    Reverse engineer [FUNCTION NAME] at [ADDRESS].

    Language: C (this is a C free function — [brief reason: Win32 wrapper, CRT helper, etc.])

    Database: loco

    Write output to native/[name].c
```

### Language Hinting

The coordinator should determine language before dispatching, based on:
- **`__thiscall` convention with ECX = this** → C++ method (default assumption)
- **Vtable write pattern** (`*(void**)this = 0x00477xxx`) → C++ method
- **`__stdcall`/`__cdecl`, no this pointer, Win32 API heavy** → possibly C
- **Function in a known class's address range, called with ECX set** → C++ method

Add an explicit `Language:` line to the prompt when you're confident. When unsure, omit it — the agent defaults to C++.

**The `Database: loco` line is critical** — the agent passes this database name to every Ghidra MCP tool call. Without it, the agent has no database context. Update the database name if the session uses a different ID (e.g. `loco3`).

## Dispatch Examples

### C++ Methods (most common)

```
Agent 1 (subagent_type: "reverse-engineer"):
  description: "RE CGWND::ShowMainMenu"
  prompt: |
    Reverse engineer CGWND::ShowMainMenu at 0x406480.

    Class: CGWND (vtable: 0x4774C4, header: core/CGWND.h)
    Language: C++
    Context: Called early in WinMain after CGWND construction. Queries screen dimensions,
    reads window position from INI, clamps to visible area, reads per-type FPS limits.

    Database: loco

    Update the canonical core/CGWND.h model first, then integrate the method in core/CGWND.cpp.

Agent 2 (subagent_type: "reverse-engineer"):
  description: "RE Building::BaseCtor"
  prompt: |
    Reverse engineer Building::BaseCtor at 0x433A20.

    Class: Building (vtable: 0x477F18 base, header: game/Building.h)
    Language: C++
    Context: Shared base constructor for Building and Train. Calls Entity::Entity,
    then zero-initializes Building-specific fields (occupation, occupant links, timers).

    Related: Building::Building (0x4326F0), Train::Train

    Database: loco

    Update the canonical game/Building.h model first, then integrate the method in game/Building.cpp.
```

### C Functions (rare, specific cases)

```
Agent 3 (subagent_type: "reverse-engineer"):
  description: "RE CGWND_ParseCmdLine"
  prompt: |
    Reverse engineer CGWND_ParseCmdLine at 0x406790.

    Language: C (this is a C free function — tokenizes lpCmdLine, uses CRT strtok, no this pointer)

    Context: Tokenizes command line by spaces. Tests tokens against demo sentinels
    ("/s", "-s", "s") and seasonal theme keywords (Easter, Desert, Halloween, Winter, XMas).

    Database: loco

    Write output to native/cgwnd_parsecmdline.c
```

## Function Grouping Rules

| Functions per agent | When |
|---|---|
| 1 | Large method (>150 insns), complex control flow, or foundational class ctor/dtor |
| 2 | Medium methods, related pair (e.g. Load/Save, Begin/End, Ctor/Dtor of same class) |
| 3 | Small leaf methods, same class, no complex interdependencies |
| 4+ | Never — quality drops measurably |

When grouping multiple methods of the same class, the agent writes all of them into the same `.cpp` file. When grouping methods of different classes, specify separate output paths.

## Class Knowledge Flow

The agent knows core class layouts (Entity, CGWND, Building, BuildingComplex) and shared types (FrameData, RESDATA, RECT) from the agent definition and shared headers. When agents discover NEW fields, offsets, or classes:

### Between batches:

1. **Read new/modified headers**: `*/*.h` files agents wrote or updated
2. **Extract newly discovered**: field offsets, vtable addresses, class relationships
3. **Update shared headers**:
   - New vtable addresses → `shared/vtable_addrs.h`
   - New common structs/forward declarations → `shared/types.h`
4. **Update agent definition**: `.claude/agents/reverse-engineer.md` — add newly discovered classes to the "Known Classes" table
5. **Next batch's agents** automatically have the expanded knowledge

### Header merge coordination:

When dispatching multiple agents that may modify the same canonical class header (e.g., two agents both recovering `Building`), serialize them. Each agent must read the complete current header and may restructure assembly-shaped legacy declarations when evidence requires it; concurrent model edits would conflict even if methods differ.

## Batch Dispatch Pattern

Dispatch 4-5 agents in parallel only when their canonical classes and headers do
not overlap. After all complete, the coordinator must review each diff rather
than trusting a completion label:

1. Reject any added `reinterpret_cast`, C-style cast, known-object `void*`,
   literal object offset, local `*View`/`*Fields` layout, or manual vtable
   access unless it is a marked genuine external `ABI_BOUNDARY`.
2. Confirm the canonical header was updated before implementation and no
   duplicate/partial class was introduced.
3. Confirm constructors, destructors, inheritance, typed virtual calls,
   allocation type, and ownership agree with Ghidra evidence.
4. Collect concise integrated/blocked reports; do not accept `TRANSCRIBED` as
   progress completion.
5. Update shared documentation/function names and save the Ghidra database.

If a subagent lacks evidence for the model, have it return the exact blocker and
dispatch the constructor/vtable/caller investigation next. Never ask it to
"make it compile for now" with casts.

## Continuous Mode

When asked to work **continuously** (or "keep going", "don't stop", "all remaining"), do NOT stop on your own. Context size is NOT a reason to stop. Keep dispatching reviewed batches until:

- All scoped functions are integrated or have a precise model-evidence blocker, OR
- The user explicitly tells you to stop, OR
- You hit a hard error (Ghidra crash, disk full, MCP server down, etc.)

Between batches, inspect every source/header diff for the rejection patterns in
the quality gate above; a one-line agent summary is not sufficient evidence of
integration. Keep concise model/evidence summaries after review and save the
Ghidra database every 2-3 batches.

### Batch ordering strategy for continuous mode:

1. **First:** Core classes (GameObject, Entity, CGWND) — foundational, everything depends on them
2. **Then:** Major game classes (Building, BuildingComplex, Train, BuildingMgr) — build on core
3. **Then:** Subsystem classes (LOCOBITMAP, UI*, Network*, Audio*) — self-contained subsystems
4. **Finally:** Leaf/utility functions and C free functions

Within each tier, dispatch methods of the same class together (to minimize header merge conflicts), but spread methods of different classes across parallel agents.
