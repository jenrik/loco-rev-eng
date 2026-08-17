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

// Status: TRANSCRIBED

#include "NetworkPlayerList.h"
#include "DPlayManager.h"
#include "../graphics/LOCOBITMAP.h"
#include "../game/PlayerConfig.h"
#include "../platform/ddraw_interfaces.h"
#ifndef _WIN32
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
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

/* File I/O — real host bodies live in shared/link_stubs.cpp (fopen/fread-
 * backed); the CRT_Find* trio backs the _WIN32-only NET_GetHostName
 * enumeration path below (host uses std::filesystem instead — see
 * NET_GetHostName's #ifndef _WIN32 branch). */
extern void*   __stdcall CreateFileA(const char* name, uint32_t access, uint32_t share,
                                      void* security, uint32_t creation,
                                      uint32_t flags, void* tmpl);
extern int32_t __stdcall ReadFile(void* file, void* buf, uint32_t n,
                                   uint32_t* read, void* ovlp);
extern int32_t __stdcall CloseHandle(void* h);
#ifdef _WIN32
extern void*   __stdcall CRT_FindFirstFile(const char* pattern, void* find_data);
extern int32_t __stdcall CRT_FindNextFile(void* handle, void* find_data);
extern int32_t __stdcall CRT_FindClose(void* handle);
#endif

} /* extern "C" */

/* C++ allocation helpers */
extern void* __cdecl operator_new(size_t size);
extern void  __cdecl GLOBAL_free(void* ptr);

/* Game globals */
class ResourceManager;
extern ResourceManager g_resmgr;        /* 0x4855E8 — object, not a pointer (was void*,
                                          * a widespread cross-TU landmine — see
                                          * PROGRESS.md's g_resmgr sweep) */
extern char  g_install_path[];          /* 0x4A99C8 */
extern PlayerConfig* g_player_config;   /* 0x4AA4A8 */

/* UI helpers */
/* Real def: ui/UI_WindowBase.cpp:0x426B00 (a thin free-function shim over
 * the real UI_WindowBase::BeginPaint() method — moved 2026-08-16 from a
 * stale UIPANEL attribution, see PROGRESS.md's 2026-08-16 entry),
 * HDC(void* self). Was declared here with an `int32_t` 1st param,
 * mangling to a distinct symbol from the real function
 * (docs/landmine-sweep-worklist.md). Currently unused within this file
 * (no call sites), so this was a dormant landmine, not a live bug —
 * fixed anyway for consistency with the canonical signature. */
extern HDC   UIPANEL_BeginPaint(void* panel);
/* Real def: ui/UIPANEL.cpp:0x426B90 — the 2nd param is `int hdc`, not
 * `void*`. Was declared here with a void* 2nd param, mangling to a
 * distinct symbol from the real function (same landmine class as
 * UIPANEL_BeginPaint above; docs/landmine-sweep-worklist.md). Currently
 * unused within this file (no call sites), so this was a dormant
 * landmine, not a live bug — fixed anyway for consistency with the
 * canonical signature. */
extern void  __thiscall UIPANEL_EndPaintEx(void* panel, int32_t hdc,
                                            int32_t unlockParam, uint8_t unlockFlag,
                                            RECT* restrictRect);
/* Real def: ui/UIPANEL_Surface.cpp, bool(void*,uint32_t,uint32_t,int32_t,
 * uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t) — was declared
 * uniformly int32_t, which doesn't match the real mixed uint32_t/int32_t
 * shape (call-0 landmine). */
extern bool  __cdecl    UIPANEL_Blit(void* surface, uint32_t srcX, uint32_t srcY,
                                      int32_t srcW, uint32_t srcH,
                                      void* dstSurface, uint32_t dstX,
                                      uint32_t dstY, int32_t dstW,
                                      uint32_t dstH, uint32_t flags);
/* UIPANEL_Surface construction/copy/destruction: no local declarations
 * here — the real, fully INTEGRATED constructor/copy-constructor/
 * destructor (graphics/LOCOBITMAP.h/.cpp, 0x42A110/0x42A1C0/0x42A140) are
 * visible via the LOCOBITMAP.h include above. Callers use
 * `new UIPANEL_Surface()`, `new UIPANEL_Surface(*src)`, and `delete`. */
extern void  __cdecl    UIPANEL_StretchBlit(void* surface, const char* path,
                                              int32_t x, int32_t y, int32_t flags);
extern void* __thiscall ResourceManager_GetById(void* resmgr, uint32_t id);
extern void  __cdecl    FormatResourceString(void* resmgr, uint32_t id,
                                              char* buf, int32_t bufsize);
extern void  __cdecl    UI_CenterWindow(void* outer, void* inner);

/* DPlay helpers */
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
#ifdef _WIN32
    /* Windows-faithful path: literal addresses of the original PE string
     * table (verified via Ghidra get_strings against locoaudit — the raw
     * bytes at each address are the backslash-prefixed subfolder name).
     * Documentation only: this branch is never linked, only type-checked
     * under the MinGW cross build (cross/mingw32-typecheck.txt). */
    switch (type) {
    case 0: return reinterpret_cast<const char*>(0x0047eba4);   /* "\\Album" */
    case 1: return reinterpret_cast<const char*>(0x0047ebc4);   /* "\\Sort\\In" */
    case 2: return reinterpret_cast<const char*>(0x0047ebb8);   /* "\\Sort\\Out" */
    case 3: return reinterpret_cast<const char*>(0x0047ebac);   /* "\\Sort\\Bag" */
    case 4: return reinterpret_cast<const char*>(0x0047eb90);   /* "\\Att_Out" */
    case 5: return reinterpret_cast<const char*>(0x0047eb9c);   /* "\\Att_In" */
    case 6:
        switch (DAT_004a97a0) {
        default: return reinterpret_cast<const char*>(0x0047ebf8);  /* "\\Easter\\Eng" */
        case 1:  return reinterpret_cast<const char*>(0x0047ec58);  /* "\\Easter\\Dan" */
        case 2:  return reinterpret_cast<const char*>(0x0047ec4c);  /* "\\Easter\\Dut" */
        case 4:  return reinterpret_cast<const char*>(0x0047ec40);  /* "\\Easter\\Fre" */
        case 5:  return reinterpret_cast<const char*>(0x0047ec34);  /* "\\Easter\\Ger" */
        case 6:  return reinterpret_cast<const char*>(0x0047ec28);  /* "\\Easter\\Ita" */
        case 7:  return reinterpret_cast<const char*>(0x0047ec1c);  /* "\\Easter\\Nor" */
        case 8:  return reinterpret_cast<const char*>(0x0047ec10);  /* "\\Easter\\Spa" */
        case 9:  return reinterpret_cast<const char*>(0x0047ec04);  /* "\\Easter\\Swe" */
        }
    case 7: return reinterpret_cast<const char*>(0x0047ed18);   /* "\\Design" */
    default: return reinterpret_cast<const char*>(0x0047eba4);
    }
#else
    /* Host addresses do not contain the original PE string literals (see
     * NetworkPlayerList::NetworkPlayerList's own #ifndef _WIN32 branch for
     * the established precedent). Real subfolder names, single forward
     * slash (the original's raw bytes double the backslash separator —
     * a cosmetic quirk with no behavioral effect once concatenated into a
     * filesystem path, so it is not reproduced here). */
    switch (type) {
    case 0: return "/Album";
    case 1: return "/Sort/In";
    case 2: return "/Sort/Out";
    case 3: return "/Sort/Bag";
    case 4: return "/Att_Out";
    case 5: return "/Att_In";
    case 6:
        switch (DAT_004a97a0) {
        default: return "/Easter/Eng";
        case 1:  return "/Easter/Dan";
        case 2:  return "/Easter/Dut";
        case 4:  return "/Easter/Fre";
        case 5:  return "/Easter/Ger";
        case 6:  return "/Easter/Ita";
        case 7:  return "/Easter/Nor";
        case 8:  return "/Easter/Spa";
        case 9:  return "/Easter/Swe";
        }
    case 7: return "/Design";
    default: return "/Album";
    }
#endif
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
        uint32_t* p = reinterpret_cast<uint32_t*>(path_buf + 1);
        for (i = 0x40; i != 0; i--) { *p = 0; p++; }
    }
    path_buf[0x101] = 0;
    path_buf[0x102] = 0;

    /* 1. <install>\PostBag\ */
    wsprintfA(path_buf,
              reinterpret_cast<const char*>(0x0047e8a0),  /* "%s%s" */
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4)); /* "\\PostBag\\" */
    CreateDirectoryA(path_buf, NULL);

    /* 2. <install>\PostBag\Easter (subdir at 0x47ebd8) */
    wsprintfA(path_buf,
              reinterpret_cast<const char*>(0x0047e3d0),  /* "%s%s%s" */
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),
              reinterpret_cast<const char*>(0x0047ebd8)); /* "Easter" */
    CreateDirectoryA(path_buf, NULL);

    /* 3. Sort */
    path_buf[0] = g_empty_string;
    {
        uint32_t* p = reinterpret_cast<uint32_t*>(path_buf + 1);
        for (i = 0x40; i != 0; i--) { *p = 0; p++; }
    }
    path_buf[0x101] = 0;
    path_buf[0x102] = 0;
    wsprintfA(path_buf,
              reinterpret_cast<const char*>(0x0047e3d0),
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),
              reinterpret_cast<const char*>(0x0047ebd0)); /* "Sort" */
    CreateDirectoryA(path_buf, NULL);

    /* 4. Sort_In */
    wsprintfA(path_buf,
              reinterpret_cast<const char*>(0x0047e3d0),
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),
              reinterpret_cast<const char*>(0x0047ebc4)); /* "Sort_In" */
    CreateDirectoryA(path_buf, NULL);

    /* 5. Sort_Out */
    wsprintfA(path_buf,
              reinterpret_cast<const char*>(0x0047e3d0),
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),
              reinterpret_cast<const char*>(0x0047ebb8)); /* "Sort_Out" */
    CreateDirectoryA(path_buf, NULL);

    /* 6. Sort_Bag */
    wsprintfA(path_buf,
              reinterpret_cast<const char*>(0x0047e3d0),
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),
              reinterpret_cast<const char*>(0x0047ebac)); /* "Sort_Bag" */
    CreateDirectoryA(path_buf, NULL);

    /* 7. AlbIndex */
    wsprintfA(path_buf,
              reinterpret_cast<const char*>(0x0047e3d0),
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),
              reinterpret_cast<const char*>(0x0047e0cc)); /* "AlbIndex" */
    CreateDirectoryA(path_buf, NULL);

    /* 8. Album */
    wsprintfA(path_buf,
              reinterpret_cast<const char*>(0x0047e3d0),
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),
              reinterpret_cast<const char*>(0x0047eba4)); /* "Album" */
    CreateDirectoryA(path_buf, NULL);

    /* 9. Att_In */
    wsprintfA(path_buf,
              reinterpret_cast<const char*>(0x0047e3d0),
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),
              reinterpret_cast<const char*>(0x0047eb9c)); /* "Att_In" */
    CreateDirectoryA(path_buf, NULL);

    /* 10. Att_Out */
    wsprintfA(path_buf,
              reinterpret_cast<const char*>(0x0047e3d0),
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),
              reinterpret_cast<const char*>(0x0047eb90)); /* "Att_Out" */
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
        void** mgr_vtbl = *reinterpret_cast<void***>(resource_mgr);
        /* vtable[2] = release/shutdown (byte offset 8 / 4 in the original
         * x86 vtable — array-indexing a void** scales by the host's own
         * pointer width, so this slot index is correct on both 32- and
         * 64-bit hosts; not the byte-offset-misalignment bug class). */
        reinterpret_cast<void(__stdcall*)(void)>(mgr_vtbl[2])();
        resource_mgr = NULL;
        resource_data = NULL;
    }

    /* Free all 256 cached surfaces */
    {
        for (i = 0; i < 256; i++) {
            if (surface_cache[i] != NULL) {
                delete surface_cache[i];
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
UIPANEL_Surface* NetworkPlayerList::GetOrCreateSurface(uint8_t type_hi,
                                                         uint8_t variant,
                                                         uint8_t tag_low,
                                                         uint8_t no_evict)
{
    char  filename[0x150];
    char  filepath[0x510];
    UIPANEL_Surface* surface;
    int32_t i;

    /* Initialize buffers. Address 0x4851D0 is g_empty_string's declared
     * address (see the extern above) — use the named global instead of the
     * raw literal, matching the constructor's own established precedent. */
    filename[0] = g_empty_string;
    {
        uint32_t* p = reinterpret_cast<uint32_t*>(filename + 1);
        for (i = 0x0F; i != 0; i--) { *p = 0; p++; }
    }
    filename[0x41] = 0;
    filename[0x42] = 0;

    filepath[0] = g_empty_string;
    {
        uint32_t* p = reinterpret_cast<uint32_t*>(filepath + 1);
        for (i = 0x140; i != 0; i--) { *p = 0; p++; }
    }
    filepath[0x501] = 0;
    filepath[0x502] = 0;

    /* Every operator_new(sizeof(UIPANEL_Surface)) below was originally a
     * hardcoded 0x20 (the x86 struct's real size) — use sizeof directly
     * since the pointer fields (palette_ptr/pixels/ddraw_surf) widen it to
     * 0x30 on this 64-bit host; see graphics/LOCOBITMAP.h. */

    /* Search cache for matching entry */
    for (i = 0; i < 256; i++) {
        if (this->surface_cache[i] != NULL &&
            this->tags[i].type_hi  == type_hi &&
            this->tags[i].variant  == variant &&
            this->tags[i].tag_low  == tag_low)
        {
            /* Cache hit — update LRU timestamp and return copy */
            this->lru_timestamps[i] = this->frame_counter;
            return new UIPANEL_Surface(*this->surface_cache[i]);
        }
    }

    /* Cache miss — construct filename */
    {
        uint8_t type_idx = type_hi;
        uint8_t v = variant - 1;

        if (type_idx <= 0x0F) {
            wsprintfA(filename, reinterpret_cast<const char*>(0x0047ec9c),
                      static_cast<uint32_t>(type_idx), v, tag_low);
        } else if (type_idx <= 0x19) {
            wsprintfA(filename, reinterpret_cast<const char*>(0x0047ecb0),
                      type_idx + 0x58, v, tag_low);
        } else if (type_idx <= 0x1D) {
            wsprintfA(filename, reinterpret_cast<const char*>(0x0047ecb0),
                      type_idx + 0x5A, v, tag_low);
        } else if (type_idx == 0x1E) {
            wsprintfA(filename, reinterpret_cast<const char*>(0x0047ecc0), v, tag_low);
        } else if (type_idx == 0x1F) {
            wsprintfA(filename, reinterpret_cast<const char*>(0x0047ecd0), tag_low);
        } else {
            wsprintfA(filename, reinterpret_cast<const char*>(0x0047ec8c), v, tag_low);
        }
    }

    /* Build full path */
    wsprintfA(filepath, reinterpret_cast<const char*>(0x0047ec7c),
              g_install_path, filename);

    surface = new UIPANEL_Surface();
    UIPANEL_StretchBlit(surface, filepath, 0, 0, 0);

    /* Check if surface has valid dimensions. Ghidra's decompile of this
     * function (0x4442B0) indexes the surface as an undefined4* array —
     * local_558[6]/[7], i.e. byte offsets +0x18/+0x1C — matching
     * UIPANEL_Surface::pixels/ddraw_surf (graphics/LOCOBITMAP.h), not
     * width/height (+0x08/+0x0C). Confirmed correct as originally written;
     * only the raw-offset spelling changes here. */
    if (surface->pixels == nullptr && surface->ddraw_surf == nullptr) {
        delete surface;
        return NULL;
    }

    /* Find empty cache slot */
    for (i = 0; i < 256; i++) {
        if (this->surface_cache[i] == NULL) {
            this->tags[i].type_hi = type_hi;
            this->tags[i].variant = variant;
            this->tags[i].tag_low = tag_low;
            this->lru_timestamps[i] = this->frame_counter;

            this->surface_cache[i] = new UIPANEL_Surface(*surface);
            return surface;
        }
    }

    /* Cache full — evict LRU if allowed */
    if (no_evict == 0) {
        int32_t lru = find_lru_slot(this);
        if (this->surface_cache[lru] != NULL) {
            delete this->surface_cache[lru];
        }

        this->tags[lru].type_hi = type_hi;
        this->tags[lru].variant = variant;
        this->tags[lru].tag_low = tag_low;
        this->lru_timestamps[lru] = this->frame_counter;

        this->surface_cache[lru] = new UIPANEL_Surface(*surface);
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
    UIPANEL_Surface* surface;
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

    /* width/height, not pixels/ddraw_surf — Ghidra's NET_RenderTrackEntry
     * (0x4440A0) reads this_00[2]/[3] (undefined4* indexing = byte offsets
     * +0x08/+0x0C), matching UIPANEL_Surface::width/height. The raw-offset
     * version of this code used +0x18/+0x1C (pixels/ddraw_surf) instead —
     * a real bug, not just an unmodernized cast; fixed here. */
    surf_w = static_cast<uint32_t>(surface->width);
    surf_h = static_cast<uint32_t>(surface->height);

    /* Compute destination rect from entry position */
    dst_left  = (static_cast<uint32_t>(entry[2]) * 2 - (surf_w >> 1)) + clip_x;
    dst_top   = (static_cast<uint32_t>(entry[4]) * 2 - (surf_h >> 1)) + clip_y;
    dst_right = dst_left + surf_w;
    dst_bot   = dst_top  + surf_h;

    /* Clip to target bounds */
    src_x_off = 0;
    src_y_off = 0;
    src_w = surf_w;
    src_h = surf_h;

    if (static_cast<int32_t>(dst_left) < static_cast<int32_t>(clip_x)) {
        src_x_off = clip_x - dst_left;
        dst_left = clip_x;
    }
    if (static_cast<int32_t>(dst_top) < static_cast<int32_t>(clip_y)) {
        src_y_off = clip_y - dst_top;
        dst_top = clip_y;
    }
    if (clip_right < static_cast<int32_t>(dst_right)) {
        src_w = surf_w + (clip_right - static_cast<int32_t>(dst_right));
        dst_right = clip_right;
    }
    if (static_cast<int32_t>(clip_bot) < static_cast<int32_t>(dst_bot)) {
        src_h = surf_h + (clip_bot - dst_bot);
        dst_bot = clip_bot;
    }

    UIPANEL_Blit(surface, dst_left, dst_top, dst_right, dst_bot,
                 hdc, src_x_off, src_y_off, src_w, src_h, 0);

    /* Release surface copy */
    delete surface;
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
            vtbl = *reinterpret_cast<void***>(this->resource_mgr);
            this->resource_data =
                reinterpret_cast<void*(__stdcall*)(int32_t, int32_t)>(vtbl[1])(0, 0);
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

        entry_ptr = reinterpret_cast<const uint8_t*>(player_slot) + 0x391;
        for (entry_idx = 0x7F; entry_idx >= 0; entry_idx--) {
            if (*entry_ptr != 0) break;
            entry_ptr -= 6;
        }

        if (entry_idx >= 0) {
            /* BUG FIX (Ghidra-confirmed against 0x4436C0's disassembly):
             * the checked byte above is entry[1] (RenderTrackEntry reads
             * entry[0]/entry[1]), i.e. the true entry base is one byte
             * before the address the scan loop found — matching Ghidra's
             * own `param_1 + (iVar4 + 0x19) * 6` versus the checked
             * `param_1 + 0x391`, which differ by exactly 1. The previous
             * expression here, `entry_ptr - 6 * (int32_t)(*entry_ptr -
             * *entry_ptr)`, was a self-subtracting no-op (always 0),
             * silently passing entry_ptr itself (one byte past the real
             * base) instead of entry_ptr - 1. */
            this->RenderTrackEntry(hdc, param3, param4, param5, param6,
                                   entry_ptr - 1);
        }
    }

    /* End paint cycle */
    if (this->resource_mgr != NULL) {
        vtbl = *reinterpret_cast<void***>(this->resource_mgr);
        reinterpret_cast<void(__stdcall*)(void*, void*)>(vtbl[0x44 / 4])(
            this->resource_mgr, &param7);
    }

    {
        void* hbr = GetStockObject(4);  /* BLACK_BRUSH */
        /* &param3 aliases param3/param4/param5/param6 as a packed RECT —
         * the same stack-parameter-aliasing fidelity gap RenderPlayer's
         * &param2 usage documents below; preserved as-is. uint32_t*
         * converts to const void* implicitly, no cast needed. */
        FrameRect(hdc, &param3, hbr);
    }

    if (this->resource_mgr != NULL) {
        vtbl = *reinterpret_cast<void***>(this->resource_mgr);
        reinterpret_cast<void(__stdcall*)(void*, void*)>(vtbl[0x68 / 4])(
            this->resource_mgr, hdc);
    }
}

/* ================================================================== */
/* RenderSessionFrame — 0x443F00                                         */
/* ================================================================== */
void NetworkPlayerList::RenderSessionFrame(void* hdc)
{
    UIPANEL_Surface* surface;
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
        delete surface;
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
    UIPANEL_Surface* surface;
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
        /* width/height (+0x08/+0x0C) — Ghidra's DPLAY_RenderSessionBase
         * (0x443FF0) reads puVar1[2]/[3], matching UIPANEL_Surface's
         * width/height, not pixels/ddraw_surf (+0x18/+0x1C) as the
         * raw-offset version of this code used; same bug class fixed in
         * RenderTrackEntry above. */
        int32_t surf_w = surface->width;
        int32_t surf_h = surface->height;

        UIPANEL_Blit(surface,
                     (param4 - 1) - surf_w, param3 + 1U,
                     param4 - 1, surf_h + param3 + 1U,
                     hdc, 0, 0, surf_w, surf_h, 0);
        delete surface;
    }
}

/* ================================================================== */
/* RenderPlayer — 0x4437C0                                              */
/*                                                                      */
/* Render full player list UI entry with name, session data, track     */
/* piece icons, and postcard image.                                     */
/*                                                                      */
/* === Object-model resolution (2026-08-17) ===                         */
/* A prior pass left `playerData` as an unresolved `void*` because this  */
/* function ALSO dispatches through it as a vtable pointer at slots     */
/* 0x44/4=17 and 0x68/4=26, and DPlayManager's real vtable (0x478264)   */
/* only has one entry — nowhere near 27. That contradiction is now      */
/* resolved: `playerData` was a conflation of TWO distinct real         */
/* parameters, not one, because the real ABI is a 9-argument __thiscall */
/* (confirmed via RET 0x24 in the disassembly) while Ghidra's own        */
/* decompile of this function only recognized 7 — it silently dropped   */
/* 2 of the 9 real stack arguments into unidentified `unaff_*` pseudo-   */
/* locals, and every within-body `param_N` *expression* reference in    */
/* Ghidra's pseudocode (as opposed to the formal parameter list, which   */
/* is correctly numbered) turned out to be consistently offset by +2    */
/* slots from that point on. Verified by mechanical stack-offset         */
/* arithmetic against the raw disassembly of this function AND all 3    */
/* real call sites (Cursor::blit_edit_preview @0x418A76,                */
/* PostcardAlbum::RenderTileName @0x404956, Town::clear_postcard_ui      */
/* @0x42E846):                                                           */
/*   arg1: bool highlighted   — per-call UI state (row selected/active). */
/*         Gates BOTH the background-color branch AND the               */
/*         track-entries-vs-session-base branch below. This is NOT      */
/*         DPlayManager's own m_flag39 — this function never reads that */
/*         field at all; see the corrected note on DPlayManager.h.      */
/*   arg2: DPlayManager* player — supplies every player-slot field this  */
/*         function reads (color_r/g/b, m_playerName, m_sessionBlk1/2,   */
/*         m_wordValue, m_unknown93, m_trackEntries). Confirmed at all   */
/*         3 real call sites: Cursor passes `this->obj_184`, Town passes */
/*         `this->selected_player` (both already-documented              */
/*         DPlayManager*), and PostcardAlbum passes the return of        */
/*         PixelDataCache::LookupAsset — which is itself just             */
/*         NET_ResolveAddress(...)'s return value (network/              */
/*         DPlayManager.h), i.e. also genuinely a DPlayManager*.          */
/*   arg3: IDirectDrawSurface4* surface — the object actually dispatched  */
/*         through vtable slots 17/26. Those slots are exactly            */
/*         IDirectDrawSurface4::GetDC/ReleaseDC in the real DirectDraw    */
/*         COM vtable order (platform/ddraw_interfaces.h); all 3 real     */
/*         callers pass g_primary_surface here, and the call shape at     */
/*         each site (the "this" value pushed as a plain stack argument,  */
/*         no ECX setup) is COM/__stdcall, not __thiscall — impossible    */
/*         for a first-party game method. DPlayManager's own real         */
/*         vtable was independently re-confirmed to hold exactly one      */
/*         entry (see DPlayManager.h), so it was never a candidate.       */
/*   arg4..arg7: int32_t left, top, right, bottom — the row's outer draw  */
/*         rect (Ghidra's corrupted `&param_2`-as-RECT was actually       */
/*         `&arg4` under the +2 shift, i.e. exactly this).                */
/*   arg8: HWND hWnd — the real disassembly shows this value IS pushed    */
/*         into the real RenderSessionBase call (0x443FF0), but that      */
/*         method's own signature was not re-audited in this pass (see    */
/*         the note below) — captured here but not yet forwarded to it.   */
/*   arg9: const RECT* highlightRect — optional; NULL in 2 of 3 real      */
/*         callers (Postcard, Town), a real pointer in Cursor's call.     */
/*                                                                        */
/* RenderSessionBase/RenderTrackEntry's OWN exact parameter semantics    */
/* were not re-audited in this pass (out of scope for the playerData/     */
/* vtable question) — their call arguments below continue the previously */
/* established mapping, now fed the corrected left/top/right/bottom/     */
/* player values instead of the old confused ones.                       */
/* ================================================================== */
void NetworkPlayerList::RenderPlayer(bool highlighted, DPlayManager* player,
                                      IDirectDrawSurface4* surface,
                                      int32_t left, int32_t top,
                                      int32_t right, int32_t bottom,
                                      HWND hWnd, const RECT* highlightRect)
{
    (void)hWnd;  /* real disassembly shows this feeding RenderSessionBase's call site
                  * (0x443FF0), but that method's own signature/semantics were not
                  * re-audited in this pass — see this function's doc comment. */

    int32_t i;
    void* paintHdc = nullptr;   /* real HDC, obtained via surface->GetDC() */
    void* hbrGray;              /* shared 0xE6E6E6 background/highlight brush */
    char label_sent[16];
    char label_rcvd[16];
    RECT rowRect{ left, top, right, bottom };
    RECT text_rect;
    uint32_t frame_count;

    /* Init label buffers — "Sent" / "Received" resource strings */
    label_rcvd[0] = 0;
    *reinterpret_cast<uint32_t*>(label_rcvd + 4) = 0;
    *reinterpret_cast<uint32_t*>(label_rcvd + 8) = 0;
    label_sent[0] = 0;
    *reinterpret_cast<uint32_t*>(label_sent + 4) = 0;
    *reinterpret_cast<uint32_t*>(label_sent + 8) = 0;
    *reinterpret_cast<uint32_t*>(label_sent + 12) = 0;
    *reinterpret_cast<uint16_t*>(label_sent + 14) = 0;

    /* 1. Lazily cache resource from ResourceManager_GetById(0x3CBD) */
    if (this->resource_data == NULL) {
        this->resource_mgr = ResourceManager_GetById(&g_resmgr, 0x3CBD);
        if (this->resource_mgr != NULL) {
            void** ctx_vtbl = *reinterpret_cast<void***>(this->resource_mgr);
            this->resource_data =
                reinterpret_cast<void*(__stdcall*)(int32_t, int32_t)>(ctx_vtbl[1])(0, 0);
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

    /* 4. Begin paint cycle on the real DirectDraw surface (was manual
     *    vtable[0x44/4] dispatch through the misidentified `playerData` —
     *    now a typed virtual call on the real `surface` parameter). */
    surface->GetDC(&paintHdc);

    /* 6. Shared light-gray brush — created unconditionally up front,
     * matching the original (which allocates it before the highlighted
     * check even though the `highlighted` branch below never uses it),
     * and deleted once at the very end of the function regardless of
     * which branch ran. */
    hbrGray = CreateSolidBrush(0xE6E6E6);

    /* 5. Draw background.
     *    `highlighted` (arg1) selects a custom row color computed from
     *    the player's own color_r/g/b, or the default white background
     *    with an optional highlight box. */
    if (highlighted) {
        uint32_t color = NET_ComputeColor(player->color_r, player->color_g, player->color_b);
        void* hbrColor = CreateSolidBrush(color);
        FillRect(paintHdc, &rowRect, hbrColor);
        DeleteObject(hbrColor);
    } else {
        void* hbrWhite = GetStockObject(0);  /* WHITE_BRUSH */
        FillRect(paintHdc, &rowRect, hbrWhite);

        if (highlightRect != NULL) {
            FillRect(paintHdc, highlightRect, hbrGray);
            /* DrawEdge's real signature (declared above) takes a
             * non-const void* qrc; highlightRect is an optional
             * caller-owned rect this function only reads. const_cast
             * (not reinterpret_cast) makes that qualifier drop explicit. */
            DrawEdge(paintHdc, const_cast<RECT*>(highlightRect), 5, 0xF);
        }

        /* 7. Player name text rectangle (left side panel) */
        text_rect.left   = left + 10;
        text_rect.bottom = bottom - 10;
        text_rect.top    = top + 2;
        text_rect.right  = (right - left) / 2 - 0x14 + text_rect.left;

        if (highlightRect != NULL) {
            RECT edge_rect;
            CopyRect(&edge_rect, &text_rect);
            InflateRect(&edge_rect, 2, 2);
            FillRect(paintHdc, &edge_rect, hbrGray);
            DrawEdge(paintHdc, &edge_rect, 10, 0xF);
        }

        /* 8. Draw player name in orange text (skip empty/1-char names) */
        {
            const char* name = player->m_playerName;
            uint32_t name_len;
            for (name_len = 0; name[name_len] != '\0'; name_len++) { }

            if (name_len > 1) {
                uint32_t old_text_color = SetTextColor(paintHdc, 0xFF5C00);
                int32_t  old_bk_mode    = SetBkMode(paintHdc, 1);  /* TRANSPARENT */
                void*    old_font       = SelectObject(paintHdc, g_font_small);
                DrawTextA(paintHdc, name, -1, &text_rect, 0x2810);
                SelectObject(paintHdc, old_font);
                SetTextColor(paintHdc, old_text_color);
                SetBkMode(paintHdc, old_bk_mode);
            }
        }

        /* 9. Vertical divider line (gray, width=2) */
        void* hpen = CreatePen(0, 2, 0x808080);
        void* old_pen = SelectObject(paintHdc, hpen);
        int32_t mid_x = (right - left) / 2 + left;
        MoveToEx(paintHdc, mid_x, top + 2, NULL);
        LineTo(paintHdc, mid_x, bottom - 10);

        /* 10. Session data section (right side of panel) */
        int32_t mid_y = top + (bottom - top) / 2;
        {
            int32_t label_x = mid_x + 10;
            uint32_t old_text_color;
            int32_t old_bk_mode;

            if (highlightRect == NULL) {
                MoveToEx(paintHdc, label_x, mid_y, NULL);
                LineTo(paintHdc, right - 0x14, mid_y);
                text_rect.left = label_x;
            } else {
                text_rect.left = mid_x + 0x0E;
            }
            text_rect.top    = mid_y - 0x14;
            text_rect.right  = right - 0x14;
            text_rect.bottom = mid_y;

            SelectObject(paintHdc, g_font_normal);
            old_text_color = SetTextColor(paintHdc, 0xFF5C00);
            old_bk_mode = SetBkMode(paintHdc, 1);

            /* "Sent" line — player->m_sessionBlk1, treated as a C string.
             * The original explicitly falls back to a literal "?" glyph
             * (the string at 0x47EC70) when that string is empty. */
            {
                const char* session_data = reinterpret_cast<const char*>(player->m_sessionBlk1);
                DrawTextA(paintHdc, (session_data[0] == '\0') ? "?" : session_data,
                          -1, &text_rect, 0);
            }

            /* Second horizontal divider + "Sent" resource-string label */
            {
                int32_t line2_y = mid_y - 0x1E;
                if (highlightRect == NULL) {
                    MoveToEx(paintHdc, label_x, line2_y, NULL);
                    LineTo(paintHdc, right - 0x14, line2_y);
                }
                text_rect.top    = mid_y - 0x32;
                text_rect.right  = right - 0x14;
                text_rect.left   = label_x;
                text_rect.bottom = line2_y;

                SelectObject(paintHdc, g_font_normal);
                DrawTextA(paintHdc, label_sent, -1, &text_rect, 0);
            }

            /* "Received" resource-string label */
            {
                int32_t line3_y = mid_y + 0x1E;
                MoveToEx(paintHdc, label_x, line3_y, NULL);
                LineTo(paintHdc, right - 0x14, line3_y);

                text_rect.top    = mid_y + 0x0A;
                text_rect.right  = right - 0x14;
                text_rect.left   = label_x;
                /* Reuses `right` (not `bottom`) as this label's bottom edge —
                 * preserved verbatim from the resolved disassembly; unusual,
                 * but it is what the original computes. */
                text_rect.bottom = right - 0x14;
                DrawTextA(paintHdc, label_rcvd, -1, &text_rect, 0);
            }

            /* Status data — player->m_sessionBlk2 */
            {
                int32_t line4_y = mid_y + 0x3C;
                MoveToEx(paintHdc, label_x, line4_y, NULL);
                LineTo(paintHdc, right - 0x14, line4_y);

                SelectObject(paintHdc, g_font_normal);
                DrawTextA(paintHdc, reinterpret_cast<const char*>(player->m_sessionBlk2),
                          -1, &text_rect, 0);
            }

            /* Restore GDI state */
            SelectObject(paintHdc, old_pen);
            SetTextColor(paintHdc, old_text_color);
            SetBkMode(paintHdc, old_bk_mode);
            DeleteObject(hpen);
        }
    }

    /* 11. End the first paint cycle before rendering session/track
     *     overlays and the postcard blit (matches the original's two
     *     separate GetDC/ReleaseDC round-trips — the DC is not held
     *     across these calls). */
    surface->ReleaseDC(paintHdc);

    if (!highlighted) {
        /* Non-highlighted: render base + frame session overlays.
         * player->m_unknown93 selects the session base surface variant. */
        this->RenderSessionBase(paintHdc, static_cast<uint32_t>(left), top,
                                 right, static_cast<uint32_t>(bottom),
                                 player->m_unknown93);
        this->RenderSessionFrame(paintHdc);
    } else {
        /* Highlighted row: render all non-empty track entries */
        const uint8_t* entry_ptr = player->m_trackEntries;
        int32_t entry_count;
        for (entry_count = 0; entry_count < 128; entry_count++) {
            if (entry_ptr[1] == 0) break;
            this->RenderTrackEntry(paintHdc, static_cast<uint32_t>(left),
                                    static_cast<uint32_t>(top), right,
                                    static_cast<uint32_t>(bottom), entry_ptr);
            entry_ptr += 6;
        }
    }

    /* 12. Blit postcard image if player->m_wordValue is non-zero */
    if (player->m_wordValue != 0) {
        UIPANEL_Surface* postcard_surf = static_cast<UIPANEL_Surface*>(this->resource_data);
        if (postcard_surf != NULL) {
            int32_t post_w = postcard_surf->width;
            int32_t post_h = postcard_surf->height;
            int32_t post_x = left + 0x14;
            int32_t post_y = top;

            UIPANEL_Blit(postcard_surf,
                         static_cast<uint32_t>(post_x), static_cast<uint32_t>(post_y),
                         static_cast<uint32_t>(post_x + post_w),
                         static_cast<uint32_t>(post_y + post_h),
                         paintHdc, 0, 0, static_cast<uint32_t>(post_w),
                         static_cast<uint32_t>(post_h), 0);
        }
    }

    /* 13. Second paint cycle: frame the whole row rect with black. */
    surface->GetDC(&paintHdc);
    {
        void* black_brush = GetStockObject(4);  /* BLACK_BRUSH */
        FrameRect(paintHdc, &rowRect, black_brush);
    }
    surface->ReleaseDC(paintHdc);

    DeleteObject(hbrGray);
}

/* ================================================================== */
/* EnumeratePlayers — 0x443260                                          */
/*                                                                      */
/* Load cached player names from locale-specific easter_usr file under  */
/* PostBag\Easter\<language>. Names read as newline-separated entries.  */
/* Guarded by enumerated flag — early return if already enumerated.      */
/* ================================================================== */
void NetworkPlayerList::EnumeratePlayers()
{
    char path_buf[0x2510];           /* local path/file buffer */
    char file_buf[0x2000];           /* file read buffer */
    void* file_handle;
    uint32_t bytes_read;
    int32_t i, j, k;
    uint8_t ch;

    /* Check enumeration guard — early return if already done */
    if (this->enumerated != 0) {
        return;
    }

    /* Zero the path buffer and player name array */
    {
        uint32_t* p = reinterpret_cast<uint32_t*>(path_buf);
        for (i = 0x2510 / 4; i > 0; i--) { *p++ = 0; }
    }

    /* Zero all 16 player name slots (13 bytes each) */
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 13; j++) {
            this->player_names[i][j] = '\0';
        }
    }

    /* Select Easter language-specific subdirectory path by global selector.
     * DAT_004a97a0 determines the season/language variant. Each case
     * references a Windows string literal address (these are documentation
     * only on host builds; real paths are constructed via snprintf in the
     * #ifndef _WIN32 branch). */
    extern int32_t DAT_004a97a0;
    const char* easter_path;

#ifdef _WIN32
    /* Windows path: original PE string-literal addresses. Documentation
     * only — never linked, only type-checked under cross-compile. */
    switch (DAT_004a97a0) {
    default: easter_path = reinterpret_cast<const char*>(0x0047ebf8);   /* "\\Easter\\Eng" */
        break;
    case 1:  easter_path = reinterpret_cast<const char*>(0x0047ec58);   /* "\\Easter\\Dan" */
        break;
    case 2:  easter_path = reinterpret_cast<const char*>(0x0047ec4c);   /* "\\Easter\\Dut" */
        break;
    case 4:  easter_path = reinterpret_cast<const char*>(0x0047ec40);   /* "\\Easter\\Fre" */
        break;
    case 5:  easter_path = reinterpret_cast<const char*>(0x0047ec34);   /* "\\Easter\\Ger" */
        break;
    case 6:  easter_path = reinterpret_cast<const char*>(0x0047ec28);   /* "\\Easter\\Ita" */
        break;
    case 7:  easter_path = reinterpret_cast<const char*>(0x0047ec1c);   /* "\\Easter\\Nor" */
        break;
    case 8:  easter_path = reinterpret_cast<const char*>(0x0047ec10);   /* "\\Easter\\Spa" */
        break;
    case 9:  easter_path = reinterpret_cast<const char*>(0x0047ec04);   /* "\\Easter\\Swe" */
        break;
    }
#else
    /* Host path: construct the language variant name matching DAT_004a97a0.
     * Use forward slashes for portability. */
    static const char* const lang_variants[] = {
        "/Easter/Eng",  /* default: index 0 */
        "/Easter/Dan",  /* case 1 */
        "/Easter/Dut",  /* case 2 */
        "/Easter/Eng",  /* case 3 (undefined, default) */
        "/Easter/Fre",  /* case 4 */
        "/Easter/Ger",  /* case 5 */
        "/Easter/Ita",  /* case 6 */
        "/Easter/Nor",  /* case 7 */
        "/Easter/Spa",  /* case 8 */
        "/Easter/Swe",  /* case 9 */
    };
    easter_path = (DAT_004a97a0 >= 0 && DAT_004a97a0 <= 9) ?
        lang_variants[DAT_004a97a0] : lang_variants[0];
#endif

    /* Build the full path: <install>\PostBag<Easter_lang>\easter_usr
     * Format string at 0x47ebe4 is "%s%s%s\\easter_usr" (or equivalent). */
#ifdef _WIN32
    /* Windows path: use original format string address for type-checking */
    wsprintfA(path_buf, reinterpret_cast<const char*>(0x0047ebe4),
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),    /* "\\PostBag\\" */
              easter_path);
#else
    /* Host path: construct path with forward slashes */
    std::snprintf(path_buf, sizeof(path_buf), "%s/PostBag%s/easter_usr",
                  g_install_path, easter_path);
#endif

    /* Open the file for reading */
    file_handle = CreateFileA(path_buf, 0x80000000, 1, nullptr, 3, 0x8000000, nullptr);
    if (file_handle == reinterpret_cast<void*>(static_cast<intptr_t>(-1))) {
        /* File not found or cannot open — leave enumerated = 0 */
        return;
    }

    /* Read up to 0x2000 bytes from the file */
    bytes_read = 0;
    if (!ReadFile(file_handle, file_buf, 0x2000, &bytes_read, NULL) || bytes_read == 0) {
        CloseHandle(file_handle);
        return;
    }

    /* Parse lines from file into player_names array.
     * Each line is up to 12 characters, separated by \r\n or \n.
     * Fills 16 slots of 13 bytes each (12 chars + null terminator).
     * Derived from disassembly 0x443376-0x4433E4: if a name is 12 chars,
     * skip any remaining characters on that line until \n. */
    i = 0;  /* buffer index (EAX in disasm) */
    k = 0;  /* slot index as offset: 0, 13, 26, ... 195 (EDI in disasm) */

    while (k < static_cast<int32_t>(16 * 13) && i < static_cast<int32_t>(bytes_read)) {
        j = 0;  /* characters copied to current slot (ECX in disasm) */

        /* Inner loop: copy characters until 12 copied or \r found */
        while (j < 12 && i < static_cast<int32_t>(bytes_read)) {
            ch = static_cast<uint8_t>(file_buf[i]);

            if (ch == 0x0D) {  /* 0xD = '\r' */
                i++;
                break;
            }

            /* Copy character to slot */
            this->player_names[k / 13][j] = static_cast<char>(ch);
            j++;
            i++;
        }

        /* Null-terminate the name in current slot */
        this->player_names[k / 13][j] = '\0';

        /* If name is exactly 12 chars, skip remaining chars on this line until \n.
         * This handles truncation of longer names. */
        if (j == 12) {
            while (i < static_cast<int32_t>(bytes_read)) {
                ch = static_cast<uint8_t>(file_buf[i]);
                if (ch == 0x0A) {  /* Found \n — skip it and break */
                    i++;
                    break;
                }
                i++;
            }
        } else {
            /* Name shorter than 12 chars — skip any trailing \r\n */
            while (i < static_cast<int32_t>(bytes_read)) {
                ch = static_cast<uint8_t>(file_buf[i]);
                if (ch == 0x0A) {  /* 0xA = '\n' */
                    i++;
                    break;
                }
                if (ch != 0x0D) {  /* Not \r — stop skipping */
                    break;
                }
                i++;
            }
        }

        /* Move to next slot (add 13 bytes offset) */
        k += 13;
    }

    CloseHandle(file_handle);

    /* Mark enumeration as complete */
    this->enumerated = 1;
}

/* ================================================================== */
/* count_and_free_postbag_list — shared by RegisterPlayer/                */
/* UnregisterPlayer/GetPlayerAddress (0x444D00/0x444FB0/0x445000) and     */
/* NET_UpdatePlayerList (0x445170): each walks a NET_GetHostName(2, 0)    */
/* list, frees every node, and returns the count. The decompiler shows    */
/* this exact loop inlined at all four addresses. */
/* ================================================================== */
static int16_t count_and_free_postbag_list(PostBagFileNode* node)
{
    int16_t count = 0;
    while (node != nullptr) {
        PostBagFileNode* next = node->next;
        ++count;
        GLOBAL_free(node);
        node = next;
    }
    return count;
}

/* ================================================================== */
/* RegisterPlayer — 0x444D00                                            */
/*                                                                      */
/* Save a DPLAY_PlayerSlot to a .crd file in the specified PostBag     */
/* subdirectory. Updates message count cache.                          */
/* ================================================================== */
uint32_t NetworkPlayerList::RegisterPlayer(DPlayManager* player_slot,
                                                        int32_t type,
                                                        int32_t param3)
{
    char filepath[0x504];
    const char* subdir;
    uint32_t result;

    subdir = PostBag_Subdir(type);

    if (param3 == 0) {
        wsprintfA(filepath, reinterpret_cast<const char*>(0x0047e3d0),
                  g_install_path,
                  reinterpret_cast<const char*>(0x0047e0c4),
                  subdir);
    } else {
        wsprintfA(filepath, reinterpret_cast<const char*>(0x0047ed20),
                  g_install_path,
                  reinterpret_cast<const char*>(0x0047e0c4),
                  subdir,
                  param3);
    }

    result = static_cast<uint32_t>(player_slot->SetPlayerData(filepath));
    if (static_cast<uint8_t>(result) == 0) {
        return result & 0xFFFFFF00;
    }

    /* Re-count Sort_Out cache */
    this->msg_count_cache = count_and_free_postbag_list(NET_GetHostName(2, 0));

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
        this->msg_count_cache = count_and_free_postbag_list(NET_GetHostName(2, 0));
    }
}

/* ================================================================== */
/* GetPlayerAddress — 0x445000                                          */
/*                                                                      */
/* Delete a player's .crd file by reading configId from player_slot.   */
/* ================================================================== */
void NetworkPlayerList::GetPlayerAddress(DPlayManager* player_slot,
                                                      int32_t type)
{
    char filepath[0x504];
    const char* subdir;
    int32_t configId;

    subdir = PostBag_Subdir(type);
    configId = player_slot->m_configId;

    wsprintfA(filepath, reinterpret_cast<const char*>(0x0047ed2c),
              g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4),
              subdir,
              configId);

    if (DeleteFileA(filepath) != 0) {
        this->msg_count_cache = count_and_free_postbag_list(NET_GetHostName(2, 0));
    }
}

/* ================================================================== */
/* NET_GetHostName — 0x4446F0                                             */
/*                                                                        */
/* Enumerate validated PostBag .crd files under a subdirectory selected   */
/* by `type` (see PostBag_Subdir), matching the local player's numeric    */
/* id prefix ("%03d*.crd"). A file is "validated" when its first 2 bytes  */
/* equal PLAYERCONFIG_MAGIC (0x66) — the same postcard-record magic        */
/* PlayerConfig writes (game/PlayerConfig.h). Returns a caller-owned       */
/* singly-linked list of full file paths (free with GLOBAL_free, node by   */
/* node — see PostBagFileNode in NetworkPlayerList.h for why this is a     */
/* typed struct rather than the original's raw 0x508-byte heap block).     */
/*                                                                        */
/* `param2` selects an extra numbered subfolder when non-zero; every real  */
/* call site recovered in this codebase (Town.cpp, NetworkPlayerList.cpp)  */
/* always passes 0, so that branch is implemented for fidelity/completeness*/
/* only and is not exercised by any test.                                  */
/* ================================================================== */
PostBagFileNode* NET_GetHostName(int32_t type, int32_t param2)
{
#ifdef _WIN32
    /* Windows-faithful path: mirrors the decompiled 0x4446F0 control flow
     * with the original PE string-literal addresses. Documentation only —
     * never linked, only type-checked under the MinGW cross build. */
    const char* subdir = PostBag_Subdir(type);
    char wildcard[0x504] = {};
    char directory[0x504] = {};
    if (param2 == 0) {
        wsprintfA(wildcard, reinterpret_cast<const char*>(0x0047ece4), g_install_path,
                  reinterpret_cast<const char*>(0x0047e0c4), subdir, g_player_config->player_id);
        wsprintfA(directory, reinterpret_cast<const char*>(0x0047ecdc), g_install_path,
                  reinterpret_cast<const char*>(0x0047e0c4), subdir);
    } else {
        wsprintfA(wildcard, reinterpret_cast<const char*>(0x0047ed04), g_install_path,
                  reinterpret_cast<const char*>(0x0047e0c4), subdir, param2,
                  g_player_config->player_id);
        wsprintfA(directory, reinterpret_cast<const char*>(0x0047ecf8), g_install_path,
                  reinterpret_cast<const char*>(0x0047e0c4), subdir, param2);
    }

    PostBagFileNode* head = nullptr;
    uint8_t find_data[280] = {};   /* CRT _finddata_t-style block (0x467A20) */
    void* handle = CRT_FindFirstFile(wildcard, find_data);
    if (handle != reinterpret_cast<void*>(static_cast<intptr_t>(-1))) {
        const char* filename = reinterpret_cast<const char*>(find_data + 0x14);
        do {
            if (filename[0] != '.') {
                auto* node = static_cast<PostBagFileNode*>(operator_new(sizeof(PostBagFileNode)));
                node->path[0] = '\0';
                node->next = nullptr;
                wsprintfA(node->path, reinterpret_cast<const char*>(0x0047e8a0), directory, filename);

                void* file = CreateFileA(node->path, 0x80000000, 1, nullptr, 3,
                                          0x8000000, nullptr);
                uint16_t magic = 0;
                uint32_t bytes_read = 0;
                if (file != reinterpret_cast<void*>(static_cast<intptr_t>(-1))) {
                    if (!ReadFile(file, &magic, 2, &bytes_read, nullptr) || bytes_read != 2) {
                        magic = 0;
                    }
                    CloseHandle(file);
                }
                if (magic == PLAYERCONFIG_MAGIC) {
                    node->next = head;
                    head = node;
                } else {
                    GLOBAL_free(node);
                }
            }
        } while (CRT_FindNextFile(handle, find_data) == 0);
        CRT_FindClose(handle);
    }
    return head;
#else
    /* Host path: same algorithm (subdirectory -> numeric-prefix match ->
     * 2-byte magic check), built on std::filesystem instead of the CRT
     * _findfirst/_findnext pair (see PostBag_Subdir's #ifndef _WIN32
     * branch for the sibling precedent of real, non-address strings). */
    const char* subdir = PostBag_Subdir(type);
    char directory[0x504] = {};
    char numeric_prefix[16] = {};
    if (param2 == 0) {
        std::snprintf(directory, sizeof(directory), "%s/PostBag%s",
                      g_install_path, subdir);
    } else {
        std::snprintf(directory, sizeof(directory), "%s/PostBag%s/%d",
                      g_install_path, subdir, param2);
    }
    std::snprintf(numeric_prefix, sizeof(numeric_prefix), "%03d",
                  g_player_config != nullptr ? g_player_config->player_id : 0);

    PostBagFileNode* head = nullptr;
    std::error_code error;
    const std::filesystem::path dir_path(directory);
    for (std::filesystem::directory_iterator it(dir_path, error), end;
         !error && it != end; it.increment(error)) {
        const std::string filename = it->path().filename().string();
        std::error_code type_error;
        if (filename.empty() || filename.front() == '.' ||
            it->is_directory(type_error)) {
            continue;
        }
        /* "%03d*.crd" — numeric-id prefix + ".crd" suffix. */
        if (filename.rfind(numeric_prefix, 0) != 0) continue;
        if (filename.size() < 4 ||
            filename.compare(filename.size() - 4, 4, ".crd") != 0) {
            continue;
        }

        const std::string full_path = (dir_path / filename).string();
        auto* node = static_cast<PostBagFileNode*>(operator_new(sizeof(PostBagFileNode)));
        node->next = nullptr;
        std::strncpy(node->path, full_path.c_str(), sizeof(node->path) - 1);
        node->path[sizeof(node->path) - 1] = '\0';

        uint16_t magic = 0;
        void* file = CreateFileA(node->path, 0x80000000, 1, nullptr, 3, 0x8000000, nullptr);
        if (file != reinterpret_cast<void*>(static_cast<intptr_t>(-1))) {
            uint32_t bytes_read = 0;
            if (!ReadFile(file, &magic, 2, &bytes_read, nullptr) || bytes_read != 2) {
                magic = 0;
            }
            CloseHandle(file);
        }
        if (magic == PLAYERCONFIG_MAGIC) {
            node->next = head;
            head = node;
        } else {
            GLOBAL_free(node);
        }
    }
    return head;
#endif
}

/* ================================================================== */
/* NET_SendFile — 0x445620                                               */
/*                                                                        */
/* Build a locale-suffixed PostBag attachment file path for `name`        */
/* (typically a numeric player-slot id string from CRT_itoa).             */
/* `wantsRoute` selects the extension: nonzero -> ".crd" (route/card       */
/* file), zero -> ".rsp" (address/response file) — matches                */
/* NETMAN_ReceiveSignalChange's two call sites (route file, then address   */
/* file). The locale subdirectory is selected by the same DAT_004a97a0     */
/* global EnumeratePlayers/NET_GetHostName already read above — despite    */
/* the similar name, this is NOT ResourceManager::language_id (different   */
/* numbering/purpose; that one offsets localized string-table resource     */
/* ids, this one picks a PostBag\Easter\<lang> directory).                 */
/* ================================================================== */
void NET_SendFile(const char* name, uint8_t wantsRoute, char* pathBuf)
{
    extern int32_t DAT_004a97a0;

#ifdef _WIN32
    /* Windows path: original PE string-literal addresses. Documentation
     * only — never linked, only type-checked under the MinGW cross build. */
    const char* suffix = wantsRoute
        ? reinterpret_cast<const char*>(0x0047eb4c)   /* ".crd" */
        : reinterpret_cast<const char*>(0x0047ed68);  /* ".rsp" */
    const char* easter_path;
    switch (DAT_004a97a0) {
    default: easter_path = reinterpret_cast<const char*>(0x0047ebf8); break; /* "\\Easter\\Eng" */
    case 1:  easter_path = reinterpret_cast<const char*>(0x0047ec58); break; /* "\\Easter\\Dan" */
    case 2:  easter_path = reinterpret_cast<const char*>(0x0047ec4c); break; /* "\\Easter\\Dut" */
    case 4:  easter_path = reinterpret_cast<const char*>(0x0047ec40); break; /* "\\Easter\\Fre" */
    case 5:  easter_path = reinterpret_cast<const char*>(0x0047ec34); break; /* "\\Easter\\Ger" */
    case 6:  easter_path = reinterpret_cast<const char*>(0x0047ec28); break; /* "\\Easter\\Ita" */
    case 7:  easter_path = reinterpret_cast<const char*>(0x0047ec1c); break; /* "\\Easter\\Nor" */
    case 8:  easter_path = reinterpret_cast<const char*>(0x0047ec10); break; /* "\\Easter\\Spa" */
    case 9:  easter_path = reinterpret_cast<const char*>(0x0047ec04); break; /* "\\Easter\\Swe" */
    }
    wsprintfA(pathBuf, reinterpret_cast<const char*>(0x0047ed5c), g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4), /* "PostBag" */
              easter_path, name, suffix);
#else
    /* Host path: construct path with forward slashes, matching
     * EnumeratePlayers'/NET_GetHostName's #ifndef _WIN32 branches. */
    static const char* const lang_variants[] = {
        "/Easter/Eng",  /* default: index 0 */
        "/Easter/Dan",  /* case 1 */
        "/Easter/Dut",  /* case 2 */
        "/Easter/Eng",  /* case 3 (undefined, default) */
        "/Easter/Fre",  /* case 4 */
        "/Easter/Ger",  /* case 5 */
        "/Easter/Ita",  /* case 6 */
        "/Easter/Nor",  /* case 7 */
        "/Easter/Spa",  /* case 8 */
        "/Easter/Swe",  /* case 9 */
    };
    const char* easter_path = (DAT_004a97a0 >= 0 && DAT_004a97a0 <= 9)
        ? lang_variants[DAT_004a97a0] : lang_variants[0];
    const char* suffix = wantsRoute ? ".crd" : ".rsp";
    std::snprintf(pathBuf, 0x504, "%s/PostBag%s/%s%s",
                  g_install_path, easter_path, name, suffix);
#endif
}

/* ================================================================== */
/* NET_UpdatePlayerList — 0x445170                                        */
/*                                                                        */
/* Count validated Sort_Out (.crd) entries; frees the whole list as it     */
/* counts, matching the decompiled loop exactly. */
/* ================================================================== */
short NET_UpdatePlayerList(void)
{
    return count_and_free_postbag_list(NET_GetHostName(2, 0));
}

/* ================================================================== */
/* NET_DownloadAsset — 0x445A40                                          */
/*                                                                        */
/* Read up to 0x400 bytes of a PostBag attachment. Album/Sort_In/        */
/* Sort_Out/Att_In/Att_Out/Easter (language variant)/Design subfolder     */
/* keyed by `type`, named "<install>/PostBag<subdir>/%08d.dat"           */
/* (player_id), read into `buf`.                                         */
/* ================================================================== */
void NET_DownloadAsset(uint32_t player_id, int32_t type, void* buf)
{
    const char* subdir = PostBag_Subdir(type);
    char path[0x504] = {};

#ifdef _WIN32
    /* Windows-faithful path: original PE string-literal address for the
     * "%s%s%s\%08d.dat" format (documentation only, never linked). */
    wsprintfA(path, reinterpret_cast<const char*>(0x0047ed3c), g_install_path,
              reinterpret_cast<const char*>(0x0047e0c4), subdir, player_id & 0xffff);
#else
    std::snprintf(path, sizeof(path), "%s/PostBag%s/%08u.dat",
                  g_install_path, subdir, player_id & 0xffffu);
#endif

    void* file = CreateFileA(path, 0x80000000, 1, nullptr, 3, 0x8000000, nullptr);
    if (file != reinterpret_cast<void*>(static_cast<intptr_t>(-1))) {
        uint32_t bytes_read = 0;
        ReadFile(file, buf, 0x400, &bytes_read, nullptr);
        CloseHandle(file);
    }
}
