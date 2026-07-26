/**
 * stream_lock.c — Thread synchronization helpers for stream objects
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions manage a CRITICAL_SECTION embedded in a stream/buffer
 * object. The sync flag at offset +0x34 determines whether the critical
 * section is active (negative = active). When active, the CRITICAL_SECTION
 * at offset +0x38 must be entered/left for thread-safe access.
 *
 * The stream object layout (subset):
 *   +0x34: sync_active (int, negative = CRITICAL_SECTION initialized)
 *   +0x38: CRITICAL_SECTION (24 bytes, Windows CRITICAL_SECTION)
 *
 * Called by: CGWND_AboutDialog_LoadCredits (0x40FEA0..0x410234)
 *            during WVE file loading via WIN32_StreamRead/Write operations.
 */

#include <stdint.h>

/* Windows CRITICAL_SECTION type */
typedef struct _CRITICAL_SECTION {
    void* DebugInfo;
    long LockCount;
    long RecursionCount;
    void* OwningThread;
    void* LockSemaphore;
    uintptr_t SpinCount;
} CRITICAL_SECTION;


/**
 * StreamObject_Lock — Enter critical section if sync is active.
 * Address: 0x410240
 *
 * Checks if the sync flag at stream+0x34 is negative (active). If so,
 * enters the CRITICAL_SECTION at stream+0x38. Otherwise no-ops.
 *
 * Previously named Game_LockMutex (misleading name — operates on a
 * stream object, not a Game instance).
 *
 * @param stream  Pointer to stream/buffer object
 */
void __cdecl StreamObject_Lock(void* stream)
{
    int* sync_active = (int*)((uint8_t*)stream + 0x34);
    if (*sync_active < 0) {
        CRITICAL_SECTION* cs = (CRITICAL_SECTION*)((uint8_t*)stream + 0x38);
        extern void __stdcall WNDPROC_EnterCriticalSection(CRITICAL_SECTION* cs);
        WNDPROC_EnterCriticalSection(cs);
    }
}


/**
 * StreamObject_Unlock — Leave critical section if sync is active.
 * Address: 0x410260
 *
 * Mirror of StreamObject_Lock. Checks sync flag, leaves CRITICAL_SECTION.
 *
 * Previously named Game_UnlockMutex.
 *
 * @param stream  Pointer to stream/buffer object
 */
void __cdecl StreamObject_Unlock(void* stream)
{
    int* sync_active = (int*)((uint8_t*)stream + 0x34);
    if (*sync_active < 0) {
        CRITICAL_SECTION* cs = (CRITICAL_SECTION*)((uint8_t*)stream + 0x38);
        extern void __stdcall WNDPROC_LeaveCriticalSection(CRITICAL_SECTION* cs);
        WNDPROC_LeaveCriticalSection(cs);
    }
}
