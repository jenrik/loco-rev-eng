/**
 * TrainStation.cpp — Train station city-view interaction object
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * TrainStation is the city-view train station interaction object,
 * extending UI_ChildWindow. Handles mouse events (hover sound via string
 * resource ID at +0x174), sprite/resource loading, and road connection
 * configuration.
 *
 * DIFFERENT from TrainStationWindow (vtable 0x478130), which is the
 * UI popup window showing animated train car sprites.
 */

// Status: TRANSCRIBED

#include "TrainStation.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External functions and globals                                      */
/* ================================================================== */

void*  __cdecl operator_new(size_t size);       /* 0x465CE0 */
void   __cdecl GLOBAL_free(void* ptr);          /* 0x465CD0 */

extern "C" {

/* Memory management */
void   __cdecl CRT_free(void* ptr);             /* 0x465CD0 alias */
void   __cdecl CRT_sprintf_buf(void* buf, const char* fmt); /* va_list wrapper */

/* Resource Manager */
void*  __thiscall ResourceManager_GetStringById(void* mgr, uint32_t id); /* 0x4472B0 */
int    __thiscall RESMGR_LoadSoundResource(void* res_handle);            /* 0x448D60 */
void   __thiscall RESMGR_ReleaseSoundResource(void* res_handle);         /* 0x448EE0 */

/* UI Window base functions */
void*  __thiscall UI_CreateChildWindow(void* window, int param1, int param2); /* 0x425nnn */
void   __thiscall UI_ChildWindow_Dtor(void* window);                          /* 0x424nnn */
int    __thiscall UI_ChildWindow_Render(void* window, void* stream);          /* 0x424nnn */
void   __thiscall UI_PaintWindow(void* window, int param1, int param2);       /* 0x425670 */
void   __fastcall UI_OnMouseLeave(void* window);                              /* 0x4257F0 */

/* Win32 stream helpers */
int    __fastcall WIN32_StreamOpen(int* stream, int mode);                    /* 0x461nnn */
int    __fastcall WIN32_StreamOpenPath(int* stream, char* path,
                                        int mode, int flags);                 /* 0x461nnn */
void   __fastcall WIN32_StreamDestroy(int* stream);                           /* 0x461nnn */
void   __fastcall WIN32_StreamDestroyImmediate(int* stream);                  /* 0x461nnn */
void*  __thiscall WNDPROC_StreamFromMemory(void* stream, char* data,
                                             int size, int mode);             /* 0x47nnnn */
void   __fastcall WNDPROC_StreamCleanup(void* stream);                        /* 0x47nnnn */

/* Asset manager */
void*  __thiscall AssetMgr_LoadFile(void* asset_mgr, void* path,
                                      int* out_size);                         /* 0x45CD00 */

} /* extern "C" */


/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern void*  g_resmgr;              /* 0x4855E8 */
extern void*  g_asset_mgr;           /* 0x485600 */
extern int    g_exception_state;     /* SEH global */


/* ================================================================== */
/* TrainStation_Ctor — Constructor                                     */
/* Address: 0x436400                                                    */
/* Size: 98 bytes (24 instructions)                                     */
/* Calling convention: __thiscall (ECX = this, 2 stack args), RET 0x8  */
/*                                                                     */
/* Standard MSVC SEH-guarded constructor:                               */
/*   1. Calls UI_CreateChildWindow(this, param1, 0) for base init.     */
/*   2. Overrides vtable to VTBL_TRAIN_STATION_VIEW (0x478118).        */
/*   3. Calls TrainStation_Init(this, param1, param2).                 */
/*                                                                     */
/* Called by: CGWND init path during station creation.                 */
/* ================================================================== */
void* __thiscall
TrainStation_Ctor(TrainStation* window, int32_t param1, int32_t param2)
{
    /* SEH prologue */

    /* Step 1: Create base child window */
    UI_CreateChildWindow(window, param1, 0);  /* 0x425nnn */

    /* Step 2: The binary installs the TrainStation dispatch table here;
     * natural C++ makes that compiler-managed. */

    /* Step 3: Initialize train-station-specific configuration */
    TrainStation_Init(window, param1, param2);  /* 0x436490 */

    /* SEH epilogue */
    return window;
}


/* ================================================================== */
/* TrainStation_Dtor — Scalar deleting destructor (vtable[0])          */
/* Address: 0x436460                                                    */
/* Size: 30 bytes (11 instructions)                                     */
/* Calling convention: __thiscall (ECX = this, 1 byte arg), RET 0x4   */
/*                                                                     */
/* Standard MSVC scalar-deleting destructor pattern:                    */
/*   1. Call TrainStation_BaseDtor for cleanup.                         */
/*   2. If (flags & 1): free memory via GLOBAL_free(this).             */
/*   3. Return this.                                                    */
/* ================================================================== */
void* __thiscall
TrainStation_Dtor(TrainStation* window, byte flags)
{
    /* Step 1: Base destructor for real cleanup */
    TrainStation_BaseDtor(window);  /* 0x436480 */

    /* Step 2: Conditionally free memory */
    if (flags & 1) {
        GLOBAL_free(window);
    }

    return window;
}


/* ================================================================== */
/* TrainStation_BaseDtor — Base destructor body                        */
/* Address: 0x436480                                                    */
/* Size: 11 bytes                                                       */
/* Calling convention: __fastcall (ECX = this), RET                    */
/*                                                                     */
/* Restores the vtable defensively, then tail-calls UI_ChildWindow_Dtor*/
/* for inherited cleanup.                                               */
/* ================================================================== */
void __fastcall
TrainStation_BaseDtor(TrainStation* window)
{
    /* The binary restores the TrainStation dispatch table defensively for
     * partial destruction; natural C++ makes that compiler-managed. */

    /* Delegate to UI_ChildWindow base destructor for inherited cleanup */
    UI_ChildWindow_Dtor(window);  /* 0x424nnn */
}


/* ================================================================== */
/* TrainStation_Init — Initialize train station with sprites and config*/
/* Address: 0x436490                                                    */
/* Size: 717 bytes (207 instructions)                                   */
/* Calling convention: __thiscall (ECX = this, 2 stack args), RET 0x8  */
/*                                                                     */
/* Comprehensive initialization:                                        */
/*   1. Open Win32 stream for resource loading.                         */
/*   2. Initialize all TrainStation-specific fields to defaults.        */
/*   3. If param2 == 0: clean up and return immediately.               */
/*   4. If param2 != 0:                                                */
/*      - Construct .dat/.bmp filenames and load resources.            */
/*      - Load and render the sprite data window.                      */
/*      - Adjust road connection offsets and animation frame indices.   */
/*      - Set default vertical road offset if both offsets are zero.   */
/*   5. Clean up stream and return.                                    */
/* ================================================================== */
void __thiscall
TrainStation_Init(TrainStation* window, int32_t param1, int32_t param2)
{
    int     stream_handle[2];       /* local stream handle pair */
    char    dat_filename[264];      /* .dat filename buffer */
    char    bmp_filename[264];      /* .bmp filename buffer (also mapped to +0x48) */
    int     file_size;
    void*   file_data;
    void*   mem_stream;
    void*   render_result;
    int16_t sub_window_count;
    int16_t i;

    /* SEH prologue */

    /* Step 1: Open Win32 stream */
    WIN32_StreamOpen(stream_handle, 1);  /* mode 1 = read */

    /* Step 2: Initialize all TrainStation-specific fields to defaults */
    window->field_168       = 0;       /* +0x168 */
    window->field_169       = 0;       /* +0x169 */
    window->sound_string_id = 0;       /* +0x174 — default 0 = no-op for OnMouseMove/OnMouseLeave */
    window->hover_sound_id  = 0x4D;    /* +0x170 — set to 'M'=77 but NEVER READ by known functions */
    window->z_threshold     = 8;       /* +0x16A */
    window->field_16B       = 0xFF;    /* +0x16B */
    window->removable_flag  = 0;       /* +0x16C */
    window->sprites_loaded  = 0;       /* +0x162 */

    /* Step 3: Early return if param2 is 0 (no sprite loading) */
    if (param2 == 0) {
        WIN32_StreamDestroy(stream_handle);
        WNDPROC_StreamCleanup(stream_handle);
        return;
    }

    /* Step 4: Construct filenames for sprite resources */
    /* The format string (at 0x47E368) is something like "%s_%s.dat" */
    /* and (at 0x47E35C) "%s_%s.bmp", where %s comes from param info */
    CRT_sprintf_buf(dat_filename, "%s_%s.dat");   /* 0x47E354 format */
    CRT_sprintf_buf(bmp_filename, "%s_%s.bmp");   /* 0x47E35C format */

    /* Copy bmp filename into window+0x48 buffer */
    /* The buffer at +0x48 holds the .bmp filename for later use */
    {
        char* dst = reinterpret_cast<char*>(window) + 0x48;
        const char* src = bmp_filename;
        while (*src) { *dst++ = *src++; }
        *dst = '\0';
    }

    /* Step 4a: Load .dat file via AssetMgr */
    if (g_asset_mgr != nullptr) {
        /* The .dat format string (at 0x47E354) is "%s.dat" */
        char* dat_path = dat_filename;  /* already formatted */
        file_data = AssetMgr_LoadFile(g_asset_mgr, dat_path, &file_size);
        if (file_data != nullptr) {
            /* Create sub-stream from the loaded data */
            mem_stream = operator_new(0x5C);  /* 92-byte stream object */
            if (mem_stream != nullptr) {
                render_result = WNDPROC_StreamFromMemory(
                    mem_stream, static_cast<char*>(file_data), file_size, 1);
            } else {
                render_result = nullptr;
            }

            if (render_result != nullptr) {
                /* Named implementation of UI_ChildWindow's virtual render slot. */
                uint8_t render_ok = static_cast<uint8_t>(
                    UI_ChildWindow_Render(window, render_result));

                window->sprites_loaded = render_ok;

                /* Release the memory stream */
                void** stream_vt = *reinterpret_cast<void***>(render_result);
                using StreamDestructor = void (__thiscall*)(int);
                StreamDestructor destroy = reinterpret_cast<StreamDestructor>(stream_vt[0]);
                destroy(1);  /* dtor with free */
            }

            CRT_free(file_data);
        }
    }

    /* Step 4b: Load .bmp file via stream path */
    {
        /* Format: "%s\0" concatenated with the .dat name as a path */
        char* path_buffer = dat_filename;  /* re-use local buffer */
        WIN32_StreamOpenPath(stream_handle, path_buffer, 0x20, 0x479190);

        /* Check if stream has data (offset +0x4C in stream object) */
        const uint8_t* stream_bytes = reinterpret_cast<const uint8_t*>(stream_handle);
        const int stream_data_offset = *reinterpret_cast<const int*>(
            stream_bytes + stream_handle[1]);
        int stream_data_available = *reinterpret_cast<const int*>(
            reinterpret_cast<const uint8_t*>(
                static_cast<uintptr_t>(static_cast<uint32_t>(stream_data_offset))) + 0x4C);
        if (stream_data_available != -1) {
            uint8_t render_ok = static_cast<uint8_t>(UI_ChildWindow_Render(
                window, stream_handle));

            window->sprites_loaded = render_ok;
            WIN32_StreamDestroyImmediate(stream_handle);
        }
    }

    /* Step 4c: Reset road connection offsets for sub-window entries */
    sub_window_count = *reinterpret_cast<const int16_t*>(
        reinterpret_cast<const uint8_t*>(window) + 0x1A);
    for (i = 0; i < sub_window_count; i++) {
        if (i >= 4) break;  /* only process first 4 entries */

        /* Each sub-window entry is at [window+0x20+8 + n*0x18]:
           +0x00..+0x07: header/type
           +0x08: offset_x (int32) — clear when > 0 */
        void* entry = *reinterpret_cast<void* const*>(
            reinterpret_cast<const uint8_t*>(window) + 0x20);
        if (entry == nullptr) break;

        int* offset_x = reinterpret_cast<int*>(
            static_cast<uint8_t*>(entry) + 8 + i * 0x18);
        if (*offset_x > 0) {
            *offset_x = 0;
            window->sprites_loaded = 0;  /* re-mark as needing refresh */
        }
    }

    /* Step 4d: Adjust animation frame indices */
    for (i = 0; i < sub_window_count; i++) {
        if (i >= 8) break;  /* only process first 8 entries */

        void* entry = *reinterpret_cast<void* const*>(
            reinterpret_cast<const uint8_t*>(window) + 0x20);
        if (entry == nullptr) break;

        /* Frame ID at [entry + 0x0C + n*0x18] */
        int16_t* frame_id = reinterpret_cast<int16_t*>(
            static_cast<uint8_t*>(entry) + 0x0C + i * 0x18);
        int16_t current_id = *frame_id;

        if (current_id != i && current_id != -1) {
            *frame_id = i;  /* set to match array index */
        }
    }

    /* Step 4e: Set default road offset if none configured */
    int16_t* road_offset_x = reinterpret_cast<int16_t*>(
        reinterpret_cast<uint8_t*>(window) + 0x32);
    int16_t* road_offset_y = reinterpret_cast<int16_t*>(
        reinterpret_cast<uint8_t*>(window) + 0x34);

    if (*road_offset_x == 0 && *road_offset_y == 0) {
        *road_offset_x = 0;   /* no horizontal offset */
        *road_offset_y = 8;   /* default vertical offset (8 pixels) */
    }

    /* Step 5: Clean up stream */
    WIN32_StreamDestroy(stream_handle);
    WNDPROC_StreamCleanup(stream_handle);
}


/* ================================================================== */
/* TrainStation_OnMouseMove — Mouse-move event handler (vtable[1])     */
/* Address: 0x436960                                                    */
/* Size: 18 instructions                                                */
/* Calling convention: __thiscall (ECX = this, 2 stack args), RET 0x8  */
/*                                                                     */
/* Reads string resource ID from sound_string_id (+0x174). If non-zero,*/
/* looks it up via ResourceManager_GetStringById and loads the sound   */
/* resource via RESMGR_LoadSoundResource to play the hover sound.      */
/* Then chains to UI_PaintWindow for standard mouse-move processing.   */
/*                                                                     */
/* NOTE: sound_string_id (+0x174) is initialized to 0 in Init, so the  */
/* sound load is a no-op unless set externally. The field at +0x170    */
/* (hover_sound_id = 0x4D = 'M') is SET by Init but NEVER READ by this */
/* or any other known function — possibly dead code or an externally    */
/* triggered feature.                                                   */
/*                                                                     */
/* BUG: The original binary reads from +0x174, NOT +0x170. The field   */
/* at +0x170 (hover_sound_id) is never read by OnMouseMove. If hover   */
/* sounds were intended to work, sound_string_id needed to be set to a */
/* valid string resource ID (0x5000-0x605F range) by external code.   */
/* ================================================================== */
void __thiscall
TrainStation_OnMouseMove(TrainStation* window, int32_t param1, int32_t param2)
{
    /* Load and play the hover sound if a string resource is configured */
    uint32_t string_id = window->sound_string_id;  /* +0x174 */
    if (string_id != 0) {
        void* res_handle = ResourceManager_GetStringById(&g_resmgr, string_id);  /* 0x4472B0 */
        if (res_handle != nullptr) {
            RESMGR_LoadSoundResource(res_handle);  /* 0x448D60 */
        }
    }

    /* Standard mouse-move processing (renders/refreshes the UI surface) */
    UI_PaintWindow(window, param1, param2);  /* 0x425670 */
}


/* ================================================================== */
/* TrainStation_OnMouseLeave — Mouse-leave event handler (vtable[2])   */
/* Address: 0x4369A0                                                    */
/* Size: 14 instructions                                                */
/* Calling convention: __fastcall (ECX = this), RET                    */
/*                                                                     */
/* Reads string resource ID from sound_string_id (+0x174). If non-zero,*/
/* looks it up via ResourceManager_GetStringById and releases the      */
/* sound resource via RESMGR_ReleaseSoundResource.                     */
/* Then chains to UI_OnMouseLeave for standard mouse-leave processing. */
/*                                                                     */
/* NOTE: Same caveat as OnMouseMove — sound_string_id initialized to 0 */
/* makes this a no-op unless set externally.                            */
/* ================================================================== */
void __fastcall
TrainStation_OnMouseLeave(TrainStation* window)
{
    /* Release the hover sound if a string resource is configured */
    uint32_t string_id = window->sound_string_id;  /* +0x174 */
    if (string_id != 0) {
        void* res_handle = ResourceManager_GetStringById(&g_resmgr, string_id);  /* 0x4472B0 */
        if (res_handle != nullptr) {
            RESMGR_ReleaseSoundResource(res_handle);  /* 0x448EE0 */
        }
    }

    /* Standard mouse-leave processing */
    UI_OnMouseLeave(window);  /* 0x4257F0 */
}
