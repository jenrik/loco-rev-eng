// Status: VALIDATED
/**
 * sdl3_game_audio.cpp — SDL3 implementation of the host sound boundary.
 *
 * This file is host-only.  It does not alter the original DirectSound ABI or
 * the reconstructed GameAudio/AudioChannel implementations.
 */
#ifndef _WIN32

#include "sdl3_game_audio.h"

#include "resource_manager_sdl3.h"
#include "host_test_events.h"

#include <SDL3/SDL.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

namespace {

struct WaveFormat {
    SDL_AudioSpec spec{};
    const uint8_t* pcm = nullptr;
    uint32_t pcm_bytes = 0;
};

uint16_t read_u16_le(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0]) |
           (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t read_u32_le(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

bool parse_wave(const std::vector<uint8_t>& bytes, WaveFormat* wave) {
    if (!wave || bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return false;
    }

    bool have_format = false;
    size_t cursor = 12;
    while (cursor + 8 <= bytes.size()) {
        const uint8_t* const chunk = bytes.data() + cursor;
        const uint32_t size = read_u32_le(chunk + 4);
        cursor += 8;
        if (size > bytes.size() - cursor) return false;

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            if (size < 16) return false;
            const uint16_t encoding = read_u16_le(bytes.data() + cursor);
            const uint16_t channels = read_u16_le(bytes.data() + cursor + 2);
            const uint32_t frequency = read_u32_le(bytes.data() + cursor + 4);
            const uint16_t bits = read_u16_le(bytes.data() + cursor + 14);
            if (encoding != 1 || channels == 0 || frequency == 0) return false;

            SDL_AudioFormat format = SDL_AUDIO_UNKNOWN;
            if (bits == 8) format = SDL_AUDIO_U8;
            if (bits == 16) format = SDL_AUDIO_S16LE;
            if (format == SDL_AUDIO_UNKNOWN) return false;
            wave->spec = {format, static_cast<int>(channels), static_cast<int>(frequency)};
            have_format = true;
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            wave->pcm = bytes.data() + cursor;
            wave->pcm_bytes = size;
        }

        cursor += size;
        if ((size & 1u) != 0 && cursor < bytes.size()) ++cursor;
    }
    return have_format && wave->pcm != nullptr && wave->pcm_bytes != 0;
}

struct StreamDeleter {
    void operator()(SDL_AudioStream* stream) const {
        if (stream) SDL_DestroyAudioStream(stream);
    }
};
using OwnedStream = std::unique_ptr<SDL_AudioStream, StreamDeleter>;

// One-shot streams: reaped by Pump() when they drain.
std::vector<OwnedStream> g_active_streams;

// Looping-file state: a single background-music stream with owned PCM.
struct LoopStream {
    SDL_AudioStream* stream = nullptr;
    std::vector<uint8_t> pcm;   // owned copy for re-queue
    SDL_AudioSpec spec{};
};
std::unique_ptr<LoopStream> g_loop_stream;

bool load_wave_file(const char* raw_path, std::vector<uint8_t>* pcm, SDL_AudioSpec* spec) {
    // Normalize backslashes from Windows paths.
    std::string normalized(raw_path);
    for (char& c : normalized) { if (c == '\\') c = '/'; }
    const char* path = normalized.c_str();

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto size = static_cast<size_t>(file.tellg());
    if (size < 44) return false;  // minimum WAV header
    file.seekg(0);
    std::vector<uint8_t> bytes(size);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
        return false;

    WaveFormat wave;
    if (!parse_wave(bytes, &wave)) return false;
    *spec = wave.spec;
    pcm->assign(wave.pcm, wave.pcm + wave.pcm_bytes);
    return true;
}

}  // namespace

// =======================================================================
// Resource-based playback (one-shot, from RFH/RFD archive)
// =======================================================================

bool SDL3_GameAudioPreloadResource(uint32_t resource_id) {
    return loco::assets::host_resource_manager().load_asset_by_id(
               resource_id, loco::assets::AssetType::Wave) != nullptr;
}

bool SDL3_GameAudioPlayResource(uint32_t resource_id) {
    const loco::assets::AssetBlob* const asset =
        loco::assets::host_resource_manager().load_asset_by_id(
            resource_id, loco::assets::AssetType::Wave);
    WaveFormat wave;
    if (!asset || !parse_wave(asset->bytes, &wave)) return false;

    SDL_AudioStream* const stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wave.spec, nullptr, nullptr);
    if (!stream) return false;
    OwnedStream owned_stream(stream);
    if (!SDL_PutAudioStreamData(stream, wave.pcm, static_cast<int>(wave.pcm_bytes))) {
        return false;
    }
    if (!SDL_ResumeAudioStreamDevice(stream)) return false;
    g_active_streams.push_back(std::move(owned_stream));
    loco::host_test::emit_audio_queued(resource_id);
    return true;
}

// =======================================================================
// Lifecycle
// =======================================================================

bool SDL3_GameAudioPump() {
    // Reap completed one-shot streams.
    for (auto stream = g_active_streams.begin(); stream != g_active_streams.end();) {
        if (SDL_GetAudioStreamQueued(stream->get()) > 0) {
            ++stream;
        } else {
            stream = g_active_streams.erase(stream);
        }
    }

    // Refill looping stream when it drains.
    if (g_loop_stream && g_loop_stream->stream) {
        if (SDL_GetAudioStreamQueued(g_loop_stream->stream) == 0) {
            SDL_PutAudioStreamData(g_loop_stream->stream,
                                   g_loop_stream->pcm.data(),
                                   static_cast<int>(g_loop_stream->pcm.size()));
        }
    }

    return !g_active_streams.empty() || g_loop_stream != nullptr;
}

void SDL3_GameAudioStopAll() {
    g_active_streams.clear();
    g_loop_stream.reset();
}

// =======================================================================
// Disk-file playback with optional looping
// =======================================================================

bool SDL3_GameAudioPlayFile(const char* path, bool looping) {
    // Stop any existing file playback first.
    g_loop_stream.reset();

    std::vector<uint8_t> pcm;
    SDL_AudioSpec spec;
    if (!load_wave_file(path, &pcm, &spec)) return false;

    SDL_AudioStream* const stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream) return false;

    if (!SDL_PutAudioStreamData(stream, pcm.data(), static_cast<int>(pcm.size()))) {
        SDL_DestroyAudioStream(stream);
        return false;
    }
    if (!SDL_ResumeAudioStreamDevice(stream)) {
        SDL_DestroyAudioStream(stream);
        return false;
    }

    if (looping) {
        auto ls = std::make_unique<LoopStream>();
        ls->stream = stream;
        ls->pcm = std::move(pcm);
        ls->spec = spec;
        g_loop_stream = std::move(ls);
    } else {
        OwnedStream owned(stream);
        g_active_streams.push_back(std::move(owned));
    }
    return true;
}

bool SDL3_GameAudioFilePlaying() {
    return g_loop_stream != nullptr;
}

#endif  // !_WIN32
