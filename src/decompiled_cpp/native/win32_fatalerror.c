/**
 * win32_fatalerror.c — Fatal error message box
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C free function, __cdecl. Shows a modal error dialog with the
 * formatted string from resource ID 0x14A ("An error has occurred...").
 */

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
extern char  g_empty_string;

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
    uint32_t* ptr;

    /* Zero the buffer */
    buf[0] = g_empty_string;     /* copy first byte from g_empty_string at 0x4FD230 */
    for (ptr = (uint32_t*)(buf + 1); (uint8_t*)ptr < (uint8_t*)buf + 0x200; ) {
        *ptr = 0;
        ptr++;
    }

    /* Load and format error string from resource 0x14A */
    FormatResourceString(g_resmgr, 0x14A, buf, 0x200);

    /* Show message box */
    MessageBoxA(NULL, buf, "LEGO LOCO", 0x30);  /* MB_ICONWARNING | MB_OK */
}
