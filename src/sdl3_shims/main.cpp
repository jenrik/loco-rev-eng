/**
 * main.cpp — SDL3-native entry point for Lego Loco
 */

#include "sdl3_window.h"
#include "../decompiled_cpp/core/CGWND.h"
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
    TRACE("CGWND constructed at %p", (void*)cgwnd);

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

    TRACE("Transitioning to mode 2 (main-menu UI)...");
    CGWND_SetMode(2);
    TRACE("Mode 2 set");

    TRACE("Entering CGWND_PumpMessages (main loop)...");
    CGWND_PumpMessages(cgwnd, 0);
    TRACE("PumpMessages returned — shutting down");

    cgwnd->~CGWND();
    GLOBAL_free(cgwnd);
    CoUninitialize();
    SDL3_WindowQuit();
    TRACE("Clean exit");
    return 0;
}
