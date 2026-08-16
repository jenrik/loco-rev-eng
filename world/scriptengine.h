/**
 * scriptengine.h — ScriptEngine and ScriptedObject classes
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The ScriptEngine is a utility class used by ScriptedObject for managing
 * script-like animation/callback dispatch on game entities. It inherits
 * from a RESDATA base and stores critical section, state flags, and a
 * pointer to a callback/dispatch function.
 *
 * The RESDATA_ScriptedObject is a large singleton (~0x74C bytes) that
 * manages scripted interactions on buildings/tracks in the game world.
 * It contains an inline GameObject, ScriptEngine, and UIPANEL ScrollPanel
 * sub-objects. It has an extensive vtable for event handling, animation,
 * tool mode switching, drag operations, and child object management.
 *
 * Class hierarchy:
 *   RESDATA base (vtable 0x4782A4)
 *     +-- ScriptEngine (base: RESDATA, vtable 0x478378)
 *
 *   RESDATA base (vtable 0x4782A4)
 *     +-- ScriptedObject (vtable 0x4782A8, type=10)
 *
 * Global singleton: g_scripted_object at 0x4A99E0
 *
 * CORRECTION (2026-08-16): "Child ScriptedObject (vtable 0x478358)" below,
 * and this class's own `AddChild`/`DtorChain`/`RemoveChild`/`HandleEvent`
 * members (0x44B190/0x44B200/0x44B220/0x44B290), are a mis-attribution —
 * confirmed via direct disassembly that ECX at 0x44B190 is a fresh
 * BuildingDescriptorEditor-shaped allocation, not this class's own `this`
 * (its ctor call and destructor-chain call both target
 * BuildingDescriptorEditor's real constructor/destructor body directly,
 * neither of which this class derives from). The real class is
 * `TrackTileDescriptor` (input/TrackTileDescriptor.h/.cpp — a genuine
 * `BuildingDescriptorEditor` subclass with a real 5-slot vtable). This
 * file's own duplicate `AddChild`/`DtorChain`/`RemoveChild`/`HandleEvent`
 * method bodies (world/scriptengine.cpp) are left as-is rather than
 * rewritten here — this file appears to be an independent, parallel,
 * `Status: TRANSCRIBED` reconstruction of the same class as
 * `game/ScriptedObject.h`/`.cpp` (same vtable 0x4782A8, same ~0x74C size,
 * same method address list throughout) that was never reconciled with it;
 * resolving which of the two is canonical is a separate, larger task, not
 * attempted here (see PROGRESS.md's Remaining work).
 *
 * Child ScriptedObject (vtable 0x478358) — see the correction above:
 *   Used for .dat script child objects created via AddChild.
 *   DtorChain = 0x44B200 (scalar-deleting dtor that calls RemoveChild)
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
/*                                                                        */
/* NOTE — separate, larger, NOT resolved by this pass: this file's own    */
/* `RESDATA_ScriptedObject` class (below) independently declares/defines  */
/* (world/scriptengine.cpp) `AddChild`/`DtorChain`/`RemoveChild`/          */
/* `HandleEvent` at these exact same addresses (0x44B190/0x44B200/        */
/* 0x44B220/0x44B290) as ITS OWN methods — the identical mis-attribution  */
/* this session fixed in `game/ScriptedObject.h`/`.cpp`, in a SEPARATE,   */
/* parallel, also-`Status: TRANSCRIBED` file that appears to duplicate    */
/* `game/ScriptedObject`'s entire class (same vtable 0x4782A8, same       */
/* ~0x74C size, same method addresses throughout — `Ctor`/`Dtor`/         */
/* `Update`/`MoveTo`/`HitTest`/etc.). Neither `ScriptedObject` nor         */
/* `RESDATA_ScriptedObject` appears to be constructed anywhere in the     */
/* current tree (no `new`/global instance of either found) — this looks   */
/* like two independent, competing reconstructions of the same original   */
/* class that were never reconciled, not a live conflict, but resolving   */
/* which one is canonical (and retiring/merging the other) is a separate, */
/* substantial task, out of scope for this pass — tracked in              */
/* PROGRESS.md's Remaining work. */

/* ================================================================== */
/* RESDATA_ScriptedObject class                                        */
/* ================================================================== */

/**
 * ScriptedObject singleton — manages scripted interactions on game
 * entities. Sits at 0x4A99E0 in the global data section.
 *
 * Layout (key offsets):
 *   +0x000: compiler-managed ScriptedObject vtable (0x4782A8)
 *   +0x004: type = 10
 *   +0x008: x position
 *   +0x00C: y position
 *   +0x0XX: RESDATA base fields
 *   +0x0E0: inline GameObject sub-object
 *   +0x178: inline ScriptEngine sub-object
 *   +0x260: inline UIPANEL ScrollPanel sub-object
 *   +0x740: dispatch_state (int16, mode: 0=idle, 1=in-world, 2=dragging, 3=placed)
 *   +0x744: child sprite pointer 1
 *   +0x748: child sprite pointer 2
 *   Size: ~0x74C bytes
 *
 * Key field offsets (from RESDATA_ScriptedObject_Update analysis):
 *   +0x024: drag flag (byte) — cursor tracking active
 *   +0x025..+0x026: drag offset x/y (int16)
 *   +0x028: tooltip pointer
 *   +0x034: child list head pointer
 *   +0x040: unknown struct with +0x14 strip width and +0x2E/+0x30 offsets
 *   +0x0AD: direction flag (byte, 0=left, 1=right)
 *   +0x0D0: last_tool_param (void*) — last active tool param pointer
 *   +0x0E0: GameObject sub-object
 *   +0x120: unknown struct (+0x2E/+0x30 offset into frame data)
 *   +0x168..+0x174: drag-handle rect
 *   +0x178: ScriptEngine sub-object
 *   +0x1B0: ScriptEngine width offset
 *   +0x200: ScriptEngine visible flag (byte)
 *   +0x260: UIPANEL ScrollPanel sub-object
 *   +0x298: ScrollPanel width offset
 *   +0x2E8: ScrollPanel visible flag (byte)
 *   +0x740: dispatch_state (int16)
 */

class RESDATA_ScriptedObject {
public:
    virtual ~RESDATA_ScriptedObject() {}

    /* ================================================================ */
    /* Fields (total size: ~0x74C bytes)                                */
    /* ================================================================ */

    /* vtable at +0x00 — compiler-managed */

    /* ---- Identity / position (+0x04..+0x21) ---- */
    int32_t     type;                    /* +0x04  type = 10 */
    int32_t     x;                       /* +0x08  screen X */
    int32_t     y;                       /* +0x0C  screen Y */
    /* +0x08 x/+0x0C y/+0x10 right/+0x14 bottom form a RECT-shaped bounding
     * box (matches GameObject::screen_rect's own +0x08 left/+0x0C top/
     * +0x10 right/+0x14 bottom layout exactly). RESDATA_ScriptedObject_
     * IsDragging (0x449CE0) calls GameObject_PtInRect(this, x, y) directly
     * — a non-virtual call reusing GameObject's own field offsets against
     * this object's own base address, not real inheritance — confirmed by
     * decompiling both 0x449CE0 and 0x436A10 in Ghidra. */
    int32_t     right;                  /* +0x10  bounding-box right edge */
    int32_t     bottom;                 /* +0x14  bounding-box bottom edge */
    uint8_t     _pad_18[0x0C];           /* +0x18..+0x23 */

    /* ---- Input / drag state (+0x24..+0x33) ---- */
    uint8_t     drag_flag;              /* +0x24  drag/cursor tracking active */
    uint8_t     _pad_25[3];             /* +0x25..+0x27 */
    int32_t     tooltip_state;          /* +0x28  tooltip anim state (0/1) */
    int32_t     field_2C;               /* +0x2C  flags/state */
    int32_t     field_30;               /* +0x30 */

    /* ---- Resource (+0x34..+0x53) ---- */
    uint8_t     _pad_34[0x08];          /* +0x34..+0x3B */
    int32_t     field_3C;               /* +0x3C  world height clamping ref */
    void*       resource;               /* +0x40  RESDATA resource pointer */
    uint8_t     _pad_44[0x10];          /* +0x44..+0x53 */
    int32_t     anim_index;             /* +0x54  animation index */

    /* ---- More state (+0x58..+0x9F) ---- */
    uint8_t     _pad_58[0x30];          /* +0x58..+0x87 */
    uint8_t     field_88;               /* +0x88  byte flag (0=off, 1=on) */
    uint8_t     _pad_89[3];             /* +0x89..+0x8B */
    int32_t     field_8C;               /* +0x8C */
    uint8_t     drag_state;             /* +0x90  drag active flag (byte within int32) */
    uint8_t     _pad_91[3];             /* +0x91..+0x93 */
    int32_t     drag_offset_x;          /* +0x94  world-space drag offset X */
    int32_t     drag_offset_y;          /* +0x98  world-space drag offset Y */
    uint8_t     _pad_9C[0x04];          /* +0x9C..+0x9F */
    void*       tooltip_id;             /* +0xA0  tooltip handle/ID */

    /* ---- Direction / tool state (+0xA4..+0xDF) ---- */
    uint8_t     _pad_A4[0x09];          /* +0xA4..+0xAC */
    uint8_t     direction;              /* +0xAD  direction flag (0=left, 1=right) */
    uint8_t     _pad_AE[0x22];          /* +0xAE..+0xCF */
    void*       child_list_head;        /* +0xD0  child linked list head (also last_tool_param) */
    uint8_t     _pad_D4[0x0C];          /* +0xD4..+0xDF */

    /* ---- Embedded GameObject (+0xE0..+0x166) ----
     * GameObject_BaseCtor initializes +0x00..+0x86 (87 bytes).
     * ScriptedObject repurposes the slack after construction for its own fields. */
    uint8_t     gameobject[0x87];       /* +0xE0  Embedded GameObject (Entity, 0x87 bytes) */

    /* ---- ScriptedObject fields in gameobject slack (+0x167..+0x177) ---- */
    uint8_t     field_162;              /* +0x167  byte flag (reuses Entity name[6] slot) */
    RECT        drag_handle;            /* +0x168  drag-hit target rectangle */

    /* ---- Embedded ScriptEngine (+0x178..+0x1B3) ----
     * ScriptEngine_Init initializes ~0x3C bytes. */
    uint8_t     scriptengine[0x3C];     /* +0x178  Embedded ScriptEngine (0x3C bytes) */

    /* ---- ScriptedObject fields in scriptengine slack (+0x1B4..+0x25F) ---- */
    uint8_t     _pad_1B4[0x4C];         /* +0x1B4..+0x1FF */
    uint8_t     scriptengine_visible;   /* +0x200  ScriptEngine sub-object visible flag */
    uint8_t     _pad_201[0x5F];         /* +0x201..+0x25F */

    /* ---- Embedded ScrollPanel (+0x260..+0x2E7) ---- */
    uint8_t     scrollpanel[0x88];      /* +0x260  Embedded UIPANEL ScrollPanel (0x88 bytes) */

    /* ---- ScriptedObject field past scrollpanel (+0x2E8) ---- */
    uint8_t     scrollpanel_visible;    /* +0x2E8  ScrollPanel sub-object visible flag */

    /* ---- More state (+0x2E9..+0x73F) ---- */
    uint8_t     _pad_2E9[0x347];        /* +0x2E9..+0x62F */
    void*       child_ptr;              /* +0x630  child object pointer */
    uint8_t     _pad_634[0x10C];        /* +0x634..+0x73F */

    /* ---- Dispatch state (+0x740..+0x74B) ---- */
    int16_t     dispatch_state;         /* +0x740  mode: 0=idle, 1=in-world, 2=dragging, 3=placed */
    void*       child_sprite1;          /* +0x744 */
    void*       child_sprite2;          /* +0x748 */

    /* ================================================================ */
    /* Methods                                                          */
    /* ================================================================ */

    /**
     * Constructor. 0x449430.
     * __fastcall (ECX=this).
     * Inits RESDATA base, creates GameObject(+0xE0), ScriptEngine(+0x178),
     * UIPANEL ScrollPanel(+0x260), sets vtable=0x4782A8, type=10.
     * Called from GameLoop_Setup.
     */
    void __fastcall Ctor();

    /**
     * Destructor. 0x4494C0.
     * __thiscall (this, flags).
     * Calls InitSubObjects (teardown), frees memory if flags&1.
     * Virtual slot [0] of the ScriptedObject table at 0x4782A8.
     */
    virtual void* __thiscall Dtor(uint8_t flags);

    /**
     * InitSubObjects (misnamed — actually a TEARDOWN). 0x4494E0.
     * __fastcall (ECX=this).
     * Destroys all embedded sub-objects in reverse construction order:
     * GameObject, ScriptEngine, ScrollPanel via vtable dispatch.
     * Called from scalar-deleting Dtor and init failure path.
     */
    void __fastcall InitSubObjects();

    /**
     * Shutdown. 0x4495B0.
     * __fastcall (ECX=this).
     * Lightweight shutdown called from CGWND_Cleanup. Stops sub-objects
     * and calls RESDATA_DtorBase. Object stays allocated for re-init.
     */
    void __fastcall Shutdown();

    /**
     * Start. 0x449600.
     * __fastcall (ECX=this).
     * Activates ScriptedObject: loads resources 0x2400-0x2413, inits child
     * GameObject, creates sprites, creates tooltip. Sets dispatch_state=0.
     * Returns non-zero on success.
     */
    uint32_t __fastcall Start();

    /**
     * Update. 0x4497A0.
     * __fastcall (ECX=this).
     * Per-frame update. 4-state machine (+0x740):
     *   0=idle (hover detection), 1=in-world (bounds clamping),
     *   2=dragging (cursor follow), 3=placed/active.
     * Called every frame from GameLoop_FrameUpdate.
     */
    void __fastcall Update();

    /**
     * Dispatch (Draw). 0x449C00 (RET 0x14 — 5 stack args: x1,y1,x2,y2,flag;
     * verified from the RET immediate, not the 0-arg guess this header
     * previously made).
     * __thiscall (this, x1, y1, x2, y2, flag).
     * Draws the ScriptedObject via GameObject_Draw, then conditionally
     * draws sub-objects based on active flags.
     * Vtable slot for rendering dispatch.
     */
    void __thiscall Dispatch(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                             int32_t flag);

    /**
     * IsDragging / PtInRect. 0x449CE0.
     * __thiscall (this, x, y).
     * Delegates to GameObject_PtInRect for hit-testing. Returns bool.
     */
    bool __thiscall IsDragging(int32_t x, int32_t y);

    /**
     * CheckClick. 0x449D00.
     * __thiscall (this, x, y).
     * Multi-layered click hit-test: tries own PtInRect, secondary,
     * then sub-objects.
     */
    bool __thiscall CheckClick(int32_t x, int32_t y);

    /**
     * GetDragOffset. 0x449D80.
     * __thiscall (this, x, y).
     * Tests drag-handle rect (+0x168..+0x174). Returns 1 if (x,y)
     * is inside the drag-handle region, 0 otherwise.
     */
    uint32_t __thiscall GetDragOffset(int32_t x, int32_t y);

    /**
     * MoveTo. 0x449DC0.
     * __thiscall (this, x, y).
     * Move object to (x,y) with boundary clamping. Updates child
     * sprite positions, dirty rect, cursor position, tooltip.
     */
    void __thiscall MoveTo(int32_t x, int32_t y);

    /**
     * HitTest. 0x44A0C0.
     * __thiscall (this, x, y).
     * Hit-test against world-space point. Handles drag initiation,
     * child-object hit-testing, and build-mode entry.
     */
    uint8_t __thiscall HitTest(int32_t x, int32_t y);

    /**
     * HandleToolClick. 0x44A250.
     * __thiscall (this, toolObj, x, y).
     * Central tool interaction handler. Dispatches by tool type:
     * track placement (0x2403-0x2405,0x2409-0x240A), build mode toggle
     * (0x2406), switch tools (0x2407-0x2408), placement toggle (0x240B),
     * fullscreen (0x240C), mute (0x240E). Returns non-zero on handled.
     * `toolObj` is a TrackPiece* — see world/scriptengine.cpp's definition
     * for the Ghidra evidence (TrackPiece::SetZoom call target, matching
     * +0x44/+0x48/+0x56 field offsets).
     */
    uint32_t __thiscall HandleToolClick(TrackPiece* toolObj, int32_t x, int32_t y);

    /**
     * EnterBuildMode. 0x44A9D0.
     * __thiscall (this, enable).
     * Enter/exit build mode. Enter: set dispatch_state=1, create tooltip,
     * mute audio. Exit: close scroll panel, reset track zooms, save.
     */
    void __thiscall EnterBuildMode(uint8_t enable);

    /**
     * UpdateToolState. 0x44AC20.
     * __thiscall (this, toolObj).
     * Update visual state (zoom level) of a child tool bar sprite based
     * on tool type and current game state (fullscreen, mute, scenario).
     * Called per-frame for each child via vtable dispatch.
     */
    uint32_t __thiscall UpdateToolState(void* toolObj);

    /**
     * AddChild. 0x44B190.
     * __thiscall (this, resId, strPtr).
     * Constructs a child ScriptedObject (0x63C bytes). Calls
     * INPUT_ExitGame base, sets vtable to DtorChain (0x478358),
     * loads .dat script via HandleEvent.
     * Called from ResourceManager_AddString type 3.
     */
    void* __thiscall AddChild(uint32_t resId, int32_t strPtr);

    /**
     * DtorChain. 0x44B200.
     * __thiscall (this, flags).
     * Scalar-deleting dtor for child ScriptedObjects. Calls RemoveChild,
     * frees memory if flags&1. Virtual slot [0] of the child table at 0x478358.
     */
    void* __thiscall DtorChain(uint8_t flags);

    /**
     * RemoveChild. 0x44B220.
     * __fastcall (ECX=this).
     * Destructor for child ScriptedObject. Frees child ptr at +0x630,
     * calls INPUT base destructor.
     */
    void __fastcall RemoveChild();

    /**
     * HandleEvent. 0x44B290.
     * __thiscall (this, resId, strPtr).
     * Loads and processes a .dat script resource for a ScriptedObject
     * child. Builds paths using name_string, tries RFD archive load
     * first, falls back to file I/O. Pipeline: parse commands, render
     * UI, init from stream.
     */
    void __thiscall HandleEvent(uint32_t resId, int32_t strPtr);
};

/* ================================================================== */
/* External global                                                     */
/* ================================================================== */
#define ADDR_g_scripted_object           0x004A99E0

extern RESDATA_ScriptedObject* g_scripted_object;  /* 0x4A99E0 — host-constructed singleton pointer */

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
