/**
 * WndProcStream.h — formatted stream I/O facade (classic old-iostream
 * ipfx/eatwhite/operator>>/flush pattern, reimplemented on top of
 * WNDPROC_StreamBuf/StreamObject)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly.
 *
 * WNDPROC_Stream sits above a WNDPROC_StreamBuf (see WndProcStreamBuf.h)
 * and reaches its lock/error-state/format-flags/width block through a
 * StreamObject virtual base (see StreamObject.h) via a classic MSVC
 * vbtable lookup: *(void**)this is a vbtable, and vbtable slot [1] (a
 * byte offset) gives the distance from `this` to the StreamObject
 * subobject. Every method below recomputes that offset on every access
 * rather than using a fixed constant — direct evidence the same code is
 * shared by more than one concrete facade type (mirroring the classic
 * ios/istream/ostream/iostream diamond, though no evidence here confirms
 * loco.exe's hierarchy matches that shape exactly).
 *
 * This class is behaviorally named after the closest well-known analogue
 * (classic pre-standard <iostream.h> ipfx/eatwhite/operator>>/flush) for
 * documentation clarity — no debug symbols survive to confirm the
 * original class or method names.
 *
 * This is a MINIMAL, PARTIAL reconstruction of the still-open
 * "WNDPROC_Stream* facade" PROGRESS.md tracks (0x4640B0-0x464D70:
 * WNDPROC_StreamRead/Write/Printf/ReadLine/SeekForward, still no-op host
 * stubs — see PROGRESS.md "win32_stream.c removed (partial)"). It
 * captures only what the InputPrefix/SkipWhitespace/ExtractToken/Flush
 * cluster proves. A future pass reconstructing that larger facade must
 * reconcile with (not duplicate) this class — in particular, confirm
 * whether `this` at 0x4640B0-0x464D70's call sites is really the same
 * concrete type as this class, or a sibling that shares the same
 * StreamObject virtual base.
 *
 * Address map:
 *   ios::ipfx-equivalent       0x4648F0  -> InputPrefix(int)
 *   ios::eatwhite-equivalent   0x464B10  -> SkipWhitespace() (private,
 *                                            only called from InputPrefix)
 *   operator>>(char*)          0x4649F0  -> ExtractToken(char*)
 *   ios::flush-equivalent      0x465960  -> Flush() (previously mislabeled
 *     "StreamBuf_FlushOrPut" as though it were a WNDPROC_StreamBuf method;
 *     it exhibits the identical vbtable/StreamObject-lookup pattern as the
 *     three functions above operating on its own `this`, so it belongs on
 *     this facade, not on WNDPROC_StreamBuf — renamed accordingly)
 *
 * All four validated instruction-by-instruction against disassembly.
 */

// Status: VALIDATED

#pragma once

#include "StreamObject.h"

class WNDPROC_Stream : public virtual StreamObject {
public:
    /* 0x4648F0. Classic old-iostream "input prefix", called before every
     * extraction:
     *   1. Locks this object's own StreamObject lock.
     *   2. If need != 0 (unformatted-read callers: get/read/getline-style),
     *      resets gcount_ to 0. NOTE: gcount_ lives on this facade at
     *      +0x08, a DIFFERENT address from StreamObject::state_bits (also
     *      +0x08, but relative to the virtual base) — confirmed distinct
     *      via disassembly, not a duplicate read of the same field.
     *   3. If state_bits is already nonzero (a prior failure), sets
     *      failbit and bails out (unlocking on the way).
     *   4. If a tied stream exists: flushes it unconditionally when
     *      need == 0 (formatted extraction), or only when rdbuf doesn't
     *      already have `need` bytes buffered (AvailableToRead() below
     *      `need`) for unformatted reads.
     *   5. Locks rdbuf's own CRITICAL_SECTION.
     *   6. For formatted extraction (need == 0) with skipws set, calls
     *      SkipWhitespace() and re-checks state_bits.
     * Returns 1 if the stream is ready for extraction, 0 if it was
     * already (or just became) fail/bad — releasing both locks first. */
    int InputPrefix(int need);

    /* 0x4649F0. operator>>(char*)-equivalent: extracts one whitespace-
     * delimited token into buffer, bounded by (width - 1) characters,
     * compared UNSIGNED. width == 0 (never set) wraps to UINT32_MAX —
     * effectively unlimited, the classic "0 = unbounded" istream
     * behavior, arrived at via unsigned wraparound rather than an
     * explicit check. width == 1 gives a zero character budget and an
     * IMMEDIATE failure with zero characters read. On success,
     * null-terminates buffer and returns `this`. On failure
     * (buffer == nullptr, width == 1, immediate EOF, or an immediate
     * whitespace character) sets failbit (and badbit/eofbit as
     * appropriate) and leaves buffer completely UNTOUCHED — no null
     * terminator is written in the failure path — verified against
     * disassembly. Always resets width to 0 after use, win or lose. */
    void* ExtractToken(char* buffer);

    /* 0x465960 (previously mislabeled StreamBuf_FlushOrPut). Locks this
     * object's own and rdbuf's locks, calls rdbuf's virtual Flush() hook
     * (WNDPROC_StreamBuf vtable+0x04), sets failbit if that returns -1,
     * unlocks both, returns `this`. */
    void* Flush();

private:
    /* 0x464B10. Discards characters from rdbuf (via ReadChar()/GetChar())
     * while they're whitespace, stopping at the first non-space char or
     * EOF. Sets eofbit if the stream runs dry. Locks/unlocks rdbuf's own
     * CRITICAL_SECTION around the read loop, and additionally locks this
     * object's own lock only around the eofbit write. Only called from
     * InputPrefix(0) when skipws is set. */
    void SkipWhitespace();

    /* +0x08 on this facade (NOT StreamObject::state_bits, which is also
     * nominally "+0x08" but relative to the virtual base at a different
     * runtime address — InputPrefix reads/writes both independently in
     * the same basic blocks, proving they're distinct storage). TODO:
     * likely the classic istream::gcount_ (only istream needs a per-read
     * character count; keeping it on the derived facade rather than the
     * StreamObject virtual base shared with an ostream-equivalent fits
     * that pattern) — no direct read of this field has been found yet
     * to confirm the "gcount" identity further, only the need != 0 write
     * in InputPrefix. */
    int32_t gcount_;
};

/* Real definition of the pre-existing `WNDPROC_CriticalSectionLock`
 * free-function symbol that `game/TrainStation.cpp`, `input/
 * BuildingDescriptorEditor.cpp`, and `ui/UI_ChildWindow.cpp` already
 * declare (C++-mangled linkage, `_Z27WNDPROC_CriticalSectionLockPiPc`)
 * and call against their own `int* stream` handles — those files predate
 * this class and never had a real definition to bind to; they were
 * calling a crashing `assert(0)` stub in shared/stubs_impl.cpp. This
 * thin adapter is the fix: `stream` is really a `WNDPROC_Stream*`, this
 * just forwards to ExtractToken(). Declared with the EXACT signature
 * those files use so the mangled name matches and the linker binds here
 * instead of (now-removed) stub. NOT declared `extern "C"` — must keep
 * C++ linkage to match. */
void WNDPROC_CriticalSectionLock(int* stream, char* buf);
