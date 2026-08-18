/**
 * TrainStation.h — Train station city-view interaction object
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * TrainStation is the city/town-view train station interaction object.
 * It extends ChildWindow and handles mouse events (hover sound via string
 * resource at +0x174), sprite/resource loading, and road connection
 * configuration for the train station building on the map.
 *
 * This is DIFFERENT from TrainStationWindow (vtable 0x478130), which is
 * the UI popup window showing animated train car sprites.
 * TrainStation (vtable 0x478118) is the view-level station interaction
 * object in the city scene.
 *
 * Class hierarchy: ChildWindow -> TrainStation
 *
 * Size: 0x178 bytes (base 0x168 + derived 0x10)
 * Vtable: 0x478118
 *
 * Vtable layout:
 *   [0] +0x00: scalar deleting destructor (0x436460)
 *   [1] +0x04: OnMouseMove (0x436960)
 *   [2] +0x08: OnMouseLeave (0x4369A0)
 *   [3] +0x0C: Render (TrainStation override @ 0x436750)
 *   [4] +0x10: Constructor init body (0x424BF0, non-virtual in base; not
 *              overridden by TrainStation)
 *   [5] +0x14: NULL (reserved)
 */

#pragma once

#include "../ui/UI_ChildWindow.h"
#include <cstdint>

// Status: INTEGRATED

class TrainStation : public ChildWindow {
public:
    /* ================================================================ */
    /* Fields (TrainStation-specific, inherited ChildWindow at +0x00..+0x167) */
    /* ================================================================ */

    uint8_t    field_168;           // +0x168  "walk_speed" directive, 1st value (zeroed in Init)
    uint8_t    field_169;           // +0x169  "walk_speed" directive, 2nd value (zeroed in Init)
    uint8_t    z_threshold;         // +0x16A  "groundwidth" directive (default 8)
    uint8_t    spawn_limit;         // +0x16B  "SpawnLimit" directive (default 0xFF)
    uint8_t    removable_flag;      // +0x16C  "Employable" directive: 0=not removable, non-zero=removable
    uint8_t    _pad_16D[3];         // +0x16D  padding to align int32_t
    int32_t    sex_code;            // +0x170  "sex" directive: ASCII 'M' (0x4D, default) or 'F' (0x46).
                                     //        Confirmed via Render() (0x436750): reads the directive's
                                     //        first character, uppercases it, and stores 'M'/'F' here.
                                     //        Not a sound resource ID despite Init's 0x4D-looking default
                                     //        (which is coincidentally also the 'M' default) — renamed
                                     //        from the earlier placeholder `hover_sound_id`. Still never
                                     //        read by any known function.
    int32_t    sound_string_id;     // +0x174  "PickUpSoundId" directive: string resource ID for hover
                                     //        sound, read by OnMouseMove/OnMouseLeave. Init sets to 0,
                                     //        making both no-ops unless the .dat sets it via Render().

    /* Total: 0x178 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * TrainStation constructor.
     * Address: 0x436400
     *
     * Calls base ChildWindow constructor, then TrainStation_Init to load
     * sprites and configure road connections. The binary passes a third
     * undocumented argument (1) to the base constructor, but it is unused
     * and omitted here.
     *
     * Called by: CGWND init path during train station creation
     *
     * @param resourceId  Resource/panel ID for base window
     * @param name        Non-null to load sprites and fully initialize
     *                    (widened from the original's int32_t ABI slot to a
     *                    real `const char*` — see ui/UI_ChildWindow.h's
     *                    ChildWindow constructor doc for why the int32
     *                    pointer-handle round trip is unsafe on this host)
     */
    TrainStation(uint32_t resourceId, const char* name);

    /**
     * Virtual destructor (vtable[0]).
     * Address: 0x436460 (scalar-deleting-destructor thunk)
     * Body: 0x436480 (TrainStation_BaseDtor, then delegates to base)
     *
     * Cleans up any TrainStation-specific resources, then calls base
     * class destructor for inherited cleanup (renderSurface, heapBuffer, etc.).
     * Compiler-managed virtual dispatch.
     */
    virtual ~TrainStation();

    /* ================================================================ */
    /* Virtual Methods (overridden from ChildWindow)                    */
    /* ================================================================ */

    /**
     * OnMouseMove — Handle mouse motion over this window.
     * Address: 0x436960 (vtable[1])
     *
     * Reads string resource ID from sound_string_id (+0x174). If non-zero,
     * looks it up via ResourceManager_GetStringById and loads the sound
     * resource via RESMGR_LoadSoundResource to play the hover sound.
     * Then chains to base class OnMouseMove for standard rendering.
     *
     * NOTE: sound_string_id is initialized to 0 in Init, making the sound
     * load a no-op unless set externally. The field at +0x170 (sex_code)
     * is SET by Init and by Render()'s "sex" directive but NEVER READ by
     * this or any other known function — likely dead code.
     *
     * @param x      Mouse X coordinate
     * @param y      Mouse Y coordinate
     * @return       The render-surface pointer, or null if unavailable
     */
    virtual void* OnMouseMove(int32_t x, int32_t y) override;

    /**
     * OnMouseLeave — Handle mouse leaving this window.
     * Address: 0x4369A0 (vtable[2])
     *
     * Reads string resource ID from sound_string_id (+0x174). If non-zero,
     * looks it up via ResourceManager_GetStringById and releases the sound
     * resource via RESMGR_ReleaseSoundResource.
     * Then chains to base class OnMouseLeave for inherited cleanup.
     *
     * NOTE: Same caveat as OnMouseMove — sound_string_id initialized to 0
     * makes this a no-op unless set externally.
     */
    virtual void OnMouseLeave() override;

    /**
     * Render — Parse stream for TrainStation configuration (vtable[3]).
     * Address: 0x436750
     *
     * TrainStation's own override — NOT inherited from ChildWindow::Render
     * (0x424E00, which is a distinct, still-deferred function). Real
     * decompilation recovered this session (0x436750, 513 bytes, 150 x86
     * instructions; Ghidra's auto-analysis never followed the vtable
     * pointer to define it as a function, so it was created and decompiled
     * directly for this pass).
     *
     * Reads directive lines from the stream via WNDPROC_CriticalSectionLock
     * until a terminator line is hit (same `CRT_wcsstr`/`_stricmp(line,
     * sentinel)` idiom as ChildWindow's sibling parsers — real _stricmp
     * semantics: a match is a 0 return, confirmed via disassembly of
     * TrainStation::Render at 0x436750, `CALL 0x471480; TEST EAX,EAX;
     * JNZ <skip>`). Recognized directives:
     * "walk_speed" (two values -> field_168/field_169), "Employable"
     * (-> removable_flag), "sex" (first char, uppercased, 'M'/'F' ->
     * sex_code), "groundwidth" (-> z_threshold), "SpawnLimit" (->
     * spawn_limit), "PickUpSoundId" (raw stream write -> sound_string_id).
     *
     * Called by: TrainStation::Init during sprite/config loading.
     *
     * @param stream  Pointer to a WNDPROC stream object (file or memory-based)
     * @return        uint8_t: 1 if parsing succeeded, 0 if stream ended or error
     */
    virtual uint8_t Render(void* stream) override;

    /* ================================================================ */
    /* Private Helper                                                    */
    /* ================================================================ */

    /**
     * Init — Initialize train station with sprites and configuration.
     * Address: 0x436490
     * Size: 717 bytes (207 instructions)
     *
     * Comprehensive initialization:
     *   1. Initialize all TrainStation-specific fields to defaults.
     *   2. If param2 == 0: early return (no sprite loading).
     *   3. If param2 != 0:
     *      a. Construct .dat/.bmp filenames via sprintf.
     *      b. Load .dat file via AssetMgr, render via UI_ChildWindow_Render.
     *      c. Load .bmp file via stream path, render similarly.
     *      d. For each rendered sub-window, clear offset fields when > 0.
     *      e. Adjust animation frame IDs to match array indices.
     *      f. Set default road offset (Y=8) if both offsets are zero.
     *
     * @param param1  Resource/panel ID (passed from constructor)
     * @param name    Non-null to load sprites; nullptr to defer
     */
    void Init(int32_t param1, const char* name);
};

/* ================================================================== */
/* Extern "C" Compatibility Bridge                                    */
/* ================================================================== */

extern "C" {

/**
 * TrainStation_Ctor — Constructor bridge using placement-new.
 * Address: 0x436400
 *
 * Replaces the original free-function TrainStation_Ctor. Calls the C++
 * constructor via placement-new, maintaining binary compatibility with
 * existing callers (e.g., ResourceManager).
 *
 * @param memory   Pre-allocated memory block for the new TrainStation object
 * @param resId    Resource/panel ID for base window
 * @param name     Non-null to load sprites
 * @return         Pointer to the constructed TrainStation object (memory)
 */
void* TrainStation_Ctor(void* memory, uint32_t resId, const char* name);

}  // extern "C"

/* ================================================================== */
/* Layout Verification (x86 32-bit only)                              */
/* ================================================================== */

#if UINTPTR_MAX == 0xffffffffu

static_assert(offsetof(TrainStation, field_168) == 0x168,
    "TrainStation::field_168 offset mismatch");
static_assert(offsetof(TrainStation, field_169) == 0x169,
    "TrainStation::field_169 offset mismatch");
static_assert(offsetof(TrainStation, z_threshold) == 0x16A,
    "TrainStation::z_threshold offset mismatch");
static_assert(offsetof(TrainStation, spawn_limit) == 0x16B,
    "TrainStation::spawn_limit offset mismatch");
static_assert(offsetof(TrainStation, removable_flag) == 0x16C,
    "TrainStation::removable_flag offset mismatch");
static_assert(offsetof(TrainStation, sex_code) == 0x170,
    "TrainStation::sex_code offset mismatch");
static_assert(offsetof(TrainStation, sound_string_id) == 0x174,
    "TrainStation::sound_string_id offset mismatch");
static_assert(sizeof(TrainStation) == 0x178,
    "TrainStation must match the 32-bit loco.exe layout (0x178 bytes)");

#endif
