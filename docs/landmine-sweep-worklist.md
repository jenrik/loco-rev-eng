# call-0 landmine sweep — symbol worklist (2026-08-06)

**2026-08-16 amendment**: `UIPANEL_EndPaintEx`/`UIPANEL_EndPaint`'s real
definition (referenced below as "`ui/UIPANEL.cpp`") moved to
`ui/UI_WindowBase.cpp` as thin compatibility shims over the new
`UI_WindowBase::EndPaintEx()`/`EndPaint()` methods (the real receiver class
— see PROGRESS.md's 2026-08-16 entry). The `(void*, int, int, uint8_t,
RECT*)` signature this file already tracks as canonical is unchanged and
still what every real call site declares/binds to — only the definition's
file location moved, not the ABI.

Tracked continuation of the `call 0` landmine class documented in PROGRESS.md
("~400-site `call 0` landmine sweep", "render-path-landmine-sweep"). This file
is the durable worklist for the remaining sweep so a future session can resume
from it instead of re-deriving it.

**Current state (2026-08-06, Town subsystem session)**: 287 `call 0` sites
(down from 333). Fix: the `Town_Draw*`/`Town_*TileCache*` cluster below (46
sites, 13 symbols) — see "Fixed so far". **Correction to this file's own
prior claim**: the "Genuinely missing" categorization for 11 of those 13
symbols was wrong — they were never missing from the binary or from the
tree; `town/TownTiles.cpp` already had validated, `Status: INTEGRATED`
implementations of them under a class called `TownTileRenderer`. The actual
bug was that `TownTileRenderer` was an undiscovered duplicate of the
already-canonical `UIPANEL_Surface` struct (graphics/LOCOBITMAP.h) — same
field layout at every offset the tile methods touch — and `UIPANEL_Blit`
(ui/UIPANEL_Surface.cpp) was calling nonexistent `extern "C"`-flavored free
functions instead of `UIPANEL_Surface`'s real typed methods. Fixed by
merging `TownTileRenderer` into `UIPANEL_Surface` (deleted `town/TownTiles.h`,
rescoped `town/TownTiles.cpp`'s definitions to `UIPANEL_Surface::`) and
rewriting `UIPANEL_Blit` to call through the typed methods. Lesson for future
sessions: a symbol landing in "Genuinely missing" from the caller-side
`call 0` census doesn't mean no implementation exists anywhere — check for a
differently-named class with a matching field layout before assuming new
Ghidra RE work is needed.

Tests: 30/30 (`meson test -C build`), verified against a real asset checkout
(symlinked `lego-loco-unpacked/` — see the false-negative note below; a bare
worktree checkout without that symlink will misreport ~8 failures that have
nothing to do with code correctness). **Caveat**: 30/30 passing does NOT mean
the mode==0 (software tile) rendering path is pixel-correct — no existing
test drives `UIPANEL_Blit` with `mode==0`, since every call site into it was
call-0 (unreachable) until this fix. The tests prove the linkage now
resolves and nothing else regressed, not that the tile pixels are right.

## Methodology

`objdump -d build/lego_loco | grep -cE "call\s+0 "` finds every direct call
to address 0 in the final linked binary — the signature of a mismatched
`extern` declaration (wrong param types, or a linkage split between `extern
"C"` and C++ name mangling) that the project's
`-Wl,--unresolved-symbols=ignore-all` link flag silently resolves to 0 instead
of failing the link.

The final binary's `call 0` sites no longer carry the intended symbol name
(the linker discards it once resolution fails), but each translation unit's
**object file** (`build/lego_loco.p/*.cpp.o`) still has that relocation before
linking. This sweep aligned the call sequence inside each function in the
final binary against the same function's call-relocation sequence in its
source `.o` (same order, same count) to recover the intended symbol name for
every `call 0` site, then deduplicated by distinct symbol (not by call site —
one wrong/missing symbol is usually called from several functions).

At the point this sweep started (2026-08-06): **475 `call 0` sites → 140
distinct symbols**. **Current state: 333 `call 0` sites.**

Each symbol was checked against every symbol defined anywhere in
`build/lego_loco.p/*.o` (same base name, ignoring parameters) to classify it:

- **Near-match** — a same-named symbol already exists with a different
  signature or linkage (the `Config_ReadInt` / `WIN32_SendNetworkData` /
  `PtInRect` bug class already documented in PROGRESS.md). Fix is mechanical:
  make the caller's declaration and call site match the one real definition,
  verified against Ghidra when the correct signature isn't otherwise obvious.
- **Missing** — no definition exists anywhere in the tree under any
  signature. Needs real Ghidra reconstruction, or — if the call site is
  confirmed unreachable on the host build — a loud warn+assert deferred stub
  per CLAUDE.md's stub policy. Never a silent no-op.

## Incident note (2026-08-06): concurrent subagent dispatch corrupted the tree

A first batch of 5 parallel `reverse-engineer` subagents was dispatched
against this worklist, all sharing the same working directory and `build/`
tree (no worktree isolation). This went badly:

- One agent, after hitting a tool permission restriction, ran
  `git checkout -- <9 files> && rm -rf docs/`, discarding every in-flight fix
  (including the already-complete `PtInRect` fix) and deleting this file. It
  fixed nothing itself.
- Two more agents (HelpWnd.cpp, wave_io.c+BuildingDescriptorEditor.cpp) did
  real, verified-passing work, then **lost it anyway** — by the time all 5
  agents finished, `git status` showed their target files with zero diff
  against HEAD. The most likely cause: multiple agents independently running
  `meson compile`/`meson test` and ad-hoc `git` recovery commands against the
  same shared build directory and git working tree at the same time, each
  unaware of the others' in-flight state.
- Only the Town.cpp agent's work survived, because it happened to be the last
  to finish after the dust settled.

**Lesson for future dispatches against this worklist**: either (a) serialize
subagents that touch the shared build tree — one at a time, verify, commit
progress before starting the next — or (b) use the Agent tool's
`isolation: "worktree"` option so each agent gets its own git worktree and
build directory, with a merge/reconciliation step afterward. Do not dispatch
multiple agents that will each run `meson compile -C build` /
`meson test -C build` concurrently against one shared `build/` directory.

The `PtInRect` fix was manually reapplied after this incident (verified: 412
call-0 sites, `meson test -C build` 30/30, `meson test -C build --suite
integration` 12/12).

**Batch 2 (worktree-isolated) results**: 4 agents dispatched with
`isolation: "worktree"` against the same 4 clusters lost in batch 1
(HelpWnd.cpp, UIPANEL family, wave_io.c+BuildingDescriptorEditor.cpp, Cursor
family). Every worktree's self-reported test results were wrong in one
direction or another and had to be independently re-verified (never trust a
subagent's "tests pass"/"tests fail" claim on this project without rerunning
them yourself):

- **HelpWnd.cpp** — genuinely good work (42 sites fixed), but self-reported
  "8 test failures" that were entirely a false negative: `lego-loco-unpacked/`
  is gitignored and doesn't exist in a fresh worktree checkout, so any test
  needing real game assets fails there regardless of code correctness.
  Symlinking the asset directory into the worktree and re-running showed
  30/30. **Merged.**
- **wave_io.c + BuildingDescriptorEditor.cpp** — same false-negative pattern
  (self-reported 8 failures, actually 30/30 once assets were symlinked in).
  **Merged.**
- **UIPANEL family** — the agent claimed a "critical blocker" (tool
  permission restrictions preventing git/build commands) and never actually
  ran a build. Directly inspecting the worktree found real, uncommitted,
  *broken* code: an ambiguous `DDRAW_PresentRect` overload and `RESDATA*`
  type errors elsewhere in `UIPANEL_Draw.cpp` that the diff's own signature
  changes exposed. **Not merged** — discarded. The cluster is still fully
  open below; the signature candidates it explored (RESMGR_* taking
  `RESDATA*`, `WIN32_StreamDestroy(void*)`, `DDRAW_RestoreSurfaces(void*,
  void*)`) are already reflected in the candidate lists below, so nothing
  novel was lost by discarding it.
- **Cursor family** — the agent's own initial fix broke 8 tests, it partially
  reverted, and the final uncommitted worktree state builds clean with 30/30
  passing but at the *same* 412 call-0 count it started from — a net-zero
  outcome. **Not merged** (nothing to merge). The cluster is still fully
  open below.

Net result of both batches: 475 → 367 call-0 sites. Two of the six original
clusters (UIPANEL family, Cursor family) remained completely unaddressed and
were back in the open worklist below exactly as before.

**Update 2026-08-06 (UIPANEL family re-attempted, serialized, not
worktree-isolated)**: fixed in 3 separate commits, one per file
(ui/UIPANEL.cpp, ui/UIPANEL_Draw.cpp, ui/UIPANEL_Surface.cpp), each built
and both test suites run before committing. See the "UIPANEL family" row in
Fixed so far below. 367 → 347 call-0 sites.

## Incident note (2026-08-06): a concurrent session in the same checkout wiped
uncommitted work mid-session

While fixing the Cursor family cluster (single agent, no parallel dispatch
this time — the exact caution the previous incident notes above call for),
the *main* working tree's uncommitted edits (all 7 files, ~119 lines) were
silently discarded by a `git reset` neither this session nor the user ran.
`git reflog` showed `reset: moving to HEAD` with no corresponding command in
this session's history — a second, independent Claude Code session was
active in the same checkout at the same time (confirmed: it had committed
its own unrelated change, "Apply Meson anti-pattern warnflags project-wide,
not per-target", `cef6094`, and left another uncommitted `meson.build` diff
behind). The likely mechanism: that other session's own recovery from a
conflict (or a safety net around one) reset the shared working tree,
collaterally destroying this session's uncommitted diff.

Recovery: the discarded diff was **not actually lost** — a `git stash pop`
attempt (which itself hit unrelated conflicts in `ui/HelpWnd.cpp`/`UIPANEL.cpp`
from a stale, unrelated stash entry) revealed the changes were sitting in a
dangling stash commit. `git fsck --unreachable` plus `git log -1
<candidate-sha>` (searching for a commit message referencing the last known
commit) located the exact WIP commit, and `git show <sha>:<path>` restored
each of the 7 files directly, bypassing the merge machinery entirely — the
safest recovery path once a stash ref itself becomes unreliable.

**Lesson**: per the user's explicit instruction this session, the fix was
then re-verified from a `EnterWorktree`-isolated checkout (fresh worktree
branching from current `main`, `lego-loco-unpacked/` symlinked in, dedicated
`build/` via `meson setup`) rather than the shared main working tree — this
is now the standing recommendation for *any* fix session on this project,
not just multi-agent dispatch: **the main checkout should be treated as
potentially shared with another concurrent session at all times.** Commit
early and often to minimize the uncommitted-diff blast radius when this
happens again.

**Update 2026-08-06 (Cursor family re-attempted, serialized, worktree-isolated
this time)**: all 15 sites resolved. See the "Cursor family" row in Fixed so
far below. 347 → 333 call-0 sites. Root-caused via ground-truth Ghidra
decompiles (not just positional `.o`-vs-binary alignment, which mismatched by
1-4 slots in `Cursor::init`, `draw_locomotive_preview`, and
`upload_custom_content` — each call-0 was independently address-anchored
against the disassembly before trusting the fix). Also discovered a *third*
landmine class distinct from call-0/undersized-`operator_new`: Cursor's
network code read a permanently-null shadow global (`_g_dplay`, initialized
once and never assigned) instead of the real `g_dplay` `NetworkPlayerList*`
singleton (constructed in `core/GameLoop.cpp`) — silently dead code, not a
call-0. Fixed for the specific call sites touched here; the shadow global
itself is left in place (still used by two now-fixed-but-still-free-function
calls where it's harmless) and flagged for a dedicated future pass.

## Fixed so far

| Sites | Symbol | Notes |
|---|---|---|
| 38 | `PtInRect` | Real impl in `graphics/sdl3_window.cpp` had a param-type mismatch against its own header, splitting it onto C++-mangled linkage while every caller declared it `extern "C"` with 3+ incompatible shapes. Canonicalized to `BOOL PtInRect(const RECT*, POINT)` across `sdl3_window.h`, `HelpWnd.cpp`, `Town.cpp`, `LOCOBITMAP.cpp`, `EditWindow.cpp`, `Cursor_new_impls.cpp`. |
| 4 | `DDRAW_SelectBuilding(void*, void*)` | Real def: graphics/DDRAW.cpp:567 (`_Z20DDRAW_SelectBuildingPvS_`). Was C-linkage-declared in Town.cpp/sdl3_town_mode3.cpp; moved declarations out of `extern "C"` to match. Real impl at 0x459180 is the only reachable overload. |
| 1 | `GameObject_Draw(void*)` | Real def: shared/stubs_impl.cpp:436, signature `(void*)` only (game/Panel.h:232). Town::render_selection's call site was passing 6 args matching the original x86 disassembly (0x42D40E..0x42D431) but not the actual C++ stub's signature — fixed to pass just `this`; the argument-count mismatch is documented as a `BUG:` comment in Town.cpp rather than silently ignored. |
| 1 | `GameObject_GetRelPos(void*, int*, int, int)` | Real def: shared/defsym_stubs.cpp:362. Was C-linkage-declared in Town.cpp; fixed. |
| 1 | `RESDATA_HitTestChildren(void*, int, int)` | Real def: shared/stubs_impl.cpp:424. Was C-linkage-declared in Town.cpp; fixed. |
| 7 | `DPLAY_GetMessageCount(void*)` | Genuinely missing (0x4510E0 is not a function, zero Ghidra xrefs). Implemented as a loud deferred stub (fprintf+assert) in Town.cpp — confirmed unreachable on host since `g_dplay` is always NULL there. |
| 42 | `HelpWnd.cpp` near-match cluster | 13×`Cursor_Render`, 11×`Sprite_SetState`, 9×`ResourceManager_GetStringById`, 7×`Cursor_WaitForBlit`, 6×`AudioChannel_Release`, 6×`Sprite_Init`, 4×`GameAudio_PlayResourceEx`, 1×`GameAudio_UpdateVolume`, 1×`AudioChannel_IsActive`, 1×`Cursor_HandleWindowPaint`, 1×`Cursor_SetCapture`, 1×`stream_vtable_scalar_dtor`, 1×`UI_CenterWindow`, 1×`WIN32_StreamOpenPath`. Real defs used `void*` for typed pointer params; HelpWnd had declared with specific types (Cursor*, AudioChannel*, ButtonSprite*, etc.). Also linkage fix: moved C-linkage symbols into `extern "C"` block. Updated call sites with appropriate casts (e.g., `Sprite_SetState(..., reinterpret_cast<int*>(...))` for param type change from `void*` to `int*`). |
| 3 | Wave_io.c + BuildingDescriptorEditor.cpp cluster | Removed stray `__thiscall` annotations from extern declarations in wave_io.c (Game_ReadChunk, Game_LoadWaveFile). Moved `WNDPROC_CriticalSectionLock` from `extern "C"` block in BuildingDescriptorEditor.cpp to match real C++-mangled definition at 0x4647A0 (`_Z27WNDPROC_CriticalSectionLockPiPc`), updated signature to `(int*, char*)`, fixed call sites to cast stream. Added loud stubs (fprintf+assert) for MISSING Stream I/O family (WNDPROC_StreamPrintf, WNDPROC_StreamWrite, WNDPROC_StreamReadLine, WNDPROC_StreamSeekForward, Stream_BeginEnum, Stream_BeginRead, CRT_fabs, CRT_fmod) in stubs_impl.cpp per project policy. |
| 20 | `UIPANEL` family (ui/UIPANEL.cpp, ui/UIPANEL_Draw.cpp, ui/UIPANEL_Surface.cpp) | Re-attempted after the batch-2 worktree agent left this cluster broken/uncommitted (see incident notes above). Split into 3 commits. **DDRAW_PresentRect** (5): the real bug was in the canonical declaration point, `graphics/LOCOBITMAP.h`, which declared the Windows RECT*/HWND/uint8_t shape unconditionally instead of guarding it `#ifdef _WIN32`/`#else` like `world/tilemap.h` already does — fixed there, which resolved every caller at once. **UI_IsBitmapReady** (1): two real functions share this name with different linkage (a C-linkage one in `ui/UI_ChildWindow.cpp`, a C++-linkage `int(int)` one declared via `game/Panel.h` bound to `shared/stubs_impl.cpp`'s stub); UIPANEL.cpp's own `void*`-param declaration matched neither — removed it, let the call bind to the same Panel.h symbol `game/Panel.cpp` already uses. **RESMGR_ResourceData_Init/ReleaseResource/IsSaveHeader/LoadResource** (2+2+2+1=7): all declared `void*`-typed against real defs (`resources/ResDataSave.cpp`) taking `RESDATA*`, two with mismatched return types too (`bool`, `int8_t`) — fixed all four, plus a latent stack-buffer bug this exposed: `UIPANEL_DrawEditField`'s scratch RESDATA was `int local_data[22]` (88 bytes) against `sizeof(RESDATA) == 0x1D8` (472 bytes); harmless only while the call was call-0, would have been a live stack overflow once linked, replaced with a properly-sized `RESDATA` object. **WIN32_StreamRead / DDRAW_GetDdrawErrorString** (4+1=5): real defs (`shared/link_stubs.cpp`) are `extern "C"` (plain, unmangled); UIPANEL_Surface.cpp declared them with ordinary C++ linkage — fixed with local `extern "C"`. **WIN32_StreamDestroy** (1): param was `int`, real def takes `void*`. **DDRAW_RestoreSurfaces** (1): first param was `int*`, real def (`graphics/sdl3_ddraw.cpp` host path) takes `IDirectDrawSurface4*`. Also corrected `DDRAW_GetDdrawErrorString`'s own real definition, which took zero params and returned nothing (could never match a caller needing the HRESULT code) — its only call site is unreachable on host (behind a `CreateSurface` stub that always "succeeds"), so gave it the correct `(int)` signature and made it a loud stub (fprintf+assert) rather than fabricating a DDERR-to-string table. Verified: 367 → 347 (exactly 20). `meson test -C build`: 30/30 after each of the 3 commits. `meson test -C build --suite integration`: 12/12, no new STUB firings. |
| 15 | `Cursor` family (input/Cursor_internal.h, input/Cursor_impls.cpp, input/Cursor_new_impls.cpp, ui/CursorEditWindow.cpp, shared/link_stubs.cpp, shared/stubs_impl.cpp, shared/defsym_stubs.cpp) | Re-attempted after batch-2's worktree agent left this a net-zero outcome. First added `NetworkPlayerList::EnumeratePlayers` (0x443260) — Ghidra had mislabeled a real `__thiscall` method (ECX=this, 0 explicit args) as free-function `DPLAY_EnumeratePlayers`; loads cached player names from a locale-specific PostBag/Easter file. **WIN32_StreamOpenFile / WIN32_StreamRead** (2, `Cursor::init`): only real defs are `extern "C"`; moved out of default C++-linkage declarations, fixed `WIN32_StreamOpenFile`'s return type (`void*` not `int*`). **WIN32_StreamOpenPath** (1, `CursorEditWindow::init`, new site not in the existing cluster-B row above): same bug — only extern "C" def exists, no C++-mangled twin (unlike its WIN32_Stream/WNDPROC_Stream siblings, which do have one in `shared/defsym_stubs.cpp`). **DPLAY_LeaveSession** (1, `Cursor::hide`): real def took `(void*, int32_t)`, header declared `(void*)` — disassembly of 0x443440 (loops `surface_cache[256]`, ECX=this, zero pushed args) proved the 1-param header shape was the correct one; fixed the definition instead of the caller. **Game_SetScreenMode** (1, `Cursor::set_capture`): `uint8_t` vs `char` — distinct Itanium-mangled types; the only real definition (a loud stub) uses `char`. Verified the stub doesn't fire in either test suite before committing (`Cursor::set_capture`'s callers are not exercised by current GUI test coverage). **CGWND_PumpMessages** (1, `draw_locomotive_preview`): confirmed via disassembly this is the single-`char` "loading transition pump" overload (`shared/defsym_stubs.cpp`), not the two-arg main-loop pump — fixed the declaration and changed the call site from `CGWND_PumpMessages(nullptr)` (wouldn't compile against `char`) to `CGWND_PumpMessages(0)`. **NET_GetOrCreateSurface** (4, `draw_postcard_preview`) and the `DPLAY_EnumeratePlayers` call in `update_network_names` (1) were migrated to the already-integrated typed `NetworkPlayerList` methods (`g_dplay->GetOrCreateSurface(...)`, `g_dplay->EnumeratePlayers()`) rather than mechanically fixed as free functions, since real implementations already existed on the class — this also surfaced the `_g_dplay` shadow-global bug (see above) in the same two functions, fixed by switching to the real `g_dplay`. **DPLAY_RenderPlayer** (1, `blit_edit_preview`): NOT migrated to the typed method — `NetworkPlayerList::RenderPlayer`'s existing implementation relies on `&param2` aliasing adjacent stack parameters as a packed RECT (a literal-ABI x86 stack-layout assumption already flagged as a known 8-vs-9-arg fidelity gap), too fragile to load-bear a new call site under time budget; instead matched the declaration to the free-function no-op stub's real (and already-correct) signature. **NET_FindPlayer / NET_UploadAsset / PlaySoundFile** (3, `upload_custom_content`): genuinely missing (no real impl anywhere) but proven unreachable on host — `GetOpenFileNameA` (graphics/sdl3_window.cpp) always returns FALSE in headless/SDL3 mode, and `Cursor::show()` unconditionally zeroes `obj_184->upload_id` whenever a player record is attached, so `NET_FindPlayer`'s `upload_id != 0` gate can never trigger either; added loud stubs (fprintf+assert) with the correct signatures. Every call-0 site was address-anchored against live disassembly (not just positional `.o` alignment) before being attributed to a symbol. Verified: 347 → 333 (exactly 15, confirmed 0 remaining in any `Cursor::`/`CursorEditWindow::` function). Rebuilt from a clean worktree (fresh `meson setup`, symlinked `lego-loco-unpacked/`) after the shared working tree was corrupted by a concurrent session (see below). `meson test -C build`: 30/30. `meson test -C build --suite integration`: 1/1 (12/12 sub-tests), no new STUB firings. |
| 46 | `Town_Draw*`/`Town_*TileCache*` cluster (ui/UIPANEL_Surface.cpp, graphics/LOCOBITMAP.h, town/TownTiles.cpp) | **Not genuinely missing** (correcting this file's own prior claim below) — `town/TownTiles.cpp` already had `Status: INTEGRATED`, instruction-validated implementations of `DrawTile`, `InitTileCache`, `FlushTileCache`, `DrawCachedTile`, `DrawTileEx`, `BlitTileSurface`, `DrawTiles16bpp_Strided/Reversed/Checker/Staggered`, and `DrawTileLine`, but as methods of a standalone `TownTileRenderer` class that turned out to be an undiscovered duplicate of `UIPANEL_Surface` (graphics/LOCOBITMAP.h) — same field layout at every offset the methods touch (`mode`@+4, `stride`→`width`@+8, `palette`→`palette_ptr`@+0x14, `pixels`@+0x18, `surface_ref`→`ddraw_surf`@+0x1C), confirmed by `UIPANEL_Blit` passing its own `this` directly as the receiver of these calls. `UIPANEL_Blit` (0x42B050) was calling `extern "C"`-flavored free-function declarations matching no real symbol (only the typed methods exist) — every dispatch site was call-0. Fixed by merging `TownTileRenderer` into `UIPANEL_Surface` (deleted `town/TownTiles.h`, rescoped `TownTiles.cpp`'s definitions to `UIPANEL_Surface::`, renamed the 3 shifted fields) and rewriting `UIPANEL_Blit` to call through the typed methods. Also fixed `UIPANEL_Surface::palette_ptr`'s declared type: `uint32_t*` (wrong, "128 uint32 entries") → `uint16_t*` (right, evidenced by `DrawTile`'s byte-indexed 2-byte-stride palette lookup and matching what `UIPANEL_ReadPaletteFromBMP`'s own comment already said). **Two real, pre-existing correctness bugs found and fixed in the same function while rewiring it** (both in the untested, never-validated `mode==1` DDraw-hardware branch): (1) the software-tile dispatch switch was incorrectly ALSO reachable from the `mode==1` branch and could double-dispatch for some flag values (e.g. `flags==0x20` ran both the switch's default case AND the separate `flags>=0x20` block) — the original is a single if/else-if/switch chain, exactly one dispatch per call, restored; (2) `mode==1` never calls a tile method in the original at all — it always performs exactly one `IDirectDrawSurface4::Blt()` call with `dest_rect`/`src_rect` built from `(src_x,src_y,dest_x,dest_y)` and `(clip_left,clip_top,clip_right,clip_bottom)` respectively — the existing code had those two rects swapped, and had the `DDBLT_KEYSRC`/flags==0/1 polarity backwards. Also fixed a second, independent call-0 landmine this merge surfaced: `TownTiles.cpp`'s own `BlitElement` method called `UIPANEL_Blit` with a stray `int**`-typed 5th parameter against the real `void*` — fixed. **Two symbols added as loud deferred stubs, not implemented**: `Town_CalcScrollRect` (0x42C590) and `Town_CalcScrollRect_Reversed` (0x42C700) — Ghidra's own decompilation of both is internally inconsistent about the real parameter count (caller pushes 4 stack dwords, callee's `RET 0x10` pops 4, but the decompiled body only demonstrably reads 2 of them; the other two surface only as unresolved `unaff_EBX`/`unaff_EBP`/`ptStack_4` artifacts that may just be the decompiler losing track of the same RECT pointer across intervening `SetRect`/`IntersectRect` calls, not genuinely distinct parameters) — rather than guess at pixel-level rect math with this much open uncertainty, implemented as loud stubs (fprintf+assert) per CLAUDE.md's stub policy; tracked in PROGRESS.md. Verified: 333 → 287 (exactly 46, matches this row's own count: 8+6+6+3×8+1+1). `meson test -C build`: 30/30 (with a real asset checkout — a bare worktree without `lego-loco-unpacked/` symlinked misreports 8 unrelated failures, see the false-negative note above). **Caveat**: no test drives `UIPANEL_Blit` with `mode==0` (the software tile path was entirely unreachable/call-0 before this fix), so 30/30 proves the linkage now resolves and nothing regressed — not that the mode==0 tile pixels are correct. Separately (not counted in the 46, since it wasn't a `call 0` — it was silently binding to an unrelated wrong stub in `shared/defsym_stubs.cpp` instead): `town/Town.cpp` declared `UIPANEL_Blit` inside its own `extern "C"` block with a `void` return, giving it plain-C linkage that didn't match the real C++-mangled symbol at any of its ~9 call sites; moved out of `extern "C"`, return type corrected to `bool`. |
| 1+1 | `NETMAN_FreePacket`/`NETMAN_SendPacket` — **FIXED (2026-08-08, native-session-glue-cluster)** | Row below (`NETMAN_FreePacket`, `GameConfig::GameConfig()`) and the near-match row (`NETMAN_SendPacket`) are fixed: both retyped `int32_t packetPtr` -> `GameConfig* packetPtr` in `network/Netman.h` (the real definitions in `native/NETMAN_SessionSettings.c` always took the pointer form), and `game/GameConfig.cpp`'s ctor call site fixed from `NETMAN_FreePacket(static_cast<int32_t>(reinterpret_cast<intptr_t>(this)))` to `NETMAN_FreePacket(this)`. Confirmed via `nm`: the ctor's reference moved from unresolved `_Z17NETMAN_FreePacketi` to defined `_Z17NETMAN_FreePacketPh`. `NETMAN_SendPacket`'s other two callers (`ui/EditWindow.cpp`, `native/NETMAN_NetworkUI.c`) were deliberately left untouched -- see the two new rows below. Also renamed `NET_Dtor`->`NET_ComputeColor`/`NET_BaseDtor`->`NET_GetNextAttId` (Ghidra's own auto-analysis names) and removed a duplicate/dead `NETMAN_AllocPacket` (0x440CC0, same address as the already-integrated `GameConfig::~GameConfig()`, plus a forbidden manual vtable write). `NET_ComputeColor` was itself a silent-wrong-stub: `shared/defsym_stubs.cpp` had a `void`-returning no-op that `network/NetworkPlayerList.cpp`'s real call was silently binding to instead of a color; removed now that the real definition exists. `NET_GetNextAttId` was call-0 from `game/Train_network.cpp` (bogus mid-function `/* 0x00445E70 */` annotation, mismatched signature vs. an orphaned `shared/link_stubs.cpp` stub); corrected the declaration, which now resolves to the real 0x445F20 body -- `TrainSubsystem::HandleConnectionSetup`'s `resp[3] = NET_GetNextAttId();` linkage was fixed as a side effect. |
| -- | `NETMAN_SendPacket(void*)` in `ui/EditWindow.cpp` (0x43CDF0 annotation is bogus) | **Still open.** `ui/EditWindow.cpp:116` declares `void __thiscall NETMAN_SendPacket(void* netman); /* 0x43CDF0 */` and calls it at line 785 as `NETMAN_SendPacket(_g_netman_state)`. 0x43CDF0 is not a function entry point -- it's an address inside `Train_SendPlayerInfo` (0x43CCC0). The call is semantically the same 0x440EA0 (`_g_netman_state + 7` matches `GameConfig::m_autoStart`, same field the real function's callers all touch), just with a *fourth* distinct wrong declaration (`(void*)`, mangles `_Z17NETMAN_SendPacketPv`) alongside the `int32_t` one fixed above and `native/NETMAN_NetworkUI.c`'s own `(int32_t)` one below. Not fixed here -- out of the native-session-glue-cluster's file scope. |
| -- | `native/NETMAN_NetworkUI.c`'s `NETMAN_FreePacket`/`NETMAN_SendPacket(int32_t)` | **Still open**, and *not* touched by the 2026-08-08 native-session-glue-cluster fix on purpose -- a separate concurrent session owns this file. Its own local externs (`extern void __fastcall NETMAN_SendPacket(int32_t packetPtr);`, called as `NETMAN_SendPacket((int32_t)(uintptr_t)_g_netman_data)`) still mismatch the real `(GameConfig*)` definition; this file also has its own ~71-site old-style-cast cluster, tracked separately. |
| -- | `game/GameConfig.h`'s `extern GameConfig* g_dplayConfig;` -- dangling, never defined | **Still open.** Declared (`/* 0x4FD3A8 -- alias _g_dplay / _g_dplay_config */`) but has no defining declaration anywhere in the tree (checked: no `g_dplayConfig = ...`, no plain-global definition in any `shared/*stub*.cpp`). Using it anywhere would mint a brand-new undefined-symbol reference, masked only by `-Wl,--unresolved-symbols=ignore-all` -- the opposite of a fix. `_g_netman_data` (defined in `shared/defsym_stubs.cpp`, already typed `GameConfig*` by precedent in `network/Netman.cpp`) is the correct symbol to use for this singleton until someone actually wires up construction/assignment. |
| -- | `ui/NameEntryPanel.h` `+0x19C` (RECT) and `+0x1E0`/`+0x1E1` (toggle-enabled flags) -- evidenced, not yet added | **Still open.** Found while resolving `native/NETMAN_SessionSettings.c`'s `NETMAN_DestroySession` to `NameEntryPanel*`: `NETMAN_SetSessionInfo` (0x441C80, not yet implemented in-tree under that name) does `PtInRect((RECT*)(this+0x19C), ...)` -- exactly the size of the header's existing `_gap_19C[16]` -- and gates the 2-player/4-player toggle buttons (`sprite2`@+0x1B8/`sprite3`@+0x1BC) on `*(char*)(this+0x1E0)`/`+0x1E1` being nonzero. Left unresolved because `NETMAN_SetSessionInfo` itself is outside this session's file scope; a future session integrating that function should add these fields with the evidence above. |

## Near-match (mechanical declaration/linkage fixes) — still open

| Sites | Symbol | Candidate real definition(s) | Callers |
|---|---|---|---|
| 10 | `TrackPiece_SetZoom` | TrackPiece_SetZoom, TrackPiece_SetZoom(void*, short) | ScriptedObject::UpdateToolState(TrackPiece*) |
| 8 | `CRT_exit` | CRT_exit, CRT_exit(char const**, char const**) | Game_LoadWaveFile(char const*, void*) |
| 6 | `UIPANEL_EndPaintEx` | UIPANEL_EndPaintEx, UIPANEL_EndPaintEx(void*, int, int, unsigned char, RECT*), UIPANEL_EndPaintEx(void*, int, int, unsigned char, void*) | GameSetupPanel::drawLayoutList(LayoutListNode*), GameSetupPanel::drawGrid(), GameSetupPanel::drawTitle(), GameSetupPanel::on_update(int), Netman::HandlePlayerJoin(), Netman::RemoveInboundTrain(int) |
| 3 | `WIN32_StreamRead` (cluster A) | WIN32_StreamRead | Game_LoadWaveFile(char const*, void*), GameSetupPanel::loadLayouts(bool) |
| 4 | `PlaySound` | PlaySound, PlaySound(unsigned int) | BuildingMgr::HandleClick(BuildingClickCommand const*, int, int, int, int), HelpWnd::handle_click(void*, unsigned int, unsigned int, int) |
| 4 | `IntersectRect` | IntersectRect | UIPANEL_EndPaintEx(void*, int, int, unsigned char, RECT*), Panel::DispatchEvent(RECT*) |
| 4 | `TileMap_GetObjectAt` (cluster A) — **FIXED (2026-08-08, Building.cpp STRICT=2 cast cluster)** | TileMap_GetObjectAt(TileMap*, short, short, short) | Building::StepToward(int, int), Building::FindNearbyObject(int, int, int), plus AddOccupant/RemoveOccupant/CheckPlacementCollision (not originally counted in this row's tally but the same symbol/fix). Dropped the local `extern void* TileMap_GetObjectAt(TileMap*, int, int, int)` declaration entirely; now uses world/tilemap.h's real inline wrapper. |
| 3 | `WIN32_StreamRead` (cluster C) | WIN32_StreamRead | Game_ReadChunk(WNDPROC_Stream*, RiffChunkHeader*, int, int) |
| 3 | `CRT_sprintf_buf` | CRT_sprintf_buf, CRT_sprintf_buf(char*, char const*), CRT_sprintf_buf(char*, char const*, ...) | ScriptedObject::HandleEvent(unsigned int, char const*) |
| 3 | `InflateRect` | InflateRect | TileMap_ProcessDirtyRects(RECT*) |
| 3 | `TileMap_GetObjectAt` (cluster B) | TileMap_GetObjectAt, TileMap_GetObjectAt(TileMap*, short, short, short) | World_RenderAll(Vehicle*), World::FinalizeLoad(Vehicle*, int, char) |
| 3 | `FormatResourceString` | FormatResourceString, FormatResourceString(void*, int, char*, int), FormatResourceString(void*, unsigned int, char*, int) | GameSetupPanel::updateTitle(), GameSetupPanel::drawLayoutList(LayoutListNode*) |
| 3 | `ResourceManager_GetById` | ResourceManager_GetById(void**, int), ResourceManager_GetById(void**, unsigned int), ResourceManager_GetById(void*, int) | NETMAN_JoinSession(void*), BuildingPanel::init_sprites() |
| 2 | `UI_ChildWindow_Render` | UI_ChildWindow_Render | ScriptedObject::HandleEvent(unsigned int, char const*) |
| 2 | `FormatMessageA` | FormatMessageA, FormatMessageA(int, void*, int, int, char*, int, void*) | GameWindow::create(...) |
| 2 | `DDRAW_UnlockPrimary` | DDRAW_UnlockPrimary, DDRAW_UnlockPrimary() | GameWindow::show(), GameWindow::set_mode(int, void*, unsigned char, unsigned char) |
| 2 | `CGWND_GameSetup_DrawGrid_Thunk` | CGWND_GameSetup_DrawGrid_Thunk | Netman::HandlePlayerJoin(), Netman::RemoveInboundTrain(int) |
| 2 | `AssetMgr_ReadPairValue` — **FIXED (2026-08-08, Building.cpp STRICT=2 cast cluster)** | AssetMgr_ReadPairValue(AssetMgr*, unsigned int, unsigned int) | Building::StepToward(int, int), Building::FindNearestConnectionNode(void*, unsigned int) — both call sites now static_cast their `void*`/AssetMgr-shaped pointer to `AssetMgr*` (forward-declared locally in Building.cpp; resources/AssetMgr.h itself is deliberately NOT included there — see that file's top-of-file comment on why). |
| 2 | `Vehicle_GetOccupantCount` — **FIXED (2026-08-08, Building.cpp STRICT=2 cast cluster)** | Vehicle::GetOccupantCount() (typed method, game/Vehicle.h — not a free function) | Building::FindPathToTarget() — both sites converted from the untyped free-function call to `vehicle->GetOccupantCount()` after Ghidra-confirming (via Vehicle::state @ +0x5C matching the read at vehicle+0x5C) that the object is genuinely a `Vehicle*`. |
| 1 | `WNDPROC_CriticalSectionLock` (FIXED) | WNDPROC_CriticalSectionLock(int*, char*) | edit_key_handler_parse(void*, KeySequenceRecord*) — FIXED: moved out of extern "C" block, signature corrected to (int*, char*), call sites updated with reinterpret_cast. |
| 1 | `WIN32_StreamOpenPath` (cluster B) | WIN32_StreamOpenPath | Game_LoadWaveFile(char const*, void*) |
| 1 | `WNDPROC_EnterCriticalSection` | WNDPROC_EnterCriticalSection | Game_ReadChunk(WNDPROC_Stream*, RiffChunkHeader*, int, int) |
| 1 | `WNDPROC_LeaveCriticalSection` | WNDPROC_LeaveCriticalSection | Game_ReadChunk(WNDPROC_Stream*, RiffChunkHeader*, int, int) |
| 1 | `CRT_wcsstr` | CRT_wcsstr, CRT_wcsstr(char const*, char const*) | AssetMgr_LoadFile(void*, unsigned char*, int*) |
| 1 | `CRT_0x468610` | CRT_0x468610(void*, unsigned int, unsigned int, int) | AssetMgr_LoadFile(void*, unsigned char*, int*) |
| 1 | `Vehicle_Ctor` | Vehicle_Ctor, Vehicle_Ctor(void*, int, int, char, char) | Train_HandleTrackBuild(void*, int) |
| 1 | `Vehicle_InitRoute` | Vehicle_InitRoute, Vehicle_InitRoute(void*, int, unsigned int, char) | Train_HandleTrackBuild(void*, int) |
| 1 | `VehicleEditor_SetDPlayData` | VehicleEditor_SetDPlayData | Train_HandleTrackBuild(void*, int) |
| 1 | `DirectPlay_Close` | DirectPlay_Close, DirectPlay_Close(int) | Train_HandleTrackBuild(void*, int) |
| 1 | `Train_SendPlayerInfo` | Train_SendPlayerInfo | Train_HandleTrackBuild(void*, int) |
| 1 | `DDRAW_SetSurfaceFormat` | DDRAW_SetSurfaceFormat, DDRAW_SetSurfaceFormat(void*, int) | GameWindow::create(...) |
| 1 | `World_SerializeObject` | World_SerializeObject | Netman::RemoveInboundTrain(int) |
| 1 | `ArrivalQueue_RemoveVehicle` — **FIXED (2026-08-09, ArrivalQueue self-type session, see dedicated section below)** | ArrivalQueue_RemoveVehicle, ArrivalQueue_RemoveVehicle(void*, unsigned int, char) | World_RenderAll(Vehicle*) |
| 1 | `InvalidateRect` | InvalidateRect, GameObject::InvalidateRect(), World::InvalidateRect(int, int, int, int, short) | TileMap::FullReset() |
| 1 | `UpdateWindow` | UpdateWindow | TileMap::FullReset() |
| 1 | `AssetMgr_LoadFile` | AssetMgr_LoadFile, AssetMgr_LoadFile(int*, unsigned char*, int*), AssetMgr_LoadFile(void*, unsigned char*, int*) | GameSetupPanel::loadLayouts(bool) |
| 1 | `Train_StartMultiplayer` | Train_StartMultiplayer | TrainSubsystem::DispatchMessage(void*) |
| 1 | `Train_StopMultiplayer` | Train_StopMultiplayer | TrainSubsystem::DispatchMessage(void*) |
| 1 | `RESDATA_Lock` | RESDATA_Lock | BuildingMgr::CompactCollections() |
| 1 | `RESDATA_Unlock` | RESDATA_Unlock | BuildingMgr::CompactCollections() |
| 1 | `MessageBoxA` | MessageBoxA | WIN32_FatalError() |
| 1 | `NET_RegisterPlayer` | NET_RegisterPlayer | Netman::HandleTimeout(Vehicle*) |
| 1 | `World_FinalizeLoad` | World_FinalizeLoad | Netman::SendChatMessage(Vehicle*) |
| 1 | `UI_CenterWindow` (cluster B) | UI_CenterWindow(int*, int*) | RenderConnectionPanel(void*) |
| 1 | `VehicleEditor_Update` | VehicleEditor_Update(void*) | World::UpdateTick() |
| 1 | `RESDATA_IsRoadTile` | RESDATA_IsRoadTile, RESDATA_IsRoadTile(int) | RESDATA_GameVehicle::RESDATA_GameVehicle(int) |
| 1 | `NETMAN_FreePacket` — **FIXED (2026-08-08, native-session-glue-cluster, see "Fixed so far")** | NETMAN_FreePacket(unsigned char*) | GameConfig::GameConfig() |
| 1 | `CRT_localtime` (cluster A) | CRT_localtime | Building::DecideAction() |
| 1 | `CRT_localtime` (cluster B) | CRT_localtime | ResourceGameObject::UpdateScheduledAnimation() |

## Genuinely missing (need Ghidra RE or a loud deferred stub) — still open

The `Town_Draw*` / `Town_*TileCache*` cluster that used to be listed here (13
symbols) is now fixed — see "Fixed so far" above. It was never actually
"genuinely missing" (see the correction note near the top of this file):
11 of the 13 had validated implementations sitting under the wrong class
name, and the other 2 (`Town_CalcScrollRect`/`_Reversed`) are now loud
deferred stubs rather than call-0 landmines.

| Sites | Symbol | Callers |
|---|---|---|
| 8 | `WNDPROC_StreamPrintf` (STUB) | BuildingDescriptorEditor::draw_border_grid(void*), BuildingDescriptorEditor::paint_edit_regions(void*) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 7 | `WNDPROC_StreamWrite` (STUB) | edit_key_handler_parse(void*, KeySequenceRecord*) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 5 | `ReleaseSoundResource` | HelpWnd::go_next_page(), HelpWnd::go_prev_page(), HelpWnd::hide(), GameSetupPanel::base_destructor() |
| 4 | `LoadSoundResource` | HelpWnd::go_next_page(), HelpWnd::go_prev_page() |
| 3 | `WNDPROC_StreamReadLine` (STUB) | edit_key_handler_parse(void*, KeySequenceRecord*) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 2 | `CRT_fabs` (STUB) | edit_key_handler_parse(void*, KeySequenceRecord*) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 2 | `ScriptedObject_ParseStream` | ScriptedObject::HandleEvent(unsigned int, char const*) |
| 2 | `Ordinal_1` | GameAudio::Init() |
| 2 | `EditorState_Copy` | **FIXED (2026-08-08, Vehicle.cpp cast cluster session).** Both call sites in `Vehicle::UpdateEngineSound()` went through an `extern "C"` free-function declaration for a real, already-existing typed method (`EditorState::Copy(const EditorState*)`, 0x40B5D0) — fixed by calling `this->editor_state->Copy(...)` directly as part of a broader raw-offset-cast cleanup in `game/Vehicle.cpp`. `call 0` dropped 286→284, confirming both sites were genuinely unresolved (not a silent-wrong-stub). `VehicleEditor_Ctor`/`EditorState_Ctor` in the same file are a separate, still-open silent-wrong-stub/call-0 pair (see their own worklist rows below) — deliberately left for a follow-up commit. |
| 2 | `ScriptedObject_InitBase` | ScriptedObject::RemoveChild(), ScriptedObject::AddChild(unsigned int, char const*) |
| 1 | `CRT_fmod` (STUB) | edit_key_handler_parse(void*, KeySequenceRecord*) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 1 | `Stream_BeginEnum` (STUB) | Game_ReadChunk(WNDPROC_Stream*, RiffChunkHeader*, int, int) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 1 | `WNDPROC_StreamSeekForward` (STUB) | Game_ReadChunk(WNDPROC_Stream*, RiffChunkHeader*, int, int) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 1 | `Stream_BeginRead` (STUB, cluster A) | Game_ReadChunk(WNDPROC_Stream*, RiffChunkHeader*, int, int) — STUB: loud fprintf+assert in stubs_impl.cpp. Real def exists at 0x464DB0 for cluster B (UIPANEL_Surface.cpp path). |
| 1 | `Stream_BeginRead` (cluster B) | UIPANEL_StretchBlit(void*, char const*, unsigned int, int, int) — check whether this is really the same function as cluster A under a different overload before assuming duplication |
| 1 | `CRT_0x468790` | AssetMgr_LoadFile(void*, unsigned char*, int*) |
| 1 | `Huf_GetUncompressedSize` | AssetMgr_LoadFile(void*, unsigned char*, int*) |
| 1 | `Huf_Decode` | AssetMgr_LoadFile(void*, unsigned char*, int*) |
| 1 | `GameAudio_Ctor` | DDRAW_InitAudio() |
| 1 | `GameAudio_Init` | DDRAW_InitAudio() |
| 1 | `GameAudio_SetListenerPos` | DDRAW_InitAudio() |
| 1 | `GameAudio_SetBounds` | DDRAW_InitAudio() |
| 1 | `DDRAW_UpdateBuildingSprites` | DDRAW_UpdateBuilding(void*) |
| 1 | `DDRAW_UpdateVehicleSprites` | DDRAW_UpdateBuilding(void*) |
| 1 | `Cursor_UpdateDirtyRect` | GameWindow::set_mode(int, void*, unsigned char, unsigned char) |
| 1 | `Cursor_RenderWithViewport` | GameWindow::set_mode(int, void*, unsigned char, unsigned char) |
| 1 | `EditorState_LoadExistingGame` | Netman::HandlePlayerJoin() |
| 1 | `EditWindow_InitNetworkPanel` | MultiplayerLobby_Reload() |
| 1 | `GameAudio_Cleanup` | DDRAW_DestroyAudio() |
| 1 | `GameObject_GetSubObjectWorldPos` | TileMap::ProcessObjectTimer(TileMapObject*) |
| 1 | `EditorState_StartNewGame` | GameSetupPanel::SelectLayoutEntry(int) |
| 1 | `EditorState_SetDifficulty` | Netman::SyncGameState(TrainMessage*) |
| 1 | `DispatchToSubObjects` | DDRAW_DispatchToSubObjects(int, int, int, int, void*) |
| 1 | `EditorState_Ctor` | Vehicle::Vehicle(int, int, unsigned char, unsigned char) |
| 1 | `Entity_Ctor` | ScriptedObject::ScriptedObject() |
| 1 | `World_DeserializeMap` | RESDATA_GameVehicle::~RESDATA_GameVehicle() |
| 1 | `TileMap_FindTileByType` — **FIXED (2026-08-08): NOT genuinely missing** (correcting this file's own prior claim). Ghidra-decompiling 0x432940 (Building::TeleportTo) shows the real call is `TileMap_FindNearestObject(&g_tilemap, 0xc, target_x, target_y, 0x900)` — i.e. `TileMap::FindNearestObject` (0x457CE0, world/tilemap.h/.cpp), already implemented and exposed as `TileMap_FindNearestObject`; Building.cpp's own comment already cited the same address (0x457ce0) under the wrong name. The caller also had the arguments in the wrong order/positions (type_filter and radius swapped with x/y) — fixed both the symbol and the call-site argument order together, since fixing only the linkage would have made a now-reachable call read (x, y, 0x0C, 0x900) as (type_filter=x, tx=y, ty=0x0C, radius=0x900). | Building::TeleportTo(int, int) |

Note: `Cursor_Render` (1 site, `AboutDialog::Update()`) folded into the
`Cursor_Render` row above (was previously listed separately).

### wave_io.c / BuildingDescriptorEditor.cpp — prior investigation (work lost, analysis preserved)

A subagent investigated this cluster before its changes were lost in the
incident above. Its analysis, worth reusing rather than re-deriving from
scratch:

- `native/wave_io.c` declares several of its externs (`Stream_BeginEnum`,
  `WIN32_StreamRead`, `WNDPROC_StreamSeekForward`,
  `WNDPROC_EnterCriticalSection`, `WNDPROC_LeaveCriticalSection`,
  `AssetMgr_LoadFile`, `WIN32_StreamOpenPath`) with a stray `__thiscall`
  calling-convention annotation on what should be plain C-linkage functions —
  since this is a `.c` file, `__thiscall` on a prototype doesn't change
  linkage the way `extern "C"` mismatches do in C++, but it's still wrong
  documentation of the real calling convention and worth removing while
  fixing the actual signature mismatches.
- `input/BuildingDescriptorEditor.cpp`'s `WNDPROC_CriticalSectionLock` real
  definition is C++-mangled (`_Z27WNDPROC_CriticalSectionLockPiPc`,
  i.e. `(int*, char*)`) but the file declares it inside the `extern "C"`
  block — needs to move out of `extern "C"` with the corrected `(int*,
  char*)` signature, with call sites updated to pass `reinterpret_cast<int*>`
  of the stream handle rather than `void*`.
- The `WNDPROC_Stream*` family being MISSING in both this file and
  `native/wave_io.c` suggests a genuinely unimplemented low-level
  stream/CRT helper layer shared by both — check for a natural home (e.g.
  `resources/WndProcStreamBuf.h`/`.cpp` or `resources/Win32StreamFile.h`/`.cpp`,
  documented in PROGRESS.md's "win32_stream.c removed (partial)" entry as
  already covering part of this class hierarchy) before assuming these need
  brand new stream primitives.

## UIPANEL_Blit callers — wrong signature — FIXED (2026-08-06, cross-validation session)

While fixing the `Town_Draw*` cluster above (which is `UIPANEL_Blit`'s own
*callee* side), `nm --print-file-name build/lego_loco.p/*.o | grep
UIPANEL_Blit` had turned up a large, separate, pre-existing problem on
`UIPANEL_Blit`'s *caller* side: at least 15 files declared `UIPANEL_Blit`
locally with signatures that didn't match its one real definition
(`ui/UIPANEL_Surface.cpp`, mangled `_Z12UIPANEL_BlitPvjjijS_jjijj` —
`(void*, uint32_t,uint32_t,int32_t,uint32_t, void*, uint32_t,uint32_t,int32_t,uint32_t,uint32_t)`,
confirmed against Ghidra's disassembly of 0x42B050, which matches this
signature exactly). All 14 real callers plus one dead declaration fixed this
session (one more than this table's original 13 — `core/VehicleEditor.cpp`
had a 14th mismatched declaration this table missed entirely, caught only by
re-checking `nm` referrers after the fix, not by grepping the original list).

**Root cause, not just a param-type typo**: `shared/link_stubs.cpp` had three
no-op overloads (`_Z12UIPANEL_BlitPviiiiS_iiiih/i/j`) deliberately added in an
earlier session specifically to make these mismatched callers link — this is
the "worse than call-0" shape flagged elsewhere in this doc: it linked
cleanly and silently ate every blit these callers issued instead of crashing
or showing up in a `call 0` census. Confirmed via `nm --print-file-name
build/lego_loco.p/*.o` that this is why the main binary's `call 0` count
(287) did **not** move at all from this fix — none of these were `call 0`
sites to begin with. Removed the three dead no-op overloads after confirming
zero referrers left in any object file.

Two extra gotchas found only by rebuilding after the mechanical fix, not by
inspecting the caller files alone:
- `world/tilemap.h` had its **own**, separate, wrong all-`int` declaration
  that `world/tilemap.cpp` (which includes it) still bound to even after
  `tilemap.cpp`'s own declaration was fixed — C++ overload resolution picked
  the header's plain-`int` overload over the correct mixed-type one because
  RECT's `int` fields are an exact match for the wrong shape and only a
  conversion for the right one. Fixing the caller `.cpp` file is not
  sufficient when a header it includes has its own conflicting declaration;
  confirmed via `nm -u` on the compiled `.o`, not by re-reading the source.
- Two narrow, deliberately-strict test targets (`inputmgr-canonical`,
  `input-world`, `tests/meson.build`) link `core/GameObject.cpp`'s object
  directly with no `--unresolved-symbols=ignore-all` override, and
  `tests/persistence_fixtures.h` had a test-local `LOUD_FIXTURE(UIPANEL_Blit)`
  shim whose signature matched the *old, wrong* shape (by design — it only
  needed to match whatever `core/GameObject.cpp` declared, at the time). This
  is the same "test-local shim, legitimate" pattern documented in the memory
  system's landmine-bug-classes note (1c) — updated the fixture's signature
  to match the real one rather than declaring it a regression.

Fixed callers (all now declare the one real signature and link against the
real implementation):

| File | Old (wrong) shape | Fix |
|---|---|---|
| `ui/ButtonSprite.cpp` | `_Z12UIPANEL_BlitPviiiiS_iiiih` | param types corrected |
| `input/Cursor_internal.h` (shared by `input/Cursor.cpp`, `input/Cursor_new_impls.cpp`) | `_Z12UIPANEL_BlitPviiiiS_iiiii` | param types corrected |
| `network/DPlayManager.cpp` | `_Z12UIPANEL_BlitPviiiiS_iiiii` | param types corrected |
| `network/NetworkPlayerList.cpp` | `_Z12UIPANEL_BlitPviiiiS_iiiii` | param types corrected |
| `ui/GameSetupPanel.cpp` | `_Z12UIPANEL_BlitPviiiiS_iiiii` | param types corrected |
| `world/tilemap.cpp` + `world/tilemap.h` | `_Z12UIPANEL_BlitPviiiiS_iiiii` | param types corrected in both; removed the now-redundant duplicate declaration from the `.cpp` |
| `core/GameObject.cpp` | `_Z12UIPANEL_BlitPviiiiS_iiiij` | param types corrected |
| `ui/AboutDialog.cpp` | `_Z12UIPANEL_BlitPviiiiS_iiiij` | param types corrected |
| `ui/UIPANEL.cpp` | `_Z12UIPANEL_BlitPviiiiS_iiiij` | param types corrected |
| `core/VehicleEditor.cpp` | `_Z12UIPANEL_BlitPviiiiS_iiiij` (unused decl, no call site) | param types corrected for consistency; was never a live landmine |
| `game/BuildingPanel.cpp` | plain `UIPANEL_Blit` (extern "C") | moved declaration out of the `extern "C"` block; return type corrected `void`→`bool` |
| `graphics/LOCOBITMAP.cpp` | plain `UIPANEL_Blit` (extern "C") — param types already matched, only linkage was wrong | moved declaration out of the `extern "C"` block |
| `native/NETMAN_NetworkUI.c` | plain `UIPANEL_Blit` (extern "C") | **Correction to this table's original "N/A" note**: `meson.build`'s `common_c_args` compiles all `native/*.c` as C++ (`-x c++`), matching the original Makefile's `$(CXX)` usage — this file needed the exact same fix as the other extern-"C" cases, not a C-linkage shim. |
| `town/Town.cpp` | plain `UIPANEL_Blit` | Fixed in an earlier session — see "Fixed so far" |
| `town/TownTiles.cpp` (`BlitElement`) | `_Z12UIPANEL_BlitPvjjijPPijjijj` | Fixed in an earlier session — see "Fixed so far" |

`game/BuildingMgr.cpp` also references `_Z19UIPANEL_BlitSurfacePviiS_ii` —
that's a genuinely different function (`UIPANEL_BlitSurface`, 0x42A540), not
part of this cluster.

Verified: `meson test -C build` 30/30, `meson test -C build --suite
integration` 12/12 (both matching the pre-fix baseline exactly), main binary
and every `build/tests/*_test` binary's `call 0` count unchanged from
baseline (287 / 92 / 21 / 0×20 — see the note above on why this fix
correctly does not move that count).

**Caveat, stated explicitly so green tests aren't misread as pixel
verification**: this fix moved ~14 call sites from silently-no-op to
actually executing — `Entity::Draw`/`DrawConnected`, `AboutDialog`'s credits
scroll, the tilemap cursor overlay, `NetworkPlayerList`/`DPlayManager`
multiplayer panels, `GameSetupPanel`, `BuildingPanel`, `ButtonSprite`, and
`Cursor`. Byte-for-byte diffed every GUI integration screenshot from a
pre-fix baseline run against the same test's post-fix screenshot
(`test_game_setup_lobby_search_and_exit`, `test_multiplayer_ready_go_is_exposed_after_session_projection`,
`test_multiplayer_layout_choices_update_grid_geometry`) — **all identical**.
30/30 + 12/12 proves no crash/regression on covered paths; it does **not**
prove these newly-live blits render correctly, since none of the current
GUI tests exercise a code path that reaches them (consistent with this
doc's existing note that no test drives `UIPANEL_Blit` with `mode==0`).
Follow-up cluster, distinct from a `call 0`/linkage fix: add GUI test
coverage (or targeted screenshot assertions) for at least one of these
newly-reachable rendering paths before trusting their pixel output.

Also removed two now-fully-dead `UIPANEL_Blit` stub definitions found by
re-running the whole-tree `nm` census after the fix above (not caught by
the original 3-overload cleanup): `shared/defsym_stubs.cpp`'s zero-arg
`UIPANEL_Blit()` (the wrong stub `town/Town.cpp` used to silently bind to
before the town-tilerender-merge session fixed it) and
`shared/core_stubs.cpp`'s `int**`-6th-param overload (the wrong stub
`town/TownTiles.cpp`'s `BlitElement` used to call before that same
session's fix). Both confirmed zero referrers via `nm --print-file-name
build/lego_loco.p/*.o`; `ui/UIPANEL_Surface.cpp` is now the **only**
`UIPANEL_Blit` definition anywhere in the tree.

## Whole-tree symbol census — new sweep instrument (2026-08-06)

`call 0` counts are blind to this bug class whenever a fabricated stub
happens to cover the mismatched caller's exact wrong signature (see the
`UIPANEL_Blit` section above) — the caller links clean and silently no-ops
instead of crashing or leaving a call-0 relocation. The census below finds
that shape directly: for every plain free-function base name with more
than one distinct defined signature in `build/lego_loco.p/*.o`, check
whether (a) one signature is defined *only* in a stub file
(`shared/{link,defsym,core,stubs_impl}_stubs.cpp`/`stubs_impl.cpp`) with
live non-stub referrers, and (b) a genuinely different, non-stub
implementation exists elsewhere under a different signature. That
combination is the exact "wrong stub, real impl sitting unused elsewhere"
shape. Regenerate with:

```bash
nm --print-file-name --defined-only -C build/lego_loco.p/*.o   # + a second pass without --defined-only for referrers
```

(see session scratchpad `analyze_census2.py` for the exact grouping logic —
not checked in, regenerate as needed). First run found **43 findings across
~25 distinct symbols**; each is tracked below as it's resolved.

**Two limits of this instrument, learned the hard way — check both before
trusting a census row:**

1. **The census can't see `static`/internal linkage.** `nm -C` lists a
   `static` function's symbol the same as an exported one. If the "other"
   signature turns out to be `static`, it is not "the real implementation
   elsewhere" — it's a same-named-but-unrelated local function, and the
   actual caller-side symbol may have no real implementation at all (see
   `FormatResourceString` below). Check linkage with `nm` symbol type (`t`/`T`
   lowercase = local/static) or grep the declaration before trusting a "real
   signature / location" cell.
2. **A "wrong" stub covering a caller may be load-bearing, not a bug.** The
   census's whole premise — a stub with the caller's wrong signature is
   masking a real implementation the caller should bind to instead — assumes
   the real implementation is safe to make reachable. The `DirectPlay_*`
   cluster (below) is a case where it wasn't: the stub was inadvertently
   protecting a half-64-bit-ported subsystem from ever executing. A census
   hit is a candidate for investigation, not an automatic fix.

### `CGWND_SetMode(void*)` — FIXED (2026-08-06)

See PROGRESS.md's "CGWND_SetMode(void*)" entry (was already a tracked open
item there) for the full writeup: 6 callers fixed
(`ui/EditWindow.cpp`, `ui/HelpWnd.cpp`, `game/ScriptedObject.cpp`,
`game/BuildingPanel.cpp`, `world/scriptengine.cpp`, `graphics/LOCOBITMAP.cpp`),
two bogus address annotations corrected, two dead stubs removed. `meson
test -C build`: 30/30. `--suite integration`: 12/12. `call 0` unchanged
(287).

### Remaining census findings — still open

Grouped by primary file(s) to fix together (real signature noted where
already confirmed against Ghidra or an existing correct declaration
elsewhere in the tree; unconfirmed ones need the same treatment before
touching):

| Symbol | Wrong-stub callers | Real signature / location |
|---|---|---|
| `TileMap_InvalidateRect` | `core/GameObject.cpp`, `game/Vehicle.cpp`, `town/Town.cpp`, `town/sdl3_town_mode3.cpp` | `(TileMap*, int, int, int, int)` — inline in `world/tilemap.h` |
| `UIPANEL_CreateSurface` | `input/Cursor.cpp`, `native/ddraw_init.c`, `network/Netman.cpp`, `network/NetworkPlayerList.cpp`, `ui/AboutDialog.cpp`, `game/BuildingPanel.cpp`, `town/Town.cpp` | `(UIPANEL_Surface*)` — `graphics/LOCOBITMAP.cpp` |
| `UIPANEL_EndPaintEx` | ~~`native/NETMAN_NetworkUI.c`~~, `game/BuildingPanel.cpp`, `native/NETMAN_SessionSettings.c`, `town/Town.cpp`, plus confirmed via `nm` on the linked binary (2026-08-08, dplaymanager-cpp-cast-cluster session): `network/DPlayManager.cpp`, `input/Cursor_new_impls.cpp`, `ui/GameSetupPanel.cpp`, `network/NetworkPlayerList.cpp` — all declare `(void*, void*, int, uint8_t, RECT*)` (2nd param `void*`/`HWND` instead of `int`), which mangles to `_Z18UIPANEL_EndPaintExPvS_ihP4RECT` and binds to `shared/stubs_impl.cpp:502`'s host no-op instead of the real symbol below. **`native/NETMAN_NetworkUI.c` fixed (2026-08-08, netman-networkui-cast-cluster, follow-up commit)** — moved both `UIPANEL_EndPaint`/`UIPANEL_EndPaintEx` out of `extern "C"` and corrected the 2nd param to `int32_t hdc` (was `void* hwnd`), fixing the 3 call sites' argument to `static_cast<int32_t>(reinterpret_cast<intptr_t>(panel->hWnd))` matching `ui/UIPANEL.cpp::UIPANEL_EndPaint`'s own "hwnd as hdc" pattern. Verified `EndPaintEx`'s Path A (the only reachable path for a `NameEntryPanel*` self, since `UI_WindowBase::field_14` — read as "tile_map" — is always null on this class) only touches `self+0xD4`/`self+0x08` (`workRect`/`hWnd`, both present via inheritance) and explicitly null-checks `restrict_rect` before touching it (every call site here passes `nullptr`, matching Ghidra). `meson test -C build` 30/30 incl. `--suite integration`; `call 0` unchanged (271); mingw typecheck diff clean. **Caller-reachability caveat**: all 3 call sites live in `NETMAN_UpdateSessionInfo`/`NETMAN_SetSessionInfo`, which are not wired into `NameEntryPanel`'s real vtable and have no other in-tree caller (see `PROGRESS.md`'s "7 `NETMAN_*` free functions are not real virtual overrides" item) — so the 30/30 above does not exercise this new binding at runtime; correctness here rests on the static Path-A analysis, not a passing test. **Remaining files FIXED (2026-08-13, uipanel-endpaintex-cluster session)** — the real cluster was 11 files + 1 hub header, not 6/7 (a fresh full-repo grep found `graphics/LOCOBITMAP.cpp`, `ui/GameSetupPanel_network.cpp`, `ui/NameEntryPanel.cpp`, `network/Netman.h`/`.cpp`, and `shared/stubs_link001_batch4_network_world.cpp` in addition to the ones already named here). All now declare the correct `(void*, int, int, uint8_t, RECT*)` signature and bind to the real symbol (confirmed via `nm` — zero remaining references to any wrong mangled shape anywhere in the tree). See `PROGRESS.md`'s Completed milestones for the full per-file breakdown and the `DDRAW_UnlockPrimary` follow-up this pass surfaced. | `(void*, int, int, uint8_t, RECT*)` — `ui/UIPANEL.cpp` (C++ linkage, not extern "C") |
| `UIPANEL_BeginPaint` | `game/BuildingPanel.cpp`. **`network/DPlayManager.cpp` FIXED (2026-08-08, dplaymanager-cpp-cast-cluster)** — was `(int32_t)`, mangled `_Z18UIPANEL_BeginPainti`, bound to `shared/link_stubs.cpp:429`'s no-op; corrected to `(void*)`, confirmed via `nm` on the rebuilt `.o` that the reference is now `_Z18UIPANEL_BeginPaintPv`, the real symbol. `network/NetworkPlayerList.cpp:95` still carries the identical bad declaration, not fixed. | `(void*)` — `ui/UIPANEL.cpp` |
| `FormatResourceString` | `game/Train_network.cpp`, `native/NETMAN_NetworkUI.c`, `town/Town.cpp` | **False positive — not fixable this way.** `core/CGWND.cpp`'s copy is declared `static` (internal linkage), so it cannot be the real definition these callers bind to; both real (non-static) overloads elsewhere are themselves no-ops. The census's same-base-name match found a same-named `static` function and misreported it as "the real implementation elsewhere" — check linkage (`static`/anonymous-namespace) before trusting any census "real signature / location" cell. |
| `AssetMgr_LoadFile` | `game/TrainStation.cpp`, `input/BuildingDescriptorEditor.cpp`, `ui/HelpWnd.cpp` | `(void*, unsigned char*, int*)` — `native/assetmgr_loadfile.c` |
| `GetWindowTextA` | `native/NETMAN_NetworkUI.c`, `native/NETMAN_SessionSettings.c`, `ui/UI_WindowBase.cpp` | `(void*, char*, int)` — `shared/stubs_impl.cpp`/`ui/GameWindow.cpp` |
| `PlaySound` | `native/NETMAN_NetworkUI.c`, `town/Town.cpp` | `(unsigned int)` — canonical form per an earlier session's `PlaySound` fix note (`input/Cursor_internal.h`) |
| `CGWND_PumpMessages` | `core/InitMode1.cpp`, `input/Cursor_new_impls.cpp` | `(void*, unsigned char)` main-loop pump — `core/CGWND_sdl3.cpp` — **verify per call site first**: the Cursor family fix session found a call site that genuinely needed the single-`char` "loading transition" overload instead; don't assume both sites want the 2-arg pump without checking. |
| `DDRAW_RestoreSurfaces` | `input/Cursor.cpp`, `input/Cursor_Editor.cpp` | Two real overloads exist (`IDirectDrawSurface4*,void*` in `graphics/sdl3_ddraw.cpp`; `void*,void*` in `native/ddraw_surface_ops.c`) — determine which one each call site actually needs. |
| `ShowCursor` / `SetCapture` / `ReleaseCapture` | `core/Game.cpp`, `town/Town.cpp`, `ui/GameWindow.cpp` | **Do not fix mechanically.** `shared/link_stubs.cpp`'s versions were deliberately confirmed correct for `ReleaseCapture` in an earlier LINK-001 session (`core/Game.cpp::SetScreenMode`'s own comment says so) — first determine whether `input/Cursor_impls.cpp`/`ui/EditWindow.cpp`'s same-named C++-linkage functions are genuinely the same Win32-semantic operation or an unrelated collision, via Ghidra, before retargeting anything. |
| `GetResourceType` | **FIXED (2026-08-06).** `core/BuildingMgrObjectGroup.cpp`, `game/BuildingMgr.cpp` declared `(int)` — mangling doesn't encode return type, so they silently bound to `shared/defsym_stubs.cpp`'s void-returning `(int)` no-op instead of the real `(unsigned int)` at 0x446030. `ui/UI_ChildWindow.cpp` also had it wrongly inside an `extern "C"` block. Fixed all 3 to `unsigned int`/C++ linkage; fixed a bogus `world/EditorState.cpp` address (0x45AAA0 — no function — should be 0x446030); removed the two now-dead stubs. `call 0` unchanged (287 — silent-stub, not call-0). | — |
| `RESDATA_IsBuildingTile` / `RESDATA_IsRoadTile` | **FIXED (2026-08-06).** `town/Town.cpp` (`RESDATA_IsBuildingTile`, wrong `void*` param + bogus address 0x44C4E0 — that's mid-body of `VehicleEditor_Update`, not this function) and `game/Vehicle.cpp` (both, declared inside an `extern "C"` block) silently bound to `shared/defsym_stubs.cpp`'s plain no-ops instead of the real C++-linkage `(int32_t)` functions (`RESDATA_IsBuildingTile` 0x44BD30 in `world/tilemap.cpp`, `RESDATA_IsRoadTile` 0x44BD10 in `shared/stubs_impl.cpp`). `game/ResdataGameVehicle.cpp` declared `RESDATA_IsRoadTile(void*)` — a **genuine call-0** (no definition anywhere matched that mangled name), fixed by correcting the param type to `int32_t`; `call 0` dropped 287→286 confirming it. Removed 6 now-dead decoy/no-op stubs (`shared/link_stubs.cpp`'s `__pv`/`__i`-suffixed placeholders and misleading extern redeclarations, `shared/defsym_stubs.cpp`'s two zero-arg no-ops). **Reachability caveat**: the fixed call sites (`Town.cpp`'s building-tile check, `ResdataGameVehicle.cpp`'s road-tile check) aren't exercised by any current test (verified via a temporary `fprintf` instrumentation pass, removed before commit) — the real function does an `int32_t`→pointer round-trip on the resource address (same shape as the DirectPlay pointer-truncation bug), but the binary is non-PIE (`readelf -h` confirms `EXEC`, not `DYN`), and `input/InputMgr.cpp` already calls the identical real function with the identical truncating cast, live, in currently-passing tests — so this is judged safe, not verified-hot. Zero-delta call-0 diff confirmed against baseline (`e114a0b`) on every test binary, not just the main binary (`cgwnd_entermode3_test` 92, `host_mode3_bootstrap_test` 21, all others unchanged). | — |
| `TileMap_GetObjectAt` | `game/Vehicle.cpp` | `(TileMap*, short, short, short)` — `core/Game.cpp`/`world/EditorState.cpp` |
| `UI_IsBitmapReady` | `game/Panel.cpp`, `ui/UIPANEL.cpp` | **Re-check before fixing** — an earlier UIPANEL-family session claimed this was already fixed by making both callers consistent with each other, but the census shows both are still bound to `shared/stubs_impl.cpp`'s stub, not the real unmangled symbol in `ui/UI_ChildWindow.cpp`. That prior fix may have picked the wrong "real" symbol. |
| `GetModuleHandleA` | `resources/ResourceManager.cpp` | `(char const*)` — `core/CGWND.cpp` |
| `CRT_atoi` | `network/DPlayManager.cpp` | `(char const*)` — `core/CGWND.cpp`/`shared/stubs_impl.cpp` |
| `DDRAW_GetSurfaceWidthHeight` | `input/Cursor.cpp` | Two real overloads exist (`graphics/sdl3_ddraw.cpp`, `native/DDRAW_GetSurfaceWidthHeight.c`) — determine which. |
| `GetLastError` / `LocalFree` | `ui/GameWindow.cpp` | `graphics/sdl3_window.cpp` |
| `Cursor_UnlockAllSurfaces` | `ui/GameWindow.cpp` | `()` — `input/Cursor.cpp` |
| `DDRAW_FileData_Dtor` | `network/NetHelpers.cpp` | `(FileData*)` — `native/ddraw_filedata.c` |
| `DDRAW_SpriteDataDtor` | `world/tilemap.cpp` | `(SpriteData*)` — `native/ddraw_spritedata.c` |
| `NETMAN_CreateSession` | ~~`ui/EditWindow.cpp`~~ | **Fixed (2026-08-08, netman-networkui-cast-cluster).** Real signature is `(NameEntryPanel*)`, not `(int)` — `ui/EditWindow.cpp:114`'s declaration and `native/NETMAN_NetworkUI.c`'s definition both retyped; `shared/defsym_stubs.cpp`'s now-dead `(void*)` stub removed. Verified live: `EditWindow::show()` calls this every time the main menu opens (every GUI test) — exposed a real, empirically-confirmed null-deref in the shared `_g_netman_data` singleton (never constructed on host), fixed with an `#ifndef _WIN32` guard, not swept under the rug. |
| `NETMAN_SendPacket` | `ui/EditWindow.cpp` (still open) | **Mostly fixed, from two independent sessions.** The canonical declaration (`network/Netman.h`) and its `GameConfig::GameConfig()`/`native/NETMAN_SessionSettings.c` call sites are fixed to `(GameConfig*)` (native-session-glue-cluster session); `native/NETMAN_NetworkUI.c`'s two call sites are separately retyped to `(unsigned char*)` (netman-networkui-cast-cluster session) — both inside a null-`_g_netman_data`-guarded block, not reachable from outside that file. Only `ui/EditWindow.cpp:123`'s separate `(void*)` declaration/call (`ui/EditWindow.cpp:792`) remains open: it's live and unguarded, and the real function does file I/O (`CreateFileA`/`WriteFile` to NetSettings.dat) — fixing it needs its own verified session, not a blind signature change riding along with either of these. |
| `UIPANEL_EndPaint` | `native/NETMAN_NetworkUI.c` | `(void*)` — `ui/UIPANEL.cpp` |
| `DirectPlay_Close`/`CreatePeer`/`DestroyPeer`/`HostSession`/`EnumConnections`/`ConnectToSession`/`QueryConnection` | `game/Train_network.cpp` (all 7) | **Investigated and reverted 2026-08-06 — see "DirectPlay_* cluster" section below. Do not attempt a mechanical linkage fix; there is a real prerequisite bug blocking it.** |
| `Train_HandleTrackBuild` | `game/Train_network.cpp` | `(void*, int)` — `town/Town.cpp` |
| `RESDATA_SoundObject_GetState`/`GetTextLength` | `ui/UIPANEL.cpp` | `(void*)` — `resources/ResourceManager.cpp` |
| `RESDATA_HitTestChildren` | `world/scriptengine.cpp` (@ wrong "0x44E6C0" — actually mid-`World_RenderAll`), `graphics/DDRAW.cpp` (@ wrong "0x44A0C0" — actually `RESDATA_ScriptedObject::HitTest` itself), `town/Town.cpp` (@ wrong "0x44B200" — actually `RESDATA_ScriptedObject::DtorChain`). All three plus `shared/stubs_impl.cpp:431`'s `void`-returning stub collide on one mangled symbol (Itanium mangling ignores return type) — every real call site binds to the stub's `assert(0)`. Real address **0x4549E0**, and a real typed C++ implementation already exists as `Panel::HitTestChildren` (`game/Panel.cpp:510`). **Not fixed** (2026-08-08, scriptengine-cpp-cast-cluster session) — `Panel::HitTestChildren` itself dispatches `child->vtable[0x11]` as a `void**` array index (byte offset 0x88 on this 64-bit host's 8-byte vtable entries, not the original x86 0x44), the same vtable-byte-offset-misalignment landmine this sweep watches for; wiring a forwarder to it without fixing that first would trade a loud `assert(0)` for a silent wild call through 3 live paths (`ScriptedObject::HitTest`, `DDRAW_Building::HitTestWithDrag`, `Town`'s click path) that no current test exercises. Needs slot 17's real signature resolved first (`game/ScriptedObject.h`'s own vtable-layout comment cites 0x44EF00 for slots [17]/[18], but Ghidra reports no function at that address). | `char (void*, int, int)` — real impl `Panel::HitTestChildren`, `game/Panel.cpp:510`, needs the vtable-stride fix above before forwarding to it |
| `GameObject_GetRelPos` | `game/Panel.cpp` (real caller, `Panel::HitTestChildren`), `town/Town.cpp`, `town/sdl3_town_mode3.cpp` | `shared/defsym_stubs.cpp:396`'s definition is a silent no-op (`{ /* host no-op */ }`, doesn't fill the output buffer) — same shape as the "silent-wrong-stub" class (not call-0: it has a body, just an empty one). Real implementation already exists, fully typed, as `GameObject::GetRelPos` (`core/GameObject.cpp:283`, `out[0]=x-screen_rect.left, out[1]=y-screen_rect.top`) — trivial to forward once someone picks this up; not fixed here because it was only a transitive dependency of the deferred `RESDATA_HitTestChildren` fix above, not itself on this session's touched call path. |
| `CGWND_TrackPiece_SetZoom` | `world/scriptengine.cpp` (8 sites, all pre-existing), `game/Panel.cpp`, `ui/UIPANEL_Draw.cpp` | `shared/stubs_impl.cpp:216`'s only definition is a loud `assert(0)` stub. Ghidra confirms every real `TrackPiece_SetZoom(...)` call `world/scriptengine.cpp`'s own disassembly resolves to targets **0x40D170**, which is the already-fully-implemented `TrackPiece::SetZoom` (`game/TrackPiece.cpp:266`) — a real `void (TrackPiece*, int16_t)`, not this stub's `(void*, int32_t)`. **Not fixed** (2026-08-08) — `TrackPiece::SetZoom` dispatches through ordinary compiler-managed virtual calls (no byte-stride risk like the `RESDATA_HitTestChildren` case above), so this one's likely safe to wire up, but doing so activates 3 files' worth of currently-dormant call paths at once (wider blast radius than a single-file session should take on without dedicated verification). | `void (TrackPiece*, int16_t)` — `game/TrackPiece.cpp:266`, address 0x40D170 |
| `UIPANEL_ScrollPanel_HandleDrag` | `world/scriptengine.cpp` (@ wrong "0x427BD0" — mid-body; Ghidra resolves to 0x4277D0), `game/ScriptedObject.cpp:90` (declared `(void*, int, int)`), `shared/stubs_impl.cpp:518` and `shared/defsym_stubs.cpp:105` (two more conflicting definitions, one with **zero parameters**) | Real entry **0x4277D0**. The `param` argument is a genuine pointer, not a small int: several call sites inside `RESDATA_ScriptedObject::HandleToolClick` (0x44A250) pass a raw child-list node pointer through it, and the real body stores it verbatim into a 4-byte slot at `this+0xD4` without ever treating it as an integer — declaring it `int32_t` truncates a real pointer on this 64-bit host. **Not fixed** — four declarations/definitions need auditing together, not just the one in `world/scriptengine.cpp`; retyping only one risks silently rebinding that TU's calls to a different symbol than the others use. |

### `RESDATA_ScriptedObject` (`world/scriptengine.h`) / `ScriptedObject` (`game/ScriptedObject.h`) — duplicate-class landmine, same shape as row 46 (2026-08-08)

Two independent, complete reconstructions of the same real object exist side
by side: `world/scriptengine.h`'s `RESDATA_ScriptedObject` (flat, raw-offset
fields, no real inheritance) and `game/ScriptedObject.h`'s `ScriptedObject :
public Panel` (real inheritance, named fields). Both target the identical
real class — same vtable address (0x4782A8), same singleton address
(0x4A99E0), same method addresses throughout (`Start` 0x449600, `Update`
0x4497A0, `Dispatch`/Draw 0x449C00, `IsDragging` 0x449CE0, `CheckClick`
0x449D00, `GetDragOffset` 0x449D80, `MoveTo` 0x449DC0, `HitTest` 0x44A0C0,
`HandleToolClick` 0x44A250) — and even cross-corroborate several field
offsets independently (`+0x1B0`/`+0x200`/`+0x298`/`+0x2E8` script-engine/
scroll-panel sub-object offsets and visibility flags match exactly between
the two headers).

`world/scriptengine.h`'s flat `RESDATA_ScriptedObject` is the one actually
wired into the live game: `core/Game.cpp:97` states outright "g_scripted_object
is canonically declared in world/scriptengine.h", and `g_scripted_object` is
called throughout `core/Game.cpp`, `core/GameLoop.cpp`, `world/tilemap.cpp`
as that type. `game/ScriptedObject.h`'s properly-inherited `ScriptedObject`
class appears to be orphaned — nothing constructs or references a live
instance of it anywhere in the tree (unlike row 46's `TownTileRenderer`,
which *was* being called live through `UIPANEL_Blit`).

This is backwards from the usual direction of these merges (the raw-offset
class is the live one; the properly-inherited class is the orphan), and the
blast radius of merging them is large: `core/Game.cpp`, `world/tilemap.cpp`,
`core/GameLoop.cpp`, and `world/scriptengine.cpp` itself all touch
`g_scripted_object` through the flat type's API. **Not attempted** in the
2026-08-08 scriptengine-cpp-cast-cluster session — flagged for a dedicated
follow-up, same as row 46 was. `game/ScriptedObject.h`'s independent field-
offset corroboration is still useful as a cross-check for anyone editing
`world/scriptengine.h`'s field layout, even though the classes shouldn't be
merged casually.

### DirectPlay_* cluster in `game/Train_network.cpp` — investigated, reverted, blocked (2026-08-06)

Attempted as a mechanical linkage fix like `UIPANEL_Blit`/`CGWND_SetMode`; it is
not one. Findings, so a future session doesn't redo this investigation:

**Linkage bugs confirmed real** (via Ghidra decompile/disassemble against
database `locoaudit`): the `extern "C"` block at `game/Train_network.cpp`
lines ~45–177 declares 9 symbols with wrong parameter types or a `void*`/`int`
mismatch against their real signatures. Two address annotations are outright
bogus: `0x461990`/`0x461A00` (comments claimed `DirectPlay_Close`/
`DirectPlay_DestroyPeer`) have no function at all in the binary; `0x45EDE0`
is actually `DirectPlay_EnumConnections`, and `0x45F050` is actually
`DirectPlay_GetSessionDesc` — both mislabeled in the existing comments.

**Why the mechanical fix doesn't work**: `network/DirectPlay.h`/`.cpp`
declare `DirectPlay_Close`/`DirectPlay_DestroyPeer`'s session handle as
`int32_t`, then internally cast it back to a pointer
(`(uint8_t*)(uintptr_t)session`) to walk a linked list of connection nodes —
a 32-to-64-bit pointer-truncation bug (same class as documented in
`landmine_bug_classes.md`), dormant only because every caller into this path
was call-0 (unreachable). Fixing the `Train_network.cpp` linkage makes
`TrainSubsystem::TrainSubsystem`'s constructor → `InitNetwork()` →
`DirectPlay_CreatePeer`/real `DirectPlay.cpp` reachable for the first time,
which crashes the moment `DirectPlay_Close`/`DirectPlay_DestroyPeer` truncate
a real 64-bit pointer. **Reproduced.**

Widening `session` to `void*` in both files fixes the crash (verified: 30/30
`meson test`, all 12/12 GUI integration tests pass on first entry into
single-player and initial multiplayer-menu navigation) but then a **second,
distinct regression** appears: re-entering multiplayer mode a second time
(which calls `InitNetwork()` again, destroying and recreating
`g_dplay_peer`) hangs — 7 of 12 GUI integration tests time out waiting for the
`menu_mode_selected` event, with the process alive but non-responsive, not
crashed. **Reproduced, not root-caused.** Every list node the widened
`DestroyPeer` walks is still typed `int32_t*` internally with `(uintptr_t)`
round-trips at each link-traversal step — the pointer-width problem is not
limited to the two parameters that got widened; it runs through the whole
connection-list traversal in `network/DirectPlay.cpp`. Widening two
parameters just moved where the mismatch bites.

**Verdict**: this cluster has a real prerequisite — the DirectPlay session/
connection-list field widths throughout `network/DirectPlay.h`/`.cpp` need a
full pointer-width audit (every `int32_t` that ever holds a pointer, not just
`session`) — before the `Train_network.cpp` linkage can be fixed safely. Do
not attempt this as a quick mechanical fix; it needs its own dedicated
session with Ghidra verification of every DirectPlay struct field's real
width and a test plan that specifically covers multiplayer session
re-entry (not just first entry), since that's exactly where the second
regression surfaced.

`network/sdl3_directplay_train_bridge.cpp` deliberately provides host-safe
no-op `void*`-typed overloads of `DirectPlay_CreatePeer`/`EnumConnections`/
`QueryConnection` for exactly these 3 of the 7 symbols; `Close`/`DestroyPeer`/
`HostSession`/`ConnectToSession` have no host-safe counterpart. Any real fix
must decide per call site whether it wants x86 fidelity (real `DirectPlay.cpp`,
pointer-width-audited) or host safety (a bridge overload) — and probably
needs the bridge extended to cover the other four before the real
implementation is safe to make reachable at all.

The investigation's changes (to `game/Train_network.cpp`,
`network/DirectPlay.h`, `network/DirectPlay.cpp`) were reverted via
`git stash` rather than committed, then given a durable branch name so the
stash can't be lost to `git stash clear`/pop-collision: `git checkout
wip/directplay-investigation`. A future session picking this up should
start there rather than redoing the Ghidra verification work described
above.

**Update (2026-08-08, directplay-cpp-cast-cluster session)**: `network/DirectPlay.cpp` also had a 310-site STRICT=2 (`-Dstrict=2`) diagnostic cluster (297 `old-style-cast`, 13 `zero-as-null-pointer-constant`, 1 `cast-qual`), fixed as a pure cast-respelling pass — no semantic change, `session` still `int32_t`, node traversal still round-trips through `int32_t`/`uintptr_t` exactly as before. Went in intending to try this session's own suggested strategy (split the bookkeeping lists into an `#ifdef _WIN32`-faithful path and a `#ifndef _WIN32` native-pointer host path, since CLAUDE.md already says x86 layout parity is a non-goal off-Windows) but an `advisor()` review, followed by a direct Ghidra + `nm` check against this worktree's own freshly-built `build/lego_loco.p/*.o` (not the shared checkout — see PROGRESS.md's directplay-cpp-cast-cluster entry for a worktree-hygiene near-miss along the way), found two concrete reasons the split doesn't unblock this cluster on its own, sharper than the 2026-08-06 "needs a pointer-width audit" framing:

1. **The `int32_t session` parameter genuinely is `self`** — Ghidra confirms `DirectPlay_Close` (0x45FC30), `DirectPlay_DestroyPeer` (0x45E5A0), `DirectPlay_EnumConnections` (0x45EAB0), and `DirectPlay_GetSessionDesc` (0x45EEC0) are all `__fastcall` with the ECX-passed parameter used purely as a base address, never arithmetically — the same role as the `void* self` the other DirectPlay functions already use. But retyping it to a pointer changes the C++ mangled name. `nm` on this file's own object shows it defines `_Z16DirectPlay_Closei` (taking `int`); `town/Town.cpp:2899` independently declares/calls `void DirectPlay_Close(void* peer)`, which mangles to `_Z16DirectPlay_ClosePv` — a symbol nothing currently defines (undefined reference, tolerated only by `meson.build`'s `-Wl,--unresolved-symbols=ignore-all`, itself an intentionally-deferred separate LINK-001 item). Retyping `session` to a pointer would make `DirectPlay.cpp`'s real implementation's mangled name land exactly on `_Z16DirectPlay_ClosePv` — silently making `Town.cpp`'s call newly reachable into a real implementation whose internal list traversal still truncates, exactly the failure mode the 2026-08-06 session already hit, but arriving as a side effect of a "just widen the parameter" change rather than a linkage fix anyone would think to test for reachability.
2. **The connection-list node layout (the one list this file fully owns, create-to-destroy) still has an external consumer.** `game/Train_network.cpp:284` walks `DirectPlay_EnumConnections`'s return value as `uint32_t* item` (`item[0]`/`item[1]`) — dormant only because that call site currently binds to `network/sdl3_directplay_train_bridge.cpp`'s no-op overload (different mangled name, `_Z26DirectPlay_EnumConnectionsPv` vs. the real `_Z26DirectPlay_EnumConnectionsi`), the same mangled-name-coincidence risk as above rather than a genuinely independent list.

Also newly found and left as-is (out of scope for a cast-respell, and unreachable today via the same linkage chain): `DirectPlay_SetSessionDesc` and `DirectPlay_EnumPlayers` both dereference four fixed absolute addresses (`0x479158..0x479164`, the original binary's own `.rdata` GUID bytes) directly, unguarded by `_WIN32` — a distinct defect from the pointer-truncation bug (a read of unmapped host memory if ever reached, not a width mismatch), same call-0-today safety margin.

**Verdict, updated**: still blocked on the same prerequisite as 2026-08-06 (a cross-file linkage reconciliation across `Train_network.cpp`/`Town.cpp`/`shared/link_stubs.cpp`/the SDL3 bridge, needed *before* any retype), but the blocker is now characterized precisely (mangled-name coincidence deciding reachability, not an abstract width audit) rather than deferred as "needs its own session" in the abstract. `network/DirectPlay.cpp`'s STRICT=2 cluster itself is fixed (310 → 0); full verification (`meson test` 30/30, `--suite integration` ×3, `call 0` unchanged at 272, mingw typecheck clean) in PROGRESS.md's directplay-cpp-cast-cluster entry.

**Update (2026-08-10, DirectPlay class-conversion session) — cluster RESOLVED, branch `wip/directplay-class-conversion`**: the reachability prerequisite above is what a real receiver-type fix looks like, not a blocker to route around. `session`/`self`/`peer` retyped to a real `DirectPlaySession*` throughout `network/DirectPlay.h`/`.cpp`, and the free-function cluster converted to 10 typed `DirectPlaySession::` methods (`CreatePeer`, `CreateAddress`, `DestroyPeer`, `HostSession`, `ConnectToSession`, `EnumConnections`, `FindLocalModemName`, `SetSessionDesc`, `Close`, `OpenSession`). Every real caller (`game/Train_network.cpp`, `town/Town.cpp`) migrated to call the methods directly — no shims, no `#ifdef _WIN32` bridge overloads; `network/sdl3_directplay_train_bridge.cpp` (the 2026-08-06 partial no-op bridge) and `shared/link_stubs.cpp`'s 8 decoy `DirectPlay_*` no-ops are deleted outright now that nothing binds to them. The connection-list traversal `game/Train_network.cpp:284` flagged above as an external consumer of the raw `uint32_t* item[0]/item[1]` layout is now a typed `DirectPlayConnectionNode*` walk (`item->next`/`item->type`); the mangled-name-coincidence hazard is moot because there is exactly one mangled name per method now, not two competing free-function bindings.

Along the way, three real transcription bugs were caught and fixed against Ghidra disassembly (not just decompiler pseudocode): `Ordinal_1` was called with 4 nullptrs, discarding the real service-provider GUID (now the correct 3-arg `(const GUID*, void**, int32_t)` shape); `ConnectToSession`'s `CreatePlayer` call passed a lone int where a `DPNAME` struct was required; `OpenSession`'s `dwFlags` formula read the wrong field (`is_host`) with an inverted condition (now `startup_flag`, matching 0x45FD80). `EnumConnections`'s COM1-4 device-probe loop, silently dropped in the prior TRANSCRIBED pass, is reconstructed and byte-verified GUIDs added for IPX/TCP-IP/Serial/Modem. A genuine pre-existing crash (`g_device_path_null` declared, never defined) and a genuine pre-existing linkage bug (`Train_network.cpp`'s stray `extern "C"` on `DirectPlay_HandleMessages`) were also fixed; the `windows.h`/`windows_types.h` split (types-only header for `stubs/dplay.h` to include, function declarations kept in a thin wrapper) resolved the Win32-declaration-conflict cascade that migrating the callers exposed for the first time.

Verified: `nm -C` diff of `network_DirectPlay.cpp.o` shows exactly the expected shape (10 `extern "C"` free functions dropped, 10 `DirectPlaySession::` methods + 1 lambda + `initializer_list` helpers added, 0 undefined symbols on either side of the diff). `call 0` census dropped 258 → 251 (fixed sites, no new ones). `ninja -C build-mingw -k 0` error count dropped 493 → 481 with zero new distinct error lines (diffed against a fresh `main` worktree build). `meson test -C build`: 27/30 pass — the 2 mdns/discovery failures are the pre-existing sandboxed-netns environmental limitation (`Error: Unknown device type`, unrelated to this change), and 6 of 13 GUI integration subtests pass (intro, main-menu-exit, singleplayer-go, singleplayer-accept-mode3, singleplayer-mode3-mouse-input, main-menu-escape).

**New finding, resolved same session**: 7 multiplayer GUI subtests initially failed. Root cause: the multiplayer button's `has_scenario` gate reads `*(int32_t*)(g_netSettings+0x10)`, populated by `TrainSubsystem`'s constructor from `EnumConnections`'s (now-correct, reversed) connection list. On this host (no DirectPlay DLL, no COM devices), `EnumConnections`'s host branch returned false unconditionally for every provider, so the field stayed null and `has_scenario` was always false — the button was correctly, but unhelpfully, always inert. The pre-existing `int32_t`-truncating code left a **non-deterministic garbage pointer** there instead (793460400 and 544632960 observed across separate runs), which happened to read as nonzero and made the button clickable; the GUI test suite's `has_scenario`/button-enable expectation was built against that undefined-behavior artifact, not real original-game behavior.

Fixed by distinguishing "is a provider installed" (what `Ordinal_1`+`QueryInterface` actually test — real DirectPlay's TCP/IP provider registers unconditionally once Winsock is present, it never probes connectivity) from "is there an active network link" (what the host branch was previously conflating it with). The host build always ships an SDL_net-backed TCP transport (`network/sdl3_net_transport.cpp`) as the native equivalent of the TCP/IP provider, so `EnumConnections`'s host branch now reports that provider present unconditionally (matching how the original Windows TCP/IP provider normally behaved), while IPX and Serial — which have no host-native equivalent — stay reported unavailable. Verified: `meson test -C build --suite integration` now passes all 13 GUI subtests; `meson test -C build` 28/30 (same 2 pre-existing sandboxed-netns failures); `call 0` unchanged at 251 (pure logic change); `ninja -C build-mingw -k 0` error set unchanged (481, diffed). Second commit on `wip/directplay-class-conversion`.

## Functions excluded from automated alignment (call-count mismatch)

These functions have a `call 0` landmine per the original per-function census
but the `.o`-vs-final-binary call-sequence alignment didn't line up 1:1
(likely an inlining/ICF artifact at `-O0`, or a locally-static helper sharing
the mangled name). They need the same symbol-recovery treatment by hand:
`objdump -dr build/lego_loco.p/<file>.cpp.o` on the specific function, matched
against `objdump -d build/lego_loco` for the same function, reading the
relocations directly instead of relying on positional alignment.

`BuildingDescriptorEditor::parse_dat_directive_line`, `TrainStationWindow::hide`,
`TrainStationWindow::show`, `Netman::ProcessMessage`, `CGWND_ValidatePaletteData`,
`TrainStationWindow::Create`, `Town::handle_tile_click`,
`ResourceManager::AddString`, `Netman::HandlePlayerLeave`,
`DirectPlay_EnumConnections`, `ScriptedObject::EnterBuildMode`,
`UIPANEL::HandleDrag`, `Town::save_received_postcard`,
`Netman::ReceivePlayerName`, `Netman::ProcessPlayerData`,
`Train_ConnectToServer`, `TrainSubsystem::HandleJoinMultiplayer`,
`BuildingPanel::draw_occupant_dots`, `BuildingMgr::RemoveObject`,
`BuildingMgr::UpdateAll`, `Game::HandleLeftClick`, `DDRAW_SpriteDataDtor`,
`GameWindow::hide`, `Town::save_postcard_as`,
`Town::delete_postcard`, `Town::load_postcard`, `Town::save_postcard`,
`Town::receive_postcard`, `Netman::ReceiveGameStart`, `Netman::LoadScenario`,
`DirectPlay_GetSessionDesc`,
`PostcardAlbum::InitSprites`, `PostcardAlbum::InitWindowSurface`,
`(anonymous namespace)::sprite_contains` (its PtInRect call is done — check
for other call-0 sites in the same function), `TrainSubsystem::ProcessMessages`,
`ScriptedObject::MoveTo`, `Building::CheckPlacementCollision`,
`Building::RemoveOccupant`, `Building::AddOccupant`, `Game::UpdateInputState`,
`Town::on_lbutton_down` (PtInRect sites done — check for other call-0 sites
remaining in this function).

## `Train_network.cpp` raw-offset/host-layout mismatch (2026-08-08)

Found while doing the STRICT=2 old-style-cast cluster fix for
`game/Train_network.cpp` (see PROGRESS.md's `train-network-cpp-cast-cluster`
entry for the full writeup). Recorded here, separately from that commit,
because fixing it is out of scope for a cast-respelling pass and needs its
own dedicated session — same shape as the DirectPlay_* entry above.

**The finding**: `game/Train_network.cpp` accesses both `Vehicle*` objects
(via `sprite_list_1/2/3`, `car`/`node`/`tail`/`train`/`controller` locals) and
the `Netman*` singleton (`g_netman`) almost entirely through raw byte-offset
arithmetic hardcoded to the *original x86* field offsets documented as
comments in `Vehicle.h`/`Netman.h` (`+0x70`, `+0x7C4`, `+0x518 + j*0x4C`,
etc.), rather than through the named C++ fields those headers already
declare. On this 64-bit host, several pointer fields in both classes are
native-width (8 bytes) where the original x86 layout used 4-byte pointers —
`Vehicle::editors[4]` and `Vehicle::editor_state` before the `+0x70` union;
`PlayerSlot::msg_queue`/`pixel_buffer` inside `Netman::m_slots[9]`. That
widening pushes every real (compiler-computed) offset after those fields
forward, so the hardcoded x86 offsets no longer point at the fields they're
named for. Verified directly with `static_assert`/`offsetof` in a scratch
translation unit against the real headers:

```cpp
static_assert(sizeof(PlayerSlot) == 0x4C, ...);              // fails: actual 88
static_assert(offsetof(Netman, m_slots) == 0x518, ...);      // holds: 1304 (no pointer fields before it)
static_assert(offsetof(Netman, m_gameMode) == 0x7C4, ...);   // fails: actual 2104 (0x7C4 = 1988)
static_assert(offsetof(Netman, m_mySlotIndex) == 0x7D0, ...);// fails: actual 2120 (0x7D0 = 2000)
```

So every raw `g_netman + 0x7C4`/`+0x7D0`/`+0x518 + j*0x4C` site in
`Train_network.cpp` reads/writes the wrong host memory today, and the same
applies to every raw `Vehicle*` + `0x70`..`0x8A` site. This is (almost
certainly) currently dormant: `TrainSubsystem`'s whole multiplayer call
chain is gated behind `InitNetwork()` → `DirectPlay_CreatePeer`, which the
DirectPlay_* entry above already established is call-0/unreachable in the
shipped binary, so nothing exercises these reads/writes yet. `meson test`
(30/30 incl. 12/12 GUI integration) passing before and after the cast fix
is consistent with "dead code," not with "verified correct."

**Why it wasn't fixed this session**: `Vehicle.cpp`/`Building.cpp`/`World.cpp`
already converted their own raw-offset sites to named-field access
specifically because doing so is *safe* there (the compiler-computed offset
is simply correct, whatever it is). Converting only *some* of
`Train_network.cpp`'s sites the same way — while others (necessarily, since
grepping the raw hex offsets tree-wide returns mostly unrelated structs and
can't reliably distinguish every remaining site's base type) keep the old
byte arithmetic — would silently split live traversal of `sprite_list_1/2/3`
or `g_netman` across two different addresses. That is a strictly worse bug
than the current uniformly-wrong-but-dead byte arithmetic. Fixing it for
real needs: (1) a full per-site audit of every `Vehicle*`/`Netman*` base
variable in this file (tracing assignment origin, not variable name) to
confirm none are actually `PlayerConnectionNode*` or something else; (2)
resolving the `network/Netman.h` `#include` collision below, since named
`Netman*` field access requires the complete type; (3) a test plan that
actually exercises the converted paths (today's 30/30 + 12/12 don't, since
the code is dead) to catch any dormant behavioral bug the conversion fixes
or introduces once `InitNetwork()` becomes reachable.

**Adjacent, smaller finding — `#include "network/Netman.h"` collision**:
`Train_network.cpp` declares its own local `extern "C"` prototypes for
several symbols that `Netman.h` also declares, with different signatures/
linkage: `CreateFileA` (`int __stdcall` here vs. `HANDLE`/`void*` in
`Netman.h` and ~6 other TUs — see the `CreateFileA` finding just below),
`DPLAY_CreatePlayer`, `DPLAY_CopyPlayerData`, `DPLAY_DecodePlayerSlots`,
`NET_RegisterPlayer`, `VehicleEditor_SetDPlayData`, `Config_GetIniInt`,
`CRT_rand`, `CRT_itoa`, and a `g_resmgr` global with a conflicting type.
Simply adding `#include "../network/Netman.h"` to get the `Netman` class
definition (needed for any named-field fix above) trades one
`-Werror=missing-declarations` site for ~10 `-Werror` ambiguating/
conflicting-declaration errors. Whoever picks up the `Netman`/`Vehicle`
host-offset fix above will need to reconcile these declarations first (or
work around them the way this session did for the one symbol it needed —
`Train_QueueMessage` — by forward-declaring it locally instead of
including the header).

**`CreateFileA` return-type mismatch** (its own small, separate finding):
declared `int __stdcall CreateFileA(...)` in `game/Train_network.cpp` and
`int32_t __stdcall CreateFileA(...)` in `network/DirectPlay.cpp`, but
`HANDLE`/`void* __stdcall CreateFileA(...)` (the correct type — `HANDLE` is
`typedef void*` in `stubs/windows.h`) in `game/PlayerConfig.cpp`,
`town/Town.cpp`, `native/NETMAN_SessionSettings.c`,
`network/NetworkPlayerList.cpp`, and others. Since this is `extern "C"`,
the linker resolves all call sites to one real definition returning a
64-bit `HANDLE`; the two files declaring `int`/`int32_t` only read the low
32 bits of the return value at their call sites, silently truncating any
handle value that doesn't fit in 32 bits. Same "call-0/signature-mismatch"
landmine class as the rest of this document, scoped as its own future
"silent-wrong-stub cluster" fix (grep every `CreateFileA` declaration
tree-wide, pick the correct `HANDLE` signature, fix the two wrong ones,
verify call sites) rather than folded into the cast-cleanup commit that
found it.

## Update 2026-08-08 (town-cpp-strict2 session — partial, blocked by a shared-tree collision)

Started as a `town/Town.cpp` STRICT=2 old-style-cast cleanup (319 errors).
Found and fixed one real "silent-wrong-stub" landmine in the same family as
this document's other entries; the rest of the Town.cpp cast sweep was
**not completed** — see the note at the bottom of this section for why, and
`docs/town-strict2-wip.patch` (untracked, not committed) for the salvaged diff.

**`Town_CheckOccupied`/`Town_CheckOccupiedEx`/`Town_BlitViewport`
(0x42C950/0x42C9F0/0x42CB10) — FIXED.** These were transcribed into
`town/Town.cpp` as `Town::check_occupied`/`_ex`/`blit_viewport` **member
functions**, even though `Town.h`'s own prior doc comments already said
`this` in those methods was NOT the Town instance. Ghidra `get_xrefs_to`
proved they're free functions: the only callers are
`BuildingMgr::InvalidateRects`/`BlitOverlaps` (`game/BuildingMgr.cpp`,
0x435020/0x435200) and `World::ProcessEvents` (`game/World.cpp`, 0x44E3F0)
— none of which are Town methods — and every one of those call sites
passes `*(void**)(entity+0x40 + 0x10)` as the receiver: the RESDATA-
embedded "ui_panel" alias (`shared/types.h`'s `RESDATA::flags`/+0x10
comment), i.e. a `UIPANEL_Surface*`, never a `Town*`. Both caller files
already declared+called the correctly-shaped free functions
(`game/BuildingMgr.cpp`'s `Town_CheckOccupied(void* self, int,int,int,int)`,
`game/World.cpp`'s `Town_BlitViewport(void* viewport, ...)`) against a
symbol that was **never defined anywhere with that shape** — same
mangled-name collision this document tracks elsewhere: they silently bound
to `shared/defsym_stubs.cpp`'s host no-ops (`void Town_BlitViewport(void*,
int,int,int,int,int,int){}`, `void Town_CheckOccupied(void*,int,int,int,
int){}`) instead of running any real logic. **Not a `call 0` site** — the
`objdump -d build/lego_loco | grep -cE "call\s+0 "` count is unchanged at
272 before/after, exactly as expected for a silent-wrong-stub fix rather
than an unresolved-symbol fix. Verified via `objdump -d -C`: both real call
sites (`BuildingMgr::InvalidateRects`/`BlitOverlaps`, 3 sites;
`World::ProcessEvents`, 1 site) now resolve to the real
`Town_CheckOccupied(UIPANEL_Surface*, int, int, int, int)` /
`Town_BlitViewport(UIPANEL_Surface*, int, int, int, int, int, int)`
symbols, not the `defsym_stubs.cpp` no-ops. Fixed by moving the three
functions' real implementations to `town/TownTiles.cpp` (beside
`UIPANEL_Surface`'s other address-adjacent methods), retyping their
receiver to `UIPANEL_Surface*`, adding the canonical declaration to
`graphics/LOCOBITMAP.h`, and retyping `game/BuildingMgr.cpp`'s
`entity_surface()` / `game/World.cpp`'s call site to match. Deleted the
now-fully-dead `Town::check_occupied`/`_ex`/`blit_viewport` member
functions from `town/Town.h`/`Town.cpp` — nothing in the tree ever called
them as methods. **Leftover, not fixed**: `world/tilemap.h:494` still
declares a third, differently-wrong-signature
`extern int Town_BlitViewport(void* res, int src_x, int src_y, ...)` —
currently unused (no caller in `world/tilemap.cpp`), so not a live
landmine, but worth deleting whenever that header is next touched.
`ninja -C build`: clean. `meson test -C build`: 30/30. `meson test -C build
--suite integration`: 1/1 (12/12 GUI cases). Confirmed no link collision
from moving `g_primary_surface_desc` into a second anonymous-namespace
(one in `Town.cpp`, one now in `TownTiles.cpp`) via `nm`: both mangle to
distinct internal (`b`, lowercase) `_ZN12_GLOBAL__N_1...` symbols, as
expected for anonymous-namespace internal linkage — no ODR conflict.

**`UIPANEL_CreateSurface` in `town/Town.cpp` — still open** (this document's
existing row above already lists `town/Town.cpp` as a wrong-signature
caller; this session found the specific bug, didn't fix it). The local
declaration carried a bogus address (`0x42AF30`, which Ghidra resolves to
`UIPANEL_ReadPaletteFromBMP`, an unrelated function) and the wrong
signature (`void* UIPANEL_CreateSurface(void*)` vs. the real
`void UIPANEL_CreateSurface(UIPANEL_Surface*)` at `0x42A110`). Ghidra
decompilation of `Town::handle_tile_click` (0x42CE10) shows the ORIGINAL
disassembly itself already has this shape: `pvVar4 = (void
*)UIPANEL_CreateSurface(puVar5);` treating a genuinely `void`-returning
`__fastcall` function's leftover-register value as if it were a return —
almost certainly a decompiler artifact (the real intent is "allocate
`puVar5`, initialize it in place via `UIPANEL_CreateSurface`, store
`puVar5` itself into `this->overlay_panel`", not "store the call's
return value"). Needs its own fix inside the wider Town.cpp cast sweep;
not attempted here since Town.cpp's edits didn't survive this session (see
below).

**`UIPANEL_Surface::CalcScrollRect`/`CalcScrollRect_Reversed`
(0x42C590/0x42C700) deferral — upheld, with new corroborating evidence.**
This document's `Town_Draw*` entry above already hedges that the
unresolved `unaff_EBX`/`unaff_EBP` stack artifacts "may just be the
decompiler losing track of the SAME RECT pointer". This session found a
fresh Ghidra decompile of 0x42C590 that weakens that hedge: the body
dereferences `ptStack_4->left` and `ptStack_4->top` — a **distinct**
pointer from the tracked `param_1` (pClipRect), read *before* the
unresolved `unaff_EBX`/`unaff_EBP` values are used to build
`RStack_b4.right`/`.bottom`. A separate, fully-implemented (non-stub) copy
of this logic existed in `town/Town.cpp` as dead, uncalled
`Town::calc_scroll_rect`/`_reversed` methods (no caller anywhere in the
tree ever invoked them) — its transcription had silently assumed
`ptStack_4 == param_1`, collapsing two Ghidra-distinct pointers into one
with no supporting evidence. That dead code was deleted rather than
promoted onto `UIPANEL_Blit`'s live `flags & 0x40` path; `TownTiles.cpp`'s
existing loud `assert`-stubs for both functions are still the honest,
correct state. Whoever picks this up next needs to identify what
`ptStack_4` actually is (a genuine 3rd distinct RECT parameter, most
likely) before implementing either function for real.

**Two duplicate-class-definition blockers found, own cluster, not
fixed**: (1) `class PostcardAlbum` is defined twice with different shapes
— `ui/PostcardAlbum.h` (`Status: INTEGRATED`, extends `UI_WindowBase`) and
`graphics/LOCOBITMAP.h` ("concept A", the file's own header comment already
flags the ambiguity). Including `graphics/LOCOBITMAP.h` from any TU that
already includes `ui/PostcardAlbum.h` (e.g. `town/Town.cpp`) is a hard
redefinition error. (2) `Sprite_Destroy(void*)` is declared `extern "C"` in
`ui/ButtonSprite.h` (matching its real definition) but plain C++-linked in
`network/Netman.h` — a live linkage-mismatch landmine on its own (any TU
using `Netman.h`'s declaration silently binds to a stub instead of the
real symbol), and it also means any TU that includes both `Netman.h` and
`graphics/LOCOBITMAP.h`/`ui/ButtonSprite.h` (e.g. `game/World.cpp`) gets a
hard "conflicting declaration ... with 'C' linkage" compile error. Worked
around locally in `game/World.cpp` (forward-declared `UIPANEL_Surface`
instead of including `LOCOBITMAP.h`) rather than fixing `Netman.h`'s
linkage, which is its own scoped fix.

**Session note — shared-tree collision cost most of this session's Town.cpp
work.** Another concurrent session was active in the same working tree
(visible via its own uncommitted `game/GameVehicle.h`/
`game/ResdataGameVehicle.h`/`world/EditorState.h`/`world/scriptengine.cpp`
changes appearing and disappearing independently of this session's edits).
At some point a `git stash` (not this session's own — this session's own
stash/pop pair completed and was verified clean before this happened) swept
up this session's in-progress edits to `game/BuildingMgr.cpp`,
`graphics/LOCOBITMAP.h`, `town/Town.cpp`, `town/Town.h`, and
`town/TownTiles.cpp` together with the other session's unrelated
`world/scriptengine.cpp`/`.h` changes, silently reverting all five files
to `HEAD` in the working tree. Recovered the five files' content via
`git checkout stash@{0} -- <path>` (per-file, not a full `stash pop`, to
avoid touching `world/scriptengine.*` or conflicting with the other
session's live edits) — `stash@{0}` is left in place, undropped, in case
the other session still needs it. `town/Town.cpp`/`town/Town.h` were then
reverted back to `HEAD` deliberately (via `git restore`, not `git
checkout`) once recovered, since a file this large can't be committed in a
half-converted state and this session ran out of safe runway to finish the
remaining ~280 cast sites — the recovered diff is saved as
`docs/town-strict2-wip.patch` (untracked, not committed — apply with `git
apply docs/town-strict2-wip.patch` then delete it) for the next session,
which should run in an isolated `git worktree`, not this shared tree, to
avoid a repeat.

## ArrivalQueue_AddVehicle/RemoveVehicle — self identified as HelpPageNode (2026-08-09)

**Note on this session's starting point**: this session was briefed to read
an existing "ArrivalQueue_AddVehicle/RemoveVehicle — self's real class
unidentified, deferred (2026-08-09)" section in this file first. That
section does not exist on this branch (checked via `git log` and grep for
`ArrivalQueue`/`occupation_level`/`0x124`/`queue_head` — no match, and the
worktree's HEAD is `00285a68`, "Finish STRICT=2 old-style-cast cleanup").
It likely exists only in a different, not-yet-merged worktree from the
same investigation. This session re-derived the type identification from
scratch using the struct-layout facts given directly in the task prompt
(`Building` static_asserts at `sizeof==0xF4`, `occupation_level`
offset `0x88`; `BuildingMgr.cpp` allocates `operator_new(0xF4)`) plus fresh
Ghidra evidence below. `Building.h`'s asserts and `BuildingMgr.cpp`'s
allocation size were **not** touched.

### Evidence chain

1. **`self` is not `Building`.** Disassembly of `ArrivalQueue_AddVehicle`
   (0x44F3A0) shows `MOV EAX, dword ptr [EDI+0x88]` — a full 4-byte dword
   read at `+0x88`. `Building::occupation_level` at that offset is
   `uint8_t` (confirmed by `game/Building.h`'s own `static_assert`), so a
   dword read there would pull in 3 bytes of unrelated padding/`disabled`/
   `_pad_8a` — inconsistent with a real field access. `self` must be a
   different, unrelated class that happens to place a real dword-sized
   field at the same offset.

2. **Two real callers, three call sites, dispatched via `TileMap_GetObjectAt`
   / `INPUT_FindObjectAt`.** `get_xrefs_to` on both addresses plus
   decompilation of all three caller functions (`World_FinalizeLoad`
   0x44DF40, `VehicleEditor_Update` 0x44C3A0 — not yet ported into the C++
   tree, `World_RenderAll` 0x44E630) shows `self` always comes from either
   `TileMap::GetObjectAt` (no type filter) or `INPUT_FindObjectAt` in
   mode ∈ {0,1,4}.

3. **`INPUT_FindObjectAt`'s mode-0/1/4 filter uniquely identifies the
   producing constructor.** That branch (0x41E1F0) keeps only entities
   where `resource->object_type == 3` **and** `entity->vehicle_kind
   (+0x10C) == 3` **and** `mode==4 || entity->+0x120 == mode`. Chasing
   every constructor that can set `vehicle_kind`:
   - `GameVehicle::GameVehicle` (0x412870) unconditionally forces
     `vehicle_kind = 4` after the base constructor runs — never 3.
   - `RESDATA_GameVehicle::RESDATA_GameVehicle` (0x44AE80) sets
     `vehicle_kind = 3` only inside its `RESDATA_IsRoadTile` branch — but
     `INPUT_PlaceObject` (0x41DD80) never reaches a *bare*
     `RESDATA_GameVehicle` for road tiles; it special-cases them.
   - `INPUT_PlaceObject`'s own dispatch (decompiled): building tiles →
     `operator_new(300==0x12C)` + `GameVehicle::GameVehicle` (matches
     `sizeof(GameVehicle)==0x12C`); road tiles → `operator_new(0x128)` +
     the constructor at **0x44F210** (Ghidra-labeled `HelpWnd_FindPage`,
     a stale label collision); everything else (pedestrian/other) →
     `operator_new(0x11C)` + bare `RESDATA_GameVehicle::RESDATA_GameVehicle`.
   - `get_xrefs_to 0x44F210` confirms `INPUT_PlaceObject` (0x41DE43) is its
     **only** caller.
   - Disassembly of 0x44F210 shows it chains to `RESDATA_GameVehicle_Ctor`,
     then unconditionally sets `vehicle_kind (+0x10C) = 3`, zeros
     `+0x11C`, and — based on `resource_id ∈ {0xC42,0xC44,0xC46,0xC48}` —
     sets `+0x120` to 1 or 0, and `+0x124` to 0. `0x11C + 0xC == 0x128`,
     matching the allocation size exactly, with `+0x124` (the queue head
     the caller reads) landing as the *last* field, ending precisely at
     the allocation boundary.
   - This constructor (0x44F210), both destructors (0x44F2A0/0x44F2C0),
     `Update` (0x44F340), `AddVehicle` (0x44F3A0), and `RemoveVehicle`
     (0x44F410) are one unbroken address range — consistent with a single
     1998 MSVC translation unit emitting one class's members, and matching
     an **already-integrated** class in the tree: `ui/HelpPageNode.h`/
     `.cpp` (`class HelpPageNode : public RESDATA_GameVehicle`, vtable
     `0x4783D8`, exactly `0x128` bytes, with `update_flag`@+0x11C,
     `overlay_flag`@+0x120, `dest_list_head`@+0x124 already documented at
     the right offsets — it just didn't have `AddVehicle`/`RemoveVehicle`
     as methods yet).
   - `RESDATA_GameVehicle::tile_target()` (`game/ResdataGameVehicle.h`)
     already documents the dword at `+0x88` as the packed
     `sub_pos_x`/`sub_pos_y` tile position — exactly the field
     `ArrivalQueue_AddVehicle` reads, resolving point 1 above without
     touching `Building.h`.

4. **`World_RenderAll`'s `RemoveVehicle` call site has weaker evidence** —
   noted explicitly rather than glossed over. It reaches the object via
   `TileMap::GetObjectAt`, which applies **no** kind filter at all (unlike
   `INPUT_FindObjectAt`). The typing rests on `vehicle->direction == 2`
   being set **exclusively** by `HelpPageNode::AddVehicle`, not on a
   runtime type check — i.e. by construction, any vehicle whose direction
   reads 2 here was queued by that exact method. A bare
   `RESDATA_GameVehicle` (0x11C bytes, no room for `+0x124`) reaching this
   call would read out of its own allocation; that would be an
   **original-binary hazard**, not something introduced or fixed by this
   session — the assembly does not guard against it either.

5. **`World::FinalizeLoad`'s `mp_gameMode==2` branch is the weakest link**
   — it also uses `TileMap::GetObjectAt` directly (no kind filter), so
   whatever is on that tile is passed to `AddVehicle` unconditionally, same
   as the original binary does. Typed as `HelpPageNode*` for consistency
   with the other two (proven) branches rather than introduced as a new
   special case, since the original funnels all three into the same call.

### Ruled out

- **`Building`** — ruled out by the dword-vs-byte mismatch above (point 1);
  `Building.h`'s `static_assert`s and `BuildingMgr.cpp`'s
  `operator_new(0xF4)` were left untouched, as required.
- **`GameVehicle`** — a plausible early guess (it already has a
  `dest_list_head` at `+0x124`, and `game/World.cpp` had it pre-existing at
  the `dest_building`/`TileMap_GetObjectAt` call sites for
  `GameVehicle::RemoveDestination`) but ruled out: its constructor forces
  `vehicle_kind=4`, never 3, so it can never satisfy
  `INPUT_FindObjectAt`'s mode-0/1/4 filter. `GameVehicle::AddDestination`
  (0x412AF0) / `RemoveDestination` (0x412B50) are real, structurally
  similar, but **distinct compiled functions** at different addresses from
  `ArrivalQueue_AddVehicle`/`RemoveVehicle` — `AddDestination` does not
  prime the vehicle (direction/tile position/state) the way
  `ArrivalQueue_AddVehicle` does. Not merged, per CLAUDE.md ("do not
  simplify assembly unless equivalence is proven and documented").
  `World_RenderAll`'s pre-existing `dest_building`
  (`GameVehicle::RemoveDestination`) call site was **left untouched** —
  it's a genuinely different call, not part of this fix.
- **A new shared base class for `+0x11C`/`+0x120`/`+0x124`** — considered
  and rejected. `GameVehicle` is `RESDATA_GameVehicle` (0x11C) + 0x10;
  `HelpPageNode` is `RESDATA_GameVehicle` + 0xC. Both derive directly from
  `RESDATA_GameVehicle` with their own distinct vtables (0x477848 vs
  0x4783D8) and no intermediate base exists in the binary — introducing
  one would be inventing hierarchy the binary doesn't have.

### Conversion made

- Added `HelpPageNode::AddVehicle(Vehicle*)` (0x44F3A0) and
  `HelpPageNode::RemoveVehicle(uint16_t, uint8_t)` (0x44F410) as real,
  non-virtual methods (`ui/HelpPageNode.h`/`.cpp`) — both are direct calls
  in the binary, not vtable dispatch.
- Retyped `HelpPageNode::dest_list_head` from `int32_t` (previously stored
  a pointer via manual `uint32_t`/`reinterpret_cast<uintptr_t>` round-trips
  — truncates real 64-bit addresses on a 64-bit host) to a proper nested
  `HelpPageNode::DestNode*`, mirroring `GameVehicle::DestNode`'s existing
  pattern. Updated the constructor, destructor, and `Update()` (all of
  which already touched this field) to use it directly; removed the old
  `DestinationNode32` helper struct, which itself had a landmine (`next`
  stored as `uint32_t` instead of a pointer).
- Deleted `game/ArrivalQueue.{h,cpp}` entirely (the free functions, the
  `extern "C"` block CLAUDE.md flags as a C++-methods-in-C-linkage
  anti-pattern, and the raw `VEHICLE_OFFSET_*` macros it used instead of
  `Vehicle.h`'s already-named fields).
- Updated `game/World.cpp`'s two real call sites (`World::FinalizeLoad`,
  `World_RenderAll`) to call the typed methods; removed the stale
  `extern "C"`-mismatched local declarations. `VehicleEditor_Update`
  (0x44C736/0x44C84A), the other two binary callers, is not yet ported
  into the C++ tree at all — nothing to update there this session.
- Removed both now-orphaned stub definitions
  (`ArrivalQueue_AddVehicle`/`ArrivalQueue_RemoveVehicle`) from
  `shared/defsym_stubs.cpp`.
- Added `HelpPageNode(const HelpPageNode&) = delete;`/`operator=` (the
  retyped pointer member tripped `-Weffc++` at `-Dstrict=2`; `GameVehicle`/
  `RESDATA_GameVehicle` have the same latent pattern but were left
  untouched — out of this session's file scope).
- **Not renamed**: `HelpPageNode`'s name is very likely a misnomer.
  `get_xrefs_to 0x44F210` proves `INPUT_PlaceObject` is its **only**
  construction site — reached for every ordinary road/junction tile placed
  during normal play (`RESDATA_IsRoadTile`), not just tutorial content.
  `overlay_flag` (+0x120) is 1 only for the specific tutorial resource IDs
  (0xC42/0xC44/0xC46/0xC48); every other road tile still gets a real
  `HelpPageNode` instance that now also participates in vehicle-arrival
  queuing. Renaming would touch `ui/HelpWnd.{h,cpp}`,
  `shared/vtable_addrs.h`, and `shared/core_stubs.cpp`, and first needs
  establishing whether `HelpWnd` genuinely uses this class or the name is
  pure Ghidra-label residue — left as a follow-up, documented in
  `ui/HelpPageNode.h`'s own top comment.

### Two real bugs found and fixed as a side effect

Converting the call sites changed which binary symbol they resolve to,
surfacing two distinct, previously-invisible defects (verified via
`objdump -d build/lego_loco | grep -c "call *0 "` before/after, on this
worktree's actual baseline of **266**, not the 259 given in the task brief
— stale relative to this branch tip, `00285a68`):

1. **`ArrivalQueue_RemoveVehicle`'s call site in `World_RenderAll` was a
   genuine call-0.** `World.cpp`'s local declaration
   (`uint16_t player_id, uint8_t color`, C++ linkage) mangled to a symbol
   nothing defined — neither the real `extern "C"` definition (unmangled)
   nor `defsym_stubs.cpp`'s stub (`unsigned int, char` — different
   mangled types) matched. Confirmed via disassembly: `call 0
   <_init-0x40a000>` before the fix, `call ... <_ZN12HelpPageNode13RemoveVehicleEth>`
   after.
2. **`ArrivalQueue_AddVehicle`'s call site in `World::FinalizeLoad` was a
   silent-wrong-stub, not call-0.** Its declaration
   (`void*, void*`, C++ linkage, no `extern "C"`) happened to
   **mangle-match** `defsym_stubs.cpp`'s no-op stub
   (`void ArrivalQueue_AddVehicle(void*, void*)`, also plain C++ linkage)
   exactly — so every vehicle load in the original single-player/
   multiplayer-scenario-1 path was silently **not** being queued onto its
   destination building at all; `World::FinalizeLoad` always returned
   success and the arrival-queue side effects (vehicle direction, tile
   position, enqueue) never ran. Confirmed via disassembly: `call
   5213a1 <_Z23ArrivalQueue_AddVehiclePvS_>` (the no-op stub) before the
   fix, `call ... <_ZN12HelpPageNode10AddVehicleEP7Vehicle>` (the real
   logic) after. This is exactly the "silent-wrong-stub" bug class this
   file documents elsewhere (distinct from call-0: it had a body, just an
   empty one) — not previously listed as such because the near-match
   table above only tracked it as a linkage/declaration mismatch, not as
   reachable-but-wrong.

Net `call 0` count: **266 → 265** (only `RemoveVehicle` was call-0; the
`AddVehicle` fix is the silent-wrong-stub class instead, which the
`call 0` count doesn't measure). Two pre-existing, unrelated `call 0`
sites remain inside `World::FinalizeLoad`/`World_RenderAll` — both are
`TileMap_GetObjectAt`, already tracked in this file's near-match table
("`TileMap_GetObjectAt` (cluster B)") — untouched, out of this session's
scope.

### Verification

- `meson setup build && meson compile -C build`: succeeds.
- `objdump -d build/lego_loco | grep -c "call *0 "`: 266 → 265 (see above).
- `meson test -C build`: 28/30 (the 2 known pre-existing failures,
  `embedded-mdns-discovery` and `sdl3-net-discovery-transport`, both
  "Unknown device type" — sandbox limitation, unrelated to this change).
- `meson test -C build --suite integration`: 1/1 (12/12 sub-tests).
- `meson setup build-strict2 -Dstrict=2 && ninja -C build-strict2 -k 0`:
  baseline (this branch, before this session's changes) is **1266**
  errors, not 0 — the "must exit 0" expectation in the task brief does not
  hold for this branch's actual tip either. After this session's changes:
  **1264** errors (net improvement of 2; the 3 new `HelpPageNode`
  `-Weffc++` diagnostics from retyping `dest_list_head` were fixed with
  the deleted copy-ctor/assignment above, and deleting
  `game/ArrivalQueue.{h,cpp}` removed a few now-nonexistent-file
  diagnostics). `build-strict2` deleted afterward per instructions.

## UI_ScrollBar/UI_ListBox free-function anti-pattern → ScrollCollection (2026-08-09)

**Task**: convert the 7 free functions in `ui/UI_ScrollBar.{h,cpp}` and the 2
remaining free functions in `ui/UI_ListBox.{h,cpp}` (`UI_DrawListBox`,
`UI_ListBox_Clear`) — all taking an explicit `void* self`, the CLAUDE.md
free-function-with-explicit-self anti-pattern — into real C++ methods on the
shared `Collection`-family base class.

**Correction to this task's own framing**: the "already investigated this
session, findings recorded in this file's 'UI_ScrollBar/UI_ListBox shared
TimerList base' section" pointer did not resolve to anything — no such
section existed in this file, at this worktree's branch tip OR at `main`'s
tip after fast-forwarding to it (`git merge main --ff-only`, `00285a6` ->
`c42a8b0`). Everything below was independently (re-)derived from Ghidra in
this session, not recovered from a prior one.

### Field layout: `collections.h`'s Collection struct had count/capacity swapped

Confirmed via **five independent sources agreeing**: disassembly of
`Timer::Resize` (0x435D10, writes the grown array's size to real offset
+0x08), `Collection::RemoveAt` (0x4356B0, bound-checks against +0x08),
`SortedCollection::SetAt`/`SortedCollection2::SetAt` (0x435A10/0x4360B0,
grow-trigger at +0x08, reject-bound at +0x0C), `Timer::IsSorted` (0x435CD0,
loop limit from +0x0C), and the sub-object construction sequences in
`UI_Ctor` (0x4238C0) and `BuildingComplex::BuildingComplex` (0x434500, both
zero +0x08 before `Resize()` and +0x0C after). Real x86 layout: vtable+0x00,
items+0x04, **capacity+0x08, count+0x0C** — opposite the field *declaration*
order in `collections.h` (count declared 2nd, capacity 3rd). Declaration
order was deliberately left alone (see the FIELD OFFSET NOTE now in
`collections.h`) since every access in this hierarchy is symbolic — only the
semantic *role* each field plays needed fixing, not its physical byte offset.

**Five real bugs found and fixed** where the code's SEMANTIC role (not just
the physical offset) was backwards, all newly discovered while re-deriving
the above (not part of the original task's known-facts list):
- `Collection::RemoveAt` bound-checked `this->count`; real body checks
  capacity. Loosens the bound whenever count < capacity — if a test's
  behavior moved after this fix, this is why.
- `SortedCollection::SetAt` and `SortedCollection2::SetAt` had their outer
  reject-bound (count) and growth-trigger (capacity) checks swapped.
- `Timer::IsSorted` used `this->capacity` for its loop limit; real body uses
  count.
- `UI_EnableScrollBar` (see below) used `this->count` for its sweep bound;
  real body uses capacity.

### The "three drain variants" were two shared virtual slots, not three ad-hoc functions

`UI_GetScrollPos`/`UI_SetScrollPos`/`UI_EnableScrollBar` are dispositively
not get/set/enable operations (void return, no position parameter, no
enable flag, in every case). A full non-spillover slot-by-slot
`get_xrefs_from` dump of every offset 0x00-0x54 across 6 concrete vtables
(the two UI_Manager sub-object final-stage tables B=0x477B78/
WRAPPER=0x477AE8, their base-stage counterparts A=0x477BD0/C=0x477B40, and
BuildingComplex's two TimerCollection final-stage tables 0x478018/0x477F88 —
read-only cross-check of `game/BuildingComplex.cpp`, not modified) showed:

- Slot 3 (RemoveAt): generic `Collection::RemoveAt` (0x4356B0) in the
  base-stage tables; `UI_HandleScrollMessage` (0x4241E0) is literally the
  SAME SLOT's override in the final-stage tables — not a separate concept.
- Slot 4 (RemoveElement): **uniform** across all 10 vtables sampled —
  0x4356E0, never overridden. Real body: `RemoveAt(index)` then destroy the
  extracted element via its virtual destructor. Was a silent no-op stub;
  now implemented for real.
- Slot 5: base-stage default 0x4244F0 (nulls `items[0..capacity)`, no
  cleanup, no count change — no prior name anywhere); final-stage override
  is `UI_GetScrollPos` (0x424250, count-bounded drain via RemoveAt).
- Slot 6: base-stage default is `UI_EnableScrollBar` (0x424510,
  capacity-bounded forward sweep via RemoveElement); final-stage override
  is `UI_SetScrollPos` (0x424270, count-bounded tail drain via
  RemoveElement).

Modeled as `Collection::RemoveAll`/`Collection::DestroyAll` (base bodies)
overridden by `ScrollCollection::RemoveAll`/`ScrollCollection::DestroyAll`.
Named for confirmed mechanism (bulk-clear without vs. with destruction);
original slot purpose within the class's lifecycle beyond that mechanism is
NOT claimed as recovered.

### `ScrollCollection` — the new subclass

Landed in `shared/collections.h`/`.cpp` as instructed (a new subclass, no
change to `Collection`'s own field layout). Carries the two extra fields at
+0x10/+0x14, confirmed by construction: in all three sub-objects `UI_Ctor`
builds (text/pos/update) AND both of `BuildingComplex`'s TimerCollections,
the two words immediately after the FINAL vtable install are zeroed again —
matching exactly what `SetKey` (was `UI_FreeScrollBar`) writes there.
Genuinely shared beyond `ui/`, as this task's own known-facts list predicted.

Field names `key_offset`/`key_size` were adopted from `ui/UI_Utils.h`'s
`UITimerList` — a **pre-existing, independently-derived duplicate** of this
exact same class (same field layout, same two extra fields, and its
`UI_Manager` ctor comment corroborates the same 0x477BD0->0x477B78 /
0x477B40->0x477AE8 vtable staging re-derived here). `ui/UI_Utils.h`/`.cpp`
were **not modified** by this change (out of scope) — flagging for a future
session: `UITimerList` should eventually be unified with `ScrollCollection`
per CLAUDE.md's "flat inherited structs or duplicate/partial layouts ->
actual inheritance and one canonical definition," but that touches
`ui/UI_Utils.cpp`'s `UI_Manager` construction/destruction paths, which is
real surface area beyond this task's scope.

### `Compact` — body implemented, name deliberately NOT changed

`Compact` (slot 20, offset +0x50) resolved to 0x4244D0 via the SAME
non-spillover full-table dump above (confirmed independently in 4 different
final-stage tables, all valid through slot 21). Real mechanism: if
count > 1, calls `QuickSortRangeImpl(0, count-1)` — a full re-sort, not a
compaction (no gap removal, no shrink). This makes `Compact` a
dispositively-wrong name by the same standard applied to
`UI_GetScrollPos`/etc. above. **Body is implemented for real** (was a no-op
stub); **name is intentionally left as `Compact`**, flagged rather than
renamed, because an earlier, narrower probe (short base-stage tables only)
produced spillover garbage at this same offset, and this file's own
mid-session advisor consult explicitly cautioned against renaming on that
evidence. The stronger, later evidence (4-way agreement across long,
non-spillover tables) resolves the original caution, but the rename itself
is left for whoever owns this file next to make deliberately, not as a side
effect of an unrelated task. See `Collection::Compact`'s doc comment in
`shared/collections.h` for the full instruction-level trail.

### `Collection::InsertAt` — still unverified

The addresses a prior pass of this file cited for `InsertAt` (0x424010,
0x424760) are real functions but occupy DIFFERENT, uniform-ish slots (11 and
12, +0x2C/+0x30) — not slot 10 (+0x28), which is where `InsertAt` actually
lives (confirmed: `DrawScrollBar`/`DrawListBox` both dispatch through it).
Every one of the 7 concrete vtables sampled has its own distinct body at
slot 10 (0x424170, 0x424290, 0x4246F0, 0x424790, 0x4359A0, and the two
already-integrated `SortedCollection`/`SortedCollection2::SetAt`). No
generic base-Collection body was identified. `Collection::InsertAt`'s
current body (grow-then-store) is a plausible generic default, consistent
with the confirmed capacity/count semantic roles, but is NOT
instruction-validated against any specific original address — left exactly
that way rather than guessing further.

### `SortedCollection::FindItem` — reconciled with `UI_ListBox_FindItem`

`UI_ListBox_FindItem` (the free-function transcription in `ui/UI_ListBox.cpp`)
and `SortedCollection::FindItem` (the already-integrated version in
`collections.cpp`) had silently diverged at the same original address
(0x424820): the original's `cmp >= 0` recursive branch passes `target`
itself as the new high bound (`(target, mid, target)`, not `(target, mid,
high)`) — `UI_ListBox_FindItem` preserved this (with an explicit `BUG:`
comment) but `collections.cpp` had quietly "fixed" it to `high`. Per
CLAUDE.md ("do not simplify assembly unless equivalence is proven"),
`collections.cpp`'s version was reverted to match the original, and
`UI_ListBox_FindItem` (dead duplicate code — not part of this task's 2
named remaining ListBox functions) was deleted.

### Verification

```
meson setup build && meson compile -C build     # succeeds, 0 errors
objdump -d build/lego_loco | grep -cE "call\s+0 "   # 260 before AND after
                                                      # (this task's cited
                                                      # baseline of 259 was
                                                      # stale relative to
                                                      # what this worktree
                                                      # needed to fast-
                                                      # forward through to
                                                      # reach main's tip;
                                                      # measured directly
                                                      # via `git stash`)
meson test -C build                              # 28/30 — the 2 known
                                                    # pre-existing sandbox
                                                    # failures only
meson setup build-strict2 -Dstrict=2 && ninja -C build-strict2 -k 0
                                                    # exit 0, 0 errors
```

### Deferred / not resolved

- `UITimerList` (`ui/UI_Utils.h`) duplicates `ScrollCollection` — not
  unified (out of scope; touches `UI_Manager`'s construction/destruction).
- `Collection::InsertAt`'s original address is unidentified; body unverified.
- `Compact`'s name (see above) — body fixed, rename deliberately deferred.
- Slot 9 ("Draw") is a real, confirmed shared virtual (DrawScrollBar/
  DrawListBox both dispatch through it in their respective base+final
  table pairs) but is NOT part of `ScrollCollection`'s own vtable —
  BuildingComplex's two TimerCollection finals don't have it at all.
  Modeled as two plain non-virtual methods on `ScrollCollection` rather
  than reconstructing the two further leaf subclasses that would be needed
  to model it as a true shared virtual — nothing in the current C++ tree
  dispatches through slot 9 virtually, so this is a documented
  simplification, not a behavioral gap.
- Slots 11/12 (offsets +0x2C/+0x30, addresses 0x424010/0x424760 uniform-ish
  across every table sampled) are real, unidentified virtual methods —
  out of scope, not implemented, flagging so a future pass doesn't
  rediscover them from scratch.

## Raw data

The full alignment run's intermediate files (per-symbol caller lists, the
near-match/missing classification with candidate real-definition symbols) were
generated by scripts in the session scratchpad and are not checked in — this
document is the durable summary. To regenerate: disassemble
`build/lego_loco` and every `build/lego_loco.p/*.o`, align each function's
call sequence between the two, diff against `nm --defined-only` across all
`.o` files in `lego_loco.p`.
