/**
 * ui_manager.c — UI Manager functions (tooltip, message box, timer lists)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions operate on the UI Manager singleton, a lightweight struct
 * that manages three TimerList sub-objects for tooltip and popup lifecycle:
 *   +0x00: vtable -> VTBL_UI_MANAGER (0x477AD0)
 *   +0x04: TimerList A (text_list)     — vtable VTBL_TIMERLIST_A   (0x477BD0)
 *   +0x1C: TimerList B (pos_list)      — vtable VTBL_TIMERLIST_B   (0x477B78)
 *   +0x34: TimerList C (update_list)   — vtable VTBL_TIMERLIST_C   (0x477B40), 100-entry array
 *
 * Each TimerList is a 0x18-byte dynamic array:
 *   +0x00: vtable (Timer_Resize interface)
 *   +0x04: data_ptr (allocated array of entries)
 *   +0x08: capacity (max entries)
 *   +0x0C: count (current entries)
 *   +0x10: (unknown)
 *   +0x14: (unknown)
 *
 * Timer_Resize (0x435D10) initializes the array to a given capacity.
 *
 * Called from: game logic, input handlers, sprite subsystem.
 */

#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Heap/CRT */
extern void*  __cdecl operator_new(size_t size);            /* 0x465CE0 */
extern void   __cdecl GLOBAL_free(void* ptr);               /* 0x465CD0 */
extern int    __fastcall Timer_Resize(void* timer, int count); /* 0x435D10 */

/* Resource manager */
extern int    __fastcall ResourceManager_GetById(void** mgr, int id); /* 0x460A30 */

/* UIEntity constructor */
extern void*  __thiscall UIEntity_Ctor(void* _this, int res_id,
                                       short param2, byte param3,
                                       int param4, int param5);       /* 0x447B60? */

/* Game-state globals */
extern void*  g_main_window;           /* defined by the SDL entry point */
extern double g_fps_limit;             /* 0x481170 (float read as double) */
extern void** g_resmgr;                /* 0x4855E8 */

/* GameObject base constructor */
extern int*   __thiscall GameObject_BaseCtor(void* _this, int a, int b,
                                            int c, int d);            /* 0x405790 */

/* Audio channel */
extern char   __thiscall CGWND_AudioChannel_IsActive(void* audio);    /* 0x40EEB0 */

/* Global game time */
extern uint32_t g_game_time;           /* 0x4A99B4 */
extern char __fastcall UI_Window_UpdateScroll(void* obj);

/* ================================================================== */
/* TimerList struct layout (0x18 bytes)                                */
/* ================================================================== */

/**
 * Each TimerList functions as a dynamic array with resize semantics.
 * vtable methods include (index * 4):
 *   [0] +0x00: scalar deleting destructor
 *   [3] +0x0C: get_by_index(index) -> item
 *   [4] +0x10: remove_by_index(index)
 *   [7] +0x1C: resize(new_count)
 *   [8] +0x20: get_item(index) -> item
 *   [11]+0x2C: get_count() -> int
 *   [19]+0x4C: resize_wrapper(a, b)
 *
 * Specific vtables (variants A/B/C) differ only in destructor linking.
 */

/* ================================================================== */
/* UI Manager struct layout                                            */
/* ================================================================== */
struct UI_Manager {
/* vtable at +0x00 is compiler-managed */
    /* TimerList A — text_list (tooltip text objects) */
    void*    tlA_vtable;          /* +0x04 -> VTBL_TIMERLIST_A (0x477BD0) */
    void*    tlA_data;            /* +0x08 */
    int      tlA_capacity;        /* +0x0C */
    int      tlA_count;           /* +0x10 */
    int      tlA_unk;             /* +0x14 */
    int      tlA_unk2;            /* +0x18 */
    /* TimerList B — pos_list (position objects) */
    void*    tlB_vtable;          /* +0x1C -> VTBL_TIMERLIST_B (0x477B78) */
    void*    tlB_data;            /* +0x20 */
    int      tlB_capacity;        /* +0x24 */
    int      tlB_count;           /* +0x28 */
    int      tlB_unk;             /* +0x2C */
    int      tlB_unk2;            /* +0x30 */
    /* TimerList C — update_list (scroll/animation objects, 100-entry array) */
    void*    tlC_vtable;          /* +0x34 -> VTBL_TIMERLIST_C (0x477B40) */
    void*    tlC_data;            /* +0x38 */
    int      tlC_capacity;        /* +0x3C */
    int      tlC_count;           /* +0x40 */
    int      tlC_unk;             /* +0x44 */
    int      tlC_unk2;            /* +0x48 */
};

/* ================================================================== */
/* UI Manager — Constructor                                            */
/* Address: 0x4238C0                                                   */
/*                                                                     */
/* Creates the UI Manager object (tooltip/message/timer manager).      */
/* Builds three TimerList sub-objects at +0x04, +0x1C, +0x34.         */
/* TimerList C gets a 100-entry (400 byte) data array.                 */
/* Sets final vtable to VTBL_UI_MANAGER (0x477AD0).                    */
/*                                                                     */
/* Called by: CGWND_InitAllSubsystems (0x406F90) to initialize the     */
/*            global UI Manager at g_tooltip_mgr.                      */
/* ================================================================== */
struct UI_Manager* __fastcall UI_Ctor(struct UI_Manager* mgr)
{
    void* data;
    int*  p;

    /* Initialize TimerList A at +0x04 (text_list) */
    mgr->tlA_vtable    = (void*)VTBL_TIMERLIST_A;   /* 0x477BD0 */
    mgr->tlA_capacity  = 0;
    mgr->tlA_data      = NULL;
    mgr->tlA_count     = 0;
    Timer_Resize(&mgr->tlA_vtable, 100);              /* resize to 100 entries */

    mgr->tlA_unk  = 0;                                /* +0x10 */
    mgr->tlA_vtable = (void*)VTBL_TIMERLIST_B;        /* +0x04, switch to variant B */
    mgr->tlA_unk2 = 0;                                /* +0x14 */
    /* Note: tlA_data and tlA_capacity now hold timer data */
    /* These fields overlap — Timer_Resize may relocate the data ptr */

    /* Initialize TimerList B at +0x1C (pos_list) */
    mgr->tlB_vtable    = (void*)VTBL_TIMERLIST_C;     /* 0x477B40 */
    mgr->tlB_capacity  = 0;
    mgr->tlB_data      = NULL;
    mgr->tlB_count     = 0;
    Timer_Resize(&mgr->tlB_vtable, 100);              /* resize to 100 entries */

    mgr->tlB_unk  = 0;                                /* +0x28 */
    mgr->tlB_vtable = (void*)VTBL_TIMERLIST_WRAPPER;  /* +0x1C, switch to wrapper */
    mgr->tlB_unk2 = 0;                                /* +0x2C */

    /* Initialize TimerList C at +0x34 (update_list, 100-entry static) */
    mgr->tlC_vtable    = (void*)VTBL_TIMERLIST_C;     /* 0x477B40 */
    mgr->tlC_capacity  = 0;
    mgr->tlC_data      = NULL;
    mgr->tlC_count     = 0;

    data = operator_new(400);                          /* 100 ints */
    mgr->tlC_data = data;                             /* +0x38 */

    /* Zero-fill the 100-entry array */
    for (int i = 0; i < 100; i++) {
        ((int*)data)[i] = 0;
    }

    /* Compute capacity: 100 if allocation succeeded, 0 if failed */
    mgr->tlC_capacity = (mgr->tlC_data != NULL) ? 100 : 0;  /* +0x3C */
    if (mgr->tlC_data == NULL) {
        mgr->tlC_data = NULL;                         /* +0x38 */
    }

    mgr->tlC_count = 0;                               /* +0x40 */
    mgr->tlC_vtable = (void*)VTBL_TIMERLIST_WRAPPER;  /* +0x34, switch to wrapper */
    mgr->tlC_unk  = 0;                                /* +0x44 */
    mgr->tlC_unk2 = 0;                                /* +0x48 */

    /* Set final vtable */
/* In the binary: mgr->vtable = VTBL_*. Compiler-managed in natural C++. */

    /* Finalize TimerList C by calling vtable[19]=vtable[0x4C/4] resize wrapper */
    {
        void** vtab = (void**)mgr->tlC_vtable;
        typedef void (__thiscall* ResizeWrapper)(void* self, int a, int b);
        ResizeWrapper rw = (ResizeWrapper)(vtab[0x4C / 4]);  /* vtable slot 19 */
        rw(&mgr->tlC_vtable, 0x0C, -4);
    }

    return mgr;
}

/* ================================================================== */
/* UI Manager — Reset/Cleanup                                          */
/* Address: 0x4239E0                                                   */
/*                                                                     */
/* Resets vtable and frees all three TimerList data buffers.           */
/* Called from UI_DestroyWindow (dtor wrapper) and full shutdown.      */
/* ================================================================== */
void __fastcall UI_ResetWindow(struct UI_Manager* mgr)
{
/* In the binary: mgr->vtable = VTBL_*. Compiler-managed in natural C++. */

    /* Free TimerList C (+0x34) */
    mgr->tlC_vtable = (void*)VTBL_TIMERLIST_C;
    mgr->tlC_count  = 0;
    if (mgr->tlC_data != NULL) {
        GLOBAL_free(mgr->tlC_data);
    }
    mgr->tlC_data     = NULL;
    mgr->tlC_capacity = 0;

    /* Free TimerList B (+0x1C) */
    mgr->tlB_vtable = (void*)VTBL_TIMERLIST_C;
    mgr->tlB_count  = 0;
    if (mgr->tlB_data != NULL) {
        GLOBAL_free(mgr->tlB_data);
    }
    mgr->tlB_data     = NULL;
    mgr->tlB_capacity = 0;

    /* Free TimerList A (+0x04) */
    mgr->tlA_vtable = (void*)VTBL_TIMERLIST_A;
    mgr->tlA_count  = 0;
    if (mgr->tlA_data != NULL) {
        GLOBAL_free(mgr->tlA_data);
    }
    mgr->tlA_data     = NULL;
    mgr->tlA_capacity = 0;
}

/* ================================================================== */
/* UI Manager — Scalar Deleting Destructor (vtable[0] of VTBL_UI_MANAGER) */
/* Address: 0x4239C0                                                   */
/*                                                                     */
/* Calls UI_ResetWindow to clean up, then optionally frees memory.     */
/* Despite Ghidra name "UI_DestroyWindow", _this is the UI Manager dtor.*/
/* ================================================================== */
struct UI_Manager* __thiscall UI_DestroyWindow(struct UI_Manager* mgr, byte flags)
{
    UI_ResetWindow(mgr);
    if ((flags & 1) != 0) {
        GLOBAL_free(mgr);
    }
    return mgr;
}

/* ================================================================== */
/* UI_FreeTooltipManager — Free all 3 timer sub-objects                */
/* Address: 0x423A90                                                   */
/*                                                                     */
/* Calls each timer's vtable+0x18 cleanup method to free internal data.*/
/* Called once during CGWND_Cleanup teardown.                          */
/* ================================================================== */
void __fastcall UI_FreeTooltipManager(struct UI_Manager* mgr)
{
    /* Free TimerList B (+0x1C) */
    {
        void** vtab = (void**)mgr->tlB_vtable;
        typedef void (__thiscall* Cleanup)(void* self);
        Cleanup cl = (Cleanup)(vtab[0x18 / 4]);   /* vtable slot 6 */
        cl(&mgr->tlB_vtable);
    }
    /* Free TimerList C (+0x34) */
    {
        void** vtab = (void**)mgr->tlC_vtable;
        typedef void (__thiscall* Cleanup)(void* self);
        Cleanup cl = (Cleanup)(vtab[0x18 / 4]);
        cl(&mgr->tlC_vtable);
    }
    /* Free TimerList A (+0x04) */
    {
        void** vtab = (void**)mgr->tlA_vtable;
        typedef void (__thiscall* Cleanup)(void* self);
        Cleanup cl = (Cleanup)(vtab[0x18 / 4]);
        cl(&mgr->tlA_vtable);
    }
}

/* ================================================================== */
/* UI_CleanupTooltips — Partial cleanup of TimerLists B and C only     */
/* Address: 0x423D00                                                   */
/*                                                                     */
/* Frees TimerLists B and C but NOT A (text_list).                     */
/* Used during sprite resets (Sprite_UnlockAll, INPUT_NewWorld).       */
/* ================================================================== */
void __fastcall UI_CleanupTooltips(struct UI_Manager* mgr)
{
    /* Free TimerList B (+0x1C) */
    {
        void** vtab = (void**)mgr->tlB_vtable;
        typedef void (__thiscall* Cleanup)(void* self);
        Cleanup cl = (Cleanup)(vtab[0x18 / 4]);
        cl(&mgr->tlB_vtable);
    }
    /* Free TimerList C (+0x34) */
    {
        void** vtab = (void**)mgr->tlC_vtable;
        typedef void (__thiscall* Cleanup)(void* self);
        Cleanup cl = (Cleanup)(vtab[0x18 / 4]);
        cl(&mgr->tlC_vtable);
    }
    /* TimerList A (+0x04) is intentionally NOT freed */
}

/* ================================================================== */
/* UI_CreateMessageBox — Create a world-positioned message box popup   */
/* Address: 0x423AB0                                                   */
/*                                                                     */
/* Creates a UIEntity-based message box popup at world position.       */
/* FPS-gated: skips if current FPS <= threshold (unless resource       */
/* 0x3861 which bypasses the gate).                                    */
/*                                                                     */
/* Validates resource availability:                                    */
/*   1. Looks up the primary resource (param_1) in ResourceManager     */
/*   2. Checks if resource has available slots (usage < max)           */
/*   3. Optionally validates shadow/primary dependencies               */
/*                                                                     */
/* On success: allocates 0xA4-byte UIEntity, adds to parent's          */
/* timer list at +0x1C (if param_6 != 0) or +0x34 (otherwise).        */
/*                                                                     */
/* @param mgr       UI Manager (tooltip context, _this=ECX)             */
/* @param res_id    Primary resource ID for the message box            */
/* @param sub_id    Sub-type/anim index (short)                        */
/* @param msg_type  Message type char ('W' = world, etc.)              */
/* @param world_x   World X position                                   */
/* @param world_y   World Y position                                   */
/* @param add_to_pos_list  If non-zero, add to pos_list (+0x1C);       */
/*                         otherwise, add to update_list (+0x34)       */
/* @return           Pointer to the created UIEntity, or NULL on fail  */
/* ================================================================== */
void* __thiscall UI_CreateMessageBox(struct UI_Manager* mgr,
                                      int res_id, short sub_id,
                                      char msg_type, int world_x, int world_y,
                                      char add_to_pos_list)
{
    /* FPS gate: skip if FPS <= limit (unless resource 0x3861) */
    byte* fps_ptr = (byte*)g_main_window + 0x14;
    if ((double)(int)*fps_ptr <= g_fps_limit && res_id != 0x3861) {
        return NULL;
    }

    /* Validate primary resource */
    int* primary_res = ResourceManager_GetById(&g_resmgr, res_id);
    if (primary_res == NULL) {
        return NULL;
    }

    /* Check if resource has available slots */
    unsigned short usage = *(unsigned short*)(primary_res + 0x158 / 4); /* +0x158 */
    unsigned int max = *(unsigned int*)(primary_res + 0x15C / 4);       /* +0x15C */
    if (usage >= max) {
        return NULL;
    }

    /* Check shadow resource dependency at +0x40 */
    int shadow_id = *(int*)((char*)primary_res + 0x40);
    if (shadow_id != -1) {
        int* shadow_res = ResourceManager_GetById(&g_resmgr, shadow_id);
        if (shadow_res == NULL || *(short*)(shadow_res + 0x158 / 4) == 0) {
            return NULL;
        }
    }

    /* Check primary resource dependency at +0x44 */
    int dep_id = *(int*)((char*)primary_res + 0x44);
    if (dep_id != -1) {
        int* dep_res = ResourceManager_GetById(&g_resmgr, dep_id);
        if (dep_res == NULL || *(short*)(dep_res + 0x158 / 4) == 0) {
            return NULL;
        }
    }

    /* Allocate and construct UIEntity (0xA4 bytes) */
    void* entity = operator_new(0xA4);
    if (entity == NULL) {
        return NULL;
    }

    void* result = UIEntity_Ctor(entity, res_id, sub_id, msg_type, world_x, world_y);
    if (result == NULL) {
        return NULL;
    }

    /* Check init success flag at +0x18 (word offset 6) */
    if (*(char*)((int)result + 0x18) == 1) {
        /* Success — add to appropriate timer list */
        int* mgrInt = (int*)mgr;
        void* timerList;

        if (add_to_pos_list != 0) {
            /* Add to pos_list (+0x1C) */
            timerList = (void*)(mgrInt + 0x1C / 4);
        } else {
            /* Add to update_list (+0x34) */
            timerList = (void*)(mgrInt + 0x34 / 4);
        }

        /* Call timerlist->vtable[13] = vtable[0x34/4] to add item */
        void** tlVtab = *(void***)timerList;
        typedef void (__thiscall* AddFunc)(void* list, void* item);
        AddFunc add = (AddFunc)(tlVtab[0x34 / 4]);   /* slot 13 */
        add(timerList, result);

        return result;
    }

    /* Init failed — destroy the entity via vtable[0] */
    if (result != NULL) {
        void** vtab = *(void***)result;
        typedef void* (__thiscall* DtorFunc)(void* self, byte flags);
        DtorFunc dtor = (DtorFunc)(vtab[0]);
        dtor(result, 1);
    }

    return NULL;
}

/* ================================================================== */
/* UI_CreateTooltip — Create a tooltip object                          */
/* Address: 0x423C50                                                   */
/*                                                                     */
/* Allocates a 0x88-byte tooltip (GameObject-based). Calls             */
/* GameObject_BaseCtor, sets screen position via vtable+0x0C,          */
/* sets flag bit 0x02 at +0x2C, adds to parent text_list (+0x04).     */
/*                                                                     */
/* @param _this    UI Manager (tooltip context)                         */
/* @param res_id  Tooltip resource ID                                  */
/* @param param3  Type/depth parameter (short)                         */
/* @param x       Screen X position                                    */
/* @param y       Screen Y position                                    */
/* @return        Pointer to tooltip, or NULL on failure               */
/* ================================================================== */
int* __thiscall UI_CreateTooltip(struct UI_Manager* mgr, int res_id,
                                  short param3, int x, int y)
{
    int* tooltip;

    tooltip = (int*)operator_new(0x88);                   /* 136-byte tooltip */
    if (tooltip == NULL) {
        return NULL;
    }

    tooltip = GameObject_BaseCtor(tooltip, res_id, param3, 0, 0);

    if (tooltip != NULL) {
        /* Check init success flag (+0x18, byte at word offset 6) */
        if (*((char*)tooltip + 0x18) == 1) {
            /* Set screen position via vtable[3] (slot 0x0C/4) */
            {
                void** vtab = (void**)*((void**)tooltip);
                typedef void (__thiscall* SetPosFunc)(void* self, int x, int y);
                SetPosFunc setPos = (SetPosFunc)(vtab[3]);
                setPos(tooltip, x, y);
            }

            /* Set flag bit 0x02 at +0x2C (word offset 11 -> byte offset 0x2C
               which is piVar1[0xB] in int* indexing) */
            tooltip[0xB] |= 2;

            /* Add to parent text_list (+0x04): call vtable[13] = vtable[0x34/4] */
            {
                void** tlVtab = (void**)mgr->tlA_vtable;
                typedef void (__thiscall* AddFunc)(void* list, void* item);
                AddFunc add = (AddFunc)(tlVtab[0x34 / 4]);   /* slot 13 */
                add(&mgr->tlA_vtable, tooltip);
            }
        } else {
            /* Init failed — destroy tooltip via vtable[0] */
            {
                void** vtab = (void**)*((void**)tooltip);
                typedef void* (__thiscall* DtorFunc)(void* self, byte flags);
                DtorFunc dtor = (DtorFunc)(vtab[0]);
                dtor(tooltip, 1);
            }
            return NULL;
        }
    }

    return tooltip;
}

/* ================================================================== */
/* UI_DestroyTooltip — Remove tooltip from text_list                   */
/* Address: 0x423D20                                                   */
/*                                                                     */
/* Searches text_list (+0x04) for tooltip_ptr and removes it via       */
/* vtable[4] (remove_by_index). Called when owning entity destroyed.   */
/*                                                                     */
/* @param _this   UI Manager                                           */
/* @param ptr    Tooltip pointer to destroy                            */
/* ================================================================== */
void __thiscall UI_DestroyTooltip(struct UI_Manager* mgr, int* tooltip_ptr)
{
    int count;
    unsigned int i;

    void** tlVtab = (void**)mgr->tlA_vtable;  /* text_list at +0x04 */

    /* Get count via vtable[11] */
    typedef int (__thiscall* GetCountFunc)(void* self);
    GetCountFunc getCount = (GetCountFunc)(tlVtab[0x2C / 4]);
    count = getCount(&mgr->tlA_vtable);

    if (count != 0) {
        for (i = 0; ; i++) {
            /* Get item at index via vtable[8] */
            typedef int* (__thiscall* GetItemFunc)(void* self, unsigned int index);
            GetItemFunc getItem = (GetItemFunc)(tlVtab[0x20 / 4]);
            int* item = getItem(&mgr->tlA_vtable, i);

            if (item == tooltip_ptr) {
                /* Found — remove via vtable[4] */
                typedef void (__thiscall* RemoveFunc)(void* self, unsigned int index);
                RemoveFunc remove = (RemoveFunc)(tlVtab[0x10 / 4]);
                remove(&mgr->tlA_vtable, i);
                return;
            }

            count = getCount(&mgr->tlA_vtable);
            if ((unsigned int)count <= i) {
                return;
            }
        }
    }
}

/* ================================================================== */
/* UI_SetTooltipText — Set rect/flags on all tooltips in text_list     */
/* Address: 0x423E00                                                   */
/*                                                                     */
/* Iterates text_list (+0x04). For each initialized+visible tooltip,   */
/* calls vtable[11] (SetRectAndFlags) with rect and flags.             */
/* ================================================================== */
void __thiscall UI_SetTooltipText(struct UI_Manager* mgr, int left, int top,
                                   int right, int bottom)
{
    unsigned int i;
    int count;

    void** tlVtab = (void**)mgr->tlA_vtable;  /* text_list at +0x04 */

    typedef int (__thiscall* GetCountFunc)(void* self);
    GetCountFunc getCount = (GetCountFunc)(tlVtab[0x2C / 4]);
    count = getCount(&mgr->tlA_vtable);

    if (count != 0) {
        for (i = 0; ; i++) {
            typedef int* (__thiscall* GetItemFunc)(void* self, unsigned int index);
            GetItemFunc getItem = (GetItemFunc)(tlVtab[0x20 / 4]);
            int* item = getItem(&mgr->tlA_vtable, i);

            if (item != NULL) {
                /* Check init flag at +0x18 (byte at word offset 6) */
                if (*((char*)item + 0x18) == 1) {
                    /* Check visible flag at +0x24 (byte at word offset 9) */
                    if (*((char*)item + 0x24) == 1) {
                        /* Call vtable[11] = 0x2C/4 = SetRectAndFlags */
                        void** itemVtab = (void**)*((void**)item);
                        typedef void (__thiscall* SetRectFunc)(void* self, int a,
                                                               int b, int c, int d, int flags);
                        SetRectFunc srf = (SetRectFunc)(itemVtab[0x2C / 4]);
                        srf(item, left, top, right, bottom, item[0xB]);
                    }
                }
            }

            count = getCount(&mgr->tlA_vtable);
            if ((unsigned int)count <= i) {
                return;
            }
        }
    }
}

/* ================================================================== */
/* UI_SetTooltipPos — Set rect/flags on all tooltips in pos_list       */
/* Address: 0x423E80                                                   */
/*                                                                     */
/* Mirrors UI_SetTooltipText but operates on pos_list (+0x1C).         */
/* ================================================================== */
void __thiscall UI_SetTooltipPos(struct UI_Manager* mgr, int left, int top,
                                  int right, int bottom)
{
    unsigned int i;
    int count;

    void** tlVtab = (void**)mgr->tlB_vtable;  /* pos_list at +0x1C */

    typedef int (__thiscall* GetCountFunc)(void* self);
    GetCountFunc getCount = (GetCountFunc)(tlVtab[0x2C / 4]);
    count = getCount(&mgr->tlB_vtable);

    if (count != 0) {
        for (i = 0; ; i++) {
            typedef int* (__thiscall* GetItemFunc)(void* self, unsigned int index);
            GetItemFunc getItem = (GetItemFunc)(tlVtab[0x20 / 4]);
            int* item = getItem(&mgr->tlB_vtable, i);

            if (item != NULL) {
                if (*((char*)item + 0x18) == 1 && *((char*)item + 0x24) == 1) {
                    void** itemVtab = (void**)*((void**)item);
                    typedef void (__thiscall* SetRectFunc)(void* self, int a, int b,
                                                           int c, int d, int flags);
                    SetRectFunc srf = (SetRectFunc)(itemVtab[0x2C / 4]);
                    srf(item, left, top, right, bottom, item[0xB]);
                }
            }

            count = getCount(&mgr->tlB_vtable);
            if ((unsigned int)count <= i) {
                return;
            }
        }
    }
}

/* ================================================================== */
/* UI_UpdateTooltip — Set rect/flags on all tooltips in update_list    */
/* Address: 0x423F00                                                   */
/*                                                                     */
/* Mirrors UI_SetTooltipText but operates on update_list (+0x34).      */
/* Called from TileMap_ProcessRect after SetTooltipText.               */
/* ================================================================== */
void __thiscall UI_UpdateTooltip(struct UI_Manager* mgr, int left, int top,
                                  int right, int bottom)
{
    unsigned int i;
    int count;

    void** tlVtab = (void**)mgr->tlC_vtable;  /* update_list at +0x34 */

    typedef int (__thiscall* GetCountFunc)(void* self);
    GetCountFunc getCount = (GetCountFunc)(tlVtab[0x2C / 4]);
    count = getCount(&mgr->tlC_vtable);

    if (count != 0) {
        for (i = 0; ; i++) {
            typedef int* (__thiscall* GetItemFunc)(void* self, unsigned int index);
            GetItemFunc getItem = (GetItemFunc)(tlVtab[0x20 / 4]);
            int* item = getItem(&mgr->tlC_vtable, i);

            if (item != NULL) {
                if (*((char*)item + 0x18) == 1 && *((char*)item + 0x24) == 1) {
                    void** itemVtab = (void**)*((void**)item);
                    typedef void (__thiscall* SetRectFunc)(void* self, int a, int b,
                                                           int c, int d, int flags);
                    SetRectFunc srf = (SetRectFunc)(itemVtab[0x2C / 4]);
                    srf(item, left, top, right, bottom, item[0xB]);
                }
            }

            count = getCount(&mgr->tlC_vtable);
            if ((unsigned int)count <= i) {
                return;
            }
        }
    }
}

/* ================================================================== */
/* UI_ResetTooltips — Reset/hide all tooltips                          */
/* Address: 0x423F80                                                   */
/*                                                                     */
/* Iterates update_list (+0x34) then pos_list (+0x1C).                 */
/* Calls vtable[9] (0x24/4) on each non-null item to reset/hide.      */
/* ================================================================== */
void __thiscall UI_ResetTooltips(struct UI_Manager* mgr, int param)
{
    unsigned int i;
    int count;

    /* Process update_list (+0x34) first */
    {
        void** tlVtab = (void**)mgr->tlC_vtable;

        typedef int (__thiscall* GetCountFunc)(void* self);
        GetCountFunc getCount = (GetCountFunc)(tlVtab[0x2C / 4]);
        count = getCount(&mgr->tlC_vtable);

        if (count != 0) {
            for (i = 0; ; i++) {
                typedef int* (__thiscall* GetItemFunc)(void* self, unsigned int index);
                GetItemFunc getItem = (GetItemFunc)(tlVtab[0x20 / 4]);
                int* item = getItem(&mgr->tlC_vtable, i);

                if (item != NULL) {
                    void** itemVtab = (void**)*((void**)item);
                    typedef void (__thiscall* ResetFunc)(void* self, int param);
                    ResetFunc reset = (ResetFunc)(itemVtab[0x24 / 4]);  /* vtable[9] */
                    reset(item, param);
                }

                count = getCount(&mgr->tlC_vtable);
                if ((unsigned int)count <= i) {
                    break;
                }
            }
        }
    }

    /* Process pos_list (+0x1C) */
    {
        void** tlVtab = (void**)mgr->tlB_vtable;

        typedef int (__thiscall* GetCountFunc)(void* self);
        GetCountFunc getCount = (GetCountFunc)(tlVtab[0x2C / 4]);
        count = getCount(&mgr->tlB_vtable);

        if (count != 0) {
            for (i = 0; ; i++) {
                typedef int* (__thiscall* GetItemFunc)(void* self, unsigned int index);
                GetItemFunc getItem = (GetItemFunc)(tlVtab[0x20 / 4]);
                int* item = getItem(&mgr->tlB_vtable, i);

                if (item != NULL) {
                    void** itemVtab = (void**)*((void**)item);
                    typedef void (__thiscall* ResetFunc)(void* self, int param);
                    ResetFunc reset = (ResetFunc)(itemVtab[0x24 / 4]);
                    reset(item, param);
                }

                count = getCount(&mgr->tlB_vtable);
                if ((unsigned int)count <= i) {
                    break;
                }
            }
        }
    }
}

/* ================================================================== */
/* UI_HideTooltip — Per-frame tooltip animation tick                   */
/* Address: 0x423D70                                                   */
/*                                                                     */
/* Per-frame maintenance: iterates update_list (+0x34) and             */
/* pos_list (+0x1C), calls UI_Window_UpdateScroll on each tooltip.     */
/* Removes completed animations (UpdateScroll returns 1).              */
/* Despite name, _this is an animation tick, not a hide operation.      */
/* ================================================================== */
void __fastcall UI_HideTooltip(struct UI_Manager* mgr)
{
    unsigned int i;
    int count;

    /* Process update_list (+0x34) */
    {
        void** tlVtab = (void**)mgr->tlC_vtable;  /* update_list */

        typedef int (__thiscall* GetCountFunc)(void* self);
        GetCountFunc getCount = (GetCountFunc)(tlVtab[0x2C / 4]);
        count = getCount(&mgr->tlC_vtable);

        if (count != 0) {
            for (i = 0; ; i++) {
                typedef int* (__thiscall* GetItemFunc)(void* self, unsigned int index);
                GetItemFunc getItem = (GetItemFunc)(tlVtab[0x20 / 4]);
                int* item = getItem(&mgr->tlC_vtable, i);

                if (item != NULL) {
                    char completed = UI_Window_UpdateScroll(item);
                    if (completed == 1) {
                        /* Remove completed animation via vtable[4] */
                        typedef void (__thiscall* RemoveFunc)(void* self, unsigned int index);
                        RemoveFunc remove = (RemoveFunc)(tlVtab[0x10 / 4]);
                        remove(&mgr->tlC_vtable, i);
                    }
                }

                count = getCount(&mgr->tlC_vtable);
                if ((unsigned int)count <= i) {
                    break;
                }
            }
        }
    }

    /* Process pos_list (+0x1C) */
    {
        void** tlVtab = (void**)mgr->tlB_vtable;  /* pos_list */

        typedef int (__thiscall* GetCountFunc)(void* self);
        GetCountFunc getCount = (GetCountFunc)(tlVtab[0x2C / 4]);
        count = getCount(&mgr->tlB_vtable);

        if (count != 0) {
            for (i = 0; ; i++) {
                typedef int* (__thiscall* GetItemFunc)(void* self, unsigned int index);
                GetItemFunc getItem = (GetItemFunc)(tlVtab[0x20 / 4]);
                int* item = getItem(&mgr->tlB_vtable, i);

                if (item != NULL) {
                    char completed = UI_Window_UpdateScroll(item);
                    if (completed == 1) {
                        typedef void (__thiscall* RemoveFunc)(void* self, unsigned int index);
                        RemoveFunc remove = (RemoveFunc)(tlVtab[0x10 / 4]);
                        remove(&mgr->tlB_vtable, i);
                    }
                }

                count = getCount(&mgr->tlB_vtable);
                if ((unsigned int)count <= i) {
                    break;
                }
            }
        }
    }
}
