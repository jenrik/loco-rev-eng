// Status: TRANSCRIBED
/**
 * Train.h — Train game object and TrainSubsystem (network manager)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Two classes in this header:
 *
 * 1. TrainEntity — extends Building, represents a train car/entity on the map.
 *    Vtable: 0x4780B8. Binary size: 0xF0 bytes; unlike a full Building,
 *    Train has no +0xF0 occupant_ptr tail field. Constructor 0x4533D0 calls
 *    the intermediate Building::BaseCtor directly.
 *
 * 2. TrainSubsystem — standalone network manager singleton (0x38 bytes).
 *    Vtable: 0x4781C4. Manages DirectPlay networking, sprite lists, and
 *    train car linked lists. Stored in the _g_train global (0x004FD3A4).
 *    Constructor at 0x438BC0. NOT related to Building.
 *
 * TrainStationWindow lives in ui/TrainStationWindow.h (vtable 0x478130,
 * inherits GameWindow). Free functions Train_StartMultiplayer and
 * Train_StopMultiplayer are declared below for TrainSubsystem use.
 */

#pragma once

#include "Building.h"

/* Forward declarations for network-linked-list node types              */
struct InboundTrainNode;
struct PlayerConnectionNode;

/* ================================================================== */
/* TrainEntity — Building-derived train entity                         */
/* ================================================================== */

/**
 * TrainEntity — Building-derived train game object for train cars on the map.
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Extends Building with train-specific per-frame update behavior. Shares
 * Building::BaseCtor for construction (occupation_level=4 default). The
 * per-frame update (vtable[15]) overrides Building::Update with simplified
 * logic: no party mode, no occupant management, no AI action switching, and
 * no animation updates. Idle behavior uses random object IDs (0x3400..0x3428)
 * instead of Building's search-by-type pattern.
 *
 * Binary size: 0xF0 bytes in both BuildingMgr creation (0x434A76) and the
 * 0x435DB0 factory. The full Building size is 0xF4; Train uses only the
 * intermediate Building base and has no +0xF0 occupant_ptr.
 *
 * Vtable: 0x4780B8 (set in TrainEntity constructor, restored in BaseDtor)
 *
 * Class hierarchy:
 *   GameObject (type=1, vtable 0x477820, +0x00..+0x3F)
 *     -> Entity    (type=2, vtable 0x477488, +0x00..+0x86)
 *       -> Building (type=7, vtable 0x477EB8, +0x00..+0xF3)
 *         -> TrainEntity (type=8, vtable 0x4780B8, +0x00..+0xEF)
 *
 * Vtable layout (partial):
 *   [0]  +0x00: scalar deleting destructor     (0x4363E0)
 *   [3]  +0x0C: SetWorldPos / SetPosition      (inherited)
 *   [10] +0x28: RegisterEntity                 (factory method)
 *   [15] +0x3C: Update (per-frame tick)        (0x453450)
 *   [16] +0x40: SetTarget(x, y)                (inherited)
 *   [17] +0x44: update-anim / notify           (inherited)
 *   [18] +0x48: MoveToTarget(x, y)             (inherited)
 *   [22] +0x58: IsMovementActive               (inherited)
 *   [23] +0x5C: PartyModeUpdate                (inherited)
 *
 * 0x435DB0 is a separate factory in another prototype vtable; it is not a
 * TrainEntity vtable slot.
 */
class TrainEntity : public Building {
public:
    using Building::Update;

    /* ================================================================ */
    /* Fields                                                            */
    /* ================================================================ */

    /* No new fields beyond the intermediate 0xF0-byte Building base. */
    /* A natural C++ compiler retains Building's +0xF0 tail in sizeof, */
    /* but no Train binary function accesses that field.               */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * Constructor — calls Building::BaseCtor, sets TrainEntity vtable (0x4780B8).
     * Address: 0x4533D0 (the BaseCtor call is at 0x4533D8)
     * Size: 32 bytes
     * RET 0x4 (1 stack arg).
     *
     * Called by: BuildingMgr_CreateFromResource (0x434A85) when resource type == 8
     *
     * @param resource_id  Resource identifier for the train car model/type.
     */
    TrainEntity(int resource_id);

    /**
     * Base destructor body — real cleanup logic.
     * Address: 0x4533F0
     * Size: 95 bytes (25 instructions)
     * No stack args.
     *
     * Restores TrainEntity vtable (0x4780B8), deselects this object if it
     * is currently selected (g_selected_building == this), then calls
     * Building::BaseCleanup to disconnect from parent and run entity-level
     * cleanup (release audio/sound/res, mark dead).
     *
     * SEH-guarded to handle exceptions during cleanup.
     *
     * Called by: TrainEntity::scalar deleting destructor (0x4363E3)
     */
    void BaseDtor();

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x4363E0
     * Size: 30 bytes
     * RET 0x4 (1 byte-stack arg).
     *
     * Calls BaseDtor() for cleanup, then conditionally frees memory
     * via GLOBAL_free if (flags & 1).
     *
     * In the binary: scalar deleting destructor at 0x4363E0.
     * Natural C++: compiler-generated from virtual destructor chain.
     *
     * @param flags  If bit 0 set, also free the object's memory.
     * @return       The object pointer (this).
     */
    virtual ~TrainEntity();

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Update — per-frame update tick (overrides Building::Update, vtable[15]).
     * Address: 0x453450
     * Size: 281 bytes (97 instructions)
     * RET 0x4 (1 stack arg).
     *
     * Simplified update compared to Building::Update:
     *   1. Skips if disabled (+0x89 != 0).
     *   2. Calls Building::CheckTimeout (decrements occupant level if expired).
     *   3. If movement active (field_dc != 0):
     *      a. Calls vtable[22] (IsMovementActive) — if still moving, skip.
     *      b. If arrived: checks if position matches target_x/target_y.
     *         - At target: calls this->HandleAction(last_action).
     *         - Not at target: calls vtable[18] (MoveToTarget) to continue.
     *   4. If not moving and action timer expired + not selected:
     *      a. Finds random object at ID (rand() % 0x29 + 0x3400) via InputMgr.
     *      b. If found object differs from prev target position:
     *         calls vtable[16] (SetTarget) to move toward it.
     *
     * Differences from Building::Update:
     *   - NO party mode check (no vtable[23] dispatch).
     *   - NO occupant management (no DecideAction, no MoveToTarget for occupants).
     *   - NO animation update (no UpdateAnimByOccupancy).
     *   - Random object ID range instead of fixed search type.
     *   - Checks dest_x/dest_y instead of obj_x/target_x for no-target condition.
     *
     * The stack parameter is unused by 0x453450. It exists only for the
     * uniform vtable-slot signature inherited from Building::Update.
     *
     * Called by: vtable dispatch at slot[15] (0x3C)
     *
     * @param next_entity  Unused vtable parameter.
     */
    void Update(void* next_entity) override;

};

/**
 * Train deserialization factory.
 * Address: 0x435DB0
 *
 * This is not a TrainEntity method: its two vtable references are at
 * 0x477FAC and 0x478004, while the Train vtable starts at 0x4780B8 and
 * ends before 0x478130.  The factory prototype merely supplies virtual
 * RegisterEntity dispatch at slot 10.
 */
void TrainEntity_DeserializeFactory(GameObject* prototype,
                                    void* context, void* save_data);

/* ================================================================== */
/* TrainSubsystem — Network manager/subsystem singleton                */
/* ================================================================== */

/**
 * TrainSubsystem — Network manager singleton for train multiplayer subsystem.
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Standalone class (no inheritance). Manages DirectPlay peer networking,
 * train sprite rendering lists, train car linked lists (trains + carriages),
 * and network message dispatch for multiplayer train operations.
 * Also handles downloading missing assets from other players over the
 * network, including building, track, and multi-frame sprite assets.
 *
 * The subsystem manages three car lists:
 *   sprite_list_1 (+0x14) — active train controller list (ScriptedObject/Vehicle)
 *   sprite_list_2 (+0x18) — dead/orphaned train car list
 *   sprite_list_3 (+0x1C) — multiplayer persistent train list
 *   handle_list_1 (+0x28) — sender-side attachment file transfer queue
 *   handle_list_2 (+0x2C) — receiver-side attachment file transfer queue
 *
 * Instantiated by UI_NetPanel_Init (0x422820) via operator_new(0x38).
 * The singleton pointer is stored in the _g_train global (0x004FD3A4).
 *
 * Size: 0x38 bytes
 * Vtable: 0x4781C4
 *
 * Field layout:
 *   +0x00  vtable (-> 0x4781C4)
 *   +0x04  context_id_a     — first constructor parameter
 *   +0x08  context_id_b     — second constructor parameter
 *   +0x0C  byte_flags       — packed byte flags
 *   +0x0D  byte_flag_2      — additional byte flag (1 = disconnect pending)
 *   +0x10  player_peer_id   — DirectPlay player ID for network sends
 *   +0x14  sprite_list_1    — linked list head (train car objects, next at +0x70)
 *   +0x18  sprite_list_2    — linked list head (train car objects, next at +0x70)
 *   +0x1C  sprite_list_3    — linked list head (train car objects, next at +0x70)
 *   +0x20  field_20         — pointer/flag
 *   +0x24  some_limit       — limit value, initialized to 20 (0x14)
 *   +0x28  handle_list_1    — sender attachment transfer queue (PlayerConnectionNode,
 *                              HANDLE at +0x0C, next at +0x18)
 *   +0x2C  handle_list_2    — receiver attachment transfer queue (PlayerConnectionNode,
 *                              HANDLE at +0x0C, next at +0x18)
 *   +0x30  field_30         — byte flag (not initialized by 0x438BC0;
 *                              set during lobby/connection setup)
 *   +0x34  request_count    — count of pending/requested asset downloads
 *   Total: 0x38
 */

/**
 * PlayerConnectionNode — linked-list node for file attachment transfers.
 *
 * Used by handle_list_1 (sender queue) and handle_list_2 (receiver queue)
 * in TrainSubsystem.  Nodes are allocated via operator_new and linked
 * through the next pointer at +0x18.
 *
 *   +0x00: player_id       — DirectPlay player ID
 *   +0x04: sub_type        — subtype selector
 *   +0x06: extra_info      — extra info field
 *   +0x08: transfer_state  — 0=FIRST, 1=INTERIM, 2=FINAL
 *   +0x09: _pad_09
 *   +0x0A: notify_id       — receiver-side completion ID
 *   +0x0C: file_handle     — HANDLE to the file being transferred
 *   +0x10: throttle        — throttle control
 *   +0x12: sequence_num    — sequence counter
 *   +0x14: _pad_14[4]
 *   +0x18: next            — linked-list next pointer
 */
struct PlayerConnectionNode {
    int32_t   player_id;       /* +0x00 */
    uint16_t  sub_type;        /* +0x04 */
    uint16_t  extra_info;      /* +0x06 */
    uint8_t   transfer_state;  /* +0x08 (0=FIRST, 1=INTERIM, 2=FINAL) */
    uint8_t   _pad_09;         /* +0x09 */
    uint16_t  notify_id;       /* +0x0A receiver-side completion ID */
    int32_t   file_handle;     /* +0x0C */
    int16_t   throttle;        /* +0x10 */
    uint16_t  sequence_num;    /* +0x12 */
    uint8_t   _pad_14[4];      /* +0x14 */
    void*     next;            /* +0x18 */
};

class TrainSubsystem {
public:
    /* ================================================================ */
    /* Fields                                                           */
    /* ================================================================ */

    /* vtable at +0x00 is compiler-managed via virtual methods */
    int32_t    context_id_a;        // +0x04  first ctor param (from panel descriptor)
    int32_t    context_id_b;        // +0x08  second ctor param (from panel descriptor)
    uint8_t    byte_flags;          // +0x0C  packed byte flags
    uint8_t    byte_flag_2;         // +0x0D  additional byte flag (1=disconnect pending)
    int32_t    player_peer_id;      // +0x10  DirectPlay player ID for network sends
    InboundTrainNode*  sprite_list_1;   // +0x14  active train controller list (next at +0x70)
    InboundTrainNode*  sprite_list_2;   // +0x18  dead/orphaned train car list (next at +0x70)
    InboundTrainNode*  sprite_list_3;   // +0x1C  multiplayer persistent train list (next at +0x70)
    void*              field_20;        // +0x20  pointer/flag
    int32_t            some_limit;      // +0x24  initialized to 20 (0x14)
    PlayerConnectionNode* handle_list_1; // +0x28  sender attachment transfer queue
    PlayerConnectionNode* handle_list_2; // +0x2C  receiver attachment transfer queue
    uint8_t    field_30;            // +0x30  byte flag; ctor leaves it untouched
    uint8_t    _pad_31[3];          // +0x31  padding
    int32_t    request_count;       // +0x34  count of requested asset downloads

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * Constructor — initializes DirectPlay networking subsystem.
     * Address: 0x438BC0
     *
     * Initializes the documented fields except field_30 (+0x30), which the
     * binary deliberately leaves untouched; sets vtable to 0x4781C4.
     * If not in demo mode: calls InitNetwork, enumerates DirectPlay
     * connections, queries connection capabilities for 4 connections.
     *
     * Called by: UI_NetPanel_Init (0x422820) with panel descriptor fields.
     *
     * @param context_a  Panel context ID (param_1[1] from caller)
     * @param context_b  Panel context ID (param_1[3] from caller)
     */
    TrainSubsystem(int context_a, int context_b);

    /**
     * Base destructor — real cleanup logic.
     * Address: 0x438CC0
     *
     * Flushes pending network messages, destroys DirectPlay peer,
     * frees all 3 sprite linked lists (destroying each node via
     * its vtable[0] destructor), drains the network message queue,
     * and frees handle-tracked lists (CloseHandle + GLOBAL_free).
     */
    void BaseDtor();

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x438CA0
     *
     * In the binary: scalar deleting destructor at 0x438CA0.
     * Natural C++: compiler-generated from virtual destructor chain.
     *
     * @param flags  If bit 0 set, also free the object's memory.
     * @return       The object pointer (this).
     */
    virtual ~TrainSubsystem();

    /* ================================================================ */
    /* Network initialization and messaging                              */
    /* ================================================================ */

    /**
     * InitNetwork — Create/re-create the DirectPlay peer handle.
     * Address: 0x4391A0
     *
     * Destroys existing g_dplay_peer, allocates 0x160c bytes, calls
     * DirectPlay_CreatePeer with context_id_a, stores context_id_b at
     * peer+0x938. SEH-guarded.
     *
     * Called by:
     *   - TrainSubsystem constructor (0x438C11)
     *   - Train_DispatchMessage (0x43957D) — on case 0 before HostSession
     */
    void InitNetwork();

    /**
     * QueueMessage — Enqueue (or discard/process) a network message.
     * Address: 0x4393D0
     *
     * In multiplayer (g_game_mode==10): messages are handled immediately
     * or freed. In single-player/offline: appends to g_network_queue linked
     * list under critical section (EnterCriticalSection wrapper). Throttles type-6
     * (SendNetworkData) messages when queue depth >= 6.
     *
     * Called by: Train_FlushMessages, Train_UpdateTrainMovement, many others.
     *
     * @param msg  Pointer to a 0x1c-byte network message struct.
     */
    void QueueMessage(void* msg);

    /**
     * FlushMessages — Flush pending network messages and wait for
     *                 network thread to finish.
     * Address: 0x4394E0
     *
     * Sends a DISCONNECT (type-8) message if thread is active, then
     * spin-waits via Sleep(10) until the thread terminates.
     *
     * BUG: Null pointer dereference if operator_new fails during
     * disconnect message allocation.
     *
     * Called by:
     *   - TrainSubsystem::BaseDtor (0x438CE8)
     */
    void FlushMessages();

    /**
     * DispatchMessage — Process a single network message from the queue.
     * Address: 0x439550
     *
     * Dispatches by type:
     *   0 = HostSession, 1 = StartMultiplayer, 2 = StopMultiplayer,
     *   3 = ShutdownNetwork, 5 = HandleDisconnect,
     *   6 = SendNetworkData, 8 = DisconnectPending,
     *   14 = HandleJoinMultiplayer, 16 = HandleFileTransfer,
     *   25 = UpdatePlayerCount+Reset
     *
     * Called by: Train_ProcessMessages message dispatch loop.
     *
     * @param msg  Pointer to a 0x1c-byte network message struct.
     */
    void DispatchMessage(void* msg);

    /**
     * ProcessMessages — Main network message processing loop.
     * Address: 0x4396C0
     * Size: 1508 bytes
     *
     * Polls DirectPlay for incoming messages (type 0x0A-0x3FD),
     * dispatches to type-specific handlers for player join/leave,
     * train pos sync, signal changes, file transfers, chat,
     * connection setup, game over, and building data.
     *
     * Loop exits when g_dplay_peer is NULL or no messages remain.
     */
    void ProcessMessages();

    /* ================================================================ */
    /* Player management                                                 */
    /* ================================================================ */

    /**
     * HandlePlayerJoin — Process incoming player join message (type 0x3FB).
     * Address: 0x439D00
     *
     * Allocates a PlayerConnectionNode, opens the player's .att file for
     * reading, and appends the node to handle_list_1 (+0x28).
     *
     * @param data    Incoming message data buffer
     * @param player_id  DirectPlay player ID of the joining player
     */
    void HandlePlayerJoin(void* data, int player_id);

    /**
     * UploadPendingAttachments — Sender side of attachment file transfer.
     * Address: 0x439DF0
     *
     * Iterates PlayerConnectionNode linked list (handle_list_1, +0x28),
     * reads chunks from source .att/.dat files, and sends them as
     * DirectPlay messages (type 0x3FC) with sub-types:
     *   0 = FIRST block, 1 = INTERIM block, 2 = FINAL block.
     * 20-tick throttle per node.
     *
     * Counterpart to HandleAttachmentFileData.
     */
    void UploadPendingAttachments();

    /**
     * HandleAttachmentFileData — Receiver side of attachment file transfer.
     * Address: 0x43A140
     *
     * Receives DirectPlay messages type 0x3FC (internal type 0x12).
     * Three sub-types: 0=FIRST (create .att), 1=INTERIM (write append),
     * 2=FINAL (create .dat, notify completion via type 0x18).
     *
     * Counterpart to UploadPendingAttachments.
     *
     * @param data  Incoming attachment file data buffer
     */
    void HandleAttachmentFileData(void* data);

    /**
     * HandleTrainPosUpdate — Process remote player train position update.
     * Address: 0x43A4B0
     *
     * If the transferred train belongs to the local player: reverses
     * direction (180-deg flip), takes local control, sends type 0x11
     * notification to UI.
     *
     * @param data           Position update data buffer
     * @param player_index   Index of the player sending the update
     */
    void HandleTrainPosUpdate(void* data, int player_index);

    /**
     * HandlePlayerLeave — Handle player disconnecting from multiplayer.
     * Address: 0x43A5C0
     *
     * Finds player index via NETMAN_FindPlayerIndex, updates counts,
     * resets trains, removes attachment transfers, sends cancellation
     * and player-left broadcast.
     *
     * @param player_id  DirectPlay player ID of the leaving player
     */
    void HandlePlayerLeave(int player_id);

    /**
     * UpdatePlayerCount — Remove PlayerSlot nodes matching player_index
     *                     from sprite_list_3 (+0x1C).
     * Address: 0x43A6D0
     *
     * Local player's slots preserved on free list (sprite_list_1).
     * Remote player's slots destroyed via vtable[0] destructor.
     *
     * @param player_index  Player index to remove
     */
    void UpdatePlayerCount(uint32_t player_index);

    /* ================================================================ */
    /* Network lifecycle                                                 */
    /* ================================================================ */

    /**
     * ShutdownNetwork — Full network teardown.
     * Address: 0x43AA00
     *
     * Reconnects to session via DirectPlay_ConnectToSession to send
     * type-3 (Shutdown) or type-5 (Disconnect). Three connection paths
     * depending on host/client and scenario mode.
     */
    void ShutdownNetwork();

    /**
     * HandleDisconnect — Clean up network state on disconnect.
     * Address: 0x43AC10
     *
     * Closes/frees DirectPlay peer, destroys all car lists
     * (+0x14/+0x18/+0x1C). Sends 0x3FD game-over if scenario.
     */
    void HandleDisconnect();

    /**
     * HandleFileTransfer — Dispatch a train car to a neighbor town.
     * Address: 0x43AD00
     *
     * Computes target from direction + currentTownIndex. Routes via
     * MoveToNeighborTown or AddTrainCar.
     *
     * @param msg  Network message data for the car transfer
     */
    void HandleFileTransfer(void* msg);

    /**
     * MoveToNeighborTown — Transfer a train car to a neighbor player.
     * Address: 0x43AE20
     * Size: 1016 bytes
     *
     * @param to_player  Target player's DirectPlay ID
     * @param car        Train car (ScriptedObject) to transfer
     * @param direction  Movement direction
     * @return           1 on success, 0 on failure
     */
    uint32_t MoveToNeighborTown(int to_player, void* car, int direction);

    /**
     * HandleConnectionSetup — Receive MSG_CONN_SETUP (0x3F2) message.
     * Address: 0x43B240
     * Size: 1162 bytes
     *
     * @param data  Incoming MSG_CONN_SETUP message buffer
     */
    void HandleConnectionSetup(void* data);

    /**
     * HandleControllerInit — Init controller from MSG_CTRL_INIT (0x3F3).
     * Address: 0x43B6D0
     *
     * @param data     Incoming MSG_CTRL_INIT message buffer
     * @param dplay_id DirectPlay ID for the controller session
     */
    void HandleControllerInit(void* data, int dplay_id);

    /**
     * ResetMultiplayerState — Reset multiplayer state for a player.
     * Address: 0x43B770
     *
     * In scenario mode: finds matching cars, unlinks, reverses direction,
     * reassigns to local player, clears track DPlay data.
     *
     * @param player_id  DirectPlay player ID to reset (0=local)
     */
    void ResetMultiplayerState(int player_id);

    /**
     * AddTrainCar — Add a train car to the controller's lists.
     * Address: 0x43B8C0
     *
     * @param car            Train car data (ScriptedObject/Vehicle)
     * @param direction      Movement direction
     * @param player_index   Target player index
     */
    void AddTrainCar(void* car, int direction, int player_index);

    /**
     * UpdateTrainMovement — Main train physics update tick.
     * Address: 0x43BB00
     * Size: 1622 bytes
     */
    void UpdateTrainMovement();

    /**
     * RouteTrainAtEdge — Handle trains at map edges.
     * Address: 0x43C160
     *
     * Checks track connections to neighbor towns. Routes via
     * MoveToNeighborTown or AddTrainCar. Reverses direction if no
     * connection (bounce).
     *
     * @param prev_node   Previous node in train list (for unlink)
     * @param train       Train car to route
     * @param pos_x       Current tile X position
     * @param pos_y       Current tile Y position
     * @param map_width   Map width in tiles
     * @param map_height  Map height in tiles
     * @return            1 if routed, 0xFFFFFF01 if bounced
     */
    uint32_t RouteTrainAtEdge(void* prev_node, void* train,
                              int pos_x, int pos_y,
                              int map_width, int map_height);

    /**
     * HandleJoinMultiplayer — Handle a join-multiplayer message.
     * Address: 0x43C410
     * Size: 1089 bytes
     *
     * @param msg  Network message data for the join request
     */
    void HandleJoinMultiplayer(void* msg);

    /**
     * DownloadMissingAssets — Check/request missing train assets from peers.
     * Address: 0x438E40
     *
     * Scans entity asset array, deduplicates missing entries, and sends
     * type-0x03ED asset request messages for each missing asset.
     *
     * @param entity  The game entity whose assets to verify
     */
    void DownloadMissingAssets(void* entity);

    /**
     * RemoveAllCars — Remove all cars from the active controller list.
     * Address: 0x43CBE0
     *
     * Iterates sprite_list_1 (+0x14), queues NETMAN type-0xF (RemoveCar)
     * messages for each car, unlinks, marks dead (+0x88 = 0).
     */
    void RemoveAllCars();
};

/**
 * Train_StartMultiplayer — Build session address, start multiplayer.
 * Address: 0x43A760
 */
void __cdecl Train_StartMultiplayer(void);

/**
 * Train_StopMultiplayer — Graceful multiplayer shutdown.
 * Address: 0x43A8B0
 */
void __cdecl Train_StopMultiplayer(void);

/**
 * Train_HandleLobbyInfo — Constructor callback for track element slots.
 * Address: 0x43B220
 */
int __fastcall Train_HandleLobbyInfo(int buf);
