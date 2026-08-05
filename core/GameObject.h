/**
 * GameObject.h — Root object base reconstructed from loco.exe.
 *
 * Binary vtable: 0x477820 (six slots)
 *   [0] 0x412600 scalar deleting destructor
 *   [1] 0x436AB0 InvalidateRect
 *   [2] 0x436A10 PtInRect
 *   [3] 0x436A60 MoveTo
 *   [4] 0x436AE0 InvokeCallback1
 *   [5] 0x436B00 InvokeCallback2
 *
 * sizeof(GameObject) = 0x24 on 32-bit x86.  Entity begins at +0x24.
 */
#pragma once

#include "../shared/types.h"


// Status: TRANSCRIBED
class GameObject {
public:
    int32_t type;                       // +0x04
    RECT screen_rect;                   // +0x08
    uint8_t initialized;                // +0x18
    uint8_t _pad_19[3];                 // +0x19
    BOOL (*callback_1)(int, int);         // +0x1C, cdecl callback
    BOOL (*callback_2)(int, int);         // +0x20, cdecl callback

    /* Relocated up from Entity so that plain GameObject* handles (used by
       generic dispatch code such as TrainEntity_DeserializeFactory) can
       reach these without a downcast. Binary offset (on Entity-derived
       objects) is +0x4C/+0x50; the raw offset here differs since these
       fields no longer live in Entity's own layout region. */
    int32_t world_x;                      // +0x4C world X
    int32_t world_y;                      // +0x50 world Y

    /** Address: 0x4369D0 */
    GameObject();

    /** Vtable [0]; body 0x436A00, scalar deleting wrapper 0x412600. */
    virtual ~GameObject();

    /** Address: 0x436AB0, vtable [1]. */
    virtual void InvalidateRect();

    /** Address: 0x436A10, vtable [2]. */
    virtual BOOL PtInRect(int x, int y);

    /** Address: 0x436A60, vtable [3]. */
    virtual void MoveTo(int x, int y);

    /** Address: 0x436AE0, vtable [4]. */
    virtual BOOL InvokeCallback1(int x, int y);

    /** Address: 0x436B00, vtable [5]. */
    virtual BOOL InvokeCallback2(int x, int y);

    /** Address: 0x436A00. */
    void MarkDead();

    /** Address: 0x436A40. */
    void GetRelPos(int* out, int x, int y);

    /** Update — base no-op; overridden by Entity/derived classes. */
    virtual void Update();

    /** RegisterEntity — register a constructed/deserialized entity with
        the given context. Base no-op; overridden where needed. */
    virtual void RegisterEntity(void* context, void* entity);

    /** InitBase — base no-op initialization hook (overridden by Entity). */
    virtual int InitBase(int resource_id, int anim_index, bool force_reload);
};
