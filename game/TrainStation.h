/**
 * TrainStation.h — Train station city-view interaction object
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * TrainStation is the city/town-view train station interaction object.
 * It extends UI_ChildWindow (via UI_WindowBase) and handles mouse
 * events (hover sound via string resource at +0x174), sprite/resource
 * loading, and road connection configuration for the train station
 * building on the map.
 *
 * This is DIFFERENT from TrainStationWindow (vtable 0x478130), which is
 * the UI popup window showing animated train car sprites.
 * TrainStation (vtable 0x478118) is the view-level station interaction
 * object in the city scene.
 *
 * Class hierarchy: UI_WindowBase -> UI_ChildWindow -> TrainStation
 *
 * Size: ~0x178 bytes (0x170 highest known field offset + room for base)
 * Vtable: 0x478118 (VTBL_TRAIN_STATION_VIEW)
 *
 * Vtable layout (inherits from UI_WindowBase/UI_ChildWindow):
 *   [0]  +0x00: scalar deleting destructor  (TrainStation_Dtor, 0x436460)
 *   [1]  +0x04: OnMouseMove (TrainStation_OnMouseMove, 0x436960)
 *   [2]  +0x08: OnMouseLeave (TrainStation_OnMouseLeave, 0x4369A0)
 *   ... remaining slots inherited from UI_WindowBase ...
 *
 * Key fields (TrainStation-specific, offset from struct base):
 *   +0x48: sprite_filename_buf — .bmp filename buffer (sprintf'd from resource)
 *   +0x162: sprites_loaded — loaded/flags byte
 *   +0x168: field_168 — byte flag
 *   +0x169: field_169 — byte flag
 *   +0x16A: z_threshold — z-proximity byte threshold for overlap detection
 *   +0x16B: field_16B — byte (initialized to 0xFF)
 *   +0x16C: removable_flag — byte, 0=not removable, non-zero=removable
 *   +0x170: hover_sound_id — resource ID for hover sound (default 0x4D = 77).
 *            SET but never READ by any known function — possibly dead code
 *            or used by external dispatch path.
 *   +0x174: sound_string_id — string resource ID read by OnMouseMove/OnMouseLeave
 *            for hover sound load/release. Initialized to 0 (no-op). Only
 *            non-zero if set from an external path.
 */

#pragma once

#include "../shared/types.h"


// Status: TRANSCRIBED
#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* TrainStation — City-view train station interaction object           */
/* ================================================================== */

struct TrainStation;

/**
 * TrainStation_Ctor — Constructor.
 * Address: 0x436400
 * Size: 98 bytes
 * Calling convention: __thiscall (ECX = this, 2 stack args), RET 0x8
 *
 * Calls UI_CreateChildWindow(this, param1, 0) for base initialization,
 * overrides vtable to VTBL_TRAIN_STATION_VIEW (0x478118), then calls
 * TrainStation_Init(this, param1, param2) for train-specific setup.
 *
 * SEH-guarded to handle exceptions during creation.
 *
 * Called by: CGWND init path during train station creation
 *
 * @param window  ECX = pointer to TrainStation struct
 * @param param1  Resource/panel ID for base window creation
 * @param param2  Context/param for TrainStation_Init (non-zero to load sprites)
 * @return        The TrainStation pointer (this)
 */
void* __thiscall
TrainStation_Ctor(TrainStation* window, int32_t param1, int32_t param2);

/**
 * TrainStation_Dtor — Scalar deleting destructor (vtable[0]).
 * Address: 0x436460
 * Size: 30 bytes
 * Calling convention: __thiscall (ECX = this, 1 byte stack arg), RET 0x4
 *
 * Calls TrainStation_BaseDtor for cleanup, then optionally frees memory
 * via GLOBAL_free if (flags & 1).
 *
 * @param window  ECX = pointer to TrainStation struct
 * @param flags   If bit 0 set, also free the object's memory
 * @return        The object pointer (this)
 */
void* __thiscall
TrainStation_Dtor(TrainStation* window, byte flags);

/**
 * TrainStation_BaseDtor — Base destructor body (real cleanup).
 * Address: 0x436480
 * Size: 11 bytes
 * Calling convention: __fastcall (ECX = this), RET
 *
 * Restores vtable to VTBL_TRAIN_STATION_VIEW (0x478118), then tail-calls
 * UI_ChildWindow_Dtor for inherited cleanup.
 *
 * Called by: TrainStation_Dtor (0x436463)
 */
void __fastcall
TrainStation_BaseDtor(TrainStation* window);

/**
 * TrainStation_Init — Initialize train station with sprites and config.
 * Address: 0x436490
 * Size: 717 bytes (207 instructions)
 * Calling convention: __thiscall (ECX = this, 2 stack args), RET 0x8
 *
 * Comprehensive initialization that:
 *   1. Opens a Win32 stream for resource loading.
 *   2. Initializes all TrainStation-specific fields to defaults:
 *      - +0x168: 0, +0x169: 0, +0x174: 0
 *      - +0x170: 0x4D ('M' = default sound resource, SET BUT NEVER READ)
 *      - +0x16A: 8 (z-threshold), +0x16B: 0xFF
 *      - +0x16C: 0, +0x162: 0
 *   3. If param2 == 0: cleans up stream and returns immediately.
 *   4. If param2 != 0:
 *      a. Constructs filenames via sprintf: "%s_%s.dat" and "%s_%s.bmp"
 *         into +0x48 (bmp buffer) and a local buffer (dat).
 *      b. Loads .dat file via AssetMgr_LoadFile from game assets.
 *      c. Converts data stream via WNDPROC_StreamFromMemory and calls
 *         UI_ChildWindow_Render to initialize the window from stream data.
 *      d. Loads .bmp file via WIN32_StreamOpenPath and renders it.
 *      e. Resets road connection offsets: for each rendered sub-window
 *         entry, clears offset fields at [20+8+n*0x18] when the offset > 0.
 *      f. Adjusts animation frame indices: for each frame entry at
 *         [20+0xC+n*0x18], sets the frame ID to match the array index
 *         when it doesn't already match (and isn't -1).
 *      g. Sets default road offset: if both +0x32 and +0x34 are 0,
 *         sets +0x34 = 8 (default vertical road offset).
 *   5. Cleans up stream and returns.
 *
 * Called by: TrainStation_Ctor (0x43643B)
 *
 * @param window  ECX = pointer to TrainStation struct
 * @param param1  Resource/panel ID (passed from Ctor)
 * @param param2  Non-zero to load sprites and fully initialize
 */
void __thiscall
TrainStation_Init(TrainStation* window, int32_t param1, int32_t param2);

/**
 * TrainStation_OnMouseMove — Mouse-move event handler (vtable[1]).
 * Address: 0x436960
 * Size: 18 instructions
 * Calling convention: __thiscall (ECX = this, 2 stack args), RET 0x8
 *
 * Reads string resource ID from field_174 (+0x174). If non-zero, looks it
 * up via ResourceManager_GetStringById and loads the sound resource via
 * RESMGR_LoadSoundResource. Then chains to UI_PaintWindow for default
 * mouse-move processing.
 *
 * NOTE: field_174 (+0x174) is initialized to 0 in TrainStation_Init,
 * making the sound load a no-op unless field_174 is set externally.
 * The hover_sound_id at +0x170 (set to 0x4D = 'M') is NOT read here.
 *
 * Called by: vtable dispatch through slot [1] (offset 0x04)
 *
 * @param window  ECX = pointer to TrainStation struct
 * @param param1  Standard UI event parameter (x/wParam)
 * @param param2  Standard UI event parameter (y/lParam)
 */
void __thiscall
TrainStation_OnMouseMove(TrainStation* window, int32_t param1, int32_t param2);

/**
 * TrainStation_OnMouseLeave — Mouse-leave event handler (vtable[2]).
 * Address: 0x4369A0
 * Size: 14 instructions
 * Calling convention: __fastcall (ECX = this), RET
 *
 * Reads string resource ID from field_174 (+0x174). If non-zero, looks it
 * up via ResourceManager_GetStringById and releases the sound resource via
 * RESMGR_ReleaseSoundResource. Then chains to UI_OnMouseLeave for default
 * mouse-leave processing.
 *
 * NOTE: Same caveat as OnMouseMove — field_174 is initialized to 0,
 * making this a no-op unless set externally.
 *
 * Called by: vtable dispatch through slot [2] (offset 0x08)
 *
 * @param window  ECX = pointer to TrainStation struct
 */
void __fastcall
TrainStation_OnMouseLeave(TrainStation* window);

/**
 * TrainStation struct layout (C-compatible).
 * Inherits from UI_ChildWindow via UI_WindowBase.
 * Only TrainStation-specific fields are documented here;
 * inherited base fields are part of UI_WindowBase/UI_ChildWindow layout.
 */
struct TrainStation {
    /* ---- Inherited from UI_WindowBase (first ~0x48 bytes) ---- */
    void**   vtable;             /* +0x00 vtable pointer (manual — C struct) */
    /* ... 0x04..0x47: UI_WindowBase fields ... */

    /* ---- TrainStation-specific fields ---- */
    char     bmp_filename[264];   /* +0x48  .bmp filename buffer (sprintf'd)     */
    /* ... 0x14C..0x161: more UI_WindowBase fields ... */
    uint8_t  sprites_loaded;      /* +0x162 sprite/resource loaded flags         */
    /* ... 0x163..0x167: padding ... */
    uint8_t  field_168;           /* +0x168 byte flag                            */
    uint8_t  field_169;           /* +0x169 byte flag                            */
    uint8_t  z_threshold;         /* +0x16A z-proximity threshold (default 8)    */
    uint8_t  field_16B;           /* +0x16B byte (default 0xFF)                  */
    uint8_t  removable_flag;      /* +0x16C byte: 0=not removable               */
    uint8_t  _pad_16D[3];         /* +0x16D padding                              */
    int32_t  hover_sound_id;      /* +0x170 sound resource ID (default 'M'=77).
                                     SET by Init (0x436490) but NEVER READ by
                                     any known function — possibly dead code or
                                     used by an external dispatch path.         */
    int32_t  sound_string_id;     /* +0x174 string resource ID for hover sound,
                                     read by OnMouseMove (0x436960) and
                                     OnMouseLeave (0x4369A0). Init sets to 0,
                                     making both functions effectively no-ops
                                     unless set externally.                     */
};

#ifdef __cplusplus
}
#endif
