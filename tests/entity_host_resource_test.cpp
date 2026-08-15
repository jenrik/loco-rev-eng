// Status: VALIDATED
/**
 * entity_host_resource_test.cpp — reachability proof for Entity::InitBase's
 * host branch (core/GameObject.cpp).
 *
 * Constructs a real Entity with a real archive resource ID, through the
 * REAL resource_manager_sdl3.cpp bridge (not the fixture fake used by the
 * persistence tests) -- proving the host branch added to InitBase/
 * SetAnimState/SetFrame actually fires on a real loco::assets::SpriteResource*
 * without crashing on the null acquire_surface/release_surface vtable slots
 * the original code path would otherwise dereference. See PROGRESS.md's
 * InitBase host-safety entry: this is the "measure it fires" half of the
 * fix, before INPUT_PlaceObject's dispatcher can safely exercise it.
 */
#include "core/Entity.h"
#include "resources/resource_manager_sdl3.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <string>

/* ---- Minimal link fixtures for GameObject.o/Entity.o's non-resource
 * dependencies. This test only exercises Entity::InitBase's *host* branch
 * (a real archive resource_id, no g_tilemap/g_audio/g_building_mgr set up),
 * so every one of these is a harmless no-op -- they exist only to satisfy
 * the linker, mirroring tests/persistence_fixtures.h's stub style but
 * deliberately NOT reusing that file, since it fakes ResourceManager_GetById
 * (this test needs the REAL one, from resource_manager_sdl3.cpp). ---- */
extern "C" {
uint32_t CRT_rand(void) { return 0; }
char* _strncpy(char* dst, const char*, size_t) { return dst; }
int IsCharAlphaNumericA(char) { return 0; }
void SetRect(RECT* r, int left, int top, int right, int bottom) {
    if (r) { r->left = left; r->top = top; r->right = right; r->bottom = bottom; }
}
void SetRectEmpty(RECT* r) { if (r) { r->left = r->top = r->right = r->bottom = 0; } }
BOOL IsRectEmpty(const RECT*) { return TRUE; }
BOOL OffsetRect(RECT*, int, int) { return TRUE; }
BOOL IntersectRect(RECT*, const RECT*, const RECT*) { return FALSE; }
}
extern "C" void TileMap_InvalidateRect(void*, int, int, int, int) {}
void RESMGR_ReleaseSoundResource(void*) {}
void GameAudio_AllocChannel(void*, int, void** out_ch, int, int, int, int) {
    if (out_ch) *out_ch = nullptr;
}
void CGWND_AudioChannel_Release(void*) {}
void CGWND_AudioChannel_Play(void*) {}
void CGWND_AudioChannel_Stop(void*) {}
void CGWND_AudioChannel_UpdatePosition(void*, int, int) {}
bool UIPANEL_Blit(void*, uint32_t, uint32_t, int32_t, uint32_t,
                   void*, uint32_t, uint32_t, int32_t, uint32_t, uint32_t) {
    return false;
}
void* operator_new(size_t size) { return std::malloc(size); }
void  GLOBAL_free(void* p) { std::free(p); }
extern "C" void OutputDebugStringA(const char* s) { if (s) std::fprintf(stderr, "DEBUG: %s\n", s); }

/* UIPANEL_Surface's live-instance counter (0x00485254, canonically defined
 * in shared/link_stubs.cpp) -- defined locally rather than pulling that
 * whole stub file in, same reasoning as this file's other local fixtures.
 * Needed once Entity::Draw/DrawConnected started routing host
 * SpriteResources through UIPANEL_Surface's real constructor
 * (graphics/UIPANEL_Surface_lifecycle.cpp). */
int32_t g_ref_count = 0;

/* g_ddraw (0x485440, IDirectDraw4*) and SDL3_WrapSdlSurfaceAsDirectDraw
 * (graphics/sdl3_ddraw.cpp) -- the real DirectDraw device/surface wrapper
 * is not linked into this narrow test (it would pull in
 * graphics/sdl3_window.cpp, which duplicates several of this file's own
 * Win32 stub fixtures above). Stubbed only to satisfy the linker. */
void* g_ddraw = nullptr;
struct IDirectDrawSurface4;
IDirectDrawSurface4* SDL3_WrapSdlSurfaceAsDirectDraw(SDL_Surface*) { return nullptr; }

void* g_primary_surface = nullptr;
class ResourceManager {};
ResourceManager g_resmgr;
void* g_audio = nullptr;
uint32_t g_game_time = 0;
HWND g_main_window = nullptr;
double _DAT_00481170 = 0.0;
char g_empty_string = 0;
void* g_tilemap = nullptr;

namespace {
bool fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}
}  // namespace

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return fail(SDL_GetError()) ? 0 : 1;
    std::string error;
    if (!loco::assets::host_resource_manager().initialize("lego-loco-unpacked", &error)) {
        return fail(error.c_str()) ? 0 : 1;
    }

    // scenery\bigfount.dat (0x1020) has real physical_occupancy/animation
    // metadata, verified against real resource.RFD bytes elsewhere in this
    // suite -- a real host SpriteResource, not a synthetic fixture.
    //
    // Constructed and destroyed in a nested scope, deterministically before
    // host_resource_manager().reset() below, so ~Entity's host branch (the
    // release_surface skip) runs on a still-live resource -- not just
    // incidentally at process exit -- and a crash there fails this test
    // rather than silently passing.
    {
        Entity entity(0x1020, -1, 0, 0);

        if (!loco::assets::is_host_sprite_resource(entity.resource)) {
            return fail("Entity(0x1020, ...) did not load a host SpriteResource -- "
                         "test no longer proves what it claims to") ? 0 : 1;
        }
        if (entity.initialized != 1) {
            return fail("Entity::InitBase's host branch left the entity uninitialized") ? 0 : 1;
        }
        if (entity.anim_index < 0) {
            return fail("Entity::InitBase's host branch left anim_index unresolved (-1)") ? 0 : 1;
        }

        // Entity::Update (vtable[10]) runs every game tick for any
        // initialized entity -- InputMgr's per-tick loop calls it on the
        // whole live-entity collection (input/InputMgr.cpp). Its host guard
        // (core/GameObject.cpp) is a separate fix from InitBase's: reading
        // resource+0x20's FrameData array on a host SpriteResource has no
        // verified field mapping yet, so it rejects loudly and holds the
        // current frame instead of reading past the allocation. Call it
        // twice and check the guard's own hit counter, not just
        // frame_index -- the original also has early-return paths
        // (single-frame no-loop animation, already-at-end-frame,
        // mid-wait) that would leave frame_index unchanged with no guard
        // at all, so an unchanged frame_index alone doesn't prove the
        // guard fired rather than one of those.
        const uint32_t hits_before = loco::host_test::entity_update_host_guard_hit_count();
        const int frame_before = entity.frame_index;
        entity.Update();
        entity.Update();
        if (entity.frame_index != frame_before) {
            return fail("Entity::Update advanced frame_index on a host "
                         "SpriteResource -- its host guard did not fire") ? 0 : 1;
        }
        if (loco::host_test::entity_update_host_guard_hit_count() != hits_before + 2) {
            return fail("Entity::Update's host guard did not run on both calls -- "
                         "frame_index staying put was coincidental, not proof") ? 0 : 1;
        }
    }
    std::puts("PASS: ~Entity's host branch released a live host SpriteResource "
              "without calling the original's null release_surface slot");

    loco::assets::host_resource_manager().reset();
    SDL_Quit();
    std::puts("PASS: Entity::InitBase's host branch loads a real archive resource "
              "without crashing on the original's null acquire/release_surface slots");
    return 0;
}
