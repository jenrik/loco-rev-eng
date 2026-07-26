/**
 * GameView.cpp — GameView class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file contains the GameView (TownGameView / ScrollView) helper class
 * implementation. GameView handles viewport scrolling and camera position
 * within a Town scene. It manages scroll offsets, an embedded GameObject
 * sub-object, and cleanup of child resources.
 */

#include "GameView.h"
#include "../shared/vtable_addrs.h"
/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */
void __fastcall GameView_DtorBody_S(void* obj);

/* ================================================================== */
/* External references (C-linkage from Win32 and other modules)        */
/* ================================================================== */

/* CRT memory */
void   GLOBAL_free(void* ptr);

    /* RESDATA base */
    void   RESDATA_BaseInit(void* self);                             /* 0x4544E0 */
    void   RESDATA_DtorBase(void* self);                             /* 0x454630 */

    /* GameObject */
    void   GameObject_BaseCtor(void* obj, int x, int y, int w, int h); /* 0x405790 */
    void   GameObject_DtorBody(void* obj);                            /* 0x405870 */

/* Panel */
void   Panel_DtorBody(void* obj);                                 /* 0x4545A0 */

/* ================================================================== */
/* GameView_Ctor — Free function constructor wrapper                    */
/* Address: 0x42CCE0 (__fastcall, ECX = this)                           */
/*                                                                      */
/* The original binary treats this as a free C function called with     */
/* ECX set to the allocation. We call it GameView_Ctor since it is      */
/* effectively the constructor but registered as a free function        */
/* rather than a class method.                                          */
/* ================================================================== */
void* __fastcall GameView_Ctor(void* obj)
{
    /* Step 1: Initialize RESDATA base */
    RESDATA_BaseInit(obj);

    /* Step 2: Create GameObject sub-object at +0xE4 */
    /* GameObject_BaseCtor(self, x=-1, y=-1, width=0, height=0) */
    GameObject_BaseCtor((uint8_t*)obj + 0xE4, -1, -1, 0, 0);

    /* Step 3: Set vtable */
    *(void**)obj = (void*)VTBL_TOWN_GAMEVIEW;         /* 0x477D30 */

    /* Step 4: Set type to 14 (0x0E) */
    *(int32_t*)((uint8_t*)obj + 4) = 14;

    /* Step 5: Initialize scroll offsets */
    *(int32_t*)((uint8_t*)obj + 0xE0) = 0;             /* scroll_x */
    *(int32_t*)((uint8_t*)obj + 0x17C) = 0;            /* scroll_y */

    /* Step 6: Set scroll active flag */
    *(uint8_t*)((uint8_t*)obj + 0xAD) = 1;             /* scroll_active_flag */

    return obj;
}

/* ================================================================== */
/* GameView_Dtor — Scalar deleting destructor (vtable[0])               */
/* Address: 0x42CD60 (__thiscall)                                       */
/* ================================================================== */
void* __thiscall GameView_Dtor(void* obj, byte flags)
{
    GameView_DtorBody_S(obj);
    if (flags & 1) {
        GLOBAL_free(obj);
    }
    return obj;
}

/**
 * GameView_DtorBody_S — Static helper for the destructor body.
 * Address: 0x42CD80 (__fastcall, ECX = this)
 *
 * This is named with _S suffix to avoid linker conflicts with
 * the C++ method name. The original binary uses a free function.
 */
void __fastcall GameView_DtorBody_S(void* obj)
{
    /* Step 1: Restore vtable for correct dispatch during destruction */
    *(void**)obj = (void*)VTBL_TOWN_GAMEVIEW;           /* 0x477D30 */

    /* Step 2: Destroy embedded GameObject sub-object at +0xE4 */
    GameObject_DtorBody((uint8_t*)obj + 0xE4);

    /* Step 3: Call Panel base destructor */
    Panel_DtorBody(obj);
}

/* ================================================================== */
/* GameView_Cleanup — Release child resources and call base cleanup     */
/* Address: 0x42CDD0 (__fastcall, ECX = this)                           */
/* vtable[15] at +0x3C (GameView-specific cleanup slot)                 */
/*                                                                      */
/* Called from: CGWND_Cleanup directly on g_town_view                   */
/* ================================================================== */
void __fastcall GameView_Cleanup(void* obj)
{
    /* Step 1: Destroy child reference at +0x17C via vtable[0] */
    void* child = *(void**)((uint8_t*)obj + 0x17C);
    if (child != 0) {
        ((void (*)(void*, int))(*(void***)child)[0])(child, 1);
    }

    /* Step 2: Invalidate/cleanup GameObject sub-object at +0xE4 via vtable[6] */
    void* game_obj = (uint8_t*)obj + 0xE4;
    ((void (*)(void*, int, int, int))(
        *(void***)game_obj)[6])(game_obj, 0, -1, 0);

    /* Step 3: Invalidate/cleanup self via vtable[6] */
    ((void (*)(void*, int, int, int))(
        *(void***)obj)[6])(obj, 0, -1, 0);

    /* Step 4: Call RESDATA base destructor */
    RESDATA_DtorBase(obj);
}
