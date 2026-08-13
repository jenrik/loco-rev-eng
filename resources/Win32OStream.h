/**
 * Win32OStream.h — WIN32_OStream: the file-backed write-only WNDPROC_OStream
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly.
 *
 * WIN32_OStream is the concrete WNDPROC_OStream (see WndProcOStream.h)
 * whose rdbuf is a heap-allocated WIN32_StreamFile (see Win32StreamFile.h)
 * opened for writing (mode | 2) — the write counterpart of WIN32_Stream's
 * read constructor (WIN32_Stream(path, flags, shareMask), Win32Stream.h,
 * 0x463970, mode | 1).
 *
 * Confirmed a genuinely distinct class from WIN32_Stream, not a second
 * vtable-poke variant of the same class constructed at a different
 * most-derived-vs-base-subobject level — see WndProcOStream.h's header
 * comment for the full vbtable/size evidence (different vbtable, 4 bytes
 * smaller allocation, one fewer own data member than WNDPROC_Stream).
 * WIN32_OStream itself adds no further fields of its own beyond
 * WNDPROC_OStream, exactly mirroring how WIN32_Stream adds none beyond
 * WNDPROC_Stream — both concrete classes contribute only constructors
 * (and, for WIN32_Stream, OpenPath/CloseNow methods for which no
 * write-side counterpart has been found in the binary).
 *
 * Address map:
 *   WIN32_OStream(path,flags,shareMask)  0x465090  -> ctor
 *     (Ghidra auto-analysis mislabeled this "CRT_floor" — it is not the
 *     CRT floor() function; same class of auto-analysis artifact as
 *     "CRT_exp" turning out to be WIN32_StreamFile::Open at 0x4652D0, see
 *     Win32StreamFile.h)
 *
 * No default (no-path) constructor, OpenPath, or CloseNow analog for this
 * class has been found anywhere in the binary — 0x465090's sole caller
 * (RESMGR_LoadResourceData, 0x447E8F, passing mode 0x92 and the same
 * shareMask global other WIN32_Stream callers use) is the only evidenced
 * entry point, so only that one constructor is modeled here.
 */

// Status: VALIDATED (see WndProcOStream.h for the one pre-existing,
// shared gap this status reflects — StreamObject's own destructor body)

#pragma once

#include "WndProcOStream.h"
#include "Win32StreamFile.h"

class WIN32_OStream : public WNDPROC_OStream {
public:
    /* 0x465090. Allocates a fresh, unopened WIN32_StreamFile, attaches it
     * as rdbuf (WNDPROC_OStream::AttachBuffer) then sets owns_rdbuf = 1
     * (the original sets this flag itself, separately from AttachBuffer,
     * immediately after attaching — same convention as WIN32_Stream's read
     * constructor). Immediately calls rdbuf->Open(path, flags | 2,
     * shareMask); ORs failbit into state_bits if that fails. */
    WIN32_OStream(const char* path, uint32_t flags, uint32_t shareMask);
};

/* ================================================================== */
/* Free-function facade (entry point for the existing caller)          */
/*                                                                      */
/* Plain C++ linkage, matching WIN32_Stream's equivalent facades         */
/* (Win32Stream.h). resources/ResDataSave.cpp's `#else` (_WIN32-only)    */
/* branch is the only caller in the tree.                               */
/* ================================================================== */

/* 0x465090. `initBase` is accepted for call-site arity compatibility
 * only — see Win32Stream.h's equivalent facade comment for why: MSVC's
 * most-derived-vs-base-subobject construction flag, superseded by real
 * C++ virtual-base construction ordering. */
void* WIN32_StreamOpenWriteFile(void* stream, const char* path, uint32_t flags,
                                 uint32_t shareMask, int initBase);

/* Returns sizeof(WIN32_OStream) on this host, mirroring
 * Win32Stream.h's WIN32_Stream_Size() (same rationale: lets callers that
 * only need to size a WIN32_StreamOpenWriteFile allocation get the real
 * size without including this header's class declaration). */
size_t WIN32_OStream_Size();
