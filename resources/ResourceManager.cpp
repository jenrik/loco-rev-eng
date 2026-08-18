/**
 * ResourceManager.cpp — ResourceManager implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file documents the ResourceManager class — the central asset registry
 * that loads and tracks all game resources, localized strings, clock animation,
 * and sound playback — plus all RESMGR_* helper functions for resource data
 * loading, screensaver management, and sound resource management.
 *
 * Key design notes:
 *   - ResourceManager is a non-virtual class (no vtable). Global instance at 0x4855E8.
 *   - Internal arrays use a two-level indirection: type_idx[] stores pointers
 *     to resource_ptrs[] slots. This supports lazy loading.
 *   - Sound functions access a separate global sound cache at 0x49161C.
 *   - String table loading applies language offsets for IDs 100-500 based on
 *     the language_id field (+0x241B8).
 *   - RESDATA is the raw resource data object with pixel buffer + IStreams.
 *   - ResourceEntry is the file-backed resource entry with DirectSound buffer.
 */

// Status: TRANSCRIBED

#include "ResourceManager.h"
#include "../core/CGWND.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <new>
#include <cstdio>

/* Class headers for placement-new constructor dispatch in AddString */
#include "../ui/UI_ChildWindow.h"
#include "../ui/CursorEditWindow.h"
#include "../game/TrainStation.h"
#include "../input/BuildingDescriptorEditor.h"
#include "../input/TrackTileDescriptor.h"
#include "../audio/GameAudio.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* C++ allocation helpers */
void* operator_new(uint32_t size);   /* operator new @ 0x00465CE0 */
void  GLOBAL_free(void* ptr);        /* @ 0x00465CD0 */

extern "C" {

/* Windows API */
void* __stdcall GetModuleHandleA(void* lpModuleName);
int32_t __stdcall LoadStringA(void* hInstance, UINT uID, char* lpBuffer, int32_t cchBufferMax);
int32_t __stdcall DeleteObject(void* hObject);
void* __stdcall CreateFontA(
    int32_t nHeight, int32_t nWidth, int32_t nEscapement, int32_t nOrientation,
    int32_t fnWeight, uint32_t fdwItalic, uint32_t fdwUnderline,
    uint32_t fdwStrikeOut, uint32_t fdwCharSet, uint32_t fdwOutputPrecision,
    uint32_t fdwClipPrecision, uint32_t fdwQuality, uint32_t fdwPitchAndFamily,
    const char* lpszFace);

void __stdcall SetRect(void* lprc, int32_t left, int32_t top, int32_t right, int32_t bottom);
void __stdcall CopyRect(void* lprcDst, const void* lprcSrc);
void __stdcall OffsetRect(void* lprc, int32_t dx, int32_t dy);

int32_t __stdcall wsprintfA(char* out, const char* format, ...);
uint32_t __stdcall GetSystemDefaultLCID(void);
uint32_t __stdcall GetSystemDirectoryA(char* buffer, uint32_t size);
void* __stdcall LoadLibraryA(const char* libName);
void* __stdcall GetProcAddress(void* hModule, const char* procName);
uint32_t __stdcall SetErrorMode(uint32_t mode);
uint32_t __stdcall GetFileAttributesA(const char* path);
int32_t __stdcall PostMessageA(void* hWnd, uint32_t msg, uint32_t wParam, uint32_t lParam);
int32_t __cdecl PlaySoundA(const char* pszSound, void* hmod, uint32_t fdwSound);

/* CRT */
void  CRT_free(void* ptr);           /* @ 0x00465280 */
int32_t* CRT_errno(void);            /* @ 0x00467FD0 — returns pointer to errno */
uint32_t CRT_rand(void);             /* @ 0x004682A0 */
void CRT_srand(uint32_t seed);       /* @ 0x004682B0 */
uint32_t CRT_timeGetTime(int32_t*);  /* @ 0x00466AF0 */
void CRT_sprintf_buf(void* buf, const char* format, ...); /* @ 0x00467F60 */
int32_t CRT_strupr(char* str);       /* @ 0x00467D00 */
void* CRT_ceil(void* stream, void* buf, uint32_t size);   /* @ 0x00465010 — this is the
                                                             *   WIN32 stream WRITE function
                                                             *   (WIN32_StreamWrite in
                                                             *   ResDataSave.cpp), NOT CRT
                                                             *   ceil (mislabel fixed) */
/* @ 0x00465090 is the WIN32 WRITE-stream constructor (mode|2,
 * WIN32_StreamOpenWriteFile in ResDataSave.cpp) — NOT CRT floor
 * (mislabel fixed; no callers remain). */

} /* extern "C" */

/* Sound system (C++ linkage) */
extern GameAudio* g_audio;           /* @ 0x004FD3BC */
extern int32_t g_listener_x;         /* @ 0x004AAD2C */
extern int32_t g_listener_y;         /* @ 0x004AAD30 */

int32_t GameAudio_AllocChannel(                       /* @ 0x00413210 */
    GameAudio* audio, int32_t soundResource,
    void* pCallback, int32_t x, int32_t y,
    uint32_t priority, uint32_t flags);
int32_t GameAudio_Play(GameAudio* audio, void* desc, void** buffer, int32_t flag); /* @ 0x004132F0 */
/* NOTE: a `Game_LoadWaveFile` extern used to be declared here at
 * "@ 0x00412700", but 0x00412700 is actually TrackPos_IsObjectBetween
 * (confirmed via Ghidra decompile) — a stale/wrong-address leftover, unused
 * anywhere in this file. Real Game_LoadWaveFile is at 0x413660, defined in
 * native/wave_io.c; removed rather than fixed in place since nothing here
 * calls it. */
char* DDRAW_GetDsoundErrorString(int32_t error);      /* @ 0x0045BBC0 */

/* Graphics system */
bool DDRAW_GetSurface(void);                         /* @ 0x0045B500 */
void DDRAW_ReleaseSurfaces(void);                    /* @ 0x0045BAA0 */
void DDRAW_DestroyAudio(void);                       /* @ 0x0045BB20 */
int32_t DDRAW_LoadFile(int32_t* outHandle, const char* filename); /* @ 0x0045CAA0 */
void Config_GetIniString(void* config, const char* section,
    const char* key, const char* def, char* out, int32_t maxLen); /* @ 0x00452D80 */
int32_t Config_GetIniInt(void* config, const char* section,
    const char* key, int32_t defaultValue);           /* @ 0x00452D60 */

/* BUG-mode3-input-processing-crashes.md: this used to declare a
 * `void* INPUT_ExitGame(void*, int32_t, int32_t)` free function with no
 * definition anywhere in the tree (a stale comment here claimed "loud
 * deferred stub in InputMgr.cpp" — no such definition actually exists
 * there either, just a comment referencing the address) — a dangling
 * extern, permitted to link by -Wl,--unresolved-symbols=ignore-all,
 * that crashed calling through a null function pointer the instant
 * AddString's odd-resId type-0/2/4/12/13 branches were ever actually
 * reached (Game::PlaySound → ResourceManager::GetById → AddString).
 * input/BuildingDescriptorEditor.h's BuildingDescriptorEditor_Ctor is
 * the real, already-implemented placement-new bridge for this exact
 * 0x630-byte allocation (its own doc comment already names this file's
 * AddString and cites the same "INPUT_ExitGame" Ghidra misnomer) —
 * these call sites just hadn't been updated to use it. */
extern "C" {
    void* CursorEditWindow_Ctor(void* memory, int32_t resId, const char* name);        /* @ 0x0040E600 */
}
/* TrackTileDescriptor_Ctor (@ 0x0044B190, Ghidra label
 * "RESDATA_ScriptedObject_AddChild" — misnomer) has real C++ linkage,
 * declared in input/TrackTileDescriptor.h (included above) — it is a
 * BuildingDescriptorEditor subclass constructor bridge, not a
 * ScriptedObject method, and is no longer declared extern "C" here. */

void Town_CopyTiles8bpp_Transparent(
    void* surface, int32_t srcX, int32_t srcY,
    int32_t dstX, int32_t dstY, int32_t srcStride,
    int32_t srcPixelSize, int32_t clipLeft, int32_t clipTop,
    int32_t clipRight, int32_t clipBottom);          /* @ 0x0042C330 */

void TileMap_InvalidateRect(void* tilemap,
    int32_t left, int32_t top, int32_t right, int32_t bottom); /* @ 0x00455840 */

/* UIPANEL_CreateSurface declaration removed 2026-08-14: had zero call
 * sites in this file (RESMGR_LoadResourceData, the one real xref caller
 * of the actual constructor at 0x42A110, is declared in
 * ResourceManager.h but never implemented here) and cited the wrong
 * address anyway -- 0x004286B0 is UIPANEL_DrawButton, confirmed via
 * Ghidra decompile, not UIPANEL_CreateSurface. When RESMGR_LoadResourceData
 * is implemented for real, its surface should be built with
 * `new UIPANEL_Surface()` (graphics/LOCOBITMAP.h/.cpp, 0x42A110). */
uint32_t UIPANEL_LockSurface(void* surf);            /* @ 0x00428A70 */
void TileMap_CreateOverlay(void* tilemap, void* surface, int32_t flag); /* @ 0x00455CB0 */

void* AssetMgr_LoadFile(void** assetMgr, const uint8_t* name, int32_t* outSize); /* @ 0x00457C00 */
/* The WIN32_StreamOpen(File/Path)/Destroy(Immediate)/WNDPROC_StreamCleanup/
 * WIN32_StreamRead declarations formerly here (addresses 0x461600-
 * 0x461880) were dead — RESMGR_OpenResourceFile (declared in
 * ResourceManager.h, real address 0x448A70) is not yet implemented in
 * this file, so nothing calls them — AND wrong: 0x4617C0/0x461600/etc.
 * actually belong to unrelated functions (WIN32_QueueAsyncTask/
 * WIN32_SendNetworkData; confirmed via Ghidra decompile), not this
 * family at all. Removed rather than fixed, since there is nothing here
 * to fix. When RESMGR_OpenResourceFile is implemented for real, it
 * should use a real resources/Win32Stream.h WIN32_Stream local (RAII
 * construction/destruction) for its outer stream, matching
 * game/TrainStation.cpp/game/ScriptedObject.cpp/ui/HelpWnd.cpp/
 * ui/CursorEditWindow.cpp/ui/UIPANEL_Surface.cpp's real addresses
 * (WIN32_StreamOpen 0x463890, WIN32_StreamOpenPath 0x463AA0,
 * WIN32_StreamRead 0x463810, WIN32_StreamDestroyImmediate 0x463B10) —
 * WIN32_StreamDestroy (0x463A80) no longer exists as a callable symbol
 * at all (see Win32Stream.h's doc comment on that address). */

void* CRT_FindFirstFile(const char* path, void* findData); /* @ 0x00468310 */
int32_t CRT_FindNextFile(void* handle, void* findData);    /* @ 0x00468350 */
void CRT_FindClose(void* handle);                          /* @ 0x00468370 */

/* SoundObject uses the canonical TrackPiece C++ base. */

extern int32_t g_game_mode;                          /* @ 0x004851F4 */
/* g_config_ini declared in shared/types.h */
extern int32_t g_demo_mode;                          /* @ 0x004A9918 */
/* g_asset_mgr was extern-declared here as `int32_t` at a bogus address
 * (0x004A9EEC, unrelated to the real global at 0x485600) and never used
 * anywhere in this file — a stray, unused, wrong-type/wrong-address
 * redeclaration. Removed rather than corrected in place; see
 * resources/AssetArchive.h for the canonical declaration. */
extern char g_install_path[];                        /* @ 0x004A99C8 */

/* Global sound cache — separate from ResourceManager, shared by PlaySound/PlaySoundAt */
extern int32_t g_sound_cache[]; /* @ 0x0049161C, indexed by sound ID (0x5000-0x605F) */

/* Vehicle slot globals — updated by vehicle placement code */
extern int32_t g_vehicle_slots[3]; /* @ 0x004A98B8, 3 vehicle pointers */

/* Win32 GDI */
extern "C" {
void* __stdcall CreateSolidBrush(int color);
int   __stdcall DestroyWindow(void* hwnd);
int   __stdcall FreeLibrary(void* hLib);
} /* extern "C" */

/* ================================================================== */
/* Global instances                                                     */
/* ================================================================== */

ResourceManager g_resmgr;       /* @ 0x004855E8 */
ScreenSaverModule g_scrsaver_mod;  /* @ 0x004A9910 */

/* ================================================================== */
/* ScreenSaverModule::FilterMessage                                     */
/* Address: 0x4484A0 (Ghidra: FUN_004484a0)                             */
/*                                                                      */
/* Only ever consulted by CGWND::WndProc (0x4618C0, core/CGWND.cpp)     */
/* when g_demo_mode == 1; see that method's own doc comment.            */
/* ================================================================== */
namespace {

extern "C" {
    void*   __stdcall LoadCursorA(void* hInstance, const char* lpCursorName);
    void    __stdcall SetCursor(void* hCursor);
    int32_t __stdcall ShowCursor(int32_t bShow);
    void    __stdcall OutputDebugStringA(const char* lpOutputString);
}

/* Shared "verify password (if configured), then close" tail used by
 * every trigger message below. Posts WM_CLOSE to the main window on
 * success; on a rejected password, resets the guard so a later attempt
 * can retry. The original checks `this+4` (closing_flag) again after
 * this runs before deciding what to return, but nothing can change it
 * between the two checks in this single-threaded call — the second
 * check is always false when the first was, so it is not repeated
 * here (CLAUDE.md: simplify proven-redundant assembly-shaped checks). */
void close_screensaver_if_verified(ScreenSaverModule& mod, HWND main_hwnd)
{
    mod.closing_flag = 1;

    bool verified = true;
    if (mod.get_password_status != nullptr) {
        const int status = mod.get_password_status(1);
        if ((status & 1) == 1) {
            const int result = mod.verify_screen_save_pwd(main_hwnd);
            verified = (result != 0);
        }
    }

    if (!verified) {
        mod.closing_flag = 0;
        mod.idle_move_count = 0;
        return;
    }

    PostMessageA(main_hwnd, 0x10 /* WM_CLOSE */, 0, 0);
    mod.idle_move_count = 0;
}

} // namespace

int ScreenSaverModule::FilterMessage(UINT msg, WPARAM wParam)
{
    extern void* g_main_window;  /* 0x4AA4A0 — CGWND* (stored as void* project-
                                   * wide per shared/stubs_impl.cpp's own
                                   * definition; every other call site casts
                                   * to CGWND* rather than indexing offsets). */
    CGWND* main_wnd = static_cast<CGWND*>(g_main_window);
    HWND main_hwnd = (main_wnd != nullptr) ? main_wnd->hWnd : nullptr;

    switch (msg) {
    case 0x1C:  /* WM_ACTIVATEAPP — only the deactivate transition matters */
        if (wParam != 0) return 0;
        if (this->closing_flag != 0) return 2;
        close_screensaver_if_verified(*this, main_hwnd);
        return 2;

    case 0x20: {  /* WM_SETCURSOR — force the arrow cursor visible */
        /* ABI_BOUNDARY: MAKEINTRESOURCE(IDC_ARROW) — Win32's standard-
         * cursor-ID-as-pointer convention, not a modeled game object. */
        void* cursor = LoadCursorA(
            nullptr, reinterpret_cast<const char*>(static_cast<uintptr_t>(0x7F00)));
        SetCursor(cursor);
        int32_t count = ShowCursor(1);
        while (count < 0) { count = ShowCursor(1); }
        return 1;
    }

    case 0x100:  /* WM_KEYDOWN */
    case 0x201:  /* WM_LBUTTONDOWN */
    case 0x204:  /* WM_RBUTTONDOWN */
    case 0x207:  /* WM_MBUTTONDOWN */
    case 0x104:  /* WM_SYSKEYDOWN */
    case 0x105:  /* WM_SYSKEYUP */
    case 0x106:  /* WM_SYSCHAR */
        if (this->closing_flag != 0) return 3;
        close_screensaver_if_verified(*this, main_hwnd);
        return 3;

    case 0x112:  /* WM_SYSCOMMAND */
        if ((wParam & 0xFFF0) == 0xF060) {  /* SC_MINIMIZE */
            OutputDebugStringA("ScreenSaver: SC_MINIMIZE intercepted\n");
            return 3;
        }
        if ((wParam & 0xFFF0) == 0xF130) {  /* SC_TASKLIST */
            OutputDebugStringA("ScreenSaver: SC_TASKLIST intercepted\n");
            return 3;
        }
        return 2;

    case 0x200:  /* WM_MOUSEMOVE — debounced: only closes after 3 moves */
        if (this->closing_flag != 0) return 1;
        ++this->idle_move_count;
        if (this->idle_move_count > 2) {
            close_screensaver_if_verified(*this, main_hwnd);
        }
        return 1;

    default:
        return 0;
    }
}

/* ================================================================== */
/* Constants                                                           */
/* ================================================================== */

/* Resource ID to type mask: type = (id >> 10) & 0xF */
static const int RESOURCE_TYPE_SHIFT = 10;
static const int RESOURCE_TYPE_MASK  = 0x0F;

/* Array sizes */
static const int RESOURCE_ARRAY_SIZE   = 0x4001;  /* 16385 entries for main registry */
static const int STRING_CACHE_SIZE     = 0x1061;  /* 4193 entries for string cache */

/* String ID range for GetStringById */
static const int STRING_ID_MIN = 0x5000;
static const int STRING_ID_MAX = 0x605F;

/* Clock sprite resource IDs */
static const int CLOCK_RES_BG        = 0x842;   /* background resource */
static const int CLOCK_HOUR_HAND     = 0x3DAD;  /* hour hand panel-strip sprite */
static const int CLOCK_HOUR_BG       = 0x3DAE;  /* hour hand background */
static const int CLOCK_MINUTE_HAND   = 0x3DB0;  /* minute hand panel-strip sprite */
static const int CLOCK_MINUTE_BG     = 0x3DB1;  /* minute hand background */
static const int CLOCK_RES_MINUTE    = 0x843;   /* minute hand resource (checked for +0x10) */

/* Clock sound resource IDs */
static const int SOUND_HOUR          = 0x53AB;  /* chime on hour (segment 0) */
static const int SOUND_QUARTER       = 0x5399;  /* tick on quarter-hour (segments 3,6,9) */

/* Clock render offsets (from viewport origin) */
static const int CLOCK_HOUR_OFFSET_X = 0x0F;
static const int CLOCK_HOUR_OFFSET_Y = 0x18;
static const int CLOCK_MINUTE_OFFSET_X = 0x1F;
static const int CLOCK_MINUTE_OFFSET_Y = 0x2A;

/* Clock segments (12 segments per clock face) */
static const int CLOCK_SEGMENTS = 12;

/* Screensaver sound file format */
static const char SND_MUSIC_FORMAT[] = "%s\\video\\music.wav";

namespace {
int32_t handle_from_pointer(const void* pointer)
{
    return static_cast<int32_t>(reinterpret_cast<uintptr_t>(pointer));
}

template <typename T>
T* pointer_from_handle(int32_t handle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(static_cast<uint32_t>(handle)));
}

template <typename T>
T& field_at(void* object, size_t offset)
{
    return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(object) + offset);
}

void destroy_resource(int32_t handle)
{
    /* Original vtable[0] scalar deleting destructor with flag 1 -- ordinary
     * `delete` reproduces this exactly; see resources/ResourceObject.h. */
    delete static_cast<ResourceObject*>(pointer_from_handle<void>(handle));
}

int32_t create_string_resource(int32_t resource_id, char* string_data)
{
    void* allocation = operator_new(300);
    if (allocation == nullptr) {
        return 0;
    }

    ResourceEntry* entry = RESMGR_AllocResourceEntry(
        static_cast<ResourceEntry*>(allocation), resource_id,
        handle_from_pointer(string_data));
    int32_t handle = handle_from_pointer(entry);
    if (entry != nullptr && entry->is_valid == 0) {
        destroy_resource(handle);
        return -1;
    }
    return handle;
}
}

/* ================================================================== */
/* Helper: compute language-specific string table ID                   */
/*                                                                     */
/* For resource IDs 100-500, applies a language-dependent offset       */
/* to account for localized string table blocks in the EXE.            */
/* ================================================================== */

static uint32_t apply_language_offset(int32_t languageId, uint32_t param)
{
    if (param < 100 || param > 500) {
        return param;
    }

    switch (languageId) {
    case 0:  return param;               /* English (default) */
    case 1:  return param + 0x6CFC;      /* French */
    case 2:  return param + 0x652C;      /* German */
    case 3:  return param;               /* default/unknown */
    case 4:  return param + 0x6338;      /* Spanish */
    case 5:  return param + 0x6144;      /* Italian */
    case 6:  return param + 0x6914;      /* Dutch */
    case 7:  return param + 0x6720;      /* Portuguese */
    case 8:  return param + 0x6EF0;      /* Swedish */
    case 9:  return param + 0x6B08;      /* Danish */
    default: return param;               /* unknown language, no offset */
    }
}

/* ================================================================== */
/* Language name strings (stored at fixed addresses in .rdata)         */
/* ================================================================== */

static const char* get_language_name(int32_t langId)
{
    switch (langId) {
    case 0:  return "FRENCH";
    case 1:  return "DUTCH";
    case 2:  return "ENGLISH";
    case 3:  return "SPANISH";
    case 4:  return "ITALIAN";
    case 5:  return "NORWEGIAN";
    case 6:  return "PORTUGUESE";
    case 7:  return "SWEDISH";
    default: return "";
    }
}

/* ================================================================== */
/* ResourceManager::Init                                               */
/* Address: 0x446050                                                   */
/* ================================================================== */

bool ResourceManager::Init()
{
    char resFilePath[260];
    char fontName[52];

    /* Step 1: Verify DirectDraw surface is available */
    if (!DDRAW_GetSurface()) {
        return false;
    }

    /* Step 2: Read the resource file path from config */
    Config_GetIniString(
        g_config_ini,
        "DIRECTORIES",      /* section */
        "ResFile",          /* key */
        "",                 /* default (empty) */
        resFilePath,
        0x104               /* max length */
    );

    /* Step 3: Load the resource file archive */
    DDRAW_LoadFile(&this->file_data_handle /* +0x18 */, resFilePath);
    if (this->file_data_handle == 0) {
        return false;
    }

    /* Step 4: Build font face name "Arial" (strlen + copy) */
    {
        const char* faceName = "Arial";
        int32_t i;
        for (i = 0; faceName[i] != '\0' &&
             i < static_cast<int32_t>(sizeof(fontName)) - 1; i++) {
            fontName[i] = faceName[i];
        }
        fontName[i] = '\0';
    }

    /* Create 5 GDI fonts with varying sizes and weights */
    this->font_small = CreateFontA(
        12, 0, 0, 0, 800, 0, 0, 0, 1, 0, 0, 2, 0, fontName
    );

    this->font_medium = CreateFontA(
        14, 0, 0, 0, 700, 0, 0, 0, 1, 0, 0, 2, 0, fontName
    );

    this->font_title = CreateFontA(
        16, 0, 0, 0, 700, 0, 0, 0, 1, 0, 0, 2, 0, fontName
    );

    this->font_large = CreateFontA(
        24, 0, 0, 0, 700, 0, 0, 0, 1, 0, 0, 2, 0, fontName
    );

    this->font_clock = CreateFontA(
        20, 0, 0, 0, 900, 0, 0, 0, 1, 0, 0, 2, 0, fontName
    );

    /* Step 5: Initialize keyboard and mouse input subsystems.
     * Original (ResourceManager_Init 0x4461B4/0x4461C1):
     *   mov ecx,0x4A99B0; call 0x41F7E0   (INPUT_SetKeyboard —
     *                                      [EasterEggs] loader)
     *   mov ecx,0x4A99B0; call 0x41F970   (INPUT_SetMouse — easter-egg
     *                                      record / season date)
     * The 0x4A99B0 event-list object is NOT reconstructed yet; the host
     * path is an explicit guarded adapter — it logs loudly instead of
     * silently no-op'ing — and the original path is preserved under
     * _WIN32. */
#ifndef _WIN32
    std::fprintf(stderr,
        "[HOST] ResourceManager_Init: INPUT_SetKeyboard/INPUT_SetMouse "
        "(0x41F7E0/0x41F970) deferred: 0x4A99B0 event-list object not "
        "reconstructed\n");
    std::fflush(stderr);
#else
    extern uint8_t g_input_events[];   /* 0x4A99B0 — event-list object */
    extern void INPUT_SetKeyboard(void* self);  /* 0x41F7E0 */
    extern void INPUT_SetMouse(void* self);     /* 0x41F970 */
    INPUT_SetKeyboard(&g_input_events);
    INPUT_SetMouse(&g_input_events);
#endif

    /* Step 6: Record start time (stack value, not further used) */
    int32_t startTime;
    CRT_timeGetTime(&startTime);

    /* Step 7: Load the full string table for the entire string ID range */
    this->LoadStringTable(STRING_ID_MIN, STRING_ID_MAX);

    /* Step 8: Pre-load string resources for IDs 0x400 through 0x3FFF */
    {
        int32_t* slotPtr = this->resource_ptrs + 0x400;
        int32_t id = 0x400;

        while (id < 0x4001 && g_game_mode != 10) {
            char stringBuf[264];
            uint32_t adjustedId = apply_language_offset(this->language_id, id);

            /* Load the string from EXE string table */
            void* hInstance = GetModuleHandleA(nullptr);
            int32_t result = LoadStringA(hInstance, adjustedId, stringBuf, 0x104);

            if (adjustedId == static_cast<uint32_t>(id)) {
                /* No language offset applied */
                if (result != 0) {
                    this->AddString(id, stringBuf);
                } else {
                    *slotPtr = -1;  /* mark as not found */
                }
            } else {
                /* Language offset applied */
                if (result == 0) {
                    /* Try original ID if translated fails */
                    hInstance = GetModuleHandleA(nullptr);
                    result = LoadStringA(hInstance, id, stringBuf, 0x104);
                    if (result == 0) {
                        *slotPtr = -1;
                    } else {
                        this->AddString(id, stringBuf);
                    }
                } else {
                    this->AddString(id, stringBuf);
                }
            }

            id++;
            slotPtr++;
        }
    }

    /* Step 9: Initialize clock hand segment to invalid state */
    this->clock_hand_segment = -1;

    return true;
}

/* ================================================================== */
/* ResourceManager::InitData                                           */
/* Address: 0x4463C0                                                   */
/*                                                                     */
/* Detects game language from config INI or system locale.             */
/* Called from WinMain with ECX = &g_resmgr.                          */
/* ================================================================== */

void ResourceManager::InitData()
{
    char langBuf[1024];  /* 0x400 bytes */

    if (g_config_ini != nullptr) {
        /* Read [Locale] Language= key from config */
        Config_GetIniString(g_config_ini, "Locale", "Language", "",
                            langBuf, static_cast<int32_t>(sizeof(langBuf)));

        /* Uppercase for comparison */
        CRT_strupr(langBuf);

        /* Compare against known language names */
        if (strcmp(langBuf, "FRENCH") == 0) {
            this->language_id = 1;  /* French */
            return;
        }
        if (strcmp(langBuf, "DUTCH") == 0) {
            this->language_id = 2;  /* Dutch */
            return;
        }
        if (strcmp(langBuf, "ENGLISH") == 0) {
            this->language_id = 3;  /* English (stored as 3 internally) */
            return;
        }
        if (strcmp(langBuf, "SPANISH") == 0) {
            this->language_id = 4;  /* Spanish */
            return;
        }
        if (strcmp(langBuf, "ITALIAN") == 0) {
            this->language_id = 5;  /* Italian */
            return;
        }
        if (strcmp(langBuf, "NORWEGIAN") == 0) {
            this->language_id = 6;  /* Norwegian */
            return;
        }
        if (strcmp(langBuf, "PORTUGUESE") == 0) {
            this->language_id = 7;  /* Portuguese */
            return;
        }
        if (strcmp(langBuf, "SWEDISH") == 0) {
            this->language_id = 8;  /* Swedish */
            return;
        }
    }

    /* No config or unknown language name — fall back to system locale */
    {
        uint32_t lcid = GetSystemDefaultLCID();
        uint32_t langId = lcid & 0x3FF;  /* primary language ID */

        switch (langId) {
        case 0x03:  /* Norwegian */
        case 0x0A:  /* Norwegian (Nynorsk) */
            this->language_id = 8;  /* Swedish — reused for Norwegian */
            return;
        case 0x06:  /* Danish */
            this->language_id = 1;  /* French — BUG: misalignment in original? */
            return;
        case 0x07:  /* German */
            this->language_id = 5;  /* Italian */
            return;
        case 0x0C:  /* French */
            this->language_id = 4;  /* Spanish */
            return;
        case 0x10:  /* Italian */
            this->language_id = 6;  /* Dutch */
            return;
        case 0x13:  /* Dutch */
            this->language_id = 2;  /* German */
            return;
        case 0x14:  /* Norwegian (Bokmal) */
            this->language_id = 7;  /* Portuguese */
            return;
        case 0x1D:  /* Swedish */
            this->language_id = 9;  /* Danish */
            return;
        default:
            /* Fallthrough to default */
            break;
        }
    }

    /* Default: language_id = 3 (English/internal default) */
    this->language_id = 3;
}

/* ================================================================== */
/* ResourceManager::Shutdown                                           */
/* Address: 0x446340                                                   */
/* ================================================================== */

int32_t ResourceManager::Shutdown()
{
    /* Step 1: Stop all audio */
    if (g_audio != nullptr) {
        g_audio->StopAll();
    }

    /* Step 2: Free all resources */
    this->FreeAllResources();

    /* Step 3-4: Release DDRAW surfaces and audio */
    DDRAW_ReleaseSurfaces();
    DDRAW_DestroyAudio();

    /* Step 5: Delete GDI font objects */
    if (this->font_small != nullptr) {
        DeleteObject(this->font_small);
        this->font_small = nullptr;
    }
    if (this->font_medium != nullptr) {
        DeleteObject(this->font_medium);
        this->font_medium = nullptr;
    }
    if (this->font_title != nullptr) {
        DeleteObject(this->font_title);
        this->font_title = nullptr;
    }
    if (this->font_large != nullptr) {
        DeleteObject(this->font_large);
        this->font_large = nullptr;
    }
    if (this->font_clock != nullptr) {
        DeleteObject(this->font_clock);
        this->font_clock = nullptr;
    }

    return 1;
}

/* ================================================================== */
/* ResourceManager::FreeAllResources                                   */
/* Address: 0x4467E0                                                   */
/* ================================================================== */

void ResourceManager::FreeAllResources()
{
    /* Array 1: Main resource registry at +0x10030 */
    int32_t* slotPtr = this->resource_ptrs;
    int32_t count = RESOURCE_ARRAY_SIZE;  /* 0x4001 */

    do {
        if (*slotPtr == -1) {
            *slotPtr = 0;  /* reset sentinel */
        }
        if (*slotPtr != 0) {
            /* Invoke the common typed resource destruction slot. */
            destroy_resource(*slotPtr);
            *slotPtr = 0;
        }
        /* Clear corresponding type_idx entry */
        int32_t idx = static_cast<int32_t>(slotPtr - this->resource_ptrs);
        (this->resource_type_idx)[idx] = 0;

        slotPtr++;
        count--;
    } while (count != 0);

    /* Array 2: String cache at +0x20034 */
    slotPtr = this->string_cache;
    count = STRING_CACHE_SIZE;  /* 0x1061 */

    do {
        if (*slotPtr == -1) {
            *slotPtr = 0;  /* reset sentinel */
        }
        if (*slotPtr != 0) {
            /* Invoke the common typed resource destruction slot. */
            destroy_resource(*slotPtr);
            *slotPtr = 0;
        }

        slotPtr++;
        count--;
    } while (count != 0);
}

/* ================================================================== */
/* ResourceManager::AddString                                          */
/* Address: 0x446840                                                   */
/* ================================================================== */

uint8_t ResourceManager::AddString(int32_t resId, const char* name)
{
    int32_t existing = this->resource_ptrs[resId];

    /* If already loaded or sentinel, return 1 */
    if (existing != 0 && existing != -1) {
        return 1;
    }

    /* Extract resource type from ID */
    int32_t rawType = resId >> RESOURCE_TYPE_SHIFT;  /* SAR 10 */
    uint8_t typeBits = static_cast<uint8_t>(rawType & 0xFF);
    uint8_t resourceType = (typeBits < 0x10) ? typeBits : 0;  /* clamp to 0-15 */

    void* newObj = nullptr;
    void* result = nullptr;

    if (resourceType > 0xE) {
        /* Fallthrough for type > 14 */
        newObj = operator_new(sizeof(ChildWindow));
        if (newObj != nullptr) {
            result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
        }
    } else {
        switch (resourceType) {
        case 0:
            if ((resId & 1) == 0) {
                newObj = operator_new(sizeof(ChildWindow));
                if (newObj != nullptr) {
                    result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
                }
            } else {
                newObj = operator_new(sizeof(BuildingDescriptorEditor));
                if (newObj != nullptr) {
                    result = BuildingDescriptorEditor_Ctor(newObj, resId, name);
                }
            }
            break;

        case 1:
            newObj = operator_new(sizeof(ChildWindow));
            if (newObj != nullptr) {
                result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
            }
            break;

        case 2:
        case 4:
            if ((resId & 1) == 0) {
                newObj = operator_new(sizeof(ChildWindow));
                if (newObj != nullptr) {
                    result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
                }
            } else {
                newObj = operator_new(sizeof(BuildingDescriptorEditor));
                if (newObj != nullptr) {
                    result = BuildingDescriptorEditor_Ctor(newObj, resId, name);
                }
            }
            break;

        case 3:
            if ((resId & 1) == 0) {
                newObj = operator_new(sizeof(ChildWindow));
                if (newObj != nullptr) {
                    result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
                }
            } else {
                /* 0x63C was the original x86 sizeof of TrackTileDescriptor
                 * (input/TrackTileDescriptor.h — a BuildingDescriptorEditor
                 * subclass, 0x630 base + 0xC own fields); now decompiled and
                 * reconstructed, so this allocates the real host sizeof()
                 * instead of the stale x86 literal, matching the sibling
                 * BuildingDescriptorEditor/TrainStation branches above. */
                newObj = operator_new(sizeof(TrackTileDescriptor));
                if (newObj != nullptr) {
                    result = TrackTileDescriptor_Ctor(newObj, resId, name);
                }
            }
            break;

        case 5:
            /* Type 5: ChildWindow with persistence enabled */
            newObj = operator_new(sizeof(ChildWindow));
            if (newObj != nullptr) {
                result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
            }
            this->resource_ptrs[resId] = handle_from_pointer(result);
            if (result != nullptr) {
                field_at<uint8_t>(result, 0x162) = 1;  /* persistent flag */
            }
            return 1;

        case 6:
            if (resId == 0x1802 || (resId >= 0x1866 && (resId & 1) == 1)) {
                newObj = operator_new(sizeof(CursorEditWindow));
                if (newObj != nullptr) {
                    result = CursorEditWindow_Ctor(newObj, resId, name);
                }
            } else {
                newObj = operator_new(sizeof(ChildWindow));
                if (newObj != nullptr) {
                    result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
                }
            }
            break;

        case 7:
        case 8:
            if ((resId & 1) == 0) {
                newObj = operator_new(sizeof(TrainStation));
                if (newObj != nullptr) {
                    result = TrainStation_Ctor(newObj, resId, name);
                }
            } else {
                newObj = operator_new(sizeof(ChildWindow));
                if (newObj != nullptr) {
                    result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
                }
            }
            break;

        case 9:
        case 10:
        case 11:
            newObj = operator_new(sizeof(ChildWindow));
            if (newObj != nullptr) {
                result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
            }
            break;

        case 12:
        case 13:
            newObj = operator_new(sizeof(BuildingDescriptorEditor));
            if (newObj != nullptr) {
                result = BuildingDescriptorEditor_Ctor(newObj, resId, name);
            }
            break;

        case 14:
            newObj = operator_new(sizeof(ChildWindow));
            if (newObj != nullptr) {
                result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
            }
            this->resource_ptrs[resId] = handle_from_pointer(result);
            if (resId > 0x3801 && result != nullptr) {
                field_at<uint8_t>(result, 0x162) = 1;
            }
            return 1;

        default:
            /* Fallthrough for types not explicitly handled */
            newObj = operator_new(sizeof(ChildWindow));
            if (newObj != nullptr) {
                result = new (newObj) ChildWindow(static_cast<uint32_t>(resId), name);
            }
            break;
        }
    }

    /* Store the result in the resource registry */
    this->resource_ptrs[resId] = handle_from_pointer(result);

    /* Check persistence: type 1 and 15 are always persistent */
    rawType = resId >> RESOURCE_TYPE_SHIFT;
    uint8_t finalType = static_cast<uint8_t>(rawType);
    if (finalType >= 0x10) {
        finalType = 0;
    }

    bool isPersistent = (finalType == 1 || finalType == 0xF);
    if (!isPersistent && result != nullptr) {
        /* Check persistent flag at +0x162 in ChildWindow */
        isPersistent = (field_at<uint8_t>(result, 0x162) == 1);
    }

    if (isPersistent) {
        return 1;
    }

    /* Not persistent — destroy the resource */
    int32_t stored = this->resource_ptrs[resId];
    if (stored != 0) {
        destroy_resource(stored);
    }
    this->resource_ptrs[resId] = -1;

    return 0;
}

/* ================================================================== */
/* ResourceManager::LoadStringTable                                    */
/* Address: 0x446CC0                                                   */
/* ================================================================== */

BOOL ResourceManager::LoadStringTable(UINT startId, int32_t endId)
{
    /* Clamp endId to max string table entry */
    if (endId > STRING_ID_MAX) {
        endId = STRING_ID_MAX;
    }

    int32_t id = static_cast<int32_t>(startId);

    if (id > endId) {
        return (id == endId + 1) ? 1 : 0;
    }

    while (id <= endId) {
        /* Early exit if in slideshow/screensaver mode */
        if (g_game_mode == 10) {
            break;
        }

        char stringBuf[264];
        uint32_t adjustedId;

        /* Apply language offset for IDs in 100-500 range */
        if (id < 100 || id > 500) {
            adjustedId = static_cast<uint32_t>(id);
        } else {
            adjustedId = apply_language_offset(this->language_id, id);
        }

        /* Load string from EXE */
        void* hInstance = GetModuleHandleA(nullptr);
        int32_t result = LoadStringA(hInstance, adjustedId, stringBuf, 0x104);

        /* The original stores the loaded string resource at
         * this + 0xC034 + id*4 (0x446E02/0x446E2C); the C++ model's
         * string_cache sits at +0x20034 = 0xC034 + 0x5000*4, so the
         * index is (id - STRING_ID_MIN).  (The old string_cache[id]
         * indexing wrote out of bounds for every sound ID.) */
        const int32_t cache_index = id - STRING_ID_MIN;

        if (adjustedId == static_cast<uint32_t>(id)) {
            /* No language offset — direct load */
            if (result == 0) {
                this->string_cache[cache_index] = -1;
            } else {
                this->string_cache[cache_index] =
                    create_string_resource(id, stringBuf);
            }
        } else {
            /* Language offset applied */
            if (result == 0) {
                /* Fall back to original ID */
                hInstance = GetModuleHandleA(nullptr);
                result = LoadStringA(hInstance, static_cast<UINT>(id), stringBuf, 0x104);

                if (result == 0) {
                    this->string_cache[cache_index] = -1;
                } else {
                    this->string_cache[cache_index] =
                        create_string_resource(id, stringBuf);
                }
            } else {
                /* Translation loaded successfully */
                this->string_cache[cache_index] =
                    create_string_resource(id, stringBuf);
            }
        }

        id++;
    }

    return (id == endId + 1) ? 1 : 0;
}

/* ================================================================== */
/* ResourceManager::GetById                                            */
/* Address: 0x446EA0                                                   */
/* ================================================================== */

int32_t ResourceManager::GetById(int32_t resId)
{
    /* Validate ID range */
    if (resId < 0 || resId > 0x3FFF) {
        *CRT_errno() = 1;  /* EINVAL */
        return 0;
    }

    /* Read from type-index array at +0x2C */
    int32_t* typeEntry = &this->resource_type_idx[resId];
    if (typeEntry == nullptr) {
        return 0;
    }

    int32_t resourcePtr = *typeEntry;

    if (resourcePtr == 0) {
        /* Lazy-load: compute resource index and load */
        int32_t idx = typeEntry - this->resource_ptrs;               /* pointer diff = array index */
        int32_t endIdx = idx;
        if (endIdx > 0x3FFF) {
            endIdx = 0x4000;
        }

        if (idx <= endIdx) {
            int32_t* slotPtr = &this->resource_ptrs[idx];

            while (idx <= endIdx && g_game_mode != 10) {
                char stringBuf[264];
                uint32_t adjustedId = apply_language_offset(
                    this->language_id, static_cast<uint32_t>(idx));

                void* hInstance = GetModuleHandleA(nullptr);
                int32_t result = LoadStringA(hInstance, adjustedId, stringBuf, 0x104);

                if (adjustedId == static_cast<uint32_t>(idx)) {
                    if (result != 0) {
                        this->AddString(idx, stringBuf);
                    } else {
                        *slotPtr = -1;
                    }
                } else {
                    if (result == 0) {
                        hInstance = GetModuleHandleA(nullptr);
                        result = LoadStringA(hInstance, static_cast<UINT>(idx), stringBuf, 0x104);
                        if (result == 0) {
                            *slotPtr = -1;
                        } else {
                            this->AddString(idx, stringBuf);
                        }
                    } else {
                        this->AddString(idx, stringBuf);
                    }
                }

                idx++;
                slotPtr++;
            }
        }

        /* Re-read after loading */
        typeEntry = &this->resource_type_idx[resId];
        if (typeEntry != nullptr) {
            resourcePtr = *typeEntry;
        }

        if (resourcePtr == 0) {
            *typeEntry = -1;
            *CRT_errno() = 2;  /* ENOENT */
        }
    }

    if (resourcePtr == -1) {
        *CRT_errno() = 2;  /* ENOENT */
        return 0;
    }

    return resourcePtr;
}

/* ================================================================== */
/* ResourceManager::GetStringById                                      */
/* Address: 0x4472B0                                                   */
/* ================================================================== */

int32_t ResourceManager::GetStringById(UINT stringId)
{
    /* Validate string ID range */
    if (stringId < STRING_ID_MIN || stringId > STRING_ID_MAX) {
        *CRT_errno() = 1;  /* EINVAL */
        return 0;
    }

    /* Read from string cache (inline offset: +0xC034) */
    int32_t* cacheEntry = &this->string_cache[stringId - 0x5000];
    int32_t value = *cacheEntry;

    if (value == 0) {
        /* Lazy-load this single ID */
        this->LoadStringTable(stringId, static_cast<int32_t>(stringId));

        /* Re-read after loading */
        value = *cacheEntry;
        if (value == 0) {
            *cacheEntry = -1;
            *CRT_errno() = 2;  /* ENOENT */
        }
    }

    if (value == -1) {
        *CRT_errno() = 2;  /* ENOENT */
        return 0;
    }

    return value;
}

/* ================================================================== */
/* ResourceManager::LoadStringToResource                                */
/* Address: 0x4470B0                                                   */
/*                                                                     */
/* Loads a single string resource by ID into the main resource array.  */
/* Returns the slot value, or 0 with errno=2 if not found.            */
/* ================================================================== */

int32_t ResourceManager::LoadStringToResource(UINT resId)
{
    /* Validate ID range */
    if (static_cast<int32_t>(resId) < 0 || resId > 0x3FFF) {
        *CRT_errno() = 1;
        return 0;
    }

    int32_t* slotPtr = &this->resource_ptrs[resId];
    int32_t value = *slotPtr;

    if (value == 0) {
        uint32_t curId = resId;

        /* Load the string (range: curId..resId, effectively 1 iteration) */
        do {
            if (g_game_mode == 10) break;

            uint32_t adjustedId;
            if (curId < 100 || curId > 500) {
                adjustedId = curId;
            } else {
                adjustedId = apply_language_offset(this->language_id, curId);
            }

            void* hInstance = GetModuleHandleA(nullptr);
            char stringBuf[264];
            int32_t result = LoadStringA(hInstance, adjustedId, stringBuf, 0x104);

            if (adjustedId == curId) {
                if (result != 0) {
                    this->AddString(static_cast<int32_t>(curId), stringBuf);
                } else {
                    this->resource_ptrs[curId] = -1;
                }
            } else {
                if (result == 0) {
                    hInstance = GetModuleHandleA(nullptr);
                    result = LoadStringA(hInstance, static_cast<UINT>(curId), stringBuf, 0x104);
                    if (result == 0) {
                        this->resource_ptrs[curId] = -1;
                    } else {
                        this->AddString(static_cast<int32_t>(curId), stringBuf);
                    }
                } else {
                    this->AddString(static_cast<int32_t>(curId), stringBuf);
                }
            }

            curId++;
            slotPtr++;
        } while (static_cast<int32_t>(curId) <= static_cast<int32_t>(resId));

        /* Re-read result */
        value = this->resource_ptrs[resId];
        if (value == 0) {
            this->resource_ptrs[resId] = -1;
            *CRT_errno() = 2;
        }
    }

    if (value == -1) {
        *CRT_errno() = 2;
        return 0;
    }

    return value;
}

/* ================================================================== */
/* ResourceManager::RegisterDependency                                 */
/* Address: 0x447290                                                   */
/*                                                                     */
/* Writes a pointer to resource slot at +0x10030[resIndex] into the   */
/* type-index array at +0x2C[depIndex]. Creates a weak reference from */
/* one slot to another. Called by INPUT_SetMouse (0x41F970).          */
/* ================================================================== */

void ResourceManager::RegisterDependency(int32_t depIndex, int32_t resIndex)
{
    /* Write the address of the resource slot into the type-index array */
    this->resource_type_idx[depIndex] =
        handle_from_pointer(&this->resource_ptrs[resIndex]);
}

/* ================================================================== */
/* ResourceManager::AnimateClock                                       */
/* Address: 0x447400                                                   */
/* ================================================================== */

void ResourceManager::AnimateClock(int32_t timestamp)
{
    int32_t bgResource = this->GetById(CLOCK_RES_BG);  /* 0x842 */
    if (bgResource == 0) {
        return;
    }

    int32_t* bgData = pointer_from_handle<int32_t>(bgResource);
    if (bgData[4] == 0) {  /* +0x10 */
        return;
    }

    int32_t minutes = (timestamp / 60) % 60;
    int32_t segment = (minutes / 5 + 1) % CLOCK_SEGMENTS;
    int32_t soundId = 0;

    if (segment != this->clock_hand_segment) {
        if (segment == 0) {
            this->clock_hand_segment = 0;
            int32_t* soundCache = reinterpret_cast<int32_t*>(
                static_cast<uintptr_t>(0x004A64C8));
            soundId = *soundCache;
            if (soundId == 0) {
                this->LoadStringTable(SOUND_HOUR, SOUND_HOUR);
                soundId = *soundCache;
                if (soundId == 0) {
                    *soundCache = -1;
                    *CRT_errno() = 2;
                }
            }
            if (soundId == -1) {
                *CRT_errno() = 2;
                soundId = 0;
            }
        } else if (segment == 3 || segment == 6 || segment == 9) {
            this->clock_hand_segment = segment;
            int32_t* soundCache = reinterpret_cast<int32_t*>(
                static_cast<uintptr_t>(0x004A6480));
            soundId = *soundCache;
            if (soundId == 0) {
                this->LoadStringTable(SOUND_QUARTER, SOUND_QUARTER);
                soundId = *soundCache;
                if (soundId == 0) {
                    *soundCache = -1;
                    *CRT_errno() = 2;
                }
            }
            if (soundId == -1) {
                *CRT_errno() = 2;
                soundId = 0;
            }
        } else {
            this->clock_hand_segment = segment;
        }

        if (g_audio != nullptr && soundId != 0) {
            GameAudio_AllocChannel(g_audio, soundId, nullptr,
                                    g_listener_x, g_listener_y, 4, 0);
        }
    }

    void* background_data = pointer_from_handle<void>(bgData[4]);

    int32_t hourBgResource = this->GetById(CLOCK_HOUR_BG);
    if (hourBgResource != 0) {
        void* hourBgRes = pointer_from_handle<void>(hourBgResource);
        ResourceObject* hourBg = static_cast<ResourceObject*>(hourBgRes);
        void* hourBgSurface = hourBg->Lock(0, 0);
        int32_t hourBgWidth = field_at<uint16_t>(hourBgRes, 0x14);
        int32_t hourBgHeight = field_at<uint16_t>(hourBgRes, 0x16);

        RECT srcRect;
        SetRect(&srcRect, 0, 0, hourBgWidth - 1, hourBgHeight - 1);
        RECT dstRect;
        CopyRect(&dstRect, &srcRect);
        OffsetRect(&srcRect, segment * hourBgWidth, 0);
        OffsetRect(&dstRect, CLOCK_HOUR_OFFSET_X, CLOCK_HOUR_OFFSET_Y);

        Town_CopyTiles8bpp_Transparent(
            hourBgSurface, dstRect.left, dstRect.top,
            dstRect.right, dstRect.bottom,
            field_at<uint32_t>(background_data, 0x18),
            field_at<int32_t>(background_data, 0x8),
            srcRect.left, srcRect.top, srcRect.right, srcRect.bottom);
        hourBg->Unlock();

        int32_t hourHandResource = this->GetById(CLOCK_HOUR_HAND);
        if (hourHandResource != 0) {
            void* hourHandRes = pointer_from_handle<void>(hourHandResource);
            ResourceObject* hourHand = static_cast<ResourceObject*>(hourHandRes);
            void* hourHandSurface = hourHand->Lock(0, 0);
            int32_t hourWidth = field_at<uint16_t>(hourHandRes, 0x14);
            int32_t hourHeight = field_at<uint16_t>(hourHandRes, 0x16);

            RECT srcRect2;
            SetRect(&srcRect2, 0, 0, hourWidth - 1, hourHeight - 1);
            RECT dstRect2;
            CopyRect(&dstRect2, &srcRect2);
            OffsetRect(&srcRect2, segment * hourWidth, 0);
            OffsetRect(&dstRect2, CLOCK_HOUR_OFFSET_X, CLOCK_HOUR_OFFSET_Y);

            Town_CopyTiles8bpp_Transparent(
                hourHandSurface, dstRect2.left, dstRect2.top,
                dstRect2.right, dstRect2.bottom,
                field_at<uint32_t>(background_data, 0x18),
                field_at<int32_t>(background_data, 0x8),
                srcRect2.left, srcRect2.top,
                srcRect2.right, srcRect2.bottom);
            hourHand->Unlock();
        }
    }

    int32_t minuteResource = this->GetById(CLOCK_RES_MINUTE);  /* 0x843 */
    void* minute_data = pointer_from_handle<void>(minuteResource);
    if (minuteResource != 0 && field_at<int32_t>(minute_data, 0x10) != 0) {
        int32_t minBgResource = this->GetById(CLOCK_MINUTE_BG);
        if (minBgResource != 0) {
            void* minBgRes = pointer_from_handle<void>(minBgResource);
            ResourceObject* minBg = static_cast<ResourceObject*>(minBgRes);
            void* minBgSurface = minBg->Lock(0, 0);
            int32_t minBgWidth = field_at<uint16_t>(minBgRes, 0x14);
            int32_t minBgHeight = field_at<uint16_t>(minBgRes, 0x16);

            RECT minSrcRect1;
            SetRect(&minSrcRect1, 0, 0, minBgWidth - 1, minBgHeight - 1);
            RECT minDstRect1;
            CopyRect(&minDstRect1, &minSrcRect1);
            OffsetRect(&minSrcRect1, minutes * minBgWidth, 0);
            OffsetRect(&minDstRect1, CLOCK_MINUTE_OFFSET_X, CLOCK_MINUTE_OFFSET_Y);

            Town_CopyTiles8bpp_Transparent(
                minBgSurface, minDstRect1.left, minDstRect1.top,
                minDstRect1.right, minDstRect1.bottom,
                field_at<uint32_t>(field_at<void*>(minute_data, 0x10), 0x18),
                field_at<int32_t>(field_at<void*>(minute_data, 0x10), 0x8),
                minSrcRect1.left, minSrcRect1.top,
                minSrcRect1.right, minSrcRect1.bottom);
            minBg->Unlock();

            int32_t minHandResource = this->GetById(CLOCK_MINUTE_HAND);
            if (minHandResource != 0) {
                void* minHandRes = pointer_from_handle<void>(minHandResource);
                ResourceObject* minHand = static_cast<ResourceObject*>(minHandRes);
                void* minHandSurface = minHand->Lock(0, 0);
                int32_t minWidth = field_at<uint16_t>(minHandRes, 0x14);
                int32_t minHeight = field_at<uint16_t>(minHandRes, 0x16);

                RECT minSrcRect2;
                SetRect(&minSrcRect2, 0, 0, minWidth - 1, minHeight - 1);
                RECT minDstRect2;
                CopyRect(&minDstRect2, &minSrcRect2);
                OffsetRect(&minSrcRect2, minutes * minWidth, 0);
                OffsetRect(&minDstRect2, CLOCK_MINUTE_OFFSET_X, CLOCK_MINUTE_OFFSET_Y);

                Town_CopyTiles8bpp_Transparent(
                    minHandSurface, minDstRect2.left, minDstRect2.top,
                    minDstRect2.right, minDstRect2.bottom,
                    field_at<uint32_t>(field_at<void*>(minute_data, 0x10), 0x18),
                    field_at<int32_t>(field_at<void*>(minute_data, 0x10), 0x8),
                    minSrcRect2.left, minSrcRect2.top,
                    minSrcRect2.right, minSrcRect2.bottom);
                minHand->Unlock();
            }
        }

        extern int32_t g_viewport_rect_left;
        extern int32_t g_viewport_rect_top;
        extern int32_t g_viewport_rect_right;
        extern int32_t g_viewport_rect_bottom;
        extern void* g_tilemap;
        TileMap_InvalidateRect(g_tilemap, g_viewport_rect_left,
                               g_viewport_rect_top, g_viewport_rect_right,
                               g_viewport_rect_bottom);
    }
}

/* ================================================================== */
/* Free functions — not class members                                   */
/* ================================================================== */

/* ================================================================== */
/* GetResourceType                                                     */
/* Address: 0x446030                                                   */
/* ================================================================== */

UINT __cdecl GetResourceType(UINT id)
{
    int32_t rawType = static_cast<int32_t>(id) >> RESOURCE_TYPE_SHIFT;
    uint8_t typeByte = static_cast<uint8_t>(rawType);
    return (typeByte < 0x10) ? static_cast<UINT>(typeByte) : 0;
}

/* ================================================================== */
/* PlaySound                                                           */
/* Address: 0x447930                                                   */
/* ================================================================== */

void __cdecl PlaySound(UINT soundId)
{
    if (soundId < STRING_ID_MIN || soundId > STRING_ID_MAX) {

        *CRT_errno() = 2;
        return;
    }

    int32_t soundRes;
    soundRes = g_sound_cache[soundId];

    if (soundRes == 0) {
        g_resmgr.LoadStringTable(soundId, static_cast<int32_t>(soundId));
        soundRes = g_sound_cache[soundId];

        if (soundRes == 0) {
            g_sound_cache[soundId] = -1;
            *CRT_errno() = 2;
        }
    }

    if (soundRes == -1) {
        *CRT_errno() = 2;
        soundRes = 0;
    }

check_audio:
    if (g_audio != nullptr && soundRes != 0) {
        GameAudio_AllocChannel(
            g_audio,
            soundRes,
            nullptr,
            g_listener_x,
            g_listener_y,
            4, 0
        );
    }
}

/* ================================================================== */
/* ResourceData_Dtor — compiler-generated vtable slot                  */
/* Address: 0x447B60                                                   */
/* ================================================================== */
/* The original slot is emitted by the MSVC destructor machinery. The
 * user cleanup is performed by RESMGR_RemoveResource; no free function
 * or literal vtable write is needed in the C++ reconstruction.  The
 * save/load primitives themselves live in resources/ResDataSave.cpp. */

/** SoundObject::SoundObject — compiler-managed construction body
 * Address: 0x448F30 */
SoundObject::SoundObject(int32_t text_length, void* town, RESDATA* resource,
                         void* font_handle, uint16_t flags)
    : TrackPiece(town, resource, flags),
      consume_state(0),
      _pad_59{0, 0, 0},
      max_text_len(text_length),
      text_buf(static_cast<char*>(operator_new(text_length + 1))),
      font(font_handle)
{
    /* Binary overrides TrackPiece marker (7) → 8 for SoundObject type.
     * Original: MOV dword ptr [this+4], 8 after base ctor returns. */
    this->type = 8;

    if (text_buf != nullptr) {
        strcpy(text_buf, "");
    }
}

/** SoundObject::~SoundObject — user-defined base destructor body
 * Address: 0x449000 (Ghidra: SoundObject_Dtor)
 *
 * Frees text_buf, resets the vtable pointer to VTBL_SOUND_OBJECT (compiler-
 * managed in this C++ reconstruction), then chains to TrackPiece::~TrackPiece
 * (0x40D040). This is the real destructor body, not the vtable-slot thunk —
 * see RESMGR_SoundObject_Ctor below for that distinction. */
SoundObject::~SoundObject()
{
    if (text_buf != nullptr) {
        GLOBAL_free(text_buf);
        text_buf = nullptr;
    }
}

/* ================================================================== */
/* RESMGR_SoundObject_Ctor — ABI bridge to SoundObject::SoundObject    */
/* Address: 0x448F30                                                   */
/* ================================================================== */
void* RESMGR_SoundObject_Ctor(void* self, int32_t strLen, int32_t param2,
                               int32_t param3, void* font, uint16_t param5)
{
    return ::new (self) SoundObject(
        strLen, pointer_from_handle<void>(param2),
        pointer_from_handle<RESDATA>(param3), font, param5);
}

/* The original scalar deleting destructor — vtable[0], Ghidra:
 * SoundObject_ScalarDeletingDtor, address 0x448FE0 (NOT 0x449000, which is
 * the base destructor body above) — is a 30-byte thunk that calls
 * SoundObject::~SoundObject then conditionally frees the object if flags&1.
 * Emitted automatically by the compiler; no free-function wrapper is
 * reimplemented. */

/* ================================================================== */
/* RESDATA_SoundObject_Init                                            */
/* ================================================================== */
void* __thiscall RESDATA_SoundObject_Init(void* self, const char* source)
{
    SoundObject* sound_object = static_cast<SoundObject*>(self);
    char* textBuf = sound_object->text_buf;
    int32_t maxLen = sound_object->max_text_len;
    if (textBuf && maxLen > 0 && source) {
        strncpy(textBuf, source, maxLen - 1);
        textBuf[maxLen - 1] = '\0';
    }
    return self;
}

/* ================================================================== */
/* RESDATA_SoundObject_GetState                                        */
/* ================================================================== */
void* __fastcall RESDATA_SoundObject_GetState(void* self)
{
    return static_cast<SoundObject*>(self)->text_buf;
}

/* ================================================================== */
/* RESDATA_SoundObject_GetTextLength                                   */
/* ================================================================== */
int32_t __fastcall RESDATA_SoundObject_GetTextLength(void* self)
{
    const char* textBuf = static_cast<SoundObject*>(self)->text_buf;
    return textBuf ? static_cast<int32_t>(strlen(textBuf)) : 0;
}
