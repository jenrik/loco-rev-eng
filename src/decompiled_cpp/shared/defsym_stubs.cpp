/**
 * defsym_stubs.cpp — Stub implementations replacing --defsym=0 entries
 */

// Status: TRANSCRIBED

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cassert>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

extern "C" {
void AudioChannel_Pause() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void AudioChannel_Play() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void ButtonSprite_Ctor() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_SetMode() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Config_GetIniString() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_RestoreSurfaces() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_SetSurfaceFormat() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_UnlockPrimary() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DPlayManager_RenderConnectionPanel() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void FormatResourceString() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
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
void NETMAN_SendPacket() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void PlaySound() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void PlaySoundAt() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_IsBuildingTile() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_IsRoadTile() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESMGR_LoadSoundResource() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESMGR_ReleaseSoundResource() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Sprite_Init() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Sprite_SetState() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TileMap_InvalidateRect() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_CreateChildWindow() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIEntity_Ctor() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_MainMenu_SetState() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_BeginPaint() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_Blit() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_CreateSurface() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_EndPaintEx() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_WindowBase_BaseDtor() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_WindowBase_Ctor() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void VehicleEditor_CheckEditBounds1() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_StreamOpen() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* WIN32_StreamOpenFile(void*, const char*, uint32_t, uint32_t, uint32_t) { return nullptr; }
void WNDPROC_EnterCriticalSection() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WNDPROC_LeaveCriticalSection() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* WNDPROC_StreamFromMemory(void*, const char*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); return nullptr; }
void RESDATA_DtorBase() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void ScriptEngine_Init() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_InitScrollPanel() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_ScrollPanel_Dtor() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void ScriptEngine_Call() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_SetPosition() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void HelpWnd_PlayNarration() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_SetBuildMode() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameAudio_UpdateVolume() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_ScrollPanel_HandleDrag() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Panel_DtorBody() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_localtime() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_wcsstr() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_GetBoundingRect() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TileMap_GetObjectAt() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_MainMenu_SetState_void() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Vehicle_GetOccupantCount() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
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
void Cursor_BlitEditPreview() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Cursor_UpdateScrollButtons() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Cursor_DrawColorPalette() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Cursor_HandleTabChange() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GetOpenFileNameA() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NET_GetOrCreateSurface() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NET_UploadAsset() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void PlaySoundFile() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TileMap_CreateOverlay() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void FMT_LAYOUT_PATH() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GAMESTATE_StartGameTimer() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* _g_netman = nullptr;
} /* extern "C" */
void CGWND_PumpMessages(char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }  /* loading-transition pump — C++ linkage */
extern "C" {
void* _g_netman_data = nullptr;
void* STR_LEGO_LOCO = nullptr;
void CGWND_QuitToMenu() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* MessageBeep = nullptr;
void IsWindowVisible() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* World_FinalizeLoad = nullptr;
void* World_GetObjectAt = nullptr;
void* GAMESTATE_SelectLayout = nullptr;
void* CGWND_GameSetup_DrawGrid_Thunk = nullptr;
} /* end extern "C" */

/* C++-linkage stubs */
void* g_scripted_object = nullptr;
void RESDATA_GameObject_UpdateAnimation(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_SoundObject_GetState(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_SoundObject_GetTextLength(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_SendAck(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_CreateTooltip(void*, int, short, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_LockSurface(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_IsBitmapReady(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_CreateChildSprite(void*, void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_DefWndProc(void*, unsigned int, unsigned int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Cursor_HandleWindowPaint(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
/** CRT_itoa — Convert integer to string in given radix (base 2-36).
 *  Standard C runtime function. Returns buf. */
char* CRT_itoa(int value, char* buf, int radix) {
    if (radix < 2 || radix > 36) { buf[0] = '\0'; return buf; }
    char tmp[36];
    int i = 0;
    unsigned u = (value < 0 && radix == 10) ? -value : (unsigned)value;
    int neg = (value < 0 && radix == 10);
    do {
        int d = u % radix;
        tmp[i++] = (d < 10) ? '0' + d : 'a' + d - 10;
        u /= radix;
    } while (u);
    int j = 0;
    if (neg) buf[j++] = '-';
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
    return buf;
}
void Cursor_WaitForBlit(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void AudioChannel_IsActive(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GAMESTATE_ConnectToNetworkGame(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameWindow_BaseDtor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_SetFullscreenMode(char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameWindow_SetPosition(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameWindow_Show(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_SetWindowVisible(void*, char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameWindow_Hide(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void ResourceManager_GetStringById(void*, unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TileMap_Init(void**, unsigned char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* TrainSubsystem_Ctor = nullptr;
void* WIN32_CreateThread = nullptr;
void Train_ProcessMessages(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_QueueAsyncTask(void*, void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void EditWindow_render(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Town_BlitElement(void*, int, int, int, int, void*, int, int, int, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_exit(char const**, char const**) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* _g_netman_state = nullptr;
void WIN32_ResumeThread(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_WindowBase_OnCreate(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_WindowProc(void*, unsigned int, unsigned int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void PlayerConfig_SetName(void*, char const*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void PlayerConfig_Save(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Config_ReadInt(void*, char const*, char const*, char const*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_SendPacket(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* CreateHatchBrush = nullptr;
void* g_editwindow_ptr = nullptr;
void EditWindow_cleanupSprites(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_FreeWindow(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void ResourceManager_GetStringById(void**, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NameEntryPanel_Ctor(void*, void*, unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NameEntryPanel_CreateWindow(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_GameSetup_Ctor(void*, void*, unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_GameSetup_Create(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_SetWindowVisible(void*, unsigned char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_CreateSession(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_WindowBase_Show(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* BringWindowToTop = nullptr;
void TrackPiece_Dtor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void __ftol(double) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_ChildWindow_Dtor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_StreamDestroy(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WNDPROC_StreamCleanup(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_ChildWindow_Render(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_StreamDestroyImmediate(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TileMap_SetViewport(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TileMap_GetTileAt(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_GetSurface() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_LoadFile(int*, char const*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_ScriptedObject_AddChild(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_CursorEditWindow_Ctor(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TrainStation_Ctor(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Town_CopyTiles8bpp_Transparent(void*, int, int, int, int, int, int, int, int, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* DPLAY_SetPlayerName = nullptr;
void UIPANEL_CopySurface(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NET_ComputeColor(unsigned char, unsigned char, unsigned char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DPLAY_SetPlayerData(void*, char const*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NET_GetHostName(int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TileMap_UpdateViewport(void*, void*, short) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TileMap_GetTileRect(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void SetRect(void*, int, int, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* GAMESTATE_LoadExistingGame = nullptr;
void* World_SerializeObject = nullptr;
void* STR_REMOVED = nullptr;
void* DPLAY_InitPlayerSlot = nullptr;
void NETMAN_ReceiveFileTransfer(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_SendAck(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* GAMESTATE_SetDifficulty = nullptr;
void* DPLAY_EnumeratePlayers = nullptr;
void* NET_SendFile = nullptr;
void Vehicle_SetState(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_ReceiveLayoutSelect(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void PlayerConfig_SaveToFile(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void ResourceManager_Shutdown(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_FileData_Dtor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GAMESTATE_HandleNetworkGame() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void VehicleEditor_GetResourceId(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void VehicleEditor_RemoveVehicle(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void ArrivalQueue_RemoveVehicle(void*, unsigned int, char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameVehicle_RemoveDestination(void*, unsigned int, char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GAMESTATE_EditorState_Detach(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_memset_pattern(void*, int, int, void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_free_pattern(void*, int, int, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_DtorBody(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_PresentRect(void*, void*, int*, unsigned char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_RecvNetworkData(void*, unsigned int, char const*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_GetSystemMetrics(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Ordinal_4(void*, void**, void*, void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_memcpy(void*, void const*, unsigned long) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_malloc(unsigned long) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_sprintf_buf(char*, char const*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_toupper(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_timeGetTime(int*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_FindClose(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_FindFirstFile(char const*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_FindNextFile(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_UpdateChild(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Game_IsPositionBetween(int, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void PlayerConfig_GetName(void*, char*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_memset(void*, int, unsigned long) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void World_CheckActive(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TrackPiece_SetZoom(void*, short) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Town_BlitViewport(void*,int,int,int,int,int,int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Town_BlitElement(void*, int, int, int, int, void*, int, int, int, int, unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* g_game_instance = nullptr;
int32_t g_OutputDebugStringA = 0;
void ArrivalQueue_AddVehicle(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void VehicleEditor_Update(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void VehicleEditor_IsInBounds(void*, short, short, short) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void VehicleEditor_BlitBackground(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }


void Vehicle_InitRoute(void*, int, unsigned int, char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Vehicle_Ctor(void*, int, int, char, char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Vehicle_UpdatePosition(void*, char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Building_RemoveOccupant(int*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void TrackPiece_Ctor(void*, int, int, unsigned short) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_SetWorldPos(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_InvalidateRect(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_GetRelPos(void*, int*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
/** Config_WriteInt — Write integer to INI file section:key
 *  Address: 0x452DB0. __thiscall (ECX=config, stack=section,key,value).
 *  Converts int to string via CRT_itoa(10), writes via WritePrivateProfileStringA.
 *  INI path is at config+4. */
/** WritePrivateProfileStringA — Win32 INI file write.
 *  Host stub: logs and returns success (non-zero).
 *  TODO: implement actual INI file persistence. */
extern "C" int WritePrivateProfileStringA(const char* section, const char* key,
                                           const char* value, const char* filename) {
    fprintf(stderr, "HOST: WritePrivateProfileStringA(%s, %s, %s, %s) — stub\n",
            section ? section : "(null)", key ? key : "(null)",
            value ? value : "(null)", filename ? filename : "(null)");
    return 1; /* non-zero = success */
}
void __thiscall Config_WriteInt(void* config, const char* section, const char* key, int value) {
    char buf[100];
    CRT_itoa(value, buf, 10);
    WritePrivateProfileStringA(section, key, buf, (const char*)((char*)config + 4));
}
void LOCOBITMAP_ColorKeyBlit_thunk(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NET_UpdatePlayerList() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Town_CheckOccupied(void*, int, int, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Town_SelectBuilding(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_BlitSurface(void*, int, int, void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_SelectBuilding(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_PostQuit() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_GameVehicle_Ctor(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_GameVehicle_BaseDtor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Vehicle_InitOccupant(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Vehicle_IsMoving(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Vehicle_Stop(void*, int, unsigned char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Entity_StopSound(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_InitBase(void*, int, int, unsigned char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_UnlockSurface(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* DAT_00485270 = nullptr;
void NETMAN_SetGameMode(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GAMESTATE_FindAdjacentTrack(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_IsValidTrackIndex(void*, short) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Vehicle_GetNearestTrack(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GAMESTATE_FindTrackPosition(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DPLAY_CreatePlayer(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GAMESTATE_EditorState_Ctor(void*, char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DPLAY_CleanupPlayer(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GAMESTATE_InitTrackAtPosition(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameObject_HitTest(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Vehicle_DetachAll(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_InitAllSubsystems(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void timeBeginPeriod(unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
/* CGWND_PumpMessages(void*,unsigned char) — now in CGWND_sdl3.cpp */
void CGWND_AudioChannel_UpdatePosition(void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_AudioChannel_Stop(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_ResetNetworkState(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_StopSession(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_StartClientSession(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Train_QueueMessage(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_Init(void*, unsigned char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameSetupPanel_loadLayouts(void*, unsigned char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameSetupPanel_updateTitle(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameSetupPanel_drawGrid(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Game_Shutdown(int*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESMGR_Shutdown(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CRT_0x470650() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* UI_MainMenu_Ctor(void* mem, void*, unsigned int) { return mem; }
void UI_MainMenu_Create(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* Town_Ctor(void* mem, void*, unsigned int) { return mem; }
void Town_InitSprites(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* PostcardPreviewWindow_Ctor(void* mem, void*, unsigned int) { return mem; }
void LOCOBITMAP_InitWindow(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* TrainStationWindow_Ctor(void* mem, void*, unsigned int) { return mem; }
void TrainStationWindow_Create(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* LOCOBITMAP_CreateFromResource(void* mem, void*, unsigned int) { return mem; }
void* Cursor_Ctor(void* mem, void*, unsigned int) { return mem; }
void Cursor_Create(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* AudioMgr_Ctor(void* mem, void*, unsigned int) { return mem; }
void HelpWnd_Create(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void* CGWND_AboutDialog_Ctor(void* mem, void*, unsigned int) { return mem; }
void CGWND_AboutDialog_Create(void*, void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_RegisterWindowClass(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_constructor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DirectPlay_constructor(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
/** PixelDataCache_Ctor — PixelDataCache constructor (address: 0x401620)
 *  Sets vtable=0x4773E8, album_index=-1, buffer=NULL, calls Load(1). */
void* PixelDataCache_Ctor(void* self) {
    extern void PixelDataCache_Load(void*, int);
    extern const void* PTR_PixelDataCache_Dtor_004773e8;
    *(const void**)self = &PTR_PixelDataCache_Dtor_004773e8;
    ((int*)self)[1] = -1;  /* current_album_index */
    ((int*)self)[2] = 0;   /* buffer NULL */
    ((int*)self)[4] = -1;
    ((int*)self)[5] = -1;
    PixelDataCache_Load(self, 1);
    return self;
}
/** PixelDataCache_Load — Host stub: no-op (cache initialized empty).
 *  TODO: decompile 0x401650 for real implementation. */
void PixelDataCache_Load(void* self, int mode) {
    (void)self; (void)mode;
}
void TileMap_Init(void*, unsigned char) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameAudio_StopFinished(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_GetDsoundErrorString(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Ordinal_2(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESMGR_GetById(void*, unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GetResourceType(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_AudioChannel_Play(unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_AudioChannel_Pause(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Game_SetScreenMode(void*, int, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void BuildingMgr_DestroyAll(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_ResetTooltips(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void World_Reset(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Cursor_Show(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_SendMapData(void*, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_AudioChannel_IsActive(unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_AudioChannel_Release(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_CloseHandle(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_timeKillEvent(unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GameAudio_StopFinished() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_GetDsoundErrorString() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Ordinal_2() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESMGR_GetById() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void GetResourceType() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_AudioChannel_Play() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_AudioChannel_Pause() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Game_SetScreenMode() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void BuildingMgr_DestroyAll() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_ResetTooltips() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void World_Reset() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Cursor_Show() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void NETMAN_SendMapData() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_AudioChannel_IsActive() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void CGWND_AudioChannel_Release() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_CloseHandle() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_timeKillEvent() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
int WIN32_GetThreadResult(void*) { return 0; }
void World_Shutdown(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Train_FlushMessages(void*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_Sleep(unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Sprite_UnlockAll(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UIPANEL_FreeAllSurfaces() { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void WIN32_timeEndPeriod(unsigned int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Sprite_Shutdown(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void Town_GameView_Cleanup(int*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void DDRAW_InvalidateAll(int*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void RESDATA_ScriptedObject_Shutdown(int*) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }
void UI_FreeMessageBox(int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); }

