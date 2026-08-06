/**
 * sdl3_town_mode3.cpp — SDL host glue for mode-3 town gameplay
 *
 * Lego Loco (loco.exe, 1998, MSVC x86) — host-only deviation (#ifndef _WIN32)
 */
#ifndef _WIN32

#include "core/GameView.h"
#include "graphics/DDRAW.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern void* g_town_view;

namespace {

/* The original instances are BSS-backed.  These host-only backing stores are
 * deliberately zero-initialized and remain alive for the process lifetime.
 * Constructors cannot be run: their recovered x86 layouts write through
 * incompatible host offsets. */
alignas(GameView) std::array<std::byte, sizeof(GameView)> s_game_view_storage{};

struct HostDdrawBuildingStorage {
    alignas(DDRAW_Building) std::array<std::byte, sizeof(DDRAW_Building)> bytes{};

    HostDdrawBuildingStorage()
    {
        constexpr std::size_t kOriginalTypeOffset = 0x04;
        const std::int32_t type = 0x0D;
        std::memcpy(bytes.data() + kOriginalTypeOffset, &type, sizeof(type));
    }
};

HostDdrawBuildingStorage s_ddraw_building_storage;

} // namespace

namespace loco {
namespace host {

bool Mode3FrameDependenciesReady()
{
    return g_town_view != nullptr && g_ddraw_building != nullptr;
}

void BootstrapTownMode3Objects()
{
    /* This is the required host-only mode-3 frame dependency boundary.
     * The first GameLoop_FrameUpdate after the menu transition dispatches
     * both objects unconditionally (0x45C4E1/0x45C4E6). */
    if (g_town_view == nullptr) {
        g_town_view = s_game_view_storage.data();
    }
    if (g_ddraw_building == nullptr) {
        g_ddraw_building = reinterpret_cast<DDRAW_Building*>(
            s_ddraw_building_storage.bytes.data());
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
    if (!bytes) return;
    if (bytes[0x88] == 0) return;

    int16_t building_type = *reinterpret_cast<int16_t*>(bytes + 0x16C);
    if (building_type == 6) {
        void* selected = *reinterpret_cast<void**>(bytes + 0xE0);
        if (selected != nullptr) {
            uint8_t* selBytes = static_cast<uint8_t*>(selected);
            if (selBytes[0x24] == 0) {
                extern void Town_SelectBuilding(void* self, void* building);
                Town_SelectBuilding(self_ptr, nullptr);
            }
        }
    }

    void* selected = *reinterpret_cast<void**>(bytes + 0xE0);
    if (!selected) return;
    uint8_t* sel = static_cast<uint8_t*>(selected);
    int left   = *reinterpret_cast<int*>(sel + 0x08);
    int top    = *reinterpret_cast<int*>(sel + 0x0C);
    int right  = *reinterpret_cast<int*>(sel + 0x10);
    int bottom = *reinterpret_cast<int*>(sel + 0x14);
    int center_x = ((right - left) >> 1) + left;
    int center_y = ((bottom - top) >> 1) + top;

    int cached_x = *reinterpret_cast<int*>(bytes + 0x190);
    int cached_y = *reinterpret_cast<int*>(bytes + 0x194);
    if (cached_x != center_x || cached_y != center_y) {
        void*** vtable = *reinterpret_cast<void****>(self_ptr);
        if (vtable && vtable[0] && vtable[0][3])
            reinterpret_cast<void(*)(void*,int,int)>(vtable[0][3])(self_ptr, center_x, center_y);
        *reinterpret_cast<int*>(bytes + 0x190) = center_x;
        *reinterpret_cast<int*>(bytes + 0x194) = center_y;
    }

    extern int g_cursor_world_x, g_cursor_world_y;
    extern int GameObject_GetRelPos(void* self, int* out, int x, int y);
    int relPos[2];
    GameObject_GetRelPos(self_ptr, relPos, g_cursor_world_x, g_cursor_world_y);

    void* child = *reinterpret_cast<void**>(bytes + 0xD0);
    while (child) {
        void*** childVt = *reinterpret_cast<void****>(child);
        if (childVt && childVt[0] && childVt[0][20])
            reinterpret_cast<void(*)(void*,void*)>(childVt[0][20])(child, child);
        child = *reinterpret_cast<void**>(static_cast<uint8_t*>(child) + 0x28);
    }

    void* entity = bytes + 0xE4;
    void*** entityVt = *reinterpret_cast<void****>(entity);
    if (entityVt && entityVt[0] && entityVt[0][1])
        reinterpret_cast<void(*)(void*)>(entityVt[0][1])(entity);
}

/* ================================================================== */
/* DDRAW_UpdateBuilding — per-frame building selection / sprite update */
/* Address: 0x459DA0                                                   */
/* ================================================================== */

/* C++ linkage — DDRAW_SelectBuilding has a C++ mangled definition */
void  DDRAW_SelectBuilding(void* self, void* building);

extern "C" {
    int   CRT_rand(void);
    void  TileMap_InvalidateRect(void* tm, int left, int top, int right, int bottom);
    void  TrackPiece_SetZoom(void* track, int zoom);
    void  HelpWnd_PlayNarration(void* mgr, int narration_id, int flags);
}
extern void DDRAW_UpdateBuildingSprites(void* self);
extern void DDRAW_UpdateVehicleSprites(int self);
extern void* g_tilemap;
extern int   g_cursor_world_x, g_cursor_world_y;
extern void* g_audio_mgr;
extern uint32_t g_game_time;

void DDRAW_UpdateBuilding(void* self_ptr)
{
    uint8_t* bytes = static_cast<uint8_t*>(self_ptr);
    if (!bytes) return;
    if (bytes[0x88] == 0) return;

    void* target_obj = *reinterpret_cast<void**>(bytes + 0x538);
    if (!target_obj || static_cast<uint8_t*>(target_obj)[0x18] != 1) {
        DDRAW_SelectBuilding(self_ptr, nullptr);
        return;
    }

    void* hitEntity = bytes + 0xE0;
    void*** hitVt = *reinterpret_cast<void****>(hitEntity);
    if (!hitVt || !hitVt[0] || !hitVt[0][2]) return;
    bool hovering = reinterpret_cast<bool(*)(void*,int,int)>(hitVt[0][2])(
        hitEntity, g_cursor_world_x, g_cursor_world_y);

    int hoverState = *reinterpret_cast<int*>(bytes + 0x108);
    if (hovering && hoverState != 1) {
        if (hitVt[0][7])
            reinterpret_cast<void(*)(void*,int)>(hitVt[0][7])(hitEntity, 1);
        int* rect = reinterpret_cast<int*>(bytes + 0xE8);
        TileMap_InvalidateRect(g_tilemap, rect[0], rect[1], rect[2], rect[3]);
    }
    if (!hovering && hoverState != 0) {
        if (hitVt[0][7])
            reinterpret_cast<void(*)(void*,int)>(hitVt[0][7])(hitEntity, 0);
        int* rect = reinterpret_cast<int*>(bytes + 0xE8);
        TileMap_InvalidateRect(g_tilemap, rect[0], rect[1], rect[2], rect[3]);
    }

    if (bytes[0x44C] != 0) {
        void* patternContainer = bytes + 0x428;
        void*** pcVt = *reinterpret_cast<void****>(patternContainer);
        if (pcVt && pcVt[0] && pcVt[0][10])
            reinterpret_cast<void(*)(void*)>(pcVt[0][10])(patternContainer);
        int updateFlag = *reinterpret_cast<int*>(bytes + 0x47C);
        if (updateFlag != 0) {
            uint8_t* patBase = bytes + 0x1A8;
            for (int i = 0; i < 4; i++) {
                void* patObj = *reinterpret_cast<void**>(patBase);
                if (patObj) {
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
                    if (parentObj) {
                        void*** parentVt = *reinterpret_cast<void****>(parentObj);
                        if (parentVt && parentVt[0] && parentVt[0][7])
                            reinterpret_cast<void(*)(void*,int)>(parentVt[0][7])(parentObj, frame);
                    }
                }
                patBase += 0x88;
            }
        }
    }

    if (bytes[0x90] == 1) {
        int dragOffX = *reinterpret_cast<int*>(bytes + 0x94);
        int dragOffY = *reinterpret_cast<int*>(bytes + 0x98);
        void*** selfVt = *reinterpret_cast<void****>(self_ptr);
        if (selfVt && selfVt[0] && selfVt[0][3])
            reinterpret_cast<void(*)(void*,int,int)>(selfVt[0][3])(
                self_ptr,
                g_cursor_world_x - dragOffX,
                g_cursor_world_y - dragOffY);
        return;
    }

    extern int GameObject_GetRelPos(void* self, int* out, int x, int y);
    int relPos[2];
    GameObject_GetRelPos(self_ptr, relPos, g_cursor_world_x, g_cursor_world_y);

    void* child = *reinterpret_cast<void**>(bytes + 0xD0);
    while (child) {
        void*** childVt = *reinterpret_cast<void****>(child);
        if (childVt && childVt[0] && childVt[0][20])
            reinterpret_cast<void(*)(void*,void*)>(childVt[0][20])(child, child);
        child = *reinterpret_cast<void**>(static_cast<uint8_t*>(child) + 0x28);
    }

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
                if (ppVt && ppVt[0] && ppVt[0][7])
                    reinterpret_cast<void(*)(void*,int)>(ppVt[0][7])(popupPanel, childCount);
            }
        }
        void* popupPanel = bytes + 0x3A0;
        void*** ppVt = *reinterpret_cast<void****>(popupPanel);
        if (ppVt && ppVt[0] && ppVt[0][10])
            reinterpret_cast<void(*)(void*)>(ppVt[0][10])(popupPanel);
    }

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

    if (*reinterpret_cast<int16_t*>(bytes + 0x398) == 6) {
        void* vehicleData = *reinterpret_cast<void**>(static_cast<uint8_t*>(target_obj) + 0x44C);
        if (vehicleData) {
            uint8_t* vd = static_cast<uint8_t*>(vehicleData);
            int dirCount = *reinterpret_cast<int*>(vd + 0x08);
            int16_t trackDir = *reinterpret_cast<int16_t*>(vd + 0x58);
            int mode = *reinterpret_cast<int*>(vd + 0x5C);
            bool isMode2 = (mode == 2);

            if (isMode2 && dirCount == 1) {
                if (trackDir == *reinterpret_cast<int16_t*>(vd + 0x24)) {
                    TrackPiece_SetZoom(*reinterpret_cast<void**>(bytes + 0x5A4), 2);
                    void* trackEnt = bytes + 0x4B0;
                    void*** teVt = *reinterpret_cast<void****>(trackEnt);
                    if (teVt && teVt[0] && teVt[0][7])
                        reinterpret_cast<void(*)(void*,int)>(teVt[0][7])(trackEnt, 1);
                } else {
                    TrackPiece_SetZoom(*reinterpret_cast<void**>(bytes + 0x5A4), 3);
                    void* trackEnt = bytes + 0x4B0;
                    void*** teVt = *reinterpret_cast<void****>(trackEnt);
                    if (teVt && teVt[0] && teVt[0][7])
                        reinterpret_cast<void(*)(void*,int)>(teVt[0][7])(trackEnt, 0);
                }
            } else {
                TrackPiece_SetZoom(*reinterpret_cast<void**>(bytes + 0x5A4), 1);
            }

            if (isMode2 && dirCount == 0) {
                if (trackDir == *reinterpret_cast<int16_t*>(vd + 0x24)) {
                    TrackPiece_SetZoom(*reinterpret_cast<void**>(bytes + 0x5AC), 2);
                    void* trackEnt = bytes + 0x4B0;
                    void*** teVt = *reinterpret_cast<void****>(trackEnt);
                    if (teVt && teVt[0] && teVt[0][7])
                        reinterpret_cast<void(*)(void*,int)>(teVt[0][7])(trackEnt, 3);
                } else {
                    TrackPiece_SetZoom(*reinterpret_cast<void**>(bytes + 0x5AC), 3);
                    void* trackEnt = bytes + 0x4B0;
                    void*** teVt = *reinterpret_cast<void****>(trackEnt);
                    if (teVt && teVt[0] && teVt[0][7])
                        reinterpret_cast<void(*)(void*,int)>(teVt[0][7])(trackEnt, 4);
                }
            } else {
                TrackPiece_SetZoom(*reinterpret_cast<void**>(bytes + 0x5AC), 1);
            }

            if (!isMode2) {
                TrackPiece_SetZoom(*reinterpret_cast<void**>(bytes + 0x5A8), 2);
                void* trackEnt = bytes + 0x4B0;
                void*** teVt = *reinterpret_cast<void****>(trackEnt);
                if (teVt && teVt[0] && teVt[0][7])
                    reinterpret_cast<void(*)(void*,int)>(teVt[0][7])(trackEnt, 2);
            } else {
                TrackPiece_SetZoom(*reinterpret_cast<void**>(bytes + 0x5A8), 1);
            }

            if (mode == 0 || mode == 1) {
                DDRAW_UpdateVehicleSprites(static_cast<int>(reinterpret_cast<intptr_t>(self_ptr)));
            }

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
