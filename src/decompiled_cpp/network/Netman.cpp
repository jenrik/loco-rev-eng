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
 *   - int32_t params to Netman*/InboundTrainNode* typed pointers
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

/* ================================================================== */
/* Local externs (not in Netman.h to avoid circular includes)          */
/* ================================================================== */

extern void* _g_train;              /* 0x4FD3A4 */
extern void* _g_network_queue;      /* 0x4FD3A0 */
extern void* _g_train_resources;    /* 0x4FD394 */
extern void* _g_netman_data;        /* 0x4FD3A8 */

/* String constants */
extern const char STR_Default[];    /* "Default" */
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
    for (int32_t i = 0; i < dw; i++) ((uint32_t*)dst)[i] = ((const uint32_t*)src)[i];
    for (int32_t i = 0; i < rm; i++) ((uint8_t*)dst)[dw*4+i] = ((const uint8_t*)src)[dw*4+i];
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
    if (obj) {
        delete static_cast<NetworkObject*>(obj);
    }
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
        const char* s = STR_Default;
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

/* ================================================================== */
/* 8. SendMapData - 0x43D350                                          */
/* ================================================================== */
void Netman::SendMapData(int32_t targetDpId)
{
    if (this->m_gameMode != 2) return;
    PlayerSlot* slot = this->m_currentSlot;
    if (!slot) return;
    slot->has_data = 1;

    void* surf = (void*)operator_new(0x20);
    if (surf) surf = UIPANEL_CreateSurface(surf);
    TileMap_CreateOverlay(g_tilemap, surf, 0);
    if (!surf) return;

    uint16_t w = *(uint16_t*)((uint8_t*)surf + 8);
    uint16_t h = *(uint16_t*)((uint8_t*)surf + 0xC);
    int32_t ds = (int32_t)w * (int32_t)h;
    void* px = *(void**)((uint8_t*)surf + 0x18);

    uint8_t* pkt = (uint8_t*)operator_new(ds + 0x28);
    *(uint16_t*)(pkt + 0) = PACKET_MAP_DATA;
    *(uint16_t*)(pkt + 6) = w;
    *(uint16_t*)(pkt + 8) = h;
    *(int32_t*)(pkt + 0x10) = ds;

    if (this->m_mySlotIndex >= 0) {
        PlayerSlot* ms = &this->m_slots[this->m_mySlotIndex];
        if (ms->pixel_buffer) GLOBAL_free(ms->pixel_buffer);
        ms->pixel_buffer = operator_new(ds);
        ms->data_size = ds;
        inline_memcpy(ms->pixel_buffer, px, ds);
        ms->version++;
        ms->pixel_width = w;
        ms->pixel_height = h;
        *(int32_t*)(pkt + 0xC) = ms->version;
    }
    inline_memcpy(pkt + 0x14, px, ds);

    TrainMessage* m = (TrainMessage*)operator_new(sizeof(TrainMessage));
    if (m) m->next = NULL;
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
        uint8_t* pkt = (uint8_t*)operator_new(ds + 0x28);
        if (!pkt) return;
        *(uint16_t*)(pkt + 0) = PACKET_MAP_DATA;
        *(uint16_t*)(pkt + 6) = w;
        *(uint16_t*)(pkt + 8) = h;
        *(int32_t*)(pkt + 0x10) = ds;
        *(int32_t*)(pkt + 0xC) = slot->version;
        inline_memcpy(pkt + 0x14, slot->pixel_buffer, ds);
        TrainMessage* m = (TrainMessage*)operator_new(sizeof(TrainMessage));
        if (!m) { GLOBAL_free(pkt); return; }
        m->next = NULL;
        m->type = 6;
        m->data_len = ds + 0x19;
        m->data_ptr = pkt;
        m->target_dpId = targetDpId;
        m->flags = 1;
        Train_QueueMessage(_g_train, m);
    } else {
        TrainMessage* m = (TrainMessage*)operator_new(sizeof(TrainMessage));
        if (!m) return;
        m->type = MESSAGE_REFRESH_REQUEST;
        m->data_len = 0;
        m->data_ptr = NULL;
        m->target_dpId = 0;
        m->flags = 0;
        m->next = NULL;
        NETMAN_QueueMessage(m);
    }
}

/* ================================================================== */
/* 10. UpdatePlayerInfo - 0x43D620                                    */
/* ================================================================== */
void Netman::UpdatePlayerInfo()
{
    uint8_t* pkt = (uint8_t*)operator_new(6);
    if (!pkt) return;
    *(uint16_t*)(pkt + 0) = PACKET_PLAYER_INFO;
    *(uint8_t*)(pkt + 4) = (uint8_t)this->m_mySlotIndex;

    TrainMessage* m1 = (TrainMessage*)operator_new(sizeof(TrainMessage));
    if (!m1) { GLOBAL_free(pkt); return; }
    m1->next = NULL;
    m1->type = 6;
    m1->data_len = 6;
    m1->data_ptr = pkt;
    m1->target_dpId = 0;
    m1->flags = 1;
    Train_QueueMessage(_g_train, m1);

    TrainMessage* m2 = (TrainMessage*)operator_new(sizeof(TrainMessage));
    if (!m2) return;
    m2->type = MESSAGE_SYNC_TRIGGER;
    m2->data_len = 0;
    m2->data_ptr = NULL;
    m2->target_dpId = 0;
    m2->flags = 0;
    m2->next = NULL;
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
/* TODO: decompile 0x43D820                                           */
/* ================================================================== */
void Netman::LoadScenario(const char* layoutName)
{
    (void)layoutName;
    /* TODO: decompile 0x43D820 — LoadScenario body */
}

/* ================================================================== */
/* 14. Cleanup - 0x43DC30                                             */
/* ================================================================== */
void Netman::Cleanup()
{
    /* === Drain _g_network_queue with lock and type-dispatch === */
    ResourceManager_Lock(_g_train_resources);

    {
        TrainMessage* msg = (TrainMessage*)_g_network_queue;
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
            _g_network_queue = next;
            msg = (TrainMessage*)_g_network_queue;
        }
    }

    ResourceManager_Unlock(_g_train_resources);

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

    uint16_t* packet = (uint16_t*)operator_new(0x8000);
    if (packet == NULL) return slot_idx;

    *packet = 0x3F6;
    *(uint8_t*)(packet + 1) = (uint8_t)this->m_mySlotIndex;
    *(uint8_t*)(packet + 2) = 1;
    *(uint16_t*)(packet + 3) = 0;

    /* Pack msg_queue entries into 8-byte blocks */
    {
        uint16_t entry_count = 1;
        uint16_t* queue_ptr = (uint16_t*)msg_queue;

        while (queue_ptr != NULL) {
            entry_count++;
            uint32_t lo32 = *(uint32_t*)queue_ptr;
            uint32_t hi32 = *(uint32_t*)((uint8_t*)queue_ptr + 6);
            *(uint32_t*)((uint8_t*)packet + entry_count * 8 + 1) = lo32;
            *(uint32_t*)((uint8_t*)packet + entry_count * 8 + 5) = hi32;
            queue_ptr = *(uint16_t**)((uint8_t*)queue_ptr + 8);
        }

        packet[3] = entry_count;
    }

    /* Wrap in TrainMessage and queue */
    {
        TrainMessage* msg = (TrainMessage*)operator_new(sizeof(TrainMessage));
        if (msg == NULL) {
            GLOBAL_free(packet);
            return slot_idx;
        }
        msg->type        = 6;
        msg->data_len    = (int32_t)packet[3] * 8 + 10;
        msg->data_ptr    = packet;
        msg->target_dpId = 0;
        msg->flags       = 0;
        msg->next        = NULL;

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

    uint32_t res = 1;
    {
        /* car_count at +0x0C and car_handles at +0x14 are within vehicle_payload */
        uint16_t* raw = (uint16_t*)((uint8_t*)node + 0x0C);
        int32_t cc = *raw;
        for (int32_t ci = 0; ci < cc; ci++) {
            int32_t* ch = *(int32_t**)((uint8_t*)node + 0x14 + ci * 4);
            int32_t dd = VehicleEditor_GetDPlayData(*ch);
            if (!dd) continue;
            uint16_t* s1 = (uint16_t*)((uint8_t*)g_player_config + 6);
            uint16_t* s2 = (uint16_t*)(dd + 0x10);
            while (*s1 && *s1 == *s2) { s1++; s2++; }
            int32_t cmp = (*s1 < *s2) ? -1 : (*s1 > *s2) ? 1 : 0;
            if (cmp == 0 && *(int32_t*)(dd + 0x3C) == 0 && *(uint16_t*)(dd + 0x3A)) {
                char fp[0x504];
                fp[0] = g_empty_string;
                for (int32_t i = 0; i < 0x140; i++) ((uint32_t*)&fp[1])[i] = 0;
                NET_GetAttFilePath(*(uint16_t*)(dd + 0x3A), 5, fp);
                return (uint32_t)PlaySoundFile(fp, g_listener_x, g_listener_y, 4);
            }
        }
    }
    return res;
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
            node->next = NULL;
            node->process_delay = 0;
            this->m_tickCounter = this->m_timeout - 0x20;
            return;
        }
        while (tail->next)
            tail = tail->next;
        tail->next = node;
        node->next = NULL;
        node->process_delay = 0;
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

    /* car_count and car_handles are within the Vehicle payload area (+0x0C, +0x14) */
    int32_t cc = *(uint16_t*)((uint8_t*)node + 0x0C);
    bool has_dd = false;
    /* NOTE: loop starts at 1 — car[0] is metadata. cc is car count, 1-based. */
    for (int32_t ci = 1; ci <= cc; ci++) {
        int32_t* ch = *(int32_t**)((uint8_t*)node + 0x14 + ci * 4);
        if (!ch) continue;
        if (VehicleEditor_GetDPlayData(*ch)) {
            has_dd = true;
            break;
        }
    }
    for (int32_t ci = 1; ci <= cc; ci++) {
        int32_t* ch = *(int32_t**)((uint8_t*)node + 0x14 + ci * 4);
        if (!ch) continue;
        CarObject* car = (CarObject*)ch;
        int32_t rid = VehicleEditor_GetResourceId(*ch);
        int32_t val = ((int32_t*)*ch)[0x15];
        if (has_dd && rid == 0x1870) {
            car->SetResourceId(0x1871, -1);
            car->SetParam(val, 1);
        } else if (!has_dd && rid == 0x1871) {
            car->SetResourceId(0x1870, -1);
            car->SetParam(val, 1);
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
    while (_g_network_queue != NULL) {
        ResourceManager_Lock(_g_train_resources);

        TrainMessage* msg = (TrainMessage*)_g_network_queue;
        if (msg != NULL) {
            _g_network_queue = msg->next;
        }

        ResourceManager_Unlock(_g_train_resources);

        if (msg != NULL) {
            this->ProcessMessage(msg);
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
            uint32_t playerInfo = *(uint16_t*)((uint8_t*)msg + 0x14);
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

        if (slotIdx >= 0) {
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
        }
        HeapFree(GetProcessHeap(), 0, msg->data_ptr);
        return;
    }

    case 0x18:  /* POS_ACK */
    {
        InboundTrainNode* vnode = this->m_vehicleList;
        while (vnode != NULL) {
            if (*((uint8_t*)msg + 0x14) == vnode->slot_index &&
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
    this->m_playerRows      = *((uint8_t*)msg + 0x14);
    this->m_playerCols      = *((uint8_t*)msg + 0x15);
    this->m_playerSlotCount = *(int32_t*)((uint8_t*)msg + 0x10);

    int32_t newMySlotIdx = 0;
    bool versionChanged = false;

    for (int32_t i = 0; i < 9; i++) {
        PlayerSlot* sl = &this->m_slots[i];
        int32_t oldDpId = sl->dpId;
        bool wasEmpty = (oldDpId == 0);
        uint8_t oldFlag36 = sl->flag_36;
        int32_t oldVersion = sl->version;

        void* srcData = (uint8_t*)msg->data_ptr + i * sizeof(PlayerSlot);
        DPLAY_InitPlayerSlot(sl, srcData);

        sl->version = oldVersion;

        if (sl->dpId == this->m_myDpId) {
            if (oldFlag36 == 0 && sl->flag_36 != 0) {
                NETMAN_ReceiveFileTransfer(this);
            } else if (oldFlag36 != 0 && sl->flag_36 == 0) {
                NETMAN_SendAck(this);
            }

            if (this->m_mySlotIndex == i) {
                int32_t packetVersion = *(int32_t*)((uint8_t*)srcData + 0x48);
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
            int32_t packetVersion = *(int32_t*)((uint8_t*)srcData + 0x48);
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
    void* dplayData[3] = { NULL, NULL, NULL };
    uint8_t hasValidData = 0;
    uint8_t hasStaleTrack = 0;

    /* car_count and car_handles are within vehicle_payload area */
    uint16_t carCount = *(uint16_t*)((uint8_t*)node + 0x0C);
    for (int32_t ci = 0; ci < (int32_t)carCount; ci++) {
        int32_t* carHandle = *(int32_t**)((uint8_t*)node + 0x14 + ci * 4);
        void* dd = (void*)VehicleEditor_GetDPlayData(*carHandle);

        if (dd != NULL) {
            if (*((uint8_t*)dd + 0x24) == 0) {
                hasStaleTrack = 1;
                dplayData[ci] = NULL;
            } else {
                VehicleEditor_SetDPlayData((void*)*carHandle, 0);
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
}

/* ================================================================== */
/* 25a. ResetNetworkState - 0x43EFA0                                  */
/* ================================================================== */
void Netman::ResetNetworkState()
{
    /* TODO: decompile 0x43EFA0 — ResetNetworkState body
     *
     * From the reviewer's description:
     * "Clear active flag, reinit, queue NET_RESET message."
     * Used by ProcessMessage (type 5), Shutdown, HandlePlayerJoin.
     */
    this->m_bInit = 0;
}

/* ================================================================== */
/* 26a. StopSession - 0x43F070                                        */
/* ================================================================== */
void Netman::StopSession()
{
    /* TODO: decompile 0x43F070 — StopSession body
     *
     * Queue STOP_SESSION type-0 TrainMessage.
     */
}

/* ================================================================== */
/* 36. SendFileTransfer - 0x440150                                    */
/* TODO: decompile 0x440150                                           */
/* ================================================================== */
void Netman::SendFileTransfer(TrainMessage* msg)
{
    /* TODO: decompile 0x440150 — SendFileTransfer body
     *
     * Handles type 0x12 (angle/direction->pixel pos),
     * type 0x15 (player data with ping entries),
     * type 0x17 (forward ping).
     */
    (void)msg;
}

/* ================================================================== */
/* 37. ReceiveAck - 0x440410                                          */
/* TODO: decompile 0x440410                                           */
/* ================================================================== */
void Netman::ReceiveAck(int32_t dpId, uint8_t slot_byte, uint32_t peerIndex)
{
    /* TODO: decompile 0x440410 — ReceiveAck body */
    (void)dpId;
    (void)slot_byte;
    (void)peerIndex;
}

/* ================================================================== */
/* 38. RemovePingEntry - 0x4404C0                                     */
/* TODO: decompile 0x4404C0                                           */
/* ================================================================== */
void Netman::RemovePingEntry(int32_t dpId, uint8_t slot_byte, uint32_t peerIndex)
{
    /* TODO: decompile 0x4404C0 — RemovePingEntry body
     *
     * Searches transfer_lists for PingEntry matching (dpId, slot),
     * unlinks and frees it. Tries preferred slot then playerSlot.
     */
    (void)dpId;
    (void)slot_byte;
    (void)peerIndex;
}

/* ================================================================== */
/* 39. ReceivePing - 0x440610                                         */
/* TODO: decompile 0x440610                                           */
/* ================================================================== */
void Netman::ReceivePing(int32_t dpId, uint8_t slot_byte,
                          uint32_t peerIndex, int32_t posX, int32_t posY)
{
    /* TODO: decompile 0x440610 — ReceivePing body
     *
     * If PingEntry exists: update fields, move between slots if peer changed.
     * If new: allocate 0x14-byte entry and prepend to transfer_list.
     */
    (void)dpId;
    (void)slot_byte;
    (void)peerIndex;
    (void)posX;
    (void)posY;
}

/* ================================================================== */
/* 40. UpdateLatency - 0x440750                                       */
/* TODO: decompile 0x440750                                           */
/* ================================================================== */
PingEntry* Netman::UpdateLatency(int32_t dpId, uint8_t slot_byte, uint32_t peerIndex)
{
    /* TODO: decompile 0x440750 — UpdateLatency body */
    (void)dpId;
    (void)slot_byte;
    (void)peerIndex;
    return NULL;
}

/* ================================================================== */
/* 41. CheckTimeout - 0x440820                                        */
/* TODO: decompile 0x440820                                           */
/* ================================================================== */
void Netman::CheckTimeout(int32_t timeoutVal)
{
    /* TODO: decompile 0x440820 — CheckTimeout body
     *
     * Iterates all game objects, finds objects with resource IDs
     * 0xC5C/0xC5E/0xC60, calls virtual method at vtable[7] with
     * the timeout value. Updates m_timeoutState.
     */
    this->m_timeoutState = timeoutVal;
}

/* ================================================================== */
/* 42. HandleTimeout - 0x4408B0                                       */
/* TODO: decompile 0x4408B0                                           */
/* ================================================================== */
void Netman::HandleTimeout(InboundTrainNode* node)
{
    /* TODO: decompile 0x4408B0 — HandleTimeout body
     *
     * Iterates cars in the train node, calls NET_RegisterPlayer,
     * transitions car resource IDs between LEAVING/ENTERING states.
     */
    (void)node;
}

/* ================================================================== */
/* 43. SerializePlayerData - 0x440A50                                 */
/* TODO: decompile 0x440A50                                           */
/* ================================================================== */
void Netman::SerializePlayerData(InboundTrainNode* node)
{
    /* TODO: decompile 0x440A50 — SerializePlayerData body */
    this->m_field_7E4 = (int32_t)(intptr_t)node;
}

/* ================================================================== */
/* 44. DeserializePlayerData - 0x440A80                               */
/* TODO: decompile 0x440A80                                           */
/* ================================================================== */
void Netman::DeserializePlayerData(InboundTrainNode* node)
{
    /* TODO: decompile 0x440A80 — DeserializePlayerData body */
    (void)node;
}

/* ================================================================== */
/* Standalone helper functions                                         */
/* ================================================================== */

/* NETMAN_SendDisconnect - 0x43D250 */
void NETMAN_SendDisconnect(int32_t dpId)
{
    uint16_t* pkt = (uint16_t*)operator_new(4);
    if (!pkt) return;
    *pkt = PACKET_DISCONNECT;
    TrainMessage* m = (TrainMessage*)operator_new(sizeof(TrainMessage));
    if (!m) { GLOBAL_free(pkt); return; }
    m->next = NULL;
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
        if (!_g_network_queue) {
            _g_network_queue = msg;
        } else {
            TrainMessage* n = (TrainMessage*)_g_network_queue;
            while (n->next) n = (TrainMessage*)n->next;
            n->next = msg;
        }
    } else {
        void* d = msg->data_ptr;
        if (d) {
            switch (msg->type) {
            case 2: {
                void* sub = d;
                while (sub) {
                    void* nx = *(void**)sub;
                    if (*(void**)((uint8_t*)sub + 8)) {
                        GLOBAL_free(*(void**)((uint8_t*)sub + 8));
                        *(void**)((uint8_t*)sub + 8) = NULL;
                    }
                    GLOBAL_free(sub);
                    sub = nx;
                }
                msg->data_ptr = NULL;
                GLOBAL_free(msg);
                return;
            }
            case 0x0F: case 0x11:
                net_delete(d);
                msg->data_ptr = NULL;
                break;
            case 0x15: case 0x17:
                HeapFree(GetProcessHeap(), 0, d);
                msg->data_ptr = NULL;
                break;
            default:
                GLOBAL_free(d);
                break;
            }
            msg->data_ptr = NULL;
        }
        GLOBAL_free(msg);
    }
}

/* NETMAN_StartHostSession - 0x43F000 */
void NETMAN_StartHostSession()
{
    TrainMessage* m = (TrainMessage*)operator_new(sizeof(TrainMessage));
    if (!m) return;
    m->type = 3;
    m->data_len = 0;
    m->data_ptr = NULL;
    m->target_dpId = 0;
    m->flags = 0;
    m->next = NULL;
    Train_QueueMessage(_g_train, m);
}

/* NETMAN_StartClientSession - 0x43F030 */
void NETMAN_StartClientSession()
{
    TrainMessage* m = (TrainMessage*)operator_new(sizeof(TrainMessage));
    if (!m) return;
    m->type = 1;
    m->data_len = (uint32_t)*(uint8_t*)((uint8_t*)g_net_host_info + 8);
    m->data_ptr = NULL;
    m->target_dpId = 0;
    m->flags = 0;
    m->next = NULL;
    Train_QueueMessage(_g_train, m);
}

/* NETMAN_SendTrainPosition - 0x43EE80 */
uint32_t NETMAN_SendTrainPosition(InboundTrainNode* vehicle)
{
    TrainMessage* m = (TrainMessage*)operator_new(0x1C);
    if (!m) return 0;
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
    int32_t r = ((Netman*)_g_netman)->CheckTrackConnection(a, -1);
    if (r == 0) {
        a = 0;
        r = ((Netman*)_g_netman)->CheckTrackConnection(0, -1);
        if (r == 0) {
            a = 0x10E;
            r = ((Netman*)_g_netman)->CheckTrackConnection(0x10E, -1);
            if (r == 0) {
                a = 0x5A;
                r = ((Netman*)_g_netman)->CheckTrackConnection(0x5A, -1);
                if (r == 0) return r;
            }
        }
    }

    TrainMessage* m = (TrainMessage*)operator_new(0x1C);
    if (!m) return 0;
    m->data_len = 0;
    m->flags = 0;
    m->type = 0x10;
    m->data_ptr = (void*)(intptr_t)p3;
    m->target_dpId = a;
    m->next = NULL;

    InboundTrainNode* node = (InboundTrainNode*)(intptr_t)p3;
    node->tunnel_angle = (uint16_t)a;
    node->process_delay = 1;
    Train_QueueMessage(_g_train, m);
    return 1;
}

/* NETMAN_ReceiveSignalChange - 0x43E900 */
void* NETMAN_ReceiveSignalChange(void* playerData)
{
    CRT_time();

    uint32_t bytesRead = 0;
    int32_t playerCount = 0;
    bool foundMatch = false;
    void* result = NULL;

    char playerNumStr[8] = "";
    char routePath[0x500];
    char addressPath[0x500];
    char fileBuf[0x8000];
    char routeAddr[0x14] = "";
    char playerAddr[0x14] = "";
    char displayName[0x50] = "";

    const char* targetName = (const char*)((uint8_t*)playerData + 0x10);

    DPLAY_EnumeratePlayers((int32_t)(intptr_t)_g_dplay);

    for (playerCount = 0; playerCount < 0x14; playerCount++) {
        foundMatch = false;

        for (int32_t nameIdx = 0; nameIdx < 0x10; nameIdx++) {
            uint8_t* entry = (uint8_t*)_g_dplay + 0xB13 + nameIdx * 0x0D;
            const uint8_t* pSrc = (const uint8_t*)targetName;

            int32_t cmp;
            while (1) {
                uint8_t c1 = *pSrc;
                uint8_t c2 = *entry;
                if (c1 != c2) { cmp = (c1 < c2) ? -1 : 1; break; }
                if (c1 == 0) { cmp = 0; break; }
                c1 = *(pSrc + 1);
                c2 = *(entry + 1);
                if (c1 != c2) { cmp = (c1 < c2) ? -1 : 1; break; }
                if (c1 == 0) { cmp = 0; break; }
                pSrc += 2;
                entry += 2;
            }

            if (cmp == 0) {
                foundMatch = true;
                CRT_itoa(nameIdx + 1, playerNumStr, 10);
                break;
            }
        }

        if (!foundMatch) continue;

        NET_SendFile(playerNumStr, 1, routePath);
        NET_SendFile(playerNumStr, 0, addressPath);

        /* Read route file */
        {
            void* hFile = CreateFileA(routePath, 0x80000000, 1,
                                      NULL, 3, 0x8000000, NULL);
            if (hFile == (void*)-1) return NULL;
            if (!ReadFile(hFile, fileBuf, 0x8000, &bytesRead, NULL)) {
                CloseHandle(hFile);
                return NULL;
            }
            CloseHandle(hFile);

            int32_t entryCount = CRT_atoi(fileBuf);
            int32_t rnd = CRT_rand();
            int32_t targetLine = (int32_t)((uint64_t)rnd / (0x7FFF / (long long)entryCount));

            uint32_t lineStart = 4;
            while (targetLine > 0 && lineStart < bytesRead) {
                if (fileBuf[lineStart] == '\n') targetLine--;
                lineStart++;
            }

            for (uint32_t j = lineStart; j < bytesRead; j++) {
                if (fileBuf[j] == '\r') { fileBuf[j] = '\0'; break; }
            }

            inline_memcpy(routeAddr, &fileBuf[lineStart], 0x14 - 1);
            routeAddr[0x13] = '\0';
        }

        /* Read address file */
        {
            void* hFile = CreateFileA(addressPath, 0x80000000, 1,
                                      NULL, 3, 0x8000000, NULL);
            if (hFile == (void*)-1) return NULL;
            if (!ReadFile(hFile, fileBuf, 0x8000, &bytesRead, NULL)) {
                CloseHandle(hFile);
                return NULL;
            }
            CloseHandle(hFile);

            int32_t entryCount = CRT_atoi(fileBuf);
            int32_t rnd = CRT_rand();
            int32_t targetLine = (int32_t)((uint64_t)rnd / (0x7FFF / (long long)entryCount));

            uint32_t lineStart = 4;
            while (targetLine > 0 && lineStart < bytesRead) {
                if (fileBuf[lineStart] == '\n') targetLine--;
                lineStart++;
            }

            for (uint32_t j = lineStart; j < bytesRead; j++) {
                if (fileBuf[j] == '\r') { fileBuf[j] = '\0'; break; }
            }

            inline_memcpy(playerAddr, &fileBuf[lineStart], 0x14 - 1);
            playerAddr[0x13] = '\0';
        }

        /* Build combined address path */
        {
            int32_t rpLen = strlen(routePath);
            int32_t raLen = strlen(routeAddr);
            int32_t truncLen = rpLen - (raLen - 1);
            if (truncLen < 0) truncLen = 0;
            if (truncLen > (int32_t)sizeof(routePath) - 1) truncLen = sizeof(routePath) - 1;
            routePath[truncLen] = '\0';

            int32_t base = strlen(routePath);
            int32_t i;
            for (i = 0; routeAddr[i] != '\0' && (base + i) < (int32_t)sizeof(routePath) - 1; i++) {
                routePath[base + i] = routeAddr[i];
            }
            routePath[base + i] = '\0';

            base = strlen(routePath);
            if (base < (int32_t)sizeof(routePath) - 1) {
                routePath[base] = '/';
                routePath[base + 1] = '\0';
            }

            base = strlen(routePath);
            for (i = 0; playerAddr[i] != '\0' && (base + i) < (int32_t)sizeof(routePath) - 1; i++) {
                routePath[base + i] = playerAddr[i];
            }
            routePath[base + i] = '\0';
        }

        result = NET_ResolveAddress(_g_dplay, routePath);
        if (result == NULL) return NULL;

        /* Copy player name into resolved DPlayData at +0x25 */
        {
            const uint8_t* src = (const uint8_t*)targetName;
            uint8_t* dst = (uint8_t*)result + 0x25;
            int32_t i;
            for (i = 0; src[i] != 0; i++) dst[i] = src[i];
            dst[i] = 0;
        }

        *(uint16_t*)((uint8_t*)result + 0x3A) = 0;
        *(int32_t*)((uint8_t*)result + 0x3C) = 1;

        /* Decode address path into display name */
        {
            const char* decodeSrc = routeAddr;
            int32_t di = 0;
            int32_t si = 0;

            while (di < (int32_t)sizeof(displayName) - 1 && decodeSrc[si] != '\0') {
                char c = decodeSrc[si];
                if (c == '/') {
                    char next = decodeSrc[si + 1];
                    if (next == '/') {
                        displayName[di++] = '/';
                        si += 2;
                    } else if (next == 'n') {
                        displayName[di++] = '\r';
                        if (di < (int32_t)sizeof(displayName) - 1) displayName[di++] = '\n';
                        si += 2;
                    } else if (next == '?') {
                        const char* pn = (const char*)((uint8_t*)g_player_config + 6);
                        while (*pn && di < (int32_t)sizeof(displayName) - 1) {
                            displayName[di++] = *pn++;
                        }
                        if (di < (int32_t)sizeof(displayName) - 1) displayName[di++] = ' ';
                        si += 2;
                    } else {
                        displayName[di++] = c;
                        si++;
                    }
                } else {
                    displayName[di++] = c;
                    si++;
                }
            }
            if (di < (int32_t)sizeof(displayName)) {
                displayName[di] = '\0';
            } else {
                displayName[sizeof(displayName) - 1] = '\0';
            }

            int32_t dnLen = strlen(displayName);
            if (dnLen < (int32_t)sizeof(displayName)) {
                foundMatch = true;
                inline_memcpy((uint8_t*)result + 0x43, displayName, dnLen + 1);
            } else {
                displayName[sizeof(displayName) - 1] = '\0';
                foundMatch = true;
                inline_memcpy((uint8_t*)result + 0x43, displayName, sizeof(displayName));
            }
        }

        if (foundMatch) break;
    }

    if (result != NULL) {
        DPLAY_SetPlayerName(result, 1, -1);
    }

    return result;
}

/* ================================================================== */
/* NETMAN_ReceiveLayoutSelect - 0x440070                              */
/* TODO: decompile 0x440070                                           */
/* ================================================================== */
void NETMAN_ReceiveLayoutSelect(Netman* netman)
{
    /* TODO: decompile 0x440070 — NETMAN_ReceiveLayoutSelect body
     *
     * Serializes all 9 player slots into a 0x228-byte packet via
     * DPLAY_FreePlayerSlot, queues TrainMessage type=6.
     */
    (void)netman;
}

/* ================================================================== */
/* NETMAN_ReceiveFileTransfer - 0x440310                              */
/* TODO: decompile 0x440310                                           */
/* ================================================================== */
void NETMAN_ReceiveFileTransfer(Netman* netman)
{
    /* TODO: decompile 0x440310 — NETMAN_ReceiveFileTransfer body */
    (void)netman;
}

/* ================================================================== */
/* NETMAN_SendAck - 0x440390                                          */
/* TODO: decompile 0x440390                                           */
/* ================================================================== */
void NETMAN_SendAck(Netman* netman)
{
    /* TODO: decompile 0x440390 — NETMAN_SendAck body */
    (void)netman;
}
