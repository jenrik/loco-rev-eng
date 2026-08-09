/**
 * core_stubs.cpp — Core game function stubs replacing remaining defsym entries
 *
 * Provides proper C++ class definitions for vtable generation and
 * stub implementations for overloaded free functions.
 * Win32/DirectX shim symbols are excluded - handled by sdl3_shims.
 */

// Status: TRANSCRIBED

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>

typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HMENU;
typedef void* HICON;
typedef uint32_t DWORD;
typedef uint32_t UINT;
typedef int32_t BOOL;
typedef const char* LPCSTR;
struct RECT { int32_t left, top, right, bottom; };
struct POINT { int32_t x, y; };
struct RESDATA;
struct ResourceEntry;
struct Building;
struct GameAudio;
struct WNDPROC_Stream;

/* =========================================================== */
/* A. Free function stubs (overloaded signatures)               */
/* =========================================================== */

/* UIPANEL_Blit's `int**` 6th-param overload (_Z12UIPANEL_BlitPvjjijPPijjijj)
 * removed: it was the unrelated wrong stub town/TownTiles.cpp's
 * BlitElement used to call with a stray int**-typed argument before the
 * town-tilerender-merge session fixed it (2026-08-06) — confirmed zero
 * referrers via `nm --print-file-name build/lego_loco.p/*.o | grep
 * _Z12UIPANEL_BlitPvjjijPPijjijj`. See docs/landmine-sweep-worklist.md. */

/* AssetMgr_LoadFile — two overloads */
void AssetMgr_LoadFile(int*, unsigned char*, int*);
void AssetMgr_LoadFile(int*, unsigned char*, int*) {}
void AssetMgr_LoadFile(void*, char const*, int*);
void AssetMgr_LoadFile(void*, char const*, int*) {}

/* GameWindow_Create — two overloads */
void GameWindow_Create(void*, int, void*, int, int, int, int, void*, void*, unsigned int, int, int, unsigned char);
void GameWindow_Create(void*, int, void*, int, int, int, int, void*, void*, unsigned int, int, int, unsigned char) {}
void GameWindow_Create(void*, int, void*, int, int, int, int, void*, void*, unsigned int, unsigned int, unsigned int, unsigned char);
void GameWindow_Create(void*, int, void*, int, int, int, int, void*, void*, unsigned int, unsigned int, unsigned int, unsigned char) {}

/* Vehicle_LoadSounds */
void Vehicle_LoadSounds(void*, int*, char);
void Vehicle_LoadSounds(void*, int*, char) {}

/* GameObject_SetFrame — Address: 0x405DE0 */
void GameObject_SetFrame(void*, int, bool);
void GameObject_SetFrame(void*, int, bool) {}

/* UIPANEL_StretchBlit — two overloads, Address: 0x42AB10 */
void UIPANEL_StretchBlit(void*, char*, unsigned int, int, int);
void UIPANEL_StretchBlit(void*, char*, unsigned int, int, int) {}
void UIPANEL_StretchBlit(void*, char const*, int, int, int);
void UIPANEL_StretchBlit(void*, char const*, int, int, int) {}

/* FormatResourceString — Address: referenced from UI_WindowBase ctor */
void FormatResourceString(void*, unsigned int, char*, unsigned int);
void FormatResourceString(void*, unsigned int, char*, unsigned int) {}

/* RESMGR_RemoveResource is real code in resources/ResDataSave.cpp */

/* GameAudio_UpdateVolume — two overloads, Address: 0x4135B0 */
void GameAudio_UpdateVolume(void*, char);
void GameAudio_UpdateVolume(void*, char) {}
void GameAudio_UpdateVolume(void*, int);
void GameAudio_UpdateVolume(void*, int) {}

/* Building_CheckPlacement */
void Building_CheckPlacement(Building*, int, int);
void Building_CheckPlacement(Building*, int, int) {}

/* CGWND_AudioChannel_Play */
void CGWND_AudioChannel_Play(void*);
void CGWND_AudioChannel_Play(void*) {}

/* GameAudio_PlayResourceEx — two overloads, Address: 0x4131C0 */
void GameAudio_PlayResourceEx(void*, int, unsigned int*);
void GameAudio_PlayResourceEx(void*, int, unsigned int*) {}
void GameAudio_PlayResourceEx(void*, unsigned int, int*);
void GameAudio_PlayResourceEx(void*, unsigned int, int*) {}

/* RESMGR_AllocResourceEntry */
void RESMGR_AllocResourceEntry(ResourceEntry*, int, int);
void RESMGR_AllocResourceEntry(ResourceEntry*, int, int) {}

/* =========================================================== */
/* B. Class definitions (generate vtables + typeinfo)           */
/* =========================================================== */

/* --- Collection + SortedCollection (need vtables) --- */
struct Collection_Stub {
    void** items;
    int32_t count;
    int32_t capacity;
    virtual ~Collection_Stub() {}
    virtual void Resize(int32_t) {}
    virtual void* GetAt(int32_t) { return nullptr; }
};

struct SortedCollection_Stub : Collection_Stub {
    virtual int32_t Compare(void*, void*) { return 0; }
    virtual void SortRange(int32_t, int32_t) {}
};

/* --- UI_WindowBase (ctor + dtor + base dtor + typeinfo) ---
 * Addresses: Ctor=0x425870, Dtor=0x4258F0, BaseDtor=0x425910
 * Vtable: 0x477C30, Size: ~0x80 bytes
 */
struct UI_WindowBase_Stub {
    void* vtable_ptr;
    HINSTANCE hInstance;      // +0x04
    UINT resourceId;          // +0x08
    void* field_0c;           // +0x0C
    void* field_10;           // +0x10
    void* field_14;           // +0x14
    int32_t field_18;         // +0x18
    int32_t field_1c;         // +0x1C
    int32_t width;            // +0x20
    int32_t height;           // +0x24
    int32_t field_28;         // +0x28
    int32_t field_2c;         // +0x2C
    int32_t x;                // +0x30
    int32_t y;                // +0x34
    int32_t field_38;         // +0x38
    int32_t field_3c;         // +0x3C
    char visible;             // +0x40
    char field_41;            // +0x41
    char field_42;            // +0x42
    char field_43;            // +0x43
    int32_t cursor_x;         // +0x44
    int32_t cursor_y;         // +0x48
    int32_t field_4c;         // +0x4C
    int32_t field_50;         // +0x50
    RECT dirty_rect;          // +0x54
    int32_t field_64;         // +0x64
    int32_t field_68;         // +0x68
    int32_t field_6c;         // +0x6C
    int32_t field_70;         // +0x70
    int32_t field_74;         // +0x74
    int32_t field_78;         // +0x78
    int32_t field_7c;         // +0x7C

    /* Ctor — Address: 0x425870 */
    UI_WindowBase_Stub(void*, UINT) {
        hInstance = nullptr;
        resourceId = 0;
        field_0c = nullptr;
        field_10 = nullptr;
        field_14 = nullptr;
        field_18 = 0;
        field_1c = 0;
        width = 0;
        height = 0;
        field_28 = 0;
        field_2c = 0;
        x = 0;
        y = 0;
        field_38 = 0;
        field_3c = 0;
        visible = 0;
        field_41 = 0;
        field_42 = 0;
        field_43 = 0;
        cursor_x = 0;
        cursor_y = 0;
        field_4c = 0;
        field_50 = 0;
        field_64 = 0;
        field_68 = 0;
        field_6c = 0;
        field_70 = 0;
        field_74 = 0;
        field_78 = 0;
        field_7c = 0;
    }

    /* Base Dtor — Address: 0x425910 */
    virtual ~UI_WindowBase_Stub() {
        field_18 = 0;
        field_1c = 0;
        field_64 = 0;
        field_68 = 0;
        field_6c = 0;
        field_70 = 0;
        field_74 = 0;
    }
};

/* --- UIEntity (vtable only) --- */
struct UIEntity_Stub {
    virtual ~UIEntity_Stub() {}
    virtual void Render() {}
    virtual void Update() {}
    virtual void HandleInput() {}
};

/* --- Building::Building(int) ---
 * Address: Building_BaseCtor=0x433A20
 * Vtable: 0x477EB8, Size: 0xF4
 */
struct Building_Stub {
    char data[0xF4];
    Building_Stub(int) {
        for (int i = 0; i < static_cast<int>(sizeof(data)); i++) data[i] = 0;
    }
    virtual ~Building_Stub() {}
    virtual void Deserialize(void*, int) {}
    virtual int32_t HitTest(int32_t, int32_t) { return 0; }
    virtual void Update() {}
    virtual void Draw() {}
};

/* --- VehicleEditor member functions --- */
struct VehicleEditor_Stub {
    char data[0x440];
    virtual ~VehicleEditor_Stub() {}
    virtual void CalcAngle() {}                    // 0x40DF80
    virtual void CheckEdgeBounds(void*) {}         // 0x40E2A0
    virtual void CheckEditBounds1(void*) {}        // 0x40E440
    virtual void CheckEditBounds2(void*) {}        // 0x40E520
    virtual void CheckVehicleAttach(void*) {}      // 0x40E340
    virtual int32_t IsInBounds(int16_t, int16_t, int16_t) { return 0; }
};

/* --- GameSetupPanel member functions --- */
struct GameSetupPanel_Stub {
    char data[0x240];
    virtual ~GameSetupPanel_Stub() {}
    virtual void HandleMapClick(int32_t, int32_t) {}
    virtual void SelectLayoutEntry(int32_t) {}
    virtual void SendScenarioSelect(int32_t) {}
    virtual void ConnectToNetworkGame(int32_t) {}
};

/* --- HelpWnd member functions ---
 * Address: HelpWnd_FindPage=0x44F210, HelpWnd_SetPage=0x44F340
 */
struct HelpWnd_Stub {
    char data[0x130];
    virtual ~HelpWnd_Stub() {}
    virtual void render_page(int*) {}
    virtual void render_scroll_up(int*) {}
    virtual void render_scroll_down(int*) {}
    virtual void update_anim_sprite(int32_t) {}
};

/* --- TrainEntity::TrainEntity(int) ---
 * Address: 0x4533D0, Vtable: 0x4780B8
 */
struct TrainEntity_Stub {
    char data[0x90];
    TrainEntity_Stub(int32_t) {
        for (int i = 0; i < static_cast<int>(sizeof(data)); i++) data[i] = 0;
    }
    virtual ~TrainEntity_Stub() {}
};

/* --- RESDATA_ScriptedObject::EnterBuildMode --- */
struct RESDATA_ScriptedObject_Stub {
    virtual ~RESDATA_ScriptedObject_Stub() {}
    virtual void EnterBuildMode(unsigned char) {}
};

/* --- IDirectDrawSurface4 (included for completeness) --- */
struct IDirectDrawSurface4_Stub {
    virtual ~IDirectDrawSurface4_Stub() {}
};

/* =========================================================== */
/* C. Explicit member function definitions                      */
/*   (for symbols that need free-function-style mangling)       */
/* =========================================================== */

/* Collection::GetAt — separate from Collection_Stub vtable */
void* Collection_GetAt(void*, int32_t);
void* Collection_GetAt(void*, int32_t) { return nullptr; }

/* Building::Building(int) — separate from Building_Stub */
void Building_Building(void*, int32_t);
void Building_Building(void*, int32_t) {}

