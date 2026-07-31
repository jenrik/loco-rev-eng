/**
 * NetHelpers.h — Network helper classes (PoolAllocator, DPLAY helpers)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * PoolAllocator is a fixed-size memory pool (~0x20034 bytes) used by the
 * network/surface system. The binary uses dispatch tables at 0x47826C
 * during initialization and 0x478270 after initialization. In reconstructed
 * C++, dispatch is represented by the virtual Unlock method.
 * NET_Shutdown cleans up resources (calls ResourceManager_Shutdown and
 * a file data destructor at +0x18).
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* ================================================================== */
/* PoolAllocator — Fixed-size memory pool with free-list               */
/*                                                                    */
/* Size: ~0x20034 bytes                                                */
/* Vtable (init): 0x47826C                                             */
/* Vtable (post-init): 0x478270                                        */
/* ================================================================== */
struct PoolAllocator {
    virtual ~PoolAllocator() {}

    /* +0x00: compiler-managed dispatch pointer (binary: 0x47826C/0x478270) */
    /* vtable at +0x00 is compiler-managed via virtual methods */
    /* +0x04: Counter 1 (initialized to 0 by NET_Lock) */
    int32_t     counter1;

    /* +0x08: Counter 2 (initialized to 0 by NET_Lock) */
    int32_t     counter2;

    /* +0x0C: Counter 3 (initialized to 0 by NET_Lock) */
    int32_t     counter3;

    /* +0x10: Counter 4 (initialized to 0 by NET_Lock) */
    int32_t     counter4;

    /* +0x14: Padding/gap (4 bytes) */
    uint8_t     _pad_14[4];

    /* +0x18: File data sub-structure (freed by NET_Shutdown sub-call) */
    /*   DDRAW_FileData_Dtor at 0x45CA20: frees file handle + linked lists + name */
    uint32_t    filedata[4];        /* 16 bytes for the sub-object */

    /* +0x28: Padding/gap (4 bytes) */
    uint8_t     _pad_28[4];

    /* +0x2C: Free-list array — 0x4000 entries, each 4 bytes
     *   Each entry at index i points to index i + 0x4001 (linked-list slot offset)
     */
    int32_t     freelist[0x4000];   /* 0x10000 bytes, from +0x2C to +0x1002B */

    /* +0x10030: Flags array — 0x4001 entries, each 4 bytes
     *   Initialized to 0 by NET_Lock
     */
    int32_t     alloc_flags[0x4001]; /* 0x10004 bytes, from +0x10030 to +0x20033 */

    /* ================================================================ */
    /* Methods                                                          */
    /* ================================================================ */

    /**
     * NET_Lock — Pool allocator init (constructor).
     * Address: 0x445F70
     *
     * __fastcall (ECX = this). The binary selects dispatch table 0x478270,
     * clears the flags array (0x4001 dwords), initializes the free-list
     * (each entry points 0x4001 slots ahead), and zeros 4 counters.
     *
     * Called by: GameLoop_FrameUpdate (via pool initialization)
     *
     * @return  Pointer to this (the initialized pool)
     */
    void* __fastcall Lock();

    /**
     * NET_Unlock — Pool allocator scalar deleting destructor.
     * Address: 0x445FC0
     *
     * __thiscall (ECX = this, stack = flags byte). Calls NET_Shutdown
     * then frees this via GLOBAL_free if flags & 1.
     * This is virtual slot [0] in the post-initialization table at 0x478270.
     *
     * @param flags  0 = don't free, 1 = free after cleanup
     * @return       Pointer to this (or NULL if freed)
     */
    virtual void* __thiscall Unlock(uint8_t flags);

    /**
     * NET_Shutdown — Pool allocator full cleanup.
     * Address: 0x445FE0
     *
     * __fastcall (ECX = this). Uses SEH for exception safety.
     * The binary restores dispatch table 0x478270, calls ResourceManager_Shutdown(this),
     * then frees the sub-object at +0x18 via DDRAW_FileData_Dtor.
     */
    void __fastcall Shutdown();
};
