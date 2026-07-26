/**
 * InputMgr.h — Input manager class for cursor editor and input handling
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * InputMgr manages the toolbar editor, file dialogs, network player
 * configuration, color adjustment tools, and the cursor editor panel.
 * It extends UI_WindowBase and works closely with the Cursor class.
 *
 * The Cursor class (in Cursor.h/cpp) handles the actual mouse cursor
 * rendering, while InputMgr handles the editor panel UI and input state
 * management for the toolbar.
 *
 * Vtable: 0x477960 (Cursor editor panel dispatch table)
 *
 * Class hierarchy:
 *   UI_WindowBase (vtable 0x477C30)
 *     └─ InputMgr / Cursor (vtable 0x477930 / 0x477960)
 *
 * Size: ~0x740 bytes (same allocation as Cursor, shares layout)
 * Global: g_input_mgr at 0x4A9990
 */

#pragma once

#include "../shared/types.h"
#include "../ui/UI_WindowBase.h"

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

struct UISprite;
struct PlayerRecord;
struct NETMAN;
struct DDRAW_Building;

/* ================================================================== */
/* External global addresses                                           */
/* ================================================================== */

#define ADDR_g_input_mgr            0x004A9990  /* InputMgr singleton */
#define ADDR_g_netman               0x004FD3AC  /* NETMAN singleton */
#define ADDR_g_player_config        0x004AA4A8  /* PlayerConfig */
#define ADDR_g_tooltip_mgr          0x0048526C  /* Tooltip/UI manager */

/* ================================================================== */
/* InputMgr — Toolbar editor and input state manager                    */
/* ================================================================== */

class InputMgr : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this, after UI_WindowBase at +0xE8)          */
    /* ================================================================ */

    /* vtable at +0x00 is compiler-managed */

    /* --- Editor state fields, in assembly offset order --- */
    uint8_t   _pad_e8[4];              // +0xE8
    int32_t   editorState;             // +0xEC  panel/editor state (1=idle, 5=color-adjust, 9=file-dialog, etc.)
    uint8_t   _pad_f0_to_18c[0x9C];    // +0xF0
    void*     timerHandle;             // +0x18C  active timer handle (0x44 for animation)
    uint8_t   _pad_190[4];             // +0x190
    uint8_t   colorAdjustFlag;         // +0x194  non-zero when color adjustment is active
    uint8_t   _pad_195[3];             // +0x195
    int32_t   _pad_198[5];             // +0x198
    uint8_t   _pad_1ac_to_24c[0xA0];   // +0x1AC
    void*     colorPreviewTimer;       // +0x24C  color preview timer handle
    uint8_t   _pad_250_to_2b1[0x61];   // +0x250
    uint8_t   tabHoverIndex;           // +0x2B1  current toolbar tab hover index (1-6, 0=none)
    uint8_t   _pad_2b2[6];             // +0x2B2
    int32_t   tabResetIndex1;          // +0x2B8  reset tracking index 1 (set to 0xFFFF on tab change)
    int32_t   tabResetIndex2;          // +0x2BC  reset tracking index 2
    uint8_t   colourPalette[10][4];    // +0x2C0  ten RGBA colour entries
    uint8_t   _pad_2e8_to_308[0x20];   // +0x2E8

    /* --- Tab hit-rect regions (6 tabs) --- */
    int32_t   tabRects[6][4];          // +0x308  (left, top, right, bottom) per tab
    uint8_t   _pad_368_to_384[0x1C];   // +0x368
    int32_t   tabResetIndex3;          // +0x384  reset tracking index 3
    uint8_t   _pad_388_to_48c[0x104];  // +0x388
    void*     toolbarSprites[64];      // +0x48C  array of 64 toolbar sprite pointers

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * InputMgr constructor.
     * Address: 0x415980 (shared with Cursor_Ctor)
     *
     * Allocates 0x740 bytes, initializes editor state, sprites,
     * toolbar tables, and network player records.
     */
    InputMgr(HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor. Virtual via vtable[0].
     * Address: 0x4159E0 (shared with Cursor_Dtor)
     *
     * Releases 40+ sprite objects, GDI brush, player records,
     * chains to UI_WindowBase destructor.
     */
    virtual ~InputMgr();

    /* ================================================================ */
    /* Editor state management                                           */
    /* ================================================================ */

    /**
     * Show file dialog — transition panel into file-dialog mode (state 9).
     * Address: 0x41A050
     * __fastcall (ECX = this)
     *
     * Guards against re-entry, schedules 200ms timer (ID 0x44),
     * highlights file-dialog sprite, calls vtable[3] to position dialog,
     * and flushes repaint.
     */
    void ShowFileDialog();

    /**
     * Cancel color-adjust mode or dismiss tooltip.
     * Address: 0x41AA40
     * __thiscall (ECX = this, 4 stack params)
     *
     * Vtable[+0x0C] dispatch. State-dependent:
     *   state 5: clears color flag, resets to idle, kills preview timer, resets sprites
     *   state 9: dismisses tooltip overlay
     */
    void CancelColorAdjust(int32_t unused1, int32_t unused2, int32_t unused3, int32_t unused4);

    /* ================================================================ */
    /* Network player management                                         */
    /* ================================================================ */

    /**
     * Initialize/reallocate the local network player record.
     * Address: 0x41A0E0
     * __fastcall (ECX = this)
     *
     * Releases any existing PlayerRecord, allocates a new one via
     * DPLAY_CreatePlayer, copies player description and name from config,
     * and syncs player locomotive color.
     */
    void InitNetworkPlayer();

    /* ================================================================ */
    /* Toolbar tab management                                            */
    /* ================================================================ */

    /**
     * Handle toolbar hover — set tab highlight on mouse hover.
     * Address: 0x41A460
     * __thiscall (ECX = this, x/y on stack)
     *
     * Hit-tests cursor position against 6 tab sprite rects. Sets
     * tabHoverIndex (1-6) when hovered. On tab change:
     *   1. Resets 3 tracking indices to 0xFFFF
     *   2. Frees all 64 toolbar sprite pointers
     *   3. Kills active timer, resets state to 1
     *   4. Clears highlight state
     *   5. Plays tab switch sound (0x1402)
     */
    void HandleToolbarHover(int32_t cursorX, int32_t cursorY);

    /* ================================================================ */
    /* Edit set focus / scheduled events                                 */
    /* ================================================================ */

    /**
     * Edit set focus — scheduled event trigger check.
     * Address: 0x41FF20
     * __thiscall (ECX = this)
     *
     * Checks if a scheduled event (date/time range) is active.
     * Uses Game_IsPositionBetween (0x412790) for date/time range check
     * with wrap support. Activates edit mode if within the scheduled window.
     */
    void EditSetFocus();

    /* ================================================================ */
    /* State query helpers                                               */
    /* ================================================================ */

    /** Get current editor state */
    int32_t GetState() const { return editorState; }

    /** Check if in file dialog mode */
    bool IsFileDialogMode() const { return editorState == 9; }

    /** Check if in color adjust mode */
    bool IsColorAdjustMode() const { return editorState == 5; }

    /** Check if idle */
    bool IsIdle() const { return editorState == 1; }

    /* ================================================================ */
    /* Lifecycle methods                                                 */
    /* ================================================================ */

    /** Destructor body — frees timer buffer. Address: 0x41D2B0 */
    void DtorBody();

    /** Full initialization helper. */
    void Init();

    /* ================================================================ */
    /* File / world management                                           */
    /* ================================================================ */

    /** Load a saved .loco world from disk. Address: 0x41D5C0 */
    void LoadSaveFile(const char* filename);

    /** Save current game state to .loco file. Address: 0x41D9B0 */
    void SaveCurrentWorld(const char* filename);

    /** Generate save file name. Address: 0x41DD40 */
    const char* GetSaveFileName();

    /** Calculate new world dimensions. Address: 0x41D2D0 */
    void NewWorld();

    /* ================================================================ */
    /* Editor / colour management                                        */
    /* ================================================================ */

    /** Draw colour selection grid. Address: 0x41DEF0 */
    void DrawColourGrid();

    /** Set selected colour index. Address: 0x41E100 */
    void SetColourIndex(int32_t index);

    /** Adjust R/G/B colour component. Address: 0x41E120 */
    void AdjustColorComponent(int32_t component, int32_t delta);

    /** Init/reset tile-edit dialog. Address: 0x41E6E0 */
    void HandleEditMessage();

    /** Parse .dat tile descriptor file. Address: 0x41E9F0 */
    void EditWndProc();

    /** Exit game / tile edit mode. Address: 0x41E570 */
    void ExitGame();

    /** Render editor overlay. Address: 0x418210 */
    void RenderEditor();

    /** Switch to locomotive selection tab. Address: 0x41F6E0 */
    void SwitchToLocomotiveTab();

    /* ================================================================ */
    /* Per-frame / config                                                */
    /* ================================================================ */

    /** Per-frame input processing. Address: 0x41EFA0 */
    void WorldTick();

    /** Load input configuration. Address: 0x41F430 */
    void LoadConfig();

    /** Update scroll position for locomotive list. Address: 0x41FBE0 */
    void UpdateScrollPosition(int32_t delta);

    /** Update network player name array. Address: 0x416E00 */
    void UpdateNetworkNames();

    /* ================================================================ */
    /* Hit-test helpers                                                  */
    /* ================================================================ */

    /** Point-in-rect hit test. Address: 0x41DD80 */
    int32_t IsPointerInRect(int32_t x, int32_t y, const RECT* r);

    /** Toolbar window procedure. Address: 0x419A60 */
    static LRESULT ToolbarWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

};