/**
 * Entity.h — Mid-level base class for all interactive game objects
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Extends GameObject with:
 *   - Parent/child scene graph pointer (+0x40)
 *   - Audio resource and channel handles (+0x44, +0x48)
 *   - World position with resource offset (+0x4C, +0x50)
 *   - Frame tracking and timer state (+0x54..+0x70)
 *   - Hit-test coordinate storage (+0x60..+0x6C)
 *   - Object name buffer[11] (+0x7C)
 *
 * Vtable: 0x477488
 *   Inherits most virtual methods from GameObject.
 *   Overrides constructor/destructor for Entity field initialization.
 *
 * Class hierarchy:
 *   GameObject (root, type=1, vtable 0x477820)
 *     └─ Entity (type=2, vtable 0x477488)  ← this class
 *          ├─ Building (vtable 0x477EB8)
 *          ├─ LOCOBITMAP (vtables 0x4773E8/0x4773F0, extends to ~0x1F4+)
 *          ├─ Game (vtable 0x477718)
 *          └─ Train (shares Building base ctor)
 */

#pragma once

#include "GameObject.h"

class Entity : public GameObject {
public:
    /* ================================================================ */
    /* Entity-specific fields (start at +0x40, after GameObject fields)  */
    /* ================================================================ */

    Entity*  parent;            // +0x40  parent in scene graph, or NULL
    uint32_t sound_res_id;      // +0x44  sound resource ID (RESMGR)
    void*    audio_channel;     // +0x48  audio channel handle (CGWND)
    int32_t  world_x;           // +0x4C  world X (resource offset applied)
    int32_t  world_y;           // +0x50  world Y (resource offset applied)
    uint16_t frame_index;       // +0x54  current sprite frame number
    uint8_t  _pad_56[2];        // +0x56
    uint32_t timer;             // +0x58  general-purpose timer/counter
    uint32_t active_state;      // +0x5C  audio state flag
    uint32_t next_sound_time;   // +0x60  absolute game tick for next sound
    uint32_t hit_miss_x;        // +0x64  miss-case X for HitTest dispatch
    uint32_t hit_miss_y;        // +0x68  miss-case Y for HitTest dispatch
    uint32_t hit_hit_x;         // +0x68  hit-case X for HitTest dispatch (overlaps)
    uint32_t hit_hit_y;         // +0x6C  hit-case Y for HitTest dispatch
    uint32_t phase_timer;       // +0x6C  animation phase step counter (overlaps)
    uint8_t  waiting_flag;      // +0x70  1 = waiting at animation boundary
    uint8_t  _pad_71[3];        // +0x71
    int32_t  world_x_raw;       // +0x74  raw X (before resource offset)
    int32_t  world_y_raw;       // +0x78  raw Y (before resource offset)
    char     name[11];          // +0x7C  null-terminated, max 10 chars

    /* ================================================================ */
    /* Constructor                                                       */
    /* ================================================================ */

    /**
     * Entity base constructor.
     * Address: 0x405790
     *
     * Calls GameObject() (0x4369D0) to initialize base fields, then:
     *   - Sets vtable to 0x477488 (Entity vtable)
     *   - Sets type = 2
     *   - Zeroes parent, audio/sound handles, timers
     *   - Copies empty string to name field
     *   - If resource_id > 0, calls InitBase() to load the resource
     *
     * Called by all derived class constructors (Building, Train, Game,
     * LOCOBITMAP, UIPANEL, ScriptedObject, etc.).
     *
     * @param resource_id  numeric resource ID (> 0 to load)
     * @param anim_idx     initial animation index (-1 = use resource default)
     * @param world_x      initial world X position
     * @param world_y      initial world Y position
     */
    Entity(int resource_id, int16_t anim_idx, int world_x, int world_y);

    /**
     * Destructor — inherited from GameObject.
     * Entity does NOT override the destructor body; it inherits
     * GameObject::~GameObject() which resets vtable to 0x477488,
     * releases resources, and marks dead.
     */
};
