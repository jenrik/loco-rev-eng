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
#include "../resources/Win32Stream.h"
#include "../resources/Win32StreamFile.h"
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

/* Stream / WNDPROC helpers.
 *
 * localStream below is a real WIN32_Stream object (resources/Win32Stream.h)
 * — see CursorEditWindow::init()'s doc comment for why this replaces the
 * former raw `int localStream[22]` buffer plus its manual
 * WIN32_StreamOpen/WIN32_StreamDestroy+WNDPROC_StreamCleanup construction/
 * destruction pair. streamAlloc/streamResult below still use the separate,
 * not-yet-reconstructed WNDPROC_StreamFromMemory heap-stream variant. */
extern int*   __thiscall WNDPROC_StreamFromMemory(void* stream, const char* data,
                                                   int size, int mode);    /* 0x464490 */
extern size_t WIN32_Stream_Size();  /* resources/Win32Stream.cpp — real sizeof(WIN32_Stream) */

/* Cursor data parsing helpers.
 *
 * Real signatures re-derived from disassembly of the call site at 0x40E8D0
 * (CursorEditWindow::Render) after the previous declaration below proved
 * disassembly-impossible: 0x4649F0 is __thiscall and ends in `RET 0x4`
 * (this + exactly ONE stack dword), not four output pointers. Matches the
 * canonical declarations already used for the same real symbols in
 * game/TrainStation.cpp, input/BuildingDescriptorEditor.cpp, and
 * ui/UI_ChildWindow.cpp:
 *   - WNDPROC_CriticalSectionLock(int*, char*) — free-function adapter for
 *     WNDPROC_Stream::ExtractToken (0x4649F0, operator>>(char*)); C++
 *     mangled linkage, matches _Z27WNDPROC_CriticalSectionLockPiPc.
 *   - WNDPROC_StreamPrintf/StreamWrite(void*, void*) — despite the
 *     misleading Ghidra-inherited names, disassembly of 0x464750/0x4646C0
 *     from this exact call site proves both are stream *extractors*
 *     (operator>>(int16_t*) and operator>>(int32_t*) respectively), not
 *     output/write functions: each parses a formatted number via a CRT
 *     numeric-parse call and stores it into its single output pointer,
 *     with ERANGE-based overflow handling. Not renamed here (that's a
 *     Ghidra-side follow-up), only correctly typed. */
extern void WNDPROC_CriticalSectionLock(int* stream, char* buf);         /* 0x4649F0 */
extern void*  __thiscall WNDPROC_StreamPrintf(void* stream, void* outVal); /* 0x464750 — operator>>(int16_t*) */
extern void*  __thiscall WNDPROC_StreamWrite(void* stream, void* outVal);  /* 0x4646C0 — operator>>(int32_t*) */
extern uint8_t __fastcall CGWND_ValidatePaletteData(int classPtr);        /* 0x40E950 */

/* Asset manager */
extern int*   __thiscall AssetMgr_LoadFile(void* mgr, const char* path,
                                            int* outSize);                 /* 0x45CD00 */

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
/* Reads one text line's worth of cursor metrics from a .dat stream,   */
/* via the same WNDPROC_Stream extraction chain used by                */
/* game/TrainStation.cpp, input/BuildingDescriptorEditor.cpp, and       */
/* ui/UI_ChildWindow.cpp for their own directive-line parsing:          */
/*   1. WNDPROC_CriticalSectionLock/ExtractToken — skips a leading      */
/*      token into a scratch line buffer (discarded; the original       */
/*      reserves exactly 264 bytes of stack for it here, matching every */
/*      sibling call site's `char lineBuf[264]`).                       */
/*   2. WNDPROC_StreamPrintf/operator>>(int16_t*) x2 — extracts          */
/*      field_7A8 (X) then field_7AA (Y).                                */
/*   3. WNDPROC_StreamWrite/operator>>(int32_t*) — extracts a full int   */
/*      that must equal -9 for the line to be considered valid.          */
/* game/TrainStation.cpp independently recovered a literal "-9" string   */
/* (0x47E3CC, s_terminator) used as a section-terminator sentinel        */
/* tested against parsed lines in this same class of .dat text format —  */
/* corroborating that the -9 check here is that same end-of-record       */
/* marker, not an ad hoc "error code".                                   */
/*                                                                        */
/* Re-derived by direct disassembly after the previous version's 5-      */
/* argument WNDPROC_CriticalSectionLock call was disassembly-disproven    */
/* (see the extern declarations above) and had misrouted the parsed      */
/* fields (field_7AA/field_7A8/tempBuf) relative to the real call order.  */
/*                                                                        */
/* BUG (preserved, not fixed): the original never sets the stream's       */
/* `width` field before step 1's ExtractToken call, and ExtractToken      */
/* zeroes it on every use — so width is whatever a prior use left it as.  */
/* If it's 0 (WNDPROC_StreamGetSize's constructor path zeroes it), the    */
/* character limit wraps to unbounded and a leading token longer than     */
/* 263 characters overflows this 264-byte stack buffer, in the original   */
/* binary as much as here. Not hardened per CLAUDE.md's stub policy —     */
/* faithful to the assembly.                                              */
/*                                                                        */
/* Called by: CursorEditWindow::init() [virtual dispatch]              */
/* ================================================================== */
uint8_t CursorEditWindow::Render(void* stream)
{
    /* Extracted 4th field; must equal -9 (the section-terminator
       sentinel, see above) for the line to be considered valid. */
    int lineTerminatorValue = 0;

    /* Scratch line buffer for the discarded leading token (step 1);
       264 bytes matches every sibling WNDPROC_CriticalSectionLock call
       site's `char lineBuf[264]` and this function's own real stack
       frame size (0x10C bytes total, minus 4 for lineTerminatorValue). */
    char lineBuf[264];

    /* Single shared exit in the original (both the early-out below and
       the -9 check reconverge on the same tail block that always calls
       CGWND_ValidatePaletteData and returns this flag) — no separate
       early `return 0;`s. Starts false; the original never null-checks
       `stream` or its vtable pointer here (dereferences both
       unconditionally), matching the fact that every real caller
       (CursorEditWindow::init) already passes a validated pointer. */
    uint8_t resultFlag = 0;

    /* Check stream validity via vtable. Stream object layout:
       [0] = vtable pointer
       vtable[1] = offset to stream control data
       At [vtable[1] + stream + 0x8] is a flag byte where bit 0x4 indicates
       the stream has an error condition. */
    int* streamVtable = *(int**)stream;
    int vtableOffset = streamVtable[1];  /* +0x4 in vtable */
    uint8_t* flagPtr = (uint8_t*)((uintptr_t)vtableOffset + (uintptr_t)stream + 0x8);
    uint8_t streamFlags = *flagPtr;     /* +0x8 relative to vtableOffset */

    /* If stream error flag (bit 0x4) is already set, skip straight to the
       shared tail below with resultFlag still false. */
    if ((streamFlags & 0x4) == 0) {
        /* Set result to success; may be cleared below if the -9 check fails. */
        resultFlag = 1;

        /* Step 1: skip a leading token (discarded). */
        WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);

        /* Steps 2-4: chain three extractions. Each real function always
           returns `this` (the same stream object) unchanged — verified
           against disassembly — so threading the return value through is
           behaviorally a no-op, kept only to match the original's chained-
           call structure and this codebase's established idiom for it
           (see e.g. input/BuildingDescriptorEditor.cpp's identical pattern). */
        void* s1 = WNDPROC_StreamPrintf(stream, &this->field_7A8);  /* X */
        void* s2 = WNDPROC_StreamPrintf(s1, &this->field_7AA);      /* Y */
        WNDPROC_StreamWrite(s2, &lineTerminatorValue);

        /* Validate the 4th field against the -9 section-terminator sentinel. */
        if (lineTerminatorValue != (int)0xfffffff7) {
            resultFlag = 0;
        }
    }

    /* Shared tail: reached on every path (error-flag early-out included).
       Call palette validation function on this object. This validates or
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
    /* Real WIN32_Stream object (resources/Win32Stream.h) — replaces the
     * original's WIN32_StreamOpen(&buf,1) construction and paired
     * WIN32_StreamDestroy(&buf)+WNDPROC_StreamCleanup(&buf) destruction;
     * see StreamObject::~StreamObject()'s doc comment for the full
     * evidence trail. Also fixes two pre-existing bugs in this file's
     * former raw `int localStream[22]` buffer: (1) 88 bytes is smaller
     * than sizeof(WIN32_Stream) on this host (0x80 bytes, wider pointer
     * fields than the original x86's 0x5C — see Win32Stream.h), and
     * (2) the cleanup calls below passed `&localStream[2]` (this+0x8),
     * not this+0xC — the wrong-offset bug documented in Win32Stream.h. */
    WIN32_Stream localStream;

    int* pLoadedData = nullptr;  /* AssetMgr loaded data pointer */
    int  dataSize = 0;

    /* Clear cursor-specific state fields */
    this->field_7A8 = 0;           /* +0x7A8 (short) */
    this->field_7AA = 0;           /* +0x7AA (short) */
    this->loaded = 0;              /* +0x162 (inherited, byte) */

    /* If nameParam is 0 (null pointer cast), skip all loading.
     * localStream's destructor runs automatically here (real C++ RAII). */
    if (nameParam == 0) {
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
            /* Create memory stream from loaded data. 0x5C was the original
             * x86 sizeof(WIN32_Stream); use the real host size (see
             * resources/Win32Stream.h). */
            void* streamAlloc = operator_new(WIN32_Stream_Size());
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
        localStream.OpenPath(
            datPath, 0x20,
            *reinterpret_cast<const int*>(static_cast<uintptr_t>(0x479190)));

        /* Check if the file actually opened: rdbuf->fileHandle() != -1,
         * matching the original's `*(rdbuf+0x4C) != -1` (the WIN32_Stream
         * vbtable-relative reach-through to rdbuf's fileHandle field —
         * now a real, typed accessor via WIN32_StreamFile::fileHandle()).
         * The previous version of this check read `stream_vtable[4]`
         * (vtable slot 4) as the vbase byte offset — disassembly (see
         * CursorEditWindow::init's doc comment) proves the real vbase
         * offset lives in vtable slot [1], not [4]; that stale manual
         * computation is removed along with the raw offset entirely. */
        WIN32_StreamFile* file = static_cast<WIN32_StreamFile*>(localStream.rdbuf);
        if (file != nullptr && file->fileHandle() != -1) {   /* valid file handle */
            /* Call Render() to process data from the file stream */
            this->loaded = this->Render(&localStream);

            if (this->loaded != 0) {
                uint8_t baseRenderResult = ChildWindow::Render(&localStream);
                this->loaded = (baseRenderResult != 0) ? 1 : 0;
            }

            /* Matches the original's WIN32_StreamDestroyImmediate — NOT
             * the object's own destructor, which still runs once at
             * scope exit below. */
            localStream.CloseNow();
        }
    }

    /* localStream's destructor runs automatically here (real C++ RAII) —
     * replaces the original's WIN32_StreamDestroy+WNDPROC_StreamCleanup
     * pair. */
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
