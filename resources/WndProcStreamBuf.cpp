/**
 * WndProcStreamBuf.cpp — WNDPROC_StreamBuf implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly (locoaudit DB).
 *
 * See WndProcStreamBuf.h for the field/address map. This file transcribes:
 *   WNDPROC_StreamBuf_Ctor                 0x465470
 *   WNDPROC_StreamBuf_DtorBody             0x4654E0
 *   WNDPROC_StreamBuf_CheckFlush           0x4656D0
 *   WNDPROC_StreamBuf_AllocateDefaultBuffer 0x4656F0
 *   WNDPROC_StreamBuf_SetBuffer            0x465730
 *   StreamBuf_ReadChar                     0x4652A0
 *   StreamBuf_GetChar                      0x4651A0
 * Lock()/Unlock() have no dedicated original address — they consolidate
 * logic inlined at every call site inside WNDPROC_Stream's methods (see
 * WndProcStream.cpp).
 */

// Status: VALIDATED

#include "WndProcStreamBuf.h"

#include <cstring>  /* memcpy */

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* operator_new(size_t)/GLOBAL_free are already declared by stubs/windows.h
 * (included via WndProcStreamBuf.h); declaring a second operator_new
 * overload here (e.g. uint32_t, as ResourceManager.cpp/ResDataSave.cpp do)
 * would make plain integer-literal calls ambiguous between the two. */

extern "C" {
/* Win32 CRITICAL_SECTION wrappers — thin IAT forwarders in the original
 * binary (0x464D70/0x464D80), same family as WNDPROC_EnterCriticalSection/
 * WNDPROC_LeaveCriticalSection (0x464D90/0x464DA0, already declared and
 * stubbed as host no-ops in resources/StreamObject.cpp + shared/
 * defsym_stubs.cpp). Declared __stdcall(void*) to match that precedent. */
void __stdcall WNDPROC_InitializeCriticalSection(void* cs);  /* 0x464D70 */
void __stdcall WNDPROC_DeleteCriticalSection(void* cs);      /* 0x464D80 */

/* Same family, used by Lock()/Unlock() below (0x464D90/0x464DA0 — already
 * declared/stubbed in resources/StreamObject.cpp; redeclaring the
 * identical extern "C" signature here is fine ODR-wise, both are
 * declarations only). */
void __stdcall WNDPROC_EnterCriticalSection(void* cs);
void __stdcall WNDPROC_LeaveCriticalSection(void* cs);
}

/* ================================================================== */
/* WNDPROC_StreamBuf_Ctor — 0x465470                                   */
/* ================================================================== */
WNDPROC_StreamBuf::WNDPROC_StreamBuf()
    : ownsBuffer_(0),
      unbuffered_(0),
      peekCache_(-1),
      bufferStart_(nullptr),
      bufferEnd_(nullptr),
      writeBase_(nullptr),
      writePtr_(nullptr),
      writeHigh_(nullptr),
      readBase_(nullptr),
      readPtr_(nullptr),
      readHigh_(nullptr),
      syncActive_(-1)
{
    /* Original sets the vtable pointer explicitly here; the compiler emits
     * that store automatically for a C++ constructor. */
    WNDPROC_InitializeCriticalSection(&cs_);
}

/* ================================================================== */
/* WNDPROC_StreamBuf_DtorBody — 0x4654E0                                */
/* ================================================================== */
WNDPROC_StreamBuf::~WNDPROC_StreamBuf()
{
    WNDPROC_DeleteCriticalSection(&cs_);

    /* The original also calls a helper here (0x4656B0, renamed
     * WNDPROC_StreamBuf_HasPendingData) that compares the write and read
     * region bases against their cursors and returns 0/-1. Its result is
     * never consumed — verified against the raw disassembly, which falls
     * straight through to the buffer-free check below with no branch on
     * EAX — so the call has no observable effect and is omitted here.
     * See the Ghidra comment at 0x4656B0 for the full argument. */

    if (ownsBuffer_ != 0 && bufferStart_ != nullptr) {
        GLOBAL_free(bufferStart_);
    }

    /* The SEH scaffolding around the original DtorBody (ExceptionList
     * save/restore, the local unwind-guard slot) is MSVC frame-based
     * exception-handling machinery, not application logic; the C++
     * compiler emits equivalent unwind support for this destructor. */
}

/* ================================================================== */
/* WNDPROC_StreamBuf_CheckFlush — 0x4656D0                              */
/* ================================================================== */
int32_t WNDPROC_StreamBuf::CheckFlush()
{
    if (unbuffered_ == 0 && bufferStart_ == nullptr) {
        int32_t rc = AllocateDefaultBuffer();
        /* Original: return (-(uint)(rc != -1) & 2) - 1;  i.e. 1 on success,
         * -1 on failure. Equivalence: rc != -1 => -1u & 2 == 2, 2-1 == 1;
         * rc == -1 => 0 & 2 == 0, 0-1 == -1. */
        return (rc != -1) ? 1 : -1;
    }
    return 0;
}

/* ================================================================== */
/* WNDPROC_StreamBuf_AllocateDefaultBuffer — 0x4656F0                   */
/* (Ghidra auto-analysis mislabeled this "CRT_exit_handler"; renamed —   */
/*  see WndProcStreamBuf.h header comment for the vtable-read evidence.)*/
/* ================================================================== */
int32_t WNDPROC_StreamBuf::AllocateDefaultBuffer()
{
    constexpr int32_t kDefaultBufferSize = 0x200;
    void* buf = operator_new(kDefaultBufferSize);
    if (buf == nullptr) {
        return -1;
    }
    SetBufferPtrs(static_cast<uint8_t*>(buf),
                  static_cast<uint8_t*>(buf) + kDefaultBufferSize,
                  /* owns = */ 1);
    return 1;
}

/* ================================================================== */
/* WNDPROC_StreamBuf_SetBuffer — 0x465730                               */
/* ================================================================== */
void WNDPROC_StreamBuf::SetBufferPtrs(uint8_t* bufferStart, uint8_t* bufferEnd, int32_t owns)
{
    if (ownsBuffer_ != 0 && bufferStart_ != nullptr) {
        GLOBAL_free(bufferStart_);
    }
    bufferStart_ = bufferStart;
    ownsBuffer_ = owns;
    bufferEnd_ = bufferEnd;
}

/* ================================================================== */
/* WNDPROC_StreamBuf_ReadBytes — 0x4655C0 (vtable+0x18)                 */
/* ================================================================== */
int32_t WNDPROC_StreamBuf::ReadBytes(void* buf, int32_t size)
{
    uint8_t* out = static_cast<uint8_t*>(buf);
    int32_t totalRead = 0;

    if (unbuffered_ == 0) {
        if (size != 0) {
            do {
                if (Underflow() == -1) {
                    return totalRead;
                }
                int32_t avail = static_cast<int32_t>(readHigh_ - readPtr_);
                int32_t chunk = (size <= avail) ? size : avail;
                if (chunk > 0) {
                    memcpy(out, readPtr_, static_cast<size_t>(chunk));
                    totalRead += chunk;
                    out += chunk;
                    readPtr_ += chunk;
                    size -= chunk;
                }
            } while (size != 0);
        }
        return totalRead;
    }

    /* Unbuffered mode: one byte at a time via peekCache_, matching
     * ReadChar()/GetChar()'s own peekCache_ usage above. */
    if (peekCache_ == -1) {
        peekCache_ = Underflow();
    }
    if (size != 0) {
        for (;;) {
            --size;
            if (peekCache_ == -1) {
                break;
            }
            *out++ = static_cast<uint8_t>(peekCache_);
            ++totalRead;
            peekCache_ = Underflow();
            if (size == 0) {
                break;
            }
        }
    }
    return totalRead;
}

/* ================================================================== */
/* StreamBuf_ReadChar — 0x4652A0                                        */
/* ================================================================== */
int32_t WNDPROC_StreamBuf::ReadChar()
{
    if (unbuffered_ != 0) {
        if (peekCache_ == -1) {
            peekCache_ = Underflow();
        }
        return peekCache_;
    }
    return Underflow();
}

/* ================================================================== */
/* StreamBuf_GetChar — 0x4651A0                                         */
/* ================================================================== */
uint32_t WNDPROC_StreamBuf::GetChar()
{
    if (unbuffered_ != 0) {
        if (peekCache_ == -1) {
            Underflow();          /* prime; result intentionally discarded */
        }
        uint32_t c = static_cast<uint32_t>(Underflow());
        peekCache_ = static_cast<int32_t>(c);
        return c;
    }

    if (readHigh_ == nullptr || readPtr_ >= readHigh_) {
        Underflow();               /* refill; result intentionally discarded */
    }
    readPtr_ = readPtr_ + 1;
    if (readPtr_ < readHigh_) {
        return *readPtr_;
    }
    return static_cast<uint32_t>(Underflow());
}

/* ================================================================== */
/* Lock/Unlock — inlined at every call site in the original            */
/* (InputPrefix/SkipWhitespace/Flush, see WndProcStream.cpp); given a  */
/* single home here since WNDPROC_Stream can't reach the protected     */
/* syncActive_/cs_ fields directly.                                    */
/* ================================================================== */
void WNDPROC_StreamBuf::Lock()
{
    if (syncActive_ < 0) {
        WNDPROC_EnterCriticalSection(&cs_);
    }
}

void WNDPROC_StreamBuf::Unlock()
{
    if (syncActive_ < 0) {
        WNDPROC_LeaveCriticalSection(&cs_);
    }
}
