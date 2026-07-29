/**
 * GameSetupPanel.h — Game setup / city-selection lobby panel
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * GameSetupPanel is a full-screen lobby window for game setup and city
 * (play-scenario) selection. It is created as a child of UI_MainMenu
 * (EditWindow) and stored at +0x220 in the parent.
 *
 * The panel manages 5 main ButtonSprite objects (resources 0x429..0x42F),
 * 9 layout-slot ButtonSprite objects (resources 0x43A..0x442), and a
 * background sprite (+0x238). It supports layout list display with
 * scrollable titles (linked list at +0xEC/+0xF0), a player grid, and
 * title text rendering. Default player count is 3.
 *
 * Inheritance:
 *   UI_WindowBase (0xE8 bytes, vtable 0x477C30)
 *     +-- GameSetupPanel (+0x178 bytes subclass data)
 *
 * Size: 0x260 bytes
 * Vtable: 0x4774D0 (VTBL_GAMESETUPPANEL)
 *
 * Vtable layout (extends UI_WindowBase 12-slot vtable):
 *   [0] +0x00: scalar deleting destructor (GameSetupPanel_Dtor, 0x408B00)
 *   [1] +0x04: Hide                        (inherited: UI_WindowBase_Hide, 0x425990)
 *   [2] +0x08: Show                        (inherited: UI_WindowBase_Show, 0x4259C0)
 *   [3] +0x0C: virtual (default stub)      (inherited: 0x425FD0)
 *   [4] +0x10: virtual (default stub)      (inherited: 0x426020)
 *   [5] +0x14: virtual (default stub)      (inherited: 0x426130)
 *   [6] +0x18: CreateFullWindow            (inherited: UI_CreateFullWindow, 0x425B70)
 *   [7] +0x1C: OnCreate                    (inherited: UI_WindowBase_OnCreate, 0x425D30)
 *   [8] +0x20: Render/Update               (overridden: GameSetupPanel_Render, 0x409280)
 *   [9] +0x24: virtual (default no-op)     (inherited: 0x4661A0)
 *   [10]+0x28: virtual (default stub)      (inherited: 0x426140)
 *   [11]+0x2C: WindowProc                  (inherited: UI_DefWndProc, 0x422EA0)
 *
 * NOTE: Address 0x4784CC has zero cross-references in the binary — it is a
 * Ghidra auto-label artifact, not a real vtable. The four functions below are
 * called via direct (UNCONDITIONAL_CALL) dispatch, not through virtual dispatch:
 *   HandleMapClick              (0x40ABA0, called from EditorState::HandleClick)
 *   SelectLayoutEntry           (0x40AAF0, called from 4 sites)
 *   SendScenarioSelect          (0x40AC50, called from 2 sites)
 *   ConnectToNetworkGame        (0x40AA20, called from 3 sites incl. loadLayouts)
 *
 * Called by: UI_MainMenu_Create @ 0x4205D6 (alloc 0x260, ctor, createWindow)
 */

// Status: TRANSCRIBED

#pragma once

#include "UI_WindowBase.h"
#include <cstdint>
#include "RenderSurface.h"
#include "LayoutListNode.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* Forward declaration */
class ButtonSprite;
struct AssetMgr;

class GameSetupPanel : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* ---- Inherited from UI_WindowBase (0x00..0xE7) ---- */
    /* (see UI_WindowBase.h for full layout) */
    /* ---- GameSetupPanel-specific fields (0xE8..0x25F) ---- */

    uint8_t    field_E8;               // +0xE8  (unknown byte, init 0, cleared by cleanup)

    /* Linked list of scenario/layout titles */
    LayoutListNode* titleList;         // +0xEC  Linked list of title entries (scenario names)
                                       //        Nodes: [0]=next, [2]=name_string

    /* Linked list of scenario/layout entries */
    LayoutListNode* layoutList;        // +0xF0  Linked list of layout entries (full data)

    int32_t    selectedEntry;          // +0xF4  Index of selected/highlighted list entry

    int32_t    field_F8;               // +0xF8  (unknown, init 0)
    int32_t    field_FC;               // +0xFC  (unknown, init 0)

    int32_t    lineHeight;             // +0x100  Font line height + 4 (init 0x10 = 16, then
                                       //          set to font height + 4 by drawLayoutList)

    int32_t    displayedCount;         // +0x104  Number of entries drawn in layout list

    LayoutListNode* currentList;       // +0x108  Pointer to the list currently being displayed
                                       //          (set by drawLayoutList to its list parameter)

    uint8_t    field_10C;              // +0x10C  (unknown byte, init 0, cleared by cleanup)
    uint8_t    _pad_10D[3];            // +0x10D  padding

    int32_t    field_110;              // +0x110  (unknown, init 0)

    uint8_t    titleDrawnFlag;         // +0x114  Set to 1 by drawTitle(), cleared by cleanup

    int32_t    timerId1;               // +0x118  First timer ID (killed by cleanup)
    int32_t    timerId2;               // +0x11C  Second timer ID (killed by cleanup)

    char       titleText[128];         // +0x120  Title text buffer (128 bytes, loaded from
                                       //          resource string by updateTitle)

    /* +0x1A0..+0x1AF: title drawing rect */

    RECT       titleDrawRect;          // +0x1A0  RECT for drawn title text position
                                       //          (calculated by drawTitle)

    int32_t    textAlignMode;          // +0x1B0  Title text alignment mode (init 3 = maxPlayers
                                       //          but reused! 0=right, 1=left, 2=bottom, else=default)

    HICON      hIcon;                  // +0x1B4  Window icon handle (loaded in create_window)

    uint8_t    renderFlag;             // +0x1B8  Render state flag (set to 1 by Render)
    uint8_t    _pad_1B9[3];            // +0x1B9  padding

    /* +0x1BC..+0x1CB: unknown gap (4 ints) */

    RECT       clipRect;               // +0x1CC  Source clip rect for UIPANEL_Blit
                                       //          (source coordinates on background sprite)

    RECT       layoutListRect;         // +0x1DC  Layout list bounding rectangle
    RECT       gridRect;               // +0x1EC  Player grid bounding rectangle
    RECT       titleRect;              // +0x1FC  Title text bounding rectangle

    /* +0x20C..+0x21B: unknown gap */

    uint8_t    spritesCreated;         // +0x21C  Non-zero = sprites have been allocated
    uint8_t    _pad_21D[3];            // +0x21D  padding

    /* Main UI ButtonSprites */
    ButtonSprite* titleSprite;          // +0x220  Title sprite (res 0x42A)
    ButtonSprite* field_224;            // +0x224  (res 0x42C)
    ButtonSprite* field_228;            // +0x228  (res 0x429)
    ButtonSprite* field_22C;            // +0x22C  (res 0x42B)
    ButtonSprite* field_230;            // +0x230  (res 0x42F)

    RenderSurface* field_234;          // +0x234  Offscreen render surface (released via
                                       //          Release() in cleanup and base_dtor)

    RenderSurface* backgroundSprite;   // +0x238  Background render surface (released via
                                       //          Release() in base_dtor only)

    /* Layout slot ButtonSprites (9 entries, res 0x43A..0x442) */
    ButtonSprite* layoutSprite[9];      // +0x23C..+0x25C

#ifndef _WIN32
    // SDL DirectPlay has no session provider. This records completion of the
    // original Search control's empty-session scan without changing x86 ABI.
    bool hostSearchCompleted = false;
    // GAMESTATE_HandleClick (0x40A548 et seq.) displays frame 1 and blocks
    // for 150 ms before completing each main lobby-control action.
    uint8_t hostPressedControl = 0;
    uint64_t hostPressedUntilMs = 0;
    // Index into the host-only layouts below. The original receives the same
    // slot count and dimensions in NETMAN_SyncGameState (0x43FC50).
    uint8_t hostLayoutIndex = 0;
#endif

    /* Total: 0x260 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * GameSetupPanel constructor.
     * Address: 0x408AA0
     *
     * Chains to UI_WindowBase(hInstance, resId), sets vtable to
     * VTBL_GAMESETUPPANEL (0x4774D0), then calls Init() to initialize
     * all subclass fields and create 14 ButtonSprite objects.
     *
     * Called by: UI_MainMenu_Create @ 0x4205D6 with hInstance + resId=0x1F9
     *
     * @param hInstance  Application instance handle
     * @param resId      Window resource ID (0x1F9 = 505)
     */
    GameSetupPanel(HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x408B00
     *
     * Calls BaseDtor to release all sprites, lists, and sound resources,
     * then optionally frees the heap allocation.
     *
     * @param flags  Delete flag (bit 0 = free heap memory)
     */
    virtual ~GameSetupPanel();

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Init — Initialize panel fields and create sprite objects.
     * Address: 0x408B20
     *
     * Zeroes all subclass fields, sets lineHeight to 0x10 and textAlignMode
     * to 3, creates 5 main ButtonSprite objects (res 0x429..0x42F), then
     * creates 9 layout-slot ButtonSprite objects (res 0x43A..0x442) in
     * a loop.
     *
     * Called by: constructor (0x408AA0 @ +0x3E)
     */
    void init();

    /**
     * Base destructor — Release all resources.
     * Address: 0x408D10
     *
     * Frees two linked lists (titleList at +0xEC and layoutList at +0xF0),
     * destroys all 14 ButtonSprite objects (releasing pixel data and
     * freeing memory), releases sound resource 0x5015, then calls the
     * inherited base destructor (UI_WindowBase::base_destructor).
     *
     * Called by: scalar deleting destructor (vtable[0])
     */
    void base_destructor();

    /**
     * CreateWindow — Create the full-screen panel window.
     * Address: 0x408F00
     *
     * Loads icon resource 0x65 from the instance handle, stores it at
     * hIcon (+0x1B4), then calls UI_CreateFullWindow to register and
     * create a full-screen window covering the entire desktop.
     *
     * @param hWndParent  Parent window HWND
     * @return            true on success, false on failure
     */
    bool create_window(HWND hWndParent);

    /**
     * Render — Full render pass for the game setup lobby. (vtable[8])
     * Address: 0x409280
     *
     * Blits the background sprite, updates title text, renders main
     * ButtonSprites, draws the layout list (scenario list), draws the
     * player grid, then ends the paint operation.
     *
     * @param unused   Unused 4-byte parameter (inherited virtual signature)
     */
    void render(int unused);

    /**
     * updateTitle — Update the title text from resource strings.
     * Address: 0x409360
     *
     * Reads network session state and resource strings to determine the
     * appropriate title text. If network mode is active, hides the title
     * sprite and uses the "network game" resource string. Otherwise,
     * selects between "select scenario" (0x6F) and "select layout" (0x70)
     * based on _g_netman->m_playerSlotCount (+0x08). Always
     * calls drawTitle() after updating the buffer.
     */
    void updateTitle();

    /**
     * drawLayoutList — Draw the scenario/layout list entries.
     * Address: 0x4094B0
     *
     * Blits the layout list region background, enumerates the linked list,
     * and draws each entry's name string. The selected entry
     * (selectedEntry, +0xF4) is highlighted with background color 0x2525DC;
     * unselected entries use text color 0xFF5C00. Empty list falls back to
     * resource string 0x7F ("No layouts available").
     *
     * @param list  The linked list to display (titleList or layoutList)
     */
    void drawLayoutList(LayoutListNode* list);

    /**
     * drawTitle — Draw the title text at the title area.
     * Address: 0x409770
     *
     * Restores the background, selects the title font (0x4855FC), draws
     * the title text (from +0x120 buffer) with DrawTextA, then applies
     * alignment offsets based on textAlignMode (+0x1B0):
     *   0 = right-align, 1 = left-align, 2 = bottom-align,
     *   default = center vertically.
     */
    void drawTitle();

    /**
     * drawGrid — Draw the player/scenario selection grid.
     * Address: 0x409980
     *
     * Iterates _g_netman fields (m_playerCols rows x m_playerRows columns) of
     * 0xA5x0x7B cells. Each cell: empty slot uses layout sprite state 1
     * (empty), occupied slot uses state 2 (filled) and renders the
     * player name, overflow (beyond maxPlayers) uses state 0 (hidden).
     * Sprites are positioned at grid cell coordinates calculated from
     * gridRect (+0x1EC).
     */
    void drawGrid();

    /**
     * cleanup — Clean up panel resources (non-virtual).
     * Address: 0x409DB0
     *
     * Resets internal flags, hides the window, destroys pixel data for
     * all 14 ButtonSprites (but does NOT free the objects), releases
     * field_234 via vtable[2], and kills both timers (+0x118, +0x11C).
     * Less destructive than base_destructor — does not free the
     * ButtonSprite objects or the title/layout lists.
     */
    void cleanup();

    /**
     * loadLayouts — Load layout/scenario index from disk.
     * Address: 0x409E70
     *
     * Clears existing titleList, builds path "install_dir\\Layouts\\index.lay",
     * attempts to load via AssetMgr first, then falls back to direct file
     * open. Parses the file content (0x2000-byte buffer) line-by-line into
     * 0x0C-byte linked-list nodes with 0x100-byte name strings. If
     * connectToNetwork is non-zero, resets selectedEntry to 0 and calls
     * ConnectToNetworkGame().
     *
     * @param connectToNetwork  If non-zero, connect to network game after loading
     */
    void loadLayouts(bool connectToNetwork);

    /* ================================================================ */
    /* Direct-call game-state methods (NOT virtual — called via direct    */
    /* UNCONDITIONAL_CALL in the binary, not through any vtable)         */
    /* ================================================================ */

    /**
     * HandleMapClick — Handle click on the scenario selection grid (3x3 grid).
     * Address: 0x40ABA0
     * Called from: GAMESTATE_HandleClick @ 0x40AA0E (direct call)
     *
     * @param clickX  mouse X in grid-local coordinates
     * @param clickY  mouse Y in grid-local coordinates
     */
    void HandleMapClick(int32_t clickX, int32_t clickY);

    /**
     * SelectLayoutEntry — Select a single-player layout by index from the linked list.
     * Address: 0x40AAF0
     * Called from: GAMESTATE_SelectLayout, GAMESTATE_StartGameTimer,
     *              GAMESTATE_HandleClick (direct calls)
     *
     * @param index  zero-based index into layout linked list
     */
    void SelectLayoutEntry(int32_t index);

    /**
     * SendScenarioSelect — Send the selected scenario/layout choice to peers or local game.
     * Address: 0x40AC50
     * Called from: GAMESTATE_HandleNetworkGame, GAMESTATE_HandleMapClick (direct calls)
     *
     * @param scenarioIndex  scenario index or context flag
     */
    void SendScenarioSelect(int32_t scenarioIndex);

    /**
     * ConnectToNetworkGame — Select a network session and join it.
     * Address: 0x40AA20
     * Called from: GameSetupPanel::loadLayouts, GAMESTATE_HandleClick (direct calls)
     *
     * @param index  session index to join
     */
    void ConnectToNetworkGame(int32_t index);

#ifndef _WIN32
    /**
     * Host SDL frame composition for the game-setup lobby.
     *
     * Assembly basis: GameSetupPanel::render (0x409280) blits a background
     * sprite, updates title text, draws the layout list, and draws the
     * player grid.  The host composes equivalent operations onto the SDL
     * primary canvas.  This method is excluded from the original Win32 build.
     */
    void hostRenderFrame();

    /**
     * Host SDL pointer adapter for GAMESTATE_HandleClick (0x40A4E0).
     * It maps display coordinates through the primary-canvas projection and
     * dispatches the controls currently rendered by hostRenderFrame().
     */
    void hostHandlePointer(float display_x, float display_y, bool pressed);
#endif
};
