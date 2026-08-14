// Status: INTEGRATED
/**
 * Cursor.h — Mouse cursor / UI overlay manager class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The Cursor class manages the entire mouse cursor rendering pipeline:
 * sprite animation, DirectDraw surface blitting, mouse capture/release,
 * dirty-rect tracking, and viewport clipping. It also hosts the editor
 * toolbar as a sub-window with its own sprite sheet, color palette,
 * and network-player status overlay.
 *
 * Inherits from UI_WindowBase (vtable: 0x477C30), which provides the
 * base window lifecycle (Create/Show/Hide/Destroy) and a shared cursor
 * backbuffer reference-counting mechanism.
 *
 * Size: 0x740 bytes (1856 bytes)
 * Allocated via: operator_new(0x740)
 * Vtable: 0x477930 (set during Cursor::Cursor())
 *
 * NOTE: Cursor overlays UI_WindowBase fields in +0x00..+0xE7 with
 * Cursor-specific reinterpretations. Field access uses inline accessor
 * methods that return references to the base class storage cast to
 * the Cursor-specific type. For example, cursor_state() overlays
 * field_14, primary_surface() overlays lastCursorY (base offset +0x38), etc.
 *
 * Vtable layout (verified against the raw PE bytes at 0x477930; the
 * Cursor vtable ends at slot [37] with a NULL terminator at 0x4779C4 —
 * everything at +0x98 and beyond in older notes was actually the
 * separate InputMgr vtable at 0x4779C8, not Cursor slots):
 *   [0]  +0x00: scalar deleting destructor (Cursor_Dtor, 0x4159E0)
 *   [1]  +0x04: Hide (Cursor_Hide, 0x416F70)
 *   [2]  +0x08: Show (UI_WindowBase_Show, 0x4259C0, inherited)
 *   [3]  +0x0C: SetMode (UI_WindowBase_SetMode, 0x425FD0, inherited —
 *               NOT the GameWindow-family 0x414340!)
 *   [4]  +0x10: SetRenderSurface (UI_WindowBase_SetRenderSurface, 0x426020, inherited)
 *   [5]  +0x14: OnAsyncTaskFailure (UI_WindowBase_OnAsyncTaskFailure, 0x426130, inherited)
 *   [6]  +0x18: CreateFullWindow (UI_CreateFullWindow, 0x425B70, inherited)
 *   [7]  +0x1C: on_show / on_create hook (0x417180 — unlabeled region;
 *               sole DATA ref is vtable slot [7]; code at 0x417186 calls
 *               UI_WindowBase::on_create 0x425D30)
 *   [8]  +0x20: render_editor (Cursor_RenderEditor, 0x418210)
 *   [9]  +0x24: no-op thunk (0x4661A0, inherited default)
 *   [10] +0x28: message dispatcher (FUN_00426140, inherited; routes
 *               WM_* to slots [13]..[36])
 *   [11] +0x2C: WindowProc (Cursor_ToolbarWndProc, 0x419A60)
 *   [12] +0x30: 0x41A8A0   [13] +0x34: 0x422EA0 (DefWndProc)
 *   [14] +0x38: 0x41AC10   [15] +0x3C: 0x41AA40 (Cursor_CancelColorAdjust)
 *   [16] +0x40: 0x41CA80   [17] +0x44: 0x41AAE0 (Cursor_ConfirmColorAdjust)
 *   [18] +0x48: 0x41AB70   [19] +0x4C: 0x422EA0 (DefWndProc)
 *   [20] +0x50: 0x41CE50   [21] +0x54: 0x417040
 *   [22] +0x58: 0x422EA0   [23] +0x5C: 0x426950
 *   [24] +0x60: 0x41CDF0   [25] +0x64: 0x422EA0
 *   [26] +0x68: 0x426960   [27] +0x6C: 0x426980
 *   [28] +0x70: 0x426A60   [29] +0x74: 0x422EA0
 *   [30] +0x78: 0x426AC0   [31] +0x7C: 0x426AD0
 *   [32] +0x80: 0x419A10   [33] +0x84: 0x422EA0
 *   [34] +0x88: 0x422EA0   [35] +0x8C: 0x422EA0
 *   [36] +0x90: 0x422EA0   [37] +0x94: 0x00000000 (NULL — vtable end)
 *
 * The dwords at +0x98..+0xC0 are NOT Cursor slots: 0x4779C8 is the
 * separate InputMgr vtable (slot[0]=0x41D2B0 dtor, [1]=0x41DD80
 * INPUT_PlaceObject, [2]=0x41DEF0 INPUT_RemoveObject, [3]=0x41E100
 * InputMgr::ResetWorldState; slots [4]/[5] are float data), and 0x4779F4
 * begins the 0x4A99B0 event-list object's vtable ([0]=0x41F4B0 scalar
 * dtor, [1]=0x4203A0, [2]=0x420860).  See input/InputMgr.h.
 *
 * The C++ virtual set below covers the slots the reconstructed code
 * actually dispatches through (dtor [0], hide [1], render_editor [8],
 * on_show [7]); the rest of the binary vtable is message/input
 * handlers documented above. UI_WindowBase's own C++ declaration order
 * (show before hide) differs from the binary vtable order; this
 * pre-existing base-class deviation is shared by EditWindow and is
 * accepted for the native host build (see AGENTS.md "Host deviations").
 *
 * Class hierarchy:
 *   UI_WindowBase
 *     └─ Cursor  ← this class
 */

#pragma once

#include <cstdint>
#include "../shared/types.h"
#include "../ui/UI_WindowBase.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */
class ButtonSprite;   /* ui/ButtonSprite.h — UI button sprite, 0x24 bytes */
class UIPANEL;          /* ui/UIPANEL.h — UI panel surface, 0x20 bytes */
class DPlayManager;   /* network/DPlayManager.h — real type of obj_184, see below */

/* CursorEditorRecord (a partial 0x94-byte struct duplicating a subset of
 * DPlayManager's real layout) removed 2026-08-14: `obj_184` is a real
 * DPlayManager* (the 0x39C-byte player slot, network/DPlayManager.h),
 * confirmed by 8 independent byte-for-byte field-offset matches from
 * +0x10 through +0x93 with zero counter-evidence. init_network_player()
 * (input/Cursor_impls.cpp) constructs one via operator_new(sizeof(DPlayManager))
 * + placement-new + CreatePlayer(), exactly like every other real
 * DPlayManager::CreatePlayer() caller. See DPlayManager.h's own class-level
 * comment for why several of its fields (m_wordValue, m_dwordValue,
 * m_unknown93, m_sessionBlk1's tail) are reused by this class's editor UI
 * for local, not-yet-networked state instead of being given Cursor-specific
 * names in the canonical class. */

/* ================================================================== */
/* Cursor class                                                        */
/* ================================================================== */

/* These members overlay storage inherited from UI_WindowBase at the
 * documented x86 offsets.  may_alias keeps the representation-preserving
 * accessors explicit without making GCC assume unrelated field types cannot
 * occupy the same bytes. */
template <typename T>
struct CursorOverlayValue {
    T value;
} __attribute__((__may_alias__));

class Cursor : public UI_WindowBase {
public:
    using UI_WindowBase::show;

    /* ================================================================ */
    /* Base class field accessors                                        */
    /*                                                                   */
    /* Cursor overlays many UI_WindowBase fields with Cursor-specific    */
    /* interpretations. These inline accessors provide typed references  */
    /* to the base class storage, preserving the binary-matching layout.  */
    /* ================================================================ */

    /* +0x0C: parent window HWND (base hWndParent) */

    /* +0x10: resource_id aliases resourceId */
    UINT&       resource_id()    { return this->resourceId; }

    /* +0x14: cursor_state / cursor_sprite_surface aliases field_14 */
    int32_t&    cursor_state()   { return this->field_14; }
    void*&      cursor_sprite_surface() { return reinterpret_cast<CursorOverlayValue<void*>*>(&this->field_14)->value; }

    /* +0x18..+0x24: viewport clip rectangle (four base int32 fields) */
    RECT*       clip_rect()      { return reinterpret_cast<RECT*>(&this->field_18); }
    const RECT* clip_rect() const { return reinterpret_cast<const RECT*>(&this->field_18); }
    int32_t&    clip_rect_left()   { return this->field_18; }
    int32_t&    clip_rect_top()    { return this->field_1C; }
    int32_t&    clip_rect_right()  { return this->field_20; }
    int32_t&    clip_rect_bottom() { return this->field_24; }

    /* +0x28: timer_id aliases timerId */
    UINT_PTR&   timer_id()       { return this->timerId; }

    /* +0x38: primary_surface aliases UI_WindowBase::lastCursorY (was field_38;
     * renamed in the base class once dispatch_message's WM_TIMER (0x113) fast
     * path proved that offset's meaning for the base UI_WindowBase codepath —
     * see ui/UI_WindowBase.h/.cpp. Cursor overlays the same storage with its
     * own interpretation, as it does throughout this class.) */
    void*&      primary_surface() { return reinterpret_cast<CursorOverlayValue<void*>*>(&this->lastCursorY)->value; }

    /* +0x3C: sprite_width (uint32_t) overlays captureFlag + _pad_3E */
    uint32_t&   sprite_width()   { return reinterpret_cast<CursorOverlayValue<uint32_t>*>(&this->captureFlag)->value; }

    /* +0x3D: high byte of the sprite_width dword (+0x3C). Alias of base
     * field_3D. Zeroed by upload_custom_content (0x419B10) together with
     * sprite_height when clearing the sprite dimensions. */
    uint8_t&    sprite_width_hi() { return this->field_3D; }

    /* +0x40: sprite_height aliases field_40 */
    int32_t&    sprite_height()  { return this->field_40; }

    /* +0x44: anim_resdata (RESDATA*) overlays activeFlag + _pad_45 (4 bytes) */
    RESDATA*&   anim_resdata()   { return reinterpret_cast<CursorOverlayValue<RESDATA*>*>(&this->activeFlag)->value; }

    /* +0x48: anim_frame aliases cursorRefCount */
    int32_t&    anim_frame()     { return this->cursorRefCount; }

    /* +0x50: dirty_rect_left aliases field_50 */
    int32_t&    dirty_rect_left()  { return this->field_50; }

    /* +0x54: dirty_rect_top aliases field_54 */
    int32_t&    dirty_rect_top()   { return this->field_54; }

    /* +0x58: capture_flag (uint8_t) overlays field_58 */
    uint8_t&    capture_flag()   { return reinterpret_cast<CursorOverlayValue<uint8_t>*>(&this->field_58)->value; }

    /* +0x5C: backbuffer aliases field_5C */
    void*&      backbuffer()     { return reinterpret_cast<CursorOverlayValue<void*>*>(&this->field_5C)->value; }

    /* +0x60: child_obj_60 aliases childCount0 */
    void*&      child_obj_60()   { return reinterpret_cast<CursorOverlayValue<void*>*>(&this->childCount0)->value; }

    /* +0x64: curs_pos_x aliases childObj0 */
    int32_t&    curs_pos_x()     { return reinterpret_cast<CursorOverlayValue<int32_t>*>(&this->childObj0)->value; }

    /* +0x68: cursor_rect (RECT) overlays childCount1..childObj2 */
    RECT&       cursor_rect()    { return reinterpret_cast<CursorOverlayValue<RECT>*>(&this->childCount1)->value; }
    const RECT& cursor_rect() const {
        return reinterpret_cast<const CursorOverlayValue<RECT>*>(&this->childCount1)->value;
    }

    /* +0x78: prev_cursor_rect (RECT) overlays title[50] */
    RECT&       prev_cursor_rect() { return reinterpret_cast<CursorOverlayValue<RECT>*>(this->title)->value; }

    /* +0x88: viewport_render_enabled (inside title buffer at +0x10) */
    uint8_t&    viewport_render_enabled() { return reinterpret_cast<CursorOverlayValue<uint8_t>*>(this->title + 0x10)->value; }

    /* +0x90: primary_surface_fmt (inside title buffer at +0x18) */
    int32_t&    primary_surface_fmt()  { return reinterpret_cast<CursorOverlayValue<int32_t>*>(this->title + 0x18)->value; }
    /* +0x94: primary_surface_obj (inside title buffer at +0x1C) */
    void*&      primary_surface_obj()  { return reinterpret_cast<CursorOverlayValue<void*>*>(this->title + 0x1C)->value; }
    /* +0x98: primary_resdata (RESDATA*, inside title buffer at +0x20) */
    RESDATA*&   primary_resdata()      { return reinterpret_cast<CursorOverlayValue<RESDATA*>*>(this->title + 0x20)->value; }
    /* +0x9C: overlay_surface_fmt (inside title buffer at +0x24) */
    int32_t&    overlay_surface_fmt()  { return reinterpret_cast<CursorOverlayValue<int32_t>*>(this->title + 0x24)->value; }
    /* +0xA0: overlay_surface_obj (inside title buffer at +0x28) */
    void*&      overlay_surface_obj()  { return reinterpret_cast<CursorOverlayValue<void*>*>(this->title + 0x28)->value; }
    /* +0xA4: overlay_resdata (RESDATA*, inside title buffer at +0x2C) */
    RESDATA*&   overlay_resdata()      { return reinterpret_cast<CursorOverlayValue<RESDATA*>*>(this->title + 0x2C)->value; }

    /* +0xDB: wndproc_flag (inside workRect at +0x7) */
    uint8_t&    wndproc_flag()   { return reinterpret_cast<CursorOverlayValue<uint8_t>*>(reinterpret_cast<uint8_t*>(&this->workRect) + 7)->value; }

    /* +0xE4: cached_width (int32_t) overlays visible (uint8_t) + _pad_E5 */
    int32_t&    cached_width()   { return reinterpret_cast<CursorOverlayValue<int32_t>*>(&this->visible)->value; }

    /* +0xE8: cached_height — first field beyond base class (0xE8 bytes) */
    int32_t     cached_height;          // +0xE8  client height cache
    /* +0xEC: cached_client_width / editor_state */
    union {
        int32_t cached_client_width;   // +0xEC  client area width cache
        int32_t editor_state;          // +0xEC  (aliased: editor mode state)
    };

    /* +0xF0: cached_client_height / delayed_focus_flag */
    union {
        int32_t cached_client_height;  // +0xF0  client area height cache
        int32_t delayed_focus_flag;    // +0xF0  (aliased: delayed focus flag)
    };

    /* +0xF4: window_rect overlaps hEditWnd */
    union {
        RECT   window_rect;            // +0xF4  window client rectangle
        HWND   hEditWnd;               // +0xF4  edit control HWND (overlaps window_rect.left)
    };

    /* +0x104: client_rect overlaps base clientRect fields.
     * The binary stores a second copy of the client rect at +0x104.
     * This is separate from base::clientRect at +0xC4. */
    RECT       cursor_client_rect;     // +0x104 client area rect copy

    int32_t    field_114;              // +0x114  (unknown — no evidence of use)
    int32_t    field_118;              // +0x118  (unknown — no evidence of use)
    uint8_t    _pad_11C[12];           // +0x11C  undocumented gap (verified by binary offset map)

    /* --- Editor scroll/list fields (+0x128..+0x184) --- */
    RECT       scroll_bg_rect;         // +0x128  scrollable list background rect
    uint8_t    _pad_138[16];           // +0x138  undocumented gap (verified by binary offset map)
    ButtonSprite*  sprite_148;         // +0x148 (compat)
    ButtonSprite*  sprite_14C;         // +0x14C (compat)
    RECT       scroll_border_rect;     // +0x150  scrollable list border rect
    RECT       scroll_header_rect;     // +0x160  scrollable list header text rect
    int32_t    scroll_top_idx;         // +0x170  first visible player index
    int32_t    scroll_bottom_idx;      // +0x174  last visible player index
    int32_t    scroll_line_height;     // +0x178  player name line height (pixels)
    int32_t    scroll_visible_count;   // +0x17C  number of visible lines
    int32_t    scroll_end_flag;        // +0x180  byte: 1 = end-of-list reached

    /* +0x184: union — int32_t and DPlayManager* share the same storage */
    union {
        int32_t         field_184;  // +0x184  integer alias
        DPlayManager*   obj_184;    // +0x184  real DPlayManager player slot, see above
    };
    uint8_t    ui_active;              // +0x188  byte: master UI-active flag (init 1).
                                       //         When 0 the status/scroll sprites render
                                       //         disabled/hidden (draw_color_palette,
                                       //         draw_network_status); also forms the low
                                       //         byte of the HDC handle in blit_edit_preview.
    uint8_t    _pad_189[3];            // +0x189  padding
    uint32_t   timer_id_18C;           // +0x18C  timer ID for periodic update
    int32_t    field_190;              // +0x190  (unknown — no evidence of use)
    uint8_t    field_194;              // +0x194  byte flag (init 0 — no evidence of use)
    uint8_t    _pad_195[3];            // +0x195  padding
    uint32_t   timer_id_198;           // +0x198  timer ID for scroll/network update
    uint32_t   timer_id_19C;           // +0x19C  second timer ID

    RECT       edit_preview_rect;      // +0x1A0  {x, y, w, h} destination rect for edit preview blit

    /* +0x1B0..+0x1BF: palette region rect — the binary reads these as a
     * 4-dword (x, y, width-bound, height-bound) tuple in draw_color_palette
     * (0x418A90), draw_locomotive_preview (0x418E20), draw_postcard_preview
     * (0x419260) and show() (0x416B80, surface sizing). */
    RECT       palette_rect;           // +0x1B0  palette/toolbar region {left, top, right, bottom}
    /* Union: +0x1C0..+0x1E7 — per Ghidra, holds sprites at low offsets
     * and editor_clip_rect overlaps the full 16-byte region */
    union {
        struct {
            ButtonSprite* sprite_1C0;     // +0x1C0  (confirmed by Ghidra @ 0x417F20)
            ButtonSprite* sprite_1C4;     // +0x1C4
            int32_t field_1C8;        // +0x1C8
            int32_t field_1CC;        // +0x1CC
        };
        RECT    editor_clip_rect;     // +0x1C0..+0x1CF — 16-byte overlay
    };

    /* --- Editor clip/dest rect for full blit (+0x1D8..+0x1E8) --- */
    int32_t    editor_blit_x;          // +0x1D8  source x offset for editor background blit
    int32_t    editor_blit_y;          // +0x1DC  source y offset for editor background blit
    int32_t    editor_blit_w;          // +0x1E0  source width for editor background blit
    int32_t    editor_blit_h;          // +0x1E4  source height for editor background blit

    UIPANEL*   background_surface;     // +0x1E8  background panel surface
    void*      editor_surface;         // +0x1EC  locked editor sprite-sheet surface (IDirectDrawSurface*)
    RESDATA*   editor_resdata;         // +0x1F0  RESDATA* for editor sprite-sheet (resource 0x3CB9)

    ButtonSprite*  editor_sprites[10];     // +0x1F4  editor palette sprite array (10 entries)

    uint8_t    edit_colors[30];        // +0x22C  editor colour table (10 rows x 3 bytes RGB)
    uint8_t    _pad_24A[2];            // +0x24A  padding before counter_24C at +0x24C

    int32_t    counter_24C;            // +0x24C  integer counter (init to 0, used for color-adjust timer)

    int32_t    color_adjust_component; // +0x250  color component index (0=R, 1=G, 2=B) for adjust
    uint8_t    color_adjust_direction; // +0x254  byte: color adjust direction (0=dec, non-zero=inc)
    uint8_t    _pad_255[3];            // +0x255  padding
    RECT       color_bar_rects[3];     // +0x258  three RECTs for R/G/B color bars (16 bytes each)

    int32_t    field_288;              // +0x288
    int32_t    field_28C;              // +0x28C
    int32_t    field_290;              // +0x290
    int32_t    field_294;              // +0x294

    int32_t    color_r;                // +0x298  red component (0-255)
    int32_t    color_g;                // +0x29C  green component (0-255)
    int32_t    color_b;                // +0x2A0  blue component (0-255)

    ButtonSprite*  sprite_2A4;             // +0x2A4  red color bar button (res 0x3CBF)
    ButtonSprite*  sprite_2A8;             // +0x2A8  green color bar button (res 0x3CC0)
    ButtonSprite*  sprite_2AC;             // +0x2AC  blue color bar button (res 0x3CC1)

    uint8_t    editor_flags[4];        // +0x2B0  byte flags [0]=tab_visible, [1]=active_tab,
                                       //         [2]=scroll_dir, [3]=? (init: 1,1,0,0)

    uint8_t    has_next_page;          // +0x2B4  byte: 1 = more palette/postcard items follow
    uint8_t    has_prev_page;          // +0x2B5  byte: 1 = previous palette/postcard items exist

    int32_t    palette_end_idx;        // +0x2B8  last displayed palette item index
    int32_t    palette_start_idx;      // +0x2BC  first displayed palette item index (init -1)

    uint8_t    editor_initialized;     // +0x2C0  byte flag: 1 = editor sprites loaded
    uint8_t    _pad_2C1[3];            // +0x2C1  padding

    ButtonSprite*  sprite_2C4;             // +0x2C4  (resource 0x3C8C)
    ButtonSprite*  sprite_2C8;             // +0x2C8  (resource 0x3C8E)
    ButtonSprite*  sprite_2CC;             // +0x2CC  (resource 0x3CC3)
    int32_t    field_2D0;              // +0x2D0
    int32_t    field_2D4;              // +0x2D4
    int32_t    field_2D8;              // +0x2D8
    int32_t    field_2DC;              // +0x2DC

    ButtonSprite*  sprite_2E0;             // +0x2E0  (resource 0x3C8F)
    ButtonSprite*  sprite_2E4;             // +0x2E4  (resource 0x3C90)
    ButtonSprite*  sprite_2E8;             // +0x2E8  (resource 0x3CAC)
    ButtonSprite*  sprite_2EC;             // +0x2EC  (resource 0x3CBC)
    ButtonSprite*  sprite_2F0;             // +0x2F0  (resource 0x3C92)
    ButtonSprite*  sprite_2F4;             // +0x2F4  (resource 0x3C93)
    int32_t    field_2F8;              // +0x2F8
    int32_t    field_2FC;              // +0x2FC
    int32_t    field_300;              // +0x300
    int32_t    field_304;              // +0x304

    ButtonSprite*  sprite_308;             // +0x308  tab 1 (resource 0x3C94)
    ButtonSprite*  sprite_30C;             // +0x30C  tab 2 (resource 0x3C95)
    ButtonSprite*  sprite_310;             // +0x310  tab 3 (resource 0x3C96)
    ButtonSprite*  sprite_314;             // +0x314  tab 4 (resource 0x3C97)
    ButtonSprite*  sprite_318;             // +0x318  tab 5 (resource 0x3C98)
    ButtonSprite*  sprite_31C;             // +0x31C  tab 6 (resource 0x3C99)
    int32_t    field_320;              // +0x320
    int32_t    field_324;              // +0x324
    int32_t    field_328;              // +0x328
    int32_t    field_32C;              // +0x32C

    ButtonSprite*  bonus_sprites[16];      // +0x330  bonus sprite array (16 entries, res 0x3C9A..0x3CA9)
    uint8_t    bonus_ids[12];          // +0x370  random bonus ID table (12 bytes, range 1..1057)

    ButtonSprite*  sprite_37C;             // +0x37C  palette background (resource 0x3CAB)

    HBRUSH     hBrush;                 // +0x380  GDI brush (RGB 0xE8E8E8 light grey)

    int32_t    selected_idx_384;       // +0x384  third selected index (init -1)

    uint8_t    field_388;              // +0x388  byte flag (init 0 — no evidence of use)

    RECT       palette_item_rects[16]; // +0x38C  cached palette item on-screen positions
                                       //         (16 RECTs, 0x100 bytes)

    ButtonSprite*  toolbar_sprites[64];    // +0x48C  toolbar icon sprite array (64 entries)
    int32_t    surface_toggle;         // +0x58C  dword toggle (0/1) selecting which editor
                                       //         offscreen surface is the draw target;
                                       //         inverted each draw_locomotive_preview call

    void*      editor_surf_a;          // +0x590  editor offscreen surface A (COM Release via vtable[2])
    uint8_t    surf_a_dirty;           // +0x594  byte (init 0 — dirty flag for surf A)

    void*      editor_surf_b;          // +0x598  editor offscreen surface B (COM Release via vtable[2])
    uint8_t    surf_b_dirty;           // +0x59C  byte (init 0 — dirty flag for surf B)
    uint8_t    bonus_mode;             // +0x59D  byte (init 0 — bonus prize mode flag;
                                       //         selects the random bonus_ids table)

    /* Network player names (26 entries, 13 bytes each, null-terminated) */
    char       player_names[26][13];   // +0x59E  network player names buffer (338 bytes)
                                       //  filled by update_network_names() from
                                       //  g_netman (scenario player entries) and
                                       //  _g_dplay (+0xB13, stride 0xD)

    int32_t    toolbar_sentinel;       // +0x6F0  always -1 (sentinel marker,
                                       //         never written again after init)
    int32_t    player_count;           // +0x6F4  current player name count (init 999,
                                       //         overwritten by update_network_names)
    int32_t    toolbar_res_ids[17];    // +0x6F8  toolbar resource ID table (17 entries,
                                       //         0x526C..0x5289, skipping 0x5271-0x527D gap)

    uintptr_t  prev_wndproc;           // +0x73C  saved original WindowProc for subclassed
                                       //         edit control (SetWindowLongA GWL_WNDPROC).
                                       //         init to 0; populated by create() @ 0x4169E0.

    /* Total size: 0x740 bytes */

    /* ================================================================ */
    /* Inline accessors for cursor_rect / prev_cursor_rect fields       */
    /* ================================================================ */
    /* Replaces preprocessor macros that would pollute global namespace. */
    LONG cursor_rect_left()   const { return cursor_rect().left; }
    LONG cursor_rect_top()    const { return cursor_rect().top; }
    LONG cursor_rect_right()  const { return cursor_rect().right; }
    LONG cursor_rect_bottom() const { return cursor_rect().bottom; }

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * Cursor constructor.
     * Address: 0x415980
     *
     * Chains to UI_WindowBase_Ctor with resource ID 0x1FA (506),
     * sets vtable to VTBL_CURSOR (0x477930), then calls Cursor::init()
     * to bulk-create all sprite objects and initialize fields.
     *
     * Called by: CGWND::InitAllSubsystems @ 0x4073C2 (via
     *            g_cursor = new (0x740) Cursor(hInstance, 0x1FA))
     *
     * @param hInstance     HINSTANCE - application instance handle
     * @param resId         UINT - window resource ID (always 0x1FA)
     */
    Cursor(HINSTANCE hInstance, uint32_t resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x4159E0
     *
     * Calls base_destructor() to release all sprite objects, surfaces,
     * and the GDI brush, then conditionally frees the allocation via
     * GLOBAL_free if `flags & 1`.
     */
    virtual ~Cursor();

    /* ================================================================ */
    /* Core lifecycle methods                                            */
    /* ================================================================ */

    /**
     * Base destructor body.
     * Address: 0x4166B0
     *
     * Releases every UISprite, editor surface, GDI brush, and child
     * object owned by the Cursor, then chains to UI_WindowBase::base_destructor
     * which decrements the shared backbuffer refcount and cleans up
     * base class children.
     *
     * Called by: Cursor::~Cursor() @ 0x4159E3
     */
    void base_destructor();

    /**
     * Full initialization (called once from constructor).
     * Address: 0x415A00
     *
     * Phases:
     *   1. Field initialization (flags, timers, brush)
     *   2. UISprite object creation for all editor/UI sprites
     *   3. Edit_colour.dat palette loading
     *   4. Random bonus ID generation
     *   5. Toolbar resource ID table setup
     */
    void init();

    /* ================================================================ */
    /* Window lifecycle methods                                          */
    /* ================================================================ */

    /**
     * Create the cursor window and edit control.
     * Address: 0x4169E0
     *
     * Creates the full-screen overlay window (via UI_CreateFullWindow) and
     * a child edit control for toolbar text input. The edit control is
     * subclassed with a custom WindowProc (0x416B00).
     *
     * Called by: CGWND::InitAllSubsystems @ 0x407444
     *
     * NOTE: Not a virtual method in the binary — 0x4169E0 has no vtable
     * DATA reference (all xrefs are direct calls); the Cursor vtable slot
     * [6] is the inherited UI_WindowBase::create_full_window (0x425B70).
     *
     * @param hParent  HWND - parent window handle
     * @return         int32_t - 1 on success, 0 on failure
     */
    int32_t create(HWND hParent);

    /**
     * Initialize cursor sprites from resource registry.
     * Address: 0x414130
     *
     * Loads resources 0x1400 (primary cursor sprite) and 0x1403 (overlay
     * sprite) from g_resmgr. Creates shared 256x256 offscreen surface
     * (g_cursor_back at 0x4FD3CC) on first call. Stores surface pointers,
     * RESDATA pointers, pixel formats, and sprite dimensions in Cursor
     * fields. Increments the global cursor backbuffer reference count.
     *
     * Called by: GameWindow::create @ 0x413F71
     */
    void init_sprites();

    /**
     * Initialize shared background surface.
     * Address: 0x416460
     *
     * Creates a 1280x1024 (0x500x0x400) UIPANEL background surface stored
     * at +0x1E8 (background_surface), then composites 4 resources (0x3CAA,
     * 0x3CC4, 0x3CC5, 0x3CC6) onto it using Town_BlitElement. Guarded by
     * a once-flag: if background_surface is non-null, the function is a
     * no-op.
     *
     * Called by: CGWND_InitMode1 @ 0x408435, 0x40858A (twice: startup and
     *            world reload)
     */
    void init_background();

    /**
     * Refresh network player names from NetMan + DPLAY sources.
     * Address: 0x416E00
     *
     * Populates player_names[26][13] (+0x59E) with up to 26 player names:
     *   1. If _g_netman->m_gameMode (+0x7C4) == 2: copies scenario player
     *      names from _g_netman->m_slots[9] (+0x518, stride 0x4C), skipping
     *      the local slot (m_mySlotIndex, +0x7D0) and empty dpIds
     *      (slot+0x00); name at slot+0x51D (PlayerSlot::compact_name)
     *   2. Otherwise: uses formatted resource string #0x6E (13 chars max)
     *      as a single default name
     *   3. Calls DPLAY_EnumeratePlayers() then appends names from _g_dplay
     *      (+0xB13, stride 0xD, up to 16 entries)
     *   4. Zero-fills remaining slots up to 26
     *
     * Player count is stored at +0x6F4 (player_count).
     *
     * Called by: Cursor::show() @ 0x416DD5, Cursor::init_network_player() @ 0x41AE3E
     */
    void update_network_names();

    /**
     * Initialize network player data for local/offline mode.
     * Address: 0x41A0E0
     *
     * Creates a local player entry in obj_184 (+0x184) when no network
     * player data is available.
     */
    void init_network_player();

    /**
     * Poll for blit completion on the primary surface.
     * Address: 0x414BB0
     *
     * Unlocks the primary surface, then polls primary_surface->vtable[0x44]
     * (at byte offset 0x44, slot 17) with &this->curs_pos_x (+0x64) as
     * output parameter. Sleeps 10ms between polls, times out after ~10
     * seconds (1000 iterations) with WIN32_FatalError + ExitProcess(1).
     *
     * Returns the HDC value written to this->curs_pos_x (+0x64) by the
     * successful poll.
     *
     * Called by: HelpWnd_UpdateScroll, HelpWnd_MeasureTextHeight,
     *            HelpWnd_UpdateButtonStates, HelpWnd_RenderPage,
     *            HelpWnd_GoNextPage, HelpWnd_HighlightButton,
     *            Train_DrawTextOverlay (9 callers total)
     *
     * @param hWnd  HWND - forwarded to DDRAW_UnlockPrimary
     * @return      void* - the HDC value at this->curs_pos_x
     */
    void* wait_for_blit(HWND hWnd);

    /**
     * Destroy the cursor window (WindowProc callback).
     * Address: 0x414B80
     *
     * WindowProc-style callback with 4 parameters (HWND, UINT, WPARAM,
     * LPARAM). Sets wndproc_flag (+0xDB) to 0, calls DestroyWindow on
     * hWnd (+0x08), and if field_0C (+0x0C) is 0, calls PostQuitMessage(0)
     * to exit the message loop. Returns 0.
     *
     * Called by: vtable dispatch at Cursor vtable slot (address 0x477914),
     *            CGWND at 0x40F794, Train_DrawTextOverlay at 0x437F61
     *
     * @return  int32_t - always 0 (standard WndProc return)
     */
    int32_t destroy_window();

    /* ================================================================ */
    /* Editor / helper methods                                            */
    /* ================================================================ */

    /**
     * Initialize all editor/toolbar sprite objects.
     * Address: 0x417F20
     *
     * Loads the editor sprite sheet (resource 0x3CB9), retrieves its
     * surface via RESDATA vtable[1], and calls Sprite_Init on all ~49
     * UISprite objects used by the toolbar. Sets editor_initialized (+0x2C0).
     *
     * Guarded: if editor_initialized is already set, returns immediately.
     * Pair with cleanup_editor_sprites().
     *
     * Called by: Cursor::create(), Cursor::show()
     */
    void init_editor_sprites();

    /**
     * Destroy all editor/toolbar sprite objects.
     * Address: 0x4180A0
     *
     * Reverses init_editor_sprites(): calls Sprite_Destroy on all ~49
     * UISprite objects, releases the editor sprite-sheet resource via
     * RESDATA vtable[2], and clears editor_initialized (+0x2C0).
     *
     * Guarded: if editor_initialized is 0, returns immediately.
     * Pair with init_editor_sprites().
     *
     * Called by: Cursor::base_destructor(), Cursor::hide(), Cursor::create()
     */
    void cleanup_editor_sprites();

    /**
     * Full editor toolbar render.
     * Address: 0x418210
     *
     * Renders the complete editor toolbar: blits background surface to
     * primary, draws edit preview, color bars, network status, and color
     * palette. Two paths based on palette_start_idx (+0x2BC):
     *
     * palette_start_idx < 0 (tab-switch mode):
     *   Ends paint, resets surface flags, dispatches to
     *   INPUT_SwitchToLocomotiveTab.
     *
     * palette_start_idx >= 0 (normal editor mode):
     *   Draws color palette via draw_color_palette(), resets paint.
     *
     * Post-render: if delayed-focus flag (+0xF0) is set, restores
     * focus to hWnd and optionally plays narration audio.
     *
     * Called by: vtable slot [8] from editor render loop
     */
    /** Binary slot [8] 0x418210 (render_editor). */
    void on_update(int32_t param) override;

    /**
     * Handle click on a preset color swatch.
     * Address: 0x418340
     *
     * Hit-tests the 10 editor_sprites for a click at (x, y). On match:
     * highlights the swatch, reads RGB from edit_colors[] at +0x22C,
     * propagates to color_r/g/b (+0x298/29C/2A0), redraws color bars,
     * and copies color to obj_184 (+0x184) if set.
     *
     * @param x  LONG — click X position
     * @param y  LONG — click Y position
     */
    void handle_color_swatch_click(LONG x, LONG y);

    /**
     * Adjust one R/G/B color component by +/-6.
     * Address: 0x418450
     *
     * Adjusts a single color channel (R=0, G=1, B=2) by +/-6 based on
     * the direction flag. Sets a 100ms timer for auto-repeat. Plays a
     * sound effect (resource 0x5279) on first adjustment. Clamps results
     * to [0, 255]. Redraws color bars and blits the edit preview.
     *
     * @param component  int32_t — color component index (0=R, 1=G, 2=B)
     * @param direction  uint8_t — 0 = decrease, non-zero = increase
     * @param posX       int32_t — X position (forwarded to PlaySoundAt)
     * @param posY       int32_t — Y position (forwarded to PlaySoundAt)
     */
    void adjust_color_component(int32_t component, uint8_t direction, int32_t posX, int32_t posY);

    /**
     * Draw the three R/G/B vertical color bars.
     * Address: 0x418780
     *
     * Draws three vertical color bars in the editor using GDI FillRect.
     * Each bar's filled height is proportional to the channel value (0-255),
     * bottom-aligned within the bar RECT (+0x258/+0x268/+0x278).
     * If reset_buttons is non-zero, resets the +/- button sprites to state 0.
     *
     * @param reset_buttons  uint8_t — if non-zero, reset button sprite states
     */
    void draw_color_bars(uint8_t reset_buttons);

    /**
     * Blit the cursor/edit preview to the primary surface.
     * Address: 0x4189A0
     *
     * Blits the edit preview area (background_surface portion defined by
     * the edit_preview_rect at +0x1A0 and clip rect at +0x1D8) to the
     * primary display surface. If obj_184 (+0x184) has a cursor bitmap,
     * also renders the player cursor overlay via DPLAY_RenderPlayer.
     *
     * Called by: render_editor, handle_color_swatch_click,
     *            adjust_color_component, upload_custom_content, and
     *            ~22 callers across input/toolbar handlers
     */
    void blit_edit_preview();

    /**
     * Draw the color palette swatch strip.
     * Address: 0x418A90
     *
     * Draws the scrollable colour palette strip. Two modes:
     * mode=0: blits background, draws palette items from toolbar_sprites[]
     *         right-to-left with tiered vertical positioning based on
     *         sprite height, caches positions in palette_item_rects[16].
     * mode=1: blits to an alternate surface (clear + draw).
     *
     * Updates scroll button sprite states based on scroll flags.
     *
     * @param target_surf  int* — target surface (NULL = _g_primary_surface)
     * @param mode         uint8_t — 0 = normal draw, non-zero = alternate surface
     */
    void draw_color_palette(int* target_surf, uint8_t mode);

    /**
     * Animate locomotive colour-change preview.
     * Address: 0x418E20
     *
     * Performs a wipe transition showing the new locomotive colour.
     * Double-buffers between editor_surf_a (+0x590) and editor_surf_b (+0x598),
     * alternating each call. Divides the toolbar area into 6 bands and
     * animates left-to-right or right-to-left per frame.
     *
     * @param direction  uint8_t — 0 = left-to-right wipe, non-zero = right-to-left
     */
    void draw_locomotive_preview(uint8_t direction);

    /**
     * Layout and load postcard thumbnail icons.
     * Address: 0x419260
     *
     * Walks the 64-slot toolbar sprite cache (+0x48C) either forward or
     * backward from the current selection to determine which postcard
     * thumbnails are visible. Calls NET_GetOrCreateSurface to load each
     * postcard bitmap. Updates palette_start_idx/end_idx cache.
     *
     * @param direction  uint8_t — 0 = go backward from current selection,
     *                             non-zero = go forward
     * @return           uint8_t — 1 if items were loaded, 0 on failure/empty
     */
    uint8_t draw_postcard_preview(uint8_t direction);

    /**
     * Update network status indicator sprites.
     * Address: 0x419560
     *
     * Resets all network status indicator sprites (sprite_2C4 through
     * sprite_2CC), then conditionally shows status icons based on tab
     * selection, netman scenario state, and the bonus mode flag (+0x59D).
     * Calls handle_tab_change() then iterates 16 bonus_sprites[] for
     * tab-matching highlight states.
     *
     * Called by: Cursor::render_editor(), INPUT_SwitchToLocomotiveTab
     */
    void draw_network_status();

    /**
     * Draw/paint the scrollable locomotive player name list.
     * Address: 0x419680
     *
     * Only active when cached_client_width (+0xEC) equals 7 (locomotive tab).
     * Uses GDI to render player names from player_names[] (13-byte stride)
     * into the scrollable area defined by scroll_border_rect (+0x150).
     * Highlights the selected entry matching toolbar_sentinel (+0x6F0).
     * Tracks visible range via scroll_top_idx/scroll_bottom_idx.
     *
     * Called by: Cursor::render_editor(), INPUT_HandleLocomotiveListClick
     */
    void update_scroll_buttons();

    /**
     * Update toolbar tab sprite states.
     * Address: 0x4198B0
     *
     * Reads the tab visibility flag (+0x2B0) and active tab index (+0x2B1,
     * range 1-6). If tabs are hidden: sets all 6 tab sprites to state 2
     * (invisible). If visible: sets all tab sprites to state 0 (default),
     * then highlights the active tab with state 1.
     *
     * Called by: Cursor::draw_network_status()
     */
    void handle_tab_change();

    /**
     * Show the custom-content file-open dialog (editor state 9).
     * Address: 0x41A050 (Ghidra label "INPUT_ShowFileDialog").
     *
     * No-op if already in state 9 (editor_state, +0xEC). Starts a 200ms
     * timer (+0x18C) if not already running, sets selected_idx_384 (+0x384)
     * to -1, sets editor_state to 9, highlights sprite_2E0, dispatches
     * set_mode through the base vtable slot [3] with (childCount2,
     * childObj2) — the same base-overlay fields Cursor's other set_mode
     * dispatches use — clears sprite_height (+0x40), sets sprite_width_hi
     * (+0x3D), and repaints.
     */
    void show_file_dialog();

    /**
     * Handle a locomotive-list selection.
     * Address: 0x41A360 (Ghidra label "INPUT_HandleLocomotiveSelect").
     *
     * ui_active (+0x188) selects one of two behaviors:
     *   - Non-zero (network mode): if in state 9, cancels the file-dialog
     *     timer and resets to state 1; then unconditionally sets state 2,
     *     records the selection in selected_idx_384 (+0x384), and dispatches
     *     set_render_surface (base vtable slot [4]) on the corresponding
     *     toolbar_sprites[] entry with an origin derived from its
     *     y/width fields.
     *   - Zero (editor mode): writes bonus_ids[index] (+0x370) + 1 into
     *     the player record's (obj_184, +0x184) DPlayManager::m_unknown93
     *     (+0x93, editor-local reuse — see DPlayManager.h), then blits the
     *     edit preview.
     *
     * @param index  Selected locomotive index (also a bonus_ids[] index
     *               in the editor-mode branch — bytes 0-255, no bounds
     *               check against the 12-byte bonus_ids array; preserved
     *               faithfully as a possible original out-of-bounds read).
     */
    void handle_locomotive_select(uint32_t index);

    /**
     * Hit-test the 6 toolbar tab sprites and handle a tab change.
     * Address: 0x41A460 (Ghidra label "INPUT_HandleToolbarHover").
     *
     * Hit-tests sprite_308.._31C (+0x308..+0x31C) against (x, y), updating
     * editor_flags[1] (+0x2B1, active tab 1-6) on a match. If the active
     * tab changed: releases all 64 toolbar_sprites, cancels the active
     * timer, resets editor_state to 1, dispatches set_mode through the
     * base vtable, then calls handle_tab_change(), draw_postcard_preview(1),
     * and draw_locomotive_preview(1).
     */
    void handle_toolbar_hover(LONG x, LONG y);

    /**
     * Handle a click in the network-player scroll list.
     * Address: 0x41A650 (Ghidra label "INPUT_HandleLocomotiveListClick").
     *
     * Hit-tests the up/down scroll buttons (sprite_148/sprite_14C) and the
     * list body (scroll_border_rect, +0x150). On a body click: computes
     * the clicked row index from scroll_line_height/scroll_top_idx, reads
     * the row's player_names[] entry (+0x59E, 13-byte stride), and if
     * non-empty records the index in toolbar_sentinel (+0x6F0), sets the
     * player record's (obj_184, +0x184) DPlayManager::m_sessionBlk1[20]
     * (+0x24, editor-local reuse) depending on whether the index is below
     * player_count (+0x6F4), copies the name into the first 20 bytes of
     * DPlayManager::m_sessionBlk1 (+0x10, also editor-local reuse),
     * dispatches set_mode with (editor_surface, editor_resdata), sets
     * editor_state to 6, and blits the edit preview.
     */
    void handle_locomotive_list_click(LONG x, LONG y);

    /**
     * Toolbar edit control window procedure (subclassed).
     * Address: 0x419A60
     *
     * Subclassed WindowProc for the edit control at +0xF4. Handles:
     *   WM_CTLCOLOREDIT (0x133): returns the light-grey GDI brush (+0x380)
     *                            with dark-green text color
     *   WM_SYSCOMMAND/SC_CLOSE (0x112/0xF140): posts quit message
     *   WM_USER+0x5F5: calls upload_custom_content()
     *   WM_USER+0x5F6: re-enables the parent window
     *
     * All other messages pass through to DefWindowProcA.
     *
     * @param hWnd   HWND — edit control handle
     * @param msg    UINT — window message
     * @param wParam WPARAM — message parameter
     * @param lParam LPARAM — message parameter
     * @return       LRESULT — result of message processing
     */
    /** Binary slot [11] 0x419A60 (toolbar_wndproc). */
    LRESULT window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    /**
     * Launch file-open dialog and upload custom content.
     * Address: 0x419B10
     *
     * Opens a file-open dialog for custom content upload. Validates file
     * size (max 1MB), uploads via NET_UploadAsset, and optionally previews
     * non-WAV files as audio. Resets to editor mode 1 on completion.
     *
     * Called by: Cursor::toolbar_wndproc() (WM_USER+0x5F5 handler)
     */
    void upload_custom_content();

    /* ================================================================ */
    /* Cursor operational methods (below)                                 */
    /* ================================================================ */

    /**
     * Show the cursor editor overlay.
     * Address: 0x416B80
     *
     * Shows the cursor editor toolbar overlay. Guards on `visible` (+0xE4):
     * if already visible, returns immediately. Otherwise:
     *
     *   1. Calls Cursor::init_editor_sprites() to initialize sprite objects
     *   2. Calls vtable[7] - virtual pre-show hook
     *   3. Sets field_180 = 0
     *   4. Shows main hWnd (SW_SHOWMAXIMIZED), hides hEditWnd (SW_HIDE)
     *   5. Sets focus to main hWnd
     *   6. Resets editor flags and selection indices
     *   7. Releases all 64 toolbar sprites at +0x48C
     *   8. Creates two DDraw surfaces at +0x590 and +0x598 using the
     *      editor toolbar rect dimensions at +0x1B0
     *   9. If playerData != 0: stores as obj_184 (+0x184), copies player
     *      name/livery data into the color fields, copies player name
     *      from g_player_config
     *   10. If playerData == 0: calls init_network_player() if obj_184
     *       is null, sets edit window text to empty
     *   11. Calls update_network_names()
     *   12. Starts timer 0x53 (83) at 50ms interval stored at +0x19C
     *
     * Called by: CGWND::SetMode (mode 7) @ 0x40824C, other game mode handlers
     *
     * @param playerData  Pointer to network player data, or 0 for local/offline
     */
    void show(void* playerData);

    /**
     * Hide the cursor editor overlay and release resources.
     * Address: 0x416F70
     *
     * Guards on visible flag (+0xE4). If visible:
     *   1. Calls UI_WindowBase::hide() to hide the window
     *   2. Calls Cursor::cleanup_editor_sprites() to release editor sprites
     *   3. Kills timers at +0x18C and +0x19C
     *   4. Releases two DDraw surfaces at +0x590 and +0x598 via vtable[2]
     *   5. Releases all 64 toolbar sprites at +0x48C via vtable[0](1)
     *   6. Calls DPLAY_LeaveSession to leave network session
     *   7. Sets ui_active (+0x188) = 1, cached_client_height (+0xF0) = 1
     *
     * Called by: (indirectly via Cursor base destructor)
     */
    void hide() override;

    /**
     * Main cursor compositing / render function.
     * Address: 0x414C20
     *
     * The primary render path for the cursor overlay. When `skipRender` is 0:
     *
     *   1. Unlocks the primary surface via DDRAW_UnlockPrimary()
     *   2. If cursor_state != 0 and capture_flag == 0:
     *      a. Sets dirty rect to (-1, -1)
     *      b. Gets Windows cursor position, adjusts by current sprite hotspot
     *         (frame offset at RESDATA+0x32/+0x34)
     *      c. Clips cursor rect to viewport bounds (+0x18..+0x24)
     *      d. Stores clipped cursor rect in cursor_rect (+0x68)
     *      e. Handles animation frame advance: if sprite has 2+ frames
     *         (+0x160), increments keyframe index (+0x48) with wraparound
     *      f. Captures background from primary_surface into cursor backbuffer
     *      g. Blits cursor sprite frame (colour-keyed via 0x1008000 flag) onto
     *         cursor backbuffer
     *      h. Composites cursor backbuffer onto the primary scene backbuffer
     *         (_g_backbuffer @ 0x4FD3C0)
     *      i. Restores cursor area from primary surface (_g_primary_surface)
     *   3. If cursor_state == 0 or capture_flag != 0: only restores the
     *      old dirty-rect area from primary surface (background restoration)
     *   4. Always calls Cursor_UnlockAllSurfaces() at exit
     *
     * When `hdc` is non-zero: calls vtable[0x1A] on primary_surface (+0x38)
     * before the main render (surface lock/unlock helper).
     *
     * Called by: ~30 callers across CGWND, HelpWnd, Train subsystems
     *
     * @param hWnd         HWND - window handle (used for ClientToScreen)
     * @param hdc          HDC or context - if non-null, pre-lock surface
     * @param skipRender   byte - if non-zero, skip sprite composition
     *                     (background restoration only)
     */
    void render(HWND hWnd, void* hdc, uint8_t skipRender);

    /**
     * Render cursor with viewport-aware clipping.
     * Address: 0x415440
     *
     * Viewport-aware version of render(). Instead of using the clip rect
     * at +0x18..+0x24, this function builds a viewport rect from global
     * variables: in windowed mode uses g_clientWidth directly; in
     * fullscreen mode offsets by g_viewportX/Y.
     *
     * Two render paths:
     *
     * ACCELERATED PATH (dirty region < 256px in both dimensions):
     *   Single composite blit over the union rect:
     *     1. Capture screen background into cursor backbuffer
     *     2. Overlay cursor sprite colour-keyed onto backbuffer
     *     3. Composite backbuffer to scene backbuffer
     *
     * NORMAL PATH (large dirty region or first frame):
     *     1. Separate background restore from primary surface
     *     2. Composite cursor sprite onto backbuffer
     *     3. Composite backbuffer to scene backbuffer
     *
     * Called by: Cursor::set_capture(), Cursor::set_mode(), Cursor::handle_window_paint(),
     *            and other cursor state change handlers
     *
     * @param param  byte - if non-zero, perform full recomposite
     *               (used as dirty-rect flag from callers)
     */
    void render_with_viewport(uint8_t param);

    /**
     * Core cursor dirty-rect tracker.
     * Address: 0x414FB0
     *
     * Updates the cursor's dirty rectangle by computing the new cursor
     * rect, unioning it with the stored cursor_rect (+0x68), and either
     * performing an accelerated composite (small rect, <256px) or a
     * standard restore+render for the dirty region.
     *
     * Operation:
     *   1. Gets current cursor position via GetCursorPos()
     *   2. Adjusts by hotspot offset (RESDATA+0x32/+0x34)
     *   3. Builds new cursor rect, clips to viewport (+0x18..+0x24)
     *   4. Unions new rect with stored cursor_rect (+0x68) via UnionRect
     *   5. Expands union rect by 4px on all sides (anti-alias bleed)
     *   6. Re-clips expanded rect to viewport bounds
     *   7. If cursor active and not captured: restores background from
     *      primary surface for old rect area, then composites cursor
     *      sprite onto backbuffer and blits to scene
     *   8. Two paths: accelerated (<256px, uses union rect as single
     *      destination) vs. normal (clipped cursor dimensions)
     *
     * Called by: Cursor::set_capture(), Cursor::set_mode(), Cursor::handle_window_paint(),
     *            and internal cursor motion handlers
     *
     * @param param  byte - if non-zero, enable accelerated blit path
     *               for small dirty regions (<256px)
     */
    void update_dirty_rect(uint8_t param);

    /**
     * Toggle Windows mouse capture and OS cursor visibility.
     * Address: 0x414290
     *
     * Manages the interaction between Windows' mouse capture mechanism,
     * the OS cursor visibility, and the game's own cursor rendering.
     *
     * releaseFlag != 0 (ACQUIRE):
     *   If capture_flag (+0x58) is 0:
     *     1. Sets capture_flag = 1
     *     2. If our window has Windows capture, releases it
     *     3. Unlocks primary, updates dirty rect, unlocks all surfaces
     *     4. Calls Game_SetScreenMode with no changes
     *     5. Optionally calls RenderWithViewport if enabled
     *   If already captured, returns immediately.
     *
     * releaseFlag == 0 (RELEASE):
     *   If capture_flag is set, or GetCapture() != our window:
     *     1. Sets capture_flag = 0
     *     2. Calls SetCapture(hWnd) to re-acquire Windows capture
     *     3. Hides OS cursor via ShowCursor(FALSE) loop until < 0
     *
     * Called by: GameWindow::show/hide, HelpWnd::hide, CGWND_Screensaver_Hide
     *
     * @param releaseFlag  byte - 0 = release (hide OS cursor, capture window),
     *                      non-zero = acquire (show game cursor, release windows capture)
     */
    void set_capture(uint8_t releaseFlag);

    /**
     * NOTE ON set_mode: Cursor does NOT override the base slot [3] in the
     * binary — the Cursor vtable at 0x47793C points to UI_WindowBase::set_mode
     * (0x425FD0), NOT the GameWindow-family 0x414340. Cursor code that
     * "sets the cursor mode" (draw_locomotive_preview @ 0x418E20,
     * upload_custom_content @ 0x419B10) dispatches through vtable slot [3]
     * with (field_60, field_64, 0, 1), which resolves to the base
     * implementation (hotspot/frame-count → set_render_surface). The
     * GameWindow-family 0x414340 implementation lives in ui/GameWindow.cpp
     * (GameWindow::set_mode) and is not part of the Cursor class.
     */

    /**
     * Pre-show virtual hook (vtable[7]).
     * Address: 0x417180 (unlabeled code region in Ghidra — not a defined
     *          function; sole DATA reference is Cursor vtable slot [7] at
     *          0x47794C. The code at 0x417186 calls UI_WindowBase::on_create
     *          (0x425D30), so the hook chains to the base client-rect
     *          synchronizer. The earlier "0x426130 stub" annotation was
     *          incorrect: 0x426130 is the inherited on_async_task_failure
     *          slot [5].)
     *
     * Called before the cursor editor overlay is shown (dispatched from
     * Cursor::show @ 0x416BA1 via `CALL dword ptr [vtable+0x1C]` with no
     * arguments). Subclasses may override.
     */
    /** Binary slot [7] 0x417180 (pre-show hook). */
    void on_create() override;

    /**
     * Handle WM_PAINT dispatch for cursor window.
     * Address: 0x414A80
     *
     * Window procedure handler for paint messages. If the incoming HWND
     * matches this->hWnd (+0x08), performs:
     *   1. DDRAW_UnlockPrimary(hWnd)
     *   2. Cursor::update_dirty_rect(true)
     *   3. Cursor_UnlockAllSurfaces()
     *   4. Optionally Cursor::render_with_viewport(true)
     *
     * Returns 0 always (standard WndProc return for handled message).
     *
     * Called by: HelpWnd_HandleMouseMove, CGWND at 0x40F863,
     *            Train_HandleClick
     *
     * @param hWnd  HWND - the window being painted
     * @return      0 (handled)
     */
    int32_t handle_window_paint(HWND hWnd);
};
