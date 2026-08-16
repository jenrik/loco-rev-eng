/**
 * UIPANEL_Draw.cpp — UIPANEL drawing functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Drawing functions for UIPANEL: DrawBorder, DrawButton, DrawCheckbox,
 * DrawRadioButton, DrawEditField, plus sprite-list management (CreateSprite,
 * InitSprite, BlitSprite, BlitSpriteEx, Hide) built on the real SaveSprite
 * class (ui/SaveSprite.h) — its constructor/destructor (formerly the
 * separate free functions FreeSprite/DtorSprite) are also implemented here.
 */

// Status: TRANSCRIBED

#include "UIPANEL.h"
#include "UIPANEL_Surface.h"
#ifndef _WIN32
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <strings.h>
#include <system_error>
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {
    #define INVALID_HANDLE_VALUE    ((void*)-1)
    #define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
    /* Additional Win32 API declarations. (Note: file enumeration itself
     * goes through the CRT_FindFirstFile/CRT_FindNextFile/CRT_FindClose
     * trio below, not raw FindFirstFileA/FindNextFileA/FindClose -- see
     * UIPANEL::DrawEditField's doc comment.) */
    int    CreateDirectoryA(const char* lpPathName, void* lpSecurityAttributes);
    int    wsprintfA(char* buf, const char* fmt, ...);
    DWORD  GetFileAttributesA(const char* lpFileName);
    int    DeleteFileA(const char* lpFileName);
    DWORD  GetLastError(void);
}

    /* UIPANEL internal functions */
    void   UIPANEL_SetClipRect(void* surf, int x, int y);
    void   UIPANEL_DrawButton(int param_1);
    void   UIPANEL_StretchBlit(void* dst, void* src, int a, int b, int c);
    void   UIPANEL_FillRect(void* surf, int w, int h);

    void __thiscall UIPANEL_DrawBorder(UIPANEL* self, SaveSprite* resource_ptr);
    void __thiscall UIPANEL_GetButtonState(void* self, int pos_x, int pos_y);
    uint32_t __thiscall UIPANEL_SetButtonState(void* self, void* entity,
                                                uint32_t state, uint32_t mode);
    uint8_t __thiscall UIPANEL_DrawCheckbox(void* self, void* entity,
                                             uint32_t state, uint32_t mode);
    void __thiscall UIPANEL_DrawRadioButton(UIPANEL* self, void* entity);
    uint32_t __fastcall UIPANEL_DrawEditField(UIPANEL* self);
    /* UIPANEL_FreeSprite (0x429820) / UIPANEL_DtorSprite (0x429830) are no
     * longer separate free functions -- they were SaveSprite's destructor
     * body/scalar-deleting-dtor slot; see ui/SaveSprite.h's ~SaveSprite(). */
    void __thiscall UIPANEL_CreateSprite(UIPANEL* self, SaveSprite* list_entry);
    void __fastcall UIPANEL_InitSprite(UIPANEL* self);
    void __fastcall UIPANEL_BlitSprite(UIPANEL* self);
    void __fastcall UIPANEL_BlitSpriteEx(UIPANEL* self);
    void __thiscall UIPANEL_Hide(void* self, const char* filename);
    /* operator_new, GLOBAL_free already declared via compat.h */
    void   CRT_strncat_s(void* dst, void* src, void* max, void* src2, void* max2);
    void   RESDATA_BaseInit(void* self);
    void   RESDATA_DtorBase(void* self);
    void   RESDATA_SoundObject_Init(void* sprite, const char* str);
    char*  RESDATA_SoundObject_GetState(void* sprite);
    int    RESDATA_SoundObject_GetTextLength(void* sprite);
    void   RESDATA_SetPosition(void* obj, int x, int y);
    void*  RESDATA_CreateChildSprite(void* parent, void* res, int x, int y);
    /* RESDATA (shared/types.h, sizeof == 0x1D8) forward-declared via
     * graphics/LOCOBITMAP.h, included above through UIPANEL.h. Real defs:
     * resources/ResDataSave.cpp. */
    void   RESMGR_ResourceData_Init(RESDATA* data);
    void   RESMGR_ReleaseResource(RESDATA* data);
    bool   RESMGR_IsSaveHeader(RESDATA* data);
    int8_t RESMGR_LoadResource(RESDATA* res, const char* path);
    class InputMgr;
   void   INPUT_SaveCurrentWorld(InputMgr* input, const char* name);
    void   Game_SetScreenMode(void* game, char a, char b, char c);
    void   TileMap_InvalidateRect(void* tilemap, int left, int top, int right, int bottom);
    void   TileMap_InvalidateDirtyRects(void* tilemap, char flag);
    void   UI_CenterWindow(int* a, int* b);
    void*  UI_CreateChildWindow(void* obj, int parent, int title);
    size_t ChildWindow_Size();  /* ui/UI_ChildWindow.cpp — real sizeof(ChildWindow) */

    /* String / CRT */
    char*  _strncpy(char* dst, const char* src, int max);
    char*  _strncat(char* dst, const char* src, int max);
    void*  CRT_malloc(void* ptr, size_t size);
    void   CRT_free(void* ptr);
    void   CRT_exit(void* a, void* b);

#ifdef _WIN32
    /* CRT_wcsstr and the file-enumeration trio below are only real on
     * Windows -- see UIPANEL_DrawEditField's #ifdef _WIN32/#else split
     * for why the host build uses std::filesystem/strcasecmp instead of
     * these directly. Signature matches the canonical real implementation
     * in shared/stubs_link001_batch1_crt_win32.cpp (real _stricmp/
     * strcasecmp semantics, int32_t so ordering comparisons like the
     * `< 0` below work correctly) -- this file previously declared it as
     * `int CRT_wcsstr(void*, void*)`, matching neither that real
     * implementation's parameter types nor (before a 2026-08-16 fix) its
     * signed return type. */
    int32_t CRT_wcsstr(uint8_t* a, uint8_t* b);

    /* File system */
    HANDLE CRT_FindFirstFile(LPCSTR path, void* data);
    int    CRT_FindNextFile(HANDLE h, void* data);
    void   CRT_FindClose(HANDLE h);
#endif

    /* Resource manager */
    class ResourceManager;
    extern ResourceManager g_resmgr;      /* 0x4855E8 — object, not a pointer (was void*
                                            * with a stale "0x4FD228" address comment —
                                            * confirmed via the single real definition in
                                            * resources/ResourceManager.cpp that this
                                            * always linked to the same object regardless;
                                            * a widespread cross-TU landmine, see
                                            * PROGRESS.md's g_resmgr sweep) */
    extern char   g_install_path[];        /* 0x4852B8 */
    extern char   g_empty_string;          /* 0x476934 */
    extern void*  g_game;                 /* 0x4FD144 */
    extern void*  g_tilemap;              /* 0x4FD244 */
    extern int32_t g_player_id;           /* 0x4AAD46 (see world/tilemap.h) */
    extern int32_t g_player_color;        /* 0x4AAD48 (see world/tilemap.h) */
    extern InputMgr g_input_mgr;            /* 0x4A9990 — static InputMgr object */

    /* EditWindow references */
    extern int    g_world_width;           /* 0x4AAD0C (TileMap.width; the old
                                             0x4FD3D8 comment was a mislabel) */
    extern int    g_world_height;          /* 0x4AAD10 (TileMap.height; the old
                                             0x4FD3DC comment was a mislabel —
                                             that address is
                                             g_allow_building_placement) */

    /* TrackPiece helpers */
    /* CGWND_TrackPiece_SetZoom already declared elsewhere */

    /* Town tile rendering */
    void   Town_BlitElement(void* self, uint32_t sx, uint32_t sy, int sw, uint32_t sh,
                            void* element, uint32_t cl, uint32_t ct, int cr, uint32_t cb,
                            uint32_t flags);

/* ================================================================== */
/* String constants referenced by these functions                      */
/* ================================================================== */

static const char s_savegame_dir[]     = "savegame\\";
static const char s_backdrop_dir[]     = "backdrop\\";
static const char s_savegame_ext[]     = "*.sav";
static const char s_backdrop_ext[]     = "*.bmp";
static const char s_savegame_prefix[]  = "savegame\\";
static const char s_backdrop_prefix[]  = "backdrop\\";
static const char s_dot_sav[]          = ".sav";
static const char s_dot_bmp[]          = ".bmp";

/* ================================================================== */
/* UIPANEL_DrawBorder                                                   */
/* Address: 0x428400                                                   */
/*                                                                     */
/* Draw panel border/background. If resource_ptr is non-NULL: loads    */
/* bitmap. If mode==5: loads the backdrop bitmap named by               */
/* resource_ptr->prefix. Otherwise: resource-backed button (save/BMP    */
/* preview). If resource_ptr or buffer exists: draws default button.    */
/*                                                                     */
/* resource_ptr is a SaveSprite* (its embedded `data` member's RESDATA  */
/* is what gets stored into self->save_header and read for preview     */
/* dimensions/pixels) -- confirmed via disassembly: the original adds   */
/* +0x50 to resource_ptr before both storing it at self+0x498 and       */
/* reading +0xB2/+0xB4/+0x1C4 off the stored value (RESDATA/SaveRegion  */
/* offsets, see shared/types.h).                                       */
/* ================================================================== */
void __thiscall UIPANEL_DrawBorder(UIPANEL* self, SaveSprite* resource_ptr)
{
    if (resource_ptr == nullptr || *(int*)((intptr_t)self + 0x490) != 0) {
        /* Clear and draw default button */
        self->save_header = nullptr;
        UIPANEL_SetClipRect((void*)((intptr_t)self + 0x478), 8, 0);
        UIPANEL_DrawButton((int)self);
    }

    if (resource_ptr != nullptr) {
        if (self->mode == 5) {
            /* Backdrop tab (mode 5): load the backdrop bitmap named by
             * resource_ptr->prefix ("backdrop\\<file>") -- NOT a "player
             * color picker" (the previous transcription's comment/globals
             * were both wrong: it read 0x47E8A0 as a literal bitmap path
             * handed straight to StretchBlit, when it is actually the
             * "%s%s" sprintf format string used to build the real path
             * here first; confirmed via disassembly of 0x4284DC-0x428536
             * plus a direct byte read of 0x47E8A0 == "%s%s\0"). The two
             * globals are g_player_id/g_player_color (0x4AAD46/0x4AAD48,
             * already declared in world/tilemap.h et al.), not the stale
             * 0x4AAAAA/0x4AAAAC the previous comment claimed -- reused
             * here as generic width/height locals by this call, same as
             * every other real caller of UIPANEL_InitSurface/StretchBlit
             * in this file. The real call is CRT_sprintf_buf (0x466D60,
             * a CRT vsprintf variant), not wsprintfA -- substituted
             * deliberately: both are behaviorally identical for a plain
             * two-%s concatenation with bounded inputs (g_install_path is
             * a fixed game-install prefix; resource_ptr->prefix is a
             * fixed char[0x41]), and wsprintfA is already declared/used
             * for the identical "%s%s" pattern elsewhere in this file
             * (InitSprite/BlitSprite). */
            self->save_header = nullptr;
            char backdrop_path[264];
            wsprintfA(backdrop_path, "%s%s", g_install_path, resource_ptr->prefix);
            UIPANEL_InitSurface((void*)((intptr_t)self + 0x478),
                                g_player_id, g_player_color, 1, 0, 0);
            UIPANEL_StretchBlit((void*)((intptr_t)self + 0x478),
                                backdrop_path, 0, g_player_id, g_player_color);
        } else {
            /* Resource-backed button */
            if (!RESMGR_IsSaveHeader(&resource_ptr->data)) {
                return;
            }

            self->save_header = &resource_ptr->data;

            UIPANEL_InitSurface((void*)((intptr_t)self + 0x478),
                                resource_ptr->data.save.player_id,
                                resource_ptr->data.save.player_color,
                                0, 0, 8);

            RESDATA* save_header = self->save_header;
            if (save_header->save_pixels != nullptr) {
                /* Copy pixel data from save header to surface buffer */
                uint32_t pixel_count = static_cast<uint32_t>(save_header->save.player_color) *
                                      static_cast<uint32_t>(save_header->save.player_id);
                void* src_pixels = save_header->save_pixels;
                void* dst_pixels = *(uintptr_t**)((intptr_t)self + 0x490);
                memcpy(dst_pixels, src_pixels, pixel_count);

                UIPANEL_DrawButton((int)self);
                return;
            }
        }
        UIPANEL_DrawButton((int)self);
    }
}

/* ================================================================== */
/* UIPANEL_DrawButton — Draw centred panel button                       */
/* Address: 0x428550                                                   */
/*                                                                     */
/* Draws a centred button on the UIPANEL surface. Small buttons        */
/* (50/64px wide) drawn at 2x, normal (72/80px) at 1x. Blits via      */
/* Town_BlitElement then calls TileMap_InvalidateRect.                  */
/* ================================================================== */
void __fastcall UIPANEL_DrawButton(int param_1)
{
    RECT btn_rect, src_rect, dest_rect;

    /* Button rect in panel coordinates */
    SetRect(&btn_rect, 0x0F, 0x1A, 0x8E, 0x79);

    /* Source rect from surface dimensions */
    int surf_w = *(int*)(uintptr_t)(param_1 + 0x480);  /* surface width at +0x480 */
    int surf_h = *(int*)(uintptr_t)(param_1 + 0x484);
    SetRect(&src_rect, 0, 0, surf_w, surf_h);

    /* Determine scaling based on width */
    uint32_t flags;
    int element_surf = *(int*)(uintptr_t)(*(int*)(uintptr_t)(param_1 + 0x40) + 0x10);

    switch (surf_w) {
    case 0x32:  /* 50px */
    case 0x40:  /* 64px */
        SetRect(&dest_rect, 0, 0, surf_w * 2, surf_h * 2);
        UI_CenterWindow(&btn_rect.left, &dest_rect.left);
        flags = 4;
        break;

    case 0x48:  /* 72px */
    case 0x50:  /* 80px */
        SetRect(&dest_rect, 0, 0, surf_w, surf_h);
        UI_CenterWindow(&btn_rect.left, &dest_rect.left);
        flags = 0;
        break;

    default:
        /* Unknown size, just invalidate */
        TileMap_InvalidateRect(g_tilemap,
            *(int*)(uintptr_t)(param_1 + 8), *(int*)(uintptr_t)(param_1 + 0xC),
            *(int*)(uintptr_t)(param_1 + 0x10), *(int*)(uintptr_t)(param_1 + 0x14));
        return;
    }

    Town_BlitElement((void*)(uintptr_t)(param_1 + 0x478),
                     dest_rect.left, dest_rect.top,
                     dest_rect.right, dest_rect.bottom,
                     (void*)(uintptr_t)element_surf,
                     src_rect.left, src_rect.top,
                     src_rect.right, src_rect.bottom,
                     flags);

    /* Invalidate the panel rect */
    TileMap_InvalidateRect(g_tilemap,
        *(int*)(uintptr_t)(param_1 + 8), *(int*)(uintptr_t)(param_1 + 0xC),
        *(int*)(uintptr_t)(param_1 + 0x10), *(int*)(uintptr_t)(param_1 + 0x14));
}

/* ================================================================== */
/* UIPANEL_GetButtonState — Hit-test a button by position               */
/* Address: 0x428770                                                   */
/*                                                                     */
/* Sets panel world position then dispatches to embedded GameObject's  */
/* vtable[3] (HitTest) with adjusted coordinates (adding button offset */
/* from the child sprite at +0x430).                                    */
/* ================================================================== */
void __thiscall UIPANEL_GetButtonState(void* self, int pos_x, int pos_y)
{
    RESDATA_SetPosition(self, pos_x, pos_y);

    /* Dispatch HitTest on embedded GO with adjusted coordinates */
    void* child_go = (void*)((intptr_t)self + 0x3F0);
    void* button = *(uintptr_t**)((intptr_t)self + 0x430);

    int adjusted_x = *(short*)((intptr_t)button + 0x2E) + pos_x;
    int adjusted_y = *(short*)((intptr_t)button + 0x30) + pos_y;

    typedef void (__thiscall* HitTestFunc)(void* self, int x, int y);
    HitTestFunc hitTest = (HitTestFunc)(*(uintptr_t**)child_go)[3];
    hitTest(child_go, adjusted_x, adjusted_y);
}

/* ================================================================== */
/* UIPANEL_SetButtonState — Set button state on click                   */
/* Address: 0x4287B0                                                   */
/*                                                                     */
/* Validates entity visibility and dispatches by resource type.         */
/* Cases: zoom changes, sound object text sync, color picker.           */
/* Returns 0 on failure, 1 on success (upper byte or full byte).        */
/* ================================================================== */
uint32_t __thiscall UIPANEL_SetButtonState(void* self, void* entity,
    uint32_t param_2, uint32_t param_3)
{
    if (entity == NULL || *(char*)((intptr_t)entity + 0x56) == '\0') {
        return 0;
    }

    /* Call vtable[2] on entity (PtInRect/HitTest) */
    typedef uint32_t (__thiscall* HitTestFunc)(void* self, uint32_t a, uint32_t b);
    HitTestFunc hitTest = (HitTestFunc)(*(uintptr_t**)entity)[2];
    uint32_t result = hitTest(entity, param_2, param_3);
    if ((char)result == '\0') {
        return 0;
    }

    uint32_t res_type = *(int*)(uintptr_t)(*(int*)(uintptr_t)((intptr_t)entity + 0x44) + 4);
    uint16_t panel_mode = *(uint16_t*)((intptr_t)self + 0x49C);

    switch (res_type) {
    case 0x2C02:
        /* Tab 0: Buildings */
        if (panel_mode != 1 && panel_mode != 2) {
            return 0;
        }
        break;

    case 0x2C03:
        /* Tab 1: People */
        if (panel_mode != 1 && panel_mode != 3) {
            return 0;
        }
        break;

    case 0x2C04:
        /* Content background */
    case 0x2C0C:
        /* Tab 3: Scenery */
        if (panel_mode != 1 && panel_mode != 4 && panel_mode != 5) {
            return 0;
        }
        break;

    case 0x2C05:
        /* Tab 2: Vehicles */
        if (panel_mode != 1 && panel_mode != 4) {
            return 0;
        }
        break;

    case 0x2C07:
    case 0x2C08:
        /* List background / scrollbar */
        if (panel_mode >= 2 && panel_mode <= 5 &&
            *(short*)((intptr_t)entity + 0x48) == 1) {
            CGWND_TrackPiece_SetZoom(entity, 2);
            *(uint16_t*)((intptr_t)entity + 0x54) = 6;
            return 1;
        }
        return 0;

    case 0x2C09:
        /* Sound button */
        if ((panel_mode == 2 || panel_mode == 3 || panel_mode == 4 || panel_mode == 5) &&
            entity != *(uintptr_t**)((intptr_t)self + 0x4BC)) {
            /* Sync text from clicked item to sound button */
            char* state = RESDATA_SoundObject_GetState(entity);
            RESDATA_SoundObject_Init(*(uintptr_t**)((intptr_t)self + 0x4BC), state);

            if (RESDATA_SoundObject_GetTextLength(*(uintptr_t**)((intptr_t)self + 0x4BC)) == 0 &&
                *(int*)((intptr_t)self + 0x490) != 0) {
                UIPANEL_DrawBorder(static_cast<UIPANEL*>(self), nullptr);
            }

            /* Trigger redraw */
            typedef void (__thiscall* DrawFunc)(void* self);
            DrawFunc draw = (DrawFunc)(*(uintptr_t**)(*(uintptr_t**)((intptr_t)self + 0x4BC)))[0x20/4];
            draw(*(uintptr_t**)((intptr_t)self + 0x4BC));
            return 1;
        }
        return 0;

    case 0x2C0A: /* 8 in zero-based, 10 in switch */
        /* Color swatch in scenery/depot tab */
        if (panel_mode == 5) {
            if (*(short*)((intptr_t)entity + 0x48) == 1) {
                CGWND_TrackPiece_SetZoom(entity, 2);
                *(uint16_t*)((intptr_t)entity + 0x54) = 6;
            }
            return 1;
        }
        return 0;

    default:
        return 0;
    }

    /* Tab selection: set zoom to 2 */
    if (*(short*)((intptr_t)entity + 0x48) == 1) {
        CGWND_TrackPiece_SetZoom(entity, 2);
        *(uint16_t*)((intptr_t)entity + 0x54) = 6;
    }
    return 1;
}

/* ================================================================== */
/* UIPANEL_DrawCheckbox — Hit-test checkbox-style button                */
/* Address: 0x4289A0                                                   */
/*                                                                     */
/* If hit passes and panel is in edit mode (state==1), translate the   */
/* button's resource ID (0x2C02-0x2C05) into dispatch value (1-4) and  */
/* call vtable slot [7] on the embedded GameObject (+0x3F0).           */
/* ================================================================== */
uint8_t __thiscall UIPANEL_DrawCheckbox(void* self, void* entity,
    uint32_t param_2, uint32_t param_3)
{
    if (entity == NULL) {
        return 0;
    }

    /* Call vtable[2] on entity for hit-test */
    typedef uint32_t (__thiscall* HitTestFunc)(void* self, uint32_t a, uint32_t b);
    HitTestFunc hitTest = (HitTestFunc)(*(uintptr_t**)entity)[2];
    if ((char)hitTest(entity, param_2, param_3) == '\0') {
        return 0;
    }

    /* Only active in edit mode */
    if (*(uint16_t*)((intptr_t)self + 0x49C) != 1) {
        return 0;
    }

    uint32_t res_id = *(uint32_t*)(uintptr_t)(*(int*)(uintptr_t)((intptr_t)entity + 0x44) + 4);

    uint32_t dispatch_val;
    switch (res_id) {
    case 0x2C02: dispatch_val = 1; break;
    case 0x2C03: dispatch_val = 2; break;
    case 0x2C04: dispatch_val = 4; break;
    case 0x2C05: dispatch_val = 3; break;
    default: return 0;
    }

    /* Call vtable[7] on embedded GameObject */
    void* child_go = (void*)((intptr_t)self + 0x3F0);
    typedef void (__thiscall* DispatchFunc)(void* self, uint32_t val);
    DispatchFunc dispatch = (DispatchFunc)(*(uintptr_t**)child_go)[7];
    dispatch(child_go, dispatch_val);

    return 1;
}

/* ================================================================== */
/* UIPANEL_DrawRadioButton — Radio-button selection state machine       */
/* Address: 0x428F90                                                   */
/*                                                                     */
/* Compares state strings via RESDATA_SoundObject_GetState. If the     */
/* clicked item differs from the currently selected sound button:      */
/*   - Unzooms old selection if zoom=2 (sets back to 1)                */
/*   - Zooms clicked item if zoom=1 (sets to 2)                        */
/*   - Redraws border if tracking same resource                        */
/* ================================================================== */
void __thiscall UIPANEL_DrawRadioButton(UIPANEL* self, void* entity)
{
    void* current_sound = *(uintptr_t**)((intptr_t)self + 0x4BC);  /* +0x4BC: sound_btn_sprite */

    if (entity == current_sound) {
        return;
    }

    if (*(short*)((intptr_t)entity + 0x48) == 3) {
        return;  /* Zoom == 3 means hidden/disabled */
    }

    int text_len = RESDATA_SoundObject_GetTextLength(current_sound);
    if (text_len == 0) {
        return;
    }

    /* Compare state strings between current and clicked entity */
    char* current_state = RESDATA_SoundObject_GetState(current_sound);
    char* entity_state  = RESDATA_SoundObject_GetState(entity);

    int cmp_result = strcmp((const char*)entity_state, (const char*)current_state);

    if (cmp_result != 0 && *(short*)((intptr_t)entity + 0x48) == 2) {
        /* Clicked a different item that's zoomed in — unzoom it. entity's
         * own +0x30 field (on that CGWND/TrackPiece-family entity's
         * as-yet-unmodeled class) holds a SaveSprite* -- the file entry
         * bound to that content slot (see UIPANEL::CreateSprite, which
         * writes it). */
        SaveSprite* target = *(SaveSprite**)((intptr_t)entity + 0x30);
        if (*(int*)((intptr_t)self + 0x490) != 0 &&
            self->save_header == &target->data) {
            UIPANEL_DrawBorder(self, nullptr);
        }
        CGWND_TrackPiece_SetZoom(entity, 1);
    }

    /* Re-compare (state may have changed) */
    current_state = RESDATA_SoundObject_GetState(current_sound);
    entity_state  = RESDATA_SoundObject_GetState(entity);
    cmp_result = strcmp((const char*)entity_state, (const char*)current_state);

    if (cmp_result == 0 && *(short*)((intptr_t)entity + 0x48) == 1) {
        /* Same item, zoom in */
        CGWND_TrackPiece_SetZoom(entity, 2);
        UIPANEL_DrawBorder(self, *(SaveSprite**)((intptr_t)entity + 0x30));
    }
}

/* ================================================================== */
/* SaveSprite::SaveSprite / SaveSprite::~SaveSprite                     */
/* Addresses: construction body inlined at 0x42964A-0x4296E7 (within    */
/* UIPANEL_DrawEditField, 0x429490); destructor at 0x429830             */
/* (UIPANEL_DtorSprite) / body at 0x429820 (UIPANEL_FreeSprite).        */
/* See ui/SaveSprite.h for the full field-layout evidence trail.        */
/* ================================================================== */
SaveSprite::SaveSprite(const char* basename, const char* path_prefix,
                        const char* original_filename)
    : prev(nullptr), next(nullptr)
{
    RESMGR_ResourceData_Init(&data);

    /* name[11]: strncpy bounded to 10 chars, guaranteed extra NUL. */
    name[0] = '\0';
    strncpy(name, basename, 10);
    name[10] = '\0';

    /* prefix[0x41]: "savegame\\"/"backdrop\\" + original filename,
     * strncat bounded to 0x40 bytes, guaranteed extra NUL. */
    prefix[0] = '\0';
    strcpy(prefix, path_prefix);
    strncat(prefix, original_filename,
            sizeof(prefix) - strlen(prefix) - 1);
    prefix[sizeof(prefix) - 1] = '\0';
}

SaveSprite::~SaveSprite()
{
    RESMGR_ReleaseResource(&data);
}

/* ================================================================== */
/* UIPANEL_DrawEditField — Enumerate savegame/backdrop files            */
/* Address: 0x429490                                                   */
/*                                                                     */
/* Enumerates files via CRT_FindFirstFile/CRT_FindNextFile (CRT          */
/* _findfirst/_findnext wrappers, NOT raw Win32 FindFirstFileA) to build */
/* a sorted doubly-linked list of SaveSprite entries at                 */
/* self->sprite_list_head, alphabetically ordered (case-insensitive,    */
/* see ui/SaveSprite.h).                                                */
/* Panel mode at +0x49C: 5 = backdrop (*.bmp) files in backdrop\\,      */
/* else = savegame (*.sav) files in savegame\\.                        */
/*                                                                     */
/* Called by: HandleDrag (tab switch), BlitSprite (save),              */
/*            BlitSpriteEx (delete)                                     */
/* ================================================================== */
/* Inserts a newly constructed SaveSprite into *sprite_list_ptr in
 * case-insensitive alphabetical order and links it via the real
 * ->next/->prev fields (see ui/SaveSprite.h for the evidence trail on
 * both the sort direction and the comparator semantics).
 *
 * The comparator itself must differ by platform: the original binary's
 * CRT_wcsstr (0x471480) really is an inlined case-fold _stricmp on
 * Windows, but this codebase's current host stub
 * (shared/stubs_impl.cpp) only implements substring containment and
 * always returns 0 or 1 -- never negative -- which would make the
 * `< 0` loop condition below permanently false and silently break
 * sorting for every insertion (each new entry would land at the head
 * instead of its sorted position). Call the real CRT_wcsstr only on
 * Windows; use a real case-insensitive compare on host instead of
 * routing through that known-broken shared stub. */
static void UIPANEL_InsertSpriteSorted(SaveSprite** sprite_list_ptr,
                                        const char* filename_buf,
                                        int panel_mode,
                                        const char* real_filename)
{
    SaveSprite* sprite_mem = new SaveSprite(
        filename_buf,
        (panel_mode == 5) ? s_backdrop_prefix : s_savegame_prefix,
        real_filename);

    SaveSprite** insert_pos = sprite_list_ptr;
    SaveSprite* list_node = *sprite_list_ptr;

    while (list_node != NULL &&
#ifdef _WIN32
           CRT_wcsstr(reinterpret_cast<uint8_t*>(list_node->name),
                      reinterpret_cast<uint8_t*>(sprite_mem->name)) < 0) {
#else
           strcasecmp(list_node->name, sprite_mem->name) < 0) {
#endif
        insert_pos = &list_node->next;
        list_node = list_node->next;
    }

    sprite_mem->next = list_node;
    sprite_mem->prev = *insert_pos;
    *insert_pos = sprite_mem;

    if (list_node != NULL) {
        list_node->prev = sprite_mem;
    }
}

uint32_t __fastcall UIPANEL_DrawEditField(UIPANEL* self)
{
    /* Stack SEH setup omitted */

    char path_buf[260];
    char filename_buf[260];
    uint16_t panel_mode = self->mode;

    /* Original binary constructs a full stack-local SaveSprite here
     * purely as RAII scratch space around the enumeration/insertion loop
     * (its vtable ptr is written at entry and re-written at exit; its
     * embedded RESDATA is inited/released; its own next/prev slots are
     * never linked into the real list here). Modeled as a plain local
     * RESDATA, which is all this function actually observes. */
    RESDATA local_data;
    memset(&local_data, 0, sizeof(local_data));
    RESMGR_ResourceData_Init(&local_data);

    /* Destroy any existing sprite list */
    SaveSprite* current = self->sprite_list_head;
    while (current != NULL) {
        SaveSprite* next_node = current->next;
        self->sprite_list_head = next_node;
        delete current;
        current = self->sprite_list_head;
    }

    SaveSprite* sprite_list = self->sprite_list_head;

    /* Build path: <install_path> + "savegame\\" or "backdrop\\" */
    memset(path_buf, 0, sizeof(path_buf));
    strcpy(path_buf, g_install_path);
    /* Find end of install path */
    int path_end = strlen(path_buf);

    strcpy(path_buf + path_end,
           (panel_mode == 5) ? s_backdrop_dir : s_savegame_dir);

    /* Append wildcard: "*.sav" or "*.bmp" */
    int dir_end = strlen(path_buf);
    strcpy(path_buf + dir_end,
           (panel_mode == 5) ? s_backdrop_ext : s_savegame_ext);

#ifdef _WIN32
    /* Try to find first file. Ground truth calls the CRT `_findfirst`/
     * `_findnext` wrappers (CRT_FindFirstFile/CRT_FindNextFile, 0x467A20/
     * 0x467B50 -- already declared above, confirmed via disassembly:
     * those two addresses, not raw FindFirstFileA/FindNextFileA), which
     * internally call the Win32 API but repack the result into the real
     * CRT `_finddata_t` layout (attrib+3 time_t+size = 0x14 bytes, THEN
     * name[260] -- not Win32 WIN32_FIND_DATAA's layout, where the name is
     * at +0x2C). CRT_FindNextFile also returns 0 on SUCCESS (opposite of
     * Win32 FindNextFileA's BOOL convention) -- the loop below continues
     * while it returns 0, not "!= 0". */
    struct CRT_FindDataT {
        uint32_t attrib;         /* +0x00 */
        uint32_t time_create;    /* +0x04 */
        uint32_t time_access;    /* +0x08 */
        uint32_t time_write;     /* +0x0C */
        uint32_t size;           /* +0x10 */
        char     name[260];      /* +0x14 */
    };
    CRT_FindDataT find_data;
    HANDLE hFind = CRT_FindFirstFile(path_buf, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        /* Directory may not exist — try creating it */
        memset(path_buf, 0, sizeof(path_buf));
        strcpy(path_buf, g_install_path);
        path_end = strlen(path_buf);
        strcpy(path_buf + path_end,
               (panel_mode == 5) ? s_backdrop_dir : s_savegame_dir);

        /* Remove trailing backslash and create */
        int len = strlen(path_buf);
        if (len > 0 && path_buf[len-1] == '\\') {
            path_buf[len-1] = '\0';
        }
        CreateDirectoryA(path_buf, NULL);
    } else {
        /* Iterate files */
        do {
            /* Skip "." and ".." */
            if (find_data.name[0] == '.') {
                continue;
            }

            /* Build filename without extension */
            strcpy(filename_buf, find_data.name);
            /* Remove extension */
            char* dot = strrchr(filename_buf, '.');
            if (dot != NULL) {
                *dot = '\0';
            }

            /* Accept basenames of length <= 10 (confirmed via disassembly:
             * `CMP ECX,0xA` / `JA skip` rejects only length > 10 -- the
             * previous `>= 10` gate wrongly dropped exactly-10-char
             * basenames). */
            if (strlen(filename_buf) > 10) {
                continue;
            }

            UIPANEL_InsertSpriteSorted(&sprite_list, filename_buf, panel_mode, find_data.name);

        } while (CRT_FindNextFile(hFind, &find_data) == 0);

        CRT_FindClose(hFind);
    }
#else
    /* Host: the CRT `_findfirst`/`_findnext`/`_findclose` trio above has
     * no real implementation on this build (shared/defsym_stubs.cpp only
     * provides a void-returning link-time filler for symbols nobody was
     * expected to call) -- and unlike Windows, C++ free-function
     * mangling here ignores return type, so calling the HANDLE-returning
     * declaration above against that void-returning definition would
     * silently read an unspecified/garbage return value, i.e. real
     * undefined behavior, not just "always empty". This mirrors the
     * pattern already established for the same CRT trio in
     * network/PostBagCleanup.cpp's DPLAY_ReceiveMessage and
     * network/NetworkPlayerList.cpp's PostBag_Enumerate: keep the CRT
     * calls Windows-only and use std::filesystem here instead. */
    std::error_code ec;
    std::filesystem::path dir(g_install_path);
    dir /= (panel_mode == 5) ? "backdrop" : "savegame";
    const char* want_ext = (panel_mode == 5) ? ".bmp" : ".sav";

    if (!std::filesystem::is_directory(dir, ec)) {
        std::filesystem::create_directories(dir, ec);
    } else {
        for (std::filesystem::directory_iterator it(dir, ec), end;
             !ec && it != end; it.increment(ec)) {
            std::error_code type_ec;
            if (it->is_directory(type_ec)) {
                continue;
            }

            std::string fname = it->path().filename().string();
            if (fname.empty() || fname.front() == '.') {
                continue;
            }

            std::string ext = it->path().extension().string();
            for (char& c : ext) {
                c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            }
            if (ext != want_ext) {
                continue;
            }

            std::string basename = it->path().stem().string();
            if (basename.size() > 10) {
                continue;
            }

            strcpy(filename_buf, basename.c_str());
            UIPANEL_InsertSpriteSorted(&sprite_list, filename_buf, panel_mode, fname.c_str());
        }
    }
#endif

    /* Store list head */
    self->sprite_list_head = sprite_list;
    if (sprite_list != NULL) {
        sprite_list->prev = NULL;  /* head has no earlier node */
    }

    /* Cleanup RESDATA on stack */
    RESMGR_ReleaseResource(&local_data);

    return 1;
}

/* ================================================================== */
/* UIPANEL_CreateSprite — Render sprite list onto 6 CGWND slots        */
/* Address: 0x429850                                                   */
/*                                                                     */
/* Iterates the linked list at +0x4D8, filling 6 content item slots    */
/* (+0x4C0..+0x4D7) with filenames. For each slot: sets text to        */
/* filename, loads resource if needed, sets zoom (1=normal, 3=far).    */
/* After loop, adjusts list_bg (+0x4B4) and list_text (+0x4B8) zoom   */
/* based on whether there are more items to scroll to.                 */
/* ================================================================== */
void __thiscall UIPANEL_CreateSprite(UIPANEL* self, SaveSprite* list_entry)
{
    char name_buf[260];
    memset(name_buf, 0, sizeof(name_buf));

    if (list_entry == NULL) {
        list_entry = self->sprite_list_head;
    }

    /* Store current scroll-window anchor (see UIPANEL.h's field comment --
     * this is NOT a true list tail). */
    self->sprite_list_tail = list_entry;

    void** item_slots = self->item_sprites;  /* item_sprites[6] */
    SaveSprite* current = list_entry;

    for (int i = 0; i < 6; i++) {
        /* Set target pointer in each sprite slot (that entity's own +0x30
         * field -- see ui/UIPANEL.h's item_sprites comment). */
        *(SaveSprite**)((intptr_t)item_slots[i] + 0x30) = current;

        if (current == NULL) {
            RESDATA_SoundObject_Init(item_slots[i], "");
            CGWND_TrackPiece_SetZoom(item_slots[i], 3);
        } else {
            RESDATA_SoundObject_Init(item_slots[i], current->name);

            /* Check if resource has been loaded: current+0x100 == data+0xB0
             * == &data.save.type (SaveRegion's first field; RESMGR_LoadResource
             * sets it to 8, RESMGR_ResourceData_Init leaves it zeroed). */
            if (current->data.save.type == 0) {
                /* Build full path: install_path + prefix + filename */
                memset(name_buf, 0, sizeof(name_buf));
                strcpy(name_buf, g_install_path);

                int path_end = strlen(name_buf);
                strcpy(name_buf + path_end, current->prefix);

                /* Load resource */
                RESMGR_LoadResource(&current->data, name_buf);
            }

            /* Determine zoom based on panel mode */
            uint16_t panel_mode = self->mode;
            if (panel_mode == 3 || panel_mode == 4 || panel_mode == 5) {
                CGWND_TrackPiece_SetZoom(item_slots[i], 1);
            } else {
                if (RESMGR_IsSaveHeader(&current->data)) {
                    CGWND_TrackPiece_SetZoom(item_slots[i], 1);
                } else {
                    CGWND_TrackPiece_SetZoom(item_slots[i], 3);
                }
            }

            current = current->next;
        }

        /* Redraw the slot sprite */
        typedef void (__thiscall* DrawFunc)(void* self);
        DrawFunc draw = (DrawFunc)(*(uintptr_t**)item_slots[i])[0x20/4];
        draw(item_slots[i]);
    }

    /* Update scroll indicators. Ground truth (disassembly of 0x429850) has
     * exactly ONE condition here -- `anchor != NULL && anchor->prev !=
     * NULL` -- not an OR of separate has_next/has_prev checks; a fabricated
     * has_prev term was removed (it did not exist in the original binary). */
    SaveSprite* anchor = self->sprite_list_tail;
    short bg_zoom = (anchor != NULL && anchor->prev != NULL) ? 1 : 3;
    CGWND_TrackPiece_SetZoom(self->list_bg_sprite, bg_zoom);

    /* +0x4D4 is item_sprites[5] (0x4C0 + 5*4), the LAST of the 6 content
     * slots -- not a separate field. Its text length nonzero means the
     * last slot is occupied, i.e. more entries exist past this page. */
    int text_len = RESDATA_SoundObject_GetTextLength(self->item_sprites[5]);
    short text_zoom = (text_len != 0) ? 1 : 3;
    CGWND_TrackPiece_SetZoom(self->list_text_sprite, text_zoom);

    /* Redraw scroll indicators */
    typedef void (__thiscall* DrawFunc)(void* self);
    DrawFunc draw = (DrawFunc)(*(uintptr_t**)(*(uintptr_t**)((intptr_t)self + 0x4B4)))[0x20/4];
    draw(*(uintptr_t**)((intptr_t)self + 0x4B4));

    draw = (DrawFunc)(*(uintptr_t**)(*(uintptr_t**)((intptr_t)self + 0x4B8)))[0x20/4];
    draw(*(uintptr_t**)((intptr_t)self + 0x4B8));

    /* Redraw sound button */
    draw = (DrawFunc)(*(uintptr_t**)(*(uintptr_t**)((intptr_t)self + 0x4BC)))[0x20/4];
    draw(*(uintptr_t**)((intptr_t)self + 0x4BC));
}

/* ================================================================== */
/* UIPANEL_InitSprite — Load saved sprite/world from file               */
/* Address: 0x429A10                                                   */
/*                                                                     */
/* Builds path "<install>savegame\\<NAME>.bmp" from the sound button   */
/* state text (+0x4BC), creates directory if needed, loads world via   */
/* INPUT_LoadWorld. Guarded by directory existence check.               */
/* ================================================================== */
void __fastcall UIPANEL_InitSprite(UIPANEL* self)
{
    char probe_buf[260];

    /* Build savegame directory path (this probe genuinely uses a stack
     * buffer in the original -- confirmed via disassembly of 0x429A10). */
    wsprintfA(probe_buf, "%s%s", g_install_path, s_savegame_dir);

    /* Create directory if it doesn't exist */
    DWORD attrs = GetFileAttributesA(probe_buf);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryA(probe_buf, NULL);
    }

    /* Build full save path: install_path + savegame\\ + name.bmp, into the
     * panel's own +0x2EA member buffer (confirmed via disassembly:
     * `LEA EBX,[EBP+0x2ea]` -- NOT a stack local, unlike the probe above). */
    memset(self->save_path_buf, 0, sizeof(self->save_path_buf));
    strcpy(self->save_path_buf, g_install_path);
    strcat(self->save_path_buf, s_savegame_prefix);

    /* Get filename from sound button state */
    char* name = RESDATA_SoundObject_GetState(self->sound_btn_sprite);
    strcat(self->save_path_buf, name);
    strcat(self->save_path_buf, s_dot_bmp);

    /* Load the world */
    Game_SetScreenMode(g_game, 1, 1, 1);
    TileMap_InvalidateDirtyRects(g_tilemap, 0);
    INPUT_SaveCurrentWorld(&g_input_mgr, self->save_path_buf);
    Game_SetScreenMode(g_game, 1, 1, 0);
}

/* ================================================================== */
/* UIPANEL_BlitSprite — Save current world as sprite file               */
/* Address: 0x429B20                                                   */
/*                                                                     */
/* Saves the current world as a sprite file, rebuilds the sprite list,  */
/* then searches through the sorted list to find the matching entry     */
/* and centers on it via repeated CreateSprite calls (scrolling).       */
/* ================================================================== */
/* NOTE on the loop structure below: ground truth (disassembly of
 * 0x429B20) computes `cmp = strcmp(first_state, saved_state)` --
 * FIRST minus SAVED, not the other way round -- so cmp>0 means the
 * first-displayed item sorts AFTER the saved name (scroll toward
 * earlier entries, ->prev) and cmp<0 means scroll toward later entries
 * (->next). The two loops are also structurally asymmetric in the
 * original: the backward loop checks `prev == NULL` before recursing;
 * the forward loop has no such check on `next` (it relies on
 * UIPANEL_CreateSprite's own "NULL list_entry defaults to
 * sprite_list_head" behavior) but DOES gate continuation on the last
 * slot (item_sprites[5]) still having text. Preserved exactly, not
 * symmetrized. */
void __fastcall UIPANEL_BlitSprite(UIPANEL* self)
{
    /* Check if there's a name to save under */
    if (RESDATA_SoundObject_GetTextLength(self->sound_btn_sprite) == 0) {
        return;
    }

    char probe_buf[260];

    /* Build savegame directory path (genuine stack buffer -- only this
     * first, directory-existence probe uses one). */
    wsprintfA(probe_buf, "%s%s", g_install_path, s_savegame_dir);
    DWORD attrs = GetFileAttributesA(probe_buf);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryA(probe_buf, NULL);
    }

    /* Build full save path into the panel's own +0x2EA member buffer
     * (confirmed via disassembly: `LEA EBX,[EBP+0x2ea]`). */
    memset(self->save_path_buf, 0, sizeof(self->save_path_buf));
    strcpy(self->save_path_buf, g_install_path);
    strcat(self->save_path_buf, s_savegame_prefix);

    char* name = RESDATA_SoundObject_GetState(self->sound_btn_sprite);
    strcat(self->save_path_buf, name);
    strcat(self->save_path_buf, s_dot_bmp);

    /* Save the current world */
    Game_SetScreenMode(g_game, 1, 1, 1);

    /* Destroy existing sprite list */
    SaveSprite* current = self->sprite_list_head;
    while (current != NULL) {
        SaveSprite* next = current->next;
        self->sprite_list_head = next;
        delete current;
        current = self->sprite_list_head;
    }

    /* Save world */
    INPUT_SaveCurrentWorld(&g_input_mgr, self->save_path_buf);

    /* Rebuild file list and center on saved entry */
    UIPANEL_DrawEditField(self);
    UIPANEL_CreateSprite(self, self->sprite_list_head);

    /* Scroll to find the saved entry by comparing state strings
     * (case-sensitive strcmp(first, saved) -- see note above). */
    char* first_state = RESDATA_SoundObject_GetState(self->item_sprites[0]);
    char* saved_state = RESDATA_SoundObject_GetState(self->sound_btn_sprite);
    int cmp = strcmp((const char*)first_state, (const char*)saved_state);

    /* Scroll toward earlier entries while first > saved */
    while (cmp > 0) {
        SaveSprite* prevAnchor = self->sprite_list_tail->prev;
        if (prevAnchor == NULL) break;
        UIPANEL_CreateSprite(self, prevAnchor);
        first_state = RESDATA_SoundObject_GetState(self->item_sprites[0]);
        saved_state = RESDATA_SoundObject_GetState(self->sound_btn_sprite);
        cmp = strcmp((const char*)first_state, (const char*)saved_state);
    }

    /* Scroll toward later entries while first < saved, as long as the
     * last content slot still shows an entry (more to scroll to). */
    while (cmp < 0 &&
           RESDATA_SoundObject_GetTextLength(self->item_sprites[5]) != 0) {
        SaveSprite* nextAnchor = self->sprite_list_tail->next;  /* may be NULL:
             * CreateSprite(NULL) defaults back to sprite_list_head, matching
             * the original's unchecked dereference here. */
        UIPANEL_CreateSprite(self, nextAnchor);
        first_state = RESDATA_SoundObject_GetState(self->item_sprites[0]);
        saved_state = RESDATA_SoundObject_GetState(self->sound_btn_sprite);
        cmp = strcmp((const char*)first_state, (const char*)saved_state);
    }

    Game_SetScreenMode(g_game, 1, 1, 0);
}

/* ================================================================== */
/* UIPANEL_BlitSpriteEx — Extended blit: delete save file and recreate  */
/* Address: 0x429DD0                                                   */
/*                                                                     */
/* Deletes the current save file, rebuilds the sprite list, clears     */
/* the name text if delete succeeded, then calls CreateSprite.         */
/* ================================================================== */
void __fastcall UIPANEL_BlitSpriteEx(UIPANEL* self)
{
    /* Build full file path into the panel's own +0x2EA member buffer
     * (confirmed via disassembly: `LEA EBX,[EBP+0x2ea]`; no directory
     * probe in this function, unlike BlitSprite/InitSprite). */
    memset(self->save_path_buf, 0, sizeof(self->save_path_buf));
    strcpy(self->save_path_buf, g_install_path);
    strcat(self->save_path_buf, s_savegame_prefix);

    char* name = RESDATA_SoundObject_GetState(self->sound_btn_sprite);
    strcat(self->save_path_buf, name);
    strcat(self->save_path_buf, s_dot_bmp);

    /* Destroy existing sprite list */
    SaveSprite* current = self->sprite_list_head;
    while (current != NULL) {
        SaveSprite* next = current->next;
        self->sprite_list_head = next;
        delete current;
        current = self->sprite_list_head;
    }

    /* Delete the file */
    BOOL deleted = DeleteFileA(self->save_path_buf);

    /* Rebuild file list */
    UIPANEL_DrawEditField(self);

    /* Ground truth: on delete FAILURE, calls GetLastError() and returns --
     * no SoundObject_Init/CreateSprite refresh (the previous transcription
     * called CreateSprite unconditionally on this path; fixed). */
    if (deleted != 1) {
        GetLastError();
        return;
    }

    /* Clear the sound button text */
    RESDATA_SoundObject_Init(self->sound_btn_sprite, &g_empty_string);

    /* Refresh display */
    UIPANEL_CreateSprite(self, self->sprite_list_head);
}

/* ================================================================== */
/* UIPANEL_Hide — Create/replace backdrop overlay surface               */
/* Address: 0x429EF0                                                   */
/*                                                                     */
/* MISNAMED: This shows/creates the backdrop (hides the game world).    */
/* Builds "backdrop\\<filename>" path, creates a child window with that */
/* surface, and fills it with a dithered pattern. Falls back to        */
/* resource 0x400 if the backdrop file can't be loaded.                */
/* ================================================================== */
void __thiscall UIPANEL_Hide(void* self, const char* filename)
{
    /* Free existing cursor surface if any */
    int** g_cursor_surf = (int**)0x4FD3CC;
    if (*g_cursor_surf != NULL) {
        typedef void (__thiscall* ReleaseFunc)(void* self);
        ReleaseFunc rel = (ReleaseFunc)(*(uintptr_t**)*g_cursor_surf)[2];
        rel(*g_cursor_surf);

        if ((*g_cursor_surf)[1] == -1 && *g_cursor_surf != NULL) {
            typedef void (__thiscall* DtorFunc)(void* self, int flags);
            DtorFunc dtor = (DtorFunc)(*(uintptr_t**)*g_cursor_surf)[0];
            dtor(*g_cursor_surf, 1);
        }
        *g_cursor_surf = NULL;
    }

    /* Build backdrop path: "backdrop\\" + filename */
    char full_path[260];
    strcpy(full_path, s_backdrop_prefix);
    strcat(full_path, filename);

    /* Create child window for backdrop. 0x168 was the original x86
     * sizeof(ChildWindow); use the real host size (see
     * ui/UI_ChildWindow.h). */
    void* child = operator_new(ChildWindow_Size());
    if (child == NULL) {
        *g_cursor_surf = NULL;
    } else {
        *g_cursor_surf = (int*)UI_CreateChildWindow(child, -1, (int)full_path);
    }

    if (*g_cursor_surf != NULL) {
        /* Get surface from child window */
        typedef void* (__thiscall* GetSurfaceFunc)(void* self, int a, int b);
        GetSurfaceFunc getSurf = (GetSurfaceFunc)(*(uintptr_t**)*g_cursor_surf)[1];
        void* surface = getSurf(*g_cursor_surf, 0, 0);

        if (surface != NULL) {
            int surf_w = *(int*)((intptr_t)surface + 8);
            int surf_h = *(int*)((intptr_t)surface + 0xC);
            if (surf_w != g_world_width || surf_h != g_world_height) {
                /* Fill backdrop with dithered pattern */
                UIPANEL_FillRect(surface, g_world_width, g_world_height);
                (*g_cursor_surf)[5] = surf_w / (uint16_t)(*g_cursor_surf)[0x58/2];
                *(uint16_t*)((intptr_t)*g_cursor_surf + 0x16) = (uint16_t)surf_h;
            }
        }

        /* Store the filename for later use */
        if ((*g_cursor_surf)[4] != 0) {
            strcpy((char*)((intptr_t)self + 0x1E5), filename);
        } else {
            /* Free and fallback to resource 0x400 */
            if (*g_cursor_surf != NULL) {
                if ((*g_cursor_surf)[1] == -1 && *g_cursor_surf != NULL) {
                    typedef void (__thiscall* DtorFunc)(void* self, int flags);
                    DtorFunc dtor = (DtorFunc)(*(uintptr_t**)*g_cursor_surf)[0];
                    dtor(*g_cursor_surf, 1);
                }
            }
            *g_cursor_surf = (int*)ResourceManager_GetById(reinterpret_cast<void**>(&g_resmgr), 0x400);
            if (*g_cursor_surf != NULL) {
                typedef void* (__thiscall* GetSurfFunc)(void* self, int a, int b);
                GetSurfFunc gs = (GetSurfFunc)(*(uintptr_t**)*g_cursor_surf)[1];
                gs(*g_cursor_surf, 0, 0);
            }
        }
    }

    /* Invalidate the full viewport */
    TileMap_InvalidateRect(g_tilemap,
        *(int*)0x4FD0F0, *(int*)0x4FD0F4,
        *(int*)0x4FD0F8, *(int*)0x4FD0FC);
}

#pragma GCC diagnostic pop
