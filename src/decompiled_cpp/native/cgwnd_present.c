/**
 * cgwnd_present.c — Frame-present window message helper
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C free function, __cdecl. Sends WM_USER+7 to the main window to
 * signal a frame-present / UI-initialization-complete event.
 */

#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* g_main_window at 0x4AA4A0, hWnd at +0x08 */
extern void* g_main_window;

extern int32_t __stdcall SendMessageA(void* hWnd, uint32_t msg,
                                       uint32_t wParam, int32_t lParam);

/* ================================================================== */
/* CGWND_Present — Send WM_USER+7 present message                    */
/* Address: 0x45E1E0                                                   */
/* Size: 34 bytes (10 insn)                                            */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Sends WM_USER+7 (0x407) to the game window, used to synchronise    */
/* UI initialisation in CGWND_InitMode1 and the main loop.            */
/*                                                                     */
/* Called by: CGWND_InitMode1 (0x40853E, 0x4085B7)                    */
/*            GameLoop_FrameUpdate (0x45C3C0 — via SendMessage)       */
/*                                                                     */
/* @param param  Low byte sent as wParam (sub-mode indicator)          */
/* ================================================================== */
void __cdecl CGWND_Present(uint32_t param)
{
    void* hWnd = *(void**)((uint8_t*)g_main_window + 8);
    SendMessageA(hWnd, 0x407, param & 0xFF, 0);
}
