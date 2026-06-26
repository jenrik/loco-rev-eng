/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: NETMAN (DirectPlay multiplayer, up to 9 players)
 * WIN32: DirectPlay (DPLAYX.DLL) -> LINUX: ENet or SDL_net
 *
 * Original binary: loco.exe
 *
 * Decompiled functions covered in this file:
 *
 * Batch 1 — Core NETMAN:
 *   0x0043d0a0  NETMAN_Constructor
 *   0x0043d130  NETMAN_InitSlots
 *   0x0043d820  NETMAN_LoadLayoutFile
 *   0x0043f0c0  NETMAN_Update
 *   0x0043f2b0  NETMAN_DispatchMessage
 *   0x0043f940  NETMAN_RemovePlayer
 *   0x0043e010  NETMAN_ProcessInboundTrains
 *   0x0043e1d0  NETMAN_CheckTrainDestination
 *   0x0043e2e0  NETMAN_QueueInboundTrain
 *   0x0043e370  NETMAN_DeliverTrainToClient
 *   0x0043ded0  NETMAN_SendOutboundTrains
 *   0x0043d350  NETMAN_SendAvatarToHost
 *   0x00440610  NETMAN_AddOutboundTrain
 *   0x00440410  NETMAN_SendTrainAccepted
 *   0x0043fe30  NETMAN_UpdatePlayerSlot
 *   0x0043fc50  NETMAN_ProcessStateUpdate
 *   0x0043f880  NETMAN_OnSessionReset
 *   0x0043fb50  NETMAN_CancelInboundTrain
 *   0x0043d6c0  NETMAN_LoadSlotAvatar
 *   0x0043d250  NETMAN_SendPlayerReady
 *
 * Batch 2 — Session management:
 *   0x0045f390  CreateDirectPlay
 *   0x0045fd80  OpenSession_Host
 *   0x00460360  OpenSession_Join
 *   0x0045e730  ConnectSession
 *   0x0045fc30  CloseSession
 *   0x00460d40  DirectPlaySend
 *   0x004606d0  DirectPlayReceive
 *   0x004393d0  EnqueueNetMsg
 *   0x004396c0  DispatchReceivedMsgs
 *   0x00440070  SendTrainState
 *   0x00440410  SendTrainPositionUpdate
 *   0x004427d0  SerializeTrainSlot
 *   0x004426d0  DeserializeTrainSlot
 *   0x00442c90  AddTrainSegment
 *   0x00442e00  CompactTrainSegmentList
 *   0x00442ec0  SerializeSegmentManager
 *   0x00442850  InitTrainObject
 *   0x00442bf0  SetTrainDirection
 *   0x00442d30  HitTestTrainSegment
 *   0x0045ff30  DPErrToString
 *   0x0043ae20  SendMapSync
 */

#include "netman.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* ---------------------------------------------------------------
   Forward declarations for engine functions referenced by NETMAN.
   These are resolved from other subsystems / the original binary.
   --------------------------------------------------------------- */

/* DirectPlay receive queue (DAT_004fd3a0) and send queue (DAT_004fd39c) */
extern void *g_dp_recv_queue;   /* DAT_004fd3a0 */
extern void *g_dp_send_queue;   /* DAT_004fd39c */

/* Global IDirectPlay4A pointer area (DAT_004fd3a4) */
extern void *g_dp_interface;    /* DAT_004fd3a4 */

/* Local player's NETMAN pointer (DAT_004fd3ac) */
extern NETMAN *g_netman;        /* DAT_004fd3ac */

/* Lock/unlock helpers for the receive queue */
extern void FUN_00449410(void); /* acquire receive-queue lock */
extern void FUN_00449420(void); /* release receive-queue lock */

/* Tunnel capacity queries (indexed by direction) */
extern int  FUN_0041d920(void); /* capacity query: North */
extern int  FUN_0041d950(void); /* capacity query: East  */
extern int  FUN_0041d980(void); /* capacity query: South */
extern int  FUN_0041d8f0(void); /* capacity query: West  */

/* Tunnel reservation */
extern int  FUN_0044df40(void *train);

/* Car placement helpers */
extern void FUN_00445400(void *car, void *train);
extern void FUN_00447a70(void *car, void *train);

/* World object lookup: find car by name in DAT_004aa4a8+6 */
extern void *FUN_lookup_car_by_name(const char *name);

/* Minimap render */
extern void *FUN_00457080(NETMAN *self, int *out_w, int *out_h, int *out_pixel_count);

/* Layout file reader (avatar/minimap bitmap) */
extern int  FUN_00447b20(const char *path);
extern void *FUN_00447ba0(int handle, int *w, int *h, int *sz);

/* UI redraw */
extern void FUN_00426b90(void);

/* Player-ready notify */
extern void FUN_0043d250(NETMAN *self, int slot_index);

/* Session helpers */
extern void FUN_0040a260(void); /* return-to-lobby (host) */
extern void FUN_00440070(void); /* rebind session (client) */
extern void FUN_00440820(NETMAN *self, void *msg);

/* Apply host-provided DPID to slot (ProcessStateUpdate) */
extern void FUN_00442750(NETMAN *self, int slot_index, int dpid);

/* Train state transfer dispatcher (DispatchReceivedMsgs case 0x3F1) */
extern void FUN_004426d0(NETMAN *self, void *buf);

/* Train position update handler (DispatchReceivedMsgs case 0x3F7) */
extern void FUN_0043a4b0(NETMAN *self, void *buf);

/* Accept train in local world (after SendTrainAccepted) */
extern void FUN_004404c0(NETMAN *self, void *msg);

/* Timer query (used by SetTrainDirection) */
extern int  FUN_00466150(void);

/* vtable pointer for train objects */
extern void *PTR_LAB_00478264;

/* vtable pointer for NETMAN objects */
extern void *PTR_FUN_004781c8;

/* Data directory path helper */
extern const char *GetDataDir(void);

/* Global world state source (DAT_004aa4a8) */
extern void *DAT_004aa4a8;


/* ================================================================
   BATCH 1 — CORE NETMAN
   ================================================================ */

/*
 * NETMAN_Constructor — 0x0043d0a0
 *
 * Constructor for the 0x804-byte NETMAN object. Sets vtable to
 * PTR_FUN_004781c8, calls NETMAN_InitSlots with flag=1, then zeroes all
 * live-session fields: role=3 (offline), inbound_train_list=NULL,
 * tick_counter=0, outbound_interval=0xF, field_0x800=0x960.
 */
void NETMAN_Constructor(NETMAN *self)
{
    self->vtable = &PTR_FUN_004781c8;

    NETMAN_InitSlots(self, 1);

    self->role                     = ROLE_OFFLINE;
    self->inbound_train_list       = NULL;
    self->tick_counter             = 0;
    self->outbound_interval        = NETMAN_OUTBOUND_INTERVAL;
    self->field_0x800              = NETMAN_FIELD_7FC_INIT;
}

/*
 * NETMAN_InitSlots — 0x0043d130
 *
 * Resets all nine PlayerSlots and header fields.
 * Sets: max_players=9, local_player_index=-1 (at +0x7D0),
 *       self_slot_ptr=NULL (+0x7CC).
 * Each slot: dpid=0, active=0, outbound_train_list=NULL,
 *            avatar_data=NULL.
 *
 * If flag==0, frees live outbound train entries and avatar buffers
 * before zeroing.
 */
void NETMAN_InitSlots(NETMAN *self, int flag)
{
    int i;

    if (flag == 0) {
        /* Free live outbound train lists and avatar buffers */
        for (i = 0; i < NETMAN_MAX_PLAYERS; i++) {
            PlayerSlot *slot = &self->slots[i];

            /* Walk and free outbound_train_list */
            OutboundTrain *ot = slot->outbound_train_list;
            while (ot) {
                OutboundTrain *next = ot->next;
                free(ot);
                ot = next;
            }

            /* Free avatar buffer */
            if (slot->avatar_data) {
                free(slot->avatar_data);
                slot->avatar_data = NULL;
            }
        }

        /* Free inbound train list via vtable destructor */
        InboundTrain *it = self->inbound_train_list;
        while (it) {
            InboundTrain *next = ITRAIN_NEXT(it);
            /* Call vtable destructor: (*vtable[1])(node) */
            void (*dtor)(InboundTrain *) = (void (*)(InboundTrain *))it->vtable[1];
            dtor(it);
            it = next;
        }
        self->inbound_train_list = NULL;
    }

    /* Zero all nine slots */
    for (i = 0; i < NETMAN_MAX_PLAYERS; i++) {
        memset(&self->slots[i], 0, sizeof(PlayerSlot));
    }

    self->self_slot_ptr        = NULL;
    self->local_player_index   = -1;
}

/*
 * NETMAN_LoadLayoutFile — 0x0043d820
 *
 * Loads the town-grid layout config from '<DataDir>\Layouts\<name>.lay'.
 * Reads player_count (max 9), grid_cols (max 3), grid_rows (max 3) from
 * ASCII header, then copies each town's layout_name string into
 * slot[i]+0x12. Clamps grid to valid combos (2x1, 2x2, 3x1, 3x2, 3x3).
 * For empty slots, calls NETMAN_LoadSlotAvatar.
 */
void NETMAN_LoadLayoutFile(NETMAN *self, const char *name)
{
    char path[256];
    snprintf(path, sizeof(path), "%s\\Layouts\\%s.lay", GetDataDir(), name);

    FILE *f = fopen(path, "r");
    if (!f) return;

    int player_count = 0, grid_cols = 0, grid_rows = 0;
    fscanf(f, "%d %d %d", &player_count, &grid_cols, &grid_rows);

    /* Clamp to maximums */
    if (player_count > NETMAN_MAX_PLAYERS)   player_count = NETMAN_MAX_PLAYERS;
    if (grid_cols    > NETMAN_MAX_GRID_COLS) grid_cols    = NETMAN_MAX_GRID_COLS;
    if (grid_rows    > NETMAN_MAX_GRID_ROWS) grid_rows    = NETMAN_MAX_GRID_ROWS;

    /* Clamp to valid grid combos: 2x1, 2x2, 3x1, 3x2, 3x3
     * (1-column grids are not valid) */
    if (grid_cols < 2) grid_cols = 2;

    /* Read per-slot layout names */
    int i;
    for (i = 0; i < player_count; i++) {
        char layout_name[64];
        if (fscanf(f, "%63s", layout_name) != 1) break;
        strncpy(self->slots[i].layout_name, layout_name,
                sizeof(self->slots[i].layout_name) - 1);
        self->slots[i].layout_name[sizeof(self->slots[i].layout_name) - 1] = '\0';
    }

    fclose(f);

    /* For empty (no-DPID) slots, load the avatar/minimap bitmap */
    for (i = 0; i < NETMAN_MAX_PLAYERS; i++) {
        if (self->slots[i].dpid == 0) {
            NETMAN_LoadSlotAvatar(self, &self->slots[i]);
        }
    }
}

/*
 * NETMAN_Update — 0x0043f0c0
 *
 * Main per-frame tick called from the game loop. Increments tick_counter
 * (+0x7EC). Drains the global DirectPlay receive queue (DAT_004fd3a0)
 * under a lock (FUN_00449410/00449420), dispatching each message via
 * NETMAN_DispatchMessage then freeing it. If inbound_train_list (+0x7E0)
 * is non-NULL, calls NETMAN_ProcessInboundTrains. Always calls
 * NETMAN_SendOutboundTrains.
 */
void NETMAN_Update(NETMAN *self)
{
    self->tick_counter++;

    /* Drain receive queue under lock */
    FUN_00449410();
    {
        DPRecvNode *node;
        while ((node = (DPRecvNode *)g_dp_recv_queue) != NULL) {
            g_dp_recv_queue = NULL; /* simplified; real code walks list */

            NETMAN_DispatchMessage(self, node->buf);
            free(node->buf);
            free(node);
        }
    }
    FUN_00449420();

    if (self->inbound_train_list != NULL) {
        NETMAN_ProcessInboundTrains(self);
    }

    NETMAN_SendOutboundTrains(self);
}

/*
 * NETMAN_DispatchMessage — 0x0043f2b0
 *
 * Central switch on message type (*param_1):
 *   0x03 = PlayerJoined        — copy host DPID and player name to slot[0]
 *   0x0B = PlayerLeft          — calls NETMAN_RemovePlayer
 *   0x0C = SessionReset        — calls NETMAN_OnSessionReset
 *   0x0F = QueueInboundTrain   — host-only; calls NETMAN_QueueInboundTrain
 *   0x11 = DeliverTrainToClient
 *   0x12, 0x15, 0x17 = TrainPositionUpdate (three variants)
 *   0x13, 0x14 = avatar ready/gone flags
 *   0x16 = AvatarDataReceived
 *   0x18 = TrainTransferAck    — decrements ack_countdown at inbound_train+0x89
 *   0x1A = TrainTransferCancelled — calls NETMAN_CancelInboundTrain
 *   0x1B = HostSync            — triggers NETMAN_SendAvatarToHost
 *   0x1C = FUN_00440820
 */
void NETMAN_DispatchMessage(NETMAN *self, void *param_1)
{
    if (!param_1) return;

    uint8_t *msg  = (uint8_t *)param_1;
    uint8_t  type = msg[0];

    switch (type) {
    case PKT_PLAYER_JOINED: /* 0x03 */
        /* Copy host DPID and player name to slot[0] */
        self->slots[0].dpid = *(int *)(msg + 4);
        {
            const char *pname = (const char *)(msg + 8);
            strncpy(self->slots[0].player_name, pname,
                    sizeof(self->slots[0].player_name) - 1);
            self->slots[0].player_name[sizeof(self->slots[0].player_name) - 1] = '\0';
        }
        break;

    case PKT_PLAYER_LEFT: /* 0x0B */
        {
            int dpid = *(int *)(msg + 4);
            NETMAN_RemovePlayer(self, dpid);
        }
        break;

    case PKT_SESSION_RESET: /* 0x0C */
        NETMAN_OnSessionReset(self, param_1);
        break;

    case PKT_QUEUE_INBOUND_TRAIN: /* 0x0F — host-only */
        NETMAN_QueueInboundTrain(self, param_1);
        break;

    case PKT_DELIVER_TRAIN_TO_CLIENT: /* 0x11 */
        NETMAN_DeliverTrainToClient(self, param_1);
        break;

    case PKT_TRAIN_POS_UPDATE_A: /* 0x12 */
    case PKT_TRAIN_POS_UPDATE_B: /* 0x15 */
    case PKT_TRAIN_POS_UPDATE_C: /* 0x17 */
        FUN_0043a4b0(self, param_1);
        break;

    case PKT_AVATAR_READY: /* 0x13 */
        {
            int slot_index = msg[4];
            if (slot_index >= 0 && slot_index < NETMAN_MAX_PLAYERS) {
                self->slots[slot_index].active |= 0x02;
            }
        }
        break;

    case PKT_AVATAR_GONE: /* 0x14 */
        {
            int slot_index = msg[4];
            if (slot_index >= 0 && slot_index < NETMAN_MAX_PLAYERS) {
                self->slots[slot_index].active &= ~0x02;
            }
        }
        break;

    case PKT_AVATAR_DATA_RECEIVED: /* 0x16 */
        /* Avatar pixel data received; handled by session layer */
        break;

    case PKT_TRAIN_TRANSFER_ACK: /* 0x18 */
        /* Decrement ack_countdown on the matching inbound train (+0x89) */
        {
            uint16_t uid = *(uint16_t *)(msg + 4);
            InboundTrain *it = self->inbound_train_list;
            while (it) {
                if (ITRAIN_TRAIN_UID(it) == uid) {
                    uint8_t *countdown = (uint8_t *)it + 0x89;
                    if (*countdown > 0) (*countdown)--;
                    break;
                }
                it = ITRAIN_NEXT(it);
            }
        }
        break;

    case PKT_TRAIN_TRANSFER_CANCEL: /* 0x1A */
        NETMAN_CancelInboundTrain(self, param_1);
        break;

    case PKT_HOST_SYNC: /* 0x1B */
        NETMAN_SendAvatarToHost(self);
        break;

    case PKT_FUN_440820: /* 0x1C */
        FUN_00440820(self, param_1);
        break;

    case PKT_PLAYER_DATA: /* 0x04 */
        {
            int dpid        = *(int   *)(msg + 4);
            int slot_index  = *(int   *)(msg + 8);
            const char *nm  = (const char *)(msg + 12);
            int color_type  = *(int   *)(msg + 12 + 14);
            NETMAN_UpdatePlayerSlot(self, dpid, slot_index, nm, color_type);
        }
        break;

    case PKT_SESSION_STATE: /* 0x09 */
        NETMAN_ProcessStateUpdate(self, param_1);
        break;

    default:
        break;
    }
}

/*
 * NETMAN_RemovePlayer — 0x0043f940
 *
 * Handles player disconnect (param_1=DPID). Scans PlayerSlot[0..8] at
 * this+0x518 (stride 0x4C) for matching DPID. Walks inbound_train_list
 * (+0x7E0, linked at train+0x70) removing all entries whose
 * src_player_slot (train+0x78) matches, calling the train vtable
 * destructor. Then scans each slot's outbound_train_list (+0x38) freeing
 * entries targeting the departed player. Clears slot dpid/active/name
 * and frees avatar buffer at slot+0x44. Triggers UI redraw via
 * FUN_00426b90.
 */
void NETMAN_RemovePlayer(NETMAN *self, int dpid)
{
    int i;
    int removed_slot = -1;

    /* Find the slot with matching DPID */
    for (i = 0; i < NETMAN_MAX_PLAYERS; i++) {
        if (self->slots[i].dpid == dpid) {
            removed_slot = i;
            break;
        }
    }

    if (removed_slot < 0) return;

    /* Remove all inbound trains from that player slot */
    {
        InboundTrain **pnext = &self->inbound_train_list;
        InboundTrain  *it    = self->inbound_train_list;
        while (it) {
            InboundTrain *next = ITRAIN_NEXT(it);
            if (ITRAIN_SRC_SLOT(it) == (uint8_t)removed_slot) {
                *pnext = next;
                void (*dtor)(InboundTrain *) =
                    (void (*)(InboundTrain *))it->vtable[1];
                dtor(it);
            } else {
                pnext = (InboundTrain **)((uint8_t *)it + 0x70);
            }
            it = next;
        }
    }

    /* Remove outbound trains targeting the departed player from all slots */
    for (i = 0; i < NETMAN_MAX_PLAYERS; i++) {
        OutboundTrain **pnext = &self->slots[i].outbound_train_list;
        OutboundTrain  *ot   = self->slots[i].outbound_train_list;
        while (ot) {
            OutboundTrain *next = ot->next;
            if (ot->dest_slot == (uint8_t)removed_slot) {
                *pnext = next;
                free(ot);
            } else {
                pnext = &ot->next;
            }
            ot = next;
        }
    }

    /* Clear the slot */
    {
        PlayerSlot *slot = &self->slots[removed_slot];
        slot->dpid   = 0;
        slot->active = 0;
        memset(slot->player_name, 0, sizeof(slot->player_name));
        if (slot->avatar_data) {
            free(slot->avatar_data);
            slot->avatar_data = NULL;
        }
    }

    FUN_00426b90();
}

/*
 * NETMAN_ProcessInboundTrains — 0x0043e010
 *
 * Called each tick when inbound_train_list is non-empty. Fires every
 * (tick_counter % inbound_process_interval == 0). Pops the head
 * InboundTrain entry (linked at train+0x70). If destination slot has no
 * DPID yet, frees via destructor and returns. Otherwise calls
 * NETMAN_CheckTrainDestination; on failure re-pushes to list head. On
 * success, if the train has cars (train+0x0C > 0), iterates car_handles[]
 * at train+0x14, finds the car in the world by name (DAT_004aa4a8+6),
 * and if it has a free docking track (car+0x3A != 0 and car+0x3C == 0)
 * calls FUN_00445400 and FUN_00447a70 to place it.
 */
void NETMAN_ProcessInboundTrains(NETMAN *self)
{
    if ((self->tick_counter % self->inbound_process_interval) != 0)
        return;

    /* Pop head */
    InboundTrain *train = self->inbound_train_list;
    if (!train) return;

    InboundTrain *next = ITRAIN_NEXT(train);
    self->inbound_train_list = next;

    /* Check that destination slot has a player */
    uint8_t dst_slot = ITRAIN_DIRECTION(train);
    if (dst_slot >= NETMAN_MAX_PLAYERS ||
        self->slots[dst_slot].dpid == 0) {
        void (*dtor)(InboundTrain *) = (void (*)(InboundTrain *))train->vtable[1];
        dtor(train);
        return;
    }

    /* Check tunnel destination availability */
    if (!NETMAN_CheckTrainDestination(self, train)) {
        /* Destination full — re-push to list head */
        ITRAIN_NEXT(train) = self->inbound_train_list;
        self->inbound_train_list = train;
        return;
    }

    /* Place car handles into world */
    int num_cars = (int)train->num_cars;
    int c;
    for (c = 0; c < num_cars; c++) {
        int handle = train->car_handles[c];
        void *car = FUN_lookup_car_by_name((const char *)(intptr_t)handle);
        if (!car) continue;

        /* Check for free docking track: car+0x3A != 0 and car+0x3C == 0 */
        uint8_t *car_bytes = (uint8_t *)car;
        if (*(uint16_t *)(car_bytes + 0x3A) != 0 &&
            *(uint16_t *)(car_bytes + 0x3C) == 0) {
            FUN_00445400(car, train);
            FUN_00447a70(car, train);
        }
    }

    /* Destroy train node after placement */
    {
        void (*dtor)(InboundTrain *) = (void (*)(InboundTrain *))train->vtable[1];
        dtor(train);
    }
}

/*
 * NETMAN_CheckTrainDestination — 0x0043e1d0
 *
 * Checks whether an inbound train's destination tunnel is free. Reads
 * tunnel_angle from train+0x74 (0=N, 0x5A=E, 0xB4=S, 0x10E=W) and calls
 * the matching capacity query (FUN_0041d920/0041d950/0041d980/0041d8f0).
 * Then calls FUN_0044df40 to attempt reservation. Returns 1 if a slot was
 * reserved, 0 if destination is full (caller will re-queue the train).
 */
int NETMAN_CheckTrainDestination(NETMAN *self, InboundTrain *train)
{
    (void)self;

    uint16_t angle = ITRAIN_TUNNEL_ANGLE(train);
    int capacity;

    switch (angle) {
    case TUNNEL_ANGLE_NORTH: capacity = FUN_0041d920(); break;
    case TUNNEL_ANGLE_EAST:  capacity = FUN_0041d950(); break;
    case TUNNEL_ANGLE_SOUTH: capacity = FUN_0041d980(); break;
    case TUNNEL_ANGLE_WEST:  capacity = FUN_0041d8f0(); break;
    default:                 return 0;
    }

    if (capacity <= 0) return 0;

    return FUN_0044df40(train);
}

/*
 * NETMAN_QueueInboundTrain — 0x0043e2e0
 *
 * Handles msg 0xF (host-side).
 *   role==1 (joining): appends the train object from param_1[8] to the
 *     tail of inbound_train_list (+0x7E0), chained via train+0x70, and
 *     primes the tick counter to fire soon (+0x7EC = +0x7F0 - 0x20).
 *   role==2 (client): the train is not for us; frees via vtable destructor.
 */
void NETMAN_QueueInboundTrain(NETMAN *self, void *msg)
{
    InboundTrain *train = *(InboundTrain **)((uint8_t *)msg + 8);
    if (!train) return;

    if (self->role == ROLE_JOINING) {
        /* Append to tail of inbound_train_list */
        if (!self->inbound_train_list) {
            self->inbound_train_list = train;
        } else {
            InboundTrain *tail = self->inbound_train_list;
            while (ITRAIN_NEXT(tail)) {
                tail = ITRAIN_NEXT(tail);
            }
            ITRAIN_NEXT(tail) = train;
        }
        ITRAIN_NEXT(train) = NULL;

        /* Prime tick counter to fire soon */
        self->tick_counter = self->inbound_process_interval - NETMAN_INBOUND_LEAD;

    } else if (self->role == ROLE_CLIENT) {
        /* Not for us — destroy */
        void (*dtor)(InboundTrain *) = (void (*)(InboundTrain *))train->vtable[1];
        dtor(train);
    }
}

/*
 * NETMAN_DeliverTrainToClient — 0x0043e370
 *
 * Handles msg 0x11 (client receives inbound train descriptor from host).
 * Appends train object to inbound_train_list tail. Reads tunnel_angle
 * (train+0x74) to determine exit direction, increments the appropriate
 * capacity counter via FUN_0041d920/50/80/8f0. Calls FUN_00440610 to
 * register in the per-player outbound list. Iterates car_handles
 * (train+0x0C count, train+0x14 base) setting each car to the entering
 * (0x1871) or leaving (0x1870) state.
 */
void NETMAN_DeliverTrainToClient(NETMAN *self, void *msg)
{
    InboundTrain *train = *(InboundTrain **)((uint8_t *)msg + 8);
    if (!train) return;

    /* Append to inbound_train_list tail */
    if (!self->inbound_train_list) {
        self->inbound_train_list = train;
    } else {
        InboundTrain *tail = self->inbound_train_list;
        while (ITRAIN_NEXT(tail)) {
            tail = ITRAIN_NEXT(tail);
        }
        ITRAIN_NEXT(tail) = train;
    }
    ITRAIN_NEXT(train) = NULL;

    /* Increment appropriate capacity counter by direction */
    uint16_t angle = ITRAIN_TUNNEL_ANGLE(train);
    switch (angle) {
    case TUNNEL_ANGLE_NORTH: FUN_0041d920(); break;
    case TUNNEL_ANGLE_EAST:  FUN_0041d950(); break;
    case TUNNEL_ANGLE_SOUTH: FUN_0041d980(); break;
    case TUNNEL_ANGLE_WEST:  FUN_0041d8f0(); break;
    default: break;
    }

    /* Register in per-player outbound list */
    NETMAN_AddOutboundTrain(self,
        *(int    *)((uint8_t *)msg + 0x10),    /* train_handle */
        *(int    *)((uint8_t *)msg + 0x14),    /* screen_x */
        *(int    *)((uint8_t *)msg + 0x18),    /* screen_y */
        *(uint8_t*)((uint8_t *)msg + 0x1C),    /* dest_slot */
        *(uint8_t*)((uint8_t *)msg + 0x1D)     /* src_slot */
    );

    /* Set car states: entering = 0x1871, leaving = 0x1870 */
    int num_cars = (int)train->num_cars;
    uint8_t direction = ITRAIN_DIRECTION(train);
    int c;
    for (c = 0; c < num_cars; c++) {
        int handle = train->car_handles[c];
        void *car = FUN_lookup_car_by_name((const char *)(intptr_t)handle);
        if (!car) continue;
        uint16_t *state_field = (uint16_t *)((uint8_t *)car + 0x30);
        *state_field = (direction == 0) ? TRAIN_STATE_ENTERING : TRAIN_STATE_LEAVING;
    }
}

/*
 * NETMAN_SendOutboundTrains — 0x0043ded0
 *
 * Periodic outbound broadcaster (role==2 / client only). Fires every
 * outbound_interval ticks (0xF, stored at +0x7F8). Reads the local
 * player's outbound_train_list (slot[self_slot_index]+0x38). Builds a
 * packet (type 0x3F6) containing the list of outbound-train descriptors
 * (8 bytes each: DPID, screen X/Y, slot, direction). Sends via
 * FUN_004393d0(DAT_004fd3a4, ...).
 */
void NETMAN_SendOutboundTrains(NETMAN *self)
{
    if (self->role != ROLE_CLIENT) return;
    if ((self->tick_counter % self->outbound_interval) != 0) return;
    if (self->local_player_index < 0) return;

    PlayerSlot *my_slot = &self->slots[self->local_player_index];
    OutboundTrain *ot   = my_slot->outbound_train_list;
    if (!ot) return;

    /* Count entries */
    int count = 0;
    OutboundTrain *cur = ot;
    while (cur) { count++; cur = cur->next; }

    /* Allocate packet: 4-byte header + count * 8 bytes */
    size_t pkt_size = 4 + (size_t)count * 8;
    uint8_t *pkt = (uint8_t *)malloc(pkt_size);
    if (!pkt) return;

    *(uint16_t *)(pkt + 0) = WIRE_PKT_OUTBOUND_TRAINS;
    *(uint16_t *)(pkt + 2) = 0; /* version filled by DirectPlaySend */

    uint8_t *entry = pkt + 4;
    cur = ot;
    while (cur) {
        *(int     *)(entry + 0) = self->slots[cur->dest_slot].dpid;
        *(int16_t *)(entry + 4) = (int16_t)cur->screen_x;
        *(int16_t *)(entry + 6) = (int16_t)cur->screen_y;
        entry += 8;
        cur = cur->next;
    }

    EnqueueNetMsg(g_dp_send_queue, WIRE_PKT_OUTBOUND_TRAINS,
                  pkt, (uint32_t)pkt_size, DPID_ALLPLAYERS, 0);
}

/*
 * NETMAN_SendAvatarToHost — 0x0043d350
 *
 * Handles msg 0x1B (host requests avatar sync). Reads the host player's
 * slot entry via self_slot_ptr (+0x7CC). Calls FUN_00457080 to render
 * local town minimap into a buffer. Allocates packet (type 0x3F9)
 * containing width, height, pixel_count, and raw pixel data. Also caches
 * the pixel buffer into self_slot.avatar_data (+0x44) and increments
 * avatar_version (+0x48). Sends broadcast via FUN_004393d0.
 */
void NETMAN_SendAvatarToHost(NETMAN *self)
{
    if (!self->self_slot_ptr) return;

    int width = 0, height = 0, pixel_count = 0;
    void *pixels = FUN_00457080(self, &width, &height, &pixel_count);
    if (!pixels) return;

    /* Cache into self slot */
    PlayerSlot *slot = self->self_slot_ptr;
    if (slot->avatar_data) free(slot->avatar_data);
    slot->avatar_data    = pixels;
    slot->avatar_width   = (uint16_t)width;
    slot->avatar_height  = (uint16_t)height;
    slot->avatar_version++;

    /* Build and enqueue packet: header(4) + w(4) + h(4) + count(4) + pixels */
    size_t pkt_size = 4 + 4 + 4 + 4 + (size_t)pixel_count;
    uint8_t *pkt = (uint8_t *)malloc(pkt_size);
    if (!pkt) return;

    *(uint16_t *)(pkt + 0)  = WIRE_PKT_AVATAR_DATA;
    *(uint16_t *)(pkt + 2)  = 0; /* version filled by DirectPlaySend */
    *(uint32_t *)(pkt + 4)  = (uint32_t)width;
    *(uint32_t *)(pkt + 8)  = (uint32_t)height;
    *(uint32_t *)(pkt + 12) = (uint32_t)pixel_count;
    memcpy(pkt + 16, pixels, (size_t)pixel_count);

    EnqueueNetMsg(g_dp_send_queue, WIRE_PKT_AVATAR_DATA,
                  pkt, (uint32_t)pkt_size, DPID_ALLPLAYERS, 0);
}

/*
 * NETMAN_AddOutboundTrain — 0x00440610
 *
 * Inserts or updates an OutboundTrain entry in slot[dest_slot].outbound_train_list
 * (+0x38). Each entry is 0x14 bytes: [0]=train_handle, [4]=screen_x,
 * [8]=screen_y, [0xC]=dest_slot_byte, [0xD]=src_slot_byte, [0x10]=next_ptr.
 * If a matching (train_handle, dest) entry already exists, updates its
 * position. If src_slot changed, unlinks from old slot's list and links
 * into new one.
 */
void NETMAN_AddOutboundTrain(NETMAN *self, int train_handle,
                              int screen_x, int screen_y,
                              int dest_slot, int src_slot)
{
    if (dest_slot < 0 || dest_slot >= NETMAN_MAX_PLAYERS) return;
    if (src_slot  < 0 || src_slot  >= NETMAN_MAX_PLAYERS) return;

    PlayerSlot *dst = &self->slots[dest_slot];

    /* Search for existing entry with matching train_handle and dest_slot */
    OutboundTrain *ot = dst->outbound_train_list;
    while (ot) {
        if (ot->train_handle == train_handle &&
            ot->dest_slot    == (uint8_t)dest_slot) {
            /* Update position */
            ot->screen_x = screen_x;
            ot->screen_y = screen_y;

            /* If src_slot changed, relink from old to new slot list */
            if (ot->src_slot != (uint8_t)src_slot) {
                /* Unlink from old src slot's list */
                PlayerSlot *old_src = &self->slots[(int)ot->src_slot];
                OutboundTrain **pp = &old_src->outbound_train_list;
                while (*pp && *pp != ot) pp = &(*pp)->next;
                if (*pp == ot) *pp = ot->next;

                /* Link into new src slot's list */
                ot->src_slot = (uint8_t)src_slot;
                PlayerSlot *new_src = &self->slots[src_slot];
                ot->next = new_src->outbound_train_list;
                new_src->outbound_train_list = ot;
            }
            return;
        }
        ot = ot->next;
    }

    /* Allocate new entry */
    OutboundTrain *entry = (OutboundTrain *)malloc(sizeof(OutboundTrain));
    if (!entry) return;

    entry->train_handle = train_handle;
    entry->screen_x     = screen_x;
    entry->screen_y     = screen_y;
    entry->dest_slot    = (uint8_t)dest_slot;
    entry->src_slot     = (uint8_t)src_slot;
    entry->next         = dst->outbound_train_list;
    dst->outbound_train_list = entry;
}

/*
 * NETMAN_SendTrainAccepted — 0x00440410
 *
 * Client-side (role==2) only. Verifies the accepting player's ID matches
 * local player (DAT_004fd3ac+0x7D0). Allocates a 0x0C-byte packet
 * (type 0x3F7) containing: train_handle, dest_slot_byte,
 * src_player_index. Sends to host via FUN_004393d0 broadcast. Then calls
 * FUN_004404c0 to actually place the accepted train in the local world.
 */
void NETMAN_SendTrainAccepted(NETMAN *self, void *msg)
{
    if (self->role != ROLE_CLIENT) return;

    /* Verify the accepting player is the local player */
    if (g_netman->local_player_index != self->local_player_index) return;

    uint8_t *m = (uint8_t *)msg;
    int train_handle   = *(int    *)(m + 4);
    uint8_t dest_slot  = m[8];
    uint8_t src_idx    = (uint8_t)self->local_player_index;

    /* Build 0x0C-byte packet */
    uint8_t *pkt = (uint8_t *)malloc(PKT_SIZE_TRAIN_ACCEPTED);
    if (!pkt) return;

    *(uint16_t *)(pkt + 0) = WIRE_PKT_TRAIN_ACCEPTED;
    *(uint16_t *)(pkt + 2) = 0; /* version filled by DirectPlaySend */
    *(int      *)(pkt + 4) = train_handle;
    pkt[8]  = dest_slot;
    pkt[9]  = src_idx;
    pkt[10] = 0;
    pkt[11] = 0;

    EnqueueNetMsg(g_dp_send_queue, WIRE_PKT_TRAIN_ACCEPTED,
                  pkt, PKT_SIZE_TRAIN_ACCEPTED, DPID_ALLPLAYERS, 0);

    /* Place accepted train in local world */
    FUN_004404c0(self, msg);
}

/*
 * NETMAN_UpdatePlayerSlot — 0x0043fe30
 *
 * Handles msg 0x4 (player data). Given a remote DPID (param_1), an
 * assigned slot index (param_2), a name string (param_3), and a
 * color/type field (param_4): finds or allocates a slot for the DPID.
 * Copies name to slot+0x05, records DPID at slot+0x00, stores param_4
 * at slot+0x32 (+0x54A). If this DPID is the host (==host_dpid), updates
 * self_slot_ptr (+0x7CC) and self_slot_index (+0x7D0).
 */
void NETMAN_UpdatePlayerSlot(NETMAN *self, int dpid, int slot_index,
                              const char *name, int color_type)
{
    if (slot_index < 0 || slot_index >= NETMAN_MAX_PLAYERS) return;

    PlayerSlot *slot = &self->slots[slot_index];

    slot->dpid = dpid;
    if (name) {
        strncpy(slot->player_name, name, sizeof(slot->player_name) - 1);
        slot->player_name[sizeof(slot->player_name) - 1] = '\0';
    }
    slot->color_type = (uint16_t)color_type;
    slot->active     = 1;

    /* If this DPID is the host (self), record self pointers */
    if (dpid == self->host_dpid) {
        self->self_slot_ptr       = slot;
        self->local_player_index  = slot_index;
    }
}

/*
 * NETMAN_ProcessStateUpdate — 0x0043fc50
 *
 * Handles msg 0x9 (host broadcasts full session state). Iterates all 9
 * slots, calling FUN_00442750 to apply the host-provided DPID for each.
 * Detects host DPID match to set self_slot_ptr/index. Handles slot gains
 * (new DPID: calls FUN_0043d250 player-ready notify), losses (DPID gone
 * to 0: calls NETMAN_LoadSlotAvatar), and host handoff. Redraws UI overlay.
 */
void NETMAN_ProcessStateUpdate(NETMAN *self, void *msg)
{
    uint8_t *m = (uint8_t *)msg;
    /* Slot DPIDs start at msg+4, stride 4 each */
    int i;
    for (i = 0; i < NETMAN_MAX_PLAYERS; i++) {
        int new_dpid = *(int *)(m + 4 + i * 4);
        int old_dpid = self->slots[i].dpid;

        FUN_00442750(self, i, new_dpid);

        if (new_dpid != 0 && old_dpid == 0) {
            /* Slot gained a player */
            FUN_0043d250(self, i);
        } else if (new_dpid == 0 && old_dpid != 0) {
            /* Slot lost a player — reload avatar from layout */
            NETMAN_LoadSlotAvatar(self, &self->slots[i]);
        }

        /* Detect if this slot is the local player */
        if (new_dpid == self->self_dpid) {
            self->self_slot_ptr      = &self->slots[i];
            self->local_player_index = i;
        }
    }

    FUN_00426b90();
}

/*
 * NETMAN_OnSessionReset — 0x0043f880
 *
 * Handles msg 0xC. Scans all 9 slots: if slot.dpid == self_dpid
 * (+0x7D8), clears that slot's DPID and active flag. Zeros self_dpid.
 * If role==0 (host): triggers FUN_0040a260 (return-to-lobby).
 * If role==2 (client): calls FUN_00440070 (rebind session).
 * Redraws UI.
 */
void NETMAN_OnSessionReset(NETMAN *self, void *msg)
{
    (void)msg;
    int i;

    for (i = 0; i < NETMAN_MAX_PLAYERS; i++) {
        if (self->slots[i].dpid == self->self_dpid) {
            self->slots[i].dpid   = 0;
            self->slots[i].active = 0;
        }
    }

    self->self_dpid = 0;

    if (self->role == ROLE_HOST) {
        FUN_0040a260();
    } else if (self->role == ROLE_CLIENT) {
        FUN_00440070();
    }

    FUN_00426b90();
}

/*
 * NETMAN_CancelInboundTrain — 0x0043fb50
 *
 * Handles msg 0x1A (host rejects a transfer). param at msg+8 contains
 * src_player_slot_index. Removes matching InboundTrain from
 * inbound_train_list (matched by train+0x78 == slot_index) and calls
 * destructor. Then re-sends any outbound trains queued for that slot via
 * NETMAN_SendTrainAccepted (FUN_00440410). Redraws UI.
 */
void NETMAN_CancelInboundTrain(NETMAN *self, void *msg)
{
    uint8_t slot_index = ((uint8_t *)msg)[8];

    /* Remove all matching inbound trains */
    InboundTrain **pnext = &self->inbound_train_list;
    InboundTrain  *it    = self->inbound_train_list;
    while (it) {
        InboundTrain *next = ITRAIN_NEXT(it);
        if (ITRAIN_SRC_SLOT(it) == slot_index) {
            *pnext = next;
            void (*dtor)(InboundTrain *) =
                (void (*)(InboundTrain *))it->vtable[1];
            dtor(it);
        } else {
            pnext = (InboundTrain **)((uint8_t *)it + 0x70);
        }
        it = next;
    }

    /* Re-send outbound trains queued for that slot */
    if (slot_index < NETMAN_MAX_PLAYERS) {
        OutboundTrain *ot = self->slots[(int)slot_index].outbound_train_list;
        while (ot) {
            uint8_t synth[12];
            memset(synth, 0, sizeof(synth));
            *(int *)(synth + 4) = ot->train_handle;
            synth[8] = ot->dest_slot;
            NETMAN_SendTrainAccepted(self, synth);
            ot = ot->next;
        }
    }

    FUN_00426b90();
}

/*
 * NETMAN_LoadSlotAvatar — 0x0043d6c0
 *
 * Loads a player slot's avatar/minimap bitmap from
 * '<DataDir>\Layouts\<slot.layout_name>' using FUN_00447b20/447ba0
 * (layout file reader). Populates slot+0x40 (avatar_width),
 * slot+0x42 (avatar_height), slot+0x3C (avatar_size),
 * slot+0x44 (avatar_data ptr) from the loaded image. Called for
 * empty-DPID slots after loading the grid layout.
 */
void NETMAN_LoadSlotAvatar(NETMAN *self, PlayerSlot *slot)
{
    (void)self;

    if (!slot || slot->layout_name[0] == '\0') return;

    char path[256];
    snprintf(path, sizeof(path), "%s\\Layouts\\%s",
             GetDataDir(), slot->layout_name);

    int handle = FUN_00447b20(path);
    if (handle < 0) return;

    int w = 0, h = 0, sz = 0;
    void *data = FUN_00447ba0(handle, &w, &h, &sz);
    if (!data) return;

    if (slot->avatar_data) free(slot->avatar_data);

    slot->avatar_width  = (uint16_t)w;
    slot->avatar_height = (uint16_t)h;
    slot->avatar_size   = (uint32_t)sz;
    slot->avatar_data   = data;
}

/*
 * NETMAN_SendPlayerReady — 0x0043d250
 *
 * Allocates a 4-byte packet (type 0x3FA) and enqueues it for DirectPlay
 * broadcast via FUN_004393d0. This is the player-ready sync notification,
 * sent when a client's slot assignment or DPID changes (called from
 * FUN_0043fc50 on new-player detection).
 */
void NETMAN_SendPlayerReady(NETMAN *self)
{
    (void)self;

    uint8_t *pkt = (uint8_t *)malloc(PKT_SIZE_PLAYER_READY);
    if (!pkt) return;

    *(uint16_t *)(pkt + 0) = WIRE_PKT_PLAYER_READY;
    *(uint16_t *)(pkt + 2) = 0; /* version filled by DirectPlaySend */

    EnqueueNetMsg(g_dp_send_queue, WIRE_PKT_PLAYER_READY,
                  pkt, PKT_SIZE_PLAYER_READY, DPID_ALLPLAYERS, 0);
}


/* ================================================================
   BATCH 2 — SESSION MANAGEMENT
   ================================================================ */

/*
 * CreateDirectPlay — 0x0045f390
 *
 * Constructs the IDirectPlay4A COM object via CoCreateInstance
 * (GUID 0x478f98) and calls InitializeConnection (vtable+0x98).
 * Dispatches on protocol type (1=modem, 2=serial, 3=IPX, 4=TCP/IP)
 * from this+0x518. Stores IDirectPlay4A* at this+0x1588.
 *
 * NOTE: Windows COM / DirectPlay — stub for Linux port.
 * Replace with ENet or SDL_net initialisation for the Linux target.
 */
int CreateDirectPlay(NETMAN *self)
{
    (void)self;
    /* TODO (Linux port): initialise ENet / SDL_net here instead of COM */
    return 0;
}

/*
 * OpenSession_Host — 0x0045fd80
 *
 * Host path: calls IDP4->Open() at vtable+0x60 with DPSESSION_CREATE
 * (0x82) flag. DPSESSION descriptor built at this+0x158c (dwSize=0x50)
 * with application GUID 0xf9cd2546/0x11d2577f/0xa0002694/0x7ada4b24 at
 * offsets 0x15a4-0x15b3 and max-players from this+0x920. Retries on
 * DPERR_CONNECTING (-0x7788fea2) if not async. On failure calls
 * FUN_0045ff30 to convert DPERR code and logs 'Failed to Open new
 * session - <errstr>'. Sets this+0xd50=1 on success.
 *
 * Application GUID: {F9CD2546-11D2-577F-A000-26947ADA4B24}
 *
 * WIN32 outline:
 *   DPSESSIONDESC2 desc = {0};
 *   desc.dwSize = 0x50;
 *   desc.dwFlags = DPSESSION_CREATE;  // 0x82
 *   memcpy(&desc.guidApplication, &LOCO_APP_GUID, 16);
 *   desc.dwMaxPlayers = *(uint32_t *)((uint8_t *)self + 0x920);
 *   IDirectPlay4A *dp4 = *(IDirectPlay4A **)((uint8_t *)self + 0x1588);
 *   int hr;
 *   do { hr = IDP4_Open(dp4, &desc, DPOPEN_CREATE); }
 *   while (hr == DPERR_CONNECTING);
 *   if (FAILED(hr)) {
 *       fprintf(stderr, "Failed to Open new session - %s\n", DPErrToString(hr));
 *       return 0;
 *   }
 *   *(uint32_t *)((uint8_t *)self + 0xD50) = 1;
 *   return 1;
 */
int OpenSession_Host(NETMAN *self)
{
    (void)self;
    /* TODO (Linux port): create ENet server host */
    return 0;
}

/*
 * OpenSession_Join — 0x00460360
 *
 * Joiner path: calls IDP4->EnumSessions() at vtable+0x34 (flags=0x81)
 * with a callback at LAB_00460620 to discover sessions and select one
 * (GlobalAlloc/GlobalLock at this+0x92c). Then calls IDP4->Open() at
 * vtable+0x60 with JOIN (0x81) flag. On failure logs
 * 'Failed to join session Direct Pl...'.
 */
int OpenSession_Join(NETMAN *self)
{
    (void)self;
    /* TODO (Linux port): enumerate ENet / SDL_net sessions */
    return 0;
}

/*
 * ConnectSession — 0x0045e730
 *
 * Top-level session connect. this[0]==0 means joiner (calls
 * OpenSession_Join), else host (calls OpenSession_Host). After session
 * open calls IDP4->CreatePlayer() at vtable+0x18 with DPNAME struct
 * (dwSize=0x10) at this+0x924 storing the local DPID. Password optional
 * (this+0x498). Returns 1 on success.
 */
int ConnectSession(NETMAN *self)
{
    int ok;

    /* this[0]==0 means joiner; non-zero means host */
    if (*(uint32_t *)self == 0) {
        ok = OpenSession_Join(self);
    } else {
        ok = OpenSession_Host(self);
    }

    if (!ok) return 0;

    /*
     * WIN32:
     *   IDirectPlay4A *dp4 = *(IDirectPlay4A **)((uint8_t *)self + 0x1588);
     *   DPNAME name = {0};
     *   name.dwSize = 0x10;
     *   IDP4_CreatePlayer(dp4, &self->local_dpid, &name, NULL, NULL, 0, 0);
     */

    return 1;
}

/*
 * CloseSession — 0x0045fc30
 *
 * Calls IDP4->Close() at vtable+0x10 then IDP4->Release() at vtable+0x08.
 * Zeroes this+0x1588.
 *
 * WIN32:
 *   IDirectPlay4A *dp4 = *(IDirectPlay4A **)((uint8_t *)self + 0x1588);
 *   if (!dp4) return;
 *   IDP4_Close(dp4);
 *   IDP4_Release(dp4);
 *   *(IDirectPlay4A **)((uint8_t *)self + 0x1588) = NULL;
 */
void CloseSession(NETMAN *self)
{
    (void)self;
    /* TODO (Linux port): enet_host_destroy / SDL_net teardown */
}

/*
 * DirectPlaySend — 0x00460d40
 *
 * Thin Send wrapper. Sets *(uint16*)(buf+2) = 300 (wire protocol version
 * tag). Calls IDP4->Send() at vtable+0x68: (this+0x924=idFrom,
 * param_1=idTo, param_4=flags, param_2=buf, size). Alternate path for
 * grouped sends uses vtable+0xC4 (SendEx) with flags|0x600.
 * Returns DPID on success, 0 on error.
 */
int DirectPlaySend(NETMAN *self, int dest_dpid, void *buf,
                   int size, int flags)
{
    if (!buf || size < 4) return 0;

    /* Stamp wire version at bytes [2..3] */
    *(uint16_t *)((uint8_t *)buf + 2) = LOCO_WIRE_VERSION;

    (void)self;
    (void)flags;
    /*
     * WIN32:
     *   IDirectPlay4A *dp4 = *(IDirectPlay4A **)((uint8_t *)self + 0x1588);
     *   int local_dpid = *(int *)((uint8_t *)self + 0x924);
     *   if (flags & 0x200) {
     *       IDP4_SendEx(dp4, local_dpid, dest_dpid, flags|0x600, buf, size,
     *                   0, 0, NULL, NULL);
     *   } else {
     *       IDP4_Send(dp4, local_dpid, dest_dpid, flags, buf, size);
     *   }
     */
    return dest_dpid;
}

/*
 * DirectPlayReceive — 0x004606d0
 *
 * Receive loop. Calls IDP4->Receive() at vtable+0x64 (decimal 100) with
 * DPID_ALLPLAYERS. Validates wire version field at *(int16*)(buf+2)==300;
 * sends DPMSG_SESSION_DESC type 0x1e (30) back as reject if mismatch.
 * Translates system DPID messages (0x31=player lost, 0x03=player add,
 * 0x05=player info, 0x101=session joined, 0x103=player data,
 * 0x104=hostname change) into internal events.
 * Returns an 8-byte {dpid, buf} node for application dispatch.
 */
DPRecvNode *DirectPlayReceive(NETMAN *self)
{
    (void)self;
    /*
     * WIN32:
     *   IDirectPlay4A *dp4 = *(IDirectPlay4A **)((uint8_t *)self + 0x1588);
     *   DPID from_id = DPID_ALLPLAYERS;
     *   void *buf = NULL;
     *   DWORD buf_size = 0;
     *   int hr = IDP4_Receive(dp4, &from_id, &from_id,
     *                         DPRECEIVE_ALL, buf, &buf_size);
     *   if (FAILED(hr)) return NULL;
     *   if (*(int16_t *)((uint8_t *)buf + 2) != LOCO_WIRE_VERSION) {
     *       // send 0x1e reject
     *       HeapFree(GetProcessHeap(), 0, buf);
     *       return NULL;
     *   }
     *   DPRecvNode *node = malloc(sizeof(DPRecvNode));
     *   node->dpid = from_id;
     *   node->buf  = buf;
     *   return node;
     */
    return NULL; /* TODO (Linux port) */
}

/*
 * EnqueueNetMsg — 0x004393d0
 *
 * Enqueues a network message into the global send queue (DAT_004fd39c).
 * Queue nodes are 28-byte structs: [0]=type, [1]=buf_size, [2]=buf_ptr,
 * [3]=dest_dpid, [4]=flags, [6]=next_ptr. Type-6 nodes (train position)
 * are dropped when queue depth>=6 and flags==0 (non-urgent). Frees
 * payload on queue-full drop.
 */
void EnqueueNetMsg(void *queue, uint32_t type, void *buf,
                   uint32_t buf_size, int dest_dpid, uint32_t flags)
{
    (void)queue;

    NetMsgNode *node = (NetMsgNode *)malloc(sizeof(NetMsgNode));
    if (!node) {
        free(buf);
        return;
    }

    node->type      = type;
    node->buf_size  = buf_size;
    node->buf       = buf;
    node->dest_dpid = dest_dpid;
    node->flags     = flags;
    node->next      = NULL;

    /* TODO (Linux port): lock, append to g_dp_send_queue tail, unlock */
    (void)node;
}

/*
 * DispatchReceivedMsgs — 0x004396c0
 *
 * Polls DirectPlayReceive in a loop and routes by packet type field at
 * buf+0. Handles all application-level protocol messages 0x0a-0x3FD.
 * Key routes:
 *   0x3e8 -> send 0x3e9 response
 *   0x3f1 -> train-state transfer (FUN_004426d0 / DeserializeTrainSlot)
 *   0x3f7 -> train position update (FUN_0043a4b0)
 * Packets 0x3f6, 0x3f7, 0x3f9 retain buffer ownership (not freed after
 * dispatch).
 */
void DispatchReceivedMsgs(NETMAN *self)
{
    DPRecvNode *node;

    while ((node = DirectPlayReceive(self)) != NULL) {
        if (!node->buf) { free(node); continue; }

        uint16_t pkt_type = *(uint16_t *)node->buf;
        int retain_buf = 0;

        switch (pkt_type) {
        case 0x03E8:
            /* Ping: send 0x3e9 response */
            {
                uint8_t *resp = (uint8_t *)malloc(4);
                if (resp) {
                    *(uint16_t *)(resp + 0) = 0x03E9;
                    *(uint16_t *)(resp + 2) = 0;
                    EnqueueNetMsg(g_dp_send_queue, 0x03E9, resp, 4,
                                  node->dpid, 0);
                }
            }
            break;

        case WIRE_PKT_TRAIN_STATE: /* 0x3F1 */
            FUN_004426d0(self, node->buf);
            retain_buf = 1;
            break;

        case WIRE_PKT_TRAIN_POS: /* 0x3F7 */
            FUN_0043a4b0(self, node->buf);
            retain_buf = 1;
            break;

        case WIRE_PKT_OUTBOUND_TRAINS: /* 0x3F6 */
        case WIRE_PKT_AVATAR_DATA:     /* 0x3F9 */
            retain_buf = 1;
            break;

        default:
            NETMAN_DispatchMessage(self, node->buf);
            break;
        }

        if (!retain_buf) free(node->buf);
        free(node);
    }
}

/*
 * SendTrainState — 0x00440070
 *
 * Builds and queues packet type 0x3F1 (552 bytes). Header at buf+0:
 * type=0x3F1 (uint16), version=300 (uint16, filled by DirectPlaySend),
 * from_player_id (uint32 at +4), flag_byte (+8), extra_byte (+9).
 * Followed by exactly 9 train-slot entries each serialized to 60 bytes
 * via FUN_004427d0 from the in-memory 76-byte slots at this+0x518.
 * Queue dest=0 (DPID_ALLPLAYERS).
 */
void SendTrainState(NETMAN *self)
{
    uint8_t *pkt = (uint8_t *)malloc(PKT_SIZE_TRAIN_STATE);
    if (!pkt) return;

    memset(pkt, 0, PKT_SIZE_TRAIN_STATE);

    *(uint16_t *)(pkt + 0) = WIRE_PKT_TRAIN_STATE;
    *(uint16_t *)(pkt + 2) = 0; /* version — filled by DirectPlaySend */
    *(uint32_t *)(pkt + 4) = (uint32_t)self->self_dpid;
    pkt[8] = 0; /* flag_byte */
    pkt[9] = 0; /* extra_byte */

    /* Serialize all 9 in-memory train slots (76 bytes each) to wire (60 bytes each) */
    uint8_t *slot_base = (uint8_t *)self + 0x518;
    int i;
    for (i = 0; i < NETMAN_MAX_PLAYERS; i++) {
        SerializeTrainSlot(slot_base + i * MEM_TRAIN_SLOT_SIZE,
                           pkt + 12 + i * WIRE_TRAIN_SLOT_SIZE);
    }

    EnqueueNetMsg(g_dp_send_queue, WIRE_PKT_TRAIN_STATE,
                  pkt, PKT_SIZE_TRAIN_STATE, DPID_ALLPLAYERS, 0);
}

/*
 * SendTrainPositionUpdate — 0x00440410
 *
 * Builds and queues packet type 0x3F7 (12 bytes). Only fires when
 * train track_slot == local player slot (this+0x7D0). Payload:
 * type=0x3F7 (uint16 at +0), version=300 (set by DirectPlaySend at +2),
 * train_id (uint32 at +4), direction_byte (uint8 at +8),
 * track_slot (uint8 at +9). dest=0 (DPID_ALLPLAYERS).
 * Calls FUN_004404c0 regardless to handle local update.
 */
void SendTrainPositionUpdate(NETMAN *self, int train_id,
                              int direction, int track_slot)
{
    /* Only send packet when track_slot matches local player slot */
    if (track_slot == self->local_player_index) {
        WireTrainPos *pkt = (WireTrainPos *)malloc(sizeof(WireTrainPos));
        if (pkt) {
            pkt->hdr.pkt_type = WIRE_PKT_TRAIN_POS;
            pkt->hdr.version  = 0; /* filled by DirectPlaySend */
            pkt->train_id     = (uint32_t)train_id;
            pkt->direction    = (uint8_t)direction;
            pkt->track_slot   = (uint8_t)track_slot;
            pkt->_pad[0]      = 0;
            pkt->_pad[1]      = 0;

            EnqueueNetMsg(g_dp_send_queue, WIRE_PKT_TRAIN_POS,
                          pkt, PKT_SIZE_TRAIN_POS, DPID_ALLPLAYERS, 0);
        }
    }

    /* Always call local update handler regardless of send */
    FUN_004404c0(self, NULL);
}

/*
 * SerializeTrainSlot — 0x004427d0
 *
 * Serializes a 76-byte in-memory train slot to the 60-byte wire format
 * used in packet 0x3F1 entries. Field mapping (src offsets into 76-byte
 * slot, dst offsets into 60-byte wire entry):
 *   wire+0x00 = src[0]          (4 bytes)
 *   wire+0x04 = src[0x32]       (4 bytes)
 *   wire+0x08 = src[8]          (4 bytes)
 *   wire+0x0C = strcpy(src+20)  (NUL-terminated)
 *   wire+0x19 = strcpy(src+0x12)(NUL-terminated)
 *   wire+0x39 = src[0x36]       (1 byte)
 *   wire+0x3A = src[4]          (1 byte)
 * Called 9 times from SendTrainState.
 */
void SerializeTrainSlot(const uint8_t *src_76, uint8_t *dst_60)
{
    memset(dst_60, 0, WIRE_TRAIN_SLOT_SIZE);

    *(uint32_t *)(dst_60 + 0x00) = *(const uint32_t *)(src_76 + 0x00);
    *(uint32_t *)(dst_60 + 0x04) = *(const uint32_t *)(src_76 + 0x32);
    *(uint32_t *)(dst_60 + 0x08) = *(const uint32_t *)(src_76 + 0x08);
    /* player name (src+20, 12 chars max) -> wire+0x0C */
    strncpy((char *)(dst_60 + 0x0C), (const char *)(src_76 + 20), 0x0C);
    /* layout name (src+0x12) -> wire+0x19 */
    strncpy((char *)(dst_60 + 0x19), (const char *)(src_76 + 0x12), 0x1F);
    dst_60[0x39] = src_76[0x36];
    dst_60[0x3A] = src_76[0x04];
}

/*
 * DeserializeTrainSlot — 0x004426d0
 *
 * Inverse of SerializeTrainSlot. Reads 60-byte wire entry and populates
 * a 76-byte in-memory struct. Field mapping:
 *   dest+0x00 = wire[0]          (4 bytes)
 *   dest+0x04 = wire[0x3A]       (1 byte)
 *   dest+0x05 = strcpy(wire+12)  (NUL-terminated player name)
 *   dest+0x12 = strcpy(wire+0x19)(NUL-terminated layout name)
 *   dest+0x32 = wire[4]          (4 bytes)
 *   dest+0x36 = wire[0x39]       (1 byte)
 *   dest+0x48 = wire[8]          (4 bytes)
 * Called 9 times in DispatchReceivedMsgs case 0x3F1.
 */
void DeserializeTrainSlot(const uint8_t *src_60, uint8_t *dst_76)
{
    memset(dst_76, 0, MEM_TRAIN_SLOT_SIZE);

    *(uint32_t *)(dst_76 + 0x00) = *(const uint32_t *)(src_60 + 0x00);
    dst_76[0x04] = src_60[0x3A];
    strncpy((char *)(dst_76 + 0x05), (const char *)(src_60 + 0x0C), 0x0C);
    strncpy((char *)(dst_76 + 0x12), (const char *)(src_60 + 0x19), 0x1F);
    *(uint32_t *)(dst_76 + 0x32) = *(const uint32_t *)(src_60 + 0x04);
    dst_76[0x36] = src_60[0x39];
    *(uint32_t *)(dst_76 + 0x48) = *(const uint32_t *)(src_60 + 0x08);
}

/*
 * AddTrainSegment — 0x00442c90
 *
 * Appends one 6-byte segment record to the 128-slot ring buffer at
 * this+0x97. Each slot: [0]=segment_type|track<<3 (derived from params),
 * [1]=x/2, [2]=width/2, [3]=y/2, [4]=height/2, [5]=unused. When full
 * (128 slots) compacts via FUN_00442e00 and reuses slot 127. Returns the
 * slot index. Used to record which track cells a train occupies.
 */
int AddTrainSegment(void *seg_mgr, int segment_type, int track,
                    int x, int w, int y, int h)
{
    uint8_t  *base  = (uint8_t *)seg_mgr;
    uint16_t *count = (uint16_t *)(base + 0x8E);
    uint8_t  *ring  = base + 0x97;

    int slot;
    if (*count >= TRAIN_SEGMENT_RING_SIZE) {
        CompactTrainSegmentList(seg_mgr);
        slot = TRAIN_SEGMENT_RING_SIZE - 1;
    } else {
        slot = (int)*count;
        (*count)++;
    }

    TrainSegment *entry = (TrainSegment *)(ring + slot * sizeof(TrainSegment));
    entry->type_and_track = (uint8_t)((segment_type & 0x07) | ((track & 0x1F) << 3));
    entry->x_half        = (uint8_t)(x / 2);
    entry->w_half        = (uint8_t)(w / 2);
    entry->y_half        = (uint8_t)(y / 2);
    entry->h_half        = (uint8_t)(h / 2);
    entry->_unused       = 0;

    return slot;
}

/*
 * CompactTrainSegmentList — 0x00442e00
 *
 * Removes the first occupied slot in the 128-slot segment ring (at
 * this+0x97) by sliding all subsequent entries left 6 bytes. Called when
 * the ring is full, keeping the list contiguous from index 0.
 */
void CompactTrainSegmentList(void *seg_mgr)
{
    uint8_t  *base  = (uint8_t *)seg_mgr;
    uint16_t *count = (uint16_t *)(base + 0x8E);
    uint8_t  *ring  = base + 0x97;

    if (*count == 0) return;

    size_t move_bytes = ((size_t)(*count) - 1) * sizeof(TrainSegment);
    memmove(ring, ring + sizeof(TrainSegment), move_bytes);
    (*count)--;
}

/*
 * SerializeSegmentManager — 0x00442ec0
 *
 * Copies all 128 train-segment slots (6 bytes each) from src+0x96 to
 * this+0x90, writing count 0x80 at this+0x8e. Also copies fixed metadata
 * fields (speed, color flags etc.) into this+0x08/0x1d/0x3b/0x8b-0x8d/
 * 0x32-0x3a. Used to package the segment manager for transfer/file save.
 */
void SerializeSegmentManager(void *dst, const void *src)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    /* Write segment count = 0x80 (128) */
    *(uint16_t *)(d + 0x8E) = TRAIN_SEGMENT_RING_SIZE;

    /* Copy ring buffer: src+0x96 -> dst+0x90 (128 * 6 = 768 bytes) */
    memcpy(d + 0x90, s + 0x96, TRAIN_SEGMENT_RING_SIZE * sizeof(TrainSegment));

    /* Copy fixed metadata fields */
    *(uint32_t *)(d + 0x08) = *(const uint32_t *)(s + 0x08); /* speed */
    d[0x1D] = s[0x1D]; /* color flags */
    d[0x3B] = s[0x3B];
    d[0x8B] = s[0x8B];
    d[0x8C] = s[0x8C];
    d[0x8D] = s[0x8D];
    memcpy(d + 0x32, s + 0x32, 9); /* fields 0x32-0x3A */
}

/*
 * InitTrainObject — 0x00442850
 *
 * Zero-initializes a train object and sets its vtable ptr to
 * PTR_LAB_00478264. Sets magic word 0x0066 at param_1+4 (version/type
 * tag), copies track-group ID from DAT_004aa4a8+0x18 into field[2],
 * zeros all 128 segment slots at +0x97.
 */
void InitTrainObject(void *train_obj)
{
    uint8_t *obj = (uint8_t *)train_obj;
    size_t obj_size = 0x97 + TRAIN_SEGMENT_RING_SIZE * sizeof(TrainSegment);
    memset(obj, 0, obj_size);

    /* Set vtable */
    *(void **)obj = &PTR_LAB_00478264;

    /* Magic/version tag at +4 */
    *(uint16_t *)(obj + 4) = 0x0066;

    /* Copy track-group ID from global DAT_004aa4a8+0x18 */
    *(uint32_t *)(obj + 8) = *(const uint32_t *)((uint8_t *)&DAT_004aa4a8 + 0x18);
}

/*
 * SetTrainDirection — 0x00442bf0
 *
 * Sets direction state at this+0x95 and heading at this+0x94.
 *   param_1==0: clears both fields.
 *   param_1==1: sets pending direction; if param_2==-1 picks odd/even
 *               from timer (FUN_00466150).
 *   param_1==2: sets heading 2.
 *   param_1==3: forces heading 1 going left.
 */
void SetTrainDirection(void *train_obj, int mode, int param_2)
{
    uint8_t *obj = (uint8_t *)train_obj;

    switch (mode) {
    case 0:
        obj[0x94] = 0;
        obj[0x95] = 0;
        break;

    case 1:
        obj[0x95] = 1; /* pending direction */
        if (param_2 == -1) {
            int t = FUN_00466150();
            obj[0x94] = (uint8_t)(t & 1);
        } else {
            obj[0x94] = (uint8_t)param_2;
        }
        break;

    case 2:
        obj[0x94] = 2;
        break;

    case 3:
        /* Force heading 1 going left */
        obj[0x94] = 1;
        obj[0x95] = 1;
        break;

    default:
        break;
    }
}

/*
 * HitTestTrainSegment — 0x00442d30
 *
 * Iterates segment slots in reverse (127 down to 0); for each active slot
 * reconstructs a RECT from stored half-coordinate fields and calls
 * PtInRect. On hit clears the segment slot and calls
 * CompactTrainSegmentList, then returns slot index.
 */
int HitTestTrainSegment(void *seg_mgr, int x, int y)
{
    uint8_t  *base  = (uint8_t *)seg_mgr;
    uint16_t *count = (uint16_t *)(base + 0x8E);
    uint8_t  *ring  = base + 0x97;
    int n = (int)*count;
    int i;

    for (i = n - 1; i >= 0; i--) {
        TrainSegment *entry = (TrainSegment *)(ring + i * sizeof(TrainSegment));
        int left   = (int)entry->x_half * 2;
        int top    = (int)entry->y_half * 2;
        int right  = left + (int)entry->w_half * 2;
        int bottom = top  + (int)entry->h_half * 2;

        if (x >= left && x < right && y >= top && y < bottom) {
            memset(entry, 0, sizeof(TrainSegment));
            CompactTrainSegmentList(seg_mgr);
            return i;
        }
    }

    return -1;
}

/*
 * DPErrToString — 0x0045ff30
 *
 * Converts a DPERR_* HRESULT to its string name. Covers all standard
 * DirectPlay 4 error codes. DP_OK (0) returns 'DP_OK'.
 */
const char *DPErrToString(int dperr)
{
    switch (dperr) {
    case 0:             return "DP_OK";
    case -2147220992:   return "DPERR_ALREADYINITIALIZED";
    case -2147220991:   return "DPERR_ACCESSDENIED";
    case -2147220990:   return "DPERR_ACTIVEPLAYERS";
    case -2147220989:   return "DPERR_BUFFERTOOSMALL";
    case -2147220988:   return "DPERR_CANTADDPLAYER";
    case -2147220987:   return "DPERR_CANTCREATEGROUP";
    case -2147220986:   return "DPERR_CANTCREATEPLAYER";
    case -2147220985:   return "DPERR_CANTCREATESESSION";
    case -2147220984:   return "DPERR_CAPSNOTAVAILABLEYET";
    case -2147220983:   return "DPERR_EXCEPTION";
    case -2147220982:   return "DPERR_GENERIC";
    case -2147220978:   return "DPERR_INVALIDFLAGS";
    case -2147220976:   return "DPERR_INVALIDOBJECT";
    case -2147220975:   return "DPERR_INVALIDPARAM";
    case -2147220974:   return "DPERR_INVALIDINTERFACE";
    case -2147220973:   return "DPERR_INVALIDGROUP";
    case -2147220972:   return "DPERR_INVALIDPLAYER";
    case -2147220970:   return "DPERR_INVALIDPASSWORD";
    case -2147220968:   return "DPERR_NOCAPS";
    case -2147220967:   return "DPERR_NOCONNECTION";
    case -2147220964:   return "DPERR_NOMESSAGES";
    case -2147220963:   return "DPERR_NONAMESERVERFOUND";
    case -2147220962:   return "DPERR_NOPLAYERS";
    case -2147220961:   return "DPERR_NOSESSIONS";
    case -2147220957:   return "DPERR_NOTLOGGEDIN";
    case -2147220954:   return "DPERR_SENDTOOBIG";
    case -2147220950:   return "DPERR_SESSIONLOST";
    case -2147220948:   return "DPERR_SIGNATUREERROR";
    case -2147220947:   return "DPERR_TIMEOUT";
    case -2147220944:   return "DPERR_UNAVAILABLE";
    case -2147220941:   return "DPERR_UNINITIALIZED";
    case -2147220939:   return "DPERR_USERCANCEL";
    case -2147220937:   return "DPERR_CANTLOADCAPI";
    case -2147220934:   return "DPERR_CANTLOADSSPI";
    case -2147220932:   return "DPERR_CONNECTING";
    case -2147220930:   return "DPERR_BUFFERTOOLARGE";
    case -2147220929:   return "DPERR_CANTCREATEPROCESS";
    case -2147220928:   return "DPERR_APPNOTSTARTED";
    case -2147220927:   return "DPERR_INVALIDPASSKEY";
    case -2147220926:   return "DPERR_ENCRYPTIONFAILED";
    case -2147220925:   return "DPERR_SIGNFAILED";
    case -2147220924:   return "DPERR_CANTLOADPROVIDER";
    case -2147220923:   return "DPERR_UNKNOWNAPPLICATION";
    case -2147220922:   return "DPERR_NOTLOBBIED";
    case -2147220921:   return "DPERR_SERVICEPROVIDERLOADED";
    case -2147220920:   return "DPERR_ALREADYREGISTERED";
    case -2147220919:   return "DPERR_NOTREGISTERED";
    default:            return "DPERR_UNKNOWN";
    }
}

/*
 * SendMapSync — 0x0043ae20
 *
 * Builds and sends packet type 0x3F2 (2844 bytes = 0xB1C). Contains full
 * track/junction layout: header fields (type, speed, junction count,
 * player reference), then per-junction entries with car references and
 * 0xE7 * 4 = 932-byte car-data blobs. Calls DirectPlaySend to a specific
 * dest player. On send failure puts the train back in the pending queue.
 */
void SendMapSync(NETMAN *self, int dest_dpid)
{
    uint8_t *pkt = (uint8_t *)malloc(PKT_SIZE_MAP_SYNC);
    if (!pkt) return;

    memset(pkt, 0, PKT_SIZE_MAP_SYNC);

    /* Packet header */
    *(uint16_t *)(pkt + 0) = 0x03F2;  /* packet type */
    *(uint16_t *)(pkt + 2) = 0;        /* version filled by DirectPlaySend */

    /*
     * Remaining fields: speed, junction_count, player_reference,
     * per-junction entries (car references + 932-byte blobs).
     * TODO (Linux port): populate from live game world state.
     */

    int ret = DirectPlaySend(self, dest_dpid, pkt, PKT_SIZE_MAP_SYNC, 0);
    if (ret == 0) {
        /* Send failure: free packet (original re-queues the train) */
        free(pkt);
    }
}
