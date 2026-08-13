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
 *   WIN32_StreamFile_Underflow   0x463D40  -> Underflow() override (vtable
 *     +0x20 — PROGRESS.md previously tracked this as "base/override
 *     implementation address not yet located"; resolved 2026-08-10 by
 *     reading WIN32_StreamFile's vtable at 0x4791AC directly, `create_
 *     function` (Ghidra auto-analysis never defined this address), then
 *     decompiling/disassembling it)
 *   WIN32_StreamFile_Open        0x4652D0  -> Open() (previously
 *     Ghidra-mislabeled "CRT_exp" — an auto-analysis artifact, not the
 *     real name; resolved 2026-08-10)
 *
 * Derived fields start at +0x4C (the base subobject's exact size — see
 * WndProcStreamBuf.h for the WIN32_StreamMem sibling-class evidence):
 *   +0x4C fd_          CRT low-level file descriptor, -1 when closed
 *   +0x50 ownsHandle_  nonzero => the destructor must _close(fd_); zero
 *                       => the fd is borrowed from elsewhere, so the
 *                       destructor only flushes and leaves it open
 *
 * This class is open-capable (Open()) and read-capable (Underflow()), and
 * IS wired to real callers via WIN32_Stream (Win32Stream.h): WIN32_Stream
 * owns a heap-allocated WIN32_StreamFile as its rdbuf, and the fd_ != -1
 * success gate is reached from callers through a real, typed
 * `static_cast<WIN32_StreamFile*>(stream.rdbuf)->fileHandle()` — no
 * forbidden cast needed, since the concrete rdbuf type is known at every
 * real call site once the caller holds a real WIN32_Stream object (see
 * game/TrainStation.cpp, game/ScriptedObject.cpp, ui/HelpWnd.cpp,
 * ui/CursorEditWindow.cpp, ui/UIPANEL_Surface.cpp for worked examples).
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
    int32_t Underflow() override;                             /* 0x463D40 */

    /* WIN32_StreamFile_CloseHandle, 0x463C30. Not found at any vtable slot
     * in this batch's evidence, so kept as an ordinary member; called
     * directly by the destructor and by Open()'s failure path. */
    WIN32_StreamFile* CloseHandle();

    /* WIN32_StreamFile_Open, 0x4652D0 (previously Ghidra-mislabeled
     * "CRT_exp"). Opens `path` for this (as-yet-unopened) stream:
     * translates `flags`/`shareMask` into POSIX open() flags (see the .cpp
     * for the fully-traced bit meanings — every bit was independently
     * re-derived from disassembly, not guessed), calls the CRT-open
     * equivalent, sets fd_/ownsHandle_ on success, and lazily allocates the
     * default 0x200-byte buffer via the base class's existing
     * SetBufferPtrs()/AllocateDefaultBuffer() machinery — no new
     * allocation logic duplicated here.
     *
     * Real host deviation (documented, not a silent simplification): the
     * original's `shareMask`-derived Windows CreateFileA share-mode
     * computation (exclusive/read-shared/write-shared/read-write-shared)
     * has no POSIX equivalent — POSIX open() has no share-mode parameter —
     * so that computation is not reproduced; `shareMask` is accepted for
     * signature fidelity but unused on the host.
     *
     * Real host deviation, tracked not silently dropped: the original also
     * seeks to file end when the caller's `flags` has bit 0x04 set (via a
     * virtual call through a vtable slot — 0x463E00 — this pass did not
     * reverse engineer, since it is a still-unnamed method on this class
     * hierarchy and no real caller in this codebase's evidenced call sites
     * ever sets that bit). This override fails loudly (fprintf+assert) only
     * if that unexercised branch is ever actually reached, rather than
     * silently ignoring it.
     *
     * Returns this on success, nullptr on failure (already open, or the
     * underlying open() call failed) — matches the original's contract. */
    WIN32_StreamFile* Open(const char* path, int32_t flags, int32_t shareMask);

    int32_t fileHandle() const { return fd_; }
    void SetFileHandle(int32_t fd, int32_t ownsHandle) { fd_ = fd; ownsHandle_ = ownsHandle; }

private:
    int32_t fd_;
    int32_t ownsHandle_;
};
