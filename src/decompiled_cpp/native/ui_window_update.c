/**
 * ui_window_update.c — UI_Window_UpdateScroll
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * UI_Window_UpdateScroll is the per-frame scroll/animation tick for
 * UI window objects. It operates on a GameObject-derived struct with:
 *
 * Fields used (offsets from this in bytes):
 *   +0x00: vtable
 *   +0x08: X position / target X
 *   +0x0C: Y position / target Y
 *   +0x10: width / target width
 *   +0x14: height / count
 *   +0x28: current frame / tick counter
 *   +0x34: (audio channel pointer?)
 *   +0x40: parent resource handle
 *   +0x48: audio resource ID
 *   +0x4C: scroll offset
 *   +0x54: animation frame
 *   +0x58: game time check
 *   +0x88: mode byte ('D'=down, 'U'=up, 'P'=panel show, 'S'=panel hide, 0=playback)
 *   +0x8A: select index (short)
 *   +0x8C: scroll_target
 *   +0x94: speed (byte)
 *   +0x98: linked next pointer (tooltip child)
 *   +0x9C: linked x offset
 *   +0xA0: linked y offset
 *
 * Returns:
 *   0 = animation still in progress
 *   1 = animation completed (caller should remove from processing list)
 */

#include <stdint.h>

/* External declarations */
extern int   g_world_width;          /* 0x4AAD0C — screen/world width */
extern int   g_world_height;         /* 0x4AAD10 — screen/world height */
extern uint32_t g_game_time;         /* 0x4A99B4 — global tick counter */
extern char  __thiscall CGWND_AudioChannel_IsActive(void* audio); /* 0x40EEB0 */

/* Forward declaration for extern */
extern char __fastcall UI_Window_UpdateScroll(void* obj);

/* ================================================================== */
/* UI_Window_UpdateScroll — Per-frame scroll/animation tick            */
/* Address: 0x423560                                                   */
/*                                                                     */
/* Dispatches on mode byte at +0x88:                                   */
/*   'D' (0x44): Down bounce scroll                                   */
/*     If current offset == 0: draw immediate                          */
/*     If abs(offset) odd + world check: scroll down                   */
/*     If abs(offset) even + count check: scroll up                    */
/*     Else: mark complete (return 1)                                  */
/*   'U' (0x55): Up bounce scroll (mirror of 'D')                     */
/*   'P' (0x50): Panel slide-in ('Present')                           */
/*     Slides panel from current offset toward target                  */
/*   'S' (0x53): Panel slide-out ('Slide')                             */
/*     Slides panel away from target                                   */
/*   Default: Frame playback                                           */
/*     Advances animation frame. Checks frame range, audio state,      */
/*     and game time before progressing.                               */
/*                                                                     */
/* After main dispatch: if animation not complete and linked child     */
/* object exists at +0x98, calls child->vtable[3] with linked offsets. */
/* ================================================================== */
char __fastcall UI_Window_UpdateScroll(void* obj)
{
    int* objInt = (int*)obj;
    char* objByte = (char*)obj;
    char completed = 0;    /* local variable at ESP+0x0B */

    switch (objByte[0x88]) {
    case 'D':  /* 0x44 — Down bounce scroll */
    case 'U':  /* 0x55 — Up bounce scroll */
    {
        int offset = objInt[10];   /* +0x28 = current offset/tick */

        if (offset == 0) {
            /* Offset zero — draw immediate, don't animate */
            goto draw_and_complete;
        }

        /* Check parity of absolute offset */
        int abs_offset = (offset < 0) ? -offset : offset;
        int parity = abs_offset & 1;   /* odd = animate, even = settled? */

        if (parity == 0) {
            /* Even offset — check if we should scroll */
            if (offset == 0) {
                goto draw_and_complete;
            }

            /* If Y position < world_height, scroll down (increase Y) */
            if (objInt[3] < g_world_height) {   /* +0x0C = Y */
                /* Call vtable[3] = position set (x, y + speed) */
                void** vtab = (void**)objInt[0];
                typedef void (__thiscall* SetPosFunc)(void* self, int x, int y);
                SetPosFunc setPos = (SetPosFunc)(vtab[3]);
                setPos(obj, objInt[2], (unsigned char)objByte[0x94] + objInt[3]);
                /* vtable[10] = 0x28/4 = redraw/render */
                typedef void (__thiscall* RenderFunc)(void* self);
                RenderFunc render = (RenderFunc)(vtab[0x28 / 4]);
                render(obj);
                goto done;
            }
            /* Fall through to complete (window at edge) */
        } else {
            /* Odd offset — scroll up if count > 0 */
            if (objInt[5] > 0) {   /* +0x14 = count/height? */
                void** vtab = (void**)objInt[0];
                typedef void (__thiscall* SetPosFunc)(void* self, int x, int y);
                SetPosFunc setPos = (SetPosFunc)(vtab[3]);
                setPos(obj, objInt[2], objInt[3] - (unsigned char)objByte[0x94]);
                typedef void (__thiscall* RenderFunc)(void* self);
                RenderFunc render = (RenderFunc)(vtab[0x28 / 4]);
                render(obj);
                goto done;
            }
        }

        /* Animation complete */
        completed = 1;
        /* Fall through to vtable[10] draw */
        /* (no break — intentional fall-through to draw_and_complete) */
        goto draw_and_complete;
    }

    case 'P':  /* 0x50 — Panel slide-in */
    {
        int* ptr_0x8C = (int*)((char*)obj + 0x8C);  /* scroll_target */
        if (*ptr_0x8C != 0 && objInt[4] < *ptr_0x8C) {  /* +0x10 vs target */
            short sel_idx = *(short*)((char*)obj + 0x8A);  /* +0x8A */
            unsigned short list_limit = *(unsigned short*)(objInt[0x10] + 0x1A) - 2;
                                          /* parent+0x1A */
            if (sel_idx < (int)list_limit) {
                int cur_idx = objInt[10];          /* +0x28 */
                if (cur_idx == sel_idx) {
                    /* Reached target — advance select index */
                    void** vtab = (void**)objInt[0];
                    typedef void (__thiscall* SelectFunc)(void* self, int idx);
                    SelectFunc sel = (SelectFunc)(vtab[0x1C / 4]);  /* slot 7 */
                    sel(obj, sel_idx + 1);
                    typedef void (__thiscall* RenderFunc)(void* self);
                    RenderFunc render = (RenderFunc)(vtab[0x28 / 4]);
                    render(obj);
                    goto done;
                }

                /* Check if current frame is the final frame of an animation set */
                int* parent_ptr = objInt[0x10];          /* parent resource */
                int* anim_table = *(int**)(parent_ptr + 0x20 / 4);
                                                          /* parent->anim_table */
                short* frameEntry = (short*)((char*)anim_table + cur_idx * 0x18);
                if (frameEntry[0x0C / 2] != -1) {         /* sound_fx_index */
                    goto draw_and_complete;
                }

                /* Check frame end position */
                unsigned short frame_pos = *(unsigned short*)((char*)anim_table + cur_idx * 0x18 + 2);
                if (objInt[0x15] == frame_pos) {          /* +0x54 */
                    sel_idx = (short)*(short*)((char*)obj + 0x8A);
                    void** vtab = (void**)objInt[0];
                    typedef void (__thiscall* SelectFunc)(void* self, int idx);
                    SelectFunc sel = (SelectFunc)(vtab[0x1C / 4]);
                    sel(obj, sel_idx + 2);
                }
            }

            /* Set mode to 'S' (slide-out) and clear target */
            objByte[0x88] = 'S';
            *ptr_0x8C = 0;
            /* Redraw */
            {
                void** vtab = (void**)objInt[0];
                typedef void (__thiscall* RenderFunc)(void* self);
                RenderFunc render = (RenderFunc)(vtab[0x28 / 4]);
                render(obj);
            }
            goto done;
        }

        /* Slide in progress — move X left by speed */
        int neg_limit = -(int)*(unsigned short*)(objInt[0x10] + 0x14);
                                       /* parent->frame_width */
        if (objInt[0x13] > neg_limit) {   /* +0x4C = scroll offset */
            void** vtab = (void**)objInt[0];
            typedef void (__thiscall* SetPosFunc)(void* self, int x, int y);
            SetPosFunc setPos = (SetPosFunc)(vtab[3]);
            setPos(obj, objInt[2] - (unsigned char)objByte[0x94], objInt[3]);
            /* vtable[10] = render */
            typedef void (__thiscall* RenderFunc)(void* self);
            RenderFunc render = (RenderFunc)(vtab[0x28 / 4]);
            render(obj);
        }
        goto done;
    }

    case 'S':  /* 0x53 — Panel slide-out */
    {
        int* ptr_0x8C = (int*)((char*)obj + 0x8C);
        if (*ptr_0x8C != 0 && *ptr_0x8C < objInt[2]) {  /* target < X */
            short sel_idx = *(short*)((char*)obj + 0x8A);
            if (sel_idx > 1) {
                int cur_idx = objInt[10];          /* +0x28 */
                if (cur_idx == sel_idx) {
                    /* Move selection back */
                    void** vtab = (void**)objInt[0];
                    typedef void (__thiscall* SelectFunc)(void* self, int idx);
                    SelectFunc sel = (SelectFunc)(vtab[0x1C / 4]);
                    sel(obj, sel_idx - 1);
                    typedef void (__thiscall* RenderFunc)(void* self);
                    RenderFunc render = (RenderFunc)(vtab[0x28 / 4]);
                    render(obj);
                    goto done;
                }

                /* Check frame end */
                int* parent_ptr = objInt[0x10];
                int* anim_table = *(int**)((char*)parent_ptr + 0x20);
                short* frameEntry = (short*)((char*)anim_table + cur_idx * 0x18);
                if (frameEntry[0x0C / 2] != -1) {
                    goto draw_and_complete;
                }
                unsigned short frame_pos = *(unsigned short*)((char*)anim_table + cur_idx * 0x18 + 2);
                if (objInt[0x15] == frame_pos) {
                    sel_idx = (short)*(short*)((char*)obj + 0x8A);
                    void** vtab = (void**)objInt[0];
                    typedef void (__thiscall* SelectFunc)(void* self, int idx);
                    SelectFunc sel = (SelectFunc)(vtab[0x1C / 4]);
                    sel(obj, sel_idx - 2);
                }
            }

            /* Switch to 'P' slide-in */
            objByte[0x88] = 'P';
            *ptr_0x8C = 0;
            {
                void** vtab = (void**)objInt[0];
                typedef void (__thiscall* RenderFunc)(void* self);
                RenderFunc render = (RenderFunc)(vtab[0x28 / 4]);
                render(obj);
            }
            goto done;
        }

        /* Slide out progress — move X right by speed */
        if (objInt[0x13] < g_world_width) {   /* +0x4C < world_width */
            void** vtab = (void**)objInt[0];
            typedef void (__thiscall* SetPosFunc)(void* self, int x, int y);
            SetPosFunc setPos = (SetPosFunc)(vtab[3]);
            setPos(obj, objInt[2] + (unsigned char)objByte[0x94], objInt[3]);
            goto draw_and_complete;
        }
        goto done;
    }

    default:
    {
        /* Default: frame playback */
        unsigned short cur_frame = (unsigned short)objInt[0x15];  /* +0x54 */
        int* parent_ptr = objInt[0x10];      /* parent resource */
        int* anim_table = *(int**)((char*)parent_ptr + 0x20);
                                              /* parent->anim_table */
        int anim_idx = objInt[10];           /* +0x28, current animation index */
        unsigned short frame_limit = *(unsigned short*)((char*)anim_table + anim_idx * 0x18 + 2);
                                              /* end_frame */

        if (cur_frame >= frame_limit) {
            /* Past end of current animation — check if frame is playable */
            goto draw_and_complete;
        }

        short sound_idx = *(short*)((char*)anim_table + anim_idx * 0x18 + 0x0C);
        if (sound_idx != -1) {
            /* Frame has a sound bound to it */
            goto draw_and_complete;
        }

        if (g_game_time <= objInt[0x16]) {   /* +0x58 = next_step_time */
            goto draw_and_complete;
        }

        /* Check audio channel */
        void* audio = (void*)objInt[0x12];   /* +0x48 */
        if (audio != NULL) {
            char active = CGWND_AudioChannel_IsActive(audio);
            if (active == 0) {
                goto draw_and_complete;
            }
        }

        /* Advance frame — fall through to draw_and_complete */
        break;
    }
    }

    /* ============================================================== */
    /* draw_and_complete — mark as complete, call vtable[10] render    */
    /* ============================================================== */
draw_and_complete:
    completed = 1;
    {
        void** vtab = (void**)objInt[0];
        typedef void (__thiscall* RenderFunc)(void* self);
        RenderFunc render = (RenderFunc)(vtab[0x28 / 4]);  /* slot 10 */
        render(obj);
    }

done:
    /* If animation not complete and linked child exists, update child pos */
    if (completed == 0) {
        int* child = (int*)((char*)obj + 0x98);   /* +0x98 = linked tooltip child */
        if (child != NULL) {
            void** childVtab = (void**)*child;
            /* Call child->vtable[3] with (parent X + child_x_off, parent Y + child_y_off) */
            typedef void (__thiscall* SetPosFunc)(void* self, int x, int y);
            SetPosFunc setPos = (SetPosFunc)(childVtab[3]);
            setPos(child, objInt[2] + objInt[0x27],   /* X + 0x9C */
                           objInt[3] + objInt[0x28]);  /* Y + 0xA0 */
        }
    }

    return completed;
}
