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
 * This reconstruction now also covers the WIN32_StreamRead cluster's
 * facade-level pieces (Read/AttachBuffer, see below) confirmed to operate
 * on this exact class's fields — see resources/Win32Stream.h/.cpp for the
 * WIN32_StreamFile-specific layer built on top. The remaining part of
 * PROGRESS.md's "WNDPROC_Stream* facade" item (0x4640B0-0x464D70's
 * WNDPROC_StreamOpen/FromMemory/Seek/Tell/GetSize/Cleanup/ReadLine/
 * SeekForward) is still open — a future pass reconstructing that must
 * reconcile with (not duplicate) this class.
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
 *   Read(void*,uint32_t)       0x463810  -> "WIN32_StreamRead" at the call
 *     sites, but operates only on this facade's own state (see below)
 *   AttachBuffer(WNDPROC_StreamBuf*) 0x464840 -> previously mislabeled
 *     "WNDPROC_StreamVPrintf"
 *
 * All validated instruction-by-instruction against disassembly.
 */

// Status: VALIDATED

#pragma once

#include <cstddef>

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

    /* 0x463810 (Ghidra/callers auto-named this "WIN32_StreamRead", but it
     * operates purely on WNDPROC_Stream/StreamObject-level state — rdbuf,
     * gcount_, state_bits, sync_flag/critical_section — with nothing
     * WIN32_StreamFile-specific, so it belongs on this facade like
     * InputPrefix/ExtractToken/Flush above, not on WIN32_Stream). Calls
     * InputPrefix(1) (locks both this object's and rdbuf's CRITICAL_
     * SECTIONs, leaving them held on success per InputPrefix's documented
     * contract), then rdbuf->ReadBytes(buf, size) (WNDPROC_StreamBuf
     * vtable+0x18), stores the count read into gcount_, sets failbit|eofbit
     * on a short read, and performs the unlock InputPrefix left pending.
     * If InputPrefix itself fails (stream already bad), does nothing
     * further — matches the original, which never reaches the read at all
     * in that case. Returns `this` either way. */
    WNDPROC_Stream* Read(void* buf, uint32_t size);

    /* Classic istream::gcount() accessor for the private gcount_ field
     * below — added so real callers (e.g. input/Cursor.cpp's palette
     * loader) can read the byte count from the last Read() without a raw
     * offset read (`streamObj[2]` on the original x86 layout, a landmine
     * on this host since preceding pointer-bearing members widen). */
    int32_t Gcount() const { return gcount_; }

    /* 0x4646C0 (real C++-mangled method — operator>>(int32_t*) for
     * SIGNED integers). Re-enters InputPrefix(0)/ReadNumericToken()
     * itself (CRITICAL_SECTIONs are reentrant, matching the original's
     * own nested Enter/Leave pairs), converts the resulting token via the
     * CRT's signed strtol-family primitive, and sets failbit
     * UNCONDITIONALLY whenever that conversion overflows (errno==ERANGE)
     * — unlike ExtractUnsignedInt() below, which only consults errno for
     * its own ambiguous overflow-clamp sentinel. Always returns `this`
     * (classic operator>> chaining), regardless of success. Real caller:
     * ResourceEntry::Parse's "ResourceReplayDelay" branch
     * (resources/ResourceManager.cpp). */
    WNDPROC_Stream* ExtractInt(int32_t* out);

    /* 0x464F70 (Ghidra auto-analysis mislabeled this "CRT_fabs" — NOT
     * floating-point absolute value; disassembly shows it operates
     * purely on stream/StreamObject state and calls the CRT's UNSIGNED
     * strtoul-family primitive, the same 0x469560 numeric-parse routine
     * ExtractInt() uses with its "unsigned" flag set). Genuinely distinct
     * from ExtractInt(), not a duplicate: only checks errno==ERANGE when
     * the parsed result is exactly -1 (0xFFFFFFFF, the unsigned overflow-
     * clamp sentinel) — any other return value cannot be an overflow
     * result for this variant, so skipping the errno check for those is
     * a genuine, intentional CRT optimization, not dead code. Real
     * caller: ResourceEntry::Parse's "MaxInstances" branch
     * (resources/ResourceManager.cpp). */
    WNDPROC_Stream* ExtractUnsignedInt(int32_t* out);

protected:
    /* 0x464840 (Ghidra auto-analysis named this "WNDPROC_StreamVPrintf"; it
     * is not printf-related — it attaches a buffer). Calls the inherited
     * StreamObject::AttachBuffer(newBuf), then sets format_flags's skipws
     * bit (classic istream default: skip leading whitespace before
     * formatted extraction), and zeroes gcount_ and _reserved_04. Does NOT
     * set owns_rdbuf — WIN32_Stream's constructors do that themselves,
     * matching the original (the original's own initBase/vtable-poke
     * branch is MSVC most-derived-vs-base-subobject construction
     * bookkeeping; real C++ virtual-base construction ordering already
     * provides the equivalent, so it is not reproduced here — see
     * StreamObject::StreamObject()'s doc comment for the same reasoning). */
    void AttachBuffer(WNDPROC_StreamBuf* newBuf);

private:
    /* 0x465AD0 (Ghidra AND shared/crt_stubs.cpp both mislabel this
     * "_ftol"/"CRT ftol" — a genuinely distinct 511-byte function, not
     * the tiny FPU-based double-to-long intrinsic; see crt_stubs.cpp's
     * own doc comment for that still-outstanding collision). Classic
     * old-iostream numeric-token tokenizer, shared by ExtractInt() and
     * ExtractUnsignedInt() above: re-enters InputPrefix(0) itself, then
     * consumes a run of characters honoring an optional leading sign and
     * an auto-detected "0x"/"0" radix prefix (this codebase never sets
     * the explicit kFmtDec/kFmtHex/kFmtOct format_flags bits anywhere —
     * see StreamObject.h — so radix is always auto-detected exactly like
     * strtol/strtoul's own base==0 contract) into `buf`, bounded to the
     * classic 15-significant-character old-iostream cap. On total
     * failure (zero valid digits consumed), sets failbit and pushes
     * every consumed character back onto rdbuf via PutBack()
     * (WndProcStreamBuf.h). Returns the detected radix (0/8/10/16) for
     * the caller to hand to strtol/strtoul. `bufCapacity` is always 16 at
     * both real call sites, matching the original's fixed 16-byte stack
     * buffer.
     * NOTE: the original's final `if (finalLength == 0x10) failbit=true`
     * safety check is provably unreachable (the loop's own hard cap
     * guarantees finalLength <= 15) and is not reproduced — see the .cpp
     * for the reachability proof. */
    uint32_t ReadNumericToken(char* buf, size_t bufCapacity);

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
     * the same basic blocks, proving they're distinct storage). Confirmed
     * to be the classic istream::gcount_: InputPrefix resets it to 0 on
     * every unformatted-read call, and Read() (0x463810) above stores the
     * actual byte count read into it — the only two writers found. */
    int32_t gcount_;

    /* +0x04 on this facade. Zeroed alongside gcount_ by AttachBuffer
     * (0x464840); no read of this field has been found anywhere in the
     * evidenced call graph. Kept as a named placeholder (not folded into
     * padding) so AttachBuffer's write is reproduced faithfully; purpose
     * unconfirmed. */
    int32_t _reserved_04;
};

/* Free-function adapter for the pre-existing `WNDPROC_CriticalSectionLock`
 * symbol that `game/TrainStation.cpp`, `input/BuildingDescriptorEditor.cpp`,
 * and `ui/UI_ChildWindow.cpp` declare (C++-mangled linkage,
 * `_Z27WNDPROC_CriticalSectionLockPiPc`) and call against their own
 * `int* stream` handles.
 *
 * INTENDED to forward to ExtractToken() on the assumption `stream` is
 * really a `WNDPROC_Stream*` — but gdb-confirmed SIGSEGV (2026-08-10,
 * see PROGRESS.md "WNDPROC_Stream facade recovery" postmortem) proved
 * that assumption false at the real call sites: `stream` there is a raw,
 * never-constructed `int[2]` handle from the still-unimplemented
 * `WIN32_StreamOpenPath`, not an object with the vtable this class's
 * virtual base (StreamObject) needs. The .cpp definition is currently a
 * loud deferred stub for that reason — see its doc comment for the full
 * postmortem and what a real fix requires. Declared with the EXACT
 * signature those files use so the mangled name matches and the linker
 * binds here. NOT declared `extern "C"` — must keep C++ linkage to
 * match. */
void WNDPROC_CriticalSectionLock(int* stream, char* buf);

/* Free-function `extern "C"` adapter for the pre-existing
 * `WNDPROC_Stream__ExtractInt` symbol `input/TrackTileDescriptor.cpp`
 * declares (`extern "C" void* WNDPROC_Stream__ExtractInt(void*,
 * int32_t*);`) and calls on the same `stream` parameter as
 * WNDPROC_CriticalSectionLock above — real callers there always pass a
 * genuine, already-constructed WNDPROC_Stream-family object (a
 * WIN32_Stream local or a WNDPROC_StreamFromMemory result; confirmed by
 * reading TrackTileDescriptor::Render's own call sites), so forwarding
 * to the real WNDPROC_Stream::ExtractInt() is safe, matching
 * WNDPROC_CriticalSectionLock's own established precedent. Declared
 * here (rather than left as a bare definition) to satisfy
 * -Wmissing-declarations at -Dstrict=2. */
extern "C" void* WNDPROC_Stream__ExtractInt(void* stream, int32_t* out);
