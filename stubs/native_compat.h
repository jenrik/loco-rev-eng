/**
 * stubs/native_compat.h — compatibility types for native Ghidra C output.
 *
 * Force-included after compat.h when compiling native/*.c as C++17.  These
 * declarations model the 32-bit Win32 names used by the original executable;
 * they are compile-time scaffolding, not platform implementations.
 */
#ifndef STUBS_NATIVE_COMPAT_H
#define STUBS_NATIVE_COMPAT_H

#include "../shared/types.h"
#include "../shared/vtable_addrs.h"

typedef int                INT;
typedef unsigned int       uint;
typedef unsigned short     ushort;
typedef uint32_t           undefined4;
typedef uint32_t           MMRESULT;
typedef struct tagMSG      MSG;
typedef MSG*               LPMSG;
typedef LRESULT (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);

/* Win32 BITMAP is 0x18 bytes in the 32-bit executable. */
typedef struct tagBITMAP {
    LONG   bmType;
    LONG   bmWidth;
    LONG   bmHeight;
    LONG   bmWidthBytes;
    WORD   bmPlanes;
    WORD   bmBitsPixel;
    LPVOID bmBits;
} BITMAP;

typedef struct native_DDSCAPS2 {
    DWORD dwCaps;
    DWORD dwCaps2;
    DWORD dwCaps3;
    DWORD dwCaps4;
} DDSCAPS2;

/* Only fields referenced by native/ddraw_helpers.c are named. */
typedef struct native_DDSURFACEDESC {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    LONG  lPitch;
    void* lpSurface;
    DWORD reserved[20];
    DDSCAPS2 ddsCaps;
} DDSURFACEDESC;

struct HelpWnd;

/* APIs used by native files which intentionally do not include windows.h. */
#ifdef __cplusplus
extern "C" {
#endif
extern HDC     __stdcall CreateCompatibleDC(HDC);
extern HGDIOBJ __stdcall SelectObject(HDC, HGDIOBJ);
extern BOOL    __stdcall DeleteDC(HDC);
extern int     __stdcall GetObjectA(HGDIOBJ, int, LPVOID);
extern void    __stdcall OutputDebugStringA(LPCSTR);
extern DWORD   __stdcall GetFileAttributesA(LPCSTR);
extern HANDLE  __stdcall LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT);
extern BOOL    __stdcall DeleteObject(HGDIOBJ);
extern BOOL    __stdcall IsRectEmpty(const RECT*);
extern BOOL    __stdcall OffsetRect(RECT*, int, int);
extern BOOL    __stdcall ClientToScreen(HWND, POINT*);
extern BOOL    __stdcall GetWindowRect(HWND, RECT*);
extern BOOL    __stdcall IntersectRect(RECT*, const RECT*, const RECT*);
#ifdef __cplusplus
}
#endif
extern int     __cdecl DDRAW_BlitHBITMAPToSurface(void*, void*, int, int, int);

#ifndef WM_SYSCOMMAND
#define WM_SYSCOMMAND 0x0112
#endif
#ifndef SC_CLOSE
#define SC_CLOSE 0xF060
#endif
#ifndef MB_ICONEXCLAMATION
#define MB_ICONEXCLAMATION MB_ICONWARNING
#endif
#ifndef SM_CXSCREEN
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#endif
#ifndef HWND_TOP
#define HWND_TOP ((HWND)0)
#endif
#ifndef SW_SHOWNORMAL
#define SW_SHOWNORMAL 1
#endif
#ifndef PM_REMOVE
#define PM_REMOVE 1
#endif

#endif /* STUBS_NATIVE_COMPAT_H */
