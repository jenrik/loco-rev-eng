/**
 * GameObject.cpp — Implementation of the root GameObject base class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * All method body addresses reference the original loco.exe binary.
 */

#include "GameObject.h"
#include "../shared/vtable_addrs.h"

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
    BOOL IntersectRect(RECT* out, const RECT* a, const RECT* b);         /* USER32 */
    BOOL PtInRect(const RECT* r, uint32_t packedXY);                      /* USER32 */
    void SetRect(RECT* r, int left, int top, int right, int bottom);     /* USER32 */
}

/* Resource Manager */
extern void* ResourceManager_GetById(void* resmgr, int id);             /* 0x446xxx */
extern void  RESMGR_ReleaseSoundResource(void* resource);                /* 0x447xxx */

/* Audio */
extern void GameAudio_AllocChannel(void* audio, int res_id, void** out_ch,
                                   int x, int y, int volume, int immediate);  /* 0x450xxx */
extern void CGWND_AudioChannel_Release(void* channel);                  /* 0x406xxx */
extern void CGWND_AudioChannel_Play(void* channel);                     /* 0x406xxx */
extern void CGWND_AudioChannel_UpdatePosition(void* channel, int x, int y); /* 0x406xxx */

/* Surface blitting */
extern void UIPANEL_Blit(void* panel, int dst_left, int dst_top,
                          int dst_right, int dst_bottom,
                          void* surface, int src_left, int src_top,
                          int src_right, int src_bottom, uint32_t flags);

/* Object lifecycle helpers */
extern void GameObject_InvalidateRect(int* obj);                         /* 0x405xxxx */
extern void GameObject_MarkDead(void** obj);                             /* 0x405xxxx */
extern void GameObject_MoveTo_raw(void* obj, int x, int y);             /* 0x405Bxx */

/* Global variables */
extern void* g_primary_surface;     /* primary DirectDraw surface */
extern void* g_resmgr;              /* global resource manager singleton */
extern void* g_audio;               /* global GameAudio singleton */
extern uint32_t g_game_time;        /* 0x4A99B4 — game tick counter */
extern HWND   g_main_window;        /* 0x4AA4A0 */
extern double  _DAT_00481170;       /* FPS limit threshold */
extern char   g_empty_string;       /* empty string constant */


/* ================================================================== */
/* GameObject::GameObject() — Base constructor                         */
/* Address: 0x4369D0                                                  */
/*                                                                     */
/* Called by:                                                          */
/*   GameObject_BaseCtor (0x4057B0) — Entity-level base constructor   */
/*   CGWND_TrackPiece_Ctor (0x40CFBD) — track piece constructor       */
/*                                                                     */
/* Zeroes +0x1C/+0x20, sets vtable=0x477820, type=1, clears screen    */
/* rect, marks initialized.                                            */
/* ================================================================== */
GameObject::GameObject()
{
    /* Zero out two padding fields */
    this->_pad_1C = 0;
    this->_pad_20 = 0;

    /* Set root vtable (0x477820) — will be overridden by derived classes */
    this->vtable = (void**)VTBL_GAMEOBJECT;

    /* Type 1 = GameObject */
    this->type = 1;

    /* Clear screen rectangle */
    SetRect(&this->screen_rect, 0, 0, 0, 0);

    /* Mark as initialized */
    this->initialized = 1;
}


/* ================================================================== */
/* GameObject::~GameObject() — Destructor body                         */
/* Address: 0x405870                                                   */
/*                                                                     */
/* Called by:                                                          */
/*   GameObject::scalar_deleting_destructor (0x405850) — vtable[0]    */
/*   Derived class dtors (Building_Dtor, etc.)                          */
/*                                                                     */
/* Releases audio channel (+0x48), resource reference (+0x40), sound   */
/* resource (+0x44), then marks object dead in manager. Uses SEH       */
/* (__try/__except) to guard cleanup.                                  */
/* ================================================================== */
GameObject::~GameObject()
{
    /* Reset vtable to Entity vtable — preserved during destruction
     * so that if a derived dtor already changed it, we still have
     * valid virtual dispatch for cleanup.                                */
    this->vtable = (void**)VTBL_ENTITY;

    /* Release audio channel if active (+0x48).
     * Note: offset +0x48 / sizeof(void*) = 0x12 * 4 = 0x48.
     * The Ghidra decomp uses param_1[0x12] for this.                    */
    void* audio_ch = *(void**)((uint8_t*)this + 0x48);
    if (audio_ch != nullptr) {
        CGWND_AudioChannel_Release(audio_ch);
        *(uint32_t*)((uint8_t*)this + 0x5C) = 0;  /* clear active state */
    }

    /* Release resource reference (+0x40) */
    void* resource = *(void**)((uint8_t*)this + 0x40);
    if (resource != nullptr) {
        /* If resource has a locked flag at +0x162, invalidate rect
         * and release via vtable[2] (resource's Release method) */
        if (*(uint8_t*)((uint8_t*)resource + 0x162) == 1) {
            GameObject_InvalidateRect((int*)this);
            /* Call resource's Release() — vtable[2] at resource+0x00 */
            void** res_vtbl = *(void***)resource;
            ((void(*)())res_vtbl[2])();
        }
        *(void**)((uint8_t*)this + 0x40) = nullptr;
    }

    /* Release sound resource (+0x44) */
    void* snd_res = *(void**)((uint8_t*)this + 0x44);
    if (snd_res != nullptr) {
        *(uint32_t*)((uint8_t*)this + 0x5C) = 0;
        RESMGR_ReleaseSoundResource(snd_res);
        *(void**)((uint8_t*)this + 0x44) = nullptr;
    }

    /* Mark dead in object manager */
    GameObject_MarkDead((void**)this);
}


/* ================================================================== */
/* GameObject::scalar_deleting_destructor — Vtable slot [0]            */
/* Address: 0x405850                                                   */
/*                                                                     */
/* Standard MSVC scalar-deleting destructor pattern:                   */
/*   1. Call destructor body (~GameObject)                             */
/*   2. If flags & 1, free heap memory                                 */
/* ================================================================== */
void* GameObject::scalar_deleting_destructor(byte flags)
{
    this->~GameObject();
    if (flags & 1) {
        /* GLOBAL_free at 0x465CD0 */
        extern void GLOBAL_free(void*);
        GLOBAL_free(this);
    }
    return this;
}


/* ================================================================== */
/* GameObject::InitBase — Vtable slot [6]                              */
/* Address: 0x405900                                                   */
/*                                                                     */
/* Called by:                                                          */
/*   GameObject_BaseCtor (0x40582D)                                    */
/*   Panel_Init (0x454696)                                             */
/*   RESDATA_SetPosition (0x454830)                                    */
/*   10+ vtable entries reference this as InitBase                      */
/*                                                                     */
/* Loads a resource by ID, sets up bounding rects from frame dims,     */
/* initializes animation state via SetAnimState (vtable[7]).           */
/* Returns 1 on success, 0 on failure.                                 */
/* ================================================================== */
int GameObject::InitBase(int resource_id, int anim_index, bool force_reload)
{
    void* resource = *(void**)((uint8_t*)this + 0x40);

    /* Reset timer */
    *(uint32_t*)((uint8_t*)this + 0x58) = 0;

    /* Check if we already have the right resource loaded */
    if (resource == nullptr || force_reload ||
        *(int32_t*)((uint8_t*)resource + 4) != resource_id)
    {
        /* Mark not initialized until resource loads */
        this->initialized = 1;  /* still 1 during load attempt */

        /* Release old resource if any — vtable[1] = StopSound, then vtable[2] = Release */
        if (resource != nullptr) {
            /* vtable[1] — invalidate/stop */
            ((void(*)(void*,int))this->vtable[1])(this, 0);

            /* resource->vtable[2] — Release() */
            void** res_vtbl = *(void***)((uint8_t*)this + 0x40);
            ((void(*)())res_vtbl[2])();

            *(void**)((uint8_t*)this + 0x40) = nullptr;
        }

        /* Load new resource if valid ID */
        if (resource_id > 0) {
            resource = ResourceManager_GetById(&g_resmgr, resource_id);
            *(void**)((uint8_t*)this + 0x40) = resource;
        }

        /* If no resource loaded, play "no resource" animation and bail */
        if (*(void**)((uint8_t*)this + 0x40) == nullptr) {
            this->PlayAnimation(-1);
            this->initialized = 0;
            return 0;
        }

        /* Lock/get surface via resource vtable[1] */
        void** res_vtbl = *(void***)((uint8_t*)this + 0x40);
        int raw_x = *(int32_t*)((uint8_t*)this + 0x74);
        int raw_y = *(int32_t*)((uint8_t*)this + 0x78);
        ((void(*)(int,int))res_vtbl[1])(raw_x, raw_y);

        /* If no surface data (flags at +0x10 == 0), bail */
        resource = *(void**)((uint8_t*)this + 0x40);
        if (*(uint32_t*)((uint8_t*)resource + 0x10) == 0) {
            this->initialized = 0;
            return 0;
        }

        /* Set screen_rect from resource frame dimensions */
        uint16_t frame_w = *(uint16_t*)((uint8_t*)resource + 0x14);
        uint16_t frame_h = *(uint16_t*)((uint8_t*)resource + 0x16);
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
    resource = *(void**)((uint8_t*)this + 0x40);
    this->blit_flags = *(uint32_t*)((uint8_t*)resource + 0x164);

    /* If no anim_index specified, use resource's default */
    if (anim_index < 0) {
        resource = *(void**)((uint8_t*)this + 0x40);
        anim_index = *(int16_t*)((uint8_t*)resource + 0x1E);
    }

    /* Select animation via vtable[7] = SetAnimState */
    int result = this->SetAnimState(anim_index);

    /* Check if animation was set successfully */
    if (this->anim_index != -1) {
        return 1;
    }

    this->initialized = 0;
    return 0;
}


/* ================================================================== */
/* GameObject::StopSound — Vtable slot [1]                             */
/* Address: 0x405A20                                                   */
/*                                                                     */
/* Called by: vtable dispatch                                          */
/*                                                                     */
/* Stops current audio playback. Releases the audio channel at +0x48   */
/* if active, then calls vtable[14] (AnimStateSelect) for the given    */
/* sound parameter.                                                     */
/* ================================================================== */
void GameObject::StopSound(int param)
{
    void* audio_ch = *(void**)((uint8_t*)this + 0x48);
    if (audio_ch != nullptr) {
        CGWND_AudioChannel_Release(audio_ch);
        *(uint32_t*)((uint8_t*)this + 0x5C) = 0;  /* clear active state */
    }
    /* vtable[14] — AnimStateSelect */
    this->AnimStateSelect(param);
}


/* ================================================================== */
/* GameObject::SetAnimState — Vtable slot [7]                          */
/* Address: 0x405A50                                                   */
/*                                                                     */
/* Called by:                                                          */
/*   GameObject::InitBase (0x405900) — initial animation setup        */
/*   GameObject::Update (0x405C40) — boundary animation switch        */
/*   Building_UpdateAnimBy... — occupancy/dimension-based anim change  */
/*                                                                     */
/* Sets the active animation to the given index in the resource's      */
/* FrameData table. Validates range, stores frame ID, computes byte    */
/* offsets, triggers redraw via SetFrame (vtable[8]), and plays the    */
/* associated sound. Returns the new frame index.                      */
/* ================================================================== */
int GameObject::SetAnimState(int anim_index)
{
    if (this->initialized != 1) {
        return 0;
    }

    void* resource = *(void**)((uint8_t*)this + 0x40);
    uint16_t anim_count = *(uint16_t*)((uint8_t*)resource + 0x1A);

    if (anim_index >= 0 && anim_index < anim_count) {
        this->anim_index = anim_index;

        /* Get FrameData pointer: resource->anim_table[anim_index]
         * FrameData is at resource+0x20, each entry is 0x18 bytes */
        FrameData* fd = (FrameData*)(*(uint8_t**)((uint8_t*)resource + 0x20)
                                     + anim_index * sizeof(FrameData));

        /* Reset phase tracking */
        *(uint32_t*)((uint8_t*)this + 0x6C) = 0;  /* phase_timer */
        *(uint32_t*)((uint8_t*)this + 0x58) = 0;  /* timer */

        /* Set starting frame from FrameData.start_frame */
        uint16_t start_frame = fd->start_frame;
        *(uint16_t*)((uint8_t*)this + 0x54) = start_frame;

        /* Update source rect via vtable[8] = SetFrame */
        this->SetFrame(start_frame, true);

        /* Play sound from FrameData.audio_res_id (offset 0x0E, field index 7) */
        this->PlayAnimation(fd->audio_res_id);
    }

    return *(uint16_t*)((uint8_t*)this + 0x54);
}


/* ================================================================== */
/* GameObject::PlayAnimation — Play sound for a resource ID            */
/* Address: 0x405AB0                                                   */
/*                                                                     */
/* Called by:                                                          */
/*   GameObject::SetAnimState (0x405A50)                               */
/*   GameObject::InitBase (0x405900) — failure case                   */
/*   External callers via vtable[14] = AnimStateSelect                 */
/*                                                                     */
/* Looks up the sound resource by ID, releases old audio channel if    */
/* needed, allocates a new channel from GameAudio, and schedules       */
/* playback. Supports immediate playback or random-delayed playback    */
/* based on FrameData::audio_delay field.                              */
/* ================================================================== */
void GameObject::PlayAnimation(int sound_id)
{
    /* Bail if no audio system or null sound */
    if (g_audio == nullptr || sound_id == 0) {
        return;
    }

    void* resource = *(void**)((uint8_t*)this + 0x40);
    uint32_t* active_state = (uint32_t*)((uint8_t*)this + 0x5C);

    /* If different sound than current, release old */
    if (sound_id != *(uint32_t*)((uint8_t*)this + 0x5C)) {
        void* audio_ch = *(void**)((uint8_t*)this + 0x48);
        if (audio_ch != nullptr) {
            CGWND_AudioChannel_Release(audio_ch);
            *active_state = 0;
        }
        *(uint32_t*)((uint8_t*)this + 0x60) = 0;  /* next_sound_time = 0 */

        /* Load sound resource via ResourceManager */
        int snd_res = (int)RESMGR_GetById(&g_resmgr, sound_id);
        *(int*)((uint8_t*)this + 0x44) = snd_res;

        /* Validate sound resource: if byte +0x09 != 1, not a valid sound */
        if (snd_res != 0 && *(uint8_t*)(snd_res + 9) != 1) {
            *(int*)((uint8_t*)this + 0x44) = 0;
        }
    }

    int snd_res = *(int*)((uint8_t*)this + 0x44);

    /* Accept sound_id == -1 as "no sound" marker */
    if (snd_res != 0 || sound_id == -1) {
        *active_state = sound_id;
    }

    /* If no valid sound resource, nothing to play */
    if (snd_res == 0) {
        return;
    }

    /* Get FrameData for current animation */
    FrameData* fd = (FrameData*)(*(uint8_t**)((uint8_t*)resource + 0x20)
                                 + this->anim_index * sizeof(FrameData));

    void** audio_ch_ptr = (void**)((uint8_t*)this + 0x48);

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
                *(uint32_t*)((uint8_t*)this + 0x60) =
                    g_game_time + (r % delay) + 1;
                return;
            }
            if (delay != 2) {
                uint32_t r = CRT_rand();
                delay = (r % (2 - delay)) + delay;
            }
            *(uint32_t*)((uint8_t*)this + 0x60) = g_game_time + delay;
        }
    } else {
        /* Channel already allocated — check if it's time to play */
        if (fd->audio_delay > 0 &&
            *(uint32_t*)((uint8_t*)this + 0x60) < g_game_time)
        {
            CGWND_AudioChannel_Play((uint32_t)*audio_ch_ptr);
        }
    }
}


/* ================================================================== */
/* GameObject::SetWorldPos — Set world position                        */
/* Address: 0x405C00                                                   */
/*                                                                     */
/* Called by:                                                          */
/*   Game_PlaySound (0x412019, 0x41204F)                               */
/*   UI_Construct (0x423415, 0x423252)                                 */
/*   RESDATA_SetPosition (0x454830)                                    */
/*   Various entity placement code                                     */
/*                                                                     */
/* Updates the object's world position. Stores raw coords, applies     */
/* resource offset for final screen position, and repositions the      */
/* audio channel.                                                      */
/* ================================================================== */
void GameObject::SetWorldPos(int x, int y)
{
    /* Store raw position */
    GameObject_MoveTo_raw(this, x, y);

    /* Apply resource offset to get final world position */
    void* resource = *(void**)((uint8_t*)this + 0x40);
    *(int32_t*)((uint8_t*)this + 0x4C) = *(int16_t*)((uint8_t*)resource + 0x32) + x;
    *(int32_t*)((uint8_t*)this + 0x50) = *(int16_t*)((uint8_t*)resource + 0x34) + y;

    /* Update audio channel position if active */
    void* audio_ch = *(void**)((uint8_t*)this + 0x48);
    if (audio_ch != nullptr) {
        CGWND_AudioChannel_UpdatePosition(audio_ch, x, y);
    }
}


/* ================================================================== */
/* GameObject::Update — Animation state machine                        */
/* Address: 0x405C40                                                   */
/*                                                                     */
/* Called by: per-frame game loop dispatch                             */
/*                                                                     */
/* Advances the sprite frame index based on FrameData timing.          */
/* Handles forward/reverse animation, ping-pong (loop at boundary),    */
/* pause-at-boundary (wait_time > 0), and step_mode (+2 per tick).     */
/* Dispatches SetFrame (vtable[8]) when frame changes.                 */
/* ================================================================== */
void GameObject::Update()
{
    if (this->initialized != 1) {
        return;
    }

    void* resource = *(void**)((uint8_t*)this + 0x40);
    FrameData* fd = (FrameData*)(*(uint8_t**)((uint8_t*)resource + 0x20)
                                 + this->anim_index * sizeof(FrameData));

    uint16_t start_frame = fd->start_frame;
    uint16_t end_frame   = fd->end_frame;
    uint16_t cur_frame   = *(uint16_t*)((uint8_t*)this + 0x54);
    uint8_t  waiting     = *(uint8_t*)((uint8_t*)this + 0x70);
    uint32_t phase_timer = *(uint32_t*)((uint8_t*)this + 0x6C);

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
        if (_DAT_00481170 < *(double*)((uint8_t*)g_main_window + 0x11) &&
            fd->wait_time > 0)
        {
            return;
        }
        /* Still within wait period */
        if (g_game_time < *(uint32_t*)((uint8_t*)this + 0x58)) {
            return;
        }
    }

    uint16_t new_frame;
    uint8_t step_mode = *(uint8_t*)((uint8_t*)fd + 0x17);

    if (step_mode == 0) {
        /* Normal step mode: advance by 1 per step_delay ticks */
        phase_timer = phase_timer + 1;
        *(uint32_t*)((uint8_t*)this + 0x6C) = phase_timer;

        if (start_frame < end_frame) {
            /* Forward animation */
            int step = (int16_t)(phase_timer / fd->step_delay);
            new_frame = (uint16_t)(step + start_frame);

            if (new_frame > end_frame) {
                /* Hit boundary */
                waiting = 1;
                new_frame = end_frame;
            }
        } else {
            /* Reverse animation */
            int step = (int16_t)(phase_timer / fd->step_delay);
            new_frame = (uint16_t)(start_frame - step);

            if (new_frame < end_frame) {
                /* Hit boundary */
                waiting = 1;
                new_frame = end_frame;
            }
        }
    } else {
        /* Step mode 1: advance by 2 per tick (skip odd frames) */
        phase_timer = phase_timer + 2;
        *(uint32_t*)((uint8_t*)this + 0x6C) = phase_timer;

        if (start_frame < end_frame) {
            int16_t step = (int16_t)(phase_timer / fd->step_delay) + start_frame;
            new_frame = (uint16_t)(step & 0xFFFE);  /* force even */

            if (new_frame > end_frame) {
                waiting = 1;
                new_frame = end_frame;
            }
        } else {
            int16_t step = (start_frame - (int16_t)(phase_timer / fd->step_delay)) + 1;
            new_frame = (uint16_t)(step & 0xFFFE);  /* force even */

            if (new_frame < end_frame) {
                waiting = 1;
                new_frame = end_frame;
            }
        }
    }

    /* Handle boundary */
    if (waiting == 0 && new_frame != cur_frame) {
        /* Normal case: frame changed within range */
        // frame will be updated below
    } else if (waiting == 1 || (waiting == 0 && new_frame == cur_frame)) {
        /* At boundary */
        if (this->initialized == 0) {
            /* First time hitting boundary — set wait timer */
            *(uint8_t*)((uint8_t*)this + 0x70) = 1;
            *(uint32_t*)((uint8_t*)this + 0x58) = fd->wait_time + g_game_time;
        } else {
            /* Already waited — call vtable[14] to select next animation */
            new_frame = this->AnimStateSelect(fd->sound_fx_index);
            *(uint8_t*)((uint8_t*)this + 0x70) = 0;
        }
    }

    /* Update frame if changed */
    if (*(uint16_t*)((uint8_t*)this + 0x54) != new_frame) {
        this->SetFrame(new_frame, true);
    }
}


/* ================================================================== */
/* GameObject::SetFrame — Vtable slot [8]                              */
/* Address: 0x405DE0                                                   */
/*                                                                     */
/* Stores the frame number at +0x54, computes source rect byte offsets */
/* from frame width, and optionally triggers invalidation.             */
/* ================================================================== */
void GameObject::SetFrame(int frame_id, bool trigger_invalidate)
{
    if (this->initialized != 1) {
        return;
    }

    *(int32_t*)((uint8_t*)this + 0x54) = frame_id;

    void* resource = *(void**)((uint8_t*)this + 0x40);
    uint16_t frame_w = *(uint16_t*)((uint8_t*)resource + 0x14);

    /* Compute source rect X offsets from frame index and width */
    this->source_rect.left  = frame_id * frame_w;
    this->source_rect.right = (frame_id + 1) * frame_w;

    if (trigger_invalidate) {
        /* vtable[1] — invalidate rect */
        ((void(*)(void*,int))this->vtable[1])(this, 0);
    }
}


/* ================================================================== */
/* GameObject::SetName — Vtable slot [9]                               */
/* Address: 0x405E20                                                   */
/*                                                                     */
/* Validates the first character (alpha-numeric or null allowed),      */
/* copies up to 10 characters to +0x7C, null-terminates at +0x86.      */
/* ================================================================== */
void GameObject::SetName(const char* name)
{
    this->CopyName(name);
}


/* ================================================================== */
/* GameObject::CopyName — Name copy helper (non-virtual)               */
/* Address: 0x405E20 (same body as SetName)                            */
/*                                                                     */
/* Called by Building_BaseCtor and other constructors that need        */
/* direct name access without virtual dispatch overhead.               */
/* ================================================================== */
void GameObject::CopyName(const char* name)
{
    /* Validate: first char must be alphanumeric or null */
    if (IsCharAlphaNumericA(*name) || *name == '\0') {
        _strncpy((char*)this + 0x7C, name, 10);
    }
    *(uint8_t*)((uint8_t*)this + 0x86) = 0;  /* null terminate */
}


/* ================================================================== */
/* GameObject::Draw — Vtable slot [10]                                 */
/* Address: 0x405E60                                                   */
/*                                                                     */
/* Renders the current sprite frame to the primary surface. Computes   */
/* source/dest rectangles, handles horizontal flip, clips to visible   */
/* area via IntersectRect, and blits via UIPANEL_Blit.                 */
/* ================================================================== */
void GameObject::Draw(RECT clip_bounds, int enable_scroll, uint32_t extra_flags)
{
    void* resource = *(void**)((uint8_t*)this + 0x40);

    /* Bail if no surface or not visible */
    if (*(int*)((uint8_t*)resource + 0x10) == 0 || this->visible != 1) {
        return;
    }

    RECT clipped;
    if (!IntersectRect(&clipped, &this->screen_rect, &clip_bounds)) {
        return;
    }

    uint32_t flags = extra_flags | this->blit_flags;

    /* Check for horizontal flip in FrameData */
    FrameData* fd = (FrameData*)(*(uint8_t**)((uint8_t*)resource + 0x20)
                                 + this->anim_index * sizeof(FrameData));

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

    RECT src_rect;
    SetRect(&src_rect, src_left, src_top, src_right, src_bottom);

    if (enable_scroll == 1) {
        flags |= 0x40;
    }

    /* Blit via UIPANEL */
    UIPANEL_Blit(*(void**)((uint8_t*)resource + 0x10),
                 clipped.left, clipped.top, clipped.right, clipped.bottom,
                 g_primary_surface,
                 src_rect.left, src_rect.top, src_rect.right, src_rect.bottom,
                 flags);
}


/* ================================================================== */
/* GameObject::DrawConnected — Vtable slot [11]                        */
/* Address: 0x405FD0                                                   */
/*                                                                     */
/* Renders a connected/multi-tile sprite. Temporarily increments       */
/* the frame index, blits, then restores the frame. Used for sprite    */
/* overlays and connected tile rendering.                              */
/* ================================================================== */
void GameObject::DrawConnected(RECT clip_bounds, int enable_scroll, uint32_t extra_flags)
{
    /* Bail if not visible or not a connected sprite */
    if (this->visible != 1) {
        return;
    }

    void* resource = *(void**)((uint8_t*)this + 0x40);
    FrameData* fd = (FrameData*)(*(uint8_t**)((uint8_t*)resource + 0x20)
                                 + this->anim_index * sizeof(FrameData));

    if (fd->is_connected == 0) {
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
    uint16_t cur_frame = *(uint16_t*)((uint8_t*)this + 0x54);
    this->SetFrame(cur_frame + 1, false);

    /* Second source rect for the connected tile */
    int src2_left  = (clipped.right - this->screen_rect.left) + this->source_rect.left;
    int src2_top   = clipped.bottom - this->screen_rect.top;
    int src2_right = (clipped.left - this->screen_rect.right) + this->source_rect.right;
    int src2_bottom= (clipped.top - this->screen_rect.bottom) + this->source_rect.bottom;

    SetRect(&src_rect, src2_left, src2_top, src2_right, src2_bottom);

    /* Blit connected tile */
    UIPANEL_Blit(*(void**)((uint8_t*)resource + 0x10),
                 clipped.right, clipped.bottom,
                 clipped.left, clipped.top,
                 g_primary_surface,
                 src2_right, src2_bottom, src_rect.left, src_rect.top,
                 flags);

    /* Restore original frame */
    this->SetFrame(cur_frame - 1, false);
}


/* ================================================================== */
/* GameObject::HitTest — Vtable slot [3]                               */
/* Address: 0x405680                                                   */
/*                                                                     */
/* Hit-tests a packed (X|Y) point against 8 UISprite sub-rectangle     */
/* pointers at +0x148..+0x164. Dispatches vtable[3] with animation     */
/* data from +0x68/+0x6C (hit case) or +0x60/+0x64 (miss case).       */
/* Used by LOCOBITMAP for click-region dispatch.                       */
/* ================================================================== */
int GameObject::HitTest(uint32_t packedXY)
{
    /* Check "destroyed" flag at +0x110 (LOCOBITMAP-specific, but checked here) */
    if (*(uint8_t*)((uint8_t*)this + 0x110) != 0) {
        return 0;
    }

    /* If no held child object at +0x130, test the 8 sub-rectangles */
    if (*(void**)((uint8_t*)this + 0x130) == nullptr) {
        /* Array of 8 UISprite pointers at +0x148 through +0x164 */
        for (int i = 0; i < 8; i++) {
            UISprite** sprite_ptr = (UISprite**)((uint8_t*)this + 0x148 + i * 4);
            if (*sprite_ptr == nullptr) continue;

            /* PtInRect on sprite's RECT (at sprite+4) */
            RECT* spr_rect = (RECT*)((uint8_t*)(*sprite_ptr) + 4);
            if (PtInRect(spr_rect, packedXY)) {
                /* Hit: dispatch vtable[3] with hit coords */
                int hit_x = *(int*)((uint8_t*)this + 0x68);
                int hit_y = *(int*)((uint8_t*)this + 0x6C);
                ((void(*)(void*,int,int,int,int))this->vtable[3])(this, hit_x, hit_y, 0, 1);
                return 0;
            }
        }
    }

    /* Miss: dispatch vtable[3] with miss coords */
    int miss_x = *(int*)((uint8_t*)this + 0x60);
    int miss_y = *(int*)((uint8_t*)this + 0x64);
    ((void(*)(void*,int,int,int,int))this->vtable[3])(this, miss_x, miss_y, 0, 1);
    return 0;
}


/* ================================================================== */
/* GameObject::OnTimerTick — Vtable slot [12]                          */
/* Address: 0x4055E0                                                   */
/*                                                                     */
/* Timer/event completion handler. Destroys the child callback object  */
/* at +0x130 via its scalar dtor (vtable[0]), then calls vtable[3]    */
/* to update the display state.                                        */
/* ================================================================== */
int GameObject::OnTimerTick()
{
    void* child = *(void**)((uint8_t*)this + 0x130);
    if (child != nullptr) {
        /* Call child's scalar deleting destructor with flags=1 (free) */
        void** child_vtbl = *(void***)child;
        ((void(*)(void*,byte))child_vtbl[0])(child, 1);

        *(void**)((uint8_t*)this + 0x130) = nullptr;

        /* Dispatch vtable[3] to update display */
        int disp_x = *(int*)((uint8_t*)this + 0x60);
        int disp_y = *(int*)((uint8_t*)this + 0x64);
        ((void(*)(void*,int,int,int,int))this->vtable[3])(this, disp_x, disp_y, 0, 1);
    }
    return 0;
}


/* ================================================================== */
/* GameObject::AnimStateSelect — Vtable slot [14]                      */
/* Address: 0x405AB0 (default impl = PlayAnimation)                    */
/*                                                                     */
/* Called by:                                                          */
/*   GameObject::Update — when animation hits boundary                */
/*                                                                     */
/* Default implementation plays the sound associated with the given    */
/* index. Derived classes (Building, etc.) override this to implement  */
/* animation state transitions based on game logic.                    */
/* ================================================================== */
void GameObject::AnimStateSelect(int sound_id)
{
    /* Default: just play the sound */
    this->PlayAnimation(sound_id);
}


/* ================================================================== */
/* GameObject::MoveTo — Internal position helper                       */
/*                                                                     */
/* Sets the raw world position fields (+0x74, +0x78) before resource   */
/* offset is applied by SetWorldPos.                                   */
/* ================================================================== */
void GameObject::MoveTo(int x, int y)
{
    *(int32_t*)((uint8_t*)this + 0x74) = x;
    *(int32_t*)((uint8_t*)this + 0x78) = y;
}
