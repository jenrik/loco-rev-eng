/**
 * win32_fatalerror.c — Fatal error message box
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C free function, __cdecl. Shows a modal error dialog with the
 * formatted string from resource ID 0x14A ("An error has occurred...").
 */

#include <cstring>
#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void __cdecl FormatResourceString(void* resmgr, uint32_t res_id,
                                          char* out_buf, uint32_t buf_size);
extern int32_t __stdcall MessageBoxA(void* hWnd, const char* text,
                                      const char* caption, uint32_t type);

/* Global resource manager at 0x4855E8 */
extern void* g_resmgr;
extern char  g_empty_string;    /* 0x4851D0 — single-byte NUL/empty string */

/* Forward declaration (STRICT=2 -Wmissing-declarations); no other TU
 * declares this signature (input/Cursor_internal.h and ui/UIPANEL.cpp
 * both redeclare a matching zero-arg WIN32_FatalError(void), but
 * shared/link_stubs.cpp separately defines an unrelated single-arg
 * overload WIN32_FatalError(const char*) — a pre-existing two-overload
 * situation out of scope here, so this file stays self-contained). */
void __cdecl WIN32_FatalError(void);

/* ================================================================== */
/* WIN32_FatalError — Show fatal error dialog                          */
/* Address: 0x463600                                                   */
/* Size: 101 bytes                                                     */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Loads format string for resource 0x14A ("Fatal error occurred..."), */
/* displays it in a modal MessageBox with MB_ICONWARNING|MB_OK (0x30), */
/* then returns (does NOT exit the process — caller handles that).     */
/*                                                                     */
/* Called by: WinMain error path                                       */
/* ================================================================== */
void __cdecl WIN32_FatalError(void)
{
    char buf[0x200];

    /* Zero the buffer. Original (0x463617-0x463643): one byte copied
     * from g_empty_string (at 0x4851D0, not 0x4FD230 as previously
     * annotated here), then the remaining 0x1FF bytes zeroed via
     * REP STOSD/STOSW/STOSB starting at buf+1 — i.e. memset(buf+1, 0,
     * 0x1FF), not a dword-stride loop starting misaligned at buf+1
     * (which would both misalign every store and overrun buf by one
     * byte on its last iteration). */
    buf[0] = g_empty_string;
    std::memset(buf + 1, 0, sizeof(buf) - 1);

    /* Load and format error string from resource 0x14A */
    FormatResourceString(g_resmgr, 0x14A, buf, 0x200);

    /* Show message box */
    MessageBoxA(NULL, buf, "LEGO LOCO", 0x30);  /* MB_ICONWARNING | MB_OK */
}
