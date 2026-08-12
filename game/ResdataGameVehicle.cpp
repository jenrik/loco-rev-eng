/**
 * ResdataGameVehicle.cpp — RESDATA_GameVehicle implementation
 * Status: INTEGRATED
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "ResdataGameVehicle.h"

#ifndef _WIN32
#include "../resources/resource_manager_sdl3.h"
#endif

namespace {

struct ResourceVehicleTileMetadata {
    uint8_t prefix_00_639[0x63A];
    uint8_t tile_type;
};

/* Host deviation: `resource` may be a loco::assets::SpriteResource*
 * (undersized-object landmine, see PROGRESS.md) rather than a real
 * RESDATA/TileMapResource. 0 matches none of the tile_type branches
 * below, so it falls through to the RESDATA_IsRoadTile/etc. checks --
 * already guarded the same way -- exactly like a real resource whose
 * +0x63A byte happens to be 0. */
uint8_t ResolveVehicleTileType(const void* resource)
{
#ifndef _WIN32
    uint8_t byte = 0;
    if (loco::assets::sprite_tile_type_byte(resource, &byte)) {
        return byte;
    }
    if (loco::assets::is_host_sprite_resource(resource)) {
        return 0;
    }
#endif
    return reinterpret_cast<const ResourceVehicleTileMetadata*>(resource)->tile_type;
}

} // namespace

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void  World_DeserializeMap(void* world, int obj);  /* 0x44DAD0 */
extern void  GameObject_StopSound(void*, int);             /* 0x405A20 — Entity::StopSound */
/* RESDATA_IsRoadTile takes int32_t, not void* — the real implementation
 * (world/tilemap.h) treats it as a __fastcall(int32_t); a void* param
 * declaration here mangled to a symbol nothing defines (genuine call-0,
 * not merely a wrong-stub binding). */
extern uint8_t RESDATA_IsRoadTile(int32_t resource);       /* 0x44BD10 */
extern void* g_world;                                      /* 0x4A98B0 */

/* ================================================================== */
/* RESDATA_GameVehicle::RESDATA_GameVehicle — Constructor              */
/* Address: 0x44AE80                                                   */
/*                                                                      */
/* Chains to ResourceGameObject(resource_id), then:                    */
/*   1. Sets type = 4, vtable to 0x478308                              */
/*   2. Reads tile type byte at resource+0x63A                         */
/*   3. Determines vehicle_kind and init_state:                        */
/*      0x0C → train (kind=1), init_state from resource[0x1E]         */
/*      0x0B → pedestrian (kind=2), init_state=5                       */
/*      Road tile → road vehicle (kind=3)                              */
/*      0x0D → crossing signal (kind=6)                                */
/*      0x05/0x06 → fuel pump (kind=5)                                 */
/*      0x0E-0x11 → signal (kind=7), init_state=4, StopSound(1)       */
/*      Resource IDs 0xC64-0xC6A → special (kind=8)                   */
/*   4. Zeros counter_timer and boarding_vehicle                       */
/* ================================================================== */
RESDATA_GameVehicle::RESDATA_GameVehicle(int resource_id)
    : ResourceGameObject(resource_id)
{
    this->type = 4;

    this->vehicle_kind = 0;
    this->init_state   = 3;

    if (this->resource == nullptr) {
        this->boarding_vehicle = nullptr;
        this->counter_timer    = 0;
        return;
    }

    /* Read tile type byte at resource + 0x63A */
    uint8_t tile_type = ResolveVehicleTileType(this->resource);

    if (tile_type == 0x0C) {
        /* Train tile */
        this->vehicle_kind = 1;

        /* Read animation/state value from resource header at +0x1E */
        int16_t anim_val = 0;
#ifndef _WIN32
        if (loco::assets::is_host_sprite_resource(this->resource)) {
            const loco::assets::SpriteMetadata* metadata =
                ResourceManager_GetSpriteMetadata(this->resource);
            anim_val = metadata ? static_cast<int16_t>(metadata->cursor_frame) : 0;
        } else
#endif
        {
            const RESDATA* resource_data = static_cast<const RESDATA*>(this->resource);
            anim_val = resource_data->default_anim;
        }

        /* NOTE: The binary re-checks tile_type for 0x0B (pedestrian)
         * inside the train branch at 0x44AEEE — dead code since we
         * already confirmed tile_type == 0x0C. We preserve the logic
         * exactly as the binary has it for fidelity. */
        if (tile_type == 0x0B) {
            /* DEAD CODE — tile_type != 0x0B here */
            if (anim_val == 0) {
                this->init_state = 5;
                GameObject_StopSound(this, 0);
            } else {
                if (anim_val == 1) {
                    this->init_state = 4;
                }
                GameObject_StopSound(this, anim_val);
            }
        } else {
            this->init_state = anim_val;
            GameObject_StopSound(this, anim_val);
        }
    } else if (tile_type == 0x0B) {
        /* Pedestrian tile */
        this->vehicle_kind = 2;
        this->init_state   = 5;
    } else {
        /* Check for road tile. RESDATA_IsRoadTile takes int32_t (the
         * original x86 ABI's pointer width) -- same pointer-truncation
         * defect as InputMgr.cpp's dispatch (see its comment on
         * INPUT_PlaceObject). tile_type here is already the resolved
         * +0x63A byte, so check its value set directly instead of
         * re-deriving it through a truncated pointer. */
        bool is_road;
#ifndef _WIN32
        is_road = (tile_type == 0x01 || tile_type == 0x02 ||
                    tile_type == 0x03 || tile_type == 0x04);
#else
        is_road = RESDATA_IsRoadTile(static_cast<int32_t>(
            reinterpret_cast<intptr_t>(this->resource))) != 0;
#endif
        if (is_road) {
            this->vehicle_kind = 3;
        } else if (tile_type == 0x0D) {
            /* Crossing signal */
            this->vehicle_kind = 6;
        } else if (tile_type == 0x05 || tile_type == 0x06) {
            /* Fuel pump */
            this->vehicle_kind = 5;
        } else if (tile_type >= 0x0E && tile_type <= 0x11) {
            /* Signal types */
            this->init_state   = 4;
            this->vehicle_kind = 7;
            this->init_state   = 4;  /* binary writes twice */
            GameObject_StopSound(this, 1);
        } else {
            /* Check resource ID for special vehicles */
            int32_t res_id = -1;
#ifndef _WIN32
            if (loco::assets::is_host_sprite_resource(this->resource)) {
                res_id = static_cast<int32_t>(
                    loco::assets::sprite_resource_id(
                        static_cast<loco::assets::SpriteResource*>(this->resource)));
            } else
#endif
            {
                const RESDATA* resource_data = static_cast<const RESDATA*>(this->resource);
                res_id = (resource_data != nullptr) ? resource_data->resource_id : -1;
            }
            if (res_id == 0xC64 || res_id == 0xC66 ||
                res_id == 0xC68 || res_id == 0xC6A) {
                this->vehicle_kind = 8;
            }
        }
    }

    this->boarding_vehicle = nullptr;
    this->counter_timer    = 0;
}


/* ================================================================== */
/* RESDATA_GameVehicle::~RESDATA_GameVehicle — Destructor              */
/* Address: 0x44B050 (base destructor body)                            */
/*                                                                      */
/* Calls World_DeserializeMap to remove from world grid, then chains   */
/* to ~ResourceGameObject() which handles BuildingMgr_DestroyObjectGroup. */
/* ================================================================== */
RESDATA_GameVehicle::~RESDATA_GameVehicle()
{
    World_DeserializeMap(g_world,
                         static_cast<int>(reinterpret_cast<intptr_t>(this)));
    /* ~ResourceGameObject() runs automatically after this body */
}


/* ================================================================== */
/* RESDATA_GameVehicle::StopSound — State machine transition           */
/* Address: 0x44B130 (vtable[7])                                       */
/*                                                                      */
/* Dispatches based on tile type at resource+0x63A and vehicle_kind:   */
/*   - Pedestrian tile (0x0B) or signal vehicle (kind=7):              */
/*       state==0 → init_state=5, Entity::StopSound(0), return         */
/*       state==1 → init_state=4, fall through                         */
/*   - Train (kind=1): init_state=state, Entity::StopSound(state), ret */
/*   - Default: Entity::StopSound(state)                               */
/*                                                                      */
/* Verified against disassembly at 0x44B130:                            */
/*   MOV EAX,[ECX+0x40]; MOV DL,[EAX+0x63A]; CMP DL,0x0B;              */
/*   JE pedestrian_path; MOV EAX,[ECX+0x10C]; CMP EAX,7; JE ped;       */
/*   CMP EAX,1; JNE default; push state; MOV [ECX+0x110],state;        */
/*   CALL Entity::StopSound; RET 4.                                     */
/* ================================================================== */
void RESDATA_GameVehicle::StopSound(int state)
{
    uint8_t tile_type = ResolveVehicleTileType(this->resource);

    if (tile_type == 0x0B || this->vehicle_kind == 7) {
        /* Pedestrian tile or signal vehicle */
        if (state == 0) {
            this->init_state = 5;
            Entity::StopSound(0);
            return;
        }
        if (state == 1) {
            this->init_state = 4;
            /* Fall through to default StopSound */
        }
    } else if (this->vehicle_kind == 1) {
        /* Train */
        this->init_state = state;
        Entity::StopSound(state);
        return;
    }

    Entity::StopSound(state);
}
