// Status: VALIDATED
/**
 * win32_stream_file_test.cpp — WIN32_StreamFile / WNDPROC_StreamBuf
 * regressions
 *
 * Nothing in the tree calls these classes yet (the higher-level
 * WIN32_StreamOpen / WIN32_StreamRead / WIN32_StreamDestroy layer that
 * would construct and drive them is still a no-op stub -- see
 * PROGRESS.md's "win32_stream.c removed" entry). This test exists so the five
 * newly-reconstructed methods (Ctor, DtorBody, WriteChar, Flush, SetBuffer)
 * are actually exercised somewhere, rather than only proven to compile and
 * link. It drives the classes directly against real temp files.
 */

#include "resources/Win32StreamFile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

int g_failures = 0;

void Check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::fprintf(stderr, "ok: %s\n", what);
    }
}

std::string ReadWholeFile(const char* path)
{
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return std::string();
    std::string out;
    char buf[256];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    std::fclose(f);
    return out;
}

int MakeTempFile(std::string* pathOut)
{
    char tmpl[] = "/tmp/win32_stream_file_test_XXXXXX";
    int fd = ::mkstemp(tmpl);
    if (fd >= 0) {
        *pathOut = tmpl;
    }
    return fd;
}

} // namespace

/* ---- external symbols WIN32_StreamFile/WNDPROC_StreamBuf depend on ---- */
/* Real definitions (not host no-ops): matches shared/link_stubs.cpp's
 * operator_new(size_t)/GLOBAL_free(void*) semantics exactly, without
 * pulling in that whole translation unit's few hundred other symbols. */
void* operator_new(size_t size)
{
    void* p = std::malloc(size);
    if (p == nullptr) { std::fprintf(stderr, "FATAL: operator_new OOM\n"); std::abort(); }
    std::memset(p, 0, size);
    return p;
}
void GLOBAL_free(void* ptr) { std::free(ptr); }

extern "C" {
void WNDPROC_InitializeCriticalSection(void*) { /* single-threaded test */ }
void WNDPROC_DeleteCriticalSection(void*) { /* single-threaded test */ }
void WNDPROC_EnterCriticalSection(void*) { /* single-threaded test */ }
void WNDPROC_LeaveCriticalSection(void*) { /* single-threaded test */ }
}

int main()
{
    /* --- WriteChar/Flush: buffered path, real disk bytes --- */
    {
        std::string path;
        int fd = MakeTempFile(&path);
        Check(fd >= 0, "buffered: temp file created");

        WIN32_StreamFile stream;
        stream.SetFileHandle(fd, /* ownsHandle = */ 0);

        /* First WriteChar lazily allocates the default buffer (CheckFlush ->
         * AllocateDefaultBuffer) and buffers 'A' without touching disk. */
        int32_t rc = stream.WriteChar('A');
        Check(rc == 1, "buffered: WriteChar('A') returns 1");
        Check(ReadWholeFile(path.c_str()).empty(), "buffered: 'A' not yet on disk");

        /* Second WriteChar's internal Flush() call flushes the pending 'A'
         * to disk before buffering 'B'. */
        rc = stream.WriteChar('B');
        Check(rc == 1, "buffered: WriteChar('B') returns 1");
        Check(ReadWholeFile(path.c_str()) == "A", "buffered: 'A' flushed to disk by second WriteChar");

        /* Explicit Flush() flushes the still-pending 'B'. */
        rc = stream.Flush();
        Check(rc == 0, "buffered: explicit Flush() returns 0");
        Check(ReadWholeFile(path.c_str()) == "AB", "buffered: 'B' flushed to disk");

        /* WriteChar(-1) is the flush-only sentinel: no new byte, no-op
         * Flush (nothing pending), buffer region still reinitialized. */
        rc = stream.WriteChar(-1);
        Check(rc == 1, "buffered: WriteChar(-1) sentinel returns 1");
        Check(ReadWholeFile(path.c_str()) == "AB", "buffered: WriteChar(-1) writes no byte");

        /* SetBuffer's reject guard: fd is open and a buffer is already
         * configured (from the lazy AllocateDefaultBuffer above), so a new
         * SetBuffer call must be refused. */
        uint8_t someBuf[8];
        void* setRc = stream.SetBuffer(someBuf, sizeof(someBuf));
        Check(setRc == nullptr, "buffered: SetBuffer refuses once fd+buffer are configured");

        ::close(fd);
        ::unlink(path.c_str());
    }

    /* --- SetBuffer(nullptr, 0): unbuffered path, immediate direct write --- */
    {
        std::string path;
        int fd = MakeTempFile(&path);
        Check(fd >= 0, "unbuffered: temp file created");

        WIN32_StreamFile stream;
        void* setRc = stream.SetBuffer(nullptr, 0);
        Check(setRc == &stream, "unbuffered: SetBuffer(nullptr, 0) succeeds before fd is set");

        stream.SetFileHandle(fd, /* ownsHandle = */ 0);
        int32_t rc = stream.WriteChar('X');
        Check(rc == 1, "unbuffered: WriteChar('X') returns 1");
        Check(ReadWholeFile(path.c_str()) == "X", "unbuffered: 'X' written to disk immediately");

        ::close(fd);
        ::unlink(path.c_str());
    }

    /* --- Destructor: ownsHandle_ == 0 flushes but leaves the fd open --- */
    {
        std::string path;
        int fd = MakeTempFile(&path);
        Check(fd >= 0, "dtor(borrowed fd): temp file created");
        {
            WIN32_StreamFile stream;
            stream.SetFileHandle(fd, /* ownsHandle = */ 0);
            stream.WriteChar('Y');
            /* stream destructs here: DtorBody sees ownsHandle_ == 0, calls
             * Flush() (not CloseHandle()), so fd stays open afterward. */
        }
        Check(ReadWholeFile(path.c_str()) == "Y", "dtor(borrowed fd): pending byte flushed on destruction");
        Check(::write(fd, "Z", 1) == 1, "dtor(borrowed fd): fd is still open and writable after destruction");
        ::close(fd);
        ::unlink(path.c_str());
    }

    /* --- Destructor: ownsHandle_ != 0 flushes and closes the fd --- */
    {
        std::string path;
        int fd = MakeTempFile(&path);
        Check(fd >= 0, "dtor(owned fd): temp file created");
        {
            WIN32_StreamFile stream;
            stream.SetFileHandle(fd, /* ownsHandle = */ 1);
            stream.WriteChar('Q');
            /* stream destructs here: DtorBody sees ownsHandle_ != 0, calls
             * CloseHandle(), which flushes and _close()s the fd. */
        }
        Check(ReadWholeFile(path.c_str()) == "Q", "dtor(owned fd): pending byte flushed before close");
        errno = 0;
        ssize_t wrote = ::write(fd, "Z", 1);
        Check(wrote < 0, "dtor(owned fd): fd was actually closed by the destructor");
        ::unlink(path.c_str());
    }

    /* --- CloseHandle(): explicit close, double-close guard --- */
    {
        std::string path;
        int fd = MakeTempFile(&path);
        Check(fd >= 0, "close-handle: temp file created");

        WIN32_StreamFile stream;
        stream.SetFileHandle(fd, /* ownsHandle = */ 1);
        stream.WriteChar('R');

        WIN32_StreamFile* rc = stream.CloseHandle();
        Check(rc == &stream, "close-handle: first CloseHandle() returns this");
        Check(stream.fileHandle() == -1, "close-handle: fd reset to -1 after close");
        Check(ReadWholeFile(path.c_str()) == "R", "close-handle: pending byte flushed before close");

        rc = stream.CloseHandle();
        Check(rc == nullptr, "close-handle: second CloseHandle() on an already-closed stream returns nullptr");

        ::unlink(path.c_str());
    }

    /* --- GetChar/ReadChar via Underflow(): buffered read path --- */
    /*
     * IMPORTANT — GetChar()'s real (disassembly-verified, 0x4651A0)
     * contract is NOT "returns the same character ReadChar() just peeked".
     * It unconditionally pre-increments readPtr_ BEFORE reading, so on its
     * own it returns the character AFTER the current position. Every real
     * caller in this codebase (WNDPROC_Stream::SkipWhitespace's `c =
     * GetChar()` loop; WNDPROC_Stream::ExtractToken's bare `rdbuf->
     * GetChar();`) uses it exactly this way: ReadChar() peeks the current
     * character, GetChar() is called purely for its cursor-advance SIDE
     * EFFECT (consuming the just-peeked character) and to hand back the
     * NEXT one to peek again via ReadChar() — its return value from that
     * pairing is either discarded (ExtractToken) or immediately re-peeked
     * anyway (SkipWhitespace's next loop iteration doesn't re-check it
     * directly, it flows into the next ReadChar()). This test follows that
     * real idiom rather than asserting a "peek/get return the same char"
     * contract the original code never actually has.
     */
    {
        std::string path;
        int fd = MakeTempFile(&path);
        Check(fd >= 0, "buffered read: temp file created");
        Check(::write(fd, "AB", 2) == 2, "buffered read: seed file with 2 bytes");
        ::lseek(fd, 0, SEEK_SET);

        WIN32_StreamFile stream;
        stream.SetFileHandle(fd, /* ownsHandle = */ 0);

        Check(stream.ReadChar() == 'A', "buffered read: ReadChar() peeks 'A'");
        Check(stream.ReadChar() == 'A', "buffered read: repeated ReadChar() re-peeks 'A' (not consumed)");
        stream.GetChar(); /* consume 'A' (side effect only, real-caller idiom) */
        Check(stream.ReadChar() == 'B', "buffered read: ReadChar() peeks 'B' after consuming 'A'");
        stream.GetChar(); /* consume 'B' */
        Check(stream.ReadChar() == -1, "buffered read: ReadChar() hits real EOF after consuming both bytes");

        ::close(fd);
        ::unlink(path.c_str());
    }

    /* --- Cold GetChar() with no preceding ReadChar(): documents the      */
    /* --- original's real quirk, not a contract any real caller relies    */
    /* --- on. A bare GetChar() on an exhausted get-region refills via     */
    /* --- Underflow(), DISCARDS that call's return value (per 0x4651A0's  */
    /* --- own disassembly), then pre-increments before reading — so the   */
    /* --- very first buffered byte is silently skipped. Real code never   */
    /* --- hits this because InputPrefix()/SkipWhitespace() always prime   */
    /* --- with a ReadChar() first (see the block above).                  */
    {
        std::string path;
        int fd = MakeTempFile(&path);
        Check(fd >= 0, "cold GetChar quirk: temp file created");
        Check(::write(fd, "PQ", 2) == 2, "cold GetChar quirk: seed file with 2 bytes");
        ::lseek(fd, 0, SEEK_SET);

        WIN32_StreamFile stream;
        stream.SetFileHandle(fd, /* ownsHandle = */ 0);
        Check(stream.GetChar() == 'Q', "cold GetChar quirk: unprimed GetChar() skips 'P', returns 'Q'");

        ::close(fd);
        ::unlink(path.c_str());
    }

    /* --- ReadChar/GetChar via Underflow(): unbuffered read path,        */
    /* --- same peek-then-consume-for-side-effect idiom as buffered above. */
    {
        std::string path;
        int fd = MakeTempFile(&path);
        Check(fd >= 0, "unbuffered read: temp file created");
        Check(::write(fd, "YZ", 2) == 2, "unbuffered read: seed file with 2 bytes");
        ::lseek(fd, 0, SEEK_SET);

        WIN32_StreamFile stream;
        stream.SetBuffer(nullptr, 0);
        stream.SetFileHandle(fd, /* ownsHandle = */ 0);

        Check(stream.ReadChar() == 'Y', "unbuffered read: ReadChar() peeks 'Y'");
        stream.GetChar(); /* consume 'Y' */
        Check(stream.ReadChar() == 'Z', "unbuffered read: ReadChar() peeks 'Z' after consuming 'Y'");
        stream.GetChar(); /* consume 'Z' */
        Check(stream.ReadChar() == -1, "unbuffered read: ReadChar() hits real EOF after consuming both bytes");

        ::close(fd);
        ::unlink(path.c_str());
    }

    /* --- Open(): real path-based open (0x4652D0), read-only against a --- */
    /* --- pre-existing file, matching every real caller's flags value.  --- */
    {
        std::string path;
        int seedFd = MakeTempFile(&path);
        Check(seedFd >= 0, "open: temp file created");
        Check(::write(seedFd, "hi", 2) == 2, "open: seed file with content");
        ::close(seedFd);

        WIN32_StreamFile stream;
        /* 0x21 == 0x20 (no-create) | 0x01 (read intent) — exactly what
         * WIN32_StreamOpenPath's real disassembly ORs into every real
         * caller's flags (see resources/WndProcStream.cpp's callers). */
        WIN32_StreamFile* rc = stream.Open(path.c_str(), 0x21, 0);
        Check(rc == &stream, "open: Open() on an existing file succeeds");
        Check(stream.fileHandle() != -1, "open: fd is valid after Open()");
        /* Peek-then-consume-for-side-effect idiom, see the buffered-read
         * block above for why GetChar()'s return value isn't asserted
         * directly. */
        Check(stream.ReadChar() == 'h', "open: opened stream reads real file content ('h')");
        stream.GetChar();
        Check(stream.ReadChar() == 'i', "open: opened stream reads real file content ('i')");
        stream.GetChar();
        Check(stream.ReadChar() == -1, "open: opened stream hits real EOF");

        /* Already-open guard. */
        rc = stream.Open(path.c_str(), 0x21, 0);
        Check(rc == nullptr, "open: second Open() on an already-open stream fails");

        stream.CloseHandle();
        ::unlink(path.c_str());
    }

    /* --- Open(): nonexistent path with no-create bit set fails cleanly --- */
    {
        WIN32_StreamFile stream;
        WIN32_StreamFile* rc = stream.Open("/nonexistent/path/for/win32_stream_file_test", 0x21, 0);
        Check(rc == nullptr, "open: Open() on a nonexistent no-create path returns nullptr");
        Check(stream.fileHandle() == -1, "open: fd stays -1 after a failed Open()");
    }

    /* --- Open(): must NOT allocate a default buffer when the stream was  */
    /* --- already put into unbuffered mode before Open() is called. The   */
    /* --- guard is unbuffered_/bufferEnd_ (+0x08/+0x14 in the original,   */
    /* --- verified via disassembly at 0x4653D7/0x4653E6) — NOT bufferStart_/ */
    /* --- writeBase_, which an earlier draft of Open() used by mistake    */
    /* --- (caught by this exact test case). SetBuffer()'s own reject      */
    /* --- guard (fd_ != -1 && bufferEnd_ != nullptr) is the observation   */
    /* --- point: it only refuses a second SetBuffer() call if Open()      */
    /* --- correctly left bufferEnd_ untouched.                            */
    {
        std::string path;
        int seedFd = MakeTempFile(&path);
        Check(seedFd >= 0, "open(unbuffered): temp file created");
        ::close(seedFd);

        WIN32_StreamFile stream;
        void* setRc = stream.SetBuffer(nullptr, 0);
        Check(setRc == &stream, "open(unbuffered): SetBuffer(nullptr, 0) succeeds before Open()");

        WIN32_StreamFile* openRc = stream.Open(path.c_str(), 0x21, 0);
        Check(openRc == &stream, "open(unbuffered): Open() still succeeds");

        uint8_t someBuf[8];
        setRc = stream.SetBuffer(someBuf, sizeof(someBuf));
        Check(setRc == &stream,
              "open(unbuffered): SetBuffer() after Open() still succeeds -- Open() did not "
              "allocate a default buffer out from under the unbuffered stream");

        stream.CloseHandle();
        ::unlink(path.c_str());
    }

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "all checks passed\n");
    return 0;
}
