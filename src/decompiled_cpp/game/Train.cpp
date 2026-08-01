// Status: TRANSCRIBED
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
extern void* g_game_instance;                          /* 0x4854C8 */
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
    : Building(resource_id, true)
{
    /* 0x4533E8: occupation_level is explicitly restored after BaseCtor. */
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
        Game_SelectGameObject(g_game_instance, nullptr);
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

    const uint8_t* saved = static_cast<const uint8_t*>(save_data);
    const int resource_id = *reinterpret_cast<const int32_t*>(saved + 0x64);
    void* storage = operator_new(sizeof(TrainEntity));
    TrainEntity* train = nullptr;

    if (storage != nullptr) {
        train = new (storage) TrainEntity(resource_id);

        /* 0x435DD3..0x436143 copies the serialized body at +0x04..+0xEF.
         * The leading vptr is intentionally excluded and is supplied by the
         * C++ constructor.  Raw byte access is confined to the save buffer. */
        memcpy(reinterpret_cast<uint8_t*>(train) + 4, saved + 4, 0xEC);

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
        const int object_id = static_cast<int>(CRT_rand() % 0x29) + 0x3400;
        GameObject* target = static_cast<GameObject*>(
            INPUT_FindObjectAt(&g_input_mgr, object_id));

        if (target != nullptr &&
            (target->world_x != this->prev_target_x ||
             target->world_y != this->prev_target_y)) {
            this->TeleportTo(target->world_x, target->world_y);
        }
    }
}
