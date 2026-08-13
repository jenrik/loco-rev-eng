/**
 * WndProcOStream.h — write-only WNDPROC_Stream sibling ("ostream"-analog)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly.
 *
 * WNDPROC_OStream sits above the same StreamObject virtual base as
 * WNDPROC_Stream (see WndProcStream.h/StreamObject.h) and uses the exact
 * same MSVC vbtable-lookup mechanics, but it is a GENUINELY DISTINCT class
 * from WNDPROC_Stream — not a second vtable-poke variant of it. Evidence:
 *
 *   - WNDPROC_Stream's own vbtable is [self=0, vbase=0xC] (read at
 *     0x479238, the "most-derived" vtable WNDPROC_Stream::AttachBuffer
 *     pokes into `*this` on its initBase branch — see WndProcStream.h). This
 *     class's own vbtable is [self=0, vbase=0x8] (read at 0x479288, the
 *     equivalent poke inside THIS class's AttachBuffer, 0x465A30) —
 *     confirmed byte-for-byte via read_bytes, not inferred. Four fewer
 *     bytes of own data before the virtual base.
 *   - That matches field-for-field: WNDPROC_Stream owns TWO dwords before
 *     StreamObject (_reserved_04 @+4, gcount_ @+8 — WndProcStream.h);
 *     WNDPROC_Stream::AttachBuffer zeroes both. THIS class's AttachBuffer
 *     (0x465A30) zeroes only ONE dword (@+4) — there is no gcount_
 *     equivalent, consistent with a write-only facade that never counts
 *     unformatted-read bytes.
 *   - WNDPROC_Stream::AttachBuffer additionally sets the istream-only
 *     skipws format flag (StreamObject::format_flags |= kSkipws); this
 *     class's AttachBuffer does not touch format_flags at all — there is
 *     no formatted-extraction concept on a write-only facade.
 *   - Confirmed end-to-end via allocation size at the one real construction
 *     site: WIN32_Stream (WNDPROC_Stream-derived, vbase offset 0xC) is
 *     documented as sizeof 0x5C on the original x86. This class's concrete
 *     subclass (WIN32_OStream, see Win32OStream.h) is allocated with
 *     `operator_new(0x58)` at its one real call site
 *     (RESMGR_LoadResourceData, 0x447E63/0x447E8F) — exactly 4 bytes
 *     smaller, matching the vbtable evidence above exactly (0x8 vbase
 *     offset + 0x50 StreamObject size = 0x58).
 *
 * Address map:
 *   AttachBuffer(WNDPROC_StreamBuf*) 0x465A30 -> previously mislabeled
 *     "CRT_except_handler" by Ghidra auto-analysis (it is not an SEH
 *     filter — same class of auto-analysis artifact as "CRT_floor"/
 *     "CRT_exp" elsewhere in this stream family; see Win32OStream.h).
 *
 * The destructor-side vtable-descent helpers this class's construction
 * path implies are pure MSVC ABI bookkeeping for locating/re-tagging the
 * virtual base as destruction unwinds through each level of the hierarchy
 * — real C++ virtual inheritance and a real virtual destructor on
 * StreamObject already provide the equivalent, so they are not reproduced
 * here (same reasoning as StreamObject::StreamObject()'s doc comment).
 * Identified and named in Ghidra, documented not reimplemented (see
 * shared/vtable_addrs.h's VTBL_WNDPROC_OSTREAM* and VTBL_WIN32_OSTREAM*
 * entries for the exact vtable-slot wiring):
 *   WIN32_OStream_ScalarDtor          0x465060  (WIN32_OStream-view thunk)
 *   WIN32_OStream_DtorVftableReset    0x465180  (steps down to WNDPROC_OStream)
 *   WNDPROC_OStream_ScalarDtor        0x465A00  (WNDPROC_OStream-view thunk)
 *   WNDPROC_OStream_DtorVftableReset  0x465AC0  (steps down to its own identity)
 *
 * No caller anywhere in the binary constructs a bare WNDPROC_OStream (no
 * xrefs to 0x465A30 other than WIN32_OStream's own constructor) or calls
 * any other method through this facade alone.
 */

// Status: VALIDATED (matches sibling WNDPROC_Stream/WIN32_Stream: the
// StreamObject virtual base's own real destructor-body behavior — freeing
// an owned rdbuf via WNDPROC_StreamCleanup, 0x464620 — is not yet expressed
// as a real ~StreamObject() in StreamObject.h, an identical, pre-existing
// gap on both hierarchies; not fixed here as it is shared, cross-cutting
// infrastructure outside the one constructor this pass adds).

#pragma once

#include "StreamObject.h"

class WNDPROC_OStream : public virtual StreamObject {
protected:
    /* 0x465A30 (Ghidra auto-analysis named this "CRT_except_handler"; it is
     * not an SEH filter — it attaches a buffer, the write-only sibling of
     * WNDPROC_Stream::AttachBuffer, see WndProcStream.h/.cpp). Calls the
     * inherited StreamObject::AttachBuffer(newBuf), then zeroes
     * _reserved_04. Does NOT set owns_rdbuf (callers do that themselves,
     * same convention as WNDPROC_Stream::AttachBuffer) and does NOT touch
     * StreamObject::format_flags — no skipws-equivalent concept here. */
    void AttachBuffer(WNDPROC_StreamBuf* newBuf);

private:
    /* +0x04 on this facade. Zeroed alongside by AttachBuffer (0x465A30);
     * no read of this field has been found anywhere in the evidenced call
     * graph — same unconfirmed-purpose placeholder as WNDPROC_Stream's own
     * _reserved_04 (WndProcStream.h), kept named rather than folded into
     * padding so AttachBuffer's write is reproduced faithfully. */
    int32_t _reserved_04;
};
