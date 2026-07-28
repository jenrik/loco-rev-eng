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

#include <SDL3/SDL.h>

#include <cstring>
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
std::vector<OwnedStream> g_active_streams;

}  // namespace

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
    return true;
}

bool SDL3_GameAudioPump() {
    for (auto stream = g_active_streams.begin(); stream != g_active_streams.end();) {
        if (SDL_GetAudioStreamQueued(stream->get()) > 0) {
            ++stream;
        } else {
            stream = g_active_streams.erase(stream);
        }
    }
    return !g_active_streams.empty();
}

void SDL3_GameAudioStopAll() {
    g_active_streams.clear();
}

#endif  // !_WIN32
