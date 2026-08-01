/**
 * defsym_stubs.cpp — Stub implementations replacing --defsym=0 entries
 */

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

extern "C" {
void AudioChannel_Pause() {}
void AudioChannel_Play() {}
void ButtonSprite_Ctor() {}
void CGWND_SetMode() {}
void Config_GetIniString() {}
void DDRAW_RestoreSurfaces() {}
void DDRAW_SetSurfaceFormat() {}
void DDRAW_UnlockPrimary() {}
void DPlayManager_RenderConnectionPanel() {}
void FormatResourceString() {}
// PlayerRecord_constructor (0x452E10) calls GetUserNameA only after its
// Configuration/PlayerName lookup is empty. Preserve the Win32 size contract
// for the POSIX host instead of silently forcing its "LEGO LOCO" fallback.
uint32_t GetUserNameA(char* buffer, uint32_t* size)
{
#ifndef _WIN32
    if (buffer == nullptr || size == nullptr || *size == 0) return 0;
    const passwd* const account = getpwuid(getuid());
    const char* const name = account != nullptr ? account->pw_name : nullptr;
    if (name == nullptr || *name == '\0') return 0;
    const size_t required = std::strlen(name) + 1;
    if (required > *size) {
        *size = static_cast<uint32_t>(required);
        return 0;
    }
    std::memcpy(buffer, name, required);
    *size = static_cast<uint32_t>(required);
    return 1;
#else
    static_cast<void>(buffer);
    static_cast<void>(size);
    return 0;
#endif
}
void NETMAN_SendPacket() {}
void PlaySound() {}
void PlaySoundAt() {}
void RESDATA_IsBuildingTile() {}
void RESDATA_IsRoadTile() {}
void RESMGR_LoadSoundResource() {}
void RESMGR_ReleaseSoundResource() {}
void Sprite_Init() {}
void Sprite_SetState() {}
void TileMap_InvalidateRect() {}
void UI_CreateChildWindow() {}
void UIEntity_Ctor() {}
void UI_MainMenu_SetState() {}
void UIPANEL_BeginPaint() {}
void UIPANEL_Blit() {}
void UIPANEL_CreateSurface() {}
void UIPANEL_EndPaintEx() {}
void UI_WindowBase_BaseDtor() {}
void UI_WindowBase_Ctor() {}
void VehicleEditor_CheckEditBounds1() {}
void WIN32_StreamOpen() {}
void WIN32_StreamOpenFile() {}
void WNDPROC_EnterCriticalSection() {}
void WNDPROC_LeaveCriticalSection() {}
void WNDPROC_StreamFromMemory() {}
void RESDATA_DtorBase() {}
void ScriptEngine_Init() {}
void UIPANEL_InitScrollPanel() {}
void UIPANEL_ScrollPanel_Dtor() {}
void ScriptEngine_Call() {}
void RESDATA_SetPosition() {}
void HelpWnd_PlayNarration() {}
void INPUT_NewWorld() {}
void CGWND_SetBuildMode() {}
void INPUT_SaveCurrentWorld() {}
void GameAudio_UpdateVolume() {}
void UIPANEL_ScrollPanel_HandleDrag() {}
void Panel_DtorBody() {}
void CRT_localtime() {}
void CRT_wcsstr() {}
void GameObject_GetBoundingRect() {}
void INPUT_EditWndProc() {}
void INPUT_ExitGame() {}
void TileMap_GetObjectAt() {}
void UI_MainMenu_SetState_void() {}
void Vehicle_GetOccupantCount() {}
void* DAT_00479190 = nullptr;
void* DAT_004A9908 = nullptr;
void* DAT_004fd19c = nullptr;
void* DAT_004fd1ac = nullptr;
void* DAT_004fd1c0 = nullptr;
void* g_clipper_0 = nullptr;
void* g_clipper_1 = nullptr;
void* g_clipper_2 = nullptr;
void* g_clipper_3 = nullptr;
void* g_clipper_4 = nullptr;
void* g_clipper_5 = nullptr;
void* g_clipper_surf = nullptr;
void* _g_cursor_back = nullptr;
int32_t _g_cursor_refcount = 0;
void* g_net_host_info = nullptr;
void* g_player_color = nullptr;
void* _g_primary_surface = nullptr;
void* g_remote_res_path = nullptr;
void* s_Configuration_0047e734 = nullptr;
void* s_PlayerName_0047e73c = nullptr;
void* s_Sound_0047e2c0 = nullptr;
void* s_VolumeHigh_0047f14c = nullptr;
void* s_VolumeLow_0047f164 = nullptr;
void* s_VolumeMed_0047f158 = nullptr;
void* __imp_SystemParametersInfoA = nullptr;
void* g_scene_name = nullptr;
void INPUT_DirToOffset_Up() {}
void INPUT_DirToOffset_Left() {}
void INPUT_DirToOffset_Down() {}
void INPUT_DirToOffset_Right() {}
void Cursor_BlitEditPreview() {}
void Cursor_UpdateScrollButtons() {}
void INPUT_SwitchToLocomotiveTab() {}
void Cursor_DrawColorPalette() {}
void Cursor_HandleTabChange() {}
void GetOpenFileNameA() {}
void NET_GetOrCreateSurface() {}
void NET_UploadAsset() {}
void PlaySoundFile() {}
void TileMap_CreateOverlay() {}
void FMT_LAYOUT_PATH() {}
void RESMGR_ResourceData_Init() {}
void RESMGR_LoadResource() {}
void GAMESTATE_StartGameTimer() {}
void* _g_netman = nullptr;
} /* extern "C" */
void CGWND_PumpMessages(char) {}  /* loading-transition pump — C++ linkage */
extern "C" {
void* _g_netman_data = nullptr;
void* STR_LEGO_LOCO = nullptr;
void CGWND_QuitToMenu() {}
void* MessageBeep = nullptr;
void IsWindowVisible() {}
void* RESMGR_ReleaseResource = nullptr;
void* World_FinalizeLoad = nullptr;
void* World_GetObjectAt = nullptr;
void* GAMESTATE_SelectLayout = nullptr;
void* CGWND_GameSetup_DrawGrid_Thunk = nullptr;
} /* end extern "C" */

/* C++-linkage stubs */
void* g_scripted_object = nullptr;
void RESDATA_GameObject_UpdateAnimation(void*) {}
void RESDATA_SoundObject_GetState(int) {}
void RESDATA_SoundObject_GetTextLength(int) {}
void NETMAN_SendAck(void*) {}
void UI_CreateTooltip(void*, int, short, int, int) {}
void UIPANEL_LockSurface(void*) {}
void UI_IsBitmapReady(void*) {}
void RESDATA_CreateChildSprite(void*, void*, int, int) {}
void UI_DefWndProc(void*, unsigned int, unsigned int, int) {}
void Cursor_HandleWindowPaint(void*, int) {}
void CRT_itoa(int, char*, int) {}
void Cursor_WaitForBlit(void*) {}
void AudioChannel_IsActive(int) {}
void GAMESTATE_ConnectToNetworkGame(void*) {}
void GameWindow_BaseDtor(void*) {}
void CGWND_SetFullscreenMode(char) {}
void GameWindow_SetPosition(void*, int, int) {}
void GameWindow_Show(void*) {}
void UI_SetWindowVisible(void*, char) {}
void GameWindow_Hide(void*) {}
void ResourceManager_GetStringById(void*, unsigned int) {}
void TileMap_Init(void**, unsigned char) {}
void* TrainSubsystem_Ctor = nullptr;
void* WIN32_CreateThread = nullptr;
void Train_ProcessMessages(void*) {}
void WIN32_QueueAsyncTask(void*, void*, void*) {}
void EditWindow_render(void*) {}
void Town_BlitElement(void*, int, int, int, int, void*, int, int, int, int, int) {}
void CRT_exit(char const**, char const**) {}
void* _g_netman_state = nullptr;
void WIN32_ResumeThread(void*, int) {}
void UI_WindowBase_OnCreate(void*) {}
void UIPANEL_WindowProc(void*, unsigned int, unsigned int, int) {}
void PlayerConfig_SetName(void*, char const*) {}
void PlayerConfig_Save(void*) {}
void Config_ReadInt(void*, char const*, char const*, char const*) {}
void NETMAN_SendPacket(void*) {}
void* CreateHatchBrush = nullptr;
void* g_editwindow_ptr = nullptr;
void EditWindow_cleanupSprites(void*) {}
void RESDATA_FreeWindow(void*) {}
void ResourceManager_GetStringById(void**, int) {}
void NameEntryPanel_Ctor(void*, void*, unsigned int) {}
void NameEntryPanel_CreateWindow(void*, void*) {}
void CGWND_GameSetup_Ctor(void*, void*, unsigned int) {}
void CGWND_GameSetup_Create(void*, void*) {}
void UI_SetWindowVisible(void*, unsigned char) {}
void NETMAN_CreateSession(void*) {}
void UI_WindowBase_Show(void*) {}
void* BringWindowToTop = nullptr;
void TrackPiece_Dtor(void*) {}
void __ftol(double) {}
void UI_ChildWindow_Dtor(void*) {}
void WIN32_StreamDestroy(void*) {}
void WNDPROC_StreamCleanup(void*) {}
void UI_ChildWindow_Render(void*, void*) {}
void WIN32_StreamDestroyImmediate(void*) {}
void TileMap_SetViewport(void*, void*) {}
void TileMap_GetTileAt(void*, void*) {}
void DDRAW_GetSurface() {}
void DDRAW_LoadFile(int*, char const*) {}
void INPUT_SetKeyboard(void*) {}
void INPUT_SetMouse(void*) {}
void INPUT_ExitGame(void*, int, int) {}
void RESDATA_ScriptedObject_AddChild(void*, int, int) {}
void CGWND_CursorEditWindow_Ctor(void*, int, int) {}
void TrainStation_Ctor(void*, int, int) {}
void Town_CopyTiles8bpp_Transparent(void*, int, int, int, int, int, int, int, int, int, int) {}
void* DPLAY_SetPlayerName = nullptr;
void UIPANEL_CopySurface(void*, int) {}
void NET_ComputeColor(unsigned char, unsigned char, unsigned char) {}
void DPLAY_SetPlayerData(void*, char const*) {}
void NET_GetHostName(int, int) {}
void TileMap_UpdateViewport(void*, void*, short) {}
void TileMap_GetTileRect(void*, void*) {}
void SetRect(void*, int, int, int, int) {}
void* GAMESTATE_LoadExistingGame = nullptr;
void* World_SerializeObject = nullptr;
void* STR_REMOVED = nullptr;
void* DPLAY_InitPlayerSlot = nullptr;
void NETMAN_ReceiveFileTransfer(int) {}
void NETMAN_SendAck(int) {}
void* GAMESTATE_SetDifficulty = nullptr;
void* DPLAY_EnumeratePlayers = nullptr;
void* NET_SendFile = nullptr;
void Vehicle_SetState(void*, int) {}
void NETMAN_ReceiveLayoutSelect(int) {}
void PlayerConfig_SaveToFile(void*) {}
void ResourceManager_Shutdown(int) {}
void DDRAW_FileData_Dtor(void*) {}
void GAMESTATE_HandleNetworkGame() {}
void VehicleEditor_GetResourceId(int) {}
void VehicleEditor_RemoveVehicle(void*, int) {}
void ArrivalQueue_RemoveVehicle(void*, unsigned int, char) {}
void GameVehicle_RemoveDestination(void*, unsigned int, char) {}
void GAMESTATE_EditorState_Detach(int) {}
void CRT_memset_pattern(void*, int, int, void*) {}
void CRT_free_pattern(void*, int, int, void*) {}
void RESDATA_DtorBody(void*) {}
void DDRAW_PresentRect(void*, void*, int*, unsigned char) {}
void WIN32_RecvNetworkData(void*, unsigned int, char const*) {}
void WIN32_GetSystemMetrics(void*) {}
void Ordinal_4(void*, void**, void*, void*, void*) {}
void CRT_memcpy(void*, void const*, unsigned long) {}
void CRT_malloc(unsigned long) {}
void CRT_sprintf_buf(char*, char const*) {}
void CRT_toupper(int) {}
void CRT_timeGetTime(int*) {}
void CRT_FindClose(void*) {}
void CRT_FindFirstFile(char const*, void*) {}
void CRT_FindNextFile(void*, void*) {}
void RESDATA_UpdateChild(void*) {}
void Game_IsPositionBetween(int, int, int) {}
void PlayerConfig_GetName(void*, char*, int) {}
void CRT_memset(void*, int, unsigned long) {}
void World_CheckActive(void*) {}
void TrackPiece_SetZoom(void*, short) {}
void Town_BlitViewport(void*,int,int,int,int,int,int) {}
void Town_BlitElement(void*, int, int, int, int, void*, int, int, int, int, unsigned int) {}
void* g_game_instance = nullptr;
int32_t g_OutputDebugStringA = 0;
void ArrivalQueue_AddVehicle(void*, void*) {}
void VehicleEditor_Update(void*) {}
void VehicleEditor_IsInBounds(void*, short, short, short) {}
void VehicleEditor_BlitBackground(void*, int, int) {}


void Vehicle_InitRoute(void*, int, unsigned int, char) {}
void Vehicle_Ctor(void*, int, int, char, char) {}
void Vehicle_UpdatePosition(void*, char) {}
void Building_RemoveOccupant(int*) {}
void TrackPiece_Ctor(void*, int, int, unsigned short) {}
void GameObject_SetWorldPos(void*, int, int) {}
void GameObject_InvalidateRect(void*) {}
void GameObject_GetRelPos(void*, int*, int, int) {}
void Config_WriteInt(void*, char const*, char const*, int) {}
void LOCOBITMAP_ColorKeyBlit_thunk(void*) {}
void INPUT_InitNetworkPlayer(void*) {}
void NET_UpdatePlayerList() {}
void Town_CheckOccupied(void*, int, int, int, int) {}
void Town_SelectBuilding(void*, void*) {}
void UIPANEL_BlitSurface(void*, int, int, void*, int, int) {}
void DDRAW_SelectBuilding(void*, void*) {}
void WIN32_PostQuit() {}
void RESDATA_GameVehicle_Ctor(void*, int) {}
void RESDATA_GameVehicle_BaseDtor(void*) {}
void Vehicle_InitOccupant(void*, int) {}
void Vehicle_IsMoving(void*) {}
void Vehicle_Stop(void*, int, unsigned char) {}
void Entity_StopSound(void*, int) {}
void GameObject_InitBase(void*, int, int, unsigned char) {}
void UIPANEL_UnlockSurface(void*) {}
void* DAT_00485270 = nullptr;
void NETMAN_SetGameMode(void*, int) {}
void GAMESTATE_FindAdjacentTrack(void*) {}
void RESDATA_IsValidTrackIndex(void*, short) {}
void Vehicle_GetNearestTrack(int) {}
void GAMESTATE_FindTrackPosition(void*, int, int) {}
void DPLAY_CreatePlayer(void*) {}
void GAMESTATE_EditorState_Ctor(void*, char) {}
void DPLAY_CleanupPlayer(void*) {}
void GAMESTATE_InitTrackAtPosition(void*, int, int) {}
void GameObject_HitTest(void*, int, int) {}
void Vehicle_DetachAll(int) {}
void CGWND_InitAllSubsystems(void*) {}
void INPUT_LoadConfig(void*) {}
void timeBeginPeriod(unsigned int) {}
/* CGWND_PumpMessages(void*,unsigned char) — now in CGWND_sdl3.cpp */
void CGWND_AudioChannel_UpdatePosition(void*, int, int) {}
void CGWND_AudioChannel_Stop(void*) {}
void NETMAN_ResetNetworkState(void*) {}
void NETMAN_StopSession(void*) {}
void NETMAN_StartClientSession(void*) {}
void Train_QueueMessage(void*, void*) {}
void NETMAN_Init(void*, unsigned char) {}
void GameSetupPanel_loadLayouts(void*, unsigned char) {}
void GameSetupPanel_updateTitle(void*) {}
void GameSetupPanel_drawGrid(void*) {}
void Game_Shutdown(int*) {}
void RESMGR_Shutdown(int) {}
void CRT_0x470650() {}
void* UI_MainMenu_Ctor(void* mem, void*, unsigned int) { return mem; }
void UI_MainMenu_Create(void*, void*) {}
void* Town_Ctor(void* mem, void*, unsigned int) { return mem; }
void Town_InitSprites(void*, void*) {}
void* PostcardPreviewWindow_Ctor(void* mem, void*, unsigned int) { return mem; }
void LOCOBITMAP_InitWindow(void*, void*) {}
void* TrainStationWindow_Ctor(void* mem, void*, unsigned int) { return mem; }
void TrainStationWindow_Create(void*, void*) {}
void* LOCOBITMAP_CreateFromResource(void* mem, void*, unsigned int) { return mem; }
void* Cursor_Ctor(void* mem, void*, unsigned int) { return mem; }
void Cursor_Create(void*, void*) {}
void* AudioMgr_Ctor(void* mem, void*, unsigned int) { return mem; }
void HelpWnd_Create(void*, void*) {}
void* CGWND_AboutDialog_Ctor(void* mem, void*, unsigned int) { return mem; }
void CGWND_AboutDialog_Create(void*, void*) {}
void CGWND_RegisterWindowClass(void*) {}
void NETMAN_constructor(void*) {}
void DirectPlay_constructor(void*) {}
void PixelDataCache_Ctor(void*) {}
void TileMap_Init(void*, unsigned char) {}
void GameAudio_StopFinished(void*) {}
void DDRAW_GetDsoundErrorString(int) {}
void Ordinal_2(void*) {}
void RESMGR_GetById(void*, unsigned int) {}
void GetResourceType(int) {}
void CGWND_AudioChannel_Play(unsigned int) {}
void CGWND_AudioChannel_Pause(int) {}
void Game_SetScreenMode(void*, int, int, int) {}
void BuildingMgr_DestroyAll(void*, int) {}
void UI_ResetTooltips(void*, int) {}
void World_Reset(void*, int) {}
void Cursor_Show(void*) {}
void NETMAN_SendMapData(void*, int) {}
void CGWND_AudioChannel_IsActive(unsigned int) {}
void CGWND_AudioChannel_Release(void*) {}
void WIN32_CloseHandle(void*) {}
void WIN32_timeKillEvent(unsigned int) {}
void INPUT_Shutdown(int) {}
void INPUT_Cleanup(int*) {}
void GameAudio_StopFinished() {}
void DDRAW_GetDsoundErrorString() {}
void Ordinal_2() {}
void RESMGR_GetById() {}
void GetResourceType() {}
void CGWND_AudioChannel_Play() {}
void CGWND_AudioChannel_Pause() {}
void Game_SetScreenMode() {}
void BuildingMgr_DestroyAll() {}
void UI_ResetTooltips() {}
void World_Reset() {}
void Cursor_Show() {}
void NETMAN_SendMapData() {}
void CGWND_AudioChannel_IsActive() {}
void CGWND_AudioChannel_Release() {}
void WIN32_CloseHandle() {}
void WIN32_timeKillEvent() {}
void INPUT_Shutdown() {}
void INPUT_Cleanup() {}
int WIN32_GetThreadResult(void*) { return 0; }
void World_Shutdown(int) {}
void Train_FlushMessages(void*) {}
void WIN32_Sleep(unsigned int) {}
void Sprite_UnlockAll(int) {}
void UIPANEL_FreeAllSurfaces() {}
void WIN32_timeEndPeriod(unsigned int) {}
void Sprite_Shutdown(int) {}
void Town_GameView_Cleanup(int*) {}
void DDRAW_InvalidateAll(int*) {}
void RESDATA_ScriptedObject_Shutdown(int*) {}
void UI_FreeMessageBox(int) {}

