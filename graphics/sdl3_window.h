/**
 * sdl3_window.h — Win32 windowing → SDL3 compatibility shim
 *
 * Implements the Win32 USER32.dll and GDI32.dll APIs that Lego Loco
 * uses for window creation, message pumping, and basic GDI drawing.
 * Backed by SDL3 windows and renderers.
 *
 * APIs are grouped by implementation depth:
 *   FULL    — real SDL3-backed implementation
 *   STUB    — returns success but no-ops (GDI drawing handled by DirectDraw)
 *   HELPER  — simple C implementation (string helpers, etc.)
 *
 * NOT part of the Lego Loco reverse-engineering project.
 */

#ifndef LOCO_SDL3_WINDOW_H
#define LOCO_SDL3_WINDOW_H

#include "sdl3_types.h"
#include <SDL3/SDL.h>
#include <cstdint>

#ifndef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

/* Windows types: see sdl3_types.h (guarded against compat.h/types.h) */

/* Timer callback type */
typedef void (*TIMERPROC)(HWND, UINT, uintptr_t, DWORD);
typedef void* HLOCAL;

/* Window style constants */
#define WS_OVERLAPPEDWINDOW  0x00CF0000
#define WS_VISIBLE           0x10000000
#define WS_CHILD             0x40000000
#define WS_POPUP             0x80000000
#define WS_EX_TOPMOST        0x00000008
#define WS_EX_APPWINDOW      0x00040000

/* ShowWindow */
#define SW_SHOW              5
#define SW_HIDE              0
#define SW_SHOWNORMAL        1

/* SetWindowPos */
#define HWND_TOP             0
#define SWP_NOSIZE           0x0001
#define SWP_NOMOVE           0x0002
#define SWP_NOZORDER         0x0004

/* GetSystemMetrics */
#define SM_CXSCREEN          0
#define SM_CYSCREEN          1
#define SM_CXFULLSCREEN      16
#define SM_CYFULLSCREEN      17

/* Messages */
#define WM_PAINT             0x000F
#define WM_TIMER             0x0113
#define WM_CLOSE             0x0010
#define WM_QUIT              0x0012
#define WM_LBUTTONDOWN       0x0201
#define WM_LBUTTONUP         0x0202
#define WM_RBUTTONDOWN       0x0204
#define WM_RBUTTONUP         0x0205
#define WM_MOUSEMOVE         0x0200
#define WM_KEYDOWN           0x0100
#define WM_KEYUP             0x0101
#define PM_REMOVE            0x0001

/* GDI */
#define SRCCOPY              0x00CC0020
#define TRANSPARENT          1
#define OPAQUE               2

/* Window long offsets */
#define GWL_USERDATA         0

/* Timer */
#define TIMERPROC_FN          void (*)(HWND, UINT, uintptr_t, DWORD)

/* MessageBox */
#define MB_OK                0x00000000

/* Registry */
#ifndef HKEY_LOCAL_MACHINE
#define HKEY_LOCAL_MACHINE   0x80000002
#endif
#ifndef HKEY_CURRENT_USER
#define HKEY_CURRENT_USER    0x80000001
#endif
#define KEY_READ             0x20019
#define KEY_WRITE            0x20006
#define REG_SZ               1
#define REG_DWORD            4
#define ERROR_SUCCESS        0

/* WNDCLASSA/WNDCLASSEXA: provided by shared/types.h */

struct MSG {
    void*         hwnd;
    unsigned int  message;
    unsigned int  wParam;
    int           lParam;
    unsigned int  time;
    POINT         pt;
};

struct PAINTSTRUCT {
    void*         hdc;
    int           fErase;
    RECT          rcPaint;
    int           fRestore;
    int           fIncUpdate;
    unsigned char rgbReserved[32];
};

/* RECT/POINT/MSG/PAINTSTRUCT: see sdl3_types.h */

/* =========================================================================
 * Window creation and management (FULL implementation)
 * ========================================================================= */

ATOM  RegisterClassA(const WNDCLASSA* lpWndClass);
ATOM  RegisterClassExA(const WNDCLASSEXA* lpWndClass);
HWND  CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
                      DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                      HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
                      void* lpParam);
BOOL  ShowWindow(HWND hWnd, int nCmdShow);
BOOL  UpdateWindow(HWND hWnd);
BOOL  SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y,
                   int cx, int cy, UINT uFlags);
BOOL  GetClientRect(HWND hWnd, RECT* lpRect);
int   GetSystemMetrics(int nIndex);
BOOL  EnableWindow(HWND hWnd, BOOL bEnable);
BOOL  InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
BOOL  SetWindowTextA(HWND hWnd, LPCSTR lpString);
HWND  GetDesktopWindow(void);
HWND  FindWindowA(LPCSTR lpClassName, LPCSTR lpWindowName);
LONG  SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong);
LONG  GetWindowLongA(HWND hWnd, int nIndex);
BOOL  SetScrollRange(HWND hWnd, int nBar, int nMinPos, int nMaxPos, BOOL bRedraw);
int   SetScrollPos(HWND hWnd, int nBar, int nPos, BOOL bRedraw);
BOOL  ShowScrollBar(HWND hWnd, int wBar, BOOL bShow);
BOOL  AdjustWindowRect(RECT* lpRect, DWORD dwStyle, BOOL bMenu);

/* =========================================================================
 * Message loop (FULL implementation)
 * ========================================================================= */

BOOL  PeekMessageA(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin,
                   UINT wMsgFilterMax, UINT wRemoveMsg);
BOOL  GetMessageA(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin,
                  UINT wMsgFilterMax);
BOOL  TranslateMessage(const MSG* lpMsg);
LONG  DispatchMessageA(const MSG* lpMsg);
BOOL  PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

/* =========================================================================
 * Modal dialogs with child controls (STUB — no host equivalent yet; this
 * project has no native SDL3 dialog/control-widget system built. Callers
 * currently reachable through this path are dead code (see
 * network/DirectPlay.cpp's DirectPlay_ChooseConnectionDlgProc, which
 * nothing invokes yet), so a safe no-op with a one-time warning is used
 * instead of the loud-assert stub policy reserved for deferred internal
 * game logic. Revisit if/when a real caller makes this path reachable.
 * ========================================================================= */

int32_t DialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName,
                         HWND hWndParent, void* lpDialogFunc, LPARAM dwInitParam);
LRESULT SendDlgItemMessageA(HWND hDlg, int nIDDlgItem, UINT Msg,
                            WPARAM wParam, LPARAM lParam);
BOOL    EndDialog(HWND hDlg, int32_t nResult);
HWND    GetDlgItem(HWND hDlg, int nIDDlgItem);

/* =========================================================================
 * GDI (STUB — drawing is done via DirectDraw/SDL renderer)
 * ========================================================================= */

HDC   GetDC(HWND hWnd);
int   ReleaseDC(HWND hWnd, HDC hDC);
HDC   BeginPaint(HWND hWnd, PAINTSTRUCT* lpPaint);
BOOL  EndPaint(HWND hWnd, const PAINTSTRUCT* lpPaint);
BOOL  BitBlt(HDC hdc, int x, int y, int cx, int cy,
             HDC hdcSrc, int x1, int y1, DWORD rop);
BOOL  StretchBlt(HDC hdcDest, int xDest, int yDest, int wDest, int hDest,
                 HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc,
                 DWORD rop);
HDC   CreateCompatibleDC(HDC hdc);
BOOL  DeleteDC(HDC hdc);
void* SelectObject(HDC hdc, void* hgdiobj);
BOOL  DeleteObject(void* ho);
HBRUSH CreateSolidBrush(COLORREF color);
COLORREF SetTextColor(HDC hdc, COLORREF color);
int   SetBkMode(HDC hdc, int mode);
int   DrawTextA(HDC hdc, LPCSTR lpchText, int cchText,
                RECT* lprc, UINT format);
int   FillRect(HDC hdc, const RECT* lprc, HBRUSH hbr);
void  SetRect(RECT* lprc, int left, int top, int right, int bottom);
void  SetRectEmpty(RECT* lprc);
BOOL  PtInRect(const RECT* lprc, POINT pt);

/* =========================================================================
 * Additional window/message functions
 * ========================================================================= */

BOOL  DestroyWindow(HWND hWnd);
void  PostQuitMessage(int nExitCode);
BOOL  ClientToScreen(HWND hWnd, POINT* lpPoint);
void  GetCursorPos(POINT* lpPoint);
void  OutputDebugStringA(const char* lpOutputString);
BOOL  CopyRect(RECT* lprcDst, const RECT* lprcSrc);
BOOL  OffsetRect(RECT* lprc, int dx, int dy);
int   UnionRect(RECT* lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);
BOOL  IntersectRect(RECT* lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);

/* =========================================================================
 * GDI drawing extensions
 * ========================================================================= */

HWND     SetFocus(HWND hWnd);
BOOL     SetForegroundWindow(HWND hWnd);
HGDIOBJ  GetStockObject(int fnObject);
BOOL    DrawEdge(HDC hdc, RECT* qrc, UINT edge, UINT grfFlags);
COLORREF SetBkColor(HDC hdc, COLORREF color);

/* =========================================================================
 * File I/O (stub — use POSIX internally)
 * ========================================================================= */

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   void* lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
DWORD  GetFileSize(HANDLE hFile, DWORD* lpFileSizeHigh);
BOOL   CloseHandle(HANDLE hObject);
DWORD  GetLastError(void);
DWORD  FormatMessageA(DWORD dwFlags, const void* lpSource, DWORD dwMessageId,
                      DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, void* Arguments);
void*  LocalFree(void* hMem);
BOOL   GetOpenFileNameA(void* lpofn);

/* =========================================================================
 * Process / thread
 * ========================================================================= */

void   Sleep(DWORD dwMilliseconds);
void   ExitProcess(UINT uExitCode);

/* =========================================================================
 * Cursor (STUB — cursor managed by SDL3)
 * ========================================================================= */

HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);
HICON   LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
void    SetCursor(HCURSOR hCursor);

/* =========================================================================
 * Timer (FULL implementation via SDL_AddTimer)
 * ========================================================================= */

uintptr_t SetTimer(HWND hWnd, uintptr_t nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc);
BOOL      KillTimer(HWND hWnd, uintptr_t uIDEvent);

/* =========================================================================
 * Dialog / message box (FULL implementation via SDL_ShowSimpleMessageBox)
 * ========================================================================= */

int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);

/* =========================================================================
 * String helpers (HELPER — standard C implementations)
 * ========================================================================= */

int   wsprintfA(char* buf, const char* fmt, ...);
int   lstrlenA(LPCSTR lpString);
char* lstrcpyA(char* dst, LPCSTR src);
char* lstrcatA(char* dst, LPCSTR src);
int   LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax);

/* =========================================================================
 * System (STUB — returns success)
 * ========================================================================= */

HRESULT CoInitializeEx(void* pvReserved, DWORD dwCoInit);
void    CoUninitialize(void);
DWORD   GetFileVersionInfoSizeA(LPCSTR file, DWORD* handle);
BOOL    GetFileVersionInfoA(LPCSTR file, DWORD handle, DWORD len, void* data);
BOOL    VerQueryValueA(void* block, LPCSTR subBlock, void** buffer, UINT* len);
BOOL    PlaySoundA(LPCSTR pszSound, HMODULE hmod, DWORD fdwSound);

/* =========================================================================
 * Registry (STUB — simple filesystem-backed or hardcoded)
 * ========================================================================= */

LONG RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions,
                   DWORD samDesired, HKEY* phkResult);
LONG RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, DWORD* lpReserved,
                      DWORD* lpType, uint8_t* lpData, DWORD* lpcbData);
LONG RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved,
                     LPCSTR lpClass, DWORD dwOptions, DWORD samDesired,
                     void* lpSecurityAttributes, HKEY* phkResult,
                     DWORD* lpdwDisposition);
LONG RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved,
                    DWORD dwType, const uint8_t* lpData, DWORD cbData);
LONG RegCloseKey(HKEY hKey);

/* =========================================================================
 * SDL3 lifecycle — called by our port of WinMain
 * ========================================================================= */

/**
 * Initialize the SDL3 window shim. Creates the SDL window and renderer.
 * Must be called before any window API. Returns 0 on success.
 */
int  SDL3_WindowInit(const char* title, int width, int height);

/**
 * Shut down the SDL3 window shim. Destroys window and renderer.
 */
void SDL3_WindowQuit(void);

/**
 * Get the global SDL_Renderer for use by the DirectDraw shim.
 */
SDL_Renderer* SDL3_GetRenderer(void);

/**
 * Get the global SDL_Window.
 */
SDL_Window* SDL3_GetWindow(void);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* LOCO_SDL3_WINDOW_H */
