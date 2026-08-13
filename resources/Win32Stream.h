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
 * callable function at all (not even a free-function facade) — its real
 * behavior settles the question rather than merely defers it:
 *
 * Disassembly (4 instructions) shows this function's real argument is
 * `&stream->StreamObject_subobject` (`this+0xC` in the original x86
 * layout — confirmed via the `MOV EAX,[ECX-0xC]` back-reference at the
 * function's own entry, not a decompiler artifact) and its ENTIRE body is:
 *   MOV EAX, [ECX-0xC]                    ; own-identity vtable pointer
 *   MOV EDX, [EAX+0x4]                    ; vtable slot [1] = vbase offset
 *   MOV [EDX+ECX-0xC], 0x479184           ; re-tag the StreamObject
 *                                          ; subobject's own vptr slot
 *   JMP WNDPROC_Stream_DtorVftableReset (0x4648E0, tail call, same shape,
 *                                          re-tags the same slot to 0x479234)
 * i.e. it does no cleanup whatsoever — it is pure MSVC "walk down
 * re-tagging each virtual-base vptr as the destructor chain unwinds"
 * bookkeeping, EXACTLY analogous to WIN32_OStream_DtorVftableReset/
 * WNDPROC_OStream_DtorVftableReset documented in WndProcOStream.h (same
 * instruction shape, different constant). Confirmed end-to-end by
 * tracing all ~15 real callers in the original binary (game/
 * TrainStation.cpp, game/ScriptedObject.cpp, ui/HelpWnd.cpp,
 * ui/CursorEditWindow.cpp, ui/UIPANEL_Surface.cpp, input/
 * BuildingDescriptorEditor.cpp, ui/AboutDialog.cpp, UI_ChildWindow_Create,
 * RESMGR_OpenResourceFile, WIN32_Stream_ScalarDtor itself): every one
 * constructs a WIN32_Stream in a raw local buffer via
 * WIN32_StreamOpen(&buf,1), then on cleanup calls
 * WIN32_StreamDestroy(&buf.StreamObject_subobject) immediately followed
 * by WNDPROC_StreamCleanup(&buf.StreamObject_subobject) — a SEPARATE
 * function (0x464620) on the exact same pointer that does the real
 * cleanup (frees an owned rdbuf, tears down the lock, decrements a shared
 * refcount). That pairing is exactly what a real C++ `~WIN32_Stream()`
 * does: virtual-base vptr bookkeeping (free, from the compiler) followed
 * by `~StreamObject()`'s real body — now implemented for real as
 * StreamObject::~StreamObject() (see StreamObject.h/.cpp's doc comments
 * for the full evidence trail). Every one of the ~15 callers has been
 * converted to a real stack-allocated WIN32_Stream local with real C++
 * construction/destruction (or, for the few callers that never actually
 * call WIN32_StreamDestroy — input/BuildingDescriptorEditor.cpp,
 * ui/AboutDialog.cpp, resources/ResourceManager.cpp,
 * world/scriptengine.cpp, UI_ChildWindow_Create — left as the
 * dead/unresolved declarations or authorized stubs they already were).
 * With every real caller converted, WIN32_StreamDestroy has no remaining
 * reason to exist as a callable symbol in this codebase at all: real C++
 * virtual-base destruction ordering provides its one real effect (correct
 * vtable identity during the unwind) automatically, for free.
 *
 * The original's `initBase` parameter on WIN32_StreamOpen/OpenFile (and the
 * matching parameter on AttachBuffer's underlying original function, see
 * WndProcStream.h) is MSVC's most-derived-vs-base-subobject construction
 * flag; real C++ virtual-base construction ordering provides the same
 * guarantee automatically, so it is not reproduced as a meaningful
 * parameter on the class constructors below (same reasoning as
 * StreamObject::StreamObject()'s doc comment) — it is kept only on the
 * free-function facades, unused, for call-site arity compatibility.
 *
 * WIN32_Stream is NOT the write-side counterpart of the WIN32_StreamFile
 * rdbuf story: the write-stream constructor at 0x465090 (previously
 * mislabeled "CRT_floor") builds a genuinely distinct, 4-bytes-smaller
 * sibling class — WIN32_OStream, deriving from WNDPROC_OStream rather than
 * WNDPROC_Stream (see Win32OStream.h/WndProcOStream.h for the vbtable/
 * allocation-size evidence this is a separate class, not this one
 * constructed at a different most-derived level).
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

/* 0x463810. Accepts any WNDPROC_Stream-family object (not just
 * WIN32_Stream) — see the .cpp definition for why. */
void* WIN32_StreamRead(void* stream, void* buf, uint32_t size);

/* 0x463B10 */
void WIN32_StreamDestroyImmediate(void* stream);

/* Returns sizeof(WIN32_Stream) on this host (0x80 bytes here vs. the
 * original x86's 0x5C — StreamObject's pointer-bearing base-class fields
 * widen from 4 to 8 bytes; see StreamObject.h/WndProcStream.h). Exists so
 * callers that only need to size a WIN32_StreamOpen(File)/
 * WNDPROC_StreamFromMemory allocation can get the real size without
 * including this header, whose class declaration would conflict with
 * those callers' own pre-existing, already-flagged-inconsistent local
 * extern "C" declarations of WIN32_StreamOpen et al (see the
 * "Free-function facades" comment above) — unifying those is the
 * separately tracked follow-up this header already defers. */
size_t WIN32_Stream_Size();

/* WIN32_StreamDestroy (0x463A80) is intentionally NOT declared as a
 * callable function here — see the class doc comment above for the full
 * evidence trail. It is pure MSVC vptr-retagging bookkeeping with no
 * observable effect once real C++ virtual-base destruction is in play;
 * every real caller has been converted to a real WIN32_Stream local and
 * no longer calls it. */
