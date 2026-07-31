/**
 * ResourceManager.h — Central asset registry for Lego Loco
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The Resource Manager is the central asset registry. It loads and tracks all
 * game resources (sprites, sounds, UI elements), manages the localized string
 * table, animates clock hands, and provides sound playback convenience methods.
 * It is the factory through which most GameObject subclasses obtain their sprite
 * data.
 *
 * This is a non-virtual class (no vtable). All methods are called directly
 * through the global instance g_resmgr at 0x4855E8.
 *
 * Size: 0x241BC bytes
 * Global instance: g_resmgr @ 0x004855E8
 *
 * Resource ID format:
 *   Bits 31-16: resource type (see GetResourceType)
 *   Bits 15-0:  resource index
 *   Type extraction: (id >> 10) & 0xF
 *
 * Internal storage layout:
 *   +0x002C: resource_type_idx (int32*[0x4001]) — pointer-to-pointer indirection
 *   +0x10030: resource_ptrs     (void*[0x4001]) — main resource object registry (IDs 0-0x3FFF)
 *   +0x20034: string_cache      (void*[0x1061]) — string table cache (IDs 0x5000-0x605F)
 *   +0x241B8: language_id       (int32)         — language offset selector
 *
 * Resource types (from ID >> 10):
 *   0  = ChildWindow (UI)
 *   1  = ChildWindow (UI)
 *   2  = ExitGame window
 *   3  = ScriptedObject
 *   4  = ChildWindow
 *   5  = ChildWindow (persistent)
 *   6  = CursorEditWindow / ChildWindow
 *   7  = TrainStation / ChildWindow
 *   8  = TrainStation / ChildWindow
 *   9  = ChildWindow (UI)
 *   10 = (unused fallthrough)
 *   11 = (unused fallthrough)
 *   12 = ExitGame window
 *   13 = (unused fallthrough)
 *   14 = ChildWindow (some with persistence for IDs > 0x3801)
 *   15 = (persistent, never destroyed)
 */

#pragma once

#include "../shared/types.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif
#include "../game/TrackPiece.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <stdint.h>

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

struct GameAudio;

/* ================================================================== */
/* ResourceObject — typed view of the common resource vtable            */
/* ================================================================== */
class ResourceObject {
public:
    /* Original slot 0 is the compiler-generated scalar deleting
     * destructor. This bridge names the slot without writing a vtable. */
    virtual void Destroy(uint8_t flags) = 0;
    virtual void* Lock(int32_t flags, int32_t mode) = 0;
    virtual void Unlock() = 0;

protected:
    ~ResourceObject() = default;
};

/* ================================================================== */
/* RESDATA — resource data object (loaded by ResourceManager)          */
/* Non-virtual class with vtable 0x478274. Base size: 0x1D8 bytes.    */
/* Building resources extend this with schedule data at +0x534/+0x548. */
/*                                                                     */
/* Vtable layout:                                                      */
/*   [0] scalar deleting destructor (ResourceData_Dtor, 0x447B60)     */
/*   [1] Lock/GetSurface (returns pixel data surface)                  */
/*   [2] Unlock/Release surface                                       */
/* ================================================================== */

/* ================================================================== */
/* ResourceEntry — individual resource entry with file backing         */
/* Non-virtual class with vtable 0x478278. Size: 0x12C bytes (300).   */
/*                                                                     */
/* Vtable layout:                                                      */
/*   [0] scalar deleting destructor (RESMGR_ResourceEntry_Dtor,       */
/*       0x4489D0)                                                    */
/*   [1] OpenResourceFile / Parse (virtual, reads + parses data)      */
/* ================================================================== */

struct ResourceEntry {
    void* vtable;                 /* +0x00: compiler-managed resource vtable */
    /* +0x04: resource ID (-1 for external files) */
    int32_t resource_id;

    /* +0x08: flags/status word */
    int16_t flags;

    /* +0x09: validity flag (1 = loaded and ready) */
    uint8_t is_valid;

    /* +0x0A..+0x0B: padding */

    /* +0x0C: buffer/stream pointer (DirectSound buffer) */
    void* buffer;

    /* +0x10..+0x17: additional fields (direct sound format, etc.) */

    /* +0x18: path string (formatted "%s\\name.wav", 0x108 bytes max) */
    char   path[0x108];

    /* +0x120: refcount (for sound resources) */
    int32_t refcount;

    /* +0x128: flag byte (8-bit flag for playback mode) */
    uint8_t mode_flag;
};

/* ================================================================== */
/* ScreenSaverModule — screensaver password/registry module            */
/* Standalone struct at 0x004A9910, NOT part of ResourceManager.      */
/* Size: ~0x80+ bytes.                                                */
/* ================================================================== */
struct ScreenSaverModule {
    /* +0x00..+0x6F: other fields (not yet mapped) */

    /* +0x70: GetPasswordStatus function pointer */
    void* get_password_status;

    /* +0x74: VerifyScreenSavePwd function pointer */
    void* verify_screen_save_pwd;

    /* +0x78: HMODULE for password.cpl (loaded via LoadLibrary) */
    void* password_cpl_module;

    /* +0x7C..+0x7F: padding */
};

/* ================================================================== */
/* SoundObject — TrackPiece subclass with text label                    */
/* Size: 0x68 bytes on the recovered 32-bit layout.                    */
/* TrackPiece supplies the vtable, type, position, timer, and active    */
/* fields through +0x57; SoundObject owns the fields beginning at +0x58. */
/* ================================================================== */

class SoundObject : public TrackPiece {
public:
    SoundObject(int32_t text_length, void* town, RESDATA* resource,
                void* font, uint16_t flags);
    ~SoundObject() override;
    SoundObject(const SoundObject&) = delete;
    SoundObject& operator=(const SoundObject&) = delete;

    /* TrackPiece supplies the inherited fields through +0x57. */
    uint8_t   consume_state;     /* +0x58  0=inactive, 1=consume */
    uint8_t   _pad_59[3];        /* +0x59 */
    int32_t   max_text_len;      /* +0x5C  text buffer capacity */
    char*     text_buf;          /* +0x60  heap-allocated text buffer */
    void*     font;              /* +0x64  font handle */
};

/* ================================================================== */
/* ResourceManager class                                               */
/* ================================================================== */

class ResourceManager {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* +0x00: No vtable (non-virtual class) */

    HFONT      font_small;           /* +0x04  Arial 12pt, 800 weight              */
    HFONT      font_medium;          /* +0x08  Arial 14pt, 700 weight              */
    HFONT      font_title;           /* +0x0C  Arial 16pt, 700 weight              */
    HFONT      font_large;           /* +0x10  Arial 24pt, 700 weight              */
    HFONT      font_clock;           /* +0x14  Arial 20pt, 900 weight              */

    int32_t    file_data_handle;     /* +0x18  DDRAW_LoadFile result (0=invalid)   */

    uint8_t    _pad_1C[0x0C];        /* +0x1C..+0x27  unknown/padding */

    int32_t    clock_hand_segment;   /* +0x28  current clock hand segment (0-11)   */

    /* +0x2C: resource_type_idx — indirection array.
     * Each entry stores the address of the corresponding slot in resource_ptrs. */
    int32_t    resource_type_idx[0x4001];  /* +0x2C..+0x1002F (16385 entries) */

    /* +0x10030: resource_ptrs — main resource object registry.
     * Array of void* pointers, one per resource ID (0-0x3FFF).
     * NULL=unloaded, -1=not-found, non-null=valid resource object. */
    int32_t    resource_ptrs[0x4001];      /* +0x10030..+0x20033 (16385 entries) */

    /* +0x20034: string_cache — string ID to resource mapping.
     * Maps string IDs (0x5000-0x605F) to loaded string resource objects. */
    int32_t    string_cache[0x1061];       /* +0x20034..+0x241B7 (4193 entries) */

    int32_t    language_id;          /* +0x241B8 language offset selector
                                     * Values: 0=English, 1=French, 2=German,
                                     * 3=default/unknown, 4=Spanish, 5=Italian,
                                     * 6=Dutch, 7=Portuguese, 8=Swedish, 9=Danish */

    /* ================================================================ */
    /* Constructor / Initializer                                         */
    /* ================================================================ */

    /**
     * Init — Initialize the resource manager.
     * Address: 0x446050
     *
     * Loads the resource file archive, creates 5 GDI fonts (Arial variant),
     * initializes keyboard/mouse input subsystems, loads the full string
     * table (IDs 0x5000-0x6060), and pre-loads string resources for IDs
     * 0x400-0x3FFF. Sets clock_hand_segment to -1 (no valid segment).
     *
     * Called by: GameLoop_Setup (0x406DBC)
     *
     * @return  bool — 1 on success, 0 if DDRAW_GetSurface or file load failed
     */
    bool Init();

    /* ================================================================ */
    /* Destructor / Shutdown                                             */
    /* ================================================================ */

    /**
     * Shutdown — Release all resources and GDI fonts.
     * Address: 0x446340
     *
     * Stops all audio playback, frees all loaded resources, releases and
     * recreates DirectDraw surfaces, destroys audio subsystem, deletes 5
     * GDI font objects at +0x04..+0x14.
     *
     * Called by: CGWND_Cleanup (0x407AD2), NET_Shutdown (0x44600B)
     *
     * @return  always 1
     */
    int32_t Shutdown();

    /* ================================================================ */
    /* Data Initialization (language detection)                          */
    /* ================================================================ */

    /**
     * InitData — Detect game language from config or system locale.
     * Address: 0x4463C0
     *
     * Reads [Locale] Language= from config INI. Compares against known
     * language names: FRENCH, DUTCH, ENGLISH, SPANISH, ITALIAN, NORWEGIAN,
     * PORTUGUESE, SWEDISH. Falls back to GetSystemDefaultLCID() -> LCID
     * mapping. Writes language ID to this->language_id (+0x241B8).
     *
     * Called by: WinMain (0x463002) with ECX = &g_resmgr
     */
    void InitData();

    /* ================================================================ */
    /* Resource Management                                               */
    /* ================================================================ */

    /**
     * FreeAllResources — Destroy all resource objects.
     * Address: 0x4467E0
     *
     * Iterates two arrays at +0x10030 (16385 entries) and +0x20034 (4193
     * entries). For each non-null, non-sentinel entry, calls vtable[0]
     * destructor with flag=1 (free memory). Resets -1 sentinels to 0.
     * Also clears the type-index array at +0x2C.
     *
     * Called by: Shutdown (0x446358)
     */
    void FreeAllResources();

    /**
     * GetById — Look up a resource by ID (range 0-0x3FFF).
     * Address: 0x446EA0
     *
     * Accesses the type-index array at +0x2C to get a pointer to the
     * resource slot at +0x10030, then dereferences to return the resource
     * object pointer. If the entry is 0 (not yet loaded), lazy-loads the
     * string by calling LoadStringTable then AddString.
     *
     * Called by: AnimateClock, GameObject_InitBase, Cursor_InitSprites,
     *            Train_LoadSprites, UI_PaintWindow, and 90+ other callers
     *
     * @param resId  Resource ID (0-0x3FFF). Returns -1/errno for invalid.
     * @return       Resource object pointer, or 0 (not found), or -1 (error).
     *               Must check > 0 before use.
     */
    int32_t GetById(int32_t resId);

    /**
     * GetStringById — Look up a string table entry by ID (range 0x5000-0x605F).
     * Address: 0x4472B0
     *
     * Accesses the string cache at +0xC034 (maps to +0x20034 array) to
     * retrieve a loaded string resource. Lazy-loads via LoadStringTable on
     * first access. Used to retrieve localized UI strings.
     *
     * Called by: GameObject_PlayAnimation, GameAudio_PlayResource,
     *            UI_PaintWindow, HelpWnd_Hide/GoPrevPage/GoNextPage,
     *            TrainStation_OnMouseMove/OnMouseLeave,
     *            NETMAN_ConnectToPeer/JoinSession, and 30+ other callers
     *
     * @param stringId  String ID (0x5000-0x605F). Returns -1/errno for invalid.
     * @return          String resource pointer, or 0 (not found), or -1 (error).
     */
    int32_t GetStringById(UINT stringId);

    /**
     * LoadStringToResource — Load single string into main resource array.
     * Address: 0x4470B0
     *
     * Loads a range of strings starting from param_1 up to param_1 (single
     * ID) into the main resource array at +0x10030. Applies language offset
     * for IDs 100-500. Calls AddString for each loaded string.
     * Returns the slot value, or 0 with errno=2 if not found.
     *
     * Called by: five call sites in one function (0x44EAC9 area)
     *
     * @param resId  Resource ID to load
     * @return       Resource slot value (pointer, 0, or -1->0)
     */
    int32_t LoadStringToResource(UINT resId);

    /**
     * RegisterDependency — Write dependency pointer to type-index array.
     * Address: 0x447290
     *
     * Writes a pointer to the resource slot at +0x10030[param_2] into the
     * type-index array at +0x2C[param_1]. This creates a weak reference
     * from one resource slot to another. Called by INPUT_SetMouse for
     * cursor sprite dependencies.
     *
     * @param depIndex  Index in the type-index array (+0x2C) to write to
     * @param resIndex  Resource index (slot at +0x10030) to reference
     */
    void RegisterDependency(int32_t depIndex, int32_t resIndex);

    /* ================================================================ */
    /* String Table Loading                                              */
    /* ================================================================ */

    /**
     * LoadStringTable — Load a range of string resources.
     * Address: 0x446CC0
     *
     * Loads localized strings from the EXE's string table resources into
     * the string cache (+0xC034 / +0x20034). Applies language-specific
     * offsets for IDs 100-500 (0x190-0x7D0 in iVar3 computation).
     * Each loaded string creates a resource entry via operator_new(300)
     * and stores it in the string cache. On failure, stores -1.
     *
     * Called by: Init, GetById, AnimateClock, PlaySound, PlaySoundAt
     *
     * @param startId  First string ID to load
     * @param endId    Last string ID to load (clamped to 0x6060)
     * @return         BOOL — 1 if all strings loaded, 0 otherwise
     */
    BOOL LoadStringTable(UINT startId, int32_t endId);

    /**
     * AddString — Create and register a resource object.
     * Address: 0x446840
     *
     * Central resource factory. Creates objects based on resource type
     * (bits 10+ of resource_id). Types dispatch to:
     *   0,1,4,9,10,11,13,14: UI_CreateChildWindow (0x168 bytes)
     *   2,12:                INPUT_ExitGame (0x630 bytes)
     *   3:                   RESDATA_ScriptedObject_AddChild (0x63C bytes)
     *   6:                   CGWND_CursorEditWindow_Ctor (0x7AC bytes)
     *   7,8:                 TrainStation_Ctor (0x178 bytes)
     *
     * Stores result at this + resId*4 + 0x10030. Destroys non-persistent
     * entries unless resource type is 1 or 15 (persistent).
     * SEH-protected.
     *
     * Called by: LoadStringTable, Init, RESMGR_LoadStringTable_single
     *
     * @param resId    Resource ID
     * @param strPtr   String pointer (typically localized string data)
     * @return         1 on success, 0 if already exists, -1 on error
     */
    uint8_t AddString(int32_t resId, int32_t strPtr);

    /* ================================================================ */
    /* Formatted String Loading                                          */
    /* ================================================================ */

    /**
     * FormatResourceString — Load localized string from EXE string table.
     * Address: 0x447330
     *
     * Loads a string from the EXE's string table resources into the
     * provided buffer. For resource IDs 100-500, applies a language
     * offset based on language_id (+0x241B8) to select the correct
     * localized table. Falls back to the un-offset ID if the translated
     * string is not found.
     *
     * This is the primary formatted-string loader used by UI_WindowBase,
     * GameWindow, and other classes to populate window titles, tooltip
     * text, and menu labels.
     *
     * Called by: WinMain, CGWND_InitGame, GameWindow_Ctor,
     *            UI_WindowBase_Ctor, GameSetupPanel, HelpWnd,
     *            NETMAN_ProcessMessage, and 30+ other callers
     *
     * @param resId    String resource ID (in EXE string table range)
     * @param outBuf   Output buffer for the formatted string
     * @param bufSize  Maximum output buffer size in bytes
     */
    void FormatResourceString(UINT resId, char* outBuf, int32_t bufSize);

    /* ================================================================ */
    /* Clock Animation                                                   */
    /* ================================================================ */

    /**
     * AnimateClock — Render the in-game clock overlay.
     * Address: 0x447400
     *
     * Computes the current hour segment (0-11) from the timestamp.
     * (timestamp / 60) % 60 / 5 + 1 % 12 -> maps each 5-minute block
     * to a clock hand position.
     *
     * Plays sounds on segment change, renders 4 sprite layers, then
     * invalidates the full viewport rect.
     *
     * Called by: DDRAW_SelectBuilding (0x4594BA), UI_PaintWindow (0x4257D1)
     *
     * @param timestamp  Game tick count (in milliseconds since game start)
     */
    void AnimateClock(int32_t timestamp);
};

/* ================================================================== */
/* Global instance declaration                                          */
/* ================================================================== */

extern ResourceManager g_resmgr;  /* @ 0x004855E8 */

/* ================================================================== */
/* ScreenSaverModule global instance                                    */
/* ================================================================== */

extern ScreenSaverModule g_scrsaver_mod;  /* @ 0x004A9910 */

/* ================================================================== */
/* Free utility functions (not class members)                           */
/* ================================================================== */

/**
 * GetResourceType — Extract resource type from a resource ID.
 * Address: 0x446030
 * Calling convention: __cdecl
 *
 * Extracts resource type from bits 10+ of the resource ID.
 * Valid types are 0-15. Returns 0 for types >= 16.
 *
 * @param id  Resource ID
 * @return    Resource type (0-15), 0 for invalid
 */
UINT __cdecl GetResourceType(UINT id);

/**
 * PlaySound — Play a sound at the global listener position.
 * Address: 0x447930
 * Calling convention: __cdecl
 *
 * Lazy-loads sound resource from string table (ID 0x5000-0x605F)
 * into the global sound cache at 0x49161C, then plays it via
 * GameAudio_AllocChannel at the global listener position (g_listener_x/y)
 * with priority 4 and no callback.
 *
 * Called by: GAMESTATE_HandleClick, HelpWnd_HandleClick,
 *            UI_PaintWindow, TrainStationWindow_Show,
 *            PostcardAlbum_BlitElement, Cursor_UploadCustomContent,
 *            and 66+ other callers
 *
 * @param soundId  Sound resource ID (0x5000-0x605F)
 */
void __cdecl PlaySound(UINT soundId);

/**
 * PlaySoundAt — Play a sound at a specified world position.
 * Address: 0x4479D0
 * Calling convention: __cdecl
 *
 * Same lazy-loading pattern as PlaySound but passes caller-supplied
 * x/y coordinates and priority instead of the global listener position.
 *
 * Called by: Game_HandleLeftClick, TileMap_HandleClick,
 *            Cursor_AdjustColorComponent, NETMAN_SetSessionInfo,
 *            and 48+ other callers
 *
 * @param soundId  Sound resource ID (0x5000-0x605F)
 * @param x        World X position
 * @param y        World Y position
 * @param priority Priority level (higher = more important)
 */
void __cdecl PlaySoundAt(UINT soundId, int32_t x, int32_t y, uint32_t priority);

/**
 * PlaySoundFile — Play a sound from an external WAV file.
 * Address: 0x447A70
 * Calling convention: __cdecl
 *
 * Allocates a temporary resource entry from the file path, loads the
 * sound data, plays it at the specified position/priority, then releases.
 * SEH-protected.
 *
 * @param filename  WAV file path
 * @param x         World X position
 * @param y         World Y position
 * @param priority  Priority level
 */
void __cdecl PlaySoundFile(const char* filename, int32_t x, int32_t y, uint32_t priority);

/**
 * MultiplayerLobby_Reload — Reload/demo lobby cleanup.
 * Address: 0x448350
 * Calling convention: __cdecl
 *
 * Re-enables UI network flag, re-inits network panel, sets polling
 * to 50ms, sets game mode to 1 (hosting), resumes network thread,
 * re-inits audio. Called from WinMain demo mode exit.
 */
void __cdecl MultiplayerLobby_Reload();

/**
 * RESMGR_FreeResourceEntry — Post deferred free-resource message.
 * Address: 0x448970
 * Calling convention: __cdecl
 *
 * Posts WM_USER+5 (0x405) to g_main_window HWND (+0x08).
 * Used during DirectPlay initialization cleanup.
 */
void __cdecl RESMGR_FreeResourceEntry();

/* ================================================================== */
/* Screensaver functions (use g_scrsaver_mod at 0x4A9910)              */
/* ================================================================== */

/**
 * ResourceManager_Regenerate — Screensaver/demo-mode startup.
 * Address: 0x4480C0
 *
 * If g_demo_mode == 1, calls RESMGR_LoadCompressedResource to load
 * password.cpl, reads [ScreenSaver] Sound config, and may play
 * video/music.wav via PlaySoundA. Returns 1 on success.
 *
 * Called by: WinMain (0x463099) with ECX = &g_scrsaver_mod
 */
int32_t ResourceManager_Regenerate();

/**
 * RESMGR_LoadCompressedResource — Load screensaver password DLL.
 * Address: 0x4487F0
 *
 * Loads password.cpl from the system directory, caches GetPasswordStatus
 * and VerifyScreenSavePwd proc addresses. Called from
 * ResourceManager_Regenerate during demo/screensaver init.
 */
void RESMGR_LoadCompressedResource();

/**
 * RESMGR_SelectScreensaver — Select random .sav file for screensaver.
 * Address: 0x4481B0
 * Calling convention: __cdecl
 *
 * Reads [ScreenSaver] Layout/Random config. If Random!=0, enumerates
 * ScrSaver/*.sav files, picks one randomly via CRT_rand(), formats
 * as "ScrSaver\\<name>" into the output buffer.
 *
 * @param outPath  Output buffer for formatted path string
 */
void __cdecl RESMGR_SelectScreensaver(char* outPath);

/**
 * RESMGR_EnumScreenSavers — Enumerate .sav files in SaveGame/ or ScrSaver/.
 * Address: 0x448390
 * Calling convention: __cdecl
 *
 * Builds a linked list of allocated filename entries (0x508 bytes each).
 * param_1=0 -> SaveGame (*.sav), param_1!=0 -> ScrSaver (*.sav).
 * Each node has the filename at +0x00 and a next pointer at +0x504.
 *
 * @param scrSaverMode  0 = SaveGame/, non-zero = ScrSaver/
 * @return              Head pointer to linked list, or NULL
 */
char* __cdecl RESMGR_EnumScreenSavers(char scrSaverMode);

/**
 * RESMGR_VehicleAnimationTick — Per-frame vehicle animation update.
 * Address: 0x448120
 *
 * Counts animation frames. Every 2048 ticks, randomly picks one of 3
 * vehicle slots from a global array and calls Vehicle_CalcSpeed.
 * Called from GameLoop_FrameUpdate (0x45C3EA).
 */
void RESMGR_VehicleAnimationTick();

/* ================================================================== */
/* RESDATA methods (operate on RESDATA struct)                         */
/* ================================================================== */

/**
 * RESMGR_ResourceData_Init — Initialize a RESDATA struct to zero.
 * Address: 0x447B20
 *
 * Initializes the resource-data object and zeros all fields:
 * pixels (+0x1C4), streams (+0x1C8/+0x1CC), asset data (+0x1D0/+0x1D4),
 * dimensions (+0xB0/+0xB2/+0xB4).
 */
void RESMGR_ResourceData_Init(RESDATA* resdata);

/**
 * RESMGR_ReleaseResource — Mid-level resource release.
 * Address: 0x447B90
 *
 * Resets resource-data state, then calls RESMGR_RemoveResource
 * to free sub-resources (streams, pixels, asset data). Does NOT free
 * the struct's own memory. Used by save-game + SEH unwind paths.
 */
void RESMGR_ReleaseResource(RESDATA* resdata);

/**
 * RESMGR_LoadResource — Primary resource file loader into RESDATA.
 * Address: 0x447BA0
 *
 * Loads file via AssetMgr_LoadFile (install-path-stripping) or falls
 * back to WIN32_StreamOpenFile. Reads 0x114-byte header to +0xB0,
 * allocates pixel buffer at +0x1C4 (width*height*depth), reads pixel
 * data. Returns 1 on success, 0 on failure. SEH-protected.
 */
int8_t RESMGR_LoadResource(RESDATA* resdata, const char* filename);

/**
 * RESMGR_LockResource — Read entity record from primary stream.
 * Address: 0x447DB0
 *
 * Reads 0x80 bytes from primary stream at +0x1C8 into the entity
 * record buffer at +0x04. Returns buffer pointer, or NULL on short read.
 * Used by INPUT_LoadSaveFile entity-loading loop.
 *
 * @return  Pointer to entity buffer (+0x04), or NULL
 */
void* RESMGR_LockResource(RESDATA* resdata);

/**
 * RESMGR_UnlockResource — Read vehicle record from primary stream.
 * Address: 0x447DF0
 *
 * Reads 0x2C bytes from primary stream at +0x1C8 into the vehicle
 * record buffer at +0x84. Returns buffer pointer, or NULL on short read.
 * Used after entity records are processed in save loading.
 *
 * @return  Pointer to vehicle buffer (+0x84), or NULL
 */
void* RESMGR_UnlockResource(RESDATA* resdata);

/**
 * RESMGR_LoadResourceData — Alternative resource loader for tilemap.
 * Address: 0x447E30
 *
 * Creates a UIPANEL surface, opens file stream at +0x1CC (mode 0x92),
 * calls TileMap_CreateOverlay, reads 0x114-byte header to +0xB0 + pixel
 * data into surface buffer. Returns 1 on success, 0 on failure.
 * SEH-protected. Used by INPUT_SaveCurrentWorld.
 *
 * @return  1 on success, 0 on failure
 */
int32_t RESMGR_LoadResourceData(RESDATA* resdata, const char* filename);

/**
 * RESMGR_WriteSaveRecord — Write 0x80-byte entity record to secondary stream.
 * Address: 0x447F50
 *
 * Writes serialized entity data to the secondary stream at +0x1CC.
 * Used during save serialization. Misnamed as "FindResource".
 *
 * @param data  Pointer to 0x80 bytes of entity data
 * @return      1 on success, 0 on failure (stream closed/error)
 */
int32_t RESMGR_WriteSaveRecord(RESDATA* resdata, const void* data);

/**
 * RESMGR_WriteTableRecord — Write 0x2C-byte level/table entry to secondary stream.
 * Address: 0x447F80
 *
 * Writes serialized vehicle/level entry to the secondary stream at +0x1CC.
 * Used during save serialization. Misnamed as "AddResource".
 *
 * @param data  Pointer to 0x2C bytes of level entry data
 * @return      1 on success, 0 on failure
 */
int32_t RESMGR_WriteTableRecord(RESDATA* resdata, const void* data);

/**
 * RESMGR_RemoveResource — Clean up all resource sub-data.
 * Address: 0x447FB0
 *
 * Releases COM interfaces at +0x1C8 and +0x1CC, frees raw buffer at
 * +0x1D0 via CRT_free, frees pixel data at +0x1C4 via GLOBAL_free.
 * Called by destructor, load failure paths, and save-game read.
 *
 * @return  1 on success, 0 if pixel data was NULL
 */
int32_t RESMGR_RemoveResource(RESDATA* resdata);

/**
 * RESMGR_IsSaveHeader — Check if RESDATA contains a save header.
 * Address: 0x448030
 *
 * Returns true if the word at +0xB0 equals 8 (save-game header type).
 * Misnamed as "GetRefCount".
 *
 * @return  true if resource_type == 8 (save header)
 */
bool RESMGR_IsSaveHeader(RESDATA* resdata);

/* ================================================================== */
/* ResourceEntry methods (operate on ResourceEntry struct)             */
/* ================================================================== */

/**
 * RESMGR_AllocResourceEntry — Allocate and init a resource entry.
 * Address: 0x448990
 *
 * Initializes the resource-entry object, stores resource_id,
 * formats path as "%s\\name.wav" from param_2 string, calls
 * RESMGR_OpenResourceFile to load the resource.
 * Called by ResourceManager_LoadStringTable.
 *
 * @param resEntry  ResourceEntry object (pre-allocated)
 * @param resId     Resource ID
 * @param nameStr   String to format into path (may be NULL)
 * @return          The ResourceEntry pointer (this)
 */
ResourceEntry* RESMGR_AllocResourceEntry(ResourceEntry* resEntry, int32_t resId, int32_t nameStr);

/**
 * RESMGR_ResourceEntry_Dtor — Scalar-deleting destructor for ResourceEntry.
 * Address: 0x4489D0
 *
 * Destroys the resource-entry sub-object at
 * +0x0C via vtable dispatch (slot 0x48/+0x09 then slot 8), clears +0x09
 * status flag. If flags & 1, frees the struct memory.
 * This is the compiler-generated destruction slot.
 *
 * @param flags  Bit 0: free memory flag
 * @return       this pointer
 */
void* RESMGR_ResourceEntry_Dtor(ResourceEntry* resEntry, uint8_t flags);

/**
 * RESMGR_CreateResourceFromFile — Create ResourceEntry from file path.
 * Address: 0x448A20
 *
 * Initializes the resource-entry state, calls RESMGR_OpenResourceFile,
 * sets resource_id to -1, copies raw file path to +0x18.
 * Used by RESMGR_PlaySoundFile for external WAV files.
 *
 * @param resEntry  ResourceEntry object (pre-allocated)
 * @param filePath  File path string
 * @return          The ResourceEntry pointer
 */
ResourceEntry* RESMGR_CreateResourceFromFile(ResourceEntry* resEntry, const char* filePath);

/**
 * RESMGR_OpenResourceFile — Core resource resolution + loading.
 * Address: 0x448A70
 *
 * Initialises entry fields, then tries (in order):
 *   1. AssetMgr_LoadFile via install path prefix
 *   2. WIN32_StreamOpenPath direct file open
 *   3. GetFileAttributesA existence check
 * Calls vtable[1] on entry to parse loaded data. Sets +0x09=1 on success.
 * SEH-protected.
 */
void RESMGR_OpenResourceFile(ResourceEntry* resEntry);

/* ================================================================== */
/* Sound resource methods (operate on ResourceEntry as sound resource) */
/* ================================================================== */

/**
 * RESMGR_LoadSoundResource — Load and play .wav via DirectSound.
 * Address: 0x448D60
 *
 * Increments refcount at +0x120. On first load, calls Game_LoadWaveFile
 * + GameAudio_Play, locks the DirectSound buffer, copies wave data in,
 * marks +0x09=1 on success. Returns 0 on failure (no audio, file error).
 *
 * @return  0 on failure, (value&0xFFFFFF00) on error code, or success
 */
int32_t RESMGR_LoadSoundResource(ResourceEntry* entry);

/**
 * ReleaseSoundResource — Release refcounted sound resource.
 * Address: 0x448EE0
 *
 * Decrements refcount at +0x120. When refcount reaches 0 and a
 * DirectSound buffer exists at +0x0C (and +0x08 bit0 is clear),
 * stops the buffer and releases it. Always returns 1.
 *
 * @return  always 1
 */
int32_t ReleaseSoundResource(ResourceEntry* entry);

/* ================================================================== */
/* SoundObject — TrackPiece subclass with text label                   */
/* Operates on the canonical SoundObject class.                        */
/* SoundObject fields (key offsets from this pointer):                  */
/*   +0x00: compiler-managed TrackPiece-derived vtable                 */
/*   +0x04: type = 8                                                    */
/*   +0x08: x position                                                  */
/*   +0x0C: y position                                                  */
/*   +0x50: timer counter                                               */
/*   +0x56: active flag (byte)                                          */
/*   +0x58: flag byte / input-state                                     */
/*   +0x5C: max text length                                             */
/*   +0x60: text buffer (char*, heap-allocated)                         */
/*   +0x64: font handle                                                 */
/* ================================================================== */

void* RESMGR_SoundObject_Ctor(void* self, int32_t strLen, int32_t param2,
                              int32_t param3, void* font, uint16_t param5);
/* SoundObject::~SoundObject owns the text buffer and invokes the
 * compiler-generated TrackPiece base destructor. The original scalar
 * deleting wrapper at 0x448FE0 is compiler-generated. */
/**
 * RESDATA_SoundObject_Init — Initialize SoundObject text from a source string.
 * Address: 0x449070
 * Calling convention: __thiscall (ECX = this, args on stack)
 *
 * Copies source string via strncpy into text buffer at +0x60 with max
 * length at +0x5C. Null-terminates. Converts all characters to uppercase
 * via CRT_toupper. Returns pointer to text buffer at +0x60.
 *
 * @param source  Source string to copy and uppercase
 * @return        Pointer to text buffer (+0x60)
 */
void* __thiscall RESDATA_SoundObject_Init(void* self, const char* source);

/**
 * RESDATA_SoundObject_GetState — Return text buffer pointer.
 * Address: 0x4490D0
 * Calling convention: __fastcall (ECX = this)
 *
 * Trivial getter. Returns the text buffer pointer at +0x60. Used by UI
 * code to retrieve the stored text string (e.g. for alphabetical
 * comparison in scroll panel file lists).
 *
 * @return  Text buffer pointer (+0x60)
 */
void* __fastcall RESDATA_SoundObject_GetState(void* self);

/**
 * RESDATA_SoundObject_GetTextLength — Return length of text buffer.
 * Address: 0x4490E0
 * Calling convention: __fastcall (ECX = this)
 *
 * Returns strlen() of the text buffer at +0x60. Used by UI code for
 * text length determination, list iteration, and bounds checking.
 *
 * @return  Length of text string (strlen result)
 */
int32_t __fastcall RESDATA_SoundObject_GetTextLength(void* self);

/**
 * RESDATA_TextInput_HandleChar — Handle character input for text UI.
 * Address: 0x449100
 * Calling convention: __thiscall (ECX = this, char on stack)
 *
 * Processes keyboard input for a text input SoundObject. Backspace (0x08)
 * or Delete (0x2E) removes last character. Printable ASCII (0x20-0x7E)
 * appends if buffer has room. On modification, calls vtable[0x20] update
 * callback. Always plays click sound (res 0x5460) at position (+0x08, +0x0C).
 * Returns 0 if inactive (+0x58 == 0), 1 otherwise.
 *
 * @param ch  Character code (uint32, e.g. VK_BACK=8, VK_DELETE=0x2E, ascii)
 * @return    1 if active, 0 if inactive
 */
uint32_t __thiscall RESDATA_TextInput_HandleChar(void* self, uint32_t ch);

/**
 * RESDATA_TextInput_GetText — Poll text-input completion.
 * Address: 0x449190
 * Calling convention: __fastcall (ECX = this)
 *
 * Polls text input state. Returns 0 if +0x56 active flag is clear.
 * Otherwise advances animation via CGWND_TrackPiece_UpdateAnim.
 * If +0x58 == 1 and timer at +0x50 is a multiple of 3, calls
 * vtable[0x20] (consume text). Returns 1 if active, 0 if not.
 *
 * @return  1 if input active, 0 if inactive
 */
uint32_t __fastcall RESDATA_TextInput_GetText(void* self);

/* ================================================================== */
/* EditControlStruct — Lightweight edit control wrapper struct         */
/* Used by DDRAW overlay code for tooltip/edit functionality.          */
/* Size: 0x7C bytes.                                                   */
/* ================================================================== */
struct EditControlStruct {
    HWND        hwnd;             /* +0x00  edit control window handle        */
    uint8_t     _pad04;           /* +0x04  padding / alignment byte          */
    /* +0x05..+0x07: padding */
    int32_t     field_08;         /* +0x08  (zeroed on init)                  */
    int32_t     field_0C;         /* +0x0C  (zeroed on init)                  */
    int32_t     field_10;         /* +0x10  (zeroed on init)                  */
    int32_t     textMaxLen;       /* +0x14  max text length (0x400 = 1024)    */
    HBRUSH      hBrush;           /* +0x18  solid brush (color 0xA8C4D8)      */
    uint8_t     _pad1C[0x54];     /* +0x1C..+0x6F: padding / other fields     */
    int32_t     field_70;         /* +0x70  (zeroed on init)                  */
    int32_t     field_74;         /* +0x74  (zeroed on init)                  */
    HMODULE     hModule;          /* +0x78  loaded module (freed on cleanup)  */
};

/**
 * UI_EditWindow_Init — Initialize an EditControlStruct.
 * Address: 0x448040
 * Calling convention: __fastcall (ECX = this)
 *
 * Sets all fields to defaults: HWND and module to NULL, brush to
 * solid color 0xA8C4D8, text max length to 0x400.
 *
 * @param editWnd  Pointer to EditControlStruct
 * @return         The initialized struct pointer
 */
void* __fastcall UI_EditWindow_Init(void* editWnd);

/**
 * UI_EditWindow_Cleanup — Clean up an EditControlStruct.
 * Address: 0x448080
 * Calling convention: __fastcall (ECX = this)
 *
 * Deletes the brush, destroys the window handle, and frees the module.
 * All fields are reset to NULL/0 after cleanup.
 */
void __fastcall UI_EditWindow_Cleanup(void* editWnd);