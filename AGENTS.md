# AGENTS.md — Lego Loco Reverse Engineering

## Core philosophy: Assembly-first

**Everything** in this project derives from the assembly code in `loco.exe`. The
Ghidra database is the single source of truth. All C++ code, all struct layouts,
all field names, all function signatures, and all class hierarchies are
reconstructed from the disassembly and decompiler output — never invented.

When in doubt, open Ghidra and look at the actual bytes.

---


## PROGRESS.md — Cross-session progress tracking

`PROGRESS.md` at the repo root is the durable cross-session record of what
has been completed and what remains. It is NOT generated — it is manually
maintained.

### When to update

Update `PROGRESS.md` after any significant milestone:
- A class or subsystem is fully decompiled and validated
- A new shim or tool is created
- A major bug is fixed across multiple files
- A new phase of work begins
- The binary becomes launchable (or reaches a new capability level)

### What to record

- **Completed**: checked-off items with brief descriptions
- **Current status**: metrics table (file counts, compilation status, etc.)
- **Remaining work**: prioritized TODO list with unchecked items
- **Architecture notes**: class hierarchy, key addresses, patterns discovered
- **Session log**: date-stamped summary of each session's accomplishments

### Conventions

- Use `- [x]` for done, `- [ ]` for remaining work
- Keep items concise — one line per task
- Update the session log with date + 1-line summary at end of each session
- Do NOT include transient debugging notes, stack traces, or speculative ideas
- If a remaining-work item is completed, move it to Completed and check it off

### Reading on session start

Always read `PROGRESS.md` at the start of a new session to understand
current state before beginning work.

## Opening the Ghidra database

The Ghidra MCP server provides tools under the `mcp.ghidra.*` namespace.
Use `mcp.ghidra.open_database` and `mcp.ghidra.wait_for_analysis` to
open the binary.

**Always open the raw binary, never the `.gpr` project file** (the project
file fails with "No load spec found").

```
mcp.ghidra.open_database({
  file_path: "/home/user/projects/v43/jenrik/lego-loco-rev-eng/lego-loco-unpacked/Exe/loco.exe",
  database_id: "loco3"   // use a fresh, unique ID every session
})
```

Then wait for analysis to complete:

```
mcp.ghidra.wait_for_analysis({
  database: "loco3"
})
```

**Rules:**
- Never use `force_new: true` — it's blocked by the MCP server.
- Always use a **unique** `database_id` each session (e.g. `loco3`, `loco4`, …).
  Stale IDs from failed opens stay reserved and block reuse.
- The binary is `loco.exe` (x86, 32-bit PE, MSVC 1998).
- After analysis completes, the decompiler is available via
  `mcp.ghidra.decompile_function` and `mcp.ghidra.disassemble_function`.

### Key Ghidra MCP tools

| Tool | Purpose |
|------|---------|
| `mcp.ghidra.open_database` | Open the binary |
| `mcp.ghidra.wait_for_analysis` | Wait for auto-analysis |
| `mcp.ghidra.decompile_function` | Get C decompiler output for a function |
| `mcp.ghidra.disassemble_function` | Get raw disassembly listing |
| `mcp.ghidra.list_functions` | List all functions |
| `mcp.ghidra.get_xrefs_to` | Cross-references to an address |
| `mcp.ghidra.get_xrefs_from` | Cross-references from an address |
| `mcp.ghidra.list_structures` | List defined struct types |
| `mcp.ghidra.get_structure` | Get struct layout |
| `mcp.ghidra.list_names` | List named labels/addresses |
| `mcp.ghidra.get_strings` | List strings in the binary |
| `mcp.ghidra.find_code_by_string` | Find code referencing a string |

---

## How everything derives from assembly

### 1. Function addresses -> C++ methods

Every function in the C++ code is tagged with its original address:

```cpp
/**
 * GameObject::HitTest — Vtable slot [3] override
 * Address: 0x405680
 */
int GameObject::HitTest(uint32_t packedXY)
```

The **address is canonical**. The name (`GameObject::HitTest`) is derived from:
- Which vtable the function appears in (cross-reference the vtable at 0x477488)
- The `this` pointer usage (ECX register = `__thiscall` = member function)
- Field offsets accessed relative to `this`

### 2. Vtable analysis -> class hierarchy

The binary's `.rdata` section contains vtable arrays. Each vtable is a
consecutive block of function pointers. By analyzing which functions appear
in which vtables, we reconstruct the class hierarchy.

Vtable addresses are documented in `shared/vtable_addrs.h` as `#define`
constants with the prefix `VTBL_`. These are **reference only** — the
actual C++ code uses `virtual` methods and lets the compiler manage vtables.

**Example**: Vtable 0x477488 (Entity) has 15 slots. By examining each slot's
target function and its field accesses, we determine the method name and
signature. Slot [3] at 0x405680 accesses PostcardAlbum fields at +0x110,
+0x130, +0x148-0x164 — telling us this method belongs to PostcardAlbum,
not the base Entity.

### 3. Field offsets -> struct layouts

Every field in a C++ class is annotated with its byte offset:

```cpp
int32_t  world_x;           // +0x4C  world X position
uint32_t sound_res_id;      // +0x44  sound resource ID
```

These offsets come from the disassembly: `mov eax, [ecx+0x4C]` means
the field is at offset 0x4C from `this` (ECX).

### 4. Ghidra names -> C++ names

The decompiler assigns working names. We refine them:

| Ghidra name | C++ name | Why |
|-------------|----------|-----|
| `LOCOBITMAP_*` | `PostcardAlbum::*` | The class is PostcardAlbum; LOCOBITMAP was an auto-label |
| `FUN_00405680` | `GameObject::HitTest` | Context from vtable and field accesses |
| `DAT_004851f4` | `g_game_mode` | Global variable at that address |
| `PTR_LAB_00478f88` | `VTBL_*` or actual name | Vtable or function pointer |

The `typedef PostcardAlbum LOCOBITMAP;` in `graphics/LOCOBITMAP.h` exists
solely to bridge decompiler naming artifacts to the correct C++ names.

---



## Running Fabric programs from files

The project includes reusable TypeScript programs under `tools/` that orchestrate
decompilation workflows. These programs use `agents.create()`, `agents.ask()`,
and `agents.run()` to spawn persistent actors and one-shot reviewers.

### Loading programs via eval

`fabric_exec` executes a single TypeScript code block. To run a program stored
in a file without inlining its contents, use the async-IIFE + eval pattern:

```typescript
const code = await pi.read('tools/deprecated/decompile-class.ts');
const wrapped = '(async () => { ' + code + ' })()';
const result = await eval(wrapped);
// result contains { decompileClass, transcribeFunction, ... }
```

The outer async IIFE is required because `eval` in the QuickJS sandbox does
not support top-level `return`. Wrapping in `(async () => { ... })()` gives
the program's `return` statements a function boundary.

Do not commit class-specific runner files or fixed Ghidra database IDs. Invoke the parameterized `decompileClass` library above, or configure a session runner for the current task.

### Program architecture

Fabric programs in this project follow a three-layer pattern:

```
fabric_exec (top-level TypeScript — the orchestrator loop)
  ├─ agents.create(PRIMARY ACTOR)     — persistent, reused across passes
  │     └─ agents.ask(actor, task)    — blocking send/receive
  ├─ agents.run(REVIEWER, schema)     — one-shot, returns structured JSON
  │     └─ result.value.approved      — pure boolean, no regex parsing
  └─ Loop: if (!approved) → template feedback → agents.ask(actor, ...)
```

**Do NOT wrap the program in an extra orchestrator agent.** The TypeScript
loop in `fabric_exec` IS the orchestrator. Spawning an agent whose only job
is to `fabric_exec` the program adds an unnecessary layer. Run the program
directly via `eval` as shown above.

### Key APIs used

| API | Purpose |
|-----|---------|
| `agents.create({...})` | Create a persistent actor that survives across iterations |
| `agents.ask({id, message})` | Send a task to an actor, block until response |
| `agents.run({..., schema})` | Run a one-shot agent that returns structured JSON |
| `agents.remove({id})` | Clean up an actor when done |
| `result.value` | Access structured data from a schema-backed agent run |

### Schema-driven review

The reviewer agent uses a JSON Schema so the orchestrator gets structured,
validated output. No regex parsing of free text:

```typescript
const revResult = await agents.run({
  name: "review-foo",
  task: "Review the code per AGENTS.md...",
  model: "deepseek/deepseek-v4-pro",
  runner: "pi",
  tools: ["read", "grep", "find", "ls"],
  schema: {
    type: "object",
    properties: {
      approved: { type: "boolean" },
      issues: { type: "array", items: { type: "object", ... } },
      compilationStatus: { type: "string", enum: ["PASS", "FAIL", "UNKNOWN"] },
    },
    required: ["approved", "issues", "compilationStatus"],
  },
});

// Pure TypeScript decision — no parsing
if (revResult.value.approved) {
  return { status: "approved" };
}
// Template the structured issues into natural feedback for the primary
const feedback = templateReviewFeedback(revResult.value);
```

### Actor cleanup

Always remove persistent actors when the workflow completes (success, failure,
or max iterations). Leaving actors alive leaks resources:

```typescript
try {
  const result = await decompileClass({...});
} finally {
  await agents.remove({ id: actor.id }).catch(() => {});
}
```


## Correctness and completeness

### What "correct" means

A decompiled function is **correct** when its C++ implementation is behaviorally
equivalent to the original assembly for all possible inputs. This means:

- **Control flow matches basic-block-for-basic-block.** Every branch, loop, and
  goto corresponds exactly to the assembly. No simplified conditions, no merged
  branches, no invented early-returns. When the binary uses a computed goto or
  jump table, the C++ must preserve that structure (or document the
  transformation with a `// NOTE:` comment if a structured equivalent is
  provably identical).

- **Data flow is preserved.** Every field access, every argument passed, every
  return value, every register-based temporary — all represented. The decompiler
  sometimes optimizes away temporaries that the binary actually computes; verify
  they are truly unused before omitting them.

- **Signedness and width are correct.** An unsigned comparison that becomes
  signed changes behavior. MSVC uses `int32_t`/`uint32_t` extensively; verify
  `JA`/`JB` (unsigned) vs `JG`/`JL` (signed) branch instructions in the
  disassembly.

- **Calling conventions are correct.** `__thiscall` (ECX = this), `__fastcall`
  (ECX + EDX), `__stdcall` (callee cleanup), `__cdecl` (caller cleanup). Each
  function uses the right convention. C++ member functions are implicitly
  `__thiscall` — the annotation is unnecessary noise in C++ code.

- **Side effects are preserved.** Global variable writes, indirect calls through
  function pointers, memory allocations, I/O operations — every observable
  effect on program state is represented.

### What "complete" means

A function is **complete** when nothing is left out or deferred:

- [ ] Every basic block in the disassembly appears in the C++ (including error
      paths, edge cases, and "impossible" branches the decompiler may have
      elided)
- [ ] Every call target is identified — decompiled, imported (Win32 API), or
      documented as external with its address
- [ ] Every `this`-relative offset is mapped to a named field in a header (one
      canonical header per class)
- [ ] Every global variable accessed has a named definition in `types.h` (or the
      appropriate class header if it's a static member)
- [ ] All helper functions called by this function are themselves decompiled, or
      documented with `// TODO: decompile 0xADDRESS` if intentionally deferred
- [ ] Cross-references verified: callers' signatures match, callees exist, field
      offsets are consistent across all functions accessing the same class
- [ ] Ghidra auto-label artifacts (`FUN_`, `DAT_`, `RESDATA_`, `GAMESTATE_`,
      `LOCOBITMAP_`, `PTR_LAB_`) replaced with semantic names
- [ ] The file compiles under `make check` with no more than documented,
      intentional warnings

### File status tags

Every `.cpp` and `.h` file carries a status line near the top to communicate
how far through the decompilation pipeline it is:

| Tag | Meaning |
|-----|---------|
| `// Status: TRANSCRIBED` | Cleaned from Ghidra decompiler output, compiles, but **not yet validated** against the disassembly. May contain subtle errors. |
| `// Status: VALIDATED` | Manually compared against the disassembly instruction-by-instruction. All offsets, branches, calls, and data references confirmed correct. Any remaining issues documented with `// NOTE:` or `// DECOMPILER NOTE:` comments. |
| `// Status: INTEGRATED` | Validated **and** wired into the class hierarchy. All `(uint8_t*)this + offset` accesses replaced with named fields. All virtual calls use C++ dispatch. All cross-references consistent. |

Only **INTEGRATED** files are considered done. A file can be TRANSCRIBED for
many sessions before being validated — the tag makes that status explicit so
future sessions know where to focus.

### The decompilation pipeline (multi-pass)

1. **Pass 1 — Transcription.** Open Ghidra, decompile the function, clean up
   MSVC-isms and Ghidra anti-patterns (see § "Ghidra decompiler anti-patterns"),
   get it compiling. Tag `TRANSCRIBED`.

2. **Pass 2 — Validation.** Side-by-side with the disassembly. Verify every
   instruction — each `mov`, `cmp`, `jmp`, `call` accounted for. Confirm all
   field offsets, branch conditions, call targets, and data references. Fix any
   errors found. Tag `VALIDATED`.

3. **Pass 3 — Integration.** Wire into the class hierarchy. Remove temporary
   offset-based accesses. Add virtual method declarations to the class header.
   Ensure field names, types, and signatures are consistent with sibling
   functions. Tag `INTEGRATED`.

### When stubs are acceptable

The "NO stubs" rule (see § "C++ coding rules") has specific, documented
exceptions:

**Acceptable stubs:**

1. **Platform API calls.** `CreateFileA`, `BitBlt`, DirectDraw methods — these
   talk to hardware or the OS. A stub that documents expected behavior and
   returns a sensible default is acceptable. These live in `stubs/` or
   `sdl3_shims/`, never in decompiled class implementations.

2. **Deferred decompilation.** If function A calls function B and B hasn't been
   decompiled yet, a temporary stub for B is acceptable **if and only if**:
   - It is marked `// TODO: decompile 0xADDRESS`
   - It lives in a dedicated stub file (not in the class implementation)
   - It is tracked in PROGRESS.md under "Remaining work"

3. **Compiler-generated helpers.** MSVC's `scalar deleting destructor`,
   `vector deleting destructor`, RTTI, and exception handling machinery are
   compiler-generated — do not decompile them. The C++ compiler regenerates
   equivalent code. Document them in the class header's vtable layout comment
   but do not implement them.

**Not acceptable:**

- Stubbing an internal function without an address annotation
- Stubbing an internal function and never coming back to it (it must appear in
  PROGRESS.md)
- Stubs that silently return 0 when the real function returns nonzero (the stub
  must either fail loudly with `assert(!"stub")` or document the assumption)


---

## Ghidra decompiler anti-patterns

Ghidra decompiles C++ binaries as C. It has no concept of classes, constructors,
virtual dispatch, or inheritance. The decompiler output is full of patterns that
are technically correct (they match the assembly) but structurally wrong for a
C++ codebase. Every one of these must be transformed during Pass 1
(Transcription).

### 1. Constructors and destructors as free functions (`_Ctor` / `_Dtor`)

Ghidra sees a constructor call as a free function that takes a pre-allocated
memory block and returns `this`. It names it `ClassName_Ctor` or `ClassName_Dtor`.

```c
// WRONG — Ghidra output (106 Ctor + 56 Dtor occurrences in codebase)
void* __thiscall Vehicle_Ctor(void* mem, int resource_id, int dir, char flag1, char flag2);
void* __thiscall GAMESTATE_EditorState_Ctor(void* this, char viewport_side);
void* __thiscall RESDATA_GameVehicle_Dtor(void* this, uint8_t free_memory);

// Usage in Ghidra output:
Vehicle* vehicle = (Vehicle*)Vehicle_Ctor(operator_new(0xF0), resource_id, 0, 0, 0);
```

```cpp
// CORRECT — C++ constructor/destructor
class Vehicle : public ScriptedObject {
public:
    Vehicle(int resource_id, int dir, char flag1, char flag2);
    ~Vehicle();
};

// Usage:
Vehicle* vehicle = new Vehicle(resource_id, 0, 0, 0);
```

**Transformation rules:**
- `void*` return → no return type (it's a constructor)
- `_Ctor` → class constructor `ClassName::ClassName(...)`
- `_Dtor` → class destructor `ClassName::~ClassName()`
- The `uint8_t free_memory` parameter on destructors is the MSVC
  scalar-deleting-destructor flag (see §3 below). Remove it.
- The `operator_new(sizeof(Class))` call followed by `_Ctor` becomes
  `new Class(...)`.

### 2. C++ methods as free functions with explicit `this`

Ghidra decompiles every member function as a free function taking the object
pointer as the first argument.

```c
// WRONG — Ghidra output
typedef struct World {                    // defined as POD struct
    int32_t _pad_00;
    int16_t vehicle_count;                // +0x04
    void*   vehicles[4];                  // +0x08
} World;

void   __thiscall World_Init(World* world);              // @ 0x44D30
uint   __thiscall World_SaveToFile(World* world, uint id, char player, char flag);
void   __thiscall World_InvalidateRect(World* world, int x, int y, int w, int h);
int    __thiscall Town_SelectBuilding(void** town_view, int building);
void   __thiscall Town_RenderSelection(void* town_view);
```

```cpp
// CORRECT — C++ class with methods
class World {
    int32_t _pad_00;
    int16_t vehicle_count;                // +0x04
    Vehicle* vehicles[4];                 // +0x08  (typed, not void*)
public:
    void Init();                          // was World_Init
    uint SaveToFile(uint id, char player, char flag);  // was World_SaveToFile
    void InvalidateRect(int x, int y, int w, int h);
};

class Town {
public:
    int  SelectBuilding(int building);
    void RenderSelection();
};
```

**Transformation rules:**
- `typedef struct` → `class` (when the type has a vtable or methods)
- `ClassName_MethodName(ClassName* self, ...)` → `ClassName::MethodName(...)`
  with implicit `this`
- `void**` / `void*` typed parameters → actual type (`Town*`)
- Remove `__thiscall` annotation — implicit in C++

### 3. Scalar deleting destructor

MSVC generates a "scalar deleting destructor" for every class with a virtual
destructor. It calls `~ClassName()` and conditionally calls `operator delete`.
This is a **compiler-generated wrapper** — never user code.

```c
// WRONG — Ghidra decompiles the scalar deleting destructor as the class destructor
void* __thiscall RESDATA_GameVehicle_Dtor(void* this, uint8_t free_memory)
{
    // ... actual destructor body (clean up fields, release resources) ...
    if (free_memory & 1) {
        operator_delete(this);
    }
    return this;
}
```

```cpp
// CORRECT — C++ destructor contains ONLY the cleanup logic
class GameVehicle : public ScriptedObject {
public:
    ~GameVehicle() {
        // ... actual destructor body only (NO operator delete call) ...
        // The compiler emits the delete call when `delete ptr` is used.
    }
};
```

**Transformation rules:**
- Remove the `uint8_t free_memory` parameter
- Remove the `if (free_memory & 1) { operator_delete(this); }` block
- Remove the `return this;` at the end
- The compiler handles deletion automatically
- In the vtable comment, note which slot was the scalar deleting destructor
  (always slot [0] in MSVC) so there's a record of it

### 4. Literal vtable dispatch

Ghidra shows every virtual call as a raw function-pointer dereference through
the vtable.

```cpp
// WRONG — Ghidra output (101 occurrences in codebase)
(*(void (**)(void*, int32_t, int32_t))(*(void**)this->gameobject + 6))(
    *(void**)this->gameobject, 0, -1);
(*(void (**)(void))(*(void**)this->scriptengine + 15))();
int32_t* result = (int32_t*)(*(int32_t* (**)(void*, int32_t, int32_t))(*(void**)this + 6))(
    this, 0x2402, -1);
```

```cpp
// CORRECT — C++ virtual dispatch
this->gameobject->SetAnimState(0, -1);    // was vtable[6], 2 params
this->scriptengine->Cleanup();            // was vtable[15], 0 params
int32_t* result = this->Init(0x2402, -1); // was vtable[6], 2 params returning int32_t*
```

**Transformation rules:**
- Trace the vtable slot number → method name from the class header's vtable
  layout comment
- Determine the method signature from the cast's function pointer type
- Replace with a virtual method call
- If the method doesn't exist in the class yet, add it to the header as a
  virtual method with the correct signature

### 5. `__thiscall` and `__fastcall` annotations on declarations

Ghidra preserves MSVC calling conventions. In C++ these are either implicit or
handled by the compiler. Remove them from declarations — they're noise. Keep
them only in `stubs/compat.h` where the macros are defined.

```c
// WRONG — noise on every declaration (101 occurrences)
void  __thiscall Town_RenderSelection(void* town_view);
int   __thiscall RESDATA_IsRoadTile(int ptr);
void  __thiscall UI_SetTooltipText(void* mgr, int x, int y, const char* text);
```

```cpp
// CORRECT — clean C++ declarations
void  Town::RenderSelection();
int   TileData::IsRoadTile();
void  TooltipManager::SetTooltipText(int x, int y, const char* text);
```

### 6. Ghidra auto-label prefixes

Ghidra auto-names functions and data based on nearby strings or data references.
These labels are artifacts of automated analysis and must be replaced with
semantic names.

| Ghidra prefix | Meaning | Replace with |
|---------------|---------|-------------|
| `FUN_XXXXXXXX` | Function at that address | `ClassName::MethodName` |
| `DAT_XXXXXXXX` | Global data at that address | `g_descriptive_name` in `types.h` |
| `PTR_LAB_XXXXXXXX` | Function pointer or vtable | `VTBL_*` (doc only) or actual name |
| `RESDATA_*` | Auto-labeled from .rdata section | Actual class name (`ScriptedObject::`, `TileData::`, etc.) |
| `GAMESTATE_*` | Auto-labeled from game state data | `EditorState::` |
| `LOCOBITMAP_*` | Auto-labeled from bitmap data | `PostcardAlbum::` |
| `RESMGR_*` | Auto-labeled from resource manager | `ResourceManager::` or specific resource class name |

Example:
```c
// WRONG — Ghidra auto-labels
extern void  __thiscall RESDATA_ScriptedObject_Dispatch(void* obj, int x, int y, int msg);
extern int   __thiscall RESDATA_IsRoadTile(int ptr);
extern void* __thiscall RESMGR_SoundObject_Ctor(void* this, int param_2, int param_3, char param_5);
```

```cpp
// CORRECT — semantic names
void  ScriptedObject::Dispatch(int x, int y, int msg);
int   TileData::IsRoadTile();
SoundObject::SoundObject(int param_2, int param_3, char param_5);
```

### 7. `extern "C"` wrapping for C++ methods

Decompiled files often declare C++ constructors and methods inside `extern "C"`
blocks. Remove the C linkage — these are C++ symbols.

```cpp
// WRONG — C++ methods with C linkage
extern "C" {
    void* ButtonSprite_Ctor(void* obj, int res_id);     // C++ constructor
    void  UI_WindowBase_Hide(void* self);                // C++ method
    void  UI_ChildWindow_Dtor(void* self);               // C++ destructor
}
```

```cpp
// CORRECT — C++ declarations (no extern "C")
class ButtonSprite {
public:
    ButtonSprite(int res_id);
};
class UI_WindowBase {
public:
    void Hide();
};
// Only Win32 API imports stay in extern "C":
extern "C" {
    HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int,
                         HWND, HMENU, HINSTANCE, void*);
}
```

### 8. `void*` fields instead of typed pointers

Ghidra types most pointers as `void*`. When the pointed-to type is known (from
context, allocation sites, or field usage), use the actual type.

```c
// WRONG — untyped pointers obscure intent
typedef struct World {
    void* vehicles[4];          // what type?
    void* sub_objects[16];      // what type?
} World;
extern void* g_building_mgr;    // BuildingMgr*
extern void* g_town_view;       // Town*
```

```cpp
// CORRECT — typed pointers
class World {
    Vehicle* vehicles[4];
    GameObject* sub_objects[16];
};
extern BuildingMgr* g_building_mgr;
extern Town* g_town_view;
```

### 9. `param_1`, `param_2` generic parameter names

Ghidra uses numbered parameter names when it can't recover the original.
Replace with descriptive names based on how the parameter is used.

```c
// WRONG — meaningless names
void* __thiscall RESDATA_GameVehicle_Ctor(void* self, int32_t param_1);
void* __thiscall CollisionData_Dtor(void* this, uint8_t param_1);
```

```cpp
// CORRECT — descriptive names
GameVehicle::GameVehicle(int32_t resource_id);
CollisionData::~CollisionData();
// (param_1 was the scalar-deleting-dtor flag — removed)
```

### 10. Inheritance as flat struct

Ghidra shows the complete object layout as a single flat struct with all base
class fields inlined. In C++, use actual inheritance.

```c
// WRONG — flat struct (Town example: UI_WindowBase fields + Town fields
typedef struct Town {
    // 0xE8 bytes of UI_WindowBase fields listed inline...
    uint8_t  base_padding[0xE8];
    // Town-specific fields:
    uint8_t  field_E8;
    // ...
} Town;
```

```cpp
// CORRECT — inheritance
class Town : public UI_WindowBase {
    // Only Town-specific fields (from +0xE8 onward):
    uint8_t  field_E8;
    // ...
};
```

### 11. Array fields expanded as consecutive scalar fields

Ghidra sometimes shows consecutive same-type fields as separate items rather
than an array.

```c
// WRONG — separate fields that are really an array
int32_t occupantTracks_0;    // +0x38
int32_t occupantTracks_1;    // +0x3C
int32_t occupantTracks_2;    // +0x40
// ... 8 total
```

```cpp
// CORRECT — array
int32_t occupantTracks[8];   // +0x38
```

**Detection rule:** When 3+ consecutive fields have the same type and are
accessed with computed indices in the disassembly (`[ecx + eax*4 + 0x38]`),
they are an array.

### 12. Constructor return type `void*`

MSVC constructors return `this` in EAX. Ghidra preserves this as `void*` return
type. Standard C++ constructors have no return type.

```c
// WRONG — constructor with return type
void* __thiscall Vehicle_Ctor(void* mem, int resource_id, int dir, char f1, char f2)
{
    *(void**)mem = VTBL_VEHICLE;     // also wrong — see §4
    mem->field_04 = resource_id;
    // ...
    return mem;
}
```

```cpp
// CORRECT — constructor has no return type, no vtable assignment
Vehicle::Vehicle(int resource_id, int dir, char f1, char f2)
{
    // compiler sets vtable automatically
    this->field_04 = resource_id;
    // ...
}
```

### Summary: What to check for in every file

After transcribing a file from Ghidra, verify none of these remain:

- [ ] No `_Ctor` / `_Dtor` free functions → use constructors/destructors
- [ ] No `ClassName_MethodName` free functions → use `ClassName::MethodName`
- [ ] No `*(uint8_t*)this + N` → use named fields
- [ ] No `*(void**)this = VTBL_...` → compiler manages vtables
- [ ] No `(*(void (**)(...))(*(void**)this + N))(...)` → virtual method calls
- [ ] No `__thiscall` / `__fastcall` on C++ declarations
- [ ] No `RESDATA_` / `GAMESTATE_` / `LOCOBITMAP_` / `FUN_` / `DAT_` labels
- [ ] No `extern "C"` around C++ constructors, destructors, or methods
- [ ] No `void*` fields where the type is known
- [ ] No `param_1`, `param_2` parameter names
- [ ] No `typedef struct` for types with vtables → use `class` with inheritance
- [ ] No `void*` return type on constructors
- [ ] No scalar-deleting-destructor `free_memory` flag in destructors
## C++ coding rules

### NO literal vtable access

The decompiler produces code like:

```cpp
// WRONG — decompiler artifact, do not write this
*(void**)this = (void*)VTBL_GAME;
((void(*)(void*, int))this->vtable[7])(this, 0);
```

**Instead**, use C++ virtual methods:

```cpp
// CORRECT — the compiler manages the vtable
class Game : public Entity {
public:
    virtual ~Game();
    void SetAnimState(int state) override;   // was vtable[7]
};
```

The `VTBL_*` constants in `shared/vtable_addrs.h` are **documentation only**.
They record what the binary contains. They are never used to manually set or
read vtable pointers in C++ code. The sole exception: files that have not yet
been refactored to use virtual dispatch may reference `VTBL_*` in code as
a temporary measure; these are tracked under `[VTBL]` in `make check`.

### NO raw pointer arithmetic for field access

The decompiler produces code like:

```cpp
// WRONG — raw offset math
int x = *(int*)((uint8_t*)this + 0x4C);
void* res = *(void**)((uint8_t*)building + 0x40);
```

**Instead**, use named struct/class member access:

```cpp
// CORRECT — named fields
int x = this->world_x;
void* res = building->resource;
```

**Exception**: When accessing a field through a base class pointer that does
not declare the field (cross-cast within a class hierarchy), a brief
offset-based access with a comment is acceptable as an intermediate step:

```cpp
// Acceptable intermediate form — field is in PostcardAlbum at +0x110
if (*(uint8_t*)((uint8_t*)this + 0x110) != 0) return 0;
// TODO: move this method to PostcardAlbum where 'destroyed' is a named field
```

### NO MSVC-specific syntax

The decompiler produces MSVC-isms that don't compile under GCC:

```cpp
// WRONG
(void*)this->field = nullptr;     // cast-to-lvalue
void __fastcall ClassName();       // constructor with return type
extern "C" { void operator_new(); }  // C++ operator in C linkage
```

**Fix each pattern**:
- **Cast-to-lvalue**: `this->field = 0;` (the field already has a proper type)
- **Constructor return type**: Remove `void`, constructors have no return type
- **C-linkage conflicts**: Move `operator_new`, `GLOBAL_free`, and C++ class
  members out of `extern "C"` blocks. Windows API imports (kernel32, user32)
  may remain in `extern "C"`.

### NO extern "C" wrapping for C++ symbols

Functions like `operator_new`, `GLOBAL_free`, `GameObject_Update`, and
global variables like `g_game_time` are **C++ symbols** — they have C++
linkage. They must not be declared inside `extern "C" { }` blocks.

Global variables declared in `shared/types.h` with C++ linkage MUST NOT be
re-declared with C linkage in `.cpp` files. If a `.cpp` re-declares a
global that is already in `types.h`, remove the duplicate declaration
rather than changing its linkage.

### NO stubs

Functions should be fully decompiled including the body. Leaving a stub or only
a function signature with a minimal return value is an incomplete result and
requires future work before it can be declared done.

**See § "When stubs are acceptable" above for documented exceptions.**
In brief: platform API stubs, temporary `// TODO: decompile` stubs for deferred
functions, and compiler-generated helpers (scalar deleting destructor, RTTI,
exception handling) are acceptable. Internal logic stubs with no plan to
decompile are not.

### Struct definitions live in headers

Each class/struct has ONE canonical header with its complete field layout.
No partial or duplicate definitions across files. If a file needs to access
fields of a class, it includes that class's header.

### Every method is traceable to an address

Every function implementation must document its original address in the binary:

```cpp
/**
 * GameObject::Draw — Single-frame sprite blit (vtable[11])
 * Address: 0x405E60
 */
void GameObject::Draw()
```

If a method is split across multiple files or has helper functions, each
piece must carry its own address annotation.

---

## Compilation and checking

The project uses a Makefile in `src/decompiled_cpp/`:

```bash
cd src/decompiled_cpp
make          # compile known-good files
make check    # show per-file compilation status
make all      # attempt all files
```

The build uses `-include stubs/compat.h` to provide:
- Standard library headers
- MSVC calling convention shims (`__thiscall`, `__fastcall`, `__stdcall`)
- Windows API type stubs (`HWND`, `HINSTANCE`, `DWORD`, `HKEY`, …)

**Never add real Windows headers** — the project is compiled on Linux with
GCC. All Windows types are stubbed.

---

## File organization

```
src/decompiled_cpp/
├── shared/           # types.h, vtable_addrs.h, Collection, Timer
├── stubs/            # compat.h, windows.h, ddraw.h, dsound.h, dplay.h
├── core/             # GameObject, Entity, Game, CGWND, BuildingMgr
├── game/             # Building, Vehicle, Train, World, TrackPiece, Panel
├── graphics/         # DDRAW, LOCOBITMAP/PostcardAlbum, PixelDataCache
├── audio/            # AudioChannel, GameAudio
├── input/            # Cursor, Cursor_Editor, Cursor_Render
├── network/          # DirectPlay, Netman, DPlayManager, NetworkPlayerList
├── resources/        # ResourceManager, AssetMgr, StreamObject
├── town/             # Town, TownTiles
├── ui/               # EditWindow, GameWindow, HelpWnd, UIPANEL, UI_*
├── world/            # scriptengine, tilemap
└── Makefile
```

Headers live alongside their `.cpp` files. The `shared/` directory holds
cross-cutting types and utilities. The `stubs/` directory holds platform
abstraction headers (never real Windows SDK headers).

---

## Ghidra database location

```
./lego-loco-unpacked/Exe/ghidra_projects/
```

The `.gpr` project file is **not usable directly** — always open the raw
`loco.exe` binary through the MCP as described above.
