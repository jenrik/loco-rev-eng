// Status: VALIDATED
/**
 * sdl3_dsound_adapter_test.cpp — DirectSound bridge regression
 *
 * Exercises GameAudio::Init() end to end through the real
 * AudioDirectSoundDevice/AudioDirectSoundBuffer interface (audio/
 * AudioChannel.h) down to the concrete SDL3 backend (audio/sdl3_dsound.cpp)
 * via the adapter (audio/sdl3_dsound_adapter.cpp) -- this path has zero
 * other test coverage: GameAudio::Init() is never called on the real host
 * bootstrap path at all (core/HostMode3Bootstrap.cpp constructs a bare
 * `GameAudio` and defers device init to the separate `SDL3_GameAudio`
 * subsystem instead), so this bridge is otherwise entirely unexercised.
 *
 * Asserts Init() succeeds and both the device and primary-buffer pointers
 * come back non-null, then Cleanup() runs clean (no double-free/double-
 * destruct -- see the sdl3_dsound.cpp Release() fix this bridge depends on).
 */
#include "../audio/GameAudio.h"
#include "../audio/sdl3_dsound_adapter.h"
#include "../resources/ResourceManager.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
bool fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}
}  // namespace

/* ---- Fixtures: this cone links audio/GameAudio.cpp + audio/AudioChannel.cpp
 * + audio/sdl3_dsound.cpp + audio/sdl3_dsound_adapter.cpp + the real
 * game/ConfigIni.cpp, none of which pull these in transitively. ---- */

void* operator_new(size_t size) { return std::malloc(size); }
void  GLOBAL_free(void* ptr) { std::free(ptr); }
void  CRT_memset_pattern(void* dst, int value, int count, void*) {
    std::memset(dst, value, static_cast<size_t>(count));
}

// DirectSoundEnumerateA: no real caller in this cone (only reached on the
// select_best!=0 branch, which just logs an enumeration attempt before
// falling through to the real Ordinal_1 device creation this test exercises).
void Ordinal_2(void*) {}

// Real implementation lives in shared/stubs_link001_batch3_resource_audio.cpp,
// which this narrow cone doesn't link (it pulls in the wider resource/audio
// stub surface) -- delegates to the same real adapter constructor that file's
// real Ordinal_1 calls on host, so this exercises the actual bridge under
// test, just one indirection shorter.
int32_t Ordinal_1(int32_t, void* out_object) {
    if (out_object == nullptr) return -1;
    AudioDirectSoundDevice* device = Sdl3CreateAudioDirectSoundDevice();
    *static_cast<AudioDirectSoundDevice**>(out_object) = device;
    return device != nullptr ? 0 : -1;
}

// Not reached: g_config_ini stays null, so GameAudio::Init() never calls this.
int32_t Config_GetIniInt(void*, const char*, const char*, int32_t default_val) {
    return default_val;
}

// Not reached: only PlayResource/PlayResourceEx call this, not Init/Cleanup.
void* RESMGR_GetById(void*, uint32_t) { return nullptr; }

void* g_config_ini = nullptr;              /* skips Config_GetIniInt entirely */
int32_t g_listener_x = 0;
int32_t g_listener_y = 0;
uint32_t g_game_time = 0;
ResourceManager g_resmgr;
GameAudio* g_audio = nullptr;

extern "C" void OutputDebugStringA(const char* s) {
    if (s) std::fprintf(stderr, "DEBUG: %s\n", s);
}
// Neither reached: LoadSound (RESMGR_LoadSoundResource) and the error-string
// helper are only called from AudioChannel::LoadSound/error paths, not
// Init/Cleanup -- declared only to satisfy the link.
void RESMGR_LoadSoundResource(void*) {}
void DDRAW_GetDsoundErrorString(int32_t) {}

int main() {
    GameAudio audio;
    uint32_t ok = audio.Init();
    if (ok == 0) {
        return fail("GameAudio::Init() reported failure") ? 0 : 1;
    }
    if (audio.ds_device == nullptr) {
        return fail("GameAudio::Init() left ds_device null") ? 0 : 1;
    }
    if (audio.ds_primary == nullptr) {
        return fail("GameAudio::Init() left ds_primary null") ? 0 : 1;
    }
    if (audio.channels == nullptr || audio.channel_usage == nullptr) {
        return fail("GameAudio::Init() did not allocate the channel array") ? 0 : 1;
    }
    if (audio.max_channels <= 0) {
        return fail("GameAudio::Init() left max_channels unset") ? 0 : 1;
    }

    audio.Cleanup(nullptr);
    if (audio.ds_device != nullptr || audio.ds_primary != nullptr) {
        return fail("GameAudio::Cleanup() did not clear device/primary pointers") ? 0 : 1;
    }

    std::fprintf(stderr, "PASS\n");
    return 0;
}
