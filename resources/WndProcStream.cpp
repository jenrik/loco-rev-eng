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

#include <cassert>
#include <cstdio>

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
/* WndProcStream.h for the full rationale). `stream` is really meant   */
/* to be a WNDPROC_Stream*, forwarding to ExtractToken() below —        */
/* BUT deferred back to a loud stub (2026-08-10, see PROGRESS.md        */
/* "WNDPROC_Stream facade recovery" postmortem):                        */
/*                                                                       */
/* gdb-confirmed SIGSEGV (coredumpctl, reproduced twice): at its real   */
/* call sites (BuildingDescriptorEditor::Render, TrainStation's         */
/* equivalent), `stream` is an `int streamHandle[2]` — an 8-byte raw    */
/* handle from the still-unimplemented WIN32_StreamOpenPath (no-op host */
/* stub), never a real, constructed `WNDPROC_Stream` object. `WNDPROC_  */
/* Stream` has a virtual base (StreamObject) reached through a vtable   */
/* pointer that only a real C++ constructor establishes — a raw,        */
/* uninitialized/undersized buffer reinterpret_cast to this type has no */
/* such pointer, so ExtractToken()/InputPrefix() dereference garbage.   */
/*                                                                       */
/* A follow-up attempt to fix this by hand-writing the object's fields  */
/* via raw offset arithmetic (mimicking the original x86 MSVC vbtable   */
/* layout: forward slot `vtable_ptr[1]` = byte offset to the virtual    */
/* base) was reverted: this host build's `WNDPROC_Stream` is a real     */
/* GCC/Itanium-ABI C++ object, whose virtual-base offsets live at        */
/* *negative* vtable indices (confirmed from this exact crash's fault   */
/* disassembly: `mov (%rax),%rax; sub $0x18,%rax; mov (%rax),%rax`) —    */
/* not the MSVC forward-slot convention. Manually poking bytes to        */
/* imitate the wrong ABI is worse than the original bug: silent          */
/* corruption instead of a clean crash. Per CLAUDE.md, raw `this +      */
/* offset` construction of a typed C++ object is not allowed regardless.*/
/*                                                                       */
/* The real fix needs a real `WNDPROC_Stream` constructed via its own    */
/* constructor (placement-new) wrapping a real `WIN32_StreamFile`        */
/* rdbuf — which means reverse engineering the `WIN32_StreamOpen`/       */
/* `OpenFile`/`OpenPath`/`Read`/`Destroy`/`DestroyImmediate` cluster      */
/* (0x463810-0x463B6B) AND first unifying 8+ mutually incompatible,      */
/* non-`extern "C"` declarations of `WIN32_StreamOpen`/`OpenPath` spread  */
/* across game/ScriptedObject.cpp, TrainStation.cpp, input/               */
/* BuildingDescriptorEditor.cpp, ui/CursorEditWindow.cpp,                 */
/* ui/GameSetupPanel.cpp (5-arg!), ui/UIPANEL_Surface.cpp, ui/HelpWnd.cpp,*/
/* ui/AboutDialog.cpp — most of which currently bind to unrelated stub    */
/* symbols, not each other. That is its own dedicated RE pass (tracked    */
/* in PROGRESS.md), not a quick fix.                                      */
/*                                                                        */
/* Until then: fail loudly rather than silently corrupting memory or      */
/* returning wrong results, per CLAUDE.md's stub policy. The real         */
/* ExtractToken()/InputPrefix()/SkipWhitespace()/Flush() implementations  */
/* above remain intact and validated for whenever a real WNDPROC_Stream   */
/* object reaches them.                                                   */
/* ================================================================== */
void WNDPROC_CriticalSectionLock(int* stream, char* buf)
{
    (void)stream;
    (void)buf;
    fprintf(stderr,
            "STUB: WNDPROC_CriticalSectionLock (0x4649F0 adapter) reached — "
            "its `stream` argument is not a real constructed WNDPROC_Stream "
            "on the host path (WIN32_StreamOpen* cluster not yet "
            "implemented), see resources/WndProcStream.cpp\n");
    assert(false &&
           "WNDPROC_CriticalSectionLock: deferred, see TODO in "
           "resources/WndProcStream.cpp");
}
