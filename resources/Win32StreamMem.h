/**
 * Win32StreamMem.h — memory-backed stream buffer, and the concrete
 * WNDPROC_Stream sibling class that uses it
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly (loco DB).
 *
 * This file holds two related but distinct classes:
 *
 *   WIN32_StreamMem     : public WNDPROC_StreamBuf   — the streambuf
 *       (put/get regions backed by a caller-supplied memory block instead
 *       of an open CRT file descriptor; the memory-backed sibling of
 *       WIN32_StreamFile, see Win32StreamFile.h/.cpp).
 *
 *   WIN32_MemoryStream  : public WNDPROC_Stream      — the connector/facade
 *       (the memory-backed sibling of WIN32_Stream, see Win32Stream.h/.cpp;
 *       owns a heap WIN32_StreamMem as its rdbuf).
 *
 * They are kept in one file because WIN32_MemoryStream exists for no other
 * reason than to own and expose a WIN32_StreamMem, exactly mirroring how
 * Win32Stream.h keeps WIN32_Stream next to (by including) Win32StreamFile.h.
 *
 * ==================================================================
 * WIN32_StreamMem
 * ==================================================================
 * WndProcStreamBuf.h's field-layout comment already documents that
 * WIN32_StreamMem_Ctor confirms the base subobject ends at +0x4C (it
 * starts its own two fields there and at +0x58).
 *
 * Address map, cross-checked against the vtable this constructor stores
 * at +0x00 (0x4791DC, read via read_bytes and compared dword-by-dword
 * against WIN32_StreamFile's vtable at 0x4791AC — see Win32StreamFile.h
 * for that address):
 *   WIN32_StreamMem_Ctor        0x463FF0  -> WIN32_StreamMem()
 *   WIN32_StreamMem_ScalarDtor  0x463FD0  -> compiler-generated
 *   WIN32_StreamMem_DtorBody    0x4640B0  -> ~WIN32_StreamMem(), NOT modeled
 *     (see .cpp for why — it branches on a field at +0x60 that this ctor
 *     never initializes, so its exact contract needs more evidence than
 *     this batch established; the implicit/compiler-generated derived
 *     destructor is safe for every object this ctor produces, since it
 *     always constructs with ownsMemory_ == 0, so the base destructor's
 *     conditional GLOBAL_free never fires).
 *   vtable +0x04 Flush          0x46E590  -> override, deferred stub.
 *   vtable +0x08 SetBuffer      0x464250  -> override, deferred stub.
 *   vtable +0x1C WriteChar      0x464260  -> override; IS disassembled
 *     (Ghidra mislabels it "WNDPROC_StreamClose", unrelated to the real
 *     WIN32_StreamClose at 0x463A60 documented below) but implementing it
 *     correctly also requires the AllocateDefaultBuffer override below —
 *     deferred stub.
 *   vtable +0x20 Underflow       0x4642F0  -> override, REAL (this batch).
 *     Fills the peek/get slot: if the get-region still has unread bytes
 *     (readPtr_ < readHigh_), returns the next byte without advancing. If
 *     exhausted, the original also supports a "growable buffer" refill
 *     (re-home readPtr_/readHigh_ onto bufferStart_/writePtr_) — dead code
 *     for every real caller (see below), reproduced anyway for basic-block
 *     completeness, since it IS a real branch in this function's body.
 *     Returns -1 on true EOF.
 *   vtable +0x28 AllocateDefaultBuffer  0x464120 -> the original DOES
 *     override this (Ghidra mislabels it "WNDPROC_StreamOpen"; 130
 *     instructions implementing a growable-buffer reallocation, not a
 *     simple allocate-once) — confirmed via a fresh vtable+0x28 read this
 *     batch (a prior session's note claiming this slot was "not
 *     overridden, inherits the base" was wrong and is corrected here).
 *     Declared as a real override below so the class's vtable membership
 *     is honest, but its body stays a deferred stub alongside WriteChar's
 *     (WriteChar's own real behavior depends on it) — never reached by any
 *     of this class's real construction paths (see below), so leaving it
 *     unimplemented changes no observable behavior yet.
 * None of Flush/SetBuffer/WriteChar/AllocateDefaultBuffer's real bodies are
 * in this task's scope; they are overridden here only because
 * WNDPROC_StreamBuf declares Flush/SetBuffer/WriteChar pure virtual and
 * because the original genuinely overrides AllocateDefaultBuffer, so a
 * concrete, instantiable WIN32_StreamMem needs *some* body for each.
 * Tracked in PROGRESS.md.
 *
 * Field layout added by this class beyond the WNDPROC_StreamBuf base
 * (offsets are the original x86 layout, documentation only):
 *   +0x4C ownsMemory_   zeroed by the ctor; no further evidence this batch
 *                        (name is a guess by analogy with ownsBuffer_/
 *                        ownsHandle_ — NOT confirmed by a read site).
 *   +0x58 field_58_     set to 1 by the ctor; no further evidence.
 *   +0x60 (unnamed)     read by the real destructor (0x4640B0) as a
 *                        possible free-callback function pointer, but
 *                        never written by this constructor — see above.
 *                        Not represented as a member here (no ctor-side
 *                        evidence for its initial value).
 *
 * sizeof(WIN32_StreamMem) on the original x86 is 0x64 bytes (the literal
 * `operator_new(0x64)` immediately preceding WIN32_StreamMem_Ctor's call in
 * WNDPROC_StreamFromMemory, its sole caller — see WIN32_MemoryStream below).
 *
 * ==================================================================
 * WIN32_MemoryStream
 * ==================================================================
 * WIN32_MemoryStream is the memory-backed sibling of WIN32_Stream
 * (Win32Stream.h): a concrete WNDPROC_Stream whose rdbuf is always a
 * heap-allocated WIN32_StreamMem, constructed in read-only-view mode
 * (bufferCapacity == 0 always — every real caller passes a complete,
 * already-loaded buffer with a real byte count, never a growable one; see
 * WIN32_StreamMem's own ctor doc for what bufferCapacity == 0 means).
 *
 * Confirmed a genuinely distinct concrete class from WIN32_Stream (NOT the
 * same class constructed at a different most-derived level) via vtable
 * evidence: WNDPROC_StreamFromMemory (below) retags the StreamObject
 * subobject's vptr to 0x47920C after attaching, a DIFFERENT address from
 * WIN32_Stream's own final retag target (0x479184, Win32Stream.h). Reading
 * both tables confirms each is the classic MSVC "2-slot vbtable + 1-slot
 * StreamObject-view vftable" pair (see shared/vtable_addrs.h's
 * VTBL_WIN32_MEMORYSTREAM(_VIEW) for the byte-level detail and the sibling
 * WNDPROC_Stream/WIN32_Stream pairs it was cross-checked against) — not
 * two different tables for the *same* class, and not a "slot-shifted
 * sub-vtable" trick: the vbtable installs `[0]=0, [1]=0xC` at `this+0x00`
 * (the byte offset to the StreamObject virtual base), and the 1-slot
 * vftable installs this class's own scalar deleting destructor
 * (0x464460) at the StreamObject subobject's own vptr slot (`this+0xC`).
 * Every one of this class's ~11 real, currently-ported callers allocates
 * exactly 0x5C bytes (`operator_new(0x5C)`) for the WIN32_MemoryStream
 * object itself — the SAME size as WIN32_Stream's own 0x5C (Win32Stream.h)
 * — confirming this class adds no fields beyond WNDPROC_Stream, exactly
 * like WIN32_Stream/WIN32_OStream add none beyond their own bases.
 *
 * Address map:
 *   WNDPROC_StreamFromMemory     0x464490  -> WIN32_MemoryStream(data,size)
 *     free-function facade (see Win32Stream.h's WIN32_StreamOpen for the
 *     exact analogous pattern: placement-constructs into caller-supplied
 *     `stream` storage, sized via WIN32_MemoryStream_Size() below).
 *   WIN32_MemoryStream_ScalarDtor        0x464460 -> compiler-generated
 *     (calls WIN32_MemoryStream_DtorVftableReset then StreamObject::
 *     ~StreamObject(), then conditionally frees — the exact same shape as
 *     WIN32_Stream_ScalarDtor, Win32Stream.h's WIN32_StreamDestroy doc
 *     comment; not reimplemented, real C++ `delete` reproduces this).
 *   WIN32_MemoryStream_DtorVftableReset  0x464550 -> pure MSVC vptr-retag
 *     bookkeeping (Ghidra auto-name "WNDPROC_StreamSeek" was misleading —
 *     it does not seek; renamed). Not reimplemented, same reasoning as
 *     WIN32_StreamDestroy (Win32Stream.h).
 *
 * bufferCapacity is hardcoded 0 in WNDPROC_StreamFromMemory's call into
 * WIN32_StreamMem's constructor — confirmed by disassembly, not an
 * assumption — so every WIN32_MemoryStream's rdbuf is a read-only view:
 * no write region, SetBufferPtrs(..., owns=0) (the caller-supplied data
 * buffer is never freed by this class or its rdbuf; every real caller
 * frees it itself, via CRT_free, after destroying the stream). This is
 * also why WIN32_StreamMem's WriteChar/SetBuffer staying deferred stubs is
 * safe for this class's actual usage (never reached), whereas Underflow
 * is NOT safe to leave stubbed — every real caller reads through this
 * stream (WNDPROC_Stream::Read/ReadChar/GetChar, WNDPROC_StreamReadLine,
 * WNDPROC_StreamSeekForward all fall through to Underflow()).
 */

// Status: VALIDATED (WIN32_StreamMem_Ctor, WIN32_StreamMem::Underflow,
// WIN32_MemoryStream, WNDPROC_StreamFromMemory; the three remaining
// deferred vtable overrides on WIN32_StreamMem are TRANSCRIBED-of-a-stub
// — see .cpp)

#pragma once

#include "WndProcStreamBuf.h"
#include "WndProcStream.h"

/* ================================================================== */
/* WIN32_StreamMem — memory-backed WNDPROC_StreamBuf                   */
/* ================================================================== */
class WIN32_StreamMem : public WNDPROC_StreamBuf {
public:
    /** WIN32_StreamMem_Ctor — 0x463FF0.
     *  @param data            backing buffer.
     *  @param dataLen         0 => data is NUL-terminated, use its strlen;
     *                         <0 => end pointer is the sentinel (char*)-1
     *                         (matches the disassembly's literal -1, not a
     *                         guarded "unknown length" case);
     *                         >0 => explicit length in bytes.
     *  @param bufferCapacity  0 => "read-only view" mode (write region left
     *                         empty; the only mode WNDPROC_StreamFromMemory
     *                         ever uses — see WIN32_MemoryStream above);
     *                         nonzero => also sets up a nominal write
     *                         region sized bufferCapacity (see .cpp for the
     *                         exact, branch-dependent field semantics this
     *                         mirrors from the assembly; dead code for
     *                         every real caller in this codebase, but a
     *                         real basic block of the original function). */
    WIN32_StreamMem(char* data, int32_t dataLen, int32_t bufferCapacity);

    /* vtable +0x04, +0x08, +0x1C, +0x28: see file header — deferred, out
     * of scope for this task, present only so this class is concrete/
     * instantiable and its vtable membership is honest. */
    int32_t Flush() override;                          /* 0x46E590 */
    void* SetBuffer(void* buffer, int32_t size) override; /* 0x464250 */
    int32_t WriteChar(int32_t ch) override;             /* 0x464260 */
    int32_t AllocateDefaultBuffer() override;           /* 0x464120 */

    /* vtable +0x20 ("underflow" hook). REAL — see file header. */
    int32_t Underflow() override;                       /* 0x4642F0 */

private:
    int32_t ownsMemory_;  /* +0x4C */
    int32_t field_58_;    /* +0x58 */
};

/* WIN32_StreamClose (0x463A60) is intentionally NOT declared as a
 * callable function here. Disassembly (8 instructions) shows its entire
 * body is:
 *   LEA ESI, [ECX+0xC]                    ; the WNDPROC_StreamBuf-family
 *                                          ; sub-object embedded at +0xC
 *   CALL WNDPROC_Stream_DtorVftableReset(ESI)   ; pure vptr-retag
 *                                                ; bookkeeping (see
 *                                                ; resources/Win32Stream.h's
 *                                                ; WIN32_StreamDestroy doc
 *                                                ; comment — same shape,
 *                                                ; same target address)
 *   CALL WNDPROC_StreamCleanup(ESI)             ; = StreamObject::
 *                                                ; ~StreamObject(), now a
 *                                                ; real destructor (see
 *                                                ; resources/StreamObject.h)
 * i.e. the exact same two-call shape WIN32_StreamDestroy+WNDPROC_
 * StreamCleanup already had, now resolved as ~WIN32_Stream()'s bookkeeping-
 * plus-real-body pattern. get_xrefs_to(0x463A60) confirms its ONLY real
 * caller in the original binary is Unwind@004766a5 — a compiler-generated
 * SEH unwind funclet cleaning up an in-progress WIN32_StreamMem_Ctor on
 * exception, not game logic. Real C++ exception-safe construction (a
 * partially-constructed WIN32_StreamMem's base/member destructors run
 * automatically on an exception during its own constructor) already
 * reproduces this for free, and this codebase has zero real callers of
 * WIN32_StreamClose (confirmed via grep) — nothing here calls it in the
 * first place. See CLAUDE.md's "compiler-generated ... EH ... helpers
 * are documented but not reimplemented" rule. */

/* ================================================================== */
/* WIN32_MemoryStream — concrete WNDPROC_Stream over a WIN32_StreamMem  */
/* ================================================================== */
class WIN32_MemoryStream : public WNDPROC_Stream {
public:
    /* WNDPROC_StreamFromMemory's real body — 0x464490. Allocates a heap
     * WIN32_StreamMem(data, dataLen, bufferCapacity=0) [read-only
     * view], attaches it via WNDPROC_Stream::AttachBuffer, then sets
     * owns_rdbuf = 1 (the original sets this flag itself, separately from
     * AttachBuffer, immediately after attaching — same convention as
     * WIN32_Stream's own constructors). `data` is never freed by this
     * class or its rdbuf — every real caller retains ownership and frees
     * it itself after destroying this stream. */
    WIN32_MemoryStream(char* data, int32_t dataLen);
};

/* ================================================================== */
/* Free-function facade (entry point for existing callers)             */
/*                                                                      */
/* Plain C++ linkage, matching Win32Stream.h's WIN32_StreamOpen/Win32   */
/* OStream.h's WIN32_StreamOpenWriteFile pattern exactly. `stream` is    */
/* caller-supplied, uninitialized storage sized via                     */
/* WIN32_MemoryStream_Size() below — placement-constructed in place,     */
/* mirroring the original's own caller-allocated-storage convention.    */
/* ================================================================== */

/* 0x464490. `initBase` is accepted for call-site arity compatibility
 * only, same reasoning as Win32Stream.h's WIN32_StreamOpen: MSVC's
 * most-derived-vs-base-subobject construction flag, superseded by real
 * C++ virtual-base construction ordering. Returns a WNDPROC_Stream* (not
 * void*) since every real caller immediately uses the result only through
 * WNDPROC_Stream-level operations (Read/ReadLine/SeekForward/ExtractToken)
 * or `delete`s it — never anything WIN32_MemoryStream-specific. */
WNDPROC_Stream* WNDPROC_StreamFromMemory(void* stream, char* data,
                                          int32_t size, int32_t initBase);

/* Returns sizeof(WIN32_StreamMem) on this host — the memory-backed
 * streambuf (rdbuf), NOT the facade/connector object. No real caller in
 * this codebase needs this: WNDPROC_StreamFromMemory allocates the
 * WIN32_StreamMem internally; callers only ever size the facade object
 * (see WIN32_MemoryStream_Size() below). Provided for parity with
 * Win32StreamFile.h/WIN32_Stream_Size()'s convention and any future
 * caller that needs to size a bare WIN32_StreamMem directly. */
size_t WIN32_StreamMem_Size();

/* Returns sizeof(WIN32_MemoryStream) on this host (0x5C on the original
 * x86 — see the class doc comment above for the allocation-size evidence
 * across all ~11 real callers). This is the size every real caller
 * actually needs: it sizes the `stream` buffer passed to
 * WNDPROC_StreamFromMemory above, mirroring Win32Stream.h's
 * WIN32_Stream_Size() convention exactly. */
size_t WIN32_MemoryStream_Size();
