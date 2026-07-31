#undef RESDATA_DEFINED
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

#include "ResourceManager.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include <stdint.h>
#include <string.h>
#include <ctype.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* C++ allocation helpers */
void* operator_new(uint32_t size);   /* operator new @ 0x00465CE0 */
void  GLOBAL_free(void* ptr);        /* @ 0x00465CD0 */

extern "C" {

#define VTBL_RESDATA ((void*)0x478274)
#define VTBL_SOUND_OBJECT ((void*)0x47827C)

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
void* CRT_ceil(void* stream, void* buf, uint32_t size);   /* @ 0x00465010 */
void* CRT_floor(void* stream, const char* path, uint32_t mode, void* param, int32_t flag); /* @ 0x00465090 */

} /* extern "C" */

/* Sound system (C++ linkage) */
struct GameAudio;
extern GameAudio* g_audio;           /* @ 0x004FD3BC */
extern int32_t g_listener_x;         /* @ 0x004AAD2C */
extern int32_t g_listener_y;         /* @ 0x004AAD30 */

void GameAudio_StopAll(GameAudio* audio);            /* @ 0x00413140 */
int32_t GameAudio_AllocChannel(                       /* @ 0x00413210 */
    GameAudio* audio, int32_t soundResource,
    void* pCallback, int32_t x, int32_t y,
    uint32_t priority, uint32_t flags);
int32_t GameAudio_Play(GameAudio* audio, void* desc, void** buffer, int32_t flag); /* @ 0x004132F0 */
uint32_t Game_LoadWaveFile(const char* path, void* waveDesc); /* @ 0x00412700 */
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

void INPUT_SetKeyboard(void* obj);                   /* @ 0x0041F7E0 */
void INPUT_SetMouse(void* obj);                      /* @ 0x0041F970 */
void* UI_CreateChildWindow(void* obj, int32_t resId, int32_t strPtr); /* @ 0x00424AF0 */
void* INPUT_ExitGame(void* obj, int32_t resId, int32_t strPtr);       /* @ 0x0041E570 */
void* TrainStation_Ctor(void* obj, int32_t resId, int32_t strPtr);    /* @ 0x00436400 */
void* CGWND_CursorEditWindow_Ctor(void* obj, int32_t resId, int32_t strPtr); /* @ 0x0040E600 */
void* RESDATA_ScriptedObject_AddChild(void* obj, int32_t resId, int32_t strPtr); /* @ 0x0044B190 */

void Town_CopyTiles8bpp_Transparent(
    void* surface, int32_t srcX, int32_t srcY,
    int32_t dstX, int32_t dstY, int32_t srcStride,
    int32_t srcPixelSize, int32_t clipLeft, int32_t clipTop,
    int32_t clipRight, int32_t clipBottom);          /* @ 0x0042C330 */

void TileMap_InvalidateRect(void* tilemap,
    int32_t left, int32_t top, int32_t right, int32_t bottom); /* @ 0x00455840 */

void UIPANEL_CreateSurface(void* surfOut);           /* @ 0x004286B0 */
uint32_t UIPANEL_LockSurface(void* surf);            /* @ 0x00428A70 */
void TileMap_CreateOverlay(void* tilemap, void* surface, int32_t flag); /* @ 0x00455CB0 */

void* AssetMgr_LoadFile(void** assetMgr, const uint8_t* name, int32_t* outSize); /* @ 0x00457C00 */
void* WNDPROC_StreamFromMemory(void* stream, char* data, int32_t size, int32_t flag); /* @ 0x00460C10 */
void* WIN32_StreamOpenFile(void* stream, const char* path, uint32_t mode, void* param, int32_t flag); /* @ 0x00461710 */
void* WIN32_StreamOpenPath(void* stream, const char* path, uint32_t mode, void* param); /* @ 0x00461640 */
void* WIN32_StreamOpen(void* stream, int32_t flag);   /* @ 0x00461600 */
void WIN32_StreamDestroyImmediate(void* stream);      /* @ 0x00461800 */
void WIN32_StreamDestroy(int32_t* streamInfo);        /* @ 0x004617C0 */
void WNDPROC_StreamCleanup(int32_t* streamInfo);      /* @ 0x00460D50 */
uint32_t WIN32_StreamRead(void* stream, void* buf, uint32_t size); /* @ 0x00461880 */

void* CRT_FindFirstFile(const char* path, void* findData); /* @ 0x00468310 */
int32_t CRT_FindNextFile(void* handle, void* findData);    /* @ 0x00468350 */
void CRT_FindClose(void* handle);                          /* @ 0x00468370 */

void TrackPiece_Ctor(void* self, int32_t param2, int32_t param3, uint16_t param5); /* @ 0x0040CF20 */
void __thiscall TrackPiece_Dtor(void* self);                     /* @ 0x0040D040 */
/* RESDATA_SoundObject_BaseDtor already declared elsewhere */

extern int32_t g_game_mode;                          /* @ 0x004851F4 */
/* g_config_ini declared in shared/types.h */
extern int32_t g_demo_mode;                          /* @ 0x004A9918 */
extern int32_t g_asset_mgr;                          /* @ 0x004A9EEC — AssetMgr instance ptr */
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
        for (i = 0; faceName[i] != '\0' && i < (int32_t)sizeof(fontName) - 1; i++) {
            fontName[i] = faceName[i];
        }
        fontName[i] = '\0';
    }

    /* Create 5 GDI fonts with varying sizes and weights */
    this->font_small = (HFONT)CreateFontA(
        12, 0, 0, 0, 800, 0, 0, 0, 1, 0, 0, 2, 0, fontName
    );

    this->font_medium = (HFONT)CreateFontA(
        14, 0, 0, 0, 700, 0, 0, 0, 1, 0, 0, 2, 0, fontName
    );

    this->font_title = (HFONT)CreateFontA(
        16, 0, 0, 0, 700, 0, 0, 0, 1, 0, 0, 2, 0, fontName
    );

    this->font_large = (HFONT)CreateFontA(
        24, 0, 0, 0, 700, 0, 0, 0, 1, 0, 0, 2, 0, fontName
    );

    this->font_clock = (HFONT)CreateFontA(
        20, 0, 0, 0, 900, 0, 0, 0, 1, 0, 0, 2, 0, fontName
    );

    /* Step 5: Initialize keyboard and mouse input subsystems */
    /* Uses global input context at 0x4A99B0 */
    void* inputContext = (void*)0x004A99B0;
    INPUT_SetKeyboard(inputContext);
    INPUT_SetMouse(inputContext);

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
            void* hInstance = GetModuleHandleA(NULL);
            int32_t result = LoadStringA(hInstance, adjustedId, stringBuf, 0x104);

            if (adjustedId == (uint32_t)id) {
                /* No language offset applied */
                if (result != 0) {
                    this->AddString(id, (int32_t)stringBuf);
                } else {
                    *slotPtr = -1;  /* mark as not found */
                }
            } else {
                /* Language offset applied */
                if (result == 0) {
                    /* Try original ID if translated fails */
                    hInstance = GetModuleHandleA(NULL);
                    result = LoadStringA(hInstance, id, stringBuf, 0x104);
                    if (result == 0) {
                        *slotPtr = -1;
                    } else {
                        this->AddString(id, (int32_t)stringBuf);
                    }
                } else {
                    this->AddString(id, (int32_t)stringBuf);
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

    if (g_config_ini != NULL) {
        /* Read [Locale] Language= key from config */
        Config_GetIniString(g_config_ini, "Locale", "Language", "",
                            langBuf, (int32_t)sizeof(langBuf));

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
    if (g_audio != NULL) {
        GameAudio_StopAll(g_audio);
    }

    /* Step 2: Free all resources */
    this->FreeAllResources();

    /* Step 3-4: Release DDRAW surfaces and audio */
    DDRAW_ReleaseSurfaces();
    DDRAW_DestroyAudio();

    /* Step 5: Delete GDI font objects */
    if (this->font_small != NULL) {
        DeleteObject(this->font_small);
        this->font_small = NULL;
    }
    if (this->font_medium != NULL) {
        DeleteObject(this->font_medium);
        this->font_medium = NULL;
    }
    if (this->font_title != NULL) {
        DeleteObject(this->font_title);
        this->font_title = NULL;
    }
    if (this->font_large != NULL) {
        DeleteObject(this->font_large);
        this->font_large = NULL;
    }
    if (this->font_clock != NULL) {
        DeleteObject(this->font_clock);
        this->font_clock = NULL;
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
            /* Call vtable[0] destructor with free-flag = 1 */
            void* resource = (void*)(uintptr_t)*slotPtr;
            void** vtable = (void**)resource;
            void (*dtor)(void*, uint8_t) = (void (*)(void*, uint8_t))vtable[0];
            dtor(resource, 1);
            *slotPtr = 0;
        }
        /* Clear corresponding type_idx entry */
        int32_t idx = (int32_t)(slotPtr - this->resource_ptrs);
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
            /* Call vtable[0] destructor with free-flag = 1 */
            void* resource = (void*)(uintptr_t)*slotPtr;
            void** vtable = (void**)resource;
            void (*dtor)(void*, uint8_t) = (void (*)(void*, uint8_t))vtable[0];
            dtor(resource, 1);
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

uint8_t ResourceManager::AddString(int32_t resId, int32_t strPtr)
{
    int32_t existing = this->resource_ptrs[resId];

    /* If already loaded or sentinel, return 1 */
    if (existing != 0 && existing != -1) {
        return 1;
    }

    /* Extract resource type from ID */
    int32_t rawType = resId >> RESOURCE_TYPE_SHIFT;  /* SAR 10 */
    uint8_t typeBits = (uint8_t)(rawType & 0xFF);
    uint8_t resourceType = (typeBits < 0x10) ? typeBits : 0;  /* clamp to 0-15 */

    void* newObj = NULL;
    void* result = NULL;

    if (resourceType > 0xE) {
        /* Fallthrough for type > 14 */
        newObj = operator_new(0x168);
        if (newObj != NULL) {
            result = UI_CreateChildWindow(newObj, resId, strPtr);
        }
    } else {
        switch (resourceType) {
        case 0:
            if ((resId & 1) == 0) {
                newObj = operator_new(0x168);
                if (newObj != NULL) {
                    result = UI_CreateChildWindow(newObj, resId, strPtr);
                }
            } else {
                newObj = operator_new(0x630);
                if (newObj != NULL) {
                    result = INPUT_ExitGame(newObj, resId, strPtr);
                }
            }
            break;

        case 1:
            newObj = operator_new(0x168);
            if (newObj != NULL) {
                result = UI_CreateChildWindow(newObj, resId, strPtr);
            }
            break;

        case 2:
        case 4:
            if ((resId & 1) == 0) {
                newObj = operator_new(0x168);
                if (newObj != NULL) {
                    result = UI_CreateChildWindow(newObj, resId, strPtr);
                }
            } else {
                newObj = operator_new(0x630);
                if (newObj != NULL) {
                    result = INPUT_ExitGame(newObj, resId, strPtr);
                }
            }
            break;

        case 3:
            if ((resId & 1) == 0) {
                newObj = operator_new(0x168);
                if (newObj != NULL) {
                    result = UI_CreateChildWindow(newObj, resId, strPtr);
                }
            } else {
                newObj = operator_new(0x63C);
                if (newObj != NULL) {
                    result = RESDATA_ScriptedObject_AddChild(newObj, resId, strPtr);
                }
            }
            break;

        case 5:
            /* Type 5: ChildWindow with persistence enabled */
            newObj = operator_new(0x168);
            if (newObj != NULL) {
                result = UI_CreateChildWindow(newObj, resId, strPtr);
            }
            this->resource_ptrs[resId] = result;
            if (result != NULL) {
                *(uint8_t*)((uint8_t*)result + 0x162) = 1;  /* persistent flag */
            }
            return 1;

        case 6:
            if (resId == 0x1802 || (resId >= 0x1866 && (resId & 1) == 1)) {
                newObj = operator_new(0x7AC);
                if (newObj != NULL) {
                    result = CGWND_CursorEditWindow_Ctor(newObj, resId, strPtr);
                }
            } else {
                newObj = operator_new(0x168);
                if (newObj != NULL) {
                    result = UI_CreateChildWindow(newObj, resId, strPtr);
                }
            }
            break;

        case 7:
        case 8:
            if ((resId & 1) == 0) {
                newObj = operator_new(0x178);
                if (newObj != NULL) {
                    result = TrainStation_Ctor(newObj, resId, strPtr);
                }
            } else {
                newObj = operator_new(0x168);
                if (newObj != NULL) {
                    result = UI_CreateChildWindow(newObj, resId, strPtr);
                }
            }
            break;

        case 9:
        case 10:
        case 11:
            newObj = operator_new(0x168);
            if (newObj != NULL) {
                result = UI_CreateChildWindow(newObj, resId, strPtr);
            }
            break;

        case 12:
        case 13:
            newObj = operator_new(0x630);
            if (newObj != NULL) {
                result = INPUT_ExitGame(newObj, resId, strPtr);
            }
            break;

        case 14:
            newObj = operator_new(0x168);
            if (newObj != NULL) {
                result = UI_CreateChildWindow(newObj, resId, strPtr);
            }
            this->resource_ptrs[resId] = result;
            if (resId > 0x3801 && result != NULL) {
                *(uint8_t*)((uint8_t*)result + 0x162) = 1;
            }
            return 1;

        default:
            /* Fallthrough for types not explicitly handled */
            newObj = operator_new(0x168);
            if (newObj != NULL) {
                result = UI_CreateChildWindow(newObj, resId, strPtr);
            }
            break;
        }
    }

    /* Store the result in the resource registry */
    this->resource_ptrs[resId] = result;

    /* Check persistence: type 1 and 15 are always persistent */
    rawType = resId >> RESOURCE_TYPE_SHIFT;
    uint8_t finalType = (uint8_t)rawType;
    if (finalType >= 0x10) {
        finalType = 0;
    }

    bool isPersistent = (finalType == 1 || finalType == 0xF);
    if (!isPersistent && result != NULL) {
        /* Check persistent flag at +0x162 in ChildWindow */
        isPersistent = (*(uint8_t*)((uint8_t*)result + 0x162) == 1);
    }

    if (isPersistent) {
        return 1;
    }

    /* Not persistent — destroy the resource */
    void* stored = this->resource_ptrs[resId];
    if (stored != NULL) {
        void** vtable = (void**)stored;
        void (*dtor)(void*, uint8_t) = (void (*)(void*, uint8_t))vtable[0];
        dtor(stored, 1);
    }
    this->resource_ptrs[resId] = (void*)-1;

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

    int32_t id = (int32_t)startId;
    int32_t idTimes4 = (int32_t)startId * 4;  /* pre-multiplied offset */

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
            adjustedId = (uint32_t)id;
        } else {
            adjustedId = apply_language_offset(this->language_id, id);
        }

        /* Load string from EXE */
        void* hInstance = GetModuleHandleA(NULL);
        int32_t result = LoadStringA(hInstance, adjustedId, stringBuf, 0x104);

        if (adjustedId == (uint32_t)id) {
            /* No language offset — direct load */
            if (result == 0) {
                this->string_cache[idTimes4/4] = -1;
            } else {
                void* newEntry = operator_new(300);
                void* storedEntry = NULL;
                if (newEntry != NULL) {
                    storedEntry = RESMGR_AllocResourceEntry(
                        (ResourceEntry*)newEntry, id, (int32_t)stringBuf
                    );
                }
                this->string_cache[idTimes4/4] = storedEntry;

                /* If entry lacks surface data (flag at +9), destroy immediately */
                if (storedEntry != NULL &&
                    ((ResourceEntry*)storedEntry)->is_valid == 0)
                {
                    void** vtable = (void**)storedEntry;
                    void (*dtor)(void*, uint8_t) = (void (*)(void*, uint8_t))vtable[0];
                    dtor(storedEntry, 1);
                    this->string_cache[idTimes4/4] = -1;
                }
            }
        } else {
            /* Language offset applied */
            if (result == 0) {
                /* Fall back to original ID */
                hInstance = GetModuleHandleA(NULL);
                result = LoadStringA(hInstance, (UINT)id, stringBuf, 0x104);

                if (result == 0) {
                    this->string_cache[idTimes4/4] = -1;
                } else {
                    void* newEntry = operator_new(300);
                    void* storedEntry = NULL;
                    if (newEntry != NULL) {
                        storedEntry = RESMGR_AllocResourceEntry(
                            (ResourceEntry*)newEntry, id, (int32_t)stringBuf
                        );
                    }
                    this->string_cache[idTimes4/4] = storedEntry;

                    if (storedEntry != NULL &&
                        ((ResourceEntry*)storedEntry)->is_valid == 0)
                    {
                        void** vtable = (void**)storedEntry;
                        void (*dtor)(void*, uint8_t) = (void (*)(void*, uint8_t))vtable[0];
                        dtor(storedEntry, 1);
                        this->string_cache[idTimes4/4] = -1;
                    }
                }
            } else {
                /* Translation loaded successfully */
                void* newEntry = operator_new(300);
                void* storedEntry = NULL;
                if (newEntry != NULL) {
                    storedEntry = RESMGR_AllocResourceEntry(
                        (ResourceEntry*)newEntry, id, (int32_t)stringBuf
                    );
                }
                this->string_cache[idTimes4/4] = storedEntry;

                if (storedEntry != NULL &&
                    ((ResourceEntry*)storedEntry)->is_valid == 0)
                {
                    void** vtable = (void**)storedEntry;
                    void (*dtor)(void*, uint8_t) = (void (*)(void*, uint8_t))vtable[0];
                    dtor(storedEntry, 1);
                    this->string_cache[idTimes4/4] = -1;
                }
            }
        }

        id++;
        idTimes4 += 4;
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
    if (typeEntry == NULL) {
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
                uint32_t adjustedId = apply_language_offset(this->language_id, (uint32_t)idx);

                void* hInstance = GetModuleHandleA(NULL);
                int32_t result = LoadStringA(hInstance, adjustedId, stringBuf, 0x104);

                if (adjustedId == (uint32_t)idx) {
                    if (result != 0) {
                        this->AddString(idx, (int32_t)stringBuf);
                    } else {
                        *slotPtr = -1;
                    }
                } else {
                    if (result == 0) {
                        hInstance = GetModuleHandleA(NULL);
                        result = LoadStringA(hInstance, (UINT)idx, stringBuf, 0x104);
                        if (result == 0) {
                            *slotPtr = -1;
                        } else {
                            this->AddString(idx, (int32_t)stringBuf);
                        }
                    } else {
                        this->AddString(idx, (int32_t)stringBuf);
                    }
                }

                idx++;
                slotPtr++;
            }
        }

        /* Re-read after loading */
        typeEntry = &this->resource_type_idx[resId];
        if (typeEntry != NULL) {
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
        this->LoadStringTable(stringId, (int32_t)stringId);

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
    if ((int32_t)resId < 0 || resId > 0x3FFF) {
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

            void* hInstance = GetModuleHandleA(NULL);
            char stringBuf[264];
            int32_t result = LoadStringA(hInstance, adjustedId, stringBuf, 0x104);

            if (adjustedId == curId) {
                if (result != 0) {
                    this->AddString((int32_t)curId, (int32_t)stringBuf);
                } else {
                    this->resource_ptrs[curId] = -1;
                }
            } else {
                if (result == 0) {
                    hInstance = GetModuleHandleA(NULL);
                    result = LoadStringA(hInstance, (UINT)curId, stringBuf, 0x104);
                    if (result == 0) {
                        this->resource_ptrs[curId] = -1;
                    } else {
                        this->AddString((int32_t)curId, (int32_t)stringBuf);
                    }
                } else {
                    this->AddString((int32_t)curId, (int32_t)stringBuf);
                }
            }

            curId++;
            slotPtr++;
        } while ((int32_t)curId <= (int32_t)resId);

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
/* one slot to another. Called by INPUT_SetMouse.                     */
/* ================================================================== */

void ResourceManager::RegisterDependency(int32_t depIndex, int32_t resIndex)
{
    /* Write the address of the resource slot into the type-index array */
    this->resource_type_idx[depIndex] =
        (int32_t)&this->resource_ptrs[resIndex];
}

/* ================================================================== */
/* ResourceManager::AnimateClock                                       */
/* Address: 0x447400                                                   */
/* ================================================================== */

void ResourceManager::AnimateClock(int32_t timestamp)
{
    /* ================================================================ */
    /* Phase 0: Check if clock background resource has surface data     */
    /* ================================================================ */

    int32_t bgResource = this->GetById(CLOCK_RES_BG);  /* 0x842 */
    if (bgResource == 0) {
        return;
    }

    /* Check if background resource has surface data (flag at +0x10) */
    int32_t* bgData = (int32_t*)(uintptr_t)bgResource;
    if (bgData[4] == 0) {  /* +0x10 */
        return;
    }

    /* ================================================================ */
    /* Phase 1: Compute clock segment and play sound on change          */
    /* ================================================================ */

    int32_t minutes = (timestamp / 60) % 60;
    int32_t segment = (minutes / 5 + 1) % CLOCK_SEGMENTS;

    int32_t soundId = 0;

    if (segment != this->clock_hand_segment) {
        if (segment == 0) {
            /* Hour — play chime (sound 0x53AB) */
            this->clock_hand_segment = 0;

            /* Lazy-load sound at global sound cache entry 0x4A64C8 */
            int32_t* soundCache = (int32_t*)0x004A64C8;
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
        } else if ((segment == 3) || (segment == 6) || (segment == 9)) {
            /* Quarter-hour — play tick (sound 0x5399) */
            this->clock_hand_segment = segment;

            int32_t* soundCache = (int32_t*)0x004A6480;
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
            /* Other segments — update state, no sound */
            this->clock_hand_segment = segment;
        }

        if (g_audio != NULL && soundId != 0) {
            GameAudio_AllocChannel(
                g_audio, soundId,
                NULL,
                g_listener_x,
                g_listener_y,
                4,      /* priority = 4 */
                0       /* flags = 0 */
            );
        }
    }

    /* ================================================================ */
    /* Phase 2: Render hour hand from sprite strips                     */
    /* ================================================================ */

    /* Get hour hand background frame sprite (0x3DAE) */
    int32_t hourBgResource = this->GetById(CLOCK_HOUR_BG);
    if (hourBgResource != 0) {
        void* hourBgRes = (void*)(uintptr_t)hourBgResource;
        void** hourBgVtbl = *(void***)hourBgRes;

        /* Lock surface (vtable[1]) */
        void* hourBgSurface = (void*)((void* (*)(void*, int32_t, int32_t))hourBgVtbl[1])(hourBgRes, 0, 0);

        int32_t hourBgWidth  = *(uint16_t*)((uint8_t*)hourBgRes + 0x14);
        int32_t hourBgHeight = *(uint16_t*)((uint8_t*)hourBgRes + 0x16);

        RECT srcRect;
        SetRect(&srcRect, 0, 0, hourBgWidth - 1, hourBgHeight - 1);

        RECT dstRect;
        CopyRect(&dstRect, &srcRect);

        OffsetRect(&srcRect, segment * hourBgWidth, 0);
        OffsetRect(&dstRect, CLOCK_HOUR_OFFSET_X, CLOCK_HOUR_OFFSET_Y);

        Town_CopyTiles8bpp_Transparent(
            hourBgSurface,
            dstRect.left, dstRect.top, dstRect.right, dstRect.bottom,
            *(uint32_t*)(uintptr_t)(bgData[4] + 0x18),
            *(int32_t*)(uintptr_t)(bgData[4] + 0x8),
            srcRect.left, srcRect.top, srcRect.right, srcRect.bottom
        );

        /* Unlock surface (vtable[2]) */
        ((void (*)(void*))hourBgVtbl[2])(hourBgRes);

        /* Get hour hand sprite (0x3DAD) */
        int32_t hourHandResource = this->GetById(CLOCK_HOUR_HAND);
        if (hourHandResource != 0) {
            void* hourHandRes = (void*)(uintptr_t)hourHandResource;
            void** hourHandVtbl = *(void***)hourHandRes;

            void* hourHandSurface = (void*)((void* (*)(void*, int32_t, int32_t))hourHandVtbl[1])(hourHandRes, 0, 0);

            int32_t hourWidth  = *(uint16_t*)((uint8_t*)hourHandRes + 0x14);
            int32_t hourHeight = *(uint16_t*)((uint8_t*)hourHandRes + 0x16);

            RECT srcRect2;
            SetRect(&srcRect2, 0, 0, hourWidth - 1, hourHeight - 1);

            RECT dstRect2;
            CopyRect(&dstRect2, &srcRect2);

            OffsetRect(&srcRect2, segment * hourWidth, 0);
            OffsetRect(&dstRect2, CLOCK_HOUR_OFFSET_X, CLOCK_HOUR_OFFSET_Y);

            Town_CopyTiles8bpp_Transparent(
                hourHandSurface,
                dstRect2.left, dstRect2.top, dstRect2.right, dstRect2.bottom,
                *(uint32_t*)(uintptr_t)(bgData[4] + 0x18),
                *(int32_t*)(uintptr_t)(bgData[4] + 0x8),
                srcRect2.left, srcRect2.top, srcRect2.right, srcRect2.bottom
            );

            ((void (*)(void*))hourHandVtbl[2])(hourHandRes);
        }
    }

    /* ================================================================ */
    /* Phase 3: Render minute hand                                      */
    /* ================================================================ */

    int32_t minuteResource = this->GetById(CLOCK_RES_MINUTE);  /* 0x843 */
    if (minuteResource != 0 && *(int32_t*)((uintptr_t)minuteResource + 0x10) != 0) {
        /* Get minute hand background sprite (0x3DB1) */
        int32_t minBgResource = this->GetById(CLOCK_MINUTE_BG);
        if (minBgResource != 0) {
            void* minBgRes = (void*)(uintptr_t)minBgResource;
            void** minBgVtbl = *(void***)minBgRes;

            void* minBgSurface = (void*)((void* (*)(void*, int32_t, int32_t))minBgVtbl[1])(minBgRes, 0, 0);

            int32_t minBgWidth  = *(uint16_t*)((uint8_t*)minBgRes + 0x14);
            int32_t minBgHeight = *(uint16_t*)((uint8_t*)minBgRes + 0x16);

            RECT minSrcRect1;
            SetRect(&minSrcRect1, 0, 0, minBgWidth - 1, minBgHeight - 1);

            RECT minDstRect1;
            CopyRect(&minDstRect1, &minSrcRect1);

            OffsetRect(&minSrcRect1, minutes * minBgWidth, 0);
            OffsetRect(&minDstRect1, CLOCK_MINUTE_OFFSET_X, CLOCK_MINUTE_OFFSET_Y);

            int32_t* minResourceData = (int32_t*)(uintptr_t)minuteResource;
            Town_CopyTiles8bpp_Transparent(
                minBgSurface,
                minDstRect1.left, minDstRect1.top,
                minDstRect1.right, minDstRect1.bottom,
                *(uint32_t*)(uintptr_t)(minResourceData[4] + 0x18),
                *(int32_t*)(uintptr_t)(minResourceData[4] + 0x8),
                minSrcRect1.left, minSrcRect1.top,
                minSrcRect1.right, minSrcRect1.bottom
            );

            ((void (*)(void*))minBgVtbl[2])(minBgRes);

            /* Get minute hand sprite (0x3DB0) */
            int32_t minHandResource = this->GetById(CLOCK_MINUTE_HAND);
            if (minHandResource != 0) {
                void* minHandRes = (void*)(uintptr_t)minHandResource;
                void** minHandVtbl = *(void***)minHandRes;

                void* minHandSurface = (void*)((void* (*)(void*, int32_t, int32_t))minHandVtbl[1])(minHandRes, 0, 0);

                int32_t minWidth  = *(uint16_t*)((uint8_t*)minHandRes + 0x14);
                int32_t minHeight = *(uint16_t*)((uint8_t*)minHandRes + 0x16);

                RECT minSrcRect2;
                SetRect(&minSrcRect2, 0, 0, minWidth - 1, minHeight - 1);

                RECT minDstRect2;
                CopyRect(&minDstRect2, &minSrcRect2);

                OffsetRect(&minSrcRect2, minutes * minWidth, 0);
                OffsetRect(&minDstRect2, CLOCK_MINUTE_OFFSET_X, CLOCK_MINUTE_OFFSET_Y);

                Town_CopyTiles8bpp_Transparent(
                    minHandSurface,
                    minDstRect2.left, minDstRect2.top,
                    minDstRect2.right, minDstRect2.bottom,
                    *(uint32_t*)(uintptr_t)(minResourceData[4] + 0x18),
                    *(int32_t*)(uintptr_t)(minResourceData[4] + 0x8),
                    minSrcRect2.left, minSrcRect2.top,
                    minSrcRect2.right, minSrcRect2.bottom
                );

                ((void (*)(void*))minHandVtbl[2])(minHandRes);
            }
        }

        /* Phase 4: Invalidate viewport */
        /* Globals: g_viewport_rect vars at fixed addresses */
        extern int32_t g_viewport_rect_left;   /* @ 0x...... */
        extern int32_t g_viewport_rect_top;
        extern int32_t g_viewport_rect_right;
        extern int32_t g_viewport_rect_bottom;
        extern void* g_tilemap;

        TileMap_InvalidateRect(
            g_tilemap,
            g_viewport_rect_left,
            g_viewport_rect_top,
            g_viewport_rect_right,
            g_viewport_rect_bottom
        );
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
    int32_t rawType = (int32_t)id >> RESOURCE_TYPE_SHIFT;
    uint8_t typeByte = (uint8_t)rawType;
    return (typeByte < 0x10) ? (UINT)typeByte : 0;
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
        g_resmgr.LoadStringTable(soundId, (int32_t)soundId);
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
    if (g_audio != NULL && soundRes != 0) {
        GameAudio_AllocChannel(
            g_audio,
            soundRes,
            NULL,
            g_listener_x,
            g_listener_y,
            4, 0
        );
    }
}

/* ================================================================== */
/* ResourceData_Dtor                                                   */
/* Address: 0x447B60                                                   */
/* ================================================================== */
void* __thiscall ResourceData_Dtor(void* self, uint8_t flags)
{
    *(void***)self = (void**)VTBL_RESDATA;
    RESMGR_RemoveResource((RESDATA*)self);
    return self;
}

/* ================================================================== */
/* RESMGR_ResourceData_Init                                            */
/* Address: 0x447B20                                                   */
/* ================================================================== */
void RESMGR_ResourceData_Init(RESDATA* resdata)
{
    /* Save/load fields accessed via offset (not in the sprite-metadata RESDATA layout) */
    *(void**)((uint8_t*)resdata + 0x1C4) = NULL;   /* pixels          */
    *(void**)((uint8_t*)resdata + 0x1C8) = NULL;   /* primary_stream  */
    *(void**)((uint8_t*)resdata + 0x1CC) = NULL;   /* secondary_stream*/
    *(void**)((uint8_t*)resdata + 0x1D0) = NULL;   /* asset_data      */
    *(int32_t*)((uint8_t*)resdata + 0x1D4) = 0;    /* asset_size      */
    *(uint16_t*)((uint8_t*)resdata + 0xB0) = 0;    /* resource_type   */
    *(uint16_t*)((uint8_t*)resdata + 0xB2) = 0;    /* height          */
    *(uint16_t*)((uint8_t*)resdata + 0xB4) = 0;    /* width           */
}

/* ================================================================== */
/* RESMGR_SoundObject_Ctor                                             */
/* Address: 0x448F30                                                   */
/* ================================================================== */
void* RESMGR_SoundObject_Ctor(void* self, int32_t strLen, int32_t param2,
                               int32_t param3, void* font, uint16_t param5)
{
    TrackPiece_Ctor(self, param2, param3, param5);
    ((SoundObject*)self)->max_text_len = strLen;
    *(void***)self = (void**)VTBL_SOUND_OBJECT;
    ((SoundObject*)self)->type = 8;
    char* textBuf = (char*)operator_new(strLen + 1);
    ((SoundObject*)self)->text_buf = textBuf;
    if (textBuf != NULL) strcpy(textBuf, "");
    ((SoundObject*)self)->consume_state = 0;
    ((SoundObject*)self)->font = font;
    return self;
}

/* ================================================================== */
/* RESMGR_SoundObject_Dtor                                             */
/* Address: 0x448FE0                                                   */
/* ================================================================== */
void* RESMGR_SoundObject_Dtor(void* self, uint8_t flags)
{
    RESDATA_SoundObject_BaseDtor(self);
    return self;
}

/* ================================================================== */
/* RESDATA_SoundObject_BaseDtor                                        */
/* ================================================================== */
void __fastcall RESDATA_SoundObject_BaseDtor(void* self)
{
    *(void***)self = (void**)VTBL_SOUND_OBJECT;
    if (((SoundObject*)self)->text_buf != NULL) {
        GLOBAL_free(((SoundObject*)self)->text_buf);
        ((SoundObject*)self)->text_buf = NULL;
    }
    TrackPiece_Dtor(self);
}

/* ================================================================== */
/* RESDATA_SoundObject_Init                                            */
/* ================================================================== */
void* __thiscall RESDATA_SoundObject_Init(void* self, const char* source)
{
    char* textBuf = ((SoundObject*)self)->text_buf;
    int32_t maxLen = ((SoundObject*)self)->max_text_len;
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
    return ((SoundObject*)self)->text_buf;
}

/* ================================================================== */
/* RESDATA_SoundObject_GetTextLength                                   */
/* ================================================================== */
int32_t __fastcall RESDATA_SoundObject_GetTextLength(void* self)
{
    const char* textBuf = ((SoundObject*)self)->text_buf;
    return textBuf ? (int32_t)strlen(textBuf) : 0;
}
