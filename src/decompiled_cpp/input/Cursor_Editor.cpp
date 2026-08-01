// Status: INTEGRATED
/* Cursor_Editor.cpp — Window management and editor overlay lifecycle */

#include "Cursor.h"
#include "Cursor_internal.h"
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
        &g_empty_string[0],                        /* lpWindowName: "" */
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
/* Unlocks primary surface, then polls surface->vtable[0x44] with     */
/* &this->curs_pos_x() (+0x64) as output. Sleeps 10ms between polls,    */
/* times out after ~10s (1000 iterations) with FatalError+ExitProcess.*/
/* Returns the HDC value written to curs_pos_x.                        */
/* Called by: HelpWnd_*, Train_DrawTextOverlay                          */
/* ================================================================== */
void* Cursor::wait_for_blit(HWND hWnd)
{
    /* Unlock primary surface */
    DDRAW_UnlockPrimary();

    /* Poll surface vtable[0x44] (slot 17 = poll blit).
     * primary_surface is an opaque IDirectDrawSurface4 COM object; the
     * literal vtable dispatch is the documented DirectDraw ABI. */
    void** vtbl = *reinterpret_cast<void***>(this->primary_surface()); /* +0x38 */
    using PollBlit = int (*)(void*, void*);
    int result = reinterpret_cast<PollBlit>(vtbl[0x44 / 4])(
        this->primary_surface(), &this->curs_pos_x());                 /* +0x64 */

    for (int i = 0; i < 1000; i++) {
        if (result == 0) {
            return reinterpret_cast<void*>(
                static_cast<intptr_t>(this->curs_pos_x()));             /* +0x64 */
        }
        Sleep(10);
        result = reinterpret_cast<PollBlit>(vtbl[0x44 / 4])(
            this->primary_surface(), &this->curs_pos_x());
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
/*   6. Create two DDraw editor surfaces (via g_ddraw vtable[6])      */
/*   7. Handle player data: store + copy name/colours from playerData */
/*      or call init_network_player() if null                         */
/*   8. update_network_names(), start 50ms timer 0x53 at +0x19C       */
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

    /* Create two DDraw offscreen surfaces for editor toolbar.
     * g_ddraw is an opaque IDirectDraw4 COM object; CreateSurface via
     * vtable slot [6] is the documented DirectDraw ABI. Ghidra asm
     * @0x416C37-0x416C75 confirms: width = palette_rect.right - left
     * (+0x1B8 minus +0x1B0), height = palette_rect.bottom - top
     * (+0x1BC minus +0x1B4), dwFlags=7 = DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH. */

    int desc[24] = { 0 };
    void** ddrawVtbl = *reinterpret_cast<void***>(g_ddraw);

    int surfWidth = this->palette_rect.right - this->palette_rect.left;
    int surfHeight = this->palette_rect.bottom - this->palette_rect.top;

    desc[0] = 0x7C;       /* dwSize */
    desc[1] = 7;          /* dwFlags */
    desc[2] = surfHeight; /* dwHeight */
    desc[3] = surfWidth;  /* dwWidth */

    /* Create surface A at +0x590 */
    using CreateSurface = int (*)(void*, int*, void**, int);
    int err = reinterpret_cast<CreateSurface>(ddrawVtbl[6])(
        g_ddraw, desc, &this->editor_surf_a, 0);
    if (err != 0) {
        OutputDebugStringA("EDIT WINDOW : failed to create surface A");
    }
    auto* formatStorage = desc - 0x8C;
    DDRAW_SetSurfaceFormat(
        this->editor_surf_a,
        static_cast<int>(reinterpret_cast<intptr_t>(formatStorage)));
    DDRAW_RestoreSurfaces(this->editor_surf_a, formatStorage);

    /* Create surface B at +0x598 */
    err = reinterpret_cast<CreateSurface>(ddrawVtbl[6])(
        g_ddraw, desc, &this->editor_surf_b, 0);
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
            SetWindowTextA(this->hEditWnd, &g_empty_string[0]);
        }
    } else {
        /* Network mode: store player data */
        if (this->obj_184 != nullptr) {
            delete this->obj_184;
            this->obj_184 = nullptr;
        }

        this->obj_184 = static_cast<CursorEditorRecord*>(playerData); /* +0x184 */

        /* Copy player name from playerData+0x43 into edit control */
        SetWindowTextA(
            this->hEditWnd,
            reinterpret_cast<const char*>(
                reinterpret_cast<uint8_t*>(playerData) + 0x43));

        /* Zero the upload session field and mark audio preview */
        this->obj_184->upload_id = 0;                       /* +0x3A */
        this->obj_184->is_audio_preview = 1;                /* +0x3C */

        /* Copy body colour RGB from player record */
        this->color_r = this->obj_184->color_r;             /* +0x298 */
        this->color_g = this->obj_184->color_g;             /* +0x29C */
        this->color_b = this->obj_184->color_b;             /* +0x2A0 */

        /* Copy player name from g_player_config into player record at +0x25 */
        const char* cfgName = reinterpret_cast<const char*>(
            static_cast<uint8_t*>(g_player_config) + 6);
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

    /* Release editor DDraw surfaces.
     * vtable[2] = IDirectDrawSurface4::Release() (COM IUnknown). */
    if (this->editor_surf_a != nullptr) {                            /* +0x590 */
        Cursor_ComSurfaceRelease(this->editor_surf_a);
        this->editor_surf_a = nullptr;
    }
    if (this->editor_surf_b != nullptr) {                            /* +0x598 */
        Cursor_ComSurfaceRelease(this->editor_surf_b);
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


