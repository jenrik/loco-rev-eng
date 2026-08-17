// Status: INTEGRATED
/**
 * PostcardAlbum.h — Postcard collection album window
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * RESOLVED 2026-08-17: this is now the SOLE definition of PostcardAlbum.
 * A second, competing, flat (non-inheriting) `class PostcardAlbum` used to
 * exist in graphics/LOCOBITMAP.h/.cpp (different base-class claim, different
 * field names — tile_offset/tile_shown_count/tile_text_buf/tile_left/
 * paint_inited/BlitToSurface) and mangled identically to several methods
 * defined here, which was a duplicate-symbol / silent-misbind hazard once
 * both translation units were linked together. That flat class and every
 * one of its method bodies have been deleted; graphics/LOCOBITMAP.h/.cpp now
 * contain only unrelated concepts (UIPANEL_Surface, DDRAW_PresentRect — see
 * that header's own doc comment).
 *
 * vtable[24]'s on_set_focus (0x405620) has no function defined at that
 * address in the database yet and remains out of scope for this class.
 *
 * PostcardAlbum is the postcard collection album window where players view
 * received postcard images, browse tile screenshots by name, scroll through
 * album pages, and manage their collection. It extends UI_WindowBase.
 *
 * Size: 0x254 bytes (allocated by CGWND_InitAllSubsystems @ 0x4072C8)
 * Vtable: 0x4773F0 (VTBL_POSTCARD_ALBUM)
 *
 * Class hierarchy:
 *   UI_WindowBase (VTBL_UI_WINDOWBASE, size 0xE8)
 *     └─ PostcardAlbum  <- this class (+0xE8..+0x254, total size 0x254)
 *
 * Vtable layout (0x4773F0, extends UI_WindowBase — verified via DATA xrefs):
 *   [0]  +0x00: scalar deleting destructor (PostcardAlbum_DestroyFromResource, 0x401FB0)
 *               -> ~PostcardAlbum -> FreeAllSprites
 *   [1]  +0x04: Hide / DestroyWindow (0x402660)            — overridden: hide()
 *   [2]  +0x08: Show (0x402590)                            — overridden: show()
 *               (dispatched by CGWND_SetMode(6) @ 0x408216)
 *   [3]  +0x0C: set_mode (inherited, 0x425FD0)
 *   [4]  +0x10: set_render_surface (inherited, 0x426020)
 *   [5]  +0x14: on_async_task_failure (inherited, 0x426130)
 *   [6]  +0x18: create_full_window (inherited, 0x425B70)
 *   [7]  +0x1C: on_create override (0x4028B0 — PORTED, see on_create() below;
 *               UI_WindowBase.h declares this slot `virtual void on_create()`.
 *               The override sets tiles_per_page (+0x11C) to 4 or 6 by
 *               resolution and lays out all sprite rects, gated on
 *               sprites_inited (+0x111) — resolves the former show()
 *               divide-by-zero hard blocker, see this file's top banner)
 *   [8]  +0x20: render/update (overridden, 0x404DB0, Ghidra name
 *               PostcardAlbum_OnUpdate — read via disassembly this pass to
 *               resolve RenderAllTiles/RenderTileName's real callers, but
 *               NOT yet ported to a named C++ method here; sets +0x112=1,
 *               blits the work rect, updates all element sprites, calls
 *               RenderAllTiles/EndPaintEx)
 *   [9]  +0x24: mousewheel slot (inherited no-op, 0x4661A0)
 *   [10] +0x28: (inherited, 0x426140)
 *   [11] +0x2C: WindowProc (overridden: GAMESTATE_WndProc, 0x40B4C0)
 *   [12] +0x30: UI_DefWndProc (0x422EA0)
 *   [13] +0x34: UI_DefWndProc (0x422EA0)
 *   [14] +0x38: click/key dispatch (0x404F60, Ghidra name
 *               PostcardAlbum_OnLButtonDown — read via disassembly this
 *               pass (verifying BlitElement/UpdateSprite's dead 2nd stack
 *               arg across all 12 of its call sites), but NOT yet ported to
 *               a named C++ method here; calls HitTest 0x403CD0 and
 *               dispatches element actions)
 *   [15] +0x3C: UI_DefWndProc (0x422EA0)
 *   [16] +0x40: on_rbutton_down override (0x4055E0) — overridden: this
 *               class's on_rbutton_down(). Confirmed via dispatch_message
 *               (0x426140): WM_RBUTTONDOWN (0x204) is the ONLY message
 *               routed through vtable+0x40; the sole DATA xref to 0x4055E0
 *               is this class's own vtable slot (0x477430). Ghidra's
 *               auto-generated name/comment for 0x4055E0 ("OnTimerTick" /
 *               "vtable[0x30] for GameObject") was WRONG on both counts —
 *               renamed. Releases selected_postcard_player (+0x130) if set
 *               and calls set_mode(childCount0, childObj0, 0, 1); a prior
 *               version of this table wrongly called the slot "OnTimerTick".
 *   [17] +0x44: UI_DefWndProc (0x422EA0)
 *   [18] +0x48: UI_DefWndProc (0x422EA0)
 *   [19] +0x4C: UI_DefWndProc (0x422EA0)
 *   [20] +0x50: on_mouse_move override (0x405680) — a prior version of this
 *               table wrongly labeled this slot "HitTest"; HitTest is the
 *               non-virtual helper at 0x403CD0 (see [14]/below), a
 *               different function entirely. 0x405680 itself is not yet
 *               decompiled/ported.
 *   [21] +0x54: PaintWindow (0x402690)                      — this class
 *   [22] +0x58: UI_DefWndProc (0x422EA0)
 *   [23] +0x5C: (0x426950 — UIPANEL region, not yet decompiled)
 *   [24] +0x60: on_set_focus override (0x405620 — not yet decompiled)
 *   [25] +0x64: UI_DefWndProc (0x422EA0)
 *   [26] +0x68: (0x426960 — UIPANEL region, not yet decompiled)
 *   [27] +0x6C: (0x426980 — UIPANEL region, not yet decompiled)
 *   [28] +0x70: (0x426A60 — UIPANEL region, not yet decompiled)
 *   [29] +0x74: UI_DefWndProc (0x422EA0)
 *   [30] +0x78: (0x426AC0 — UIPANEL region, not yet decompiled)
 *   [31] +0x7C: (0x426AD0 — UIPANEL region, not yet decompiled)
 *   [32] +0x80: (0x419A10 — Cursor region, not yet decompiled)
 *   [33]..[36]: UI_DefWndProc (0x422EA0)
 *
 * Note: the previously documented "[8] HitTest 0x403CD0 / [11] PaintWindow
 * 0x402690" slot mapping was WRONG. Verified vtable contents (0x4773F0
 * region): [8]=0x404DB0, [11]=GAMESTATE_WndProc 0x40B4C0, and
 * HitTest (0x403CD0) is a NON-virtual helper called by the slot-[14]
 * dispatch function; PaintWindow (0x402690) sits at slot [21].
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
#include "../ui/UI_WindowBase.h"
#include "../ui/ButtonSprite.h"
#include "../resources/ResourceObject.h" /* ResourceObject::Unlock() — real type of album_bg_resource, see below */

class DPlayManager;   /* network/DPlayManager.h — real type of selected_postcard_player, see below */

/* ================================================================== */
/* PostcardAlbum — postcard collection album window                     */
/* ================================================================== */

class PostcardAlbum : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* --- Inherited from UI_WindowBase (0x00..0xE8) --- */
    /* +0x00: vtable (compiler-managed)                                */
    /* +0x04: hInstance, +0x08: hWnd, +0x0C..+0xE7: see UI_WindowBase.h */
    /* +0xE4: visible                                                   */

    /* --- Album-specific fields (+0xE8..+0x254) --- */

    HICON      icon_handle;             // +0xE8  loaded by InitWindow (resource 0x65, MAKEINTRESOURCE)
    /* RETYPED 2026-08-17 (was `int32_t blit_dest_x`/`blit_dest_y` + 8 bytes
     * padding): the vtable[7] on_create override (0x4028B0) writes/reads all
     * 4 sub-fields as a real RECT via CopyRect((param+0xec),...) and
     * UI_CenterWindow(&imageRect, param+0xec) (both operate on all 4 ints,
     * not just left/top) — verified against disassembly (LEA EDI,[ESI+0xec]
     * passed whole to CopyRect/CALL EBX at 0x40292B/0x402944). BlitElement
     * (0x403E80), PostcardAlbum::on_update (0x404DB0), and RenderAllTiles
     * (0x404AC0) only ever read the .left/.top sub-fields (as a dx/dy blit
     * offset via OffsetRect(&r, this->bg_blit_rect.left, this->bg_blit_rect.top)),
     * which is why the prior pass modeled just two int32 scalars — but the
     * underlying storage is a genuine 16-byte RECT, written wholesale by
     * on_create to center the window rect within the loaded background
     * bitmap's own pixel bounds. */
    RECT       bg_blit_rect;            // +0xEC..+0xFB  background-image blit rect (on_create
                                        //  centers the window rect within the loaded album
                                        //  background bitmap here; .left/.top read elsewhere
                                        //  as a dx/dy blit offset)
    uint8_t    window_surface_inited;   // +0xFC  1 = album background surface created (one-shot guard)
    uint8_t    _pad_FD[3];              // +0xFD..+0xFF
    /* RETYPED 2026-08-17 (was 16 bytes of `_pad_FD` padding): on_create
     * (0x4028B0) writes this region as a RECT too (pRVar1->left=0;
     * +0x104=0; +0x108=800/1024; +0x10c=600/768; disassembly confirms
     * `LEA EBP,[ESI+0x100]` passed whole to UI_CenterWindow). Holds the
     * per-resolution design-canvas rect ({0,0,800,600} low-res or
     * {0,0,1024,768} high-res), then centered in place within workRect
     * (+0xD4) — the base reference every sprite rect in on_create is
     * computed from (CopyRect(&local, &layout_rect); OffsetRect(&local, dx, dy)).
     * Not read by any other method — verified by decompiling every
     * PostcardAlbum method with a defined function body (ctor,
     * InitFromResource, FreeAllSprites, InitWindow, show, hide,
     * PaintWindow/on_key_down, HitTest, BlitElement, UpdateSprite,
     * RenderTileName, RenderAllTiles, InitWindowSurface, InitSprites,
     * FreeSprites, on_update, the vtable[14] click dispatcher,
     * on_rbutton_down, on_mouse_move) and scanning every operand (both
     * hex- and decimal-spelled) for the +0xEC..+0x110 byte range: only
     * on_create itself ever touches it. The same sweep also confirmed
     * bg_blit_rect's .right/.bottom (+0xF4/+0xF8) are likewise written
     * only by on_create and read by no other swept method — every other
     * consumer (BlitElement/OnUpdate/RenderTileName/RenderAllTiles) reads
     * only .left/.top (+0xEC/+0xF0) as a dx/dy offset. NOT swept: the
     * vtable[24] on_set_focus slot (0x405620) has no function defined at
     * that address in the database yet (out of scope for this pass). */
    RECT       layout_rect;             // +0x100..+0x10F  per-resolution design-canvas rect
    uint8_t    active_flag;             // +0x110  1 = album busy/active (set by the slot-[8] render
                                        //         when narration plays; PaintWindow/HitTest ignore
                                        //         input while set). Cleared by InitFromResource,
                                        //         show() and the HelpWnd hide path.
    uint8_t    sprites_inited;          // +0x111  1 = AlbumSprites initialized (InitSprites)
    uint8_t    text_rendered;           // +0x112  1 = tile text labels rendered (set by slot-[8],
                                        //         cleared by hide())
    uint8_t    _pad_113;                // +0x113
    int32_t    scroll_pixel_offset;     // +0x114  current scroll pixel offset
    int32_t    tile_index;              // +0x118  current tile selection index
    int32_t    tiles_per_page;          // +0x11C  tiles per album page
    int32_t    hovered_tile;            // +0x120  currently hovered tile index
    int32_t    tile_count_init;         // +0x124  initial tile count (default 9)
    int32_t    scroll_wheel_pos;        // +0x128  scrollwheel position counter
    uint8_t    scroll_wheel_enabled;    // +0x12C  1 = scrollwheel navigation enabled
    uint8_t    _pad_12D[3];             // +0x12D..+0x12F
    /* RESOLVED 2026-08-17 (was `void* tile_preview_sprite`, guessed as "likely
     * a sprite"): real type is DPlayManager*, not a sprite. Evidence:
     * PixelDataCache::LookupAsset (0x401C10, graphics/PixelDataCache.h)
     * returns NET_ResolveAddress's result verbatim; NET_ResolveAddress is
     * declared `DPlayManager* NET_ResolveAddress(const char*)`
     * (network/DPlayManager.h:523); RenderTileName (0x4048E0) vtable[0]-
     * deletes the same LookupAsset result; matches the established
     * `Town::postcard_data`/`selected_player` DPlayManager* precedent
     * (town/Town.h:194-195). Set by the vtable[14] click dispatcher
     * (0x404F60) cases 8/10 via LookupAsset; from there it has three
     * possible fates, all inside 0x404F60:
     *   - TRANSFERRED to g_cursor->obj_184 via g_cursor->show(selected_postcard_player)
     *     (0x4054A3 and 0x405027 blocks) — cleared to null WITHOUT deleting
     *     afterward; Cursor::show()/base_destructor() takes ownership and
     *     deletes it later. This is correct, not a leak.
     *   - RETAINED unchanged after NetworkPlayerList::RegisterPlayer (0x4053E3 block).
     *   - DELETED via vtable[0] only when PixelDataCache::RemoveByAsset
     *     returns true, otherwise retained (0x40542E block).
     * No leak was found in PostcardAlbum::show() (0x402590): it deletes
     * this field before clearing it, and both transfer sites above null
     * it immediately after handing it to Cursor::show(), so a later show()
     * never sees a stale non-null value. */
    DPlayManager* selected_postcard_player; // +0x130 (was tile_preview_sprite)
    int32_t    is_high_res;             // +0x134  0=800x600 mode, 1=1024x768+ mode
                                        //          Determines resource 0x3C0A vs 0x3C0B
    /* RE-RESOLVED 2026-08-17 (a prior pass on this same date retyped this
     * FIELD to `ResourceObject*`, citing PostcardPreviewWindow::hide()'s
     * cast-through pattern as precedent — but that precedent is actually
     * `void* background_resource` in ui/PostcardPreviewWindow.h with a
     * LOCAL `static_cast<ResourceObject*>(...)->Unlock()` at the one call
     * site, not a field-level ResourceObject* retype; confirmed identical
     * void*-plus-local-cast pattern in town/Town.h (overlay_resource/
     * background_resource/button_strip_resource) and ui/NameEntryPanel.h.
     * Reverted to `void*` here for the same reason those use it: the
     * vtable[7] on_create override (0x4028B0) also reads this resource's
     * raw frame_width/frame_height header fields (RESDATA, shared/types.h)
     * for background-centering math — `static_cast<RESDATA*>` requires a
     * void* source (ResourceObject* and RESDATA* are unrelated types, so
     * that cast would need a forbidden reinterpret_cast if this field
     * stayed ResourceObject*-typed). Unlock() is now called via a local
     * `static_cast<ResourceObject*>(album_bg_resource)->Unlock()` instead,
     * matching every sibling class's resolution of this exact tension. */
    void*      album_bg_resource;       // +0x138  album background resource (0x3C0A/0x3C0B):
                                        //         static_cast<ResourceObject*> for Lock/Unlock,
                                        //         static_cast<RESDATA*> for frame_width/frame_height
    void*      album_bg_surface;        // +0x13C  album background surface (Lock(0,0) result).
                                        //         NOT cleared by FreeAllSprites (verified: only
                                        //         album_bg_resource is nulled there) — a genuine
                                        //         original quirk (stale pointer after release),
                                        //         preserved rather than "fixed."
    /* RE-RESOLVED 2026-08-17: see album_bg_resource above — reverted to
     * `void*` for the identical reason (on_create needs RESDATA*-typed
     * frame_width/frame_height reads off this same resource kind, which a
     * field-level ResourceObject* type cannot provide without a forbidden
     * reinterpret_cast). Unlock() now goes through a local
     * static_cast<ResourceObject*> at the FreeSprites() call site. */
    void*      photo_bg_resource;       // +0x140  photo background resource (0x3CFA)
    void*      photo_bg_surface;        // +0x144  photo background surface. NOT cleared by
                                        //         FreeSprites (verified: only photo_bg_resource
                                        //         is nulled there) — same stale-pointer quirk as
                                        //         album_bg_surface above, preserved not "fixed."

    /* Button sprites (8 x 0x24-byte ButtonSprite objects) */
    ButtonSprite* btn_close;            // +0x148  close button sprite (res 0x3C04)
    ButtonSprite* btn_delete;           // +0x14C  delete/trash sprite (res 0x3C09) — element 2
    ButtonSprite* btn_save;             // +0x150  save button sprite (res 0x3C05) — element 3
    ButtonSprite* btn_rotate;           // +0x154  rotate button sprite (res 0x3C08) — element 4
    ButtonSprite* btn_print;            // +0x158  print button sprite (res 0x3C0F) — element 9
    ButtonSprite* btn_prev;             // +0x15C  previous page sprite (res 0x3C06) — element 5
    ButtonSprite* btn_next;             // +0x160  next page sprite (res 0x3C07) — element 6
    ButtonSprite* btn_scrollwheel;      // +0x164  scrollwheel sprite (res 0x3C0C/0x3C0D) — element 7

    /* Row sprite groups (three separate 6-element arrays created by
     * InitFromResource: icons at +0x168, tiles at +0x180, names at +0x198) */
    ButtonSprite* row_icon[6];          // +0x168  icon sprites for 6 album rows (res 0)
    ButtonSprite* row_tile[6];          // +0x180  tile preview sprites (res 0x3C0E)
    ButtonSprite* row_name[6];          // +0x198  tile-name sprites (res 0) — rect used for
                                        //         DrawTextA in RenderAllTiles; hit-test target 10
    ButtonSprite* tile_label_sprites[9];// +0x1B0  label sprites for individual tiles (res 0)

    /* Element enable flags (+0x1D4..+0x1D9) — byte per element; named by
     * element role, not by row:
     *   [0] +0x1D4 prev button (element 5), [1] +0x1D5 next button (element 6),
     *   [2] +0x1D6 delete button (element 2), [3] +0x1D7 rotate button (element 4),
     *   [4] +0x1D8 page-up scroll,          [5] +0x1D9 page-down scroll */
    uint8_t    row_enabled[6];          // +0x1D4..+0x1D9

    /* Tile name buffers (6 x 0x14 = 0x78 bytes starting at +0x1DA) */
    char       tile_names[6][20];       // +0x1DA  tile name text for each row (null-terminated)

    /* Total x86 size: 0x254 bytes (end = +0x1DA + 0x78 = +0x252 + 2 pad) */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * PostcardAlbum constructor.
     * (part of the factory PostcardAlbum_CreateFromResource, 0x401F50)
     *
     * Chains to UI_WindowBase(hInstance, resId), then calls
     * InitFromResource to initialize all album fields and sprites.
     *
     * @param hInstance  Application instance handle
     * @param resId      Window resource ID (0x1FB in the game)
     */
    PostcardAlbum(HINSTANCE hInstance, UINT resId);

    /**
     * Factory — construct a PostcardAlbum in pre-allocated memory.
     * Address: 0x401F50 (PostcardAlbum_CreateFromResource, __thiscall)
     *
     * The binary version calls UI_WindowBase_Ctor + InitFromResource on
     * memory supplied by the caller (CGWND_InitAllSubsystems allocates
     * the original x86's 0x254 bytes via operator_new; the host build
     * allocates operator_new(sizeof(PostcardAlbum)) == 0x328 instead, since
     * PostcardAlbum's pointer-bearing base fields widen on a 64-bit host).
     * This is placement-new construction.
     *
     * A null `mem` returns nullptr without constructing anything — this
     * folds the *caller's* `TEST EAX,EAX; JZ` allocation-failure check
     * (0x4072AC, immediately after `operator_new`) into the callee, since
     * every real caller now folds `operator_new(...)` directly into this
     * call's argument (e.g. `PostcardAlbum::CreateFromResource(
     * operator_new(sizeof(PostcardAlbum)), hInst, 0x1FB)` in
     * core/CGWND.cpp's `_WIN32` branch) rather than checking the
     * allocation result before calling. Without this guard, an allocation
     * failure would null-deref inside UI_WindowBase's constructor instead
     * of surfacing as CGWND.cpp's own `g_postcard == nullptr` failure path.
     *
     * @param mem         Pre-allocated memory (operator_new(sizeof(PostcardAlbum))),
     *                    or nullptr on allocation failure
     * @param hInstance   Application instance handle
     * @param resId       Window resource ID
     * @return            Pointer to constructed PostcardAlbum (same as mem), or
     *                    nullptr if mem was nullptr
     */
    static PostcardAlbum* CreateFromResource(void* mem, HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x401FB0 (PostcardAlbum_DestroyFromResource)
     *
     * Calls FreeAllSprites; the compiler-emitted deleting destructor then
     * releases the heap allocation (GLOBAL_free) when delete is used.
     */
    virtual ~PostcardAlbum();

    /**
     * Hide / destroy the album window (vtable[1]).
     * Address: 0x402660 (PostcardAlbum_DestroyWindow, __fastcall)
     *
     * Guard is UI_WindowBase::visible (+0xE4, NOT this class's own
     * active_flag +0x110 — verified via disassembly). If visible: calls
     * UI_WindowBase::hide(), clears text_rendered (+0x112) and calls
     * FreeSprites().
     *
     * Ghidra's auto-generated name ("DestroyWindow") is misleading — the
     * function never calls the real Win32 DestroyWindow API, only
     * UI_WindowBase::hide() (which itself calls ShowWindow(SW_HIDE)).
     */
    void hide() override;

    /**
     * Show the album (vtable[2]).
     * Address: 0x402590 (dispatched by CGWND_SetMode(6) @ 0x408216)
     *
     * Sequence (verified from raw x86 bytes):
     *   InitSprites() -> this->on_create() (vtable[7], 0x4028B0, ported
     *   below) -> UI_WindowBase::show() ->
     *   ShowWindow(hWnd, SW_SHOWMAXIMIZED) -> SetFocus(hWnd) -> delete
     *   selected_postcard_player if set -> set_mode(childCount0, childObj0,
     *   0, 1) -> clear active_flag (+0x110) -> restore scroll position from
     *   g_dplay_config's (PixelDataCache) insert_index/saved_album_index
     *   temp fields and re-render all tiles via RenderAllTiles() -> reset
     *   the cache temp fields to -1.
     */
    void show() override;

    /**
     * on_create — per-resolution absolute sprite-rect layout (vtable[7]).
     * Address: 0x4028B0 (__fastcall, ECX = this)
     *
     * Guarded on sprites_inited (+0x111): a no-op (does not even call the
     * base on_create()) unless sprites have already been initialized —
     * verified against the disassembly (`JZ` immediately past the entire
     * function body when the guard byte is 0). show() calls InitSprites()
     * (which sets sprites_inited) before calling this->on_create(), so the
     * guard is satisfied on every real invocation.
     *
     * SEPARATE PRECONDITION (verified, not merely assumed): this method
     * unconditionally dereferences album_bg_resource (+0x138) to read its
     * frame_width/frame_height. That field is populated by a DIFFERENT
     * guard (InitWindowSurface(), window_surface_inited +0xFC), not by
     * sprites_inited, and show() never calls InitWindowSurface() itself.
     * The guarantee instead comes from game-mode ordering: g_postcard
     * doesn't exist until GameLoop_Setup's CGWND_InitAllSubsystems runs,
     * and the only album-opening UI (CGWND_SetMode(6), reached from
     * PostcardAlbum::show()) lives inside the town view (mode 3) —
     * verified that PostcardAlbum_InitWindowSurface's only two callers
     * (0x404720's xrefs) are both inside CGWND_InitMode1 (0x408350), and
     * traced (through both the demo-mode and normal-mode branches, incl.
     * the `LAB_0040856e` merge) that CGWND_InitMode1 always calls it
     * before the one call path that ends in CGWND_SetMode(3). Which of
     * CGWND_SetMode's ~39 callers pass the literal mode 6 was not
     * individually enumerated; the practical guarantee (no in-game UI to
     * open the album exists before town) does not depend on that
     * enumeration. So album_bg_resource is non-null on every real call to
     * on_create() — an original-game invariant this port relies on rather
     * than re-verifies with a runtime null check (the original doesn't
     * have one either).
     *
     * When the guard passes:
     *   1. Calls UI_WindowBase::on_create() (populates clientRect/workRect
     *      +0xC4/+0xD4 from GetClientRect).
     *   2. Sets layout_rect (+0x100) to {0,0,800,600} (is_high_res==0) or
     *      {0,0,1024,768} (is_high_res==1; any other value is UNREACHABLE —
     *      is_high_res is only ever 0 or 1, set by InitFromResource — and
     *      returns early exactly like the original's `if (!= 1) return`),
     *      then centers layout_rect in place within workRect via
     *      UI_CenterWindow (accounts for a real desktop larger than the
     *      800x600/1024x768 design resolution).
     *   3. Sets bg_blit_rect (+0xEC) to a copy of workRect, then centers it
     *      in place within the loaded album_bg_resource bitmap's own pixel
     *      bounds (RESDATA::frame_width/frame_height) via a second
     *      UI_CenterWindow call.
     *   4. Sets tiles_per_page (+0x11C) to 4 (low-res) or 6 (high-res) —
     *      this is the field show()'s scroll-offset divide reads; resolves
     *      the former divide-by-zero hard blocker (see top banner).
     *   5. Lays out every button/row/label sprite's rect (x/y/sourceX/
     *      sourceY, used elsewhere as left/top/right/bottom — see
     *      BlitElement/HitTest) as a fixed offset from layout_rect: the 8
     *      named button sprites (sized from their own pixel-data frame
     *      dimensions), the scrollwheel sprite plus 9 evenly-sliced
     *      tile_label_sprites beneath it, and the per-resolution row_icon/
     *      row_tile (300x200) / row_name (300x25) arrays — 4 rows in
     *      low-res mode, all 6 in high-res mode (matching tiles_per_page).
     *      Every literal offset below is transcribed directly from the
     *      decompiled/disassembled 0x4028B0 (verified address-for-address,
     *      not re-derived from a design assumption).
     */
    void on_create() override;

    /**
     * Right-click handler (vtable[16], overrides UI_WindowBase::on_rbutton_down).
     * Address: 0x4055E0 (__thiscall, RET 0x10 — confirms the base's real
     * 4-stack-arg on_rbutton_down(HWND,UINT,WPARAM,LPARAM) signature; all
     * four are ignored by the body).
     *
     * If selected_postcard_player (+0x130) is set: deletes it (vtable[0]
     * with the deleting-destructor flag) and calls
     * set_mode(childCount0, childObj0, 0, 1) — the identical call show()
     * makes. Effectively: right-click cancels the current postcard/player
     * selection and refreshes the render mode. No-op otherwise.
     *
     * Ghidra auto-named this function "GameObject_OnTimerTick" with a
     * misleading plate comment ("vtable[0x30] for GameObject... calls
     * GameObject_Update"); both were wrong — its sole DATA xref is this
     * class's own vtable slot 16 (0x477430), and dispatch_message (0x426140)
     * shows slot+0x40 is reached ONLY via WM_RBUTTONDOWN (0x204).
     */
    LRESULT on_rbutton_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    /* ================================================================ */
    /* Initialization                                                    */
    /* ================================================================ */

    /**
     * InitFromResource — Core initialization of all album fields and sprites.
     * Address: 0x401FD0 (__fastcall, ECX = this)
     *
     * Zeroes all album state, detects high-res mode, creates 8 button
     * sprites (res 0x3C04-0x3C0F), 6 row groups (icon/tile/name sprites),
     * 9 tile label sprites, and sets all element enable flags to 1.
     */
    void InitFromResource();

    /**
     * InitWindow — Create the album child window.
     * Address: 0x402520 (__thiscall)
     *
     * Creates a full-desktop child window via this->create_full_window()
     * (vtable[6]), loads icon resource 0x65 into icon_handle (+0xE8) via
     * LoadIconA(hInstance, MAKEINTRESOURCE(0x65)). Returns true on success.
     *
     * VERIFIED SHARED: CGWND_InitAllSubsystems (0x406F90) calls this exact
     * address (0x402520) on BOTH g_postcard (PostcardAlbum*) and
     * g_postcard_send (PostcardPreviewWindow*, allocated 0x2C4 bytes) —
     * confirmed via fresh decompile (two call sites, 0x407159 and 0x40732B).
     * This only works in the original because both classes derive directly
     * from UI_WindowBase and this function only touches base-class
     * hInstance (+0x04) plus each derived class's own first member (an
     * HICON at +0xE8 in both layouts) — genuine original address-level code
     * reuse, not real polymorphism. core/CGWND.cpp's host port already
     * reproduces this via an explicit `(PostcardAlbum*)g_postcard_send`
     * C-style cast at its own call site (documented there), which this
     * pass does not need to touch.
     *
     * @param hParent  Parent window HWND
     * @return         TRUE if window created successfully
     */
    bool InitWindow(HWND hParent);

    /**
     * InitWindowSurface — One-shot initialization of album background surface.
     * Address: 0x404720 (__fastcall, ECX = this)
     *
     * Loads background resource (0x3C0A for low-res, 0x3C0B for high-res),
     * caches at album_bg_resource + album_bg_surface (Lock(0,0)).
     * Guarded by window_surface_inited flag at +0xFC.
     */
    void InitWindowSurface();

    /**
     * InitSprites — Initialize all album sprites via Sprite_Init.
     * Address: 0x404770 (__fastcall, ECX = this)
     *
     * Calls Sprite_Init (ButtonSprite::init) on each of the 8 button
     * sprites and the 6 row TILE sprites (+0x180). Loads photo background
     * resource (0x3CFA) and surface. Sets sprites_inited flag.
     */
    void InitSprites();

    /* ================================================================ */
    /* Cleanup / teardown                                                */
    /* ================================================================ */

    /**
     * FreeSprites — Free album child sprites.
     * Address: 0x404830 (__fastcall, ECX = this)
     *
     * Releases photo background resource (Lock/Unlock pairing), calls
     * Sprite_Destroy (ButtonSprite::destroy) on each of the 8 button
     * sprites and the 6 row tile sprites. Clears sprites_inited flag.
     */
    void FreeSprites();

    /**
     * FreeAllSprites — Full destructor body for PostcardAlbum.
     * Address: 0x402380 (__fastcall, ECX = this)
     *
     * Calls FreeSprites if sprites_inited, releases album_bg_resource,
     * scalar-deletes all 8 button sprites, 6 icon/tile/name sprites,
     * 9 tile label sprites, the +0x130 sprite slot. Base cleanup runs
     * through the C++ destructor chain (UI_WindowBase::~UI_WindowBase).
     *
     * Called from: ~PostcardAlbum
     */
    void FreeAllSprites();

    /* ================================================================ */
    /* Rendering and event handling                                      */
    /* ================================================================ */

    /**
     * PaintWindow — Key handler for the album (vtable[21], 0x477444).
     * Address: 0x402690 (__thiscall)
     *
     * Guards on +0x110 (returns 0 while the album is busy). Handles:
     *   0x0D ENTER / 0x1B ESC: hide() + CGWND_SetMode(3)
     *   0x25 VK_LEFT:  allowed when row_enabled[0] is set; page-up when
     *                  row_enabled[4] set, else scroll left one page
     *   0x27 VK_RIGHT: allowed when row_enabled[1] is set; page-down when
     *                  row_enabled[5] set, else scroll right one page
     * Other messages fall through to DefWindowProcA, RETURNING its result
     * directly (unlike UI_WindowBase's default slots, which always return
     * 0) — a real, verified difference, preserved here. After any scroll
     * the tile grid is re-rendered (RenderAllTiles() + EndPaintEx()).
     *
     * Switches on wParam (the virtual key code), not msg — msg is used only
     * in the default/DefWindowProcA fallthrough.
     */
    /** Binary slot [21] 0x402690 (PaintWindow). */
    LRESULT on_key_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    /**
     * BlitElement — Render a specific album button/sprite element.
     * Address: 0x403E80 (__thiscall)
     *
     * Plays sound (0x5015) and blits the sprite region from the album
     * background surface to the primary surface via BlitToSurface(). Element
     * IDs: 1 = btn_close, 2 = btn_delete, 3 = btn_save, 4 = btn_rotate,
     *   5 = btn_prev, 6 = btn_next, 7 = btn_scrollwheel, 9 = btn_print.
     * Disabled elements (flag cleared) are dimmed via setState(2) and
     * skipped (no sound, no blit). Element 6 plays its sound AFTER the
     * blit (every other element plays it first); element 7 (scrollwheel)
     * skips BlitToSurface entirely and just plays the sound + sets the
     * sprite's frame from scroll_wheel_pos (+0x128).
     *
     * VERIFIED (disassembly, 0x403E80): the function epilogue is `RET 8` —
     * 2 stack dwords popped — but only the first ([ESP+4] at entry,
     * element_id) is ever read; no instruction reads the true entry-time
     * [ESP+8]. Checked against every real call site (on_key_down at 0x4026E6/
     * 0x40270D and PostcardAlbum_OnLButtonDown, not yet ported, at all 12 of
     * its own call sites, e.g. 0x404FB2-B6): every site pushes a literal 0
     * (or an always-zeroed EBP register) immediately before the element ID,
     * so the second stack dword is provably dead on every real caller — the
     * 1-arg `BlitElement(int)` shape here is the verified ABI, not merely
     * inherited from a prior pass's guess.
     */
    void BlitElement(int element_id);

    /**
     * UpdateSprite — Update sprite visual state for element.
     * Address: 0x403BA0 (__thiscall)
     *
     * Sets sprite to state 0 (normal) or 2 (dimmed) based on the
     * corresponding element enable flag:
     *   element 2 -> row_enabled[2], 4 -> row_enabled[3],
     *   5 -> row_enabled[0], 6 -> row_enabled[1]
     * Elements 1, 3, 9 always go to state 0; element 7 and any other value
     * is a no-op (no sprite carries state for the scrollwheel/rows here).
     *
     * VERIFIED (disassembly, 0x403BA0): same `RET 8`-with-one-real-argument
     * shape as BlitElement() above, verified the same way (every real call
     * site pushes a literal 0 as the dead second stack dword) — see
     * BlitElement()'s doc comment for the full evidence trail.
     */
    void UpdateSprite(int element_id);

    /**
     * RenderTileName — Render a single tile name onto the album surface.
     * Address: 0x4048E0 (__thiscall)
     *
     * Looks up tile data via PixelDataCache::LookupAsset using scroll
     * offset + index. If data found, calls NetworkPlayerList::RenderPlayer
     * to render the player/tile preview into the row's icon sprite area
     * (row_icon[row_index]), copies the player's name into
     * tile_names[row_index], deletes the resolved DPlayManager* entry, and
     * sets row_tile[row_index]'s sprite state to 0. If data not found,
     * clears the row's name buffer and blits row_icon[row_index]'s
     * background area from the album background surface via
     * BlitToSurface().
     *
     * VERIFIED (disassembly, 0x4048E0): the decompiler's CONCAT31(...)
     * around the `highlighted` argument is exactly the kind of register-
     * fusion artifact that produced the earlier +0x39 misattribution
     * documented in DPlayManager.h — so this call's exact argument mapping
     * was re-derived from the raw push sequence rather than trusted from
     * the decompile. Counting every PUSH between LookupAsset's result and
     * `CALL 0x4437C0` gives exactly 9 stack slots (matching RenderPlayer's
     * real RET 0x24 / 9-argument ABI): in push order (chronological), the
     * final `PUSH EDX` (the +0x12C `scroll_wheel_enabled` byte, with
     * incidental garbage in its upper 24 bits from a prior full-register
     * load — harmless, since `bool` parameters conventionally read only
     * the low byte) is pushed LAST and therefore IS the first parameter
     * (`highlighted`); working backward: entry (`player`), g_primary_surface
     * (`surface`), the 4 in-place-constructed RECT dwords (`left/top/right/
     * bottom`), `this->hWnd`, and a leading literal `PUSH 0x0` is the LAST
     * parameter (`highlightRect = nullptr`). This confirms both
     * `scroll_wheel_enabled` and `this->hWnd` are correctly positioned, not
     * merely assumed from the earlier decompile.
     *
     * RETURN VALUE (verified via disassembly, not decompile alone): the
     * original DOES return a deterministic AL on both paths -- `MOV AL,1`
     * (0x4049A0) on the found/rendered path, `XOR AL,AL` (0x404AAC) on the
     * not-found/cleared path (Ghidra's `(uint)uVar6 << 8` decompile was
     * only flagging bits 8-31 as unknown register residue; bit 0, the real
     * AL return, is written explicitly on both paths). However,
     * get_xrefs_to confirms RenderTileName has exactly ONE caller,
     * RenderAllTiles (0x404AE8 — `PostcardAlbum_RenderTileName(param_1,
     * uVar5);` with the result discarded), so this 1-rendered/0-cleared
     * value is real but dead at its only call site. Modeled as `void`
     * here rather than reintroducing an unused return.
     *
     * @param row_index  Tile row index (0..5)
     */
    void RenderTileName(int row_index);

    /**
     * RenderAllTiles — Render all visible tile names in the album.
     * Address: 0x404AC0 (__fastcall, ECX = this)
     *
     * Phase 1: for each of the first tiles_per_page rows, calls
     * RenderTileName(i) then blits row_name[i]'s background area via
     * BlitToSurface(). Phase 2: opens a GDI paint session via
     * UI_WindowBase::BeginPaint(), then for each of the first
     * tiles_per_page rows, checks scroll_wheel_enabled (+0x12C) — per
     * iteration, matching the original's placement, even though the flag
     * cannot change mid-loop — and if 1, draws that row's tile_names[]
     * string via DrawTextA with DT_SINGLELINE|DT_VCENTER|DT_CENTER (0x25)
     * into row_name[i]'s rect using g_font_small. Finally releases the DC
     * via EndPaintEx(hdc, true, nullptr) with unlockOnly=true — no present,
     * since the visible blitting already happened via BlitToSurface in
     * phase 1. Phase 3:
     * updates row_enabled[0]/[4] (prev-button/"can page
     * up" flags) from scroll_pixel_offset and tile_index, and
     * row_enabled[1]/[5] (next-button/"can page down" flags) from
     * scroll_pixel_offset + tiles_per_page vs
     * PixelDataCache::GetEntryCount() — calling UpdateSprite(5)/
     * UpdateSprite(6) whenever a flag actually flips.
     *
     * VERIFIED (disassembly, 0x404AC0): both Phase-3 bounds comparisons are
     * unsigned (`JC`/`CMP ECX,EAX` and `CMP EAX,8`/`JC`, not `JG`/`JL`) —
     * i.e. `tile_index` is compared as `(uint32_t)tile_index > 7`, not a
     * signed `> 7`. This matters because on_key_down's VK_LEFT path can
     * decrement scroll_wheel_pos (and therefore tile_index) below zero with
     * no clamp; a negative tile_index reads as a huge unsigned value here
     * and takes the "already scrolled past the end" branch rather than the
     * signed-negative branch a naive `int > 7` port would take.
     */
    void RenderAllTiles();

    /**
     * BlitToSurface — Apply the base-window/background-image scroll offsets
     * to a sprite's rect and blit that region from the album background
     * surface to the primary surface.
     *
     * NOT its own original address — this is a repeated inline pattern
     * (read sprite rect, guard on sprites_inited+text_rendered, copy the
     * rect twice, OffsetRect each copy by workRect.left/top (src) and
     * bg_blit_rect.left/top (dst), UIPANEL_Blit, log on failure) that
     * appears verbatim inside BlitElement (0x403E80), RenderTileName
     * (0x4048E0), and RenderAllTiles (0x404AC0) in the original binary —
     * factored out here as a real member function rather than transcribed
     * three times. Guarded on sprites_inited (+0x111) and text_rendered
     * (+0x112): returns true without blitting if either is unset (matches
     * every one of the three original call sites' identical guard).
     *
     * @param sprite  Sprite whose rect (x/y/sourceX/sourceY) is blitted.
     * @return        true on a successful blit (or on the early
     *                not-ready-to-paint guard); false if UIPANEL_Blit
     *                itself reports failure (logged via OutputDebugStringA).
     */
    bool BlitToSurface(ButtonSprite* sprite);

    /**
     * HitTest — Hit-test album sprites for click dispatch.
     * Address: 0x403CD0 (__thiscall, NON-virtual helper)
     *
     * Tests click point (x, y) against each sprite rect. Returns:
     *   1  = btn_close (+0x148), 9 = btn_print (+0x158),
     *   4  = btn_rotate (+0x154), 2 = btn_delete (+0x14C),
     *   3  = btn_save (+0x150), 5 = btn_prev (+0x15C),
     *   6  = btn_next (+0x160),
     *   8  = row icon sprite (+0x168..+0x17C) -> hovered_tile = index,
     *   10 = row name sprite (+0x198..+0x1AC) -> hovered_tile = index,
     *   7  = tile label sprite (+0x1B0..+0x1D4) -> hovered_tile = index,
     *   0  = no hit
     *
     * @param x  Screen X coordinate
     * @param y  Screen Y coordinate
     * @return   Hit result: 0 = none, 1..10 = sprite ID
     */
    int HitTest(int x, int y);
};
