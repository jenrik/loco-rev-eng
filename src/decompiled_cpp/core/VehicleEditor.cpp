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
#include "../game/Vehicle.h"
#include "../network/DPlayManager.h"
#include <cmath>
#include <new>

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

/* Resource classification (same externs used by EditorState.cpp) */
extern uint8_t  Resource_IsValidTrackIndex(void* resource, int16_t idx);               /* 0x44BCD0 */
extern uint8_t  Resource_IsRoadTile(void* resource);                                    /* 0x44BD10 */
extern uint8_t  Resource_IsBuildingTile(void* resource);                                /* 0x44BD30 */

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
extern void    UIPANEL_Blit(void* panel, int dst_l, int dst_t, int dst_r, int dst_b,
                            void* surface, int src_l, int src_t, int src_r, int src_b,
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
        if (((Building*)this->target_building)->occupation_level == 0) {
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

    int tile_off_x = *(int16_t*)((uint8_t*)resource + 0x168 +
                                 (uint32_t)this->angle_frame * 4);
    int tile_off_y = *(int16_t*)((uint8_t*)resource + 0x16a +
                                 (uint32_t)this->angle_frame * 4);

    int left = this->end_a->pos_x - tile_off_x;
    int top = this->end_a->pos_y - tile_off_y;

    this->screen_rect.left = left;
    this->screen_rect.top = top;
    this->screen_rect.right = *(uint16_t*)((uint8_t*)resource + 0x14) + left;
    this->screen_rect.bottom = *(uint16_t*)((uint8_t*)resource + 0x16) + top;

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
    uintptr_t road_building = 0;

    if (vehicle->active_editor == 0) {
        /* Active index 0: use end B from vehicle's child entry */
        uint32_t child_idx = vehicle->editor_count;
        Vehicle* child_veh = (Vehicle*)vehicle->editors[child_idx];
        VehicleEditor* child_ed = (VehicleEditor*)child_veh->editors[0]; /* +0x434 = end_b */
        road_building = (uintptr_t)child_ed->end_b->building;
    } else {
        Vehicle* child_veh = (Vehicle*)vehicle->editors[0];
        VehicleEditor* child_ed = (VehicleEditor*)child_veh->editors[0]; /* +0x430 = end_a */
        road_building = (uintptr_t)child_ed->end_a->building;
    }

    char moved = 0;

    /* Get buildings for both editor ends */
    uintptr_t bldg_a = (uintptr_t)this->end_a->building;
    uintptr_t bldg_b = (uintptr_t)this->end_b->building;

    if (bldg_a != 0 && bldg_b != 0) {
        int substate_a = *(int32_t*)(bldg_a + 0x10c);
        int substate_b = *(int32_t*)(bldg_b + 0x10c);

        if (substate_a == 5 && substate_b == 5 && bldg_a == bldg_b) {
            /* Both ends on the same road building */
            vehicle->detach_flag = 1;

            void* resource = *(void**)(bldg_a + 0x40);
            char res_type = *(char*)((uint8_t*)resource + 0x63a);

            if (res_type == 5) {
                /* Road type — move along track */
                moved = (char)this->MoveAlongTrack(vehicle);
            } else {
                /* Non-road bridge conditions */
                int a_limit = *(int32_t*)(bldg_a + 0x14) - 0x20;
                int b_limit = *(int32_t*)(bldg_b + 0x14) - 0x20;

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
            Vehicle_DetachAll((int)(intptr_t)vehicle);
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
    if (road_building != 0) {
        void* road_res = *(void**)(road_building + 0x40);
        if (road_res != nullptr) {
            uint8_t is_road = Resource_IsRoadTile(road_res);
            if (is_road == 1) {
                uintptr_t target_bldg = 0;
                if (vehicle->active_editor == 0) {
                    uint32_t child_idx = vehicle->editor_count;
                    Vehicle* child_veh = (Vehicle*)vehicle->editors[child_idx];
                    VehicleEditor* child_ed = (VehicleEditor*)child_veh->editors[0];
                    target_bldg = (uintptr_t)child_ed->end_b->building;
                } else {
                    Vehicle* child_veh = (Vehicle*)vehicle->editors[0];
                    VehicleEditor* child_ed = (VehicleEditor*)child_veh->editors[0];
                    target_bldg = (uintptr_t)child_ed->end_a->building;
                }

                if (target_bldg != 0) {
                    void* target_res = *(void**)(target_bldg + 0x40);
                    if (target_res != nullptr) {
                        uint8_t target_is_road = Resource_IsRoadTile(target_res);
                        if (target_is_road == 0 &&
                            *(int32_t*)(road_building + 0x11c) == 1)
                        {
                            *(int32_t*)(road_building + 0x11c) = 0;
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
    void* track_bldg = *(void**)((uint8_t*)active_end->building + 0x40);
    int track_idx = active_end->track_pos;
    int track_count = *(uint16_t*)((uint8_t*)track_bldg + 0x636);

    /* If at edge, recalibrate both ends */
    if ((dir == 1 && track_idx == track_count - 1) ||
        (dir == 0 && track_idx == 1))
    {
        int16_t* track_coords = *(int16_t**)((uint8_t*)track_bldg + 0x630);
        /* NOTE: The binary reads 16-bit grid coordinates from Building+0x88
           and Building+0x8A. Building.h names these 'occupation_level' (+0x88)
           and '_pad_8a[0]' (+0x8A), but in VehicleEditor context they are
           16-bit grid positions. TODO: verify with Ghidra whether the owning
           object type at these offsets is truly a Building or a track subtype. */
        int16_t base_x_a = (int16_t)((Building*)this->end_a->building)->occupation_level;
        int16_t base_y_a = (int16_t)((Building*)this->end_a->building)->_pad_8a[0];
        int16_t base_x_b = (int16_t)((Building*)this->end_b->building)->occupation_level;
        int16_t base_y_b = (int16_t)((Building*)this->end_b->building)->_pad_8a[0];

        this->end_a->pos_x = (int)track_coords[this->end_a->track_pos * 2] + base_x_a * 0x10;
        this->end_a->pos_y = (int)track_coords[this->end_b->track_pos * 2 + 1] + base_y_a * 0x10;
        this->end_b->pos_x = (int)track_coords[this->end_b->track_pos * 2] + base_x_b * 0x10;
        this->end_b->pos_y = (int)track_coords[this->end_b->track_pos * 2 + 1] + base_y_b * 0x10;
        return 0;
    }

    /* Check bound check flag using proximity to edges */
    if (dir == 1) {
        if ((track_count - 0x32) < track_idx) {
            this->bound_check_flag = 0;
        } else if (0x50 < track_idx) {
            this->bound_check_flag = 1;
        }
        if ((uint32_t)track_idx >= (uint32_t)track_count) {
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

    int16_t* track_coords = *(int16_t**)((uint8_t*)track_bldg + 0x630);
    int16_t base_x_a = (int16_t)((Building*)this->end_a->building)->occupation_level;
    int16_t base_y_a = (int16_t)((Building*)this->end_a->building)->_pad_8a[0];
    int16_t base_x_b = (int16_t)((Building*)this->end_b->building)->occupation_level;
    int16_t base_y_b = (int16_t)((Building*)this->end_b->building)->_pad_8a[0];

    this->end_a->pos_x = (int)track_coords[offset * 2] + base_x_a * 0x10;
    this->end_a->pos_y = (int)track_coords[offset * 2 + 1] + base_y_a * 0x10;
    this->end_b->pos_x = (int)track_coords[this->end_b->track_pos * 2] + base_x_b * 0x10;
    this->end_b->pos_y = (int)track_coords[this->end_b->track_pos * 2 + 1] + base_y_b * 0x10;

    TileMap_InvalidateRect(
        &g_tilemap,
        this->screen_rect.left, this->screen_rect.top,
        this->screen_rect.right, this->screen_rect.bottom
    );

    /* Update render offset via vtable[3] */
    void* resource = ((Building*)this->end_a->building)->resource;
    int tile_off_x = *(int16_t*)((uint8_t*)resource + 0x168 + (uint32_t)this->angle_frame * 4);
    int tile_off_y = *(int16_t*)((uint8_t*)resource + 0x16a + (uint32_t)this->angle_frame * 4);

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
    double angle = atan2((double)dy, (double)dx);
    if (angle < 0.0) {
        angle += 2.0 * M_PI;
    }

    /* Map to [0, 128) — 128 discrete sprite frames for full rotation */
    int frame = (int)(angle / (2.0 * M_PI) * 128.0);
    if (frame >= 128) frame = 0;

    this->angle_frame = (uint16_t)frame;
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
    /* TODO: decompile 0x40E130 */
}

/**
 * BlitBackground — Blit the track sprite background to the primary surface.
 * Address: 0x40E160
 * TODO: decompile 0x40E160
 */
uint32_t VehicleEditor::BlitBackground(int clip_x, int clip_y)
{
    (void)clip_x; (void)clip_y;
    /* TODO: decompile 0x40E160 */
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
    /* TODO: decompile 0x40E250 */
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
    /* TODO: decompile 0x40E2A0 */
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
    /* TODO: decompile 0x40E340 */
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
    /* TODO: decompile 0x40E440 */
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
    /* TODO: decompile 0x40E520 */
    return 0;
}

/**
 * GetResourceId (vtable[7]) — Returns the resource ID if track resource loaded.
 * Address: 0x40E0D0
 * TODO: decompile 0x40E0D0
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
    this->dplay_initialized = 1;
    return 1;
}
