/**
 * Win32StreamFile.h — file-backed stream, derived from WNDPROC_StreamBuf
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly (locoaudit DB).
 *
 * WIN32_StreamFile is the "filebuf"-equivalent subclass of WNDPROC_StreamBuf
 * (see WndProcStreamBuf.h): it backs the put/get regions with an open CRT
 * low-level file descriptor instead of a caller-supplied memory block.
 *
 * Address map (locoaudit):
 *   WIN32_StreamFile_Ctor        0x463B70  -> WIN32_StreamFile()
 *   WIN32_StreamFile_ScalarDtor  0x463B90  -> compiler-generated
 *   WIN32_StreamFile_DtorBody    0x463BB0  -> ~WIN32_StreamFile()
 *   WIN32_StreamFile_CloseHandle 0x463C30  -> CloseHandle()
 *   WIN32_StreamFile_WriteChar   0x463CB0  -> WriteChar() override
 *   WIN32_StreamFile_Flush       0x463E50  -> Flush() override
 *   WIN32_StreamFile_SetBuffer   0x463F50  -> SetBuffer() override
 *
 * Derived fields start at +0x4C (the base subobject's exact size — see
 * WndProcStreamBuf.h for the WIN32_StreamMem sibling-class evidence):
 *   +0x4C fd_          CRT low-level file descriptor, -1 when closed
 *   +0x50 ownsHandle_  nonzero => the destructor must _close(fd_); zero
 *                       => the fd is borrowed from elsewhere, so the
 *                       destructor only flushes and leaves it open
 *
 * This class is not yet wired to any caller: the higher-level entry points
 * that construct/open it (WIN32_StreamOpen 0x463890, WIN32_StreamOpenFile
 * 0x463970, WIN32_StreamOpenPath 0x463AA0) are still no-op host stubs (see
 * shared/link_stubs.cpp / shared/defsym_stubs.cpp) — that layer is a
 * separate, larger piece of tracked work (PROGRESS.md "win32_stream.c
 * removed"). This file only supplies real bodies for the five class-method
 * addresses listed above plus their required, previously-unimplemented
 * callees (CloseHandle and the WNDPROC_StreamBuf base).
 */

// Status: VALIDATED

#pragma once

#include "WndProcStreamBuf.h"

class WIN32_StreamFile : public WNDPROC_StreamBuf {
public:
    WIN32_StreamFile();
    ~WIN32_StreamFile() override;

    int32_t WriteChar(int32_t ch) override;                 /* 0x463CB0 */
    int32_t Flush() override;                                /* 0x463E50 */
    void* SetBuffer(void* buffer, int32_t size) override;    /* 0x463F50 */

    /* WIN32_StreamFile_CloseHandle, 0x463C30. Not found at any vtable slot
     * in this batch's evidence, so kept as an ordinary member; called
     * directly by the destructor and available for the not-yet-wired
     * WIN32_StreamOpen* layer to call once that is reverse engineered. */
    WIN32_StreamFile* CloseHandle();

    int32_t fileHandle() const { return fd_; }
    void SetFileHandle(int32_t fd, int32_t ownsHandle) { fd_ = fd; ownsHandle_ = ownsHandle; }

private:
    int32_t fd_;
    int32_t ownsHandle_;
};
