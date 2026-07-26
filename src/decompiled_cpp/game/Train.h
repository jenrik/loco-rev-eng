/**
 * Train.h — Train game object, TrainSubsystem (network manager), and
 *           TrainStationWindow (sprite rendering) classes
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * There are THREE distinct data structures with "Train" in their names:
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
 * 3. TrainStationWindow — UI panel window showing train car sprite animation.
 *    Vtable: 0x478130. Size: ~0x1D4 bytes (+0x1D4 highest known offset = 468).
 *    Represents the popup window that displays when clicking a train station,
 *    with animated train car sprite rendering, text overlays, and multiple
 *    animation states (idle, moving, doors opening/closing).
 *    Class hierarchy: GameWindow -> TrainStationWindow
 */

#pragma once

#include "Building.h"

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
     * Calling convention: __thiscall (ECX = this, 1 stack arg), RET 0x4
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
     * Calling convention: __thiscall (ECX = this, no stack args), RET
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
     * Calling convention: __thiscall (ECX = this, 1 byte-stack arg), RET 0x4
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
     * Calling convention: __thiscall (ECX = this, 1 stack arg), RET 0x4
     *
     * Simplified update compared to Building::Update:
     *   1. Skips if disabled (+0x89 != 0).
     *   2. Calls Building::CheckTimeout (decrements occupant level if expired).
     *   3. If movement active (field_dc != 0):
     *      a. Calls vtable[22] (IsMovementActive) — if still moving, skip.
     *      b. If arrived: checks if position matches target_x/target_y.
     *         - At target: calls Building::HandleAction(this, last_action).
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
    void*      sprite_list_1;       // +0x14  active train controller list (next at +0x70)
    void*      sprite_list_2;       // +0x18  dead/orphaned train car list (next at +0x70)
    void*      sprite_list_3;       // +0x1C  multiplayer persistent train list (next at +0x70)
    void*      field_20;            // +0x20  pointer/flag
    int32_t    some_limit;          // +0x24  initialized to 20 (0x14)
    void*      handle_list_1;       // +0x28  sender attachment transfer queue (PlayerConnectionNode)
    void*      handle_list_2;       // +0x2C  receiver attachment transfer queue (PlayerConnectionNode)
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
     * list under critical section (RESDATA_Lock). Throttles type-6
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

/* ================================================================== */
/* TrainStationWindow — UI popup for train car sprite animation        */
/* ================================================================== */

/**
 * TrainStationWindow — UI panel displaying animated train car sprites.
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * NOT a C++ class — this is a C struct used by free functions that render
 * the train station popup window. Shows an animated train car (selected by
 * train_type at +0x124) with sprite sheet animation, destination markers,
 * and text overlays (e.g., "PLEASE WAIT" for delivery trains).
 *
 * Class hierarchy: GameWindow (base, sets +0x00..+0x10 fields via
 *   GameWindow_Ctor) -> TrainStationWindow (sets vtable to 0x478130).
 *
 * Fields documented from offset accesses in Train_LoadSprites (0x437670),
 * Train_BlitSprite (0x437900), Train_DrawTextOverlay (0x437CF0),
 * Train_SetAnimState (0x438280), Train_UpdateAnim (0x438590),
 * Train_Hide (0x438890), Train_SendMessage (0x4370F0),
 * Train_HandleClick (0x438AD0), TrainStationWindow_Ctor (0x436B20),
 * TrainStationWindow_BaseDtor (0x436BB0), TrainStationWindow_Create (0x436C50),
 * TrainStationWindow_UpdateTooltip (0x436D60), TrainStationWindow_Show (0x436EC0),
 * and TrainStationWindow_Hide (0x436F70).
 *
 * Size: ~0x1D4 bytes (highest known offset: +0x1D4 = +0x1C4 tooltip_saved_rect + 16).
 * Vtable: 0x478130 (VTBL_TRAIN_STATION_WINDOW)
 *
 * Vtable layout (partial):
 *   [0] +0x00: scalar deleting destructor (TrainStationWindow_Dtor, 0x436B90)
 *   [1] +0x04: Hide / InvalidateRect-style method (parent window invalidation)
 *   [2] +0x08: Show / display method (inherited from GameWindow)
 *   [3] +0x0C: sound play method (called from Train_HandleClick)
 *   [6] +0x18: update_client_rect / on_show (inherited from GameWindow)
 */
struct TrainStationWindow {
    /* vtable at +0x00 is compiler-managed via virtual methods */
    void*    hInstance;             /* +0x04  — application instance handle (LoadIconA) */
    void*    hwnd;                  /* +0x08  — Win32 window handle */
    /* +0x0C..+0x37: (gap — 44 bytes, likely more UI state) */
    void*    palette;               /* +0x38  — palette/CLUT pointer used by UIPANEL_Blit */
    /* +0x3C..+0x63: (gap — 40 bytes) */
    void*    hdc_cache;             /* +0x64  — cached HDC from Cursor_WaitForBlit */
    /* +0x68..+0x8F: (gap — 40 bytes) */

    /* Sound resource parameters for vtable[3] click audio dispatch */
    int32_t  sound_param_1;         /* +0x90  — first sound param (default click region) */
    /* +0x94: (gap — 4 bytes) */
    int32_t  sound_param_2;         /* +0x98  — second sound param (default click region) */
    int32_t  sound_param_3;         /* +0x9C  — first sound param (rect 1/2 click regions) */
    /* +0xA0: (gap — 4 bytes) */
    int32_t  sound_param_4;         /* +0xA4  — second sound param (rect 1/2 click regions) */

    /* +0xA8..+0x117: (gap — 112 bytes) */
    char     hide_state;            /* +0x118 — hide notification flag for WM_USER+1 PostMessage */
    /* +0x119: (3 bytes padding) */
    int32_t  hide_param;            /* +0x11C — lParam value for WM_USER+1 hide notification */
    char     visible;               /* +0x120 — visibility flag (0=hidden, 1=visible) */
    /* +0x121..+0x123: (3 bytes padding) */
    int32_t  train_type;            /* +0x124 — train car type index (0-13) */
    void*    icon;                  /* +0x128 — HICON handle (loaded from resource 0x65) */

    /* Clickable region RECTs for hit-testing mouse clicks (PtInRect).        */
    /* RECT 1 (+0x12C) / RECT 2 (+0x13C) are the train car animation area;    */
    /* hits trigger sound_param_3/sound_param_4 audio and anim_state 1/2.     */
    /* RECT 3 (+0x15C) is a dismissal region; hits skip anim state changes.  */
    RECT     click_rect_1;          /* +0x12C — first clickable region (16 bytes) */
    RECT     click_rect_2;          /* +0x13C — second clickable region (16 bytes) */

    /* Destination rectangle — used as source RECT for sprite blitting */
    int32_t  dst_left;              /* +0x14C */
    int32_t  dst_top;               /* +0x150 */
    int32_t  dst_right;             /* +0x154 */
    int32_t  dst_bottom;            /* +0x158 */

    RECT     click_rect_3;          /* +0x15C — third clickable region (dismissal, 16 bytes) */

    char     sprites_loaded;        /* +0x16C — 1 after sprites loaded, 0 before */
    char     sound_played;          /* +0x16D — 1 after station-open sound played */

    /* Sprite resource pointers */
    void*    car_sprite_res;        /* +0x170 — train car sprite resource object (vtable) */
    void*    car_surface;           /* +0x174 — locked surface for car sprite */
    void*    bg_sprite_res;         /* +0x178 — background sprite resource object */
    void*    bg_surface;            /* +0x17C — locked surface for background */
    void*    frame_data_res;        /* +0x180 — frame data/animation descriptor resource */
    void*    frame_surface;         /* +0x184 — locked surface for frame data */
    void*    dest_data_res;         /* +0x188 — sprite destinations resource (for window sizing) */
    void*    dest_surface;          /* +0x18C — locked surface for destinations */

    /* Animation state */
    int32_t  frame_index;           /* +0x190 — current animation sub-frame index */
    int32_t  state_counter;         /* +0x194 — animation state counter/offset into table */
    int32_t  cur_state;             /* +0x198 — current state, initialized to 0 */
    void*    timer;                 /* +0x19C — UINT_PTR timer ID from SetTimer */

    /* Frame range */
    int16_t  anim_state;            /* +0x1A0 — animation state (0=idle, 1=fwd, 2=bkwd, etc.) */
    int16_t  anim_state_pad;        /* +0x1A2 — padding */
    /* +0x1A4: (gap — 4 bytes) */
    int32_t  end_frame;             /* +0x1A8 — max frame index to animate */
    int32_t  cur_start_frame;       /* +0x1AC — current start frame (copied from start_frame) */
    int32_t  start_frame;           /* +0x1B0 — first frame index (always 0) */
    int32_t  wrap_frame;            /* +0x1B4 — wrap-around frame index */
    int32_t  anim_tick_counter;     /* +0x1B8 — animation tick counter */
    char     tooltip_visible;       /* +0x1BC — tooltip visibility flag */
    void*    tooltip_ptr;           /* +0x1C0 — tooltip object pointer (or NULL) */
    RECT     tooltip_saved_rect;    /* +0x1C4 — saved tooltip rect for invalidation tracking (16 bytes) */
};

/* ================================================================== */
/* Train sprite rendering and window functions (C free functions)      */
/* ================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TrainStationWindow_Ctor — Constructor.
 * Address: 0x436B20
 */
void* __thiscall
TrainStationWindow_Ctor(TrainStationWindow* window, int32_t param1, int32_t param2);

/**
 * TrainStationWindow_Dtor — Scalar deleting destructor (vtable[0]).
 * Address: 0x436B90
 */
void* __thiscall
TrainStationWindow_Dtor(TrainStationWindow* window, byte flags);

/**
 * TrainStationWindow_BaseDtor — Base destructor body.
 * Address: 0x436BB0
 */
void __thiscall
TrainStationWindow_BaseDtor(TrainStationWindow* window);

/**
 * TrainStationWindow_Create — Create the Win32 window.
 * Address: 0x436C50
 */
int32_t __thiscall
TrainStationWindow_Create(TrainStationWindow* window, void* parent_hwnd);

/**
 * TrainStationWindow_UpdateTooltip — Update tooltip position.
 * Address: 0x436D60
 */
void __thiscall
TrainStationWindow_UpdateTooltip(TrainStationWindow* window);

/**
 * TrainStationWindow_Show — Show the window with animation.
 * Address: 0x436EC0
 */
void __thiscall
TrainStationWindow_Show(TrainStationWindow* window, int32_t train_type, int32_t hide_param);

/**
 * TrainStationWindow_Hide — Hide the window and clean up.
 * Address: 0x436F70
 */
void __thiscall
TrainStationWindow_Hide(TrainStationWindow* window);

/**
 * Train_LoadSprites — Load all train station sprite resources.
 * Address: 0x437670
 */
void __fastcall Train_LoadSprites(TrainStationWindow* window);

/**
 * Train_BlitSprite — Blit a single train animation frame.
 * Address: 0x437900
 */
void __thiscall
Train_BlitSprite(TrainStationWindow* window, uint32_t* src_rect,
                 int32_t frame_idx, int32_t unused, void* surface);

/**
 * Train_DrawTextOverlay — Draw "PLEASE WAIT" shadow text.
 * Address: 0x437CF0
 */
void __fastcall Train_DrawTextOverlay(TrainStationWindow* window);

/**
 * Train_SetAnimState — Set the animation to a new state.
 * Address: 0x438280
 */
void __thiscall Train_SetAnimState(TrainStationWindow* window, uint16_t new_state);

/**
 * Train_UpdateAnim — Advance animation by one frame.
 * Address: 0x438590
 */
void __thiscall Train_UpdateAnim(TrainStationWindow* window, int32_t unused);

/**
 * Train_Hide — Hide window with sound and WM_USER+1 notify.
 * Address: 0x438890
 */
int32_t __thiscall
Train_Hide(TrainStationWindow* window, int32_t unused1, int32_t unused2,
           int32_t unused3, int32_t unused4);

/**
 * Train_SendMessage — Post WM_USER+1 hide notification.
 * Address: 0x4370F0
 */
void __fastcall Train_SendMessage(TrainStationWindow* window);

/**
 * Train_HandleClick — Process mouse clicks.
 * Address: 0x438AD0
 */
int32_t __thiscall
Train_HandleClick(TrainStationWindow* window, uint32_t lParam,
                  int32_t arg2, int32_t arg3, int32_t arg4);

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

#ifdef __cplusplus
}
#endif
