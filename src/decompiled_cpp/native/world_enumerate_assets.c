/**
 * world_enumerate_assets.c — Post-load asset enumeration for World
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C free function, __fastcall. Called during world loading sequence to
 * enumerate asset categories after the main load is complete.
 */

#include "../world/world.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern int32_t g_game_mode;            /* 0x004851F4 */
extern void __fastcall AssetMgr_EnumerateCategory(uint32_t** category);

/* ================================================================== */
/* World_EnumeratePostLoadAssets                                      */
/* Address: 0x457380                                                   */
/* __fastcall (ECX=world_ptr = World*)                                 */
/*                                                                     */
/* Enumerates post-load asset categories if game_mode == 3 (town).     */
/* Iterates two asset category lists at the World struct at offsets    */
/* +0x52488 and +0x5248C, passing each to AssetMgr_EnumerateCategory.  */
/* Then sets the "assets_enumerated" flag at +0x52490 to 1.           */
/*                                                                     */
/* Called by: World loading sequence                                   */
/* ================================================================== */
void __fastcall World_EnumeratePostLoadAssets(void* world_ptr)
{
    if (g_game_mode != 3) {
        return;
    }

    /* Enumerate first asset category list */
    uint32_t** cat_a = *(uint32_t***)((uint8_t*)world_ptr + 0x52488);
    if (cat_a != NULL && *cat_a != 0) {
        AssetMgr_EnumerateCategory(cat_a);
    }

    /* Second enumeration (only in town mode) */
    if (g_game_mode == 3) {
        uint32_t** cat_b = *(uint32_t***)((uint8_t*)world_ptr + 0x5248C);
        if (cat_b != NULL && *cat_b != 0) {
            AssetMgr_EnumerateCategory(cat_b);
        }

        /* Mark asset enumeration as complete */
        *(uint8_t*)((uint8_t*)world_ptr + 0x52490) = 1;
    }
}
