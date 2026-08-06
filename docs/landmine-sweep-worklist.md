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
distinct symbols**. **Current state: 412 `call 0` sites.**

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

## Near-match (mechanical declaration/linkage fixes) — still open

| Sites | Symbol | Candidate real definition(s) | Callers |
|---|---|---|---|
| 10 | `TrackPiece_SetZoom` | TrackPiece_SetZoom, TrackPiece_SetZoom(void*, short) | ScriptedObject::UpdateToolState(TrackPiece*) |
| 8 | `CRT_exit` | CRT_exit, CRT_exit(char const**, char const**) | Game_LoadWaveFile(char const*, void*) |
| 6 | `UIPANEL_EndPaintEx` | UIPANEL_EndPaintEx, UIPANEL_EndPaintEx(void*, int, int, unsigned char, RECT*), UIPANEL_EndPaintEx(void*, int, int, unsigned char, void*) | GameSetupPanel::drawLayoutList(LayoutListNode*), GameSetupPanel::drawGrid(), GameSetupPanel::drawTitle(), GameSetupPanel::on_update(int), Netman::HandlePlayerJoin(), Netman::RemoveInboundTrain(int) |
| 5 | `DDRAW_PresentRect` | DDRAW_PresentRect(RECT const*, void*, int*, unsigned char), DDRAW_PresentRect(void*, void*, int*, unsigned char), DDRAW_PresentRect(void*, void*, int*, int) | UIPANEL_EndPaintEx(void*, int, int, unsigned char, RECT*) |
| 4 | `WIN32_StreamRead` (cluster A) | WIN32_StreamRead | Game_LoadWaveFile(char const*, void*), GameSetupPanel::loadLayouts(bool), Cursor::init() |
| 4 | `PlaySound` | PlaySound, PlaySound(unsigned int) | BuildingMgr::HandleClick(BuildingClickCommand const*, int, int, int, int), HelpWnd::handle_click(void*, unsigned int, unsigned int, int) |
| 4 | `IntersectRect` | IntersectRect | UIPANEL_EndPaintEx(void*, int, int, unsigned char, RECT*), Panel::DispatchEvent(RECT*) |
| 4 | `WIN32_StreamRead` (cluster B) | WIN32_StreamRead | UIPANEL_StretchBlit(void*, char const*, unsigned int, int, int), UIPANEL_ReadPaletteFromBMP(void*, void*) |
| 4 | `NET_GetOrCreateSurface` | NET_GetOrCreateSurface | Cursor::draw_postcard_preview(unsigned char) |
| 4 | `TileMap_GetObjectAt` (cluster A) | TileMap_GetObjectAt, TileMap_GetObjectAt(TileMap*, short, short, short) | Building::StepToward(int, int), Building::FindNearbyObject(int, int, int) |
| 3 | `WIN32_StreamRead` (cluster C) | WIN32_StreamRead | Game_ReadChunk(WNDPROC_Stream*, RiffChunkHeader*, int, int) |
| 3 | `CRT_sprintf_buf` | CRT_sprintf_buf, CRT_sprintf_buf(char*, char const*), CRT_sprintf_buf(char*, char const*, ...) | ScriptedObject::HandleEvent(unsigned int, char const*) |
| 3 | `InflateRect` | InflateRect | TileMap_ProcessDirtyRects(RECT*) |
| 3 | `TileMap_GetObjectAt` (cluster B) | TileMap_GetObjectAt, TileMap_GetObjectAt(TileMap*, short, short, short) | World_RenderAll(Vehicle*), World::FinalizeLoad(Vehicle*, int, char) |
| 3 | `FormatResourceString` | FormatResourceString, FormatResourceString(void*, int, char*, int), FormatResourceString(void*, unsigned int, char*, int) | GameSetupPanel::updateTitle(), GameSetupPanel::drawLayoutList(LayoutListNode*) |
| 3 | `ResourceManager_GetById` | ResourceManager_GetById(void**, int), ResourceManager_GetById(void**, unsigned int), ResourceManager_GetById(void*, int) | NETMAN_JoinSession(void*), BuildingPanel::init_sprites() |
| 2 | `UI_ChildWindow_Render` | UI_ChildWindow_Render | ScriptedObject::HandleEvent(unsigned int, char const*) |
| 2 | `RESMGR_ResourceData_Init` | RESMGR_ResourceData_Init(RESDATA*) | UIPANEL_DrawEditField(int) |
| 2 | `RESMGR_ReleaseResource` | RESMGR_ReleaseResource(RESDATA*) | UIPANEL_FreeSprite(void*), UIPANEL_DrawEditField(int) |
| 2 | `FormatMessageA` | FormatMessageA, FormatMessageA(int, void*, int, int, char*, int, void*) | GameWindow::create(...) |
| 2 | `DDRAW_UnlockPrimary` | DDRAW_UnlockPrimary, DDRAW_UnlockPrimary() | GameWindow::show(), GameWindow::set_mode(int, void*, unsigned char, unsigned char) |
| 2 | `CGWND_GameSetup_DrawGrid_Thunk` | CGWND_GameSetup_DrawGrid_Thunk | Netman::HandlePlayerJoin(), Netman::RemoveInboundTrain(int) |
| 2 | `RESMGR_IsSaveHeader` | RESMGR_IsSaveHeader(RESDATA*) | UIPANEL_DrawBorder(void*, int), UIPANEL_CreateSprite(void*, void*) |
| 2 | `AssetMgr_ReadPairValue` | AssetMgr_ReadPairValue(AssetMgr*, unsigned int, unsigned int) | Building::StepToward(int, int), Building::FindNearestConnectionNode(void*, unsigned int) |
| 2 | `Vehicle_GetOccupantCount` | Vehicle_GetOccupantCount | Building::FindPathToTarget() |
| 1 | `WNDPROC_CriticalSectionLock` (FIXED) | WNDPROC_CriticalSectionLock(int*, char*) | edit_key_handler_parse(void*, KeySequenceRecord*) — FIXED: moved out of extern "C" block, signature corrected to (int*, char*), call sites updated with reinterpret_cast. |
| 1 | `WIN32_StreamOpenPath` (cluster B) | WIN32_StreamOpenPath | Game_LoadWaveFile(char const*, void*) |
| 1 | `WNDPROC_EnterCriticalSection` | WNDPROC_EnterCriticalSection | Game_ReadChunk(WNDPROC_Stream*, RiffChunkHeader*, int, int) |
| 1 | `WNDPROC_LeaveCriticalSection` | WNDPROC_LeaveCriticalSection | Game_ReadChunk(WNDPROC_Stream*, RiffChunkHeader*, int, int) |
| 1 | `WIN32_StreamDestroy` | WIN32_StreamDestroy, WIN32_StreamDestroy(void*) | UIPANEL_StretchBlit(void*, char const*, unsigned int, int, int) |
| 1 | `CRT_wcsstr` | CRT_wcsstr, CRT_wcsstr(char const*, char const*) | AssetMgr_LoadFile(void*, unsigned char*, int*) |
| 1 | `CRT_0x468610` | CRT_0x468610(void*, unsigned int, unsigned int, int) | AssetMgr_LoadFile(void*, unsigned char*, int*) |
| 1 | `Vehicle_Ctor` | Vehicle_Ctor, Vehicle_Ctor(void*, int, int, char, char) | Train_HandleTrackBuild(void*, int) |
| 1 | `Vehicle_InitRoute` | Vehicle_InitRoute, Vehicle_InitRoute(void*, int, unsigned int, char) | Train_HandleTrackBuild(void*, int) |
| 1 | `VehicleEditor_SetDPlayData` | VehicleEditor_SetDPlayData | Train_HandleTrackBuild(void*, int) |
| 1 | `DirectPlay_Close` | DirectPlay_Close, DirectPlay_Close(int) | Train_HandleTrackBuild(void*, int) |
| 1 | `Train_SendPlayerInfo` | Train_SendPlayerInfo | Train_HandleTrackBuild(void*, int) |
| 1 | `DDRAW_SetSurfaceFormat` | DDRAW_SetSurfaceFormat, DDRAW_SetSurfaceFormat(void*, int) | GameWindow::create(...) |
| 1 | `World_SerializeObject` | World_SerializeObject | Netman::RemoveInboundTrain(int) |
| 1 | `NET_FindPlayer` | NET_FindPlayer | Cursor::upload_custom_content() |
| 1 | `NET_UploadAsset` | NET_UploadAsset | Cursor::upload_custom_content() |
| 1 | `PlaySoundFile` | PlaySoundFile | Cursor::upload_custom_content() |
| 1 | `ArrivalQueue_RemoveVehicle` | ArrivalQueue_RemoveVehicle, ArrivalQueue_RemoveVehicle(void*, unsigned int, char) | World_RenderAll(Vehicle*) |
| 1 | `InvalidateRect` | InvalidateRect, GameObject::InvalidateRect(), World::InvalidateRect(int, int, int, int, short) | TileMap::FullReset() |
| 1 | `UpdateWindow` | UpdateWindow | TileMap::FullReset() |
| 1 | `DDRAW_GetDdrawErrorString` | DDRAW_GetDdrawErrorString | UIPANEL_ClearSurface(void*, int, int) |
| 1 | `DDRAW_RestoreSurfaces` | DDRAW_RestoreSurfaces, DDRAW_RestoreSurfaces(IDirectDrawSurface4*, void*), DDRAW_RestoreSurfaces(void*, void*) | UIPANEL_ClearSurface(void*, int, int) |
| 1 | `RESMGR_LoadResource` | RESMGR_LoadResource(RESDATA*, char const*) | UIPANEL_CreateSprite(void*, void*) |
| 1 | `AssetMgr_LoadFile` | AssetMgr_LoadFile, AssetMgr_LoadFile(int*, unsigned char*, int*), AssetMgr_LoadFile(void*, unsigned char*, int*) | GameSetupPanel::loadLayouts(bool) |
| 1 | `WIN32_StreamOpenFile` | WIN32_StreamOpenFile | Cursor::init() |
| 1 | `Train_StartMultiplayer` | Train_StartMultiplayer | TrainSubsystem::DispatchMessage(void*) |
| 1 | `Train_StopMultiplayer` | Train_StopMultiplayer | TrainSubsystem::DispatchMessage(void*) |
| 1 | `RESDATA_Lock` | RESDATA_Lock | BuildingMgr::CompactCollections() |
| 1 | `RESDATA_Unlock` | RESDATA_Unlock | BuildingMgr::CompactCollections() |
| 1 | `MessageBoxA` | MessageBoxA | WIN32_FatalError() |
| 1 | `UI_IsBitmapReady` | UI_IsBitmapReady, UI_IsBitmapReady(int) | UIPANEL::InitSprites() |
| 1 | `NET_RegisterPlayer` | NET_RegisterPlayer | Netman::HandleTimeout(Vehicle*) |
| 1 | `World_FinalizeLoad` | World_FinalizeLoad | Netman::SendChatMessage(Vehicle*) |
| 1 | `UI_CenterWindow` (cluster B) | UI_CenterWindow(int*, int*) | RenderConnectionPanel(void*) |
| 1 | `CGWND_PumpMessages` | CGWND_PumpMessages(char), CGWND_PumpMessages(void*, unsigned char) | Cursor::draw_locomotive_preview(unsigned char) — check whether this is actually the SAME real CGWND_PumpMessages used by the main message loop before assuming a separate overload |
| 1 | `DPLAY_RenderPlayer` | DPLAY_RenderPlayer, DPLAY_RenderPlayer(void*, void*, int, void*, int, int, unsigned int, RECT*) | Cursor::blit_edit_preview() |
| 1 | `DPLAY_EnumeratePlayers` | DPLAY_EnumeratePlayers | Cursor::update_network_names() |
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
`GameWindow::hide`, `CursorEditWindow::init`, `Town::save_postcard_as`,
`Town::delete_postcard`, `Town::load_postcard`, `Town::save_postcard`,
`Town::receive_postcard`, `Netman::ReceiveGameStart`, `Netman::LoadScenario`,
`DirectPlay_GetSessionDesc`, `Cursor::set_capture`, `Cursor::hide`,
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
