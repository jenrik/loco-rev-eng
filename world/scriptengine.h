/**
 * scriptengine.h — ScriptEngine class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The ScriptEngine is a utility class used by ScriptedObject for managing
 * script-like animation/callback dispatch on game entities. It inherits
 * from a RESDATA base and stores critical section, state flags, and a
 * pointer to a callback/dispatch function.
 *
 * Class hierarchy:
 *   RESDATA base (vtable 0x4782A4)
 *     +-- ScriptEngine (base: RESDATA, vtable 0x478378)
 *
 * BLOCKER (2026-08-16, not resolved by this pass): reading ScriptEngine's
 * own full vtable (0x478378) directly shows several slots installed with
 * the EXACT SAME addresses as Panel's own vtable (0x454820
 * Panel::SetPosition at slot 3, 0x454890 Panel::UpdateChild at slot 1,
 * 0x4549E0 Panel::HitTestChildren at slot 5, 0x454680 Panel::Init at
 * slot 6, ...) — evidence that ScriptEngine's real inheritance may not be
 * the plain "RESDATA base" this class currently models, but something
 * Panel-shaped. Re-deriving ScriptEngine's true hierarchy and full field
 * layout is a separate, substantial investigation, tracked in
 * PROGRESS.md, not attempted here.
 *
 * RECONCILIATION (2026-08-16): this file's own `RESDATA_ScriptedObject`
 * class — an independent, unreconciled duplicate of
 * `game/ScriptedObject.h`'s `ScriptedObject` (same vtable 0x4782A8, same
 * ~0x74C size, same method address list) — has been removed. See
 * `game/ScriptedObject.h`'s class doc comment for the full trail and
 * PROGRESS.md's Completed section for this session's evidence.
 */

#pragma once

#include "../shared/types.h"

// Status: TRANSCRIBED
/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */
class GameObject;
class TrackPiece;

/* ================================================================== */
/* ScriptEngine class                                                  */
/* Size: ~0x48+ bytes. Vtable at 0x4782A4 (base) -> 0x478378 (full).   */
/*                                                                     */
/* Layout:                                                              */
/*   +0x00: vtable                                                     */
/*   +0x04: CRITICAL_SECTION (24 bytes on Win9x)                       */
/*     +0x04..+0x1B: CRITICAL_SECTION fields                           */
/*   +0x1C..+0x48+: state/script fields                                */
/* ================================================================== */
class ScriptEngine {
public:
    virtual ~ScriptEngine() {}

    /* ================================================================ */
    /* Fields                                                           */
    /* ================================================================ */

    /* vtable at +0x00 is compiler-managed via virtual methods */
    /* ---- Critical section (+0x04, 24 bytes on Win9x) ---- */
    uint8_t     cs[0x18];               /* +0x04  Win9x CRITICAL_SECTION storage */

    /* ---- Script state ---- */
    uint8_t     active_flag;            /* +0x1C  (was +0x22 in earlier estimate) */
    uint8_t     field_1D;               /* +0x1D */
    uint8_t     field_1E;               /* +0x1E */
    uint8_t     script_state;           /* +0x1F  (was +0x2B) — flag byte */
    int32_t     field_20;               /* +0x20  script state/type field */
    int32_t     script_flags;           /* +0x24  (was +0x30) — flags word, init = 0xB */
    int32_t     callback_ptr;           /* +0x28  (was +0x34) — callback pointer */
    int32_t     field_2C;               /* +0x2C  (was +0x38) */
    int32_t     field_30;               /* +0x30  (was +0x3C) */
    int32_t     field_34;               /* +0x34  (was +0x40) */
    int32_t     field_38;               /* +0x38  (was +0x44) */

    /* Size: ~0x3C+ bytes */

    /* ================================================================ */
    /* Methods                                                          */
    /* ================================================================ */

    /**
     * Constructor. 0x4493A0.
     * __fastcall (ECX=this).
     * The compiler installs the ScriptEngine base table (0x4782A4); initializes the critical section at +0x04.
     */
    ScriptEngine();

    /**
     * Init/reset. 0x44E8D0.
     * __fastcall (ECX=this).
     * Calls RESDATA_BaseInit; the compiler supplies the full table at 0x478378;
     * clears state bytes, sets init type to 0xB.
     */
    void __fastcall Init();

    /**
     * Reset with optional free. 0x44E910.
     * __thiscall (this, flags). Calls Call(), optionally frees.
     */
    void* __thiscall Reset(uint8_t free_memory);

    /**
     * Cleanup. 0x4493C0.
     * __thiscall (this, flags).
     * Sets vtable, deletes critical section, frees memory if flags&1.
     * Virtual slot [0] of the ScriptEngine base table at 0x4782A4.
     */
    virtual void* __thiscall Cleanup(uint8_t flags);

    /**
     * Destructor. 0x4493F0.
     * __fastcall (ECX=this). Sets vtable, deletes critical section.
     */
    void __fastcall Dtor();

    /**
     * Lock. 0x449410.
     * __fastcall (ECX=this). EnterCriticalSection wrapper. Returns 1.
     * Vtable[1] slot for RESDATA base.
     */
    virtual uint8_t __fastcall Lock();

    /**
     * Unlock. 0x449420.
     * __fastcall (ECX=this). LeaveCriticalSection wrapper. Returns 1.
     * Vtable[2] slot for RESDATA base.
     */
    virtual uint8_t __fastcall Unlock();

    /**
     * Shutdown/dispatch, full-table virtual slot [3]. Address: 0x44E930.
     * __fastcall (ECX=this). Calls Panel_DtorBody.
     */
    virtual void __fastcall Call();
};

/* ================================================================== */
/* ScriptedObjectChild struct removed (2026-08-16)                     */
/*                                                                       */
/* This was a raw partial-layout struct (no inheritance, no methods, no  */
/* vtable) duplicating the exact 0x63C-byte object at vtable 0x478358    */
/* this same comment names — confirmed zero uses anywhere in this file   */
/* or elsewhere in the tree (dead documentation, not a live cast target).*/
/* The real, fully-modeled class is `TrackTileDescriptor` (input/         */
/* TrackTileDescriptor.h/.cpp): a genuine `BuildingDescriptorEditor`      */
/* subclass with real inheritance, a real 5-slot vtable, and named        */
/* fields for every offset this struct only inventoried as `_pad_*`       */
/* (its `path_buf`/`parse_success`/`child_ptr`/`stream_status` are        */
/* `ChildWindow::bmpPath`/`loaded` — inherited, not local — and            */
/* `TrackTileDescriptor::tile_type_entries`/`tile_type` respectively).     */
/* ================================================================== */

/* ================================================================== */
/* RESDATA_ScriptedObject class — RETIRED (2026-08-16)                  */
/*                                                                       */
/* This class body (and its out-of-line method definitions in            */
/* world/scriptengine.cpp) has been removed. It was an independent,      */
/* parallel, `Status: TRANSCRIBED` reconstruction of the EXACT SAME       */
/* original class as `game/ScriptedObject.h`'s `ScriptedObject` — same    */
/* vtable (0x4782A8), same ~0x74C size, same method address list          */
/* throughout (Ctor 0x449430, Dtor 0x4494C0, Update 0x4497A0, MoveTo      */
/* 0x449DC0, HitTest 0x44A0C0, HandleToolClick 0x44A250, UpdateToolState   */
/* 0x44AC20, GetDragOffset 0x449D80, CheckClick 0x449D00, EnterBuildMode   */
/* 0x44A9D0) — that was never reconciled with it. `game/ScriptedObject.h` */
/* was kept as canonical because it already modeled real `Panel`          */
/* inheritance rather than duplicating GameObject/Entity/Panel fields     */
/* inline (this file's own anti-pattern, per CLAUDE.md). This file's own  */
/* `g_scripted_object` address (0x4A99E0) was also stale/wrong — the real */
/* singleton is at 0x4AA5B8 (constructed via a CRT static-init thunk near */
/* 0x45C6B0), now declared canonically at the bottom of                  */
/* `game/ScriptedObject.h`. Every real caller (core/HostMode3Bootstrap.cpp,*/
/* core/GameLoop.cpp, core/Game.cpp, world/tilemap.cpp, ui/UIPANEL.cpp,   */
/* ui/EditWindow.cpp) has been retargeted to `ScriptedObject`/            */
/* `g_scripted_object` from game/ScriptedObject.h. See that header's      */
/* class doc comment for the full trail, and PROGRESS.md's Completed      */
/* section for this reconciliation's evidence and disassembly citations   */
/* (in particular for the +0x740 mode field's state-2 meaning). */

/* ================================================================== */
/* C-linkage free functions                                            */
/* ================================================================== */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * RESDATA_GameObject_UpdateAnimation — Reset UI overlay state.
 * Address: 0x44AB80
 * Calling convention: __fastcall (ECX = this = ScriptedObject*)
 *
 * Resets UI overlay state (exit build/edit mode). Same cleanup as exit
 * branch of EnterBuildMode: stops animation/video, closes scroll panels,
 * clears build mode, resets all track-piece sprites to zoom=1.
 */
void __fastcall RESDATA_GameObject_UpdateAnimation(void* scriptedObj);

/**
 * RESDATA_Lock — EnterCriticalSection wrapper.
 * Address: 0x449410
 * Calling convention: __fastcall (ECX = ptr with CS at +0x04)
 *
 * RESDATA base vtable[1] method. Calls EnterCriticalSection on the
 * CRITICAL_SECTION at +0x04. Returns 1.
 *
 * @param ptr  Pointer to struct with CRITICAL_SECTION at offset +0x04
 * @return     Always 1
 */
uint8_t __fastcall RESDATA_Lock(void* ptr);

/**
 * RESDATA_Unlock — LeaveCriticalSection wrapper.
 * Address: 0x449420
 * Calling convention: __fastcall (ECX = ptr with CS at +0x04)
 *
 * RESDATA base vtable[2] method. Calls LeaveCriticalSection on the
 * CRITICAL_SECTION at +0x04. Returns 1.
 *
 * @param ptr  Pointer to struct with CRITICAL_SECTION at offset +0x04
 * @return     Always 1
 */
uint8_t __fastcall RESDATA_Unlock(void* ptr);

#ifdef __cplusplus
}
#endif
