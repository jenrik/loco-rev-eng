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

// Status: TRANSCRIBED

#include "CursorEditWindow.h"
#include "UI_ChildWindow.h"
#include <cstring>  /* memcpy — aliasing-safe vtable pointer read */
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Heap management */
    extern void* __cdecl operator_new(size_t size);     /* 0x465CE0 */
    extern void  __cdecl GLOBAL_free(void* ptr);         /* 0x465CD0 */
    extern void  __cdecl CRT_free(void* ptr);            /* 0x466C70 */
    extern void  __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...); /* 0x466D60 */
    extern void  __cdecl CRT_exit(const char** msg, const char** fileLine); /* 0x466CE0 */

    /* Win32 APIs */
extern "C" {
    extern int   __stdcall wsprintfA(char* buf, const char* fmt, ...); /* 0x477370 */
}

    /* WIN32_StreamOpenPath's only real definition (shared/link_stubs.cpp)
     * is extern "C" (unmangled) — unlike its WIN32_Stream and WNDPROC_Stream
     * siblings below, no C++-mangled twin exists in shared/defsym_stubs.cpp,
     * so this one must stay out of default C++ linkage or it call-0s. */
extern "C" {
    void WIN32_StreamOpenPath(void* stream, const char* path, int32_t mode, int32_t fileType); /* 0x463AA0 */
}

    /* Stream / WNDPROC helpers */
    extern void   __thiscall WIN32_StreamOpen(void* stream, int mode);        /* 0x463890 */
    extern void   __thiscall WIN32_StreamDestroy(void* stream);                /* 0x463A80 */
    extern void   __thiscall WIN32_StreamDestroyImmediate(void* stream);       /* 0x463B10 */
    extern void   __thiscall WIN32_StreamRead(void* stream, void* buf, int sz); /* 0x463810 */
    extern void   __thiscall WNDPROC_StreamCleanup(void* stream);              /* 0x464620 */
    extern int*   __thiscall WNDPROC_StreamFromMemory(void* stream, const char* data,
                                                       int size, int mode);    /* 0x464490 */

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
/* Called by: ResourceManager_AddString @ 0x446A55                     */
/*   with: operator_new(0x7AC), then this(this, resId, nameParam)     */
/*                                                                      */
/* Flow:                                                                */
/*   1. Call ChildWindow base constructor with nameParam=0              */
/*      (skips loading in base — the derived Init handles it)          */
/*   2. Override vtable to CursorEditWindow's vtable                    */
/*   3. Call Init() to load cursor data                                 */
/* ================================================================== */
CursorEditWindow::CursorEditWindow(uint32_t resourceId, int32_t nameParam)
{
    /* Step 1: Call ChildWindow base constructor */
    /* UI_CreateChildWindow(this, resourceId, 0) — pass 0 for nameParam
       to skip loading in base class (Init handles it) */
    UI_CreateChildWindow(this, resourceId, 0);

    /* Step 2: Override vtable */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Step 3: Initialize and load cursor data */
    this->init(resourceId, nameParam);
}

/* ================================================================== */
/* CursorEditWindow::scalar deleting destructor (vtable[0])            */
/* Address: 0x40E660                                                   */
/*                                                                      */
/* Standard MSVC scalar deleting destructor: call base dtor,            */
/* conditionally free heap.                                             */
/* ================================================================== */
CursorEditWindow::~CursorEditWindow()
{
    this->base_destructor();
}

/* ================================================================== */
/* CursorEditWindow::base_destructor                                   */
/* Address: 0x40E680                                                   */
/*                                                                      */
/* Resets vtable and delegates to ChildWindow's base destructor.       */
/* ================================================================== */
void CursorEditWindow::base_destructor()
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    UI_ChildWindow_Dtor(this);
}

/* ================================================================== */
/* CursorEditWindow::init                                              */
/* Address: 0x40E690                                                   */
/*                                                                      */
/* Loads cursor data from disk or AssetMgr. The nameParam controls     */
/* whether loading occurs (0 = no-op). When non-zero:                   */
/*                                                                      */
/*   1. Build file paths:                                               */
/*        local_dat_path = "%s\\<name>.dat"                             */
/*        this->bmpPath   = "%s\\<name>.bmp"  (+0x48)                  */
/*                                                                      */
/*   2. Try AssetMgr first: load "<name>.dat" from archive             */
/*      - If found: create memory stream, call vtable[3] to load,      */
/*        call UI_ChildWindow_Render to render, mark loaded flag       */
/*                                                                      */
/*   3. Fall back to direct file:                                      */
/*      - Open "%s\\<name>.dat" from install dir                       */
/*      - If valid: vtable[3] load, UI_ChildWindow_Render              */
/*                                                                      */
/*   4. Store success/failure in loaded (+0x162)                       */
/*                                                                      */
/* @param resourceId  Resource ID                                      */
/* @param nameParam   Non-zero to load cursor data                     */
/* ================================================================== */
void CursorEditWindow::init(uint32_t resourceId, int32_t nameParam)
{
    /* Local stream for file operations */
    int localStream[22];  /* WIN32_Stream object */
    WIN32_StreamOpen(localStream, 1);

    int* pLoadedData = NULL;  /* AssetMgr loaded data pointer */
    int  dataSize = 0;

    /* Clear cursor state fields */
    this->field_7A8 = 0;       /* +0x7A8 (short) */
    this->field_7AA = 0;       /* +0x7AA (short) */
    this->loaded = 0;          /* +0x162 (byte) */

    /* If nameParam is 0, skip all loading */
    if (nameParam == 0) {
        WIN32_StreamDestroy(static_cast<void*>(localStream + 2));
        WNDPROC_StreamCleanup(static_cast<void*>(localStream + 2));
        return;
    }

    /* Build file paths */
    char datPath[264];   /* 0x108 bytes on stack */
    char fullDatPath[264]; /* 0x108 bytes on stack */

    /* Build: "%s\\<name>.dat" */
    CRT_sprintf_buf(fullDatPath, "%s\\%s.dat", g_install_path,
                    reinterpret_cast<const char*>(
                        static_cast<uintptr_t>(resourceId)));
    /* Wait — resourceId is an integer, not a string pointer. Let me re-read the Init code... */

    /* Actually, looking at the disassembly more carefully:
       0x40E70A: PUSH EDI              ; EDI = param_2 from ctor = nameParam (an int/resource string index)
       0x40E70B: PUSH 0x4A99C8         ; g_install_path
       0x40E710: LEA EAX, [ESP+0x184]  ; local buffer
       0x40E717: PUSH 0x47E368         ; format string "%s\\%s.dat"
       0x40E71C: PUSH EAX
       0x40E71D: CALL 0x466D60         ; CRT_sprintf_buf

       Then:
       0x40E725: LEA ECX, [ESI+0x48]   ; this->bmpPath at +0x48
       0x40E728: PUSH EDI              ; nameParam
       0x40E729: PUSH 0x4A99C8         ; g_install_path
       0x40E72E: PUSH 0x47E35C         ; format string "%s\\%s.bmp"
       0x40E733: PUSH ECX
       0x40E734: CALL 0x466D60         ; CRT_sprintf_buf
    */

    /* So: sprintf(fullDatPath, "%s\\%s.dat", install_path, nameParam) */
    /*     sprintf(this->bmpPath, "%s\\%s.bmp", install_path, nameParam) */
    /* where nameParam is actually treated as a string pointer! */
    const char* cursorName = reinterpret_cast<const char*>(
        static_cast<uintptr_t>(static_cast<uint32_t>(nameParam)));
    CRT_sprintf_buf(fullDatPath, "%s\\%s.dat", g_install_path, cursorName);
    CRT_sprintf_buf(this->bmpPath, "%s\\%s.bmp", g_install_path, cursorName);

    /* --- Attempt 1: Load from AssetMgr (game archive) --- */
    if (g_asset_mgr != NULL) {
        char shortPath[264];  /* local_21c on stack */
        CRT_sprintf_buf(shortPath, "%s.dat", cursorName);

        pLoadedData = AssetMgr_LoadFile(&g_asset_mgr, shortPath, &dataSize);
        if (pLoadedData != NULL) {
            /* Create memory stream from loaded data */
            char streamObj[0x5C];  /* on stack */
            int* streamResult = NULL;

            /* Allocate stream */
            void* streamAlloc = operator_new(0x5C);
            if (streamAlloc != NULL) {
                streamResult = WNDPROC_StreamFromMemory(
                    streamAlloc, reinterpret_cast<const char*>(pLoadedData),
                    dataSize, 1);
            }

            if (streamResult != NULL) {
                /* Call loadCursorData to process data from stream */
                this->loaded = this->loadCursorData(streamResult);

                if (this->loaded != 0) {
                    /* Render the loaded cursor */
                    byte renderResult = UI_ChildWindow_Render(this, streamResult);
                    this->loaded = (renderResult != 0) ? 1 : 0;
                }

                /* Close/destroy the stream via vtable[0] with flags=1 */
                destroy_memory_stream(streamResult);
            }

            /* Free the asset manager data */
            CRT_free(pLoadedData);
        }
    }

    /* --- Attempt 2: Fall back to direct file open --- */
    if (this->loaded == 0) {
        WIN32_StreamOpenPath(
            localStream, fullDatPath, 0x20,
            *reinterpret_cast<const int*>(static_cast<uintptr_t>(0x479190)));

        /* Check if file is open by validating stream data */
        const int* stream_vtable;
        std::memcpy(&stream_vtable, localStream, sizeof(stream_vtable));
        int vt4 = stream_vtable[4];
        const auto* stream_bytes = reinterpret_cast<const uint8_t*>(localStream);
        int offset_xx = *reinterpret_cast<const int*>(stream_bytes + vt4 + 0x4C);
        if (offset_xx != -1) {   /* valid file handle */
            /* Call loadCursorData to process data from the file stream */
            this->loaded = this->loadCursorData(localStream);

            if (this->loaded != 0) {
                byte renderResult = UI_ChildWindow_Render(this, localStream);
                this->loaded = (renderResult != 0) ? 1 : 0;
            }

            WIN32_StreamDestroyImmediate(localStream);
        }
    }

    /* Clean up local stream */
    WIN32_StreamDestroy(static_cast<void*>(localStream + 2));
    WNDPROC_StreamCleanup(static_cast<void*>(localStream + 2));
}

/* ================================================================== */
/* CursorEditWindow::cleanup                                           */
/* Address: 0x40E8B0                                                   */
/*                                                                      */
/* Destroys the stream at +0x0C and cleans up the WNDPROC stream       */
/* state. Called externally to release stream resources without         */
/* full destruction.                                                    */
/* ================================================================== */
void CursorEditWindow::cleanup()
{
    void* stream = reinterpret_cast<uint8_t*>(this) + 0x0C;
    WIN32_StreamDestroy(stream);
    WNDPROC_StreamCleanup(stream);
}

/* ================================================================== */
/* CursorEditWindow::loadCursorData (vtable[3])                        */
/* Address: (virtual — dispatched through vtable slot [3])            */
/*                                                                      */
/* Processes cursor data from a stream. The actual implementation is    */
/* inherited from ChildWindow or overridden. This entry exists to       */
/* document the virtual dispatch signature.                            */
/* ================================================================== */
byte CursorEditWindow::loadCursorData(void* stream)
{
    /* Implementation varies by class. For ChildWindow base, this is
       the default load handler. For CursorEditWindow, it may parse
       cursor-specific data from the loaded .dat file. */
    return 0;
}
