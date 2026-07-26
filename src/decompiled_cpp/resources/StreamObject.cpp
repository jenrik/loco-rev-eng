/**
 * StreamObject.cpp — Stream I/O object synchronization helpers
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "StreamObject.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {
    void __stdcall WNDPROC_EnterCriticalSection(void* cs);  /* 0x464D90 — EnterCriticalSection */
    void __stdcall WNDPROC_LeaveCriticalSection(void* cs);  /* 0x464DA0 — LeaveCriticalSection */
}

/* ================================================================== */
/* StreamObject_Lock                                                   */
/* Address: 0x410240                                                   */
/*                                                                     */
/* Called by: CGWND_AboutDialog_LoadCredits (0x4101AA)                 */
/*                                                                     */
/* Conditionally enters the stream's CRITICAL_SECTION if the sync flag */
/* at +0x34 is negative (signalling active synchronization state).      */
/* ================================================================== */
void __cdecl StreamObject_Lock(void* stream)
{
    /* Check sync flag at +0x34 */
    if (*(int32_t*)((uint8_t*)stream + 0x34) < 0) {
        WNDPROC_EnterCriticalSection((uint8_t*)stream + 0x38);  /* CRITICAL_SECTION at +0x38 */
    }
}

/* ================================================================== */
/* StreamObject_Unlock                                                 */
/* Address: 0x410260                                                   */
/*                                                                     */
/* Called by: CGWND_AboutDialog_LoadCredits (0x4101C5)                 */
/*                                                                     */
/* Conditionally leaves the stream's CRITICAL_SECTION if the sync flag */
/* at +0x34 is negative.                                                */
/* ================================================================== */
void __cdecl StreamObject_Unlock(void* stream)
{
    /* Check sync flag at +0x34 */
    if (*(int32_t*)((uint8_t*)stream + 0x34) < 0) {
        WNDPROC_LeaveCriticalSection((uint8_t*)stream + 0x38);  /* CRITICAL_SECTION at +0x38 */
    }
}
