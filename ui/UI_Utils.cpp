/**
 * UI_Utils.cpp — UI_Manager, tooltip, and helper implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "UI_Utils.h"
#include "UIEntity.h"        /* for resetTooltips' typed Entity::SetVisible dispatch */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <new>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void* operator_new(size_t size);                    /* 0x465CE0 — malloc wrapper */
    void  GLOBAL_free(void* ptr);                       /* 0x465CD0 — free wrapper   */
extern "C" {
    void  __stdcall DefWindowProcA(HWND hWnd, UINT Msg,
                                   void* wParam, void* lParam);  /* USER32 */
}
    int   __thiscall ResourceManager_GetById(void** mgr, int id); /* 0x460A30 */

/* External declarations from other modules */
extern void* g_main_window;              /* 0x4AA4A0 */
class ResourceManager;
extern ResourceManager g_resmgr;         /* 0x4855E8 — object, not a pointer (was void*,
                                           * a widespread cross-TU landmine — see
                                           * PROGRESS.md's g_resmgr sweep) */

/* Global tooltip manager singleton (defined in graphics/DDRAW.cpp). */
extern UI_Manager* g_tooltip_mgr;        /* 0x4FD220 */

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
    /* original vtable[8] (0x424030) forwards to vtable[7] (0x424530),
     * which bounds-checks against `capacity` before reading. */
    if (index >= capacity) {
        return nullptr;
    }
    return items[index];
}

uint32_t UITimerList::GetCount() const
{
    return count;
}

/* ================================================================== */
/* UITimerList::RemoveAt — original vtable[4] (0x4356E0) + vtable[3]    */
/* (0x4241E0, Ghidra-named "UI_HandleScrollMessage" — a FLIRT misnomer, */
/* not a scroll message handler).                                      */
/* ================================================================== */
void UITimerList::RemoveAt(uint32_t index)
{
    if (index >= capacity) {
        return;
    }
    void* item = items[index];
    if (item == nullptr) {
        return;
    }
    if (index < count - 1) {
        std::memmove(&items[index], &items[index + 1],
                     (count - index - 1) * sizeof(void*));
    }
    items[count - 1] = nullptr;
    --count;

    /* Delete the extracted item through its own virtual destructor —
     * the typed equivalent of the original's manual
     * `(**(code**)*item)(1)` scalar-deleting-destructor dispatch. Every
     * item ever added to one of UI_Manager's three lists is a
     * GameObject-derived tooltip/message-box entity (text_list holds
     * plain Entity-sized objects from GameObject_BaseCtor; pos_list/
     * update_list hold UIEntity from UI_CreateMessageBox's
     * UIEntity_Ctor). */
    delete static_cast<GameObject*>(item);
}

/* ================================================================== */
/* UITimerList::RemoveAll — original vtable[6] (0x424270, Ghidra-named  */
/* "UI_SetScrollPos" — another FLIRT misnomer).                        */
/* ================================================================== */
void UITimerList::RemoveAll()
{
    while (count != 0) {
        RemoveAt(count - 1);
    }
}

/* ================================================================== */
/* UITimerList::SetAt — original vtable[10] (0x424790).                */
/* ================================================================== */
void* UITimerList::SetAt(uint32_t index, void* item)
{
    if (index > count) {
        return nullptr;
    }
    if (index >= capacity) {
        /* Same 1.1x growth policy as InsertAt (0x477C10). This branch is
         * not reached by any call path exercised today — InsertAt below
         * always grows first — kept for fidelity with the original's
         * always-present guard. */
        Resize(static_cast<uint32_t>((index + 1) * 1.1));
    }
    if (items[index] != nullptr) {
        delete static_cast<GameObject*>(items[index]);
        items[index] = nullptr;
    }
    items[index] = item;
    return items[index];
}

/* ================================================================== */
/* UITimerList::InsertAt — original vtable[17] (0x4248C0).             */
/* ================================================================== */
uint32_t UITimerList::InsertAt(uint32_t index, void* item)
{
    if (index > count) {
        return static_cast<uint32_t>(-1);
    }
    if (capacity < count + 1) {
        Resize(static_cast<uint32_t>((count + 1) * 1.1));
    }
    if (index != count) {
        std::memmove(&items[index + 1], &items[index],
                     (count - index) * sizeof(void*));
        items[index] = nullptr;
    }
    SetAt(index, item);
    ++count;
    return index;
}

/* ================================================================== */
/* UITimerList::Add — original vtable[13] (0x4362B0).                  */
/* ================================================================== */
void UITimerList::Add(void* item)
{
    if (key_size != 0) {
        /* Keyed linear-scan insert (comparator vtable[18]/0x424960) is
         * not reconstructed — only update_list has a non-zero key_size,
         * and update_list is only ever populated by UI_CreateMessageBox
         * (0x423AB0), an unimplemented stub that always returns nullptr
         * on every call site in this tree. Fail loudly rather than
         * silently inserting unsorted if that ever changes. */
        fprintf(stderr, "STUB: UITimerList::Add keyed-insert branch "
                        "(key_size=%d) not implemented (0x424960 not "
                        "reconstructed)\n", key_size);
        assert(0 && "UITimerList::Add: keyed-insert branch unreachable-but-unimplemented");
        return;
    }
    InsertAt(count, item);
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
    pos_list.RemoveAll();
    update_list.RemoveAll();
    text_list.RemoveAll();
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
Entity* UI_Manager::createTooltip(int resourceId, short param2,
                                   int posX, int posY)
{
    /* 0x88 was this call site's real x86 allocation size; it matches
     * sizeof(Entity) on x86 exactly (core/Entity.h's fields run +0x24..
     * +0x87), confirmed independently by every real user of the returned
     * pointer treating it as a plain Entity* (ui/UIEntity.cpp's pTooltip
     * StopSound/SetVisible/Update calls) and by network/DirectPlay.cpp's
     * own shadow-GameObject site needing the identical size. Use
     * sizeof(Entity) (the real host size, pointer fields widen it past
     * 0x88) rather than the stale x86 literal, and construct through
     * Entity's real constructor (0x405790) — not the free-function
     * "GameObject_BaseCtor" misdeclaration fixed elsewhere this session.
     *
     * Placement-new pairing: this object is allocated with operator_new
     * (0x465CE0) and must be torn down with obj->~Entity() + GLOBAL_free
     * (0x465CD0), never plain `delete` (mismatched allocator). No current
     * caller frees a tooltip at all — destroyTooltip only removes it from
     * text_list — so this is latent, not live; flagged for whoever
     * implements real teardown. */
    void* mem = operator_new(sizeof(Entity));
    if (mem == NULL) {
        return NULL;
    }
    Entity* obj = new (mem) Entity(resourceId, param2, 0, 0);

    if (obj->initialized) {
        /* Position — vtable[3], Entity::SetWorldPos overriding
         * GameObject::MoveTo. */
        obj->MoveTo(posX, posY);

        obj->blit_flags |= 2;

        /* Unordered append — text_list's key_size is 0. */
        text_list.Add(obj);
    } else {
        obj->~Entity();
        GLOBAL_free(mem);
        return NULL;
    }

    return obj;
}

/* ================================================================== */
/* UI_Manager::cleanupTooltips (partial — skips text_list)             */
/* Address: 0x423D00                                                    */
/* ================================================================== */
void UI_Manager::cleanupTooltips()
{
    pos_list.RemoveAll();
    update_list.RemoveAll();
}

/* ================================================================== */
/* UI_Manager::destroyTooltip                                          */
/* Address: 0x423D20                                                    */
/* ================================================================== */
void UI_Manager::destroyTooltip(int* tooltipPtr)
{
    uint32_t count = text_list.GetCount();
    for (uint32_t idx = 0; idx < count; ++idx) {
        if (text_list.GetItem(idx) == tooltipPtr) {
            text_list.RemoveAt(idx);
            return;
        }
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
        uint32_t idx = 0;
        uint32_t count = update_list.GetCount();
        while (idx < count) {
            UIEntity* item = static_cast<UIEntity*>(update_list.GetItem(idx));
            if (item != NULL && item->UpdateScroll()) {
                update_list.RemoveAt(idx);
            }
            idx++;
            count = update_list.GetCount();
        }
    }

    /* Process pos_list (+0x1C) */
    {
        uint32_t idx = 0;
        uint32_t count = pos_list.GetCount();
        while (idx < count) {
            UIEntity* item = static_cast<UIEntity*>(pos_list.GetItem(idx));
            if (item != NULL && item->UpdateScroll()) {
                pos_list.RemoveAt(idx);
            }
            idx++;
            count = pos_list.GetCount();
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
        uint32_t idx = 0;
        uint32_t count = update_list.GetCount();
        while (idx < count) {
            void* item = update_list.GetItem(idx);
            if (item != NULL) {
                /* original vtable[9] == UIEntity::SetVisible — see
                 * ui/UIEntity.h's live vtable-slot dump. */
                static_cast<Entity*>(item)->SetVisible(param != 0);
            }
            idx++;
            count = update_list.GetCount();
        }
    }

    /* Process pos_list (+0x1C) */
    {
        uint32_t idx = 0;
        uint32_t count = pos_list.GetCount();
        while (idx < count) {
            void* item = pos_list.GetItem(idx);
            if (item != NULL) {
                static_cast<Entity*>(item)->SetVisible(param != 0);
            }
            idx++;
            count = pos_list.GetCount();
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
    g_tooltip_mgr->setTooltipText(x, y, w, h, 1);
}

void UI_SetTooltipPos(int x, int y, int w, int h, int flag);
void UI_SetTooltipPos(int x, int y, int w, int h, int flag)
{
    g_tooltip_mgr->setTooltipPos(x, y, w, h, flag);
}

void UI_UpdateTooltip(int x, int y, int w, int h, int flag);
void UI_UpdateTooltip(int x, int y, int w, int h, int flag)
{
    g_tooltip_mgr->updateTooltip(x, y, w, h, flag);
}

/* ================================================================== */
/* Free-function facades for UI_Manager's tooltip-lifecycle methods.    */
/*                                                                      */
/* Declared with a `void* mgr` parameter (rather than `UI_Manager*`) to */
/* match every existing extern declaration of these five names across  */
/* the tree — dozens of TUs each carry their own local                  */
/* `extern void* g_tooltip_mgr`-style forward declaration and pass that */
/* value straight through. The real object underneath is always the    */
/* UI_Manager singleton at 0x4FD220 (g_tooltip_mgr, graphics/DDRAW.cpp), */
/* so `static_cast<UI_Manager*>(mgr)` here is a real-type recovery, not */
/* a manual layout cast, following the same pattern already established */
/* by UI_SetTooltipText/UI_SetTooltipPos/UI_UpdateTooltip above.        */
/* ================================================================== */

/** UI_CleanupTooltips — Address: 0x423D00. See UI_Manager::cleanupTooltips. */
void UI_CleanupTooltips(void* mgr);
void UI_CleanupTooltips(void* mgr)
{
    static_cast<UI_Manager*>(mgr)->cleanupTooltips();
}

/** UI_HideTooltip — Address: 0x423D70. See UI_Manager::hideTooltip, which
 *  now calls the real UIEntity::UpdateScroll (0x423560, 2026-08-14) on
 *  each item — still unreachable today since both lists hideTooltip
 *  iterates are populated only by UI_CreateMessageBox (0x423AB0), itself
 *  an unimplemented stub that always returns nullptr. */
void UI_HideTooltip(void* mgr);
void UI_HideTooltip(void* mgr)
{
    static_cast<UI_Manager*>(mgr)->hideTooltip();
}

/** UI_DestroyTooltip — Address: 0x423D20. See UI_Manager::destroyTooltip.
 *  `tooltip` is a GameObject* smuggled through an int handle by every
 *  caller in this tree that uses this (void*, int) overload (game/
 *  Panel.cpp, game/ScriptedObject.cpp — out of scope to retype here);
 *  matches the original 32-bit x86 ABI, where a pointer and an int are
 *  the same width, exactly. */
void UI_DestroyTooltip(void* mgr, int tooltip);
void UI_DestroyTooltip(void* mgr, int tooltip)
{
    // ABI_BOUNDARY: `tooltip` is a pointer value carried through this
    // (void*, int) overload's `int` parameter, matching the original
    // 32-bit x86 ABI where a pointer and an int are the same width — the
    // established shape of every caller of this overload (see doc
    // comment above). Not a stand-in for a known object's field layout.
    static_cast<UI_Manager*>(mgr)->destroyTooltip(
        reinterpret_cast<int*>(static_cast<intptr_t>(tooltip)));
}

/** UI_ResetTooltips — Address: 0x423F80. See UI_Manager::resetTooltips. */
void UI_ResetTooltips(void* mgr, int param);
void UI_ResetTooltips(void* mgr, int param)
{
    static_cast<UI_Manager*>(mgr)->resetTooltips(param);
}

/**
 * UI_CreateTooltip — Address: 0x423C50, wired to UI_Manager::createTooltip.
 * Its previous blocker ("depends on GameObject_BaseCtor 0x405790, still
 * unimplemented") was a misreading — 0x405790 is Entity's real constructor,
 * already implemented (core/Entity.cpp); the free-function declaration was
 * simply the wrong shape to ever call it. See PROGRESS.md's 2026-08-14
 * entry.
 */
Entity* UI_CreateTooltip(void* mgr, int resourceId, int16_t param2, int x, int y);
Entity* UI_CreateTooltip(void* mgr, int resourceId, int16_t param2, int x, int y)
{
    return static_cast<UI_Manager*>(mgr)->createTooltip(resourceId, param2, x, y);
}
