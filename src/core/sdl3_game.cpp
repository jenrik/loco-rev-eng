/*
 * sdl3_game.cpp — SDL3 game loop implementation
 *
 * Drives the main game loop: ProcessEvents → Update → Render → Present.
 */

#include "sdl3_game.h"
#include "../port/sdl3/sdl3_backend.h"
#include "../port/sdl3/sdl3_input.h"
#include "../decompiled_cpp/port/sdl3_compat.h"
#include "../decompiled_cpp/port/sdl3_dsound_bridge.h"
#include "../platform/sdl3_ini.h"
#include "../resources/sdl3_resources.h"
#include "../world/sdl3_tilemap.h"
#include "../graphics/sdl3_sprites.h"
#include "../ui/sdl3_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Global state
 * ========================================================================= */

CGWND     g_game       = {0};
uint32_t  g_game_time  = 0;
uint8_t   g_game_mode  = GAME_STATE_INIT;

static ResMgr     *g_res_mgr  = NULL;
static TileMap    *g_tilemap  = NULL;
static SpriteCache g_sprites;
static MenuMgr     g_menu;

/* Mouse pan state */
static bool  g_mouse_panning = false;
static int   g_mouse_pan_start_x = 0;
static int   g_mouse_pan_start_y = 0;
static int   g_view_save_x = 0;
static int   g_view_save_y = 0;

/* Zoom level (1 = native, 2 = 2x, etc.) */
static float g_zoom = 1.0f;

/* Tile placement mode */
static bool     g_edit_mode = false;
static TileType g_selected_tile = TILE_ROAD;
static int      g_tool_size = 1;

ResMgr *Game_GetResMgr(void) { return g_res_mgr; }

/* =========================================================================
 * Game_Init
 * ========================================================================= */

int Game_Init(SDL_Window *window, const char *data_dir, INI_File *ini)
{
    memset(&g_game, 0, sizeof(g_game));
    g_game.window     = window;
    g_game.screen_w   = g_sdl3_backbuffer_w;
    g_game.screen_h   = g_sdl3_backbuffer_h;
    g_game.display_w  = g_sdl3_display_w;
    g_game.display_h  = g_sdl3_display_h;
    g_game.fullscreen = (g_sdl3_fullscreen != 0);
    g_game.state      = GAME_STATE_INIT;
    g_game.fps        = 60.0f;
    g_game.running    = true;

    g_game_time  = 0;
    g_game_mode  = GAME_STATE_INIT;

    /* Apply INI config overrides */
    if (ini) {
        int w = INI_GetInt(ini, "DISPLAY", "ScreenWidth", 640);
        int h = INI_GetInt(ini, "DISPLAY", "ScreenHeight", 480);
        int fs = INI_GetInt(ini, "DISPLAY", "FullScreen", 0);
        if (w > 0 && h > 0) {
            g_game.screen_w = w;
            g_game.screen_h = h;
        }
        g_game.fullscreen = (fs != 0);

        /* Load min FPS settings */
        int minVeh = INI_GetInt(ini, "BALANCING", "MinVehicleFPS", 20);
        int minBld = INI_GetInt(ini, "BALANCING", "MinBuildingFPS", 18);
        int minFig = INI_GetInt(ini, "BALANCING", "MinMinifigFPS", 12);
        printf("BALANCING: vehicle=%d building=%d minifig=%d\n",
               minVeh, minBld, minFig);
    }

    /* Load resources */
    if (data_dir) {
        g_res_mgr = ResMgr_Open(data_dir);
        if (g_res_mgr) {
            int count = ResMgr_GetEntryCount(g_res_mgr);
            printf("Resources: %d entries loaded from resource.RFH\n", count);
        } else {
            printf("WARNING: Could not open resource.RFH/RFD\n");
        }
    }

    /* Init sprite cache */
    SpriteCache_Init(&g_sprites);

    /* Try loading tile sprites from the data directory */
    if (data_dir) {
        char path[512];
        /* Load tile sprites if available */
        snprintf(path, sizeof(path), "%sgraphics/tiles.bmp", data_dir);
        if (SpriteCache_Load(&g_sprites, g_sdl3_renderer, path) < 0) {
            /* Try alternate paths */
            snprintf(path, sizeof(path), "%sExe/graphics/tiles.bmp", data_dir);
            SpriteCache_Load(&g_sprites, g_sdl3_renderer, path);
        }
        /* Load individual tile BMPs */
        const char *tile_names[] = {
            "grass", "water", "road", "track", "sand", "rock", "building", NULL
        };
        for (int i = 0; tile_names[i]; i++) {
            snprintf(path, sizeof(path), "%sgraphics/tile_%s.bmp",
                     data_dir, tile_names[i]);
            if (SpriteCache_Load(&g_sprites, g_sdl3_renderer, path) < 0) {
                snprintf(path, sizeof(path), "%sExe/graphics/tile_%s.bmp",
                         data_dir, tile_names[i]);
                SpriteCache_Load(&g_sprites, g_sdl3_renderer, path);
            }
        }
        printf("Sprites: %d textures loaded\n", g_sprites.count);
    }

    /* Create world tilemap (80x60 tiles = 1280x960 pixel world) */
    g_tilemap = TileMap_Create(80, 60, g_game.screen_w, g_game.screen_h);
    if (g_tilemap) {
        TileMap_GenerateTestWorld(g_tilemap);
        /* Center viewport */
        TileMap_ScrollTo(g_tilemap,
            (g_tilemap->pixel_width  - g_tilemap->view_w) / 2,
            (g_tilemap->pixel_height - g_tilemap->view_h) / 2);
    }

    printf("Game_Init: %dx%d fullscreen=%d data=%s\n",
           g_game.screen_w, g_game.screen_h, g_game.fullscreen,
           data_dir ? data_dir : "(none)");

    /* Initialize menu and show title screen */
    Menu_Init(&g_menu);
    Menu_Show(&g_menu, MENU_TITLE);

    /* Start in menu state */
    g_game.state = GAME_STATE_MENU;

    return 0;
}

/* =========================================================================
 * Game_Shutdown
 * ========================================================================= */

void Game_Shutdown(void)
{
    printf("Game_Shutdown\n");
    SpriteCache_Destroy(&g_sprites);
    if (g_tilemap) {
        TileMap_Destroy(g_tilemap);
        g_tilemap = NULL;
    }
    if (g_res_mgr) {
        ResMgr_Close(g_res_mgr);
        g_res_mgr = NULL;
    }
    g_game.running = false;
}

/* =========================================================================
 * Game_ProcessEvents
 * ========================================================================= */

int Game_ProcessEvents(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        /* Route to input system */
        LocoInput_ProcessEvent(&event);

        switch (event.type) {
        case SDL_EVENT_QUIT:
            g_game.running = false;
            return 1;

        case SDL_EVENT_KEY_DOWN: {
            SDL_Keycode key = event.key.key;
            SDL_Keymod  mod = event.key.mod;

            /* Global hotkeys */
            if (key == SDLK_F4 && (mod & SDL_KMOD_ALT)) {
                g_game.running = false;
                return 1;
            }

            /* Escape toggles between game and menu */
            if (key == SDLK_ESCAPE) {
                if (g_game.state == GAME_STATE_RUNNING) {
                    g_game.state = GAME_STATE_MENU;
                    Menu_Show(&g_menu, MENU_MAIN);
                    break;
                } else if (g_game.state == GAME_STATE_MENU) {
                    if (g_menu.screen == MENU_MAIN) {
                        g_game.running = false;
                        return 1;
                    }
                    Menu_Show(&g_menu, MENU_MAIN);
                    break;
                }
            }

            /* Game-state-specific key handling */
            switch (g_game.state) {
            case GAME_STATE_MENU: {
                int action = Menu_HandleEvent(&g_menu, &event);
                switch (action) {
                case MENU_ACT_START:
                    Menu_Show(&g_menu, MENU_MAIN);
                    break;
                case MENU_ACT_NEW_GAME:
                    g_game.state = GAME_STATE_RUNNING;
                    break;
                case MENU_ACT_OPTIONS:
                    Menu_Show(&g_menu, MENU_OPTIONS);
                    break;
                case MENU_ACT_CREDITS:
                    Menu_Show(&g_menu, MENU_CREDITS);
                    break;
                case MENU_ACT_BACK:
                    Menu_Show(&g_menu, MENU_MAIN);
                    break;
                case MENU_ACT_QUIT:
                    g_game.running = false;
                    break;
                default:
                    break;
                }
                break;
            }

            case GAME_STATE_RUNNING:
                /* Tile selection (number keys 1-7) */
                switch (key) {
                case SDLK_1: g_selected_tile = TILE_GRASS;    g_edit_mode = true; break;
                case SDLK_2: g_selected_tile = TILE_WATER;    g_edit_mode = true; break;
                case SDLK_3: g_selected_tile = TILE_ROAD;     g_edit_mode = true; break;
                case SDLK_4: g_selected_tile = TILE_TRACK;    g_edit_mode = true; break;
                case SDLK_5: g_selected_tile = TILE_SAND;     g_edit_mode = true; break;
                case SDLK_6: g_selected_tile = TILE_ROCK;     g_edit_mode = true; break;
                case SDLK_7: g_selected_tile = TILE_BUILDING; g_edit_mode = true; break;
                case SDLK_0: g_selected_tile = TILE_EMPTY;    g_edit_mode = true; break;
                case SDLK_ESCAPE: g_edit_mode = false; break;
                case SDLK_BACKSPACE: /* Eraser */ g_selected_tile = TILE_GRASS; g_edit_mode = true; break;
                case SDLK_LEFTBRACKET:  if (g_tool_size > 1) g_tool_size--; break;
                case SDLK_RIGHTBRACKET: if (g_tool_size < 10) g_tool_size++; break;
                default: break;
                }

                /* Viewport scrolling with arrow keys */
                if (g_tilemap) {
                    int scroll_speed = (int)(8.0f / g_zoom);
                    if (scroll_speed < 1) scroll_speed = 1;
                    if (mod & SDL_KMOD_SHIFT) scroll_speed *= 4;
                    switch (key) {
                    case SDLK_LEFT:  TileMap_ScrollBy(g_tilemap, -scroll_speed, 0); break;
                    case SDLK_RIGHT: TileMap_ScrollBy(g_tilemap,  scroll_speed, 0); break;
                    case SDLK_UP:    TileMap_ScrollBy(g_tilemap, 0, -scroll_speed); break;
                    case SDLK_DOWN:  TileMap_ScrollBy(g_tilemap, 0,  scroll_speed); break;
                    case SDLK_HOME:  /* Reset view */
                        TileMap_ScrollTo(g_tilemap,
                            (g_tilemap->pixel_width  - g_tilemap->view_w) / 2,
                            (g_tilemap->pixel_height - g_tilemap->view_h) / 2);
                        break;
                    case SDLK_MINUS: case SDLK_KP_MINUS:
                        g_zoom *= 0.5f;
                        if (g_zoom < 0.25f) g_zoom = 0.25f;
                        printf("Zoom: %.1fx\n", (double)g_zoom);
                        break;
                    case SDLK_EQUALS: case SDLK_KP_PLUS:
                        g_zoom *= 2.0f;
                        if (g_zoom > 4.0f) g_zoom = 4.0f;
                        printf("Zoom: %.1fx\n", (double)g_zoom);
                        break;
                    case SDLK_0:
                        g_zoom = 1.0f;
                        printf("Zoom: reset to 1.0x\n");
                        break;
                    default: break;
                    }
                }
            default:
                break;
            }
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            /* Route to menu if in menu state */
            if (g_game.state == GAME_STATE_MENU ||
                g_game.state == GAME_STATE_PAUSED) {
                Menu_HandleEvent(&g_menu, &event);
                break;
            }
            if (event.button.button == SDL_BUTTON_LEFT &&
                g_game.state == GAME_STATE_RUNNING) {
                if (g_edit_mode && g_tilemap) {
                    /* Place tile at click position */
                    int mx = (int)(event.button.x / g_zoom) + g_tilemap->view_x;
                    int my = (int)(event.button.y / g_zoom) + g_tilemap->view_y;
                    int tx = mx / g_tilemap->tile_size;
                    int ty = my / g_tilemap->tile_size;
                    for (int dy = 0; dy < g_tool_size; dy++) {
                        for (int dx = 0; dx < g_tool_size; dx++) {
                            TileMap_SetTile(g_tilemap, tx + dx, ty + dy, g_selected_tile);
                        }
                    }
                } else {
                    /* Pan mode */
                    g_mouse_panning = true;
                    g_mouse_pan_start_x = (int)event.button.x;
                    g_mouse_pan_start_y = (int)event.button.y;
                    if (g_tilemap) {
                        g_view_save_x = g_tilemap->view_x;
                        g_view_save_y = g_tilemap->view_y;
                    }
                }
            } else if (event.button.button == SDL_BUTTON_RIGHT &&
                       g_game.state == GAME_STATE_RUNNING) {
                /* Right click = pick tile under cursor */
                if (g_tilemap) {
                    int mx = (int)(event.button.x / g_zoom) + g_tilemap->view_x;
                    int my = (int)(event.button.y / g_zoom) + g_tilemap->view_y;
                    int tx = mx / g_tilemap->tile_size;
                    int ty = my / g_tilemap->tile_size;
                    g_selected_tile = TileMap_GetTile(g_tilemap, tx, ty);
                    g_edit_mode = true;
                }
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                g_mouse_panning = false;
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            /* Route to menu if in menu state */
            if (g_game.state == GAME_STATE_MENU ||
                g_game.state == GAME_STATE_PAUSED) {
                Menu_HandleEvent(&g_menu, &event);
                break;
            }
            if (g_mouse_panning && g_tilemap &&
                g_game.state == GAME_STATE_RUNNING) {
                int dx = g_mouse_pan_start_x - (int)event.motion.x;
                int dy = g_mouse_pan_start_y - (int)event.motion.y;
                TileMap_ScrollTo(g_tilemap,
                    g_view_save_x + (int)(dx / g_zoom),
                    g_view_save_y + (int)(dy / g_zoom));
            }
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            if (!g_game.fullscreen) {
                SDL_SetRenderLogicalPresentation(g_sdl3_renderer,
                    g_game.screen_w, g_game.screen_h,
                    SDL_LOGICAL_PRESENTATION_LETTERBOX);
            }
            break;

        default:
            break;
        }
    }
    return 0;
}

/* =========================================================================
 * Game_Update
 * ========================================================================= */

void Game_Update(void)
{
    g_game_time++;

    /* State-specific update */
    switch (g_game.state) {
    case GAME_STATE_INIT:
        break;

    case GAME_STATE_LOADING:
        break;

    case GAME_STATE_MENU:
        Menu_Update(&g_menu, 1.0f / g_game.fps);
        if (g_menu.selected_action == MENU_ACT_QUIT) {
            g_game.running = false;
        }
        break;

    case GAME_STATE_RUNNING: {
        /* TODO: update world, entities, animations, UI */
        break;
    }

    case GAME_STATE_PAUSED:
        /* No updates while paused */
        break;

        break;

    case GAME_STATE_QUIT:
        g_game.running = false;
        break;

    default:
        break;
    }
}

/* =========================================================================
 * Game_Render
 * ========================================================================= */

void Game_Render(void)
{
    SDL_Renderer *renderer = g_sdl3_renderer;

    /* Clear to dark blue (classic Lego Loco background) */
    SDL_SetRenderDrawColor(renderer, 0, 40, 80, 255);
    SDL_RenderClear(renderer);

    /* State-specific rendering */
    switch (g_game.state) {
    case GAME_STATE_MENU:
        Menu_Render(&g_menu, renderer, g_game.screen_w, g_game.screen_h);
        break;

    case GAME_STATE_RUNNING: {
        /* Apply zoom: scale the renderer */
        if (g_zoom != 1.0f) {
            SDL_SetRenderScale(renderer, g_zoom, g_zoom);
        }

        /* Draw the world tilemap */
        if (g_tilemap) {
            TileMap_Draw(g_tilemap, renderer);
        }

        /* Reset zoom for HUD overlay */
        if (g_zoom != 1.0f) {
            SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        }

        /* HUD overlay — viewport position, zoom, edit mode */
        {
            const char *tile_names[] = {
                "Empty", "Grass", "Water", "Road", "Track", "Sand", "Rock", "Building"
            };
            char hud[512];
            if (g_edit_mode) {
                snprintf(hud, sizeof(hud),
                    "LEGO LOCO  |  EDIT: %s (tool=%d)  |  View: %d,%d  |  Zoom: %.1fx  |  1-7=tile  Rclick=pick  Esc=pan  Arrows/Mouse=move",
                    tile_names[g_selected_tile], g_tool_size,
                    g_tilemap ? g_tilemap->view_x : 0,
                    g_tilemap ? g_tilemap->view_y : 0,
                    (double)g_zoom);
            } else {
                snprintf(hud, sizeof(hud),
                    "LEGO LOCO  |  View: %d,%d  |  Zoom: %.1fx  |  1-7=edit mode  Arrows/Mouse=pan  +/-=zoom  Esc=quit",
                    g_tilemap ? g_tilemap->view_x : 0,
                    g_tilemap ? g_tilemap->view_y : 0,
                    (double)g_zoom);
            }
            SDL_SetWindowTitle(g_sdl3_window, hud);
        }

        /* Edit mode cursor — show selected tile ghost */
        if (g_edit_mode && g_tilemap) {
            float fmx, fmy;
            SDL_GetMouseState(&fmx, &fmy);
            int wx = (int)(fmx / g_zoom) + g_tilemap->view_x;
            int wy = (int)(fmy / g_zoom) + g_tilemap->view_y;
            int tx = (wx / g_tilemap->tile_size) * g_tilemap->tile_size;
            int ty = (wy / g_tilemap->tile_size) * g_tilemap->tile_size;

            /* Draw tool size indicator */
            int ts = g_tilemap->tile_size;
            for (int dy = 0; dy < g_tool_size; dy++) {
                for (int dx = 0; dx < g_tool_size; dx++) {
                    float sx = (float)(tx + dx * ts - g_tilemap->view_x) * g_zoom;
                    float sy = (float)(ty + dy * ts - g_tilemap->view_y) * g_zoom;
                    SDL_FRect rect = { sx, sy, (float)(ts * g_zoom), (float)(ts * g_zoom) };
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 120);
                    SDL_RenderRect(renderer, &rect);
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 40);
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }

        /* Resource count status bar at bottom */
        int res_count = g_res_mgr ? ResMgr_GetEntryCount(g_res_mgr) : 0;
        if (res_count > 0) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
            SDL_FRect bar_bg = { 0, (float)g_game.screen_h - 24,
                                 (float)g_game.screen_w, 24 };
            SDL_RenderFillRect(renderer, &bar_bg);

            SDL_SetRenderDrawColor(renderer, 0, 220, 0, 255);
            int bar_w = (res_count * g_game.screen_w) / 4096;
            if (bar_w < 4) bar_w = 4;
            SDL_FRect bar = { 0, (float)g_game.screen_h - 24,
                              (float)bar_w, 24 };
            SDL_RenderFillRect(renderer, &bar);
        }

        /* Panning cursor indicator */
        if (g_mouse_panning) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 128);
            SDL_FRect cross_h = { 0, (float)g_game.screen_h / 2 - 1,
                                  (float)g_game.screen_w, 2 };
            SDL_FRect cross_v = { (float)g_game.screen_w / 2 - 1, 0,
                                  2, (float)g_game.screen_h };
            SDL_RenderFillRect(renderer, &cross_h);
            SDL_RenderFillRect(renderer, &cross_v);
        }

        break;
    }

        break;

    case GAME_STATE_LOADING:
        /* Loading screen */
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        { SDL_FRect r = { 220.0f, 230.0f, 200.0f, 20.0f }; SDL_RenderFillRect(renderer, &r); }
        break;

    default:
        break;
    }

    /* Present the frame */
    SDL_RenderPresent(renderer);
}

/* =========================================================================
 * Game_Run — Main game loop
 *
 * Original flow (loco.exe):
 *   while (g_game_mode != GAME_STATE_QUIT) {
 *       PeekMessage / GetMessage → DispatchMessage
 *       if (g_game_mode == RUNNING) { GameLoop_FrameUpdate(); }
 *       DDRAW_PresentRect(); // Flip backbuffer to primary
 *   }
 * ========================================================================= */

void Game_Run(void)
{
    printf("Game_Run: entering main loop\n");

    uint64_t last_time  = SDL_GetTicks();
    uint64_t frame_start;
    double   dt;

    while (g_game.running) {
        frame_start = SDL_GetTicks();

        /* Process all pending events */
        Game_ProcessEvents();

        /* Update game logic */
        Game_Update();

        /* Render frame */
        Game_Render();

        /* Per-frame input cleanup */
        LocoInput_EndFrame();

        /* Frame timing */
        dt = (double)(SDL_GetTicks() - frame_start) / 1000.0;
        double target_ms = 1000.0 / (double)g_game.fps;
        if (dt < target_ms) {
            SDL_Delay((uint32_t)(target_ms - dt));
        }

        /* Debug: log FPS every 5 seconds */
        static uint64_t fps_last = 0;
        static int      fps_count = 0;
        fps_count++;
        uint64_t now = SDL_GetTicks();
        if (now - fps_last >= 5000) {
            double fps = (double)fps_count / ((double)(now - fps_last) / 1000.0);
            if (g_tilemap) {
                printf("FPS: %.1f view=(%d,%d) world=%dx%d\n",
                    fps, g_tilemap->view_x, g_tilemap->view_y,
                    g_tilemap->map_width, g_tilemap->map_height);
            } else {
                printf("FPS: %.1f (state=%d)\n", fps, g_game.state);
            }
            fps_count = 0;
            fps_last  = now;
        }
    }

    printf("Game_Run: exiting main loop\n");
}
