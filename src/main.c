/*
 * Lego Loco (1998) - Native Linux Port
 * src/main.c — Linux entry point replacing Win32 entry() + WinMain()
 *
 * Original: loco.exe entry at 0x004689e0 (CRT stub) -> WinMain at 0x00462e90
 * Developer: Intelligent Games for LEGO Media
 *
 * This file replaces the Windows-specific startup chain:
 *   entry() [0x4689e0] -> WinMain() [0x462e90]
 * with a standard POSIX main() and SDL2 initialization.
 *
 * Key Win32 -> Linux replacements here:
 *   GetCommandLineA()      -> argc/argv
 *   GetModuleHandleA(NULL) -> not needed (SDL handles window creation)
 *   GetStartupInfoA()      -> not needed
 *   CoInitializeEx()       -> removed (no COM on Linux)
 *   CoUninitialize()       -> removed
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/core.h"
#include "resources/resources.h"
#include "graphics/ddraw_init.h"
#include "audio/audio.h"
#include "platform/sdl_window.h"

/* Game data directory — searched in priority order:
 *   1. $LEGO_LOCO_DATA environment variable
 *   2. ~/.config/lego-loco/
 *   3. /usr/share/lego-loco/
 *   4. ./data/  (development layout)
 */
static const char *g_dataDirs[] = {
    NULL,                       /* $LEGO_LOCO_DATA (filled at runtime) */
    NULL,                       /* ~/.config/lego-loco/ (filled at runtime) */
    "/usr/share/lego-loco/",
    "./data/",
    NULL
};

static char g_envDataDir[512];
static char g_xdgDataDir[512];

static const char *FindDataDir(void) {
    const char *env = getenv("LEGO_LOCO_DATA");
    if (env) {
        snprintf(g_envDataDir, sizeof(g_envDataDir), "%s/", env);
        g_dataDirs[0] = g_envDataDir;
    }

    const char *home = getenv("HOME");
    if (home) {
        snprintf(g_xdgDataDir, sizeof(g_xdgDataDir), "%s/.config/lego-loco/", home);
        g_dataDirs[1] = g_xdgDataDir;
    }

    for (int i = 0; g_dataDirs[i]; i++) {
        char iniPath[600];
        snprintf(iniPath, sizeof(iniPath), "%slego.ini", g_dataDirs[i]);
        FILE *f = fopen(iniPath, "r");
        if (f) {
            fclose(f);
            return g_dataDirs[i];
        }
        /* Also try Exe/LEGO.INI (original game tree layout) */
        snprintf(iniPath, sizeof(iniPath), "%sExe/LEGO.INI", g_dataDirs[i]);
        f = fopen(iniPath, "r");
        if (f) {
            fclose(f);
            return g_dataDirs[i];
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    /* SDL2 init — replaces Win32 CRT startup initialization */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Locate game data directory */
    const char *dataDir = FindDataDir();
    if (!dataDir) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LEGO LOCO",
            "Game data not found.\n"
            "Set LEGO_LOCO_DATA to the directory containing lego.ini,\n"
            "or place lego.ini in ~/.config/lego-loco/", NULL);
        SDL_Quit();
        return 1;
    }

    fprintf(stderr, "Data directory: %s\n", dataDir);

    /* Initialize engine — mirrors CGWND_Constructor (0x4061e0) */
    CGWND *game = CGWND_Create(dataDir);
    if (!game) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LEGO LOCO",
            "Failed to initialize game engine.", NULL);
        SDL_Quit();
        return 1;
    }

    /* Load configuration from lego.ini — mirrors CGWND_LoadConfig (0x4068d0) */
    if (!CGWND_LoadConfig(game)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LEGO LOCO",
            "Failed to load configuration (lego.ini missing or corrupt).", NULL);
        CGWND_Destroy(game);
        SDL_Quit();
        return 1;
    }

    /* Create SDL2 window — replaces RegisterClassA + CreateWindowExA (0x406680) */
    if (!Platform_CreateWindow(game, "LEGO LOCO", 640, 480)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LEGO LOCO",
            "Failed to create window.", NULL);
        CGWND_Destroy(game);
        SDL_Quit();
        return 1;
    }

    /* Initialize graphics, audio, input — mirrors GameLoop_Setup (0x406ba0) */
    if (!CGWND_SetupSubsystems(game)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LEGO LOCO",
            "Failed to initialize subsystems.", NULL);
        Platform_DestroyWindow(game);
        CGWND_Destroy(game);
        SDL_Quit();
        return 1;
    }

    /* Main game loop — replaces the PeekMessage loop in WinMain (0x462e90)
     *
     * Original: MsgWaitForMultipleObjects(0, NULL, 0, 3, QS_ALLINPUT) + multimedia timer
     * Linux:    SDL_AddTimer(28ms, FrameTimerCallback) + SDL_PollEvent
     */
    int exitCode = CGWND_RunLoop(game);

    /* Shutdown */
    CGWND_Shutdown(game);
    Platform_DestroyWindow(game);
    CGWND_Destroy(game);
    SDL_Quit();

    return exitCode;
}
