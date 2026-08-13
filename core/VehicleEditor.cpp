/**
 * VehicleEditor.cpp — VehicleEditor implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: TRANSCRIBED
 */

#include "VehicleEditor.h"
#include "../game/Building.h"
#include "../game/GameVehicle.h"
#include "../game/Vehicle.h"
#include "../network/DPlayManager.h"
#include <cmath>
#include <cstdio>
#include <new>
#include <cassert>

/* Forward-declared rather than including resources/resource_manager_sdl3.h
 * wholesale, matching game/Vehicle.cpp's identical forward declarations
 * (that header's ResourceManager_Init(void*) -> int ambiguates against
 * network/Netman.h's ResourceManager_Init(void*) -> void once both are
 * visible in one TU). */
namespace loco::assets {
bool is_host_sprite_resource(const void* resource);
bool sprite_tile_type_byte(const void* resource, uint8_t* out_byte);
}  // namespace loco::assets

namespace {

template <typename T>
T* field_at(void* object, size_t offset)
{
    return reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
}

} // namespace

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void*  operator_new(size_t size);                                   /* 0x465CE0 */
void   GLOBAL_free(void* ptr);                                      /* 0x465CD0 */

extern "C" {
    void   SetRect(void* rect, int left, int top, int right, int bottom); /* USER32 */
    BOOL   IntersectRect(void* out, const void* a, const void* b);      /* USER32 */
}

/* GameObject / Entity base methods */
extern void GameObject_BaseCtor(void* obj, int res_id, short anim_idx, int x, int y);  /* 0x405790 */
extern void GameObject_DtorBody(void* obj);                                             /* 0x405870 */
extern void GameObject_SetFrame(void* obj, int frame_id, bool trigger);                /* 0x405DE0 */
extern void GameObject_PlayAnimation(void* obj, int snd_id);                           /* 0x405AB0 */

extern void GameObject_HitTest(void* obj, int x, int y);             /* vtable[3] @ 0x40D8E0 caller */
extern void GameObject_MarkDead(void* obj);                                             /* 0x405A70 */
extern void GameObject_InvalidateRect(void* obj);                                       /* 0x405A20 */

/* DPLAY */
extern void   DPLAY_CreatePlayer(void* slot);                                           /* 0x442850 */
extern void   DPLAY_CleanupPlayer(void* slot);                                          /* 0x442A00 */

/* Resource classification (same extern used by EditorState.cpp).
 * Resource_IsRoadTile (0x44BD10) / Resource_IsBuildingTile (0x44BD30) are
 * NOT declared here: their extern declarations collide at link time with a
 * void-returning stub in shared/link_stubs.cpp (C++ return type isn't part
 * of Itanium mangling, so both mangle identically and the stub silently
 * wins). Call sites use ClassifyResourceTile() (game/Vehicle.h) instead. */
extern uint8_t  Resource_IsValidTrackIndex(void* resource, int16_t idx);               /* 0x44BCD0 */
extern unsigned int __cdecl CGWND_MapResourceToDirection(int resource_id);              /* 0x40EB60 —
                                                     same real function as game/World.cpp's
                                                     `uint`-returning declaration (equivalent
                                                     type; spelled out here since this TU
                                                     doesn't otherwise pull in the typedef and
                                                     mingw's headers don't provide the bare
                                                     `uint` alias), used by
                                                     VehicleEditor::SetResourceId */

/* Vehicle / Train functions */
extern char   Vehicle_GetOccupantCount(int vehicle);                                    /* 0x44C370 */
extern uint32_t Vehicle_SetState(void* vehicle, int state);                             /* 0x44D740 */
extern void*  Vehicle_GetNearestTrack(int vehicle);                                     /* 0x44D4C0 */
extern void   Vehicle_DetachAll(int vehicle);                                            /* 0x44D610 */

/* TileMap */
struct TileMap { int dummy; };
extern TileMap g_tilemap;                                                               /* global */
extern void    TileMap_InvalidateRect(TileMap* tm, int left, int top, int right, int bottom); /* 0x430820 */

/* Misc */
extern int     g_world_width;        /* 0x4AAD0C — world width in pixels */
extern int     g_world_height;       /* 0x4AAD10 — world height in pixels */
extern void*   g_primary_surface;    /* primary DirectDraw surface */
/* Real def: ui/UIPANEL_Surface.cpp, bool(void*,uint32_t,uint32_t,int32_t,
 * uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t) — was declared
 * uniformly `int`, which doesn't match the real mixed uint32_t/int32_t
 * shape. Not currently called from this file (declaration only), so this
 * wasn't a live call-0 landmine, but the declaration is now correct if a
 * caller is added. */
extern bool    UIPANEL_Blit(void* panel, uint32_t dst_l, uint32_t dst_t, int32_t dst_r, uint32_t dst_b,
                            void* surface, uint32_t src_l, uint32_t src_t, int32_t src_r, uint32_t src_b,
                            uint32_t flags);


/* ================================================================== */
/* VehicleEditor constructor                                           */
/* Address: 0x40D500                                                   */
/* ================================================================== */
VehicleEditor::VehicleEditor(int res_id, int param_2, char flag) : Entity(res_id, -1, 0, 0)
{
#ifdef _WIN32
    auto* embedded_dplay = ::new (this->dplay_data) DPlayManager;
    embedded_dplay->CreatePlayer();
#else
    void* dplay_storage = operator_new(sizeof(DPlayManager));
    this->host_dplay_data = dplay_storage != nullptr
        ? ::new (dplay_storage) DPlayManager : nullptr;
    if (this->host_dplay_data != nullptr) this->host_dplay_data->CreatePlayer();
#endif
    /* In the binary: sets vtable here. Compiler-managed in natural C++. */

    this->end_a = nullptr;
    this->end_b = nullptr;
    this->target_building = nullptr;
    this->res_id = res_id;

    if (this->initialized != 1) {
        return;
    }

    this->res_id_2 = param_2;

    /* In the original binary: operator_new(0x20) allocates raw memory,
       then EditorState::EditorState(char) placement-constructs on it.
       In C++, 'new EditorState(flag)' handles both allocation and
       construction in one call. */
    this->end_a = new EditorState(flag);
    this->end_b = new EditorState(flag);

    this->angle_frame = 0;
    this->second_angle = 0;
    this->unknown_flag = 0;

    if (flag == 0) {
        this->edge_dir_b = 2;
        this->edge_dir_a = 0;
    } else {
        this->edge_dir_b = 0;
        this->edge_dir_a = 2;
    }

    this->bound_check_flag = 0;
    SetRect(&this->screen_rect, 0, 0, 0, 0);
    GameObject_SetFrame(this, this->angle_frame, true);
    this->dplay_initialized = 0;
    this->visible = 0;
}

#ifndef _WIN32
/** Host-only network editor: preserves 0x40D500 logical state without
 * invoking Entity::InitBase on the original pointer-based resource ABI. */
VehicleEditor::VehicleEditor(HostNetworkEditorTag, int resource_id,
                             int secondary_resource_id, char flag)
    : Entity(0, -1, 0, 0)
{
    void* dplay_storage = operator_new(sizeof(DPlayManager));
    this->host_dplay_data = dplay_storage != nullptr
        ? ::new (dplay_storage) DPlayManager : nullptr;
    if (this->host_dplay_data != nullptr) this->host_dplay_data->CreatePlayer();
    this->dplay_initialized = 0;
    this->end_a = new EditorState(flag);
    this->end_b = new EditorState(flag);
    this->target_building = nullptr;
    this->res_id = resource_id;
    this->res_id_2 = secondary_resource_id;
    this->angle_frame = 0;
    this->second_angle = 0;
    this->unknown_flag = 0;
    this->edge_dir_a = flag == 0 ? 0 : 2;
    this->edge_dir_b = flag == 0 ? 2 : 0;
    this->bound_check_flag = 0;
    SetRect(&this->screen_rect, 0, 0, 0, 0);
    this->visible = 0;
}
#endif

/* ================================================================== */
/* VehicleEditor destructor (vtable[0] scalar deleting wrapper at      */
/* 0x40D660 delegates to this body at 0x40D680)                        */
/* ================================================================== */
VehicleEditor::~VehicleEditor()
{
    /* In the binary: sets vtable here. Compiler-managed in natural C++. */

    if (this->end_a != nullptr) {
        delete this->end_a;
        this->end_a = nullptr;
    }

    if (this->end_b != nullptr) {
        delete this->end_b;
        this->end_b = nullptr;
    }

    if (this->target_building != nullptr) {
        if (static_cast<Building*>(this->target_building)->occupation_level == 0) {
            TileMap_InvalidateRect(
                &g_tilemap,
                this->screen_rect.left,
                this->screen_rect.top,
                this->screen_rect.right,
                this->screen_rect.bottom
            );
        }
        this->target_building = nullptr;
    }

#ifdef _WIN32
    reinterpret_cast<DPlayManager*>(this->dplay_data)->~DPlayManager();
#else
    if (this->host_dplay_data != nullptr) {
        this->host_dplay_data->~DPlayManager();
        GLOBAL_free(this->host_dplay_data);
        this->host_dplay_data = nullptr;
    }
#endif
    GameObject_DtorBody(this);
}


/* ================================================================== */
/* VehicleEditor::InitTracks                                           */
/* Address: 0x40D890                                                   */
/* ================================================================== */
uint32_t VehicleEditor::InitTracks(int x, int y)
{
    if (this->end_a == nullptr) {
        return 0;
    }

    this->end_a->InitTrackAtPosition(x, y);
    this->end_b->InitTrackAtPosition(
        this->end_a->pos_x + 0x16,
        this->end_a->pos_y
    );

    this->SetRenderOffset();
    return 1;
}


/* ================================================================== */
/* VehicleEditor::SetRenderOffset — vtable[3]                          */
/* Address: 0x40D8E0                                                   */
/*                                                                     */
/* Recomputes screen_rect from end_a's position minus resource tile    */
/* origin offsets (indexed by angle_frame). After updating the rect,   */
/* dispatches vtable[3] (HitTest) with the new left/top coordinates    */
/* to notify the system of the updated position.                       */
/*                                                                     */
/* NOTE: The resource pointer is loaded from the inherited Entity      */
/* parent slot at +0x40, which is repurposed to store the resource     */
/* pointer (set during GameObject_BaseCtor). This is NOT accessed      */
/* through end_a->building->resource like in other methods.            */
/* ================================================================== */
void VehicleEditor::SetRenderOffset()
{
    /* Load resource pointer from repurposed Entity parent slot */             /* +0x40 */
    void* resource = this->resource;

    int tile_off_x = *reinterpret_cast<int16_t*>(
        static_cast<uint8_t*>(resource) + 0x168 +
        static_cast<uint32_t>(this->angle_frame) * 4);
    int tile_off_y = *reinterpret_cast<int16_t*>(
        static_cast<uint8_t*>(resource) + 0x16a +
        static_cast<uint32_t>(this->angle_frame) * 4);

    int left = this->end_a->pos_x - tile_off_x;
    int top = this->end_a->pos_y - tile_off_y;

    this->screen_rect.left = left;
    this->screen_rect.top = top;
    this->screen_rect.right = *field_at<uint16_t>(resource, 0x14) + left;
    this->screen_rect.bottom = *field_at<uint16_t>(resource, 0x16) + top;

    /* Dispatch vtable[3] (HitTest/position update) with new coordinates */
    GameObject_HitTest(this, left, top);
}


/* ================================================================== */
/* VehicleEditor::ProcessMove — Per-frame editing update               */
/* Address: 0x40D940                                                   */
/*                                                                     */
/* Called every frame while track editing is active.                   */
/* ================================================================== */
void VehicleEditor::ProcessMove(Vehicle* vehicle)
{
    /* Determine the "road building" — which editor state (end A or B) */
    /* tracks the road segment for non-road-to-road transitions.        */
    /* GameVehicle*, not Building* — see world/EditorState.h's class-level
     * doc comment for the EditorState::building retype evidence; every
     * offset this function reads through these pointers (+0x10C, +0x14 as
     * screen_rect.bottom, +0x11C) is GameObject/RESDATA_GameVehicle/
     * GameVehicle-shaped, not Building-shaped. */
    GameVehicle* road_building = nullptr;

    if (vehicle->active_editor == 0) {
        /* Active index 0: use end B from vehicle's child entry */
        uint32_t child_idx = vehicle->editor_count;
        /* The original stores a Vehicle handle in this editor slot. */
        Vehicle* child_veh = reinterpret_cast<Vehicle*>(vehicle->editors[child_idx]);
        VehicleEditor* child_ed = static_cast<VehicleEditor*>(child_veh->editors[0]); /* +0x434 = end_b */
        road_building = child_ed->end_b->building;
    } else {
        /* The original stores a Vehicle handle in this editor slot. */
        Vehicle* child_veh = reinterpret_cast<Vehicle*>(vehicle->editors[0]);
        VehicleEditor* child_ed = static_cast<VehicleEditor*>(child_veh->editors[0]); /* +0x430 = end_a */
        road_building = child_ed->end_a->building;
    }

    char moved = 0;

    /* Get track nodes for both editor ends */
    GameVehicle* bldg_a = this->end_a->building;
    GameVehicle* bldg_b = this->end_b->building;

    if (bldg_a != nullptr && bldg_b != nullptr) {
        int substate_a = bldg_a->vehicle_kind;   /* +0x10C */
        int substate_b = bldg_b->vehicle_kind;   /* +0x10C */

        if (substate_a == 5 && substate_b == 5 && bldg_a == bldg_b) {
            /* Both ends on the same road building */
            vehicle->detach_flag = 1;

            /* Host deviation: `resource` is a loco::assets::SpriteResource*
             * on this build, not the real x86 TileMapResource* a raw
             * +0x63A read assumes (the "raw fixed-offset reads against
             * undersized host resource objects" landmine already fixed
             * in ScrollRect/InputMgr.cpp/ResdataGameVehicle.cpp/
             * Vehicle::LoadSounds). */
            void* resource = bldg_a->resource;
            uint8_t res_type = 0;
#ifndef _WIN32
            if (!loco::assets::is_host_sprite_resource(resource) ||
                !loco::assets::sprite_tile_type_byte(resource, &res_type)) {
                res_type = 0;
            }
#else
            res_type = *field_at<char>(resource, 0x63a);
#endif

            if (res_type == 5) {
                /* Road type — move along track */
                moved = static_cast<char>(this->MoveAlongTrack(vehicle));
            } else {
                /* Non-road bridge conditions. +0x14 is GameObject::screen_rect.bottom
                 * (RECT = left/top/right/bottom at +0x08/+0x0C/+0x10/+0x14). */
                int a_limit = bldg_a->screen_rect.bottom - 0x20;
                int b_limit = bldg_b->screen_rect.bottom - 0x20;

                if ((a_limit < this->end_a->pos_y) ||
                    (b_limit < this->end_b->pos_y) ||
                    (bldg_a != bldg_b))
                {
                    this->bound_check_flag = 0;
                    moved = 0;
                } else {
                    this->bound_check_flag = 1;
                    moved = 0;
                }
            }
        } else {
            this->bound_check_flag = 0;
            Vehicle_DetachAll(static_cast<int>(reinterpret_cast<intptr_t>(vehicle)));
        }
    }

    /* If MoveAlongTrack didn't handle it, update each EditorState position */
    if (moved == 0) {
        uint32_t result_a = this->end_a->UpdatePosition(vehicle, this);
        uint32_t result_b = this->end_b->UpdatePosition(vehicle, this);

        if (result_a || result_b) {
            TileMap_InvalidateRect(
                &g_tilemap,
                this->screen_rect.left,
                this->screen_rect.top,
                this->screen_rect.right,
                this->screen_rect.bottom
            );

            this->CalcAngle();
            GameObject_SetFrame(this, this->angle_frame, true);
            this->SetRenderOffset();
        }

        /* Check edge/bridge conditions */
        if (vehicle->direction != 0) {
            int edge_check = this->edge_dir_a;
            if (edge_check == 0 || edge_check == 1) {
                if (edge_check == 1) {
                    if (vehicle->active_editor == 0 &&
                        this->end_b->move_state == 2)
                    {
                        this->CheckEdgeBounds(vehicle);
                    } else if (vehicle->active_editor == 1 &&
                               this->end_a->move_state == 2)
                    {
                        this->CheckEdgeBounds(vehicle);
                    }
                }
            }
            if (edge_check == 4) {
                this->CheckVehicleAttach(vehicle);
            }
        }

        if (vehicle->occupancy != 0) {
            this->CheckBridge(vehicle);
        }
    }

    /* Road-to-building transition check — same road_building all the way through */
    if (road_building != nullptr) {
        void* road_res = road_building->resource;
        if (road_res != nullptr) {
            /* Resource_IsRoadTile (0x44BD10) mangles identically to its
             * void-returning no-op stub in shared/link_stubs.cpp
             * (_Z18Resource_IsRoadTilePv -- return type isn't part of
             * Itanium mangling), so every call through this declaration
             * already binds to the stub and reads garbage -- independent
             * of the host-SpriteResource landmine below. See
             * game/Vehicle.h's ClassifyResourceTile() for the fix, shared
             * across this file, game/Vehicle.cpp, and
             * world/EditorState.cpp. */
            bool is_road, unused_is_building;
            ClassifyResourceTile(road_res, &is_road, &unused_is_building);
            if (is_road) {
                GameVehicle* target_bldg = nullptr;
                if (vehicle->active_editor == 0) {
                    uint32_t child_idx = vehicle->editor_count;
                    Vehicle* child_veh = reinterpret_cast<Vehicle*>(vehicle->editors[child_idx]);
                    VehicleEditor* child_ed = static_cast<VehicleEditor*>(child_veh->editors[0]);
                    target_bldg = child_ed->end_b->building;
                } else {
                    Vehicle* child_veh = reinterpret_cast<Vehicle*>(vehicle->editors[0]);
                    VehicleEditor* child_ed = static_cast<VehicleEditor*>(child_veh->editors[0]);
                    target_bldg = child_ed->end_a->building;
                }

                if (target_bldg != nullptr) {
                    void* target_res = target_bldg->resource;
                    if (target_res != nullptr) {
                        bool target_is_road, target_unused_is_building;
                        ClassifyResourceTile(target_res, &target_is_road, &target_unused_is_building);
                        if (!target_is_road && road_building->occupant_state == 1) {
                            road_building->occupant_state = 0;   /* +0x11C */
                        }
                    }
                }
            }
        }
    }
}


/* ================================================================== */
/* VehicleEditor::MoveAlongTrack                                       */
/* Address: 0x40DC20                                                   */
/* ================================================================== */
uint32_t VehicleEditor::MoveAlongTrack(Vehicle* vehicle)
{
    EditorState* active_end;
    if (vehicle->active_editor == 0) {
        active_end = this->end_a;
    } else {
        active_end = this->end_b;
    }

    int dir = active_end->direction;
    void* track_bldg = active_end->building->resource;
    int track_idx = active_end->track_pos;

    /* Host deviation: this entire function's bound-check and position
     * logic is built on RESDATA+0x636/+0x630 (the track control-point
     * count/table), which have no host source -- world/EditorState.cpp's
     * own header comment already records these offsets as "never routed
     * through a named struct member." Unlike Vehicle::LoadSounds' isolated
     * reads, every line below this point depends on real track_count/
     * track_coords data, so there is no single raw read to swap for a
     * host accessor -- guard the whole function instead of guessing at a
     * partial translation. Returning 0 (the "not handled" value the
     * ProcessMove caller already checks via `if (moved == 0)`) makes the
     * vehicle fall through to the ordinary per-frame UpdatePosition path
     * rather than reading a table that doesn't exist at this address. */
#ifndef _WIN32
    if (loco::assets::is_host_sprite_resource(track_bldg)) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr,
                "[HOST] VehicleEditor::MoveAlongTrack: no host source for "
                "RESDATA+0x636/+0x630 (track control-point table) -- "
                "falling back to ordinary position update\n");
            std::fflush(stderr);
        }
        return 0;
    }
#endif
    int track_count = *field_at<uint16_t>(track_bldg, 0x636);

    /* If at edge, recalibrate both ends */
    if ((dir == 1 && track_idx == track_count - 1) ||
        (dir == 0 && track_idx == 1))
    {
        int16_t* track_coords = *field_at<int16_t*>(track_bldg, 0x630);
        /* end_a/end_b->building is GameVehicle* (world/EditorState.h's
         * class-level doc comment has the full retype evidence trail), and
         * +0x88/+0x8A are ResourceGameObject::sub_pos_x/sub_pos_y — resolves
         * this function's own former TODO ("verify... Building or a track
         * subtype"): it's neither Building::occupation_level (a single
         * byte, can't hold a 16-bit grid coordinate) nor a distinct
         * unnamed subtype, just the same GameVehicle-family node used
         * throughout world/EditorState.cpp. */
        int16_t base_x_a = this->end_a->building->sub_pos_x;
        int16_t base_y_a = this->end_a->building->sub_pos_y;
        int16_t base_x_b = this->end_b->building->sub_pos_x;
        int16_t base_y_b = this->end_b->building->sub_pos_y;

        this->end_a->pos_x = static_cast<int>(track_coords[this->end_a->track_pos * 2]) + base_x_a * 0x10;
        this->end_a->pos_y = static_cast<int>(track_coords[this->end_b->track_pos * 2 + 1]) + base_y_a * 0x10;
        this->end_b->pos_x = static_cast<int>(track_coords[this->end_b->track_pos * 2]) + base_x_b * 0x10;
        this->end_b->pos_y = static_cast<int>(track_coords[this->end_b->track_pos * 2 + 1]) + base_y_b * 0x10;
        return 0;
    }

    /* Check bound check flag using proximity to edges */
    if (dir == 1) {
        if ((track_count - 0x32) < track_idx) {
            this->bound_check_flag = 0;
        } else if (0x50 < track_idx) {
            this->bound_check_flag = 1;
        }
        if (static_cast<uint32_t>(track_idx) >=
            static_cast<uint32_t>(track_count)) {
            return 0;
        }
    } else {
        if (track_idx < 0x32) {
            this->bound_check_flag = 0;
        } else if (track_idx < (track_count - 0x55)) {
            this->bound_check_flag = 1;
        }
        if (track_idx == 0) {
            return 0;
        }
    }

    vehicle->detach_flag = 1;

    /* Advance or retreat track positions */
    if (this->end_a->direction == 1) {
        this->end_a->track_pos += 1;
        this->end_b->track_pos += 1;
    } else {
        this->end_a->track_pos -= 1;
        this->end_b->track_pos -= 1;
    }

    /* Compute pixel offset for track position lookup */
    int offset;
    if (vehicle->active_editor == 0) {
        if (this->end_a->direction == 1) {
            offset = this->end_a->track_pos - 0xb;
        } else {
            offset = this->end_a->track_pos + 0xb;
        }
    } else {
        if (this->end_a->direction == 1) {
            offset = this->end_b->track_pos - 0xb;
        } else {
            offset = this->end_b->track_pos + 0xb;
        }
    }

    int16_t* track_coords = *field_at<int16_t*>(track_bldg, 0x630);
    int16_t base_x_a = this->end_a->building->sub_pos_x;
    int16_t base_y_a = this->end_a->building->sub_pos_y;
    int16_t base_x_b = this->end_b->building->sub_pos_x;
    int16_t base_y_b = this->end_b->building->sub_pos_y;

    this->end_a->pos_x = static_cast<int>(track_coords[offset * 2]) + base_x_a * 0x10;
    this->end_a->pos_y = static_cast<int>(track_coords[offset * 2 + 1]) + base_y_a * 0x10;
    this->end_b->pos_x = static_cast<int>(track_coords[this->end_b->track_pos * 2]) + base_x_b * 0x10;
    this->end_b->pos_y = static_cast<int>(track_coords[this->end_b->track_pos * 2 + 1]) + base_y_b * 0x10;

    TileMap_InvalidateRect(
        &g_tilemap,
        this->screen_rect.left, this->screen_rect.top,
        this->screen_rect.right, this->screen_rect.bottom
    );

    /* Update render offset via vtable[3] */
    void* resource = this->end_a->building->resource;
    int tile_off_x = *reinterpret_cast<int16_t*>(
        static_cast<uint8_t*>(resource) + 0x168 +
        static_cast<uint32_t>(this->angle_frame) * 4);
    int tile_off_y = *reinterpret_cast<int16_t*>(
        static_cast<uint8_t*>(resource) + 0x16a +
        static_cast<uint32_t>(this->angle_frame) * 4);

    int render_x = this->end_a->pos_x - tile_off_x;
    int render_y = this->end_a->pos_y - tile_off_y;

    if (vehicle->active_editor == 0) {
        if (this->end_a->direction == 1) {
            render_x -= 0xb;
        } else {
            render_x += 0xb;
        }
    } else {
        if (this->end_a->direction != 1) {
            render_x -= 0xb;
        } else {
            render_x += 0xb;
        }
    }

    GameObject_HitTest(this, render_x, render_y);
    return 1;
}


/* ================================================================== */
/* VehicleEditor::CheckBridge                                          */
/* Address: 0x40DB90                                                   */
/* ================================================================== */
void VehicleEditor::CheckBridge(Vehicle* vehicle)
{
    if (vehicle->occupancy == 0) {
        return;
    }

    if (vehicle->occupancy == 1) {
        if (vehicle->active_editor == 0 &&
            this->end_b->edit_state == 1) {
            CheckEditBounds1(vehicle);
            return;
        }
        if (vehicle->active_editor == 1 &&
            this->end_a->edit_state == 1) {
            CheckEditBounds1(vehicle);
            return;
        }
    } else {
        if (this->edge_dir_b == 5) {
            CheckEditBounds2(vehicle);
            return;
        }
        if (this->edge_dir_b == 4 &&
            this->end_a->edit_state == 0 &&
            this->end_b->edit_state == 0) {
            this->edge_dir_b = 0;
        }
    }
}


/* ================================================================== */
/* VehicleEditor::CalcAngle                                            */
/* Address: 0x40DF80                                                   */
/*                                                                     */
/* Computes the sprite frame index (0-127) based on the relative       */
/* positions of the two editor endpoints (end_a relative to end_b).    */
/*                                                                     */
/* Uses atan2 to compute the angle of the vector from end_b to end_a   */
/* in screen coordinates, then maps the result to 128 sprite frame     */
/* indices (0-127, wrapping at 128) for a full rotation.               */
/*                                                                     */
/* The original x87 FPU code uses FPATAN (partial arctangent) with     */
/* absolute deltas and manual quadrant adjustment. The calling         */
/* convention passes an unused stack argument from vehicle[+0x08]      */
/* (RET 4 at function exit), but the parameter is not used by          */
/* the function body.                                                  */
/*                                                                     */
/* TODO: Complete decompilation — validate against disassembly.        */
/* ================================================================== */
void VehicleEditor::CalcAngle()
{
    int dx = this->end_a->pos_x - this->end_b->pos_x;
    int dy = this->end_a->pos_y - this->end_b->pos_y;

    if (dx == 0 && dy == 0) {
        this->angle_frame = 0;
        return;
    }

    /* FPATAN-based atan2 approximation: compute angle in [0, 2π),
       then map to 128 sprite frames (128 = full circle). */
    double angle = atan2(static_cast<double>(dy), static_cast<double>(dx));
    if (angle < 0.0) {
        angle += 2.0 * M_PI;
    }

    /* Map to [0, 128) — 128 discrete sprite frames for full rotation */
    int frame = static_cast<int>(angle / (2.0 * M_PI) * 128.0);
    if (frame >= 128) frame = 0;

    this->angle_frame = static_cast<uint16_t>(frame);
}


/* ================================================================ */
/* Stubs — TODO: decompile these from the original binary            */
/* ================================================================ */

/**
 * TriggerSound — Play the sound associated with the current frame.
 * Address: 0x40E130
 * TODO: decompile 0x40E130
 */
void VehicleEditor::TriggerSound()
{
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::TriggerSound 0x40E130");
}

/**
 * BlitBackground — Blit the track sprite background to the primary surface.
 * Address: 0x40E160
 * TODO: decompile 0x40E160
 */
uint32_t VehicleEditor::BlitBackground(int clip_x, int clip_y)
{
    (void)clip_x; (void)clip_y;
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::BlitBackground 0x40E160");
    return 0;
}

/**
 * IsInBounds (vtable[9]) — Test if a 16x16 point intersects the editor rect.
 * Address: 0x40E250
 * TODO: decompile 0x40E250
 */
uint32_t VehicleEditor::IsInBounds(short x, short y, short flag)
{
    (void)x; (void)y; (void)flag;
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::IsInBounds 0x40E250");
    return 0;
}

/**
 * CheckEdgeBounds — Detect when vehicle reaches world edge.
 * Address: 0x40E2A0
 * TODO: decompile 0x40E2A0
 */
uint32_t VehicleEditor::CheckEdgeBounds(Vehicle* vehicle)
{
    (void)vehicle;
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::CheckEdgeBounds 0x40E2A0");
    return 0;
}

/**
 * CheckVehicleAttach — Auto-attach logic for road/vehicle alignment.
 * Address: 0x40E340
 * TODO: decompile 0x40E340
 */
uint32_t VehicleEditor::CheckVehicleAttach(Vehicle* vehicle)
{
    (void)vehicle;
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::CheckVehicleAttach 0x40E340");
    return 0;
}

/**
 * CheckEditBounds1 — First-stage edit bounds check (bridge approach).
 * Address: 0x40E440
 * TODO: decompile 0x40E440
 */
uint32_t VehicleEditor::CheckEditBounds1(Vehicle* vehicle)
{
    (void)vehicle;
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::CheckEditBounds1 0x40E440");
    return 0;
}

/**
 * CheckEditBounds2 — Second-stage edit bounds check (bridge retreat).
 * Address: 0x40E520
 * TODO: decompile 0x40E520
 */
uint32_t VehicleEditor::CheckEditBounds2(Vehicle* vehicle)
{
    (void)vehicle;
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::CheckEditBounds2 0x40E520");
    return 0;
}

/**
 * GetResourceId (non-virtual — see core/VehicleEditor.h's vtable slot
 * correction; vtable[7] is actually inherited Entity::StopSound, not this
 * method) — Returns the resource ID if track resource loaded.
 * Address: 0x40E0D0
 */
uint32_t VehicleEditor::GetResourceId()
{
#ifndef _WIN32
    // Host network editors intentionally omit the original resource object,
    // but preserve its evidenced ID for gameplay state transitions.
    return static_cast<uint32_t>(this->res_id);
#else
    return this->resource != nullptr
        ? static_cast<uint32_t>(this->res_id) : 0xFFFFFFFFu;
#endif
}

/**
 * SetResourceId (vtable[15], +0x3C) — Reload the editor's resource and
 * recompute its route direction. Address: 0x40E0F0.
 *
 * Disassembly (0x40E0F0-0x40E126): writes res_id (+0x428) unconditionally,
 * calls Entity::InitBase(resource_id, anim_index, force_reload=false)
 * (0x405900), and only when that succeeds recomputes res_id_2 (+0x42C) via
 * CGWND_MapResourceToDirection(resource_id) (0x40EB60). Returns InitBase's
 * result unchanged.
 */
int VehicleEditor::SetResourceId(int resource_id, int anim_index)
{
    this->res_id = resource_id;
    int result = this->InitBase(resource_id, anim_index, false);
    if (result != 0) {
        this->res_id_2 = static_cast<int32_t>(CGWND_MapResourceToDirection(resource_id));
    }
    return result;
}

/**
 * GetDPlayData — Get pointer to DPLAY network data if initialized.
 * Address: 0x40D750
 */
DPlayManager* VehicleEditor::GetDPlayData()
{
    if (this->dplay_initialized == 0) return nullptr;
#ifdef _WIN32
    return reinterpret_cast<DPlayManager*>(this->dplay_data);
#else
    return this->host_dplay_data;
#endif
}

/**
 * SetDPlayData — Copy network data into the editor's DPLAY buffer.
 * Address: 0x40D770
 */
int VehicleEditor::SetDPlayData(const DPlayManager* data)
{
    if (this->dplay_initialized != 0 && data != nullptr) return 0;
    if (data == nullptr) {
        this->dplay_initialized = 0;
        return 1;
    }
#ifdef _WIN32
    auto* destination = reinterpret_cast<DPlayManager*>(this->dplay_data);
#else
    auto* destination = this->host_dplay_data;
#endif
    if (destination == nullptr) return 0;
    destination->CopyLogicalStateFrom(*data);
    /* TODO VE-012: binary copies an additional dword at +0x398 after
     * CopyLogicalStateFrom. Decompile 0x40D770 for exact semantics. */
    this->dplay_initialized = 1;
    return 1;
}
