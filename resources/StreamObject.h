/**
 * StreamObject.h — shared formatted-I/O state block (ios-equivalent)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly.
 *
 * StreamObject is the classic-old-iostream "ios"-equivalent state block:
 * an associated WNDPROC_StreamBuf, error-state bits, format flags, a
 * formatted-extraction width limit, an optional tied stream, and a lock
 * (sync flag + CRITICAL_SECTION). It is reached from WNDPROC_Stream (see
 * WndProcStream.h) as a virtual base — WNDPROC_Stream::InputPrefix/
 * SkipWhitespace/ExtractToken/Flush recompute the byte offset to this
 * subobject on every access via a classic MSVC vbtable lookup
 * (*(void**)this is a vbtable, slot [1] gives the byte distance from
 * `this` to here) rather than a hardcoded constant — evidence the same
 * code is shared by more than one concrete facade type.
 *
 * Originally only offsets +0x34 (sync_flag) and +0x38 (critical_section)
 * were evidenced (via CGWND_AboutDialog_LoadCredits's direct, non-virtual
 * StreamObject_Lock/Unlock calls). The remaining named fields below were
 * recovered while reverse engineering WNDPROC_Stream's InputPrefix/
 * SkipWhitespace/ExtractToken/Flush (0x4648F0, 0x464B10, 0x4649F0,
 * 0x465960) — every one of those functions reaches this exact struct
 * shape through the vbtable lookup and reads/writes these offsets.
 *
 * ~StreamObject() (0x464620, Ghidra-labeled "WNDPROC_StreamCleanup") is
 * this class's real destructor body — confirmed by tracing every one of
 * WIN32_StreamDestroy's (0x463A80, see Win32Stream.h) ~15 real callers in
 * the original binary: each constructs a WIN32_Stream in a raw local
 * buffer via WIN32_StreamOpen(&buf, 1), then on cleanup calls
 * WIN32_StreamDestroy(&buf.StreamObject_subobject) immediately followed
 * by WNDPROC_StreamCleanup(&buf.StreamObject_subobject) on the exact same
 * pointer. WIN32_StreamDestroy (0x463A80) itself does no cleanup: its 4
 * instructions read this object's own vptr, take vbtable slot [1] (the
 * byte offset from a WNDPROC_Stream-family object's own identity down to
 * this virtual base), and re-tag that slot's vptr to the next-more-
 * specific-than-WIN32_Stream identity before tail-calling the next
 * descent step (WNDPROC_Stream_DtorVftableReset, 0x4648E0) — classic MSVC
 * "walk down re-tagging each virtual-base vptr as the destructor chain
 * unwinds" bookkeeping, exactly analogous to WIN32_OStream_DtorVftableReset/
 * WNDPROC_OStream_DtorVftableReset documented in WndProcOStream.h. Real
 * C++ virtual-base destruction ordering provides the same guarantee for
 * free, so it is not reproduced — see Win32Stream.h's WIN32_StreamDestroy
 * doc comment for the address map of that whole re-tagging chain.
 * WNDPROC_StreamCleanup (0x464620), by contrast, is genuine cleanup logic
 * (frees an owned rdbuf, tears down the lock, decrements the shared
 * refcount) and IS reproduced below, as this class's one real destructor.
 *
 * Field layout:
 *   +0x00: vtable ptr (compiler-managed; not stored here — confirmed a
 *          real vptr, not padding, by WIN32_StreamDestroy's re-tagging
 *          chain above, which pokes exactly this slot)
 *   +0x04: rdbuf          WNDPROC_StreamBuf* — this stream's own buffer
 *   +0x08: state_bits     eofbit(1) | failbit(2) | badbit(4)
 *   +0x0C..0x1F: unknown — never read by any function examined so far
 *   +0x20: tied           WNDPROC_Stream* — flushed before a blocking read
 *                          when this object's own rdbuf doesn't already
 *                          have enough buffered data (classic ios::tie())
 *   +0x24: format_flags   bit0 = skipws (skip leading whitespace before
 *                          formatted extraction)
 *   +0x25..0x2F: unknown — never read by any function examined so far
 *   +0x30: width           formatted-extraction character limit, compared
 *                          UNSIGNED as (width - 1) against the running
 *                          character count. width == 0 (never set) wraps
 *                          to UINT32_MAX, i.e. effectively unlimited —
 *                          the classic C++98 istream "0 = unbounded"
 *                          behavior, but arrived at via unsigned
 *                          wraparound rather than an explicit check.
 *                          width == 1 gives a zero character budget, so
 *                          the very next extraction fails immediately
 *                          with zero characters read. Reset to 0 after
 *                          each use.
 *   +0x34: sync_flag       negative = use critical_section (pre-existing)
 *   +0x38: critical_section  Win32 CRITICAL_SECTION (pre-existing)
 *
 * Used directly (non-virtually) by: CGWND_AboutDialog_LoadCredits, via
 * StreamObject_Lock/Unlock below.
 * Used as a virtual base by: WNDPROC_Stream (see WndProcStream.h).
 */

#pragma once

#include "../shared/types.h"

// Status: VALIDATED
/* compat.h is force-included for the native build; undefine the three
 * duplicate macro definitions before the stub Win32 header supplies them. */
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

#include "WndProcStreamBuf.h"

class WNDPROC_Stream; /* WndProcStream.h — forward declared to avoid a
                        * circular include (WndProcStream.h derives from
                        * this struct). */

/* ================================================================== */
/* StreamObject layout                                                 */
/* ================================================================== */
struct StreamObject {
    /* +0x00 vtable ptr — compiler-managed (this class is polymorphic
     * because ~StreamObject() below is virtual; see the class doc comment
     * above for the evidence this slot is real, not unused padding). */
    WNDPROC_StreamBuf*  rdbuf;              /* +0x04 */
    uint32_t            state_bits;         /* +0x08 */
    uint8_t             _unknown_0C[0x10];  /* +0x0C..+0x1B */
    int32_t             owns_rdbuf;         /* +0x1C — nonzero iff rdbuf was
                                             * heap-allocated by this object
                                             * and must be freed on replacement */
    WNDPROC_Stream*     tied;               /* +0x20 — stream flushed before
                                             * blocking read (classic ios::tie) */
    uint8_t             format_flags;       /* +0x24 */
    uint8_t             _unknown_25[0x03];  /* +0x25..+0x27 */
    uint32_t            precision;          /* +0x28 — formatted output
                                             * precision (initialized to 6 by
                                             * StreamObject_Ctor) */
    uint8_t             fill;               /* +0x2C — formatted output fill
                                             * character (initialized to ' ' by
                                             * StreamObject_Ctor) */
    uint8_t             _unknown_2D[0x03];  /* +0x2D..+0x2F */
    int32_t             width;              /* +0x30 — formatted extraction
                                             * character limit */
    int32_t             sync_flag;          /* +0x34 */
    CRITICAL_SECTION    critical_section;   /* +0x38 */

    /* state_bits values (classic ios::iostate) */
    static constexpr uint32_t kEofBit  = 0x1;
    static constexpr uint32_t kFailBit = 0x2;
    static constexpr uint32_t kBadBit  = 0x4;

    /* format_flags bits. kSkipws is the only bit ever written anywhere in
     * this codebase (WNDPROC_Stream::AttachBuffer) -- kFmtDec/kFmtHex/
     * kFmtOct are read-only evidence from StreamBuf_ReadString's numeric-
     * token radix selection (0x465AD0, see WndProcStream.cpp's
     * ReadNumericToken) but have no writer anywhere in the binary, so a
     * numeric extraction's radix is always auto-detected (0), exactly like
     * strtol/strtoul's own base==0 contract. Kept as real, named bits
     * rather than collapsed away, since ReadNumericToken is a genuine,
     * reachable branch on them even though it is never observed to fire
     * with anything but the "unset" (auto) combination in this binary. */
    static constexpr uint8_t kSkipws = 0x1;
    static constexpr uint8_t kFmtDec = 0x10;
    static constexpr uint8_t kFmtOct = 0x20;
    static constexpr uint8_t kFmtHex = 0x40;

    /* 0x464590 (Ghidra auto-analysis named this "WNDPROC_StreamGetSize";
     * it does not get any size — it default-initializes this virtual base:
     * rdbuf/tied null, badbit set (no buffer attached yet), owns_rdbuf
     * clear, precision/fill at their classic-iostream defaults, width/
     * sync_flag/critical_section as documented above. Also lazily
     * initializes a process-global CRITICAL_SECTION shared across every
     * StreamObject, guarded by a global refcount — see the .cpp file. The
     * original additionally pokes this object's own vtable pointer to a
     * standalone "StreamObject alone" vtable here (classic MSVC
     * base-before-derived vptr sequencing); real C++ virtual-base
     * construction ordering already provides that, so it is not
     * reproduced. */
    StreamObject();

    /* 0x464620 (Ghidra auto-analysis named this "WNDPROC_StreamCleanup"; it
     * is this class's real destructor body — see the class doc comment
     * above for the full evidence trail distinguishing it from
     * WIN32_StreamDestroy's pure vptr-retagging). Resets sync_flag to -1,
     * tears down this object's own critical_section, decrements the
     * process-global StreamObject refcount (tearing down the shared
     * global critical_section when it reaches zero), deletes the owned
     * rdbuf if any (mirrors AttachBuffer's identical ownership check),
     * then clears rdbuf and resets state_bits to badbit — matching the
     * original's exact field-write ordering. Virtual so that deleting
     * through any less-derived pointer into this virtual base still runs
     * this body, matching the original's own polymorphic vtable-slot
     * dispatch evidence. */
    virtual ~StreamObject();

protected:
    /* 0x464680 (Ghidra auto-analysis named this "WNDPROC_StreamFlush"; it
     * does not flush anything — it replaces the attached buffer). If this
     * object owns its current rdbuf, deletes it (the original does this by
     * calling the old buffer's scalar-deleting-destructor vtable slot; real
     * C++ `delete` through WNDPROC_StreamBuf's virtual destructor is the
     * same operation). Installs `newBuf` as the new rdbuf and clears/sets
     * badbit depending on whether it is null. Does NOT touch owns_rdbuf —
     * every caller sets that separately (see Win32Stream.cpp). */
    void AttachBuffer(WNDPROC_StreamBuf* newBuf);
};

/* ================================================================== */
/* StreamObject_Lock — Enter CRITICAL_SECTION if sync is active       */
/* Address: 0x410240                                                   */
/*                                                                     */
/* If stream->sync_flag (+0x34) is negative (bit 31 set), enters the   */
/* CRITICAL_SECTION at stream+0x38. Otherwise does nothing.            */
/*                                                                     */
/* Called by: CGWND_AboutDialog_LoadCredits (0x4101AA)                 */
/*                                                                     */
/* @param stream  Pointer to stream object                            */
/* ================================================================== */
void __cdecl StreamObject_Lock(StreamObject* stream);

/* ================================================================== */
/* StreamObject_Unlock — Leave CRITICAL_SECTION if sync is active     */
/* Address: 0x410260                                                   */
/*                                                                     */
/* If stream->sync_flag (+0x34) is negative (bit 31 set), leaves the   */
/* CRITICAL_SECTION at stream+0x38. Otherwise does nothing.            */
/*                                                                     */
/* Called by: CGWND_AboutDialog_LoadCredits (0x4101C5)                 */
/*                                                                     */
/* @param stream  Pointer to stream object                            */
/* ================================================================== */
void __cdecl StreamObject_Unlock(StreamObject* stream);
