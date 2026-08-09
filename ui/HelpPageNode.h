/**
 * HelpPageNode.h — Road-tile vehicle queue / help-page sequencing node
 * Status: TRANSCRIBED
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * HelpPageNode is a small (0x128 byte) class that extends
 * RESDATA_GameVehicle with a per-object singly-linked vehicle queue.
 * The "HelpPageNode" name is a legacy misnomer from an earlier session
 * (Ghidra's decompiler had bound this constructor to a colliding
 * "HelpWnd_FindPage" label): its *sole* construction site is
 * INPUT_PlaceObject (0x41DD80, verified via get_xrefs_to 0x44F210 —
 * exactly one caller), reached whenever a ROAD-type resource
 * (RESDATA_IsRoadTile) is placed on the tile map — i.e. every ordinary
 * road/junction tile placed during normal play becomes a HelpPageNode,
 * not just tutorial content. `overlay_flag` (+0x120) is 1 only for the
 * specific tutorial overlay resource IDs (0xC42/0xC44/0xC46/0xC48); for
 * every other road resource it is 0 and the object still participates in
 * normal vehicle-arrival queuing via AddVehicle/RemoveVehicle below. Not
 * renamed in this session — see docs/landmine-sweep-worklist.md for the
 * follow-up note (renaming touches ui/HelpWnd.{h,cpp},
 * shared/vtable_addrs.h, shared/core_stubs.cpp and needs its own pass).
 *
 * Vtable: 0x4783D8 (VTBL_HELPPAGE_NODE)
 * Size: 0x128 bytes (RESDATA_GameVehicle 0x11C + 0xC)
 *
 * Vtable layout (extends RESDATA_GameVehicle's 15-slot vtable):
 *   [0]  +0x00: scalar deleting destructor  (0x44F2A0) — OVERRIDDEN
 *   [1]  +0x04: base destructor             (0x44F2C0)
 *   [10] +0x28: Update                      (0x44F340) — OVERRIDDEN
 *   (remaining slots inherited from RESDATA_GameVehicle)
 *
 * AddVehicle (0x44F3A0) and RemoveVehicle (0x44F410) are NOT in the
 * vtable — both are direct (non-virtual) calls in the binary, address-
 * contiguous with this class's other members (0x44F210..0x44F488: ctor,
 * both dtors, Update, AddVehicle, RemoveVehicle — one unbroken block,
 * consistent with a single MSVC 1998 translation unit emitting one
 * class's members). Formerly transcribed as free functions
 * `ArrivalQueue_AddVehicle`/`ArrivalQueue_RemoveVehicle` operating on an
 * untyped `void* self` (see docs/landmine-sweep-worklist.md's ArrivalQueue
 * section for the full evidence chain that identified `self` as
 * HelpPageNode*, not Building* or GameVehicle*).
 *
 * These are structurally similar to, but distinct compiled functions
 * from, GameVehicle::AddDestination (0x412AF0) / RemoveDestination
 * (0x412B50) — different addresses, different callers, and AddVehicle
 * additionally primes the vehicle (direction, tile position, state)
 * before enqueueing. Preserved as separate methods rather than merged,
 * per CLAUDE.md ("do not simplify assembly unless equivalence is proven
 * and documented" — these are two really-distinct binary routines that
 * happen to share the same queue-node layout).
 *
 * Class hierarchy:
 *   GameObject → Entity → ResourceGameObject
 *     → RESDATA_GameVehicle (0x11C bytes, vtable 0x478308)
 *       → HelpPageNode (0x128 bytes, vtable 0x4783D8)  ← this class
 */

#pragma once

#include "../game/ResdataGameVehicle.h"

class Vehicle;

class HelpPageNode : public RESDATA_GameVehicle {
public:
    /* ================================================================ */
    /* Singly-linked list node for the vehicle queue (mirrors           */
    /* GameVehicle::DestNode's shape; a distinct nested type since the  */
    /* two classes are unrelated siblings under RESDATA_GameVehicle).   */
    /* ================================================================ */

    struct DestNode {
        Vehicle*  vehicle;       // +0x00  Vehicle* pointer
        DestNode* next;          // +0x04  next node (NULL = tail)
    };

    /* ================================================================ */
    /* Fields (+0x11C..+0x127, extending RESDATA_GameVehicle's 0x11C)   */
    /* ================================================================ */

    int32_t    update_flag;        // +0x11C  0 = idle, 1 = update pending
    int32_t    overlay_flag;       // +0x120  1 = tutorial overlay active
    DestNode*  dest_list_head;     // +0x124  head of singly-linked vehicle
                                   //          queue (real pointer on host;
                                   //          the binary stores a 32-bit
                                   //          x86 pointer at this offset)

    /* Total object size: 0x128 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * HelpPageNode constructor. 0x44F210.
     *
     * Chains to RESDATA_GameVehicle(resource_id) for base init.
     * Sets update_flag=0, overlay_flag based on resourceId (1 for
     * 0xC42/0xC44/0xC46/0xC48, 0 otherwise), dest_list_head=0.
     * Also overrides resource_kind to 5 at +0x04 and vehicle_kind to 3
     * at +0x10C (overriding the base RESDATA_GameVehicle values).
     *
     * @param resource_id  Resource ID for the page node (0xC42-0xC48 range)
     */
    explicit HelpPageNode(int resource_id);

    /* dest_list_head owns its linked list exclusively (freed in the
     * destructor); the binary never copies a HelpPageNode (it is only
     * ever constructed via INPUT_PlaceObject and referenced through
     * pointers). Deleted rather than hand-writing a deep-copy the
     * original never performs. */
    HelpPageNode(const HelpPageNode&) = delete;
    HelpPageNode& operator=(const HelpPageNode&) = delete;

    /**
     * Destructor. 0x44F2C0.
     *
     * Frees the destination linked list at dest_list_head (+0x124),
     * then chains to ~RESDATA_GameVehicle() for base cleanup.
     */
    ~HelpPageNode() override;

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Update — Per-frame page event processor (vtable[10] override).
     * Address: 0x44F340.
     *
     * Overrides RESDATA_GameVehicle::Update. Calls Entity::Update for
     * base animation, then checks dest_list_head for pending page data.
     * If a node is queued and update_flag is 0:
     *   1. Sets update_flag = 1
     *   2. Dequeues the vehicle from the front of the list
     *   3. Frees the dequeued node
     *   4. Calls vehicle->LoadSounds, vehicle->SetState(2),
     *      vehicle->field_60 = 4
     */
    void Update() override;

    /**
     * AddVehicle — Prime a vehicle for arrival and enqueue it.
     * Address: 0x44F3A0 (NON-VIRTUAL, direct call in the binary).
     *
     * Called by World::FinalizeLoad (0x44DFFE) and the not-yet-ported
     * VehicleEditor_Update (0x44C736, 0x44C84A) whenever
     * INPUT_FindObjectAt(mode 0/1/4) resolves a destination — that
     * filter matches vehicle_kind==3, which only this class's
     * constructor ever sets (RESDATA_GameVehicle's own default is never
     * 3 on that path; GameVehicle always overrides to 4), so `self`
     * there is provably a HelpPageNode.
     *
     * Sets vehicle->direction = 2 (ARRIVING), copies this node's packed
     * tile position (tile_target(), the dword at +0x88) into
     * vehicle->tile_x/tile_y, calls vehicle->SetState(0), then appends
     * the vehicle to dest_list_head (+0x124). Disassembly confirms both
     * RET paths XOR AL,AL first (always returns 0 in the original ABI);
     * modeled as void since the caller never inspects EAX.
     *
     * @param vehicle  Vehicle to add to this node's arrival queue.
     */
    void AddVehicle(Vehicle* vehicle);

    /**
     * RemoveVehicle — Remove a vehicle from the arrival queue by ID/color.
     * Address: 0x44F410 (NON-VIRTUAL, direct call in the binary).
     *
     * Called by World_RenderAll (0x44E683) when a vehicle whose
     * direction is EDGE_OF_MAP(2) or DEPOT(3) reaches its destination
     * tile. direction==2 is set exclusively by AddVehicle above, so the
     * object World_RenderAll looks up via TileMap::GetObjectAt at that
     * point is — by construction, not by a runtime type check — the
     * same HelpPageNode the vehicle was queued on. (TileMap::GetObjectAt
     * itself applies no kind filter; a bare RESDATA_GameVehicle, 0x11C
     * bytes, reaching this call would read +0x124 out of its own
     * allocation — an original-binary hazard the assembly does not
     * guard against, not something to paper over here.)
     *
     * Searches dest_list_head for a node whose vehicle matches
     * player_id (vehicle+0x7A) and color (vehicle+0x78), unlinks and
     * frees it. player_id is compared as a zero-extended 16-bit value
     * in the original (loaded as a dword on the caller side, matched
     * against a 16-bit field), hence the uint16_t parameter here.
     *
     * @param player_id  Player ID to match.
     * @param color      Color/platform byte to match.
     * @return           1 if a vehicle was removed, 0 if not found.
     */
    uint8_t RemoveVehicle(uint16_t player_id, uint8_t color);
};

/* Compile-time size verification */
#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(HelpPageNode) == 0x128,
              "HelpPageNode size mismatch (expected 0x128)");
static_assert(offsetof(HelpPageNode, update_flag) == 0x11C,
              "HelpPageNode::update_flag offset mismatch");
static_assert(offsetof(HelpPageNode, overlay_flag) == 0x120,
              "HelpPageNode::overlay_flag offset mismatch");
static_assert(offsetof(HelpPageNode, dest_list_head) == 0x124,
              "HelpPageNode::dest_list_head offset mismatch");
#endif
