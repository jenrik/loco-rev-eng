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
/* NetmanTypes.h (not the full Netman.h) — gets the complete Netman/
 * PlayerSlot types for named-field access (g_netman->m_gameMode, etc.)
 * without pulling in Netman.h's extern "C" Win32 block or its ~13
 * C++-linkage free-function declarations, which collide with (and are
 * superseded in practice by) this file's own correct extern "C"
 * declarations of the same names below — see NetmanTypes.h's header
 * comment for why that split exists and why the full header is unsafe here. */
#include "../network/NetmanTypes.h"
#include "Vehicle.h"
#include "../world/scriptengine.h"
#include <new>
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
int    __thiscall NETMAN_GetPlayerCount(void* netman);                 /* 0x00446890 */
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

/* Vehicle */
void*  __thiscall Vehicle_Ctor(void* obj, int resource_id, int type,
                                uint8_t flag, int unknown);   /* 0x0043D380 */
void   __thiscall Vehicle_CalcSpeed(void* obj, short param);  /* 0x0043D980 */
void   __thiscall Vehicle_InitRoute(void* obj, int resource_id, int type, uint8_t flag); /* 0x0043DC60 */
int    __thiscall VehicleEditor_GetResourceId(void* vehicle);  /* 0x0043CC20 */
void*  __thiscall VehicleEditor_GetDPlayData(void* vehicle);   /* 0x0043CD00 */
void   __thiscall VehicleEditor_SetDPlayData(void* vehicle, int data); /* 0x0043CD60 */

/* Config */
int    __thiscall Config_GetIniInt(void* config, const char* section, const char* key, int default_val); /* 0x00413FC0 */
void*  __thiscall Config_GetIniString(void* config, const char* section, const char* key,
                                       const char* default_val, char* out, int out_size); /* 0x00414030 */

/* Input helpers */
void*  __thiscall INPUT_DirToOffset_Left(void* param);   /* 0x0041CF90 */
void*  __thiscall INPUT_DirToOffset_Right(void* param);  /* 0x0041CFB0 */
void*  __thiscall INPUT_DirToOffset_Up(void* param);     /* 0x0041CFD0 */
void*  __thiscall INPUT_DirToOffset_Down(void* param);   /* 0x0041CFF0 */

/* Internal train functions referenced from ProcessMessages */
/* Train_ConnectToServer is __thiscall (ECX=subsystem, one stack arg),
 * not __fastcall — see disassembly at 0x43C860 (MOV EDX,[ESP+8] fetches
 * the single stack argument; ECX is only ever used as `this`). The stack
 * argument is a payload pointer, not an int — the old `int data` type
 * only compiled here because of -fpermissive. */
void   __thiscall Train_ConnectToServer(void* subsystem, void* payload); /* 0x43C860 */
void   __fastcall Train_HandleTrackBuild(void* subsystem, int data); /* 0x0043CE10 */
void   __fastcall Train_SendPlayerInfo(void* subsystem);             /* 0x0043CDA0 */
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
extern void*    g_netSettings;     /* 0x004FD3A8 */
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

        /* Reverse EnumConnections's list into g_netSettings+0x10. Real typed
         * pointers throughout now (DirectPlaySession::EnumConnections
         * returns a genuine DirectPlayConnectionNode* list) — the previous
         * int32_t-truncating-pointer TODO here no longer applies, since
         * nothing here round-trips a pointer through a narrower integer
         * anymore. */
        DirectPlayConnectionNode* reversed = nullptr;
        for (DirectPlayConnectionNode* item = g_dplay_peer->EnumConnections();
             item != nullptr;
             item = item->next) {
            auto* copy = static_cast<DirectPlayConnectionNode*>(operator_new(sizeof(DirectPlayConnectionNode)));
            copy->next = reversed;
            copy->type = item->type;
            reversed = copy;
        }
        *reinterpret_cast<void**>(static_cast<uint8_t*>(g_netSettings) + 0x10) = reversed;

        char index[2] = {'0', 0};
        for (int i = 0; i < 4; ++i, ++index[0]) {
            *(static_cast<uint8_t*>(g_netSettings) + 0x14 + i) =
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

                /* If scenario mode, unlink all cars from sprite_list_1 */
                if (g_netman->m_gameMode == 1) {
                    uint8_t* car = reinterpret_cast<uint8_t*>(this->sprite_list_1);
                    while (car != nullptr) {
                        NetworkMsg* qmsg = AllocateNetworkMessage();
                        if (qmsg) {
                            qmsg->data = NULL; qmsg->next = NULL;
                            qmsg->type = 0x0F;
                            qmsg->data = this->sprite_list_1;
                        }
                        *reinterpret_cast<uint8_t*>((car + 0x88)) = 0;
                        this->sprite_list_1 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70));
                        if (qmsg && qmsg->data) {
                            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(qmsg->data) + 0x70)) = NULL;
                        }
                        NETMAN_QueueMessage(qmsg);
                        car = reinterpret_cast<uint8_t*>(this->sprite_list_1);
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

                /* Reset timeout on all controller cars */
                {
                    uint8_t* car = reinterpret_cast<uint8_t*>(this->sprite_list_1);
                    while (car != nullptr) {
                        *reinterpret_cast<uint16_t*>((car + 0x74)) = 32000;
                        car = *reinterpret_cast<uint8_t**>((car + 0x70));
                    }
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

            uint8_t* car = reinterpret_cast<uint8_t*>(this->sprite_list_1);
            while (car) { *reinterpret_cast<uint16_t*>((car + 0x74)) = 32000; car = *reinterpret_cast<uint8_t**>((car + 0x70)); }

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

    void*  prev = NULL;
    void*  node = this->sprite_list_3;

    while (node != NULL) {
        uint8_t node_owner = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x7C));
        void*   next_node = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));

        if (node_owner == static_cast<uint8_t>(player_index)) {
            /* Unlink this node */
            if (prev == NULL) {
                this->sprite_list_3 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));
            } else {
                *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(prev) + 0x70)) = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));
            }

            if (player_index == self_slot_index) {
                /* Local player's slot: preserve on sprite_list_1 free list */
                *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x7C)) = static_cast<uint8_t>(self_slot_index);
                *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70)) = this->sprite_list_1;
                this->sprite_list_1 = node;
                node = this->sprite_list_3;
            } else {
                /* Remote player: destroy via vtable[0] */
                void** vt = *reinterpret_cast<void***>(node);
                (reinterpret_cast<void (__thiscall*)(void*, byte)>(vt[0]))(node, 1);
                node = this->sprite_list_3;
            }
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
        /* Send game-over notification if connected in scenario mode */
        if (g_dplay_peer->session_ready != 0 && g_netman->m_gameMode == 2) {
            uint16_t msg = 0x3FD;
            WIN32_SendNetworkData(g_dplay_peer, 0, &msg, 4, 1);
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
        /* Free sprite_list_1 (active) */
        {
            void* node = this->sprite_list_1;
            while (node != NULL) {
                this->sprite_list_1 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));
                void** vt = *reinterpret_cast<void***>(node);
                (reinterpret_cast<void (__thiscall*)(void*, byte)>(vt[0]))(node, 1);
                node = this->sprite_list_1;
            }
        }

        /* Free sprite_list_2 (dead) */
        {
            void* node = this->sprite_list_2;
            while (node != NULL) {
                this->sprite_list_2 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));
                void** vt = *reinterpret_cast<void***>(node);
                (reinterpret_cast<void (__thiscall*)(void*, byte)>(vt[0]))(node, 1);
                node = this->sprite_list_2;
            }
        }

        /* Free sprite_list_3 (persistent) */
        {
            void* node = this->sprite_list_3;
            while (node != NULL) {
                this->sprite_list_3 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));
                void** vt = *reinterpret_cast<void***>(node);
                (reinterpret_cast<void (__thiscall*)(void*, byte)>(vt[0]))(node, 1);
                node = this->sprite_list_3;
            }
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
    void*       car     = net_msg->data;
    int         dir     = net_msg->flags;

    if (g_netman->m_gameMode != 2) {
        /* Not in multiplayer scenario — destroy car */
        if (car != NULL) {
            void** vt = *reinterpret_cast<void***>(car);
            (reinterpret_cast<void (__thiscall*)(void*, byte)>(vt[0]))(car, 1);
            net_msg->data = NULL;
        }
        return;
    }

    /* Check if this is a 'remove' message */
    if (car && *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(car) + 4)) == 1) {
        if (car) {
            void** vt = *reinterpret_cast<void***>(car);
            (reinterpret_cast<void (__thiscall*)(void*, byte)>(vt[0]))(car, 1);
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
     * it is the field occupying +0xC, whatever its name says. */
    int self_slot_index = g_netman->m_mySlotIndex;
    int target_town = self_slot_index;

    if (dir < 0x5B) {
        if (dir == 0x5A)      target_town = self_slot_index + 1;
        else if (dir == 0)    target_town = self_slot_index - g_netman->m_playerRows;
    } else {
        if (dir == 0xB4)      target_town = g_netman->m_playerRows + self_slot_index;
        else if (dir == 0x10E) target_town = self_slot_index - 1;
    }

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
    /* Forward train to another player at the target */
    {
        if (this->sprite_list_2 != NULL) {
            uint8_t* tail = reinterpret_cast<uint8_t*>(this->sprite_list_2);
            while (*reinterpret_cast<uint8_t**>((tail + 0x70)) != nullptr) {
                tail = *reinterpret_cast<uint8_t**>((tail + 0x70));
            }
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
            *reinterpret_cast<void**>((tail + 0x70)) = car;
        } else {
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
            this->sprite_list_2 = car;
        }
        Train_RemoveAllTracks(this);
    }
}


/* ================================================================== */
/* TrainSubsystem::HandleConnectionSetup                               */
/* Address: 0x43B240                                                    */
/* Size: 1162 bytes                                                     */
/* ================================================================== */
void TrainSubsystem::HandleConnectionSetup(void* data)
{
    uint16_t* p = reinterpret_cast<uint16_t*>(data);

    /* Create a new Vehicle controller. 0x94 was the original x86
     * sizeof(Vehicle); use the real host size (see game/Vehicle.h). */
    void* vehicle_obj = operator_new(sizeof(Vehicle));
    void* controller = NULL;
    if (vehicle_obj) {
        controller = Vehicle_Ctor(vehicle_obj, *reinterpret_cast<int*>((p + 8)), 2, 1, 1);
    }
    if (controller == NULL) return;

    /* Set direction and town info */
    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(controller) + 0x74)) = p[2];    /* direction */
    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(controller) + 0x7A)) = p[3];    /* resource ID */
    *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(controller) + 0x78)) = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(p) + 10));  /* type */
    Vehicle_CalcSpeed(controller, *reinterpret_cast<short*>(reinterpret_cast<uint8_t*>(p) + 8));

    /* Set parent/controller reference */
    *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(controller) + 8)) = *reinterpret_cast<int32_t*>((p + 6));

    /* Process up to 3 track elements */
    if (*reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(p) + 0x14)) != 0) {
        uint32_t* track_entry = reinterpret_cast<uint32_t*>((p + 0x16));

        for (int i = 0; i < *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(p) + 0x14)); i++) {
            Vehicle_InitRoute(controller, track_entry[-5], track_entry[-4], 1);

            if (*reinterpret_cast<uint8_t*>((track_entry + -3)) != 0) {
                /* Has DPLAY data — check if it matches local player */
                uint16_t track_id = static_cast<uint16_t>(track_entry[-1]);
                int player_count = NETMAN_GetPlayerCount(g_netman);

                uint8_t* player_name = reinterpret_cast<uint8_t*>(track_entry);
                /* 2-byte stride name compare */
                int match = -1;
                for (int j = 0; j < player_count; j++) {
                    /* Compare player name from track_entry */
                    uint8_t* pn = reinterpret_cast<uint8_t*>(g_netman->m_slots[j].compact_name);
                    uint8_t* tn = player_name;
                    int k = 0;
                    while (tn[k] == pn[k] && tn[k] != 0) { k++; }
                    if (tn[k] == pn[k]) { match = j; break; }
                }

                if (match >= 0) {
                    /* Send PlayerJoin response for matching track */
                    uint16_t* resp = reinterpret_cast<uint16_t*>(operator_new(8));
                    if (resp) {
                        resp[0] = 0x3FB;
                        resp[2] = track_id;
                        resp[3] = NET_GetNextAttId();

                        /* Find player by name match */
                        PlayerSlot* target_player = NULL;
                        for (int j = 0; j < player_count; j++) {
                            uint8_t* pn = reinterpret_cast<uint8_t*>(g_netman->m_slots[j].compact_name);
                            uint8_t* tn = reinterpret_cast<uint8_t*>(track_entry) + 6; /* 2+ byte per char */
                            int k = 0;
                            while (tn[k*2] == pn[k] && tn[k*2] != 0) { k++; }
                            if (tn[k*2] == pn[k]) {
                                target_player = &g_netman->m_slots[j];
                                break;
                            }
                        }

                        if (target_player) {
                            WIN32_SendNetworkData(g_dplay_peer, target_player->dpId,
                                                  resp, 8, 1);

                            /* Create PlayerConnectionNode for attachment transfer */
                            track_id = resp[3]; /* att ID */
                            *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(controller) + 0x89)) += 1;

                            PlayerConnectionNode* node = reinterpret_cast<PlayerConnectionNode*>(operator_new(sizeof(PlayerConnectionNode)));
                            if (node) {
                                node->player_id      = target_player->dpId;
                                node->file_handle    = 0;
                                node->sub_type       = resp[3];
                                node->extra_info     = resp[3];
                                node->transfer_state = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(controller) + 0x78));
                                node->notify_id      = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(controller) + 0x7A));
                                node->sequence_num   = 0;
                                node->throttle       = 0;
                                node->next           = this->handle_list_2;
                                *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 8)) = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(controller) + 0x78));
                                *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 10)) = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(controller) + 0x7A));
                                this->handle_list_2 = node;
                            }
                        }
                        GLOBAL_free(resp);
                    }
                }

                /* Set DPLAY data for this track element */
                VehicleEditor_SetDPlayData(reinterpret_cast<void*>(static_cast<uintptr_t>(*reinterpret_cast<uint32_t*>((reinterpret_cast<uint8_t*>(controller) + 0x14 + i * 4)))),
                                            static_cast<int>(reinterpret_cast<uintptr_t>(&track_entry[-7])));
            }
            track_entry += 0x75; /* advance by 0xEA bytes / 4 = 0x75 dwords */
        }
    }

    /* Call vtable[13] on the controller's 5th field (4 = +0x10 ptr) */
    {
        void** vt = *reinterpret_cast<void***>((*reinterpret_cast<uintptr_t*>((reinterpret_cast<uint8_t*>(controller) + 0x10))));
        (reinterpret_cast<void (__thiscall*)(void*, void*)>(vt[13]))(*reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(controller) + 0x10)),
                                                     reinterpret_cast<void*>((reinterpret_cast<uint8_t*>(p) + 0xB10)));
    }

    /* If this train belongs to the local player, remove from sprite_list_1 */
    if (*reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(controller) + 0x78)) ==
        static_cast<uint32_t>(g_netman->m_mySlotIndex)) {
        void* prev = NULL;
        void* node = this->sprite_list_1;
        while (node) {
            if (*reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x7A)) ==
                *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(controller) + 0x7A))) {
                if (prev == NULL) {
                    this->sprite_list_1 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));
                } else {
                    *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(prev) + 0x70)) = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));
                }
                void** vt = *reinterpret_cast<void***>(node);
                (reinterpret_cast<void (__thiscall*)(void*, byte)>(vt[0]))(node, 1);
                break;
            }
            prev = node;
            node = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));
        }
    }

    /* Broadcast MSG_CTRL_INIT (0x3F3) to all players */
    {
        uint16_t* ctrl_init = reinterpret_cast<uint16_t*>(operator_new(10));
        if (ctrl_init) {
            ctrl_init[0] = 0x3F3;
            ctrl_init[2] = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(controller) + 0x7A));
            *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(ctrl_init) + 6)) = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(controller) + 0x78));
            *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(ctrl_init) + 7)) = static_cast<uint8_t>(g_netman->m_mySlotIndex);
            ctrl_init[4] = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(controller) + 0x74));
            WIN32_SendNetworkData(g_dplay_peer, 0, ctrl_init, 10, 1);
            GLOBAL_free(ctrl_init);
        }
    }

    /* Set controller owner and queue type-0x11 notification */
    *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(controller) + 0x7C)) = static_cast<uint8_t>(g_netman->m_mySlotIndex);
    *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(controller) + 0x8A)) = 0;

    {
        NetworkMsg* notify = AllocateNetworkMessage();
        if (notify) {
            notify->data = NULL; notify->next = NULL;
            notify->type = 0x11;
            notify->data = controller;
        }
        *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(controller) + 0x88)) = 0;
        NETMAN_QueueMessage(notify);
    }
}


/* ================================================================== */
/* TrainSubsystem::HandleControllerInit                                */
/* Address: 0x43B6D0                                                    */
/* ================================================================== */
void TrainSubsystem::HandleControllerInit(void* data, int dplay_id)
{
    uint16_t* p = reinterpret_cast<uint16_t*>(data);
    uint16_t  train_id = p[2];
    uint8_t   color    = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(p) + 6));
    uint8_t   owner    = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(p) + 7));
    uint16_t  dir      = p[4];

    /* Find matching car in sprite_list_1 */
    {
        uint8_t* car = reinterpret_cast<uint8_t*>(this->sprite_list_1);
        while (car != nullptr) {
            if (*reinterpret_cast<uint16_t*>((car + 0x7A)) == train_id &&
                *reinterpret_cast<uint8_t*>((car + 0x78)) == color) {
                *reinterpret_cast<int32_t*>((car + 0x8C)) = dplay_id;
                *reinterpret_cast<uint8_t*>((car + 0x7C)) = owner;
                break;
            }
            car = *reinterpret_cast<uint8_t**>((car + 0x70));
        }
    }

    /* Broadcast type-0x12 notification */
    NetworkMsg* msg = AllocateNetworkMessage();
    if (msg) {
        msg->data = NULL; msg->next = NULL;
        msg->type = 0x12;
        msg->data = NULL;
        msg->flags = train_id;
        msg->setMetadata0(color);
        msg->setMetadata1(owner);
        msg->size = dir;
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

    /* Walk sprite_list_1 and remove matching cars */
    {
        void* prev = NULL;
        void* node = this->sprite_list_1;

        while (node != NULL) {
            uint8_t owner_byte = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x7C));
            uint8_t color_byte = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x78));
            int     dplay_id   = *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(node) + 0x8C));

            int match = 0;
            if (player_id != 0 && owner_byte == static_cast<uint8_t>(player_index)) {
                match = 1;
            }
            if (player_id == 0 && color_byte == static_cast<uint8_t>(player_index)) {
                match = 1;
            }
            if (dplay_id == player_id) {
                match = 1;
            }

            if (match) {
                void* next_node = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));

                /* Unlink from sprite_list_1 */
                if (prev == NULL) {
                    this->sprite_list_1 = next_node;
                } else {
                    *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(prev) + 0x70)) = next_node;
                }

                /* Reverse direction */
                uint16_t dir = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x74));
                if (dir < 0x5B) {
                    if (dir == 0x5A)      dir = 0x10E;
                    else if (dir == 0)    dir = 0xB4;
                } else {
                    if (dir == 0xB4)      dir = 0;
                    else if (dir == 0x10E) dir = 0x5A;
                }
                *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x74)) = dir;

                /* Set owner to local player */
                *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x7C)) =
                    static_cast<uint8_t>(g_netman->m_mySlotIndex);

                /* Clear DPlay data on all carriages */
                if (*reinterpret_cast<short*>(reinterpret_cast<uint8_t*>(node) + 0x0C) != 0) {
                    void** carriage = reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x14));
                    for (int j = 0; j < *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x0C)); j++) {
                        VehicleEditor_SetDPlayData(carriage[j], 0);
                    }
                }

                /* Send type-0x11 release message */
                NetworkMsg* msg = AllocateNetworkMessage();
                if (msg) {
                    msg->data = NULL; msg->next = NULL;
                    msg->type = 0x11;
                    msg->data = node;
                }
                *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x88)) = 0;
                NETMAN_QueueMessage(msg);

                node = this->sprite_list_1;
            } else {
                prev = node;
                node = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));
            }
        }
    }
}


/* ================================================================== */
/* TrainSubsystem::AddTrainCar                                         */
/* Address: 0x43B8C0                                                    */
/* ================================================================== */
void TrainSubsystem::AddTrainCar(void* car, int direction, int player_index)
{
    /* Check if player exists at target index */
    PlayerSlot* player_slot = NULL;
    if (player_index >= 0) {
        player_slot = &g_netman->m_slots[player_index];
    }

    if (player_slot && player_slot->pixel_buffer != nullptr) {
        /* === Multiplayer path: prepend to sprite_list_3, broadcast 0x3F3 === */

        *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(car) + 0x7C)) = static_cast<uint8_t>(player_index);
        *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = this->sprite_list_3;
        this->sprite_list_3 = car;

        /* Set movement direction in +0x76 */
        if (direction < 0x5B) {
            if (direction == 0x5A)      *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x76)) = 0x5A;
            else if (direction == 0)    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x76)) = 0;
        } else {
            if (direction == 0xB4)      *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x76)) = 0xB4;
            else if (direction == 0x10E) *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x76)) = 0x10E;
        }

        /* Broadcast MSG_CTRL_INIT (0x3F3) */
        uint16_t* ctrl_init = reinterpret_cast<uint16_t*>(operator_new(10));
        if (ctrl_init) {
            ctrl_init[0] = 0x3F3;
            ctrl_init[2] = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x7A));
            *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(ctrl_init) + 6)) = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(car) + 0x78));
            *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(ctrl_init) + 7)) = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(car) + 0x7C));
            ctrl_init[4] = static_cast<uint16_t>(direction);
            WIN32_SendNetworkData(g_dplay_peer, 0, ctrl_init, 10, 1);

            /* Also update local handler */
            this->HandleControllerInit(ctrl_init, g_netman->m_myDpId);

            GLOBAL_free(ctrl_init);
        }

        /* Compute tile offset from direction */
        int dx = 0, dy = 0;
        uint16_t move_dir = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x76));

        if (move_dir == 0x5A) { dx = 1; dy = 0; }
        else if (move_dir == 0) { dx = 0; dy = -1; }
        else if (move_dir == 0xB4) { dx = 0; dy = 1; }
        else if (move_dir == 0x10E) { dx = -1; dy = 0; }

        *reinterpret_cast<int16_t*>((reinterpret_cast<uint8_t*>(car) + 0x7E)) = static_cast<int16_t>((dx + 1));
        *reinterpret_cast<int16_t*>((reinterpret_cast<uint8_t*>(car) + 0x80)) = static_cast<int16_t>((dy + (move_dir == 0x5A ? 1 : move_dir == 0 ? -1 : move_dir == 0xB4 ? 1 : 1)));
        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x84)) = 0xFFFF;
        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x86)) = 0xFFFF;
        *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(car) + 0x82)) = 0;
        return;
    }

    /* === Single-player path: reverse direction, append to sprite_list_2 === */

    /* Mirror direction */
    if (direction < 0x5B) {
        if (direction == 0x5A)      direction = 0x10E;
        else if (direction == 0)    direction = 0xB4;
    } else {
        if (direction == 0xB4)      direction = 0;
        else if (direction == 0x10E) direction = 0x5A;
    }
    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x74)) = static_cast<uint16_t>(direction);

    /* Append to sprite_list_2 */
    if (this->sprite_list_2 == NULL) {
        *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
        this->sprite_list_2 = car;
    } else {
        uint8_t* tail = reinterpret_cast<uint8_t*>(this->sprite_list_2);
        while (*reinterpret_cast<uint8_t**>((tail + 0x70)) != nullptr) {
            tail = *reinterpret_cast<uint8_t**>((tail + 0x70));
        }
        *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
        *reinterpret_cast<void**>((tail + 0x70)) = car;
    }
    Train_RemoveAllTracks(this);
}


/* ================================================================== */
/* TrainSubsystem::UpdateTrainMovement                                 */
/* Address: 0x43BB00                                                    */
/* Size: 1622 bytes                                                     */
/* ================================================================== */
void TrainSubsystem::UpdateTrainMovement()
{
    void*  prev = NULL;
    void*  node = this->sprite_list_3;

    while (node != NULL) {
        void* next_node = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70));

        /* Check if player at node's town is still connected */
        uint8_t owner = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x7C));
        PlayerSlot* player_slot = &g_netman->m_slots[owner];

        if (player_slot->pixel_buffer == nullptr || player_slot->dpId != 0) {
            /* Owner disconnected — move from sprite_list_3 to sprite_list_2 */
            if (prev == NULL) {
                this->sprite_list_3 = next_node;
            } else {
                *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(prev) + 0x70)) = next_node;
            }
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(node) + 0x70)) = NULL;

            /* Mirror direction */
            uint16_t dir = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x74));
            if (dir < 0x5B) {
                if (dir == 0x5A)      dir = 0x10E;
                else if (dir == 0)    dir = 0xB4;
            } else {
                if (dir == 0xB4)      dir = 0;
                else if (dir == 0x10E) dir = 0x5A;
            }
            *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x74)) = dir;

            /* Append to sprite_list_2 */
            if (this->sprite_list_2 == NULL) {
                this->sprite_list_2 = node;
            } else {
                uint8_t* tail = reinterpret_cast<uint8_t*>(this->sprite_list_2);
                while (*reinterpret_cast<uint8_t**>((tail + 0x70)) != nullptr) tail = *reinterpret_cast<uint8_t**>((tail + 0x70));
                *reinterpret_cast<void**>((tail + 0x70)) = node;
            }

            /* Re-notify all cars in sprite_list_2 */
            uint8_t* car = reinterpret_cast<uint8_t*>(this->sprite_list_2);
            while (car != nullptr) {
                NetworkMsg* msg = AllocateNetworkMessage();
                if (msg) { msg->data = NULL; msg->next = NULL;
                           msg->type = 0x11;
                           msg->data = this->sprite_list_2; }
                (reinterpret_cast<Building*>(this->sprite_list_2))->occupation_level = 0;

                uint8_t* cur = reinterpret_cast<uint8_t*>(this->sprite_list_2);
                *reinterpret_cast<uint8_t*>((cur + 0x7C)) = static_cast<uint8_t>(g_netman->m_mySlotIndex);
                *reinterpret_cast<void**>((cur + 0x70)) = NULL;
                *reinterpret_cast<uint8_t*>((cur + 0x88)) = 0;
                this->sprite_list_2 = *reinterpret_cast<void**>((cur + 0x70));

                NETMAN_QueueMessage(msg);
                car = reinterpret_cast<uint8_t*>(this->sprite_list_2);
            }

            node = this->sprite_list_3;
            continue;
        }

        /* === Check map edge routing === */
        int pos_x = *reinterpret_cast<int16_t*>((reinterpret_cast<uint8_t*>(node) + 0x7E));
        int pos_y = *reinterpret_cast<int16_t*>((reinterpret_cast<uint8_t*>(node) + 0x80));
        int map_width  = static_cast<int>(static_cast<int16_t>(player_slot->pixel_width));
        int map_height = static_cast<int>(static_cast<int16_t>(player_slot->pixel_height));

        uint8_t routed = this->RouteTrainAtEdge(
            prev, node, pos_x, pos_y, map_width, map_height);

        if (routed) {
            prev = node;
            node = next_node;
            continue;
        }

        /* === Movement-steering: advance one tile === */
        uint16_t move_dir = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76));
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
                        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0xB4;
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
                    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0x5A;
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
                    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0x10E;
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
                        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0;
                    } else { new_y = pos_y; }
                }
            }
        }

        /* === Stuck detection === */
        if (new_x == pos_x && new_y == pos_y) {
            uint32_t rand_val = CRT_rand();
            int r = static_cast<int>((rand_val / 0x1FFF));
            switch (r % 4) {
            case 0: *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0x5A;  break;
            case 1: *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0x10E; break;
            case 2: *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0;     break;
            case 3: *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0xB4;  break;
            }
            *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x84)) = 0xFFFF;
            *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x86)) = 0xFFFF;
            *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x82)) = 0;
        } else {
            /* Update position */
            *reinterpret_cast<int16_t*>((reinterpret_cast<uint8_t*>(node) + 0x7E)) = static_cast<int16_t>(new_x);
            *reinterpret_cast<int16_t*>((reinterpret_cast<uint8_t*>(node) + 0x80)) = static_cast<int16_t>(new_y);
            *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x82)) += 1;

            /* Check stuck counter loop */
            uint32_t rand_val = CRT_rand();
            int threshold = static_cast<int>((rand_val / 0x1999)) + 3;
            if (threshold < static_cast<int>(*reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x82)))) {
                if (*reinterpret_cast<int16_t*>((reinterpret_cast<uint8_t*>(node) + 0x7E)) == *reinterpret_cast<int16_t*>((reinterpret_cast<uint8_t*>(node) + 0x84)) &&
                    *reinterpret_cast<int16_t*>((reinterpret_cast<uint8_t*>(node) + 0x80)) == *reinterpret_cast<int16_t*>((reinterpret_cast<uint8_t*>(node) + 0x86))) {
                    uint32_t rv = CRT_rand();
                    int rr = static_cast<int>((rv / 0x1FFF));
                    switch (rr % 4) {
                    case 0: *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0x5A;  break;
                    case 1: *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0x10E; break;
                    case 2: *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0;     break;
                    case 3: *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x76)) = 0xB4;  break;
                    }
                    *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x82)) = 0;
                    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x84)) = 0xFFFF;
                    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x86)) = 0xFFFF;
                }
                *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x82)) = 0;
                *reinterpret_cast<uint32_t*>((reinterpret_cast<uint8_t*>(node) + 0x84)) = *reinterpret_cast<uint32_t*>((reinterpret_cast<uint8_t*>(node) + 0x7E));
            }

            /* === Build and send type-0x3F6 position update message === */
            {
                /* Allocate message buffer. Fixed-size raw network message
                 * buffer (explicit byte offsets below), not a C++ object —
                 * safe as-is on any host. */
                uint16_t* buf = reinterpret_cast<uint16_t*>(operator_new(0x2000));
                if (buf) {
                    buf[0] = 0x3F6;
                    *reinterpret_cast<uint8_t*>((buf + 1)) = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x7C));
                    *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(buf) + 4)) = 0;
                    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(buf) + 6)) = 1;
                    uint8_t c1 = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x78));
                    uint8_t c2 = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(node) + 0x7C));
                    *reinterpret_cast<uint32_t*>((reinterpret_cast<uint8_t*>(buf) + 9)) =
                        (static_cast<uint32_t>(new_x) << 16) | *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(node) + 0x7A));
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
uint32_t TrainSubsystem::RouteTrainAtEdge(void* prev_node, void* train,
                                           int pos_x, int pos_y,
                                           int map_width, int map_height)
{
    uint8_t owner = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(train) + 0x7C));

    /* RIGHT edge (pos_x >= map_width - 1) */
    if (pos_x >= map_width - 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0xB4, owner);
        if (static_cast<char>(conn) != 0) {
            /* Track connection to neighbor exists — route the train */
            if (prev_node == NULL) {
                this->sprite_list_3 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70));
            } else {
                *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(prev_node) + 0x70)) = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70));
            }
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70)) = NULL;

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
                return this->MoveToNeighborTown(player_slot->dpId, train, 0xB4);
            }
            this->AddTrainCar(train, 0xB4, target_town);
            return 1;
        }
        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(train) + 0x76)) = 0; /* Bounce left */
    }
    /* LEFT edge (pos_x <= 0) */
    else if (pos_x < 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0, owner);
        if (static_cast<char>(conn) != 0) {
            if (prev_node == NULL) {
                this->sprite_list_3 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70));
            } else {
                *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(prev_node) + 0x70)) = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70));
            }
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70)) = NULL;

            int target_town = static_cast<int>(owner) - g_netman->m_playerRows;
            PlayerSlot* player_slot = NULL;
            if (target_town >= 0) {
                player_slot = &g_netman->m_slots[target_town];
            }
            if (player_slot && player_slot->dpId != 0) {
                return this->MoveToNeighborTown(player_slot->dpId, train, 0);
            }
            this->AddTrainCar(train, 0, target_town);
            return 1;
        }
        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(train) + 0x76)) = 0xB4; /* Bounce right */
    }
    /* TOP edge (pos_y <= 0) */
    else if (pos_y < 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0x10E, owner);
        if (static_cast<char>(conn) != 0) {
            if (prev_node == NULL) {
                this->sprite_list_3 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70));
            } else {
                *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(prev_node) + 0x70)) = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70));
            }
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70)) = NULL;

            int target_town = static_cast<int>(owner) - 1;
            PlayerSlot* player_slot = NULL;
            if (target_town >= 0) {
                player_slot = &g_netman->m_slots[target_town];
            }
            if (player_slot && player_slot->dpId != 0) {
                return this->MoveToNeighborTown(player_slot->dpId, train, 0x10E);
            }
            this->AddTrainCar(train, 0x10E, target_town);
            return 1;
        }
        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(train) + 0x76)) = 0x5A; /* Bounce down */
    }
    /* BOTTOM edge (pos_y >= map_height - 1) */
    else if (pos_y >= map_height - 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0x5A, owner);
        if (static_cast<char>(conn) != 0) {
            if (prev_node == NULL) {
                this->sprite_list_3 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70));
            } else {
                *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(prev_node) + 0x70)) = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70));
            }
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(train) + 0x70)) = NULL;

            PlayerSlot* player_ptr = &g_netman->m_slots[owner + 1];
            if (player_ptr->dpId != 0) {
                return this->MoveToNeighborTown(player_ptr->dpId, train, 0x5A);
            }
            this->AddTrainCar(train, 0x5A, owner + 1);
            return 1;
        }
        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(train) + 0x76)) = 0x10E; /* Bounce up */
    } else {
        /* Not at any edge — continue normal movement */
        return 0;
    }

    /* Bounce (no track connection): clear stuck counter */
    *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(train) + 0x82)) = 0;
    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(train) + 0x84)) = 0xFFFF;
    *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(train) + 0x86)) = 0xFFFF;
    return 0xFFFFFF01u;
}


/* ================================================================== */
/* TrainSubsystem::MoveToNeighborTown                                  */
/* Address: 0x43AE20                                                    */
/* Size: 1016 bytes                                                     */
/* ================================================================== */
uint32_t TrainSubsystem::MoveToNeighborTown(int to_player, void* car, int direction)
{
    int self_slot_index = g_netman->m_mySlotIndex;
    int target_idx = NETMAN_FindPlayerIndex(g_netman, to_player);

    if (self_slot_index == target_idx) {
        /* === Local player: mirror direction, append to sprite_list_2 === */

        /* Mirror direction */
        if (direction < 0x5B) {
            if (direction == 0x5A)      direction = 0x10E;
            else if (direction == 0)    direction = 0xB4;
        } else {
            if (direction == 0xB4)      direction = 0;
            else if (direction == 0x10E) direction = 0x5A;
        }
        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x74)) = static_cast<uint16_t>(direction);

        /* Append to sprite_list_2 */
        if (this->sprite_list_2 == NULL) {
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
            this->sprite_list_2 = car;
        } else {
            uint8_t* tail = reinterpret_cast<uint8_t*>(this->sprite_list_2);
            while (*reinterpret_cast<uint8_t**>((tail + 0x70)) != nullptr) tail = *reinterpret_cast<uint8_t**>((tail + 0x70));
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
            *reinterpret_cast<void**>((tail + 0x70)) = car;
        }
        Train_RemoveAllTracks(this);
        return 1;
    }

    /* === Remote player: serialize into 0xB1C-byte MSG_CONN_SETUP ===
     * Fixed-size raw network message buffer (memset + explicit byte
     * offsets below), not a C++ object — safe as-is on any host. */
    uint8_t* buf = reinterpret_cast<uint8_t*>(operator_new(0xB1C));
    if (buf == NULL) return 0;

    memset(buf, 0, 0xB1C);

    *reinterpret_cast<uint16_t*>(buf) = 0x3F2;                        /* message type */
    *reinterpret_cast<uint16_t*>((buf + 4)) = static_cast<uint16_t>(direction);     /* direction */
    *reinterpret_cast<uint16_t*>((buf + 6)) = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x7A));  /* resource ID */
    *reinterpret_cast<uint8_t*>((buf + 10)) = *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(car) + 0x78));   /* type */
    *reinterpret_cast<uint16_t*>((buf + 8)) = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x58));  /* speed parameter */

    /* Copy parent data */
    *reinterpret_cast<int32_t*>((buf + 0x0C)) = *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(car) + 8));

    /* Copy player name (up to 10 bytes from +0x7C) */
    {
        const char* name_src = reinterpret_cast<const char*>((reinterpret_cast<uint8_t*>(car) + 0x7C));
        char* name_dst = reinterpret_cast<char*>((buf + 0xB10));
        for (int i = 0; i < 10 && name_src[i] != 0; i++) {
            name_dst[i] = name_src[i];
        }
    }

    /* Process carriages */
    uint8_t carriage_count = 0;
    if (*reinterpret_cast<short*>(reinterpret_cast<uint8_t*>(car) + 0x0C) != 0) {
        int* carriage_ptr = reinterpret_cast<int*>((reinterpret_cast<uint8_t*>(car) + 0x14));
        for (int i = 0; i < *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x0C)); i++) {
            if (carriage_ptr[i] == 0) continue;

            int res_id = VehicleEditor_GetResourceId(reinterpret_cast<void*>(static_cast<uintptr_t>(carriage_ptr[i])));
            *reinterpret_cast<int32_t*>((buf + 6 + carriage_count * 0xEA)) = res_id;
            *reinterpret_cast<int32_t*>((buf + 6 + carriage_count * 0xEA + 4)) =
                *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(carriage_ptr[i])) + 0x42C));

            void* dplay_data = VehicleEditor_GetDPlayData(reinterpret_cast<void*>(static_cast<uintptr_t>(carriage_ptr[i])));
            if (dplay_data) {
                *reinterpret_cast<uint8_t*>((buf + 6 + carriage_count * 0xEA + 8)) = 1;
                memcpy(buf + 6 + carriage_count * 0xEA + 9, dplay_data, 0x39C);
                /* 0x39C = 0xE7 * 4 bytes */
            } else {
                *reinterpret_cast<uint8_t*>((buf + 6 + carriage_count * 0xEA + 8)) = 0;
            }
            carriage_count++;
        }
    }
    *reinterpret_cast<uint8_t*>((buf + 0x14)) = carriage_count;

    /* Send the message */
    int send_result = WIN32_SendNetworkData(g_dplay_peer, to_player, buf, 0xB1C, 1);

    if (send_result == 0) {
        /* Send failed — take local control instead */
        GLOBAL_free(buf);

        /* Mirror direction */
        uint16_t dir = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x74));
        if (dir < 0x5B) {
            if (dir == 0x5A)      dir = 0x10E;
            else if (dir == 0)    dir = 0xB4;
        } else {
            if (dir == 0xB4)      dir = 0;
            else if (dir == 0x10E) dir = 0x5A;
        }
        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x74)) = dir;

        /* Append to sprite_list_2 */
        if (this->sprite_list_2 == NULL) {
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
            this->sprite_list_2 = car;
        } else {
            uint8_t* tail = reinterpret_cast<uint8_t*>(this->sprite_list_2);
            while (*reinterpret_cast<uint8_t**>((tail + 0x70)) != nullptr) tail = *reinterpret_cast<uint8_t**>((tail + 0x70));
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
            *reinterpret_cast<void**>((tail + 0x70)) = car;
        }

        /* Notify UI for all cars in sprite_list_2 */
        {
            uint8_t* c = reinterpret_cast<uint8_t*>(this->sprite_list_2);
            while (c != nullptr) {
                NetworkMsg* msg = AllocateNetworkMessage();
                if (msg) { msg->data = NULL; msg->next = NULL;
                           msg->type = 0x11;
                           msg->data = this->sprite_list_2; }
                (reinterpret_cast<Building*>(this->sprite_list_2))->occupation_level = 0;

                uint8_t* cur = reinterpret_cast<uint8_t*>(this->sprite_list_2);
                *reinterpret_cast<uint8_t*>((cur + 0x7C)) = static_cast<uint8_t>(g_netman->m_mySlotIndex);
                *reinterpret_cast<void**>((cur + 0x70)) = NULL;
                *reinterpret_cast<uint8_t*>((cur + 0x88)) = 0;
                this->sprite_list_2 = *reinterpret_cast<void**>((cur + 0x70));

                NETMAN_QueueMessage(msg);
                c = reinterpret_cast<uint8_t*>(this->sprite_list_2);
            }
        }
    } else {
        /* Send succeeded — car transferred to remote player */

        /* Mirror direction for local tracking */
        {
            uint16_t dir = *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x74));
            if (dir < 0x5B) {
                if (dir == 0x5A)      dir = 0x10E;
                else if (dir == 0)    dir = 0xB4;
            } else {
                if (dir == 0xB4)      dir = 0;
                else if (dir == 0x10E) dir = 0x5A;
            }
            *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x74)) = dir;
        }

        /* Set owner to target player */
        int target_idx2 = NETMAN_FindPlayerIndex(g_netman, to_player);
        *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(car) + 0x7C)) = static_cast<uint8_t>(target_idx2);
        *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(car) + 0x8A)) = 0;

        /* Append to sprite_list_2 with owner tracking */
        if (this->sprite_list_2 == NULL) {
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
            this->sprite_list_2 = car;
        } else {
            uint8_t* tail = reinterpret_cast<uint8_t*>(this->sprite_list_2);
            while (*reinterpret_cast<uint8_t**>((tail + 0x70)) != nullptr) tail = *reinterpret_cast<uint8_t**>((tail + 0x70));
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
            *reinterpret_cast<void**>((tail + 0x70)) = car;
        }

        GLOBAL_free(buf);
    }

    return 1;
}


/* ================================================================== */
/* TrainSubsystem::HandleJoinMultiplayer                               */
/* Address: 0x43C410                                                    */
/* Size: 1089 bytes                                                     */
/* ================================================================== */
void TrainSubsystem::HandleJoinMultiplayer(void* msg)
{
    NetworkMsg* net_msg = reinterpret_cast<NetworkMsg*>(msg);
    void*       car     = net_msg->data;

    if (car == NULL) return;

    if (g_netman->m_gameMode == 1) {
        /* === Scenario mode: append car to sprite_list_1 === */
        *reinterpret_cast<uint16_t*>((reinterpret_cast<uint8_t*>(car) + 0x74)) = 32000; /* max timeout */

        if (*reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(car) + 4)) == 1) {
            /* Remove message: destroy car */
            void** vt = *reinterpret_cast<void***>(car);
            (reinterpret_cast<void (__thiscall*)(void*, byte)>(vt[0]))(car, 1);
            net_msg->data = NULL;
            return;
        }

        /* Append to end of sprite_list_1 */
        if (this->sprite_list_1 == NULL) {
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
            this->sprite_list_1 = car;
        } else {
            uint8_t* tail = reinterpret_cast<uint8_t*>(this->sprite_list_1);
            while (*reinterpret_cast<uint8_t**>((tail + 0x70)) != nullptr) tail = *reinterpret_cast<uint8_t**>((tail + 0x70));
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(car) + 0x70)) = NULL;
            *reinterpret_cast<void**>((tail + 0x70)) = car;
        }

        if (g_demo_mode == 1) {
            /* Demo mode: remove all existing cars from sprite_list_1 */
            uint8_t* c = reinterpret_cast<uint8_t*>(this->sprite_list_1);
            while (c != nullptr) {
                NetworkMsg* qmsg = AllocateNetworkMessage();
                if (qmsg) { qmsg->data = NULL; qmsg->next = NULL;
                           qmsg->type = 0x0F;
                           qmsg->data = this->sprite_list_1; }
                *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(this->sprite_list_1) + 0x88)) = 0;
                this->sprite_list_1 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(this->sprite_list_1) + 0x70));
                if (qmsg && qmsg->data) {
                    *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(qmsg->data) + 0x70)) = NULL;
                }
                NETMAN_QueueMessage(qmsg);
                c = reinterpret_cast<uint8_t*>(this->sprite_list_1);
            }
        }
        return;
    }

    /* === Free-play mode === */
    if (car && *reinterpret_cast<int32_t*>((reinterpret_cast<uint8_t*>(car) + 4)) == 1) {
        void** vt = *reinterpret_cast<void***>(car);
        (reinterpret_cast<void (__thiscall*)(void*, byte)>(vt[0]))(car, 1);
        net_msg->data = NULL;
        return;
    }

    if (g_demo_mode == 1 || this->byte_flags != 0) {
        /* Demo mode or flag set — remove all cars */
        uint8_t* c = reinterpret_cast<uint8_t*>(this->sprite_list_1);
        while (c != nullptr) {
            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) { qmsg->data = NULL; qmsg->next = NULL;
                       qmsg->type = 0x0F;
                       qmsg->data = this->sprite_list_1; }
            *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(this->sprite_list_1) + 0x88)) = 0;
            this->sprite_list_1 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(this->sprite_list_1) + 0x70));
            if (qmsg && qmsg->data) {
                *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(qmsg->data) + 0x70)) = NULL;
            }
            NETMAN_QueueMessage(qmsg);
            c = reinterpret_cast<uint8_t*>(this->sprite_list_1);
        }
        return;
    }

    /* Create DirectPlay session and connect to train server */
    if (g_dplay_peer == NULL) {
        /* Sized to the real class — see TrainSubsystem::InitNetwork's own
         * comment on why sizeof(DirectPlaySession) replaces the original
         * x86 struct's hardcoded 0x160c bytes. */
        auto* new_peer = static_cast<DirectPlaySession*>(operator_new(sizeof(DirectPlaySession)));
        if (new_peer != NULL) {
            new_peer->CreatePeer(this->context_id_a, 0);
        }
        g_dplay_peer = new_peer;

        if (g_dplay_peer) {
            /* See InitNetwork's identical override — deliberate caller-side
             * policy, not a bug. */
            g_dplay_peer->error_callback = nullptr;
            g_dplay_peer->show_dialogs = 0;
            g_dplay_peer->hwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(this->context_id_b));
        }
    }

    if (g_dplay_peer && g_dplay_peer->session_ready != 0) {
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
            Train_SendPlayerInfo(this);
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
        Train_SendPlayerInfo(this);
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
            player->m_flag41 = 0xFF;

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

    /* Remove all existing cars from sprite_list_1 to join fresh */
    {
        uint8_t* c = reinterpret_cast<uint8_t*>(this->sprite_list_1);
        while (c != nullptr) {
            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) { qmsg->data = NULL; qmsg->next = NULL;
                       qmsg->type = 0x0F;
                       qmsg->data = this->sprite_list_1; }
            *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(this->sprite_list_1) + 0x88)) = 0;
            this->sprite_list_1 = *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(this->sprite_list_1) + 0x70));
            if (qmsg && qmsg->data) {
                *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(qmsg->data) + 0x70)) = NULL;
            }
            NETMAN_QueueMessage(qmsg);
            c = reinterpret_cast<uint8_t*>(this->sprite_list_1);
        }
    }
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

        /* 0x43CC0B writes through msg unconditionally, matching the binary. */
        msg->type = 0x0F;
        msg->data = this->sprite_list_1;
        *reinterpret_cast<uint8_t*>((reinterpret_cast<uint8_t*>(this->sprite_list_1) + 0x88)) = 0;
        this->sprite_list_1 =
            *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(this->sprite_list_1) + 0x70));
        *reinterpret_cast<void**>((reinterpret_cast<uint8_t*>(msg->data) + 0x70)) = NULL;
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
        player->m_flag41 = 0xFF;
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
