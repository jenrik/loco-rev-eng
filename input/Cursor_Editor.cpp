// Status: INTEGRATED
/* Cursor_Editor.cpp — Window management and editor overlay lifecycle */

#include "Cursor.h"
#include "Cursor_internal.h"
#include "../platform/ddraw_interfaces.h"
#include "../network/DPlayManager.h"
#include "../ui/ButtonSprite.h"

#ifndef _WIN32
#include <SDL3/SDL.h>
#include "sdl3_window.h"

/* Host-only adapter: sdl3_window.h declares SetFocus but the shim does not
 * implement it; provide a no-op host definition with the same ABI. The
 * Win32 path uses the real user32 function. */
HWND SetFocus(HWND hWnd) { (void)hWnd; return nullptr; }
#endif /* !_WIN32 */

int32_t Cursor::create(HWND hParent)
{
    /* Get desktop window rect for fullscreen size */
    HWND desktop = GetDesktopWindow();
    RECT desktopRect;
    GetClientRect(desktop, &desktopRect);

    /* Load icon resource 0x65 */
    HICON icon = LoadIconA(
        this->hInstance,
        reinterpret_cast<LPCSTR>(static_cast<intptr_t>(0x65)));
    /* Binary temporarily stores HICON at +0xE8 (reuses cached_height slot);
     * icon is consumed by UI_CreateFullWindow and field is re-initialized
     * later by update_client_rect(). */
    this->cached_height = static_cast<int32_t>(reinterpret_cast<intptr_t>(icon));

    /* Init editor sprites */
    this->init_editor_sprites();

    /* Create the full-screen overlay window */
    int result = UI_CreateFullWindow(
        this, 0, hParent,
        desktopRect.left, desktopRect.top,
        desktopRect.right - desktopRect.left,
        desktopRect.bottom - desktopRect.top,
        nullptr,
        icon,
        0);

    if (result == 0) {
        return result;
    }

    /* Create child edit control for toolbar text input.
     * Class name "EDIT" lives at 0x47E464 in the binary (the previous
     * transcription wrongly passed g_empty_string at 0x4851D0).
     * Position/size args per the 0x4169F1..0x416A13 disassembly:
     *   x = window_rect.top (+0xF8), y = window_rect.right (+0xFC),
     *   w = window_rect.bottom - window_rect.top (+0x100 - +0xF8),
     *   h = cursor_client_rect.left - window_rect.right (+0x104 - +0xFC). */
    HWND editWnd = CreateWindowExA(
        0x200,                                    /* dwExStyle: WS_EX_CLIENTEDGE */
        reinterpret_cast<LPCSTR>(static_cast<intptr_t>(0x47E464)),
                                                   /* lpClassName: "EDIT" */
        &g_empty_string,                        /* lpWindowName: "" */
        0x40001004,                                /* dwStyle: WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL */
        this->window_rect.top,            /* x */
        this->window_rect.right,          /* y */
        this->window_rect.bottom - this->window_rect.top,     /* width */
        this->cursor_client_rect.left - this->window_rect.right,  /* height */
        this->hWnd,                                 /* parent */
        reinterpret_cast<HMENU>(static_cast<intptr_t>(0x411)),
                                                   /* control ID */
        this->hInstance,
        nullptr);

    this->hEditWnd = editWnd;                                      /* +0xF4 */

    /* Configure edit control: set font, limit text length, subclass WndProc.
     * The subclass procedure at 0x416B00 is an unlabeled binary region
     * (not the vtable-slot Cursor::toolbar_wndproc at 0x419A60). */
    PostMessageA(editWnd, 0x30,
                 static_cast<WPARAM>(reinterpret_cast<uintptr_t>(g_font_small)),
                 1);  /* WM_SETFONT */
    PostMessageA(editWnd, 0xC5, 0x4F, 0);                            /* EM_LIMITTEXT */
    LONG prevWndProc = SetWindowLongA(editWnd, -4 /* GWL_WNDPROC */, 0x416B00);
    this->prev_wndproc = prevWndProc;                                /* +0x73C */

    /* Clean up editor sprites (they were only needed during window creation) */
    this->cleanup_editor_sprites();

    return 1;
}

/* ================================================================== */
/* Cursor::destroy_window — Window destroy handler                      */
/* Address: 0x414B80                                                    */
/*                                                                     */
/* Sets wndproc_flag=0, calls DestroyWindow(hWnd), PostQuitMessage(0) */
/* if field_0C==0. Returns 0 always.                                   */
/* ================================================================== */
int32_t Cursor::destroy_window()
{
    this->wndproc_flag() = 0;                                         /* +0xDB */
    DestroyWindow(this->hWnd);                                      /* +0x08 */

    if (this->hWndParent == nullptr) {                                /* +0x0C */
        PostQuitMessage(0);
    }

    return 0;
}

/* ================================================================== */
/* Cursor::wait_for_blit — Poll for blit completion on primary surface */
/* Address: 0x414BB0                                                    */
/*                                                                     */
/* Unlocks primary surface, then polls surface->vtable[0x44]           */
/* (slot 17 = IDirectDrawSurface::GetDC(HDC*)) with &this->             */
/* blit_wait_hdc() (+0x64, independent Cursor-only storage — see that   */
/* accessor's doc comment in Cursor.h for why this no longer aliases    */
/* UI_WindowBase::childObj0) as the GetDC out-param. The retry loop is  */
/* the standard DirectDraw idiom: GetDC fails with a nonzero HRESULT    */
/* (DDERR_SURFACEBUSY/WASSTILLDRAWING) while the blit is still in       */
/* flight. Sleeps 10ms between polls, times out after ~10s (1000        */
/* iterations) with FatalError+ExitProcess.                             */
/* Returns the HDC value written by the successful GetDC call.          */
/* Called by: HelpWnd_*, Train_DrawTextOverlay                          */
/* ================================================================== */
void* Cursor::wait_for_blit(HWND hWnd)
{
    /* Unlock primary surface */
    DDRAW_UnlockPrimary();

    /* Poll IDirectDrawSurface4::GetDC() on the primary surface (real COM
     * ABI slot 17, confirmed via disassembly — see doc comment above and
     * the matching BeginPaint()/EndPaintEx() precedent, ui/UI_WindowBase.cpp).
     * Dispatched via the typed interface rather than a raw vtable-slot
     * literal, same as every other DirectDraw call site converted this
     * session. */
    auto* surface = static_cast<IDirectDrawSurface4*>(this->primary_surface());
    HRESULT result = surface->GetDC(&this->blit_wait_hdc());

    for (int i = 0; i < 1000; i++) {
        if (result == 0) {
            return this->blit_wait_hdc();                                /* +0x64 */
        }
        Sleep(10);
        result = surface->GetDC(&this->blit_wait_hdc());
    }

    WIN32_FatalError();
    ExitProcess(1);
    /* UNREACHABLE */
    return nullptr;
}

/* ================================================================== */
/* Cursor::show — Show cursor editor overlay                           */
/* Address: 0x416B80                                                    */
/*                                                                     */
/* Shows the editor toolbar. Guards on visible (+0xE4). If hidden:   */
/*   1. InitEditorSprites, vtable[7] (pre-show hook)                  */
/*   2. ShowWindow(hWnd, SW_SHOWMAXIMIZED=3)                          */
/*   3. Hide edit control, set focus to hWnd                          */
/*   4. Reset editor flags and selection indices                       */
/*   5. Release all toolbar_sprites[64]                                */
/*   6. Create two DDraw editor surfaces (via g_ddraw->CreateSurface)  */
/*   7. Handle player data: store + copy name/colours from playerData */
/*      or call init_network_player() if null                         */
/*   8. update_network_names(), start 50ms timer 0x53 at +0x19C       */
/*                                                                     */
/* REACHABILITY (2026-08-14): unreached on the host build today — its  */
/* only real caller, the free function Cursor_Show(void*), is still a  */
/* no-op stub (shared/defsym_stubs.cpp). Of this function's 4 real x86 */
/* callers (get_xrefs_to 0x416B80), only CGWND_SetMode's mode-7 case   */
/* passes a literal NULL; the other 2 push field values (ESI+0x60C,    */
/* ESI+0x130) from code Ghidra has never bounded as a function, so     */
/* their producer/allocation is untraced. The `playerData != nullptr`  */
/* branch below now does `this->obj_184 = static_cast<DPlayManager*>(  */
/* playerData)` and later `delete`s it — dispatching a REAL virtual    */
/* destructor through playerData's vptr. Whoever wires Cursor_Show for */
/* real must first confirm those 2 untraced call sites' `playerData`   */
/* is a real, placement-new-constructed DPlayManager (same protocol as */
/* init_network_player/Train_network.cpp), not raw operator_new(0x39C) */
/* storage — the latter would make this `delete` read a garbage vptr.  */
/* ================================================================== */
void Cursor::show(void* playerData)
{
    if (this->visible != 0)                                          /* +0xE4 */
        return;

    this->init_editor_sprites();

    /* Call vtable[7] = pre-show virtual hook */
    this->on_create();

    this->scroll_end_flag = 0;                                             /* +0x180 */

    UI_WindowBase::show();

    /* Show main window maximized, hide edit control */
    ShowWindow(this->hWnd, 3);                                       /* SW_SHOWMAXIMIZED */
    ShowWindow(this->hEditWnd, 0);                                   /* SW_HIDE */
    SetFocus(this->hWnd);

    /* Reset editor flags. The binary (0x416C0D..0x416C1B) writes +0xEC = 1
     * (editor_state), then bytes +0x2B1 = 1, +0x2B2 = 0, +0x2B3 = 0. */
    this->editor_state = 1;                                       /* +0xEC */
    this->editor_flags[1] = 1;                                        /* active_tab = 1 */
    this->editor_flags[2] = 0;                                        /* scroll_dir */
    this->editor_flags[3] = 0;                                        /* (unknown) */

    this->palette_end_idx = -1;                                       /* +0x2B8 */
    this->palette_start_idx = -1;                                     /* +0x2BC */
    this->selected_idx_384 = -1;                                      /* +0x384 */

    /* Release all toolbar_sprites[64] at +0x48C */
    for (int i = 0; i < 64; i++) {
        delete this->toolbar_sprites[i];
        this->toolbar_sprites[i] = nullptr;
    }

    /* Create two DDraw offscreen surfaces for editor toolbar. Ghidra asm
     * @0x416C37-0x416C75 confirms: width = palette_rect.right - left
     * (+0x1B8 minus +0x1B0), height = palette_rect.bottom - top
     * (+0x1BC minus +0x1B4), dwFlags=7 = DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH.
     *
     * `desc`/`formatStorage` are kept exactly as originally transcribed —
     * a raw int[24] whose `- 0x8C` pointer arithmetic further down is a
     * separate, pre-existing, unverified stack-layout landmine this pass
     * does not disturb (see PROGRESS.md's DirectDraw-shim Phase 5 note).
     * CreateSurface itself is dispatched through the real, properly-sized
     * DDSURFACEDESC below instead of reusing this undersized 96-byte
     * buffer as the real struct — a 1:1 translation of desc[0..3]'s
     * already-recovered meaning, nothing new claimed. */

    int desc[24] = { 0 };

    int surfWidth = this->palette_rect.right - this->palette_rect.left;
    int surfHeight = this->palette_rect.bottom - this->palette_rect.top;

    desc[0] = 0x7C;       /* dwSize (historical annotation only) */
    desc[1] = 7;          /* dwFlags */
    desc[2] = surfHeight; /* dwHeight */
    desc[3] = surfWidth;  /* dwWidth */

    DDSURFACEDESC surface_desc;
    surface_desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    surface_desc.dwHeight = surfHeight;
    surface_desc.dwWidth = surfWidth;

    IDirectDraw4* dd = static_cast<IDirectDraw4*>(g_ddraw);

    /* Create surface A at +0x590 */
    IDirectDrawSurface4* surf_a = nullptr;
    HRESULT err = dd->CreateSurface(&surface_desc, &surf_a, nullptr);
    this->editor_surf_a = surf_a;
    if (err != 0) {
        OutputDebugStringA("EDIT WINDOW : failed to create surface A");
    }
    auto* formatStorage = desc - 0x8C;
    DDRAW_SetSurfaceFormat(
        this->editor_surf_a,
        static_cast<int>(reinterpret_cast<intptr_t>(formatStorage)));
    DDRAW_RestoreSurfaces(this->editor_surf_a, formatStorage);

    /* Create surface B at +0x598 */
    IDirectDrawSurface4* surf_b = nullptr;
    err = dd->CreateSurface(&surface_desc, &surf_b, nullptr);
    this->editor_surf_b = surf_b;
    if (err != 0) {
        OutputDebugStringA("EDIT WINDOW : failed to create surface B");
    }
    DDRAW_SetSurfaceFormat(
        this->editor_surf_b,
        static_cast<int>(reinterpret_cast<intptr_t>(formatStorage)));
    DDRAW_RestoreSurfaces(this->editor_surf_b, formatStorage);

    /* Clear dirty flags */
    this->surf_a_dirty = 0;                                          /* +0x594 */
    this->surf_b_dirty = 0;                                          /* +0x59C */
    this->surface_toggle = 0;                                        /* +0x58C */

    /* Handle player data */
    if (playerData == nullptr) {
        /* Offline mode: create local player if none exists */
        if (this->obj_184 == nullptr) {                              /* +0x184 */
            this->init_network_player();
            SetWindowTextA(this->hEditWnd, &g_empty_string);
        }
    } else {
        /* Network mode: store player data */
        if (this->obj_184 != nullptr) {
            delete this->obj_184;
            this->obj_184 = nullptr;
        }

        /* playerData is a real, already-constructed DPlayManager* — see
         * input/Cursor.h's removal comment on the former CursorEditorRecord
         * partial view. */
        this->obj_184 = static_cast<DPlayManager*>(playerData); /* +0x184 */

        /* Copy player name from playerData+0x43 (m_playerName) into edit control */
        SetWindowTextA(this->hEditWnd, this->obj_184->m_playerName);

        /* Zero the upload session field and mark audio preview (editor-local
         * reuse of m_wordValue/m_dwordValue — see DPlayManager.h). */
        this->obj_184->m_wordValue = 0;                     /* +0x3A */
        this->obj_184->m_dwordValue = 1;                    /* +0x3C */

        /* Copy body colour RGB from player record */
        this->color_r = this->obj_184->color_r;             /* +0x298 */
        this->color_g = this->obj_184->color_g;             /* +0x29C */
        this->color_b = this->obj_184->color_b;             /* +0x2A0 */

        /* Copy player name from g_player_config into player record at +0x25 */
        const char* cfgName = reinterpret_cast<const char*>(
            reinterpret_cast<uint8_t*>(g_player_config) + 6);
        size_t nameLen = strlen(cfgName);
        memcpy(reinterpret_cast<uint8_t*>(playerData) + 0x25, cfgName, nameLen);
        reinterpret_cast<char*>(playerData)[0x25 + nameLen] = '\0';
    }

    /* Refresh network player names */
    this->update_network_names();

    /* Start 50ms periodic timer */
    this->timer_id_19C = SetTimer(this->hWnd, 0x53, 50, nullptr);   /* +0x19C */
}

/* ================================================================== */
/* Cursor::hide — Hide cursor editor overlay and release resources    */
/* Address: 0x416F70                                                    */
/*                                                                     */
/* Guards on visible (+0xE4). Hides window, cleans up sprites,        */
/* kills timers, releases DDraw surfaces and toolbar sprites,         */
/* leaves DPlay session.                                               */
/* ================================================================== */
void Cursor::hide()
{
    if (this->visible == 0)                                          /* +0xE4 */
        return;

    UI_WindowBase::hide();
    this->cleanup_editor_sprites();

    this->cached_client_width = 0;                                 /* +0xEC */

    /* Kill timers */
    if (this->timer_id_18C != 0) {                                   /* +0x18C */
        KillTimer(this->hWnd, this->timer_id_18C);
        this->timer_id_18C = 0;
    }
    if (this->timer_id_19C != 0) {                                   /* +0x19C */
        KillTimer(this->hWnd, this->timer_id_19C);
        this->timer_id_19C = 0;
    }

    /* Release editor DDraw surfaces. */
    if (this->editor_surf_a != nullptr) {                            /* +0x590 */
        static_cast<IDirectDrawSurface4*>(this->editor_surf_a)->Release();
        this->editor_surf_a = nullptr;
    }
    if (this->editor_surf_b != nullptr) {                            /* +0x598 */
        static_cast<IDirectDrawSurface4*>(this->editor_surf_b)->Release();
        this->editor_surf_b = nullptr;
    }

    /* Release all toolbar_sprites[64] */
    for (int i = 0; i < 64; i++) {
        delete this->toolbar_sprites[i];
        this->toolbar_sprites[i] = nullptr;
    }

    /* Leave network session */
    DPLAY_LeaveSession(_g_dplay);

    /* Reset flags */
    this->ui_active = 1;                                             /* +0x188 */
    this->cached_client_height = 1;                         /* +0xF0 */
}


