/**
 * WndProcStream.cpp — WNDPROC_Stream implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly.
 *
 * See WndProcStream.h for the class/address map. This file transcribes:
 *   InputPrefix    0x4648F0
 *   SkipWhitespace 0x464B10
 *   ExtractToken   0x4649F0
 *   Flush          0x465960
 *   Read           0x463810
 *   AttachBuffer   0x464840
 */

// Status: VALIDATED

#include "WndProcStream.h"

#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

extern "C" {
/* Same family as WNDPROC_StreamBuf's Lock()/Unlock() (resources/
 * WndProcStreamBuf.cpp) — 0x464D90/0x464DA0, already declared/stubbed in
 * resources/StreamObject.cpp. */
void __stdcall WNDPROC_EnterCriticalSection(void* cs);
void __stdcall WNDPROC_LeaveCriticalSection(void* cs);

/* CRT character classification. */
int __cdecl _isspace(int c);

/* Stream buffer attachment via WNDPROC_StreamFlush (0x464680). */
void WNDPROC_StreamFlush(void* stream, int rdbuf_child);
}

/* ================================================================== */
/* AttachBuffer — 0x464840 (previously mislabeled WNDPROC_StreamVPrintf) */
/* ================================================================== */
void WNDPROC_Stream::AttachBuffer(WNDPROC_StreamBuf* newBuf)
{
    StreamObject::AttachBuffer(newBuf);
    format_flags |= kSkipws;
    gcount_ = 0;
    _reserved_04 = 0;
}

/* ================================================================== */
/* InputPrefix — 0x4648F0                                              */
/* ================================================================== */
int WNDPROC_Stream::InputPrefix(int need)
{
    if (sync_flag < 0) {
        WNDPROC_EnterCriticalSection(&critical_section);
    }

    if (need != 0) {
        gcount_ = 0;
    }

    if (state_bits != 0) {
        state_bits |= kFailBit;
        goto release_own;
    }

    if (tied != nullptr) {
        bool doFlush;
        if (need == 0) {
            doFlush = true;
        } else {
            int32_t avail = (rdbuf != nullptr) ? rdbuf->AvailableToRead() : 0;
            doFlush = (need > avail);
        }
        if (doFlush) {
            tied->Flush();
        }
    }

    if (rdbuf != nullptr) {
        rdbuf->Lock();
    }

    if (need == 0 && (format_flags & kSkipws) != 0) {
        SkipWhitespace();
        if (state_bits != 0) {
            state_bits |= kFailBit;
            goto release_rdbuf;
        }
    }

    /* Success: intentionally returns WITHOUT releasing either lock — both
     * stay held for the caller (ExtractToken()/Flush()) to use while it
     * performs the actual read/write, and to release itself when done.
     * Verified against disassembly: this exact return path (0x4649E1)
     * pops registers and returns 1 with no Leave*CriticalSection calls at
     * all, for all three ways of reaching it (need != 0; need == 0 with
     * skipws unset; need == 0 with skipws set and state_bits still 0
     * after SkipWhitespace()). */
    return 1;

release_rdbuf:
    if (rdbuf != nullptr) {
        rdbuf->Unlock();
    }
release_own:
    if (sync_flag < 0) {
        WNDPROC_LeaveCriticalSection(&critical_section);
    }
    return 0;
}

/* ================================================================== */
/* SkipWhitespace — 0x464B10                                           */
/* ================================================================== */
void WNDPROC_Stream::SkipWhitespace()
{
    if (rdbuf == nullptr) {
        return;
    }

    /* Locks rdbuf's own CRITICAL_SECTION around the whole read loop below
     * — a SECOND, independent lock/unlock of rdbuf, nested inside the one
     * InputPrefix already took and holds for the rest of the extraction.
     * Matches the original exactly (0x464B10-0x464B2E take it, released
     * again either at the "found non-whitespace" early exit or after the
     * EOF handling below — both paths funnel through the same release). */
    rdbuf->Lock();

    int32_t c = rdbuf->ReadChar();
    while (c != -1) {
        if (!_isspace(c)) {
            rdbuf->Unlock();
            return;
        }
        c = static_cast<int32_t>(rdbuf->GetChar());
    }

    /* EOF reached while skipping whitespace: set eofbit, guarded by this
     * object's own lock (a separate, narrower Enter/LeaveCriticalSection
     * pair around just this write, distinct from rdbuf's lock taken
     * above). */
    if (sync_flag < 0) {
        WNDPROC_EnterCriticalSection(&critical_section);
    }
    state_bits |= kEofBit;
    if (sync_flag < 0) {
        WNDPROC_LeaveCriticalSection(&critical_section);
    }

    rdbuf->Unlock();
}

/* ================================================================== */
/* ExtractToken — 0x4649F0 (operator>>(char*))                         */
/* ================================================================== */
void* WNDPROC_Stream::ExtractToken(char* buffer)
{
    if (!InputPrefix(0)) {
        return this;
    }

    /* width - 1, computed then compared as UNSIGNED (matches the original:
     * TEST/JBE and CMP/JC after the decrement both operate unsigned).
     * width == 0 (never set) wraps to UINT32_MAX, i.e. effectively
     * unlimited — verified against disassembly, this does NOT mean
     * "unlimited" via a documented special case, it falls out naturally
     * from the unsigned wraparound. width == 1 gives limit == 0, which
     * the skip-check below turns into an immediate zero-character
     * failure. */
    uint32_t limit = static_cast<uint32_t>(width - 1);
    width = 0;

    if (buffer == nullptr) {
        state_bits |= kFailBit;
        goto unlock_and_return;
    }

    {
        uint32_t count = 0;
        bool hitEof = false;

        if (limit != 0 && rdbuf != nullptr) {
            int32_t c = rdbuf->ReadChar();
            while (c != -1) {
                if (_isspace(c)) {
                    break;
                }
                buffer[count] = static_cast<char>(c);
                rdbuf->GetChar();
                ++count;
                if (count >= limit) {
                    break;
                }
                c = rdbuf->ReadChar();
            }
            hitEof = (c == -1);
        }

        if (hitEof) {
            state_bits |= kEofBit;
            if (count == 0) {
                /* No characters read before EOF: also badbit+failbit,
                 * and — verified against disassembly — the buffer is
                 * left untouched, no null terminator is written. */
                state_bits |= (kBadBit | kFailBit);
                goto unlock_and_return;
            }
            /* EOF after reading >=1 char: eofbit only, fall through to
             * null-terminate. */
        } else if (count == 0) {
            /* limit == 0 (width was 1), or the very first character was
             * whitespace: failbit only, buffer left untouched. */
            state_bits |= kFailBit;
            goto unlock_and_return;
        }

        buffer[count] = '\0';
    }

unlock_and_return:
    if (rdbuf != nullptr) {
        rdbuf->Unlock();
    }
    if (sync_flag < 0) {
        WNDPROC_LeaveCriticalSection(&critical_section);
    }
    return this;
}

/* ================================================================== */
/* Flush — 0x465960 (previously mislabeled StreamBuf_FlushOrPut)       */
/* ================================================================== */
void* WNDPROC_Stream::Flush()
{
    if (sync_flag < 0) {
        WNDPROC_EnterCriticalSection(&critical_section);
    }

    if (rdbuf != nullptr) {
        rdbuf->Lock();

        int32_t rc = rdbuf->Flush(); /* rdbuf's own virtual Flush() hook,
                                       * WNDPROC_StreamBuf vtable+0x04 */
        if (rc == -1) {
            state_bits |= kFailBit;
        }

        rdbuf->Unlock();
    }

    if (sync_flag < 0) {
        WNDPROC_LeaveCriticalSection(&critical_section);
    }
    return this;
}

/* ================================================================== */
/* Read — 0x463810 ("WIN32_StreamRead" at its call sites)               */
/* ================================================================== */
WNDPROC_Stream* WNDPROC_Stream::Read(void* buf, uint32_t size)
{
    if (InputPrefix(1)) {
        /* InputPrefix(1) left both this object's and rdbuf's CRITICAL_
         * SECTIONs held on success, per its own documented contract — the
         * two Unlock/Leave calls below are exactly the release it deferred
         * to us. */
        int32_t count = (rdbuf != nullptr)
                            ? rdbuf->ReadBytes(buf, static_cast<int32_t>(size))
                            : 0;
        gcount_ = count;
        if (static_cast<uint32_t>(count) < size) {
            state_bits |= (kFailBit | kEofBit);
        }

        if (rdbuf != nullptr) {
            rdbuf->Unlock();
        }
        if (sync_flag < 0) {
            WNDPROC_LeaveCriticalSection(&critical_section);
        }
    }
    return this;
}

/* ================================================================== */
/* ReadNumericToken — 0x465AD0 (Ghidra + shared/crt_stubs.cpp both      */
/* mislabel this "_ftol"/"CRT ftol"; see WndProcStream.h's doc comment) */
/* ================================================================== */
uint32_t WNDPROC_Stream::ReadNumericToken(char* buf, size_t bufCapacity)
{
    uint32_t radix;
    if (format_flags & kFmtDec) {
        radix = 10;
    } else if (format_flags & kFmtHex) {
        radix = 16;
    } else if (format_flags & kFmtOct) {
        radix = 8;
    } else {
        radix = 0;
    }

    if (!InputPrefix(0)) {
        return radix;
    }

    size_t pos = 0;          /* characters written to buf so far */
    size_t significant = 0;  /* count of validated digit characters */
    size_t signWidth = 1;    /* buf index at which digit checking begins;
                               * becomes 2 once a leading sign is consumed */
    bool naturalEof = false;

    int32_t c = (rdbuf != nullptr) ? rdbuf->ReadChar() : -1;
    while (c != -1) {
        if (pos == 0 && (c == '-' || c == '+')) {
            ++signWidth;
        } else {
            if (pos == signWidth && buf[pos - 1] == '0') {
                /* Positioned right after a leading '0': check for an
                 * "0x"/"0X" hex prefix, else promote to octal. */
                if ((c == 'x' || c == 'X') && (radix == 0 || radix == 16)) {
                    radix = 16;
                    significant = 0;
                    goto store_char;
                }
                if (radix == 0) {
                    radix = 8;
                }
            }
            if (radix == 16) {
                if (!std::isxdigit(static_cast<unsigned char>(c))) {
                    break;
                }
            } else {
                if (!std::isdigit(static_cast<unsigned char>(c)) ||
                    (radix == 8 && c > '7')) {
                    break;
                }
            }
            ++significant;
        }
    store_char:
        buf[pos] = static_cast<char>(c);
        c = static_cast<int32_t>(rdbuf->GetChar());
        ++pos;
        if (pos > bufCapacity - 2) {
            /* Classic old-iostream 15-significant-character extraction
             * cap — matches the original's `if (0xe < iVar7) goto DONE`,
             * which bypasses the natural-EOF eofbit assignment below. */
            goto done_reading;
        }
    }
    naturalEof = true;

done_reading:
    if (naturalEof) {
        state_bits |= kEofBit;
    }

    if (significant == 0) {
        state_bits |= kFailBit;
        /* Push every consumed character back onto the stream, matching
         * the original's exact unwind order (last-written first). */
        while (pos != 0) {
            --pos;
            int32_t result = (rdbuf != nullptr)
                ? rdbuf->PutBack(static_cast<uint8_t>(buf[pos]))
                : -1;
            if (result == -1) {
                state_bits |= kBadBit;
                break;
            }
            state_bits &= ~kEofBit;
        }
        pos = 0;
    }
    buf[pos] = '\0';

    if (rdbuf != nullptr) {
        rdbuf->Unlock();
    }
    if (sync_flag < 0) {
        WNDPROC_LeaveCriticalSection(&critical_section);
    }
    return radix;
}

/* ================================================================== */
/* ExtractInt — 0x4646C0 (operator>>(int32_t*), signed)                 */
/* ================================================================== */
WNDPROC_Stream* WNDPROC_Stream::ExtractInt(int32_t* out)
{
    if (!InputPrefix(0)) {
        return this;
    }

    char buf[16] = {};  /* zero-initialized: if ReadNumericToken() itself
                          * bails out early (its own nested InputPrefix(0)
                          * fails) it returns before writing a terminator,
                          * and strtol() below must not scan uninitialized
                          * stack bytes. */
    uint32_t radix = ReadNumericToken(buf, sizeof(buf));

    errno = 0;
    char* end = nullptr;
    long value = std::strtol(buf, &end, static_cast<int>(radix));
    *out = static_cast<int32_t>(value);
    if (errno == ERANGE) {
        state_bits |= kFailBit;
    }

    /* ReadNumericToken() re-entered InputPrefix(0)/released its own
     * (reentrant) lock pair already; this releases the OUTER pair this
     * function itself acquired above — matches the original's own
     * double Enter/Leave nesting exactly. */
    if (rdbuf != nullptr) {
        rdbuf->Unlock();
    }
    if (sync_flag < 0) {
        WNDPROC_LeaveCriticalSection(&critical_section);
    }
    return this;
}

/* ================================================================== */
/* ExtractUnsignedInt — 0x464F70 (Ghidra-mislabeled "CRT_fabs")         */
/* ================================================================== */
WNDPROC_Stream* WNDPROC_Stream::ExtractUnsignedInt(int32_t* out)
{
    if (!InputPrefix(0)) {
        return this;
    }

    char buf[16] = {};  /* see ExtractInt()'s identical comment above */
    uint32_t radix = ReadNumericToken(buf, sizeof(buf));

    errno = 0;
    char* end = nullptr;
    unsigned long value = std::strtoul(buf, &end, static_cast<int>(radix));
    *out = static_cast<int32_t>(value);
    if (*out == -1 && errno == ERANGE) {
        state_bits |= kFailBit;
    }

    if (rdbuf != nullptr) {
        rdbuf->Unlock();
    }
    if (sync_flag < 0) {
        WNDPROC_LeaveCriticalSection(&critical_section);
    }
    return this;
}

/* ================================================================== */
/* WNDPROC_Stream__ExtractInt(void*, int32_t*) — free-function adapter */
/* for the pre-existing caller in input/TrackTileDescriptor.cpp (which   */
/* declares it `extern "C"` — matched here — and also calls the         */
/* WNDPROC_CriticalSectionLock adapter above on the same `stream`        */
/* parameter). Real callers there (TrackTileDescriptor::Render, see its  */
/* own .cpp) always pass either a WIN32_Stream local or a                */
/* WNDPROC_StreamFromMemory-constructed WNDPROC_Stream — genuine,        */
/* already-constructed WNDPROC_Stream-family objects, confirmed by       */
/* reading that function's real call sites — so forwarding to the real,  */
/* already-validated ExtractInt() is safe, matching                      */
/* WNDPROC_CriticalSectionLock's own established precedent above.        */
/* Previously a loud deferred stub in shared/stubs_impl.cpp (removed).   */
/* ================================================================== */
extern "C" void* WNDPROC_Stream__ExtractInt(void* stream, int32_t* out)
{
    return reinterpret_cast<WNDPROC_Stream*>(stream)->ExtractInt(out);
}

/* ================================================================== */
/* WNDPROC_CriticalSectionLock(int*, char*) — free-function adapter    */
/* for the pre-existing callers in game/TrainStation.cpp, input/       */
/* BuildingDescriptorEditor.cpp, ui/UI_ChildWindow.cpp, game/           */
/* ScriptedObject.cpp, ui/CursorEditWindow.cpp, ui/UIPANEL_Surface.cpp, */
/* ui/HelpWnd.cpp (see WndProcStream.h for the full rationale).         */
/* `stream` is meant to be a `WNDPROC_Stream*`, forwarding to           */
/* ExtractToken() below.                                                */
/*                                                                       */
/* Was deferred back to a loud stub (2026-08-10, see PROGRESS.md        */
/* "WNDPROC_Stream facade recovery" postmortem) after a gdb-confirmed    */
/* SIGSEGV: at BuildingDescriptorEditor::Render's real call site,        */
/* `stream` was an `int streamHandle[2]` — an 8-byte raw stack handle    */
/* passed straight to `WIN32_StreamOpenPath` (a plain method call on an  */
/* already-constructed `WIN32_Stream`, per resources/Win32Stream.cpp —   */
/* NOT itself a constructor) with no prior `WIN32_StreamOpen` call, so    */
/* no vtable was ever established there. A follow-up attempt to work     */
/* around this by hand-writing the object's fields via raw offset        */
/* arithmetic (mimicking the original x86 MSVC vbtable layout) was       */
/* reverted: this host's `WNDPROC_Stream` is a real GCC/Itanium-ABI      */
/* object whose virtual-base offsets live at *negative* vtable indices,  */
/* not the MSVC forward-slot convention — manually poking bytes to       */
/* imitate the wrong ABI would have been worse than the original bug.    */
/*                                                                        */
/* Root cause fixed (2026-08-11, FUN_-sweep session): input/              */
/* BuildingDescriptorEditor.cpp's path-load branch now calls              */
/* `WIN32_StreamOpen` (the real placement-new constructor) before        */
/* `WIN32_StreamOpenPath`, matching its own sibling archive-load branch   */
/* and every one of this function's other real, currently-implemented    */
/* callers (game/ScriptedObject.cpp, game/TrainStation.cpp, ui/           */
/* CursorEditWindow.cpp, ui/UIPANEL_Surface.cpp, ui/HelpWnd.cpp — audited  */
/* individually and confirmed to already call `WIN32_StreamOpen` before   */
/* `WIN32_StreamOpenPath`/`OpenFile`). Declaration-shape differences       */
/* across those files (extern "C" vs C++-mangled, `int*` vs `void*`       */
/* parameter spelling) do not affect this: shared/                        */
/* stubs_link001_integration.cpp already bridges every extern "C" caller  */
/* to the one real C++-mangled implementation via GCC's asm-label         */
/* extension, and extern "C" linkage doesn't encode parameter types in    */
/* the symbol name the way C++ mangling does, so those spelling           */
/* differences were never a second landmine. (Two callers are NOT in      */
/* this audited set and must not be assumed safe: ui/GameSetupPanel.cpp's */
/* `loadLayouts` uses a distinct 5-arg `WIN32_StreamOpen` overload with    */
/* its own honest "not implemented" stub, unrelated to this path; ui/      */
/* AboutDialog.cpp's `LoadCredits` is not implemented at all yet — an      */
/* empty function body, so it cannot reach this adapter regardless.)      */
/*                                                                        */
/* Every real, reachable caller now constructs a genuine WIN32_Stream     */
/* (which IS-A WNDPROC_Stream via single, non-virtual inheritance) before */
/* reaching this adapter, so forwarding to the real, already-validated     */
/* ExtractToken() is safe. */
/* ================================================================== */
void WNDPROC_CriticalSectionLock(int* stream, char* buf)
{
    reinterpret_cast<WNDPROC_Stream*>(stream)->ExtractToken(buf);
}
