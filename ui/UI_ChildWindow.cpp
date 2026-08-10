/**
 * UI_ChildWindow.cpp — ChildWindow base class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "UI_ChildWindow.h"

#include <cassert>
#include <cstdio>

// Status: INTEGRATED

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
extern void* g_asset_mgr;   /* 0x485600 — Asset manager singleton */
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
/* ChildWindow::ChildWindow (Constructor)                             */
/* Address: 0x424AF0 (wrapper) + 0x424BF0 (init body)                  */
/* ================================================================== */
ChildWindow::ChildWindow(uint32_t resourceId, int32_t nameParam)
{
    /* Delegate to InitFields to populate all member variables */
    InitFields(resourceId, nameParam);
}

/* ================================================================== */
/* ChildWindow::InitFields (Factored init body)                       */
/* Address: 0x424BF0 (UI_ChildWindow_Create body)                     */
/* ================================================================== */
void ChildWindow::InitFields(uint32_t resourceId, int32_t nameParam)
{
    /* Common field initialization (both _WIN32 and host) */
    this->resourceId = resourceId;
    this->resourceType = static_cast<uint8_t>(GetResourceType(resourceId));
    this->streamData = nullptr;
    this->renderSurface = nullptr;
    this->field_14 = 0;
    this->field_16 = 0;
    this->sticky = 0;
    this->subWindowCount = 0;
    this->field_1C = 0;
    this->field_1E = 0;
    this->heapBuffer = nullptr;
    this->subObject = nullptr;
    this->field_28 = 0;
    this->field_2A = 0;
    this->frameCount = 0;
    this->roadOffsetX = 0;
    this->roadOffsetY = 0;
    this->field_38 = 0;
    this->field_3C = 0;
    this->depResourceId1 = -1;
    this->depResourceId2 = -1;
    this->field_14D = 0;
    this->overlayRefCount = 0;
    this->field_15C = -1;
    this->field_160 = 1;
    this->loaded = 0;
    this->ready = 1;
    this->animFlags = 0;

    /* Conditional resource loading (nameParam != 0 path).
     * This branch is not exercised by any current caller in the codebase;
     * both CursorEditWindow and TrainStation pass nameParam=0 and handle
     * their own resource loading separately. The branch is deferred pending
     * recovery of CRT_sprintf_buf's vararg signature. */
    if (nameParam != 0) {
        std::fprintf(stderr,
            "STUB: ChildWindow InitFields (0x424BF0) nameParam!=0 resource-"
            "loading branch reached — not yet ported (see PROGRESS.md).\n");
        assert(false &&
               "ChildWindow InitFields: nameParam!=0 branch not yet ported");
    }
}

/* ================================================================== */
/* ChildWindow::~ChildWindow (Destructor)                             */
/* Address: 0x424BA0 (UI_ChildWindow_Dtor — real cleanup body)         */
/* ================================================================== */
ChildWindow::~ChildWindow()
{
    /* Clear the loaded flag — non-vtable operation, works on all platforms */
    loaded = 0;

#ifdef _WIN32
    /* Release renderSurface sub-object (if present).
     * Uses the scalar-deleting-destructor convention (vtable[0] with flags=1),
     * which is Windows x86 ABI specific. */
    if (renderSurface != nullptr) {
        ReleaseSubObject(renderSurface);
        renderSurface = nullptr;
    }

    /* Free heapBuffer (if present) — works on all platforms */
    if (heapBuffer != nullptr) {
        GLOBAL_free(heapBuffer);
        heapBuffer = nullptr;
    }

    /* Release subObject sub-object (if present) — Windows x86 ABI specific */
    if (subObject != nullptr) {
        ReleaseSubObject(subObject);
        subObject = nullptr;
    }
#else
    /* Host-path: The sub-object releases require the scalar-deleting-
     * destructor ABI (calling vtable[0] with flags=1), which is a Windows
     * x86 detail. On the host build, these objects are never created
     * (OnMouseMove is a no-op), so the releases are not reachable; free
     * heapBuffer as a safety measure. */
    if (heapBuffer != nullptr) {
        GLOBAL_free(heapBuffer);
        heapBuffer = nullptr;
    }
#endif
}

/* ================================================================== */
/* ChildWindow::OnMouseMove (Render handler)                          */
/* Address: 0x425670 (UI_PaintWindow)                                 */
/* Vtable slot: [1] +0x04                                              */
/* ================================================================== */
void* ChildWindow::OnMouseMove(int32_t x, int32_t y)
{
#ifdef _WIN32
    if (field_160 == 0) {
        return nullptr;
    }

    if (renderSurface == nullptr) {
        void* const raw = operator_new(0x20);
        void* const surface = (raw != nullptr) ? UIPANEL_CreateSurface(raw) : nullptr;
        renderSurface = surface;
        if (surface == nullptr) {
            return nullptr;
        }
        UIPANEL_StretchBlit(surface, reinterpret_cast<LPCSTR>(&bmpPath[0]), 0,
                             static_cast<uint32_t>(x), y);
    }

    int32_t* const surfaceWords = static_cast<int32_t*>(renderSurface);
    if (surfaceWords[6] == 0 && surfaceWords[7] == 0) {
        ReleaseSubObject(renderSurface);
        renderSurface = nullptr;
        return nullptr;
    }

    field_14 = static_cast<int16_t>(
        static_cast<uint32_t>(surfaceWords[2]) /
        static_cast<uint16_t>(field_160));
    const int16_t surfaceField0C = *reinterpret_cast<const int16_t*>(&surfaceWords[3]);
    overlayRefCount += 1;
    field_16 = surfaceField0C;

    if (ready == 0) {
        INPUT_EditScrollHandler(&DAT_004a99b0, resourceId);
    }

    if (subWindowCount != 0) {
        const uint8_t* const entryTable = static_cast<const uint8_t*>(heapBuffer);
        for (uint16_t i = 0; i < subWindowCount; ++i) {
            const uint16_t stringId =
                *reinterpret_cast<const uint16_t*>(entryTable + i * 0x18 + 0x0E);
            void* const strRes = ResourceManager_GetStringById(&g_resmgr, stringId);
            if (strRes != nullptr) {
                RESMGR_LoadSoundResource(strRes);
            }
        }
    }

    if (resourceId == 0x842) {
        ResourceManager_AnimateClock(&g_resmgr, g_game_time);
    }

    return renderSurface;
#else
    (void)x;
    (void)y;
    std::fprintf(stderr,
        "TODO: ChildWindow::OnMouseMove (0x425670) on host build — requires "
        "Windows UIPANEL rendering API (see PROGRESS.md).\n");
    return nullptr;
#endif
}

/* ================================================================== */
/* ChildWindow::OnMouseLeave                                           */
/* Address: 0x4257F0 (UI_OnMouseLeave)                                */
/* Vtable slot: [2] +0x08                                              */
/* ================================================================== */
void ChildWindow::OnMouseLeave()
{
    if (overlayRefCount != 0) {
        overlayRefCount -= 1;
    }

#ifdef _WIN32
    const bool isStickyWindow = (sticky == 1);
    if (overlayRefCount == 0 && renderSurface != nullptr && !isStickyWindow) {
        ReleaseSubObject(renderSurface);
        renderSurface = nullptr;

        if (subWindowCount != 0) {
            const uint8_t* const entryTable = static_cast<const uint8_t*>(heapBuffer);
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
    /* Host-path: The renderSurface is never created (OnMouseMove is a no-op),
     * so the release and sound-cleanup branches are unreachable. */
#endif
}

/* ================================================================== */
/* ChildWindow::Render (Stream parsing handler)                       */
/* Address: 0x424E00 (UI_ChildWindow_Render)                          */
/* Vtable slot: [3] +0x0C                                              */
/* ================================================================== */
uint8_t ChildWindow::Render(void* stream)
{
    (void)stream;
    std::fprintf(stderr,
        "STUB: ChildWindow::Render (0x424E00) reached — TODO: decompile "
        "0x424E00 (see ui/UI_ChildWindow.h and PROGRESS.md for why this "
        "one is deferred rather than transcribed).\n");
    assert(false && "ChildWindow::Render: not yet ported (TODO: decompile 0x424E00)");
    return 0;
}

/* ================================================================== */
/* ChildWindow::IsBitmapReady (Non-virtual member)                     */
/* Address: 0x4255F0 (UI_IsBitmapReady)                               */
/* ================================================================== */
bool ChildWindow::IsBitmapReady() const
{
    /* This logic is the same on both _WIN32 and host paths, as it uses only
     * named fields and ResourceManager calls. */
    if (ready == 0) {
        return false;
    }
    if (subObject == nullptr) {
        return false;
    }
    if (frameCount == 0) {
        return false;
    }

    /* NOTE: ResourceManager_GetById is called unconditionally for dep1,
     * even when depResourceId1 == -1. The assembly at 0x42560B..0x425614
     * calls it always, then checks the ID afterward (CMP + JZ at 0x425619).
     * If ResourceManager_GetById has side effects, they must occur. */
    void* const res1 = ResourceManager_GetById(&g_resmgr, depResourceId1);
    if (depResourceId1 != -1) {
        const bool dep1Ready = res1 != nullptr &&
            *reinterpret_cast<const int16_t*>(static_cast<const uint8_t*>(res1) + 0x158) != 0;
        if (!dep1Ready) {
            return false;
        }
    }

    void* const res2 = ResourceManager_GetById(&g_resmgr, depResourceId2);
    const bool dep2Ready = res2 != nullptr &&
        *reinterpret_cast<const int16_t*>(static_cast<const uint8_t*>(res2) + 0x158) != 0;
    /* NOTE: Assembly at 0x425647 uses JA (unsigned >), so when dep2Ready is
     * true (res2[0x158] > 0), the function returns false (not ready).
     * This is the original behavior per disassembly 0x4255F0. */
    if (dep2Ready) {
        return false;
    }

    /* Scenario-mode special case for resource 0xC42 (matches the same
     * g_netman[0x17].scenarioId idiom already used in
     * game/ScriptedObject.cpp). */
    if (resourceId == 0xC42) {
        const int32_t scenarioId =
            *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(g_netman) + 0x7C4);
        if (scenarioId == 2) {
            return false;
        }
    }

    return true;
}

/* ================================================================== */
/* Compatibility Shims (extern "C")                                    */
/* ================================================================== */

extern "C" {

/**
 * UI_CreateChildWindow — ChildWindow "constructor" shim.
 * Address: 0x424AF0
 *
 * Forwards to ChildWindow's InitFields method after casting void*
 * to ChildWindow*. Kept for compatibility with existing callers
 * (CursorEditWindow, TrainStation_Ctor) that may not be converted to C++
 * in this batch. These shims can be removed as each derived class's
 * callers migrate to direct C++ constructor calls.
 */
void* UI_CreateChildWindow(void* self, uint32_t resourceId, int32_t nameParam)
{
#ifdef _WIN32
    /* Cast the pre-allocated derived-object pointer and call InitFields
     * to populate base-class fields. Mirrors the assembly behavior of
     * 0x424AF0: zeroes fields, stages the vtable, and delegates to 0x424BF0. */
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    obj->InitFields(resourceId, nameParam);
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

/**
 * UI_ChildWindow_Create — Init-body shim.
 * Address: 0x424BF0
 *
 * This was the init-body helper called from UI_CreateChildWindow.
 * In the C++ class, the logic is in InitFields(). This shim is kept
 * for reference; no external caller invokes it directly in current code.
 */
void UI_ChildWindow_Create(void* self, uint32_t resourceId, int32_t nameParam)
{
#ifdef _WIN32
    /* If called directly (which shouldn't happen), delegate to InitFields */
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    obj->InitFields(resourceId, nameParam);
#else
    (void)self;
    (void)resourceId;
    (void)nameParam;
    std::fprintf(stderr,
        "STUB: UI_ChildWindow_Create (0x424BF0) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_ChildWindow_Create: host implementation not yet ported");
#endif
}

/**
 * UI_ChildWindow_Dtor — Destructor shim.
 * Address: 0x424BA0
 *
 * Forwards to ChildWindow destructor after casting. Qualified to call
 * the base implementation directly (not virtual dispatch), which prevents
 * infinite recursion if called from a derived-class override's chain.
 */
void UI_ChildWindow_Dtor(void* self)
{
#ifdef _WIN32
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    /* Qualified destructor call forces base implementation, matching the
     * assembly's direct call to 0x424BA0. */
    obj->ChildWindow::~ChildWindow();
#else
    (void)self;
    std::fprintf(stderr,
        "STUB: UI_ChildWindow_Dtor (0x424BA0) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_ChildWindow_Dtor: host implementation not yet ported");
#endif
}

/**
 * UI_ChildWindow_Render — Render method shim (stub).
 * Address: 0x424E00
 *
 * Qualified to call the base implementation directly (not virtual dispatch).
 */
uint8_t UI_ChildWindow_Render(void* self, void* stream)
{
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    /* Qualified call forces base implementation, matching the assembly. */
    return obj->ChildWindow::Render(stream);
}

/**
 * UI_IsBitmapReady — IsBitmapReady method shim.
 * Address: 0x4255F0
 *
 * NOTE: Original signature takes int32_t (truncated pointer); shim handles
 * the conversion back to ChildWindow* for the member call. On the host build,
 * real callers (Town::handle_tile_click, RESDATA_ScriptedObject::Start) pass
 * unrelated bridge objects, not ChildWindow instances, so this shim's host
 * path is a loud stub.
 */
int32_t UI_IsBitmapReady(int32_t self)
{
#ifdef _WIN32
    const uint8_t* const p =
        reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(self)));
    const ChildWindow* const obj = reinterpret_cast<const ChildWindow*>(p);
    return obj->IsBitmapReady() ? 1 : 0;
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

/**
 * UI_PaintWindow — OnMouseMove method shim.
 * Address: 0x425670
 *
 * Qualified to call the base implementation directly (not virtual dispatch).
 */
void* UI_PaintWindow(void* self, int32_t param1, int32_t param2)
{
#ifdef _WIN32
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    /* Qualified call forces base implementation. */
    return obj->ChildWindow::OnMouseMove(param1, param2);
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

/**
 * UI_OnMouseLeave — OnMouseLeave method shim.
 * Address: 0x4257F0
 *
 * Qualified to call the base implementation directly (not virtual dispatch).
 */
void UI_OnMouseLeave(void* self)
{
#ifdef _WIN32
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    /* Qualified call forces base implementation, matching the assembly's
     * direct call to 0x4257F0. */
    obj->ChildWindow::OnMouseLeave();
#else
    (void)self;
    std::fprintf(stderr,
        "STUB: UI_OnMouseLeave (0x4257F0) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_OnMouseLeave: host implementation not yet ported");
#endif
}

} // extern "C"
