/**
 * Train_MoveToNeighborTown — Transfer a train car to a player in a neighbor town.
 * Address: 0x43AE20
 * Size: 1016 bytes (254 instructions)
 * Calling convention: __thiscall (ECX = TrainSubsystem*, 3 stack args), RET 0xC
 *
 * Transfers a train car (rail vehicle) to a player in a connected/neighbor town
 * over the DirectPlay network. Two execution paths depending on whether the
 * target player is local (self) or remote:
 *
 * 1. Local (player is self):
 *    - Mirrors direction (0<->180, 90<->270) for opposite-facing display
 *    - Appends car_node to the train->car_list_b (+0x18) linked list
 *    - Calls Train_RemoveAllTracks to clear source train's track data
 *
 * 2. Remote (player is another player):
 *    - Allocates a 0xB1C-byte MSG_CONN_SETUP (0x3F2) message buffer
 *    - Serialises car state: direction, type, capacity, resource_id, owner name,
 *      and up to 3 passenger cars with their DPlay data
 *    - Calls WIN32_SendNetworkData with a 0xB1C-byte payload to target player
 *    - On send success: appends to car_list_b, queues NETMAN messages per car
 *    - On send failure: marks car with owner flag, returns to car_list_a
 *
 * Called by: VehicleEditor dispatch functions, train departure logic
 *
 * @param train     ECX = TrainSubsystem* (g_train singleton at 0x4FD3A4)
 * @param player    Target player index (matches g_netman player entries)
 * @param car_node  Train car node (0x1C bytes: header, capacity, next, links)
 * @param direction Current movement direction (0=up, 90=right, 180=down, 270=left)
 * @return          1 on success/initiated, and mirror handling on failure
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* operator_new(size_t size);
extern void  GLOBAL_free(void* ptr);

extern int   g_netman;              /* 0x4FD33C — Network manager */
extern void* g_dplay_peer;           /* 0x4FD1F0 — DirectPlay peer */
extern void* g_train;               /* 0x4FD3A4 — TrainSubsystem singleton */

extern int  NETMAN_FindPlayerIndex(void* netman, int player_id);
extern int  WIN32_SendNetworkData(void* peer, int player_id, void* msg,
                                   int size, int unknown);
extern void* Train_RemoveAllTracks(void* self);
extern int  VehicleEditor_GetResourceId(void* editor);
extern void* VehicleEditor_GetDPlayData(void* editor);
extern int  NETMAN_QueueMessage(void* msg);
extern void CRT_memset_pattern(void* dst, int size, int count, void* pattern);
extern void CRT_free_pattern(void* dst, int size, int count, void* dtor_f);

/**
 * Train_HandleLobbyInfo — pattern template (0x3A8 bytes, used 3 times).
 * Address: 0x43B230
 * Referenced by CRT_memset_pattern in Train_MoveToNeighborTown for
 * zeroing/initializing the 3 passenger-car slots in the network message.
 */
extern byte _TrainMessageTemplate_CarSlot;  /* 0x43B230 */

int __thiscall
Train_MoveToNeighborTown(void* train, int player, void* car_node, int direction);
Train_MoveToNeighborTown(void* train, int player, void* car_node, int direction)
{
    /* NOTE: Full decompiled implementation is in the legacy C decompilation
     * at src/decompiled/FUN_0043ae20.c. This file documents the interface
     * and high-level flow. See legacy decompilation for the 254-instruction
     * implementation involving:
     *
     *   1. Player lookup: NETMAN_FindPlayerIndex to compare target vs local
     *   2. Direction mirroring: 0<->180, 90<->270, plus range checks
     *   3. Local path: car_list_b append + Train_RemoveAllTracks
     *   4. Remote path: 0xB1C message allocation with memset_pattern
     *      (0x3A8 * 3 = car data template carved from 0x43B230)
     *   5. Message serialization: direction, type, capacity, resource,
     *      owner name (+0x7C), and passenger car data (up to 3)
     *   6. WIN32_SendNetworkData with retry/success handling
     *   7. On success: mirror direction, append to car_list_b,
     *      NETMAN_QueueMessage for each car
     *   8. On failure: mark owner index (+0x1F), return to car_list_a (+0x14)
     */

    /* Stub: return 1 (success) */
    (void)train;
    (void)player;
    (void)car_node;
    (void)direction;
    return 1;
}
