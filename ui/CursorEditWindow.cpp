/**
 * CursorEditWindow.cpp — CursorEditWindow implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * CursorEditWindow loads cursor data (.dat cursor metrics + .bmp pixel
 * data) from the game's install directory or AssetMgr archive. It is
 * created as a child window via the ResourceManager, with the cursor
 * name derived from the resource ID's text string.
 */

// Status: INTEGRATED

#include "CursorEditWindow.h"
#include <cstring>
#include <cassert>
#include <cstdio>
#include <new>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Heap management */
extern void* __cdecl operator_new(size_t size);     /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);        /* 0x465CD0 */
extern void  __cdecl CRT_free(void* ptr);           /* 0x466C70 */
extern int   __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...); /* 0x466D60 */

/* Stream / WNDPROC helpers */
extern void   __thiscall WIN32_StreamOpen(void* stream, int mode);        /* 0x463890 */
extern void   __thiscall WIN32_StreamDestroy(void* stream);                /* 0x463A80 */
extern void   __thiscall WIN32_StreamDestroyImmediate(void* stream);       /* 0x463B10 */
extern void   __thiscall WNDPROC_StreamCleanup(void* stream);              /* 0x464620 */
extern int*   __thiscall WNDPROC_StreamFromMemory(void* stream, const char* data,
                                                   int size, int mode);    /* 0x464490 */

/* Cursor data parsing helpers */
extern void*  __thiscall WNDPROC_CriticalSectionLock(void* stream,
                                                      int* errorCode,
                                                      int16_t* fieldY,
                                                      int16_t* fieldX,
                                                      int* tempBuf);      /* 0x4649F0 */
extern void*  __thiscall WNDPROC_StreamPrintf(void* stream, int* outVal);  /* 0x464750 */
extern void*  __thiscall WNDPROC_StreamWrite(void* stream, int* outVal);   /* 0x4646C0 */
extern uint8_t __fastcall CGWND_ValidatePaletteData(int classPtr);        /* 0x40E950 */

/* Asset manager */
extern int*   __thiscall AssetMgr_LoadFile(void* mgr, const char* path,
                                            int* outSize);                 /* 0x45CD00 */

extern "C" {
    void WIN32_StreamOpenPath(void* stream, const char* path, int32_t mode, int32_t fileType); /* 0x463AA0 */
}

namespace {
using StreamDestructor = void (__fastcall *)(void*);

void destroy_memory_stream(int* stream_result)
{
    void** stream_vtable = *reinterpret_cast<void***>(stream_result);
    const auto* offset_bytes = reinterpret_cast<const uint8_t*>(stream_vtable) + 4;
    const uintptr_t stream_offset = *reinterpret_cast<const uintptr_t*>(offset_bytes);
    auto* stream_base = reinterpret_cast<uint8_t*>(stream_result) + stream_offset;
    auto destroy = reinterpret_cast<StreamDestructor>(
        *reinterpret_cast<void**>(stream_base));
    destroy(stream_result);
}
}

/* ================================================================== */
/* Global variables                                                    */
/* ================================================================== */

extern void* g_asset_mgr;             /* 0x485600 — asset manager */
extern char  g_install_path[];        /* 0x4A99C8 — install directory path */

/* ================================================================== */
/* CursorEditWindow Constructor                                        */
/* Address: 0x40E600                                                   */
/*                                                                      */
/* Initializes as a ChildWindow (with nameParam=0 to defer loading     */
/* in base), then calls init() to load cursor data in derived context. */
/* ================================================================== */
CursorEditWindow::CursorEditWindow(uint32_t resourceId, int32_t nameParam)
    : ChildWindow(resourceId, 0)  /* Base constructor with nameParam=0 */
{
    /* Load cursor data via derived init() */
    this->init(resourceId, nameParam);
}

/* ================================================================== */
/* CursorEditWindow::~CursorEditWindow (destructor)                    */
/* Address: 0x40E660 (scalar-deleting-destructor thunk)                */
/* Base cleanup: 0x40E680                                              */
/*                                                                      */
/* Virtual destructor. Compiler-managed; base class destructor         */
/* ~ChildWindow() is called automatically.                             */
/* ================================================================== */
CursorEditWindow::~CursorEditWindow()
{
    /* Compiler-managed destruction chain: this->~CursorEditWindow() →
       base->~ChildWindow(). No explicit body needed. */
}

/* ================================================================== */
/* CursorEditWindow::Render                                            */
/* Address: 0x40E8D0                                                   */
/* Vtable slot: [3] +0x0C                                              */
/*                                                                      */
/* Reads cursor metrics from a .dat stream. Initializes field_7A8      */
/* (width/X coordinate) and field_7AA (height/Y coordinate) by calling */
/* helper functions to parse and validate stream data.                 */
/*                                                                      */
/* Called by: CursorEditWindow::init() [virtual dispatch]              */
/* ================================================================== */
uint8_t CursorEditWindow::Render(void* stream)
{
    /* Local variable for error tracking: initialized to 0, may be set by
       parsing functions. Value 0xfffffff7 (-9) indicates success. */
    int errorCode = 0;

    /* Local variable for temp storage during read operations */
    int tempBuf = 0;

    /* Result flag: starts as failure (0), set to success (1) if processing
       occurs without stream errors */
    uint8_t resultFlag = 0;

    /* Validate stream is not null */
    if (stream == nullptr) {
        return 0;
    }

    /* Check stream validity via vtable. Stream object layout:
       [0] = vtable pointer
       vtable[1] = offset to stream control data
       At [vtable[1] + stream + 0x8] is a flag byte where bit 0x4 indicates
       the stream has an error condition. */
    int* streamVtable = *(int**)stream;
    if (streamVtable == nullptr) {
        return 0;
    }

    int vtableOffset = streamVtable[1];  /* +0x4 in vtable */
    uint8_t* flagPtr = (uint8_t*)((uintptr_t)vtableOffset + (uintptr_t)stream + 0x8);
    uint8_t streamFlags = *flagPtr;     /* +0x8 relative to vtableOffset */

    /* If stream error flag (bit 0x4) is already set, skip processing */
    if ((streamFlags & 0x4) != 0) {
        return 0;
    }

    /* Set result to success; may be cleared below if error detected */
    resultFlag = 1;

    /* Call helper function to read cursor data from stream.
       WNDPROC_CriticalSectionLock acquires stream lock, reads whitespace-
       delimited values, and populates the cursor field data. */
    WNDPROC_CriticalSectionLock(
        stream,
        &errorCode,
        &this->field_7AA,    /* +0x7AA — cursor field (Y/height) */
        &this->field_7A8,    /* +0x7A8 — cursor field (X/width) */
        &tempBuf);

    /* Chain three more stream read operations. Each function returns a
       stream pointer (or derived object) that becomes the this pointer
       (in ECX) for the next call. */
    void* streamPtr1 = WNDPROC_StreamPrintf(stream, &tempBuf);
    void* streamPtr2 = WNDPROC_StreamPrintf(streamPtr1, nullptr);
    WNDPROC_StreamWrite(streamPtr2, nullptr);

    /* Validate error code: if errorCode was set to -9 (0xfffffff7) by the
       parsing functions, it indicates parsing succeeded. Any other value
       (including 0) indicates an error or parsing failure. */
    if (errorCode != (int)0xfffffff7) {
        resultFlag = 0;
    }

    /* Call palette validation function on this object. This validates or
       loads palette data associated with the cursor. Return value is not
       used for Render success status. */
    CGWND_ValidatePaletteData((int)(uintptr_t)this);

    return resultFlag;
}

/* ================================================================== */
/* CursorEditWindow::init                                              */
/* Address: 0x40E690                                                   */
/*                                                                      */
/* Loads cursor data from disk or AssetMgr. The nameParam (cast from   */
/* int32_t ABI) controls whether loading occurs (0 = no-op).           */
/* When non-zero:                                                       */
/*                                                                      */
/*   1. Build file paths:                                               */
/*        datPath    = "%s\\<name>.dat"                                 */
/*        bmpPath    = "%s\\<name>.bmp"  (inherited +0x48)              */
/*                                                                      */
/*   2. Try AssetMgr first: load "<name>.dat" from archive             */
/*      - If found: create memory stream, call Render() to load,       */
/*        call ChildWindow::Render() to render, mark loaded flag       */
/*                                                                      */
/*   3. Fall back to direct file:                                      */
/*      - Open "%s\\<name>.dat" from install dir                       */
/*      - If valid: Render() load, ChildWindow::Render()               */
/*                                                                      */
/*   4. Store success/failure in loaded (+0x162)                       */
/* ================================================================== */
void CursorEditWindow::init(uint32_t resourceId, int32_t nameParam)
{
    /* Local stream for file operations */
    int localStream[22];  /* WIN32_Stream object (88 bytes) */
    WIN32_StreamOpen(&localStream[0], 1);

    int* pLoadedData = nullptr;  /* AssetMgr loaded data pointer */
    int  dataSize = 0;

    /* Clear cursor-specific state fields */
    this->field_7A8 = 0;           /* +0x7A8 (short) */
    this->field_7AA = 0;           /* +0x7AA (short) */
    this->loaded = 0;              /* +0x162 (inherited, byte) */

    /* If nameParam is 0 (null pointer cast), skip all loading */
    if (nameParam == 0) {
        WIN32_StreamDestroy(&localStream[2]);
        WNDPROC_StreamCleanup(&localStream[2]);
        return;
    }

    /* nameParam is passed as int32_t but is actually a string pointer */
    const char* cursorName = reinterpret_cast<const char*>(
        static_cast<uintptr_t>(static_cast<uint32_t>(nameParam)));

    /* Build file paths */
    char datPath[264];   /* local buffer for .dat path */

    /* Build: "%s\\<name>.dat" and "%s\\<name>.bmp" */
    CRT_sprintf_buf(datPath, "%s\\%s.dat", g_install_path, cursorName);
    CRT_sprintf_buf(this->bmpPath, "%s\\%s.bmp", g_install_path, cursorName);  /* +0x48 */

    /* --- Attempt 1: Load from AssetMgr (game archive) --- */
    if (g_asset_mgr != nullptr) {
        char shortPath[264];  /* local buffer for just "<name>.dat" */
        CRT_sprintf_buf(shortPath, "%s.dat", cursorName);

        pLoadedData = AssetMgr_LoadFile(&g_asset_mgr, shortPath, &dataSize);
        if (pLoadedData != nullptr) {
            /* Create memory stream from loaded data */
            void* streamAlloc = operator_new(0x5C);
            if (streamAlloc != nullptr) {
                int* streamResult = WNDPROC_StreamFromMemory(
                    streamAlloc, reinterpret_cast<const char*>(pLoadedData),
                    dataSize, 1);

                if (streamResult != nullptr) {
                    /* Call Render() (virtual dispatch to slot[3], this class or base) */
                    this->loaded = this->Render(streamResult);

                    if (this->loaded != 0) {
                        /* Call base ChildWindow::Render() directly (0x424E00) */
                        uint8_t baseRenderResult = ChildWindow::Render(streamResult);
                        this->loaded = (baseRenderResult != 0) ? 1 : 0;
                    }

                    /* Destroy the memory stream via its vtable[0] */
                    destroy_memory_stream(streamResult);
                }
            }

            /* Free the asset manager data */
            CRT_free(pLoadedData);
        }
    }

    /* --- Attempt 2: Fall back to direct file open --- */
    if (this->loaded == 0) {
        WIN32_StreamOpenPath(
            &localStream[0], datPath, 0x20,
            *reinterpret_cast<const int*>(static_cast<uintptr_t>(0x479190)));

        /* Check if file is open by validating stream data pointer */
        const int* stream_vtable;
        std::memcpy(&stream_vtable, &localStream[0], sizeof(stream_vtable));
        int vt4 = stream_vtable[4];
        const auto* stream_bytes = reinterpret_cast<const uint8_t*>(&localStream[0]);
        int offset_xx = *reinterpret_cast<const int*>(stream_bytes + vt4 + 0x4C);
        if (offset_xx != -1) {   /* valid file handle */
            /* Call Render() to process data from the file stream */
            this->loaded = this->Render(&localStream[0]);

            if (this->loaded != 0) {
                uint8_t baseRenderResult = ChildWindow::Render(&localStream[0]);
                this->loaded = (baseRenderResult != 0) ? 1 : 0;
            }

            WIN32_StreamDestroyImmediate(&localStream[0]);
        }
    }

    /* Clean up local stream */
    WIN32_StreamDestroy(&localStream[2]);
    WNDPROC_StreamCleanup(&localStream[2]);
}

/* ================================================================== */
/* 0x40E8B0 — NOT a real CursorEditWindow method; see the removal note */
/* in ui/CursorEditWindow.h for the full evidence trail. Its only 9    */
/* callers are SEH `Unwind@...` funclets belonging to unrelated        */
/* functions (confirmed via get_xrefs_to + disassembly of each         */
/* funclet, which compute their argument as a raw `EBP + <offset>`     */
/* stack address, never a `CursorEditWindow*`), and it has zero        */
/* normal-control-flow callers anywhere in the binary — a shared,      */
/* compiler/linker-folded exception-safety helper, not part of this    */
/* class's real API. Deliberately not reimplemented (CLAUDE.md's stub  */
/* exemption for compiler-generated EH helpers).                       */
/* ================================================================== */

/* ================================================================== */
/* CursorEditWindow_Ctor — Bridge Constructor (C linkage)              */
/* Address: 0x40E600 (exported C function for compatibility)           */
/*                                                                      */
/* Bridges from ResourceManager's C-style allocation (operator_new)    */
/* to C++ placement-new constructor.                                   */
/* ================================================================== */
extern "C"
void* CursorEditWindow_Ctor(void* memory, int32_t resId, int32_t strPtr)
{
    if (memory == nullptr) {
        return nullptr;
    }
    return new (memory) CursorEditWindow(static_cast<uint32_t>(resId), strPtr);
}
