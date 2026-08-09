/**
 * main.cpp — SDL3-native entry point for Lego Loco
 */

#include "sdl3_window.h"
#include "sdl3_game_audio.h"
#include "host_test_events.h"
#include "sdl3_intro_video.h"
#include "core/CGWND.h"
#include <cstdint>
#include <cstdio>
#include <new>

extern void* operator_new(size_t size);
extern void  GLOBAL_free(void* ptr);
extern void  CGWND_PumpMessages(void*, uint8_t filter);
extern int   DDRAW_Init(void);
extern "C" int  GameLoop_Setup(void* cgwnd);
extern void CGWND_SetMode(int mode);
extern void* g_main_window;

#define TRACE(fmt, ...) do { std::fprintf(stderr, "[TRACE] " fmt "\n", ##__VA_ARGS__); std::fflush(stderr); } while(0)

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    setvbuf(stderr, NULL, _IONBF, 0);
    loco::host_test::emit_process_started();
    TRACE("main() entered");
    TRACE("Lego Loco — SDL3 Native Port");

    TRACE("Calling SDL3_WindowInit...");
    if (SDL3_WindowInit("LEGO LOCO", 800, 600) != 0) {
        std::fprintf(stderr, "FATAL: SDL3_WindowInit failed\n");
        return 1;
    }
    TRACE("SDL3_WindowInit OK");

    TRACE("Allocating CGWND...");
    void* cgwnd_mem = operator_new(sizeof(CGWND));  /* was 0x28 (32-bit) */
    TRACE("CGWND memory: %p", cgwnd_mem);
    if (!cgwnd_mem) {
        std::fprintf(stderr, "FATAL: CGWND allocation failed\n");
        SDL3_WindowQuit();
        return 1;
    }

    TRACE("Constructing CGWND...");
    CGWND* cgwnd = new (cgwnd_mem) CGWND(nullptr);
    g_main_window = cgwnd;
    TRACE("CGWND constructed at %p", static_cast<void*>(cgwnd));

    TRACE("Calling CoInitializeEx...");
    CoInitializeEx(nullptr, 0);
    
    TRACE("Calling CGWND_InstallPathInit...");
    CGWND_InstallPathInit();
    TRACE("InstallPathInit done");

    TRACE("Calling ShowMainMenu...");
    cgwnd->ShowMainMenu();
    TRACE("ShowMainMenu done");

    TRACE("Calling GameLoop_Setup...");
    int setup_result = GameLoop_Setup(cgwnd);
    TRACE("GameLoop_Setup returned %d", setup_result);
    if (setup_result != 0) {
        std::fprintf(stderr, "WARNING: GameLoop_Setup failed with %d, continuing anyway\n", setup_result);
    }

#ifndef _WIN32
    // Host-only deviation: the original MCIWnd player (RESDATA_CtorBase,
    // 0x454380) is Win32-specific. Decode the three shipped launch AVIs via
    // the SDL/GStreamer adapter before exposing the reconstructed main menu.
    if (loco::intro::startLaunchSequence()) {
        TRACE("Playing launch intro sequence before mode 2");
    } else {
        TRACE("Transitioning to mode 2 (main-menu UI)...");
        CGWND_SetMode(2);
        TRACE("Mode 2 set");
    }
#else
    TRACE("Transitioning to mode 2 (main-menu UI)...");
    CGWND_SetMode(2);
    TRACE("Mode 2 set");
#endif

    TRACE("Entering CGWND_PumpMessages (main loop)...");
    CGWND_PumpMessages(cgwnd, 0);
    TRACE("PumpMessages returned — shutting down");

    // CGWND_SetMode(10) queues the exit sweep 0x5026 then returns.
    // The original posts WM_CLOSE immediately and DirectSound hardware
    // buffers outlive the process.  On SDL3 we must drain the stream
    // before SDL_Quit tears down the audio device.
    extern int g_game_mode;
    if (g_game_mode == 10) {
        TRACE("Mode 10 exit: hiding window, draining audio...");
        SDL_HideWindow(SDL3_GetWindow());
        for (int n = 0; n < 300 && SDL3_GameAudioPump(); n++) {
            SDL_Delay(10);
        }
        SDL_Delay(150);
        TRACE("Exit audio drained");
    }

    cgwnd->~CGWND();
    GLOBAL_free(cgwnd);
    CoUninitialize();
    SDL3_GameAudioStopAll();
#ifndef _WIN32
    loco::intro::stop();
#endif
    SDL3_WindowQuit();
    loco::host_test::emit_clean_shutdown();
    TRACE("Clean exit");
    return 0;
}
