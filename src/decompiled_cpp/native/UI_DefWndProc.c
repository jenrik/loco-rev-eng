/**
 * UI_DefWndProc — Default WindowProc stub (pass-through to DefWindowProcA)
 * Address: 0x422EA0
 * Size: 29 bytes
 * Calling convention: __stdcall (RET 0x10)
 *
 * Acts as a stub window procedure for UI windows that don't need custom
 * message handling. All messages are forwarded directly to DefWindowProcA.
 *
 * This function is referenced from multiple vtables as a default WndProc
 * slot (vtable[9,10,11] of EditWindow at 0x4779F8 + 0x24..0x2C, plus
 * many other vtables in the 0x477400-0x477C00 range for Entity-derived
 * and Panel-derived classes).
 *
 * Called by: Window message dispatch via vtable (120+ references across
 *            multiple class vtables)
 *
 * @param hwnd    Window handle
 * @param msg     Window message ID
 * @param wParam  WPARAM
 * @param lParam  LPARAM
 * @return        Result from DefWindowProcA
 */

#include <windows.h>

/* ================================================================== */
/* UI_DefWndProc                                                       */
/* ================================================================== */
LRESULT __stdcall UI_DefWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
