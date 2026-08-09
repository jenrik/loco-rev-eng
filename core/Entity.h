/**
 * Entity.h — Resource-backed interactive object base.
 *
 * Binary vtable: 0x477488 (15 slots)
 *   [0]  0x405850 scalar deleting destructor
 *   [1]  0x436AB0 GameObject::InvalidateRect
 *   [2]  0x436A10 GameObject::PtInRect
 *   [3]  0x405C00 SetWorldPos (overrides GameObject::MoveTo)
 *   [4]  0x436AE0 GameObject::InvokeCallback1
 *   [5]  0x436B00 GameObject::InvokeCallback2
 *   [6]  0x405900 InitBase
 *   [7]  0x405A20 StopSound
 *   [8]  0x405DE0 SetFrame
 *   [9]  0x4061B0 SetVisible
 *   [10] 0x405C40 Update
 *   [11] 0x405E60 Draw
 *   [12] 0x405FD0 DrawConnected
 *   [13] 0x405E20 SetName
 *   [14] 0x405A50 SetAnimState
 */
#pragma once

#include "GameObject.h"


// Status: TRANSCRIBED
class Entity : public GameObject {
public:
    /* Copy constructor is real behavior, not incidental: ui/UI_ListBox.cpp
     * and ui/UI_ScrollBar.cpp placement-new-copy an Entity/UIEntity to
     * install the compiler-managed vtable over a raw byte snapshot
     * (0x424040/0x424550) -- deleting it would break that. Defaulting it
     * is behavior-neutral (identical to the previously-implicit
     * memberwise copy this code already relied on). */
    Entity(const Entity&) = default;
    Entity& operator=(const Entity&) = delete;

    uint8_t visible;                    // +0x24
    uint8_t _pad_25[3];                 // +0x25
    int32_t anim_index;                 // +0x28
    uint32_t blit_flags;                // +0x2C
    RECT source_rect;                   // +0x30
    union {
        void* resource;                 // +0x40, resource-backed entities
        Entity* parent;                 // +0x40, alias used by some derived layouts
    };
    uint32_t sound_res_id;              // +0x44
    void* audio_channel;                // +0x48
    /* world_x / world_y (binary +0x4C/+0x50) now declared on GameObject
       so plain GameObject* handles can reach them without a downcast. */
    int32_t frame_index;                // +0x54 (all binary accesses are dword)
    uint32_t timer;                     // +0x58
    uint32_t active_state;              // +0x5C
    uint32_t next_sound_time;           // +0x60
    union {
        uint32_t stored_resource_id;    // +0x64 Building: resource id (set 0x433A20, used 0x435580)
        uint32_t hit_miss_x;            // historical derived-class alias
    };
    union {
        uint32_t action_cooldown_time;  // +0x68 Building: click-action cooldown (0x435580, 0x433A20)
        uint32_t hit_hit_x;             // historical derived-class alias
    };
    uint32_t phase_timer;               // +0x6C
    uint8_t waiting_flag;               // +0x70
    uint8_t _pad_71[3];                 // +0x71
    int32_t world_x_raw;                // +0x74
    int32_t world_y_raw;                // +0x78
    char name[11];                      // +0x7C..+0x86
    uint8_t _pad_87;                    // +0x87

    Entity();

    /** Address: 0x405790. */
    Entity(int resource_id, int16_t anim_idx, int world_x, int world_y);

    /** Body 0x405870; scalar deleting wrapper 0x405850. */
    ~Entity() override;

    /** Address: 0x405C00, vtable [3]. */
    void MoveTo(int x, int y) override;

    /** Descriptive non-virtual alias used by reconstructed callers. */
    void SetWorldPos(int x, int y) { MoveTo(x, y); }

    /** Address: 0x405900, vtable [6]. */
    int InitBase(int resource_id, int anim_index, bool force_reload) override;
    /** Address: 0x405A20, vtable [7]. */
    virtual void StopSound(int param);
    /** Address: 0x405DE0, vtable [8]. */
    virtual void SetFrame(int frame_id, bool trigger_invalidate);
    /** Address: 0x4061B0, vtable [9]. */
    virtual void SetVisible(bool visible);
    /** Address: 0x405C40, vtable [10]. */
    void Update() override;
    /** Address: 0x405E60, vtable [11]. */
    virtual void Draw(RECT clip_bounds, int enable_scroll, uint32_t extra_flags);
    /** Address: 0x405FD0, vtable [12]. */
    virtual void DrawConnected(RECT clip_bounds, int enable_scroll, uint32_t extra_flags);
    /** Address: 0x405E20, vtable [13]. */
    virtual void SetName(const char* name);
    /** Address: 0x405A50, vtable [14]. */
    virtual int SetAnimState(int anim_index);

    /** Address: 0x405AB0. */
    void PlayAnimation(int sound_id);
    /** Address: 0x405E20 (same implementation as SetName). */
    void CopyName(const char* name);

    /** Address: 0x458350. */
    void GetSubObjectPosition(int* out_xy, int sub_index);
    /** Address: 0x4583C0. */
    BOOL GetBoundingRect(RECT* out_rect);
};
