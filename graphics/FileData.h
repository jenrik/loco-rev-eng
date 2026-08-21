/**
 * FileData.h — loaded game resource file descriptor
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Canonical layout for the FileData/ChunkNode structs DDRAW_LoadFile
 * (0x45CAA0) and DDRAW_FileData_Dtor (0x45CA20) operate on, confirmed
 * against the 0x45CAA0 disassembly's actual read order (see
 * native/ddraw_filedata.c's implementation comment for the byte-level
 * evidence). Split into its own header so both graphics/DDRAW.h and
 * resources/ResourceManager.h (which embeds a FileData member for
 * ResourceManager::Init's Step 3 archive load) see one identical
 * definition rather than two independently-guessed layouts.
 *
 * Size on the original x86 target: 0x10 bytes (all four members are
 * 4-byte pointers/ints there). On this 64-bit host, chunk_list/filename
 * are 8-byte pointers, so sizeof(FileData) is larger — always use
 * sizeof(FileData), never the literal 0x10, when allocating one.
 */
#pragma once

#include <cstdint>

struct ChunkNode {
    char*      data;       /* +0x00  malloc'd chunk name/data blob */
    int32_t    chunk_id;   /* +0x04  chunk type identifier */
    int32_t    data_size;  /* +0x08  chunk data byte count */
    ChunkNode* next;       /* +0x0C  next in singly-linked list */
};

struct FileData {
    int32_t    file_handle;  /* +0x00  CRT file handle (int32-as-FILE*,
                               * see shared/stubs_impl.cpp's
                               * CRT_0x468480/610/4681D0) */
    ChunkNode* chunk_list;   /* +0x04  head of chunk linked list */
    int32_t    field_08;     /* +0x08  state/flags, zeroed on close */
    char*      filename;     /* +0x0C  malloc'd filename copy */
};
