/**
 * Win32StreamFile.cpp — WIN32_StreamFile implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly (locoaudit DB).
 *
 * See Win32StreamFile.h for the field/address map. Transcribes:
 *   WIN32_StreamFile_Ctor        0x463B70
 *   WIN32_StreamFile_DtorBody    0x463BB0
 *   WIN32_StreamFile_CloseHandle 0x463C30
 *   WIN32_StreamFile_WriteChar   0x463CB0
 *   WIN32_StreamFile_Flush       0x463E50
 *   WIN32_StreamFile_SetBuffer   0x463F50
 *   WIN32_StreamFile_Underflow   0x463D40
 *   WIN32_StreamFile_Open        0x4652D0
 */

// Status: VALIDATED

#include "Win32StreamFile.h"

#include <cassert>
#include <cstdio>
#include <cstring>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {
/* Same Win32 CRITICAL_SECTION family used by WndProcStreamBuf.cpp and by
 * resources/StreamObject.cpp (0x464D90/0x464DA0). */
void __stdcall WNDPROC_EnterCriticalSection(void* cs);  /* 0x464D90 */
void __stdcall WNDPROC_LeaveCriticalSection(void* cs);  /* 0x464DA0 */
}

/* ================================================================== */
/* CRT low-level I/O (_write/_close/_lseek, 0x468CF0/0x468BF0/0x469230). */
/* On the original path these are the statically-linked MSVC CRT calls   */
/* (documented, not reimplemented, per shared/crt_stubs.h/.cpp). On the  */
/* host they are POSIX write/close/lseek with identical contracts — this */
/* is not a behavior change, just the equivalent libc entry point.       */
/* ================================================================== */
#include <fcntl.h>  /* O_CREAT/O_EXCL/O_APPEND/O_TRUNC/O_WRONLY/O_RDWR — used
                      * by Open() below on both platforms; POSIX values on
                      * the host, MinGW's real-CRT-matching values under
                      * _WIN32. */
#ifndef O_BINARY
#define O_BINARY 0  /* No POSIX equivalent (no text/binary distinction on
                      * Linux) — file scope so it doesn't leak a local
                      * macro definition into the rest of the translation
                      * unit. */
#endif

#ifndef _WIN32
#include <unistd.h>
namespace {
inline int32_t HostWrite(int32_t fd, const void* buf, uint32_t count)
{
    return static_cast<int32_t>(::write(fd, buf, count));
}
inline int32_t HostRead(int32_t fd, void* buf, uint32_t count)
{
    return static_cast<int32_t>(::read(fd, buf, count));
}
inline int32_t HostClose(int32_t fd) { return ::close(fd); }
inline long HostLseek(int32_t fd, long offset, int32_t whence) { return ::lseek(fd, offset, whence); }
inline int32_t HostOpen(const char* path, int32_t posixFlags)
{
    return ::open(path, static_cast<int>(posixFlags), 0644);
}
} // namespace
#else
extern "C" int _write(int fd, const void* buf, unsigned int count);   /* 0x468CF0 */
extern "C" int _read(int fd, void* buf, unsigned int count);          /* 0x468D30-ish, CRT low-level read */
extern "C" int _close(int fd);                                        /* 0x468BF0 */
extern "C" long _lseek(int fd, long offset, int origin);              /* 0x469230 */
extern "C" int _open(const char* path, int oflag, ...);               /* CRT low-level open */
#define HostWrite _write
#define HostRead _read
#define HostClose _close
#define HostLseek _lseek
#define HostOpen(path, flags) _open(path, flags)

/* MSVC CRT-internal per-fd _osfile[] mode table, referenced only by the
 * _WIN32 branch of Flush() below (text-mode CRLF seek-back adjustment).
 * A linkage-specification must appear at namespace scope in C++, not
 * inside a function body — declared here instead of inline in Flush(). */
extern "C" unsigned char* DAT_005007e0[];
#endif

/* ================================================================== */
/* WIN32_StreamFile_Ctor — 0x463B70                                    */
/* ================================================================== */
WIN32_StreamFile::WIN32_StreamFile()
    : WNDPROC_StreamBuf(), fd_(-1), ownsHandle_(0)
{
    /* Original re-stores the vtable pointer explicitly here; the compiler
     * emits that store automatically once WNDPROC_StreamBuf::
     * WNDPROC_StreamBuf() returns and this constructor body runs. */
}

/* ================================================================== */
/* WIN32_StreamFile_DtorBody — 0x463BB0                                 */
/* ================================================================== */
WIN32_StreamFile::~WIN32_StreamFile()
{
    if (syncActive_ < 0) {
        WNDPROC_EnterCriticalSection(&cs_);
    }
    if (ownsHandle_ == 0) {
        Flush();
    } else {
        CloseHandle();
    }
    /* Faithful to the original: there is no matching LeaveCriticalSection
     * call in DtorBody itself (verified via disassembly — the function
     * falls straight through to the base-class teardown after the Flush/
     * CloseHandle branch). CloseHandle() enters/leaves its own nested
     * (recursive) critical-section pair, so this is not a deadlock on
     * Win32, just an outer lock left held until ~WNDPROC_StreamBuf() below
     * deletes cs_ — harmless because the object is being destroyed and no
     * other thread can observe it again. base class dtor
     * (~WNDPROC_StreamBuf) runs automatically after this body, matching
     * the original's explicit trailing call to WNDPROC_StreamBuf_DtorBody.
     */
}

/* ================================================================== */
/* WIN32_StreamFile_CloseHandle — 0x463C30                              */
/* ================================================================== */
WIN32_StreamFile* WIN32_StreamFile::CloseHandle()
{
    if (fd_ == -1) {
        return nullptr;
    }
    if (syncActive_ < 0) {
        WNDPROC_EnterCriticalSection(&cs_);
    }
    /* Original dispatches through vtable+0x04 (`this->Flush()`); WriteChar
     * override for this class is at a different slot (+0x1C), confirmed by
     * a direct read of WIN32_StreamFile's vtable at 0x4791AC (see
     * WndProcStreamBuf.h). Calling Flush() here reproduces that dispatch
     * exactly, since no further subclass in scope overrides it. */
    int32_t flushResult = Flush();
    int32_t closeResult = HostClose(fd_);
    if (closeResult != -1 && flushResult != -1) {
        fd_ = -1;
        if (syncActive_ < 0) {
            WNDPROC_LeaveCriticalSection(&cs_);
        }
        return this;
    }
    if (syncActive_ < 0) {
        WNDPROC_LeaveCriticalSection(&cs_);
    }
    return nullptr;
}

/* ================================================================== */
/* WIN32_StreamFile_WriteChar — 0x463CB0                                */
/* ================================================================== */
int32_t WIN32_StreamFile::WriteChar(int32_t ch)
{
    /* "overflow" hook: called when the fast-path (caller writing directly
     * into the buffer) has run out of room, or before the buffer exists at
     * all. It flushes whatever is currently pending, resets the write
     * region, then buffers (or directly writes) the incoming character. */
    int32_t rc = CheckFlush();
    if (rc == -1) {
        return -1;
    }
    rc = Flush();
    if (rc == -1) {
        return -1;
    }
    if (unbuffered_ == 0) {
        writeBase_ = bufferStart_;
        writePtr_ = bufferStart_;
        writeHigh_ = bufferEnd_;
    }
    if (ch != -1) {
        if (unbuffered_ == 0 && writePtr_ < writeHigh_) {
            *writePtr_ = static_cast<uint8_t>(ch);
            ++writePtr_;
            return 1;
        }
        const char c = static_cast<char>(ch);
        int32_t written = HostWrite(fd_, &c, 1);
        if (written != 1) {
            return -1;
        }
    }
    return 1;
}

/* ================================================================== */
/* WIN32_StreamFile_Underflow — 0x463D40 (vtable+0x20)                  */
/*                                                                       */
/* "underflow"-equivalent get-side refill hook, called by the base       */
/* class's ReadChar()/GetChar() (WndProcStreamBuf.cpp) when the get      */
/* region is exhausted. Refills from fd_ via the CRT low-level read      */
/* equivalent, then returns the next byte WITHOUT consuming it — the     */
/* base class's ReadChar()/GetChar() own the read-cursor advance, not    */
/* this override (verified: the original never touches readPtr_ past    */
/* resetting it to bufferStart_ on refill; it returns *readPtr_, it      */
/* does not increment past it). Returns -1 on EOF/error (read count      */
/* < 1), matching the original's contract.                               */
/*                                                                        */
/* Note for any future subclass: the original calls WIN32_StreamFile's    */
/* own Flush() via a direct absolute CALL (0x463D6F -> 0x463E50), NOT     */
/* through the vtable — i.e. non-virtually, even though Flush() is a      */
/* virtual slot. This override below calls Flush() through normal C++     */
/* member-call syntax, which resolves virtually. Identical behavior       */
/* today since nothing derives from WIN32_StreamFile, but a future        */
/* subclass overriding Flush() would observably diverge from the          */
/* original here — flagged so that divergence isn't silently introduced.  */
/* ================================================================== */
int32_t WIN32_StreamFile::Underflow()
{
    int32_t avail = (readPtr_ < readHigh_) ? static_cast<int32_t>(readHigh_ - readPtr_) : 0;
    if (avail != 0) {
        return *readPtr_;
    }

    int32_t rc = CheckFlush();
    if (rc == -1) {
        return -1;
    }
    rc = Flush();
    if (rc == -1) {
        return -1;
    }

    if (unbuffered_ != 0) {
        char c;
        int32_t n = HostRead(fd_, &c, 1);
        if (n < 1) {
            return -1;
        }
        return static_cast<uint8_t>(c);
    }

    uint32_t space = (bufferStart_ < bufferEnd_)
        ? static_cast<uint32_t>(bufferEnd_ - bufferStart_) : 0;
    int32_t n = HostRead(fd_, bufferStart_, space);
    if (n < 1) {
        return -1;
    }
    peekCache_ = -1;
    readBase_ = bufferStart_;
    readPtr_ = bufferStart_;
    readHigh_ = bufferStart_ + n;
    return *readPtr_;
}

/* ================================================================== */
/* WIN32_StreamFile_Flush — 0x463E50                                    */
/* ================================================================== */
int32_t WIN32_StreamFile::Flush()
{
    if (fd_ == -1) {
        return -1;
    }
    if (unbuffered_ != 0) {
        return 0;
    }

    /* --- flush the pending write region --- */
    uint8_t* base = writeBase_;
    uint32_t pending = (writePtr_ < base) ? 0u : static_cast<uint32_t>(writePtr_ - base);
    if (pending != 0) {
        int32_t written = HostWrite(fd_, base, pending);
        if (static_cast<uint32_t>(written) != pending) {
            if (written < 1) {
                return -1;
            }
            /* Short write: keep the unwritten remainder, shift it to the
             * front of the buffer (matches the original's call into what
             * Ghidra auto-analysis mislabeled "CRT_strncpy" at 0x466EA0 —
             * verified by decompiling it: no NUL handling at all, just an
             * overlap-aware backward/forward byte copy, i.e. memmove).
             * writeHigh_ acts as the "buffering is active" guard here,
             * matching the original's `*(this+0x20) != 0` check. */
            if (writeHigh_ != nullptr) {
                writePtr_ -= written;
            }
            std::memmove(base, base + written, pending - written);
            return -1;
        }
    }

    /* --- reset the write region and, if any read-ahead was buffered but  */
    /* not consumed, seek the fd back over it so read and write positions */
    /* stay consistent (classic "switching from read to write" fixup).    */
    uint8_t* pendingReadEnd = readHigh_;
    uint8_t* pendingReadCur = readPtr_;
    writeBase_ = nullptr;
    writePtr_ = nullptr;
    writeHigh_ = nullptr;

    int32_t pendingRead = (pendingReadCur < pendingReadEnd)
        ? static_cast<int32_t>(pendingReadEnd - pendingReadCur)
        : 0;
    if (pendingRead > 0) {
#ifdef _WIN32
        /* Original: looks up the CRT's internal per-fd _osfile[] mode byte
         * (DAT_005007e0, 0x24-byte records) and, if the fd is in text mode
         * (bit 0x80), adds one to the seek-back count for every '\n' in the
         * unconsumed region (plus one more if a lone trailing CR is
         * pending, bit 0x02) to account for CRLF -> LF translation. This
         * table is MSVC CRT-internal and only meaningful when actually
         * linked against that CRT (the MinGW typecheck target never links
         * this translation unit — see CLAUDE.md's Stubs section). */
        const uint32_t fdBlock = static_cast<uint32_t>(fd_) >> 5;
        const uint32_t fdIndex = static_cast<uint32_t>(fd_) & 0x1f;
        const unsigned char mode = DAT_005007e0[fdBlock][4 + fdIndex * 0x24];
        if (mode & 0x80) {
            for (uint8_t* p = pendingReadCur; p < pendingReadEnd; ++p) {
                if (*p == '\n') {
                    ++pendingRead;
                }
            }
            if (mode & 0x02) {
                ++pendingRead;
            }
        }
#else
        /* Host deviation: POSIX has no CRT text-mode byte-doubling table —
         * files opened via this class are never translated, so the
         * seek-back amount is exactly the pending unread byte count, with
         * no newline adjustment. */
#endif
        long seekResult = HostLseek(fd_, -pendingRead, /* SEEK_CUR */ 1);
        if (seekResult == -1) {
            return -1;
        }
    }

    readBase_ = nullptr;
    readPtr_ = nullptr;
    readHigh_ = nullptr;
    peekCache_ = -1;
    return 0;
}

/* ================================================================== */
/* WIN32_StreamFile_SetBuffer — 0x463F50                                */
/* ================================================================== */
void* WIN32_StreamFile::SetBuffer(void* buffer, int32_t size)
{
    if (fd_ != -1 && bufferEnd_ != nullptr) {
        /* Already open with a configured buffer — refuse to change it. */
        return nullptr;
    }
    if (buffer == nullptr || size < 1) {
        unbuffered_ = 1;
    } else {
        /* Both branches converge on `return this` in the original (the
         * unlock-then-return and the unconditional trailing return are the
         * same statement); collapsed here since equivalence is exact. */
        if (syncActive_ < 0) {
            WNDPROC_EnterCriticalSection(&cs_);
        }
        SetBufferPtrs(static_cast<uint8_t*>(buffer),
                      static_cast<uint8_t*>(buffer) + size,
                      /* owns = */ 0);
        if (syncActive_ < 0) {
            WNDPROC_LeaveCriticalSection(&cs_);
        }
    }
    return this;
}

/* ================================================================== */
/* WIN32_StreamFile_Open — 0x4652D0 (previously Ghidra-mislabeled       */
/* "CRT_exp" — an auto-analysis artifact)                                */
/*                                                                       */
/* `flags` bit meanings, independently re-derived from the raw           */
/* disassembly (not the caller's naming, which never named them):        */
/*   0x01  read intent (required alongside 0x02 for write, or alone      */
/*         for read-only — real CRT _O_RDONLY is numerically 0, so the   */
/*         read-only path leaves the low oflag bits at 0)                */
/*   0x02  write/read-write intent gate                                  */
/*   0x04  seek to end after opening (NOT IMPLEMENTED here — see below)  */
/*   0x08  append (-> real CRT _O_APPEND); also forces the write gate    */
/*   0x10  truncate (-> real CRT _O_TRUNC); also forces the write gate   */
/*   0x20  "open existing only" — its ABSENCE is what adds real CRT      */
/*         _O_CREAT (every real caller in this codebase SETS this bit,   */
/*         i.e. never creates — all real call sites only ever open       */
/*         pre-existing resource files for reading)                      */
/*   0x40  exclusive (-> real CRT _O_EXCL)                                */
/*   0x80  binary mode (set) vs. text mode (clear); on the host this      */
/*         only matters for POSIX's optional O_BINARY (no-op on Linux)   */
/* `shareMask` selects a Windows CreateFileA share mode (exclusive/       */
/* read-shared/write-shared/read-write-shared) in the original — no      */
/* POSIX equivalent exists, so it is accepted for signature fidelity     */
/* but unused on the host (documented host deviation, not a silent       */
/* simplification).                                                      */
/* ================================================================== */
WIN32_StreamFile* WIN32_StreamFile::Open(const char* path, int32_t flags, int32_t shareMask)
{
    (void)shareMask;

    if (fd_ != -1) {
        /* Already open. */
        return nullptr;
    }

    /* O_CREAT/O_EXCL/O_APPEND/O_TRUNC/O_WRONLY/O_RDWR/O_BINARY come from
     * <fcntl.h> (see the file-scope O_BINARY fallback near the top of this
     * file) — no hand-picked numeric literals needed on either platform. */
    int32_t posixFlags = (flags & 0x80) ? O_BINARY : 0;
    if ((flags & 0x20) == 0) {
        posixFlags |= O_CREAT;
    }
    if ((flags & 0x40) != 0) {
        posixFlags |= O_EXCL;
    }
    if ((flags & 0x08) != 0) {
        flags |= 0x02;
        posixFlags |= O_APPEND;
    }
    if ((flags & 0x10) != 0) {
        flags |= 0x02;
        posixFlags |= O_TRUNC;
    }
    if ((flags & 0x02) == 0) {
        if ((flags & 0x01) == 0) {
            /* Neither read nor write intent specified — original returns
             * NULL here without ever calling the underlying open(). */
            return nullptr;
        }
        /* Read-only: real CRT _O_RDONLY is numerically 0, nothing to OR in. */
    } else {
        posixFlags |= ((flags & 0x01) == 0) ? O_WRONLY : O_RDWR;
        if ((flags & 0x4D) == 0) {
            posixFlags |= O_TRUNC;
        }
    }

    int32_t newFd = HostOpen(path, posixFlags);
    fd_ = newFd;
    if (newFd == -1) {
        return nullptr;
    }

    if (syncActive_ < 0) {
        WNDPROC_EnterCriticalSection(&cs_);
    }
    ownsHandle_ = 1;
    /* Guard is unbuffered_/bufferEnd_ (+0x08/+0x14), NOT bufferStart_/
     * writeBase_ — verified against the disassembly: 0x4653D7 reads
     * [ESI+0x8] (unbuffered_), 0x4653E6 reads [ESI+0x14] (bufferEnd_). A
     * stream already put into unbuffered mode via SetBuffer(nullptr, 0)
     * before Open() must NOT get a buffer allocated out from under it. */
    if (unbuffered_ == 0 && bufferEnd_ == nullptr) {
        /* Lazily allocate the default 0x200-byte buffer, matching the
         * original's inline allocate-and-SetBuffer sequence — reuses the
         * existing SetBufferPtrs() helper rather than duplicating it. */
        void* buf = operator_new(0x200);
        if (buf == nullptr) {
            unbuffered_ = 1;
        } else {
            SetBufferPtrs(static_cast<uint8_t*>(buf), static_cast<uint8_t*>(buf) + 0x200, /* owns = */ 1);
        }
    }

    if ((flags & 0x04) != 0) {
        /* Seek-to-end-after-open: real logic dispatches through a still-
         * unnamed vtable slot (0x463E00 in the original — a distinct
         * method this pass did not reverse engineer, out of scope for
         * this class's WriteChar/Flush/SetBuffer/Underflow/Open batch).
         * No real caller in this codebase's evidenced call sites ever
         * sets this bit (verified: every real WIN32_StreamOpenPath/
         * WIN32_StreamOpenFile call site uses 0x20/0x21/0xA0/0xA1, all
         * with bit 0x04 clear) — fail loudly rather than silently
         * ignoring the request if this branch is ever actually reached. */
        fprintf(stderr,
                "STUB: WIN32_StreamFile::Open seek-to-end branch (flags&0x04, "
                "original vtable slot 0x463E00) reached — not yet reverse "
                "engineered, see resources/Win32StreamFile.cpp\n");
        assert(false &&
               "WIN32_StreamFile::Open: seek-to-end branch deferred, see "
               "TODO in resources/Win32StreamFile.cpp");
    }

    if (syncActive_ < 0) {
        WNDPROC_LeaveCriticalSection(&cs_);
    }
    return this;
}
