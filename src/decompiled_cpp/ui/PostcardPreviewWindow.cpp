// Status: INTEGRATED
/**
 * PostcardPreviewWindow.cpp — PostcardPreviewWindow class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file contains the PostcardPreviewWindow postcard preview/send dialog
 * implementation, including constructor, destructor, sprite creation,
 * background initialization, show and cleanup.
 */

#include "PostcardPreviewWindow.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include "../shared/types.h"
#include "../resources/ResourceManager.h"
#include "../core/CGWND.h"
#include <new>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* CRT memory */
    void*  operator_new(size_t size);               /* 0x465CE0 */
    void   GLOBAL_free(void* ptr);

    /* Network — Netman.h cannot be included here (its Win32 API
     * declarations conflict with the project's stubs/windows.h
     * signatures), so the Netman type is forward-declared and the
     * m_gameMode field at +0x7C4 is read by offset. */
class Netman;
    void   NETMAN_SendAck(Netman* netman);           /* 0x440390 */
    void   NETMAN_ReceiveFileTransfer(Netman* netman); /* 0x440310 */

    /* Tile map */
    void   TileMap_InvalidateRect(void* tilemap, int left, int top,
                                  int right, int bottom);            /* 0x455840 */

    /* CGWND — CGWND_SetMode declared in core/CGWND.h */

extern "C" {
    /* Timer */
    UINT_PTR SetTimer(HWND hWnd, UINT_PTR id, UINT ms, void* callback);
    void   KillTimer(HWND hWnd, UINT_PTR id);

    /* Focus / cursor / window */
    HWND   SetFocus(HWND hWnd);
    int    ShowCursor(BOOL bShow);
    BOOL   ShowWindow(HWND hWnd, int nCmdShow);
}

/* ================================================================== */
/* Global variables referenced                                         */
/* ================================================================== */

/* Netman singleton — the C++ build defines the storage as void* in
 * shared/stubs_impl.cpp; global mangling is by name, so the typed
 * view below aliases it. */
extern Netman* g_netman;              /* 0x4FD3AC — Network manager */
extern void*   g_main_window;         /* 0x4AA4A0 — Main CGWND window pointer */
class TileMap;
extern TileMap*  g_tilemap;             /* 0x4AAD08 — Tile map pointer */
extern int     g_viewport_rect_left;  /* 0x4AAD14 — Viewport left */
extern int     g_viewport_rect_top;   /* 0x4AAD18 — Viewport top */
extern int     g_viewport_rect_right; /* 0x4AAD1C — Viewport right */
extern int     g_viewport_rect_bottom;/* 0x4AAD20 — Viewport bottom */

/* ================================================================== */
/* Netman m_gameMode accessor                                          */
/* ================================================================== */

/**
 * Read Netman::m_gameMode (+0x7C4). Netman.h cannot be included here
 * (Win32 API declaration conflicts), so the field is read by its
 * canonical offset; 0=waiting, 1=hosting, 2=joined, 3=error.
 */
int32_t netman_m_game_mode()
{
    return *reinterpret_cast<const int32_t*>(
        reinterpret_cast<const uint8_t*>(g_netman) + 0x7C4);
}

/* ================================================================== */
/* PostcardPreviewWindow — Constructor                                  */
/* Address: 0x430A90                                                    */
/*                                                                      */
/* Called by: CGWND_InitAllSubsystems @ 0x407116 (new PostcardPreviewWindow) */
/* ================================================================== */
PostcardPreviewWindow::PostcardPreviewWindow(HINSTANCE hInstance, UINT resId)
    : UI_WindowBase(hInstance, resId)
{

    /* Override base vtable with PostcardPreviewWindow vtable */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Create all preview dialog sprites */
    this->draw_sprites();
}

/* ================================================================== */
/* PostcardPreviewWindow::draw_sprites — Create 11 preview sprites      */
/* Address: 0x430B10 (__fastcall, ECX = this)                           */
/*                                                                      */
/* Initializes all state fields (+0xE8, +0xEC timerId, +0x270, +0x278, */
/* +0x27E sprites_created, +0x280 background_surface, +0x284 background */
/* _resource). Creates 11 ButtonSprite objects (0x24 bytes each):       */
/*   - close button   (res 0x3d89) at +0x298                           */
/*   - options button (res 0x3d8b) at +0x2C0                           */
/*   - 9 status       (res 0x3da4..0x3dac) at +0x29C[0..8]            */
/* ================================================================== */
void PostcardPreviewWindow::draw_sprites()
{
    /* Initialize all state fields */
    this->icon_handle = nullptr;                 /* +0xE8 (icon set later by InitWindow) */
    this->sprites_created = 0;                   /* +0x27E */
    this->timerId = 0;                           /* +0xEC — start with no timer */
    this->unknown_270 = -1;                  /* +0x270 */
    this->unknown_278 = 0;                   /* +0x278 */
    this->background_resource = nullptr;         /* +0x284 */
    this->background_surface = nullptr;          /* +0x280 */

    /* Create close button sprite (res 0x3d89) */
    void* mem = operator_new(sizeof(ButtonSprite));
    this->sprite_close = mem ? new (mem) ButtonSprite(0x3d89) : nullptr; /* +0x298 */

    /* Create options button sprite (res 0x3d8b) */
    mem = operator_new(sizeof(ButtonSprite));
    this->sprite_options = mem ? new (mem) ButtonSprite(0x3d8b) : nullptr; /* +0x2C0 */

    /* Create 9 status/indicator sprites (res 0x3da4..0x3dac) */
    for (int i = 0; i < 9; i++) {
        mem = operator_new(sizeof(ButtonSprite));
        this->sprite_status[i] = mem ? new (mem) ButtonSprite(0x3DA4 + i) : nullptr; /* +0x29C+i*4 */
    }
}

/* ================================================================== */
/* PostcardPreviewWindow::init_sprites — Initialize sprite surfaces     */
/* Address: 0x431270 (__fastcall, ECX = this)                           */
/*                                                                      */
/* Ghidra labels this BuildingPanel_InitSprites, but the field offsets  */
/* (+0x27E, +0x28C/+0x288, +0x294/+0x290, +0x298, +0x2C0, +0x29C) are   */
/* this class's layout; show() (0x430D70) calls it on the preview       */
/* window object.                                                       */
/* ================================================================== */
void PostcardPreviewWindow::init_sprites()
{
    if (this->sprites_created != 0) {            /* +0x27E */
        return;
    }

    /* Load overlay resource 2 (res 0x3d87) and its surface */
    {
        int32_t res = g_resmgr.GetById(0x3d87);
        void* resource = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(res)));
        this->overlay_resource_2 = resource;     /* +0x28C */
        /* Resource vtable[1] = Lock(0, 0) — the binary dereferences the
         * GetById result unconditionally. */
        this->overlay_surface_2 = static_cast<ResourceObject*>(resource)->Lock(0, 0); /* +0x288 */
    }

    /* Load overlay resource 3 (res 0x3d88) and its surface */
    {
        int32_t res = g_resmgr.GetById(0x3d88);
        void* resource = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(res)));
        this->overlay_resource_3 = resource;     /* +0x294 */
        this->overlay_surface_3 = static_cast<ResourceObject*>(resource)->Lock(0, 0); /* +0x290 */
    }

    /* Initialize close button sprite (ButtonSprite::init, 0x454BF0) */
    this->sprite_close->init();                  /* +0x298 */

    /* Initialize options button sprite */
    this->sprite_options->init();                /* +0x2C0 */

    /* Initialize 9 status sprites */
    for (int i = 0; i < 9; i++) {
        this->sprite_status[i]->init();          /* +0x29C+i*4 */
    }

    this->sprites_created = 1;                   /* +0x27E */
}

/* ================================================================== */
/* PostcardPreviewWindow::init_background — Lazy-init preview bg        */
/* Address: 0x430C20 (__fastcall, ECX = this)                           */
/*                                                                      */
/* Loads resource 0x3d8a via g_resmgr.GetById(), stores the result at   */
/* +0x284 (background_resource), then immediately calls resource        */
/* vtable[1] (Lock(0, 0)) to get the surface handle, storing at +0x280  */
/* (background_surface).                                                */
/*                                                                      */
/* NOTE: The binary does NOT check if GetById returns 0 or -1 before    */
/* dereferencing — if the resource were missing, this would crash.      */
/* In practice, resource 0x3d8a always exists.                          */
/* ================================================================== */
void PostcardPreviewWindow::init_background()
{
    if (this->background_resource != nullptr) {  /* +0x284 */
        return;  /* Already initialized */
    }

    /* Load background resource (res 0x3d8a) */
    int32_t res = g_resmgr.GetById(0x3d8a);
    void* resource = reinterpret_cast<void*>(
        static_cast<uintptr_t>(static_cast<uint32_t>(res)));
    this->background_resource = resource;         /* +0x284 */

    /* Get surface via resource vtable[1] = Lock(0, 0).
       NOTE: Binary dereferences EAX immediately — no null check.     */
    this->background_surface = static_cast<ResourceObject*>(resource)->Lock(0, 0); /* +0x280 */
}

/* ================================================================== */
/* PostcardPreviewWindow::show — Show the preview dialog (vtable[2])    */
/* Address: 0x430D70 (__thiscall)                                       */
/*                                                                      */
/* Dispatched by CGWND_SetMode(9) @ 0x408294. Verified from raw x86     */
/* bytes:                                                               */
/*   netman m_gameMode (+0x7C4) != 2 -> CGWND_SetMode(3), return       */
/*   init_sprites (0x431270) -> vtable[7] (0x430FE0, not decompiled;    */
/*   UI_WindowBase has no C++ virtual for slot [7]) -> flag_27D = 0 ->  */
/*   UI_WindowBase::show() -> NETMAN_ReceiveFileTransfer(g_netman) ->   */
/*   ShowWindow(hWnd, SW_MAXIMIZE) -> hide OS cursor -> SetFocus(hWnd)  */
/*   -> timerId = SetTimer(hWnd, 0x4D, 120ms, NULL).                   */
/* ================================================================== */
void PostcardPreviewWindow::show()
{
    /* Gate: only show the preview dialog while joined to a network game
     * (Netman::m_gameMode at +0x7C4 == 2); otherwise return to town. */
    if (netman_m_game_mode() != 2) {
        CGWND_SetMode(3);                        /* 0x408130 */
        return;
    }

    /* Initialize sprite surfaces and overlays */
    this->init_sprites();                        /* 0x431270 */

    /* In the binary: vtable slot [7] (0x430FE0) is dispatched with
     * `this` — not yet decompiled (undefined in the Ghidra DB) and
     * UI_WindowBase has no C++ virtual for slot [7], so it is not
     * dispatched here. TODO: decompile 0x430FE0. */

    this->flag_27D = 0;                          /* +0x27D */

    this->UI_WindowBase::show();                 /* 0x4259C0 */

    NETMAN_ReceiveFileTransfer(g_netman);         /* 0x440310 */

    ShowWindow(this->hWnd, 3);                   /* SW_MAXIMIZE */

#ifdef _WIN32
    /* Hide the OS cursor while the dialog is shown */
    while (ShowCursor(0) >= 0) { }
#else
    /* Host deviation: SDL cursor ownership is handled by the window
     * shim; do not call the Win32 import (ShowCursor is a data-stub
     * on the host). */
#endif

    SetFocus(this->hWnd);

    /* 120 ms preview dialog timer (ID 0x4D) */
    this->timerId = SetTimer(this->hWnd, 0x4D, 0x78, nullptr);  /* +0xEC */
}

/* ================================================================== */
/* PostcardPreviewWindow::hide — Hide dialog and destroy sprites        */
/* Address: 0x430E00 (__fastcall, ECX = this, vtable[1])                */
/*                                                                      */
/* Release sequence (verified from raw x86):                            */
/*   1. Clear flag_27D                                                  */
/*   2. Hide window via UI_WindowBase::hide()                           */
/*   3. If netman m_gameMode (+0x7C4) == 2: NETMAN_SendAck              */
/*   4. Restore focus to main game window                               */
/*   5. Invalidate viewport rect                                        */
/*   6. KillTimer(hWnd, timerId) unconditionally, clear timerId         */
/*   7. If sprites_created: release overlays, Sprite_Destroy all 11     */
/*      sprites, clear sprites_created                                  */
/* ================================================================== */
void PostcardPreviewWindow::hide()
{
    this->flag_27D = 0;                          /* +0x27D */

    /* Step 2: Hide the window */
    this->UI_WindowBase::hide();                 /* 0x425990 */

    /* Step 3: Send network ack if joined to a network game
     * (Netman::m_gameMode at +0x7C4 == 2 — verified; the previous
     * +0x5C check was incorrect) */
    if (netman_m_game_mode() == 2) {
        NETMAN_SendAck(g_netman);                /* 0x440390 */
    }

    /* Step 4: Restore focus to main game window */
    HWND main_hwnd = static_cast<CGWND*>(g_main_window)->hWnd;   /* +0x08 */
    SetFocus(main_hwnd);

    /* Step 5: Invalidate viewport rect */
    TileMap_InvalidateRect(g_tilemap,
                           g_viewport_rect_left, g_viewport_rect_top,
                           g_viewport_rect_right, g_viewport_rect_bottom);

    /* Step 6: Kill timer — the binary calls KillTimer unconditionally
     * (Win32 treats a missing timer as a no-op; the SDL shim's KillTimer
     * also tolerates unknown IDs). */
    KillTimer(this->hWnd, this->timerId);        /* +0xEC */
    this->timerId = 0;                           /* +0xEC */

    /* Step 7: Destroy sprites (surface side only) */
    if (this->sprites_created) {                 /* +0x27E */
        /* Release overlay resource 2 via resource vtable[2] (Unlock) */
        if (this->overlay_resource_2) {          /* +0x28C */
            static_cast<ResourceObject*>(this->overlay_resource_2)->Unlock();
        }
        this->overlay_surface_2 = nullptr;       /* +0x288 */

        /* Release overlay resource 3 via resource vtable[2] (Unlock) */
        if (this->overlay_resource_3) {          /* +0x294 */
            static_cast<ResourceObject*>(this->overlay_resource_3)->Unlock();
        }
        this->overlay_surface_3 = nullptr;       /* +0x290 */

        /* Destroy close button sprite (surface only) */
        this->sprite_close->destroy();           /* +0x298 */

        /* Destroy options button sprite (surface only) */
        this->sprite_options->destroy();         /* +0x2C0 */

        /* Destroy 9 status sprites (surface only) */
        for (int i = 0; i < 9; i++) {
            this->sprite_status[i]->destroy();   /* +0x29C+i*4 */
        }

        /* Clear sprites created flag */
        this->sprites_created = 0;               /* +0x27E */
    }
}

/* ================================================================== */
/* PostcardPreviewWindow::destroy — Full destructor body                */
/* Address: 0x430C60 (__fastcall, ECX = this, PostcardPreviewWindow_DtorBody) */
/*                                                                      */
/* Called from: ~PostcardPreviewWindow (scalar deleting destructor).    */
/*                                                                      */
/* Release sequence:                                                    */
/*   1. Release background resource (vtable[2]), clear surface          */
/*   2. If sprites_created: release overlays + Sprite_Destroy all 11    */
/*   3. Free each sprite object via scalar-deleting dtor                */
/*   4. Base cleanup runs through the C++ destructor chain              */
/*      (UI_WindowBase::~UI_WindowBase -> base_destructor); the binary  */
/*      calls UI_WindowBase_BaseDtor explicitly here, which would       */
/*      double-run the base cleanup in natural C++.                     */
/* ================================================================== */
void PostcardPreviewWindow::destroy()
{
    /* Step 1: Release background resource */
    if (this->background_resource) {             /* +0x284 */
        static_cast<ResourceObject*>(this->background_resource)->Unlock();
        this->background_surface = nullptr;      /* +0x280 */
    }

    /* Step 2: Destroy sprites (surface + object free) */
    if (this->sprites_created) {                 /* +0x27E */
        /* Release overlay resource 2, clear surface */
        if (this->overlay_resource_2) {          /* +0x28C */
            static_cast<ResourceObject*>(this->overlay_resource_2)->Unlock();
        }
        this->overlay_surface_2 = nullptr;       /* +0x288 */

        /* Release overlay resource 3, clear surface */
        if (this->overlay_resource_3) {          /* +0x294 */
            static_cast<ResourceObject*>(this->overlay_resource_3)->Unlock();
        }
        this->overlay_surface_3 = nullptr;       /* +0x290 */

        /* Destroy close button sprite (surface only) */
        this->sprite_close->destroy();           /* +0x298 */

        /* Destroy options button sprite (surface only) */
        this->sprite_options->destroy();         /* +0x2C0 */

        /* Destroy 9 status sprites (surface only) */
        for (int i = 0; i < 9; i++) {
            this->sprite_status[i]->destroy();   /* +0x29C+i*4 */
        }

        this->sprites_created = 0;               /* +0x27E */
    }

    /* Step 3: Free each sprite object via scalar-deleting destructor */
    if (this->sprite_close) {                    /* +0x298 */
        delete this->sprite_close;
    }
    this->sprite_close = nullptr;

    if (this->sprite_options) {                  /* +0x2C0 */
        delete this->sprite_options;
    }
    this->sprite_options = nullptr;

    for (int i = 0; i < 9; i++) {
        if (this->sprite_status[i]) {            /* +0x29C+i*4 */
            delete this->sprite_status[i];
        }
        this->sprite_status[i] = nullptr;
    }

    /* Step 4: base cleanup via C++ destructor chain */
}

/* ================================================================== */
/* PostcardPreviewWindow::scalar deleting destructor — vtable[0]        */
/* Address: 0x430AF0 (__thiscall)                                       */
/*                                                                      */
/* Called by: vtable dispatch when destroying the object.               */
/* ================================================================== */
PostcardPreviewWindow::~PostcardPreviewWindow()
{
    this->destroy();
}
