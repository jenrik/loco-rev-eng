# Lego Loco — Network Protocol (NETMAN)

## Overview

Lego Loco (1998) supports multiplayer sessions for up to 9 players over a TCP/IP network. The networking subsystem — called **NETMAN** — is built on Microsoft DirectPlay 4 (IDirectPlay4A) and implements a client/host architecture for sharing trains, signal changes, player avatars, and session state.

On non-Windows hosts, DirectPlay transport is being replaced by the explicitly framed SDL_net TCP protocol documented in `docs/sdl-net-wire-protocol.md`. The original application payloads and version-300 field remain authoritative; the outer envelope, virtual player IDs, DNS-SD discovery, and host relay are host-only additions.

The core NETMAN object is 0x804 bytes (2052 bytes), constructed at `NETMAN_Constructor` (`0x0043d0a0`). The Train subsystem (`TrainSubsystem`) acts as the message dispatch target and also handles its own set of DirectPlay message types for asset transfers, connection setup, and controller synchronization.

## Architecture

```
┌─────────────────────────────────────────────┐
│                Game Loop                     │
│  NETMAN_Update() ── per-frame tick          │
└──────────────────┬──────────────────────────┘
                   │
     ┌─────────────┴──────────────┐
     │         NETMAN             │  (0x804 bytes)
     │  ┌──────────────────────┐  │
     │  │ 9 × PlayerSlot       │  │  (+0x518, 0x4C each)
     │  │  - dpId, name, avatar│  │
     │  │  - outbound train list│  │
     │  ├──────────────────────┤  │
     │  │ InboundTrain list    │  │  (+0x7E0, singly-linked)
     │  ├──────────────────────┤  │
     │  │ Session state        │  │  (role, dpids, tick counters)
     │  └──────────────────────┘  │
     │  NETMAN_DispatchMessage()  │  — app-layer message dispatch
     │  NETMAN_ProcessInbound()   │  — train delivery
     │  NETMAN_SendOutbound()     │  — periodic outbound broadcast
     └──────────┬─────────────────┘
                │
     ┌──────────┴──────────────────┐
     │     TrainSubsystem           │
     │  ProcessMessages()           │  — wire protocol dispatch (0x3E9–0x3FD)
     │  QueueMessage()              │  — send queue with throttling
     │  InitNetwork()               │  — DirectPlay peer creation
     └──────────┬──────────────────┘
                │
     ┌──────────┴──────────────────┐
     │      DirectPlay 4            │
     │  CreatePlayer, Open, Send,   │
     │  Receive, Close, EnumSessions│
     └─────────────────────────────┘
```

## Wire Protocol

### Wire Version

Every application packet carries a 16-bit version field at bytes `[2..3]` with value **300** (`0x012C`). `DirectPlayReceive` (`0x004606d0`) rejects any packet whose version field doesn't match.

Defined in `src/network/netman.h` as `LOCO_WIRE_VERSION`.

### Generic Wire Header

All multibyte fields are little-endian.

```
Offset  Size  Field
+0x00   2     pkt_type    — packet type code (e.g. 0x3F1)
+0x02   2     version     — always 300 (0x012C), stamped by DirectPlaySend()
```

Defined as `WireHeader` in `src/network/netman.h`.

### DirectPlaySend — `0x00460d40`

Thin wrapper around `IDirectPlay4A::Send` (vtable `+0x68`). Before sending, stamps `LOCO_WIRE_VERSION` at `buf[2..3]`. For grouped sends, uses `IDP4::SendEx` (vtable `+0xC4`) with `flags | 0x600`.

Function: `DirectPlaySend()` in `src/network/netman.c`

### DirectPlayReceive — `0x004606d0`

Receive loop calling `IDirectPlay4A::Receive` (vtable `+0x64`). Validates wire version field at `buf[2..3] == 300`. Translates DP system messages into internal events:

| DP System ID | Meaning              |
|-------------|----------------------|
| 0x31        | Player lost          |
| 0x03        | Player added         |
| 0x05        | Player info          |
| 0x101       | Session joined       |
| 0x103       | Player data          |
| 0x104       | Hostname changed     |

Returns an 8-byte `DPRecvNode { dpid, buf }` for application dispatch.

Function: `DirectPlayReceive()` in `src/network/netman.c`

## Application-Layer Packet Types (NETMAN)

These are internal message types dispatched by `NETMAN_DispatchMessage` (`0x0043f2b0`). Defined as `PKT_*` constants in `src/network/netman.h`.

| Type | Name                     | Direction      | Description                          |
|------|--------------------------|----------------|--------------------------------------|
| 0x03 | PLAYER_JOINED            | host→client    | Copy host DPID + name to slot[0]    |
| 0x04 | PLAYER_DATA              | host→client    | Update player slot (dpid, name, color) |
| 0x09 | SESSION_STATE            | host→all       | Full session state (9 slot DPIDs)    |
| 0x0B | PLAYER_LEFT              | any            | Remove player by DPID                |
| 0x0C | SESSION_RESET            | any            | Reset session, return to lobby        |
| 0x0F | QUEUE_INBOUND_TRAIN      | host→client    | Host queues train for client delivery |
| 0x11 | DELIVER_TRAIN_TO_CLIENT  | host→client    | Deliver inbound train descriptor      |
| 0x12 | TRAIN_POS_UPDATE_A       | client→host    | Train position update (variant A)     |
| 0x13 | AVATAR_READY             | any            | Set slot ready flag (active \|= 0x02) |
| 0x14 | AVATAR_GONE              | any            | Clear slot ready flag (active &= ~0x02)|
| 0x15 | TRAIN_POS_UPDATE_B       | client→host    | Train position update (variant B)     |
| 0x16 | AVATAR_DATA_RECEIVED     | any            | Avatar pixel data received            |
| 0x17 | TRAIN_POS_UPDATE_C       | client→host    | Train position update (variant C)     |
| 0x18 | TRAIN_TRANSFER_ACK       | client→host    | Acknowledge train transfer receipt    |
| 0x1A | TRAIN_TRANSFER_CANCEL    | host→client    | Host rejects/cancels train transfer   |
| 0x1B | HOST_SYNC                | host→client    | Host requests avatar re-sync         |
| 0x1C | FUN_440820               | any            | Timeout check trigger                 |

Dispatch logic in `NETMAN_DispatchMessage()` in `src/network/netman.c`.

## Wire Packet Types (DirectPlay Layer)

These are the `uint16_t` packet type codes written at `buf[0..1]` and dispatched by `TrainSubsystem::ProcessMessages` (`0x4396c0`) and `DispatchReceivedMsgs` (`0x396c0`). Defined in `src/network/netman.h` and `src/decompiled_cpp/network/Netman.h`.

### Packet Type Reference (0x3E8–0x3FD)

| Wire Type | Name              | Size      | Description                                  | Dispatch                                                |
|-----------|-------------------|-----------|----------------------------------------------|---------------------------------------------------------|
| 0x3E8     | Ping              | 4         | Ping request; responder sends 0x3E9           | `DispatchReceivedMsgs` in `src/network/netman.c`       |
| 0x3E9     | Pong              | 24        | Ping response (24 bytes)                      | `TrainSubsystem::ProcessMessages` in `Train_network.cpp` |
| 0x3EA     | PlayerInfo        | var       | Player info/config broadcast                  | `TrainSubsystem::ProcessMessages`                       |
| 0x3EB     | ConnectToServer   | var       | Client connection to server                   | `TrainSubsystem::ProcessMessages`                       |
| 0x3EC     | HandleTrackBuild  | var       | Track build event                             | `TrainSubsystem::ProcessMessages`                       |
| 0x3EE     | FileData          | var       | Incoming asset file chunk                     | `TrainSubsystem::ProcessMessages`                       |
| 0x3F0     | GameOver          | var       | Game over control signal                      | `TrainSubsystem::ProcessMessages`                       |
| 0x3F1     | TrainState        | 552       | Full train state (header + 9×60-byte slots)   | `DispatchReceivedMsgs` → `DeserializeTrainSlot`         |
| 0x3F2     | MapSync           | 2844      | Full track/junction layout for a player       | `SendMapSync` in `src/network/netman.c`                |
| 0x3F3     | ControllerInit    | 10        | Track controller initialization               | `TrainSubsystem::ProcessMessages`                       |
| 0x3F4     | PlayerJoin        | var       | Player join request                           | `TrainSubsystem::ProcessMessages`                       |
| 0x3F5     | PlayerCount       | var       | Player count update                           | `TrainSubsystem::ProcessMessages`                       |
| 0x3F6     | TrainPosUpdate    | var       | Train position update (outer msg)             | `DispatchReceivedMsgs` (retained)                       |
| 0x3F7     | TrainPos          | 12        | Train position / accepted                     | `DispatchReceivedMsgs` → `HandleTrainPosUpdate`         |
| 0x3F8     | PlayerCountUpdate | var       | Player count + update broadcast               | `TrainSubsystem::ProcessMessages`                       |
| 0x3F9     | AvatarData        | var       | Map overlay pixel data                        | `DispatchReceivedMsgs` (retained)                       |
| 0x3FA     | PlayerReady       | 4         | Player ready notification                     | `NETMAN_SendPlayerReady` in `src/network/netman.c`     |
| 0x3FB     | HandlePlayerJoin  | var       | Player join handling                          | `TrainSubsystem::ProcessMessages`                       |
| 0x3FC     | AttachmentFile    | var       | File attachment transfer (3-phase)            | `TrainSubsystem::ProcessMessages`                       |
| 0x3FD     | PlayerLeave       | 4         | Player leave notification                     | `TrainSubsystem::ProcessMessages`                       |

### Packet Format Details

#### 0x3E8 — Ping / 0x3E9 — Pong

```
Ping (4 bytes):
  +0x00: uint16  type = 0x3E8
  +0x02: uint16  version

Pong (24 bytes):
  +0x00: uint16  type = 0x3E9
  +0x02: uint16  version
  +0x04: int32   player_config+0x14
  +0x08: int32   main_window+0x18
  +0x0C: int32   main_window+0x1C
  +0x10: int32   main_window+0x20
  +0x14: int32   main_window+0x24
```

Dispatched in `DispatchReceivedMsgs` (ping) and `TrainSubsystem::ProcessMessages` (pong, case 1000) in `Train_network.cpp`.

#### 0x3F1 — TrainState (552 bytes)

```
+0x00: uint16  type = 0x3F1
+0x02: uint16  version = 300
+0x04: int32   from_player_id (sender DPID)
+0x08: uint8   flag_byte
+0x09: uint8   extra_byte
+0x0A: uint8   _pad[2]
+0x0C: uint8   slots[9][60] — nine serialized train slots
```

Each 60-byte wire slot is produced by `SerializeTrainSlot` (`0x004427d0`) and consumed by `DeserializeTrainSlot` (`0x004426d0`). The wire format packs a 76-byte in-memory slot:

| Wire Offset | Source Offset | Size | Field                        |
|------------|---------------|------|------------------------------|
| +0x00      | src[0x00]     | 4    | dword ID                     |
| +0x04      | src[0x32]     | 4    | dword value                  |
| +0x08      | src[0x08]     | 4    | dword value 2                |
| +0x0C      | src[0x14]     | 12   | player name (strncpy)        |
| +0x19      | src[0x12]     | 31   | layout name (strncpy)        |
| +0x39      | src[0x36]     | 1    | flag byte                    |
| +0x3A      | src[0x04]     | 1    | flag byte 2                  |

Functions: `SerializeTrainSlot()`, `DeserializeTrainSlot()` in `src/network/netman.c`.

#### 0x3F2 — MapSync (2844 bytes)

```
+0x00: uint16  type = 0x3F2
+0x02: uint16  version
+0x04: ...     speed, junction_count, player_reference, per-junction entries
```

Contains full track/junction layout with car references and 932-byte car-data blobs. Sent by `SendMapSync` (`0x0043ae20`) to a specific destination player.

Function: `SendMapSync()` in `src/network/netman.c`.

#### 0x3F3 — ControllerInit (10 bytes)

```
+0x00: uint16  type = 0x3F3
+0x02: uint16  version
+0x04: uint16  train_id / resource ID
+0x06: uint8   color/type
+0x07: uint8   owner slot
+0x08: uint16  direction
```

Sent when a train controller is initialized, broadcast to all players. Handled by `TrainSubsystem::HandleControllerInit` (`0x43B6D0`) in `Train_network.cpp`.

#### 0x3F6 — Train Position Update (outbound trains, variable)

```
+0x00: uint16  type = 0x3F6
+0x02: uint16  version
+0x04: entry[0..N] — 8 bytes each:
       +0: int32   DPID of destination player
       +4: int16   screen_x
       +6: int16   screen_y
```

Built by `NETMAN_SendOutboundTrains` (`0x43ded0`). Sent periodically by clients every `outbound_interval` ticks (default `0x0F`).

Function: `NETMAN_SendOutboundTrains()` in `src/network/netman.c`.

#### 0x3F7 — TrainPos / TrainAccepted (12 bytes)

```
+0x00: uint16  type = 0x3F7
+0x02: uint16  version
+0x04: int32   train_id (game-engine train handle)
+0x08: uint8   direction
+0x09: uint8   track_slot (player slot index)
+0x0A: uint8   _pad[2]
```

Defined as `WireTrainPos` in `src/network/netman.h`. Used for both train position updates and train-accepted acknowledgments.

Functions: `SendTrainPositionUpdate()` in `src/network/netman.c`, `TrainSubsystem::HandleTrainPosUpdate` in `Train_network.cpp`.

#### 0x3F9 — AvatarData (variable)

```
+0x00: uint16  type = 0x3F9
+0x02: uint16  version
+0x04: int32   width
+0x08: int32   height
+0x0C: int32   pixel_count
+0x10: uint8[] raw pixel data
```

Contains the minimap/building overlay pixel data. Built by `NETMAN_SendAvatarToHost` (`0x43d350`) which calls the minimap render function to capture the local town view.

Function: `NETMAN_SendAvatarToHost()` in `src/network/netman.c`.

#### 0x3FA — PlayerReady (4 bytes)

```
+0x00: uint16  type = 0x3FA
+0x02: uint16  version
```

Minimal 4-byte packet. Sent when a client's slot assignment changes, broadcast to all players.

Defined as `WirePlayerReady` in `src/network/netman.h`.
Function: `NETMAN_SendPlayerReady()` in `src/network/netman.c`.

#### 0x3FC — AttachmentFile (variable, 3-phase)

```
Header (0x10 bytes):
  +0x00: uint16  type = 0x3FC
  +0x02: uint16  version
  +0x04: int32   data_length
  +0x08: uint16  sub_type (attachment type ID)
  +0x0A: uint16  sequence_num
  +0x0C: uint8   phase (0=FIRST, 1=INTERIM, 2=FINAL)
  ...
  +0x0D: uint8[] file data

Max payload per chunk: 0x7FDC bytes (FIRST/INTERIM), 0x400 bytes (FINAL)
```

Three-phase file transfer:

1. **FIRST** (phase=0): Opens a new `.att` file in the PostBag `Att_In` directory
2. **INTERIM** (phase=1): Appends to the open file, sequence-checked
3. **FINAL** (phase=2): Closes the attachment file, reads a companion route file from `Att_Out`, and sends it

Handled by `TrainSubsystem::HandleAttachmentFileData` (`0x43A140`) and sent by `TrainSubsystem::UploadPendingAttachments` (`0x439DF0`) in `Train_network.cpp`.

## Session Management

### Player Roles

Defined in `src/network/netman.h`:

| Constant      | Value | Description                        |
|---------------|-------|------------------------------------|
| ROLE_HOST     | 0     | Session host (created the session) |
| ROLE_JOINING  | 1     | Transitional joining state          |
| ROLE_CLIENT   | 2     | Connected client                    |
| ROLE_OFFLINE  | 3     | Not connected (initial state)       |

Stored at `NETMAN+0x7FC`.

### Session Lifecycle

```
┌──────────┐   CreateDirectPlay()    ┌────────────┐
│ OFFLINE  │────────────────────────→│ INITIALIZED│
│ (role=3) │                         └─────┬──────┘
└──────────┘                               │
                                    ConnectSession()
                                           │
                          ┌────────────────┴────────────────┐
                          ↓                                 ↓
                   OpenSession_Host()              OpenSession_Join()
                   (DPSESSION_CREATE)              (EnumSessions + JOIN)
                          │                                 │
                          ↓                                 ↓
                   ┌──────────┐                     ┌──────────┐
                   │  HOST    │                     │ CLIENT   │
                   │ (role=0) │                     │ (role=2) │
                   └──────────┘                     └──────────┘
```

Functions: `CreateDirectPlay()` (`0x45f390`), `ConnectSession()` (`0x45e730`), `OpenSession_Host()` (`0x45fd80`), `OpenSession_Join()` (`0x460360`), `CloseSession()` (`0x45fc30`) — all in `src/network/netman.c`.

### Application GUID

```
{F9CD2546-577F-11D2-9426-00A0244BDA7A}
```

Stored in `DPSESSIONDESC2.guidApplication`. Defined in `src/network/netman.h`.

### DirectPlay 4 Vtable Offsets

Defined in `src/network/netman.h`:

| Offset | Method          | Use                    |
|--------|-----------------|------------------------|
| 0x08   | Release         | IUnknown::Release      |
| 0x10   | Close           | Session teardown       |
| 0x18   | CreatePlayer    | Local player identity  |
| 0x34   | EnumSessions    | Session discovery      |
| 0x38   | Connect         | —                      |
| 0x48   | GetCaps         | Capability query        |
| 0x60   | Open            | Host or join session   |
| 0x64   | Receive         | Poll for messages      |
| 0x68   | Send            | Send to player/group   |
| 0xC4   | SendEx          | Grouped send           |

## Internal Message Queue

### NetMsgNode (28 bytes)

Messages are queued via `EnqueueNetMsg` (`0x4393d0`) into a global send queue. Queue nodes:

```
Offset  Size  Field
+0x00   4     type          — internal message type
+0x04   4     buf_size      — payload size in bytes
+0x08   4     buf           — payload heap pointer
+0x0C   4     dest_dpid     — destination DPID; 0 = DPID_ALLPLAYERS
+0x10   4     flags         — send flags
+0x14   4     _pad
+0x18   4     next          — intrusive list pointer
```

Defined as `NetMsgNode` in `src/network/netman.h`.

### TrainSubsystem QueueMessage (`0x4393D0`)

The Train subsystem has its own message queue with throttling. When queue depth ≥ 6 and the message is a type-6 (network data) with flags=0, the message is dropped. This prevents flooding the send queue.

Function: `TrainSubsystem::QueueMessage()` in `Train_network.cpp`.

## Train Transfer Protocol

### Outbound Trains

When a train needs to leave one player's town and enter another's, it goes through an outbound train protocol:

1. **Registration**: `NETMAN_AddOutboundTrain` (`0x440610`) inserts an `OutboundTrain` entry (0x14 bytes) into the destination slot's `outbound_train_list` (at `slot+0x38`)

2. **Periodic Broadcast**: `NETMAN_SendOutboundTrains` (`0x43ded0`) fires every `outbound_interval` ticks (default 0x0F). Only clients (role==2) send. Packs all outbound trains into a type `0x3F6` packet with 8-byte entries `{DPID, screen_x, screen_y}`.

3. **Acceptance**: Client calls `NETMAN_SendTrainAccepted` (`0x404410`) with a type `0x3F7` packet containing `{train_handle, dest_slot, src_index}`.

Defined in `src/network/netman.h` and `src/network/netman.c`.

### Inbound Trains

Inbound trains arrive as linked-list nodes (`InboundTrain`) at `NETMAN+0x7E0`:

```
InboundTrain node (variable size):
  +0x00: vtable          — destructor: (*vtable[1])(node) frees
  +0x0C: uint16 num_cars — number of wagon handles
  +0x14: int[]  car_handles[num_cars] — game-engine object IDs
  --- fixed tail (byte offsets from base) ---
  +0x70: next*           — linked list next pointer
  +0x74: uint16 tunnel_angle — 0=N, 0x5A=E, 0xB4=S, 0x10E=W
  +0x78: uint8  src_player_slot — source slot index (0–8)
  +0x7A: uint16 train_uid — unique transfer ID
  +0x7C: uint8  direction  — 0=entering, 1=leaving
  +0x88: uint8  process_delay — countdown; processed when reaches 0
  +0x89: uint8  ack_countdown — decremented by msg type 0x18
```

Defined as `InboundTrain` with accessor macros `ITRAIN_*` in `src/network/netman.h`.

### Train Processing Flow

1. Host sends type `0x0F` (QueueInboundTrain) → `NETMAN_QueueInboundTrain` appends to client's `inbound_train_list`
2. Each tick, `NETMAN_ProcessInboundTrains` (`0x43e010`) fires when `tick_counter % inbound_process_interval == 0`
3. Pops head from list, checks destination via `NETMAN_CheckTrainDestination` (`0x43e1d0`)
4. On success, iterates `car_handles[]` and places cars into world via `FUN_00445400` and `FUN_00447a70`
5. Client acknowledges with type `0x18` (TrainTransferAck), which decrements `ack_countdown` at `InboundTrain+0x89`
6. Host can cancel with type `0x1A` (TrainTransferCancel), handled by `NETMAN_CancelInboundTrain` (`0x43fb50`)

### Tunnel Angles

| Constant           | Value  | Direction |
|--------------------|--------|-----------|
| TUNNEL_ANGLE_NORTH | 0x000  | North (0°)|
| TUNNEL_ANGLE_EAST  | 0x05A  | East (90°)|
| TUNNEL_ANGLE_SOUTH | 0x0B4  | South (180°)|
| TUNNEL_ANGLE_WEST  | 0x10E  | West (270°)|

Defined in `src/network/netman.h`.

## Player Slots

Nine player slots at `NETMAN+0x518`, each 0x4C bytes:

```
PlayerSlot (0x4C bytes):
  +0x00: int32   dpid              — DirectPlay player ID; 0 = empty
  +0x04: uint8   active            — 1 = slot in use
  +0x05: char[13] player_name      — NUL-terminated display name
  +0x12: char[14] layout_name      — town layout name string
  +0x20: uint8   _pad[0x12]
  +0x32: uint16  color_type        — color/type field
  +0x34: uint8   _pad[0x04]
  +0x38: OutboundTrain* outbound_train_list — singly-linked outbound trains
  +0x3C: uint32  avatar_size       — avatar data byte count
  +0x40: uint16  avatar_width
  +0x42: uint16  avatar_height
  +0x44: void*   avatar_data       — heap buffer, freed on slot clear
  +0x48: uint32  avatar_version    — incremented on each SendAvatarToHost
```

Defined as `PlayerSlot` in `src/network/netman.h`.

Slot management functions:
- `NETMAN_InitSlots` (`0x43d130`) — reset all slots
- `NETMAN_UpdatePlayerSlot` (`0x43fe30`) — assign DPID, name, color to a slot
- `NETMAN_RemovePlayer` (`0x43f940`) — clear slot, free trains and avatar
- `NETMAN_LoadSlotAvatar` (`0x43d6c0`) — load minimap bitmap from layout file
- `NETMAN_LoadLayoutFile` (`0x43d820`) — parse `.lay` grid layout config

## File-Based Messaging (PostBag)

In addition to DirectPlay, Lego Loco uses a file-based message system via PostBag directories for train routing:

```
<InstallPath>\PostBag\
  ├── Sort_In\     — incoming sort messages
  ├── Sort_Out\    — outgoing sort messages (counted for postcard availability)
  ├── Att_In\      — incoming attachment files
  └── Att_Out\     — outgoing attachment files
```

`DPLAY_SendMessages` (`0x443470`) and `DPLAY_ReceiveMessage` (`0x443550`) delete all files from these four directories to process/clean up messages. The PostBag is primarily used for postcard exchanges between players.

Functions in `src/decompiled_cpp/network/DPLAY_Message.c`.

### PostBag File Reading

`NETMAN_ReceiveSignalChange` (`0x43E900`) reads route and address files from PostBag to resolve a player name to a full DPlayData structure with routing information. It:
1. Enumerates DPLAY players, matches name
2. Reads route file, picks random entry
3. Reads address file, picks random address
4. Resolves via `NET_ResolveAddress`

Function: `NETMAN_ReceiveSignalChange()` in `src/decompiled_cpp/network/Netman_ReceiveSignalChange.cpp`.

## DPlayManager / Player Slot Serialization

### In-Memory Format (DPlayManager, 0x39C bytes)

The `DPlayManager` class (vtable `0x478264`) is the in-memory player slot — it IS the slot structure. Key fields:

```
+0x00: vtable           — VTBL_DPLAY_PLAYER_SLOT (0x478264)
+0x04: uint16 magic     — always 0x66 (file format marker)
+0x08: int32  colorId   — from PlayerConfig+0x18
+0x0C: int32  configId  — parsed from PlayerConfig string
+0x10: uint8[21] sessionBlk1
+0x25: uint8[20] sessionBlk2
+0x39: uint8  flag39
+0x3A: uint16 wordValue
+0x3C: int32  dwordValue
+0x40: uint8  flag40–42
+0x43: char[80] playerName
+0x93: uint8  unknown93
+0x94: uint8  playerType (0=none, 1=typeA, 2=typeB)
+0x95: uint8  playerTrack (0=off, 1=track1, 2=track2)
+0x96: uint8[768] trackEntries — 128 × 6 bytes each
```

Defined in `src/decompiled_cpp/network/DPlayManager.h`.

### Session Snapshot (DPLAY_SessionData, 0x390 bytes)

For network transmission, the player slot is converted to a compact session snapshot (vtable `0x478268`):

```
+0x00: vtable           — VTBL_DPLAY_SESSION_DATA (0x478268)
+0x04: uint8[4] _pad
+0x08: uint8[21] dataBlk1
+0x1D: uint8[20] dataBlk2
+0x32: uint16 wordValue
+0x34: int32  dwordValue
+0x38: uint8  flag38–3A
+0x3B: char[80] playerName
+0x8B: uint8  unknown8B
+0x8C: uint8  sessionType
+0x8D: uint8  sessionTrack
+0x8E: uint16 entryCount (always 0x80 = 128)
+0x90: uint8[768] trackEntries
```

Conversion functions:
- `DestroySessionInternal` (`0x442EC0`) — player slot → session snapshot
- `DPlayManager::DestroyPlayer` (`0x4428E0`) — session snapshot → player slot
- `DPlayManager::CopyPlayerData` (`0x4426D0`) — compact packet → player slot
- `DPlayManager::FreePlayerSlot` (`0x4427D0`) — player slot → compact packet

All in `src/decompiled_cpp/network/DPlayManager.cpp`.

### Track Entry Format (6 bytes)

Each entry describes a track segment in the player's town:

```
Track entry (6 bytes):
  byte 0: packed_flags = (typeLow - 1) | (packedHigh << 3)
  byte 1: signal_type
  byte 2: x_pos / 2
  byte 3: flag3 (width)
  byte 4: y_pos / 2
  byte 5: flag5 (height)
```

Track entries are managed by:
- `DPlayManager::InitPlayer` (`0x442C90`) — add entry
- `DPlayManager::SetSessionName` (`0x442E00`) — compact array
- `DPlayManager::GetSessionName` (`0x442D30`) — hit-test and clear

### Segment Ring Buffer (Train-level)

Separate from player track entries, each train object has a 128-entry ring buffer at `+0x97` for recording which track cells it occupies:

```
TrainSegment (6 bytes):
  byte 0: type_and_track  — segment_type | (track << 3)
  byte 1: x_half          — x / 2
  byte 2: w_half          — width / 2
  byte 3: y_half          — y / 2
  byte 4: h_half          — height / 2
  byte 5: _unused
```

Functions: `AddTrainSegment` (`0x442c90`), `CompactTrainSegmentList` (`0x442e00`), `SerializeSegmentManager` (`0x442ec0`) — all in `src/network/netman.c`.

## Session State Sync

### State Update (type 0x09)

`NETMAN_ProcessStateUpdate` (`0x43fc50`) handles the host's full session state broadcast. Iterates all 9 slots, applying the host-provided DPID for each:

- **Slot gained** (new DPID where old was 0): calls player-ready notify
- **Slot lost** (DPID gone to 0): reloads avatar from layout file
- **Local player detected**: sets `self_slot_ptr` and `local_player_index`

Function: `NETMAN_ProcessStateUpdate()` in `src/network/netman.c`.

### Session Reset (type 0x0C)

`NETMAN_OnSessionReset` (`0x43f880`) clears the local player's slot DPID and active flag:
- **Host** (role==0): triggers return-to-lobby (`FUN_0040a260`)
- **Client** (role==2): rebinds session (`FUN_00440070`)

Function: `NETMAN_OnSessionReset()` in `src/network/netman.c`.

## Grid Layout Configuration

Multiplayer sessions are arranged in a grid (max 3×3). The layout is loaded from `<DataDir>\Layouts\<name>.lay`:

```
Layout file format:
  <player_count> <grid_cols> <grid_rows>
  <layout_name_1>
  <layout_name_2>
  ...
```

Valid grid combinations: 2×1, 2×2, 3×1, 3×2, 3×3. 1-column grids are not valid.

Function: `NETMAN_LoadLayoutFile()` in `src/network/netman.c`.

## Key Data Structures Summary

| Structure           | Size     | Location            | Purpose                                 |
|---------------------|----------|---------------------|-----------------------------------------|
| NETMAN              | 0x804    | `src/network/netman.h` | Main network manager singleton         |
| PlayerSlot          | 0x4C     | `src/network/netman.h` | Per-player state (9 slots)             |
| InboundTrain        | variable | `src/network/netman.h` | Train arriving from remote player      |
| OutboundTrain       | 0x14     | `src/network/netman.h` | Train queued for remote delivery       |
| NetMsgNode          | 0x1C     | `src/network/netman.h` | Internal message queue node            |
| DPRecvNode          | 0x08     | `src/network/netman.h` | DirectPlay receive node                |
| WireHeader          | 4        | `src/network/netman.h` | Generic wire packet header             |
| WireTrainPos        | 12       | `src/network/netman.h` | Train position update (type 0x3F7)     |
| WireTrainState      | 552      | `src/network/netman.h` | Full train state (type 0x3F1)          |
| WirePlayerReady     | 4        | `src/network/netman.h` | Player ready (type 0x3FA)              |
| TrainSegment        | 6        | `src/network/netman.h` | Train segment ring entry               |
| DPSESSIONDESC2      | 0x50     | `src/network/netman.h` | DirectPlay session descriptor          |
| DPlayManager        | 0x39C    | `src/decompiled_cpp/network/DPlayManager.h` | In-memory player slot     |
| DPLAY_SessionData   | 0x390    | `src/decompiled_cpp/network/DPlayManager.h` | Network session snapshot   |
| Netman              | ~0x804   | `src/decompiled_cpp/network/Netman.h` | High-level session manager class      |
| TrainMessage        | 0x1C     | `src/decompiled_cpp/network/Netman.h` | Train dispatch message                |
| PingEntry           | 0x14     | `src/decompiled_cpp/network/Netman.h` | Per-ping tracking node                |
| NetworkMsg          | 0x1C     | `src/decompiled_cpp/game/Train_network.cpp` | Network message struct       |
| PlayerConnectionNode | 0x1C    | `src/decompiled_cpp/game/Train_network.cpp` | Attachment transfer node     |

## Send Queue & Message Dispatch Flow

```
Game Loop tick
  │
  ├─ NETMAN_Update()                          [src/network/netman.c]
  │   ├─ Drain DP receive queue (under lock)
  │   │   └─ NETMAN_DispatchMessage()         — app-layer types (0x03–0x1C)
  │   ├─ NETMAN_ProcessInboundTrains()         — deliver queued trains
  │   └─ NETMAN_SendOutboundTrains()           — periodic client broadcast
  │
  └─ TrainSubsystem::ProcessMessages()        [Train_network.cpp]
      ├─ WIN32_PeekMessageLoop()              — poll DirectPlay
      └─ switch(msg_type):
          ├─ 0x3E9: Pong response
          ├─ 0x3EA–0x3EE: PlayerInfo, Connect, TrackBuild, FileData
          ├─ 0x3F0: GameOver
          ├─ 0x3F1–0x3F3: LobbyInfo, ConnectionSetup, ControllerInit
          ├─ 0x3F4–0x3F5: PlayerJoin, PlayerCount
          ├─ 0x3F6: TrainPosUpdate (outer msg)
          ├─ 0x3F7: HandleTrainPosUpdate
          ├─ 0x3F8: PlayerCountUpdate
          ├─ 0x3F9: Carriage data
          ├─ 0x3FA: SendBuildingData
          ├─ 0x3FB: HandlePlayerJoin
          ├─ 0x3FC: HandleAttachmentFileData
          └─ 0x3FD: HandlePlayerLeave
```

## File Persistence Format (.crd files)

Player slot data is persisted to `.crd` files in the format `<name>_<config>.crd`. Each file is 0x398 bytes, written from `DPlayManager+4` (skipping the vtable pointer). The magic word at `+4` is `0x66`.

Functions: `DPlayManager::SetPlayerData` (`0x442A70`) for writing, `DPlayManager::GetPlayerName` (`0x442B50`) for reading, in `src/decompiled_cpp/network/DPlayManager.cpp`.

## References

### Source Files

| File | Content |
|------|---------|
| `src/network/netman.h` | NETMAN structures, packet types, function declarations |
| `src/network/netman.c` | NETMAN core implementation (all batches) |
| `src/decompiled_cpp/network/Netman.h` | High-level Netman class with method docs |
| `src/decompiled_cpp/network/DPlayManager.h` | Player slot / session snapshot structures |
| `src/decompiled_cpp/network/DPlayManager.cpp` | Player slot serialization implementation |
| `src/decompiled_cpp/network/DPLAY_Message.c` | PostBag file message system |
| `src/decompiled_cpp/network/Netman_ReceiveSignalChange.cpp` | Signal change / PostBag address resolution |
| `src/decompiled_cpp/game/Train_network.cpp` | Train networking methods (full message dispatch) |
| `src/decompiled_cpp/native/NETMAN_NetworkUI.c` | Network UI (session list, connect) |
| `src/decompiled_cpp/native/NETMAN_SessionSettings.c` | Session settings panel |
| `src/decompiled_cpp/native/win32_network.c` | Win32 network transport layer |

### Key Functions

| Function | Address | File |
|----------|---------|------|
| NETMAN_Constructor | 0x43D0A0 | `src/network/netman.c` |
| NETMAN_InitSlots | 0x43D130 | `src/network/netman.c` |
| NETMAN_Update | 0x43F0C0 | `src/network/netman.c` |
| NETMAN_DispatchMessage | 0x43F2B0 | `src/network/netman.c` |
| NETMAN_RemovePlayer | 0x43F940 | `src/network/netman.c` |
| NETMAN_ProcessInboundTrains | 0x43E010 | `src/network/netman.c` |
| NETMAN_CheckTrainDestination | 0x43E1D0 | `src/network/netman.c` |
| NETMAN_QueueInboundTrain | 0x43E2E0 | `src/network/netman.c` |
| NETMAN_DeliverTrainToClient | 0x43E370 | `src/network/netman.c` |
| NETMAN_SendOutboundTrains | 0x43DED0 | `src/network/netman.c` |
| NETMAN_SendAvatarToHost | 0x43D350 | `src/network/netman.c` |
| NETMAN_AddOutboundTrain | 0x440610 | `src/network/netman.c` |
| NETMAN_SendTrainAccepted | 0x440410 | `src/network/netman.c` |
| NETMAN_UpdatePlayerSlot | 0x43FE30 | `src/network/netman.c` |
| NETMAN_ProcessStateUpdate | 0x43FC50 | `src/network/netman.c` |
| NETMAN_OnSessionReset | 0x43F880 | `src/network/netman.c` |
| NETMAN_CancelInboundTrain | 0x43FB50 | `src/network/netman.c` |
| NETMAN_LoadSlotAvatar | 0x43D6C0 | `src/network/netman.c` |
| NETMAN_SendPlayerReady | 0x43D250 | `src/network/netman.c` |
| NETMAN_LoadLayoutFile | 0x43D820 | `src/network/netman.c` |
| CreateDirectPlay | 0x45F390 | `src/network/netman.c` |
| OpenSession_Host | 0x45FD80 | `src/network/netman.c` |
| OpenSession_Join | 0x460360 | `src/network/netman.c` |
| ConnectSession | 0x45E730 | `src/network/netman.c` |
| CloseSession | 0x45FC30 | `src/network/netman.c` |
| DirectPlaySend | 0x460D40 | `src/network/netman.c` |
| DirectPlayReceive | 0x4606D0 | `src/network/netman.c` |
| EnqueueNetMsg | 0x4393D0 | `src/network/netman.c` |
| DispatchReceivedMsgs | 0x4396C0 | `src/network/netman.c` |
| SendTrainState | 0x440070 | `src/network/netman.c` |
| SendTrainPositionUpdate | 0x440410 | `src/network/netman.c` |
| SerializeTrainSlot | 0x4427D0 | `src/network/netman.c` |
| DeserializeTrainSlot | 0x4426D0 | `src/network/netman.c` |
| SendMapSync | 0x43AE20 | `src/network/netman.c` |
| TrainSubsystem::InitNetwork | 0x4391A0 | `Train_network.cpp` |
| TrainSubsystem::QueueMessage | 0x4393D0 | `Train_network.cpp` |
| TrainSubsystem::FlushMessages | 0x4394E0 | `Train_network.cpp` |
| TrainSubsystem::DispatchMessage | 0x439550 | `Train_network.cpp` |
| TrainSubsystem::ProcessMessages | 0x4396C0 | `Train_network.cpp` |
| TrainSubsystem::HandlePlayerJoin | 0x439D00 | `Train_network.cpp` |
| TrainSubsystem::UploadPendingAttachments | 0x439DF0 | `Train_network.cpp` |
| TrainSubsystem::HandleAttachmentFileData | 0x43A140 | `Train_network.cpp` |
| TrainSubsystem::HandleTrainPosUpdate | 0x43A4B0 | `Train_network.cpp` |
| TrainSubsystem::HandlePlayerLeave | 0x43A5C0 | `Train_network.cpp` |
| TrainSubsystem::UpdatePlayerCount | 0x43A6D0 | `Train_network.cpp` |
| TrainSubsystem::ShutdownNetwork | 0x43AA00 | `Train_network.cpp` |
| TrainSubsystem::HandleDisconnect | 0x43AC10 | `Train_network.cpp` |
| TrainSubsystem::HandleFileTransfer | 0x43AD00 | `Train_network.cpp` |
| TrainSubsystem::HandleConnectionSetup | 0x43B240 | `Train_network.cpp` |
| TrainSubsystem::HandleControllerInit | 0x43B6D0 | `Train_network.cpp` |
| TrainSubsystem::ResetMultiplayerState | 0x43B770 | `Train_network.cpp` |
| TrainSubsystem::AddTrainCar | 0x43B8C0 | `Train_network.cpp` |
| NETMAN_ReceiveSignalChange | 0x43E900 | `Netman_ReceiveSignalChange.cpp` |
| DPLAY_SendMessages | 0x443470 | `DPLAY_Message.c` |
| DPLAY_ReceiveMessage | 0x443550 | `DPLAY_Message.c` |
| DPlayManager::CreatePlayer | 0x442850 | `DPlayManager.cpp` |
| DPlayManager::DestroyPlayer | 0x4428E0 | `DPlayManager.cpp` |
| DPlayManager::CopyPlayerData | 0x4426D0 | `DPlayManager.cpp` |
| DPlayManager::FreePlayerSlot | 0x4427D0 | `DPlayManager.cpp` |
| DPlayManager::GetPlayerData | 0x442A10 | `DPlayManager.cpp` |
