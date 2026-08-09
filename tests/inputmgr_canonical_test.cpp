// Status: VALIDATED
/**
 * InputMgr canonical reconstruction — component regression.
 *
 * Evidence source: raw disassembly of loco.exe (Ghidra MCP unavailable in
 * the reconstructing session; all offsets/addresses verified with objdump).
 *
 * InputMgr is the 0x20-byte static object at 0x4A9990 (g_input_mgr):
 *
 *   ctor              0x41D250  (CRT static-init thunk 0x45C620)
 *   scalar-deleting   0x41D2B0  (wrapper) / 0x41D2D0 (body)
 *   vtable            0x4779C8  (slot[3] = 0x41E100 ResetWorldState;
 *                                cleanup thunk 0x41D310 dispatches slot[3])
 *   fields            +0x04 embedded collection vtable (0x477798→0x477758)
 *                     +0x08 heap buffer (0x28 bytes, 10 Entity* slots)
 *                     +0x0C capacity (10) / +0x10 count / +0x14 entity count
 *                     +0x18 special/vehicle count
 *
 * This test links against the real InputMgr.o and exercises:
 *   1. ctor field contract (buffer allocated+zeroed, capacity 10, counts 0)
 *   2. typed collection ops GetCount/GetItem/RemoveAt (vtable[11]/[8]/[3])
 *   3. INPUT_GetSaveFileName (0x41DD40) per-frame tick on an empty list
 *      (the real GameLoop mode-3/9 entry point; the loop body is skipped)
 *   4. ResetWorldState (0x41E100) clears the list and zeroes sub-counts
 *      (with an empty g_game, so no Game object is needed)
 *   5. DtorBody (0x41D2D0) frees the buffer and resets count/capacity
 *
 * The link is honest: NO --unresolved-symbols=ignore-all.  Every symbol
 * InputMgr.o references is either provided by persistence_fixtures.h
 * (canonical globals + fail-loud fixtures for the gated host paths) or
 * comes from the C/C++ runtime.  If a future change makes InputMgr.o
 * reference anything else, this test will fail to link instead of
 * silently resolving to zero.
 */
#include "input/InputMgr.h"
#include "persistence_fixtures.h"   /* canonical globals + fail-loud fixtures */
#include "core/Game.h"              /* Game::DeselectGameObject fixture */
#include "core/Entity.h"            /* Entity element type of the collection */

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int failures = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::fprintf(stderr, "FAIL: %s\n", msg);                          \
            failures++;                                                       \
        }                                                                     \
    } while (0)

int main()
{
    /* ---- 1. ctor contract (0x41D250) ---- */
    {
        InputMgr mgr;
        CHECK(mgr.buffer != nullptr, "ctor allocates the 0x28-byte slot buffer");
        CHECK(mgr.capacity == 10, "ctor sets capacity to 10 on success");
        CHECK(mgr.count == 0, "ctor zeroes count");
        CHECK(mgr.entity_count == 0, "ctor zeroes entity_count");
        CHECK(mgr.special_count == 0, "ctor zeroes special_count");
        if (mgr.buffer != nullptr) {
            for (int i = 0; i < 10; i++) {
                if (mgr.buffer[i] != nullptr) {
                    std::fprintf(stderr, "FAIL: ctor buffer slot %d not zeroed\n", i);
                    failures++;
                    break;
                }
            }
        }
        mgr.DtorBody();
        CHECK(mgr.buffer == nullptr, "DtorBody frees and nulls the buffer");
        CHECK(mgr.capacity == 0, "DtorBody resets capacity to 0");
        CHECK(mgr.count == 0, "DtorBody resets count to 0");
    }

    /* ---- 2. typed collection ops (vtable[11] GetCount / [8] GetItem) ----
     * Dummy Entity* addresses are only ever compared by identity; they are
     * never dereferenced, so no Entity vtable is required. */
    {
        InputMgr mgr;
        Entity* a = reinterpret_cast<Entity*>(static_cast<uintptr_t>(0x100));
        Entity* b = reinterpret_cast<Entity*>(static_cast<uintptr_t>(0x200));
        Entity* c = reinterpret_cast<Entity*>(static_cast<uintptr_t>(0x300));
        mgr.buffer[0] = a;
        mgr.buffer[1] = b;
        mgr.buffer[2] = c;
        mgr.count = 3;

        CHECK(mgr.ListGetCount() == 3, "GetCount returns count (+0x10)");
        CHECK(mgr.ListGetItem(0) == a, "GetItem(0) returns buffer[0]");
        CHECK(mgr.ListGetItem(1) == b, "GetItem(1) returns buffer[1]");
        CHECK(mgr.ListGetItem(2) == c, "GetItem(2) returns buffer[2]");
        CHECK(mgr.ListGetItem(3) == nullptr, "GetItem(3) is null (slot zeroed)");
        CHECK(mgr.ListGetItem(10) == nullptr, "GetItem(10) is null (>= capacity)");

        /* RemoveAt (vtable[3] 0x4241E0): shift-remove, count-- */
        Entity* removed = mgr.ListRemoveAt(1);
        CHECK(removed == b, "RemoveAt(1) returns buffer[1]");
        CHECK(mgr.count == 2, "RemoveAt decrements count");
        CHECK(mgr.ListGetItem(0) == a, "RemoveAt shifts tail left (slot 0)");
        CHECK(mgr.ListGetItem(1) == c, "RemoveAt shifts tail left (slot 1)");
        CHECK(mgr.buffer[2] == nullptr, "RemoveAt nulls the vacated last slot");

        removed = mgr.ListRemoveAt(0);
        CHECK(removed == a, "RemoveAt(0) returns the first element");
        CHECK(mgr.count == 1 && mgr.ListGetItem(0) == c,
              "RemoveAt(0) shifts c into slot 0");

        removed = mgr.ListRemoveAt(0);
        CHECK(removed == c && mgr.count == 0, "RemoveAt drains the list");
        CHECK(mgr.ListRemoveAt(0) == nullptr, "RemoveAt on empty list returns null");

        /* DtorBody must tolerate a non-empty list (elements are not owned
         * here; the ctor contract leaves the buffer free-able). */
        mgr.DtorBody();
        CHECK(mgr.buffer == nullptr && mgr.capacity == 0, "DtorBody teardown after list ops");
    }

    /* ---- 3. INPUT_GetSaveFileName (0x41DD40) on an empty list ----
     * This is the real per-frame entity tick GameLoop_FrameUpdate runs in
     * modes 3/9.  With count == 0 the loop body (Entity::Update via
     * vtable[10]) never executes, so no Entity vtable is needed — but the
     * exported entry point and its loop-boundary logic are exercised. */
    {
        InputMgr mgr;
        INPUT_GetSaveFileName(&mgr);
        CHECK(mgr.count == 0, "INPUT_GetSaveFileName on empty list is a no-op");
        mgr.DtorBody();
    }

    /* ---- 4. ResetWorldState (0x41E100) on an empty list ---- */
    {
        InputMgr mgr;
        mgr.entity_count = 4;
        mgr.special_count = 2;
        /* g_game == nullptr: the host-only guard skips DeselectGameObject.
         * The embedded list is empty, so ClearAll is a no-op. */
        mgr.ResetWorldState();
        CHECK(mgr.entity_count == 0, "ResetWorldState zeroes entity_count (+0x14)");
        CHECK(mgr.special_count == 0, "ResetWorldState zeroes special_count (+0x18)");
        CHECK(mgr.count == 0, "ResetWorldState leaves count at 0");
        mgr.DtorBody();
    }

    /* ---- 5. INPUT_DirToOffset_* (0x41D8F0/0x41D920/0x41D950/0x41D980)
     * Neighbour-tile offsets packed as (Y << 16) | X from the 16-bit
     * globals g_player_id (0x4AAD46) / g_player_color (0x4AAD48). ---- */
    {
        g_player_id = 0x12;       /* 18 */
        g_player_color = 0;       /* low 16 bits used */
        int32_t out = 0;
        /* Up:   X = id - 3 = 15, Y = (color>>1)-1 = -1 */
        INPUT_DirToOffset_Up(&out);
        CHECK(out == static_cast<int32_t>(0xFFFF0000u | 0x0Fu),
              "DirToOffset_Up packs (Y<<16)|X");
        /* Left: X = 0, Y = -1 */
        INPUT_DirToOffset_Left(&out);
        CHECK(out == static_cast<int32_t>(0xFFFF0000u), "DirToOffset_Left packs Y<<16");
        /* Down: X = (id>>1)-1 = 8, Y = color-2 = -2 */
        INPUT_DirToOffset_Down(&out);
        CHECK(out == static_cast<int32_t>(0xFFFE0000u | 0x08u),
              "DirToOffset_Down packs (Y<<16)|X");
        /* Right: X = 8, Y = 0 */
        INPUT_DirToOffset_Right(&out);
        CHECK(out == 8, "DirToOffset_Right packs X only");
        g_player_id = 0;
        g_player_color = 0;
    }

    if (failures == 0) {
        std::puts("PASS: canonical InputMgr ctor/collection/tick/reset/dtor contract");
        return 0;
    }
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
}
