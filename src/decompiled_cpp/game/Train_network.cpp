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

#include "Train.h"
#include "../network/TrainMessage.h"
#include "../network/DPlayManager.h"
#include "Vehicle.h"
#include "../world/scriptengine.h"
#include <new>
#ifndef _WIN32
#include "../../sdl3_shims/sdl3_net_runtime.h"
#include "../../sdl3_shims/host_test_events.h"
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

extern "C" {

/* CRT pattern helpers */
void   __cdecl CRT_memset_pattern(void* dst, int pattern, int count, void* callback); /* 0x004660D0 */
void   __cdecl CRT_free_pattern(void* ptr, int pattern, int count, void* cleanup);   /* 0x004660F0 */
char*  __cdecl CRT_itoa(int value, char* str, int radix);  /* CRT */

/* DirectPlay */
void   DirectPlay_Close(void* peer);          /* 0x00461990 */
int    DirectPlay_HostSession(void* peer, int enable, int max_players, int a, int b); /* 0x0045EDE0 */
int    DirectPlay_ConnectToSession(void* peer, char* player_name, char* session_name, char* pwd); /* 0x0045F050 */
int    DirectPlay_SetSessionDesc(void* peer, char* desc); /* 0x0045FB70 */
void   DirectPlay_HandleMessages(void);       /* 0x45F390 */

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

/* DirectPlay message polling */
void*  __thiscall WIN32_PeekMessageLoop(void* dplay_peer);  /* 0x00460F10 */

/* Network manager */
int    __thiscall NETMAN_FindPlayerIndex(void* netman, int player_id); /* 0x00446760 */
int    __thiscall NETMAN_CheckTrackConnection(void* netman, int direction, uint8_t player_index); /* 0x00446830 */
int    __thiscall NETMAN_GetPlayerCount(void* netman);                 /* 0x00446890 */
void   __thiscall NETMAN_SendBuildingData(void* netman, int player_id); /* 0x00446510 */

/* DPLAY player helpers */
void*  __thiscall DPLAY_CreatePlayer(void* player);          /* 0x004429C0 */
void DPLAY_CopyPlayerData(void* dst, const void* src); /* 0x4426D0 */
#ifndef _WIN32
void* DPLAY_DecodePlayerSlots(const void* firstCompactSlot);
#endif
void   __thiscall DPLAY_CleanupPlayer(void* player);         /* 0x00442A00 */
void   __thiscall DPLAY_InitPlayer(void* player, uint8_t mode, uint8_t sub_mode,
                                    int a, int b, int c, int d, int e); /* 0x00442A40 */

/* NET helpers */
void   __thiscall NET_GetAttFilePath(uint32_t type, int mode, char* buffer);  /* 0x00445B30 */
void   __thiscall NET_GetFilePath(uint32_t type, int mode, char* buffer);      /* 0x004459A0 */
int    __thiscall NET_FindPlayer(int mode, uint32_t player_id);                 /* 0x004461D0 */
void   __thiscall NET_RegisterPlayer(void* dplay, void* player, int flag, int unknown); /* 0x00446260 */
int    __thiscall NET_GetNextAttId(void);                                        /* 0x00445E70 */

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
void   __fastcall Train_ConnectToServer(void* subsystem, int data);  /* 0x0043CDD0 */
void   __fastcall Train_HandleTrackBuild(void* subsystem, int data); /* 0x0043CE10 */
void   __fastcall Train_SendPlayerInfo(void* subsystem);             /* 0x0043CDA0 */
void   __fastcall Train_RemoveAllTracks(void* subsystem);            /* 0x0043CA50 */

/* OutputDebugStringA is available as g_OutputDebugStringA from main file */

/* DirectPlay helpers */
void   __thiscall DirectPlay_DestroyPeer(void* peer);  /* 0x461A00 */
void*  __thiscall DirectPlay_CreatePeer(void* peer, int ctx_a, int ctx_b); /* 0x45E490 */
void*  __thiscall DirectPlay_EnumConnections(void* peer);                 /* 0x45EAB0 */
int    __cdecl DirectPlay_QueryConnection(const char* index);             /* 0x45EE60 */

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
extern void*    g_dplay_peer;      /* 0x0048525C */
extern void*    g_train;           /* 0x004FD3A4 */
extern void*    g_network_thread;  /* 0x004FD398 */
extern void*    g_netman;          /* 0x004FD3AC */
extern void*    g_netSettings;     /* 0x004FD3A8 */
extern void*    g_main_window;     /* 0x004AA4A0 */
extern void*    g_resmgr;          /* 0x004855E8 */
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

        void* reversed = NULL;
        for (uint32_t* item = (uint32_t*)DirectPlay_EnumConnections(g_dplay_peer);
             item != NULL; item = (uint32_t*)(uintptr_t)item[0]) {
            uint32_t* copy = (uint32_t*)operator_new(8);
            copy[0] = (uint32_t)reversed;
            copy[1] = item[1];
            reversed = copy;
        }
        *(void**)((uint8_t*)g_netSettings + 0x10) = reversed;

        char index[2] = {'0', 0};
        for (int i = 0; i < 4; ++i, ++index[0]) {
            *(uint8_t*)((uint8_t*)g_netSettings + 0x14 + i) =
                (uint8_t)DirectPlay_QueryConnection(index);
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
    if (g_network_thread != NULL &&
        (intptr_t)WIN32_GetThreadResult(g_network_thread) != 0) {
        this->FlushMessages();
    }

    if (g_dplay_peer != NULL) {
        void* peer = g_dplay_peer;
        DirectPlay_DestroyPeer(peer);
        GLOBAL_free(peer);
        g_dplay_peer = NULL;
    }

    void** lists[] = {&sprite_list_1, &sprite_list_2, &sprite_list_3};
    for (unsigned i = 0; i < 3; ++i) {
        while (*lists[i] != NULL) {
            void* node = *lists[i];
            *lists[i] = *(void**)((uint8_t*)node + 0x70);
            void** vtable = *(void***)node;
            ((void (__thiscall*)(void*, byte))vtable[0])(node, 1);
        }
    }

    static_cast<ScriptEngine*>(g_train_resources)->ScriptEngine::Lock();
    while (g_network_queue != NULL) {
        NetworkMsg* msg = (NetworkMsg*)g_network_queue;
        g_network_queue = msg->next;
        if (msg->data != NULL) {
            if (msg->type == 0x0E) {
                void** vtable = *(void***)msg->data;
                ((void (__thiscall*)(void*, byte))vtable[0])(msg->data, 1);
            } else {
                GLOBAL_free(msg->data);
            }
        }
        GLOBAL_free(msg);
    }
    static_cast<ScriptEngine*>(g_train_resources)->ScriptEngine::Unlock();

    PlayerConnectionNode** handles[] = {
        (PlayerConnectionNode**)&handle_list_1,
        (PlayerConnectionNode**)&handle_list_2
    };
    for (unsigned i = 0; i < 2; ++i) {
        while (*handles[i] != NULL) {
            PlayerConnectionNode* node = *handles[i];
            if (node->file_handle != 0) {
                CloseHandle((void*)(uintptr_t)node->file_handle);
                node->file_handle = 0;
            }
            *handles[i] = (PlayerConnectionNode*)node->next;
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
    if (*(int32_t*)((uint8_t*)g_netman + 0x7C4) != 1) return;

    struct MissingAsset { uint8_t type, mode; uint8_t pad[2]; MissingAsset* next; };
    MissingAsset* missing = NULL;
    uint8_t* record = (uint8_t*)entity + 0x96;

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
            uint8_t* request = (uint8_t*)operator_new(6);
            *(uint16_t*)request = 0x3ED;
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
        *(uint8_t*)((uint8_t*)entity + 0x94),
        *(uint8_t*)((uint8_t*)entity + 0x93)
    };
    uint8_t special_types[2] = {
        *(uint8_t*)((uint8_t*)entity + 0x95), 1
    };
    uint8_t special_kinds[2] = {0x1E, 0x1F};
    for (int i = 0; i < 2; ++i) {
        if (special_modes[i] == 0) continue;
        uint8_t type = NET_MapSpecialAsset(special_kinds[i], special_types[i]);
        char path[0x504] = {0};
        NET_GetAssetPath(type, special_modes[i], path);
        bool present = GetFileAttributesA(path) != 0xFFFFFFFFu;
        if (!present) {
            uint8_t* request = (uint8_t*)operator_new(6);
            *(uint16_t*)request = 0x3ED;
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
    void* old_peer;

    old_peer = g_dplay_peer;

    if (g_dplay_peer != NULL) {
        DirectPlay_DestroyPeer(g_dplay_peer);
        GLOBAL_free(old_peer);
        g_dplay_peer = NULL;
        Sleep(1);
    }

    void* new_peer = operator_new(0x160c);
    if (new_peer == NULL) {
        new_peer = NULL;
    } else {
        new_peer = DirectPlay_CreatePeer(new_peer, this->context_id_a, 0);
    }
    g_dplay_peer = new_peer;

    if (g_dplay_peer != NULL) {
        *(int32_t*)((uint8_t*)g_dplay_peer + 0x940) = 0;
        *(uint8_t*)((uint8_t*)g_dplay_peer + 0x944) = 0;
        *(int32_t*)((uint8_t*)g_dplay_peer + 0x938) = this->context_id_b;
    }
}


/* ================================================================== */
/* TrainSubsystem::QueueMessage                                        */
/* Address: 0x4393D0                                                    */
/* ================================================================== */
void TrainSubsystem::QueueMessage(void* msg)
{
    NetworkMsg* net_msg = (NetworkMsg*)msg;

    /* In multiplayer mode (g_game_mode==10), handle/discard immediately */
    if (g_game_mode == 10 && net_msg->type != 8) {
        if (net_msg->data != NULL) {
            if (net_msg->type == 0x0E || net_msg->type == 0x10) {
                void** data_vt = *(void***)net_msg->data;
                ((void (__thiscall*)(void*, byte))data_vt[0])(net_msg->data, 1);
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
        g_network_queue = (NetworkQueueNode*)net_msg;
        static_cast<ScriptEngine*>(g_train_resources)->ScriptEngine::Unlock();
        return;
    }

    /* Walk to end, counting depth */
    NetworkMsg* cursor = (NetworkMsg*)g_network_queue;
    int depth = 1;
    while (cursor->next != NULL) {
        depth++;
        cursor = (NetworkMsg*)cursor->next;
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

    if ((intptr_t)thread_result != 0) {
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
    while ((intptr_t)thread_result != 0) {
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
    NetworkMsg* net_msg = (NetworkMsg*)msg;

    switch (net_msg->type) {
    case 0: /* HostSession */
        if (g_dplay_peer == NULL) {
            this->InitNetwork();
        }
        DirectPlay_Close(g_dplay_peer);
        DirectPlay_HostSession(g_dplay_peer,
                                net_msg->data != NULL ? 1 : 0,
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
        if (g_dplay_peer != NULL &&
            *(uint8_t*)((uint8_t*)g_dplay_peer + 0xd50) != 0) {
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
        /* netman[0x17].field_7cc at g_netman + 0x7D0 */
        uint32_t player_count = *(uint32_t*)((uint8_t*)g_netman + 0x7D0);
        this->UpdatePlayerCount(player_count);
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
        int32_t  player_id = *(int32_t*)lpMem;
        void*    payload   = *(void**)((uint8_t*)lpMem + 4);
        uint16_t msg_type  = *(uint16_t*)payload;

        /* === Low message types (0-20) === */
        if (msg_type < 0x15) {
            switch (msg_type) {
            case 0x14: /* 20 — PlayerLeave */
                this->HandlePlayerLeave(*(int32_t*)((uint16_t*)payload + 2));
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
                if (*(int32_t*)((uint8_t*)g_netman + 0x7C4) == 1) {
                    uint8_t* car = (uint8_t*)this->sprite_list_1;
                    while (car != 0) {
                        NetworkMsg* qmsg = AllocateNetworkMessage();
                        if (qmsg) {
                            qmsg->data = NULL; qmsg->next = NULL;
                            qmsg->type = 0x0F;
                            qmsg->data = this->sprite_list_1;
                        }
                        *(uint8_t*)(car + 0x88) = 0;
                        this->sprite_list_1 = *(void**)((uint8_t*)car + 0x70);
                        if (qmsg && qmsg->data) {
                            *(void**)((uint8_t*)qmsg->data + 0x70) = NULL;
                        }
                        NETMAN_QueueMessage(qmsg);
                        car = (uint8_t*)this->sprite_list_1;
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
                    uint8_t* car = (uint8_t*)this->sprite_list_1;
                    while (car != 0) {
                        *(uint16_t*)(car + 0x74) = 32000;
                        car = *(uint8_t**)(car + 0x70);
                    }
                }

                /* Send player info response (0x3E9, 24 bytes) */
                {
                    uint16_t* resp = (uint16_t*)operator_new(0x18);
                    if (resp) {
                        resp[0] = 0x3E9;
                        *(int32_t*)(resp + 2) = *(int32_t*)((uint8_t*)g_player_config + 0x14);
                        *(int32_t*)(resp + 4) = *(int32_t*)((uint8_t*)g_main_window + 0x18);
                        *(int32_t*)(resp + 6) = *(int32_t*)((uint8_t*)g_main_window + 0x1C);
                        *(int32_t*)(resp + 8) = *(int32_t*)((uint8_t*)g_main_window + 0x20);
                        *(int32_t*)(resp + 10) = *(int32_t*)((uint8_t*)g_main_window + 0x24);
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
                this->player_peer_id = *(int32_t*)((uint8_t*)g_dplay_peer + 0x924);
                NETMAN_QueueMessage(qmsg);
            }
            goto free_msg;
        }

        /* === High message types (0x3EA-0x3FD) === */
        switch (msg_type - 0x3EA) {
        case 0: { /* 0x3EA — PlayerInfo */
            uint16_t* p = (uint16_t*)payload;
            uint8_t info_flag = (uint8_t)p[4];
            int32_t config_val = *(int32_t*)(p + 2);

            uint8_t* car = (uint8_t*)this->sprite_list_1;
            while (car) { *(uint16_t*)(car + 0x74) = 32000; car = *(uint8_t**)(car + 0x70); }

            if (info_flag) this->field_30 = 1;
            *(int32_t*)((uint8_t*)g_player_config + 0x14) = config_val;
            /* PlayerConfig_Save is called inside Train_SendPlayerInfo */
            Train_SendPlayerInfo(this);
            break;
        }

        case 1: /* 0x3EB — ConnectToServer */
            Train_ConnectToServer(this, (uint8_t*)payload);
            break;

        case 2: /* 0x3EC — HandleTrackBuild */
            Train_HandleTrackBuild(this, (uint8_t*)payload);
            break;

        case 4: { /* 0x3EE — FileData (incoming asset) */
            uint16_t* p = (uint16_t*)payload;
            char path_buf[1284];
            uint32_t bytes_written;

            NET_GetAssetPath(*(uint8_t*)((uint8_t*)p + 5), (uint8_t)p[2], path_buf);
            void* hFile = (void*)(uintptr_t)CreateFileA(path_buf, 0x40000000, 0, NULL, 1, 0x80, NULL);
            if (hFile != (void*)0xFFFFFFFF) {
                WriteFile(hFile, p + 6, *(uint32_t*)(p + 4), &bytes_written, NULL);
                CloseHandle(hFile);
                this->request_count--;
            }
            if (this->request_count == 0) {
                DirectPlay_Close(g_dplay_peer);
            }
            break;
        }

        case 6: { /* 0x3F0 — GameOver control signal */
            if (*(uint8_t*)g_dplay_peer != 0) {
                NetworkMsg* qmsg = AllocateNetworkMessage();
                if (qmsg) {
                    qmsg->data = NULL; qmsg->next = NULL;
                    qmsg->type = 4;
                    /* 0x439948 allocates the protocol's fixed 13-byte name
                     * buffer and copies the C string at payload +0x08. */
                    char* str = (char*)operator_new(0x0D);
                    if (str) strcpy(str, (char*)payload + 8);
                    qmsg->data = str;
                    qmsg->to_player = player_id;
                    qmsg->flags = *(int32_t*)((uint16_t*)payload + 2);
                }
                NETMAN_QueueMessage(qmsg);
            }
            break;
        }

        case 7: { /* 0x3F1 — LobbyInfo/PlayerList */
            uint16_t* p = (uint16_t*)payload;
            this->player_peer_id = player_id;

            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) {
                qmsg->data = NULL; qmsg->next = NULL;
                qmsg->type = 9;
                qmsg->flags = *(int32_t*)(p + 2);
                qmsg->setMetadata0((uint8_t)p[4]);
                qmsg->setMetadata1(*(uint8_t*)((uint8_t*)p + 9));

#ifndef _WIN32
                qmsg->data = DPLAY_DecodePlayerSlots(
                    static_cast<uint8_t*>(payload) + 0x0C);
#else
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
            this->HandleConnectionSetup((void*)((uint16_t*)payload));
            break;

        case 9: /* 0x3F3 — ControllerInit */
            this->HandleControllerInit((void*)((uint16_t*)payload), player_id);
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
            uint16_t* p = (uint16_t*)payload;
            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) {
                qmsg->data = NULL; qmsg->next = NULL;
                qmsg->type = 0x1A;
                qmsg->size = 0;
                qmsg->data = (void*)(uintptr_t)(uint32_t)((uint8_t)p[2]);
                qmsg->to_player = player_id;
            }
            NETMAN_QueueMessage(qmsg);
            this->UpdatePlayerCount((uint32_t)((uint8_t)p[2]));
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
                void* heap_payload = *(void**)((uint8_t*)lpMem + 4);
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

    PlayerConnectionNode* node = (PlayerConnectionNode*)
        operator_new(sizeof(PlayerConnectionNode));
    if (node == NULL) return;

    node->player_id      = player_id;
    node->file_handle    = 0;
    node->sub_type       = *(uint16_t*)((uint8_t*)data + 4);
    node->extra_info     = *(uint16_t*)((uint8_t*)data + 6);
    node->transfer_state = 0;
    node->sequence_num   = 0;
    node->throttle       = 0;
    node->next           = NULL;

    /* Open .att file for reading */
    {
        char att_path[0x144];
        att_path[0] = 0;
        NET_GetAttFilePath(node->sub_type, 4, att_path);
        void* hFile = (void*)(uintptr_t)CreateFileA(att_path, 0x80000000, 1, NULL,
                                          4, 0x8000000, NULL);
        node->file_handle = (int32_t)hFile;
    }

    if (node->file_handle != -1) {
        /* Append to handle_list_1 */
        if (this->handle_list_1 == NULL) {
            this->handle_list_1 = node;
        } else {
            PlayerConnectionNode* tail = (PlayerConnectionNode*)this->handle_list_1;
            while (tail->next) tail = (PlayerConnectionNode*)tail->next;
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
    PlayerConnectionNode* node = (PlayerConnectionNode*)this->handle_list_1;

    if (node == NULL) return;

    while (node != NULL) {
        if (node->throttle > 0) {
            node->throttle--;
            prev = node;
            node = (PlayerConnectionNode*)node->next;
            continue;
        }
        node->throttle = 0x14; /* 20-tick throttle */

        if (node->transfer_state == 0) {
            /* FIRST block */
            uint16_t* buf = (uint16_t*)operator_new(0x7FEC);
            uint32_t bytes_read = 0;

            if (!ReadFile((void*)(uintptr_t)node->file_handle,
                           (void*)((uint8_t*)buf + 0x0D),
                           0x7FDC, &bytes_read, NULL)) {
                CloseHandle((void*)(uintptr_t)node->file_handle);
                node->file_handle = 0;
                GLOBAL_free(buf);
                goto remove_node;
            }

            buf[0] = 0x3FC;
            *(int32_t*)(buf + 2) = bytes_read;
            buf[4] = node->sub_type;
            buf[5] = 0;
            *(uint8_t*)(buf + 6) = 0; /* sub-type = FIRST */
            WIN32_SendNetworkData(g_dplay_peer, node->player_id,
                                  buf, bytes_read + 0x10, 1);
            GLOBAL_free(buf);
            node->transfer_state = 1;
            return;
        }

        if (node->transfer_state == 1) {
            /* INTERIM block */
            uint16_t* buf = (uint16_t*)operator_new(0x7FEC);
            uint32_t bytes_read = 0;

            if (!ReadFile((void*)(uintptr_t)node->file_handle,
                           (void*)((uint8_t*)buf + 0x0D),
                           0x7FDC, &bytes_read, NULL)) {
                CloseHandle((void*)(uintptr_t)node->file_handle);
                node->file_handle = 0;
                GLOBAL_free(buf);
                goto remove_node;
            }

            if (bytes_read > 0) {
                node->sequence_num++;
                buf[0] = 0x3FC;
                *(int32_t*)(buf + 2) = bytes_read;
                buf[4] = node->sub_type;
                buf[5] = node->sequence_num;
                *(uint8_t*)(buf + 6) = 1; /* sub-type = INTERIM */
                WIN32_SendNetworkData(g_dplay_peer, node->player_id,
                                      buf, bytes_read + 0x10, 1);
                GLOBAL_free(buf);
                return;
            }

            /* Zero bytes -> move to FINAL */
            node->transfer_state = 2;
            CloseHandle((void*)(uintptr_t)node->file_handle);
            node->file_handle = 0;
            GLOBAL_free(buf);
            prev = node;
            node = (PlayerConnectionNode*)node->next;
            continue;
        }

        /* FINAL block */
        {
            uint16_t* buf = (uint16_t*)operator_new(0x410);
            char att_path[0x144] = {0};
            uint32_t bytes_read = 0;

            NET_GetFilePath(node->sub_type, 4, att_path);
            void* hFile = (void*)(uintptr_t)CreateFileA(att_path, 0x80000000, 1, NULL,
                                              4, 0x8000000, NULL);
            node->file_handle = (int32_t)hFile;

            if (hFile == (void*)0xFFFFFFFF) {
                node->file_handle = 0;
                GLOBAL_free(buf);
                goto remove_node;
            }

            if (ReadFile(hFile, (void*)((uint8_t*)buf + 0x0D), 0x400,
                         &bytes_read, NULL)) {
                node->sequence_num++;
                buf[0] = 0x3FC;
                *(int32_t*)(buf + 2) = bytes_read;
                buf[4] = node->sub_type;
                buf[5] = node->sequence_num;
                *(uint8_t*)(buf + 6) = 2; /* sub-type = FINAL */
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
            PlayerConnectionNode* next = (PlayerConnectionNode*)node->next;
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
    uint8_t  sub_type   = *(uint8_t*)((uint8_t*)data + 0x0C);
    uint16_t train_type = *(uint16_t*)((uint8_t*)data + 8);

    PlayerConnectionNode* node = (PlayerConnectionNode*)this->handle_list_2;
    PlayerConnectionNode* prev = NULL;

    /* Find matching node */
    while (node != NULL) {
        if (node->sub_type == train_type) break;
        prev = node;
        node = (PlayerConnectionNode*)node->next;
    }
    if (node == NULL) return;

    if (sub_type == 0) {
        /* FIRST block */
        char path_buf[0x144] = {0};
        uint32_t bytes_written;

        if (node->sequence_num == 0) {
            NET_GetAttFilePath(train_type, 5, path_buf);
            void* hFile = (void*)(uintptr_t)CreateFileA(path_buf, 0x40000000, 1, NULL,
                                              1, 0x8000000, NULL);
            node->file_handle = (int32_t)hFile;

            if (hFile != (void*)0xFFFFFFFF) {
                WriteFile(hFile, (void*)((uint8_t*)data + 0x0D),
                          *(uint32_t*)((uint8_t*)data + 4),
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
        uint16_t expected = *(uint16_t*)((uint8_t*)data + 10);

        if (node->sequence_num != expected) {
            g_OutputDebugStringA("Attachment Interim Block out of sequence");
            CloseHandle((void*)(uintptr_t)node->file_handle);
            node->file_handle = 0;

            NetworkMsg* msg = AllocateNetworkMessage();
            if (msg) { msg->data = NULL; msg->next = NULL;
                       msg->type = 0x18;
                       msg->to_player = node->notify_id;
                       msg->setMetadata0(node->transfer_state); }
            NETMAN_QueueMessage(msg);
            goto unlink_node;
        }

        if (!WriteFile((void*)(uintptr_t)node->file_handle,
                        (void*)((uint8_t*)data + 0x0D),
                        *(uint32_t*)((uint8_t*)data + 4),
                        &bytes_written, NULL)) {
            CloseHandle((void*)(uintptr_t)node->file_handle);
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
            CloseHandle((void*)(uintptr_t)node->file_handle);
            node->file_handle = 0;
        }

        node->sequence_num++;
        uint16_t expected = *(uint16_t*)((uint8_t*)data + 10);

        if (node->sequence_num == expected) {
            NET_GetFilePath(train_type, 5, path_buf);
            void* hFile = (void*)(uintptr_t)CreateFileA(path_buf, 0x40000000, 1, NULL,
                                              1, 0x8000000, NULL);
            node->file_handle = (int32_t)hFile;
            if (hFile != (void*)0xFFFFFFFF) {
                WriteFile(hFile, (void*)((uint8_t*)data + 0x0D),
                          *(uint32_t*)((uint8_t*)data + 4),
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
    local_slot_index = *reinterpret_cast<const int32_t*>(
        static_cast<const uint8_t*>(g_netman) + 0x7D0);
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
        PlayerConnectionNode* node = (PlayerConnectionNode*)this->handle_list_2;

        while (node != NULL) {
            if (node->player_id == player_id) {
                if (prev == NULL) {
                    this->handle_list_2 = node->next;
                } else {
                    prev->next = node->next;
                }
                node->next = NULL;

                if (node->file_handle != 0 && node->file_handle != -1) {
                    CloseHandle((void*)(uintptr_t)node->file_handle);
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
                node = (PlayerConnectionNode*)this->handle_list_2;
            } else {
                prev = node;
                node = (PlayerConnectionNode*)node->next;
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
    uint32_t local_player = *(uint32_t*)((uint8_t*)g_netman + 0x7D0);

    void*  prev = NULL;
    void*  node = this->sprite_list_3;

    while (node != NULL) {
        uint8_t node_owner = *(uint8_t*)((uint8_t*)node + 0x7C);
        void*   next_node = *(void**)((uint8_t*)node + 0x70);

        if (node_owner == (uint8_t)player_index) {
            /* Unlink this node */
            if (prev == NULL) {
                this->sprite_list_3 = *(void**)((uint8_t*)node + 0x70);
            } else {
                *(void**)((uint8_t*)prev + 0x70) = *(void**)((uint8_t*)node + 0x70);
            }

            if (player_index == local_player) {
                /* Local player's slot: preserve on sprite_list_1 free list */
                *(uint8_t*)((uint8_t*)node + 0x7C) = (uint8_t)local_player;
                *(void**)((uint8_t*)node + 0x70) = this->sprite_list_1;
                this->sprite_list_1 = node;
                node = this->sprite_list_3;
            } else {
                /* Remote player: destroy via vtable[0] */
                void** vt = *(void***)node;
                ((void (__thiscall*)(void*, byte))vt[0])(node, 1);
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

    if (*(int*)((uint8_t*)g_dplay_peer + 0x1588) == 0) {
        /* No session — queue type-5 disconnect */
        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg) { msg->data = NULL; msg->next = NULL; }
        if (msg) { msg->type = 5; msg->data = NULL; }
        NETMAN_QueueMessage(msg);
        return;
    }

    /* Reconnect to session to send shutdown message */
    if (*(uint8_t*)g_dplay_peer == 0) {
        /* Host player */
        if (*(int32_t*)((uint8_t*)g_netman + 0x7C4) == 1) {
            /* Scenario mode: read server name from config */
            char buf[1024];
            Config_GetIniString(g_config_ini, "Configuration", "ServerName",
                                "LEGO International Train Server", buf, 0x400);
            DirectPlay_ConnectToSession(g_dplay_peer,
                                         (char*)((uint8_t*)g_player_config + 6),
                                         buf, NULL);
            if (*(uint8_t*)((uint8_t*)g_dplay_peer + 0xd50) == 0) {
                Sleep(1000);
                DirectPlay_ConnectToSession(g_dplay_peer,
                                             (char*)((uint8_t*)g_player_config + 6),
                                             buf, NULL);
            }
            if (*(uint8_t*)((uint8_t*)g_dplay_peer + 0xd50) == 0) {
                Sleep(2000);
                goto send_disconnect;
            }
        } else {
            /* Non-scenario: use UI address */
            char* addr = *(char**)(*(uintptr_t*)((uint8_t*)g_ui_main + 0x220) + 0xF8);
            DirectPlay_ConnectToSession(g_dplay_peer,
                                         (char*)((uint8_t*)g_player_config + 6),
                                         addr, NULL);
        }
    } else {
        /* Client player: format name pair */
        char name_buf[256];
        wsprintfA(name_buf, "%s %s",
                  *(char**)(*(uintptr_t*)((uint8_t*)g_ui_main + 0x220) + 0xFC),
                  (char*)((uint8_t*)g_player_config + 6));
        DirectPlay_ConnectToSession(g_dplay_peer,
                                     (char*)((uint8_t*)g_player_config + 6),
                                     name_buf, NULL);
    }

    if (*(uint8_t*)((uint8_t*)g_dplay_peer + 0xd50) != 0) {
        /* Connected — send type-3 shutdown */
        NetworkMsg* msg = AllocateNetworkMessage();
        if (msg) { msg->data = NULL; msg->next = NULL; }
        if (msg) {
            msg->type = 3;
            msg->data = NULL;
            msg->to_player = *(int32_t*)((uint8_t*)g_dplay_peer + 0x924);
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
        if (*(uint8_t*)((uint8_t*)g_dplay_peer + 0xd50) != 0 &&
            *(int32_t*)((uint8_t*)g_netman + 0x7C4) == 2) {
            uint16_t msg = 0x3FD;
            WIN32_SendNetworkData(g_dplay_peer, 0, &msg, 4, 1);
            Sleep(10);
        }

        /* Close and destroy DirectPlay peer */
        DirectPlay_Close(g_dplay_peer);
        void* old_peer = g_dplay_peer;
        if (old_peer != NULL) {
            DirectPlay_DestroyPeer(old_peer);
            GLOBAL_free(old_peer);
        }
        g_dplay_peer = NULL;
    }

    /* Only free lists in scenario mode or non-scenario */
    int scenario = *(int32_t*)((uint8_t*)g_netman + 0x7C4);
    if (scenario == 2 || scenario == 0) {
        /* Free sprite_list_1 (active) */
        {
            void* node = this->sprite_list_1;
            while (node != NULL) {
                this->sprite_list_1 = *(void**)((uint8_t*)node + 0x70);
                void** vt = *(void***)node;
                ((void (__thiscall*)(void*, byte))vt[0])(node, 1);
                node = this->sprite_list_1;
            }
        }

        /* Free sprite_list_2 (dead) */
        {
            void* node = this->sprite_list_2;
            while (node != NULL) {
                this->sprite_list_2 = *(void**)((uint8_t*)node + 0x70);
                void** vt = *(void***)node;
                ((void (__thiscall*)(void*, byte))vt[0])(node, 1);
                node = this->sprite_list_2;
            }
        }

        /* Free sprite_list_3 (persistent) */
        {
            void* node = this->sprite_list_3;
            while (node != NULL) {
                this->sprite_list_3 = *(void**)((uint8_t*)node + 0x70);
                void** vt = *(void***)node;
                ((void (__thiscall*)(void*, byte))vt[0])(node, 1);
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
    NetworkMsg* net_msg = (NetworkMsg*)msg;
    void*       car     = net_msg->data;
    int         dir     = net_msg->flags;

    if (*(int32_t*)((uint8_t*)g_netman + 0x7C4) != 2) {
        /* Not in multiplayer scenario — destroy car */
        if (car != NULL) {
            void** vt = *(void***)car;
            ((void (__thiscall*)(void*, byte))vt[0])(car, 1);
            net_msg->data = NULL;
        }
        return;
    }

    /* Check if this is a 'remove' message */
    if (car && *(int32_t*)((uint8_t*)car + 4) == 1) {
        if (car) {
            void** vt = *(void***)car;
            ((void (__thiscall*)(void*, byte))vt[0])(car, 1);
        }
        net_msg->data = NULL;
        return;
    }

    /* Compute target town index from direction */
    int local_player = *(int*)((uint8_t*)g_netman + 0x7D0);
    int target_town = local_player;

    if (dir < 0x5B) {
        if (dir == 0x5A)      target_town = local_player + 1;
        else if (dir == 0)    target_town = local_player - *(int*)((uint8_t*)g_netman + 0xC);
    } else {
        if (dir == 0xB4)      target_town = *(int*)((uint8_t*)g_netman + 0xC) + local_player;
        else if (dir == 0x10E) target_town = local_player - 1;
    }

    /* Find player at target town */
    int* player_slot = NULL;
    if (target_town >= 0) {
        /* netman[0xF].playerIds + target_town * 0x13 + 6 */
        player_slot = (int*)((uint8_t*)g_netman + 0x518 + target_town * 0x4C);
    }

    if (player_slot && *player_slot != 0) {
        /* Player exists at target — transfer the car */
        uint8_t result = this->MoveToNeighborTown(*player_slot, car, dir);
        if ((char)result == 0) {
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
            uint8_t* tail = (uint8_t*)this->sprite_list_2;
            while (*(uint8_t**)(tail + 0x70) != 0) {
                tail = *(uint8_t**)(tail + 0x70);
            }
            *(void**)((uint8_t*)car + 0x70) = NULL;
            *(void**)(tail + 0x70) = car;
        } else {
            *(void**)((uint8_t*)car + 0x70) = NULL;
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
    uint16_t* p = (uint16_t*)data;

    /* Create a new Vehicle controller */
    void* vehicle_obj = operator_new(0x94);
    void* controller = NULL;
    if (vehicle_obj) {
        controller = Vehicle_Ctor(vehicle_obj, *(int*)(p + 8), 2, 1, 1);
    }
    if (controller == NULL) return;

    /* Set direction and town info */
    *(uint16_t*)((uint8_t*)controller + 0x74) = p[2];    /* direction */
    *(uint16_t*)((uint8_t*)controller + 0x7A) = p[3];    /* resource ID */
    *(uint8_t*)((uint8_t*)controller + 0x78) = *(uint8_t*)((uint8_t*)p + 10);  /* type */
    Vehicle_CalcSpeed(controller, *(short*)((uint8_t*)p + 8));

    /* Set parent/controller reference */
    *(int32_t*)((uint8_t*)controller + 8) = *(int32_t*)(p + 6);

    /* Process up to 3 track elements */
    if (*(uint8_t*)((uint8_t*)p + 0x14) != 0) {
        uint32_t* track_entry = (uint32_t*)(p + 0x16);

        for (int i = 0; i < *(uint8_t*)((uint8_t*)p + 0x14); i++) {
            Vehicle_InitRoute(controller, track_entry[-5], track_entry[-4], 1);

            if (*(uint8_t*)(track_entry + -3) != 0) {
                /* Has DPLAY data — check if it matches local player */
                uint16_t track_id = (uint16_t)track_entry[-1];
                int player_count = NETMAN_GetPlayerCount(g_netman);

                uint8_t* player_name = (uint8_t*)track_entry;
                /* 2-byte stride name compare */
                int match = -1;
                for (int j = 0; j < player_count; j++) {
                    /* Compare player name from track_entry */
                    uint8_t* pn = (uint8_t*)((uint8_t*)g_netman + 0x51D + j * 0x4C);
                    uint8_t* tn = player_name;
                    int k = 0;
                    while (tn[k] == pn[k] && tn[k] != 0) { k++; }
                    if (tn[k] == pn[k]) { match = j; break; }
                }

                if (match >= 0) {
                    /* Send PlayerJoin response for matching track */
                    uint16_t* resp = (uint16_t*)operator_new(8);
                    if (resp) {
                        resp[0] = 0x3FB;
                        resp[2] = track_id;
                        resp[3] = NET_GetNextAttId();

                        /* Find player by name match */
                        int* target_player = NULL;
                        for (int j = 0; j < player_count; j++) {
                            uint8_t* pn = (uint8_t*)((uint8_t*)g_netman + 0x51D + j * 0x4C);
                            uint8_t* tn = (uint8_t*)track_entry + 6; /* 2+ byte per char */
                            int k = 0;
                            while (tn[k*2] == pn[k] && tn[k*2] != 0) { k++; }
                            if (tn[k*2] == pn[k]) {
                                target_player = (int*)((uint8_t*)g_netman + 0x518 + j * 0x4C);
                                break;
                            }
                        }

                        if (target_player) {
                            WIN32_SendNetworkData(g_dplay_peer, *target_player,
                                                  resp, 8, 1);

                            /* Create PlayerConnectionNode for attachment transfer */
                            track_id = resp[3]; /* att ID */
                            *(uint8_t*)((uint8_t*)controller + 0x89) += 1;

                            PlayerConnectionNode* node = (PlayerConnectionNode*)
                                operator_new(sizeof(PlayerConnectionNode));
                            if (node) {
                                node->player_id      = *target_player;
                                node->file_handle    = 0;
                                node->sub_type       = resp[3];
                                node->extra_info     = resp[3];
                                node->transfer_state = *(uint8_t*)((uint8_t*)controller + 0x78);
                                node->notify_id      = *(uint16_t*)((uint8_t*)controller + 0x7A);
                                node->sequence_num   = 0;
                                node->throttle       = 0;
                                node->next           = this->handle_list_2;
                                *(uint8_t*)((uint8_t*)node + 8) = *(uint8_t*)((uint8_t*)controller + 0x78);
                                *(uint16_t*)((uint8_t*)node + 10) = *(uint16_t*)((uint8_t*)controller + 0x7A);
                                this->handle_list_2 = node;
                            }
                        }
                        GLOBAL_free(resp);
                    }
                }

                /* Set DPLAY data for this track element */
                VehicleEditor_SetDPlayData((void*)(uintptr_t)*(uint32_t*)((uint8_t*)controller + 0x14 + i * 4),
                                            (int)&track_entry[-7]);
            }
            track_entry += 0x75; /* advance by 0xEA bytes / 4 = 0x75 dwords */
        }
    }

    /* Call vtable[13] on the controller's 5th field (4 = +0x10 ptr) */
    {
        void** vt = *(void***)(*(uintptr_t*)((uint8_t*)controller + 0x10));
        ((void (__thiscall*)(void*, void*))vt[13])(*(void**)((uint8_t*)controller + 0x10),
                                                     (void*)((uint8_t*)p + 0xB10));
    }

    /* If this train belongs to the local player, remove from sprite_list_1 */
    if (*(uint8_t*)((uint8_t*)controller + 0x78) ==
        *(uint32_t*)((uint8_t*)g_netman + 0x7D0)) {
        void* prev = NULL;
        void* node = this->sprite_list_1;
        while (node) {
            if (*(uint16_t*)((uint8_t*)node + 0x7A) ==
                *(uint16_t*)((uint8_t*)controller + 0x7A)) {
                if (prev == NULL) {
                    this->sprite_list_1 = *(void**)((uint8_t*)node + 0x70);
                } else {
                    *(void**)((uint8_t*)prev + 0x70) = *(void**)((uint8_t*)node + 0x70);
                }
                void** vt = *(void***)node;
                ((void (__thiscall*)(void*, byte))vt[0])(node, 1);
                break;
            }
            prev = node;
            node = *(void**)((uint8_t*)node + 0x70);
        }
    }

    /* Broadcast MSG_CTRL_INIT (0x3F3) to all players */
    {
        uint16_t* ctrl_init = (uint16_t*)operator_new(10);
        if (ctrl_init) {
            ctrl_init[0] = 0x3F3;
            ctrl_init[2] = *(uint16_t*)((uint8_t*)controller + 0x7A);
            *(uint8_t*)((uint8_t*)ctrl_init + 6) = *(uint8_t*)((uint8_t*)controller + 0x78);
            *(uint8_t*)((uint8_t*)ctrl_init + 7) = *(uint8_t*)((uint8_t*)g_netman + 0x7D0);
            ctrl_init[4] = *(uint16_t*)((uint8_t*)controller + 0x74);
            WIN32_SendNetworkData(g_dplay_peer, 0, ctrl_init, 10, 1);
            GLOBAL_free(ctrl_init);
        }
    }

    /* Set controller owner and queue type-0x11 notification */
    *(uint8_t*)((uint8_t*)controller + 0x7C) = *(uint8_t*)((uint8_t*)g_netman + 0x7D0);
    *(uint8_t*)((uint8_t*)controller + 0x8A) = 0;

    {
        NetworkMsg* notify = AllocateNetworkMessage();
        if (notify) {
            notify->data = NULL; notify->next = NULL;
            notify->type = 0x11;
            notify->data = controller;
        }
        *(uint8_t*)((uint8_t*)controller + 0x88) = 0;
        NETMAN_QueueMessage(notify);
    }
}


/* ================================================================== */
/* TrainSubsystem::HandleControllerInit                                */
/* Address: 0x43B6D0                                                    */
/* ================================================================== */
void TrainSubsystem::HandleControllerInit(void* data, int dplay_id)
{
    uint16_t* p = (uint16_t*)data;
    uint16_t  train_id = p[2];
    uint8_t   color    = *(uint8_t*)((uint8_t*)p + 6);
    uint8_t   owner    = *(uint8_t*)((uint8_t*)p + 7);
    uint16_t  dir      = p[4];

    /* Find matching car in sprite_list_1 */
    {
        uint8_t* car = (uint8_t*)this->sprite_list_1;
        while (car != 0) {
            if (*(uint16_t*)(car + 0x7A) == train_id &&
                *(uint8_t*)(car + 0x78) == color) {
                *(int32_t*)(car + 0x8C) = dplay_id;
                *(uint8_t*)(car + 0x7C) = owner;
                break;
            }
            car = *(uint8_t**)(car + 0x70);
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
    if (*(int32_t*)((uint8_t*)g_netman + 0x7C4) != 2) return;

    uint32_t player_index;
    if (player_id == 0) {
        player_index = *(uint32_t*)((uint8_t*)g_netman + 0x7D0);
    } else {
        player_index = NETMAN_FindPlayerIndex(g_netman, player_id);
    }

    if ((int)player_index < 0) return;

    /* Walk sprite_list_1 and remove matching cars */
    {
        void* prev = NULL;
        void* node = this->sprite_list_1;

        while (node != NULL) {
            uint8_t owner_byte = *(uint8_t*)((uint8_t*)node + 0x7C);
            uint8_t color_byte = *(uint8_t*)((uint8_t*)node + 0x78);
            int     dplay_id   = *(int32_t*)((uint8_t*)node + 0x8C);

            int match = 0;
            if (player_id != 0 && owner_byte == (uint8_t)player_index) {
                match = 1;
            }
            if (player_id == 0 && color_byte == (uint8_t)player_index) {
                match = 1;
            }
            if (dplay_id == player_id) {
                match = 1;
            }

            if (match) {
                void* next_node = *(void**)((uint8_t*)node + 0x70);

                /* Unlink from sprite_list_1 */
                if (prev == NULL) {
                    this->sprite_list_1 = next_node;
                } else {
                    *(void**)((uint8_t*)prev + 0x70) = next_node;
                }

                /* Reverse direction */
                uint16_t dir = *(uint16_t*)((uint8_t*)node + 0x74);
                if (dir < 0x5B) {
                    if (dir == 0x5A)      dir = 0x10E;
                    else if (dir == 0)    dir = 0xB4;
                } else {
                    if (dir == 0xB4)      dir = 0;
                    else if (dir == 0x10E) dir = 0x5A;
                }
                *(uint16_t*)((uint8_t*)node + 0x74) = dir;

                /* Set owner to local player */
                *(uint8_t*)((uint8_t*)node + 0x7C) =
                    *(uint8_t*)((uint8_t*)g_netman + 0x7D0);

                /* Clear DPlay data on all carriages */
                if (*(short*)((uint8_t*)node + 0x0C) != 0) {
                    void** carriage = (void**)((uint8_t*)node + 0x14);
                    for (int j = 0; j < *(uint16_t*)((uint8_t*)node + 0x0C); j++) {
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
                *(uint8_t*)((uint8_t*)node + 0x88) = 0;
                NETMAN_QueueMessage(msg);

                node = this->sprite_list_1;
            } else {
                prev = node;
                node = *(void**)((uint8_t*)node + 0x70);
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
    int* player_slot = NULL;
    if (player_index >= 0) {
        player_slot = (int*)((uint8_t*)g_netman + 0x518 + player_index * 0x4C);
    }

    if (player_slot && player_slot[0x11] != 0) {
        /* === Multiplayer path: prepend to sprite_list_3, broadcast 0x3F3 === */

        *(uint8_t*)((uint8_t*)car + 0x7C) = (uint8_t)player_index;
        *(void**)((uint8_t*)car + 0x70) = this->sprite_list_3;
        this->sprite_list_3 = car;

        /* Set movement direction in +0x76 */
        if (direction < 0x5B) {
            if (direction == 0x5A)      *(uint16_t*)((uint8_t*)car + 0x76) = 0x5A;
            else if (direction == 0)    *(uint16_t*)((uint8_t*)car + 0x76) = 0;
        } else {
            if (direction == 0xB4)      *(uint16_t*)((uint8_t*)car + 0x76) = 0xB4;
            else if (direction == 0x10E) *(uint16_t*)((uint8_t*)car + 0x76) = 0x10E;
        }

        /* Broadcast MSG_CTRL_INIT (0x3F3) */
        uint16_t* ctrl_init = (uint16_t*)operator_new(10);
        if (ctrl_init) {
            ctrl_init[0] = 0x3F3;
            ctrl_init[2] = *(uint16_t*)((uint8_t*)car + 0x7A);
            *(uint8_t*)((uint8_t*)ctrl_init + 6) = *(uint8_t*)((uint8_t*)car + 0x78);
            *(uint8_t*)((uint8_t*)ctrl_init + 7) = *(uint8_t*)((uint8_t*)car + 0x7C);
            ctrl_init[4] = (uint16_t)direction;
            WIN32_SendNetworkData(g_dplay_peer, 0, ctrl_init, 10, 1);

            /* Also update local handler */
            this->HandleControllerInit(ctrl_init,
                *(int32_t*)((uint8_t*)g_netman + 0x7D4)); /* field_7d0 */

            GLOBAL_free(ctrl_init);
        }

        /* Compute tile offset from direction */
        int dx = 0, dy = 0;
        uint16_t move_dir = *(uint16_t*)((uint8_t*)car + 0x76);

        if (move_dir == 0x5A) { dx = 1; dy = 0; }
        else if (move_dir == 0) { dx = 0; dy = -1; }
        else if (move_dir == 0xB4) { dx = 0; dy = 1; }
        else if (move_dir == 0x10E) { dx = -1; dy = 0; }

        *(int16_t*)((uint8_t*)car + 0x7E) = (int16_t)(dx + 1);
        *(int16_t*)((uint8_t*)car + 0x80) = (int16_t)(dy + (move_dir == 0x5A ? 1 : move_dir == 0 ? -1 : move_dir == 0xB4 ? 1 : 1));
        *(uint16_t*)((uint8_t*)car + 0x84) = 0xFFFF;
        *(uint16_t*)((uint8_t*)car + 0x86) = 0xFFFF;
        *(uint8_t*)((uint8_t*)car + 0x82) = 0;
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
    *(uint16_t*)((uint8_t*)car + 0x74) = (uint16_t)direction;

    /* Append to sprite_list_2 */
    if (this->sprite_list_2 == NULL) {
        *(void**)((uint8_t*)car + 0x70) = NULL;
        this->sprite_list_2 = car;
    } else {
        uint8_t* tail = (uint8_t*)this->sprite_list_2;
        while (*(uint8_t**)(tail + 0x70) != 0) {
            tail = *(uint8_t**)(tail + 0x70);
        }
        *(void**)((uint8_t*)car + 0x70) = NULL;
        *(void**)(tail + 0x70) = car;
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
        void* next_node = *(void**)((uint8_t*)node + 0x70);

        /* Check if player at node's town is still connected */
        uint8_t owner = *(uint8_t*)((uint8_t*)node + 0x7C);
        int* player_slot = (int*)((uint8_t*)g_netman + 0x518 + owner * 0x4C);

        if (player_slot[0x11] == 0 || *player_slot != 0) {
            /* Owner disconnected — move from sprite_list_3 to sprite_list_2 */
            if (prev == NULL) {
                this->sprite_list_3 = next_node;
            } else {
                *(void**)((uint8_t*)prev + 0x70) = next_node;
            }
            *(void**)((uint8_t*)node + 0x70) = NULL;

            /* Mirror direction */
            uint16_t dir = *(uint16_t*)((uint8_t*)node + 0x74);
            if (dir < 0x5B) {
                if (dir == 0x5A)      dir = 0x10E;
                else if (dir == 0)    dir = 0xB4;
            } else {
                if (dir == 0xB4)      dir = 0;
                else if (dir == 0x10E) dir = 0x5A;
            }
            *(uint16_t*)((uint8_t*)node + 0x74) = dir;

            /* Append to sprite_list_2 */
            if (this->sprite_list_2 == NULL) {
                this->sprite_list_2 = node;
            } else {
                uint8_t* tail = (uint8_t*)this->sprite_list_2;
                while (*(uint8_t**)(tail + 0x70) != 0) tail = *(uint8_t**)(tail + 0x70);
                *(void**)(tail + 0x70) = node;
            }

            /* Re-notify all cars in sprite_list_2 */
            uint8_t* car = (uint8_t*)this->sprite_list_2;
            while (car != 0) {
                NetworkMsg* msg = AllocateNetworkMessage();
                if (msg) { msg->data = NULL; msg->next = NULL;
                           msg->type = 0x11;
                           msg->data = this->sprite_list_2; }
                ((Building*)this->sprite_list_2)->occupation_level = 0;

                uint8_t* cur = (uint8_t*)this->sprite_list_2;
                *(uint8_t*)(cur + 0x7C) = *(uint8_t*)((uint8_t*)g_netman + 0x7D0);
                *(void**)(cur + 0x70) = NULL;
                *(uint8_t*)(cur + 0x88) = 0;
                this->sprite_list_2 = *(void**)(cur + 0x70);

                NETMAN_QueueMessage(msg);
                car = (uint8_t*)this->sprite_list_2;
            }

            node = this->sprite_list_3;
            continue;
        }

        /* === Check map edge routing === */
        int pos_x = *(int16_t*)((uint8_t*)node + 0x7E);
        int pos_y = *(int16_t*)((uint8_t*)node + 0x80);
        int map_width  = (int)(short)player_slot[0x10];
        int map_height = (int)*(short*)((uint8_t*)player_slot + 0x42);

        uint8_t routed = this->RouteTrainAtEdge(
            prev, node, pos_x, pos_y, map_width, map_height);

        if (routed) {
            prev = node;
            node = next_node;
            continue;
        }

        /* === Movement-steering: advance one tile === */
        uint16_t move_dir = *(uint16_t*)((uint8_t*)node + 0x76);
        int new_x = pos_x;
        int new_y = pos_y;
        int tile_row_size = map_width;
        uint8_t* tile_data_base = (uint8_t*)(uintptr_t)player_slot[0x11];

        if (move_dir == 0x5A) {
            /* Moving right */
            if (*(uint8_t*)(tile_row_size * pos_y + pos_x + 1 + tile_data_base) == 0x05) {
                new_x = pos_x + 1;
            } else {
                new_y = pos_y - 1;
                if (new_y >= 0 && *(uint8_t*)(tile_row_size * new_y + pos_x + 1 + tile_data_base) == 0x05) {
                    new_x = pos_x + 1;
                } else {
                    new_y = pos_y + 1;
                    if (new_y < map_height && *(uint8_t*)(tile_row_size * new_y + pos_x + 1 + tile_data_base) == 0x05) {
                        new_x = pos_x + 1;
                    } else if (*(uint8_t*)(tile_row_size * (pos_y - 1) + pos_x + tile_data_base) == 0x05 && pos_y >= 1) {
                        new_y = pos_y - 1;
                    } else if (new_y < map_height && *(uint8_t*)(tile_row_size * new_y + pos_x + tile_data_base) == 0x05) {
                        *(uint16_t*)((uint8_t*)node + 0x76) = 0xB4;
                    } else {
                        new_y = pos_y;
                    }
                }
            }
        } else if (move_dir == 0) {
            /* Moving up */
            new_y = pos_y - 1;
            if (new_y < 0) new_y = 0;
            if (*(uint8_t*)(tile_row_size * new_y + pos_x + tile_data_base) != 0x05) {
                if (*(uint8_t*)(tile_row_size * new_y + pos_x + 1 + tile_data_base) == 0x05 && pos_x < map_width - 1)
                    new_x = pos_x + 1;
                else if (*(uint8_t*)(tile_row_size * new_y + pos_x - 1 + tile_data_base) == 0x05 && pos_x > 0)
                    new_x = pos_x - 1;
                else if (*(uint8_t*)(tile_row_size * pos_y + pos_x - 1 + tile_data_base) == 0x05 && pos_x > 0) {
                    new_x = pos_x - 1; new_y = pos_y;
                } else if (*(uint8_t*)(tile_row_size * pos_y + pos_x + 1 + tile_data_base) == 0x05 && pos_x < map_width - 1) {
                    new_x = pos_x + 1; new_y = pos_y;
                    *(uint16_t*)((uint8_t*)node + 0x76) = 0x5A;
                } else { new_y = pos_y; }
            }
        } else if (move_dir == 0xB4) {
            /* Moving down */
            new_y = pos_y + 1;
            if (new_y >= map_height) new_y = map_height - 1;
            if (*(uint8_t*)(tile_row_size * new_y + pos_x + tile_data_base) != 0x05) {
                if (*(uint8_t*)(tile_row_size * new_y + pos_x + 1 + tile_data_base) == 0x05 && pos_x < map_width - 1)
                    new_x = pos_x + 1;
                else if (*(uint8_t*)(tile_row_size * new_y + pos_x - 1 + tile_data_base) == 0x05 && pos_x > 0)
                    new_x = pos_x - 1;
                else if (*(uint8_t*)(tile_row_size * pos_y + pos_x - 1 + tile_data_base) == 0x05 && pos_x > 0) {
                    new_x = pos_x - 1; new_y = pos_y;
                } else if (*(uint8_t*)(tile_row_size * pos_y + pos_x + 1 + tile_data_base) == 0x05 && pos_x < map_width - 1) {
                    new_x = pos_x + 1; new_y = pos_y;
                    *(uint16_t*)((uint8_t*)node + 0x76) = 0x10E;
                } else { new_y = pos_y; }
            }
        } else if (move_dir == 0x10E) {
            /* Moving left */
            if (*(uint8_t*)(tile_row_size * pos_y + pos_x - 1 + tile_data_base) == 0x05) {
                new_x = pos_x - 1;
            } else {
                new_y = pos_y - 1;
                if (new_y >= 0 && *(uint8_t*)(tile_row_size * new_y + pos_x - 1 + tile_data_base) == 0x05) {
                    new_x = pos_x - 1;
                } else {
                    new_y = pos_y + 1;
                    if (new_y < map_height && *(uint8_t*)(tile_row_size * new_y + pos_x - 1 + tile_data_base) == 0x05) {
                        new_x = pos_x - 1;
                    } else if (*(uint8_t*)(tile_row_size * (pos_y - 1) + pos_x + tile_data_base) == 0x05 && pos_y >= 1) {
                        new_y = pos_y - 1;
                    } else if (new_y < map_height && *(uint8_t*)(tile_row_size * new_y + pos_x + tile_data_base) == 0x05) {
                        *(uint16_t*)((uint8_t*)node + 0x76) = 0;
                    } else { new_y = pos_y; }
                }
            }
        }

        /* === Stuck detection === */
        if (new_x == pos_x && new_y == pos_y) {
            uint32_t rand_val = CRT_rand();
            int r = (int)(rand_val / 0x1FFF);
            switch (r % 4) {
            case 0: *(uint16_t*)((uint8_t*)node + 0x76) = 0x5A;  break;
            case 1: *(uint16_t*)((uint8_t*)node + 0x76) = 0x10E; break;
            case 2: *(uint16_t*)((uint8_t*)node + 0x76) = 0;     break;
            case 3: *(uint16_t*)((uint8_t*)node + 0x76) = 0xB4;  break;
            }
            *(uint16_t*)((uint8_t*)node + 0x84) = 0xFFFF;
            *(uint16_t*)((uint8_t*)node + 0x86) = 0xFFFF;
            *(uint8_t*)((uint8_t*)node + 0x82) = 0;
        } else {
            /* Update position */
            *(int16_t*)((uint8_t*)node + 0x7E) = (int16_t)new_x;
            *(int16_t*)((uint8_t*)node + 0x80) = (int16_t)new_y;
            *(uint8_t*)((uint8_t*)node + 0x82) += 1;

            /* Check stuck counter loop */
            uint32_t rand_val = CRT_rand();
            int threshold = (int)(rand_val / 0x1999) + 3;
            if (threshold < (int)*(uint8_t*)((uint8_t*)node + 0x82)) {
                if (*(int16_t*)((uint8_t*)node + 0x7E) == *(int16_t*)((uint8_t*)node + 0x84) &&
                    *(int16_t*)((uint8_t*)node + 0x80) == *(int16_t*)((uint8_t*)node + 0x86)) {
                    uint32_t rv = CRT_rand();
                    int rr = (int)(rv / 0x1FFF);
                    switch (rr % 4) {
                    case 0: *(uint16_t*)((uint8_t*)node + 0x76) = 0x5A;  break;
                    case 1: *(uint16_t*)((uint8_t*)node + 0x76) = 0x10E; break;
                    case 2: *(uint16_t*)((uint8_t*)node + 0x76) = 0;     break;
                    case 3: *(uint16_t*)((uint8_t*)node + 0x76) = 0xB4;  break;
                    }
                    *(uint8_t*)((uint8_t*)node + 0x82) = 0;
                    *(uint16_t*)((uint8_t*)node + 0x84) = 0xFFFF;
                    *(uint16_t*)((uint8_t*)node + 0x86) = 0xFFFF;
                }
                *(uint8_t*)((uint8_t*)node + 0x82) = 0;
                *(uint32_t*)((uint8_t*)node + 0x84) = *(uint32_t*)((uint8_t*)node + 0x7E);
            }

            /* === Build and send type-0x3F6 position update message === */
            {
                /* Allocate message buffer */
                uint16_t* buf = (uint16_t*)operator_new(0x2000);
                if (buf) {
                    buf[0] = 0x3F6;
                    *(uint8_t*)(buf + 1) = *(uint8_t*)((uint8_t*)node + 0x7C);
                    *(uint8_t*)((uint8_t*)buf + 4) = 0;
                    *(uint16_t*)((uint8_t*)buf + 6) = 1;
                    uint8_t c1 = *(uint8_t*)((uint8_t*)node + 0x78);
                    uint8_t c2 = *(uint8_t*)((uint8_t*)node + 0x7C);
                    *(uint32_t*)((uint8_t*)buf + 9) =
                        ((uint32_t)new_x << 16) | *(uint16_t*)((uint8_t*)node + 0x7A);
                    *(uint32_t*)((uint8_t*)buf + 0x0D) =
                        (c2 << 24) | (c1 << 16) | (uint16_t)new_y;

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
    uint8_t owner = *(uint8_t*)((uint8_t*)train + 0x7C);

    /* RIGHT edge (pos_x >= map_width - 1) */
    if (pos_x >= map_width - 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0xB4, owner);
        if ((char)conn != 0) {
            /* Track connection to neighbor exists — route the train */
            if (prev_node == NULL) {
                this->sprite_list_3 = *(void**)((uint8_t*)train + 0x70);
            } else {
                *(void**)((uint8_t*)prev_node + 0x70) = *(void**)((uint8_t*)train + 0x70);
            }
            *(void**)((uint8_t*)train + 0x70) = NULL;

            int target_town = (int)owner +
                *(int32_t*)((uint8_t*)g_netman + 0x0C);
            int* player_slot = NULL;
            if (target_town >= 0) {
                player_slot = (int*)((uint8_t*)g_netman + target_town * 0x4C + 0x518);
            }
            if (player_slot && *player_slot != 0 &&
                *(char*)((uint8_t*)player_slot + 4) != 0) {
                return this->MoveToNeighborTown(*player_slot, train, 0xB4);
            }
            this->AddTrainCar(train, 0xB4, target_town);
            return 1;
        }
        *(uint16_t*)((uint8_t*)train + 0x76) = 0; /* Bounce left */
    }
    /* LEFT edge (pos_x <= 0) */
    else if (pos_x < 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0, owner);
        if ((char)conn != 0) {
            if (prev_node == NULL) {
                this->sprite_list_3 = *(void**)((uint8_t*)train + 0x70);
            } else {
                *(void**)((uint8_t*)prev_node + 0x70) = *(void**)((uint8_t*)train + 0x70);
            }
            *(void**)((uint8_t*)train + 0x70) = NULL;

            int target_town = (int)owner -
                *(int32_t*)((uint8_t*)g_netman + 0x0C);
            int* player_slot = NULL;
            if (target_town >= 0) {
                player_slot = (int*)((uint8_t*)g_netman + target_town * 0x4C + 0x518);
            }
            if (player_slot && *player_slot != 0) {
                return this->MoveToNeighborTown(*player_slot, train, 0);
            }
            this->AddTrainCar(train, 0, target_town);
            return 1;
        }
        *(uint16_t*)((uint8_t*)train + 0x76) = 0xB4; /* Bounce right */
    }
    /* TOP edge (pos_y <= 0) */
    else if (pos_y < 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0x10E, owner);
        if ((char)conn != 0) {
            if (prev_node == NULL) {
                this->sprite_list_3 = *(void**)((uint8_t*)train + 0x70);
            } else {
                *(void**)((uint8_t*)prev_node + 0x70) = *(void**)((uint8_t*)train + 0x70);
            }
            *(void**)((uint8_t*)train + 0x70) = NULL;

            int target_town = (int)owner - 1;
            int* player_slot = NULL;
            if (target_town >= 0) {
                player_slot = (int*)((uint8_t*)g_netman + 0x518 + target_town * 0x4C);
            }
            if (player_slot && *player_slot != 0) {
                return this->MoveToNeighborTown(*player_slot, train, 0x10E);
            }
            this->AddTrainCar(train, 0x10E, target_town);
            return 1;
        }
        *(uint16_t*)((uint8_t*)train + 0x76) = 0x5A; /* Bounce down */
    }
    /* BOTTOM edge (pos_y >= map_height - 1) */
    else if (pos_y >= map_height - 1) {
        int conn = NETMAN_CheckTrackConnection(g_netman, 0x5A, owner);
        if ((char)conn != 0) {
            if (prev_node == NULL) {
                this->sprite_list_3 = *(void**)((uint8_t*)train + 0x70);
            } else {
                *(void**)((uint8_t*)prev_node + 0x70) = *(void**)((uint8_t*)train + 0x70);
            }
            *(void**)((uint8_t*)train + 0x70) = NULL;

            int* player_ptr = (int*)((uint8_t*)g_netman + 0x518 + (owner + 1) * 0x4C);
            if (*player_ptr != 0) {
                return this->MoveToNeighborTown(*player_ptr, train, 0x5A);
            }
            this->AddTrainCar(train, 0x5A, owner + 1);
            return 1;
        }
        *(uint16_t*)((uint8_t*)train + 0x76) = 0x10E; /* Bounce up */
    } else {
        /* Not at any edge — continue normal movement */
        return 0;
    }

    /* Bounce (no track connection): clear stuck counter */
    *(uint8_t*)((uint8_t*)train + 0x82) = 0;
    *(uint16_t*)((uint8_t*)train + 0x84) = 0xFFFF;
    *(uint16_t*)((uint8_t*)train + 0x86) = 0xFFFF;
    return 0xFFFFFF01u;
}


/* ================================================================== */
/* TrainSubsystem::MoveToNeighborTown                                  */
/* Address: 0x43AE20                                                    */
/* Size: 1016 bytes                                                     */
/* ================================================================== */
uint32_t TrainSubsystem::MoveToNeighborTown(int to_player, void* car, int direction)
{
    int local_player = *(int*)((uint8_t*)g_netman + 0x7D0);
    int target_idx = NETMAN_FindPlayerIndex(g_netman, to_player);

    if (local_player == target_idx) {
        /* === Local player: mirror direction, append to sprite_list_2 === */

        /* Mirror direction */
        if (direction < 0x5B) {
            if (direction == 0x5A)      direction = 0x10E;
            else if (direction == 0)    direction = 0xB4;
        } else {
            if (direction == 0xB4)      direction = 0;
            else if (direction == 0x10E) direction = 0x5A;
        }
        *(uint16_t*)((uint8_t*)car + 0x74) = (uint16_t)direction;

        /* Append to sprite_list_2 */
        if (this->sprite_list_2 == NULL) {
            *(void**)((uint8_t*)car + 0x70) = NULL;
            this->sprite_list_2 = car;
        } else {
            uint8_t* tail = (uint8_t*)this->sprite_list_2;
            while (*(uint8_t**)(tail + 0x70) != 0) tail = *(uint8_t**)(tail + 0x70);
            *(void**)((uint8_t*)car + 0x70) = NULL;
            *(void**)(tail + 0x70) = car;
        }
        Train_RemoveAllTracks(this);
        return 1;
    }

    /* === Remote player: serialize into 0xB1C-byte MSG_CONN_SETUP === */
    uint8_t* buf = (uint8_t*)operator_new(0xB1C);
    if (buf == NULL) return 0;

    memset(buf, 0, 0xB1C);

    *(uint16_t*)buf = 0x3F2;                        /* message type */
    *(uint16_t*)(buf + 4) = (uint16_t)direction;     /* direction */
    *(uint16_t*)(buf + 6) = *(uint16_t*)((uint8_t*)car + 0x7A);  /* resource ID */
    *(uint8_t*)(buf + 10) = *(uint8_t*)((uint8_t*)car + 0x78);   /* type */
    *(uint16_t*)(buf + 8) = *(uint16_t*)((uint8_t*)car + 0x58);  /* speed parameter */

    /* Copy parent data */
    *(int32_t*)(buf + 0x0C) = *(int32_t*)((uint8_t*)car + 8);

    /* Copy player name (up to 10 bytes from +0x7C) */
    {
        const char* name_src = (const char*)((uint8_t*)car + 0x7C);
        char* name_dst = (char*)(buf + 0xB10);
        for (int i = 0; i < 10 && name_src[i] != 0; i++) {
            name_dst[i] = name_src[i];
        }
    }

    /* Process carriages */
    uint8_t carriage_count = 0;
    if (*(short*)((uint8_t*)car + 0x0C) != 0) {
        int* carriage_ptr = (int*)((uint8_t*)car + 0x14);
        for (int i = 0; i < *(uint16_t*)((uint8_t*)car + 0x0C); i++) {
            if (carriage_ptr[i] == 0) continue;

            int res_id = VehicleEditor_GetResourceId((void*)(uintptr_t)carriage_ptr[i]);
            *(int32_t*)(buf + 6 + carriage_count * 0xEA) = res_id;
            *(int32_t*)(buf + 6 + carriage_count * 0xEA + 4) =
                *(int32_t*)((uint8_t*)(uintptr_t)carriage_ptr[i] + 0x42C);

            void* dplay_data = VehicleEditor_GetDPlayData((void*)(uintptr_t)carriage_ptr[i]);
            if (dplay_data) {
                *(uint8_t*)(buf + 6 + carriage_count * 0xEA + 8) = 1;
                memcpy(buf + 6 + carriage_count * 0xEA + 9, dplay_data, 0x39C);
                /* 0x39C = 0xE7 * 4 bytes */
            } else {
                *(uint8_t*)(buf + 6 + carriage_count * 0xEA + 8) = 0;
            }
            carriage_count++;
        }
    }
    *(uint8_t*)(buf + 0x14) = carriage_count;

    /* Send the message */
    int send_result = WIN32_SendNetworkData(g_dplay_peer, to_player, buf, 0xB1C, 1);

    if (send_result == 0) {
        /* Send failed — take local control instead */
        GLOBAL_free(buf);

        /* Mirror direction */
        uint16_t dir = *(uint16_t*)((uint8_t*)car + 0x74);
        if (dir < 0x5B) {
            if (dir == 0x5A)      dir = 0x10E;
            else if (dir == 0)    dir = 0xB4;
        } else {
            if (dir == 0xB4)      dir = 0;
            else if (dir == 0x10E) dir = 0x5A;
        }
        *(uint16_t*)((uint8_t*)car + 0x74) = dir;

        /* Append to sprite_list_2 */
        if (this->sprite_list_2 == NULL) {
            *(void**)((uint8_t*)car + 0x70) = NULL;
            this->sprite_list_2 = car;
        } else {
            uint8_t* tail = (uint8_t*)this->sprite_list_2;
            while (*(uint8_t**)(tail + 0x70) != 0) tail = *(uint8_t**)(tail + 0x70);
            *(void**)((uint8_t*)car + 0x70) = NULL;
            *(void**)(tail + 0x70) = car;
        }

        /* Notify UI for all cars in sprite_list_2 */
        {
            uint8_t* c = (uint8_t*)this->sprite_list_2;
            while (c != 0) {
                NetworkMsg* msg = AllocateNetworkMessage();
                if (msg) { msg->data = NULL; msg->next = NULL;
                           msg->type = 0x11;
                           msg->data = this->sprite_list_2; }
                ((Building*)this->sprite_list_2)->occupation_level = 0;

                uint8_t* cur = (uint8_t*)this->sprite_list_2;
                *(uint8_t*)(cur + 0x7C) = *(uint8_t*)((uint8_t*)g_netman + 0x7D0);
                *(void**)(cur + 0x70) = NULL;
                *(uint8_t*)(cur + 0x88) = 0;
                this->sprite_list_2 = *(void**)(cur + 0x70);

                NETMAN_QueueMessage(msg);
                c = (uint8_t*)this->sprite_list_2;
            }
        }
    } else {
        /* Send succeeded — car transferred to remote player */

        /* Mirror direction for local tracking */
        {
            uint16_t dir = *(uint16_t*)((uint8_t*)car + 0x74);
            if (dir < 0x5B) {
                if (dir == 0x5A)      dir = 0x10E;
                else if (dir == 0)    dir = 0xB4;
            } else {
                if (dir == 0xB4)      dir = 0;
                else if (dir == 0x10E) dir = 0x5A;
            }
            *(uint16_t*)((uint8_t*)car + 0x74) = dir;
        }

        /* Set owner to target player */
        int target_idx2 = NETMAN_FindPlayerIndex(g_netman, to_player);
        *(uint8_t*)((uint8_t*)car + 0x7C) = (uint8_t)target_idx2;
        *(uint8_t*)((uint8_t*)car + 0x8A) = 0;

        /* Append to sprite_list_2 with owner tracking */
        if (this->sprite_list_2 == NULL) {
            *(void**)((uint8_t*)car + 0x70) = NULL;
            this->sprite_list_2 = car;
        } else {
            uint8_t* tail = (uint8_t*)this->sprite_list_2;
            while (*(uint8_t**)(tail + 0x70) != 0) tail = *(uint8_t**)(tail + 0x70);
            *(void**)((uint8_t*)car + 0x70) = NULL;
            *(void**)(tail + 0x70) = car;
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
    NetworkMsg* net_msg = (NetworkMsg*)msg;
    void*       car     = net_msg->data;

    if (car == NULL) return;

    if (*(int32_t*)((uint8_t*)g_netman + 0x7C4) == 1) {
        /* === Scenario mode: append car to sprite_list_1 === */
        *(uint16_t*)((uint8_t*)car + 0x74) = 32000; /* max timeout */

        if (*(int32_t*)((uint8_t*)car + 4) == 1) {
            /* Remove message: destroy car */
            void** vt = *(void***)car;
            ((void (__thiscall*)(void*, byte))vt[0])(car, 1);
            net_msg->data = NULL;
            return;
        }

        /* Append to end of sprite_list_1 */
        if (this->sprite_list_1 == NULL) {
            *(void**)((uint8_t*)car + 0x70) = NULL;
            this->sprite_list_1 = car;
        } else {
            uint8_t* tail = (uint8_t*)this->sprite_list_1;
            while (*(uint8_t**)(tail + 0x70) != 0) tail = *(uint8_t**)(tail + 0x70);
            *(void**)((uint8_t*)car + 0x70) = NULL;
            *(void**)(tail + 0x70) = car;
        }

        if (g_demo_mode == 1) {
            /* Demo mode: remove all existing cars from sprite_list_1 */
            uint8_t* c = (uint8_t*)this->sprite_list_1;
            while (c != 0) {
                NetworkMsg* qmsg = AllocateNetworkMessage();
                if (qmsg) { qmsg->data = NULL; qmsg->next = NULL;
                           qmsg->type = 0x0F;
                           qmsg->data = this->sprite_list_1; }
                *(uint8_t*)((uint8_t*)this->sprite_list_1 + 0x88) = 0;
                this->sprite_list_1 = *(void**)((uint8_t*)this->sprite_list_1 + 0x70);
                if (qmsg && qmsg->data) {
                    *(void**)((uint8_t*)qmsg->data + 0x70) = NULL;
                }
                NETMAN_QueueMessage(qmsg);
                c = (uint8_t*)this->sprite_list_1;
            }
        }
        return;
    }

    /* === Free-play mode === */
    if (car && *(int32_t*)((uint8_t*)car + 4) == 1) {
        void** vt = *(void***)car;
        ((void (__thiscall*)(void*, byte))vt[0])(car, 1);
        net_msg->data = NULL;
        return;
    }

    if (g_demo_mode == 1 || this->byte_flags != 0) {
        /* Demo mode or flag set — remove all cars */
        uint8_t* c = (uint8_t*)this->sprite_list_1;
        while (c != 0) {
            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) { qmsg->data = NULL; qmsg->next = NULL;
                       qmsg->type = 0x0F;
                       qmsg->data = this->sprite_list_1; }
            *(uint8_t*)((uint8_t*)this->sprite_list_1 + 0x88) = 0;
            this->sprite_list_1 = *(void**)((uint8_t*)this->sprite_list_1 + 0x70);
            if (qmsg && qmsg->data) {
                *(void**)((uint8_t*)qmsg->data + 0x70) = NULL;
            }
            NETMAN_QueueMessage(qmsg);
            c = (uint8_t*)this->sprite_list_1;
        }
        return;
    }

    /* Create DirectPlay session and connect to train server */
    if (g_dplay_peer == NULL) {
        void* new_peer = operator_new(0x160c);
        if (new_peer == NULL) new_peer = NULL;
        else new_peer = DirectPlay_CreatePeer(new_peer, this->context_id_a, 0);
        g_dplay_peer = new_peer;

        if (g_dplay_peer) {
            *(int32_t*)((uint8_t*)g_dplay_peer + 0x940) = 0;
            *(uint8_t*)((uint8_t*)g_dplay_peer + 0x944) = 0;
            *(int32_t*)((uint8_t*)g_dplay_peer + 0x938) = this->context_id_b;
        }
    }

    if (g_dplay_peer && *(uint8_t*)((uint8_t*)g_dplay_peer + 0xd50) != 0) {
        Train_SendPlayerInfo(this);
        return;
    }

    if (g_dplay_peer == NULL) return;

    /* Host a new session */
    DirectPlay_Close(g_dplay_peer);
    DirectPlay_HostSession(g_dplay_peer, 0, 1, 0, 0);
    Train_StartMultiplayer();

    if (*(int*)((uint8_t*)g_dplay_peer + 0x1588) != 0) {
        /* Get server name from config and connect */
        char server_name[0x200];
        Config_GetIniString(g_config_ini, "Configuration", "ServerName",
                            "LEGO International Train Server",
                            server_name, 0x200);

        DirectPlay_ConnectToSession(g_dplay_peer,
                                     (char*)((uint8_t*)g_player_config + 6),
                                     server_name, NULL);

        if (*(uint8_t*)((uint8_t*)g_dplay_peer + 0xd50) != 0) {
            Train_SendPlayerInfo(this);
            return;
        }

        /* Retry after close+re-host */
        DirectPlay_Close(g_dplay_peer);
        Sleep(1000);
        DirectPlay_HostSession(g_dplay_peer, 0, 1, 0, 0);
        Train_StartMultiplayer();
        DirectPlay_ConnectToSession(g_dplay_peer,
                                     (char*)((uint8_t*)g_player_config + 6),
                                     server_name, NULL);
    }

    if (g_dplay_peer && *(uint8_t*)((uint8_t*)g_dplay_peer + 0xd50) != 0) {
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

    /* Create and register a DPLAY player for this session */
    {
        void* player = DPLAY_CreatePlayer(operator_new(0x39C));
        if (player) {
            char name_buf[0x50];
            FormatResourceString(&g_resmgr, 0xDF, name_buf, 0x50);
            /* Copy name into player struct at appropriate offset */
            memcpy((uint8_t*)player + 0x43, name_buf, 0x50);

            DPLAY_InitPlayer(player, 5, 1, 5, 0x94, 99, 0x48, 0x48);
            *(uint8_t*)((uint8_t*)player + 0x41) = 0xFF;

            /* Copy player name from g_player_config + 6 */
            memcpy((uint32_t*)((uint8_t*)player + 0x10),
                   (uint32_t*)((uint8_t*)g_player_config + 6), 0x14);
            /* Copy "LEGO LOCO" as session name at +0x25 */
            memcpy((uint8_t*)player + 0x25, "LEGO LOCO", 10);

            NET_RegisterPlayer(g_dplay, player, 1, 0);

            void** vt = *(void***)player;
            ((void (__thiscall*)(void*, byte))vt[0])(player, 1);
        }
    }

    /* Remove all existing cars from sprite_list_1 to join fresh */
    {
        uint8_t* c = (uint8_t*)this->sprite_list_1;
        while (c != 0) {
            NetworkMsg* qmsg = AllocateNetworkMessage();
            if (qmsg) { qmsg->data = NULL; qmsg->next = NULL;
                       qmsg->type = 0x0F;
                       qmsg->data = this->sprite_list_1; }
            *(uint8_t*)((uint8_t*)this->sprite_list_1 + 0x88) = 0;
            this->sprite_list_1 = *(void**)((uint8_t*)this->sprite_list_1 + 0x70);
            if (qmsg && qmsg->data) {
                *(void**)((uint8_t*)qmsg->data + 0x70) = NULL;
            }
            NETMAN_QueueMessage(qmsg);
            c = (uint8_t*)this->sprite_list_1;
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
        *(uint8_t*)((uint8_t*)this->sprite_list_1 + 0x88) = 0;
        this->sprite_list_1 =
            *(void**)((uint8_t*)this->sprite_list_1 + 0x70);
        *(void**)((uint8_t*)msg->data + 0x70) = NULL;
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
/* ================================================================== */
void Train_QueueMessage(void* train, TrainMessage* msg)
{
    if (train != nullptr && msg != nullptr) {
        static_cast<TrainSubsystem*>(train)->QueueMessage(msg);
    }
}
