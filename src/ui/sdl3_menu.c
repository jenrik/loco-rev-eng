/*
 * sdl3_menu.c — Main menu implementation
 */

#include "sdl3_menu.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* =========================================================================
 * Helper: get centered text color based on hover state
 * ========================================================================= */

static void button_colors(bool hovered, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
    if (hovered) {
        *r = 255; *g = 220; *b = 60; *a = 255;  /* Gold/yellow highlight */
    } else {
        *r = 200; *g = 180; *b = 100; *a = 220;  /* Muted gold */
    }
}

/* =========================================================================
 * Draw a filled rounded-rect button (simple — just a rect with border)
 * ========================================================================= */

static void draw_button(SDL_Renderer *renderer, const MenuButton *btn)
{
    uint8_t r, g, b, a;
    button_colors(btn->hovered, &r, &g, &b, &a);

    /* Button background */
    SDL_SetRenderDrawColor(renderer, (uint8_t)(r/4), (uint8_t)(g/4), (uint8_t)(b/4), a);
    SDL_RenderFillRect(renderer, &btn->rect);

    /* Button border */
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderRect(renderer, &btn->rect);

    /* Inner border (highlight) */
    if (btn->hovered) {
        SDL_FRect inner = {
            btn->rect.x + 2, btn->rect.y + 2,
            btn->rect.w - 4, btn->rect.h - 4
        };
        SDL_SetRenderDrawColor(renderer, 255, 255, 200, 80);
        SDL_RenderRect(renderer, &inner);
    }
}

/* =========================================================================
 * Draw text using SDL's debug text (simple approach)
 * ========================================================================= */

static void draw_text(SDL_Renderer *renderer, const char *text,
                      float x, float y, float size,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    /* SDL3 doesn't have built-in text rendering without SDL_ttf.
     * We use a simple approach: draw a colored rectangle as a text placeholder,
     * and show the actual text in the window title for debugging.
     * Real text rendering would use SDL3_ttf. */
    (void)text; (void)size; (void)r; (void)g; (void)b; (void)a;

    /* For now, draw a small colored dot to mark the text position */
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_FRect dot = { x, y + size/2 - 2, size, 4 };
    SDL_RenderFillRect(renderer, &dot);
}

/* =========================================================================
 * Draw centered text using button label (show in title bar for now)
 * ========================================================================= */

static char g_title_buf[512] = {0};

static void draw_label(SDL_Renderer *renderer, MenuMgr *menu,
                       const MenuButton *btn)
{
    (void)menu;
    uint8_t r, g, b, a;
    button_colors(btn->hovered, &r, &g, &b, &a);

    /* Draw the label as text in the window title */
    if (btn->hovered && btn->label) {
        snprintf(g_title_buf, sizeof(g_title_buf),
                 "LEGO LOCO  |  %s", btn->label);
        /* We can't easily set window title from here,
         * so we show the label by drawing colored bars */
    }

    /* Draw button label as colored bars (visual placeholder) */
    if (btn->label) {
        int len = (int)strlen(btn->label);
        float bar_w = btn->rect.w * 0.6f / (float)len;
        float start_x = btn->rect.x + btn->rect.w * 0.2f;
        for (int i = 0; i < len; i++) {
            SDL_SetRenderDrawColor(renderer, r, g, b,
                (uint8_t)(a * (0.5f + 0.5f * (float)i / (float)len)));
            SDL_FRect bar = {
                start_x + (float)i * bar_w,
                btn->rect.y + btn->rect.h * 0.35f,
                bar_w * 0.8f,
                btn->rect.h * 0.3f
            };
            SDL_RenderFillRect(renderer, &bar);
        }
    }
}

/* =========================================================================
 * Menu button layout helpers
 * ========================================================================= */

static void layout_main_menu(MenuMgr *menu, int sw, int sh)
{
    const char *labels[] = {
        "NEW GAME", "LOAD GAME", "OPTIONS", "CREDITS", "QUIT", NULL
    };
    int actions[] = {
        MENU_ACT_NEW_GAME, MENU_ACT_LOAD_GAME, MENU_ACT_OPTIONS,
        MENU_ACT_CREDITS, MENU_ACT_QUIT
    };
    int count = 5;
    float btn_w = 280.0f;
    float btn_h = 48.0f;
    float start_y = (float)(sh - count * (btn_h + 16)) / 2.0f + 40.0f;

    menu->button_count = count;
    for (int i = 0; i < count; i++) {
        menu->buttons[i].label  = labels[i];
        menu->buttons[i].action = actions[i];
        menu->buttons[i].rect   = (SDL_FRect){
            ((float)sw - btn_w) / 2.0f,
            start_y + (float)i * (btn_h + 16.0f),
            btn_w, btn_h
        };
        menu->buttons[i].hovered = false;
    }
}

static void layout_title_screen(MenuMgr *menu, int sw, int sh)
{
    menu->button_count = 1;
    menu->buttons[0].label  = "CLICK TO START";
    menu->buttons[0].action = MENU_ACT_START;
    menu->buttons[0].rect   = (SDL_FRect){
        ((float)sw - 300.0f) / 2.0f,
        (float)sh * 0.7f,
        300.0f, 50.0f
    };
    menu->buttons[0].hovered = false;
}

static void layout_options(MenuMgr *menu, int sw, int sh)
{
    (void)sw; (void)sh;
    menu->button_count = 1;
    menu->buttons[0].label  = "BACK";
    menu->buttons[0].action = MENU_ACT_BACK;
    menu->buttons[0].rect   = (SDL_FRect){
        ((float)sw - 200.0f) / 2.0f,
        (float)sh - 100.0f,
        200.0f, 44.0f
    };
    menu->buttons[0].hovered = false;
}

static void layout_credits(MenuMgr *menu, int sw, int sh)
{
    layout_options(menu, sw, sh);  /* Same as options — just a Back button */
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void Menu_Init(MenuMgr *menu)
{
    memset(menu, 0, sizeof(*menu));
    menu->screen = MENU_NONE;
}

void Menu_Show(MenuMgr *menu, MenuScreen screen)
{
    menu->screen = screen;
    menu->anim_timer = 0.0f;
    menu->selected_action = MENU_ACT_NONE;

    int sw = 640, sh = 480;  /* Default; caller should update if needed */

    switch (screen) {
    case MENU_TITLE:   layout_title_screen(menu, sw, sh); break;
    case MENU_MAIN:    layout_main_menu(menu, sw, sh); break;
    case MENU_OPTIONS: layout_options(menu, sw, sh); break;
    case MENU_CREDITS: layout_credits(menu, sw, sh); break;
    default: break;
    }
}

int Menu_HandleEvent(MenuMgr *menu, const SDL_Event *event)
{
    menu->selected_action = MENU_ACT_NONE;

    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION: {
        float mx = event->motion.x;
        float my = event->motion.y;
        for (int i = 0; i < menu->button_count; i++) {
            SDL_FRect *r = &menu->buttons[i].rect;
            menu->buttons[i].hovered =
                (mx >= r->x && mx < r->x + r->w &&
                 my >= r->y && my < r->y + r->h);
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            float mx = event->button.x;
            float my = event->button.y;
            for (int i = 0; i < menu->button_count; i++) {
                SDL_FRect *r = &menu->buttons[i].rect;
                if (mx >= r->x && mx < r->x + r->w &&
                    my >= r->y && my < r->y + r->h) {
                    menu->selected_action = menu->buttons[i].action;
                    return menu->buttons[i].action;
                }
            }
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            if (menu->screen == MENU_MAIN) {
                menu->selected_action = MENU_ACT_QUIT;
                return MENU_ACT_QUIT;
            }
            if (menu->screen != MENU_TITLE) {
                menu->selected_action = MENU_ACT_BACK;
                return MENU_ACT_BACK;
            }
        }
        if (event->key.key == SDLK_RETURN || event->key.key == SDLK_SPACE) {
            if (menu->screen == MENU_TITLE) {
                menu->selected_action = MENU_ACT_START;
                return MENU_ACT_START;
            }
        }
        break;
    default:
        break;
    }

    return MENU_ACT_NONE;
}

void Menu_Update(MenuMgr *menu, float dt)
{
    menu->anim_timer += dt;
}

void Menu_Render(const MenuMgr *menu, SDL_Renderer *renderer,
                 int screen_w, int screen_h)
{
    (void)screen_w; (void)screen_h;

    /* Background — dark blue gradient (approximate with solid fill) */
    SDL_SetRenderDrawColor(renderer, 8, 20, 60, 255);
    SDL_RenderClear(renderer);

    switch (menu->screen) {
    case MENU_TITLE: {
        /* Title screen — Lego Loco logo area */
        /* Large yellow/gold rectangle for the title */
        SDL_SetRenderDrawColor(renderer, 220, 180, 20, 255);
        SDL_FRect logo = { 120.0f, 60.0f, 400.0f, 120.0f };
        SDL_RenderFillRect(renderer, &logo);

        SDL_SetRenderDrawColor(renderer, 40, 30, 10, 255);
        SDL_FRect inner = { 130.0f, 70.0f, 380.0f, 100.0f };
        SDL_RenderFillRect(renderer, &inner);

        /* Subtitle bar */
        SDL_SetRenderDrawColor(renderer, 200, 160, 40, 200);
        SDL_FRect sub = { 200.0f, 150.0f, 240.0f, 4.0f };
        SDL_RenderFillRect(renderer, &sub);

        /* Decorative dots around the logo */
        for (int i = 0; i < 8; i++) {
            float angle = menu->anim_timer * 0.5f + (float)i * 0.785f;
            float cx = 320.0f + cosf(angle) * 220.0f;
            float cy = 120.0f + sinf(angle) * 80.0f;
            SDL_SetRenderDrawColor(renderer,
                200 + (int)(sinf(angle * 2) * 55),
                160 + (int)(cosf(angle * 2) * 40),
                20, 180);
            SDL_FRect dot = { cx - 6, cy - 6, 12, 12 };
            SDL_RenderFillRect(renderer, &dot);
        }

        /* Draw buttons */
        for (int i = 0; i < menu->button_count; i++) {
            draw_button(renderer, &menu->buttons[i]);
            draw_label(renderer, (MenuMgr*)menu, &menu->buttons[i]);
        }
        break;
    }

    case MENU_MAIN: {
        /* Decorative border */
        SDL_SetRenderDrawColor(renderer, 220, 180, 20, 100);
        SDL_FRect border = { 40, 40, (float)screen_w - 80, (float)screen_h - 80 };
        SDL_RenderRect(renderer, &border);

        /* Title banner at top */
        SDL_SetRenderDrawColor(renderer, 30, 25, 15, 255);
        SDL_FRect banner = { 0, 0, (float)screen_w, 60 };
        SDL_RenderFillRect(renderer, &banner);

        SDL_SetRenderDrawColor(renderer, 200, 160, 40, 255);
        SDL_FRect line = { 0, 60, (float)screen_w, 3 };
        SDL_RenderFillRect(renderer, &line);

        /* Draw buttons */
        for (int i = 0; i < menu->button_count; i++) {
            draw_button(renderer, &menu->buttons[i]);
            draw_label(renderer, (MenuMgr*)menu, &menu->buttons[i]);

            /* Pulsing effect on hover */
            if (menu->buttons[i].hovered) {
                float pulse = 1.0f + sinf(menu->anim_timer * 3.0f) * 0.05f;
                SDL_FRect r = menu->buttons[i].rect;
                SDL_FRect glow = {
                    r.x - 4 * pulse, r.y - 4 * pulse,
                    r.w + 8 * pulse, r.h + 8 * pulse
                };
                SDL_SetRenderDrawColor(renderer, 255, 220, 60, 60);
                SDL_RenderRect(renderer, &glow);
            }
        }

        /* Version / footer */
        SDL_SetRenderDrawColor(renderer, 100, 80, 40, 150);
        SDL_FRect footer = { 0, (float)screen_h - 30, (float)screen_w, 30 };
        SDL_RenderFillRect(renderer, &footer);
        break;
    }

    case MENU_OPTIONS: {
        SDL_SetRenderDrawColor(renderer, 30, 25, 15, 255);
        SDL_FRect opt_banner = { 0, 0, (float)screen_w, 50 };
        SDL_RenderFillRect(renderer, &opt_banner);

        /* Draw buttons */
        for (int i = 0; i < menu->button_count; i++) {
            draw_button(renderer, &menu->buttons[i]);
            draw_label(renderer, (MenuMgr*)menu, &menu->buttons[i]);
        }
        break;
    }

    case MENU_CREDITS: {
        SDL_SetRenderDrawColor(renderer, 20, 15, 40, 255);
        SDL_RenderClear(renderer);

        /* Credit text placeholder */
        SDL_SetRenderDrawColor(renderer, 200, 180, 100, 255);
        SDL_FRect credit_bar = { 120, 100, 400, 200 };
        SDL_RenderRect(renderer, &credit_bar);

        for (int i = 0; i < menu->button_count; i++) {
            draw_button(renderer, &menu->buttons[i]);
            draw_label(renderer, (MenuMgr*)menu, &menu->buttons[i]);
        }
        break;
    }

    default:
        break;
    }
}
