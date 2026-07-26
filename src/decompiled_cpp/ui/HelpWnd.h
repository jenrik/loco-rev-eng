/**
 * HelpWnd.h — Tutorial/help window subsystem (AudioMgr / HelpWnd)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * HelpWnd is the in-game tutorial/help window that displays multi-page
 * help content with navigation (next/prev page buttons), scrollable text,
 * clickable hotspots, linked audio narration, and overlay animations.
 * It is a subclass of GameWindow and shares its lifecycle (Create, Show,
 * Hide) and rendering pipeline.
 *
 * The help data is loaded from script files which define up to 200 pages,
 * each 0x3C bytes, containing resource string IDs, sound IDs, clickable
 * RECT regions, and overlay information.
 *
 * Size: ~0x3078 bytes (HelpWnd-specific) + 0x118 (GameWindow) = 0x3190 total
 * Vtable address in loco.exe: 0x478428
 *
 * Class hierarchy:
 *   GameWindow (0x118 bytes, vtable 0x477898)
 *     └─ HelpWnd (vtable 0x478428)  ← this class
 *
 * Vtable layout (inherits 8 slots from GameWindow):
 *   [0] +0x00: scalar deleting destructor  (0x44F4F0)
 *   [1] +0x04: Hide                        (0x450AE0)
 *   [2] +0x08: Show                        (0x450240)
 *   [3] +0x0C: set_mode (Cursor dispatch)  (inherited, 0x414340)
 *   [4] +0x10: cleanup_sprites             (0x451440)
 *   [5] +0x14: Create                      (0x450CA0)
 *   [6] +0x18: init                        (0x451180)
 *   [7] +0x1C: on_show / update_anim       (0x450450)
 *
 * Called by: CGWND_AudioCreate @ 0x40FC0B (via CGWND::AudioCreate)
 *
 * NOTE: find_page, find_page_scalar_dtor, find_page_base_dtor are
 * actually HelpPageNode methods (vtable 0x4783D8), temporarily placed
 * in HelpWnd. TODO: move to separate HelpPageNode class.
 */

#pragma once

#include "GameWindow.h"
#include "../shared/types.h"

/* Forward declarations */
class ButtonSprite;
class AudioChannel;
class Town;
class PostcardAlbum;
class Cursor;
class TileMap;
class ResourceManager;
class GameAudio;
class AssetMgr;

/* ================================================================== */
/* HelpPageData — Per-page help data (0x3C bytes)                      */
/* ================================================================== */
#pragma pack(push, 1)
struct HelpPageData {
    int32_t   pageId;              // +0x00  Page resource ID / number
    int32_t   nextPageId;          // +0x04  Link to next page resource ID
    int32_t   textResId;           // +0x08  Resource string ID for page text
    int32_t   field_0C;            // +0x0C  (unknown)
    int32_t   field_10;            // +0x10  Transition/fade parameter
    uint8_t   hasOverlay;          // +0x14  Flag: 1 = page has overlay effect
    int32_t   soundResId;          // +0x18  Audio narration resource ID
    RECT      clickRect;           // +0x1C  Clickable hotspot region
    RECT      overlayRect;         // +0x2C  Overlay/indicator region
};
#pragma pack(pop)

/* ================================================================== */
/* HelpWnd class                                                        */
/* ================================================================== */

class HelpWnd : public GameWindow {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* ---- Inherited from GameWindow (0x00..0x117) ---- */
    /* See GameWindow.h for full layout. Key fields:
     *   +0x04: hInstance
     *   +0x08: hWnd (window handle)
     *   +0x38: backbufferSurface (DirectDraw surface)
     *   +0x4C: timerId (base timer)
     *   +0x58: visible (byte)
     *   +0x90: ddrawAuxCount1
     *   +0x98: ddrawAuxPtr1
     *   +0x9C: ddrawAuxCount2
     *   +0xA4: ddrawAuxPtr2
     */

    /* ---- HelpWnd-specific fields (0x118..0x3077) ---- */

    ButtonSprite* btnNext;              // +0x118  Next page button (res 0x3CFF)
    uint8_t       nextBtnEnabled;       // +0x11C  Byte flag: 1 = next button visible/enabled
    uint8_t       _pad_11D[3];          // +0x11D  Padding

    ButtonSprite* btnPrevActual;        // +0x120  Prev page button (res 0x3D00)
    uint8_t       prevBtnEnabled;       // +0x124  Byte flag: 1 = prev button visible/enabled
    uint8_t       _pad_125[3];          // +0x125  Padding

    ButtonSprite* btnClose;             // +0x128  Close/exit button (res 0x3D01)
    uint8_t       closeBtnEnabled;      // +0x12C  Byte flag: 1 = close button enabled
    uint8_t       _pad_12D[3];          // +0x12D  Padding

    ButtonSprite* btnAnim;              // +0x130  Animation/character sprite (res 0x3CFD)

    ButtonSprite* btnContent;           // +0x134  Content/object sprite (res set dynamically)

    ButtonSprite* btnTextArea;          // +0x138  Text display area sprite (res 0)

    ButtonSprite* btnTextArea2;         // +0x13C  Secondary text area sprite

    ButtonSprite* btnTextArea3;         // +0x140  Tertiary text area sprite

    uint8_t       scrollDownBtnEnabled; // +0x144  Byte: 1 = scroll-down button visible
    uint8_t       _pad_145[3];          // +0x145  Padding

    ButtonSprite* btnScrollBar;         // +0x148  Scrollbar handle sprite (res 0x3CFE)

    uint8_t       active;               // +0x14C  Byte: 1 = help window is active/visible
    uint8_t       wasFullscreen;         // +0x14D  Byte: 1 = was in fullscreen before showing
    uint8_t       spritesInited;         // +0x14E  Byte: 1 = sprites have been initialized
    uint8_t       _pad_14F;              // +0x14F  Padding

    int32_t       animFrameCount;        // +0x150  Animation frame count
    HICON         hIcon;                 // +0x154  Window icon (loaded from resource 0x65)
    AudioChannel* audioChannel;          // +0x158  Audio channel for page narration

    /* Page data array — 200 pages of 0x3C bytes each */
    HelpPageData  pages[200];           // +0x15C  Help page entries (200 * 0x3C = 0x1E00)

    /* Help window state fields at end of pages array */
    uint8_t       helpDataLoaded;        // +0x303C  Byte: 1 = help data pages loaded
    uint8_t       _pad_303D[3];          // +0x303D  Padding

    int32_t       currentPageIdx;        // +0x3040  Currently displayed page index (-1 = none)
    int32_t       scrollOffset;          // +0x3044  Text scroll offset (line number)
    int32_t       nextPageIdx;           // +0x3048  Index of next page (-1 = no next)
    int32_t       prevPageIdx;           // +0x304C  Index of prev page (-1 = no prev)

    int32_t       nextPageLinkIdx;       // +0x3050  Fallback next page link index
    int32_t       prevPageLinkIdx;       // +0x3054  Fallback prev page link index

    UINT_PTR      animTimerId;           // +0x3058  Animation timer ID (10ms, ID 0x54)
    int32_t       field_305C;            // +0x305C  (-1 init)
    int32_t       field_3060;            // +0x3060  (-1 init)
    int32_t       field_3064;            // +0x3064  (-1 init, animation frame counter)

    int32_t       lineHeight;            // +0x3068  Measured text line height in pixels
    int32_t       pageResourceType;      // +0x306C  Context resource type for page data
    int32_t       windowMode;            // +0x3070  Context mode: 1=town, 2=postcard, 3=cursor
    int32_t       returnGameMode;        // +0x3074  Game mode to restore on close (default 3)

    /* Total HelpWnd-specific size: 0x3078 bytes */
    /* Total class (GameWindow + HelpWnd): 0x118 + 0x3078 = 0x3190 bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * HelpWnd constructor. 0x44F490.
     * Chains to GameWindow(hInstance, resId) then calls init().
     */
    HelpWnd(HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]). 0x44F4F0.
     */
    virtual ~HelpWnd();

    /**
     * Base destructor body. 0x44F510.
     * Calls cleanup_sprites() then chains to GameWindow::base_destructor.
     */
    void base_destructor();

    /* ================================================================ */
    /* Core lifecycle methods (virtual — vtable dispatch entries)        */
    /* ================================================================ */

    /**
     * init — creates 9 ButtonSprite objects, zeros page array, sets state.
     * vtable[6]. 0x451180.
     */
    void init();

    /**
     * create — register WNDCLASS, create HWND, center on desktop.
     * vtable[5]. 0x450CA0.
     */
    bool create(HWND hWndParent);

    /**
     * show — display help window. vtable[2]. 0x450240.
     */
    void show(int pageTarget);

    /**
     * hide — hide help window. vtable[1]. 0x450AE0.
     */
    void hide();

    /**
     * wnd_proc — Windows message handler. 0x4518B0.
     */
    LRESULT wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * set_mode — cursor dispatch helper. vtable[3]. 0x414340.
     * Inherited from GameWindow; calls Cursor_SetMode internally.
     */
    virtual void set_mode(void* countPtr, void* dataPtr, int modeA, int modeB);

    /* ================================================================ */
    /* Page management                                                   */
    /* ================================================================ */

    /**
     * load_help_data — Parse help page data from script-file stream.
     * 0x44FC80.
     */
    int load_help_data(void* stream);

    /**
     * reset_pages — Reset all help page index/scroll state.
     * 0x44FB10.
     */
    char reset_pages();

    /**
     * load_page — Load and render a specific help page by index.
     * 0x450520.
     */
    void load_page(int pageIdx);

    /**
     * serialize_pages — Map current window mode/resource type to page index.
     * 0x44F9A0.
     */
    int serialize_pages();

    /**
     * load_page_data — Build page data key string from mode + resource type.
     * 0x44F750.
     */
    void load_page_data(char* outBuf);

    /**
     * update_scroll — Recalculate next/prev page indexes based on text.
     * 0x4500A0.
     */
    void update_scroll();

    /**
     * find_page — Init a page-list node for given resource ID.
     * 0x44F210.
     * TODO: This is actually the HelpPageNode constructor (vtable 0x4783D8).
     * Move to HelpPageNode class once class hierarchy is decompiled.
     */
    HelpPageData* find_page(int resourceId);

    /**
     * find_page_scalar_dtor — Scalar deleting destructor for HelpPageNode (vtable[0]).
     * 0x44F2A0.
     * TODO: Move to HelpPageNode class.
     */
    void* find_page_scalar_dtor(byte flags);

    /**
     * find_page_base_dtor — Base destructor for HelpPageNode (vtable[1]).
     * 0x44F2C0.
     * TODO: Move to HelpPageNode class.
     */
    void find_page_base_dtor();

    /**
     * set_page — Process page linkage event (load game object, set state).
     * 0x44F340.
     */
    void set_page();

    /**
     * play_narration — Play narration audio for current help page.
     * 0x44F560.
     */
    uint play_narration(int windowMode, uint pageResourceType);

    /* ================================================================ */
    /* Navigation                                                       */
    /* ================================================================ */

    /**
     * go_next_page — Navigate to the next help page. 0x451920.
     */
    void go_next_page();

    /**
     * go_prev_page — Navigate to the previous help page. 0x451C60.
     */
    void go_prev_page();

    /* ================================================================ */
    /* Rendering                                                        */
    /* ================================================================ */

    /**
     * render_page — Render current page text content. 0x452230.
     */
    void render_page(int* hdc_p);

    /**
     * render_scroll_up — Render scroll-up indicator. 0x452570.
     */
    void render_scroll_up(int* hdc_p);

    /**
     * render_scroll_down — Render scroll-down indicator. 0x4526B0.
     */
    void render_scroll_down(int* hdc_p);

    /**
     * highlight_button — Set button sprite to highlighted state. 0x4527B0.
     */
    void highlight_button(int buttonId);

    /**
     * update_button_states — Update button sprite visibility/state. 0x451FB0.
     */
    void update_button_states(int buttonId);

    /**
     * draw_scroll_indicator — Blit the scroll indicator to surface. 0x452B00.
     */
    void draw_scroll_indicator();

    /**
     * update_anim_sprite — Render animation sprite at frame offset. 0x452C00.
     */
    void update_anim_sprite(int frameOffset);

    /**
     * update_anim — Update animation frame tick. vtable[7] callback. 0x450450.
     */
    void update_anim();

    /**
     * draw_text — Draw one line of help text for scroll position. 0x450850.
     */
    int draw_text(int lineIdx, int* hdc_p);

    /**
     * measure_text_height — Measure height of one line of help text. 0x452170.
     */
    int measure_text_height();

    /* ================================================================ */
    /* Event handling                                                   */
    /* ================================================================ */

    /**
     * cleanup_sprites — Destroy all 9 ButtonSprite objects. vtable[4]. 0x451440.
     */
    void cleanup_sprites();

    /**
     * handle_click — Process a mouse click. 0x451540.
     */
    LRESULT handle_click(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * handle_mouse_move — Process mouse move. 0x4517B0.
     */
    LRESULT handle_mouse_move(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /* ================================================================ */
    /* Hit testing                                                      */
    /* ================================================================ */

    /**
     * hit_test — Determine which button was clicked/hovered. 0x451E90.
     * Returns button ID (1-8) or 0 for no hit.
     */
    byte hit_test(int x, int y);

    /* ================================================================ */
    /* Internal helpers                                                  */
    /* ================================================================ */

    /**
     * play_page_audio_common — Internal helper to play narration for current page.
     * Releases existing audio channel then plays new sound resource.
     */
    void play_page_audio_common();

    /**
     * refresh_all_buttons — Refresh all button states and reset animation timer.
     * Calls update_button_states for all 9 buttons.
     */
    void refresh_all_buttons();
};
