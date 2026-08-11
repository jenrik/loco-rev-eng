/**
 * Win32Stream.h — WIN32_Stream: the file-backed concrete WNDPROC_Stream
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly.
 *
 * WIN32_Stream is the concrete WNDPROC_Stream (see WndProcStream.h) whose
 * rdbuf is always a heap-allocated WIN32_StreamFile (see Win32StreamFile.h),
 * confirmed by WIN32_StreamOpen/OpenFile's disassembly: `operator_new(0x54)`
 * (0x54 == sizeof(WIN32_StreamFile)) followed by WIN32_StreamFile's real
 * constructor, then AttachBuffer() (WndProcStream.h) with owns_rdbuf set to
 * 1 immediately after — NOT an embedded/inline sub-object, a heap pointer
 * this class owns.
 *
 * Address map:
 *   WIN32_StreamOpen             0x463890  -> WIN32_Stream()
 *   WIN32_StreamOpenFile         0x463970  -> WIN32_Stream(path,flags,shareMask)
 *   WIN32_StreamOpenPath         0x463AA0  -> OpenPath(path,flags,shareMask)
 *   WIN32_StreamDestroyImmediate 0x463B10  -> CloseNow()
 *
 * WIN32_StreamDestroy (0x463A80) is deliberately NOT reconstructed as a
 * WIN32_Stream method — see the free-function facade below for why.
 *
 * The original's `initBase` parameter on WIN32_StreamOpen/OpenFile (and the
 * matching parameter on AttachBuffer's underlying original function, see
 * WndProcStream.h) is MSVC's most-derived-vs-base-subobject construction
 * flag; real C++ virtual-base construction ordering provides the same
 * guarantee automatically, so it is not reproduced as a meaningful
 * parameter on the class constructors below (same reasoning as
 * StreamObject::StreamObject()'s doc comment) — it is kept only on the
 * free-function facades, unused, for call-site arity compatibility.
 */

// Status: VALIDATED

#pragma once

#include "WndProcStream.h"
#include "Win32StreamFile.h"

class WIN32_Stream : public WNDPROC_Stream {
public:
    /* 0x463890. Allocates a fresh, unopened WIN32_StreamFile and attaches
     * it as rdbuf, then sets owns_rdbuf = 1 (the original sets this flag
     * itself, separately from AttachBuffer, immediately after attaching). */
    WIN32_Stream();

    /* 0x463970. Same as the default constructor, then immediately calls
     * OpenPath(path, flags, shareMask) on the new buffer; ORs failbit into
     * state_bits if that fails. */
    WIN32_Stream(const char* path, uint32_t flags, uint32_t shareMask);

    /* 0x463AA0. Opens `path` on the existing rdbuf if it isn't already
     * open (WIN32_StreamFile::fileHandle() == -1); ORs failbit into
     * state_bits on failure or if the buffer was already open. */
    void OpenPath(const char* path, uint32_t flags, uint32_t shareMask);

    /* 0x463B10. Immediately closes the underlying file handle (via
     * WIN32_StreamFile::CloseHandle()) rather than waiting for
     * destruction. Resets state_bits to 0 on success; ORs failbit into
     * the existing state_bits on failure (matches the original reading
     * state_bits for the OR before entering the critical section that
     * guards the write — reproduced with the same ordering). */
    void CloseNow();
};

/* ================================================================== */
/* Free-function facades (entry points for existing callers)           */
/*                                                                      */
/* Plain C++ linkage, matching the majority of existing (if mutually    */
/* inconsistent) extern declarations across the .cpp callers in the      */
/* tree. Unifying every caller onto these exact signatures is its own,   */
/* separately tracked follow-up (PROGRESS.md "win32_stream.c removed     */
/* (partial)"; see also WndProcStream.cpp's WNDPROC_CriticalSectionLock  */
/* postmortem comment for the full rationale) — NOT attempted here.      */
/* .c-file callers (native/wave_io.c, native/cgwnd_palette.c) need C     */
/* linkage and will not bind to these until that follow-up gives them    */
/* dedicated extern "C" wrappers; until then they keep resolving via     */
/* -Wl,--unresolved-symbols=ignore-all (LINK-001), same as before this   */
/* change — not a new regression.                                        */
/* ================================================================== */

/* 0x463890. `initBase` is accepted for call-site arity compatibility
 * only — see the class comment above for why it does nothing here. */
void* WIN32_StreamOpen(void* stream, int initBase);

/* 0x463970. `initBase`: see above. */
void* WIN32_StreamOpenFile(void* stream, const char* path, uint32_t flags,
                            uint32_t shareMask, int initBase);

/* 0x463AA0 */
void WIN32_StreamOpenPath(void* stream, const char* path, uint32_t flags,
                           uint32_t shareMask);

/* 0x463810 */
void* WIN32_StreamRead(void* stream, void* buf, uint32_t size);

/* 0x463B10 */
void WIN32_StreamDestroyImmediate(void* stream);

/* 0x463A80 — deliberately NOT implemented (loud deferred stub in the
 * .cpp). Disassembly shows this function's real argument is
 * `&stream->StreamObject_subobject` (`this+0xc` in the original x86
 * layout — confirmed via the `MOV EAX,[ECX-0xc]` back-reference at the
 * function's own entry, not a decompiler artifact), but every real
 * caller in the tree passes something else (the outer object's own base
 * address, or an arbitrary sub-offset that doesn't match +0xc either) —
 * none of the ~15 call sites' own (already-flagged-inconsistent) extern
 * declarations agree with the callee's true contract or with each other.
 * Safely reconstructing which offset an incoming `void*` actually points
 * at here would require guessing, which CLAUDE.md's evidence-only rule
 * and stub policy both forbid. Deferred pending the separate
 * caller-declaration-unification pass already tracked in PROGRESS.md. */
void WIN32_StreamDestroy(void* connector);
