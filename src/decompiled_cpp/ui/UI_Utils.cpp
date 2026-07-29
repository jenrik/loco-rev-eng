/**
 * UI_Utils.cpp — UI_Manager, tooltip, and helper implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "UI_Utils.h"
#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void* operator_new(size_t size);                    /* 0x465CE0 — malloc wrapper */
    void  GLOBAL_free(void* ptr);                       /* 0x465CD0 — free wrapper   */
extern "C" {
    void  __stdcall DefWindowProcA(HWND hWnd, UINT Msg,
                                   void* wParam, void* lParam);  /* USER32 */
}
    void  __fastcall GameObject_StopSound(void* self, int param); /* 0x405A20 */
    void  __fastcall GameObject_Update(void* self);              /* 0x436AB0 */
    void  __fastcall CGWND_SetPause(void* self, char pause);     /* 0x408130 */
    void  __thiscall GameObject_BaseCtor(void* self, int a, int b,
                                          int c, int d);          /* 0x405790 */
    int   __thiscall ResourceManager_GetById(void** mgr, int id); /* 0x460A30 */

/* External declarations from other modules */
extern void* g_main_window;              /* 0x4AA4A0 */
extern void* g_resmgr;                   /* 0x4855E8 */

/* Global tooltip manager pointer (set externally) */
extern void* g_tooltip_mgr;              /* 0x4FD220 */

/* FPS gate threshold for CreateMessageBox */
extern double DAT_00481170;              /* 0x481170 — FPS threshold */

/* The common resize implementation used by the binary's UI collection
 * template instantiations. */
extern void Timer_Resize(void* collection, unsigned capacity); /* 0x435D10 */

void UITimerList::Resize(uint32_t new_capacity)
{
    Timer_Resize(this, new_capacity);
}

void* UITimerList::GetItem(uint32_t index) const
{
    return items[index];
}

uint32_t UITimerList::GetCount() const
{
    return count;
}

/* ================================================================== */
/* UI_DefWndProc — Default passthrough WindowProc                      */
/* Address: 0x422EA0                                                    */
/* ================================================================== */
void __stdcall UI_DefWndProc(HWND hWnd, UINT msg, void* wParam, void* lParam)
{
    DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ================================================================== */
/* UI_ShowWindow — Show tooltip child window                            */
/* Address: 0x423840 (used as vtable[7] of some class)                  */
/* ================================================================== */
void __thiscall UI_ShowWindow(void* self, int param)
{
    /* Access child/tooltip object at +0x98 */
    void* child = *(void**)((uint8_t*)self + 0x98);
    if (child != NULL) {
        /* Call child's vtable[7] (offset 0x1C) — show method */
        void** vtbl = *(void***)child;
        typedef void (__thiscall* ShowFn)(void*, int);
        ShowFn showFn = (ShowFn)vtbl[7];
        showFn(child, param);
    }
    /* Call GameObject_StopSound on self */
    GameObject_StopSound(self, param);
}

/* ================================================================== */
/* UI_HideWindow — Hide tooltip child window                           */
/* Address: 0x423870 (used as vtable[10] of some class)                */
/* ================================================================== */
void __fastcall UI_HideWindow(void* self)
{
    int** selfInt = (int**)self;

    /* Access child/tooltip object at index 0x26 (+0x98) */
    int* child = selfInt[0x26];  /* selfInt[0x26] = *(void**)(self + 0x98) */
    if (child != NULL) {
        /* Call child's vtable[10] (offset 0x28) — hide method */
        void** vtbl = *(void***)child;
        typedef void (__fastcall* HideFn)(void*);
        HideFn hideFn = (HideFn)vtbl[10];
        hideFn(child);
    }
    /* Call GameObject_Update on self */
    GameObject_Update(self);
}

/* ================================================================== */
/* UI_EnableWindow — Enable/disable tooltip child window               */
/* Address: 0x423890 (used as vtable[9] of some class)                 */
/* ================================================================== */
void __thiscall UI_EnableWindow(void* self, int enable)
{
    /* Access child/tooltip object at +0x98 */
    void* child = *(void**)((uint8_t*)self + 0x98);
    if (child != NULL) {
        /* Call child's vtable[9] (offset 0x24) — enable method */
        void** vtbl = *(void***)child;
        typedef void (__thiscall* EnableFn)(void*, int);
        EnableFn enableFn = (EnableFn)vtbl[9];
        enableFn(child, enable);
    }
    /* Call CGWND_SetPause on self */
    CGWND_SetPause(self, (char)enable);
}

/* ================================================================== */
/* UI_Manager Constructor                                              */
/* Address: 0x4238C0 (called from CGWND_InitAllSubsystems)              */
/* ================================================================== */
UI_Manager::UI_Manager()
{
    text_list.items = NULL;
    text_list.capacity = 0;
    text_list.count = 0;
    text_list.Resize(100);
    text_list.key_offset = 0;
    text_list.key_size = 0;

    pos_list.items = NULL;
    pos_list.capacity = 0;
    pos_list.count = 0;
    pos_list.Resize(100);
    pos_list.key_offset = 0;
    pos_list.key_size = 0;

    update_list.items = NULL;
    update_list.capacity = 0;
    update_list.count = 0;
    update_list.Resize(100);
    update_list.key_offset = 0x0C;
    update_list.key_size = -4;
}

/* ================================================================== */
/* UI_Manager::scalar deleting destructor                              */
/* Address: 0x4239C0                                                    */
/* ================================================================== */
UI_Manager::~UI_Manager()
{
    this->reset();
}

/* ================================================================== */
/* UI_Manager::reset (base destructor)                                  */
/* Address: 0x4239E0                                                    */
/* ================================================================== */
void UI_Manager::reset()
{
    UITimerList* lists[] = { &update_list, &pos_list, &text_list };
    for (unsigned i = 0; i < 3; ++i) {
        if (lists[i]->items != NULL) {
            GLOBAL_free(lists[i]->items);
        }
        lists[i]->items = NULL;
        lists[i]->capacity = 0;
        lists[i]->count = 0;
        lists[i]->key_offset = 0;
        lists[i]->key_size = 0;
    }
}

/* ================================================================== */
/* UI_Manager::freeTooltipManager                                      */
/* Address: 0x423A90                                                    */
/* ================================================================== */
void UI_Manager::freeTooltipManager()
{
    /* Call vtable+0x18 on each timer (pos_list first, then update, then text) */
    {
        void** vtbl = *(void***)&this->pos_list;
        typedef void (__thiscall* CleanupFn)(void*);
        CleanupFn cleanup = (CleanupFn)vtbl[6];
        cleanup(&this->pos_list);
    }

    {
        void** vtbl = *(void***)&this->update_list;
        typedef void (__thiscall* CleanupFn)(void*);
        CleanupFn cleanup = (CleanupFn)vtbl[6];
        cleanup(&this->update_list);
    }

    {
        void** vtbl = *(void***)&this->text_list;
        typedef void (__thiscall* CleanupFn)(void*);
        CleanupFn cleanup = (CleanupFn)vtbl[6];
        cleanup(&this->text_list);
    }
}

/* ================================================================== */
/* UI_Manager::createMessageBox                                        */
/* Address: 0x423AB0                                                    */
/* ================================================================== */
void* UI_Manager::createMessageBox(int resourceId, short param2,
                                    char direction, int x, int y,
                                    char useUpdate)
{
    /* FPS gate: skip if main window FPS <= threshold, unless ID is special */
    uint8_t fps = *(uint8_t*)((uint8_t*)g_main_window + 0x14);
    if ((double)(int)fps <= DAT_00481170 && resourceId != 0x3861) {
        return NULL;
    }

    /* Validate resource frame availability */
    int res = ResourceManager_GetById((void**)&g_resmgr, resourceId);
    if (res == 0) {
        return NULL;
    }

    uint16_t frameCount = *(uint16_t*)((uintptr_t)res + 0x158);
    uint32_t maxFrames  = *(uint32_t*)((uintptr_t)res + 0x15C);
    if (frameCount >= maxFrames) {
        return NULL;
    }

    /* Check associated resource (at +0x40) */
    int childResId = *(int*)((uintptr_t)res + 0x40);
    if (childResId != -1) {
        int childRes = ResourceManager_GetById((void**)&g_resmgr, childResId);
        if (childRes == 0 || *(short*)((uintptr_t)childRes + 0x158) == 0) {
            return NULL;
        }
    }

    /* Check resource at +0x44 */
    int res2 = ResourceManager_GetById((void**)&g_resmgr, *(int*)((uintptr_t)res + 0x44));
    if (res2 != 0 && *(short*)((uintptr_t)res2 + 0x158) != 0) {
        return NULL;
    }

    /* Allocate UIEntity (0xA4 bytes) */
    typedef void* (__thiscall* UIEntityCtor)(void* self, int a, short b,
                                              char c, int d, int e);
    extern UIEntityCtor UIEntity_Ctor;    /* 0x422EC0 */

    void* entity = operator_new(0xA4);
    if (entity == NULL) {
        return NULL;
    }

    void* uiEntity = UIEntity_Ctor(entity, resourceId, param2,
                                    direction, x, y);
    if (uiEntity == NULL) {
        return NULL;
    }

    /* Check if initialization succeeded (vtable[6] cast check) */
    int** entityInt = (int**)uiEntity;
    if ((char)entityInt[6] == 1) {        /* +0x18 = initialized flag */
        /* Add to update_list or pos_list based on useUpdate parameter */
        void* targetList;
        int vtableOffset;

        if (useUpdate != 0) {
            targetList = &this->update_list;    /* +0x34 */
        } else {
            targetList = &this->pos_list;       /* +0x1C */
        }

        /* Call vtable[0x0D] (add item) on the target list */
        void** vtbl = *(void***)targetList;
        typedef void (__thiscall* AddFn)(void*, void*);
        AddFn addFn = (AddFn)vtbl[0x0D];
        addFn(targetList, uiEntity);

        return uiEntity;
    }

    /* Initialization failed — destroy the entity */
    if (uiEntity != NULL) {
        void** vtbl = *(void***)uiEntity;
        typedef void* (__thiscall* DtorFn)(void*, byte);
        DtorFn dtor = (DtorFn)vtbl[0];
        dtor(uiEntity, 1);
    }

    return NULL;
}

/* ================================================================== */
/* UI_Manager::createTooltip                                           */
/* Address: 0x423C50                                                    */
/* ================================================================== */
int* UI_Manager::createTooltip(int resourceId, short param2,
                                int posX, int posY)
{
    /* Allocate 0x88 bytes for tooltip GameObject */
    void* obj = operator_new(0x88);
    if (obj == NULL) {
        return NULL;
    }

    /* Call GameObject_BaseCtor(this, resourceId, param2, 0, 0) */
    GameObject_BaseCtor(obj, resourceId, param2, 0, 0);

    int* result = (int*)obj;

    /* Check if initialization succeeded (flag at +0x18) */
    if ((char)result[6] == 1) {     /* result[6] = *(int*)(obj + 0x18) */
        /* Set position via vtable[3] (HitTest/SetPos) */
        void** vtbl = *(void***)result;
        typedef void (__thiscall* SetPosFn)(void*, int, int);
        SetPosFn setPos = (SetPosFn)vtbl[3];
        setPos(result, posX, posY);

        /* Set flag bit 0x02 at +0x2C */
        result[0x0B] |= 2;          /* result[11] = *(int*)(obj + 0x2C) |= 2 */

        /* Add to text_list at +0x04 */
        {
            void** vtbl2 = *(void***)&this->text_list;
            typedef void (__thiscall* AddFn)(void*, void*);
            AddFn addFn = (AddFn)vtbl2[0x0D];
            addFn(&this->text_list, result);
        }
    } else {
        /* Initialization failed — destroy */
        if (result != NULL) {
            void** vtbl = *(void***)result;
            typedef void* (__thiscall* DtorFn)(void*, byte);
            DtorFn dtor = (DtorFn)vtbl[0];
            dtor(result, 1);
        }
        return NULL;
    }

    return result;
}

/* ================================================================== */
/* UI_Manager::cleanupTooltips (partial — skips text_list)             */
/* Address: 0x423D00                                                    */
/* ================================================================== */
void UI_Manager::cleanupTooltips()
{
    /* Call vtable+0x18 on pos_list */
    {
        void** vtbl = *(void***)&this->pos_list;
        typedef void (__thiscall* CleanupFn)(void*);
        CleanupFn cleanup = (CleanupFn)vtbl[6];
        cleanup(&this->pos_list);
    }

    /* Call vtable+0x18 on update_list (NOT text_list) */
    {
        void** vtbl = *(void***)&this->update_list;
        typedef void (__thiscall* CleanupFn)(void*);
        CleanupFn cleanup = (CleanupFn)vtbl[6];
        cleanup(&this->update_list);
    }
}

/* ================================================================== */
/* UI_Manager::destroyTooltip                                          */
/* Address: 0x423D20                                                    */
/* ================================================================== */
void UI_Manager::destroyTooltip(int* tooltipPtr)
{
    void** textVtbl = *(void***)&this->text_list;

    /* Get count from text_list */
    int count = (int)this->text_list.GetCount();

    if (count == 0) {
        return;
    }

    /* Search for tooltipPtr in text_list items */

    uint32_t idx = 0;
    while (idx < (uint32_t)count) {
        int* item = (int*)this->text_list.GetItem(idx);
        if (item == tooltipPtr) {
            /* Found — remove at index via vtable[4] */
            void** vtbl = *(void***)&this->text_list;
            typedef void (__thiscall* RemoveAtFn)(void*, int);
            RemoveAtFn removeAt = (RemoveAtFn)vtbl[4];
            removeAt(&this->text_list, idx);
            return;
        }
        idx++;
    }
}

/* ================================================================== */
/* UI_Manager::hideTooltip — Per-frame animation tick                   */
/* Address: 0x423D70                                                    */
/* ================================================================== */
void UI_Manager::hideTooltip()
{
    /* Process update_list (+0x34) */
    {
        void** updateVtbl = *(void***)&this->update_list;
        typedef void (__thiscall* RemoveAtFn)(void*, int);

        RemoveAtFn removeAt = (RemoveAtFn)updateVtbl[4];

        uint32_t idx = 0;
        int count = (int)this->update_list.GetCount();
        while (idx < (uint32_t)count) {
            int* item = (int*)this->update_list.GetItem(idx);
            if (item != NULL) {
                /* Call UI_Window_UpdateScroll on the item */
                extern char __fastcall UI_Window_UpdateScroll(int* item);
                char completed = UI_Window_UpdateScroll(item);
                if (completed == 1) {
                    removeAt(&this->update_list, idx);
                }
            }
            idx++;
            count = (int)this->update_list.GetCount();
        }
    }

    /* Process pos_list (+0x1C) */
    {
        void** posVtbl = *(void***)&this->pos_list;
        typedef void (__thiscall* RemoveAtFn)(void*, int);

        RemoveAtFn removeAt = (RemoveAtFn)posVtbl[4];

        uint32_t idx = 0;
        int count = (int)this->pos_list.GetCount();
        while (idx < (uint32_t)count) {
            int* item = (int*)this->pos_list.GetItem(idx);
            if (item != NULL) {
                extern char __fastcall UI_Window_UpdateScroll(int* item);
                char completed = UI_Window_UpdateScroll(item);
                if (completed == 1) {
                    removeAt(&this->pos_list, idx);
                }
            }
            idx++;
            count = (int)this->pos_list.GetCount();
        }
    }
}

/* ================================================================== */
/* UI_Manager::setTooltipText — Iterate text_list, call vtable[11]     */
/* Address: 0x423E00                                                    */
/* ================================================================== */
void UI_Manager::setTooltipText(int a1, int a2, int a3, int a4)
{
    void** textVtbl = *(void***)&this->text_list;


    uint32_t idx = 0;
    int count = (int)this->text_list.GetCount();
    while (idx < (uint32_t)count) {
        int* item = (int*)this->text_list.GetItem(idx);
        if (item != NULL) {
            /* Check initialized flag at +0x18 */
            if ((char)item[6] == 1) {            /* +0x18 */
                /* Check visible flag at +0x24 */
                if ((char)item[9] == 1) {        /* +0x24 */
                    /* Call vtable[11] (offset 0x2C) — SetRectAndFlags */
                    void** vtbl = *(void***)item;
                    typedef void (__thiscall* SetRectFn)(void*, int, int, int, int, int);
                    SetRectFn setRect = (SetRectFn)vtbl[11];
                    setRect(item, a1, a2, a3, a4, item[0x0B]);  /* +0x2C flags */
                }
            }
        }
        idx++;
        count = (int)this->text_list.GetCount();
    }
}

/* ================================================================== */
/* UI_Manager::setTooltipPos — Iterate pos_list, call vtable[11]       */
/* Address: 0x423E80                                                    */
/* ================================================================== */
void UI_Manager::setTooltipPos(int a1, int a2, int a3, int a4)
{
    void** posVtbl = *(void***)&this->pos_list;


    uint32_t idx = 0;
    int count = (int)this->pos_list.GetCount();
    while (idx < (uint32_t)count) {
        int* item = (int*)this->pos_list.GetItem(idx);
        if (item != NULL) {
            if ((char)item[6] == 1 && (char)item[9] == 1) {
                void** vtbl = *(void***)item;
                typedef void (__thiscall* SetRectFn)(void*, int, int, int, int, int);
                SetRectFn setRect = (SetRectFn)vtbl[11];
                setRect(item, a1, a2, a3, a4, item[0x0B]);
            }
        }
        idx++;
        count = (int)this->pos_list.GetCount();
    }
}

/* ================================================================== */
/* UI_Manager::updateTooltip — Iterate update_list, call vtable[11]    */
/* Address: 0x423F00                                                    */
/* ================================================================== */
void UI_Manager::updateTooltip(int a1, int a2, int a3, int a4)
{
    void** updateVtbl = *(void***)&this->update_list;


    uint32_t idx = 0;
    int count = (int)this->update_list.GetCount();
    while (idx < (uint32_t)count) {
        int* item = (int*)this->update_list.GetItem(idx);
        if (item != NULL) {
            if ((char)item[6] == 1 && (char)item[9] == 1) {
                void** vtbl = *(void***)item;
                typedef void (__thiscall* SetRectFn)(void*, int, int, int, int, int);
                SetRectFn setRect = (SetRectFn)vtbl[11];
                setRect(item, a1, a2, a3, a4, item[0x0B]);
            }
        }
        idx++;
        count = (int)this->update_list.GetCount();
    }
}

/* ================================================================== */
/* UI_Manager::resetTooltips — Iterate update_list + pos_list          */
/* Address: 0x423F80                                                    */
/* ================================================================== */
void UI_Manager::resetTooltips(int param)
{
    /* Process update_list (+0x34) */
    {
        void** updateVtbl = *(void***)&this->update_list;


        uint32_t idx = 0;
        int count = (int)this->update_list.GetCount();
        while (idx < (uint32_t)count) {
            int* item = (int*)this->update_list.GetItem(idx);
            if (item != NULL) {
                /* Call vtable[9] (offset 0x24) — reset method */
                void** vtbl = *(void***)item;
                typedef void (__thiscall* ResetFn)(void*, int);
                ResetFn reset = (ResetFn)vtbl[9];
                reset(item, param);
            }
            idx++;
            count = (int)this->update_list.GetCount();
        }
    }

    /* Process pos_list (+0x1C) */
    {
        void** posVtbl = *(void***)&this->pos_list;


        uint32_t idx = 0;
        int count = (int)this->pos_list.GetCount();
        while (idx < (uint32_t)count) {
            int* item = (int*)this->pos_list.GetItem(idx);
            if (item != NULL) {
                void** vtbl = *(void***)item;
                typedef void (__thiscall* ResetFn)(void*, int);
                ResetFn reset = (ResetFn)vtbl[9];
                reset(item, param);
            }
            idx++;
            count = (int)this->pos_list.GetCount();
        }
    }
}
