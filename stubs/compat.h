/**
 * stubs/compat.h — GNU/MinGW compatibility shim for MSVC calling conventions
 *
 * Force-included via -include before all source files to:
 *   1. Provide standard headers commonly missing from the decompiled code
 *   2. Define MSVC-specific calling convention macros
 *
 * This file MUST be included via -include (force include) to avoid
 * header order issues.
 *
 * MinGW mode: the compiler natively supports __thiscall/__fastcall/__stdcall,
 *   so these macros are no-ops (already defined by the compiler).
 * Native mode: these are mapped to GCC attributes, but GCC only supports
 *   __stdcall and __cdecl natively; __thiscall and __fastcall produce
 *   warnings. Native mode is therefore limited to syntax checking.
 */

#ifndef STUBS_COMPAT_H
#define STUBS_COMPAT_H

/* ================================================================== */
/* Standard headers commonly missing from decompiled source files      */
/* ================================================================== */

/* MinGW: define this before <cmath> to get M_E, M_LOG2E, M_PI, etc. */
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cstddef>   /* size_t, NULL */
#include <cstdint>   /* uint8_t, int32_t, etc. */
#include <cstring>   /* memcpy, memset, strlen */
#include <cmath>     /* sqrt, fabs, M_LOG2E, etc. */
#include <cstdio>    /* FILE, NULL, etc. */
#include <cstdint>   /* intptr_t */

/* ================================================================== */
/* Additional Windows API type stubs not in shared/types.h             */
/* ================================================================== */

typedef void*               LPVOID;
typedef const void*         LPCVOID;
typedef void*               LPOVERLAPPED;
typedef void*               LPSECURITY_ATTRIBUTES;
typedef char*               LPSTR;
typedef uint32_t            UINT_PTR;  /* 32-bit target window/timer integer */

/* Registry types (advapi32.dll) */
typedef void*               HKEY;
typedef uint32_t            REGSAM;
typedef uint32_t            DWORD;
#define HKEY_CURRENT_USER   ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE  ((HKEY)(uintptr_t)0x80000002)
#define KEY_READ            0x20019
#define KEY_WRITE           0x20006
#define KEY_ALL_ACCESS      0xF003F
#define REG_SZ              1
#define REG_DWORD           4
#define REG_NONE            0
#define ERROR_SUCCESS       0

/* GDI types */
typedef void*               HPEN;
typedef void*               HBRUSH;
typedef void*               HFONT;
typedef uint16_t            WORD;

/* Common dialog types */
typedef uint32_t            DWORD;
typedef void*               LPOFNHOOKPROC;

/* Window class EX structure — defined in shared/types.h */

/* MessageBox constants */
#define MB_OK               0x00000000
#define MB_YESNO            0x00000004
#define MB_ICONQUESTION     0x00000020
#define MB_ICONWARNING      0x00000030
#define MB_ICONSTOP         0x00000010
#define IDYES               6
#define IDNO                7

/* File error constants */
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3

/* FormatMessage constants */
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100

/* File flags */
#define FILE_FLAG_NO_BUFFERING 0x20000000
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000

/* CreateFile access/share/disposition/attribute constants */
#define GENERIC_READ            0x80000000
#define GENERIC_WRITE           0x40000000
#define FILE_SHARE_READ         0x00000001
#define FILE_SHARE_WRITE        0x00000002
#define OPEN_EXISTING           3
#define FILE_ATTRIBUTE_NORMAL   0x00000080
#define INVALID_HANDLE_VALUE    (reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)))

typedef DWORD*              LPDWORD;

/* ShowWindow / window class style constants */
#define SW_HIDE             0
#define SW_SHOW             5
#define CS_VREDRAW          0x0001
#define CS_HREDRAW          0x0002

/* Window message constants (subset used by Lego Loco) */
#define WM_MOUSEMOVE        0x0200
#define WM_SETCURSOR        0x0020
#define WM_LBUTTONDOWN      0x0201
#define WM_LBUTTONUP        0x0202
#define WM_RBUTTONDOWN      0x0204
#define WM_RBUTTONUP        0x0205
#define WM_KEYDOWN          0x0100
#define WM_KEYUP            0x0101
#define WM_CHAR             0x0102
#define WM_PAINT            0x000F
#define WM_CLOSE            0x0010
#define WM_DESTROY          0x0002
#define WM_CREATE           0x0001
#define WM_SIZE             0x0005
#define WM_TIMER            0x0113
#define WM_COMMAND          0x0111
#define WM_INITDIALOG       0x0110

/* Button control messages (subset used by Lego Loco's dialog procs) */
#define BM_GETCHECK         0x00F0
#define BM_SETCHECK         0x00F1

/* Listbox control messages (subset used by
 * DirectPlay_SelectSessionDlgProc, network/DirectPlay.cpp) — real WinUser.h
 * values, byte/value-verified against the message constants pushed by
 * loco.exe's own SendDlgItemMessageA call sites at 0x4611B0. */
#define LB_ADDSTRING        0x0180
#define LB_RESETCONTENT     0x0184
#define LB_SETCURSEL        0x0186
#define LB_GETCURSEL        0x0188
#define LB_GETTEXT          0x0189
#define LB_GETCOUNT         0x018B
#define LB_GETITEMDATA      0x0199
#define LB_SETITEMDATA      0x019A

/* Listbox notification code (HIWORD(wParam) in WM_COMMAND) */
#define LBN_DBLCLK          2

/* Standard dialog control IDs (OK/Cancel push buttons) */
#define IDOK                1
#define IDCANCEL            2

/* OPENFILENAMEA structure (minimal) — uses basic types since windows.h types not yet available */
typedef struct tagOFNA {
    uint32_t      lStructSize;
    void*         hwndOwner;
    void*         hInstance;
    const char*   lpstrFilter;
    char*         lpstrCustomFilter;
    uint32_t      nMaxCustFilter;
    uint32_t      nFilterIndex;
    char*         lpstrFile;
    uint32_t      nMaxFile;
    char*         lpstrFileTitle;
    uint32_t      nMaxFileTitle;
    const char*   lpstrInitialDir;
    const char*   lpstrTitle;
    uint32_t      Flags;
    uint16_t      nFileOffset;
    uint16_t      nFileExtension;
    const char*   lpstrDefExt;
    uintptr_t     lCustData;
    void*         lpfnHook;
    const char*   lpTemplateName;
} OPENFILENAMEA;

/* MSG structure — defined in shared/types.h */

/* ================================================================== */
/* MSVC calling convention aliases                                     */
/*                                                                      */
/* The recovered binary is PE32/MSVC, so these annotations are part of */
/* the Windows ABI and must remain active for x86 Windows builds.  The  */
/* host build is normally x86_64 Linux, where GCC warns that the x86    */
/* attributes are ignored.  Keep the source annotations syntactically   */
/* present there, but make their host meaning the native default ABI.    */
/* ================================================================== */

#if defined(_MSC_VER)
/* MSVC owns these keywords.  Do not define a macro with the same name: */
/* __thiscall is a real keyword even though it is not a preprocessor     */
/* macro, and replacing it would silently change the original ABI.      */
# ifndef CDECL
#  define CDECL __cdecl
# endif
# ifndef WINAPI
#  define WINAPI __stdcall
# endif
# ifndef CALLBACK
#  define CALLBACK __stdcall
# endif
#else
# if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__)) && \
     (defined(__GNUC__) || defined(__clang__))
#  define LOCO_THIS_CALL __attribute__((__thiscall__))
#  define LOCO_FAST_CALL __attribute__((__fastcall__))
#  define LOCO_STD_CALL  __attribute__((__stdcall__))
#  define LOCO_CDECL     __attribute__((__cdecl__))
# else
/* Non-x86 Windows hosts and all non-Windows hosts use their native ABI. */
#  define LOCO_THIS_CALL
#  define LOCO_FAST_CALL
#  define LOCO_STD_CALL
#  define LOCO_CDECL
# endif

# ifndef __thiscall
#  define __thiscall LOCO_THIS_CALL
# endif
# ifndef __fastcall
#  define __fastcall LOCO_FAST_CALL
# endif
# ifndef __stdcall
#  define __stdcall LOCO_STD_CALL
# endif
# ifndef __cdecl
#  define __cdecl LOCO_CDECL
# endif
# ifndef CDECL
#  define CDECL __cdecl
# endif
# ifndef WINAPI
#  define WINAPI __stdcall
# endif
# ifndef CALLBACK
#  define CALLBACK __stdcall
# endif
#endif

#endif /* STUBS_COMPAT_H */
