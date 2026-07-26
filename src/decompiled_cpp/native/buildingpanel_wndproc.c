/**
 * BuildingPanel_WndProc — Window procedure for the building selection panel
 * Address: 0x4324F0
 * Size: 70 bytes (0x46)
 * Calling convention: __stdcall (LRESULT CALLBACK) — RET 0x10, 4 stack params
 *
 * Standard Windows window procedure for the building selection panel (the UI
 * overlay shown when the player clicks on a building to inspect/manage it).
 *
 * Intercepts WM_SYSCOMMAND (0x112) with wParam matching 0xF140 to prevent the
 * system from activating the screensaver or system menu while the building
 * panel is visible. Instead of allowing the default system behavior, the game
 * switches back to town mode (mode 3) and posts a quit message to dismiss the
 * building panel overlay.
 *
 * All other messages are passed through to DefWindowProcA unmodified.
 *
 * The value 0xF140 corresponds to the Win32 SC_SCREENSAVE message; the game
 * repurposes this to return the player to town view whenever a system-level
 * event (screensaver timeout, Alt key press, etc.) would normally interrupt
 * the building panel.
 *
 * Note on 0xF140: This value is defined as SC_SCREENSAVE in the Windows SDK,
 * but the project's platform header (win32_platform.h) labels it SC_KEYMENU
 * based on observed game behavior with the Alt key. Both refer to the same
 * numeric value (0xF140).
 *
 * Pointer stored at: 0x477E4C (DATA reference — likely WNDCLASS.lpfnWndProc
 *   or a function-pointer table entry for the building panel window class)
 *
 * Called by: Windows via window message dispatch (RegisterClassA +
 *   CreateWindowExA / CreateDialogParamA)
 *
 * See also:
 *   - GAMESTATE_WndProc (0x40B4C0) — intercepts same 0xF140 as SC_CLOSE
 *   - Cursor_ToolbarWndProc (0x419A60) — also intercepts WM_SYSCOMMAND/0xF140
 *   - Town_LoadBackground (0x42EE20) — vtable[11] handler for same message
 *
 * @param hWnd    HWND    — handle to the building panel window
 * @param uMsg    UINT    — window message identifier
 * @param wParam  WPARAM  — message-specific parameter (low 4 bits masked)
 * @param lParam  LPARAM  — message-specific parameter
 * @return        LRESULT — result forwarded from DefWindowProcA
 */
extern void CGWND_SetMode(int mode);
extern void WIN32_PostQuit(void);
extern LRESULT __stdcall DefWindowProcA(HWND, UINT, WPARAM, LPARAM);

LRESULT __stdcall BuildingPanel_WndProc(
    HWND   hWnd,
    UINT   uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    /*
     * WM_SYSCOMMAND (0x112) — intercept system commands.
     * Mask wParam with 0xFFF0 to strip the low nibble (menu-event flags
     * like accelerator indicators) and compare against 0xF140.
     *
     * When this system command is received (screensaver timeout, Alt key
     * press, or similar system-level event), instead of letting the system
     * process it (which would display a menu or launch the screensaver over
     * the building panel), the game:
     *   1. Switches the game mode to 3 (town mode) via CGWND_SetMode.
     *   2. Posts a quit message via WIN32_PostQuit to dismiss the panel.
     */
    if (uMsg == WM_SYSCOMMAND &&                          /* 0x112 */
        (wParam & 0xFFF0) == 0xF140)   /* SC_SCREENSAVE / SC_KEYMENU */
    {
        CGWND_SetMode(3);              /* 0x408130 — return to town mode */
        WIN32_PostQuit();              /* 0x463670 — dismiss panel overlay */
    }

    /*
     * Pass all messages (including WM_PAINT, WM_LBUTTONDOWN, WM_DESTROY,
     * and the intercepted WM_SYSCOMMAND after the if-block above) through
     * to DefWindowProcA for default handling.
     *
     * NOTE: The return value of DefWindowProcA is passed through as the
     * function's return value via EAX (no intermediate register clobber).
     */
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}
