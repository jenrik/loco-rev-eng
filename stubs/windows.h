/**
 * stubs/windows.h — Minimal Windows API function declarations for compilation
 *
 * Types/constants live in stubs/windows_types.h (split out 2026-08-10 —
 * see that file's header comment for why). This file adds the Win32 API
 * *function* stub declarations on top; most translation units should keep
 * including this one exactly as before. Only headers that need Win32
 * *types* without the function surface (e.g. stubs/dplay.h) should include
 * windows_types.h directly instead.
 *
 * This provides the subset of Windows types and macros needed by the
 * Lego Loco decompiled C++ code. It does NOT provide implementations —
 * only type definitions sufficient for syntax checking and object-file
 * generation.
 *
 * When the real Windows SDK (via mingw-w64 or MSVC) is available, remove
 * this stub and use the real <windows.h>.
 */

#ifndef STUBS_WINDOWS_H
#define STUBS_WINDOWS_H

#include <cstdarg>  /* va_list — used by wvsprintfA() below. Previously relied
                      * on a transitive include from whichever translation
                      * unit got here first; standalone compiles without one
                      * failed (found via tests/win32_stream_file_test.cpp,
                      * 2026-08-10 — it had never actually been compiled
                      * before, since it was never registered as a meson
                      * test() target until the same session). */

#include "windows_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* Stub function declarations (linker will fail but compiler is happy)  */
/* ATOM is declared in windows_types.h.                                */
/* ================================================================== */

HWND      CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void*);
HWND      GetDesktopWindow(void);
ATOM      RegisterClassA(const WNDCLASSA*);
ATOM      RegisterClassExA(const WNDCLASSEXA*);
BOOL      GetClientRect(HWND, RECT*);
HICON     LoadIconA(HINSTANCE, LPCSTR);
BOOL      ShowWindow(HWND, int);
int       GetSystemMetrics(int);
HDC       GetDC(HWND);
int       GetDeviceCaps(HDC, int);
int       ReleaseDC(HWND, HDC);
int       MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
BOOL      SetWindowPos(HWND, HWND, int, int, int, int, UINT);
BOOL      DestroyWindow(HWND);
BOOL      UpdateWindow(HWND);
BOOL      InvalidateRect(HWND, const RECT*, BOOL);
BOOL      ValidateRect(HWND, const RECT*);
BOOL      GetWindowRect(HWND, RECT*);
BOOL      SetRect(RECT*, int, int, int, int);
BOOL      OffsetRect(RECT*, int, int);
BOOL      IntersectRect(RECT*, const RECT*, const RECT*);
BOOL      UnionRect(RECT*, const RECT*, const RECT*);
BOOL      PtInRect(const RECT*, POINT);
BOOL      IsRectEmpty(const RECT*);
BOOL      SetCursorPos(int, int);
int       ShowCursor(int);
HWND      SetCapture(HWND);
BOOL      ReleaseCapture(void);
HWND      SetFocus(HWND);
BOOL      GetCursorPos(POINT*);
BOOL      ScreenToClient(HWND, POINT*);
BOOL      ClientToScreen(HWND, POINT*);
LRESULT   DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
LRESULT   SendMessageA(HWND, UINT, WPARAM, LPARAM);
BOOL      PostMessageA(HWND, UINT, WPARAM, LPARAM);
BOOL      PeekMessageA(MSG*, HWND, UINT, UINT, UINT);
BOOL      GetMessageA(MSG*, HWND, UINT, UINT);
BOOL      TranslateMessage(const MSG*);
LRESULT   DispatchMessageA(const MSG*);
void      PostQuitMessage(int);
BOOL      KillTimer(HWND, UINT_PTR);
UINT_PTR  SetTimer(HWND, UINT_PTR, UINT, void (*)(HWND, UINT, UINT_PTR, DWORD));
DWORD     GetTickCount(void);
void      Sleep(DWORD);
void      ExitProcess(UINT);
DWORD     GetCurrentThreadId(void);
HANDLE    CreateThread(void*, size_t, LPTHREAD_START_ROUTINE, void*, DWORD, DWORD*);
DWORD     WaitForSingleObject(HANDLE, DWORD);
BOOL      CloseHandle(HANDLE);

DWORD     GetModuleFileNameA(HMODULE, char*, DWORD);
HMODULE   GetModuleHandleA(const char*);
HINSTANCE GetModuleHandleW(const wchar_t*);
int       LoadStringA(HINSTANCE, UINT, char*, int);
BOOL      PlaySoundA(const char*, HMODULE, DWORD);

HANDLE    CreateFileA(const char*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
BOOL      ReadFile(HANDLE, void*, DWORD, DWORD*, void*);
BOOL      WriteFile(HANDLE, void*, DWORD, DWORD*, void*);
DWORD     SetFilePointer(HANDLE, LONG, LONG*, DWORD);
DWORD     GetFileSize(HANDLE, DWORD*);
BOOL      DeleteFileA(const char*);

DWORD     GetUserNameA(char*, DWORD*);

/* GDI */
HBRUSH    CreateSolidBrush(COLORREF);
BOOL      DeleteObject(HGDIOBJ);
BOOL      TextOutA(HDC, int, int, LPCSTR, int);
HFONT     CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR);
HGDIOBJ   SelectObject(HDC, HGDIOBJ);
COLORREF  SetTextColor(HDC, COLORREF);
COLORREF  SetBkColor(HDC, COLORREF);
int       SetBkMode(HDC, int);
BOOL      GetTextExtentPoint32A(HDC, LPCSTR, int, SIZE*);

/* CRT equivalents (for stubbing when CRT is static).
 * operator_new(size_t)/GLOBAL_free(void*) are NOT declared here: they are
 * this project's own custom allocator hooks (0x465CE0/0x465CD0), not real
 * Win32 APIs, and every .cpp call site in the tree already declares them
 * with C++ linkage (matching their real definition in
 * shared/stubs_impl.cpp, also C++ linkage) — a C-linkage declaration here
 * conflicted with all of those the moment any file transitively included
 * both this header and one of those declarations in the same TU (hit
 * while converting ui/HelpWnd.cpp and game/ScriptedObject.cpp to use
 * resources/Win32Stream.h's real WIN32_Stream class). */
void*     CRT_malloc(size_t);
void      CRT_free(void*);
void*     CRT_calloc(size_t, size_t);
void*     CRT_realloc(void*, size_t);

/* Missing from some headers */
int       wsprintfA(char*, const char*, ...);
int       wvsprintfA(char*, const char*, va_list);

#ifdef __cplusplus
}
#endif

#endif /* STUBS_WINDOWS_H */
