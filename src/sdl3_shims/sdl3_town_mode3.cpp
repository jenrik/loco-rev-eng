/**
 * sdl3_town_mode3.cpp — SDL host glue for mode-3 town gameplay
 *
 * Lego Loco (loco.exe, 1998, MSVC x86) — host-only deviation (#ifndef _WIN32)
 *
 * This file provides:
 *   1. Heap construction of GameView (g_town_view) and DDRAW_Building
 *      (g_ddraw_building) singletons, assigned to the legacy void* globals.
 *   2. Transcribed implementations of Town_TrackBuilding (0x42D1A0) and
 *      DDRAW_UpdateBuilding (0x459DA0) validated against Ghidra disassembly.
 *
 * Every address annotation refers to the original loco.exe binary.
 */

#ifndef _WIN32

#include "core/GameView.h"
#include "graphics/DDRAW.h"

/* tilemap.h conflicts with DDRAW.h on g_tilemap/g_primary_surface types.
 * We only need the TileMap_InvalidateRect declaration, which is provided
 * by the forward declaration below. */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <new>

/* ================================================================== */
/* External globals we assign into (matching the canonical types)      */
/* ================================================================== */

extern void* g_town_view;            /* 0x4852A0 */
/* g_ddraw_building is declared as DDRAW_Building* in DDRAW.h — we use
 * that declaration, not re-declare it. */

/* ================================================================== */
/* Forward declarations of extern functions needed                     */
/* ================================================================== */

extern "C" {
    void  Town_SelectBuilding(void* self, void* building);   /* 0x42D040 */
    void  DDRAW_SelectBuilding(void* self, void* building);  /* 0x459180 */
    int   CRT_rand(void);                                     /* 0x466150 */
    void  TileMap_InvalidateRect(void* tm, int left, int top, int right, int bottom); /* 0x455840 */
    void  TrackPiece_SetZoom(void* track, int zoom);          /* 0x40D170 */
    void  HelpWnd_PlayNarration(void* mgr, int narration_id, int flags); /* 0x44F560 */
}
/* GameObject_GetRelPos is declared in game/Panel.h (C++ linkage, returns int).
 * Use that declaration rather than re-declaring it here. */

/* These are called by DDRAW_UpdateBuilding; stubs link at build time. */
extern void DDRAW_UpdateBuildingSprites(void* self);          /* 0x4597E0 */
extern void DDRAW_UpdateVehicleSprites(int self);             /* 0x45A480 */

/* g_tilemap as void* (matching DDRAW.h declaration) */
extern void* g_tilemap;
extern int   g_cursor_world_x;
extern int   g_cursor_world_y;
extern void* g_audio_mgr;
extern uint32_t g_game_time;

/* ================================================================== */
/* Host construction of the two mode-3 singletons                      */
/* ================================================================== */

namespace loco {
namespace host {

void BootstrapTownMode3Objects()
{
    /* ---------- g_town_view (GameView, 0x4852A0) ---------- */
    if (g_town_view == nullptr) {
        void* mem = operator_new(sizeof(GameView));
        if (mem) {
            g_town_view = ::new (mem) GameView();
            std::fprintf(stderr,
                "[HOST] BootstrapTownMode3Objects: GameView at %p\n",
                g_town_view);
        }
    }

    /* ---------- g_ddraw_building (DDRAW_Building, 0x4A9EF0) ---------- */
    /* g_ddraw_building is declared as DDRAW_Building* in DDRAW.h.
     * We assign a heap-allocated DDRAW_Building to it. */
    extern DDRAW_Building* g_ddraw_building;
    if (g_ddraw_building == nullptr) {
        void* mem = operator_new(sizeof(DDRAW_Building));
        if (mem) {
            g_ddraw_building = ::new (mem) DDRAW_Building();
            std::fprintf(stderr,
                "[HOST] BootstrapTownMode3Objects: DDRAW_Building at %p\n",
                static_cast<void*>(g_ddraw_building));
        }
    }
}

} // namespace host
} // namespace loco

/* ================================================================== */
/* Town_TrackBuilding — per-frame town viewport / building tracking    */
/* Address: 0x42D1A0                                                   */
/* ================================================================== */

void Town_TrackBuilding(void* self_ptr)
{
    uint8_t* bytes = static_cast<uint8_t*>(self_ptr);

    /* +0x88 byte (Town::selection_active / RESDATA::active flag) */
    if (bytes[0x88] == 0) {
        return;
    }

    /* +0x16C word = building type of the selected building */
    int16_t building_type = *reinterpret_cast<int16_t*>(bytes + 0x16C);

    /* If type == 6 (depot) and building's +0x24 byte (isVisible) == 0:
     * auto-deselect */
    if (building_type == 6) {
        void* selected = *reinterpret_cast<void**>(bytes + 0xE0);
        if (selected != nullptr) {
            uint8_t* selBytes = static_cast<uint8_t*>(selected);
            if (selBytes[0x24] == 0) {
                Town_SelectBuilding(self_ptr, nullptr);
            }
        }
    }

    /* Compute center of selected building's bounding rect */
    void* selected = *reinterpret_cast<void**>(bytes + 0xE0);
    if (selected == nullptr) {
        return;
    }
    uint8_t* sel = static_cast<uint8_t*>(selected);
    int left   = *reinterpret_cast<int*>(sel + 0x08);
    int top    = *reinterpret_cast<int*>(sel + 0x0C);
    int right  = *reinterpret_cast<int*>(sel + 0x10);
    int bottom = *reinterpret_cast<int*>(sel + 0x14);
    int center_x = ((right - left) >> 1) + left;
    int center_y = ((bottom - top) >> 1) + top;

    /* Compare to cached center at +0x190/+0x194 */
    int cached_x = *reinterpret_cast<int*>(bytes + 0x190);
    int cached_y = *reinterpret_cast<int*>(bytes + 0x194);

    bool moved = (cached_x != center_x) || (cached_y != center_y);

    if (moved) {
        /* vtable[3] = SetPosition: center viewport on building */
        void*** vtable = *reinterpret_cast<void****>(self_ptr);
        reinterpret_cast<void(*)(void*,int,int)>(vtable[0][3])(self_ptr, center_x, center_y);
        *reinterpret_cast<int*>(bytes + 0x190) = center_x;
        *reinterpret_cast<int*>(bytes + 0x194) = center_y;
    }

    /* Compute relative cursor position */
    int relPos[2];
    GameObject_GetRelPos(self_ptr, relPos, g_cursor_world_x, g_cursor_world_y);

    /* Walk child-list at +0xD0 (linked list, next ptr at +0x28)
     * Call vtable[20] (0x50) on each child */
    void* child = *reinterpret_cast<void**>(bytes + 0xD0);
    while (child != nullptr) {
        void*** childVt = *reinterpret_cast<void****>(child);
        reinterpret_cast<void(*)(void*,void*)>(childVt[0][20])(child, child);
        child = *reinterpret_cast<void**>(static_cast<uint8_t*>(child) + 0x28);
    }

    /* Call vtable[1] (0x04) on the embedded Entity sub-object at +0xE4 */
    void* entity = bytes + 0xE4;
    void*** entityVt = *reinterpret_cast<void****>(entity);
    reinterpret_cast<void(*)(void*)>(entityVt[0][1])(entity);
}


/* ================================================================== */
/* DDRAW_UpdateBuilding — per-frame building selection / sprite update */
/* Address: 0x459DA0                                                   */
/* ================================================================== */

void DDRAW_UpdateBuilding(void* self_ptr)
{
    uint8_t* bytes = static_cast<uint8_t*>(self_ptr);

    /* +0x88 byte (RESDATA::active flag): guard the entire function */
    if (bytes[0x88] == 0) {
        return;
    }

    /* +0x538 = pointer to parent/target building object.
     * Check its +0x18 byte: if != 1, deselect and return. */
    void* target_obj = *reinterpret_cast<void**>(bytes + 0x538);
    if (target_obj == nullptr || static_cast<uint8_t*>(target_obj)[0x18] != 1) {
        DDRAW_SelectBuilding(self_ptr, nullptr);
        return;
    }

    /* +0xE0 = embedded GameObject sub-object 1 (hit-test entity).
     * Call its vtable[2] (0x08) = hit_test(cursor_x, cursor_y) */
    void* hitEntity = bytes + 0xE0;
    void*** hitVt = *reinterpret_cast<void****>(hitEntity);
    bool hovering = reinterpret_cast<bool(*)(void*,int,int)>(hitVt[0][2])(
        hitEntity, g_cursor_world_x, g_cursor_world_y);

    int hoverState = *reinterpret_cast<int*>(bytes + 0x108);

    /* Hover enter */
    if (hovering && hoverState != 1) {
        reinterpret_cast<void(*)(void*,int)>(hitVt[0][7])(hitEntity, 1);
        int* rect = reinterpret_cast<int*>(bytes + 0xE8);
        TileMap_InvalidateRect(g_tilemap, rect[0], rect[1], rect[2], rect[3]);
    }

    /* Hover exit */
    if (!hovering && hoverState != 0) {
        reinterpret_cast<void(*)(void*,int)>(hitVt[0][7])(hitEntity, 0);
        int* rect = reinterpret_cast<int*>(bytes + 0xE8);
        TileMap_InvalidateRect(g_tilemap, rect[0], rect[1], rect[2], rect[3]);
    }

    /* Pattern animation (station sprites) */
    if (bytes[0x44C] != 0) {
        void* patternContainer = bytes + 0x428;
        void*** pcVt = *reinterpret_cast<void****>(patternContainer);
        reinterpret_cast<void(*)(void*)>(pcVt[0][10])(patternContainer);

        int updateFlag = *reinterpret_cast<int*>(bytes + 0x47C);
        if (updateFlag != 0) {
            uint8_t* patBase = bytes + 0x1A8;
            for (int i = 0; i < 4; i++) {
                void* patObj = *reinterpret_cast<void**>(patBase);
                if (patObj != nullptr) {
                    uint16_t maxVal = *reinterpret_cast<uint16_t*>(
                        static_cast<uint8_t*>(patObj) + 0x1A);
                    int frame;
                    int16_t sMax = static_cast<int16_t>(maxVal);
                    if (sMax < 0) {
                        int r = CRT_rand();
                        frame = r % (2 - static_cast<int>(maxVal))
                                + (-1 + static_cast<int>(maxVal));
                    } else if (maxVal == 0) {
                        frame = 0;
                    } else {
                        int r = CRT_rand();
                        frame = r % static_cast<int>(maxVal);
                    }
                    void* parentObj = *reinterpret_cast<void**>(patBase - 0x40);
                    if (parentObj != nullptr) {
                        void*** parentVt = *reinterpret_cast<void****>(parentObj);
                        reinterpret_cast<void(*)(void*,int)>(parentVt[0][7])(
                            parentObj, frame);
                    }
                }
                patBase += 0x88;
            }
        }
    }

    /* Drag handling */
    if (bytes[0x90] == 1) {
        int dragOffX = *reinterpret_cast<int*>(bytes + 0x94);
        int dragOffY = *reinterpret_cast<int*>(bytes + 0x98);
        void*** selfVt = *reinterpret_cast<void****>(self_ptr);
        reinterpret_cast<void(*)(void*,int,int)>(selfVt[0][3])(
            self_ptr,
            g_cursor_world_x - dragOffX,
            g_cursor_world_y - dragOffY);
        return;
    }

    /* Compute relative cursor position */
    int relPos[2];
    GameObject_GetRelPos(self_ptr, relPos, g_cursor_world_x, g_cursor_world_y);

    /* Walk child-list at +0xD0 */
    void* child = *reinterpret_cast<void**>(bytes + 0xD0);
    while (child != nullptr) {
        void*** childVt = *reinterpret_cast<void****>(child);
        reinterpret_cast<void(*)(void*,void*)>(childVt[0][20])(child, child);
        child = *reinterpret_cast<void**>(static_cast<uint8_t*>(child) + 0x28);
    }

    /* Popup panel handling */
    int popupVisible = *reinterpret_cast<int*>(bytes + 0x3E0);
    if (popupVisible != 0) {
        int16_t buildingType = *reinterpret_cast<int16_t*>(bytes + 0x398);
        if (buildingType == 7) {
            uint8_t* parentBytes = static_cast<uint8_t*>(target_obj);
            int childCount = parentBytes[0x88] >> 1;
            int scrollPos = *reinterpret_cast<int*>(bytes + 0x3C8);
            if (scrollPos != childCount) {
                void* popupPanel = bytes + 0x3A0;
                void*** ppVt = *reinterpret_cast<void****>(popupPanel);
                reinterpret_cast<void(*)(void*,int)>(ppVt[0][7])(
                    popupPanel, childCount);
            }
        }
        void* popupPanel = bytes + 0x3A0;
        void*** ppVt = *reinterpret_cast<void****>(popupPanel);
        reinterpret_cast<void(*)(void*)>(ppVt[0][10])(popupPanel);
    }

    /* Building type == 3 (e.g., signal box) */
    if (*reinterpret_cast<int16_t*>(bytes + 0x398) == 3) {
        int lastUpdate = *reinterpret_cast<int*>(bytes + 0x8C);
        if (lastUpdate < static_cast<int>(g_game_time)) {
            if (bytes[0x44C] != 1) {
                DDRAW_UpdateBuildingSprites(self_ptr);
                *reinterpret_cast<int*>(bytes + 0x8C) = g_game_time;
            }
        }
        HelpWnd_PlayNarration(g_audio_mgr, 0x0C, 0);
    }

    /* Building type == 6 (depot / locomotive) */
    if (*reinterpret_cast<int16_t*>(bytes + 0x398) == 6) {
        void* vehicleData = *reinterpret_cast<void**>(
            static_cast<uint8_t*>(target_obj) + 0x44C);
        if (vehicleData != nullptr) {
            uint8_t* vd = static_cast<uint8_t*>(vehicleData);
            int dirCount = *reinterpret_cast<int*>(vd + 0x08);
            int16_t trackDir = *reinterpret_cast<int16_t*>(vd + 0x58);
            int mode = *reinterpret_cast<int*>(vd + 0x5C);
            bool isMode2 = (mode == 2);

            /* Front engine */
            if (isMode2 && dirCount == 1) {
                if (trackDir == *reinterpret_cast<int16_t*>(vd + 0x24)) {
                    TrackPiece_SetZoom(
                        *reinterpret_cast<void**>(bytes + 0x5A4), 2);
                    void* trackEnt = bytes + 0x4B0;
                    void*** teVt = *reinterpret_cast<void****>(trackEnt);
                    reinterpret_cast<void(*)(void*,int)>(teVt[0][7])(trackEnt, 1);
                } else {
                    TrackPiece_SetZoom(
                        *reinterpret_cast<void**>(bytes + 0x5A4), 3);
                    void* trackEnt = bytes + 0x4B0;
                    void*** teVt = *reinterpret_cast<void****>(trackEnt);
                    reinterpret_cast<void(*)(void*,int)>(teVt[0][7])(trackEnt, 0);
                }
            } else {
                TrackPiece_SetZoom(
                    *reinterpret_cast<void**>(bytes + 0x5A4), 1);
            }

            /* Rear engine */
            if (isMode2 && dirCount == 0) {
                if (trackDir == *reinterpret_cast<int16_t*>(vd + 0x24)) {
                    TrackPiece_SetZoom(
                        *reinterpret_cast<void**>(bytes + 0x5AC), 2);
                    void* trackEnt = bytes + 0x4B0;
                    void*** teVt = *reinterpret_cast<void****>(trackEnt);
                    reinterpret_cast<void(*)(void*,int)>(teVt[0][7])(trackEnt, 3);
                } else {
                    TrackPiece_SetZoom(
                        *reinterpret_cast<void**>(bytes + 0x5AC), 3);
                    void* trackEnt = bytes + 0x4B0;
                    void*** teVt = *reinterpret_cast<void****>(trackEnt);
                    reinterpret_cast<void(*)(void*,int)>(teVt[0][7])(trackEnt, 4);
                }
            } else {
                TrackPiece_SetZoom(
                    *reinterpret_cast<void**>(bytes + 0x5AC), 1);
            }

            /* Center car */
            if (!isMode2) {
                TrackPiece_SetZoom(
                    *reinterpret_cast<void**>(bytes + 0x5A8), 2);
                void* trackEnt = bytes + 0x4B0;
                void*** teVt = *reinterpret_cast<void****>(trackEnt);
                reinterpret_cast<void(*)(void*,int)>(teVt[0][7])(trackEnt, 2);
            } else {
                TrackPiece_SetZoom(
                    *reinterpret_cast<void**>(bytes + 0x5A8), 1);
            }

            /* Vehicle sprite update */
            if (mode == 0 || mode == 1) {
                DDRAW_UpdateVehicleSprites(
                    static_cast<int>(reinterpret_cast<intptr_t>(self_ptr)));
            }

            /* Auto-deselect if done */
            if (*reinterpret_cast<int*>(vd + 0x68) == 2 ||
                *reinterpret_cast<int*>(vd + 0x60) != 0 ||
                *reinterpret_cast<int*>(vd + 0x64) == 2) {
                DDRAW_SelectBuilding(self_ptr, nullptr);
            }
        }
        HelpWnd_PlayNarration(g_audio_mgr, 0x0B, 0);
    }
}

#endif /* _WIN32 */
