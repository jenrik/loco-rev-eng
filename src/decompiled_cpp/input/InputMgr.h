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
     * Reset world/entity state. Address: 0x41E100.  Virtual — the
     * binary vtable 0x4779C8 slot[3]; the cleanup thunk 0x41D310
     * (mov eax,[ecx]; jmp [eax+0x0C]) is the binary's virtual-dispatch
     * site for it.  CGWND_Cleanup (0x407ABE) calls through that thunk,
     * so its C++ call below dispatches through the vtable exactly like
     * the original, while TileMap::FullReset (0x455003) calls the body
     * directly (0x41E100) and uses the qualified call
     * InputMgr::ResetWorldState() to keep that direct shape.  The C++
     * vtable slot order is compiler-managed and host-native (no literal
     * VTBL_ writes — see AGENTS.md).
     *
     * Deselects the Game's selected object (Game::DeselectGameObject,
     * 0x411580, on g_game at 0x4854C8), clears the embedded entity
     * collection (collection vtable[6], 0x424270 — deletes every
     * element), and zeroes entity_count (+0x14) and special_count
     * (+0x18).  The legacy label "INPUT_FileDlgProc" for this function
     * is a misnomer — the body contains no file-dialog logic.
     */
    virtual void ResetWorldState();

    /* ---- Embedded entity collection (+0x04..+0x1C) ----------------- */

    /* +0x04 collection vtable (0x477798 init/dead → 0x477758 running).
     * Kept for layout documentation only; typed methods below replace all
     * vtable reads/writes in the C++ model. */
    void*    list_vtable;
    Entity** buffer;          /* +0x08 heap buffer, 10 Entity* slots
                                  (0x28 bytes on x86 = 10×4; host uses
                                  10×sizeof(Entity*)) */
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
    Entity* ListGetItem(int32_t index) const;

    /** RemoveAt — collection vtable[3], 0x4241E0.  Shift-remove at index,
     *  decrements count, returns the removed element (or nullptr when
     *  index is out of range).  Does not destroy the element. */
    Entity* ListRemoveAt(int32_t index);

    /** ClearAll — collection vtable[6], 0x424270.  Repeatedly removes
     *  the last element (RemoveElement, vtable[4] 0x4356E0) which
     *  destroys each removed element via its virtual destructor.
     *
     *  The collection holds Entity*: INPUT_GetSaveFileName (0x41DD40)
     *  dispatches each item's vtable[10] = Entity::Update (0x405C40),
     *  and ClearAll deletes each item as Entity (its virtual dtor). */
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
/* g_input_events — 0x630-byte event-list window object at 0x4A99B0     */
/*                                                                      */
/* Defined in shared/stubs_impl.cpp as a zeroed anchor (the original   */
/* is a BSS static object constructed by CRT thunk 0x45C650 -> ctor    */
/* 0x41F480, vtable family 0x4779E0/0x4779F4).  It owns the           */
/* [LoadEvents] (loader 0x41F5E0), [TimeEvents] (loader 0x41F6E0) and */
/* [EasterEggs] (0x41F7E0 / recorder 0x41F970 / 0x41F8E0) lists from  */
/* <ResDir>EE.INI and LOCO.INI; 0x41F4E0 frees them at               */
/* CGWND_Cleanup.  The typed reconstruction is deferred (persistence  */
/* milestone); the _WIN32 call sites keep the original thiscall       */
/* shapes through this anchor, and the host paths log loudly instead  */
/* of silently no-op'ing (see GameLoop.cpp step 7, CGWND_Cleanup,     */
/* ResourceManager::Init step 5).                                     */
/* ================================================================== */
extern uint8_t g_input_events[];

/* ================================================================== */
/* INPUT_* entry points (free thiscall functions in the binary)         */
/* ================================================================== */

/* ---- 0x4A99B0 event-list window entry points (the class is not     */
/* ---- reconstructed yet; the _WIN32 sites keep the original thiscall */
/* ---- shapes, the host paths are loud deferred adapters) ----------- */

/** Load the [LoadEvents] section (string 0x47E608) from the INI file
 *  built as "%s%s.ini" (0x47E61C) over the Res-dir buffer 0x4A99C8 and
 *  the caller's suffix (GameLoop_Setup pushes 0x47E29C = "ee"), keys
 *  "%03ld" (0x47E614), 1..N, adding each entry via 0x41FB20.
 *  Address: 0x41F5E0.  The legacy "INPUT_LoadConfig" label was a
 *  misnomer — the section is "LoadEvents", not input configuration. */
void INPUT_LoadEvents(void* self, const char* suffix);  /* 0x41F5E0 */

/** Free both event lists (LoadEvents head +0x08 with next at node+0x30,
 *  TimeEvents head +0x0C with next at node+0x44), destroying every
 *  entry (0x41F540/0x41F590) and freeing the node.  Address: 0x41F4E0.
 *  Called by CGWND_Cleanup (0x407AB4).  The legacy "INPUT_Shutdown"
 *  label was a misnomer — this is the event-list teardown, not an
 *  input-system shutdown. */
void INPUT_FreeEvents(void* self);                     /* 0x41F4E0 */

/** Load the [EasterEggs] section (0x47E640) from LOCO.INI with "%ld"
 *  keys.  Address: 0x41F7E0.  Deferred loud stub in InputMgr.cpp. */
void INPUT_SetKeyboard(void* self);                    /* 0x41F7E0 */

/** Record a newly collected easter egg / apply the seasonal date
 *  (reads g_easter_egg, 0x485230).  Address: 0x41F970.
 *  Deferred loud stub in InputMgr.cpp. */
void INPUT_SetMouse(void* self);                       /* 0x41F970 */

/** Ctor of the 0x630-byte event-list window class (vtable 0x4779E0;
 *  chains to UI_CreateChildWindow 0x424AF0).  Address: 0x41E570.
 *  Used by ResourceManager::AddString (0x446840) for resource types
 *  0/2 odd ids and 12/13.  "INPUT_ExitGame" is a legacy misnomer.
 *  Deferred loud stub in InputMgr.cpp. */
void* INPUT_ExitGame(void* self, int32_t resId, int32_t strPtr); /* 0x41E570 */

/** Per-frame entity tick. Address: 0x41DD40.  Iterates the embedded
 *  collection and calls Entity::Update (vtable[10], 0x405C40) on each
 *  entry.  Called every frame in game modes 3 and 9 from
 *  GameLoop_FrameUpdate (0x45C4F5).  The "GetSaveFileName" name is a
 *  legacy misnomer — the function returns nothing and generates no file
 *  name; it updates placed entities. */
void INPUT_GetSaveFileName(InputMgr* self);

/* ---- World new/load/save (implemented in InputMgr.cpp, see below) -- */

/** Start a fresh game world. Address: 0x41E120.
 *
 *  Sets g_in_build_mode (0x4FD199) = 1, plays the new-game sound
 *  (PlaySound 0x447930 with 0x5026), cleans up tooltips
 *  (UI_CleanupTooltips 0x423D00 on g_tooltip_mgr 0x4FD220), runs
 *  World_Init (0x44D9B0 on g_world 0x4A98B0) to generate fresh terrain,
 *  then — while any entity exists — enables Game screen-tracking
 *  (Game::SetScreenMode 0x411DC0, 1,1,1) and scrolls the viewport to
 *  each entity (TileMap_ScrollTo 0x455AB0) and disables tracking again
 *  (1,1,0).  The index increments on failed scrolls; the count is
 *  re-read every iteration.
 *
 *  Scroll-loop quirk (verified 0x41E181..0x41E1D0): the binary divides
 *  the re-read entity_count by 10, not the loop index — so the
 *  animated-branch condition (entity_count % 10 == 0) is CONSTANT for
 *  the whole loop: either every iteration runs the animated scroll +
 *  UI_HideTooltip 0x423D70 + TileMap_InvalidateDirtyRects 0x456150
 *  path, or none does.  (The old "every 10th entity" doc was wrong.) */
void  INPUT_NewWorld(InputMgr* self);                          /* 0x41E120 */

/** Load a saved world. Address: 0x41D320.  Returns the first
 *  INPUT_LoadSaveFile result.
 *
 *  1. LoadSaveFile(path, 1, 1).
 *  2. On success and when the name contains the current-save marker
 *     (original string 0x47E2A0 = "~curr"; the SDL host deliberately
 *     uses "curr" — see the #ifndef _WIN32 guard), record the path in
 *     the current-save global (0x4AA8F8).
 *  3. Always attempt the ".sav" companion: the backdrop window's
 *     (0x4FD3C8) save name + ".sav" via LoadSaveFile(..., 0, 0); the
 *     result is discarded.  On the host the companion's "curr.sav"
 *     marker match must not overwrite the primary load's recorded
 *     current-save name, so the host snapshots/restores the global
 *     around the companion load (the original never writes that global
 *     from LoadSaveFile — it calls UIPANEL_Hide on the backdrop
 *     window).
 *  4. When Netman (0x4FD3AC) scenario == 2: scroll to each placed
 *     player building (resource type 3 with +0x10C == 3 and
 *     +0x120 == 1) and clear the edge buildings' +0xC0 placement flag
 *     for each NETMAN edge check that succeeds (0x43DE10 up /
 *     0x43DE20 down / 0x43DDF0 right / 0x43DE00 left), restoring
 *     g_allow_building_placement (0x4FD3DC) around the block. */
char  INPUT_LoadWorld(InputMgr* self, const char* path);       /* 0x41D320 */
/** Load a save file. Address: 0x41D5C0.
 *
 *  thiscall (ECX = self); stack args verified at 0x41D5FF/0x41D6C9/
 *  0x41D780: S+4 = path (strlen'd and copied to a stack buffer),
 *  S+8 = flags (when non-zero, TileMap::FullReset 0x454FE0 runs first),
 *  S+0xC = flags2 (when zero, byte +0xC0 of each placed object is
 *  cleared).  Callers: INPUT_LoadWorld (0x41D320) calls (path,1,1) and
 *  (local-path,0,0); the function returns 1 on success, 0 on failure.
 *
 *  Reads the 0x114 header via the RESMGR primitives (ResDataSave.cpp),
 *  computes the placement offset ((player_id - saved_id)/2,
 *  (player_color - saved_color)/2 where the saved fields are the
 *  header words at +0x02/+0x04 — preview dimensions on designer saves)
 *  with FLOOR division for negative odd deltas (the binary's
 *  cltd/sub/sar idiom at 0x41D6A9..0x41D6D2, not C++ truncation),
 *  then places every 0x80-byte entity record (TileMap_FindObject
 *  0x4550C0, entity vtable[13] SetName on record+0x10, vtable[7]
 *  SetAnimState on +0x08 unless 0x852 or a building tile, dest to
 *  +0xBC, up to 5 child records at +0x1C: vtable[15] lookup,
 *  "PARTY"-name deserialize, +0x94 value) and every 0x2C-byte vehicle
 *  record (INPUT_FindObjectAt(3) building, World_LoadFromFile 0x44DC10,
 *  Vehicle::UpdatePosition 0x44D500, editors[0]->SetName on +0x20).
 *
 *  Host (#ifndef _WIN32): the file side runs exactly the RESMGR
 *  primitives above; the placement side is gated by the typed
 *  PersistenceAdapter (records carried, limitation explicit — see
 *  PersistenceAdapter.h).  Truncated/oversized/corrupt files fail
 *  explicitly instead of the original's silent skip. */
char  INPUT_LoadSaveFile(InputMgr* self, const char* path,
                         int flags, int flags2);               /* 0x41D5C0 */
/** Save the current world. Address: 0x41D9B0.
 *
 *  Builds the 0x114 header (type 8, g_player_id/g_player_color at
 *  +0x02/+0x04, entity count at +0x08 from this->entity_count (+0x14),
 *  vehicle count at +0x0C from g_world->vehicle_count (0x4A98B4),
 *  empty name from the BSS string at 0x4AA9FD), opens the output via
 *  RESMGR_LoadResourceData (0x447E30) and writes every placed entity
 *  (collection member with +0xC0 == 1) as a 0x80 record + every level
 *  table entry (0x4A98B8..0x4A98C8) as a 0x2C record — the level-table
 *  record name is a strlen+1 copy of the FIRST sub-slot's
 *  (*(obj+0x10)) +0x7C name (the name owner is sub-slot[0] at
 *  obj+0x10, 0x41DC78, not *(obj+0x20)).  When the name contains the
 *  current-save marker, the path is recorded in the current-save
 *  global (0x4AA8F8).  Returns 1 on success, 0 when the output stream
 *  cannot be opened.
 *
 *  Host (#ifndef _WIN32): the entity enumeration is fed by the typed
 *  PersistenceAdapter's recovered record set (the placement gate in
 *  INPUT_LoadSaveFile — see PersistenceAdapter.h); the preview written
 *  by RESMGR_LoadResourceData is the caller-prepared typed preview
 *  buffer (the original renders a TileMap overlay), capped at 16 MiB;
 *  the save is ATOMIC (temp + rename via host_save_commit) — every
 *  record write is checked and a failed write/commit returns 0 with no
 *  partial file, so the fresh seed never reports success without a
 *  durable curr; the current-save marker is "curr" (original
 *  "~curr") and is recorded only for a durable save; save names that
 *  escape the save directory are refused (symmetric with
 *  INPUT_LoadSaveFile). */
char  INPUT_SaveCurrentWorld(InputMgr* self, const char* name);/* 0x41D9B0 */

/* ---- Neighbour-tile offsets (packed (Y<<16)|X; real, verified) ----- */

/** Store the packed (Y<<16)|X tilemap offset for the given direction
 *  relative to the player position.  Addresses: Up 0x41D8F0 /
 *  Left 0x41D920 / Down 0x41D950 / Right 0x41D980.  Reads the 16-bit
 *  globals g_player_id (0x4AAD46) and g_player_color (0x4AAD48); used
 *  by Netman for tunnel-angle to neighbour-tile conversion.
 *
 *  ABI (preserved exactly): the binary leaves EAX = the output pointer
 *  and the original callers dereference it (0x43E252 mov (%eax),%ecx).
 *  The C++ reconstruction returns the output pointer too; Netman.cpp
 *  callers write `off = *INPUT_DirToOffset_Left(&off)` — the same
 *  dereference the binary performs at every call site. */
int32_t* INPUT_DirToOffset_Up(int32_t* output);      /* 0x41D8F0 */
int32_t* INPUT_DirToOffset_Left(int32_t* output);    /* 0x41D920 */
int32_t* INPUT_DirToOffset_Down(int32_t* output);    /* 0x41D950 */
int32_t* INPUT_DirToOffset_Right(int32_t* output);   /* 0x41D980 */

/* ---- Editor placement (INPUT_FindObjectAt 0x41E1F0 is implemented in
 * ---- InputMgr.cpp — it is the callee INPUT_LoadSaveFile's vehicle loop
 * ---- uses (0x41D864).  INPUT_PlaceObject 0x41DD80 and INPUT_RemoveObject
 * ---- 0x41DEF0 are editor-only and stay loud deferred stubs) ---------- */

void*      INPUT_PlaceObject(InputMgr* self, unsigned int resource_id); /* 0x41DD80 */
uintptr_t  INPUT_RemoveObject(InputMgr* self, void* obj,
                              unsigned int param);            /* 0x41DEF0 */
/** Find a placed object by mode. Address: 0x41E1F0.
 *
 *  Jump table at 0x41E550 on (mode+1), unsigned; modes -1..4 are table
 *  cases, everything else (mode > 4 or mode < -1) falls to the default
 *  path (0x41E498): the pick range is the 16-bit count at +0x158 of
 *  g_resmgr.GetById(mode) (0x446EA0) — a TYPED ResourceManager lookup,
 *  NOT a collection scan — and the second pass returns the pick-th
 *  entity whose resource id (+0x04) equals mode.  The host returns
 *  nullptr for a non-positive lookup (the binary checks only ==0 and
 *  would read 0x157 for a -1 error result; documented hardening). */
void*      INPUT_FindObjectAt(InputMgr* self, int mode);      /* 0x41E1F0 */
