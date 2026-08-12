// Status: VALIDATED
/**
 * input_place_object_test.cpp — reachability proof for INPUT_PlaceObject's
 * host-safe dispatcher (input/InputMgr.cpp, 0x41DD80).
 *
 * Links the real InputMgr.o AND the real resources/resource_manager_sdl3.cpp
 * bridge (unlike inputmgr_canonical_test.cpp/input_world_test.cpp, which fake
 * ResourceManager_GetById to always return null) -- this test needs an actual
 * archive resource to construct a real placed entity through, not just prove
 * the dispatcher doesn't crash on a null lookup.
 *
 * Defines PERSISTENCE_FIXTURES_REAL_RESOURCE_MANAGER before including
 * persistence_fixtures.h so its own ResourceManager_GetById/loco::assets::*
 * fakes step aside instead of colliding with the real symbols at link time.
 *
 * Constructs a real ResourceGameObject (0x1020, scenery\bigfount.dat --
 * GetResourceType gives type 4, the "type != 3" dispatch branch, so this
 * specific call doesn't also need RESDATA_IsBuildingTile/IsRoadTile to
 * classify correctly; that's exercised separately by
 * resource_manager_sdl3_test.cpp's sprite_tile_type_byte() assertions.
 * 0x1020 specifically: it's the same resource entity_host_resource_test.cpp
 * already proved has both a decoded bitmap and animation metadata --
 * Entity::InitBase's host branch bails with initialized=0 for a resource
 * whose .dat has no matching .bmp (core/GameObject.cpp:486), which ruled
 * out an initial attempt with 0x816 (building\factory1.dat, a pure
 * metadata/behavior descriptor with no sprite of its own).
 *
 * Asserts -- not just "didn't abort" -- that the returned object is
 * initialized, is registered in InputMgr's collection, and that
 * entity_count/special_count advanced. See PROGRESS.md's INPUT_PlaceObject
 * entry.
 *
 * Also places resource 0xc54 ("depot top", GetResourceType==3, tile_type 9 --
 * a building value per InputMgr.cpp's is_building_tile set) to exercise the
 * type==3 dispatch branch and its GameVehicle constructor -- the first three
 * commits of this chain only ever exercised the type!=3 -> ResourceGameObject
 * branch (0x1020). Confirmed via resource_manager_sdl3_test.cpp that 0xc54's
 * own tile_type parses to 9 (DepotTop). 0xc54 specifically, not 0xc0a
 * ("points", tile_type 0xb): 0xc0a turned out to have no decoded bitmap
 * (Entity::InitBase bails with initialized=0, same failure mode as 0x816) --
 * itself a real finding about which track-type resources are pure metadata
 * descriptors, tracked in PROGRESS.md rather than silently swapped away.
 */
#define PERSISTENCE_FIXTURES_REAL_RESOURCE_MANAGER
#include "input/InputMgr.h"
#include "persistence_fixtures.h"
#include "core/Entity.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <string>

/* Forward declarations, not #include "resources/resource_manager_sdl3.h":
 * that header's ResourceManager_Init(void*) -> int declaration collides
 * with network/Netman.h's pre-existing ResourceManager_Init(void*) -> void
 * (included transitively via persistence_fixtures.h/InputMgr.h above) --
 * see that header's own comment on initialize_host_resource_manager()/
 * reset_host_resource_manager(), added specifically for this collision. */
namespace loco::assets {
bool initialize_host_resource_manager(const std::string& game_root, std::string* error);
void reset_host_resource_manager();
bool is_host_sprite_resource(const void* resource);
}  // namespace loco::assets

namespace {
bool fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}
}  // namespace

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return fail(SDL_GetError()) ? 0 : 1;
    std::string error;
    if (!loco::assets::initialize_host_resource_manager("lego-loco-unpacked", &error)) {
        return fail(error.c_str()) ? 0 : 1;
    }

    const int32_t count_before = g_input_mgr.ListGetCount();
    const int32_t entity_count_before = g_input_mgr.entity_count;
    const int32_t special_count_before = g_input_mgr.special_count;

    void* placed = INPUT_PlaceObject(&g_input_mgr, 0x1020);
    Entity* entity = static_cast<Entity*>(placed);

    if (entity == nullptr) {
        return fail("INPUT_PlaceObject(0x1020) returned null -- construction "
                     "failed or initialized != 1") ? 0 : 1;
    }
    if (!loco::assets::is_host_sprite_resource(entity->resource)) {
        return fail("placed entity's resource is not a host SpriteResource -- "
                     "test no longer proves what it claims to") ? 0 : 1;
    }
    if (entity->initialized != 1) {
        return fail("INPUT_PlaceObject's constructed entity is not initialized") ? 0 : 1;
    }
    if (g_input_mgr.ListGetCount() != count_before + 1) {
        return fail("INPUT_PlaceObject did not register the entity via ListInsert") ? 0 : 1;
    }
    if (g_input_mgr.ListGetItem(count_before) != entity) {
        return fail("ListInsert stored the entity at the wrong index") ? 0 : 1;
    }
    if (g_input_mgr.entity_count != entity_count_before + 1) {
        return fail("INPUT_PlaceObject did not increment entity_count") ? 0 : 1;
    }
    /* 0x1020 (scenery\bigfount.dat) carries "LeisureDestination 1" --
     * resource_manager_sdl3_test.cpp confirms sprite_leisure_destination_byte()
     * returns true/1 for it. If this assertion is dropped, the entire
     * sprite_leisure_destination_byte() tail of INPUT_PlaceObject passes
     * identically whether or not it actually resolves -- caught by advisor
     * review before this was added. */
    if (g_input_mgr.special_count != special_count_before + 1) {
        return fail("INPUT_PlaceObject did not increment special_count for a "
                     "leisure-destination resource") ? 0 : 1;
    }

    /* Second placement: resource 0xc54 ("depot top"), GetResourceType==3, to
     * exercise the type==3 dispatch branch (0x1020 above is type!=3 ->
     * ResourceGameObject only). tile_type 9 is a building value, so this
     * lands in the GameVehicle constructor -- previously guarded but never
     * reachability-tested end to end. */
    const int32_t count_before_2 = g_input_mgr.ListGetCount();
    const int32_t entity_count_before_2 = g_input_mgr.entity_count;

    void* placed_tile = INPUT_PlaceObject(&g_input_mgr, 0xc54);
    Entity* tile_entity = static_cast<Entity*>(placed_tile);

    if (tile_entity == nullptr) {
        return fail("INPUT_PlaceObject(0xc54) returned null -- construction "
                     "failed or initialized != 1 (resource may lack a decoded "
                     "bitmap, as 0x816 did)") ? 0 : 1;
    }
    if (tile_entity->initialized != 1) {
        return fail("INPUT_PlaceObject's type==3 constructed entity is not "
                     "initialized") ? 0 : 1;
    }
    if (g_input_mgr.ListGetCount() != count_before_2 + 1) {
        return fail("INPUT_PlaceObject did not register the type==3 entity "
                     "via ListInsert") ? 0 : 1;
    }
    if (g_input_mgr.entity_count != entity_count_before_2 + 1) {
        return fail("INPUT_PlaceObject did not increment entity_count for "
                     "the type==3 entity") ? 0 : 1;
    }

    g_input_mgr.ListClearAll();
    loco::assets::reset_host_resource_manager();
    SDL_Quit();
    std::puts("PASS: INPUT_PlaceObject constructs real entities from real "
              "archive resources across both dispatch branches (type!=3 -> "
              "ResourceGameObject, type==3 -> RESDATA_GameVehicle) and "
              "registers them via ListInsert");
    return 0;
}
