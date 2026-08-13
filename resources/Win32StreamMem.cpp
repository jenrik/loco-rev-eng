/**
 * Win32StreamMem.cpp — WIN32_StreamMem implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly (locoaudit DB).
 *
 * See Win32StreamMem.h for the field/address map. This file implements:
 *   WIN32_StreamMem_Ctor  0x463FF0  (this task)
 * plus four deferred-stub vtable overrides required only so the class is
 * concrete (see header — none of the four are in this task's scope).
 */

// Status: VALIDATED (ctor only)

#include "Win32StreamMem.h"
#include <cassert>
#include <cstdio>
#include <cstring>

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
/*             write region.)                                            */
/*       != 0: readHigh_ = (uint8_t*)(intptr_t)bufferCapacity (the raw   */
/*             int reused as a pointer-sized value — matches the         */
/*             original's "undefined4" field exactly; not a real         */
/*             pointer until something else interprets it, mirroring    */
/*             the lazy-buffer-allocation design documented in           */
/*             WndProcStreamBuf.h); peekCache_ = -1 (redundant with the */
/*             base ctor — the original re-sets it too, preserved for    */
/*             fidelity); writeBase_ = writePtr_ = the same raw-int-as-  */
/*             pointer value; writeHigh_ = end.                          */
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
/* Deferred vtable overrides — see Win32StreamMem.h for why these exist */
/* and why each is out of this task's scope.                            */
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

/* WIN32_StreamClose (0x463A60) and its two callees are intentionally not
 * defined here — see Win32StreamMem.h's doc comment on 0x463A60 for the
 * full evidence trail. The address this file previously called
 * "WNDPROC_StreamPutChar" (0x4648E0) under a deferred-stub TODO is not a
 * distinct, undecompiled function at all: it is the exact same address
 * already fully resolved this session as WNDPROC_Stream_DtorVftableReset
 * (resources/Win32Stream.h's WIN32_StreamDestroy doc comment) — pure MSVC
 * vptr-retagging bookkeeping that real C++ virtual-base destruction
 * already reproduces. WIN32_StreamClose itself has zero real callers
 * anywhere in this codebase (confirmed via grep) and, in the original
 * binary, is called only by a compiler-generated SEH unwind funclet
 * (Unwind@004766a5, confirmed via get_xrefs_to) — not game logic. */
