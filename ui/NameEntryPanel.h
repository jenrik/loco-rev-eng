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
 *   [2] +0x08: Show                        (inherited: UI_WindowBase_Show, 0x4259C0)
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

    uint8_t    field_E8;               // +0xE8  (unknown byte, init 0)
    uint8_t    _pad_E9[3];             // +0xE9  padding

    int32_t    field_EC;               // +0xEC  (unknown, init 0)

    uint8_t    field_F0;               // +0xF0  (unknown byte, init 0)
    uint8_t    _pad_F1[3];             // +0xF1  padding

    /* +0xF4..+0x13F: unknown — not initialized by Init */

    int32_t    gameMode;               // +0x140  Game mode / max players count (init 3)

    int32_t    field_144;              // +0x144  (unknown, init 0)

    /* +0x148..+0x1AB: unknown — not initialized by Init */

    uint8_t    hasSprites;             // +0x1AC  Flag: non-zero when sprites are allocated
    uint8_t    _pad_1AD[3];            // +0x1AD  padding

    ButtonSprite* sprite0;             // +0x1B0  ButtonSprite for res 0x419
    ButtonSprite* sprite1;             // +0x1B4  ButtonSprite for res 0x41A
    ButtonSprite* sprite2;             // +0x1B8  ButtonSprite for res 0x417
    ButtonSprite* sprite3;             // +0x1BC  ButtonSprite for res 0x418
    ButtonSprite* sprite4;             // +0x1C0  ButtonSprite for res 0x41F
    ButtonSprite* sprite5;             // +0x1C4  ButtonSprite for res 0x420
    ButtonSprite* sprite6;             // +0x1C8  ButtonSprite for res 0x421

    void*        spriteTerminator;     // +0x1CC  Always 0 (terminator after sprite array)

    /* +0x1D0: padding */

    HBRUSH       backgroundBrush;      // +0x1D4  Solid brush for background (color 0xA8C4D8)

    int32_t      field_1D8;            // +0x1D8  (unknown, init 0)

    uint8_t      field_1E0;            // +0x1E0  (unknown byte, init 0)
    uint8_t      field_1E1;            // +0x1E1  (unknown byte, init 0)
    /* Total: 0x1E4 bytes */

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
