/**
 * InputMgr.cpp — canonical InputMgr implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered from raw loco.exe disassembly (Ghidra MCP was not
 * available in the reconstructing session; every claim below was verified
 * with objdump on the shipped PE).
 *
 * Implements the 0x20-byte InputMgr class (static object g_input_mgr at
 * 0x4A9990):
 *
 *   InputMgr()        0x41D250  no-arg ctor (CRT thunk 0x45C620)
 *   ~InputMgr()       0x41D2B0  scalar-deleting wrapper (compiler-generated)
 *   DtorBody()        0x41D2D0  dtor body
 *   ResetWorldState() 0x41E100  vtable[3]; cleanup thunk 0x41D310 target
 *   INPUT_GetSaveFileName() 0x41DD40  per-frame entity tick (misnomer)
 *
 * World new/load/save (INPUT_NewWorld 0x41E120, INPUT_LoadWorld 0x41D320,
 * INPUT_LoadSaveFile 0x41D5C0, INPUT_SaveCurrentWorld 0x41D9B0) and the
 * editor placement helpers (INPUT_PlaceObject 0x41DD80, INPUT_RemoveObject
 * 0x41DEF0, INPUT_FindObjectAt 0x41E1F0) are NOT part of this milestone:
 * they live here as deferred stubs that log loudly and abort, replacing the
 * previous silent no-op stubs.  Tracked in PROGRESS.md.
 */

#include "InputMgr.h"
#include "../core/Game.h"
#include "../core/Entity.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* operator_new(size_t size);            /* 0x465CE0 */
extern void  GLOBAL_free(void* ptr);               /* 0x465CD0 */
extern void* g_game;                               /* 0x4854C8 */

/* ================================================================== */
/* g_input_mgr — static object at 0x4A9990                             */
/*                                                                      */
/* The original is a BSS static object constructed by the CRT thunk     */
/* 0x45C620 (mov ecx,0x4A9990; call 0x41D250) and destroyed at exit     */
/* through 0x45C640 (jmp 0x41D2D0).  A namespace-scope C++ object with  */
/* a constructor/destructor reproduces exactly that lifecycle.          */
/* ================================================================== */

InputMgr g_input_mgr;

/* ================================================================== */
/* Constructor                                                         */
/* Address: 0x41D250 (CRT static-init thunk 0x45C620)                  */
/*                                                                      */
/* Allocates the 0x28-byte slot buffer (10 pointers), zeroes it, sets   */
/* capacity = 10 (0 on allocation failure), and zeroes count and the    */
/* entity sub-counts.  The binary additionally flips the embedded-list  */
/* vtable from 0x477798 (init) to 0x477758 (running); the C++ model     */
/* dispatches list operations through the typed methods instead.        */
/* ================================================================== */
InputMgr::InputMgr()
{
    this->buffer = nullptr;                 /* +0x08 */
    this->capacity = 0;                     /* +0x0C */

    /* The original allocates 0x28 bytes = 10 four-byte slots on x86 and
     * zeroes 10 dwords (mov ecx,0xA; rep stosd).  The host uses the same
     * ten-slot capacity with native pointer width (10 * sizeof(void*)),
     * which is byte-identical on 32-bit and a safe native layout on
     * 64-bit hosts. */
    const size_t slot_bytes = 10 * sizeof(void*);
    void** items = static_cast<void**>(operator_new(slot_bytes));
    this->buffer = items;
    if (items != nullptr) {
        std::memset(items, 0, slot_bytes);
        this->capacity = 10;                /* +0x0C */
    } else {
        this->buffer = nullptr;             /* +0x08 */
        this->capacity = 0;                 /* +0x0C */
    }

    this->count = 0;                        /* +0x10 */
    this->entity_count = 0;                 /* +0x14 */
    /* +0x18 special_count: the original relies on BSS zeroing (the ctor
     * does not write it); set it explicitly so stack instances match. */
    this->special_count = 0;                /* +0x18 */
}

/* ================================================================== */
/* Destructor body                                                     */
/* Address: 0x41D2D0                                                   */
/*                                                                      */
/* Resets count and capacity to 0 and frees the slot buffer.  (The      */
/* binary also restores vtable 0x4779C8 and flips the embedded-list     */
/* vtable to the 0x477798 dead marker — compiler/typed-model artifacts  */
/* in the C++ reconstruction.)                                          */
/* ================================================================== */
void InputMgr::DtorBody()
{
    this->count = 0;                        /* +0x10 */
    this->capacity = 0;                     /* +0x0C */
    if (this->buffer != nullptr) {          /* +0x08 */
        GLOBAL_free(this->buffer);
        this->buffer = nullptr;             /* +0x08 */
    }
}

/* ================================================================== */
/* Scalar deleting destructor                                          */
/* Address: 0x41D2B0 (wrapper) — compiler-generated: calls DtorBody,   */
/* then frees the object when the deleting flag is set.                */
/* ================================================================== */
InputMgr::~InputMgr()
{
    DtorBody();
}

/* ================================================================== */
/* Embedded collection interface                                       */
/* ================================================================== */

/* ---- GetCount — collection vtable[11], 0x424000: return count ---- */
int32_t InputMgr::ListGetCount() const
{
    return this->count;                     /* +0x10 */
}

/* ---- GetItem — collection vtable[8] (0x424030) → vtable[7] (0x424530)
 *      buffer[index] when 0 <= index < capacity (unsigned compare),
 *      else nullptr.                                                ---- */
void* InputMgr::ListGetItem(int32_t index) const
{
    if (static_cast<uint32_t>(index) >= static_cast<uint32_t>(this->capacity)) {
        return nullptr;
    }
    return this->buffer[index];             /* +0x08 */
}

/* ---- RemoveAt — collection vtable[3], 0x4241E0.
 *      Shift-remove at index: shift the tail left, null the last slot,
 *      decrement count, return the removed element (nullptr out of
 *      range).  Does not destroy the element.                        ---- */
void* InputMgr::ListRemoveAt(int32_t index)
{
    void* element = this->ListGetItem(index);
    if (element == nullptr) {
        return nullptr;
    }
    if (index < this->count - 1) {
        std::memmove(&this->buffer[index], &this->buffer[index + 1],
                     static_cast<size_t>(this->count - index - 1) * sizeof(void*));
    }
    this->buffer[this->count - 1] = nullptr;
    this->count--;                          /* +0x10 */
    return element;
}

/* ---- ClearAll — collection vtable[6], 0x424270.
 *      While count > 0, remove the last element and destroy it
 *      (RemoveElement vtable[4], 0x4356E0, deletes the removed element
 *      through its virtual destructor).                             ---- */
void InputMgr::ListClearAll()
{
    while (this->count > 0) {
        void* element = this->ListRemoveAt(this->count - 1);
        if (element != nullptr) {
            delete static_cast<Entity*>(element);
        }
    }
}

/* ================================================================== */
/* ResetWorldState — vtable[3] (cleanup thunk 0x41D310 target)         */
/* Address: 0x41E100                                                   */
/*                                                                      */
/* Deselects the Game's selected object (Game::DeselectGameObject,      */
/* 0x411580), clears the embedded entity collection (deleting every     */
/* element), and zeroes entity_count (+0x14) and special_count (+0x18). */
/* Called from CGWND_Cleanup (0x407ABE) and TileMap::FullReset          */
/* (0x455003).  Legacy label "INPUT_FileDlgProc" is a misnomer.          */
/* ================================================================== */
void InputMgr::ResetWorldState()
{
#ifndef _WIN32
    /* Host-only deviation: g_game (0x4854C8) is constructed by
     * BootstrapMode3Core at runtime, not at static init; it may be null
     * in host tests.  The original calls unconditionally. */
    if (g_game != nullptr)
#endif
    {
        static_cast<Game*>(g_game)->DeselectGameObject();   /* 0x411580 */
    }
    this->ListClearAll();                   /* collection vtable[6] 0x424270 */
    this->entity_count = 0;                 /* +0x14 */
    this->special_count = 0;                /* +0x18 */
}

/* ================================================================== */
/* INPUT_GetSaveFileName — per-frame entity tick                       */
/* Address: 0x41DD40                                                   */
/*                                                                      */
/* Iterates the embedded entity collection and calls Entity::Update     */
/* (vtable[10], 0x405C40) on each entry.  The count is re-read every     */
/* iteration because updates may mutate the list.  Called every frame   */
/* in game modes 3 and 9 from GameLoop_FrameUpdate (0x45C4F5).          */
/* The name is a legacy misnomer — the function neither reads nor        */
/* returns a file name.                                                 */
/* ================================================================== */
void INPUT_GetSaveFileName(InputMgr* self)
{
    int32_t index = 0;
    while (index < self->ListGetCount()) {
        void* item = self->ListGetItem(index);
        if (item != nullptr) {
            static_cast<Entity*>(item)->Update();   /* vtable[10] 0x405C40 */
        }
        index++;
    }
}

/* ================================================================== */
/* Deferred InputMgr entry points                                      */
/*                                                                      */
/* World new/load/save and the editor placement helpers are later       */
/* milestones (tracked in PROGRESS.md).  These stubs log a clear        */
/* warning and abort instead of silently succeeding — the previous      */
/* no-op stubs masked the missing decompilation.                        */
/* ================================================================== */

namespace {

[[noreturn]] void inputmgr_deferred(const char* name, uint32_t address)
{
    std::fprintf(stderr,
        "[InputMgr] %s (0x%08X) is a deferred stub: not yet decompiled "
        "(world new/load/save and placement helpers are later milestones)\n",
        name, address);
    std::fflush(stderr);
    std::abort();
}

} // namespace

/* ---- World new/load/save (deferred; NOT this milestone) ----------- */

void INPUT_NewWorld(InputMgr* self)              /* 0x41E120 */
{
    (void)self;
    inputmgr_deferred("INPUT_NewWorld", 0x41E120);
}

char INPUT_LoadWorld(InputMgr* self, const char* path)   /* 0x41D320 */
{
    (void)self;
    (void)path;
    inputmgr_deferred("INPUT_LoadWorld", 0x41D320);
}

char INPUT_LoadSaveFile(InputMgr* self, int a, int b, const char* path) /* 0x41D5C0 */
{
    (void)self;
    (void)a;
    (void)b;
    (void)path;
    inputmgr_deferred("INPUT_LoadSaveFile", 0x41D5C0);
}

void INPUT_SaveCurrentWorld(InputMgr* self, const char* name)  /* 0x41D9B0 */
{
    (void)self;
    (void)name;
    inputmgr_deferred("INPUT_SaveCurrentWorld", 0x41D9B0);
}

/* ---- Editor placement helpers (deferred) --------------------------- */

void* INPUT_PlaceObject(InputMgr* self, unsigned int resource_id)  /* 0x41DD80 */
{
    (void)self;
    (void)resource_id;
    inputmgr_deferred("INPUT_PlaceObject", 0x41DD80);
}

uintptr_t INPUT_RemoveObject(InputMgr* self, void* obj, unsigned int param) /* 0x41DEF0 */
{
    (void)self;
    (void)obj;
    (void)param;
    inputmgr_deferred("INPUT_RemoveObject", 0x41DEF0);
}

void* INPUT_FindObjectAt(InputMgr* self, int mode)   /* 0x41E1F0 */
{
    (void)self;
    (void)mode;
    inputmgr_deferred("INPUT_FindObjectAt", 0x41E1F0);
}
