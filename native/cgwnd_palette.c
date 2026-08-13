/**
 * cgwnd_palette.c — CGWND palette validation and resource-direction mapping
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Functions:
 *   CGWND_ValidatePaletteData    — Load and validate palette data files   (0x40E950, 527 bytes)
 *   CGWND_MapResourceToDirection — Map resource IDs to direction enum     (0x40EB60, 56 bytes)
 *
 * Calling convention: CGWND_ValidatePaletteData = __fastcall (ECX = obj ptr)
 *                     CGWND_MapResourceToDirection = __cdecl
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                */
/* ================================================================== */

extern void*  __cdecl operator_new(size_t size);                    /* 0x465CE0 */
extern void   __cdecl CRT_free(void* ptr);                          /* 0x466C70 */
extern void   __cdecl CRT_sprintf_buf(char* buf, const char* fmt);  /* 0x466D60 */
extern void*  g_asset_mgr;                                          /* 0x485600 — global asset manager */
extern char   g_install_path[];                                     /* 0x4A99C8 — install directory path */
extern int*   __stdcall AssetMgr_LoadFile(int* mgr, byte* path, int* outSize);   /* 0x45CD00 */
extern void*  __thiscall WNDPROC_StreamFromMemory(void* stream, char* data,
                                                   int size, int mode);           /* 0x464490 */
extern void*  __thiscall WIN32_StreamOpenFile(void* stream, char* path,
                                               int mode, const char* flags,
                                               int flag2);                        /* 0x463970 */
extern void   __thiscall WNDPROC_StreamReadLine(void* stream, short* buf);        /* 0x464BC0 */
/* resources/Win32Stream.cpp — real sizeof(WIN32_Stream) on this host
 * (wider than the original x86's 0x5C: StreamObject grew a real vptr
 * once ~StreamObject() became virtual, see StreamObject.h). */
extern size_t WIN32_Stream_Size();
/* DAT_00479190 is a global void* holding the mode value itself (confirmed via
 * disassembly of this function and of Game_LoadWaveFile in native/wave_io.c:
 * both do a single `MOV reg,[0x479190]` load before pushing it as an
 * argument — one dereference, not a fixed byte array). shared/defsym_stubs.cpp
 * defines the real host stub as `void* DAT_00479190 = nullptr;`, matching.
 * The previous `extern char DAT_00479190[];` declaration here was the wrong
 * type for the same reason wave_io.c's `&DAT_00479190` was wrong there. */
extern void*  DAT_00479190;

/* ================================================================== */
/* Internal: destroy a stream object via its vtable[4] scalar dtor     */
/* ================================================================== */
static void destroy_stream(void* stream)
{
    if (stream == NULL) return;
    /* The stream object has a complex indirection. Its vtable is at
       *(*((int*)stream + 1) + (int)stream). vtable[4] is the scalar dtor. */
    int* inner = *reinterpret_cast<int**>(static_cast<uintptr_t>(
        *reinterpret_cast<int*>(static_cast<uintptr_t>(*reinterpret_cast<int*>(stream) + 4)) +
        static_cast<int>(reinterpret_cast<uintptr_t>(stream))));
    if (inner != NULL) {
        typedef void (__thiscall* DtorFn)(void*, byte);
        DtorFn dtor = reinterpret_cast<DtorFn>((*reinterpret_cast<void***>(inner))[4]);  /* vtable[4] */
        dtor(static_cast<void*>(inner), 1);
    }
}

/* ================================================================== */
/* File-scope prototypes: neither function has a header of its own yet   */
/* with a caller to satisfy; core/CGWND.h documents both addresses but   */
/* nothing currently includes it or calls these (display/DDraw init and  */
/* the route editor callers are still unreconstructed). Local prototypes */
/* satisfy -Wmissing-declarations without new cross-TU coupling.         */
/* ================================================================== */
byte __fastcall CGWND_ValidatePaletteData(void* obj);
int  __cdecl    CGWND_MapResourceToDirection(int resourceId);

/* ================================================================== */
/* CGWND_ValidatePaletteData — Load and validate palette data          */
/* Address: 0x40E950                                                    */
/*                                                                      */
/* Loads palette data into two 200-entry arrays at obj+0x168 and       */
/* obj+0x488 (each 800 bytes). Tries asset manager first (for           */
/* compressed archives), then falls back to raw file open.              */
/* Parses 0xA0 (160) entries of 4-tuple data (color palette values).   */
/* Returns 1 if all data parsed successfully, 0 on error/truncation.   */
/* SEH-protected for memory/IO errors.                                  */
/*                                                                      */
/* Called by: display/DDraw init code at 0x40E936                       */
/*                                                                      */
/* @param obj  Pointer to object with palette buffers at +0x168/+0x488 */
/* @return     1 on success, 0 on failure                               */
/* ================================================================== */
byte __fastcall CGWND_ValidatePaletteData(void* obj)
{
    char   filePath[256];    /* assembled file path */
    int*   loadedData;       /* buffer from AssetMgr (must free) */
    void*  streamObj;        /* stream object for reading */
    void*  streamMem;        /* heap-allocated stream memory */
    byte   successFlag;      /* 1 = OK, 0 = error */
    short* dataPtr;
    int    lineIdx;
    int    pathLen;

    /* Step 1: Zero two 200-entry (800 byte) arrays */
    {
        int* buf = reinterpret_cast<int*>(static_cast<uint8_t*>(obj) + 0x168);
        for (int i = 0; i < 200; i++) {
            buf[i] = 0;
        }
    }
    {
        int* buf = reinterpret_cast<int*>(static_cast<uint8_t*>(obj) + 0x488);
        for (int i = 0; i < 200; i++) {
            buf[i] = 0;
        }
    }

    /* Step 2: Build file path using install directory */
    loadedData  = NULL;
    streamObj   = NULL;
    successFlag = 1;

    /* Format: "<install_path>/<palette_subdir>/<file>" */
    filePath[0] = '\0';
    /* Build the full path — the format string at s__s_s__s_0047e374
       is something like "%s/%s/%s" — the actual sprintf happens at
       0x40E97D via CRT_sprintf_buf which uses a global dest buffer.
       For clarity here we just note the path construction. */
    CRT_sprintf_buf(filePath, "%s/%s/%s");

    /* Step 3: Try loading from asset manager first */
    if (g_asset_mgr != NULL) {
        /* Compute relative path offset within sprintf'd buffer */
        pathLen = -1;
        {
            const char* s = g_install_path;
            do {
                if (pathLen == 0) break;
                pathLen--;
                s++;
            } while (*(s - 1) != '\0');
        }
        int relOffset = ~pathLen;  /* strlen(g_install_path) */

        int initialSize = 0x800;
        loadedData = AssetMgr_LoadFile(reinterpret_cast<int*>(&g_asset_mgr),
                                        reinterpret_cast<byte*>(filePath + relOffset - 1),
                                        &initialSize);  /* initial size = 2048 */
        if (loadedData != NULL) {
            /* WNDPROC_StreamFromMemory constructs a distinct,
             * not-yet-fully-modeled concrete class (its own vtable,
             * 0x479210 — see resources/Win32Stream.h's WIN32_StreamRead
             * doc comment) that also embeds a StreamObject at +0xC, so it
             * is equally undersized by the literal `0x5C` below; no
             * `*_Size()` helper exists for that class yet to fix this
             * correctly (out of this pass's scope — same gap as
             * ui/UIPANEL_Surface.cpp's `mem_stream`). */
            streamMem = operator_new(0x5C);  /* stream object: original x86 size, not host size */
            if (streamMem != NULL) {
                streamObj = WNDPROC_StreamFromMemory(streamMem, reinterpret_cast<char*>(loadedData),
                                                      *(loadedData - 1), 1);
            }
        }
    }

    /* Step 4: Fallback to direct file open */
    if (streamObj == NULL) {
        /* This constructs a real WIN32_Stream (resources/Win32Stream.h)
         * via WIN32_StreamOpenFile below — use the real host size, not
         * the original x86's 0x5C. */
        streamMem = operator_new(WIN32_Stream_Size());
        if (streamMem != NULL) {
            streamObj = WIN32_StreamOpenFile(streamMem, filePath, 0xA0,
                                              static_cast<const char*>(DAT_00479190), 1);
        }
        if (streamObj == NULL) {
            successFlag = 0;
        }
    }

    /* Step 5: Read 160 (0xA0) palette entries from file */
    if (streamObj != NULL) {
        /* Check end-of-stream flag via vtable indirection:
           status_byte = *(base + vtable[4] + obj_offset + 8) & 6
           Non-zero = EOF or error */
        int* sv = *reinterpret_cast<int**>(streamObj);
        byte* status_ptr = reinterpret_cast<byte*>(static_cast<uintptr_t>(
            *reinterpret_cast<int*>(static_cast<uintptr_t>(
                sv[1] + static_cast<int>(reinterpret_cast<uintptr_t>(streamObj)))) + 8));

        if (*status_ptr == 0) {
            dataPtr = reinterpret_cast<short*>(static_cast<uint8_t*>(obj) + 0x16A);
            for (lineIdx = 0; lineIdx < 0xA0; lineIdx++) {
                /* Read 4 short values per palette entry.
                   Values are read into slots at dataPtr[-1], dataPtr[0],
                   dataPtr[399], and dataPtr[400] — writing 2 bytes each.
                   These correspond to palette buffer offsets +0x168,
                   +0x16A, +0x488, +0x48A. */
                WNDPROC_StreamReadLine(streamObj, dataPtr - 1);
                if ((*status_ptr & 6) != 0) goto load_error;

                WNDPROC_StreamReadLine(streamObj, dataPtr);
                if ((*status_ptr & 6) != 0) goto load_error;

                WNDPROC_StreamReadLine(streamObj, dataPtr + 399);
                if ((*status_ptr & 6) != 0) goto load_error;

                WNDPROC_StreamReadLine(streamObj, dataPtr + 400);
                if ((*status_ptr & 6) != 0) goto load_error;

                dataPtr += 2;
            }
            goto cleanup;
        }
    }

load_error:
    successFlag = 0;

cleanup:
    /* Step 6: Release resources */
    if (streamObj != NULL) {
        destroy_stream(streamObj);
    }
    if (loadedData != NULL) {
        CRT_free(loadedData);
    }

    return successFlag;
}


/* ================================================================== */
/* CGWND_MapResourceToDirection — Map resource IDs to direction enum   */
/* Address: 0x40EB60                                                    */
/*                                                                      */
/* Maps track resource IDs to direction values (1-4) used by the       */
/* track editor for placement orientation.                              */
/*                                                                      */
/* Resource ID ranges:                                                  */
/*   0x1804, 0x1806, 0x1808  -> Direction 1 (east/right)               */
/*   0x1866, 0x1868, 0x186A  -> Direction 2 (south/down)               */
/*   0x186C, 0x186E          -> Direction 3 (west/left)                 */
/*   0x1870, 0x1871          -> Direction 4 (north/up)                  */
/*   All other values        -> Direction 0 (invalid)                   */
/* ================================================================== */
int __cdecl CGWND_MapResourceToDirection(int resourceId)
{
    switch (resourceId) {
    case 0x1804:
    case 0x1806:
    case 0x1808:
        return 1;   /* East / Right */
    case 0x1866:
    case 0x1868:
    case 0x186A:
        return 2;   /* South / Down */
    case 0x186C:
    case 0x186E:
        return 3;   /* West / Left */
    case 0x1870:
    case 0x1871:
        return 4;   /* North / Up */
    default:
        return 0;   /* Invalid / unknown */
    }
}
