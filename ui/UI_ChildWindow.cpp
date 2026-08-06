/**
 * UI_ChildWindow.cpp — Shared "ChildWindow" free-function cluster
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * See UI_ChildWindow.h for the full evidence trail on why these are free
 * functions (not a base class) and why the host (`#ifndef _WIN32`) path
 * is a loud deferred stub rather than a raw-offset reimplementation.
 */

// Status: TRANSCRIBED

#include "UI_ChildWindow.h"

#include <cassert>
#include <cstdio>

/* ================================================================== */
/* External references (all already implemented elsewhere)             */
/* ================================================================== */

extern "C" {
void   __cdecl GLOBAL_free(void* ptr);                       /* 0x465CD0 */
void*  __cdecl operator_new(size_t size);                     /* 0x465CE0 */
void*  __thiscall ResourceManager_GetById(void* mgr, int32_t resId); /* 0x446EA0 */
void*  __thiscall ResourceManager_GetStringById(void* mgr, uint32_t id); /* 0x4472B0 */
int    __thiscall RESMGR_LoadSoundResource(void* resHandle);  /* 0x448D60 */
void   __thiscall RESMGR_ReleaseSoundResource(void* resHandle); /* 0x448EE0 */
}
/* GetResourceType has plain C++ linkage (resources/ResourceManager.h) —
 * declared outside the extern "C" block above, not inside it. */
extern unsigned int GetResourceType(unsigned int resourceId);  /* 0x446030 */

#ifdef _WIN32
extern void*  __thiscall UIPANEL_CreateSurface(void* panel);                    /* 0x42A110 */
extern uint8_t __thiscall UIPANEL_StretchBlit(void* surface, LPCSTR filePath,
                                               uint32_t param2, int32_t param3,
                                               int32_t param4);                  /* 0x42AB10 */
#endif

extern void* g_resmgr;      /* 0x4855E8 — ResourceManager singleton */
extern void* g_netman;      /* 0x4FD3AC — NetMan singleton (raw-offset view; see
                                game/ScriptedObject.cpp for the same
                                g_netman[0x17].scenarioId idiom) */
extern uint32_t g_game_time; /* 0x4A99B4 */

#ifdef _WIN32
/* Only referenced from the faithful x86-offset bodies below; neither is
 * implemented anywhere else in this codebase yet (a separate, pre-
 * existing gap — not introduced by this file), so these are declaration-
 * only. The MinGW typecheck build compiles _WIN32 code but does not link
 * it, so this is sufficient for that build's purpose. */
extern "C" {
void __thiscall INPUT_EditScrollHandler(void* obj, uint32_t resId);
void __thiscall ResourceManager_AnimateClock(void* mgr, uint32_t gameTime);
}
extern void* DAT_004a99b0;
#endif

namespace {

#ifdef _WIN32
/* Call vtable slot 0 (scalar deleting destructor convention: flags=1
 * frees no memory, just releases sub-resources) on a sub-object whose
 * concrete type is not known here. Matches the original's
 * `(**(code**)*obj)(1)` idiom exactly. */
void ReleaseSubObject(void* obj)
{
    void** const vtbl = *reinterpret_cast<void***>(obj);
    using ScalarDtor = void (*)(void*, int32_t);
    reinterpret_cast<ScalarDtor>(vtbl[0])(obj, 1);
}
#endif

} // namespace

/* ================================================================== */
/* UI_CreateChildWindow                                                 */
/* Address: 0x424AF0                                                    */
/* ================================================================== */
void* UI_CreateChildWindow(void* self, uint32_t resourceId, int32_t nameParam)
{
#ifdef _WIN32
    uint8_t* const p = static_cast<uint8_t*>(self);

    *reinterpret_cast<int32_t*>(p + 0x10)  = 0;  /* render-surface / childObj */
    *reinterpret_cast<int32_t*>(p + 0x24)  = 0;  /* sub-object pointer        */
    *reinterpret_cast<int32_t*>(p + 0x20)  = 0;  /* heap buffer               */
    *reinterpret_cast<uint8_t*>(p + 0x18)  = 0;  /* byte flag                 */
    *reinterpret_cast<int16_t*>(p + 0x158) = 0;  /* overlay refcount          */
    *reinterpret_cast<int32_t*>(p + 0x164) = 0;  /* animation-metadata flags  */
    *reinterpret_cast<int16_t*>(p + 0x32)  = 0;  /* road offset x             */
    *reinterpret_cast<int16_t*>(p + 0x34)  = 0;  /* road offset y             */

    /* The binary stages the ChildWindow vtable (0x477C18) here; every
     * real caller's own constructor installs its real vtable right
     * after this call returns, so the compiler manages it — no explicit
     * write needed in real C++. */

    UI_ChildWindow_Create(self, resourceId, nameParam);
    return self;
#else
    (void)resourceId;
    (void)nameParam;
    std::fprintf(stderr,
        "STUB: UI_CreateChildWindow (0x424AF0) reached on host build — "
        "the ChildWindow cluster is not yet ported to a non-Windows "
        "receiver type (see PROGRESS.md).\n");
    assert(false && "UI_CreateChildWindow: host implementation not yet ported");
    return self;
#endif
}

/* ================================================================== */
/* UI_ChildWindow_Create                                                */
/* Address: 0x424BF0                                                    */
/* ================================================================== */
void UI_ChildWindow_Create(void* self, uint32_t resourceId, int32_t nameParam)
{
#ifdef _WIN32
    uint8_t* const p = static_cast<uint8_t*>(self);

    *reinterpret_cast<uint32_t*>(p + 0x04) = resourceId;               /* resourceId   */
    *reinterpret_cast<uint8_t*>(p + 0x08)  =
        static_cast<uint8_t>(GetResourceType(resourceId)); /* resourceType */
    *reinterpret_cast<int32_t*>(p + 0x0C)  = 0;                        /* streamData   */
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;
    *reinterpret_cast<int32_t*>(p + 0x3C)  = 0;
    *reinterpret_cast<int16_t*>(p + 0x14)  = 0;
    *reinterpret_cast<int16_t*>(p + 0x16)  = 0;
    *reinterpret_cast<int16_t*>(p + 0x160) = 1;                        /* field_160    */
    *reinterpret_cast<int16_t*>(p + 0x1A)  = 0;                        /* sub-window count */
    *reinterpret_cast<int16_t*>(p + 0x1C)  = 0;
    *reinterpret_cast<int16_t*>(p + 0x1E)  = 0;
    *reinterpret_cast<int16_t*>(p + 0x28)  = 0;
    *reinterpret_cast<int16_t*>(p + 0x2A)  = 0;
    *reinterpret_cast<int16_t*>(p + 0x2C)  = 0;
    *reinterpret_cast<int32_t*>(p + 0x40)  = -1;                       /* dependent resource id 1 */
    *reinterpret_cast<int32_t*>(p + 0x44)  = -1;                       /* dependent resource id 2 */
    *reinterpret_cast<uint8_t*>(p + 0x18)  = 0;
    *reinterpret_cast<uint8_t*>(p + 0x163) = 1;
    *reinterpret_cast<uint8_t*>(p + 0x162) = 0;                        /* loaded       */
    *reinterpret_cast<uint8_t*>(p + 0x14D) = 0;                        /* inside bmpPath buffer */
    *reinterpret_cast<int32_t*>(p + 0x15C) = -1;                       /* inside bmpPath buffer */

    if (nameParam != 0) {
        /* Not exercised by any current caller (both CursorEditWindow
         * and TrainStation always pass nameParam=0 — see the header
         * comment). Deferred rather than guessed: this branch depends
         * on a vararg CRT_sprintf_buf call whose full argument list
         * Ghidra could not recover for this call site, and dispatches
         * through the receiver's own vtable slot 3 (loadCursorData). */
        std::fprintf(stderr,
            "STUB: UI_ChildWindow_Create (0x424BF0) nameParam!=0 resource-"
            "loading branch reached — not yet ported (see PROGRESS.md).\n");
        assert(false &&
               "UI_ChildWindow_Create: nameParam!=0 branch not yet ported");
    }
#else
    (void)resourceId;
    (void)nameParam;
    std::fprintf(stderr,
        "STUB: UI_ChildWindow_Create (0x424BF0) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_ChildWindow_Create: host implementation not yet ported");
#endif
}

/* ================================================================== */
/* UI_ChildWindow_Dtor                                                  */
/* Address: 0x424BA0                                                    */
/* ================================================================== */
void UI_ChildWindow_Dtor(void* self)
{
#ifdef _WIN32
    uint8_t* const p = static_cast<uint8_t*>(self);

    /* The binary resets the ChildWindow vtable (0x477C18) here for
     * partial-destruction safety; compiler-managed in real C++. */

    *reinterpret_cast<uint8_t*>(p + 0x162) = 0;  /* loaded */

    void** const renderSurface = reinterpret_cast<void**>(p + 0x10);
    if (*renderSurface != nullptr) {
        ReleaseSubObject(*renderSurface);
        *renderSurface = nullptr;
    }

    void** const heapBuf = reinterpret_cast<void**>(p + 0x20);
    if (*heapBuf != nullptr) {
        GLOBAL_free(*heapBuf);
        *heapBuf = nullptr;
    }

    void** const subObject = reinterpret_cast<void**>(p + 0x24);
    if (*subObject != nullptr) {
        ReleaseSubObject(*subObject);
        *subObject = nullptr;
    }
#else
    (void)self;
    std::fprintf(stderr,
        "STUB: UI_ChildWindow_Dtor (0x424BA0) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_ChildWindow_Dtor: host implementation not yet ported");
#endif
}

/* ================================================================== */
/* UI_ChildWindow_Render                                                */
/* Address: 0x424E00                                                    */
/* ================================================================== */
uint8_t UI_ChildWindow_Render(void* self, void* stream)
{
    (void)self;
    (void)stream;
    std::fprintf(stderr,
        "STUB: UI_ChildWindow_Render (0x424E00) reached — TODO: decompile "
        "0x424E00 (see ui/UI_ChildWindow.h and PROGRESS.md for why this "
        "one is deferred rather than transcribed).\n");
    assert(false && "UI_ChildWindow_Render: not yet ported (TODO: decompile 0x424E00)");
    return 0;
}

/* ================================================================== */
/* UI_IsBitmapReady                                                     */
/* Address: 0x4255F0                                                    */
/* ================================================================== */
int32_t UI_IsBitmapReady(int32_t self)
{
#ifdef _WIN32
    const uint8_t* const p =
        reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(self)));

    if (*reinterpret_cast<const uint8_t*>(p + 0x163) == 0) {
        return 0;
    }
    if (*reinterpret_cast<const int32_t*>(p + 0x24) == 0) {
        return 0;
    }
    if (*reinterpret_cast<const int16_t*>(p + 0x2C) == 0) {
        return 0;
    }

    const int32_t dep1 = *reinterpret_cast<const int32_t*>(p + 0x40);
    if (dep1 != -1) {
        void* const res1 = ResourceManager_GetById(&g_resmgr, dep1);
        const bool dep1Ready = res1 != nullptr &&
            *reinterpret_cast<const int16_t*>(static_cast<const uint8_t*>(res1) + 0x158) != 0;
        if (!dep1Ready) {
            return 0;
        }
    }

    const int32_t dep2 = *reinterpret_cast<const int32_t*>(p + 0x44);
    void* const res2 = ResourceManager_GetById(&g_resmgr, dep2);
    const bool dep2Ready = res2 != nullptr &&
        *reinterpret_cast<const int16_t*>(static_cast<const uint8_t*>(res2) + 0x158) != 0;
    if (dep2Ready) {
        return 0;
    }

    /* Scenario-mode special case for resource 0xC42 (matches the same
     * g_netman[0x17].scenarioId idiom already used in
     * game/ScriptedObject.cpp). */
    const int32_t resourceId = *reinterpret_cast<const int32_t*>(p + 0x04);
    if (resourceId == 0xC42) {
        const int32_t scenarioId =
            *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(g_netman) + 0x7C4);
        if (scenarioId == 2) {
            return 0;
        }
    }

    return 1;
#else
    (void)self;
    std::fprintf(stderr,
        "STUB: UI_IsBitmapReady (0x4255F0) reached on host build — its "
        "real call sites (Town::handle_tile_click, "
        "RESDATA_ScriptedObject::Start) pass a resource_manager_sdl3.cpp "
        "bridge object, not a ChildWindow-shaped receiver, so an offset-"
        "based body would read unrelated bytes (see PROGRESS.md).\n");
    assert(false && "UI_IsBitmapReady: host implementation not yet ported");
    return 0;
#endif
}

/* ================================================================== */
/* UI_PaintWindow                                                       */
/* Address: 0x425670                                                    */
/* ================================================================== */
void* UI_PaintWindow(void* self, int32_t param1, int32_t param2)
{
#ifdef _WIN32
    uint8_t* const p = static_cast<uint8_t*>(self);

    if (*reinterpret_cast<const int16_t*>(p + 0x160) == 0) {
        return nullptr;
    }

    void** const renderSurfaceSlot = reinterpret_cast<void**>(p + 0x10);
    if (*renderSurfaceSlot == nullptr) {
        void* const raw = operator_new(0x20);
        void* const surface = (raw != nullptr) ? UIPANEL_CreateSurface(raw) : nullptr;
        *renderSurfaceSlot = surface;
        if (surface == nullptr) {
            return nullptr;
        }
        UIPANEL_StretchBlit(surface, reinterpret_cast<LPCSTR>(p + 0x48), 0,
                             static_cast<uint32_t>(param1), param2);
    }

    int32_t* const surfaceWords = static_cast<int32_t*>(*renderSurfaceSlot);
    if (surfaceWords[6] == 0 && surfaceWords[7] == 0) {
        ReleaseSubObject(*renderSurfaceSlot);
        *renderSurfaceSlot = nullptr;
        return nullptr;
    }

    *reinterpret_cast<int16_t*>(p + 0x14) = static_cast<int16_t>(
        static_cast<uint32_t>(surfaceWords[2]) /
        *reinterpret_cast<const uint16_t*>(p + 0x160));
    const int16_t surfaceField0C = *reinterpret_cast<const int16_t*>(&surfaceWords[3]);
    *reinterpret_cast<int16_t*>(p + 0x158) += 1;   /* overlay refcount */
    *reinterpret_cast<int16_t*>(p + 0x16) = surfaceField0C;

    if (*reinterpret_cast<const uint8_t*>(p + 0x163) == 0) {
        INPUT_EditScrollHandler(&DAT_004a99b0, *reinterpret_cast<const uint32_t*>(p + 0x04));
    }

    const uint16_t subWindowCount = *reinterpret_cast<const uint16_t*>(p + 0x1A);
    if (subWindowCount != 0) {
        const uint8_t* const entryTable = *reinterpret_cast<const uint8_t* const*>(p + 0x20);
        for (uint16_t i = 0; i < subWindowCount; ++i) {
            const uint16_t stringId =
                *reinterpret_cast<const uint16_t*>(entryTable + i * 0x18 + 0x0E);
            void* const strRes = ResourceManager_GetStringById(&g_resmgr, stringId);
            if (strRes != nullptr) {
                RESMGR_LoadSoundResource(strRes);
            }
        }
    }

    if (*reinterpret_cast<const int32_t*>(p + 0x04) == 0x842) {
        ResourceManager_AnimateClock(&g_resmgr, g_game_time);
    }

    return *renderSurfaceSlot;
#else
    (void)param1;
    (void)param2;
    std::fprintf(stderr,
        "STUB: UI_PaintWindow (0x425670) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_PaintWindow: host implementation not yet ported");
    return nullptr;
#endif
}

/* ================================================================== */
/* UI_OnMouseLeave                                                      */
/* Address: 0x4257F0                                                    */
/* ================================================================== */
void UI_OnMouseLeave(void* self)
{
#ifdef _WIN32
    uint8_t* const p = static_cast<uint8_t*>(self);

    int16_t* const overlayRefCount = reinterpret_cast<int16_t*>(p + 0x158);
    if (*overlayRefCount != 0) {
        *overlayRefCount -= 1;
    }

    void** const renderSurfaceSlot = reinterpret_cast<void**>(p + 0x10);
    const bool sticky = *reinterpret_cast<const uint8_t*>(p + 0x18) == 1;
    if (*overlayRefCount == 0 && *renderSurfaceSlot != nullptr && !sticky) {
        ReleaseSubObject(*renderSurfaceSlot);
        *renderSurfaceSlot = nullptr;

        const uint16_t subWindowCount = *reinterpret_cast<const uint16_t*>(p + 0x1A);
        if (subWindowCount != 0) {
            const uint8_t* const entryTable = *reinterpret_cast<const uint8_t* const*>(p + 0x20);
            for (uint16_t i = 0; i < subWindowCount; ++i) {
                const uint16_t stringId =
                    *reinterpret_cast<const uint16_t*>(entryTable + i * 0x18 + 0x0E);
                void* const strRes = ResourceManager_GetStringById(&g_resmgr, stringId);
                if (strRes != nullptr) {
                    RESMGR_ReleaseSoundResource(strRes);
                }
            }
        }
    }
#else
    (void)self;
    std::fprintf(stderr,
        "STUB: UI_OnMouseLeave (0x4257F0) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_OnMouseLeave: host implementation not yet ported");
#endif
}
