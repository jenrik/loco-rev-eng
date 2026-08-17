// Status: INTEGRATED
/**
 * Train.cpp — Building-derived Train entity implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered from the assembly in loco.exe.
 */

#include "Train.h"
#include <cstring>
#include <new>

extern void* operator_new(size_t size);                 /* 0x465CE0 */
extern void  Game_SelectGameObject(void* game, void*); /* 0x4113A0 */
class InputMgr;
extern void* INPUT_FindObjectAt(InputMgr* mgr, int id);    /* 0x41E1F0 */
extern InputMgr g_input_mgr;                               /* 0x4A9990 — static object */
extern void* g_selected_building;                      /* 0x4855B0 */
extern void* g_game;                                 /* 0x4854C8 — Game singleton (binary: &g_game) */
extern uint32_t g_game_time;                           /* 0x4A99B4 */
extern "C" uint32_t CRT_rand(void);                    /* 0x466150 */

/**
 * TrainEntity::TrainEntity
 * Address: 0x4533D0 (Building::BaseCtor call at 0x4533D8)
 *
 * The binary allocates only 0xF0 bytes and calls the intermediate Building
 * constructor, not Building's full 0xF4-byte constructor.
 */
TrainEntity::TrainEntity(int resource_id)
    : Building(resource_id, true)  /* base_only=true → Building_BaseCtor path (0x433A20) */
{
    /* Binary: calls Building_BaseCtor(this, resource_id), sets TrainEntity
     * vtable (0x4780B8), then occupation_level = 4 at +0x88.
     * The base_only flag routes to the limited init matching BaseCtor. */
    this->occupation_level = 4;
}

/**
 * TrainEntity::~TrainEntity — scalar deleting destructor target 0x4363E0.
 */
TrainEntity::~TrainEntity()
{
    this->BaseDtor();
}

/**
 * TrainEntity::BaseDtor
 * Address: 0x4533F0
 */
void TrainEntity::BaseDtor()
{
    if (g_selected_building == this) {
        Game_SelectGameObject(g_game, nullptr);
    }

    /* Train bypasses Building::BaseDtor: it has no +0xF0 occupant field. */
    Building::BaseCleanup();
}

/**
 * TrainEntity::Deserialize — prototype factory virtual.
 * Address: 0x435DB0
 */
void TrainEntity_DeserializeFactory(GameObject* prototype,
                                    void* context, void* save_data)
{
    if (save_data == nullptr) {
        prototype->RegisterEntity(context, nullptr);
        return;
    }

    /* ABI_BOUNDARY: raw x86 object-image save format. 0x435DD3..0x436143
     * copies the serialized body at +0x04..+0xEF from a flat in-memory
     * snapshot of a live x86 TrainEntity (leading vptr excluded; supplied
     * by the C++ constructor below). A blanket memcpy at these x86 byte
     * offsets is only correct if every field is scalar and the same width
     * on both sides — pointer-valued fields are neither: an x86 4-byte
     * address from the original process is never a valid pointer after
     * reload (true of the original binary too, not just this port), and
     * this 64-bit host's pointer members are 8 bytes wide besides. So each
     * field is read individually at its already-canonical x86 offset
     * (core/GameObject.h/core/Entity.h/game/Building.h) and pointer
     * members are zeroed instead of reinterpreted from the saved bytes. */
    const uint8_t* saved = static_cast<const uint8_t*>(save_data);
    const int resource_id = *reinterpret_cast<const int32_t*>(saved + 0x64);
    void* storage = operator_new(sizeof(TrainEntity));
    TrainEntity* train = nullptr;

    if (storage != nullptr) {
        train = new (storage) TrainEntity(resource_id);

        auto read_i32 = [saved](size_t off) {
            int32_t v; memcpy(&v, saved + off, sizeof(v)); return v;
        };
        auto read_u32 = [saved](size_t off) {
            uint32_t v; memcpy(&v, saved + off, sizeof(v)); return v;
        };
        auto read_u8 = [saved](size_t off) { return saved[off]; };

        /* GameObject (+0x04..+0x23, plus the relocated +0x4C/+0x50 pair) */
        train->type = read_i32(0x04);
        memcpy(&train->screen_rect, saved + 0x08, sizeof(RECT));
        train->initialized = read_u8(0x18);
        train->callback_1 = nullptr;  /* +0x1C: x86 code pointer */
        train->callback_2 = nullptr;  /* +0x20: x86 code pointer */
        train->world_x = read_i32(0x4C);
        train->world_y = read_i32(0x50);

        /* Entity (+0x24..+0x87) */
        train->visible = read_u8(0x24);
        train->anim_index = read_i32(0x28);
        train->blit_flags = read_u32(0x2C);
        memcpy(&train->source_rect, saved + 0x30, sizeof(RECT));
        train->resource = nullptr;      /* +0x40: re-acquired via InitBase */
        train->sound_res_id = read_u32(0x44);
        train->audio_channel = nullptr; /* +0x48 */
        train->frame_index = read_i32(0x54);
        train->timer = read_u32(0x58);
        train->active_state = read_u32(0x5C);
        train->next_sound_time = read_u32(0x60);
        train->stored_resource_id = read_u32(0x64);
        train->action_cooldown_time = read_u32(0x68);
        train->phase_timer = read_u32(0x6C);
        train->waiting_flag = read_u8(0x70);
        train->world_x_raw = read_i32(0x74);
        train->world_y_raw = read_i32(0x78);
        memcpy(train->name, saved + 0x7C, sizeof(train->name));

        /* Building base slice used by Train (+0x88..+0xEF) */
        train->occupation_level = read_u8(0x88);
        train->disabled = read_u8(0x89);
        train->occupant_a = nullptr; /* +0x8C */
        train->occupant_b = nullptr; /* +0x90 */
        train->create_time = read_u32(0x94);
        train->conn_building_a = read_i32(0x98);
        train->conn_building_b = read_i32(0x9C);
        train->next_action_time = read_u32(0xA0);
        train->field_a4 = read_u32(0xA4);
        train->target_x = read_i32(0xA8);
        train->target_y = read_i32(0xAC);
        train->search_x1 = read_i32(0xB0);
        train->search_y1 = read_i32(0xB4);
        train->track_x = read_i32(0xB8);
        train->track_y = read_i32(0xBC);
        train->track_node_id = read_i32(0xC0);
        train->prev_target_x = read_i32(0xC4);
        train->prev_target_y = read_i32(0xC8);
        train->dest_x = read_i32(0xCC);
        train->dest_y = read_i32(0xD0);
        train->waypoint_x1 = read_i32(0xD4);
        train->waypoint_y1 = read_i32(0xD8);
        train->field_dc = read_i32(0xDC);
        train->field_e0 = read_u32(0xE0);
        train->field_e4 = read_u8(0xE4);
        train->last_action = read_i32(0xE8);
        train->field_ec = read_u32(0xEC);

        /* The natural C++ layout still contains Building's tail field even
         * though the 0xF0-byte binary Train does not.  Keep it harmless for
         * the compiler-generated base-destructor chain. */
        train->occupant_ptr = nullptr;
    }

    prototype->RegisterEntity(context, train);
}

/**
 * TrainEntity::Update — vtable slot 15.
 * Address: 0x453450
 */
void TrainEntity::Update(void* next_entity)
{
    (void)next_entity; /* Uniform vtable argument; 0x453450 does not read it. */

    if (this->disabled != 0) {
        return;
    }

    this->CheckTimeout();

    if (this->field_dc != 0) {
        if (this->IsActionComplete() == 0) {
            if (this->dest_x != -1 || this->dest_y != -1) {
                if (this->world_x == this->target_x &&
                    this->world_y == this->target_y) {
                    this->HandleAction(this->last_action);
                } else {
                    this->StepToward(this->world_x, this->world_y);
                }
            }
        }

        if (this->field_dc != 0) {
            return;
        }
    }

    if (this->next_action_time < g_game_time &&
        g_selected_building != this) {
        /* Binary: uVar3 = CRT_rand(); iVar2 = (int)uVar3 % 0x29 + 0x3400.
         * The cast-to-int BEFORE modulo matches the raw signed division. */
        const int object_id = (static_cast<int>(CRT_rand()) % 0x29) + 0x3400;
        GameObject* target = static_cast<GameObject*>(
            INPUT_FindObjectAt(&g_input_mgr, object_id));

        if (target != nullptr &&
            (target->world_x != this->prev_target_x ||
             target->world_y != this->prev_target_y)) {
            this->TeleportTo(target->world_x, target->world_y);
        }
    }
}
