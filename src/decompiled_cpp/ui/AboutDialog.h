/**
 * AboutDialog.h — About/Credits dialog and idle screensaver class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * AboutDialog renders the Lego Loco About/Credits screen, which doubles
 * as the idle screensaver. The dialog manages:
 *   - A scrolling credits text display (loaded from a WVE resource file)
 *   - An optional background image parsed from the credits file
 *   - Screensaver fade-in/out overlay animation
 *   - Credits music playback (resource 0x5597)
 *
 * The object is heap-allocated (0x1184 bytes) during CGWND_InitAllSubsystems
 * (subsystem index 7) and stored in the global g_about (0x4FD390). It is
 * created hidden; when the game goes idle (no user input for a timeout),
 * the screensaver hides the cursor and begins calling Update() which
 * scrolls the credits text and renders it via RenderCredits().
 *
 * Credits text is loaded from a WVE animation file. The text format
 * supports plain text lines, a '<number>' tag at the start of the first
 * line for background image resource ID, and '*' comment/separator lines
 * (which are skipped).
 *
 * Size: 0x1184 bytes (4484 bytes)
 * Vtable: 0x477680 (VTBL_ABOUTDIALOG)
 *
 * Class hierarchy:
 *   GameWindow  (base, vtable 0x477898)
 *     └─ AboutDialog  ← this class (vtable 0x477680)
 *
 * Vtable layout (8 entries):
 *   [0] +0x00: scalar deleting destructor  (CGWND_AboutDialog_Dtor,    0x40F270)
 *   [1] +0x04: Hide                        (CGWND_Screensaver_Hide,    0x40F480)
 *   [2] +0x08: Show                        (CGWND_AboutDialog_Show,    0x40F2A0)
 *   [3] +0x0C: set_mode                    (inherited: Cursor_SetMode, 0x414340)
 *   [4] +0x10: method_4                    (inherited: stub,           0x426130)
 *   [5] +0x14: Create                      (inherited: GameWindow_Create, 0x413DE0)
 *   [6] +0x18: Init / update_client_rect   (CGWND_AboutDialog_Init,    0x40F5C0)
 *   [7] +0x1C: method_7                    (CGWND_AboutDialog_m7,      0x40F890)
 */

#pragma once

#include "../ui/GameWindow.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
class AboutDialog : public GameWindow {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* GameWindow base fields: +0x00..+0x117 — see GameWindow.h */

    /* Screensaver / credits state */
    int32_t    fade_timer;              // +0x118  fade timer (computed as scroll_accum / 10)
                                         //         Used in Game_RenderScreensaver for
                                         //         fade-in (0-7), full display (8-84),
                                         //         fade-out (85-92).

    int32_t    scroll_timer;            // +0x11C  scroll speed timer; initialized to -10,
                                         //         incremented by 2 each frame until >= 15.
                                         //         Drives the scroll_accum accumulator.

    int32_t    scroll_accum;            // +0x120  scroll accumulator; adds scroll_timer
                                         //         each frame. When >= 1000 - scroll_timer,
                                         //         triggers a RenderCredits call.

    uint32_t   timer_id;                // +0x124  Windows timer ID from SetTimer (killed in Hide).
                                         //         Initialized to 0 in constructor.

    int32_t    frame_counter;           // +0x128  screensaver frame counter; incremented
                                         //         each update cycle, passed as HDC-like value
                                         //         to RenderCredits. Reset to 1 after
                                         //         RenderCredits returns 0.

    uint8_t    sprites_initialized;     // +0x12C  byte: 1 = screensaver sprites loaded.
                                         //         Checked by InitSprites and Hide.

    uint8_t    panel_active;            // +0x12D  byte: 1 = rendering panel active.
                                         //         Set by Show, cleared by Hide.

    uint8_t    _pad_12E[2];             // +0x12E  padding

    void*      res_surface;             // +0x130  surface handle from screensaver resource
                                         //         (0x3DAF). Used in Screensaver_InitSprites.

    void*      res_object;              // +0x134  screensaver resource object pointer
                                         //         (resource 0x3DAF). Released via vtable[2]
                                         //         in Hide.

    int32_t    offset_x;                // +0x138  X offset for dark fade overlay rect
                                         //         (used in Game_RenderScreensaver).

    int32_t    offset_y;                // +0x13C  Y offset for dark fade overlay rect.

    /* +0x140..+0x147: gap (8 bytes) */

    uint16_t   scroll_pos;              // +0x148  scroll Y position (word).
                                         //         Read in RenderCredits for initial offset.

    uint8_t    _pad_14A[2];             // +0x14A  padding

    void*      panel;                   // +0x14C  UI rendering panel pointer (UIPANEL).
                                         //         Created by Show, destroyed in Hide.
                                         //         Used as rendering target in RenderCredits.

    void*      hIcon;                   // +0x150  HICON window icon (loaded from resource 0x65).

    char       credits_text[0x1000];    // +0x154  Credits text buffer (4096 bytes).
                                         //         Loaded by LoadCredits, zeroed in ctor.

    /* Credits rendering state */
    uint32_t   background_res_id;       // +0x1154 resource ID for background image
                                         //         (parsed from '<number>' tag in credits file).
                                         //         Zeroed in ctor, parsed by RenderCredits.

    int32_t    field_1158;              // +0x1158 (unknown; zeroed in ctor, set in InitSprites
                                         //         to UIPANEL surface handle).

    RECT       scroll_rect;             // +0x115C blit rect for scrolling background.
                                         //         SetRectEmpty in ctor, updated by
                                         //         RenderCredits / Game_RenderScreensaver.

    void*      res_credits_obj;         // +0x116C resource object pointer for credits
                                         //         background image. Loaded by RenderCredits.
                                         //         Released via vtable[2] in RenderCredits
                                         //         and Hide.

    void*      res_credits_data;        // +0x1170 surface/data handle from credits
                                         //         background image resource.
                                         //         Used as UIPANEL_Blit source.

    RECT       image_rect;              // +0x1174 positioning rect for credits background
                                         //         image. Init in RenderCredits to
                                         //         (0, 0, frame_w, frame_h), then
                                         //         OffsetRect to center in 216x196 area.

    /* Total size: 0x1184 bytes (verified by operator_new in CGWND_InitAllSubsystems) */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * AboutDialog constructor.
     * Address: 0x40F1C0
     *
     * Calls GameWindow base constructor, then initializes all
     * AboutDialog-specific fields to defaults. Sets vtable to
     * VTBL_ABOUTDIALOG (0x477680).
     *
     * Called by: CGWND_InitAllSubsystems @ 0x407638
     *
     * @param hInstance  Application instance handle
     * @param resId      Window resource ID (0x1FD = 509 for Credits dialog)
     */
    AboutDialog(HINSTANCE hInstance, uint32_t resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x40F270
     *
     * Calls base_destructor, then frees heap memory if flags & 1.
     *
     * @param flags  Delete flag (bit 0 = free heap memory)
     * @return       This pointer (after destructor body)
     */
    virtual ~AboutDialog();

    /**
     * Base destructor body.
     * Address: 0x40F290
     *
     * Restores vtable to VTBL_ABOUTDIALOG, then calls
     * GameWindow::base_destructor.
     */
    void base_destructor();

    /* ================================================================ */
    /* Non-virtual methods                                               */
    /* ================================================================ */

    /**
     * Create the About dialog window.
     * Address: 0x40F510
     *
     * Creates the about window centered on screen at 248x232 pixels:
     *   1. Loads icon from resource 0x65 (101 IDI_APPLICATION)
     *   2. Gets desktop client rect
     *   3. Calls UI_CenterWindow for centered position
     *   4. Calls GameWindow::Create with computed position
     *
     * Called by: CGWND_InitAllSubsystems @ 0x4076DE
     *
     * @param hWndParent  Parent window HWND
     * @return            true on success, false on failure
     */
    bool Create(HWND hWndParent);

    /**
     * Render one frame of scrolling credits text.
     * Address: 0x40F980 (1212 bytes)
     *
     * Full render sequence:
     *   1. Play credits music (resource 0x5597) on first call
     *   2. Get current scroll position from +0x148
     *   3. Set up rendering via panel virtual methods
     *   4. Copy and process credits text from +0x154 into local buffer:
     *      - Skip '*' comment/separator lines and blank lines
     *      - If first char is '<', parse '<number>' as background_res_id
     *   5. Count newlines, pad text to 13 lines (centered)
     *   6. If background_res_id != 0:
     *      - Load resource via ResourceManager_GetById
     *      - Lock surface, set up centered image rect
     *      - Blit background via UIPANEL_Blit
     *   7. Render text: SetTextColor(0xFF5C00 orange), SetBkMode(TRANSPARENT),
     *      SelectObject(g_font_small), DrawTextA(DT_CENTER|DT_TOP)
     *   8. Restore GDI objects
     *
     * Called by: CGWND_Screensaver_Update @ 0x40F42E, 0x40F445
     *
     * @param hdc  HDC for GDI text rendering
     * @return     1 if credits rendered, 0 if credits buffer empty
     */
    uint32_t RenderCredits(HDC hdc);

    /**
     * Load credits text from WVE animation resource file.
     * Address: 0x40FE50 (1005 bytes)
     *
     * Opens a credits text file (first tries AssetManager resource,
     * then falls back to direct file open), reads lines of text,
     * discards line-start markers, and appends them to the
     * credits_text buffer (+0x154, max 4096 bytes). Each appended
     * line is followed by a CR/LF-style separator.
     *
     * Called by: AboutDialog Show override @ 0x40F311
     */
    void LoadCredits();
};
