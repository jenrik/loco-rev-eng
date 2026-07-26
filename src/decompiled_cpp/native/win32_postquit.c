/**
 * win32_postquit.c — Hide all UI windows on quit
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C free function, __cdecl. Hides all game windows before quitting.
 * If g_demo_mode is active, does nothing.
 */

#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern int32_t __stdcall ShowWindow(void* hWnd, int32_t nCmdShow);

/* Global state */
extern int32_t  g_demo_mode;         /* 0x4A9914 — 1 = demo mode active */
extern void*    g_about;             /* 0x4FD234 — AboutDialog */
extern void*    g_audio_mgr;         /* 0x4FD238 — GameAudio */
extern void*    g_cursor;            /* 0x4FD23C — Cursor */
extern void*    g_town;              /* 0x4FD240 — Town */
extern void*    g_postcard;          /* 0x4FD244 — PostcardAlbum */
extern void*    g_postcard_send;     /* 0x4FD248 — PostcardSend dialog */
extern void*    g_ui_main;           /* 0x4A991C — UI_MainMenu */
extern void*    g_main_window;       /* 0x4AA4A0 — CGWND main window */

/* UI_MainMenu_SetState at 0x4208F0 */
extern void __cdecl UI_MainMenu_SetState(void* main_menu, int32_t state);

/* ================================================================== */
/* WIN32_PostQuit — Hide all game UI windows before quit               */
/* Address: 0x463670                                                   */
/* Size: 329 bytes                                                     */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Iterates through all known UI windows (about, audio_mgr, cursor,    */
/* town, postcard, postcard_send, ui_main), checks if each is visible  */
/* (non-zero visible flag at +0xE4), and sends SW_MINIMIZE (7) to     */
/* hide it. Also handles the UI_MainMenu's children at +0x220/+0x21C. */
/*                                                                     */
/* If g_demo_mode is active, returns immediately without hiding.       */
/* ================================================================== */
void __cdecl WIN32_PostQuit(void)
{
    if (g_demo_mode == 1) {
        return;
    }

    /* Helper macro: if instance exists and is visible, hide it */
#define HIDE_IF_VISIBLE(inst) \
    do { \
        if ((inst) != NULL && *(uint8_t*)((uint8_t*)(inst) + 0xE4) != 0) { \
            ShowWindow(*(void**)((uint8_t*)(inst) + 8), 7); \
        } \
    } while (0)

    HIDE_IF_VISIBLE(g_about);
    HIDE_IF_VISIBLE(g_audio_mgr);
    HIDE_IF_VISIBLE(g_cursor);
    HIDE_IF_VISIBLE(g_town);
    HIDE_IF_VISIBLE(g_postcard);
    HIDE_IF_VISIBLE(g_postcard_send);

    /* UI_MainMenu children at +0x220 and +0x21C */
    if (g_ui_main != NULL) {
        uint32_t* child1 = *(uint32_t**)((uint8_t*)g_ui_main + 0x220);
        if (child1 != NULL && *(uint8_t*)((uint8_t*)child1 + 0xE8) != 0) {
            ShowWindow(*(void**)((uint8_t*)child1 + 8), 7);
        }

        if (g_ui_main != NULL) {
            uint32_t* child2 = *(uint32_t**)((uint8_t*)g_ui_main + 0x21C);
            if (child2 != NULL && *(uint8_t*)((uint8_t*)child2 + 0xE4) != 0) {
                ShowWindow(*(void**)((uint8_t*)child2 + 8), 7);
            }

            if (*(uint8_t*)((uint8_t*)g_ui_main + 0xE4) != 0) {
                if (*(uint32_t*)((uint8_t*)g_ui_main + 0x210) != 0) {
                    UI_MainMenu_SetState(g_ui_main, 7);  /* SW_MINIMIZE */
                }
                ShowWindow(*(void**)((uint8_t*)g_ui_main + 8), 7);
            }
        }
    }

    /* Hide main window */
    ShowWindow(*(void**)((uint8_t*)g_main_window + 8), 7);
}
