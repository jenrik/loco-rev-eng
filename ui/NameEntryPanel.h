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
 * Vtable layout (extends UI_WindowBase 12-slot vtable):
 *   [0] +0x00: scalar deleting destructor (NameEntryPanel_Dtor, 0x440F80)
 *   [1] +0x04: Hide                        (inherited: UI_WindowBase_Hide, 0x425990)
 *   [2] +0x08: Show                        OVERRIDDEN: NETMAN_JoinSession (0x441870),
 *                                           NOT the inherited UI_WindowBase_Show —
 *                                           confirmed via the vtable data at 0x4781D8.
 *                                           NETMAN_JoinSession inits the 7 sprites,
 *                                           starts a timer, and ends by calling
 *                                           DPlayManager::RenderConnectionPanel (0x4421D0)
 *                                           on `this`, which is the concrete evidence that
 *                                           RenderConnectionPanel's `void* panel` parameter
 *                                           is really a `NameEntryPanel*` (see
 *                                           network/DPlayManager.cpp / DPlayManager.h).
 *                                           NETMAN_JoinSession itself remains
 *                                           undecompiled/unimplemented (declared only,
 *                                           `network/Netman.h`'s `NETMAN_JoinSession`,
 *                                           dead: not defined or called anywhere in-tree).
 *   [3] +0x0C: virtual (default stub)      (inherited: 0x425FD0)
 *   [4] +0x10: virtual (default stub)      (inherited: 0x426020)
 *   [5] +0x14: virtual (default stub)      (inherited: 0x426130)
 *   [6] +0x18: CreateFullWindow            (inherited: UI_CreateFullWindow, 0x425B70)
 *   [7] +0x1C: OnCreate                    (inherited: UI_WindowBase_OnCreate, 0x425D30)
 *   [8] +0x20: virtual (default stub)      (inherited: 0x426130)
 *   [9] +0x24: virtual (default no-op)     (inherited: 0x4661A0)
 *   [10]+0x28: virtual (default stub)      (inherited: 0x426140)
 *   [11]+0x2C: WindowProc                  (inherited: UI_DefWndProc, 0x422EA0)
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

    /* +0x148: input-gate flag. Evidence: NETMAN_JoinSession (0x441870)
     * clears it to 0 unconditionally on (re)open; NETMAN_DestroySession
     * (0x441F80) and NETMAN_SetSessionInfo (0x441C80) both gate all
     * ENTER/ESC/click handling on `*(char*)(this+0x148) != 0`. No in-tree
     * setter that assigns it nonzero has been found yet (same read-side-
     * only situation as field_E8 above) — open TODO for whoever
     * decompiles the rest of NETMAN_JoinSession/UI_DefWndProc for this
     * panel. +0x149..+0x14B remain an unevidenced gap. */
    uint8_t    inputEnabled;           // +0x148  nonzero = ENTER/ESC/click handling active
    uint8_t    _gap_149[3];            // +0x149  gap (unnamed, unevidenced)

    /* +0x14C/+0x150: second scroll-offset pair, read by RenderConnectionPanel
     * and passed to OffsetRect() alongside UI_WindowBase::workRect's
     * left/top (+0xD4/+0xD8, the panel's "first" scroll offset pair) when
     * blitting the child surface (+0x1D0, below). */
    int32_t    scrollOffsetX2;         // +0x14C  second blit-offset X delta
    int32_t    scrollOffsetY2;         // +0x150  second blit-offset Y delta

    /* +0x154..+0x18B: unknown — not initialized by Init */
    uint8_t    _gap_154[0x38];         // +0x154  gap (unnamed, unevidenced)

    /* +0x18C (16 bytes): panel bounding RECT. Evidence: RenderConnectionPanel
     * reads it as {left,top,right,bottom} and passes it to UI_CenterWindow()
     * as the outer RECT*. */
    RECT       panelRect;              // +0x18C  panel bounding RECT

    /* +0x19C..+0x1AB: unknown — not initialized by Init */
    uint8_t    _gap_19C[16];           // +0x19C  gap (unnamed, unevidenced)

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
                                       //        (0x441870, undecompiled) repurposes this slot
                                       //        for a resource pointer (ResourceManager_GetById
                                       //        0x439) the first time it runs.

    /* +0x1D0: previously documented as padding — WRONG. RenderConnectionPanel
     * (0x4421D0) dereferences it as `*(void**)(this+0x1D0)` and passes it to
     * UIPANEL_Blit as the source surface for the child-window blit. Its
     * writer is identified but not yet decompiled: NETMAN_JoinSession
     * (0x441870) calls vtable slot [1] (+0x04) on the +0x1CC resource
     * pointer above with (0,0) and stores the result here — evidently
     * creating/fetching the child surface — but that call target isn't
     * decompiled, so `void*` is kept rather than guessing a surface type. */
    void*        childSurface;         // +0x1D0  child surface blitted by RenderConnectionPanel

    HBRUSH       backgroundBrush;      // +0x1D4  Solid brush for background (color 0xA8C4D8)

    /* +0x1D8: child edit-control HWND for the typed player/session name.
     * Evidence: NETMAN_DestroySession (0x441F80) and NETMAN_SetSessionInfo
     * (0x441C80) both call GetWindowTextA(*(HWND*)(this+0x1D8), buf, 0x40)
     * to copy the typed text into GameConfig::m_sessionName. Previously
     * documented as an unevidenced int32_t; not yet confirmed which
     * Init()/OnCreate() path creates this child control. */
    HWND         nameEditHwnd;         // +0x1D8  edit control HWND (name/session text entry)

    uint8_t      field_1E0;            // +0x1E0  (unknown byte, init 0)
    uint8_t      field_1E1;            // +0x1E1  (unknown byte, init 0)
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
