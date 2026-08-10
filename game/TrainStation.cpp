/**
 * TrainStation.cpp — Train station city-view interaction object
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Conversion from C struct with free functions to C++ derived class.
 * Implements real constructors, destructors, and virtual methods
 * that delegate to the original function bodies.
 */

#include "TrainStation.h"
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

/* ================================================================== */
/* External references (original C functions and globals)             */
/* ================================================================== */

extern "C" {

/* Memory management */
void* __cdecl operator_new(size_t size);               /* 0x465CE0 */
void  __cdecl GLOBAL_free(void* ptr);                  /* 0x465CD0 */
void  __cdecl CRT_free(void* ptr);                     /* 0x466C70 */

/* Resource Manager */
void* __thiscall ResourceManager_GetStringById(void* mgr, uint32_t id);
int   __thiscall RESMGR_LoadSoundResource(void* res_handle);
void  __thiscall RESMGR_ReleaseSoundResource(void* res_handle);

/* Win32 stream helpers */
int   __fastcall WIN32_StreamOpen(int* stream, int mode);
int   __fastcall WIN32_StreamOpenPath(int* stream, char* path, int mode, int flags);
void  __fastcall WIN32_StreamDestroy(int* stream);
void  __fastcall WIN32_StreamDestroyImmediate(int* stream);
void* __thiscall WNDPROC_StreamFromMemory(void* stream, char* data, int size, int mode);
void  __fastcall WNDPROC_StreamCleanup(void* stream);

/* Asset manager */
void* __thiscall AssetMgr_LoadFile(void* asset_mgr, void* path, int* out_size);

/* Format string construction (sprintf wrapper) */
void __cdecl sprintf_wrapper(char* buffer, const char* format, ...);  /* 0x466D60 */

/* Used only by TrainStation::Render (0x436750) below — matches
 * input/BuildingDescriptorEditor.cpp's existing declarations for these
 * same real symbols (extern "C" linkage there too). */
void* CRT_wcsstr(const void* haystack, const void* needle);
void* WNDPROC_StreamPrintf(void* stream, void* outBuf);
void* WNDPROC_StreamWrite(void* stream, void* outBuf);

}  // extern "C"

/* WNDPROC_CriticalSectionLock has C++ mangled linkage in this tree (matches
 * input/BuildingDescriptorEditor.cpp's declaration of the same real symbol,
 * _Z27WNDPROC_CriticalSectionLockPiPc) — declared outside extern "C". */
extern void WNDPROC_CriticalSectionLock(int* stream, char* buf);

/* ================================================================== */
/* Global variables referenced                                        */
/* ================================================================== */

extern void* g_resmgr;                              /* 0x4855E8 */
extern void* g_asset_mgr;                           /* 0x485600 */
extern char g_install_path[];                       /* 0x4A99C8 — install directory path */

/* Directive-keyword string literals used only by TrainStation::Render
 * (0x436750) below — read directly from the binary's .rdata this session
 * via Ghidra's read_bytes, not guessed. */
static const char s_walk_speed[]    = "walk_speed";     /* 0x47E990 */
static const char s_Employable[]    = "Employable";     /* 0x47E984 */
static const char s_sex[]           = "sex";             /* 0x47E980 */
static const char s_groundwidth[]   = "groundwidth";    /* 0x47E974 */
static const char s_SpawnLimit[]    = "SpawnLimit";     /* 0x47E968 */
static const char s_PickUpSoundId[] = "PickUpSoundId";  /* 0x47E958 */

/* Section-terminator sentinel string ("-9") tested via CRT_wcsstr against
 * each read line — matches DAT_0047e3cc's real bytes, read via read_bytes
 * this session (earlier files' "&DAT_..." usage of this exact address
 * speculated it was an empty marker string; it is not). */
static const char s_terminator[] = "-9";  /* 0x47E3CC */
extern void* g_resource_dir_path;                   /* 0x479190 — resource directory path pointer */

/* ================================================================== */
/* TrainStation::TrainStation — Constructor                           */
/* Address: 0x436400                                                   */
/* Size: 98 bytes (24 instructions)                                    */
/*                                                                     */
/* Initializes base class (ChildWindow), then calls Init for           */
/* TrainStation-specific setup (sprite/resource loading and config).  */
/* ================================================================== */
TrainStation::TrainStation(uint32_t resourceId, int32_t param2)
    : ChildWindow(resourceId, 0)  /* Base ctor with nameParam=0 */
{
    /* SEH is compiler-managed in real C++ */
    Init(resourceId, param2);
}

/* ================================================================== */
/* TrainStation::~TrainStation — Virtual destructor                   */
/* Address: 0x436460 (scalar-deleting-destructor entry point)         */
/* Body: 0x436480 (base-dtor, delegates to base class dtor)           */
/*                                                                     */
/* Compiler-managed: ~ChildWindow() handles inherited cleanup         */
/* (renderSurface, heapBuffer, etc.). Scalar-deleting-destructor       */
/* calling convention and vtable restore at 0x436480 are compiler     */
/* artifacts.                                                          */
/* ================================================================== */
TrainStation::~TrainStation() = default;

/* ================================================================== */
/* TrainStation::OnMouseMove — Handle mouse motion (vtable[1])        */
/* Address: 0x436960                                                   */
/* Size: 52 bytes (18 instructions)                                    */
/* Calling convention: __thiscall (ECX = this), RET 0x8               */
/*                                                                     */
/* Reads sound_string_id (+0x174). If non-zero, loads hover sound     */
/* via ResourceManager and RESMGR_LoadSoundResource. Then chains      */
/* to base class OnMouseMove for standard UI processing.              */
/*                                                                     */
/* NOTE: sound_string_id is initialized to 0 in Init, so the sound    */
/* load is a no-op unless set externally. The sex_code field          */
/* (+0x170, set to 'M'=0x4D by default) is never read by this or any  */
/* known function. */
/* ================================================================== */
void* TrainStation::OnMouseMove(int32_t x, int32_t y)
{
    /* Load and play the hover sound if a string resource is configured */
    if (this->sound_string_id != 0) {                   /* +0x174 */
        void* res_handle = ResourceManager_GetStringById(
            &g_resmgr, this->sound_string_id);
        if (res_handle != nullptr) {
            RESMGR_LoadSoundResource(res_handle);
        }
    }

    /* Standard mouse-move processing (renders/refreshes the UI surface) */
    return ChildWindow::OnMouseMove(x, y);
}

/* ================================================================== */
/* TrainStation::OnMouseLeave — Handle mouse leaving (vtable[2])      */
/* Address: 0x4369A0                                                   */
/* Size: 40 bytes (14 instructions)                                    */
/* Calling convention: __fastcall (ECX = this), RET                   */
/*                                                                     */
/* Reads sound_string_id (+0x174). If non-zero, releases hover sound  */
/* via ResourceManager and RESMGR_ReleaseSoundResource. Then chains   */
/* to base class OnMouseLeave for inherited cleanup.                  */
/*                                                                     */
/* NOTE: Same caveat as OnMouseMove — sound_string_id is initialized  */
/* to 0, making this a no-op unless set externally.                   */
/* ================================================================== */
void TrainStation::OnMouseLeave()
{
    /* Release the hover sound if a string resource is configured */
    if (this->sound_string_id != 0) {                   /* +0x174 */
        void* res_handle = ResourceManager_GetStringById(
            &g_resmgr, this->sound_string_id);
        if (res_handle != nullptr) {
            RESMGR_ReleaseSoundResource(res_handle);
        }
    }

    /* Standard mouse-leave processing */
    ChildWindow::OnMouseLeave();
}

namespace {

/* Stream-state bit tests used by TrainStation::Render below. Same raw
 * `*(byte*)(*(int*)(*stream+4) + 8 + (int)stream)` idiom already used by
 * input/BuildingDescriptorEditor.cpp's dat_stream_state_ok() helper — an
 * internal WNDPROC_Stream-hierarchy detail not yet reconstructed with a
 * typed accessor (see resources/WndProcStreamBuf.h). TrainStation::Render
 * tests two distinct bits of this same flags byte (bit 0x4 = stream error,
 * bit 0x1 = stream ended), unlike BuildingDescriptorEditor's single-bit
 * (0x7) check — both preserved verbatim from their own disassembly rather
 * than assumed to match.
 * TODO: replace with a typed accessor once that reconstruction extends to
 * this slot. */
uint8_t trainstation_stream_flags(void* stream)
{
    int32_t vtable = *reinterpret_cast<int32_t*>(stream);
    int32_t slot1  = *reinterpret_cast<int32_t*>(static_cast<intptr_t>(vtable) + 4);
    return *reinterpret_cast<uint8_t*>(
        static_cast<intptr_t>(slot1) + 8 + reinterpret_cast<intptr_t>(stream));
}

} // namespace

/* ================================================================== */
/* TrainStation::Render — Parse stream for TrainStation config        */
/* Address: 0x436750 (vtable slot [3])                                */
/* Size: 513 bytes (150 x86 instructions)                              */
/* Calling convention: __thiscall (ECX = this), RET 4                 */
/*                                                                     */
/* TrainStation's OWN override — a distinct function from              */
/* ChildWindow::Render (0x424E00), not a delegation to it. Ghidra's    */
/* auto-analysis never followed the vtable pointer here, so this       */
/* function had no defined bounds until decompiled directly for this   */
/* pass (create_function + decompile_function against 0x436750).      */
/*                                                                     */
/* Reads directive lines via WNDPROC_CriticalSectionLock until a       */
/* terminator line is hit (same CRT_wcsstr(line, sentinel) idiom as    */
/* the sibling ChildWindow-family parsers — a MATCH is a NULL return,  */
/* per the inverted-return convention documented in                    */
/* input/BuildingDescriptorEditor.cpp). Recognized directives:         */
/* "walk_speed" (two values -> field_168/field_169), "Employable"      */
/* (-> removable_flag), "sex" (first char, uppercased, 'M' else 'F'    */
/* -> sex_code), "groundwidth" (-> z_threshold), "SpawnLimit"          */
/* (-> spawn_limit), "PickUpSoundId" (raw 4-byte stream write ->       */
/* sound_string_id).                                                    */
/*                                                                     */
/* Returns 1 if the loop exited via a genuine terminator match, 0 if   */
/* it exited early because the stream's "ended" bit (0x1) was set.     */
/*                                                                     */
/* Called by: TrainStation::Init, during sprite/config loading.        */
/* ================================================================== */
uint8_t TrainStation::Render(void* stream)
{
    char lineBuf[264];

    /* Bit 0x4 = stream already in an error state; bail out immediately
     * (matches the original's early-out — result stays 0). */
    if ((trainstation_stream_flags(stream) & 0x4) != 0) {
        return 0;
    }

    uint8_t result = 1;

    WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);

    /* Loop while NOT at the terminator line (CRT_wcsstr's inverted
     * convention: 0/NULL == matched) and the stream's "ended" bit (0x1)
     * is not set. */
    while (CRT_wcsstr(lineBuf, s_terminator) != nullptr &&
           (trainstation_stream_flags(stream) & 0x1) == 0) {
        if (CRT_wcsstr(lineBuf, s_walk_speed) == nullptr) {
            uint16_t v0 = 0, v1 = 0;
            WNDPROC_StreamPrintf(stream, &v0);
            this->field_168 = static_cast<uint8_t>(v0);
            WNDPROC_StreamPrintf(stream, &v1);
            this->field_169 = static_cast<uint8_t>(v1);
        } else if (CRT_wcsstr(lineBuf, s_Employable) == nullptr) {
            uint16_t v = 0;
            WNDPROC_StreamPrintf(stream, &v);
            this->removable_flag = static_cast<uint8_t>(v);
        } else if (CRT_wcsstr(lineBuf, s_sex) == nullptr) {
            /* Re-reads a line (matches the original: a second
             * WNDPROC_CriticalSectionLock call here, distinct from the
             * loop's own line reads), then takes just the first
             * character. */
            WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);
            int upper = std::toupper(static_cast<unsigned char>(lineBuf[0]));
            this->sex_code = (upper == 'M') ? 0x4D : 0x46;
        } else if (CRT_wcsstr(lineBuf, s_groundwidth) == nullptr) {
            uint16_t v = 0;
            WNDPROC_StreamPrintf(stream, &v);
            this->z_threshold = static_cast<uint8_t>(v);
        } else if (CRT_wcsstr(lineBuf, s_SpawnLimit) == nullptr) {
            uint16_t v = 0;
            WNDPROC_StreamPrintf(stream, &v);
            this->spawn_limit = static_cast<uint8_t>(v);
        } else if (CRT_wcsstr(lineBuf, s_PickUpSoundId) == nullptr) {
            WNDPROC_StreamWrite(stream, &this->sound_string_id);
        }
        /* No matching keyword: fall through and read the next line
         * (matches the original — no "unknown keyword" terminator
         * branch here, unlike BuildingDescriptorEditor::Render). */

        WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);
    }

    /* If the final line read is NOT the terminator, the loop exited via
     * the stream-ended bit rather than a genuine terminator match —
     * mark as failure. */
    if (CRT_wcsstr(lineBuf, s_terminator) != nullptr) {
        result = 0;
    }

    return result;
}

/* ================================================================== */
/* TrainStation::Init — Initialize train station with sprites/config  */
/* Address: 0x436490                                                   */
/* Size: 717 bytes (207 instructions)                                  */
/* Calling convention: __thiscall (ECX = this, 1 stack arg), RET 0x8  */
/*                                                                     */
/* Comprehensive initialization:                                       */
/*   1. Open Win32 stream for resource loading.                        */
/*   2. Initialize all TrainStation-specific fields to defaults.       */
/*   3. If param2 == 0: clean up stream and return immediately.       */
/*   4. If param2 != 0:                                                */
/*      a. Construct .dat/.bmp filenames via sprintf.                 */
/*      b. Load .dat file via AssetMgr_LoadFile.                      */
/*      c. Render via virtual Render method.                          */
/*      d. Load .bmp file via WIN32_StreamOpenPath and render.        */
/*      e. Reset road connection offsets for sub-window entries.       */
/*      f. Adjust animation frame indices (frame ID = array index).   */
/*      g. Set default road offset (Y=8) if both offsets are zero.    */
/*   5. Clean up stream and return.                                   */
/*                                                                     */
/* NOTE: param1 (resourceId) is read in constructor but not used      */
/* within Init itself; the actual sprite resources are driven by      */
/* param2 (passed as format string argument to sprintf).              */
/* ================================================================== */
void TrainStation::Init(int32_t param1, int32_t param2)
{
    int     stream_handle[2];       /* local stream handle pair */
    char    dat_filename[264];      /* .dat filename buffer */
    char    bmp_filename[264];      /* .bmp filename buffer */
    int     file_size;
    void*   file_data;
    void*   mem_stream;
    int16_t sub_window_count;
    int16_t i;

    /* SEH prologue (compiler-managed) */

    /* Step 1: Open Win32 stream */
    WIN32_StreamOpen(stream_handle, 1);  /* mode 1 = read */

    /* Step 2: Initialize all TrainStation-specific fields to defaults */
    this->field_168       = 0;                          /* +0x168 */
    this->field_169       = 0;                          /* +0x169 */
    this->sound_string_id = 0;                          /* +0x174 */
    this->sex_code        = 0x4D;                       /* +0x170 */
    this->z_threshold     = 8;                          /* +0x16A */
    this->spawn_limit     = 0xFF;                       /* +0x16B */
    this->removable_flag  = 0;                          /* +0x16C */
    this->loaded          = 0;                          /* +0x162 (inherited from ChildWindow) */

    /* Step 3: Early return if param2 is 0 (no sprite loading) */
    if (param2 == 0) {
        WIN32_StreamDestroy(stream_handle);
        WNDPROC_StreamCleanup(stream_handle);
        return;
    }

    /* Step 4: Construct filenames and load sprite resources */
    /* ======================================================
     * Three sprintf calls build filenames for sprite/resource loading:
     *
     * 1. dat_filename (full path): "%s%s.dat" with g_install_path + resource name
     * 2. bmpPath (full path): "%s%s.bmp" with g_install_path + resource name
     * 3. short name: "%s.dat" with just resource name (for AssetMgr archive lookup)
     *
     * Addresses and format strings from Ghidra disassembly:
     *   - 0x43653B: sprintf(dat_filename, "%s%s.dat", ...) @ 0x47E368
     *   - 0x436552: sprintf(bmpPath, "%s%s.bmp", ...) @ 0x47E35C
     *   - 0x43657A: sprintf(short_name, "%s.dat", ...) @ 0x47E354
     *
     * Arguments determined by disassembly analysis:
     *   - g_install_path (0x4A99C8) = address of install directory path string
     *   - param2 (via EDI) = resource name pointer (e.g., "trainsta" or similar)
     * ==================================================== */

    /* First sprintf: build full .dat path (buffer at local [ESP+0x184]) */
    sprintf_wrapper(dat_filename, "%s%s.dat", g_install_path,
                    reinterpret_cast<char*>(static_cast<uintptr_t>(param2)));

    /* Second sprintf: build full .bmp path (buffer at this->bmpPath +0x48) */
    sprintf_wrapper(this->bmpPath, "%s%s.bmp", g_install_path,
                    reinterpret_cast<char*>(static_cast<uintptr_t>(param2)));

    /* Third sprintf: build short .dat name for archive lookup (buffer at local [ESP+0x78]) */
    char short_dat_name[264];  /* Local buffer for short filename */
    sprintf_wrapper(short_dat_name, "%s.dat",
                    reinterpret_cast<char*>(static_cast<uintptr_t>(param2)));

    /* Step 4a: Load .dat file via AssetMgr (using short archive-relative name) */
    if (g_asset_mgr != nullptr) {
        file_data = AssetMgr_LoadFile(&g_asset_mgr, short_dat_name, &file_size);
        if (file_data != nullptr) {
            /* Create sub-stream from the loaded data */
            mem_stream = operator_new(0x5C);  /* 92-byte stream object */
            if (mem_stream != nullptr) {
                void* render_stream = WNDPROC_StreamFromMemory(
                    mem_stream, static_cast<char*>(file_data), file_size, 1);

                if (render_stream != nullptr) {
                    /* Call virtual Render method */
                    uint8_t render_ok = this->Render(render_stream);
                    this->loaded = render_ok;                    /* 0x4365E0 */

                    /* Release the memory stream */
                    void** stream_vt = *reinterpret_cast<void***>(render_stream);
                    using StreamDestructor = void (__thiscall*)(int);
                    StreamDestructor destroy = reinterpret_cast<StreamDestructor>(stream_vt[0]);
                    destroy(1);  /* dtor with free */
                }
            }

            CRT_free(file_data);  /* 0x466C70, not GLOBAL_free */
        }
    }

    /* Step 4b: Fall back to re-opening the .dat file from disk using full path
       (address 0x436619: MOV EAX,[0x00479190] loads resource directory reference) */
    {
        WIN32_StreamOpenPath(stream_handle, dat_filename, 0x20,
                            static_cast<int>(reinterpret_cast<uintptr_t>(g_resource_dir_path)));

        /* Check if stream has data (stream field at +0x4C) */
        const uint8_t* stream_bytes = reinterpret_cast<const uint8_t*>(stream_handle);
        const int* stream_offset = reinterpret_cast<const int*>(stream_bytes + 0x4C);

        /* Guard: skip rendering if stream.offset == -1 */
        if (*stream_offset != -1) {
            /* Call virtual Render method */
            uint8_t render_ok = this->Render(stream_handle);
            this->loaded = render_ok;                    /* 0x4365E0 or 0x436653 */

            /* If render succeeded, call base Render directly for additional processing */
            if (render_ok != 0) {
                uint8_t base_render_ok = ChildWindow::Render(stream_handle);
                this->loaded = (base_render_ok != 0) ? 1 : 0;  /* 0x4365FD */
            }

            /* Clean up the stream */
            void** stream_vt = *reinterpret_cast<void***>(stream_handle);
            using StreamDestructor = void (__thiscall*)(int);
            StreamDestructor destroy = reinterpret_cast<StreamDestructor>(stream_vt[0]);
            destroy(1);  /* dtor with free */
        }
    }

    /* Step 4c: Reset road connection offsets for sub-window entries */
    /* Loop: for i = 0; (uint16_t)subWindowCount > 0 && i < 4; ++i */
    if (this->subWindowCount != 0) {                    /* +0x1A, unsigned compare */
        for (i = 0; i < 4; ++i) {
            if (this->heapBuffer == nullptr) {          /* +0x20 */
                break;
            }

            int* offset_x = reinterpret_cast<int*>(
                static_cast<uint8_t*>(this->heapBuffer) + 8 + i * 0x18);
            if (*offset_x > 0) {
                *offset_x = 0;
                this->loaded = 0;                       /* 0x4366A4 */
            }
        }
    }

    /* Step 4d: Adjust animation frame indices */
    /* Loop: for i = 0; (uint16_t)subWindowCount > 0 && i < 8; ++i */
    if (this->subWindowCount != 0) {                    /* +0x1A, unsigned compare */
        for (i = 0; i < 8; ++i) {
            if (this->heapBuffer == nullptr) {          /* +0x20, re-read each iteration */
                break;
            }

            int16_t* frame_id = reinterpret_cast<int16_t*>(
                static_cast<uint8_t*>(this->heapBuffer) + 0x0C + i * 0x18);
            int16_t current_id = *frame_id;

            if (current_id != i && current_id != -1) {
                *frame_id = static_cast<int16_t>(i);  /* set to match array index */
            }
        }
    }

    /* Step 4e: Set default road offset if none configured */
    if (this->roadOffsetX == 0 && this->roadOffsetY == 0) {   /* +0x32, +0x34 */
        this->roadOffsetX = 0;   /* no horizontal offset */
        this->roadOffsetY = 8;   /* default vertical offset (8 pixels) */
    }

    /* Step 5: Clean up stream */
    WIN32_StreamDestroy(stream_handle);
    WNDPROC_StreamCleanup(stream_handle);

    /* SEH epilogue (compiler-managed) */
}

/* ================================================================== */
/* TrainStation_Ctor — Placement-new constructor bridge               */
/* Address: 0x436400 (exported C function for compatibility)          */
/*                                                                     */
/* Replaces the original free-function TrainStation_Ctor. Calls the   */
/* C++ constructor via placement-new. Callers (e.g., ResourceManager) */
/* pass a pre-allocated memory block and expect it to be initialized  */
/* in-place, then return the same pointer.                            */
/* ================================================================== */
extern "C"
void* TrainStation_Ctor(void* memory, uint32_t resId, int32_t strPtr)
{
    if (memory == nullptr) {
        return nullptr;
    }

    /* Construct the TrainStation object at the given address */
    TrainStation* obj = new (memory) TrainStation(resId, strPtr);
    return obj;
}
