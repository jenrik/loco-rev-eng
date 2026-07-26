/**
 * EditorState.cpp — Per-endpoint track editor state machine implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: TRANSCRIBED
 *
 * Implements all 15 EditorState methods verified against disassembly.
 */

#include "EditorState.h"
#include "../core/VehicleEditor.h"
#include "../core/Entity.h"
#include "../game/Vehicle.h"
#include "../game/Building.h"
#include "../game/GameVehicle.h"
#include "../game/BuildingMgr.h"
#include "tilemap.h"

#include <cstdint>

/* ================================================================== */
/* External globals                                                    */
/* ================================================================== */
extern int32_t  g_game_mode;        /* 0x004851F4 */
extern int32_t  g_world_width;
extern int32_t  g_world_height;
extern TileMap  g_tilemap;          /* 0x4AAD08 */

/* ================================================================== */
/* External free functions — TODO: refactor to TileData class methods  */
/* ================================================================== */
/* External resource classification functions                          */
/* ================================================================== */
extern uint8_t  GetResourceType(uint32_t res_id);                     /* 0x45AAA0 */

/* Resource_IsValidTrackIndex — __thiscall method on resource object.
 * Reads current/alternate track index fields at +0x636/+0x638.
 * Address: 0x44BCD0 */
extern uint8_t  Resource_IsValidTrackIndex(void* resource, int16_t idx);

/* Resource_IsRoadTile — __fastcall, reads tile type byte at resource+0x63A.
 * Returns 1 if type ∈ {1,2,3,4}. Address: 0x44BD10 */
extern uint8_t  Resource_IsRoadTile(void* resource);

/* Resource_IsBuildingTile — __fastcall, reads tile type byte at resource+0x63A.
 * Returns 1 if type ∈ {7,8,9,10}. Address: 0x44BD30 */
extern uint8_t  Resource_IsBuildingTile(void* resource);

/* BuildingMgr singleton at 0x485448 */
extern BuildingMgr* g_building_mgr;          /* 0x485448 — BuildingMgr singleton */

/* Helper: read int16_t from control point array */
static inline int16_t ReadCP(uintptr_t cp_array_base, int index, int byte_offset)
{
    return *(int16_t*)(cp_array_base + (uintptr_t)(index * 4 + byte_offset));
}

/* ==================================================================== */
/* EditorState::EditorState — Constructor                                */
/* Address: 0x40B500                                                     */
/* ==================================================================== */
EditorState::EditorState(char viewport_side)
{
    this->pos_x = -1;
    this->pos_y = -1;
    this->direction = 1;
    this->track_pos = 0;
    this->building = NULL;

    if (viewport_side == 0) {
        this->move_state = 0;
        this->edit_state = 2;
    } else {
        this->move_state = 2;
        this->edit_state = 0;
    }
}

/* ==================================================================== */
/* EditorState::~EditorState — Destructor (vtable[0])                    */
/* Address: 0x40B550                                                     */
/* ==================================================================== */
EditorState::~EditorState()
{
    if (this->building != NULL && g_game_mode != 10) {
        int16_t* ref_count = (int16_t*)((uint8_t*)this->building + 0x114);
        (*ref_count)--;
        this->building = NULL;
    }
}

/* ==================================================================== */
/* EditorState::Detach — Detach from parent building                     */
/* Address: 0x40B5A0                                                     */
/* ==================================================================== */
void EditorState::Detach()
{
    if (this->building != NULL && g_game_mode != 10) {
        int16_t* ref_count = (int16_t*)((uint8_t*)this->building + 0x114);
        (*ref_count)--;
        this->building = NULL;
    }
}

/* ==================================================================== */
/* EditorState::Copy — Shallow-copy from another EditorState             */
/* Address: 0x40B5D0                                                     */
/* ==================================================================== */
void EditorState::Copy(const EditorState* src)
{
    this->direction  = src->direction;
    this->track_pos  = src->track_pos;
    this->pos_x      = src->pos_x;
    this->pos_y      = src->pos_y;
    this->building   = src->building;
    this->move_state = src->move_state;
    this->edit_state = src->edit_state;
}

/* ==================================================================== */
/* EditorState::FindTrackPosition — Snap to nearest control point        */
/* Address: 0x40B610                                                     */
/*                                                                       */
/* BUG: If pixel_x matches control_points[0].x AND pixel_y ALSO matches  */
/* control_points[0].y, the function falls through returning 0 without   */
/* setting any fields. This is a bug in the original binary.             */
/* ==================================================================== */
int EditorState::FindTrackPosition(int pixel_x, int pixel_y)
{
    int32_t* building_ptr;
    void*    resource;
    int16_t* control_points;
    uint16_t i;
    int      result = 0;

    building_ptr = (int32_t*)this->building;
    if (building_ptr == NULL) {
        return 0;
    }

    resource = *(void**)((uint8_t*)building_ptr + 0x40);
    control_points = *(int16_t**)((uint8_t*)resource + 0x630);

    int tile_x_pixels = *(int16_t*)((uint8_t*)building_ptr + 0x88) * 16;
    int tile_y_pixels = *(int16_t*)((uint8_t*)building_ptr + 0x8A) * 16;

    if (pixel_x == tile_x_pixels + control_points[0]) {
        if (pixel_y != tile_y_pixels + control_points[1]) {
            for (i = 0; i < *(uint16_t*)((uint8_t*)resource + 0x636); i++) {
                if (pixel_y - tile_y_pixels == control_points[i * 2 + 1]) {
                    this->track_pos = (int32_t)i;
                    this->pos_y = control_points[i * 2 + 1] + tile_y_pixels;
                    result = 1;
                    break;
                }
            }
        }
    } else {
        for (i = 0; i < *(uint16_t*)((uint8_t*)resource + 0x636); i++) {
            if (pixel_x - tile_x_pixels == control_points[i * 2]) {
                this->track_pos = (int32_t)i;
                this->pos_x = control_points[i * 2] + tile_x_pixels;
                return 1;
            }
        }
        return 0;
    }

    return result;
}

/* ==================================================================== */
/* EditorState::InitTrackAtPosition — Initialize track at snapped pos    */
/* Address: 0x40B740                                                     */
/* ==================================================================== */
uint32_t EditorState::InitTrackAtPosition(int pixel_x, int pixel_y)
{
    short    tile_x = (short)(pixel_x >> 4);
    short    tile_y = (short)(pixel_y >> 4);
    short    clamped_x = (pixel_x < 0) ? -1 : tile_x;
    short    clamped_y = (pixel_y < 0) ? -1 : tile_y;
    int32_t* building_ptr;
    void*    resource;
    uint16_t i;
    int32_t  tile_origin[2];

    building_ptr = (int32_t*)TileMap_GetObjectAt(&g_tilemap, clamped_x, clamped_y, 0);
    this->building = (Building*)building_ptr;
    if (building_ptr == NULL) {
        return 0;
    }

    uintptr_t res_ptr = *(uintptr_t*)((uint8_t*)building_ptr + 0x40);
    uint32_t resource_type = GetResourceType(*(uint32_t*)(res_ptr + 4));
    if (resource_type != 3) {
        return resource_type & 0xFFFFFF00;
    }

    resource = *(void**)((uint8_t*)building_ptr + 0x40);
    this->direction = 1;

    if (pixel_y < 0) tile_y = -1;
    if (pixel_x < 0) tile_x = -1;

    int32_t* origin_out = TileMap_GetTileOrigin(&g_tilemap, tile_origin,
                                                 tile_x, tile_y, 0);
    *(int32_t*)((uint8_t*)building_ptr + 0x88) = *origin_out;

    int building_x_pixels = *(int16_t*)((uint8_t*)building_ptr + 0x88) * 16;
    int32_t cp_array = *(int32_t*)((uint8_t*)resource + 0x630);
    for (i = 0; i < *(uint16_t*)((uint8_t*)resource + 0x636); i++) {
        if (pixel_x - building_x_pixels == ReadCP(cp_array, (int)i, 0)) {
            this->track_pos = (uint32_t)i;
            break;
        }
    }

    this->pos_x = ReadCP(cp_array, this->track_pos, 0) +
                  *(int16_t*)((uint8_t*)building_ptr + 0x88) * 16;

    this->pos_y = ReadCP(cp_array, this->track_pos, 2) +
                  *(int16_t*)((uint8_t*)building_ptr + 0x8A) * 16;

    return 1;
}

/* ==================================================================== */
/* EditorState::FindAdjacentTrack — Find adjacent track piece            */
/* Address: 0x40B880                                                     */
/*                                                                       */
/* Returns NULL on success (connected to adjacent track).                */
/* Returns current building pointer on failure.                          */
/* ==================================================================== */
Building* EditorState::FindAdjacentTrack()
{
    int32_t* building_ptr;
    void*    resource;
    int16_t* control_points;
    int      pixel_x, pixel_y;
    short    tile_x, tile_y;
    int32_t* adjacent;
    void*    adj_resource;
    int      tile_x_pixels, tile_y_pixels;
    uint16_t pt_count;
    int16_t* adj_pts;
    int32_t  tile_origin_packed[2];

    building_ptr = (int32_t*)this->building;
    resource = *(void**)((uint8_t*)building_ptr + 0x40);
    control_points = *(int16_t**)((uint8_t*)resource + 0x630);

    pixel_x = *(int16_t*)((uint8_t*)building_ptr + 0x88) * 16 +
              control_points[this->track_pos * 2];
    pixel_y = *(int16_t*)((uint8_t*)building_ptr + 0x8A) * 16 +
              control_points[this->track_pos * 2 + 1];

    if (pixel_x < 0 || pixel_y < 0) {
        return this->building;
    }

    tile_x = (short)(pixel_x >> 4);
    tile_y = (short)(pixel_y >> 4);

    TileMap_GetTileOriginEx(&g_tilemap, tile_origin_packed,
                             tile_x, tile_y, 0);
    int snapped_tile_x = tile_origin_packed[0] & 0xFFFF;
    int snapped_tile_y = (tile_origin_packed[0] >> 16) & 0xFFFF;

    if (snapped_tile_x < 0) {
        return this->building;
    }

    tile_x_pixels = snapped_tile_x * 16;
    tile_y_pixels = snapped_tile_y * 16;

    adjacent = (int32_t*)TileMap_GetObjectAt(&g_tilemap, tile_x, tile_y, 0);
    if (adjacent == NULL) {
        return this->building;
    }

    uintptr_t adj_res_ptr = *(uintptr_t*)((uint8_t*)adjacent + 0x40);
    uint32_t res_type = GetResourceType(*(uint32_t*)(adj_res_ptr + 4));
    if (res_type != 3 || adjacent == NULL) {
        return this->building;
    }

    adj_resource = *(void**)((uint8_t*)adjacent + 0x40);
    pt_count = *(uint16_t*)((uint8_t*)adj_resource + 0x636);
    adj_pts = *(int16_t**)((uint8_t*)adj_resource + 0x630);

    int adj_type_code = *(int8_t*)((uint8_t*)adj_resource + 0x63A);
    int adj_exclusion = *(int32_t*)((uint8_t*)adjacent + 0x110);

    if (adj_exclusion == 4 && adj_type_code != 0x0D) {
        return this->building;
    }

    if (pixel_x == adj_pts[0] + tile_x_pixels &&
        pixel_y == adj_pts[1] + tile_y_pixels) {
        (*(int16_t*)((uint8_t*)building_ptr + 0x114))--;
        this->building = (Building*)adjacent;
        this->direction = 1;
        this->track_pos = 1;
        (*(int16_t*)((uint8_t*)adjacent + 0x114))++;
        return NULL;
    }

    int last_idx = pt_count * 2 - 2;
    if (pixel_x == adj_pts[last_idx] + tile_x_pixels &&
        pixel_y == adj_pts[last_idx + 1] + tile_y_pixels) {
        if (adj_type_code == 0x0B && adj_exclusion == 4) {
            uint16_t branch_idx = *(uint16_t*)((uint8_t*)adj_resource + 0x638);
            if (adj_pts[0] != adj_pts[branch_idx * 2 + 2] ||
                adj_pts[1] != adj_pts[branch_idx * 2 + 3]) {
                goto check_branch;
            }
        }
        (*(int16_t*)((uint8_t*)building_ptr + 0x114))--;
        this->building = (Building*)adjacent;
        this->direction = 0;
        this->track_pos = pt_count - 1;
        (*(int16_t*)((uint8_t*)adjacent + 0x114))++;
        return NULL;
    }

check_branch:
    if (*(uint16_t*)((uint8_t*)adj_resource + 0x638) != 0) {
        uint16_t branch_idx = *(uint16_t*)((uint8_t*)adj_resource + 0x638);

        if (pixel_x == adj_pts[pt_count * 2 + 4] + tile_x_pixels &&
            pixel_y == adj_pts[pt_count * 2 + 5] + tile_y_pixels) {
            if (adj_type_code == 0x0B && adj_exclusion == 5) {
                if (adj_pts[0] != adj_pts[branch_idx * 2 + 2] ||
                    adj_pts[1] != adj_pts[branch_idx * 2 + 3]) {
                    return this->building;
                }
            }
            (*(int16_t*)((uint8_t*)building_ptr + 0x114))--;
            this->building = (Building*)adjacent;
            this->direction = 1;
            this->track_pos = pt_count + 2;
            (*(int16_t*)((uint8_t*)adjacent + 0x114))++;
            return NULL;
        }

        if (pixel_x == adj_pts[branch_idx * 2 - 2] + tile_x_pixels &&
            pixel_y == adj_pts[branch_idx * 2 - 1] + tile_y_pixels) {
            if (adj_type_code == 0x0B && adj_exclusion == 5) {
                if (adj_pts[0] != adj_pts[branch_idx * 2 + 2] ||
                    adj_pts[1] != adj_pts[branch_idx * 2 + 3]) {
                    return this->building;
                }
            }
            (*(int16_t*)((uint8_t*)building_ptr + 0x114))--;
            this->building = (Building*)adjacent;
            this->direction = 0;
            this->track_pos = branch_idx - 1;
            (*(int16_t*)((uint8_t*)adjacent + 0x114))++;
            return NULL;
        }
    }

    return this->building;
}

/* ==================================================================== */
/* EditorState::UpdateVehiclePlacement — Core placement algorithm         */
/* Address: 0x40BBD0                                                     */
/* ==================================================================== */
uint32_t EditorState::UpdateVehiclePlacement(Vehicle* vehicle)
{
    int32_t* building_ptr;
    void*    resource;
    uint8_t  found_adjacent = 0;
    uint8_t  state_changed  = 0;
    int32_t* adj_result = NULL;

    building_ptr = (int32_t*)this->building;
    if (building_ptr == NULL || *(int8_t*)((uint8_t*)building_ptr + 0x18) != 1) {
        return 0;
    }

    resource = *(void**)((uint8_t*)building_ptr + 0x40);

    int step_state = this->move_state;
    int sub_state  = this->edit_state;

    if (step_state == 4) {
        int dir_type = *(int8_t*)((uint8_t*)resource + 0x63A);
        switch (dir_type) {
        case 1: this->pos_x++; break;
        case 2: this->pos_x--; break;
        case 3: this->pos_y++; break;
        case 4: this->pos_y--; break;
        }
        switch (dir_type - 1) {
        /* NOTE: Binary uses asymmetric bounds for X vs Y:
         * X uses strict < g_world_width, Y uses <= g_world_height.
         * This asymmetry appears consistently across CheckBounds,
         * UpdatePosition, and UpdateVehiclePlacement — matches the
         * original binary's Jcc instructions. */
        case 0: if (this->pos_x >= 0) { this->move_state = 0; state_changed = 1; } break;
        case 1: if (this->pos_x < g_world_width) { this->move_state = 0; state_changed = 1; } break;
        case 2: if (this->pos_y >= 0) { this->move_state = 0; state_changed = 1; } break;
        case 3: if (this->pos_y <= g_world_height) { this->move_state = 0; state_changed = 1; } break;
        }
    } else if (step_state == 2 || sub_state == 4 || sub_state == 5) {
        if (sub_state == 4 || sub_state == 5) {
            state_changed = this->ScrollEdge();
            if (state_changed) {
                ((Entity*)building_ptr)->StopSound(0);
            }
        }
    } else {
        uint8_t valid_idx = Resource_IsValidTrackIndex(resource, (int16_t)this->track_pos);
        if (valid_idx == 1) {
            adj_result = (int32_t*)this->FindAdjacentTrack();
        }

        if (adj_result == NULL) {
            if (valid_idx == 1) {
                found_adjacent = 1;
                resource = *(void**)((uint8_t*)this->building + 0x40);

                if (Resource_IsRoadTile(resource)) {
                    /* vehicle->editor_state is the paired EditorState;
                     * cast for typed access. +0x14 = EditorState::building. */
                    EditorState* ve_es = (EditorState*)vehicle->editor_state;
                    Building* ve_building = ve_es->building;
                    if (*(int32_t*)((uint8_t*)ve_building + 0x11C) == 1 &&
                        vehicle->direction != 1) {
                        if (vehicle->IsMoving()) {
                            vehicle->Stop((vehicle->active_editor == 0) ? 0 : 1, 1);
                            return 0;
                        }
                        vehicle->SetState(1);
                        vehicle->active_flag = 1;
                        vehicle->move_timer = 2;
                        return 0;
                    }
                    *(int32_t*)((uint8_t*)ve_building + 0x11C) = 1;
                    this->move_state = 1;
                    vehicle->direction = 1;
                    /* +0x88 on the pointed-to object is tile_x (int16_t);
                     * this object is a TileMapObject/Building-like struct,
                     * not a C++ Building (which has occupation_level at +0x88). */
                    vehicle->tile_x = *(int16_t*)((uint8_t*)ve_building + 0x88);
                } else if (Resource_IsBuildingTile(resource)) {
                    if (vehicle->occupancy != 0) {
                        if (vehicle->IsMoving()) {
                            vehicle->Stop((vehicle->active_editor == 0) ? 0 : 1, 1);
                            return 0;
                        }
                        vehicle->SetState(1);
                        vehicle->active_flag = 1;
                        vehicle->move_timer = 2;
                        return 0;
                    }
                    uint8_t started = ((GameVehicle*)this->building)->StartMoving(vehicle);
                    if (started) {
                        this->edit_state = 1;
                    }
                }

                if (*(void**)((uint8_t*)building_ptr + 0x118) == vehicle) {
                    *(void**)((uint8_t*)building_ptr + 0x118) = NULL;
                }
            }
        } else {
            int state_code = *(int32_t*)((uint8_t*)adj_result + 0x10C);
            switch (state_code) {
            case 8:
                vehicle->SetState(0);
                return 0;
            case 2:
                if (!this->HandleDirection(vehicle, (Building*)adj_result))
                    return 0;
                break;
            case 7: {
                int cleared = g_building_mgr->InvalidateRects(*(RECT*)((uint8_t*)adj_result + 0x08));
                if (cleared == 0) {
                    ((Entity*)adj_result)->StopSound(0);
                    Building* adj2 = this->FindAdjacentTrack();
                    if (adj2 == NULL) {
                        vehicle->SetState(2);
                        found_adjacent = 1;
                    } else {
                        vehicle->SetState(1);
                    }
                } else {
                    vehicle->SetState(1);
                    this->building = (Building*)adj_result;
                }
                if (!found_adjacent) return 0;
                break;
            }
            case 3:
                if (vehicle->IsMoving()) {
                    vehicle->Stop((vehicle->active_editor == 0) ? 0 : 1, 1);
                    return 0;
                }
                vehicle->SetState(1);
                vehicle->active_flag = 1;
                vehicle->move_timer = 2;
                return 0;
            default:
                vehicle->SetState(1);
                return 0;
            }
        }

        uint8_t attached = this->TryAttach(vehicle);
        if (attached) goto early_exit;

        if (vehicle->state == 1) {
            int32_t* par = (int32_t*)this->building;
            if (par == NULL) {
                vehicle->SetState(1);
                return 0;
            }

            int vstate = *(int32_t*)((uint8_t*)par + 0x10C);
            if (vstate == 1) {
                return vehicle->HandleStop();
            }
            if (vstate == 2) {
                if (*(int16_t*)((uint8_t*)par + 0x114) > 1) goto early_exit;

                int dir = this->direction;
                void* par_res = *(void**)((uint8_t*)par + 0x40);
                int cp_count = *(uint16_t*)((uint8_t*)par_res + 0x636);
                int max_idx  = *(uint16_t*)((uint8_t*)par_res + 0x638);

                if ((dir == 1 && this->track_pos == 1) ||
                    (dir == 0 && this->track_pos == cp_count - 1) ||
                    (dir == 1 && this->track_pos == cp_count + 2) ||
                    (dir == 0 && this->track_pos == max_idx - 1)) {
                    vehicle->SetState(2);
                    return 0;
                }

                if (*(int32_t*)((uint8_t*)par + 0x110) == 4) {
                    *(int32_t*)((uint8_t*)par + 0x110) = 5;
                    return 0;
                }
                if (*(int32_t*)((uint8_t*)par + 0x110) == 5) {
                    *(int32_t*)((uint8_t*)par + 0x110) = 4;
                    return 0;
                }
            } else if (vstate == 7) {
                /* NOTE: adj_result was set by FindAdjacentTrack() before
                 * this->building was potentially reassigned (line ~480).
                 * The binary re-reads the value at this point; both
                 * adj_result and building_ptr point to the same object
                 * after the reassignment, so this is safe. */
                int cleared = g_building_mgr->InvalidateRects(*(RECT*)((uint8_t*)adj_result + 0x08));
                if (cleared == 0) {
                    *(int32_t*)((uint8_t*)building_ptr + 0x110) = 5;
                    ((Entity*)building_ptr)->StopSound(0);
                    vehicle->SetState(2);
                    return 0;
                }
            } else if (found_adjacent) {
                vehicle->SetState(2);
                return 0;
            }
        }

        int32_t* cur_building = (int32_t*)this->building;
        if (cur_building == NULL) goto continue_loop;

        int cur_state = this->move_state;
        int cur_sub   = this->edit_state;

        if (cur_state == 0 && cur_sub == 0) {
            if (this->direction == 1) {
                this->track_pos++;
            } else {
                this->track_pos--;
            }
            state_changed = 1;
            void* res = *(void**)((uint8_t*)cur_building + 0x40);
            int32_t cp_array = *(int32_t*)((uint8_t*)res + 0x630);
            this->pos_x = ReadCP(cp_array, this->track_pos, 0) +
                *(int16_t*)((uint8_t*)cur_building + 0x88) * 16;
            this->pos_y = ReadCP(cp_array, this->track_pos, 2) +
                *(int16_t*)((uint8_t*)cur_building + 0x8A) * 16;
        } else if (cur_state == 1 || cur_sub == 1) {
            int dir_type = *(int8_t*)((uint8_t*)resource + 0x63A) - 1;
            switch (dir_type) {
            case 0: case 6: this->pos_x--; break;
            case 1: case 7: this->pos_x++; break;
            case 2: case 8: this->pos_y--; break;
            case 3: case 9: this->pos_y++; break;
            }
            state_changed = 1;
            if (cur_state == 1) {
                switch (dir_type) {
                case 0: if (this->pos_x < 1) this->move_state = 2; break;
                case 1: if (this->pos_x > g_world_width) this->move_state = 2; break;
                case 2: if (this->pos_y < 1) this->move_state = 2; break;
                case 3: if (this->pos_y >= g_world_height) this->move_state = 2; break;
                }
            }
        } else if (cur_state == 4) {
            void* cur_res = *(void**)((uint8_t*)cur_building + 0x40);
            int   dir_byte = *(int8_t*)((uint8_t*)cur_res + 0x63A);
            switch (dir_byte) {
            case 1: this->pos_x++; break;
            case 2: this->pos_x--; break;
            case 3: this->pos_y++; break;
            case 4: this->pos_y--; break;
            }
            state_changed = 1;
            int base_dir = dir_byte - 1;
            switch (base_dir) {
            case 0: if (this->pos_x >= 0) this->move_state = 0; break;
            case 1: if (this->pos_x < g_world_width) this->move_state = 0; break;
            case 2: if (this->pos_y >= 0) this->move_state = 0; break;
            case 3: if (this->pos_y <= g_world_height) this->move_state = 0; break;
            }
        } else if (cur_sub == 4 || cur_sub == 5) {
            state_changed = this->ScrollEdge();
            if (state_changed) {
                ((Entity*)cur_building)->StopSound(0);
            }
        }
    }

continue_loop:
    building_ptr = (int32_t*)this->building;
    if (building_ptr != NULL && *(int32_t*)((uint8_t*)building_ptr + 0x10C) == 4) {
        if ((vehicle->occupancy == 4 || vehicle->occupancy == 5) &&
            *(int32_t*)((uint8_t*)building_ptr + 0x11C) != 0) {
            ((Entity*)building_ptr)->StopSound(0);
            *(int32_t*)((uint8_t*)building_ptr + 0x11C) = 0;
            return 0;
        }
    }

early_exit:
    return state_changed;
}

/* ==================================================================== */
/* EditorState::UpdatePosition — Per-frame position update               */
/* Address: 0x40C580                                                     */
/* ==================================================================== */
uint32_t EditorState::UpdatePosition(Vehicle* vehicle, VehicleEditor* vehicleEditor)
{
    int32_t* building_ptr;
    void*    resource;

    building_ptr = (int32_t*)this->building;
    if (building_ptr == NULL) {
        return 0;
    }

    int move_st = this->move_state;

    if (move_st == 2) {
        void* res = *(void**)((uint8_t*)building_ptr + 0x40);
        int   dir_byte = *(int8_t*)((uint8_t*)res + 0x63A) - 1;
        switch (dir_byte) {
        case 0: case 6: this->pos_x--; return 1;
        case 1: case 7: this->pos_x++; return 1;
        case 2: case 8: this->pos_y--; return 1;
        case 3: case 9: this->pos_y++; return 1;
        }
        return 1;
    }

    resource = *(void**)((uint8_t*)building_ptr + 0x40);

    if (move_st != 4) {
        int sub_st = this->edit_state;

        if (sub_st == 4 || sub_st == 5) {
            if (move_st == 4) goto handle_edit_nudge;
            if (sub_st != 4 && sub_st != 5) goto done;
            goto handle_scroll_edge;
        }

        uint8_t valid_idx = Resource_IsValidTrackIndex(resource, (int16_t)this->track_pos);
        if (valid_idx != 1) goto after_track_advance;

        int32_t nearest_track = vehicle->GetNearestTrack();
        int32_t* nearest_ptr = (nearest_track != 0) ? (int32_t*)(intptr_t)nearest_track : NULL;

        Building* adj_result = this->FindAdjacentTrack();
        if (adj_result == NULL) {
            resource = *(void**)((uint8_t*)this->building + 0x40);

            if (vehicle->direction == 4 ||
                Resource_IsRoadTile(resource) == 0) {
                if (vehicle->occupancy != 4 &&
                    vehicle->occupancy != 5 &&
                    Resource_IsBuildingTile(resource) != 0) {
                    this->edit_state = 1;
                    vehicleEditor->edge_dir_b = 1;
                }
            } else {
                this->move_state = 1;
                vehicleEditor->edge_dir_a = 1;
            }

            if (nearest_ptr != NULL && *(int16_t*)((uint8_t*)nearest_ptr + 0x114) == 0) {
                ((Entity*)nearest_ptr)->StopSound(1);
            }
            goto after_track_advance;
        }

        void* adj_res = *(void**)((uint8_t*)this->building + 0x40);
        int dir_type = *(int8_t*)((uint8_t*)adj_res + 0x63A);
        switch (dir_type) {
        case 1: if (this->pos_x < 1) this->move_state = 2; break;
        case 2: if (this->pos_x > g_world_width) this->move_state = 2; break;
        case 3: if (this->pos_y < 1) this->move_state = 2; break;
        case 4: if (g_world_height <= this->pos_y) this->move_state = 2; break;
        }

        if (this->move_state != 2 && this->move_state != 3) {
            vehicle->SetState(1);
            goto after_track_advance;
        }

        switch (dir_type) {
        case 1: case 7: this->pos_x--; break;
        case 2: case 8: this->pos_x++; break;
        case 3: case 9: this->pos_y--; break;
        case 4: case 10: this->pos_y++; break;
        }
    }

after_track_advance:
    if (vehicle->state == 2 &&
        this->building != NULL &&
        this->move_state == 0 &&
        this->edit_state == 0) {
        if (this->direction == 1) {
            this->track_pos++;
        } else {
            this->track_pos--;
        }
        building_ptr = (int32_t*)this->building;
        resource = *(void**)((uint8_t*)building_ptr + 0x40);
        int32_t cp_array = *(int32_t*)((uint8_t*)resource + 0x630);
        this->pos_x = ReadCP(cp_array, this->track_pos, 0) +
            *(int16_t*)((uint8_t*)building_ptr + 0x88) * 16;
        this->pos_y = ReadCP(cp_array, this->track_pos, 2) +
            *(int16_t*)((uint8_t*)building_ptr + 0x8A) * 16;
        return 1;
    }

    if (this->move_state != 1 && this->edit_state != 1) {
        if (this->move_state != 4) {
            if (this->edit_state == 4) {
handle_scroll_edge:
                this->ScrollEdge();
                goto done;
            } else if (this->edit_state == 5) {
                this->ScrollEdge();
                return 1;
            }
            goto done;
        }

handle_edit_nudge:
        {
            void* cur_res = *(void**)((uint8_t*)this->building + 0x40);
            int   dir_byte = *(int8_t*)((uint8_t*)cur_res + 0x63A);
            switch (dir_byte) {
            case 1: this->pos_x++; break;
            case 2: this->pos_x--; break;
            case 3: this->pos_y++; break;
            case 4: this->pos_y--; break;
            }
            int base_dir = dir_byte - 1;
            switch (base_dir) {
            case 0: if (this->pos_x >= 0) { this->move_state = 0; return 1; } break;
            case 1: if (this->pos_x < g_world_width) { this->move_state = 0; return 1; } break;
            case 2: if (this->pos_y >= 0) { this->move_state = 0; return 1; } break;
            case 3: if (this->pos_y <= g_world_height) { this->move_state = 0; return 1; } break;
            }
            goto done;
        }
    }

    {
        void* par_res = *(void**)((uint8_t*)this->building + 0x40);
        int   dir_byte = *(int8_t*)((uint8_t*)par_res + 0x63A);
        switch (dir_byte) {
        case 1: case 7: this->pos_x--; break;
        case 2: case 8: this->pos_x++; break;
        case 3: case 9: this->pos_y--; break;
        case 4: case 10: this->pos_y++; break;
        }

        int base_dir = dir_byte - 1;
        switch (base_dir) {
        case 0: if (this->pos_x < 1) this->move_state = 2; break;
        case 1: if (g_world_width < this->pos_x) this->move_state = 2; break;
        case 2: if (this->pos_y < 1) this->move_state = 2; break;
        case 3: if (g_world_height <= this->pos_y) this->move_state = 2; break;
        }
    }

done:
    return 1;
}

/* ==================================================================== */
/* EditorState::TryAttach — Attach vehicle to station track              */
/* Address: 0x40C3D0                                                     */
/* ==================================================================== */
uint8_t EditorState::TryAttach(Vehicle* vehicle)
{
    uint8_t result = 0;
    Building* bld = this->building;
    if (bld == NULL) return 0;

    void* resource = *(void**)((uint8_t*)bld + 0x40);
    int8_t dir_type = *(int8_t*)((uint8_t*)resource + 0x63A);

    if (dir_type == 0x12 || dir_type == 0x13) {
        if (vehicle->move_timer == 1) {
            vehicle->move_timer = 0;
            return 0;
        }

        uint8_t occ_count = vehicle->GetOccupantCount();
        if (occ_count != 0 &&
            ((this->direction == 0 && this->track_pos == 1) ||
             (this->direction == 1 &&
              this->track_pos == *(uint16_t*)((uint8_t*)resource + 0x636) - 1))) {
            *(void**)((uint8_t*)bld + 0x118) = vehicle;
            vehicle->move_timer = 200;
            vehicle->SetState(1);
            result = 1;
        }
    }

    return result;
}

/* ==================================================================== */
/* EditorState::HandleDirection — Toggle train direction state           */
/* Address: 0x40C460                                                     */
/*                                                                       */
/* Verified against disassembly: returns 0 or 1 (uint8_t in AL).         */
/* ==================================================================== */
uint8_t EditorState::HandleDirection(Vehicle* vehicle, Building* train)
{
    if (*(int16_t*)((uint8_t*)train + 0x114) > 0) {
        vehicle->SetState(1);
        return 0;
    }

    int32_t  dir      = this->direction;
    void*    res      = *(void**)((uint8_t*)train + 0x40);
    uint16_t cp_count = *(uint16_t*)((uint8_t*)res + 0x636);
    uint16_t branch   = *(uint16_t*)((uint8_t*)res + 0x638);

    /* Check if at track boundary positions — if so, return 0 */
    if ((dir == 1 && this->track_pos == 0) ||
        (dir == 0 && this->track_pos == cp_count) ||
        (dir == 1 && this->track_pos == cp_count + 1) ||
        (dir == 0 && this->track_pos == branch)) {
        return 0;
    }

    /* Toggle exclusion state */
    if (*(int32_t*)((uint8_t*)train + 0x110) == 4) {
        *(int32_t*)((uint8_t*)train + 0x110) = 5;
    } else if (*(int32_t*)((uint8_t*)train + 0x110) == 5) {
        *(int32_t*)((uint8_t*)train + 0x110) = 4;
    }

    Building* adj_result = this->FindAdjacentTrack();
    if (adj_result == NULL) {
        /* Dead end — dispatch based on building state at +0x110 */
        Building* par = this->building;
        int state_110 = *(int32_t*)((uint8_t*)par + 0x110);
        if (state_110 == 4) {
            ((Entity*)par)->StopSound(1);
            return 1;
        }
        if (state_110 == 5) {
            ((Entity*)par)->StopSound(0);
        }
        return 1;
    }

    /* Adjacent found — toggle its exclusion */
    int adj_excl = *(int32_t*)((uint8_t*)train + 0x110);
    if (adj_excl == 4) {
        *(int32_t*)((uint8_t*)train + 0x110) = 5;
        return 0;
    }
    if (adj_excl == 5) {
        *(int32_t*)((uint8_t*)train + 0x110) = 4;
    }
    return 0;
}

/* ==================================================================== */
/* EditorState::ScrollEdge — Move one pixel along edge boundary          */
/* Address: 0x40CB10                                                     */
/*                                                                       */
/* NOTE: caller guarantees this->building != NULL. Checked by callers    */
/* (UpdateVehiclePlacement, UpdatePosition) before invoking.             */
/* ==================================================================== */
uint32_t EditorState::ScrollEdge()
{
    Building* bld   = this->building;  /* NOTE: caller-guaranteed non-NULL */
    void* resource  = *(void**)((uint8_t*)bld + 0x40);
    int8_t dir_type = *(int8_t*)((uint8_t*)resource + 0x63A);

    switch (dir_type) {
    case 7:  this->pos_x++; break;
    case 8:  this->pos_x--; break;
    case 9:  this->pos_y++; break;
    case 10: this->pos_y--; break;
    }

    int tile_x = *(int16_t*)((uint8_t*)bld + 0x88);
    int tile_y = *(int16_t*)((uint8_t*)bld + 0x8A);
    int span_x = *(uint8_t*)((uint8_t*)resource + 0x16B);
    int span_y = *(uint8_t*)((uint8_t*)resource + 0x16C);
    int within_bounds = 0;
    int check_x = 0, check_y = 0;

    switch (dir_type) {
    case 7:
        if (this->pos_x <= (tile_x + span_x) * 16 - 16) {
            check_x = this->pos_x; check_y = this->pos_y;
            within_bounds = 1;
        }
        break;
    case 8:
        if ((tile_x + 1) * 16 <= this->pos_x) {
            check_x = this->pos_x; check_y = this->pos_y;
            within_bounds = 1;
        }
        break;
    case 9:
        if (this->pos_y <= (tile_y + span_y - 2) * 16) {
            check_x = this->pos_x; check_y = this->pos_y;
            within_bounds = 1;
        }
        break;
    case 10:
        if ((tile_y + 1) * 16 <= this->pos_y) {
            check_x = this->pos_x; check_y = this->pos_y;
            within_bounds = 1;
        }
        break;
    }

    if (within_bounds) {
        this->FindTrackPosition(check_x, check_y);
        this->edit_state = 0;
    }

    return (this->edit_state == 0) ? 1 : 0;
}

/* ==================================================================== */
/* EditorState::CheckBounds — Check world boundaries                     */
/* Address: 0x40CC20                                                     */
/*                                                                       */
/* NOTE: caller guarantees this->building != NULL. Checked by callers    */
/* (UpdateVehiclePlacement, UpdatePosition) before invoking.             */
/* ==================================================================== */
void EditorState::CheckBounds()
{
    Building* bld = this->building;  /* NOTE: caller-guaranteed non-NULL */
    void* res = *(void**)((uint8_t*)bld + 0x40);
    int8_t dir_type = *(int8_t*)((uint8_t*)res + 0x63A);

    switch (dir_type) {
    case 1:
        if (this->pos_x < 1) { this->move_state = 2; return; }
        break;
    case 2:
        if (g_world_width < this->pos_x) { this->move_state = 2; return; }
        break;
    case 3:
        if (this->pos_y < 1) { this->move_state = 2; return; }
        break;
    case 4:
        if (g_world_height <= this->pos_y) { this->move_state = 2; }
        break;
    }
}

/* ==================================================================== */
/* EditorState::CheckBounds2 — Check tile edge bounds                    */
/* Address: 0x40CC90                                                     */
/*                                                                       */
/* NOTE: caller guarantees this->building != NULL. Checked by callers    */
/* (UpdateVehiclePlacement, UpdatePosition) before invoking.             */
/* ==================================================================== */
void EditorState::CheckBounds2()
{
    if (this->edit_state != 1) return;

    Building* bld      = this->building;  /* NOTE: caller-guaranteed non-NULL */
    void*    resource  = *(void**)((uint8_t*)bld + 0x40);
    int8_t   dir_type  = *(int8_t*)((uint8_t*)resource + 0x63A);
    int      tile_x    = *(int16_t*)((uint8_t*)bld + 0x88);
    int      tile_y    = *(int16_t*)((uint8_t*)bld + 0x8A);
    int      span_x    = *(uint8_t*)((uint8_t*)resource + 0x16B);
    int      span_y    = *(uint8_t*)((uint8_t*)resource + 0x16C);

    switch (dir_type) {
    case 7:
        if (this->pos_x < (tile_x + span_x) * 16 - 16) {
            this->edit_state = 2; return;
        }
        break;
    case 8:
        if ((tile_x + 1) * 16 < this->pos_x) {
            this->edit_state = 2; return;
        }
        break;
    case 9:
        if (this->pos_y < (tile_y + span_y) * 16 - 16) {
            this->edit_state = 2; return;
        }
        break;
    case 10:
        if ((tile_y + 1) * 16 < this->pos_y) {
            this->edit_state = 2;
        }
        break;
    }
}

/* ==================================================================== */
/* EditorState::UpdateEditMode — Per-frame edit mode state machine       */
/* Address: 0x40CD60                                                     */
/*                                                                       */
/* NOTE: caller guarantees this->building != NULL. Checked by callers    */
/* (UpdateVehiclePlacement, UpdatePosition) before invoking.             */
/* ==================================================================== */
void EditorState::UpdateEditMode(Vehicle* vehicle)
{
    void* resource = *(void**)((uint8_t*)this->building + 0x40);  /* NOTE: caller-guaranteed non-NULL */

    uint8_t valid = Resource_IsValidTrackIndex(resource, (int16_t)this->track_pos);
    if (valid == 0) {
        this->direction = (this->direction == 0) ? 1 : 0;
    } else {
        /* FindAdjacentTrack called for side effect only */
        this->FindAdjacentTrack();
        this->direction = (this->direction == 0) ? 1 : 0;
    }

    switch (this->move_state) {
    case 0:
        if (vehicle->direction == 1 &&
            *(int32_t*)((uint8_t*)this->building + 0x10C) == 3) {
            goto set_move_to_1;
        }
        break;
    case 1:
    case 2:
        this->move_state = 4;
        break;
    case 4:
    case 5:
set_move_to_1:
        this->move_state = 1;
        break;
    }

    if (this->move_state != 4) {
        int8_t dir_type = *(int8_t*)((uint8_t*)resource + 0x63A);
        switch (dir_type) {
        case 1: if (this->pos_x >= 0) this->move_state = 0; break;
        case 2: if (this->pos_x <= g_world_width) this->move_state = 0; break;
        case 3: if (this->pos_y >= 0) this->move_state = 0; break;
        case 4: if (this->pos_y <= g_world_height) this->move_state = 0; break;
        }

        if (this->move_state == 0) {
            if (*(int32_t*)((uint8_t*)this->building + 0x10C) == 3) {
                this->FindTrackPosition(this->pos_x, this->pos_y);
            }
        } else {
            switch (dir_type) {
            case 1: this->track_pos = *(uint16_t*)((uint8_t*)resource + 0x636) - 1; break;
            case 2: case 3: this->track_pos = 1; break;
            case 4: this->track_pos = *(uint16_t*)((uint8_t*)resource + 0x636) - 1; break;
            }
        }
    }

    switch (this->edit_state) {
    case 0: {
        if (*(int32_t*)((uint8_t*)this->building + 0x10C) != 4) return;
        int occ = vehicle->occupancy;
        if (occ == 4 || occ == 5) {
            this->edit_state = 1; return;
        }
        if (occ != 0) return;

        int32_t res_id = 0;
        void* par_res = *(void**)((uint8_t*)this->building + 0x40);
        if (par_res != NULL) {
            res_id = *(int32_t*)((uint8_t*)par_res + 4);
        } else {
            res_id = -1;
        }

        switch (res_id) {
        case 0xC54:
            if (this->direction == 0) { this->edit_state = 1; return; }
            return;
        case 0xC56:
        case 0xC5A:
            if (this->direction == 1) { this->edit_state = 1; return; }
            return;
        case 0xC58:
            if (this->direction != 0) return;
            break;
        default:
            return;
        }
        this->edit_state = 1;
        return;
    }
    case 1:
        this->edit_state = 4; return;
    case 4:
    case 5:
        this->edit_state = 1; return;
    default:
        return;
    }
}

/* ==================================================================== */
/* C-compatible bridge for world.c — not a binary function              */
/*                                                                       */
/* world.c is compiled as C (not C++) and cannot call C++ methods        */
/* directly. This shim provides C linkage to EditorState::Detach().      */
/* Address: N/A (bridge shim, not present in binary)                     */
/* ==================================================================== */
extern "C" void EditorState_DetachCompat(void* editor_state)
{
    ((EditorState*)editor_state)->Detach();
}
