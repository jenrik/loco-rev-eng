/**
 * Win32StreamMem.cpp — WIN32_StreamMem / WIN32_MemoryStream implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly (loco DB).
 *
 * See Win32StreamMem.h for the class/field/address maps. This file
 * implements:
 *   WIN32_StreamMem::WIN32_StreamMem              0x463FF0
 *   WIN32_StreamMem::Flush                        0x46E590
 *   WIN32_StreamMem::SetBuffer                    0x464250
 *   WIN32_StreamMem::WriteChar                    0x464260
 *   WIN32_StreamMem::AllocateDefaultBuffer        0x464120
 *   WIN32_StreamMem::Underflow                    0x4642F0
 *   WIN32_MemoryStream::WIN32_MemoryStream / WNDPROC_StreamFromMemory 0x464490
 */

// Status: VALIDATED (WIN32_StreamMem ctor + Underflow + Flush/SetBuffer/
// WriteChar/AllocateDefaultBuffer, WIN32_MemoryStream, WNDPROC_StreamFromMemory)

#include "Win32StreamMem.h"
#include <cstring>
#include <new>

/* operator_new(size_t)/GLOBAL_free(void*) (0x465CE0/0x465CD0) — this
 * project's own custom allocator hooks, not real Win32 APIs. C++ linkage,
 * matching every other .cpp call site in the tree and their real definition
 * in shared/stubs_impl.cpp (see resources/WndProcStreamBuf.cpp's identical
 * declaration/comment). Used by AllocateDefaultBuffer() below as the
 * fallback when allocHook_/freeHook_ are null. */
void* operator_new(size_t size);
void  GLOBAL_free(void* ptr);

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
/*   - reserveSize_/unknown_0x54_/allocHook_/freeHook_ (+0x50/+0x54/     */
/*     +0x5C/+0x60) are NOT touched by this constructor at all — real    */
/*     objects leave them uninitialized. This C++ port zero-initializes  */
/*     them (below) purely so reading an object's fields is never UB;    */
/*     every real construction path (WNDPROC_StreamFromMemory) never     */
/*     reaches a branch that reads any of the four before something      */
/*     else (never reached either) would have written them, so this is   */
/*     a behavior-preserving choice, not a simplification of real logic. */
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
      reserveSize_(0),
      unknown_0x54_(0),
      field_58_(1),
      allocHook_(nullptr),
      freeHook_(nullptr)
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
/* WIN32_StreamMem::Flush — 0x46E590                                   */
/*                                                                      */
/* Disassembly (entire body): `xor eax,eax; ret`. Nothing to sync for a */
/* memory-backed stream — there is no underlying file descriptor.       */
/* ================================================================== */
int32_t WIN32_StreamMem::Flush()
{
    return 0;
}

/* ================================================================== */
/* WIN32_StreamMem::SetBuffer — 0x464250                               */
/*                                                                      */
/* Disassembly: ignores `buffer` entirely; if `size != 0`, stores it    */
/* into reserveSize_ (own field, consumed by AllocateDefaultBuffer()     */
/* below as the grow-by hint). Always returns `this`, matching the       */
/* sibling WIN32_StreamFile::SetBuffer's `return this` contract.         */
/* ================================================================== */
void* WIN32_StreamMem::SetBuffer(void* /*buffer*/, int32_t size)
{
    if (size != 0) {
        reserveSize_ = size;
    }
    return this;
}

/* ================================================================== */
/* WIN32_StreamMem::WriteChar — 0x464260                               */
/* (Ghidra mislabels it "WNDPROC_StreamClose" — an auto-analysis         */
/* artifact; unrelated to the real WIN32_StreamClose at 0x463A60, see    */
/* the header's doc comment on that address.)                            */
/*                                                                        */
/* "Overflow" hook: if the write region is full (writeHigh_ <= writePtr_,*/
/* unsigned), refuses to grow unless ownsMemory_ is set (own field —     */
/* CONFIRMED shared gate with the real destructor, see header), then      */
/* calls AllocateDefaultBuffer() and re-homes the write cursor: onto the  */
/* get-region's tail if this is the first-ever write-region setup         */
/* (writeHigh_ was still null going in), or onto its own previous          */
/* relative offset if a write region already existed (subsequent grows).  */
/* Writes the character and advances writePtr_ only when buffering         */
/* (writeHigh_) is active — matches the disassembly's                      */
/* `if (*(this+0x20) != 0) ++writePtr_` guard exactly.                      */
/* ================================================================== */
int32_t WIN32_StreamMem::WriteChar(int32_t ch)
{
    if (writeHigh_ <= writePtr_) {
        if (ownsMemory_ == 0) {
            return -1;
        }
        if (AllocateDefaultBuffer() == -1) {
            return -1;
        }
        if (writeHigh_ == nullptr) {
            uint8_t* const base = bufferStart_ + (readHigh_ - readBase_);
            writeBase_ = base;
            writePtr_  = base;
            writeHigh_ = bufferEnd_;
        } else {
            uint8_t* const oldWriteBase = writeBase_;
            uint8_t* const oldWritePtr  = writePtr_;
            writePtr_  = oldWriteBase;
            writeHigh_ = bufferEnd_;
            if (bufferEnd_ != nullptr) {
                writePtr_ = oldWriteBase + (oldWritePtr - oldWriteBase);
            }
        }
    }

    if (ch != -1) {
        *writePtr_ = static_cast<uint8_t>(ch);
        if (writeHigh_ != nullptr) {
            ++writePtr_;
        }
    }
    return 1;
}

/* ================================================================== */
/* WIN32_StreamMem::AllocateDefaultBuffer — 0x464120                   */
/* (Ghidra mislabels it "WNDPROC_StreamOpen" — an auto-analysis          */
/* artifact; unrelated to the real WIN32_StreamFile::Open.)              */
/*                                                                        */
/* Growable-buffer reallocator, not a simple allocate-once (unlike the    */
/* base class's own AllocateDefaultBuffer, WndProcStreamBuf.cpp):         */
/*   1. newSize = reserveSize_ if reserveSize_ exceeds usedBytes+growBy,  */
/*      else usedBytes+growBy, where growBy = max(reserveSize_, 1) and    */
/*      usedBytes = max(bufferEnd_-bufferStart_, 0). (The disassembly     */
/*      recomputes growBy/usedBytes a second time inside the taken        */
/*      branch purely because the registers holding them were reused —   */
/*      both branches read the exact same fields, so this is one          */
/*      evaluation here, not two; verified byte-for-byte equivalent.)     */
/*   2. Allocate newSize bytes via allocHook_(newSize) if set, else       */
/*      ::operator new(newSize).                                          */
/*   3. memcpy the live [bufferStart_, bufferEnd_) bytes into the new      */
/*      buffer (the original's manual dword+byte REP MOVSD/MOVSB loop —   */
/*      reproduced here as std::memcpy, a provable behavioral match).      */
/*   4. Free the old buffer via freeHook_(oldBufferStart) if set, else     */
/*      GLOBAL_free(oldBufferStart).                                       */
/*   5. Install the new buffer via the base's SetBufferPtrs() (owns=0 —   */
/*      matches the disassembly's literal 3rd PUSH 0 at the call site;     */
/*      this class tracks "should I free my own buffer" via its own        */
/*      ownsMemory_/freeHook_ pair instead, see header).                   */
/*   6. If any bytes were actually moved (shift != 0) and a read region    */
/*      was live (readHigh_ != nullptr), shift readBase_/readPtr_/          */
/*      readHigh_ by the move delta and reset peekCache_ to -1.            */
/*   7. If a write region was live (writeHigh_ != nullptr) going in,       */
/*      shift writeBase_/writeHigh_ by the delta and restore writePtr_'s   */
/*      relative offset — same "temporarily reset, then restore if         */
/*      buffering is active" pattern as WriteChar's own grow logic above.  */
/* ================================================================== */
int32_t WIN32_StreamMem::AllocateDefaultBuffer()
{
    const int32_t growBy = (reserveSize_ > 1) ? reserveSize_ : 1;
    const int32_t usedBytes = (bufferStart_ < bufferEnd_)
        ? static_cast<int32_t>(bufferEnd_ - bufferStart_) : 0;
    const int32_t newSize = (reserveSize_ > usedBytes + growBy)
        ? reserveSize_ : usedBytes + growBy;

    uint8_t* const newBuf = allocHook_
        ? static_cast<uint8_t*>(allocHook_(newSize))
        : static_cast<uint8_t*>(::operator new(static_cast<size_t>(newSize)));
    if (newBuf == nullptr) {
        return -1;
    }

    uint8_t* const oldBufferStart = bufferStart_;
    int32_t shift = 0;
    if (usedBytes != 0) {
        std::memcpy(newBuf, oldBufferStart, static_cast<size_t>(usedBytes));
        shift = static_cast<int32_t>(newBuf - oldBufferStart);
    }

    if (freeHook_) {
        freeHook_(oldBufferStart);
    } else {
        GLOBAL_free(oldBufferStart);
    }

    SetBufferPtrs(newBuf, newBuf + newSize, /*owns=*/0);

    if (shift != 0 && readHigh_ != nullptr) {
        readBase_ += shift;
        readPtr_  += shift;
        readHigh_ += shift;
        peekCache_ = -1;
    }

    if (writeHigh_ != nullptr) {
        uint8_t* const oldWriteBase = writeBase_;
        uint8_t* const oldWritePtr  = writePtr_;
        writeHigh_ += shift;
        writeBase_  = oldWriteBase + shift;
        writePtr_   = writeBase_;
        if (writeHigh_ != nullptr) {
            writePtr_ = writeBase_ + (oldWritePtr - oldWriteBase);
        }
    }

    return 1;
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
