/**
 * Netman.h — High-level network session manager (Netman class)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Netman is the high-level network manager singleton for multiplayer
 * sessions. It manages 9 player slots (each with dpId, layout name, player
 * identity, pixel overlay cache, and a per-slot message queue), the
 * game-mode state machine (waiting/hosting/joined/error), and serialization
 * of map/building overlay pixel data for network sync. It builds on
 * DPlayManager for the actual DirectPlay transport and uses the global
 * _g_train object as the message dispatch target.
 *
 * Size: ~0x804 bytes
 * Vtable: 0x4781C8 (single entry: scalar deleting destructor)
 *
 * Class hierarchy:
 *   (standalone — no base class)
 *
 * Vtable layout:
 *   [0] +0x00: scalar deleting destructor (compiler-generated, 0x43D110)
 *
 * === Message types dispatched by ProcessMessage (0x43F2B0) ===
 *   type 2:  SYNC_GAME_STATE    — receive and sync remote game state data
 *   type 3:  HOST_SESSION_START — begin hosting session
 *   type 4:  LAYOUT_SELECT      — assign/reassign player layout slots
 *   type 5:  NET_RESET          — reset and reinitialize network state
 *   type 9:  GAME_STATE_SYNC    — sync game state (client variant)
 *   type 0xB: REMOVE_INBOUND    — remove player and their inbound trains
 *   type 0xC: PLAYER_JOIN       — new player joined session
 *   type 0xF: INBOUND_APPEND    — append inbound train node to list
 *   type 0x11: GAME_START_HOST  — host-side game start with train spawning
 *   type 0x12/0x15/0x17: FILE_TRANSFER — protocol messages
 *   type 0x13: FLAG_SET         — set per-slot flag
 *   type 0x14: FLAG_CLEAR       — clear per-slot flag
 *   type 0x16: PIXEL_DATA       — receive pixel overlay data for a slot
 *   type 0x18: TRAIN_POSITION   — acknowledge train position update
 *   type 0x1A: PLAYER_LEAVE     — player left session
 *   type 0x1B: REFRESH_REQUEST  — request building overlay refresh
 *   type 0x1C: TIMEOUT_CHECK    — trigger timeout check
 *
 * === InboundTrainNode — linked-list node in m_vehicleList ===
 * Size: 0x94 bytes (same allocation as Vehicle, fields repurpose padding)
 *   +0x00: vtable            — compiler-managed vtable pointer
 *   +0x04: vehicle_data[0x6C] — Vehicle fields (offset 0x04-0x6F)
 *   +0x70: next              — linked-list next pointer
 *   +0x74: tunnel_angle      — tunnel exit angle (uint16_t)
 *   +0x76: field_76          — unknown uint16_t
 *   +0x78: slot_index        — source slot index (uint8_t)
 *   +0x7A: network_id        — network-assigned ID (uint16_t)
 *   +0x7C: peer_index        — peer routing index (uint8_t)
 *   +0x7E: field_7E          — unknown uint16_t
 *   +0x80: field_80          — unknown uint16_t
 *   +0x82: field_82          — unknown uint8_t
 *   +0x84: field_84          — unknown uint16_t
 *   +0x86: field_86          — unknown uint16_t
 *   +0x88: process_delay     — process delay counter (uint8_t)
 *   +0x89: ack_counter       — ack countdown (uint8_t)
 *
 * === PingEntry — per-ping tracking node in transfer_list ===
 * Size: 0x14 bytes
 *   +0x00: dpId            — DirectPlay ID of tracked player
 *   +0x04: pos_x           — X position component
 *   +0x08: pos_y           — Y position component
 *   +0x0C: peer_index      — peer slot index for routing
 *   +0x0D: slot_index      — preferred slot index
 *   +0x10: next            — linked-list next pointer
 *
 * Status: TRANSCRIBED
 */

#pragma once

#include "../shared/types.h"
#include "TrainMessage.h"
#include "../resources/ResourceManager.h"
#include "../game/Vehicle.h"
class AssetMgr;  /* forward declaration for g_asset_mgr */

/* ================================================================== */
/* Packet type constants used by the network subsystem                 */
/* ================================================================== */

#define PACKET_PLAYER_INFO      0x3F8   /* Player info broadcast packet        */
#define PACKET_MAP_DATA         0x3F9   /* Map/building overlay pixel data     */
#define PACKET_DISCONNECT       0x3FA   /* Disconnect notification             */
#define PACKET_LAYOUT_DATA      0x3F1   /* Layout/player-slot serialized data  */
#define PACKET_FILE_TRANSFER    0x3F4   /* File transfer/ack packet            */
#define PACKET_FILE_ACK         0x3F5   /* File transfer ack packet            */
#define PACKET_PLAYER_NAME      0x3F6   /* Player name/presence packet         */
#define PACKET_LAYOUT_ACK       0x3F7   /* Layout select ack packet            */

#define MESSAGE_REFRESH_REQUEST 0x1B    /* Request building overlay refresh    */
#define MESSAGE_SYNC_TRIGGER    0x19    /* Trigger game-state sync             */

/* ================================================================== */
/* Forward declarations for network-linked types                       */
/* ================================================================== */

class Building;
class PlayerConfig;

/* ================================================================== */
/* PingEntry — per-ping tracking node (0x14 bytes)                     */
/* Linked into per-slot transfer_lists (at slot+0x38).                 */
/* ================================================================== */
struct PingEntry {
    int32_t   dpId;             /* +0x00  DirectPlay ID                  */
    int32_t   pos_x;            /* +0x04  X position component           */
    int32_t   pos_y;            /* +0x08  Y position component           */
    uint8_t   peer_index;       /* +0x0C  peer slot index for routing    */
    uint8_t   slot_index;       /* +0x0D  preferred slot index           */
    uint16_t  _pad_0E;          /* +0x0E  padding                        */
    void*     next;             /* +0x10  linked-list next pointer       */
};

/* ================================================================== */
/* InboundTrainNode — linked-list node in Netman::m_vehicleList         */
/*                                                                      */
/* Represents an inbound train in transit between players. This object  */
/* is allocated as a Vehicle (0x94 bytes) by Vehicle_Ctor, then the     */
/* Vehicle's padding area (offsets +0x70..+0x93) is repurposed for      */
/* network metadata (linked-list next pointer, tunnel angle, slot       */
/* indices, ack counters).                                              */
/*                                                                      */
/* At the TRANSCRIBED stage, this struct maps the network-view of the   */
/* fields. Pass 3 (INTEGRATED) should resolve the Vehicle/InboundTrain  */
/* relationship, potentially through a common base or union.            */
/* ================================================================== */
using InboundTrainNode = Vehicle;

/* ================================================================== */
/* CarObject — minimal forward-declared type for car handles            */
/*                                                                      */
/* Car handles in SendGameStart are pointers to objects with vtables.   */
/* At TRANSCRIBED stage, we define the needed virtual methods here.     */
/* Pass 3 should replace with the actual class from the hierarchy.      */
/* ================================================================== */
class CarObject {
public:
    virtual ~CarObject() {}

    /**
     * SetResourceId — vtable[0xF] (offset +0x3C).
     * Transitions car resource ID between LEAVING(0x1870)/ENTERING(0x1871).
     * @param resourceId  New resource ID
     * @param flag        Control flag (-1 or other)
     */
    virtual void SetResourceId(int32_t resourceId, int32_t flag) = 0;

    /**
     * SetParam — vtable[8] (offset +0x20).
     * @param val   Parameter value
     * @param flag  Control flag
     */
    virtual void SetParam(int32_t val, int32_t flag) = 0;
};

/* ================================================================== */
/* SpriteObject — minimal forward-declared type for sprite handles      */
/*                                                                      */
/* Sprites have a vtable accessed in SendSignalChange at vtable[0xD].   */
/* Pass 3 should replace with the actual sprite class.                  */
/* ================================================================== */
class SpriteObject {
public:
    virtual ~SpriteObject() {}

    /**
     * SetDisplayName — vtable[0xD] (offset +0x34).
     * Sets the display name string on the sprite.
     * @param name  C-string display name
     */
    virtual void SetDisplayName(const char* name) = 0;
};

/* ================================================================== */
/* PlayerSlot — per-player state in the Netman's 9-slot table          */
/* Size: 0x4C bytes per slot                                           */
/* ================================================================== */
struct PlayerSlot {
    int32_t   dpId;              /* +0x00  DirectPlay player ID         */
    uint8_t   is_connected;      /* +0x04  1 = connected flag            */
    union {
        struct {
            uint8_t has_data;     /* +0x05 legacy first-byte alias      */
            uint8_t _pad_06[12];  /* +0x06                              */
        };
        char compact_name[13];    /* +0x05 serialized player text       */
    };

    char      layout_name[32];   /* +0x12  layout/scenario name string  */
    uint16_t  player_id;         /* +0x32  short player ID (from global)*/
    uint16_t  player_color;      /* +0x34  short player color (global)  */
    uint8_t   flag_36;           /* +0x36  flag byte (file-transfer state) */
    uint8_t   _pad_37;           /* +0x37  padding                      */

    void*     msg_queue;         /* +0x38  per-slot message queue head   */
    int32_t   data_size;         /* +0x3C  byte count of pixel_buffer   */
    uint16_t  pixel_width;       /* +0x40  overlay pixel width          */
    uint16_t  pixel_height;      /* +0x42  overlay pixel height         */
    void*     pixel_buffer;      /* +0x44  cached overlay pixel data    */
    int32_t   version;           /* +0x48  version/tick count for cache */
};

/* ================================================================== */
/* Netman — Network session manager (singleton, ~0x804 bytes)           */
/* ================================================================== */
class Netman {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* +0x000: compiler-managed vtable pointer (binary table 0x4781C8) */
    uint8_t    m_bInit;             /* +0x004  1 = initialized            */
    uint8_t    _pad_005[3];         /* +0x005  padding                  */

    int32_t    m_playerSlotCount;   /* +0x008  number of slots (default 9, max 9) */
    int32_t    m_playerRows;        /* +0x00C  rows config (default 3, max 3) */
    int32_t    m_playerCols;        /* +0x010  cols config (default 3, max 3) */

    char       m_layoutName[32];    /* +0x014  scenario/layout name ("Default") */

    /* gap +0x034..+0x517 = unused/reserved (1164 bytes) */
    uint8_t    _gap_034[0x4E4];     /* +0x034  gap                        */

    /* -- Player slots (9 x 0x4C = 0x2AC bytes) -- */
    PlayerSlot m_slots[9];          /* +0x518  player slot array, each 0x4C bytes */

    /* -- Network/game management fields (after slots, starting at +0x7C4) -- */
    int32_t    m_gameMode;          /* +0x7C4  0=waiting,1=hosting,2=joined,3=error */
    uint8_t    m_bFlag1;            /* +0x7C8  flag byte                */
    uint8_t    _pad_7C9[3];         /* +0x7C9  padding                  */
    PlayerSlot* m_currentSlot;      /* +0x7CC  pointer to current player slot
                                                (in mode 2 — joined game)   */
    int32_t    m_mySlotIndex;       /* +0x7D0  self slot index (0-8 or -1) */
    int32_t    m_myDpId;            /* +0x7D4  self DirectPlay ID       */
    int32_t    m_field_7D8;         /* +0x7D8  target dpId for search   */
    Building*  m_buildingList;      /* +0x7DC  building linked list head */

    /* -- Inbound train list head -- */
    InboundTrainNode* m_vehicleList; /* +0x7E0  inbound train linked list head */

    int32_t    m_field_7E4;         /* +0x7E4  original last-node value */
#ifndef _WIN32
    Vehicle*   m_hostLastSerializedVehicle; // native-width counterpart
#endif
    int32_t    m_field_7E8;         /* +0x7E8  network ID counter       */
    int32_t    m_tickCounter;       /* +0x7EC  tick counter (incremented per Update) */
    int32_t    m_timeout;           /* +0x7F0  tick timeout (500=host, 20=join) */
    int32_t    m_sendTimer;         /* +0x7F4  send timer accumulator    */
    int32_t    m_visibility;        /* +0x7F8  session visibility (default 0xF) */
    int32_t    m_tickInterval;      /* +0x7FC  tick interval (default 0x960=2400) */
    int32_t    m_timeoutState;      /* +0x800  last timeout state       */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * Netman constructor.
     * Address: 0x43D0A0
     */
    Netman();

    /**
     * Destructor (vtable[0] = scalar deleting destructor at 0x43D110).
     * Address: 0x43D110 (destructor body; scalar deleting wrapper is compiler-generated)
     */
    virtual ~Netman();

    /* ================================================================ */
    /* Initialization / Cleanup                                          */
    /* ================================================================ */

    /**
     * Init — Full (re)initialization.
     * Address: 0x43D130
     */
    void Init(uint8_t is_init);

    /**
     * Cleanup — Full network resource teardown.
     * Address: 0x43DC30
     */
    void Cleanup();

    /* ================================================================ */
    /* Query Methods                                                     */
    /* ================================================================ */

    /**
     * GetPlayerCount — Return connected player count in joined mode.
     * Address: 0x43D210
     *
     * // TODO: Ghidra verification needed — current cast of m_currentSlot
     * // pointer to int32_t may be a decompiler misinterpretation.
     * // Likely should return connected slot count in mode 2.
     */
    int32_t GetPlayerCount();

    /**
     * FindPlayerIndex — Linear search through 9 slots for matching dpId.
     * Address: 0x43D230
     */
    int32_t FindPlayerIndex(int32_t dpId);

    /* ================================================================ */
    /* Game Mode Management                                              */
    /* ================================================================ */

    /**
     * SetGameMode — Transition game mode state machine.
     * Address: 0x43D2B0
     */
    void SetGameMode(int32_t newMode);

#ifndef _WIN32
    // Host transport projection. These fields mirror the state established
    // by NETMAN_ProcessMessage type 3 and NETMAN_SetGameMode, but SDL_net
    // supplies virtual player IDs instead of DirectPlay DPIDs.
    void HostBeginTransportSession(bool hosting, int32_t localPlayerId,
                                   const char* localPlayerName);
    void HostAddTransportPlayer(int32_t playerId, const char* playerName);
    void HostRemoveTransportPlayer(int32_t playerId);
    void HostEndTransportSession();
    bool HostClonePendingRouteForLoading();
#endif

    /* ================================================================ */
    /* Network Data Send Methods                                         */
    /* ================================================================ */

    /**
     * SendMapData — Serialize tilemap overlay into MAP_DATA packet.
     * Address: 0x43D350
     */
    void SendMapData(int32_t targetDpId);

    /**
     * SendBuildingData — Send cached building overlay as MAP_DATA packet.
     * Address: 0x43D520
     */
    void SendBuildingData(int32_t targetDpId);

    /**
     * UpdatePlayerInfo — Broadcast PLAYER_INFO packet + SYNC_TRIGGER.
     * Address: 0x43D620
     */
    void UpdatePlayerInfo();

    /* ================================================================ */
    /* Player Data Processing                                            */
    /* ================================================================ */

    /**
     * ProcessPlayerData — Load and cache a player's layout thumbnail.
     * Address: 0x43D6C0
     */
    void ProcessPlayerData(int32_t slotIndex);

    /* ================================================================ */
    /* Scenario Loading                                                  */
    /* ================================================================ */

    /**
     * LoadScenario — Load .lay scenario file and parse player config.
     * Address: 0x43D820
     *
     * // TODO: decompile 0x43D820
     */
    void LoadScenario(const char* layoutName);

    /* ================================================================ */
    /* Track Connection / Grid Edge Checks                              */
    /* ================================================================ */

    /**
     * CheckTrackConnection — Core grid boundary check for track routing.
     * Address: 0x43DE30
     *
     * Given a direction angle (angle / 0x28 = 0=up, 2=right, 4=down, 6=left)
     * and a grid position, returns 1 if track can continue in that direction
     * (not at grid edge). position=-1 uses m_mySlotIndex.
     *
     * Grid size: m_playerCols x m_playerRows
     *
     * @param angle      Direction in degrees (0, 0x5A, 0xB4, 0x10E)
     * @param position   Grid position index, or -1 for self
     * @return           1 if track can continue, 0 at edge
     */
    int32_t CheckTrackConnection(int32_t angle, int32_t position);

    /**
     * CheckLeftEdge — Wrapper: check angle 0x10E (left)
     * Address: 0x43DE00
     *
     * The binary leaves CheckTrackConnection's return in AL (0x43DE10:
     * push -1; push 0; call 0x43DE30; ret), so the wrappers return the
     * edge-check result.  INPUT_LoadWorld (0x41D3FC..) tests it. */
    int32_t CheckLeftEdge();

    /**
     * CheckUpEdge — Wrapper: check angle 0 (up)
     * Address: 0x43DE10
     */
    int32_t CheckUpEdge();

    /**
     * CheckDownEdge — Wrapper: check angle 0xB4 (down)
     * Address: 0x43DE20
     */
    int32_t CheckDownEdge();

    /**
     * CheckRightEdge — Wrapper: check angle 0x5A (right)
     * Address: 0x43DDF0
     */
    int32_t CheckRightEdge();

    /* ================================================================ */
    /* Player Name Send/Receive (periodic presence broadcast)            */
    /* ================================================================ */

    /**
     * SendPlayerName — Periodically broadcast local player presence.
     * Address: 0x43DED0
     *
     * Rate-limited by m_sendTimer / m_visibility ratio. Packs local slot's
     * msg_queue into 8-byte blocks, wrapped in PLAYER_NAME packet (type
     * 0x3F6), queued via Train_QueueMessage.
     *
     * Called by: Update (per frame)
     */
    int32_t SendPlayerName();

    /**
     * ReceivePlayerName — Process inbound train arrivals.
     * Address: 0x43E010
     *
     * Timer-driven: pops from m_vehicleList when ack countdown reaches 0.
     * Routes trains via SendChatMessage, iterates car handles to play
     * arrival notification sound.
     */
    uint32_t ReceivePlayerName();

    /* ================================================================ */
    /* Train Routing / Chat (misnamed — routes trains, not chat)        */
    /* ================================================================ */

    /**
     * SendChatMessage — Route arriving train through correct tunnel exit.
     * Address: 0x43E1D0
     *
     * Maps InboundTrainNode.tunnel_angle to direction+offset:
     *   0     -> down (4)
     *   0x5A  -> left (1)
     *   0xB4  -> right (3)
     *   0x10E -> up (2)
     * Calls World_FinalizeLoad. Only runs when g_game_mode is 3,5,9.
     * NOT chat-related despite name.
     *
     * @param node  Inbound train node to finalize
     * @return      1 on success, 0 if cannot route
     */
    uint8_t SendChatMessage(InboundTrainNode* node);

    /**
     * ReceiveChatMessage — Append inbound train to m_vehicleList (type 0xF).
     * Address: 0x43E2E0
     *
     * Host (mode==1) appends node from msg->data_ptr to m_vehicleList,
     * sets process_delay=0 and tick_counter=timeout-0x20.
     * Client/other destroys node via delete.
     *
     * @param msg  TrainMessage to process
     */
    void ReceiveChatMessage(TrainMessage* msg);

    /* ================================================================ */
    /* Game Start / Signal Change                                       */
    /* ================================================================ */

    /**
     * SendGameStart — Host-only: append train, compute route, register ping (type 0x11).
     * Address: 0x43E370
     *
     * Host appends node to m_vehicleList, maps tunnel_angle to direction,
     * registers train tracking via ReceivePing. Transitions car resource IDs:
     * if DPlayData exists -> LEAVING(0x1870)->ENTERING(0x1871);
     * else -> ENTERING(0x1871)->LEAVING(0x1870).
     *
     * @param msg  TrainMessage to process
     */
    void SendGameStart(TrainMessage* msg);

    /**
     * ReceiveGameStart — Process arriving train, dispatch by role.
     * Address: 0x43E560
     *
     * Called from World_SaveToFile/World_UpdateTick finalizing train arrival.
     * Calls World_GetObjectAt + HandleTimeout + SendSignalChange.
     * Role dispatch: host -> SendTrainPosition, client -> ReceiveTrainPosition,
     * other -> queue.
     *
     * @param worldOrObj  // TODO: Ghidra verify — likely World* or object pointer
     * @param param       // TODO: Ghidra verify — likely tile coordinate or index
     * @param node        Inbound train node
     * @return            Node pointer (opaque success indicator)
     */
    bool ReceiveGameStart(int32_t position_x, int32_t position_y,
                          InboundTrainNode* node);

    /**
     * SendSignalChange — Process incoming signal change from remote player.
     * Address: 0x43E690
     *
     * Iterates track pieces, retrieves DPlay data, creates a Vehicle for
     * each valid track, links into m_vehicleList, assigns network ID,
     * initializes route. Calls HandleTimeout.
     *
     * @return  1 if host and no stale tracks, 0 otherwise
     */
    uint8_t SendSignalChange(InboundTrainNode* node);

    /* ================================================================ */
    /* Network Lifecycle Management                                      */
    /* ================================================================ */

    /**
     * ResetNetworkState — Clear active flag, reinit, queue NET_RESET message.
     * Address: 0x43EFA0
     */
    void ResetNetworkState();

    /**
     * StopSession — Queue STOP_SESSION type-0 TrainMessage.
     * Address: 0x43F070
     *
     * // TODO: decompile 0x43F070
     */
    void StopSession();

    /**
     * Update — Per-frame network processing.
     * Address: 0x43F0C0
     */
    void Update();

    /**
     * ProcessMessage — Dispatch received network messages by type.
     * Address: 0x43F2B0
     *
     * Main dispatch function handling 20+ message types for game sync,
     * layout select, file transfer, player join/leave, and timeouts.
     * Called from Update() for each queued message.
     *
     * @param msg  TrainMessage to process
     */
    void ProcessMessage(TrainMessage* msg);

    /**
     * Shutdown — Full network shutdown.
     * Address: 0x43F7B0
     */
    void Shutdown();

    /**
     * HandlePlayerJoin — Process PLAYER_JOIN event (type 0xC).
     * Address: 0x43F880
     */
    void HandlePlayerJoin();

    /**
     * RemoveInboundTrain — Remove player and all their inbound trains (type 0xB).
     * Address: 0x43F940
     *
     * Fully removes player by dpId: serializes world objects, unlinks/destroys
     * linked-list nodes, clears slot fields, frees pending_data, drains
     * transfer_list, calls ProcessPlayerData, redraws UI.
     *
     * @param dpId  DirectPlay ID of player to remove
     */
    void RemoveInboundTrain(int32_t dpId);

    /**
     * HandlePlayerLeave — Process player leaving session (type 0x1A).
     * Address: 0x43FB50
     *
     * Reads slot index from msg, removes matching node from m_vehicleList,
     * walks transfer_list calling ReceiveAck for matching pending transfers,
     * redraws UI.
     *
     * // NOTE: msg->data_ptr field reused as inline int32_t slot index
     * // (the binary packs the slot index directly into the pointer field)
     *
     * @param msg  TrainMessage with player leave data
     */
    void HandlePlayerLeave(TrainMessage* msg);

    /**
     * SyncGameState — Process incoming SYNC_GAME_STATE packet (type 2/9).
     * Address: 0x43FC50
     */
    void SyncGameState(TrainMessage* msg);

    /**
     * SendLayoutSelect — Process layout-selection assignment.
     * Address: 0x43FE30
     *
     * Finds source slot by dpId or by name string, moves player to target
     * slot, copies name/metadata, clears old slot. If game_started flag set,
     * calls ReceiveLayoutSelect to serialize state.
     *
     * @param dpId       Source player dpId, or -1 to search by name
     * @param targetSlot Target slot number to assign
     * @param name       Player name string for slot identification
     * @param playerInfo Combined player_id and player_color word
     */
    void SendLayoutSelect(int32_t dpId, int32_t targetSlot,
                          const char* name, int32_t playerInfo);

    /* ================================================================ */
    /* File Transfer / Ping / Latency / Timeout                         */
    /* ================================================================ */

    /**
     * SendFileTransfer — Process file-transfer protocol messages.
     * Address: 0x440150
     *
     * Despite "Send" name, primarily handles INCOMING data. Dispatches on
     * msg->type: 0x12=angle/direction to pixel pos, 0x15=player data with
     * ping entries, 0x17=forward ping.
     *
     * @param msg  TrainMessage with file transfer data
     */
    void SendFileTransfer(TrainMessage* msg);

    /**
     * ReceiveAck — Process ACK from peer, forward or clean up.
     * Address: 0x440410
     */
    void ReceiveAck(int32_t dpId, uint8_t slot_byte, uint32_t peerIndex);

    /**
     * RemovePingEntry — Search and remove a PingEntry from transfer_lists.
     * Address: 0x4404C0
     *
     * MISNAMED in Ghidra as NETMAN_SendPing. Searches transfer_lists for a
     * PingEntry matching (dpId, slot), unlinks and frees it. Tries preferred
     * slot first, then playerSlot, then all others. Never sends anything.
     *
     * @param dpId       DirectPlay ID to match
     * @param slot_byte  Slot byte to match
     * @param peerIndex  Peer index for preferred slot search
     */
    void RemovePingEntry(int32_t dpId, uint8_t slot_byte, uint32_t peerIndex);

    /**
     * ReceivePing — Create or update a PingEntry in transfer_list.
     * Address: 0x440610
     *
     * If PingEntry exists, updates fields and optionally moves between
     * slots when peer_index changes. If new, allocates 0x14-byte entry
     * and prepends to list.
     */
    void ReceivePing(int32_t dpId, uint8_t slot_byte,
                     uint32_t peerIndex, int32_t posX, int32_t posY);

    /**
     * UpdateLatency — Search transfer_lists for matching PingEntry.
     * Address: 0x440750
     *
     * Tries preferred slotIndex first, then playerSlot, then all others.
     *
     * @return  Pointer to PingEntry, or NULL if not found
     */
    PingEntry* UpdateLatency(int32_t dpId, uint8_t slot_byte, uint32_t peerIndex);

    /**
     * CheckTimeout — Set timeout state on matching game objects.
     * Address: 0x440820
     *
     * Iterates all game objects, finds objects with resource IDs 0xC5C/0xC5E/0xC60,
     * calls virtual method at vtable[7] with the timeout value (0, 1, 2, or 3).
     * Updates m_timeoutState to track last set value.
     *
     * @param timeoutVal  Timeout state to apply (1=red, 2=yellow, 3=green, 0=off)
     */
    void CheckTimeout(int32_t timeoutVal);

    /**
     * HandleTimeout — Process timeout: register players, transition car states.
     * Address: 0x4408B0
     *
     * Iterates cars in the train node. For each car with DPlayData matching
     * player_config, calls NET_RegisterPlayer and clears DPlayData on the vehicle.
     * Transitions car resource IDs between LEAVING(0x1870)/ENTERING(0x1871) states.
     * Updates CheckTimeout based on current m_timeoutState.
     *
     * @param node  Inbound train node to process
     */
    void HandleTimeout(InboundTrainNode* node);

    /**
     * SerializePlayerData — Process player data from inbound train.
     * Address: 0x440A50
     *
     * Calls HandleTimeout then DeserializePlayerData based on state.
     * Stores the node pointer in m_field_7E4.
     *
     * @param node  Inbound train node to serialize
     */
    void SerializePlayerData(InboundTrainNode* node);

    /**
     * DeserializePlayerData — Resolve player DPlayData from host name list.
     * Address: 0x440A80
     *
     * Gets host name count via DPLAY_GetMessageCount. For each car with
     * valid DPlayData, resolves via NET_ResolveAddress, assigns DPlayData,
     * transitions car states, updates player list. Calls CheckTimeout based
     * on current m_timeoutState.
     *
     * @param node  Inbound train node to process
     */
    void DeserializePlayerData(InboundTrainNode* node);
};

/* ================================================================== */
/* Standalone network helper functions (C++ linkage)                    */
/* ================================================================== */

/** Return the current mode from an optional Netman instance. */
int32_t NETMAN_GetGameMode(const void* netman);

/**
 * NETMAN_SendDisconnect — Build and queue a DISCONNECT packet.
 * Address: 0x43D250
 */
void NETMAN_SendDisconnect(int32_t dpId);

/**
 * NETMAN_QueueMessage — Queue or free a network message.
 * Address: 0x43F140
 */
void NETMAN_QueueMessage(TrainMessage* msg);

/**
 * NETMAN_StartHostSession — Queue HOST_SESSION_START (type 3) message.
 * Address: 0x43F000
 */
void NETMAN_StartHostSession();

/**
 * NETMAN_StartClientSession — Queue CLIENT_SESSION_START (type 1) message.
 * Address: 0x43F030
 */
void NETMAN_StartClientSession();

/**
 * NETMAN_ReceiveLayoutSelect — Serialize all 9 slots into packet and send.
 * Address: 0x440070
 *
 * Despite "Receive" name, this creates a 0x228-byte packet with all
 * player slots compacted via DPLAY_FreePlayerSlot and queues it.
 *
 * @param netman  Netman instance pointer
 */
void NETMAN_ReceiveLayoutSelect(Netman* netman);

/**
 * NETMAN_ReceiveFileTransfer — Mark receiving state, send type 0x3F4 packet.
 * Address: 0x440310
 */
void NETMAN_ReceiveFileTransfer(Netman* netman);

/**
 * NETMAN_SendAck — Mark idle state, send type 0x3F5 packet.
 * Address: 0x440390
 */
void NETMAN_SendAck(Netman* netman);

/**
 * NETMAN_SendTrainPosition — Queue type 0xE signal_change message.
 * Address: 0x43EE80
 *
 * @param vehicle  Vehicle pointer
 * @return         1
 */
bool NETMAN_SendTrainPosition(InboundTrainNode* vehicle);

/**
 * NETMAN_ReceiveTrainPosition — Process received position, compute route.
 * Address: 0x43EEC0
 *
 * Computes track route from source/dest params, validates connectivity,
 * queues TRAIN_POSITION message (type 0x10).
 */
bool NETMAN_ReceiveTrainPosition(int32_t position_x, int32_t position_y,
                                 InboundTrainNode* vehicle);

/**
 * NETMAN_ReceiveSignalChange — Resolve remote player address from PostBag files.
 * Address: 0x43E900
 *
 * Enumerates DPLAY players, matches player name against param+0x10,
 * reads route/address PostBag files, picks random route entry, resolves
 * address via NET_ResolveAddress, copies data into resolved DPlayData.
 *
 * @param playerDPlayData  Source DPlayData with player name to resolve
 * @return                 Resolved DPlayData pointer, or NULL on failure
 */
void* NETMAN_ReceiveSignalChange(void* playerDPlayData);

/* ================================================================== */
/* Win32 API imports (C linkage only)                                   */
/* ================================================================== */

extern "C" {
    /* Win32 API */
    int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);
    void*   __stdcall GetProcessHeap(void);
    int32_t __stdcall HeapFree(void* hHeap, uint32_t dwFlags, void* lpMem);
    int32_t __stdcall IsWindowVisible(void* hWnd);
    void*   __stdcall CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess,
                                   uint32_t dwShareMode, void* lpSecurityAttributes,
                                   uint32_t dwCreationDisposition,
                                   uint32_t dwFlagsAndAttributes, void* hTemplateFile);
    int32_t __stdcall ReadFile(void* hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead,
                                uint32_t* lpNumberOfBytesRead, void* lpOverlapped);
    int32_t __stdcall WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
                                 uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
    int32_t __stdcall CloseHandle(void* hObject);
    int32_t __stdcall CopyRect(RECT* lprcDst, const RECT* lprcSrc);
    int32_t __stdcall OffsetRect(RECT* lprc, int32_t dx, int32_t dy);
    int32_t __stdcall SetWindowTextA(void* hWnd, const char* lpString);
    int32_t __stdcall GetWindowTextA(void* hWnd, char* lpString, int32_t nMaxCount);
    int32_t __stdcall PostMessageA(void* hWnd, uint32_t Msg, uint32_t wParam, uint32_t lParam);
    void*   __stdcall SetTimer(void* hWnd, uint32_t nIDEvent, uint32_t uElapse, void* lpTimerFunc);
    int32_t __stdcall KillTimer(void* hWnd, uint32_t nIDEvent);
    void    __stdcall SetFocus(void* hWnd);
    void    __stdcall ShowWindow(void* hWnd, int32_t nCmdShow);
    void*   __stdcall SetWindowLongA(void* hWnd, int32_t nIndex, void* dwNewLong);
    void*   __stdcall CreateWindowExA(uint32_t dwExStyle, const char* lpClassName,
                                       const char* lpWindowName, uint32_t dwStyle,
                                       int32_t x, int32_t y, int32_t nWidth, int32_t nHeight,
                                       void* hWndParent, void* hMenu,
                                       void* hInstance, void* lpParam);
    void    __stdcall MessageBeep(uint32_t uType);
    int32_t __stdcall MessageBoxA(void* hWnd, const char* lpText,
                                   const char* lpCaption, uint32_t uType);
    void    __stdcall OutputDebugStringA(const char* lpOutputString);
    /* void (matches tilemap.h's extern void Sleep(uint32_t) — InputMgr.cpp
     * includes both headers, and the Win32 Sleep return value is never
     * used; tilemap.cpp calls it without checking a result). */
    void    __stdcall Sleep(uint32_t dwMilliseconds);
}

/* ================================================================== */
/* C++ extern declarations for globals and game engine functions       */
/* (NOT inside extern "C" — these have C++ linkage)                    */
/* ================================================================== */

/* -- Globals -- */
class TileMap;   /* forward decl for g_tilemap below (tilemap.h) */
extern int32_t  g_game_mode;          /* 0x4851F4 — global game mode   */
extern char     g_install_path[];     /* 0x4A99C8 — installation path  */
extern int32_t  g_player_id;          /* 0x4AAD46 — global player ID   */
extern int32_t  g_player_color;       /* 0x4AAD48 — host-declared 32-bit for
                                          *   uniformity (16-bit storage + 16-bit
                                          *   loads in the binary) */
extern TileMap* g_tilemap;            /* 0x4AAD08 — tilemap object     */
extern void*    g_asset_mgr;          /* 0x485600 — asset manager ptr  */
extern void*    g_net_host_info;      /* 0x4FD3A8 — net host info struct */
extern void*    g_ui_main;            /* 0x4A8860 — main UI window ptr */
extern void*    g_listener_x;         /* 0x486BBC — listener position x */
extern void*    g_listener_y;         /* 0x486BC0 — listener position y */
extern void*    _g_dplay;             /* 0x4FD3A8 — DPLAY/NetworkPlayerList instance */
extern void*    _g_dplay_config;      /* 0x4FD3AC — DPLAY config instance */
extern int32_t  g_object_count;       /* 0x4AAD04 — object count       */
extern Netman*  _g_netman;            /* 0x4FD3AC — Netman singleton pointer */
extern PlayerConfig* g_player_config; /* 0x4AA4A8 — PlayerConfig singleton */
extern char     g_empty_string;       /* 0x4851D0 — empty string constant  */

/* -- CRT helpers (C++ linkage) -- */
void* operator_new(size_t size);              /* 0x465CE0 */
void  GLOBAL_free(void* ptr);                  /* 0x465CD0 */
void  CRT_exit(const char** msg, const char** fileLine);  /* 0x466CE0 */
int32_t CRT_atoi(const char* str);            /* 0x466390 */
void  CRT_itoa(int32_t val, char* buf, int32_t radix); /* 0x4663D0 */
int32_t CRT_rand(void);                       /* 0x466150 */
void  CRT_time(void);                         /* 0x466490 */

/* -- Game engine functions -- */
void  Train_QueueMessage(void* train, TrainMessage* msg);
void* UIPANEL_CreateSurface(void* surface);
void  TileMap_CreateOverlay(void* tilemap, void* surface, int32_t flags);

/* -- Resource manager functions -- */
void    ResourceManager_Init(void* resdata);         /* was RESMGR_ResourceData_Init */
uint8_t ResourceManager_LoadResource(void* resdata, const char* path); /* was RESMGR_LoadResource */
void    ResourceManager_ReleaseResource(void* resdata); /* was RESMGR_ReleaseResource */
void    ResourceManager_LoadSoundResource(int32_t resId); /* was RESMGR_LoadSoundResource */
void*   ResourceManager_GetById(void* resmgr, uint32_t id); /* was ResourceManager_GetById */
int32_t ResourceManager_GetStringById(void* resmgr, uint32_t id);

/* -- UI functions -- */
void  EditorState_LoadExistingGame(void* uiPanel);     /* was GAMESTATE_LoadExistingGame */
void  EditorState_HandleNetworkGame(void* uiPanel);    /* was GAMESTATE_HandleNetworkGame */
void  EditorState_SelectLayout(void* uiPanel, int32_t layoutData); /* was GAMESTATE_SelectLayout */
void  EditorState_StartGameTimer(int32_t* uiPanel);    /* was GAMESTATE_StartGameTimer */
void  EditorState_SetDifficulty(void* uiPanel, int32_t difficulty); /* was GAMESTATE_SetDifficulty */
void  CGWND_GameSetup_DrawGrid_Thunk(void* uiPanel);
void  CGWND_QuitToMenu(void);
void  UIPANEL_EndPaintEx(void* panel, void* hwnd, int32_t hdc,
                          uint8_t repaint, void* updateRect);
void  UIPANEL_EndPaint(void* panel);
void  UI_MainMenu_SetState(void* ui_main, int32_t state);

/* -- Audio -- */
/* Canonical signature (ResourceManager.h, 0x447930).  The old int32_t
 * forms hijacked overload resolution in TUs including both headers,
 * binding calls to the never-defined _Z9PlaySoundi /
 * _Z11PlaySoundAtiiii instead of the real _Z9PlaySoundj (runtime
 * crash with the ignore-all link). */
void    PlaySound(unsigned int soundId);
void    PlaySoundAt(unsigned int soundId, int32_t x, int32_t y, int32_t flags);
int32_t PlaySoundFile(const char* path, void* x, void* y, int32_t flags);

/* -- Network helpers -- */
void*   NET_CreateSession(void* dplay, uint8_t param1, uint8_t param2,
                           uint8_t param3, char param4);
void*   NET_RegisterPlayer(void* dplay, void* playerData, int32_t type, int32_t param);
void    NET_UnregisterPlayer(void* dplay, const char* name);
void*   NET_ResolveAddress(void* dplay, const char* path);
void*   NET_GetHostName(int32_t mode, int32_t param);
int32_t NET_UpdatePlayerList(void);
void    NET_GetAttFilePath(uint16_t id, int32_t type, char* outPath);
void    NET_SendFile(const char* name, uint8_t flag, char* pathBuf);

/* -- VehicleEditor helpers -- */
int32_t VehicleEditor_GetDPlayData(int32_t trackPiece);
void    VehicleEditor_SetDPlayData(void* trackPiece, int32_t data);
int32_t VehicleEditor_GetResourceId(int32_t trackPiece);

/* -- World / Game functions -- */
void    World_SerializeObject(void* world, int32_t param);
void __stdcall World_GetObjectAt(void* object);
uint8_t World_FinalizeLoad(void* world, InboundTrainNode* node, void* param, uint8_t dir);
InboundTrainNode* Vehicle_Ctor(InboundTrainNode* vehicle, int32_t resourceId, int32_t param2,
                      uint8_t param3, uint8_t param4);
void    Vehicle_CalcSpeed(InboundTrainNode* vehicle, int16_t speed);
void    Vehicle_InitRoute(InboundTrainNode* vehicle, int32_t resourceId, int32_t param2, uint8_t param3);
void    Vehicle_SetState(InboundTrainNode* vehicle, int32_t state);

/* -- UI helpers -- */
void    FormatResourceString(void* resmgr, uint32_t id, char* buf, int32_t bufsize);
void    Sprite_Init(void* sprite);
void    Sprite_Destroy(void* sprite);
void    Sprite_SetState(void* sprite, int32_t state, int32_t* unk);
void    UI_WindowBase_Show(void* window);
void    UI_WindowBase_Hide(void* window);
void    UIPANEL_StretchBlit(void* surface, const char* path, int32_t x, int32_t y, int32_t flags);
void*   UIPANEL_CopySurface(void* dst, int32_t src);
void    Config_GetIniInt(void* ini, const char* section, const char* key, int32_t def);
void    Config_WriteInt(void* ini, const char* section, const char* key, int32_t val);

/* -- Direction mapping -- */
int32_t INPUT_DirToOffset_Up(int32_t* param);
int32_t INPUT_DirToOffset_Down(int32_t* param);
int32_t INPUT_DirToOffset_Left(int32_t* param);
int32_t INPUT_DirToOffset_Right(int32_t* param);

/* -- DPLAY helper functions -- */
void*   DPLAY_CreatePlayer(void* slot);
int32_t DPLAY_GetPlayerName(void* slot, const char* path);
int32_t DPLAY_SetPlayerData(void* slot, const char* path);
void    DPLAY_SetPlayerName(void* slot, int32_t trainId, int8_t specific);
void    DPLAY_CopyPlayerData(void* dstSlot, const void* packet); /* 0x4426D0 */
void    DPLAY_InitPlayerSlot(void* dstSlot, const void* srcSlot); /* 0x442750 */
void    DPLAY_FreePlayerSlot(void* packet, const int32_t* slotSrc); /* 0x4427D0 */
#ifndef _WIN32
void*   DPLAY_DecodePlayerSlots(const void* firstCompactSlot);
#endif  /* @ 0x442750 */
int16_t DPLAY_GetMessageCount(int32_t dplay);
void    DPLAY_EnumeratePlayers(int32_t dplay);

/* -- Stream I/O -- */
void* WIN32_StreamFromMemory(void* stream, const char* data,
                              int32_t size, int32_t flags);
void* WIN32_StreamOpenFile(void* stream, const char* path,
                            int32_t mode, int32_t flags, int32_t param);
void  WIN32_StreamRead(void* stream, void* buf, int32_t size);

/* -- Asset manager -- */
uint8_t* AssetMgr_LoadFile(AssetMgr* self, uint8_t* filename, int32_t* out_size);

/* -- NET class helpers -- */
uint32_t NET_Dtor(uint8_t param1, uint8_t param2, uint8_t param3);
void     NET_Ctor(void* dplay, void* param1, uint32_t param2, uint32_t param3,
                   int32_t param4, uint32_t param5, uint8_t* param6);
void     NET_BaseDtor(void* dplay);

/* -- Network UI helpers -- */
void  NETMAN_EnumerateSessions(int32_t panel);
void  NETMAN_JoinSession(void* panel);
void  NETMAN_CreateSession(int32_t panel);
void  NETMAN_LeaveSession(int32_t panel);
void  NETMAN_UpdateSessionInfo(void* panel);
void  NETMAN_GetSessionInfo(int32_t panel);
void  NETMAN_SetSessionInfo(void* panel);
void* NETMAN_DestroySession(void* panel, void* hWnd, uint32_t msg,
                             uint32_t wParam, uint32_t lParam);

/* -- DirectPlay message/file management -- */
void  DPLAY_SendMessages(void);
void __stdcall DPLAY_ReceiveMessage(const char* path);

/* -- Network settings persistence -- */
void  NETMAN_FreePacket(int32_t packetPtr);
void  NETMAN_SendPacket(int32_t packetPtr);

/* -- DPlayConfig destructor -- */
void* NETMAN_FreeProviderList(void* config, uint8_t flags);

/* -- Postcard attachment ID -- */
uint16_t NET_BaseDtor(void);
