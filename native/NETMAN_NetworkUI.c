/**
 * NETMAN_NetworkUI — Session-name EDIT control subclass WndProc
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file previously held 7 free-function "Class_Method(self, ...)"
 * transcriptions of NameEntryPanel (ui/NameEntryPanel.h) vtable overrides
 * and helpers — NETMAN_EnumerateSessions (0x441720), NETMAN_JoinSession
 * (0x441870, show() override), NETMAN_CreateSession (0x4419C0,
 * applyProviderModes()), NETMAN_LeaveSession (0x441A00, hide() override),
 * NETMAN_UpdateSessionInfo (0x441A90, on_update() override),
 * NETMAN_GetSessionInfo (0x441B40), and NETMAN_SetSessionInfo (0x441C80,
 * on_lbutton_down() override). All 7 were moved into ui/NameEntryPanel.cpp
 * 2026-08-17 as real compiler-managed methods/overrides, matching the
 * on_create()/window_proc()/on_timer() precedent already established
 * there — see ui/NameEntryPanel.h's vtable table for the full evidence
 * trail on each one. NETMAN_DestroySession (0x441F80), a DIFFERENT
 * function that turned out to be NameEntryPanel::on_key_down() (vtable
 * slot 21, discovered the same session), was never in this file — it
 * lived in native/NETMAN_SessionSettings.c, also since moved.
 *
 * What remains here is a genuine Win32-callback boundary, not part of the
 * NameEntryPanel C++ object model: the session-name EDIT control's
 * subclass WndProc, registered by NameEntryPanel::enumerateSessions()
 * (ui/NameEntryPanel.cpp) via SetWindowLongA(hwnd, GWL_WNDPROC, ...).
 */
#include "../shared/types.h"

#include <cstdio>
#include <cstdlib>

/* Forward declaration, matching the definition below exactly — satisfies
 * -Werror=missing-declarations now that ui/NameEntryPanel.cpp needs an
 * external (non-static) declaration to call this via SetWindowLongA. */
LRESULT __stdcall NETMAN_EditControlSubclassProc(void* hWnd, uint32_t msg,
                                                   uint32_t wParam, uint32_t lParam);

/* ================================================================== */
/* EDIT-control subclass WndProc — TODO: decompile 0x4417E0.            */
/* NameEntryPanel::enumerateSessions() (ui/NameEntryPanel.cpp)          */
/* subclasses the session-name edit control to this procedure via       */
/* SetWindowLongA(hwnd, GWL_WNDPROC, ...); it isn't decompiled yet, and  */
/* enumerateSessions() is currently only reached via on_create(), which  */
/* is itself only reached via show(), which every GUI-facing test        */
/* exercises through EditWindow — so this IS reachable at runtime once   */
/* a real player interacts with the session-name edit control. Per      */
/* CLAUDE.md's stub policy, this is a loud, asserting stub (matching     */
/* ui/UI_ChildWindow.cpp's precedent) rather than a silent no-op or a    */
/* raw, unresolvable function-pointer literal.                          */
/*                                                                       */
/* Uses std::abort(), not assert(), for the fallthrough guard: this      */
/* project's meson.build does not force `b_ndebug=false` (it only        */
/* defaults there via the "debug" buildtype -- `meson introspect         */
/* --buildoptions` shows `b_ndebug` is a normal, overridable option, and */
/* b_ndebug=true/if-release compiles assert() to nothing). An            */
/* assert-only guard would silently fall through to                     */
/* `return DefWindowProcA(...)` in an NDEBUG build -- exactly the        */
/* "internal no-op/null-return stub" this repo's stub policy forbids.    */
/* std::abort() fails the same way in every build configuration; the     */
/* fprintf above already carries the diagnostic message an               */
/* assert(false, "...") string would have duplicated.                    */
/* ================================================================== */
LRESULT __stdcall NETMAN_EditControlSubclassProc(void* hWnd, uint32_t msg,
                                                   uint32_t wParam, uint32_t lParam)
{
    (void)hWnd; (void)msg; (void)wParam; (void)lParam;
    std::fprintf(stderr,
        "STUB: NETMAN_EditControlSubclassProc (0x4417E0) reached — "
        "the session-name edit control's subclass WndProc is not yet "
        "decompiled (see PROGRESS.md).\n");
    std::abort();
}
