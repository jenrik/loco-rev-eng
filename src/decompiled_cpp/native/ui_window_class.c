/**
 * ui_window_class.c — UI_Window class methods
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * UI_Window is a GameObject-derived class (vtable VTBL_UIENTITY, 0x477A90)
 * that wraps a UI overlay window with a tooltip child pointer at +0x98.
 *
 * Class layout (extends GameObject):
 *   +0x00..+0x97: GameObject base fields
 *   +0x98: tooltip_child (void*) — child tooltip/overlay reference
 *
 * Vtable: VTBL_UIENTITY (0x477A90)
 *   [0] scalar deleting destructor -> UI_DtorWrapper (0x4234E0)
 *   [1]..[14]: inherited from GameObject
 */

#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void  __cdecl GLOBAL_free(void* ptr);                    /* 0x465CD0 */
extern void  __thiscall GameObject_DtorBody(void* obj);         /* 0x405870 */
extern void  __thiscall GameObject_StopSound(void* obj, int a); /* 0x405A20 */
extern void  __thiscall GameObject_Update(void* obj);           /* 0x405C40 */
extern void  __fastcall CGWND_SetPause(void* obj, char pause);  /* 0x4061B0 */

void __fastcall UI_Window_Dtor(void* obj);

/* UI Manager functions */
extern void  __thiscall UI_DestroyTooltip(void* mgr, int* ptr); /* 0x423D20 */

/* Global tooltip manager singleton */
extern void* g_tooltip_mgr;   /* global UI Manager — defined in caller's scope */

/* ================================================================== */
/* UI_DtorWrapper — Scalar deleting destructor (vtable[0] @ VTBL_UIENTITY) */
/* Address: 0x4234E0                                                   */
/*                                                                     */
/* Calls UI_Window_Dtor body then optionally frees memory.             */
/* Despite Ghidra's name, this is NOT a WndProc wrapper. It is the     */
/* vtable[0] entry for VTBL_UIENTITY (0x477A90).                      */
/* ================================================================== */
void* __thiscall UI_DtorWrapper(void* obj, byte flags)
{
    UI_Window_Dtor(obj);

    if ((flags & 1) != 0) {
        GLOBAL_free(obj);
    }

    return obj;
}

/* ================================================================== */
/* UI_Window_Dtor — Destructor body for UI_Window                      */
/* Address: 0x423500                                                   */
/*                                                                     */
/* Resets vtable to VTBL_UIENTITY. Destroys tooltip child at +0x98     */
/* via UI_DestroyTooltip on the global tooltip manager.                */
/* Then calls GameObject_DtorBody on self.                             */
/* ================================================================== */
void __fastcall UI_Window_Dtor(void* obj)
{
    int** ptr = (int**)obj;

    /* Reset vtable for partial-destruction safety */
    *ptr = (int*)VTBL_UIENTITY;        /* 0x477A90 */

    /* Destroy tooltip child at +0x98 (word offset 0x26 = 38 decimal) */
    int* tooltip_child = (int*)((char*)obj + 0x98);
    if (tooltip_child != NULL && *tooltip_child != 0) {
        UI_DestroyTooltip(&g_tooltip_mgr, *tooltip_child);
    }

    /* Call GameObject base destructor */
    GameObject_DtorBody(obj);
}

/* ================================================================== */
/* UI_ShowWindow — Show child tooltip/window                            */
/* Address: 0x423840                                                   */
/*                                                                     */
/* If tooltip child at +0x98 exists, calls child->vtable[7] show       */
/* method (0x1C/4). Then calls GameObject_StopSound.                   */
/* ================================================================== */
void __thiscall UI_ShowWindow(void* obj, int param)
{
    int* tooltip_child = (int*)((char*)obj + 0x98);   /* +0x98 */

    if (tooltip_child != NULL && *tooltip_child != 0) {
        void** childVtab = (void**)*((void**)*tooltip_child);
        typedef void (__thiscall* ShowFunc)(void* self, int param);
        ShowFunc show = (ShowFunc)(childVtab[0x1C / 4]);  /* vtable[7] */
        show((void*)*tooltip_child, param);
    }

    GameObject_StopSound(obj, param);
}

/* ================================================================== */
/* UI_HideWindow — Hide child window                                    */
/* Address: 0x423870                                                   */
/*                                                                     */
/* If tooltip child at +0x98 exists, calls child->vtable[10] hide      */
/* method (0x28/4). Then calls GameObject_Update.                      */
/* ================================================================== */
void __fastcall UI_HideWindow(void* obj)
{
    int* tooltip_child = (int*)((char*)obj + 0x98);   /* +0x98 */

    if (tooltip_child != NULL && *tooltip_child != 0) {
        void** childVtab = (void**)*((void**)*tooltip_child);
        typedef void (__thiscall* HideFunc)(void* self);
        HideFunc hide = (HideFunc)(childVtab[0x28 / 4]);  /* vtable[10] */
        hide((void*)*tooltip_child);
    }

    GameObject_Update(obj);
}

/* ================================================================== */
/* UI_EnableWindow — Enable/disable child window                        */
/* Address: 0x423890                                                   */
/*                                                                     */
/* If tooltip child at +0x98 exists, calls child->vtable[9] enable     */
/* method (0x24/4). Then calls CGWND_SetPause.                         */
/* ================================================================== */
void __thiscall UI_EnableWindow(void* obj, int enable)
{
    int* tooltip_child = (int*)((char*)obj + 0x98);   /* +0x98 */

    if (tooltip_child != NULL && *tooltip_child != 0) {
        void** childVtab = (void**)*((void**)*tooltip_child);
        typedef void (__thiscall* EnableFunc)(void* self, int enable);
        EnableFunc enableFunc = (EnableFunc)(childVtab[0x24 / 4]);  /* vtable[9] */
        enableFunc((void*)*tooltip_child, enable);
    }

    CGWND_SetPause(obj, (char)enable);
}
