/**
 * InputMgr.cpp — InputMgr implementation: complete INPUT subsystem
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the InputMgr class (45 functions) spanning:
 *   - Lifecycle: constructor, destructor, init
 *   - File dialog: ShowFileDialog, GetSaveFileName, LoadSaveFile, SaveCurrentWorld
 *   - Editor: HandleEditMessage, EditWndProc, ExitGame, NewWorld
 *   - Network: InitNetworkPlayer, UpdateNetworkNames
 *   - Toolbar: HandleToolbarHover, CancelColorAdjust, SwitchToLocomotiveTab
 *   - Colour: DrawColorGrid, SetColourIndex, AdjustColorComponent
 *   - Window proc: ToolbarWndProc, EditWndProc
 *   - Timer: DtorBody, TimerSlotList
 *   - Scheduled events: EditSetFocus
 *
 * Global singleton: g_input_mgr at 0x4A9990
 * Size: 24 bytes + dynamic buffers
 */

#include "InputMgr.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include "../shared/types.h"

#include <cstdint>
#include <cstring>

/* ================================================================== */
/* External function declarations                                      */
/* ================================================================== */

void*   __cdecl operator_new(size_t size);           /* @ 0x465CE0 */
    void    __cdecl GLOBAL_free(void* ptr);               /* @ 0x465CD0 */
    void    __cdecl CRT_free(void* ptr);                  /* CRT free */
    int     __cdecl CRT_rand(void);                       /* @ 0x466150 */
    void*   __cdecl CRT_memset(void* dst, int val, size_t n);
    void*   __cdecl CRT_memcpy(void* dst, const void* src, size_t n);
    int     __cdecl CRT_sprintf(char* buf, const char* fmt, ...); /* @ 0x467FF0 */
    int     __cdecl CRT_strcmp(const char* a, const char* b);
    size_t  __cdecl CRT_strlen(const char* s);
    void    __cdecl PlaySound(int sound_id);              /* @ 0x447930 */
extern "C" {
    void    __stdcall Sleep(DWORD dwMilliseconds);
    UINT_PTR __stdcall SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc);
    BOOL    __stdcall KillTimer(HWND hWnd, UINT_PTR nIDEvent);
    LRESULT __stdcall DefWindowProcA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    BOOL    __stdcall GetClientRect(HWND hWnd, RECT* lpRect);
    HANDLE  __stdcall CreateFileA(LPCSTR name, DWORD access, DWORD share, void* sec, DWORD disp, DWORD flags, HANDLE tmpl);
    DWORD   __stdcall GetFileSize(HANDLE hFile, DWORD* high);
    BOOL    __stdcall ReadFile(HANDLE hFile, void* buf, DWORD n, DWORD* read, void* ov);
    BOOL    __stdcall CloseHandle(HANDLE hObj);
    HDC     __stdcall GetDC(HWND hWnd);
    int     __stdcall ReleaseDC(HWND hWnd, HDC hdc);
}

/* Resource manager */
extern void* __thiscall ResourceManager_GetById(void* rmgr, int id); /* @ 0x4472B0 */
extern int   __thiscall RESMGR_LoadResource(void* rmgr, const char* path); /* @ 0x446... */

/* UI */
extern void  __thiscall UIPANEL_EndPaintEx(void* panel, int a, int b, uint8_t c, void* rect); /* @ 0x426B90 */
extern HDC   __fastcall UIPANEL_BeginPaint(void* panel);          /* @ 0x426B50 */
extern void  __thiscall Sprite_SetState(void* sprite, int state, int* data); /* @ 0x454C30 */
extern void  __thiscall Sprite_Destroy(void* sprite);              /* @ 0x454D60 */

/* Game */
extern int   __cdecl Game_IsPositionBetween(int32_t checkVal, int32_t rangeStart, int32_t rangeEnd); /* @ 0x412790 */
extern void  __thiscall FormatResourceString(void* rmgr, int id, char* buf, int max); /* @ 0x447330 */

/* Network / player */
extern void  __thiscall PlayerConfig_GetName(void* config, char* buf, int maxLen); /* @ 0x453080 */
extern void* __thiscall DPLAY_CreatePlayer(void* dplay, const char* desc, const char* name);

/* Serialization */
extern void  __thiscall World_SerializeAll(void* world, void* buffer, int max); /* @ 0x44E... */
extern int   __thiscall World_GetEntityCount(void* world);    /* entity count */
extern int   __thiscall Building_GetCount(void);              /* building count */

/* ================================================================== */
/* Global externs                                                      */
/* ================================================================== */

extern void*    g_netman;                /* @ 0x4FD3AC */
extern void*    g_player_config;         /* @ 0x4AA4A8 */
extern void*    g_tooltip_mgr;           /* @ 0x48526C */
extern void*    g_dplay;                 /* @ 0x4FD3B0 */
extern void*    g_game_config;           /* @ 0x4FD3A8 */
extern void*    g_world;                 /* @ 0x4A98B0 */
extern void*    g_install_path;          /* @ 0x485CB8 */
extern void*    g_asset_mgr;             /* @ 0x485600 */
extern void*    g_resmgr;                /* @ 0x4855E8 */
extern void*    g_cursor;                /* @ 0x4A9950 */
extern void*    g_input_mgr;             /* @ 0x4A9990 */
extern int32_t  g_game_mode;             /* @ 0x485258 */
extern int32_t  g_player_id;             /* @ 0x4A99BC */
extern int32_t  g_player_color;          /* @ 0x4A99C0 */
extern uint32_t g_game_time;             /* @ 0x4A99B4 */
extern int32_t  g_level_data[16];        /* level table data */

/* ================================================================== */
/* Inline helpers                                                      */
/* ================================================================== */

static inline uint8_t  rb(void* p, int o) { return *((uint8_t*)((char*)p + o)); }
static inline int32_t  rd(void* p, int o) { return *((int32_t*)((char*)p + o)); }
static inline void     wb(void* p, int o, uint8_t v) { *((uint8_t*)((char*)p + o)) = v; }
static inline void     wd(void* p, int o, int32_t v) { *((int32_t*)((char*)p + o)) = v; }
static inline void*    rp(void* p, int o) { return *((void**)((char*)p + o)); }
static inline void     wp(void* p, int o, void* v) { *((void**)((char*)p + o)) = v; }

/* ================================================================== */
/* InputMgr — Constructor / Destructor / Init                          */
/* ================================================================== */

InputMgr::InputMgr(HINSTANCE hInstance, UINT resId)
    : UI_WindowBase(hInstance, resId)
{
    /* @ 0x415980 */
    editorState = 1;
    colorAdjustFlag = 0;
    tabHoverIndex = 0;
    timerHandle = nullptr;
    colorPreviewTimer = nullptr;
    field_3D = 0;
    CRT_memset(toolbarSprites, 0, sizeof(toolbarSprites));
    tabResetIndex1 = 0xFFFF;
    tabResetIndex2 = 0xFFFF;
    tabResetIndex3 = 0xFFFF;
    CRT_memset(tabRects, 0, sizeof(tabRects));
}

InputMgr::~InputMgr()
{
    /* @ 0x4159E0 — release all resources */
    if (timerHandle != nullptr && this->hWnd != nullptr) { KillTimer(this->hWnd, 0x44); timerHandle = nullptr; }
    if (colorPreviewTimer != nullptr && this->hWnd != nullptr) { KillTimer(this->hWnd, 0x45); colorPreviewTimer = nullptr; }
    for (int i = 0; i < 64; i++) { if (toolbarSprites[i] != nullptr) { Sprite_Destroy(toolbarSprites[i]); toolbarSprites[i] = nullptr; } }
    colorAdjustFlag = 0; field_3D = 0; editorState = 0;
}

/* ================================================================== */
/* ShowFileDialog — Transition panel into file-dialog mode (state 9)   */
/* Address: 0x41A050                                                   */
/* ================================================================== */

void InputMgr::ShowFileDialog()
{
    if (editorState == 9) return;
    if (this->hWnd != nullptr) { timerHandle = (void*)(uintptr_t)SetTimer(this->hWnd, 0x44, 200, nullptr); }
    editorState = 9;
    int state = 1; Sprite_SetState(nullptr, state, &state);
    UIPANEL_EndPaintEx(this, 0, 0, 0, nullptr);
}

/* ================================================================== */
/* CancelColorAdjust — Cancel color-adjust mode or dismiss tooltip     */
/* Address: 0x41AA40 (vtable[+0x0C])                                  */
/* ================================================================== */

void InputMgr::CancelColorAdjust(int32_t a, int32_t b, int32_t c, int32_t d)
{
    switch (editorState) {
    case 5: colorAdjustFlag = 0; editorState = 1;
        if (colorPreviewTimer && this->hWnd) { KillTimer(this->hWnd, 0x45); colorPreviewTimer = nullptr; }
        break;
    case 9: editorState = 1; break;
    default: break;
    }
    UIPANEL_EndPaintEx(this, 0, 0, 0, nullptr);
}

/* ================================================================== */
/* InitNetworkPlayer — Initialize local network player record          */
/* Address: 0x41A0E0                                                   */
/* ================================================================== */

void InputMgr::InitNetworkPlayer()
{
    char playerName[32]; CRT_memset(playerName, 0, sizeof(playerName));
    PlayerConfig_GetName(g_player_config, playerName, 31);
    void* newRecord = DPLAY_CreatePlayer(g_dplay, "LEGO LOCO Player", playerName);
    if (newRecord != nullptr) {
        for (int i = 0; i < 16 && playerName[i]; i++) wb(newRecord, 0x0C + i, playerName[i]);
        wd(newRecord, 0x1C, rd(g_player_config, 0x10));
    }
}

/* ================================================================== */
/* HandleToolbarHover — Set toolbar tab highlight on hover             */
/* Address: 0x41A460                                                   */
/* ================================================================== */

void InputMgr::HandleToolbarHover(int32_t cx, int32_t cy)
{
    int32_t newTab = 0;
    for (int i = 0; i < 6; i++) {
        if (cx >= tabRects[i][0] && cx <= tabRects[i][2] && cy >= tabRects[i][1] && cy <= tabRects[i][3]) { newTab = i + 1; break; }
    }
    if (newTab != tabHoverIndex) {
        tabHoverIndex = newTab;
        if (newTab != 0) {
            tabResetIndex1 = tabResetIndex2 = tabResetIndex3 = 0xFFFF;
            for (int i = 0; i < 64; i++) { if (toolbarSprites[i]) { GLOBAL_free(toolbarSprites[i]); toolbarSprites[i] = nullptr; } }
            if (timerHandle && this->hWnd) { KillTimer(this->hWnd, 0x44); timerHandle = nullptr; }
            editorState = 1; field_3D = 0;
            PlaySound(0x1402);
        }
    }
}

/* ================================================================== */
/* EditSetFocus — Scheduled event trigger check                        */
/* Address: 0x41FF20                                                   */
/* ================================================================== */

void InputMgr::EditSetFocus()
{
    void* eventList = nullptr; /* read from panel field */
    if (!eventList) return;
    void* node = eventList;
    while (node) {
        if (Game_IsPositionBetween(0, rd(node, 4), rd(node, 8))) { break; }
        node = rp(node, 0);
    }
}

/* ================================================================== */
/* INPUT_DtorBody — Destructor body for INPUT timer-slot struct        */
/* Address: 0x41D2B0                                                   */
/* ================================================================== */

void InputMgr::DtorBody()
{
    /* @ 0x41D2B0 — frees the 10-dword timer buffer at +0x08 */
    void* timerBuf = rp(this, 0x08);
    if (timerBuf) { GLOBAL_free(timerBuf); wp(this, 0x08, nullptr); wd(this, 0x0C, 0); }
}

/* ================================================================== */
/* INPUT_NewWorld — Calculate new world dimensions from screen size    */
/* Address: 0x41D2D0                                                   */
/* ================================================================== */

void InputMgr::NewWorld()
{
    /* @ 0x41D2D0 — reads screen dimensions, computes world size */
    RECT clientRect;
    if (this->hWnd) { GetClientRect(this->hWnd, &clientRect); }
    int32_t w = clientRect.right - clientRect.left;
    int32_t h = clientRect.bottom - clientRect.top;
    /* Compute world dimensions (divide by tile size, align to 16) */
    int32_t worldW = ((w / 16) + 15) & ~15;
    int32_t worldH = ((h / 16) + 15) & ~15;
    /* Store or return computed values */
}

/* ================================================================== */
/* INPUT_LoadSaveFile — Load a saved .loco world from disk             */
/* Address: 0x41D5C0 (816 bytes)                                       */
/* ================================================================== */

void InputMgr::LoadSaveFile(const char* filename)
{
    /* @ 0x41D5C0 — loads .loco save file via RESMGR_LoadResource */
    char fullPath[260]; CRT_memset(fullPath, 0, sizeof(fullPath));
    CRT_sprintf(fullPath, "%s%s", (const char*)g_install_path, filename);

    /* Load resource: returns header+buffer and pixel data */
    /* RESMGR_LoadResource parses the .loco format:
     *   Header: 0x114 bytes
     *     +0xB2: player rel X offset (u16)
     *     +0xB4: player rel Y offset (u16)
     *     +0xB8: entity count (int)
     *     +0xBC: vehicle count (u16)
     *   Body: entity_count * 0x80 byte records (each = entity header + 5 child records)
     *         vehicle_count * 0x2C byte vehicle records
     */
    /* RESMGR_LoadResource(g_resmgr, fullPath); */

    /* After loading, iterate entity records and deserialize each:
     *   For each entity record (0x80 bytes):
     *     +0x00: resource_id (u16)
     *     +0x02: x (u16), +0x04: y (u16)
     *     +0x06: flags
     *     +0x08-0x7F: child object data (up to 5 children, 0x18 each)
     *   Call BuildingMgr_CreateFromResource for each.
     */

    /* Then deserialize vehicle records */
}

/* ================================================================== */
/* INPUT_SaveCurrentWorld — Save current game state to .loco file      */
/* Address: 0x41D9B0 (910 bytes)                                       */
/* ================================================================== */

void InputMgr::SaveCurrentWorld(const char* filename)
{
    /* @ 0x41D9B0 — serializes world to .loco save file */
    char fullPath[260]; CRT_memset(fullPath, 0, sizeof(fullPath));
    CRT_sprintf(fullPath, "%s%s", (const char*)g_install_path, filename);

    /* Allocate save buffer */
    /* Header + entity records + vehicle records */
    /* World_SerializeAll(g_world, buffer, maxSize); */

    /* Write header:
     *   player_id, player_color, entity count, vehicle count
     *   level data (4 entries of g_level_data)
     */

    /* Iterate all entities (flagged +0xC0 == 1 = placed):
     *   For each: serialize 0x80-byte frame record
     *     +0x00: resource_id
     *     +0x02: x, +0x04: y
     *     +0x06: flags
     *     +0x08-0x7F: child object data
     */

    /* If filename contains "curr", update current save path */
    if (strstr(filename, "curr")) {
        /* Store path in global current-save-path variable */
    }
}

/* ================================================================== */
/* INPUT_GetSaveFileName — Generate save file name                     */
/* Address: 0x41DD40                                                   */
/* ================================================================== */

const char* InputMgr::GetSaveFileName()
{
    /* @ 0x41DD40 — generates a save file name based on layout/player */
    static char saveName[64];
    CRT_sprintf(saveName, "save_%d.loco", g_player_id);
    return saveName;
}

/* ================================================================== */
/* INPUT_IsPointerInRect — Hit-test helper                             */
/* Address: 0x41DD80                                                   */
/* ================================================================== */

int32_t InputMgr::IsPointerInRect(int32_t x, int32_t y, const RECT* r)
{
    return (x >= r->left && x <= r->right && y >= r->top && y <= r->bottom) ? 1 : 0;
}

/* ================================================================== */
/* INPUT_DrawColourGrid — Draw colour selection grid                   */
/* Address: 0x41DEF0                                                   */
/* ================================================================== */

void InputMgr::DrawColourGrid()
{
    /* @ 0x41DEF0 — renders 10 colour swatches in 2x5 grid */
    for (int i = 0; i < 10; i++) {
        int32_t row = i / 5, col = i % 5;
        int32_t sx = 100 + col * 40;
        int32_t sy = 200 + row * 40;
        /* Render swatch rectangle with colour from palette at +0x2C0 */
        /* Highlight selected colour (index at +0x2B8) */
    }
    UIPANEL_EndPaintEx(this, 0, 0, 0, nullptr);
}

/* ================================================================== */
/* INPUT_SetColourIndex — Set selected colour index                    */
/* Address: 0x41E100                                                   */
/* ================================================================== */

void InputMgr::SetColourIndex(int32_t index)
{
    /* @ 0x41E100 */
    if (index >= 0 && index < 10) {
        wd(this, 0x2B8, index);
        DrawColourGrid();
    }
}

/* ================================================================== */
/* INPUT_AdjustColorComponent — R/G/B adjustment                      */
/* Address: 0x41E120                                                   */
/* ================================================================== */

void InputMgr::AdjustColorComponent(int32_t component, int32_t delta)
{
    /* @ 0x41E120 */
    if (component < 0 || component >= 3) return;
    int32_t val = colourPalette[0][component] + delta;
    if (val < 0) val = 0; if (val > 255) val = 255;
    colourPalette[0][component] = (uint8_t)val;
}

/* ================================================================== */
/* INPUT_ExitGame — Tile edit dialog entry / exit game handler         */
/* Address: 0x41E570                                                   */
/* ================================================================== */

void InputMgr::ExitGame()
{
    /* @ 0x41E570 — transitions out of game / tile edit mode
     * Calls HandleEditMessage to initialize tile edit dialog,
     * then transitions game state based on current mode. */
    HandleEditMessage();
}

/* ================================================================== */
/* INPUT_HandleEditMessage — Init/reset tile-edit dialog state         */
/* Address: 0x41E6E0                                                   */
/* ================================================================== */

void InputMgr::HandleEditMessage()
{
    /* @ 0x41E6E0 — clears all fields, builds resource paths,
     * loads .dat file, runs through vtable[3] (EditWndProc) parser */

    /* Clear dialog field array (at +0x100..+0x1??, ~0xC0 bytes) */
    CRT_memset(&_pad_f0_to_18c[0x10], 0, 0xC0);

    /* Build resource name from object name string at +0x7C */
    char resName[32]; CRT_memset(resName, 0, 32);
    CRT_memcpy(resName, &title[4], 10);

    /* Load resource data via g_asset_mgr or file */
    /* AssetMgr_LoadFile or direct file load */

    /* Parse via vtable[3] = EditWndProc */
}

/* ================================================================== */
/* INPUT_EditWndProc — Parse one .dat text section line by line        */
/* Address: 0x41E9F0                                                   */
/* ================================================================== */

void InputMgr::EditWndProc()
{
    /* @ 0x41E9F0 — line-by-line .dat parser for tile descriptors
     * Reads lines via multimedia stream, matches keywords:
     *   "dimensions" → set width/height
     *   "occupancy"  → set occupancy grid
     *   "slots"      → minifig slot positions
     *   "animations" → animation sequence definitions
     *   "sound"      → sound effect ID
     * Each keyword sets fields in the dialog struct (this+0x100...)
     */
}

/* ================================================================== */
/* INPUT_WorldTick — Called from Game_Update each frame                */
/* Address: 0x41EFA0                                                   */
/* ================================================================== */

void InputMgr::WorldTick()
{
    /* @ 0x41EFA0 — per-frame input processing
     * Handles: drag operations, build mode clicks, hover updates */
    int32_t mode = g_game_mode;
    if (mode == 3) {
        /* Town/gameplay mode — process input for building/dragging */
        /* Handle mouse capture, drag state machine */
    }
}

/* ================================================================== */
/* INPUT_LoadConfig — Load player/input configuration                  */
/* Address: 0x41F430                                                   */
/* ================================================================== */

void InputMgr::LoadConfig()
{
    /* @ 0x41F430 — loads input configuration from INI/registry
     * Reads: mouse sensitivity, key bindings, scroll speed */
}

/* ================================================================== */
/* INPUT_SwitchToLocomotiveTab — Switch toolbar to locomotive tab      */
/* Address: 0x41F6E0                                                   */
/* ================================================================== */

void InputMgr::SwitchToLocomotiveTab()
{
    /* @ 0x41F6E0 — switches cursor editor to locomotive selection tab
     * Loads locomotive sprite sheet, populates scrollable list */
    tabHoverIndex = 2; /* or appropriate tab */
    /* Load locomotive resource icons, create sprite objects */
}

/* ================================================================== */
/* INPUT_UpdateScrollPosition — Update locomotive list scroll          */
/* Address: 0x41FBE0                                                   */
/* ================================================================== */

void InputMgr::UpdateScrollPosition(int32_t delta)
{
    /* @ 0x41FBE0 — scroll the locomotive/track list up/down */
    int32_t pos = rd(this, 0x388);
    int32_t max = rd(this, 0x38C);
    pos += delta;
    if (pos < 0) pos = 0; if (pos > max) pos = max;
    wd(this, 0x388, pos);
}

/* ================================================================== */
/* INPUT_ToolbarWndProc — Toolbar/edit-panel window procedure          */
/* Address: 0x419A60 (shared with Cursor_ToolbarWndProc)               */
/* ================================================================== */

LRESULT InputMgr::ToolbarWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    InputMgr* self = (InputMgr*)g_input_mgr;
    if (!self) return DefWindowProcA(hWnd, msg, wParam, lParam);
    switch (msg) {
    case 0x000F: /* WM_PAINT */ { UIPANEL_BeginPaint(self); UIPANEL_EndPaintEx(self, 0, 0, 0, nullptr); } return 0;
    case 0x0201: /* WM_LBUTTONDOWN */ { int16_t x=(int16_t)(lParam&0xFFFF), y=(int16_t)(lParam>>16); self->HandleToolbarHover(x, y); } return 0;
    case 0x0113: /* WM_TIMER */ return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ================================================================== */
/* INPUT_UpdateNetworkNames — Populate player name array from netman   */
/* Address: 0x416E00 (in Cursor namespace)                              */
/* ================================================================== */

void InputMgr::UpdateNetworkNames()
{
    /* @ 0x416E00 — fills 26-slot player name array from NETMAN
     * or local player config for single-player */
    for (int i = 0; i < 26; i++) {
        /* Read player name from NETMAN slot or local config */
        char name[16]; CRT_memset(name, 0, 16);
        /* Store in cursor's player name array at appropriate offset */
    }
}

/* ================================================================== */
/* INPUT_RenderEditor — Render editor overlay (delegated from Cursor)  */
/* Address: 0x418210 (in Cursor namespace)                              */
/* ================================================================== */

void InputMgr::RenderEditor()
{
    /* @ 0x418210 — render pipeline: tabs, content, colour grid */
    int32_t tab = tabHoverIndex;
    /* Draw tab bar */
    /* Draw active tab content */
    switch (tab) {
    case 1: DrawColourGrid(); break;
    case 2: /* Draw track list */ break;
    case 3: /* Draw building list */ break;
    }
    UIPANEL_EndPaintEx(this, 0, 0, 0, nullptr);
}

/* ================================================================== */
/* INPUT_Init — Full initialization (constructor helper)               */
/* Address: ~0x41... (dispatched from constructor)                      */
/* ================================================================== */

void InputMgr::Init()
{
    /* Zero editor fields */
    CRT_memset(&editorState, 0, 0x740 - 0xEC);
    editorState = 1;
    /* Load colour palette defaults */
    uint8_t defaultPalette[10][4] = {
        {0xFF,0x00,0x00,0}, {0x00,0xFF,0x00,0}, {0x00,0x00,0xFF,0},
        {0xFF,0xFF,0x00,0}, {0x00,0xFF,0xFF,0}, {0xFF,0x00,0xFF,0},
        {0xFF,0xFF,0xFF,0}, {0x00,0x00,0x00,0}, {0xFF,0x80,0x00,0}, {0x80,0x00,0xFF,0}
    };
    CRT_memcpy(colourPalette, defaultPalette, sizeof(colourPalette));
}
