
// Status: TRANSCRIBED
#include "BuildingMgrObjectGroup.h"
#include "../game/BuildingMgr.h"
#include "../game/Building.h"
#include <ctime>

#ifndef _WIN32
#include "../resources/resource_manager_sdl3.h"
#endif

extern int CRT_rand(void);                                      // 0x466150
extern uint32_t g_game_time;                                    // 0x4a99b4
extern BuildingMgr* g_building_mgr;                             // 0x485448 — host-constructed singleton
class ResourceManager;
extern ResourceManager g_resmgr;    // 0x4855e8 — object, not a pointer (was void*,
                                     // a widespread cross-TU landmine — see
                                     // PROGRESS.md's g_resmgr sweep)
extern void* ResourceManager_GetById(void*, int);                // 0x446ea0
extern unsigned int GetResourceType(unsigned int);                // 0x446030
extern tm* CRT_localtime(uint32_t*);                             // 0x4674e0
extern int Game_CheckTimeInRange(int*, int*, int*);              // 0x412710
extern BOOL Building_CheckPlacement(Building*, int, int);        // 0x433860, vtable [21]
extern BOOL (__stdcall* g_IsRectEmpty)(const RECT*);
extern BOOL (__stdcall* g_OffsetRect)(RECT*, int, int);

/** Resource type-3 constructor. Address: 0x4580a0. */
ResourceGameObject::ResourceGameObject(int resource_id)
    : Entity(resource_id, -1, 0, 0)
{
    type = 3;
    sub_pos_x = sub_pos_y = 0;
    if (resource == nullptr) return;

#ifndef _WIN32
    /* Host deviation: `resource` may be a loco::assets::SpriteResource*
     * (undersized-object landmine, see PROGRESS.md), not a real RESDATA.
     * ee_replay_delay/name/frame_sets are the already-verified host
     * sources for +0x522/+0x14D/+0x20 (see the childwindow-descriptor-
     * fields and cursor-default-frame-set-fix session-log entries). */
    if (loco::assets::is_host_sprite_resource(resource)) {
        const loco::assets::SpriteMetadata* metadata =
            ResourceManager_GetSpriteMetadata(resource);
        const uint8_t maximum = metadata ? metadata->ee_replay_delay : 0;
        member_limit = maximum == 0
            ? 0
            : static_cast<uint8_t>(CRT_rand() % maximum + 1);
        created_count = 0;
        group_flag = 0;
        for (int i = 0; i < 5; ++i) {
            member_objects[i] = nullptr;
            linked_objects[i] = nullptr;
        }
        field_b8 = field_bc = 0;
        group_active = 1;

        if (metadata != nullptr && !metadata->name.empty()) {
            SetName(metadata->name.c_str());
        }

        /* anim_index can be -1 here (SetAnimState's host branch leaves it
         * unresolved when default_anim is out of range) -- unlike the
         * original's frames[-1] OOB heap read (preserved elsewhere for
         * fidelity), std::vector::operator[](-1) is real memory
         * corruption on host, so this documented deviation just skips
         * the timer set instead of reading out of bounds. */
        if (metadata != nullptr && anim_index >= 0 &&
            static_cast<size_t>(anim_index) < metadata->frame_sets.size() &&
            metadata->frame_sets[static_cast<size_t>(anim_index)].restart_delay >= 0) {
            timer = static_cast<uint32_t>(CRT_rand() % 0x3d) + g_game_time;
        }

        for (int i = 0; i < 4; ++i) {
            occupancy_links[i] = nullptr;
            occupancy_scores[i] = 0;
            build_links[i] = nullptr;
            build_scores[i] = 0;
        }
        occupancy_more = -1;
        build_more = -1;
        return;
    }
#endif

    uint8_t maximum = *reinterpret_cast<uint8_t*>(
        static_cast<uint8_t*>(resource) + 0x522);
    member_limit = maximum == 0
        ? 0
        : static_cast<uint8_t>(CRT_rand() % maximum + 1);
    created_count = 0;
    group_flag = 0;
    for (int i = 0; i < 5; ++i) {
        member_objects[i] = nullptr;
        linked_objects[i] = nullptr;
    }
    field_b8 = field_bc = 0;
    group_active = 1;

    const char* resource_name = reinterpret_cast<const char*>(
        static_cast<uint8_t*>(resource) + 0x14d);
    if (*resource_name != '\0') SetName(resource_name);

    FrameData* frames = *reinterpret_cast<FrameData**>(
        static_cast<uint8_t*>(resource) + 0x20);
    if (frames[anim_index].wait_time >= 0)
        timer = static_cast<uint32_t>(CRT_rand() % 0x3d) + g_game_time;

    for (int i = 0; i < 4; ++i) {
        occupancy_links[i] = nullptr;
        occupancy_scores[i] = 0;
        build_links[i] = nullptr;
        build_scores[i] = 0;
    }
    occupancy_more = -1;
    build_more = -1;
}

/** Destructor body. Address: 0x458270. */
ResourceGameObject::~ResourceGameObject()
{
    for (int i = 0; i < 5; ++i) {
        if (member_objects[i] != nullptr) {
            /* Disassembly pushes show_message=1 at 0x4582b3. */
            g_building_mgr->RemoveObject(member_objects[i], true);
            member_objects[i] = nullptr;
        }
    }
    for (int i = 0; i < 5; ++i) {
        if (linked_objects[i] != nullptr) {
            linked_objects[i]->occupant_b = nullptr;     // linked +0x90
            linked_objects[i]->next_action_time = 0;     // linked +0xa0
        }
    }
    /* Entity::~Entity runs automatically; the explicit ~GameObject call in
     * the old source caused a second/desynchronized destruction path. */
}

/** Draw override thunk. Address: 0x4343b0. */
void ResourceGameObject::Draw(RECT clip, int enable_scroll, uint32_t flags)
{
    Entity::Draw(clip, enable_scroll, flags);
}

/** Create and attach one managed member. Address: 0x458430. */
Building* ResourceGameObject::CreateMember(uint32_t resource_id)
{
    if (initialized != 1 || member_limit == 0 ||
        static_cast<uint32_t>(g_building_mgr->building_count +
                              g_building_mgr->secondary_count) >= 100) return nullptr;

    int slot = 0;
    while (slot < member_limit && member_objects[slot] != nullptr) ++slot;
    if (slot >= member_limit || slot >= 5) return nullptr;

    if (resource_id < 1) {
        uint16_t* choices = reinterpret_cast<uint16_t*>(
            static_cast<uint8_t*>(resource) + 0x524);
        resource_id = choices[CRT_rand() % 5];
    }
    void* member_resource = ResourceManager_GetById(&g_resmgr, resource_id);
    if (member_resource == nullptr) return nullptr;

    uint8_t kind = *reinterpret_cast<uint8_t*>(
        static_cast<uint8_t*>(member_resource) + 8);
    uint16_t references = *reinterpret_cast<uint16_t*>(
        static_cast<uint8_t*>(member_resource) + 0x158);
    uint8_t maximum = *reinterpret_cast<uint8_t*>(
        static_cast<uint8_t*>(member_resource) + 0x16b);
    bool building_kind = kind == 7;
    bool train_kind = GetResourceType(resource_id) == 8;
    if ((!building_kind && !train_kind) || references >= maximum) return nullptr;

    int x = world_x;
    int y = world_y;
    RECT bounds = *reinterpret_cast<RECT*>(
        static_cast<uint8_t*>(resource) + 0x61c);
    if (!g_IsRectEmpty(&bounds)) {
        g_OffsetRect(&bounds, screen_rect.left, screen_rect.top);
        int width = *reinterpret_cast<uint16_t*>(
            static_cast<uint8_t*>(member_resource) + 0x14);
        int height = *reinterpret_cast<uint16_t*>(
            static_cast<uint8_t*>(member_resource) + 0x16);
        int x_range = bounds.right - bounds.left - width + 1;
        int y_range = bounds.bottom - bounds.top - height + 1;
        x = x_range > 0 ? bounds.left + CRT_rand() % x_range : bounds.right - width;
        y = y_range > 0 ? bounds.top + CRT_rand() % y_range : bounds.top;
    } else if (building_kind) {
        x -= *reinterpret_cast<int16_t*>(
            static_cast<uint8_t*>(member_resource) + 0x32);
        y -= *reinterpret_cast<int16_t*>(
            static_cast<uint8_t*>(member_resource) + 0x34);
    }

    Building* member = g_building_mgr->CreateFromResource(
        static_cast<int>(resource_id), slot, x, y);
    member_objects[slot] = member;
    if (member != nullptr) {
        if (!Building_CheckPlacement(member, world_x, world_y)) {
            g_building_mgr->RemoveObject(member, false);
            member_objects[slot] = nullptr;
            return nullptr;
        }
        if (building_kind) member->PostMoveDispatch();   // vtable [20]
        ++created_count;
    }
    return member;
}

/** Vtable [16]. Address: 0x458800. */
uint32_t ResourceGameObject::RestartAnimation()
{
    SetAnimState(anim_index);
    return 0; /* assembly explicitly clears AL */
}

/** Vtable [17]. Address: 0x458810. */
bool ResourceGameObject::IsMemberActionActive()
{
    return false;
}

/** Scheduled animation switch. Address: 0x458940. */
void ResourceGameObject::UpdateScheduledAnimation()
{
    if (initialized != 1) return;
    int16_t scheduled_anim = *reinterpret_cast<int16_t*>(
        static_cast<uint8_t*>(resource) + 0x530);
    if (scheduled_anim == -1) return;
    tm* now = CRT_localtime(&g_game_time);
    bool in_range = Game_CheckTimeInRange(
        reinterpret_cast<int*>(now),
        reinterpret_cast<int*>(static_cast<uint8_t*>(resource) + 0x534),
        reinterpret_cast<int*>(static_cast<uint8_t*>(resource) + 0x548)) != 0;
    if (!in_range && anim_index != scheduled_anim)
        SetAnimState(static_cast<int>(scheduled_anim));
    else if (in_range && anim_index == scheduled_anim)
        SetAnimState(static_cast<int>(*reinterpret_cast<int16_t*>(
            static_cast<uint8_t*>(resource) + 0x1e)));
}
