/**
 * Win32StreamMem.cpp — WIN32_StreamMem / WIN32_MemoryStream implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly (loco DB).
 *
 * See Win32StreamMem.h for the class/field/address maps. This file
 * implements:
 *   WIN32_StreamMem::WIN32_StreamMem   0x463FF0
 *   WIN32_StreamMem::Underflow         0x4642F0
 *   WIN32_MemoryStream::WIN32_MemoryStream / WNDPROC_StreamFromMemory 0x464490
 * plus three deferred-stub vtable overrides required only so
 * WIN32_StreamMem is concrete (see header — none of the three are in this
 * task's scope, and none are reachable from any real caller of
 * WIN32_MemoryStream, which only ever constructs WIN32_StreamMem in
 * read-only-view mode).
 */

// Status: VALIDATED (WIN32_StreamMem ctor + Underflow, WIN32_MemoryStream,
// WNDPROC_StreamFromMemory)

#include "Win32StreamMem.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <new>

/* ================================================================== */
/* WIN32_StreamMem_Ctor — 0x463FF0                                     */
/*                                                                      */
/* Disassembly summary (0x463FF0-0x4640AC):                             */
/*   - Calls the base WNDPROC_StreamBuf ctor (implicit here via the      */
/*     member-initializer list below).                                  */
/*   - Stores the WIN32_StreamMem vtable at +0x00 (compiler-managed;     */
/*     not modeled).                                                     */
/*   - field_58_ = 1, then ownsMemory_ = 0 (MOV [ESI+0x58],1 precedes    */
/*     MOV [ESI+0x4C],EBP where EBP was zeroed earlier — verified via    */
/*     fresh disassembly read, not just the decompiler's field order).   */
/*   - Computes an end pointer from (data, dataLen):                    */
/*       dataLen == 0  -> data + strlen(data)                            */
/*       dataLen < 0   -> the literal sentinel (char*)-1                 */
/*       dataLen > 0   -> data + dataLen                                 */
/*   - SetBufferPtrs(data, end, owns=false) (0x465730).                  */
/*   - readBase_ = readPtr_ = data (get region starts at the buffer      */
/*     start; get-region pointers, not previously evidenced by the base  */
/*     ctor alone — confirmed by WndProcStreamBuf.h's independent        */
/*     naming, which this matches exactly).                              */
/*   - Then branches on bufferCapacity:                                  */
/*       == 0: readHigh_ = end; peekCache_ = -1; writeBase_ = writePtr_ */
/*             = nullptr; writeHigh_ = nullptr.  (read-only view: no     */
/*             write region — the only mode WNDPROC_StreamFromMemory     */
/*             ever uses; get_xrefs_to(0x463FF0) confirms it is this     */
/*             constructor's sole caller in the whole binary.)           */
/*       != 0: readHigh_ = (uint8_t*)(intptr_t)bufferCapacity (the raw   */
/*             int reused as a pointer-sized value — matches the         */
/*             original's "undefined4" field exactly; not a real         */
/*             pointer until something else interprets it, mirroring     */
/*             the lazy-buffer-allocation design documented in           */
/*             WndProcStreamBuf.h); peekCache_ = -1 (redundant with the */
/*             base ctor — the original re-sets it too, preserved for    */
/*             fidelity); writeBase_ = writePtr_ = the same raw-int-as-  */
/*             pointer value; writeHigh_ = end. Dead code for every real */
/*             caller in this codebase (see above), reproduced anyway    */
/*             since it is a real branch of the original function.       */
/* ================================================================== */
WIN32_StreamMem::WIN32_StreamMem(char* data, int32_t dataLen, int32_t bufferCapacity)
    : ownsMemory_(0),
      field_58_(1)
{
    char* end;
    if (dataLen == 0) {
        end = data + strlen(data);
    } else if (dataLen < 0) {
        end = reinterpret_cast<char*>(static_cast<intptr_t>(-1));
    } else {
        end = data + dataLen;
    }

    SetBufferPtrs(reinterpret_cast<uint8_t*>(data), reinterpret_cast<uint8_t*>(end),
                  /* owns = */ 0);
    readBase_ = reinterpret_cast<uint8_t*>(data);
    readPtr_  = reinterpret_cast<uint8_t*>(data);

    if (bufferCapacity == 0) {
        readHigh_   = reinterpret_cast<uint8_t*>(end);
        peekCache_ = -1;
        writeBase_  = nullptr;
        writePtr_   = nullptr;
        writeHigh_  = nullptr;
    } else {
        uint8_t* const capacityAsPointer =
            reinterpret_cast<uint8_t*>(static_cast<intptr_t>(bufferCapacity));
        readHigh_   = capacityAsPointer;
        peekCache_ = -1;
        writeBase_  = capacityAsPointer;
        writePtr_   = capacityAsPointer;
        writeHigh_  = reinterpret_cast<uint8_t*>(end);
    }
}

/* ================================================================== */
/* WIN32_StreamMem::Underflow — 0x4642F0                               */
/*                                                                      */
/* Disassembly summary:                                                 */
/*   - Fast path: if the get-region still has unread bytes              */
/*     (readPtr_ < readHigh_, unsigned compare), return *readPtr_        */
/*     zero-extended WITHOUT advancing readPtr_ (that is GetChar()'s     */
/*     job, StreamBuf_GetChar 0x4651A0 — this is purely the peek hook).  */
/*   - Otherwise, if readHigh_ < writePtr_ (unsigned) — i.e. there is     */
/*     pending written-but-not-yet-readable data beyond the current      */
/*     read window — re-home the read cursor onto bufferStart_ (in case  */
/*     the buffer was reallocated) and advance readHigh_ up to           */
/*     writePtr_, then retry the fast path. This "growable buffer"        */
/*     refill is DEAD CODE for every real caller of WIN32_MemoryStream:   */
/*     WNDPROC_StreamFromMemory (Win32StreamMem.h) always constructs      */
/*     this class with bufferCapacity == 0, which the ctor above turns    */
/*     into writePtr_ == nullptr — and readHigh_ (a real, non-null        */
/*     pointer in every real construction) is never less than nullptr    */
/*     unsigned, so this branch never executes for any object this        */
/*     codebase actually creates. Reproduced anyway: it is a real basic   */
/*     block of the original function, and WIN32_StreamMem's ctor takes   */
/*     a bufferCapacity parameter specifically to support it.             */
/*   - If still exhausted after that, return -1 (EOF).                    */
/* ================================================================== */
int32_t WIN32_StreamMem::Underflow()
{
    if (readPtr_ < readHigh_) {
        return *readPtr_;
    }

    if (readHigh_ < writePtr_) {
        uint8_t* const oldReadBase = readBase_;
        uint8_t* const oldReadPtr  = readPtr_;
        readBase_ = bufferStart_;
        readHigh_ = writePtr_;
        peekCache_ = -1;
        readPtr_  = bufferStart_ + (oldReadPtr - oldReadBase);
    }

    if (readPtr_ >= readHigh_) {
        return -1;
    }
    return *readPtr_;
}

/* ================================================================== */
/* Deferred vtable overrides — see Win32StreamMem.h for why these exist */
/* and why each is out of this task's scope. None are reachable from     */
/* WIN32_MemoryStream's real construction path (read-only view only).    */
/* ================================================================== */

int32_t WIN32_StreamMem::Flush()
{
    fprintf(stderr, "STUB: WIN32_StreamMem::Flush (0x46E590) reached — "
                     "not yet a disassembled Ghidra function, see "
                     "resources/Win32StreamMem.h\n");
    assert(false && "WIN32_StreamMem::Flush: deferred, see TODO in Win32StreamMem.h");
    return -1;
}

void* WIN32_StreamMem::SetBuffer(void* /*buffer*/, int32_t /*size*/)
{
    fprintf(stderr, "STUB: WIN32_StreamMem::SetBuffer (0x464250) reached — "
                     "not yet a disassembled Ghidra function, see "
                     "resources/Win32StreamMem.h\n");
    assert(false && "WIN32_StreamMem::SetBuffer: deferred, see TODO in Win32StreamMem.h");
    return nullptr;
}

int32_t WIN32_StreamMem::WriteChar(int32_t /*ch*/)
{
    fprintf(stderr, "STUB: WIN32_StreamMem::WriteChar (0x464260, mislabeled "
                     "\"WNDPROC_StreamClose\" by Ghidra) reached — its real "
                     "body also depends on the not-yet-modeled "
                     "AllocateDefaultBuffer override (0x464120), see "
                     "resources/Win32StreamMem.h\n");
    assert(false && "WIN32_StreamMem::WriteChar: deferred, see TODO in Win32StreamMem.h");
    return -1;
}

int32_t WIN32_StreamMem::AllocateDefaultBuffer()
{
    fprintf(stderr, "STUB: WIN32_StreamMem::AllocateDefaultBuffer (0x464120) "
                     "reached — 130-instruction growable-buffer reallocator, "
                     "not yet decompiled, see resources/Win32StreamMem.h\n");
    assert(false &&
           "WIN32_StreamMem::AllocateDefaultBuffer: deferred, see TODO in Win32StreamMem.h");
    return -1;
}

/* WIN32_StreamClose (0x463A60) and its two callees are intentionally not
 * defined here — see Win32StreamMem.h's doc comment on 0x463A60 for the
 * full evidence trail. It is pure MSVC vptr-retagging bookkeeping that
 * real C++ virtual-base destruction already reproduces, and this class
 * has zero real callers anywhere in this codebase (confirmed via grep). */

/* ================================================================== */
/* WIN32_MemoryStream::WIN32_MemoryStream — WNDPROC_StreamFromMemory's   */
/* real body, 0x464490                                                   */
/* ================================================================== */
WIN32_MemoryStream::WIN32_MemoryStream(char* data, int32_t dataLen)
{
    AttachBuffer(new WIN32_StreamMem(data, dataLen, /*bufferCapacity=*/0));
    owns_rdbuf = 1;
}

/* ================================================================== */
/* Free-function facade — see Win32StreamMem.h for scope/linkage notes  */
/* ================================================================== */

size_t WIN32_StreamMem_Size()
{
    return sizeof(WIN32_StreamMem);
}

size_t WIN32_MemoryStream_Size()
{
    return sizeof(WIN32_MemoryStream);
}

WNDPROC_Stream* WNDPROC_StreamFromMemory(void* stream, char* data,
                                          int32_t size, int32_t /*initBase*/)
{
    return ::new (stream) WIN32_MemoryStream(data, size);
}
