/* link_stubs.cpp — Correct C++ mangling via native overloading */
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <unistd.h>

/* Fwd decls for correct type names in mangling */
struct RECT{int32_t l,t,r,b;};
struct POINT{int32_t x,y;};
struct GameWindow; struct Building; struct GameAudio;
struct TileMap; struct ResourceEntry; struct RESDATA;
struct RESDATA_ScriptedObject; struct GameSetupPanel;
struct HelpWnd; struct VehicleEditor; struct UI_WindowBase;
struct Cursor; struct TrainEntity;
struct UIPANEL_Surface;

/* =========================================================== */
/* A. extern "C" — Win32 APIs + CRT + non-overloaded stubs     */
/* =========================================================== */
extern "C" {
typedef void*HANDLE;typedef void*HWND;typedef void*HINSTANCE;typedef void*HDC;
typedef void*HBRUSH;typedef void*HPEN;typedef void*HFONT;typedef void*HMODULE;
typedef uint32_t DWORD;typedef int32_t BOOL;typedef uint32_t UINT;
typedef const char*LPCSTR;typedef char*LPSTR;

/* Kernel32 */
void Sleep(DWORD){}HANDLE GetProcessHeap(void){return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(1));}
int32_t HeapFree(HANDLE,DWORD,void*){return 1;}int32_t CloseHandle(HANDLE){return 1;}
HINSTANCE GetModuleHandleA(LPCSTR){return reinterpret_cast<HINSTANCE>(static_cast<uintptr_t>(1));}DWORD GetLastError(void){return 0;}
DWORD FormatMessageA(DWORD,void*,DWORD,DWORD,char*,DWORD,void*){return 0;}
void*LocalFree(void*){return nullptr;}DWORD GetSystemDefaultLCID(void){return 0x0409;}
int32_t CreateDirectoryA(LPCSTR,void*){return 1;}int32_t DeleteFileA(LPCSTR){return 1;}
DWORD GetFileAttributesA(LPCSTR){return static_cast<DWORD>(-1);}
void*FindFirstFileA(LPCSTR,void*){return nullptr;}int32_t FindNextFileA(void*,void*){return 0;}
int32_t FindClose(void*){return 0;}
#ifndef _WIN32
#include <sys/stat.h>
#endif
HANDLE CreateFileA(LPCSTR n,DWORD a,DWORD,DWORD,DWORD,DWORD,HANDLE){
#ifndef _WIN32
 const char*m=(a&0x40000000)?((a&0x80000000)?"r+b":"wb"):"rb";
 FILE*f=fopen(n,m);return f?reinterpret_cast<HANDLE>(f):reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1));
#else
 static_cast<void>(n);static_cast<void>(a);return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1));
#endif
}
int32_t ReadFile(HANDLE h,void*b,DWORD n,DWORD*r,void*){
#ifndef _WIN32
 if(!h||h==reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1)))return 0;
 size_t s=fread(b,1,n,reinterpret_cast<FILE*>(h));if(r)*r=static_cast<DWORD>(s);return(s>0||n==0)?1:0;
#else
 static_cast<void>(h);static_cast<void>(b);static_cast<void>(n);static_cast<void>(r);return 0;
#endif
}
int32_t WriteFile(HANDLE h,const void*b,DWORD n,DWORD*w,void*){
#ifndef _WIN32
 if(!h||h==reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1)))return 0;
 size_t s=fwrite(b,1,n,reinterpret_cast<FILE*>(h));if(w)*w=static_cast<DWORD>(s);return(s==n)?1:0;
#else
 static_cast<void>(h);static_cast<void>(b);static_cast<void>(n);static_cast<void>(w);return 0;
#endif
}
DWORD GetFileSize(HANDLE h,DWORD*hi){
#ifndef _WIN32
 if(hi)*hi=0;if(!h||h==reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1)))return static_cast<DWORD>(-1);
 FILE*f=reinterpret_cast<FILE*>(h);long p=ftell(f);fseek(f,0,SEEK_END);long s=ftell(f);
 fseek(f,p,SEEK_SET);return static_cast<DWORD>(s);
#else
 static_cast<void>(h);static_cast<void>(hi);return 0;
#endif
}
int32_t GetModuleFileNameA(HINSTANCE,char*,DWORD){return 0;}
int32_t SystemParametersInfoA(UINT,UINT,void*,UINT){return 0;}
HANDLE HeapAlloc(HANDLE,DWORD,size_t){return malloc(1);}
HANDLE GlobalAlloc(UINT,size_t){return malloc(1);}void*GlobalLock(HANDLE h){return h;}
int32_t GlobalUnlock(HANDLE){return 1;}HANDLE GlobalHandle(void*p){return reinterpret_cast<HANDLE>(p);}
int32_t GlobalFree(HANDLE h){free(h);return 0;}void timeBeginPeriod(unsigned int){}
int32_t ExitProcess(unsigned int){exit(0);return 0;}
int32_t PlaySoundA(LPCSTR,HMODULE,DWORD){return 1;}

/* User32 */
int32_t ClientToScreen(HWND,POINT*){return 1;}int32_t SetCursorPos(int32_t,int32_t){return 1;}
int32_t GetWindowTextA(HWND,char*,int32_t){return 0;}int32_t GetClientRect(HWND,RECT*){return 0;}
HWND GetCapture(void){return nullptr;}int32_t ReleaseCapture(void){return 1;}
/* Win32 cursor/screen APIs used by Game::SetScreenMode and the cursor
 * engine. The SDL host drives the pointer itself; these are no-ops. */
BOOL ScreenToClient(HWND, void* pt){ (void)pt; return 0; }
HWND SetCapture(HWND){ return nullptr; }
void* LoadCursorFromFileA(const char* path){ (void)path; return nullptr; }
HWND WindowFromPoint(void){ return nullptr; }
/* Win32 ShowCursor visibility counter. The host SDL cursor is not counted;
 * mirror the decompiled callers' loop expectations (hide -> -1, show -> 0). */
int ShowCursor(int bShow){return bShow ? 0 : -1;}
/* Synchronous window-message send. The SDL host has no Win32 message queue;
 * CGWND_Present's WM_USER+7 is a presentation sync that the pump does itself.
 * Signature matches the native cgwnd_present.c declaration. */
int32_t SendMessageA(void*, uint32_t, uint32_t, int32_t){return 0;}

/* CGWND_Present — original posts WM_USER+7 to sync UI-init presentation.
 * The SDL pump presents every frame; this is a host no-op with a trace. */
void CGWND_Present(uint32_t){}

/* Game_DispatchCursorFeedback — original 0x411760; host cursor feedback is
 * driven by Game::UpdateCursorMode directly. */
void Game_DispatchCursorFeedback(void*){}

/* UI_ProcessObjectTimers — original 0x420000 walks UI timer lists; the SDL
 * host drives timers from its own pump. */
void UI_ProcessObjectTimers(){}
int32_t UpdateWindow(HWND){return 1;}
int32_t InvalidateRect(HWND,const RECT*,int32_t){return 1;}
int32_t GetWindowRect(HWND,RECT*){return 0;}int32_t DestroyWindow(HWND){return 1;}
int32_t PostQuitMessage(int32_t){return 0;}int32_t GetCursorPos(POINT*){return 0;}
int32_t SetFocus(HWND){return 0;}int32_t IsCharAlphaNumericA(char){return 1;}

/* GDI32 */
void CopyRect(RECT*d,const RECT*s){if(d&&s)*d=*s;}
int32_t OffsetRect(RECT*r,int32_t dx,int32_t dy){if(r){r->l+=dx;r->t+=dy;r->r+=dx;r->b+=dy;}return 1;}
int32_t InflateRect(RECT*r,int32_t dx,int32_t dy){if(r){r->l-=dx;r->t-=dy;r->r+=dx;r->b+=dy;}return 1;}
int32_t SetRect(RECT*r,int32_t l,int32_t t,int32_t ri,int32_t b){if(r){r->l=l;r->t=t;r->r=ri;r->b=b;}return 1;}
int32_t SetRectEmpty(RECT*r){if(r){r->l=r->t=r->r=r->b=0;}return 1;}
HBRUSH CreateSolidBrush(DWORD){return reinterpret_cast<HBRUSH>(static_cast<uintptr_t>(1));}int32_t DeleteObject(void*){return 1;}
HBRUSH GetStockObject(int32_t){return reinterpret_cast<HBRUSH>(static_cast<uintptr_t>(1));}HPEN CreatePen(int32_t,int32_t,DWORD){return reinterpret_cast<HPEN>(static_cast<uintptr_t>(1));}
HFONT CreateFontA(int32_t,int32_t,int32_t,int32_t,int32_t,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCSTR){return reinterpret_cast<HFONT>(static_cast<uintptr_t>(1));}
int32_t DrawEdge(HDC,RECT*,UINT,UINT){return 1;}int32_t FrameRect(HDC,const RECT*,HBRUSH){return 1;}
int32_t LineTo(HDC,int32_t,int32_t){return 1;}int32_t MoveToEx(HDC,int32_t,int32_t,POINT*){return 1;}
int32_t IntersectRect(RECT*,const RECT*,const RECT*){return 0;}int32_t IsRectEmpty(const RECT*){return 1;}
int32_t Ellipse(HDC,int32_t,int32_t,int32_t,int32_t){return 1;}int32_t SetPixel(HDC,int32_t,int32_t,DWORD){return 1;}
int32_t GetDeviceCaps(HDC,int32_t){return 1;}int32_t SetBkColor(HDC,DWORD){return 1;}
void OutputDebugStringA(LPCSTR s){if(s)fprintf(stderr,"DEBUG: %s\n",s);}

/* CRT */
static int32_t crt_errno_val=0;int32_t*CRT_errno(void){return&crt_errno_val;}
void CRT_strupr(char*s){if(s)for(;*s;s++)if(*s>='a'&&*s<='z')*s-=32;}
void CRT_free(void*p){free(p);}
void*CRT_malloc_zero(uint32_t sz){void*p=malloc(static_cast<size_t>(sz));if(p)memset(p,0,static_cast<size_t>(sz));return p;}
uint32_t CRT_timeGetTime(void){return 0;}
int32_t CRT_atoi(const char*s){return s?atoi(s):0;}
void CRT_sprintf(char*,const char*,...){}
int32_t CRT_sprintf_buf(char*,const char*,...){return 0;}
int32_t CRT_rand(void){return rand();}void CRT_srand(unsigned int s){srand(s);}
unsigned int CRT_time(unsigned int*t){return static_cast<unsigned int>(::time(reinterpret_cast<time_t*>(t)));}
void*CRT_memset(void*d,int32_t c,size_t n){return memset(d,c,n);}
void*CRT_memcpy(void*d,const void*s,size_t n){return memcpy(d,s,n);}
char*CRT_strtok(char*,const char*){return nullptr;}
int32_t CRT_toupper(int32_t c){return(c>='a'&&c<='z')?c-32:c;}
char*CRT_itoa(int32_t v,char*buf,int32_t radix){if(buf)snprintf(buf,32,"%d",v);return buf;}
void*CRT_localtime(unsigned int*){static int32_t t=0;return&t;}
int32_t CRT_exit(const char**,const char**){exit(0);return 0;}int32_t CRT_mkdir(const char*){return 0;}
void CRT_memset_pattern(void*,int32_t,int32_t,void*){}
void CRT_free_pattern(void*,int32_t,int32_t,void*){}
void CRT_0x470650(void){}void CRT_strncpy(void*,void*,int32_t){}
void _strncpy(char*d,const char*s,size_t n){if(d&&s)strncpy(d,s,n);}
void InitializeCriticalSection(void*){}void EnterCriticalSection(void*){}
void LeaveCriticalSection(void*){}void DeleteCriticalSection(void*){}
void*operator_new(size_t size){void*p=malloc(size);if(!p){fprintf(stderr,"FATAL\n");abort();}memset(p,0,size);return p;}
void GLOBAL_free(void*ptr){free(ptr);}

/* Non-overloaded game stubs (C-linkage, one definition each) */
void DDRAW_DestroyAudio(void){}
void*DDRAW_GetSurface(void){return nullptr;}
void DDRAW_LoadFile(int32_t*,const char*){}
void DDRAW_ReleaseSurfaces(void){}
void DDRAW_GetSurfaceWidthHeight(void*,uint16_t*,uint16_t*){}
void DDRAW_GetDdrawErrorString(void){}
void DPLAY_SetPlayerData(void*,const char*){}
void DPLAY_CleanupPlayer(void*){}
void*DPLAY_CreatePlayer(void*){return nullptr;}
void DPLAY_InitPlayer(void*,void*,int32_t){}
void DPLAY_LeaveSession(void*,int32_t){}
void Sprite_Destroy(void*){}
void Sprite_Shutdown(int32_t){}
void Sprite_UnlockAll(int32_t){}
void RESMGR_PlaySound(int32_t){}
void RESMGR_GetResourceType(void*,uint32_t){}
void RESMGR_AllocResourceEntry(ResourceEntry*,int32_t,int32_t){}
void RESMGR_SelectScreensaver(char*){}
void ResourceManager_GetStringById(void*,uint32_t){}
void Train_QueueMessage(void*,void*){}
void Train_ConnectToServer(void*,void*){}
void Train_HandleTrackBuild(void*,void*,int32_t){}
void Train_RemoveAllTracks(void*,int32_t){}
void Train_SendPlayerInfo(void*,int32_t){}
void Train_StartMultiplayer(void*,int32_t){}
void Train_StopMultiplayer(void*,void*){}
void TrackPiece_SetZoom(void*,int16_t){}
void WIN32_FatalError(const char*){fprintf(stderr,"FATAL\n");exit(1);}
void WIN32_GetSystemMetrics(void*,int32_t*){}
void*WIN32_GetThreadResult(void*){return nullptr;}
void WIN32_PeekMessageLoop(void*,int32_t){}
void WIN32_PostQuit(void){}
void WIN32_QueueAsyncTask(void*,void*,uint32_t){}
void WIN32_RecvNetworkData(void*,uint32_t,const char*){}
void WIN32_Sleep(uint32_t){}
void WIN32_SendNetworkData(void*,void*,uint32_t){}
void WIN32_StreamOpenPath(void*,const char*,int32_t,int32_t){}
void WIN32_StreamDestroy(void*){}
void WIN32_StreamDestroyImmediate(void*){}
void WIN32_StreamRead(void*,void*,int32_t){}
void WNDPROC_StreamCleanup(void*){}
void WIN32_CloseHandle(void*){}
void WIN32_timeEndPeriod(uint32_t){}void WIN32_timeKillEvent(uint32_t){}
void DirectPlay_Close(void*,int32_t){}
void DirectPlay_ConnectToSession(void*,const char*,const char*,const char*){}
void DirectPlay_constructor(void*){}
void DirectPlay_CreatePeer(void*,void*){}
void DirectPlay_DestroyPeer(void*,int32_t){}
void DirectPlay_EnumConnections(void*,int32_t){}
void DirectPlay_HostSession(void*,const char*,const char*){}
void DirectPlay_QueryConnection(void*,void*){}
int32_t NET_CheckAssetExists(void*,int32_t){return 0;}
int32_t NET_FindArchivedAsset(int32_t){return 0;}
int32_t NET_FindPlayer(void*,int32_t){return 0;}
int32_t NET_GetAssetPath(int32_t){return 0;}
int32_t NET_GetAttFilePath(int32_t){return 0;}
int32_t NET_GetFilePath(int32_t){return 0;}
int32_t NET_GetNextAttId(int32_t){return 0;}
int32_t NET_MapSpecialAsset(int32_t){return 0;}
int32_t NET_RegisterPlayer(int32_t){return 0;}
int32_t NET_ResolveAddress(void*,int32_t){return 0;}
void NETMAN_QueueMessage(void*,int32_t,void*){}
void NETMAN_CheckTrackConnection(void*,int32_t,int32_t){}
int32_t NETMAN_FindPlayerIndex(void*,int32_t){return 0;}
int32_t NETMAN_GetPlayerCount(void*){return 0;}
void NETMAN_ReceiveAck(void*,void*){}
void NETMAN_ReceivePing(void*,int32_t,void*){}
void NETMAN_SendBuildingData(void*,int32_t,void*){}
void GAMESTATE_EditorState_Copy(void*,void*){}
void GAMESTATE_EditorState_Ctor(void*,char){}
void GAMESTATE_InitTrackAtPosition(void*,int32_t,int32_t){}
void GAMESTATE_UpdateVehiclePlacement(void*,int32_t,void*){}
void Cursor_CleanupEditorSprites(void*,int32_t){}
void Cursor_InitEditorSprites(void*,int32_t,void*){}
void Cursor_InitNetworkPlayer(void*,int32_t){}
void UI_CalcDialogCoords(void*,void*,int32_t){}
void UI_ChildWindow_Dtor(void*){}
void UI_ChildWindow_Render(void*,void*){}
void UI_CreateFullWindow(void*,int32_t,void*,int32_t,int32_t,int32_t,int32_t,void*,void*,uint32_t){}
void UI_CreateTooltip(void*,int32_t,int16_t,int32_t,int32_t){}
void UI_OnMouseLeave(void*,int32_t){}
void UI_PaintWindow(void*,void*,void*){}
void UIPANEL_CreateSurface__UIPANEL_Surface(UIPANEL_Surface*){}
void UIPANEL_EndPaint(void*){}
int32_t UIPANEL_UnlockSurface(void*,uint32_t){return 0;}
void UI_WindowBase_Hide(void*){}
void UI_WindowBase_Show(void*){}
void Vehicle_CalcSpeed(void*,void*){}
void*Vehicle_Ctor(void*,int32_t,int32_t,char,char){return nullptr;}
void Vehicle_InitRoute(void*,int32_t,uint32_t,char){}
void VehicleEditor_CheckBounds(void*,void*,int32_t){}
void VehicleEditor_CheckBounds2(void*,void*,int32_t){}
void VehicleEditor_Ctor(void*,void*){}
void VehicleEditor_GetDPlayData(void*,void*){}
int32_t VehicleEditor_GetResourceId(int32_t){return 0;}
void VehicleEditor_InitTracks(void*,void*){}
void VehicleEditor_SetDPlayData(void*,void*){}
void VehicleEditor_TriggerSound(void*,int32_t){}
void VehicleEditor_UpdateEditMode(void*,int32_t,void*){}
void GameVehicle_AddDestination(void*,void*,int32_t){}
void AssetMgr_LoadFile(void*,const char*,int32_t*){}
void*RESDATA_CreateSpriteObject(void*,int32_t,int32_t){return nullptr;}
void RESDATA_IsBuildingTile__pv(void*){}
void RESDATA_IsRoadTile__pv(void*){}
void RESDATA_IsBuildingTile__i(int32_t){}
void RESDATA_IsRoadTile__i(int32_t){}
void*DPLAY_CreatePlayer__strstr(void*,const char*,const char*){return nullptr;}
void DPLAY_RenderPlayer(void*,void*,int32_t,void*,int32_t,int32_t,uint32_t,RECT*){}
long double __ftol(double d){return static_cast<long double>(d);}

/* AudioChannel — C-linkage versions */
void AudioChannel_Pause_C(int32_t x){}
void AudioChannel_Play_C(int32_t x){}

/* PlaySound */
void PlaySound__i(int32_t){}

/* Ordinals */
void Ordinal_1__iPv(int32_t,void*){}
void Ordinal_1__PvS_S_S_(void*,void*,void*,void*){}
void Ordinal_2(void*){}
void Ordinal_4(void*,void**,void*,void*,void*){}

} /* end extern "C" */

/* =========================================================== */
/* B. C++-linkage overloaded stubs (native C++ overloading)     */
/*    Same function names, different params = correct mangling  */
/* =========================================================== */

/* AudioChannel — C++ overloads */
void AudioChannel_Pause(){}         /* _Z19AudioChannel_Pausev */
void AudioChannel_Pause(int32_t){}  /* _Z19AudioChannel_Pausei */
void AudioChannel_Play(){}          /* _Z19AudioChannel_Playv */
void AudioChannel_Play(int32_t){}   /* _Z19AudioChannel_Playi */

/* CGWND_SetMode — C++ overloads */
void CGWND_SetMode(int32_t){}       /* _Z13CGWND_SetModei */
void CGWND_SetMode(void*){}         /* _Z13CGWND_SetModePv */

/* Collection — these need to be member functions for correct vtable mangling */
/* We'll handle vtables separately */

/* Config_GetIniString — C++ overloads */
int32_t Config_GetIniString(void*,const char*,const char*,const char*,char*,int32_t){return 0;}
int32_t Config_GetIniString(void*,const char*,const char*,const char*,char*,uint32_t){return 0;}

/* Cursor_SetCapture — C++ overloads */
void Cursor_SetCapture(void*,int32_t){}        /* _Z17Cursor_SetCapturePvi */
void Cursor_SetCapture(GameWindow*,uint8_t){}  /* _Z17Cursor_SetCaptureP10GameWindowh */

/* Cursor_UnlockAllSurfaces — C++ overloads */
void Cursor_UnlockAllSurfaces(){}               /* _Z24Cursor_UnlockAllSurfacesv */
void Cursor_UnlockAllSurfaces(GameWindow*){}    /* _Z24Cursor_UnlockAllSurfacesP10GameWindow */

/* Cursor_InitSprites — C++ version with GameWindow* */
void Cursor_InitSprites(GameWindow*){}          /* _Z18Cursor_InitSpritesP10GameWindow */

/* FormatResourceString — C++ overloads */
void FormatResourceString(void*,int32_t,char*,int32_t){}   /* _Z20FormatResourceStringPviPci */
void FormatResourceString(void*,uint32_t,char*,int32_t){}  /* _Z20FormatResourceStringPvjPci */

/* GameAudio_AllocChannel — C++ overloads */
void GameAudio_AllocChannel(GameAudio*,int32_t,void*,int32_t,int32_t,uint32_t,uint32_t){}
void GameAudio_AllocChannel(void*,int32_t,void**,int32_t,int32_t,int32_t,int32_t){}

/* GameAudio_StopAll */
void GameAudio_StopAll(GameAudio*){}

/* ResourceManager_GetById overloads live in sdl3_shims/resource_manager_sdl3.cpp. */

/* RESMGR_LoadSoundResource — C++ overloads */
void RESMGR_LoadSoundResource(int32_t){}                   /* _Z24RESMGR_LoadSoundResourcei */
void RESMGR_LoadSoundResource(void*){}                     /* _Z24RESMGR_LoadSoundResourcePv */

/* RESMGR_ReleaseSoundResource — C++ overloads */
void RESMGR_ReleaseSoundResource(int32_t){}                /* _Z27RESMGR_ReleaseSoundResourcei */
void RESMGR_ReleaseSoundResource(void*){}                  /* _Z27RESMGR_ReleaseSoundResourcePv */

/* Sprite_Init — C++ overloads */
void Sprite_Init(void*,int32_t){}                          /* _Z11Sprite_InitPvi */
void Sprite_Init(void*,int32_t,void*){}                    /* _Z11Sprite_InitPviS_ */

/* Sprite_SetState — C++ overloads */
void Sprite_SetState(void*,int32_t){}                      /* _Z15Sprite_SetStatePvi */
void Sprite_SetState(void*,int32_t,void*){}                /* _Z15Sprite_SetStatePviS_ */

/* TileMap_InvalidateRect — C++ overloads */
void TileMap_InvalidateRect(void*,int32_t,int32_t,int32_t,int32_t){}     /* _Z22TileMap_InvalidateRectPviiii */
void TileMap_InvalidateRect(TileMap*,int32_t,int32_t,int32_t,int32_t){} /* _Z22TileMap_InvalidateRectP7TileMapiiii */

/* UIPANEL_BeginPaint — C++ overloads */
void UIPANEL_BeginPaint(int32_t){}       /* _Z18UIPANEL_BeginPainti */
void UIPANEL_BeginPaint(void*){}         /* _Z18UIPANEL_BeginPaintPv */

/* UIPANEL_Blit — C++ overloads */
void UIPANEL_Blit(void*,int32_t,int32_t,int32_t,int32_t,void*,int32_t,int32_t,int32_t,int32_t,uint8_t){}   /* _Z12UIPANEL_BlitPviiiiS_iiiih */
void UIPANEL_Blit(void*,int32_t,int32_t,int32_t,int32_t,void*,int32_t,int32_t,int32_t,int32_t,int32_t){}  /* _Z12UIPANEL_BlitPviiiiS_iiiii */
void UIPANEL_Blit(void*,int32_t,int32_t,int32_t,int32_t,void*,int32_t,int32_t,int32_t,int32_t,uint32_t){} /* _Z12UIPANEL_BlitPviiiiS_iiiij */
void UIPANEL_Blit(void*,uint32_t,uint32_t,int32_t,uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t){} /* _Z12UIPANEL_BlitPvjjijPPijjijj */

/* UIPANEL_EndPaintEx — C++ overloads */
void UIPANEL_EndPaintEx(void*,int32_t,int32_t,uint8_t,void*){}        /* _Z18UIPANEL_EndPaintExPviihS_ */
void UIPANEL_EndPaintEx(void*,void*,void*,uint8_t,void*){}            /* _Z18UIPANEL_EndPaintExPvS_S_hS_ */

/* UI_CreateChildWindow — C++ overloads */
void UI_CreateChildWindow(void*,int32_t,int32_t){}     /* _Z20UI_CreateChildWindowPvii */
void UI_CreateChildWindow(void*,uint32_t,int32_t){}    /* _Z20UI_CreateChildWindowPvji */

/* UI_CreateMessageBox — C++ overloads */
void*UI_CreateMessageBox(void*,int32_t,int32_t,char,int32_t,int32_t,int32_t){return nullptr;} /* _Z19UI_CreateMessageBoxPviiciii */
void*UI_CreateMessageBox(void*,int32_t,int32_t,char,int32_t,int32_t,char){return nullptr;} /* _Z19UI_CreateMessageBoxPviiciic */
void*UI_CreateMessageBox(void*,int32_t,int16_t,char,int32_t,int32_t,char){return nullptr;} /* _Z19UI_CreateMessageBoxPvisciic */

/* UI_CenterWindow — C++ overloads */
void UI_CenterWindow(RECT*,RECT*){}     /* _Z15UI_CenterWindowP4RECTS0_ */
void UI_CenterWindow(void*,void*){}     /* _Z15UI_CenterWindowPvS_ */

/* UI_WindowBase_BaseDtor — C++ overloads */
void UI_WindowBase_BaseDtor(void*){}            /* _Z22UI_WindowBase_BaseDtorPv */
void UI_WindowBase_BaseDtor(UI_WindowBase*){}   /* _Z22UI_WindowBase_BaseDtorP13UI_WindowBase */

/* UI_WindowBase_Ctor — C++ overloads */
void UI_WindowBase_Ctor(void*,void*,uint32_t){}           /* _Z18UI_WindowBase_CtorPvS_j */
void UI_WindowBase_Ctor(UI_WindowBase*,void*,uint32_t){}  /* _Z18UI_WindowBase_CtorP13UI_WindowBaseS_j */

/* Vehicle_FindPath — C++ overloads */
void Vehicle_FindPath(void*,int32_t*,char){}     /* _Z16Vehicle_FindPathPvPic */
void Vehicle_FindPath(void*,void*,uint8_t){}     /* _Z16Vehicle_FindPathPvS_h */

/* VehicleEditor_CheckEditBounds1 — C++ overloads */
void VehicleEditor_CheckEditBounds1(void*,void*){}            /* _Z31VehicleEditor_CheckEditBounds1PvS_ */
void VehicleEditor_CheckEditBounds1(VehicleEditor*,void*){}   /* _ZN13VehicleEditor16CheckEditBounds1EPv */

/* WIN32_StreamOpen — C++ overloads */
void*WIN32_StreamOpen(void*,int32_t){return nullptr;}                        /* _Z16WIN32_StreamOpenPvi */
void*WIN32_StreamOpen(void*,const char*,int32_t,void*,int32_t){return nullptr;} /* _Z16WIN32_StreamOpenPvPKciS_i */

/* WNDPROC_StreamFromMemory — C++ overloads */
void WNDPROC_StreamFromMemory(void*,const char*,int32_t,int32_t){}  /* _Z24WNDPROC_StreamFromMemoryPvPKcii */
void WNDPROC_StreamFromMemory(void*,char*,int32_t,int32_t){}        /* _Z24WNDPROC_StreamFromMemoryPvPcii */

/* RESDATA_IsBuildingTile / RESDATA_IsRoadTile — implemented in stubs_impl.cpp
 * (see the Ghidra-verified implementations there).  These C++ overloads
 * are now just extern declarations to avoid duplicate definitions. */
extern void RESDATA_IsBuildingTile(void*);
extern void RESDATA_IsBuildingTile(int32_t);
extern void RESDATA_IsRoadTile(void*);
extern void RESDATA_IsRoadTile(int32_t);

/* Resource_IsBuildingTile / Resource_IsRoadTile / Resource_IsValidTrackIndex */
void Resource_IsBuildingTile(void*){}    /* _Z22Resource_IsBuildingTilePv */
void Resource_IsRoadTile(void*){}        /* _Z18Resource_IsRoadTilePv */
void Resource_IsValidTrackIndex(void*, short){} /* _Z28Resource_IsValidTrackIndexPvs */

/* DPLAY_CreatePlayer — C++ overloads */
void*DPLAY_CreatePlayer(void*,const char*,const char*){return nullptr;} /* _Z18DPLAY_CreatePlayerPvPKcS1_ */

/* ButtonSprite_Ctor — C++ version */
void ButtonSprite_Ctor(void*,int32_t){}  /* _Z17ButtonSprite_CtorPvi */

/* =========================================================== */
/* C. Member function stubs (Class::method syntax)               */
/*    These produce correct mangled names for class methods     */
/* =========================================================== */

/* Building::Building(int) */
void Building_Building(Building*,int32_t){}  /* won't match _ZN8BuildingC1Ei */

/* TrainEntity::TrainEntity(int) */
void TrainEntity_TrainEntity(TrainEntity*,int32_t){}  /* won't match _ZN11TrainEntityC1Ei */

/* GameSetupPanel methods */
void GameSetupPanel_HandleMapClick(GameSetupPanel*,int32_t,int32_t){}
void GameSetupPanel_SelectLayoutEntry(GameSetupPanel*,int32_t){}
void GameSetupPanel_SendScenarioSelect(GameSetupPanel*,int32_t){}
void GameSetupPanel_ConnectToNetworkGame(GameSetupPanel*,int32_t){}

/* HelpWnd methods */
void HelpWnd_render_page(HelpWnd*,int32_t*){}
void HelpWnd_render_scroll_down(HelpWnd*,int32_t*){}
void HelpWnd_render_scroll_up(HelpWnd*,int32_t*){}
void HelpWnd_update_anim_sprite(HelpWnd*,int32_t){}

/* VehicleEditor methods */
void VehicleEditor_CheckEdgeBounds(VehicleEditor*,void*){}
void VehicleEditor_CheckEditBounds2(VehicleEditor*,void*){}
void VehicleEditor_CheckVehicleAttach(VehicleEditor*,void*){}
void VehicleEditor_CalcAngle(VehicleEditor*){}

/* RESDATA_ScriptedObject::EnterBuildMode */
void RESDATA_ScriptedObject_EnterBuildMode(RESDATA_ScriptedObject*,uint8_t){}

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
void* g_font_normal = nullptr; int32_t g_is_fullscreen = 0; void* g_primary_surface = nullptr;
int32_t g_surface_bpp = 0; int32_t g_surface_bshift = 0;
int32_t g_surface_channel1 = 0; int32_t g_surface_channel2 = 0; int32_t g_surface_lost = 0;
int32_t g_pixel_format_mask = 0; int32_t g_viewport_rect_left = 0; int32_t g_viewport_rect_top = 0;
int32_t g_viewport_rect_right = 800; int32_t g_viewport_rect_bottom = 600;
int32_t g_window_left = 0; int32_t g_window_top = 0; int32_t g_listener_x = 0; int32_t g_listener_y = 0;
void* _g_train = nullptr; void* DAT_004a97a0 = nullptr; void* DAT_004a9994 = nullptr;
void* g_click_on_town = nullptr; void* g_click_on_building = nullptr; void* g_cgwnd = nullptr;
int32_t g_clean_exit = 0; void* g_client_rect = nullptr; void* _g_cursor_surface = nullptr;
void* g_frame_event = nullptr; void* g_fullscreen_rect = nullptr; void* g_game_config = nullptr;
int32_t g_game_difficulty = 0; int32_t g_in_build_mode = 0; uint8_t g_is_town_mode = 0;
void* g_nameEntryPanel = nullptr; void* g_netSettings = nullptr; void* _g_network_queue = nullptr;
void* g_network_queue = nullptr; void* _g_network_thread = nullptr; void* g_network_thread = nullptr;
void* g_pixel_data_cache = nullptr; int32_t g_placement_resource_id = 0; int32_t g_ref_count = 0;
void* g_resource_mgr = nullptr; int32_t g_road_build_mode = 0; int32_t g_screen_bpp = 16;
int32_t g_screen_center_x = 400; int32_t g_screen_center_y = 300; void* g_script_engine = nullptr;
int32_t g_show_scrollbars = 0; int32_t g_timer_event_id = 0; int32_t g_timer_id = 0;
void* g_title_font = nullptr; void* g_trackSegmentOffsets = nullptr; void* g_train = nullptr;
void* _g_train_resources = nullptr; void* g_train_resources = nullptr; int32_t g_window_bottom = 600;
int32_t g_window_mode = 0; int32_t g_window_right = 800; void* g_world = nullptr; void* g_about = nullptr;
void* g_asset_archive = nullptr; void* g_asset_base_path = nullptr; void* _g_audio_config = nullptr;
void* _g_backbuffer = nullptr; int32_t g_build_mode = 0; void* _g_dplay = nullptr;
void* _g_dplay_config = nullptr; void* _g_dsound_object = nullptr;
int32_t DAT_0047e0f4=0;int32_t DAT_0047e220=0;int32_t DAT_0047e224=0;
int32_t _DAT_00481170=0;int32_t DAT_00481218=0;void* g_world_release_a=nullptr;
void* g_world_release_b=nullptr;int32_t DAT_004aad34=0;int32_t DAT_004aad38=0;
int32_t _DAT_004fd3a8=0;int32_t s_BALANCING_0047e164=0;int32_t s_CleanExit_0047e128=0;
int32_t s_LEGO_LOCO_0047e1c0=0;int32_t s_measure_test_char=0;
int32_t s_MinBuildingFPS_0047e154=0;int32_t s_MinFlyingFPS_0047e134=0;
int32_t s_MinMinifigFPS_0047e144=0;int32_t s_MinVehicleFPS_0047e170=0;
int32_t s_PROCESS_0047e120=0;int32_t s_RectBottom_0047e180=0;
int32_t s_RectLeft_0047e1b4=0;int32_t s_RectRight_0047e18c=0;
int32_t s_RectTop_0047e198=0;
int32_t s_StringFileInfo_080904B0_FileVer_0047e0f8=0;
int32_t s_WINDOW_ATTRIBUTES_0047e1a0=0;
}

#ifndef _WIN32
bool DDRAW_Init(void) { return true; }
#endif
