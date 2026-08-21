/* link_stubs.cpp — Correct C++ mangling via native overloading */

// Status: TRANSCRIBED
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <cassert>
#include <unistd.h>

/* Fwd decls for correct type names in mangling */
struct RECT{int32_t l,t,r,b;};
struct POINT{int32_t x,y;};
struct GameWindow; struct Building; struct GameAudio;
struct TileMap; struct RESDATA;
struct RESDATA_ScriptedObject; struct GameSetupPanel;
struct HelpWnd; struct VehicleEditor; struct UI_WindowBase;
struct Cursor; struct TrainEntity;
struct UIPANEL_Surface;
class GameConfig;  /* g_netSettings below — same 0x4FD3A8 singleton as
                     * game/GameConfig.h's canonical `_g_netman_data`. */

/* =========================================================== */
/* A. extern "C" — Win32 APIs + CRT + non-overloaded stubs     */
/* =========================================================== */
extern "C" {
typedef void*HANDLE;typedef void*HWND;typedef void*HINSTANCE;typedef void*HDC;
typedef void*HBRUSH;typedef void*HPEN;typedef void*HFONT;typedef void*HMODULE;
typedef uint32_t DWORD;typedef int32_t BOOL;typedef uint32_t UINT;
typedef const char*LPCSTR;typedef char*LPSTR;

/* Kernel32 */
/* Sleep/CloseHandle/GetLastError/FormatMessageA/LocalFree/CreateFileA/
 * GetFileSize/ExitProcess/PlaySoundA/ClientToScreen/GetClientRect/
 * UpdateWindow/InvalidateRect/DestroyWindow/PostQuitMessage/GetCursorPos/
 * SetFocus/CopyRect/OffsetRect/SetRect/SetRectEmpty/CreateSolidBrush/
 * DeleteObject/GetStockObject/DrawEdge/IntersectRect/SetBkColor/
 * OutputDebugStringA all real-implemented in graphics/sdl3_window.cpp (or,
 * for ReleaseCapture/SetFocus, ui/GameWindow.cpp / input/Cursor_Editor.cpp).
 * The no-op copies that used to live here were dead weight that only
 * surfaced as LINK-001 --allow-multiple-definition collisions because
 * both sides are extern "C" (same unmangled symbol name). Removed. */
HANDLE GetProcessHeap(void);
HANDLE GetProcessHeap(void){return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(1));}
int32_t HeapFree(HANDLE,DWORD,void*);
int32_t HeapFree(HANDLE,DWORD,void*){return 1;}
/* WaitForSingleObject/ResumeThread: host substitutes for
 * network/WIN32Thread.cpp's WIN32_WaitForThread/WIN32_CloseThreadHandle,
 * which declare these extern "C" (like CloseHandle above) so they must be
 * defined extern "C" too, not with default C++ (mangled) linkage. The SDL
 * host never creates a real OS thread for the network worker
 * (WIN32_QueueAsyncTask's host path is the single-threaded pump in
 * core/HostMode3Bootstrap.cpp), so these are only ever called with a null
 * handle in practice; provided so the assembly-faithful bodies of those
 * two functions still link and, if ever reached, behave sanely:
 * WaitForSingleObject reports "signaled" (never WAIT_TIMEOUT), so a real
 * handle would not be treated as still-running; ResumeThread reports
 * success. */
DWORD WaitForSingleObject(HANDLE,DWORD);
DWORD WaitForSingleObject(HANDLE,DWORD){return 0;}
DWORD ResumeThread(HANDLE);
DWORD ResumeThread(HANDLE){return 0;}
HINSTANCE GetModuleHandleA(LPCSTR);
HINSTANCE GetModuleHandleA(LPCSTR){return reinterpret_cast<HINSTANCE>(static_cast<uintptr_t>(1));}
DWORD GetSystemDefaultLCID(void);
DWORD GetSystemDefaultLCID(void){return 0x0409;}
int32_t CreateDirectoryA(LPCSTR,void*); int32_t DeleteFileA(LPCSTR);
int32_t CreateDirectoryA(LPCSTR,void*){return 1;}int32_t DeleteFileA(LPCSTR){return 1;}
DWORD GetFileAttributesA(LPCSTR);
DWORD GetFileAttributesA(LPCSTR){return static_cast<DWORD>(-1);}
int32_t ReadFile(HANDLE h,void*b,DWORD n,DWORD*r,void*);
int32_t ReadFile(HANDLE h,void*b,DWORD n,DWORD*r,void*){
#ifndef _WIN32
 if(!h||h==reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1)))return 0;
 size_t s=fread(b,1,n,reinterpret_cast<FILE*>(h));if(r)*r=static_cast<DWORD>(s);return(s>0||n==0)?1:0;
#else
 static_cast<void>(h);static_cast<void>(b);static_cast<void>(n);static_cast<void>(r);return 0;
#endif
}
int32_t WriteFile(HANDLE h,const void*b,DWORD n,DWORD*w,void*);
int32_t WriteFile(HANDLE h,const void*b,DWORD n,DWORD*w,void*){
#ifndef _WIN32
 if(!h||h==reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1)))return 0;
 size_t s=fwrite(b,1,n,reinterpret_cast<FILE*>(h));if(w)*w=static_cast<DWORD>(s);return(s==n)?1:0;
#else
 static_cast<void>(h);static_cast<void>(b);static_cast<void>(n);static_cast<void>(w);return 0;
#endif
}
int32_t SystemParametersInfoA(UINT,UINT,void*,UINT);
int32_t SystemParametersInfoA(UINT,UINT,void*,UINT){return 0;}
HANDLE HeapAlloc(HANDLE,DWORD,size_t);
HANDLE HeapAlloc(HANDLE,DWORD,size_t){return malloc(1);}
int32_t GlobalUnlock(HANDLE); HANDLE GlobalHandle(void*p);
int32_t GlobalUnlock(HANDLE){return 1;}HANDLE GlobalHandle(void*p){return reinterpret_cast<HANDLE>(p);}
int32_t GlobalFree(HANDLE h); void timeBeginPeriod(unsigned int);
int32_t GlobalFree(HANDLE h){free(h);return 0;}void timeBeginPeriod(unsigned int){}
/* ShellExecuteA — used by game/Train_network.cpp's Train_ConnectToServer
 * (0x3EB browser-open path, decompiled at 0x43C860) to launch a validated
 * http(s) URL. No established host URL-open helper exists elsewhere in
 * this tree (no xdg-open/SDL_OpenURL wrapper), and Train_ConnectToServer's
 * entire caller chain is currently dead code (TrainSubsystem::ProcessMessages
 * has zero callers), so this is a loud deferred stub rather than a real
 * implementation: fail loudly if ever actually reached instead of silently
 * pretending to open a browser. */
int32_t ShellExecuteA(HWND,LPCSTR operation,LPCSTR file,LPCSTR,LPCSTR,int32_t);
int32_t ShellExecuteA(HWND,LPCSTR operation,LPCSTR file,LPCSTR,LPCSTR,int32_t){
#ifndef _WIN32
 fprintf(stderr,"STUB: ShellExecuteA(op=%s, file=%s) at %s:%d — host URL-open not implemented\n",
         operation?operation:"(null)",file?file:"(null)",__FILE__,__LINE__);
 assert(0 && "stub reached: ShellExecuteA (host URL-open) — implement a real host opener "
             "before wiring up a live caller of Train_ConnectToServer's browser-open path");
 return 0;
#else
 static_cast<void>(operation);static_cast<void>(file);return 0;
#endif
}

/* User32 */
int32_t SetCursorPos(int32_t,int32_t);
int32_t SetCursorPos(int32_t,int32_t){return 1;}
int32_t GetWindowTextA(HWND,char*,int32_t);
int32_t GetWindowTextA(HWND,char*,int32_t){return 0;}
/* ReleaseCapture — real provider for core/Game.cpp::SetScreenMode and
 * ui/GameWindow.cpp (both declare `extern BOOL ReleaseCapture(void);` and
 * rely on this symbol). A first LINK-001 pass deleted this outright,
 * reasoning (wrongly) that ui/GameWindow.cpp's `static` copy was the
 * real one — that copy was internal-linkage-only, so removing both left
 * zero definitions and a silent unresolved-symbol call (a real crash:
 * `-Wl,--unresolved-symbols=ignore-all` resolves it to address 0). */
int32_t ReleaseCapture(void);
int32_t ReleaseCapture(void){return 1;}
/* Win32 cursor/screen APIs used by Game::SetScreenMode and the cursor
 * engine. The SDL host drives the pointer itself; these are no-ops. */
BOOL ScreenToClient(HWND, void* pt);
BOOL ScreenToClient(HWND, void* pt){ (void)pt; return 0; }
HWND SetCapture(HWND);
HWND SetCapture(HWND){ return nullptr; }
void* LoadCursorFromFileA(const char* path);
void* LoadCursorFromFileA(const char* path){ (void)path; return nullptr; }
HWND WindowFromPoint(void);
HWND WindowFromPoint(void){ return nullptr; }
/* Win32 ShowCursor visibility counter. The host SDL cursor is not counted;
 * mirror the decompiled callers' loop expectations (hide -> -1, show -> 0). */
int ShowCursor(int bShow);
int ShowCursor(int bShow){return bShow ? 0 : -1;}
/* Synchronous window-message send. The SDL host has no Win32 message queue;
 * CGWND_Present's WM_USER+7 is a presentation sync that the pump does itself.
 * Signature matches the native cgwnd_present.c declaration. */
int32_t SendMessageA(void*, uint32_t, uint32_t, int32_t);
int32_t SendMessageA(void*, uint32_t, uint32_t, int32_t){return 0;}

/* CGWND_Present — original posts WM_USER+7 to sync UI-init presentation.
 * The SDL pump presents every frame; this is a host no-op with a trace. */
void CGWND_Present(uint32_t);
void CGWND_Present(uint32_t){}

/* Game_DispatchCursorFeedback — original 0x411760; host cursor feedback is
 * driven by Game::UpdateCursorMode directly. */
void Game_DispatchCursorFeedback(void*);
void Game_DispatchCursorFeedback(void*){}

/* UI_ProcessObjectTimers — original 0x420000 walks UI timer lists; the SDL
 * host drives timers from its own pump. */
void UI_ProcessObjectTimers();
void UI_ProcessObjectTimers(){}
int32_t GetWindowRect(HWND,RECT*);
int32_t GetWindowRect(HWND,RECT*){return 0;}
int32_t IsCharAlphaNumericA(char);
int32_t IsCharAlphaNumericA(char){return 1;}

/* GDI32 */
int32_t InflateRect(RECT*r,int32_t dx,int32_t dy);
int32_t InflateRect(RECT*r,int32_t dx,int32_t dy){if(r){r->l-=dx;r->t-=dy;r->r+=dx;r->b+=dy;}return 1;}
HPEN CreatePen(int32_t,int32_t,DWORD);
HPEN CreatePen(int32_t,int32_t,DWORD){return reinterpret_cast<HPEN>(static_cast<uintptr_t>(1));}
HFONT CreateFontA(int32_t,int32_t,int32_t,int32_t,int32_t,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCSTR);
HFONT CreateFontA(int32_t,int32_t,int32_t,int32_t,int32_t,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCSTR){return reinterpret_cast<HFONT>(static_cast<uintptr_t>(1));}
int32_t FrameRect(HDC,const RECT*,HBRUSH);
int32_t FrameRect(HDC,const RECT*,HBRUSH){return 1;}
int32_t LineTo(HDC,int32_t,int32_t); int32_t MoveToEx(HDC,int32_t,int32_t,POINT*);
int32_t LineTo(HDC,int32_t,int32_t){return 1;}int32_t MoveToEx(HDC,int32_t,int32_t,POINT*){return 1;}
int32_t IsRectEmpty(const RECT*);
int32_t IsRectEmpty(const RECT*){return 1;}
int32_t Ellipse(HDC,int32_t,int32_t,int32_t,int32_t); int32_t SetPixel(HDC,int32_t,int32_t,DWORD);
int32_t Ellipse(HDC,int32_t,int32_t,int32_t,int32_t){return 1;}int32_t SetPixel(HDC,int32_t,int32_t,DWORD){return 1;}

/* CRT */
int32_t*CRT_errno(void);
static int32_t crt_errno_val=0;int32_t*CRT_errno(void){return&crt_errno_val;}
void CRT_strupr(char*s);
void CRT_strupr(char*s){if(s)for(;*s;s++)if(*s>='a'&&*s<='z')*s-=32;}
void CRT_free(void*p);
void CRT_free(void*p){free(p);}
void*CRT_malloc_zero(uint32_t sz);
void*CRT_malloc_zero(uint32_t sz){void*p=malloc(static_cast<size_t>(sz));if(p)memset(p,0,static_cast<size_t>(sz));return p;}
uint32_t CRT_timeGetTime(void);
uint32_t CRT_timeGetTime(void){return 0;}
int32_t CRT_atoi(const char*s);
int32_t CRT_atoi(const char*s){return s?atoi(s):0;}
void CRT_sprintf(char*,const char*,...);
void CRT_sprintf(char*,const char*,...){}
int32_t CRT_sprintf_buf(char*,const char*,...);
int32_t CRT_sprintf_buf(char*,const char*,...){return 0;}
int32_t CRT_rand(void);
int32_t CRT_rand(void){return rand();}
void _strncpy(char*d,const char*s,size_t n);
void _strncpy(char*d,const char*s,size_t n){if(d&&s)strncpy(d,s,n);}
void InitializeCriticalSection(void*); void EnterCriticalSection(void*);
void InitializeCriticalSection(void*){}void EnterCriticalSection(void*){}
void LeaveCriticalSection(void*); void DeleteCriticalSection(void*);
void LeaveCriticalSection(void*){}void DeleteCriticalSection(void*){}
void*operator_new(size_t size);
void*operator_new(size_t size){void*p=malloc(size);if(!p){fprintf(stderr,"FATAL\n");abort();}memset(p,0,size);return p;}
void GLOBAL_free(void*ptr);
void GLOBAL_free(void*ptr){free(ptr);}

/* Non-overloaded game stubs (C-linkage, one definition each) */
void DDRAW_GetSurfaceWidthHeight(void*,uint16_t*,uint16_t*);
void DDRAW_GetSurfaceWidthHeight(void*,uint16_t*,uint16_t*){}
/* Was declared/defined with zero params (a stale --defsym-era shape); its
 * one caller (ui/UIPANEL_Surface.cpp's UIPANEL_ClearSurface) needs the
 * HRESULT code param to match the real function and discards the return,
 * so the return type is unconstrained. That call site is unreachable on
 * host (guarded behind a local CreateSurface stub that always "succeeds"),
 * so a loud stub is safe per CLAUDE.md's stub policy rather than
 * fabricating a DDERR-code-to-string table without Ghidra evidence. */
void* DDRAW_GetDdrawErrorString(int);
void* DDRAW_GetDdrawErrorString(int){ fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__); assert(0 && "stub reached"); return nullptr; }
/* DPLAY_SetPlayerData(void*,const char*) [extern "C"] removed 2026-08-15
 * — a separate, unmangled symbol from the C++-linkage stub also removed
 * this pass (shared/defsym_stubs.cpp); zero callers use extern "C"
 * linkage for this name anywhere in the tree, confirmed via full-tree
 * grep, so it was already dead, not a live landmine. */
/* DPLAY_CreatePlayer(void*) [extern "C"] removed 2026-08-14 — zero real
 * call sites tree-wide; its one former caller (Cursor::init_network_player)
 * now constructs a real DPlayManager directly (input/Cursor_impls.cpp). */
/* Confirmed via 0x443440: __thiscall, ECX = this, zero pushed stack args —
 * loops surface_cache[256] releasing each cached surface. Was previously
 * (void*, int32_t), which never matched any real caller declaration. */
void DPLAY_LeaveSession(void*);
void DPLAY_LeaveSession(void*){}
void Sprite_Destroy(void*);
void Sprite_Destroy(void*){}
/* RESMGR_PlaySound(int32_t) [extern "C"] removed 2026-08-15 — a
 * fabricated symbol name (the original binary calls the real PlaySound
 * directly, under its real name, at every one of its 66 real call
 * sites — confirmed via get_xrefs_to/decompile on 0x447930/0x403E80).
 * graphics/LOCOBITMAP.cpp and ui/AboutDialog.cpp were the two files
 * still routing through this name; both now call the real
 * PlaySound(UINT) directly. */
/* ResourceManager_GetStringById(void*,uint32_t) removed 2026-08-15 — this
 * declaration sat inside this file's outer extern "C" block (line 27),
 * giving it bare C linkage regardless of its C++-looking signature. That
 * bare symbol is what game/TrainStation.cpp's own (wrongly) extern-"C"
 * `uint32_t id` declaration and ui/UI_ChildWindow.cpp's own extern-"C"
 * declaration both bound to, instead of the real facade
 * (shared/stubs_link001_batch3_resource_audio.cpp, plain C++ linkage,
 * `int id`). Both callers moved to ordinary C++-mangled declarations
 * matching the real facade; see PROGRESS.md. */
void Train_HandleTrackBuild(void*,void*,int32_t);
void Train_HandleTrackBuild(void*,void*,int32_t){}
void Train_SendPlayerInfo(void*,int32_t);
void Train_SendPlayerInfo(void*,int32_t){}
void TrackPiece_SetZoom(void*,int16_t);
void TrackPiece_SetZoom(void*,int16_t){}
void*WIN32_GetThreadResult(void*);
void*WIN32_GetThreadResult(void*){return nullptr;}
/* WIN32_QueueAsyncTask: real implementation now in
 * network/WIN32Thread.cpp (host path: core/HostMode3Bootstrap.cpp). */
/* WIN32_PeekMessageLoop (0x460F10) / WIN32_SendNetworkData (0x460FD0):
 * these extern "C" names (see game/Train_network.cpp's declarations,
 * both __thiscall/unmangled) are DISTINCT linker symbols from the
 * similarly-named C++-mangled overloads network/DirectPlay.cpp declares
 * for its own internal use (WIN32_RecvNetworkData/WIN32_GetSystemMetrics,
 * stubbed in shared/defsym_stubs.cpp) — DirectPlay.cpp never defines a
 * body for either of these two, and its forward declarations are unused
 * within that file. A prior pass removed the no-op stubs that used to
 * satisfy these two names under the false belief that DirectPlay.cpp's
 * (differently-linked) declarations already provided real bodies; with
 * -Wl,--unresolved-symbols=ignore-all (LINK-001) that turned a
 * link-time gap into a runtime null-pointer call the first time
 * TrainSubsystem::DownloadMissingAssets tried to send a missing-asset
 * request (confirmed via `segfault at 0 ip 0000000000000000`). Neither
 * is implemented; both are proven reachable from ordinary multiplayer
 * hosting, so — per CLAUDE.md's stub policy — this warns once instead
 * of a bare assert(0), which would abort the process on the same path.
 * TODO: decompile 0x460F10 / 0x460FD0 for real SDL_net-backed bodies. */
void* WIN32_PeekMessageLoop(void*);
void* WIN32_PeekMessageLoop(void*) {
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: WIN32_PeekMessageLoop not implemented "
                         "(TODO: decompile 0x460F10) — no message polled\n");
        warned = true;
    }
    return nullptr;
}
int WIN32_SendNetworkData(void*, int, void*, int, int);
int WIN32_SendNetworkData(void*, int, void*, int, int) {
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: WIN32_SendNetworkData not implemented "
                         "(TODO: decompile 0x460FD0) — network send dropped\n");
        warned = true;
    }
    return 0;
}
/* WNDPROC_StreamCleanup(void*) extern "C" no-op removed: its only real
 * caller was resources/Win32StreamMem.cpp's WIN32_StreamClose, itself
 * removed as compiler-generated EH-unwind-only dead code (see that
 * file's doc comment on 0x463A60) -- confirmed via grep that nothing
 * else in the tree calls the free function by this name. The real
 * address, 0x464620, is StreamObject::~StreamObject() (real destructor,
 * resources/StreamObject.h/.cpp). */
/* DirectPlay_Close/ConnectToSession/CreatePeer/DestroyPeer/EnumConnections/
 * HostSession/QueryConnection decoy stubs removed 2026-08-10: their
 * extern "C" signatures matched nothing anywhere in the tree even before
 * this pass (confirmed via grep), and now that game/Train_network.cpp and
 * town/Town.cpp call real DirectPlaySession:: methods directly (see
 * network/DirectPlay.h), there is no remaining reason for a decoy of any
 * shape to exist. DirectPlay_constructor (0x443000) was always a bogus
 * address — see network/DirectPlay.h's file-header note: that address is
 * NetworkPlayerList::NetworkPlayerList_ctor, unrelated to DirectPlay. */
int32_t NET_CheckAssetExists(void*,int32_t);
int32_t NET_CheckAssetExists(void*,int32_t){return 0;}
/* NET_FindPlayer moved to shared/stubs_impl.cpp as a loud stub with the
 * real (int, int) C++-linkage signature — this (void*, int32_t) shape had
 * no real caller (see docs/landmine-sweep-worklist.md "Cursor family"). */
int32_t NET_GetAssetPath(int32_t);
int32_t NET_GetAssetPath(int32_t){return 0;}
int32_t NET_GetAttFilePath(int32_t);
int32_t NET_GetAttFilePath(int32_t){return 0;}
int32_t NET_GetFilePath(int32_t);
int32_t NET_GetFilePath(int32_t){return 0;}
/* NET_GetNextAttId(int32_t) removed: this (int32_t)->int32_t shape had no
 * real caller and mangled to a different symbol than either the real
 * definition (native/NET_BaseDtor.c: NET_GetNextAttId(void), 0x445F20) or
 * game/Train_network.cpp's call site — same "no real caller" situation
 * documented for NET_FindPlayer above. Keeping it after adding the real
 * definition would not conflict (different mangled name), but it was
 * already dead weight even before that. */
int32_t NET_MapSpecialAsset(int32_t);
int32_t NET_MapSpecialAsset(int32_t){return 0;}
int32_t NET_RegisterPlayer(int32_t);
int32_t NET_RegisterPlayer(int32_t){return 0;}
/* NET_ResolveAddress(void*,int32_t) [extern "C"] removed 2026-08-14 — it
 * was the one real definition every mismatched-signature declaration of
 * this symbol tree-wide silently bound to (extern "C" ignores signature
 * mismatches at link time), making every real caller always get null.
 * Real implementation now at network/DPlayManager.h/.cpp:
 * DPlayManager* NET_ResolveAddress(const char* hostname). */
void NETMAN_CheckTrackConnection(void*,int32_t,int32_t);
void NETMAN_CheckTrackConnection(void*,int32_t,int32_t){}
int32_t NETMAN_FindPlayerIndex(void*,int32_t);
int32_t NETMAN_FindPlayerIndex(void*,int32_t){return 0;}
int32_t NETMAN_GetPlayerCount(void*);
int32_t NETMAN_GetPlayerCount(void*){return 0;}
void NETMAN_ReceiveAck(void*,void*);
void NETMAN_ReceiveAck(void*,void*){}
void NETMAN_ReceivePing(void*,int32_t,void*);
void NETMAN_ReceivePing(void*,int32_t,void*){}
void NETMAN_SendBuildingData(void*,int32_t,void*);
void NETMAN_SendBuildingData(void*,int32_t,void*){}
void UI_CreateFullWindow(void*,int32_t,void*,int32_t,int32_t,int32_t,int32_t,void*,void*,uint32_t);
void UI_CreateFullWindow(void*,int32_t,void*,int32_t,int32_t,int32_t,int32_t,void*,void*,uint32_t){}
void Vehicle_CalcSpeed(void*,void*);
void Vehicle_CalcSpeed(void*,void*){}
void*Vehicle_Ctor(void*,int32_t,int32_t,char,char);
void*Vehicle_Ctor(void*,int32_t,int32_t,char,char){return nullptr;}
void Vehicle_InitRoute(void*,int32_t,uint32_t,char);
void Vehicle_InitRoute(void*,int32_t,uint32_t,char){}
void VehicleEditor_Ctor(void*,void*);
void VehicleEditor_Ctor(void*,void*){}
void VehicleEditor_GetDPlayData(void*,void*);
void VehicleEditor_GetDPlayData(void*,void*){}
int32_t VehicleEditor_GetResourceId(int32_t);
int32_t VehicleEditor_GetResourceId(int32_t){return 0;}
void VehicleEditor_SetDPlayData(void*,void*);
void VehicleEditor_SetDPlayData(void*,void*){}
/* AssetMgr_LoadFile(void*, const char*, int32_t*) no-op stub removed
 * 2026-08-17 — see shared/core_stubs.cpp's matching removal note; the
 * g_asset_mgr extern-global-type-mismatch cleanup
 * (resources/AssetArchive.h) left every real call site using
 * `g_asset_mgr.LoadFile(...)` directly. */
/* DPLAY_RenderPlayer stub removed 2026-08-17 — nothing calls this
 * free-function facade anymore; all 3 real call sites now use
 * NetworkPlayerList::RenderPlayer directly (see its own doc comment for
 * the resolved 9-arg object model). */
} /* end extern "C" */

/* =========================================================== */
/* B. C++-linkage overloaded stubs (native C++ overloading)     */
/*    Same function names, different params = correct mangling  */
/* =========================================================== */

/* CGWND_SetMode(void*) removed (2026-08-06, cross-validation session): all
 * 6 callers that used to declare/call this void*-mode overload
 * (ui/EditWindow.cpp, ui/HelpWnd.cpp, game/ScriptedObject.cpp,
 * game/BuildingPanel.cpp, world/scriptengine.cpp, graphics/LOCOBITMAP.cpp)
 * were fixed to call the real CGWND_SetMode(int) (core/CGWND.cpp,
 * 0x408130) instead — see docs/landmine-sweep-worklist.md. Confirmed zero
 * referrers left via nm before removing. */

/* Collection — these need to be member functions for correct vtable mangling */
/* We'll handle vtables separately */

/* Config_GetIniString — C++ overloads */
int32_t Config_GetIniString(void*,const char*,const char*,const char*,char*,int32_t);
int32_t Config_GetIniString(void*,const char*,const char*,const char*,char*,int32_t){return 0;}
int32_t Config_GetIniString(void*,const char*,const char*,const char*,char*,uint32_t);
int32_t Config_GetIniString(void*,const char*,const char*,const char*,char*,uint32_t){return 0;}

/* Cursor_SetCapture — C++ overloads */
void Cursor_SetCapture(void*,int32_t);
void Cursor_SetCapture(void*,int32_t){}        /* _Z17Cursor_SetCapturePvi */
void Cursor_SetCapture(GameWindow*,uint8_t);
void Cursor_SetCapture(GameWindow*,uint8_t){}  /* _Z17Cursor_SetCaptureP10GameWindowh */

/* Cursor_UnlockAllSurfaces() — real implementation is input/Cursor.cpp's
 * 0-arg Cursor_UnlockAllSurfaces (0x414EF0); this no-op copy was dead
 * weight (LINK-001). */
void Cursor_UnlockAllSurfaces(GameWindow*);
void Cursor_UnlockAllSurfaces(GameWindow*){}    /* _Z24Cursor_UnlockAllSurfacesP10GameWindow */

/* Cursor_InitSprites — C++ version with GameWindow* */
void Cursor_InitSprites(GameWindow*);
void Cursor_InitSprites(GameWindow*){}          /* _Z18Cursor_InitSpritesP10GameWindow */

/* FormatResourceString — C++ overloads */
void FormatResourceString(void*,int32_t,char*,int32_t);
void FormatResourceString(void*,int32_t,char*,int32_t){}   /* _Z20FormatResourceStringPviPci */
void FormatResourceString(void*,uint32_t,char*,int32_t);
void FormatResourceString(void*,uint32_t,char*,int32_t){}  /* _Z20FormatResourceStringPvjPci */

/* GameAudio_AllocChannel — C++ overloads */
void GameAudio_AllocChannel(GameAudio*,int32_t,void*,int32_t,int32_t,uint32_t,uint32_t);
void GameAudio_AllocChannel(GameAudio*,int32_t,void*,int32_t,int32_t,uint32_t,uint32_t){}
void GameAudio_AllocChannel(void*,int32_t,void**,int32_t,int32_t,int32_t,int32_t);
void GameAudio_AllocChannel(void*,int32_t,void**,int32_t,int32_t,int32_t,int32_t){}

/* GameAudio_StopAll(GameAudio*) removed 2026-08-15 — was a silent-wrong-
 * stub of the class documented elsewhere in this file (NET_ComputeColor,
 * UIPANEL_CopySurface): ResourceManager::Shutdown's own real, already-
 * implemented body called this expecting the receiver's channels to be
 * actually released, but got this always-empty no-op instead. The call
 * site (resources/ResourceManager.cpp) now calls the real
 * GameAudio::StopAll() method directly instead of a free-function
 * facade. */

/* ResourceManager_GetById overloads live in sdl3_shims/resource_manager_sdl3.cpp. */

/* RESMGR_LoadSoundResource — C++ overloads */
void RESMGR_LoadSoundResource(int32_t);
void RESMGR_LoadSoundResource(int32_t){}                   /* _Z24RESMGR_LoadSoundResourcei */
void RESMGR_LoadSoundResource(void*);
void RESMGR_LoadSoundResource(void*){}                     /* _Z24RESMGR_LoadSoundResourcePv */

/* RESMGR_ReleaseSoundResource — C++ overloads */
void RESMGR_ReleaseSoundResource(int32_t);
void RESMGR_ReleaseSoundResource(int32_t){}                /* _Z27RESMGR_ReleaseSoundResourcei */
void RESMGR_ReleaseSoundResource(void*);
void RESMGR_ReleaseSoundResource(void*){}                  /* _Z27RESMGR_ReleaseSoundResourcePv */

/* Sprite_SetState(void*,int32_t) — no real caller found (every real
 * call site passes a 3rd arg, matching the (void*,int32_t,void*)
 * overload below); duplicated stubs_impl.cpp's loud version of the
 * same 2-arg overload (LINK-001) — stubs_impl.cpp's survives per
 * CLAUDE.md's stub policy. */

/* TileMap_InvalidateRect — C++ overloads */
void TileMap_InvalidateRect(void*,int32_t,int32_t,int32_t,int32_t);
void TileMap_InvalidateRect(void*,int32_t,int32_t,int32_t,int32_t){}     /* _Z22TileMap_InvalidateRectPviiii */
void TileMap_InvalidateRect(TileMap*,int32_t,int32_t,int32_t,int32_t);
void TileMap_InvalidateRect(TileMap*,int32_t,int32_t,int32_t,int32_t){} /* _Z22TileMap_InvalidateRectP7TileMapiiii */

/* UIPANEL_BeginPaint(void*) — real implementation is ui/UIPANEL.cpp's
 * UIPANEL_BeginPaint (0x?, __fastcall); this no-op copy (and the one in
 * stubs_impl.cpp) were dead weight (LINK-001). */

/* UIPANEL_Blit — these three all-`int` no-op overloads used to silently
 * satisfy every mismatched caller declaration in ui/AboutDialog.cpp,
 * core/GameObject.cpp, core/VehicleEditor.cpp (unused decl only),
 * world/tilemap.cpp/.h, network/NetworkPlayerList.cpp,
 * network/DPlayManager.cpp, and input/Cursor_internal.h — worse than a
 * call-0 crash, since it linked cleanly while silently no-op'ing every
 * real blit those callers issued. All those callers now declare the one
 * real signature (ui/UIPANEL_Surface.cpp, bool(void*,uint32_t,uint32_t,
 * int32_t,uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t)) and
 * link against the real implementation instead — see
 * docs/landmine-sweep-worklist.md, UIPANEL_Blit caller cluster. Removed as
 * dead code (zero referrers, confirmed via nm across every lego_loco.p/*.o). */

/* UIPANEL_EndPaintEx — two wrong C++ overloads
 * (void*,int32_t,int32_t,uint8_t,void*) [_Z18UIPANEL_EndPaintExPviihS_,
 * 5th param void* instead of RECT*] and
 * (void*,void*,void*,uint8_t,void*) [_Z18UIPANEL_EndPaintExPvS_S_hS_,
 * 2nd/3rd params void* instead of int] — both were silent-wrong-stub
 * no-ops that mis-declared callers across the tree bound to instead of
 * the real ui/UIPANEL.cpp implementation
 * (docs/landmine-sweep-worklist.md). Removed 2026-08-13 after fixing
 * all known callers; confirmed via `nm` that no remaining .o (native or
 * mingw-typecheck) has an undefined reference to either mangled name. */

/* UI_CreateChildWindow — real implementation now in ui/UI_ChildWindow.cpp
 * (0x424AF0, extern "C"). The C++-mangled overloads that used to shadow
 * it here are gone; all callers now include ui/UI_ChildWindow.h. */

/* UI_CreateMessageBox — C++ overloads */
void*UI_CreateMessageBox(void*,int32_t,int32_t,char,int32_t,int32_t,int32_t);
void*UI_CreateMessageBox(void*,int32_t,int32_t,char,int32_t,int32_t,int32_t){return nullptr;} /* _Z19UI_CreateMessageBoxPviiciii */
void*UI_CreateMessageBox(void*,int32_t,int16_t,char,int32_t,int32_t,char);
void*UI_CreateMessageBox(void*,int32_t,int16_t,char,int32_t,int32_t,char){return nullptr;} /* _Z19UI_CreateMessageBoxPvisciic */

/* UI_CenterWindow — canonical implementation now in stubs_impl.cpp (0x425A50) */
// void UI_CenterWindow(RECT*,RECT*){} — removed duplicate
// void UI_CenterWindow(void*,void*){} — removed duplicate

/* UI_WindowBase_BaseDtor(void*)/UI_WindowBase_Ctor(void*,void*,uint32_t) —
 * duplicated (silent no-op here vs loud stub in stubs_impl.cpp);
 * stubs_impl.cpp's loud versions survive per CLAUDE.md's stub policy
 * (LINK-001). */


/* WIN32_StreamOpen/OpenFile/OpenPath/Read/DestroyImmediate/Destroy — real
 * implementations now in resources/Win32Stream.h/.cpp (0x463810-0x463B6B
 * cluster); the two mismatched extern "C" overloads that used to live here
 * were a decompiler-era mistake (this file's own declarations conflated
 * WIN32_StreamOpen 0x463890 and the distinct WIN32_StreamOpenFile 0x463970
 * into bogus overloads of one name — they are separate functions, see
 * Win32Stream.h). */

/* WNDPROC_StreamFromMemory — REMOVED. Real definition now in
 * resources/Win32StreamMem.cpp (constructs a real WIN32_MemoryStream);
 * every real caller in the tree has been unified onto the (void*, char*,
 * int32_t, int32_t) signature (`_Z24WNDPROC_StreamFromMemoryPvPcii`), and
 * the (void*, const char*, int32_t, int32_t) overload
 * (`_Z24WNDPROC_StreamFromMemoryPvPKcii`) has zero remaining references
 * anywhere in the tree (confirmed via `nm` across every object file). */

/* Resource_IsBuildingTile(void*) / Resource_IsRoadTile(void*) — both had
 * zero remaining call sites tree-wide (confirmed via grep; superseded by
 * ClassifyResourceTile(), game/Vehicle.h) and were only ever a call-0-
 * adjacent trap: they mangle identically to a real, differently-signed
 * RESDATA_IsRoadTile/IsBuildingTile pair used elsewhere. Removed rather
 * than left as no-op stubs a future caller could silently bind to. */

/* Resource_IsValidTrackIndex(void*, int16_t) — real address 0x44BCD0
 * (Ghidra label RESDATA_IsValidTrackIndex). The prior stub here,
 * `void Resource_IsValidTrackIndex(void*, short){}`, mangled identically
 * to every real caller's `extern uint8_t Resource_IsValidTrackIndex(void*,
 * int16_t)` declaration (return type isn't part of Itanium mangling), so
 * every call already read an uninitialized register on every build. Real
 * logic (disassembly-verified): valid when idx==0 (off), idx equals the
 * current track index (+0x636), or (the alternate track index at +0x638
 * is nonzero and idx equals either current+1 or the alternate index).
 * +0x636/+0x638 are the same RESDATA+0x630-family control-point fields
 * documented (and still raw-offset-read, not yet named struct fields) at
 * every one of this function's call sites in world/EditorState.cpp/
 * core/VehicleEditor.cpp — no host resource has real data there yet (see
 * PROGRESS.md's "RESDATA+0x630/+0x636/+0x638 has no host source" item);
 * matches the same raw-offset pattern rather than inventing a new named
 * field ahead of that separate, already-tracked modeling item. */
uint8_t Resource_IsValidTrackIndex(void* resource, int16_t idx);
uint8_t Resource_IsValidTrackIndex(void* resource, int16_t idx)
{
    if (idx == 0) {
        return 1;
    }
    uint16_t current = *reinterpret_cast<const uint16_t*>(
        reinterpret_cast<const uint8_t*>(resource) + 0x636);
    if (static_cast<uint16_t>(idx) == current) {
        return 1;
    }
    uint16_t alternate = *reinterpret_cast<const uint16_t*>(
        reinterpret_cast<const uint8_t*>(resource) + 0x638);
    if (alternate != 0 &&
        (static_cast<uint16_t>(idx) == static_cast<uint16_t>(current + 1) ||
         static_cast<uint16_t>(idx) == alternate)) {
        return 1;
    }
    return 0;
}

/* =========================================================== */
/* D. VTABLE / TYPEINFO (via proper class definitions)          */
/* =========================================================== */

/* Collection — need out-of-line virtual for vtable */
struct Collection_C {
    void**items; int32_t count, capacity;
    virtual ~Collection_C(){}
    virtual void Resize(int32_t){}
    virtual void*GetAt(int32_t){return nullptr;}
};

/* SortedCollection — inherits Collection, adds Compare/SortRange */
struct SortedCollection_C : Collection_C {
    virtual int Compare(void*,void*){return 0;}
    virtual void SortRange(int32_t,int32_t){}
};

/* UIEntity vtable */
struct UIEntity_C {
    virtual ~UIEntity_C(){}
};

/* UI_WindowBase vtable + typeinfo */
struct UI_WindowBase_C {
    virtual ~UI_WindowBase_C(){}
};

/* IDirectDrawSurface4 vtable */
struct IDirectDrawSurface4_C {
    virtual ~IDirectDrawSurface4_C(){}
};

/* =========================================================== */
/* E. GLOBALS                                                    */
/* =========================================================== */
extern "C" {
void* g_object_count = nullptr; void* g_cursor_back = nullptr; int32_t g_cursor_refcount = 0;
void* g_font_normal = nullptr; int32_t g_is_fullscreen = 0;
int32_t g_surface_lost = 0;
/* g_primary_surface moved to platform/ddraw_globals.cpp (2026-08-14) —
 * see that file's own comment; SDL3_EnsurePrimarySurface wires it to a
 * real Sdl3DirectDrawSurface there. */
/* g_surface_bpp/g_surface_bshift/g_surface_channel1/g_surface_channel2/
 * g_surface_red_mask/g_surface_blue_mask moved to platform/ddraw_globals.cpp
 * (2026-08-14) — SDL3_EnsurePrimarySurface (graphics/sdl3_ddraw.cpp) sets
 * them for real once a device exists (see PROGRESS.md's DirectDraw-shim
 * pixel-format note), and that dependency-free TU is what small standalone
 * unit tests linking graphics/sdl3_ddraw.cpp actually link against. */
int32_t g_pixel_format_mask = 0; int32_t g_viewport_rect_left = 0; int32_t g_viewport_rect_top = 0;
int32_t g_viewport_rect_right = 800; int32_t g_viewport_rect_bottom = 600;
int32_t g_window_left = 0; int32_t g_window_top = 0; int32_t g_listener_x = 0; int32_t g_listener_y = 0;
void* _g_train = nullptr;
/* DAT_004a97a0 — Easter-egg language selector (int32_t; PostBag_Subdir's
 * `switch (DAT_004a97a0)` in network/NetworkPlayerList.cpp compares it as a
 * 32-bit int, so the storage must match: on a 64-bit host, defining this as
 * `void*` let NetworkPlayerList.cpp's `extern int32_t` read only the first
 * 4 of the pointer's 8 bytes — harmless today only because nothing else
 * reads/writes it as a pointer, but still a real type mismatch). */
int32_t DAT_004a97a0 = 0;
void* DAT_004a9994 = nullptr;
void* g_click_on_town = nullptr; void* g_click_on_building = nullptr; void* g_cgwnd = nullptr;
int32_t g_clean_exit = 0; void* g_client_rect = nullptr; void* _g_cursor_surface = nullptr;
void* g_frame_event = nullptr; void* g_fullscreen_rect = nullptr;
int32_t g_game_difficulty = 0; int32_t g_in_build_mode = 0; uint8_t g_is_town_mode = 0;
void* g_nameEntryPanel = nullptr; void* _g_network_queue = nullptr;
void* g_network_queue = nullptr; void* _g_network_thread = nullptr; void* g_network_thread = nullptr;
void* g_pixel_data_cache = nullptr; int32_t g_placement_resource_id = 0; int32_t g_ref_count = 0;
void* g_resource_mgr = nullptr; int32_t g_road_build_mode = 0; int32_t g_screen_bpp = 16;
int32_t g_screen_center_x = 400; int32_t g_screen_center_y = 300; void* g_script_engine = nullptr;
int32_t g_show_scrollbars = 0; int32_t g_timer_event_id = 0; int32_t g_timer_id = 0;
void* g_title_font = nullptr; void* g_trackSegmentOffsets = nullptr; void* g_train = nullptr;
void* _g_train_resources = nullptr; void* g_train_resources = nullptr; int32_t g_window_bottom = 600;
int32_t g_window_mode = 0; int32_t g_window_right = 800; void* g_world = nullptr; void* g_about = nullptr;
void* g_asset_archive = nullptr; void* g_asset_base_path = nullptr; void* _g_audio_config = nullptr;
// Same confirmed address as g_backbuffer (platform/ddraw_globals.cpp), which
// is wired to a real Sdl3DirectDrawSurface. Deliberately left unwired here —
// see project_directdraw_shim memory / PROGRESS.md Phase 5(c) note.
void* _g_backbuffer = nullptr;
int32_t g_build_mode = 0; void* _g_dsound_object = nullptr;
/* _g_dplay/_g_dplay_config (formerly declared here as always-null void*
 * globals) and _DAT_004fd3a8 (formerly an int32_t, an additional real
 * 4-vs-8-byte-width mismatch against core/CGWND.cpp's `void*` extern of
 * the same name) removed 2026-08-17: all three were dead, permanently-
 * null/zero aliases of two real singletons that already have correctly
 * assigned canonical pointers elsewhere — `g_dplay`/`g_dplay_config`
 * (network/NetworkPlayerList.h, no leading underscore; core/GameLoop.cpp)
 * for the NetworkPlayerList/PixelDataCache pair at 0x4FD3B0/0x4FD3B4, and
 * `_g_netman_data` (game/GameConfig.h) for the GameConfig singleton at
 * 0x4FD3A8. See game/GameConfig.h's header comment and
 * core/CGWND.cpp's PHASE 5 shutdown for the consolidation. */
int32_t DAT_0047e0f4=0;int32_t DAT_0047e220=0;int32_t DAT_0047e224=0;
int32_t _DAT_00481170=0;int32_t DAT_00481218=0;void* g_world_release_a=nullptr;
void* g_world_release_b=nullptr;int32_t DAT_004aad34=0;int32_t DAT_004aad38=0;
int32_t s_BALANCING_0047e164=0;int32_t s_CleanExit_0047e128=0;
int32_t s_LEGO_LOCO_0047e1c0=0;int32_t s_measure_test_char=0;
int32_t s_MinBuildingFPS_0047e154=0;int32_t s_MinFlyingFPS_0047e134=0;
int32_t s_MinMinifigFPS_0047e144=0;int32_t s_MinVehicleFPS_0047e170=0;
int32_t s_PROCESS_0047e120=0;int32_t s_RectBottom_0047e180=0;
int32_t s_RectLeft_0047e1b4=0;int32_t s_RectRight_0047e18c=0;
int32_t s_RectTop_0047e198=0;
int32_t s_StringFileInfo_080904B0_FileVer_0047e0f8=0;
int32_t s_WINDOW_ATTRIBUTES_0047e1a0=0;
}

/* g_netSettings — same 0x4FD3A8 GameConfig singleton as `_g_netman_data`
 * (game/GameConfig.h), under the stale translation-unit name
 * game/Train_network.cpp's TrainSubsystem historically used. Assigned for
 * real by shared/stubs_impl.cpp's GameConfig_constructor. Declared outside
 * the extern "C" block above since it needs the real GameConfig* type. */
GameConfig* g_netSettings = nullptr;

/* DDRAW_Init — real, Ghidra-verified implementation (address 0x45C8A0,
 * thumbnail-palette surface init) now lives in native/ddraw_init.c.
 * This was the flagship LINK-001 example: two no-op copies (here and in
 * stubs_impl.cpp) plus the real one collided, and --allow-multiple-
 * definition let whichever won be nondeterministic across rebuilds
 * (see PROGRESS.md's "Found and fixed a real nondeterminism bug"
 * entry). Both no-op copies removed. */
