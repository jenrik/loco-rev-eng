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

/* IDirectDrawClipper — forward declaration only (g_clipper_0..5 below are
 * pointers, no complete type needed). Declared outside extern "C" per
 * CLAUDE.md: C++ class declarations must not be forced to C linkage; the
 * `extern "C"` on the *variable* declarations themselves is fine — it only
 * affects the pointer's own symbol name, not the pointee type. Avoids
 * #include "../platform/ddraw_interfaces.h" here specifically: it pulls in
 * shared/types.h, which redeclares _g_primary_surface (used a few lines
 * below) with ordinary C++ linkage — conflicting, within this one
 * translation unit, with this file's own extern "C" definition of the same
 * name (a pre-existing, unrelated linkage split between this file and
 * shared/types.h's canonical declaration; out of scope to reconcile here). */
struct IDirectDrawClipper;

extern "C" {
void AudioChannel_Pause();
void AudioChannel_Pause() { /* host no-op */ }
void AudioChannel_Play();
void AudioChannel_Play() { /* host no-op */ }
void ButtonSprite_Ctor();
void ButtonSprite_Ctor() { /* host no-op */ }
/* CGWND_SetMode() (0-arg extern "C") removed (2026-08-06, cross-validation
 * session): was the wrong stub graphics/LOCOBITMAP.cpp silently bound to
 * (its own correctly-typed CGWND_SetMode(int) declaration was inside an
 * extern "C" block, giving it C linkage instead of the real C++-mangled
 * one). Fixed by moving the declaration out of extern "C"; confirmed zero
 * referrers left via nm before removing this stub. */
/* Config_GetIniString(): this 0-arg extern "C" landmine placeholder
 * collided with game/ConfigIni.cpp's real 6-arg extern "C" body
 * (LINK-001 — extern "C" doesn't mangle by arg count/type, so both are
 * literally the same symbol). Removed; no evidence any real caller
 * wants a 0-arg shape. ButtonSprite_Ctor() above is the same 0-arg-landmine
 * pattern but doesn't currently collide with anything (untouched — out of
 * LINK-001's scope; tracked separately in docs/landmine-sweep-worklist.md). */
void DDRAW_RestoreSurfaces();
void DDRAW_RestoreSurfaces() { /* host no-op */ }
void DDRAW_SetSurfaceFormat();
void DDRAW_SetSurfaceFormat() { /* host no-op */ }
void DDRAW_UnlockPrimary();
void DDRAW_UnlockPrimary() { /* host no-op */ }
/* DPlayManager_RenderConnectionPanel — removed. It was a distinct, wrongly
 * (0-arg) named stub that native/NETMAN_NetworkUI.c's NETMAN_JoinSession
 * silently called instead of the real RenderConnectionPanel(NameEntryPanel*)
 * (network/DPlayManager.cpp, 0x4421D0) — a call-0 landmine fixed by
 * correcting that call site to the real name/signature. */
void FormatResourceString();
void FormatResourceString() { /* host no-op */ }
// PlayerRecord_constructor (0x452E10) calls GetUserNameA only after its
// Configuration/PlayerName lookup is empty. Preserve the Win32 size contract
// for the POSIX host instead of silently forcing its "LEGO LOCO" fallback.
uint32_t GetUserNameA(char* buffer, uint32_t* size);
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
void NETMAN_SendPacket();
void NETMAN_SendPacket() { /* host no-op */ }
void PlaySound();
void PlaySound() { /* host no-op */ }
void PlaySoundAt();
void PlaySoundAt() { /* host no-op */ }
void RESMGR_LoadSoundResource();
void RESMGR_LoadSoundResource() { /* host no-op */ }
void RESMGR_ReleaseSoundResource();
void RESMGR_ReleaseSoundResource() { /* host no-op */ }
void Sprite_Init();
void Sprite_Init() { /* host no-op */ }
void Sprite_SetState();
void Sprite_SetState() { /* host no-op */ }
void TileMap_InvalidateRect();
void TileMap_InvalidateRect() { /* host no-op */ }
void UIEntity_Ctor();
void UIEntity_Ctor() { /* host no-op */ }
void UI_MainMenu_SetState();
void UI_MainMenu_SetState() { /* host no-op */ }
void UIPANEL_BeginPaint();
void UIPANEL_BeginPaint() { /* host no-op */ }
/* UIPANEL_Blit() (zero-arg, extern "C") removed: this is the unrelated
 * wrong stub town/Town.cpp used to silently bind to before the
 * town-tilerender-merge session fixed its declaration (2026-08-06), and
 * every other caller cluster was fixed in the 2026-08-06 cross-validation
 * session — confirmed zero referrers via `nm --print-file-name
 * build/lego_loco.p/*.o | grep "U UIPANEL_Blit$"`. See
 * docs/landmine-sweep-worklist.md. */
void UIPANEL_CreateSurface();
void UIPANEL_CreateSurface() { /* host no-op */ }
/* UIPANEL_EndPaintEx() (zero-arg, extern "C") removed 2026-08-13: this was
 * the plain-C-linkage wrong stub that town/Town.cpp and
 * native/NETMAN_SessionSettings.c used to silently bind to (their own
 * correctly-shaped declarations were wrongly inside an extern "C" block,
 * giving them C linkage instead of the real C++-mangled one — extern "C"
 * doesn't mangle by param count/type, so any extern-"C" declaration of
 * this name collided here regardless of its params). Fixed by moving
 * both declarations out of extern "C" (docs/landmine-sweep-worklist.md);
 * confirmed zero referrers left via nm before removing this stub. */
void UI_WindowBase_BaseDtor();
void UI_WindowBase_BaseDtor() { /* host no-op */ }
void UI_WindowBase_Ctor();
void UI_WindowBase_Ctor() { /* host no-op */ }
void VehicleEditor_CheckEditBounds1();
void VehicleEditor_CheckEditBounds1() { /* host no-op */ }
/* WIN32_StreamOpen()/WIN32_StreamOpenFile() extern "C" no-ops removed:
 * both are now real implementations in resources/Win32Stream.h/.cpp, with
 * plain C++ (mangled) linkage matching the majority of existing .cpp
 * callers — these extern "C" bare-name stubs only ever matched the two
 * .c-file callers (native/wave_io.c, native/cgwnd_palette.c), which keep
 * resolving via -Wl,--unresolved-symbols=ignore-all (LINK-001) same as
 * before, pending the separate caller-unification pass tracked in
 * PROGRESS.md. */
void WNDPROC_EnterCriticalSection();
void WNDPROC_EnterCriticalSection() { /* host no-op — single-threaded */ }
void WNDPROC_LeaveCriticalSection();
void WNDPROC_LeaveCriticalSection() { /* host no-op — single-threaded */ }
/* Same family, used by resources/WndProcStreamBuf.cpp's constructor/
 * destructor (originally thin IAT forwarders to Win32 Initialize/
 * DeleteCriticalSection at 0x464D70/0x464D80). */
void WNDPROC_InitializeCriticalSection();
void WNDPROC_InitializeCriticalSection() { /* host no-op — single-threaded */ }
void WNDPROC_DeleteCriticalSection();
void WNDPROC_DeleteCriticalSection() { /* host no-op — single-threaded */ }
/* WNDPROC_StreamFromMemory (extern "C", bare-name) — REMOVED. This
 * unmangled C-linkage stub was the landmine every extern-"C"-declared
 * caller (input/BuildingDescriptorEditor.cpp, ui/UIPANEL_Surface.cpp,
 * ui/HelpWnd.cpp, game/TrainStation.cpp) silently bound to instead of the
 * real, now-implemented C++-mangled definition (resources/Win32StreamMem.cpp)
 * — every one of those callers has been moved out of its extern "C" block
 * to restore the intended C++ linkage; zero remaining references to the
 * bare unmangled symbol anywhere in the tree (confirmed via `nm`). */
void RESDATA_DtorBase();
void RESDATA_DtorBase() { /* host no-op */ }
void ScriptEngine_Init();
void ScriptEngine_Init() { /* host no-op */ }
void UIPANEL_InitScrollPanel();
void UIPANEL_InitScrollPanel() { /* host no-op */ }
void UIPANEL_ScrollPanel_Dtor();
void UIPANEL_ScrollPanel_Dtor() { /* host no-op */ }
void ScriptEngine_Call();
void ScriptEngine_Call() { /* host no-op */ }
void RESDATA_SetPosition();
void RESDATA_SetPosition() { /* host no-op */ }
void HelpWnd_PlayNarration();
void HelpWnd_PlayNarration() { /* host no-op */ }
void CGWND_SetBuildMode();
void CGWND_SetBuildMode() { /* host no-op */ }
void GameAudio_UpdateVolume();
void GameAudio_UpdateVolume() { /* host no-op */ }
void UIPANEL_ScrollPanel_HandleDrag();
void UIPANEL_ScrollPanel_HandleDrag() { /* host no-op */ }
void Panel_DtorBody();
void Panel_DtorBody() { /* host no-op */ }
/* CRT_localtime(): this 0-arg landmine collided with shared/link_stubs.cpp's
 * real (unsigned int*) -> void* body (LINK-001); removed — link_stubs.cpp's
 * survives. */
/* CRT_wcsstr() (0-arg, extern "C") removed 2026-08-16: this exact same
 * call-0 landmine collided with every extern "C" CRT_wcsstr declaration
 * across the tree (game/Building.cpp, game/TrainStation.cpp,
 * ui/UI_ChildWindow.cpp, input/BuildingDescriptorEditor.cpp — all fixed
 * this session, see PROGRESS.md's 2026-08-16 CRT_wcsstr entry), silently
 * dropping both real arguments and returning garbage at every one of
 * their ~44 combined call sites. Those four files now declare it with
 * plain (non-extern "C") C++ linkage matching the real implementation in
 * shared/stubs_link001_batch1_crt_win32.cpp, so this bare-symbol filler
 * is no longer needed and removing it prevents a future extern "C"
 * redeclaration from silently re-arming the same trap. */
void GameObject_GetBoundingRect();
void GameObject_GetBoundingRect() { /* host no-op */ }
void TileMap_GetObjectAt();
void TileMap_GetObjectAt() { /* host no-op */ }
void UI_MainMenu_SetState_void();
void UI_MainMenu_SetState_void() { /* host no-op */ }
void Vehicle_GetOccupantCount();
void Vehicle_GetOccupantCount() { /* host no-op */ }
void* DAT_00479190 = nullptr;
void* DAT_004A9908 = nullptr;
void* DAT_004fd19c = nullptr;
void* DAT_004fd1ac = nullptr;
void* DAT_004fd1c0 = nullptr;
IDirectDrawClipper* g_clipper_0 = nullptr;
IDirectDrawClipper* g_clipper_1 = nullptr;
IDirectDrawClipper* g_clipper_2 = nullptr;
IDirectDrawClipper* g_clipper_3 = nullptr;
IDirectDrawClipper* g_clipper_4 = nullptr;
IDirectDrawClipper* g_clipper_5 = nullptr;
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
void Cursor_BlitEditPreview();
void Cursor_BlitEditPreview() { /* host no-op */ }
void Cursor_UpdateScrollButtons();
void Cursor_UpdateScrollButtons() { /* host no-op */ }
void Cursor_DrawColorPalette();
void Cursor_DrawColorPalette() { /* host no-op */ }
void Cursor_HandleTabChange();
void Cursor_HandleTabChange() { /* host no-op */ }
/* GetOpenFileNameA(): this 0-arg landmine collided with graphics/
 * sdl3_window.cpp's real (void* lpofn) -> BOOL body (LINK-001); removed —
 * sdl3_window.cpp's survives. */
void NET_GetOrCreateSurface();
void NET_GetOrCreateSurface() { /* host no-op */ }
/* NET_UploadAsset and PlaySoundFile moved to shared/stubs_impl.cpp as loud
 * stubs with their real (int, char*)/(char*, int, int, int) signatures —
 * these 0-arg shapes had no real caller (see
 * docs/landmine-sweep-worklist.md "Cursor family"). */
void TileMap_CreateOverlay();
void TileMap_CreateOverlay() { /* host no-op */ }
void FMT_LAYOUT_PATH();
void FMT_LAYOUT_PATH() { /* host no-op */ }
void GAMESTATE_StartGameTimer();
void GAMESTATE_StartGameTimer() { /* host no-op */ }
void* _g_netman = nullptr;
} /* extern "C" */
void CGWND_PumpMessages(char);
void CGWND_PumpMessages(char) { /* host no-op */ }  /* loading-transition pump — C++ linkage */
extern "C" {
void* _g_netman_data = nullptr;
void* STR_LEGO_LOCO = nullptr;
void CGWND_QuitToMenu();
void CGWND_QuitToMenu() { /* host no-op */ }
void* MessageBeep = nullptr;
void IsWindowVisible();
void IsWindowVisible() { /* host no-op */ }
void* World_FinalizeLoad = nullptr;
void* World_GetObjectAt = nullptr;
void* GAMESTATE_SelectLayout = nullptr;
void* CGWND_GameSetup_DrawGrid_Thunk = nullptr;
} /* end extern "C" */

/* C++-linkage stubs */
void* g_scripted_object = nullptr;
void RESDATA_GameObject_UpdateAnimation(void*);
void RESDATA_GameObject_UpdateAnimation(void*) { /* host no-op */ }
void RESDATA_SoundObject_GetState(int);
void RESDATA_SoundObject_GetState(int) { /* host no-op */ }
void RESDATA_SoundObject_GetTextLength(int);
void RESDATA_SoundObject_GetTextLength(int) { /* host no-op */ }
void NETMAN_SendAck(void*);
void NETMAN_SendAck(void*) { /* host no-op */ }
/* UI_CreateTooltip(void*, int, short, int, int): removed — the one
 * canonical UI_CreateTooltip is now declared/defined for real in
 * ui/UI_Utils.cpp, wired to UI_Manager::createTooltip (2026-08-14). */
void UIPANEL_LockSurface(void*);
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
void* RESDATA_CreateChildSprite(void*, void*, int, int);
void* RESDATA_CreateChildSprite(void*, void*, int, int) { fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached — RESDATA_CreateChildSprite(void*, void*, int, int) 0x4546D0, verified unreachable but must not silently return garbage if that changes"); return nullptr; }
void UI_DefWndProc(void*, unsigned int, unsigned int, int);
void UI_DefWndProc(void*, unsigned int, unsigned int, int) { /* host no-op */ }
void Cursor_HandleWindowPaint(void*, int);
void Cursor_HandleWindowPaint(void*, int) { /* host no-op */ }
/** CRT_itoa — Convert integer to string in given radix (base 2-36).
 *  Standard C runtime function. Returns buf. */
char* CRT_itoa(int value, char* buf, int radix);
char* CRT_itoa(int value, char* buf, int radix) {
    if (radix < 2 || radix > 36) { buf[0] = '\0'; return buf; }
    char tmp[36];
    int i = 0;
    unsigned u = (value < 0 && radix == 10) ? -value : static_cast<unsigned>(value);
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
void Cursor_WaitForBlit(void*);
void Cursor_WaitForBlit(void*) { /* host no-op */ }
/* AudioChannel_IsActive(int) removed 2026-08-14 — this `void`-returning
 * stub silently mismatched every real caller's `int`-returning
 * declaration (return type isn't part of C++ mangling); its one real
 * caller (ui/HelpWnd.cpp) now calls the real AudioChannel::IsActive()
 * typed method directly instead. */
void GAMESTATE_ConnectToNetworkGame(void*);
void GAMESTATE_ConnectToNetworkGame(void*) { /* host no-op */ }
void GameWindow_BaseDtor(void*);
void GameWindow_BaseDtor(void*) { /* host no-op */ }
void CGWND_SetFullscreenMode(char);
void CGWND_SetFullscreenMode(char) { /* host no-op */ }
void GameWindow_SetPosition(void*, int, int);
void GameWindow_SetPosition(void*, int, int) { /* host no-op */ }
void GameWindow_Show(void*);
void GameWindow_Show(void*) { /* host no-op */ }
void GameWindow_Hide(void*);
void GameWindow_Hide(void*) { /* host no-op */ }
/* ResourceManager_GetStringById(void*, unsigned int) removed 2026-08-15 —
 * a dead facade this signature ("UINT id") never matched: the real facade
 * is (void*, int) (shared/stubs_link001_batch3_resource_audio.cpp). This
 * overload existed only because ui/HelpWnd.cpp's own declaration used
 * `UINT id`, silently binding all 9 of its call sites here instead of to
 * the real implementation — fixed in HelpWnd.cpp; see PROGRESS.md. */
void* TrainSubsystem_Ctor = nullptr;
/* WIN32_CreateThread / WIN32_QueueAsyncTask: real implementations now
 * live in network/WIN32Thread.cpp (WIN32_QueueAsyncTask's host path is
 * core/HostMode3Bootstrap.cpp's pending-async-task pump). */
void Train_ProcessMessages(void*);
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
uint32_t WIN32_GetSystemMetrics(void*);
uint32_t WIN32_GetSystemMetrics(void*) {
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: WIN32_GetSystemMetrics not implemented "
                         "(TODO: decompile 0x460360)\n");
        warned = true;
    }
    return 0;
}
uint32_t WIN32_RecvNetworkData(void*, uint32_t, const char*);
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
void EditWindow_render(void*);
void EditWindow_render(void*) { /* host no-op */ }
void Town_BlitElement(void*, int, int, int, int, void*, int, int, int, int, int);
void Town_BlitElement(void*, int, int, int, int, void*, int, int, int, int, int) { /* host no-op */ }
void CRT_exit(char const**, char const**);
void CRT_exit(char const**, char const**) { /* host no-op */ }
void* _g_netman_state = nullptr;
void WIN32_ResumeThread(void*, int);
void WIN32_ResumeThread(void*, int) { /* host no-op */ }
void UI_WindowBase_OnCreate(void*);
void UI_WindowBase_OnCreate(void*) { /* host no-op */ }
void UIPANEL_WindowProc(void*, unsigned int, unsigned int, int);
void UIPANEL_WindowProc(void*, unsigned int, unsigned int, int) { /* host no-op */ }
void PlayerConfig_SetName(void*, char const*);
void PlayerConfig_SetName(void*, char const*) { /* host no-op */ }
void PlayerConfig_Save(void*);
void PlayerConfig_Save(void*) { /* host no-op */ }
void NETMAN_SendPacket(void*);
void NETMAN_SendPacket(void*) { /* host no-op */ }
void* CreateHatchBrush = nullptr;
void* g_editwindow_ptr = nullptr;
void EditWindow_cleanupSprites(void*);
void EditWindow_cleanupSprites(void*) { /* host no-op */ }
/* RESDATA_FreeWindow removed: its only caller (ui/EditWindow.cpp) now calls
 * the real PopupWindow::DestroyMCIChild() (0x4544A0) instead — see
 * PROGRESS.md. This stub was orphaned dead code, not a live no-op. */
/* ResourceManager_GetStringById(void**, int) removed 2026-08-15 — a dead
 * facade this signature ("void** mgr") never matched: the real facade is
 * (void* mgr, int id) (shared/stubs_link001_batch3_resource_audio.cpp).
 * This overload existed only because ui/EditWindow.cpp's own declaration
 * used `void** mgr` (and cited a wrong address, 0x460AA0, which is
 * actually inside WIN32_PeekMessageLoop) — fixed in EditWindow.cpp; see
 * PROGRESS.md. */
void NameEntryPanel_Ctor(void*, void*, unsigned int);
void NameEntryPanel_Ctor(void*, void*, unsigned int) { /* host no-op */ }
void NameEntryPanel_CreateWindow(void*, void*);
void NameEntryPanel_CreateWindow(void*, void*) { /* host no-op */ }
void CGWND_GameSetup_Ctor(void*, void*, unsigned int);
void CGWND_GameSetup_Ctor(void*, void*, unsigned int) { /* host no-op */ }
void CGWND_GameSetup_Create(void*, void*);
void CGWND_GameSetup_Create(void*, void*) { /* host no-op */ }
/* NETMAN_CreateSession(void*) — removed. ui/EditWindow.cpp's own
 * `NETMAN_CreateSession` declaration was retyped from `void*` to the real
 * `NameEntryPanel*` (native/NETMAN_NetworkUI.c, 0x4419C0), so this
 * mismatched-signature stub is now dead: EditWindow::show()'s call binds
 * to the real function instead (guarded internally against
 * `_g_netman_data` being null on the host — see that file's comment). */
void UI_WindowBase_Show(void*);
void UI_WindowBase_Show(void*) { /* host no-op */ }
void* BringWindowToTop = nullptr;
void TrackPiece_Dtor(void*);
void TrackPiece_Dtor(void*) { /* host no-op */ }
void __ftol(double);
void __ftol(double) { /* host no-op */ }
/* WIN32_StreamDestroy(void*)/WIN32_StreamDestroyImmediate(void*) silent
 * no-ops removed: DestroyImmediate is now a real implementation in
 * resources/Win32Stream.h/.cpp; Destroy no longer exists as a callable
 * symbol at all — it was pure MSVC vptr-retagging bookkeeping with no
 * observable effect once real C++ virtual-base destruction is in play,
 * and every real caller now uses a real WIN32_Stream local instead of
 * calling it (see Win32Stream.h's doc comment on 0x463A80 for the full
 * evidence trail). WNDPROC_StreamCleanup(void*)'s (C++-linkage) silent
 * no-op is removed too: its only real caller was
 * resources/Win32StreamMem.cpp's WIN32_StreamClose, itself removed as
 * compiler-generated EH-unwind-only dead code (see that file's doc
 * comment on 0x463A60) -- confirmed via grep. The real address,
 * 0x464620, is StreamObject::~StreamObject() (real destructor,
 * resources/StreamObject.h/.cpp). */
void TileMap_SetViewport(void*, void*);
void TileMap_SetViewport(void*, void*) { /* host no-op */ }
void TileMap_GetTileAt(void*, void*);
void TileMap_GetTileAt(void*, void*) { /* host no-op */ }
void DDRAW_GetSurface();
void DDRAW_GetSurface() { /* host no-op */ }
void DDRAW_LoadFile(int*, char const*);
void DDRAW_LoadFile(int*, char const*) { /* host no-op */ }
void Town_CopyTiles8bpp_Transparent(void*, int, int, int, int, int, int, int, int, int, int);
void Town_CopyTiles8bpp_Transparent(void*, int, int, int, int, int, int, int, int, int, int) { /* host no-op */ }
void* DPLAY_SetPlayerName = nullptr;
/* Signature corrected (2026-08-08, network/NetworkPlayerList.cpp STRICT=2
 * cluster): real 2nd param is a UIPANEL_Surface* (Ghidra-confirmed at
 * 0x42A1C0 — dereferences it at the struct's real field offsets), not an
 * int, and callers expect a void* return (they cache it in
 * NetworkPlayerList::surface_cache[]), not void — Itanium mangling ignores
 * return type, so the old `void`-returning stub linked fine and every
 * caller read an undefined register as the "surface" it got back. Still a
 * host no-op (the real deep-copy body at 0x42A1C0 was never transcribed —
 * TODO), but now returns a well-defined value instead of UB. */
struct UIPANEL_Surface;
void* UIPANEL_CopySurface(void*, UIPANEL_Surface*);
void* UIPANEL_CopySurface(void*, UIPANEL_Surface*) { return nullptr; }
/* NET_ComputeColor's real definition now lives in native/NET_Dtor.c
 * (renamed from the stale Ghidra-default-label filename; 0x4441C0). This
 * stub used to be a silent-wrong-stub of exactly the kind documented
 * above for UIPANEL_CopySurface: network/NetworkPlayerList.cpp calls this
 * exact name expecting a real uint32_t color, but got this always-empty
 * `void`-returning no-op (Itanium mangling ignores return type, so it
 * linked fine and every caller read an undefined register as the
 * "color"). Removed now that a real, correctly-typed definition exists —
 * keeping both would be a duplicate-definition link error. */
/* DPLAY_SetPlayerData(void*, const char*) [C++ linkage] removed
 * (2026-08-15) — same silent-wrong-stub class as NET_ComputeColor above:
 * network/NetworkPlayerList.cpp's RegisterPlayer called this expecting a
 * real int32_t result, but got this always-void no-op (Itanium mangling
 * ignores return type, so it linked fine and every caller read an
 * undefined register as the result). RegisterPlayer now calls the real,
 * already-implemented DPlayManager::SetPlayerData() method directly. */
void TileMap_UpdateViewport(void*, void*, short);
void TileMap_UpdateViewport(void*, void*, short) { /* host no-op */ }
void TileMap_GetTileRect(void*, void*);
void TileMap_GetTileRect(void*, void*) { /* host no-op */ }
void SetRect(void*, int, int, int, int);
void SetRect(void*, int, int, int, int) { /* host no-op */ }
void* GAMESTATE_LoadExistingGame = nullptr;
void* World_SerializeObject = nullptr;
void* STR_REMOVED = nullptr;
void* DPLAY_InitPlayerSlot = nullptr;
void NETMAN_ReceiveFileTransfer(int);
void NETMAN_ReceiveFileTransfer(int) { /* host no-op */ }
void NETMAN_SendAck(int);
void NETMAN_SendAck(int) { /* host no-op */ }
void* GAMESTATE_SetDifficulty = nullptr;
void* DPLAY_EnumeratePlayers = nullptr;
void* NET_SendFile = nullptr;
/* Vehicle_SetState(void*,int): wrong return type (void; real callers in
 * core/GameLoop.cpp etc. declare `int`) — collided with shared/
 * stubs_impl.cpp's correctly-typed `int Vehicle_SetState(void*, int)`
 * (LINK-001). Removed; stubs_impl.cpp's survives. */
void NETMAN_ReceiveLayoutSelect(int);
void NETMAN_ReceiveLayoutSelect(int) { /* host no-op */ }
/* PlayerConfig_SaveToFile(void*) removed 2026-08-15 — declared `char*`-
 * returning at its one real caller (network/DPlayManager.cpp's
 * DPlayManager::SetPlayerData) but defined `void`-returning here
 * (Itanium mangling ignores return type, so it linked silently and
 * `config_str` read an undefined register) — the same return-type-
 * mismatch landmine documented repeatedly this session. The real
 * PlayerConfig::SaveToFile() (game/PlayerConfig.cpp, 0x453320) was
 * already fully implemented; the call site now calls
 * g_player_config->SaveToFile() directly instead of this facade. This
 * became live (not just latent) once NetworkPlayerList::RegisterPlayer
 * was wired to the real DPlayManager::SetPlayerData earlier this
 * session — that fix made this facade's caller reachable for real. */
void ResourceManager_Shutdown(int);
void ResourceManager_Shutdown(int) { /* host no-op */ }
void DDRAW_FileData_Dtor(void*);
void DDRAW_FileData_Dtor(void*) { /* host no-op */ }
void GAMESTATE_HandleNetworkGame();
void GAMESTATE_HandleNetworkGame() { /* host no-op */ }
void VehicleEditor_GetResourceId(int);
void VehicleEditor_GetResourceId(int) { /* host no-op */ }
void VehicleEditor_RemoveVehicle(void*, int);
void VehicleEditor_RemoveVehicle(void*, int) { /* host no-op */ }
void GameVehicle_RemoveDestination(void*, unsigned int, char);
void GameVehicle_RemoveDestination(void*, unsigned int, char) { /* host no-op */ }
void GAMESTATE_EditorState_Detach(int);
void GAMESTATE_EditorState_Detach(int) { /* host no-op */ }
void CRT_memset_pattern(void*, int, int, void*, void*);
void CRT_memset_pattern(void*, int, int, void*, void*) { /* host no-op */ }
void CRT_free_pattern(void*, int, int, void*);
void CRT_free_pattern(void*, int, int, void*) { /* host no-op */ }
void RESDATA_DtorBody(void*);
void RESDATA_DtorBody(void*) { /* host no-op */ }
void DDRAW_PresentRect(void*, void*, int*, unsigned char);
void DDRAW_PresentRect(void*, void*, int*, unsigned char) { /* host no-op */ }
/* WIN32_RecvNetworkData/WIN32_GetSystemMetrics: these C++-mangled overloads
 * existed only because network/DirectPlay.cpp declared the two functions
 * outside its extern "C" block. Both are now real implementations in
 * network/DirectPlay.cpp (0x460EA0, 0x460360) declared extern "C" there
 * (matching game/Train_network.cpp's existing extern "C" declarations for
 * the sibling WIN32_SendNetworkData/WIN32_PeekMessageLoop), so nothing
 * requests these mangled symbols anymore. */
void Ordinal_4(void*, void**, void*, void*, void*);
void Ordinal_4(void*, void**, void*, void*, void*) { /* host no-op */ }
void CRT_memcpy(void*, void const*, unsigned long);
void CRT_memcpy(void*, void const*, unsigned long) { /* host no-op */ }
void CRT_malloc(unsigned long);
void CRT_malloc(unsigned long) { /* host no-op */ }
void CRT_sprintf_buf(char*, char const*);
void CRT_sprintf_buf(char*, char const*) { /* host no-op */ }
void CRT_toupper(int);
void CRT_toupper(int) { /* host no-op */ }
void CRT_timeGetTime(int*);
void CRT_timeGetTime(int*) { /* host no-op */ }
void CRT_FindClose(void*);
void CRT_FindClose(void*) { /* host no-op */ }
void CRT_FindFirstFile(char const*, void*);
void CRT_FindFirstFile(char const*, void*) { /* host no-op */ }
void CRT_FindNextFile(void*, void*);
void CRT_FindNextFile(void*, void*) { /* host no-op */ }
void RESDATA_UpdateChild(void*);
void RESDATA_UpdateChild(void*) { /* host no-op */ }
void Game_IsPositionBetween(int, int, int);
void Game_IsPositionBetween(int, int, int) { /* host no-op */ }
void PlayerConfig_GetName(void*, char*, int);
void PlayerConfig_GetName(void*, char*, int) { /* host no-op */ }
void CRT_memset(void*, int, unsigned long);
void CRT_memset(void*, int, unsigned long) { /* host no-op */ }
void World_CheckActive(void*);
void World_CheckActive(void*) { /* host no-op */ }
void TrackPiece_SetZoom(void*, short);
void TrackPiece_SetZoom(void*, short) { /* host no-op */ }
void Town_BlitViewport(void*,int,int,int,int,int,int);
void Town_BlitViewport(void*,int,int,int,int,int,int) { /* host no-op */ }
void Town_BlitElement(void*, int, int, int, int, void*, int, int, int, int, unsigned int);
void Town_BlitElement(void*, int, int, int, int, void*, int, int, int, int, unsigned int) { /* host no-op */ }
void* g_game_instance = nullptr;
int32_t g_OutputDebugStringA = 0;
void VehicleEditor_Update(void*);
void VehicleEditor_Update(void*) { /* host no-op */ }
void VehicleEditor_IsInBounds(void*, short, short, short);
void VehicleEditor_IsInBounds(void*, short, short, short) { /* host no-op */ }
void VehicleEditor_BlitBackground(void*, int, int);
void VehicleEditor_BlitBackground(void*, int, int) { /* host no-op */ }


void Vehicle_InitRoute(void*, int, unsigned int, char);
void Vehicle_InitRoute(void*, int, unsigned int, char) { /* host no-op */ }
void Vehicle_Ctor(void*, int, int, char, char);
void Vehicle_Ctor(void*, int, int, char, char) { /* host no-op */ }
void Vehicle_UpdatePosition(void*, char);
void Vehicle_UpdatePosition(void*, char) { /* host no-op */ }
void Building_RemoveOccupant(int*);
void Building_RemoveOccupant(int*) { /* host no-op */ }
void TrackPiece_Ctor(void*, int, int, unsigned short);
void TrackPiece_Ctor(void*, int, int, unsigned short) { /* host no-op */ }
void GameObject_SetWorldPos(void*, int, int);
void GameObject_SetWorldPos(void*, int, int) { /* host no-op */ }
void GameObject_InvalidateRect(void*);
void GameObject_InvalidateRect(void*) { /* host no-op */ }
void GameObject_GetRelPos(void*, int*, int, int);
void GameObject_GetRelPos(void*, int*, int, int) { /* host no-op */ }
/** Config_WriteInt — Write integer to INI file section:key
 *  Address: 0x452DB0. __thiscall (ECX=config, stack=section,key,value).
 *  Converts int to string via CRT_itoa(10), writes via WritePrivateProfileStringA.
 *  INI path is at config+4. */
/** WritePrivateProfileStringA — Win32 INI file write.
 *  Host stub: logs and returns success (non-zero).
 *  TODO: implement actual INI file persistence. */
extern "C" int WritePrivateProfileStringA(const char* section, const char* key,
                                           const char* value, const char* filename);
extern "C" int WritePrivateProfileStringA(const char* section, const char* key,
                                           const char* value, const char* filename) {
    fprintf(stderr, "HOST: WritePrivateProfileStringA(%s, %s, %s, %s) — stub\n",
            section ? section : "(null)", key ? key : "(null)",
            value ? value : "(null)", filename ? filename : "(null)");
    return 1; /* non-zero = success */
}
/* NOTE: this C++-linkage Config_WriteInt overload coexists with at least
 * five other declarations of the same name across the tree (game/
 * ConfigIni.cpp's extern "C" real implementation, network/Netman.h,
 * native/{NET_BaseDtor,ddraw_audio_destroy}.c, core/CGWND.cpp's own
 * static-inline no-op) — different linkage/signatures mean these don't
 * all collide at the linker, so which one a given caller reaches depends
 * on how that caller declared it. `config`'s real type was not
 * re-verified here; cast style only, no semantic change. Left for a
 * dedicated Config_WriteInt linkage-cluster session, not this STRICT=2
 * pass. */
void __thiscall Config_WriteInt(void* config, const char* section, const char* key, int value);
void __thiscall Config_WriteInt(void* config, const char* section, const char* key, int value) {
    char buf[100];
    CRT_itoa(value, buf, 10);
    WritePrivateProfileStringA(section, key, buf, static_cast<char*>(config) + 4);
}
void LOCOBITMAP_ColorKeyBlit_thunk(void*);
void LOCOBITMAP_ColorKeyBlit_thunk(void*) { /* host no-op */ }
/* NET_UpdatePlayerList: real body now in network/NetworkPlayerList.cpp
 * (0x445170, returns short). This used to be a wrong-signature (void
 * return) no-op stub that PlayerConfig.cpp's C++-linkage declaration
 * silently resolved to instead of the (previously nonexistent) real
 * function; keeping both would now be a duplicate-definition/ODR
 * violation since C++ mangling does not encode return type. */
void Town_CheckOccupied(void*, int, int, int, int);
void Town_CheckOccupied(void*, int, int, int, int) { /* host no-op */ }
/* Town_SelectBuilding(void*, void*) — real implementation now in
 * town/Town.cpp (calls GameView::select_building with a real Building*).
 * Removed the no-op stub here per CLAUDE.md's "no --defsym-style
 * placeholders" rule; a duplicate definition here would also be a link
 * error against the real one. */
void UIPANEL_BlitSurface(void*, int, int, void*, int, int);
void UIPANEL_BlitSurface(void*, int, int, void*, int, int) { /* host no-op */ }
void DDRAW_SelectBuilding(void*, void*);
void DDRAW_SelectBuilding(void*, void*) { /* host no-op */ }
/* WIN32_PostQuit: real implementation now in core/CGWND.cpp (0x463670). */
void RESDATA_GameVehicle_Ctor(void*, int);
void RESDATA_GameVehicle_Ctor(void*, int) { /* host no-op */ }
void RESDATA_GameVehicle_BaseDtor(void*);
void RESDATA_GameVehicle_BaseDtor(void*) { /* host no-op */ }
void Vehicle_InitOccupant(void*, int);
void Vehicle_InitOccupant(void*, int) { /* host no-op */ }
void Vehicle_IsMoving(void*);
void Vehicle_IsMoving(void*) { /* host no-op */ }
void Vehicle_Stop(void*, int, unsigned char);
void Vehicle_Stop(void*, int, unsigned char) { /* host no-op */ }
void Entity_StopSound(void*, int);
void Entity_StopSound(void*, int) { /* host no-op */ }
void GameObject_InitBase(void*, int, int, unsigned char);
void GameObject_InitBase(void*, int, int, unsigned char) { /* host no-op */ }
void UIPANEL_UnlockSurface(void*);
void UIPANEL_UnlockSurface(void*) { /* host no-op */ }
void* DAT_00485270 = nullptr;
void NETMAN_SetGameMode(void*, int);
void NETMAN_SetGameMode(void*, int) { /* host no-op */ }
void GAMESTATE_FindAdjacentTrack(void*);
void GAMESTATE_FindAdjacentTrack(void*) { /* host no-op */ }
void RESDATA_IsValidTrackIndex(void*, short);
void RESDATA_IsValidTrackIndex(void*, short) { /* host no-op */ }
void Vehicle_GetNearestTrack(int);
void Vehicle_GetNearestTrack(int) { /* host no-op */ }
void GAMESTATE_FindTrackPosition(void*, int, int);
void GAMESTATE_FindTrackPosition(void*, int, int) { /* host no-op */ }
/* DPLAY_CreatePlayer(void*) removed 2026-08-14 — zero real call sites
 * tree-wide; its one former caller (Cursor::init_network_player) now
 * constructs a real DPlayManager directly (input/Cursor_impls.cpp). */
void GAMESTATE_EditorState_Ctor(void*, char);
void GAMESTATE_EditorState_Ctor(void*, char) { /* host no-op */ }
void DPLAY_CleanupPlayer(void*);
void DPLAY_CleanupPlayer(void*) { /* host no-op */ }
void GAMESTATE_InitTrackAtPosition(void*, int, int);
void GAMESTATE_InitTrackAtPosition(void*, int, int) { /* host no-op */ }
void GameObject_HitTest(void*, int, int);
void GameObject_HitTest(void*, int, int) { /* host no-op */ }
void Vehicle_DetachAll(int);
void Vehicle_DetachAll(int) { /* host no-op */ }
void CGWND_InitAllSubsystems(void*);
void CGWND_InitAllSubsystems(void*) { /* host no-op */ }
void timeBeginPeriod(unsigned int);
void timeBeginPeriod(unsigned int) { /* host no-op */ }
/* CGWND_PumpMessages(void*,unsigned char) — now in CGWND_sdl3.cpp */
void CGWND_AudioChannel_UpdatePosition(void*, int, int);
void CGWND_AudioChannel_UpdatePosition(void*, int, int) { /* host no-op */ }
void CGWND_AudioChannel_Stop(void*);
void CGWND_AudioChannel_Stop(void*) { /* host no-op */ }
void NETMAN_ResetNetworkState(void*);
void NETMAN_ResetNetworkState(void*) { /* host no-op */ }
void NETMAN_StopSession(void*);
void NETMAN_StopSession(void*) { /* host no-op */ }
void NETMAN_StartClientSession(void*);
void NETMAN_StartClientSession(void*) { /* host no-op */ }
void Train_QueueMessage(void*, void*);
void Train_QueueMessage(void*, void*) { /* host no-op */ }
void NETMAN_Init(void*, unsigned char);
void NETMAN_Init(void*, unsigned char) { /* host no-op */ }
void GameSetupPanel_loadLayouts(void*, unsigned char);
void GameSetupPanel_loadLayouts(void*, unsigned char) { /* host no-op */ }
void GameSetupPanel_updateTitle(void*);
void GameSetupPanel_updateTitle(void*) { /* host no-op */ }
void GameSetupPanel_drawGrid(void*);
void GameSetupPanel_drawGrid(void*) { /* host no-op */ }
void Game_Shutdown(int*);
void Game_Shutdown(int*) { /* host no-op */ }
/* RESMGR_Shutdown(int) removed 2026-08-15 — its one caller
 * (core/CGWND.cpp's CGWND_Cleanup) always passed the literal address
 * 0x4855E8 (confirmed via Ghidra disassembly of 0x407AD2: `MOV ECX,
 * 0x4855e8` immediately before the real call), i.e. &g_resmgr — this
 * was another silent-wrong-stub of the class documented elsewhere in
 * this file: the real ResourceManager::Shutdown() (0x446340) is
 * already fully implemented, but this free-function facade discarded
 * the call entirely. The call site now calls g_resmgr.Shutdown()
 * directly. NOT the same fix as ResourceManager_Shutdown(int32_t)
 * below, which network/NetHelpers.cpp's PoolAllocator::Shutdown()
 * still calls with a different, as-yet-unresolved receiver — see
 * PROGRESS.md. */
void CRT_0x470650();
void CRT_0x470650() { /* host no-op */ }
void* UI_MainMenu_Ctor(void* mem, void*, unsigned int);
void* UI_MainMenu_Ctor(void* mem, void*, unsigned int) { return mem; }
void UI_MainMenu_Create(void*, void*);
void UI_MainMenu_Create(void*, void*) { /* host no-op */ }
void* Town_Ctor(void* mem, void*, unsigned int);
void* Town_Ctor(void* mem, void*, unsigned int) { return mem; }
void Town_InitSprites(void*, void*);
void Town_InitSprites(void*, void*) { /* host no-op */ }
void* PostcardPreviewWindow_Ctor(void* mem, void*, unsigned int);
void* PostcardPreviewWindow_Ctor(void* mem, void*, unsigned int) { return mem; }
void LOCOBITMAP_InitWindow(void*, void*);
void LOCOBITMAP_InitWindow(void*, void*) { /* host no-op */ }
void* TrainStationWindow_Ctor(void* mem, void*, unsigned int);
void* TrainStationWindow_Ctor(void* mem, void*, unsigned int) { return mem; }
void TrainStationWindow_Create(void*, void*);
void TrainStationWindow_Create(void*, void*) { /* host no-op */ }
void* LOCOBITMAP_CreateFromResource(void* mem, void*, unsigned int);
void* LOCOBITMAP_CreateFromResource(void* mem, void*, unsigned int) { return mem; }
void* Cursor_Ctor(void* mem, void*, unsigned int);
void* Cursor_Ctor(void* mem, void*, unsigned int) { return mem; }
void Cursor_Create(void*, void*);
void Cursor_Create(void*, void*) { /* host no-op */ }
void* AudioMgr_Ctor(void* mem, void*, unsigned int);
void* AudioMgr_Ctor(void* mem, void*, unsigned int) { return mem; }
void HelpWnd_Create(void*, void*);
void HelpWnd_Create(void*, void*) { /* host no-op */ }
void* CGWND_AboutDialog_Ctor(void* mem, void*, unsigned int);
void* CGWND_AboutDialog_Ctor(void* mem, void*, unsigned int) { return mem; }
void CGWND_AboutDialog_Create(void*, void*);
void CGWND_AboutDialog_Create(void*, void*) { /* host no-op */ }
void CGWND_RegisterWindowClass(void*);
void CGWND_RegisterWindowClass(void*) { /* host no-op */ }
/* NETMAN_constructor(void*): silent no-op returning void, but
 * core/GameLoop.cpp's caller (`g_netman = NETMAN_constructor(mem)`)
 * expects and uses a void* return — collided with shared/stubs_impl.cpp's
 * correctly-typed, loud `void* NETMAN_constructor(void*)` stub
 * (LINK-001). Removed; stubs_impl.cpp's survives. */
void DirectPlay_constructor(void*);
void DirectPlay_constructor(void*) { /* host no-op */ }
/* PixelDataCache_Ctor/PixelDataCache_Load removed (2026-08-08): both were
 * a duplicate ABI-bridge reimplementation of the already-real, already-
 * correct PixelDataCache::Create/Load (graphics/PixelDataCache.h/.cpp,
 * same address 0x401620) — the manual vtable/field poke matched
 * PixelDataCache::Create field-for-field, and PixelDataCache::Load has a
 * real implementation (this stub's Load was a no-op). The one caller
 * (core/GameLoop.cpp) now calls PixelDataCache::Create directly. */
void GameAudio_StopFinished(void*);
void GameAudio_StopFinished(void*) { /* host no-op */ }
void DDRAW_GetDsoundErrorString(int);
void DDRAW_GetDsoundErrorString(int) { /* host no-op */ }
void Ordinal_2(void*);
void Ordinal_2(void*) { /* host no-op */ }
void RESMGR_GetById(void*, unsigned int);
void RESMGR_GetById(void*, unsigned int) { /* host no-op */ }
void CGWND_AudioChannel_Play(unsigned int);
void CGWND_AudioChannel_Play(unsigned int) { /* host no-op */ }
void CGWND_AudioChannel_Pause(int);
void CGWND_AudioChannel_Pause(int) { /* host no-op */ }
void Game_SetScreenMode(void*, int, int, int);
void Game_SetScreenMode(void*, int, int, int) { /* host no-op */ }
void BuildingMgr_DestroyAll(void*, int);
void BuildingMgr_DestroyAll(void*, int) { /* host no-op */ }
/* UI_ResetTooltips(void*, int): removed — real definition now in
 * ui/UI_Utils.cpp, routing to UI_Manager::resetTooltips. */
void World_Reset(void*, int);
void World_Reset(void*, int) { /* host no-op */ }
void Cursor_Show(void*);
void Cursor_Show(void*) { /* host no-op */ }
void NETMAN_SendMapData(void*, int);
void NETMAN_SendMapData(void*, int) { /* host no-op */ }
/* CGWND_AudioChannel_IsActive(unsigned int) removed 2026-08-14 — same
 * return-type-mismatch landmine as AudioChannel_IsActive above (this
 * `void`-returning stub vs. core/CGWND.cpp's only caller declaring it
 * `int`-returning). Its one caller now calls AudioChannel::IsActive()
 * directly instead of through this free-function facade. */
void CGWND_AudioChannel_Release(void*);
void CGWND_AudioChannel_Release(void*) { /* host no-op */ }
void WIN32_CloseHandle(void*);
void WIN32_CloseHandle(void*) { /* host no-op */ }
void WIN32_timeKillEvent(unsigned int);
void WIN32_timeKillEvent(unsigned int) { /* host no-op */ }
void GameAudio_StopFinished();
void GameAudio_StopFinished() { /* host no-op */ }
void DDRAW_GetDsoundErrorString();
void DDRAW_GetDsoundErrorString() { /* host no-op */ }
void Ordinal_2();
void Ordinal_2() { /* host no-op */ }
void RESMGR_GetById();
void RESMGR_GetById() { /* host no-op */ }
void CGWND_AudioChannel_Play();
void CGWND_AudioChannel_Play() { /* host no-op */ }
void CGWND_AudioChannel_Pause();
void CGWND_AudioChannel_Pause() { /* host no-op */ }
void Game_SetScreenMode();
void Game_SetScreenMode() { /* host no-op */ }
void BuildingMgr_DestroyAll();
void BuildingMgr_DestroyAll() { /* host no-op */ }
void UI_ResetTooltips();
void UI_ResetTooltips() { /* host no-op */ }
void World_Reset();
void World_Reset() { /* host no-op */ }
void Cursor_Show();
void Cursor_Show() { /* host no-op */ }
void NETMAN_SendMapData();
void NETMAN_SendMapData() { /* host no-op */ }
void CGWND_AudioChannel_Release();
void CGWND_AudioChannel_Release() { /* host no-op */ }
void WIN32_CloseHandle();
void WIN32_CloseHandle() { /* host no-op */ }
void WIN32_timeKillEvent();
void WIN32_timeKillEvent() { /* host no-op */ }
int WIN32_GetThreadResult(void*);
int WIN32_GetThreadResult(void*) { return 0; }
void World_Shutdown(int);
void World_Shutdown(int) { /* host no-op */ }
void Train_FlushMessages(void*);
void Train_FlushMessages(void*) { /* host no-op */ }
void WIN32_Sleep(unsigned int);
void WIN32_Sleep(unsigned int) { /* host no-op */ }
void Sprite_UnlockAll(int);
void Sprite_UnlockAll(int) { /* host no-op */ }
void UIPANEL_FreeAllSurfaces();
void UIPANEL_FreeAllSurfaces() { /* host no-op */ }
void WIN32_timeEndPeriod(unsigned int);
void WIN32_timeEndPeriod(unsigned int) { /* host no-op */ }
void Sprite_Shutdown(int);
void Sprite_Shutdown(int) { /* host no-op */ }
void Town_GameView_Cleanup(int*);
void Town_GameView_Cleanup(int*) { /* host no-op */ }
void DDRAW_InvalidateAll(int*);
void DDRAW_InvalidateAll(int*) { /* host no-op */ }
void RESDATA_ScriptedObject_Shutdown(int*);
void RESDATA_ScriptedObject_Shutdown(int*) { /* host no-op */ }
void UI_FreeMessageBox(int);
void UI_FreeMessageBox(int) { /* host no-op */ }

