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
 */

// Status: VALIDATED

#include "WndProcStream.h"

extern "C" {
/* Same family as WNDPROC_StreamBuf's Lock()/Unlock() (resources/
 * WndProcStreamBuf.cpp) — 0x464D90/0x464DA0, already declared/stubbed in
 * resources/StreamObject.cpp. */
void __stdcall WNDPROC_EnterCriticalSection(void* cs);
void __stdcall WNDPROC_LeaveCriticalSection(void* cs);

/* CRT character classification. */
int __cdecl _isspace(int c);
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
/* WNDPROC_CriticalSectionLock(int*, char*) — free-function adapter    */
/* for the pre-existing callers in game/TrainStation.cpp, input/       */
/* BuildingDescriptorEditor.cpp, ui/UI_ChildWindow.cpp (see             */
/* WndProcStream.h for the full rationale). `stream` is really a        */
/* WNDPROC_Stream*.                                                    */
/* ================================================================== */
void WNDPROC_CriticalSectionLock(int* stream, char* buf)
{
    reinterpret_cast<WNDPROC_Stream*>(stream)->ExtractToken(buf);
}
