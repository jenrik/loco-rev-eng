/**
 * Win32OStream.cpp — WIN32_OStream implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly.
 *
 * See Win32OStream.h for the class/address map and the vbtable/size
 * evidence distinguishing this class from WIN32_Stream. This file
 * transcribes:
 *   WIN32_OStream::WIN32_OStream(path,flags,shareMask)  0x465090
 */

// Status: VALIDATED

#include "Win32OStream.h"

#include <new>

/* ================================================================== */
/* WIN32_OStream::WIN32_OStream(path,flags,shareMask) — 0x465090        */
/* (Ghidra auto-analysis mislabeled this "CRT_floor"; it is not the CRT */
/* floor() function — see Win32OStream.h)                               */
/* ================================================================== */
WIN32_OStream::WIN32_OStream(const char* path, uint32_t flags, uint32_t shareMask)
{
    AttachBuffer(new WIN32_StreamFile());
    owns_rdbuf = 1;

    WIN32_StreamFile* file = static_cast<WIN32_StreamFile*>(rdbuf);
    if (file->Open(path, flags | 2, shareMask) == nullptr) {
        state_bits |= kFailBit;
    }
}

/* ================================================================== */
/* Free-function facades — see Win32OStream.h for scope/linkage notes  */
/* ================================================================== */

size_t WIN32_OStream_Size()
{
    return sizeof(WIN32_OStream);
}

void* WIN32_StreamOpenWriteFile(void* stream, const char* path, uint32_t flags,
                                 uint32_t shareMask, int /*initBase*/)
{
    return ::new (stream) WIN32_OStream(path, flags, shareMask);
}
