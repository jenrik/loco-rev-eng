/**
 * sprite_tilemap.c — TileMap sprite data lifecycle and cleanup functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * MISNAMED as "Sprite_*": These functions operate on the global TileMap
 * struct (g_tilemap at 0x4AAD08), not on individual ButtonSprite objects.
 * They manage sprite data buffers stored as part of the TileMap:
 *
 *   TileMap+0x52484: occupancy bitmap pointer (from TileMap_Init)
 *   TileMap+0x52488: sprite data buffer 1
 *   TileMap+0x5248C: sprite data buffer 2
 *
 * These are __fastcall but called as C-linkage functions with the
 * g_tilemap address passed in ECX.
 */

#include <stdint.h>

/* ================================================================== */
/* External functions                                                  */
/* ================================================================== */

extern void  __cdecl GLOBAL_free(void* ptr);
extern void  __cdecl DDRAW_SpriteDataDtor(void* data);   /* 0x45CE10 */
extern void  __cdecl Game_DeselectGameObject(void* game);
extern void  __cdecl UI_CleanupTooltips(void* tooltip_mgr);
extern void  __cdecl World_Init(void* world);
extern void  __cdecl INPUT_FileDlgProc(void* input);
extern void  __cdecl DDRAW_SpriteDataDtor(uint32_t* data);
extern int32_t __stdcall InvalidateRect(void* hWnd, void* rect, uint32_t erase);
extern int32_t __stdcall UpdateWindow(void* hWnd);

/* Globals */
extern int32_t  g_screen_width;         /* 0x4851D8 */
extern int32_t  g_screen_height;        /* 0x485214 */
extern int32_t  g_client_offset_x;      /* 0x485228 */
extern int32_t  g_client_width;         /* 0x485220 */
extern int32_t  g_client_offset_y;      /* 0x48522C */
extern int32_t  g_client_height;        /* 0x485224 */
extern void*    g_main_window;          /* 0x4AA4A0 */
extern uint32_t g_game_mode;            /* 0x4851D4 */

/* TileMap struct offsets for sprite data */
#define TILEMAP_OFF_TILES_X         0x3E    /* int16: tiles_x */
#define TILEMAP_OFF_TILES_Y         0x40    /* int16: tiles_y */
#define TILEMAP_OFF_OCCUPANCY       0x52484 /* uint8_t*: bitmap pointer */
#define TILEMAP_OFF_SPRITE_DATA_1   0x52488 /* uint32_t*: first sprite buffer */
#define TILEMAP_OFF_SPRITE_DATA_2   0x5248C /* uint32_t*: second sprite buffer */

/* ================================================================== */
/* Sprite_Shutdown — Free TileMap sprite buffers and occupancy         */
/* Address: 0x454DE0                                                   */
/* Size: 113 bytes                                                     */
/* Calling convention: __fastcall (param_1 in ECX = TileMap*)         */
/*                                                                     */
/* Releases both sprite data buffers (via DDRAW_SpriteDataDtor +       */
/* GLOBAL_free), then frees the occupancy bitmap. Called during        */
/* CGWND_Cleanup.                                                      */
/*                                                                     */
/* @param tilemap  Pointer to global TileMap struct (0x4AAD08)        */
/* ================================================================== */
void __fastcall Sprite_Shutdown(void* tilemap)
{
    uint32_t* p = (uint32_t*)tilemap;
    uint32_t* data;

    /* Free sprite data 1 */
    data = (uint32_t*)p[TILEMAP_OFF_SPRITE_DATA_1 / 4];
    if (data != NULL) {
        DDRAW_SpriteDataDtor(data);
        GLOBAL_free(data);
        p[TILEMAP_OFF_SPRITE_DATA_1 / 4] = 0;
    }

    /* Free sprite data 2 */
    data = (uint32_t*)p[TILEMAP_OFF_SPRITE_DATA_2 / 4];
    if (data != NULL) {
        DDRAW_SpriteDataDtor(data);
        GLOBAL_free(data);
        p[TILEMAP_OFF_SPRITE_DATA_2 / 4] = 0;
    }

    /* Free occupancy bitmap */
    if (*(void**)((uint8_t*)tilemap + TILEMAP_OFF_OCCUPANCY) != NULL) {
        GLOBAL_free(*(void**)((uint8_t*)tilemap + TILEMAP_OFF_OCCUPANCY));
        *(uint32_t*)((uint8_t*)tilemap + TILEMAP_OFF_OCCUPANCY) = 0;
    }
}

/* ================================================================== */
/* Sprite_LockAll — Recalculate TileMap centre offsets                 */
/* Address: 0x454FA0                                                   */
/* Size: 59 bytes                                                      */
/* Calling convention: __fastcall (param_1 in ECX = TileMap*)         */
/*                                                                     */
/* Recalculates the centre_x (+0x2C) and centre_y (+0x30) fields      */
/* from the client area dimensions and the tile offset fields at       */
/* +0x1C and +0x20.                                                    */
/*                                                                     */
/* Called by: CGWND_ScrollHorizontal, CGWND_ScrollVertical,           */
/*            CGWND_SetFullscreenMode                                   */
/*                                                                     */
/* Formula: centre = (g_client_offset - g_client_size) / 2             */
/*                    + tile_offset + g_client_size                     */
/*                                                                     */
/* @param tilemap  Pointer to global TileMap struct (0x4AAD08)        */
/* ================================================================== */
void __fastcall Sprite_LockAll(void* tilemap)
{
    uint8_t* t = (uint8_t*)tilemap;

    /* centre_x (+0x2C) = (g_client_offset_x - g_client_width) / 2 + tile_offset_x + g_client_width */
    *(int32_t*)(t + 0x2C) =
        ((g_client_offset_x - g_client_width) / 2)
        + *(int32_t*)(t + 0x1C)
        + g_client_width;

    /* centre_y (+0x30) = (g_client_offset_y - g_client_height) / 2 + tile_offset_y + g_client_height */
    *(int32_t*)(t + 0x30) =
        ((g_client_offset_y - g_client_height) / 2)
        + *(int32_t*)(t + 0x20)
        + g_client_height;
}

/* ================================================================== */
/* Sprite_UnlockAll — Full TileMap reset and redraw                    */
/* Address: 0x454FE0                                                   */
/* Size: 211 bytes                                                     */
/* Calling convention: __fastcall (param_1 in ECX = TileMap*)         */
/*                                                                     */
/* Performs a global state reset before loading a new world:           */
/*   1. Calls Game_DeselectGameObject (deselect from g_game)           */
/*   2. World_Init (reset global world state)                          */
/*   3. UI_CleanupTooltips (release tooltips)                          */
/*   4. INPUT_FileDlgProc (reset file dialog state)                    */
/*   5. Clears a large data array at TileMap+0x44 (0x14910 dwords)    */
/*   6. Re-initialises the occupancy bitmap to all 1s                  */
/*   7. Clears a 65x81 matrix of two-byte values at TileMap+0x81      */
/*      (stride 0x1040, 0x41 rows)                                     */
/*   8. Invalidates the main game window to trigger full redraw        */
/*                                                                     */
/* Called by: CGWND_Cleanup, CGWND_QuitToMenu, INPUT_LoadSaveFile,    */
/*            Sprite_Create, various world transitions                  */
/*                                                                     */
/* @param tilemap  Pointer to global TileMap struct (0x4AAD08)        */
/* ================================================================== */
void __fastcall Sprite_UnlockAll(void* tilemap)
{
    uint8_t* t = (uint8_t*)tilemap;
    int16_t tiles_x, tiles_y;
    int32_t total_tiles, bitmap_size;

    /* Step 1: Deselect game object */
    Game_DeselectGameObject((void*)0x4854C8);  /* g_game */

    /* Step 2: Init global world state */
    World_Init((void*)0x4A98B0);               /* g_world */

    /* Step 3: Cleanup tooltips */
    UI_CleanupTooltips((void*)0x4FD220);        /* g_tooltip_mgr */

    /* Step 4: Reset file dialog */
    INPUT_FileDlgProc((void*)0x4A9990);         /* g_input_mgr */

    /* Step 5: Clear the large data array at TileMap+0x44 */
    /* 0x14910 dwords = 0x52440 bytes — fills from +0x44 to +0x52484 */
    {
        uint32_t* clear_ptr = (uint32_t*)(t + 0x44);
        int32_t count = 0x14910;
        while (count-- > 0) {
            *clear_ptr++ = 0;
        }
    }

    /* Step 6: Re-initialise occupancy bitmap to all 1s */
    if (*(uint32_t**)(t + TILEMAP_OFF_OCCUPANCY) != NULL) {
        tiles_x = *(int16_t*)(t + TILEMAP_OFF_TILES_X);  /* +0x3E */
        tiles_y = *(int16_t*)(t + TILEMAP_OFF_TILES_Y);  /* +0x40 */
        total_tiles = (int32_t)tiles_x * (int32_t)tiles_y;
        bitmap_size = ((total_tiles + 7) >> 3) + 1;

        uint32_t* bitmap = *(uint32_t**)(t + TILEMAP_OFF_OCCUPANCY);
        uint32_t dword_count = bitmap_size >> 2;
        uint32_t byte_remainder = bitmap_size & 3;

        /* Fill dwords with 0xFFFFFFFF */
        while (dword_count-- > 0) {
            *bitmap++ = 0xFFFFFFFF;
        }
        /* Fill remaining bytes with 0xFF */
        {
            uint8_t* byte_ptr = (uint8_t*)bitmap;
            while (byte_remainder-- > 0) {
                *byte_ptr++ = 0xFF;
            }
        }
    }

    /* Step 7: Clear 65x81 matrix at TileMap+0x81 */
    {
        uint8_t* row_ptr = t + 0x81;
        int32_t row_count = 0x41;  /* 65 rows */

        do {
            int32_t col_count = 0x51;  /* 81 columns */
            uint8_t* col_ptr = row_ptr;
            do {
                col_ptr[-1] = 0xFF;
                *col_ptr = 0xFF;
                col_ptr += 0x1040;  /* stride between columns */
                col_count--;
            } while (col_count != 0);

            row_ptr += 0x40;  /* stride between rows */
            row_count--;
        } while (row_count != 0);
    }

    /* Step 8: Invalidate and redraw the main window */
    if (g_main_window != NULL) {
        void* hWnd = *(void**)((uint8_t*)g_main_window + 8);
        if (hWnd != NULL && g_game_mode != 1) {
            InvalidateRect(hWnd, NULL, 0);   /* no erase background */
            UpdateWindow(hWnd);
        }
    }
}
