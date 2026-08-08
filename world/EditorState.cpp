/**
 * EditorState.cpp — Per-endpoint track editor state machine implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: TRANSCRIBED
 *
 * Implements all 15 EditorState methods verified against disassembly.
 *
 * `this->building` is GameVehicle* (see EditorState.h's class-level doc
 * comment for the full retype evidence trail). Every offset this file
 * reads/writes through it now goes through a named field:
 *   +0x18  GameObject::initialized      +0x40  Entity::resource
 *   +0x88  ResourceGameObject::sub_pos_x        +0x8A  ::sub_pos_y
 *   +0x10C RESDATA_GameVehicle::vehicle_kind    +0x110 ::init_state
 *   +0x114 ::counter_timer              +0x118 ::boarding_vehicle
 *   +0x11C GameVehicle::occupant_state
 * The RESDATA (tile resource) pointer obtained via `->resource` is typed
 * TileMapResource* (world/tilemap.h) for the fields that header already
 * names and that sit BEFORE its +0x564 `expected_ids` member (resource_id
 * at +0x04, grid_span_y at +0x16B, original_span at +0x16C). state_63A
 * (+0x63A) is deliberately NOT read through the struct despite being a
 * named member there: `expected_ids` is declared `int32_t*` — a native,
 * host-width pointer inside a struct that otherwise mirrors an on-disk
 * 32-bit resource blob byte-for-byte — so on a 64-bit host it's 8 bytes
 * instead of 4, and every member after it (state_63A included) lands 4
 * bytes further out than the real asset data than the struct's own
 * `offsetof` claims. Verified directly: `static_assert(offsetof(
 * TileMapResource, state_63A) == 0x63A)` fails on this host (computes
 * 0x642). This is a pre-existing TileMapResource layout bug — it also
 * affects game/Building.cpp's and world/tilemap.cpp's own `->state_63A`
 * reads through the same struct, not introduced here — and fixing it
 * properly means reworking TileMapResource's on-disk-vs-host pointer
 * representation, out of scope for this cluster. ReadTileTypeByte() below
 * reads the fixed 32-bit byte offset directly instead of through the
 * struct. The track control-point array/count/branch-index fields at
 * +0x630/+0x636/+0x638 are past +0x564 too, but were never routed through
 * named struct members in the first place (always raw offset reads), so
 * they're unaffected by this same drift.
 */

#include "EditorState.h"
#include "../core/VehicleEditor.h"
#include "../core/Entity.h"
#include "../game/Vehicle.h"
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
extern TileMap* g_tilemap;          /* 0x4AAD08 */

/* ================================================================== */
/* External free functions — TODO: refactor to TileData class methods  */
/* ================================================================== */
/* External resource classification functions                          */
/* ================================================================== */
extern uint8_t  GetResourceType(uint32_t res_id);                     /* 0x446030 */

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

extern "C" void EditorState_DetachCompat(void* editor_state);

/* Reads the tile-type/direction byte at TileMapResource+0x63A directly,
 * bypassing TileMapResource::state_63A's compiler-computed offset — see
 * this file's header comment for why that offset drifts on 64-bit hosts. */
static inline uint8_t ReadTileTypeByte(void* resource)
{
    return *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(resource) + 0x63A);
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
    this->building = nullptr;

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
    if (this->building != nullptr && g_game_mode != 10) {
        this->building->counter_timer--;
        this->building = nullptr;
    }
}

/* ==================================================================== */
/* EditorState::Detach — Detach from parent building                     */
/* Address: 0x40B5A0                                                     */
/* ==================================================================== */
void EditorState::Detach()
{
    if (this->building != nullptr && g_game_mode != 10) {
        this->building->counter_timer--;
        this->building = nullptr;
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
    uint16_t i;
    int      result = 0;

    GameVehicle* gv = this->building;
    if (gv == nullptr) {
        return 0;
    }

    TileMapResource* resource = static_cast<TileMapResource*>(gv->resource);
    int16_t* control_points = *reinterpret_cast<int16_t**>(
        reinterpret_cast<uint8_t*>(resource) + 0x630);

    int tile_x_pixels = gv->sub_pos_x * 16;
    int tile_y_pixels = gv->sub_pos_y * 16;

    if (pixel_x == tile_x_pixels + control_points[0]) {
        if (pixel_y != tile_y_pixels + control_points[1]) {
            uint16_t cp_count = *reinterpret_cast<uint16_t*>(
                reinterpret_cast<uint8_t*>(resource) + 0x636);
            for (i = 0; i < cp_count; i++) {
                if (pixel_y - tile_y_pixels == control_points[i * 2 + 1]) {
                    this->track_pos = static_cast<int32_t>(i);
                    this->pos_y = control_points[i * 2 + 1] + tile_y_pixels;
                    result = 1;
                    break;
                }
            }
        }
    } else {
        uint16_t cp_count = *reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(resource) + 0x636);
        for (i = 0; i < cp_count; i++) {
            if (pixel_x - tile_x_pixels == control_points[i * 2]) {
                this->track_pos = static_cast<int32_t>(i);
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
    short    tile_x = static_cast<short>(pixel_x >> 4);
    short    tile_y = static_cast<short>(pixel_y >> 4);
    short    clamped_x = (pixel_x < 0) ? -1 : tile_x;
    short    clamped_y = (pixel_y < 0) ? -1 : tile_y;
    uint16_t i;
    int32_t  tile_origin[2];

    GameVehicle* gv = static_cast<GameVehicle*>(
        TileMap_GetObjectAt(g_tilemap, clamped_x, clamped_y, 0));
    this->building = gv;
    if (gv == nullptr) {
        return 0;
    }

    TileMapResource* resource = static_cast<TileMapResource*>(gv->resource);
    uint32_t resource_type = GetResourceType(static_cast<uint32_t>(resource->resource_id));
    if (resource_type != 3) {
        return resource_type & 0xFFFFFF00;
    }

    this->direction = 1;

    if (pixel_y < 0) tile_y = -1;
    if (pixel_x < 0) tile_x = -1;

    int32_t* origin_out = TileMap_GetTileOrigin(g_tilemap, tile_origin,
                                                 tile_x, tile_y, 0);
    gv->set_tile_target(*origin_out);

    int building_x_pixels = gv->sub_pos_x * 16;
    int16_t* cp_array = *reinterpret_cast<int16_t**>(
        reinterpret_cast<uint8_t*>(resource) + 0x630);
    uint16_t cp_count = *reinterpret_cast<uint16_t*>(
        reinterpret_cast<uint8_t*>(resource) + 0x636);
    for (i = 0; i < cp_count; i++) {
        if (pixel_x - building_x_pixels == cp_array[i * 2]) {
            this->track_pos = static_cast<uint32_t>(i);
            break;
        }
    }

    this->pos_x = cp_array[this->track_pos * 2]     + gv->sub_pos_x * 16;
    this->pos_y = cp_array[this->track_pos * 2 + 1] + gv->sub_pos_y * 16;

    return 1;
}

/* ==================================================================== */
/* EditorState::FindAdjacentTrack — Find adjacent track piece            */
/* Address: 0x40B880                                                     */
/*                                                                       */
/* Returns NULL on success (connected to adjacent track).                */
/* Returns current building pointer on failure.                          */
/* ==================================================================== */
GameVehicle* EditorState::FindAdjacentTrack()
{
    int pixel_x, pixel_y;
    short tile_x, tile_y;
    int tile_x_pixels, tile_y_pixels;
    int32_t tile_origin_packed[2];

    GameVehicle* gv = this->building;
    TileMapResource* resource = static_cast<TileMapResource*>(gv->resource);
    int16_t* control_points = *reinterpret_cast<int16_t**>(
        reinterpret_cast<uint8_t*>(resource) + 0x630);

    pixel_x = gv->sub_pos_x * 16 + control_points[this->track_pos * 2];
    pixel_y = gv->sub_pos_y * 16 + control_points[this->track_pos * 2 + 1];

    if (pixel_x < 0 || pixel_y < 0) {
        return gv;
    }

    tile_x = static_cast<short>(pixel_x >> 4);
    tile_y = static_cast<short>(pixel_y >> 4);

    TileMap_GetTileOriginEx(g_tilemap, tile_origin_packed, tile_x, tile_y, 0);
    int snapped_tile_x = tile_origin_packed[0] & 0xFFFF;
    int snapped_tile_y = (tile_origin_packed[0] >> 16) & 0xFFFF;

    if (snapped_tile_x < 0) {
        return gv;
    }

    tile_x_pixels = snapped_tile_x * 16;
    tile_y_pixels = snapped_tile_y * 16;

    GameVehicle* adjacent = static_cast<GameVehicle*>(
        TileMap_GetObjectAt(g_tilemap, tile_x, tile_y, 0));
    if (adjacent == nullptr) {
        return gv;
    }

    TileMapResource* adj_resource = static_cast<TileMapResource*>(adjacent->resource);
    uint32_t res_type = GetResourceType(static_cast<uint32_t>(adj_resource->resource_id));
    if (res_type != 3) {
        return gv;
    }

    uint16_t pt_count = *reinterpret_cast<uint16_t*>(
        reinterpret_cast<uint8_t*>(adj_resource) + 0x636);
    int16_t* adj_pts = *reinterpret_cast<int16_t**>(
        reinterpret_cast<uint8_t*>(adj_resource) + 0x630);

    int adj_type_code = ReadTileTypeByte(adj_resource);
    int adj_exclusion = adjacent->init_state;

    if (adj_exclusion == 4 && adj_type_code != 0x0D) {
        return gv;
    }

    if (pixel_x == adj_pts[0] + tile_x_pixels &&
        pixel_y == adj_pts[1] + tile_y_pixels) {
        gv->counter_timer--;
        this->building = adjacent;
        this->direction = 1;
        this->track_pos = 1;
        adjacent->counter_timer++;
        return nullptr;
    }

    int last_idx = pt_count * 2 - 2;
    if (pixel_x == adj_pts[last_idx] + tile_x_pixels &&
        pixel_y == adj_pts[last_idx + 1] + tile_y_pixels) {
        if (adj_type_code == 0x0B && adj_exclusion == 4) {
            uint16_t branch_idx = *reinterpret_cast<uint16_t*>(
                reinterpret_cast<uint8_t*>(adj_resource) + 0x638);
            if (adj_pts[0] != adj_pts[branch_idx * 2 + 2] ||
                adj_pts[1] != adj_pts[branch_idx * 2 + 3]) {
                goto check_branch;
            }
        }
        gv->counter_timer--;
        this->building = adjacent;
        this->direction = 0;
        this->track_pos = pt_count - 1;
        adjacent->counter_timer++;
        return nullptr;
    }

check_branch:
    if (*reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(adj_resource) + 0x638) != 0) {
        uint16_t branch_idx = *reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(adj_resource) + 0x638);

        if (pixel_x == adj_pts[pt_count * 2 + 4] + tile_x_pixels &&
            pixel_y == adj_pts[pt_count * 2 + 5] + tile_y_pixels) {
            if (adj_type_code == 0x0B && adj_exclusion == 5) {
                if (adj_pts[0] != adj_pts[branch_idx * 2 + 2] ||
                    adj_pts[1] != adj_pts[branch_idx * 2 + 3]) {
                    return gv;
                }
            }
            gv->counter_timer--;
            this->building = adjacent;
            this->direction = 1;
            this->track_pos = pt_count + 2;
            adjacent->counter_timer++;
            return nullptr;
        }

        if (pixel_x == adj_pts[branch_idx * 2 - 2] + tile_x_pixels &&
            pixel_y == adj_pts[branch_idx * 2 - 1] + tile_y_pixels) {
            if (adj_type_code == 0x0B && adj_exclusion == 5) {
                if (adj_pts[0] != adj_pts[branch_idx * 2 + 2] ||
                    adj_pts[1] != adj_pts[branch_idx * 2 + 3]) {
                    return gv;
                }
            }
            gv->counter_timer--;
            this->building = adjacent;
            this->direction = 0;
            this->track_pos = branch_idx - 1;
            adjacent->counter_timer++;
            return nullptr;
        }
    }

    return gv;
}

/* ==================================================================== */
/* EditorState::UpdateVehiclePlacement — Core placement algorithm         */
/* Address: 0x40BBD0                                                     */
/* ==================================================================== */
uint32_t EditorState::UpdateVehiclePlacement(Vehicle* vehicle)
{
    uint8_t      found_adjacent = 0;
    uint8_t      state_changed  = 0;
    GameVehicle* adj_result = nullptr;

    GameVehicle* gv = this->building;
    if (gv == nullptr || gv->initialized != 1) {
        return 0;
    }

    TileMapResource* resource = static_cast<TileMapResource*>(gv->resource);

    int step_state = this->move_state;
    int sub_state  = this->edit_state;

    if (step_state == 4) {
        int dir_type = ReadTileTypeByte(resource);
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
            state_changed = static_cast<uint8_t>(this->ScrollEdge());
            if (state_changed) {
                gv->StopSound(0);
            }
        }
    } else {
        uint8_t valid_idx = Resource_IsValidTrackIndex(resource, static_cast<int16_t>(this->track_pos));
        if (valid_idx == 1) {
            adj_result = this->FindAdjacentTrack();
        }

        if (adj_result == nullptr) {
            if (valid_idx == 1) {
                found_adjacent = 1;
                resource = static_cast<TileMapResource*>(this->building->resource);

                if (Resource_IsRoadTile(resource)) {
                    /* vehicle->editor_state is the paired EditorState for the
                     * opposite end of this track segment. */
                    GameVehicle* ve_building = vehicle->editor_state->building;
                    if (ve_building->occupant_state == 1 && vehicle->direction != 1) {
                        if (vehicle->IsMoving()) {
                            vehicle->Stop((vehicle->active_editor == 0) ? 0 : 1, 1);
                            return 0;
                        }
                        vehicle->SetState(1);
                        vehicle->active_flag = 1;
                        vehicle->move_timer = 2;
                        return 0;
                    }
                    ve_building->occupant_state = 1;
                    this->move_state = 1;
                    vehicle->direction = 1;
                    vehicle->tile_x = ve_building->sub_pos_x;
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
                    uint8_t started = this->building->StartMoving(vehicle);
                    if (started) {
                        this->edit_state = 1;
                    }
                }

                if (gv->boarding_vehicle == vehicle) {
                    gv->boarding_vehicle = nullptr;
                }
            }
        } else {
            int state_code = adj_result->vehicle_kind;
            switch (state_code) {
            case 8:
                vehicle->SetState(0);
                return 0;
            case 2:
                if (!this->HandleDirection(vehicle, adj_result))
                    return 0;
                break;
            case 7: {
                int cleared = g_building_mgr->InvalidateRects(adj_result->screen_rect);
                if (cleared == 0) {
                    adj_result->StopSound(0);
                    GameVehicle* adj2 = this->FindAdjacentTrack();
                    if (adj2 == nullptr) {
                        vehicle->SetState(2);
                        found_adjacent = 1;
                    } else {
                        vehicle->SetState(1);
                    }
                } else {
                    vehicle->SetState(1);
                    this->building = adj_result;
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
            GameVehicle* par = this->building;
            if (par == nullptr) {
                vehicle->SetState(1);
                return 0;
            }

            int vstate = par->vehicle_kind;
            if (vstate == 1) {
                return vehicle->HandleStop();
            }
            if (vstate == 2) {
                if (par->counter_timer > 1) goto early_exit;

                int dir = this->direction;
                TileMapResource* par_res = static_cast<TileMapResource*>(par->resource);
                int cp_count = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(par_res) + 0x636);
                int max_idx  = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(par_res) + 0x638);

                if ((dir == 1 && this->track_pos == 1) ||
                    (dir == 0 && this->track_pos == cp_count - 1) ||
                    (dir == 1 && this->track_pos == cp_count + 2) ||
                    (dir == 0 && this->track_pos == max_idx - 1)) {
                    vehicle->SetState(2);
                    return 0;
                }

                if (par->init_state == 4) {
                    par->init_state = 5;
                    return 0;
                }
                if (par->init_state == 5) {
                    par->init_state = 4;
                    return 0;
                }
            } else if (vstate == 7) {
                /* NOTE: adj_result was set by FindAdjacentTrack() before
                 * this->building was potentially reassigned (case 7 above).
                 * The binary re-reads the value at this point; both
                 * adj_result and gv point to the same object after the
                 * reassignment, so this is safe. */
                int cleared = g_building_mgr->InvalidateRects(adj_result->screen_rect);
                if (cleared == 0) {
                    gv->init_state = 5;
                    gv->StopSound(0);
                    vehicle->SetState(2);
                    return 0;
                }
            } else if (found_adjacent) {
                vehicle->SetState(2);
                return 0;
            }
        }

        GameVehicle* cur_building = this->building;
        if (cur_building == nullptr) goto continue_loop;

        int cur_state = this->move_state;
        int cur_sub   = this->edit_state;

        if (cur_state == 0 && cur_sub == 0) {
            if (this->direction == 1) {
                this->track_pos++;
            } else {
                this->track_pos--;
            }
            state_changed = 1;
            TileMapResource* res = static_cast<TileMapResource*>(cur_building->resource);
            int16_t* cp_array = *reinterpret_cast<int16_t**>(reinterpret_cast<uint8_t*>(res) + 0x630);
            this->pos_x = cp_array[this->track_pos * 2]     + cur_building->sub_pos_x * 16;
            this->pos_y = cp_array[this->track_pos * 2 + 1] + cur_building->sub_pos_y * 16;
        } else if (cur_state == 1 || cur_sub == 1) {
            int dir_type = ReadTileTypeByte(resource) - 1;
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
            TileMapResource* cur_res = static_cast<TileMapResource*>(cur_building->resource);
            int dir_byte = ReadTileTypeByte(cur_res);
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
            state_changed = static_cast<uint8_t>(this->ScrollEdge());
            if (state_changed) {
                cur_building->StopSound(0);
            }
        }
    }

continue_loop:
    gv = this->building;
    if (gv != nullptr && gv->vehicle_kind == 4) {
        if ((vehicle->occupancy == 4 || vehicle->occupancy == 5) && gv->occupant_state != 0) {
            gv->StopSound(0);
            gv->occupant_state = 0;
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
    GameVehicle* gv = this->building;
    if (gv == nullptr) {
        return 0;
    }

    int move_st = this->move_state;

    if (move_st == 2) {
        TileMapResource* res = static_cast<TileMapResource*>(gv->resource);
        int dir_byte = ReadTileTypeByte(res) - 1;
        switch (dir_byte) {
        case 0: case 6: this->pos_x--; return 1;
        case 1: case 7: this->pos_x++; return 1;
        case 2: case 8: this->pos_y--; return 1;
        case 3: case 9: this->pos_y++; return 1;
        }
        return 1;
    }

    TileMapResource* resource = static_cast<TileMapResource*>(gv->resource);

    if (move_st != 4) {
        int sub_st = this->edit_state;

        if (sub_st == 4 || sub_st == 5) {
            if (move_st == 4) goto handle_edit_nudge;
            if (sub_st != 4 && sub_st != 5) goto done;
            goto handle_scroll_edge;
        }

        uint8_t valid_idx = Resource_IsValidTrackIndex(resource, static_cast<int16_t>(this->track_pos));
        if (valid_idx != 1) goto after_track_advance;

        int32_t nearest_track = vehicle->GetNearestTrack();
        GameVehicle* nearest = (nearest_track != 0)
            ? reinterpret_cast<GameVehicle*>(static_cast<intptr_t>(nearest_track))
            : nullptr;

        GameVehicle* adj_result = this->FindAdjacentTrack();
        if (adj_result == nullptr) {
            resource = static_cast<TileMapResource*>(this->building->resource);

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

            if (nearest != nullptr && nearest->counter_timer == 0) {
                nearest->StopSound(1);
            }
            goto after_track_advance;
        }

        TileMapResource* adj_res = static_cast<TileMapResource*>(this->building->resource);
        int dir_type = ReadTileTypeByte(adj_res);
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
        this->building != nullptr &&
        this->move_state == 0 &&
        this->edit_state == 0) {
        if (this->direction == 1) {
            this->track_pos++;
        } else {
            this->track_pos--;
        }
        gv = this->building;
        resource = static_cast<TileMapResource*>(gv->resource);
        int16_t* cp_array = *reinterpret_cast<int16_t**>(reinterpret_cast<uint8_t*>(resource) + 0x630);
        this->pos_x = cp_array[this->track_pos * 2]     + gv->sub_pos_x * 16;
        this->pos_y = cp_array[this->track_pos * 2 + 1] + gv->sub_pos_y * 16;
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
            TileMapResource* cur_res = static_cast<TileMapResource*>(this->building->resource);
            int dir_byte = ReadTileTypeByte(cur_res);
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
        TileMapResource* par_res = static_cast<TileMapResource*>(this->building->resource);
        int dir_byte = ReadTileTypeByte(par_res);
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
    GameVehicle* gv = this->building;
    if (gv == nullptr) return 0;

    TileMapResource* resource = static_cast<TileMapResource*>(gv->resource);
    int8_t dir_type = ReadTileTypeByte(resource);

    if (dir_type == 0x12 || dir_type == 0x13) {
        if (vehicle->move_timer == 1) {
            vehicle->move_timer = 0;
            return 0;
        }

        uint8_t occ_count = vehicle->GetOccupantCount();
        uint16_t cp_count = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(resource) + 0x636);
        if (occ_count != 0 &&
            ((this->direction == 0 && this->track_pos == 1) ||
             (this->direction == 1 && this->track_pos == cp_count - 1))) {
            gv->boarding_vehicle = vehicle;
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
uint8_t EditorState::HandleDirection(Vehicle* vehicle, GameVehicle* train)
{
    if (train->counter_timer > 0) {
        vehicle->SetState(1);
        return 0;
    }

    int32_t  dir      = this->direction;
    TileMapResource* res = static_cast<TileMapResource*>(train->resource);
    uint16_t cp_count = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(res) + 0x636);
    uint16_t branch   = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(res) + 0x638);

    /* Check if at track boundary positions — if so, return 0 */
    if ((dir == 1 && this->track_pos == 0) ||
        (dir == 0 && this->track_pos == cp_count) ||
        (dir == 1 && this->track_pos == cp_count + 1) ||
        (dir == 0 && this->track_pos == branch)) {
        return 0;
    }

    /* Toggle exclusion state */
    if (train->init_state == 4) {
        train->init_state = 5;
    } else if (train->init_state == 5) {
        train->init_state = 4;
    }

    GameVehicle* adj_result = this->FindAdjacentTrack();
    if (adj_result == nullptr) {
        /* Dead end — dispatch based on building state at +0x110 */
        GameVehicle* par = this->building;
        int state_110 = par->init_state;
        if (state_110 == 4) {
            par->StopSound(1);
            return 1;
        }
        if (state_110 == 5) {
            par->StopSound(0);
        }
        return 1;
    }

    /* Adjacent found — toggle its exclusion */
    int adj_excl = train->init_state;
    if (adj_excl == 4) {
        train->init_state = 5;
        return 0;
    }
    if (adj_excl == 5) {
        train->init_state = 4;
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
    GameVehicle* gv = this->building;  /* NOTE: caller-guaranteed non-NULL */
    TileMapResource* resource = static_cast<TileMapResource*>(gv->resource);
    int8_t dir_type = ReadTileTypeByte(resource);

    switch (dir_type) {
    case 7:  this->pos_x++; break;
    case 8:  this->pos_x--; break;
    case 9:  this->pos_y++; break;
    case 10: this->pos_y--; break;
    }

    int tile_x = gv->sub_pos_x;
    int tile_y = gv->sub_pos_y;
    int span_x = resource->grid_span_y;
    int span_y = resource->original_span;
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
    GameVehicle* gv = this->building;  /* NOTE: caller-guaranteed non-NULL */
    TileMapResource* resource = static_cast<TileMapResource*>(gv->resource);
    int8_t dir_type = ReadTileTypeByte(resource);

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

    GameVehicle* gv = this->building;  /* NOTE: caller-guaranteed non-NULL */
    TileMapResource* resource = static_cast<TileMapResource*>(gv->resource);
    int8_t dir_type = ReadTileTypeByte(resource);
    int    tile_x   = gv->sub_pos_x;
    int    tile_y   = gv->sub_pos_y;
    int    span_x   = resource->grid_span_y;
    int    span_y   = resource->original_span;

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
    /* `resource` is captured once, from this->building at entry, and
     * stays frozen for the rest of the function — matches the original
     * (0x40CD60 loads it once into a register and never reloads it).
     * this->building itself, however, IS re-read fresh at every site
     * below: FindAdjacentTrack() (called below for its side effect) can
     * reassign it to an adjacent node, and the disassembly re-dereferences
     * `this+0x14` after that call at each of the vehicle_kind/resource_id
     * checks rather than reusing a cached copy. Do not collapse those
     * fresh `this->building->...` reads back into a single frozen local —
     * that would silently change behavior whenever FindAdjacentTrack()
     * actually advances to a new node. */
    TileMapResource* resource = static_cast<TileMapResource*>(this->building->resource);

    uint8_t valid = Resource_IsValidTrackIndex(resource, static_cast<int16_t>(this->track_pos));
    if (valid == 0) {
        this->direction = (this->direction == 0) ? 1 : 0;
    } else {
        /* FindAdjacentTrack called for side effect only — may reassign
         * this->building to an adjacent node. */
        this->FindAdjacentTrack();
        this->direction = (this->direction == 0) ? 1 : 0;
    }

    switch (this->move_state) {
    case 0:
        if (vehicle->direction == 1 && this->building->vehicle_kind == 3) {
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
        int8_t dir_type = ReadTileTypeByte(resource);
        switch (dir_type) {
        case 1: if (this->pos_x >= 0) this->move_state = 0; break;
        case 2: if (this->pos_x <= g_world_width) this->move_state = 0; break;
        case 3: if (this->pos_y >= 0) this->move_state = 0; break;
        case 4: if (this->pos_y <= g_world_height) this->move_state = 0; break;
        }

        if (this->move_state == 0) {
            if (this->building->vehicle_kind == 3) {
                this->FindTrackPosition(this->pos_x, this->pos_y);
            }
        } else {
            uint16_t cp_count = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(resource) + 0x636);
            switch (dir_type) {
            case 1: this->track_pos = cp_count - 1; break;
            case 2: case 3: this->track_pos = 1; break;
            case 4: this->track_pos = cp_count - 1; break;
            }
        }
    }

    switch (this->edit_state) {
    case 0: {
        if (this->building->vehicle_kind != 4) return;
        int occ = vehicle->occupancy;
        if (occ == 4 || occ == 5) {
            this->edit_state = 1; return;
        }
        if (occ != 0) return;

        void* par_res = this->building->resource;
        int32_t res_id = (par_res != nullptr)
            ? static_cast<TileMapResource*>(par_res)->resource_id
            : -1;

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
    static_cast<EditorState*>(editor_state)->Detach();
}
