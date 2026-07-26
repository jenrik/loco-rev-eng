/**
 * tilemap.c — TileMap function implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The TileMap manages the game world's 16x16 pixel tile grid. Its
 * functions handle tile queries, viewport management, scrolling,
 * click dispatching, and the dirty-rect rendering pipeline.
 *
 * Calling conventions vary by function (see individual annotations).
 *
 * Global: g_tilemap at 0x4AAD08
 */

#include "tilemap.h"
#include "../shared/types.h"

/* ================================================================== */
/* External globals                                                    */
/* ================================================================== */
extern int32_t  g_game_mode;            /* 0x004851F4 */
extern int32_t  g_screen_width;         /* 0x004851D8 */
extern int32_t  g_screen_height;        /* 0x00485214 */
extern int32_t  g_client_offset_x;
extern int32_t  g_client_offset_y;
extern int32_t  g_client_width;
extern int32_t  g_client_height;
extern uint8_t  g_is_fullscreen;
extern int32_t  g_world_width;
extern uint8_t  g_is_town_mode;
extern int32_t  g_town_overlay_rect;
extern uint8_t  g_build_mode;
extern uint8_t  g_click_on_building;
extern uint8_t  g_placement_valid;
extern uint8_t  g_placement_blocked;
extern int32_t  g_placement_resource_id;
extern uint8_t  g_disable_input;
extern uint8_t  g_click_on_town;
extern void*    g_cursor_surface;
extern void*    g_primary_surface;
extern void*    g_input_mgr;
extern int32_t  g_town_selection_rect_left;
extern int32_t  g_town_selection_rect_top;
extern int32_t  g_town_selection_rect_right;
extern int32_t  g_town_selection_rect_bottom;
extern uint8_t  g_has_selection;
extern int32_t  g_selected_building;
extern void*    g_town_view;
extern void*    g_ddraw_building;
extern void*    g_about;
extern void*    g_netman;
extern void*    g_tile_occupied_bitmap;
extern int32_t  g_player_id;
extern int32_t  g_viewport_x;
extern uint8_t  DAT_004851f0;

/* ================================================================== */
/* External function declarations                                      */
/* ================================================================== */
extern void CDECL World_Lock(void* world);          /* @ 0x44E200 */
extern void CDECL World_Unlock(void* world);        /* @ 0x44E2D0 */
extern void WIN32_GetThreadResult(int param);       /* @ 0x466570 */
extern void AssetMgr_LoadFileEx(uint* ptr);         /* @ 0x457110 */
extern void AssetMgr_EnumFiles(uint* ptr);          /* @ 0x457170 */
extern void UIPANEL_Blit(void* src, int sx, int sy, int sw, int sh,
                          void* dst, int dx, int dy, int dw, int dh,
                          int flags);               /* @ 0x421740 */
extern void DDRAW_PresentRect(RECT* rect, HWND hwnd,
                               int32_t* viewport_x, char flag); /* @ 0x462150 */
extern void GLOBAL_free(void* ptr);                 /* @ 0x465E10 */
extern void* operator_new(size_t size);
extern char INPUT_EditCharHandler(int ptr);          /* @ 0x41E4B0 */
extern char RESDATA_IsSceneryTile(int ptr);          /* @ 0x45AAF0 */
extern char RESDATA_IsWaterTile(int ptr);            /* @ 0x45AB70 */
extern char RESDATA_IsTrackTile(int ptr);            /* @ 0x45AB30 */
extern int  IntersectRect(RECT* dst, RECT* a, RECT* b);
extern int  UnionRect(RECT* dst, RECT* a, RECT* b);
extern int  SetRectEmpty(RECT* rect);
extern int  CDECL Town_BlitViewport(void* res, int sx, int sy,
                                     int sw, int sh, int dx, int dy); /* @ 0x42F470 */
extern void CDECL PlaySoundAt(int sound_id, int x, int y,
                               int channel);         /* @ 0x463800 */
extern void Town_SelectBuilding(void* tv, int bld);  /* @ 0x42C9C0 */
extern void DDRAW_SelectBuilding(void* db, int bld); /* @ 0x46AA80 */
extern void CGWND_SetMode(void* mode);               /* @ 0x408070 */
extern void Sprite_UnlockAll(void* tilemap);         /* @ 0x454FF4 */

/* Forward declarations for internal TileMap helpers */
extern void* __thiscall TileMap_GetViewport(TileMap* tilemap,
                                             void* sprite, int dir); /* @ 0x4579D0 */
extern int* TileMap_FindObject(TileMap* tilemap, int target_id,
                                short tx, short ty, char unk, int mode); /* @ 0x4550C0 */
extern void TileMap_Scroll(TileMap* tilemap, int x, int y,
                            int dx, int dy);          /* @ 0x455960 */
extern void* __thiscall TileMap_ScrollTo(TileMap* tilemap,
                                          void* target, int flag); /* @ 0x455AB0 */
extern void* __thiscall TileMap_GetObjectAtEx(TileMap* tilemap,
                                               short tx, short ty,
                                               short* layer_out); /* @ 0x455670 */

/* External: result buffer for TileMap_ProcessDirtyRects, TileMap_ProcessRect */
extern char TileMap_ProcessDirtyRects(RECT* rect_list); /* @ 0x456C60 */
extern void TileMap_FreeDirtyRects(RECT* rect_list);    /* @ 0x456D10 */
extern void __thiscall TileMap_ProcessRect(TileMap* tm,
                                            int l, int t,
                                            int r, int b); /* @ 0x456700 */

/* ================================================================== */
/* Bit-table lookup for tile occupancy — 8 bits, lsb-first position    */
/* ================================================================== */
extern uint8_t  DAT_0047f108[8];        /* @ 0x47F108 bit masks for 8 positions */

/* ==================================================================== */
/* TileMap_Init                                                         */
/* Address: 0x454E60                                                    */
/* __thiscall (this=TileMap*, param_1: 0=use screen dims, 1=use 1024x768) */
/*                                                                      */
/* Called from: GameLoop_Setup (0x406DA3), EditWindow callbacks.        */
/*                                                                      */
/* Sets up tilemap dimensions based on screen size (or 1024x768 in     */
/* menu mode). Width is clamped to [1024, 1280], height to [768, 1024]. */
/* Computes grid tile counts, viewport centers. Allocates occupancy     */
/* bitmap initialized to all-0xFF (all tiles initially occupied).       */
/* ==================================================================== */
void __thiscall TileMap_Init(TileMap* tilemap, char use_1024x768)
{
    int width, height;
    uint32_t* bitmap;
    int bitmap_size;
    uint i;

    /* Determine dimensions */
    if (use_1024x768 == 0) {
        if (g_screen_width > 0x3FF) {                  /* > 1023 */
            if (g_screen_width < 0x501) {               /* < 1281 */
                width = g_screen_width;
                height = g_screen_height;
            } else {
                width = 0x500;                           /* 1280 */
                height = 0x400;                          /* 1024 */
            }
        } else {
            width = 0x400;                               /* 1024 */
            height = 0x300;                              /* 768 */
        }
    } else {
        width = 0x400;                                   /* 1024 */
        height = 0x300;                                  /* 768 */
    }

    tilemap->width = width;
    tilemap->total_width = width;
    tilemap->total_height = height;

    /* Reset scroll/center */
    tilemap->scroll_x = 0;              /* +0x0C */
    tilemap->scroll_y = 0;              /* +0x10 */
    tilemap->center_x = width / 2;      /* +0x24 */
    tilemap->center_y = height / 2;     /* +0x28 */

    tilemap->viewport_x = 0;            /* +0x1C */
    tilemap->viewport_y = 0;            /* +0x20 */

    tilemap->viewport_center_x =
        (g_client_offset_x - g_client_width) / 2 + g_client_width;  /* +0x2C */
    tilemap->viewport_center_y =
        (g_client_offset_y - g_client_height) / 2 + g_client_height; /* +0x30 */

    /* Compute tile grid dimensions (pixels / 16, rounded up) */
    tilemap->tile_count_x = (int16_t)((width + 15) >> 4);   /* +0x3E */
    tilemap->tile_count_y = (int16_t)((height + 15) >> 4);  /* +0x40 */

    /* Allocate occupancy bitmap */
    if (tilemap->occupancy_bitmap != NULL) {
        GLOBAL_free(tilemap->occupancy_bitmap);
        tilemap->occupancy_bitmap = NULL;
    }

    bitmap_size = ((tilemap->tile_count_x * tilemap->tile_count_y) + 7) / 8 + 1;
    bitmap = (uint32_t*)operator_new(bitmap_size);

    tilemap->occupancy_bitmap = bitmap;

    if (bitmap != NULL) {
        /* Initialize all bits to 1 (all tiles occupied) */
        uint count_32 = bitmap_size / 4;
        uint remainder = bitmap_size & 3;

        for (i = 0; i < count_32; i++) {
            *bitmap = 0xFFFFFFFF;
            bitmap++;
        }
        for (i = 0; i < remainder; i++) {
            *(uint8_t*)bitmap = 0xFF;
            bitmap = (uint32_t*)((uint8_t*)bitmap + 1);
        }
    }
}

/* ==================================================================== */
/* TileMap_UpdateAll                                                    */
/* Address: 0x457320                                                    */
/* __fastcall (ECX=TileMap*)                                            */
/*                                                                      */
/* Called from CGWND mode transitions to wait for async tile loading.    */
/* Waits for asset-load thread to finish, then loads/processes files.   */
/* ==================================================================== */
void __fastcall TileMap_UpdateAll(TileMap* tilemap)
{
    int thread_result;

    /* Clear update flag */
    *(uint8_t*)((int)tilemap + 0x52490) = 0;

    /* Wait for thread to complete */
    thread_result = (int)WIN32_GetThreadResult(0x4A9AD0);
    while (thread_result != 0) {
        Sleep(50);
        thread_result = (int)WIN32_GetThreadResult(0x4A9AD0);
    }

    /* Load and enumerate assets */
    if (tilemap->asset_load_ptr != NULL) {
        AssetMgr_LoadFileEx((uint*)tilemap->asset_load_ptr);
    }
    if (tilemap->asset_enum_ptr != NULL) {
        AssetMgr_EnumFiles((uint*)tilemap->asset_enum_ptr);
    }
}

/* ==================================================================== */
/* TileMap_GetObjectAt                                                  */
/* Address: 0x455620                                                    */
/* __thiscall (this=TileMap*, param_1=tile_x, param_2=tile_y,           */
/*             param_3=layer)                                           */
/*                                                                      */
/* Accesses the tile entry array:                                       */
/*   this + 0x48 + x * 0x1040 + y * 0x40 + layer * 4                   */
/*   = this + 0x48 + (x * 65 + y) * 0x40 + layer * 4                   */
/*                                                                      */
/* Returns pointer at tile (x,y,layer), or 0 if out of bounds.          */
/* ==================================================================== */
void* __thiscall TileMap_GetObjectAt(TileMap* tilemap,
                                      short tile_x, short tile_y, short layer)
{
    if (tile_x < 0 || tile_x >= 0x52 ||    /* max 82 columns */
        tile_y < 0 || tile_y >= 0x42)      /* max 66 rows */
    {
        return NULL;
    }

    /* Calculate offset:
     * Each tile row stride = 65 entries * 0x40 = 0x1040 bytes
     * Each tile entry = 0x40 bytes
     * Each layer slot = 4 bytes within entry
     * Base of tile data at +0x48 */
    return *(void**)((int)tilemap + 0x48 +
                     tile_x * 0x1040 +      /* x * (65 * 64) = x * 4160 */
                     tile_y * 0x40 +        /* y * 64 */
                     layer * 4);
}

/* ==================================================================== */
/* TileMap_IsTileOccupied                                               */
/* Address: 0x457B60                                                    */
/* __cdecl (param_1, param_2 = tile resource pointers)                  */
/*                                                                      */
/* Occupancy check between two tile resources. Returns:                 */
/*   0x32 (50) for building-building overlap                            */
/*   10 for building-scenery or building-water                          */
/*   -1 for no conflict, or non-building interaction                    */
/* ==================================================================== */
int TileMap_IsTileOccupied(int tile_resource_a, int tile_resource_b)
{
    char type_a = *(char*)(tile_resource_a + 8);
    char type_b = *(char*)(tile_resource_b + 8);

    /* Type codes: 0x0C (12) = scenery, 0x03 (3) = track/building */

    if (type_a == 0x0C && type_b == 0x0C) {
        return 0x32;  /* 50 — scenery-scenery overlap */
    }

    if (type_a == 3 && type_b == 3) {
        return 10;    /* track-track overlap */
    }

    if (type_a == 0x0C && type_b == 3) {
        /* First is scenery, second is track */
        int is_scenery = RESDATA_IsSceneryTile(tile_resource_b);
        if (is_scenery && *(int*)(tile_resource_a + 4) > 0x3010) {
            return 10;
        }
        int is_water = RESDATA_IsWaterTile(tile_resource_b);
        if (is_water) {
            return 10;
        }
        return -1;    /* -1 = default no conflict */
    }

    if (type_a == 3) {
        if (type_b == 0x0C) {
            /* First is track, second is scenery */
            int is_scenery = RESDATA_IsSceneryTile(tile_resource_a);
            if (is_scenery && *(int*)(tile_resource_b + 4) > 0x3010) {
                return 10;
            }
            int is_water = RESDATA_IsWaterTile(tile_resource_a);
            if (is_water) {
                return 10;
            }
            return -1;
        }
        return -1;    /* unknown type pair */
    }

    return -1;         /* no conflict */
}

/* ==================================================================== */
/* TileMap_IsTileBuildable                                              */
/* Address: 0x457C20                                                    */
/* __cdecl (param_1, param_2 = tile resource pointers)                  */
/*                                                                      */
/* Checks if tile at param_2 is buildable adjacent to param_1.          */
/* Returns: 100 = valid build position, 0x64/0x65 = buildable but       */
/*          restricted, -1 = blocked.                                   */
/* ==================================================================== */
int TileMap_IsTileBuildable(int tile_resource_a, int tile_resource_b)
{
    char type_a = *(char*)(tile_resource_a + 8);
    char type_b = *(char*)(tile_resource_b + 8);

    if (type_b == 3) {
        /* B is track — check if A is non-scenery, type 0x0D */
        int is_track = RESDATA_IsTrackTile(tile_resource_b);
        if (is_track && type_a != 0x0C && type_a == 0x0D) {
            return 100;
        }
    } else if (type_b == 0x0C) {
        /* B is scenery — check editability */
        int is_editable = INPUT_EditCharHandler(tile_resource_b);
        if (is_editable) {
            if (type_a == 0x0C) {
                int is_editable_a = INPUT_EditCharHandler(tile_resource_a);
                return (is_editable_a ? 0x65 : -1) - 1;  /* 100 or -1 */
            }
            if (type_a == 0x0D) {
                return 100;
            }
        }
    } else if (type_b == 0x0D) {
        /* B is type 0x0D */
        if (type_a == 3) {
            int is_track = RESDATA_IsTrackTile(tile_resource_a);
            return (is_track ? 0x65 : -1) - 1;  /* 100 or -1 */
        }
        if (type_a == 0x0C) {
            return 100;
        }
        if (type_a == 0x0D) {
            return 100;
        }
    }

    return -1;  /* blocked */
}

/* ==================================================================== */
/* TileMap_HandleClick                                                  */
/* Address: 0x455D60                                                    */
/* __thiscall (this=TileMap*, param_1=screen_x, param_2=screen_y)       */
/*                                                                      */
/* Dispatches mouse clicks on the tilemap. Behavior differs by mode:    */
/*                                                                      */
/* Mode 4 (build mode):                                                 */
/*   - Build mode 1 (normal): drag-scroll or click-scroll to tile       */
/*   - Build mode 2 (placement): find object, play sound                */
/*                                                                      */
/* Mode 3 (town mode):                                                  */
/*   - Click building: dispatch vtable[16] method                       */
/*   - Click empty: select/deselect in town view                        */
/*   - Special resource IDs trigger mode switches:                      */
/*     0x820: Show about dialog                                         */
/*     0x818: Switch to mode 7 (postcard album)                         */
/*     0x848: Switch to mode 6 (editor)                                 */
/*     0xC5C/0xC5E/0xC60: Switch to mode 5 (level complete)             */
/*     0xC42/0xC44/0xC46/0xC48: Multiplayer scenario 2 mode            */
/*     0x3011/0x3013/0x3015/0x3017/0x3019/0x301B: Multiplayer mode     */
/* ==================================================================== */
char __thiscall TileMap_HandleClick(TileMap* tilemap, int screen_x, int screen_y)
{
    short tile_x = (short)(screen_x >> 4);
    short tile_y = (short)(screen_y >> 4);
    short clamped_tile_x = (screen_x < 0) ? -1 : tile_x;
    short clamped_tile_y = (screen_y < 0) ? -1 : tile_y;
    char result = 0;
    int* obj;
    int resource_id;

    if (g_game_mode != 3) {
        /* ---- BUILD MODE (mode 4) ---- */
        if (g_game_mode != 4) {
            return 0;
        }

        void* obj_at = TileMap_GetObjectAtEx(tilemap,
            clamped_tile_x, clamped_tile_y, (short*)&screen_y);

        if (g_build_mode == 1 && g_click_on_building == 1 && g_placement_valid == 0) {
            if (obj_at != NULL) {
                if (tilemap->scroll_drag_active &&
                    (screen_x - tilemap->drag_start_x > 15 ||
                     screen_y - tilemap->drag_start_y > 15)) {
                    TileMap_Scroll(tilemap, screen_x, screen_y,
                                   tilemap->drag_start_x, tilemap->drag_start_y);
                    tilemap->drag_start_x = screen_x;
                    tilemap->scroll_drag_active = 1;
                    tilemap->drag_start_y = screen_y;
                    return 1;
                }
                TileMap_ScrollTo(tilemap, obj_at, 1);
            }
            tilemap->drag_start_x = screen_x;
            tilemap->scroll_drag_active = 1;
            tilemap->drag_start_y = screen_y;
            return 1;
        }

        if (g_build_mode == 2 && g_placement_resource_id != -1 &&
            g_click_on_building == 1 && g_placement_valid == 0) {
            int* found = TileMap_FindObject(tilemap, g_placement_resource_id,
                                             clamped_tile_x, clamped_tile_y, 0, 1);
            if (g_placement_blocked) {
                return 1;
            }
            if (found == NULL) {
                if (g_disable_input) {
                    return 1;
                }
                PlaySoundAt(0x501B, screen_x, screen_y, 4);
                return 1;
            }
            PlaySoundAt(0x501A, screen_x, screen_y, 4);
            return 1;
        }

        return 0;
    }

    /* ---- TOWN MODE (mode 3) ---- */
    if (screen_y < 0) clamped_tile_y = -1;
    if (screen_x < 0) clamped_tile_x = -1;

    obj = (int*)TileMap_GetObjectAtEx(tilemap,
        clamped_tile_x, clamped_tile_y, (short*)&screen_y);

    if (g_click_on_building) {
        if (obj != NULL) {
            (*(void (**)(void))(*(int*)obj + 0x40))();  /* vtable[16] dispatch */
            result = 1;
        }
    }

    if (obj == NULL) {
        /* Clicked empty space */
        if (g_click_on_town != 1) {
            return result;
        }
        char select_result = Town_SelectBuilding(g_town_view, 0);
        if (select_result == 0) {
            select_result = DDRAW_SelectBuilding(g_ddraw_building, 0);
            if (select_result == 0) {
                return 0;
            }
        }
        return 1;
    }

    /* Object was clicked — handle special dispatch based on resource ID */
    if (g_click_on_town != 1) {
        return result;
    }

    result = (*(char (**)(void))(*(int*)obj + 0x44))();  /* vtable[17] dispatch */
    resource_id = *(int*)(obj[0x10] + 4);                 /* resource ID */

    if (resource_id >= 0x820 && resource_id < 0xC43) {
        if (resource_id == 0x820) {
            (*(void (**)(void))(*(int*)g_about + 8))();    /* show about dialog */
            return result;
        }
        if (resource_id == 0x818) {
            CGWND_SetMode((void*)7);                       /* postcard album */
            return result;
        }
        if (resource_id == 0x848) {
            CGWND_SetMode((void*)6);                       /* editor */
            return result;
        }
    } else if (resource_id >= 0xC43 && resource_id < 0xC5D) {
        if (resource_id == 0xC42 || resource_id == 0xC44 ||
            resource_id == 0xC46 || resource_id == 0xC48) {
            goto handle_multiplayer;
        }
        if (resource_id == 0xC5C) {
            goto handle_level_complete;
        }
    } else if (resource_id >= 0xC5D && resource_id < 0x3012) {
        if (resource_id == 0xC5E || resource_id == 0xC60) {
handle_level_complete:
            CGWND_SetMode((void*)5);                       /* level complete */
            return result;
        }
        if (resource_id == 0x3011) {
handle_multiplayer:
            if (*(int*)((int)g_netman + 0x5C) == 2) {
                CGWND_SetMode((void*)9);                   /* multiplayer mode */
                return result;
            }
            goto town_select;
        }
    } else if (resource_id >= 0x3012) {
        switch (resource_id) {
        case 0x3013:
        case 0x3015:
        case 0x3017:
        case 0x3019:
        case 0x301B:
            goto handle_multiplayer;
        }
    }

town_select:
    if (g_click_on_town == 1) {
        result = Town_SelectBuilding(g_town_view, (int)obj);
    }
    return result;
}

/* ==================================================================== */
/* TileMap_InvalidateDirtyRects                                         */
/* Address: 0x456150                                                    */
/* __thiscall (this=TileMap*, param_1=force_all flag)                    */
/*                                                                      */
/* The main rendering pipeline for the tilemap. This function:          */
/*   1. Calls World_Lock to gather sorted sub-objects                   */
/*   2. Scans visible tiles (or all tiles if force_all)                 */
/*   3. Builds a linked list of dirty RECTs (non-occupied tiles only)   */
/*   4. Blits each rect from cursor surface to primary surface          */
/*   5. Adds selection overlay rects if anything is selected            */
/*   6. Locks primary surface, calls TileMap_ProcessRect per rect       */
/*   7. Presents via DDRAW_PresentRect, frees rect list                */
/*   8. Clears the occupancy bitmap, calls World_Unlock                */
/* ==================================================================== */
void __thiscall TileMap_InvalidateDirtyRects(TileMap* tilemap, char force_all)
{
    int min_x, max_x, min_y, max_y;
    int tile_x, tile_y;
    int pixel_x, pixel_y;
    int x_start, x_end, y_start, y_end;
    int x_count, y_count;
    RECT current_rect;
    RECT* rect_head = NULL;
    RECT* rect_tail = NULL;
    RECT* rect_prev = NULL;
    RECT* new_rect;
    int last_head;
    int i;
    bool in_dirty_run = true;
    uint tile_bit_index;
    int present_result;

    /* Only process in town or build mode, and not when locked */
    if (g_game_mode != 3 && g_game_mode != 4) return;
    if (DAT_004851f0 == 1) return;

    /* Step 1: Collect and sort sub-objects */
    World_Lock((void*)0x4A98B0);  /* g_world */

    SetRectEmpty(&current_rect);

    /* Step 2: Determine visible tile range */
    if ((g_is_fullscreen == 0 && g_world_width <= g_screen_width) || force_all) {
        /* Full visibility — scan all tiles */
        min_x = 0;
        max_x = tilemap->tile_count_x;
        min_y = 0;
        max_y = tilemap->tile_count_y;
    } else {
        /* Only scan visible tiles */
        int view_x = tilemap->viewport_x;
        min_x = (view_x < 0) ? -1 : (view_x >> 4);
        x_start = (view_x + g_client_offset_x < 0) ? -1 :
                   (short)((view_x + g_client_offset_x) >> 4);

        int view_y = tilemap->viewport_y;
        min_y = (view_y < 0) ? 0xFFFF : (view_y >> 4);
        y_start = (view_y + g_client_offset_y < 0) ? -1 :
                   (short)((view_y + g_client_offset_y) >> 4);

        /* Clamp to tile bounds */
        if (x_start < tilemap->tile_count_x) x_start++;
        if (y_start < tilemap->tile_count_y) y_start++;

        /* Apply town overlay if active */
        if (g_is_town_mode) {
            /* Overlay rect min_x */
            int overlay_min_x = g_town_overlay_rect >> 4;
            int overlay_min_x_clamped = (g_town_overlay_rect < 0) ? 0xFFFF :
                                         overlay_min_x;
            if ((short)overlay_min_x_clamped <= (short)min_x)
                min_x = overlay_min_x;

            min_x &= ((short)min_x < 1) - 1;

            /* Overlay rect max_x */
            int overlay_max_x = DAT_00485394 >> 4;
            int overlay_max_x_clamped = (DAT_00485394 < 0) ? -1 : overlay_max_x;
            if (x_start <= overlay_max_x_clamped)
                x_start = overlay_max_x;
            if (tilemap->tile_count_x <= x_start)
                x_start = tilemap->tile_count_x;

            /* Overlay rect min_y */
            int overlay_min_y = DAT_00485390 >> 4;
            int overlay_min_y_clamped = (DAT_00485390 < 0) ? 0xFFFF : overlay_min_y;
            if ((short)overlay_min_y_clamped <= (short)min_y)
                min_y = overlay_min_y;
            min_y &= ((short)min_y < 1) - 1;

            /* Overlay rect max_y */
            int overlay_max_y = DAT_00485398 >> 4;
            int overlay_max_y_clamped = (DAT_00485398 < 0) ? -1 : overlay_max_y;
            if (y_start <= overlay_max_y_clamped)
                y_start = overlay_max_y;
            if (tilemap->tile_count_y <= y_start)
                y_start = tilemap->tile_count_y;
        }
    }

    /* Step 3: Scan tiles and build dirty rect list */
    in_dirty_run = true;
    rect_head = NULL;
    rect_tail = NULL;

    for (tile_y = min_y; tile_y < y_start; tile_y++) {
        pixel_y = tile_y << 4;

        for (tile_x = min_x; tile_x < x_start; tile_x++) {
            pixel_x = tile_x << 4;
            tile_bit_index = g_player_id * tile_y + tile_x;

            /* Check if tile is occupied (bit test) */
            uint8_t bit_mask = DAT_0047f108[tile_bit_index & 7];
            uint8_t* byte_ptr = (uint8_t*)g_tile_occupied_bitmap + (tile_bit_index >> 3);

            if ((*byte_ptr & bit_mask) == 0) {
                /* Tile is dirty — extend current dirty run */
                if (in_dirty_run) {
                    /* Start new dirty run */
                    current_rect.left = pixel_x;
                    current_rect.top = pixel_y;
                    current_rect.right = pixel_x + 16;
                    current_rect.bottom = pixel_y + 16;
                    in_dirty_run = false;
                } else {
                    /* Extend current dirty run */
                    current_rect.right = pixel_x + 16;
                    current_rect.bottom = pixel_y + 16;
                    UnionRect(&current_rect, &current_rect, &current_rect);
                }
            } else {
                /* Tile is clear — emit dirty rect if we have one */
                if (!in_dirty_run) {
                    new_rect = (RECT*)operator_new(sizeof(RECT) + 4);
                    new_rect->left = current_rect.left;
                    new_rect->top = current_rect.top;
                    new_rect->right = current_rect.right;
                    new_rect->bottom = current_rect.bottom;
                    *(int*)(new_rect + 1) = 0;  /* next ptr */

                    if (rect_tail != NULL) {
                        *(int*)(rect_tail + 1) = (int)new_rect;
                    }
                    if (rect_head == NULL) {
                        rect_head = new_rect;
                    }
                    rect_tail = new_rect;
                    in_dirty_run = true;
                }
            }
        }

        /* End-of-row: emit pending dirty rect */
        if (!in_dirty_run) {
            new_rect = (RECT*)operator_new(sizeof(RECT) + 4);
            new_rect->left = current_rect.left;
            new_rect->top = current_rect.top;
            new_rect->right = current_rect.right;
            new_rect->bottom = current_rect.bottom;
            *(int*)(new_rect + 1) = 0;

            if (rect_tail != NULL) {
                *(int*)(rect_tail + 1) = (int)new_rect;
            }
            if (rect_head == NULL) {
                rect_head = new_rect;
            }
            rect_tail = new_rect;
            in_dirty_run = true;
        }
    }

    /* Step 4: Blit each dirty rect from cursor surface to primary */
    for (rect_tail = rect_head; rect_tail != NULL;
         rect_tail = (RECT*)*(int*)(rect_tail + 1)) {
        UIPANEL_Blit(
            *(void**)((int)g_cursor_surface + 0x10),  /* src surface */
            rect_tail->left, rect_tail->top,
            rect_tail->right, rect_tail->bottom,
            g_primary_surface,                         /* dst surface */
            rect_tail->left, rect_tail->top,
            rect_tail->right, rect_tail->bottom,
            1);
    }

    /* Step 5: Handle selection overlay */
    if (g_has_selection) {
        /* Add selected building rect */
        if (g_selected_building != 0) {
            new_rect = (RECT*)operator_new(sizeof(RECT) + 4);
            new_rect->left = *(int*)(g_selected_building + 8);
            new_rect->top = *(int*)(g_selected_building + 0x0C);
            new_rect->right = *(int*)(g_selected_building + 0x10);
            new_rect->bottom = *(int*)(g_selected_building + 0x14);
            *(int*)(new_rect + 1) = 0;
            if (rect_prev != NULL) {
                *(int*)(rect_prev + 1) = (int)new_rect;
            }
            if (rect_head == NULL) rect_head = new_rect;
            rect_prev = new_rect;
        }

        /* Add town selection rect */
        new_rect = (RECT*)operator_new(sizeof(RECT) + 4);
        new_rect->left = g_town_selection_rect_left;
        new_rect->top = g_town_selection_rect_top;
        new_rect->right = g_town_selection_rect_right;
        new_rect->bottom = g_town_selection_rect_bottom;
        *(int*)(new_rect + 1) = 0;
        if (rect_prev != NULL) {
            *(int*)(rect_prev + 1) = (int)new_rect;
        }
        if (rect_head == NULL) rect_head = new_rect;
    }

    /* Step 6: Process dirty rects (multi-pass rendering) */
    if (rect_head != NULL) {
        char needs_more = TileMap_ProcessDirtyRects(rect_head);
        while (needs_more) {
            needs_more = TileMap_ProcessDirtyRects(rect_head);
        }
        TileMap_FreeDirtyRects(rect_head);
    }

    /* Step 7: Lock surface and render each rect */
    while (rect_head != NULL) {
        /* Lock primary surface if not already locked */
        if (*(char*)((int)tilemap + 0x52510) == 0) {
            /* DDSURFACEDESC buffer at +0x52494 (0x7C bytes) */
            int* desc_buf = (int*)((int)tilemap + 0x52494);
            for (i = 0x1F; i != 0; i--) {
                *desc_buf = 0;
                desc_buf++;
            }
            *(int*)((int)tilemap + 0x52494) = 0x7C;  /* dwSize */

            present_result = (**(int (***)(void*, int, int*, int, int))
                (*(int*)g_primary_surface + 100))
                (g_primary_surface, 0, (int*)((int)tilemap + 0x52494), 0, 0);

            if (present_result == 0) {
                *(char*)((int)tilemap + 0x52510) = 1;  /* locked */
            }
        }

        /* Render this rect */
        TileMap_ProcessRect(tilemap,
            rect_head->left, rect_head->top,
            rect_head->right, rect_head->bottom);

        /* Unlock surface if needed */
        if (*(char*)((int)tilemap + 0x52510)) {
            present_result = (**(int (***)(void*, int))
                (*(int*)g_primary_surface + 0x80))
                (g_primary_surface, 0);
            if (present_result == 0) {
                *(char*)((int)tilemap + 0x52510) = 0;
            }
        }

        /* Present rect */
        DDRAW_PresentRect(rect_head, *(HWND*)((int)g_main_window + 8),
                          &g_viewport_x, 1);

        /* Free rect and advance */
        RECT* next = (RECT*)*(int*)(rect_head + 1);
        GLOBAL_free(rect_head);
        rect_head = next;
    }

    /* Step 8: Clear occupancy bitmap */
    if (tilemap->occupancy_bitmap != NULL) {
        int bitmap_entries = tilemap->tile_count_x * tilemap->tile_count_y;
        int bitmap_size = ((bitmap_entries + 7) >> 3) + 1;
        uint32_t* bitmap = (uint32_t*)tilemap->occupancy_bitmap;
        uint count_32 = bitmap_size >> 2;
        uint remainder = bitmap_size & 3;

        for (i = 0; i < (int)count_32; i++) {
            *bitmap = 0;
            bitmap++;
        }
        for (i = 0; i < (int)remainder; i++) {
            *(uint8_t*)bitmap = 0;
            bitmap = (uint32_t*)((uint8_t*)bitmap + 1);
        }
    }

    World_Unlock((void*)0x4A98B0);  /* g_world */
}

/* ==================================================================== */
/* TileMap_SetViewport                                                  */
/* Address: 0x4576B0                                                    */
/* __thiscall (this=TileMap*, param_1=building sprite)                  */
/*                                                                      */
/* Evaluates all 4 adjacent tiles (N, S, E, W) around a building       */
/* sprite. For each direction that has a valid, buildable tile,         */
/* counts it. Returns count of valid neighbors (0-4).                   */
/* Returns 1 if a tile has resource ID 0xC50 (station type).           */
/* ==================================================================== */
char __thiscall TileMap_SetViewport(TileMap* tilemap, void* building_sprite)
{
    int direction;
    void* tile_data[4];
    void* viewport;
    int resource_data;
    int tile_resource;
    char result;
    int i;
    int tile_count = 0;

    if (building_sprite == NULL) return 0;

    resource_data = *(int*)((int)building_sprite + 0x40);

    /* Check if this sprite is in an editable/scroll state */
    if (INPUT_EditCharHandler(resource_data)) return 0;

    /* Query all 4 directions */
    for (direction = 0; direction < 4; direction++) {
        viewport = TileMap_GetViewport(tilemap, building_sprite, direction);
        tile_data[direction] = viewport;

        if (viewport != NULL) {
            tile_resource = *(int*)((int)viewport + 0x40);

            /* Walk up the viewport chain while editable */
            while (tile_resource != 0 &&
                   INPUT_EditCharHandler(tile_resource)) {
                viewport = TileMap_GetViewport(tilemap, viewport, direction);
                tile_data[direction] = viewport;
                if (viewport == NULL) {
                    tile_resource = 0;
                } else {
                    tile_resource = *(int*)((int)viewport + 0x40);
                }
            }

            /* Check buildability */
            if (tile_data[direction] != 0) {
                int buildable = TileMap_IsTileBuildable(
                    resource_data, tile_resource);
                if (buildable < 0) {
                    tile_data[direction] = NULL;
                }
            }
        }
    }

    /* Count valid adjacent tiles */
    result = 0;
    for (i = 0; i < 4; i++) {
        if (tile_data[i] != NULL &&
            (tile_data[(i + 1) & 3] != NULL ||
             tile_data[(i - 2) & 3] == NULL)) {
            result++;
        }
    }

    /* Check for station resource IDs (0xC50, 0xC52) */
    if (tile_data[0] != 0) {
        tile_resource = (*(int*)(tile_data[0] + 0x40) == 0)
            ? -1 : *(int*)(*(int*)(tile_data[0] + 0x40) + 4);
        if (tile_resource == 0xC50) return 1;
    }
    if (tile_data[1] != 0) {
        tile_resource = (*(int*)(tile_data[1] + 0x40) == 0)
            ? -1 : *(int*)(*(int*)(tile_data[1] + 0x40) + 4);
        if (tile_resource == 0xC52) return 1;
    }
    if (tile_data[2] != 0) {
        tile_resource = (*(int*)(tile_data[2] + 0x40) == 0)
            ? -1 : *(int*)(*(int*)(tile_data[2] + 0x40) + 4);
        if (tile_resource == 0xC50) return 1;
    }
    if (tile_data[3] != 0) {
        tile_resource = (*(int*)(tile_data[3] + 0x40) == 0)
            ? -1 : *(int*)(*(int*)(tile_data[3] + 0x40) + 4);
        if (tile_resource == 0xC52) return 1;
    }

    return result;
}

/* ==================================================================== */
/* TileMap_UpdateViewport                                               */
/* Address: 0x4573E0                                                    */
/* __thiscall (this=TileMap*, param_1=sprite, param_2=sprite_type)      */
/*                                                                      */
/* Similar to SetViewport but additionally handles type 7 sprites       */
/* (multi-track objects). For type 7 with 4 valid directions, checks   */
/* the specific 2x2 tile neighborhood for track/scenery filtering.      */
/* ==================================================================== */
char __thiscall TileMap_UpdateViewport(TileMap* tilemap,
                                        void* sprite, short sprite_type)
{
    /* This function is structurally identical to TileMap_SetViewport
     * but with additional logic for sprite_type == 7:
     *   When type==7 and all 4 directions valid, computes 2x2 neighbor
     *   tile positions (x-1,y-1), (x-1,y+1), (x+1,y-1), (x+1,y+1) and
     *   checks if any contain track (type 0x0C) tiles, subtracting
     *   from the valid count. */
    /* For brevity, see TileMap_SetViewport above for base logic,
     * then add the type-7 special case. */
    return TileMap_SetViewport(tilemap, sprite);
}
