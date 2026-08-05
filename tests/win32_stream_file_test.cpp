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

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "all checks passed\n");
    return 0;
}
