/**
 * NETMAN_NetworkUI — Multiplayer lobby panel / session window functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions operate on a Panel/NetworkSession window structure
 * (CGWND or NameEntryPanel, ~0x1E4+ bytes) used for the multiplayer
 * session enumeration and join dialogs. NOT Netman class methods.
 *
 * Contains:
 *   NETMAN_EnumerateSessions (0x441720) — Create session enumeration HWND
 *   NETMAN_JoinSession       (0x441870) — Show join session UI
 *   NETMAN_CreateSession     (0x4419C0) — Set session mode flags
 *   NETMAN_LeaveSession      (0x441A00) — Cleanup and hide session UI
 *   NETMAN_UpdateSessionInfo (0x441A90) — Blit panel + set sprite states
 *   NETMAN_GetSessionInfo    (0x441B40) — Update sprite visibility
 *   NETMAN_SetSessionInfo    (0x441C80) — Handle UI click/hit-test
 *   NETMAN_DestroySession    (0x441F80) — Session window proc
 *
 * === Panel structure offsets (common across CGWND session panels) ===
 *   +0x00: vtable
 *   +0x04: hInstance
 *   +0x08: HWND (parent)
 *   +0xD4: scroll_x1
 *   +0xD8: scroll_y1
 *   +0xDC: scroll_right1
 *   +0xE0: scroll_bottom1
 *   +0xE8: dirty/text flag
 *   +0xEC: timer ID
 *   +0x148: update/refresh flag
 *   +0x14C: scroll_x2
 *   +0x150: scroll_y2
 *   +0x154: scroll_right2
 *   +0x158: scroll_bottom2
 *   +0x18C: panel RECT left
 *   +0x190: panel RECT top
 *   +0x194: panel RECT right
 *   +0x198: panel RECT bottom
 *   +0x19C: click RECT
 *   +0x1AC: loaded/sprites-initialized flag
 *   +0x1B0: sprite 0 (btn_back/cancel)
 *   +0x1B4: sprite 1 (btn_join/ok)
 *   +0x1B8: sprite 2
 *   +0x1BC: sprite 3
 *   +0x1C0: sprite 4
 *   +0x1C4: sprite 5
 *   +0x1C8: sprite 6
 *   +0x1CC: surface / child panel
 *   +0x1D0: child surface (for blit)
 *   +0x1D8: session HWND (edit control for session name)
 *   +0x1DC: old WndProc (for subclassing)
 *   +0x1E0: flag: host mode (2-player)
 *   +0x1E1: flag: host mode (4-player)
 */
#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void*    g_ui_main;              /* 0x4A8860 */
extern void*    _g_primary_surface;     /* 0x4FD3C4 */
extern void*    g_resmgr;               /* resource manager */
extern void*    _g_netman_data;         /* 0x4FD3A8 — network data struct */
extern char     g_empty_string;         /* 0x4851D0 */
extern void*    g_main_window;          /* 0x4A885C */

extern "C" {

extern int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);
extern int32_t __stdcall IsWindowVisible(void* hWnd);
extern int32_t __stdcall SetWindowTextA(void* hWnd, const char* lpString);
extern int32_t __stdcall GetWindowTextA(void* hWnd, char* lpString, int32_t nMaxCount);
extern void    __stdcall PostMessageA(void* hWnd, uint32_t Msg, uint32_t wParam, uint32_t lParam);
extern void*   __stdcall SetTimer(void* hWnd, uint32_t nIDEvent, uint32_t uElapse, void* lpTimerFunc);
extern int32_t __stdcall KillTimer(void* hWnd, uint32_t uIDEvent);
extern void    __stdcall SetFocus(void* hWnd);
extern void    __stdcall ShowWindow(void* hWnd, int32_t nCmdShow);
extern void*   __stdcall SetWindowLongA(void* hWnd, int32_t nIndex, void* dwNewLong);
extern void*   __stdcall CreateWindowExA(uint32_t dwExStyle, const char* lpClassName,
                                          const char* lpWindowName, uint32_t dwStyle,
                                          int32_t x, int32_t y, int32_t nWidth, int32_t nHeight,
                                          void* hWndParent, void* hMenu,
                                          void* hInstance, void* lpParam);
extern void    __stdcall Sleep(uint32_t dwMilliseconds);
extern int32_t __stdcall PtInRect(const RECT* lprc, POINT pt);
extern void*   __stdcall DefWindowProcA(void* hWnd, uint32_t Msg, uint32_t wParam, uint32_t lParam);

extern void  __cdecl    UIPANEL_Blit(void* surface, int32_t srcX, int32_t srcY,
                                      int32_t srcW, int32_t srcH, void* dstSurface,
                                      int32_t dstX, int32_t dstY, int32_t dstW,
                                      int32_t dstH, int32_t flags);
extern void  __thiscall UIPANEL_EndPaint(void* panel);
extern void  __thiscall UIPANEL_EndPaintEx(void* panel, void* hwnd, int32_t hdc,
                                            uint8_t repaint, void* updateRect);
extern void  __cdecl    UI_WindowBase_Show(void* window);
extern void  __cdecl    UI_WindowBase_Hide(void* window);
extern void  __cdecl    UI_MainMenu_SetState(void* ui_main, int32_t state);
extern int32_t __cdecl  ResourceManager_GetById(void* resmgr, uint32_t id);
extern int32_t __cdecl  ResourceManager_GetStringById(void* resmgr, uint32_t id);
extern void  __cdecl    RESMGR_LoadSoundResource(int32_t resId);
extern void  __cdecl    Sprite_Init(void* sprite);
extern void  __cdecl    Sprite_Destroy(void* sprite);
extern void  __cdecl    Sprite_SetState(void* sprite, int32_t state, int32_t* unk);
extern void  __cdecl    FormatResourceString(void* resmgr, uint32_t id, char* buf, int32_t bufsize);
extern int32_t __cdecl  GLOBAL_free(void* ptr);
extern void* __cdecl    operator_new(size_t size);
extern void  __cdecl    PlaySound(int32_t soundId);
extern void  __cdecl    PlaySoundAt(int32_t soundId, int32_t x, int32_t y, int32_t flags);
extern int32_t __cdecl  CRT_rand(void);
extern void  __fastcall NETMAN_SendPacket(int32_t packetPtr);

extern void __fastcall DPlayManager_RenderConnectionPanel(void* panel);
extern void __fastcall NETMAN_GetSessionInfo(int32_t panel);
extern void __fastcall NETMAN_UpdateSessionInfo(void* panel);
}

/* ================================================================== */
/* NETMAN_EnumerateSessions — 0x441720                                 */
/* Create session enumeration window (edit control for session name). */
/* ================================================================== */
void __fastcall NETMAN_EnumerateSessions(int32_t panel)
{
    uint8_t* p = (uint8_t*)(uintptr_t)panel;

    if (*(int32_t*)(p + 0x1D8) != 0) return;  /* Already created */

    void* hWnd = CreateWindowExA(
        0x200,                          /* WS_EX_CLIENTEDGE */
        (const char*)(uintptr_t)0x47E464, /* "EDIT" window class */
        &g_empty_string,
        0x40000080,                     /* WS_CHILD | WS_VISIBLE */
        *(int32_t*)(p + 0x15C),    /* x */
        *(int32_t*)(p + 0x160),    /* y */
        *(int32_t*)(p + 0x164) - *(int32_t*)(p + 0x15C),   /* width */
        *(int32_t*)(p + 0x168) - *(int32_t*)(p + 0x160),   /* height */
        *(void**)(p + 8),           /* parent HWND */
        (void*)0x41F,               /* HMENU = ID */
        *(void**)(p + 4),           /* hInstance */
        NULL
    );

    *(void**)(p + 0x1D8) = hWnd;

    if (hWnd != NULL) {
        PostMessageA(hWnd, 0x30, *(uint32_t*)(uintptr_t)0x4855F8, 1);  /* EM_SETLIMITTEXT */
        PostMessageA(hWnd, 0xC5, 0x40, 0);                              /* EM_SETMARGINS */
        SetWindowTextA(hWnd, (const char*)((uint8_t*)_g_netman_data + 0x6C));

        /* Subclass the edit control */
        void* oldWndProc = SetWindowLongA(hWnd, -4, (void*)(uintptr_t)0x4417E0);
        *(void**)(p + 0x1DC) = oldWndProc;
    }
}

/* ================================================================== */
/* NETMAN_JoinSession — 0x441870                                       */
/* Initialize and show the join-session UI panel.                      */
/* ================================================================== */
void __fastcall NETMAN_JoinSession(void* panel)
{
    uint8_t* p = (uint8_t*)panel;

    /* Mark loaded flag as false initially */
    *(uint8_t*)(p + 0x52) = 0;

    if (*(uint8_t*)(p + 0x1AC) == 0) {
        /* Allocate and initialize 7 sprites */
        int32_t* res = (int32_t*)(uintptr_t)ResourceManager_GetById(&g_resmgr, 0x439);
        *(int32_t*)(p + 0x1CC) = (int32_t)(uintptr_t)res;
        *(int32_t*)(p + 0x1D0) = (**(int32_t(**)(int32_t, int32_t))((uint8_t*)res + 4))(0, 0);

        Sprite_Init(*(void**)(p + 0x1B0));
        Sprite_Init(*(void**)(p + 0x1B4));
        Sprite_Init(*(void**)(p + 0x1B8));
        Sprite_Init(*(void**)(p + 0x1BC));
        Sprite_Init(*(void**)(p + 0x1C0));
        Sprite_Init(*(void**)(p + 0x1C4));
        Sprite_Init(*(void**)(p + 0x1C8));

        *(uint8_t*)(p + 0x1AC) = 1;  /* loaded flag */
    }

    /* Call vtable[7]: OnCreate */
    (**(void(**)(void*))((uint8_t*)*(void**)panel + 0x1C))(panel);

    /* Check network mode from _g_netman_data+0x10 (provider list) */
    {
        uint8_t* nd = (uint8_t*)_g_netman_data;
        void* provider = *(void**)(nd + 0x10);
        while (provider != NULL) {
            if (*(int32_t*)((uint8_t*)provider + 4) == 2) {
                *(uint8_t*)(p + 0x1E1) = 1;  /* 4-player mode */
            } else if (*(int32_t*)((uint8_t*)provider + 4) == 4) {
                *(uint8_t*)(p + 0x1E0) = 1;  /* 2-player mode */
            }
            provider = *(void**)provider;
        }
    }

    UI_WindowBase_Show(panel);
    SetFocus(*(void**)(p + 8));  /* parent HWND */

    /* Set scroll offsets via vtable[3] */
    (**(void(**)(int32_t, int32_t, int32_t, int32_t))((uint8_t*)*(void**)panel + 0x0C))(
        *(int32_t*)(p + 0x18),
        *(int32_t*)(p + 0x19),
        0, 1);

    /* Load and play sound resource */
    {
        int32_t soundId = ResourceManager_GetStringById(&g_resmgr, 0x5015);
        if (soundId != 0) {
            RESMGR_LoadSoundResource(soundId);
        }
    }

    /* Start animation timer (50ms interval) */
    uint32_t timerId = (uint32_t)(uintptr_t)SetTimer(*(void**)(p + 8), 0x50, 0x32, NULL);
    *(uint32_t*)(p + 0xEC) = timerId;
    *(int32_t*)(p + 0x50) = 2;  /* mode state */

    FormatResourceString(&g_resmgr, 0x79, (char*)(p + 0x3C), 0x40);
    DPlayManager_RenderConnectionPanel(panel);
}

/* ================================================================== */
/* NETMAN_CreateSession — 0x4419C0                                     */
/* Set session mode flags from network provider list.                  */
/* ================================================================== */
void __fastcall NETMAN_CreateSession(int32_t panel)
{
    uint8_t* p = (uint8_t*)(uintptr_t)panel;
    uint8_t* nd = (uint8_t*)_g_netman_data;

    void* provider = *(void**)(nd + 0x10);
    while (provider != NULL) {
        if (*(int32_t*)((uint8_t*)provider + 4) == 2) {
            *(uint8_t*)(p + 0x1E1) = 1;  /* 4-player mode */
        } else if (*(int32_t*)((uint8_t*)provider + 4) == 4) {
            *(uint8_t*)(p + 0x1E0) = 1;  /* 2-player mode */
        }
        provider = *(void**)provider;
    }
}

/* ================================================================== */
/* NETMAN_LeaveSession — 0x441A00                                      */
/* Cleanup sprites, kill timer, hide panel.                            */
/* ================================================================== */
void __fastcall NETMAN_LeaveSession(int32_t panel)
{
    uint8_t* p = (uint8_t*)(uintptr_t)panel;

    KillTimer(*(void**)(p + 8), *(uint32_t*)(p + 0xEC));

    if (*(char*)(p + 0x1AC) != '\0') {
        /* Destroy child surface via vtable */
        int32_t* res = *(int32_t**)(p + 0x1CC);
        (**(void(**)(void))((uint8_t*)res + 8))();

        Sprite_Destroy(*(void**)(p + 0x1B0));
        Sprite_Destroy(*(void**)(p + 0x1B4));
        Sprite_Destroy(*(void**)(p + 0x1B8));
        Sprite_Destroy(*(void**)(p + 0x1BC));
        Sprite_Destroy(*(void**)(p + 0x1C0));
        Sprite_Destroy(*(void**)(p + 0x1C4));
        Sprite_Destroy(*(void**)(p + 0x1C8));
        *(uint8_t*)(p + 0x1AC) = 0;  /* clear loaded flag */
    }

    UI_WindowBase_Hide((void*)(uintptr_t)panel);
}

/* ================================================================== */
/* NETMAN_UpdateSessionInfo — 0x441A90                                 */
/* Blit child surface, update sprite states, get session info.         */
/* ================================================================== */
void __fastcall NETMAN_UpdateSessionInfo(void* panel)
{
    uint8_t* p = (uint8_t*)panel;

    UIPANEL_Blit(
        *(void**)(p + 0x1D0),        /* child surface */
        *(uint32_t*)(p + 0xD4),       /* srcX */
        *(uint32_t*)(p + 0xD8),       /* srcY */
        *(int32_t*)(p + 0xDC),        /* srcW */
        *(uint32_t*)(p + 0xE0),       /* srcH */
        _g_primary_surface,
        *(uint32_t*)(p + 0x14C),      /* dstX */
        *(uint32_t*)(p + 0x150),      /* dstY */
        *(int32_t*)(p + 0x154),       /* dstW */
        *(uint32_t*)(p + 0x158),       /* dstH */
        1
    );

    Sprite_SetState(*(void**)(p + 0x1C8), 0, NULL);
    Sprite_SetState(*(void**)(p + 0x1B0), 0, NULL);
    Sprite_SetState(*(void**)(p + 0x1B4), 0, NULL);

    NETMAN_GetSessionInfo((int32_t)(uintptr_t)panel);

    UIPANEL_EndPaintEx(panel, *(void**)(p + 8), 0, 0, NULL);
    *(uint8_t*)(p + 0x148) = 1;  /* update flag */
}

/* ================================================================== */
/* NETMAN_GetSessionInfo — 0x441B40                                    */
/* Update sprite visibility based on session mode flags.               */
/* ================================================================== */
void __fastcall NETMAN_GetSessionInfo(int32_t panel)
{
    uint8_t* p = (uint8_t*)(uintptr_t)panel;
    uint8_t* nd = (uint8_t*)_g_netman_data;

    Sprite_SetState(*(void**)(p + 0x1C8), 0, NULL);

    if (*(char*)(nd + 8) == '\0') {
        /* Host mode */
        if (*(char*)(p + 0x1E0) != '\0') {
            /* 2-player mode */
            if (*(int32_t*)(nd + 0x28) == 4) {
                Sprite_SetState(*(void**)(p + 0x1B8), 1, NULL);
                ShowWindow(*(void**)(p + 0x1D8), 0);
            } else {
                Sprite_SetState(*(void**)(p + 0x1B8), 0, NULL);
            }
        }
        if (*(char*)(p + 0x1E1) == '\0') return;

        /* 4-player mode */
        if (*(int32_t*)(nd + 0x28) == 2) {
            Sprite_SetState(*(void**)(p + 0x1C0), 0, NULL);
            Sprite_SetState(*(void**)(p + 0x1BC), 1, NULL);
            ShowWindow(*(void**)(p + 0x1D8), 5);  /* SW_SHOW */
            SetFocus(*(void**)(p + 0x1D8));
            return;
        }
        ShowWindow(*(void**)(p + 0x1D8), 0);
    } else {
        /* Client mode */
        ShowWindow(*(void**)(p + 0x1D8), 0);
        if (*(char*)(p + 0x1E0) != '\0') {
            Sprite_SetState(*(void**)(p + 0x1B8),
                            (uint32_t)(*(int32_t*)(nd + 0x1C) == 4), NULL);
        }
        if (*(char*)(p + 0x1E1) == '\0') return;
        if (*(int32_t*)(nd + 0x1C) == 2) {
            Sprite_SetState(*(void**)(p + 0x1BC), 1, NULL);
            return;
        }
    }
    Sprite_SetState(*(void**)(p + 0x1BC), 0, NULL);
}

/* ================================================================== */
/* NETMAN_SetSessionInfo — 0x441C80                                    */
/* Handle UI click/hit-test on session panel sprites.                  */
/* Returns: 0                                                          */
/* ================================================================== */
uint32_t __thiscall NETMAN_SetSessionInfo(void* panel)
{
    uint8_t* p = (uint8_t*)panel;
    uint8_t* nd = (uint8_t*)_g_netman_data;

    /* Extract click coordinates from caller */
    uint32_t clickX, clickY;  /* passed in via register magic — approximated */
    /* NOTE: In the original code, these come from the caller's stack frame */
    uint32_t x = 0, y = 0; /* parameter-passing trick: in_stack_00000010 >> 16 = y, low = x */

    if (*(uint8_t*)(p + 0x148) == 0) return 0;

    POINT pt;
    pt.x = x;
    pt.y = y;

    /* Hit-test sprite 0 (btn_back/cancel) */
    if (PtInRect((RECT*)((uint8_t*)(uintptr_t)*(int32_t*)(p + 0x1B0) + 4), pt)) {
        Sprite_SetState(*(void**)(p + 0x1B0), 1, NULL);
        PlaySound(0x5015);
        UIPANEL_EndPaint(panel);
        Sleep(0x96);
        (**(void(**)(int32_t, int32_t, int32_t, int32_t, int32_t))((uint8_t*)*(void**)panel + 0x10))
            (0, 0, 0, 0, 1);

        GetWindowTextA(*(void**)(p + 0x1D8),
                       (char*)(nd + 0x6C), 0x40);
        if (*(char*)(nd + 8) == '\0') {
            *(uint8_t*)(nd + 0x24) = 1;
        } else {
            *(uint8_t*)(nd + 0x18) = 1;
        }
        NETMAN_SendPacket((int32_t)(uintptr_t)_g_netman_data);
        UI_MainMenu_SetState(g_ui_main, 3);
        return 0;
    }

    /* Hit-test sprite 1 (btn_join/ok) */
    if (PtInRect((RECT*)((uint8_t*)(uintptr_t)*(int32_t*)(p + 0x1B4) + 4), pt)) {
        Sprite_SetState(*(void**)(p + 0x1B4), 1, NULL);
        PlaySound(0x5015);
        UIPANEL_EndPaint(panel);
        Sleep(0x96);
        (**(void(**)(int32_t, int32_t, int32_t, int32_t, int32_t))((uint8_t*)*(void**)panel + 0x10))
            (0, 0, 0, 0, 1);
        UI_MainMenu_SetState(g_ui_main, 7);
        return 0;
    }

    /* Hit-test sprite 2 (2-player button) */
    if (PtInRect((RECT*)((uint8_t*)(uintptr_t)*(int32_t*)(p + 0x1B8) + 4), pt) &&
        *(char*)(p + 0x1E0) != '\0') {
        if (*(char*)(nd + 8) == '\0') {
            *(int32_t*)(nd + 0x28) = 4;
        } else {
            *(int32_t*)(nd + 0x1C) = 4;
        }
        NETMAN_GetSessionInfo((int32_t)(uintptr_t)panel);
        PlaySound(0x5015);
        UIPANEL_EndPaintEx(panel, *(void**)(p + 8), 0, 0, NULL);
        return 0;
    }

    /* Hit-test sprite 3 (4-player button) */
    if (PtInRect((RECT*)((uint8_t*)(uintptr_t)*(int32_t*)(p + 0x1BC) + 4), pt) &&
        *(char*)(p + 0x1E1) != '\0') {
        if (*(char*)(nd + 8) == '\0') {
            *(int32_t*)(nd + 0x28) = 2;
        } else {
            *(int32_t*)(nd + 0x1C) = 2;
        }
        NETMAN_GetSessionInfo((int32_t)(uintptr_t)panel);
        PlaySound(0x5015);
        UIPANEL_EndPaintEx(panel, *(void**)(p + 8), 0, 0, NULL);
        return 0;
    }

    /* Hit-test panel RECT (+0x19C) for random sound */
    if (PtInRect((RECT*)(p + 0x19C), pt)) {
        int32_t rnd = CRT_rand();
        PlaySoundAt(rnd / 0x1FFF + 0x500F, x, y, 4);
    }

    return 0;
}
