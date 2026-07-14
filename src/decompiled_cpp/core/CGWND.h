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
 */

#pragma once

#include "../shared/types.h"

class CGWND {
public:
    /* ================================================================ */
    /* Core fields                                                       */
    /* ================================================================ */
    void*      vtable;          // +0x00  vtable → 0x4774C4
    HWND       hWnd;            // +0x04  desktop window handle
    int32_t    field_08;        // +0x08  (unknown, init to 0)
    HINSTANCE  hInstance;       // +0x0C  application instance handle
    uint8_t    field_10;        // +0x10  byte flag (init to 0)
    uint8_t    minVehicleFPS;   // +0x11  min FPS for vehicles (default 20)
    uint8_t    minBuildingFPS;  // +0x12  min FPS for buildings (default 18)
    uint8_t    minMinifigFPS;   // +0x13  min FPS for minifigs (default 16)
    uint8_t    minFlyingFPS;    // +0x14  min FPS for flying objects (default 14)
    uint8_t    _pad_15[3];      // +0x15
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
    void* scalar_deleting_destructor(byte flags);

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
     * Read install path from registry and load lego.ini.
     * Address: 0x4068D0
     *
     * Reads HKLM\SOFTWARE\Intelligent Games\LEGO Loco registry key,
     * loads lego.ini configuration, creates data directory if missing.
     */
    void InstallPathInit();

    /**
     * Set display mode / toggle pause state.
     * Address: 0x4061B0
     *
     * Toggles active/paused state on the UI window.
     */
    void SetPause();

    /**
     * Set video mode (fullscreen/windowed).
     * Address: (in cgwnd_setmode.c area)
     */
    void SetMode();

    /**
     * Clean up and release all subsystems.
     * Address: (in cgwnd_cleanup.c area)
     */
    void Cleanup();

    /**
     * Initialize all game subsystems (audio, video, input, network).
     * Address: (in cgwnd_initallsubsystems.c area)
     */
    void InitAllSubsystems();

    /**
     * Initialize mode 1 (game mode) subsystems.
     * Address: (in cgwnd_initmode1.c area)
     */
    void InitMode1();
};

/* ================================================================ */
/* Free functions in the core address range (0x406000-0x413fff)     */
/* ================================================================ */

/**
 * Parse command-line for demo mode flags and easter egg themes.
 * Address: 0x406790
 *
 * Tokenizes lpCmdLine by spaces. Tests each token against three
 * blacklisted demo sentinels ("/s", "-s", "s"). Tokens that pass
 * are checked for seasonal theme keywords:
 *   "Easter"=1, "Desert"=2, "Halloween"=3, "Winter"=4, "/XMas"=5
 */
void CGWND_ParseCmdLine(char* lpCmdLine);
