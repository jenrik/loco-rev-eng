/**
 * PostcardPreviewWindow.cpp — PostcardPreviewWindow class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file contains the PostcardPreviewWindow postcard preview/send dialog
 * implementation, including constructor, destructor, sprite creation,
 * background initialization, and cleanup.
 */

#include "PostcardPreviewWindow.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include "../shared/types.h"
#include "../resources/ResourceManager.h"

/* ================================================================== */
/* External references (C-linkage from Win32 and other modules)        */
/* ================================================================== */

/* CRT memory */
    void*  operator_new(size_t size);               /* 0x465CE0 */
    void   GLOBAL_free(void* ptr);

    /* Sprite management */
    void   Sprite_Destroy(void* sprite);                             /* 0x44AE90 */

    /**
     * ButtonSprite_Ctor — Constructor for ButtonSprite (vtable 0x47851C).
     * Address: 0x454B50 (__thiscall, ECX = allocated memory)
     *
     * Allocates via operator_new(0x24), then calls this on the result.
     * Sets vtable, zeroes fields, stores resource ID.
     *
     * @param obj     Allocated memory (ECX = this)
     * @param res_id  Resource ID to associate with this sprite
     * @return        obj pointer (same as ECX input)
     */
    void*  ButtonSprite_Ctor(void* obj, int res_id);                 /* 0x454B50 */

    /* UI Window management */
    void   UI_WindowBase_Hide(void* self);                           /* 0x425990 */
    void   UI_WindowBase_BaseDtor(void* self);                       /* 0x425910 */
    void*  UI_WindowBase_Ctor(void* self, HINSTANCE hInstance, UINT resId); /* 0x425880 */

    /* Network */
    void   NETMAN_SendAck(void* netman);                             /* 0x4415C0 */

    /* Tile map */
    void   TileMap_InvalidateRect(void* tilemap, int left, int top,
                                  int right, int bottom);            /* 0x416FF0 */

extern "C" {
    /* Timer */
    void   KillTimer(HWND hWnd, UINT_PTR id);

    /* Focus */
    void   SetFocus(HWND hWnd);
}

/* ================================================================== */
/* Global variables referenced                                          */
/* ================================================================== */

extern void* g_netman;                  /* 0x4FD33C — Network manager */
extern void* g_tilemap;                 /* 0x4FD244 — Tile map pointer */
extern void* g_main_window;             /* 0x4FD230 — Main CGWND window pointer */
extern int   g_viewport_rect_left;      /* 0x4FD0F0 — Viewport left */
extern int   g_viewport_rect_top;       /* 0x4FD0F4 — Viewport top */
extern int   g_viewport_rect_right;     /* 0x4FD0F8 — Viewport right */
extern int   g_viewport_rect_bottom;    /* 0x4FD0FC — Viewport bottom */

/* ================================================================== */
/* PostcardPreviewWindow — Constructor                                  */
/* Address: 0x430A90                                                    */
/*                                                                      */
/* Called by: CGWND_InitAllSubsystems @ 0x407107                        */
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
/* Called from: constructor @ 0x430ACE                                  */
/*                                                                      */
/* Initializes all state fields (+0xE8, +0xEC timerId, +0x270, +0x278, */
/* +0x27E sprites_created, +0x280 background_surface, +0x284 background */
/* _resource). Creates 11 ButtonSprite objects (0x24 bytes each):       */
/*   - close button   (res 0x3d89) at +0x298                           */
/*   - options button (res 0x3d8b) at +0x2C0                           */
/*   - 9 status       (res 0x3da4..0x3dac) at +0x29C[0..8]            */
/*                                                                      */
/* Uses MSVC SEH prologue (handler @ 0x475B41) for exception safety     */
/* during sprite allocation (the try-level local_4 var tracks which     */
/* sprites were successfully created).                                  */
/* ================================================================== */
void PostcardPreviewWindow::draw_sprites()
{
    /* Initialize all state fields */
    this->field_E8 = 0;                                     /* +0xE8 */
    this->sprites_created = 0;                               /* +0x27E */
    this->timerId = 0;                                       /* +0xEC — start with no timer */
    this->state_field_270 = -1;                              /* +0x270 */
    this->state_field_278 = 0;                               /* +0x278 */
    this->background_resource = 0;                           /* +0x284 */
    this->background_surface = 0;                            /* +0x280 */

    /* Create close button sprite (res 0x3d89) */
    void* mem = operator_new(0x24);
    this->sprite_close = mem ? ButtonSprite_Ctor(mem, 0x3d89) : 0; /* +0x298 */

    /* Create options button sprite (res 0x3d8b) */
    mem = operator_new(0x24);
    this->sprite_options = mem ? ButtonSprite_Ctor(mem, 0x3d8b) : 0; /* +0x2C0 */

    /* Create 9 status/indicator sprites (res 0x3da4..0x3dac) */
    for (int i = 0; i < 9; i++) {
        mem = operator_new(0x24);
        this->sprite_status[i] = mem ? ButtonSprite_Ctor(mem, 0x3DA4 + i) : 0; /* +0x29C+i*4 */
    }
}

/* ================================================================== */
/* PostcardPreviewWindow::init_background — Lazy-init preview bg        */
/* Address: 0x430C20 (__fastcall, ECX = this)                           */
/*                                                                      */
/* Called by: CGWND_InitMode1 @ 0x408487, 0x4085AE                      */
/*                                                                      */
/* Loads resource 0x3d8a via g_resmgr.GetById(), stores the result at   */
/* +0x284 (background_resource), then immediately calls vtable[1] on    */
/* the resource with args (0, 0) to get the surface handle, storing at  */
/* +0x280 (background_surface).                                          */
/*                                                                      */
/* NOTE: The binary does NOT check if GetById returns 0 or -1 before    */
/* dereferencing — if the resource were missing, this would crash.      */
/* In practice, resource 0x3d8a always exists.                          */
/* ================================================================== */
void PostcardPreviewWindow::init_background()
{
    if (this->background_resource != 0) {                    /* +0x284 */
        return;  /* Already initialized */
    }

    /* Load background resource (res 0x3d8a) */
    int32_t res = g_resmgr.GetById(0x3d8a);
    this->background_resource = (void*)(uintptr_t)res;                  /* +0x284 */

    /* Get surface via vtable[1] (ECX = res, stack args: 0, 0).
       NOTE: Binary dereferences EAX immediately — no null check.     */
    this->background_surface =                                /* +0x280 */
        ((void* (*)(void*, int, int))(*(void***)(uintptr_t)res)[1])((void*)(uintptr_t)res, 0, 0);
}

/* ================================================================== */
/* PostcardPreviewWindow::show — Show the preview dialog (vtable[2])    */
/* Address: 0x430C40 (__thiscall)                                       */
/*                                                                      */
/* Calls UI_WindowBase_Show to show window, then lazy-inits background. */
/* Called via vtable[2] dispatch from CGWND_InitMode1.                  */
/* ================================================================== */
void PostcardPreviewWindow::show()
{
    /* Call base class Show (vtable[2] inherited) */
    ((void (*)(void*))(*(void***)this)[2])(this);

    /* Lazy-init background resource */
    this->init_background();

    /* Mark sprites as created */
    this->sprites_created = 1;                               /* +0x27E */
}

/* ================================================================== */
/* PostcardPreviewWindow::cleanup — Hide dialog and destroy sprites     */
/* Address: 0x430E00 (__fastcall, ECX = this, vtable[1])                */
/*                                                                      */
/* Called via vtable[1] dispatch from vtable slot or scalar dtor.       */
/*                                                                      */
/* Release sequence:                                                    */
/*   1. Clear flag_27D                                                  */
/*   2. Hide window via UI_WindowBase_Hide                              */
/*   3. If netman+0x5C == 2: send network ack                           */
/*   4. Restore focus to main game window                               */
/*   5. Invalidate viewport rect                                        */
/*   6. Kill timer at +0xEC, clear timerId                              */
/*   7. If sprites_created: destroy surface-side of all 11 sprites      */
/*      via Sprite_Destroy, clear sprites_created                       */
/* ================================================================== */
void PostcardPreviewWindow::cleanup()
{
    this->flag_27D = 0;                                        /* +0x27D */

    /* Step 2: Hide the window */
    UI_WindowBase_Hide(this);

    /* Step 3: Send network ack if needed */
    if (*(int*)((uint8_t*)g_netman + 0x5C) == 2) {            /* netman session state */
        NETMAN_SendAck(g_netman);
    }

    /* Step 4: Restore focus to main game window */
    HWND main_hwnd = *(HWND*)((uint8_t*)g_main_window + 8);   /* hWnd at +0x08 */
    SetFocus(main_hwnd);

    /* Step 5: Invalidate viewport rect */
    TileMap_InvalidateRect(&g_tilemap,
                           g_viewport_rect_left, g_viewport_rect_top,
                           g_viewport_rect_right, g_viewport_rect_bottom);

    /* Step 6: Kill timer */
    if (this->timerId != 0) {                                  /* +0xEC */
        KillTimer(this->hWnd, this->timerId);
    }
    this->timerId = 0;                                         /* +0xEC */

    /* Step 7: Destroy sprites (surface side only) */
    if (this->sprites_created) {                               /* +0x27E */
        /* Release overlay resource 2 via vtable[2] */
        if (this->overlay_resource_2) {                        /* +0x28C */
            ((void (*)(void*))(*(void***)this->overlay_resource_2)[2])(
                this->overlay_resource_2);
        }
        this->overlay_surface_2 = 0;                           /* +0x288 */

        /* Release overlay resource 3 via vtable[2] */
        if (this->overlay_resource_3) {                        /* +0x294 */
            ((void (*)(void*))(*(void***)this->overlay_resource_3)[2])(
                this->overlay_resource_3);
        }
        this->overlay_surface_3 = 0;                           /* +0x290 */

        /* Destroy close button sprite (surface only) */
        Sprite_Destroy(this->sprite_close);                    /* +0x298 */

        /* Destroy options button sprite (surface only) */
        Sprite_Destroy(this->sprite_options);                  /* +0x2C0 */

        /* Destroy 9 status sprites (surface only) */
        for (int i = 0; i < 9; i++) {
            Sprite_Destroy(this->sprite_status[i]);            /* +0x29C+i*4 */
        }

        /* Clear sprites created flag */
        this->sprites_created = 0;                             /* +0x27E */
    }
}

/* ================================================================== */
/* PostcardPreviewWindow::destroy — Full destructor body (NOT vtable)  */
/* Address: 0x430C60 (__fastcall, ECX = this)                           */
/*                                                                      */
/* Called from: scalar deleting destructor                               */
/*                                                                      */
/* This is the full destructor body called from the scalar-deleting     */
/* destructor. It does everything Cleanup does PLUS frees the sprite    */
/* object memory by calling each sprite's scalar-deleting destructor    */
/* (vtable[0]) instead of just Sprite_Destroy (which only frees the    */
/* surface side). Then calls UI_WindowBase_BaseDtor for base cleanup.   */
/*                                                                      */
/* Release sequence:                                                    */
/*   1. Restore vtable to VTBL_POSTCARD_PREVIEW_WINDOW                   */
/*   2. Release background resource via vtable[2], clear surface        */
/*   3. If sprites_created: destroy all 11 sprites (surface + free)     */
/*   4. Free each sprite object via scalar-deleting dtor vtable[0]      */
/*   5. Call UI_WindowBase_BaseDtor                                     */
/* ================================================================== */
void PostcardPreviewWindow::destroy()
{
    /* Step 1: Restore vtable for correct dispatch during destruction */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Step 2: Release background resource */
    if (this->background_resource) {                           /* +0x284 */
        ((void (*)(void*))(*(void***)this->background_resource)[2])(
            this->background_resource);
        this->background_surface = 0;                          /* +0x280 */
    }

    /* Step 3: Destroy sprites (surface + object free) */
    if (this->sprites_created) {                               /* +0x27E */
        /* Release overlay resource 2 via vtable[2], clear surface */
        if (this->overlay_resource_2) {                        /* +0x28C */
            ((void (*)(void*))(*(void***)this->overlay_resource_2)[2])(
                this->overlay_resource_2);
        }
        this->overlay_surface_2 = 0;                           /* +0x288 */

        /* Release overlay resource 3 via vtable[2], clear surface */
        if (this->overlay_resource_3) {                        /* +0x294 */
            ((void (*)(void*))(*(void***)this->overlay_resource_3)[2])(
                this->overlay_resource_3);
        }
        this->overlay_surface_3 = 0;                           /* +0x290 */

        /* Destroy close button sprite (Sprite_Destroy — surface only) */
        Sprite_Destroy(this->sprite_close);                    /* +0x298 */

        /* Destroy options button sprite (Sprite_Destroy — surface only) */
        Sprite_Destroy(this->sprite_options);                  /* +0x2C0 */

        /* Destroy 9 status sprites (Sprite_Destroy — surface only) */
        for (int i = 0; i < 9; i++) {
            Sprite_Destroy(this->sprite_status[i]);            /* +0x29C+i*4 */
        }

        this->sprites_created = 0;                             /* +0x27E */
    }

    /* Step 4: Free each sprite object via scalar-deleting destructor */
    if (this->sprite_close) {                                  /* +0x298 */
        ((void* (*)(void*, byte))(*(void***)this->sprite_close)[0])(
            this->sprite_close, 1);
    }
    this->sprite_close = 0;

    if (this->sprite_options) {                                /* +0x2C0 */
        ((void* (*)(void*, byte))(*(void***)this->sprite_options)[0])(
            this->sprite_options, 1);
    }
    this->sprite_options = 0;

    for (int i = 0; i < 9; i++) {
        if (this->sprite_status[i]) {                          /* +0x29C+i*4 */
            ((void* (*)(void*, byte))(*(void***)this->sprite_status[i])[0])(
                this->sprite_status[i], 1);
        }
        this->sprite_status[i] = 0;
    }

    /* Step 5: Call base class destructor */
    UI_WindowBase_BaseDtor(this);
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
