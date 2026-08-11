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
 *   StreamBuf_ReadChar                    0x4652A0  -> ReadChar()  (peek,
 *     found + validated while reverse engineering WNDPROC_Stream — see
 *     WndProcStream.h)
 *   StreamBuf_GetChar                     0x4651A0  -> GetChar()  (get +
 *     advance; same discovery as ReadChar())
 *   WNDPROC_StreamBuf_ReadBytes           0x4655C0  -> ReadBytes(void*,int32_t)
 *     (vtable+0x18; had no Ghidra function defined at all before this pass —
 *     found by reading WIN32_StreamFile's real vtable bytes at 0x4791AC and
 *     decompiling the dword at +0x18. Discovered while reverse engineering
 *     WIN32_StreamRead, see Win32Stream.cpp.)
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
 *   +0x0C peekCache_            get-side one-character cache, initialized
 *                                to -1 ("empty"). Read/written only by the
 *                                newly reconstructed ReadChar()/GetChar()
 *                                below (0x4652A0/0x4651A0, found while
 *                                reverse engineering WNDPROC_Stream — see
 *                                WndProcStream.h) — not read anywhere in
 *                                Ctor/DtorBody/CheckFlush/SetBuffer or
 *                                WIN32_StreamFile's WriteChar/Flush/
 *                                SetBuffer/CloseHandle, confirming it is
 *                                exclusively an unbuffered_-mode peek slot,
 *                                not a general pushback/ungetc buffer.
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

    /* vtable +0x18. Bulk read: fills `buf` with up to `size` bytes, draining
     * the buffered get-region and calling the Underflow() hook to refill (or
     * to fetch one byte at a time in unbuffered_ mode) until `size` bytes are
     * read or Underflow() reports EOF/error. Returns the number of bytes
     * actually read (may be less than `size` on EOF). No override for this
     * slot was found in WIN32_StreamFile's own vtable (0x4791AC dword at
     * +0x18 points at this exact base-class address) — the base
     * implementation, driven entirely through the virtual Underflow() hook,
     * is what every concrete stream type uses. Discovered while reverse
     * engineering the WIN32_StreamRead cluster (see Win32Stream.cpp), which
     * dispatches through this slot. */
    virtual int32_t ReadBytes(void* buf, int32_t size);

    /* vtable +0x1C ("overflow" hook proper). WIN32_StreamFile overrides this
     * with its real WriteChar (0x463CB0). */
    virtual int32_t WriteChar(int32_t ch) = 0;

    /* vtable +0x20 ("underflow"-equivalent get-side refill/peek hook,
     * called by ReadChar()/GetChar() below). No base implementation
     * address has been located — WIN32_StreamFile.h documents its
     * WriteChar/Flush/SetBuffer overrides but not this slot, so its real
     * override address is still unconfirmed. TODO: locate and decompile
     * WIN32_StreamFile's vtable+0x20 override. */
    virtual int32_t Underflow() = 0;

    /* vtable +0x28 ("doallocate"). Base class supplies a real, concrete
     * implementation (0x4656F0); WIN32_StreamFile does not override this
     * slot (confirmed by direct vtable read — see file header). */
    virtual int32_t AllocateDefaultBuffer();

    /* StreamBuf_ReadChar, 0x4652A0. Peeks the next character without
     * consuming it: unbuffered mode caches the result in peekCache_ (so
     * repeated peeks don't re-invoke Underflow()); buffered mode just
     * calls Underflow() every time (which itself refills/peeks via
     * readPtr_/readHigh_ without advancing). Returns -1 on EOF/error. */
    int32_t ReadChar();

    /* StreamBuf_GetChar, 0x4651A0. Reads and consumes the next character,
     * advancing the read cursor. Unbuffered mode always forces a fresh
     * Underflow() call (unlike ReadChar(), which reuses a cached peek).
     * Buffered mode refills via Underflow() when the get-region is
     * exhausted, advances readPtr_, and returns *readPtr_ (or calls
     * Underflow() once more if still exhausted). Returns -1 on EOF/error. */
    uint32_t GetChar();

    /* Bytes already buffered and available to read without a fresh
     * Underflow() call. Matches the space check WNDPROC_Stream::
     * InputPrefix performs inline before deciding whether to flush a
     * tied stream (readHigh_ - readPtr_, floored at 0). */
    int32_t AvailableToRead() const {
        return (readPtr_ < readHigh_) ? static_cast<int32_t>(readHigh_ - readPtr_) : 0;
    }

    /* Enter/leave this buffer's own CRITICAL_SECTION if syncActive_ is
     * negative. Matches the StreamObject_Lock/Unlock pattern (StreamObject.h)
     * applied to this class's own syncActive_/cs_ pair; inlined at every
     * call site in the original (InputPrefix, SkipWhitespace, Flush — see
     * WndProcStream.h) rather than calling out to a shared helper there,
     * but consolidated here since WNDPROC_Stream can't reach the
     * protected syncActive_/cs_ fields directly. */
    void Lock();
    void Unlock();

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
    int32_t peekCache_;
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
