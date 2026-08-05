/**
 * PostBagFileNode.h — validated PostBag .crd file list node + free
 * functions that walk it.
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Split out of NetworkPlayerList.h/Netman.h so translation units that only
 * need this one small type (town/Town.cpp) don't have to pull in either
 * header's much larger surface (NetworkPlayerList's class layout, Netman.h's
 * extern "C" Win32 API block and cross-cutting globals) — Town.cpp already
 * carries its own local declarations for many of those same names with
 * incompatible types, so including either full header there is a real
 * (previously-verified) compile conflict, not just extra weight.
 *
 * Real implementations: network/NetworkPlayerList.cpp.
 */

#pragma once

#include "../shared/types.h"

/* ================================================================== */
/* PostBagFileNode — validated PostBag .crd file list node.            */
/*                                                                     */
/* Returned by NET_GetHostName (0x4446F0) as a singly-linked list of   */
/* full file paths to PostBag entries whose 2-byte header matched the  */
/* PLAYERCONFIG_MAGIC (0x66) postcard signature. The original allocates */
/* a flat operator_new(0x508) block and packs the x86 "next" pointer   */
/* into the last 4 bytes (at +0x504, right after a 0x504-byte path      */
/* buffer). That only works because an x86 pointer is 4 bytes; a host   */
/* pointer is 8, so replicating the raw layout would overrun the        */
/* allocation by 4 bytes on every 64-bit build. CLAUDE.md's host-layout  */
/* exemption applies here (exact x86 parity is a documentation/Windows- */
/* reconstruction concern, not a host requirement) — this typed struct   */
/* is the safe native replacement; callers use ->path / ->next instead   */
/* of raw offset arithmetic. Freed node-by-node via GLOBAL_free, exactly */
/* like the original's heap blocks. */
struct PostBagFileNode {
    char              path[0x504];
    PostBagFileNode*  next;
};

/**
 * NET_GetHostName — Enumerate validated PostBag .crd files.
 * Address: 0x4446F0
 *
 * Real implementation: network/NetworkPlayerList.cpp.
 *
 * @param type    PostBag subdirectory selector (0-7, see PostBag_Subdir)
 * @param param2  Optional extra numbered subfolder; every real call site
 *                recovered in this codebase passes 0.
 * @return        Caller-owned list (free node-by-node with GLOBAL_free)
 */
PostBagFileNode* NET_GetHostName(int32_t type, int32_t param2);

/**
 * NET_UpdatePlayerList — Count (and free) the Sort_Out .crd list.
 * Address: 0x445170
 */
short NET_UpdatePlayerList(void);

/**
 * NET_DownloadAsset — Read up to 0x400 bytes of a PostBag attachment.
 * Address: 0x445A40
 *
 * @param player_id  Player id (low 16 bits used) — selects the "%08d.dat"
 *                   file name within the PostBag subdirectory
 * @param type       PostBag subdirectory selector (0-7)
 * @param buf        Destination buffer (caller-owned, at least 0x400 bytes)
 */
void NET_DownloadAsset(uint32_t player_id, int32_t type, void* buf);
