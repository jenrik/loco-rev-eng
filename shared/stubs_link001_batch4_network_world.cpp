/**
 * stubs_link001_batch4_network_world.cpp
 *
 * LINK-001 call-0 landmine sweep — batch 4 (Network/World/EditorState/
 * Vehicle family). Every symbol here is a linker-unresolved free-function
 * reference (`-Wl,--unresolved-symbols=ignore-all` binds it to address 0)
 * from a specific caller assigned to this batch. Per CLAUDE.md's stub
 * policy and the batch's own constraints, no existing file is edited —
 * every fix lives in this one new translation unit, either as a thin
 * bridge to an already-integrated real implementation (when the caller's
 * own declaration just has the wrong shape/linkage for something that
 * already exists) or as a loud/safe-default deferred stub (when the real
 * logic is not yet ported into the C++ tree).
 *
 * Status: TRANSCRIBED (bridges) / STUB (deferred pieces, marked below)
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <new>

#include "../game/World.h"
#include "../game/Vehicle.h"
#include "../core/VehicleEditor.h"
#include "../world/EditorState.h"
#include "../world/tilemap.h"
#include "../network/NetworkPlayerList.h"
#include "../network/NetmanTypes.h"
#include "../network/DPlayManager.h"
#include "../ui/GameSetupPanel.h"
#include "../ui/LayoutListNode.h"
#include "../resources/ResourceManager.h"

/* ==================================================================== */
/* Shared externs (matching the exact shapes already used by sibling    */
/* files in these subsystems — see network/NetmanTypes.h's own header   */
/* comment on why these are hand-declared rather than pulled in via     */
/* network/Netman.h, which gives ~13 unrelated free functions the wrong */
/* (C++) linkage and would collide with this batch's other fixes).      */
/*                                                                        */
/* NOTE: world/tilemap.h (included above, for TileMap_GetObjectAt)      */
/* already declares `g_netman` as `void*` (not `Netman*`) and already   */
/* declares `operator_new`/`GLOBAL_free` — redeclaring any of those      */
/* with a different type here is a hard conflicting-declaration error,   */
/* so this file casts `g_netman` locally instead of re-typing the global.*/
/* ==================================================================== */

extern void*   _g_train;                            /* 0x4A9990-adjacent TrainSubsystem singleton */
extern char*   _g_netman_state;                     /* 0x4FD3A8 — host/join menu selection byte
                                                        blob; see ui/GameSetupPanel.cpp /
                                                        ui/EditWindow.cpp for the established
                                                        `_g_netman_state[8] != 0` "network mode"
                                                        idiom this file follows. */
void NETMAN_StartClientSession();                   /* real def: network/Netman.cpp, 0x43F030 */
void DPLAY_CopyPlayerData(void* dstSlot, const void* packet); /* real def: network/Netman.cpp, 0x4426D0 */
void Train_QueueMessage(void* train, TrainMessage* msg);      /* real def: game/Train_network.cpp, 0x4393D0 */
/* Real def: ui/UIPANEL.cpp:0x426B90, void(void* self, int hdc,
 * int unlockParam, uint8_t unlockFlag, RECT* restrictRect) — the 2nd
 * param is `int hdc`, not `HWND window`, and the 3rd/5th are `int`/`RECT*`,
 * not `void*`. Was declared (void*, HWND, void*, uint8_t, void*) here,
 * a distinct mangled symbol from the real function (ui/GameSetupPanel_
 * network.cpp had the identical wrong declaration, fixed separately —
 * see docs/landmine-sweep-worklist.md). All 4 call sites below already
 * pass an `hWnd`-as-`hdc` value and integer literal `0` for unlockParam,
 * matching the established pattern once the declaration is corrected. */
void UIPANEL_EndPaintEx(void* panel, int32_t hdc, int32_t unlockParam,
                         uint8_t unlockFlag, RECT* restrictRect); /* 0x426B90 */
uintptr_t SetTimer(HWND hWnd, uintptr_t nIDEvent, UINT uElapse,
                    void (*lpTimerFunc)(HWND, UINT, uintptr_t, DWORD)); /* real def:
                                                        graphics/sdl3_window.cpp */

/* g_netman is declared `void*` by world/tilemap.h; every use below casts
 * through this helper rather than redeclaring the global with a
 * different (conflicting) type. */
static inline Netman* GameNetman() { return static_cast<Netman*>(g_netman); }

/* ====================================================================
 * SYMBOL: World_DeserializeMap(void*, int)
 * CALLER: RESDATA_GameVehicle::~RESDATA_GameVehicle()  (game/ResdataGameVehicle.cpp)
 * ADDRESS: 0x44DAD0
 * ACTION: caller-declaration-is-wrong (real implementation already exists
 *   as World::DeserializeMap(RESDATA_GameVehicle*) in game/World.cpp; the
 *   caller declares a free function instead of calling the method).
 * SHOULD_BE_FIXED_AT: game/ResdataGameVehicle.cpp:24,143 — drop the local
 *   `extern void World_DeserializeMap(void*, int)` and call
 *   `g_world->DeserializeMap(this)` directly (World.cpp already includes
 *   ResdataGameVehicle-compatible forward decls for this).
 * ==================================================================== */
void World_DeserializeMap(void* world, int obj)
{
    if (world == nullptr) return;
    static_cast<World*>(world)->DeserializeMap(
        reinterpret_cast<RESDATA_GameVehicle*>(static_cast<intptr_t>(obj)));
}

/* ====================================================================
 * SYMBOL: World_SerializeObject(void*, int)
 * CALLERS: Netman::RemoveInboundTrain(int32_t), Netman::HandlePlayerLeave(TrainMessage*)
 * ADDRESS: 0x44DA50
 * ACTION: caller-declaration-is-wrong (real implementation already exists
 *   as World::SerializeObject(char) in game/World.cpp; network/Netman.h
 *   declares an unimplemented free function instead — and, worse, the only
 *   thing satisfying that *name* today is shared/defsym_stubs.cpp's
 *   `void* World_SerializeObject = nullptr;` DATA symbol, not a function).
 * SHOULD_BE_FIXED_AT: network/Netman.h:299 — drop the free-function decl;
 *   both call sites (network/Netman.cpp:1569,1677) already have `this`
 *   available and should call `g_world->SerializeObject(...)` (they pass
 *   the literal g_world address as their first argument today).
 * ==================================================================== */
void World_SerializeObject(void* world, int32_t param)
{
    if (world == nullptr) return;
    static_cast<World*>(world)->SerializeObject(static_cast<char>(param));
}

/* ====================================================================
 * SYMBOL: World_FinalizeLoad(void*, Vehicle*, void*, unsigned char)
 * CALLER: Netman::SendChatMessage(Vehicle*)  (network/Netman.cpp)
 * ADDRESS: 0x44DF40
 * ACTION: caller-declaration-is-wrong (real implementation already exists
 *   as World::FinalizeLoad(Vehicle*, int, char) in game/World.cpp).
 * SHOULD_BE_FIXED_AT: network/Netman.h:301 — drop the free-function decl;
 *   the one call site (network/Netman.cpp:1039) already has `this` and
 *   should call `g_world->FinalizeLoad(node, off, dir)` directly.
 * ==================================================================== */
uint8_t World_FinalizeLoad(void* world, Vehicle* node, void* param, uint8_t dir)
{
    if (world == nullptr || node == nullptr) return 0;
    const int packed_coords = static_cast<int>(reinterpret_cast<intptr_t>(param));
    return static_cast<uint8_t>(
        static_cast<World*>(world)->FinalizeLoad(node, packed_coords, static_cast<char>(dir)));
}

/* ====================================================================
 * SYMBOL: World_GetObjectAt(void*)
 * CALLER: Netman::ReceiveGameStart(int, int, Vehicle*)  (network/Netman.cpp)
 * ADDRESS: 0x44E800
 * ACTION: caller-declaration-is-wrong (the real free function
 *   `void __stdcall World_GetObjectAt(Vehicle*)` already exists, fully
 *   implemented, in game/World.cpp/World.h — network/Netman.h just
 *   declares it with the wrong param type, giving it a different mangled
 *   name).
 * SHOULD_BE_FIXED_AT: network/Netman.h:300 — change
 *   `void __stdcall World_GetObjectAt(void* object);` to
 *   `void __stdcall World_GetObjectAt(Vehicle* object);` (matches
 *   game/World.h:401 exactly; the one call site, network/Netman.cpp:1146,
 *   already passes a Vehicle pointer (InboundTrainNode is a type alias
 *   for Vehicle; see network/NetmanTypes.h) as `node`).
 * ==================================================================== */
void World_GetObjectAt(void* object)
{
    World_GetObjectAt(static_cast<Vehicle*>(object));
}

/* ====================================================================
 * SYMBOL: TileMap_GetObjectAt(void*, short, short, short)
 * CALLERS: World::FinalizeLoad(Vehicle*, int, char), World_RenderAll(Vehicle*)
 *   (both in game/World.cpp)
 * ADDRESS: 0x455620
 * ACTION: caller-declaration-is-wrong (the real function is
 *   `TileMap_GetObjectAt(TileMap*, short, short, short)`, an inline
 *   wrapper for TileMap::GetObjectAt in world/tilemap.h; game/World.cpp's
 *   own local `extern void* __thiscall TileMap_GetObjectAt(void* tilemap,
 *   short, short, short)` declaration — the only one visible in that TU,
 *   since it does not #include world/tilemap.h — uses `void*` instead of
 *   `TileMap*`, giving it a different mangled name).
 * SHOULD_BE_FIXED_AT: game/World.cpp:67 — change the local extern's first
 *   param from `void* tilemap` to `TileMap*` (or just #include
 *   "../world/tilemap.h" and drop the local extern entirely; `g_tilemap`
 *   is already declared `TileMap*` there).
 * ==================================================================== */
void* TileMap_GetObjectAt(void* tilemap, short x, short y, short layer)
{
    if (tilemap == nullptr) return nullptr;
    return TileMap_GetObjectAt(static_cast<TileMap*>(tilemap), x, y, layer);
}

/* ====================================================================
 * SYMBOL: NET_RegisterPlayer(void*, void*, int, int)
 * CALLER: Netman::HandleTimeout(Vehicle*)  (network/Netman.cpp:2341,
 *   `NET_RegisterPlayer(_g_dplay, dplay, 1, 0)`)
 * ADDRESS: 0x444D00 (formerly labeled NET_RegisterPlayer, now
 *   NetworkPlayerList::RegisterPlayer — see network/NetworkPlayerList.h)
 * ACTION: caller-declaration-is-wrong (real implementation already exists
 *   as NetworkPlayerList::RegisterPlayer(DPlayManager*, int32_t, int32_t)
 *   -> uint32_t; network/Netman.h's free-function declaration predates
 *   that method being identified/ported and has the wrong return type
 *   (void* vs the real uint32_t) — harmless here since every call site
 *   discards the return value, but still a real declaration/
 *   implementation mismatch). `dplay` in the caller is documented as
 *   "0x4FD3AC — DPLAY/NetworkPlayerList instance", i.e. exactly the
 *   receiver NetworkPlayerList::RegisterPlayer expects; `playerData`
 *   at every real caller (game/Train_network.cpp, town/Town.cpp,
 *   network/Netman.cpp) is a real DPlayManager* (2026-08-14/15).
 * SHOULD_BE_FIXED_AT: network/Netman.h:282 — drop the free-function decl;
 *   network/Netman.cpp:2341 should call
 *   `static_cast<NetworkPlayerList*>(_g_dplay)->RegisterPlayer(dplay, 1, 0)`
 *   directly (return type corrected to uint32_t there too).
 * ==================================================================== */
void* NET_RegisterPlayer(void* dplay, void* playerData, int32_t type, int32_t param)
{
    if (dplay == nullptr) return nullptr;
    const uint32_t result = static_cast<NetworkPlayerList*>(dplay)->RegisterPlayer(
        static_cast<DPlayManager*>(playerData), type, param);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(result));
}

/* ====================================================================
 * SYMBOL: DPLAY_DecodePlayerSlots(const void*)  [extern "C"]
 * CALLER: TrainSubsystem::ProcessMessages()  (game/Train_network.cpp:980)
 * ADDRESS: 0x442750 (per network/Netman.h's own address annotation on the
 *   sibling C++-linkage declaration)
 * ACTION: caller-declaration-is-wrong (a fully real, already-integrated
 *   C++-linkage implementation exists — network/Netman.cpp's
 *   `void* DPLAY_DecodePlayerSlots(const void*)`, matching
 *   network/Netman.h:340's declaration exactly — but game/Train_network.cpp
 *   declares this name *inside* its file-scope `extern "C" { }` block,
 *   giving it C linkage. A C-linkage and a C++-linkage function cannot
 *   share one definition with the same name/params in one translation
 *   unit, so the real body is duplicated here verbatim under extern "C"
 *   rather than referenced by name).
 * SHOULD_BE_FIXED_AT: game/Train_network.cpp — move the
 *   `#ifndef _WIN32 / void* DPLAY_DecodePlayerSlots(const void*); / #endif`
 *   declaration out of the enclosing `extern "C" { }` block (matching how
 *   network/Netman.h itself declares it, outside any extern "C").
 * ==================================================================== */
#ifndef _WIN32
extern "C" void* DPLAY_DecodePlayerSlots(const void* first_compact_slot)
{
    if (first_compact_slot == nullptr) return nullptr;
    auto* slots = static_cast<PlayerSlot*>(operator_new(sizeof(PlayerSlot) * 9));
    if (slots == nullptr) return nullptr;
    /* Binary does not memset the slot array (see network/Netman.cpp's
     * matching comment on its own copy of this logic). */
    const auto* compact = static_cast<const uint8_t*>(first_compact_slot);
    for (int32_t index = 0; index < 9; ++index) {
        DPLAY_CopyPlayerData(&slots[index], compact + index * 0x3C);
    }
    return slots;
}
#endif

/* ====================================================================
 * SYMBOL: NET_FindPlayer(int, uint32_t)  [extern "C"]
 * CALLER: TrainSubsystem::UploadPendingAttachments()  (game/Train_network.cpp:1262)
 * ADDRESS: unknown (no Ghidra xref chased — see rationale below)
 * ACTION: loud-deferred-stub (assert; confirmed unreachable today)
 * RATIONALE: grep-confirmed zero callers of
 *   TrainSubsystem::UploadPendingAttachments() anywhere in the tree
 *   (game/Train.h declares it, game/Train_network.cpp defines it, nothing
 *   else references it) — this exact extern "C" overload of NET_FindPlayer
 *   (distinct from shared/stubs_impl.cpp's already-loud C++-linkage
 *   `NET_FindPlayer(int,int)` used by input/Cursor_new_impls.cpp) is
 *   therefore provably dead code, so assert(0) here is safe per CLAUDE.md's
 *   stub policy exception for provably-unreachable paths.
 * ==================================================================== */
extern "C" int NET_FindPlayer(int mode, uint32_t player_id)
{
    (void)mode; (void)player_id;
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached: NET_FindPlayer(int,uint32_t) [extern \"C\"] via "
                "TrainSubsystem::UploadPendingAttachments — this caller has zero "
                "referrers anywhere in the tree; if that has changed, this needs "
                "a real implementation, not this assert");
    return 0;
}

/* ====================================================================
 * SYMBOL: NET_GetAttFilePath(unsigned short, int, char*)
 * CALLER: Netman::ReceivePlayerName()  (network/Netman.cpp:1014)
 * ADDRESS: 0x445B30 (per game/Train_network.cpp's address annotation on a
 *   different, extern "C"-linked overload of this name — see below)
 * ACTION: safe-default-stub
 * RATIONALE: genuinely missing under this exact C++-linkage
 *   (uint16_t, int32_t, char*) shape — network/Netman.h:290 is the only
 *   declaration with this signature anywhere in the tree, and nothing
 *   defines it. (Three OTHER, incompatible extern "C" overloads of this
 *   bare name already exist — game/Train_network.cpp's (uint32_t,int,char*),
 *   town/Town.cpp's (uint,int,char*), and shared/link_stubs.cpp's
 *   (int32_t) no-op — none of them apply here since this caller's
 *   declaration has C++ linkage.) The real body (0x445B30 family) builds a
 *   PostBag attachment file path via wsprintfA from g_install_path plus a
 *   subdirectory selected by `type`; not reimplemented here (out of this
 *   batch's scope — needs g_install_path/PostBag plumbing this file
 *   doesn't otherwise touch). Per the advisor's guidance for out-param
 *   stubs: terminate the caller's buffer to an empty string before
 *   warning, so PlaySoundFile's subsequent call gets a well-defined (if
 *   useless) path instead of uninitialized stack memory.
 * ==================================================================== */
void NET_GetAttFilePath(uint16_t id, int32_t type, char* outPath)
{
    (void)id; (void)type;
    if (outPath != nullptr) outPath[0] = '\0';
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: NET_GetAttFilePath(uint16_t,int32_t,char*) not implemented "
                         "(TODO: decompile 0x445B30 family) — empty path returned\n");
        warned = true;
    }
}

/* ====================================================================
 * SYMBOL: Train_StartMultiplayer(void)
 * CALLERS: TrainSubsystem::DispatchMessage(void*), TrainSubsystem::HandleJoinMultiplayer(void*)
 * ADDRESS: 0x43A760
 * ACTION: loud-deferred-stub (warn-once, safe no-op)
 * RATIONALE: genuinely missing (game/Train.h's `void __cdecl
 *   Train_StartMultiplayer(void);` is plain C++ linkage; the only existing
 *   definitions anywhere — shared/link_stubs.cpp's `(void*,int32_t)` — are
 *   extern "C", a different symbol). Decompiled at 0x43A760: builds a
 *   DirectPlay session address from config/g_joinInfo and calls
 *   DirectPlay_HandleMessages — substantial, not-yet-ported logic (depends
 *   on g_config_ini, g_dplay_peer's raw DPLAY_SessionData-adjacent fields,
 *   DirectPlay_HandleMessages). Reachable: network/Netman.cpp:1210 routes
 *   real messages into TrainSubsystem::DispatchMessage, so this is not
 *   dead code — warn-once rather than assert, per this batch's bias
 *   toward safe defaults for plausibly-live network paths.
 * ==================================================================== */
void Train_StartMultiplayer(void)
{
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: Train_StartMultiplayer not implemented "
                         "(TODO: decompile 0x43A760) — multiplayer session not started\n");
        warned = true;
    }
}

/* ====================================================================
 * SYMBOL: Train_StopMultiplayer(void)
 * CALLER: TrainSubsystem::DispatchMessage(void*)
 * ADDRESS: 0x43A8B0
 * ACTION: loud-deferred-stub (warn-once, safe no-op)
 * RATIONALE: same shape of gap as Train_StartMultiplayer above (genuinely
 *   missing under plain C++ linkage). Decompiled at 0x43A8B0: reads the
 *   DirectPlay session's player list, filters players whose name matches
 *   the configured server name, and queues a type-2 message with the
 *   filtered list — depends on DirectPlay_SetSessionDesc and raw
 *   DPLAY session-list structures not yet ported. Reachable via the same
 *   DispatchMessage path as above — warn-once, not assert.
 * ==================================================================== */
void Train_StopMultiplayer(void)
{
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: Train_StopMultiplayer not implemented "
                         "(TODO: decompile 0x43A8B0) — multiplayer session not stopped\n");
        warned = true;
    }
}

/* ====================================================================
 * SYMBOL: Train_SendPlayerInfo(void*)
 * CALLER: Train_HandleTrackBuild(void*, int)  (town/Town.cpp:2916)
 * ADDRESS: 0x43CCC0
 * ACTION: loud-deferred-stub (warn-once, safe no-op)
 * RATIONALE: town/Town.cpp:291 declares this with plain C++ linkage
 *   (`extern void __fastcall Train_SendPlayerInfo(void* subsystem);`,
 *   outside any extern "C" block) — genuinely missing under that shape;
 *   the only existing definitions (shared/link_stubs.cpp's
 *   `(void*,int32_t)`, and game/Train_network.cpp's own *extern "C"*
 *   1-param declaration, itself call-0 and out of this batch's scope) are
 *   different symbols. Decompiled at 0x43CCC0: serializes every owned
 *   track piece's DPLAY data (via VehicleEditor::GetDPlayData/
 *   DPLAY_GetPlayerData) into a 0x3EC network packet and sends it via
 *   WIN32_SendNetworkData — real logic, but depends on several
 *   not-yet-confirmed TrainSubsystem field offsets beyond this batch's
 *   scope to port safely. Reachable from Train_HandleTrackBuild's
 *   else-branch (sub->sprite_list_1 == nullptr) — warn-once, not assert.
 * ==================================================================== */
void Train_SendPlayerInfo(void* subsystem)
{
    (void)subsystem;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: Train_SendPlayerInfo not implemented "
                         "(TODO: decompile 0x43CCC0) — player-info packet not sent\n");
        warned = true;
    }
}

/* ====================================================================
 * SYMBOL: Train_LoadSprites(int)
 * CALLERS: TrainStationWindow::Create(HWND), TrainStationWindow::show(int, int)
 * ADDRESS: 0x437670 (Ghidra-labeled TrainStationWindow_LoadSprites, 590 bytes)
 * ACTION: loud-deferred-stub (warn-once, safe no-op)
 * RATIONALE: genuinely missing (ui/TrainStationWindow.cpp:23 declares a
 *   free function `Train_LoadSprites(int thisPtr)`, called via its own
 *   `legacy_this_pointer()` helper which truncates the real `this` pointer
 *   to a 32-bit int — a pre-existing pointer-truncation bug in that caller,
 *   out of scope to fix here, but noted since it means even a correct
 *   implementation reconstructing a pointer from `thisPtr` on this 64-bit
 *   host would be unsafe for any heap address above 4GiB). The real
 *   function (590 bytes) is not yet ported as a TrainStationWindow method;
 *   not attempted here. Reachable whenever the train-station UI opens —
 *   warn-once, not assert.
 * ==================================================================== */
void Train_LoadSprites(int thisPtr)
{
    (void)thisPtr;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: Train_LoadSprites not implemented "
                         "(TODO: port TrainStationWindow_LoadSprites, 0x437670) — "
                         "train station sprites not loaded\n");
        warned = true;
    }
}

/* ====================================================================
 * SYMBOL: Vehicle_Ctor(void*, int, int, unsigned char, int)
 * CALLER: Train_HandleTrackBuild(void*, int)  (town/Town.cpp:2829)
 * ADDRESS: 0x44BE50 (Vehicle::Vehicle's real constructor)
 * ACTION: real-implementation (bridge to the already-integrated ctor)
 * RATIONALE: town/Town.cpp:285 declares
 *   `Vehicle_Ctor(void* obj, int resource_id, int type, uint8_t flag, int unknown)`
 *   with plain C++ linkage — genuinely missing under that exact shape
 *   (shared/link_stubs.cpp's/defsym_stubs.cpp's same-named stubs all use
 *   `char` for the last two params instead of `uint8_t`/`int`, a different
 *   mangled name). The call site passes exactly 4 constructor args after
 *   `obj` (resource_id, type, flag, unknown), matching
 *   Vehicle::Vehicle(int32_t,int32_t,uint8_t,uint8_t) position-for-position
 *   (the 5th/`unknown` param narrows to the ctor's uint8_t 4th param).
 * ==================================================================== */
void* Vehicle_Ctor(void* obj, int resource_id, int type, uint8_t flag, int unknown)
{
    if (obj == nullptr) return nullptr;
    return new (obj) Vehicle(resource_id, type, flag, static_cast<uint8_t>(unknown));
}

/* ====================================================================
 * SYMBOL: Vehicle_InitRoute(void*, int, int, unsigned char)
 * CALLER: Train_HandleTrackBuild(void*, int)  (town/Town.cpp:2862)
 * ADDRESS: 0x44C220 (Vehicle::InitRoute)
 * ACTION: real-implementation (bridge)
 * RATIONALE: town/Town.cpp:287 declares this exact shape with plain C++
 *   linkage; the real logic already exists as Vehicle::InitRoute
 *   (game/Vehicle.h/.cpp) — town/Town.cpp calls the free-function name
 *   instead of the method.
 * ==================================================================== */
void Vehicle_InitRoute(void* obj, int resource_id, int type, uint8_t flag)
{
    if (obj == nullptr) return;
    static_cast<Vehicle*>(obj)->InitRoute(resource_id, type, flag);
}

/* ====================================================================
 * SYMBOL: VehicleEditor_SetDPlayData(void*, int)
 * CALLER: Train_HandleTrackBuild(void*, int)  (town/Town.cpp:2865)
 * ADDRESS: 0x40D770 (VehicleEditor::SetDPlayData)
 * ACTION: real-implementation (bridge)
 * RATIONALE: town/Town.cpp:289 declares this exact shape with plain C++
 *   linkage; the real logic already exists as
 *   VehicleEditor::SetDPlayData(const DPlayManager*) — town/Town.cpp's
 *   call site already reinterprets a DPlayManager* as an int for this
 *   free-function call (a separate, pre-existing pointer-truncation
 *   concern in that caller, not fixed here), so this bridge reverses
 *   exactly that cast back before calling the real method.
 * ==================================================================== */
void VehicleEditor_SetDPlayData(void* vehicleEditor, int data)
{
    if (vehicleEditor == nullptr) return;
    static_cast<VehicleEditor*>(vehicleEditor)->SetDPlayData(
        reinterpret_cast<const DPlayManager*>(static_cast<intptr_t>(data)));
}

/* ====================================================================
 * SYMBOL: VehicleEditor_Update(Vehicle*)
 * CALLER: World::UpdateTick()  (game/World.cpp:688)
 * ADDRESS: 0x44C3A0 (1520 bytes)
 * ACTION: loud-deferred-stub (warn-once, safe no-op)
 * RATIONALE: genuinely missing — game/World.cpp:65 declares
 *   `extern void __thiscall VehicleEditor_Update(Vehicle* vehicle);` with
 *   plain C++ linkage; the only existing same-named stub
 *   (shared/defsym_stubs.cpp, `VehicleEditor_Update(void*)`) is a
 *   different, wrong-signature symbol. Ghidra-decompiled: a substantial
 *   (1520-byte) per-frame vehicle movement/engine-sound/edge-routing/
 *   network-sync state machine — full decompilation is a dedicated-session
 *   task, out of scope for this landmine-linkage batch. Reachable from
 *   every tick of World::UpdateTick() while any vehicle is active in game
 *   modes 3/9 — this leaves vehicle movement unimplemented on that path
 *   until a real port lands; warn-once (not assert) so gameplay/tests that
 *   reach it degrade instead of aborting.
 * ==================================================================== */
void VehicleEditor_Update(Vehicle* vehicle)
{
    (void)vehicle;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "STUB: VehicleEditor_Update not implemented "
                         "(TODO: decompile 0x44C3A0) — vehicle movement/engine-sound/"
                         "edge-routing update skipped this tick\n");
        warned = true;
    }
}

/* EditorState_Ctor(void*, unsigned char) [extern "C"] — this bridge (formerly
 * here) was removed: its only caller, Vehicle::Vehicle (game/Vehicle.cpp),
 * now constructs EditorState directly via
 * `new (operator_new(sizeof(EditorState))) EditorState(...)`, per this
 * function's own SHOULD_BE_FIXED_AT note, closing the operator_new(0x20)
 * vs. sizeof(EditorState)==0x28 undersized-allocation deviation at the
 * source instead of working around it here. */

/* ====================================================================
 * SYMBOLS: EditorState_SelectLayout / EditorState_HandleNetworkGame /
 *   EditorState_StartGameTimer / EditorState_LoadExistingGame /
 *   EditorState_SetDifficulty / EditorState_StartNewGame
 * CALLERS: Netman::ProcessMessage(TrainMessage*) [3x],
 *   Netman::HandlePlayerJoin(), Netman::SyncGameState(TrainMessage*),
 *   GameSetupPanel::SelectLayoutEntry(int32_t)
 * ADDRESSES: 0x40A3D0 / 0x40A300 / 0x40A350 / 0x40A260 / 0x40A4A0 / 0x40A150
 * ACTION: real-implementation
 * RATIONALE: despite the "EditorState_" name (a prior session's rename of
 *   the Ghidra `GAMESTATE_*` labels), these are NOT world/EditorState.h
 *   methods — Ghidra confirms their real address range (0x40A150-0x40AD94)
 *   sits entirely BEFORE EditorState::EditorState at 0x40B500, and every
 *   one operates on a GameSetupPanel* (`this+0xE8`/`+0xEC`/`+0xF0`/`+0xF4`/
 *   `+0x10C`/`+0x1B8` match ui/GameSetupPanel.h's `field_E8`/`titleList`/
 *   `layoutList`/`selectedEntry`/`field_10C`/`renderFlag` exactly, and
 *   ui/GameSetupPanel.h's own doc comments already say "Called from:
 *   GAMESTATE_SelectLayout, GAMESTATE_StartGameTimer" for
 *   SelectLayoutEntry). PROGRESS.md's own open TODO ("gamestate_handlers.c
 *   removed — original GAMESTATE_* addresses (0x40A150-0x40B4C0) are
 *   claimed by the renamed EditorState_* reconstruction, which itself is
 *   still incomplete") is exactly this gap. Decompiled all six from Ghidra
 *   (0x40A150/0x40A260/0x40A300/0x40A350/0x40A3D0/0x40A4A0) and transcribed
 *   using GameSetupPanel's already-named fields/methods.
 *
 *   Two decompiler-vs-host hazards fixed while transcribing (would have
 *   been new landmines if copied literally):
 *     1. `DAT_004fd3a8 + 8` (the "network mode active" gate in
 *        LoadExistingGame/HandleNetworkGame/StartGameTimer/SetDifficulty)
 *        is null on this host if read as a raw address — the established,
 *        already-integrated idiom for this exact byte (ui/GameSetupPanel.cpp,
 *        ui/GameSetupPanel_network.cpp, ui/EditWindow.cpp all use it) is
 *        `_g_netman_state != nullptr && _g_netman_state[8] != 0`; followed
 *        that instead of dereferencing the raw address.
 *     2. StartGameTimer's `(**(code**)(*param_1 + 0x20))(0)` is a vtable
 *        dispatch at x86 BYTE offset 0x20 = index 8 = GameSetupPanel's
 *        Render/Update slot (see ui/GameSetupPanel.h's vtable map) — on
 *        this 64-bit host, byte offset 0x20 is index 4, a different slot
 *        entirely (the vtable-byte-offset-misalignment landmine class).
 *        Replaced with the named virtual call `panel->on_update(0)`.
 *
 *   One open discrepancy flagged, not resolved (would need its own
 *   evidence-gathering session): StartGameTimer's decompiler comment says
 *   its final field-write "sets game state to 2 (running)", but that same
 *   byte offset (+0x1B0) is named `textAlignMode` (init 3) in
 *   ui/GameSetupPanel.h. The write is preserved (same storage either way)
 *   via the header's existing field name; the semantic conflict between
 *   the two descriptions is left for whoever integrates this properly.
 * ==================================================================== */

/* EditorState_StartNewGame — 0x40A150. Declared by
 * ui/GameSetupPanel_network.cpp:23 (plain C++ linkage, outside any
 * extern "C" block). */
void EditorState_StartNewGame(void* uiPanel)
{
    auto* panel = static_cast<GameSetupPanel*>(uiPanel);
    if (panel == nullptr || panel->field_10C != 0) return;

    GameNetman()->m_gameMode = 0;
    GameNetman()->ResetNetworkState();
    GameNetman()->StopSession();
    NETMAN_StartClientSession();

    if (panel->layoutList == reinterpret_cast<LayoutListNode*>(static_cast<intptr_t>(-1))) {
        panel->layoutList = nullptr;
    }
    for (LayoutListNode* it = panel->layoutList; it != nullptr; ) {
        LayoutListNode* next = it->next;
        if (it->name != nullptr) GLOBAL_free(it->name);
        GLOBAL_free(it);
        it = next;
    }
    panel->layoutList = nullptr;

    TrainMessage* msg = new TrainMessage();
    msg->type = 2;
    msg->data_ptr = panel->hWnd;
    Train_QueueMessage(_g_train, msg);

    panel->field_10C = 1;
}

/* EditorState_LoadExistingGame — 0x40A260. network/Netman.h:257. */
void EditorState_LoadExistingGame(void* uiPanel)
{
    auto* panel = static_cast<GameSetupPanel*>(uiPanel);
    if (panel == nullptr) return;

    GameNetman()->ResetNetworkState();
    if (_g_netman_state == nullptr || _g_netman_state[8] == 0) {
        EditorState_StartNewGame(panel);
        GameNetman()->m_gameMode = 0;
        panel->drawLayoutList(panel->layoutList);
        g_resmgr.FormatResourceString(0x6f, panel->titleText, 0x80);
        panel->drawTitle();
    } else {
        GameNetman()->ResetNetworkState();
        GameNetman()->StopSession();
        NETMAN_StartClientSession();
        panel->loadLayouts(true);
    }
    panel->drawGrid();
    UIPANEL_EndPaintEx(panel, static_cast<int32_t>(reinterpret_cast<intptr_t>(panel->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
}

/* EditorState_HandleNetworkGame — 0x40A300. network/Netman.h:258. */
void EditorState_HandleNetworkGame(void* uiPanel)
{
    auto* panel = static_cast<GameSetupPanel*>(uiPanel);
    if (panel == nullptr || panel->renderFlag == 0) return;

    if (_g_netman_state != nullptr && _g_netman_state[8] != 0) {
        GameNetman()->m_gameMode = 1;
        panel->updateTitle();
    }
    panel->drawGrid();
    UIPANEL_EndPaintEx(panel, static_cast<int32_t>(reinterpret_cast<intptr_t>(panel->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
    panel->SendScenarioSelect(0);
}

/* EditorState_StartGameTimer — 0x40A350. network/Netman.h:260.
 * NOTE: the real param is a GameSetupPanel*, not a generic int32_t*
 * (Netman.h's own comment says "was GAMESTATE_StartGameTimer"; the
 * caller, network/Netman.cpp:1311, reinterpret_casts a GameSetupPanel*
 * to int32_t* to call it this exact way already). */
void EditorState_StartGameTimer(int32_t* uiPanelRaw)
{
    auto* panel = reinterpret_cast<GameSetupPanel*>(uiPanelRaw);
    if (panel == nullptr) return;

    panel->field_10C = 0;
    if (_g_netman_state == nullptr || _g_netman_state[8] == 0) {
        panel->SelectLayoutEntry(panel->selectedEntry + 1);
    } else {
        GameNetman()->Init(0);
        panel->loadLayouts(true);
    }
    panel->on_update(0);  /* vtable[8]; see landmine note above */
    panel->timerId1 = static_cast<int32_t>(SetTimer(panel->hWnd, 0x50, 0x32, nullptr));
    panel->timerId2 = static_cast<int32_t>(SetTimer(panel->hWnd, 0x52, 0x4b, nullptr));
    /* +0x1B0 — named `textAlignMode` in the header; decompiler comment
     * says "game state = 2". Same storage either way; see rationale
     * above for the unresolved semantic discrepancy. */
    panel->textAlignMode = 2;
}

/* EditorState_SelectLayout — 0x40A3D0. network/Netman.h:259.
 * NOTE: `layoutData` is stored directly into `panel->layoutList` by the
 * original (an int-sized slot reused to hold either a real list pointer
 * or a sentinel/zero), exactly as the caller — network/Netman.cpp:1240 —
 * reads it: a raw int32_t out of a network message buffer. Preserved
 * verbatim (not "fixed") per CLAUDE.md's fidelity requirement; the
 * pointer/int aliasing here is a pre-existing property of the original
 * function, not something this bridge should paper over. */
void EditorState_SelectLayout(void* uiPanel, int32_t layoutData)
{
    auto* panel = static_cast<GameSetupPanel*>(uiPanel);
    if (panel == nullptr) return;

    panel->field_10C = 0;
    if (panel->layoutList == reinterpret_cast<LayoutListNode*>(static_cast<intptr_t>(-1))) {
        panel->layoutList = nullptr;
    }
    for (LayoutListNode* it = panel->layoutList; it != nullptr; ) {
        LayoutListNode* next = it->next;
        if (it->name != nullptr) GLOBAL_free(it->name);
        GLOBAL_free(it);
        it = next;
    }

    if (layoutData == 0) {
        panel->layoutList = reinterpret_cast<LayoutListNode*>(static_cast<intptr_t>(-1));
        if (panel->field_E8 != 0) {
            EditorState_StartNewGame(panel);
            return;
        }
    } else {
        panel->layoutList = reinterpret_cast<LayoutListNode*>(static_cast<intptr_t>(layoutData));
        panel->SelectLayoutEntry(0);
        if (panel->renderFlag != 0) {
            panel->drawLayoutList(panel->layoutList);
            UIPANEL_EndPaintEx(panel, static_cast<int32_t>(reinterpret_cast<intptr_t>(panel->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
        }
    }
}

/* EditorState_SetDifficulty — 0x40A4A0. network/Netman.h:261. */
void EditorState_SetDifficulty(void* uiPanel, int32_t difficulty)
{
    auto* panel = static_cast<GameSetupPanel*>(uiPanel);
    if (panel == nullptr) return;

    panel->field_110 = difficulty;
    if ((_g_netman_state == nullptr || _g_netman_state[8] == 0) && panel->renderFlag != 0) {
        panel->updateTitle();
        UIPANEL_EndPaintEx(panel, static_cast<int32_t>(reinterpret_cast<intptr_t>(panel->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
    }
}

/* ====================================================================
 * SYMBOL: Entity_Ctor(void*, int, int, int, int)
 * CALLER: ScriptedObject::ScriptedObject()  (game/ScriptedObject.cpp:222,
 *   `Entity_Ctor(&sub_entity, -1, -1, 0, 0);`)
 * ADDRESS: 0x405790 (Entity::Entity(int,int16_t,int,int) — Ghidra's own
 *   label "GameObject_BaseCtor" is the same address; core/Entity.cpp
 *   documents 0x405790 as Entity's real base constructor and its body
 *   matches field-for-field)
 * ACTION: caller-declaration-is-wrong / safe-default-stub (see rationale)
 * RATIONALE: `sub_entity` is declared in game/ScriptedObject.h as
 *   `uint8_t sub_entity[0x88]` (136 bytes) — the ORIGINAL x86 size of
 *   Entity. On this 64-bit host, sizeof(Entity) is 168 bytes (confirmed by
 *   compiling core/Entity.h) because of pointer widening, so
 *   placement-constructing a real Entity into that inline 136-byte buffer
 *   would write up to 32 bytes past it, corrupting ScriptedObject's own
 *   adjacent fields (drag_rect etc. at +0x168). This is the fixed-size
 *   sibling of the "undersized operator_new" landmine class (MEMORY.md),
 *   except here there is no buffer to reallocate — the storage is inline
 *   and its address is fixed by ScriptedObject's own layout.
 *
 *   Cannot safely reproduce the real constructor's writes in general:
 *   offsetof(Entity, world_x_raw)==144, world_y_raw==148, and the
 *   name[11] buffer at 152 are all already past the 136-byte boundary.
 *   However, EVERY field the real Entity ctor writes a *value other than
 *   zero* to (`type`@8, `visible`@56) lies safely within [0,136), and at
 *   this call site's actual arguments (resource_id=-1, so
 *   `0 < resource_id` is false and GameObject_InitBase — the only
 *   OOB-relevant conditional branch — never runs; world_x=world_y=0, so
 *   the OOB `world_x_raw`/`world_y_raw` writes the real ctor would do are
 *   the same zero this stub already establishes via memset). So for the
 *   one real call site in the tree today, this reproduces the exact
 *   observable result of the real constructor without touching memory
 *   outside the 136-byte buffer. A future caller passing resource_id > 0
 *   or nonzero world_x/world_y would not be handled correctly (their
 *   effects are exactly the ones this stub cannot safely perform) — the
 *   one-time warning below flags that gap loudly rather than silently
 *   mis-constructing.
 * SHOULD_BE_FIXED_AT: game/ScriptedObject.h — retype `sub_entity` from
 *   `uint8_t[0x88]` to a real `Entity` member (or resize the byte array to
 *   sizeof(Entity)), so a real placement-new becomes safe.
 * ==================================================================== */
void Entity_Ctor(void* this_, int resource_id, int anim_idx, int world_x, int world_y)
{
    (void)anim_idx;
    constexpr size_t kUndersizedBufferBytes = 0x88;
    if (this_ == nullptr) return;

    std::memset(this_, 0, kUndersizedBufferBytes);
    auto* entity_view = static_cast<Entity*>(this_);
    entity_view->type = 2;       /* offset 8  — safely inside [0,0x88) */
    entity_view->visible = 1;    /* offset 56 — safely inside [0,0x88) */

    const bool would_init_base = resource_id > 0;
    const bool has_oob_position = (world_x != 0) || (world_y != 0);
    if (would_init_base || has_oob_position) {
        static bool warned = false;
        if (!warned) {
            fprintf(stderr,
                    "STUB: Entity_Ctor(0x405790) called with resource_id=%d world=(%d,%d) — "
                    "ScriptedObject::sub_entity[0x88] is undersized for a real Entity "
                    "(needs 168 bytes) on this host, so GameObject_InitBase and the "
                    "out-of-bounds world_x_raw/world_y_raw writes are skipped rather than "
                    "risking memory corruption. Fix ScriptedObject.h's sub_entity sizing "
                    "to handle this case for real.\n",
                    resource_id, world_x, world_y);
            warned = true;
        }
    }
}
