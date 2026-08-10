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

/* ================================================================== */
/* External reference for WIN32_StreamClose below.                     */
/*                                                                       */
/* WNDPROC_StreamCleanup — 0x464620. Already has a real (still no-op)   */
/* symbol resolved by shared/defsym_stubs.cpp, shared by ui/HelpWnd.cpp  */
/* and others via this exact `void*` overload; NOT implemented here.    */
/* Its own real body remains its own tracked gap (PROGRESS.md           */
/* "win32_stream.c removed").                                            */
/* ================================================================== */
extern void WNDPROC_StreamCleanup(void* stream);

/* ================================================================== */
/* WNDPROC_StreamPutChar — TODO: decompile 0x4648E0                    */
/*                                                                       */
/* Deferred. Disassembly (4 instructions):                              */
/*   MOV EAX, [ECX - 0xC]                                               */
/*   MOV EDX, [EAX + 0x4]                                               */
/*   MOV dword ptr [EDX + ECX - 0xC], 0x479234                          */
/*   RET                                                                */
/* i.e. it treats [child-0xC] as a "connector" object pointer, reads    */
/* the connector's own +0x4 field as a second table base, and stores a  */
/* fixed label address (0x479234) at (that table base) + (the child's   */
/* own pointer value used as a byte offset). That arithmetic is only    */
/* meaningful inside the original 32-bit x86 process's address space;   */
/* reproducing it on a 64-bit host with unrelated heap addresses would  */
/* compute an unrelated, likely invalid, write address instead of       */
/* detaching the child stream as intended. The connector/table types    */
/* are not reconstructed (only reachable via WNDPROC_StreamFromMemory,  */
/* itself still a stub), so there is no typed replacement available     */
/* yet either. Tracked in PROGRESS.md alongside "win32_stream.c         */
/* removed". Called by: WIN32_StreamClose (0x463A60).                  */
/* ================================================================== */
static void WNDPROC_StreamPutChar(void* /*child*/)
{
    fprintf(stderr, "STUB: WNDPROC_StreamPutChar (0x4648E0) reached — "
                     "connector/vtable hierarchy not yet reconstructed, "
                     "see resources/Win32StreamMem.cpp\n");
    assert(false && "WNDPROC_StreamPutChar: deferred, see TODO in Win32StreamMem.cpp");
}

/* ================================================================== */
/* WIN32_StreamClose — 0x463A60                                        */
/* ================================================================== */
void WIN32_StreamClose(void* streamOwner)
{
    char* embeddedStream = static_cast<char*>(streamOwner) + 0xC;
    WNDPROC_StreamPutChar(embeddedStream);
    WNDPROC_StreamCleanup(embeddedStream);
}
