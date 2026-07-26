/**
 * InitMode1.cpp — CGWND mode-1 initialization (loading screen / game start)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: INTEGRATED — Pass 3 complete.
 *   - CGWND_InitMode1 free function → CGWND::initMode1() method
 *   - g_netman→m_gameMode verified at 0x7C4 (fixed Ghidra decompiler error)
 *   - All field offsets cross-referenced and consistent across callers
 *   - InitAllSubsystems uses direct new; constructor bridges eliminated
 *
 * CGWND_InitMode1 has two code paths:
 *   PATH A (field_10 == 0): First-time loading screen with incremental
 *     subsystem initialization + progress pump. Each init step is
 *     followed by CGWND_Present(0) + CGWND_PumpMessages(1) to render
 *     progress feedback. Starts async background task at 0x45DE40.
 *   PATH B (field_10 != 0): Return-to-menu — loads a world save file
 *     and transitions to gameplay mode 3. Handles demo/screensaver
 *     cycling and multiplayer layout selection.
 */

#include <stddef.h>


/* Real class headers for typed casts in initMode1() */
#include "../ui/EditWindow.h"
#include "../town/Town.h"
#include "../ui/PostcardPreviewWindow.h"
#include "../graphics/LOCOBITMAP.h"
#include "../input/Cursor.h"
#include "../network/Netman.h"
#include "CGWND.h"

/* Win32 API — declared as extern; resolved at link time.
 * SetTimer is already declared via compat.h / sdl3_window.h. */
extern "C" {
    BOOL  EnableWindow(HWND hWnd, BOOL bEnable);
    BOOL  InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
    BOOL  UpdateWindow(HWND hWnd);
    BOOL  PlaySoundA(const char* pszSound, HMODULE hmod, DWORD fdwSound);
}



/* ================================================================== */
/* Globals referenced by CGWND_InitMode1                               */
/* ================================================================== */

extern void*    g_ui_main;              /* 0x4FD378 — EditWindow* (main menu) */
extern void*    g_town;                 /* 0x4FD37C — Town* */
extern void*    g_cursor;               /* 0x4FD380 — Cursor* */
extern void*    g_postcard;             /* 0x4FD384 — PostcardAlbum* */
extern void*    g_postcard_send;        /* 0x4FD388 — PostcardPreviewWindow* */
extern void*    g_audio;               /* 0x4FD3BC — GameAudio* */
extern void*    g_game;                 /* 0x4854C8 — Game* */
extern void*    g_netman;               /* 0x4FD3AC — Netman* */
extern int      g_demo_mode;            /* 0x4A9918 */
extern int      g_input_mgr;            /* 0x4A99B0 — InputMgr */
extern uint8_t  g_clean_exit;           /* 0x485218 — clean exit flag */
extern int   g_timer_id;                /* timer ID global */
extern void*    g_async_task_queue;     /* 0x4A9AD0 */

/* String constants */
extern const char s__curr_0047e2a0[];   /* "curr" */
extern const char s_Layouts_fmt_0047e2a8[];  /* "Layouts\%02d" format string */


/* ================================================================== */
/* Forward declarations for extern functions used in CGWND_InitMode1   */
/* ================================================================== */

extern void  GameAudio_UpdateVolume(void* audio, int level);
extern void  Game_SetScreenMode(void* game, int a, int b, int c);
extern void  DirectPlay_Init(void);
extern void  CGWND_Present(uint32_t flags);
extern void  CGWND_PumpMessages(char filter);
extern void  WIN32_QueueAsyncTask(void* queue, void* callback, int param);
extern void  RESMGR_SelectScreensaver(char* outBuf);
extern char  INPUT_LoadWorld(void* input_mgr, const char* path);
extern void  CGWND_SetMode(int mode);
extern int   wsprintfA(char* buf, const char* fmt, ...);


/* ================================================================== */
/* CGWND::initMode1 — Mode 1 initialization state machine              */
/* Address: 0x408350 — called from CGWND_SetMode(1) via g_main_window  */
/* ================================================================== */
/**
 * Mode 1 is the "loading / world select" state. On first entry
 * (field_10 == 0) it shows a loading progress screen while
 * incrementally initializing subsystems. On subsequent entries
 * (field_10 != 0, set by CGWND_QuitToMenu) it loads a saved world
 * and transitions directly to gameplay mode 3.
 */
void CGWND::initMode1()
{
    char   screensaver_path[0x104];   /* local_210 — screensaver / world path */
    char   layout_path[0x104];        /* local_108 — multiplayer layout path   */

    /* Zero both path buffers (0x41 DWORDs each = 0x104 bytes) */
    for (int i = 0; i < 0x41; i++) {
        ((uint32_t*)screensaver_path)[i] = 0;
        ((uint32_t*)layout_path)[i] = 0;
    }

    HWND hWnd = this->hWnd;

    /* Start 150ms timer (ID 0x47) for loading animation spinner */
    g_timer_id = SetTimer(hWnd, 0x47, 150, nullptr);

    /* Set audio volume to max on mode entry */
    if (g_audio != nullptr) {
        GameAudio_UpdateVolume(g_audio, 1);
    }

    /* ================================================================ */
    /* PATH A: First-time init — loading screen with progress updates    */
    /* ================================================================ */
    if (this->field_10 == 0) {

        /* Hide the main menu UI (vtable[1] = EditWindow::hide, 0x420860) */
        ((EditWindow*)g_ui_main)->hide();

        /* Set screen mode: loading transition */
        Game_SetScreenMode(&g_game, 0, 1, 0);

        /* Initialize DirectPlay compositing (0x45E090):
         *   - Fills primary surface with black
         *   - Creates shadow GameObject
         *   - Presents first frame */
        DirectPlay_Init();

        /* Disable main window during loading to prevent input */
        EnableWindow(hWnd, FALSE);

        /* Pump messages once to render the initial loading frame */
        CGWND_PumpMessages(1);

        if (g_demo_mode != 1) {
            /* --- Incremental subsystem initialization ---
             * Each step initializes a subsystem then presents + pumps
             * to update the loading screen progress. */

            /* Town overlay sprite (0x42FDF0) */
            ((Town*)g_town)->init_overlay_sprite();
            CGWND_Present(0);
            CGWND_PumpMessages(1);

            /* Cursor background surface (0x416460) */
            ((Cursor*)g_cursor)->init_background();
            CGWND_Present(0);
            CGWND_PumpMessages(1);

            /* Postcard album window surface (0x404720) */
            ((PostcardAlbum*)g_postcard)->InitWindowSurface();
            CGWND_Present(0);
            CGWND_PumpMessages(1);

            /* Postcard preview window — only when hosting/joined.
             * Netman::m_gameMode at +0x7C4: 0=waiting,1=hosting,2=joined */
            if (((Netman*)g_netman)->m_gameMode == 2) {
                ((PostcardPreviewWindow*)g_postcard_send)->init_background();
                CGWND_Present(0);
                CGWND_PumpMessages(1);
            }
        }

        /* Start async background task for loading/intro playback.
         * Callback at 0x45DE40 handles the loading sequence. */
        WIN32_QueueAsyncTask(&g_async_task_queue, (void*)0x45DE40, 0);

        /* Invalidate + update to trigger initial repaint */
        InvalidateRect(hWnd, nullptr, FALSE);
        UpdateWindow(hWnd);
        return;
    }

    /* ================================================================ */
    /* PATH B: Return-to-menu — load world and transition to gameplay    */
    /* ================================================================ */

    const char* worldPath;

    if (g_demo_mode == 1) {
        /* --- Demo mode: select random screensaver, load it --- */
        RESMGR_SelectScreensaver(screensaver_path);
        char loadResult = INPUT_LoadWorld(&g_input_mgr, screensaver_path);
        if (loadResult != 0) {
            /* Load succeeded — jump to common subsystem init */
            goto common_init;
        }
        /* Load failed — fall through to clean-exit check below */
    } else {
        /* --- Retail mode --- */
        /* Check if this is a multiplayer scenario (m_gameMode == 2 = joined) */
        if (((Netman*)g_netman)->m_gameMode != 2) {
            /* Not multiplayer — fall through to clean-exit check */
        } else {
            /* Multiplayer: build layout path "Layouts\XX.loco"
             * where XX = playerCount + 0x12 (18) */
            int playerCount = ((Netman*)g_netman)->GetPlayerCount();
            if (playerCount == 0) {
                /* No players — jump to common init (no world to load) */
                goto common_init;
            }
            wsprintfA(layout_path, s_Layouts_fmt_0047e2a8, playerCount + 0x12);
            worldPath = layout_path;
            goto load_world;
        }
    }

    /* --- Clean-exit flag check ---
     * Reached when: demo load failed, or retail non-multiplayer.
     * If g_clean_exit == 0 (unclean exit), skip world loading entirely
     * and go straight to common init. Otherwise load "curr". */
    if (g_clean_exit == 0) {
        goto common_init;
    }
    worldPath = s__curr_0047e2a0;   /* "curr" */

load_world:
    INPUT_LoadWorld(&g_input_mgr, worldPath);

common_init:
    /* --- Common subsystem initialization (both paths converge here) --- */

    /* Hide the main menu UI (vtable[1] = EditWindow::hide) */
    ((EditWindow*)g_ui_main)->hide();

    /* Incremental subsystem init (no progress pump this time) */
    ((Town*)g_town)->init_overlay_sprite();
    ((Cursor*)g_cursor)->init_background();
    ((PostcardAlbum*)g_postcard)->InitWindowSurface();

    /* Postcard preview when hosting/joined (m_gameMode == 2) */
    if (((Netman*)g_netman)->m_gameMode == 2) {
        ((PostcardPreviewWindow*)g_postcard_send)->init_background();
    }

    /* Stop any currently playing sound */
    PlaySoundA(nullptr, nullptr, 0);

    /* Transition to gameplay mode 3 */
    CGWND_SetMode(3);
}

