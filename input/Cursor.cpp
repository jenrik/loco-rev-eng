// Status: INTEGRATED
/* Cursor.cpp — Core initialization, lifecycle, and shared utilities */

#include "Cursor.h"
#include "Cursor_internal.h"
#include "../ui/UI_WindowBase.h"
#include "../ui/ButtonSprite.h"


/* Forward declarations for internal functions called within this file */
/* declared in Cursor_internal.h */

/* ================================================================== */
/* CORE LIFECYCLE IMPLEMENTATIONS                                      */
/* ================================================================== */

/* ================================================================== */
/* Cursor_UnlockAllSurfaces (C free function, not a Cursor method)     */
/* Address: 0x414EF0                                                   */
/*                                                                     */
/* Checks g_town, g_postcard, g_cursor, g_postcard_send, g_ui_main    */
/* for a visible window (+0xE4), then calls DDRAW_UnlockPrimary.      */
/* Fallback to g_main_window.                                          */
/* ================================================================== */
void Cursor_UnlockAllSurfaces(void)
{
    const auto unlock_if_visible = [](void* opaque_window) {
        auto* window = static_cast<UI_WindowBase*>(opaque_window);
        if (window != nullptr && window->visible != 0) {
            DDRAW_UnlockPrimary();
            return true;
        }
        return false;
    };

    if (unlock_if_visible(g_town) ||
        unlock_if_visible(g_postcard) ||
        unlock_if_visible(g_cursor) ||
        unlock_if_visible(g_postcard_send) ||
        unlock_if_visible(g_ui_main)) {
        return;
    }

    DDRAW_UnlockPrimary();
}

/* ================================================================== */
/* Cursor::Cursor — Constructor                                        */
/* Address: 0x415980                                                    */
/*                                                                     */
/* Chains to UI_WindowBase(hInstance, resId), then calls init().       */
/* Called by: CGWND_InitAllSubsystems @ 0x4073C2                       */
/* ================================================================== */
Cursor::Cursor(HINSTANCE hInstance, uint32_t resId)
    : UI_WindowBase(hInstance, resId)
{
#ifndef _WIN32
    /* Host: skip init() — the SDL host does not need the full editor/
     * colour-picker UI for singleplayer rendering.  init() loads
     * Edit_colour.dat and creates ~45 ButtonSprites; it relies on
     * file-I/O stubs not yet complete for the host path. */
#else
    this->init();
#endif
}

/* ================================================================== */
/* Cursor::~Cursor — Destructor (vtable[0])                             */
/* Address: 0x4159E0 (scalar deleting destructor)                       */
/*                                                                     */
/* Body from base_destructor() @ 0x4166B0. The compiler automatically  */
/* chains to UI_WindowBase::~UI_WindowBase() after this body.          */
/* ================================================================== */
Cursor::~Cursor()
{
    this->base_destructor();
    /* NOTE: Compiler auto-calls UI_WindowBase::~UI_WindowBase() after this.
     * The binary's UI_WindowBase_BaseDtor @ 0x425910 (decrements cursor
     * refcount, releases child objects) is handled by the C++ destructor chain. */
}

/* ================================================================== */
/* Cursor::base_destructor — Base destructor body                      */
/* Address: 0x4166B0                                                    */
/*                                                                     */
/* Releases: obj_184 (player record), hBrush (GDI brush),              */
/* background_surface, all ~40 UISprite objects, 10 editor_sprites,    */
/* toolbar_sprites[64], bonus_sprites[16], 2 editor DDraw surfaces,    */
/* then chains to UI_WindowBase_BaseDtor.                               */
/* ================================================================== */
void Cursor::base_destructor()
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Release obj_184 (player record).
     * The binary uses the record's scalar deleting destructor (vtable[0])
     * with flag 1. The record is the 0x39C-byte DPlay player record
     * created by DPLAY_CreatePlayer; the CursorEditorRecord view covers
     * only its first 0x43 bytes. delete() on this view frees the
     * allocation; the record's virtual cleanup (if any) is not modelled
     * here — TODO: type the full record as a class with a virtual dtor. */
    if (this->obj_184 != nullptr) {                             /* +0x184 */
        delete this->obj_184;
        this->obj_184 = nullptr;
    }

    /* Delete GDI brush */
    if (this->hBrush != nullptr) {                              /* +0x380 */
        DeleteObject(this->hBrush);
        this->hBrush = nullptr;
    }

    /* Release background surface (UIPANEL object at +0x1E8).
     * The binary calls its scalar deleting destructor vtable[0](1)
     * (0x4166F6). UIPANEL's full definition lives in ui/UIPANEL.h but
     * pulling it here conflicts with the ResourceManager_GetById bridge
     * declaration (game/Panel.h); this explicit dispatch reproduces the
     * exact binary ABI instead. */
    if (this->background_surface != nullptr) {                  /* +0x1E8 */
        void** vtbl = *reinterpret_cast<void***>(this->background_surface);
        using DeletePanel = void (*)(void*, uint8_t);
        reinterpret_cast<DeletePanel>(vtbl[0])(this->background_surface, 1);
        this->background_surface = nullptr;
    }

    /* Cleanup editor sprites if initialized */
    if (this->editor_initialized != 0) {                        /* +0x2C0 */
        this->cleanup_editor_sprites();
    }

    /* Release all ButtonSprite objects via delete (vtable[0] scalar deleting dtor in binary) */
    #define DELETE_SPRITE(field) do { delete this->field; this->field = nullptr; } while(0)

    DELETE_SPRITE(sprite_2C4);                                 /* +0x2C4 */
    DELETE_SPRITE(sprite_2C8);                                 /* +0x2C8 */
    DELETE_SPRITE(sprite_2E0);                                 /* +0x2E0 */
    DELETE_SPRITE(sprite_2E4);                                 /* +0x2E4 */
    DELETE_SPRITE(sprite_2E8);                                 /* +0x2E8 */
    DELETE_SPRITE(sprite_2EC);                                 /* +0x2EC */
    DELETE_SPRITE(sprite_1C0);                                 /* +0x1C0 */
    DELETE_SPRITE(sprite_1C4);                                 /* +0x1C4 */
    DELETE_SPRITE(sprite_2CC);                                 /* +0x2CC */
    DELETE_SPRITE(sprite_2F0);                                 /* +0x2F0 */
    DELETE_SPRITE(sprite_2F4);                                 /* +0x2F4 */
    DELETE_SPRITE(sprite_148);                                 /* +0x148 */
    DELETE_SPRITE(sprite_14C);                                 /* +0x14C */
    DELETE_SPRITE(sprite_308);                                 /* +0x308 */
    DELETE_SPRITE(sprite_30C);                                 /* +0x30C */
    DELETE_SPRITE(sprite_310);                                 /* +0x310 */
    DELETE_SPRITE(sprite_314);                                 /* +0x314 */
    DELETE_SPRITE(sprite_318);                                 /* +0x318 */
    DELETE_SPRITE(sprite_31C);                                 /* +0x31C */
    DELETE_SPRITE(sprite_2A4);                                 /* +0x2A4 */
    DELETE_SPRITE(sprite_2A8);                                 /* +0x2A8 */
    DELETE_SPRITE(sprite_2AC);                                 /* +0x2AC */

    #undef DELETE_SPRITE

    /* Release bonus_sprites[16] at +0x330 */
    for (int i = 0; i < 16; i++) {
        delete this->bonus_sprites[i];
        this->bonus_sprites[i] = nullptr;
    }

    /* Release sprite_37C */
    delete this->sprite_37C;                                   /* +0x37C */
    this->sprite_37C = nullptr;

    /* Release toolbar_sprites[64] at +0x48C */
    for (int i = 0; i < 64; i++) {
        delete this->toolbar_sprites[i];
        this->toolbar_sprites[i] = nullptr;
    }

    /* Release editor_sprites[10] at +0x1F4 */
    for (int i = 0; i < 10; i++) {
        delete this->editor_sprites[i];
        this->editor_sprites[i] = nullptr;
    }

    /* Release editor DDraw surfaces at +0x590 and +0x598.
     * vtable[2] = IDirectDrawSurface4::Release() (COM IUnknown).
     * DirectDraw surfaces are platform API; literal vtable dispatch preserved
     * because these are opaque COM objects, not our classes. */
    if (this->editor_surf_a != nullptr) {                       /* +0x590 */
        Cursor_ComSurfaceRelease(this->editor_surf_a);
        this->editor_surf_a = nullptr;
    }
    if (this->editor_surf_b != nullptr) {                       /* +0x598 */
        Cursor_ComSurfaceRelease(this->editor_surf_b);
        this->editor_surf_b = nullptr;
    }

    /* NOTE: UI_WindowBase::~UI_WindowBase() is called automatically by the
     * compiler after ~Cursor() returns. The binary's UI_WindowBase_BaseDtor
     * @ 0x425910 (cursor refcount, child objects, visible flag) is handled
     * by the C++ destructor chain. */
}

/* ================================================================== */
/* Cursor::init — Full initialization (called once from constructor)  */
/* Address: 0x415A00                                                    */
/*                                                                     */
/* 5 phases:                                                           */
/*   1. Reset fields, create GDI brush                                 */
/*   2. Create ~40 UISprite objects via ButtonSprite_Ctor              */
/*   3. Load spost/Edit_colour.dat palette (10 colours, 3 bytes each)  */
/*   4. Generate 12 unique random bonus IDs (range 1..1057)            */
/*   5. Write 18-entry toolbar resource ID table at +0x6F0             */
/* ================================================================== */
void Cursor::init()
{
    /* ---- PHASE 1: Reset fields, create brush ---- */

    this->bonus_mode = 0;                                        /* +0x59D */
    this->hEditWnd = nullptr;                                    /* +0xF4 */
    this->cached_client_width = 0;                                 /* +0xEC */
    this->cached_height = 0;                     /* +0xE8 (cached_height) */
    this->obj_184 = nullptr;                                     /* +0x184 */
    this->editor_initialized = 0;                                /* +0x2C0 */

    this->editor_flags[0] = 1;                                   /* +0x2B0 tab_visible */
    this->editor_flags[1] = 1;                                   /* +0x2B1 active_tab */
    this->editor_flags[2] = 0;                                   /* +0x2B2 scroll_dir */
    this->editor_flags[3] = 0;                                   /* +0x2B3 */

    this->palette_end_idx = -1;                                  /* +0x2B8 */
    this->palette_start_idx = -1;                                /* +0x2BC */
    this->selected_idx_384 = -1;                                 /* +0x384 */

    this->counter_24C = 0;                                       /* +0x24C */
    this->ui_active = 1;                                         /* +0x188 */
    this->cached_client_height = 1;                     /* +0xF0 */
    this->field_388 = 0;                                         /* +0x388 */

    this->timer_id_18C = 0;                                      /* +0x18C */
    this->timer_id_19C = 0;                                      /* +0x19C */
    this->timer_id_198 = 0;                                      /* +0x198 */
    this->field_194 = 0;                                         /* +0x194 */

    /* Create light grey GDI brush */
    this->hBrush = CreateSolidBrush(0xE8E8E8);                  /* +0x380 */

    /* ---- PHASE 2: Create all UISprite objects ---- */

    /* Helper for sprite creation: placement-new ButtonSprite with given resource ID */
    #define CREATE_SPRITE(resId) \
        new ButtonSprite(resId)

    this->sprite_2C4 = CREATE_SPRITE(0x3C8C);  /* +0x2C4 */
    this->sprite_2C8 = CREATE_SPRITE(0x3C8E);  /* +0x2C8 */
    this->sprite_2E0 = CREATE_SPRITE(0x3C8F);  /* +0x2E0 */
    this->sprite_2E4 = CREATE_SPRITE(0x3C90);  /* +0x2E4 */
    this->sprite_2E8 = CREATE_SPRITE(0x3CAC);  /* +0x2E8 */
    this->sprite_2EC = CREATE_SPRITE(0x3CBC);  /* +0x2EC */
    this->sprite_1C0 = CREATE_SPRITE(0x3CBE);  /* +0x1C0 */
    this->sprite_1C4 = CREATE_SPRITE(0x3CC2);  /* +0x1C4 */
    this->sprite_2CC = CREATE_SPRITE(0x3CC3);  /* +0x2CC */

    this->sprite_148 = CREATE_SPRITE(0x3CBA);  /* +0x148 */
    this->sprite_14C = CREATE_SPRITE(0x3CBB);  /* +0x14C */

    this->editor_surface = nullptr;                              /* +0x1EC */
    this->editor_resdata = nullptr;                              /* +0x1F0 */
    this->prev_wndproc = 0;                                        /* +0x73C */

    this->sprite_2F0 = CREATE_SPRITE(0x3C92);  /* +0x2F0 */
    this->sprite_2F4 = CREATE_SPRITE(0x3C93);  /* +0x2F4 */

    this->scroll_top_idx = 0;                    /* scroll_top_idx */
    this->scroll_bottom_idx = 0;                    /* scroll_bottom_idx */

    /* Tab sprites */
    this->sprite_308 = CREATE_SPRITE(0x3C94);  /* +0x308 */
    this->sprite_30C = CREATE_SPRITE(0x3C95);  /* +0x30C */
    this->sprite_310 = CREATE_SPRITE(0x3C96);  /* +0x310 */
    this->sprite_314 = CREATE_SPRITE(0x3C97);  /* +0x314 */
    this->sprite_318 = CREATE_SPRITE(0x3C98);  /* +0x318 */
    this->sprite_31C = CREATE_SPRITE(0x3C99);  /* +0x31C */

    /* Colour bar up/down button sprites */
    this->sprite_2A4 = CREATE_SPRITE(0x3CBF);  /* +0x2A4 */
    this->sprite_2A8 = CREATE_SPRITE(0x3CC0);  /* +0x2A8 */
    this->sprite_2AC = CREATE_SPRITE(0x3CC1);  /* +0x2AC */

    /* 16 bonus/prize sprites at +0x330, resource IDs 0x3C9A..0x3CA9 */
    for (int i = 0; i < 16; i++) {
        this->bonus_sprites[i] = CREATE_SPRITE(0x3C9A + i);
    }

    this->sprite_37C = CREATE_SPRITE(0x3CAB);  /* +0x37C */

    /* Zero-fill toolbar_sprites[64] at +0x48C */
    for (int i = 0; i < 64; i++) {
        this->toolbar_sprites[i] = nullptr;
    }

    this->surface_toggle = 0;                    /* +0x58C */
    this->editor_surf_a = nullptr;                               /* +0x590 */
    this->editor_surf_b = nullptr;                               /* +0x598 */
    this->has_next_page = 0;                                     /* +0x2B4 */
    this->has_prev_page = 0;                                     /* +0x2B5 */
    this->surf_a_dirty = 0;                                      /* +0x594 */
    this->surf_b_dirty = 0;                                      /* +0x59C */

    /* 10 palette colour swatch sprites at +0x1F4, resource IDs 0x3CAD..0x3CB6
       Also init their 3-byte RGB slots at +0x22C */
    for (int i = 0; i < 10; i++) {
        this->editor_sprites[i] = CREATE_SPRITE(0x3CAD + i);
        this->edit_colors[i * 3] = 0;                            /* R */
        this->edit_colors[i * 3 + 1] = 0;                        /* G */
        this->edit_colors[i * 3 + 2] = 0;                        /* B */
    }

    #undef CREATE_SPRITE

    this->background_surface = nullptr;                          /* +0x1E8 */

    /* ---- PHASE 3: Load Edit_colour.dat palette ---- */

    /* The binary's format string at 0x47E44C is "%spost\Edit\colour.dat"
     * (mixed slashes, British spelling) — passed with &g_install_path. */
    char filePath[0x504] = { 0 };
    wsprintfA(filePath, "%spost\\Edit\\colour.dat", &g_install_path);

    void* readBuffer = operator_new(0x2000);
    int* streamObj = nullptr;
    int* memBuffer = nullptr;
    int fileSize = 0;
    uint8_t* streamBytes = nullptr;
    uint8_t* streamHeader = nullptr;

    /* Try Asset Manager first */
    if (g_asset_mgr != nullptr) {
        memBuffer = AssetMgr_LoadFile(&g_asset_mgr,
                                      reinterpret_cast<uint8_t*>(filePath),
                                      &fileSize);
        if (memBuffer != nullptr) {
            void* stream = operator_new(0x5C);
            if (stream != nullptr) {
                streamObj = static_cast<int*>(
                    WNDPROC_StreamFromMemory(stream, reinterpret_cast<char*>(memBuffer), fileSize, 1));
            }
        }
    }

    /* Fallback to direct file open */
    if (streamObj == nullptr) {
        void* stream = operator_new(0x5C);
        if (stream != nullptr) {
            streamObj = static_cast<int*>(
                WIN32_StreamOpenFile(stream, filePath, 0xA0, 0x479190, 1));
        }
        if (streamObj == nullptr) {
            goto skip_palette_load;
        }
    }

    /* Read palette file data */
    streamBytes = reinterpret_cast<uint8_t*>(streamObj);
    streamHeader = reinterpret_cast<uint8_t*>(
        static_cast<intptr_t>(*streamObj));
    if (*reinterpret_cast<int32_t*>(streamHeader +
                                    *reinterpret_cast<int32_t*>(streamHeader + 4) +
                                    8 + reinterpret_cast<intptr_t>(streamBytes)) == 0) {
        WIN32_StreamRead(streamObj, readBuffer, 0x2000);
        if (streamObj[2] != 0 && streamObj[2] < 0x2000) {
            /* Parse 10 rows x 3 bytes from Edit_colour.dat (whitespace-delimited ints) */
            int byteIdx = 0;
            for (int row = 0; row < 10; row++) {
                for (int col = 0; col < 3; col++) {
                    /* Skip whitespace */
                    while (byteIdx < 0x2000) {
                        char c = reinterpret_cast<char*>(readBuffer)[byteIdx];
                        if (c != ' ' && c != '\n' && c != '\r') break;
                        byteIdx++;
                    }
                    /* Read integer */
                    int val = CRT_atoi(reinterpret_cast<char*>(readBuffer) + byteIdx);
                    this->edit_colors[row * 3 + col] = static_cast<uint8_t>(val);  /* +0x22C */

                    /* Skip non-whitespace */
                    while (byteIdx < 0x2000) {
                        char c = reinterpret_cast<char*>(readBuffer)[byteIdx];
                        if (c == ' ' || c == '\n' || c == '\r') break;
                        byteIdx++;
                    }
                }
            }
        }
    }

skip_palette_load:
    if (readBuffer != nullptr) {
        GLOBAL_free(readBuffer);
    }
    /* Close the stream object. The stream is an opaque internal ABI object
     * (created by WNDPROC_StreamFromMemory / WIN32_StreamOpenFile); the
     * binary calls its vtable slot [0] with flag 1 — equivalent to a
     * scalar-deleting-dtor. The exact layout is undocumented; the dispatch
     * below mirrors the binary's pointer chain (*stream + 4 → vtable). */
    if (streamObj != nullptr) {
        auto* streamAddress = reinterpret_cast<uint8_t*>(streamObj);
        auto* streamVtableAddress = reinterpret_cast<uint8_t*>(
            static_cast<intptr_t>(*streamObj) + 4);
        void** vtbl = *reinterpret_cast<void***>(
            streamAddress + *reinterpret_cast<int32_t*>(streamVtableAddress));
        using CloseStream = void (*)(void*, uint8_t);
        reinterpret_cast<CloseStream>(vtbl[0])(streamObj, 1);
    }
    if (memBuffer != nullptr) {
        CRT_free(memBuffer);
    }

    /* ---- PHASE 4: Generate 12 unique random bonus IDs ---- */

    /* The binary divides CRT_rand() by 0x421 (fixed-point imul magic
     * 0x3E007C01 in the x86) and adds 1 — it does NOT take the modulo.
     * CRT_rand() returns 0..32767, so the IDs are in 1..31. */
    for (int i = 0; i < 12; i++) {
        uint32_t id;
        bool unique;
        do {
            unique = true;
            id = static_cast<uint32_t>(CRT_rand() / 0x421) + 1;
            for (int j = 0; j < i; j++) {
                if (this->bonus_ids[j] == static_cast<uint8_t>(id)) {
                    unique = false;
                    break;
                }
            }
        } while (!unique);
        this->bonus_ids[i] = static_cast<uint8_t>(id);          /* +0x370 */
    }

    /* ---- PHASE 5: Toolbar resource ID table at +0x6F0 -- +0x738 ---- */

    this->toolbar_sentinel = -1;                                 /* +0x6F0 */
    this->player_count = 999;                                    /* +0x6F4 */

    int32_t* resTable = this->toolbar_res_ids;
    resTable[0]  = 0x526C;
    resTable[1]  = 0x526D;
    resTable[2]  = 0x526E;
    resTable[3]  = 0x526F;
    resTable[4]  = 0x5270;
    resTable[5]  = 0x527E;
    resTable[6]  = 0x527F;
    resTable[7]  = 0x5280;
    resTable[8]  = 0x5281;
    resTable[9]  = 0x5282;
    resTable[10] = 0x5283;
    resTable[11] = 0x5284;
    resTable[12] = 0x5285;
    resTable[13] = 0x5286;
    resTable[14] = 0x5287;
    resTable[15] = 0x5288;
    resTable[16] = 0x5289;
}

/* ================================================================== */
/* Cursor::init_sprites — Load cursor sprite resources                 */
/* Address: 0x414130                                                    */
/*                                                                     */
/* Loads resources 0x1400 (primary cursor) and 0x1403 (overlay) from  */
/* g_resmgr. Stores surfaces, RESDATA pointers, pixel formats, dims.  */
/* Creates shared 256x256 backbuffer on first call. Bumps refcount.   */
/* ================================================================== */
void Cursor::init_sprites()
{
    /* Resource 0x1400 — primary cursor sprite */
    RESDATA* resdata = static_cast<RESDATA*>(
        ResourceManager_GetById(&g_resmgr, 0x1400));
    this->primary_resdata() = resdata;                          /* +0x98 */

    if (resdata != nullptr) {
        /* Get surface via RESDATA vtable[1] */
        void* surface = RESDATA_GetSurface(resdata, 0, 0);
        this->primary_surface_obj() = surface;                  /* +0x94 */
        UIPANEL_UnlockSurface(surface);

        /* Read pixel format from surface (+0x1C), dimensions from RESDATA (+0x14/+0x16) */
        this->primary_surface_fmt() = *reinterpret_cast<int32_t*>(
            reinterpret_cast<uint8_t*>(surface) + 0x1C);                    /* +0x90 */
        this->sprite_width() = *reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(resdata) + 0x14);                    /* +0x3C */
        this->sprite_height() = *reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(resdata) + 0x16);                    /* +0x40 */
    }

    /* Resource 0x1403 — cursor overlay sprite */
    resdata = static_cast<RESDATA*>(
        ResourceManager_GetById(&g_resmgr, 0x1403));
    this->overlay_resdata() = resdata;                           /* +0xA4 */

    if (resdata != nullptr) {
        void* surface = RESDATA_GetSurface(resdata, 0, 0);
        this->overlay_surface_obj() = surface;                   /* +0xA0 */
        UIPANEL_UnlockSurface(surface);
        this->overlay_surface_fmt() = *reinterpret_cast<int32_t*>(
            reinterpret_cast<uint8_t*>(surface) + 0x1C);                    /* +0x9C */
    }

    /* Create shared 256x256 cursor backbuffer if not yet created */
    if (_g_cursor_back == nullptr) {
        /* Build DDraw surface descriptor: 0x100x0x100, format 0x7C, caps 0x840 */
        int desc[24] = { 0 };
        desc[0] = 0x7C;       /* dwSize = sizeof(DDSCAPS2) */
        desc[1] = 7;          /* ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | ... */
        desc[2] = 0x100;      /* dwHeight = 256 */
        desc[3] = 0x100;      /* dwWidth = 256 */

        /* g_ddraw is an opaque IDirectDraw4 COM object; CreateSurface via
         * vtable slot [6] is the documented DirectDraw ABI. */
        void** ddrawVtbl = *reinterpret_cast<void***>(g_ddraw);
        using CreateSurface = int (*)(void*, int*, void**, int);
        reinterpret_cast<CreateSurface>(ddrawVtbl[6])(
            g_ddraw, desc, &_g_cursor_back, 0);

        uint16_t cur_w, cur_h;
        DDRAW_GetSurfaceWidthHeight(_g_cursor_back, &cur_h, &cur_w);
        (void)cur_w; (void)cur_h;
        auto* formatStorage = desc - 0x8C;
        DDRAW_SetSurfaceFormat(
            _g_cursor_back,
            static_cast<int>(reinterpret_cast<intptr_t>(formatStorage)));
        DDRAW_RestoreSurfaces(_g_cursor_back, formatStorage);
    }

    this->backbuffer() = _g_cursor_back;                           /* +0x5C */
    _g_cursor_refcount++;                                        /* global counter */
}

/* ================================================================== */
/* Cursor::init_background — Initialize shared background surface     */
/* Address: 0x416460                                                    */
/*                                                                     */
/* Creates 1280x1024 (0x500x0x400) UIPANEL background surface at      */
/* +0x1E8, then composites 4 resources (0x3CAA, 0x3CC4, 0x3CC5,      */
/* 0x3CC6) onto it. Guarded: if background_surface already set,       */
/* function is a no-op.                                                */
/*                                                                     */
/* The binary dispatches each resource through Town_BlitElement        */
/* (0x42B960) with several register-clobbered arguments (unaff_EBX/    */
/* EBP/ESI/EDI in the decompiler); the exact per-resource blit         */
/* geometry is not recoverable from the decompilation. UIPANEL_Blit    */
/* is the same leaf call Town_BlitElement forwards to; the coordinates */
/* below are a documented approximation pending raw-bytes inspection.  */
/* ================================================================== */
void Cursor::init_background()
{
    if (this->background_surface != nullptr)                      /* +0x1E8 */
        return;

    RECT bgRect;
    SetRect(&bgRect, 0, 0, 0x500, 0x400);

    /* Create the background UIPANEL surface */
    void* panel = operator_new(0x20);
    void* surface;
    if (panel != nullptr) {
        surface = UIPANEL_CreateSurface(panel);
    } else {
        surface = nullptr;
    }
    this->background_surface = static_cast<UIPANEL*>(surface);   /* +0x1E8 */

    UIPANEL_InitSurface(surface, 0x500, 0x400, 1, 0, 0);

    /* Helper: get surface from resource, blit, then release surface.
     * RESDATA vtable[1] = GetSurface, vtable[2] = ReleaseSurface. */

    /* Composite resource 0x3CAA — main panel background */
    {
        RESDATA* resdata = static_cast<RESDATA*>(
            ResourceManager_GetById(&g_resmgr, 0x3CAA));
        void* srcSurf = RESDATA_GetSurface(resdata, 0, 0);
        /* Blit at full surface rect */
        UIPANEL_Blit(srcSurf, 0, 0, 0x500, 0x400,
                     this->background_surface, 0, 0, 0x500, 0x400, 0);
        RESDATA_ReleaseSurface(resdata);
    }

    /* Composite resource 0x3CC4 */
    {
        RESDATA* resdata = static_cast<RESDATA*>(
            ResourceManager_GetById(&g_resmgr, 0x3CC4));
        void* srcSurf = RESDATA_GetSurface(resdata, 0, 0);
        UIPANEL_Blit(srcSurf, 0, 0, 0, 0,
                     this->background_surface, 0, 0, 0, 0, 0);
        RESDATA_ReleaseSurface(resdata);
    }

    /* Composite resource 0x3CC5 */
    {
        RESDATA* resdata = static_cast<RESDATA*>(
            ResourceManager_GetById(&g_resmgr, 0x3CC5));
        void* srcSurf = RESDATA_GetSurface(resdata, 0, 0);
        UIPANEL_Blit(srcSurf, 0, 0, 0, 0,
                     this->background_surface, 0, 0, 0, 0, 0);
        RESDATA_ReleaseSurface(resdata);
    }

    /* Composite resource 0x3CC6 */
    {
        RESDATA* resdata = static_cast<RESDATA*>(
            ResourceManager_GetById(&g_resmgr, 0x3CC6));
        void* srcSurf = RESDATA_GetSurface(resdata, 0, 0);
        UIPANEL_Blit(srcSurf, 0, 0, 0, 0,
                     this->background_surface, 0, 0, 0, 0, 0);
        RESDATA_ReleaseSurface(resdata);
    }
}

/* ================================================================== */
/* Cursor::create — Create cursor window and edit control              */
/* Address: 0x4169E0                                                    */
/*                                                                     */
/* Creates full-screen overlay window via UI_CreateFullWindow and      */
/* a child EDIT control for toolbar text input. Subclasses edit WndProc*/
/* with 0x416B00. Returns 1 on success, 0 on failure.                  */
/* Called by: CGWND_InitAllSubsystems @ 0x407444                       */
/* ================================================================== */
