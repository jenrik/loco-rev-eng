/**
 * ResourceGameObject (legacy file name BuildingMgrObjectGroup).
 *
 * loco_v8: constructor 0x4580a0, vtable 0x4777d0, resource type 3.
 * The class derives directly from Entity.  It is not part of BuildingMgr's
 * hierarchy; it owns up to five Building/Train objects managed by BuildingMgr.
 *
 * This is also the real, canonical class for every object TileMap places
 * into the tile grid via TileMap::FindObject -> INPUT_PlaceObject
 * (0x41DD80): every branch of that dispatch (GameVehicle_Ctor 0x412870,
 * RESDATA_GameVehicle_Ctor 0x44AE80, HelpWnd_FindPage/HelpPageNode
 * 0x44F210, and this class's own ctor 0x4580a0 for the "everything else"
 * case -- buildings, track, scenery) either constructs a
 * ResourceGameObject directly or chains through RESDATA_GameVehicle_Ctor,
 * which itself chains through ResourceGameObject's ctor as its base. So
 * sub_pos_x, sub_pos_y, group_active, and the occupancy/build chain fields
 * below (+0x88..+0x108) are the one real home for the tile-placement bookkeeping
 * TileMap::ScrollRect/ScrollTo/GetTileRect/GetTileAt/GetViewport read and
 * write on whatever object a tile-grid cell holds.
 *
 * `Building` (game/Building.h) is a sibling class -- also Entity-derived,
 * constructed via a completely different path (BuildingMgr_CreateFromResource
 * 0x4349D0, not INPUT_PlaceObject) -- and reuses +0x88 for its own,
 * unrelated occupation_level/disabled fields. A `Building*` is never
 * written into the tile grid by TileMap::FindObject, so this does not
 * collide in practice, but it means a tile-grid cell must never be
 * blindly reinterpreted as a `Building*` (or vice versa): the two classes
 * only agree on the inherited Entity/GameObject prefix (+0x00..+0x87).
 *
 * A previous, unsynchronized second model of this exact object
 * (world/tilemap.h's `TileMapObject`) existed as a hand-written x86-offset
 * mirror struct that callers `reinterpret_cast`ed onto whatever real
 * object a tile-grid slot held. On this 64-bit host that struct's own
 * `resource` member (declared as a 4-byte-assumed pointer at +0x40) is
 * actually 8 bytes, silently shifting every field declared after it by 4
 * real bytes relative to the mirror's own offset comments -- a live
 * memory-corruption bug, not just an unreliable read. It has been deleted
 * in favor of this canonical class; see PROGRESS.md's "TileMapObject
 * mirror struct" entry for the full root-cause analysis.
 */
#pragma once
#include "Entity.h"


// Status: TRANSCRIBED
class Building;

class ResourceGameObject : public Entity {
public:
    ResourceGameObject(const ResourceGameObject&) = delete;
    ResourceGameObject& operator=(const ResourceGameObject&) = delete;

    int16_t sub_pos_x;                // +0x88
    int16_t sub_pos_y;                // +0x8a
    uint8_t member_limit;             // +0x8c, random 1..resource[0x522]
    uint8_t created_count;            // +0x8d
    uint8_t group_flag;               // +0x8e
    uint8_t _pad_8f;
    Building* member_objects[5];      // +0x90..+0xa0
    Building* linked_objects[5];      // +0xa4..+0xb4
    int32_t field_b8;                 // +0xb8
    int32_t field_bc;                 // +0xbc

    /* +0xC0: set unconditionally to 1 by both constructor paths (ctor
     * 0x4580a0's `*(this+0xc0)=1`) -- "this slot holds a placed, active
     * object", not literally "is moving". TileMap::ScrollRect (0x4553E0)
     * tests `group_active == 1` (its `*(char*)(iVar1+0xc0)=='\x01'`) as
     * the gate for whether an occupant blocking a placement may be
     * displaced; TileMap::ScrollTo (0x455AB0) requires it non-zero before
     * removing the object from the grid. */
    uint8_t group_active;             // +0xc0
    uint8_t _pad_c1[3];

    /* Per-direction occupancy chain state, filled by TileMap::GetTileRect
     * (0x457830): occupancy_links[dir] = adjacent object in that
     * direction (only set when the neighbour is not a sprite-editor
     * object), occupancy_scores[dir] = accumulated occupancy score.
     * occupancy_more (ctor sets -1) is read off the *neighbour* object by
     * TileMap::GetViewport's chain walk: <0 means "keep walking the
     * occupancy chain", >=0 stops it. Declared as real typed pointers
     * (not the original x86 int32_t the tile grid itself uses) because
     * these fields hold live object pointers copied directly from an
     * already-resolved neighbour, never a serialized/tile-grid handle --
     * host pointers do not survive an int32_t round-trip (see
     * TileMap::StoreTilePointer's doc comment for the general problem;
     * this is the same failure mode applied to a different field). */
    ResourceGameObject* occupancy_links[4];   // +0xc4
    int32_t             occupancy_scores[4];  // +0xd4
    int32_t             occupancy_more;       // +0xe4 = -1

    /* Per-direction buildability chain state, filled by
     * TileMap::GetTileAt (0x457900); same shape as the occupancy_* fields
     * above but gated on TileMap_IsTileBuildable instead of
     * TileMap_IsTileOccupied. */
    ResourceGameObject* build_links[4];       // +0xe8
    int32_t             build_scores[4];      // +0xf8
    int32_t             build_more;           // +0x108 = -1

    /** Address: 0x4580a0. */
    explicit ResourceGameObject(int resource_id);
    /** Body 0x458270; scalar deleting wrapper 0x4125e0. */
    ~ResourceGameObject() override;

    /** Vtable [11] override, thunk 0x4343b0 -> Entity::Draw. */
    void Draw(RECT clip_bounds, int enable_scroll, uint32_t extra_flags) override;
    /** Vtable [15], address 0x458430. */
    virtual Building* CreateMember(uint32_t resource_id);
    /** Vtable [16], address 0x458800. */
    virtual uint32_t RestartAnimation();
    /** Vtable [17], address 0x458810. */
    virtual bool IsMemberActionActive();
    /** Vtable [18], address 0x458940. */
    virtual void UpdateScheduledAnimation();
};

/* Source-compatibility alias; this was never a distinct class in loco_v8. */
using BuildingMgrObjectGroup = ResourceGameObject;

#if UINTPTR_MAX == 0xffffffffu
static_assert(offsetof(ResourceGameObject, member_objects) == 0x90,
              "ResourceGameObject member layout mismatch");
static_assert(offsetof(ResourceGameObject, linked_objects) == 0xa4,
              "ResourceGameObject link layout mismatch");
static_assert(offsetof(ResourceGameObject, group_active) == 0xc0,
              "ResourceGameObject group_active offset mismatch");
static_assert(offsetof(ResourceGameObject, occupancy_links) == 0xc4,
              "ResourceGameObject occupancy_links offset mismatch");
static_assert(offsetof(ResourceGameObject, occupancy_more) == 0xe4,
              "ResourceGameObject occupancy_more offset mismatch");
static_assert(offsetof(ResourceGameObject, build_links) == 0xe8,
              "ResourceGameObject build_links offset mismatch");
static_assert(offsetof(ResourceGameObject, build_more) == 0x108,
              "ResourceGameObject build_more offset mismatch");
static_assert(sizeof(ResourceGameObject) == 0x10c,
              "ResourceGameObject size mismatch");
#endif
