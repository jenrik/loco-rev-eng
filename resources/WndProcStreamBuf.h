/**
 * WndProcStreamBuf.h — abstract base class for loco.exe's buffered stream
 * hierarchy (a private, pre-standard C++ "streambuf" reimplementation)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly (locoaudit DB).
 *
 * WNDPROC_StreamBuf is the shared base of WIN32_StreamFile (file-backed
 * stream, see Win32StreamFile.h) and WIN32_StreamMem (memory-backed stream,
 * not yet reverse engineered — WIN32_StreamMem_Ctor at 0x463FF0 is confirmed
 * to call WNDPROC_StreamBuf_Ctor too, proving this really is a shared base,
 * not a coincidence of similar layouts).
 *
 * This models the classic Dinkumware/AT&T pre-standard "streambuf" class
 * bundled with early MSVC's <iostream.h> (not std::): a put-region
 * (writeBase_/writePtr_/writeHigh_), a get-region (readBase_/readPtr_/
 * readHigh_), an overridable "overflow" hook (WriteChar, declared by the
 * derived class), and a "doallocate" hook (AllocateDefaultBuffer) that
 * lazily creates a default buffer on first use.
 *
 * Address map (locoaudit):
 *   WNDPROC_StreamBuf_Ctor                0x465470  -> WNDPROC_StreamBuf()
 *   WNDPROC_StreamBuf_ScalarDtor          0x4654C0  -> compiler-generated
 *   WNDPROC_StreamBuf_DtorBody            0x4654E0  -> ~WNDPROC_StreamBuf()
 *   WNDPROC_StreamBuf_HasPendingData      0x4656B0  -> not reimplemented;
 *     see the .cpp file's destructor comment (proven dead code: called by
 *     DtorBody with its result discarded, verified via disassembly).
 *   WNDPROC_StreamBuf_CheckFlush          0x4656D0  -> CheckFlush()
 *   WNDPROC_StreamBuf_AllocateDefaultBuffer 0x4656F0 -> AllocateDefaultBuffer()
 *     (Ghidra auto-analysis had mislabeled this "CRT_exit_handler"; renamed
 *     after confirming it is vtable+0x28 on WIN32_StreamFile's own vtable at
 *     0x4791AC, read directly via read_bytes: dword at +0x28 == 0x004656F0.)
 *   WNDPROC_StreamBuf_SetBuffer           0x465730  -> SetBufferPtrs()
 *
 * Field layout (verified against WNDPROC_StreamBuf_Ctor's disassembly,
 * which zero-initializes +0x04..+0x2C and sets +0x0C/+0x30 = -1, plus
 * cross-checked against every accessor in WIN32_StreamFile's WriteChar/
 * Flush/SetBuffer). Base subobject size is 0x4C bytes — confirmed by
 * WIN32_StreamMem_Ctor, a sibling subclass, which starts its own fields at
 * +0x4C/+0x58, the same boundary WIN32_StreamFile uses for fd_/ownsHandle_:
 *
 *   +0x00 vtable ptr            (compiler-managed; not stored here)
 *   +0x04 ownsBuffer_           nonzero => bufferStart_ was heap-allocated
 *                                by this object (via operator_new) and must
 *                                be freed on replacement/destruction
 *   +0x08 unbuffered_           nonzero => bypass buffering entirely
 *   +0x0C reserved0C_           initialized/reset to -1; no other read
 *                                observed in Ctor/DtorBody/CheckFlush/
 *                                SetBuffer or WIN32_StreamFile's WriteChar/
 *                                Flush/SetBuffer/CloseHandle — likely an
 *                                ungetc/pushback slot exercised only by
 *                                get-side methods not yet reverse engineered
 *   +0x10 bufferStart_          allocated/caller buffer base
 *   +0x14 bufferEnd_            one past the end of the buffer
 *   +0x18 writeBase_            start of the pending (unflushed) write run
 *   +0x1C writePtr_             current write cursor
 *   +0x20 writeHigh_            end of the write region (== bufferEnd_ when
 *                                buffering is active)
 *   +0x24 readBase_             start of the buffered-but-unconsumed region
 *   +0x28 readPtr_              current read cursor
 *   +0x2C readHigh_             end of the buffered-but-unconsumed region
 *   +0x30 syncActive_           <0 => this object's operations lock cs_
 *   +0x34 cs_                   Win32 CRITICAL_SECTION (24 bytes, ends the
 *                                base subobject at +0x4C)
 *
 * Per CLAUDE.md's host-deviation rules, exact offsets above are recovery
 * documentation only; the C++ members below use natural layout and the
 * compiler manages the vtable — no manual offset packing.
 */

// Status: VALIDATED

#pragma once

#include <cstdint>

#include "../shared/types.h"  /* defines COLORREF etc. before stubs/windows.h
                                * uses them; must precede that include (same
                                * ordering resources/StreamObject.h relies on) */

#ifdef INVALID_HANDLE_VALUE
#undef INVALID_HANDLE_VALUE
#endif
#ifdef HKEY_CURRENT_USER
#undef HKEY_CURRENT_USER
#endif
#ifdef HKEY_LOCAL_MACHINE
#undef HKEY_LOCAL_MACHINE
#endif
#include <windows.h>

class WNDPROC_StreamBuf {
public:
    WNDPROC_StreamBuf();
    virtual ~WNDPROC_StreamBuf();

    /* vtable +0x04 ("overflow"-equivalent sync/flush hook). WIN32_StreamFile
     * overrides this with its real Flush (0x463E50); WIN32_StreamMem (out of
     * scope) presumably supplies its own. No base implementation exists in
     * the binary, so this stays pure. */
    virtual int32_t Flush() = 0;

    /* vtable +0x08. WIN32_StreamFile overrides this with its real SetBuffer
     * (0x463F50, 2-arg buf/size contract). Distinct from the protected
     * SetBufferPtrs() helper below (0x465730, 3-arg buf/end/owns contract),
     * which is not virtual and is called BY the overrides, not overridden
     * itself. */
    virtual void* SetBuffer(void* buffer, int32_t size) = 0;

    /* vtable +0x1C ("overflow" hook proper). WIN32_StreamFile overrides this
     * with its real WriteChar (0x463CB0). */
    virtual int32_t WriteChar(int32_t ch) = 0;

    /* vtable +0x28 ("doallocate"). Base class supplies a real, concrete
     * implementation (0x4656F0); WIN32_StreamFile does not override this
     * slot (confirmed by direct vtable read — see file header). */
    virtual int32_t AllocateDefaultBuffer();

protected:
    /* WNDPROC_StreamBuf_CheckFlush, 0x4656D0. Despite the address's original
     * (wrong) auto-generated name, this is a lazy buffer-allocation guard,
     * not a flush: if buffering is active but no buffer exists yet, it
     * invokes the virtual AllocateDefaultBuffer() hook. Returns 1 if a
     * buffer is available (already present or just allocated), -1 if
     * allocation failed, 0 only if buffering is inactive (unbuffered_) —
     * matches the derived callers' `if (rc == -1) return -1;` contract. */
    int32_t CheckFlush();

    /* WNDPROC_StreamBuf_SetBuffer, 0x465730. Low-level buffer-pointer
     * setter: frees any previously owned buffer, then installs the new
     * bufferStart_/bufferEnd_/ownsBuffer_. */
    void SetBufferPtrs(uint8_t* bufferStart, uint8_t* bufferEnd, int32_t owns);

    int32_t ownsBuffer_;
    int32_t unbuffered_;
    int32_t reserved0C_;
    uint8_t* bufferStart_;
    uint8_t* bufferEnd_;
    uint8_t* writeBase_;
    uint8_t* writePtr_;
    uint8_t* writeHigh_;
    uint8_t* readBase_;
    uint8_t* readPtr_;
    uint8_t* readHigh_;
    int32_t syncActive_;
    CRITICAL_SECTION cs_;
};
