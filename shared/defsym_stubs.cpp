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
void AudioChannel_Pause() { /* host no-op */ }
void AudioChannel_Play() { /* host no-op */ }
void ButtonSprite_Ctor() { /* host no-op */ }
void CGWND_SetMode() { /* host no-op */ }
/* Config_GetIniString(): this 0-arg extern "C" landmine placeholder
 * collided with game/ConfigIni.cpp's real 6-arg extern "C" body
 * (LINK-001 — extern "C" doesn't mangle by arg count/type, so both are
 * literally the same symbol). Removed; no evidence any real caller
 * wants a 0-arg shape. ButtonSprite_Ctor()/CGWND_SetMode() above are
 * the same 0-arg-landmine pattern but don't currently collide with
 * anything (untouched — out of LINK-001's scope; tracked separately in
 * docs/landmine-sweep-worklist.md). */
void DDRAW_RestoreSurfaces() { /* host no-op */ }
void DDRAW_SetSurfaceFormat() { /* host no-op */ }
void DDRAW_UnlockPrimary() { /* host no-op */ }
void DPlayManager_RenderConnectionPanel() { /* host no-op */ }
void FormatResourceString() { /* host no-op */ }
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
void NETMAN_SendPacket() { /* host no-op */ }
void PlaySound() { /* host no-op */ }
void PlaySoundAt() { /* host no-op */ }
void RESDATA_IsBuildingTile() { /* host no-op */ }
void RESDATA_IsRoadTile() { /* host no-op */ }
void RESMGR_LoadSoundResource() { /* host no-op */ }
void RESMGR_ReleaseSoundResource() { /* host no-op */ }
void Sprite_Init() { /* host no-op */ }
void Sprite_SetState() { /* host no-op */ }
void TileMap_InvalidateRect() { /* host no-op */ }
void UIEntity_Ctor() { /* host no-op */ }
void UI_MainMenu_SetState() { /* host no-op */ }
void UIPANEL_BeginPaint() { /* host no-op */ }
/* UIPANEL_Blit() (zero-arg, extern "C") removed: this is the unrelated
 * wrong stub town/Town.cpp used to silently bind to before the
 * town-tilerender-merge session fixed its declaration (2026-08-06), and
 * every other caller cluster was fixed in the 2026-08-06 cross-validation
 * session — confirmed zero referrers via `nm --print-file-name
 * build/lego_loco.p/*.o | grep "U UIPANEL_Blit$"`. See
 * docs/landmine-sweep-worklist.md. */
void UIPANEL_CreateSurface() { /* host no-op */ }
void UIPANEL_EndPaintEx() { /* host no-op */ }
void UI_WindowBase_BaseDtor() { /* host no-op */ }
void UI_WindowBase_Ctor() { /* host no-op */ }
void VehicleEditor_CheckEditBounds1() { /* host no-op */ }
void WIN32_StreamOpen() { /* host no-op */ }
void* WIN32_StreamOpenFile(void*, const char*, uint32_t, uint32_t, uint32_t) { return nullptr; }
void WNDPROC_EnterCriticalSection() { /* host no-op — single-threaded */ }
void WNDPROC_LeaveCriticalSection() { /* host no-op — single-threaded */ }
/* Same family, used by resources/WndProcStreamBuf.cpp's constructor/
 * destructor (originally thin IAT forwarders to Win32 Initialize/
 * DeleteCriticalSection at 0x464D70/0x464D80). */
void WNDPROC_InitializeCriticalSection() { /* host no-op — single-threaded */ }
void WNDPROC_DeleteCriticalSection() { /* host no-op — single-threaded */ }
void* WNDPROC_StreamFromMemory(void*, const char*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); return nullptr; }
void RESDATA_DtorBase() { /* host no-op */ }
void ScriptEngine_Init() { /* host no-op */ }
void UIPANEL_InitScrollPanel() { /* host no-op */ }
void UIPANEL_ScrollPanel_Dtor() { /* host no-op */ }
void ScriptEngine_Call() { /* host no-op */ }
void RESDATA_SetPosition() { /* host no-op */ }
void HelpWnd_PlayNarration() { /* host no-op */ }
void CGWND_SetBuildMode() { /* host no-op */ }
void GameAudio_UpdateVolume() { /* host no-op */ }
void UIPANEL_ScrollPanel_HandleDrag() { /* host no-op */ }
void Panel_DtorBody() { /* host no-op */ }
/* CRT_localtime(): this 0-arg landmine collided with shared/link_stubs.cpp's
 * real (unsigned int*) -> void* body (LINK-001); removed — link_stubs.cpp's
 * survives. */
void CRT_wcsstr() { /* host no-op */ }
void GameObject_GetBoundingRect() { /* host no-op */ }
void TileMap_GetObjectAt() { /* host no-op */ }
void UI_MainMenu_SetState_void() { /* host no-op */ }
void Vehicle_GetOccupantCount() { /* host no-op */ }
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
void Cursor_BlitEditPreview() { /* host no-op */ }
void Cursor_UpdateScrollButtons() { /* host no-op */ }
void Cursor_DrawColorPalette() { /* host no-op */ }
void Cursor_HandleTabChange() { /* host no-op */ }
/* GetOpenFileNameA(): this 0-arg landmine collided with graphics/
 * sdl3_window.cpp's real (void* lpofn) -> BOOL body (LINK-001); removed —
 * sdl3_window.cpp's survives. */
void NET_GetOrCreateSurface() { /* host no-op */ }
/* NET_UploadAsset and PlaySoundFile moved to shared/stubs_impl.cpp as loud
 * stubs with their real (int, char*)/(char*, int, int, int) signatures —
 * these 0-arg shapes had no real caller (see
 * docs/landmine-sweep-worklist.md "Cursor family"). */
void TileMap_CreateOverlay() { /* host no-op */ }
void FMT_LAYOUT_PATH() { /* host no-op */ }
void GAMESTATE_StartGameTimer() { /* host no-op */ }
void* _g_netman = nullptr;
} /* extern "C" */
void CGWND_PumpMessages(char) { /* host no-op */ }  /* loading-transition pump — C++ linkage */
extern "C" {
void* _g_netman_data = nullptr;
void* STR_LEGO_LOCO = nullptr;
void CGWND_QuitToMenu() { /* host no-op */ }
void* MessageBeep = nullptr;
void IsWindowVisible() { /* host no-op */ }
void* World_FinalizeLoad = nullptr;
void* World_GetObjectAt = nullptr;
void* GAMESTATE_SelectLayout = nullptr;
void* CGWND_GameSetup_DrawGrid_Thunk = nullptr;
} /* end extern "C" */

/* C++-linkage stubs */
void* g_scripted_object = nullptr;
void RESDATA_GameObject_UpdateAnimation(void*) { /* host no-op */ }
void RESDATA_SoundObject_GetState(int) { /* host no-op */ }
void RESDATA_SoundObject_GetTextLength(int) { /* host no-op */ }
void NETMAN_SendAck(void*) { /* host no-op */ }
void UI_CreateTooltip(void*, int, short, int, int) { /* host no-op */ }
void UIPANEL_LockSurface(void*) { /* host no-op */ }
/* RESDATA_CreateChildSprite(void*, void*, int, int) — 0x4546D0.
 * Ghidra confirms one real function (`void* __thiscall
 * RESDATA_CreateChildSprite(void* this, int resource, ushort x, int
 * type_flag)`) with 41 real xrefs (Town_HandleTileClick,
 * RESDATA_ScriptedObject_Start, UIPANEL_ScrollPanel_InitSprites,
 * DDRAW_InitBuildingSprites, ...). This C++ overload's declared callers
 * (ui/UIPANEL.cpp: tab_sprites[]/content_bg_sprite/list_bg_sprite/
 * list_text_sprite/sound_btn_sprite/item_sprites[], all via
 * UIPANEL::InitSprites; town/Town.cpp: cursor sprites via
 * Town::handle_tile_click) all expect a `void*` back — this stub used to
 * return `void`, so every caller stored whatever garbage happened to be
 * in the return register into a sprite-pointer field.
 *
 * Verified unreachable today: `grep`-confirmed zero C++ callers of both
 * UIPANEL::InitSprites() and Town::handle_tile_click() anywhere in this
 * tree (matches PROGRESS.md's 2026-08-05 finding for the latter, arrived
 * at independently here). Kept loud rather than silently "fixed" to
 * return nullptr, per this file's own precedent one entry up
 * (WNDPROC_StreamFromMemory) and CLAUDE.md's stub policy: a future
 * caller must fail loudly, not read a plausible-looking null forever. */
void* RESDATA_CreateChildSprite(void*, void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached — RESDATA_CreateChildSprite(void*, void*, int, int) 0x4546D0, verified unreachable but must not silently return garbage if that changes"); return nullptr; }
void UI_DefWndProc(void*, unsigned int, unsigned int, int) { /* host no-op */ }
void Cursor_HandleWindowPaint(void*, int) { /* host no-op */ }
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
void Cursor_WaitForBlit(void*) { /* host no-op */ }
void AudioChannel_IsActive(int) { /* host no-op */ }
void GAMESTATE_ConnectToNetworkGame(void*) { /* host no-op */ }
void GameWindow_BaseDtor(void*) { /* host no-op */ }
void CGWND_SetFullscreenMode(char) { /* host no-op */ }
void GameWindow_SetPosition(void*, int, int) { /* host no-op */ }
void GameWindow_Show(void*) { /* host no-op */ }
void GameWindow_Hide(void*) { /* host no-op */ }
void ResourceManager_GetStringById(void*, unsigned int) { /* host no-op */ }
void TileMap_Init(void**, unsigned char) { /* host no-op */ }
void* TrainSubsystem_Ctor = nullptr;
/* WIN32_CreateThread / WIN32_QueueAsyncTask: real implementations now
 * live in network/WIN32Thread.cpp (WIN32_QueueAsyncTask's host path is
 * core/HostMode3Bootstrap.cpp's pending-async-task pump). */
void Train_ProcessMessages(void*) { /* host no-op */ }
/* WIN32_GetSystemMetrics(void*) / WIN32_RecvNetworkData(void*,uint32_t,
 * const char*) — C++-mangled overloads network/DirectPlay.cpp declares
 * (deliberately outside its extern "C" block) for its own join/host-
 * enumeration and DirectPlay-error-reporting paths. Neither has a body
 * anywhere: a prior pass removed the no-op stubs that used to satisfy
 * them, under the false belief DirectPlay.cpp itself now defines them
 * (it only forward-declares). With -Wl,--unresolved-symbols=ignore-all
 * (LINK-001) that silently turns a link gap into a null-pointer call the
 * first time either path is exercised (see WIN32_SendNetworkData's
 * matching note in shared/link_stubs.cpp for the confirmed crash shape).
 * Both are reachable from ordinary DirectPlay join/host attempts, so this
 * warns once rather than a bare assert(0).
 * TODO: decompile 0x460360 (GetSystemMetrics) / 0x460EA0
 * (RecvNetworkData) for real DirectPlay-backed bodies. */
uint32_t WIN32_GetSystemMetrics(void*) {
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: WIN32_GetSystemMetrics not implemented "
                         "(TODO: decompile 0x460360)\n");
        warned = true;
    }
    return 0;
}
uint32_t WIN32_RecvNetworkData(void*, uint32_t, const char*) {
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: WIN32_RecvNetworkData not implemented "
                         "(TODO: decompile 0x460EA0)\n");
        warned = true;
    }
    return 0;
}
/* WIN32_PostQuit: real implementation now in core/CGWND.cpp (verified
 * address 0x463670 — despite the name, it minimizes every constructed UI
 * subsystem window then the main window; it posts no quit message). The
 * previous no-op here was itself wrong on top of being a stub: its own
 * doc comment's "address unresolved" claim was false (0x463670 was
 * findable via a direct xref walk from all three real call sites) and its
 * "quit-message post dropped" framing mischaracterized what the original
 * function does. See core/CGWND.cpp's WIN32_PostQuit for the real body
 * and PROGRESS.md for this correction's date-stamped entry. */
void EditWindow_render(void*) { /* host no-op */ }
void Town_BlitElement(void*, int, int, int, int, void*, int, int, int, int, int) { /* host no-op */ }
void CRT_exit(char const**, char const**) { /* host no-op */ }
void* _g_netman_state = nullptr;
void WIN32_ResumeThread(void*, int) { /* host no-op */ }
void UI_WindowBase_OnCreate(void*) { /* host no-op */ }
void UIPANEL_WindowProc(void*, unsigned int, unsigned int, int) { /* host no-op */ }
void PlayerConfig_SetName(void*, char const*) { /* host no-op */ }
void PlayerConfig_Save(void*) { /* host no-op */ }
void NETMAN_SendPacket(void*) { /* host no-op */ }
void* CreateHatchBrush = nullptr;
void* g_editwindow_ptr = nullptr;
void EditWindow_cleanupSprites(void*) { /* host no-op */ }
/* RESDATA_FreeWindow removed: its only caller (ui/EditWindow.cpp) now calls
 * the real PopupWindow::DestroyMCIChild() (0x4544A0) instead — see
 * PROGRESS.md. This stub was orphaned dead code, not a live no-op. */
void ResourceManager_GetStringById(void**, int) { /* host no-op */ }
void NameEntryPanel_Ctor(void*, void*, unsigned int) { /* host no-op */ }
void NameEntryPanel_CreateWindow(void*, void*) { /* host no-op */ }
void CGWND_GameSetup_Ctor(void*, void*, unsigned int) { /* host no-op */ }
void CGWND_GameSetup_Create(void*, void*) { /* host no-op */ }
void NETMAN_CreateSession(void*) { /* host no-op */ }
void UI_WindowBase_Show(void*) { /* host no-op */ }
void* BringWindowToTop = nullptr;
void TrackPiece_Dtor(void*) { /* host no-op */ }
void __ftol(double) { /* host no-op */ }
void WIN32_StreamDestroy(void*) { /* host no-op */ }
void WNDPROC_StreamCleanup(void*) { /* host no-op */ }
void WIN32_StreamDestroyImmediate(void*) { /* host no-op */ }
void TileMap_SetViewport(void*, void*) { /* host no-op */ }
void TileMap_GetTileAt(void*, void*) { /* host no-op */ }
void DDRAW_GetSurface() { /* host no-op */ }
void DDRAW_LoadFile(int*, char const*) { /* host no-op */ }
void RESDATA_ScriptedObject_AddChild(void*, int, int) { /* host no-op */ }
void CGWND_CursorEditWindow_Ctor(void*, int, int) { /* host no-op */ }
void TrainStation_Ctor(void*, int, int) { /* host no-op */ }
void Town_CopyTiles8bpp_Transparent(void*, int, int, int, int, int, int, int, int, int, int) { /* host no-op */ }
void* DPLAY_SetPlayerName = nullptr;
void UIPANEL_CopySurface(void*, int) { /* host no-op */ }
void NET_ComputeColor(unsigned char, unsigned char, unsigned char) { /* host no-op */ }
void DPLAY_SetPlayerData(void*, char const*) { /* host no-op */ }
void TileMap_UpdateViewport(void*, void*, short) { /* host no-op */ }
void TileMap_GetTileRect(void*, void*) { /* host no-op */ }
void SetRect(void*, int, int, int, int) { /* host no-op */ }
void* GAMESTATE_LoadExistingGame = nullptr;
void* World_SerializeObject = nullptr;
void* STR_REMOVED = nullptr;
void* DPLAY_InitPlayerSlot = nullptr;
void NETMAN_ReceiveFileTransfer(int) { /* host no-op */ }
void NETMAN_SendAck(int) { /* host no-op */ }
void* GAMESTATE_SetDifficulty = nullptr;
void* DPLAY_EnumeratePlayers = nullptr;
void* NET_SendFile = nullptr;
/* Vehicle_SetState(void*,int): wrong return type (void; real callers in
 * core/GameLoop.cpp etc. declare `int`) — collided with shared/
 * stubs_impl.cpp's correctly-typed `int Vehicle_SetState(void*, int)`
 * (LINK-001). Removed; stubs_impl.cpp's survives. */
void NETMAN_ReceiveLayoutSelect(int) { /* host no-op */ }
void PlayerConfig_SaveToFile(void*) { /* host no-op */ }
void ResourceManager_Shutdown(int) { /* host no-op */ }
void DDRAW_FileData_Dtor(void*) { /* host no-op */ }
void GAMESTATE_HandleNetworkGame() { /* host no-op */ }
void VehicleEditor_GetResourceId(int) { /* host no-op */ }
void VehicleEditor_RemoveVehicle(void*, int) { /* host no-op */ }
void ArrivalQueue_RemoveVehicle(void*, unsigned int, char) { /* host no-op */ }
void GameVehicle_RemoveDestination(void*, unsigned int, char) { /* host no-op */ }
void GAMESTATE_EditorState_Detach(int) { /* host no-op */ }
void CRT_memset_pattern(void*, int, int, void*, void*) { /* host no-op */ }
void CRT_free_pattern(void*, int, int, void*) { /* host no-op */ }
void RESDATA_DtorBody(void*) { /* host no-op */ }
void DDRAW_PresentRect(void*, void*, int*, unsigned char) { /* host no-op */ }
/* WIN32_RecvNetworkData/WIN32_GetSystemMetrics: these C++-mangled overloads
 * existed only because network/DirectPlay.cpp declared the two functions
 * outside its extern "C" block. Both are now real implementations in
 * network/DirectPlay.cpp (0x460EA0, 0x460360) declared extern "C" there
 * (matching game/Train_network.cpp's existing extern "C" declarations for
 * the sibling WIN32_SendNetworkData/WIN32_PeekMessageLoop), so nothing
 * requests these mangled symbols anymore. */
void Ordinal_4(void*, void**, void*, void*, void*) { /* host no-op */ }
void CRT_memcpy(void*, void const*, unsigned long) { /* host no-op */ }
void CRT_malloc(unsigned long) { /* host no-op */ }
void CRT_sprintf_buf(char*, char const*) { /* host no-op */ }
void CRT_toupper(int) { /* host no-op */ }
void CRT_timeGetTime(int*) { /* host no-op */ }
void CRT_FindClose(void*) { /* host no-op */ }
void CRT_FindFirstFile(char const*, void*) { /* host no-op */ }
void CRT_FindNextFile(void*, void*) { /* host no-op */ }
void RESDATA_UpdateChild(void*) { /* host no-op */ }
void Game_IsPositionBetween(int, int, int) { /* host no-op */ }
void PlayerConfig_GetName(void*, char*, int) { /* host no-op */ }
void CRT_memset(void*, int, unsigned long) { /* host no-op */ }
void World_CheckActive(void*) { /* host no-op */ }
void TrackPiece_SetZoom(void*, short) { /* host no-op */ }
void Town_BlitViewport(void*,int,int,int,int,int,int) { /* host no-op */ }
void Town_BlitElement(void*, int, int, int, int, void*, int, int, int, int, unsigned int) { /* host no-op */ }
void* g_game_instance = nullptr;
int32_t g_OutputDebugStringA = 0;
void ArrivalQueue_AddVehicle(void*, void*) { /* host no-op */ }
void VehicleEditor_Update(void*) { /* host no-op */ }
void VehicleEditor_IsInBounds(void*, short, short, short) { /* host no-op */ }
void VehicleEditor_BlitBackground(void*, int, int) { /* host no-op */ }


void Vehicle_InitRoute(void*, int, unsigned int, char) { /* host no-op */ }
void Vehicle_Ctor(void*, int, int, char, char) { /* host no-op */ }
void Vehicle_UpdatePosition(void*, char) { /* host no-op */ }
void Building_RemoveOccupant(int*) { /* host no-op */ }
void TrackPiece_Ctor(void*, int, int, unsigned short) { /* host no-op */ }
void GameObject_SetWorldPos(void*, int, int) { /* host no-op */ }
void GameObject_InvalidateRect(void*) { /* host no-op */ }
void GameObject_GetRelPos(void*, int*, int, int) { /* host no-op */ }
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
void LOCOBITMAP_ColorKeyBlit_thunk(void*) { /* host no-op */ }
/* NET_UpdatePlayerList: real body now in network/NetworkPlayerList.cpp
 * (0x445170, returns short). This used to be a wrong-signature (void
 * return) no-op stub that PlayerConfig.cpp's C++-linkage declaration
 * silently resolved to instead of the (previously nonexistent) real
 * function; keeping both would now be a duplicate-definition/ODR
 * violation since C++ mangling does not encode return type. */
void Town_CheckOccupied(void*, int, int, int, int) { /* host no-op */ }
void Town_SelectBuilding(void*, void*) { /* host no-op */ }
void UIPANEL_BlitSurface(void*, int, int, void*, int, int) { /* host no-op */ }
void DDRAW_SelectBuilding(void*, void*) { /* host no-op */ }
/* WIN32_PostQuit: real implementation now in core/CGWND.cpp (0x463670). */
void RESDATA_GameVehicle_Ctor(void*, int) { /* host no-op */ }
void RESDATA_GameVehicle_BaseDtor(void*) { /* host no-op */ }
void Vehicle_InitOccupant(void*, int) { /* host no-op */ }
void Vehicle_IsMoving(void*) { /* host no-op */ }
void Vehicle_Stop(void*, int, unsigned char) { /* host no-op */ }
void Entity_StopSound(void*, int) { /* host no-op */ }
void GameObject_InitBase(void*, int, int, unsigned char) { /* host no-op */ }
void UIPANEL_UnlockSurface(void*) { /* host no-op */ }
void* DAT_00485270 = nullptr;
void NETMAN_SetGameMode(void*, int) { /* host no-op */ }
void GAMESTATE_FindAdjacentTrack(void*) { /* host no-op */ }
void RESDATA_IsValidTrackIndex(void*, short) { /* host no-op */ }
void Vehicle_GetNearestTrack(int) { /* host no-op */ }
void GAMESTATE_FindTrackPosition(void*, int, int) { /* host no-op */ }
void DPLAY_CreatePlayer(void*) { /* host no-op */ }
void GAMESTATE_EditorState_Ctor(void*, char) { /* host no-op */ }
void DPLAY_CleanupPlayer(void*) { /* host no-op */ }
void GAMESTATE_InitTrackAtPosition(void*, int, int) { /* host no-op */ }
void GameObject_HitTest(void*, int, int) { /* host no-op */ }
void Vehicle_DetachAll(int) { /* host no-op */ }
void CGWND_InitAllSubsystems(void*) { /* host no-op */ }
void timeBeginPeriod(unsigned int) { /* host no-op */ }
/* CGWND_PumpMessages(void*,unsigned char) — now in CGWND_sdl3.cpp */
void CGWND_AudioChannel_UpdatePosition(void*, int, int) { /* host no-op */ }
void CGWND_AudioChannel_Stop(void*) { /* host no-op */ }
void NETMAN_ResetNetworkState(void*) { /* host no-op */ }
void NETMAN_StopSession(void*) { /* host no-op */ }
void NETMAN_StartClientSession(void*) { /* host no-op */ }
void Train_QueueMessage(void*, void*) { /* host no-op */ }
void NETMAN_Init(void*, unsigned char) { /* host no-op */ }
void GameSetupPanel_loadLayouts(void*, unsigned char) { /* host no-op */ }
void GameSetupPanel_updateTitle(void*) { /* host no-op */ }
void GameSetupPanel_drawGrid(void*) { /* host no-op */ }
void Game_Shutdown(int*) { /* host no-op */ }
void RESMGR_Shutdown(int) { /* host no-op */ }
void CRT_0x470650() { /* host no-op */ }
void* UI_MainMenu_Ctor(void* mem, void*, unsigned int) { return mem; }
void UI_MainMenu_Create(void*, void*) { /* host no-op */ }
void* Town_Ctor(void* mem, void*, unsigned int) { return mem; }
void Town_InitSprites(void*, void*) { /* host no-op */ }
void* PostcardPreviewWindow_Ctor(void* mem, void*, unsigned int) { return mem; }
void LOCOBITMAP_InitWindow(void*, void*) { /* host no-op */ }
void* TrainStationWindow_Ctor(void* mem, void*, unsigned int) { return mem; }
void TrainStationWindow_Create(void*, void*) { /* host no-op */ }
void* LOCOBITMAP_CreateFromResource(void* mem, void*, unsigned int) { return mem; }
void* Cursor_Ctor(void* mem, void*, unsigned int) { return mem; }
void Cursor_Create(void*, void*) { /* host no-op */ }
void* AudioMgr_Ctor(void* mem, void*, unsigned int) { return mem; }
void HelpWnd_Create(void*, void*) { /* host no-op */ }
void* CGWND_AboutDialog_Ctor(void* mem, void*, unsigned int) { return mem; }
void CGWND_AboutDialog_Create(void*, void*) { /* host no-op */ }
void CGWND_RegisterWindowClass(void*) { /* host no-op */ }
/* NETMAN_constructor(void*): silent no-op returning void, but
 * core/GameLoop.cpp's caller (`g_netman = NETMAN_constructor(mem)`)
 * expects and uses a void* return — collided with shared/stubs_impl.cpp's
 * correctly-typed, loud `void* NETMAN_constructor(void*)` stub
 * (LINK-001). Removed; stubs_impl.cpp's survives. */
void DirectPlay_constructor(void*) { /* host no-op */ }
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
void TileMap_Init(void*, unsigned char) { /* host no-op */ }
void GameAudio_StopFinished(void*) { /* host no-op */ }
void DDRAW_GetDsoundErrorString(int) { /* host no-op */ }
void Ordinal_2(void*) { /* host no-op */ }
void RESMGR_GetById(void*, unsigned int) { /* host no-op */ }
void GetResourceType(int) { /* host no-op */ }
void CGWND_AudioChannel_Play(unsigned int) { /* host no-op */ }
void CGWND_AudioChannel_Pause(int) { /* host no-op */ }
void Game_SetScreenMode(void*, int, int, int) { /* host no-op */ }
void BuildingMgr_DestroyAll(void*, int) { /* host no-op */ }
void UI_ResetTooltips(void*, int) { /* host no-op */ }
void World_Reset(void*, int) { /* host no-op */ }
void Cursor_Show(void*) { /* host no-op */ }
void NETMAN_SendMapData(void*, int) { /* host no-op */ }
void CGWND_AudioChannel_IsActive(unsigned int) { /* host no-op */ }
void CGWND_AudioChannel_Release(void*) { /* host no-op */ }
void WIN32_CloseHandle(void*) { /* host no-op */ }
void WIN32_timeKillEvent(unsigned int) { /* host no-op */ }
void GameAudio_StopFinished() { /* host no-op */ }
void DDRAW_GetDsoundErrorString() { /* host no-op */ }
void Ordinal_2() { /* host no-op */ }
void RESMGR_GetById() { /* host no-op */ }
void GetResourceType() { /* host no-op */ }
void CGWND_AudioChannel_Play() { /* host no-op */ }
void CGWND_AudioChannel_Pause() { /* host no-op */ }
void Game_SetScreenMode() { /* host no-op */ }
void BuildingMgr_DestroyAll() { /* host no-op */ }
void UI_ResetTooltips() { /* host no-op */ }
void World_Reset() { /* host no-op */ }
void Cursor_Show() { /* host no-op */ }
void NETMAN_SendMapData() { /* host no-op */ }
void CGWND_AudioChannel_IsActive() { /* host no-op */ }
void CGWND_AudioChannel_Release() { /* host no-op */ }
void WIN32_CloseHandle() { /* host no-op */ }
void WIN32_timeKillEvent() { /* host no-op */ }
int WIN32_GetThreadResult(void*) { return 0; }
void World_Shutdown(int) { /* host no-op */ }
void Train_FlushMessages(void*) { /* host no-op */ }
void WIN32_Sleep(unsigned int) { /* host no-op */ }
void Sprite_UnlockAll(int) { /* host no-op */ }
void UIPANEL_FreeAllSurfaces() { /* host no-op */ }
void WIN32_timeEndPeriod(unsigned int) { /* host no-op */ }
void Sprite_Shutdown(int) { /* host no-op */ }
void Town_GameView_Cleanup(int*) { /* host no-op */ }
void DDRAW_InvalidateAll(int*) { /* host no-op */ }
void RESDATA_ScriptedObject_Shutdown(int*) { /* host no-op */ }
void UI_FreeMessageBox(int) { /* host no-op */ }

