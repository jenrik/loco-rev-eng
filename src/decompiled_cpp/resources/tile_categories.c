/**
 * tile_categories.c — Tile category classification helpers
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Simple predicates that inspect the tile type byte at offset +0x63A
 * (or object resource type at +0x08) to classify tiles/objects into
 * logical categories: road, building, water, track, scenery.
 *
 * Functions:
 *   RESDATA_IsRoadTile       — Returns 1 if tile type in {1,2,3,4}     (0x44BD10, 28 bytes)
 *   RESDATA_IsBuildingTile   — Returns 1 if tile type in {7,8,9,10}   (0x44BD30, 28 bytes)
 *   RESDATA_IsWaterTile      — Returns 1 if tile type in {0x0E,0x0F}  (0x44BD50, 20 bytes)
 *   RESDATA_IsTrackTile      — Returns 1 if tile type in {0x10,0x11}  (0x44BD70, 20 bytes)
 *   RESDATA_IsSceneryTile    — Returns 1 if tile type in {0x12,0x13}  (0x44BD90, 20 bytes)
 *   RESDATA_GetTileCategory  — Dispatch tile category matching query   (0x44BDB0, 151 bytes)
 *   RESDATA_IsValidTrackIndex —Validates track index field             (0x44BCD0, 63 bytes)
 *
 * Calling convention: All __fastcall (ECX=param1) except GetTileCategory
 * and IsValidTrackIndex which are __thiscall.
 */

#include "../shared/types.h"

/* ================================================================== */
/* Tile type constants (offsets into the tile descriptor object)       */
/* ================================================================== */

#define TILE_TYPE_ROAD_1        0x01
#define TILE_TYPE_ROAD_2        0x02
#define TILE_TYPE_ROAD_3        0x03
#define TILE_TYPE_ROAD_4        0x04

#define TILE_TYPE_BUILDING_1    0x07
#define TILE_TYPE_BUILDING_2    0x08
#define TILE_TYPE_BUILDING_3    0x09
#define TILE_TYPE_BUILDING_4    0x0A

#define TILE_TYPE_WATER_1       0x0E
#define TILE_TYPE_WATER_2       0x0F

#define TILE_TYPE_TRACK_1       0x10
#define TILE_TYPE_TRACK_2       0x11

#define TILE_TYPE_SCENERY_1     0x12
#define TILE_TYPE_SCENERY_2     0x13

/* ================================================================== */
/* RESDATA_IsRoadTile — Returns 1 if tile at +0x63A is a road piece   */
/* Address: 0x44BD10                                                   */
/* ================================================================== */
byte __fastcall RESDATA_IsRoadTile(int tileObj)
{
    byte tileType = *(byte*)(tileObj + 0x63A);

    if (tileType != TILE_TYPE_ROAD_1 && tileType != TILE_TYPE_ROAD_3 &&
        tileType != TILE_TYPE_ROAD_2 && tileType != TILE_TYPE_ROAD_4) {
        return 0;
    }
    return 1;
}

/* ================================================================== */
/* RESDATA_IsBuildingTile — Returns 1 if tile at +0x63A is a building */
/* Address: 0x44BD30                                                   */
/* ================================================================== */
byte __fastcall RESDATA_IsBuildingTile(int tileObj)
{
    byte tileType = *(byte*)(tileObj + 0x63A);

    if (tileType != TILE_TYPE_BUILDING_1 && tileType != TILE_TYPE_BUILDING_3 &&
        tileType != TILE_TYPE_BUILDING_2 && tileType != TILE_TYPE_BUILDING_4) {
        return 0;
    }
    return 1;
}

/* ================================================================== */
/* RESDATA_IsWaterTile — Returns 1 if tile at +0x63A is water         */
/* Address: 0x44BD50                                                   */
/* ================================================================== */
byte __fastcall RESDATA_IsWaterTile(int tileObj)
{
    byte tileType = *(byte*)(tileObj + 0x63A);

    if (tileType != TILE_TYPE_WATER_1 && tileType != TILE_TYPE_WATER_2) {
        return 0;
    }
    return 1;
}

/* ================================================================== */
/* RESDATA_IsTrackTile — Returns 1 if tile at +0x63A is track/rail    */
/* Address: 0x44BD70                                                   */
/* ================================================================== */
byte __fastcall RESDATA_IsTrackTile(int tileObj)
{
    byte tileType = *(byte*)(tileObj + 0x63A);

    if (tileType != TILE_TYPE_TRACK_1 && tileType != TILE_TYPE_TRACK_2) {
        return 0;
    }
    return 1;
}

/* ================================================================== */
/* RESDATA_IsSceneryTile — Returns 1 if tile at +0x63A is scenery     */
/* Address: 0x44BD90                                                   */
/*                                                                     */
/* Used by TileMap_IsTileOccupied for collision/pathing checks.       */
/* ================================================================== */
byte __fastcall RESDATA_IsSceneryTile(int tileObj)
{
    byte tileType = *(byte*)(tileObj + 0x63A);

    if (tileType != TILE_TYPE_SCENERY_1 && tileType != TILE_TYPE_SCENERY_2) {
        return 0;
    }
    return 1;
}

/* ================================================================== */
/* RESDATA_GetTileCategory — Dispatches tile type matching logic      */
/* Address: 0x44BDB0                                                   */
/*                                                                     */
/* Dispatches on the tile type byte at +0x63A:                         */
/*   0x01 = player check: returns 1 if param_1 != 0                    */
/*   0x03 = color delta check: returns 1 if param_2 matches            */
/*         (byte at +0x16C) - (byte at +0x169)                         */
/*   0x02 = player ID check: returns 1 if (byte at +0x16B + param_1)  */
/*         matches g_player_id                                         */
/*   0x04 = color match check: returns 1 if (short param_2 +          */
/*         byte at +0x16C) matches g_player_color                      */
/*                                                                     */
/* Used by TileMap_ScrollRect during viewport scrolling to determine  */
/* which tiles to scroll to.                                           */
/* ================================================================== */
uint __thiscall RESDATA_GetTileCategory(void* this, short param_1, ushort param_2)
{
    byte tileType = *(byte*)((char*)this + 0x63A);

    /* Type 0x01: Player presence check - succeeds if param_1 != 0 */
    if (tileType == 0x01) {
        if (param_1 != 0) {
            return 1;
        }
        return 0;
    }

    /* Type 0x03: Color delta check - compares param_2 with color offset */
    if (tileType == 0x03) {
        byte offsetDiff = *(byte*)((char*)this + 0x16C) -
                           *(char*)((char*)this + 0x169);
        if (param_2 == offsetDiff) {
            return 1;
        }
        return 0;
    }

    /* Type 0x02: Player ID offset check */
    if (tileType == 0x02) {
        int playerCheck = *(byte*)((char*)this + 0x16B) + (int)param_1;
        if (playerCheck == (int)g_player_id) {
            return 1;
        }
        return 0;
    }

    /* Type 0x04: Color match check */
    if (tileType == 0x04) {
        int colorCheck = (int)(short)param_2 + *(byte*)((char*)this + 0x16C);
        if (colorCheck == (int)g_player_color) {
            return 1;
        }
        return 0;
    }

    return 0;
}

/* ================================================================== */
/* RESDATA_IsValidTrackIndex — Validate current/alternate track index  */
/* Address: 0x44BCD0                                                   */
/*                                                                     */
/* Checks if the given track index is valid:                           */
/*   - 0 = "off" (no track)                                            */
/*   - matches current track at +0x636                                 */
/*   - matches current track + 1 (sequential)                          */
/*   - matches alternate track at +0x638                               */
/*                                                                     */
/* Returns 1 if valid, 0 if invalid.                                   */
/* ================================================================== */
uint __thiscall RESDATA_IsValidTrackIndex(void* this, short trackIdx)
{
    if (trackIdx == 0) {
        return 1;  /* 0 = off (always valid) */
    }

    ushort currentTrack = *(ushort*)((char*)this + 0x636);
    ushort alternateTrack = *(ushort*)((char*)this + 0x638);

    if ((ushort)trackIdx == currentTrack) {
        return 1;
    }

    if (alternateTrack == 0) {
        return 0;  /* No alternate track, only current is valid */
    }

    /* Check current+1 (sequential next track) */
    if ((ushort)trackIdx == currentTrack + 1) {
        return 1;
    }

    /* Check alternate track */
    if ((ushort)trackIdx == alternateTrack) {
        return 1;
    }

    return 0;
}

/* External globals referenced */
extern int g_player_id;        /* Global player ID */
extern int g_player_color;     /* Global player color */
