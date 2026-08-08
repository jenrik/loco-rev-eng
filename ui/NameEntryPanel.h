/**
 * NameEntryPanel.h — Name-entry / multiplayer lobby panel
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * NameEntryPanel is a full-screen UI panel displayed as part of the
 * main menu. It handles player name entry, network session setup, and
 * multiplayer lobby display. It is created as a child of UI_MainMenu
 * (EditWindow) and stored at +0x21C in the parent.
 *
 * The panel manages 7 ButtonSprite objects (resources 0x417..0x421)
 * for its UI elements, a background brush, and game-mode state.
 *
 * Inheritance:
 *   UI_WindowBase (0xE8 bytes, vtable 0x477C30)
 *     +-- NameEntryPanel (+0xFC bytes subclass data)
 *
 * Size: 0x1E4 bytes
 * Vtable address in loco.exe: 0x4781D0
 *
 * Vtable layout (extends UI_WindowBase's full 37-slot vtable —
 * ui/UI_WindowBase.h — not the 12-slot table this header previously
 * described; re-derived by reading the raw vtable bytes at 0x4781D0 and
 * matching slot [0] against NameEntryPanel_Dtor, 0x440F80, then indexing
 * every other known override by ((address - 0x4781D0) / 4)). Only slots
 * overridden by NameEntryPanel are listed; all others inherit
 * UI_WindowBase's default exactly as documented there:
 *   [0] +0x00: scalar deleting destructor  OVERRIDDEN: NameEntryPanel_Dtor, 0x440F80
 *   [1] +0x04: hide()                      OVERRIDDEN: NETMAN_LeaveSession (0x441A00,
 *                                           native/NETMAN_NetworkUI.c) — destroys the 7
 *                                           sprites/child surface then calls the inherited
 *                                           UI_WindowBase::hide(). Previously (wrongly)
 *                                           documented as inherited/not overridden.
 *   [2] +0x08: show()                      OVERRIDDEN: NETMAN_JoinSession (0x441870,
 *                                           native/NETMAN_NetworkUI.c) —
 *                                           confirmed via the vtable data at 0x4781D8.
 *                                           Inits the 7 sprites, starts a timer, and ends
 *                                           by calling DPlayManager::RenderConnectionPanel
 *                                           (0x4421D0) on `this`, which is the concrete
 *                                           evidence that RenderConnectionPanel's
 *                                           `void* panel` parameter is really a
 *                                           `NameEntryPanel*` (see
 *                                           network/DPlayManager.cpp / DPlayManager.h).
 *   [7] +0x1C: on_create()                 OVERRIDDEN: sub_441360 (not yet decompiled;
 *                                           called from NETMAN_JoinSession via
 *                                           `panel->on_create()`). Previously (wrongly)
 *                                           documented as inherited UI_WindowBase_OnCreate.
 *   [8] +0x20: on_update(int32_t)           OVERRIDDEN: NETMAN_UpdateSessionInfo (0x441A90,
 *                                           native/NETMAN_NetworkUI.c) — blits the child
 *                                           surface, resets sprite states, refreshes
 *                                           session info, ends paint. Previously (wrongly)
 *                                           documented as inherited default no-op.
 *   [11]+0x2C: window_proc()               OVERRIDDEN: sub_442150 (not yet decompiled).
 *                                           Previously (wrongly) documented as inherited
 *                                           UI_DefWndProc.
 *   [12]+0x30: on_timer()                  OVERRIDDEN: sub_4423D0 (not yet decompiled;
 *                                           plausibly drives the 50ms animation timer
 *                                           NETMAN_JoinSession starts).
 *   [14]+0x38: on_lbutton_down()           OVERRIDDEN: NETMAN_SetSessionInfo (0x441C80,
 *                                           native/NETMAN_NetworkUI.c) — hit-tests the
 *                                           WM_LBUTTONDOWN lParam (packed x/y) against the
 *                                           7 sprites' rects + panelClickRect. Confirmed by
 *                                           slot arithmetic from the 0x4781D0 base: the
 *                                           real signature carries the full
 *                                           (HWND, UINT, WPARAM, LPARAM) override params
 *                                           (matching UI_WindowBase::on_lbutton_down), even
 *                                           though only lParam's low/high words are read.
 *
 * All other slots (including [3]/[4]/[6]/[9]/[10]/[13]/[15]-[36]) inherit
 * UI_WindowBase's defaults unmodified.
 *
 * Called by: UI_MainMenu_Create @ 0x42058D (alloc 0x1E4, ctor, createWindow)
 */

#pragma once

#include "UI_WindowBase.h"

// Status: TRANSCRIBED
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* Forward declaration */
class ButtonSprite;

class NameEntryPanel : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* ---- Inherited from UI_WindowBase (0x00..0xE7) ---- */
    /* (see UI_WindowBase.h for full layout) */
    /* ---- NameEntryPanel-specific fields (0xE8..0x1E3) ---- */

    uint8_t    field_E8;               // +0xE8  (unknown byte, init 0) — read/set as a
                                       //        "text dirty" flag by
                                       //        DPlayManager::RenderConnectionPanel (0x4421D0)
    uint8_t    _pad_E9[3];             // +0xE9  padding

    int32_t    field_EC;               // +0xEC  (unknown, init 0) — receives the SetTimer()
                                       //        result in NETMAN_JoinSession (0x441870)

    /* +0xF0 (64 bytes): join-panel text buffer. Evidence: NETMAN_JoinSession
     * (0x441870) fills it via FormatResourceString(&g_resmgr, 0x79, this+0xF0, 0x40);
     * DPlayManager::RenderConnectionPanel (0x4421D0) renders it with
     * DrawTextA(hdc, this+0xF0, -1, ...). Previously modeled as a lone byte
     * (field_F0) plus an "unknown, not initialized by Init" gap through
     * +0x13F — Init only ever clears byte [0] (null-terminates the buffer),
     * which is why the gap looked uninitialized. */
    char       textBuffer[64];         // +0xF0  join-session text (null-terminated)

    /* +0x130 (16 bytes): scratch RECT used while laying out textBuffer's
     * drawn position. Evidence: RenderConnectionPanel builds it from the
     * panel RECT (below) via DrawTextA(..., &textDrawRect, ...), then
     * OffsetRect()s it per the alignment mode (gameMode, below). */
    RECT       textDrawRect;           // +0x130  text draw/layout RECT

    int32_t    gameMode;               // +0x140  Game mode / max players count (init 3).
                                       //        Also read by RenderConnectionPanel as a
                                       //        text-alignment selector (0=right, 1=left,
                                       //        2=bottom, else=top) — same dual-use pattern
                                       //        already documented on GameSetupPanel's
                                       //        textAlignMode (+0x1B0 there).

    int32_t    field_144;              // +0x144  (unknown, init 0)

    /* +0x148: "paint ready" gate byte. Evidence (native/NETMAN_NetworkUI.c,
     * native/NETMAN_SessionSettings.c): NETMAN_JoinSession (0x441870) clears
     * it to 0 unconditionally on (re)open; NETMAN_UpdateSessionInfo
     * (0x441A90) sets it to 1 after blitting the child surface + resetting
     * sprite states; NETMAN_SetSessionInfo (0x441C80, the panel's
     * on_lbutton_down override) AND NETMAN_DestroySession (0x441F80) both
     * gate all ENTER/ESC/click handling on `*(char*)(this+0x148) != 0` —
     * makes the flag's name/lifecycle coherent for the first time
     * (previously mistranscribed at a scaled ×4 offset, +0x52). */
    uint8_t    paintReadyFlag;         // +0x148  1 once the panel has painted at least once
    uint8_t    _gap_149[3];            // +0x149  gap (unnamed, unevidenced)

    /* +0x14C/+0x150: second scroll-offset pair, read by RenderConnectionPanel
     * and passed to OffsetRect() alongside UI_WindowBase::workRect's
     * left/top (+0xD4/+0xD8, the panel's "first" scroll offset pair) when
     * blitting the child surface (+0x1D0, below). Also reused directly as
     * the blit's destination X/Y by NETMAN_UpdateSessionInfo (0x441A90). */
    int32_t    scrollOffsetX2;         // +0x14C  second blit-offset X delta / blit dest X
    int32_t    scrollOffsetY2;         // +0x150  second blit-offset Y delta / blit dest Y

    /* +0x154/+0x158: blit destination width/height. Evidence:
     * NETMAN_UpdateSessionInfo (0x441A90) passes these as UIPANEL_Blit's
     * clip_w/clip_h alongside scrollOffsetX2/Y2 above. */
    int32_t    blitDestWidth;          // +0x154  child-surface blit destination width
    int32_t    blitDestHeight;         // +0x158  child-surface blit destination height

    /* +0x15C (16 bytes): session-name EDIT control's placement rect
     * {left,top,right,bottom}. Evidence: NETMAN_EnumerateSessions (0x441720)
     * reads left/top as CreateWindowExA's x/y and right-left/bottom-top as
     * its width/height when creating sessionNameEditHwnd (below). */
    RECT       editControlRect;        // +0x15C  session-name edit control placement

    /* +0x16C..+0x18B: unknown — not initialized by Init */
    uint8_t    _gap_16C[0x20];         // +0x16C  gap (unnamed, unevidenced)

    /* +0x18C (16 bytes): panel bounding RECT. Evidence: RenderConnectionPanel
     * reads it as {left,top,right,bottom} and passes it to UI_CenterWindow()
     * as the outer RECT*. */
    RECT       panelRect;              // +0x18C  panel bounding RECT

    /* +0x19C (16 bytes): secondary hit-test RECT for the panel background.
     * Evidence: NETMAN_SetSessionInfo (0x441C80) PtInRect()s this rect
     * (distinct from panelRect above) to decide whether a click outside all
     * sprites should play a random ambience sound. Previously modeled as
     * an unnamed gap. */
    RECT       panelClickRect;         // +0x19C  panel background click-test rect

    uint8_t    hasSprites;             // +0x1AC  Flag: non-zero when sprites are allocated
    uint8_t    _pad_1AD[3];            // +0x1AD  padding

    ButtonSprite* sprite0;             // +0x1B0  ButtonSprite for res 0x419
    ButtonSprite* sprite1;             // +0x1B4  ButtonSprite for res 0x41A
    ButtonSprite* sprite2;             // +0x1B8  ButtonSprite for res 0x417
    ButtonSprite* sprite3;             // +0x1BC  ButtonSprite for res 0x418
    ButtonSprite* sprite4;             // +0x1C0  ButtonSprite for res 0x41F
    ButtonSprite* sprite5;             // +0x1C4  ButtonSprite for res 0x420
    ButtonSprite* sprite6;             // +0x1C8  ButtonSprite for res 0x421

    void*        spriteTerminator;     // +0x1CC  Always 0 after Init() (terminator after
                                       //        sprite array) — but NETMAN_JoinSession
                                       //        (0x441870, now decompiled in
                                       //        native/NETMAN_NetworkUI.c) repurposes this
                                       //        slot for a resource pointer
                                       //        (ResourceManager_GetById 0x439) the first
                                       //        time it runs.

    /* +0x1D0: previously documented as padding — WRONG. RenderConnectionPanel
     * (0x4421D0) dereferences it as `*(void**)(this+0x1D0)` and passes it to
     * UIPANEL_Blit as the source surface for the child-window blit. Its
     * writer: NETMAN_JoinSession (0x441870, native/NETMAN_NetworkUI.c) calls
     * vtable slot [1] (+0x04, "Lock/GetSurface" per shared/types.h's RESDATA
     * convention) on the +0x1CC resource pointer above with (0,0) and stores
     * the result here — evidently creating/fetching the child surface. The
     * resource object's real class still isn't modeled (ResourceManager_GetById
     * returns `void*` everywhere in this tree), so `void*` is kept here too. */
    void*        childSurface;         // +0x1D0  child surface blitted by RenderConnectionPanel

    HBRUSH       backgroundBrush;      // +0x1D4  Solid brush for background (color 0xA8C4D8)

    /* +0x1D8: session-name EDIT control HWND. Evidence: NETMAN_EnumerateSessions
     * (0x441720) creates the child EDIT control and stores its HWND here;
     * NETMAN_GetSessionInfo/NETMAN_SetSessionInfo (0x441B40/0x441C80) show/
     * hide/focus/read it via this same slot; NETMAN_DestroySession (0x441F80)
     * and NETMAN_SetSessionInfo also call GetWindowTextA(*(HWND*)(this+0x1D8),
     * buf, 0x40) to copy the typed text into GameConfig::m_sessionName.
     * Previously modeled as a plain `int32_t`, matching the original x86
     * 4-byte HWND — retyped to `HWND` (a real pointer on this host; see
     * AGENTS.md "Host deviations": exact x86 layout parity is a non-goal
     * off-Windows, only the +0x1D8 provenance comment is preserved). */
    HWND         sessionNameEditHwnd;  // +0x1D8  session-name EDIT control HWND

    /* +0x1DC: previously undocumented gap. Evidence: NETMAN_EnumerateSessions
     * (0x441720) subclasses sessionNameEditHwnd via
     * SetWindowLongA(hwnd, GWL_WNDPROC, 0x4417E0) and stores the *previous*
     * (original EDIT control) WndProc here, for chaining in the subclass
     * procedure. */
    void*        originalEditWndProc; // +0x1DC  EDIT control's original WndProc (pre-subclass)

    /* +0x1E0/+0x1E1: session player-count mode-availability flags, set from
     * GameConfig::m_providerList (game/GameConfig.h) entries' type field.
     * Evidence: NETMAN_CreateSession (0x4419C0) and NETMAN_JoinSession
     * (0x441870) both walk the provider list and set supportsTwoPlayerMode
     * when a provider's type == 4, supportsFourPlayerMode when type == 2.
     * NETMAN_GetSessionInfo/NETMAN_SetSessionInfo gate the 2-player/4-player
     * sprite buttons (sprite2/sprite3) on these same flags; ui/EditWindow.cpp
     * reads them directly (as the previous field_1E0/field_1E1 names) to
     * decide the post-session game-mode transition. */
    uint8_t      supportsTwoPlayerMode;  // +0x1E0  non-zero: a 2-player provider is available
    uint8_t      supportsFourPlayerMode; // +0x1E1  non-zero: a 4-player provider is available
    /* Total: 0x1E4 bytes */

    static_assert(sizeof(RECT) == 16, "NameEntryPanel layout assumes 16-byte RECT");

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * NameEntryPanel constructor.
     * Address: 0x440F20
     *
     * Chains to UI_WindowBase(hInstance, resId); C++ installs the subclass
     * vtable (0x4781D0), then calls Init() to initialize
     * all subclass fields and allocate 7 ButtonSprite objects.
     *
     * Called by: UI_MainMenu_Create @ 0x42058D with hInstance + resId=0x1F6
     *
     * @param hInstance  Application instance handle
     * @param resId      Window resource ID (0x1F6 = 502)
     */
    NameEntryPanel(HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x440F80
     *
     * Calls BaseDtor to release sprites, sound resources, and background
     * brush, then optionally frees the heap allocation.
     *
     * @param flags  Delete flag (bit 0 = free heap memory)
     */
    virtual ~NameEntryPanel();

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Init — Initialize panel fields and create sprite objects.
     * Address: 0x440FA0
     *
     * Zeroes all subclass fields, sets gameMode to 3, creates a solid
     * background brush (color 0xA8C4D8 = RGB(216, 196, 168)), then
     * allocates 7 ButtonSprite objects with resource IDs 0x417..0x421.
     * Stores this panel pointer in the global at 0x485260.
     *
     * Called by: constructor (0x440F20 @ +0x5E)
     */
    void init();

    /**
     * Base destructor — Release sprites, sound, brush.
     * Address: 0x441190
     *
     * Destroys all 7 ButtonSprites, releases sound resource 0x5015,
     * deletes the background brush, then calls the inherited base
     * destructor (UI_WindowBase::base_destructor).
     *
     * Called by: scalar deleting destructor (vtable[0])
     */
    void base_destructor();

    /**
     * CreateWindow — Create the full-screen panel window.
     * Address: 0x4412F0
     *
     * Loads icon resource 0x65 from the instance handle, stores it at
     * field 0x144, then calls UI_CreateFullWindow to register and create
     * a full-screen window covering the entire desktop.
     *
     * @param hWndParent  Parent window HWND
     * @return            true on success, false on failure
     */
    bool create_window(HWND hWndParent);
};
