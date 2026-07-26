/*
 * sdl3_menu.h — Main menu system
 *
 * Replaces the original game's menu screens.
 * Supports: title screen, main menu, options, credits.
 */

#ifndef LOCO_SDL3_MENU_H
#define LOCO_SDL3_MENU_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum menu buttons. */
#define MENU_MAX_BUTTONS  8

/** One menu button. */
typedef struct {
    const char *label;         /* Button text                    */
    SDL_FRect   rect;          /* Bounding rectangle             */
    bool        hovered;       /* Mouse is over this button      */
    int         action;        /* Action ID to trigger on click  */
} MenuButton;

/** Menu screen state. */
typedef enum {
    MENU_NONE       = 0,
    MENU_TITLE,           /* Splash / title screen              */
    MENU_MAIN,            /* Main menu: New/Load/Options/Quit   */
    MENU_OPTIONS,         /* Options screen                     */
    MENU_CREDITS,         /* Credits / about                    */
} MenuScreen;

/** Menu action IDs. */
enum {
    MENU_ACT_NONE        = 0,
    MENU_ACT_NEW_GAME    = 1,
    MENU_ACT_LOAD_GAME   = 2,
    MENU_ACT_OPTIONS     = 3,
    MENU_ACT_CREDITS     = 4,
    MENU_ACT_QUIT        = 5,
    MENU_ACT_BACK        = 6,
    MENU_ACT_START       = 7,
};

/** Menu manager. */
typedef struct {
    MenuScreen   screen;                  /* Current screen */
    MenuButton   buttons[MENU_MAX_BUTTONS];
    int          button_count;
    float        anim_timer;              /* Animation timer */
    int          selected_action;         /* Action taken this frame */
} MenuMgr;

/**
 * Menu_Init — Initialize the menu system.
 */
void Menu_Init(MenuMgr *menu);

/**
 * Menu_Show — Switch to a menu screen.
 */
void Menu_Show(MenuMgr *menu, MenuScreen screen);

/**
 * Menu_HandleEvent — Process an SDL event for menu interaction.
 *
 * @return  Action ID if a button was clicked, MENU_ACT_NONE otherwise.
 */
int  Menu_HandleEvent(MenuMgr *menu, const SDL_Event *event);

/**
 * Menu_Update — Update menu animation (call each frame).
 */
void Menu_Update(MenuMgr *menu, float dt);

/**
 * Menu_Render — Render the current menu screen.
 */
void Menu_Render(const MenuMgr *menu, SDL_Renderer *renderer, int screen_w, int screen_h);

#ifdef __cplusplus
}
#endif

#endif /* LOCO_SDL3_MENU_H */
