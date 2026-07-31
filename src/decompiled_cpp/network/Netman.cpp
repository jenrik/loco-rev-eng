/**
 * Netman.cpp - Network Manager implementation
 * Lego Loco (loco.exe, 1998, MSVC x86)
 *
 * Status: TRANSCRIBED
 *
 * Covers all Netman class methods and standalone network helper functions.
 *
 * Anti-patterns removed (TRANSCRIBED cleanup):
 *   - vtable_delete helper → C++ 'delete' operator
 *   - Raw vtable dispatch → virtual method calls (CarObject, SpriteObject)
 *   - (uint8_t*)ptr+offset → named InboundTrainNode field access
 *   - void* typed fields → Building*, InboundTrainNode*, int32_t
 *   - int32_t parameters replaced with Netman and InboundTrainNode pointers
 *   - SendGameStart off-by-one: verified 1-based indexing (ci=1..cc)
 *   - __thiscall/__fastcall removed from DPlayManager.h dependency
 *
 * Functions implemented (35 class methods + 9 standalone):
 *   1. 0x43D0A0  ctor          2. 0x43D110  ~dtor
 *   3. 0x43D130  Init          4. 0x43D210  GetPlayerCount
 *   5. 0x43D230  FindPlayerIndex   6. 0x43D2B0  SetGameMode
 *   7. 0x43D350  SendMapData   8. 0x43D520  SendBuildingData
 *   9. 0x43D620  UpdatePlayerInfo  10. 0x43D6C0 ProcessPlayerData
 *  11. 0x43D820  LoadScenario  12. 0x43DC30 Cleanup
 *  13. 0x43DDF0  CheckRightEdge   14. 0x43DE00 CheckLeftEdge
 *  15. 0x43DE10  CheckUpEdge   16. 0x43DE20 CheckDownEdge
 *  17. 0x43DE30  CheckTrackConnection
 *  18. 0x43DED0  SendPlayerName   19. 0x43E010 ReceivePlayerName
 *  20. 0x43E1D0  SendChatMessage  21. 0x43E2E0 ReceiveChatMessage
 *  22. 0x43E370  SendGameStart    23. 0x43E560 ReceiveGameStart
 *  24. 0x43E690  SendSignalChange
 *  25. 0x43EFA0  ResetNetworkState 26. 0x43F070 StopSession
 *  27. 0x43F0C0  Update         28. 0x43F2B0 ProcessMessage
 *  29. 0x43F7B0  Shutdown       30. 0x43F880 HandlePlayerJoin
 *  31. 0x43F940  RemoveInboundTrain 32. 0x43FB50 HandlePlayerLeave
 *  33. 0x43FC50  SyncGameState  34. 0x43FE30 SendLayoutSelect
 *  35. 0x440150  SendFileTransfer   36. 0x440410 ReceiveAck
 *  37. 0x4404C0  RemovePingEntry    38. 0x440610 ReceivePing
 *  39. 0x440750  UpdateLatency  40. 0x440820 CheckTimeout
 *  41. 0x4408B0  HandleTimeout  42. 0x440A50 SerializePlayerData
 *  43. 0x440A80  DeserializePlayerData
 */

#include "Netman.h"
#include "DPlayManager.h"
#include "../game/Building.h"
#include "../game/PlayerConfig.h"
#include "../core/VehicleEditor.h"
#include "../core/Entity.h"
#include "../shared/Collection.h"
#include "../core/Entity.h"
#include "../shared/Collection.h"
#ifndef _WIN32
#include "../game/Train.h"
#include "../../sdl3_shims/host_test_events.h"
#include "../../sdl3_shims/sdl3_net_runtime.h"
#endif
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <new>
#include <string>
#include <vector>

/* ================================================================== */
/* Local externs (not in Netman.h to avoid circular includes)          */
/* ================================================================== */

extern void* _g_train;              /* 0x4FD3A4 */
extern void* g_network_queue;       /* 0x4FD39C */
extern void* g_train_resources;    /* 0x4FD394 */
extern void* _g_netman_data;        /* 0x4FD3A8 */

/* String constants */
extern const char STR_LEGO_LOCO[];  /* 0x47E1C0 — "LEGO LOCO" */
extern const char STR_REMOVED[];    /* 0x47EB54 — "NETMAN: inbound train removed\n" */

/* Path format strings */
extern const char FMT_LAYOUT_PATH[];
extern const char FMT_LAY_FILE[];

/* Error strings */
extern const char ERR_NO_STREAM[];
extern const char ERR_BAD_STREAM[];
extern const char ERR_NO_BUF[];

/* Additional externs for Win32 API (C linkage) */
extern "C" {
    extern void* __stdcall GetProcessHeap(void);
    extern int32_t __stdcall HeapFree(void*, uint32_t, void*);
}

/* ================================================================== */
/* NetworkObject — base class with virtual destructor for safe delete  */
/*                                                                      */
/* Objects pushed into m_buildingList, m_vehicleList, and other linked  */
/* lists all have vtables with scalar deleting destructors at vtable[0].*/
/* This base class provides a typed target for C++ 'delete' without     */
/* requiring the full class hierarchy to be resolved at TRANSCRIBED.    */
/* ================================================================== */
class NetworkObject {
public:
    virtual ~NetworkObject() {}
};

/* ================================================================== */
/* Helper: inline memcpy (REP MOVSD/MOVSB pattern)                     */
/* ================================================================== */
static void inline_memcpy(void* dst, const void* src, int32_t len)
{
    int32_t dw = len >> 2, rm = len & 3;
    for (int32_t i = 0; i < dw; i++)
        reinterpret_cast<uint32_t*>(dst)[i] =
            reinterpret_cast<const uint32_t*>(src)[i];
    for (int32_t i = 0; i < rm; i++)
        reinterpret_cast<uint8_t*>(dst)[dw * 4 + i] =
            reinterpret_cast<const uint8_t*>(src)[dw * 4 + i];
}

static TrainMessage* allocate_train_message()
{
    void* storage = operator_new(sizeof(TrainMessage));
    return storage == nullptr
        ? nullptr
        : ::new (storage) TrainMessage{};
}

/* ================================================================== */
/* Helper: delete object with virtual destructor via NetworkObject*     */
/*                                                                      */
/* The binary calls vtable[0](obj,1) — the scalar deleting destructor.  */
/* C++ 'delete' on a pointer to a class with virtual destructor         */
/* generates the same code. NetworkObject provides the vtable anchor.   */
/* ================================================================== */
static inline void net_delete(void* obj)
{
    if (obj) delete static_cast<NetworkObject*>(obj);
}

static inline void net_delete(Vehicle* vehicle)
{
    if (vehicle == nullptr) return;
    vehicle->~Vehicle();
    GLOBAL_free(vehicle);
}

static TrainMessage* net_new_message()
{
    return allocate_train_message();
}

namespace {
void CopyPlayerSlotText(char* destination, std::size_t capacity,
                        const char* source)
{
    if (capacity == 0) return;
    std::strncpy(destination, source != nullptr ? source : "", capacity - 1);
    destination[capacity - 1] = '\0';
}
}

/** DPLAY_CopyPlayerData
 *  Address: 0x4426D0 */
void DPLAY_CopyPlayerData(void* destination, const void* compact_packet)
{
    if (destination == nullptr || compact_packet == nullptr) return;
    auto* slot = static_cast<PlayerSlot*>(destination);
    const auto* packet = static_cast<const uint8_t*>(compact_packet);
    std::memcpy(&slot->dpId, packet, sizeof(slot->dpId));
    slot->is_connected = packet[0x3A];
    CopyPlayerSlotText(slot->compact_name, sizeof(slot->compact_name),
                       reinterpret_cast<const char*>(packet + 0x0C));
    CopyPlayerSlotText(slot->layout_name, sizeof(slot->layout_name),
                       reinterpret_cast<const char*>(packet + 0x19));
    std::memcpy(&slot->player_id, packet + 4,
                sizeof(slot->player_id) + sizeof(slot->player_color));
    slot->flag_36 = packet[0x39];
    std::memcpy(&slot->version, packet + 8, sizeof(slot->version));
}

/** DPLAY_InitPlayerSlot
 *  Address: 0x442750 */
void DPLAY_InitPlayerSlot(void* destination, const void* source)
{
    if (destination == nullptr || source == nullptr) return;
    auto* output = static_cast<PlayerSlot*>(destination);
    const auto* input = static_cast<const PlayerSlot*>(source);
    output->dpId = input->dpId;
    output->is_connected = input->is_connected;
    CopyPlayerSlotText(output->compact_name, sizeof(output->compact_name),
                       input->compact_name);
    CopyPlayerSlotText(output->layout_name, sizeof(output->layout_name),
                       input->layout_name);
    output->player_id = input->player_id;
    output->player_color = input->player_color;
    output->flag_36 = input->flag_36;
    output->version = input->version;
}

#ifndef _WIN32
void* DPLAY_DecodePlayerSlots(const void* first_compact_slot)
{
    if (first_compact_slot == nullptr) return nullptr;
    auto* slots = static_cast<PlayerSlot*>(operator_new(sizeof(PlayerSlot) * 9));
    if (slots == nullptr) return nullptr;
    std::memset(slots, 0, sizeof(PlayerSlot) * 9);
    const auto* compact = static_cast<const uint8_t*>(first_compact_slot);
    for (int32_t index = 0; index < 9; ++index)
        DPLAY_CopyPlayerData(&slots[index], compact + index * 0x3C);
    return slots;
}
#endif

/** DPLAY_FreePlayerSlot
 *  Address: 0x4427D0 */
void DPLAY_FreePlayerSlot(void* compact_packet, const int32_t* source)
{
    if (compact_packet == nullptr || source == nullptr) return;
    auto* packet = static_cast<uint8_t*>(compact_packet);
    const auto* slot = reinterpret_cast<const PlayerSlot*>(source);
    std::memset(packet, 0, 0x3C);
    std::memcpy(packet, &slot->dpId, sizeof(slot->dpId));
    packet[0x3A] = slot->is_connected;
    CopyPlayerSlotText(reinterpret_cast<char*>(packet + 0x0C), 13,
                       slot->compact_name);
    CopyPlayerSlotText(reinterpret_cast<char*>(packet + 0x19), 32,
                       slot->layout_name);
    std::memcpy(packet + 4, &slot->player_id,
                sizeof(slot->player_id) + sizeof(slot->player_color));
    packet[0x39] = slot->flag_36;
    std::memcpy(packet + 8, &slot->version, sizeof(slot->version));
}

int32_t NETMAN_GetGameMode(const void* netman)
{
    return netman != nullptr
        ? static_cast<const Netman*>(netman)->m_gameMode : -1;
}

/* ================================================================== */
/* 1. Constructor - 0x43D0A0                                          */
/* ================================================================== */
Netman::Netman()
{
    this->Init(1);
    this->m_gameMode = 3;
    this->m_bFlag1 = 0;
    this->m_myDpId = 0;
    this->m_buildingList = NULL;
    this->m_vehicleList = NULL;
    this->m_field_7E4 = 0;
#ifndef _WIN32
    this->m_hostLastSerializedVehicle = nullptr;
#endif
    this->m_field_7E8 = 0;
    this->m_tickCounter = 0;
    this->m_sendTimer = 0;
    this->m_timeoutState = 0;
    this->m_visibility = 0xF;
    this->m_tickInterval = 0x960;
}

/* ================================================================== */
/* 2. Destructor - 0x43D110 (body only; scalar-deleting wrapper is     */
/*    compiler-generated at vtable[0])                                  */
/* ================================================================== */
Netman::~Netman()
{
    this->Cleanup();
}

/* ================================================================== */
/* 3. Init - 0x43D130                                                 */
/* ================================================================== */
void Netman::Init(uint8_t is_init)
{
    this->m_playerSlotCount = 9;
    this->m_bInit = 0;

    /* Copy "Default" layout name */
    {
        const char* s = "Default";
        char* d = this->m_layoutName;
        int32_t n;
        for (n = 0; s[n] != 0; n++) d[n] = s[n];
        d[n] = 0;
    }

    this->m_playerRows = 3;
    this->m_playerCols = 3;
    this->m_currentSlot = NULL;
    this->m_mySlotIndex = -1;

    for (int32_t i = 0; i < 9; i++) {
        PlayerSlot* sl = &this->m_slots[i];
        sl->dpId = 0;
        sl->is_connected = 0;
        sl->has_data = 0;
        sl->flag_36 = 0;
        if (is_init) {
            sl->msg_queue = NULL;
        } else {
            void* n = sl->msg_queue;
            while (n) {
                void* nx = *(void**)((uint8_t*)n + 0x10);
                GLOBAL_free(n);
                n = nx;
            }
            sl->msg_queue = NULL;
            if (sl->pixel_buffer) GLOBAL_free(sl->pixel_buffer);
        }
        sl->pixel_buffer = NULL;
        sl->player_id = g_player_id;
        sl->player_color = g_player_color;
        sl->data_size = 0;
        sl->pixel_width = 0;
        sl->pixel_height = 0;
        sl->version = 0;
    }
}

/* ================================================================== */
/* 4. GetPlayerCount - 0x43D210                                       */
/*                                                                     */
/* TODO: Ghidra verification needed. The cast of m_currentSlot         */
/* (PlayerSlot*) to int32_t produces a truncated pointer, not a count. */
/* The original binary likely counts connected slots.                  */
/* ================================================================== */
int32_t Netman::GetPlayerCount()
{
    return (this->m_gameMode == 2) ? (int32_t)(intptr_t)this->m_currentSlot : 0;
}

/* ================================================================== */
/* 5. FindPlayerIndex - 0x43D230                                      */
/* ================================================================== */
int32_t Netman::FindPlayerIndex(int32_t dpId)
{
    for (int32_t i = 0; i < 9; i++)
        if (this->m_slots[i].dpId == dpId) return i;
    return -1;
}

/* ================================================================== */
/* 7. SetGameMode - 0x43D2B0                                          */
/* ================================================================== */
void Netman::SetGameMode(int32_t newMode)
{
    if (newMode == this->m_gameMode) return;
    this->m_gameMode = newMode;
    switch (newMode) {
    case 0: case 3: break;
    case 1:
        this->m_timeout = 500;
        return;
    case 2:
        for (int32_t i = 0; i < 9; i++) {
            if (this->m_slots[i].dpId == this->m_myDpId) {
                this->m_mySlotIndex = i;
                this->m_currentSlot = &this->m_slots[i];
                break;
            }
        }
        this->m_timeout = 20;
        NETMAN_SendDisconnect(0);
        return;
    default:
        this->m_gameMode = 3;
        break;
    }
}

#ifndef _WIN32
namespace {
void CopyTransportPlayerName(PlayerSlot& slot, const char* name)
{
    const char* source = name ? name : "";
    std::strncpy(slot.layout_name, source, sizeof(slot.layout_name) - 1);
    slot.layout_name[sizeof(slot.layout_name) - 1] = '\0';
}
}

void Netman::HostAddTransportPlayer(int32_t playerId, const char* playerName)
{
    if (playerId < 1 || playerId > 9) return;
    PlayerSlot& slot = this->m_slots[playerId - 1];
    slot.dpId = playerId;
    slot.is_connected = 1;
    slot.has_data = 0;
    slot.flag_36 = 0;
    slot.version = 0;
    CopyTransportPlayerName(slot, playerName);
    if (playerId == this->m_myDpId) {
        this->m_mySlotIndex = playerId - 1;
        this->m_currentSlot = &slot;
    }
}

void Netman::HostBeginTransportSession(bool hosting, int32_t localPlayerId,
                                       const char* localPlayerName)
{
    this->Init(0);
    this->m_myDpId = localPlayerId;
    this->m_field_7D8 = hosting ? localPlayerId : 1;
    this->m_bInit = 1;
    this->m_bFlag1 = 1;
    this->m_gameMode = hosting ? 1 : 2;
    this->m_timeout = hosting ? 500 : 20;
    this->HostAddTransportPlayer(localPlayerId, localPlayerName);
}

void Netman::HostRemoveTransportPlayer(int32_t playerId)
{
    if (playerId < 1 || playerId > 9) return;
    PlayerSlot& slot = this->m_slots[playerId - 1];
    if (slot.dpId != playerId) return;
    slot.dpId = 0;
    slot.is_connected = 0;
    slot.has_data = 0;
    slot.flag_36 = 0;
    slot.layout_name[0] = '\0';
    slot.version = 0;
    if (playerId == this->m_myDpId) this->HostEndTransportSession();
}

bool Netman::HostClonePendingRouteForLoading()
{
    Vehicle* source = this->m_vehicleList;
    if (source == nullptr) return false;
    uint32_t before = 0;
    for (Vehicle* item = this->m_vehicleList; item != nullptr; item = item->next)
        ++before;
    this->SendSignalChange(source);
    uint32_t after = 0;
    for (Vehicle* item = this->m_vehicleList; item != nullptr; item = item->next)
        ++after;
    if (after > before) {
        loco::host_test::emit_netman_route_cloned(
            source->editor_count + 1, this->m_vehicleList->editor_count + 1,
            after);
        return true;
    }
    return false;
}

void Netman::HostEndTransportSession()
{
    this->Init(0);
    this->m_bFlag1 = 0;
    this->m_gameMode = 3;
    this->m_myDpId = 0;
    this->m_field_7D8 = 0;
}
#endif

/* ================================================================== */
/* 8. SendMapData - 0x43D350                                          */
/* ================================================================== */
void Netman::SendMapData(int32_t targetDpId)
{
    if (this->m_gameMode != 2) return;
    PlayerSlot* slot = this->m_currentSlot;
    if (!slot) return;
    slot->has_data = 1;

    void* surf = operator_new(0x20);
    if (surf != nullptr) surf = UIPANEL_CreateSurface(surf);
    TileMap_CreateOverlay(g_tilemap, surf, 0);
    if (surf == nullptr) return;

    const auto* surface_bytes = reinterpret_cast<const uint8_t*>(surf);
    uint16_t w = *reinterpret_cast<const uint16_t*>(surface_bytes + 8);
    uint16_t h = *reinterpret_cast<const uint16_t*>(surface_bytes + 0xC);
    int32_t ds = static_cast<int32_t>(w) * static_cast<int32_t>(h);
    void* px = *reinterpret_cast<void* const*>(surface_bytes + 0x18);

    uint8_t* pkt = static_cast<uint8_t*>(operator_new(ds + 0x28));
    *reinterpret_cast<uint16_t*>(pkt + 0) = PACKET_MAP_DATA;
    *reinterpret_cast<uint16_t*>(pkt + 6) = w;
    *reinterpret_cast<uint16_t*>(pkt + 8) = h;
    *reinterpret_cast<int32_t*>(pkt + 0x10) = ds;

    if (this->m_mySlotIndex >= 0) {
        PlayerSlot* ms = &this->m_slots[this->m_mySlotIndex];
        if (ms->pixel_buffer) GLOBAL_free(ms->pixel_buffer);
        ms->pixel_buffer = operator_new(ds);
        ms->data_size = ds;
        inline_memcpy(ms->pixel_buffer, px, ds);
        ms->version++;
        ms->pixel_width = w;
        ms->pixel_height = h;
        *reinterpret_cast<int32_t*>(pkt + 0xC) = ms->version;
    }
    inline_memcpy(pkt + 0x14, px, ds);

    TrainMessage* m = allocate_train_message();
    if (m == nullptr) {
        net_delete(surf);
        GLOBAL_free(pkt);
        return;
    }
    m->type = 6;
    m->data_len = ds + 0x19;
    m->data_ptr = pkt;
    m->target_dpId = targetDpId;
    m->flags = 1;
    Train_QueueMessage(_g_train, m);

    /* Delete surf (has vtable with virtual destructor) */
    net_delete(surf);
}

/* ================================================================== */
/* 9. SendBuildingData - 0x43D520                                     */
/* ================================================================== */
void Netman::SendBuildingData(int32_t targetDpId)
{
    if (this->m_gameMode != 2) return;
    PlayerSlot* slot = this->m_currentSlot;
    if (!slot) return;

    if (slot->pixel_buffer) {
        int32_t ds = slot->data_size;
        uint16_t w = slot->pixel_width, h = slot->pixel_height;
        uint8_t* pkt = static_cast<uint8_t*>(operator_new(ds + 0x28));
        if (pkt == nullptr) return;
        *reinterpret_cast<uint16_t*>(pkt + 0) = PACKET_MAP_DATA;
        *reinterpret_cast<uint16_t*>(pkt + 6) = w;
        *reinterpret_cast<uint16_t*>(pkt + 8) = h;
        *reinterpret_cast<int32_t*>(pkt + 0x10) = ds;
        *reinterpret_cast<int32_t*>(pkt + 0xC) = slot->version;
        inline_memcpy(pkt + 0x14, slot->pixel_buffer, ds);
        TrainMessage* m = allocate_train_message();
        if (m == nullptr) { GLOBAL_free(pkt); return; }
        m->type = 6;
        m->data_len = ds + 0x19;
        m->data_ptr = pkt;
        m->target_dpId = targetDpId;
        m->flags = 1;
        Train_QueueMessage(_g_train, m);
    } else {
        TrainMessage* m = allocate_train_message();
        if (m == nullptr) return;
        m->type = MESSAGE_REFRESH_REQUEST;
        m->data_len = 0;
        m->data_ptr = nullptr;
        m->target_dpId = 0;
        m->flags = 0;
        m->next = nullptr;
        NETMAN_QueueMessage(m);
    }
}

/* ================================================================== */
/* 10. UpdatePlayerInfo - 0x43D620                                    */
/* ================================================================== */
void Netman::UpdatePlayerInfo()
{
    uint8_t* pkt = static_cast<uint8_t*>(operator_new(6));
    if (pkt == nullptr) return;
    *reinterpret_cast<uint16_t*>(pkt + 0) = PACKET_PLAYER_INFO;
    pkt[4] = static_cast<uint8_t>(this->m_mySlotIndex);

    TrainMessage* m1 = allocate_train_message();
    if (m1 == nullptr) { GLOBAL_free(pkt); return; }
    m1->type = 6;
    m1->data_len = 6;
    m1->data_ptr = pkt;
    m1->target_dpId = 0;
    m1->flags = 1;
    Train_QueueMessage(_g_train, m1);

    TrainMessage* m2 = allocate_train_message();
    if (m2 == nullptr) return;
    m2->type = MESSAGE_SYNC_TRIGGER;
    m2->data_len = 0;
    m2->data_ptr = nullptr;
    m2->target_dpId = 0;
    m2->flags = 0;
    m2->next = nullptr;
    Train_QueueMessage(_g_train, m2);
}

/* ================================================================== */
/* 11. ProcessPlayerData - 0x43D6C0                                   */
/* ================================================================== */
void Netman::ProcessPlayerData(int32_t slotIndex)
{
    PlayerSlot* sl = &this->m_slots[slotIndex];
    if (sl->pixel_buffer) {
        GLOBAL_free(sl->pixel_buffer);
        sl->pixel_buffer = NULL;
        sl->data_size = 0;
        sl->pixel_width = 0;
        sl->pixel_height = 0;
        sl->version = 0;
    }
    if (sl->layout_name[0] == 0) return;

    char path[0x504];
    wsprintfA(path, FMT_LAYOUT_PATH, g_install_path, sl->layout_name);
    uint8_t rb[0x6E8];
    ResourceManager_Init(rb);
    if (ResourceManager_LoadResource(rb, path)) {
        uint16_t rw = *(uint16_t*)(rb + 0xC2);
        uint16_t rh = *(uint16_t*)(rb + 0xC4);
        int32_t ds = (int32_t)rw * (int32_t)rh;
        sl->pixel_width = rw;
        sl->pixel_height = rh;
        sl->data_size = ds;
        void* pd = operator_new(ds);
        sl->pixel_buffer = pd;
        void* sp = *(void**)(rb + 0x1D8);
        inline_memcpy(pd, sp, ds);
    }
    ResourceManager_ReleaseResource(rb);
}

/* ================================================================== */
/* 12. LoadScenario - 0x43D820                                        */
/* ================================================================== */
void Netman::LoadScenario(const char* layoutName)
{
    this->m_playerRows = 3;
    this->m_playerCols = 3;
    this->m_playerSlotCount = 9;
    this->m_bInit = 0;
    std::strcpy(this->m_layoutName, layoutName);

    char path[0x52c] = {};
    wsprintfA(path, "%sLayouts\\%s.lay", g_install_path, this->m_layoutName);
    std::vector<uint8_t> contents;
    int32_t asset_size = 0;
    uint8_t* asset_data = nullptr;
    if (g_asset_mgr != nullptr) {
        const std::size_t prefix = std::strlen(g_install_path);
        asset_data = AssetMgr_LoadFile(
            g_asset_mgr, reinterpret_cast<uint8_t*>(path + prefix), &asset_size);
        if (asset_data != nullptr && asset_size > 0)
            contents.assign(asset_data, asset_data + asset_size);
    }
    if (asset_data != nullptr) GLOBAL_free(asset_data);

    if (contents.empty()) {
#ifndef _WIN32
        // Host filesystems use native separators; the archive lookup above
        // remains the first path just as it is in 0x43D820.
        std::string native_path(path);
        for (char& ch : native_path) if (ch == '\\') ch = '/';
        std::FILE* file = std::fopen(native_path.c_str(), "rb");
#else
        std::FILE* file = std::fopen(path, "rb");
#endif
        if (file == nullptr) {
            const char* message = ERR_NO_STREAM;
            CRT_exit(&message, reinterpret_cast<const char**>(0x47a5e8));
            return;
        }
        contents.resize(0x2000);
        const std::size_t size = std::fread(contents.data(), 1, contents.size(), file);
        std::fclose(file);
        contents.resize(size);
    }
    contents.push_back(0);

    std::size_t cursor = 0;
    const auto next_digit = [&]() -> int32_t {
        while (contents[cursor] < '0' || contents[cursor] > '9') ++cursor;
        return contents[cursor++] - '0';
    };
    this->m_playerSlotCount = next_digit();
    this->m_playerRows = next_digit();
    this->m_playerCols = next_digit();
    while (contents[cursor] != '\n' && contents[cursor] != 0) ++cursor;
    if (contents[cursor] == '\n') ++cursor;

    for (int32_t index = 0; index < this->m_playerSlotCount; ++index) {
        const std::size_t begin = cursor;
        while (contents[cursor] != '\r' && contents[cursor] != '\n' &&
               contents[cursor] != 0) ++cursor;
        const std::size_t length = std::min<std::size_t>(cursor - begin, 31);
        std::memcpy(this->m_slots[index].layout_name, contents.data() + begin, length);
        this->m_slots[index].layout_name[length] = '\0';
        while (contents[cursor] == '\r' || contents[cursor] == '\n') ++cursor;
    }

    if (this->m_playerSlotCount > 9) this->m_playerSlotCount = 9;
    if (this->m_playerRows > 3) this->m_playerRows = 3;
    if (this->m_playerCols > 3) this->m_playerCols = 3;
    const int32_t cells = this->m_playerRows * this->m_playerCols;
    if ((cells != this->m_playerSlotCount && this->m_playerSlotCount <= cells) ||
        !((this->m_playerSlotCount == 2 && this->m_playerRows == 2 && this->m_playerCols == 1) ||
          (this->m_playerSlotCount == 3 && this->m_playerRows == 3 && this->m_playerCols == 1) ||
          (this->m_playerSlotCount == 4 && this->m_playerRows == 2 && this->m_playerCols == 2) ||
          (this->m_playerSlotCount == 6 && this->m_playerRows == 3 && this->m_playerCols == 2) ||
          (this->m_playerSlotCount == 9 && this->m_playerRows == 3 && this->m_playerCols == 3))) {
        this->m_playerSlotCount = 9;
        this->m_playerRows = 3;
        this->m_playerCols = 3;
    }
    for (int32_t index = 0; index < this->m_playerSlotCount; ++index)
        if (this->m_slots[index].dpId == 0) this->ProcessPlayerData(index);
}

/* ================================================================== */
/* 14. Cleanup - 0x43DC30                                             */
/* ================================================================== */
void Netman::Cleanup()
{
    /* === Drain g_network_queue with lock and type-dispatch === */
    ResourceManager_Lock(g_train_resources);

    {
        TrainMessage* msg = (TrainMessage*)g_network_queue;
        while (msg != NULL) {
            void* data_ptr = msg->data_ptr;
            TrainMessage* next = (TrainMessage*)msg->next;

            if (data_ptr != NULL) {
                switch (msg->type) {
                case 2: {
                    /* type 2: data_ptr is a linked list */
                    void* list = data_ptr;
                    while (list != NULL) {
                        void* next_list = *(void**)list;
                        void* sub_data = *(void**)((uint8_t*)list + 8);
                        if (sub_data != NULL) {
                            GLOBAL_free(sub_data);
                            *(void**)((uint8_t*)list + 8) = NULL;
                        }
                        GLOBAL_free(list);
                        list = next_list;
                    }
                    msg->data_ptr = NULL;
                    break;
                }
                case 0x0F:  /* INBOUND_APPEND — C++ object */
                case 0x11:  /* GAME_START_HOST — C++ object */
                    net_delete(data_ptr);
                    msg->data_ptr = NULL;
                    break;
                case 0x15:  /* FILE_TRANSFER player data — HeapAlloc'd */
                case 0x17:  /* FILE_TRANSFER forward ping — HeapAlloc'd */
                    HeapFree(GetProcessHeap(), 0, data_ptr);
                    msg->data_ptr = NULL;
                    break;
                default:
                    GLOBAL_free(data_ptr);
                    msg->data_ptr = NULL;
                    break;
                }
            }

            GLOBAL_free(msg);
            g_network_queue = next;
            msg = (TrainMessage*)g_network_queue;
        }
    }

    ResourceManager_Unlock(g_train_resources);

    /* === Free building list (m_buildingList at +0x7DC) === */
    {
        Building* node = this->m_buildingList;
        while (node != NULL) {
            /* Building's next pointer is at +0x70 (within Entity padding area) */
            Building* next = *(Building**)((uint8_t*)node + 0x70);
            this->m_buildingList = next;
            delete node;
            node = this->m_buildingList;
        }
    }

    /* === Free vehicle list (m_vehicleList at +0x7E0) === */
    {
        InboundTrainNode* node = this->m_vehicleList;
        while (node != NULL) {
            InboundTrainNode* next = node->next;
            this->m_vehicleList = next;
            net_delete(node);
            node = this->m_vehicleList;
        }
    }

    /* === Free per-slot msg_queues and pixel_buffers === */
    for (int32_t i = 0; i < 9; i++) {
        PlayerSlot* slot = &this->m_slots[i];

        /* Drain per-slot message queue */
        {
            void* msg = slot->msg_queue;
            while (msg != NULL) {
                void* next = *(void**)((uint8_t*)msg + 0x10);
                slot->msg_queue = next;
                GLOBAL_free(msg);
                msg = slot->msg_queue;
            }
        }

        /* Free cached pixel overlay buffer */
        if (slot->pixel_buffer != NULL) {
            GLOBAL_free(slot->pixel_buffer);
            slot->pixel_buffer = NULL;
        }
    }
}

/* ================================================================== */
/* 19. CheckTrackConnection - 0x43DE30                                */
/* ================================================================== */
int32_t Netman::CheckTrackConnection(int32_t angle, int32_t position)
{
    if (position < 0) position = this->m_mySlotIndex;
    int32_t col = position % this->m_playerCols;
    int32_t row = position / this->m_playerCols;
    switch (angle / 0x28) {
    case 0: return (row > 0) ? 1 : 0;
    case 2: return (col < this->m_playerCols - 1) ? 1 : 0;
    case 4: return (row < this->m_playerRows - 1) ? 1 : 0;
    case 6: return (col > 0) ? 1 : 0;
    }
    return 0;
}

/* 15-18. Edge wrappers */
void Netman::CheckRightEdge() { this->CheckTrackConnection(0x5A, -1); }
void Netman::CheckLeftEdge()  { this->CheckTrackConnection(0x10E, -1); }
void Netman::CheckUpEdge()    { this->CheckTrackConnection(0, -1); }
void Netman::CheckDownEdge()  { this->CheckTrackConnection(0xB4, -1); }

/* ================================================================== */
/* 20. SendPlayerName - 0x43DED0                                      */
/* ================================================================== */
int32_t Netman::SendPlayerName()
{
    if (this->m_gameMode != 2) return 0;

    this->m_sendTimer++;
    if (this->m_sendTimer % this->m_visibility != 0) {
        return this->m_sendTimer;
    }
    this->m_sendTimer = 0;

    if (this->m_currentSlot == NULL) return 0;

    /* Access m_currentSlot->msg_queue */
    void* msg_queue = this->m_currentSlot->msg_queue;
    if (msg_queue == NULL) return 0;

    /* Search for non-self slot with flag_36 != 0 */
    int32_t slot_idx;
    for (slot_idx = 0; slot_idx < this->m_playerSlotCount; slot_idx++) {
        if (slot_idx == this->m_mySlotIndex) continue;
        if (this->m_slots[slot_idx].flag_36 != 0) break;
    }

    if (slot_idx >= this->m_playerSlotCount) return slot_idx;

    uint16_t* packet = static_cast<uint16_t*>(operator_new(0x8000));
    if (packet == nullptr) return slot_idx;

    *packet = 0x3F6;
    *(uint8_t*)(packet + 1) = (uint8_t)this->m_mySlotIndex;
    *(uint8_t*)(packet + 2) = 1;
    *(uint16_t*)(packet + 3) = 0;

    /* Pack PingEntry nodes into the exact eight-byte wire records used at
     * 0x43DF75..0x43DFBA: dpId16, x16, y16, peer8, slot8. */
    {
        uint16_t entry_count = 0;
        auto* entry = static_cast<PingEntry*>(msg_queue);
        while (entry != nullptr) {
            ++entry_count;
            uint8_t* output = reinterpret_cast<uint8_t*>(packet) +
                              static_cast<std::size_t>(entry_count) * 8 + 1;
            *reinterpret_cast<uint16_t*>(output) = static_cast<uint16_t>(entry->dpId);
            *reinterpret_cast<uint16_t*>(output + 2) = static_cast<uint16_t>(entry->pos_x);
            *reinterpret_cast<uint16_t*>(output + 4) = static_cast<uint16_t>(entry->pos_y);
            output[6] = entry->peer_index;
            output[7] = entry->slot_index;
            entry = static_cast<PingEntry*>(entry->next);
        }
        packet[3] = entry_count;
        reinterpret_cast<uint8_t*>(packet)[entry_count * 8 + 9] = 0;
    }

    /* Wrap in TrainMessage and queue */
    {
        TrainMessage* msg = allocate_train_message();
        if (msg == nullptr) {
            GLOBAL_free(packet);
            return slot_idx;
        }
        msg->type        = 6;
        msg->data_len    = static_cast<int32_t>(packet[3]) * 8 + 10;
        msg->data_ptr    = packet;
        msg->target_dpId = 0;
        msg->flags       = 0;
        msg->next        = nullptr;

        Train_QueueMessage(_g_train, msg);
    }

    return slot_idx;
}

/* ================================================================== */
/* 21. ReceivePlayerName - 0x43E010                                   */
/* ================================================================== */
uint32_t Netman::ReceivePlayerName()
{
    if (!this->m_tickCounter) return 0;
    if (this->m_tickCounter % this->m_timeout) return (uint32_t)this->m_tickCounter;
    this->m_tickCounter = 0;

    InboundTrainNode* prev = NULL;
    InboundTrainNode* node = this->m_vehicleList;
    while (node) {
        if (node->ack_counter == 0) {
            if (!prev)
                this->m_vehicleList = node->next;
            else
                prev->next = node->next;
            node->next = NULL;
            break;
        }
        prev = node;
        node = node->next;
    }
    if (!node) return 0;

    if (this->m_gameMode == 2) {
        if (this->m_slots[node->slot_index].dpId == 0) {
            net_delete(node);
            return 1;
        }
    }

    if (!this->SendChatMessage(node)) {
        node->next = this->m_vehicleList;
        this->m_vehicleList = node;
        return 0;
    }

    for (uint16_t index = 1; index <= node->editor_count && index < 4;
         ++index) {
        VehicleEditor* editor = node->editors[index];
        if (editor == nullptr) continue;
        DPlayManager* dplay = editor->GetDPlayData();
        if (dplay == nullptr || g_player_config == nullptr) continue;
        if (std::strcmp(g_player_config->name,
                        reinterpret_cast<const char*>(dplay->m_sessionBlk1)) == 0 &&
            dplay->m_dwordValue == 0 && dplay->m_wordValue != 0) {
            char path[0x504] = {};
            NET_GetAttFilePath(dplay->m_wordValue, 5, path);
            return static_cast<uint32_t>(
                PlaySoundFile(path, g_listener_x, g_listener_y, 4));
        }
    }
    return 1;
}

/* ================================================================== */
/* 22. SendChatMessage - 0x43E1D0                                     */
/* ================================================================== */
uint8_t Netman::SendChatMessage(InboundTrainNode* node)
{
    uint16_t ang = node->tunnel_angle;
    if (g_game_mode != 3 && g_game_mode != 5 && g_game_mode != 9) return 0;
    int32_t dir, off;
    if (this->m_gameMode == 1) { dir = 0; off = 0; goto fin; }
    if (ang < 0x5B) {
        if (ang == 0x5A)      { dir = 1; off = INPUT_DirToOffset_Left(&off); }
        else if (ang == 0)    { dir = 4; off = INPUT_DirToOffset_Down(&off); }
        else                  { dir = 0; off = 0; }
    } else if (ang == 0xB4)   { dir = 3; off = INPUT_DirToOffset_Right(&off); }
    else if (ang == 0x10E)    { dir = 2; off = INPUT_DirToOffset_Up(&off); }
    else                      { dir = 0; off = 0; }
fin:
    return World_FinalizeLoad((void*)0x4A98B0, node, (void*)(intptr_t)off, (uint8_t)dir);
}

/* ================================================================== */
/* 23. ReceiveChatMessage - 0x43E2E0                                  */
/* ================================================================== */
void Netman::ReceiveChatMessage(TrainMessage* msg)
{
    InboundTrainNode* node = (InboundTrainNode*)msg->data_ptr;
    if (this->m_gameMode == 1) {
        InboundTrainNode* tail = this->m_vehicleList;
        if (!tail) {
            this->m_vehicleList = node;
            this->m_tickCounter = this->m_timeout - 0x20;
        } else {
            while (tail->next) tail = tail->next;
            tail->next = node;
        }
        node->next = nullptr;
        node->process_delay = 0;
#ifndef _WIN32
        uint32_t depth = 0;
        for (Vehicle* item = this->m_vehicleList; item != nullptr;
             item = item->next) ++depth;
        loco::host_test::emit_netman_vehicle_adopted(
            static_cast<uint32_t>(node->editor_count + 1),
            node->network_id, depth);
#endif
    } else {
        msg->data_ptr = NULL;
        net_delete(node);
    }
}

/* ================================================================== */
/* 24. SendGameStart - 0x43E370                                       */
/* ================================================================== */
void Netman::SendGameStart(TrainMessage* msg)
{
    InboundTrainNode* node = (InboundTrainNode*)msg->data_ptr;
    if (this->m_gameMode != 2) {
        msg->data_ptr = NULL;
        net_delete(node);
        return;
    }
    /* Append to m_vehicleList */
    {
        InboundTrainNode* tail = this->m_vehicleList;
        if (!tail) this->m_vehicleList = node;
        else {
            while (tail->next)
                tail = tail->next;
            tail->next = node;
        }
    }
    node->process_delay = 0;
    int32_t off = 0, dir;
    uint16_t ang = node->tunnel_angle;
    if (ang < 0x5B) {
        if (ang == 0x5A)      { off = INPUT_DirToOffset_Left(&off);  dir = 1; }
        else if (ang == 0)    { off = INPUT_DirToOffset_Down(&off);  dir = 4; }
        else                  { dir = 0; }
    } else if (ang == 0xB4)   { off = INPUT_DirToOffset_Right(&off); dir = 3; }
    else if (ang == 0x10E)    { off = INPUT_DirToOffset_Up(&off);    dir = 2; }
    else                      { dir = 0; }

    this->ReceivePing(node->network_id, node->slot_index, node->peer_index, off, dir);

    bool has_dplay_data = false;
    for (uint16_t index = 1; index <= node->editor_count && index < 4;
         ++index) {
        VehicleEditor* editor = node->editors[index];
        if (editor != nullptr && editor->GetDPlayData() != nullptr) {
            has_dplay_data = true;
            break;
        }
    }
    for (uint16_t index = 1; index <= node->editor_count && index < 4;
         ++index) {
        VehicleEditor* editor = node->editors[index];
        if (editor == nullptr) continue;
        const int32_t resource_id = static_cast<int32_t>(editor->GetResourceId());
        const int32_t replacement = has_dplay_data ? 0x1871 : 0x1870;
        if ((has_dplay_data && resource_id == 0x1870) ||
            (!has_dplay_data && resource_id == 0x1871)) {
#ifndef _WIN32
            // Host network editors intentionally have no original resource
            // object; retain the evidenced state transition by ID.
            editor->res_id = replacement;
#else
            CarObject* car = static_cast<CarObject*>(static_cast<void*>(editor));
            const int32_t frame = editor->frame_index;
            car->SetResourceId(replacement, -1);
            car->SetParam(frame, 1);
#endif
        }
    }
}

/* ================================================================== */
/* 25. ReceiveGameStart - 0x43E560                                    */
/* ================================================================== */
void* Netman::ReceiveGameStart(void* worldOrObj, int param, InboundTrainNode* node)
{
    World_GetObjectAt((int32_t)(intptr_t)node);
    this->HandleTimeout(node);

    /* field_04 at +0x04 within vehicle_payload */
    if (*(uint32_t*)((uint8_t*)node + 4) == 1) {
        net_delete(node);
        return (void*)1;
    }
    if (this->SendSignalChange(node) && this->m_gameMode == 1) {
        node->next = this->m_vehicleList;
        this->m_vehicleList = node;
        return (void*)1;
    }
    if (*(int32_t*)((uint8_t*)_g_netman_data + 0x10) == 0) {
        node->next = this->m_vehicleList;
        this->m_vehicleList = node;
        return (void*)1;
    }
    if (this->m_gameMode == 1) {
        if (!NETMAN_SendTrainPosition(node)) {
            node->next = this->m_vehicleList;
            this->m_vehicleList = node;
        }
        return (void*)1;
    }
    if (this->m_gameMode != 2) {
        node->next = this->m_vehicleList;
        this->m_vehicleList = node;
        return (void*)1;
    }
    if (this->m_slots[node->slot_index].dpId == 0) {
        net_delete(node);
    } else {
        if (!NETMAN_ReceiveTrainPosition(param, (int32_t)(intptr_t)worldOrObj, (int32_t)(intptr_t)node)) {
            node->next = this->m_vehicleList;
            this->m_vehicleList = node;
        }
    }
    return (void*)1;
}

/* ================================================================== */
/* 26. Update — Per-frame network update (0x43F0C0)                   */
/* ================================================================== */
void Netman::Update()
{
    this->m_tickCounter++;

    /* Drain network message queue under lock */
    while (g_network_queue != NULL) {
        ResourceManager_Lock(g_train_resources);

        TrainMessage* msg = (TrainMessage*)g_network_queue;
        if (msg != NULL) {
            g_network_queue = msg->next;
        }

        ResourceManager_Unlock(g_train_resources);

        if (msg != NULL) {
#ifndef _WIN32
            // Original 0x439240 gives outbound type-6 messages to the Train
            // worker. The SDL host has no Win32 worker; retain queue ordering
            // but dispatch transport sends before processing Netman messages.
            if (msg->type == 6 && _g_train != nullptr)
                static_cast<TrainSubsystem*>(_g_train)->DispatchMessage(msg);
            else
                this->ProcessMessage(msg);
#else
            this->ProcessMessage(msg);
#endif
            GLOBAL_free(msg);
        }
    }

    /* Process inbound train arrivals */
    if (this->m_vehicleList != NULL) {
        this->ReceivePlayerName();
    }

    /* Broadcast local player name / presence */
    this->SendPlayerName();
}

/* ================================================================== */
/* 27. ProcessMessage — Main message dispatcher (0x43F2B0)            */
/* ================================================================== */
void Netman::ProcessMessage(TrainMessage* msg)
{
#ifndef _WIN32
    loco::host_test::emit_netman_message_processed(msg->type, msg->flags);
#endif
    switch (msg->type) {
    case 2:  /* SYNC_GAME_STATE */
        if (this->m_gameMode == 0) {
            EditorState_SelectLayout(
                *(void**)((uint8_t*)g_ui_main + 0x220),
                *(int32_t*)((uint8_t*)msg->data_ptr + 8));
            return;
        }
        /* Free linked list data in msg->data_ptr */
        {
            void* listPtr = msg->data_ptr;
            while (listPtr != NULL) {
                void* nextPtr = *(void**)listPtr;
                if (*(void**)((uint8_t*)listPtr + 8) != NULL) {
                    GLOBAL_free(*(void**)((uint8_t*)listPtr + 8));
                    *(void**)((uint8_t*)listPtr + 8) = NULL;
                }
                GLOBAL_free(listPtr);
                listPtr = nextPtr;
            }
        }
        msg->data_ptr = NULL;
        return;

    case 3:  /* HOST_SESSION_START */
    {
        this->m_bFlag1 = 1;
        this->m_field_7D8 = *(int32_t*)((uint8_t*)_g_train + 0x10);
        this->m_myDpId = msg->target_dpId;

        if (*((uint8_t*)_g_netman_data + 8) != 0) {
            this->m_slots[0].dpId = this->m_myDpId;
            inline_memcpy(
                this->m_slots[0].layout_name,
                (const uint8_t*)g_player_config + 6,
                strlen((const char*)((uint8_t*)g_player_config + 6)) + 1);
        }

        if (this->m_gameMode == 0) {
            if (*((uint8_t*)(*(void**)((uint8_t*)g_ui_main + 0x220)) + 0xE8) != 0) {
                EditorState_HandleNetworkGame(*(void**)((uint8_t*)g_ui_main + 0x220));
            }
        }
        break;
    }

    case 4:  /* LAYOUT_SELECT */
    {
        if (*((uint8_t*)_g_netman_data + 8) != 0) {
            uint32_t playerInfo = msg->metadata16();
            this->SendLayoutSelect(
                msg->target_dpId,
                msg->flags,
                (const char*)msg->data_ptr,
                playerInfo);
        }
        GLOBAL_free(msg->data_ptr);
        goto redraw_if_visible;
    }

    case 5:  /* NET_RESET */
    {
        this->ResetNetworkState();
        this->m_field_7D8 = 0;
        this->m_myDpId = 0;

        if (this->m_gameMode == 0) {
            int32_t* panel = *(int32_t**)((uint8_t*)g_ui_main + 0x220);
            if (*((uint8_t*)panel + 0xE8) != 0) {
                EditorState_StartGameTimer(panel);
                return;
            }
        } else if (this->m_gameMode == 2) {
            MessageBeep(0x30);
            {
                char msgBuf[256];
                FormatResourceString(&g_resmgr, 0x7E, msgBuf, sizeof(msgBuf));
                MessageBoxA(
                    *(void**)((uint8_t*)g_main_window + 8),
                    msgBuf,
                    STR_LEGO_LOCO,
                    0);
            }
            CGWND_QuitToMenu();
            return;
        }
        break;
    }

    case 9:  /* GAME_STATE_SYNC (client variant) */
    {
        if (*((uint8_t*)_g_netman_data + 8) == 0) {
            this->SyncGameState(msg);
        }
        GLOBAL_free(msg->data_ptr);
        return;
    }

    case 0xB:  /* REMOVE_TRAIN */
        this->RemoveInboundTrain(msg->target_dpId);
        goto redraw_if_visible;

    case 0xC:  /* PLAYER_JOIN */
        this->HandlePlayerJoin();
        return;

    case 0xF:  /* INBOUND_APPEND */
        this->ReceiveChatMessage(msg);
        return;

    case 0x11:  /* GAME_START (host-side) */
        this->SendGameStart(msg);
        return;

    case 0x12:  /* FILE_TRANSFER variants */
    case 0x15:
    case 0x17:
        this->SendFileTransfer(msg);
        return;

    case 0x13:  /* FLAG_SET */
        if (msg->flags >= 0) {
            this->m_slots[msg->flags].flag_36 = 1;
        }
        return;

    case 0x14:  /* FLAG_CLEAR */
        if (msg->flags >= 0) {
            this->m_slots[msg->flags].flag_36 = 0;
        }
        return;

    case 0x16:  /* PIXEL_DATA */
    {
        uint8_t* pkt = (uint8_t*)msg->data_ptr;
        uint16_t w = *(uint16_t*)(pkt + 6);
        uint16_t h = *(uint16_t*)(pkt + 0xC);
        int32_t  ds  = *(int32_t*)(pkt + 0x10);
        int32_t  slotIdx = msg->flags;

        if (slotIdx >= 0 && slotIdx < 9 && ds >= 0) {
            PlayerSlot* sl = &this->m_slots[slotIdx];
            if (sl->pixel_buffer != NULL) {
                GLOBAL_free(sl->pixel_buffer);
            }
            sl->data_size = ds;
            sl->pixel_buffer = operator_new(ds);
            inline_memcpy(sl->pixel_buffer, pkt + 0x14, ds);
            sl->pixel_width = w;
            sl->pixel_height = h;
            sl->version = *(int32_t*)(pkt + 0x0C);
            sl->is_connected = 1;
#ifndef _WIN32
            loco::host_test::emit_netman_pixel_data_updated(
                slotIdx, ds, w, h);
#endif
        }
        HeapFree(GetProcessHeap(), 0, msg->data_ptr);
        return;
    }

    case 0x18:  /* POS_ACK */
    {
        InboundTrainNode* vnode = this->m_vehicleList;
        while (vnode != NULL) {
            if (msg->metadata0() == vnode->slot_index &&
                msg->flags == vnode->network_id) {
                if (vnode->ack_counter != 0) {
                    vnode->ack_counter--;
                }
                return;
            }
            vnode = vnode->next;
        }
        break;
    }

    case 0x1A:  /* PLAYER_LEAVE */
        this->HandlePlayerLeave(msg);
        return;

    case 0x1B:  /* REFRESH request */
        this->SendMapData(0);
        return;

    case 0x1C:  /* TIMEOUT check */
        this->CheckTimeout(2);
        break;
    }
    return;

redraw_if_visible:
    /* Shared UI redraw path */
    {
        void* panel = *(void**)((uint8_t*)g_ui_main + 0x220);
        if (IsWindowVisible(*(void**)((uint8_t*)panel + 8))) {
            CGWND_GameSetup_DrawGrid_Thunk(panel);
            UIPANEL_EndPaintEx(panel,
                *(void**)((uint8_t*)panel + 8),
                0, 0, NULL);
        }
    }
}

/* ================================================================== */
/* 28. Shutdown — Full network subsystem shutdown (0x43F7B0)          */
/* ================================================================== */
void Netman::Shutdown()
{
    this->ResetNetworkState();
    this->m_field_7D8 = 0;
    this->m_myDpId = 0;

    /* Free building list */
    {
        Building* node = this->m_buildingList;
        while (node != NULL) {
            Building* next = *(Building**)((uint8_t*)node + 0x70);
            this->m_buildingList = next;
            delete node;
            node = this->m_buildingList;
        }
    }

    /* Free vehicle list */
    {
        InboundTrainNode* node = this->m_vehicleList;
        while (node != NULL) {
            InboundTrainNode* next = node->next;
            this->m_vehicleList = next;
            net_delete(node);
            node = this->m_vehicleList;
        }
    }

    /* Free per-slot resources */
    for (int32_t i = 0; i < 9; i++) {
        PlayerSlot& slot = this->m_slots[i];
        void* q = slot.msg_queue;
        while (q != NULL) {
            void* next = *(void**)((uint8_t*)q + 0x10);
            slot.msg_queue = next;
            GLOBAL_free(q);
            q = slot.msg_queue;
        }

        if (slot.pixel_buffer != NULL) {
            GLOBAL_free(slot.pixel_buffer);
            slot.pixel_buffer = NULL;
        }
    }

    this->Init(0);
    this->SetGameMode(0);
    this->ResetNetworkState();
}

/* ================================================================== */
/* 29. HandlePlayerJoin - 0x43F880                                    */
/* ================================================================== */
void Netman::HandlePlayerJoin()
{
    for (int32_t i = 0; i < 9; i++) {
        if (this->m_slots[i].dpId == this->m_field_7D8) {
            this->m_slots[i].is_connected = 0;
            this->m_slots[i].dpId = 0;
        }
    }
    this->m_field_7D8 = 0;

    if (this->m_gameMode == 0) {
        void* panel = *(void**)((uint8_t*)g_ui_main + 0x220);
        if (*((uint8_t*)panel + 0xE8) != 0) {
            EditorState_LoadExistingGame(panel);
        }
    } else if (this->m_gameMode == 2) {
        *((uint8_t*)_g_netman_data + 8) = 1;
        NETMAN_ReceiveLayoutSelect(this);

        void* panel = *(void**)((uint8_t*)g_ui_main + 0x220);
        if (IsWindowVisible(*(void**)((uint8_t*)panel + 8))) {
            CGWND_GameSetup_DrawGrid_Thunk(panel);
            UIPANEL_EndPaintEx(panel,
                *(void**)((uint8_t*)panel + 8),
                0, 0, NULL);
        }
    } else {
        this->ResetNetworkState();
    }
}

/* ================================================================== */
/* 30. RemoveInboundTrain - 0x43F940                                  */
/* ================================================================== */
void Netman::RemoveInboundTrain(int32_t dpId)
{
    int32_t slotIdx = -1;
    for (int32_t i = 0; i < 9; i++) {
        if (this->m_slots[i].dpId == dpId) {
            slotIdx = i;
            break;
        }
    }
    if (slotIdx < 0) return;

    World_SerializeObject((void*)0x4A98B0, (char)slotIdx);

    /* Unlink and destroy matching nodes from m_vehicleList */
    {
        InboundTrainNode* prev = NULL;
        InboundTrainNode* vnode = this->m_vehicleList;
        while (vnode != NULL) {
            InboundTrainNode* next = vnode->next;
            if (slotIdx == vnode->slot_index) {
                if (prev == NULL) {
                    this->m_vehicleList = next;
                } else {
                    prev->next = next;
                }
                vnode->next = NULL;
                net_delete(vnode);
                OutputDebugStringA(STR_REMOVED);
                vnode = this->m_vehicleList;
                prev = NULL;
            } else {
                prev = vnode;
                vnode = next;
            }
        }
    }

    /* Walk per-slot transfer_lists, remove matching PingEntry nodes */
    if (this->m_mySlotIndex >= 0) {
        for (int32_t i = 0; i < 9; i++) {
            PlayerSlot* sl = &this->m_slots[i];
            void** listHead = &sl->msg_queue;
            void* pingPrev = NULL;
            void* pingNode = *listHead;
            while (pingNode != NULL) {
                void* pingNext = *(void**)((uint8_t*)pingNode + 0x10);
                if (*((uint8_t*)pingNode + 0x0C) == slotIdx) {
                    if (pingPrev == NULL) {
                        *listHead = pingNext;
                    } else {
                        *(void**)((uint8_t*)pingPrev + 0x10) = pingNext;
                    }
                    GLOBAL_free(pingNode);
                    pingNode = *listHead;
                } else {
                    pingPrev = pingNode;
                    pingNode = pingNext;
                }
            }
        }
    }

    /* Clear slot fields */
    {
        int32_t idx;
        for (idx = 0; idx < 9; idx++) {
            if (this->m_slots[idx].dpId == dpId) break;
        }
        if (idx < 9) {
            PlayerSlot* sl = &this->m_slots[idx];
            sl->layout_name[0] = '\0';
            sl->dpId = 0;
            sl->is_connected = 0;
            if (sl->pixel_buffer != NULL) {
                GLOBAL_free(sl->pixel_buffer);
                sl->pixel_buffer = NULL;
                sl->data_size = 0;
                sl->pixel_width = 0;
                sl->pixel_height = 0;
                sl->version = 0;
            }
        }
    }

    this->ProcessPlayerData(slotIdx);

    /* Drain per-slot msg_queue */
    {
        PlayerSlot* sl = &this->m_slots[slotIdx];
        void* q = sl->msg_queue;
        while (q != NULL) {
            void* next = *(void**)((uint8_t*)q + 0x10);
            sl->msg_queue = next;
            GLOBAL_free(q);
            q = sl->msg_queue;
        }
    }

    /* Redraw UI */
    {
        void* panel = *(void**)((uint8_t*)g_ui_main + 0x220);
        if (IsWindowVisible(*(void**)((uint8_t*)panel + 8))) {
            CGWND_GameSetup_DrawGrid_Thunk(panel);
            UIPANEL_EndPaintEx(panel,
                *(void**)((uint8_t*)panel + 8),
                0, 0, NULL);
        }
    }
}

/* ================================================================== */
/* 31. HandlePlayerLeave - 0x43FB50                                   */
/*                                                                     */
/* NOTE: msg->data_ptr field reused as inline int32_t slot index       */
/* (the binary packs the slot index directly into the pointer field).  */
/* ================================================================== */
void Netman::HandlePlayerLeave(TrainMessage* msg)
{
    /* data_ptr contains inline slot index, not a pointer */
    int32_t slotIdx = *(int32_t*)((uint8_t*)msg + 8);

    World_SerializeObject((void*)0x4A98B0, (char)slotIdx);

    /* Remove ONE matching node from m_vehicleList */
    {
        InboundTrainNode* prev = NULL;
        InboundTrainNode* vnode = this->m_vehicleList;
        while (vnode != NULL && slotIdx >= 0) {
            if (slotIdx == vnode->slot_index) {
                if (prev == NULL) {
                    this->m_vehicleList = vnode->next;
                } else {
                    prev->next = vnode->next;
                }
                vnode->next = NULL;
                net_delete(vnode);
                break;
            }
            prev = vnode;
            vnode = vnode->next;
        }
    }

    /* Walk transfer_list calling ReceiveAck for matching pending transfers */
    if (this->m_mySlotIndex >= 0) {
        void* listHead = this->m_slots[this->m_mySlotIndex].msg_queue;
        void* pingNode = listHead;
        while (pingNode != NULL) {
            void* pingNext = *(void**)((uint8_t*)pingNode + 0x10);
            if (*((uint8_t*)pingNode + 0x0C) == slotIdx) {
                this->ReceiveAck(
                    *(int32_t*)pingNode,
                    *((uint8_t*)pingNode + 0x0C),
                    *((uint8_t*)pingNode + 0x0D));
                pingNode = this->m_slots[this->m_mySlotIndex].msg_queue;
            } else {
                pingNode = pingNext;
            }
        }
    }

    /* Redraw UI */
    {
        void* panel = *(void**)((uint8_t*)g_ui_main + 0x220);
        if (IsWindowVisible(*(void**)((uint8_t*)panel + 8))) {
            CGWND_GameSetup_DrawGrid_Thunk(panel);
            UIPANEL_EndPaintEx(panel,
                *(void**)((uint8_t*)panel + 8),
                0, 0, NULL);
        }
    }
}

/* ================================================================== */
/* 32. SyncGameState - 0x43FC50                                       */
/* ================================================================== */
void Netman::SyncGameState(TrainMessage* msg)
{
    /* Update grid dimensions from packet header */
    this->m_playerRows      = msg->metadata0();
    this->m_playerCols      = msg->metadata1();
    this->m_playerSlotCount = msg->flags;

    int32_t newMySlotIdx = 0;
    bool versionChanged = false;

    for (int32_t i = 0; i < 9; i++) {
        PlayerSlot* sl = &this->m_slots[i];
        int32_t oldDpId = sl->dpId;
        bool wasEmpty = (oldDpId == 0);
        uint8_t oldFlag36 = sl->flag_36;
        int32_t oldVersion = sl->version;

        const PlayerSlot* srcData = static_cast<const PlayerSlot*>(msg->data_ptr) + i;
        sl->dpId = srcData->dpId;
        sl->is_connected = srcData->is_connected;
        std::memcpy(sl->compact_name, srcData->compact_name,
                    sizeof(sl->compact_name));
        std::memcpy(sl->layout_name, srcData->layout_name, sizeof(sl->layout_name));
        sl->player_id = srcData->player_id;
        sl->player_color = srcData->player_color;
        sl->flag_36 = srcData->flag_36;
        sl->version = oldVersion;

        if (sl->dpId == this->m_myDpId) {
            if (oldFlag36 == 0 && sl->flag_36 != 0) {
                NETMAN_ReceiveFileTransfer(this);
            } else if (oldFlag36 != 0 && sl->flag_36 == 0) {
                NETMAN_SendAck(this);
            }

            if (this->m_mySlotIndex == i) {
                int32_t packetVersion = srcData->version;
                if (packetVersion != sl->version) {
                    versionChanged = true;
                }
            } else {
                newMySlotIdx = i;
                this->m_currentSlot = sl;
                this->m_mySlotIndex = i;
            }
        } else if (sl->dpId == 0) {
            if (!wasEmpty || sl->data_size == 0) {
                this->ProcessPlayerData(i);
            }
        } else {
            int32_t packetVersion = srcData->version;
            if (!wasEmpty && oldVersion != packetVersion) {
                NETMAN_SendDisconnect(sl->dpId);
            }
        }
    }

    if (this->m_bInit == 0) {
        this->m_bInit = 1;
        NETMAN_SendDisconnect(0);
    }

    if (versionChanged) {
        this->SendBuildingData(this->m_field_7D8);
    }

    if (this->m_gameMode == 0) {
        EditorState_SetDifficulty(
            *(void**)((uint8_t*)g_ui_main + 0x220),
            newMySlotIdx);
    }

#ifdef _WIN32
    /* Original child-HWND redraw; SDL composition refreshes every frame. */
    {
        void* panel = *(void**)((uint8_t*)g_ui_main + 0x220);
        if (IsWindowVisible(*(void**)((uint8_t*)panel + 8))) {
            CGWND_GameSetup_DrawGrid_Thunk(panel);
            UIPANEL_EndPaintEx(panel,
                *(void**)((uint8_t*)panel + 8),
                0, 0, NULL);
        }
    }
#endif
}

/* ================================================================== */
/* 33. SendLayoutSelect - 0x43FE30                                    */
/* ================================================================== */
void Netman::SendLayoutSelect(int32_t dpId, int32_t targetSlot,
                               const char* name, int32_t playerInfo)
{
    int32_t srcSlotIdx = -2;

    if (dpId == -1) {
        /* Search by name string */
        for (int32_t i = 0; i < 9; i++) {
            const char* slotName = this->m_slots[i].layout_name;
            int32_t j;
            for (j = 0; ; j++) {
                uint8_t c1 = (uint8_t)name[j];
                uint8_t c2 = (uint8_t)slotName[j];
                if (c1 != c2) break;
                if (c1 == 0) { srcSlotIdx = i; break; }
            }
            if (srcSlotIdx == i) break;
        }
    } else {
        for (int32_t i = 0; i < 9; i++) {
            if (this->m_slots[i].dpId == dpId) {
                srcSlotIdx = i;
                break;
            }
        }
    }

    uint16_t player_id    = (uint16_t)(playerInfo & 0xFFFF);
    uint16_t player_color = (uint16_t)((playerInfo >> 16) & 0xFFFF);

    if (srcSlotIdx == targetSlot) {
        this->m_slots[srcSlotIdx].player_id    = player_id;
        this->m_slots[srcSlotIdx].player_color = player_color;
        this->m_slots[srcSlotIdx].version      = 0;
    } else {
        PlayerSlot* tgt = &this->m_slots[targetSlot];

        if (tgt->dpId == 0 && targetSlot >= 0) {
            inline_memcpy(tgt->layout_name, name, strlen(name) + 1);
            /* NOTE: when dpId=-1 (search by name), use the found source slot's dpId */
            tgt->dpId         = (srcSlotIdx >= 0) ? this->m_slots[srcSlotIdx].dpId : dpId;
            tgt->player_id    = player_id;
            tgt->player_color = player_color;
            tgt->version      = 0;

            if (tgt->dpId == this->m_myDpId) {
                this->m_currentSlot  = tgt;
                this->m_mySlotIndex  = targetSlot;
            }

            if (srcSlotIdx >= 0) {
                this->m_slots[srcSlotIdx].layout_name[0] = '\0';
                this->m_slots[srcSlotIdx].dpId           = 0;
                this->m_slots[srcSlotIdx].version        = 0;
            }
        } else if (srcSlotIdx < 0) {
            for (int32_t i = 0; i < 9; i++) {
                if (this->m_slots[i].dpId == 0 && this->m_slots[i].layout_name[0] == '\0') {
                    inline_memcpy(this->m_slots[i].layout_name, name, strlen(name) + 1);
                    this->m_slots[i].dpId         = dpId;
                    this->m_slots[i].player_id    = player_id;
                    this->m_slots[i].player_color = player_color;
                    this->m_slots[i].version      = 0;
                    break;
                }
            }
        }
    }

    if (this->m_bFlag1 != 0) {
        NETMAN_ReceiveLayoutSelect(this);
    }
}

/* ================================================================== */
/* 34. SendSignalChange - 0x43E690                                    */
/* ================================================================== */
uint8_t Netman::SendSignalChange(InboundTrainNode* node)
{
#ifndef _WIN32
    DPlayManager* resolved_routes[3] = {nullptr, nullptr, nullptr};
    bool has_valid_data = false;
    bool has_stale_track = false;
    const uint16_t route_count = node != nullptr && node->editor_count < 4
        ? static_cast<uint16_t>(node->editor_count) : 0;
    for (uint16_t index = 1; index <= route_count; ++index) {
        VehicleEditor* editor = node->editors[index];
        if (editor == nullptr) continue;
        DPlayManager* source = editor->GetDPlayData();
        if (source == nullptr) continue;
        if (source->m_sessionBlk1[20] == 0) {
            has_stale_track = true;
            continue;
        }
        editor->SetDPlayData(nullptr);
        resolved_routes[index - 1] = static_cast<DPlayManager*>(
            NETMAN_ReceiveSignalChange(source));
        has_valid_data = has_valid_data || resolved_routes[index - 1] != nullptr;
    }

    if (has_valid_data) {
        void* storage = operator_new(sizeof(Vehicle));
        Vehicle* vehicle = storage != nullptr
            ? ::new (storage) Vehicle(HostNetworkVehicleTag{},
                  (CRT_rand() % 3) * 2 + 0x1804)
            : nullptr;
        if (vehicle != nullptr && vehicle->editors[0] != nullptr) {
            vehicle->network_id = static_cast<uint16_t>(++this->m_field_7E8);
            vehicle->editors[0]->CopyName(STR_LEGO_LOCO);
            vehicle->max_steps = node->max_steps;
            vehicle->tunnel_angle = 0;
            vehicle->field_76 = vehicle->field_7E = vehicle->field_80 = 0;
            vehicle->field_82 = 0;
            vehicle->field_84 = vehicle->field_86 = 0;
            for (DPlayManager*& route : resolved_routes) {
                if (route == nullptr) continue;
                if (g_player_config != nullptr) {
                    const std::size_t name_size = std::min(
                        std::strlen(g_player_config->name) + 1,
                        sizeof(route->m_sessionBlk1));
                    std::memcpy(route->m_sessionBlk1, g_player_config->name,
                                name_size);
                }
                route->m_wordValue = 0;
                vehicle->AddHostNetworkRoute(*route);
                route->~DPlayManager();
                GLOBAL_free(route);
                route = nullptr;
            }
            vehicle->direction = 2;
            vehicle->state = 0;
            vehicle->init_flag = 0;
            vehicle->next = this->m_vehicleList;
            this->m_vehicleList = vehicle;
        } else if (vehicle != nullptr) {
            vehicle->~Vehicle();
            GLOBAL_free(vehicle);
        }
    }
    for (DPlayManager* route : resolved_routes) {
        if (route == nullptr) continue;
        route->~DPlayManager();
        GLOBAL_free(route);
    }
    this->HandleTimeout(node);
    return (!has_stale_track && this->m_gameMode == 1) ? 1 : 0;
#else
    void* dplayData[3] = { NULL, NULL, NULL };
    uint8_t hasValidData = 0;
    uint8_t hasStaleTrack = 0;

    /* car_count and car_handles are within vehicle_payload area */
    uint16_t carCount = *(uint16_t*)((uint8_t*)node + 0x0C);
    for (int32_t ci = 0; ci < (int32_t)carCount; ci++) {
        int32_t* carHandle = *(int32_t**)((uint8_t*)node + 0x14 + ci * 4);
        void* dd = (void*)(uintptr_t)VehicleEditor_GetDPlayData(*carHandle);

        if (dd != NULL) {
            if (*((uint8_t*)dd + 0x24) == 0) {
                hasStaleTrack = 1;
                dplayData[ci] = NULL;
            } else {
                VehicleEditor_SetDPlayData((void*)(uintptr_t)*carHandle, 0);
                dplayData[ci] = NETMAN_ReceiveSignalChange(dd);
                if (dplayData[ci] != NULL) {
                    hasValidData = 1;
                }
            }
        }
    }

    if (hasValidData) {
        InboundTrainNode* vehicle = (InboundTrainNode*)operator_new(0x94);
        if (vehicle != NULL) {
            int32_t rnd = CRT_rand();
            int32_t resId = (rnd % 3) * 2 + 0x1804;
            vehicle = Vehicle_Ctor(vehicle, resId, 1, 1, 0);
        }

        if (vehicle != NULL) {
            this->m_field_7E8++;
            vehicle->network_id = (uint16_t)this->m_field_7E8;

            /* Set display name via vehicle's sprite (at +0x10 in vehicle_payload) */
            {
                SpriteObject* sprite = *(SpriteObject**)((uint8_t*)vehicle + 0x10);
                sprite->SetDisplayName(STR_LEGO_LOCO);
            }

            /* speed is at +0x58 within vehicle_payload */
            Vehicle_CalcSpeed(vehicle, *(uint16_t*)((uint8_t*)node + 0x58));

            vehicle->tunnel_angle = 0;
            vehicle->field_76 = 0;
            vehicle->field_7E = 0;
            vehicle->field_80 = 0;
            vehicle->field_82 = 0;
            vehicle->field_84 = 0;
            vehicle->field_86 = 0;

            for (int32_t ci = 0; ci < 3; ci++) {
                void* dd = dplayData[ci];
                if (dd != NULL) {
                    inline_memcpy(
                        (uint8_t*)dd + 0x10,
                        (const uint8_t*)g_player_config + 6,
                        strlen((const char*)((uint8_t*)g_player_config + 6)) + 1);

                    *(uint16_t*)((uint8_t*)dd + 0x3A) = 0;

                    Vehicle_InitRoute(vehicle, 0x1871, 4, 1);

                    int32_t* nextHandle = *(int32_t**)((uint8_t*)node + 0x14 + (ci + 1) * 4);
                    VehicleEditor_SetDPlayData(nextHandle, (int32_t)(intptr_t)dd);

                    net_delete(dd);
                    dplayData[ci] = NULL;
                }
            }

            /* state field at +0x60 within vehicle_payload */
            *(int32_t*)((uint8_t*)vehicle + 0x60) = 2;
            Vehicle_SetState(vehicle, 0);
            vehicle->next = this->m_vehicleList;
            this->m_vehicleList = vehicle;
        }
    }

    this->HandleTimeout(node);

    return (hasStaleTrack == 0 && this->m_gameMode == 1) ? 1 : 0;
#endif
}

/* ================================================================== */
/* 25a. ResetNetworkState - 0x43EFA0                                  */
/* ================================================================== */
void Netman::ResetNetworkState()
{
    this->m_bFlag1 = 0;
    this->Init(0);
#ifndef _WIN32
    // The original queues type 5 to the Win32 Train worker. The host worker
    // owns transport teardown directly; requeueing onto the main-thread
    // Netman queue would feed the reset back into this same dispatcher.
    lego_loco::network::HostTransportWorker().StopTransport();
#else
    TrainMessage* message = allocate_train_message();
    if (message != nullptr) {
        message->type = 5;
    }
    Train_QueueMessage(_g_train, message);
#endif
}

/* ================================================================== */
/* 26a. StopSession - 0x43F070                                        */
/* ================================================================== */
void Netman::StopSession()
{
#ifndef _WIN32
    lego_loco::network::HostTransportWorker().StopTransport();
    this->HostEndTransportSession();
#else
    TrainMessage* message = allocate_train_message();
    if (message == nullptr) return;
    message->type = 0;
    message->data_ptr = reinterpret_cast<void*>(
        static_cast<uintptr_t>(*reinterpret_cast<uint8_t*>(_g_netman_data + 8) != 0));
    message->flags = this->m_playerSlotCount;
    Train_QueueMessage(_g_train, message);
#endif
}

/* ================================================================== */
/* 36. SendFileTransfer - 0x440150                                    */
/* ================================================================== */
void Netman::SendFileTransfer(TrainMessage* msg)
{
    if (msg->type == 0x15) {
        auto* packet = static_cast<uint8_t*>(msg->data_ptr);
        const uint8_t source_slot = packet[4];
        if (source_slot >= 9) return;
        if (packet[8] != 0) {
            auto* entry = static_cast<PingEntry*>(m_slots[source_slot].msg_queue);
            while (entry != nullptr) {
                PingEntry* next = static_cast<PingEntry*>(entry->next);
                GLOBAL_free(entry);
                entry = next;
            }
            m_slots[source_slot].msg_queue = nullptr;
        }
        const uint16_t count = *reinterpret_cast<uint16_t*>(packet + 6);
        const uint8_t* item = packet + 9;
        for (uint16_t index = 0; index < count; ++index, item += 8) {
            ReceivePing(*reinterpret_cast<const uint16_t*>(item), item[6], item[7],
                        *reinterpret_cast<const uint16_t*>(item + 2),
                        *reinterpret_cast<const uint16_t*>(item + 4));
        }
        HeapFree(GetProcessHeap(), 0, msg->data_ptr);
        msg->data_ptr = nullptr;
        return;
    }
    if (msg->type == 0x17) {
        auto* packet = static_cast<uint8_t*>(msg->data_ptr);
        RemovePingEntry(*reinterpret_cast<int32_t*>(packet + 4), packet[8], packet[9]);
        HeapFree(GetProcessHeap(), 0, msg->data_ptr);
        msg->data_ptr = nullptr;
        return;
    }
    if (msg->type != 0x12) return;

    int32_t packed_offset = 0;
    const int32_t angle = msg->data_len;
    if (angle == 0x5A) {
        packed_offset = INPUT_DirToOffset_Left(&packed_offset);
        packed_offset += 0x10000;
    } else if (angle == 0) {
        packed_offset = INPUT_DirToOffset_Down(&packed_offset);
        packed_offset += 1;
    } else if (angle == 0xB4) {
        packed_offset = INPUT_DirToOffset_Right(&packed_offset);
        packed_offset += 1;
    } else if (angle == 0x10E) {
        packed_offset = INPUT_DirToOffset_Up(&packed_offset);
        packed_offset += 0x10000;
    }
    ReceivePing(msg->flags, msg->metadata0(), msg->metadata1(),
                static_cast<int16_t>(packed_offset),
                static_cast<int16_t>(
                    static_cast<uint32_t>(packed_offset) >> 16));
}

/* ================================================================== */
/* 37. ReceiveAck - 0x440410                                          */
/* ================================================================== */
void Netman::ReceiveAck(int32_t dpId, uint8_t slot_byte, uint32_t peerIndex)
{
    if (m_gameMode != 2) return;
    if (m_mySlotIndex == static_cast<int32_t>(peerIndex & 0xff)) {
        auto* packet = static_cast<uint8_t*>(operator_new(0x0c));
        *reinterpret_cast<uint16_t*>(packet) = 0x3f7;
        *reinterpret_cast<int32_t*>(packet + 4) = dpId;
        packet[8] = slot_byte;
        packet[9] = static_cast<uint8_t>(peerIndex);
        TrainMessage* message = allocate_train_message();
        if (message == nullptr) {
            GLOBAL_free(packet);
            return;
        }
        message->type = 6;
        message->data_len = 0x0c;
        message->data_ptr = packet;
        message->flags = 1;
        Train_QueueMessage(_g_train, message);
    }
    RemovePingEntry(dpId, slot_byte, peerIndex);
}

/* ================================================================== */
/* 38. RemovePingEntry - 0x4404C0                                     */
/* ================================================================== */
void Netman::RemovePingEntry(int32_t dpId, uint8_t slot_byte, uint32_t peerIndex)
{
    const auto remove_from = [&](uint8_t index) -> bool {
        if (index >= 9) return false;
        auto** head = reinterpret_cast<PingEntry**>(&m_slots[index].msg_queue);
        PingEntry* previous = nullptr;
        for (PingEntry* entry = *head; entry != nullptr;
             previous = entry, entry = static_cast<PingEntry*>(entry->next)) {
            if (entry->dpId == dpId && entry->peer_index == slot_byte) {
                if (previous == nullptr) *head = static_cast<PingEntry*>(entry->next);
                else previous->next = entry->next;
                GLOBAL_free(entry);
                return true;
            }
        }
        return false;
    };
    const uint8_t preferred = static_cast<uint8_t>(peerIndex);
    if (remove_from(preferred) || remove_from(slot_byte)) return;
    for (uint8_t index = 0; index < 9; ++index)
        if (index != preferred && index != slot_byte && remove_from(index)) return;
}

/* ================================================================== */
/* 39. ReceivePing - 0x440610                                         */
/* ================================================================== */
void Netman::ReceivePing(int32_t dpId, uint8_t slot_byte,
                          uint32_t peerIndex, int32_t posX, int32_t posY)
{
    if (m_gameMode != 2) return;
    const uint8_t destination = static_cast<uint8_t>(peerIndex);
    if (destination >= 9) return;
    PingEntry* entry = UpdateLatency(dpId, slot_byte, peerIndex);
    if (entry == nullptr) {
        entry = static_cast<PingEntry*>(operator_new(sizeof(PingEntry)));
        std::memset(entry, 0, sizeof(*entry));
        entry->dpId = dpId;
        entry->pos_x = posX;
        entry->pos_y = posY;
        entry->peer_index = slot_byte;
        entry->slot_index = destination;
        entry->next = m_slots[destination].msg_queue;
        m_slots[destination].msg_queue = entry;
#ifndef _WIN32
        loco::host_test::emit_netman_ping_updated(
            dpId, slot_byte, destination, posX, posY);
#endif
        return;
    }
    entry->dpId = dpId;
    entry->pos_x = posX;
    entry->pos_y = posY;
    if (entry->slot_index == destination) {
#ifndef _WIN32
        loco::host_test::emit_netman_ping_updated(
            dpId, slot_byte, destination, posX, posY);
#endif
        return;
    }

    const uint8_t old_index = entry->slot_index;
    if (old_index < 9) {
        auto** head = reinterpret_cast<PingEntry**>(&m_slots[old_index].msg_queue);
        PingEntry* previous = nullptr;
        for (PingEntry* cursor = *head; cursor != nullptr;
             previous = cursor, cursor = static_cast<PingEntry*>(cursor->next)) {
            if (cursor == entry) {
                if (previous == nullptr) *head = static_cast<PingEntry*>(entry->next);
                else previous->next = entry->next;
                break;
            }
        }
    }
    entry->slot_index = destination;
    entry->next = m_slots[destination].msg_queue;
    m_slots[destination].msg_queue = entry;
#ifndef _WIN32
    loco::host_test::emit_netman_ping_updated(
        dpId, slot_byte, destination, posX, posY);
#endif
}

/* ================================================================== */
/* 40. UpdateLatency - 0x440750                                       */
/* ================================================================== */
PingEntry* Netman::UpdateLatency(int32_t dpId, uint8_t slot_byte, uint32_t peerIndex)
{
    const auto find_in = [&](uint8_t index) -> PingEntry* {
        if (index >= 9) return nullptr;
        for (auto* entry = static_cast<PingEntry*>(m_slots[index].msg_queue);
             entry != nullptr; entry = static_cast<PingEntry*>(entry->next))
            if (entry->dpId == dpId && entry->peer_index == slot_byte) return entry;
        return nullptr;
    };
    const uint8_t preferred = static_cast<uint8_t>(peerIndex);
    if (PingEntry* entry = find_in(preferred)) return entry;
    if (PingEntry* entry = find_in(slot_byte)) return entry;
    for (uint8_t index = 0; index < 9; ++index)
        if (index != preferred && index != slot_byte)
            if (PingEntry* entry = find_in(index)) return entry;
    return nullptr;
}

/* ================================================================== */
/* 41. CheckTimeout - 0x440820                                        */
/* ================================================================== */
void Netman::CheckTimeout(int32_t timeoutVal)
{
    if (this->m_timeoutState == timeoutVal) return;
#ifdef _WIN32
    extern Collection DAT_004a9994; // in-place collection at 0x4A9994
    const int32_t count = std::min(g_object_count, DAT_004a9994.count);
    for (int32_t index = 0; index < count; ++index) {
        auto* object = static_cast<Entity*>(DAT_004a9994.items[index]);
        if (object == nullptr || object->resource == nullptr) continue;
        const int32_t resource_id = *reinterpret_cast<const int32_t*>(
            static_cast<const uint8_t*>(object->resource) + 4);
        if (resource_id == 0xC5C || resource_id == 0xC5E ||
            resource_id == 0xC60) {
            object->StopSound(timeoutVal);
        }
    }
#endif
    // The SDL host has no mode-3 world objects yet; retain the state so the
    // future world adapter can apply it when materializing signal objects.
    this->m_timeoutState = timeoutVal;
}

/* ================================================================== */
/* 42. HandleTimeout - 0x4408B0                                       */
/* ================================================================== */
void Netman::HandleTimeout(InboundTrainNode* node)
{
    if (node == nullptr) return;
    bool registered_local_route = false;
    for (uint16_t index = 1; index <= node->editor_count && index < 4;
         ++index) {
        VehicleEditor* editor = node->editors[index];
        if (editor == nullptr) continue;
        DPlayManager* dplay = editor->GetDPlayData();
        if (dplay == nullptr || g_player_config == nullptr) continue;
        if (std::strcmp(reinterpret_cast<const char*>(dplay->m_sessionBlk1),
                        g_player_config->name) == 0) {
            NET_RegisterPlayer(_g_dplay, dplay, 1, 0);
            editor->SetDPlayData(nullptr);
            registered_local_route = true;
        }
    }

    bool has_dplay_data = false;
    for (uint16_t index = 1; index <= node->editor_count && index < 4;
         ++index) {
        VehicleEditor* editor = node->editors[index];
        if (editor != nullptr && editor->GetDPlayData() != nullptr) {
            has_dplay_data = true;
            break;
        }
    }
    const int32_t desired_resource = has_dplay_data ? 0x1871 : 0x1870;
    for (uint16_t index = 1; index <= node->editor_count && index < 4;
         ++index) {
        VehicleEditor* editor = node->editors[index];
        if (editor == nullptr) continue;
        const int32_t resource_id = static_cast<int32_t>(editor->GetResourceId());
        if ((has_dplay_data && resource_id == 0x1870) ||
            (!has_dplay_data && resource_id == 0x1871)) {
#ifndef _WIN32
            editor->res_id = desired_resource;
#else
            CarObject* car = static_cast<CarObject*>(static_cast<void*>(editor));
            car->SetResourceId(desired_resource, -1);
            car->SetParam(editor->frame_index, 1);
#endif
        }
    }
    if (registered_local_route) {
        if (this->m_timeoutState == 1) this->CheckTimeout(3);
        else if (this->m_timeoutState == 0) this->CheckTimeout(2);
    }
}

/* ================================================================== */
/* 43. SerializePlayerData - 0x440A50                                 */
/* ================================================================== */
void Netman::SerializePlayerData(InboundTrainNode* node)
{
    this->HandleTimeout(node);
    if (node != nullptr && node->owner_handle != 1)
        this->DeserializePlayerData(node);
#ifndef _WIN32
    this->m_hostLastSerializedVehicle = node;
#else
    this->m_field_7E4 = static_cast<int32_t>(reinterpret_cast<intptr_t>(node));
#endif
}

/* ================================================================== */
/* 44. DeserializePlayerData - 0x440A80                               */
/* ================================================================== */
void Netman::DeserializePlayerData(InboundTrainNode* node)
{
#ifndef _WIN32
    if (node == nullptr) return;
    const DPlayManager* donor = nullptr;
    for (uint16_t index = 1; index <= node->editor_count && index < 4;
         ++index) {
        VehicleEditor* editor = node->editors[index];
        if (editor != nullptr && editor->GetDPlayData() != nullptr) {
            donor = editor->GetDPlayData();
            break;
        }
    }
    if (donor == nullptr) return;

    bool assigned = false;
    for (uint16_t index = 1; index <= node->editor_count && index < 4;
         ++index) {
        VehicleEditor* editor = node->editors[index];
        if (editor == nullptr || editor->GetDPlayData() != nullptr) continue;
        const int32_t resource_id = static_cast<int32_t>(editor->GetResourceId());
        if (resource_id != 0x1870 && resource_id != 0x1871) continue;
        void* storage = operator_new(sizeof(DPlayManager));
        if (storage == nullptr) break;
        auto* replacement = ::new (storage) DPlayManager;
        replacement->CreatePlayer();
        replacement->CopyLogicalStateFrom(*donor);
        replacement->SetPlayerName(1, -1);
        if (editor->SetDPlayData(replacement)) {
            editor->res_id = 0x1871;
            assigned = true;
        }
        replacement->~DPlayManager();
        GLOBAL_free(replacement);
    }
    if (assigned) {
        if (this->m_timeoutState == 3) this->CheckTimeout(2);
        else this->CheckTimeout(0);
    }
#else
    // Original 0x440A80 enumerates PostBag Sort_Out .crd records through
    // NET_GetHostName/NET_ResolveAddress. The Windows reconstruction retains
    // that filesystem behavior in its untranslated path.
    (void)node;
#endif
}

/* ================================================================== */
/* Standalone helper functions                                         */
/* ================================================================== */

/* NETMAN_SendDisconnect - 0x43D250 */
void NETMAN_SendDisconnect(int32_t dpId)
{
    uint16_t* pkt = static_cast<uint16_t*>(operator_new(4));
    if (pkt == nullptr) return;
    *pkt = PACKET_DISCONNECT;
    TrainMessage* m = allocate_train_message();
    if (m == nullptr) { GLOBAL_free(pkt); return; }
    m->type = 6;
    m->data_len = 4;
    m->data_ptr = pkt;
    m->target_dpId = dpId;
    m->flags = 1;
    Train_QueueMessage(_g_train, m);
}

/* NETMAN_QueueMessage - 0x43F140 */
void NETMAN_QueueMessage(TrainMessage* msg)
{
    if (g_game_mode != 10) {
        msg->next = NULL;
        if (!g_network_queue) {
            g_network_queue = msg;
        } else {
            TrainMessage* n = static_cast<TrainMessage*>(g_network_queue);
            while (n->next) n = static_cast<TrainMessage*>(n->next);
            n->next = msg;
        }
    } else {
        void* d = msg->data_ptr;
        if (d) {
            switch (msg->type) {
            case 2: {
                void* sub = d;
                while (sub) {
                    void* nx = *reinterpret_cast<void**>(sub);
                    void** sub_data = reinterpret_cast<void**>(
                        reinterpret_cast<uint8_t*>(sub) + 8);
                    if (*sub_data != nullptr) {
                        GLOBAL_free(*sub_data);
                        *sub_data = nullptr;
                    }
                    GLOBAL_free(sub);
                    sub = nx;
                }
                msg->data_ptr = nullptr;
                GLOBAL_free(msg);
                return;
            }
            case 0x0F: case 0x11:
                net_delete(d);
                msg->data_ptr = NULL;
                break;
            case 0x15: case 0x17:
                HeapFree(GetProcessHeap(), 0, d);
                msg->data_ptr = nullptr;
                break;
            default:
                GLOBAL_free(d);
                break;
            }
            msg->data_ptr = nullptr;
        }
        GLOBAL_free(msg);
    }
}

/* NETMAN_StartHostSession - 0x43F000 */
void NETMAN_StartHostSession()
{
    TrainMessage* m = allocate_train_message();
    if (m == nullptr) return;
    m->type = 3;
    m->data_len = 0;
    m->data_ptr = nullptr;
    m->target_dpId = 0;
    m->flags = 0;
    m->next = nullptr;
    Train_QueueMessage(_g_train, m);
}

/* NETMAN_StartClientSession - 0x43F030 */
void NETMAN_StartClientSession()
{
    TrainMessage* m = allocate_train_message();
    if (m == nullptr) return;
    m->type = 1;
    const auto* host_info = reinterpret_cast<const uint8_t*>(g_net_host_info);
    m->data_len = static_cast<uint32_t>(host_info[8]);
    m->data_ptr = nullptr;
    m->target_dpId = 0;
    m->flags = 0;
    m->next = nullptr;
    Train_QueueMessage(_g_train, m);
}

/* NETMAN_SendTrainPosition - 0x43EE80 */
uint32_t NETMAN_SendTrainPosition(InboundTrainNode* vehicle)
{
    TrainMessage* m = allocate_train_message();
    if (m == nullptr) return 0;
    m->data_len = 0;
    m->flags = 0;
    m->type = 0x0E;
    m->data_ptr = vehicle;
    m->next = NULL;
    vehicle->process_delay = 1;
    Train_QueueMessage(_g_train, m);
    return 1;
}

/* NETMAN_ReceiveTrainPosition - 0x43EEC0 */
int32_t NETMAN_ReceiveTrainPosition(int p1, int p2, int p3)
{
    int32_t a;
    if (p2 == 0) a = 0;
    else if (p1 == 0) a = 0x10E;
    else a = ((p1 <= p2) - 1 & 0xFFFFFFA6) + 0xB4;

    /* Use 32-bit zero test instead of truncation to uint8_t */
    int32_t r = static_cast<Netman*>(_g_netman)->CheckTrackConnection(a, -1);
    if (r == 0) {
        a = 0;
        r = static_cast<Netman*>(_g_netman)->CheckTrackConnection(0, -1);
        if (r == 0) {
            a = 0x10E;
            r = static_cast<Netman*>(_g_netman)->CheckTrackConnection(0x10E, -1);
            if (r == 0) {
                a = 0x5A;
                r = static_cast<Netman*>(_g_netman)->CheckTrackConnection(0x5A, -1);
                if (r == 0) return r;
            }
        }
    }

    TrainMessage* m = allocate_train_message();
    if (m == nullptr) return 0;
    m->data_len = 0;
    m->flags = 0;
    m->type = 0x10;
    m->data_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(p3));
    m->target_dpId = a;
    m->next = nullptr;

    InboundTrainNode* node = reinterpret_cast<InboundTrainNode*>(
        static_cast<intptr_t>(p3));
    node->tunnel_angle = static_cast<uint16_t>(a);
    node->process_delay = 1;
    Train_QueueMessage(_g_train, m);
    return 1;
}

/* ================================================================== */
/* NETMAN_ReceiveLayoutSelect - 0x440070                              */
/* ================================================================== */
void NETMAN_ReceiveLayoutSelect(Netman* netman)
{
    if (netman == nullptr || _g_train == nullptr) return;
    constexpr int32_t packet_size = 0x228;
    auto* packet = static_cast<uint8_t*>(operator_new(packet_size));
    if (packet == nullptr) return;
    std::memset(packet, 0, packet_size);
    *reinterpret_cast<uint16_t*>(packet) = 0x3F1;
    *reinterpret_cast<int32_t*>(packet + 4) = netman->m_playerSlotCount;
    packet[8] = static_cast<uint8_t>(netman->m_playerRows);
    packet[9] = static_cast<uint8_t>(netman->m_playerCols);
    for (int32_t index = 0; index < 9; ++index) {
        DPLAY_FreePlayerSlot(packet + 0x0C + index * 0x3C,
                            reinterpret_cast<const int32_t*>(
                                &netman->m_slots[index]));
    }
    TrainMessage* message = net_new_message();
    if (message == nullptr) {
        GLOBAL_free(packet);
        return;
    }
    message->type = 6;
    message->data_len = packet_size;
    message->data_ptr = packet;
    message->target_dpId = 0;
    message->flags = 1;
    Train_QueueMessage(_g_train, message);

#ifdef _WIN32
    void* panel = g_ui_main != nullptr
        ? *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ui_main) + 0x220)
        : nullptr;
    if (panel != nullptr && IsWindowVisible(*reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(panel) + 8))) {
        CGWND_GameSetup_DrawGrid_Thunk(panel);
        UIPANEL_EndPaintEx(panel,
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(panel) + 8),
            0, 0, nullptr);
    }
#endif
}

/* ================================================================== */
/* NETMAN_ReceiveFileTransfer - 0x440310                              */
/* ================================================================== */
void NETMAN_ReceiveFileTransfer(Netman* netman)
{
    if (netman == nullptr || _g_train == nullptr) return;
    if (netman->m_gameMode == 2 && netman->m_currentSlot != nullptr)
        netman->m_currentSlot->flag_36 = 1;
    auto* packet = static_cast<uint8_t*>(operator_new(4));
    if (packet == nullptr) return;
    std::memset(packet, 0, 4);
    *reinterpret_cast<uint16_t*>(packet) = 0x3F4;
    TrainMessage* message = net_new_message();
    if (message == nullptr) {
        GLOBAL_free(packet);
        return;
    }
    message->type = 6;
    message->data_len = 4;
    message->data_ptr = packet;
    message->flags = 1;
    Train_QueueMessage(_g_train, message);
}

/* ================================================================== */
/* NETMAN_SendAck - 0x440390                                          */
/* ================================================================== */
void NETMAN_SendAck(Netman* netman)
{
    if (netman == nullptr) return;
    if (netman->m_gameMode == 2 && netman->m_currentSlot != nullptr)
        netman->m_currentSlot->flag_36 = 0;
    if (_g_train == nullptr) return;
    auto* packet = static_cast<uint8_t*>(operator_new(4));
    if (packet == nullptr) return;
    std::memset(packet, 0, 4);
    *reinterpret_cast<uint16_t*>(packet) = 0x3F5;
    TrainMessage* message = net_new_message();
    if (message == nullptr) {
        GLOBAL_free(packet);
        return;
    }
    message->type = 6;
    message->data_len = 4;
    message->data_ptr = packet;
    message->flags = 1;
    Train_QueueMessage(_g_train, message);
}
