/**
 * WIN32Thread.h — Thread-marshalling helper object used by the network
 * subsystem to spawn/track the worker thread that pumps TrainSubsystem
 * network messages.
 *
 * Lego Loco (loco.exe, 1998, MSVC x86) — reverse engineered via Ghidra.
 *
 * PROGRESS.md tracks this under the "win32_network.c removed" gap: the
 * original DirectPlay/thread marshalling for the network worker thread was
 * never carried over and was masked by silent no-op stubs. This header
 * covers the thread-lifecycle slice of that gap:
 *
 *   WIN32_CreateThread     0x461610
 *   WIN32_WaitForThread    0x461690
 *   WIN32_CloseThreadHandle 0x461740
 *   WIN32_QueueAsyncTask   0x461790
 *
 * Three sibling functions in the same contiguous block are NOT implemented
 * here (out of scope for this pass; also currently backed only by no-op
 * stubs in shared/link_stubs.cpp / shared/defsym_stubs.cpp, despite
 * appearing to be an already-finished neighbor at a glance):
 *
 *   WIN32_TerminateThread  0x461640  — scalar deleting destructor (vtable
 *                                      slot [0]): closes the handle, then
 *                                      frees `this` if (flags&1).
 *   WIN32_ResumeThread     0x4616C0  — WaitForSingleObject(handle,0) poll,
 *                                      then SetThreadPriority if still
 *                                      alive.
 *   WIN32_GetThreadResult  0x461710  — WaitForSingleObject(handle,0) poll;
 *                                      returns 0x101 if still running, else
 *                                      the low byte of the wait result.
 *
 * NOTE ON NAMING: WIN32_WaitForThread and WIN32_CloseThreadHandle are
 * label mismatches against the decompiled behavior (confirmed via
 * disassembly, not just decompiler pseudocode) — neither name matches what
 * the bytes do:
 *   - WIN32_WaitForThread (0x461690) never calls WaitForSingleObject; it
 *     unconditionally closes the handle (if set) and clears state. It reads
 *     like a partial-reset/cleanup helper, not a wait.
 *   - WIN32_CloseThreadHandle (0x461740) never calls CloseHandle; it polls
 *     WaitForSingleObject(handle,0) and, if the thread is still alive AND
 *     the suspended flag is set, calls ResumeThread and clears the flag.
 * The names are kept as-is (they are the identifiers used by the task and
 * by any future caller wiring) rather than invented; the mismatch is
 * documented instead of silently "fixed" by renaming, since no additional
 * evidence (e.g. an xref-derived caller context) supports a better name.
 *
 * LAYOUT: WIN32Thread is a 0x41C-byte MSVC object whose single-entry
 * vtable at offset 0 is not reconstructed here (see WIN32_TerminateThread
 * above) — nothing in this file dispatches through it. Field offsets below
 * are evidenced by this-relative accesses across all 7 sibling functions in
 * the 0x461610-0x4617F0 block plus the allocation site
 * (EditWindow::netPanelInit / EditWindow_InitNetworkPanel, 0x422820, which
 * allocates exactly sizeof(WIN32Thread) == 0x41C bytes).
 */

#ifndef LOCO_NETWORK_WIN32THREAD_H
#define LOCO_NETWORK_WIN32THREAD_H

#include <cstdint>

/* ================================================================== */
/* WIN32Thread — original x86 layout (0x41C bytes)                     */
/* ================================================================== */
struct WIN32Thread {
    /* +0x000 — MSVC vtable pointer; slot[0] is the scalar deleting
     * destructor (WIN32_TerminateThread, 0x461640). Not reconstructed:
     * left untouched by every function in this file (see file comment). */
    void* vtable;

    /* +0x004 — last error/status code, written by WIN32_QueueAsyncTask's
     * "already active" path (-21) only; the "CreateThread failed" path
     * (-22) does NOT write here (see WIN32_QueueAsyncTask). */
    int32_t lastError;

    /* +0x008 — 0x400-byte inline buffer, reused as the strcpy target for
     * both WIN32_QueueAsyncTask diagnostic strings ("Thread already
     * active" / "CreateThread failed."). Not subdivided further; no
     * evidence distinguishes sub-fields within it. */
    char message[0x400];

    /* +0x408 — thread id, passed as the LPDWORD out-param to
     * CreateThread/_beginthreadex. */
    uint32_t threadId;

    /* +0x40C — HANDLE to the worker thread, or null if none is running. */
    void* handle;

    /* +0x410 — callback invoked by the worker-thread trampoline. */
    void* callback;

    /* +0x414 — argument passed to `callback`. */
    void* param;

    /* +0x418 — nonzero if the thread was created suspended and still needs
     * a Resume (consumed by WIN32_CloseThreadHandle). */
    uint8_t suspended;
};

#ifdef _WIN32
/* Exact x86 layout parity is a Windows-reconstruction concern, not a
 * host-build goal (CLAUDE.md): on the native 64-bit host, void* is 8
 * bytes, so this struct is intentionally larger than 0x41C there. Only
 * the 32-bit Windows target is asserted against the original layout. */
static_assert(sizeof(WIN32Thread) == 0x41C,
              "WIN32Thread must match the original 0x41C-byte x86 layout");
#endif

/* ================================================================== */
/* Function declarations                                                */
/* ================================================================== */

/** WIN32_CreateThread — placement-constructs a WIN32Thread in
 *  caller-allocated storage (typically operator_new(sizeof(WIN32Thread)))
 *  and returns it. Zero-initializes the handle/callback/param/suspended
 *  fields; does not create an OS thread (that happens in
 *  WIN32_QueueAsyncTask).
 *  Address: 0x461610 */
void* WIN32_CreateThread(void* buffer);

/** WIN32_WaitForThread — see NOTE ON NAMING above: does not wait. If a
 *  handle is present, closes it and clears handle+suspended.
 *  Address: 0x461690 */
void WIN32_WaitForThread(void* thread);

/** WIN32_CloseThreadHandle — see NOTE ON NAMING above: does not close a
 *  handle. Polls the thread; if still running AND `suspended` is set,
 *  resumes it and clears the flag.
 *  Address: 0x461740 */
void WIN32_CloseThreadHandle(void* thread);

/** WIN32_QueueAsyncTask — if a thread is already active, records error
 *  -21 and returns it. Otherwise closes any stale handle, stores
 *  callback/param, and spawns a worker thread (via _beginthreadex on the
 *  original path) whose trampoline calls callback(param). Returns 1 on
 *  success, or -22 (without recording it in lastError) if thread creation
 *  fails.
 *  Address: 0x461790 */
int WIN32_QueueAsyncTask(void* thread, void* callback, void* param);

#endif /* LOCO_NETWORK_WIN32THREAD_H */
