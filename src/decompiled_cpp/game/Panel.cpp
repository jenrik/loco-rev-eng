/**
 * Panel.cpp — Panel implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "Panel.h"
#include "TrackPiece.h"
#include "../resources/ResourceManager.h"
#include <new>
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */

namespace {

struct PanelResourceSurfaceFields {
    uint8_t prefix_00_0f[0x10];
    void* surface;
};

struct SurfaceLockFields {
    uint8_t prefix_00_03[4];
    int32_t locked;
};

struct ChildLinkFields {
    uint8_t prefix_00_27[0x28];
    void* next;
};

struct ChildSpriteFields {
    void** vtable;
    uint8_t prefix_04_27[0x24];
    ChildSpriteFields* next;
};

struct TrackChildStateFields {
    uint8_t prefix_00_47[0x48];
    int16_t type;
    uint8_t prefix_4a_53[0x0A];
    int16_t state;
};

} // namespace
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* g_tooltip_mgr;                  /* 0x4FD220 — tooltip manager instance */
extern void* DAT_00485270;                   /* 0x485270 — global sprite/return value */
extern void* g_font_normal;                  /* 0x485478 — normal font handle */
extern int32_t g_game_mode;                  /* 0x4851F4 */
extern void* g_about;                        /* 0x4FD428 — AboutDialog singleton */
extern void* g_netman;                       /* 0x4FD3B8 — Netman singleton */
extern void* g_tilemap;                      /* 0x4AAD08 — tilemap singleton */
/* GLOBAL_free — already declared via compat.h */

/* GameObject methods (from core) */
extern int __thiscall GameObject_InitBase(void* self, int resource_id,
                                          int anim_index, byte force_reload); /* 0x405900 */
extern void __thiscall GameObject_SetWorldPos(void* self, int x, int y);    /* 0x405C00 */
extern void __thiscall GameObject_InvalidateRect(void* self);               /* 0x436AB0 */
extern void __thiscall GameObject_Draw(void* self);                         /* 0x405E60 */
extern int  __thiscall GameObject_GetRelPos(void* self, int* out,
                                            int param_1, int param_2);      /* 0x436A40 */

/* UI helpers */
extern void __thiscall UI_DestroyTooltip(void* mgr, int handle);           /* 0x423D20 */
/* UIPANEL_UnlockSurface — declared in Panel.h (0x42A3D0) */
extern int  __thiscall UI_IsBitmapReady(int handle);                        /* 0x424C30 */

/* Win32 */
extern void __stdcall SetRect(void* rect, int left, int top,
                               int right, int bottom);
extern void __stdcall OffsetRect(void* rect, int dx, int dy);
extern int __stdcall IntersectRect(RECT* out, RECT* a, RECT* b);

/* DDRAW */
extern void __cdecl DDRAW_DimSurfaceRect(int left, int top,
                                          int right, int bottom);          /* 0x469A50 */
extern void __cdecl CGWND_SetMode(void* mode);                             /* 0x407AF0 */
extern void __thiscall Town_SelectBuilding(void* town_view, int building); /* 0x42C9C0 */
extern void __thiscall DDRAW_SelectBuilding(void* ddraw_building,
                                            int building);                 /* 0x46AA80 */

/* Tilemap */
extern void __thiscall TileMap_InvalidateRect(void* tilemap, int left, int top,
                                               int right, int bottom);     /* 0x455840 */

/* Memory */
void* __cdecl operator_new(size_t size);                                   /* 0x465CE0 */

namespace {

#ifndef _WIN32
void* AllocateTrackPieceStorage()
{
    return operator_new(sizeof(TrackPiece));
}

void* AllocateSoundObjectStorage()
{
    return operator_new(sizeof(SoundObject));
}
#else
void* AllocateTrackPieceStorage()
{
    return operator_new(0x58);
}

void* AllocateSoundObjectStorage()
{
    return operator_new(0x68);
}
#endif

RESDATA* ResourceFromHandle(int res_handle)
{
    return reinterpret_cast<RESDATA*>(
        static_cast<uintptr_t>(static_cast<uint32_t>(res_handle)));
}

} // namespace

/* ================================================================== */
/* Panel::UpdateResourceByState                                         */
/* Address: 0x453FB0                                                    */
/* Vtable slot: [7] (+0x1C)                                             */
/*                                                                      */
/* Called by: unnamed code at 0x453F63                                  */
/*                                                                      */
/* Selects a resource type (0, 2, 4, 6, 8, 10, 12, 14) based on       */
/* the combined state of panel_state (+0xD8) and panel_substate        */
/* (+0xD4). If the currently loaded resource at +0x28 does not         */
/* match, dispatches the SetResource virtual to change it.             */
/* ================================================================== */
void Panel::UpdateResourceByState()
{
    int substate = this->panel_substate;  /* +0xD4 */
    int state = this->panel_state;        /* +0xD8 */
    int current_type = this->anim_index;  /* +0x28 */
    int target_type;

    /* Decision table: (substate, state) -> target_type */
    if (substate > 0) {
        if (state > 0) {
            target_type = 6;
        } else if (state == 0) {
            target_type = 8;
        } else { /* state < 0 */
            target_type = 10;
        }
    } else if (substate == 0) {
        if (state > 0) {
            target_type = 4;
        } else if (state == 0) {
            /* No change needed when both state and substate are zero */
            return;
        } else { /* state < 0 */
            target_type = 12;
        }
    } else { /* substate < 0 */
        if (state > 0) {
            target_type = 2;
        } else if (state == 0) {
            /* Only dispatch if current type isn't already 0 */
            if (current_type == 0) {
                return;
            }
            target_type = 0;
        } else { /* state < 0 */
            target_type = 14;
        }
    }

    /* If the current resource type already matches, no change needed */
    if (current_type == target_type) {
        return;
    }

    /* Dispatch the SetResource virtual method (vtable slot 7) */
    this->UpdateResourceByState();
}

/* ================================================================== */
/* Panel::DtorBody                                                     */
/* Address: 0x4545A0                                                   */
/*                                                                     */
/* Destructor body for Panel. Destroys child surface (+0xD0),          */
/* removes tooltip (+0xA0), notifies parent via vtable[6], then        */
/* calls GameObject destructor.                                        */
/* ================================================================== */
void Panel::DtorBody()
{
    /* Reset vtable to Panel's vtable during destruction */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Destroy child surface at +0xD0 */
    if (this->child_surface != nullptr) {          /* +0xD0 */
        /* Call vtable[0] (scalar deleting destructor) on child */
        using DtorFunc = void* (__thiscall*)(void* self, byte flags);
        DtorFunc dtor = reinterpret_cast<DtorFunc>(
            reinterpret_cast<void**>(this->child_surface)[0]);
        dtor(this->child_surface, 1);           /* delete with free */
        this->child_surface = nullptr;
    }

    /* Destroy tooltip at +0xA0 */
    if (this->tooltip_handle != 0) {            /* +0xA0 */
        UI_DestroyTooltip(g_tooltip_mgr, this->tooltip_handle);
        this->tooltip_handle = 0;
    }

    /* Notify parent via vtable[6] (set parent notification) with (0, -1, 0) */
    this->Init(0, -1, 0);

    /* Call base GameObject destructor body */
    GameObject_DtorBody(this);
}

/* ================================================================== */
/* Panel::scalar deleting destructor                                    */
/* Address: 0x454580                                                   */
/* Vtable slot: [0] (+0x00)                                            */
/*                                                                     */
/* Calls DtorBody, then optionally frees memory.                       */
/* ================================================================== */
Panel::~Panel()
{
    this->DtorBody();
}

/* ================================================================== */
/* Panel::Init                                                         */
/* Address: 0x454680                                                   */
/* Vtable slot: [6] (+0x18)                                            */
/*                                                                     */
/* Initializes Panel via GameObject::InitBase. If the resource type    */
/* is not 0x2401 (UI_PANEL), unlocks the child surface.                */
/* ================================================================== */
byte Panel::Init(int resource_id, int anim_index, byte force_reload)
{
    int result;

    result = GameObject_InitBase(this, resource_id, anim_index, force_reload);
    byte cResult = static_cast<byte>(result);

    if (cResult != 0 && resource_id != 0x2401) {
        const auto* resource_surface = reinterpret_cast<const PanelResourceSurfaceFields*>(
            this->surface_ref);
        void* surface = resource_surface->surface;
        int surface_locked = reinterpret_cast<const SurfaceLockFields*>(surface)->locked;

        if (surface_locked == 0) {
            UIPANEL_UnlockSurface(surface);
        }
    }

    return cResult;
}

/* ================================================================== */
/* Panel::PartialDtor                                                  */
/* Address: 0x454630                                                   */
/*                                                                     */
/* Partial destructor helper: destroys child surface (+0xD0) and       */
/* tooltip (+0xA0), then calls vtable[6] notification with (0,-1,0).  */
/* Does NOT call GameObject base destructor.                           */
/*                                                                     */
/* Used by: UIPANEL_ScrollPanel_Dtor @ 0x4274DC,                       */
/*          GameView_Cleanup @ 0x42CE07,                                */
/*          RESDATA_ScriptedObject_Shutdown @ 0x4495F5,                 */
/*          DDRAW_InvalidateAll @ 0x458C2E, etc.                       */
/* ================================================================== */
void Panel::PartialDtor()
{
    /* Destroy child surface at +0xD0 */
    if (this->child_surface != nullptr) {          /* +0xD0 */
        /* Call vtable[0] (scalar deleting destructor) on child with flag 1 */
        using DtorFunc = void* (__thiscall*)(void* self, byte flags);
        DtorFunc dtor = reinterpret_cast<DtorFunc>(
            reinterpret_cast<void**>(this->child_surface)[0]);
        dtor(this->child_surface, 1);
        this->child_surface = nullptr;
    }

    /* Destroy tooltip at +0xA0 */
    if (this->tooltip_handle != 0) {            /* +0xA0 */
        UI_DestroyTooltip(g_tooltip_mgr, this->tooltip_handle);
        this->tooltip_handle = 0;
    }

    /* Notify parent via vtable[6] with (0, -1, 0) */
    this->Init(0, -1, 0);
}

/* ================================================================== */
/* Panel::CreateChildSprite                                             */
/* Address: 0x4546D0                                                   */
/*                                                                     */
/* Creates a TrackPiece (0x58 bytes) or SoundObject (0x68 bytes)       */
/* child sprite and stores it at +0xD0. If param_3 == 0, creates a    */
/* TrackPiece; otherwise creates a SoundObject with the given sound.   */
/*                                                                     */
/* If +0xD0 is already occupied, creates a linked-list node before     */
/* the existing child (old child becomes next in chain via +0x28).     */
/*                                                                     */
/* Returns the child sprite pointer, or DAT_00485270 (global default)  */
/* if the resource bitmap is not ready.                                */
/* ================================================================== */
void* Panel::CreateChildSprite(int res_handle, ushort z_order, int sound_res)
{
    /* If no resource handle, return global default */
    if (res_handle == 0) {
        return DAT_00485270;
    }

    /* Check if the bitmap resource is ready */
    if (!UI_IsBitmapReady(res_handle)) {
        return DAT_00485270;
    }

    void* new_child;
    int cleanup_flag;

    if (this->child_surface != nullptr) {   /* +0xD0 — existing child, prepend to chain */
        if (sound_res == 0) {
            /* Create TrackPiece (0x58 bytes in the original x86 binary). */
            void* mem = AllocateTrackPieceStorage();
            cleanup_flag = 3;
            if (mem != nullptr) {
                new_child = ::new (mem) TrackPiece(
                    this, ResourceFromHandle(res_handle), z_order);
            } else {
                new_child = nullptr;
            }
        } else {
            /* Create SoundObject (0x68 bytes in the original x86 binary). */
            void* mem = AllocateSoundObjectStorage();
            cleanup_flag = 2;
            if (mem != nullptr) {
                new_child = ::new (mem) SoundObject(
                    sound_res, this, ResourceFromHandle(res_handle),
                    g_font_normal, z_order);
            } else {
                new_child = nullptr;
            }
        }
        /* The new child's vtable[?] is stored at DAT_00485270[10] and
           DAT_00485270 is updated to the new child pointer */
        ChildLinkFields* old_child = reinterpret_cast<ChildLinkFields*>(DAT_00485270);
        old_child->next = new_child;                              /* link old child as next */
        return old_child->next;                                   /* return new child */
    }

    /* No existing child — create first child */
    if (sound_res == 0) {
        void* mem = AllocateTrackPieceStorage();
        cleanup_flag = 1;
        if (mem != nullptr) {
            DAT_00485270 = ::new (mem) TrackPiece(
                this, ResourceFromHandle(res_handle), z_order);
        } else {
            DAT_00485270 = nullptr;
        }
    } else {
        void* mem = AllocateSoundObjectStorage();
        cleanup_flag = 0;
        if (mem != nullptr) {
            DAT_00485270 = ::new (mem) SoundObject(
                sound_res, this, ResourceFromHandle(res_handle),
                g_font_normal, z_order);
        } else {
            DAT_00485270 = nullptr;
        }
    }
    this->child_surface = DAT_00485270;   /* +0xD0 */

    return DAT_00485270;
}

/* ================================================================== */
/* Panel::SetPosition                                                   */
/* Address: 0x454820                                                   */
/*                                                                     */
/* Sets world position via GameObject_SetWorldPos, then updates two    */
/* child RECTs at +0xB0 and +0xC0 based on the current screen_rect.   */
/* The RECTs are offset by +0x32 (50) in certain directions.          */
/* Finally calls vtable[4] (InvalidateRect) to trigger redraw.         */
/* ================================================================== */
void Panel::SetPosition(int x, int y)
{
    GameObject_SetWorldPos(this, x, y);

    /* Build child_rect_b (+0xC0): based on screen_rect.right and bottom + height */
    /* left = screen_rect.left (+0x08) + 0x32 */
    /* top = screen_rect.bottom (+0x14)  */
    /* right = screen_rect.bottom (+0x14) + 0x32 */
    /* bottom = screen_rect.right (+0x10) + 0x31 */
    SetRect(
        &this->child_rect_b,       /* +0xC0 */
        this->screen_rect.left   + 0x32,   /* left = x + 50 */
        this->screen_rect.bottom,           /* top = height */
        this->screen_rect.bottom + 0x32,    /* right = height + 50 */
        this->screen_rect.right  + 0x31     /* bottom = width + 49 */
    );

    /* Build child_rect_a (+0xB0): based on screen_rect fields */
    /* left = screen_rect.right (+0x10) */
    /* top = screen_rect.bottom (+0x14) + 0x32 */
    /* right = screen_rect.bottom (+0x14) + 0x32 */
    /* bottom = screen_rect.right (+0x10) */
    SetRect(
        &this->child_rect_a,       /* +0xB0 */
        this->screen_rect.right,            /* left = width */
        this->screen_rect.bottom + 0x32,    /* top = height + 50 */
        this->screen_rect.bottom + 0x32,    /* right = height + 50 */
        this->screen_rect.right              /* bottom = width */
    );

    /* Invalidate via vtable[4] */
    this->InvalidateRect();
}

/* ================================================================== */
/* Panel::UpdateChild                                                   */
/* Address: 0x454890                                                   */
/*                                                                     */
/* Invalidates the parent rect, then invalidates tile rects for        */
/* child_rect_a (+0xB0) and child_rect_b (+0xC0) on the tilemap        */
/* if the update_child_flags byte (+0x88) is non-zero.                 */
/* ================================================================== */
void Panel::UpdateChild()
{
    GameObject_InvalidateRect(this);

    if (this->update_child_flags != 0) {  /* +0x88 */
        TileMap_InvalidateRect(
            g_tilemap,
            this->child_rect_a.left,   /* +0xB0 */
            this->child_rect_a.top,
            this->child_rect_a.right,
            this->child_rect_a.bottom
        );
        TileMap_InvalidateRect(
            g_tilemap,
            this->child_rect_b.left,   /* +0xC0 */
            this->child_rect_b.top,
            this->child_rect_b.right,
            this->child_rect_b.bottom
        );
    }
}

/* ================================================================== */
/* Panel::DispatchEvent                                                 */
/* Address: 0x454900                                                   */
/*                                                                     */
/* Draws the GameObject, then dims overlapping child rects if the      */
/* update_child_flags (+0x88) is non-zero. For each child rect that    */
/* intersects the given param_rect, calls DDRAW_DimSurfaceRect.        */
/* Skips dimming the +0xB0 rect if byte at +0xAD is non-zero.          */
/* ================================================================== */
void Panel::DispatchEvent(RECT* param_rect)
{
    GameObject_Draw(this);

    if (this->update_child_flags != 0) {  /* +0x88 */
        RECT intersect_rect;

        /* Dim child_rect_b (+0xC0) if it intersects param_rect */
        if (IntersectRect(&intersect_rect, &this->child_rect_b, param_rect)) {
            DDRAW_DimSurfaceRect(
                intersect_rect.left, intersect_rect.top,
                intersect_rect.right, intersect_rect.bottom
            );
        }

        /* Dim child_rect_a (+0xB0) if byte at +0xAD is clear */
        /* NOTE: +0xAD is within _pad_89 area — this might be a flag
           controlling whether rect_a is dimmed */
        uint8_t* dim_flag = &this->dim_flag;
        if (*dim_flag == 0) {
            if (IntersectRect(&intersect_rect, &this->child_rect_a, param_rect)) {
                DDRAW_DimSurfaceRect(
                    intersect_rect.left, intersect_rect.top,
                    intersect_rect.right, intersect_rect.bottom
                );
            }
        }
    }
}

/* ================================================================== */
/* Panel::HitTestChildren                                               */
/* Address: 0x4549E0                                                   */
/*                                                                     */
/* Tests if the point (param_1, param_2) hits any child sprite in the  */
/* linked list starting from child_surface (+0xD0).                    */
/* First calls vtable[2] (PtInRect) for quick rejection.               */
/* Then iterates child linked list (next pointer at +0x28), calling    */
/* vtable[0x11] (vtable[17] at +0x44) on each child for hit-test.     */
/* Skips the "selected child" at +0x9C.                                */
/* ================================================================== */
char Panel::HitTestChildren(int x, int y)
{
    char hit_any = 0;

    /* Quick rejection via vtable[2] (PtInRect) */
    if (!this->PtInRect(x, y)) {
        return 0;
    }

    /* Convert to relative coordinates */
    int rel_pos[2];
    GameObject_GetRelPos(this, rel_pos, x, y);
    int rel_x = rel_pos[0];
    int rel_y = rel_pos[1];

    /* Iterate child linked list (next pointer at +0x28) */
    /* NOTE: +0x9C is a "selected child" pointer that is SKIPPED during hit-testing */
    for (ChildSpriteFields* child = reinterpret_cast<ChildSpriteFields*>(this->child_surface);
         child != nullptr; child = child->next) {

        if (child == reinterpret_cast<ChildSpriteFields*>(this->field_9C)) {
            /* Skip the currently selected child */
            continue;
        }

        /* Call vtable[0x11] (slot 17 at +0x44) for hit-test on child */
        using ChildHitTestFunc = char (__thiscall*)(void* self, int x, int y);
        ChildHitTestFunc child_test = reinterpret_cast<ChildHitTestFunc>(
            child->vtable[0x11]);
        char hit = child_test(child, rel_x, rel_y);

        if (hit) {
            hit_any = 1;
        }
    }

    return hit_any;
}

/* ================================================================== */
/* Panel::HandleKey                                                     */
/* Address: 0x454AE0                                                   */
/*                                                                     */
/* Handles keyboard events for scripted objects on the panel.          */
/* Enter (0x0D) — if +0xD8 child has type byte == 1 at +0x48,         */
/*                calls CGWND_TrackPiece_SetZoom(2) and sets +0x54 = 6.*/
/* Escape (0x1B) — if +0xDC child has type byte == 1 at +0x48,        */
/*                 calls CGWND_TrackPiece_SetZoom(2) and sets +0x54=6. */
/*                                                                                                                                                                                                                                                    * Returns 1 if handled, 0 if unhandled key.                                                                                                                                                                                                          */
/* ================================================================== */
uint Panel::HandleKey(int key_code)
{
    switch (key_code) {
    case 0x0D:  /* Enter/Return */
    {
        void* child = reinterpret_cast<void*>(static_cast<uintptr_t>(this->panel_state));
        TrackChildStateFields* child_state = reinterpret_cast<TrackChildStateFields*>(child);
        if (child != nullptr && child_state->type == 1) {
            CGWND_TrackPiece_SetZoom(child, 2);
            child_state->state = 6;
        }
        return 1;
    }

    case 0x1B:  /* Escape */
    {
        void* child = this->child_ptr;
        TrackChildStateFields* child_state = reinterpret_cast<TrackChildStateFields*>(child);
        if (child != nullptr && child_state->type == 1) {
            CGWND_TrackPiece_SetZoom(child, 2);
            child_state->state = 6;
            return 1;
        }
        return 1;  /* Always returns 1 for Escape even if no child */
    }

    default:
        return 0;  /* Unhandled key */
    }
}

/* ================================================================== */
/* Panel::HitTestChild — vtable slot 17                                */
/*                                                                      */
/* In the binary, this is a pure virtual: the vtable entry at 0x467E90 */
/* pushes 0x19 and calls __amsg_exit (the MSVC _purecall handler that   */
/* terminates the program). Subclasses (UIPANEL, BuildingPanel, etc.)   */
/* override this slot with real hit-testing.                            */
/*                                                                      */
/* We return 0 ("not hit") as a safe default for the base class.        */
/* ================================================================== */
int Panel::HitTestChild(int x, int y) { return 0; }
