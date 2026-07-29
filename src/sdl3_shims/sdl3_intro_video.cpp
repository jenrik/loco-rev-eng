#include "sdl3_intro_video.h"

#ifndef _WIN32

#include "host_test_events.h"
#include "sdl3_window.h"

#include <SDL3/SDL.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace loco::intro {
namespace {

struct Player {
    GstElement* pipeline = nullptr;
    GstElement* sink = nullptr;
    GstBus* bus = nullptr;
    SDL_Texture* texture = nullptr;
    int textureWidth = 0;
    int textureHeight = 0;
    std::size_t clipIndex = 0;
    bool active = false;
    bool gstInitialized = false;
};

Player g_player;

std::filesystem::path assetRoot()
{
    const char* dataRoot = std::getenv("LEGO_LOCO_DATA");
    return dataRoot && *dataRoot ? std::filesystem::path(dataRoot)
                                 : std::filesystem::current_path() / "lego-loco-unpacked";
}

std::string clipPath(std::size_t index)
{
    return (assetRoot() / std::string(kOriginalLaunchVideoPaths.at(index))).string();
}

void destroyPipeline()
{
    if (g_player.pipeline) {
        gst_element_set_state(g_player.pipeline, GST_STATE_NULL);
        gst_object_unref(g_player.pipeline);
        g_player.pipeline = nullptr;
    }
    g_player.sink = nullptr;
    if (g_player.bus) {
        gst_object_unref(g_player.bus);
        g_player.bus = nullptr;
    }
    if (g_player.texture) {
        SDL_DestroyTexture(g_player.texture);
        g_player.texture = nullptr;
    }
    g_player.textureWidth = 0;
    g_player.textureHeight = 0;
}

bool startClip(std::size_t index)
{
    const std::string path = clipPath(index);
    if (!std::filesystem::is_regular_file(path)) {
        std::fprintf(stderr, "[INTRO] missing video: %s\n", path.c_str());
        loco::host_test::emit_intro_video_failed(static_cast<int>(index), "missing file");
        return false;
    }

    GstElement* const pipeline = gst_element_factory_make("playbin", "loco-launch-video");
    GstElement* const sink = gst_element_factory_make("appsink", "loco-video-sink");
    if (!pipeline || !sink) {
        std::fprintf(stderr, "[INTRO] GStreamer playbin/appsink unavailable\n");
        if (pipeline) gst_object_unref(pipeline);
        if (sink) gst_object_unref(sink);
        loco::host_test::emit_intro_video_failed(static_cast<int>(index), "missing GStreamer element");
        return false;
    }

    GstCaps* const caps = gst_caps_from_string("video/x-raw,format=BGRA");
    g_object_set(sink, "caps", caps, "sync", TRUE, "max-buffers", 2u, "drop", TRUE, nullptr);
    gst_caps_unref(caps);

    gchar* const uri = gst_filename_to_uri(path.c_str(), nullptr);
    if (!uri) {
        gst_object_unref(sink);
        gst_object_unref(pipeline);
        loco::host_test::emit_intro_video_failed(static_cast<int>(index), "invalid file URI");
        return false;
    }
    g_object_set(pipeline, "uri", uri, "video-sink", sink, nullptr);
    g_free(uri);

    // Headless test runs deliberately use SDL's dummy audio backend. Avoid an
    // unrelated system-audio failure while still decoding and presenting video.
    const char* audioDriver = std::getenv("SDL_AUDIODRIVER");
    if (audioDriver && std::strcmp(audioDriver, "dummy") == 0) {
        GstElement* const audioSink = gst_element_factory_make("fakesink", nullptr);
        if (audioSink) g_object_set(pipeline, "audio-sink", audioSink, nullptr);
    }

    g_player.pipeline = pipeline;
    g_player.sink = sink;
    g_player.bus = gst_element_get_bus(pipeline);
    g_player.clipIndex = index;
    g_player.active = true;
    loco::host_test::emit_intro_video_started(
        static_cast<int>(index), kOriginalLaunchVideoPaths[index].data());

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        std::fprintf(stderr, "[INTRO] cannot start %s\n", path.c_str());
        loco::host_test::emit_intro_video_failed(static_cast<int>(index), "state change failed");
        destroyPipeline();
        g_player.active = false;
        return false;
    }
    return true;
}

bool advanceClip(bool skipped)
{
    const int finished = static_cast<int>(g_player.clipIndex);
    destroyPipeline();
    loco::host_test::emit_intro_video_finished(finished, skipped);

    for (std::size_t next = static_cast<std::size_t>(finished + 1);
         next < kOriginalLaunchVideoPaths.size(); ++next) {
        if (startClip(next)) return true;
    }

    g_player.active = false;
    loco::host_test::emit_intro_sequence_complete();
    return false;
}

void renderFrame()
{
    if (!g_player.texture) return;
    SDL_Renderer* const renderer = SDL3_GetRenderer();
    if (!renderer) return;

    int outputWidth = 0;
    int outputHeight = 0;
    if (!SDL_GetRenderOutputSize(renderer, &outputWidth, &outputHeight) ||
        outputWidth <= 0 || outputHeight <= 0) return;

    const float scaleX = static_cast<float>(outputWidth) / g_player.textureWidth;
    const float scaleY = static_cast<float>(outputHeight) / g_player.textureHeight;
    const float scale = scaleX < scaleY ? scaleX : scaleY;
    const SDL_FRect destination = {
        (outputWidth - g_player.textureWidth * scale) * 0.5f,
        (outputHeight - g_player.textureHeight * scale) * 0.5f,
        g_player.textureWidth * scale,
        g_player.textureHeight * scale,
    };

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, g_player.texture, nullptr, &destination);
    SDL_RenderPresent(renderer);
}

void consumeLatestSample()
{
    if (!g_player.sink) return;
    GstSample* latest = nullptr;
    while (GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(g_player.sink), 0)) {
        if (latest) gst_sample_unref(latest);
        latest = sample;
    }
    if (!latest) return;

    GstVideoInfo info{};
    GstCaps* const caps = gst_sample_get_caps(latest);
    GstBuffer* const buffer = gst_sample_get_buffer(latest);
    GstMapInfo mapped{};
    SDL_Renderer* const renderer = SDL3_GetRenderer();
    if (!caps || !buffer || !renderer || !gst_video_info_from_caps(&info, caps) ||
        !gst_buffer_map(buffer, &mapped, GST_MAP_READ)) {
        gst_sample_unref(latest);
        return;
    }

    const int width = GST_VIDEO_INFO_WIDTH(&info);
    const int height = GST_VIDEO_INFO_HEIGHT(&info);
    if (!g_player.texture || g_player.textureWidth != width || g_player.textureHeight != height) {
        if (g_player.texture) SDL_DestroyTexture(g_player.texture);
        g_player.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32,
                                             SDL_TEXTUREACCESS_STREAMING, width, height);
        g_player.textureWidth = width;
        g_player.textureHeight = height;
        if (g_player.texture) {
            SDL_SetTextureBlendMode(g_player.texture, SDL_BLENDMODE_NONE);
            loco::host_test::emit_intro_video_frame(static_cast<int>(g_player.clipIndex), width, height);
        }
    }
    if (g_player.texture) {
        SDL_UpdateTexture(g_player.texture, nullptr, mapped.data,
                          GST_VIDEO_INFO_PLANE_STRIDE(&info, 0));
    }
    gst_buffer_unmap(buffer, &mapped);
    gst_sample_unref(latest);
}

}  // namespace

bool startLaunchSequence()
{
    const char* skip = std::getenv("LEGO_LOCO_SKIP_INTRO");
    if (skip && std::strcmp(skip, "1") == 0) return false;

    if (!g_player.gstInitialized) {
        GError* error = nullptr;
        if (!gst_init_check(nullptr, nullptr, &error)) {
            std::fprintf(stderr, "[INTRO] GStreamer initialization failed: %s\n",
                         error ? error->message : "unknown error");
            if (error) g_error_free(error);
            loco::host_test::emit_intro_video_failed(0, "GStreamer initialization failed");
            return false;
        }
        g_player.gstInitialized = true;
    }

    stop();
    for (std::size_t index = 0; index < kOriginalLaunchVideoPaths.size(); ++index) {
        if (startClip(index)) return true;
    }
    loco::host_test::emit_intro_sequence_complete();
    return false;
}

bool isActive()
{
    return g_player.active;
}

bool pumpAndRender()
{
    if (!g_player.active) return false;

    consumeLatestSample();
    while (g_player.bus) {
        GstMessage* const message = gst_bus_pop_filtered(
            g_player.bus, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR | GST_MESSAGE_WARNING));
        if (!message) break;
        const GstMessageType type = GST_MESSAGE_TYPE(message);
        if (type == GST_MESSAGE_ERROR) {
            GError* error = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(message, &error, &debug);
            std::fprintf(stderr, "[INTRO] %s failed: %s%s%s\n",
                         kOriginalLaunchVideoPaths[g_player.clipIndex].data(),
                         error ? error->message : "unknown error",
                         debug ? " (" : "", debug ? debug : "");
            loco::host_test::emit_intro_video_failed(static_cast<int>(g_player.clipIndex),
                                                     error ? error->message : "unknown GStreamer error");
            if (error) g_error_free(error);
            if (debug) g_free(debug);
            gst_message_unref(message);
            return advanceClip(false);
        }
        if (type == GST_MESSAGE_WARNING) {
            GError* error = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_warning(message, &error, &debug);
            std::fprintf(stderr, "[INTRO] %s warning: %s%s%s\n",
                         kOriginalLaunchVideoPaths[g_player.clipIndex].data(),
                         error ? error->message : "unknown warning",
                         debug ? " (" : "", debug ? debug : "");
            if (error) g_error_free(error);
            if (debug) g_free(debug);
        }
        gst_message_unref(message);
        // Only advance on EOS; warnings are non-fatal (e.g. missing codec).
        if (type == GST_MESSAGE_EOS) return advanceClip(false);
    }

    renderFrame();
    return g_player.active;
}

void skipAll()
{
    if (!g_player.active) return;

    const int finished = static_cast<int>(g_player.clipIndex);
    destroyPipeline();
    g_player.active = false;
    loco::host_test::emit_intro_video_finished(finished, true);
    loco::host_test::emit_intro_sequence_complete();
}

void stop()
{
    if (!g_player.active && !g_player.pipeline && !g_player.texture) return;
    destroyPipeline();
    g_player.active = false;
}

}  // namespace loco::intro

#endif  // !_WIN32
