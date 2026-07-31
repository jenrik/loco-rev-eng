/**
 * NetworkPlayerList.cpp — NetworkPlayerList class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the NetworkPlayerList class which manages the DPLAY-level
 * player list UI, surface cache, and PostBag registration methods.
 *
 * === Constructor (0x443000) ===
 * Creates all PostBag subdirectories (PostBag, Easter, Sort, Sort_In,
 * Sort_Out, Sort_Bag, AlbIndex, Album, Att_In, Att_Out) and zeros the
 * 256-entry surface cache.
 *
 * === Destructor Term (0x4431F0) ===
 * Frees resource_mgr sub-object, frees all 256 cached surfaces, calls
 * DPLAY_SendMessages for postbag cleanup, optionally frees self.
 *
 * === RenderPlayer (0x4437C0) ===
 * Full player list UI rendering: draws player name, session data, track
 * piece icons, divider lines, and blits a cached postcard image.
 * Due to Ghidra decompiler register confusion (unaff_EBP/this ambiguity
 * across 1855 bytes), some exact parameter mappings are approximated.
 */

#include "NetworkPlayerList.h"
#include "../graphics/LOCOBITMAP.h"
#ifndef _WIN32
#include <cstdio>
#endif
/* Dispatch-table addresses are documentation only; C++ manages dispatch. */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {

/* Win32 API */
extern int32_t __stdcall wsprintfA(char*, const char*, ...);
extern int32_t __stdcall CreateDirectoryA(const char*, void*);
extern int32_t __stdcall DeleteFileA(const char*);
extern void*   __stdcall SelectObject(void* hdc, void* hgdiobj);
extern int32_t __stdcall DrawTextA(void* hdc, const char*, int32_t,
                                    void* lprc, uint32_t format);
extern int32_t __stdcall CopyRect(void* lprcDst, const void* lprcSrc);
extern int32_t __stdcall OffsetRect(void* lprc, int32_t dx, int32_t dy);
extern int32_t __stdcall InflateRect(void* lprc, int32_t dx, int32_t dy);
extern int32_t __stdcall FillRect(void* hdc, const void* lprc, void* hbr);
extern int32_t __stdcall FrameRect(void* hdc, const void* lprc, void* hbr);
extern int32_t __stdcall MoveToEx(void* hdc, int32_t x, int32_t y, void* oldPt);
extern int32_t __stdcall LineTo(void* hdc, int32_t x, int32_t y);
extern void*   __stdcall CreateSolidBrush(uint32_t crColor);
extern void*   __stdcall CreatePen(int32_t style, int32_t width, uint32_t color);
extern int32_t __stdcall DeleteObject(void* hgdiobj);
extern void*   __stdcall GetStockObject(int32_t fnObject);
extern uint32_t __stdcall SetTextColor(void* hdc, uint32_t crColor);
extern int32_t __stdcall SetBkMode(void* hdc, int32_t mode);
extern int32_t __stdcall DrawEdge(void* hdc, void* qrc, uint32_t edge,
                                   uint32_t grfFlags);

} /* extern "C" */

/* C++ allocation helpers */
extern void* __cdecl operator_new(size_t size);
extern void  __cdecl GLOBAL_free(void* ptr);

/* Game globals */
extern void* g_resmgr;                  /* 0x4855E8 */
extern char  g_install_path[];          /* 0x4A99C8 */
extern void* g_player_config;           /* 0x4AA4A8 */

/* UI helpers */
extern void* __thiscall UIPANEL_BeginPaint(int32_t panel);
extern void  __thiscall UIPANEL_EndPaintEx(void* panel, void* param2,
                                            int32_t hdc, uint8_t repaint,
                                            void* updateRect);
extern void  __cdecl    UIPANEL_Blit(void* surface, int32_t srcX, int32_t srcY,
                                      int32_t srcW, int32_t srcH,
                                      void* dstSurface, int32_t dstX,
                                      int32_t dstY, int32_t dstW,
                                      int32_t dstH, int32_t flags);
extern void* __thiscall UIPANEL_CreateSurface(void* surface);
extern void* __thiscall UIPANEL_CopySurface(void* dst, int32_t src);
extern void  __cdecl    UIPANEL_StretchBlit(void* surface, const char* path,
                                              int32_t x, int32_t y, int32_t flags);
extern void* __thiscall ResourceManager_GetById(void* resmgr, uint32_t id);
extern void  __cdecl    FormatResourceString(void* resmgr, uint32_t id,
                                              char* buf, int32_t bufsize);
extern void  __cdecl    UI_CenterWindow(void* outer, void* inner);

/* DPlay helpers */
extern int32_t __thiscall DPLAY_SetPlayerData(void* slot, const char* path);
extern void*  __cdecl       NET_GetHostName(int32_t param_1, int32_t param_2);
extern uint32_t __cdecl     NET_ComputeColor(uint8_t param1, uint8_t param2,
                                             uint8_t param3);

/* DPLAY message cleanup */
extern void __cdecl DPLAY_SendMessages(void);  /* 0x443470 */

/* Font globals */
extern void* g_font_small;              /* 0x4855F8 */
extern void* g_font_normal;             /* 0x4855FC */

/* String constants */
extern char  g_empty_string;            /* 0x4851D0 */

/* ================================================================== */
/* PostBag subdirectory name table                                      */
/* ================================================================== */

static const char* PostBag_Subdir(int32_t type)
{
    extern int32_t DAT_004a97a0;
    switch (type) {
    case 0: return (const char*)0x0047eba4;
    case 1: return (const char*)0x0047ebc4;
    case 2: return (const char*)0x0047ebb8;
    case 3: return (const char*)0x0047ebac;
    case 4: return (const char*)0x0047eb90;
    case 5: return (const char*)0x0047eb9c;
    case 6:
        switch (DAT_004a97a0) {
        default: return (const char*)0x0047ebf8;
        case 1:  return (const char*)0x0047ec58;
        case 2:  return (const char*)0x0047ec4c;
        case 4:  return (const char*)0x0047ec40;
        case 5:  return (const char*)0x0047ec34;
        case 6:  return (const char*)0x0047ec28;
        case 7:  return (const char*)0x0047ec1c;
        case 8:  return (const char*)0x0047ec10;
        case 9:  return (const char*)0x0047ec04;
        }
    case 7: return (const char*)0x0047ed18;
    default: return (const char*)0x0047eba4;
    }
}

/* ================================================================== */
/* SeekTrackEntryLowestTimestamp — Find least-recently-used cache slot */
/* ================================================================== */
static int32_t find_lru_slot(NetworkPlayerList* list)
{
    uint32_t oldest = list->lru_timestamps[0];
    int32_t  lru_slot = 0;
    int32_t i;

    for (i = 1; i < 256; i++) {
        if (list->lru_timestamps[i] < oldest) {
            oldest = list->lru_timestamps[i];
            lru_slot = i;
        }
    }
    return lru_slot;
}

/* ================================================================== */
/* NetworkPlayerList::NetworkPlayerList — 0x443000                   */
/*                                                                      */
/* Constructor: creates PostBag directories, zeros cache, init state.  */
/* C++ constructor; the compiler establishes the dispatch pointer.     */
/*                                                                      */
/* Called by: GameLoop_Setup (0x406CBC)                                */
/* ================================================================== */
NetworkPlayerList::NetworkPlayerList()
{
    char path_buf[0x104];    /* local path buffer for directory creation */
    int32_t i;

    /* Dynamic dispatch is compiler-managed in reconstructed C++. */

    /* Initialize each cache record through its declared fields. */
    for (i = 0; i < 256; i++) {
        surface_cache[i] = NULL;
        lru_timestamps[i] = 0;
        tags[i].type_hi = 0;
        tags[i].variant = 0;
        tags[i].tag_low = 0;
    }

    /* Initialize remaining fields */
    this->frame_counter = 0;
    this->msg_count_cache = -1;         /* -1 = uncached sentinel */
    this->resource_mgr = NULL;
    this->resource_data = NULL;
    this->enumerated = 0;

    /* Create PostBag subdirectories */
#ifndef _WIN32
    /* Host addresses do not contain the original PE string literals. */
    static const char* const host_subdirectories[] = {
        "", "Easter", "Sort", "Sort_In", "Sort_Out", "Sort_Bag",
        "AlbIndex", "Album", "Att_In", "Att_Out"
    };
    for (const char* subdirectory : host_subdirectories) {
        std::snprintf(path_buf, sizeof(path_buf), "%s/PostBag/%s",
                      g_install_path, subdirectory);
        CreateDirectoryA(path_buf, NULL);
    }
    return;
#endif
    path_buf[0] = g_empty_string;
    {
        uint32_t* p = (uint32_t*)(path_buf + 1);
        for (i = 0x40; i != 0; i--) { *p = 0; p++; }
    }
    path_buf[0x101] = 0;
    path_buf[0x102] = 0;

    /* 1. <install>\PostBag\ */
    wsprintfA(path_buf,
              (const char*)0x0047e8a0,  /* "%s%s" */
              g_install_path,
              (const char*)0x0047e0c4); /* "\\PostBag\\" */
    CreateDirectoryA(path_buf, NULL);

    /* 2. <install>\PostBag\Easter (subdir at 0x47ebd8) */
    wsprintfA(path_buf,
              (const char*)0x0047e3d0,  /* "%s%s%s" */
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047ebd8); /* "Easter" */
    CreateDirectoryA(path_buf, NULL);

    /* 3. Sort */
    path_buf[0] = g_empty_string;
    {
        uint32_t* p = (uint32_t*)(path_buf + 1);
        for (i = 0x40; i != 0; i--) { *p = 0; p++; }
    }
    path_buf[0x101] = 0;
    path_buf[0x102] = 0;
    wsprintfA(path_buf,
              (const char*)0x0047e3d0,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047ebd0); /* "Sort" */
    CreateDirectoryA(path_buf, NULL);

    /* 4. Sort_In */
    wsprintfA(path_buf,
              (const char*)0x0047e3d0,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047ebc4); /* "Sort_In" */
    CreateDirectoryA(path_buf, NULL);

    /* 5. Sort_Out */
    wsprintfA(path_buf,
              (const char*)0x0047e3d0,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047ebb8); /* "Sort_Out" */
    CreateDirectoryA(path_buf, NULL);

    /* 6. Sort_Bag */
    wsprintfA(path_buf,
              (const char*)0x0047e3d0,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047ebac); /* "Sort_Bag" */
    CreateDirectoryA(path_buf, NULL);

    /* 7. AlbIndex */
    wsprintfA(path_buf,
              (const char*)0x0047e3d0,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047e0cc); /* "AlbIndex" */
    CreateDirectoryA(path_buf, NULL);

    /* 8. Album */
    wsprintfA(path_buf,
              (const char*)0x0047e3d0,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047eba4); /* "Album" */
    CreateDirectoryA(path_buf, NULL);

    /* 9. Att_In */
    wsprintfA(path_buf,
              (const char*)0x0047e3d0,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047eb9c); /* "Att_In" */
    CreateDirectoryA(path_buf, NULL);

    /* 10. Att_Out */
    wsprintfA(path_buf,
              (const char*)0x0047e3d0,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047eb90); /* "Att_Out" */
    CreateDirectoryA(path_buf, NULL);

}

/* ================================================================== */
/* NetworkPlayerList::~NetworkPlayerList — 0x4431F0                  */
/*                                                                      */
/* Destructor body for virtual slot [0], address 0x4431F0.             */
/* Frees resource_mgr and cached surfaces, then cleans PostBag state.   */
/* ================================================================== */
NetworkPlayerList::~NetworkPlayerList()
{
    int32_t i;

    /* Dynamic dispatch remains compiler-managed during cleanup. */

    /* Free sub-object at +0xB08 (resource_mgr) via vtable[2] */
    if (resource_mgr != NULL) {
        void** mgr_vtbl = *(void***)resource_mgr;
        ((void(__stdcall*)(void))mgr_vtbl[2])();  /* vtable[2] = release/shutdown */
        resource_mgr = NULL;
        resource_data = NULL;
    }

    /* Free all 256 cached surfaces */
    {
        for (i = 0; i < 256; i++) {
            if (surface_cache[i] != NULL) {
                UIPANEL_DestroySurface(surface_cache[i], 1);
                surface_cache[i] = NULL;
            }
        }
    }

    /* Clean up PostBag messages */
    DPLAY_SendMessages();

    /* Heap release is emitted by the compiler's deleting-destructor
     * wrapper, not by the user destructor body. */
}

/* ================================================================== */
/* GetOrCreateSurface — 0x4442B0                                        */
/*                                                                      */
/* Lookup or create a cached UIPANEL surface keyed by 3-byte tag.      */
/* ================================================================== */
void* NetworkPlayerList::GetOrCreateSurface(uint8_t type_hi,
                                                         uint8_t variant,
                                                         uint8_t tag_low,
                                                         uint8_t no_evict)
{
    char  filename[0x150];
    char  filepath[0x510];
    void* surface;
    int32_t i;

    /* Initialize buffers */
    filename[0] = *(char*)0x4851D0;
    {
        uint32_t* p = (uint32_t*)(filename + 1);
        for (i = 0x0F; i != 0; i--) { *p = 0; p++; }
    }
    filename[0x41] = 0;
    filename[0x42] = 0;

    filepath[0] = *(char*)0x4851D0;
    {
        uint32_t* p = (uint32_t*)(filepath + 1);
        for (i = 0x140; i != 0; i--) { *p = 0; p++; }
    }
    filepath[0x501] = 0;
    filepath[0x502] = 0;

    /* Search cache for matching entry */
    for (i = 0; i < 256; i++) {
        if (this->surface_cache[i] != NULL &&
            this->tags[i].type_hi  == type_hi &&
            this->tags[i].variant  == variant &&
            this->tags[i].tag_low  == tag_low)
        {
            /* Cache hit — update LRU timestamp and return copy */
            this->lru_timestamps[i] = this->frame_counter;

            void* new_surf = operator_new(0x20);
            if (new_surf != NULL) {
                return UIPANEL_CopySurface(new_surf, (int32_t)this->surface_cache[i]);
            }
            return NULL;
        }
    }

    /* Cache miss — construct filename */
    {
        uint8_t type_idx = type_hi;
        uint8_t v = variant - 1;

        if (type_idx <= 0x0F) {
            wsprintfA(filename, (const char*)0x0047ec9c,
                      (uint32_t)type_idx, v, tag_low);
        } else if (type_idx <= 0x19) {
            wsprintfA(filename, (const char*)0x0047ecb0,
                      type_idx + 0x58, v, tag_low);
        } else if (type_idx <= 0x1D) {
            wsprintfA(filename, (const char*)0x0047ecb0,
                      type_idx + 0x5A, v, tag_low);
        } else if (type_idx == 0x1E) {
            wsprintfA(filename, (const char*)0x0047ecc0, v, tag_low);
        } else if (type_idx == 0x1F) {
            wsprintfA(filename, (const char*)0x0047ecd0, tag_low);
        } else {
            wsprintfA(filename, (const char*)0x0047ec8c, v, tag_low);
        }
    }

    /* Build full path */
    wsprintfA(filepath, (const char*)0x0047ec7c,
              g_install_path, filename);

    /* Create surface from file */
    surface = operator_new(0x20);
    if (surface != NULL) {
        surface = UIPANEL_CreateSurface(surface);
    }
    UIPANEL_StretchBlit(surface, filepath, 0, 0, 0);

    /* Check if surface has valid dimensions */
    if (surface != NULL &&
        *(int32_t*)((int8_t*)surface + 0x18) == 0 &&
        *(int32_t*)((int8_t*)surface + 0x1C) == 0) {
        if (surface != NULL) {
            UIPANEL_DestroySurface((UIPANEL_Surface*)surface, 1);
        }
        return NULL;
    }

    /* Find empty cache slot */
    for (i = 0; i < 256; i++) {
        if (this->surface_cache[i] == NULL) {
            this->tags[i].type_hi = type_hi;
            this->tags[i].variant = variant;
            this->tags[i].tag_low = tag_low;
            this->lru_timestamps[i] = this->frame_counter;

            void* copy = operator_new(0x20);
            if (copy != NULL) {
                this->surface_cache[i] = UIPANEL_CopySurface(copy, (int32_t)surface);
            } else {
                this->surface_cache[i] = NULL;
            }
            return surface;
        }
    }

    /* Cache full — evict LRU if allowed */
    if (no_evict == 0) {
        int32_t lru = find_lru_slot(this);
        if (this->surface_cache[lru] != NULL) {
            UIPANEL_DestroySurface(this->surface_cache[lru], 1);
        }
        this->surface_cache[lru] = NULL;

        this->tags[lru].type_hi = type_hi;
        this->tags[lru].variant = variant;
        this->tags[lru].tag_low = tag_low;
        this->lru_timestamps[lru] = this->frame_counter;

        void* copy = operator_new(0x20);
        if (copy != NULL) {
            this->surface_cache[lru] = UIPANEL_CopySurface(copy, (int32_t)surface);
        } else {
            this->surface_cache[lru] = NULL;
        }
    }

    return surface;
}

/* ================================================================== */
/* RenderTrackEntry — 0x4440A0                                          */
/*                                                                      */
/* Blit a cached track entry surface onto a HDC with computed source   */
/* and destination rectangles. Clips to target bounds.                 */
/* ================================================================== */
void NetworkPlayerList::RenderTrackEntry(void* hdc,
                                                      uint32_t clip_x,
                                                      uint32_t clip_y,
                                                      int32_t clip_right,
                                                      uint32_t clip_bot,
                                                      const uint8_t entry[6])
{
    void* surface;
    uint32_t surf_w, surf_h;
    uint32_t dst_left, dst_top;
    uint32_t dst_right, dst_bot;
    uint32_t src_x_off, src_y_off;
    uint32_t src_w, src_h;

    /* Get surface for this track entry's packed type */
    surface = this->GetOrCreateSurface(
        entry[0] >> 3,
        (entry[0] & 7) + 1,
        entry[1],
        0);

    if (surface == NULL) return;

    surf_w = *(uint32_t*)((int8_t*)surface + 0x18);
    surf_h = *(uint32_t*)((int8_t*)surface + 0x1C);

    /* Compute destination rect from entry position */
    dst_left  = ((uint32_t)entry[2] * 2 - (surf_w >> 1)) + clip_x;
    dst_top   = ((uint32_t)entry[4] * 2 - (surf_h >> 1)) + clip_y;
    dst_right = dst_left + surf_w;
    dst_bot   = dst_top  + surf_h;

    /* Clip to target bounds */
    src_x_off = 0;
    src_y_off = 0;
    src_w = surf_w;
    src_h = surf_h;

    if ((int32_t)dst_left < (int32_t)clip_x) {
        src_x_off = clip_x - dst_left;
        dst_left = clip_x;
    }
    if ((int32_t)dst_top < (int32_t)clip_y) {
        src_y_off = clip_y - dst_top;
        dst_top = clip_y;
    }
    if (clip_right < (int32_t)dst_right) {
        src_w = surf_w + (clip_right - (int32_t)dst_right);
        dst_right = clip_right;
    }
    if ((int32_t)clip_bot < (int32_t)dst_bot) {
        src_h = surf_h + (clip_bot - dst_bot);
        dst_bot = clip_bot;
    }

    UIPANEL_Blit(surface, dst_left, dst_top, dst_right, dst_bot,
                 hdc, src_x_off, src_y_off, src_w, src_h, 0);

    /* Release surface copy */
    UIPANEL_DestroySurface((UIPANEL_Surface*)surface, 1);
}

/* ================================================================== */
/* PeekMessage — 0x4436C0                                               */
/*                                                                      */
/* MISNAMED — renders track entry and manages resource cache.           */
/* ================================================================== */
void NetworkPlayerList::PeekMessage(void* player_slot, void* hdc,
                                                 uint32_t param3, uint32_t param4,
                                                 int32_t param5, uint32_t param6,
                                                 uint32_t param7)
{
    void** vtbl;
    int32_t i;

    /* Lazily cache resource */
    if (this->resource_data == NULL) {
        this->resource_mgr = ResourceManager_GetById(&g_resmgr, 0x3CBD);
        if (this->resource_mgr != NULL) {
            vtbl = *(void***)this->resource_mgr;
            this->resource_data = ((void*(__stdcall*)(int32_t,int32_t))vtbl[1])(0, 0);
        }
    }

    /* Increment frame counter */
    this->frame_counter++;
    if (this->frame_counter > 0xEFFFFFFF) {
        this->frame_counter = 1;
        for (i = 0; i < 256; i++) {
            this->lru_timestamps[i] = 1;
        }
    }

    /* Render last non-empty track entry */
    {
        int32_t entry_idx;
        const uint8_t* entry_ptr;

        entry_ptr = (const uint8_t*)player_slot + 0x391;
        for (entry_idx = 0x7F; entry_idx >= 0; entry_idx--) {
            if (*entry_ptr != 0) break;
            entry_ptr -= 6;
        }

        if (entry_idx >= 0) {
            this->RenderTrackEntry(hdc, param3, param4, param5, param6,
                                   entry_ptr - 6 * (int32_t)(uintptr_t)(*entry_ptr - *entry_ptr));
        }
    }

    /* End paint cycle */
    if (this->resource_mgr != NULL) {
        vtbl = *(void***)this->resource_mgr;
        ((void(__stdcall*)(void*, void*))vtbl[0x44/4])(this->resource_mgr, &param7);
    }

    {
        void* hbr = GetStockObject(4);  /* BLACK_BRUSH */
        FrameRect(hdc, (void*)(uintptr_t)&param3, hbr);
    }

    if (this->resource_mgr != NULL) {
        vtbl = *(void***)this->resource_mgr;
        ((void(__stdcall*)(void*, void*))vtbl[0x68/4])(this->resource_mgr, hdc);
    }
}

/* ================================================================== */
/* RenderSessionFrame — 0x443F00                                         */
/* ================================================================== */
void NetworkPlayerList::RenderSessionFrame(void* hdc)
{
    void* surface;
    int32_t i;

    /* Increment frame counter and handle overflow */
    this->frame_counter++;
    if (this->frame_counter > 0xEFFFFFFF) {
        this->frame_counter = 1;
        for (i = 0; i < 256; i++) {
            this->lru_timestamps[i] = 1;
        }
    }

    /* Get session surface (tag 0x1E) */
    surface = this->GetOrCreateSurface(0x1E, 0, 0, 0);
    if (surface != NULL) {
        UIPANEL_DestroySurface((UIPANEL_Surface*)surface, 1);
    }
}

/* ================================================================== */
/* RenderSessionBase — 0x443FF0                                          */
/* ================================================================== */
void NetworkPlayerList::RenderSessionBase(void* hdc,
                                                       uint32_t param2,
                                                       int32_t param3,
                                                       int32_t param4,
                                                       uint32_t param5,
                                                       uint8_t param6)
{
    void* surface;
    int32_t i;

    /* Increment frame counter */
    this->frame_counter++;
    if (this->frame_counter > 0xEFFFFFFF) {
        this->frame_counter = 1;
        for (i = 0; i < 256; i++) {
            this->lru_timestamps[i] = 1;
        }
    }

    /* Get session base surface (tag 0x1F) */
    surface = this->GetOrCreateSurface(0x1F, 1, param6, 0);
    if (surface != NULL) {
        int32_t surf_w = *(int32_t*)((int8_t*)surface + 0x18);
        int32_t surf_h = *(int32_t*)((int8_t*)surface + 0x1C);

        UIPANEL_Blit(surface,
                     (param4 - 1) - surf_w, param3 + 1U,
                     param4 - 1, surf_h + param3 + 1U,
                     hdc, 0, 0, surf_w, surf_h, 0);
        UIPANEL_DestroySurface((UIPANEL_Surface*)surface, 1);
    }
}

/* ================================================================== */
/* RenderPlayer — 0x4437C0                                              */
/*                                                                      */
/* Render full player list UI entry with name, session data, track     */
/* piece icons, and postcard image.                                     */
/*                                                                      */
/* NOTE on param "playerData": In the Ghidra decompiler output,        */
/* this parameter is used both as a rendering context (vtable slots    */
/* 17 and 26) AND as a player-slot data pointer (fields at +0x39,      */
/* +0x40/0x41/0x42, +0x43 name, +0x10 session, +0x25, +0x3A,          */
/* +0x93, +0x96 track entries). This dual use suggests it is a         */
/* UIPANEL-like struct that embeds player slot data.                   */
/*                                                                      */
/* Due to Ghidra register confusion (unaff_EBP / unaff_retaddr          */
/* across 1855 bytes), some exact parameter details are approx.        */
/* ================================================================== */
void NetworkPlayerList::RenderPlayer(void* hdc, int32_t param2,
                                                  void* playerData, int32_t param4,
                                                  int32_t param5, uint32_t param6,
                                                  const void* param7)
{
    int32_t i;
    uint32_t color;
    void* hbr;
    char label_sent[16];
    char label_rcvd[16];
    void* hpen;
    void* old_pen;
    void* old_font;
    uint32_t old_text_color;
    int32_t old_bk_mode;
    int32_t mid_x, mid_y;
    RECT text_rect;
    void** ctx_vtbl;
    uint32_t frame_count;

    /* Init label buffers — "Sent" / "Received" resource strings */
    label_rcvd[0] = 0;
    *(uint32_t*)(label_rcvd + 4) = 0;
    *(uint32_t*)(label_rcvd + 8) = 0;
    label_sent[0] = 0;
    *(uint32_t*)(label_sent + 4) = 0;
    *(uint32_t*)(label_sent + 8) = 0;
    *(uint32_t*)(label_sent + 12) = 0;
    *(uint16_t*)(label_sent + 14) = 0;

    /* 1. Lazily cache resource from ResourceManager_GetById(0x3CBD) */
    if (this->resource_data == NULL) {
        this->resource_mgr = ResourceManager_GetById(&g_resmgr, 0x3CBD);
        if (this->resource_mgr != NULL) {
            ctx_vtbl = *(void***)this->resource_mgr;
            this->resource_data = ((void*(__stdcall*)(int32_t, int32_t))ctx_vtbl[1])(0, 0);
        }
    }

    /* 2. Format resource string labels */
    FormatResourceString(&g_resmgr, 100, label_sent, 0x10);
    FormatResourceString(&g_resmgr, 0x65, label_rcvd, 0x10);

    /* 3. Frame counter management with LRU timeout reset */
    frame_count = this->frame_counter + 1;
    this->frame_counter = frame_count;
    if (frame_count > 0xEFFFFFFF) {
        this->frame_counter = 1;
        for (i = 0; i < 256; i++) {
            this->lru_timestamps[i] = 1;
        }
    }

    /* 4. Begin paint cycle — call vtable slot 17 (0x44/4) on the
     *    rendering context (playerData). The context has a vtable
     *    where slot 17 is an BeginPaint-like method. */
    ctx_vtbl = *(void***)playerData;
    ((void(__stdcall*)(void*, void*))ctx_vtbl[0x44 / 4])(playerData, &hbr);

    /* 5. Draw background.
     *    If playerData+0x39 flag is set, compute fill color from
     *    fields +0x40/0x41/0x42 via NET_ComputeColor.
     *    Otherwise use WHITE_BRUSH and draw optional highlight rect. */
    if (*(int8_t*)((int8_t*)playerData + 0x39) != 0) {
        color = NET_ComputeColor(
            *(uint8_t*)((int8_t*)playerData + 0x40),
            *(uint8_t*)((int8_t*)playerData + 0x41),
            *(uint8_t*)((int8_t*)playerData + 0x42));
        hbr = CreateSolidBrush(color);
        FillRect(hdc, (const RECT*)&param2, hbr);
        DeleteObject(hbr);
    } else {
        hbr = GetStockObject(0);  /* WHITE_BRUSH */
        FillRect(hdc, (const RECT*)&param2, hbr);

        if (param7 != NULL) {
            hbr = CreateSolidBrush(0xE6E6E6);
            FillRect(hdc, (const RECT*)param7, hbr);
            DrawEdge(hdc, (void*)param7, 5, 0xF);
        }
    }

    /* 6. Create light gray brush for text area backgrounds */
    hbr = CreateSolidBrush(0xE6E6E6);

    /* 7. Player name text rectangle (left side panel) */
    text_rect.left   = param2 + 10;
    text_rect.bottom = param5 - 10;
    text_rect.top    = (int32_t)playerData + 2;
    text_rect.right  = (param4 - param2) / 2 - 0x14 + text_rect.left;

    if (param7 != NULL) {
        RECT edge_rect;
        CopyRect(&edge_rect, &text_rect);
        InflateRect(&edge_rect, 2, 2);
        FillRect(hdc, &edge_rect, hbr);
        DrawEdge(hdc, &edge_rect, 10, 0xF);
    }

    /* 8. Draw player name (+0x43) in orange text */
    {
        const char* name = (const char*)((int8_t*)playerData + 0x43);
        uint32_t name_len;
        for (name_len = 0; name[name_len] != '\0'; name_len++) { }

        if (name_len > 1) {
            old_text_color = SetTextColor(hdc, 0xFF5C00);
            old_bk_mode = SetBkMode(hdc, 1);          /* TRANSPARENT */
            old_font = SelectObject(hdc, g_font_small);
            DrawTextA(hdc, name, -1, &text_rect, 0x2810);
            SelectObject(hdc, old_font);
            SetTextColor(hdc, old_text_color);
            SetBkMode(hdc, old_bk_mode);
        }
    }

    /* 9. Vertical divider line (gray, width=2) */
    hpen = CreatePen(0, 2, 0x808080);
    old_pen = SelectObject(hdc, hpen);
    mid_x = (param4 - param2) / 2 + param2;
    MoveToEx(hdc, mid_x, (int32_t)playerData + 2, NULL);
    LineTo(hdc, mid_x, param5 - 10);

    /* 10. Session data section (right side of panel) */
    mid_y = (int32_t)playerData + (param5 - (int32_t)playerData) / 2;
    {
        int32_t label_x = mid_x + 10;

        if (param7 == NULL) {
            MoveToEx(hdc, label_x, mid_y, NULL);
            LineTo(hdc, param4 - 0x14, mid_y);
            text_rect.left = label_x;
        } else {
            text_rect.left = mid_x + 0x0E;
        }
        text_rect.top    = mid_y - 0x14;
        text_rect.right  = param4 - 0x14;
        text_rect.bottom = mid_y;

        SelectObject(hdc, g_font_normal);
        old_text_color = SetTextColor(hdc, 0xFF5C00);
        old_bk_mode = SetBkMode(hdc, 1);

        /* Draw "Sent" label (+0x10) */
        {
            const char* session_data = (const char*)((int8_t*)playerData + 0x10);
            uint32_t sd_len;
            for (sd_len = 0; session_data[sd_len] != '\0'; sd_len++) { }
            DrawTextA(hdc, session_data, -1, &text_rect, 0);
        }

        /* Second horizontal divider */
        {
            int32_t line2_y = mid_y - 0x1E;
            if (param7 == NULL) {
                MoveToEx(hdc, label_x, line2_y, NULL);
                LineTo(hdc, param4 - 0x14, line2_y);
            }
            text_rect.top    = mid_y - 0x32;
            text_rect.right  = param4 - 0x14;
            text_rect.left   = label_x;
            text_rect.bottom = line2_y;

            SelectObject(hdc, g_font_normal);
            DrawTextA(hdc, label_sent, -1, &text_rect, 0);
        }

        /* "Received" label */
        {
            int32_t line3_y = mid_y + 0x1E;
            MoveToEx(hdc, label_x, line3_y, NULL);
            LineTo(hdc, param4 - 0x14, line3_y);

            text_rect.top    = mid_y + 0x0A;
            text_rect.right  = param4 - 0x14;
            text_rect.left   = label_x;
            text_rect.bottom = param4 - 0x14;
            DrawTextA(hdc, label_rcvd, -1, &text_rect, 0);
        }

        /* Status data (+0x25) */
        {
            int32_t line4_y = mid_y + 0x3C;
            MoveToEx(hdc, label_x, line4_y, NULL);
            LineTo(hdc, param4 - 0x14, line4_y);

            SelectObject(hdc, g_font_normal);
            DrawTextA(hdc, (const char*)((int8_t*)playerData + 0x25), -1, &text_rect, 0);
        }

        /* Restore GDI state */
        SelectObject(hdc, old_font);
        SelectObject(hdc, old_pen);
        SetTextColor(hdc, old_text_color);
        SetBkMode(hdc, old_bk_mode);
        DeleteObject(hpen);
    }

    /* 11. Render session/track overlays */
    /* Call vtable slot 26 (0x68/4) on rendering context — EndPaint-like method */
    ctx_vtbl = *(void***)playerData;
    ((void(__stdcall*)(void*, void*))ctx_vtbl[0x68 / 4])(playerData, hdc);

    if (*(int8_t*)((int8_t*)playerData + 0x39) == 0) {
        /* Non-highlighted: render base + frame session overlays */
        /* NOTE: Exact parameters to RenderSessionBase are approximate;
           the original passes values derived from playerData, hdc, and
           the slider positions. The +0x93 field selects the session
           base surface variant. */
        this->RenderSessionBase(
            hdc, (uint32_t)param2, (int32_t)playerData,
            param4, (uint32_t)param5,
            *(uint8_t*)((int8_t*)playerData + 0x93));
        this->RenderSessionFrame(hdc);
    } else {
        /* Highlighted row: render all non-empty track entries (+0x96) */
        const uint8_t* entry_ptr = (const uint8_t*)((int8_t*)playerData + 0x96);
        int32_t entry_count;
        for (entry_count = 0; entry_count < 128; entry_count++) {
            if (entry_ptr[1] == 0) break;
            /* NOTE: Clip coordinates here are approximate; the original
               derives them from playerData, hdc, and param2 offsets */
            this->RenderTrackEntry(
                hdc, (uint32_t)(int32_t)playerData, (uint32_t)hdc,
                param2, (uint32_t)param5,
                entry_ptr);
            entry_ptr += 6;
        }
    }

    /* 12. Blit postcard image if word (+0x3A) is non-zero */
    if (*(int16_t*)((int8_t*)playerData + 0x3A) != 0) {
        void* postcard_surf = this->resource_data;
        if (postcard_surf != NULL) {
            int32_t post_w = *(int32_t*)((int8_t*)postcard_surf + 0x18);
            int32_t post_h = *(int32_t*)((int8_t*)postcard_surf + 0x1C);
            /* Postcard position derived from playerData+0x14 and HDC
               struct offset +0x10 (UIPANEL surface top-left) */
            int32_t post_x = (int32_t)playerData + 0x14;
            int32_t post_y = *(int32_t*)((int8_t*)hdc + 0x10);

            UIPANEL_Blit(postcard_surf,
                         (uint32_t)post_x, (uint32_t)post_y,
                         (uint32_t)(post_x + post_w),
                         (uint32_t)(post_y + post_h),
                         hdc, 0, 0, (uint32_t)post_w, (uint32_t)post_h, 0);
        }
    }

    /* 13. End paint cycle */
    ctx_vtbl = *(void***)playerData;
    ((void(__stdcall*)(void*, void*))ctx_vtbl[0x44 / 4])(playerData, &hbr);

    /* 14. Frame outer rect with black */
    {
        void* black_brush = GetStockObject(4);  /* BLACK_BRUSH */
        FrameRect(hdc, (const RECT*)&param2, black_brush);
    }

    /* 15. Final end-paint method */
    ctx_vtbl = *(void***)playerData;
    ((void(__stdcall*)(void*, void*))ctx_vtbl[0x68 / 4])(playerData, hdc);

    DeleteObject(hbr);
}

/* ================================================================== */
/* RegisterPlayer — 0x444D00                                            */
/*                                                                      */
/* Save a DPLAY_PlayerSlot to a .crd file in the specified PostBag     */
/* subdirectory. Updates message count cache.                          */
/* ================================================================== */
uint32_t NetworkPlayerList::RegisterPlayer(void* player_slot,
                                                        int32_t type,
                                                        int32_t param3)
{
    char filepath[0x504];
    const char* subdir;
    uint32_t result;

    subdir = PostBag_Subdir(type);

    if (param3 == 0) {
        wsprintfA(filepath, (const char*)0x0047e3d0,
                  g_install_path,
                  (const char*)0x0047e0c4,
                  subdir);
    } else {
        wsprintfA(filepath, (const char*)0x0047ed20,
                  g_install_path,
                  (const char*)0x0047e0c4,
                  subdir,
                  param3);
    }

    result = DPLAY_SetPlayerData(player_slot, filepath);
    if ((uint8_t)result == 0) {
        return result & 0xFFFFFF00;
    }

    /* Re-count Sort_Out cache */
    {
        void* node;
        void* next;
        int16_t count = 0;

        node = NET_GetHostName(2, 0);
        while (node != NULL) {
            next = *(void**)((int8_t*)node + 0x504);
            count++;
            GLOBAL_free(node);
            node = next;
        }
        this->msg_count_cache = count;
    }

    if (type == 0) {
        /* PixelDataCache_Lookup omitted for simplification */
    }

    return 1;
}

/* ================================================================== */
/* UnregisterPlayer — 0x444FB0                                          */
/*                                                                      */
/* Delete a player's .crd file and update message count cache.         */
/* ================================================================== */
void NetworkPlayerList::UnregisterPlayer(const char* filepath)
{
    if (DeleteFileA(filepath) != 0) {
        void* node;
        void* next;
        int16_t count = 0;

        node = NET_GetHostName(2, 0);
        while (node != NULL) {
            next = *(void**)((int8_t*)node + 0x504);
            count++;
            GLOBAL_free(node);
            node = next;
        }
        this->msg_count_cache = count;
    }
}

/* ================================================================== */
/* GetPlayerAddress — 0x445000                                          */
/*                                                                      */
/* Delete a player's .crd file by reading configId from player_slot.   */
/* ================================================================== */
void NetworkPlayerList::GetPlayerAddress(void* player_slot,
                                                      int32_t type)
{
    char filepath[0x504];
    const char* subdir;
    int32_t configId;

    subdir = PostBag_Subdir(type);
    configId = *(int32_t*)((int8_t*)player_slot + 0x0C);

    wsprintfA(filepath, (const char*)0x0047ed2c,
              g_install_path,
              (const char*)0x0047e0c4,
              subdir,
              configId);

    if (DeleteFileA(filepath) != 0) {
        void* node;
        void* next;
        int16_t count = 0;

        node = NET_GetHostName(2, 0);
        while (node != NULL) {
            next = *(void**)((int8_t*)node + 0x504);
            count++;
            GLOBAL_free(node);
            node = next;
        }
        this->msg_count_cache = count;
    }
}
