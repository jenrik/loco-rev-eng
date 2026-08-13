/**
 * Win32StreamMem.h — memory-backed stream, derived from WNDPROC_StreamBuf
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly (locoaudit DB).
 *
 * WIN32_StreamMem is the memory-backed sibling of WIN32_StreamFile (see
 * Win32StreamFile.h/.cpp): it backs the put/get regions with a
 * caller-supplied buffer instead of an open CRT file descriptor.
 * WndProcStreamBuf.h's field-layout comment already documents that
 * WIN32_StreamMem_Ctor confirms the base subobject ends at +0x4C (it
 * starts its own two fields there and at +0x58).
 *
 * Address map (locoaudit), cross-checked against the vtable this
 * constructor stores at +0x00 (0x4791DC, read via read_bytes and compared
 * dword-by-dword against WIN32_StreamFile's vtable at 0x4791AC — see
 * Win32StreamFile.h for that address):
 *   WIN32_StreamMem_Ctor        0x463FF0  -> WIN32_StreamMem()  [THIS TASK]
 *   WIN32_StreamMem_ScalarDtor  0x463FD0  -> compiler-generated
 *   WIN32_StreamMem_DtorBody    0x4640B0  -> ~WIN32_StreamMem(), NOT modeled
 *     (see .cpp for why — it branches on a field at +0x60 that this ctor
 *     never initializes, so its exact contract needs more evidence than
 *     this batch established; the implicit/compiler-generated derived
 *     destructor is safe for every object this ctor produces, since it
 *     always constructs with ownsBuffer_ == 0, so the base destructor's
 *     conditional GLOBAL_free never fires).
 *   vtable +0x04 Flush          0x46E590  -> override, NOT YET a Ghidra
 *     Function (no disassembly available this batch) — deferred stub.
 *   vtable +0x08 SetBuffer      0x464250  -> override, NOT YET a Ghidra
 *     Function — deferred stub.
 *   vtable +0x1C WriteChar      0x464260  -> override; IS disassembled
 *     (Ghidra mislabels it "WNDPROC_StreamClose", unrelated to the real
 *     WIN32_StreamClose at 0x463A60 documented below) but implementing it
 *     correctly also requires the AllocateDefaultBuffer override below —
 *     deferred stub.
 *   vtable +0x28 AllocateDefaultBuffer  0x464120 -> the original DOES
 *     override this (Ghidra mislabels it "WNDPROC_StreamOpen"; 130
 *     instructions implementing a growable-buffer reallocation, not a
 *     simple allocate-once) but this class does NOT override it here —
 *     it inherits WNDPROC_StreamBuf's simpler fixed-0x200-byte allocator
 *     as a documented simplification, deferred alongside the three
 *     stubs above.
 * None of the four vtable-override addresses above are in this task's
 * scope (WIN32_StreamMem_Ctor, WIN32_StreamClose, WIN32_PostQuit); they
 * are overridden here only because WNDPROC_StreamBuf declares Flush/
 * SetBuffer/WriteChar pure virtual, so a concrete, instantiable
 * WIN32_StreamMem needs *some* body for each. Tracked in PROGRESS.md.
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
 */

// Status: VALIDATED (WIN32_StreamMem_Ctor only; the four vtable overrides
// below are TRANSCRIBED-of-a-stub — see .cpp)

#pragma once

#include "WndProcStreamBuf.h"

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
     *                         empty); nonzero => also sets up a nominal
     *                         write region sized bufferCapacity (see .cpp
     *                         for the exact, branch-dependent field
     *                         semantics this mirrors from the assembly). */
    WIN32_StreamMem(char* data, int32_t dataLen, int32_t bufferCapacity);

    /* vtable +0x04, +0x08, +0x1C: see file header — deferred, out of scope
     * for this task, present only so this class is concrete/instantiable. */
    int32_t Flush() override;                          /* 0x46E590 */
    void* SetBuffer(void* buffer, int32_t size) override; /* 0x464250 */
    int32_t WriteChar(int32_t ch) override;             /* 0x464260 */

    /* AllocateDefaultBuffer (vtable +0x28) is intentionally NOT overridden
     * here — see file header "Field layout" note. Inherits
     * WNDPROC_StreamBuf::AllocateDefaultBuffer(). */

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
