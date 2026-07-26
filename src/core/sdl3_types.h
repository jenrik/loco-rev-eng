/*
 * sdl3_types.h — SDL3-compatible type definitions for the Lego Loco port
 *
 * Provides Win32-like type aliases using SDL3 primitives.
 * Unlike the SDL2 port's loco_types.h, this file does NOT include
 * <windows.h> or <SDL2/SDL.h> — it uses only SDL3 and stdint.
 */

#ifndef LOCO_SDL3_TYPES_H
#define LOCO_SDL3_TYPES_H

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Win32 type aliases → SDL3
 * ========================================================================= */

typedef SDL_Window     *HWND;
typedef void           *HINSTANCE;
typedef void           *HANDLE;
typedef void           *HMODULE;
typedef void           *HMENU;
typedef void           *HICON;
typedef void           *HCURSOR;
typedef void           *HBRUSH;
typedef void           *HFONT;
typedef uint32_t        DWORD;
typedef uint16_t        WORD;
typedef uint8_t         BYTE;
typedef int32_t         LONG;
typedef int32_t         BOOL;
typedef char           *LPSTR;
typedef const char     *LPCSTR;
typedef void           *LPVOID;
typedef void           *LPDWORD;
typedef const void     *LPCVOID;

/* =========================================================================
 * RECT, POINT, SIZE
 * ========================================================================= */

typedef SDL_Rect   RECT;
typedef SDL_Point  POINT;
typedef SDL_FPoint POINTF;

/* =========================================================================
 * Game state machine
 *
 * Original: DAT_004851f4 in loco.exe (uint32_t)
 * ========================================================================= */

typedef enum {
    GAME_STATE_INIT      = 1,   /* Initialising / resetting world */
    GAME_STATE_LOADING   = 2,   /* Async asset load in progress   */
    GAME_STATE_RUNNING   = 3,   /* Normal gameplay                */
    GAME_STATE_PAUSED    = 4,   /* Game paused                    */
    GAME_STATE_MENU      = 5,   /* Main menu                      */
    GAME_STATE_MOVIE     = 7,   /* FMV playback                   */
    GAME_STATE_SAVE      = 8,   /* Save-game screen               */
    GAME_STATE_QUIT      = 10,  /* Trigger teardown               */
} GameState;

/* =========================================================================
 * CGWND — Main engine root object (0x28 bytes in original, simplified here)
 *
 * Original addresses:
 *   Constructor: 0x004061e0
 *   Singleton:   0x004aa4a0
 *
 * On SDL3, HWND fields map to SDL_Window*.
 * ========================================================================= */

typedef struct CGWND {
    SDL_Window   *window;          /* Main game window                  */
    int           screen_w;        /* Backbuffer width (e.g., 640)     */
    int           screen_h;        /* Backbuffer height (e.g., 480)    */
    int           display_w;       /* Native display width             */
    int           display_h;       /* Native display height            */
    bool          fullscreen;      /* Fullscreen flag                  */
    GameState     state;           /* Current game state               */
    uint32_t      game_time;       /* Frame counter / game time ticks  */
    float         fps;             /* Target frames per second          */
    bool          running;         /* false → exit game loop           */
} CGWND;

/* =========================================================================
 * Global game state
 * ========================================================================= */

extern CGWND    g_game;             /* Main engine object (replaces 0x4aa4a0) */
extern uint32_t g_game_time;        /* Frame counter (replaces 0x485204)      */
extern uint8_t  g_game_mode;        /* Game mode (replaces 0x4851f4)          */

/* =========================================================================
 * SDL3 macro helpers
 * ========================================================================= */

#define SetRect(r, l, t, ri, b)  do { \
    (r)->x = (l); (r)->y = (t); \
    (r)->w = (ri) - (l); (r)->h = (b) - (t); \
} while(0)

#define IntersectRect(out, a, b)  SDL_GetRectIntersection(a, b, out)

#ifdef __cplusplus
}
#endif

#endif /* LOCO_SDL3_TYPES_H */
