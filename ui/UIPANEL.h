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

// Status: TRANSCRIBED
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
class UIPANEL : public Panel {
public:
    /* ================================================================ */
    /* Fields                                                           */
    /* ================================================================ */

    /* --- Panel / GameObject inherited fields (0x00..0xE3) --- */
    /* See Panel.h and GameObject.h for offsets 0x00-0xD3 */

    /* byte at +0xE0: string/path buffer start (zeroed in ctor) */
    uint8_t  _pad_E0[0x310];    /* +0xE0..+0x3EF (gap + path buffer) */

    /* --- UIPANEL-specific embedded sub-objects --- */
    GameObject        child_sprites;         /* +0x3F0  embedded GameObject acting as sprite manager */
    UIPANEL_Surface   surface_buf;           /* +0x478  embedded offscreen DDraw surface (0x20 bytes) */

    /* --- UIPANEL-specific fields --- */
    int32_t   _field_498;                    /* +0x498  (zeroed in ctor) */
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

    /* Content item sprite slots (6 entries) */
    void*     item_sprites[6];               /* +0x4C0..+0x4D7  sprites for content list items */

    /* Linked list management */
    void*     sprite_list_head;               /* +0x4D8  head of file/system sprite linked list */
    void*     sprite_list_tail;               /* +0x4DC  tail of file/system sprite linked list */

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

    /**
     * WindowProc — per-panel window message handler.
     * Address: 0x426900
     *
     * When the message hwnd matches this->hwnd (+0x08): unlocks primary,
     * renders the panel (UIPANEL_Render), re-locks primary.
     * Always forwards to DefWindowProcA.
     */
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * OnDestroy — panel destroy handler.
     * Address: 0x426A90
     *
     * Sets alive flag (+0xAB) to 0, calls DestroyWindow(this->hwnd).
     * If child_count (+0x0C) is 0, calls PostQuitMessage(0).
     *
     * @return  0
     */
    LRESULT OnDestroy();

    /**
     * BeginPaint — start buffered panel rendering.
     * Address: 0x426B00
     *
     * Unlocks the primary surface, then calls
     * IDirectDrawSurface4::GetDC() on the primary surface (original
     * vtable+0x44, confirmed to match GetDC's real COM ABI slot).
     * Retries up to 1000 times with 10ms delay while GetDC keeps
     * failing, then calls WIN32_FatalError()+ExitProcess(1) — a
     * genuine original fatal path. Every real caller pushes
     * this->hwnd as a second argument, but it is provably dead
     * (DDRAW_UnlockPrimary, 0x45B940, is void(void) and never reads
     * it) and is not part of this method's signature or behavior.
     * See ui/UIPANEL.cpp for the full evidence trail, including why
     * g_primary_surface being wired to a real surface today would
     * make this retry loop always exhaust and self-destruct the
     * process (Sdl3DirectDrawSurface::GetDC is a permanent no-op).
     *
     * @return  HDC from the primary surface
     */
    HDC BeginPaint();

    /**
     * EndPaint — end buffered panel rendering (simple wrapper).
     * Address: 0x426B70
     *
     * Delegates to EndPaintEx(this, this->hwnd, 0, 0, &stack_rect).
     * Has a WndProc-style calling convention (4 stack args ignored).
     */
    void EndPaint();

    /**
     * EndPaintEx — full present with cursor overlay and dirty rect.
     * Address: 0x426B90
     *
     * Main EndPaint: unlocks primary surface, computes dirty rect from
     * cursor, blits tile content into the offscreen buffer, presents the
     * dirty region to the screen, and copies the background back from the
     * backbuffer. Two paths:
     *   (A) no tile_map → simple DDRAW_PresentRect
     *   (B) tile_map active → cursor-relative blit + background restore
     *
     * @param hdc            int — HDC from BeginPaint or surface handle
     * @param unlock_param   int — parameter to unlock surface (0 = skip)
     * @param unlock_flag    byte — if 0, proceed with rendering; if non-zero, only unlock
     * @param restrict_rect  RECT* — optional clip rect to restrict present area
     */
    void EndPaintEx(int hdc, int unlock_param, byte unlock_flag, RECT* restrict_rect);

    /**
     * Render — per-frame panel foreground render with cursor overlay.
     * Address: 0x426EB0
     *
     * Computes dirty rect from cursor position, clips to viewport (+0xD4..+0xE0),
     * inflates by 4 pixels for smooth cursor when <256x256 visible region.
     * Blits tile content into offscreen buffer via UIPANEL_Blit, then copies
     * the result to the primary surface. Restores background from backbuffer.
     *
     * @param enable_tile_map  byte — if non-zero and tile_map exists, render tile content
     */
    void Render(byte enable_tile_map);

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
