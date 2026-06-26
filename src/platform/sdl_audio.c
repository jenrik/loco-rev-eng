/*
 * Lego Loco (1998) - Native Linux Port
 * src/platform/sdl_audio.c — SDL2_mixer + GStreamer replacement for Win32 audio
 *
 * Replaces the following Win32 subsystems documented in src/audio/audio.c:
 *   DSOUND.DLL  Ordinal_1 (DirectSoundCreate)       -> Mix_OpenAudio
 *   DSOUND.DLL  Ordinal_2 (DirectSoundEnumerate)    -> SDL_GetNumAudioDevices
 *   WINMM.DLL   PlaySoundA                          -> Mix_LoadMUS + Mix_PlayMusic
 *   WINMM.DLL   mciSendCommandA (MCI_SETAUDIO)      -> GStreamer g_object_set
 *   MSVFW32.DLL MCIWndRegisterClass / CreateWindow  -> GStreamer gst_parse_launch
 *   USER32.DLL  SendMessageA (MCIWNDM_OPEN etc.)    -> gst_element_set_state
 *
 * WIN32 → LINUX API mapping table:
 *   DirectSoundCreate(NULL, &g_pDS, NULL)
 *     -> Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)
 *
 *   IDirectSound::CreateSoundBuffer(&desc, &pBuf, NULL)
 *     -> Mix_LoadWAV(path)  →  Mix_Chunk *
 *
 *   IDirectSoundBuffer::Play(0, 0, DSBPLAY_LOOPING)
 *     -> Mix_PlayChannel(-1, chunk, -1)   // -1 loops = infinite
 *
 *   IDirectSoundBuffer::Play(0, 0, 0)
 *     -> Mix_PlayChannel(-1, chunk, 0)    // 0 loops = play once
 *
 *   IDirectSoundBuffer::Stop()
 *     -> Mix_HaltChannel(channel)
 *
 *   IDirectSoundBuffer::SetVolume(lMillibels)  range [-10000, 0]
 *     -> Mix_Volume(channel, sdl_vol)
 *        where sdl_vol = (int)((lMillibels + 10000) * MIX_MAX_VOLUME / 10000)
 *
 *   PlaySoundA(path, NULL, SND_FILENAME|SND_ASYNC)
 *     -> g_bgmChunk = Mix_LoadMUS(path); Mix_PlayMusic(g_bgmChunk, 1)
 *
 *   PlaySoundA(NULL, NULL, 0)   [stop]
 *     -> Mix_HaltMusic(); Mix_FreeMusic(g_bgmMusic); g_bgmMusic = NULL
 *
 *   MCIWndRegisterClass() + CreateWindowExA("MCIWndClass", ...)
 *     -> gst_init(NULL, NULL); g_gstPipeline = gst_parse_launch("playbin uri=...")
 *
 *   SendMessageA(hMCIWnd, MCIWNDM_OPEN, 0, filepath)
 *     -> g_object_set(G_OBJECT(g_gstPipeline), "uri", uri, NULL)
 *
 *   SendMessageA(hMCIWnd, 0x804, 0, 0)   [MCIWNDM_STOP]
 *     -> gst_element_set_state(g_gstPipeline, GST_STATE_NULL)
 *
 *   SendMessageA(hMCIWnd, 0x10, 0, 0)    [WM_CLOSE]
 *     -> gst_object_unref(g_gstPipeline); g_gstPipeline = NULL
 *
 *   mciSendCommandA(id, MCI_SETAUDIO, MCI_SETAUDIO_VOLUME, &params)
 *     -> g_object_set(G_OBJECT(g_gstPipeline), "volume", (gdouble)vol/1000.0, NULL)
 *
 *   DirectSoundEnumerate(callback, ctx)
 *     -> for (i = 0; i < SDL_GetNumAudioDevices(0); i++)
 *            SDL_GetAudioDeviceName(i, 0)
 *
 * Volume conversion formula (Win32 millibels to SDL2):
 *   Win32: -10000 = silent (DSBVOLUME_MIN), 0 = full (DSBVOLUME_MAX)
 *   SDL2:  0 = silent,  MIX_MAX_VOLUME (128) = full
 *   sdl_vol = (int)((millibels + 10000) * MIX_MAX_VOLUME / 10000)
 *   Clamp result to [0, MIX_MAX_VOLUME].
 *
 * Build dependencies:
 *   -lSDL2 -lSDL2_mixer
 *   Optional video: $(pkg-config --libs gstreamer-1.0 gstreamer-video-1.0)
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/loco_types.h"

/* =========================================================================
 * Module-level state
 * ========================================================================= */

/* SDL2_mixer audio device state */
static int    g_audioInitialised = 0;

/* Background music handle (replaces PlaySoundA WAV path) */
/* WIN32: no explicit handle — PlaySoundA is fire-and-forget */
/* LINUX: Mix_Music* must be kept alive until Mix_FreeMusic is called */
static Mix_Music *g_bgmMusic = NULL;

/* Active channel for the most-recently-played one-shot effect. */
/* Set by SDL_Audio_PlayEffect; used by SDL_Audio_StopEffect.   */
static int g_lastEffectChannel = -1;

/*
 * GStreamer pipeline for AVI/video playback (replaces MCIWnd).
 * Conditionally compiled: define LOCO_ENABLE_VIDEO_GST to activate.
 *
 * WIN32: HWND hMCIWnd stored in CMciVideoPlayer::this+4
 * LINUX: GstElement* pipeline (playbin element)
 */
#ifdef LOCO_ENABLE_VIDEO_GST
#include <gst/gst.h>
static GstElement *g_gstPipeline = NULL;
#endif

/* =========================================================================
 * SDL_Audio_Init  —  replaces DS_Init (FUN_0045b7e0)
 *
 * Initialises SDL2_mixer as a direct replacement for DirectSoundCreate.
 * Called once at game startup before any sound effects are loaded.
 *
 * WIN32: DirectSoundCreate(NULL, &g_pDS, NULL)
 *        IDirectSound::SetCooperativeLevel(hWnd, DSSCL_PRIORITY)
 * LINUX: SDL_Init(SDL_INIT_AUDIO) + Mix_OpenAudio
 *
 * Returns 1 on success, 0 on failure.
 * ========================================================================= */
int SDL_Audio_Init(void)
{
    if (g_audioInitialised)
        return 1; /* already initialised — mirrors DS_Init guard */

    /* WIN32: SDL subsystem init is handled by the window layer;
     *        DirectSound does not require separate subsystem init.
     * LINUX: SDL_INIT_AUDIO must be set before Mix_OpenAudio. */
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Audio_Init: SDL_InitSubSystem failed: %s\n",
                SDL_GetError());
        return 0;
    }

    /* WIN32: DirectSoundCreate(NULL, &g_pDS, NULL)
     *   Creates the IDirectSound COM object bound to the default device.
     *   Equivalent device selection: first entry from DirectSoundEnumerate.
     *
     * LINUX: Mix_OpenAudio(freq, format, channels, chunksize)
     *   freq      = 44100   (CD quality; matches typical DirectSound init)
     *   format    = MIX_DEFAULT_FORMAT  (AUDIO_S16SYS on all platforms)
     *   channels  = 2       (stereo; DSOUND uses WAVEFORMATEX nChannels=2)
     *   chunksize = 2048    (buffer size; tune for latency vs stutter) */
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "SDL_Audio_Init: Mix_OpenAudio failed: %s\n",
                Mix_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return 0;
    }

    /* WIN32: DirectSound allocates a fixed number of mixing channels internally.
     *        Loco uses ~32 simultaneous DirectSound buffers in busy scenes.
     * LINUX: Mix_AllocateChannels reserves that many SDL2 mixer channels. */
    Mix_AllocateChannels(32);

    /* WIN32: IDirectSound::SetCooperativeLevel(hWnd, DSSCL_PRIORITY)
     *   Requests priority access so the app can set the primary buffer format.
     *   Not needed on Linux — Mix_OpenAudio already owns the device. */

    g_audioInitialised = 1;
    fprintf(stderr, "SDL_Audio_Init: audio subsystem ready (SDL2_mixer)\n");
    return 1;
}

/* =========================================================================
 * SDL_Audio_Shutdown  —  replaces DS_SaveAndShutdown (FUN_0045bb20)
 *
 * Stops all active audio, frees the background music handle, and shuts
 * down the SDL2_mixer device.
 *
 * WIN32: IDirectSound::Release()  (vtable destructor chain)
 *        IDirectSoundBuffer::Stop() on each active buffer before release
 * LINUX: Mix_HaltChannel(-1)  — stop all channels
 *        Mix_HaltMusic()
 *        Mix_CloseAudio()
 * ========================================================================= */
void SDL_Audio_Shutdown(void)
{
    if (!g_audioInitialised)
        return;

    /* WIN32: IDirectSoundBuffer::Stop() on every allocated buffer */
    /* LINUX: -1 stops every mixer channel simultaneously */
    Mix_HaltChannel(-1);

    /* WIN32: PlaySoundA(NULL, NULL, 0)  — purge any async PlaySoundA call */
    /* LINUX: Mix_HaltMusic stops the background music channel */
    Mix_HaltMusic();

    if (g_bgmMusic != NULL) {
        /* WIN32: No explicit free; PlaySoundA manages its own buffer lifetime.
         * LINUX: Mix_Music* must be freed manually after halting playback. */
        Mix_FreeMusic(g_bgmMusic);
        g_bgmMusic = NULL;
    }

#ifdef LOCO_ENABLE_VIDEO_GST
    if (g_gstPipeline != NULL) {
        /* WIN32: SendMessageA(hMCIWnd, MCIWNDM_STOP=0x804, 0, 0)
         *        SendMessageA(hMCIWnd, WM_CLOSE=0x10, 0, 0) */
        /* LINUX: gst_element_set_state(g_gstPipeline, GST_STATE_NULL) */
        gst_element_set_state(g_gstPipeline, GST_STATE_NULL);
        /* WIN32: hMCIWnd = NULL (stored in CMciVideoPlayer::this+4) */
        /* LINUX: gst_object_unref releases all pipeline resources */
        gst_object_unref(g_gstPipeline);
        g_gstPipeline = NULL;
    }
#endif

    /* WIN32: IDirectSound::Release() — final COM reference drop */
    /* LINUX: Mix_CloseAudio releases the audio device */
    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    g_audioInitialised = 0;

    fprintf(stderr, "SDL_Audio_Shutdown: audio subsystem closed\n");
}

/* =========================================================================
 * SDL_Audio_MillibelsToSDLVolume  —  volume conversion helper
 *
 * Converts a Win32 DirectSound millibel volume value to an SDL2_mixer
 * volume integer.
 *
 * WIN32: IDirectSoundBuffer::SetVolume(LONG lVol)
 *   lVol range: DSBVOLUME_MIN (-10000) = silent  to  DSBVOLUME_MAX (0) = full
 *   Unit: hundredths of a decibel (millibels)
 *
 * LINUX: Mix_Volume(channel, int volume)
 *   volume range: 0 = silent  to  MIX_MAX_VOLUME (128) = full
 *
 * Formula: sdl_vol = clamp((millibels + 10000) * MIX_MAX_VOLUME / 10000, 0, 128)
 *
 * Examples:
 *   -10000 millibels  ->  0     (silent)
 *    -5000 millibels  ->  64    (half volume)
 *        0 millibels  ->  128   (full volume / MIX_MAX_VOLUME)
 * ========================================================================= */
int SDL_Audio_MillibelsToSDLVolume(int millibels)
{
    int sdl_vol;

    /* Clamp input to the valid Win32 range first */
    if (millibels < -10000) millibels = -10000;
    if (millibels >      0) millibels =      0;

    /* Linear mapping: shift up by 10000 then scale to 0..MIX_MAX_VOLUME */
    sdl_vol = (millibels + 10000) * MIX_MAX_VOLUME / 10000;

    /* Clamp output (rounding edge-cases) */
    if (sdl_vol < 0)              sdl_vol = 0;
    if (sdl_vol > MIX_MAX_VOLUME) sdl_vol = MIX_MAX_VOLUME;

    return sdl_vol;
}

/* =========================================================================
 * SDL_Audio_LoadSoundEffect  —  replaces IDirectSound::CreateSoundBuffer
 *
 * Loads a WAV file from disk into a Mix_Chunk, which is the SDL2_mixer
 * equivalent of an IDirectSoundBuffer populated with PCM audio data.
 *
 * WIN32: IDirectSound::CreateSoundBuffer(&wfxDesc, &pBuf, NULL)
 *        then ReadFile / UnlockBuffer to fill with PCM data
 * LINUX: Mix_LoadWAV(path)  — single call; SDL_mixer handles format decode
 *
 * Returns Mix_Chunk* on success, NULL on failure.
 * The caller owns the returned pointer and must call SDL_Audio_FreeSFX.
 * ========================================================================= */
Mix_Chunk *SDL_Audio_LoadSoundEffect(const char *wavPath)
{
    Mix_Chunk *chunk;

    if (!g_audioInitialised) {
        fprintf(stderr, "SDL_Audio_LoadSoundEffect: audio not initialised\n");
        return NULL;
    }

    /* WIN32: CreateSoundBuffer fills the buffer from a resource archive
     *        via ReadFile.  Decompressed PCM is written to the locked buffer.
     *
     * LINUX: Mix_LoadWAV decodes the WAV file directly.
     *        The path here is the on-disk WAV extracted from the game archive. */
    chunk = Mix_LoadWAV(wavPath);
    if (chunk == NULL) {
        fprintf(stderr, "SDL_Audio_LoadSoundEffect: Mix_LoadWAV('%s') failed: %s\n",
                wavPath, Mix_GetError());
    }
    return chunk;
}

/* =========================================================================
 * SDL_Audio_PlaySFX  —  replaces IDirectSoundBuffer::Play (one-shot)
 *
 * Plays a sound effect once on any available mixer channel.
 *
 * WIN32: IDirectSoundBuffer::SetCurrentPosition(0)
 *        IDirectSoundBuffer::Play(0, 0, 0)   [flags=0 → play once]
 * LINUX: Mix_PlayChannel(-1, chunk, 0)
 *   -1  = auto-assign any free channel
 *    0  = loop count 0 means play once (total 1 play, 0 additional loops)
 *
 * Returns the channel number assigned, or -1 on failure.
 * ========================================================================= */
int SDL_Audio_PlaySFX(Mix_Chunk *chunk)
{
    int channel;

    if (chunk == NULL) return -1;

    /* WIN32: IDirectSoundBuffer::SetCurrentPosition(0)
     *   Rewinds to the start before each play; SDL2_mixer does this automatically. */

    /* WIN32: IDirectSoundBuffer::Play(0, 0, 0)  [one-shot]
     * LINUX: Mix_PlayChannel(-1, chunk, 0)       [any channel, play once] */
    channel = Mix_PlayChannel(-1, chunk, 0);
    if (channel < 0) {
        fprintf(stderr, "SDL_Audio_PlaySFX: Mix_PlayChannel failed: %s\n",
                Mix_GetError());
    }
    g_lastEffectChannel = channel;
    return channel;
}

/* =========================================================================
 * SDL_Audio_PlaySFXLooping  —  replaces IDirectSoundBuffer::Play (looping)
 *
 * Plays a sound effect in an infinite loop (ambient audio, train, rain, etc.)
 *
 * WIN32: IDirectSoundBuffer::Play(0, 0, DSBPLAY_LOOPING)
 *   DSBPLAY_LOOPING = 0x00000001
 * LINUX: Mix_PlayChannel(-1, chunk, -1)
 *   -1 loop count = infinite repeat until Mix_HaltChannel is called
 *
 * Returns the channel number assigned, or -1 on failure.
 * ========================================================================= */
int SDL_Audio_PlaySFXLooping(Mix_Chunk *chunk)
{
    int channel;

    if (chunk == NULL) return -1;

    /* WIN32: IDirectSoundBuffer::Play(0, 0, DSBPLAY_LOOPING)
     * LINUX: Mix_PlayChannel(-1, chunk, -1) */
    channel = Mix_PlayChannel(-1, chunk, -1);
    if (channel < 0) {
        fprintf(stderr, "SDL_Audio_PlaySFXLooping: Mix_PlayChannel failed: %s\n",
                Mix_GetError());
    }
    return channel;
}

/* =========================================================================
 * SDL_Audio_StopChannel  —  replaces IDirectSoundBuffer::Stop
 *
 * Stops a specific mixer channel immediately.
 *
 * WIN32: IDirectSoundBuffer::Stop()
 *   Stops playback; buffer position is reset to 0 internally.
 * LINUX: Mix_HaltChannel(channel)
 *   Stops the channel; the Mix_Chunk remains valid and can be replayed.
 * ========================================================================= */
void SDL_Audio_StopChannel(int channel)
{
    /* WIN32: IDirectSoundBuffer::Stop() */
    /* LINUX: Mix_HaltChannel(channel)  — -1 stops all channels */
    Mix_HaltChannel(channel);
}

/* =========================================================================
 * SDL_Audio_SetChannelVolume  —  replaces IDirectSoundBuffer::SetVolume
 *
 * Sets the playback volume for a specific mixer channel using Win32
 * millibel values as input (sourced directly from game code that
 * calls IDirectSoundBuffer::SetVolume).
 *
 * WIN32: IDirectSoundBuffer::SetVolume(LONG lVol)
 *   lVol: -10000 (DSBVOLUME_MIN, silent) to 0 (DSBVOLUME_MAX, full)
 * LINUX: Mix_Volume(channel, sdlVol)
 *   sdlVol: 0 (silent) to MIX_MAX_VOLUME=128 (full)
 * ========================================================================= */
void SDL_Audio_SetChannelVolume(int channel, int millibels)
{
    int sdlVol = SDL_Audio_MillibelsToSDLVolume(millibels);

    /* WIN32: IDirectSoundBuffer::SetVolume(millibels) */
    /* LINUX: Mix_Volume(channel, sdlVol) */
    Mix_Volume(channel, sdlVol);
}

/* =========================================================================
 * SDL_Audio_FreeSFX  —  replaces IDirectSoundBuffer::Release
 *
 * Frees a Mix_Chunk loaded by SDL_Audio_LoadSoundEffect.
 * Must only be called when no channel is actively playing the chunk.
 *
 * WIN32: IDirectSoundBuffer::Release()  — COM reference drop; frees buffer
 * LINUX: Mix_FreeChunk(chunk)
 * ========================================================================= */
void SDL_Audio_FreeSFX(Mix_Chunk *chunk)
{
    if (chunk == NULL) return;

    /* WIN32: IDirectSoundBuffer::Release() */
    /* LINUX: Mix_FreeChunk(chunk) */
    Mix_FreeChunk(chunk);
}

/* =========================================================================
 * SDL_Audio_PlayBackgroundMusic  —  replaces PlaySoundA (SND_FILENAME|SND_ASYNC)
 *
 * Loads and plays a WAV/OGG file as looping background music.
 * Called by CAudioStateMachine_SetState (FUN_004208f0) for state 7 (in-game).
 *
 * Original call from audio.c:
 *   wsprintfA(musicPath, "%svideo\\music.wav", &DAT_004a99c8);
 *   PlaySoundA(musicPath, NULL, SND_FILENAME | SND_ASYNC);
 *
 * WIN32: PlaySoundA(path, NULL, SND_FILENAME | SND_ASYNC)
 *   Loads and plays the WAV file asynchronously; previous sound is purged.
 * LINUX: Mix_LoadMUS(path) + Mix_PlayMusic(music, -1)
 *   -1 = loop forever; matches the background ambience intent.
 * ========================================================================= */
int SDL_Audio_PlayBackgroundMusic(const char *wavPath)
{
    Mix_Music *newMusic;

    if (!g_audioInitialised) return 0;

    /* Stop and free any currently playing background music.
     * WIN32: PlaySoundA automatically replaces the previous async sound. */
    if (g_bgmMusic != NULL) {
        /* WIN32: implicit; PlaySoundA replaces previous call */
        /* LINUX: must explicitly halt before freeing */
        Mix_HaltMusic();
        Mix_FreeMusic(g_bgmMusic);
        g_bgmMusic = NULL;
    }

    /* WIN32: PlaySoundA loads from disk on each call.
     * LINUX: Mix_LoadMUS supports WAV, OGG, MP3 (depends on build flags). */
    newMusic = Mix_LoadMUS(wavPath);
    if (newMusic == NULL) {
        fprintf(stderr, "SDL_Audio_PlayBackgroundMusic: Mix_LoadMUS('%s'): %s\n",
                wavPath, Mix_GetError());
        return 0;
    }
    g_bgmMusic = newMusic;

    /* WIN32: SND_ASYNC means non-blocking; the game loop continues.
     *        SND_FILENAME means interpret the first arg as a path.
     * LINUX: Mix_PlayMusic is always async (runs on SDL audio thread).
     *        -1 = loop count → infinite. */
    if (Mix_PlayMusic(g_bgmMusic, -1) < 0) {
        fprintf(stderr, "SDL_Audio_PlayBackgroundMusic: Mix_PlayMusic: %s\n",
                Mix_GetError());
        Mix_FreeMusic(g_bgmMusic);
        g_bgmMusic = NULL;
        return 0;
    }

    return 1;
}

/* =========================================================================
 * SDL_Audio_StopBackgroundMusic  —  replaces PlaySoundA(NULL, NULL, 0)
 *
 * Stops any currently playing background music (called on state transitions
 * to menu, paused, or video modes).
 *
 * WIN32: PlaySoundA(NULL, NULL, 0)
 *   Passing NULL as the sound name with no flags stops the current async sound.
 *   Also called with SND_PURGE to explicitly cancel.
 * LINUX: Mix_HaltMusic() + Mix_FreeMusic(g_bgmMusic)
 * ========================================================================= */
void SDL_Audio_StopBackgroundMusic(void)
{
    /* WIN32: PlaySoundA(NULL, NULL, 0) */
    /* LINUX: Mix_HaltMusic() */
    Mix_HaltMusic();

    if (g_bgmMusic != NULL) {
        /* WIN32: buffer lifetime managed internally by WINMM */
        /* LINUX: Mix_Music* must be freed by the caller */
        Mix_FreeMusic(g_bgmMusic);
        g_bgmMusic = NULL;
    }
}

/* =========================================================================
 * SDL_Audio_SetMusicVolume  —  replaces MCI_SETAUDIO volume command
 *
 * Sets background music volume.  Called from CMciVideoPlayer_PlayInWindow
 * (FUN_00454380) step 7 via:
 *   mciSendCommandA(mciId, MCI_SETAUDIO=0x806, MCI_SETAUDIO_VOLUME, &params)
 *
 * WIN32: mciSendCommandA with MCI_SETAUDIO_VOLUME flag
 *   Volume range: 0–1000 (MCI convention; 1000 = full)
 * LINUX: Mix_VolumeMusic(sdlVol)
 *   Volume range: 0–MIX_MAX_VOLUME (128)
 *   Conversion: sdlVol = mciVol * MIX_MAX_VOLUME / 1000
 * ========================================================================= */
void SDL_Audio_SetMusicVolume(int mciVolume)
{
    int sdlVol;

    /* WIN32: mciSendCommandA(id, MCI_SETAUDIO, MCI_SETAUDIO_VOLUME, &params)
     *   params.dwVolume = mciVolume (0..1000) */
    /* LINUX: Mix_VolumeMusic takes 0..MIX_MAX_VOLUME */
    sdlVol = mciVolume * MIX_MAX_VOLUME / 1000;
    if (sdlVol < 0)              sdlVol = 0;
    if (sdlVol > MIX_MAX_VOLUME) sdlVol = MIX_MAX_VOLUME;

    Mix_VolumeMusic(sdlVol);
}

/* =========================================================================
 * SDL_Audio_VideoPlay  —  replaces CMciVideoPlayer_PlayInWindow (FUN_00454380)
 *
 * Plays an AVI video file (intro videos: IgSpin.avi, legoSpin.avi).
 * On Linux this is implemented via GStreamer playbin if LOCO_ENABLE_VIDEO_GST
 * is defined; otherwise it is a no-op stub (intro videos are optional for
 * gameplay).
 *
 * WIN32 sequence:
 *   1. MCIWndRegisterClass()                          — register MCIWnd class
 *   2. CreateWindowExA("MCIWndClass", ...)            — create MCIWnd child
 *   3. SendMessageA(hWnd, MCIWNDM_OPEN=0x499, 0, path) — open media file
 *   4. SendMessageA(hWnd, MCIWNDM_PUT_DEST=0x48F, 0, &rect) — set display rect
 *   5. SendMessageA(hWnd, MCIWNDM_GETDEVICEID=0x464, 0, 0) — get MCI device ID
 *   6. mciSendCommandA(id, MCI_SETAUDIO=0x806, 0x1000001, &vol) — set volume
 *
 * LINUX:
 *   gst_init(NULL, NULL)
 *   gst_parse_launch("playbin uri=file:///path/to/video.avi")
 *   g_object_set(pipeline, "volume", (gdouble)vol/1000.0, NULL)
 *   gst_element_set_state(pipeline, GST_STATE_PLAYING)
 *
 * Parameters:
 *   filePath  — absolute path to the AVI file
 *   volume    — MCI volume 0..1000 (stored at CMciVideoPlayer::this+0x18)
 * ========================================================================= */
void SDL_Audio_VideoPlay(const char *filePath, int volume)
{
#ifdef LOCO_ENABLE_VIDEO_GST
    char uri[2048];

    /* Stop any existing pipeline */
    if (g_gstPipeline != NULL) {
        /* WIN32: SendMessageA(hWnd, MCIWNDM_STOP=0x804, 0, 0) */
        /* LINUX: gst_element_set_state → GST_STATE_NULL stops playback */
        gst_element_set_state(g_gstPipeline, GST_STATE_NULL);
        /* WIN32: SendMessageA(hWnd, WM_CLOSE=0x10, 0, 0) */
        /* LINUX: gst_object_unref frees all pipeline resources */
        gst_object_unref(g_gstPipeline);
        g_gstPipeline = NULL;
    }

    /* WIN32: MCIWndRegisterClass() — must be called before CreateWindowExA */
    /* LINUX: gst_init ensures GStreamer runtime is ready */
    gst_init(NULL, NULL);

    /* Build a file:// URI from the filesystem path */
    if (filePath[0] == '/')
        snprintf(uri, sizeof(uri), "file://%s", filePath);
    else
        snprintf(uri, sizeof(uri), "file://%s/%s", SDL_GetBasePath(), filePath);

    /* WIN32: CreateWindowExA("MCIWndClass", ...) + SendMessageA(MCIWNDM_OPEN)
     * LINUX: gst_parse_launch creates a complete playbin pipeline in one call */
    {
        char launch[2200];
        snprintf(launch, sizeof(launch), "playbin uri=%s", uri);
        g_gstPipeline = gst_parse_launch(launch, NULL);
    }

    if (g_gstPipeline == NULL) {
        fprintf(stderr, "SDL_Audio_VideoPlay: gst_parse_launch failed for '%s'\n",
                filePath);
        return;
    }

    /* WIN32: mciSendCommandA(id, MCI_SETAUDIO=0x806, MCI_SETAUDIO_VOLUME, &params)
     * LINUX: g_object_set(pipeline, "volume", linear_scale, NULL)
     *   GStreamer uses linear volume 0.0–1.0 (not dB) */
    {
        gdouble gstVol = (gdouble)volume / 1000.0;
        if (gstVol < 0.0) gstVol = 0.0;
        if (gstVol > 1.0) gstVol = 1.0;
        g_object_set(G_OBJECT(g_gstPipeline), "volume", gstVol, NULL);
    }

    /* WIN32: MCIWnd automatically starts playback after MCIWNDM_OPEN.
     * LINUX: Must explicitly set state to PLAYING. */
    gst_element_set_state(g_gstPipeline, GST_STATE_PLAYING);
#else
    /* Stub: video not enabled.  Intro videos are optional for gameplay. */
    (void)filePath;
    (void)volume;
    fprintf(stderr, "SDL_Audio_VideoPlay: video playback stubbed out "
                    "(define LOCO_ENABLE_VIDEO_GST and link GStreamer to enable)\n");
#endif
}

/* =========================================================================
 * SDL_Audio_VideoStop  —  replaces CMciVideoPlayer_Stop (FUN_004544a0)
 *
 * Stops AVI video playback without freeing the pipeline (non-destructive
 * stop, mirrors SendMessageA MCIWNDM_STOP + WM_CLOSE).
 *
 * WIN32: SendMessageA(hWnd, MCIWNDM_STOP=0x804, 0, 0)
 *        SendMessageA(hWnd, WM_CLOSE=0x10,  0, 0)
 *        *(HWND*)(pPlayer+4) = NULL
 * LINUX: gst_element_set_state(GST_STATE_NULL) + gst_object_unref
 * ========================================================================= */
void SDL_Audio_VideoStop(void)
{
#ifdef LOCO_ENABLE_VIDEO_GST
    if (g_gstPipeline != NULL) {
        /* WIN32: SendMessageA(hMCIWnd, 0x804, 0, 0) — MCIWNDM_STOP */
        /* LINUX: GST_STATE_NULL stops and resets the pipeline */
        gst_element_set_state(g_gstPipeline, GST_STATE_NULL);
        /* WIN32: SendMessageA(hMCIWnd, 0x10, 0, 0)  — WM_CLOSE */
        /* LINUX: gst_object_unref destroys and frees all resources */
        gst_object_unref(g_gstPipeline);
        g_gstPipeline = NULL;
    }
#endif
}

/* =========================================================================
 * SDL_Audio_EnumerateDevices  —  replaces DirectSoundEnumerate (Ordinal_2)
 *
 * Lists available audio output device names.  In the original, this is used
 * during sound device selection.  On Linux we log them for diagnostics.
 *
 * WIN32: DirectSoundEnumerate(callback, userdata)
 *   Ordinal_2 of DSOUND.DLL; calls the callback for each DirectSound device.
 * LINUX: SDL_GetNumAudioDevices(0) + SDL_GetAudioDeviceName(i, 0)
 *   0 = output devices (not capture).
 * ========================================================================= */
void SDL_Audio_EnumerateDevices(void)
{
    int count, i;

    /* WIN32: DirectSoundEnumerate(EnumCallback, NULL) — Ordinal_2 */
    /* LINUX: query SDL2 audio device list */
    count = SDL_GetNumAudioDevices(0 /* output */);

    fprintf(stderr, "SDL_Audio_EnumerateDevices: %d output device(s) available\n",
            count);
    for (i = 0; i < count; i++) {
        /* WIN32: EnumCallback receives GUID* and device description string */
        /* LINUX: SDL_GetAudioDeviceName returns the device name */
        const char *name = SDL_GetAudioDeviceName(i, 0);
        fprintf(stderr, "  [%d] %s\n", i, name ? name : "(unknown)");
    }
}
