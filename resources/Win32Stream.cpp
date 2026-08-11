/**
 * Win32Stream.cpp — WIN32_Stream implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly.
 *
 * See Win32Stream.h for the class/address map and the free-function
 * facades' scope (WIN32_StreamDestroy is a deliberate, evidenced loud
 * stub — see its doc comment there and in this file below).
 */

// Status: VALIDATED

#include "Win32Stream.h"

#include <cassert>
#include <cstdio>
#include <new>

extern "C" {
void __stdcall WNDPROC_EnterCriticalSection(void* cs);  /* 0x464D90 */
void __stdcall WNDPROC_LeaveCriticalSection(void* cs);  /* 0x464DA0 */
}

/* ================================================================== */
/* WIN32_Stream::WIN32_Stream (default) — 0x463890                     */
/* ================================================================== */
WIN32_Stream::WIN32_Stream()
{
    AttachBuffer(new WIN32_StreamFile());
    owns_rdbuf = 1;
}

/* ================================================================== */
/* WIN32_Stream::WIN32_Stream (path) — 0x463970                        */
/* ================================================================== */
WIN32_Stream::WIN32_Stream(const char* path, uint32_t flags, uint32_t shareMask)
    : WIN32_Stream()
{
    OpenPath(path, flags, shareMask);
}

/* ================================================================== */
/* WIN32_Stream::OpenPath — 0x463AA0                                   */
/* ================================================================== */
void WIN32_Stream::OpenPath(const char* path, uint32_t flags, uint32_t shareMask)
{
    WIN32_StreamFile* file = static_cast<WIN32_StreamFile*>(rdbuf);
    if (file != nullptr && file->fileHandle() == -1) {
        if (file->Open(path, flags | 1, shareMask) != nullptr) {
            return;
        }
    }

    if (sync_flag < 0) {
        WNDPROC_EnterCriticalSection(&critical_section);
    }
    state_bits |= kFailBit;
    if (sync_flag < 0) {
        WNDPROC_LeaveCriticalSection(&critical_section);
    }
}

/* ================================================================== */
/* WIN32_Stream::CloseNow — 0x463B10 ("WIN32_StreamDestroyImmediate")   */
/* ================================================================== */
void WIN32_Stream::CloseNow()
{
    WIN32_StreamFile* file = static_cast<WIN32_StreamFile*>(rdbuf);
    bool ok = (file != nullptr) && (file->CloseHandle() != nullptr);
    /* Matches the original's exact ordering: the OR against the existing
     * state_bits is read BEFORE the critical section guarding the write
     * is entered (verified against disassembly — a pre-existing race in
     * the original, reproduced faithfully rather than "fixed"). */
    uint32_t newBits = ok ? 0u : (state_bits | kFailBit);

    if (sync_flag < 0) {
        WNDPROC_EnterCriticalSection(&critical_section);
    }
    state_bits = newBits;
    if (sync_flag < 0) {
        WNDPROC_LeaveCriticalSection(&critical_section);
    }
}

/* ================================================================== */
/* Free-function facades — see Win32Stream.h for scope/linkage notes   */
/* ================================================================== */

void* WIN32_StreamOpen(void* stream, int /*initBase*/)
{
    return ::new (stream) WIN32_Stream();
}

void* WIN32_StreamOpenFile(void* stream, const char* path, uint32_t flags,
                            uint32_t shareMask, int /*initBase*/)
{
    return ::new (stream) WIN32_Stream(path, flags, shareMask);
}

void WIN32_StreamOpenPath(void* stream, const char* path, uint32_t flags,
                           uint32_t shareMask)
{
    static_cast<WIN32_Stream*>(stream)->OpenPath(path, flags, shareMask);
}

void* WIN32_StreamRead(void* stream, void* buf, uint32_t size)
{
    return static_cast<WIN32_Stream*>(stream)->Read(buf, size);
}

void WIN32_StreamDestroyImmediate(void* stream)
{
    static_cast<WIN32_Stream*>(stream)->CloseNow();
}

/* 0x463A80 — see Win32Stream.h's doc comment for the full rationale:
 * disassembly proves the real argument is `&stream->StreamObject_subobject`
 * (this+0xc in the original layout), but no caller in this tree reliably
 * supplies that offset, and the ~15 call sites' own declarations disagree
 * with the callee's contract and with each other. Guessing an offset here
 * would violate CLAUDE.md's evidence-only rule, so this stays a loud,
 * tracked stub pending the separate caller-unification pass. */
void WIN32_StreamDestroy(void* connector)
{
    fprintf(stderr,
            "STUB: WIN32_StreamDestroy (0x463A80) reached with connector=%p "
            "— its real argument contract (this+0xc, a StreamObject "
            "sub-object address) does not match any caller in this tree; "
            "see resources/Win32Stream.h\n",
            connector);
    assert(false &&
           "WIN32_StreamDestroy: deferred, see TODO in resources/Win32Stream.h");
}
