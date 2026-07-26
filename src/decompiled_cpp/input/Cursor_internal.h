// Status: TRANSCRIBED
/* Cursor_internal.h — Shared internals for Cursor implementation files */
#pragma once

#include "Cursor.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* Win32 API declarations — only needed on real Windows.
 * On non-Windows, sdl3_window.h provides equivalent declarations. */
#ifdef _WIN32
extern "C" {
HBRUSH CreateSolidBrush(uint32_t);
BOOL DeleteObject(HGDIOBJ);
HWND GetDesktopWindow(void);
BOOL GetClientRect(HWND, RECT*);
HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void*);
HWND GetCapture(void);
HWND SetCapture(HWND);
BOOL ReleaseCapture(void);
typedef void (*TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
typedef void* HLOCAL;

int ShowCursor(BOOL);
BOOL ShowWindow(HWND, int);
BOOL DestroyWindow(HWND);
void PostQuitMessage(int);
UINT_PTR SetTimer(HWND, UINT_PTR, UINT, TIMERPROC);
BOOL KillTimer(HWND, UINT_PTR);
void PostMessageA(HWND, UINT, WPARAM, LPARAM);
void GetCursorPos(POINT*);
LONG SetWindowLongA(HWND, int, LONG);
HWND SetFocus(HWND);
void SetWindowTextA(HWND, const char*);
void OutputDebugStringA(const char*);
BOOL ClientToScreen(HWND, POINT*);
BOOL SetRect(RECT*, int, int, int, int);
BOOL CopyRect(RECT*, const RECT*);
BOOL OffsetRect(RECT*, int, int);
int UnionRect(RECT*, const RECT*, const RECT*);
BOOL IntersectRect(RECT*, const RECT*, const RECT*);
BOOL PtInRect(const RECT*, POINT);
void Sleep(DWORD);
void ExitProcess(UINT);
BOOL EnableWindow(HWND, BOOL);
BOOL FillRect(HDC, const RECT*, HBRUSH);
int DrawTextA(HDC, LPCSTR, int, RECT*, UINT);
BOOL DrawEdge(HDC, RECT*, UINT, UINT);
int MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
BOOL GetOpenFileNameA(void*);
HANDLE CreateFileA(LPCSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
DWORD GetFileSize(HANDLE, DWORD*);
BOOL CloseHandle(HANDLE);
DWORD GetLastError(void);
DWORD FormatMessageA(DWORD, const void*, DWORD, DWORD, LPSTR, DWORD, void*);
HLOCAL LocalFree(HLOCAL);
HGDIOBJ GetStockObject(int);
HGDIOBJ SelectObject(HDC, HGDIOBJ);
COLORREF SetTextColor(HDC, COLORREF);
int SetBkMode(HDC, int);
COLORREF SetBkColor(HDC, COLORREF);
int wsprintfA(char*, const char*, ...);
HICON LoadIconA(HINSTANCE, LPCSTR);
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
}
#endif /* _WIN32 */

#ifndef _WIN32
/* Non-Windows: sdl3_window.h provides SDL3-backed equivalents of the above */
#include "sdl3_window.h"
#endif /* !_WIN32 */

/* Game-specific and CRT function declarations (all platforms) */

/* C++ linkage — these are C++ symbols in the binary, not C functions */
void* operator_new(size_t);
void GLOBAL_free(void*);
int CRT_atoi(const char*);
int CRT_rand(void);
void CRT_free(void*);

/* ================================================================ */
/* C-linkage: Win32 API, DirectX/DirectDraw/DirectPlay/CRT helpers  */
/*   These are genuine C-linkage symbols from the binary.            */
/* ================================================================ */
extern "C" {
size_t strlen(const char*);
void* memcpy(void*, const void*, size_t);
void* ResourceManager_GetById(void*, int);
void Sprite_Init(void*);
void Sprite_Destroy(void*);
void Sprite_SetState(void*, int, int*);
int UI_CreateFullWindow(void*, int, HWND, int, int, int, int, HMENU, HICON, UINT);
void DDRAW_UnlockPrimary(HWND);
int DDRAW_SetSurfaceFormat(void*, int);
int DDRAW_RestoreSurfaces(void*, void*);
void DDRAW_GetSurfaceWidthHeight(void*);
void UIPANEL_Blit(void* surf, int32_t sx, int32_t sy, int32_t sw, int32_t sh,
                   int32_t dx, int32_t dy, int32_t extra, int32_t dw, int32_t dh, int32_t flags);
HDC UIPANEL_BeginPaint(void*);
void UIPANEL_EndPaint(void*);
void UIPANEL_EndPaintEx(void*, HWND, int, uint8_t, RECT*);
void* UIPANEL_CreateSurface(void*);
void UIPANEL_InitSurface(void*, int, int, int, uint32_t, uint8_t);
void UIPANEL_UnlockSurface(void*);
void FormatResourceString(void*, UINT, LPSTR, int);
void DPLAY_EnumeratePlayers(void);
void DPLAY_LeaveSession(void*);
void* NET_GetOrCreateSurface(void*, uint8_t, uint8_t, uint8_t, uint8_t);
int NET_FindPlayer(int, int);
uint16_t NET_UploadAsset(int, char*);
void* CRT_wcsstr(const uint8_t*, const uint16_t*);
void WIN32_FatalError(void);
void WIN32_PostQuit(void);
void PlaySound(int);
void INPUT_SwitchToLocomotiveTab(void*, int);
void PlaySoundAt(int, int, int, int);
void PlaySoundFile(char*, int, int, int);
int HelpWnd_PlayNarration(void*, int, int);
int* AssetMgr_LoadFile(void*, uint8_t*, int*);
int* WIN32_StreamOpenFile(void*, char*, uint32_t, uint32_t, uint32_t);
int WIN32_StreamRead(void*, void*, int);
void Game_SetScreenMode(void*, uint8_t, uint8_t, uint8_t);
void CGWND_PumpMessages(void*);
int DPLAY_CreatePlayer(void* record);
void DPLAY_RenderPlayer(void* dplay, int hdcVal, void* player,
                         void* surface, int x, int y, int w, int* h);
void* WNDPROC_StreamFromMemory(void* stream, char* data, int size, int mode);
}

/* ================================================================ */
/* C++ linkage — game helpers originally compiled as C++ symbols     */
/*   NOTE: These are transitional bridge declarations. The eventual  */
/*   intent is to replace them with direct C++ method calls.         */
/* ================================================================ */
void UI_WindowBase_BaseDtor(void*);
void UI_WindowBase_Hide(void*);
void UI_WindowBase_Show(void*);

/* Globals */
extern Cursor* g_cursor; extern void* g_resmgr; extern void* g_asset_mgr;
extern char g_install_path[]; extern void* g_ddraw; extern void* _g_backbuffer;
extern void* _g_primary_surface; extern void* _g_dplay; extern int _g_cursor_refcount;
extern void* _g_cursor_back; extern void* g_netman; extern char g_empty_string[];
extern void* g_game; extern void* g_player_config; extern uint8_t g_is_fullscreen;
extern int32_t g_client_width; extern int32_t g_client_height; extern int32_t g_viewport_x;
extern int32_t g_viewport_y; extern void* g_town; extern void* g_postcard;
extern void* g_postcard_send; extern void* g_ui_main; extern void* g_main_window;
extern void* g_font_small; extern int g_surface_bpp; extern void* g_audio_mgr;

/* Shared internal functions */
void Cursor_UnlockAllSurfaces(void);

/* Surface vtable function pointer types (DirectDraw ABI — __stdcall convention) */
typedef int (__stdcall *SurfaceBlt_t)(void*, RECT*, void*, RECT*, uint32_t, void*);
typedef int (__stdcall *SurfacePollBlit_t)(void*, void*);
typedef void* (__stdcall *ResdataGetSurface_t)(void*, int, int);
typedef void (__stdcall *ResdataReleaseSurface_t)(void*);
typedef int (__stdcall *DDrawCreateSurface_t)(void*, int*, void**, int);
#define BLIT_WAIT 0x1000000
#define BLIT_KEYSRC 0x8000
#define BLIT_KEYSRC_WAIT (BLIT_KEYSRC | BLIT_WAIT)

