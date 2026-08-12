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
 * INPUT_LoadSaveFile 0x41D5C0, INPUT_SaveCurrentWorld 0x41D9B0) are
 * implemented over the canonical InputMgr (see the persistence-milestone
 * notes in PROGRESS.md); the editor placement helpers (INPUT_PlaceObject
 * 0x41DD80, INPUT_RemoveObject 0x41DEF0) live here as deferred stubs that
 * log loudly and abort, replacing the previous silent no-op stubs.
 * **Correction (2026-08-12): `INPUT_PlaceObject` IS on the load path** --
 * `TileMap::FindObject` (0x4550C0, world/tilemap.cpp:2247-2330) calls it
 * directly, and `INPUT_LoadSaveFile` reaches `FindObject` while replaying
 * a save's entity records. Tracked in PROGRESS.md's tile-placement chain.
 *
 * Also implemented here: the verified neighbour-tile offset helpers
 * INPUT_DirToOffset_Up/Left/Down/Right (0x41D8F0/0x41D920/0x41D950/
 * 0x41D980, used by Netman); real implementations of 8 members of the
 * 0x4A99B0 event-list window class (INPUT_ResetLoadEventNode 0x41F540,
 * INPUT_ResetTimeEventNode 0x41F590, INPUT_LoadTimeEvents 0x41F6E0,
 * INPUT_DiscoverEasterEgg 0x41F8E0, INPUT_AddLoadEvent 0x41FB20,
 * INPUT_AddTimeEvent 0x41FBE0, INPUT_CheckScheduledEvents 0x41FF20,
 * INPUT_PeriodicTickDispatch 0x41FD00 — all Ghidra auto-generated names
 * were misnomers, verified by direct decompile; see InputMgr.h for each
 * one's evidence trail); and loud deferred stubs for the remaining
 * unreconstructed 0x4A99B0 members (INPUT_SetKeyboard 0x41F7E0,
 * INPUT_SetMouse 0x41F970) and Cursor's INPUT_SwitchToLocomotiveTab
 * (0x41A210). INPUT_ExitGame (0x41E570) was a different, unrelated
 * class's constructor — moved to input/BuildingDescriptorEditor.h/.cpp
 * as `BuildingDescriptorEditor`'s real constructor + Ctor bridge; its
 * stub here was removed accordingly.
 */

// Status: TRANSCRIBED

#include "InputMgr.h"
#include "PersistenceAdapter.h"
#include "../core/Game.h"
#include "../core/Entity.h"
#include "../core/BuildingMgrObjectGroup.h"
#include "../core/VehicleEditor.h"
#include "../game/World.h"
#include "../game/Vehicle.h"
#include "../game/Building.h"
#include "../game/GameVehicle.h"
#include "../game/ResdataGameVehicle.h"
#include "../game/TrackPos.h"
#include "../network/Netman.h"
#include "../ui/HelpPageNode.h"
#include "../world/tilemap.h"
#include "../resources/ResourceManager.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>

/* Typed save-record names used by the load/save path (host adapter). */
using loco::host::ChildRecord;
using loco::host::EntityRecord;
using loco::host::VehicleRecord;

/* RESDATA_IsBuildingTile (0x44BD30) / RESDATA_IsRoadTile (0x44BD10) are
 * canonically declared in world/tilemap.h (this file includes it):
 * uint8_t __fastcall RESDATA_IsBuildingTile(int32_t tile_obj) — the
 * binary ABI is __thiscall (ECX = resource, 0x41D7BB/0x41E35A); the
 * __fastcall annotation keeps that convention on 32-bit Windows and
 * expands to the native ABI elsewhere (compat.h). */

/* 0x4A99C8 — install/res-dir path buffer ("<data>/art-res/" on the host). */
extern char g_install_path[];

/* Host-only persistence isolation: integration runs share the immutable
 * asset tree but may redirect the host-only current-save pair (curr and
 * curr.sav) to LEGO_LOCO_SAVE_DIR. All other paths remain rooted at
 * g_install_path, matching the original asset lookup. */
#ifndef _WIN32
namespace {

bool is_host_current_save_path(const char* path)
{
    return std::strcmp(path, "curr") == 0 ||
           std::strcmp(path, "curr.sav") == 0;
}

bool build_host_resource_path(char* destination, size_t destination_size,
                              const char* path)
{
    const char* root = g_install_path;
    if (is_host_current_save_path(path)) {
        const char* save_root = std::getenv("LEGO_LOCO_SAVE_DIR");
        if (save_root != nullptr && *save_root != '\0') {
            root = save_root;
        }
    }

    const size_t root_length = std::strlen(root);
    const bool has_separator = root_length != 0 &&
        (root[root_length - 1] == '/' || root[root_length - 1] == '\\');
    const int written = std::snprintf(destination, destination_size, "%s%s%s",
                                      root, has_separator ? "" : "/", path);
    if (written < 0 || static_cast<size_t>(written) >= destination_size) {
        std::fprintf(stderr,
            "[HOST] persistence path is too long for '%s'\n", path);
        std::fflush(stderr);
        return false;
    }
    return true;
}

}  // namespace
#endif

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* operator_new(size_t size);            /* 0x465CE0 */
extern void  GLOBAL_free(void* ptr);               /* 0x465CD0 */
extern void* g_game;                               /* 0x4854C8 */
extern void* g_world;                              /* 0x4A98B0 */
extern void* g_netman;                             /* 0x4FD3AC */
extern void* g_tooltip_mgr;                        /* 0x4FD220 */
extern int32_t g_player_id;                        /* 0x4AAD46 */
extern int32_t g_in_build_mode;                    /* 0x4FD199 */
extern uint8_t g_allow_building_placement;         /* 0x4FD3DC */
extern char  g_current_save_path[0x108];           /* 0x4AA8F8 */
extern int32_t DAT_004a98b4;                       /* 0x4A98B4 — g_world vehicle count */
extern int32_t DAT_004a98b8[4];                    /* 0x4A98B8 — level-table entries */
/* g_tilemap (TileMap*), g_resmgr (ResourceManager), g_player_color and
 * g_install_path are canonically declared by tilemap.h / ResourceManager.h
 * (included above). */

/* Host stream helper (resources/ResDataSave.cpp) and placement gate. */
#ifndef _WIN32
extern size_t host_stream_bytes_remaining(void* stream);
#endif

/* UI tooltip entry points (0x423D00 / 0x423D70); host-gated because the
 * tooltip manager object is not reconstructed. */
extern void UI_CleanupTooltips(void* mgr);         /* 0x423D00 */
extern void UI_HideTooltip(void* mgr);             /* 0x423D70 */

/* Typed field accessor for documented binary offsets that have no named
 * C++ member on the shared types (used only where the original reads a
 * raw dword; each use carries a precise comment). */
template <typename T>
T& field_at(void* object, size_t offset)
{
    return *reinterpret_cast<T*>(
        reinterpret_cast<uint8_t*>(object) + offset);
}

template <typename T>
const T& field_at(const void* object, size_t offset)
{
    return *reinterpret_cast<const T*>(
        reinterpret_cast<const uint8_t*>(object) + offset);
}

/* ================================================================== */
/* trunc_div2 — the binary's signed division-by-2 idiom                */
/*                                                                      */
/* INPUT_LoadSaveFile (0x41D5C0) computes the placement offset         */
/* ((player - saved)/2) with the MSVC signed divide-by-2 idiom         */
/* cltd/sub/sar (0x41D6A9..0x41D6D2):                                 */
/*     eax = delta; cltd; sub %edx,%eax; sar $1                        */
/* which is (delta - sign(delta)) >> 1 — TRUNCATION TOWARD ZERO       */
/* (e.g. -3/2 = -1), exactly C++ integer division.  (The earlier       */
/* "floor division" reading of this idiom was wrong: for delta = -3   */
/* the sequence yields -1, not -2 — verified against the raw bytes.    */
/* The saved header words are read as 16-bit UNSIGNED — and $0xffff   */
/* at 0x41D6A1/0x41D6B5 — while the player globals are sign-extended  */
/* 16-bit loads (movswl 0x4aad46/0x4aad48); the caller below mirrors   */
/* both widths.  Deltas are 16-bit differences, so no overflow is      */
/* possible.                                                          */
static int32_t trunc_div2(int32_t v)
{
    return v / 2;
}

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
     * ten-slot capacity with native pointer width (10 * sizeof(Entity*)),
     * which is byte-identical on 32-bit and a safe native layout on
     * 64-bit hosts.  The collection holds Entity* — INPUT_GetSaveFileName
     * (0x41DD40) dispatches vtable[10] = Entity::Update (0x405C40) on
     * each entry. */
    const size_t slot_bytes = 10 * sizeof(Entity*);
    Entity** items = static_cast<Entity**>(operator_new(slot_bytes));
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
Entity* InputMgr::ListGetItem(int32_t index) const
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
Entity* InputMgr::ListRemoveAt(int32_t index)
{
    Entity* element = this->ListGetItem(index);
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
        Entity* element = this->ListRemoveAt(this->count - 1);
        if (element != nullptr) {
            delete element;     /* RemoveElement vtable[4] 0x4356E0:
                                   destroys via the virtual dtor */
        }
    }
}

/* ================================================================== */
/* ResetWorldState — vtable[3] (cleanup thunk 0x41D310 target)         */
/* Address: 0x41E100                                                   */
/* Virtual in the C++ model (binary vtable 0x4779C8 slot[3]);          */
/* CGWND_Cleanup (0x407ABE) dispatches through the 0x41D310 thunk      */
/* (a virtual call in C++), while TileMap::FullReset (0x455003) calls  */
/* the body directly (0x41E100 — qualified InputMgr::ResetWorldState   */
/* at the call site).                                                  */
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
        Entity* item = self->ListGetItem(index);
        if (item != nullptr) {
            item->Update();     /* vtable[10] 0x405C40 */
        }
        index++;
    }
}

/* ================================================================== */
/* INPUT_DirToOffset_* — neighbour-tile offsets (packed (Y<<16)|X)     */
/* Addresses: Up 0x41D8F0 / Left 0x41D920 / Down 0x41D950 /           */
/*            Right 0x41D980                                           */
/*                                                                      */
/* Compute the tilemap offset for a direction relative to the current  */
/* player position and store the packed value (Y << 16) | X in         */
/* *output (X/Y are 16-bit signed).  The originals load the 16-bit     */
/* globals g_player_id (0x4AAD46) and g_player_color (0x4AAD48); the   */
/* host versions truncate the 32-bit globals to 16 bits exactly like   */
/* the x86 loads.  Used by Netman (0x43E2E0/0x43E500/0x43F140) for     */
/* tunnel-angle to neighbour-tile conversion.                          */
/*                                                                      */
/* ABI (preserved exactly): the binary leaves EAX = the OUTPUT POINTER */
/* and the original callers dereference it (0x43E252 mov (%eax),%ecx;  */
/* 0x43E27C mov (%eax),%eax).  The C++ reconstruction returns the      */
/* output pointer too; Netman.cpp callers write `off =                 */
/* *INPUT_DirToOffset_Left(&off)` — the same dereference the binary    */
/* performs at every call site (no decompiler-intent value ABI).       */
/* ================================================================== */
int32_t* INPUT_DirToOffset_Up(int32_t* output)      /* 0x41D8F0 */
{
    const int16_t id = static_cast<int16_t>(g_player_id);
    const int16_t color = static_cast<int16_t>(g_player_color);
    const uint16_t x = static_cast<uint16_t>(id - 3);             /* sub $0x3, %ax */
    const uint16_t y = static_cast<uint16_t>((color >> 1) - 1);   /* sar $1, %cx; dec */
    *output = (static_cast<int32_t>(y) << 16) | static_cast<int32_t>(x);
    return output;      /* EAX = the output pointer (0x41D914) */
}

int32_t* INPUT_DirToOffset_Left(int32_t* output)    /* 0x41D920 */
{
    const int16_t color = static_cast<int16_t>(g_player_color);
    const uint16_t y = static_cast<uint16_t>((color >> 1) - 1);   /* sar $1, %ax; dec */
    *output = static_cast<int32_t>(y) << 16;                      /* X = 0 */
    return output;      /* EAX = the output pointer (0x41D937) */
}

int32_t* INPUT_DirToOffset_Down(int32_t* output)    /* 0x41D950 */
{
    const int16_t id = static_cast<int16_t>(g_player_id);
    const int16_t color = static_cast<int16_t>(g_player_color);
    const uint16_t x = static_cast<uint16_t>((id >> 1) - 1);      /* sar $1, %ax; dec */
    const uint16_t y = static_cast<uint16_t>(color - 2);          /* add $0xFFFFFFFE, %ecx */
    *output = (static_cast<int32_t>(y) << 16) | static_cast<int32_t>(x);
    return output;      /* EAX = the output pointer (0x41D969) */
}

int32_t* INPUT_DirToOffset_Right(int32_t* output)   /* 0x41D980 */
{
    const int16_t id = static_cast<int16_t>(g_player_id);
    const uint16_t x = static_cast<uint16_t>((id >> 1) - 1);      /* sar $1, %ax; dec */
    *output = static_cast<int32_t>(x);                            /* Y = 0 */
    return output;      /* EAX = the output pointer (0x41D997) */
}

/* ================================================================== */
/* World new/load/save and editor placement (this milestone)           */
/*                                                                      */
/* Addresses (all verified with objdump on the shipped PE):            */
/*   INPUT_NewWorld        0x41E120                                    */
/*   INPUT_LoadWorld       0x41D320                                    */
/*   INPUT_LoadSaveFile    0x41D5C0                                    */
/*   INPUT_SaveCurrentWorld 0x41D9B0                                   */
/*   INPUT_PlaceObject     0x41DD80                                    */
/*   INPUT_FindObjectAt    0x41E1F0                                    */
/*                                                                      */
/* INPUT_RemoveObject (0x41DEF0) stays a loud deferred stub — it is    */
/* editor-only (not on the load/save path) and its caller graph is a   */
/* later milestone.                                                    */
/* ================================================================== */

/* ================================================================== */
/* Host placement capability                                           */
/*                                                                      */
/* On the SDL host the resource objects ResourceManager_GetById returns */
/* (loco::assets::SpriteResource) do not carry the original x86 RESDATA */
/* layout that the typed entity constructors read (object type +0x08,  */
/* animation table +0x20, member limit +0x522, tile-state byte +0x63A, */
/* ...), so invoking them would be out-of-bounds reads.  The host      */
/* therefore gates in-world placement behind this flag (default false) */
/* and carries save records instead — see PersistenceAdapter.h for the */
/* explicit limitation and the milestone that removes it.  Component   */
/* tests that supply proper-layout resource objects set it to true.    */
/* ================================================================== */
#ifndef _WIN32
static bool s_host_placement_available = false;
bool loco::host::host_placement_available() { return s_host_placement_available; }
void loco::host::set_host_placement_available(bool available)
{
    s_host_placement_available = available;
}
#endif

/* ================================================================== */
/* Typed dual-view helpers                                             */
/*                                                                      */
/* The placed objects are simultaneously Entity* (InputMgr collection,  */
/* Entity::SetName/SetAnimState dispatch) and TileMapObject* (the       */
/* tilemap grid's placement view, is_moving at +0xC0).  The original    */
/* passes the same pointer through both roles; these helpers document   */
/* the cross-cast at the two call boundaries (same pattern as           */
/* TileMap::FindObject / World.cpp).                                   */
/* ================================================================== */

/* Bounded "PARTY" name probe for child records.  The binary scans the
 * 12-byte name field with CRT_wcsstr (0x471480, NUL-terminated) at
 * 0x41D7FC; a malformed record can carry a non-NUL-terminated field, so
 * the host probe is bounded to the field and honours the first NUL
 * (host NUL-safety — the binary would keep scanning past the field). */
static bool child_name_has_party(const char name[12])
{
#ifndef _WIN32
    size_t len = 0;
    while (len < 12 && name[len] != '\0') {
        len++;
    }
    static const char kParty[5] = {'P', 'A', 'R', 'T', 'Y'};
    for (size_t i = 0; i + 5 <= len; i++) {
        if (std::memcmp(name + i, kParty, 5) == 0) {
            return true;
        }
    }
    return false;
#else
    /* Original (0x41D7FC..0x41D80A): CRT_wcsstr(name, "PARTY"
     * 0x47E4FC) — NUL-terminated substring search on the name field. */
    return std::strstr(name, "PARTY") != nullptr;
#endif
}

static Entity* as_entity(TileMapObject* obj)
{
    return reinterpret_cast<Entity*>(reinterpret_cast<void*>(obj));
}

static TileMapObject* as_tilemap_object(Entity* entity)
{
    return reinterpret_cast<TileMapObject*>(reinterpret_cast<void*>(entity));
}

/* ================================================================== */
/* ListResize/ListInsert (collection vtable[0] 0x435D10 / vtable[13]   */
/* 0x412440) are NOT reconstructed in this milestone — the only        */
/* binary caller is INPUT_PlaceObject (0x41DD80), which is editor-only */
/* and stays a loud deferred stub below.                              */
/* ================================================================== */

/* ================================================================== */
/* INPUT_NewWorld                                                     */
/* Address: 0x41E120                                                   */
/*                                                                      */
/* See InputMgr.h for the full flow.  Host deviations (#ifndef         */
/* _WIN32): the tooltip manager (g_tooltip_mgr 0x4FD220) is not        */
/* reconstructed, so UI_CleanupTooltips (0x423D00) and UI_HideTooltip  */
/* (0x423D70) are loud no-ops; g_tilemap is constructed by             */
/* BootstrapMode3Core before the loading worker runs, but component    */
/* tests may leave it null (then the scroll block is a loud no-op).    */
/*                                                                      */
/* Scroll-loop quirk (verified at 0x41E181..0x41E1D0): the binary      */
/* divides the RE-READ entity_count by 10 (div %ebp after             */
/* mov 0x14(%edi),%eax) — NOT the loop index — so the animated-branch */
/* condition (entity_count % 10 == 0) is CONSTANT for the whole loop: */
/* either every iteration uses the animated path (tooltip hide +      */
/* dirty-rect invalidation) or none does.  The old "every 10th entity" */
/* comment was wrong.  The index increments on failed scrolls; the    */
/* count is re-read every iteration. */
/* ================================================================== */
void INPUT_NewWorld(InputMgr* self)
{
    /* g_in_build_mode (0x4FD199) = 1 */
    g_in_build_mode = 1;

    /* PlaySound(0x5026) — new-game jingle (PlaySound 0x447930 on
     * g_resmgr). */
#ifdef _WIN32
    PlaySound(0x5026);
#else
    /* Host: the sound-resource loading chain behind PlaySound
     * (0x448990 RESMGR_AllocResourceEntry -> RESMGR_OpenResourceFile)
     * is not reconstructed (see PROGRESS: "Complete ResourceManager
     * consumers"); invoking the real PlaySound would run the empty
     * host stub and crash in create_string_resource.  The jingle is
     * logged loudly instead (documented deviation). */
    std::fprintf(stderr,
        "[HOST] INPUT_NewWorld: new-game jingle 0x5026 skipped "
        "(PlaySound 0x447930 sound-loading chain not reconstructed)\n");
    std::fflush(stderr);
#endif

#ifndef _WIN32
    if (g_tooltip_mgr != nullptr) {
        UI_CleanupTooltips(g_tooltip_mgr);       /* 0x423D00 */
    } else {
        std::fprintf(stderr,
            "[HOST] INPUT_NewWorld: tooltip cleanup skipped "
            "(g_tooltip_mgr 0x4FD220 not reconstructed)\n");
        std::fflush(stderr);
    }
#else
    UI_CleanupTooltips(g_tooltip_mgr);           /* 0x423D00 */
#endif

    /* World_Init (0x44D9B0) — fresh terrain. */
#ifndef _WIN32
    if (g_world == nullptr) {
        std::fprintf(stderr, "[HOST] INPUT_NewWorld: World_Init skipped (g_world null)\n");
        std::fflush(stderr);
        return;
    }
#endif
    static_cast<World*>(g_world)->Init();

    /* Scroll the viewport to every entity (every 10th uses animated   */
    /* scroll + tooltip hide + dirty-rect invalidation).  The index    */
    /* increments on failed scrolls; the count is re-read each pass.   */
    if (self->entity_count == 0) {
        return;
    }
#ifndef _WIN32
    if (g_game == nullptr || g_tilemap == nullptr) {
        std::fprintf(stderr,
            "[HOST] INPUT_NewWorld: viewport scroll skipped "
            "(g_game/g_tilemap not constructed)\n");
        std::fflush(stderr);
        return;
    }
#endif

    static_cast<Game*>(g_game)->SetScreenMode(1, 1, 1);  /* 0x411DC0 */
    int32_t index = 0;
    while (index < self->entity_count) {
        Entity* entity = self->ListGetItem(index);
        if (self->entity_count % 10 == 0) {
            if (static_cast<TileMap*>(g_tilemap)->ScrollTo(
                    as_tilemap_object(entity), 1) == nullptr) {
                index++;
            }
#ifndef _WIN32
            if (g_tooltip_mgr != nullptr) {
                UI_HideTooltip(g_tooltip_mgr);      /* 0x423D70 */
            }
#else
            UI_HideTooltip(g_tooltip_mgr);          /* 0x423D70 */
#endif
            static_cast<TileMap*>(g_tilemap)->InvalidateDirtyRects(0); /* 0x456150 */
        } else {
            if (static_cast<TileMap*>(g_tilemap)->ScrollTo(
                    as_tilemap_object(entity), 0) == nullptr) {
                index++;
            }
        }
    }
    static_cast<Game*>(g_game)->SetScreenMode(1, 1, 0);  /* 0x411DC0 */
}

/* ================================================================== */
/* INPUT_PlaceObject (editor-only, deferred; loud)                    */
/* Address: 0x41DD80                                                   */
/*                                                                      */
/* Creates a typed placed object for a resource id and registers it in */
/* the collection.  Not on the load/save path (INPUT_NewWorld/Load/    */
/* Save never call it — the persistence path finds existing objects    */
/* through TileMap_FindObject and INPUT_FindObjectAt); it is editor-   */
/* only and stays a loud deferred stub until the editor milestone.    */
/* ================================================================== */

void* INPUT_PlaceObject(InputMgr* self, unsigned int resource_id)  /* 0x41DD80 */
{
    (void)self;
    (void)resource_id;
    std::fprintf(stderr,
        "[InputMgr] INPUT_PlaceObject (0x41DD80) is a deferred stub: "
        "editor-only, not on the load/save path\n");
    std::fflush(stderr);
    std::abort();
}

/* ================================================================== */
/* INPUT_FindObjectAt                                                 */
/* Address: 0x41E1F0                                                   */
/*                                                                      */
/* Jump table at 0x41E550 on (mode+1), unsigned — modes -1..4 are table
 * cases, everything else (mode > 4 or mode < -1) falls to the DEFAULT
 * path (0x41E498).  Modes:                                          */
/*   -1: random entity: GetItem(rand() % entity_count)                */
/*   0/1/4: random entity with resource type 3, +0x10C == 3 and       */
/*          (+0x120 == mode || mode == 4)                             */
/*   2:   random entity with resource +0x62C byte != 0 (special);     */
/*        the pick range is special_count (+0x18), not a first-pass   */
/*        count (there is no first pass in the binary)                */
/*   3:   random entity with resource type 3 + IsBuildingTile         */
/*   default: the pick range is the 16-bit count at resource+0x158 of */
/*        g_resmgr.GetById(mode) (typed ResourceManager lookup,       */
/*        0x446EA0 — NOT a collection scan) and the second pass       */
/*        returns the pick-th entity whose resource id (+0x04) ==     */
/*        mode                                                       */
/* Each pick is rand() % range + 1 (0x41E29C) and the second pass     */
/* returns the pick-th match (match counter checked at the top of     */
/* every scan step, 0x41E2D5); if the scan ends first, the last       */
/* match is returned.  rand is CRT_rand (0x466150).                  */
/* ================================================================== */

namespace {

/* +0x10C / +0x120 — documented on the ResourceGameObject family:
 * RESDATA_GameVehicle::vehicle_kind (+0x10C), HelpPageNode::overlay_flag
 * (+0x120) and GameVehicle::current_vehicle (+0x120).  The original
 * reads the raw dwords on every collection entity and compares them to
 * the mode int (0x41E268/0x41E27A: cmp [eax+0x10c],3; cmp [eax+0x120],
 * ecx).  Base RESDATA_GameVehicle is 0x11C bytes, so +0x120 is past the
 * object there — the typed model returns false instead of reading OOB
 * (documented deviation; no collection entity of that base class exists
 * on the placement path). */
static int32_t entity_kind(Entity* e)
{
#ifndef _WIN32
    /* Host: typed dynamic_cast (the original reads the raw +0x10C dword
     * on every entity — see the _WIN32 branch). */
    if (RESDATA_GameVehicle* rv = dynamic_cast<RESDATA_GameVehicle*>(e)) {
        return rv->vehicle_kind;                     /* +0x10C */
    }
    return -1;
#else
    /* Original (0x41E268): raw dword read on the entity. */
    return field_at<int32_t>(e, 0x10C);
#endif
}

static int32_t entity_mode_flag(Entity* e, int32_t mode)
{
#ifndef _WIN32
    /* Host: typed dynamic_cast (the original reads the raw +0x120 dword
     * — see the _WIN32 branch). */
    if (HelpPageNode* h = dynamic_cast<HelpPageNode*>(e)) {
        return h->overlay_flag;                      /* +0x120 */
    }
    if (GameVehicle* g = dynamic_cast<GameVehicle*>(e)) {
        /* The binary compares the raw +0x120 dword (a pointer for
         * GameVehicle) to the mode int — mode 0 matches only nullptr. */
        return (reinterpret_cast<uintptr_t>(g->current_vehicle) ==
                static_cast<uintptr_t>(mode)) ? mode : -1;
    }
    return -1;
#else
    /* Original (0x41E27A): the raw +0x120 dword compared to the mode
     * int. */
    int32_t value = field_at<int32_t>(e, 0x120);
    return (value == mode) ? mode : -1;
#endif
}

bool entity_matches(Entity* e, int32_t mode)
{
#ifndef _WIN32
    /* Host hardening: the binary dereferences the item unconditionally
     * (0x41E255/0x41E2E5 GetItem result is used directly). */
    if (e == nullptr) {
        return false;
    }
#endif
    uint8_t type = 0;
    /* The +0x08 type read IS null-guarded in the binary (0x41E258..
     * 0x41E260: test %ecx,%ecx; xor %cl,%cl), so the resource null
     * guard below is not a deviation — it is the original flow. */
    if (e->resource != nullptr) {
        type = *reinterpret_cast<uint8_t*>(static_cast<uint8_t*>(e->resource) + 0x08);
    }
    switch (mode) {
    case 0:
    case 1:
    case 4:
        if (type != 3) return false;
        if (entity_kind(e) != 3) return false;      /* +0x10C == 3 */
        if (mode != 4 && entity_mode_flag(e, mode) != mode) return false;
        return true;
    case 2:
#ifndef _WIN32
        /* Host hardening: the binary reads +0x62C without a null check
         * on the resource pointer (0x41E46E..0x41E471). */
        if (e->resource == nullptr) return false;
#endif
        return *reinterpret_cast<uint8_t*>(
                   static_cast<uint8_t*>(e->resource) + 0x62C) != 0;
    case 3:
        if (type != 3) return false;
        return RESDATA_IsBuildingTile(static_cast<int32_t>(
            reinterpret_cast<intptr_t>(e->resource))) != 0;
    default:
#ifndef _WIN32
        /* Host hardening: the binary compares resource+0x04 with the
         * mode without a null check on the resource pointer
         * (0x41E526..0x41E52D). */
        if (e->resource == nullptr) return false;
#endif
        return field_at<int32_t>(e->resource, 0x04) == mode;
    }
}

/* CRT_rand (0x466150) — declared in Netman.h (included above). */

/* Second pass: scan the collection and return the pick-th match.     */
/* The binary checks the match counter at the top of each scan step   */
/* (0x41E2D5) and returns the candidate stored by the previous step,  */
/* so the pick-th match is returned; if the collection ends first the */
/* last match is returned (0x41E323).                                 */
Entity* find_pick(InputMgr* self, int32_t mode, int32_t pick)
{
    Entity* candidate = nullptr;
    int32_t found = 0;
    const int32_t count = self->ListGetCount();
    int32_t index = 0;
    while (index < count) {
        if (found == pick) {
            return candidate;
        }
        Entity* e = self->ListGetItem(index);
        if (entity_matches(e, mode)) {
            found++;
            candidate = e;
        }
        index++;
    }
    return candidate;
}

}  // namespace

void* INPUT_FindObjectAt(InputMgr* self, int mode)
{
    /* ---- mode -1: random entity (jump-table[0], 0x41E214) -------- */
    if (mode == -1) {
        if (self->entity_count == 0) {
            return nullptr;
        }
        return self->ListGetItem(CRT_rand() % self->entity_count);
    }

    /* ---- mode 2: special sub-count pick (0x41E404) ---------------- */
    if (mode == 2) {
        /* Mode 2 (0x41E404): the pick range is the special sub-count
         * (+0x18, 0x41E404..0x41E445) — the binary has no first pass
         * for this mode. */
        if (self->special_count == 0) {
            return nullptr;
        }
        int32_t pick = CRT_rand() % self->special_count + 1;
        return find_pick(self, mode, pick);
    }

    /* ---- default (jump-table default, 0x41E498): modes > 4 and     */
    /* ---- mode < -1 (the unsigned (mode+1) > 5 test at 0x41E204)  -- */
    if (mode < -1 || mode > 4) {
        /* The pick range is the 16-bit count at resource+0x158 of
         * g_resmgr.GetById(mode) — a TYPED ResourceManager lookup
         * (0x446EA0), NOT a collection scan.  The binary checks only
         * == 0 (0x41E4A5 cmp %ebx,%ebp) and reads +0x158; a -1 (error)
         * lookup would read 0x157 and fault. */
        const int32_t res = g_resmgr.GetById(mode);      /* 0x446EA0 */
#ifndef _WIN32
        /* Host hardening: return nullptr for a non-positive lookup
         * (documented deviation — the binary would fault on -1). */
        if (res <= 0) {
            return nullptr;
        }
#else
        /* Original (0x41E4A5): res == 0 only. */
        if (res == 0) {
            return nullptr;
        }
#endif
        void* resource = reinterpret_cast<void*>(
            static_cast<uintptr_t>(static_cast<int32_t>(res)));
        const uint16_t range = field_at<uint16_t>(resource, 0x158);
        if (range == 0) {
            return nullptr;
        }
        int32_t pick = CRT_rand() % range + 1;
        return find_pick(self, mode, pick);
    }

    /* ---- modes 0/1/3/4: first-pass collection scan (0x41E23C/      */
    /* ---- 0x41E32F) ------------------------------------------------ */
    int32_t matches = 0;
    const int32_t count = self->ListGetCount();
    for (int32_t i = 0; i < count; i++) {
        if (entity_matches(self->ListGetItem(i), mode)) {
            matches++;
        }
    }
    if (matches == 0) {
        return nullptr;
    }

    /* ---- random pick: rand() % range + 1 (0x41E29C/0x41E385).
     * The binary also carries an unreachable branch (0x41E2A9, entered
     * only when the range < 1, which cannot happen here) that would pick
     * the fixed second match for a range of 2; it is dead code. */
    int32_t pick = CRT_rand() % matches + 1;
    return find_pick(self, mode, pick);
}

/* ================================================================== */
/* INPUT_LoadSaveFile                                                 */
/* Address: 0x41D5C0                                                   */
/*                                                                      */
/* See InputMgr.h.  Host deviations (#ifndef _WIN32):                 */
/*   - the "curr" backdrop window call (0x429EF0 on the 0x4AA818      */
/*     backdrop window) records the save name in the host current-save */
/*     global instead (the backdrop window object is not reconstructed); */
/*   - placement is gated (PersistenceAdapter): records are carried    */
/*     into the adapter document instead of placed; the placement      */
/*     block below runs only when loco::host::host_placement_available */
/*     is true (tests provide proper-layout resources);               */
/*   - a truncated/oversized file fails explicitly (the original       */
/*     silently skips short records).                                 */
/* ================================================================== */
char INPUT_LoadSaveFile(InputMgr* self, const char* path, int flags, int flags2)
{
#ifndef _WIN32
    /* Host hardening: reject save names that would escape the save
     * directory (the original concatenates caller strings verbatim). */
    if (loco::host::PersistenceAdapter::name_escapes(path)) {
        std::fprintf(stderr,
            "[HOST] INPUT_LoadSaveFile: refused path '%s' (escape)\n", path);
        std::fflush(stderr);
        return 0;
    }
#endif

    RESDATA resdata;
    RESMGR_ResourceData_Init(&resdata);            /* 0x447B20 */

    /* Build "<resdir><path>" (0x4A99C8 buffer + name) exactly like the
     * original's rep movs sequence. */
    char path_buf[0x108];
#ifndef _WIN32
    if (!build_host_resource_path(path_buf, sizeof(path_buf), path)) {
        RESMGR_RemoveResource(&resdata);
        RESMGR_ReleaseResource(&resdata);
        return 0;
    }
#else
    std::snprintf(path_buf, sizeof(path_buf), "%s%s", g_install_path, path);
#endif

    /* Open + read header + preview (0x447BA0). */
    if (RESMGR_LoadResource(&resdata, path_buf) == 0) {
        RESMGR_RemoveResource(&resdata);
        RESMGR_ReleaseResource(&resdata);
        return 0;
    }
    if (!RESMGR_IsSaveHeader(&resdata)) {          /* 0x448030 */
        RESMGR_RemoveResource(&resdata);
        RESMGR_ReleaseResource(&resdata);
        return 0;
    }

    /* Placement offset: ((player - saved)/2, (color - saved)/2) with
     * the saved fields read as the 16-bit header words at +0x02/+0x04
     * (preview dimensions on designer saves).  The binary computes this
     * with the cltd/sub/sar idiom (0x41D693..0x41D6D2): the player
     * globals are SIGN-EXTENDED 16-bit loads (movswl 0x4aad46/0x4aad48
     * at 0x41D69A/0x41D6BB) and the saved words are masked to 16-bit
     * UNSIGNED (and $0xffff at 0x41D6A1/0x41D6B5) before the
     * subtraction; the idiom itself is TRUNCATION TOWARD ZERO
     * (cltd/sub/sar — for a negative odd delta, -3/2 = -1, NOT floor
     * division -2), exactly C++ integer division.  trunc_div2
     * reproduces the exact x86 semantics with both operand widths. */
    int32_t offset_x = trunc_div2(static_cast<int16_t>(g_player_id) -
                                  static_cast<int32_t>(resdata.save.player_id));
    int32_t offset_y = trunc_div2(static_cast<int16_t>(g_player_color) -
                                  static_cast<int32_t>(resdata.save.player_color));

    /* flags != 0: TileMap::FullReset (0x454FE0) first. */
    if (flags != 0) {
        if (g_tilemap != nullptr) {
            static_cast<TileMap*>(g_tilemap)->FullReset();
        } else {
            std::fprintf(stderr,
                "[HOST] INPUT_LoadSaveFile: FullReset skipped (g_tilemap null)\n");
            std::fflush(stderr);
        }
    }

    /* "curr" marker: original string 0x47E2A0 is "~curr"; the SDL host
     * deliberately uses "curr" (documented host deviation — see
     * InputMgr.h and PROGRESS.md).  The original then feeds the header
     * name to the 0x4AA818 panel via UIPANEL_Hide (0x429EF0 — the
     * GameLoop.cpp-documented name for that slot); the host records the
     * name in the current-save global instead. */
#ifndef _WIN32
    const char* curr_marker = "curr";
#else
    const char* curr_marker = "~curr";
#endif
    if (std::strstr(path, curr_marker) != nullptr) {
#ifndef _WIN32
        std::snprintf(g_current_save_path, sizeof(g_current_save_path), "%s", path);
#else
        extern void UIPANEL_Hide(void* panel, void* str);  /* 0x429EF0 */
        UIPANEL_Hide(reinterpret_cast<void*>(0x4AA818),
                     resdata.save.name);
#endif
    }

    /* Entity loop. */
    const char saved_placement = static_cast<char>(g_allow_building_placement);
    g_allow_building_placement = 1;

    int32_t index = 0;
    const int32_t entity_count = static_cast<int32_t>(resdata.save.entity_count);
    const uint16_t vehicle_count = resdata.save.vehicle_count;
    bool truncation = false;

#ifndef _WIN32
    /* Host: validate the declared record layout against the stream so a
     * corrupt entity/vehicle count cannot spin the loops (host
     * hardening; the original trusts the header). */
    {
        size_t remaining = host_stream_bytes_remaining(resdata.primary_stream);
        size_t expected = static_cast<size_t>(entity_count) * 0x80u +
                          static_cast<size_t>(vehicle_count) * 0x2Cu;
        if (remaining < expected) {
            truncation = true;
        }
    }
#endif

    while (!truncation && index < entity_count) {
        /* One 0x80 record (0x447DB0). */
        EntityRecord* record = static_cast<EntityRecord*>(
            RESMGR_LockResource(&resdata));
        if (record == nullptr) {
#ifndef _WIN32
            /* Short read before entity_count records: explicit failure
             * (host hardening — the original skips silently). */
            truncation = true;
            break;
#else
            /* Original: the short record is skipped and the loop
             * continues to entity_count iterations (0x41D776). */
            index++;
            continue;
#endif
        }

#ifndef _WIN32
        if (!loco::host::host_placement_available()) {
            /* Placement gate closed: carry the typed record. */
            loco::host::PersistenceAdapter::instance().document().entities.push_back(*record);
            index++;
            continue;
        }
#endif

        /* ---- Original placement path ---- */
        Entity* entity = nullptr;
#ifndef _WIN32
        if (g_tilemap == nullptr) {
            std::fprintf(stderr,
                "[HOST] INPUT_LoadSaveFile: placement block skipped "
                "(g_tilemap null)\n");
            std::fflush(stderr);
            index++;
            continue;
        }
#endif
        {
            int* found = TileMap_FindObject(
                static_cast<TileMap*>(g_tilemap),
                static_cast<unsigned int>(record->resource_id),
                static_cast<short>(static_cast<int>(record->x) + offset_x),
                static_cast<short>(static_cast<int>(record->y) + offset_y),
                1, 1);
            entity = as_entity(reinterpret_cast<TileMapObject*>(found));
        }
        if (entity == nullptr) {
            index++;
            continue;
        }

        if (flags2 == 0) {
            as_tilemap_object(entity)->is_moving = 0;   /* +0xC0 */
        }

        /* vtable[13]: SetName (0x405E20) / Building::SetCustomName
         * (0x4344A0) with the record name at +0x10. */
        entity->SetName(record->name);

        /* vtable[7] SetAnimState (+0x08) unless the record is 0x852 or
         * a building tile. */
        if (record->resource_id != 0x852) {
            uint8_t res_type = static_cast<uint8_t>(
                GetResourceType(record->resource_id));
            bool building_tile = false;
#ifndef _WIN32
            if (res_type == 3 && entity->resource != nullptr) {
                building_tile = RESDATA_IsBuildingTile(
                    static_cast<int32_t>(
                        reinterpret_cast<intptr_t>(entity->resource))) != 0;
            }
#else
            if (res_type == 3) {
                building_tile = RESDATA_IsBuildingTile(
                    static_cast<int32_t>(
                        reinterpret_cast<intptr_t>(entity->resource))) != 0;
            }
#endif
            if (res_type != 3 || !building_tile) {
                entity->SetAnimState(static_cast<int>(record->anim_state));
            }
        }

        /* dest -> +0xBC (typed field; Building::track_y / ResourceGameObject::field_bc). */
#ifndef _WIN32
        if (Building* b = dynamic_cast<Building*>(entity)) {
            b->track_y = static_cast<int32_t>(record->dest);
        } else if (ResourceGameObject* r = dynamic_cast<ResourceGameObject*>(entity)) {
            r->field_bc = static_cast<int32_t>(record->dest);
        }
#else
        /* Original (0x41D7CF..0x41D7D5): the +0xBC dword is written on
         * the entity unconditionally (the dest field lives at +0xBC on
         * both the Building and ResourceGameObject families). */
        field_at<int32_t>(entity, 0xBC) = static_cast<int32_t>(record->dest);
#endif

        /* Children: 5 x 0x14 records at +0x1C.  Binary slot-15 dispatch
         * (0x41D7F3: call *0x3c(%edx) with this=entity, arg = the
         * unsigned-16 resource id; the RESULT is null-checked at
         * 0x41D7F8):
         *   - ResourceGameObject vtable slot [15] = CreateMember
         *     (0x458430): creates and attaches one member Building.
         *   - Building vtable slot [15] = Update(void*) (0x4327B0): the
         *     AI dispatch — NOT a child-creation operation.  Its return
         *     is void and EAX at return is this->field_dc (+0xDC,
         *     0x43291E), so the binary's result null-check would treat a
         *     non-zero field_dc as a bogus child pointer and then write
         *     +0x94 through it (a binary defect; a freshly placed
         *     building's field_dc is 0, so the null path is the
         *     invariant behaviour on the load path).
         * The typed model dispatches the same per-class slot-15 member:
         * the ResourceGameObject family's CreateMember for the group
         * parents; the Building family's Update(void*) semantics are
         * documented above and produce no child (the loop's null-check
         * path is taken — no garbage child is fabricated).  No raw
         * vtable access; the RTTI dynamic_cast is host-only. */
        for (int child_index = 0; child_index < 5; child_index++) {
            const ChildRecord& child_data = record->children[child_index];
            if (child_data.resource_id == 0) {
                continue;
            }
            Building* child = nullptr;
#ifndef _WIN32
            if (ResourceGameObject* parent =
                    dynamic_cast<ResourceGameObject*>(entity)) {
                child = parent->CreateMember(child_data.resource_id);
            }
#else
            /* _WIN32 (no RTTI): the canonical type tag reproduces the
             * same typed dispatch — ResourceGameObject's ctor writes
             * type = 3 (0x4580A0..0x4580A6); the Building/Train family
             * keeps the Entity type=2 tag, and its slot-15 is the
             * Update(void*) AI dispatch (documented above), so only the
             * type-3 family can yield a member child here. */
            if (entity->type == 3) {
                child = static_cast<ResourceGameObject*>(entity)
                            ->CreateMember(child_data.resource_id);
            }
#endif
            if (child == nullptr) {
                continue;
            }
            if (child_name_has_party(child_data.name)) {
                child->SetName(child_data.name);
            }
            child->create_time = child_data.value;   /* +0x94 */
        }
        index++;
    }

    /* Vehicle loop. */
    for (int veh_index = 0;
         !truncation && veh_index < static_cast<int>(vehicle_count);
         veh_index++) {
        VehicleRecord* veh_data = static_cast<VehicleRecord*>(
            RESMGR_UnlockResource(&resdata));
        if (veh_data == nullptr) {
#ifndef _WIN32
            /* Explicit failure (host hardening — the original skips). */
            truncation = true;
            break;
#else
            continue;
#endif
        }
#ifndef _WIN32
        if (!loco::host::host_placement_available()) {
            loco::host::PersistenceAdapter::instance().document().vehicles.push_back(*veh_data);
            continue;
        }
#endif
        void* building = INPUT_FindObjectAt(self, 3);
        if (building == nullptr) {
            continue;
        }
#ifndef _WIN32
        /* Host hardening: g_world (0x4A98B0) is constructed by
         * BootstrapMode3Core at runtime and may be null in host tests;
         * the original calls World_LoadFromFile (0x44DC10) on it
         * unconditionally. */
        if (g_world == nullptr) {
            continue;
        }
#endif
        Vehicle* vehicle = static_cast<World*>(g_world)->LoadFromFile(
            reinterpret_cast<int*>(building),
            reinterpret_cast<int*>(veh_data));
        if (vehicle == nullptr) {
            continue;
        }
        vehicle->UpdatePosition(0);                  /* 0x44D500 */
#ifndef _WIN32
        /* Host hardening: the original dereferences editors[0] (+0x10)
         * and dispatches SetName on it unconditionally (0x41D888..
         * 0x41D891). */
        if (vehicle->editors[0] != nullptr) {
            vehicle->editors[0]->SetName(veh_data->name);
        }
#else
        vehicle->editors[0]->SetName(veh_data->name);
#endif
    }

#ifndef _WIN32
    if (truncation) {
        RESMGR_RemoveResource(&resdata);
        RESMGR_ReleaseResource(&resdata);
        g_allow_building_placement = static_cast<uint8_t>(saved_placement);
        std::fprintf(stderr,
            "[HOST] INPUT_LoadSaveFile: '%s' truncated/oversized — load "
            "failed explicitly (no partial success)\n", path);
        std::fflush(stderr);
        return 0;
    }
#endif

    /* Cleanup + global state, exactly like the original tail. */
    RESMGR_RemoveResource(&resdata);                /* 0x447FB0 */
    g_in_build_mode = 1;                            /* 0x4FD199 */
    g_allow_building_placement = static_cast<uint8_t>(saved_placement);
    RESMGR_ReleaseResource(&resdata);               /* 0x447B90 */
    return 1;
}

/* ================================================================== */
/* INPUT_LoadWorld                                                    */
/* Address: 0x41D320                                                   */
/*                                                                      */
/* See InputMgr.h.  Host deviations (#ifndef _WIN32):                 */
/*   - the current-save marker is "curr" (original "~curr", 0x47E2A0); */
/*   - the ".sav" companion path is derived from the recorded          */
/*     current-save name (the original reads the backdrop window       */
/*     object's path buffer at [0x4FD3C8]+0x48+strlen(resdir) — the    */
/*     backdrop window class is not reconstructed on the host);        */
/*   - the Netman scenario-2 block is a guarded loud no-op when        */
/*     g_netman is null (component tests do not construct Netman).     */
/* ================================================================== */
char INPUT_LoadWorld(InputMgr* self, const char* path)
{
    /* Step 1: LoadSaveFile(path, 1, 1). */
    char result = INPUT_LoadSaveFile(self, path, 1, 1);

    /* Step 2: on success with the current-save marker, record the path
     * in the current-save global (0x4AA8F8). */
#ifndef _WIN32
    const char* curr_marker = "curr";
#else
    const char* curr_marker = "~curr";
#endif
    if (result != 0 && std::strstr(path, curr_marker) != nullptr) {
        std::snprintf(g_current_save_path, sizeof(g_current_save_path), "%s", path);
    }

    /* Step 3: always attempt the ".sav" companion (result discarded). */
    {
        char sav_path[0x108];
#ifndef _WIN32
        /* Host: derive from the recorded current-save name (falling
         * back to the caller's path when none is recorded).  The
         * original reads the 0x4FD3C8 object's save-name buffer at
         * +0x48+strlen(resdir); that object is not reconstructed on the
         * host, so the recorded name is the documented deviation. */
        const char* base = (g_current_save_path[0] != '\0')
            ? g_current_save_path : path;
        std::snprintf(sav_path, sizeof(sav_path), "%s.sav", base);
#else
        /* Original (0x41D379..0x41D3EE): copy the 0x4FD3C8 object's
         * save-name buffer ([obj]+0x48+strlen(resdir), a C string) to a
         * stack buffer, then REPLACE ITS LAST FOUR CHARACTERS with ".sav"
         * (0x47E4F4) — the binary writes ".sav" over
         * sav_path + strlen(sav_path) - 4 (0x41D3C9 sub $0x4,%eax;
         * 0x41D3CC add %eax,%edx), NOT appending.  A base shorter than
         * four characters would make the binary write before its buffer;
         * the reconstruction clamps to appending in that pathological
         * case (host-safe; the real backdrop save-name always carries the
         * res-dir prefix).  The 0x4FD3C8 slot is the tilemap.h
         * g_cursor_surface pointer; the string read is the documented
         * raw-offset access (TODO: typed backdrop/surface class during
         * integration). */
        extern void* g_cursor_surface;   /* tilemap.h, 0x4FD3C8 */
        const char* base = static_cast<const char*>(
            static_cast<char*>(g_cursor_surface) + 0x48 +
            std::strlen(g_install_path));
        std::snprintf(sav_path, sizeof(sav_path), "%s", base);
        const size_t base_len = std::strlen(sav_path);
        if (base_len >= 4) {
            std::snprintf(sav_path + base_len - 4,
                          sizeof(sav_path) - (base_len - 4), "%s", ".sav");
        } else {
            std::snprintf(sav_path + base_len,
                          sizeof(sav_path) - base_len, "%s", ".sav");
        }
#endif
#ifndef _WIN32
        /* Host: the companion's marker branch must not clobber the
         * primary load's current-name bookkeeping.  "curr" -> "curr.sav"
         * contains the "curr" marker, so INPUT_LoadSaveFile's host marker
         * branch would overwrite g_current_save_path with "curr.sav" (the
         * original never writes that global from LoadSaveFile at all — it
         * calls UIPANEL_Hide on the backdrop window instead).  The host
         * therefore snapshots the recorded name around the companion load
         * and restores it (documented deviation). */
        char saved_current[0x108];
        std::memcpy(saved_current, g_current_save_path, sizeof(saved_current));
        INPUT_LoadSaveFile(self, sav_path, 0, 0);
        std::memcpy(g_current_save_path, saved_current, sizeof(saved_current));
#else
        INPUT_LoadSaveFile(self, sav_path, 0, 0);
#endif
    }

    /* Step 4: multiplayer scenario 2 — scroll to player buildings and
     * clear the edge-building placement flags. */
#ifndef _WIN32
    if (g_netman == nullptr) {
        std::fprintf(stderr,
            "[HOST] INPUT_LoadWorld: Netman scenario-2 edge checks "
            "skipped (g_netman null)\n");
        std::fflush(stderr);
    } else
#endif
    if (static_cast<Netman*>(g_netman)->m_gameMode == 2) {
        /* Scroll to each placed player building (resource type 3,
         * +0x10C == 3, +0x120 == 1). */
        const int32_t count = self->ListGetCount();
        for (int32_t i = 0; i < count; i++) {
            Entity* entity = self->ListGetItem(i);
#ifndef _WIN32
            /* Host hardening: the binary dereferences the GetItem result
             * unconditionally (0x41D428 mov 0x40(%eax),%ecx — item->resource
             * read with no item null check). */
            if (entity == nullptr) {
                continue;
            }
#endif
            uint8_t type = 0;
            if (entity->resource != nullptr) {
                type = *reinterpret_cast<uint8_t*>(
                    static_cast<uint8_t*>(entity->resource) + 0x08);
            }
            if (type == 3 && entity_kind(entity) == 3 &&
                entity_mode_flag(entity, 1) == 1) {
#ifndef _WIN32
                if (g_tilemap == nullptr) {
                    continue;
                }
#endif
                static_cast<TileMap*>(g_tilemap)->ScrollTo(
                    as_tilemap_object(entity), 1);
            }
        }

        const uint8_t saved_placement = g_allow_building_placement;
        g_allow_building_placement = 1;

        Netman* netman = static_cast<Netman*>(g_netman);
        const int16_t player_id = static_cast<int16_t>(g_player_id);
        const int16_t player_color = static_cast<int16_t>(g_player_color);

#ifndef _WIN32
        if (g_tilemap == nullptr) {
            std::fprintf(stderr,
                "[HOST] INPUT_LoadWorld: edge placement-flag clear "
                "skipped (g_tilemap null)\n");
            std::fflush(stderr);
        } else
#endif
        {
            TileMap* tilemap = static_cast<TileMap*>(g_tilemap);
            if (netman->CheckUpEdge() != 0) {
                TileMapObject* obj = reinterpret_cast<TileMapObject*>(
                    TileMap_FindObject(tilemap, 0xC46,
                        static_cast<short>((player_id >> 1) - 1), 0, 0, 1));
                if (obj != nullptr) obj->is_moving = 0;
            }
            if (netman->CheckDownEdge() != 0) {
                TileMapObject* obj = reinterpret_cast<TileMapObject*>(
                    TileMap_FindObject(tilemap, 0xC48,
                        static_cast<short>((player_id >> 1) - 1),
                        static_cast<short>(player_color - 2), 0, 1));
                if (obj != nullptr) obj->is_moving = 0;
            }
            if (netman->CheckRightEdge() != 0) {
                TileMapObject* obj = reinterpret_cast<TileMapObject*>(
                    TileMap_FindObject(tilemap, 0xC42,
                        static_cast<short>(player_id - 3),
                        static_cast<short>((player_color >> 1) - 1), 0, 1));
                if (obj != nullptr) obj->is_moving = 0;
            }
            if (netman->CheckLeftEdge() != 0) {
                TileMapObject* obj = reinterpret_cast<TileMapObject*>(
                    TileMap_FindObject(tilemap, 0xC44, 0,
                        static_cast<short>((player_color >> 1) - 1), 0, 1));
                if (obj != nullptr) obj->is_moving = 0;
            }
        }

        g_allow_building_placement = saved_placement;
    }

    return result;
}

/* ================================================================== */
/* INPUT_SaveCurrentWorld                                             */
/* Address: 0x41D9B0                                                   */
/*                                                                      */
/* See InputMgr.h.  Host deviations (#ifndef _WIN32):                 */
/*   - the entity/vehicle counts come from the PersistenceAdapter's    */
/*     recovered record set (the collection placement is gated);       */
/*   - the preview written by RESMGR_LoadResourceData is a zeroed      */
/*     player_id*player_color buffer (the original renders a TileMap   */
/*     overlay surface), capped at 16 MiB (strict sane preview cap);   */
/*   - the save is ATOMIC (temp + rename via host_save_commit) and a   */
/*     write failure or failed commit returns 0 — the fresh seed then  */
/*     never reports success without a durable curr;                  */
/*   - the current-save marker is "curr" (original "~curr") and is     */
/*     recorded only after a durable save;                            */
/*   - save names that escape the save directory are refused (the      */
/*     same guard INPUT_LoadSaveFile applies — symmetric protection). */
/* ================================================================== */
char INPUT_SaveCurrentWorld(InputMgr* self, const char* name)
{
#ifndef _WIN32
    /* Host hardening: symmetric with INPUT_LoadSaveFile — refuse save
     * names that would escape the save directory. */
    if (loco::host::PersistenceAdapter::name_escapes(name)) {
        std::fprintf(stderr,
            "[HOST] INPUT_SaveCurrentWorld: refused name '%s' (escape)\n",
            name);
        std::fflush(stderr);
        return 0;
    }
#endif

    RESDATA resdata;
    RESMGR_ResourceData_Init(&resdata);            /* 0x447B20 */

    /* Build "<resdir><name>". */
    char path_buf[0x108];
#ifndef _WIN32
    if (!build_host_resource_path(path_buf, sizeof(path_buf), name)) {
        RESMGR_RemoveResource(&resdata);
        RESMGR_ReleaseResource(&resdata);
        return 0;
    }
#else
    std::snprintf(path_buf, sizeof(path_buf), "%s%s", g_install_path, name);
#endif

    /* ---- 0x114-byte header (RESDATA.save at +0xB0) ---- */
    std::memset(&resdata.save, 0, sizeof(SaveRegion));   /* rep stosd 0x45 */
    resdata.save.type = 8;                               /* +0x00 */
    resdata.save.player_id = static_cast<uint16_t>(g_player_id);    /* +0x02 */
    resdata.save.player_color = static_cast<uint16_t>(g_player_color); /* +0x04 */

#ifndef _WIN32
    /* Host: entity/vehicle counts from the recovered record set (the
     * collection placement is gated — see PersistenceAdapter.h). */
    const loco::host::SaveDocument& doc =
        loco::host::PersistenceAdapter::instance().document();
    resdata.save.entity_count =
        static_cast<uint32_t>(doc.entities.size());      /* +0x08 */
    resdata.save.vehicle_count =
        static_cast<uint16_t>(doc.vehicles.size());      /* +0x0C */
#else
    /* Original: entity count from this->entity_count (+0x14,
     * 0x41DA3F) and vehicle count from the 16-bit g_world vehicle
     * count at 0x4A98B4 (0x41DA6A). */
    resdata.save.entity_count = static_cast<uint32_t>(self->entity_count); /* +0x08 */
    resdata.save.vehicle_count = static_cast<uint16_t>(DAT_004a98b4);      /* +0x0C */
#endif
    /* +0x0E name: the original copies the BSS string at 0x4AA9FD, which
     * is never written and therefore empty. */
    resdata.save.name[0] = '\0';

    /* Open the output stream and write header + preview (0x447E30). */
#ifndef _WIN32
    /* Host preview: zeroed player_id*player_color buffer (no tilemap
     * overlay is rendered on the SDL host — documented deviation),
     * capped at 16 MiB (strict sane preview cap — g_player_id/color are
     * 16-bit map coordinates, so a real save is far below the cap; an
     * absurd pair fails the save instead of allocating gigabytes). */
    {
        uint32_t w = resdata.save.player_id;
        uint32_t h = resdata.save.player_color;
        if (w > 0 && h > 0) {
            constexpr size_t kMaxPreview = 16u * 1024u * 1024u;
            if (static_cast<size_t>(w) <= kMaxPreview / h) {
                size_t bytes = static_cast<size_t>(w) * h;
                void* preview = operator_new(bytes);
                if (preview != nullptr) {
                    std::memset(preview, 0, bytes);
                    resdata.save_pixels = preview;
                }
            }
        }
    }
#endif
    if (RESMGR_LoadResourceData(&resdata, path_buf) == 0) {
        RESMGR_ReleaseResource(&resdata);
        return 0;
    }

#ifndef _WIN32
    /* Host: write the recovered records (collection placement gated).
     * Every write result is checked: a failed write must not report a
     * successful save (the stream's error flag then fails the commit
     * below and the temp is removed — no partial curr). */
    bool writes_ok = true;
    for (const loco::host::EntityRecord& record : doc.entities) {
        if (RESMGR_WriteSaveRecord(&resdata, &record) == 0) {  /* 0x447F50 */
            writes_ok = false;
            break;
        }
    }
    for (const loco::host::VehicleRecord& record : doc.vehicles) {
        if (RESMGR_WriteTableRecord(&resdata, &record) == 0) {  /* 0x447F80 */
            writes_ok = false;
            break;
        }
    }
    if (!writes_ok || !loco::host::host_save_commit(&resdata)) {
        /* Uncommitted write stream: RESMGR_RemoveResource below removes
         * the temp file — the target path is never touched. */
        std::fprintf(stderr,
            "[HOST] INPUT_SaveCurrentWorld: save of '%s' failed — "
            "no durable file written (atomic save)\n", name);
        std::fflush(stderr);
        RESMGR_RemoveResource(&resdata);
        RESMGR_ReleaseResource(&resdata);
        return 0;
    }
#else
    /* Original: enumerate the collection (members with +0xC0 == 1) and
     * the level-table entries (0x4A98B8..0x4A98C8). */
    const int32_t count = self->ListGetCount();
    for (int32_t i = 0; i < count; i++) {
        Entity* entity = self->ListGetItem(i);
        if (entity == nullptr || as_tilemap_object(entity)->is_moving != 1) {
            continue;
        }
        EntityRecord record;
        std::memset(&record, 0, sizeof(record));   /* rep stosd 0x20 */
        /* Original field reads (0x41DB3D..0x41DB91): the 16-bit resource
         * id from entity->resource(+0x40)->+0x04 goes to record+0x00; the
         * FULL dword at entity+0x88 goes to record+0x02 (x = low 16,
         * y = high 16 — the writer never stores y separately); entity+0x28
         * (dword) -> record+0x08 (anim_state); entity+0xBC (dword) ->
         * record+0x0C (dest); entity+0x7C name -> record+0x10.  The
         * binary dereferences entity->resource(+0x40) unconditionally
         * (0x41DB47 mov 0x4(%ecx),%ax — no resource null check). */
        record.resource_id = *reinterpret_cast<uint16_t*>(
            static_cast<uint8_t*>(entity->resource) + 0x04);
        const uint32_t pos = field_at<uint32_t>(entity, 0x88);
        record.x = static_cast<uint16_t>(pos);
        record.y = static_cast<uint16_t>(pos >> 16);
        record.anim_state = field_at<uint32_t>(entity, 0x28);
        record.dest = field_at<uint32_t>(entity, 0xBC);
        std::strncpy(record.name, entity->name, sizeof(record.name));
        /* 5 child slots at +0x90 (occupied pointers): resource id from
         * child->resource(+0x40)->+0x04, value from child->+0x94, name
         * from child->+0x7C (0x41DB93..0x41DBE1).  The child POINTER null
         * check IS in the binary (0x41DB95 test %eax,%eax; je 0x41DBDA);
         * child->resource(+0x40) is then dereferenced unconditionally
         * (0x41DB99 mov 0x40(%eax),%ecx; 0x41DBA3 mov 0x4(%ecx),%ax). */
        for (int c = 0; c < 5; c++) {
            void* child = field_at<void*>(entity, 0x90 + c * sizeof(void*));
            if (child == nullptr) {
                continue;
            }
            record.children[c].resource_id = *reinterpret_cast<uint16_t*>(
                static_cast<uint8_t*>(field_at<void*>(child, 0x40)) + 0x04);
            record.children[c].value = field_at<uint32_t>(child, 0x94);
            std::strncpy(record.children[c].name,
                         field_at<const char*>(child, 0x7C),
                         sizeof(record.children[c].name));
        }
        RESMGR_WriteSaveRecord(&resdata, &record);
    }
    /* Level-table entries 0x4A98B8..0x4A98C8 (4 slots).  The original
     * (0x41DC10..0x41DCAA) writes, for each non-null entry object, a
     * 0x2C vehicle record: the 4 sub-slots at obj+0x10..0x1C each
     * contribute their resource id (sub-slot->resource(+0x40)->+0x04,
     * 16-bit) to record+0x00..0x0C and their +0x42C dword to
     * record+0x10..0x1C; the name is a strlen+1 copy of the FIRST
     * sub-slot's (*(obj+0x10)) +0x7C name into record+0x20 — the name
     * owner is sub-slot[0] at obj+0x10 (0x41DC78 mov (%ebx),%edi with
     * ebx = obj+0x10), NOT *(obj+0x20) (0x41DC78..0x41DC9C). */
    for (int32_t* entry = DAT_004a98b8; entry < DAT_004a98b8 + 4; entry++) {
        if (*entry == 0) {
            continue;
        }
        VehicleRecord record;
        std::memset(&record, 0, sizeof(record));   /* rep stosd 0x0B */
        void* obj = reinterpret_cast<void*>(*entry);
        for (int slot_index = 0; slot_index < 4; slot_index++) {
            void* slot = field_at<void*>(obj, 0x10 + slot_index * sizeof(void*));
            if (slot == nullptr) {
                continue;
            }
            void* resource = field_at<void*>(slot, 0x40);
            if (resource != nullptr) {
                *reinterpret_cast<uint32_t*>(
                    reinterpret_cast<uint8_t*>(&record) + slot_index * 4) =
                    *reinterpret_cast<uint16_t*>(
                        static_cast<uint8_t*>(resource) + 0x04);
            }
            *reinterpret_cast<uint32_t*>(
                reinterpret_cast<uint8_t*>(&record) + 0x10 + slot_index * 4) =
                field_at<uint32_t>(slot, 0x42C);
        }
        /* Name owner: *(obj+0x10) = sub-slot[0] (0x41DC78).  The binary
         * dereferences it unconditionally (0x41DC78 mov (%ebx),%edi;
         * 0x41DC7D add $0x7c,%edi — no null check). */
        std::strncpy(record.name,
                     field_at<const char*>(field_at<void*>(obj, 0x10), 0x7C),
                     sizeof(record.name));
        RESMGR_WriteTableRecord(&resdata, &record);
    }
#endif

    /* Finalize: RemoveResource, then — exactly like the binary tail
     * (0x41DCC5..0x41DD21: RemoveResource at 0x41DCCC, then the
     * "~curr" strstr at 0x41DCD8, then ReleaseResource, AL = 1) —
     * record the current-save path only for a durable save. */
    RESMGR_RemoveResource(&resdata);

    /* "curr" marker: record the save path in the current-save global. */
#ifndef _WIN32
    const char* curr_marker = "curr";
#else
    const char* curr_marker = "~curr";
#endif
    if (std::strstr(name, curr_marker) != nullptr) {
        std::snprintf(g_current_save_path, sizeof(g_current_save_path), "%s", name);
    }

    RESMGR_ReleaseResource(&resdata);
    return 1;
}

/* ================================================================== */
/* INPUT_RemoveObject (editor-only, deferred; loud)                   */
/* Address: 0x41DEF0                                                   */
/*                                                                      */
/* The editor placement helper is not on the load/save path; it stays  */
/* a loud deferred stub until the editor milestone.                   */
/* ================================================================== */

uintptr_t INPUT_RemoveObject(InputMgr* self, void* obj, unsigned int param) /* 0x41DEF0 */
{
    (void)self;
    (void)obj;
    (void)param;
    std::fprintf(stderr,
        "[InputMgr] INPUT_RemoveObject (0x41DEF0) is a deferred stub: "
        "editor-only, not on the load/save path\n");
    std::fflush(stderr);
    std::abort();
}

/* ================================================================== */
/* 0x4A99B0 event-list window entry points (deferred; loud)            */
/*                                                                      */
/* The 0x4A99B0 object (LoadEvents/TimeEvents/EasterEggs lists) is not */
/* reconstructed yet; these replace the old silent no-arg defsym stubs */
/* so any reachable call fails loudly instead of silently succeeding.  */
/* The host init paths that would call them are guarded adapters that  */
/* log loudly and skip (see PROGRESS.md session log).                  */
/* ================================================================== */

namespace {

[[noreturn]] void input_events_deferred(const char* name, uint32_t address)
{
    std::fprintf(stderr,
        "[InputMgr] %s (0x%08X) is a deferred stub: the 0x4A99B0 "
        "event-list window class is not reconstructed yet\n",
        name, address);
    std::fflush(stderr);
    std::abort();
}

} // namespace

void INPUT_SetKeyboard(void* self)   /* 0x41F7E0 — [EasterEggs] loader */
{
    (void)self;
    input_events_deferred("INPUT_SetKeyboard", 0x41F7E0);
}

void INPUT_SetMouse(void* self)      /* 0x41F970 — egg record / season date */
{
    (void)self;
    input_events_deferred("INPUT_SetMouse", 0x41F970);
}

void INPUT_SwitchToLocomotiveTab(void* self, int tab) /* 0x41A210 — Cursor tab-switch */;
void INPUT_SwitchToLocomotiveTab(void* self, int tab) /* 0x41A210 — Cursor tab-switch */
{
    (void)self;
    (void)tab;
    input_events_deferred("INPUT_SwitchToLocomotiveTab", 0x41A210);
}

/* ================================================================== */
/* Real reconstructions of 8 further 0x4A99B0 event-list members       */
/*                                                                      */
/* The 0x4A99B0 object itself is still not a canonical typed class      */
/* (that milestone remains open — see the header comment above); the   */
/* two node types below are documented locally, matching this file's   */
/* existing convention of passing `void* self` for this specific       */
/* object. Field names beyond the ones actually written by these 8     */
/* functions (verified by direct decompile/disassembly) are            */
/* intentionally generic per this project's naming policy.             */
/* ================================================================== */

extern "C" {
    struct tm* CRT_localtime(const int32_t* time);   /* CRT wrapper; standard struct tm layout */
}

/* Plain C++ linkage, matching core/Game.cpp's declaration of the same
 * real symbol (0x423AB0). */
void UI_CreateMessageBox(void* mgr, int32_t res_id, int32_t p2, char p3,
                         int32_t x, int32_t y, int32_t p7);

namespace {

/* LoadEvents-list node (0x34 = 52 bytes, operator_new(0x34)).
 * Verified via disassembly of INPUT_AddLoadEvent (0x41FB20): the 6-token
 * "%ld,%ld,%ld,%ld,%ld,%ld" format (string 0x47E650) writes to node+0xC,
 * +0x10, +0x20, +0x24, +0x28, +0x2C — i.e. the coordinate/segment_index
 * slots of two TrackPos-shaped 0x14-byte blocks at +0x00/+0x14, plus two
 * trailing dwords with no further evidenced meaning. */
struct LoadEventNode {
    int32_t field_00[3];    // +0x00 (vtable/field_04/field_08 slots left at TrackPos_Init's -1 sentinel)
    int32_t coord_a;         // +0x0C
    int32_t segment_a;       // +0x10 (parsed value decremented by 1: 1-based -> 0-based)
    int32_t field_14[3];     // +0x14
    int32_t coord_b;         // +0x20
    int32_t segment_b;       // +0x24 (decremented by 1)
    int32_t field_28;        // +0x28
    int32_t field_2C;        // +0x2C
    LoadEventNode* next;     // +0x30
};
static_assert(offsetof(LoadEventNode, next) == 0x30, "LoadEventNode layout must match verified offsets");

/* TimeEvents-list node (0x48 = 72 bytes, operator_new(0x48)).
 * Verified via disassembly of INPUT_AddTimeEvent (0x41FBE0): the 14-token
 * "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%hd,%ld,%c,%ld,%ld" format (string
 * 0x47E668) writes to +0xC/+0x10 (start-window day/month, decremented by
 * 1), +0x20/+0x24 (end-window day/month, decremented by 1), +0x8/+0x4
 * (start hour/minute), +0x1C/+0x18 (end hour/minute), +0x28, +0x2C
 * (16-bit), +0x30 (repeat_mode), +0x34 (a %c byte immediately overwritten
 * by the trigger-time computation below — dead value), +0x38, +0x3C.
 * INPUT_CheckScheduledEvents (0x41FF20) independently confirms +0x28
 * (int), +0x2C (short), +0x38 (char), +0x3C (int), +0x40 (int) as the
 * UI_CreateMessageBox argument tuple, and +0x30/+0x34 as repeat_mode/
 * trigger_time. +0x00/+0x14 read as a libc-compatible struct tm's first
 * 5 int fields {sec,min,hour,mday,mon} (start/end window), matching
 * Game_IsPositionBetween's documented struct shape exactly. */
struct TimeEventNode {
    int32_t start_sec;       // +0x00
    int32_t start_min;       // +0x04
    int32_t start_hour;      // +0x08
    int32_t start_day;       // +0x0C
    int32_t start_month;     // +0x10 (decremented by 1: 1-based -> 0-based)
    int32_t end_sec;         // +0x14
    int32_t end_min;         // +0x18
    int32_t end_hour;        // +0x1C
    int32_t end_day;         // +0x20
    int32_t end_month;       // +0x24 (decremented by 1)
    int32_t msg_res_id;      // +0x28  UI_CreateMessageBox arg (res_id)
    int16_t msg_type;        // +0x2C  UI_CreateMessageBox arg (p2/type)
    int16_t _pad_2E;
    int32_t repeat_mode;     // +0x30  <0 (except -1) = random range |mode|-1; -1 = immediate/no-repeat; >=0 = random 0..mode
    int32_t trigger_time;    // +0x34  next scheduled tick (set after parsing, overwrites a transient %c byte)
    char    msg_anchor;      // +0x38  UI_CreateMessageBox arg (p3/anchor)
    char    _pad_39[3];
    int32_t msg_x;           // +0x3C  UI_CreateMessageBox arg (x)
    int32_t msg_y;           // +0x40  UI_CreateMessageBox arg (y)
    TimeEventNode* next;     // +0x44 in the original x86 layout; on this
                              // 64-bit host the compiler pads to an 8-byte
                              // boundary before the pointer, landing `next`
                              // at offset 0x48 instead — no x86-layout-parity
                              // assert here for the same reason as
                              // BuildingDescriptorEditor.h's KeySequenceRecord.
};

} // namespace

/* ================================================================== */
/* INPUT_ResetLoadEventNode / INPUT_ResetTimeEventNode                  */
/* Addresses: 0x41F540 / 0x41F590                                       */
/* Ghidra's "INPUT_FreeEditControl"/"INPUT_AllocEditControl" names are  */
/* misnomers verified by direct decompile: neither frees nor allocates */
/* anything. Both bodies are identical two-call TrackPos_BaseInit       */
/* resets (only their SEH handler table entry differs); only caller of */
/* either is INPUT_FreeEvents (0x41F4E0), which then GLOBAL_frees the   */
/* node. Not methods of BuildingDescriptorEditor despite the similar    */
/* Ghidra-generated name — see BuildingDescriptorEditor.h.              */
/* ================================================================== */
void INPUT_ResetLoadEventNode(void* node)
{
    auto* n = static_cast<LoadEventNode*>(node);
    TrackPos_BaseInit(reinterpret_cast<TrackPos*>(&n->field_14));
    TrackPos_BaseInit(reinterpret_cast<TrackPos*>(n));
}

void INPUT_ResetTimeEventNode(void* node)
{
    auto* n = static_cast<TimeEventNode*>(node);
    TrackPos_BaseInit(reinterpret_cast<TrackPos*>(&n->end_sec));
    TrackPos_BaseInit(reinterpret_cast<TrackPos*>(n));
}

/* ================================================================== */
/* INPUT_LoadTimeEvents — load [TimeEvents] from LOCO.INI               */
/* Address: 0x41F6E0 (Ghidra label "INPUT_EditMouseHandler" — misnomer, */
/* verified by direct decompile; unrelated to mouse input).             */
/* Structurally identical to INPUT_LoadEvents (0x41F5E0, not part of    */
/* this pass's assigned function list): builds "LOCO.INI" via           */
/* PlayerConfig_Ctor/CRT_sprintf_buf, then reads "%03ld"-keyed           */
/* [TimeEvents] entries until empty, calling INPUT_AddTimeEvent for      */
/* each. Full INI-loading plumbing (PlayerConfig_Ctor path) is out of   */
/* scope for this pass — this loud-deferred rather than silently wired, */
/* consistent with the sibling INPUT_LoadEvents/INPUT_SetKeyboard/      */
/* INPUT_SetMouse loaders in this same object, all still deferred.      */
/* ================================================================== */
void INPUT_LoadTimeEvents(void* self)
{
    (void)self;
    input_events_deferred("INPUT_LoadTimeEvents", 0x41F6E0);
}

/* ================================================================== */
/* INPUT_DiscoverEasterEgg — Address: 0x41F8E0                          */
/* Ghidra label "INPUT_EditScrollHandler" — misnomer verified by direct */
/* decompile. If the resource is not yet marked discovered (+0x163),    */
/* writes a "%ld" entry to [EasterEggs] in LOCO.INI (Config_WriteInt)   */
/* and marks it discovered. INI plumbing is out of scope for this pass  */
/* (same as INPUT_LoadTimeEvents above) — loud deferred stub.           */
/* ================================================================== */
uint32_t INPUT_DiscoverEasterEgg(void* self, uint32_t resId)
{
    (void)self;
    (void)resId;
    input_events_deferred("INPUT_DiscoverEasterEgg", 0x41F8E0);
}

/* ================================================================== */
/* INPUT_AddLoadEvent — Address: 0x41FB20                               */
/* Ghidra label "INPUT_EditPaintSelection" — misnomer verified by       */
/* direct decompile/disassembly (see LoadEventNode above for the field  */
/* mapping). Allocates a node, parses the 6-field CSV string via         */
/* sscanf (same tokens as the original's internal CRT helper), adjusts  */
/* the two segment fields to 0-based, and prepends to self+0x08.        */
/* ================================================================== */
void* INPUT_AddLoadEvent(void* self, const char* fields)
{
    auto* node = new (std::nothrow) LoadEventNode();
    if (node == nullptr) {
        return nullptr;
    }
    std::memset(node, 0, sizeof(LoadEventNode));

    std::sscanf(fields, "%d,%d,%d,%d,%d,%d",
                &node->coord_a, &node->segment_a,
                &node->coord_b, &node->segment_b,
                &node->field_28, &node->field_2C);
    node->segment_a -= 1;
    node->segment_b -= 1;

    auto* selfBytes = static_cast<uint8_t*>(self);
    auto** head = reinterpret_cast<LoadEventNode**>(selfBytes + 0x08);
    node->next = *head;
    *head = node;
    return node;
}

/* ================================================================== */
/* INPUT_AddTimeEvent — Address: 0x41FBE0                               */
/* Ghidra label "INPUT_EditTimerHandler" — misnomer verified by direct  */
/* decompile/disassembly (see TimeEventNode above for the field         */
/* mapping and the repeat_mode/trigger_time tail logic transcribed      */
/* below, preserved exactly from the decompilation).                    */
/* ================================================================== */
void* INPUT_AddTimeEvent(void* self, const char* fields)
{
    auto* node = new (std::nothrow) TimeEventNode();
    if (node == nullptr) {
        return nullptr;
    }
    std::memset(node, 0, sizeof(TimeEventNode));

    char discardedChar = 0;
    std::sscanf(fields, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%hd,%d,%c,%d,%d",
                &node->start_day, &node->start_month,
                &node->end_day, &node->end_month,
                &node->start_hour, &node->start_min,
                &node->end_hour, &node->end_min,
                &node->msg_res_id, &node->msg_type,
                &node->repeat_mode, &discardedChar,
                &node->msg_x, &node->msg_y);
    node->start_month -= 1;
    node->end_month -= 1;

    int32_t repeatMode = node->repeat_mode;
    int32_t offset;
    if (repeatMode < 0) {
        offset = 1;
        if (repeatMode != 1) {
            offset = static_cast<int32_t>(CRT_rand()) % (1 - repeatMode) + repeatMode;
        }
    } else {
        offset = 0;
        if (repeatMode != -1) {
            offset = static_cast<int32_t>(CRT_rand()) % (repeatMode + 1);
        }
    }

    auto* selfBytes = static_cast<uint8_t*>(self);
    int32_t currentTick = *reinterpret_cast<int32_t*>(selfBytes + 0x04);
    node->trigger_time = currentTick + offset;

    auto** head = reinterpret_cast<TimeEventNode**>(selfBytes + 0x0C);
    node->next = *head;
    *head = node;
    return node;
}

/* ================================================================== */
/* INPUT_CheckScheduledEvents — Address: 0x41FF20                       */
/* Ghidra label "INPUT_EditSetFocus" — misnomer verified by direct      */
/* decompile. Scans the TimeEvents list at self+0x0C for the first      */
/* entry whose [start,end) window (Game_IsPositionBetween) contains the */
/* current local time and whose trigger_time has elapsed; shows it via  */
/* UI_CreateMessageBox and reschedules per repeat_mode. The real body   */
/* is transcribed below (commented out) exactly as decompiled/verified  */
/* against disassembly, together with the TimeEventNode field mapping   */
/* above — but Game_IsPositionBetween/UI_CreateMessageBox both live in  */
/* core/Game.cpp, whose own dependency graph is far larger than this    */
/* file's existing footprint (this file's build/test targets link a    */
/* deliberately curated, --unresolved-symbols=ignore-all-free object    */
/* set; see tests/meson.build's comment on inputmgr_canonical_test).    */
/* Pulling in core/Game.cpp here is a real architectural decision for   */
/* a future pass, not something to do as a side effect of this one, so  */
/* this stays a loud deferred stub like its siblings (INPUT_SetKeyboard,*/
/* INPUT_SetMouse, INPUT_LoadTimeEvents, INPUT_DiscoverEasterEgg) rather */
/* than silently wired half-open.                                      */
/*
uint8_t INPUT_CheckScheduledEvents_reference(void* self)
{
    auto* selfBytes = static_cast<uint8_t*>(self);
    int32_t currentTick = *reinterpret_cast<int32_t*>(selfBytes + 0x04);

    struct tm* now = CRT_localtime(reinterpret_cast<int32_t*>(selfBytes + 0x04));
    TimeEventNode* node = *reinterpret_cast<TimeEventNode**>(selfBytes + 0x0C);
    if (node == nullptr) {
        return 0;
    }

    while (true) {
        bool inWindow = Game_IsPositionBetween(
            reinterpret_cast<int*>(now),
            reinterpret_cast<int*>(&node->start_sec),
            reinterpret_cast<int*>(&node->end_sec)) != 0;
        if (inWindow && node->trigger_time < currentTick) {
            break;
        }
        node = node->next;
        if (node == nullptr) {
            return 0;
        }
    }

    UI_CreateMessageBox(g_tooltip_mgr, node->msg_res_id, node->msg_type,
                        node->msg_anchor, node->msg_x, node->msg_y, 1);

    int32_t repeatMode = node->repeat_mode;
    if (repeatMode > 0) {
        if (repeatMode == 0) {
            node->trigger_time = currentTick + 1;
            return 1;
        }
        node->trigger_time = currentTick + static_cast<int32_t>(CRT_rand()) % repeatMode + 1;
        return 1;
    }

    int32_t offset = repeatMode;
    if (repeatMode != 2) {
        offset = static_cast<int32_t>(CRT_rand()) % (2 - repeatMode) + repeatMode;
    }
    node->trigger_time = currentTick + offset;
    return 1;
}
*/
uint8_t INPUT_CheckScheduledEvents(void* self)
{
    (void)self;
    input_events_deferred("INPUT_CheckScheduledEvents", 0x41FF20);
}

/* ================================================================== */
/* INPUT_PeriodicTickDispatch — Address: 0x41FD00                       */
/* Ghidra label "INPUT_EditCommandHandler" — misnomer verified by       */
/* direct decompile. Receiver evidence is thin (see InputMgr.h); this   */
/* reads only `self+4` as a tick count (wParam) and otherwise drives    */
/* the global entity/building/vehicle lists directly, not any          */
/* InputMgr/event-list field. Preserved faithfully including its        */
/* PostMessageA-based fan-out; the exact global accessor shapes         */
/* (DAT_004a9994-vtable-style dispatch, g_building_list/g_vehicle_list) */
/* are out of scope for this pass and left as a documented TODO rather  */
/* than guessed. */
/* ================================================================== */
int32_t INPUT_PeriodicTickDispatch(void* self)
{
    (void)self;
    input_events_deferred("INPUT_PeriodicTickDispatch", 0x41FD00);
}
