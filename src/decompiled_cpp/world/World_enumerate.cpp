// Status: INTEGRATED
/**
 * World_enumerate.cpp - Post-load asset enumeration for the TileMap
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * World_EnumeratePostLoadAssets (0x457380) - Enumerates post-load asset
 * categories after the main world load is complete. Despite the Ghidra
 * "World_" prefix, the sole call site (the mode-3 loading worker thread
 * spawned by CGWND_EnterMode3 at 0x408950, body at 0x42CC60) passes
 * g_tilemap (0x4AAD08) in ECX; the function operates on the TileMap's
 * asset-loading fields at +0x52488/+0x5248C/+0x52490, which match
 * TileMap::asset_load_ptr / asset_enum_ptr / update_complete exactly.
 */

#include "../world/tilemap.h"
#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern int32_t g_game_mode;            /* 0x004851F4 */

/* Declared in resources/AssetMgr.h — replicated here instead of including
   that header because its extern "C" operator_new/GLOBAL_free/OutputDebug
   declarations conflict with tilemap.h's C++-linkage ones. */
extern void __fastcall AssetMgr_EnumerateCategory(uint32_t* param_1); /* @ 0x45D560 */

/* ================================================================== */
/* World_EnumeratePostLoadAssets                                        */
/* Address: 0x457380                                                    */
/* __fastcall (TileMap* in ECX)                                         */
/*                                                                      */
/* Enumerates post-load asset categories if game_mode == 3 (town).     */
/* Iterates two asset category lists on the TileMap at +0x52488 and    */
/* +0x5248C, passing each to AssetMgr_EnumerateCategory. Then sets     */
/* the update_complete flag at +0x52490 to 1.                          */
/*                                                                      */
/* Called by: mode-3 loading worker thread (0x42CC7C, g_tilemap)       */
/* ================================================================== */
void __fastcall World_EnumeratePostLoadAssets(TileMap* tilemap)
{
    if (g_game_mode != 3) {
        return;
    }

    /* Enumerate first asset category list (asset_load_ptr, +0x52488) */
    uint32_t* cat_a = static_cast<uint32_t*>(tilemap->asset_load_ptr);
    if (cat_a != nullptr && *cat_a != 0) {
        AssetMgr_EnumerateCategory(cat_a);
    }

    /* Second enumeration (only in town mode) */
    if (g_game_mode == 3) {
        uint32_t* cat_b = static_cast<uint32_t*>(tilemap->asset_enum_ptr);
        if (cat_b != nullptr && *cat_b != 0) {
            AssetMgr_EnumerateCategory(cat_b);
        }

        /* Mark asset enumeration as complete */
        tilemap->update_complete = 1;
    }
}
