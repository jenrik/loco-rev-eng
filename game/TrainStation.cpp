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
#include "../resources/AssetArchive.h"
#include "../resources/Win32Stream.h"
#include "../resources/Win32StreamFile.h"
#include "../resources/Win32StreamMem.h"
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

/* Asset manager — real access is g_asset_mgr.LoadFile(...)
 * (resources/AssetArchive.h, included above). */

/* Format string construction (sprintf wrapper) */
void __cdecl sprintf_wrapper(char* buffer, const char* format, ...);  /* 0x466D60 */

}  // extern "C"

/* CRT_wcsstr(uint8_t*, uint8_t*) -> TrainStation::Render (0x436750) call sites
 * below (~9 uses, lines ~274-315).
 *
 * Real target 0x471480 is a byte-wise, case-folding compare terminating on
 * NUL or first mismatch, returning 0 for "equal" and nonzero otherwise — the
 * real CRT `_stricmp`/`strcasecmp`, despite the misleading "wcsstr" name
 * (confirmed via disassembly of TrainStation::Render itself: every call site
 * is `CALL 0x471480; TEST EAX,EAX; JNZ <skip>` — i.e. falls through, taking
 * the "matched" branch, only when EAX == 0).
 *
 * This declaration was PREVIOUSLY inside this file's extern "C" block above,
 * as `void* CRT_wcsstr(const void*, const void*)`. C linkage does not encode
 * parameter types into the link symbol, so it collapsed onto the bare
 * `CRT_wcsstr` symbol — which shared/defsym_stubs.cpp binds to an unrelated,
 * zero-argument, void-returning inert no-op stub. That silently dropped both
 * real arguments and read stack/register garbage as a "return value" at
 * every call site below: this entire directive-line parser (walk_speed,
 * Employable, sex, groundwidth, SpawnLimit, PickUpSoundId, and the loop
 * terminator itself) was undefined-behavior-driven prior to this fix.
 *
 * Retyped to plain C++ (non-extern "C") linkage with the real (uint8_t*,
 * uint8_t*) -> int32_t signature, matching the canonical, already-correct
 * implementation in shared/stubs_link001_batch1_crt_win32.cpp (which defines
 * this exact overload against real strcasecmp semantics) and the same
 * uint8_t*-byte-string idiom resources/AssetArchive.cpp already uses for
 * this identical real function. This file's OWN comparison direction
 * (`== 0` / `!= 0` <-> "matched") was already correct per that disassembly;
 * only the linkage/signature was broken. */
extern int32_t CRT_wcsstr(uint8_t* str, uint8_t* sub);

/* ABI_BOUNDARY: CRT_wcsstr/_stricmp (0x471480) is a byte-oriented CRT string
 * compare — its real parameters are unsigned-char pointers on both sides,
 * not modeled game-object storage. This helper narrows the char* line
 * buffer and const char* keyword literals to that byte-string ABI shape
 * (matching the identical (uint8_t*,uint8_t*) idiom already used for this
 * same real function in resources/AssetArchive.cpp) rather than exposing
 * raw casts at every call site below. */
static inline int32_t TrainStation_LineMatches(char* line, const char* keyword)
{
    return CRT_wcsstr(reinterpret_cast<uint8_t*>(line),
                       const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(keyword)));
}

/* WNDPROC_CriticalSectionLock/StreamPrintf/StreamWrite have C++ mangled
 * linkage in this tree (matches input/BuildingDescriptorEditor.cpp's and
 * ui/UI_ChildWindow.cpp's declarations of these same real symbols) —
 * declared outside extern "C". WNDPROC_StreamPrintf/StreamWrite were
 * previously (wrongly) inside the extern "C" block above, which bound
 * them to an unmangled, undefined symbol at link time (turned into a
 * null-pointer call at every one of this file's call sites, via
 * -Wl,--unresolved-symbols=ignore-all) — the exact same defect class as
 * the ui/HelpWnd.cpp landmine fixed in the 2026-08-10 "WNDPROC_Stream
 * facade recovery" session (see PROGRESS.md). Fixed 2026-08-11. */
extern void WNDPROC_CriticalSectionLock(int* stream, char* buf);
void* WNDPROC_StreamPrintf(void* stream, void* outBuf);
void* WNDPROC_StreamWrite(void* stream, void* outBuf);
extern size_t WIN32_Stream_Size();  /* resources/Win32Stream.cpp — real sizeof(WIN32_Stream) */

/* ResourceManager_GetStringById: same extern-"C"-linkage landmine as
 * WNDPROC_CriticalSectionLock above — this file's `uint32_t id` declaration
 * inside the extern "C" block bound to a bare C-linkage no-op stub
 * (shared/link_stubs.cpp) instead of the real, already-implemented facade
 * (shared/stubs_link001_batch3_resource_audio.cpp, forwarding to
 * ResourceManager::GetStringById, 0x4472B0) — confirmed via nm on the
 * linked binary. Retyped to match that facade's real signature exactly
 * (ui/TrainStationWindow.cpp already calls it correctly this way) and moved
 * to ordinary C++-mangled linkage.
 *
 * RESMGR_LoadSoundResource/RESMGR_ReleaseSoundResource's own address
 * annotations here (previously 0x44B8E0/0x44BB90) were also wrong — both
 * addresses fall inside the unrelated RESDATA_ScriptedObject_ClassifyTileType
 * (0x44B4F0), confirmed via decompile. Neither has a real implementation
 * anywhere in the tree yet (every declared overload of both names binds to
 * a no-op stub — see PROGRESS.md's "RESMGR_LoadSoundResource/
 * ReleaseSoundResource never implemented" item); retyped their handle
 * parameter from `void*` to `int` to match the corrected
 * ResourceManager_GetStringById return type and the project's established
 * int32_t resource-handle convention (ResourceManager::GetById/
 * GetStringById), rather than introduce a fresh pointer/int mismatch. This
 * changes no behavior today — sound_string_id (+0x174) is always 0 per this
 * file's own OnMouseMove/OnMouseLeave doc comments, so neither call site is
 * live. */
int  ResourceManager_GetStringById(void* mgr, int id);   /* 0x4472B0 */
int  RESMGR_LoadSoundResource(int res_handle);
void RESMGR_ReleaseSoundResource(int res_handle);

/* ================================================================== */
/* Global variables referenced                                        */
/* ================================================================== */

class ResourceManager;
extern ResourceManager g_resmgr;    /* 0x4855E8 — object, not a pointer (was void*,
                                      * a widespread cross-TU landmine — see
                                      * PROGRESS.md's g_resmgr sweep) */
/* g_asset_mgr — real AssetArchive value object (resources/AssetArchive.h,
 * included above); same "object, not a pointer" landmine pattern as
 * g_resmgr above. */
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
TrainStation::TrainStation(uint32_t resourceId, const char* name)
    : ChildWindow(resourceId, nullptr)  /* Base ctor defers loading */
{
    /* SEH is compiler-managed in real C++ */
    Init(resourceId, name);
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
        int res_handle = ResourceManager_GetStringById(
            &g_resmgr, this->sound_string_id);
        if (res_handle != 0) {
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
        int res_handle = ResourceManager_GetStringById(
            &g_resmgr, this->sound_string_id);
        if (res_handle != 0) {
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
/* terminator line is hit (same CRT_wcsstr/_stricmp(line, sentinel)    */
/* idiom as the sibling ChildWindow-family parsers — a MATCH is a 0    */
/* return, real _stricmp semantics, per TrainStation_LineMatches above */
/* and the disassembly at 0x436750). Recognized directives:            */
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

    /* Loop while NOT at the terminator line (real _stricmp semantics:
     * 0 == matched, per the disassembly at 0x436750: every call site is
     * `CALL 0x471480; TEST EAX,EAX; JNZ <skip>`) and the stream's "ended"
     * bit (0x1) is not set. */
    while (TrainStation_LineMatches(lineBuf, s_terminator) != 0 &&
           (trainstation_stream_flags(stream) & 0x1) == 0) {
        if (TrainStation_LineMatches(lineBuf, s_walk_speed) == 0) {
            uint16_t v0 = 0, v1 = 0;
            WNDPROC_StreamPrintf(stream, &v0);
            this->field_168 = static_cast<uint8_t>(v0);
            WNDPROC_StreamPrintf(stream, &v1);
            this->field_169 = static_cast<uint8_t>(v1);
        } else if (TrainStation_LineMatches(lineBuf, s_Employable) == 0) {
            uint16_t v = 0;
            WNDPROC_StreamPrintf(stream, &v);
            this->removable_flag = static_cast<uint8_t>(v);
        } else if (TrainStation_LineMatches(lineBuf, s_sex) == 0) {
            /* Re-reads a line (matches the original: a second
             * WNDPROC_CriticalSectionLock call here, distinct from the
             * loop's own line reads), then takes just the first
             * character. */
            WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);
            int upper = std::toupper(static_cast<unsigned char>(lineBuf[0]));
            this->sex_code = (upper == 'M') ? 0x4D : 0x46;
        } else if (TrainStation_LineMatches(lineBuf, s_groundwidth) == 0) {
            uint16_t v = 0;
            WNDPROC_StreamPrintf(stream, &v);
            this->z_threshold = static_cast<uint8_t>(v);
        } else if (TrainStation_LineMatches(lineBuf, s_SpawnLimit) == 0) {
            uint16_t v = 0;
            WNDPROC_StreamPrintf(stream, &v);
            this->spawn_limit = static_cast<uint8_t>(v);
        } else if (TrainStation_LineMatches(lineBuf, s_PickUpSoundId) == 0) {
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
    if (TrainStation_LineMatches(lineBuf, s_terminator) != 0) {
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
void TrainStation::Init(int32_t param1, const char* name)
{
    /* Real WIN32_Stream object (resources/Win32Stream.h) — replaces the
     * original's WIN32_StreamOpen(&buf,1) construction and paired
     * WIN32_StreamDestroy(&buf)+WNDPROC_StreamCleanup(&buf) destruction;
     * see StreamObject::~StreamObject()'s doc comment for the full
     * evidence trail. Also fixes a pre-existing stack buffer-overflow
     * bug: the previous `int stream_handle[2]` (8 bytes) was smaller
     * than sizeof(WIN32_Stream) even on the original x86 (0x5C bytes),
     * let alone this host's wider pointer fields — WIN32_StreamOpen was
     * placement-constructing a full WIN32_Stream into it regardless. */
    WIN32_Stream stream_handle;
    char    dat_filename[264];      /* .dat filename buffer */
    char    bmp_filename[264];      /* .bmp filename buffer */
    int     file_size;
    uint8_t* file_data;     /* AssetArchive::LoadFile's real return type */
    void*   mem_stream;
    int16_t sub_window_count;
    int16_t i;

    /* SEH prologue (compiler-managed) */

    /* Step 2: Initialize all TrainStation-specific fields to defaults */
    this->field_168       = 0;                          /* +0x168 */
    this->field_169       = 0;                          /* +0x169 */
    this->sound_string_id = 0;                          /* +0x174 */
    this->sex_code        = 0x4D;                       /* +0x170 */
    this->z_threshold     = 8;                          /* +0x16A */
    this->spawn_limit     = 0xFF;                       /* +0x16B */
    this->removable_flag  = 0;                          /* +0x16C */
    this->loaded          = 0;                          /* +0x162 (inherited from ChildWindow) */

    /* Step 3: Early return if name is null (no sprite loading).
     * stream_handle's destructor runs automatically here (real C++ RAII). */
    if (name == nullptr) {
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
     *   - name (via EDI) = resource name pointer (e.g., "trainsta" or similar),
     *     widened from the original's int32_t ABI slot to a real `const char*`
     * ==================================================== */

    /* First sprintf: build full .dat path (buffer at local [ESP+0x184]) */
    sprintf_wrapper(dat_filename, "%s%s.dat", g_install_path, name);

    /* Second sprintf: build full .bmp path (buffer at this->bmpPath +0x48) */
    sprintf_wrapper(this->bmpPath, "%s%s.bmp", g_install_path, name);

    /* Third sprintf: build short .dat name for archive lookup (buffer at local [ESP+0x78]) */
    char short_dat_name[264];  /* Local buffer for short filename */
    sprintf_wrapper(short_dat_name, "%s.dat", name);

    /* Step 4a: Load .dat file via AssetMgr (using short archive-relative name) */
    if (g_asset_mgr.archive_file != 0) {
        file_data = g_asset_mgr.LoadFile(
            reinterpret_cast<const uint8_t*>(short_dat_name), &file_size);
        if (file_data != nullptr) {
            /* Create sub-stream from the loaded data. 0x5C was the
             * original x86 sizeof(WIN32_MemoryStream) (see
             * resources/Win32StreamMem.h); use the real host size. */
            mem_stream = operator_new(WIN32_MemoryStream_Size());
            if (mem_stream != nullptr) {
                // ABI_BOUNDARY: WNDPROC_StreamFromMemory's `char* data` param is this
                // codebase's older byte-buffer convention; file_data is the same raw
                // bytes under AssetArchive::LoadFile's real `uint8_t*` return type.
                WNDPROC_Stream* render_stream = WNDPROC_StreamFromMemory(
                    mem_stream, reinterpret_cast<char*>(file_data), file_size, 1);

                if (render_stream != nullptr) {
                    /* Call virtual Render method */
                    uint8_t render_ok = this->Render(render_stream);
                    this->loaded = render_ok;                    /* 0x4365E0 */

                    /* Release the memory stream. Real C++ `delete` through
                     * WNDPROC_Stream* dispatches to WIN32_MemoryStream's
                     * scalar deleting destructor via StreamObject's virtual
                     * ~StreamObject() — replaces the former raw vtable[0]
                     * dispatch (0x464460, see resources/Win32StreamMem.h). */
                    delete render_stream;
                }
            }

            CRT_free(file_data);  /* 0x466C70, not GLOBAL_free */
        }
    }

    /* Step 4b: Fall back to re-opening the .dat file from disk using full path
       (address 0x436619: MOV EAX,[0x00479190] loads resource directory reference) */
    {
        stream_handle.OpenPath(dat_filename, 0x20,
                            static_cast<int>(reinterpret_cast<uintptr_t>(g_resource_dir_path)));

        /* Check if the file actually opened: rdbuf->fileHandle() != -1,
         * matching the original's `*(rdbuf+0x4C) != -1` (the WIN32_Stream
         * vbtable-relative reach-through to rdbuf's fileHandle field —
         * now a real, typed accessor via WIN32_StreamFile::fileHandle()). */
        WIN32_StreamFile* file = static_cast<WIN32_StreamFile*>(stream_handle.rdbuf);

        /* Guard: skip rendering if the file failed to open */
        if (file != nullptr && file->fileHandle() != -1) {
            /* Call virtual Render method */
            uint8_t render_ok = this->Render(&stream_handle);
            this->loaded = render_ok;                    /* 0x4365E0 or 0x436653 */

            /* If render succeeded, call base Render directly for additional processing */
            if (render_ok != 0) {
                uint8_t base_render_ok = ChildWindow::Render(&stream_handle);
                this->loaded = (base_render_ok != 0) ? 1 : 0;  /* 0x4365FD */
            }

            /* Immediately close the file handle (matches the original's
             * WIN32_StreamDestroyImmediate, i.e. WIN32_Stream::CloseNow() —
             * NOT the object's own destructor, which still runs once at
             * scope exit below). The previous reconstruction here called
             * a scalar-deleting-destructor-with-free-flag-1 through a
             * manually read vtable slot on the undersized raw buffer —
             * wrong on two counts: it didn't match the original's real
             * call (WIN32_StreamDestroyImmediate, not a delete), and
             * manual vtable dispatch on a stack object is exactly the
             * landmine class CLAUDE.md's evidence-only rule forbids. */
            stream_handle.CloseNow();
        }
    }

    /* Step 4c: Reset road connection offsets for sub-window entries */
    /* Loop: for i = 0; (uint16_t)frameSetCount > 0 && i < 4; ++i */
    if (this->frameSetCount != 0) {                    /* +0x1A, unsigned compare */
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
    /* Loop: for i = 0; (uint16_t)frameSetCount > 0 && i < 8; ++i */
    if (this->frameSetCount != 0) {                    /* +0x1A, unsigned compare */
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

    /* Step 4e: Set default road offset if none configured. TrainStation's
     * own reinterpretation of ChildWindow's base-level hotspotX/hotspotY
     * fields (its own Render() sets them from the generic .dat "hotspot"
     * directive) as a road-connection offset — a legitimate derived-class
     * reuse of the same storage, not a naming contradiction; see the
     * field comment on hotspotX/hotspotY in ui/UI_ChildWindow.h. */
    if (this->hotspotX == 0 && this->hotspotY == 0) {   /* +0x32, +0x34 */
        this->hotspotX = 0;   /* no horizontal offset */
        this->hotspotY = 8;   /* default vertical offset (8 pixels) */
    }

    /* Step 5: stream_handle's destructor runs automatically here (real
     * C++ RAII) — replaces the original's WIN32_StreamDestroy+
     * WNDPROC_StreamCleanup pair, see the local declaration's doc
     * comment above. */

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
void* TrainStation_Ctor(void* memory, uint32_t resId, const char* name)
{
    if (memory == nullptr) {
        return nullptr;
    }

    /* Construct the TrainStation object at the given address */
    TrainStation* obj = new (memory) TrainStation(resId, name);
    return obj;
}
