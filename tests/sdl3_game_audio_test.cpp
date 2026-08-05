// Status: VALIDATED
/** Host audio regression for mode-2 preload and mode-10 resource playback. */
#include "resource_manager_sdl3.h"
#include "sdl3_game_audio.h"

#include <SDL3/SDL.h>
#include <cstdio>

int main() {
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "FAIL: %s\n", SDL_GetError());
        return 1;
    }
    if (!loco::assets::host_resource_manager().initialize("lego-loco-unpacked", nullptr)) {
        std::fputs("FAIL: could not open game assets\n", stderr);
        SDL_Quit();
        return 1;
    }

    // EditWindow::show @ 0x420780 requests this menu resource in mode 2.
    if (!SDL3_GameAudioPreloadResource(0x5015)) {
        std::fputs("FAIL: mode-2 resource 0x5015 did not preload\n", stderr);
        return 1;
    }
    // CGWND_SetMode(10) @ 0x40824C requests this exit sound.
    if (!SDL3_GameAudioPlayResource(0x5026)) {
        std::fprintf(stderr, "FAIL: mode-10 resource 0x5026 did not queue: %s\n", SDL_GetError());
        return 1;
    }
    SDL3_GameAudioStopAll();
    loco::assets::host_resource_manager().reset();
    SDL_Quit();
    std::puts("PASS: host mode-2 preload and mode-10 exit sound are backed by archive WAVs");
    return 0;
}
