// Status: TRANSCRIBED
/* Cursor_Editor.cpp — Window management and editor overlay lifecycle */

#include "Cursor.h"
#include "Cursor_internal.h"

#ifndef _WIN32
#include <SDL3/SDL.h>
#include "sdl3_window.h"

/* Inline stubs for Win32 functions not yet covered by sdl3_window.h */
static inline void* SetFocus(void*) { return NULL; }
#endif /* !_WIN32 */

int32_t Cursor::create(HWND hParent)
{
    /* Get desktop window rect for fullscreen size */
    HWND desktop = GetDesktopWindow();
    RECT desktopRect;
    GetClientRect(desktop, &desktopRect);

    /* Load icon resource 0x65 */
    HICON icon = LoadIconA(this->hInstance, (LPCSTR)(intptr_t)0x65);
    /* Binary temporarily stores HICON at +0xE8 (reuses cached_height slot);
     * icon is consumed by UI_CreateFullWindow and field is re-initialized
     * later by update_client_rect(). */
    this->cached_height = (int32_t)(intptr_t)icon;

    /* Init editor sprites */
    this->init_editor_sprites();

    /* Create the full-screen overlay window */
    int result = UI_CreateFullWindow(
        this, 0, hParent,
        desktopRect.left, desktopRect.top,
        desktopRect.right - desktopRect.left,
        desktopRect.bottom - desktopRect.top,
        (HMENU)nullptr,
        icon,
        0);

    if (result == 0) {
        return result;
    }

    /* Create child edit control for toolbar text input */
    HWND editWnd = CreateWindowExA(
        0x200,                                    /* dwExStyle: WS_EX_CLIENTEDGE */
        (LPCSTR)0x4851D0,                          /* lpClassName: "EDIT" */
        &g_empty_string[0],                        /* lpWindowName: "" */
        0x40001004,                                /* dwStyle: WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL */
        this->window_rect.top,            /* x */
        this->window_rect.right,            /* y */
        this->client_rect.left - this->window_rect.top,  /* width */
        this->client_rect.top - this->window_rect.right,  /* height */
        this->hWnd,                                 /* parent */
        (HMENU)0x411,                               /* control ID */
        this->hInstance,
        nullptr);

    this->hEditWnd = editWnd;                                      /* +0xF4 */

    /* Configure edit control: set font, limit text length, subclass WndProc */
    PostMessageA(editWnd, 0x30, (WPARAM)(intptr_t)g_font_small, 1);  /* WM_SETFONT */
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
    this->wndproc_flag = 0;                                         /* +0xDB */
    DestroyWindow(this->hWnd);                                      /* +0x08 */

    if (this->field_0C == 0) {                                      /* +0x0C */
        PostQuitMessage(0);
    }

    return 0;
}

/* ================================================================== */
/* Cursor::wait_for_blit — Poll for blit completion on primary surface */
/* Address: 0x414BB0                                                    */
/*                                                                     */
/* Unlocks primary surface, then polls surface->vtable[0x44] with     */
/* &this->curs_pos_x (+0x64) as output. Sleeps 10ms between polls,    */
/* times out after ~10s (1000 iterations) with FatalError+ExitProcess.*/
/* Returns the HDC value written to curs_pos_x.                        */
/* Called by: HelpWnd_*, Train_DrawTextOverlay                          */
/* ================================================================== */
void* Cursor::wait_for_blit(HWND hWnd)
{
    /* Unlock primary surface */
    DDRAW_UnlockPrimary(hWnd);

    /* Poll surface vtable[0x44] (slot 17 = poll blit) */
    void** vtbl = *(void***)this->primary_surface;                  /* +0x38 */
    int result = ((int (*)(void*, void*))vtbl[0x44 / 4])(
        this->primary_surface, &this->curs_pos_x);                  /* +0x64 */

    for (int i = 0; i < 1000; i++) {
        if (result == 0) {
            return (void*)(intptr_t)this->curs_pos_x;               /* +0x64 */
        }
        Sleep(10);
        result = ((int (*)(void*, void*))vtbl[0x44 / 4])(
            this->primary_surface, &this->curs_pos_x);
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
    this->on_show();

    this->scroll_end_flag = 0;                                             /* +0x180 */

    UI_WindowBase::show();

    /* Show main window maximized, hide edit control */
    ShowWindow(this->hWnd, 3);                                       /* SW_SHOWMAXIMIZED */
    ShowWindow(this->hEditWnd, 0);                                   /* SW_HIDE */
    SetFocus(this->hWnd);

    /* Reset editor flags */
    this->editor_flags[0] = 1;                                        /* tab_visible */
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

    /* Create two DDraw offscreen surfaces for editor toolbar */
    /* Build DDSURFACEDESC for two offscreen surfaces.
     * Ghidra asm @0x416C37-0x416C75 confirms field offsets:
     *   width  = field_1B8 - field_1B0  (+0x1B8 minus +0x1B0)
     *   height = field_1BC - field_1B4  (+0x1BC minus +0x1B4)
     * dwFlags=7 = DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH */

    int desc[24] = { 0 };
    int* ddrawVtbl = *(int**)g_ddraw;

    int surfWidth = this->field_1B8 - this->field_1B0;
    int surfHeight = this->field_1BC - this->field_1B4;

    desc[0] = 0x7C;       /* dwSize */
    desc[1] = 7;          /* dwFlags */
    desc[2] = surfHeight; /* dwHeight */
    desc[3] = surfWidth;  /* dwWidth */

    /* Create surface A at +0x590 */
    int err = ((int (*)(void*, int*, void**, int))ddrawVtbl[6])(g_ddraw, desc, &this->editor_surf_a, 0);
    if (err != 0) {
        OutputDebugStringA("EDIT WINDOW : failed to create surface A");
    }
    DDRAW_SetSurfaceFormat((int*)this->editor_surf_a, (int)(intptr_t)&desc[0xFFFFFF74]);
    DDRAW_RestoreSurfaces((int*)this->editor_surf_a, &desc[0xFFFFFF74]);

    /* Create surface B at +0x598 */
    err = ((int (*)(void*, int*, void**, int))ddrawVtbl[6])(g_ddraw, desc, &this->editor_surf_b, 0);
    if (err != 0) {
        OutputDebugStringA("EDIT WINDOW : failed to create surface B");
    }
    DDRAW_SetSurfaceFormat((int*)this->editor_surf_b, (int)(intptr_t)&desc[0xFFFFFF74]);
    DDRAW_RestoreSurfaces((int*)this->editor_surf_b, &desc[0xFFFFFF74]);

    /* Clear dirty flags */
    this->field_594 = 0;                                             /* +0x594 */
    this->field_59C = 0;                                             /* +0x59C */
    this->field_58C = 0;                        /* +0x58C */

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
            /* NOTE: CursorEditorRecord uses vtable[0] scalar deleting dtor.
             * Use delete once refined to a proper class with virtual dtor. */
            void** pVtbl = *(void***)this->obj_184;
            ((void (*)(void*, uint8_t))pVtbl[0])(this->obj_184, 1);
            this->obj_184 = nullptr;
        }

        this->obj_184 = playerData;                                  /* +0x184 */

        /* Copy player name from playerData+0x43 into edit control */
        SetWindowTextA(this->hEditWnd, (const char*)((intptr_t)playerData + 0x43));

        /* Zero the upload session field */
        *(int16_t*)((intptr_t)playerData + 0x3A) = 0;
        *(int32_t*)((intptr_t)playerData + 0x3C) = 1;

        /* Copy body colour RGB from player record */
        uint8_t* playerBytes = (uint8_t*)playerData;
        this->color_r = playerBytes[0x40];                           /* +0x298 */
        this->color_g = playerBytes[0x41];                           /* +0x29C */
        this->color_b = playerBytes[0x42];                           /* +0x2A0 */

        /* Copy player name from g_player_config into player record at +0x25 */
        const char* cfgName = (const char*)((intptr_t)g_player_config + 6);
        size_t nameLen = strlen(cfgName);
        memcpy((void*)((intptr_t)playerData + 0x25), cfgName, nameLen);
        ((char*)playerData)[0x25 + nameLen] = '\0';
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
        void** vtbl = *(void***)this->editor_surf_a;
        ((void (*)(void*))vtbl[2])(this->editor_surf_a);
        this->editor_surf_a = nullptr;
    }
    if (this->editor_surf_b != nullptr) {                            /* +0x598 */
        void** vtbl = *(void***)this->editor_surf_b;
        ((void (*)(void*))vtbl[2])(this->editor_surf_b);
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
    this->field_188 = 1;                                             /* +0x188 */
    this->cached_client_height = 1;                         /* +0xF0 */
}


