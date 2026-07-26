/**
 * BuildingPanel.h — Building selection grid panel
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * BuildingPanel is a UI panel window that displays a grid of building
 * icons for player selection. It extends UI_WindowBase with a scrollable
 * grid of building entries (3 columns per row), each showing a building
 * icon, name text, and colored occupant indicator dots.
 *
 * The panel uses UIPANEL surfaces for rendering with GDI for text and
 * dot drawing. Building data is read from the global g_netman (network
 * manager) structure which stores player building entries.
 *
 * Size: ~0x2C4 bytes (fields from +0x08 through +0x2C0)
 * Vtable: Referenced at 0x477E4C (WNDCLASS wndProc)
 * Base class: UI_WindowBase (or equivalent window class)
 *
 * Sprite layout:
 *   +0x298: Main sprite (selection outline/background)
 *   +0x29C: Sprite array of 9 items (3 columns x 3 rows)
 *   +0x2C0: Selection frame sprite
 *
 * Grid layout:
 *   Cells: 192x144 pixels (0xC0 x 0x90) with 1px gap (0xC1 x 0x91 stride)
 *   Columns: g_netman->playerCount
 *   Rows: g_netman->currentPlayer
 */

#pragma once

#include "../shared/types.h"

/* ================================================================ */
/* BuildingPanel class                                               */
/* ================================================================ */

class BuildingPanel {
public:
    /* ================================================================ */
    /* Fields                                                            */
    /* ================================================================ */

    /* --- Inherited base (UI_WindowBase or similar) --- */
    /* +0x00: vtable is compiler-managed */
    /* +0x04: void*   hInstance */
    void*    hWnd;                        /* +0x08 — window handle */

    /* --- Panel-specific fields --- */
    int32_t  scroll_offset_x;             /* +0x100 — scroll offset X for grid */
    int32_t  scroll_offset_y;             /* +0x104 — scroll offset Y for grid */

    /* Source/grid area rectangle */
    int32_t  src_rect_left;               /* +0x120 — grid source rect left */
    int32_t  src_rect_top;                /* +0x124 — grid source rect top */
    int32_t  src_rect_right;              /* +0x128 — grid source rect right */
    int32_t  src_rect_bottom;             /* +0x12C — grid source rect bottom */

    /* Selection state */
    int32_t  selected_index;              /* +0x270 — currently selected player index */
    uint8_t  selection_active;            /* +0x274 — non-zero if selection is active */

    uint8_t  _pad_275[9];                 /* +0x275 padding to +0x27E */
    uint8_t  sprites_initialized;         /* +0x27E — 1 after InitSprites called */

    /* Panel surfaces */
    void*    panel_surface;               /* +0x280 — UIPANEL surface for grid rendering */
    void*    main_surface;                /* +0x288 — surface from resource 0x3d87 */
    void*    main_resource;               /* +0x28C — resource 0x3d87 */
    void*    selection_surface;           /* +0x290 — surface from resource 0x3d88 */
    void*    selection_resource;          /* +0x294 — resource 0x3d88 */

    /* Sprites */
    void*    main_sprite;                 /* +0x298 — main UISprite (building icon bg) */
    void*    grid_sprites[9];             /* +0x29C — 9 sprites for 3x3 grid cells */
    void*    selection_sprite;            /* +0x2C0 — selection frame UISprite */

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Initialize sprite resources for the building panel.
     * Address: 0x431270
     *
     * Loads bitmaps 0x3d87 and 0x3d88 as panel background/selection
     * resources. Initializes 11 sprites: main_sprite (+0x298),
     * selection_sprite (+0x2C0), and 9 grid sprites (+0x29C).
     * Sets sprites_initialized (+0x27E) to 1.
     *
     * Called by: PostcardPreviewWindow_DtorBody at 0x430D90
     *
     * @return  void
     */
    void init_sprites();

    /**
     * Draw one building entry in the selection grid.
     * Address: 0x431560
     *
     * Sets sprite rect/state for the given cell position, draws the
     * building name using GDI DrawTextA (font: g_font_normal), and
     * appends a colored player indicator dot via DrawColorDot.
     *
     * @param cell_left   Cell left edge (screen X)
     * @param cell_top    Cell top edge (screen Y)
     * @param cell_right  Cell right edge (screen X)
     * @param cell_bottom Cell bottom edge (screen Y)
     * @param player_idx  Player index (0..max) for the building entry
     */
    void draw_item(LONG cell_left, int cell_top, int cell_right, int cell_bottom, int player_idx);

    /**
     * Render the full building selection grid.
     * Address: 0x4316F0
     *
     * Draws the grid with horizontal and vertical separator lines,
     * then iterates rows x columns of building icons. For each cell:
     *   - Draws building icon via DrawIcon
     *   - Draws occupant dots via DrawOccupantDots
     *   - Sets sprite state based on occupancy
     *
     * Grid dimensions:
     *   Columns: g_netman->playerCount
     *   Rows: g_netman->currentPlayer
     *   Cell size: 192x144 pixels (0xC0 x 0x90)
     *   Stride: 0xC1 (192+1) horizontal, 0x91 (144+1) vertical
     */
    void render_grid();

    /**
     * Draw occupant indicator dots on a building icon in the grid.
     * Address: 0x431B30
     *
     * Iterates the linked list at BuildingEntry+0x38 (occupant list),
     * drawing colored ellipses for each occupant. Dot size is 7x7 if
     * the building is selected (+0x274 != 0 && +0x270 == occupant_index),
     * or 5x5 otherwise.
     *
     * Colors are selected from an 8-entry palette based on
     * (occupant_type % playerCount) + (occupant_type / playerCount) * 3.
     *
     * Uses GDI: BeginPaint, CreatePen, CreateSolidBrush, Ellipse, SetPixel.
     *
     * @param cell_rect  Pointer to cell RECT (screen coordinates)
     * @param player_idx Pointer to player index for the building entry
     */
    void draw_occupant_dots(int* cell_rect, int* player_idx);

    /**
     * Draw a colored circular player indicator dot.
     * Address: 0x431ED0
     *
     * Draws a colored dot (12x12 small, 18x18 selected) using GDI
     * Ellipse with a white highlight pixel. Color is selected from the
     * same 8-entry palette as DrawOccupantDots.
     *
     * @param hdc         GDI device context handle
     * @param x           Dot left/center X position
     * @param y           Dot top/center Y position
     * @param player_idx  Player index for color selection
     */
    void draw_color_dot(HDC hdc, int x, int y, int player_idx);
};

/* ================================================================ */
/* Free functions                                                    */
/* ================================================================ */

/**
 * Draw a building's bitmap icon onto the grid cell.
 * Address: 0x431A10
 *
 * Loads pixel data from BuildingEntry+0x44 (icon bitmap data),
 * creates a temp UIPANEL surface, copies pixels, blits with
 * alpha 0x10, then releases the surface.
 *
 * BuildingEntry data layout:
 *   +0x00: player name string (5+ bytes from +5 offset)
 *   +0x10: icon width (short)
 *   +0x11: (byte, player color?)
 *   +0x32: something...
 *   +0x38: occupant list head (linked list)
 *   +0x40: icon data size (int)
 *   +0x42: icon height (short)
 *   +0x44: icon pixel data pointer
 *
 * @param cell_rect    Pointer to cell RECT for positioning
 * @param player_index Pointer to player index
 */
void BuildingPanel_DrawIcon(uint* cell_rect, int* player_index);

/**
 * BuildingPanel window procedure (__stdcall, registered at 0x477E4C).
 * Address: 0x4324F0
 *
 * Intercepts WM_SYSCOMMAND/0xF140 (SC_SCREENSAVE / Alt key) to
 * switch to town mode (CGWND_SetMode(3)) and post quit message
 * instead of activating screensaver. All other messages forwarded
 * to DefWindowProcA.
 *
 * NOTE: Even after intercepting the scrensaver message, the function
 * still passes through to DefWindowProcA and returns its result.
 *
 * @param hWnd   Window handle
 * @param msg    Window message
 * @param wParam WPARAM
 * @param lParam LPARAM
 * @return       LRESULT from DefWindowProcA
 */
LRESULT __stdcall BuildingPanel_WndProc(HWND hWnd, UINT msg, uint32_t wParam, LPARAM lParam);
