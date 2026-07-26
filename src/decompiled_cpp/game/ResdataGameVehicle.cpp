/**
 * ResdataGameVehicle.cpp — RESDATA_GameVehicle implementation
 * Status: INTEGRATED
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "ResdataGameVehicle.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void  World_DeserializeMap(void* world, int obj);  /* 0x44DAD0 */
extern void  GameObject_StopSound(void*, int);             /* 0x405A20 — Entity::StopSound */
extern char  RESDATA_IsRoadTile(void* resource);           /* 0x44BD10 */
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
/*   4. Zeros counter_timer and reserved                               */
/* ================================================================== */
RESDATA_GameVehicle::RESDATA_GameVehicle(int resource_id)
    : ResourceGameObject(resource_id)
{
    this->type = 4;

    this->vehicle_kind = 0;
    this->init_state   = 3;

    if (this->resource == nullptr) {
        this->reserved      = 0;
        this->counter_timer = 0;
        return;
    }

    /* Read tile type byte at resource + 0x63A */
    uint8_t tile_type = *(uint8_t*)((uint8_t*)this->resource + 0x63A);

    if (tile_type == 0x0C) {
        /* Train tile */
        this->vehicle_kind = 1;

        /* Read animation/state value from resource header at +0x1E */
        int16_t anim_val = *(int16_t*)((uint8_t*)this->resource + 0x1E);

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
        /* Check for road tile */
        char is_road = RESDATA_IsRoadTile(this->resource);
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
            int32_t res_id = (this->resource != nullptr)
                ? *(int32_t*)((uint8_t*)this->resource + 4)
                : -1;
            if (res_id == 0xC64 || res_id == 0xC66 ||
                res_id == 0xC68 || res_id == 0xC6A) {
                this->vehicle_kind = 8;
            }
        }
    }

    this->reserved      = 0;
    this->counter_timer = 0;
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
    World_DeserializeMap(g_world, (int)this);
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
    uint8_t tile_type = *(uint8_t*)((uint8_t*)this->resource + 0x63A);

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
