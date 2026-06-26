/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: NETMAN (DirectPlay multiplayer, up to 9 players)
 * WIN32: DirectPlay (DPLAYX.DLL) -> LINUX: ENet or SDL_net
 *
 * Original binary: loco.exe
 * NETMAN object: 0x804 bytes (2052 bytes)
 * Constructor: 0x0043d0a0
 */

#ifndef NETMAN_H
#define NETMAN_H

#include <stdint.h>
#include <stddef.h>

/* ---------------------------------------------------------------
   Wire-protocol constant: every application packet carries this
   value at bytes [2..3]. DirectPlayReceive (0x004606d0) rejects
   any packet whose version field != 300 with "Version check failed".
   --------------------------------------------------------------- */
#define LOCO_WIRE_VERSION  300   /* 0x012C */

/* Maximum players in a multiplayer session */
#define NETMAN_MAX_PLAYERS  9

/* Maximum grid dimensions */
#define NETMAN_MAX_GRID_COLS  3
#define NETMAN_MAX_GRID_ROWS  3

/* Tunnel angle constants (tunnel_angle field, InboundTrain+0x74) */
#define TUNNEL_ANGLE_NORTH  0x000   /* 0   */
#define TUNNEL_ANGLE_EAST   0x05A   /* 90  */
#define TUNNEL_ANGLE_SOUTH  0x0B4   /* 180 */
#define TUNNEL_ANGLE_WEST   0x10E   /* 270 */

/* Player roles (stored at NETMAN+role) */
#define ROLE_HOST     0
#define ROLE_JOINING  1
#define ROLE_CLIENT   2
#define ROLE_OFFLINE  3

/* Train state codes used in NETMAN_DeliverTrainToClient */
#define TRAIN_STATE_LEAVING  0x1870
#define TRAIN_STATE_ENTERING 0x1871

/* Outbound train packet fire interval (ticks, stored at NETMAN+0x7F8) */
#define NETMAN_OUTBOUND_INTERVAL  0x0F

/* Initial field_0x800 value set by constructor */
#define NETMAN_FIELD_7FC_INIT  0x960

/* Inbound train pre-fire lead (constructor: tick_counter = interval - 0x20) */
#define NETMAN_INBOUND_LEAD  0x20

/* Segment ring buffer capacity */
#define TRAIN_SEGMENT_RING_SIZE  128

/* ---------------------------------------------------------------
   Application-level packet type codes
   --------------------------------------------------------------- */
#define PKT_PLAYER_JOINED           0x03
#define PKT_PLAYER_LEFT             0x0B
#define PKT_SESSION_RESET           0x0C
#define PKT_QUEUE_INBOUND_TRAIN     0x0F   /* host-only */
#define PKT_DELIVER_TRAIN_TO_CLIENT 0x11
#define PKT_TRAIN_POS_UPDATE_A      0x12
#define PKT_AVATAR_READY            0x13
#define PKT_AVATAR_GONE             0x14
#define PKT_TRAIN_POS_UPDATE_B      0x15
#define PKT_AVATAR_DATA_RECEIVED    0x16
#define PKT_TRAIN_POS_UPDATE_C      0x17
#define PKT_TRAIN_TRANSFER_ACK      0x18
#define PKT_TRAIN_TRANSFER_CANCEL   0x1A
#define PKT_HOST_SYNC               0x1B
#define PKT_FUN_440820              0x1C
#define PKT_PLAYER_DATA             0x04
#define PKT_SESSION_STATE           0x09

/* Wire packet type codes (application-layer) */
#define WIRE_PKT_TRAIN_STATE        0x03F1
#define WIRE_PKT_TRAIN_POS          0x03F7
#define WIRE_PKT_AVATAR_DATA        0x03F9
#define WIRE_PKT_PLAYER_READY       0x03FA
#define WIRE_PKT_OUTBOUND_TRAINS    0x03F6
#define WIRE_PKT_TRAIN_ACCEPTED     0x03F7

/* Packet sizes (bytes) */
#define PKT_SIZE_TRAIN_STATE        552   /* 0x228, packet type 0x3F1 */
#define PKT_SIZE_MAP_SYNC           2844  /* 0xB1C, packet type 0x3F2 */
#define PKT_SIZE_TRAIN_POS          12    /* packet type 0x3F7 */
#define PKT_SIZE_PLAYER_READY       4     /* packet type 0x3FA */
#define PKT_SIZE_TRAIN_ACCEPTED     0x0C  /* packet type 0x3F7 */

/* Wire entry sizes */
#define WIRE_TRAIN_SLOT_SIZE        60    /* serialized train slot (SerializeTrainSlot) */
#define MEM_TRAIN_SLOT_SIZE         76    /* in-memory train slot */

/* ---------------------------------------------------------------
   IDirectPlay4A vtable offsets (confirmed from decompiled calls)
   --------------------------------------------------------------- */
#define IDP4_Release              0x08   /* IUnknown::Release          */
#define IDP4_Close                0x10   /* IDirectPlay::Close         */
#define IDP4_CreatePlayer         0x18   /* IDirectPlay::CreatePlayer  */
#define IDP4_EnumSessions         0x34   /* IDirectPlay2::EnumSessions */
#define IDP4_Connect              0x38   /* IDirectPlay3::Connect      */
#define IDP4_GetCaps              0x48   /* IDirectPlay2::GetCaps      */
#define IDP4_Open                 0x60   /* IDirectPlay4::Open         */
#define IDP4_Receive              0x64   /* IDirectPlay4::Receive (100 decimal) */
#define IDP4_Send                 0x68   /* IDirectPlay4::Send         */
#define IDP4_SendEx               0xC4   /* IDirectPlay4::SendEx (grouped sends) */

/* DirectPlay open flags */
#define DPSESSION_CREATE          0x82
#define DPSESSION_JOIN            0x81

/* DPERR codes (partial; see DPErrToString 0x0045ff30 for full list) */
#define DPERR_CONNECTING          (-0x7788FEA2)

/* DPID constants */
#define DPID_ALLPLAYERS           0

/* ---------------------------------------------------------------
   DPSESSION descriptor (built at NETMAN+0x158C, size 0x50)
   Application GUID: {0xf9cd2546, 0x11d2, 0x577f, a0-00-26-94-7a-da-4b-24}
   --------------------------------------------------------------- */
typedef struct DPSESSIONDESC2 {
    uint32_t dwSize;              /* 0x50 */
    uint32_t dwFlags;
    uint8_t  guidInstance[16];   /* +0x08 */
    uint8_t  guidApplication[16];/* +0x18  {F9CD2546-11D2-577F-A000-26947ADA4B24} */
    uint32_t dwMaxPlayers;        /* from NETMAN+0x920 */
    uint32_t dwCurrentPlayers;
    uint8_t  _pad[0x20];
} DPSESSIONDESC2;

/* ---------------------------------------------------------------
   Inbound train linked-list node (heap-allocated, owns vtable destructor)
   Total size is variable (car_handles[] is variable length).
   Fixed tail fields are at the offsets shown via accessor macros.
   --------------------------------------------------------------- */
typedef struct InboundTrain {
    void           **vtable;           /* +0x000  destructor: (*vtable[1])(node) frees node */
    uint8_t          _pad_04[0x08];   /* +0x004  unidentified */
    uint16_t         num_cars;        /* +0x00C  number of wagon handles that follow */
    uint8_t          _pad_0e[0x06];  /* +0x00E */
    int              car_handles[1];  /* +0x014  [num_cars] game-engine object IDs (variable) */
    /* --- fixed tail (byte offsets from node base) ---
     * struct InboundTrain *next;  at +0x070
     * uint16_t  tunnel_angle;     at +0x074  0=N 0x5A=E 0xB4=S 0x10E=W
     * uint8_t   src_player_slot;  at +0x078  source slot index (0-8)
     * uint8_t   _pad_79;          at +0x079
     * uint16_t  train_uid;        at +0x07A  unique transfer ID
     * uint8_t   direction;        at +0x07C
     * uint8_t   _pad_7d[0x0B];   at +0x07D
     * uint8_t   process_delay;    at +0x088  countdown; processed when reaches 0
     * uint8_t   ack_countdown;    at +0x089  decremented by msg-type 0x18
     */
} InboundTrain;

/* Accessor macros for InboundTrain tail fields (byte-offset from node base) */
#define ITRAIN_NEXT(t)           (*(struct InboundTrain **)((uint8_t *)(t) + 0x70))
#define ITRAIN_TUNNEL_ANGLE(t)   (*(uint16_t *)          ((uint8_t *)(t) + 0x74))
#define ITRAIN_SRC_SLOT(t)       (*(uint8_t *)           ((uint8_t *)(t) + 0x78))
#define ITRAIN_TRAIN_UID(t)      (*(uint16_t *)          ((uint8_t *)(t) + 0x7A))
#define ITRAIN_DIRECTION(t)      (*(uint8_t *)           ((uint8_t *)(t) + 0x7C))
#define ITRAIN_PROCESS_DELAY(t)  (*(uint8_t *)           ((uint8_t *)(t) + 0x88))
#define ITRAIN_ACK_COUNTDOWN(t)  (*(uint8_t *)           ((uint8_t *)(t) + 0x89))

/* ---------------------------------------------------------------
   Outbound train queue entry (0x14 bytes, heap-allocated)
   --------------------------------------------------------------- */
typedef struct OutboundTrain {
    int                  train_handle;  /* +0x00  game-engine object handle */
    int                  screen_x;     /* +0x04 */
    int                  screen_y;     /* +0x08 */
    uint8_t              dest_slot;   /* +0x0C  destination player slot index */
    uint8_t              src_slot;    /* +0x0D  source player slot index */
    uint8_t              _pad_0e[2];
    struct OutboundTrain *next;        /* +0x10 */
} OutboundTrain;

/* ---------------------------------------------------------------
   Per-player slot (0x4C bytes each, inline array of 9 at NETMAN+0x518)
   Stride: 0x4C bytes between entries.
   --------------------------------------------------------------- */
typedef struct PlayerSlot {
    int              dpid;              /* +0x00  DirectPlay player ID; 0 = empty */
    uint8_t          active;           /* +0x04  1 = slot in use */
    char             player_name[13];  /* +0x05  NUL-terminated display name */
    /* +0x12: layout_name overlaps with _pad in the struct body.
     * The field at slot+0x12 is used as a layout/town name string
     * (14 chars) when loading minimap avatars. */
    char             layout_name[14];  /* +0x12  town layout name string (load path) */
    uint8_t          _pad_20[0x12];   /* +0x20  unidentified */
    uint16_t         color_type;      /* +0x32  color/type field (param_4 from UpdatePlayerSlot) */
    uint8_t          _pad_34[0x04];  /* +0x34 */
    OutboundTrain   *outbound_train_list; /* +0x38 singly-linked list of outbound trains */
    uint32_t         avatar_size;     /* +0x3C  avatar data byte count */
    uint16_t         avatar_width;   /* +0x40 */
    uint16_t         avatar_height;  /* +0x42 */
    void            *avatar_data;    /* +0x44  heap buffer, freed on slot clear */
    uint32_t         avatar_version; /* +0x48  incremented on each SendAvatarToHost */
} PlayerSlot;

/* ---------------------------------------------------------------
   Train segment ring entry (6 bytes)
   Used by AddTrainSegment (0x00442c90) / CompactTrainSegmentList (0x00442e00)
   --------------------------------------------------------------- */
typedef struct TrainSegment {
    uint8_t  type_and_track;  /* segment_type | (track << 3) */
    uint8_t  x_half;         /* x / 2 */
    uint8_t  w_half;         /* width / 2 */
    uint8_t  y_half;         /* y / 2 */
    uint8_t  h_half;         /* height / 2 */
    uint8_t  _unused;
} TrainSegment;

/* ---------------------------------------------------------------
   NETMAN object layout — 0x804 bytes (2052 bytes)
   vtable: PTR_FUN_004781c8 (set by constructor at 0x0043d0a0)

   Key field offsets confirmed from decompiled code:
     +0x000  vtable
     +0x518  slots[9]           (9 x 0x4C = 0x2A4 bytes)
     +0x7BC  _pad
     +0x7CC  self_slot_ptr
     +0x7D0  local_player_index (-1 when none)
     +0x7D4  host_dpid
     +0x7D8  self_dpid
     +0x7E0  inbound_train_list
     +0x7EC  tick_counter
     +0x7F0  inbound_process_interval
     +0x7F8  outbound_interval  (0x0F default)
     +0x7FC  role               (0=host 1=joining 2=client 3=offline)
     +0x800  field_0x800        (0x960)
   --------------------------------------------------------------- */
typedef struct NETMAN {
    void          **vtable;                    /* +0x000 */

    /*
     * +0x004 .. +0x517  — unidentified / protocol buffers
     * Includes DirectPlay session/player fields at higher offsets:
     *   +0x1588  IDirectPlay4A *dp4
     *   +0x158C  DPSESSIONDESC2 session_desc  (size 0x50)
     *   +0x920   char local_name[?]           (DPNAME source)
     *   +0x924   int  local_dpid              (CreatePlayer output)
     *   +0x92c   void *session_enum_buf       (GlobalAlloc/GlobalLock in Join)
     */
    uint8_t          _body[0x514];             /* +0x004  opaque body */

    /* Player slot array — 9 slots x 0x4C bytes = 0x2A4 bytes */
    PlayerSlot       slots[NETMAN_MAX_PLAYERS]; /* +0x518 */

    uint8_t          _pad_7bc[0x10];           /* +0x7BC */

    PlayerSlot      *self_slot_ptr;            /* +0x7CC  pointer into slots[] for local player */
    int              local_player_index;       /* +0x7D0  index into slots[]; -1 = none */
    int              host_dpid;               /* +0x7D4  DirectPlay ID of the host */
    int              self_dpid;              /* +0x7D8  local player's DirectPlay ID */
    InboundTrain    *inbound_train_list;      /* +0x7E0  singly-linked, chained at train+0x70 */
    uint32_t         tick_counter;            /* +0x7EC  incremented each NETMAN_Update call */
    uint32_t         inbound_process_interval;/* +0x7F0  fires when tick_counter % this == 0 */
    uint32_t         outbound_interval;       /* +0x7F8  outbound broadcast period (default 0xF) */
    uint32_t         role;                    /* +0x7FC  0=host 1=joining 2=client 3=offline */
    uint32_t         field_0x800;             /* +0x800  set to 0x960 by constructor */
} NETMAN;

/* ---------------------------------------------------------------
   Send-queue node — 28 bytes (EnqueueNetMsg 0x004393d0)
   --------------------------------------------------------------- */
typedef struct NetMsgNode {
    uint32_t  type;         /* [0]  internal message type */
    uint32_t  buf_size;     /* [1]  payload size in bytes */
    void     *buf;          /* [2]  payload heap pointer */
    int       dest_dpid;    /* [3]  destination DPID; 0 = DPID_ALLPLAYERS */
    uint32_t  flags;        /* [4]  send flags; type-6 nodes dropped when depth>=6 & flags==0 */
    uint8_t   _pad[4];     /* [5]  padding */
    struct NetMsgNode *next; /* [6]  intrusive list pointer */
} NetMsgNode;

/* ---------------------------------------------------------------
   DirectPlay receive output node — 8 bytes (DirectPlayReceive 0x004606d0)
   --------------------------------------------------------------- */
typedef struct DPRecvNode {
    int    dpid;  /* source player DPID */
    void  *buf;   /* packet buffer pointer */
} DPRecvNode;

/* ---------------------------------------------------------------
   Wire packet headers (all multibyte fields are little-endian)
   --------------------------------------------------------------- */

/* Generic wire header (first 4 bytes of every application packet) */
typedef struct WireHeader {
    uint16_t  pkt_type;    /* +0  packet type code (e.g. 0x3F1) */
    uint16_t  version;     /* +2  always LOCO_WIRE_VERSION (300); set by DirectPlaySend */
} WireHeader;

/* Packet 0x3F7 — train position update / train accepted (12 bytes) */
typedef struct WireTrainPos {
    WireHeader  hdr;          /* +0  type=0x3F7, version=300 */
    uint32_t    train_id;     /* +4  game-engine train handle */
    uint8_t     direction;   /* +8  direction byte */
    uint8_t     track_slot;  /* +9  player slot index */
    uint8_t     _pad[2];
} WireTrainPos;

/* Packet 0x3FA — player ready (4 bytes) */
typedef struct WirePlayerReady {
    WireHeader  hdr;  /* type=0x3FA, version=300 */
} WirePlayerReady;

/* Packet 0x3F1 — full train state (552 bytes = header + 9 x 60-byte slots) */
typedef struct WireTrainState {
    WireHeader  hdr;              /* +0   type=0x3F1, version=300 */
    uint32_t    from_player_id;   /* +4   source player DPID */
    uint8_t     flag_byte;        /* +8 */
    uint8_t     extra_byte;       /* +9 */
    uint8_t     _pad[2];
    uint8_t     slots[9][60];     /* +12  nine serialized train slots */
} WireTrainState;

/* ---------------------------------------------------------------
   Function declarations — Batch 1 (core NETMAN, 0x0043d0a0 – 0x00440610)
   --------------------------------------------------------------- */

/* 0x0043d0a0 */
void NETMAN_Constructor(NETMAN *self);

/* 0x0043d130 */
void NETMAN_InitSlots(NETMAN *self, int flag);

/* 0x0043d820 */
void NETMAN_LoadLayoutFile(NETMAN *self, const char *name);

/* 0x0043f0c0 */
void NETMAN_Update(NETMAN *self);

/* 0x0043f2b0 */
void NETMAN_DispatchMessage(NETMAN *self, void *param_1);

/* 0x0043f940 */
void NETMAN_RemovePlayer(NETMAN *self, int dpid);

/* 0x0043e010 */
void NETMAN_ProcessInboundTrains(NETMAN *self);

/* 0x0043e1d0 */
int  NETMAN_CheckTrainDestination(NETMAN *self, InboundTrain *train);

/* 0x0043e2e0 */
void NETMAN_QueueInboundTrain(NETMAN *self, void *msg);

/* 0x0043e370 */
void NETMAN_DeliverTrainToClient(NETMAN *self, void *msg);

/* 0x0043ded0 */
void NETMAN_SendOutboundTrains(NETMAN *self);

/* 0x0043d350 */
void NETMAN_SendAvatarToHost(NETMAN *self);

/* 0x00440610 */
void NETMAN_AddOutboundTrain(NETMAN *self, int train_handle,
                              int screen_x, int screen_y,
                              int dest_slot, int src_slot);

/* 0x00440410 */
void NETMAN_SendTrainAccepted(NETMAN *self, void *msg);

/* 0x0043fe30 */
void NETMAN_UpdatePlayerSlot(NETMAN *self, int dpid, int slot_index,
                              const char *name, int color_type);

/* 0x0043fc50 */
void NETMAN_ProcessStateUpdate(NETMAN *self, void *msg);

/* 0x0043f880 */
void NETMAN_OnSessionReset(NETMAN *self, void *msg);

/* 0x0043fb50 */
void NETMAN_CancelInboundTrain(NETMAN *self, void *msg);

/* 0x0043d6c0 */
void NETMAN_LoadSlotAvatar(NETMAN *self, PlayerSlot *slot);

/* 0x0043d250 */
void NETMAN_SendPlayerReady(NETMAN *self);

/* ---------------------------------------------------------------
   Function declarations — Batch 2 (session management, 0x0045e730 – 0x0043ae20)
   --------------------------------------------------------------- */

/* 0x0045f390 */
int  CreateDirectPlay(NETMAN *self);

/* 0x0045fd80 */
int  OpenSession_Host(NETMAN *self);

/* 0x00460360 */
int  OpenSession_Join(NETMAN *self);

/* 0x0045e730 */
int  ConnectSession(NETMAN *self);

/* 0x0045fc30 */
void CloseSession(NETMAN *self);

/* 0x00460d40 */
int  DirectPlaySend(NETMAN *self, int dest_dpid, void *buf,
                    int size, int flags);

/* 0x004606d0 */
DPRecvNode *DirectPlayReceive(NETMAN *self);

/* 0x004393d0 */
void EnqueueNetMsg(void *queue, uint32_t type, void *buf,
                   uint32_t buf_size, int dest_dpid, uint32_t flags);

/* 0x004396c0 */
void DispatchReceivedMsgs(NETMAN *self);

/* 0x00440070 */
void SendTrainState(NETMAN *self);

/* 0x00440410 */
void SendTrainPositionUpdate(NETMAN *self, int train_id,
                              int direction, int track_slot);

/* 0x004427d0 */
void SerializeTrainSlot(const uint8_t *src_76, uint8_t *dst_60);

/* 0x004426d0 */
void DeserializeTrainSlot(const uint8_t *src_60, uint8_t *dst_76);

/* 0x00442c90 */
int  AddTrainSegment(void *seg_mgr, int segment_type, int track,
                     int x, int w, int y, int h);

/* 0x00442e00 */
void CompactTrainSegmentList(void *seg_mgr);

/* 0x00442ec0 */
void SerializeSegmentManager(void *dst, const void *src);

/* 0x00442850 */
void InitTrainObject(void *train_obj);

/* 0x00442bf0 */
void SetTrainDirection(void *train_obj, int mode, int param_2);

/* 0x00442d30 */
int  HitTestTrainSegment(void *seg_mgr, int x, int y);

/* 0x0045ff30 */
const char *DPErrToString(int dperr);

/* 0x0043ae20 */
void SendMapSync(NETMAN *self, int dest_dpid);

#endif /* NETMAN_H */
