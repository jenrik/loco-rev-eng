/**
 * Train_network.cpp — TrainSubsystem network method implementations
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file contains ALL TrainSubsystem network/multiplayer methods that
 * were not in the original Train.cpp. It includes Train.h for the class
 * definition and provides full implementations with proper types.
 *
 * NOTE: Two changes must be made to the existing Train.cpp:
 *   1. Remove lines 39-40 (extern "C" declarations for Train_InitNetwork
 *      and Train_FlushMessages) since these are now class methods.
 *   2. Change line 646 from "Train_InitNetwork(this)" to "this->InitNetwork()"
 *   3. Change line 695 from "Train_FlushMessages(g_train)" to
 *      "((TrainSubsystem*)g_train)->FlushMessages()"
 */

// Status: TRANSCRIBED

#include "Train.h"
#include "../network/TrainMessage.h"
#include "../network/DPlayManager.h"
#include "../network/DirectPlay.h"
#include "GameConfig.h"
/* NetmanTypes.h (not the full Netman.h) — gets the complete Netman/
 * PlayerSlot types for named-field access (g_netman->m_gameMode, etc.)
 * without pulling in Netman.h's extern "C" Win32 block or its ~13
 * C++-linkage free-function declarations, which collide with (and are
 * superseded in practice by) this file's own correct extern "C"
 * declarations of the same names below — see NetmanTypes.h's header
 * comment for why that split exists and why the full header is unsafe here. */
#include "../network/NetmanTypes.h"
#include "Vehicle.h"
#include "../core/VehicleEditor.h"
#include "../input/InputMgr.h"
#include "../world/scriptengine.h"
#include <new>
#include <cstring> /* strcmp/memcpy — HandleConnectionSetup's DPLAY_SessionData
                    * field copies; strcpy/memcpy/strlen were already used
                    * unqualified elsewhere in this file before this include
                    * existed, so it was presumably reaching <cstring>
                    * transitively -- added explicitly now that strcmp joins
                    * them, rather than relying on that transitive path. */
#ifndef _WIN32
#include "sdl3_net_runtime.h"
#include "host_test_events.h"
#include <algorithm>
#include <vector>
#endif
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* C-linkage externals for network helpers                              */
/* (These are in addition to those in Train.cpp)                        */
/* ================================================================== */

int32_t NETMAN_GetGameMode(const void* netman);
void NETMAN_QueueMessage(TrainMessage* message); /* 0x43F140 */
#ifndef _WIN32
int32_t NETMAN_HostLocalSlotIndex();
#endif

/* C++ linkage (not extern "C"): native/NET_BaseDtor.c's real definition has
 * no extern "C" wrapper, so it mangles as a C++ symbol despite the .c
 * filename (this project's native/*.c sources are compiled as C++ — see
 * meson.build's common_c_args). Declaring it inside the extern "C" block
 * below would give it C linkage, mismatching the real mangled symbol and
 * reintroducing the exact call-0 landmine this declaration's own address/
 * signature fix (see the comment below) was meant to close. */
uint16_t NET_GetNextAttId(void);                                                 /* 0x00445F20 */

extern "C" {

/* CRT pattern helpers */
void   __cdecl CRT_memset_pattern(void* dst, int pattern, int count, void* callback); /* 0x004660D0 */
void   __cdecl CRT_free_pattern(void* ptr, int pattern, int count, void* cleanup);   /* 0x004660F0 */
char*  __cdecl CRT_itoa(int value, char* str, int radix);  /* CRT */

/* DirectPlay — Close/HostSession/ConnectToSession/SetSessionDesc are now
 * real DirectPlaySession:: methods (network/DirectPlay.h), called directly
 * below via g_dplay_peer-> — no free-function declarations needed for them
 * here. DirectPlay_HandleMessages (0x45F390) is used below but not
 * redeclared here either — network/DirectPlay.h's own declaration (now
 * transitively included) is C++-linkage, matching its real definition in
 * network/DirectPlay.cpp; this file's own prior extern "C" declaration of
 * it (removed here) gave it the wrong linkage, silently binding every call
 * below to an undefined C symbol instead of the real function — a landmine
 * from the same class CLAUDE.md's C-vs-C++-linkage rule targets, only
 * caught now that both declarations are visible in the same TU. Real
 * signature has 3 args the DB previously hid (decompiler dropped them
 * because the function's stored signature said void(void)): disassembly at
 * 0x43C8EE-0x43C8F2 and 0x43C98D-0x43C991 (both inside Train_ConnectToServer)
 * push (protocol, address, 0) — matches this function's OWN internal calls
 * at 0x45E88C-0x45E88E / 0x45E987-0x45E989 in DirectPlay_ConnectToSession,
 * which use (0, 0, 0). */

/* Win32 I/O */
int    __stdcall CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess,
                              uint32_t dwShareMode, void* lpSecurityAttributes,
                              uint32_t dwCreationDisposition, uint32_t dwFlagsAndAttributes,
                              void* hTemplateFile);
int    __stdcall ReadFile(void* hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead,
                           uint32_t* lpNumberOfBytesRead, void* lpOverlapped);
int    __stdcall WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
                            uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
void*  __stdcall GetProcessHeap(void);
void*  __stdcall HeapAlloc(void* hHeap, uint32_t dwFlags, uint32_t dwBytes);
int    __stdcall HeapFree(void* hHeap, uint32_t dwFlags, void* lpMem);
void   __stdcall Sleep(uint32_t dwMilliseconds);
int    __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);
uint32_t __stdcall GetFileAttributesA(const char* lpFileName);
/* shell32 — used by Train_ConnectToServer's browser-open (0x3EB, flag
 * bit0 clear) path to launch a validated http(s) URL. No established
 * host URL-open helper exists elsewhere in this tree; the host body is a
 * loud deferred stub in shared/link_stubs.cpp, matching CLAUDE.md's stub
 * policy. Unreachable today (Train_ConnectToServer's whole caller chain
 * is dead code), so the stub never fires. */
int    __stdcall ShellExecuteA(void* hwnd, const char* operation, const char* file,
                                const char* params, const char* directory, int show_cmd);
/* CRT import, already declared/stubbed under this exact C name in
 * core/GameObject.cpp (see shared/link_stubs.cpp); reused as-is. */
int    IsCharAlphaNumericA(char c);

/* DirectPlay message polling */
void*  __thiscall WIN32_PeekMessageLoop(void* dplay_peer);  /* 0x00460F10 */

/* Network manager */
int    __thiscall NETMAN_FindPlayerIndex(void* netman, int player_id); /* 0x00446760 */
int    __thiscall NETMAN_CheckTrackConnection(void* netman, int direction, uint8_t player_index); /* 0x00446830 */
/* NETMAN_GetPlayerCount(void*) at "0x00446890" removed 2026-08-18 — that
 * address is mid-body of ResourceManager::AddString (0x446840), a totally
 * unrelated function (confirmed via decompile_function), not a real
 * "get player count" entry point; its sole real call site (inside
 * TrainSubsystem::HandleConnectionSetup, below) actually disassembles to
 * `CALL 0x43D210`, i.e. Netman::GetPlayerCount() (network/NetmanTypes.h) —
 * which itself, per that method's own long-standing TODO, returns
 * `m_currentSlot` reinterpreted as int32_t, not a count at all. Rather than
 * route through that pointer-truncating method (unsafe on a 64-bit host),
 * HandleConnectionSetup now reads `m_currentSlot`/`m_gameMode` directly. */
void   __thiscall NETMAN_SendBuildingData(void* netman, int player_id); /* 0x00446510 */

/* DPLAY player helpers. DPLAY_CreatePlayer/InitPlayer (formerly declared
 * here at fabricated addresses 0x4429C0/0x00442A40 — both zero-xref,
 * confirmed via get_xrefs_to; the real addresses are 0x442850/0x442C90,
 * DPlayManager::CreatePlayer/InitPlayer, network/DPlayManager.h/.cpp)
 * removed 2026-08-15 — their one real call site now uses the typed
 * DPlayManager methods directly, see TrainSubsystem::HandleJoinMultiplayer
 * below. */
void DPLAY_CopyPlayerData(void* dst, const void* src); /* 0x4426D0 */
#ifndef _WIN32
void* DPLAY_DecodePlayerSlots(const void* firstCompactSlot);
#endif
void   __thiscall DPLAY_CleanupPlayer(void* player);         /* 0x00442A00 */

/* NET helpers */
void   __thiscall NET_GetAttFilePath(uint32_t type, int mode, char* buffer);  /* 0x00445B30 */
void   __thiscall NET_GetFilePath(uint32_t type, int mode, char* buffer);      /* 0x004459A0 */
int    __thiscall NET_FindPlayer(int mode, uint32_t player_id);                 /* 0x004461D0 */
void   __thiscall NET_RegisterPlayer(void* dplay, void* player, int flag, int unknown); /* 0x00446260 */
/* NET_GetNextAttId (0x445F20) declared above, outside this extern "C"
 * block — see that comment. Address/signature corrected here from a
 * previous, wrong 0x00445E70: that address is a mid-function address
 * inside NET_UploadAsset (0x445BD0), not a callable entry point (zero
 * xrefs to it in Ghidra) — NET_UploadAsset merely inlines the same
 * NextAttId-counter logic locally instead of calling out to it. Returns
 * uint16_t (matching `resp[3]` below and the counter's 0x7FFC wraparound),
 * and takes no implicit `this` (it's a plain free function, not a method —
 * __thiscall here was already a no-op on host either way). */

/* Vehicle_Ctor/Vehicle_CalcSpeed/Vehicle_InitRoute(void*, ...) removed
 * 2026-08-18 — their sole call sites (all inside
 * TrainSubsystem::HandleConnectionSetup, below) now use the real typed
 * Vehicle constructor and Vehicle::CalcSpeed/InitRoute methods (game/Vehicle.h),
 * consistent with the real disassembly's call targets (0x44BE50/0x44D6C0/
 * 0x44C220). The removed "Vehicle_Ctor" declaration's own address comment,
 * 0x0043D380, was fabricated — the real constructor call in this function's
 * disassembly targets 0x44BE50, matching Vehicle.h's documented ctor. */
int    __thiscall VehicleEditor_GetResourceId(void* vehicle);  /* 0x0043CC20 */
void*  __thiscall VehicleEditor_GetDPlayData(void* vehicle);   /* 0x0043CD00 */
/* VehicleEditor_SetDPlayData(void*, int) removed 2026-08-18 — its sole call
 * site (HandleConnectionSetup, below) now uses the real typed
 * VehicleEditor::SetDPlayData(const DPlayManager*) method (core/VehicleEditor.h,
 * address 0x40D770, matching this function's own disassembly). */

/* Config */
int    __thiscall Config_GetIniInt(void* config, const char* section, const char* key, int default_val); /* 0x00413FC0 */
void*  __thiscall Config_GetIniString(void* config, const char* section, const char* key,
                                       const char* default_val, char* out, int out_size); /* 0x00414030 */

/* Input helpers — INPUT_DirToOffset_{Up,Left,Down,Right} are declared in
 * input/InputMgr.h (included above transitively is NOT guaranteed, so
 * included directly below this extern "C" block); this file used to
 * redeclare them locally with the wrong addresses (0x41CF90-0x41CFF0
 * instead of the real 0x41D8F0-0x41D980) and the wrong __thiscall/void*
 * signature (they are plain int32_t*(int32_t*) functions — see
 * InputMgr.h's own ABI note). Symbol-name linking made the wrong address
 * comment harmless, but AddTrainCar (0x43B8C0) genuinely calls these to
 * compute a newly-added multiplayer car's initial grid position — using
 * InputMgr.h's real declarations instead of a hand-rolled dx/dy table
 * closes that gap. */

/* Internal train functions referenced from ProcessMessages */
/* Train_ConnectToServer is __thiscall (ECX=subsystem, one stack arg),
 * not __fastcall — see disassembly at 0x43C860 (MOV EDX,[ESP+8] fetches
 * the single stack argument; ECX is only ever used as `this`). The stack
 * argument is a payload pointer, not an int — the old `int data` type
 * only compiled here because of -fpermissive. */
void   __thiscall Train_ConnectToServer(void* subsystem, void* payload); /* 0x43C860 */
void   __fastcall Train_HandleTrackBuild(void* subsystem, int data); /* 0x0043CE10 */
/* Address corrected 2026-08-18 from a fabricated 0x0043CDA0 (zero xrefs;
 * that address is mid-body of this same function, confirmed via
 * disassemble_function — Train_SendPlayerInfo's real prologue is at
 * 0x43CCC0, matching Ghidra's own function name/boundary there and its
 * three real callers: Train_HandleTrackBuild (0x43D017),
 * Train_ProcessMessages (0x439A50), TrainSubsystem::HandleJoinMultiplayer
 * (0x43C7DE) below). Signature unchanged (single ECX arg, __fastcall,
 * matches Ghidra's own `void __fastcall Train_SendPlayerInfo(int)`). */
void   __fastcall Train_SendPlayerInfo(void* subsystem);             /* 0x0043CCC0 */
void   __fastcall Train_RemoveAllTracks(void* subsystem);            /* 0x43CC40 */

/* OutputDebugStringA is available as g_OutputDebugStringA from main file */

/* DirectPlay helpers — DestroyPeer/CreatePeer/EnumConnections are now real
 * DirectPlaySession:: methods; DirectPlay_QueryConnection was a duplicate,
 * differently-named declaration for the same address (0x45EE60) as the
 * real DirectPlay_GetConnectionCaps(uint8_t*) in network/DirectPlay.h —
 * declared correctly there, not redeclared here. */

/* Resource locking */

/* Thread/network */
void*  __thiscall WIN32_GetThreadResult(void* thread);   /* 0x460D10 */
int    __thiscall WIN32_SendNetworkData(void* peer, int player_id, void* data, int size, int flags); /* 0x460FD0 */

/* NET paths */
void   __thiscall NET_GetAssetPath(int type, int mode, char* buffer); /* 0x445700 */
uint8_t __cdecl NET_MapSpecialAsset(uint8_t kind, uint8_t value);     /* 0x445910 */
void*  __thiscall NET_FindArchivedAsset(void* archive, const char* name,
                                        int* out_info);               /* 0x45CD00 */

/* Win32 handles */
int    __stdcall CloseHandle(void* hObject);

/* CRT helpers */
uint32_t __cdecl CRT_rand(void);               /* 0x4682A0 */
void     __thiscall FormatResourceString(void* resmgr, int id, char* out, int maxLen); /* resource string formatter */

} /* extern "C" */

/* Allocation */
void*  __cdecl operator_new(size_t size);    /* 0x465CE0 */
void   __cdecl GLOBAL_free(void* ptr);       /* 0x465CD0 */

/* Debug output */
extern int (__stdcall *g_OutputDebugStringA)(const char*);

/* Globals */
extern void* g_train_resources;  /* train critical section resource */
extern void* g_network_queue;    /* network message queue head */


/* ================================================================== */
/* Additional global variables for networking                           */
/* ================================================================== */

extern int      g_demo_mode;       /* 0x004A9918 */
extern DirectPlaySession* g_dplay_peer;  /* 0x0048525C */
extern void*    g_train;           /* 0x004FD3A4 */
extern void*    g_network_thread;  /* 0x004FD398 */
extern Netman*  g_netman;          /* 0x004FD3AC — matches the extern Netman*
                                     * g_netman pattern already used by
                                     * game/Vehicle.cpp, game/World.cpp,
                                     * ui/PostcardPreviewWindow.cpp; the global
                                     * variable name/symbol is shared with the
                                     * `extern void* g_netman` declared in
                                     * other TUs (BuildingPanel.cpp, EditWindow.cpp,
                                     * etc.) — harmless, since a global's extern
                                     * type annotation isn't part of the linker
                                     * symbol. */
extern GameConfig* g_netSettings;  /* 0x004FD3A8 — same singleton as
                                     * network/Netman.cpp's canonical
                                     * `_g_netman_data` (game/GameConfig.h) */
extern void*    g_main_window;     /* 0x004AA4A0 */
/* g_resmgr: declared by resources/ResourceManager.h (pulled in transitively
 * via NetmanTypes.h above) as `extern ResourceManager g_resmgr;` — the
 * correct type (an object, not a pointer; many other TUs across the tree
 * still wrongly declare it as `extern void* g_resmgr`, matching what this
 * file used to do). Removed the local wrong-typed redeclaration rather than
 * letting it conflict with the newly-visible correct one. */
extern PlayerConfig* g_player_config; /* canonical singleton */
extern void*    g_ui_main;         /* 0x004FD3B4 */
extern void*    g_config_ini;      /* 0x0048537C */
extern void*    g_dplay;           /* 0x004FD3C4 */
extern int      g_game_mode;       /* 0x004851F4 */
extern void*    g_asset_archive;   /* 0x00485600 */
extern char     g_asset_base_path[]; /* 0x004A99C8 */

/* Network queue — defined in Train.cpp */
struct NetworkQueueNode;  /* Opaque — used via pointer */


/* ================================================================== */
/* Network message struct (0x1c bytes)                                 */
/* ================================================================== */

using NetworkMsg = TrainMessage;

namespace {

/* The original allocates a 0x1C-byte queue record with operator_new and
 * writes its fields directly.  The reconstructed type has a C++ lifetime,
 * so begin that lifetime before preserving those same field stores. */
NetworkMsg* AllocateNetworkMessage()
{
    void* storage = operator_new(sizeof(NetworkMsg));
    return storage == nullptr ? nullptr : ::new (storage) NetworkMsg{};
}

/* MirrorTrainHeading — 180-degree flip of a train grid heading (degrees):
 * 0<->0xB4, 0x5A<->0x10E. Values outside that 4-element set pass through
 * unchanged, matching every one of the binary's mirror-table fall-through
 * branches (e.g. 0x43B8F9-0x43B92D, 0x43AE64-0x43AECD, 0x43C079-0x43C0B4).
 * Shared by TrainSubsystem::AddTrainCar/UpdateTrainMovement/
 * MoveToNeighborTown. */
uint16_t MirrorTrainHeading(uint16_t heading)
{
    switch (heading) {
    case 0:     return 0xB4;
    case 0x5A:  return 0x10E;
    case 0xB4:  return 0;
    case 0x10E: return 0x5A;
    default:    return heading;
    }
}

/* AppendToVehicleList — append `car` to the tail of the singly-linked
 * Vehicle list rooted at `head` (car->next always cleared first, matching
 * every one of the binary's "append to sprite_list_2/sprite_list_3 tail"
 * blocks, e.g. 0x43B931-0x43B96F, 0x43AED1-0x43AEFF, 0x43B0A3-0x43B12F). */
void AppendToVehicleList(Vehicle*& head, Vehicle* car)
{
    car->next = nullptr;
    if (head == nullptr) {
        head = car;
        return;
    }
    Vehicle* tail = head;
    while (tail->next != nullptr) tail = tail->next;
    tail->next = car;
}

/* NotifyAndDrainDeadList — process every node reachable from
 * `self->sprite_list_2`: send a type-0x11 "car removed" notification for
 * each, mark it initialized-cleared, and unlink it. Matches two
 * byte-identical inlined copies in the binary (0x43C0E0-0x43C144 inside
 * UpdateTrainMovement's dead-owner purge, and 0x43B138-0x43B19E inside
 * MoveToNeighborTown's remote-send-failure path) — including the fact
 * that the loop clears `car->next` to nullptr *before* reading it back
 * into `self->sprite_list_2`. Re-verified directly on raw instruction
 * bytes (not decompiler pseudocode) for both copies:
 *   0x43C123 MOV [EAX+0x70],EBP(0)   <- clear car->next, THEN
 *   0x43C12D MOV EDX,[EDI+0x18]      <- reload car (same node, list head
 *                                       untouched since the clear)
 *   0x43C130 MOV EAX,[EDX+0x70]      <- read car->next: always 0
 *   0x43C133 MOV [EDI+0x18],EAX      <- sprite_list_2 = 0
 * and identically at 0x43B17B/0x43B185/0x43B188/0x43B18B. The list is
 * therefore always left empty after this call regardless of how many
 * nodes it originally held. That is the binary's own behavior, not a
 * transcription artifact — do not "fix" it into a proper list-drain
 * without re-checking both call sites' surrounding assembly first. */
void NotifyAndDrainDeadList(TrainSubsystem* self, uint8_t local_slot_index)
{
    while (self->sprite_list_2 != nullptr) {
        Vehicle* car = self->sprite_list_2;

        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg) {
            msg->type = 0x11;
            msg->data = car;
        }

        car->init_flag = 0;
        car->peer_index = local_slot_index;
        car->next = nullptr;
        car->init_flag = 0; /* 0x43C10A and 0x43C126 both clear this byte */
        self->sprite_list_2 = car->next; /* always nullptr — see comment above */

        NETMAN_QueueMessage(msg);
    }
}

} // namespace

/* ================================================================== */
/**
 * TrainSubsystem::TrainSubsystem
 * Address: 0x438BC0
 */
TrainSubsystem::TrainSubsystem(int context_a, int context_b)
    : context_id_a(context_a),
      context_id_b(context_b),
      byte_flags(0),
      byte_flag_2(0),
      player_peer_id(0),
      sprite_list_1(NULL),
      sprite_list_2(NULL),
      sprite_list_3(NULL),
      field_20(NULL),
      some_limit(0x14),
      handle_list_1(NULL),
      handle_list_2(NULL),
      request_count(0)
{
    /* field_30 (+0x30) is intentionally absent from the initializer list:
     * 0x438BC0 never writes it. */
    g_dplay_peer = NULL;

    if (g_demo_mode != 1) {
        this->InitNetwork();

        /* Reverse EnumConnections's list into GameConfig::m_providerList.
         * Real typed pointers throughout now (DirectPlaySession::
         * EnumConnections returns a genuine DirectPlayConnectionNode* list,
         * and GameConfig::m_providerList is itself typed
         * DirectPlayConnectionNode* — game/GameConfig.h) — the previous
         * int32_t-truncating-pointer TODO here no longer applies, since
         * nothing here round-trips a pointer through a narrower integer,
         * or through a raw byte offset, anymore. */
        DirectPlayConnectionNode* reversed = nullptr;
        for (DirectPlayConnectionNode* item = g_dplay_peer->EnumConnections();
             item != nullptr;
             item = item->next) {
            auto* copy = static_cast<DirectPlayConnectionNode*>(operator_new(sizeof(DirectPlayConnectionNode)));
            copy->next = reversed;
            copy->type = item->type;
            reversed = copy;
        }
        g_netSettings->m_providerList = reversed;

        /* GameConfig::m_connectionCaps[4] (+0x14..+0x17) — confirmed via
         * Ghidra decompile of the original 0x438BC0 (this constructor):
         * `*(char*)(DAT_004fd3a8 + 0x14 + i) = (char)DirectPlay_GetConnectionCaps(...)`
         * for i in [0,4). Was previously documented as padding and written
         * through a raw byte offset; both fixed 2026-08-17. */
        char index[2] = {'0', 0};
        for (int i = 0; i < 4; ++i, ++index[0]) {
            g_netSettings->m_connectionCaps[i] =
                static_cast<uint8_t>(DirectPlay_GetConnectionCaps(reinterpret_cast<uint8_t*>(index)));
        }
    }
}

#ifndef _WIN32
const TrainSubsystem::HostReceivedAsset*
TrainSubsystem::FindHostReceivedAsset(uint8_t mode, uint8_t type) const
{
    for (const HostReceivedAsset& asset : host_received_assets) {
        if (asset.mode == mode && asset.type == type) return &asset;
    }
    return nullptr;
}

void TrainSubsystem::ClearHostTrackSessions()
{
    for (Vehicle* vehicle : host_track_vehicles) {
        if (vehicle == nullptr) continue;
        vehicle->~Vehicle();
        GLOBAL_free(vehicle);
    }
    host_track_vehicles.clear();
    for (DPlayManager* session : host_track_sessions) {
        if (session == nullptr) continue;
        session->~DPlayManager();
        GLOBAL_free(session);
    }
    host_track_sessions.clear();
}
#endif

/** TrainSubsystem scalar deleting destructor target: 0x438CA0. */
TrainSubsystem::~TrainSubsystem()
{
#ifndef _WIN32
    this->ClearHostTrackSessions();
#endif
    this->BaseDtor();
}

/**
 * TrainSubsystem::BaseDtor
 * Address: 0x438CC0
 */
void TrainSubsystem::BaseDtor()
{
    /* TODO NEW-054-THREAD-RESULT-ABI: binary distinguishes timeout
     * (AL=0, EAX=0) from error (AL=0, upper bits set). Current check
     * conflates both as non-zero. Decompile 0x460D10 for exact ABI. */
    if (g_network_thread != NULL &&
        reinterpret_cast<intptr_t>(WIN32_GetThreadResult(g_network_thread)) != 0) {
        this->FlushMessages();
    }

    if (g_dplay_peer != NULL) {
        DirectPlaySession* peer = g_dplay_peer;
        peer->DestroyPeer();
        GLOBAL_free(peer);
        g_dplay_peer = NULL;
    }

    /* Raw +0x70 byte offset preserved verbatim rather than converted to the
     * named Vehicle::next field — see docs/landmine-sweep-worklist.md's
     * Train_network.cpp raw-offset/host-layout note: Vehicle's `editors[4]`
     * + `editor_state` pointer fields widen on this 64-bit host, so the
     * real offsetof(Vehicle, next) here is not 0x70. Every raw Vehicle
     * offset in this file shares that mismatch; converting only some of
     * them to named-field access while others keep +0x70 arithmetic would
     * silently split sprite_list_1/2/3 traversal onto two different
     * addresses. Left uniform (byte arithmetic, -fpermissive Vehicle*<->
     * void* narrowing unchanged) and dead-code-safe. */
    void** lists[] = {&sprite_list_1, &sprite_list_2, &sprite_list_3};
    for (unsigned i = 0; i < 3; ++i) {
        while (*lists[i] != NULL) {
            void* node = *lists[i];
            *lists[i] = *reinterpret_cast<void**>(static_cast<uint8_t*>(node) + 0x70);
            void** vtable = *reinterpret_cast<void***>(node);
            reinterpret_cast<void(__thiscall*)(void*, byte)>(vtable[0])(node, 1);
        }
    }

    static_cast<ScriptEngine*>(g_train_resources)->ScriptEngine::Lock();
    while (g_network_queue != NULL) {
        NetworkMsg* msg = static_cast<NetworkMsg*>(g_network_queue);
        g_network_queue = msg->next;
        if (msg->data != NULL) {
            if (msg->type == 0x0E) {
                void** vtable = *reinterpret_cast<void***>(msg->data);
                reinterpret_cast<void(__thiscall*)(void*, byte)>(vtable[0])(msg->data, 1);
            } else {
                GLOBAL_free(msg->data);
            }
        }
        GLOBAL_free(msg);
    }
    static_cast<ScriptEngine*>(g_train_resources)->ScriptEngine::Unlock();

    PlayerConnectionNode** handles[] = {&handle_list_1, &handle_list_2};
    for (unsigned i = 0; i < 2; ++i) {
        while (*handles[i] != NULL) {
            PlayerConnectionNode* node = *handles[i];
            if (node->file_handle != 0) {
                CloseHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)));
                node->file_handle = 0;
            }
            *handles[i] = static_cast<PlayerConnectionNode*>(node->next);
            GLOBAL_free(node);
        }
    }
}

/**
 * TrainSubsystem::DownloadMissingAssets
 * Address: 0x438E40
 */
void TrainSubsystem::DownloadMissingAssets(DPlayManager* session)
{
#ifndef _WIN32
    if (session == nullptr || NETMAN_GetGameMode(g_netman) != 1) return;

    std::vector<std::pair<uint8_t, uint8_t>> assets;
    const auto add_unique = [&](uint8_t mode, uint8_t type) {
        if (mode == 0) return;
        const std::pair<uint8_t, uint8_t> key{mode, type};
        if (std::find(assets.begin(), assets.end(), key) == assets.end())
            assets.push_back(key);
    };
    for (uint16_t index = 0; index < 128; ++index) {
        const uint8_t type = session->m_trackEntries[index * 6];
        const uint8_t mode = session->m_trackEntries[index * 6 + 1];
        if (mode == 0) break;
        add_unique(mode, type);
    }
    if (session->m_playerType != 0) {
        add_unique(session->m_playerType,
                   NET_MapSpecialAsset(0x1E, session->m_playerTrack));
    }
    if (session->m_unknown93 != 0) {
        add_unique(session->m_unknown93, NET_MapSpecialAsset(0x1F, 1));
    }

    for (const auto& key : assets) {
        const uint8_t mode = key.first;
        const uint8_t type = key.second;
        if (const HostReceivedAsset* owned = FindHostReceivedAsset(mode, type)) {
            loco::host_test::emit_legacy_asset_consumed(
                mode, type, owned->bytes.size());
            continue;
        }
        char path[0x504] = {};
        NET_GetAssetPath(type, mode, path);
        if (GetFileAttributesA(path) != 0xFFFFFFFFu) continue;
        uint8_t* request = static_cast<uint8_t*>(operator_new(6));
        if (request == nullptr) continue;
        *reinterpret_cast<uint16_t*>(request) = 0x3ED;
        request[4] = mode;
        request[5] = type;
        WIN32_SendNetworkData(g_dplay_peer, player_peer_id, request, 6, 1);
        GLOBAL_free(request);
        ++request_count;
    }
    return;
#else
    void* entity = session;
    if (g_netman->m_gameMode != 1) return;

    struct MissingAsset { uint8_t type, mode; uint8_t pad[2]; MissingAsset* next; };
    MissingAsset* missing = NULL;
    uint8_t* record = reinterpret_cast<uint8_t*>(entity) + 0x96;

    for (unsigned i = 0; i < 0x80 && record[1] != 0; ++i, record += 6) {
        bool duplicate = false;
        for (MissingAsset* it = missing; it != NULL; it = it->next) {
            if (it->type == record[0] && it->mode == record[1]) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            MissingAsset* item = (MissingAsset*)operator_new(sizeof(MissingAsset));
            item->type = record[0];
            item->mode = record[1];
            item->next = missing;
            missing = item;
        }
    }

    while (missing != NULL) {
        MissingAsset* item = missing;
        char path[0x504] = {0};
        NET_GetAssetPath(item->type, item->mode, path);
        bool present = false;
        if (g_asset_archive != NULL) {
            int info = 0;
            const char* archive_name = path + strlen(g_asset_base_path);
            void* entry = NET_FindArchivedAsset(&g_asset_archive, archive_name, &info);
            if (entry != NULL) {
                present = true;
                GLOBAL_free(entry);
            }
        }
        if (!present && GetFileAttributesA(path) != 0xFFFFFFFFu) present = true;

        if (!present) {
            uint8_t* request = reinterpret_cast<uint8_t*>(operator_new(6));
            *reinterpret_cast<uint16_t*>(request) = 0x3ED;
            request[4] = item->mode;
            request[5] = item->type;
            WIN32_SendNetworkData(g_dplay_peer, player_peer_id, request, 6, 1);
            GLOBAL_free(request);
            ++request_count;
        }
        missing = item->next;
        GLOBAL_free(item);
    }

    /* Two optional singleton assets at entity +0x94 and +0x93. */
    uint8_t special_modes[2] = {
        *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(entity) + 0x94)),
        *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(entity) + 0x93))
    };
    uint8_t special_types[2] = {
        *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(entity) + 0x95)), 1
    };
    uint8_t special_kinds[2] = {0x1E, 0x1F};
    for (int i = 0; i < 2; ++i) {
        if (special_modes[i] == 0) continue;
        uint8_t type = NET_MapSpecialAsset(special_kinds[i], special_types[i]);
        char path[0x504] = {0};
        NET_GetAssetPath(type, special_modes[i], path);
        bool present = GetFileAttributesA(path) != 0xFFFFFFFFu;
        if (!present) {
            uint8_t* request = reinterpret_cast<uint8_t*>(operator_new(6));
            *reinterpret_cast<uint16_t*>(request) = 0x3ED;
            request[4] = special_modes[i];
            request[5] = type;
            WIN32_SendNetworkData(g_dplay_peer, player_peer_id, request, 6, 1);
            GLOBAL_free(request);
            ++request_count;
        }
    }
#endif
}

/* ================================================================== */
/* TrainSubsystem::InitNetwork                                         */
/* Address: 0x4391A0                                                    */
/* ================================================================== */
void TrainSubsystem::InitNetwork()
{
    DirectPlaySession* old_peer = g_dplay_peer;

    if (g_dplay_peer != NULL) {
        g_dplay_peer->DestroyPeer();
        GLOBAL_free(old_peer);
        g_dplay_peer = NULL;
        Sleep(1);
    }

    /* Sized to the real class (sizeof), not the original x86 struct's
     * hardcoded 0x160c bytes — DirectPlaySession's COM-interface/list
     * pointers are native (8-byte) width on this host, so it no longer
     * fits in 0x160c; CLAUDE.md treats x86 layout parity as a non-goal
     * off-Windows, so sizeof() here rather than re-deriving a packed size. */
    auto* new_peer = static_cast<DirectPlaySession*>(operator_new(sizeof(DirectPlaySession)));
    if (new_peer != NULL) {
        new_peer->CreatePeer(this->context_id_a, 0);
    }
    g_dplay_peer = new_peer;

    if (g_dplay_peer != NULL) {
        /* Overrides CreateAddress's own error_callback/show_dialogs/hwnd
         * initialization — a deliberate caller-side policy (dialogs off,
         * a different hwnd) for this particular network-init path, not a
         * bug; preserved exactly via named fields instead of raw offsets. */
        g_dplay_peer->error_callback = nullptr;
        g_dplay_peer->show_dialogs = 0;
        g_dplay_peer->hwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(this->context_id_b));
    }
}


/* ================================================================== */
/* TrainSubsystem::QueueMessage                                        */
/* Address: 0x4393D0                                                    */
/* ================================================================== */
void TrainSubsystem::QueueMessage(void* msg)
{
    NetworkMsg* net_msg = static_cast<NetworkMsg*>(msg);

    /* In multiplayer mode (g_game_mode==10), handle/discard immediately */
    if (g_game_mode == 10 && net_msg->type != 8) {
        if (net_msg->data != NULL) {
            if (net_msg->type == 0x0E || net_msg->type == 0x10) {
                void** data_vt = *reinterpret_cast<void***>(net_msg->data);
                reinterpret_cast<void(__thiscall*)(void*, byte)>(data_vt[0])(net_msg->data, 1);
            } else {
                GLOBAL_free(net_msg->data);
            }
            net_msg->data = NULL;
        }
        GLOBAL_free(net_msg);
        return;
    }

    /* Check disconnect-pending flag */
    if (this->byte_flag_2 != 0) {
        if (net_msg->data != NULL) {
            GLOBAL_free(net_msg->data);
        }
        GLOBAL_free(net_msg);
        return;
    }

    /* Enqueue onto g_network_queue under critical section */
    net_msg->next = NULL;
    static_cast<ScriptEngine*>(g_train_resources)->ScriptEngine::Lock();

    if (g_network_queue == NULL) {
        g_network_queue = net_msg;
        static_cast<ScriptEngine*>(g_train_resources)->ScriptEngine::Unlock();
        return;
    }

    /* Walk to end, counting depth */
    NetworkMsg* cursor = static_cast<NetworkMsg*>(g_network_queue);
    int depth = 1;
    while (cursor->next != NULL) {
        depth++;
        cursor = static_cast<NetworkMsg*>(cursor->next);
    }

    /* Throttle: if queue >= 6 and this is a 0-data SendNetworkData, drop it */
    if (depth >= 6 && net_msg->type == 6 && net_msg->flags == 0) {
        static_cast<ScriptEngine*>(g_train_resources)->ScriptEngine::Unlock();
        if (net_msg->data != NULL) {
            GLOBAL_free(net_msg->data);
        }
        net_msg->data = NULL;
        GLOBAL_free(net_msg);
        return;
    }

    cursor->next = net_msg;
    static_cast<ScriptEngine*>(g_train_resources)->ScriptEngine::Unlock();
}


/* ================================================================== */
/* TrainSubsystem::FlushMessages                                       */
/* Address: 0x4394E0                                                    */
/* ================================================================== */
void TrainSubsystem::FlushMessages()
{
    void* thread_result;

    thread_result = WIN32_GetThreadResult(g_network_thread);

    if (reinterpret_cast<intptr_t>(thread_result) != 0) {
        /* Thread active — queue DISCONNECT (type-8) message */
        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg == NULL) {
            msg = NULL;
        } else {
            msg->data = NULL;
            msg->next = NULL;
        }
        if (msg != NULL) {
            msg->type = 8;
            msg->data = NULL;
            msg->next = NULL;
        }
        if (msg != NULL) {
            this->QueueMessage(msg);
        }
    }

    /* Spin-wait for thread to terminate */
    thread_result = WIN32_GetThreadResult(g_network_thread);
    while (reinterpret_cast<intptr_t>(thread_result) != 0) {
        Sleep(10);
        thread_result = WIN32_GetThreadResult(g_network_thread);
    }
}


/* ================================================================== */
/* TrainSubsystem::DispatchMessage                                     */
/* Address: 0x439550                                                    */
/* ================================================================== */
void TrainSubsystem::DispatchMessage(void* msg)
{
    NetworkMsg* net_msg = reinterpret_cast<NetworkMsg*>(msg);

    switch (net_msg->type) {
    case 0: /* HostSession */
        if (g_dplay_peer == NULL) {
            this->InitNetwork();
        }
        g_dplay_peer->Close();
        g_dplay_peer->HostSession(net_msg->data != NULL ? 1 : 0,
                                   net_msg->flags, 0, 1);
        net_msg->data = NULL;
        return;

    case 1: /* StartMultiplayer */
        Train_StartMultiplayer();
        return;

    case 2: /* StopMultiplayer */
        Train_StopMultiplayer();
        return;

    case 3: /* ShutdownNetwork */
        this->ShutdownNetwork();
        return;

    case 6: /* SendNetworkData */
#ifndef _WIN32
        if (net_msg->data != NULL && net_msg->size >= 4) {
            const auto* first = static_cast<const uint8_t*>(net_msg->data);
            std::vector<uint8_t> payload(first, first + net_msg->size);
            // WIN32_SendNetworkData (0x460FD0) stamps protocol version 300
            // before DirectPlay::Send. Preserve that mutation at the adapter.
            payload[2] = 0x2c;
            payload[3] = 0x01;
            lego_loco::network::HostTransportWorker().SendLegacy(
                static_cast<lego_loco::network::VirtualPlayerId>(net_msg->to_player),
                std::move(payload));
        }
#else
        if (g_dplay_peer != NULL && g_dplay_peer->session_ready != 0) {
            WIN32_SendNetworkData(g_dplay_peer,
                                   net_msg->to_player,
                                   net_msg->data,
                                   net_msg->size,
                                   net_msg->flags != 0 ? 1 : 0);
        }
#endif
        if (net_msg->data != NULL) {
            GLOBAL_free(net_msg->data);
            net_msg->data = NULL;
        }
        return;

    case 8: /* DisconnectPending */
        this->byte_flag_2 = 1;
        /* fall through */
    case 5: /* HandleDisconnect */
        this->HandleDisconnect();
        return;

    case 0x0E: /* HandleJoinMultiplayer */
        this->HandleJoinMultiplayer(net_msg);
        return;

    case 0x10: /* HandleFileTransfer */
        this->HandleFileTransfer(net_msg);
        return;

    case 0x19: /* UpdatePlayerCount + Reset */
    {
        /* g_netman->m_mySlotIndex — self slot index, not a count despite the
         * misleading local name this replaces (see Netman.h's own doc
         * comment: "self slot index (0-8 or -1)"). */
        uint32_t self_slot_index = static_cast<uint32_t>(g_netman->m_mySlotIndex);
        this->UpdatePlayerCount(self_slot_index);
        this->ResetMultiplayerState(0);
        return;
    }
    }
}


/* ================================================================== */
/* TrainSubsystem::ProcessMessages                                     */
/* Address: 0x4396C0                                                    */
/* Size: 1508 bytes                                                     */
/* ================================================================== */
void TrainSubsystem::ProcessMessages()
{
    if (this->byte_flag_2 != 0 || g_dplay_peer == NULL) {
        return;
    }

    while (g_dplay_peer != NULL) {
        /* Poll DirectPlay for one message */
        void* lpMem = WIN32_PeekMessageLoop(g_dplay_peer);
        if (lpMem == NULL) break;

        /* lpMem layout: +0x00 = player_id (int), +0x04 = payload (void*) */
        int32_t  player_id = *reinterpret_cast<int32_t*>(lpMem);
        void*    payload   = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(lpMem) + 4));
        uint16_t msg_type  = *reinterpret_cast<uint16_t*>(payload);

        /* === Low message types (0-20) === */
        if (msg_type < 0x15) {
            switch (msg_type) {
            case 0x14: /* 20 — PlayerLeave */
                this->HandlePlayerLeave(*reinterpret_cast<int32_t*>((reinterpret_cast<uint16_t*>(payload) + 2)));
                goto free_msg;

            case 10: /* 10 — Disconnect */
                this->HandleDisconnect();

                /* Queue internal type-5 message */
                {
                    NetworkMsg* qmsg = AllocateNetworkMessage();
                    if (qmsg) { qmsg->data = NULL; qmsg->next = NULL; qmsg->type = 5; }
                    NETMAN_QueueMessage(qmsg);
                }

                /* If scenario mode, unlink all cars from sprite_list_1,
                 * marking each dead (init_flag=0, Vehicle.h +0x88) and
                 * queueing a type-0xF (RemoveCar) notification per car —
                 * the same "unlink + init_flag=0 + notify" pattern as
                 * RemoveAllCars (0x43CBE0). Disassembly at 0x439790-
                 * 0x4397AD confirms car->next (+0x70) is both the unlink
                 * pointer and the field cleared on the just-unlinked node
                 * after sprite_list_1 is advanced. */
                if (g_netman->m_gameMode == 1) {
                    while (this->sprite_list_1 != nullptr) {
                        Vehicle* car = this->sprite_list_1;
                        NetworkMsg* qmsg = AllocateNetworkMessage();
                        if (qmsg) {
                            qmsg->data = NULL; qmsg->next = NULL;
                            qmsg->type = 0x0F;
                            qmsg->data = car;
                        }
                        car->init_flag = 0;
                        this->sprite_list_1 = car->next;
                        car->next = nullptr;
                        NETMAN_QueueMessage(qmsg);
                    }
                }
                goto free_msg;
            }
            goto free_msg;
        }

        /* === Medium message types (0x3C, 1000) === */
        if (msg_type < 0x3E9) {
            if (msg_type == 1000) {
                this->player_peer_id = player_id;

                /* Reset timeout on all controller cars. Reuses
                 * Vehicle::tunnel_angle (+0x74) as a 32000 (0x7d00) "max
                 * timeout" sentinel rather than a heading value here —
                 * the same reuse already documented as "max timeout" at
                 * TrainSubsystem::HandleJoinMultiplayer's identical site
                 * in this file — not a distinct field. */
                for (Vehicle* car = this->sprite_list_1; car != nullptr; car = car->next) {
                    car->tunnel_angle = 32000;
                }

                /* Send player info response (0x3E9, 24 bytes). Fixed-size
                 * raw network wire-format packet (explicit field offsets
                 * below), not a C++ object — safe as-is on any host. */
                {
                    uint16_t* resp = reinterpret_cast<uint16_t*>(operator_new(0x18));
                    if (resp) {
                        resp[0] = 0x3E9;
                        *reinterpret_cast<int32_t*>((resp + 2)) = *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(g_player_config) + 0x14));
                        *reinterpret_cast<int32_t*>((resp + 4)) = *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(g_main_window) + 0x18));
                        *reinterpret_cast<int32_t*>((resp + 6)) = *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(g_main_window) + 0x1C));
                        *reinterpret_cast<int32_t*>((resp + 8)) = *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(g_main_window) + 0x20));
                        *reinterpret_cast<int32_t*>((resp + 10)) = *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(g_main_window) + 0x24));
                        WIN32_SendNetworkData(g_dplay_peer, this->player_peer_id,
                                              resp, 0x18, 1);
                        GLOBAL_free(resp);
                    }
                }
            } else if (msg_type == 0x3C) {
                /* Connection handshake */
                NetworkMsg* qmsg = AllocateNetworkMessage();
                if (qmsg) {
                    qmsg->data = NULL; qmsg->next = NULL;
                    qmsg->type = 0x0C;
                    qmsg->to_player = this->player_peer_id;
                }
                this->player_peer_id = g_dplay_peer->player_dpid;
                NETMAN_QueueMessage(qmsg);
            }
            goto free_msg;
        }

        /* === High message types (0x3EA-0x3FD) === */
        switch (msg_type - 0x3EA) {
        case 0: { /* 0x3EA — PlayerInfo */
            uint16_t* p = reinterpret_cast<uint16_t*>(payload);
            uint8_t info_flag = static_cast<uint8_t>(p[4]);
            int32_t config_val = *reinterpret_cast<int32_t*>((p + 2));

            /* Reset timeout on all controller cars — same tunnel_angle
             * "max timeout" sentinel reuse as the msg_type==1000 case
             * above. */
            for (Vehicle* car = this->sprite_list_1; car != nullptr; car = car->next) {
                car->tunnel_angle = 32000;
            }

            if (info_flag) this->field_30 = 1;
            *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(g_player_config) + 0x14)) = config_val;
            /* PlayerConfig_Save is called inside Train_SendPlayerInfo */
            Train_SendPlayerInfo(this);
            break;
        }

        case 1: /* 0x3EB — ConnectToServer */
            Train_ConnectToServer(this, payload);
            break;

        case 2: /* 0x3EC — HandleTrackBuild */
            Train_HandleTrackBuild(this, reinterpret_cast<uint8_t*>(payload));
            break;

        case 4: { /* 0x3EE — FileData (incoming asset) */
            uint16_t* p = reinterpret_cast<uint16_t*>(payload);
            char path_buf[1284];
            uint32_t bytes_written;

            NET_GetAssetPath(*reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(p) + 5)), static_cast<uint8_t>(p[2]), path_buf);
            void* hFile = reinterpret_cast<void*>(static_cast<uintptr_t>(CreateFileA(path_buf, 0x40000000, 0, NULL, 1, 0x80, NULL)));
            if (hFile != reinterpret_cast<void*>(0xFFFFFFFF)) {
                WriteFile(hFile, p + 6, *reinterpret_cast<uint32_t*>((p + 4)), &bytes_written, NULL);
                CloseHandle(hFile);
                this->request_count--;
            }
            if (this->request_count == 0) {
                g_dplay_peer->Close();
            }
            break;
        }

        case 6: { /* 0x3F0 — GameOver control signal */
            if (g_dplay_peer->session_state != 0) {
                NetworkMsg* qmsg = AllocateNetworkMessage();
                if (qmsg) {
                    qmsg->data = NULL; qmsg->next = NULL;
                    qmsg->type = 4;
                    /* 0x439948 allocates the protocol's fixed 13-byte name
                     * buffer and copies the C string at payload +0x08.
                     * Raw char buffer, not a C++ object — safe as-is on any
                     * host. */
                    char* str = reinterpret_cast<char*>(operator_new(0x0D));
                    if (str) strcpy(str, reinterpret_cast<char*>(payload) + 8);
                    qmsg->data = str;
                    qmsg->to_player = player_id;
                    qmsg->flags = *reinterpret_cast<int32_t*>((reinterpret_cast<uint16_t*>(payload) + 2));
                }
                NETMAN_QueueMessage(qmsg);
            }
            break;
        }

        case 7: { /* 0x3F1 — LobbyInfo/PlayerList */
            uint16_t* p = reinterpret_cast<uint16_t*>(payload);
            this->player_peer_id = player_id;

            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) {
                qmsg->data = NULL; qmsg->next = NULL;
                qmsg->type = 9;
                qmsg->flags = *reinterpret_cast<int32_t*>((p + 2));
                qmsg->setMetadata0(static_cast<uint8_t>(p[4]));
                qmsg->setMetadata1(*reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(p) + 9)));

#ifndef _WIN32
                qmsg->data = DPLAY_DecodePlayerSlots(
                    static_cast<uint8_t*>(payload) + 0x0C);
#else
                /* Fixed-size raw player-slot buffer (0x2AC / 0x4C = 9
                 * fixed-size slots, copied byte-offset below), not a C++
                 * object — safe as-is. This branch is also Windows-only
                 * (the host path above uses DPLAY_DecodePlayerSlots
                 * instead). */
                void* data = operator_new(0x2AC);
                qmsg->data = data;
                if (data != nullptr) {
                    std::memset(data, 0, 0x2AC);
                    for (int offset = 0; offset < 0x2AC; offset += 0x4C) {
                        DPLAY_CopyPlayerData(
                            static_cast<uint8_t*>(data) + offset,
                            static_cast<uint8_t*>(payload) + 0x0C +
                                (offset / 0x4C) * 0x3C);
                    }
                }
#endif
            }
            NETMAN_QueueMessage(qmsg);
            break;
        }

        case 8: /* 0x3F2 — ConnectionSetup */
            this->HandleConnectionSetup(reinterpret_cast<void*>((reinterpret_cast<uint16_t*>(payload))));
            break;

        case 9: /* 0x3F3 — ControllerInit */
            this->HandleControllerInit(reinterpret_cast<void*>((reinterpret_cast<uint16_t*>(payload))), player_id);
            break;

        case 10: { /* 0x3F4 — PlayerJoin request */
            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) { qmsg->data = NULL; qmsg->next = NULL; qmsg->type = 0x13; }
            int idx = NETMAN_FindPlayerIndex(g_netman, player_id);
            if (qmsg) qmsg->to_player = idx;
            NETMAN_QueueMessage(qmsg);
            break;
        }

        case 0x0B: { /* 0x3F5 — PlayerCount */
            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) { qmsg->data = NULL; qmsg->next = NULL; qmsg->type = 0x14; }
            int idx = NETMAN_FindPlayerIndex(g_netman, player_id);
            if (qmsg) qmsg->to_player = idx;
            NETMAN_QueueMessage(qmsg);
            break;
        }

        case 0x0C: { /* 0x3F6 — TrainPosUpdate msg */
            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) { qmsg->data = NULL; qmsg->next = NULL;
                        qmsg->type = 0x15; qmsg->data = payload; }
            NETMAN_QueueMessage(qmsg);
            break;
        }

        case 0x0D: /* 0x3F7 — HandleTrainPosUpdate */
        {
            int idx = NETMAN_FindPlayerIndex(g_netman, player_id);
            if (idx >= 0) this->HandleTrainPosUpdate(
                static_cast<TrainPositionAckPacket*>(payload), idx);
            break;
        }

        case 0x0E: { /* 0x3F8 — PlayerCount/Update */
            uint16_t* p = reinterpret_cast<uint16_t*>(payload);
            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) {
                qmsg->data = NULL; qmsg->next = NULL;
                qmsg->type = 0x1A;
                qmsg->size = 0;
                qmsg->data = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>((static_cast<uint8_t>(p[2])))));
                qmsg->to_player = player_id;
            }
            NETMAN_QueueMessage(qmsg);
            this->UpdatePlayerCount(static_cast<uint32_t>((static_cast<uint8_t>(p[2]))));
            break;
        }

        case 0x0F: { /* 0x3F9 — Carriage data */
            int idx = NETMAN_FindPlayerIndex(g_netman, player_id);
            if (idx >= 0) {
                NetworkMsg* qmsg = AllocateNetworkMessage();
                if (qmsg) { qmsg->data = NULL; qmsg->next = NULL;
                            qmsg->type = 0x16; qmsg->data = payload; }
                NETMAN_QueueMessage(qmsg);
            }
            break;
        }

        case 0x10: /* 0x3FA — SendBuildingData */
            NETMAN_SendBuildingData(g_netman, player_id);
            break;

        case 0x11: /* 0x3FB — HandlePlayerJoin */
            this->HandlePlayerJoin(payload, player_id);
            break;

        case 0x12: /* 0x3FC — HandleAttachmentFileData */
            this->HandleAttachmentFileData(payload);
            break;

        case 0x13: /* 0x3FD — HandlePlayerLeave */
            this->HandlePlayerLeave(player_id);
            break;
        }

free_msg:
        /* 0x3F6, 0x3F7, and 0x3F9 transfer payload ownership to a
         * queued message. Every other DirectPlay payload is freed here. */
        {
            void* hHeap = GetProcessHeap();
            if (msg_type < 0x3F6 ||
                (msg_type > 0x3F7 && msg_type != 0x3F9)) {
                void* heap_payload = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(lpMem) + 4));
                if (heap_payload != NULL) HeapFree(hHeap, 0, heap_payload);
            }
            HeapFree(hHeap, 0, lpMem);
        }
    }
}


/* ================================================================== */
/* TrainSubsystem::HandlePlayerJoin                                    */
/* Address: 0x439D00                                                    */
/* ================================================================== */
void TrainSubsystem::HandlePlayerJoin(void* data, int player_id)
{
    char path_buf[0x144] = {0};
    (void)path_buf; /* used below via NET_GetAttFilePath */

    PlayerConnectionNode* node = reinterpret_cast<PlayerConnectionNode*>(operator_new(sizeof(PlayerConnectionNode)));
    if (node == NULL) return;

    node->player_id      = player_id;
    node->file_handle    = 0;
    node->sub_type       = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(data) + 4));
    node->extra_info     = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(data) + 6));
    node->transfer_state = 0;
    node->sequence_num   = 0;
    node->throttle       = 0;
    node->next           = NULL;

    /* Open .att file for reading */
    {
        char att_path[0x144];
        att_path[0] = 0;
        NET_GetAttFilePath(node->sub_type, 4, att_path);
        void* hFile = reinterpret_cast<void*>(static_cast<uintptr_t>(CreateFileA(att_path, 0x80000000, 1, NULL,
                                          4, 0x8000000, NULL)));
        node->file_handle = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hFile));
    }

    if (node->file_handle != -1) {
        /* Append to handle_list_1 */
        if (this->handle_list_1 == NULL) {
            this->handle_list_1 = node;
        } else {
            PlayerConnectionNode* tail = reinterpret_cast<PlayerConnectionNode*>(this->handle_list_1);
            while (tail->next) tail = reinterpret_cast<PlayerConnectionNode*>(tail->next);
            tail->next = node;
        }
    } else {
        GLOBAL_free(node);
    }
}


/* ================================================================== */
/* TrainSubsystem::UploadPendingAttachments                            */
/* Address: 0x439DF0                                                    */
/* ================================================================== */
void TrainSubsystem::UploadPendingAttachments()
{
    PlayerConnectionNode* prev = NULL;
    PlayerConnectionNode* node = reinterpret_cast<PlayerConnectionNode*>(this->handle_list_1);

    if (node == NULL) return;

    while (node != NULL) {
        if (node->throttle > 0) {
            node->throttle--;
            prev = node;
            node = reinterpret_cast<PlayerConnectionNode*>(node->next);
            continue;
        }
        node->throttle = 0x14; /* 20-tick throttle */

        if (node->transfer_state == 0) {
            /* FIRST block. Fixed-size raw network transfer buffer (0x10-byte
             * header + 0x7FDC payload bytes read via ReadFile below), not a
             * C++ object — safe as-is on any host. */
            uint16_t* buf = reinterpret_cast<uint16_t*>(operator_new(0x7FEC));
            uint32_t bytes_read = 0;

            if (!ReadFile(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)),
                           reinterpret_cast<void*>((reinterpret_cast<uint8_t*>(buf) + 0x0D)),
                           0x7FDC, &bytes_read, NULL)) {
                CloseHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)));
                node->file_handle = 0;
                GLOBAL_free(buf);
                goto remove_node;
            }

            buf[0] = 0x3FC;
            *reinterpret_cast<int32_t*>((buf + 2)) = bytes_read;
            buf[4] = node->sub_type;
            buf[5] = 0;
            *reinterpret_cast<uint8_t*>((buf + 6)) = 0; /* sub-type = FIRST */
            WIN32_SendNetworkData(g_dplay_peer, node->player_id,
                                  buf, bytes_read + 0x10, 1);
            GLOBAL_free(buf);
            node->transfer_state = 1;
            return;
        }

        if (node->transfer_state == 1) {
            /* INTERIM block. Same fixed-size raw network transfer buffer as
             * the FIRST block above — safe as-is. */
            uint16_t* buf = reinterpret_cast<uint16_t*>(operator_new(0x7FEC));
            uint32_t bytes_read = 0;

            if (!ReadFile(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)),
                           reinterpret_cast<void*>((reinterpret_cast<uint8_t*>(buf) + 0x0D)),
                           0x7FDC, &bytes_read, NULL)) {
                CloseHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)));
                node->file_handle = 0;
                GLOBAL_free(buf);
                goto remove_node;
            }

            if (bytes_read > 0) {
                node->sequence_num++;
                buf[0] = 0x3FC;
                *reinterpret_cast<int32_t*>((buf + 2)) = bytes_read;
                buf[4] = node->sub_type;
                buf[5] = node->sequence_num;
                *reinterpret_cast<uint8_t*>((buf + 6)) = 1; /* sub-type = INTERIM */
                WIN32_SendNetworkData(g_dplay_peer, node->player_id,
                                      buf, bytes_read + 0x10, 1);
                GLOBAL_free(buf);
                return;
            }

            /* Zero bytes -> move to FINAL */
            node->transfer_state = 2;
            CloseHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)));
            node->file_handle = 0;
            GLOBAL_free(buf);
            prev = node;
            node = reinterpret_cast<PlayerConnectionNode*>(node->next);
            continue;
        }

        /* FINAL block. Fixed-size raw network transfer buffer (header +
         * 0x400 bytes read via ReadFile below), not a C++ object — safe
         * as-is on any host. */
        {
            uint16_t* buf = reinterpret_cast<uint16_t*>(operator_new(0x410));
            char att_path[0x144] = {0};
            uint32_t bytes_read = 0;

            NET_GetFilePath(node->sub_type, 4, att_path);
            void* hFile = reinterpret_cast<void*>(static_cast<uintptr_t>(CreateFileA(att_path, 0x80000000, 1, NULL,
                                              4, 0x8000000, NULL)));
            node->file_handle = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hFile));

            if (hFile == reinterpret_cast<void*>(0xFFFFFFFF)) {
                node->file_handle = 0;
                GLOBAL_free(buf);
                goto remove_node;
            }

            if (ReadFile(hFile, reinterpret_cast<void*>((reinterpret_cast<uint8_t*>(buf) + 0x0D)), 0x400,
                         &bytes_read, NULL)) {
                node->sequence_num++;
                buf[0] = 0x3FC;
                *reinterpret_cast<int32_t*>((buf + 2)) = bytes_read;
                buf[4] = node->sub_type;
                buf[5] = node->sequence_num;
                *reinterpret_cast<uint8_t*>((buf + 6)) = 2; /* sub-type = FINAL */
                WIN32_SendNetworkData(g_dplay_peer, node->player_id,
                                      buf, bytes_read + 0x10, 1);
            }
            GLOBAL_free(buf);

            CloseHandle(hFile);
            node->file_handle = 0;
            NET_FindPlayer(4, node->sub_type);
            goto remove_node;
        }

remove_node:
        {
            PlayerConnectionNode* next = reinterpret_cast<PlayerConnectionNode*>(node->next);
            if (prev == NULL) {
                this->handle_list_1 = node->next;
            } else {
                prev->next = node->next;
            }
            GLOBAL_free(node);
            node = next;
        }
    }
}


/* ================================================================== */
/* TrainSubsystem::HandleAttachmentFileData                            */
/* Address: 0x43A140                                                    */
/* ================================================================== */
void TrainSubsystem::HandleAttachmentFileData(void* data)
{
    uint8_t  sub_type   = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(data) + 0x0C));
    uint16_t train_type = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(data) + 8));

    PlayerConnectionNode* node = reinterpret_cast<PlayerConnectionNode*>(this->handle_list_2);
    PlayerConnectionNode* prev = NULL;

    /* Find matching node */
    while (node != NULL) {
        if (node->sub_type == train_type) break;
        prev = node;
        node = reinterpret_cast<PlayerConnectionNode*>(node->next);
    }
    if (node == NULL) return;

    if (sub_type == 0) {
        /* FIRST block */
        char path_buf[0x144] = {0};
        uint32_t bytes_written;

        if (node->sequence_num == 0) {
            NET_GetAttFilePath(train_type, 5, path_buf);
            void* hFile = reinterpret_cast<void*>(static_cast<uintptr_t>(CreateFileA(path_buf, 0x40000000, 1, NULL,
                                              1, 0x8000000, NULL)));
            node->file_handle = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hFile));

            if (hFile != reinterpret_cast<void*>(0xFFFFFFFF)) {
                WriteFile(hFile, reinterpret_cast<void*>((reinterpret_cast<uint8_t*>(data) + 0x0D)),
                          *reinterpret_cast<uint32_t*>((reinterpret_cast<uint8_t*>(data) + 4)),
                          &bytes_written, NULL);
                return; /* success */
            }
        } else {
            g_OutputDebugStringA("Attachment First Block out of sequence");
        }

        /* Error path */
        {
            NetworkMsg* msg = AllocateNetworkMessage();
            if (msg) { msg->data = NULL; msg->next = NULL;
                       msg->type = 0x18;
                       msg->to_player = node->notify_id;
                       msg->setMetadata0(node->transfer_state); }
            NETMAN_QueueMessage(msg);
        }
        goto unlink_node;
    }

    if (sub_type == 1) {
        /* INTERIM block */
        uint32_t bytes_written;
        node->sequence_num++;
        uint16_t expected = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(data) + 10));

        if (node->sequence_num != expected) {
            g_OutputDebugStringA("Attachment Interim Block out of sequence");
            CloseHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)));
            node->file_handle = 0;

            NetworkMsg* msg = AllocateNetworkMessage();
            if (msg) { msg->data = NULL; msg->next = NULL;
                       msg->type = 0x18;
                       msg->to_player = node->notify_id;
                       msg->setMetadata0(node->transfer_state); }
            NETMAN_QueueMessage(msg);
            goto unlink_node;
        }

        if (!WriteFile(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)),
                        reinterpret_cast<void*>((reinterpret_cast<uint8_t*>(data) + 0x0D)),
                        *reinterpret_cast<uint32_t*>((reinterpret_cast<uint8_t*>(data) + 4)),
                        &bytes_written, NULL)) {
            CloseHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)));
            node->file_handle = 0;
        } else {
            return; /* success */
        }
    }

    /* FINAL block (sub_type >= 2) */
    {
        uint32_t bytes_written;
        char path_buf[0x144] = {0};

        if (node->file_handle) {
            CloseHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)));
            node->file_handle = 0;
        }

        node->sequence_num++;
        uint16_t expected = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(data) + 10));

        if (node->sequence_num == expected) {
            NET_GetFilePath(train_type, 5, path_buf);
            void* hFile = reinterpret_cast<void*>(static_cast<uintptr_t>(CreateFileA(path_buf, 0x40000000, 1, NULL,
                                              1, 0x8000000, NULL)));
            node->file_handle = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hFile));
            if (hFile != reinterpret_cast<void*>(0xFFFFFFFF)) {
                WriteFile(hFile, reinterpret_cast<void*>((reinterpret_cast<uint8_t*>(data) + 0x0D)),
                          *reinterpret_cast<uint32_t*>((reinterpret_cast<uint8_t*>(data) + 4)),
                          &bytes_written, NULL);
                CloseHandle(hFile);
            }
            node->file_handle = 0;
        }

        {
            NetworkMsg* msg = AllocateNetworkMessage();
            if (msg) { msg->data = NULL; msg->next = NULL;
                       msg->type = 0x18;
                       msg->to_player = node->notify_id;
                       msg->setMetadata0(node->transfer_state); }
            NETMAN_QueueMessage(msg);
        }
        goto unlink_node;
    }

unlink_node:
    if (prev == NULL) {
        this->handle_list_2 = node->next;
    } else {
        prev->next = node->next;
    }
    GLOBAL_free(node);
}


/* ================================================================== */
/* TrainSubsystem::HandleTrainPosUpdate                                */
/* Address: 0x43A4B0                                                    */
/* ================================================================== */
void TrainSubsystem::HandleTrainPosUpdate(TrainPositionAckPacket* packet,
                                           int32_t player_index)
{
    /* 0x43A4B0 forwards packet ownership to Netman as type 0x17. */
    NetworkMsg* message = AllocateNetworkMessage();
#ifndef _WIN32
    if (message == nullptr) {
        GLOBAL_free(packet);
        return;
    }
#endif
    message->type = 0x17;
    message->data = packet;
    message->flags = player_index;
    NETMAN_QueueMessage(message);

    int32_t local_slot_index;
#ifdef _WIN32
    local_slot_index = g_netman->m_mySlotIndex;
#else
    local_slot_index = NETMAN_HostLocalSlotIndex();
#endif
    if (packet->slot_index != static_cast<uint8_t>(local_slot_index)) return;

    Vehicle* vehicle = this->sprite_list_1;
    while (vehicle != nullptr &&
           static_cast<uint32_t>(vehicle->network_id) !=
               static_cast<uint32_t>(packet->network_id)) {
        vehicle = vehicle->next;
    }
    if (vehicle == nullptr) return;

    /* The binary replaces the list head with the matched node's successor,
     * even when the match is not the original head. */
    this->sprite_list_1 = vehicle->next;
    vehicle->next = nullptr;
    const uint16_t direction = vehicle->tunnel_angle;
    if (direction < 0x5B) {
        if (direction == 0x5A) vehicle->tunnel_angle = 0x10E;
        else if (direction == 0) vehicle->tunnel_angle = 0xB4;
    } else if (direction == 0xB4) {
        vehicle->tunnel_angle = 0;
    } else if (direction == 0x10E) {
        vehicle->tunnel_angle = 0x5A;
    }
    vehicle->peer_index = static_cast<uint8_t>(local_slot_index);

    vehicle->process_delay = 0;
    NetworkMsg* notification = AllocateNetworkMessage();
#ifndef _WIN32
    if (notification == nullptr) return;
#endif
    notification->type = 0x11;
    notification->data = vehicle;
    NETMAN_QueueMessage(notification);
}


/* ================================================================== */
/* TrainSubsystem::HandlePlayerLeave                                   */
/* Address: 0x43A5C0                                                    */
/* ================================================================== */
void TrainSubsystem::HandlePlayerLeave(int player_id)
{
    uint32_t player_index = NETMAN_FindPlayerIndex(g_netman, player_id);

    if (player_id == this->player_peer_id) {
        this->player_peer_id = 1;
    }

    this->UpdatePlayerCount(player_index);
    this->ResetMultiplayerState(player_id);

    /* Clean up receiver-side (handle_list_2) for this player */
    {
        PlayerConnectionNode* prev = NULL;
        PlayerConnectionNode* node = reinterpret_cast<PlayerConnectionNode*>(this->handle_list_2);

        while (node != NULL) {
            if (node->player_id == player_id) {
                if (prev == NULL) {
                    this->handle_list_2 = node->next;
                } else {
                    prev->next = node->next;
                }
                node->next = NULL;

                if (node->file_handle != 0 && node->file_handle != -1) {
                    CloseHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(node->file_handle)));
                    node->file_handle = 0;
                }

                NetworkMsg* msg = AllocateNetworkMessage();
                if (msg) {
                    msg->data = NULL; msg->next = NULL;
                    msg->type = 0x18;
                    msg->next = NULL;
                    msg->to_player = node->notify_id;
                    msg->setMetadata0(node->transfer_state);
                }
                NETMAN_QueueMessage(msg);
                GLOBAL_free(node);

                prev = NULL;
                node = reinterpret_cast<PlayerConnectionNode*>(this->handle_list_2);
            } else {
                prev = node;
                node = reinterpret_cast<PlayerConnectionNode*>(node->next);
            }
        }
    }

    /* Broadcast player-left message */
    NetworkMsg* broadcast = AllocateNetworkMessage();
    if (broadcast) {
        broadcast->data = NULL; broadcast->next = NULL;
        broadcast->type = 0x0B;
        broadcast->data = NULL;
        broadcast->to_player = player_id;
    }
    NETMAN_QueueMessage(broadcast);
}


/* ================================================================== */
/* TrainSubsystem::UpdatePlayerCount                                   */
/* Address: 0x43A6D0                                                    */
/* ================================================================== */
void TrainSubsystem::UpdatePlayerCount(uint32_t player_index)
{
    /* g_netman->m_mySlotIndex — self slot index, not a "local player" count. */
    uint32_t self_slot_index = static_cast<uint32_t>(g_netman->m_mySlotIndex);

    Vehicle* prev = nullptr;
    Vehicle* node = this->sprite_list_3;

    while (node != nullptr) {
        Vehicle* next_node = node->next;

        /* Disassembly at 0x43A6F0 (MOV DL, [ECX+0x78]; CMP EDX, EBP) reads
         * and compares Vehicle::slot_index (+0x78), NOT peer_index (+0x7C)
         * despite peer_index being the "owner" field other sprite_list_3
         * scans in this file key off (e.g. UpdateTrainMovement's
         * `node->peer_index`, 0x43BB00). Verified against the real bytes
         * at this address rather than assumed from sibling functions —
         * the write-back a few lines below (0x43A705/0x43A72F, MOV
         * [ECX+0x7C], BL) really is a different field, peer_index. */
        if (node->slot_index == static_cast<uint8_t>(player_index)) {
            /* Unlink this node from sprite_list_3 */
            if (prev == nullptr) {
                this->sprite_list_3 = next_node;
            } else {
                prev->next = next_node;
            }

            if (player_index == self_slot_index) {
                /* Local player's slot: reassign ownership to self and
                 * preserve on the sprite_list_1 free list for reuse. */
                node->peer_index = static_cast<uint8_t>(self_slot_index);
                node->next = this->sprite_list_1;
                this->sprite_list_1 = node;
            } else {
                /* Remote player: destroy. CALL dword ptr [vtable+0] at
                 * both call sites (0x43A71E and 0x43A748) with arg 1 —
                 * confirmed to be Vehicle's scalar deleting destructor
                 * (vtable[0], Vehicle::~Vehicle at 0x44C0B0, flags=1 to
                 * free memory), so plain `delete` is exact. */
                delete node;
            }
            node = next_node;
        } else {
            prev = node;
            node = next_node;
        }
    }
}


/* ================================================================== */
/* TrainSubsystem::ShutdownNetwork                                     */
/* Address: 0x43AA00                                                    */
/* ================================================================== */
void TrainSubsystem::ShutdownNetwork()
{
    if (this->byte_flag_2 != 0 || g_dplay_peer == NULL) {
        return;
    }

    this->player_peer_id = 0;

    if (g_dplay_peer->dplay_interface == nullptr) {
        /* No session — queue type-5 disconnect */
        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg) { msg->data = NULL; msg->next = NULL; }
        if (msg) { msg->type = 5; msg->data = NULL; }
        NETMAN_QueueMessage(msg);
        return;
    }

    /* Reconnect to session to send shutdown message */
    if (g_dplay_peer->session_state == 0) {
        /* Host player */
        if (g_netman->m_gameMode == 1) {
            /* Scenario mode: read server name from config */
            char buf[1024];
            Config_GetIniString(g_config_ini, "Configuration", "ServerName",
                                "LEGO International Train Server", buf, 0x400);
            g_dplay_peer->ConnectToSession(
                reinterpret_cast<char*>((reinterpret_cast<uint8_t*>(g_player_config) + 6)),
                buf, NULL);
            if (g_dplay_peer->session_ready == 0) {
                Sleep(1000);
                g_dplay_peer->ConnectToSession(
                    reinterpret_cast<char*>((reinterpret_cast<uint8_t*>(g_player_config) + 6)),
                    buf, NULL);
            }
            if (g_dplay_peer->session_ready == 0) {
                Sleep(2000);
                goto send_disconnect;
            }
        } else {
            /* Non-scenario: use UI address */
            char* addr = *reinterpret_cast<char**>((*reinterpret_cast<uintptr_t*>((reinterpret_cast<uint8_t*>(g_ui_main) + 0x220)) + 0xF8));
            g_dplay_peer->ConnectToSession(
                reinterpret_cast<char*>((reinterpret_cast<uint8_t*>(g_player_config) + 6)),
                addr, NULL);
        }
    } else {
        /* Client player: format name pair */
        char name_buf[256];
        wsprintfA(name_buf, "%s %s",
                  *reinterpret_cast<char**>((*reinterpret_cast<uintptr_t*>((reinterpret_cast<uint8_t*>(g_ui_main) + 0x220)) + 0xFC)),
                  reinterpret_cast<char*>((reinterpret_cast<uint8_t*>(g_player_config) + 6)));
        g_dplay_peer->ConnectToSession(
            reinterpret_cast<char*>((reinterpret_cast<uint8_t*>(g_player_config) + 6)),
            name_buf, NULL);
    }

    if (g_dplay_peer->session_ready != 0) {
        /* Connected — send type-3 shutdown */
        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg) { msg->data = NULL; msg->next = NULL; }
        if (msg) {
            msg->type = 3;
            msg->data = NULL;
            msg->to_player = g_dplay_peer->player_dpid;
        }
        NETMAN_QueueMessage(msg);
        return;
    }

send_disconnect:
    /* Could not connect — send type-5 disconnect directly */
    NetworkMsg* msg = AllocateNetworkMessage();
    if (msg) { msg->data = NULL; msg->next = NULL; }
    if (msg) { msg->type = 5; msg->data = NULL; }
    NETMAN_QueueMessage(msg);
}


/* ================================================================== */
/* TrainSubsystem::HandleDisconnect                                    */
/* Address: 0x43AC10                                                    */
/* ================================================================== */
void TrainSubsystem::HandleDisconnect()
{
    if (g_dplay_peer != NULL) {
        /* Send game-over notification if connected in scenario mode.
         * BUG (original): 0x43AC10 `PUSH ECX` allocates a 4-byte stack slot
         * seeded with the incoming `this`; 0x43AC42 overwrites only the low
         * word with 0x3FD, but 0x43AC3D sends all 4 bytes — so the upper 2
         * bytes on the wire are the high half of the this-pointer
         * (Ghidra's own decompile shows this as
         * `CONCAT22((short)((uint)param_1 >> 0x10), 0x3fd)`). Every
         * receiver in this file reads only the 16-bit type at payload
         * offset 0, so that garbage is protocol-inert. Reconstructed as a
         * genuinely 4-byte, deterministic buffer instead of reading past a
         * 2-byte local. */
        if (g_dplay_peer->session_ready != 0 && g_netman->m_gameMode == 2) {
            uint32_t game_over_msg = 0x3FD;
            WIN32_SendNetworkData(g_dplay_peer, 0, &game_over_msg, 4, 1);
            Sleep(10);
        }

        /* Close and destroy DirectPlay peer */
        g_dplay_peer->Close();
        DirectPlaySession* old_peer = g_dplay_peer;
        if (old_peer != NULL) {
            old_peer->DestroyPeer();
            GLOBAL_free(old_peer);
        }
        g_dplay_peer = NULL;
    }

    /* Only free lists in scenario mode or non-scenario */
    int scenario = g_netman->m_gameMode;
    if (scenario == 2 || scenario == 0) {
        /* Free sprite_list_1/2/3. Disassembly at 0x43AC99-0x43ACF1 shows all
         * three loops share one shape (0x43ACA0-0x43ACB5 for sprite_list_1,
         * 0x43ACBE-0x43ACD3 for sprite_list_2, 0x43ACDC-0x43ACF1 for
         * sprite_list_3): each iteration reads node->next (Vehicle::next,
         * +0x70 in the original x86 layout), stores it as the new list head
         * BEFORE destroying the old head, then invokes vtable[0] with
         * flags=1 (0x43ACAA/0x43ACAE, 0x43ACC8/0x43ACCC, 0x43ACE6/0x43ACEA)
         * — confirmed to be Vehicle's scalar deleting destructor
         * (Vehicle::~Vehicle, 0x44C0B0), the same call shape already
         * verified in UpdatePlayerCount (0x43A71E/0x43A748) and
         * ProcessMessages' sprite_list_1 purge (0x439790-0x4397AD) in this
         * file — so plain `delete` is exact. The redundant `TEST ECX,ECX`/
         * `JZ` pairs at 0x43ACA3/0x43ACA8 (etc.) test the still-live node
         * pointer against itself and are never taken; not a distinct
         * null-check branch. No prev-pointer/mid-list unlink logic here —
         * unlike UpdatePlayerCount, every node reachable from each list head
         * is destroyed, matching TrainSubsystem::BaseDtor's own
         * "frees all 3 sprite linked lists" documentation. */
        while (this->sprite_list_1 != nullptr) {
            Vehicle* node = this->sprite_list_1;
            this->sprite_list_1 = node->next;
            delete node;
        }

        while (this->sprite_list_2 != nullptr) {
            Vehicle* node = this->sprite_list_2;
            this->sprite_list_2 = node->next;
            delete node;
        }

        while (this->sprite_list_3 != nullptr) {
            Vehicle* node = this->sprite_list_3;
            this->sprite_list_3 = node->next;
            delete node;
        }
    }
}


/* ================================================================== */
/* TrainSubsystem::HandleFileTransfer                                  */
/* Address: 0x43AD00                                                    */
/* ================================================================== */
void TrainSubsystem::HandleFileTransfer(void* msg)
{
    NetworkMsg* net_msg = reinterpret_cast<NetworkMsg*>(msg);
    Vehicle*    car      = static_cast<Vehicle*>(net_msg->data);
    int         dir      = net_msg->flags;

    if (g_netman->m_gameMode != 2) {
        /* Not in multiplayer scenario — destroy car. Disassembly at
         * 0x43AD25-0x43AD2B (MOV EAX,[ESI]; PUSH 1; MOV ECX,ESI;
         * CALL [EAX]) is the same vtable[0]-with-flags=1 shape already
         * confirmed as Vehicle's scalar deleting destructor
         * (Vehicle::~Vehicle, 0x44C0B0) at every other call site in this
         * file (UpdatePlayerCount, HandleDisconnect) — plain `delete` is
         * exact here too. */
        if (car != NULL) {
            delete car;
            net_msg->data = NULL;
        }
        return;
    }

    /* Check if this is a 'remove' message. Vehicle::owner_handle (+0x04)
     * doubles as an inbound-train "destroy me" flag on this alias
     * (network/NetmanTypes.h: `using InboundTrainNode = Vehicle;`) — the
     * identical `node->owner_handle == 1` check gates net_delete(node) in
     * Netman::ReceiveGameStart (network/Netman.cpp, ~line 1158, address
     * 0x43E560) for the same InboundTrainNode/Vehicle alias. Confirmed at
     * THIS call site directly against the real bytes, not assumed from
     * that sibling: 0x43AD3B `CMP dword ptr [ESI+0x4],0x1` reads
     * car->owner_handle unconditionally (no null check before the read —
     * that really is the binary's own behavior), while the separate
     * `TEST ESI,ESI`/`JZ` pair at 0x43AD41/0x43AD43 guards only the
     * destroy call below it. Same vtable[0]/flags=1 destructor shape as
     * above (0x43AD45-0x43AD4B) — `delete` is exact here as well.
     * NOTE: the `car != NULL` guard below is provably dead under C++
     * semantics (car->owner_handle above already requires a non-null
     * car) — kept only to mirror the real TEST/JZ pair 1:1; do not "fix"
     * it by hoisting a null check above the owner_handle read, which
     * would silently change behavior relative to the binary. */
    if (car->owner_handle == 1) {
        if (car != NULL) {
            delete car;
        }
        net_msg->data = NULL;
        return;
    }

    /* Compute target town index from direction. g_netman->m_playerRows is the
     * grid's row stride (Ghidra ground truth via Netman::CheckTrackConnection
     * at 0x43DE30: this field is used as the position/row divisor and as the
     * right-edge bound — despite its name, it holds the *column* count; see
     * the loud TODO in Netman.cpp's CheckTrackConnection for the same
     * pre-existing name/role mismatch). Only the raw address matters here —
     * it is the field occupying +0xC, whatever its name says.
     *
     * Flattened vs. the pre-existing transcription: the real disassembly's
     * "no heading matched" fallthrough (0x43AD86 `XOR EAX,EAX` and the
     * equivalent tail of the dir>0x5A branch; Ghidra's own decompile shows
     * both converging on a shared `iVar4 = 0;` before LAB_0043ad94) sets
     * target_town to 0, not self_slot_index — the previous transcription's
     * `int target_town = self_slot_index;` initializer was wrong for that
     * case. Every real caller only ever sends one of the four
     * MirrorTrainHeading values, so the fallback is unreachable in
     * practice, but this now matches the binary for every possible `dir`. */
    int self_slot_index = g_netman->m_mySlotIndex;
    int target_town;
    if (dir == 0x5A)       target_town = self_slot_index + 1;
    else if (dir == 0)     target_town = self_slot_index - g_netman->m_playerRows;
    else if (dir == 0xB4)  target_town = g_netman->m_playerRows + self_slot_index;
    else if (dir == 0x10E) target_town = self_slot_index - 1;
    else                   target_town = 0;

    /* Find player at target town */
    PlayerSlot* player_slot = NULL;
    if (target_town >= 0) {
        player_slot = &g_netman->m_slots[target_town];
    }

    if (player_slot && player_slot->dpId != 0) {
        /* Player exists at target — transfer the car */
        uint8_t result = this->MoveToNeighborTown(player_slot->dpId, car, dir);
        if (static_cast<char>(result) == 0) {
            /* Transfer succeeded — forward the train */
            goto forward_train;
        }
        return;
    }

    /* No player at target — add train car locally */
    this->AddTrainCar(car, dir, target_town);
    return;

forward_train:
    /* Forward train to another player at the target: append `car` to the
     * tail of sprite_list_2. Disassembly at 0x43ADD6-0x43AE11 is exactly
     * AppendToVehicleList's shape (car->next cleared, then either becomes
     * the new head or is linked onto the existing tail found by walking
     * ->next) — re-verified directly against this call site's own bytes,
     * not assumed from the sibling append blocks the helper was
     * originally extracted from. */
    AppendToVehicleList(this->sprite_list_2, car);
    Train_RemoveAllTracks(this);
}


/* ================================================================== */
/* TrainSubsystem::HandleConnectionSetup                               */
/* Address: 0x43B240 (1162 bytes)                                      */
/*                                                                      */
/* Handles an inbound MSG_CONN_SETUP (0x3F2) wire message -- the exact  */
/* 0xB1C-byte layout produced by MoveToNeighborTown's `buf` (this file, */
/* above): type(2)/direction(2)@0x04/network_id(2)@0x06/max_steps(2)@0x08/ */
/* slot_index(1)@0x0A/active_editor(4)@0x0C/carriage_count(1)@0x14, then   */
/* 0x3A8-byte carriage records starting at +0x18, and a 10-byte primary-  */
/* editor name at +0xB10. `carriage_count` is an unbounded byte straight  */
/* off the wire (no clamp in the real code, reproduced as-is below); only */
/* MoveToNeighborTown's own producer happens to always emit <=3 records — */
/* a hostile/corrupt sender with a larger count walks past the 0xB1C      */
/* buffer and writes editors[4+], matching the original's own exposure.   */
/* Re-derived directly from raw disassembly (not decompiler pseudocode,   */
/* which mis-scaled several pointer-arithmetic terms -- see the per-field */
/* notes below); every offset here is cross-validated against             */
/* MoveToNeighborTown's producer, which serializes the exact same wire    */
/* format from the opposite side, and independently against the existing */
/* DPLAY_SessionData struct doc (network/DPlayManager.h).                 */
/*                                                                      */
/* ABI_BOUNDARY: `msg`/`record` below are raw external network-message   */
/* bytes, not a modeled game object -- byte arithmetic on them is the    */
/* legitimate wire-format case, not a field-access shortcut.             */
/* ================================================================== */
void TrainSubsystem::HandleConnectionSetup(void* data)
{
    const uint8_t* msg = static_cast<const uint8_t*>(data); /* ABI_BOUNDARY */

    /* Local DPlayManager staging object (0x43B261/0x43B269:
     * `LEA ECX,[ESP+0x28]; CALL 0x442850`), used to relay each carriage's
     * serialized DPLAY data on to the matching VehicleEditor below. Created
     * once, before the Vehicle is even allocated, matching instruction
     * order; reused/overwritten per carriage (0x43B566 passes the SAME
     * ESP+0x28 address to VehicleEditor::SetDPlayData on every iteration).
     * The original's paired `DPLAY_CleanupPlayer(&obj)` at the very end
     * (0x43B69B) only resets the vtable pointer for MSVC SEH-unwind safety
     * (DPlayManager::CleanupPlayer's own doc) -- compiler/EH scaffolding,
     * not reimplemented, matching the established omission at
     * Train_ConnectToServer (this file, "MSVC SEH not supported on GCC"). */
    DPlayManager local_session;
    local_session.CreatePlayer();

    /* Allocate the new Vehicle controller (real ctor at 0x44BE50, matching
     * Vehicle.h; disassembly at 0x43B29B-0x43B2A7 pushes resource_id=
     * *(int32*)(msg+0x10), 2, 1, 1 in that param order). */
    void* vehicle_mem = operator_new(sizeof(Vehicle));
    Vehicle* controller = nullptr;
    if (vehicle_mem != nullptr) {
        int32_t resource_id = *reinterpret_cast<const int32_t*>(msg + 0x10); /* ABI_BOUNDARY */
        controller = new (vehicle_mem) Vehicle(resource_id, 2, 1, 1);
    }
    /* BUG: the original unconditionally dereferences the ctor result at
     * 0x43B2CD with no null check -- an allocation failure crashes. This
     * guard is the file-wide safe idiom used by every other sibling here
     * (see e.g. HandleControllerInit's BUG note); unreachable in practice. */
    if (controller == nullptr) return;

    controller->tunnel_angle   = *reinterpret_cast<const uint16_t*>(msg + 0x04); /* direction */
    controller->network_id     = *reinterpret_cast<const uint16_t*>(msg + 0x06);
    controller->slot_index     = msg[0x0A];
    controller->CalcSpeed(*reinterpret_cast<const int16_t*>(msg + 0x08));
    controller->active_editor  = *reinterpret_cast<const int32_t*>(msg + 0x0C);

    uint8_t carriage_count = msg[0x14];
    int dplay_editor_index = 0; /* next unfilled slot in editors[1..3];
                                  * advances only for carriages that carry
                                  * DPLAY data (0x43B562/0x43B575: local_3b4
                                  * is incremented ONLY inside the
                                  * has_dplay-data branch, not every
                                  * iteration). */

    for (int i = 0; i < carriage_count; i++) {
        const uint8_t* record = msg + 0x18 + i * 0x3A8; /* ABI_BOUNDARY */

        int32_t resource_id_1 = *reinterpret_cast<const int32_t*>(record + 0x00);
        int32_t resource_id_2 = *reinterpret_cast<const int32_t*>(record + 0x04);
        controller->InitRoute(resource_id_1, resource_id_2, 1);

        bool has_dplay_data = record[0x08] != 0;
        if (has_dplay_data) {
            /* Populate the local DPlayManager staging object field-by-field
             * from the wire record. Every offset below is disassembly-
             * verified (object base = ESP+0x28 = &local_session; e.g.
             * 0x43B341 `MOV [ESP+0x30],EDX` writes object+8 = m_colorId
             * from source EDX = record+0x14 -- and so on for every field;
             * the two REP MOVSD block copies for m_sessionBlk1/2 (0x43B34C,
             * 0x43B35E) and m_playerName/m_trackEntries (0x43B397,
             * 0x43B3CC) land exactly on those fields' documented offsets
             * in network/DPlayManager.h with zero slack). The 2-byte pad
             * after m_magic (DPlayManager.h's `_pad_06`) is never written
             * here, matching the original -- it keeps whatever CreatePlayer
             * or a prior iteration left there. */
            local_session.m_magic    = *reinterpret_cast<const uint16_t*>(record + 0x10);
            local_session.m_colorId  = *reinterpret_cast<const int32_t*>(record + 0x14);
            local_session.m_configId = *reinterpret_cast<const int32_t*>(record + 0x18);
            memcpy(local_session.m_sessionBlk1, record + 0x1C, sizeof(local_session.m_sessionBlk1));
            memcpy(local_session.m_sessionBlk2, record + 0x31, sizeof(local_session.m_sessionBlk2));
            local_session.m_dwordValue = *reinterpret_cast<const int32_t*>(record + 0x48);
            local_session.m_wordValue  = *reinterpret_cast<const uint16_t*>(record + 0x46);
            local_session.color_r = record[0x4C];
            local_session.color_g = record[0x4D];
            local_session.color_b = record[0x4E];
            memcpy(local_session.m_playerName, record + 0x4F, sizeof(local_session.m_playerName));
            local_session.m_unknown93   = record[0x9F];
            local_session.m_playerType  = record[0xA0];
            local_session.m_playerTrack = record[0xA1];
            memcpy(local_session.m_trackEntries, record + 0xA2, sizeof(local_session.m_trackEntries));
            local_session.unknown_0x398 = *reinterpret_cast<const int32_t*>(record + 0x3A4);

            /* m_wordValue doubles as an attachment-request gate here (real
             * call target is Netman::GetPlayerCount, 0x43D210 -- despite
             * its name, and per its own class-level TODO, it returns
             * `m_currentSlot` cast to int32_t, not a count; inlined directly
             * below to avoid round-tripping that pointer through a
             * truncating int32_t on a 64-bit host). m_sessionBlk1/
             * m_sessionBlk2 are generic byte blocks in DPlayManager's own
             * doc, but THIS wire message uses them to carry two
             * null-terminated player-name strings for the lookups below.
             *
             * BUG: when m_gameMode != 2, the real 0x43D210 returns 0, and
             * the caller does `ADD EAX,5; CMP DL,[EAX]` (0x43B3EC-0x43B3F7)
             * -- an unconditional read of address 0x00000005, which
             * crashes. That is a reachable live-path bug in the original,
             * not an OOM edge case. `current_slot != nullptr` below turns
             * that crash into "treat as no match" instead, a deliberate
             * safe deviation matching this file's other guarded near-null
             * derefs (see HandleControllerInit's BUG note for the same
             * class of fix). */
            if (local_session.m_wordValue != 0) {
                PlayerSlot* current_slot = (g_netman->m_gameMode == 2) ? g_netman->m_currentSlot : nullptr;
                bool is_self = current_slot != nullptr &&
                    strcmp(reinterpret_cast<const char*>(local_session.m_sessionBlk1),
                           current_slot->compact_name) == 0;

                if (is_self) {
                    uint16_t* resp = reinterpret_cast<uint16_t*>(operator_new(8));
                    if (resp) {
                        resp[0] = 0x3FB;
                        resp[2] = local_session.m_wordValue;
                        resp[3] = NET_GetNextAttId();

                        PlayerSlot* target_player = nullptr;
                        for (int j = 0; j < g_netman->m_playerSlotCount; j++) {
                            PlayerSlot& slot = g_netman->m_slots[j];
                            if (slot.dpId != 0 &&
                                strcmp(slot.compact_name,
                                       reinterpret_cast<const char*>(local_session.m_sessionBlk2)) == 0) {
                                target_player = &slot;
                                break;
                            }
                        }

                        if (target_player == nullptr) {
                            local_session.m_wordValue = 0;
                        } else {
                            WIN32_SendNetworkData(g_dplay_peer, target_player->dpId, resp, 8, 1);
                            local_session.m_wordValue = resp[3];
                            controller->ack_counter++;

                            PlayerConnectionNode* node = reinterpret_cast<PlayerConnectionNode*>(
                                operator_new(sizeof(PlayerConnectionNode)));
                            if (node) {
                                node->player_id      = target_player->dpId;
                                node->sub_type       = resp[3];
                                node->extra_info     = resp[3];
                                node->transfer_state = controller->slot_index;
                                node->notify_id       = controller->network_id;
                                node->file_handle     = 0;
                                node->throttle        = 0;
                                node->sequence_num    = 0;
                                node->next            = this->handle_list_2;
                                this->handle_list_2   = node;
                            }
                        }
                        GLOBAL_free(resp);
                    }
                }
            }

            /* Relay the populated session data on to this carriage's
             * VehicleEditor (0x43B562-0x43B56D: SetDPlayData(editors[1+n],
             * &local_session)). */
            controller->editors[1 + dplay_editor_index]->SetDPlayData(&local_session);
            dplay_editor_index++;
        }
    }

    /* Set the primary editor's display name from the wire message's fixed
     * 10-byte name field (0x43B5A2-0x43B5AE: vtable[0x34/4=13] on
     * editors[0] -- VehicleEditor's vtable slot 13 is Entity::SetName,
     * unmodified, per core/VehicleEditor.h's vtable doc; MoveToNeighborTown
     * writes this exact wire offset from `car->editors[0]->name`, closing
     * the loop on both sides of the wire format). */
    controller->editors[0]->SetName(reinterpret_cast<const char*>(msg) + 0xB10); /* ABI_BOUNDARY */

    /* If this train belongs to the local player, remove the matching entry
     * from sprite_list_1 (0x43B5B9-0x43B601). Newly found via real
     * disassembly (not present in any prior transcription of this
     * function): `controller->owner_handle = 0` (0x43B5C4) unconditionally,
     * before the list search even runs.
     *
     * Deliberately NOT narrowed to uint8_t: the real compare
     * (0x43B5B7-0x43B5BC) zero-extends slot_index into EAX and compares the
     * full 32-bit m_mySlotIndex, which is documented as "0-8 or -1" — with
     * -1, no uint8_t slot_index can ever match. `slot_index` (uint8_t)
     * promotes to int for this comparison, reproducing that zero-extend
     * exactly; casting m_mySlotIndex down to uint8_t here would let a
     * network-supplied slot_index of 0xFF wrongly match m_mySlotIndex==-1. */
    if (controller->slot_index == g_netman->m_mySlotIndex) {
        controller->owner_handle = 0;

        Vehicle* prev = nullptr;
        Vehicle* node = this->sprite_list_1;
        while (node != nullptr) {
            if (node->network_id == controller->network_id) {
                if (prev == nullptr) {
                    this->sprite_list_1 = node->next;
                } else {
                    prev->next = node->next;
                }
                delete node; /* vtable[0] scalar deleting destructor, flag=1 */
                break;
            }
            prev = node;
            node = node->next;
        }
    }

    /* Broadcast MSG_CTRL_INIT (0x3F3, 10 bytes) to all players. */
    {
        uint16_t* ctrl_init = reinterpret_cast<uint16_t*>(operator_new(10));
        if (ctrl_init) {
            ctrl_init[0] = 0x3F3;
            ctrl_init[2] = controller->network_id;
            *(reinterpret_cast<uint8_t*>(ctrl_init) + 6) = controller->slot_index;
            *(reinterpret_cast<uint8_t*>(ctrl_init) + 7) = static_cast<uint8_t>(g_netman->m_mySlotIndex);
            ctrl_init[4] = controller->tunnel_angle;
            WIN32_SendNetworkData(g_dplay_peer, 0, ctrl_init, 10, 1);
            GLOBAL_free(ctrl_init);
        }
    }

    /* Set controller owner and queue type-0x11 (car-added) notification. */
    controller->peer_index = static_cast<uint8_t>(g_netman->m_mySlotIndex);
    controller->flag_8A = 0;

    {
        NetworkMsg* notify = AllocateNetworkMessage();
        if (notify) {
            notify->data = NULL; notify->next = NULL;
            notify->type = 0x11;
            notify->data = controller;
        }
        controller->init_flag = 0;
        NETMAN_QueueMessage(notify);
    }
}


/* ================================================================== */
/* TrainSubsystem::HandleControllerInit                                */
/* Address: 0x43B6D0                                                    */
/*                                                                      */
/* Handles an inbound MSG_CTRL_INIT (0x3F3) wire message -- the exact   */
/* 10-byte layout produced by AddTrainCar's `ctrl_init` buffer          */
/* (0x43B8C0-0x43B933, this file). Finds the matching car in            */
/* sprite_list_1 by (network_id, slot_index) [CMP word[EAX+0x7A],CX;    */
/* CMP byte[EAX+0x78],DL at 0x43B6E1-0x43B6ED], records the sender's    */
/* DPlay player ID and peer_index on it [0x43B6FE/0x43B707], then       */
/* re-broadcasts a type-0x12 notification carrying the same identifying */
/* fields [0x43B733-0x43B753].                                          */
/* ================================================================== */
void TrainSubsystem::HandleControllerInit(void* data, int dplay_id)
{
    /* ABI_BOUNDARY: raw MSG_CTRL_INIT wire buffer -- external network
     * message layout, not a modeled game object. Matches AddTrainCar's
     * `ctrl_init` producer (same file) byte-for-byte. */
    const uint16_t* p = reinterpret_cast<const uint16_t*>(data);
    uint16_t network_id = p[2];                                              /* +0x04 */
    uint8_t  slot_index = *(reinterpret_cast<const uint8_t*>(p) + 6);         /* +0x06 */
    uint8_t  peer_index = *(reinterpret_cast<const uint8_t*>(p) + 7);         /* +0x07 */
    uint16_t direction  = p[4];                                              /* +0x08 */

    /* Find matching car in sprite_list_1 by (network_id, slot_index) and
     * record the sender's DPlay ID + peer_index on it.
     *
     * +0x78/+0x7A/+0x7C are `slot_index`/`network_id`/`peer_index`, not the
     * general-Vehicle `color_r`/`player_id`/`color_g` union members, per
     * two independent disassembly sites (not inferred from AddTrainCar's
     * reconstruction):
     *   - Vehicle::Vehicle (0x44BE50): under `g_netman->m_gameMode == 2`
     *     writes `m_mySlotIndex` into both +0x78 and +0x7C (and a shared
     *     literal 1/1 otherwise) -- a multiplayer slot index, not a color
     *     channel.
     *   - RESDATA_ScriptedObject_CleanupChildren (0x44C0D0, 0x44C0EF-
     *     0x44C0FE): reads byte[+0x7C], byte[+0x78], word[+0x7A] together
     *     and passes them straight into NETMAN_ReceiveAck(0x440410) -- the
     *     same three fields travel together into a network-ack call, and
     *     the same function never touches +0x8C at all (see Vehicle.h). */
    for (Vehicle* car = this->sprite_list_1; car != nullptr; car = car->next) {
        if (car->network_id == network_id && car->slot_index == slot_index) {
            car->dplay_id = dplay_id;      /* +0x8C, see Vehicle.h union doc */
            car->peer_index = peer_index;  /* +0x7C */
            break;
        }
    }

    /* BUG: the original null-checks operator new (0x43B714-0x43B716) but
     * then writes dword ptr [EAX+8]=0 (0x43B72C) and dword ptr [EAX]=0x12
     * (0x43B733) unconditionally on both branches -- an allocation failure
     * (EAX==0) dereferences a null pointer and crashes. The `if (msg)`
     * guard below is the file-wide safe idiom already used by every other
     * sibling in this file (ResetMultiplayerState, HandleFileTransfer,
     * ProcessMessages); behavior differs from the original only on the
     * OOM path, which is unreachable in practice. */
    NetworkMsg* msg = AllocateNetworkMessage();
    if (msg) {
        msg->data = NULL; msg->next = NULL;
        msg->type = 0x12;
        msg->data = NULL;
        msg->flags = network_id;
        msg->setMetadata0(slot_index);
        msg->setMetadata1(peer_index);
        msg->size = direction;
    }
    NETMAN_QueueMessage(msg);
}


/* ================================================================== */
/* TrainSubsystem::ResetMultiplayerState                               */
/* Address: 0x43B770                                                    */
/* ================================================================== */
void TrainSubsystem::ResetMultiplayerState(int player_id)
{
    if (g_netman->m_gameMode != 2) return;

    uint32_t player_index;
    if (player_id == 0) {
        player_index = static_cast<uint32_t>(g_netman->m_mySlotIndex);
    } else {
        player_index = NETMAN_FindPlayerIndex(g_netman, player_id);
    }

    if (static_cast<int>(player_index) < 0) return;

    /* Walk sprite_list_1 releasing cars owned by (or awaiting a DPlay ack
     * from) the target player back to local ownership. Field mapping
     * re-verified independently against this function's own disassembly
     * (0x43B7C5-0x43B7E3), not assumed from HandleControllerInit's:
     *   MOV CL,[ESI+0x7C]; CMP ECX,EAX; JNZ +6; CMP EDI,EBX; JNZ match
     *     -> owner_match: player_id != 0 && peer_index == player_index
     *   MOV DL,[ESI+0x78]; CMP EDX,EAX; JNZ +6; CMP EDI,EBX; JZ match
     *     -> self_match:  player_id == 0 && slot_index == player_index
     *   CMP [ESI+0x8C],EDI; JNZ advance-to-next (else fall into match)
     *     -> dplay_match: dplay_id == player_id, checked unconditionally
     *        either way (so a full reset, player_id==0, also matches any
     *        car whose dplay_id happens to be 0). */
    Vehicle* node = this->sprite_list_1;
    while (node != nullptr) {
        const bool owner_match = player_id != 0 &&
            node->peer_index == static_cast<uint8_t>(player_index);
        const bool self_match = player_id == 0 &&
            node->slot_index == static_cast<uint8_t>(player_index);
        const bool dplay_match = node->dplay_id == player_id;

        if (!owner_match && !self_match && !dplay_match) {
            node = node->next;
            continue;
        }

        /* BUG: preserved from the original. The real unlink (0x43B7E9-
         * 0x43B7EC: `MOV EAX,[ESI+0x70]; MOV [this+0x14],EAX`) sets
         * sprite_list_1 unconditionally to this node's successor,
         * regardless of whether `node` is actually the current list
         * head — there is no equivalent of UpdatePlayerCount's
         * prev->next splice anywhere in this function's disassembly.
         * Any car walked past earlier in the same call without matching
         * becomes unreachable from sprite_list_1 the instant a later
         * car matches, since its predecessor's `next` link is never
         * rewritten. Real original behavior, not a transcription
         * shortcut — do not "fix" this into a correct prev-tracking
         * unlink without re-checking the assembly first. */
        Vehicle* next_node = node->next;
        this->sprite_list_1 = next_node;
        node->next = nullptr;

        /* Reverse this car's parked/handoff heading (same mirror table
         * as AddTrainCar, 0x43B8C0) and reassign ownership to the local
         * player's slot. */
        node->tunnel_angle = MirrorTrainHeading(node->tunnel_angle);
        node->peer_index = static_cast<uint8_t>(g_netman->m_mySlotIndex);

        /* Clear DPlay data on every carriage: editors[1..editor_count],
         * deliberately skipping the lead unit at editors[0] — the same
         * base/bound already established by Vehicle::GetOccupantCount
         * (0x44C370: "Scans editors[1..3]... skips slot 0 deliberately")
         * and by this exact "for (i=1;i<=editor_count;i++) editors[i]"
         * idiom already used lower in this file's HandleConnectionSetup
         * carriage-serialization loop. Vehicle::InitRoute (0x44C220)
         * bounds editor_count to 0-3 before ever writing a new editor
         * slot, so this can never read editors[] out of bounds. No null
         * guard on editors[i], matching the disassembly exactly
         * (0x43B845-0x43B85D has no TEST/JZ on the loaded pointer before
         * the CALL) — InitRoute's own invariant guarantees a populated
         * slot for every index in [1, editor_count]. */
        for (int i = 1; i <= node->editor_count; ++i) {
            node->editors[i]->SetDPlayData(nullptr);
        }

        /* Queue a type-0x11 "release" notification carrying the car,
         * then clear init_flag (+0x88) — matches the real write order
         * (0x43B87B-0x43B884 set the message fields, then the +0x88
         * store, both before NETMAN_QueueMessage's CALL at 0x43B891).
         * `if (msg)` guards the same alloc-failure null-deref bug
         * already documented earlier in this file (see
         * HandleControllerInit's BUG comment above) rather than
         * reproducing the crash. */
        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg) {
            msg->data = NULL; msg->next = NULL;
            msg->type = 0x11;
            msg->data = node;
        }
        node->init_flag = 0;
        NETMAN_QueueMessage(msg);

        node = this->sprite_list_1;
    }
}


/* ================================================================== */
/* TrainSubsystem::AddTrainCar                                         */
/* Address: 0x43B8C0                                                    */
/* ================================================================== */
void TrainSubsystem::AddTrainCar(Vehicle* car, int direction, int player_index)
{
    /* Check if player exists at target index (same "is this town
     * populated" proxy as RouteTrainAtEdge/UpdateTrainMovement). */
    PlayerSlot* player_slot = NULL;
    if (player_index >= 0) {
        player_slot = &g_netman->m_slots[player_index];
    }

    if (player_slot && player_slot->pixel_buffer != nullptr) {
        /* === Multiplayer path: prepend to sprite_list_3, broadcast 0x3F3 === */

        car->peer_index = static_cast<uint8_t>(player_index);
        /* Unconditional prepend (0x43B980-0x43B986) — unlike sprite_list_2's
         * append-to-tail idiom used below, sprite_list_3's insertion here is
         * always a plain head-prepend regardless of prior contents. */
        car->next = this->sprite_list_3;
        this->sprite_list_3 = car;

        /* Set live heading in field_76 (only the 4 canonical values are
         * ever written; anything else leaves field_76 untouched). */
        if (direction < 0x5B) {
            if (direction == 0x5A)      car->field_76 = 0x5A;
            else if (direction == 0)    car->field_76 = 0;
        } else {
            if (direction == 0xB4)      car->field_76 = 0xB4;
            else if (direction == 0x10E) car->field_76 = 0x10E;
        }

        /* Broadcast MSG_CTRL_INIT (0x3F3). Fixed-size raw network message
         * buffer (explicit byte offsets), not a C++ object.
         * ABI_BOUNDARY: wire-format layout for MSG_CTRL_INIT. */
        uint16_t* ctrl_init = reinterpret_cast<uint16_t*>(operator_new(10));
        if (ctrl_init) {
            ctrl_init[0] = 0x3F3;
            ctrl_init[2] = car->network_id;
            *reinterpret_cast<uint8_t*>(ctrl_init + 3) = car->slot_index;
            *(reinterpret_cast<uint8_t*>(ctrl_init) + 7) = car->peer_index;
            ctrl_init[4] = static_cast<uint16_t>(direction);
            WIN32_SendNetworkData(g_dplay_peer, 0, ctrl_init, 10, 1);

            /* Also update local handler */
            this->HandleControllerInit(ctrl_init, g_netman->m_myDpId);

            GLOBAL_free(ctrl_init);
        }

        /* Compute the car's initial grid position from the real
         * INPUT_DirToOffset_{Up,Left,Down,Right} helpers, matching
         * 0x43BA15-0x43BAE8 exactly (NOT a hand-rolled dx/dy table). Each
         * reads a globals-derived packed (Y<<16)|X offset and applies its
         * own +-1 adjustment.
         *
         * The heading==0 branch calls INPUT_DirToOffset_Down (0x41D950,
         * confirmed via the real CALL target at 0x43BA38) even though
         * heading 0 means "up" per field_76's own convention (matches
         * UpdateTrainMovement's steering and RouteTrainAtEdge's TOP-edge
         * angle 0). This is not a mismatch: a car whose live heading is
         * "up" (0) got there by exiting the SOURCE town's TOP edge and is
         * now entering the TARGET town — i.e. through the target town's
         * BOTTOM/"down" edge, continuing to travel up from there. The
         * helper name describes which edge of the target town the car
         * enters through, not the direction of travel; the same relation
         * holds for the other three (0x5A/right heading enters via the
         * target's Left edge, 0xB4/down heading via Right, 0x10E/left
         * heading via Up) — all four CALL targets were independently
         * re-verified against input/InputMgr.h's documented addresses. */
        uint16_t heading = car->field_76;
        int32_t off = 0;
        int16_t new_x = 0, new_y = 0;
        if (heading == 0) {            /* entering via target's "Down" edge */
            off = *INPUT_DirToOffset_Down(&off);
            new_x = static_cast<int16_t>(off) + 1;
            new_y = static_cast<int16_t>(off >> 16) - 1;
        } else if (heading == 0x5A) {   /* entering via target's "Left" edge */
            off = *INPUT_DirToOffset_Left(&off);
            new_x = static_cast<int16_t>(off) + 1;
            new_y = static_cast<int16_t>(off >> 16) + 1;
        } else if (heading == 0xB4) {   /* entering via target's "Right" edge */
            off = *INPUT_DirToOffset_Right(&off);
            new_x = static_cast<int16_t>(off) + 1;
            new_y = static_cast<int16_t>(off >> 16) + 1;
        } else if (heading == 0x10E) {  /* entering via target's "Up" edge */
            off = *INPUT_DirToOffset_Up(&off);
            new_x = static_cast<int16_t>(off) - 1;
            new_y = static_cast<int16_t>(off >> 16) + 1;
        }
        car->field_7E = new_x;
        car->field_80 = new_y;
        car->field_84 = -1; /* 0xFFFF sentinel */
        car->field_86 = -1;
        car->field_82 = 0;
        return;
    }

    /* === Single-player path: mirror heading once, append to sprite_list_2 ===
     * 0x43B8F1-0x43B92D stores the raw `direction` first (dead store,
     * immediately overwritten) then the mirrored value — net effect is a
     * single mirror for both the 4 canonical headings AND any other value
     * (ECX==EAX going into the compare chain per 0x43B8FC, and
     * MirrorTrainHeading's default case also returns its input unchanged),
     * so the collapse below is exact in every case, not just the 4
     * canonical ones. */
    car->tunnel_angle = MirrorTrainHeading(static_cast<uint16_t>(direction));
    AppendToVehicleList(this->sprite_list_2, car);
    Train_RemoveAllTracks(this);
}


/* ================================================================== */
/* TrainSubsystem::UpdateTrainMovement                                 */
/* Address: 0x43BB00                                                    */
/* Size: 1622 bytes                                                     */
/* ================================================================== */
void TrainSubsystem::UpdateTrainMovement()
{
    Vehicle* prev = nullptr;
    Vehicle* node = this->sprite_list_3;

    while (node != nullptr) {
        Vehicle* next_node = node->next;

        /* Check if player at node's town is still connected */
        uint8_t owner = node->peer_index;
        PlayerSlot* player_slot = &g_netman->m_slots[owner];

        if (player_slot->pixel_buffer == nullptr || player_slot->dpId != 0) {
            /* Owner disconnected — move from sprite_list_3 to sprite_list_2 */
            if (prev == nullptr) {
                this->sprite_list_3 = next_node;
            } else {
                prev->next = next_node;
            }
            node->next = nullptr;

            /* Mirror this car's parked heading once (0x43C079-0x43C0B4). */
            node->tunnel_angle = MirrorTrainHeading(node->tunnel_angle);

            AppendToVehicleList(this->sprite_list_2, node);
            NotifyAndDrainDeadList(this, static_cast<uint8_t>(g_netman->m_mySlotIndex));

            /* Continue scanning sprite_list_3 from the successor captured
             * above (0x43C075: EBX is set to next_node before the notify
             * loop runs and is never touched by it; 0x43C146 branches back
             * to the loop top with that same EBX). `prev` is deliberately
             * left unchanged here, matching the binary (nothing in
             * 0x43C04F-0x43C144 writes [ESP+0x14]/`prev`) — a prior
             * transcription restarted the scan from sprite_list_3's head
             * instead, which would reprocess already-visited nodes
             * whenever a dead node was found deeper than the list head. */
            node = next_node;
            continue;
        }

        /* === Check map edge routing === */
        int pos_x = node->field_7E;
        int pos_y = node->field_80;
        int map_width  = static_cast<int>(static_cast<int16_t>(player_slot->pixel_width));
        int map_height = static_cast<int>(static_cast<int16_t>(player_slot->pixel_height));

        bool routed = this->RouteTrainAtEdge(
            prev, node, pos_x, pos_y, map_width, map_height);

        if (routed) {
            prev = node;
            node = next_node;
            continue;
        }

        /* === Movement-steering: advance one tile === */
        uint16_t move_dir = node->field_76;
        int new_x = pos_x;
        int new_y = pos_y;
        int tile_row_size = map_width;
        uint8_t* tile_data_base = reinterpret_cast<uint8_t*>(player_slot->pixel_buffer);

        if (move_dir == 0x5A) {
            /* Moving right */
            if (*reinterpret_cast<uint8_t*>((tile_row_size * pos_y + pos_x + 1 + tile_data_base)) == 0x05) {
                new_x = pos_x + 1;
            } else {
                new_y = pos_y - 1;
                if (new_y >= 0 && *reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x + 1 + tile_data_base)) == 0x05) {
                    new_x = pos_x + 1;
                } else {
                    new_y = pos_y + 1;
                    if (new_y < map_height && *reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x + 1 + tile_data_base)) == 0x05) {
                        new_x = pos_x + 1;
                    } else if (*reinterpret_cast<uint8_t*>((tile_row_size * (pos_y - 1) + pos_x + tile_data_base)) == 0x05 && pos_y >= 1) {
                        new_y = pos_y - 1;
                    } else if (new_y < map_height && *reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x + tile_data_base)) == 0x05) {
                        node->field_76 = 0xB4;
                    } else {
                        new_y = pos_y;
                    }
                }
            }
        } else if (move_dir == 0) {
            /* Moving up */
            new_y = pos_y - 1;
            if (new_y < 0) new_y = 0;
            if (*reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x + tile_data_base)) != 0x05) {
                if (*reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x + 1 + tile_data_base)) == 0x05 && pos_x < map_width - 1)
                    new_x = pos_x + 1;
                else if (*reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x - 1 + tile_data_base)) == 0x05 && pos_x > 0)
                    new_x = pos_x - 1;
                else if (*reinterpret_cast<uint8_t*>((tile_row_size * pos_y + pos_x - 1 + tile_data_base)) == 0x05 && pos_x > 0) {
                    new_x = pos_x - 1; new_y = pos_y;
                } else if (*reinterpret_cast<uint8_t*>((tile_row_size * pos_y + pos_x + 1 + tile_data_base)) == 0x05 && pos_x < map_width - 1) {
                    new_x = pos_x + 1; new_y = pos_y;
                    node->field_76 = 0x5A;
                } else { new_y = pos_y; }
            }
        } else if (move_dir == 0xB4) {
            /* Moving down */
            new_y = pos_y + 1;
            if (new_y >= map_height) new_y = map_height - 1;
            if (*reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x + tile_data_base)) != 0x05) {
                if (*reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x + 1 + tile_data_base)) == 0x05 && pos_x < map_width - 1)
                    new_x = pos_x + 1;
                else if (*reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x - 1 + tile_data_base)) == 0x05 && pos_x > 0)
                    new_x = pos_x - 1;
                else if (*reinterpret_cast<uint8_t*>((tile_row_size * pos_y + pos_x - 1 + tile_data_base)) == 0x05 && pos_x > 0) {
                    new_x = pos_x - 1; new_y = pos_y;
                } else if (*reinterpret_cast<uint8_t*>((tile_row_size * pos_y + pos_x + 1 + tile_data_base)) == 0x05 && pos_x < map_width - 1) {
                    new_x = pos_x + 1; new_y = pos_y;
                    node->field_76 = 0x10E;
                } else { new_y = pos_y; }
            }
        } else if (move_dir == 0x10E) {
            /* Moving left */
            if (*reinterpret_cast<uint8_t*>((tile_row_size * pos_y + pos_x - 1 + tile_data_base)) == 0x05) {
                new_x = pos_x - 1;
            } else {
                new_y = pos_y - 1;
                if (new_y >= 0 && *reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x - 1 + tile_data_base)) == 0x05) {
                    new_x = pos_x - 1;
                } else {
                    new_y = pos_y + 1;
                    if (new_y < map_height && *reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x - 1 + tile_data_base)) == 0x05) {
                        new_x = pos_x - 1;
                    } else if (*reinterpret_cast<uint8_t*>((tile_row_size * (pos_y - 1) + pos_x + tile_data_base)) == 0x05 && pos_y >= 1) {
                        new_y = pos_y - 1;
                    } else if (new_y < map_height && *reinterpret_cast<uint8_t*>((tile_row_size * new_y + pos_x + tile_data_base)) == 0x05) {
                        node->field_76 = 0;
                    } else { new_y = pos_y; }
                }
            }
        }

        /* === Stuck detection === */
        if (new_x == pos_x && new_y == pos_y) {
            uint32_t rand_val = CRT_rand();
            int r = static_cast<int>((rand_val / 0x1FFF));
            switch (r % 4) {
            case 0: node->field_76 = 0x5A;  break;
            case 1: node->field_76 = 0x10E; break;
            case 2: node->field_76 = 0;     break;
            case 3: node->field_76 = 0xB4;  break;
            }
            node->field_84 = -1; /* 0xFFFF sentinel */
            node->field_86 = -1;
            node->field_82 = 0;
        } else {
            /* Update position */
            node->field_7E = static_cast<int16_t>(new_x);
            node->field_80 = static_cast<int16_t>(new_y);
            node->field_82 += 1;

            /* Check stuck counter loop */
            uint32_t rand_val = CRT_rand();
            int threshold = static_cast<int>((rand_val / 0x1999)) + 3;
            if (threshold < static_cast<int>(node->field_82)) {
                if (node->field_7E == node->field_84 && node->field_80 == node->field_86) {
                    uint32_t rv = CRT_rand();
                    int rr = static_cast<int>((rv / 0x1FFF));
                    switch (rr % 4) {
                    case 0: node->field_76 = 0x5A;  break;
                    case 1: node->field_76 = 0x10E; break;
                    case 2: node->field_76 = 0;     break;
                    case 3: node->field_76 = 0xB4;  break;
                    }
                    node->field_82 = 0;
                    node->field_84 = -1;
                    node->field_86 = -1;
                }
                node->field_82 = 0;
                /* 0x43BF3C-0x43BF46: copies the dword-aligned pair
                 * (field_7E, field_80) into (field_84, field_86) in one
                 * 32-bit move — equivalent to the two int16_t assignments
                 * below since both pairs are adjacent and same-endian. */
                node->field_84 = node->field_7E;
                node->field_86 = node->field_80;
            }

            /* === Build and send type-0x3F6 position update message === */
            {
                /* Allocate message buffer. Fixed-size raw network message
                 * buffer (explicit byte offsets below), not a C++ object —
                 * safe as-is on any host.
                 * ABI_BOUNDARY: wire-format layout for MSG_TRAIN_POS (0x3F6). */
                uint16_t* buf = reinterpret_cast<uint16_t*>(operator_new(0x2000));
                if (buf) {
                    buf[0] = 0x3F6;
                    *reinterpret_cast<uint8_t*>((buf + 1)) = node->peer_index;
                    *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(buf) + 4)) = 0;
                    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(buf) + 6)) = 1;
                    uint8_t c1 = node->slot_index;
                    uint8_t c2 = node->peer_index;
                    *reinterpret_cast<uint32_t*>((reinterpret_cast<uint8_t*>(buf) + 9)) =
                        (static_cast<uint32_t>(new_x) << 16) | node->network_id;
                    *reinterpret_cast<uint32_t*>((reinterpret_cast<uint8_t*>(buf) + 0x0D)) =
                        (c2 << 24) | (c1 << 16) | static_cast<uint16_t>(new_y);

                    /* Clone into heap allocation for queuing */
                    void* heap_buf = HeapAlloc(GetProcessHeap(), 0, 0x12);
                    if (heap_buf) {
                        memcpy(heap_buf, buf, 0x12);
                    }

                    /* Queue as type-0x15 (TrainPosUpdate broadcast) */
                    NetworkMsg* qmsg = AllocateNetworkMessage();
                    if (qmsg) {
                        qmsg->data = NULL; qmsg->next = NULL;
                        qmsg->type = 0x15;
                        qmsg->data = heap_buf;
                    }
                    NETMAN_QueueMessage(qmsg);

                    /* Queue as type-6 SendNetworkData message */
                    NetworkMsg* smsg = AllocateNetworkMessage();
                    if (smsg) {
                        smsg->data = NULL; smsg->next = NULL;
                        smsg->type = 6;
                        smsg->size = 0x12;
                        smsg->data = buf;
                        smsg->to_player = 0;
                        smsg->flags = 0;
                    }
                    this->QueueMessage(smsg);
                }
            }
        }

        prev = node;
        node = next_node;
    }
}


/* ================================================================== */
/* TrainSubsystem::RouteTrainAtEdge                                    */
/* Address: 0x43C160                                                    */
/* ================================================================== */
bool TrainSubsystem::RouteTrainAtEdge(Vehicle* prev_node, Vehicle* train,
                                       int pos_x, int pos_y,
                                       int map_width, int map_height)
{
    uint8_t owner = train->peer_index;

    /* NOTE ON A FIXED BUG: an earlier pass of this function gated these
     * four blocks on the wrong axis (pos_x/map_width where pos_y/map_height
     * belonged, and vice versa) while each block's own body (angle
     * constant, neighbor arithmetic, dpId checks) was already correct —
     * a systematic 90-degree rotation. Re-derived and fixed against the
     * complete raw disassembly of 0x43C160-0x43C40A. The real structure's
     * OUTERMOST test is `pos_y >= map_height - 1` (BOTTOM) — it must stay
     * first in this if/else-if chain, not just "some" order: at a
     * bottom-left corner tile (pos_x<1 AND pos_y>=map_height-1
     * simultaneously) the real binary takes BOTTOM unconditionally
     * (0x43C170-0x43C178 branches straight there without ever evaluating
     * pos_x), so BOTTOM must short-circuit TOP/LEFT/RIGHT exactly as
     * written below, not merely appear somewhere in the chain. */

    /* BOTTOM edge (pos_y >= map_height - 1): 0x43C178-0x43C214 (the
     * function's outermost fallthrough, taken whenever NOT `pos_y <
     * map_height - 1`). Angle 0xB4 ("down", matches Vehicle.h's field_76
     * convention). Neighbor = owner + g_netman[+0xC] (0x43C1BF ADD). This
     * is the only edge with a second gate on top of dpId!=0 —
     * `is_connected` (0x43C1DC) — verified against 0x43C1D8-0x43C1DF.
     * Bounce (no track) sets field_76 = 0 ("up", 0x43C210). */
    if (pos_y >= map_height - 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0xB4, owner);
        if (static_cast<char>(conn) != 0) {
            if (prev_node == nullptr) {
                this->sprite_list_3 = train->next;
            } else {
                prev_node->next = train->next;
            }
            train->next = nullptr;

            /* g_netman->m_playerRows: see the loud TODO in the identically-
             * shaped block in HandleFileTransfer above — this is the field
             * at raw offset +0xC (address-preserving; name/role mismatch
             * tracked separately, not fixed here). */
            int target_town = static_cast<int>(owner) + g_netman->m_playerRows;
            PlayerSlot* player_slot = NULL;
            if (target_town >= 0) {
                player_slot = &g_netman->m_slots[target_town];
            }
            if (player_slot && player_slot->dpId != 0 &&
                player_slot->is_connected != 0) {
                this->MoveToNeighborTown(player_slot->dpId, train, 0xB4);
                return true;
            }
            this->AddTrainCar(train, 0xB4, target_town);
            return true;
        }
        train->field_76 = 0;
    }
    /* TOP edge (pos_y < 1): 0x43C219-0x43C2AA (only reachable once BOTTOM
     * above is ruled out). Angle 0 ("up"). Neighbor = owner -
     * g_netman[+0xC] (0x43C264 SUB, not ADD — verified). Bounce sets
     * field_76 = 0xB4 ("down", 0x43C2A4). */
    else if (pos_y < 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0, owner);
        if (static_cast<char>(conn) != 0) {
            if (prev_node == nullptr) {
                this->sprite_list_3 = train->next;
            } else {
                prev_node->next = train->next;
            }
            train->next = nullptr;

            int target_town = static_cast<int>(owner) - g_netman->m_playerRows;
            PlayerSlot* player_slot = NULL;
            if (target_town >= 0) {
                player_slot = &g_netman->m_slots[target_town];
            }
            if (player_slot && player_slot->dpId != 0) {
                this->MoveToNeighborTown(player_slot->dpId, train, 0);
                return true;
            }
            this->AddTrainCar(train, 0, target_town);
            return true;
        }
        train->field_76 = 0xB4;
    }
    /* LEFT edge (pos_x < 1): 0x43C2AF-0x43C34C (only reachable once
     * BOTTOM and TOP above are ruled out). Angle 0x10E ("left"). Neighbor
     * = owner - 1 (simple decrement, 0x43C2F7 DEC — NOT the
     * g_netman[+0xC] stride; that stride only applies to the Y-axis
     * TOP/BOTTOM edges). Bounce sets field_76 = 0x5A ("right", 0x43C346). */
    else if (pos_x < 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0x10E, owner);
        if (static_cast<char>(conn) != 0) {
            if (prev_node == nullptr) {
                this->sprite_list_3 = train->next;
            } else {
                prev_node->next = train->next;
            }
            train->next = nullptr;

            int target_town = static_cast<int>(owner) - 1;
            PlayerSlot* player_slot = NULL;
            if (target_town >= 0) {
                player_slot = &g_netman->m_slots[target_town];
            }
            if (player_slot && player_slot->dpId != 0) {
                this->MoveToNeighborTown(player_slot->dpId, train, 0x10E);
                return true;
            }
            this->AddTrainCar(train, 0x10E, target_town);
            return true;
        }
        train->field_76 = 0x5A;
    }
    /* Not at the Y-axis (top/bottom) edge or the left edge: 0x43C358 JL
     * 0x43C405 returns false unconditionally here — this is the ONLY path
     * that continues normal per-tile movement this tick. */
    else if (pos_x < map_width - 1) {
        return false;
    }
    /* RIGHT edge (pos_x >= map_width - 1): 0x43C351-0x43C3DD. Angle 0x5A
     * ("right"). Neighbor = owner + 1 (0x43C397 INC). Bounce sets
     * field_76 = 0x10E ("left", 0x43C3E0). */
    else {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0x5A, owner);
        if (static_cast<char>(conn) != 0) {
            if (prev_node == nullptr) {
                this->sprite_list_3 = train->next;
            } else {
                prev_node->next = train->next;
            }
            train->next = nullptr;

            PlayerSlot* player_ptr = &g_netman->m_slots[owner + 1];
            if (player_ptr->dpId != 0) {
                this->MoveToNeighborTown(player_ptr->dpId, train, 0x5A);
                return true;
            }
            this->AddTrainCar(train, 0x5A, owner + 1);
            return true;
        }
        train->field_76 = 0x10E;
    }

    /* Bounce (no track connection): clear stuck counter */
    train->field_82 = 0;
    train->field_84 = -1; /* 0xFFFF sentinel */
    train->field_86 = -1;
    return true;
}


/* ================================================================== */
/* TrainSubsystem::MoveToNeighborTown                                  */
/* Address: 0x43AE20                                                    */
/* Size: 1016 bytes                                                     */
/* ================================================================== */
uint32_t TrainSubsystem::MoveToNeighborTown(int to_player, Vehicle* car, int direction)
{
    int self_slot_index = g_netman->m_mySlotIndex;
    int target_idx = NETMAN_FindPlayerIndex(g_netman, to_player);

    if (self_slot_index == target_idx) {
        /* === Local player === (0x43AE58-0x43AF09)
         * The binary mirrors `direction` twice back-to-back into
         * car->tunnel_angle (0x43AE9E then 0x43AECD) — mirroring is an
         * involution over the 4 canonical headings (and a no-op on any
         * other value), so the net effect is car->tunnel_angle==direction
         * unchanged. Collapsed to the single assignment below; the
         * intermediate mirrored value is never read back in between. */
        car->tunnel_angle = static_cast<uint16_t>(direction);
        AppendToVehicleList(this->sprite_list_2, car);
        Train_RemoveAllTracks(this);
        return 1;
    }

    /* === Remote player: serialize into 0xB1C-byte MSG_CONN_SETUP ===
     * Fixed-size raw network message buffer (memset + explicit byte
     * offsets below), not a C++ object — safe as-is on any host.
     * ABI_BOUNDARY: wire-format layout for MSG_CONN_SETUP (0x3F2). */
    uint8_t* buf = reinterpret_cast<uint8_t*>(operator_new(0xB1C));
    if (buf == NULL) return 0;

    memset(buf, 0, 0xB1C);

    *reinterpret_cast<uint16_t*>(buf) = 0x3F2;                                    /* message type */
    *reinterpret_cast<uint16_t*>(buf + 4) = static_cast<uint16_t>(direction);     /* direction */
    *reinterpret_cast<uint16_t*>(buf + 6) = car->network_id;                      /* +0x7A */
    *reinterpret_cast<uint8_t*>(buf + 10) = car->slot_index;                      /* +0x78 */
    *reinterpret_cast<uint16_t*>(buf + 8) = static_cast<uint16_t>(car->max_steps);/* +0x58 speed */
    *reinterpret_cast<int32_t*>(buf + 0x0C) = car->active_editor;                 /* +0x08 */

    /* Copy the primary editor's Entity::name (0x43AF98-0x43AFA9: the name
     * lives on car->editors[0], NOT on `car` itself — car+0x7C is
     * peer_index, a completely different field). Bounded to 10 chars;
     * Entity::name is char[11], so this can never truncate.
     *
     * No null guard on editors[0] here, matching the binary exactly: the
     * real code (0x43AF98 `MOV EDI,[EBX+0x10]`; 0x43AF9E `ADD EDI,0x7C`;
     * 0x43AFA3 `SCASB`) dereferences it completely unconditionally, with
     * no `TEST`/branch on EDI anywhere in that range. This is safe because
     * every Vehicle allocates editors[0] in its constructor (Vehicle.h's
     * ctor doc: "initial VehicleEditor sub-object") and, unlike
     * editors[1..3], slot 0 is never nulled by RemoveEditor or read past
     * (GetOccupantCount/DetachAll deliberately skip index 0 rather than
     * treating it as removable) — so a Vehicle reaching this call always
     * has a non-null editors[0]. */
    {
        const char* name_src = car->editors[0]->name;
        char* name_dst = reinterpret_cast<char*>(buf + 0xB10);
        for (int i = 0; i < 10 && name_src[i] != 0; i++) {
            name_dst[i] = name_src[i];
        }
    }

    /* Serialize additional carriages: editors[1..editor_count]
     * (0x43AFCA-0x43B063). Vehicle::InitRoute (0x44C220) bounds
     * editor_count to 0-3 before ever writing a new editor slot, so this
     * loop can never read past editors[3]. Each record is 0x3A8 bytes
     * starting at buf+0x18 (0x18 + N*0x3A8 for N in 0..2 — matching the
     * CRT_memset_pattern(buf+0x18, 0x3A8, 3, ...) placement-construction
     * of exactly 3 slots at the top of this function, and the buffer's
     * own 0xB1C size: 0x18 + 3*0x3A8 + 10-byte name + 2 pad == 0xB1C).
     * The decompiler's "* 0xEA + 6" pseudocode is wrong — verified
     * against the real LEA/MOV sequence at 0x43AFEB-0x43AFF4
     * (ECX*0x75*8 + 0x18 == carriage_count*0x3A8 + 0x18). */
    uint8_t carriage_count = 0;
    for (int i = 1; i <= car->editor_count; i++) {
        VehicleEditor* editor = car->editors[i];
        if (editor == nullptr) continue;

        uint8_t* record = buf + 0x18 + carriage_count * 0x3A8;
        *reinterpret_cast<int32_t*>(record) = VehicleEditor_GetResourceId(editor);
        *reinterpret_cast<int32_t*>(record + 4) = editor->res_id_2;

        void* dplay_data = VehicleEditor_GetDPlayData(editor);
        if (dplay_data) {
            record[8] = 1;
            /* ABI_BOUNDARY: raw DPlayManager wire copy (0x39C = 0xE7 dwords). */
            memcpy(record + 0x0C, dplay_data, 0x39C);
        } else {
            record[8] = 0;
        }
        carriage_count++;
    }
    buf[0x14] = carriage_count;

    /* Send the message */
    int send_result = WIN32_SendNetworkData(g_dplay_peer, to_player, buf, 0xB1C, 1);

    if (send_result == 0) {
        /* Send failed (0x43B0A3 onward) — keep the car locally: mirror its
         * current parked heading once and re-queue+notify on
         * sprite_list_2 (byte-identical to UpdateTrainMovement's dead-
         * owner purge loop — see NotifyAndDrainDeadList). */
        GLOBAL_free(buf);
        car->tunnel_angle = MirrorTrainHeading(car->tunnel_angle);
        AppendToVehicleList(this->sprite_list_2, car);
        NotifyAndDrainDeadList(this, static_cast<uint8_t>(g_netman->m_mySlotIndex));
    } else {
        /* Send succeeded (0x43B1A0-0x43B201) — ownership transfers to the
         * remote player. This branch does NOT touch tunnel_angle and does
         * NOT append to sprite_list_2 (the previous transcription did
         * both, incorrectly — verified against 0x43B1C2-0x43B201). */
        GLOBAL_free(buf);
        car->peer_index = static_cast<uint8_t>(NETMAN_FindPlayerIndex(g_netman, to_player));
        car->flag_8A = 0;
        if (car->owner_handle == 0) {
            /* Prototype/template car (0x43B1DF-0x43B1EC): keep it locally
             * on the active controller list. */
            car->next = this->sprite_list_1;
            this->sprite_list_1 = car;
        } else {
            /* Instance car (0x43B1EE-0x43B1FF): the remote peer now owns
             * the authoritative copy — destroy the local one. `init_flag`
             * must be set before delete, matching the binary's own
             * ordering (0x43B1F0 precedes the vtable[0](1) call). */
            car->init_flag = 1;
            delete car;
        }
    }

    return 1;
}


/* ================================================================== */
/* TrainSubsystem::HandleJoinMultiplayer                               */
/* Address: 0x43C410                                                    */
/* Size: 1089 bytes                                                     */
/*                                                                      */
/* Control-flow correction (2026-08-18): the previous transcription     */
/* split this function into two top-level siblings — "scenario mode"    */
/* (g_netman->m_gameMode==1) and a "free-play mode" block containing    */
/* the DirectPlay session/host/connect/DPlayManager-registration logic. */
/* That is NOT the real shape. Re-derived directly from the disassembly */
/* (0x43C448-0x43C851, all jump targets traced): there is exactly one    */
/* branch on g_netman->m_gameMode==1 at the top (0x43C448/0x43C44D).     */
/* Its "else" (mode!=1, 0x43C44F-0x43C45D) is a short leaf that either   */
/* destroys `car` (if non-null) or does nothing, and always returns      */
/* immediately — it can NEVER reach the DirectPlay/session code, and it  */
/* does NOT check car->owner_handle (0x43C451 tests only car==0; the     */
/* previous transcription's owner_handle gate on this branch was         */
/* fabricated, not evidenced). The entire DirectPlay-session/            */
/* DPlayManager-registration sequence lives ONLY inside the mode==1      */
/* branch, nested under owner_handle!=1 (0x43C46D) and sprite_list_1     */
/* being empty before this call (0x43C489 JZ 0x43C508) and neither demo  */
/* mode nor byte_flags being set (0x43C50E/0x43C51A) — i.e. it only runs */
/* when this is the very first car being added while g_netman->m_gameMode */
/* == 1 (NetmanTypes.h: 1 = hosting). Restructured below to match that   */
/* real nesting exactly; behavior for every input is unchanged from the  */
/* function as compiled, not from the previous (wrong) transcription.    */
/* ================================================================== */
void TrainSubsystem::HandleJoinMultiplayer(void* msg)
{
    NetworkMsg* net_msg = reinterpret_cast<NetworkMsg*>(msg);
    Vehicle*    car     = static_cast<Vehicle*>(net_msg->data);

    if (g_netman->m_gameMode == 1) {
        /* Unconditional dereference below matches the binary exactly
         * (0x43C462/0x43C465 read car->owner_handle / write
         * car->tunnel_angle with no null check on `car` at all) — there is
         * no top-of-function `if (car == NULL) return;` in the original;
         * that early-out was fabricated in the previous transcription and
         * is removed here. This message type is only ever queued with a
         * real car (matching HandleFileTransfer's identical "provably
         * dead but preserved" null-check note elsewhere in this file). */
        car->tunnel_angle = 32000; /* max timeout sentinel, 0x43C465 */

        if (car->owner_handle == 1) {
            /* Remove message: destroy car. The car!=nullptr guard mirrors
             * the real TEST/JZ pair at 0x43C471-0x43C473 1:1 (provably
             * dead given the owner_handle read above already requires a
             * non-null car, same as HandleFileTransfer's identical note). */
            if (car != nullptr) {
                delete car;
            }
            net_msg->data = nullptr;
            return;
        }

        /* owner_handle != 1: append car to the tail of sprite_list_1.
         * AppendToVehicleList's shape (clear car->next, then either set
         * head=car or walk-and-link) is equivalent to the binary's two
         * separate paths (0x43C48B-0x43C4A1 walk-then-append vs.
         * 0x43C508-0x43C50B set-as-head) because `car` is always a fresh
         * inbound node, never already linked into the list. */
        const bool had_cars = (this->sprite_list_1 != nullptr);
        AppendToVehicleList(this->sprite_list_1, car);

        if (had_cars) {
            /* 0x43C4A1: only checks g_demo_mode==1; no separate
             * byte_flags check on this path. */
            if (g_demo_mode != 1) {
                return;
            }
            /* Demo mode: drain sprite_list_1, notifying (type 0x0F) per
             * car — same idiom as ProcessMessages' scenario-mode unlink
             * loop (0x439790-0x4397AD). This function's own bytes at
             * 0x43C4C2/0x43C4CC/0x43C4CE (JZ-to-XOR-EAX,EAX fallthrough,
             * then an unconditional `MOV dword ptr [EAX],0xf`) write
             * msg->type even when operator_new returned null — same BUG
             * documented on RemoveAllCars, 0x43CBE0 — guarded here per
             * dispatch intent to match the already-established
             * ProcessMessages idiom instead. */
            while (this->sprite_list_1 != nullptr) {
                Vehicle* head = this->sprite_list_1;
                NetworkMsg* qmsg = AllocateNetworkMessage();
                if (qmsg) {
                    qmsg->data = nullptr; qmsg->next = nullptr;
                    qmsg->type = 0x0F;
                    qmsg->data = head;
                }
                head->init_flag = 0;
                this->sprite_list_1 = head->next;
                head->next = nullptr;
                NETMAN_QueueMessage(qmsg);
            }
            return;
        }

        /* sprite_list_1 was empty before this call: `car` is now the sole
         * entry. 0x43C7E5's loop-entry test checks ECX (== car, not
         * sprite_list_1) directly, but this is equivalent to testing
         * sprite_list_1 here — sprite_list_1 was just set to car at
         * 0x43C50B, and car was already proven non-null by the
         * owner_handle/tunnel_angle dereferences above. */
        if (g_demo_mode == 1 || this->byte_flags != 0) {
            /* Same drain-and-notify idiom as above (second copy). */
            while (this->sprite_list_1 != nullptr) {
                Vehicle* head = this->sprite_list_1;
                NetworkMsg* qmsg = AllocateNetworkMessage();
                if (qmsg) {
                    qmsg->data = nullptr; qmsg->next = nullptr;
                    qmsg->type = 0x0F;
                    qmsg->data = head;
                }
                head->init_flag = 0;
                this->sprite_list_1 = head->next;
                head->next = nullptr;
                NETMAN_QueueMessage(qmsg);
            }
            return;
        }

        /* First car, not in demo mode: establish (or reuse) the local
         * DirectPlay session, host it, and connect to the shared train
         * server, then register a DPlayManager player and flush the
         * freshly-appended solo car. None of this touches Vehicle fields;
         * copied here verbatim from its previous (mis-nested) location,
         * only its position in the control-flow tree is corrected. */
        if (g_dplay_peer == NULL) {
            /* Sized to the real class — see TrainSubsystem::InitNetwork's
             * own comment on why sizeof(DirectPlaySession) replaces the
             * original x86 struct's hardcoded 0x160c bytes. */
            auto* new_peer = static_cast<DirectPlaySession*>(operator_new(sizeof(DirectPlaySession)));
            if (new_peer != NULL) {
                new_peer->CreatePeer(this->context_id_a, 0);
            }
            g_dplay_peer = new_peer;

            if (g_dplay_peer) {
                /* See InitNetwork's identical override — deliberate
                 * caller-side policy, not a bug. */
                g_dplay_peer->error_callback = nullptr;
                g_dplay_peer->show_dialogs = 0;
                g_dplay_peer->hwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(this->context_id_b));
            }
        }

        if (g_dplay_peer && g_dplay_peer->session_ready != 0) {
            /* Only this FIRST session_ready check calls Train_SendPlayerInfo
             * (0x43C589-0x43C7DC/0x43CCC0). The two later session_ready
             * checks below (after each connect attempt) are bare returns
             * with no call at all (0x43C60F-0x43C615, 0x43C65D-0x43C669)
             * — the previous transcription called Train_SendPlayerInfo at
             * all three, which is not what the binary does. */
            Train_SendPlayerInfo(this);
            return;
        }

        if (g_dplay_peer == NULL) return;

        /* Host a new session */
        g_dplay_peer->Close();
        g_dplay_peer->HostSession(0, 1, 0, 0);
        Train_StartMultiplayer();

        if (g_dplay_peer->dplay_interface != nullptr) {
            /* Get server name from config and connect */
            char server_name[0x200];
            Config_GetIniString(g_config_ini, "Configuration", "ServerName",
                                "LEGO International Train Server",
                                server_name, 0x200);

            g_dplay_peer->ConnectToSession(
                reinterpret_cast<char*>((reinterpret_cast<uint8_t*>(g_player_config) + 6)),
                server_name, NULL);

            if (g_dplay_peer->session_ready != 0) {
                /* Bare return — no Train_SendPlayerInfo call here (see
                 * note above). */
                return;
            }

            /* Retry after close+re-host */
            g_dplay_peer->Close();
            Sleep(1000);
            g_dplay_peer->HostSession(0, 1, 0, 0);
            Train_StartMultiplayer();
            g_dplay_peer->ConnectToSession(
                reinterpret_cast<char*>((reinterpret_cast<uint8_t*>(g_player_config) + 6)),
                server_name, NULL);
        }

        if (g_dplay_peer && g_dplay_peer->session_ready != 0) {
            /* Bare return — no Train_SendPlayerInfo call here either (see
             * note above). */
            return;
        }

        /* Failed to connect — queue error message (type 0x1C) */
        {
            NetworkMsg* err_msg = AllocateNetworkMessage();
            if (err_msg) { err_msg->data = NULL; err_msg->next = NULL;
                          err_msg->type = 0x1C; err_msg->next = NULL; }
            NETMAN_QueueMessage(err_msg);
        }

        /* Create and register a DPLAY player for this session. DPLAY_CreatePlayer's
         * real address is 0x442850 (DPlayManager::CreatePlayer, already
         * implemented for real, network/DPlayManager.cpp) — this file's extern
         * declaration's own address comment, 0x4429C0, is fabricated (zero
         * xrefs anywhere in the binary; get_xrefs_to on the real 0x442850 lists
         * this exact function, TrainSubsystem::HandleJoinMultiplayer, as one of
         * its 7 real callers). Matches the operator_new+placement-new+
         * CreatePlayer() idiom already established at Train_ConnectToServer
         * (this file, below) and network/Netman.cpp:2333-2336; the
         * free-function DPLAY_CreatePlayer(void*) declared elsewhere in this
         * file bound to a no-op/nullptr-returning stub instead of this real
         * method (2026-08-15). */
        {
            void* storage = operator_new(sizeof(DPlayManager));
            DPlayManager* player = nullptr;
            if (storage != nullptr) {
                player = ::new (storage) DPlayManager();
                player->CreatePlayer();
            }
            if (player != nullptr) {
                char name_buf[0x50];
                FormatResourceString(&g_resmgr, 0xDF, name_buf, 0x50);
                memcpy(player->m_playerName, name_buf, sizeof(name_buf));

                player->InitPlayer(5, 1, 5, 0x94, 99, 0x48, 0x48);
                player->color_g = 0xFF;

                /* Copy player name from g_player_config + 6 (PlayerConfig::name).
                 * PlayerConfig::name is char[14]; this 0x14 (20)-byte copy is the
                 * original's own width and reads 6 bytes past name[] into the
                 * adjacent PlayerConfig fields — transcribed as-evidenced, not
                 * narrowed to sizeof(name), since the sibling Train_ConnectToServer
                 * block (below) uses a different call (strcpy) on the same source
                 * and neither this file's disassembly nor the caller proves which
                 * width is intentional vs. a pre-existing over-read in the
                 * original binary. */
                memcpy(player->m_sessionBlk1,
                       reinterpret_cast<uint8_t*>(g_player_config) + 6, 0x14);
                /* Copy "LEGO LOCO" as session name */
                memcpy(player->m_sessionBlk2, "LEGO LOCO", 10);

                NET_RegisterPlayer(g_dplay, player, 1, 0);

                player->~DPlayManager();
                GLOBAL_free(player);
            }
        }

        /* Remove all existing cars from sprite_list_1 to join fresh (third
         * copy of the same drain-and-notify idiom). */
        while (this->sprite_list_1 != nullptr) {
            Vehicle* head = this->sprite_list_1;
            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) {
                qmsg->data = nullptr; qmsg->next = nullptr;
                qmsg->type = 0x0F;
                qmsg->data = head;
            }
            head->init_flag = 0;
            this->sprite_list_1 = head->next;
            head->next = nullptr;
            NETMAN_QueueMessage(qmsg);
        }
        return;
    }

    /* g_netman->m_gameMode != 1 (0x43C44F-0x43C45D): unconditionally
     * destroy `car` if present. No owner_handle check exists on this path
     * in the binary — 0x43C451 tests only car==0. */
    if (car != nullptr) {
        delete car;
    }
    net_msg->data = nullptr;
}

/**
 * TrainSubsystem::RemoveAllCars
 * Address: 0x43CBE0
 */
void TrainSubsystem::RemoveAllCars()
{
    while (this->sprite_list_1 != NULL) {
        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg != NULL) {
            msg->data = NULL;
            msg->next = NULL;
        }

        /* 0x43CC0B writes through msg unconditionally, matching the binary
         * (a real null-deref if AllocateNetworkMessage failed — see
         * network/TrainMessage.h; not introduced by this rewrite).
         * BUG: no null check on `msg` here, matching 0x43CC03. */
        msg->type = 0x0F;
        Vehicle* car = this->sprite_list_1;
        msg->data = car;
        car->init_flag = 0;
        this->sprite_list_1 = car->next;
        car->next = nullptr;
        NETMAN_QueueMessage(msg);
    }
}

/* ================================================================== */
/* Train_QueueMessage — free-function bridge (Netman.h declaration)    */
/* Address: 0x4393D0 (TrainSubsystem::QueueMessage)                    */
/*                                                                      */
/* Netman.h declares `void Train_QueueMessage(void* train,             */
/* TrainMessage* msg)`; the old defsym/link stubs declared              */
/* (void*, void*) instead, so the mangled reference stayed unresolved   */
/* and every call jumped to address 0 (silent crash when reached — the */
/* ready-Go handoff's NETMAN_SendDisconnect).  The real binary body is  */
/* TrainSubsystem::QueueMessage; forward to it.                        */
/*                                                                      */
/* Forward-declared here (matching network/Netman.h's C++-linkage      */
/* declaration exactly) rather than via #include: Netman.h's own       */
/* extern "C" block redeclares several symbols this file already       */
/* declares with different signatures (CreateFileA, DPLAY_CreatePlayer, */
/* NET_RegisterPlayer, etc.), so including it here would trade one     */
/* -Werror=missing-declarations site for a pile of ambiguating/         */
/* conflicting-declaration errors. See docs/landmine-sweep-worklist.md */
/* for that finding. */
/* ================================================================== */
void Train_QueueMessage(void* train, TrainMessage* msg);

void Train_QueueMessage(void* train, TrainMessage* msg)
{
    if (train != nullptr && msg != nullptr) {
        static_cast<TrainSubsystem*>(train)->QueueMessage(msg);
    }
}

/* ================================================================== */
/* Train_ConnectToServer — network message 0x3EB handler               */
/* Address: 0x43C860-0x43CBD1 (__thiscall; ECX=subsystem, one stack     */
/* argument = payload). Previously mis-annotated 0x0043CDD0, which is   */
/* mid-body of Train_SendPlayerInfo.                                    */
/*                                                                      */
/* Declared and called (case 1, message type 0x3EB) inside              */
/* TrainSubsystem::ProcessMessages, but had no body anywhere in the     */
/* tree — nm showed it undefined and the real call site compiled to     */
/* `call 0` (guaranteed crash if ever reached). ProcessMessages'          */
/* whole caller chain is currently dead on the host build, so this was  */
/* never exercised, but it is fully decompiled/disassembled here rather */
/* than stubbed, per this project's practice for evidenced-but-not-yet- */
/* wired functions.                                                     */
/*                                                                      */
/* Bit 0 of the payload's flag byte (payload+8) selects the path:       */
/*   - set:   close and re-host the local DirectPlay session, then      */
/*            connect to the server named by the "ServerName" ini key   */
/*            (default "LEGO International Train Server"), retrying     */
/*            the host+connect sequence once on failure. On success,    */
/*            returns immediately — no cleanup runs.                    */
/*   - clear: builds "http:\\<payload string>" (the literal two          */
/*            backslashes match the raw bytes of the format string at   */
/*            0x47EAF8 — not a typo) and, if every non-alphanumeric      */
/*            character in the result is one of ",-.:;\@_" (per the      */
/*            signed-byte range checks at 0x43CA06-0x43CA44; the         */
/*            `c != 0x2f2f` arm can never match a single byte and is     */
/*            preserved as dead code, matching the binary), opens it     */
/*            with ShellExecuteA "open". This branch ALWAYS falls        */
/*            through to the shared cleanup below, even when the URL     */
/*            opens successfully — there is no early return here.       */
/*                                                                      */
/* If the payload's address string (payload+9) is empty, neither branch */
/* runs at all and control goes straight to cleanup.                    */
/*                                                                      */
/* Shared cleanup (both failure paths, and the browser branch           */
/* unconditionally): tears down the DirectPlay peer, resets              */
/* player_peer_id, queues an internal message (type 0x1C), constructs   */
/* a placeholder local DPlayManager player and registers it, then       */
/* flushes every car queued on sprite_list_1 (type 0x0F RemoveCar        */
/* messages, matching TrainSubsystem::RemoveAllCars).                    */
/*                                                                      */
/* MSVC's SEH frame (the ExceptionList/FS:[0] chain at the top of the   */
/* real function) is compiler-generated scaffolding, not reimplemented  */
/* here — see PoolAllocator::Shutdown in network/NetHelpers.cpp for the */
/* same established omission ("MSVC SEH not supported on GCC").         */
/* ================================================================== */
void Train_ConnectToServer(void* subsystem, void* payload)
{
    TrainSubsystem* self = static_cast<TrainSubsystem*>(subsystem);
    char* address = static_cast<char*>(payload) + 9;

    if (strlen(address) != 0) {
        uint8_t flags = *(static_cast<uint8_t*>(payload) + 8);

        if ((flags & 1) == 1) {
            /* Host mode: re-host and connect to the configured train server. */
            g_dplay_peer->Close();
            Sleep(10);
            g_dplay_peer->HostSession(0, 0, 0, 0);
            int32_t protocol = Config_GetIniInt(g_config_ini, "Configuration", "Protocol", 2);
            DirectPlay_HandleMessages(protocol, address, 0);

            if (g_dplay_peer->dplay_interface != nullptr) {
                char server_name[0x4B4];
                Config_GetIniString(g_config_ini, "Configuration", "ServerName",
                                     "LEGO International Train Server",
                                     server_name, 0x4B4);
                g_dplay_peer->ConnectToSession(
                    reinterpret_cast<char*>((reinterpret_cast<uint8_t*>(g_player_config) + 6)),
                    server_name, NULL);

                if (g_dplay_peer->session_ready == 0) {
                    /* Retry once: close, wait, re-host, reconnect. Reuses
                     * the same protocol/address values (never recomputed). */
                    g_dplay_peer->Close();
                    Sleep(1000);
                    g_dplay_peer->HostSession(0, 1, 0, 0);
                    DirectPlay_HandleMessages(protocol, address, 0);
                    g_dplay_peer->ConnectToSession(
                        reinterpret_cast<char*>((reinterpret_cast<uint8_t*>(g_player_config) + 6)),
                        server_name, NULL);
                }

                if (g_dplay_peer->session_ready != 0) {
                    return;  /* connected — no cleanup */
                }
            }
        } else {
            /* Browser mode: build "http:\\<address>" and open it if every
             * non-alphanumeric byte in it validates. Always falls through
             * to cleanup below, even on a successful open. */
            self->byte_flags = 1;

            char url_buf[0x200];
            wsprintfA(url_buf, "http:\\\\%s", address);

            bool bad = false;
            for (char* p = url_buf; *p != '\0'; ++p) {
                if (IsCharAlphaNumericA(*p)) continue;
                /* Signed: the real disassembly sign-extends the byte
                 * (MOVSX) and uses JG/JGE/JL/JLE range checks. */
                int c = static_cast<signed char>(*p);
                if (c < 0x3c) {
                    if (c < 0x3a && (c < 0x2c || c > 0x2e)) bad = true;
                } else if (c < 0x5d) {
                    if (c != 0x5c && c != 0x40) bad = true;
                } else if (c != 0x5f && c != 0x2f2f) {
                    bad = true;
                }
                if (bad) break;
            }
            if (!bad) {
                ShellExecuteA(nullptr, "open", url_buf, nullptr, nullptr, 0);
            }
        }
    }

    /* --- Shared cleanup --- */
    g_dplay_peer->Close();
    self->player_peer_id = 0;

    {
        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg) { msg->data = NULL; msg->next = NULL; }
        /* 0x43CA96/0x43CA9C/0x43CA9F write through msg unconditionally,
         * matching the binary (null-deref on allocation failure is the
         * original behavior, preserved as-is — same pattern documented at
         * RemoveAllCars's 0x43CC0B). */
        msg->type = 0x1C;
        msg->next = NULL;
        msg->data = NULL;
        NETMAN_QueueMessage(msg);
    }

    {
        /* 0x43CACC calls DPlayManager::CreatePlayer (0x442850) — its own
         * cross-reference list names Train_ConnectToServer as a caller.
         * The free-function DPLAY_CreatePlayer this file used to declare
         * separately (fabricated address 0x4429C0, zero xrefs) has been
         * removed — TrainSubsystem::HandleJoinMultiplayer above was its
         * one real caller, now fixed to use the same idiom as this block.
         * Matches the operator_new+placement-new+CreatePlayer() idiom
         * already established at network/Netman.cpp:2333-2336. */
        void* storage = operator_new(sizeof(DPlayManager));
        DPlayManager* player = nullptr;
        if (storage != nullptr) {
            player = ::new (storage) DPlayManager();
            player->CreatePlayer();
        }

        /* 0x43CAD3 onward writes through `player` unconditionally even if
         * allocation failed (player stays null) — matches the binary; the
         * only null guard in the original is on the destroy call below. */
        FormatResourceString(&g_resmgr, 0xDF, player->m_playerName, 0x50);
        player->color_g = 0xFF;
        strcpy(reinterpret_cast<char*>(player->m_sessionBlk1),
               reinterpret_cast<char*>((reinterpret_cast<uint8_t*>(g_player_config) + 6)));
        strcpy(reinterpret_cast<char*>(player->m_sessionBlk2), "LEGO LOCO");

        NET_RegisterPlayer(g_dplay, player, 1, 0);

        if (player != nullptr) {
            player->~DPlayManager();
            GLOBAL_free(player);
        }
    }

    while (self->sprite_list_1 != NULL) {
        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg) { msg->data = NULL; msg->next = NULL; }
        msg->type = 0x0F;
        msg->data = self->sprite_list_1;
        *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(self->sprite_list_1) + 0x88)) = 0;
        self->sprite_list_1 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(self->sprite_list_1) + 0x70));
        *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(msg->data) + 0x70)) = NULL;
        NETMAN_QueueMessage(msg);
    }
}

/* ================================================================== */
/* Train_RemoveAllTracks — free-function bridge, sprite_list_2 drain    */
/* Address: 0x43CC40-0x43CCB2 (__fastcall; ECX=subsystem, no stack       */
/* args). Previously mis-annotated 0x0043CA50, which is mid-body of     */
/* Train_ConnectToServer.                                                */
/*                                                                      */
/* Declared and called (3 sites) but had no body anywhere in the tree — */
/* nm showed it undefined and every call site compiled to `call 0`.     */
/* Its only reachable ancestor caller, TrainSubsystem::UpdateTrainMovement,   */
/* has zero callers on the host build today, so this is currently dead  */
/* code too; implemented fully rather than stubbed, matching the        */
/* Train_ConnectToServer rationale above.                               */
/*                                                                      */
/* Operates on sprite_list_2 (+0x18) — the "dead/orphaned train car      */
/* list" per Train.h's canonical field layout. Ghidra's own auto-comment*/
/* at this address calls it "track_list"; the canonical name used here  */
/* is sprite_list_2, matching TrainSubsystem's documented field table.  */
/*                                                                      */
/* For each node still on sprite_list_2: allocates a type-0x11          */
/* NetworkMsg carrying the node, stamps the node's owner byte (+0x7C)   */
/* with the low byte of g_netman+0x7D0 (the same idiom used for the     */
/* sprite_list_2 re-notify block inside                                 */
/* TrainSubsystem::UpdateTrainMovement, e.g. its line "*(uint8_t*)(cur +  */
/* 0x7C) = *(uint8_t*)((uint8_t*)g_netman + 0x7D0);"), and queues the    */
/* message.                                                              */
/*                                                                      */
/* PRESERVED ORIGINAL BUG (verified against disassembly, not just the   */
/* decompiler's own comment): at 0x43CC8D the node's next pointer        */
/* (+0x70) is cleared to 0 *before* 0x43CC96-0x43CC99 re-reads that same */
/* field to advance the list head — so `sprite_list_2 = node->next`      */
/* always observes 0. Only the first node is ever removed per call; the */
/* rest of the list is silently leaked (never freed, never revisited).  */
/* Preserved as-is, matching this project's established practice of      */
/* keeping a verified original bug rather than "fixing" it (see          */
/* TrainSubsystem::RemoveAllCars's comment on its own unconditional      */
/* write at 0x43CC0B for the same style of preserved-quirk documentation,*/
/* though that is a correct function — a different, similarly-named      */
/* symbol in this same file).                                           */
/* ================================================================== */
void Train_RemoveAllTracks(void* subsystem)
{
    TrainSubsystem* self = static_cast<TrainSubsystem*>(subsystem);

    while (self->sprite_list_2 != NULL) {
        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg != NULL) {
            msg->data = NULL;
            msg->next = NULL;
        }
        /* 0x43CC65 writes through msg unconditionally, matching the binary. */
        msg->type = 0x11;
        msg->data = self->sprite_list_2;

        uint8_t* node = reinterpret_cast<uint8_t*>(self->sprite_list_2);
        node[0x88] = 0;
        node[0x7C] = static_cast<uint8_t>(g_netman->m_mySlotIndex);

        /* BUG (0x43CC8D clears +0x70 before 0x43CC96-0x43CC99 re-reads it
         * to advance the head) — preserved as-is; see doc comment above. */
        *reinterpret_cast<void**>((node + 0x70)) = NULL;
        node[0x88] = 0;  /* redundant with the write above; matches the binary. */

        self->sprite_list_2 = *reinterpret_cast<void**>((node + 0x70));
        NETMAN_QueueMessage(msg);
    }
}
