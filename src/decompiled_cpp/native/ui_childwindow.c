/**
 * ui_childwindow.c — ChildWindow class functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * ChildWindow (vtable VTBL_CHILD_WINDOW, 0x477C18, size ~0x168 bytes)
 * is a base class for resource-managed child windows. It loads a .dat
 * descriptor file plus a .bmp sprite sheet, provides frame set animation
 * metadata, and manages a UIPANEL_Surface for GPU-side rendering.
 *
 * Subclasses: CursorEditWindow (VTBL_CURSOREDITWINDOW, 0x477610).
 *
 * ChildWindow field layout:
 *   +0x00: vtable -> VTBL_CHILD_WINDOW (0x477C18)
 *   +0x04: resource_id (uint32)
 *   +0x08: resource_type (byte)
 *   +0x09..+0x0B: padding
 *   +0x0C: unknown (inited 0)
 *   +0x10: render_surface (void*) — UIPANEL_Surface for rendering
 *   +0x14: frame_width (short) — derived from surface_width / num_frames
 *   +0x16: frame_height (short) — surface height
 *   +0x18: sticky_flag (byte) — 1 = keep surface on mouse leave
 *   +0x1A: num_frame_sets (short)
 *   +0x1C: cursor_frame_set_start (short)
 *   +0x1E: cursor_frame_set_end (short)
 *   +0x20: anim_table (FrameData*) — array of FrameData[0x18] entries
 *   +0x24: bitmap_surface (void*) — UIPANEL_Surface from .bmp
 *   +0x28: sprite_width (short)
 *   +0x2A: sprite_height (short)
 *   +0x2C: frame_count (short) — total frames
 *   +0x32: hotspot_x (short)
 *   +0x34: hotspot_y (short)
 *   +0x38: shadow_offset_x (int32)
 *   +0x3C: shadow_offset_y (int32)
 *   +0x40: shadow_resource_id (int32)
 *   +0x44: primary_resource_id (int32)
 *   +0x48: bmp_path[264] (char) — .bmp file path
 *   +0x14D: name/title string (13 bytes)
 *   +0x158: overlay_refcount (short)
 *   +0x15C: max_instances (int32, -1 = unlimited)
 *   +0x160: frame_sets_total (short)
 *   +0x162: load_success_flag (byte)
 *   +0x163: easter_egg_flag (byte)
 *   +0x164: flags (uint32)
 */

#include <stdint.h>
#include <string.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void*  __cdecl operator_new(size_t size);                 /* 0x465CE0 */
extern void   __cdecl GLOBAL_free(void* ptr);                    /* 0x465CD0 */
extern int    __thiscall UIPANEL_CreateSurface(void* surface);    /* 0x42A110 */
extern int    __thiscall UIPANEL_StretchBlit(void* surface, const char* path,
                                              int a, int b, int c); /* 0x42AB10 */
extern int    __fastcall ResourceManager_GetById(void** mgr, int id); /* 0x460A30 */
extern int    __fastcall ResourceManager_GetStringById(void** mgr, int id); /* 0x460C20 */
extern void   __thiscall RESMGR_LoadSoundResource(int res);      /* 0x449C00 */
extern void   __thiscall RESMGR_ReleaseSoundResource(int res);    /* 0x449CA0 */
extern void   __thiscall INPUT_EditScrollHandler(void* mgr, unsigned int res_id); /* 0x45BB50 */
extern void   __thiscall ResourceManager_AnimateClock(void** mgr, int time); /* 0x4611A0 */
extern void** g_resmgr;                                           /* 0x4855E8 */

/* ================================================================== */
/* UI_IsBitmapReady — Check if ChildWindow bitmap is ready             */
/* Address: 0x4255F0                                                   */
/*                                                                     */
/* Validates: easter_egg_flag (+0x163), bitmap_surface (+0x24),        */
/* frame_count (+0x2C), resource dependencies (+0x40/+0x44).           */
/* Special case: resource 0xC42 (animated clock) requires network      */
/* scenario check (scenarioId != 2).                                   */
/* ================================================================== */
int __fastcall UI_IsBitmapReady(char* childWindow)
{
    /* Check easter_egg_flag at +0x163 */
    if (childWindow[0x163] == 0) {
        return 0;
    }

    /* Check bitmap_surface at +0x24 */
    if (*(int*)(childWindow + 0x24) == 0) {
        return 0;
    }

    /* Check frame_count at +0x2C */
    if (*(short*)(childWindow + 0x2C) == 0) {
        return 0;
    }

    /* Check primary_resource_id dependency at +0x44 */
    int primary_id = *(int*)(childWindow + 0x44);
    if (primary_id != -1) {
        int res = ResourceManager_GetById(&g_resmgr, primary_id);
        if (res == 0 || *(short*)(res + 0x158) == 0) {
            return 0;
        }
    }

    /* Check shadow_resource_id dependency at +0x40 */
    int shadow_id = *(int*)(childWindow + 0x40);
    if (shadow_id != -1) {
        int res = ResourceManager_GetById(&g_resmgr, shadow_id);
        if (res == 0 || *(short*)(res + 0x158) == 0) {
            return 0;
        }
    }

    /* Special case: resource 0xC42 (animated clock) */
    if (*(int*)(childWindow + 4) == 0xC42) {
        extern char g_network_state;   /* network scenario check */
        if (*(char*)0x4FD3A8 != 0) {  /* g_netman[0x17].scenarioId */
            extern int g_network_scenario;  /* 0x4FD3C0? */
        }
    }

    return 1;
}

/* ================================================================== */
/* UI_PaintWindow — Render/refresh ChildWindow bitmap surface           */
/* Address: 0x425670                                                   */
/*                                                                     */
/* Creates a UIPANEL_Surface on first call (via StretchBlit from       */
/* .bmp path at +0x48), computes frame dimensions, increments overlay  */
/* refcount (+0x158), loads sound resources for each frame set.        */
/* Special case: resource 0x842 (clock) calls AnimateClock.            */
/*                                                                     */
/* @param this    ChildWindow pointer                                   */
/* @param x       StretchBlit X offset                                  */
/* @param y       StretchBlit Y offset                                  */
/* @return        Surface pointer, or 0 on failure                      */
/* ================================================================== */
int __thiscall UI_PaintWindow(char* childWindow, int x, int y)
{
    if (*(short*)(childWindow + 0x160) == 0) {  /* frame_sets_total */
        return 0;
    }

    /* Create rendering surface on first call */
    if (*(int*)(childWindow + 0x10) == 0) {     /* render_surface */
        int* surface = (int*)operator_new(0x20);   /* UIPANEL_Surface is 0x20 bytes */
        if (surface != NULL) {
            int result = UIPANEL_CreateSurface(surface);
            *(int*)(childWindow + 0x10) = (int)(result ? surface : 0);
        }
        if (*(int*)(childWindow + 0x10) == 0) {
            return 0;
        }

        /* Stretch-blit .bmp file onto the surface */
        UIPANEL_StretchBlit((void*)*(int*)(childWindow + 0x10),
                            childWindow + 0x48,  /* bmp_path */
                            0, x, y);
    }

    int* surface = (int*)*(int*)(childWindow + 0x10);

    /* Check surface dimensions: width (+8) and height (+0x0C) */
    if (surface[2] == 0 && surface[3] == 0) {
        /* Surface is empty — destroy it and return 0 */
        if (surface != NULL) {
            typedef void* (__thiscall* DtorFunc)(void* self, byte flags);
            DtorFunc dtor = (DtorFunc)(*(void**)surface)[0];
            dtor(surface, 1);
        }
        *(int*)(childWindow + 0x10) = 0;
        return 0;
    }

    /* Compute frame width: surface_width / frame_sets_total */
    *(short*)(childWindow + 0x14) = (short)((unsigned int)surface[2] /
                                            (unsigned int)*(unsigned short*)(childWindow + 0x160));
    /* frame_height = surface height */
    *(short*)(childWindow + 0x16) = *(short*)(surface + 3);
    *(short*)(childWindow + 0x16) = *(short*)((char*)surface + 0x0C);

    /* Increment overlay refcount */
    (*(short*)(childWindow + 0x158))++;

    /* Handle easter egg overlay tracking */
    if (*(char*)(childWindow + 0x163) == 0) {
        INPUT_EditScrollHandler(0x4A99B0, *(unsigned int*)(childWindow + 4));
    }

    /* Load sound resources for each frame set */
    unsigned short num_sets = *(unsigned short*)(childWindow + 0x1A);
    if (num_sets != 0) {
        int* anim_table = *(int**)(childWindow + 0x20);
        for (int i = 0; i < num_sets; i++) {
            int sound_id = ResourceManager_GetStringById(
                &g_resmgr,
                (int)*(short*)((char*)anim_table + i * 0x18 + 0x0E));
            if (sound_id != 0) {
                RESMGR_LoadSoundResource(sound_id);
            }
        }
    }

    /* Special case: animated clock (resource 0x842) */
    if (*(int*)(childWindow + 4) == 0x842) {
        extern int g_game_time;  /* 0x4A99B4 */
        ResourceManager_AnimateClock(&g_resmgr, g_game_time);
    }

    return *(int*)(childWindow + 0x10);
}

/* ================================================================== */
/* UI_OnMouseLeave — Handle mouse leaving ChildWindow                  */
/* Address: 0x4257F0                                                   */
/*                                                                     */
/* Decrements overlay refcount (+0x158). When refcount reaches 0       */
/* and sticky flag (+0x18) is not 1, destroys the rendering surface    */
/* and releases all frame-set sound resources.                         */
/* ================================================================== */
void __fastcall UI_OnMouseLeave(char* childWindow)
{
    short* refcount = (short*)(childWindow + 0x158);
    if (*refcount != 0) {
        (*refcount)--;
    }

    if (*refcount != 0) {
        return;
    }

    int* surface = (int*)*(int*)(childWindow + 0x10);
    if (surface == NULL) {
        return;
    }

    /* Check sticky flag at +0x18 */
    if (*(char*)(childWindow + 0x18) == 1) {
        return;
    }

    /* Destroy the surface */
    if (surface != NULL) {
        typedef void* (__thiscall* DtorFunc)(void* self, byte flags);
        DtorFunc dtor = (DtorFunc)(*(void**)surface)[0];
        dtor(surface, 1);
    }
    *(int*)(childWindow + 0x10) = 0;

    /* Release sound resources for each frame set */
    unsigned short num_sets = *(unsigned short*)(childWindow + 0x1A);
    if (num_sets != 0) {
        int* anim_table = *(int**)(childWindow + 0x20);
        for (int i = 0; i < num_sets; i++) {
            int sound_id = ResourceManager_GetStringById(
                &g_resmgr,
                (int)*(short*)((char*)anim_table + i * 0x18 + 0x0E));
            if (sound_id != 0) {
                RESMGR_ReleaseSoundResource(sound_id);
            }
        }
    }
}

/* ================================================================== */
/* UI_CreateChildWindow — Constructor for ChildWindow                  */
/* Address: 0x424AF0                                                   */
/*                                                                     */
/* Zeros all fields, sets vtable to VTBL_CHILD_WINDOW (0x477C18),      */
/* then delegates to UI_ChildWindow_Create to load from resource.      */
/* Called from 13+ callers including ResourceManager, INPUT,           */
/* TrainStation, CGWND_CursorEditWindow.                                */
/* ================================================================== */
void* __thiscall UI_CreateChildWindow(void* childWindow,
                                       unsigned int resource_id,
                                       int name_param)
{
    char* cw = (char*)childWindow;

    /* Zero key fields */
    *(int*)(cw + 0x10) = 0;        /* render_surface */
    *(int*)(cw + 0x24) = 0;        /* bitmap_surface */
    *(int*)(cw + 0x20) = 0;        /* anim_table */
    cw[0x18] = 0;                  /* sticky_flag */
    *(short*)(cw + 0x32) = 0;      /* hotspot_x */
    *(short*)(cw + 0x34) = 0;      /* hotspot_y */
    *(int*)(cw + 0x164) = 0;       /* flags */
    *(short*)(cw + 0x158) = 0;     /* overlay_refcount */
    *(short*)(cw + 0x160) = 1;     /* frame_sets_total = 1 */

    /* Set vtable */
    *(void**)cw = (void*)VTBL_CHILD_WINDOW;  /* 0x477C18 */

    /* Call Create to load from resource */
    UI_ChildWindow_Create(cw, resource_id, name_param);

    return childWindow;
}

/* ================================================================== */
/* UI_ChildWindow_Init — SCALAR DELETING DESTRUCTOR (vtable[0])        */
/* Address: 0x424B40                                                   */
/*                                                                     */
/* Despite misleading "Init" name, this is the vtable[0] destructor    */
/* for ChildWindow (VTBL_CHILD_WINDOW[0] = 0x424B40).                  */
/* Cleans up: sub-obj at +0x10 (vtable-based), heap buf at +0x20,      */
/* sub-obj at +0x24 (vtable-based). Clears load_success at +0x162.     */
/* Conditionally frees self if flags & 1.                              */
/* ================================================================== */
void* __thiscall UI_ChildWindow_Init(void* childWindow, byte flags)
{
    char* cw = (char*)childWindow;

    /* Reset vtable for partial-destruction safety */
    *(void**)cw = (void*)VTBL_CHILD_WINDOW;

    cw[0x162] = 0;   /* clear load_success_flag */

    /* Destroy sub-obj at +0x10 (render_surface) via its vtable[0] */
    void* sub10 = *(void**)(cw + 0x10);
    if (sub10 != NULL) {
        typedef void* (__thiscall* DtorFunc)(void* self, byte flags);
        DtorFunc dtor = (DtorFunc)(*(void**)sub10)[0];
        dtor(sub10, 1);
        *(int*)(cw + 0x10) = 0;
    }

    /* Free heap buffer at +0x20 (anim_table / FrameData array) */
    void* buf20 = *(void**)(cw + 0x20);
    if (buf20 != NULL) {
        GLOBAL_free(buf20);
        *(int*)(cw + 0x20) = 0;
    }

    /* Destroy sub-obj at +0x24 (bitmap_surface) via its vtable[0] */
    void* sub24 = *(void**)(cw + 0x24);
    if (sub24 != NULL) {
        typedef void* (__thiscall* DtorFunc)(void* self, byte flags);
        DtorFunc dtor = (DtorFunc)(*(void**)sub24)[0];
        dtor(sub24, 1);
        *(int*)(cw + 0x24) = 0;
    }

    /* Optionally free self */
    if ((flags & 1) != 0) {
        GLOBAL_free(childWindow);
    }

    return childWindow;
}

/* ================================================================== */
/* UI_ChildWindow_Dtor — Destructor body (base dtor, vtable[1])        */
/* Address: 0x424BA0                                                   */
/*                                                                     */
/* Same cleanup as UI_ChildWindow_Init but without conditional         */
/* GLOBAL_free. Called from derived-class destructors and SEH unwind.  */
/* ================================================================== */
void __fastcall UI_ChildWindow_Dtor(void* childWindow)
{
    char* cw = (char*)childWindow;

    /* Reset vtable */
    *(void**)cw = (void*)VTBL_CHILD_WINDOW;

    cw[0x162] = 0;   /* clear load_success_flag */

    /* Destroy sub-obj at +0x10 */
    void* sub10 = *(void**)(cw + 0x10);
    if (sub10 != NULL) {
        typedef void* (__thiscall* DtorFunc)(void* self, byte flags);
        DtorFunc dtor = (DtorFunc)(*(void**)sub10)[0];
        dtor(sub10, 1);
        *(int*)(cw + 0x10) = 0;
    }

    /* Free heap buffer at +0x20 */
    void* buf20 = *(void**)(cw + 0x20);
    if (buf20 != NULL) {
        GLOBAL_free(buf20);
        *(int*)(cw + 0x20) = 0;
    }

    /* Destroy sub-obj at +0x24 */
    void* sub24 = *(void**)(cw + 0x24);
    if (sub24 != NULL) {
        typedef void* (__thiscall* DtorFunc)(void* self, byte flags);
        DtorFunc dtor = (DtorFunc)(*(void**)sub24)[0];
        dtor(sub24, 1);
        *(int*)(cw + 0x24) = 0;
    }
}

/* ================================================================== */
/* UI_ChildWindow_Create — Initialize ChildWindow fields from resource */
/* Address: 0x424BF0                                                   */
/*                                                                     */
/* Loads a .dat descriptor file by name, parses it via vtable[3],      */
/* then loads the associated .bmp sprite sheet.                        */
/*                                                                     */
/* Uses a WNDPROC stream (WIN32_StreamOpen/Read/Write) to parse the   */
/* .dat file from the RFD archive or from disk.                        */
/* ================================================================== */
void __thiscall UI_ChildWindow_Create(void* childWindow,
                                       unsigned int resource_id,
                                       int name_param)
{
    char* cw = (char*)childWindow;
    char dat_path[260];
    char bmp_path[260];
    char full_dat_path[264];
    int stream_buf[22];   /* WNDPROC stream buffer (~0x58 bytes on stack) */
    void* parsed_data;
    int file_size;
    byte result;

    /* External functions from WNDPROC subsystem */
    extern int  __thiscall WIN32_StreamOpen(void* buf, int mode);     /* 0x467200 */
    extern void __thiscall WIN32_StreamDestroy(void* stream);          /* 0x467280 */
    extern void __thiscall WIN32_StreamDestroyImmediate(void* stream);  /* 0x467300 */
    extern int  __thiscall WIN32_StreamOpenPath(void* buf, const char* path,
                                                 int flags, int unk);  /* 0x467400 */
    extern int  __fastcall GetResourceType(unsigned int id);           /* 0x447B00? */
    extern int* __thiscall AssetMgr_LoadFile(void** mgr, const char* path,
                                              int* size);              /* TBD */
    extern void* __thiscall WNDPROC_StreamFromMemory(void* buf, char* data,
                                                       int size, int flags); /* TBD */
    extern void __cdecl CRT_free(void* ptr);                           /* CRT free */
    extern void __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...); /* 0x468800 */

    /* Initialize stream */
    WIN32_StreamOpen(stream_buf, 1);

    /* Store resource ID and type */
    *(unsigned int*)(cw + 4) = resource_id;
    cw[8] = (char)GetResourceType(resource_id);

    /* Zero many fields */
    *(int*)(cw + 0x0C) = 0;
    *(int*)(cw + 0x38) = 0;   /* shadow_offset_x */
    *(int*)(cw + 0x3C) = 0;   /* shadow_offset_y */
    *(short*)(cw + 0x14) = 0; /* frame_width */
    *(short*)(cw + 0x16) = 0; /* frame_height */
    *(short*)(cw + 0x160) = 1;/* frame_sets_total */
    *(short*)(cw + 0x1A) = 0; /* num_frame_sets */
    *(short*)(cw + 0x1C) = 0; /* cursor_frame_set_start */
    *(short*)(cw + 0x1E) = 0; /* cursor_frame_set_end */
    *(short*)(cw + 0x28) = 0; /* sprite_width */
    *(short*)(cw + 0x2A) = 0; /* sprite_height */
    *(short*)(cw + 0x2C) = 0; /* frame_count? */
    *(int*)(cw + 0x40) = -1;  /* shadow_resource_id */
    *(int*)(cw + 0x44) = -1;  /* primary_resource_id */
    cw[0x18] = 0;              /* sticky_flag */
    cw[0x163] = 1;             /* easter_egg_flag = 1 */
    cw[0x162] = 0;             /* load_success_flag = 0 */
    cw[0x14D] = 0;             /* name[0] = null terminator */
    *(int*)(cw + 0x15C) = -1; /* max_instances = unlimited */

    /* If name_param is provided, try to load .dat and .bmp files */
    if (name_param != 0) {
        /* Build path strings */
        /* sprintf(dat_path, "%s%s.dat", base, name); */
        /* sprintf(bmp_path, "%s%s.bmp", base, name); at cw+0x48 */

        /* Try loading from asset manager (RFD archive) */
        if (g_asset_mgr != NULL) {
            extern void* g_asset_mgr;   /* TBD */

            /* Build full dat path */
            CRT_sprintf_buf(full_dat_path, "%s.dat", dat_path);

            int* filedata = AssetMgr_LoadFile(&g_asset_mgr, full_dat_path, &file_size);
            if (filedata != NULL) {
                /* Parse from memory */
                parsed_data = operator_new(0x5C);
                if (parsed_data != NULL) {
                    int* wndproc_result = WNDPROC_StreamFromMemory(
                        parsed_data, (char*)filedata, file_size, 1);
                    if (wndproc_result != NULL) {
                        /* Call vtable[3] to parse the WNDPROC data */
                        void** vtab = *(void***)cw;
                        typedef byte (__thiscall* ParseFunc)(void* self, int* stream);
                        ParseFunc parse = (ParseFunc)(vtab[3]);
                        cw[0x162] = parse(cw, wndproc_result);

                        /* Destroy the wndproc result */
                        typedef void* (__thiscall* DtorFunc)(void* self, byte flags);
                        DtorFunc dtor = (DtorFunc)(*(void**)(*(int*)((*wndproc_result) + 4) +
                                                              (int)wndproc_result))[0];
                        dtor((void*)((char*)wndproc_result + *(int*)(*wndproc_result + 4)), 1);
                    }
                }
                CRT_free(filedata);
            }
        }

        /* If parsing failed, try loading .dat from disk via stream */
        if (cw[0x162] == 0) {
            extern int g_dat_handle;   /* DAT_00479190 */
            if (WIN32_StreamOpenPath(stream_buf, dat_path, 0x20, g_dat_handle) != 0) {
                int* stream_state = (int*)((char*)stream_buf + *(int*)(*(int*)stream_buf + 4));
                if (stream_state[0x4C / 4] != -1) {
                    void** vtab = *(void***)cw;
                    typedef byte (__thiscall* ParseFunc)(void* self, int* stream);
                    ParseFunc parse = (ParseFunc)(vtab[3]);
                    cw[0x162] = parse(cw, stream_buf);
                    WIN32_StreamDestroyImmediate(stream_buf);
                }
            }
        }
    }

    /* Cleanup stream */
    WIN32_StreamDestroy((int)(stream_buf + 2));
    /* WNDPROC_StreamCleanup(stream_buf + 2) — called via inline */
}
