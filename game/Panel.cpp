/**
 * Panel.cpp — Panel implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

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

} // namespace
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

class UI_Manager;
extern UI_Manager* g_tooltip_mgr;            /* 0x4FD220 — tooltip manager instance */
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
extern int  __thiscall GameObject_GetRelPos(void* self, int* out,
                                            int param_1, int param_2);      /* 0x436A40 */

/* UI helpers */
extern void __thiscall UI_DestroyTooltip(void* mgr, int handle);           /* 0x423D20 */
/* UIPANEL_UnlockSurface — declared in Panel.h (0x42A3D0) */
extern int  __thiscall UI_IsBitmapReady(int handle);                        /* 0x4255F0 */

/* Win32 */
extern void __stdcall SetRect(void* rect, int left, int top,
                               int right, int bottom);
extern void __stdcall OffsetRect(void* rect, int dx, int dy);
extern int __stdcall IntersectRect(RECT* out, RECT* a, RECT* b);

/* DDRAW */
extern int __cdecl DDRAW_DimSurfaceRect(int left, int top,
                                         int right, int bottom);           /* 0x401540 */
extern void __cdecl CGWND_SetMode(void* mode);                             /* 0x407AF0 */
extern void __thiscall Town_SelectBuilding(void* town_view, int building); /* 0x42C9C0 */
/* Unused in this file. Address corrected: 0x46AA80 did not disassemble to
 * DDRAW_SelectBuilding; confirmed via Ghidra at 0x459180 (see
 * graphics/DDRAW.cpp for the real definition, which returns uint8_t). */
extern int __thiscall DDRAW_SelectBuilding(void* ddraw_building,
                                           int building);                  /* 0x459180 */

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
/* Real Windows 32-bit target: 0x58/0x68 are the original x86
 * sizeof(TrackPiece)/sizeof(SoundObject) exactly — safe as literals here
 * since this branch only ever builds for that original 32-bit layout (the
 * #ifndef _WIN32 branch above uses sizeof() directly for the 64-bit host,
 * where those values would be wrong due to pointer widening). */
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
/*                                                                      */
/* NOT a vtable slot (see Panel.h's own doc comment on this method —    */
/* Panel's real vtable slot [7] is Entity::StopSound, unmodified).      */
/* Called by: unnamed code at 0x453F63 (no function boundary defined    */
/* in Ghidra today; receiver class unresolved — see Panel.h).           */
/*                                                                      */
/* Selects a resource type (0, 2, 4, 6, 8, 10, 12, 14) based on a       */
/* decision table over panel_substate (+0xD4) and a second sign-        */
/* compared int historically read from +0xD8. That slot is now typed    */
/* `TrackPiece* enter_zoom_child` (see Panel.h) since nothing in the     */
/* reconstructed tree ever assigns it a plain int and HandleKey below    */
/* proves it's a pointer there; a real (non-null) pointer is never       */
/* sign-negative, so the three-way sign test collapses to a two-way      */
/* null/non-null test with no behavior change UNDER THAT MODEL. This is  */
/* a grep-based finding over the current source tree, not the original   */
/* binary: this method's own caller (address ~0x453F00) has no resolved  */
/* receiver class, so it is not proven that its own +0xD8 use is the     */
/* same pointer role GameView/HandleKey establish elsewhere — the "< 0"  */
/* branches below (target types 10/12/14) are dropped as unreachable     */
/* under that unverified-for-this-caller model, not a closed finding.    */
/* If the currently loaded resource type (+0x28, anim_index) doesn't    */
/* match, dispatches Entity::StopSound (vtable[7] — a generic "state     */
/* changed" notification here, not audio, matching                      */
/* GameView::center_on_point's identical reuse of the same slot) with   */
/* the new type. FIXED: previously mistranscribed as                    */
/* `this->UpdateResourceByState()` (unconditional self-recursion) —     */
/* the original disassembly's `(**(code**)(*this + 0x1c))(target_type)` */
/* is a slot-[7] dispatch with an argument, i.e. StopSound(target_type), */
/* not a no-arg recursive call into this same method.                   */
/* ================================================================== */
void Panel::UpdateResourceByState()
{
    int substate = this->panel_substate;                    /* +0xD4 */
    bool state_populated = (this->enter_zoom_child != nullptr); /* +0xD8 */
    int current_type = this->anim_index;                    /* +0x28 */
    int target_type;

    /* Decision table: (substate, state_populated) -> target_type. The
     * original's "state < 0" branches (target 10/12/14) are dead today —
     * see this method's doc comment — since enter_zoom_child is never a
     * raw negative sentinel, only nullptr or a real (non-negative) pointer. */
    if (substate > 0) {
        target_type = state_populated ? 6 : 8;
    } else if (substate == 0) {
        if (state_populated) {
            target_type = 4;
        } else {
            /* No change needed when both state and substate are zero */
            return;
        }
    } else { /* substate < 0 */
        if (state_populated) {
            target_type = 2;
        } else {
            /* Only dispatch if current type isn't already 0 */
            if (current_type == 0) {
                return;
            }
            target_type = 0;
        }
    }

    /* If the current resource type already matches, no change needed */
    if (current_type == target_type) {
        return;
    }

    /* Dispatch Entity::StopSound (vtable[7]) with the new resource type —
     * see this method's doc comment for why this is StopSound(target_type)
     * and not a recursive call into this same method. */
    this->StopSound(target_type);
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
        /* this->resource is the inherited Entity::resource/parent union
         * member (core/Entity.h) — was a locally-duplicated "surface_ref"
         * field here before Panel's Entity base class was recovered. */
        const auto* resource_surface = reinterpret_cast<const PanelResourceSurfaceFields*>(
            this->resource);
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
/* Panel::Draw — override of Entity::Draw (vtable[11])                 */
/* Address: 0x454900                                                   */
/*                                                                     */
/* Draws the Entity base, then dims overlapping child rects if the     */
/* update_child_flags (+0x88) is non-zero. For each child rect that    */
/* intersects clip_bounds, calls DDRAW_DimSurfaceRect.                 */
/* Skips dimming the +0xB0 rect if byte at +0xAD is non-zero.          */
/* ================================================================== */
void Panel::Draw(RECT clip_bounds, int enable_scroll, uint32_t extra_flags)
{
    this->Entity::Draw(clip_bounds, enable_scroll, extra_flags);

    if (this->update_child_flags != 0) {  /* +0x88 */
        RECT intersect_rect;

        /* Dim child_rect_b (+0xC0) if it intersects clip_bounds */
        if (IntersectRect(&intersect_rect, &this->child_rect_b, &clip_bounds)) {
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
            if (IntersectRect(&intersect_rect, &this->child_rect_a, &clip_bounds)) {
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
/* Tests if the point (x, y) hits any child sprite in the linked list   */
/* starting from child_surface (+0xD0).                                */
/* First calls PtInRect (vtable[2]) for quick rejection.                */
/* Then iterates the child linked list (next pointer at +0x28), calling */
/* HitTestChild (vtable[17]) ON `this` for each child. FIXED: previously*/
/* mistranscribed as a manual vtable call THROUGH the child's own vtable*/
/* (`child->vtable[0x11](child, x, y)`) — the disassembly shows ECX is  */
/* never reloaded from the child (`MOV ECX,ESI` before `CALL           */
/* [EAX+0x44]`, ESI = HitTestChildren's own `this`), and the child is    */
/* pushed as the first STACK argument, not the vtable-owning receiver.  */
/* Skips the "selected child" at +0x9C.                                 */
/*                                                                      */
/* NOTE: Panel::CreateChildSprite (0x4546D0, below) creates either a    */
/* TrackPiece (0x58 bytes) or a SoundObject (0x68 bytes) into this same  */
/* +0xD0 list depending on its `sound_res` argument, so the list is      */
/* genuinely heterogeneous. This loop (and the original binary, which    */
/* did the identical untyped vtable/offset walk) treats every node as    */
/* TrackPiece-shaped at +0x28 (next)/+0x48 (zoom_level, via              */
/* HitTestChild)/+0x54 (prev_frame) regardless of its real dynamic type  */
/* — a pre-existing behavior this reconstruction preserves exactly, not  */
/* a new assumption introduced here.                                     */
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

    /* Iterate child linked list. Chained via TrackPiece::sub_resource
     * (+0x28) — the same "next" pointer this codebase already established
     * as a real linked-list link despite its declared int32_t type (see
     * core/GameView.cpp's track_building/center_on_point, which walk this
     * exact +0xD0 list the same way). Dispatches HitTestChild (vtable[17])
     * ON `this` (the panel, confirmed via disassembly: `MOV ECX,ESI` before
     * `CALL [EAX+0x44]`, receiver unchanged from HitTestChildren's own
     * `this`) with the child passed as the first argument — NOT a manual
     * vtable call through the child's own vtable, which the prior
     * transcription here got backwards. */
    for (TrackPiece* child = static_cast<TrackPiece*>(this->child_surface);
         child != nullptr;
         child = reinterpret_cast<TrackPiece*>(
             static_cast<uintptr_t>(static_cast<uint32_t>(child->sub_resource)))) {

        if (child == reinterpret_cast<TrackPiece*>(
                static_cast<uintptr_t>(static_cast<uint32_t>(this->field_9C)))) {
            /* Skip the currently selected child */
            continue;
        }

        if (this->HitTestChild(child, rel_x, rel_y)) {
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
/* Enter (0x0D)  — if enter_zoom_child (+0xD8) has zoom_level == 1,    */
/*                 calls SetZoom(2) and sets prev_frame = 6.           */
/* Escape (0x1B) — if escape_zoom_child (+0xDC) has zoom_level == 1,   */
/*                 calls SetZoom(2) and sets prev_frame = 6.           */
/*                                                                     */
/* Returns 1 if handled, 0 if unhandled key.                           */
/* ================================================================== */
uint Panel::HandleKey(int key_code)
{
    switch (key_code) {
    case 0x0D:  /* Enter/Return */
    {
        TrackPiece* child = this->enter_zoom_child;
        if (child != nullptr && child->zoom_level == 1) {
            child->SetZoom(2);
            child->prev_frame = 6;
        }
        return 1;
    }

    case 0x1B:  /* Escape */
    {
        TrackPiece* child = this->escape_zoom_child;
        if (child != nullptr && child->zoom_level == 1) {
            child->SetZoom(2);
            child->prev_frame = 6;
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
/* In the binary, base Panel's own vtable entry for this slot (0x467E90)*/
/* pushes 0x19 and calls __amsg_exit (the MSVC _purecall handler that   */
/* terminates the program) — a genuine "must be overridden" trap, not a */
/* real implementation. GameView (core/GameView.cpp) is the confirmed    */
/* override (0x42D6B0); other Panel subclasses may override it too, but */
/* none are confirmed in this pass.                                     */
/*                                                                      */
/* We return 0 ("not hit") as a safe host default for the base class    */
/* rather than reproducing the original's process-terminating trap.     */
/* ================================================================== */
uint8_t Panel::HitTestChild(TrackPiece* /*child*/, int /*x*/, int /*y*/) { return 0; }
