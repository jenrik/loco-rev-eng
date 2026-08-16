/**
 * UIPANEL.h — Scrollable UI panel (building picker / tabbed selector)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * UIPANEL is the scrollable building-picker panel that appears during gameplay.
 * It manages a tabbed interface (Buildings, People, Vehicles, Scenery) with
 * a scrollable content area, drag-and-drop interaction, and an embedded offscreen
 * DirectDraw surface for buffered rendering.
 *
 * The class embeds a child GameObject (+0x3F0, acts as sub-sprite manager) and
 * a UIPANEL_Surface (+0x478, 0x20-byte DDraw wrapper). Eight sprite pointers
 * (+0x4A0..+0x4BC) control tab buttons and decorations, and a 6-element sprite
 * array (+0x4C0) renders content list items. A linked list at +0x4D8 tracks
 * active file/system sprites.
 *
 * Size: 0x4E0 bytes (approximately)
 * Vtable: 0x477CC8
 *
 * Class hierarchy:
 *   GameObject (0x477820)
 *     └─ Panel (0x4784C8)
 *          └─ UIPANEL (0x477CC8)  ← this class
 *
 * Vtable layout (UIPANEL overrides only [0]; slots [1+] inherited from Panel/GameObject):
 *   [0]  +0x00: scalar deleting destructor (UIPANEL_DtorChain, 0x427440)
 *   [1]  +0x04: (inherited from GameObject: StopSound)
 *   [2]  +0x08: (inherited)
 *   [3]  +0x0C: (inherited from GameObject: HitTest)
 *   [4]  +0x10: (inherited)
 *   [5]  +0x14: (inherited)
 *   [6]  +0x18: Init (Panel_Init, 0x454680)
 *   [7]  +0x1C: SetAnimState / UpdateResourceByState (inherited: Panel override)
 *   [8]  +0x20: SetFrame (inherited from GameObject)
 *   [9]  +0x24: SetName (inherited)
 *   [10] +0x28: Draw (inherited)
 *   [11] +0x2C: DrawConnected (inherited)
 *   [12] +0x30: OnTimerTick (inherited)
 *   [14] +0x38: AnimStateSelect (inherited)
 */

#pragma once

#include "../game/Panel.h"
#include "../graphics/LOCOBITMAP.h"   /* for UIPANEL_Surface struct */
#include "SaveSprite.h"

// Status: TRANSCRIBED
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
class UIPANEL : public Panel {
public:
    /* ================================================================ */
    /* Fields                                                           */
    /* ================================================================ */

    /* --- Panel / GameObject inherited fields (0x00..0xE3) --- */
    /* See Panel.h and GameObject.h for offsets 0x00-0xD3 */

    /* byte at +0xE0: string/path buffer start (zeroed in ctor). +0x1E5
     * (within this span) stores the backdrop filename set by
     * UIPANEL::Hide (0x429EF0); not carved out here (out of this pass's
     * scope). */
    uint8_t  _pad_E0[0x20A];    /* +0xE0..+0x2E9 */

    /* +0x2EA: save/backdrop full-path scratch buffer (MAX_PATH, 260
     * bytes), confirmed via disassembly of UIPANEL::InitSprite
     * (0x429A10, `LEA EBX,[EBP+0x2ea]`), UIPANEL::BlitSprite (0x429B20)
     * and UIPANEL::BlitSpriteEx (0x429DD0) — all three compose the
     * final "<install><prefix><name>.bmp" save path into this MEMBER
     * buffer, not a stack local (only BlitSprite's separate, earlier
     * directory-existence probe uses a genuine stack buffer). */
    char     save_path_buf[0x104];           /* +0x2EA..+0x3ED */

    uint8_t  _pad_3EE[2];                     /* +0x3EE..+0x3EF */

    /* --- UIPANEL-specific embedded sub-objects --- */
    GameObject        child_sprites;         /* +0x3F0  embedded GameObject acting as sprite manager */
    UIPANEL_Surface   surface_buf;           /* +0x478  embedded offscreen DDraw surface (0x20 bytes) */

    /* --- UIPANEL-specific fields --- */
    /* +0x498: pointer to the RESDATA embedded in the currently-displayed
     * SaveSprite's `data` member (i.e. `&sprite->data`, NOT the
     * SaveSprite itself) — set by DrawBorder (0x428400) to
     * `resource_ptr->data` and compared against `entity->target->data`
     * in DrawRadioButton (0x428F90). Confirmed by disassembly: DrawBorder
     * adds +0x50 to its resource_ptr (a SaveSprite*) before both storing
     * it here and reading +0xB2/+0xB4/+0x1C4 off the stored value (all
     * real RESDATA/SaveRegion offsets — see shared/types.h). */
    RESDATA*  save_header;                   /* +0x498  (zeroed in ctor) */
    uint16_t  mode;                          /* +0x49C  tab selection: 0=reset,1=init,2..5=tab index */

    /* Sprite pointers — tab buttons and decorations */
    void*     tab_sprites[4];                /* +0x4A0..+0x4AF: tab buttons 0-3 */
                                             /*   [0] +0x4A0: Buildings tab */
                                             /*   [1] +0x4A4: People tab */
                                             /*   [2] +0x4A8: Vehicles tab */
                                             /*   [3] +0x4AC: Scenery/Signals tab */
    void*     content_bg_sprite;             /* +0x4B0  content area background sprite */
    void*     list_bg_sprite;                /* +0x4B4  scrollbar track / list background */
    void*     list_text_sprite;              /* +0x4B8  scrollbar thumb / list text sprite */
    void*     sound_btn_sprite;              /* +0x4BC  sound toggle button sprite */

    /* Content item sprite slots (6 entries). Each slot's own +0x30 field
     * (on that CGWND/TrackPiece-family entity's, as-yet-unmodeled, class
     * — see UIPANEL::CreateSprite/DrawRadioButton) stores a SaveSprite*
     * pointing at the file entry currently bound to that slot; typed at
     * the two touch points that read it (DrawRadioButton, DrawBorder)
     * without redefining that entity's own class in this pass. */
    void*     item_sprites[6];               /* +0x4C0..+0x4D7  sprites for content list items */

    /* Linked list management. +0x4DC is not a "tail" in the usual sense
     * — UIPANEL::CreateSprite (0x429850) stores its own `list_entry`
     * argument there, i.e. it tracks the anchor/first-displayed node of
     * the CURRENT 6-item scroll window, not the true list tail. Kept the
     * existing name (sprite_list_tail) for continuity; see CreateSprite/
     * BlitSprite for the real usage. */
    SaveSprite* sprite_list_head;             /* +0x4D8  head of file/system sprite linked list */
    SaveSprite* sprite_list_tail;             /* +0x4DC  current scroll-window anchor node */

    /* ================================================================ */
    /* Constructor / Destructor                                         */
    /* ================================================================ */

    /**
     * UIPANEL constructor (a.k.a. InitScrollPanel).
     * Address: 0x427370
     *
     * Initializes the Panel base via RESDATA_BaseInit, then creates an embedded
     * GameObject at +0x3F0 and a UIPANEL_Surface at +0x478. Sets vtable to
     * 0x477CC8, type=0x0C (12), zeroes all sprite pointers (+0x4A0..+0x4DC)
     * and mode (+0x49C).
     *
     * Called by: RESDATA_ScriptedObject_Ctor (0x449488) to build the scroll panel
     *            at ScriptedObject+0x260 offset.
     */
    UIPANEL();

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x427440
     *
     * Calls the destructor body, then optionally frees memory if flags & 1.
     */
    virtual ~UIPANEL();

    /**
     * Destructor body.
     * Address: 0x427460
     *
     * Walks and destroys the sprite linked list (+0x4D8), dispatches cleanup
     * (vtable[6] with 0,-1,0) on self and the embedded GameObject, calls
     * RESDATA_DtorBase (Panel base cleanup), locks the offscreen surface,
     * destroys the embedded GameObject body, then calls Panel::DtorBody.
     */
    // ~UIPANEL() removed — now virtual (see above)

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * ClearChildren — destructively reset all child sprites.
     * Address: 0x427520
     *
     * Destroys all sprites in the linked list at +0x4D8, dispatches vtable[6]
     * cleanup on self and the embedded GameObject, calls Panel's base cleanup.
     * Used before reinitialization (e.g., tab switch).
     */
    void ClearChildren();

    /**
     * InitSprites — create child sprites from resources 0x2C00..0x2C13.
     * Address: 0x427580
     *
     * Guards with +0xD0 init flag; on first call, walks resources 0x2C00-0x2C13,
     * creates child sprites via RESDATA_CreateChildSprite, and stores them into
     * the tab/decoration sprite slots. The 0x2C09 resource triggers creation of
     * the sound button plus 6 content item sprites.
     *
     * Called by: RESDATA_ScriptedObject_Start (once, on panel activation)
     *
     * @return  byte — 1 on success, 0 on failure
     */
    byte InitSprites();

    /**
     * HandleDrag — tab/tool selection state machine.
     * Address: 0x4277D0
     *
     * Dispatches 6 actions based on mode param:
     *   0: Close/reset — calls vtable[1], clears active panel, returns to game
     *   1: Init all tabs — loads content via Init (vtable[6]), sets up tab
     *      button positions, shows tabs 0-2 visible, saves current world
     *   2-5: Select specific tab (2=Buildings, 3=People, 4=Vehicles, 5=Scenery)
     *        — per-case tab visibility + SetFrame, populates 6-item content
     *        viewport via CreateSprite, then scrolls to match current
     *        selection using 2-byte-aligned string comparison
     *
     * Per-case differences (from original binary):
     *   Case 2: g_active_panel = this, tab[0] shown, stack-var SndObj init
     *   Case 3: NO g_active_panel set, tab[1] shown, stack-var SndObj init
     *   Case 4: NO g_active_panel set, tab[2] shown, g_empty_string init
     *   Case 5: g_active_panel = this, tab[3] shown, calls DrawEditField
     *
     * Scroll-to-target logic (cases 2-5):
     *   1. Populate 6-item viewport from sprite linked list (+0x4D8)
     *   2. Compare sound_btn state with first visible item's state
     *   3. If current > first: scroll forward (follow next links)
     *   4. If current < first: scroll backward (follow prev links)
     *   5. Falls through to edge-scroll check
     *
     * Post-switch: all paths reach the edge scrolling check at +0xAD.
     *
     * @param resource   int — resource/context parameter (stored at +0xD4)
     * @param action     uint16_t — 0..5 action index
     * @return           byte — 1 on success, 0 on init failure
     */
    byte HandleDrag(int resource, uint16_t action);

    /* WindowProc/OnDestroy/BeginPaint/EndPaint/EndPaintEx/Render were
     * declared here (addresses 0x426900-0x426EB0) but do NOT belong to
     * UIPANEL — corrected 2026-08-16. get_xrefs_to on every one of them
     * shows callers exclusively from GameSetupPanel, Cursor, NameEntryPanel,
     * BuildingPanel, PostcardAlbum, Town, DPlayManager, NETMAN_* — never a
     * UIPANEL instance. A Ghidra function-address-range listing confirms
     * they sit in the same contiguous MSVC method block as
     * UI_WindowBase_SetMode (0x425FD0)/SetRenderSurface (0x426020)/
     * dispatch_message (0x426140), ending right before UIPANEL's own real
     * ctor (UIPANEL_InitScrollPanel, 0x427370) begins a new block — i.e.
     * these six are UI_WindowBase members, the "UIPANEL_" Ghidra prefix is
     * a stale misnomer. WindowProc (0x426900) and OnDestroy (0x426A90) were
     * dead duplicates of the already-correct UI_WindowBase::on_mouse_move()/
     * on_close() (ui/UI_WindowBase.cpp) and have been removed entirely.
     * BeginPaint (0x426B00) has been moved to UI_WindowBase (see its real
     * declaration/implementation there). EndPaint (0x426B70)/EndPaintEx
     * (0x426B90)/Render (0x426EB0) remain temporarily in ui/UIPANEL.cpp as
     * free functions pending a dedicated migration — do not re-declare them
     * as UIPANEL methods; see PROGRESS.md's 2026-08-16 correction entry. */

    /**
     * StopSound — halt panel sound playback (vtable[7], overrides
     * Entity::StopSound — Panel's real base is Entity, not GameObject
     * directly; see game/Panel.h).
     */
    void StopSound(int param) override;
};

/* ================================================================== */
/* Static dispatch table — UIPANEL WindowProc entries                  */
/* ================================================================== */
extern void* g_ui_panel_wndproc_table;  /* 0x477C80 — WndProc dispatch for UIPANEL */
