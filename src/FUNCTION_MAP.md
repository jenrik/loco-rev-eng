# Lego Loco (loco.exe) Function Map

Auto-generated from Ghidra headless decompiler index.
Known mappings are based on manual analysis; remaining names are best-guess from address range.

## Address Range to Subsystem

| Range | Subsystem |
|---|---|
| 0x401000–0x404fff | graphics (LOCOBITMAP, surface management) |
| 0x406000–0x413fff | core (CGWND methods, game setup) |
| 0x414000–0x42bfff | input/cursor/UI |
| 0x42c000–0x43cfff | game world (town, buildings, trains) |
| 0x43d000–0x445fff | network (NETMAN, DirectPlay) |
| 0x446000–0x44ffff | resource system |
| 0x450000–0x45ffff | audio/DirectSound + DirectDraw |
| 0x460000–0x465fff | Win32 platform layer (WndProc, etc.) |
| 0x466000+ | CRT (string functions, math, etc.) |

## Function Table

| Address | Original Name | Suggested Name | Subsystem | Notes |
|---|---|---|---|---|
| 00401000 | FUN_00401000 | LOCOBITMAP_LoadFromFile | graphics | Loads bitmap from file |
| 00401170 | FUN_00401170 | LOCOBITMAP_BlitBitmap | graphics | Blits bitmap to surface |
| 00401280 | FUN_00401280 | LOCOBITMAP_00401280 | graphics |  |
| 004014e0 | FUN_004014e0 | LOCOBITMAP_004014e0 | graphics |  |
| 00401540 | FUN_00401540 | LOCOBITMAP_00401540 | graphics |  |
| 00401620 | FUN_00401620 | LOCOBITMAP_00401620 | graphics |  |
| 00401650 | FUN_00401650 | LOCOBITMAP_00401650 | graphics |  |
| 00401680 | thunk_FUN_00401c90 | thunk_00401c90 | graphics | Thunk to thunk_FUN_00401c90 |
| 00401690 | FUN_00401690 | LOCOBITMAP_00401690 | graphics |  |
| 00401760 | FUN_00401760 | LOCOBITMAP_00401760 | graphics |  |
| 00401810 | FUN_00401810 | LOCOBITMAP_00401810 | graphics |  |
| 00401820 | FUN_00401820 | LOCOBITMAP_00401820 | graphics |  |
| 00401850 | FUN_00401850 | LOCOBITMAP_00401850 | graphics |  |
| 00401aa0 | FUN_00401aa0 | LOCOBITMAP_00401aa0 | graphics |  |
| 00401c10 | FUN_00401c10 | LOCOBITMAP_00401c10 | graphics |  |
| 00401c90 | FUN_00401c90 | LOCOBITMAP_00401c90 | graphics |  |
| 00401df0 | FUN_00401df0 | LOCOBITMAP_00401df0 | graphics |  |
| 00401f50 | FUN_00401f50 | LOCOBITMAP_00401f50 | graphics |  |
| 00401fb0 | FUN_00401fb0 | LOCOBITMAP_00401fb0 | graphics |  |
| 00401fd0 | FUN_00401fd0 | LOCOBITMAP_00401fd0 | graphics |  |
| 00402380 | FUN_00402380 | LOCOBITMAP_00402380 | graphics |  |
| 00402520 | FUN_00402520 | LOCOBITMAP_00402520 | graphics |  |
| 00402660 | FUN_00402660 | LOCOBITMAP_00402660 | graphics |  |
| 00402690 | FUN_00402690 | LOCOBITMAP_00402690 | graphics |  |
| 00403ba0 | FUN_00403ba0 | SURFACE_00403ba0 | graphics |  |
| 00403cd0 | FUN_00403cd0 | SURFACE_00403cd0 | graphics |  |
| 00403e80 | FUN_00403e80 | SURFACE_00403e80 | graphics |  |
| 00404720 | FUN_00404720 | GFX_00404720 | graphics |  |
| 00404770 | FUN_00404770 | GFX_00404770 | graphics |  |
| 00404830 | FUN_00404830 | GFX_00404830 | graphics |  |
| 004048e0 | FUN_004048e0 | GFX_004048e0 | graphics |  |
| 00404ac0 | FUN_00404ac0 | GFX_00404ac0 | graphics |  |
| 00405520 | FUN_00405520 | CGWND_GameSetup_RenderPlayerSlots | core | RENAMED 2026-07: Was UNK_00405520 (also misnamed GameObject_InitFields). Renders network player slots in game setup screen. Accesses g_dplay_config, blits player info. |
| 004055e0 | FUN_004055e0 | GameObject_OnTimerTick | core | RENAMED 2026-07: Was UNK_004055e0. Timer completion handler: destroys child callback object at +0x130, calls vtable[3] to update display. Virtual vtable slot 3. |
| 00405680 | FUN_00405680 | GameObject_HitTest | core | RENAMED 2026-07: Was UNK_00405680. Hit-tests packed (X|Y) point against 8 UISprite sub-rectangles. Dispatches vtable[3] with hit or miss coords. |
| 00405790 | FUN_00405790 | GameObject_BaseCtor | core | Entity constructor: calls GameObject_Ctor, sets vtable 0x477488, initializes fields. |
| 00405850 | FUN_00405850 | GameObject_ScrDtor | core | RENAMED 2026-07: Was UNK_00405850. Scalar deleting destructor (MSVC vtable[0]): calls DtorBody then GLOBAL_free if mode&1. |
| 00405870 | FUN_00405870 | GameObject_DtorBody | core | RENAMED 2026-07: Was UNK_00405870. Destructor body with SEH: releases audio channel (+0x48), child object (+0x40), sound resource (+0x44), calls GameObject_MarkDead. Called from 25+ destructors. |
| 00405900 | FUN_00405900 | GameObject_InitBase | core | Load resource by ID, setup bounding rects, init animation state. |
| 00405a20 | FUN_00405a20 | GameObject_StopSound | core | RENAMED 2026-07: Was UNK_00405a20. Stop current audio: releases channel at +0x48, clears sound ID, delegates to vtable[14]. Virtual function. |
| 00405a50 | FUN_00405a50 | GameObject_SetAnimState | core | RENAMED 2026-07: Was UNK_00405a50. Set animation by frame table index: validates bounds, stores frame ID, triggers vtable[8] redraw and GameObject_PlayAnimation. Virtual function. |
| 00405ab0 | FUN_00405ab0 | GameObject_PlayAnimation | core | Look up sound resource, allocate audio channel, schedule playback. |
| 00405c00 | FUN_00405c00 | GameObject_SetWorldPos | core | RENAMED 2026-07: Was UNK_00405c00. Set world position (x,y): calls GameObject_MoveTo, updates sprite offset fields (+0x4c,+0x50), repositions audio channel. Virtual function. |
| 00405c40 | FUN_00405c40 | GameObject_Update | core | Animation state-machine: advance frame index, handle ping-pong/pause. |
| 00405de0 | FUN_00405de0 | GameObject_SetFrame | core | RENAMED 2026-07: Was UNK_00405de0. Set frame ID: stores at +0x54, computes byte offsets (+0x30,+0x38) using stride from object data. Virtual function. |
| 00405e20 | FUN_00405e20 | GameObject_SetName | core | RENAMED 2026-07: Was UNK_00405e20. Set object name: validates first char (alphanumeric/null), copies max 10 chars to +0x7c, null-terminates at +0x86. Virtual function. |
| 00405e60 | FUN_00405e60 | GameObject_Draw | core | Render sprite to primary surface with clipping and flip support. |
| 00405fd0 | FUN_00405fd0 | GameObject_DrawConnected | core | RENAMED 2026-07: Was UNK_00405fd0. Renders connected/multi-tile sprites by temporarily incrementing frame index, blitting, then restoring. |
| 004061b0 | FUN_004061b0 | CGWND_SetPause | core | Toggles active/paused state on UI window; manages audio channel at +0x48 |
| 004061e0 | FUN_004061e0 | CGWND_constructor | core | Game window object constructor |
| 004062a0 | FUN_004062a0 | CGWND_destructor | core | Scalar deleting dtor; releases g_config_ini global |
| 004062e0 | FUN_004062e0 | CGWND_ResetState | core | Reads EXE FileVersion from VERSIONINFO, parses into 4 int fields (major,minor,patch,build) |
| 00406480 | FUN_00406480 | CGWND_ShowMainMenu | core | Initializes display state before main menu; reads screen res, window pos from INI, FPS limits, CleanExit |
| 00406680 | FUN_00406680 | CGWND_InitGame | core | Display validation gate: checks color depth, mouse, screen width 800-1280 before game start |
| 00406790 | FUN_00406790 | CGWND_ParseCmdLine | core | Parses command line for easter egg keywords and demo mode flags (/s, -s, s) |
| 004068d0 | FUN_004068d0 | CGWND_InstallPathInit | core | Reads install path from registry HKLM\SOFTWARE\Intelligent Games\LEGO Loco, loads lego.ini, creates data dir |
| 00406ba0 | FUN_00406ba0 | GameLoop_Setup | core | Initializes main game loop |
| 00406e80 | FUN_00406e80 | CGWND_QuitToMenu | core | Clean transition from gameplay to main menu; shuts down netman, audio, sprites, world |
| 00406ed0 | FUN_00406ed0 | CGWND_RegisterWindowClass | core | Registers LEGO_LOCO window class and creates main WS_POPUP window |
| 00406f90 | FUN_00406f90 | CGWND_InitAllSubsystems | core | MASTER initializer: creates 8 subsystems (UI,Town,Postcard,Station,Cursor,Audio,About) with SEH error handling |
| 004077a0 | FUN_004077a0 | CGWND_Cleanup | core | 8-phase teardown of ~28 COM objects plus system shutdown; reverse of InitAllSubsystems |
| 00407ae0 | FUN_00407ae0 | CGWND_ScrollHorizontal | core | stdcall horizontal scroll handler; updates viewport, sets scrollbar range|pos |
| 00407bf0 | FUN_00407bf0 | CGWND_ScrollVertical | core | stdcall vertical scroll handler; 5 callers from main WndProc WM_VSCROLL |
| 00407d00 | FUN_00407d00 | CGWND_ToggleFullscreen | core | 31-byte leaf; toggles g_is_fullscreen flag, dispatches to SetFullscreenMode |
| 00407d20 | FUN_00407d20 | CGWND_SetFullscreenMode | core | 1034-byte display mode switch; handles fullscreen|windowed with demo mode special cases |
| 00408130 | FUN_00408130 | CGWND_SetMode | core | CENTRAL game mode state machine; 10 modes via jump table at 0x408320; 38 callers |
| 00408350 | FUN_00408350 | CGWND_InitMode1 | core | Mode 1 init: Path A (fresh start) progressive loading, Path B (world reload) loads world->mode 3 |
| 004085e0 | FUN_004085e0 | CGWND_PumpMessages | core | Win32 message pump; bFilter=true discards mouse msgs during loading transitions |
| 004086f0 | FUN_004086f0 | CGWND_EnterMode3 | core | Transition to game mode 3 from any mode; handles exit cleanup per previous mode |
| 004089d0 | FUN_004089d0 | CGWND_SetBuildMode | core | Global build-mode state machine (0=exit, 1=road, 2=building); 15 call sites |
| 00408a30 | FUN_00408a30 | CGWND_ReadRegistryString | core | Thin RegOpenKeyEx|RegQueryValueEx wrapper for HKLM string reads |
| 00408aa0 | FUN_00408aa0 | CGWND_GameSetup_Ctor | core | Game Setup lobby constructor; 0x260-byte obj; creates 14 sprites (5 main + 9 layout thumbs) |
| 00408b00 | FUN_00408b00 | CGWND_GameSetup_Dtor | core | Game Setup scalar deleting dtor; chains to BaseDtor then conditional free |
| 00408b20 | FUN_00408b20 | CGWND_GameSetup_Init | core | Initializes GameSetup fields; creates 5 main sprites + 9 layout thumbnail sprites |
| 00408d10 | FUN_00408d10 | CGWND_GameSetup_BaseDtor | core | Base dtor for GameSetup; destroys linked lists (titles, layouts), 14 sprites, sound resource |
| 00408f00 | FUN_00408f00 | CGWND_GameSetup_Create | core | Creates full-screen game setup window as child of main menu via UI_CreateFullWindow |
| 00409280 | FUN_00409280 | CGWND_GameSetup_Render | core | Main render for Game Setup lobby; 6-step pipeline: bg blit, title, sprites, layout|player grid |
| 00409360 | FUN_00409360 | CGWND_00409360 | core |  |
| 004094b0 | FUN_004094b0 | CGWND_004094b0 | core |  |
| 00409770 | FUN_00409770 | CGWND_00409770 | core |  |
| 00409970 | thunk_FUN_00409980 | CGWND_GameSetup_DrawGrid_Thunk | core | RENAMED 2026-07: 5-byte JMP thunk to CGWND_GameSetup_DrawGrid at 0x409980 |
| 00409980 | FUN_00409980 | CGWND_00409980 | core |  |
| 00409db0 | FUN_00409db0 | CGWND_00409db0 | core |  |
| 00409e70 | FUN_00409e70 | CGWND_00409e70 | core |  |
| 0040a0d4 | Catch@0040a0d4 | CRT_catch_handler_0040a0d4 | core | C++ catch block handler |
| 0040a150 | FUN_0040a150 | GAMESTATE_StartNewGame | core | Entry point for new game: resets network state, clears layout list, dispatches START_NEW_GAME message, sets guard flag |
| 0040a220 | FUN_0040a220 | GAMESTATE_ReturnToMainMenu | core | Transitions from game setup to main menu: vtable cleanup, net reset, UI_MainMenu_SetState(7), clears guard |
| 0040a260 | FUN_0040a260 | GAMESTATE_LoadExistingGame | core | Host vs client game setup (not save loading): host→StartNewGame, client→network session init+LoadLayouts |
| 0040a300 | FUN_0040a300 | GAMESTATE_HandleNetworkGame | core | Handles scenario select in network mode: sets netman mode=1, updates title, sends scenario select train msg |
| 0040a350 | FUN_0040a350 | GAMESTATE_StartGameTimer | core | Transitions game to running state: creates Win timers (50ms main loop, 75ms anim loop), sets state to 2 |
| 0040a3d0 | FUN_0040a3d0 | GAMESTATE_SelectLayout | core | Layout/scenario selection mgmt: frees old layout list, stores new layout ptr, triggers SelectLayoutEntry |
| 0040a4a0 | FUN_0040a4a0 | GAMESTATE_SetDifficulty | core | Stores difficulty/scenario index at +0x110; updates title+repaint if single-player and visible |
| 0040a4e0 | FUN_0040a4e0 | GAMESTATE_HandleClick | core | Click dispatcher for game setup screen; 7 hit targets (logo, back, play, dinosaur egg, join, layout list, scenario map) with priority order |
| 0040aa20 | FUN_0040aa20 | GAMESTATE_ConnectToNetworkGame | core | Network game connection: walks session list by selectedLayoutIndex, loads scenario, broadcasts layout-select, restarts DP sessions |
| 0040aaf0 | FUN_0040aaf0 | GAMESTATE_SelectLayoutEntry | core | Single-player layout entry selection (guarded: skips if network); BUG: NULL deref if index exceeds list length |
| 0040aba0 | FUN_0040aba0 | GAMESTATE_HandleMapClick | core | Scenario grid click handler: divides 3x3 grid into cells, computes scenario index, calls SendScenarioSelect on change |
| 0040ac50 | FUN_0040ac50 | GAMESTATE_SendScenarioSelect | core | Sends scenario selection: network path→NETMAN_SendLayoutSelect, single-player→train msg with player name/color/id |
| 0040b4c0 | FUN_0040b4c0 | GAMESTATE_WndProc | core | Minimal __stdcall WndProc; intercepts SC_CLOSE for clean shutdown (hides all windows), others → DefWindowProcA |
| 0040b500 | FUN_0040b500 | GAMESTATE_EditorState_Ctor | core | EditorState ctor: 0x20-byte struct, vtable 0x477564, dual-mode init (primary/secondary split-screen) |
| 0040b550 | FUN_0040b550 | GAMESTATE_EditorState_Dtor | core | EditorState dtor: resets vtable, decrements tileObject refcount at +0x114, optional free |
| 0040b5a0 | FUN_0040b5a0 | GAMESTATE_EditorState_Detach | core | Detaches tileObject: 16-bit refcount decrement at +0x114; skipped in game mode 10 (editor) |
| 0040b5d0 | FUN_0040b5d0 | GAMESTATE_EditorState_Copy | core | Copies 7 dwords from source (skips vtable at +0x00); callers increment dest refcount post-copy |
| 0040b610 | FUN_0040b610 | GAMESTATE_FindTrackPosition | core | Snaps pixel coords to nearest track control point; X-search and Y-search modes; BUG: simultaneous X/Y match on first point |
| 0040b740 | FUN_0040b740 | GAMESTATE_InitTrackAtPosition | core | Converts world coords to tile coords, looks up track resource (type 3), matches track segment offsets, stores world position |
| 0040b880 | FUN_0040b880 | GAMESTATE_FindAdjacentTrack | core | Core track connection logic: 4 connection-point priorities (main fwd/rev, extra fwd/rev), refcount swap on match |
| 0040bbd0 | FUN_0040bbd0 | GAMESTATE_UpdateVehiclePlacement | core | Largest function in loco.exe (1928B): 9-phase state machine for vehicle placement along tracks during editing |
| 0040c3d0 | FUN_0040c3d0 | VehicleEditor_TryAttach | core | Attaches vehicle to track endpoint. |
| 0040c460 | FUN_0040c460 | VehicleEditor_HandleDirection | core | Handles direction change. |
| 0040c580 | FUN_0040c580 | GAMESTATE_EditorState_UpdatePosition | core | MAY be VehicleEditor method, verify. |
| 0040cb10 | FUN_0040cb10 | VehicleEditor_ScrollEdge | core | Scrolls editor to track network edge. |
| 0040cc20 | FUN_0040cc20 | VehicleEditor_CheckBounds | core | Bounds check. |
| 0040cc90 | FUN_0040cc90 | VehicleEditor_CheckBounds2 | core | Secondary bounds check. |
| 0040cd60 | FUN_0040cd60 | VehicleEditor_UpdateEditMode | core | Edit mode state machine. |
| 0040cfa0 | FUN_0040cfa0 | TrackPiece_Ctor | game | Constructor. Inherits GameObject. |
| 0040d020 | FUN_0040d020 | TrackPiece_ScrDtor | game | Scalar deleting destructor. |
| 0040d040 | FUN_0040d040 | TrackPiece_Dtor | game | Destructor body. |
| 0040d0b0 | FUN_0040d0b0 | TrackPiece_Init | game | Init sprites and state. |
| 0040d170 | FUN_0040d170 | TrackPiece_SetZoom | game | Set rendering zoom level. |
| 0040d2a0 | FUN_0040d2a0 | TrackPiece_SetFrame | game | Set animation frame. |
| 0040d2f0 | FUN_0040d2f0 | TrackPiece_UpdateAnim | game | Update animation state. |
| 0040d340 | FUN_0040d340 | TrackPiece_Render | game | Render sprite. |
| 0040d470 | FUN_0040d470 | TrackPiece_RecalcRect | game | Recalculate bounding rect. |
| 0040d500 | FUN_0040d500 | VehicleEditor_Ctor | core | Constructor. vtable 0x477568. Inherits Entity. |
| 0040d660 | FUN_0040d660 | VehicleEditor_ScrDtor | core | Scalar deleting destructor. vtable[0]. |
| 0040d680 | FUN_0040d680 | VehicleEditor_DtorBody | core | Destructor body. |
| 0040d750 | FUN_0040d750 | VehicleEditor_GetDPlayData | core | Get DPlay data buffer. |
| 0040d770 | FUN_0040d770 | VehicleEditor_SetDPlayData | core | Set DPlay data buffer. |
| 0040d890 | FUN_0040d890 | VehicleEditor_InitTracks | core | Init track endpoints via EditorState. |
| 0040d8e0 | FUN_0040d8e0 | VehicleEditor_SetRenderOffset | core | Set viewport render offset. |
| 0040d940 | FUN_0040d940 | VehicleEditor_ProcessMove | core | Process movement delta. |
| 0040db90 | FUN_0040db90 | VehicleEditor_CheckBridge | core | Check if on bridge tile. |
| 0040dc20 | FUN_0040dc20 | VehicleEditor_MoveAlongTrack | core | Move N steps along track. |
| 0040df80 | FUN_0040df80 | VehicleEditor_CalcAngle | core | Calc angle from two points. |
| 0040e0d0 | FUN_0040e0d0 | VehicleEditor_GetResourceId | core | Get current vehicle resource ID. |
| 0040e130 | FUN_0040e130 | VehicleEditor_TriggerSound | core | Play sound by ID. |
| 0040e160 | FUN_0040e160 | VehicleEditor_BlitBackground | core | Blit editor background. |
| 0040e250 | FUN_0040e250 | VehicleEditor_IsInBounds | core | Check if point in bounds. |
| 0040e2a0 | FUN_0040e2a0 | VehicleEditor_CheckEdgeBounds | core | Check edge bounds. |
| 0040e340 | FUN_0040e340 | VehicleEditor_CheckVehicleAttach | core | Check vehicle attach possibility. |
| 0040e440 | FUN_0040e440 | VehicleEditor_CheckEditBounds1 | core | Edit bounds 1. |
| 0040e520 | FUN_0040e520 | VehicleEditor_CheckEditBounds2 | core | Edit bounds 2. |
| 0040e600 | FUN_0040e600 | CursorEditWindow_Ctor | ui | Constructor. |
| 0040e660 | FUN_0040e660 | CursorEditWindow_ScrDtor | ui | Scalar deleting destructor. |
| 0040e680 | FUN_0040e680 | CursorEditWindow_DtorBody | ui | Destructor body. |
| 0040e690 | FUN_0040e690 | CursorEditWindow_Init | ui | Init state. |
| 0040e8b0 | FUN_0040e8b0 | CursorEditWindow_Cleanup | ui | Cleanup resources. |
| 0040e950 | FUN_0040e950 | CGWND_ValidatePaletteData | core | Validate palette block. |
| 0040eb60 | FUN_0040eb60 | CGWND_MapResourceToDirection | core | Map resource ID to direction. |
| 0040ec70 | FUN_0040ec70 | AudioChannel_Init | audio | __fastcall. Flat struct, no vtable. |
| 0040eca0 | FUN_0040eca0 | AudioChannel_Release | audio | __fastcall. |
| 0040ecf0 | FUN_0040ecf0 | AudioChannel_Reset | audio | __fastcall. |
| 0040ed10 | FUN_0040ed10 | AudioChannel_SetOutput | audio | __fastcall. |
| 0040ed20 | FUN_0040ed20 | AudioChannel_LoadSound | audio | __fastcall. |
| 0040ee00 | FUN_0040ee00 | AudioChannel_Pause | audio | __fastcall. |
| 0040ee20 | FUN_0040ee20 | AudioChannel_Play | audio | __fastcall. |
| 0040eea0 | FUN_0040eea0 | AudioChannel_IsPlaying | audio | __fastcall. |
| 0040eeb0 | FUN_0040eeb0 | AudioChannel_IsActive | audio | __fastcall. |
| 0040ef00 | FUN_0040ef00 | AudioChannel_SetPosition | audio | __fastcall. |
| 0040ef20 | FUN_0040ef20 | AudioChannel_UpdatePosition | audio | __fastcall. |
| 0040f040 | FUN_0040f040 | AudioChannel_SetBounds | audio | __fastcall. |
| 0040f070 | FUN_0040f070 | AudioChannel_SetAttenuation | audio | __fastcall. Float param. |
| 0040f090 | FUN_0040f090 | AudioChannel_ApplyAttenuation | audio | __fastcall. |
| 0040f1c0 | FUN_0040f1c0 | AboutDialog_Ctor | ui | AboutDialog constructor. |
| 0040f270 | FUN_0040f270 | AboutDialog_ScrDtor | ui | Scalar deleting destructor. |
| 0040f290 | FUN_0040f290 | AboutDialog_BaseDtor | ui | Base destructor. |
| 0040f3c0 | FUN_0040f3c0 | Screensaver_Update | core | Animation update loop. |
| 0040f480 | FUN_0040f480 | Screensaver_Hide | core | Hide/cleanup screensaver. |
| 0040f510 | FUN_0040f510 | AboutDialog_Create | ui | Create window as child of parent HWND. |
| 0040f6a0 | FUN_0040f6a0 | Screensaver_InitSprites | core | Load screensaver sprites. |
| 0040f980 | FUN_0040f980 | AboutDialog_RenderCredits | ui | Scrolling credits text render. |
| 0040fe3f | Catch@0040fe3f | CRT_catch_handler_0040fe3f | core | C++ catch block handler |
| 0040fe50 | FUN_0040fe50 | CGWND_0040fe50 | core |  |
| 00410240 | FUN_00410240 | Game_LockMutex | core | Guards EnterCriticalSection behind init flag check; used by AboutDialog credits loader |
| 00410260 | FUN_00410260 | Game_UnlockMutex | core | Guards LeaveCriticalSection behind init flag check |
| 00410280 | FUN_00410280 | Game_RenderScreensaver | core | Screensaver renderer with fade-in|hold|fade-out animation phases (timer 0-93) |
| 00410510 | FUN_00410510 | Game_Ctor | core | Full constructor for Game singleton; extends Entity; allocates timer subsystem, init mouse params |
| 00410660 | FUN_00410660 | Game_Dtor | core | Scalar deleting dtor for Game class |
| 00410680 | FUN_00410680 | Game_BaseDtor | core | Base dtor; frees 40-byte timer buffer, delegates to GameObject_DtorBody |
| 00410700 | FUN_00410700 | Game_Shutdown | core | Counterpart to Game_Ctor; restores mouse params, stops timer, switches to windowed mode |
| 00410750 | FUN_00410750 | Game_LoadIntroSounds | core | Preloads 4 intro sounds (0x5014-0x501b) via resource manager keep-alive mechanism |
| 00410840 | FUN_00410840 | Game_Update | core | MAIN per-frame game loop; 7-step pipeline: animation, mouse, clicks, hover, mode-specific cursor |
| 00410a20 | FUN_00410a20 | Game_CheckScreensaverTimeout | core | Wrapper: calls IsScreensaverActive then clears mouse mode |
| 00410a40 | FUN_00410a40 | Game_IsScreensaverActive | core | Mouse-capture handler; processes captured mouse movement, drag-scrolling, entity hover routing |
| 00410d20 | FUN_00410d20 | Game_ClearMouseMode | core | Mouse click release handler in build mode; 3-phase: clear selection state, dispatch by build_mode, signal redraw |
| 00411000 | FUN_00411000 | Game_HandleLeftClick | core | Left click dispatch with priority order: town->DDRAW->scripted->selected->building->tile |
| 00411230 | FUN_00411230 | Game_HandleRightClick | core | Right click 3-phase: type match (cycle anim frame), type mismatch, cleanup deselect |
| 004113a0 | FUN_004113a0 | Game_SelectGameObject | core | Pick-up half of pick-up-and-place; removes from building|vehicle list, plays selection sound |
| 00411580 | FUN_00411580 | Game_DeselectGameObject | core | Reverses SelectGameObject; re-inserts into sorted list by distance order |
| 00411760 | FUN_00411760 | Game_UpdateCursorMode | core | Cursor feedback dispatcher by game state (MENU=noop, TOWN=input, BUILD=hover+clear) |
| 004117b0 | FUN_004117b0 | Game_HandleCursorHover | core | Complex cursor sound engine; 7-phase priority: placement, drag, overlay, build, world-edge detection |
| 00411ae0 | FUN_00411ae0 | Game_UpdateInputState | core | Town-mode cursor input; priority: blocking->scripted drag->DDRAW drag->building sprite->town view |
| 00411c50 | FUN_00411c50 | Game_SetCursorByResourceId | core | Draws selection cursor overlay at caller-supplied rect; 3 layers: cursor sprite, Game obj, connected overlay |
| 00411d10 | FUN_00411d10 | Game_ResetCursor | core | Full cursor refresh; redraws cursor sprite+Game obj+connected overlay using own screen_rect |
| 00411dc0 | FUN_00411dc0 | Game_SetScreenMode | core | Cursor capture state machine: release (IDC_ARROW), hide (gameplay), busy (busy.ani cursor) |
| 00411fb0 | FUN_00411fb0 | Game_PlaySound | core | Plays sound resource by ID with audio panning pos adjustment; 19 call sites |
| 00412060 | FUN_00412060 | Game_ScreenToWorld | core | Screen-to-world coord conversion: viewport offset, clamp, obj constraint, tile-grid snap to 16px |
| 00412410 | FUN_00412410 | TimerSlotList_DtorBody | core | Dtor for 16-byte TimerSlotList struct; frees 10-int buffer; embedded in Game+0x10C and INPUT+0x04 |
| 00412540 | FUN_00412540 | Game_CheckIdleTimeout | core | Linear search for timer ID in items array; BUG: returns 0 not -1 when count=0 |
| 00412580 | FUN_00412580 | TimerSlotList_Dtor_Dead | core | Partial collection reset: frees items but preserves count capacity |
| 004125c0 | FUN_004125c0 | TimerSlotList_Dtor_Active | core | Full collection teardown; delegates to DtorBody then conditional free |
| 004125e0 | FUN_004125e0 | Game_ScalarDeletingDtor | core | MSVC scalar deleting dtor; calls Game_DtorBody then conditional free |
| 00412600 | FUN_00412600 | GameObjectBase_ScalarDeletingDtor | core | MSVC scalar deleting dtor; calls GameObject_MarkDead then conditional free |
| 00412620 | FUN_00412620 | TrackPos_Init | core | Full initializer for 20-byte TrackPos struct; sets all 4 fields to -1 |
| 00412660 | FUN_00412660 | TrackPos_BaseInit | core | Lightweight initializer; sets only vtable, skips data fields |
| 00412670 | FUN_00412670 | TrackPos_IsObjectBetween | core | 1D overlap check on circular 12-segment track with wrap-around |
| 00412710 | FUN_00412710 | Game_CheckTimeInRange | core | Time range check: min-since-midnight with overnight wrap; used by Building_DecideAction |
| 00412790 | FUN_00412790 | Game_IsPositionBetween | core | Date+time range check with wrap; used by INPUT_EditSetFocus for scheduled events |
| 00412870 | FUN_00412870 | GameVehicle_Ctor | core | GameVehicle constructor; zeroes 4 fields, sets vehicle_kind=4 (road) |
| 004128b0 | FUN_004128b0 | GameVehicle_Dtor | core | Scalar deleting dtor; calls BaseDtor then conditional free |
| 004128d0 | FUN_004128d0 | GameVehicle_BaseDtor | core | Base dtor; frees linked list at +0x124, chains to RESDATA base dtor |
| 004129c0 | FUN_004129c0 | GameVehicle_StartMoving | core | Assigns vehicle to building as occupant; sets busy flag, copies node ID |
| 00412a80 | FUN_00412a80 | GameVehicle_Update | core | Per-frame tick for vehicles |
| 00412af0 | FUN_00412af0 | GameVehicle_AddDestination | core | Appends vehicle to destination linked list |
| 00412b50 | FUN_00412b50 | GameVehicle_RemoveDestination | core | Removes destination matching vehicleId from linked list |
| 00412bd0 | FUN_00412bd0 | GameAudio_Ctor | core | GameAudio constructor; zeroes all audio fields |
| 00412c20 | FUN_00412c20 | GameAudio_Dtor | core | GameAudio dtor; calls Cleanup then conditional free |
| 00412c50 | FUN_00412c50 | GameAudio_Init | core | Full audio init: DirectSound, 16 channels, PCM 22050Hz 16-bit stereo |
| 00412ee0 | FUN_00412ee0 | GameAudio_Cleanup | core | Releases all audio resources |
| 00413070 | FUN_00413070 | GameAudio_Play | core | Thin wrapper delegating playback to audio device vtable |
| 004130a0 | FUN_004130a0 | GameAudio_SetListenerPos | core | Sets listener position for spatial audio panning on all channels |
| 004130f0 | FUN_004130f0 | GameAudio_StopFinished | core | Releases only active channels; DSERR_BUFFERLOST recovery |
| 00413140 | FUN_00413140 | GameAudio_StopAll | core | Unconditional release of all channels; resource mgr shutdown |
| 00413180 | FUN_00413180 | GameAudio_PlayResource | core | Fire-and-forget sound by resource ID using global listener pos |
| 004131c0 | FUN_004131c0 | GameAudio_PlayResourceEx | core | Sound playback with channel tracking; used by SetMode and HelpWnd |
| 00413210 | FUN_00413210 | GameAudio_AllocChannel | core | 5-pass audio channel allocator: audit, reuse, priority steal, load, play |
| 00413530 | FUN_00413530 | GameAudio_SetMute | core | Hard mute with backup+restore of 4 volume levels |
| 004135b0 | FUN_004135b0 | GameAudio_UpdateVolume | core | Lightweight silence for UI mode transitions; preserves mute state |
| 00413630 | FUN_00413630 | GameAudio_SetBounds | core | Stores 4 volume boundaries and re-applies mute |
| 00413660 | FUN_00413660 | Game_LoadWaveFile | core | RIFF-WAVE parser with SEH; loads sound data via AssetMgr or filesystem |
| 00413971 | Catch@00413971 | CRT_catch_handler_00413971 | core | C++ catch block handler |
| 00413980 | FUN_00413980 | Game_ReadChunk | core | RIFF chunk reader for WAVE files; 3 modes, 2 bugs (uninit stack, odd-size overshoot) |
| 00413ab0 | FUN_00413ab0 | GameWindow_Ctor | core | Base constructor for GameWindow class; sets vtable, stores hInstance, formats title |
| 00413b50 | FUN_00413b50 | GameWindow_Dtor | core | Scalar deleting dtor for GameWindow; calls BaseDtor then conditional free |
| 00413b70 | FUN_00413b70 | GameWindow_BaseDtor | core | Releases DDRAW objects, cursor ref, backbuffer surface |
| 00413c10 | FUN_00413c10 | GameWindow_Hide | core | Virtual hide: saves screen pixels, kills 190ms timer, ShowWindow(SW_HIDE) |
| 00413d10 | FUN_00413d10 | GameWindow_Show | core | Restores saved pixels, sets 190ms timer, ShowWindow(SW_SHOW) |
| 00413d90 | FUN_00413d90 | GameWindow_SetPosition | core | Repositions window using stored dimensions |
| 00413de0 | FUN_00413de0 | GameWindow_Create | core | RegisterClass+CreateWindowEx+CreateSurface+Init callback |
| 004140a0 | FUN_004140a0 | Cursor_UpdateClientRect | input/cursor/UI | Syncs cached client rect with Win32 GetClientRect after window creation|resize |
| 00414130 | FUN_00414130 | Cursor_InitSprites | input/cursor/UI | Loads cursor sprite sheets, creates shared 256x256 backbuffer singleton |
| 00414290 | FUN_00414290 | Cursor_SetCapture | input/cursor/UI | Mouse capture gatekeeper; manages SetCapture|ReleaseCapture and cursor visibility |
| 00414340 | FUN_00414340 | Cursor_SetMode | input/cursor/UI | Sets cursor animation state; handles state transitions with forceRedraw |
| 00414a80 | FUN_00414a80 | Cursor_HandleWindowPaint | input/cursor/UI | Window paint handler; checks hWnd match, updates dirty rect, renders viewport |
| 00414b80 | FUN_00414b80 | Cursor_DestroyWindow | input/cursor/UI | Destroys window, posts WM_QUIT if top-level |
| 00414bb0 | FUN_00414bb0 | Cursor_WaitForBlit | input/cursor/UI | Polls blit completion with 10s timeout; exits process on failure |
| 00414c20 | FUN_00414c20 | Cursor_Render | input/cursor/UI | Full render: GetCursorPos, hotspot offset, frame animation, 3-blit sequence |
| 00414ef0 | FUN_00414ef0 | Cursor_UnlockAllSurfaces | input/cursor/UI | Polls 5 global objects for locked surfaces and unlocks them |
| 00414fb0 | FUN_00414fb0 | Cursor_UpdateDirtyRect | input/cursor/UI | Core dirty-rect tracking; unions old+new cursor rects, restores background, redraws |
| 00415440 | FUN_00415440 | Cursor_RenderWithViewport | input/cursor/UI | Core cursor compositing with viewport clipping and 2 render paths |
| 00415980 | FUN_00415980 | Cursor_Ctor | input/cursor/UI | Cursor constructor (0x740 bytes); delegates to UI_WindowBase_Ctor then Cursor_Init |
| 004159e0 | FUN_004159e0 | Cursor_Dtor | input/cursor/UI | Scalar deleting dtor; calls Cursor_BaseDtor then conditional free |
| 00415a00 | FUN_00415a00 | Cursor_Init | input/cursor/UI | Massive init: 22+35 sprites, Edit_colour.dat, random bonus IDs, toolbar table |
| 00416460 | FUN_00416460 | Cursor_InitBackground | input/cursor/UI | Creates 1280x1024 UIPANEL bg surface; composites 4 resources via Town_BlitElement |
| 004166b0 | FUN_004166b0 | Cursor_BaseDtor | input/cursor/UI | Releases 40+ sprite objects, GDI brush, chains to UI_WindowBase_BaseDtor |
| 004169e0 | FUN_004169e0 | Cursor_Create | input/cursor/UI | Creates full-screen overlay and EDIT control for cursor editor |
| 00416b80 | FUN_00416b80 | Cursor_Show | input/cursor/UI | Activates cursor editor mode; init sprites, capture mouse, 50ms timer |
| 00416e00 | FUN_00416e00 | Cursor_UpdateNetworkNames | input/cursor/UI | Populates 26-slot player name array from netman or local player |
| 00416f70 | FUN_00416f70 | Cursor_Hide | input/cursor/UI | Hides cursor editor; kills timers, releases sprites, leaves DPLAY session |
| 00417f20 | FUN_00417f20 | Cursor_InitEditorSprites | input/cursor/UI | Batch-loads editor sprite sheet; initializes ~49 UISprite objects |
| 004180a0 | FUN_004180a0 | CURSOR_004180a0 | input/cursor/UI |  |
| 00418210 | FUN_00418210 | CURSOR_00418210 | input/cursor/UI |  |
| 00418340 | FUN_00418340 | CURSOR_00418340 | input/cursor/UI |  |
| 00418450 | FUN_00418450 | CURSOR_00418450 | input/cursor/UI |  |
| 00418780 | FUN_00418780 | CURSOR_00418780 | input/cursor/UI |  |
| 004189a0 | FUN_004189a0 | CURSOR_004189a0 | input/cursor/UI |  |
| 00418a90 | FUN_00418a90 | CURSOR_00418a90 | input/cursor/UI |  |
| 00418e20 | FUN_00418e20 | CURSOR_00418e20 | input/cursor/UI |  |
| 00419260 | FUN_00419260 | CURSOR_00419260 | input/cursor/UI |  |
| 00419560 | FUN_00419560 | CURSOR_00419560 | input/cursor/UI |  |
| 00419680 | FUN_00419680 | CURSOR_00419680 | input/cursor/UI |  |
| 004198b0 | FUN_004198b0 | CURSOR_004198b0 | input/cursor/UI |  |
| 00419a60 | FUN_00419a60 | CURSOR_00419a60 | input/cursor/UI |  |
| 00419b10 | FUN_00419b10 | CURSOR_00419b10 | input/cursor/UI |  |
| 0041a050 | FUN_0041a050 | INPUT_0041a050 | input/cursor/UI |  |
| 0041a0e0 | FUN_0041a0e0 | INPUT_0041a0e0 | input/cursor/UI |  |
| 0041a210 | FUN_0041a210 | INPUT_0041a210 | input/cursor/UI |  |
| 0041a360 | FUN_0041a360 | INPUT_0041a360 | input/cursor/UI |  |
| 0041a460 | FUN_0041a460 | INPUT_0041a460 | input/cursor/UI |  |
| 0041a650 | FUN_0041a650 | INPUT_0041a650 | input/cursor/UI |  |
| 0041aa40 | FUN_0041aa40 | INPUT_0041aa40 | input/cursor/UI |  |
| 0041aae0 | FUN_0041aae0 | INPUT_0041aae0 | input/cursor/UI |  |
| 0041d250 | FUN_0041d250 | INPUT_0041d250 | input/cursor/UI |  |
| 0041d2b0 | FUN_0041d2b0 | INPUT_0041d2b0 | input/cursor/UI |  |
| 0041d2d0 | FUN_0041d2d0 | INPUT_0041d2d0 | input/cursor/UI |  |
| 0041d310 | FUN_0041d310 | INPUT_0041d310 | input/cursor/UI |  |
| 0041d320 | FUN_0041d320 | INPUT_0041d320 | input/cursor/UI |  |
| 0041d5c0 | FUN_0041d5c0 | INPUT_0041d5c0 | input/cursor/UI |  |
| 0041d8f0 | FUN_0041d8f0 | INPUT_0041d8f0 | input/cursor/UI |  |
| 0041d920 | FUN_0041d920 | INPUT_0041d920 | input/cursor/UI |  |
| 0041d950 | FUN_0041d950 | INPUT_0041d950 | input/cursor/UI |  |
| 0041d980 | FUN_0041d980 | INPUT_0041d980 | input/cursor/UI |  |
| 0041d9b0 | FUN_0041d9b0 | INPUT_0041d9b0 | input/cursor/UI |  |
| 0041dd40 | FUN_0041dd40 | INPUT_0041dd40 | input/cursor/UI |  |
| 0041dd80 | FUN_0041dd80 | INPUT_0041dd80 | input/cursor/UI |  |
| 0041def0 | FUN_0041def0 | INPUT_0041def0 | input/cursor/UI |  |
| 0041e100 | FUN_0041e100 | INPUT_0041e100 | input/cursor/UI |  |
| 0041e120 | FUN_0041e120 | INPUT_0041e120 | input/cursor/UI |  |
| 0041e1f0 | FUN_0041e1f0 | INPUT_0041e1f0 | input/cursor/UI |  |
| 0041e570 | FUN_0041e570 | INPUT_0041e570 | input/cursor/UI |  |
| 0041e600 | FUN_0041e600 | INPUT_0041e600 | input/cursor/UI |  |
| 0041e620 | FUN_0041e620 | INPUT_0041e620 | input/cursor/UI |  |
| 0041e6e0 | FUN_0041e6e0 | INPUT_0041e6e0 | input/cursor/UI |  |
| 0041e9f0 | FUN_0041e9f0 | INPUT_0041e9f0 | input/cursor/UI |  |
| 0041efa0 | FUN_0041efa0 | INPUT_0041efa0 | input/cursor/UI |  |
| 0041f0c0 | FUN_0041f0c0 | INPUT_0041f0c0 | input/cursor/UI |  |
| 0041f2b0 | FUN_0041f2b0 | INPUT_0041f2b0 | input/cursor/UI |  |
| 0041f430 | FUN_0041f430 | INPUT_0041f430 | input/cursor/UI |  |
| 0041f480 | FUN_0041f480 | INPUT_0041f480 | input/cursor/UI |  |
| 0041f4e0 | FUN_0041f4e0 | INPUT_0041f4e0 | input/cursor/UI |  |
| 0041f540 | FUN_0041f540 | INPUT_0041f540 | input/cursor/UI |  |
| 0041f590 | FUN_0041f590 | INPUT_0041f590 | input/cursor/UI |  |
| 0041f5e0 | FUN_0041f5e0 | INPUT_LoadConfig | input/cursor/UI | RENAMED 2026-07: Was INPUT_0041f5e0 (also misnamed INPUT_Init). Loads input config from LOCO.INI [LoadEvents] section. NOT a constructor. |
| 0041f6e0 | FUN_0041f6e0 | INPUT_0041f6e0 | input/cursor/UI |  |
| 0041f7e0 | FUN_0041f7e0 | INPUT_0041f7e0 | input/cursor/UI |  |
| 0041f8e0 | FUN_0041f8e0 | INPUT_0041f8e0 | input/cursor/UI |  |
| 0041f970 | FUN_0041f970 | INPUT_0041f970 | input/cursor/UI |  |
| 0041fb20 | FUN_0041fb20 | INPUT_0041fb20 | input/cursor/UI |  |
| 0041fbe0 | FUN_0041fbe0 | INPUT_0041fbe0 | input/cursor/UI |  |
| 0041fd00 | FUN_0041fd00 | INPUT_0041fd00 | input/cursor/UI |  |
| 0041ff20 | FUN_0041ff20 | INPUT_0041ff20 | input/cursor/UI |  |
| 00420000 | FUN_00420000 | UI_00420000 | input/cursor/UI |  |
| 004202f0 | FUN_004202f0 | UI_004202f0 | input/cursor/UI |  |
| 004203a0 | FUN_004203a0 | UI_004203a0 | input/cursor/UI |  |
| 004203c0 | FUN_004203c0 | UI_004203c0 | input/cursor/UI |  |
| 004204d0 | FUN_004204d0 | UI_004204d0 | input/cursor/UI |  |
| 004206b0 | FUN_004206b0 | UI_004206b0 | input/cursor/UI |  |
| 00420860 | FUN_00420860 | UI_00420860 | input/cursor/UI |  |
| 004208f0 | FUN_004208f0 | UI_004208f0 | input/cursor/UI |  |
| 00421200 | FUN_00421200 | UI_00421200 | input/cursor/UI |  |
| 00421500 | FUN_00421500 | UI_00421500 | input/cursor/UI |  |
| 004216f0 | FUN_004216f0 | UI_004216f0 | input/cursor/UI |  |
| 00421ae0 | FUN_00421ae0 | UI_00421ae0 | input/cursor/UI |  |
| 00422010 | FUN_00422010 | UI_00422010 | input/cursor/UI |  |
| 00422440 | FUN_00422440 | UI_00422440 | input/cursor/UI |  |
| 00422570 | FUN_00422570 | UI_00422570 | input/cursor/UI |  |
| 00422660 | FUN_00422660 | UI_00422660 | input/cursor/UI |  |
| 00422820 | FUN_00422820 | UI_00422820 | input/cursor/UI |  |
| 00422d80 | FUN_00422d80 | UI_00422d80 | input/cursor/UI |  |
| 00422ea0 | FUN_00422ea0 | UI_00422ea0 | input/cursor/UI |  |
| 00422ec0 | FUN_00422ec0 | UI_00422ec0 | input/cursor/UI |  |
| 004234e0 | FUN_004234e0 | UI_004234e0 | input/cursor/UI |  |
| 00423500 | FUN_00423500 | UI_00423500 | input/cursor/UI |  |
| 00423560 | FUN_00423560 | UI_00423560 | input/cursor/UI |  |
| 00423840 | FUN_00423840 | UI_00423840 | input/cursor/UI |  |
| 00423870 | FUN_00423870 | UI_00423870 | input/cursor/UI |  |
| 00423890 | FUN_00423890 | UI_00423890 | input/cursor/UI |  |
| 004238c0 | FUN_004238c0 | UI_004238c0 | input/cursor/UI |  |
| 004239c0 | FUN_004239c0 | UI_004239c0 | input/cursor/UI |  |
| 004239e0 | FUN_004239e0 | UI_004239e0 | input/cursor/UI |  |
| 00423a90 | FUN_00423a90 | UI_00423a90 | input/cursor/UI |  |
| 00423ab0 | FUN_00423ab0 | UI_00423ab0 | input/cursor/UI |  |
| 00423c50 | FUN_00423c50 | UI_00423c50 | input/cursor/UI |  |
| 00423d00 | FUN_00423d00 | UI_00423d00 | input/cursor/UI |  |
| 00423d20 | FUN_00423d20 | UI_00423d20 | input/cursor/UI |  |
| 00423d70 | FUN_00423d70 | UI_00423d70 | input/cursor/UI |  |
| 00423e00 | FUN_00423e00 | UI_00423e00 | input/cursor/UI |  |
| 00423e80 | FUN_00423e80 | UI_00423e80 | input/cursor/UI |  |
| 00423f00 | FUN_00423f00 | UI_00423f00 | input/cursor/UI |  |
| 00423f80 | FUN_00423f80 | UI_00423f80 | input/cursor/UI |  |
| 00424040 | FUN_00424040 | UI_00424040 | input/cursor/UI |  |
| 004241e0 | FUN_004241e0 | UI_004241e0 | input/cursor/UI |  |
| 00424250 | FUN_00424250 | UI_00424250 | input/cursor/UI |  |
| 00424270 | FUN_00424270 | UI_00424270 | input/cursor/UI |  |
| 00424460 | FUN_00424460 | UI_00424460 | input/cursor/UI |  |
| 00424490 | FUN_00424490 | UI_00424490 | input/cursor/UI |  |
| 00424510 | FUN_00424510 | UI_00424510 | input/cursor/UI |  |
| 00424550 | FUN_00424550 | UI_00424550 | input/cursor/UI |  |
| 00424820 | FUN_00424820 | UI_00424820 | input/cursor/UI |  |
| 00424a00 | FUN_00424a00 | UI_00424a00 | input/cursor/UI |  |
| 00424a30 | FUN_00424a30 | UI_00424a30 | input/cursor/UI |  |
| 00424a70 | FUN_00424a70 | UI_00424a70 | input/cursor/UI |  |
| 00424a90 | FUN_00424a90 | UI_00424a90 | input/cursor/UI |  |
| 00424ad0 | FUN_00424ad0 | UI_00424ad0 | input/cursor/UI |  |
| 00424af0 | FUN_00424af0 | UI_00424af0 | input/cursor/UI |  |
| 00424b40 | FUN_00424b40 | UI_00424b40 | input/cursor/UI |  |
| 00424ba0 | FUN_00424ba0 | UI_00424ba0 | input/cursor/UI |  |
| 00424bf0 | FUN_00424bf0 | UI_00424bf0 | input/cursor/UI |  |
| 00424e00 | FUN_00424e00 | UI_00424e00 | input/cursor/UI |  |
| 004255f0 | FUN_004255f0 | UI_004255f0 | input/cursor/UI |  |
| 00425670 | FUN_00425670 | UI_00425670 | input/cursor/UI |  |
| 004257f0 | FUN_004257f0 | UI_004257f0 | input/cursor/UI |  |
| 00425870 | FUN_00425870 | UI_00425870 | input/cursor/UI |  |
| 004258f0 | FUN_004258f0 | UI_004258f0 | input/cursor/UI |  |
| 00425910 | FUN_00425910 | UI_00425910 | input/cursor/UI |  |
| 00425990 | FUN_00425990 | UI_00425990 | input/cursor/UI |  |
| 004259c0 | FUN_004259c0 | UI_004259c0 | input/cursor/UI |  |
| 00425a50 | FUN_00425a50 | UI_00425a50 | input/cursor/UI |  |
| 00425ac0 | FUN_00425ac0 | UI_00425ac0 | input/cursor/UI |  |
| 00425b70 | FUN_00425b70 | UI_00425b70 | input/cursor/UI |  |
| 00425d30 | FUN_00425d30 | UI_00425d30 | input/cursor/UI |  |
| 00425dc0 | FUN_00425dc0 | Cursor_SetupSurface | input/cursor/UI | RENAMED 2026-07: Was CGWND_SetupCursorSurface. Sets up cursor rendering surface. Located in UI range, not core. |
| 00425f20 | FUN_00425f20 | UI_00425f20 | input/cursor/UI |  |
| 00426900 | FUN_00426900 | UIPANEL_00426900 | input/cursor/UI |  |
| 00426a90 | FUN_00426a90 | UIPANEL_00426a90 | input/cursor/UI |  |
| 00426b00 | FUN_00426b00 | UIPANEL_00426b00 | input/cursor/UI |  |
| 00426b70 | FUN_00426b70 | UIPANEL_00426b70 | input/cursor/UI |  |
| 00426b90 | FUN_00426b90 | UIPANEL_00426b90 | input/cursor/UI |  |
| 00426eb0 | FUN_00426eb0 | UIPANEL_00426eb0 | input/cursor/UI |  |
| 00427370 | FUN_00427370 | UIPANEL_00427370 | input/cursor/UI |  |
| 00427440 | FUN_00427440 | UIPANEL_00427440 | input/cursor/UI |  |
| 00427460 | FUN_00427460 | UIPANEL_00427460 | input/cursor/UI |  |
| 00427520 | FUN_00427520 | UIPANEL_00427520 | input/cursor/UI |  |
| 00427580 | FUN_00427580 | UIPANEL_00427580 | input/cursor/UI |  |
| 004277d0 | FUN_004277d0 | UIPANEL_004277d0 | input/cursor/UI |  |
| 00428400 | FUN_00428400 | UIPANEL_00428400 | input/cursor/UI |  |
| 00428550 | FUN_00428550 | UIPANEL_00428550 | input/cursor/UI |  |
| 00428770 | FUN_00428770 | UIPANEL_00428770 | input/cursor/UI |  |
| 004287b0 | FUN_004287b0 | UIPANEL_004287b0 | input/cursor/UI |  |
| 004289a0 | FUN_004289a0 | UIPANEL_004289a0 | input/cursor/UI |  |
| 00428f90 | FUN_00428f90 | UIPANEL_00428f90 | input/cursor/UI |  |
| 00429490 | FUN_00429490 | UIPANEL_00429490 | input/cursor/UI |  |
| 00429820 | FUN_00429820 | UIPANEL_00429820 | input/cursor/UI |  |
| 00429830 | FUN_00429830 | UIPANEL_00429830 | input/cursor/UI |  |
| 00429850 | FUN_00429850 | UIPANEL_00429850 | input/cursor/UI |  |
| 00429a10 | FUN_00429a10 | UIPANEL_00429a10 | input/cursor/UI |  |
| 00429b20 | FUN_00429b20 | UIPANEL_00429b20 | input/cursor/UI |  |
| 00429dd0 | FUN_00429dd0 | UIPANEL_00429dd0 | input/cursor/UI |  |
| 00429ef0 | FUN_00429ef0 | UIPANEL_00429ef0 | input/cursor/UI |  |
| 0042a110 | FUN_0042a110 | UIPANEL_0042a110 | input/cursor/UI |  |
| 0042a140 | FUN_0042a140 | UIPANEL_0042a140 | input/cursor/UI |  |
| 0042a1c0 | FUN_0042a1c0 | UIPANEL_0042a1c0 | input/cursor/UI |  |
| 0042a370 | FUN_0042a370 | UIPANEL_0042a370 | input/cursor/UI |  |
| 0042a3d0 | FUN_0042a3d0 | UIPANEL_0042a3d0 | input/cursor/UI |  |
| 0042a540 | FUN_0042a540 | UIPANEL_0042a540 | input/cursor/UI |  |
| 0042a5f0 | FUN_0042a5f0 | UIPANEL_0042a5f0 | input/cursor/UI |  |
| 0042a610 | FUN_0042a610 | UIPANEL_0042a610 | input/cursor/UI |  |
| 0042a850 | FUN_0042a850 | UIPANEL_0042a850 | input/cursor/UI |  |
| 0042a980 | FUN_0042a980 | UIPANEL_0042a980 | input/cursor/UI |  |
| 0042aa90 | FUN_0042aa90 | UIPANEL_0042aa90 | input/cursor/UI |  |
| 0042ab10 | FUN_0042ab10 | UIPANEL_0042ab10 | input/cursor/UI |  |
| 0042af01 | Catch@0042af01 | CRT_catch_handler_0042af01 | input/cursor/UI | C++ catch block handler |
| 0042af30 | FUN_0042af30 | UIPANEL_0042af30 | input/cursor/UI |  |
| 0042b050 | FUN_0042b050 | UIPANEL_0042b050 | input/cursor/UI |  |
| 0042b960 | FUN_0042b960 | Town_BlitElement | town rendering | RENAMED 2026-07: Was UIPANEL_0042b960. Blit a UI element: reads bitmap/surface ptr from element+0x1c and forwards to UIPANEL_Blit. Called from UI_MainMenu_Render, Cursor_InitBackground, etc. |
| 0042b9c0 | FUN_0042b9c0 | UIPANEL_0042b9c0 | input/cursor/UI |  |
| 0042ba90 | FUN_0042ba90 | UIPANEL_0042ba90 | input/cursor/UI |  |
| 0042bb90 | FUN_0042bb90 | UIPANEL_0042bb90 | input/cursor/UI |  |
| 0042bc80 | FUN_0042bc80 | UIPANEL_0042bc80 | input/cursor/UI |  |
| 0042bd70 | FUN_0042bd70 | UIPANEL_0042bd70 | input/cursor/UI |  |
| 0042bec0 | FUN_0042bec0 | UIPANEL_0042bec0 | input/cursor/UI |  |
| 0042c050 | FUN_0042c050 | Town_DrawTiles16bpp_Strided | town rendering | 8bpp to 16bpp strided blitter. Standard left-to-right, top-to-bottom. Palette: idx0=transparent, idx1=shadow (half-bright), idx2+=tile colors. |
| 0042c130 | FUN_0042c130 | Town_DrawTiles16bpp_Reversed | town rendering | 8bpp to 16bpp horizontally-reversed blitter. Reads source bytes right-to-left within each row for horizontal mirror. |
| 0042c220 | FUN_0042c220 | Town_DrawTiles16bpp_Checker | town rendering | 8bpp to 16bpp checkerboard blitter. Renders every other row (even rows only). Adjusts offset when clip_top is odd. |
| 0042c330 | FUN_0042c330 | Town_CopyTiles8bpp_Transparent | town rendering | Copy 8bpp tile region from source to dest, skipping zero (transparent) pixels. Used for clock digit sprites. |
| 0042c3d0 | FUN_0042c3d0 | Town_CopyTiles8bpp_Direct | town rendering | Copy 8bpp tile region (direct, no transparency). Used for UI panel background fills. |
| 0042c470 | FUN_0042c470 | Town_DrawTiles16bpp_Staggered | town rendering | 16bpp staggered tile blitter with semi-transparency checkerboard effect. Every other pixel dimmed (shift right by 1) for isometric dither pattern. |
| 0042c590 | FUN_0042c590 | Town_CalcScrollRect | town rendering | Calculate visible tile rectangle from scroll position. Compensates for negative clip offsets. Outputs source-surface rect and visible-screen rect. BUG: always returns FALSE. |
| 0042c700 | FUN_0042c700 | Town_CalcScrollRect_Reversed | town rendering | Calculate visible tile rectangle with reversed scroll direction. Clips viewportRect vs pClipRect (not self-intersect). Returns TRUE on success. Used for multiplayer/mirrored views. |
| 0042c890 | FUN_0042c890 | Town_BlitTileSurface | town rendering | Blit 8-bit palette-indexed tiles to 16-bit DirectDraw surface. Reads right-to-left from index buffer, writes left-to-right. Zero index = transparent. draw_op == 0x20. |
| 0042c950 | FUN_0042c950 | Town_CheckOccupied | town rendering | Scan rectangle of tiles for non-zero occupancy. Delegates to Town_CheckOccupiedEx (DDraw surface) if this+0x04 non-null, otherwise scans byte array. |
| 0042c9f0 | FUN_0042c9f0 | Town_CheckOccupiedEx | town rendering | Extended tile occupancy check via DDraw surface lock. Occupied when ((tile & g_mask1) >> g_shift) != 0x1f AND (tile & g_mask2) != 0x1f. Manages lock/unlock with loss recovery. |
| 0042cb10 | FUN_0042cb10 | Town_BlitViewport | town rendering | Viewport occupancy check for collision detection. Mode 0: checks index_buffer[y*pitch+x]. Mode 1: locks DDraw surface, reads 16-bit pixel against water bit pattern globals. Returns 1=empty, 0=occupied. |
| 0042cce0 | FUN_0042cce0 | Town_ScrollView | town lifecycle | ScrollView constructor (__fastcall). Initializes RESDATA base, creates GameObject sub-object at +0xE4, sets vtable to 0x477D30, type=14, scroll offsets X/Y at +0xE0/+0x17C to 0. |
| 0042cd60 | FUN_0042cd60 | Town_GameView_Dtor | town lifecycle | RENAMED 2026-07: Was Town_UpdateCursor. Actually scalar deleting dtor for GameView vtable (0x477D30). Calls Town_GameView_DtorBody then conditional free. No cursor logic. |
| 0042cd80 | FUN_0042cd80 | Town_GameView_DtorBody | town lifecycle | RENAMED 2026-07: Was Town_DestroySubObjects. Actually dtor body for GameView vtable (0x477D30). Resets vtable, destroys sub-object at +0xE4, calls Game base dtor. |
| 0042cdd0 | FUN_0042cdd0 | Town_GameView_Cleanup | town lifecycle | RENAMED 2026-07: Was Town_Release. Vtable+0x3C cleanup for GameView vtable (0x477D30). Destroys child at +0x17C, calls InvalidateRect cleanup, then RESDATA_DtorBase. NOT COM Release. |
| 0042ce10 | FUN_0042ce10 | Town_HandleTileClick | town rendering | Initialize town view click-handling sprites. Loads 3 cursor indicator sprites (valid=0x3807, invalid=0x3808, hover=0x3806) and creates overlay UIPANEL for placement feedback. Returns 1 if anim resources loaded. |
| 0042cf90 | FUN_0042cf90 | Town_IsValidPlacement | town rendering | Check whether a building/track can be placed at given tile entity. Validates entity active (byte+0x18==1), then checks tile type: 0x07=always, 0x02/0x06/0x08=visible, 0x04=connected, 0x03=building, 0x0C=large-id. |
| 0042d040 | FUN_0042d040 | Town_SelectBuilding | town rendering | Select/focus a building in town view. Centers viewport, sets zoom (1 for type-6 depot, 3 for others), invalidates tile rect, notifies DDRAW. NULL arg deselects (clears flag, resets panels). |
| 0042d1a0 | FUN_0042d1a0 | Town_TrackBuilding | town rendering | Per-frame tracking of selected building. Auto-deselects invisible depot (type 6). Centers viewport on building center if moved. Updates child objects (linked list at +0xD0) and child panel (+0xE4). |
| 0042d280 | FUN_0042d280 | Town_DeselectBuilding | town rendering | Remove building selection overlay. Computes clip rect from viewport_inset and overlay dimensions, blits cached background pixels from backup surface back to primary, then re-blits UI panel overlay. |
| 0042d3a0 | FUN_0042d3a0 | Town_UpdateSelection | town rendering | Blit selection overlay panel onto primary surface. Reads source RECT from +0xEC and dest RECT from +0x114, calls UIPANEL_Blit with flag 0x40 (scroll-aware). |
| 0042d400 | FUN_0042d400 | Town_RenderSelection | town rendering | Render selection highlight for a single tile. If selection_active (+0x88) set, calls GameObject_Draw to draw flashing selection outline around the tile. |
| 0042d670 | FUN_0042d670 | Town_PostcardClickHandler | town postcard | RENAMED 2026-07: Was Town_OpenPostcard. Left-click handler for postcard overlay. Checks +0x88 flag, delegates to RESDATA_HitTestChildren. Not a dialog opener. |
| 0042d6b0 | FUN_0042d6b0 | Town_PostcardCommandHandler | town postcard | RENAMED 2026-07: Was Town_ClosePostcard. WM_COMMAND handler for postcard child controls (vtable 0x477D74). Dispatches 0x3806/08 (zoom) and 0x3807 (select building). Not a dialog closer. |
| 0042d770 | FUN_0042d770 | Town_SendPostcard | town postcard | __thiscall message handler (vtable 0x477D80). Handles 0x3806 (send: deselect building, save world), 0x3807 (zoom: DDRAW_SelectBuilding), 0x3808 (deselect: reset zoom). BUG: World_SaveToFile return discarded. |
| 0042d8a0 | FUN_0042d8a0 | Town_ReceivePostcard | town postcard | __fastcall. Processes incoming DPLAY postcard. Enumerates Sort_In/Sort_Out .crd files, resolves player IDs, registers sender in Album/. Manages session_player (+0x608), is_host (+0x606). NOTE: null-check ordering at 0x42D8F4. |
| 0042da10 | FUN_0042da10 | Town_SavePostcard | town postcard | Client-side DPLAY session mgmt. Enumerates hosts (type 2), unregisters stale players, re-registers as client (mode 1). No actual file I/O — real save is Town_SavePostcardToFile (0x430A90). |
| 0042db30 | FUN_0042db30 | Town_LoadPostcard | town postcard | Host-side DPLAY session mgmt. Enumerates players (type 1), unregisters stale, re-registers as host (mode 2). Counts remaining players → has_connections (+0x607). No actual file I/O. |
| 0042dc50 | FUN_0042dc50 | Town_DeletePostcard | town postcard | DPLAY session teardown. Enumerates hosts (type 2), unregisters stale, deregisters player, destroys player object. Counts remaining players -> has_connections (+0x607). |
| 0042dd50 | FUN_0042dd50 | Town_ListPostcards | town postcard | DPLAY player enumeration and session management. Enumerates hosts (type 2), resolves addresses, manages session_player (+0x608). Cycles through linked list of players. |
| 0042de70 | Town_ReadPostcardFile | Town_PostcardUpdateUI | town postcard | MISNAMED: postcard dialog idle/release UI handler. Dispatches 8 UI actions (2-9). Resets sprites to state 0, checks pending DPLAY messages. Complementary to Town_PostcardDlgProc (0x42E150). No file I/O. |
| 0042e150 | FUN_0042e150 | Town_PostcardDlgProc | town postcard | NOT a Windows DlgProc. Custom action dispatch handler (actions 2-9). Complements PostcardUpdateUI. BUG: case 8 always uses DPLAY_GetMessageCount ignoring is_host flag. |
| 0042e420 | FUN_0042e420 | Town_PostcardInitList | town postcard | vtable[8] WM_INITDIALOG-style handler. Guards on +0x5F9. Calls SendHandler(0) to pre-render, resets 8 button sprites to idle via PostcardUpdateUI, clears overlay, SetFocus. |
| 0042e4e0 | FUN_0042e4e0 | Town_PostcardUpdateButtons | town postcard | Blits button strip from panel (+0x668) to surface. Two-column sprite sheet: +0x605 flag offsets dest rect to "pressed" column. Driven by 20-frame paint throttle in Town_HitTest. |
| 0042e5e0 | FUN_0042e5e0 | Town_PostcardSendHandler | town postcard | Two-mode render: send=0 (init) blits send button from +0xD4-0xE0 src rect; send!=0 (pressed) blits postcard image then send animation sprite. Guarded by +0x5F9/+0x5F8. |
| 0042e760 | FUN_0042e760 | Town_ClearPostcardUI | town postcard | Restores postcard UI background. Two paths: if player exists restores surfaces (+0x648/+0x650) via UIPANEL_Blit; if no player calls SendHandler(1) + updates sprites. |
| 0042e900 | Town_PostcardRecvHandler | Town_Ctor | town lifecycle | RENAMED 2026-07: Was Town_PostcardRecvHandler. Actually Town CONSTRUCTOR (87 bytes). Calls UI_WindowBase_Ctor, sets vtable→0x477D88, calls Town_BaseCtor. |
| 0042e960 | FUN_0042e960 | Town_Dtor | town lifecycle | Scalar deleting dtor (30 bytes). Calls Town_Destroy then conditionally GLOBAL_free. vtable[0]. |

[Showing lines 1-502 of 1856 (50.0KB limit). Use offset=503 to continue.]