/**
 * VehicleEditor.h — Track-placement editor for vehicle route editing
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: TRANSCRIBED
 *
 * VehicleEditor manages the user interface for placing and editing train
 * track routes during gameplay. It owns two EditorState objects
 * (one for each end of the track segment being placed) and coordinates
 * their movement, collision detection, bridge checks, and rendering.
 *
 * The class also contains DPLAY network data for multiplayer track
 * synchronization (via GetDPlayData/SetDPlayData).
 *
 * Size: ~0x450 bytes (1092 bytes)
 * Vtable: 0x477590
 *
 * Vtable layout — re-verified this session via a raw read of the vtable
 * data at 0x477590 (64 bytes = slots [0]-[15]). Several slots were
 * mislabeled in earlier passes (see per-slot notes below); slots
 * [1]-[14] are ALL inherited from Entity unmodified (see core/Entity.h's
 * own 15-slot vtable list at 0x477488) — VehicleEditor only supplies
 * slot [0] (its own destructor override) and slot [15] (a genuinely new
 * VehicleEditor-only virtual; Entity's vtable ends at slot [14]/+0x38):
 *   [0]  +0x00: scalar deleting destructor (0x40D660) — OVERRIDDEN
 *   [1]  +0x04: InvalidateRect (inherited, unmodified: 0x436AB0)
 *   [2]  +0x08: PtInRect (inherited, unmodified: 0x436A10)
 *   [3]  +0x0C: MoveTo / SetWorldPos (inherited, unmodified: 0x405C00).
 *               A previous version of this comment claimed slot [3] WAS
 *               SetRenderOffset (0x40D8E0) — wrong; the raw vtable data
 *               is 0x405C00 (Entity::MoveTo), not 0x40D8E0. SetRenderOffset
 *               is a plain non-virtual member (see below) that itself
 *               CALLS this slot (via GameObject_HitTest) after recomputing
 *               screen_rect — it does not occupy it.
 *   [4]  +0x10: InvokeCallback1 (inherited, unmodified: 0x436AE0)
 *   [5]  +0x14: InvokeCallback2 (inherited, unmodified: 0x436B00)
 *   [6]  +0x18: InitBase (inherited, unmodified: 0x405900) — previously
 *               mislabeled "SetAnimState"; corrected against the raw
 *               vtable read.
 *   [7]  +0x1C: StopSound (inherited: 0x405A20) — NOT overridden by
 *               VehicleEditor. Confirmed by a raw read of the vtable data
 *               at 0x477590+0x1C (= 0x405A20 = Entity::StopSound); the
 *               previous "GetResourceId" label here was wrong — GetResourceId
 *               (0x40E0D0) is a plain non-virtual member, not in this vtable.
 *   [8]  +0x20: SetFrame (inherited, unmodified: 0x405DE0)
 *   [9]  +0x24: SetVisible (inherited, unmodified: 0x4061B0) — previously
 *               mislabeled "IsInBounds"; IsInBounds (0x40E250, see below)
 *               is also a plain non-virtual member, not in this vtable.
 *   [10] +0x28: Update (inherited, unmodified: 0x405C40)
 *   [11] +0x2C: Draw (inherited, unmodified: 0x405E60)
 *   [12] +0x30: DrawConnected (inherited, unmodified: 0x405FD0)
 *   [13] +0x34: SetName (inherited, unmodified: 0x405E20)
 *   [14] +0x38: SetAnimState (inherited, unmodified: 0x405A50)
 *   [15] +0x3C: SetResourceId(int resource_id, int anim_index) (0x40E0F0)
 *               — a genuinely NEW slot (nothing occupies it in Entity's
 *               own 15-slot vtable above), the only VehicleEditor-specific
 *               entry besides its own destructor at [0]. Read directly
 *               past this vtable's 64 documented bytes (0x4775D0, 48
 *               more bytes) shows non-code data immediately after,
 *               confirming the table really is exactly 16 slots.
 *               get_xrefs_to(0x40E0F0) shows exactly one reference in the
 *               whole binary: the vtable data slot itself (0x4775CC) —
 *               i.e. its sole caller was always a virtual dispatch, never
 *               a direct call. That caller is World_SerializeMap
 *               (0x44DEA0, game/World.cpp) — previously a literal
 *               `*(vtable+0x3C)` cast taken against this TU's own *host*
 *               GCC vtable pointer (an x86 byte offset misapplied to an
 *               unrelated 8-byte-entry host layout — see game/World.cpp's
 *               fix comment for why that was a live bug, not just a style
 *               issue), now this typed virtual call.
 *
 * Class hierarchy:
 *   GameObject (root, type=1)
 *     └─ Entity (type=2)
 *          └─ VehicleEditor  ← this class
 */

#pragma once

#include "Entity.h"

/* EditorState — per-end track editor state machine
   Defined in world/EditorState.h. Size: 0x20 bytes. */
#include "../world/EditorState.h"

class Vehicle;  // forward — see game/Vehicle.h
#ifdef _WIN32
/* Complete type needed here (not just forward-declared): the +0x88 slot
 * below is a real embedded DPlayManager, matching the original x86
 * layout exactly (see that field's own comment). */
#include "../network/DPlayManager.h"
#else
class DPlayManager;
struct HostNetworkEditorTag {};
#endif

class VehicleEditor : public Entity {
public:
    /* Fields at +0x00..+0x87 are inherited from Entity/GameObject —
       see Entity.h and GameObject.h for the base layout. */

#ifdef _WIN32
    /* +0x88..+0x424: embedded DPlayManager player slot (0x39C bytes,
     * network/DPlayManager.h). Previously modeled as a `uint8_t[0x398]`
     * byte array (`dplay_data`) plus a separate `dplay_trailer` dword,
     * reinterpret_cast'd back to DPlayManager* at every use site — that
     * split existed only because DPlayManager itself was 4 bytes short
     * of its real 0x39C size until 2026-08-14 (see DPlayManager.h's
     * `unknown_0x398` field comment for the full evidence trail). Now
     * that DPlayManager is correctly sized, this is one real member,
     * matching CLAUDE.md's "add the fields to the canonical class
     * instead of a raw byte view" rule — no more reinterpret_cast. */
    DPlayManager dplay_data;      // +0x88
#endif
    uint8_t   dplay_initialized;  // +0x424  1 = DPLAY data is valid
#ifndef _WIN32
    DPlayManager* host_dplay_data = nullptr; // native-width owned copy
#endif
    uint8_t   _pad_425[3];        // +0x425
    int32_t   res_id;             // +0x428  copy of resource ID from constructor
    int32_t   res_id_2;           // +0x42C  secondary resource ID (for track? from ctor param_2)
    EditorState* end_a;  // +0x430  editor state for track end A (first placeholder)
    EditorState* end_b;  // +0x434  editor state for track end B (second placeholder)
    uint16_t  angle_frame;        // +0x438  current angle / sprite frame index (computed by CalcAngle)
    uint16_t  second_angle;       // +0x43A  secondary angle component
    uint8_t   unknown_flag;       // +0x43C
    uint8_t   _pad_43D[3];        // +0x43D
    int32_t   edge_dir_a;         // +0x440  edge direction for end A (0=forward, 1=backward, 2=out-of-bounds)
    int32_t   edge_dir_b;         // +0x444  edge direction for end B (0=forward, 1=backward, 2..5=edit edges)
    uint16_t  bound_check_flag;   // +0x448  flag for bound-checking (0/1)
    uint8_t   _pad_44A[2];        // +0x44A
    /* +0x44C  Backref to the owning Vehicle (or NULL). Written by
     * Vehicle::Vehicle (0x44BF6D: `MOV [ECX+0x44C], ESI` with ECX = this
     * editor, ESI = the Vehicle under construction) and Vehicle::InitRoute
     * (0x44C2C1, identical instruction pattern) whenever the newly
     * constructed editor's `initialized` flag is set — see game/Vehicle.cpp.
     * A prior pass mistyped this `void*` and cast it to `Building*` in the
     * destructor (see ~VehicleEditor below); Building::occupation_level
     * happens to sit at the same +0x88 offset as Vehicle::init_flag, so
     * that bug silently read the wrong field instead of crashing. */
    Vehicle*  target_building;    // +0x44C

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * VehicleEditor constructor (Entity-level constructor with addition).
     * Address: 0x40D500
     *
     * @param res_id   Resource ID for the track/building being placed
     * @param param_2  Secondary resource ID (track variant?)
     * @param flag     Direction flag (0 = forward-first, non-zero = backward-first)
     */
    VehicleEditor(int res_id, int param_2, char flag);
#ifndef _WIN32
    VehicleEditor(HostNetworkEditorTag, int res_id, int param_2, char flag);
#endif

    /**
     * Destructor — releases EditorState objects and DPLAY data.
     * Address: 0x40D680 (body), 0x40D660 (scalar deleting wrapper, vtable[0])
     *
     * Also checks the owning Vehicle's init_flag (0x40D6E4:
     * `CMP byte[EAX+0x88], 0`, EAX = target_building) and invalidates the
     * editor's screen rect when it is 0 (locally-created vehicle, not a
     * remote/deferred one — see game/Vehicle.h's init_flag doc comment).
     */
    virtual ~VehicleEditor();

    /* ================================================================ */
    /* VehicleEditor methods                                            */
    /* ================================================================ */

    /**
     * InitTracks — Initialize both editor ends at a given position.
     * Address: 0x40D890
     *
     * @param x  World X position
     * @param y  World Y position
     * @return  1 on success, 0 on failure
     */
    uint32_t InitTracks(int x, int y);

    /**
     * SetRenderOffset (vtable[3]) — Recompute screen rect from end A's
     * track position and resource tile offsets. After updating the rect,
     * dispatches vtable[3] (HitTest/position update) with new left/top.
     *
     * NOTE: Resource is loaded from the repurposed Entity parent slot
     * at +0x40 (set during GameObject_BaseCtor).
     * Address: 0x40D8E0
     */
    void SetRenderOffset();

    /**
     * ProcessMove — Per-frame update for vehicle dragging on track.
     * Address: 0x40D940
     *
     * Called every frame while editing. Updates both EditorState positions,
     * handles track-follow, bounds checking, bridge/edge detection, and
     * road-to-building transitions.
     *
     * @param vehicle  Vehicle being dragged
     */
    void ProcessMove(Vehicle* vehicle);

    /**
     * MoveAlongTrack — Advance both editor ends along track one step.
     * Address: 0x40DC20
     *
     * @param vehicle  Vehicle being moved
     * @return  non-zero on success
     */
    uint32_t MoveAlongTrack(Vehicle* vehicle);

    /**
     * CheckBridge — Bridge edge collision detection dispatch.
     * Address: 0x40DB90
     *
     * @param vehicle  Vehicle to check
     */
    void CheckBridge(Vehicle* vehicle);

    /* ================================================================ */
    /* Rendering / frame management                                      */
    /* ================================================================ */

    /**
     * CalcAngle — Compute sprite angle from the two editor end positions.
     * Address: 0x40DF80
     *
     * Uses atan2 to compute the angle of the vector from end_b to end_a
     * in screen coordinates, then maps the result to 128 sprite frame
     * indices (0-127, wrapping at 128) for a full rotation.
     *
     * The original x87 FPU code uses FPATAN (partial arctangent) with
     * absolute deltas and manual quadrant adjustment. The calling
     * convention passes an unused stack argument from vehicle[+0x08]
     * (RET 4 at function exit), but the parameter is not used by
     * the function body.
     */
    void CalcAngle();

    /**
     * TriggerSound — Play the sound associated with the current frame.
     * Address: 0x40E130
     */
    void TriggerSound();

    /**
     * BlitBackground — Blit the track sprite background to the primary surface.
     * Address: 0x40E160
     *
     * @param clip_x  Screen clip X offset
     * @param clip_y  Screen clip Y offset
     * @return  1 if blitted, 0 if fully clipped
     */
    uint32_t BlitBackground(int clip_x, int clip_y);

    /**
     * IsInBounds (non-virtual — vtable[9] is actually Entity::SetVisible,
     * see the vtable slot correction above) — Test if a 16x16 point
     * intersects the editor rect.
     * Address: 0x40E250
     *
     * @param x     Grid X coordinate
     * @param y     Grid Y coordinate
     * @param flag  Bound check flag to match
     * @return  1 if in bounds, 0 otherwise
     */
    uint32_t IsInBounds(short x, short y, short flag);

    /**
     * CheckEdgeBounds — Detect when vehicle reaches world edge.
     * Address: 0x40E2A0
     *
     * @param vehicle  Vehicle being edited
     * @return  0
     */
    uint32_t CheckEdgeBounds(Vehicle* vehicle);

    /**
     * CheckVehicleAttach — Auto-attach logic for road/vehicle alignment.
     * Address: 0x40E340
     *
     * @param vehicle  Vehicle to check
     */
    uint32_t CheckVehicleAttach(Vehicle* vehicle);

    /**
     * CheckEditBounds1 — First-stage edit bounds check (bridge approach).
     * Address: 0x40E440
     *
     * @param vehicle  Vehicle being edited
     * @return  non-zero if callback was triggered
     */
    uint32_t CheckEditBounds1(Vehicle* vehicle);

    /**
     * CheckEditBounds2 — Second-stage edit bounds check (bridge retreat).
     * Address: 0x40E520
     *
     * @param vehicle  Vehicle being edited
     * @return  non-zero if callback was triggered
     */
    uint32_t CheckEditBounds2(Vehicle* vehicle);

    /**
     * GetResourceId (non-virtual — see the vtable[7] correction above) —
     * Returns the resource ID if track resource loaded.
     * Address: 0x40E0D0
     */
    uint32_t GetResourceId();

    /**
     * SetResourceId (vtable[15], +0x3C) — Reload the editor's resource and
     * recompute its route direction. Address: 0x40E0F0.
     *
     * The first VehicleEditor-specific vtable slot (see the vtable layout
     * comment above the class). Writes res_id (+0x428) unconditionally,
     * calls the inherited Entity::InitBase(resource_id, anim_index,
     * force_reload=false), and only on success recomputes res_id_2
     * (+0x42C) via CGWND_MapResourceToDirection(resource_id). Returns
     * InitBase's result.
     *
     * @param resource_id  New resource ID to load
     * @param anim_index   Initial animation index (World_SerializeMap's
     *                     sole caller passes -1)
     * @return             InitBase's result (non-zero on success)
     */
    virtual int SetResourceId(int resource_id, int anim_index);

    /**
     * GetDPlayData — Get pointer to DPLAY network data if initialized.
     * Address: 0x40D750
     *
     * @return  Pointer to +0x88 if flag at +0x424 is set, or NULL
     */
    DPlayManager* GetDPlayData();

    /**
     * SetDPlayData — Copy network data into the editor's DPLAY buffer.
     * Address: 0x40D770
     *
     * @param data  Incoming DPLAY network data buffer
     * @return  non-zero on success
     */
    int SetDPlayData(const DPlayManager* data);
};

