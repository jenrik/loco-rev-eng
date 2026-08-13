/**
 * UI_Utils.cpp — UI_Manager, tooltip, and helper implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "UI_Utils.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <stdint.h>
#include <cstring>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void* operator_new(size_t size);                    /* 0x465CE0 — malloc wrapper */
    void  GLOBAL_free(void* ptr);                       /* 0x465CD0 — free wrapper   */
extern "C" {
    void  __stdcall DefWindowProcA(HWND hWnd, UINT Msg,
                                   void* wParam, void* lParam);  /* USER32 */
}
    void  __thiscall GameObject_BaseCtor(void* self, int a, int b,
                                          int c, int d);          /* 0x405790 */
    int   __thiscall ResourceManager_GetById(void** mgr, int id); /* 0x460A30 */

/* External declarations from other modules */
extern void* g_main_window;              /* 0x4AA4A0 */
class ResourceManager;
extern ResourceManager g_resmgr;         /* 0x4855E8 — object, not a pointer (was void*,
                                           * a widespread cross-TU landmine — see
                                           * PROGRESS.md's g_resmgr sweep) */

/* Global tooltip manager pointer (set externally) */
extern void* g_tooltip_mgr;              /* 0x4FD220 */

/* FPS gate threshold for CreateMessageBox */
extern double DAT_00481170;              /* 0x481170 — FPS threshold */

/* ================================================================== */
/* UITimerList::Resize (vtable[0] on all 3 of UI_Manager's sub-lists)   */
/* Address: 0x435D10 (originally a shared "Timer_Resize"/"Collection::  */
/* Resize" routine reused across several of the binary's UI collection */
/* template instantiations via a raw `this+4`=items / `this+8`=        */
/* capacity layout — that layout is exactly UITimerList::items/         */
/* capacity, so the real logic is implemented directly as this typed   */
/* method rather than kept behind a shared void*-taking free function; */
/* see resources/AssetMgr.cpp for a distinct, unrelated caller of the   */
/* same original address on its own collection type, left untouched.)  */
/* ================================================================== */
void UITimerList::Resize(uint32_t new_capacity)
{
    uint32_t old_capacity = capacity;
    uint32_t target = new_capacity;

    /* Shrinking: trim only the trailing NULL slots, stopping as soon as
     * a live (non-NULL) entry is found — matches the disassembly's
     * backward scan from old_capacity down to new_capacity. */
    if (new_capacity < old_capacity) {
        uint32_t idx = old_capacity;
        while (idx > new_capacity) {
            if (items[idx - 1] != NULL) {
                break;
            }
            --idx;
        }
        target = idx;
    }

    void** old_items = items;

    if (target != 0) {
        void** new_items = static_cast<void**>(operator_new(target * sizeof(void*)));
        items = new_items;
        std::memset(new_items, 0, target * sizeof(void*));
    }

    if (old_items != NULL) {
        if (target != 0) {
            uint32_t copy_count = (target < old_capacity) ? target : old_capacity;
            std::memcpy(items, old_items, copy_count * sizeof(void*));
        }
        GLOBAL_free(old_items);
    }

    capacity = (items != NULL) ? target : 0;
    if (capacity == 0) {
        items = NULL;
    }
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

/* UI_ShowWindow/UI_HideWindow/UI_EnableWindow (0x423840/0x423870/
 * 0x423890) converted 2026-08-09 to real UIEntity::StopSound/Update/
 * SetVisible overrides — see ui/UIEntity.cpp. */

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

    /* Allocate UIEntity. 0xA4 was the original x86 sizeof(UIEntity); use
     * the real host size (0xC8 — see ui/UIEntity.h/.cpp) instead of the
     * stale x86 literal. */
    typedef void* (__thiscall* UIEntityCtor)(void* self, int a, short b,
                                              char c, int d, int e);
    extern UIEntityCtor UIEntity_Ctor;    /* 0x422EC0 */
    extern size_t UIEntity_Size();        /* ui/UIEntity.cpp — real sizeof(UIEntity) */

    void* entity = operator_new(UIEntity_Size());
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
    /* Allocate 0x88 bytes for tooltip GameObject. 0x88 is this call site's
     * real x86 evidence, but the concrete type constructed here is bigger
     * than the base GameObject (sizeof(GameObject) == 0x38) — it's some
     * still-unidentified GameObject-derived tooltip class whose full field
     * layout hasn't been reconstructed, so there's no sizeof(Type) to take
     * yet (guessing one would violate CLAUDE.md's evidence-only rule).
     * GameObject_BaseCtor (0x405790) is still an undecompiled stub
     * (shared/stubs_impl.cpp) that never writes through `obj`, so this is
     * not a live overflow today; revisit once the real derived type is
     * identified and GameObject_BaseCtor is decompiled. */
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
void UI_Manager::setTooltipText(int a1, int a2, int a3, int a4, int /*unused5*/)
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
void UI_Manager::setTooltipPos(int a1, int a2, int a3, int a4, int /*unused5*/)
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
void UI_Manager::updateTooltip(int a1, int a2, int a3, int a4, int /*unused5*/)
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

#pragma GCC diagnostic pop

/* ================================================================== */
/* Typed wrappers for TileMap::ProcessRect's tooltip dispatch calls.    */
/* Declared in world/tilemap.h; implemented here (not in tilemap.cpp)  */
/* to avoid pulling this file's own headers into that one. Real         */
/* signatures verified against each callee's RET immediate — all three  */
/* take a real 5th stack arg this file's own method signatures above    */
/* previously omitted; every caller passes literal 1 for it and the      */
/* body never reads it. */
/* ================================================================== */
void UI_SetTooltipText(int x, int y, int w, int h);
void UI_SetTooltipText(int x, int y, int w, int h)
{
    static_cast<UI_Manager*>(g_tooltip_mgr)->setTooltipText(x, y, w, h, 1);
}

void UI_SetTooltipPos(int x, int y, int w, int h, int flag);
void UI_SetTooltipPos(int x, int y, int w, int h, int flag)
{
    static_cast<UI_Manager*>(g_tooltip_mgr)->setTooltipPos(x, y, w, h, flag);
}

void UI_UpdateTooltip(int x, int y, int w, int h, int flag);
void UI_UpdateTooltip(int x, int y, int w, int h, int flag)
{
    static_cast<UI_Manager*>(g_tooltip_mgr)->updateTooltip(x, y, w, h, flag);
}
