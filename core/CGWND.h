/**
 * CGWND.h — Main game window class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * CGWND is the central application object — owning the game window,
 * managing display settings, holding version info, and coordinating
 * subsystem initialization. It's the first object constructed in WinMain.
 *
 * Size: 0x28 bytes (40 bytes)
 * Vtable: 0x4774C4
 *
 * Vtable layout:
 *   [0] +0x00: scalar deleting destructor (0x4062A0)
 *
 * Class hierarchy:
 *   (standalone — no base class)
 *
 * Closely associated free functions are declared after the class.
 */

#pragma once

#include "../shared/types.h"


// Status: TRANSCRIBED
/* ================================================================== */
/* CGWND — Main game window manager                                    */
/* ================================================================== */

class CGWND {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */
/* vtable at +0x00 is compiler-managed */
    HWND       hWndDesktop;     // +0x04  desktop window handle (GetDesktopWindow)
    HWND       hWnd;            // +0x08  main game window (from CreateWindowEx)
    HINSTANCE  hInstance;       // +0x0C  application instance handle
    uint8_t    field_10;        // +0x10  byte flag (init to 0; used as quit-to-menu sentinel)
    uint8_t    minVehicleFPS;   // +0x11  min FPS for vehicles (default 20)
    uint8_t    minBuildingFPS;  // +0x12  min FPS for buildings (default 18)
    uint8_t    minMinifigFPS;   // +0x13  min FPS for minifigs (default 16)
    uint8_t    minFlyingFPS;    // +0x14  min FPS for flying objects (default 14)
    uint8_t    _pad_15[3];      // +0x15  padding
    int32_t    versionMajor;    // +0x18  EXE major version
    int32_t    versionMinor;    // +0x1C  EXE minor version
    int32_t    versionBuild;    // +0x20  EXE build number
    int32_t    versionRevision; // +0x24  EXE revision number

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * CGWND constructor.
     * Address: 0x4061E0
     *
     * Initializes the game window object: sets vtable, stores HINSTANCE,
     * gets desktop HWND, resets global mode flags, clears display state,
     * and calls ResetState() to read EXE version info.
     */
    CGWND(HINSTANCE hInstance);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x4062A0
     *
     * Restores vtable, releases g_config_ini if active, frees memory.
     */
    virtual ~CGWND();

    /* ================================================================ */
    /* Display and configuration                                         */
    /* ================================================================ */

    /**
     * Initialize display settings for main menu.
     * Address: 0x406480
     *
     * Queries screen dimensions, reads window position from INI,
     * clamps to visible area, reads per-type FPS balancing limits,
     * resets CleanExit flag. Called early in WinMain.
     */
    void ShowMainMenu();

    /**
     * Display validation gate before game start.
     * Address: 0x406680
     *
     * Checks color depth (must be 8-bit paletted), mouse presence,
     * and screen width (800-1280). Returns 1 if checks pass,
     * shows error message box if they fail.
     */
    int InitGame();

    /**
     * Reset state / read EXE version info.
     * Address: 0x4062E0
     *
     * Reads loco.exe VERSIONINFO resource, parses
     * "major.minor.patch.build" via strtok("."),
     * stores four ints at versionMajor..versionRevision.
     * Called by constructor.
     */
    void ResetState();

    /**
     * Register the "LEGO_LOCO" window class and create the main window.
     * Address: 0x406ED0
     *
     * Registers WNDCLASS (style=0xB, WndProc=0x4618C0, icon ID=101),
     * then creates main popup window sized to g_screen_width/g_screen_height.
     * Stores the new HWND in this->hWnd (+0x08) and queries client rect
     * into the client RECT at 0x485220.
     *
     * Called by GameLoop_Setup (0x406D80).
     *
     * @return TRUE on success, FALSE on failure.
     */
    BOOL RegisterWindowClass();

        /**
     * InitAllSubsystems — Master subsystem initialization.
     * Address: 0x406F90
     *
     * @return 0 on success; -2 through -17 identify the failed stage
     */
    int InitAllSubsystems();

    /**
     * Initialize mode 1 — loading screen / world selection state machine.
     * Address: 0x408350
     *
     * Two code paths: first-time loading screen with incremental subsystem
     * init (field_10 == 0), or return-to-menu world loading (field_10 != 0).
     * Transitions to mode 3 (gameplay) on completion.
     */
    void initMode1();

};

/* ================================================================== */
/* Free functions in the core address range (0x406000-0x413fff)     */
/* ================================================================== */

/**
 * CGWND_QuitToMenu — Shut down active game session and return to menu.
 * Address: 0x406E80
 *
 * Shuts down netman, sets quit flag at g_main_window+0x10, updates audio,
 * unlocks sprites, clears world state, sets game mode to 2 (main menu UI).
 *
 * Called by: NETMAN_ProcessMessage (0x43F498), winmain loop (0x462B2D)
 */
void CGWND_QuitToMenu();

/**
 * CGWND_SetFullscreenMode — Switch between fullscreen and windowed mode.
 * Address: 0x407D20
 *
 * go_windowed = 0 -> fullscreen, go_windowed = 1 -> windowed.
 * Invalidates viewport, changes window style, repositions window,
 * manages scrollbars. Demo mode uses HWND_TOPMOST.
 */
void CGWND_SetFullscreenMode(char go_windowed);

/**
 * CGWND_ToggleFullscreen — Toggle between fullscreen and windowed.
 * Address: 0x407D00
 *
 * Reads g_is_fullscreen and calls SetFullscreenMode with opposite value.
 */
void CGWND_ToggleFullscreen();

/**
 * CGWND_ScrollHorizontal — Scroll viewport horizontally by pixel delta.
 * Address: 0x407AE0
 *
 * Clamps scroll within [0, (clientWidth-offsetX)+worldWidth],
 * invalidates tilemap dirty rect, updates Windows scrollbar.
 */
void CGWND_ScrollHorizontal(int scroll_delta);

/**
 * CGWND_ScrollVertical — Scroll viewport vertically by pixel delta.
 * Address: 0x407BF0
 *
 * Clamps scroll within [0, (clientHeight-offsetY)+worldHeight],
 * invalidates tilemap dirty rect, updates Windows scrollbar.
 */
void CGWND_ScrollVertical(int scroll_delta);

/**
 * CGWND_PumpMessages — Selective message pump during loading.
 * Address: 0x4085E0
 *
 * When filter=true, discards mouse events (WM_MOUSEMOVE, WM_SETCURSOR,
 * mouse clicks) to prevent input during loading. When filter=false,
 * translates and dispatches normally.
 */
void CGWND_PumpMessages(char filter);

/**
 * CGWND_EnterMode3 — Transition handler for entering game mode 3 (town/gameplay).
 * Address: 0x4086F0
 *
 * Receives the PREVIOUS mode and cleans up/transitions based on what's
 * being left. Case 2 (main menu) cancels. Cases 5/6/7/9 hide their
 * overlays. All paths fall through to common cleanup: reset buildings,
 * tooltips, world, build mode, cursor, and audio.
 */
void CGWND_EnterMode3(int old_mode);

/**
 * CGWND_SetBuildMode — Set build/placement mode.
 * Address: 0x4089D0
 *
 * 0=off, 1=road, 2=object. Updates g_build_mode, g_road_build_mode,
 * and g_placement_resource_id.
 */
void CGWND_SetBuildMode(int mode);

/**
 * CGWND_ReadRegistryString — Read REG_SZ from HKLM\Software\subKey.
 * Address: 0x408A30
 *
 * Opens HKLM\subKey\path, reads named value into buffer (max size).
 * buffer[0]='\0' and returns 0 on failure. Used by RESMGR.
 *
 * @param subKey   Registry subkey path
 * @param buffer   Output buffer for string value
 * @param bufSize  Buffer size in bytes
 * @param valueName Value name to read
 * @return 1 on success, 0 on failure
 */
int CGWND_ReadRegistryString(const char* subKey, uint8_t* buffer, uint32_t bufSize, const char* valueName);

/**
 * CGWND_ParseCmdLine — Parse command line for demo mode flags and easter egg themes.
 * Address: 0x406790
 *
 * Tokenizes lpCmdLine by spaces. Tests each token against three
 * blacklisted demo sentinels ("/s", "-s", "s"). Tokens that pass
 * are checked for seasonal theme keywords:
 *   "Easter"=1, "Desert"=2, "Halloween"=3, "Winter"=4, "/XMas"=5
 */
void CGWND_ParseCmdLine(char* lpCmdLine);

/**
 * CGWND_Present — Post WM_USER+7 to signal frame present. 0x45E1E0.
 *
 * Sends a custom message (0x407) to the main game window to signal a
 * completed frame for screen flip/blit. Called from the WinMain message
 * loop after each frame update.
 */
extern "C" void CGWND_Present(uint flags); /* original __cdecl 0x45E1E0 */

/**
 * CGWND_MapResourceToDirection — Map resource ID to direction enum. 0x40EB60.
 *
 * Maps track resource IDs (0x1804-0x1871 ranges) to direction values (1-4)
 * for the route editor. Returns 0 for unknown resources.
 */
int CGWND_MapResourceToDirection(int resourceId);

/**
 * CGWND_ValidatePaletteData — Load and validate palette data. 0x40E950.
 *
 * Loads palette entries from a PAL palette file into two internal buffers
 * at obj+0x168 and obj+0x488 (each 800 bytes = 200 int32). Reads 160 entries,
 * each with 4 RGB[A] values (one per line). Tries AssetManager first, falls
 * back to direct file open. SEH-protected.
 *
 * Called by: display/DDraw initialization path (0x40E936)
 *
 * @param obj  Pointer to object (likely a display/output struct) with palette
 *             buffers at +0x168 and +0x488
 * @return     1 on success, 0 on failure (EOF/read error)
 */
int __fastcall CGWND_ValidatePaletteData(void* obj);

/** Registry/INI path initialization. Address: 0x4068D0 (free function). */
int CGWND_InstallPathInit();

/* 0x4061B0 belongs to a ScriptedObject vtable, not to CGWND. */
void __thiscall CGWND_SetPause(void* self, uint8_t paused);

/** Core mode state machine. Address: 0x408130 (free function). */
void CGWND_SetMode(int new_mode);

/** Full shutdown sequence. Address: 0x4077A0 (free function). */
void CGWND_Cleanup();

/* AboutDialog is a separate GameWindow-derived class. Its canonical layout
 * and 0x40F1C0-0x410504 methods live in ../ui/AboutDialog.h/.cpp. */
