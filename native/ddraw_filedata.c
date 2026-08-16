/**
 * ddraw_filedata.c — File resource data loading and cleanup
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions manage a FileData struct (16 bytes) used by the
 * ResourceManager to load chunked resource files (.RES/.PKG):
 *
 * struct FileData {
 *     int       file_handle;    // +0x00  CRT file handle
 *     ChunkNode* chunk_list;    // +0x04  linked list of parsed chunks
 *     int       field_08;       // +0x08  flags/state, zeroed on close
 *     char*     filename;       // +0x0C  malloc'd filename string
 * };
 *
 * Each ChunkNode (16 bytes) in the linked list:
 * struct ChunkNode {
 *     char*     data;           // +0x00  malloc'd chunk data blob
 *     int       chunk_id;       // +0x04  chunk identifier
 *     int       data_size;      // +0x08  chunk data byte count
 *     ChunkNode* next;          // +0x0C  next node in singly-linked list
 * };
 *
 * Per-chunk read order (confirmed against the 0x45CAA0 disassembly): a
 * 4-byte length, that many bytes of name/data, then two more 4-byte
 * fields (data_size, then chunk_id) — see the read loop below for exactly
 * which raw read feeds which ChunkNode field; a from-memory guess at "the
 * file format" here previously did not match the actual value flow.
 *
 * DDRAW_LoadFile is __thiscall — it's a C++ method on a FileData struct.
 * DDRAW_FileData_Dtor is __fastcall — the destructor/cleanup for the same.
 * DDRAW_FreeClipper (in ddraw_clippers.c) zeros a different struct.
 */

#include <stdint.h>

/* ================================================================== */
/* External functions                                                  */
/* ================================================================== */

extern void*  operator_new(uint32_t size);              /* 0x465CE0 */
extern void   __cdecl GLOBAL_free(void* ptr);           /* 0x465CD0 */
extern void   __cdecl CRT_free(void* ptr);              /* 0x466C70 */
extern void*  __cdecl CRT_malloc_zero(uint32_t size);   /* 0x4673C0 */

/* CRT file read helpers */
/* mode stays void* to match the real definition's signature exactly
 * (shared/stubs_impl.cpp: `void CRT_0x468480(char*, void*) {}` — also
 * matched by graphics/DDRAW.cpp/resources/AssetMgr.h's OTHER, differently
 * mangled `(const char*, const char*)` declaration of the same name,
 * itself a separate pre-existing landmine not touched here). Changing
 * this to `const char*` would change the mangled symbol this TU calls,
 * silently creating a new call-0 site against the void* stub — verified
 * by objdump before/after. DAT_00481190 is a real read-only string, so
 * passing it to a void* parameter still needs a cast; const_cast (not a
 * lossy C-style cast) makes the const-stripping explicit. */
extern int32_t __cdecl CRT_0x468480(char* filename, void* mode);  /* fopen-like open */
extern void   __cdecl CRT_0x4681D0(int32_t handle);               /* fclose-like close */
extern int32_t __cdecl CRT_0x468610(void* buf, uint32_t size,      /* fread-like read */
                                      uint32_t count, int32_t handle);
extern void   __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...);  /* 0x466D60 */
/* CRT_wcsstr (0x471480) was declared here as `void __cdecl CRT_wcsstr(...)`
 * — wrong return type (real function returns int32_t; a genuine _stricmp-
 * style compare, see game/Building.cpp's top-of-file comment) and unused
 * in this file (zero call sites). This file is compiled as C++ (meson's
 * `-x c++` for native/*.c) with no extern "C" block around this
 * declaration, so it would in practice still link to the real
 * shared/stubs_link001_batch1_crt_win32.cpp body (return type isn't part
 * of the Itanium C++ mangled name) — but silently discarding a real
 * comparison result the moment anyone adds a call is exactly the kind of
 * trap this sweep is closing. Removed 2026-08-16. */

/* ================================================================== */
/* ChunkNode struct (16 bytes)                                         */
/* ================================================================== */
typedef struct ChunkNode {
    char*           data;       /* +0x00  malloc'd chunk data */
    int32_t         chunk_id;   /* +0x04  chunk type identifier */
    int32_t         data_size;  /* +0x08  chunk data byte count */
    struct ChunkNode* next;     /* +0x0C  next in singly-linked list */
} ChunkNode;

/* ================================================================== */
/* FileData struct (16 bytes)                                          */
/* ================================================================== */
typedef struct {
    int32_t     file_handle;    /* +0x00  CRT file handle */
    ChunkNode*  chunk_list;     /* +0x04  head of chunk linked list */
    int32_t     field_08;       /* +0x08  state/flags */
    char*       filename;       /* +0x0C  malloc'd filename copy */
} FileData;

/* ================================================================== */
/* External constants                                                  */
/* ================================================================== */

extern const char DAT_00481190[4];     /* fopen mode "rb" */
extern const char DAT_0048118c[4];     /* ".RES" extension */
extern const char DAT_00481194[4];     /* ".PKG" extension */

/* ================================================================== */
/* File-scope prototypes: graphics/DDRAW.h documents a `struct FileData`
 * and declarations for both functions, but with a different FileData
 * layout (void* file_handle/block_list vs. this file's int32_t/ChunkNode*)
 * and mismatched return/param types; network/NetHelpers.cpp has yet a
 * third (`DDRAW_FileData_Dtor(void*)`) that nothing here defines. None of
 * those are currently wired to an actual call using THIS file's types, so
 * a local prototype satisfies -Wmissing-declarations without adopting or
 * fixing that separately-tracked, pre-existing cluster. */
/* ================================================================== */
void __fastcall DDRAW_FileData_Dtor(FileData* fd);
uint8_t __thiscall DDRAW_LoadFile(FileData* fd, char* filename);

/* ================================================================== */
/* DDRAW_FileData_Dtor — Destructor for FileData struct               */
/* Address: 0x45CA20                                                   */
/* Size: 123 bytes (43 insn)                                           */
/* Calling convention: __fastcall (param_1 in ECX = FileData*)         */
/*                                                                     */
/* Frees the file handle, walks the chunk linked list freeing each     */
/* node's data blob and the node itself, then frees the filename.      */
/*                                                                     */
/* Does NOT free the FileData struct itself — caller owns memory.      */
/*                                                                     */
/* Called by: NET_Shutdown (?), direct call sites                      */
/*                                                                     */
/* @param fd  FileData* to clean up                                    */
/* ================================================================== */
void __fastcall DDRAW_FileData_Dtor(FileData* fd)
{
    /* Close file handle if open */
    if (fd->file_handle != 0) {
        CRT_0x4681D0(fd->file_handle);
        fd->file_handle = 0;
    }

    /* Walk and free the chunk linked list */
    ChunkNode* node = fd->chunk_list;
    fd->field_08 = 0;

    while (node != NULL) {
        ChunkNode* next = node->next;

        /* Free chunk data blob if present */
        if (node->data != NULL) {
            CRT_free(node->data);
            node->data = NULL;
        }

        /* Free the chunk node itself */
        GLOBAL_free(node);
        node = next;
    }

    fd->chunk_list = NULL;

    /* Free filename string if present */
    if (fd->filename != NULL) {
        CRT_free(fd->filename);
        fd->filename = NULL;
    }
}

/* ================================================================== */
/* DDRAW_LoadFile — Load and parse a chunked resource file            */
/* Address: 0x45CAA0                                                   */
/* Size: 603 bytes (219 insn)                                          */
/* Calling convention: __thiscall (ECX = FileData*, 1 stack param)    */
/*                                                                     */
/* Opens a resource file (.PKG or .RES), reads the extension to        */
/* determine file type, then reads chunks in a loop. Each iteration    */
/* does 4 raw reads — see the read loop below for the exact value flow.*/
/* Each chunk is stored as a ChunkNode appended to a singly-linked     */
/* list at fd->chunk_list (+0x04).                                     */
/*                                                                     */
/* After reading all chunks, closes the first file handle and opens    */
/* the .RES counterpart (if original was .PKG) or vice versa.          */
/* The filename used is stored at fd->filename (+0x0C).                */
/*                                                                     */
/* Called by: ResourceManager_Init (0x4460A4) — with ECX=param_1+0x18 */
/*                                                                     */
/* @param filename  Path to the resource file to load                  */
/* @return          TRUE (1) on success, FALSE (0) on failure           */
/* ================================================================== */
uint8_t __thiscall DDRAW_LoadFile(FileData* fd, char* filename)
{
    char local_name[400];   /* stack buffer for filename manipulation */
    char chunk_name[400];   /* stack buffer for chunk names */
    char ext_buffer[4];     /* extension string */

    /* Close any existing file handle + reset */
    if (fd->file_handle != 0) {
        CRT_0x4681D0(fd->file_handle);
        fd->file_handle = 0;
        fd->field_08 = 0;
    }

    /* Copy filename to local buffer */
    {
        const char* src = filename;
        char* dst = local_name;
        while (*src != '\0') {
            *dst++ = *src++;
        }
        *dst = '\0';
    }

    /* Find extension (last '.') */
    {
        char* ext_ptr = NULL;
        char* p = local_name;
        while (*p != '\0') {
            if (*p == '.') {
                ext_ptr = p;
            }
            p++;
        }

        if (ext_ptr == NULL) {
            return 0;  /* no extension found */
        }

        ext_ptr++;  /* skip the '.' */

        /* Build extension name (starting from ext_ptr). DAT_00481194 is a
         * real 4-byte ASCII constant (".PKG", verified via Ghidra memory
         * read), so it decays to `const char*` on its own — no cast, and
         * no `&` (taking its address would give `const char(*)[4]`, the
         * wrong type, which is what forced the previous const-discarding
         * C-style cast here). */
        CRT_sprintf_buf(ext_ptr, DAT_00481194);  /* ".PKG" */

        /* Open file for reading. DAT_00481190 ("rb") is a real ASCII
         * string constant (verified via Ghidra memory read) — unlike
         * DAT_00479190 in wave_io.c/cgwnd_palette.c, which is a plain
         * scalar, not a string. const_cast (rather than a lossy C-style
         * cast) makes the const-stripping explicit; CRT_0x468480 never
         * writes through this pointer (it's a no-op stub on the host, and
         * the real fopen-like function only reads a mode string). */
        fd->file_handle = CRT_0x468480(local_name, const_cast<char*>(DAT_00481190));  /* "rb" */

        if (fd->file_handle == 0) {
            return 0;
        }
    }

    /* Read chunk data loop */
    {
        while ((*reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(fd->file_handle + 0xC)) & 0x10) == 0) {
            /* Read #1: 4 bytes -> name_len. This value is BOTH the byte
             * count for read #2 AND the allocation size for node->data
             * below — it must survive unmodified until both of those
             * happen (Ghidra's decompile captures &puVar4[2] before
             * puVar4 is ever reused, keeping this value distinct from the
             * one captured by read #4 below; a prior version of this file
             * used a single `chunk_size` variable for both reads #1 and
             * #4, so by the time of the malloc/field-assignment below it
             * was already overwritten by read #4's value). */
            uint32_t name_len;
            uint32_t bytes_read = CRT_0x468610(&name_len, 1, 4, fd->file_handle);
            if (bytes_read == 0) {
                /* Ghidra: a short/zero read here falls through to the
                 * same EOF re-check the loop condition performs (disasm
                 * 0x45CB93 -> 0x45CC59 -> back to 0x45CB78, or out to the
                 * close+reopen tail if the EOF flag really is set) — it
                 * does not abort chunk parsing outright. A bare `break`
                 * would stop early on a transient short read that isn't
                 * actually end-of-file. */
                continue;
            }

            /* Read #2: name_len bytes -> chunk_name (variable-length
             * name/data blob; the original does not bounds-check
             * name_len against the 400-byte buffer either — preserved
             * as-is). */
            CRT_0x468610(chunk_name, 1, name_len, fd->file_handle);

            /* Read #3: 4 bytes -> data_size field (ChunkNode +0x08). */
            uint32_t data_size_val;
            CRT_0x468610(&data_size_val, 1, 4, fd->file_handle);

            /* Read #4: 4 bytes -> chunk_id field (ChunkNode +0x04). */
            uint32_t chunk_id_val;
            CRT_0x468610(&chunk_id_val, 1, 4, fd->file_handle);

            /* Allocate and populate chunk node. node->data is sized off
             * name_len (read #1's untouched value). */
            ChunkNode* node = static_cast<ChunkNode*>(operator_new(0x10));
            char* node_data = static_cast<char*>(CRT_malloc_zero(name_len));

            node->data = node_data;
            node->data_size = static_cast<int32_t>(data_size_val);

            /* Copy chunk name */
            {
                const char* src2 = chunk_name;
                char* dst2 = node_data;
                while (*src2 != '\0') {
                    *dst2++ = *src2++;
                }
                *dst2 = '\0';
            }

            node->chunk_id = static_cast<int32_t>(chunk_id_val);
            node->next = NULL;

            /* Append to linked list (tail insert) */
            if (fd->chunk_list == NULL) {
                fd->chunk_list = node;
            } else {
                ChunkNode* tail = fd->chunk_list;
                while (tail->next != NULL) {
                    tail = tail->next;
                }
                tail->next = node;
            }
        }
    }

    /* Close first file handle */
    CRT_0x4681D0(fd->file_handle);

    /* Build alternate filename (.RES extension) */
    /* ext_ptr is still valid from above */
    {
        char* p2 = local_name;
        while (*p2 != '\0') p2++;
        /* Back up to extension */
        while (p2 > local_name && *p2 != '.') p2--;
        if (*p2 == '.') p2++;
        CRT_sprintf_buf(p2, DAT_0048118c);  /* ".RES" */

        /* Open second file */
        fd->file_handle = CRT_0x468480(local_name, const_cast<char*>(DAT_00481190));
    }

    /* Store filename */
    {
        const char* src3 = local_name;
        uint32_t len = 0;
        while (src3[len] != '\0') len++;
        char* name_copy = static_cast<char*>(CRT_malloc_zero(len + 1));
        fd->filename = name_copy;

        src3 = local_name;
        while (*src3 != '\0') {
            *name_copy++ = *src3++;
        }
        *name_copy = '\0';
    }

    fd->field_08 = 0;

    if (fd->file_handle == 0) {
        return 0;
    }

    return 1;
}
