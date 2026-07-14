/**
 * GameObject.h — Root base class for all Lego Loco game objects
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Class hierarchy:
 *   GameObject (root, type=1, base vtable 0x477820)
 *     └─ Entity (type=2, vtable 0x477488)
 *          ├─ Building (vtable 0x477EB8, size 0xF4)
 *          ├─ BuildingComplex (vtable 0x478008)
 *          ├─ LOCOBITMAP (vtables 0x4773E8/0x4773F0)
 *          └─ ... other derived classes
 *
 * Vtable layout (at 0x477820 for GameObject base, 0x477488 for Entity):
 *   [0]  +0x00: scalar deleting destructor
 *   [1]  +0x04: StopSound / invalidate rect
 *   [2]  +0x08: (release resource?)
 *   [3]  +0x0C: HitTest dispatch (set position/animation on hit/miss)
 *   [4]  +0x10: (unknown)
 *   [5]  +0x14: (unknown)
 *   [6]  +0x18: InitBase (resource loading + setup)
 *   [7]  +0x1C: SetAnimState (select animation entry)
 *   [8]  +0x20: SetFrame (update source rect + trigger redraw)
 *   [9]  +0x24: SetName (validate + copy name string)
 *   [10] +0x28: Draw (single-frame sprite blit)
 *   [11] +0x2C: DrawConnected (multi-frame connected sprite blit)
 *   [12] +0x30: OnTimerTick (delayed dispatch)
 *   [13] +0x34: (unknown)
 *   [14] +0x38: AnimStateSelect (animation selection at boundary)
 */

#pragma once

#include "../shared/types.h"

class GameObject {
public:
    /* ================================================================ */
    /* Vtable — array of function pointers at +0x00                      */
    /* ================================================================ */
    void** vtable;              // +0x00

    /* ================================================================ */
    /* Core identity fields                                              */
    /* ================================================================ */
    int32_t  type;              // +0x04  1=GameObject, 2=Entity
    RECT     screen_rect;       // +0x08  {left, top, right, bottom}
    uint8_t  initialized;       // +0x18  1 = ready for operations
    uint8_t  _pad_19[3];        // +0x19
    uint32_t _pad_1C;           // +0x1C
    uint32_t _pad_20;           // +0x20
    uint8_t  visible;           // +0x24  1 = should be drawn
    uint8_t  _pad_25[3];        // +0x25
    int32_t  anim_index;        // +0x28  current animation entry index
    uint32_t blit_flags;        // +0x2C  base flags OR'd into every blit
    RECT     source_rect;       // +0x30  pixel offsets into sprite sheet
                                // +0x30 = left (frame_index * width)
                                // +0x38 = right ((frame_index+1) * width)
                                // +0x3C = bottom (height)

    /* ================================================================ */
    /* Virtual methods — vtable dispatch                                 */
    /* ================================================================ */

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x405850 (GameObject level), overridden by derived classes.
     * MSVC pattern: calls ~GameObject() body, then frees memory if flags & 1.
     */
    virtual void* scalar_deleting_destructor(byte flags);

    /**
     * Stop sound and optionally play a new one (vtable[1]).
     * Address: 0x405A20
     */
    virtual void StopSound(int param);

    /**
     * Hit-test dispatch (vtable[3]).
     * Address: 0x405680
     *
     * Tests a packed (X|Y) point against 8 UISprite sub-rectangles
     * (+0x148..+0x164). Dispatches vtable[3] with hit coordinates
     * (+0x68/+0x6C) or miss coordinates (+0x60/+0x64).
     */
    virtual int HitTest(uint32_t packedXY);

    /**
     * Initialize with a resource (vtable[6]).
     * Address: 0x405900
     *
     * Loads resource from ResourceManager, sets up bounding rects,
     * initializes animation state. Returns 1 on success, 0 on failure.
     */
    virtual int InitBase(int resource_id, int anim_index, bool force_reload);

    /**
     * Set animation by frame table index (vtable[7]).
     * Address: 0x405A50
     *
     * Validates range against resource's anim_count, stores frame ID,
     * triggers redraw via SetFrame and plays animation sound.
     */
    virtual int SetAnimState(int anim_index);

    /**
     * Set current frame and optionally invalidate rect (vtable[8]).
     * Address: 0x405DE0
     */
    virtual void SetFrame(int frame_id, bool trigger_invalidate);

    /**
     * Set object name with validation (vtable[9]).
     * Address: 0x405E20
     *
     * Validates first character (isalnum or null), copies max 10 chars
     * to +0x7C, null-terminates at +0x86.
     */
    virtual void SetName(const char* name);

    /**
     * Render single-frame sprite to primary surface (vtable[10]).
     * Address: 0x405E60
     *
     * Computes source/dest rects, handles horizontal flip, clips to
     * visible area, blits via UIPANEL_Blit.
     */
    virtual void Draw(RECT clip_bounds, int enable_scroll, uint32_t extra_flags);

    /**
     * Render connected/multi-tile sprite (vtable[11]).
     * Address: 0x405FD0
     *
     * Temporarily increments frame index, blits, then restores.
     */
    virtual void DrawConnected(RECT clip_bounds, int enable_scroll, uint32_t extra_flags);

    /**
     * Timer/event completion handler (vtable[12]).
     * Address: 0x4055E0
     *
     * Destroys child callback object at +0x130, calls vtable[3]
     * to update display.
     */
    virtual int OnTimerTick();

    /**
     * Animation state selection at boundary (vtable[14]).
     * Address: 0x405AB0 (PlayAnimation as the default impl)
     *
     * Called when animation reaches a boundary. Plays a sound resource
     * associated with the given ID.
     */
    virtual void AnimStateSelect(int sound_id);

    /* ================================================================ */
    /* Non-virtual methods                                               */
    /* ================================================================ */

    /**
     * Base constructor — zero-initializes core fields.
     * Address: 0x4369D0
     *
     * Sets vtable=0x477820, type=1, zeroes screen_rect, initialized=1.
     */
    GameObject();

    /**
     * Destructor body — releases resources.
     * Address: 0x405870
     *
     * Releases audio channel (+0x48), child/resource references
     * (+0x40, +0x44), marks object dead in manager.
     */
    ~GameObject();

    /**
     * Set world position (x, y).
     * Address: 0x405C00
     *
     * Updates world_x/world_y, applies resource offset to screen position,
     * repositions audio channel if active.
     */
    void SetWorldPos(int x, int y);

    /**
     * Main animation state-machine update.
     * Address: 0x405C40
     *
     * Advances sprite frame index based on FrameData timing.
     * Handles forward/reverse, ping-pong, pause-at-boundary.
     */
    void Update();

    /**
     * Play a sound/animation for the given resource ID.
     * Address: 0x405AB0
     *
     * Looks up sound resource, releases old audio channel, allocates
     * new one, schedules playback with optional random delay.
     */
    void PlayAnimation(int sound_id);

    /**
     * Copy a validated name string into the object.
     * Address: 0x405E20 (used as helper by SetName and Building_BaseCtor)
     *
     * Validates first character, copies up to 10 chars to +0x7C.
     */
    void CopyName(const char* name);

    /**
     * Move object to absolute world coordinates (raw, no resource offset).
     * Internal helper called by SetWorldPos.
     */
    void MoveTo(int x, int y);
};
