/**
 * BuildingPanel.cpp — Building selection grid panel implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * BuildingPanel is a UI window that displays a grid of building icons
 * for player selection. It renders building sprites, occupant indicator
 * dots, and building name text using UIPANEL surfaces and GDI.
 *
 * Key functions:
 *   BuildingPanel_InitSprites        @ 0x431270
 *   BuildingPanel_DrawItem           @ 0x431560
 *   BuildingPanel_RenderGrid         @ 0x4316F0
 *   BuildingPanel_DrawIcon           @ 0x431A10
 *   BuildingPanel_DrawOccupantDots   @ 0x431B30
 *   BuildingPanel_DrawColorDot       @ 0x431ED0
 *   BuildingPanel_WndProc            @ 0x4324F0
 */

// Status: TRANSCRIBED

#include "BuildingPanel.h"
#include "../ui/UIPANEL_Surface.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */

namespace {

struct SpriteRectFields {
    uint8_t prefix_00_03[4];
    LONG left;
    int top;
    int right;
    int bottom;
};

struct PanelPlayerEntryView {
    uint8_t prefix_00_04[5];
    char name[13];
    uint8_t prefix_12_31[0x20];
    int16_t icon_cell_width;
    int16_t icon_cell_height;
    uint8_t prefix_36_37[2];
    void* occupant_list;
    uint32_t icon_data_size;
    int16_t icon_width;
    int16_t icon_height;
    void* icon_pixels;
    uint8_t tail_48_4b[4];
};

struct SurfacePixelFields {
    uint8_t prefix_00_17[0x18];
    uint8_t* pixels;
};

struct OccupantEntryView {
    int x;
    int y;
    uint8_t prefix_0c[4];
    uint8_t type;
    uint8_t prefix_0d_0f[3];
    OccupantEntryView* next;
};

static PanelPlayerEntryView* panel_player_entry(void* netman, int index)
{
    uint8_t* table = reinterpret_cast<uint8_t*>(netman) + 0x3C;
    return reinterpret_cast<PanelPlayerEntryView*>(table + index * 0x4C + 0x18);
}

#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(PanelPlayerEntryView) == 0x4C);
static_assert(offsetof(PanelPlayerEntryView, icon_cell_width) == 0x32);
static_assert(offsetof(PanelPlayerEntryView, occupant_list) == 0x38);
static_assert(offsetof(PanelPlayerEntryView, icon_pixels) == 0x44);
static_assert(offsetof(OccupantEntryView, next) == 0x10);
#endif

} // namespace
/* ================================================================== */
/* External references (C-linkage)                                     */
/* ================================================================== */

void*  operator_new(size_t size);                       /* 0x465CE0 */

extern "C" {
    /* Resource management */
    void*  ResourceManager_GetById(void* resmgr, int id);    /* 0x44CB40 */
    void   Sprite_Init(void* sprite);                        /* 0x44ADA0 */
    void   Sprite_SetState(void* sprite, int state, int* unk); /* 0x44AE20 */

    /* UIPANEL */
    int    UIPANEL_BeginPaint(void* panel);                  /* 0x42B0C0 — returns HDC */
    void   UIPANEL_EndPaintEx(void* panel, HWND hWnd, int hdc, byte flag, RECT* rect);
    void*  UIPANEL_CreateSurface(void* obj);                  /* 0x42A110 */
    void   UIPANEL_Blit(void* src_surface, uint32_t src_x, uint32_t src_y,
                        int32_t src_w, uint32_t src_h,
                        void* dest_surface, uint32_t dest_x, uint32_t dest_y,
                        int32_t dest_w, uint32_t dest_h, uint32_t flags);

    /* GDI */
    HDC    BeginPaint(HWND hWnd, void* paint_struct);       /* Win32 BeginPaint */
    void   EndPaint(HWND hWnd, void* paint_struct);          /* Win32 EndPaint */
    COLORREF SetTextColor(HDC hdc, COLORREF color);  /* returns previous color */
    int    SetBkMode(HDC hdc, int mode);            /* returns previous mode */
    HGDIOBJ SelectObject(HDC hdc, HGDIOBJ obj);    /* returns previous object */
    BOOL   DeleteObject(HGDIOBJ obj);              /* returns success */
    HPEN   CreatePen(int style, int width, COLORREF color);
    HBRUSH CreateSolidBrush(COLORREF color);
    void   Ellipse(HDC hdc, int left, int top, int right, int bottom);
    void   InflateRect(RECT* rect, int dx, int dy);
    void   OffsetRect(RECT* rect, int dx, int dy);
    void   CopyRect(RECT* dest, const RECT* src);
    void   DrawTextA(HDC hdc, const char* text, int len, RECT* rect, UINT format);
    void   SetPixel(HDC hdc, int x, int y, COLORREF color);
    int    UI_CalcDialogCoords(int* coords, int* sizes, int* unk, int* base);
    LRESULT DefWindowProcA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
}

/* ================================================================== */
/* Global variables                                                    */
/* ================================================================== */

extern void* g_resmgr;                  /* 0x4FD228 — Resource manager */
extern void* g_primary_surface;         /* 0x4FD164 — Primary DirectDraw surface */
extern void* g_netman;                  /* 0x4FD33C — Network manager (contains player entries) */
extern void* g_font_normal;             /* 0x4851D8 — Normal GDI font handle */

/* Extern functions from CGWND */
extern void CGWND_SetMode(void* mode);  /* 0x408350 */
extern void WIN32_PostQuit(void);       /* 0x463670 — real body in
                                            core/CGWND.cpp (0x419710, this
                                            comment's old value, is actually
                                            Cursor_UpdateScrollButtons) */


/* ================================================================== */
/* BuildingPanel::init_sprites — Initialize sprite resources           */
/* Address: 0x431270  (size: 152 bytes)                                */
/*                                                                     */
/* Loads bitmaps 0x3d87 (main panel bg) and 0x3d88 (selection frame), */
/* initializes 11 sprites (main_sprite, selection_sprite, 9 grid       */
/* sprites). Guarded by sprites_initialized flag at +0x27E.            */
/*                                                                     */
/* Called by: PostcardPreviewWindow_DtorBody @ 0x430D90                */
/* ================================================================== */
void BuildingPanel::init_sprites()
{
    /* Skip if already initialized */
    if (this->sprites_initialized) {                            /* +0x27E */
        return;
    }

    /* Load panel background resource 0x3d87 */
    void* res = ResourceManager_GetById(g_resmgr, 0x3d87);
    this->main_resource = res;                                   /* +0x28C */
    if (res != nullptr) {
        /* Get surface via vtable[1] = GetSurface(res, 0, 0) */
        using GetSurface = void* (__thiscall*)(int, int);
        void** vtable = reinterpret_cast<void**>(res);
        GetSurface get_surface = reinterpret_cast<GetSurface>(vtable[1]);
        this->main_surface = get_surface(0, 0);  /* +0x288 */
    }

    /* Load selection frame resource 0x3d88 */
    res = ResourceManager_GetById(g_resmgr, 0x3d88);
    this->selection_resource = res;                              /* +0x294 */
    if (res != nullptr) {
        using GetSurface = void* (__thiscall*)(int, int);
        void** vtable = reinterpret_cast<void**>(res);
        GetSurface get_surface = reinterpret_cast<GetSurface>(vtable[1]);
        this->selection_surface = get_surface(0, 0);  /* +0x290 */
    }

    /* Initialize main sprite (+0x298) */
    Sprite_Init(this->main_sprite);                              /* +0x298 */

    /* Initialize selection frame sprite (+0x2C0) */
    Sprite_Init(this->selection_sprite);                         /* +0x2C0 */

    /* Initialize all 9 grid sprites at +0x29C */
    for (int i = 0; i < 9; i++) {
        Sprite_Init(this->grid_sprites[i]);                      /* +0x29C + i*4 */
    }

    /* Mark as initialized */
    this->sprites_initialized = 1;                               /* +0x27E */
}


/* ================================================================== */
/* BuildingPanel::draw_item — Draw one building entry in grid          */
/* Address: 0x431560  (size: 386 bytes)                                */
/*                                                                     */
/* For the cell at (cell_left..cell_bottom):                           */
/*   1. Sets main_sprite (+0x298) screen rect coordinates             */
/*   2. Sets sprite state: 2 (empty) if player_idx out of range,      */
/*      or 0/1 based on whether this is the selected index            */
/*   3. Begins GDI painting via UIPANEL_BeginPaint                    */
/*   4. Draws building name text using DrawTextA (font: g_font_normal)*/
/*   5. Draws colored player indicator dot via DrawColorDot           */
/*   6. Ends paint via UIPANEL_EndPaintEx                             */
/*                                                                     */
/* @param cell_left    Cell left edge                                 */
/* @param cell_top     Cell top edge                                  */
/* @param cell_right   Cell right edge                                */
/* @param cell_bottom  Cell bottom edge                               */
/* @param player_idx   Player index                                   */
/* ================================================================== */
void BuildingPanel::draw_item(LONG cell_left, int cell_top,
                               int cell_right, int cell_bottom,
                               int player_idx)
{
    /* Resolve building entry from player index. The assembly uses a
     * 0x4C-byte entry stride and starts at +0x18. */
    int* player_entry;
    if (player_idx < 0) {
        player_entry = nullptr;
    } else {
        player_entry = reinterpret_cast<int*>(panel_player_entry(g_netman, player_idx));
    }

    /* Step 1: Set main sprite screen rect */
    void* sprite = this->main_sprite;                            /* +0x298 */
    SpriteRectFields* sprite_rect = reinterpret_cast<SpriteRectFields*>(sprite);
    sprite_rect->left = cell_left;                              /* +0x04 */
    sprite_rect->top = cell_top;                                /* +0x08 */
    sprite_rect->right = cell_right;                            /* +0x0C */
    sprite_rect->bottom = cell_bottom;                          /* +0x10 */

    /* Step 2: Check if player index is within valid range
     * g_netman->maxPlayers is at offset +0x04 */
    int max_players = *reinterpret_cast<const int*>(
        reinterpret_cast<const uint8_t*>(g_netman) + 4);
    if (max_players <= player_idx) {
        /* Out of range — set sprite to "empty" state (2) */
        Sprite_SetState(this->main_sprite, 2, nullptr);
        return;
    }

    /* Within range — set sprite state: 1 if selected, 0 if not */
    int state = (player_idx == this->selected_index) ? 1 : 0;   /* +0x270 */
    Sprite_SetState(this->main_sprite, state, nullptr);

    /* Step 3: Begin GDI painting */
    HDC hdc = reinterpret_cast<HDC>(static_cast<uintptr_t>(
        UIPANEL_BeginPaint(this)));

    /* Step 4: Set text color to black, background to transparent */
    COLORREF old_color = SetTextColor(hdc, 0);                   /* black */
    int old_bk_mode = SetBkMode(hdc, 1);                         /* TRANSPARENT */

    /* Select normal font */
    HGDIOBJ old_font = SelectObject(hdc, g_font_normal);

    /* Step 5: Draw building name text
     * Name starts at player_entry + 5 bytes */
    const char* name_text = reinterpret_cast<const PanelPlayerEntryView*>(
        player_entry)->name;
    int name_len = 0xFFFFFFFF - 1;  /* strlen placeholder */

    /* Compute string length (strlen equivalent) */
    {
        const char* p = name_text;
        uint32_t remaining = 0xFFFFFFFF;
        while (remaining != 0) {
            if (*p == '\0') break;
            p++;
            remaining--;
        }
        name_len = static_cast<int>(~remaining - 1);
    }

    if (name_len > 0) {
        RECT text_rect;
        text_rect.left   = cell_left;
        text_rect.top    = cell_top;
        text_rect.right  = cell_right;
        text_rect.bottom = cell_bottom;
        InflateRect(&text_rect, -12, -5);   /* inset from cell edges */

        /* DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX = 0x124 */
        DrawTextA(hdc, name_text, name_len, &text_rect, 0x124);
    }

    /* Restore GDI state */
    SelectObject(hdc, old_font);
    SetTextColor(hdc, old_color);
    SetBkMode(hdc, old_bk_mode);

    /* Step 6: Draw colored player indicator dot */
    int dot_y = (cell_bottom - cell_top) / 2 - 2 + cell_top;
    this->draw_color_dot(hdc, cell_right - 16, dot_y, player_idx);

    /* Step 7: End paint */
    UIPANEL_EndPaintEx(this, this->hWnd,
                       static_cast<int>(reinterpret_cast<uintptr_t>(hdc)),
                       1, nullptr);  /* +0x08 = hWnd */
}


/* ================================================================== */
/* BuildingPanel::render_grid — Render full building selection grid    */
/* Address: 0x4316F0  (size: 799 bytes)                                */
/*                                                                     */
/* The grid is built from the global g_netman structure:               */
/*   Columns = g_netman->playerCount                                   */
/*   Rows = g_netman->currentPlayer                                    */
/*   Cell size = 192x144 (0xC0 x 0x90)                                 */
/*                                                                     */
/* Rendering order:                                                    */
/*   1. For each row, draw horizontal separator lines (blit from       */
/*      panel_surface with offset to primary surface)                  */
/*   2. For each column, draw vertical separator lines                 */
/*   3. For each cell, draw icon + occupant dots                       */
/* ================================================================== */
void BuildingPanel::render_grid()
{
    /* Read g_netman fields for grid dimensions */
    const int* netman_base = reinterpret_cast<const int*>(g_netman);
    int player_count    = netman_base[0];       /* g_netman+0x00 — playerCount */
    int max_players     = netman_base[1];       /* g_netman+0x04 — maxPlayers */
    int current_player  = *reinterpret_cast<const int*>(
        reinterpret_cast<const uint8_t*>(g_netman) + 0x08); /* currentPlayer index */

    /* Source rect from panel at +0x120 (4 consecutive int32_t fields = RECT layout) */
    RECT* src_rect = reinterpret_cast<RECT*>(&this->src_rect_left);
    int src_left   = src_rect->left;            /* +0x120 */
    int src_top    = src_rect->top;             /* +0x124 */
    int src_right  = src_rect->right;           /* +0x128 */
    int src_bottom = src_rect->bottom;          /* +0x12C */

    /* Grid cell origin and seed rect */
    RECT cell_rect;
    cell_rect.left   = src_left;                 /* grid left edge */
    cell_rect.top    = src_top;                  /* grid top edge */
    cell_rect.bottom = src_top + 0x90;           /* cell height = 144 */
    cell_rect.right  = cell_rect.left + 0xC0;    /* cell width = 192 */

    /* --- Step 1: Draw horizontal separator lines ---
     * For each row, blit a 1-pixel separator from panel_surface
     * to primary_surface at the scroll-adjusted position */
    {
        /* Build a 1-pixel-thick separator RECT from the source rect */
        RECT sep_src;
        CopyRect(&sep_src, src_rect);
        sep_src.left   -= 10;                    /* extend slightly */
        sep_src.right  += 10;
        sep_src.bottom  = sep_src.top + 10;      /* thicker for source */
        sep_src.top    -= 10;

        /* Build destination RECT with scroll offset */
        RECT sep_dst;
        CopyRect(&sep_dst, &sep_src);
        OffsetRect(&sep_dst, this->scroll_offset_x, this->scroll_offset_y);  /* +0x100/+0x104 */

        /* For each row, blit horizontal separator */
        for (int row = 0; row <= current_player; row++) {
            UIPANEL_Blit(
                this->panel_surface,             /* +0x280 */
                sep_src.left, sep_src.top,
                sep_src.right, sep_src.bottom,
                g_primary_surface,
                sep_dst.left, sep_dst.top,
                sep_dst.right, sep_dst.bottom,
                1);                              /* flags = 1 (copy) */

            OffsetRect(&sep_dst, 0, 0x90);       /* next row (144px down) */
            OffsetRect(&sep_src, 0, 0x90);        /* next row source */
        }
    }

    /* --- Step 2: Draw vertical separator lines ---
     * Similar pattern but horizontal offset instead of vertical */
    {
        /* Build vertical separator rect */
        RECT sep_src;
        CopyRect(&sep_src, src_rect);
        sep_src.top    -= 10;
        sep_src.right   = sep_src.left + 10;
        sep_src.bottom += 10;
        sep_src.left   -= 10;

        RECT sep_dst;
        CopyRect(&sep_dst, &sep_src);
        OffsetRect(&sep_dst, this->scroll_offset_x, this->scroll_offset_y);

        /* For each column, blit vertical separator */
        for (int col = 0; col <= current_player; col++) {
            UIPANEL_Blit(
                this->panel_surface,
                sep_src.left, sep_src.top,
                sep_src.right, sep_src.bottom,
                g_primary_surface,
                sep_dst.left, sep_dst.top,
                sep_dst.right, sep_dst.bottom,
                1);

            OffsetRect(&sep_dst, 0xC0, 0);        /* next column (192px right) */
            OffsetRect(&sep_src, 0xC0, 0);
        }
    }

    /* --- Step 3: Render each cell with icon + occupant dots ---
     * Iterates rows * columns, limited by current_player and player_count */
    int global_player_index = 0;                 /* flat index across grid */
    int base_scroll_x = cell_rect.left;
    int base_scroll_y = cell_rect.top;

    for (int row = 0; row < current_player; row++) {
        for (int col = 0; col < player_count; col++) {
            void* grid_sprite = this->grid_sprites[row * 3 + col];  /* +0x29C */

            /* Set sprite screen rect to current cell */
            SpriteRectFields* grid_rect = reinterpret_cast<SpriteRectFields*>(grid_sprite);
            grid_rect->left = cell_rect.left;
            grid_rect->top = cell_rect.top;
            grid_rect->right = cell_rect.right;
            grid_rect->bottom = cell_rect.bottom;

            if (global_player_index < max_players) {
                /* Determine sprite state: 1 = empty, 2 = occupied */
                /* Check building entry at playerIds[global_player_index] for occupant count */
                const PanelPlayerEntryView* player_entry =
                    panel_player_entry(g_netman, global_player_index);
                int sprite_state = (*reinterpret_cast<const int*>(player_entry) == 0)
                                       ? 1 : 2;
                Sprite_SetState(grid_sprite, sprite_state, nullptr);

                /* Draw building icon and occupant dots */
                BuildingPanel_DrawIcon(reinterpret_cast<uint*>(&cell_rect),
                                        &global_player_index);
                this->draw_occupant_dots(&cell_rect.left, &global_player_index);
            }

            global_player_index++;

            /* Move to next column */
            OffsetRect(&cell_rect, 0xC1, 0);      /* 192 + 1px gap */
        }

        /* Reset to start of next row */
        OffsetRect(&cell_rect, 0, 0x91);          /* 144 + 1px gap */
        cell_rect.right  = cell_rect.left + 0xC0;  /* reset width */
        cell_rect.left   = base_scroll_x;
    }
}


/* ================================================================== */
/* BuildingPanel::draw_occupant_dots — Draw occupant indicator dots    */
/* Address: 0x431B30  (size: 885 bytes)                                */
/*                                                                     */
/* Draws colored ellipses for each occupant in the building linked     */
/* list at BuildingEntry+0x38. Colors are selected from an 8-entry     */
/* palette. Selected building gets larger dots (7x7 vs 5x5) with      */
/* white highlight pixels.                                              */
/*                                                                     */
/* @param cell_rect   Pointer to cell RECT                             */
/* @param player_idx  Pointer to current player index                  */
/* ================================================================== */
void BuildingPanel::draw_occupant_dots(int* cell_rect, int* player_idx)
{
    /* Resolve building entry from player index */
    PanelPlayerEntryView* player_entry = nullptr;
    if (*player_idx >= 0) {
        player_entry = panel_player_entry(g_netman, *player_idx);
    }

    /* Calculate cell dimensions */
    int cell_w = static_cast<int>(player_entry->icon_cell_width);
    int cell_h = static_cast<int>(player_entry->icon_cell_height); /* +0x34 */

    /* Begin GDI painting */
    HDC hdc = reinterpret_cast<HDC>(static_cast<uintptr_t>(
        UIPANEL_BeginPaint(this)));

    /* Create black pen for dot outlines */
    HPEN pen = CreatePen(0, 1, 0);                /* PS_SOLID, 1px, black */
    HGDIOBJ old_pen = SelectObject(hdc, pen);

    /* Calculate dialog coordinates relative to cell */
    int coords[2] = { 1, 0 };
    int sizes[2] = { 1, 0 };
    int base[2] = { 0, 0 };
    UI_CalcDialogCoords(coords, sizes, base, cell_rect);

    int dot_spacing_x = coords[0] - cell_rect[0];  /* x spacing */
    int dot_spacing_y = sizes[0] - cell_rect[1];   /* y spacing */

    /* Iterate occupant linked list at player_entry+0x38. */
    OccupantEntryView* occupant = reinterpret_cast<OccupantEntryView*>(
        player_entry->occupant_list);
    while (occupant != nullptr) {
        int occ_x = occupant->x;
        int occ_y = occupant->y;

        if (occ_x > 0 || occ_y > 0) {
            /* Determine occupant color based on type */
            uint8_t occ_type = occupant->type;
            int netman_playercount = *reinterpret_cast<const int*>(g_netman);

            int palette_index = occ_type % netman_playercount
                              + (occ_type / netman_playercount) * 3;

            COLORREF color;
            switch (palette_index) {
            case 0: color = 0x0000FF;       break;  /* red */
            case 1: color = 0x0283FA;       break;  /* orange */
            case 2: color = 0x0FCFEF;       break;  /* yellow */
            case 3: color = 0xC2249D;       break;  /* purple */
            case 4: color = 0xF1500C;       break;  /* dark orange */
            case 5: color = 0x008000;       break;  /* green */
            case 6: color = 0xFF92FE;       break;  /* pink */
            case 7: color = 0x575757;       break;  /* gray */
            default: color = 0xD2D2D2;      break;  /* light gray */
            }

            /* Create brush and draw ellipse */
            HBRUSH brush = CreateSolidBrush(color);
            HGDIOBJ old_brush = SelectObject(hdc, brush);

            /* Recalculate positions with dialog coords */
            coords[0] = occ_x;
            sizes[0] = occ_y;
            UI_CalcDialogCoords(coords, sizes, base, cell_rect);
            occ_x = coords[0];
            occ_y = sizes[0];

            /* Clamp to cell bounds */
            if (cell_rect[2] <= occ_x) {
                occ_x = (cell_rect[2] - dot_spacing_x) - 1;
            }
            if (occ_x < cell_rect[0]) {
                occ_x = cell_rect[0];
            }
            if (cell_rect[3] <= occ_y) {
                occ_y = (cell_rect[3] - dot_spacing_y) - 1;
            }
            if (occ_y < cell_rect[1]) {
                occ_y = cell_rect[1];
            }

            /* Build dot rect */
            RECT dot_rect;
            dot_rect.right  = dot_spacing_x + occ_x;
            dot_rect.bottom = dot_spacing_y + occ_y;
            dot_rect.left   = occ_x;
            dot_rect.top    = occ_y;

            /* Determine dot size: larger if this occupant is selected */
            bool is_selected = (this->selection_active &&  /* +0x274 */
                               this->selected_index == static_cast<int>(occ_type)); /* +0x270 */
            int inflate_x = is_selected ? 5 : 3;
            int inflate_y = is_selected ? 5 : 3;
            InflateRect(&dot_rect, inflate_x, inflate_y);

            /* Draw ellipse */
            Ellipse(hdc, dot_rect.left, dot_rect.top,
                    dot_rect.right, dot_rect.bottom);

            /* Draw white highlight pixel(s) */
            if (!is_selected) {
                SetPixel(hdc, dot_rect.left + 2, dot_rect.top + 2, 0xFFFFFF);
                SetPixel(hdc, dot_rect.left + 3, dot_rect.top + 3, 0xDCDCDC);
            } else {
                SetPixel(hdc, dot_rect.left + 4, dot_rect.top + 3, 0xFFFFFF);
                SetPixel(hdc, dot_rect.left + 3, dot_rect.top + 4, 0xFFFFFF);
                SetPixel(hdc, dot_rect.left + 4, dot_rect.top + 4, 0xFFFFFF);
                SetPixel(hdc, dot_rect.left + 5, dot_rect.top + 4, 0xDCDCDC);
                SetPixel(hdc, dot_rect.left + 4, dot_rect.top + 5, 0xDCDCDC);
                SetPixel(hdc, dot_rect.left + 5, dot_rect.top + 5, 0xDCDCDC);
            }

            /* Cleanup GDI objects for this occupant:
             * SelectObject returns the brush we created (object being replaced).
             * DeleteObject of that brush handle safely deletes it. */
            DeleteObject(SelectObject(hdc, old_brush));
        }

        /* Next occupant in linked list (at +0x10) */
        occupant = occupant->next;
    }

    /* Restore GDI pen state and delete created pen */
    DeleteObject(SelectObject(hdc, old_pen));

    /* End paint */
    UIPANEL_EndPaintEx(this, this->hWnd,
                       static_cast<int>(reinterpret_cast<uintptr_t>(hdc)),
                       1, nullptr);
}


/* ================================================================== */
/* BuildingPanel::draw_color_dot — Draw player indicator dot           */
/* Address: 0x431ED0  (size: 546 bytes)                                */
/*                                                                     */
/* Draws a colored circular dot at position (x, y) for the given       */
/* player. Color palette matches DrawOccupantDots. Selected player     */
/* gets larger (18x18) with 3 highlight pixels; unselected is 12x12   */
/* with 1 highlight pixel.                                             */
/*                                                                     */
/* GDI cleanup pattern (from binary):                                  */
/*   1. SelectObject restore old_brush → returns brush handle          */
/*   2. DeleteObject(brush)                                            */
/*   3. SelectObject restore old_pen → returns pen handle             */
/*   4. DeleteObject(pen)                                              */
/*                                                                     */
/* @param hdc         GDI device context                               */
/* @param x           Dot left X position                              */
/* @param y           Dot top Y position                               */
/* @param player_idx  Player index for color selection                 */
/* ================================================================== */
void BuildingPanel::draw_color_dot(HDC hdc, int x, int y, int player_idx)
{
    /* Create black pen */
    HPEN pen = CreatePen(0, 1, 0);                /* PS_SOLID, 1px, black */
    HGDIOBJ old_pen = SelectObject(hdc, pen);

    /* Determine color from palette */
    int netman_playercount = *reinterpret_cast<const int*>(g_netman);
    int palette_index = player_idx % netman_playercount
                      + (player_idx / netman_playercount) * 3;

    COLORREF color;
    switch (palette_index) {
    case 0: color = 0x0000FF;       break;  /* red */
    case 1: color = 0x0283FA;       break;  /* orange */
    case 2: color = 0x0FCFEF;       break;  /* yellow */
    case 3: color = 0xC2249D;       break;  /* purple */
    case 4: color = 0xF1500C;       break;  /* dark orange */
    case 5: color = 0x008000;       break;  /* green */
    case 6: color = 0xFF92FE;       break;  /* pink */
    case 7: color = 0x575757;       break;  /* gray */
    default: color = 0xD2D2D2;      break;  /* light gray */
    }

    /* Create brush and select */
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ old_brush = SelectObject(hdc, brush);

    /* Build dot rect at (x, y, x+4, y+4) then inflate */
    RECT dot_rect;
    dot_rect.left   = x;
    dot_rect.right  = x + 4;
    dot_rect.top    = y;
    dot_rect.bottom = y + 4;

    /* Determine dot size based on selection state */
    bool is_selected = (this->selection_active &&  /* +0x274 */
                        this->selected_index == player_idx);  /* +0x270 */
    int inflate = is_selected ? 7 : 4;
    InflateRect(&dot_rect, inflate, inflate);

    /* Draw ellipse */
    Ellipse(hdc, dot_rect.left, dot_rect.top, dot_rect.right, dot_rect.bottom);

    /* Draw white highlight pixel(s) */
    if (!is_selected) {
        SetPixel(hdc, dot_rect.left + 3, dot_rect.top + 3, 0xFFFFFF);
        SetPixel(hdc, dot_rect.left + 4, dot_rect.top + 4, 0xDCDCDC);
    } else {
        SetPixel(hdc, dot_rect.left + 5, dot_rect.top + 4, 0xFFFFFF);
        SetPixel(hdc, dot_rect.left + 4, dot_rect.top + 5, 0xFFFFFF);
        SetPixel(hdc, dot_rect.left + 5, dot_rect.top + 5, 0xFFFFFF);
        SetPixel(hdc, dot_rect.left + 6, dot_rect.top + 5, 0xDCDCDC);
        SetPixel(hdc, dot_rect.left + 5, dot_rect.top + 6, 0xDCDCDC);
        SetPixel(hdc, dot_rect.left + 6, dot_rect.top + 6, 0xDCDCDC);
    }

    /* Restore GDI objects: restore originals, delete created ones.
     * SelectObject returns the object being replaced (which is the one
     * we created), so we DeleteObject that returned handle. */
    /* BUG FIX: The previous implementation re-selected pen instead of
     * restoring old_brush/old_pen, which could leak GDI objects. */
    DeleteObject(SelectObject(hdc, old_brush));  /* restores old brush, deletes created brush */
    DeleteObject(SelectObject(hdc, old_pen));    /* restores old pen, deletes created pen */
}


/* ================================================================== */
/* BuildingPanel_DrawIcon — Draw building bitmap icon                  */
/* Address: 0x431A10  (size: 278 bytes)                                */
/*                                                                     */
/* Creates a temp UIPANEL surface, copies pixel data from the          */
/* building entry's icon buffer, blits to primary surface at the       */
/* grid cell position with alpha 0x10, then releases the surface.      */
/*                                                                     */
/* BuildingEntry icon data layout:                                     */
/*   +0x40: pixel data size (int)                                      */
/*   +0x42: icon height (short)                                        */
/*   +0x44: icon pixel data pointer                                    */
/*                                                                     */
/* @param cell_rect    Pointer to cell RECT for positioning            */
/* @param player_index Pointer to current player index                */
/* ================================================================== */
void BuildingPanel_DrawIcon(uint* cell_rect, int* player_index)
{
    /* Resolve building entry */
    PanelPlayerEntryView* player_entry = nullptr;
    if (*player_index >= 0) {
        player_entry = panel_player_entry(g_netman, *player_index);
    }

    /* Check if entry has icon data */
    if (player_entry == nullptr || player_entry->icon_pixels == nullptr) { /* +0x44 */
        return;
    }

    /* Get icon dimensions */
    int icon_width = static_cast<int>(player_entry->icon_width); /* +0x40 */
    int icon_height = static_cast<int>(player_entry->icon_height); /* +0x42 */

    /* Create temp surface for icon */
    void* surface_obj = operator_new(0x20);
    void* tmp_surface;
    if (surface_obj == nullptr) {
        tmp_surface = nullptr;
    } else {
        tmp_surface = UIPANEL_CreateSurface(surface_obj);
    }

    if (tmp_surface != nullptr) {
        /* Initialize surface with icon dimensions */
        UIPANEL_InitSurface(tmp_surface, icon_width, icon_height, 0, 0, 0);

        /* Copy icon pixel data */
        uint32_t data_size = player_entry->icon_data_size;    /* +0x3C */
        uint8_t* src_data = static_cast<uint8_t*>(player_entry->icon_pixels); /* +0x44 */
        uint8_t* dst_data = reinterpret_cast<SurfacePixelFields*>(tmp_surface)->pixels;
                                                               /* surface pixel ptr +0x18 */

        /* Memcpy whole dwords first, then remaining bytes */
        uint32_t dwords = data_size >> 2;
        uint32_t remainder = data_size & 3;

        uint32_t* src32 = reinterpret_cast<uint32_t*>(src_data);
        uint32_t* dst32 = reinterpret_cast<uint32_t*>(dst_data);
        for (uint32_t i = 0; i < dwords; i++) {
            *dst32++ = *src32++;
        }
        for (uint32_t i = 0; i < remainder; i++) {
            *dst_data++ = *src_data++;
        }

        /* Blit icon to primary surface at cell position */
        UIPANEL_Blit(
            tmp_surface,
            cell_rect[0], cell_rect[1], cell_rect[2], cell_rect[3],
            g_primary_surface,
            0, 0,
            icon_width, icon_height,
            0x10);                                            /* flags = alpha/transparency */

        /* Release temp surface via vtable[0] */
        if (tmp_surface != nullptr) {
            using SurfaceDestructor = void (__thiscall*)(int);
            void** vtable = reinterpret_cast<void**>(tmp_surface);
            SurfaceDestructor destroy = reinterpret_cast<SurfaceDestructor>(vtable[0]);
            destroy(1);                                       /* release surface */
        }
    }
}


/* ================================================================== */
/* BuildingPanel_WndProc — Building panel window procedure              */
/* Address: 0x4324F0  (size: 70 bytes)                                  */
/*                                                                     */
/* Registered at 0x477E4C as the window class wndProc.                 */
/* Intercepts WM_SYSCOMMAND (0x112) with SC_SCREENSAVE (0xF140) to     */
/* switch to town mode (CGWND_SetMode(3)) and post quit instead of     */
/* activating screensaver. All other messages go to DefWindowProcA.     */
/*                                                                     */
/* NOTE: Even after handling SC_SCREENSAVE, the function falls through */
/* to DefWindowProcA and returns its result (EAX preserved).           */
/*                                                                     */
/* Called by: Windows message loop                                     */
/*                                                                     */
/* @param hWnd   Window handle                                         */
/* @param msg    Window message                                        */
/* @param wParam WPARAM                                                */
/* @param lParam LPARAM                                                */
/* @return       LRESULT from DefWindowProcA                           */
/* ================================================================== */
LRESULT __stdcall BuildingPanel_WndProc(HWND hWnd, UINT msg, uint32_t wParam, LPARAM lParam)
{
    /* Check for WM_SYSCOMMAND with SC_SCREENSAVE */
    if (msg == 0x112 && (wParam & 0xFFF0) == 0xF140) {
        /* Switch to town mode instead of screensaver */
        CGWND_SetMode(reinterpret_cast<void*>(static_cast<uintptr_t>(3)));
        WIN32_PostQuit();
    }

    /* Default handling for all other messages (and also after intercept) */
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}
