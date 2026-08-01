/**
 * InputMgr.h — canonical InputMgr reconstruction (input/entity manager)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered from raw loco.exe disassembly (Ghidra MCP was not
 * available in the reconstructing session; every claim below was verified
 * with objdump on the shipped PE).
 *
 * InputMgr is the 0x20-byte static object at 0x4A9990 (g_input_mgr).  It is
 * NOT the 0x740-byte Cursor/UI window object:
 *
 *   - 0x415980 is Cursor(HINSTANCE, UINT)  — a different class entirely
 *   - 0x41F5E0 is the 0x4A99B0 object's INI event loader
 *   - 0x41D31A is padding (NOPs before INPUT_LoadWorld at 0x41D320)
 *   - 0x41D250 is the real no-arg constructor (CRT static-init thunk
 *     0x45C620: mov ecx,0x4A9990; call 0x41D250; atexit 0x45C640→0x41D2D0)
 *
 * Lifecycle (verified in the binary):
 *
 *   ctor              0x41D250  (no-arg; called by CRT thunk 0x45C620)
 *   scalar-deleting   0x41D2B0  (calls 0x41D2D0, frees if flags&1)
 *   dtor body         0x41D2D0
 *   cleanup thunk     0x41D310  (mov eax,[ecx]; jmp [eax+0x0C] → vtable[3])
 *   vtable            0x4779C8  (slot[0]=0x41D2B0 dtor, slot[1]=0x41DD80,
 *                                slot[2]=0x41DEF0, slot[3]=0x41E100;
 *                                slots [4]/[5] are float data, not code)
 *
 * x86 field layout (this + offset, 0x20 bytes total):
 *
 *   +0x00  vptr            — InputMgr vtable 0x4779C8 (compiler-managed)
 *   +0x04  list_vtable     — embedded entity-collection vtable
 *                            (0x477798 init/dead → 0x477758 running)
 *   +0x08  buffer          — heap buffer (10 slot pointers; the original
 *                            allocates 0x28 bytes = 10×4 on x86, the host
 *                            uses 10×sizeof(void*))
 *   +0x0C  capacity        — 10 if the allocation succeeded, else 0
 *   +0x10  count           — populated entry count
 *   +0x14  entity_count    — entity sub-count (random pick range, mode -1)
 *   +0x18  special_count   — special/vehicle sub-count (mode 3)
 *   +0x1C  (padding)       — BSS extent ends at 0x4A99B0 (next object)
 *
 * The embedded collection at +0x04 shares the TimerList-family vtable
 * 0x477798/0x477758 used by Game's inline timer list (core/Game.h +0x10C).
 * Its binary interface (all verified by disassembly):
 *
 *   [0] 0x435D10  Resize            [1] 0x4125C0
 *   [2] 0x424020                    [3] 0x4241E0  RemoveAt (shift-remove)
 *   [4] 0x4356E0  RemoveElement     [5] 0x424250  Stop
 *   [6] 0x424270  ClearAll          [7] 0x424530  ItemAt (capacity check)
 *   [8] 0x424030  GetItem           [9] 0x412140
 *   [10] 0x4124B0                   [11] 0x424000  GetCount
 *   [12] 0x424760                   [13] 0x412440  Insert
 *   [14] 0x412540  FindIndex
 *
 * In the C++ model the collection vtable is never read or written; all
 * dispatch goes through the typed methods below (per AGENTS.md: let the
 * compiler manage vtables, no literal VTBL_ writes).
 */

#pragma once

#include "../shared/types.h"

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

class Game;
class Entity;

/* ================================================================== */
/* InputMgr — 0x20-byte static object (g_input_mgr at 0x4A9990)        */
/* ================================================================== */

class InputMgr {
public:
    /* ---- Lifecycle ------------------------------------------------- */

    /**
     * Constructor. Address: 0x41D250.
     *
     * No-arg; the CRT static-init thunk 0x45C620 constructs the static
     * object at 0x4A9990 with it.  Allocates the 0x28-byte slot buffer
     * (10 zeroed pointers), sets capacity to 10 on success (0 on failure),
     * zeroes count/entity_count, and leaves the vtable to the compiler.
     * (The binary also flips the embedded-list vtable 0x477798 → 0x477758;
     * the C++ model dispatches list operations through typed methods.)
     */
    InputMgr();

    /**
     * Scalar deleting destructor. Address: 0x41D2B0 (wrapper) /
     * 0x41D2D0 (body).  Compiler-generated wrapper calls DtorBody and
     * frees the object when the deleting flag is set.
     */
    virtual ~InputMgr();

    /** Destructor body. Address: 0x41D2D0. */
    void DtorBody();

    /* ---- vtable[3] method (cleanup thunk 0x41D310 dispatches here) -- */

    /**
     * Reset world/entity state. Address: 0x41E100.
     *
     * Deselects the Game's selected object (Game::DeselectGameObject,
     * 0x411580, on g_game at 0x4854C8), clears the embedded entity
     * collection (collection vtable[6], 0x424270 — deletes every
     * element), and zeroes entity_count (+0x14) and special_count
     * (+0x18).  Called from CGWND_Cleanup (0x407ABE via the 0x41D310
     * thunk) and TileMap::FullReset (0x455003).  The legacy label
     * "INPUT_FileDlgProc" for this function is a misnomer — the body
     * contains no file-dialog logic.
     */
    void ResetWorldState();

    /* ---- Embedded entity collection (+0x04..+0x1C) ----------------- */

    /* +0x04 collection vtable (0x477798 init/dead → 0x477758 running).
     * Kept for layout documentation only; typed methods below replace all
     * vtable reads/writes in the C++ model. */
    void*    list_vtable;
    void**   buffer;          /* +0x08 heap buffer, 10 slots (0x28 bytes
                                  on x86 = 10×4; host uses 10×sizeof(void*)) */
    int32_t  capacity;        /* +0x0C 10 if allocated, else 0 */
    int32_t  count;           /* +0x10 populated entries */
    int32_t  entity_count;    /* +0x14 entity sub-count */
    int32_t  special_count;   /* +0x18 special/vehicle sub-count */
    uint32_t _pad_1c;         /* +0x1C BSS padding to 0x4A99B0 */

    /* ---- Typed collection interface (original vtable slots) -------- */

    /** GetCount — collection vtable[11], 0x424000. */
    int32_t ListGetCount() const;

    /** GetItem — collection vtable[8] (0x424030) → vtable[7] (0x424530):
     *  buffer[index] when 0 <= index < capacity, else nullptr. */
    void* ListGetItem(int32_t index) const;

    /** RemoveAt — collection vtable[3], 0x4241E0.  Shift-remove at index,
     *  decrements count, returns the removed element (or nullptr when
     *  index is out of range).  Does not destroy the element. */
    void* ListRemoveAt(int32_t index);

    /** ClearAll — collection vtable[6], 0x424270.  Repeatedly removes
     *  the last element (RemoveElement, vtable[4] 0x4356E0) which
     *  destroys each removed element via its virtual destructor. */
    void ListClearAll();
};

/* ================================================================== */
/* g_input_mgr — static object at 0x4A9990                             */
/*                                                                      */
/* Defined in input/InputMgr.cpp as `InputMgr g_input_mgr;`.  The C++   */
/* static-init constructor/destructor reproduce the original CRT thunks */
/* 0x45C620 / 0x45C640.  Every INPUT_* entry point takes InputMgr* —    */
/* the original thiscall passes ECX = 0x4A9990 (the object address).    */
/* ================================================================== */

extern InputMgr g_input_mgr;

/* ================================================================== */
/* INPUT_* entry points (free thiscall functions in the binary)         */
/* ================================================================== */

/** Per-frame entity tick. Address: 0x41DD40.  Iterates the embedded
 *  collection and calls Entity::Update (vtable[10], 0x405C40) on each
 *  entry.  Called every frame in game modes 3 and 9 from
 *  GameLoop_FrameUpdate (0x45C4F5).  The "GetSaveFileName" name is a
 *  legacy misnomer — the function returns nothing and generates no file
 *  name; it updates placed entities. */
void INPUT_GetSaveFileName(InputMgr* self);

/* ---- World new/load/save (NOT implemented in this milestone; the
 * ---- deferred stubs in InputMgr.cpp log and abort, see PROGRESS.md) -- */

void  INPUT_NewWorld(InputMgr* self);                          /* 0x41E120 */
char  INPUT_LoadWorld(InputMgr* self, const char* path);       /* 0x41D320 */
char  INPUT_LoadSaveFile(InputMgr* self, int a, int b,
                         const char* path);                    /* 0x41D5C0 */
void  INPUT_SaveCurrentWorld(InputMgr* self, const char* name);/* 0x41D9B0 */

/* ---- Editor placement (deferred stubs; gameplay, not persistence) -- */

void*      INPUT_PlaceObject(InputMgr* self, unsigned int resource_id); /* 0x41DD80 */
uintptr_t  INPUT_RemoveObject(InputMgr* self, void* obj,
                              unsigned int param);            /* 0x41DEF0 */
void*      INPUT_FindObjectAt(InputMgr* self, int mode);      /* 0x41E1F0 */
