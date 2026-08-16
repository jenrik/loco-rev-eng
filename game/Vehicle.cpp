/**
 * Vehicle.cpp — Vehicle class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the Vehicle class — a 0x94-byte standalone object (vtable
 * 0x47836C) representing road vehicles (cars, trucks, buses, etc.) that
 * follow the track/road network through the town.
 *
 * Vehicle manages:
 *   - Movement along track grid via VehicleEditor sub-objects
 *   - Occupant loading/unloading at buildings
 *   - Engine sound state machine and audio channel management
 *   - Route navigation with forward/reverse direction support
 *   - Multiplayer sync and position broadcasting
 *
 * Sub-objects:
 *   - EditorState (0x20 bytes) at +0x20
 *   - VehicleEditor (0x450 bytes each, max 4) at +0x10[0..3]
 */

// Status: TRANSCRIBED

#include "Vehicle.h"
#include "../world/EditorState.h"
#include "../core/VehicleEditor.h"
#include "../network/DPlayManager.h"
#include "../network/Netman.h"
#include "GameVehicle.h"
#include <cstdio>
#include <new>

/* Forward-declared rather than including resources/resource_manager_sdl3.h
 * wholesale: that header's own ResourceManager_Init(void*) -> int
 * ambiguates against network/Netman.h's ResourceManager_Init(void*) -> void
 * the moment both are visible in one TU (this file already includes
 * Netman.h) -- same collision input/InputMgr.cpp's identical forward
 * declarations sidestep. These match resource_manager_sdl3.h's real
 * declarations exactly, so they resolve to the same already-compiled
 * symbols at link time. */
namespace loco::assets {
bool is_host_sprite_resource(const void* resource);
bool sprite_tile_type_byte(const void* resource, uint8_t* out_byte);
}  // namespace loco::assets
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void* operator_new(size_t size);                        /* 0x465CE0 */
void  GLOBAL_free(void* ptr);                           /* 0x465CD0 */

extern "C" {
    /* VehicleEditor construction — call-0/silent-wrong-stub landmine
     * (Vehicle::Vehicle's VehicleEditor_Ctor resolves to a 2-arg no-op stub
     * in shared/link_stubs.cpp despite this 4-arg declaration). Tracked in
     * docs/landmine-sweep-worklist.md and deliberately left as-is here —
     * fixing it is a separate, behavior-changing commit, not part of this
     * cast cluster. (EditorState_Ctor's equivalent landmine was closed by
     * constructing EditorState directly above instead of through that
     * free-function bridge.) */
    void* __thiscall VehicleEditor_Ctor(void* this_, int32_t param_1,
                                        int32_t param_2, uint8_t param_3);

    /* TileMap */
    int32_t __fastcall TileMap_GetObjectAt(void* tilemap, int16_t x, int16_t y, int32_t layer);
    void    __thiscall TileMap_InvalidateRect(void* tilemap, int32_t x, int32_t y,
                                              int32_t w, int32_t h);

    /* Audio */
    void __fastcall AudioChannel_Pause(uint32_t channel_id);
    void __fastcall AudioChannel_Play(uint32_t channel_id);

    /* Network */
    void __fastcall NETMAN_ReceivePing(void* netman, uint32_t player_id,
                                       uint8_t color_r, uint8_t color_g,
                                       int32_t x, int32_t y);
    void __fastcall NETMAN_ReceiveAck(void* netman, uint32_t player_id,
                                      uint8_t color_r, uint8_t color_g);

}

/* RESDATA_IsRoadTile/RESDATA_IsBuildingTile have plain C++ linkage
 * (world/tilemap.h) — were wrongly declared inside the extern "C" block
 * above, which bound them silently to shared/defsym_stubs.cpp's no-ops
 * instead of the real implementations (world/tilemap.cpp and
 * shared/stubs_impl.cpp respectively). */
uint8_t __fastcall RESDATA_IsRoadTile(int32_t tile_obj);      /* 0x44BD10 */
uint8_t __fastcall RESDATA_IsBuildingTile(int32_t tile_obj);  /* 0x44BD30 */

/* Declared in Vehicle.h (shared with core/VehicleEditor.cpp and
 * world/EditorState.cpp); see that header for the full evidence trail. */
void ClassifyResourceTile(void* resource, bool* is_road, bool* is_building)
{
#ifndef _WIN32
    if (loco::assets::is_host_sprite_resource(resource)) {
        uint8_t tile_type = 0;
        loco::assets::sprite_tile_type_byte(resource, &tile_type);
        *is_road = (tile_type == 1 || tile_type == 2 || tile_type == 3 || tile_type == 4);
        *is_building = !*is_road &&
            (tile_type == 7 || tile_type == 8 || tile_type == 9 || tile_type == 10);
        return;
    }
#endif
    int32_t resource_id = static_cast<int32_t>(reinterpret_cast<intptr_t>(resource));
    *is_road = RESDATA_IsRoadTile(resource_id) != 0;
    *is_building = RESDATA_IsBuildingTile(resource_id) != 0;
}

/* StopSound on GameObject */
void __fastcall GameObject_StopSound(void* obj, int32_t sound_idx);

/* Input (C++ linkage — INPUT_FindObjectAt is a C++ function defined in
 * input/InputMgr.cpp; it must NOT be declared inside extern "C".  The
 * original is a thiscall with ECX = &g_input_mgr (0x4A9990). */
class InputMgr;
void* INPUT_FindObjectAt(InputMgr* input_mgr, int32_t type);   /* 0x41E1F0 */

/* ================================================================== */
/* Global references                                                    */
/* ================================================================== */
class TileMap;
extern TileMap* g_tilemap;    /* TileMap singleton, 0x4AAD08 — matches
                                * network/Netman.h's declaration; a plain
                                * `void*` here conflicts with it once this
                                * TU also includes Netman.h. */
extern Netman* g_netman;      /* NetMan singleton */
extern InputMgr g_input_mgr;  /* 0x4A9990 — static InputMgr object */
class UI_Manager;
extern UI_Manager* g_tooltip_mgr;   /* Tooltip manager */

/* ================================================================== */
/* Vehicle constructor                                                  */
/* Address: 0x44BE50 (598 bytes)                                        */
/*                                                                      */
/* Called by:                                                            */
/*   NETMAN_SendSignalChange (0x43E780)                                  */
/*   World_LoadFromFile (0x44DCA1, 0x44DDAC)                             */
/*   Train_HandleConnectionSetup (0x43B2A7)                              */
/*   Train_HandleTrackBuild (0x43CE90)                                   */
/*                                                                      */
/* Creates a 0x94-byte Vehicle object. Initializes position to -1,      */
/* clears editor and occupant arrays, creates an EditorState sub-object */
/* and a VehicleEditor sub-object. For local vehicles, registers with   */
/* the network manager and sets up initial tracks. For remote vehicles, */
/* sets direction to EDGE_OF_MAP and occupancy to EMPTY.                */
/* ================================================================== */
Vehicle::Vehicle(int32_t param_1, int32_t param_2, uint8_t param_3, uint8_t param_4)
{
    /* ---- SEH frame setup (compiler-generated try/except) ---- */

    /* Initialize position tracking to -1 */
    this->tile_x = -1;              /* +0x2E */
    this->tile_y = -1;              /* +0x30 */
    this->target_tile_x = -1;       /* +0x32 */
    this->target_tile_y = -1;       /* +0x34 */

    this->init_flag = param_4;      /* +0x88 */
    this->owner_handle = param_2;   /* +0x04 */

    /* Set vtable */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Clear initial state */
    this->sound_guard = 0;          /* +0x5A */
    this->active_flag = 0;          /* +0x90 */
    this->detach_flag = 0;          /* +0x2C */
    this->editor_state_2 = nullptr; /* +0x8C */
    this->net_sync_flag = 0;        /* +0x68 */
    this->network_next = nullptr;   /* +0x70 */

    /* Clear occupant/track slots (8 int32_t at +0x38) */
    for (int i = 0; i < 8; i++) {
        this->occupant_tracks[i] = 0;
    }

    /* Clear editor array (4 slots at +0x10) */
    this->editors[0] = nullptr;
    this->editors[1] = nullptr;
    this->editors[2] = nullptr;
    this->editors[3] = nullptr;
    this->direction = 0;            /* +0x60 */

    /* Create EditorState sub-object. Original x86 EditorState is 0x20 bytes
     * (4-byte vtable ptr + one 4-byte `building` pointer); sizeof(EditorState)
     * on this 64-bit host is 0x28 (GameVehicle* widens to 8 bytes) — use
     * sizeof directly rather than the stale x86 literal to avoid an 8-byte
     * heap overflow. This also removes the EditorState_Ctor free-function
     * bridge (shared/stubs_link001_batch4_network_world.cpp), which existed
     * only to work around the undersized caller buffer. */
    EditorState* state = static_cast<EditorState*>(operator_new(sizeof(EditorState)));
    if (state != nullptr) {
        state = new (state) EditorState(static_cast<char>(param_3));
    }
    this->editor_state = state;     /* +0x20 */
    this->stop_timer = 0;           /* +0x28 */

    /* Re-clear editors (redundant, preserved from original) */
    this->editors[0] = nullptr;
    this->editors[1] = nullptr;
    this->editors[2] = nullptr;
    this->editors[3] = nullptr;
    this->editor_count = 0;         /* +0x0C */

    /* Create VehicleEditor sub-object. 0x450 was the original x86
     * sizeof(VehicleEditor); sizeof is 0x490 on this 64-bit host (pointer
     * fields widen) — use the real size. */
    VehicleEditor* vehicle_editor = static_cast<VehicleEditor*>(operator_new(sizeof(VehicleEditor)));
    if (vehicle_editor != nullptr) {
        vehicle_editor = static_cast<VehicleEditor*>(
            VehicleEditor_Ctor(vehicle_editor, param_1, 2, param_3));
    }
    this->editors[0] = vehicle_editor;   /* editors[0] = primary VehicleEditor */

    /* Check if first editor slot is active */
    VehicleEditor* active_slot = this->editors[this->editor_count];
    if (active_slot != nullptr) {
        if (active_slot->initialized == 1) {    /* GameObject::initialized, +0x18 */
            /* Store back-reference to Vehicle in editor */
            active_slot->target_building = this;

            /* Read speed parameters from editor config */
            const uint8_t* editor_param =
                reinterpret_cast<const uint8_t*>(this->editors[0]->resource);
            int16_t fwd_speed = *reinterpret_cast<const int16_t*>(editor_param + 0x7A8);  /* forward speed */
            this->max_speed = fwd_speed;        /* +0x24 */
            this->reverse_speed = *reinterpret_cast<const int16_t*>(editor_param + 0x7AA);  /* +0x26 */
            this->max_steps = fwd_speed;        /* +0x58 = forward speed */
            this->active_editor = 0;            /* +0x08 = 0 (forward) */

            this->SetState(0);                  /* STATE_STOPPED */
            this->move_timer = 0;               /* +0x36 */

            /* Set player colors based on scenario */
            if (g_netman->m_gameMode == 2) {  /* scenario 2 */
                this->color_r = static_cast<uint8_t>(g_netman->m_mySlotIndex);
                this->color_g = static_cast<uint8_t>(g_netman->m_mySlotIndex);
            } else {
                this->color_r = 1;
                this->color_g = 1;
            }

            if (param_3 == 0) {     /* local vehicle */
                /* Register player slot with netman */
                int32_t max_players = g_netman->m_field_7E8 + 1;
                g_netman->m_field_7E8 = max_players;
                this->player_id = static_cast<int16_t>(max_players);   /* +0x7A */

                this->editor_state->InitTrackAtPosition(-1, -1);

                int32_t track_idx = this->editor_state->pos_x + 0x0C;
                this->editors[0]->InitTracks(track_idx, -1);

                int16_t net_x = (track_idx < 0) ? -1 : static_cast<int16_t>(track_idx >> 4);
                NETMAN_ReceivePing(g_netman, static_cast<uint32_t>(this->player_id),
                                   this->color_r, this->color_g, static_cast<int32_t>(net_x), -1);
            }

            /* Common initialization for both local and remote */
            this->flag_89 = 0;                  /* +0x89 */
            this->flag_8A = 0;                  /* +0x8A */
            this->network_next = nullptr;       /* +0x70 */

            if (param_3 == 0) {                 /* local vehicle */
                this->occupancy = 2;            /* +0x64 = FULL */
                this->direction = 0;            /* +0x60 = FORWARD */
                this->UpdatePosition(0);        /* reverse=0 */
            } else {                            /* remote vehicle */
                this->direction = 2;            /* +0x60 = EDGE_OF_MAP */
                this->occupancy = 0;            /* +0x64 = EMPTY */
                this->UpdatePosition(1);        /* reverse=1 */
            }
        } else {
            /* Destroy inactive editor slot (scalar deleting destructor,
             * vtable[0], called with flags=1 in the original — matches
             * CleanupChildren's explicit dtor+free idiom below). */
            active_slot->~VehicleEditor();
            GLOBAL_free(active_slot);
            this->editors[this->editor_count] = nullptr;
        }
    }

    /* ---- SEH frame teardown ---- */
}

#ifndef _WIN32
/** Host-only resource-independent counterpart of Vehicle::Vehicle (0x44BE50).
 * It provides typed object/list/editor ownership for SDL_net without invoking
 * the original pointer-based Entity resource ABI. */
Vehicle::Vehicle(HostNetworkVehicleTag, int32_t resource_id)
{
    owner_handle = 1;
    active_editor = 0;
    editor_count = 0;
    for (VehicleEditor*& editor : editors) editor = nullptr;
    void* state_storage = operator_new(sizeof(EditorState));
    editor_state = state_storage != nullptr
        ? ::new (state_storage) EditorState(1) : nullptr;
    max_speed = reverse_speed = max_steps = 0;
    stop_timer = 0;
    detach_flag = 0;
    tile_x = tile_y = target_tile_x = target_tile_y = -1;
    move_timer = 0;
    for (int32_t& track : occupant_tracks) track = 0;
    sound_guard = 0;
    state = 0;
    direction = 2;
    occupancy = 0;
    net_sync_flag = 0;
    msg_box_count = 0;
    network_next = nullptr;
    tunnel_angle = field_76 = 0;
    slot_index = 0;
    _pad_79 = 0;
    network_id = 0;
    peer_index = 0;
    _pad_7D = 0;
    field_7E = field_80 = field_84 = field_86 = 0;
    field_82 = _pad_83 = 0;
    init_flag = 1;
    flag_89 = flag_8A = _pad_8B = 0;
    editor_state_2 = nullptr;
    active_flag = 0;

    void* storage = operator_new(sizeof(VehicleEditor));
    if (storage != nullptr) {
        editors[0] = ::new (storage)
            VehicleEditor(HostNetworkEditorTag{}, resource_id, 2, 1);
        editors[0]->target_building = this;
    }
}

bool Vehicle::AddHostNetworkRoute(const DPlayManager& session)
{
    if (editors[0] == nullptr || editor_count >= 3 ||
        editors[editor_count + 1] != nullptr) return false;
    void* storage = operator_new(sizeof(VehicleEditor));
    if (storage == nullptr) return false;
    auto* editor = ::new (storage)
        VehicleEditor(HostNetworkEditorTag{}, 0x1871, 4, 1);
    editor->target_building = this;
    if (!editor->SetDPlayData(&session)) {
        editor->target_building = nullptr;
        editor->~VehicleEditor();
        GLOBAL_free(editor);
        return false;
    }
    editors[++editor_count] = editor;
    return true;
}
#endif

/* ================================================================== */
/* Vehicle::scalar deleting destructor (vtable[0])                     */
/* Address: 0x44C0B0                                                   */
/*                                                                      */
/* Called by: MSVC runtime virtual dispatch through VTBL_VEHICLE[0]     */
/* ================================================================== */
Vehicle::~Vehicle()
{
    CleanupChildren();
}

/* ================================================================== */
/* Vehicle::CleanupChildren                                            */
/* Address: 0x44C0D0                                                    */
/*                                                                      */
/* Called by: scalar deleting destructor                                 */
/*                                                                      */
/* Resets vtable, sends NETMAN ack if not initialized, destroys all     */
/* VehicleEditor children, destroys editor state sub-object.            */
/* ================================================================== */
void Vehicle::CleanupChildren()
{
    if (g_netman != nullptr && this->init_flag == 0) {
        NETMAN_ReceiveAck(g_netman, this->player_id,
                          this->color_r, this->color_g);
    }
    const uint16_t count = this->editor_count < 4
        ? static_cast<uint16_t>(this->editor_count) : 3;
    for (uint16_t index = 0; index <= count; ++index) {
        VehicleEditor* editor = this->editors[index];
        if (editor == nullptr) continue;
        /* target_building (Vehicle* backref, core/VehicleEditor.h) is left
         * as-is here, matching the original (0x44C0D0 calls the editor's
         * destructor directly with no field reset first) -- a prior pass
         * nulled it out here under _WIN32 only, as a workaround for
         * ~VehicleEditor() wrongly casting this field to Building*. Now
         * that the destructor reads the real Vehicle::init_flag field
         * through the correct type (see ~VehicleEditor), that workaround
         * would just diverge host behavior from the original for no
         * reason, so it's removed. */
        editor->~VehicleEditor();
        GLOBAL_free(editor);
        this->editors[index] = nullptr;
    }
    this->editor_count = 0;
    if (this->editor_state != nullptr) {
        this->editor_state->~EditorState();
        GLOBAL_free(this->editor_state);
        this->editor_state = nullptr;
    }
}

/* ================================================================== */
/* Vehicle::InitOccupant                                                */
/* Address: 0x44C150 (32 bytes)                                         */
/*                                                                      */
/* Called by: GameVehicle::StartMoving (0x4129C0)                       */
/*                                                                      */
/* Sets occupant mode. If mode==2 (waiting), updates position with      */
/* reverse=0. Otherwise updates with reverse=1 (loading state).         */
/* ================================================================== */
void Vehicle::InitOccupant(int32_t mode)
{
    this->occupancy = mode;          /* +0x64 */
    if (mode != 2) {
        this->UpdatePosition(1);     /* loading */
    } else {
        this->UpdatePosition(0);     /* waiting */
    }
}

/* ================================================================== */
/* Vehicle::FindPath                                                    */
/* Address: 0x44C170 (174 bytes)                                        */
/*                                                                      */
/* Called by: GameVehicle::Update (0x412A80)                            */
/*                                                                      */
/* Registers this vehicle as an occupant of the target building. If     */
/* another vehicle is already en route, adds to destination queue.      */
/* Sets occupancy to ARRIVING (5), editors to state 5, and calls        */
/* LoadSounds for audio configuration.                                  */
/* ================================================================== */
void Vehicle::FindPath(int32_t* target, uint8_t is_remote)
{
    /* `target` is always a GameVehicle* at runtime — offsets +0x11C/+0x120/
     * +0x128 read/written below only exist on GameVehicle (Building tops out
     * at 0xF4 bytes); World::LoadFromFile passes an INPUT_FindObjectAt(3)
     * result reinterpret_cast to int32_t*, World_SerializeMap and
     * GameVehicle::Update pass a GameVehicle* directly. The parameter itself
     * stays int32_t* — tests/persistence_fixtures.h:286 fixtures this exact
     * signature, and World::LoadFromFile's own "route_data" param (also
     * int32_t*) is forwarded straight into this call, so retyping the
     * signature would break real, distinct callers. The cast is local. */
    GameVehicle* gv = reinterpret_cast<GameVehicle*>(target);

    /* Store target tile position. The original does a single 32-bit store
     * spanning target_tile_x (+0x32) and target_tile_y (+0x34) — the two
     * fields are adjacent int16_t with no padding, so two int16 stores are
     * byte-identical to the dword store (confirmed via disassembly of
     * 0x44C170: `MOV dword ptr [ESI+0x32], EAX` from GameVehicle's packed
     * sub_pos_x/sub_pos_y at +0x88). The previous version of this line only
     * copied the low half (target_tile_x), silently leaving target_tile_y
     * stale — a real bug, not just a style issue. */
    this->target_tile_x = gv->sub_pos_x;
    this->target_tile_y = gv->sub_pos_y;

    /* If another vehicle already assigned, add to destination queue */
    if (gv->current_vehicle != nullptr && gv->current_vehicle != this) {
        gv->AddDestination(this);
        return;
    }

    /* Register as occupant */
    gv->busy_flag = 1;
    gv->current_vehicle = this;
    this->occupancy = 5;                         /* ARRIVING */
    this->UpdatePosition(1);

    /* Set all editors and their wheels to ARRIVING state (5) */
    for (uint32_t i = 0; i <= static_cast<uint32_t>(this->editor_count); i++) {
        VehicleEditor* editor = this->editors[i];
        if (editor != nullptr) {
            editor->edge_dir_b = 5;              /* editor state, +0x444 */
            editor->end_a->edit_state = 5;        /* front wheel state */
            editor->end_b->edit_state = 5;        /* rear wheel state */
        }
    }

    this->SetState(2);                  /* MOVING */
    this->UpdatePosition(1);
    this->LoadSounds(target, is_remote);
    gv->occupant_state = 0;              /* clear target's occupant count */
}

/* ================================================================== */
/* Vehicle::InitRoute                                                   */
/* Address: 0x44C220 (232 bytes)                                        */
/*                                                                      */
/* Appends a new route segment by creating a VehicleEditor. Max 3       */
/* additional segments (total 4). Returns 1 on success, 0 on failure.   */
/* ================================================================== */
uint32_t Vehicle::InitRoute(int32_t param_1, int32_t param_2, uint8_t param_3)
{
    /* ---- SEH frame setup ---- */

    uint16_t count = this->editor_count;
    uint32_t result = static_cast<uint32_t>(static_cast<int16_t>(count));  /* preserve for return on failure */

    if (count < 3 && this->editors[count + 1] == nullptr) {
        this->editor_count = static_cast<int16_t>(count + 1);

        /* 0x450 was the original x86 sizeof(VehicleEditor); use the real
         * host size (0x490 — see core/VehicleEditor.h). */
        void* editor = operator_new(sizeof(VehicleEditor));
        if (editor != nullptr) {
            /* VehicleEditor_Ctor is a documented call-0/silent-wrong-stub
             * landmine (docs/landmine-sweep-worklist.md) — left as-is here,
             * fixed in a separate commit. */
            editor = VehicleEditor_Ctor(editor, param_1, param_2, param_3);
        }
        this->editors[this->editor_count] = static_cast<VehicleEditor*>(editor);

        result = 1;
        VehicleEditor* new_slot = this->editors[this->editor_count];
        if (new_slot != nullptr) {
            if (new_slot->initialized == 1) {    /* GameObject::initialized, +0x18 */
                new_slot->target_building = this;  /* backref */
                /* ---- SEH teardown ---- */
                return result;
            }
            /* Editor inactive — destroy it */
            new_slot->~VehicleEditor();
            GLOBAL_free(new_slot);
            this->editors[this->editor_count] = nullptr;
        }
        this->editor_count = static_cast<int16_t>(this->editor_count - 1);
        result = 0;
    }

    /* ---- SEH teardown ---- */
    return result;
}

/* ================================================================== */
/* Vehicle::RemoveEditor                                                */
/* Address: 0x44C310 (93 bytes)                                         */
/*                                                                      */
/* Destroys the editor at editors[index], shifts remaining slots left,  */
/* decrements editorCount. Returns 1 on success, 0 on failure.          */
/* ================================================================== */
int32_t Vehicle::RemoveEditor(uint32_t index)
{
    if (index > 3) {
        return 0;
    }

    VehicleEditor* editor = this->editors[index];
    if (editor == nullptr) {
        return 0;
    }

    /* Destroy editor via scalar deleting destructor */
    editor->~VehicleEditor();
    GLOBAL_free(editor);
    this->editors[index] = nullptr;
    this->editor_count--;

    /* Shift remaining slots left if not the last slot */
    if (index < 3) {
        for (uint32_t i = index; i < 3; i++) {
            this->editors[i] = this->editors[i + 1];
            this->editors[i + 1] = nullptr;
        }
    }

    return 1;
}

/* ================================================================== */
/* Vehicle::GetOccupantCount                                            */
/* Address: 0x44C370 (48 bytes)                                         */
/*                                                                      */
/* Checks editors[1..3] for an occupant with state 2 at +0x42C.         */
/* Returns 1 if found, 0 otherwise.                                     */
/*                                                                      */
/* NOTE: Only checks slots 1-3, skipping slot 0 deliberately.           */
/* ================================================================== */
uint8_t Vehicle::GetOccupantCount()
{
    for (uint32_t i = 1; i <= 3; i++) {
        VehicleEditor* editor = this->editors[i];
        if (editor != nullptr) {
            /* res_id_2 (+0x42C) doubles as a direction/occupant code at
             * this call site — see VehicleEditor.h's own hedge on the
             * field's dual meaning (ctor param_2 vs. runtime reuse). */
            if (editor->res_id_2 == 2) {
                return 1;
            }
        }
    }
    return 0;
}

/* ================================================================== */
/* Vehicle::ClearRoute                                                  */
/* Address: 0x44C9B0 (151 bytes)                                        */
/*                                                                      */
/* Clears the movement route if the relevant wheel's state is IDLE (0)  */
/* and the target building state is not 4. Resets occupancy, clears     */
/* tracked vehicle, and resets target tile coordinates to -1.           */
/* ================================================================== */
void Vehicle::ClearRoute()
{
    /* Select wheel based on direction */
    EditorState* wheel;
    if (this->active_editor == 0) {
        /* Forward: use last editor's rear wheel */
        wheel = this->editors[this->editor_count]->end_b;
    } else {
        /* Reverse: use first editor's front wheel */
        wheel = this->editors[0]->end_a;
    }

    /* Check if route can be cleared:
       - wheel state is IDLE (0)
       - target vehicle_kind != 4 (+0x10C)
       Note: original code does NOT null-check the target pointer —
       it is guaranteed valid by prior state checks.

       wheel->building is GameVehicle* (world/EditorState.h) — resolved
       cluster-wide; no cast needed at this call site anymore. */
    int32_t wheel_state = wheel->edit_state;
    GameVehicle* target = wheel->building;
    if (wheel_state == 0 && target != nullptr && target->vehicle_kind != 4) {

        /* Can clear route */
        this->occupancy = 0;                 /* +0x64 = EMPTY */
        this->UpdatePosition(1);

        /* Clear tracked vehicle on destination building. TileMap_GetObjectAt
         * returns int32_t (a pointer truncated to 32 bits on a 64-bit host)
         * — pre-existing, not introduced or fixed here; preserved via the
         * same uintptr_t round-trip already used elsewhere in this file. */
        GameVehicle* obj_at = reinterpret_cast<GameVehicle*>(static_cast<uintptr_t>(TileMap_GetObjectAt(
            g_tilemap, this->target_tile_x, this->target_tile_y + 1, 0)));
        if (obj_at != nullptr) {
            obj_at->occupant_state = 0;      /* clear arrival queue ptr, +0x11C */
            obj_at->busy_flag = 0;           /* clear tracked_vehicle_flag, +0x128 */
            obj_at->current_vehicle = nullptr;  /* clear tracked_vehicle, +0x120 */
        }

        this->target_tile_x = -1;
        this->target_tile_y = -1;
    }
}

/* ================================================================== */
/* Vehicle::HandleStop                                                  */
/* Address: 0x44CA50 (94 bytes)                                         */
/*                                                                      */
/* Checks target building state at +0x110:                              */
/*   - State 1 (loading): plays engine sound, sets MOVING               */
/*   - State 2 (waiting): sets APPROACHING if not already               */
/*   - State 0 (empty): sets MOVING                                     */
/* ================================================================== */
uint8_t Vehicle::HandleStop()
{
    if (this->state == 0) {                 /* already stopped */
        return 0;
    }

    GameVehicle* target = this->editor_state->building;
    if (target == nullptr) {
        return 0;
    }

    int32_t target_state = target->init_state;   /* +0x110 */

    if (target_state == 1) {
        /* Loading state — play engine sound then set MOVING */
        if (this->sound_guard == 0) {
            this->sound_guard = 1;
            this->UpdateEngineSound();
            this->sound_guard = 0;
        }
        this->SetState(2);   /* MOVING */
        return 1;
    }

    if (target_state == 2) {
        /* Waiting state — approach if not already */
        if (this->state != 1) {
            this->SetState(1);   /* APPROACHING */
        }
        return 0;
    }

    /* target_state == 0 (empty) — move along */
    this->SetState(2);   /* MOVING */
    return 1;
}

/* ================================================================== */
/* Vehicle::DetachAll                                                   */
/* Address: 0x44CAB0 (62 bytes)                                         */
/*                                                                      */
/* Scans all editors for a sub-object with DETACHED state (+0x448==1). */
/* ================================================================== */
uint8_t Vehicle::DetachAll()
{
    this->detach_flag = 0;   /* +0x2C */

    for (int32_t i = 0; i <= static_cast<int32_t>(this->editor_count); i++) {
        VehicleEditor* editor = this->editors[i];
        if (editor->bound_check_flag == 1) {   /* DETACHED state, +0x448 */
            this->detach_flag = 1;
            return 1;
        }
    }

    return this->detach_flag;
}

/* ================================================================== */
/* Vehicle::ResetState                                                  */
/* Address: 0x44CAF0 (31 bytes)                                         */
/*                                                                      */
/* Calls UpdateEngineSound with reentrancy protection.                  */
/* ================================================================== */
uint8_t Vehicle::ResetState()
{
    if (this->sound_guard != 0) {
        return 0;   /* reentrancy guard active */
    }

    this->sound_guard = 1;
    uint8_t result = this->UpdateEngineSound();
    this->sound_guard = 0;

    return result;
}

/* ================================================================== */
/* Vehicle::UpdateEngineSound                                           */
/* Address: 0x44CB10 (714 bytes)                                        */
/*                                                                      */
/* Full engine sound and state machine update. Toggles direction and    */
/* active_editor flags. Cycles editor exclusion flags and state codes.  */
/* Copies editor state, runs 12 placement iterations, checks bounds,    */
/* and determines new occupancy mode.                                   */
/*                                                                      */
/* State machine on editor state code (+0x444):                         */
/*   1 -> 4 (exclusion rotate), 2 -> 5 (clear visible)                 */
/*   4 -> 1 (check edit bounds), 5 -> 1 or 2 (detach check)            */
/* ================================================================== */
uint8_t Vehicle::UpdateEngineSound()
{
    /* Skip if occupancy is FULL (2) or direction is EDGE (2) / DEPOT (3) */
    if (this->occupancy == 2 || this->direction == 2 || this->direction == 3) {
        return 0;
    }

    /* Toggle direction between REVERSE (1) and ALT_FRONT (4) */
    if (this->direction == 1) {
        this->direction = 4;
    } else if (this->direction == 4) {
        this->direction = 1;
    }

    /* Toggle active_editor flag */
    this->active_editor = (this->active_editor == 0) ? 1 : 0;

    /* --- Process each editor --- */
    for (int32_t editor_idx = 0;
         editor_idx <= static_cast<int32_t>(static_cast<uint32_t>(this->editor_count));
         editor_idx++) {

        VehicleEditor* editor = this->editors[editor_idx];

        /* Update wheel edit modes via the real typed method
         * (EditorState::UpdateEditMode, 0x40CD60). The previous version of
         * this call went through an extern "C" free-function declaration
         * (VehicleEditor_UpdateEditMode) that collided, by name only, with
         * an unrelated 3-arg no-op stub in shared/link_stubs.cpp — a
         * silent-wrong-stub landmine that made every per-frame wheel
         * edit-mode update in this loop do nothing. */
        editor->end_a->UpdateEditMode(this);  /* front wheel */
        editor->end_b->UpdateEditMode(this);  /* rear wheel */

        /* State machine on editor exclusion flag (edge_dir_a, +0x440) */
        int32_t excl_flag = editor->edge_dir_a;
        if (excl_flag > 0) {
            if (excl_flag < 3) {
                editor->edge_dir_a = 4;
            } else if (excl_flag == 4) {
                editor->edge_dir_a = 1;
            }
        }

        /* If front or rear wheel has state 1 (ACTIVE), set exclusion to 1 */
        if (editor->end_a->move_state == 1 || editor->end_b->move_state == 1) {
            editor->edge_dir_a = 1;
        }

        /* State machine on editor state code (edge_dir_b, +0x444) */
        int32_t state_code = editor->edge_dir_b;
        switch (state_code) {
        case 1:
            editor->edge_dir_b = 4;
            break;
        case 2:
            editor->edge_dir_b = 5;
            editor->visible = 0;    /* clear visible */
            break;
        case 4:
            editor->edge_dir_b = 1;
            editor->CheckEditBounds1(this);
            break;
        case 5:
            if (editor->end_a->edit_state == 1 || editor->end_b->edit_state == 1) {
                /* Detached — goto case 4 logic */
                editor->edge_dir_b = 1;
                editor->CheckEditBounds1(this);
            } else {
                editor->edge_dir_b = 2;
                editor->visible = 0;    /* clear visible */
            }
            break;
        }
    }

    /* --- Copy editor state and run placement iteration ---
     * this->editor_state->building is GameVehicle* (world/EditorState.h);
     * re-read after each Copy() below since Copy() overwrites the
     * building field too. */
    GameVehicle* editor_target = this->editor_state->building;
    editor_target->counter_timer--;  /* +0x114 */

    if (this->active_editor == 1) {
        /* ActiveEditor 1: copy from rear wheel of last editor */
        this->editor_state->Copy(this->editors[this->editor_count]->end_b);

        editor_target = this->editor_state->building;
        editor_target->counter_timer++;

        /* Run exactly 12 placement iterations */
        for (int iter = 0; iter < 12; iter++) {
            this->editor_state->UpdateVehiclePlacement(this);
        }
    } else {
        /* Forward: copy from front wheel of first editor */
        this->editor_state->Copy(this->editors[0]->end_a);

        editor_target = this->editor_state->building;
        editor_target->counter_timer++;

        /* Run placement iterations, stop if failure, max 12 */
        for (int iter = 0; iter < 12; iter++) {
            uint32_t result = this->editor_state->UpdateVehiclePlacement(this);
            if (result == 0) break;
        }
    }

    /* Post-placement state adjustment */
    if (this->direction == 4 && this->editor_state->move_state == 2) {
        this->editor_state->move_state = 4;
    }

    this->editor_state->CheckBounds();
    this->editor_state->CheckBounds2();

    /* --- Handle editor state result (+0x1C) --- */
    switch (this->editor_state->edit_state) {
    case 0: {
        /* Check if first and last editor have exclusion state 4 or 5 */
        int32_t first_state = this->editors[0]->edge_dir_b;
        int32_t last_state = this->editors[this->editor_count]->edge_dir_b;
        if (first_state != 4 && first_state != 5 &&
            last_state != 4 && last_state != 5) {
            /* No excluded editors */
            if (this->occupancy == 0) {
                return 0;   /* EMPTY — nothing more to do */
            }

            /* Clear target building's tracked vehicle. TileMap_GetObjectAt
             * returns int32_t (pointer truncated on a 64-bit host) —
             * pre-existing, preserved via the same uintptr_t round-trip
             * used elsewhere in this file. */
            GameVehicle* target_building = reinterpret_cast<GameVehicle*>(static_cast<uintptr_t>(TileMap_GetObjectAt(
                g_tilemap, this->target_tile_x, this->target_tile_y + 1, 0)));
            if (target_building != nullptr && target_building->current_vehicle == this) {
                target_building->occupant_state = 0;   /* clear occupant count, +0x11C */
                target_building->busy_flag = 0;         /* clear occupant flag, +0x128 */
                /* vtable[7] = StopSound(int). The previous version of this
                 * call dropped the +0x1C vtable-slot offset and instead
                 * called vtable[0] (the scalar deleting destructor!) with
                 * arg 0 — confirmed via disassembly of the equivalent
                 * LoadSounds site (0x44CE74: CALL dword ptr [EDX+0x1c]) and
                 * a raw read of VehicleEditor's vtable at 0x477590
                 * (slot 7 @ +0x1C = 0x405A20 = Entity::StopSound). */
                target_building->StopSound(0);
                target_building->current_vehicle = nullptr;  /* clear tracked vehicle, +0x120 */
            }

            this->target_tile_x = -1;
            this->target_tile_y = -1;
            this->occupancy = 0;                         /* EMPTY */
            this->UpdatePosition(1);
            return 0;
        }
        /* Fall through: excluded editors found — set occupancy = STOPPING (4) */
    }
    /* fallthrough */
    case 4:
    case 5:
        this->occupancy = 4;    /* STOPPING */
        this->UpdatePosition(1);
        return 0;

    case 1:
    case 2:
        this->occupancy = 1;    /* DEPARTING */
        this->UpdatePosition(1);
        return 0;

    default:
        return 0;
    }
}

/* ================================================================== */
/* Vehicle::LoadSounds                                                  */
/* Address: 0x44CE10 (1631 bytes)                                       */
/*                                                                      */
/* Configures sound positions for all VehicleEditor sub-objects based   */
/* on tile type category. Supports road (2) and building (1) tile       */
/* types with N/S/W/E facing directions. Handles both forward and       */
/* reverse editor positioning.                                          */
/* ================================================================== */
uint8_t Vehicle::LoadSounds(int32_t* target, uint8_t param_2)
{
    if (target == nullptr) {
        return 0;
    }

    /* `target` is always a GameVehicle* — see FindPath's note. Local cast,
     * signature unchanged (this method's own callers, e.g. FindPath, pass
     * the same untyped int32_t* through). */
    GameVehicle* gv = reinterpret_cast<GameVehicle*>(target);

    /* Host deviation: gv->resource is a loco::assets::SpriteResource* on
     * this build, not the real x86 TileMapResource* the rest of this
     * function's raw +0x63A/+0x636/+0x630 offset reads assume (same
     * "raw fixed-offset reads against undersized host resource objects"
     * landmine already fixed in ScrollRect/InputMgr.cpp/
     * ResdataGameVehicle.cpp). The previous version of this line also
     * truncated the pointer through int32_t before ever reaching those
     * reads -- a second, independent bug (same class as the
     * tile-pointer-registry fix): on this 64-bit host a real heap
     * address does not survive that round-trip, so every downstream read
     * was already dereferencing a garbage address regardless of the
     * layout mismatch. Kept as a real, untruncated pointer; the resolved
     * tile_type byte (where a host source exists) is read once via the
     * existing sprite_tile_type_byte() accessor instead of re-deriving it
     * through the raw offset at each of this function's three read sites. */
    void* resource = gv->resource;   /* +0x40 */
    uint8_t tile_type = 0;
#ifndef _WIN32
    bool resource_is_host_sprite = loco::assets::is_host_sprite_resource(resource);
    if (resource_is_host_sprite) {
        loco::assets::sprite_tile_type_byte(resource, &tile_type);
    } else
#endif
    {
        tile_type = *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(resource) + 0x63A);
    }
    int32_t tile_category = 0;

    bool is_road_tile, is_building_tile;
    ClassifyResourceTile(resource, &is_road_tile, &is_building_tile);

    if (is_road_tile) {
        tile_category = 2;                       /* ROAD */
        /* Original does a single 32-bit store spanning tile_x (+0x2E) and
         * tile_y (+0x30) from gv's packed sub_pos_x/sub_pos_y (+0x88); the
         * two Vehicle fields are adjacent int16_t with no padding, so two
         * int16 stores are byte-identical. The previous version of this
         * line only copied the low half (tile_x), silently leaving tile_y
         * stale — a real bug, not just a style issue (same class as the
         * one already fixed in FindPath above). */
        this->tile_x = gv->sub_pos_x;
        this->tile_y = gv->sub_pos_y;
    } else if (is_building_tile) {
        tile_category = 1;                       /* BUILDING */
        /* vtable[7] = StopSound(int). The previous version of this call
         * dropped the +0x1C vtable-slot offset and instead called
         * vtable[0] (the scalar deleting destructor!) with arg 1 —
         * confirmed via disassembly of this exact site (0x44CE74:
         * CALL dword ptr [EDX+0x1c], EDX = *EBP = gv's vtable pointer) and
         * a raw read of VehicleEditor's vtable at 0x477590 (slot 7 @
         * +0x1C = 0x405A20 = Entity::StopSound). */
        gv->StopSound(1);
    }

    /* Host deviation: no source exists for RESDATA+0x636 (the track
     * control-point index seeding a wheel's initial track_pos) --
     * world/EditorState.cpp's own header comment already records this
     * offset as "never routed through a named struct member," and
     * nothing in SpriteMetadata maps to it. Guard rather than guess,
     * matching Entity::Update's precedent: on host, warn once and leave
     * track_pos at whatever it already holds (EditorState's constructor
     * default is 0) instead of reading raw bytes at an address that
     * isn't the real TileMapResource on this build. */
    auto host_track_pos = [&](int32_t current_value) -> int32_t {
#ifndef _WIN32
        if (resource_is_host_sprite) {
            static bool warned = false;
            if (!warned) {
                warned = true;
                std::fprintf(stderr,
                    "[HOST] Vehicle::LoadSounds: no host source for "
                    "RESDATA+0x636 (track control-point index) -- "
                    "leaving track_pos at its current value\n");
                std::fflush(stderr);
            }
            return current_value;
        }
#endif
        return *reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(resource) + 0x636) - 1;
    };

    /* --- Configure sound positions for each editor --- */
    for (int32_t i = 0; i <= static_cast<int32_t>(static_cast<uint32_t>(this->editor_count)); i++) {
        VehicleEditor* editor = this->editors[i];
        EditorState* front_wheel = editor->end_a;
        EditorState* rear_wheel  = editor->end_b;

        /* Set target reference on both wheels */
        front_wheel->building = gv;
        rear_wheel->building  = gv;

        if (tile_category == 2) {       /* ROAD */
            editor->edge_dir_a = 4;
            front_wheel->move_state = 4;
            rear_wheel->move_state  = 4;
        } else {                        /* BUILDING or default */
            front_wheel->edit_state = 5;
            rear_wheel->edit_state  = 5;
        }

        /* tile_type resolved once, above (host-safe). */
        if (tile_type == 1 || tile_type == 7) {
            /* North-facing */
            if (param_2 == 0) {
                editor->angle_frame =
                    (this->active_editor == 0) ? 0x40 : 0;
            }
            front_wheel->direction  = 0;
            front_wheel->track_pos  = host_track_pos(front_wheel->track_pos);
            rear_wheel->direction   = 0;
            rear_wheel->track_pos   = host_track_pos(rear_wheel->track_pos);
        } else if (tile_type == 2 || tile_type == 8) {
            /* South-facing */
            if (param_2 == 0) {
                editor->angle_frame =
                    (this->active_editor == 0) ? 0 : 0x40;
            }
            front_wheel->direction = 1;
            front_wheel->track_pos = 1;
            rear_wheel->direction  = 1;
            rear_wheel->track_pos  = 1;
        } else if (tile_type == 3 || tile_type == 9) {
            /* West-facing */
            if (param_2 == 0) {
                editor->angle_frame =
                    (this->active_editor == 0) ? 0x20 : 0x60;
            }
            front_wheel->direction = 1;
            front_wheel->track_pos = 1;
            rear_wheel->direction  = 1;
            rear_wheel->track_pos  = 1;
        } else if (tile_type == 4 || tile_type == 10) {
            /* East-facing */
            if (param_2 == 0) {
                editor->angle_frame =
                    (this->active_editor == 0) ? 0x60 : 0x20;
            }
            front_wheel->direction = 0;
            front_wheel->track_pos = host_track_pos(front_wheel->track_pos);
            rear_wheel->direction  = 0;
            rear_wheel->track_pos  = host_track_pos(rear_wheel->track_pos);
        }
        /* else: no special handling for other tile types */

        /* Re-set target on wheels (redundant, preserved from original) */
        front_wheel->building = gv;
        rear_wheel->building  = gv;
    }

    /* --- Set editor state position --- */
    EditorState* editor_state = this->editor_state;
    editor_state->building = gv;

    /* Copy position from front wheel of first editor to editor state.
     * NOTE: despite the original's own "copy pos X"/"copy pos Y" framing,
     * the fields actually touched (+0x04/+0x08) are direction/track_pos,
     * not pos_x(+0x0C)/pos_y(+0x10) — preserved exactly (mislabeled
     * comment predates this rewrite, not introduced by it). */
    EditorState* first_front_wheel = this->editors[0]->end_a;
    editor_state->direction = first_front_wheel->direction;   /* +0x04 */
    editor_state->track_pos = first_front_wheel->track_pos;   /* +0x08 */

    /* Calculate world position from target. editor_state->building is
     * GameVehicle* (== gv == target); target_res/track_table stay raw
     * int32_t* since they point into an opaque RESDATA blob, not a class
     * we've reconstructed.
     *
     * Host deviation: same "no host source" gap as host_track_pos()
     * above, one level further -- RESDATA+0x630 (the track
     * control-point table track_pos indexes into) has no host mapping
     * either. Guard, don't guess: on host, leave pos_x/pos_y at their
     * current values (editor_state was just constructed, so 0) instead
     * of dereferencing a table that doesn't exist at this address. */
#ifndef _WIN32
    if (resource_is_host_sprite) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr,
                "[HOST] Vehicle::LoadSounds: no host source for "
                "RESDATA+0x630 (track control-point table) -- leaving "
                "editor_state pos_x/pos_y at their current values\n");
            std::fflush(stderr);
        }
    } else
#endif
    {
        int32_t* target_res = reinterpret_cast<int32_t*>(resource);
        int32_t* track_table = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(target_res) + 0x630);

        editor_state->pos_x =
            static_cast<int32_t>(*reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(track_table) +
                first_front_wheel->track_pos * 4)) +
            gv->sub_pos_x * 0x10;
        editor_state->pos_y =
            static_cast<int32_t>(*reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(track_table) + 2 +
                first_front_wheel->track_pos * 4)) +
            gv->sub_pos_y * 0x10;
    }

    /* Set editor state code based on tile category */
    if (tile_category == 2) {
        editor_state->move_state = 4;
    } else {
        editor_state->edit_state = 5;
    }

    /* --- Position vehicle editors relative to editor state --- */
    if (this->active_editor != 0) {
        /* ActiveEditor set — position editors from last backward.
         *
         * `ed_idx` here is the same loop counter used both to index
         * `this->editors[]` and, each iteration, to test "is this the
         * first (highest-index) pass": the previous version of this
         * function froze that test into a `max_idx` local computed once
         * before the loop and never updated, so `max_idx ==
         * this->editor_count` was always true (editor_count doesn't
         * change either) — a tautology that made the `else` branch below
         * permanently unreachable. Confirmed against disassembly of
         * 0x44CE10 (`DEC EDX` / re-read of `[ESI+0xc]` each iteration):
         * the real comparison is against the live, decrementing loop
         * variable. This changes runtime behavior on any vehicle with
         * more than one VehicleEditor (a train with multiple cars) —
         * flagged explicitly, not a silent simplification. */
        int32_t offset = 0;
        int32_t ed_idx = static_cast<int32_t>(this->editor_count);

        do {
            VehicleEditor* ed = this->editors[ed_idx];
            /* tile_type resolved once, host-safe, near the top of this function. */
            switch (tile_type) {
            case 1:
            case 7:
                /* North: rear wheel above front */
                if (ed_idx == static_cast<int32_t>(this->editor_count)) {
                    offset = editor_state->pos_x - 0x0C;
                } else {
                    /* Anchor is always the *last* editor's rear wheel
                     * (editor_count, not ed_idx+1) — matches the binary
                     * exactly (0x44CE10 re-reads editors[editor_count]
                     * every iteration, not the previous loop index). */
                    EditorState* prev_rear = this->editors[this->editor_count]->end_b;
                    offset = prev_rear->pos_x - offset;
                }
                ed->end_b->pos_x = offset;
                ed->end_b->pos_y = editor_state->pos_y;
                ed->end_a->pos_x = ed->end_b->pos_x - 0x16;
                ed->end_a->pos_y = editor_state->pos_y;
                break;
            case 2:
            case 8:
                /* South: rear wheel below front */
                if (ed_idx == static_cast<int32_t>(this->editor_count)) {
                    offset = editor_state->pos_x + 0x0C;
                } else {
                    EditorState* prev_rear = this->editors[this->editor_count]->end_b;
                    offset = prev_rear->pos_x + offset;
                }
                ed->end_b->pos_x = offset;
                ed->end_b->pos_y = editor_state->pos_y;
                ed->end_a->pos_x = ed->end_b->pos_x + 0x16;
                ed->end_a->pos_y = editor_state->pos_y;
                break;
            case 3:
            case 9:
                /* West: rear wheel left of front */
                if (ed_idx == static_cast<int32_t>(this->editor_count)) {
                    offset = editor_state->pos_y - 0x0C;
                } else {
                    EditorState* prev_rear = this->editors[this->editor_count]->end_b;
                    offset = prev_rear->pos_y - offset;
                }
                ed->end_b->pos_y = offset;
                ed->end_b->pos_x = editor_state->pos_x;
                ed->end_a->pos_x = editor_state->pos_x;
                ed->end_a->pos_y = ed->end_b->pos_y - 0x16;
                break;
            case 4:
            case 10:
                /* East: rear wheel right of front */
                if (ed_idx == static_cast<int32_t>(this->editor_count)) {
                    offset = editor_state->pos_y + 0x0C;
                } else {
                    EditorState* prev_rear = this->editors[this->editor_count]->end_b;
                    offset = prev_rear->pos_y + offset;
                }
                ed->end_b->pos_y = offset;
                ed->end_b->pos_x = editor_state->pos_x;
                ed->end_a->pos_x = editor_state->pos_x;
                ed->end_a->pos_y = ed->end_b->pos_y + 0x16;
                break;
            }

            offset += 0x26;
            ed_idx--;
        } while (ed_idx >= 0);
    } else {
        /* Forward — position editors sequentially */
        for (int32_t i = 0;
             i <= static_cast<int32_t>(static_cast<uint32_t>(this->editor_count));
             i++) {

            VehicleEditor* editor = this->editors[i];
            EditorState* front_wheel_ed = editor->end_a;
            EditorState* rear_wheel_ed  = editor->end_b;
            int32_t offset = i * 0x26;

            /* tile_type resolved once, host-safe, near the top of this function. */
            if (tile_type == 1 || tile_type == 7) {
                /* North: front wheel above, rear wheel below */
                int32_t front_pos = (i == 0)
                    ? editor_state->pos_x - 0x0C
                    : this->editors[0]->end_a->pos_x + offset * -1;
                front_wheel_ed->pos_x = front_pos;
                front_wheel_ed->pos_y = editor_state->pos_y;
                rear_wheel_ed->pos_x = front_wheel_ed->pos_x - 0x16;
                rear_wheel_ed->pos_y = editor_state->pos_y;
            } else if (tile_type == 2 || tile_type == 8) {
                /* South: front wheel below, rear wheel above */
                int32_t front_pos = (i == 0)
                    ? editor_state->pos_x + 0x0C
                    : this->editors[0]->end_a->pos_x + offset;
                front_wheel_ed->pos_x = front_pos;
                front_wheel_ed->pos_y = editor_state->pos_y;
                rear_wheel_ed->pos_x = front_wheel_ed->pos_x + 0x16;
                rear_wheel_ed->pos_y = editor_state->pos_y;
            } else if (tile_type == 3 || tile_type == 9) {
                /* West: front wheel left, rear wheel right */
                int32_t front_pos = (i == 0)
                    ? editor_state->pos_y - 0x0C
                    : this->editors[0]->end_a->pos_y + offset * -1;
                front_wheel_ed->pos_y = front_pos;
                front_wheel_ed->pos_x = editor_state->pos_x;
                rear_wheel_ed->pos_x = editor_state->pos_x;
                rear_wheel_ed->pos_y = front_wheel_ed->pos_y - 0x16;
            } else if (tile_type == 4 || tile_type == 10) {
                /* East: front wheel right, rear wheel left */
                int32_t front_pos = (i == 0)
                    ? editor_state->pos_y + 0x0C
                    : this->editors[0]->end_a->pos_y + offset;
                front_wheel_ed->pos_y = front_pos;
                front_wheel_ed->pos_x = editor_state->pos_x;
                rear_wheel_ed->pos_x = editor_state->pos_x;
                rear_wheel_ed->pos_y = front_wheel_ed->pos_y + 0x16;
            }

            /* Copy front wheel position to rear wheel + 0x0C */
            rear_wheel_ed->pos_x = front_wheel_ed->pos_x;
            rear_wheel_ed->pos_y = editor_state->pos_y;
        }
    }

    return 1;
}

/* ================================================================== */
/* Vehicle::GetNearestTrack                                             */
/* Address: 0x44D4C0 (53 bytes)                                         */
/*                                                                      */
/* Gets the target building pointer from the active wheel. Returns the  */
/* target pointer if its state (+0x10C) is 7 (track-alike state),       */
/* otherwise returns 0.                                                  */
/*                                                                      */
/* NOTE: The original assembly uses a mask trick:                        */
/*   target & ((state != 7) - 1)                                        */
/*   = target & 0 when state != 7 (result 0)                             */
/*   = target & -1 when state == 7 (result target)                       */
/* ================================================================== */
int32_t Vehicle::GetNearestTrack()
{
    EditorState* wheel;

    if (this->active_editor == 0) {
        /* Forward: use last editor's rear wheel */
        wheel = this->editors[this->editor_count]->end_b;
    } else {
        /* Reverse: use first editor's front wheel */
        wheel = this->editors[0]->end_a;
    }

    GameVehicle* target = wheel->building;

    /* Return target only if vehicle_kind == 7 (track-alike), otherwise 0 */
    /* Original asm: target & ((target->vehicle_kind != 7) - 1) */
    if (target != nullptr && target->vehicle_kind == 7) {
        return static_cast<int32_t>(reinterpret_cast<uintptr_t>(target));
    }
    return 0;
}

/* ================================================================== */
/* Vehicle::UpdatePosition                                              */
/* Address: 0x44D500 (212 bytes)                                        */
/*                                                                      */
/* Updates visible flag on all VehicleEditor sub-objects. Triggers      */
/* sound on first editor if not already playing. Invalidates rect for   */
/* repaint. Pauses/plays audio channels based on reverse parameter and  */
/* vehicle state.                                                       */
/*                                                                      */
/* Skips if direction is EDGE_OF_MAP (2) or DEPOT (3), or if           */
/* net_sync_flag is non-zero.                                           */
/* ================================================================== */
void Vehicle::UpdatePosition(uint8_t reverse)
{
    /* Skip if in edge-of-map or depot direction, or if net sync pending */
    if (this->direction == 2 || this->direction == 3 ||
        this->net_sync_flag != 0) {
        return;
    }

    /* Trigger sound on first editor if not already playing */
    if (this->editors[0]->audio_channel == nullptr) {
        this->editors[0]->TriggerSound();
    }

    /* Update visible flag on all editors based on reverse param */
    for (int32_t i = 0; i <= static_cast<int32_t>(static_cast<uint32_t>(this->editor_count)); i++) {
        VehicleEditor* editor = this->editors[i];
        if (editor == nullptr) continue;

        uint8_t old_visible = editor->visible;

        /* Skip if already in the desired state */
        if (reverse == 0) {
            if (old_visible == 0) continue;   /* already invisible */
        } else {
            if (old_visible != 0) continue;   /* already visible */
        }

        editor->visible = reverse;

        /* Invalidate rect for repaint */
        TileMap_InvalidateRect(g_tilemap,
            editor->screen_rect.left, editor->screen_rect.top,
            editor->screen_rect.right, editor->screen_rect.bottom);

        /* Pause or play audio channel. audio_channel is a void* field
         * reused to store an integer channel ID (pre-existing Entity.h
         * layout choice, not introduced here). */
        uint32_t audio_ch = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(editor->audio_channel));
        if (audio_ch != 0) {
            if (reverse == 0 || this->state == 0 ||
                this->state == 1 || this->state == 4) {
                AudioChannel_Pause(audio_ch);
            } else {
                AudioChannel_Play(audio_ch);
            }
        }
    }
}

/* ================================================================== */
/* Vehicle::Stop                                                        */
/* Address: 0x44D5E0 (66 bytes)                                         */
/*                                                                      */
/* Checks activeEditor match, sound guard, and moving state. If all     */
/* clear and vehicle is moving (or force flag is set), calls             */
/* UpdateEngineSound.                                                    */
/* ================================================================== */
void Vehicle::Stop(int32_t param_1, uint8_t param_2)
{
    /* If active_editor matches param_1, skip */
    if (this->active_editor == param_1) {
        return;
    }

    /* Skip if reentrancy guard active */
    if (this->sound_guard != 0) {
        return;
    }

    /* If param_2 is 0, check if vehicle is actually moving */
    if (param_2 == 0) {
        if (this->IsMoving() == 0) {
            return;
        }
    }

    /* Play engine sound update */
    if (this->sound_guard == 0) {
        this->sound_guard = 1;
        this->UpdateEngineSound();
        this->sound_guard = 0;
    }
}

/* ================================================================== */
/* Vehicle::IsMoving                                                    */
/* Address: 0x44D630 (139 bytes)                                        */
/*                                                                      */
/* Checks if the vehicle is currently moving. Returns 0 if:             */
/*   - State is STOPPING (4)                                            */
/*   - Sound guard is active                                            */
/*   - Wheel target is road/building with matching resource              */
/* Returns 1 if the vehicle appears to be in motion.                    */
/* ================================================================== */
uint8_t Vehicle::IsMoving()
{
    /* If state is STOPPING (4), not moving */
    if (this->state == 4) {
        return 0;
    }

    /* If sound guard active, return false */
    if (this->sound_guard != 0) {
        return 0;
    }

    /* Get relevant wheel based on active_editor direction */
    EditorState* wheel;
    if (this->active_editor == 0) {
        /* Forward: use last editor's rear wheel */
        wheel = this->editors[this->editor_count]->end_b;
    } else {
        /* Reverse: use first editor's front wheel */
        wheel = this->editors[0]->end_a;
    }

    /* Check if target exists and is a road/building tile. */
    GameVehicle* target = this->editor_state->building;
    if (target == nullptr) {
        return 1;   /* no target = assume moving */
    }

    bool target_is_road, target_is_building;
    ClassifyResourceTile(target->resource, &target_is_road, &target_is_building);
    if (!target_is_road && !target_is_building) {
        return 1;   /* not road/building = assume moving */
    }

    /* Check wheel's current target for continuity */
    GameVehicle* wheel_target = wheel->building;
    if (wheel_target != nullptr) {
        bool wheel_is_road, wheel_is_building;
        ClassifyResourceTile(wheel_target->resource, &wheel_is_road, &wheel_is_building);
        if (wheel_is_road || wheel_is_building) {
            /* If wheel target differs from vehicle target, vehicle is still
             * moving. Compares the real resource pointers directly rather
             * than truncated int32_t copies of them -- on this 64-bit host,
             * two distinct real pointers can truncate to the same 32-bit
             * value, which would have made this a false "same target". */
            if (wheel_target->resource != target->resource) {
                return 0;
            }
        }
    }

    return 1;
}

/* ================================================================== */
/* Vehicle::CalcSpeed                                                   */
/* Address: 0x44D6C0 (82 bytes)                                         */
/*                                                                      */
/* Checks if the given speed matches forward or reverse speed limit.    */
/* If matched, sets max_steps and triggers editor animation via its     */
/* vtable:                                                              */
/*   vtable[7] = StopSound(int) — inherited from Entity (0x405A20), NOT */
/*               overridden by VehicleEditor; 0 for fwd, 1 for rev.     */
/*               (VehicleEditor.h's own vtable-slot comment names this  */
/*               slot GetResourceId, which is wrong — confirmed by a    */
/*               raw read of the vtable at 0x477590; fixed alongside.)  */
/*   vtable[8] = SetFrame(int, bool) — (anim_data, false)               */
/* ================================================================== */
int16_t Vehicle::CalcSpeed(int16_t speed)
{
    VehicleEditor* editor = this->editors[0];
    if (editor == nullptr) {
        return 0;
    }

    if (speed == this->max_speed) {
        /* Forward speed: set max_steps = speed, play forward animation */
        this->max_steps = speed;
        int32_t anim_data = editor->frame_index;   /* +0x54 */
        editor->StopSound(0);
        editor->SetFrame(anim_data, false);
    } else if (speed == this->reverse_speed) {
        /* Reverse speed: set max_steps = speed, play reverse animation */
        int32_t anim_data = editor->frame_index;
        this->max_steps = speed;
        editor->StopSound(1);
        editor->SetFrame(anim_data, false);
    }

    return this->max_steps;
}

/* ================================================================== */
/* Vehicle::UpdateSpeed                                                 */
/* Address: 0x44D720 (29 bytes)                                         */
/*                                                                      */
/* If current state != new state (and not in state 1 when state is 0), */
/* calls SetState.                                                      */
/* ================================================================== */
void Vehicle::UpdateSpeed(int32_t new_state)
{
    if (this->state != new_state) {
        /* Allow transition from state 1 (APPROACHING) only to state 0 (STOPPED) */
        if (this->state != 1 || new_state == 0) {
            this->SetState(new_state);
        }
    }
}

/* ================================================================== */
/* Vehicle::SetState                                                    */
/* Address: 0x44D740 (186 bytes)                                        */
/*                                                                      */
/* Sets vehicle state (0=STOPPED, 1=APPROACHING, 2=MOVING,             */
/* 3=WAITING, 4=STOPPING). Manages audio channels and timers based     */
/* on state.                                                            */
/* ================================================================== */
void Vehicle::SetState(int32_t new_state)
{
    /* Skip if same state or active_flag prevents change */
    if (this->state == new_state) return;
    if (this->active_flag != 0) return;

    this->state = new_state;

    if (new_state == 0) {
        /* STOPPED — clear everything */
        this->stop_timer = 0;
        this->move_timer = 0;

        /* Pause all editor audio channels. The previous version of this
         * read cast editors[i] (already VehicleEditor*) to VehicleEditor*
         * again and added 0x48 — pointer arithmetic on a typed pointer is
         * scaled by sizeof(VehicleEditor) (0x450 bytes), not by 1, so this
         * read garbage far outside the object on every call. Same bug
         * class as the prev_rear fix in LoadSounds. audio_channel is a
         * void* field reused to store an integer channel ID (pre-existing
         * Entity.h layout choice, not introduced here). */
        for (int32_t i = 0; i <= static_cast<int32_t>(static_cast<uint32_t>(this->editor_count)); i++) {
            uint32_t audio_ch = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this->editors[i]->audio_channel));
            if (audio_ch != 0) {
                AudioChannel_Pause(audio_ch);
            }
        }
    } else if (new_state == 1) {
        /* APPROACHING — pause all editor audio channels */
        this->move_timer = 0;
        for (int32_t i = 0; i <= static_cast<int32_t>(static_cast<uint32_t>(this->editor_count)); i++) {
            uint32_t audio_ch = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this->editors[i]->audio_channel));
            if (audio_ch != 0) {
                AudioChannel_Pause(audio_ch);
            }
        }
    } else if (new_state == 4) {
        /* STOPPING — clear move timer only */
        this->move_timer = 0;
    } else {
        /* MOVING (2) or WAITING (3) — start playing audio */
        if (this->net_sync_flag != 0) return;    /* net sync pending */
        if (this->direction == 2) return;         /* edge-of-map */
        if (this->direction == 3) return;         /* depot */

        /* Trigger sound on first editor if not already playing */
        if (this->editors[0]->audio_channel == nullptr) {
            this->editors[0]->TriggerSound();
        }

        /* Play all editor audio channels (same scaled-pointer bug fixed
         * as above). */
        for (int32_t i = 0; i <= static_cast<int32_t>(static_cast<uint32_t>(this->editor_count)); i++) {
            uint32_t audio_ch = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this->editors[i]->audio_channel));
            if (audio_ch != 0) {
                AudioChannel_Play(audio_ch);
            }
        }
    }
}
