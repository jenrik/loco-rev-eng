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
 * Field layout:
 *   +0x00..0x03: unknown — never read by any function examined so far
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

// Status: TRANSCRIBED
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
    uint8_t             _unknown_00[0x04];  /* +0x00..+0x03 */
    WNDPROC_StreamBuf*  rdbuf;              /* +0x04 */
    uint32_t            state_bits;         /* +0x08 */
    uint8_t             _unknown_0C[0x14];  /* +0x0C..+0x1F */
    WNDPROC_Stream*     tied;               /* +0x20 */
    uint8_t             format_flags;       /* +0x24 */
    uint8_t             _unknown_25[0x0B];  /* +0x25..+0x2F */
    int32_t             width;              /* +0x30 */
    int32_t             sync_flag;          /* +0x34 */
    CRITICAL_SECTION    critical_section;   /* +0x38 */

    /* state_bits values (classic ios::iostate) */
    static constexpr uint32_t kEofBit  = 0x1;
    static constexpr uint32_t kFailBit = 0x2;
    static constexpr uint32_t kBadBit  = 0x4;

    /* format_flags bits */
    static constexpr uint8_t kSkipws = 0x1;
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
