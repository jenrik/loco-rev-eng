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

#include "Vehicle.h"
#include "../world/EditorState.h"
#include "../core/VehicleEditor.h"
#include "../network/DPlayManager.h"
#include <new>
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void* operator_new(size_t size);                        /* 0x465CE0 */
void  GLOBAL_free(void* ptr);                           /* 0x465CD0 */

extern "C" {
    /* EditorState subsystem */
    void* __thiscall EditorState_Ctor(void* this_, uint8_t param_1);
    void  __fastcall GAMESTATE_InitTrackAtPosition(void* editor_state, int32_t x, int32_t y);
    uint32_t __fastcall GAMESTATE_UpdateVehiclePlacement(void* editor_state, void* vehicle);
    void  __fastcall EditorState_Copy(void* dst, void* src);
    void  __fastcall EditorState_Detach(void* editor_state);

    /* VehicleEditor subsystem */
    void* __thiscall VehicleEditor_Ctor(void* this_, int32_t param_1,
                                        int32_t param_2, uint8_t param_3);
    void  __fastcall VehicleEditor_InitTracks(void* editor, int32_t track_idx, int32_t param_3);
    void  __fastcall VehicleEditor_ProcessMove(void* editor, void* vehicle);
    void  __fastcall VehicleEditor_TriggerSound(void* editor);
    void  __fastcall VehicleEditor_UpdateEditMode(void* wheel, void* vehicle);
    void  __fastcall VehicleEditor_CheckEditBounds1(void* editor, void* vehicle);
    void  __fastcall VehicleEditor_CheckBounds(void* editor_state);
    void  __fastcall VehicleEditor_CheckBounds2(void* editor_state);

    /* TileMap */
    int32_t __fastcall TileMap_GetObjectAt(void* tilemap, int16_t x, int16_t y, int32_t layer);
    void    __thiscall TileMap_InvalidateRect(void* tilemap, int32_t x, int32_t y,
                                              int32_t w, int32_t h);

    /* Input */
    void*   __fastcall INPUT_FindObjectAt(void* input_mgr, int32_t type);

    /* Building/GameVehicle destination management */
    void __fastcall GameVehicle_AddDestination(int32_t* target, void* vehicle);

    /* Audio */
    void __thiscall AudioChannel_UpdatePosition(void* channel, int32_t x, int32_t y);
    void __fastcall AudioChannel_Pause(uint32_t channel_id);
    void __fastcall AudioChannel_Play(uint32_t channel_id);

    /* Arrival queue */
    void __fastcall ArrivalQueue_AddVehicle(void* building, void* vehicle);

    /* Network */
    void __fastcall NETMAN_ReceivePing(void* netman, uint32_t player_id,
                                       uint8_t color_r, uint8_t color_g,
                                       int32_t x, int32_t y);
    void __fastcall NETMAN_ReceiveAck(void* netman, uint32_t player_id,
                                      uint8_t color_r, uint8_t color_g);
    void __fastcall NETMAN_SerializePlayerData(void* netman, int32_t vehicle_addr);

    /* UI */
    void __cdecl UI_CreateMessageBox(void* tooltip_mgr, int32_t res_id,
                                     int32_t param2, char param3,
                                     int32_t x, int32_t y, uint8_t param7);

    /* Resource helpers */
    uint8_t __fastcall RESDATA_IsRoadTile(int32_t tile_obj);
    uint8_t __fastcall RESDATA_IsBuildingTile(int32_t tile_obj);

    /* World */
    void __fastcall World_Init(void* world);

}

/* StopSound on GameObject */
void __fastcall GameObject_StopSound(void* obj, int32_t sound_idx);

/* ================================================================== */
/* Global references                                                    */
/* ================================================================== */
extern void* g_tilemap;       /* TileMap singleton, 0x4AAD08 */
extern void* g_netman;        /* NetMan singleton */
extern void* g_input_mgr;     /* INPUT manager singleton */
extern void* g_tooltip_mgr;   /* Tooltip manager */

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
    this->editor_state_2 = 0;       /* +0x8C */
    this->net_sync_flag = 0;        /* +0x68 */
    this->network_next = nullptr;   /* +0x70 */

    /* Clear occupant/track slots (8 int32_t at +0x38) */
    for (int i = 0; i < 8; i++) {
        this->occupant_tracks[i] = 0;
    }

    /* Clear editor array (4 slots at +0x10) */
    this->editors[0] = 0;
    this->editors[1] = 0;
    this->editors[2] = 0;
    this->editors[3] = 0;
    this->direction = 0;            /* +0x60 */

    /* Create EditorState sub-object (0x20 bytes) */
    void* state = operator_new(0x20);
    if (state != 0) {
        state = EditorState_Ctor(state, param_3);
    }
    this->editor_state = state;     /* +0x20 */
    this->stop_timer = 0;           /* +0x28 */

    /* Re-clear editors (redundant, preserved from original) */
    this->editors[0] = 0;
    this->editors[1] = 0;
    this->editors[2] = 0;
    this->editors[3] = 0;
    this->editor_count = 0;         /* +0x0C */

    /* Create VehicleEditor sub-object (0x450 bytes) */
    void* vehicle_editor = operator_new(0x450);
    if (vehicle_editor != 0) {
        vehicle_editor = VehicleEditor_Ctor(vehicle_editor, param_1, 2, param_3);
    }
    this->editors[0] = vehicle_editor;   /* editors[0] = primary VehicleEditor */

    /* Check if first editor slot is active */
    void** active_slot = (void**)this->editors[this->editor_count];
    if (active_slot != 0) {
        uint8_t* editor_bytes = (uint8_t*)active_slot;
        if (editor_bytes[0x18] == 1) {    /* editor active flag at +0x18 */
            /* Store back-reference to Vehicle in editor */
            *(void**)(editor_bytes + 0x44C) = this;

            /* Read speed parameters from editor config */
            uint8_t* editor_param = (uint8_t*)((VehicleEditor*)this->editors[0])->resource;
            int16_t fwd_speed = *(int16_t*)(editor_param + 0x7A8);  /* forward speed */
            this->max_speed = fwd_speed;        /* +0x24 */
            this->reverse_speed = *(int16_t*)(editor_param + 0x7AA);  /* +0x26 */
            this->max_steps = fwd_speed;        /* +0x58 = forward speed */
            this->active_editor = 0;            /* +0x08 = 0 (forward) */

            this->SetState(0);                  /* STATE_STOPPED */
            this->move_timer = 0;               /* +0x36 */

            /* Set player colors based on scenario */
            uint8_t* netman_bytes = (uint8_t*)g_netman;
            if (*(int32_t*)(netman_bytes + 0x7C4) == 2) {  /* scenario 2 */
                this->color_r = *(uint8_t*)(netman_bytes + 0x7D0);
                this->color_g = *(uint8_t*)(netman_bytes + 0x7D0);
            } else {
                this->color_r = 1;
                this->color_g = 1;
            }

            if (param_3 == 0) {     /* local vehicle */
                /* Register player slot with netman */
                int32_t max_players = *(int32_t*)(netman_bytes + 0x7E8) + 1;
                *(int32_t*)(netman_bytes + 0x7E8) = max_players;
                this->player_id = (int16_t)max_players;   /* +0x7A */

                GAMESTATE_InitTrackAtPosition(this->editor_state, -1, -1);

                int32_t track_idx = ((EditorState*)this->editor_state)->pos_x + 0x0C;
                VehicleEditor_InitTracks(this->editors[0], track_idx, -1);

                int16_t net_x = (track_idx < 0) ? -1 : (int16_t)(track_idx >> 4);
                NETMAN_ReceivePing(g_netman, (uint32_t)this->player_id,
                                   this->color_r, this->color_g, (int32_t)net_x, -1);
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
        } else if (active_slot != 0) {
            /* Destroy inactive editor slot */
            void* vtbl = *(void**)active_slot;
            ((void (__thiscall*)(void*, uint8_t))vtbl)(active_slot, 1);
            this->editors[this->editor_count] = 0;
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
    CleanupChildren((int32_t*)this);
}

/* ================================================================== */
/* Vehicle::CleanupChildren — Static cleanup helper                     */
/* Address: 0x44C0D0                                                    */
/*                                                                      */
/* Called by: scalar deleting destructor                                 */
/*                                                                      */
/* Resets vtable, sends NETMAN ack if not initialized, destroys all     */
/* VehicleEditor children, destroys editor state sub-object.            */
/* ================================================================== */
void __fastcall Vehicle::CleanupChildren(int32_t* object)
{
    Vehicle* vehicle = reinterpret_cast<Vehicle*>(object);
    if (g_netman != nullptr && vehicle->init_flag == 0) {
        NETMAN_ReceiveAck(g_netman, vehicle->player_id,
                          vehicle->color_r, vehicle->color_g);
    }
    const uint16_t count = vehicle->editor_count < 4
        ? static_cast<uint16_t>(vehicle->editor_count) : 3;
    for (uint16_t index = 0; index <= count; ++index) {
        VehicleEditor* editor = vehicle->editors[index];
        if (editor == nullptr) continue;
        // Network-only editors use target_building as their Vehicle backref;
        // it is not a Building and must not enter Building teardown logic.
#ifndef _WIN32
        editor->target_building = nullptr;
#endif
        editor->~VehicleEditor();
        GLOBAL_free(editor);
        vehicle->editors[index] = nullptr;
    }
    vehicle->editor_count = 0;
    if (vehicle->editor_state != nullptr) {
        vehicle->editor_state->~EditorState();
        GLOBAL_free(vehicle->editor_state);
        vehicle->editor_state = nullptr;
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
    /* Store target tile position */
    this->target_tile_x = (int16_t)target[0x22];  /* target's tile X at +0x88 */

    /* If another vehicle already assigned, add to destination queue */
    if (target[0x48] != 0 && (void*)(uintptr_t)target[0x48] != this) {
        GameVehicle_AddDestination(target, this);
        return;
    }

    /* Register as occupant */
    *(uint8_t*)(target + 0x4A) = 1;              /* occupant flag */
    target[0x48] = (int32_t)(uintptr_t)this;     /* tracked vehicle */
    this->occupancy = 5;                         /* ARRIVING */
    this->UpdatePosition(1);

    /* Set all editors and their wheels to ARRIVING state (5) */
    for (uint32_t i = 0; i <= (uint32_t)this->editor_count; i++) {
        int32_t* editor = (int32_t*)this->editors[i];
        if (editor != 0) {
            editor[0x444 / 4] = 5;                           /* editor state */
            *(int32_t*)((uintptr_t)editor[0x430 / 4] + 0x1C) = 5;  /* front wheel state */
            *(int32_t*)((uintptr_t)editor[0x434 / 4] + 0x1C) = 5;  /* rear wheel state */
        }
    }

    this->SetState(2);                  /* MOVING */
    this->UpdatePosition(1);
    this->LoadSounds(target, is_remote);
    target[0x47] = 0;                   /* clear target's occupant count */
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
    uint32_t result = (uint32_t)(int16_t)count;  /* preserve for return on failure */

    if (count < 3 && this->editors[count + 1] == 0) {
        this->editor_count = (int16_t)(count + 1);

        void* editor = operator_new(0x450);
        if (editor != 0) {
            editor = VehicleEditor_Ctor(editor, param_1, param_2, param_3);
        }
        this->editors[this->editor_count] = editor;

        result = 1;
        uint8_t* new_slot = (uint8_t*)this->editors[this->editor_count];
        if (new_slot != 0) {
            if (new_slot[0x18] == 1) {           /* active flag */
                *(void**)(new_slot + 0x44C) = this;  /* backref */
                /* ---- SEH teardown ---- */
                return result;
            }
            /* Editor inactive — destroy it */
            (*(void (__thiscall**)(void*, uint8_t))*(void**)new_slot)(new_slot, 1);
            this->editors[this->editor_count] = 0;
        }
        this->editor_count = (int16_t)(this->editor_count - 1);
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

    void* editor = this->editors[index];
    if (editor == 0) {
        return 0;
    }

    /* Destroy editor via scalar deleting destructor */
    (*(void (__thiscall**)(void*, uint8_t))*(void**)editor)(editor, 1);
    this->editors[index] = 0;
    this->editor_count--;

    /* Shift remaining slots left if not the last slot */
    if (index < 3) {
        for (uint32_t i = index; i < 3; i++) {
            this->editors[i] = this->editors[i + 1];
            this->editors[i + 1] = 0;
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
        void* editor = this->editors[i];
        if (editor != 0) {
            /* Single dereference: read int32_t at editor + 0x42C, compare to 2 */
            if (*(int32_t*)((uint8_t*)editor + 0x42C) == 2) {
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
    int32_t* wheel;
    if (this->active_editor == 0) {
        /* Forward: use last editor's rear wheel */
        int32_t* last_editor = (int32_t*)this->editors[this->editor_count];
        wheel = *(int32_t**)((uint8_t*)last_editor + 0x434);    /* rear wheel */
    } else {
        /* Reverse: use first editor's front wheel */
        int32_t* first_editor = (int32_t*)this->editors[0];
        wheel = *(int32_t**)((uint8_t*)first_editor + 0x430);  /* front wheel */
    }

    /* Check if route can be cleared:
       - wheel state is IDLE (0) at +0x1C
       - target building state != 4 at +0x10C
       Note: original code does NOT null-check the target pointer —
       it is guaranteed valid by prior state checks. */
    int32_t wheel_state = *(int32_t*)((uint8_t*)wheel + 0x1C);
    int32_t* target = *(int32_t**)((uint8_t*)wheel + 0x14);
    if (wheel_state == 0 && target != 0 && *(int32_t*)((uint8_t*)target + 0x10C) != 4) {

        /* Can clear route */
        this->occupancy = 0;                 /* +0x64 = EMPTY */
        this->UpdatePosition(1);

        /* Clear tracked vehicle on destination building */
        int32_t obj_at = TileMap_GetObjectAt(&g_tilemap,
            this->target_tile_x, this->target_tile_y + 1, 0);
        if (obj_at != 0) {
            *(int32_t*)((uintptr_t)obj_at + 0x11C) = 0;   /* clear arrival queue ptr */
            *(uint8_t*)((uintptr_t)obj_at + 0x128) = 0;   /* clear tracked_vehicle_flag */
            *(int32_t*)((uintptr_t)obj_at + 0x120) = 0;   /* clear tracked_vehicle */
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

    int32_t* target = reinterpret_cast<int32_t*>(((EditorState*)this->editor_state)->building);
    if (target == 0) {
        return 0;
    }

    int32_t target_state = target[0x110 / 4];   /* +0x110 */

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

    for (int32_t i = 0; i <= (int32_t)this->editor_count; i++) {
        int32_t* editor = (int32_t*)this->editors[i];
        if (*(int16_t*)((uint8_t*)editor + 0x448) == 1) {   /* DETACHED state */
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
         editor_idx <= (int32_t)(uint32_t)this->editor_count;
         editor_idx++) {

        uint8_t* editor = (uint8_t*)(VehicleEditor*)this->editors[editor_idx];

        /* Update wheel edit modes */
        VehicleEditor_UpdateEditMode(*(void**)(editor + 0x430), this);  /* front wheel */
        VehicleEditor_UpdateEditMode(*(void**)(editor + 0x434), this);  /* rear wheel */

        /* State machine on editor exclusion flag (+0x440) */
        int32_t excl_flag = *(int32_t*)(editor + 0x440);
        if (excl_flag > 0) {
            if (excl_flag < 3) {
                *(int32_t*)(editor + 0x440) = 4;
            } else if (excl_flag == 4) {
                *(int32_t*)(editor + 0x440) = 1;
            }
        }

        /* If front or rear wheel has state 1 (ACTIVE), set exclusion to 1 */
        if (*(int32_t*)((uintptr_t)*(int32_t*)(editor + 0x430) + 0x18) == 1 ||
            *(int32_t*)((uintptr_t)*(int32_t*)(editor + 0x434) + 0x18) == 1) {
            *(int32_t*)(editor + 0x440) = 1;
        }

        /* State machine on editor state code (+0x444) */
        int32_t state_code = *(int32_t*)(editor + 0x444);
        switch (state_code) {
        case 1:
            *(int32_t*)(editor + 0x444) = 4;
            break;
        case 2:
            *(int32_t*)(editor + 0x444) = 5;
            *(uint8_t*)(editor + 0x24) = 0;    /* clear visible */
            break;
        case 4:
            *(int32_t*)(editor + 0x444) = 1;
            VehicleEditor_CheckEditBounds1(editor, this);
            break;
        case 5:
            if (*(int32_t*)((uintptr_t)*(int32_t*)(editor + 0x430) + 0x1C) == 1 ||
                *(int32_t*)((uintptr_t)*(int32_t*)(editor + 0x434) + 0x1C) == 1) {
                /* Detached — goto case 4 logic */
                *(int32_t*)(editor + 0x444) = 1;
                VehicleEditor_CheckEditBounds1(editor, this);
            } else {
                *(int32_t*)(editor + 0x444) = 2;
                *(uint8_t*)(editor + 0x24) = 0;    /* clear visible */
            }
            break;
        }
    }

    /* --- Copy editor state and run placement iteration --- */
    int16_t* target_count = (int16_t*)(
        *(int32_t**)(reinterpret_cast<uint8_t*>(((EditorState*)this->editor_state)->building) + 0x114));
    *target_count = *target_count - 1;

    if (this->active_editor == 1) {
        /* ActiveEditor 1: copy from rear wheel of last editor */
        EditorState_Copy(
            this->editor_state,
            ((VehicleEditor*)this->editors[this->editor_count])->end_b);

        target_count = (int16_t*)(
            *(int32_t**)(reinterpret_cast<uint8_t*>(((EditorState*)this->editor_state)->building) + 0x114));
        *target_count = *target_count + 1;

        /* Run exactly 12 placement iterations */
        for (int iter = 0; iter < 12; iter++) {
            GAMESTATE_UpdateVehiclePlacement(this->editor_state, this);
        }
    } else {
        /* Forward: copy from front wheel of first editor */
        EditorState_Copy(
            this->editor_state,
            ((VehicleEditor*)this->editors[0])->end_a);

        target_count = (int16_t*)(
            *(int32_t**)(reinterpret_cast<uint8_t*>(((EditorState*)this->editor_state)->building) + 0x114));
        *target_count = *target_count + 1;

        /* Run placement iterations, stop if failure, max 12 */
        for (int iter = 0; iter < 12; iter++) {
            uint32_t result = GAMESTATE_UpdateVehiclePlacement(
                this->editor_state, this);
            if (result == 0) break;
        }
    }

    /* Post-placement state adjustment */
    if (this->direction == 4 &&
        ((EditorState*)this->editor_state)->move_state == 2) {
        ((EditorState*)this->editor_state)->move_state = 4;
    }

    VehicleEditor_CheckBounds(this->editor_state);
    VehicleEditor_CheckBounds2(this->editor_state);

    /* --- Handle editor state result (+0x1C) --- */
    switch (((EditorState*)this->editor_state)->edit_state) {
    case 0: {
        /* Check if first and last editor have exclusion state 4 or 5 */
        int32_t first_state = ((VehicleEditor*)this->editors[0])->edge_dir_b;
        int32_t last_state = ((VehicleEditor*)this->editors[this->editor_count])->edge_dir_b;
        if (first_state != 4 && first_state != 5 &&
            last_state != 4 && last_state != 5) {
            /* No excluded editors */
            if (this->occupancy == 0) {
                return 0;   /* EMPTY — nothing more to do */
            }

            /* Clear target building's tracked vehicle */
            int32_t* target_building = (int32_t*)(uintptr_t)TileMap_GetObjectAt(
                &g_tilemap, this->target_tile_x, this->target_tile_y + 1, 0);
            if (target_building != 0 &&
                (void*)(uintptr_t)target_building[0x48] == this) {
                target_building[0x47] = 0;              /* clear occupant count */
                *(uint8_t*)(target_building + 0x4A) = 0; /* clear occupant flag */
                (*(void (__thiscall**)(int32_t*, int32_t))(uintptr_t)*target_building)(
                    target_building, 0);                /* vtable[7] callback */
                target_building[0x48] = 0;              /* clear tracked vehicle */
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
    if (target == 0) {
        return 0;
    }

    /* Determine tile category from target building's resource */
    int32_t resource_id = target[0x10];          /* +0x40 = resource data ptr */
    int32_t tile_category = 0;

    if (RESDATA_IsRoadTile(resource_id)) {
        tile_category = 2;                       /* ROAD */
        this->tile_x = (int16_t)target[0x22];    /* store target tile X */
    } else if (RESDATA_IsBuildingTile(resource_id)) {
        tile_category = 1;                       /* BUILDING */
        (*(void (__thiscall**)(int32_t*, int32_t))(uintptr_t)*target)(target, 1);  /* vtable[7] activate */
    }

    /* --- Configure sound positions for each editor --- */
    for (int32_t i = 0; i <= (int32_t)(uint32_t)this->editor_count; i++) {
        uint8_t* editor = (uint8_t*)(VehicleEditor*)this->editors[i];
        uintptr_t front_wheel = (uintptr_t)*(int32_t*)(editor + 0x430);
        uintptr_t rear_wheel  = (uintptr_t)*(int32_t*)(editor + 0x434);

        /* Set target building reference on both wheels */
        *(int32_t**)(front_wheel + 0x14) = target;
        *(int32_t**)(rear_wheel + 0x14) = target;

        if (tile_category == 2) {       /* ROAD */
            *(int32_t*)(editor + 0x440) = 4;
            *(int32_t*)(front_wheel + 0x18) = 4;
            *(int32_t*)(rear_wheel + 0x18) = 4;
        } else {                        /* BUILDING or default */
            *(int32_t*)(front_wheel + 0x1C) = 5;
            *(int32_t*)(rear_wheel + 0x1C) = 5;
        }

        /* Set sound pitch offsets based on tile type at RESDATA+0x63A */
        uint8_t tile_type = *(uint8_t*)((uintptr_t)resource_id + 0x63A);

        if (tile_type == 1 || tile_type == 7) {
            /* North-facing */
            if (param_2 == 0) {
                *(int16_t*)(editor + 0x438) =
                    (this->active_editor == 0) ? 0x40 : 0;
            }
            *(int32_t*)(front_wheel + 4) = 0;
            *(int32_t*)(front_wheel + 8) = *(uint16_t*)((uintptr_t)resource_id + 0x636) - 1;
            *(int32_t*)(rear_wheel + 4) = 0;
            *(int32_t*)(rear_wheel + 8) = *(uint16_t*)((uintptr_t)resource_id + 0x636) - 1;
        } else if (tile_type == 2 || tile_type == 8) {
            /* South-facing */
            if (param_2 == 0) {
                *(int16_t*)(editor + 0x438) =
                    (this->active_editor == 0) ? 0 : 0x40;
            }
            *(int32_t*)(front_wheel + 4) = 1;
            *(int32_t*)(front_wheel + 8) = 1;
            *(int32_t*)(rear_wheel + 4) = 1;
            *(int32_t*)(rear_wheel + 8) = 1;
        } else if (tile_type == 3 || tile_type == 9) {
            /* West-facing */
            if (param_2 == 0) {
                *(int16_t*)(editor + 0x438) =
                    (this->active_editor == 0) ? 0x20 : 0x60;
            }
            *(int32_t*)(front_wheel + 4) = 1;
            *(int32_t*)(front_wheel + 8) = 1;
            *(int32_t*)(rear_wheel + 4) = 1;
            *(int32_t*)(rear_wheel + 8) = 1;
        } else if (tile_type == 4 || tile_type == 10) {
            /* East-facing */
            if (param_2 == 0) {
                *(int16_t*)(editor + 0x438) =
                    (this->active_editor == 0) ? 0x60 : 0x20;
            }
            *(int32_t*)(front_wheel + 4) = 0;
            *(int32_t*)(front_wheel + 8) = *(uint16_t*)((uintptr_t)resource_id + 0x636) - 1;
            *(int32_t*)(rear_wheel + 4) = 0;
            *(int32_t*)(rear_wheel + 8) = *(uint16_t*)((uintptr_t)resource_id + 0x636) - 1;
        }
        /* else: no special handling for other tile types */

        /* Re-set target on wheels (redundant, preserved from original) */
        *(int32_t**)(front_wheel + 0x14) = target;
        *(int32_t**)(rear_wheel + 0x14) = target;
    }

    /* --- Set editor state position --- */
    uint8_t* editor_state = (uint8_t*)(EditorState*)this->editor_state;
    *(int32_t**)(editor_state + 0x14) = target;

    /* Copy position from front wheel of first editor to editor state */
    uintptr_t first_front_wheel = (uintptr_t)((VehicleEditor*)this->editors[0])->end_a;
    *(int32_t*)(editor_state + 4) = *(int32_t*)(first_front_wheel + 4);   /* copy pos X */
    *(int32_t*)(editor_state + 8) = *(int32_t*)(first_front_wheel + 8);   /* copy pos Y */

    /* Calculate world position from target building */
    int32_t* target_building = *(int32_t**)(editor_state + 0x14);
    int32_t* target_res = *(int32_t**)((uint8_t*)target_building + 0x40);
    int32_t* track_table = *(int32_t**)((uint8_t*)target_res + 0x630);

    *(int32_t*)(editor_state + 0x0C) =
        (int32_t)*(int16_t*)((uint8_t*)track_table +
            *(int32_t*)(first_front_wheel + 8) * 4) +
        *(int16_t*)((uint8_t*)target_building + 0x88) * 0x10;
    *(int32_t*)(editor_state + 0x10) =
        (int32_t)*(int16_t*)((uint8_t*)track_table + 2 +
            *(int32_t*)(first_front_wheel + 8) * 4) +
        *(int16_t*)((uint8_t*)target_building + 0x8A) * 0x10;

    /* Set editor state code based on tile category */
    if (tile_category == 2) {
        *(int32_t*)(editor_state + 0x18) = 4;
    } else {
        *(int32_t*)(editor_state + 0x1C) = 5;
    }

    /* --- Position vehicle editors relative to editor state --- */
    if (this->active_editor != 0) {
        /* ActiveEditor set — position editors from last backward */
        int32_t offset = 0;
        uint32_t max_idx = (uint32_t)this->editor_count;
        int32_t ed_idx = max_idx;

        do {
            uint8_t* ed = (uint8_t*)(VehicleEditor*)this->editors[ed_idx];
            int tile_type = *(uint8_t*)((uintptr_t)resource_id + 0x63A);

            switch (tile_type) {
            case 1:
            case 7:
                /* North: rear wheel above front */
                if (max_idx == (uint32_t)this->editor_count) {
                    offset = *(int32_t*)(editor_state + 0x0C) - 0x0C;
                } else {
                    uintptr_t prev_rear = (uintptr_t)*(int32_t*)((VehicleEditor*)this->editors[
                        (uint32_t)this->editor_count] + 0x434);
                    offset = *(int32_t*)(prev_rear + 0x0C) - offset;
                }
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x0C) = offset;
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x10) =
                    *(int32_t*)(editor_state + 0x10);
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x430) + 0x0C) =
                    *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x0C) - 0x16;
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x430) + 0x10) =
                    *(int32_t*)(editor_state + 0x10);
                break;
            case 2:
            case 8:
                /* South: rear wheel below front */
                if (max_idx == (uint32_t)this->editor_count) {
                    offset = *(int32_t*)(editor_state + 0x0C) + 0x0C;
                } else {
                    uintptr_t prev_rear = (uintptr_t)*(int32_t*)((VehicleEditor*)this->editors[
                        (uint32_t)this->editor_count] + 0x434);
                    offset = *(int32_t*)(prev_rear + 0x0C) + offset;
                }
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x0C) = offset;
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x10) =
                    *(int32_t*)(editor_state + 0x10);
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x430) + 0x0C) =
                    *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x0C) + 0x16;
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x430) + 0x10) =
                    *(int32_t*)(editor_state + 0x10);
                break;
            case 3:
            case 9:
                /* West: rear wheel left of front */
                if (max_idx == (uint32_t)this->editor_count) {
                    offset = *(int32_t*)(editor_state + 0x10) - 0x0C;
                } else {
                    uintptr_t prev_rear = (uintptr_t)*(int32_t*)((VehicleEditor*)this->editors[
                        (uint32_t)this->editor_count] + 0x434);
                    offset = *(int32_t*)(prev_rear + 0x10) - offset;
                }
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x10) = offset;
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x0C) =
                    *(int32_t*)(editor_state + 0x0C);
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x430) + 0x0C) =
                    *(int32_t*)(editor_state + 0x0C);
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x430) + 0x10) =
                    *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x10) - 0x16;
                break;
            case 4:
            case 10:
                /* East: rear wheel right of front */
                if (max_idx == (uint32_t)this->editor_count) {
                    offset = *(int32_t*)(editor_state + 0x10) + 0x0C;
                } else {
                    uintptr_t prev_rear = (uintptr_t)*(int32_t*)((VehicleEditor*)this->editors[
                        (uint32_t)this->editor_count] + 0x434);
                    offset = *(int32_t*)(prev_rear + 0x10) + offset;
                }
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x10) = offset;
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x0C) =
                    *(int32_t*)(editor_state + 0x0C);
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x430) + 0x0C) =
                    *(int32_t*)(editor_state + 0x0C);
                *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x430) + 0x10) =
                    *(int32_t*)((uintptr_t)*(int32_t*)(ed + 0x434) + 0x10) + 0x16;
                break;
            }

            offset += 0x26;
            ed_idx--;
        } while (ed_idx >= 0);
    } else {
        /* Forward — position editors sequentially */
        for (int32_t i = 0;
             i <= (int32_t)(uint32_t)this->editor_count;
             i++) {

            uint8_t* editor = (uint8_t*)(VehicleEditor*)this->editors[i];
            uintptr_t front_wheel_ed = (uintptr_t)*(int32_t*)(editor + 0x430);
            uintptr_t rear_wheel_ed  = (uintptr_t)*(int32_t*)(editor + 0x434);
            int32_t offset = i * 0x26;

            int tile_type = *(uint8_t*)((uintptr_t)resource_id + 0x63A);

            if (tile_type == 1 || tile_type == 7) {
                /* North: front wheel above, rear wheel below */
                int32_t front_pos = (i == 0)
                    ? *(int32_t*)(editor_state + 0x0C) - 0x0C
                    : *(int32_t*)((uintptr_t)((VehicleEditor*)this->editors[0])->end_a + 0x0C)
                        + offset * -1;
                *(int32_t*)(front_wheel_ed + 0x0C) = front_pos;
                *(int32_t*)(front_wheel_ed + 0x10) = *(int32_t*)(editor_state + 0x10);
                *(int32_t*)(rear_wheel_ed + 0x0C) =
                    *(int32_t*)(front_wheel_ed + 0x0C) - 0x16;
                *(int32_t*)(rear_wheel_ed + 0x10) = *(int32_t*)(editor_state + 0x10);
            } else if (tile_type == 2 || tile_type == 8) {
                /* South: front wheel below, rear wheel above */
                int32_t front_pos = (i == 0)
                    ? *(int32_t*)(editor_state + 0x0C) + 0x0C
                    : *(int32_t*)((uintptr_t)((VehicleEditor*)this->editors[0])->end_a + 0x0C)
                        + offset;
                *(int32_t*)(front_wheel_ed + 0x0C) = front_pos;
                *(int32_t*)(front_wheel_ed + 0x10) = *(int32_t*)(editor_state + 0x10);
                *(int32_t*)(rear_wheel_ed + 0x0C) =
                    *(int32_t*)(front_wheel_ed + 0x0C) + 0x16;
                *(int32_t*)(rear_wheel_ed + 0x10) = *(int32_t*)(editor_state + 0x10);
            } else if (tile_type == 3 || tile_type == 9) {
                /* West: front wheel left, rear wheel right */
                int32_t front_pos = (i == 0)
                    ? *(int32_t*)(editor_state + 0x10) - 0x0C
                    : *(int32_t*)((uintptr_t)((VehicleEditor*)this->editors[0])->end_a + 0x10)
                        + offset * -1;
                *(int32_t*)(front_wheel_ed + 0x10) = front_pos;
                *(int32_t*)(front_wheel_ed + 0x0C) = *(int32_t*)(editor_state + 0x0C);
                *(int32_t*)(rear_wheel_ed + 0x0C) = *(int32_t*)(editor_state + 0x0C);
                *(int32_t*)(rear_wheel_ed + 0x10) =
                    *(int32_t*)(front_wheel_ed + 0x10) - 0x16;
            } else if (tile_type == 4 || tile_type == 10) {
                /* East: front wheel right, rear wheel left */
                int32_t front_pos = (i == 0)
                    ? *(int32_t*)(editor_state + 0x10) + 0x0C
                    : *(int32_t*)((uintptr_t)((VehicleEditor*)this->editors[0])->end_a + 0x10)
                        + offset;
                *(int32_t*)(front_wheel_ed + 0x10) = front_pos;
                *(int32_t*)(front_wheel_ed + 0x0C) = *(int32_t*)(editor_state + 0x0C);
                *(int32_t*)(rear_wheel_ed + 0x0C) = *(int32_t*)(editor_state + 0x0C);
                *(int32_t*)(rear_wheel_ed + 0x10) =
                    *(int32_t*)(front_wheel_ed + 0x10) + 0x16;
            }

            /* Copy front wheel position to rear wheel + 0x0C */
            *(int32_t*)(rear_wheel_ed + 0x0C) =
                *(int32_t*)(front_wheel_ed + 0x0C);
            *(int32_t*)(rear_wheel_ed + 0x10) =
                *(int32_t*)(editor_state + 0x10);
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
    uintptr_t wheel;

    if (this->active_editor == 0) {
        /* Forward: use last editor's rear wheel */
        int32_t* last_editor = (int32_t*)this->editors[this->editor_count];
        wheel = (uintptr_t)*(int32_t*)((uint8_t*)last_editor + 0x434);
    } else {
        /* Reverse: use first editor's front wheel */
        int32_t* first_editor = (int32_t*)this->editors[0];
        wheel = (uintptr_t)*(int32_t*)((uint8_t*)first_editor + 0x430);
    }

    /* Get wheel's target building pointer */
    int32_t* target = *(int32_t**)(wheel + 0x14);

    /* Return target only if state == 7, otherwise 0 */
    /* Original asm: target & ((target->state != 7) - 1) */
    if (target != 0 && *(int32_t*)((uint8_t*)target + 0x10C) == 7) {
        return (int32_t)(uintptr_t)target;
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
    if (((VehicleEditor*)this->editors[0])->audio_channel == 0) {
        VehicleEditor_TriggerSound(this->editors[0]);
    }

    /* Update visible flag on all editors based on reverse param */
    for (int32_t i = 0; i <= (int32_t)(uint32_t)this->editor_count; i++) {
        uint8_t* editor = (uint8_t*)(VehicleEditor*)this->editors[i];
        if (editor == 0) continue;

        uint8_t old_visible = editor[0x24];

        /* Skip if already in the desired state */
        if (reverse == 0) {
            if (old_visible == 0) continue;   /* already invisible */
        } else {
            if (old_visible != 0) continue;   /* already visible */
        }

        editor[0x24] = reverse;

        /* Invalidate rect for repaint */
        TileMap_InvalidateRect(&g_tilemap,
            *(int32_t*)(editor + 8),
            *(int32_t*)(editor + 0x0C),
            *(int32_t*)(editor + 0x10),
            *(int32_t*)(editor + 0x14));

        /* Pause or play audio channel */
        uint32_t audio_ch = *(uint32_t*)(editor + 0x48);
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
    uintptr_t wheel;
    if (this->active_editor == 0) {
        /* Forward: use last editor's rear wheel */
        int32_t* last_editor = (int32_t*)this->editors[this->editor_count];
        wheel = (uintptr_t)*(int32_t*)((uint8_t*)last_editor + 0x434);   /* rear wheel */
    } else {
        /* Reverse: use first editor's front wheel */
        int32_t* first_editor = (int32_t*)this->editors[0];
        wheel = (uintptr_t)*(int32_t*)((uint8_t*)first_editor + 0x430);  /* front wheel */
    }

    /* Check if target building exists and is a road/building tile */
    int32_t* target = reinterpret_cast<int32_t*>(((EditorState*)this->editor_state)->building);
    if (target == 0) {
        return 1;   /* no target = assume moving */
    }

    int32_t target_res = *(int32_t*)((uint8_t*)target + 0x40);
    if (!RESDATA_IsRoadTile(target_res) && !RESDATA_IsBuildingTile(target_res)) {
        return 1;   /* not road/building = assume moving */
    }

    /* Check wheel's current target for continuity */
    int32_t* wheel_target = *(int32_t**)(wheel + 0x14);
    if (wheel_target != 0) {
        int32_t wheel_res = *(int32_t*)((uint8_t*)wheel_target + 0x40);
        if (RESDATA_IsRoadTile(wheel_res) || RESDATA_IsBuildingTile(wheel_res)) {
            /* If wheel target differs from vehicle target, vehicle is still moving */
            if (wheel_res != target_res) {
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
/*   vtable[7] (SetAnimState, at +0x1C) — 0 for fwd, 1 for rev        */
/*   vtable[8] (SetFrame, at +0x20) — (anim_data, 0)                  */
/* ================================================================== */
int16_t Vehicle::CalcSpeed(int16_t speed)
{
    void* editor = this->editors[0];
    if (editor == 0) {
        return 0;
    }

    if (speed == this->max_speed) {
        /* Forward speed: set max_steps = speed, play forward animation */
        this->max_steps = speed;
        int32_t* ed = (int32_t*)editor;
        int32_t anim_data = ed[0x15];                          /* +0x54 = anim frame data */
        uintptr_t ed_vtbl = (uintptr_t)*ed;                    /* vtable pointer */
        /* vtable[7] (SetAnimState at +0x1C) arg=0 means forward */
        (*(void (__thiscall**)(int32_t))((void*)(ed_vtbl + 0x1C)))(0);
        /* vtable[8] (SetFrame at +0x20) with (anim_data, 0) */
        (*(void (__thiscall**)(int32_t, int32_t))((void*)(ed_vtbl + 0x20)))(anim_data, 0);
    } else if (speed == this->reverse_speed) {
        /* Reverse speed: set max_steps = speed, play reverse animation */
        int32_t* ed = (int32_t*)editor;
        int32_t anim_data = ed[0x15];                          /* +0x54 = anim frame data */
        uintptr_t ed_vtbl = (uintptr_t)*ed;                    /* vtable pointer */
        this->max_steps = speed;
        /* vtable[7] (SetAnimState at +0x1C) arg=1 means reverse */
        (*(void (__thiscall**)(int32_t))((void*)(ed_vtbl + 0x1C)))(1);
        /* vtable[8] (SetFrame at +0x20) with (anim_data, 0) */
        (*(void (__thiscall**)(int32_t, int32_t))((void*)(ed_vtbl + 0x20)))(anim_data, 0);
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

        /* Pause all editor audio channels */
        for (int32_t i = 0; i <= (int32_t)(uint32_t)this->editor_count; i++) {
            uint32_t audio_ch = *(uint32_t*)(
                (VehicleEditor*)this->editors[i] + 0x48);
            if (audio_ch != 0) {
                AudioChannel_Pause(audio_ch);
            }
        }
    } else if (new_state == 1) {
        /* APPROACHING — pause all editor audio channels */
        this->move_timer = 0;
        for (int32_t i = 0; i <= (int32_t)(uint32_t)this->editor_count; i++) {
            uint32_t audio_ch = *(uint32_t*)(
                (VehicleEditor*)this->editors[i] + 0x48);
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
        if (((VehicleEditor*)this->editors[0])->audio_channel == 0) {
            VehicleEditor_TriggerSound(this->editors[0]);
        }

        /* Play all editor audio channels */
        for (int32_t i = 0; i <= (int32_t)(uint32_t)this->editor_count; i++) {
            uint32_t audio_ch = *(uint32_t*)(
                (VehicleEditor*)this->editors[i] + 0x48);
            if (audio_ch != 0) {
                AudioChannel_Play(audio_ch);
            }
        }
    }
}
