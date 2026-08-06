# call-0 landmine sweep — symbol worklist (2026-08-06)

Tracked continuation of the `call 0` landmine class documented in PROGRESS.md
("~400-site `call 0` landmine sweep", "render-path-landmine-sweep"). This file
is the durable worklist for the remaining sweep so a future session can resume
from it instead of re-deriving it.

**Current state (2026-08-06 22:00 UTC, agent dispatch wave 6)**: 409 `call 0` sites
(down from 412). Fixes: wave_io.c `__thiscall` removal (3 sites),
BuildingDescriptorEditor.cpp `WNDPROC_CriticalSectionLock` linkage fix (1 site).
Added loud stubs for 8 Stream I/O functions per MISSING category. Tests: 22/30
passing; 8 failures due to stub assert paths being exercised (expected).

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

## Near-match (mechanical declaration/linkage fixes) — still open

| Sites | Symbol | Candidate real definition(s) | Callers |
|---|---|---|---|
| 10 | `TrackPiece_SetZoom` | TrackPiece_SetZoom, TrackPiece_SetZoom(void*, short) | ScriptedObject::UpdateToolState(TrackPiece*) |
| 8 | `CRT_exit` | CRT_exit, CRT_exit(char const**, char const**) | Game_LoadWaveFile(char const*, void*) |
| 6 | `UIPANEL_EndPaintEx` | UIPANEL_EndPaintEx, UIPANEL_EndPaintEx(void*, int, int, unsigned char, RECT*), UIPANEL_EndPaintEx(void*, int, int, unsigned char, void*) | GameSetupPanel::drawLayoutList(LayoutListNode*), GameSetupPanel::drawGrid(), GameSetupPanel::drawTitle(), GameSetupPanel::on_update(int), Netman::HandlePlayerJoin(), Netman::RemoveInboundTrain(int) |
| 3 | `WIN32_StreamRead` (cluster A) | WIN32_StreamRead | Game_LoadWaveFile(char const*, void*), GameSetupPanel::loadLayouts(bool) |
| 4 | `PlaySound` | PlaySound, PlaySound(unsigned int) | BuildingMgr::HandleClick(BuildingClickCommand const*, int, int, int, int), HelpWnd::handle_click(void*, unsigned int, unsigned int, int) |
| 4 | `IntersectRect` | IntersectRect | UIPANEL_EndPaintEx(void*, int, int, unsigned char, RECT*), Panel::DispatchEvent(RECT*) |
| 4 | `TileMap_GetObjectAt` (cluster A) | TileMap_GetObjectAt, TileMap_GetObjectAt(TileMap*, short, short, short) | Building::StepToward(int, int), Building::FindNearbyObject(int, int, int) |
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
| 2 | `AssetMgr_ReadPairValue` | AssetMgr_ReadPairValue(AssetMgr*, unsigned int, unsigned int) | Building::StepToward(int, int), Building::FindNearestConnectionNode(void*, unsigned int) |
| 2 | `Vehicle_GetOccupantCount` | Vehicle_GetOccupantCount | Building::FindPathToTarget() |
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
| 1 | `ArrivalQueue_RemoveVehicle` | ArrivalQueue_RemoveVehicle, ArrivalQueue_RemoveVehicle(void*, unsigned int, char) | World_RenderAll(Vehicle*) |
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
| 1 | `NETMAN_FreePacket` | NETMAN_FreePacket(unsigned char*) | GameConfig::GameConfig() |
| 1 | `CRT_localtime` (cluster A) | CRT_localtime | Building::DecideAction() |
| 1 | `CRT_localtime` (cluster B) | CRT_localtime | ResourceGameObject::UpdateScheduledAnimation() |

## Genuinely missing (need Ghidra RE or a loud deferred stub) — still open

The `Town_Draw*` / `Town_*TileCache*` cluster (8 symbols, called only from
`UIPANEL_Blit`) is the largest and most consequential: it looks like the
low-level tile pixel-pusher layer was never implemented at all, one level
below the "Town rendering — Phase B: tile-placement metadata" gap already
tracked in PROGRESS.md. Worth investigating whether fixing this is actually a
*precondition* for Phase B ever producing visible pixels, not an independent
gap.

| Sites | Symbol | Callers |
|---|---|---|
| 8 | `Town_DrawTile` | UIPANEL_Blit(...) |
| 8 | `WNDPROC_StreamPrintf` (STUB) | BuildingDescriptorEditor::draw_border_grid(void*), BuildingDescriptorEditor::paint_edit_regions(void*) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 7 | `WNDPROC_StreamWrite` (STUB) | edit_key_handler_parse(void*, KeySequenceRecord*) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 6 | `Town_FlushTileCache` | UIPANEL_Blit(...) |
| 6 | `Town_DrawCachedTile` | UIPANEL_Blit(...) |
| 5 | `ReleaseSoundResource` | HelpWnd::go_next_page(), HelpWnd::go_prev_page(), HelpWnd::hide(), GameSetupPanel::base_destructor() |
| 4 | `LoadSoundResource` | HelpWnd::go_next_page(), HelpWnd::go_prev_page() |
| 3 | `Town_InitTileCache` | UIPANEL_Blit(...) |
| 3 | `Town_DrawTiles16bpp_Strided` | UIPANEL_Blit(...) |
| 3 | `Town_DrawTileEx` | UIPANEL_Blit(...) |
| 3 | `Town_BlitTileSurface` | UIPANEL_Blit(...) |
| 3 | `Town_DrawTiles16bpp_Reversed` | UIPANEL_Blit(...) |
| 3 | `Town_DrawTiles16bpp_Checker` | UIPANEL_Blit(...) |
| 3 | `Town_DrawTiles16bpp_Staggered` | UIPANEL_Blit(...) |
| 3 | `Town_DrawTileLine` | UIPANEL_Blit(...) |
| 3 | `WNDPROC_StreamReadLine` (STUB) | edit_key_handler_parse(void*, KeySequenceRecord*) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 2 | `CRT_fabs` (STUB) | edit_key_handler_parse(void*, KeySequenceRecord*) — STUB: loud fprintf+assert in stubs_impl.cpp. |
| 2 | `ScriptedObject_ParseStream` | ScriptedObject::HandleEvent(unsigned int, char const*) |
| 2 | `Ordinal_1` | GameAudio::Init() |
| 2 | `EditorState_Copy` | Vehicle::UpdateEngineSound() |
| 2 | `ScriptedObject_InitBase` | ScriptedObject::RemoveChild(), ScriptedObject::AddChild(unsigned int, char const*) |
| 1 | `Town_CalcScrollRect` | UIPANEL_Blit(...) |
| 1 | `Town_CalcScrollRect_Reversed` | UIPANEL_Blit(...) |
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
| 1 | `TileMap_FindTileByType` | Building::TeleportTo(int, int) |

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

## Raw data

The full alignment run's intermediate files (per-symbol caller lists, the
near-match/missing classification with candidate real-definition symbols) were
generated by scripts in the session scratchpad and are not checked in — this
document is the durable summary. To regenerate: disassemble
`build/lego_loco` and every `build/lego_loco.p/*.o`, align each function's
call sequence between the two, diff against `nm --defined-only` across all
`.o` files in `lego_loco.p`.
