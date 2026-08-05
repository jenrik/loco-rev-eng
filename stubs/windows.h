/**
 * stubs/windows.h — Minimal Windows API type definitions for compilation
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

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* Basic Windows types                                                 */
/* Skipped if types.h already defined them (LOCO_TYPES_DEFINED).       */
/* ================================================================== */

#ifndef LOCO_TYPES_DEFINED
typedef int                 BOOL;
typedef uint8_t             BYTE;
typedef uint16_t            WORD;
typedef uint32_t            DWORD;
typedef uint32_t            UINT;
#endif
typedef uint8_t             byte;
typedef int32_t             LONG;
typedef int32_t             INT;
typedef uint32_t            ULONG;
typedef void*               LPVOID;
typedef const char*         LPCSTR;
typedef const wchar_t*      LPCWSTR;
typedef char*               LPSTR;
typedef wchar_t*            LPWSTR;
typedef const void*         LPCVOID;

typedef void*               HANDLE;
typedef HANDLE              HWND;
typedef HANDLE              HINSTANCE;
typedef HANDLE              HMODULE;
typedef HANDLE              HMENU;
typedef HANDLE              HDC;
typedef HANDLE              HICON;
typedef HANDLE              HBRUSH;
typedef HANDLE              HCURSOR;
typedef HANDLE              HFONT;
typedef HANDLE              HPALETTE;
typedef HANDLE              HBITMAP;
typedef HANDLE              HRGN;
typedef HANDLE              HGDIOBJ;

typedef uint32_t            UINT_PTR;
typedef int32_t             LONG_PTR;
typedef uint32_t            WPARAM;
typedef int32_t             LPARAM;
typedef int32_t             LRESULT;

/* WCHAR / TCHAR */
typedef wchar_t             WCHAR;
#ifndef _TCHAR_DEFINED
typedef wchar_t             TCHAR;
#define _TCHAR_DEFINED
#endif

/* ================================================================== */
/* Common Win32 typedefs (not in base windows.h but used in code)      */
/* ================================================================== */

typedef unsigned int        uint;
typedef unsigned short      ushort;
typedef unsigned char       uchar;
typedef void*               PVOID;

/* ================================================================== */
/* Structures                                                          */
/* ================================================================== */

#ifndef LOCO_TYPES_DEFINED
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *LPRECT;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *LPPOINT;

typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *LPSIZE;
#endif

#ifndef LOCO_TYPES_DEFINED
typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
} MSG, *LPMSG;

typedef struct tagWNDCLASSA {
    UINT      style;
    LRESULT (*lpfnWndProc)(HWND, UINT, WPARAM, LPARAM);
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR    lpszMenuName;
    LPCSTR    lpszClassName;
} WNDCLASSA, *LPWNDCLASSA;

typedef struct tagWNDCLASSEXA {
    UINT      cbSize;
    UINT      style;
    LRESULT (*lpfnWndProc)(HWND, UINT, WPARAM, LPARAM);
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR    lpszMenuName;
    LPCSTR    lpszClassName;
    HICON     hIconSm;
} WNDCLASSEXA, *LPWNDCLASSEXA;
#else
typedef struct tagMSG MSG;
typedef MSG* LPMSG;
typedef struct WNDCLASSA* LPWNDCLASSA;
typedef struct WNDCLASSEXA* LPWNDCLASSEXA;
#endif

/* ================================================================== */
/* GUID structure                                                      */
/* ================================================================== */

typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID;

/* ================================================================== */
/* Calling conventions                                                 */
/* ================================================================== */

#define WINAPI      __stdcall
#define CALLBACK    __stdcall
#define CDECL       __cdecl
#define STDMETHODCALLTYPE __stdcall
#define APIENTRY    WINAPI

/* ================================================================== */
/* Window / message constants                                          */
/* ================================================================== */

#define FALSE   0
#define TRUE    1
#ifndef NULL
#define NULL    0
#endif

#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_SHOWMINIMIZED    2
#define SW_SHOWMAXIMIZED    3
#define SW_SHOWNOACTIVATE   4
#define SW_SHOW             5
#define SW_MINIMIZE         6
#define SW_SHOWMINNOACTIVE  7
#define SW_SHOWNA           8
#define SW_RESTORE          9

#define SM_CXSCREEN         0
#define SM_CYSCREEN         1

#define WS_OVERLAPPED       0x00000000
#define WS_POPUP            0x80000000
#define WS_CHILD            0x40000000
#define WS_MINIMIZE         0x20000000
#define WS_VISIBLE          0x10000000
#define WS_DISABLED         0x08000000
#define WS_CLIPSIBLINGS     0x04000000
#define WS_CLIPCHILDREN     0x02000000
#define WS_MAXIMIZE         0x01000000
#define WS_CAPTION          0x00C00000
#define WS_BORDER           0x00800000
#define WS_DLGFRAME         0x00400000
#define WS_VSCROLL          0x00200000
#define WS_HSCROLL          0x00100000
#define WS_SYSMENU          0x00080000
#define WS_THICKFRAME       0x00040000
#define WS_MINIMIZEBOX      0x00020000
#define WS_MAXIMIZEBOX      0x00010000
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUPWINDOW      (WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_EX_TOPMOST       0x00000008
#define WS_EX_APPWINDOW     0x00040000
#define WS_EX_TOOLWINDOW    0x00000080
#define WS_EX_LAYERED       0x00080000
#define WS_EX_TRANSPARENT   0x00000020
#define WS_EX_CONTROLPARENT 0x00010000

#define CW_USEDEFAULT       ((int)0x80000000)

/* Window messages */
#define WM_NULL             0x0000
#define WM_CREATE           0x0001
#define WM_DESTROY          0x0002
#define WM_MOVE             0x0003
#define WM_SIZE             0x0005
#define WM_ACTIVATE         0x0006
#define WM_SETFOCUS         0x0007
#define WM_KILLFOCUS        0x0008
#define WM_PAINT            0x000F
#define WM_CLOSE            0x0010
#define WM_QUERYENDSESSION  0x0011
#define WM_QUIT             0x0012
#define WM_ERASEBKGND       0x0014
#define WM_SHOWWINDOW       0x0018
#define WM_SETCURSOR        0x0020
#define WM_MOUSEACTIVATE    0x0021
#define WM_GETMINMAXINFO    0x0024

#define WM_KEYDOWN          0x0100
#define WM_KEYUP            0x0101
#define WM_CHAR             0x0102
#define WM_SYSKEYDOWN       0x0104
#define WM_SYSKEYUP         0x0105
#define WM_SYSCHAR          0x0106
#define WM_SYSCOMMAND       0x0112

#define WM_LBUTTONDOWN      0x0201
#define WM_LBUTTONUP        0x0202
#define WM_LBUTTONDBLCLK    0x0203
#define WM_RBUTTONDOWN      0x0204
#define WM_RBUTTONUP        0x0205
#define WM_RBUTTONDBLCLK    0x0206
#define WM_MBUTTONDOWN      0x0207
#define WM_MBUTTONUP        0x0208
#define WM_MBUTTONDBLCLK    0x0209
#define WM_MOUSEMOVE        0x0200

#define WM_TIMER            0x0113
#define WM_HSCROLL          0x0114
#define WM_VSCROLL          0x0115

#define WM_ACTIVATEAPP      0x001C
#define WM_NCACTIVATE       0x0086
#define WM_NCPAINT          0x0085
#define WM_NCHITTEST        0x0084
#define WM_NCLBUTTONDOWN    0x00A1
#define WM_NCLBUTTONUP      0x00A2

/* Virtual key codes */
#define VK_LEFT             0x25
#define VK_UP               0x26
#define VK_RIGHT            0x27
#define VK_DOWN             0x28
#define VK_SPACE            0x20
#define VK_RETURN           0x0D
#define VK_ESCAPE           0x1B
#define VK_TAB              0x09
#define VK_BACK             0x08
#define VK_DELETE           0x2E
#define VK_INSERT           0x2D
#define VK_HOME             0x24
#define VK_END              0x23
#define VK_PRIOR            0x21
#define VK_NEXT             0x22
#define VK_SHIFT            0x10
#define VK_CONTROL          0x11
#define VK_MENU             0x12
#define VK_F1               0x70
#define VK_F2               0x71
#define VK_F3               0x72
#define VK_F4               0x73
#define VK_F5               0x74
#define VK_F6               0x75
#define VK_F7               0x76
#define VK_F8               0x77
#define VK_F9               0x78
#define VK_F10              0x79
#define VK_F11              0x7A
#define VK_F12              0x7B

/* ================================================================== */
/* HRESULT / error codes                                               */
/* ================================================================== */

typedef int32_t HRESULT;

#define S_OK                    ((HRESULT)0x00000000)
#define S_FALSE                 ((HRESULT)0x00000001)
#define E_FAIL                  ((HRESULT)0x80004005)
#define E_OUTOFMEMORY           ((HRESULT)0x8007000E)
#define E_INVALIDARG            ((HRESULT)0x80070057)
#define E_NOTIMPL               ((HRESULT)0x80004001)
#define E_NOINTERFACE           ((HRESULT)0x80004002)
#define E_POINTER               ((HRESULT)0x80004003)
#define E_HANDLE                ((HRESULT)0x80070006)
#define E_ABORT                 ((HRESULT)0x80004004)

#define SUCCEEDED(hr)           (((HRESULT)(hr)) >= 0)
#define FAILED(hr)              (((HRESULT)(hr)) < 0)

/* ================================================================== */
/* File / I/O constants                                                */
/* ================================================================== */

#define GENERIC_READ            0x80000000
#define GENERIC_WRITE           0x40000000
#define FILE_SHARE_READ         0x00000001
#define FILE_SHARE_WRITE        0x00000002
#define OPEN_EXISTING           3
#define CREATE_ALWAYS           2
#define CREATE_NEW              1
#define OPEN_ALWAYS             4
#define FILE_ATTRIBUTE_NORMAL   0x00000080
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define INVALID_HANDLE_VALUE    ((HANDLE)-1)
#define INVALID_FILE_SIZE       ((DWORD)0xFFFFFFFF)

/* Seek */
#define FILE_BEGIN              0
#define FILE_CURRENT            1
#define FILE_END                2

/* GetLastError */
#define ERROR_SUCCESS           0
#define ERROR_FILE_NOT_FOUND    2
#define ERROR_PATH_NOT_FOUND    3
#define ERROR_ACCESS_DENIED     5

/* ================================================================== */
/* GDI constants                                                       */
/* ================================================================== */

#define SRCCOPY                 0x00CC0020
#define SRCAND                  0x008800C6
#define SRCPAINT                0x00EE0086
#define SRCINVERT               0x00660046
#define BLACKNESS               0x00000042
#define WHITENESS               0x00FF0062

#define BI_RGB                  0
#define BI_BITFIELDS            3

#define DIB_RGB_COLORS          0
#define DIB_PAL_COLORS          1

/* ================================================================== */
/* Registry / Config constants                                         */
/* ================================================================== */

#define HKEY_CURRENT_USER       ((HKEY)0x80000001)
#define HKEY_LOCAL_MACHINE      ((HKEY)0x80000002)
#define KEY_READ                0x20019
#define KEY_WRITE               0x20006
#define REG_SZ                  1
#define REG_DWORD               4

/* ================================================================== */
/* PlaySound constants                                                 */
/* ================================================================== */

#define SND_SYNC                0x0000
#define SND_ASYNC               0x0001
#define SND_NODEFAULT           0x0002
#define SND_LOOP                0x0008
#define SND_NOSTOP              0x0010
#define SND_NOWAIT              0x00002000
#define SND_RESOURCE            0x00040004

/* ================================================================== */
/* Thread / synchronization                                            */
/* ================================================================== */

typedef uint32_t (*LPTHREAD_START_ROUTINE)(void*);
#define INFINITE                0xFFFFFFFF

typedef struct _RTL_CRITICAL_SECTION { void* DebugInfo; int32_t LockCount; int32_t RecursionCount; void* OwningThread; void* LockSemaphore; uint32_t SpinCount; } CRITICAL_SECTION, *LPCRITICAL_SECTION;

/* ================================================================== */
/* DDEML / string resource                                             */
/* ================================================================== */

#define CP_ACP                  0
#define MB_OK                   0x00000000
#define MB_ICONERROR            0x00000010
#define DT_LEFT                   0x00000000
#define DT_TOP                    0x00000000
#define DT_WORDBREAK              0x00000010
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00000100
#define LANG_NEUTRAL              0x00
#define SUBLANG_DEFAULT           0x01
#define MAKELANGID(p, s)          ((((WORD)(s)) << 10) | (WORD)(p))
#define DT_LEFT                   0x00000000
#define DT_TOP                    0x00000000
#define DT_WORDBREAK              0x00000010
#define MB_ICONWARNING          0x00000030
#define MB_ICONINFORMATION      0x00000040
#define MB_YESNO                0x00000004
#define IDYES                   6
#define IDNO                    7

/* ================================================================== */
/* Common Control styles                                               */
/* ================================================================== */

#define WS_TABSTOP              0x00010000
#define WS_GROUP                0x00020000
#define BS_PUSHBUTTON           0x00000000
#define BS_DEFPUSHBUTTON        0x00000001
#define BS_CHECKBOX             0x00000002
#define BS_AUTOCHECKBOX         0x00000003
#define BS_RADIOBUTTON          0x00000004
#define BS_GROUPBOX             0x00000007
#define ES_LEFT                 0x0000
#define ES_CENTER               0x0001
#define ES_RIGHT                0x0002
#define ES_MULTILINE            0x0004
#define ES_READONLY             0x0800
#define ES_AUTOHSCROLL          0x0080
#define ES_AUTOVSCROLL          0x0040
#define CBS_DROPDOWNLIST        0x0003
#define CBS_SORT                0x0100
#define SS_LEFT                 0x00000000
#define SS_CENTER               0x00000001
#define SS_RIGHT                0x00000002
#define SS_BITMAP               0x0000000E

/* ================================================================== */
/* Atoms / classes                                                     */
/* ================================================================== */

typedef uint16_t ATOM;

/* ================================================================== */
/* Stub function declarations (linker will fail but compiler is happy)  */
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

/* CRT equivalents (for stubbing when CRT is static) */
void*     operator_new(size_t);
void      GLOBAL_free(void*);
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
