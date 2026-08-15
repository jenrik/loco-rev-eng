/**
 * Panel.h — UI panel widget class (extends GameObject)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Panel is a UI widget subclass of GameObject used for in-game panels
 * (building panels, info panels, etc.). It manages a child surface,
 * tooltip, and a state machine that selects the displayed resource
 * type based on (panelState, panelSubstate) fields.
 *
 * Size: >= 0xDC bytes (extends Entity)
 * Vtable: 0x4784C8 (VTBL_PANEL)
 *
 * Class hierarchy (verified via RESDATA_BaseInit @ 0x4544E0, which keeps
 * ECX unmodified through CALL 0x00405790, Entity's own
 * (resource_id, anim_idx, world_x, world_y) constructor):
 *   GameObject
 *     └─ Entity
 *          └─ Panel  ← this class
 *
 * Vtable layout:
 *   [0]  +0x00: scalar deleting destructor (Panel_Dtor_ScalarDeleting, 0x454580)
 *   [1]  +0x04: InvalidateRect (inherited from Entity/GameObject)
 *   [2]  +0x08: PtInRect (inherited from Entity/GameObject)
 *   [3]  +0x0C: SetWorldPos/MoveTo (inherited from Entity, overrides GameObject::MoveTo)
 *   [4]  +0x10: InvokeCallback1 (inherited from Entity/GameObject)
 *   [5]  +0x14: InvokeCallback2 (inherited from Entity/GameObject)
 *   [6]  +0x18: Init (Panel_Init, 0x454680) — overrides Entity::InitBase
 *   [7]  +0x1C: UpdateResourceByState (0x453FB0) — state-driven resource selector
 *   [8]  +0x20: SetFrame (inherited from Entity)
 *   ...  +0x24+: (inherited from Entity, see core/Entity.h for slots [9]-[14])
 */

#pragma once

#include "../core/Entity.h"

// Status: TRANSCRIBED
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
class Panel : public Entity {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* GameObject fields (0x00..0x23) and Entity fields (0x24..0x87,
     * including resource/parent at +0x40 and anim_index at +0x28) are
     * inherited — see core/GameObject.h and core/Entity.h. Panel's own
     * fields begin immediately afterward, at +0x88. */

    /* Byte +0x88: "update_child" flag — when non-zero, TileMap_InvalidateRect
       is called on the child rects during UpdateChild. Also checked in
       DispatchEvent and SetPosition. */
    uint8_t  update_child_flags;  // +0x88

    uint8_t  _pad_89[7];          // +0x89..+0x8F
    int32_t  drag_active;         // +0x90  non-zero during active drag
    int32_t  drag_offset_x;       // +0x94  cursor offset X during drag
    int32_t  drag_offset_y;       // +0x98  cursor offset Y during drag
    int32_t  field_9C;            // +0x9C  child pointer for comparison
    uint8_t  _pad_A0[0x04];       // +0xA0..+0xA3 (was tooltip_handle + gap)

    int32_t  tooltip_handle;      // +0xA0  handle for UI tooltip (destroyed in dtor)

    uint8_t  _pad_A4[0x09];       // +0xA4..+0xAC
    uint8_t  dim_flag;            // +0xAD  dimension/sizing flag

    /* RECT at +0xB0: "inner" child rectangle — used in SetPosition, HitTestChildren.
       Set from screen_rect.right (+0x10) and screen_rect.bottom (+0x14). */
    RECT     child_rect_a;        // +0xB0  first child bounding rect

    /* RECT at +0xC0: "outer" child rectangle — also used in SetPosition, DispatchEvent.
       Set from screen_rect.left (+0x08) and screen_rect.right (+0x10). */
    RECT     child_rect_b;        // +0xC0  second child bounding rect

    void*    child_surface;       // +0xD0  child surface/panel (destroyed in dtor)
    int32_t  panel_substate;      // +0xD4  sub-state for resource type selection
    int32_t  panel_state;         // +0xD8  main state for resource type selection
    void*    child_ptr;           // +0xDC  child object pointer (also used as int)

    /* ================================================================ */
    /* Virtual methods                                                   */
    /* ================================================================ */

    /**
     * Virtual destructor (vtable[0]).
     * Address: 0x454580
     *
     * In the binary: scalar deleting destructor calls DtorBody, then
     * GLOBAL_free if flags & 1.
     */
    virtual ~Panel();

    /**
     * Init — initialize Panel with resource data (vtable[6]).
     * Address: 0x454680
     *
     * Calls GameObject::InitBase to load the resource. If the resource
     * type is not 0x2401 (UI_PANEL), unlocks the child surface.
     *
     * Called by: Panel construction sites
     *
     * @param resource_id  int — resource ID to load (0x2401 = UI_PANEL)
     * @param anim_index   int — animation index
     * @param force_reload byte — force resource reload flag
     * @return             byte — 1 on success, 0 on failure
     */
    virtual byte Init(int resource_id, int anim_index, byte force_reload);

    /**
     * UpdateResourceByState — select resource type based on internal state (vtable[7]).
     * Address: 0x453FB0
     *
     * State-machine that maps (panel_state, panel_substate) to a resource
     * type value (0, 2, 4, 6, 8, 10, 12, or 14). If the current resource
     * at +0x28 differs from the target, dispatches vtable[7] with the new
     * type to trigger a resource update.
     */
    virtual void UpdateResourceByState();

    /**
     * Hit-test dispatch on child objects (vtable slot 17 = 0x44).
     */
    virtual int HitTestChild(int x, int y);

    /* ================================================================ */
    /* Non-virtual methods                                               */
    /* ================================================================ */

    /**
     * Destructor body — releases panel resources.
     * Address: 0x4545A0
     *
     * Destroys child surface at +0xD0, tooltip at +0xA0, notifies
     * parent via vtable[6], then calls GameObject destructor.
     * Sets this->vtable to VTBL_PANEL during execution.
     */
    void DtorBody();

    /**
     * Partial destructor helper — destroys child surface and tooltip,
     * calls vtable[6] notification, but does NOT call GameObject base
     * destructor. Used by Panel subclass destructors.
     *
     * Address: 0x454630
     * __fastcall (ECX = this)
     */
    void PartialDtor();

    /**
     * Create a child sprite (TrackPiece or SoundObject) on this panel.
     * Address: 0x4546D0
     * __thiscall
     *
     * Creates either a TrackPiece (0x58 bytes, if param_3 == 0) or a
     * SoundObject (0x68 bytes, if param_3 != 0) child sprite and stores
     * it at +0xD0.
     *
     * @param res_handle  Resource handle for the child sprite
     * @param z_order     Z-ordering parameter (ushort)
     * @param sound_res   Sound resource handle (0 = TrackPiece, non-0 = SoundObject)
     * @return            Child sprite pointer, or DAT_00485270 (global default)
     */
    void* CreateChildSprite(int res_handle, ushort z_order, int sound_res);

    /**
     * Set position — calls GameObject_SetWorldPos, updates child RECTs
     * at +0xB0 and +0xC0, then invalidates via vtable[4].
     * Address: 0x454820
     * __thiscall
     */
    void SetPosition(int x, int y);

    /**
     * Update child — invalidates parent and tile rects for child areas.
     * Address: 0x454890
     * __fastcall (ECX = this)
     */
    void UpdateChild();

    /**
     * Draw override — vtable[11] (Entity::Draw's own slot). Draws the
     * Panel, then dims overlapping child rects on the screen surface.
     * Address: 0x454900. Ghidra's own label for this address,
     * "RESDATA_DispatchEvent"/"DispatchEvent", is a naming artifact, not
     * a distinct method — confirmed a genuine same-signature override of
     * Entity::Draw via direct vtable[11] byte comparison (see
     * core/GameView.cpp's render_selection, PROGRESS.md 2026-08-13).
     */
    void Draw(RECT clip_bounds, int enable_scroll, uint32_t extra_flags) override;

    /**
     * Hit-test children — finds if a click point hits any child sprite
     * in the linked list at +0xD0.
     * Address: 0x4549E0
     * __thiscall
     *
     * @param x  Screen X coordinate
     * @param y  Screen Y coordinate
     * @return   1 if a child was hit, 0 otherwise
     */
    char HitTestChildren(int x, int y);

    /**
     * Handle keyboard events for scripted objects (vtable[16] = +0x40).
     * Address: 0x454AE0
     * __thiscall
     *
     * Enter (0x0D) triggers Zoom on +0xD8 child with type check.
     * Escape (0x1B) triggers Zoom on +0xDC child with type check.
     *
     * Confirmed virtual: core/GameView.h documents vtable slot [16] +0x40
     * as "Panel::HandleKey (0x454AE0, inherited)" for GameView (a Panel
     * subclass whose vtable was read directly from raw bytes) — the same
     * address appears unchanged at the same slot, i.e. GameView does not
     * override it. MainWndProc (0x4618C0) dispatches WM_KEYDOWN/WM_CHAR
     * through g_active_panel's vtable at this exact offset, so any
     * concrete Panel-family object stored there must reach this override
     * through real virtual dispatch, not a fixed non-virtual call.
     *
     * @param key_code  Virtual key code
     * @return          uint (or modified EAX from callees)
     */
    virtual uint HandleKey(int key_code);
};

/* ================================================================== */
/* External helper declarations                                        */
/* ================================================================== */
extern void __thiscall UI_DestroyTooltip(void* mgr, int handle);   /* 0x423D20 */
extern void __thiscall GameObject_DtorBody(void* self);             /* 0x405870 */
extern void  __thiscall UIPANEL_UnlockSurface(void* surface);       /* 0x42A3D0 */
extern void __thiscall GameObject_SetWorldPos(void* self, int x, int y); /* 0x405C00 */
extern void __thiscall GameObject_InvalidateRect(void* self);       /* 0x436AB0 */
extern int  __thiscall GameObject_GetRelPos(void* self, int* out, int x, int y); /* 0x436A40 */
extern void __stdcall SetRect(void* rect, int left, int top, int right, int bottom);
extern void __stdcall OffsetRect(void* rect, int dx, int dy);
extern int  __stdcall IntersectRect(RECT* out, RECT* a, RECT* b);
/* Always returns 1 (see native/DDRAW_DimSurfaceRect.c); every caller
 * discards it. Address corrected to 0x401540 (confirmed via Ghidra
 * disassembly/decompile) -- 0x469A50 had zero xrefs anywhere in the
 * binary. */
extern int __cdecl DDRAW_DimSurfaceRect(int left, int top, int right, int bottom); /* 0x401540 */
extern void* __cdecl operator_new(size_t size);                     /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);                         /* 0x465CD0 */
extern int   __thiscall UI_IsBitmapReady(int handle);                /* 0x424C30 */
extern void* __cdecl ResourceManager_GetById(void** resmgr, UINT id); /* 0x4472B0 */
extern void  __fastcall CGWND_TrackPiece_SetZoom(void* obj, int zoom); /* 0x47xxxx */
