/**
 * UIPANEL_Draw.cpp — UIPANEL drawing functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Drawing functions for UIPANEL: DrawBorder, DrawButton, DrawCheckbox,
 * DrawRadioButton, DrawEditField, plus sprite management (FreeSprite,
 * DtorSprite, CreateSprite, InitSprite, BlitSprite, BlitSpriteEx, Hide).
 */

// Status: TRANSCRIBED

#include "UIPANEL.h"
#include "UIPANEL_Surface.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {
    #define INVALID_HANDLE_VALUE    ((void*)-1)
    #define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
    #define VTBL_UIPANEL_SAVESPRITE ((void*)0x00477D24)
    /* Additional Win32 API declarations */
    void*  FindFirstFileA(const char* lpFileName, void* lpFindFileData);
    int    FindNextFileA(void* hFindFile, void* lpFindFileData);
    int    FindClose(void* hFindFile);
    int    CreateDirectoryA(const char* lpPathName, void* lpSecurityAttributes);
    int    wsprintfA(char* buf, const char* fmt, ...);
    DWORD  GetFileAttributesA(const char* lpFileName);
    int    DeleteFileA(const char* lpFileName);
}

    /* UIPANEL internal functions */
    void   UIPANEL_SetClipRect(void* surf, int x, int y);
    void   UIPANEL_DrawButton(int param_1);
    void   UIPANEL_StretchBlit(void* dst, void* src, int a, int b, int c);
    void   UIPANEL_FillRect(void* surf, int w, int h);

    void __thiscall UIPANEL_DrawBorder(void* self, int resource_ptr);
    void __thiscall UIPANEL_GetButtonState(void* self, int pos_x, int pos_y);
    uint32_t __thiscall UIPANEL_SetButtonState(void* self, void* entity,
                                                uint32_t state, uint32_t mode);
    uint8_t __thiscall UIPANEL_DrawCheckbox(void* self, void* entity,
                                             uint32_t state, uint32_t mode);
    void __thiscall UIPANEL_DrawRadioButton(void* self, void* entity);
    uint32_t __fastcall UIPANEL_DrawEditField(int param_1);
    void __fastcall UIPANEL_FreeSprite(void* sprite);
    void* __thiscall UIPANEL_DtorSprite(void* sprite, uint8_t flags);
    void __thiscall UIPANEL_CreateSprite(void* self, void* list_entry);
    void __fastcall UIPANEL_InitSprite(void* self);
    void __fastcall UIPANEL_BlitSprite(void* self);
    void __fastcall UIPANEL_BlitSpriteEx(void* self);
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
    void   RESMGR_ResourceData_Init(void* data);
    uint32_t RESMGR_ReleaseResource(void* data);
    int    RESMGR_IsSaveHeader(void* data);
    void   RESMGR_LoadResource(void* res, void* path);
    class InputMgr;
   void   INPUT_SaveCurrentWorld(InputMgr* input, const char* name);
    void   Game_SetScreenMode(void* game, char a, char b, char c);
    void   TileMap_InvalidateRect(void* tilemap, int left, int top, int right, int bottom);
    void   TileMap_InvalidateDirtyRects(void* tilemap, char flag);
    void   UI_CenterWindow(int* a, int* b);
    void*  UI_CreateChildWindow(void* obj, int parent, int title);

    /* String / CRT */
    char*  _strncpy(char* dst, const char* src, int max);
    char*  _strncat(char* dst, const char* src, int max);
    int    CRT_wcsstr(void* a, void* b);
    void*  CRT_malloc(void* ptr, size_t size);
    void   CRT_free(void* ptr);
    void   CRT_exit(void* a, void* b);

    /* File system */
    HANDLE CRT_FindFirstFile(LPCSTR path, void* data);
    int    CRT_FindNextFile(HANDLE h, void* data);
    void   CRT_FindClose(HANDLE h);

    /* Resource manager */
    extern void*  g_resmgr;               /* 0x4FD228 */
    extern char   g_install_path[];        /* 0x4852B8 */
    extern char   g_empty_string;          /* 0x476934 */
    extern void*  g_game;                 /* 0x4FD144 */
    extern void*  g_tilemap;              /* 0x4FD244 */
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
/* bitmap. If state==5: player color picker. Otherwise: resource-      */
/* backed button. If resource_ptr or buffer exists: draws default btn.  */
/* ================================================================== */
void __thiscall UIPANEL_DrawBorder(void* self, int resource_ptr)
{
    if (resource_ptr == 0 || *(int*)((intptr_t)self + 0x490) != 0) {
        /* Clear and draw default button */
        *(int*)((intptr_t)self + 0x498) = 0;
        UIPANEL_SetClipRect((void*)((intptr_t)self + 0x478), 8, 0);
        UIPANEL_DrawButton((int)self);
    }

    if (resource_ptr != 0) {
        if (*(short*)((intptr_t)self + 0x49C) == 5) {
            /* Player color picker mode */
            *(int*)((intptr_t)self + 0x498) = 0;
            UIPANEL_InitSurface((void*)((intptr_t)self + 0x478),
                                *(int*)0x4AAAAA,  /* player_id global */
                                *(int*)0x4AAAAC,  /* player_color global */
                                1, 0, 0);
            UIPANEL_StretchBlit((void*)((intptr_t)self + 0x478),
                                (LPCSTR)0x47E8A0, /* format string */
                                0,
                                *(int*)0x4AAAAA,
                                *(int*)0x4AAAAC);
        } else {
            /* Resource-backed button */
            if (!RESMGR_IsSaveHeader((void*)((intptr_t)resource_ptr + 0x50))) {
                return;
            }

            *(int*)((intptr_t)self + 0x498) = resource_ptr + 0x50;

            UIPANEL_InitSurface((void*)((intptr_t)self + 0x478),
                                *(uint16_t*)(uintptr_t)(resource_ptr + 0x102),
                                *(uint16_t*)(uintptr_t)(resource_ptr + 0x104),
                                0, 0, 8);

            void* save_header = *(uintptr_t**)((intptr_t)self + 0x498);
            if (*(uintptr_t**)((intptr_t)save_header + 0x1C4) != NULL) {
                /* Copy pixel data from save header to surface buffer */
                uint32_t pixel_count = *(uint16_t*)((intptr_t)save_header + 0xB4) *
                                      *(uint16_t*)((intptr_t)save_header + 0xB2);
                void* src_pixels = *(uintptr_t**)((intptr_t)save_header + 0x1C4);
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
            char* state = RESDATA_SoundObject_GetState((int)entity);
            RESDATA_SoundObject_Init(*(uintptr_t**)((intptr_t)self + 0x4BC), state);

            if (RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)self + 0x4BC)) == 0 &&
                *(int*)((intptr_t)self + 0x490) != 0) {
                UIPANEL_DrawBorder(self, 0);
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
void __thiscall UIPANEL_DrawRadioButton(void* self, void* entity)
{
    void* current_sound = *(uintptr_t**)((intptr_t)self + 0x4BC);  /* +0x4BC: sound_btn_sprite */

    if (entity == current_sound) {
        return;
    }

    if (*(short*)((intptr_t)entity + 0x48) == 3) {
        return;  /* Zoom == 3 means hidden/disabled */
    }

    int text_len = RESDATA_SoundObject_GetTextLength((int)current_sound);
    if (text_len == 0) {
        return;
    }

    /* Compare state strings between current and clicked entity */
    char* current_state = RESDATA_SoundObject_GetState((int)current_sound);
    char* entity_state  = RESDATA_SoundObject_GetState((int)entity);

    int cmp_result = strcmp((const char*)entity_state, (const char*)current_state);

    if (cmp_result != 0 && *(short*)((intptr_t)entity + 0x48) == 2) {
        /* Clicked a different item that's zoomed in — unzoom it */
        if (*(int*)((intptr_t)self + 0x490) != 0 &&
            *(int*)((intptr_t)self + 0x498) == *(int*)((intptr_t)entity + 0x30) + 0x50) {
            UIPANEL_DrawBorder(self, 0);
        }
        CGWND_TrackPiece_SetZoom(entity, 1);
    }

    /* Re-compare (state may have changed) */
    current_state = RESDATA_SoundObject_GetState((int)current_sound);
    entity_state  = RESDATA_SoundObject_GetState((int)entity);
    cmp_result = strcmp((const char*)entity_state, (const char*)current_state);

    if (cmp_result == 0 && *(short*)((intptr_t)entity + 0x48) == 1) {
        /* Same item, zoom in */
        CGWND_TrackPiece_SetZoom(entity, 2);
        UIPANEL_DrawBorder(self, *(int*)((intptr_t)entity + 0x30));
    }
}

/* ================================================================== */
/* UIPANEL_DrawEditField — Enumerate savegame/backdrop files            */
/* Address: 0x429490                                                   */
/*                                                                     */
/* Enumerates files via FindFirstFile/FindNextFile to build a sorted    */
/* doubly-linked list of SaveSprite entries at this+0x4D8 (head) with  */
/* next/prev links at +0x228/+0x22C. Panel mode at +0x49C:             */
/*   5 = backdrop (*.bmp) files in backdrop\\                           */
/*   else = savegame (*.sav) files in savegame\\                       */
/* Files are inserted in alphabetical order using byte-by-byte string   */
/* comparison (2 bytes at a time via CRT_wcsstr).                      */
/*                                                                     */
/* SaveSprite struct (0x230 bytes):                                     */
/*   +0x00: vtable (VTBL_UIPANEL_SAVESPRITE, 0x477D24)                 */
/*   +0x04: name[10] (null-terminated, 9 chars + null)                */
/*   +0x0E: null terminator for name (byte)                            */
/*   +0x0F: prefix string ("savegame\\" or "backdrop\\")              */
/*   +0x50: RESDATA header (used for loading save data)               */
/*   +0x228: next link in sorted list                                  */
/*   +0x22C: prev link in sorted list                                  */
/*                                                                     */
/* Called by: HandleDrag (tab switch), BlitSprite (save),              */
/*            BlitSpriteEx (delete)                                     */
/* ================================================================== */
uint32_t __fastcall UIPANEL_DrawEditField(int param_1)
{
    /* Stack SEH setup omitted */

    char path_buf[260];
    char filename_buf[260];
    uint16_t panel_mode = *(uint16_t*)((intptr_t)param_1 + 0x49C);

    /* Init RESDATA for resource data init on stack */
    int local_data[22];
    memset(local_data, 0, sizeof(local_data));
    RESMGR_ResourceData_Init(local_data);

    /* Destroy any existing sprite list */
    void* current = *(uintptr_t**)((intptr_t)param_1 + 0x4D8);  /* sprite_list_head */
    while (current != NULL) {
        void* next = *(uintptr_t**)((intptr_t)current + 0x22C);  /* next link */
        *(uintptr_t**)((intptr_t)param_1 + 0x4D8) = next;

        if (current != NULL) {
            typedef void* (__thiscall* DtorFunc)(void* self, uint8_t flags);
            DtorFunc dtor = (DtorFunc)(*(uintptr_t**)current)[0];
            dtor(current, 1);
        }

        current = *(uintptr_t**)((intptr_t)param_1 + 0x4D8);
    }

    void* sprite_list = *(uintptr_t**)((intptr_t)param_1 + 0x4D8);

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

    /* Try to find first file */
    struct WIN32_FIND_DATAA { DWORD dwFileAttributes; uint32_t ftCreationTime[2]; uint32_t ftLastAccessTime[2]; uint32_t ftLastWriteTime[2]; DWORD nFileSizeHigh; DWORD nFileSizeLow; DWORD dwReserved0; DWORD dwReserved1; char cFileName[260]; char cAlternateFileName[14]; };
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(path_buf, &find_data);

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
            if (find_data.cFileName[0] == '.') {
                continue;
            }

            /* Build filename without extension */
            strcpy(filename_buf, find_data.cFileName);
            /* Remove extension */
            char* dot = strrchr(filename_buf, '.');
            if (dot != NULL) {
                *dot = '\0';
            }

            /* Skip names longer than 10 characters (file_id limit) */
            if (strlen(filename_buf) >= 10) {
                continue;
            }

            /* Create a new SaveSprite (0x230 bytes) */
            uint8_t* sprite_mem = (uint8_t*)operator_new(0x230);
            if (sprite_mem == NULL) continue;

            /* Initialize RESDATA at +0x50 offset within struct */
            RESMGR_ResourceData_Init(sprite_mem + 0x50);

            /* Set vtable */
            *(uintptr_t**)sprite_mem = (void*)VTBL_UIPANEL_SAVESPRITE;  /* 0x477D24 */
            *(uint8_t*)(sprite_mem + 4) = 0;           /* +0x04: name[0] = 0 */
            *(uint8_t*)(sprite_mem + 0x0F) = 0;        /* +0x0F: prefix = 0 */
            *(uintptr_t**)(sprite_mem + 0x228) = NULL;      /* +0x228: next */
            *(uintptr_t**)(sprite_mem + 0x22C) = NULL;      /* +0x22C: prev */

            /* Copy filename into name field at +0x04 (max 9 chars + null) */
            strncpy((char*)sprite_mem + 4, filename_buf, 9);
            *(sprite_mem + 4 + 9) = '\0';  /* null terminate */
            *(sprite_mem + 0x0E) = '\0';   /* extra null at +0x0E */

            /* Set prefix at +0x0F */
            strcpy((char*)sprite_mem + 0x0F,
                   (panel_mode == 5) ? s_backdrop_prefix : s_savegame_prefix);
            strncat((char*)sprite_mem + 0x0F, find_data.cFileName, 0x40);
            *(sprite_mem + 0x0F + 0x40) = '\0';

            /* Insert into sorted list by filename (alphabetical) */
            void** insert_pos = (void**)(intptr_t)&sprite_list;
            void* list_node = sprite_list;

            while (list_node != NULL) {
                /* Compare filenames */
                const char* list_name = (const char*)list_node + 4;
                if (strcmp(filename_buf, list_name) < 0) {
                    break;  /* Insert before this node */
                }
                insert_pos = (void**)((intptr_t)list_node + 0x22C);  /* prev link */
                list_node = *(uintptr_t**)((intptr_t)list_node + 0x228);  /* next link */
            }

            /* Link into list */
            *(uintptr_t**)((intptr_t)sprite_mem + 0x228) = list_node;      /* next */
            *(uintptr_t**)((intptr_t)sprite_mem + 0x22C) = *insert_pos;    /* prev */
            *insert_pos = sprite_mem;

            if (list_node != NULL) {
                /* Update the found node's prev pointer */
                void** list_prev = (void**)((intptr_t)list_node + 0x22C);
                *list_prev = sprite_mem;
            }

        } while (FindNextFileA(hFind, &find_data) != 0);

        FindClose(hFind);
    }

    /* Store list head */
    *(uintptr_t**)((intptr_t)param_1 + 0x4D8) = sprite_list;
    if (sprite_list != NULL) {
        *(uintptr_t**)((intptr_t)sprite_list + 0x22C) = NULL;  /* prev of head = NULL */
    }

    /* Cleanup RESDATA on stack */
    RESMGR_ReleaseResource(local_data);

    return 1;
}

/* ================================================================== */
/* UIPANEL_FreeSprite — Release sprite resource                         */
/* Address: 0x429820                                                   */
/*                                                                     */
/* Sets vtable to sentinel (VTBL_UIPANEL_SAVESPRITE, 0x477D24) then    */
/* calls RESMGR_ReleaseResource at +0x50. Tail-called from DtorSprite  */
/* and exception handler.                                               */
/* ================================================================== */
void __fastcall UIPANEL_FreeSprite(void* sprite)
{
    *(uintptr_t**)sprite = (void*)VTBL_UIPANEL_SAVESPRITE;
    RESMGR_ReleaseResource((void*)((intptr_t)sprite + 0x50));
}

/* ================================================================== */
/* UIPANEL_DtorSprite — SaveSprite destructor (vtable[0])               */
/* Address: 0x429830                                                   */
/*                                                                     */
/* Calls FreeSprite, then frees via GLOBAL_free if flags & 1.          */
/* vtable = 0x477D24 (VTBL_UIPANEL_SAVESPRITE).                        */
/* ================================================================== */
void* __thiscall UIPANEL_DtorSprite(void* sprite, uint8_t flags)
{
    UIPANEL_FreeSprite(sprite);
    if ((flags & 1) != 0) {
        GLOBAL_free(sprite);
    }
    return sprite;
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
void __thiscall UIPANEL_CreateSprite(void* self, void* list_entry)
{
    char name_buf[260];
    memset(name_buf, 0, sizeof(name_buf));

    if (list_entry == NULL) {
        list_entry = *(uintptr_t**)((intptr_t)self + 0x4D8);  /* sprite_list_head */
    }

    /* Store current list position */
    *(uintptr_t**)((intptr_t)self + 0x4DC) = list_entry;  /* sprite_list_tail = current pos */

    void** item_slots = (void**)((intptr_t)self + 0x4C0);  /* item_sprites[6] */
    void* current = list_entry;

    for (int i = 0; i < 6; i++) {
        /* Set target pointer in each sprite slot */
        *(uintptr_t**)((intptr_t)item_slots[i] + 0x30) = current;

        if (current == NULL) {
            RESDATA_SoundObject_Init(item_slots[i], "");
            CGWND_TrackPiece_SetZoom(item_slots[i], 3);
        } else {
            RESDATA_SoundObject_Init(item_slots[i], (char*)((intptr_t)current + 4));  /* name */

            /* Check if resource has been loaded */
            if (*(short*)((intptr_t)current + 0x100) == 0) {
                /* Build full path: install_path + prefix + filename */
                memset(name_buf, 0, sizeof(name_buf));
                strcpy(name_buf, g_install_path);

                int path_end = strlen(name_buf);
                /* Copy prefix (+0x0F) from save sprite */
                strcpy(name_buf + path_end, (char*)((intptr_t)current + 0x0F));

                /* Load resource */
                RESMGR_LoadResource((void*)((intptr_t)current + 0x50), name_buf);
            }

            /* Determine zoom based on panel mode */
            uint16_t panel_mode = *(uint16_t*)((intptr_t)self + 0x49C);
            if (panel_mode == 3 || panel_mode == 4 || panel_mode == 5) {
                CGWND_TrackPiece_SetZoom(item_slots[i], 1);
            } else {
                if (RESMGR_IsSaveHeader((void*)((intptr_t)current + 0x50))) {
                    CGWND_TrackPiece_SetZoom(item_slots[i], 1);
                } else {
                    CGWND_TrackPiece_SetZoom(item_slots[i], 3);
                }
            }

            current = *(uintptr_t**)((intptr_t)current + 0x22C);  /* next link */
        }

        /* Redraw the slot sprite */
        typedef void (__thiscall* DrawFunc)(void* self);
        DrawFunc draw = (DrawFunc)(*(uintptr_t**)item_slots[i])[0x20/4];
        draw(item_slots[i]);
    }

    /* Update scroll indicators based on remaining items */
    void* tail = *(uintptr_t**)((intptr_t)self + 0x4DC);  /* sprite_list_tail */
    int has_next = (tail != NULL && *(uintptr_t**)((intptr_t)tail + 0x228) != NULL);
    int has_prev = (list_entry != NULL &&
                   *(uintptr_t**)((intptr_t)list_entry + 0x22C) != NULL);

    /* Set zoom on scroll indicators */
    short bg_zoom = (has_next || has_prev) ? 1 : 3;
    CGWND_TrackPiece_SetZoom(*(uintptr_t**)((intptr_t)self + 0x4B4), bg_zoom);  /* list_bg_sprite */

    int text_len = RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)self + 0x4D4));
    short text_zoom = (text_len != 0) ? 1 : 3;
    CGWND_TrackPiece_SetZoom(*(uintptr_t**)((intptr_t)self + 0x4B8), text_zoom);  /* list_text_sprite */

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
void __fastcall UIPANEL_InitSprite(void* self)
{
    char path_buf[260];

    /* Build savegame directory path */
    wsprintfA(path_buf, "%s%s", g_install_path, s_savegame_dir);

    /* Create directory if it doesn't exist */
    DWORD attrs = GetFileAttributesA(path_buf);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryA(path_buf, NULL);
    }

    /* Build full save path: install_path + savegame\\ + name.bmp */
    memset(path_buf, 0, sizeof(path_buf));
    strcpy(path_buf, g_install_path);
    strcat(path_buf, s_savegame_prefix);

    /* Get filename from sound button state */
    char* name = RESDATA_SoundObject_GetState(*(int*)((intptr_t)self + 0x4BC));
    strcat(path_buf, name);
    strcat(path_buf, s_dot_bmp);

    /* Load the world */
    Game_SetScreenMode(g_game, 1, 1, 1);
    TileMap_InvalidateDirtyRects(g_tilemap, 0);
    INPUT_SaveCurrentWorld(&g_input_mgr, path_buf);
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
void __fastcall UIPANEL_BlitSprite(void* self)
{
    /* Check if there's a name to save under */
    if (RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)self + 0x4BC)) == 0) {
        return;
    }

    char path_buf[260];

    /* Build savegame directory path */
    wsprintfA(path_buf, "%s%s", g_install_path, s_savegame_dir);
    DWORD attrs = GetFileAttributesA(path_buf);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryA(path_buf, NULL);
    }

    /* Build full save path */
    memset(path_buf, 0, sizeof(path_buf));
    strcpy(path_buf, g_install_path);
    strcat(path_buf, s_savegame_prefix);

    char* name = RESDATA_SoundObject_GetState(*(int*)((intptr_t)self + 0x4BC));
    strcat(path_buf, name);
    strcat(path_buf, s_dot_bmp);

    /* Save the current world */
    Game_SetScreenMode(g_game, 1, 1, 1);

    /* Destroy existing sprite list */
    void* current = *(uintptr_t**)((intptr_t)self + 0x4D8);
    while (current != NULL) {
        void* next = *(uintptr_t**)((intptr_t)current + 0x22C);
        *(uintptr_t**)((intptr_t)self + 0x4D8) = next;
        if (current != NULL) {
            typedef void* (__thiscall* DtorFunc)(void* self, uint8_t flags);
            DtorFunc dtor = (DtorFunc)(*(uintptr_t**)current)[0];
            dtor(current, 1);
        }
        current = *(uintptr_t**)((intptr_t)self + 0x4D8);
    }

    /* Save world */
    INPUT_SaveCurrentWorld(&g_input_mgr, path_buf);

    /* Rebuild file list and center on saved entry */
    UIPANEL_DrawEditField((int)self);
    UIPANEL_CreateSprite(self, *(uintptr_t**)((intptr_t)self + 0x4D8));

    /* Scroll to find the saved entry by comparing state strings */
    char* saved_state = RESDATA_SoundObject_GetState(*(int*)((intptr_t)self + 0x4BC));
    char* first_state = RESDATA_SoundObject_GetState(*(int*)((intptr_t)self + 0x4C0));

    int cmp = strcmp((const char*)saved_state, (const char*)first_state);

    /* If saved name comes after first entry, scroll forward */
    while (cmp > 0) {
        void* tail = *(uintptr_t**)((intptr_t)self + 0x4DC);
        if (tail == NULL) break;
        void* next = *(uintptr_t**)((intptr_t)tail + 0x228);
        if (next == NULL) break;

        UIPANEL_CreateSprite(self, next);

        saved_state = RESDATA_SoundObject_GetState(*(int*)((intptr_t)self + 0x4BC));
        first_state = RESDATA_SoundObject_GetState(*(int*)((intptr_t)self + 0x4C0));
        cmp = strcmp((const char*)saved_state, (const char*)first_state);
    }

    /* If saved name comes before first entry, scroll backward */
    while (cmp < 0) {
        void* head = *(uintptr_t**)((intptr_t)self + 0x4D8);
        if (head == NULL || *(uintptr_t**)((intptr_t)head + 0x22C) == NULL) {
            /* At the start, scroll to last page */
            if (RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)self + 0x4D4)) == 0) {
                break;
            }
            /* Scroll to previous page */
            void* prev = *(uintptr_t**)((intptr_t)head + 0x22C);
            if (prev == NULL) break;
            UIPANEL_CreateSprite(self, prev);
            break;
        }

        UIPANEL_CreateSprite(self, *(uintptr_t**)((intptr_t)*(uintptr_t**)((intptr_t)self + 0x4DC) + 0x22C));

        saved_state = RESDATA_SoundObject_GetState(*(int*)((intptr_t)self + 0x4BC));
        first_state = RESDATA_SoundObject_GetState(*(int*)((intptr_t)self + 0x4C0));
        cmp = strcmp((const char*)saved_state, (const char*)first_state);
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
void __fastcall UIPANEL_BlitSpriteEx(void* self)
{
    char path_buf[260];

    /* Build full file path */
    memset(path_buf, 0, sizeof(path_buf));
    strcpy(path_buf, g_install_path);
    strcat(path_buf, s_savegame_prefix);

    char* name = RESDATA_SoundObject_GetState(*(int*)((intptr_t)self + 0x4BC));
    strcat(path_buf, name);
    strcat(path_buf, s_dot_bmp);

    /* Destroy existing sprite list */
    void* current = *(uintptr_t**)((intptr_t)self + 0x4D8);
    while (current != NULL) {
        void* next = *(uintptr_t**)((intptr_t)current + 0x22C);
        *(uintptr_t**)((intptr_t)self + 0x4D8) = next;
        if (current != NULL) {
            typedef void* (__thiscall* DtorFunc)(void* self, uint8_t flags);
            DtorFunc dtor = (DtorFunc)(*(uintptr_t**)current)[0];
            dtor(current, 1);
        }
        current = *(uintptr_t**)((intptr_t)self + 0x4D8);
    }

    /* Delete the file */
    BOOL deleted = DeleteFileA(path_buf);

    /* Rebuild file list */
    UIPANEL_DrawEditField((int)self);

    if (deleted) {
        /* Clear the sound button text */
        RESDATA_SoundObject_Init(*(uintptr_t**)((intptr_t)self + 0x4BC), "");
    }

    /* Refresh display */
    UIPANEL_CreateSprite(self, *(uintptr_t**)((intptr_t)self + 0x4D8));
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

    /* Create child window for backdrop */
    void* child = operator_new(0x168);
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
            *g_cursor_surf = (int*)ResourceManager_GetById(g_resmgr, 0x400);
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
