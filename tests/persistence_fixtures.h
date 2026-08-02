// Status: VALIDATED
/**
 * persistence_fixtures.h — shared link fixtures for the persistence tests
 *
 * The persistence tests link the REAL persistence translation units
 * (InputMgr.o / ResDataSave.o / PersistenceAdapter.o) plus the REAL
 * placed-object class TUs (GameObject.o, Entity.o,
 * BuildingMgrObjectGroup.o, ResdataGameVehicle.o, GameVehicle.o,
 * HelpPageNode.o, Building.o — the typed place/find/world-load callee
 * cone).  Everything those objects reference that is not another linked
 * object is provided here:
 *
 *   - canonical globals (stubs_impl-style storage),
 *   - fail-loud fixtures for singletons the exercised host paths must
 *     never reach (g_tilemap / g_game / g_world / g_netman methods,
 *     tooltips, tile predicates),
 *   - real-but-cheap entry points (PlaySound record, GetResourceType,
 *     CRT_rand, CRT_wcsstr, rect helpers),
 *   - temp-dir helpers (writes go to build/test-artifacts; shipped
 *     art-res/SAVEGAME saves and art-res/~curr are never mutated).
 *
 * The link is honest: no --unresolved-symbols=ignore-all.
 */

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

/* Complete class definitions needed by the fixtures below. */
#include "../src/decompiled_cpp/core/Game.h"
#include "../src/decompiled_cpp/core/Entity.h"
#include "../src/decompiled_cpp/core/BuildingMgrObjectGroup.h"
#include "../src/decompiled_cpp/game/World.h"
#include "../src/decompiled_cpp/game/Vehicle.h"
#include "../src/decompiled_cpp/game/Building.h"
#include "../src/decompiled_cpp/game/BuildingMgr.h"
#include "../src/decompiled_cpp/game/GameVehicle.h"
#include "../src/decompiled_cpp/game/ResdataGameVehicle.h"
#include "../src/decompiled_cpp/ui/HelpPageNode.h"
#include "../src/decompiled_cpp/network/Netman.h"
#include "../src/decompiled_cpp/world/tilemap.h"
#include "../src/decompiled_cpp/resources/ResourceManager.h"

/* ---- Fail-loud macro ---- */

#define LOUD_FIXTURE(name)                                                     \
    static void fixture_reached_##name()                                       \
    {                                                                          \
        std::fprintf(stderr, "FAIL: unexpected reach of " #name " fixture\n"); \
        std::abort();                                                          \
    }

/* ---- Canonical globals (addresses from InputMgr.h / types.h) ---- */

void* operator_new(size_t size) { return std::malloc(size); }
void  GLOBAL_free(void* ptr) { std::free(ptr); }

void*    g_game = nullptr;                /* 0x4854C8 */
void*    g_world = nullptr;               /* 0x4A98B0 */
void*    g_netman = nullptr;              /* 0x4FD3AC */
void*    g_tooltip_mgr = nullptr;         /* 0x4FD220 */
void*    g_asset_mgr = nullptr;           /* 0x485600 */
void*    g_audio = nullptr;               /* 0x4FD3BC */
void*    g_building_mgr = nullptr;        /* 0x485448 */
void*    g_main_window = nullptr;         /* 0x4AA4A0 */
void*    g_primary_surface = nullptr;     /* 0x4FD3C4 */
int32_t  g_selected_building = 0;        /* 0x4855B0 (tilemap.h) */
int32_t  g_player_id = 0;                 /* 0x4AAD46 */
int32_t  g_player_color = 0;              /* 0x4AAD48 */
int32_t  g_in_build_mode = 0;             /* 0x4FD199 */
uint8_t  g_allow_building_placement = 1;  /* 0x4FD3DC */
int32_t  g_demo_mode = 0;                 /* 0x4A9918 */
int32_t  g_is_game_active = 0;            /* 0x4854C4 */
uint8_t  g_is_party_mode = 0;             /* 0x48548C */
uint32_t g_party_start_time = 0;          /* 0x485490 */
uint8_t  g_building_animating = 0;        /* 0x4851FC */
uint32_t g_game_time = 0;                 /* 0x4A99B4 */
char     g_empty_string = 0;              /* 0x4851D0 */
char     g_install_path[256] = ".";
char     g_current_save_path[0x108] = ""; /* 0x4AA8F8 */
int32_t  DAT_004a98b4 = 0;                /* 0x4A98B4 */
int32_t  DAT_004a98b8[4] = {0, 0, 0, 0};  /* 0x4A98B8 */
double   _DAT_00481170 = 0.0;             /* 0x481170 — FPS threshold (GameObject.cpp) */

/* g_resmgr — the canonical static object (0x4855E8); never initialized
 * in these tests (GetById below returns nullptr). */
ResourceManager g_resmgr;

/* g_tilemap — typed TileMap* (tilemap.h); kept null in these tests. */
TileMap* g_tilemap = nullptr;

/* ---- Fail-loud singletons (abort when reached) ---- */

/* TileMap methods — only reached when host_placement_available() is true
 * or a guarded singleton check regresses. */
LOUD_FIXTURE(TileMap_FindObject)
LOUD_FIXTURE(TileMap_FullReset)
LOUD_FIXTURE(TileMap_ScrollTo)
LOUD_FIXTURE(TileMap_InvalidateDirtyRects)
int* TileMap::FindObject(unsigned int, short, short, char, unsigned int)
{ fixture_reached_TileMap_FindObject(); return nullptr; }
void TileMap::FullReset()
{ fixture_reached_TileMap_FullReset(); }
void* TileMap::ScrollTo(TileMapObject*, int)
{ fixture_reached_TileMap_ScrollTo(); return nullptr; }
void TileMap::InvalidateDirtyRects(char)
{ fixture_reached_TileMap_InvalidateDirtyRects(); }

/* World / Game / Netman / Vehicle — reached only by the gated placement
 * path or the unconstructed-singleton guards. */
LOUD_FIXTURE(World_Init)
LOUD_FIXTURE(World_LoadFromFile)
LOUD_FIXTURE(Game_SetScreenMode)
LOUD_FIXTURE(Game_DeselectGameObject)
LOUD_FIXTURE(Netman_CheckUpEdge)
LOUD_FIXTURE(Netman_CheckDownEdge)
LOUD_FIXTURE(Netman_CheckLeftEdge)
LOUD_FIXTURE(Netman_CheckRightEdge)
void World::Init()
{ fixture_reached_World_Init(); }
Vehicle* World::LoadFromFile(int*, int*)
{ fixture_reached_World_LoadFromFile(); return nullptr; }
void Game::SetScreenMode(uint8_t, uint8_t, uint8_t)
{ fixture_reached_Game_SetScreenMode(); }
void Game::DeselectGameObject()
{ fixture_reached_Game_DeselectGameObject(); }
int32_t Netman::CheckUpEdge()
{ fixture_reached_Netman_CheckUpEdge(); return 0; }
int32_t Netman::CheckDownEdge()
{ fixture_reached_Netman_CheckDownEdge(); return 0; }
int32_t Netman::CheckLeftEdge()
{ fixture_reached_Netman_CheckLeftEdge(); return 0; }
int32_t Netman::CheckRightEdge()
{ fixture_reached_Netman_CheckRightEdge(); return 0; }

/* Vehicle methods referenced by the class cone (Building.o / World.o) —
 * not exercised by the persistence tests. */
LOUD_FIXTURE(Vehicle_UpdatePosition)
LOUD_FIXTURE(Vehicle_InitOccupant)
LOUD_FIXTURE(Vehicle_Stop)
LOUD_FIXTURE(Vehicle_FindPath)
LOUD_FIXTURE(Vehicle_IsMoving)
LOUD_FIXTURE(Vehicle_SetState)
void Vehicle::UpdatePosition(uint8_t)
{ fixture_reached_Vehicle_UpdatePosition(); }
void Vehicle::InitOccupant(int32_t)
{ fixture_reached_Vehicle_InitOccupant(); }
void Vehicle::Stop(int32_t, uint8_t)
{ fixture_reached_Vehicle_Stop(); }
void Vehicle::FindPath(int32_t*, uint8_t)
{ fixture_reached_Vehicle_FindPath(); }
uint8_t Vehicle::IsMoving()
{ fixture_reached_Vehicle_IsMoving(); return 0; }
void Vehicle::SetState(int32_t)
{ fixture_reached_Vehicle_SetState(); }

/* BuildingMgr methods referenced by the class cone — not exercised. */
LOUD_FIXTURE(BuildingMgr_RemoveObject)
LOUD_FIXTURE(BuildingMgr_CompactCollections)
LOUD_FIXTURE(BuildingMgr_CreateFromResource)
void BuildingMgr::RemoveObject(Building*, bool)
{ fixture_reached_BuildingMgr_RemoveObject(); }
void BuildingMgr::CompactCollections()
{ fixture_reached_BuildingMgr_CompactCollections(); }
Building* BuildingMgr::CreateFromResource(int, int, int, int)
{ fixture_reached_BuildingMgr_CreateFromResource(); return nullptr; }

/* ---- Real-but-cheap entry points ---- */

/* PlaySound (0x447930): the persistence path only plays 0x5026 (new-game
 * jingle).  Record the call so tests can assert INPUT_NewWorld fired it. */
int  g_last_play_sound_id = 0;
void PlaySound(int id)
{ g_last_play_sound_id = id; }

/* Tooltip entry points: g_tooltip_mgr stays null on the host, so the
 * guarded host paths skip them (loud log).  Reaching these means the
 * guard regressed. */
LOUD_FIXTURE(UI_CleanupTooltips)
LOUD_FIXTURE(UI_HideTooltip)
void UI_CleanupTooltips(void*)
{ fixture_reached_UI_CleanupTooltips(); }
void UI_HideTooltip(void*)
{ fixture_reached_UI_HideTooltip(); }

/* GetResourceType (0x446030): (id >> 10) & 0xFF, capped at 0x10.
 * Both the UINT form (ResourceManager.h, InputMgr.o) and the int form
 * (Building.o) are referenced. */
unsigned int GetResourceType(unsigned int id)
{
    int32_t raw = static_cast<int32_t>(id) >> 10;
    uint8_t type_byte = static_cast<uint8_t>(raw);
    return (type_byte < 0x10) ? static_cast<unsigned int>(type_byte) : 0;
}
unsigned int GetResourceType(int id)
{
    return GetResourceType(static_cast<unsigned int>(id));
}

/* RESDATA tile predicates — reached only by the gated placement path. */
LOUD_FIXTURE(RESDATA_IsBuildingTile)
LOUD_FIXTURE(RESDATA_IsRoadTile)
int RESDATA_IsBuildingTile(intptr_t)
{ fixture_reached_RESDATA_IsBuildingTile(); return 0; }
int RESDATA_IsRoadTile(int)
{ fixture_reached_RESDATA_IsRoadTile(); return 0; }
int RESDATA_IsRoadTile(void*)
{ fixture_reached_RESDATA_IsRoadTile(); return 0; }

/* Host resource bridge: no resources are loaded in the persistence
 * tests, so GetById returns nullptr (a fresh host manager). */
void* ResourceManager_GetById(void*, int) { return nullptr; }
void* ResourceManager_GetById(void*, unsigned int) { return nullptr; }

/* CRT_rand (0x466150) — deterministic linear congruential generator for
 * the FindObjectAt random-pick tests.  Netman.h declares it with C++
 * linkage (_Z8CRT_randv); some cone TUs (GameObject.o, Building.o)
 * reference the plain C symbol (CRT_rand).  C++ forbids declaring the
 * same signature under two linkages in one scope, so the C symbol is
 * provided through a GNU asm-label alias. */
static unsigned long s_rand_state = 0x12345678u;
static int crt_rand_impl(void)
{
    s_rand_state = s_rand_state * 1103515245u + 12345u;
    return static_cast<int>((s_rand_state >> 16) & 0x7FFFFFFFu);
}
int32_t CRT_rand(void)   /* C++ linkage, matches Netman.h */
{ return crt_rand_impl(); }
extern "C" int32_t CRT_rand_c_symbol(void) __asm__("CRT_rand");
extern "C" int32_t CRT_rand_c_symbol(void) { return crt_rand_impl(); }

/* CRT_wcsstr (0x471480) — byte-wise substring search (the name is a
 * legacy misnomer; Building.cpp and UIPANEL_Draw.cpp use it on bytes).
 * The decompiled TUs declare it in extern "C" blocks. */
extern "C" const wchar_t* CRT_wcsstr(const wchar_t* a, const wchar_t* b)
{
    if (a == nullptr || b == nullptr || *b == L'\0') return a;
    const unsigned char* hay = reinterpret_cast<const unsigned char*>(a);
    const unsigned char* needle = reinterpret_cast<const unsigned char*>(b);
    while (*hay != 0) {
        const unsigned char* h = hay;
        const unsigned char* n = needle;
        while (*h != 0 && *n != 0 && *h == *n) { h++; n++; }
        if (*n == 0) return reinterpret_cast<const wchar_t*>(hay);
        hay++;
    }
    return nullptr;
}

/* Rect helpers the class cone references (documented Win32 semantics). */
/* Win32 rect/string helpers the class cone references (never exercised
 * by the persistence tests).  The decompiled TUs declare these in
 * extern "C" blocks, so the fixtures need C linkage. */
LOUD_FIXTURE(SetRect)
LOUD_FIXTURE(SetRectEmpty)
LOUD_FIXTURE(OffsetRect)
LOUD_FIXTURE(IsRectEmpty)
LOUD_FIXTURE(IntersectRect)
LOUD_FIXTURE(IsCharAlphaNumericA)
LOUD_FIXTURE(LoadStringA)
LOUD_FIXTURE(TileMap_InvalidateRect)
LOUD_FIXTURE(strncpy_crt)
extern "C" {
void SetRect(void*, int, int, int, int)
{ fixture_reached_SetRect(); }
void SetRectEmpty(void*)
{ fixture_reached_SetRectEmpty(); }
/* OffsetRect is declared by Netman.h's Win32 block as
 * int32_t __stdcall OffsetRect(RECT*, int32_t, int32_t); the fixture
 * must match that signature exactly (C-linkage functions cannot be
 * overloaded). */
int32_t __stdcall OffsetRect(RECT*, int32_t, int32_t)
{ fixture_reached_OffsetRect(); return 0; }
int IsRectEmpty(const void*)
{ fixture_reached_IsRectEmpty(); return 1; }
int IntersectRect(void*, const void*, const void*)
{ fixture_reached_IntersectRect(); return 0; }
int IsCharAlphaNumericA(char)
{ fixture_reached_IsCharAlphaNumericA(); return 0; }
int LoadStringA(void*, unsigned int, char*, int)
{ fixture_reached_LoadStringA(); return 0; }
void TileMap_InvalidateRect(void*, int, int, int, int)
{ fixture_reached_TileMap_InvalidateRect(); }
/* MSVC CRT _strncpy referenced by GameObject.cpp (C linkage). */
char* _strncpy(char* dst, const char* src, size_t n)
{ (void)src; (void)n; fixture_reached_strncpy_crt(); return dst; }
}

/* Function-pointer globals the Building cone uses (real trivial impls). */
int g_OffsetRect_impl(void* r, int dx, int dy)
{
    char* b = static_cast<char*>(r);
    int* v = reinterpret_cast<int*>(b);
    v[0] += dx; v[1] += dy; v[2] += dx; v[3] += dy;
    return 1;
}
int g_IsRectEmpty_impl(const void* r)
{
    const int* v = static_cast<const int*>(r);
    return (v[0] >= v[2] || v[1] >= v[3]) ? 1 : 0;
}
int (*g_OffsetRect)(void*, int, int) = &g_OffsetRect_impl;
int (*g_IsRectEmpty)(const void*) = &g_IsRectEmpty_impl;

/* Free-function helpers referenced by the class cone (Building.o and
 * friends); never exercised by the persistence tests. */
LOUD_FIXTURE(GameObject_DtorBody)
LOUD_FIXTURE(GameObject_Update)
LOUD_FIXTURE(GameObject_StopSound)
LOUD_FIXTURE(Game_CheckTimeInRange)
LOUD_FIXTURE(Game_SelectGameObject)
LOUD_FIXTURE(UIPANEL_Blit)
LOUD_FIXTURE(Math_DistSquared)
LOUD_FIXTURE(Math_PointOnLineSegment)
LOUD_FIXTURE(CRT_localtime)
LOUD_FIXTURE(TileMap_GetObjectAt)
LOUD_FIXTURE(TileMap_FindTileByType)
LOUD_FIXTURE(World_DeserializeMap)
LOUD_FIXTURE(AssetMgr_ReadPairValue)
LOUD_FIXTURE(Building_CheckPlacement)
LOUD_FIXTURE(Vehicle_SetState_free)
LOUD_FIXTURE(Vehicle_LoadSounds)
LOUD_FIXTURE(Vehicle_GetOccupantCount)
LOUD_FIXTURE(GameAudio_AllocChannel)
LOUD_FIXTURE(CGWND_AudioChannel_Play)
LOUD_FIXTURE(CGWND_AudioChannel_Stop)
LOUD_FIXTURE(CGWND_AudioChannel_Release)
LOUD_FIXTURE(CGWND_AudioChannel_UpdatePosition)
LOUD_FIXTURE(RESMGR_ReleaseSoundResource)
void GameObject_DtorBody(void*) { fixture_reached_GameObject_DtorBody(); }
void GameObject_Update(void*) { fixture_reached_GameObject_Update(); }
void GameObject_StopSound(void*, int) { fixture_reached_GameObject_StopSound(); }
int Game_CheckTimeInRange(int*, int*, int*) { fixture_reached_Game_CheckTimeInRange(); return 0; }
void Game_SelectGameObject(void*, void*) { fixture_reached_Game_SelectGameObject(); }
void UIPANEL_Blit(void*, int, int, int, int, void*, int, int, int, int, unsigned int)
{ fixture_reached_UIPANEL_Blit(); }
int Math_DistSquared(int, int, int, int) { fixture_reached_Math_DistSquared(); return 0; }
int Math_PointOnLineSegment(int, int, int, int, int, int)
{ fixture_reached_Math_PointOnLineSegment(); return 0; }
tm* CRT_localtime(unsigned int*) { fixture_reached_CRT_localtime(); return nullptr; }
tm* CRT_localtime(const long*) { fixture_reached_CRT_localtime(); return nullptr; }
void* TileMap_GetObjectAt(TileMap*, int, int, int)
{ fixture_reached_TileMap_GetObjectAt(); return nullptr; }
int TileMap_FindTileByType(void*, int, int, int, int)
{ fixture_reached_TileMap_FindTileByType(); return 0; }
void World_DeserializeMap(void*, int) { fixture_reached_World_DeserializeMap(); }
unsigned int AssetMgr_ReadPairValue(void*, unsigned int, unsigned int)
{ fixture_reached_AssetMgr_ReadPairValue(); return 0; }
int Building_CheckPlacement(Building*, int, int)
{ fixture_reached_Building_CheckPlacement(); return 0; }
void Vehicle_SetState(void*, int) { fixture_reached_Vehicle_SetState_free(); }
void Vehicle_LoadSounds(void*, int*, char) { fixture_reached_Vehicle_LoadSounds(); }
int Vehicle_GetOccupantCount(void*) { fixture_reached_Vehicle_GetOccupantCount(); return 0; }
int GameAudio_AllocChannel(void*, int, void**, int, int, int, int)
{ fixture_reached_GameAudio_AllocChannel(); return 0; }
void CGWND_AudioChannel_Play(void*) { fixture_reached_CGWND_AudioChannel_Play(); }
void CGWND_AudioChannel_Stop(void*) { fixture_reached_CGWND_AudioChannel_Stop(); }
void CGWND_AudioChannel_Release(void*) { fixture_reached_CGWND_AudioChannel_Release(); }
void CGWND_AudioChannel_UpdatePosition(void*, int, int)
{ fixture_reached_CGWND_AudioChannel_UpdatePosition(); }
void RESMGR_ReleaseSoundResource(void*) { fixture_reached_RESMGR_ReleaseSoundResource(); }

/* ---- Temp-dir helpers ---- */

#if defined(__GNUC__)
#define PERSISTENCE_TEST_UNUSED __attribute__((unused))
#else
#define PERSISTENCE_TEST_UNUSED
#endif

/* Fresh empty directory under build/test-artifacts/persistence-XXXXXX.
 * Tests copy shipped fixtures here; shipped art-res is never written. */
static PERSISTENCE_TEST_UNUSED std::string make_temp_dir()
{
    const char* root = "build/test-artifacts";
    ::mkdir(root, 0755);
    std::string tmpl = std::string(root) + "/persistence-XXXXXX";
    char* buf = new char[tmpl.size() + 1];
    std::strcpy(buf, tmpl.c_str());
    char* made = ::mkdtemp(buf);
    std::string result = (made != nullptr) ? std::string(made) : std::string(root);
    delete[] buf;
    return result;
}

/* Copy a shipped fixture into the temp dir (never mutate the original). */
static PERSISTENCE_TEST_UNUSED bool copy_fixture(const std::string& src, const std::string& dst)
{
    FILE* in = std::fopen(src.c_str(), "rb");
    if (in == nullptr) return false;
    FILE* out = std::fopen(dst.c_str(), "wb");
    if (out == nullptr) { std::fclose(in); return false; }
    char buf[8192];
    size_t n;
    bool ok = true;
    while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) {
        if (std::fwrite(buf, 1, n, out) != n) { ok = false; break; }
    }
    std::fclose(in);
    std::fclose(out);
    return ok;
}

/* Shipped fixture root: LEGO_LOCO_DATA or the relative worktree symlink. */
static PERSISTENCE_TEST_UNUSED std::string fixture_root()
{
    const char* env = std::getenv("LEGO_LOCO_DATA");
    return (env != nullptr && *env != '\0')
        ? std::string(env) + "/art-res"
        : std::string("lego-loco-unpacked/art-res");
}
