// Status: TRANSCRIBED
/**
 * EditWindow.h — Full-screen main menu dialog controller (UI_MainMenu)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * EditWindow is the full-screen dialog controller for the main menu. It
 * manages a 7-state state machine (hidden/loading/check-config/singleplayer/
 * network/start-game/return-from-game), loads 12 main-menu button sprites,
 * creates PanelA (NameEntryPanel network panel) and PanelB (GameSetupPanel
 * city-selection panel) as child panels, and provides the player-name edit
 * control.
 *
 * Size: ~0x224 bytes (+0xE8 from UI_WindowBase = total ~0x224)
 * Vtable: 0x4779F8 (VTBL_EDITWINDOW)
 *
 * Class hierarchy:
 *   UI_WindowBase (vtable 0x477C30, size 0xE8)
 *     └─ EditWindow  ← this class (total ~0x224 bytes)
 *
 * Vtable layout (0x4779F8):
 *   [0]  +0x00: scalar deleting destructor     (0x4203A0)
 *   [1]  +0x04: Hide                           (0x420860)
 *   [2]  +0x08: Show                           (0x4206B0)
 *   [3]  +0x0C: SetMode                        (0x425FD0, inherited)
 *   [4]  +0x10: SetRenderSurface               (0x426020, inherited)
 *   [5]  +0x14: OnAsyncTaskFailure             (0x426130, inherited no-op)
 *   [6]  +0x18: CreateFullWindow               (inherited: UI_CreateFullWindow)
 *   [7]  +0x1C: OnCreate/HandleClick           (0x421200, overridden)
 *   [8]  +0x20: Render/Update                  (0x422AA0, overridden)
 *   [9]  +0x24: MouseWheel                     (0x422950, overridden)
 *   [10] +0x28: virtual method                 (0x426140, inherited stub)
 *   [11] +0x2C: WindowProc                     (EditWindow_DispatchClick, 0x422940, overridden)
 *          Note: The vtable slot points to 0x422600 but this is inside
 *          EditWindow_updateButton. The actual WindowProc entry is at
 *          0x422940 (gap between InitNetworkPanel and netPanelWndProc).
 *          This ~1110-byte function handles all main-menu button clicks
 *          including accept (plays 0x5015) and quit (no click sound).
 *   [12]+[19]: EditWindow-specific virtuals
 *   [20] +0x50: netPanelWndProc                (0x422D80)
 *
 * Button sound behavior (verified against assembly at 0x422900-0x422D80):
 *   Accept/Play (+0x13C):  PlaySound(0x5015) → Sleep(0x96) → commit name
 *   Quit/Exit  (+0x14C):   NO click sound → Sleep(0x96) → CGWND_SetMode(10) → 0x5026
 *   Selection toggles:     PlaySound(0x5015) for each toggle (no Sleep)
 *   Preload at show():     RESMGR_LoadSoundResource(0x5015) — warms cache only
 *
 * Dialog states:
 *   0 = initial (hidden)
 *   1 = hidden (fully deinitialized)
 *   2 = loading (transition -- hide PanelA, show PanelB)
 *   3 = check-config (singleplayer vs network)
 *   4 = singleplayer (PanelB visible)
 *   5 = network (PanelB visible)
 *   6 = start-game (launch gameplay)
 *   7 = return-from-game (re-entry)
 */

#pragma once

#include "../shared/types.h"
#include "UI_WindowBase.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

struct UIPANEL_Surface;
class NameEntryPanel;
class GameSetupPanel;
/** Popup window ABI fragment: virtual destructor at +0x00, HWND at +0x04 in
 * the original x86 object. The compiler performs the observed virtual delete. */
class PopupWindow {
public:
    virtual ~PopupWindow() = default;
    HWND hWnd;
};
namespace loco::assets {
class SpriteResource;
class SpriteBitmap;
}

/* ================================================================== */
/* MenuSpriteSlot -- host-side typed sprite pair                       */
/* ================================================================== */
struct MenuSpriteSlot {
    loco::assets::SpriteResource* resource;
    loco::assets::SpriteBitmap*   bitmap;
};

/* ================================================================== */
/* EditWindow class                                                     */
/* ================================================================== */

class EditWindow : public UI_WindowBase {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /*                                                                     */
    /* Fields +0x00 through +0xE4 are inherited from UI_WindowBase.      */
    /* ================================================================ */

    /* --- Inherited from UI_WindowBase (+0x00..+0xE7) --- */
    /* See UI_WindowBase.h for full inherited field layout.               */

    /* NOTE: Fields +0x14, +0x60, +0x64, +0x68, +0x6C from UI_WindowBase
       are REUSED by EditWindow for sprite selection IDs:
       +0x14: pInitGuard -- guard pointer used by netPanelWndProc
       +0x60: normalSurfaceAddress (reuses childCount0)
       +0x64: normalAnimationMetadata (reuses childObj0)
       +0x68: highlightedSurfaceAddress (reuses childCount1)
       +0x6C: highlightedAnimationMetadata (reuses childObj1)
       These are passed to UI_WindowBase::set_mode() by netPanelWndProc. */

    /* --- EditWindow-specific fields (+0xE8..~0x224) --- */

    /* State machine */
    int32_t     dialogState;        /* +0xE8  current state (0-7)          */
    int32_t     previousState;      /* +0xEC  previous state before transition */
    int32_t     field_F0;           /* +0xF0  (unused, zeroed by ctor)     */
    uint8_t     hasPopup;           /* +0xF4  1 = popup window active      */
    uint8_t     _pad_F5[3];         /* +0xF5                                */
    HICON       icon;               /* +0xF8  app icon handle              */

    /* Button hit-test RECTs (computed by HandleClick, consumed by netPanelWndProc) */
    RECT        btnPlayRect;        /* +0xFC  single-player button rect    */
    RECT        btnScenarioRect;    /* +0x10C  multiplayer button rect      */
    RECT        btnExitRect;        /* +0x11C  Exit button rect            */
    RECT        btnTextRect;        /* +0x12C  Text/action button rect     */
    RECT        btnOption1Rect;     /* +0x13C  Option button 1 rect        */
    RECT        btnOption2Rect;     /* +0x14C  Quit button rect (0x422AC3) */

    /* Player-name edit control layout */
    RECT        editBoxRect;        /* +0x15C  edit control RECT           */

    /* Centering offsets */
    int32_t     centerOffsetX;      /* +0x16C  (surface_w - screen_w) / 2 */
    int32_t     centerOffsetY;      /* +0x170  (surface_h - screen_h) / 2 */
    int32_t     surfaceRight;       /* +0x174  screen_w + centerOffsetX    */
    int32_t     surfaceBottom;      /* +0x178  screen_h + centerOffsetY    */

    /* Backdrop area */
    RECT        backdropRect;       /* +0x17C  (300, 0xAC, 0x3D4, 0x354)  */

    uint8_t     spritesLoaded;      /* +0x18C  1 = sprites loaded          */

    /* Sprite slots: each pair is 8 bytes (pResource + hBitmap)            */
    /* Group at +0x190: resources 0x407, 0x408, 0x409 (partially overlaps  */
    /*   with sprites[0..2]) */
    MenuSpriteSlot sprite_407;      /* +0x190  resource 0x407 (sprites[0]) */
    MenuSpriteSlot sprite_408;      /* +0x198  resource 0x408 (sprites[1]) */
    MenuSpriteSlot sprite_409;      /* +0x1A0  resource 0x409 (sprites[2]) */

    MenuSpriteSlot sprite_40A;      /* +0x1A8  resource 0x40A              */

    /* Group at +0x1B0: resources 0x403, 0x404, 0x405, 0x406               */
    MenuSpriteSlot sprite_403;      /* +0x1B0  resource 0x403              */
    MenuSpriteSlot sprite_404;      /* +0x1B8  resource 0x404              */
    MenuSpriteSlot sprite_405;      /* +0x1C0  resource 0x405              */
    MenuSpriteSlot sprite_406;      /* +0x1C8  resource 0x406              */

    /* Group at +0x1D0: resources 0x40B, 0x40C, 0x40E, 0x40F              */
    MenuSpriteSlot sprite_40B;      /* +0x1D0  resource 0x40B              */
    MenuSpriteSlot sprite_40C;      /* +0x1D8  resource 0x40C              */
    MenuSpriteSlot sprite_40E;      /* +0x1E0  resource 0x40E              */
    MenuSpriteSlot sprite_40F;      /* +0x1E8  resource 0x40F              */

    UIPANEL_Surface* pMainSurface;  /* +0x1F0  offscreen surface           */

    HBRUSH      hbrSolid;           /* +0x204  solid brush (0x5252E7)      */
    HBRUSH      hbrHatch;           /* +0x208  hatch brush (5, 0x0A5C0A)   */

    HWND        hwndEdit;           /* +0x20C  player-name edit control    */
    PopupWindow* pPopupWindow;      /* +0x210  popup window                */
    LONG        prevEditWndProc;    /* +0x214  saved edit WNDPROC          */
    LONG        savedPopupWndProc;  /* +0x218  saved popup WNDPROC         */
    NameEntryPanel* pPanelA;            /* +0x21C  NameEntryPanel              */
    GameSetupPanel* pPanelB;            /* +0x220  GameSetupPanel              */

#ifndef _WIN32
    // Host-only state for the SDL composition/input adapter; excluded from
    // the original Windows object layout.
    int hostHoveredButton = -1;
    // Option controls at 0x42298A/0x422AC3 retain their pushed artwork for
    // Sleep(0x96) before accepting the name or exiting.
    int hostPressedButton = -1;
    uint64_t hostPressedUntilMs = 0;
    // The original EDIT control uses EM_LIMITTEXT(11) at 0x420A56.
    char hostEditText[12] = {};
    bool hostEditFocused = true;
#endif

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    EditWindow(HINSTANCE hInstance, UINT resourceId);   /* 0x4202F0 */
    virtual ~EditWindow();        /* 0x4203A0 */
    void base_destructor();                              /* 0x4203C0 */

    /* ================================================================ */
    /* Public Methods                                                    */
    /* ================================================================ */

    int  create(HWND hWndParent);                        /* 0x4204D0 */
    void show();                                         /* 0x4206B0 */
    void hide();                                         /* 0x420860 */
    void setState(int32_t state);                        /* 0x4208F0 */
    void HandleClick();                                  /* 0x421200 */
    int  netPanelWndProc(HWND hwnd, UINT msg,            /* 0x422D80 */
                         WPARAM wParam, LPARAM lParam);
    void onPlayerNameChanged();                          /* 0x422660 */

#ifndef _WIN32
    // Host-only presentation/input boundary. These methods preserve the
    // original 1280x1024 coordinates but never alter the Win32/x86 object ABI.
    void hostRenderFrame();
    void hostHandlePointer(float display_x, float display_y, bool pressed);
    bool hostHandleKey(int32_t key_code);
    void hostHandleTextInput(const char* utf8_text);
#endif

private:
#ifndef _WIN32
    // Host equivalent of the direct onPlayerNameChanged call at 0x422AB2.
    // Shared by Enter and the resource-0x403 accept control.
    void hostCommitPlayerName();
#endif
    void netPanelInit();                                 /* 0x422820 */
    void initSprites();                                  /* 0x421500 */
    void cleanupSprites();                               /* 0x421AE0 */
    void render();                                       /* 0x4216F0 */
    void drawButtons();                                  /* 0x422010 */
    void drawText(RECT* rect, int charIndex,             /* 0x422440 */
                  loco::assets::SpriteResource* fontResource,
                  loco::assets::SpriteBitmap* fontBitmap);
    void updateButton(RECT* rect);                       /* 0x422570 */
};

/* ================================================================== */
/* Global pointer -- set by EditWindow constructor                      */
/* ================================================================== */
extern EditWindow* g_editwindow_ptr;  /* 0x485240 (g_ui_main_ptr) */
