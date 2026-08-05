/**
 * WIN32Thread.cpp — WIN32_CreateThread / WIN32_WaitForThread /
 * WIN32_CloseThreadHandle / WIN32_QueueAsyncTask
 *
 * Lego Loco (loco.exe, 1998, MSVC x86) — reverse engineered via Ghidra.
 * See WIN32Thread.h for the object layout and naming caveats.
 *
 * Three of the four functions here (CreateThread, WaitForThread,
 * CloseThreadHandle) never touch the OS thread handle unless it is
 * already non-null, and nothing on the SDL host ever sets it (see
 * WIN32_QueueAsyncTask below), so they are platform-portable as written:
 * on the host build their handle-guarded branches are simply unreachable
 * at runtime. WIN32_QueueAsyncTask is the one function that genuinely
 * diverges — the original spawns an OS thread; the SDL host is
 * single-threaded and already has a real, tested substitute in
 * core/HostMode3Bootstrap.cpp (the pending-async-task pump exercised by
 * the GUI integration suite). This file's WIN32_QueueAsyncTask is
 * therefore compiled only under #ifdef _WIN32, so the two definitions
 * never collide: exactly one exists per target.
 */

// Status: TRANSCRIBED
// (Not VALIDATED: the message-buffer copy uses std::strncpy with NUL
// padding rather than the assembly's exact strcpy-length REP MOVS; the
// worker-thread trampoline (0x461890) was reconstructed by inference,
// never disassembled as its own function; and the added null-pointer
// guards are undocumented-in-assembly deviations. See file/header
// comments for each.)

#include "WIN32Thread.h"

#include <cstring>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#endif

/* CloseHandle/WaitForSingleObject/ResumeThread/WAIT_TIMEOUT: declared
 * ourselves (extern "C") rather than relying on <windows.h> to provide
 * them under _WIN32 — this project's stubs/windows.h (force-included via
 * compat.h) collides with the real MinGW <windows.h> under the typecheck
 * cross-build (pre-existing conflict, unrelated to this file), so
 * <windows.h> is not reliably fully parsed there. On the host build these
 * are OS-thread APIs with no SDL3 path yet (the single-threaded pump
 * never creates a real OS thread — see WIN32_QueueAsyncTask below), so
 * the handle-guarded branches in WIN32_WaitForThread/WIN32_CloseThreadHandle
 * never execute at runtime; minimal linkable stubs live beside each other
 * in shared/link_stubs.cpp's extern "C" block (alongside the existing
 * CloseHandle stub there). */
#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT 0x102u
#endif
extern "C" {
int   CloseHandle(void* handle);
unsigned int WaitForSingleObject(void* handle, unsigned int timeoutMs);
unsigned int ResumeThread(void* handle);
}

/* ================================================================== */
/* WIN32_CreateThread — Address: 0x461610                              */
/* ================================================================== */
void* WIN32_CreateThread(void* buffer)
{
    if (buffer == nullptr) return nullptr;

    WIN32Thread* thread = static_cast<WIN32Thread*>(buffer);
    /* vtable (+0x000) deliberately left untouched — see WIN32Thread.h. */
    thread->threadId  = 0;
    thread->handle    = nullptr;
    thread->callback  = nullptr;
    thread->param     = nullptr;
    thread->suspended = 0;
    return thread;
}

/* ================================================================== */
/* WIN32_WaitForThread — Address: 0x461690 (see NOTE ON NAMING)         */
/* ================================================================== */
void WIN32_WaitForThread(void* threadPtr)
{
    if (threadPtr == nullptr) return;
    WIN32Thread* thread = static_cast<WIN32Thread*>(threadPtr);

    if (thread->handle != nullptr) {
        CloseHandle(thread->handle);
        thread->handle    = nullptr;
        thread->suspended = 0;
    }
}

/* ================================================================== */
/* WIN32_CloseThreadHandle — Address: 0x461740 (see NOTE ON NAMING)     */
/* ================================================================== */
void WIN32_CloseThreadHandle(void* threadPtr)
{
    if (threadPtr == nullptr) return;
    WIN32Thread* thread = static_cast<WIN32Thread*>(threadPtr);

    bool stillRunning = false;
    if (thread->handle != nullptr) {
        unsigned int waitResult = WaitForSingleObject(thread->handle, 0);
        if (waitResult == WAIT_TIMEOUT) {
            stillRunning = true;
        } else {
            thread->suspended = 0;
        }
    }

    if (stillRunning && thread->suspended != 0) {
        ResumeThread(thread->handle);
        thread->suspended = 0;
    }
}

/* ================================================================== */
/* WIN32_QueueAsyncTask — Address: 0x461790                             */
/*                                                                      */
/* Host substitute lives in core/HostMode3Bootstrap.cpp (#ifndef        */
/* _WIN32) — this is the assembly-faithful original path only.          */
/* ================================================================== */
#ifdef _WIN32

/* Reconstruction of the inline worker-thread trampoline at 0x461890
 * (pushed as the _beginthreadex start routine at 0x46182D; not itself a
 * Ghidra-recognized function, so not independently address-annotated).
 * It reads the callback/param this-relative fields and invokes
 * callback(param). */
static unsigned int __stdcall WIN32Thread_TrampolineProc(void* threadPtr)
{
    WIN32Thread* thread = static_cast<WIN32Thread*>(threadPtr);
    using Callback = void (*)(void*);
    reinterpret_cast<Callback>(thread->callback)(thread->param);
    return 0;
}

int WIN32_QueueAsyncTask(void* threadPtr, void* callback, void* param)
{
    WIN32Thread* thread = static_cast<WIN32Thread*>(threadPtr);

    bool stillRunning = false;
    if (thread->handle != nullptr) {
        DWORD waitResult = WaitForSingleObject(thread->handle, 0);
        if (waitResult == WAIT_TIMEOUT) {
            stillRunning = true;
        } else {
            thread->suspended = 0;
        }
    }

    if (stillRunning) {
        std::strncpy(thread->message, "Thread already active",
                     sizeof(thread->message) - 1);
        thread->message[sizeof(thread->message) - 1] = '\0';
        thread->lastError = -21;
        return -21;
    }

    if (thread->handle != nullptr) {
        CloseHandle(thread->handle);
        thread->handle    = nullptr;
        thread->suspended = 0;
    }

    thread->param    = param;
    thread->callback = callback;

    uintptr_t handle = _beginthreadex(
        /* security */ nullptr,
        /* stack size */ 0,
        reinterpret_cast<unsigned int (__stdcall*)(void*)>(WIN32Thread_TrampolineProc),
        thread,
        /* creation flags */ 0,
        reinterpret_cast<unsigned int*>(&thread->threadId));
    thread->handle = reinterpret_cast<void*>(handle);

    if (thread->handle != nullptr) {
        return 1;
    }

    /* Note the asymmetry with the "already active" path above: this
     * failure path writes the diagnostic message but does NOT record
     * -22 into thread->lastError before returning it (matches the
     * decompiled/disassembled control flow exactly). */
    std::strncpy(thread->message, "CreateThread failed.",
                 sizeof(thread->message) - 1);
    thread->message[sizeof(thread->message) - 1] = '\0';
    return -22;
}

#endif /* _WIN32 */
