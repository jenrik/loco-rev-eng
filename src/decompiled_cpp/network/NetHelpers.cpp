/**
 * NetHelpers.cpp — PoolAllocator implementation (NET_Lock, NET_Unlock, NET_Shutdown)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * PoolAllocator is the fixed-size memory pool used by the DDraw surface
 * cache / file data subsystem. The pool has 0x4000 free-list entries and
 * 0x4001 allocation flags.
 *
 * === Lifecycle ===
 *
 * 1. Object is created (operator_new of ~0x20034 bytes)
 * 2. NET_Lock (0x445F70, __fastcall) is called:
 *    - Initializes sub-object at +0x18
 *    - Selects the post-initialization dispatch table at 0x478270
 *    - Clears flags array and initializes free-list pointers
 *    - Zeros 4 counters
 * 3. NET_Shutdown (0x445FE0, __fastcall) is called:
 *    - Uses SEH for protection
 *    - Calls ResourceManager_Shutdown (0x446340) on the pool
 *    - Frees sub-object at +0x18 via DDRAW_FileData_Dtor (0x45CA20)
 * 4. NET_Unlock (0x445FC0, __thiscall, vtable slot[0]) is called:
 *    - Calls NET_Shutdown
 *    - Optionally frees memory via GLOBAL_free
 */

#include "NetHelpers.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* DDRAW_FreeClipper at 0x45CA10 — Zero-initialize sub-struct at +0x18 (4 dwords) */
void __fastcall DDRAW_FreeClipper(void* ptr);

/* DDRAW_FileData_Dtor at 0x45CA20 — Free file handle, decompressed block list, and name */
void __fastcall DDRAW_FileData_Dtor(void* ptr);

/* ResourceManager_Shutdown at 0x446340 — Shut down the resource manager (stop audio,
   free all resources, release DDRAW surfaces, destroy audio, delete 5 GDI fonts).
   __fastcall: ECX = ResourceManager pointer (this PoolAllocator pointer is passed). */
void __fastcall ResourceManager_Shutdown(int32_t param_1);

/* CRT / Heap */
void __cdecl GLOBAL_free(void* ptr);       /* 0x465CD0 */

/* SEH helpers (inline) */
extern void* CRT_exception_handler;        /* 0x475F8B — __except handler */

/* ================================================================== */
/* NET_Lock — 0x445F70                                                 */
/*                                                                     */
/* __fastcall (ECX = this). Initializes the pool allocator:            */
/* 1. Zeros the sub-object at +0x18 (DDRAW_FreeClipper)                */
/* 2. Selects post-initialization dispatch table 0x478270             */
/* 3. Clears flags array (0x4001 dwords at +0x10030)                  */
/* 4. Sets up free-list: each entry at +0x2c+i points to +0x2c+i+0x4001 */
/* 5. Zeros 4 counters at +0x04, +0x08, +0x0C, +0x10                 */
/*                                                                     */
/* Called by: GameLoop_FrameUpdate (0x45C565)                          */
/* ================================================================== */
void* __fastcall PoolAllocator::Lock()
{
    /* Step 1: Initialize sub-object at +0x18 */
    /* DDRAW_FreeClipper(ptr) zeros 4 dwords at ptr */
    DDRAW_FreeClipper(filedata);

    /* Step 2: Dynamic dispatch is compiler-managed in reconstructed C++. */

    /* Step 3: Clear flags array (0x4001 dwords at +0x10030) */
    {
        int32_t i;
        for (i = 0; i < 0x4001; i++) {
            alloc_flags[i] = 0;
        }
    }

    /* Step 4: Initialize free-list (0x4000 entries at +0x2c)
     * Each entry points to the entry 0x4001 slots ahead:
     *   freelist[i] = &freelist[i + 0x4001]
     * This creates a linked chain where the "next free" slot is 0x4001 entries away
     * (0x10004 bytes = each slot is 4 bytes, so 0x4001 * 4 = 0x10004 bytes offset)
     */
    {
        int32_t i;
        for (i = 0; i < 0x4000; i++) {
            freelist[i] = (int32_t)(uintptr_t)(&freelist[i] + 0x4001);
        }
    }

    /* Step 5: Zero all 4 counters */
    counter1 = 0;
    counter2 = 0;
    counter3 = 0;
    counter4 = 0;

    return this;
}

/* ================================================================== */
/* NET_Unlock — 0x445FC0                                               */
/*                                                                     */
/* __thiscall (ECX = this, stack param: flags byte).                   */
/* Scalar deleting destructor (virtual slot [0], table 0x478270).      */
/* Calls NET_Shutdown then frees memory if flags & 1.                  */
/* ================================================================== */
void* __thiscall PoolAllocator::Unlock(uint8_t flags)
{
    this->Shutdown();
    if ((flags & 1) != 0) {
        GLOBAL_free(this);
        return NULL;
    }
    return this;
}

/* ================================================================== */
/* NET_Shutdown — 0x445FE0                                             */
/*                                                                     */
/* __fastcall (ECX = this). Clean up the pool allocator:               */
/* 1. Restore post-init dispatch table 0x478270 (binary unwind detail) */
/* 2. Call ResourceManager_Shutdown (0x446340)                         */
/* 3. Free sub-object at +0x18 (DDRAW_FileData_Dtor, 0x45CA20)        */
/*                                                                     */
/* Uses SEH (__try/__except) for exception safety. The exception       */
/* handler pattern matches MSVC's standard SEH prologue/epilogue.      */
/* ================================================================== */
void __fastcall PoolAllocator::Shutdown()
{
    /* MSVC SEH setup: save FS:[0] (ExceptionList), set new handler */
    /* The exception handler at 0x475F8B catches all exceptions */
    volatile int32_t try_level = 0;

    /* __try removed — MSVC SEH not supported on GCC */
    {
        /* Dynamic dispatch remains compiler-managed during cleanup. */

        try_level = 0;

        /* Call ResourceManager_Shutdown on this pool allocator.
         * ResourceManager_Shutdown is __fastcall (ECX = param_1).
         * It stops audio, frees all resources, releases DDRAW surfaces,
         * destroys audio, and deletes 5 GDI font objects. */
        ResourceManager_Shutdown((int32_t)this);

        /* Free sub-object at +0x18 */
        try_level = -1;
        DDRAW_FileData_Dtor(filedata);

    }

    /* Restore ExceptionList (handled by SEH epilogue) */
}
