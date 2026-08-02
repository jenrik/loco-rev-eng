/**
 * sdl3_town_mode3.h — Host bootstrap for mode-3 town gameplay objects
 * Lego Loco (loco.exe, 1998, MSVC x86) — host-only deviation (#ifndef _WIN32)
 */
#ifndef SDL3_TOWN_MODE3_H
#define SDL3_TOWN_MODE3_H

#ifndef _WIN32
namespace loco {
namespace host {
    /** Construct GameView and DDRAW_Building singletons, assign to the
     *  legacy void* globals.  Must be called after ResourceManager_Init
     *  and before the first mode-3 frame tick. */
    /* True after both unconditional GameLoop_FrameUpdate dependencies
     * have host backing storage. */
    bool Mode3FrameDependenciesReady();
    void BootstrapTownMode3Objects();
} // namespace host
} // namespace loco
#endif /* _WIN32 */

#endif /* SDL3_TOWN_MODE3_H */
