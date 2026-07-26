/*
 * sdl3_game.h — SDL3 game initialization and loop
 *
 * Provides the main game loop that drives the Lego Loco engine.
 * This replaces the original WinMain() + message loop and the
 * SDL2 port's core.c game loop.
 */

#ifndef LOCO_SDL3_GAME_H
#define LOCO_SDL3_GAME_H

#include "sdl3_types.h"

/* Forward declarations for config/resources */
typedef struct INI_File INI_File;
typedef struct ResMgr   ResMgr;

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Game lifecycle
 * ========================================================================= */

/**
 * Game_Init — Initialize the game engine.
 *
 * Sets up the CGWND struct, initializes subsystems, loads config.
 *
 * @param window     SDL3 window (from SDL3_Backend_Init).
 * @param data_dir   Path to game data directory (containing lego.ini).
 * @param ini        Parsed INI config (may be NULL).
 * @return           0 on success, -1 on failure.
 */
int  Game_Init(SDL_Window *window, const char *data_dir, INI_File *ini);

/**
 * Game_GetResMgr — Get the resource manager (opened during Game_Init).
 */
ResMgr *Game_GetResMgr(void);

/**
 * Game_Run — Execute the main game loop.
 *
 * Runs until g_game.running becomes false (set by quit event or
 * game state transition to GAME_STATE_QUIT).
 */
void Game_Run(void);

/**
 * Game_Shutdown — Tear down all subsystems.
 *
 * Frees resources, shuts down audio, destroys surfaces.
 */
void Game_Shutdown(void);

/**
 * Game_ProcessEvents — Process SDL3 events for one frame.
 *
 * Called once per frame from the game loop.
 * Returns 1 if a quit event was received, 0 otherwise.
 */
int  Game_ProcessEvents(void);

/**
 * Game_Update — Update game logic for one frame.
 *
 * Called once per frame. Handles input polling, game state
 * transitions, entity updates, and animations.
 */
void Game_Update(void);

/**
 * Game_Render — Render one frame.
 *
 * Clears the backbuffer, draws the current scene (menu, world, UI),
 * then presents to the display.
 */
void Game_Render(void);

#ifdef __cplusplus
}
#endif

#endif /* LOCO_SDL3_GAME_H */
