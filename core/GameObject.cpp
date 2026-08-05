/**
 * GameObject.cpp — GameObject primitives and Entity resource methods
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * All method body addresses reference the original loco.exe binary.
 *
 * NOTE on scalar deleting destructors:
 *   GameObject's wrapper at 0x412600 calls MarkDead (0x436A00).
 *   Entity's wrapper at 0x405850 calls the resource-releasing destructor
 *   body at 0x405870. They are distinct class-level destructors.
 */

// Status: TRANSCRIBED

#include "GameObject.h"
#include "Entity.h"
#include <cstring>

namespace {

template <typename T>
T* field_at(void* object, size_t offset)
{
    return reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
}

/* RESDATA's three recovered virtual entries.  The object is a resource
 * descriptor, not an opaque C pointer: typed dispatch preserves the slot
 * signatures without reading its vptr in executable code. */
struct ResourceDataView {
    virtual void* destroy(uint8_t flags) = 0;        /* slot 0 */
    virtual void* acquire_surface(int x, int y) = 0;/* slot 1 */
    virtual void release_surface() = 0;              /* slot 2 */

protected:
    ~ResourceDataView() = default;
};

} // namespace

/* ================================================================== */
/* External function declarations (addresses from Ghidra)              */
/* ================================================================== */

/* CRT */
extern "C" {
    uint32_t CRT_rand(void);                                    /* 0x466150 */
    char*    _strncpy(char* dst, const char* src, size_t n);    /* 0x466DD0 */
    int      IsCharAlphaNumericA(char c);                       /* CRT import */
}

/* Windows API */
extern "C" {
    BOOL  IntersectRect(RECT* out, const RECT* a, const RECT* b);   /* USER32 */
    BOOL  PtInRect(const RECT* r, uint32_t packedXY);                /* USER32 */
    void  SetRect(RECT* r, int left, int top, int right, int bottom);/* USER32 */
    void  SetRectEmpty(RECT* r);                                     /* USER32 */
    BOOL  IsRectEmpty(const RECT* r);                                /* USER32 */
    BOOL  OffsetRect(RECT* r, int dx, int dy);                       /* USER32 */
}

/* Tilemap */
extern "C" {
    void TileMap_InvalidateRect(void* tilemap, int left, int top,
                                int right, int bottom);            /* 0x455840 */
}

/* Resource Manager */
extern void* ResourceManager_GetById(void* resmgr, int id);      /* 0x446xxx */
extern void  RESMGR_ReleaseSoundResource(void* resource);         /* 0x447xxx */

/* Audio */
extern void GameAudio_AllocChannel(void* audio, int res_id, void** out_ch,
                                   int x, int y, int volume, int immediate);  /* 0x450xxx */
extern void CGWND_AudioChannel_Release(void* channel);                  /* 0x406xxx */
extern void CGWND_AudioChannel_Play(void* channel);                     /* 0x406xxx */
extern void CGWND_AudioChannel_Stop(void* channel);                     /* 0x40EE00 */
extern void CGWND_AudioChannel_UpdatePosition(void* channel, int x, int y); /* 0x406xxx */

/* Surface blitting */
extern void UIPANEL_Blit(void* panel, int dst_left, int dst_top,
                          int dst_right, int dst_bottom,
                          void* surface, int src_left, int src_top,
                          int src_right, int src_bottom, uint32_t flags);

/* Memory */
extern void GLOBAL_free(void* p);                                 /* 0x465CD0 */

/* Global variables */
extern void*  g_primary_surface;     /* primary DirectDraw surface */
extern void*  g_resmgr;              /* global resource manager singleton */
extern void*  g_audio;               /* global GameAudio singleton */
extern uint32_t g_game_time;         /* 0x4A99B4 — game tick counter */
extern HWND    g_main_window;        /* 0x4AA4A0 */
extern double  _DAT_00481170;        /* FPS limit threshold */
extern char    g_empty_string;       /* empty string constant */
extern void*   g_tilemap;            /* 0x4AAD08 — tilemap singleton */


/* ================================================================== */
/* GameObject::GameObject() — Base constructor                         */
/* Address: 0x4369D0                                                  */
/* ================================================================== */
GameObject::GameObject()
{
    /* Clear the two callback slots at +0x1C and +0x20. */
    this->callback_1 = nullptr;
    this->callback_2 = nullptr;

    /* In the binary: sets vtable = 0x477820 (managed by compiler here). */

    /* Type 1 = GameObject */
    this->type = 1;

    /* Clear screen rectangle */
    SetRect(&this->screen_rect, 0, 0, 0, 0);

    /* Mark as initialized */
    this->initialized = 1;
}


/* ================================================================== */
/* GameObject::Update / RegisterEntity / InitBase — base no-op hooks   */
/*                                                                     */
/* GameObject itself has no per-frame animation or resource state;    */
/* Entity overrides these with the real implementations below. They   */
/* exist on GameObject so generic dispatch code holding a GameObject* */
/* (e.g. TrainEntity_DeserializeFactory) can call them without a       */
/* downcast.                                                            */
/* ================================================================== */
void GameObject::Update()
{
}

void GameObject::RegisterEntity(void* context, void* entity)
{
    (void)context;
    (void)entity;
}

int GameObject::InitBase(int resource_id, int anim_index, bool force_reload)
{
    (void)resource_id;
    (void)anim_index;
    (void)force_reload;
    return 0;
}


/* ================================================================== */
/* Entity::~Entity() — Resource-releasing destructor body              */
/* Address: 0x405870                                                   */
/* ================================================================== */
Entity::~Entity()
{
    /* In the binary: resets vtable to Entity vtable (0x477488) so virtual
     * dispatch remains valid during derived-class teardown. In natural C++
     * the compiler handles vtable adjustments during destruction. */

    /* Release audio channel if active (+0x48). */
    void* audio_ch = this->audio_channel;
    if (audio_ch != nullptr) {
        CGWND_AudioChannel_Release(audio_ch);
        this->active_state = 0;  /* clear active state */
    }

    /* Release resource reference (+0x40) */
    void* resource = this->resource;
    if (resource != nullptr) {
        /* If resource has a locked flag at +0x162, invalidate rect
         * and release via resource->vtable[2] */
        if (*field_at<uint8_t>(resource, 0x162) == 1) {
            this->InvalidateRect();
            reinterpret_cast<ResourceDataView*>(resource)->release_surface();
        }
        this->resource = nullptr;
    }

    /* Release sound resource (+0x44) */
    void* snd_res = reinterpret_cast<void*>(
        static_cast<uintptr_t>(this->sound_res_id));
    if (snd_res != nullptr) {
        this->active_state = 0;
        RESMGR_ReleaseSoundResource(snd_res);
        this->sound_res_id = 0;
    }

    /* Mark dead in object manager */
    this->MarkDead();
}


/* ================================================================== */
/* GameObject::MarkDead — Non-virtual helper                           */
/* Address: 0x436A00                                                   */
/*                                                                     */
/* Marks this GameObject as dead by resetting its vtable to the base   */
/* GameObject vtable and clearing the initialized flag. This prevents  */
/* any further virtual dispatch or operations on the object after      */
/* destruction begins.                                                  */
/*                                                                     */
/* Also used by the base vtable[0] scalar dtor (0x412600) and unwind   */
/* handlers as a lightweight cleanup.                                   */
/* ================================================================== */
GameObject::~GameObject()
{
    MarkDead();
}

void GameObject::MarkDead()
{
    /* In the binary: also resets vtable to 0x477820 (base GameObject).
     * In natural C++ the compiler manages vtable during destruction. */
    this->initialized = 0;
}


/* ================================================================== */
/* GameObject::InvalidateRect — Vtable slot [1]                        */
/* Address: 0x436AB0                                                   */
/*                                                                     */
/* Copies screen_rect (+0x08) to a local variable and calls            */
/* TileMap_InvalidateRect on the global tilemap. This triggers a       */
/* redraw of the tile region, clearing any previously drawn sprite     */
/* at this position.                                                    */
/*                                                                     */
/* NOTE: Called with ECX=this and NO stack arguments. The old code's   */
/* vtable[1] call pattern `(this, 0)` was incorrect — this function   */
/* does not consume any stack parameters (plain RET, not RET N).       */
/* ================================================================== */
void GameObject::InvalidateRect()
{
    /* Copy screen_rect to stack (4 x int32_t = 16 bytes) */
    RECT r;
    r.left   = this->screen_rect.left;
    r.top    = this->screen_rect.top;
    r.right  = this->screen_rect.right;
    r.bottom = this->screen_rect.bottom;

    /* Flag tile region for redraw via tilemap */
    TileMap_InvalidateRect(&g_tilemap, r.left, r.top, r.right, r.bottom);
}


/* ================================================================== */
/* GameObject::PtInRect — Vtable slot [2]                              */
/* Address: 0x436A10                                                   */
/*                                                                     */
/* Returns TRUE if point (x, y) falls within this GameObject's         */
/* screen_rect (+0x08..+0x17). Uses half-open interval convention:     */
/*   left <= x < right  AND  top <= y < bottom                         */
/* ================================================================== */
BOOL GameObject::PtInRect(int x, int y)
{
    if (x >= this->screen_rect.left   &&
        x <  this->screen_rect.right  &&
        y >= this->screen_rect.top    &&
        y <  this->screen_rect.bottom)
    {
        return TRUE;
    }
    return FALSE;
}


/* ================================================================== */
/* GameObject::GetRelPos — Non-virtual helper                          */
/* Address: 0x436A40                                                   */
/*                                                                     */
/* Computes the relative position of (x, y) within this GameObject's   */
/* screen_rect. Used for hit-test offset calculations and drag         */
/* positioning.                                                         */
/*   out[0] = x - screen_rect.left                                     */
/*   out[1] = y - screen_rect.top                                      */
/* ================================================================== */
void GameObject::GetRelPos(int* out, int x, int y)
{
    out[0] = x - this->screen_rect.left;
    out[1] = y - this->screen_rect.top;
}


/* ================================================================== */
/* GameObject::MoveTo — Vtable slot [3] (base vtable only)             */
/* Address: 0x436A60                                                   */
/*                                                                     */
/* Teleports the GameObject to new absolute screen coordinates (x, y)  */
/* while preserving its current width and height.                      */
/*                                                                     */
/* 1. Calls InvalidateRect (vtable[1]) to invalidate the old position  */
/* 2. Updates screen_rect to {x, y, x+width, y+height} via SetRect    */
/* 3. Calls InvalidateRect again to invalidate the new position        */
/*                                                                     */
/* NOTE: This is vtable[3] only in the base GameObject vtable.         */
/* In Entity-derived classes, vtable[3] is overridden by HitTest,      */
/* so callers that need the real MoveTo behavior must use direct       */
/* scope: GameObject::MoveTo(x, y).                                    */
/* ================================================================== */
void GameObject::MoveTo(int x, int y)
{
    /* Invalidate old position */
    this->InvalidateRect();

    /* Compute new rect preserving width and height */
    int width  = this->screen_rect.right  - this->screen_rect.left;
    int height = this->screen_rect.bottom - this->screen_rect.top;

    SetRect(&this->screen_rect,
            x,
            y,
            x + width,
            y + height);

    /* Invalidate new position */
    this->InvalidateRect();
}

/* ================================================================== */
/* GameObject callback dispatchers — Vtable slots [4] and [5]          */
/* Addresses: 0x436AE0, 0x436B00                                      */
/* ================================================================== */
BOOL GameObject::InvokeCallback1(int x, int y)
{
    return callback_1 != nullptr ? callback_1(x, y) : FALSE;
}

BOOL GameObject::InvokeCallback2(int x, int y)
{
    return callback_2 != nullptr ? callback_2(x, y) : FALSE;
}


/* ================================================================== */
/* Scalar deleting destructor — vtable slot [0]                         */
/*                                                                     */
/* In the binary, MSVC generates compiler-synthesized scalar deleting   */
/* destructor wrappers for each vtable:                                 */
/*                                                                     */
/*   Base GameObject vtable (0x477820) entry at 0x412600:              */
/*     Calls MarkDead(). If flags & 1, calls GLOBAL_free().             */
/*     Used for objects destroyed while vtable is still the base class. */
/*                                                                     */
/*   Entity vtable (0x477488) entry at 0x405850:                       */
/*     Calls ~GameObject() body. If flags & 1, calls GLOBAL_free().    */
/*     Used for all Entity-derived objects.                             */
/*                                                                     */
/* In natural C++, the compiler generates equivalent wrappers           */
/* automatically from `virtual ~GameObject()`. The `flags & 1` check    */
/* corresponds to `delete obj` (free) vs explicit `obj->~GameObject()`  */
/* (no free). GLOBAL_free is the original binary's operator delete.     */
/* ================================================================== */


/* ================================================================== */
/* Entity::InitBase — Vtable slot [6]                              */
/* Address: 0x405900                                                   */
/* ================================================================== */
int Entity::InitBase(int resource_id, int anim_index, bool force_reload)
{
    void* resource = this->resource;

    /* Reset timer */
    this->timer = 0;

    /* Check if we already have the right resource loaded */
    if (resource == nullptr || force_reload ||
        *field_at<int32_t>(resource, 4) != resource_id)
    {
        /* Mark not initialized until resource loads */
        this->initialized = 1;  /* still 1 during load attempt */

        /* Release old resource if any */
        if (resource != nullptr) {
            /* vtable[1] — invalidate */
            this->InvalidateRect();

            reinterpret_cast<ResourceDataView*>(this->resource)->release_surface();
            this->resource = nullptr;
        }

        /* Load new resource if valid ID */
        if (resource_id > 0) {
            resource = ResourceManager_GetById(&g_resmgr, resource_id);
            this->resource = resource;
        }

        /* If no resource loaded, play "no resource" animation and bail */
        if (this->resource == nullptr) {
            this->PlayAnimation(-1);
            this->initialized = 0;
            return 0;
        }

        /* Acquire the resource surface through its typed slot 1 method. */
        reinterpret_cast<ResourceDataView*>(this->resource)->acquire_surface(
            this->world_x_raw, this->world_y_raw);

        /* If no surface data (flags at +0x10 == 0), bail */
        resource = this->resource;
        if (*field_at<uint32_t>(resource, 0x10) == 0) {
            this->initialized = 0;
            return 0;
        }

        /* Set screen_rect from resource frame dimensions */
        uint16_t frame_w = *field_at<uint16_t>(resource, 0x14);
        uint16_t frame_h = *field_at<uint16_t>(resource, 0x16);
        SetRect(&this->screen_rect,
                this->screen_rect.left,
                this->screen_rect.top,
                this->screen_rect.left + frame_w,
                this->screen_rect.top + frame_h);

        /* Set source_rect to full frame dimensions */
        SetRect(&this->source_rect, 0, 0, frame_w, frame_h);

        /* Mark anim_index as "unset" */
        this->anim_index = -1;
    }

    /* Copy default blit flags from resource (+0x164) */
    resource = this->resource;
    this->blit_flags = *field_at<uint32_t>(resource, 0x164);

    /* If no anim_index specified, use resource's default */
    if (anim_index < 0) {
        resource = this->resource;
        anim_index = *field_at<int16_t>(resource, 0x1E);
    }

    /* Select animation via vtable[14] = SetAnimState */
    int result = this->SetAnimState(anim_index);

    /* Check if animation was set successfully */
    if (this->anim_index != -1) {
        return 1;
    }

    this->initialized = 0;
    return 0;
}


/* ================================================================== */
/* Entity::StopSound — Vtable slot [7]                             */
/* Address: 0x405A20                                                   */
/* ================================================================== */
void Entity::StopSound(int param)
{
    void* audio_ch = this->audio_channel;
    if (audio_ch != nullptr) {
        CGWND_AudioChannel_Release(audio_ch);
        this->active_state = 0;  /* clear active state */
    }
    /* Re-trigger animation state selection (vtable[14] = SetAnimState).
     * In the binary: calls vtable[14] via dispatch; in natural C++ this
     * is a virtual call that derived classes may override. */
    this->SetAnimState(param);
}


/* ================================================================== */
/* Entity::SetAnimState — Vtable slot [14]                         */
/* Address: 0x405A50                                                   */
/* ================================================================== */
int Entity::SetAnimState(int anim_index)
{
    if (this->initialized != 1) {
        return 0;
    }

    void* resource = this->resource;
    uint16_t anim_count = *field_at<uint16_t>(resource, 0x1A);

    if (anim_index >= 0 && anim_index < anim_count) {
        this->anim_index = anim_index;

        /* Get FrameData pointer: resource->anim_table[anim_index] */
        FrameData* fd = *field_at<FrameData*>(resource, 0x20)
                        + anim_index;

        /* Reset phase tracking */
        this->phase_timer = 0;  /* phase_timer */
        this->timer = 0;  /* timer */

        /* Set starting frame from FrameData.start_frame */
        uint16_t start_frame = fd->start_frame;
        this->frame_index = start_frame;

        /* Update source rect via vtable[8] = SetFrame */
        this->SetFrame(start_frame, true);

        /* Play sound from FrameData.audio_res_id */
        this->PlayAnimation(fd->audio_res_id);
    }

    return this->frame_index;
}


/* ================================================================== */
/* Entity::PlayAnimation — Play sound for a resource ID            */
/* Address: 0x405AB0                                                   */
/* ================================================================== */
void Entity::PlayAnimation(int sound_id)
{
    /* Bail if no audio system or null sound */
    if (g_audio == nullptr || sound_id == 0) {
        return;
    }

    void* resource = this->resource;
    uint32_t* active_state = &this->active_state;

    /* If different sound than current, release old */
    if (sound_id != this->active_state) {
        void* audio_ch = this->audio_channel;
        if (audio_ch != nullptr) {
            CGWND_AudioChannel_Release(audio_ch);
            *active_state = 0;
        }
        this->next_sound_time = 0;  /* next_sound_time = 0 */

        /* Load sound resource via ResourceManager */
        void* sound_resource = ResourceManager_GetById(&g_resmgr, sound_id);
        const int snd_res = static_cast<int>(
            reinterpret_cast<uintptr_t>(sound_resource));
        this->sound_res_id = static_cast<uint32_t>(snd_res);

        /* Validate sound resource: if byte +0x09 != 1, not a valid sound */
        if (sound_resource != nullptr &&
            *field_at<uint8_t>(sound_resource, 9) != 1) {
            this->sound_res_id = 0;
        }
    }

    int snd_res = static_cast<int>(this->sound_res_id);

    /* Accept sound_id == -1 as "no sound" marker */
    if (snd_res != 0 || sound_id == -1) {
        *active_state = sound_id;
    }

    /* If no valid sound resource, nothing to play */
    if (snd_res == 0) {
        return;
    }

    /* Get FrameData for current animation */
    FrameData* fd = *field_at<FrameData*>(resource, 0x20)
                    + this->anim_index;

    void** audio_ch_ptr = &this->audio_channel;

    if (*audio_ch_ptr == nullptr) {
        /* Allocate new audio channel from GameAudio */
        if (fd->audio_delay == 0) {
            /* Immediate playback */
            GameAudio_AllocChannel(g_audio, snd_res, audio_ch_ptr,
                                   this->screen_rect.left,
                                   this->screen_rect.top,
                                   fd->volume, 1);
            return;
        }

        /* Delayed playback */
        GameAudio_AllocChannel(g_audio, snd_res, audio_ch_ptr,
                               this->screen_rect.left,
                               this->screen_rect.top,
                               fd->volume, 0);

        /* Calculate next sound time with optional random offset */
        int delay = fd->audio_delay;
        if (delay > 0) {
            if (delay > 0) {
                uint32_t r = CRT_rand();
                this->next_sound_time =
                    g_game_time + (r % delay) + 1;
                return;
            }
            if (delay != 2) {
                uint32_t r = CRT_rand();
                delay = (r % (2 - delay)) + delay;
            }
            this->next_sound_time = g_game_time + delay;
        }
    } else {
        /* Channel already allocated — check if it's time to play */
        if (fd->audio_delay > 0 &&
            this->next_sound_time < g_game_time)
        {
            CGWND_AudioChannel_Play(*audio_ch_ptr);
        }
    }
}


/* ================================================================== */
/* Entity::MoveTo — Set world position                        */
/* Address: 0x405C00                                                   */
/*                                                                     */
/* The binary calls GameObject_MoveTo (0x436A60) directly, not through */
/* vtable dispatch. In Entity-derived classes, vtable[3] is overridden */
/* by HitTest (0x405680), so a virtual call to MoveTo() would dispatch */
/* to the wrong function. Use explicit scope: GameObject::MoveTo().   */
/* ================================================================== */
void Entity::MoveTo(int x, int y)
{
    /* Update screen_rect position preserving size, invalidate old/new.
     * Direct call to GameObject::MoveTo bypasses vtable dispatch.       */
    GameObject::MoveTo(x, y);

    /* Apply resource offset to get final world position */
    void* resource = this->resource;
    this->world_x = *field_at<int16_t>(resource, 0x32) + x;
    this->world_y = *field_at<int16_t>(resource, 0x34) + y;

    /* Update audio channel position if active */
    void* audio_ch = this->audio_channel;
    if (audio_ch != nullptr) {
        CGWND_AudioChannel_UpdatePosition(audio_ch, x, y);
    }
}


/* ================================================================== */
/* Entity::Update — Animation state machine                        */
/* Address: 0x405C40                                                   */
/* ================================================================== */
void Entity::Update()
{
    if (this->initialized != 1) {
        return;
    }

    void* resource = this->resource;
    FrameData* fd = *field_at<FrameData*>(resource, 0x20)
                    + this->anim_index;

    uint16_t start_frame = fd->start_frame;
    uint16_t end_frame   = fd->end_frame;
    int32_t cur_frame    = this->frame_index;
    uint8_t  waiting     = this->waiting_flag;
    uint32_t phase_timer = this->phase_timer;

    /* Single-frame animation with no-loop flag — never updates */
    if (start_frame == end_frame && fd->sound_fx_index < 0) {
        return;
    }

    /* Already at end frame with no-loop flag — done */
    if (cur_frame == end_frame && fd->sound_fx_index < 0) {
        return;
    }

    /* Check if waiting at animation boundary */
    if (waiting == 1) {
        /* fps_limit check: DAT_00481170 vs g_main_window+0x11 */
        if (_DAT_00481170 < static_cast<double>(
                *field_at<uint8_t>(g_main_window, 0x11)) &&
            fd->wait_time > 0)
        {
            return;
        }
        /* Still within wait period */
        if (g_game_time < this->timer) {
            return;
        }
    }

    int32_t new_frame;
    uint8_t step_mode = *field_at<uint8_t>(fd, 0x17);

    if (step_mode == 0) {
        /* Normal step mode: advance by 1 per step_delay ticks */
        phase_timer = phase_timer + 1;
        this->phase_timer = phase_timer;

        if (start_frame < end_frame) {
            /* Forward animation */
            int step = static_cast<int16_t>(phase_timer / fd->step_delay);
            new_frame = step + start_frame;

            if (new_frame > end_frame) {
                waiting = 1;
                new_frame = end_frame;
            }
        } else {
            /* Reverse animation */
            int step = static_cast<int16_t>(phase_timer / fd->step_delay);
            new_frame = static_cast<int32_t>(start_frame) - step;

            if (new_frame < end_frame) {
                waiting = 1;
                new_frame = end_frame;
            }
        }
    } else {
        /* Step mode 1: advance by 2 per tick (skip odd frames) */
        phase_timer = phase_timer + 2;
        this->phase_timer = phase_timer;

        if (start_frame < end_frame) {
            int16_t step = static_cast<int16_t>(phase_timer / fd->step_delay)
                         + start_frame;
            new_frame = static_cast<int32_t>(step) & ~1;  /* sign-extend, then force even */

            if (new_frame > end_frame) {
                waiting = 1;
                new_frame = end_frame;
            }
        } else {
            int16_t step = start_frame -
                           static_cast<int16_t>(phase_timer / fd->step_delay) + 1;
            new_frame = static_cast<int32_t>(step) & ~1;  /* sign-extend, then force even */

            if (new_frame < end_frame) {
                waiting = 1;
                new_frame = end_frame;
            }
        }
    }

    /* The byte at +0x70 is both the current waiting state and the
     * first-boundary latch. There is no separate byte at +0x71. */
    if (waiting == 1 || new_frame == cur_frame) {
        if (this->waiting_flag == 0) {
            this->waiting_flag = 1;
            this->timer = fd->wait_time + g_game_time;
        } else {
            this->PlayAnimation(fd->sound_fx_index);
            this->waiting_flag = 0;
        }
    }

    /* Update frame if changed */
    if (this->frame_index != new_frame) {
        this->SetFrame(new_frame, true);
    }
}


/* ================================================================== */
/* Entity::SetFrame — Vtable slot [8]                              */
/* Address: 0x405DE0                                                   */
/*                                                                     */
/* NOTE: vtable[1] (InvalidateRect) is called with NO stack args.      */
/* The binary does NOT push 0 before the call; the old code's           */
/* `(this, 0)` pattern was incorrect.                                   */
/* ================================================================== */
void Entity::SetFrame(int frame_id, bool trigger_invalidate)
{
    if (this->initialized != 1) {
        return;
    }

    this->frame_index = frame_id;

    void* resource = this->resource;
    uint16_t frame_w = *field_at<uint16_t>(resource, 0x14);

    /* Compute source rect X offsets from frame index and width */
    this->source_rect.left  = frame_id * frame_w;
    this->source_rect.right = (frame_id + 1) * frame_w;

    if (trigger_invalidate) {
        this->InvalidateRect();
    }
}


/* ================================================================== */
/* Entity::SetName — Vtable slot [13]                              */
/* Address: 0x405E20                                                   */
/* ================================================================== */
/* ================================================================== */
/* Entity::SetVisible — Vtable slot [9]                                */
/* Address: 0x4061B0                                                   */
/* ================================================================== */
void Entity::SetVisible(bool is_visible)
{
    this->visible = is_visible;
    this->InvalidateRect();

    if (this->audio_channel != nullptr) {
        if (is_visible) {
            CGWND_AudioChannel_Play(this->audio_channel);
        } else {
            CGWND_AudioChannel_Stop(this->audio_channel);
        }
    }
}


void Entity::SetName(const char* name)
{
    this->CopyName(name);
}


/* ================================================================== */
/* Entity::CopyName — Name copy helper (non-virtual)               */
/* Address: 0x405E20 (same body as SetName)                            */
/* ================================================================== */
void Entity::CopyName(const char* name)
{
    /* Validate: first char must be alphanumeric or null */
    if (IsCharAlphaNumericA(*name) || *name == '\0') {
        _strncpy(this->name, name, 10);
    }
    this->name[10] = 0;  /* null terminate */
}


/* ================================================================== */
/* Entity::Draw — Vtable slot [11]                                 */
/* Address: 0x405E60                                                   */
/* ================================================================== */
void Entity::Draw(RECT clip_bounds, int enable_scroll, uint32_t extra_flags)
{
    void* resource = this->resource;

    /* Bail if no surface or not visible */
    if (*field_at<int>(resource, 0x10) == 0 || this->visible != 1) {
        return;
    }

    RECT clipped;
    if (!IntersectRect(&clipped, &this->screen_rect, &clip_bounds)) {
        return;
    }

    uint32_t flags = extra_flags | this->blit_flags;

    /* Check for horizontal flip in FrameData */
    FrameData* fd = *field_at<FrameData*>(resource, 0x20)
                    + this->anim_index;

    int src_left, src_top, src_right, src_bottom;

    if (fd->flip_horizontal == 1) {
        /* Mirrored: reverse source X */
        src_left  = (this->source_rect.right - clipped.left) + this->screen_rect.left;
        src_right = (this->screen_rect.right + this->source_rect.left) - clipped.right;

        flags |= 0x20;  /* DDBLTFX_MIRRORLEFTRIGHT or equivalent */

        if (src_left < src_right) {
            src_bottom = (this->source_rect.bottom - this->screen_rect.bottom) + clipped.bottom;
            src_top    = clipped.top - this->screen_rect.top;
            int temp   = src_left;
            src_left   = src_right;
            src_right  = temp;
        } else {
            src_bottom = (this->source_rect.bottom - this->screen_rect.bottom) + clipped.bottom;
            src_top    = clipped.top - this->screen_rect.top;
        }
    } else {
        /* Normal orientation */
        src_bottom = (this->source_rect.bottom - this->screen_rect.bottom) + clipped.bottom;
        src_right  = (this->source_rect.right - this->screen_rect.right) + clipped.right;
        src_top    = clipped.top - this->screen_rect.top;
        src_left   = (this->source_rect.left + clipped.left) - this->screen_rect.left;
    }

    if (enable_scroll == 1) {
        flags |= 0x40;
    }

    /* Blit via UIPANEL */
    UIPANEL_Blit(*field_at<void*>(resource, 0x10),
                 clipped.left, clipped.top, clipped.right, clipped.bottom,
                 g_primary_surface,
                 src_left, src_top, src_right, src_bottom,
                 flags);
}


/* ================================================================== */
/* Entity::DrawConnected — Vtable slot [12]                        */
/* Address: 0x405FD0                                                   */
/* ================================================================== */
void Entity::DrawConnected(RECT clip_bounds, int enable_scroll, uint32_t extra_flags)
{
    /* Bail if not visible or not a connected sprite */
    if (this->visible != 1) {
        return;
    }

    void* resource = this->resource;
    FrameData* fd = *field_at<FrameData*>(resource, 0x20)
                    + this->anim_index;

    if (*field_at<uint8_t>(fd, 0x17) == 0) {
        return;
    }

    RECT clipped;
    if (!IntersectRect(&clipped, &this->screen_rect, &clip_bounds)) {
        return;
    }

    uint32_t flags = extra_flags | this->blit_flags;

    int src_left, src_top, src_right, src_bottom;

    if (fd->flip_horizontal == 0) {
        /* Normal */
        src_bottom = (clipped.bottom - this->screen_rect.bottom) + this->source_rect.bottom;
        src_right  = (clipped.right - this->screen_rect.right) + this->source_rect.right;
        src_top    = clipped.top - this->screen_rect.top;
        src_left   = (clipped.left - this->screen_rect.left) + this->source_rect.left;
    } else {
        /* Mirrored */
        src_left  = (this->source_rect.right - clipped.left) + this->screen_rect.left;
        src_right = (this->screen_rect.right - clipped.right) + this->source_rect.left;

        flags |= 0x20;

        if (src_left < src_right) {
            src_bottom = (clipped.bottom - this->screen_rect.bottom) + this->source_rect.bottom;
            src_top    = clipped.top - this->screen_rect.top;
            int temp   = src_left;
            src_left   = src_right;
            src_right  = temp;
        } else {
            src_bottom = (clipped.bottom - this->screen_rect.bottom) + this->source_rect.bottom;
            src_top    = clipped.top - this->screen_rect.top;
        }
    }

    if (enable_scroll == 1) {
        flags |= 0x40;
    }

    /* Increment frame temporarily for connected tile */
    int32_t cur_frame = this->frame_index;
    this->SetFrame(cur_frame + 1, false);

    /* Second source rect for the connected tile */
    RECT src_rect;
    int src2_left  = (clipped.right - this->screen_rect.left) + this->source_rect.left;
    int src2_top   = clipped.bottom - this->screen_rect.top;
    int src2_right = (clipped.left - this->screen_rect.right) + this->source_rect.right;
    int src2_bottom= (clipped.top - this->screen_rect.bottom) + this->source_rect.bottom;

    SetRect(&src_rect, src2_left, src2_top, src2_right, src2_bottom);

    /* Blit connected tile */
    UIPANEL_Blit(*field_at<void*>(resource, 0x10),
                 clipped.right, clipped.bottom,
                 clipped.left, clipped.top,
                 g_primary_surface,
                 src2_right, src2_bottom, src_rect.left, src_rect.top,
                 flags);

    /* Restore the frame that preceded the temporary increment. */
    this->SetFrame(this->frame_index - 1, false);
}


/* ================================================================== */
/* Entity::GetBoundingRect — Get bounding rect from resource       */
/* Address: 0x4583C0                                                   */
/*                                                                     */
/* Retrieves the bounding rectangle from the resource at +0x61C,       */
/* then offsets it by the object's screen_rect position. Returns       */
/* TRUE (1) on success, FALSE (0) if out_rect is NULL, resource is     */
/* missing, or the bounding rect is empty.                              */
/* ================================================================== */
BOOL Entity::GetBoundingRect(RECT* out_rect)
{
    void* resource = this->resource;  /* +0x40 */
    if (out_rect == nullptr || resource == nullptr) {
        return FALSE;
    }

    SetRectEmpty(out_rect);

    RECT* res_rect = field_at<RECT>(resource, 0x61C);
    if (IsRectEmpty(res_rect)) {
        return FALSE;
    }

    out_rect->left   = res_rect->left;
    out_rect->top    = res_rect->top;
    out_rect->right  = res_rect->right;
    out_rect->bottom = res_rect->bottom;

    OffsetRect(out_rect, this->screen_rect.left, this->screen_rect.top);
    return TRUE;
}
