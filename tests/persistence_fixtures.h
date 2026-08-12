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
#include <sys/mman.h>
#include <unistd.h>

/* Complete class definitions needed by the fixtures below. */
#include "core/Game.h"
#include "core/Entity.h"
#include "core/BuildingMgrObjectGroup.h"
#include "game/World.h"
#include "game/Vehicle.h"
#include "game/Building.h"
#include "game/BuildingMgr.h"
#include "game/GameVehicle.h"
#include "game/ResdataGameVehicle.h"
#include "ui/HelpPageNode.h"
#include "network/Netman.h"
#include "world/tilemap.h"
#include "resources/ResourceManager.h"

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
Entity*  g_selected_building = nullptr;  /* 0x4855B0 (Entity* — see world/tilemap.h) */
int32_t  g_player_id = 0;                 /* 0x4AAD46 */
int32_t  g_player_color = 0;              /* 0x4AAD48 */
int32_t  g_in_build_mode = 0;             /* 0x4FD199 */
uint8_t  g_allow_building_placement = 0;  /* 0x4FD3DC — loader/building placement
                                             flag (DISTINCT from g_is_town_mode
                                             0x485328; the two were once conflated
                                             under one C++ symbol).  The original
                                             is a BSS global — zero-initialized; the
                                             loader saves and restores it. */
uint8_t  g_is_town_mode = 0;              /* 0x485328 — town/tilemap flag */
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

/* g_trackSegmentOffsets (0x47E410) — 12-entry circular-track segment
 * offset table, read only by TrackPos_IsObjectBetween (game/TrackPos.cpp).
 * This test cone links the real TrackPos.o for TrackPos_Init/BaseInit
 * (needed by input/InputMgr.cpp's INPUT_ResetLoadEventNode/
 * INPUT_ResetTimeEventNode); IsObjectBetween itself is unreachable from
 * anything this test exercises, so a zeroed fixture is sufficient. */
uint16_t g_trackSegmentOffsets[12] = {0};

/* g_resmgr — the canonical static object (0x4855E8); never initialized
 * in these tests.  GetById below returns nullptr (a fresh manager), so
 * INPUT_FindObjectAt's default mode (0x41E498) resolves its +0x158 pick
 * range only against the test hook g_fixture_getbyid_count.  On x86_64
 * the binary's int32 pointer return only round-trips for addresses
 * below 2^31; the hook therefore maps the fake resource with MAP_32BIT
 * (Linux x86_64) when a range test is requested. */
ResourceManager g_resmgr;
uint16_t g_fixture_getbyid_count = 0;      /* test hook: +0x158 count of the fake
                                             GetById result (0 = not found) */
static uint8_t* fixture_fake_resource()
{
    static uint8_t* s_fake = nullptr;
    if (s_fake != nullptr) {
        return s_fake;
    }
#ifdef __linux__
    void* p = ::mmap(nullptr, 0x4000, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (p != MAP_FAILED) {
        s_fake = static_cast<uint8_t*>(p);
    }
#else
    (void)0;
#endif
    return s_fake;
}
int32_t ResourceManager::GetById(int32_t resId)
{
    (void)resId;
    if (g_fixture_getbyid_count == 0) {
        return 0;
    }
    uint8_t* fake = fixture_fake_resource();
    if (fake == nullptr) {
        return 0;   /* 32-bit region unavailable: behave as not-found */
    }
    *reinterpret_cast<uint16_t*>(fake + 0x158) = g_fixture_getbyid_count;
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(fake));
}

/* g_tilemap — typed TileMap* (tilemap.h); kept null in these tests. */
TileMap* g_tilemap = nullptr;

/* ---- Fail-loud singletons (abort when reached) ---- */

/* Recording mode for the offset/edge persistence tests.  When FALSE
 * (default) the TileMap/Netman fixture methods below abort if reached
 * (the guarded host paths must never reach them in the regular tests).
 * The offset/edge regressions set it TRUE so INPUT_LoadSaveFile /
 * INPUT_LoadWorld can drive the real placement and scenario-2 edge
 * blocks against a fake TileMap/Netman that CAPTURES the calls.
 *
 * g_fixture_edge_result is returned by every Netman::Check*Edge when
 * recording (the scenario-2 block runs the four edge checks in the
 * order Up, Down, Right, Left). */
bool   g_fixture_record_tilemap = false;
bool   g_fixture_record_netman  = false;
int32_t g_fixture_edge_result   = 0;
int    g_fixture_find_count     = 0;
int    g_fixture_find_id[16];
short  g_fixture_find_x[16];
short  g_fixture_find_y[16];
int    g_fixture_full_reset_count = 0;
int    g_fixture_scroll_count   = 0;

/* TileMap methods — only reached when host_placement_available() is true
 * or a guarded singleton check regresses.  In recording mode the calls
 * are captured instead (the fixture bodies never touch `this`). */
LOUD_FIXTURE(TileMap_FindObject)
LOUD_FIXTURE(TileMap_FullReset)
LOUD_FIXTURE(TileMap_ScrollTo)
LOUD_FIXTURE(TileMap_InvalidateDirtyRects)
/* TileMap::GetObjectAt / TileMap::FindNearestObject (member-method form)
 * are newly referenced by game/Building.cpp (StepToward/TeleportTo/
 * AddOccupant/RemoveOccupant/FindNearbyObject/CheckPlacementCollision),
 * fixing a call-0 landmine — see docs/landmine-sweep-worklist.md's
 * "TileMap_GetObjectAt (cluster A)" / "TileMap_FindTileByType" rows.
 * The old, now-superseded `TileMap_GetObjectAt(TileMap*, int, int, int)`
 * free-function fixture further below reuses this same
 * fixture_reached_TileMap_GetObjectAt helper (declared here, ahead of
 * both use sites, since this is the first one in file order). */
LOUD_FIXTURE(TileMap_GetObjectAt)
LOUD_FIXTURE(TileMap_FindNearestObject)
int* TileMap::FindObject(unsigned int id, short x, short y, char u, unsigned int m)
{
    (void)u; (void)m;
    if (!g_fixture_record_tilemap) {
        fixture_reached_TileMap_FindObject();
        return nullptr;
    }
    if (g_fixture_find_count < 16) {
        g_fixture_find_id[g_fixture_find_count] = static_cast<int>(id);
        g_fixture_find_x[g_fixture_find_count] = x;
        g_fixture_find_y[g_fixture_find_count] = y;
        g_fixture_find_count++;
    }
    return nullptr;
}
void TileMap::FullReset()
{
    if (!g_fixture_record_tilemap) {
        fixture_reached_TileMap_FullReset();
        return;
    }
    g_fixture_full_reset_count++;
}
void* TileMap::ScrollTo(TileMapObject*, int)
{
    if (!g_fixture_record_tilemap) {
        fixture_reached_TileMap_ScrollTo();
        return nullptr;
    }
    g_fixture_scroll_count++;
    return nullptr;
}
void TileMap::InvalidateDirtyRects(char)
{
    if (!g_fixture_record_tilemap) {
        fixture_reached_TileMap_InvalidateDirtyRects();
    }
}
void* TileMap::GetObjectAt(short, short, short)
{ fixture_reached_TileMap_GetObjectAt(); return nullptr; }
void* TileMap::FindNearestObject(unsigned short, int, int, int)
{ fixture_reached_TileMap_FindNearestObject(); return nullptr; }

/* Fake TileMap/Netman storage for the recording tests: a zeroed
 * instance is a safe stand-in because every fixture method above never
 * touches `this`.  g_fixture_netman is pre-set to m_gameMode == 2 so
 * INPUT_LoadWorld's scenario-2 block runs. */
TileMap::TileMap()
    : width(0), height(0), viewport_rect{}, viewport_x(0), viewport_y(0),
      center_x(0), center_y(0), viewport_center_x(0), viewport_center_y(0),
      drag_start_x(0), drag_start_y(0), scroll_drag_active(0), _pad_3D(0),
      tile_count_x(0), tile_count_y(0), occupancy_bitmap(nullptr),
      asset_load_ptr(nullptr), asset_enum_ptr(nullptr), update_complete(0),
      surface_locked(0)
{ std::memset(static_cast<void*>(this), 0, sizeof(TileMap)); }
TileMap::~TileMap() {}
Netman::Netman()
    : m_bInit(0), m_playerSlotCount(0), m_playerRows(0), m_playerCols(0),
      m_gameMode(0), m_bFlag1(0), m_currentSlot(nullptr), m_mySlotIndex(0),
      m_myDpId(0), m_field_7D8(0), m_buildingList(nullptr),
      m_vehicleList(nullptr), m_field_7E4(0), m_hostLastSerializedVehicle(nullptr),
      m_field_7E8(0), m_tickCounter(0), m_timeout(0), m_sendTimer(0),
      m_visibility(0), m_tickInterval(0), m_timeoutState(0)
{
    std::memset(static_cast<void*>(this), 0, sizeof(Netman));
    m_gameMode = 2;   /* scenario 2 — joined game */
}
Netman::~Netman() {}
static TileMap g_fixture_tilemap;   /* construct via the ctor fixture above */
static Netman  g_fixture_netman;

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
{
    if (!g_fixture_record_netman) { fixture_reached_Netman_CheckUpEdge(); }
    return g_fixture_edge_result;
}
int32_t Netman::CheckDownEdge()
{
    if (!g_fixture_record_netman) { fixture_reached_Netman_CheckDownEdge(); }
    return g_fixture_edge_result;
}
int32_t Netman::CheckLeftEdge()
{
    if (!g_fixture_record_netman) { fixture_reached_Netman_CheckLeftEdge(); }
    return g_fixture_edge_result;
}
int32_t Netman::CheckRightEdge()
{
    if (!g_fixture_record_netman) { fixture_reached_Netman_CheckRightEdge(); }
    return g_fixture_edge_result;
}

/* Vehicle methods referenced by the class cone (Building.o / World.o) —
 * not exercised by the persistence tests. */
LOUD_FIXTURE(Vehicle_UpdatePosition)
LOUD_FIXTURE(Vehicle_InitOccupant)
LOUD_FIXTURE(Vehicle_Stop)
LOUD_FIXTURE(Vehicle_FindPath)
LOUD_FIXTURE(Vehicle_IsMoving)
LOUD_FIXTURE(Vehicle_SetState)
LOUD_FIXTURE(Vehicle_GetOccupantCount)
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
/* Newly referenced by game/Building.cpp::FindPathToTarget, fixing the
 * Vehicle_GetOccupantCount free-function call-0 landmine (docs/landmine-
 * sweep-worklist.md) by calling the real typed method instead. Not
 * exercised by the persistence tests (they never construct a Building). */
uint8_t Vehicle::GetOccupantCount()
{ fixture_reached_Vehicle_GetOccupantCount(); return 0; }

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
void PlaySound(int id);
void PlaySound(int id)
{ g_last_play_sound_id = id; }

/* Tooltip entry points: g_tooltip_mgr stays null on the host, so the
 * guarded host paths skip them (loud log).  Reaching these means the
 * guard regressed. */
LOUD_FIXTURE(UI_CleanupTooltips)
LOUD_FIXTURE(UI_HideTooltip)
void UI_CleanupTooltips(void*)
{ fixture_reached_UI_CleanupTooltips(); }
void UI_HideTooltip(void*);
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
unsigned int GetResourceType(int id);
unsigned int GetResourceType(int id)
{
    return GetResourceType(static_cast<unsigned int>(id));
}

/* RESDATA tile predicates — reached only by the gated placement path
 * (host_placement_available) or the typed entity scan.  InputMgr.o now
 * references the canonical int32_t __fastcall form (0x44BD30); a fresh
 * manager never reaches it, so it stays fail-loud. */
LOUD_FIXTURE(RESDATA_IsBuildingTile)
LOUD_FIXTURE(RESDATA_IsRoadTile)
uint8_t __fastcall RESDATA_IsBuildingTile(int32_t)
{ fixture_reached_RESDATA_IsBuildingTile(); return 0; }
int RESDATA_IsRoadTile(int)
{ fixture_reached_RESDATA_IsRoadTile(); return 0; }
int RESDATA_IsRoadTile(void*);
int RESDATA_IsRoadTile(void*)
{ fixture_reached_RESDATA_IsRoadTile(); return 0; }

/* AssetMgr_ReadPairValue (0x45DD80) — newly referenced by
 * game/Building.cpp::StepToward/FindNearestConnectionNode, fixing the
 * void*-first-param call-0 landmine (docs/landmine-sweep-worklist.md,
 * "AssetMgr_ReadPairValue") by matching the real AssetMgr*-typed
 * signature (resources/AssetMgr.h). AssetMgr is only forward-declared —
 * resources/AssetMgr.h itself isn't included in this cone — the fixture
 * never touches `self`. The old void*-shaped overload further below
 * reuses this same fixture_reached_AssetMgr_ReadPairValue helper
 * (declared here, ahead of both use sites). Not exercised (this cone
 * never constructs a Building). */
struct AssetMgr;
LOUD_FIXTURE(AssetMgr_ReadPairValue)
uint8_t AssetMgr_ReadPairValue(AssetMgr*, uint32_t, uint32_t);
uint8_t AssetMgr_ReadPairValue(AssetMgr*, uint32_t, uint32_t)
{ fixture_reached_AssetMgr_ReadPairValue(); return 0xFF; }

/* Host resource bridge: no resources are loaded in the persistence
 * tests, so GetById returns nullptr (a fresh host manager).
 *
 * A test that links the REAL resources/resource_manager_sdl3.cpp (to
 * construct real placed entities, e.g. tests/input_place_object_test.cpp)
 * must define PERSISTENCE_FIXTURES_REAL_RESOURCE_MANAGER before including
 * this header, to skip these fakes and the loco::assets::* ones below --
 * otherwise the fakes' definitions collide with the real ones at link
 * time (multiple definition). */
#ifndef PERSISTENCE_FIXTURES_REAL_RESOURCE_MANAGER
void* ResourceManager_GetById(void*, int);
void* ResourceManager_GetById(void*, int) { return nullptr; }
void* ResourceManager_GetById(void*, unsigned int) { return nullptr; }

/* Host resource-adapter accessors (resources/resource_manager_sdl3.h),
 * referenced by GameObject.o's Entity::InitBase/SetAnimState/SetFrame/
 * ~Entity host branches (see PROGRESS.md's InitBase host-safety entry) and
 * by ResdataGameVehicle.o's tile-type/resource-id host branches (see the
 * entity-update-host-guard/RESDATA_Is*Tile follow-up entries).
 * Not exercised: ResourceManager_GetById above always returns nullptr in
 * this cone, so `resource` is always null before any of these would be
 * called -- these exist only to satisfy the link. Declared here rather
 * than via #include "resources/resource_manager_sdl3.h" because that
 * header's ResourceManager_Init(void*) -> int declaration collides with
 * network/Netman.h's pre-existing ResourceManager_Init(void*) -> void
 * (a real, separate mismatch between those two headers, out of scope
 * here -- this cone never calls either). */
namespace loco::assets {
class SpriteResource;
class SpriteBitmap;
struct SpriteMetadata;
bool is_host_sprite_resource(const void*) { return false; }
bool sprite_tile_type_byte(const void*, uint8_t*) { return false; }
bool sprite_leisure_destination_byte(const void*, uint8_t*) { return false; }
uint32_t sprite_resource_id(const SpriteResource*) { return 0; }
SpriteBitmap* sprite_bitmap(SpriteResource*) { return nullptr; }
uint32_t sprite_width(const SpriteResource*) { return 0; }
uint32_t sprite_height(const SpriteResource*) { return 0; }
}  // namespace loco::assets
const loco::assets::SpriteMetadata* ResourceManager_GetSpriteMetadata(void*) { return nullptr; }
#endif  /* !PERSISTENCE_FIXTURES_REAL_RESOURCE_MANAGER */

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
extern "C" const wchar_t* CRT_wcsstr(const wchar_t* a, const wchar_t* b);
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
 * by the persistence tests -- ResourceManager_GetById above always
 * returns null, so no real GameObject/Entity ever gets far enough to
 * call these).  The decompiled TUs declare these in extern "C" blocks,
 * so the fixtures need C linkage.
 *
 * A test defining PERSISTENCE_FIXTURES_REAL_RESOURCE_MANAGER (see above)
 * DOES construct real GameObject/Entity instances, so SetRect/SetRectEmpty/
 * OffsetRect/IsRectEmpty/IntersectRect/IsCharAlphaNumericA/
 * TileMap_InvalidateRect/_strncpy get real minimal bodies instead of loud
 * fixtures (matching tests/entity_host_resource_test.cpp's already-
 * validated versions) -- LoadStringA stays loud unconditionally, since
 * nothing on the construction path touches PE string-table lookups. */
LOUD_FIXTURE(LoadStringA)
extern "C" {
int LoadStringA(void*, unsigned int, char*, int);
int LoadStringA(void*, unsigned int, char*, int)
{ fixture_reached_LoadStringA(); return 0; }
}

#ifndef PERSISTENCE_FIXTURES_REAL_RESOURCE_MANAGER
LOUD_FIXTURE(SetRect)
LOUD_FIXTURE(SetRectEmpty)
LOUD_FIXTURE(OffsetRect)
LOUD_FIXTURE(IsRectEmpty)
LOUD_FIXTURE(IntersectRect)
LOUD_FIXTURE(IsCharAlphaNumericA)
LOUD_FIXTURE(TileMap_InvalidateRect)
LOUD_FIXTURE(strncpy_crt)
extern "C" {
void SetRect(void*, int, int, int, int);
void SetRect(void*, int, int, int, int)
{ fixture_reached_SetRect(); }
void SetRectEmpty(void*);
void SetRectEmpty(void*)
{ fixture_reached_SetRectEmpty(); }
/* OffsetRect is declared by Netman.h's Win32 block as
 * int32_t __stdcall OffsetRect(RECT*, int32_t, int32_t); the fixture
 * must match that signature exactly (C-linkage functions cannot be
 * overloaded). */
int32_t __stdcall OffsetRect(RECT*, int32_t, int32_t)
{ fixture_reached_OffsetRect(); return 0; }
int IsRectEmpty(const void*);
int IsRectEmpty(const void*)
{ fixture_reached_IsRectEmpty(); return 1; }
/* Must match world/tilemap.h's extern "C" declaration exactly
 * (RECT*, const RECT*, const RECT*) — C-linkage functions cannot be
 * overloaded, and this fixture is linked alongside tilemap.h's callers. */
int IntersectRect(RECT*, const RECT*, const RECT*)
{ fixture_reached_IntersectRect(); return 0; }
int IsCharAlphaNumericA(char);
int IsCharAlphaNumericA(char)
{ fixture_reached_IsCharAlphaNumericA(); return 0; }
void TileMap_InvalidateRect(void*, int, int, int, int);
void TileMap_InvalidateRect(void*, int, int, int, int)
{ fixture_reached_TileMap_InvalidateRect(); }
/* MSVC CRT _strncpy referenced by GameObject.cpp (C linkage). */
char* _strncpy(char* dst, const char* src, size_t n);
char* _strncpy(char* dst, const char* src, size_t n)
{ (void)src; (void)n; fixture_reached_strncpy_crt(); return dst; }
}
#else
/* void*-typed parameters here (not RECT*) so these remain distinct
 * C-linkage overloads from world/tilemap.h's own (mixed C/C++ linkage,
 * RECT*-typed) declarations of the same names -- matching the approach
 * the loud-fixture versions above already relied on. */
extern "C" {
void SetRect(void* r, int left, int top, int right, int bottom)
{ RECT* rect = static_cast<RECT*>(r);
  if (rect) { rect->left = left; rect->top = top; rect->right = right; rect->bottom = bottom; } }
void SetRectEmpty(void* r)
{ RECT* rect = static_cast<RECT*>(r);
  if (rect) { rect->left = rect->top = rect->right = rect->bottom = 0; } }
int32_t __stdcall OffsetRect(RECT* r, int32_t dx, int32_t dy)
{ if (r) { r->left += dx; r->right += dx; r->top += dy; r->bottom += dy; } return 1; }
int IsRectEmpty(const void* r)
{ const RECT* rect = static_cast<const RECT*>(r);
  return (rect == nullptr || rect->left >= rect->right || rect->top >= rect->bottom) ? 1 : 0; }
int IntersectRect(RECT*, const RECT*, const RECT*) { return 0; }
int IsCharAlphaNumericA(char) { return 0; }
void TileMap_InvalidateRect(void*, int, int, int, int) {}
char* _strncpy(char* dst, const char* src, size_t n)
{ (void)src; (void)n; return dst; }
}
#endif  /* PERSISTENCE_FIXTURES_REAL_RESOURCE_MANAGER */

/* Function-pointer globals the Building cone uses (real trivial impls). */
int g_OffsetRect_impl(void* r, int dx, int dy);
int g_OffsetRect_impl(void* r, int dx, int dy)
{
    char* b = static_cast<char*>(r);
    int* v = reinterpret_cast<int*>(b);
    v[0] += dx; v[1] += dy; v[2] += dx; v[3] += dy;
    return 1;
}
int g_IsRectEmpty_impl(const void* r);
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
LOUD_FIXTURE(TileMap_FindTileByType)
LOUD_FIXTURE(World_DeserializeMap)
LOUD_FIXTURE(Building_CheckPlacement)
LOUD_FIXTURE(Vehicle_SetState_free)
LOUD_FIXTURE(Vehicle_LoadSounds)
LOUD_FIXTURE(GameAudio_AllocChannel)
LOUD_FIXTURE(CGWND_AudioChannel_Play)
LOUD_FIXTURE(CGWND_AudioChannel_Stop)
LOUD_FIXTURE(CGWND_AudioChannel_Release)
LOUD_FIXTURE(CGWND_AudioChannel_UpdatePosition)
LOUD_FIXTURE(RESMGR_ReleaseSoundResource)
void GameObject_DtorBody(void*);
void GameObject_DtorBody(void*) { fixture_reached_GameObject_DtorBody(); }
void GameObject_Update(void*);
void GameObject_Update(void*) { fixture_reached_GameObject_Update(); }
void GameObject_StopSound(void*, int);
void GameObject_StopSound(void*, int) { fixture_reached_GameObject_StopSound(); }
int Game_CheckTimeInRange(int*, int*, int*) { fixture_reached_Game_CheckTimeInRange(); return 0; }
void Game_SelectGameObject(void*, void*);
void Game_SelectGameObject(void*, void*) { fixture_reached_Game_SelectGameObject(); }
/* Real def: ui/UIPANEL_Surface.cpp, bool(void*,uint32_t,uint32_t,int32_t,
 * uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t). Was a
 * uniform-int shape that happened to match the call-0-landmine-era
 * declaration in core/GameObject.cpp; updated alongside that file's
 * real-signature fix (docs/landmine-sweep-worklist.md, UIPANEL_Blit
 * caller cluster) so this fixture keeps satisfying the link. */
bool UIPANEL_Blit(void*, uint32_t, uint32_t, int32_t, uint32_t, void*, uint32_t, uint32_t, int32_t, uint32_t, uint32_t)
{ fixture_reached_UIPANEL_Blit(); return false; }
int Math_DistSquared(int, int, int, int) { fixture_reached_Math_DistSquared(); return 0; }
int Math_PointOnLineSegment(int, int, int, int, int, int);
int Math_PointOnLineSegment(int, int, int, int, int, int)
{ fixture_reached_Math_PointOnLineSegment(); return 0; }
tm* CRT_localtime(unsigned int*);
tm* CRT_localtime(unsigned int*) { fixture_reached_CRT_localtime(); return nullptr; }
tm* CRT_localtime(const long*);
tm* CRT_localtime(const long*) { fixture_reached_CRT_localtime(); return nullptr; }
void* TileMap_GetObjectAt(TileMap*, int, int, int);
void* TileMap_GetObjectAt(TileMap*, int, int, int)
{ fixture_reached_TileMap_GetObjectAt(); return nullptr; }
int TileMap_FindTileByType(void*, int, int, int, int);
int TileMap_FindTileByType(void*, int, int, int, int)
{ fixture_reached_TileMap_FindTileByType(); return 0; }
void World_DeserializeMap(void*, int);
void World_DeserializeMap(void*, int) { fixture_reached_World_DeserializeMap(); }
unsigned int AssetMgr_ReadPairValue(void*, unsigned int, unsigned int);
unsigned int AssetMgr_ReadPairValue(void*, unsigned int, unsigned int)
{ fixture_reached_AssetMgr_ReadPairValue(); return 0; }
int Building_CheckPlacement(Building*, int, int);
int Building_CheckPlacement(Building*, int, int)
{ fixture_reached_Building_CheckPlacement(); return 0; }
void Vehicle_SetState(void*, int);
void Vehicle_SetState(void*, int) { fixture_reached_Vehicle_SetState_free(); }
void Vehicle_LoadSounds(void*, int*, char);
void Vehicle_LoadSounds(void*, int*, char) { fixture_reached_Vehicle_LoadSounds(); }
int Vehicle_GetOccupantCount(void*);
int Vehicle_GetOccupantCount(void*) { fixture_reached_Vehicle_GetOccupantCount(); return 0; }
int GameAudio_AllocChannel(void*, int, void**, int, int, int, int);
int GameAudio_AllocChannel(void*, int, void**, int, int, int, int)
{ fixture_reached_GameAudio_AllocChannel(); return 0; }
void CGWND_AudioChannel_Play(void*);
void CGWND_AudioChannel_Play(void*) { fixture_reached_CGWND_AudioChannel_Play(); }
void CGWND_AudioChannel_Stop(void*);
void CGWND_AudioChannel_Stop(void*) { fixture_reached_CGWND_AudioChannel_Stop(); }
void CGWND_AudioChannel_Release(void*);
void CGWND_AudioChannel_Release(void*) { fixture_reached_CGWND_AudioChannel_Release(); }
void CGWND_AudioChannel_UpdatePosition(void*, int, int);
void CGWND_AudioChannel_UpdatePosition(void*, int, int)
{ fixture_reached_CGWND_AudioChannel_UpdatePosition(); }
void RESMGR_ReleaseSoundResource(void*);
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
