/**
 * StreamObject.cpp — Stream I/O object synchronization helpers
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: VALIDATED

#include "StreamObject.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {
    void __stdcall WNDPROC_EnterCriticalSection(void* cs);  /* 0x464D90 — EnterCriticalSection */
    void __stdcall WNDPROC_LeaveCriticalSection(void* cs);  /* 0x464DA0 — LeaveCriticalSection */
    void __stdcall WNDPROC_InitializeCriticalSection(void* cs);  /* 0x464D70 */
    void __stdcall WNDPROC_DeleteCriticalSection(void* cs);      /* 0x464D80 */
}

/* Global stream lock-counter/CRITICAL_SECTION pair (originally DAT_004ff180 /
 * DAT_004ff148): the first StreamObject ever constructed in the process
 * lazily initializes this shared CRITICAL_SECTION; every later one just
 * bumps the refcount. ~StreamObject() below (0x464620) is the confirmed
 * consumer: it decrements the refcount and tears the shared CS down when
 * the count reaches zero — see its own doc comment and the class header's
 * evidence trail. */
static int32_t g_streamObjectRefCount = 0;
static CRITICAL_SECTION g_streamObjectGlobalCriticalSection;

/* ================================================================== */
/* StreamObject::StreamObject — 0x464590                               */
/* ================================================================== */
StreamObject::StreamObject()
    : rdbuf(nullptr)
    , state_bits(kBadBit)
    , owns_rdbuf(0)
    , tied(nullptr)
    , format_flags(0)
    , precision(6)
    , fill(' ')
    , width(0)
    , sync_flag(-1)
{
    WNDPROC_InitializeCriticalSection(&critical_section);
    if (g_streamObjectRefCount++ == 0) {
        WNDPROC_InitializeCriticalSection(&g_streamObjectGlobalCriticalSection);
    }
}

/* ================================================================== */
/* StreamObject::~StreamObject — 0x464620 (mislabeled "WNDPROC_StreamCleanup") */
/*                                                                       */
/* Real destructor body — see the class doc comment in StreamObject.h   */
/* for the full evidence trail distinguishing this from                 */
/* WIN32_StreamDestroy's separate, non-reimplemented vptr-retagging      */
/* chain (0x463A80/0x4648E0, documented in Win32Stream.h). Reproduces    */
/* the original's exact field-write ordering: sync_flag reset first,    */
/* then the global refcount teardown, then this object's own lock, then */
/* the owned-rdbuf delete (same ownership check as AttachBuffer below),  */
/* then rdbuf/state_bits reset last.                                    */
/* ================================================================== */
StreamObject::~StreamObject()
{
    sync_flag = -1;

    if (--g_streamObjectRefCount == 0) {
        WNDPROC_DeleteCriticalSection(&g_streamObjectGlobalCriticalSection);
    }
    WNDPROC_DeleteCriticalSection(&critical_section);

    if (owns_rdbuf != 0 && rdbuf != nullptr) {
        delete rdbuf;
    }
    rdbuf = nullptr;
    state_bits = kBadBit;
}

/* ================================================================== */
/* StreamObject::AttachBuffer — 0x464680 (mislabeled "WNDPROC_StreamFlush") */
/* ================================================================== */
void StreamObject::AttachBuffer(WNDPROC_StreamBuf* newBuf)
{
    if (owns_rdbuf != 0 && rdbuf != nullptr) {
        delete rdbuf;
    }
    rdbuf = newBuf;
    if (newBuf != nullptr) {
        state_bits &= ~kBadBit;
    } else {
        state_bits |= kBadBit;
    }
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
void __cdecl StreamObject_Lock(StreamObject* stream)
{
    /* Check sync flag at +0x34 */
    if (stream->sync_flag < 0) {
        WNDPROC_EnterCriticalSection(&stream->critical_section);
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
void __cdecl StreamObject_Unlock(StreamObject* stream)
{
    /* Check sync flag at +0x34 */
    if (stream->sync_flag < 0) {
        WNDPROC_LeaveCriticalSection(&stream->critical_section);
    }
}
