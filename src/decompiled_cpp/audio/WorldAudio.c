/**
 * WorldAudio.c — World audio processing (C free / __thiscall functions)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * World_ProcessAudio handles in-game audio hit-testing: when the user
 * clicks on the town view, this function checks each building's
 * sub-objects against the click position. On match, it selects the
 * building in the town view.
 */

#include "../shared/types.h"

/* ================================================================== */
/* External declarations                                               */
/* ================================================================== */

extern int8_t   g_click_on_town;        /* 0x4AAD3C — click-in-town flag */
extern int32_t  g_town_view;            /* 0x4A8828 — town view instance */
extern void __thiscall Town_SelectBuilding(int32_t townView, void* building);

/* ================================================================== */
/* World_ProcessAudio — 0x44E830                                        */
/*                                                                      */
/* __thiscall on a World object (layout +0x08 = array of 4 vehicle     */
/* buckets, each at +0x10 = sub-objects, +0x0C = count).               */
/*                                                                      */
/* For each vehicle bucket, iterates sub-objects:                      */
/*   - Checks if object exists and is clickable (+9 == 1)              */
/*   - Calls vtable[2] (PtInRect / hit-test) with (audio_x, audio_y)  */
/*   - On hit, calls Town_SelectBuilding and returns 1                 */
/*                                                                      */
/* Returns: 1 if a hit was found and building selected, 0 otherwise.   */
/* ================================================================== */
uint8_t __thiscall World_ProcessAudio(void* this, int32_t audioX, int32_t audioY)
{
    int32_t* bucketPtr;
    int32_t  bucketIndex;
    uint8_t  found;

    found = 0;

    /* Don't process if not clicking on town */
    if (g_click_on_town == 0) {
        return 0;
    }

    bucketPtr = (int32_t*)((int8_t*)this + 8);
    for (bucketIndex = 4; bucketIndex != 0; bucketIndex--) {
        int32_t bucket = *bucketPtr;
        if (bucket != 0) {
            uint32_t subIndex;
            for (subIndex = 0; subIndex <= *(uint16_t*)(bucket + 0x0C); subIndex++) {
                int32_t* subObj = *(int32_t**)(bucket + 0x10 + subIndex * 4);
                if (subObj != NULL) {
                    /* Check if clickable (+9 == 1) and hit-test */
                    if ((*(int8_t*)(subObj + 9) == 1)) {
                        uint8_t hitResult = (*(uint8_t(__thiscall**)(int32_t, int32_t))
                            (*(void**)subObj + 8))(audioX, audioY);
                        if (hitResult != 0) {
                            Town_SelectBuilding(g_town_view, *(void**)(bucket + 0x10 + subIndex * 4));
                            found = 1;
                            break;
                        }
                    }
                }
                bucket = *bucketPtr;  /* Refresh (may have been modified) */
            }
        }
        bucketPtr++;
    }

    return found;
}
