/**
 * sdl3_timer_stress_test.cpp — SDL3 timer safety regression test.
 *
 * Verifies that rapid SetTimer/KillTimer cycles never cause a use-after-free
 * crash in sdl_timer_callback, even under SDL timer-thread contention.
 *
 * The original bug: KillTimer called SDL_RemoveTimer then g_timers.erase(),
 * freeing the TimerInfo while an in-flight callback on the SDL timer thread
 * still held a pointer to it.  The callback would read freed memory, get a
 * garbage function pointer, and crash.
 */
#include "sdl3_window.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace {

bool fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}

/* Dummy timer callback — uses the exact TIMERPROC signature */
void dummy_timer_proc(HWND hWnd, UINT msg, uintptr_t id, DWORD ticks) {
    (void)hWnd; (void)msg; (void)id; (void)ticks;
}

}  // namespace

int main() {
    if (SDL3_WindowInit("timer-stress-test", 200, 100) != 0)
        return fail(SDL_GetError()) ? 0 : 1;

    constexpr int kRounds = 500;
    constexpr int kTimersPerRound = 8;
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    for (int round = 0; round < kRounds; ++round) {
        /* Create timers at various intervals (some very short to increase
         * contention with the SDL timer thread) */
        uintptr_t ids[kTimersPerRound];
        for (int i = 0; i < kTimersPerRound; ++i) {
            UINT interval = static_cast<UINT>(1 + (std::rand() % 4));  // 1-4 ms
            uintptr_t id = 0x50 + static_cast<uintptr_t>(i);           // unique IDs
            ids[i] = SetTimer(nullptr, id, interval, dummy_timer_proc);
        }

        /* Kill a random subset immediately (shortest possible hold time,
         * maximises chance of racing with the timer thread) */
        for (int i = 0; i < kTimersPerRound; ++i) {
            if (std::rand() % 2) {
                KillTimer(nullptr, ids[i]);
            }
        }

        /* Kill the rest after a tiny delay */
        SDL_Delay(1);
        for (int i = 0; i < kTimersPerRound; ++i) {
            KillTimer(nullptr, ids[i]);
        }
    }

    /* Let any pending callbacks drain */
    SDL_Delay(100);
    SDL3_WindowQuit();
    std::puts("PASS: timer stress test completed without crash");
    return 0;
}
