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
 *
 * Also proves Entity::Update's and Entity::PlayAnimation's host branches
 * (added 2026-08-22 once the .dat animation-row token mapping was fully
 * confirmed by disassembly -- see shared/types.h's FrameData and
 * resources/resource_manager_sdl3.h's AnimationFrameSet doc comments):
 * resource 0x402 ("train whistle"-shaped animation set, verified directly
 * against the shipped resource.RFD bytes) has a real "complete" frame set
 * (anim_index 1: "complete 5 8 1 0  0 -1  22301 -1 4  0" -- start_frame=5,
 * end_frame=8, frame_delay=1, not connected, sound_resource_id=22301) that
 * both actually advances frame_index across repeated Update() calls and
 * exercises PlayAnimation's host branch with a real nonzero sound resource
 * ID, unlike 0x1020 below (whose Update() calls may legitimately leave
 * frame_index unchanged via the original's own early-return paths).
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
/* Non-null so Entity::PlayAnimation's real body (including its new host
 * branch, core/GameObject.cpp) actually runs instead of bailing at its
 * first `g_audio == nullptr` check -- GameAudio_AllocChannel/
 * CGWND_AudioChannel_Play above are already harmless no-op stubs, so this
 * is safe to flip on for the whole file. */
int g_audio_sentinel = 0;
void* g_audio = &g_audio_sentinel;
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
        // whole live-entity collection (input/InputMgr.cpp). This resource's
        // default animation state may legitimately be single-frame or
        // already at its end (the original's own early-return paths), so
        // this is only a crash-safety smoke test -- resource 0x402 below is
        // the real proof that frame_index actually advances.
        entity.Update();
        entity.Update();
    }
    std::puts("PASS: ~Entity's host branch released a live host SpriteResource "
              "without calling the original's null release_surface slot");

    // Resource 0x402's "complete" frame set (anim_index 1) is a real,
    // multi-frame, non-connected animation -- confirmed directly against
    // resource.RFD's animation-row text: "complete 5 8 1 0  0 -1  22301 -1
    // 4  0" (start_frame=5, end_frame=8, frame_delay=1, is_connected=0,
    // restart_delay=0, next_frame_set=-1, sound_resource_id=22301,
    // replay_delay=-1, volume=4, flip_horizontal=0). Constructed with
    // anim_idx=1 directly (not -1/"default") to land on this specific
    // frame set.
    {
        Entity entity(0x402, 1, 0, 0);

        if (!loco::assets::is_host_sprite_resource(entity.resource)) {
            return fail("Entity(0x402, 1, ...) did not load a host SpriteResource -- "
                         "test no longer proves what it claims to") ? 0 : 1;
        }
        if (entity.anim_index != 1) {
            return fail("Entity::InitBase's host branch did not honor the requested "
                         "anim_index 1 for resource 0x402") ? 0 : 1;
        }

        // Entity::Update's host branch (core/GameObject.cpp) must actually
        // step frame_index forward using AnimationFrameSet's start_frame/
        // end_frame/frame_delay/is_connected fields, not just hold the
        // current frame -- this is the real behavioral proof the older
        // guard-hit-counter test (0x1020 above, before the .dat token
        // mapping was confirmed) couldn't provide.
        const int frame_before = entity.frame_index;
        entity.Update();
        entity.Update();
        if (entity.frame_index == frame_before) {
            return fail("Entity::Update's host branch did not advance frame_index "
                         "on resource 0x402's real multi-frame \"complete\" animation "
                         "(start_frame=5, end_frame=8, frame_delay=1)") ? 0 : 1;
        }
        if (entity.frame_index < 5 || entity.frame_index > 8) {
            return fail("Entity::Update's host branch produced a frame_index outside "
                         "the \"complete\" frame set's [5,8] range") ? 0 : 1;
        }

        // Entity::PlayAnimation's host branch (core/GameObject.cpp): this
        // frame set's real sound_resource_id (22301) is nonzero, so calling
        // it directly exercises the AnimationFrameSet-sourced audio_delay/
        // volume lookup this fix added in place of the undersized-host-
        // object FrameData dereference the previous code had. Entity's
        // construction above (via SetAnimState's own existing host branch)
        // already called this same path once; call it again directly here
        // -- the load-bearing claim is "does not crash or read past the
        // host SpriteResource's allocation", not a specific audio outcome
        // (GameAudio_AllocChannel is a no-op stub in this test).
        entity.PlayAnimation(22301);
        entity.PlayAnimation(22301);
    }
    std::puts("PASS: Entity::Update's and Entity::PlayAnimation's host branches "
              "advance a real multi-frame animation and play its real sound "
              "resource without reading past a host SpriteResource's allocation");

    loco::assets::host_resource_manager().reset();
    SDL_Quit();
    std::puts("PASS: Entity::InitBase's host branch loads a real archive resource "
              "without crashing on the original's null acquire/release_surface slots");
    return 0;
}
