/**
 * ScriptedObject.cpp — ScriptedObject implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: TRANSCRIBED
 */

#include "ScriptedObject.h"
#include "../shared/types.h"
#include "../game/TrackPiece.h"
#include "../core/Entity.h"
#include "../town/Town.h"
#include "../resources/Win32Stream.h"
#include "../resources/Win32StreamMem.h"

/* ================================================================== */
/* Win32 API imports — C linkage only                                  */
/* ================================================================== */
/* ClientToScreen/SetCursorPos: declared by stubs/windows.h (canonical
 * POINT*-typed signature) — transitively included below via
 * resources/Win32Stream.h. This file's own former duplicate declaration
 * of ClientToScreen used a looser `void* point` parameter, which
 * conflicted once both headers landed in the same TU (same fix as
 * ui/HelpWnd.cpp's GetClientRect/SetRect/etc. cleanup). */

/* ================================================================== */
/* CRT helpers — C++ linkage                                            */
/* ================================================================== */
void  __cdecl CRT_free(void* ptr);                            /* 0x466C70 */
int   __cdecl CRT_sprintf_buf(void* buf, const char* fmt, ...); /* 0x466D60 */

/* ================================================================== */
/* Win32 stream I/O — C++ linkage                                       */
/*                                                                       */
/* stream_obj/parsed_stream below use WIN32_MemoryStream (resources/     */
/* Win32StreamMem.h, included above) — WNDPROC_StreamFromMemory's real   */
/* declaration/size helper come from that header, not redeclared here.   */
/* ================================================================== */
void* AssetMgr_LoadFile(void* mgr, const char* name,
                        int* out_size);                        /* 0x45CD00 */

/* ================================================================== */
/* UI / Input helpers — C++ linkage                                     */
/* ================================================================== */
char  ScriptedObject_ParseStream(void* stream);                /* 0x41E9F0 */
void  ScriptedObject_InitBase(uint32_t resource_id, int zero);  /* 0x4203E0 */
/* UI_ChildWindow_Render's real definition (ui/UI_ChildWindow.cpp:796) is
 * inside an `extern "C" { }` block (matching ui/UI_ChildWindow.h:339-376) —
 * this declaration must match that linkage, not default C++ linkage,
 * or this call binds to nothing (LINK-001 landmine). */
extern "C" char UI_ChildWindow_Render(void* obj, void* stream);           /* 0x424E00 */

/* Panel helpers declared in Panel.h */
extern void Panel_DtorBody(void* obj);                         /* 0x4545A0 */
extern void GameObject_DtorBody(void* obj);                     /* 0x405870 */

/* ScriptEngine helpers */
extern void ScriptEngine_Init(void* obj);                      /* 0x44E8D0 */
extern void ScriptEngine_Call(void* obj);                      /* 0x44E930 */

/* ScrollPanel helpers */
extern void UIPANEL_InitScrollPanel(void* obj);                /* 0x427370 */
extern void UIPANEL_ScrollPanel_Dtor(void* obj);               /* 0x427460 */

/* TrackPiece */
extern void TrackPiece_SetZoom(void* tool, int zoom);          /* 0x40D170 */

/* Tooltip */
extern void UI_DestroyTooltip(void* mgr, int handle);          /* 0x423D20 */
extern void* UI_CreateTooltip(void* mgr, int res_id, int unk,
                              int x, int y);                    /* 0x423C50 */

/* Audio */
extern void GameAudio_UpdateVolume(void* audio, uint8_t mute); /* 0x4135B0 */
extern int  HelpWnd_PlayNarration(void* mgr, int category,
                                  int res_id);                 /* 0x44F560 */

/* Input/World */
class InputMgr;
extern void INPUT_NewWorld(InputMgr* input_mgr);                   /* 0x41E120 */
extern void INPUT_LoadWorld(InputMgr* input_mgr, const char* path); /* 0x41D320 */
extern void INPUT_SaveCurrentWorld(InputMgr* input_mgr,
                                   const char* path);          /* 0x41D9B0 */
extern void CGWND_SetBuildMode(int mode);                      /* 0x4089D0 */
/* Real def: core/CGWND.cpp, void(int) — was declared void* here,
 * mismatching the real int param (call-0 landmine — silently bound to
 * shared/link_stubs.cpp's void* no-op stub). */
extern void CGWND_SetMode(int mode);                         /* 0x408130 */
extern void Game_CheckScreensaverTimeout(void* game);          /* 0x410A20 */
extern void CGWND_ToggleFullscreen();                          /* 0x408110 */
extern void GameAudio_SetMute(void* audio, uint8_t mute);      /* 0x413560 */
extern void UIPANEL_ScrollPanel_HandleDrag(void* panel, int tool,
                                           int action);        /* 0x4277D0 */
extern void TileMap_InvalidateRect(void* tilemap, int l, int t,
                                   int r, int b);              /* 0x45E240 */
extern void Town_SelectBuilding(void* town, int index);        /* 0x42EC60 */
/* Unused in this file. Address corrected: 0x458B20 disassembles to
 * DDRAW_CleanupSprites (0x458B00's body), not DDRAW_SelectBuilding;
 * confirmed via Ghidra at 0x459180 (see graphics/DDRAW.cpp for the real
 * definition, which returns uint8_t). */
extern int  DDRAW_SelectBuilding(void* ddraw, int index);      /* 0x459180 */

/* GameObject helpers — now class methods; kept as extern for transition */
extern void GameObject_PtInRect(void* obj, int x, int y);      /* 0x436A10 */
extern void GameObject_Update(void* obj);                      /* 0x405C40 */
extern void Entity_Ctor(void* obj, int a, int b, int c, int d);/* 0x405790 */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern InputMgr g_input_mgr;        /* 0x4A9990 — static InputMgr object */
extern void*    g_asset_mgr;             /* 0x485600 */
extern void*    g_audio_mgr;             /* 0x4FD38C */
extern void*    g_audio;                 /* 0x4FD3BC */
extern void*    g_netman;                /* 0x4FD3AC */
extern void*    g_tooltip_mgr;           /* 0x4FD220 */
extern void*    g_tilemap;               /* 0x4AAE90 */
extern void*    g_town_view;             /* 0x4AAD2C */
extern void*    g_ddraw_building;        /* 0x4A9EF0 */
extern void*    g_trainstation_window;   /* 0x485258 */
extern void*    g_game;                  /* 0x4854C8 — Game singleton */

extern int      g_world_width;           /* 0x4AAD0C */
extern int      g_world_height;          /* 0x4AAD10 */
extern int      g_viewport_x;            /* 0x4AAD24 */
extern int      g_viewport_y;            /* 0x4AAD28 */
extern int      g_screen_width;          /* 0x4851D8 */
extern int      g_drag_start_x;          /* 0x485574 */
extern int      g_drag_start_y;          /* 0x485578 */
extern int      g_cursor_world_x;        /* 0x48555C */
extern int      g_cursor_world_y;        /* 0x485560 */
extern char     g_disable_input;         /* 0x4855AC */
extern char     g_is_fullscreen;         /* 0x485210 */
extern char     g_allow_building_placement; /* 0x4FD3DC */
extern char     g_in_build_mode;         /* 0x4FD199 */
extern uint32_t g_last_cursor_pos;       /* 0x485558 */
extern void*    g_active_panel;          /* 0x4FD224 */
extern int      g_stream_open_flags;     /* 0x479190 */
extern char     g_scene_name[];          /* 0x4A99C8 */

/* ================================================================== */
/* Inline helpers for embedded object vtable dispatch                   */
/* These map the binary's literal vtable access to typed operations.   */
/* ================================================================== */

/** Call recovered virtual slots on embedded legacy objects. */
static inline void entity_vmove(ScriptedObject* so, int x, int y)
{
    using MoveTo = void (*)(void*, int, int);
    void** vtable = *reinterpret_cast<void***>(so->sub_entity);
    reinterpret_cast<MoveTo>(vtable[3])(so->sub_entity, x, y);
}

static inline void entity_init(ScriptedObject* so, int a, int b, int c)
{
    using Init = void (*)(void*, int, int, int);
    void** vtable = *reinterpret_cast<void***>(so->sub_entity);
    reinterpret_cast<Init>(vtable[6])(so->sub_entity, a, b, c);
}

static inline void se_shutdown(ScriptedObject* so)
{
    using Shutdown = void (*)(void*);
    void** vtable = *reinterpret_cast<void***>(so->script_engine_prefix);
    reinterpret_cast<Shutdown>(vtable[15])(so->script_engine_prefix);
}

static inline void se_handle_drag(ScriptedObject* so, void* child, int mode)
{
    using HandleDrag = void (*)(void*, void*, int);
    void** vtable = *reinterpret_cast<void***>(so->script_engine_prefix);
    reinterpret_cast<HandleDrag>(vtable[21])(so->script_engine_prefix, child, mode);
}

static inline void se_vmove(ScriptedObject* so, int x, int y)
{
    using MoveTo = void (*)(void*, int, int);
    void** vtable = *reinterpret_cast<void***>(so->script_engine_prefix);
    reinterpret_cast<MoveTo>(vtable[3])(so->script_engine_prefix, x, y);
}

static inline void sp_shutdown(ScriptedObject* so)
{
    using Shutdown = void (*)(void*);
    void** vtable = *reinterpret_cast<void***>(so->scroll_panel_prefix);
    reinterpret_cast<Shutdown>(vtable[15])(so->scroll_panel_prefix);
}

static inline void sp_vmove(ScriptedObject* so, int x, int y)
{
    using MoveTo = void (*)(void*, int, int);
    void** vtable = *reinterpret_cast<void***>(so->scroll_panel_prefix);
    reinterpret_cast<MoveTo>(vtable[3])(so->scroll_panel_prefix, x, y);
}

static inline void tooltip_vmove(int32_t handle, int x, int y)
{
    using MoveTo = void (*)(void*, int, int);
    void* object = reinterpret_cast<void*>(static_cast<intptr_t>(handle));
    void** vtable = *reinterpret_cast<void***>(object);
    reinterpret_cast<MoveTo>(vtable[3])(object, x, y);
}

static inline void tooltip_set_state(int32_t handle, int state)
{
    using SetAnimState = void (*)(void*, int);
    void* object = reinterpret_cast<void*>(static_cast<intptr_t>(handle));
    void** vtable = *reinterpret_cast<void***>(object);
    reinterpret_cast<SetAnimState>(vtable[7])(object, state);
}

/* ================================================================== */
/* Constructor — Address: 0x449430                                      */
/* ================================================================== */

ScriptedObject::ScriptedObject()
{
    /* Step 1: Initialize Panel base fields via PartialDtor.
       In the binary this resets the vtable to Panel and zeros child/tooltip fields. */
    this->PartialDtor();

    /* Step 2: Create embedded Entity at +0xE0 */
    Entity_Ctor(&sub_entity, -1, -1, 0, 0);                   /* 0x405790 */

    /* Step 3: Initialize ScriptEngine at +0x178 */
    ScriptEngine_Init(&script_engine_prefix);                        /* 0x44E8D0 */

    /* Step 4: Initialize ScrollPanel at +0x260 */
    UIPANEL_InitScrollPanel(&scroll_panel_prefix);                   /* 0x427370 */

    /* Step 5: Set type to 10 */
    this->type = 10;

    /* Step 6: Zero out trailing fields */
    this->field_744 = 0;
    this->field_748 = 0;
}


/**
 * ScriptedObject::~ScriptedObject — Body destructor
 *
 * Tears down all sub-objects via InitSubObjects. The scalar-deleting
 * destructor at vtable[0] (0x4494C0) wraps this body, tests flags & 1,
 * and conditionally calls GLOBAL_free — all compiler-generated.
 */
ScriptedObject::~ScriptedObject()
{
    this->InitSubObjects();
}

/* ================================================================== */
/* InitSubObjects — Full teardown of all sub-objects                    */
/* Address: 0x4494E0                                                    */
/* ================================================================== */

void ScriptedObject::InitSubObjects()
{
    /* Step 1: Stop embedded Entity via Init(0, -1, 0) */
    entity_init(this, 0, -1, 0);

    /* Step 2: Stop self via Panel::Init */
    this->Init(0, -1, 0);

    /* Step 3: Shutdown ScriptEngine */
    se_shutdown(this);

    /* Step 4: Shutdown ScrollPanel */
    sp_shutdown(this);

    /* Step 5: Clean Panel resources (tooltip, child surface, etc.) */
    this->PartialDtor();                                     /* was RESDATA_DtorBase @ 0x454630 */

    /* Step 6: Destroy ScrollPanel */
    UIPANEL_ScrollPanel_Dtor(&scroll_panel_prefix);                 /* 0x427460 */

    /* Step 7: Stop ScriptEngine */
    ScriptEngine_Call(&script_engine_prefix);                       /* 0x44E930 */

    /* Step 8: Destroy Entity sub-object */
    GameObject_DtorBody(&sub_entity);                        /* 0x405870 */

    /* Step 9: Destroy Panel base */
    Panel_DtorBody(this);                                    /* 0x4545A0 */
}

/* ================================================================== */
/* Shutdown — Lightweight teardown (vtable[14])                        */
/* Address: 0x4495B0                                                    */
/* ================================================================== */

void ScriptedObject::Shutdown()
{
    /* Stop embedded Entity via Init */
    entity_init(this, 0, -1, 0);

    /* Stop self via Panel::Init */
    this->Init(0, -1, 0);

    /* Stop ScriptEngine */
    se_shutdown(this);

    /* Stop ScrollPanel */
    sp_shutdown(this);

    /* Clean Panel base resources */
    this->PartialDtor();                                     /* 0x454630 */
}

/* ================================================================== */
/* HandleEvent — Load and parse a .dat script file                      */
/* Address: 0x44B290                                                    */
/* ================================================================== */

void ScriptedObject::HandleEvent(uint32_t resource_id, const char* name_suffix)
{
    char dat_path[260];
    char asset_path[260];
    int  loaded_size;
    /* Real WIN32_Stream object (resources/Win32Stream.h) — replaces the
     * original's WIN32_StreamOpen(&buf,1) construction and paired
     * WIN32_StreamDestroy(&buf)+WNDPROC_StreamCleanup(&buf) destruction;
     * see StreamObject::~StreamObject()'s doc comment for the full
     * evidence trail. Also fixes a pre-existing stack buffer-overflow
     * bug: the previous `int stream_handle[2]` (8 bytes) was smaller
     * than sizeof(WIN32_Stream) even on the original x86 (0x5C bytes),
     * let alone this host's wider pointer fields. */
    WIN32_Stream stream_handle;

    this->sub_entity[0x82] = 0;                              /* loaded_flag at +0x162 = Entity::name[6] */
    this->unk_flag    = 0;                                   /* +0x63A */

    /* stream_handle's destructor runs automatically here (real C++ RAII)
     * on every path, including this early return. */
    if (name_suffix == nullptr) {
        return;
    }

    /* Build path strings:
       dat_path = g_scene_name + name_suffix + ".dat"
       audio_channel (at +0x48) = g_scene_name + name_suffix + ".bmp" */
    CRT_sprintf_buf(dat_path, "%s%s.dat", g_scene_name, name_suffix);
    CRT_sprintf_buf(this->script_bitmap_path, "%s%s.bmp", g_scene_name, name_suffix);

    /* Try loading from RFD archive (asset manager) first */
    if (g_asset_mgr != nullptr) {
        char* file_data;
        void* stream_obj;
        WNDPROC_Stream* parsed_stream;

        CRT_sprintf_buf(asset_path, "%s.dat", name_suffix);
        file_data = static_cast<char*>(AssetMgr_LoadFile(
            g_asset_mgr, asset_path, &loaded_size));        /* 0x45CD00 */

        if (file_data != NULL) {
            /* 0x5C was the original x86 sizeof(WIN32_MemoryStream); use the
             * real host size (see resources/Win32StreamMem.h). */
            stream_obj = operator_new(WIN32_MemoryStream_Size());
            if (stream_obj != NULL) {
                parsed_stream = WNDPROC_StreamFromMemory(
                    stream_obj, file_data, loaded_size, 1);   /* 0x464490 */

                if (parsed_stream != NULL) {
                    /* Check stream error flag: real state_bits/kBadBit
                     * check (matches the fallback branch below, replacing
                     * the former raw vtable[1]+offset-8 read of the same
                     * field). */
                    if ((parsed_stream->state_bits & StreamObject::kBadBit) == 0) {
                        char loaded;

                        /* Step 1: Parse script via ScriptedObject_ParseStream */
                        loaded = ScriptedObject_ParseStream(parsed_stream); /* 0x41E9F0 */
                        this->sub_entity[0x82] = loaded;

                        /* Step 2: Render child window */
                        if (loaded != 0) {
                            loaded = UI_ChildWindow_Render(this, parsed_stream); /* 0x424E00 */
                        }
                        this->sub_entity[0x82] = loaded;

                        /* Step 3: Init from stream */
                        if (loaded != 0) {
                            loaded = this->LoadFromStream(parsed_stream);
                        }
                        this->sub_entity[0x82] = loaded;

                        /* Destroy the temporary stream: real C++ `delete`
                         * through WNDPROC_Stream* dispatches to
                         * WIN32_MemoryStream's scalar deleting destructor
                         * via StreamObject's virtual ~StreamObject() —
                         * replaces the former raw vtable[0] dispatch. */
                        delete parsed_stream;
                    }
                }
            }
            CRT_free(file_data);                             /* 0x466C70 */
        }
    }

    /* Fall back to disk file I/O if archive load didn't succeed */
    if (this->sub_entity[0x82] == 0) {
        stream_handle.OpenPath(dat_path, 0x20, g_stream_open_flags); /* 0x463AA0 */

        /* Check stream error flag: real state_bits/kBadBit check,
         * replacing the original's vbtable-relative raw read of the
         * same field (`*(rdbuf-independent StreamObject::state_bits) &
         * kBadBit`, confirmed via disassembly at 0x44B290). */
        if ((stream_handle.state_bits & StreamObject::kBadBit) == 0) {
            char loaded;

            loaded = ScriptedObject_ParseStream(&stream_handle);      /* 0x41E9F0 */
            this->sub_entity[0x82] = loaded;

            if (loaded != 0) {
                loaded = UI_ChildWindow_Render(this, &stream_handle); /* 0x424E00 */
            }
            this->sub_entity[0x82] = loaded;

            if (loaded != 0) {
                loaded = this->LoadFromStream(&stream_handle);
            }
            this->sub_entity[0x82] = loaded;
        }

        /* Matches the original's WIN32_StreamDestroyImmediate — NOT the
         * object's own destructor, which still runs once at scope exit
         * below. */
        stream_handle.CloseNow();                             /* 0x463B10 */
    }

    /* stream_handle's destructor runs automatically here (real C++
     * RAII) — replaces the original's WIN32_StreamDestroy+
     * WNDPROC_StreamCleanup pair. */
}

/* ================================================================== */
/* MoveTo — Move ScriptedObject to (x, y) with boundary clamping       */
/* Address: 0x449DC0                                                    */
/* ================================================================== */

void ScriptedObject::MoveTo(int x, int y)
{
    int sprite_width;

    /* When mode == 1 (entering build mode), skip boundary clamping */
    if (this->mode != 1) {
        if (this->dim_flag == 1) {
            /* Positive direction: clamp x >= 0 */
            if (x < 0) {
                x = 0;
            }

            /* Get sprite frame width from RESDATA (+0x14 = frame_width) */
            sprite_width = static_cast<const RESDATA*>(this->resource)->frame_width;

            /* Check against ScriptEngine right bound */
            if (this->script_engine_active != 0) {
                int max_x = g_world_width - this->script_engine_offset - sprite_width;
                if (x > max_x) x = max_x;
            }
            else if (this->scroll_panel_offset != 0) {
                int max_x = g_world_width - this->scroll_panel_offset - sprite_width;
                if (x > max_x) x = max_x;
            }
            else {
                if (x > g_world_width - sprite_width) {
                    x = g_world_width - sprite_width;
                }
            }
        }
        else {  /* dim_flag == 0 — negative direction */
            if (x < 0) {
                x = 0;
            }

            /* Check left bound for ScriptEngine */
            if (this->script_engine_active != 0 && x < this->script_engine_offset) {
                x = this->script_engine_offset;
            }
            else if (this->scroll_panel_offset != 0 && x < this->scroll_panel_offset) {
                x = this->scroll_panel_offset;
            }
            else {
                sprite_width = static_cast<const RESDATA*>(this->resource)->frame_width;
                if (x > g_world_width - sprite_width) {
                    x = g_world_width - sprite_width;
                }
            }
        }

        /* Y boundary clamping */
        if (y < 0) {
            y = 0;
        }
        else {
            int height = this->screen_rect.bottom;     /* +0x3C */
            if (y > g_world_height - height) {
                y = g_world_height - height;
            }
        }
    }

    /* Set position */
    this->SetPosition(x, y);                                /* 0x454820 */

    /* Update sub_entity position with frame offset.
       frame_data_ptr is at +0x120 (= Entity::resource at sub_entity+0x40).
       Reads int16_t offsets at frame_data+0x2E and +0x30. */
    {
        void* fdp = *reinterpret_cast<void**>(&this->sub_entity[0x40]); /* +0x120 */
        const uint8_t* frame_bytes = reinterpret_cast<const uint8_t*>(fdp);
        int go_x = x + static_cast<int>(*reinterpret_cast<const int16_t*>(frame_bytes + 0x2E));
        int go_y = y + static_cast<int>(*reinterpret_cast<const int16_t*>(frame_bytes + 0x30));
        entity_vmove(this, go_x, go_y);
    }

    /* Update ScriptEngine and ScrollPanel child sprite positions */
    if (this->dim_flag == 0) {
        /* Negative direction: child sprites left of the object */
        if (this->script_engine_active != 0) {
            se_vmove(this, x - this->script_engine_offset, y + 14);
        }
        if (this->scroll_panel_offset != 0) {
            sp_vmove(this, x - this->scroll_panel_offset, y + 14);
        }
    }
    else {  /* Direction flag == 1 — positive direction */
        int sprite_w = static_cast<const RESDATA*>(this->resource)->frame_width;

        if (this->script_engine_active != 0) {
            se_vmove(this, sprite_w + x, y + 14);
        }
        if (this->scroll_panel_offset != 0) {
            sp_vmove(this, sprite_w + x, y + 14);
        }
    }

    /* Update drag_rect based on mode */
    if (this->mode == 0) {
        /* Mode 0 (idle): Set drag rect from screen_rect offsets */
        RECT r;
        SetRect(&r,
            this->screen_rect.left   + 0x18,
            this->screen_rect.top    + 3,
            this->screen_rect.left   + 0x2C,
            this->screen_rect.top    + 0x0D);
        this->drag_rect = r;
    }
    else {
        /* Mode != 0: Copy embedded Entity's screen_rect into drag_rect.
           sub_entity is at +0xE0, screen_rect (GameObject+0x08) is at +0xE8 */
        this->drag_rect = *reinterpret_cast<const RECT*>(&this->sub_entity[8]);
    }

    /* Update cursor position if actively dragging */
    {
        int prev_x = this->callback_1;
        int prev_y = this->callback_2;
        if (this->drag_active != 0 && (x != prev_x || y != prev_y)) {
            int screen_x = (x - g_viewport_x) + this->drag_offset_x;
            int screen_y = (y - g_viewport_y) + this->drag_offset_y;

            /* Convert client → screen coords and move cursor */
            /* NOTE: This reconstructs the ClientToScreen pattern from the binary.
               The original passes a stack-based POINT struct. */
            {
                POINT pt;
                pt.x = screen_x;
                pt.y = screen_y;
                uintptr_t main_window_base = *reinterpret_cast<const uintptr_t*>(g_main_window);
                HWND main_hwnd = *reinterpret_cast<HWND*>(main_window_base + 8);
                ClientToScreen(main_hwnd, &pt);
                SetCursorPos(pt.x, pt.y);
            }

            /* Pack cursor position into global for save/restore */
            g_last_cursor_pos =
                ((static_cast<uint16_t>(y - g_viewport_y) +
                  static_cast<uint16_t>(this->drag_offset_y)) << 16) |
                (static_cast<uint16_t>(x - g_viewport_x) +
                 static_cast<uint16_t>(this->drag_offset_x));

            Game_CheckScreensaverTimeout(g_game);            /* 0x410A20 */
        }
    }

    /* Update tooltip position if one exists */
    if (this->tooltip_handle != 0) {
        tooltip_vmove(this->tooltip_handle,
            this->screen_rect.left + 0x32,
            this->screen_rect.top  + 0x32);
    }
}

/* ================================================================== */
/* RemoveChild — Destroy child ScriptedObject                           */
/* Address: 0x44B220                                                    */
/* ================================================================== */

void ScriptedObject::RemoveChild()
{
    /* Free child script pointer if non-null */
    if (this->child_script_ptr != nullptr) {
        GLOBAL_free(this->child_script_ptr);                 /* 0x465CD0 */
        this->child_script_ptr = nullptr;
    }

    /* Call base destructor (ScriptedObject_InitBase with 0,0) */
    ScriptedObject_InitBase(0, 0);                               /* 0x4203E0 */
}

/* ================================================================== */
/* AddChild — Construct child ScriptedObject                            */
/* Address: 0x44B190                                                    */
/* ================================================================== */

ScriptedObject* ScriptedObject::AddChild(uint32_t resource_id, const char* name_suffix)
{
    /* Initialize base via ExitGame */
    ScriptedObject_InitBase(resource_id, 0);                     /* 0x4203E0 */

    /* Clear child script pointer */
    this->child_script_ptr = nullptr;

    /* Load script via HandleEvent */
    this->HandleEvent(resource_id, name_suffix);

    return this;
}

/* ================================================================== */
/* UpdateToolState — Per-frame tool zoom update (vtable[19])           */
/* Address: 0x44AC20                                                    */
/* ================================================================== */

uint32_t ScriptedObject::UpdateToolState(TrackPiece* tool)
{
    if (tool == nullptr) {
        return 0;
    }

    /* Decrement frame timer (prev_frame) if non-negative */
    int16_t timer = tool->prev_frame;
    if (timer >= 0) {
        timer--;
        tool->prev_frame = timer;
    }

    /* Auto-reset zoom to 1 when timer expires and zoom==2 */
    if (timer == 0 && tool->zoom_level == 2) {
        TrackPiece_SetZoom(tool, 1);                         /* 0x40D170 */
    }

    /* Dispatch by tool type (resource->resource_id) */
    int tool_type = tool->resource->resource_id;

    switch (tool_type) {
    case 0x2407:  /* New Game tool */
        if (timer == 0) {
            if (HelpWnd_PlayNarration(g_audio_mgr, 7, 0x2407) == 0) {
                INPUT_NewWorld(&g_input_mgr);                 /* 0x41E120 */
            }
        }
        break;

    case 0x2408:  /* Load Game tool */
        if (timer == 0) {
            HelpWnd_PlayNarration(g_audio_mgr, 7, 0x2408);
            INPUT_LoadWorld(&g_input_mgr, "curr");            /* 0x41D320 */
        }
        break;

    case 0x240B:  /* Building placement toggle */
        if (g_allow_building_placement == 1) {
            TrackPiece_SetZoom(tool, 2);
        } else {
            TrackPiece_SetZoom(tool, 1);
        }
        break;

    case 0x240C:  /* Fullscreen toggle */
        if (g_is_fullscreen != 0 && g_world_width <= g_screen_width) {
            TrackPiece_SetZoom(tool, 2);
        } else {
            TrackPiece_SetZoom(tool, 1);
        }
        break;

    case 0x240D:  /* Scenario mode indicator */
        /* g_netman[0x17].scenarioId == 2 => scenario mode */
        if (*reinterpret_cast<const int*>(
                reinterpret_cast<const uint8_t*>(g_netman) + 0x7C4) == 2) {
            TrackPiece_SetZoom(tool, 3);
        } else if (timer == 0) {
            TrackPiece_SetZoom(tool, 2);
            HelpWnd_PlayNarration(g_audio_mgr, 0, 0);
        } else {
            TrackPiece_SetZoom(tool, 1);
        }
        break;

    case 0x240E:  /* Mute toggle */
        if (g_audio != nullptr && *reinterpret_cast<const uint8_t*>(
                reinterpret_cast<const uint8_t*>(g_audio) + 0xB4) == 0) {
            TrackPiece_SetZoom(tool, 1);
        } else {
            TrackPiece_SetZoom(tool, 2);
        }
        break;

    case 0x240F:  /* Exit build mode */
        if (timer == 0) {
            HelpWnd_PlayNarration(g_audio_mgr, 7, 0x240F);
            this->EnterBuildMode(0);
        }
        break;

    default:
        break;
    }

    return 0;
}

/* ================================================================== */
/* EnterBuildMode — Enter/exit build mode                                */
/* Address: 0x44A9D0                                                    */
/* ================================================================== */

void ScriptedObject::EnterBuildMode(uint8_t enter)
{
    if (enter == 0) {
        /* === EXIT BUILD MODE === */
        if (this->update_child_flags != 0) {
            /* Close all sub-panels */
            this->OnUpdateChild();

            void* child = this->child_surface;
            this->update_child_flags = 0;

            /* Stop ScriptEngine child surface */
            if (this->script_engine_active != 0) {
                se_handle_drag(this, child, 0);
            }

            /* Stop ScrollPanel child surface */
            if (this->scroll_panel_offset != 0) {
                UIPANEL_ScrollPanel_HandleDrag(
                    &scroll_panel_prefix,
                    static_cast<int>(reinterpret_cast<intptr_t>(child)), 0);
            }

            this->dim_flag = 0;
            CGWND_SetBuildMode(0);                           /* 0x4089D0 */

            /* Reset all track piece zooms in the linked list */
            while (child != nullptr) {
                const uint8_t* child_bytes = reinterpret_cast<const uint8_t*>(child);
                const uintptr_t resource_address = static_cast<uintptr_t>(
                    *reinterpret_cast<const uint32_t*>(child_bytes + 0x44));
                int child_type = *reinterpret_cast<const int*>(resource_address + 4);
                switch (child_type) {
                case 0x2403: case 0x2404: case 0x2405:
                case 0x2406: case 0x2409: case 0x240A:
                    TrackPiece_SetZoom(child, 1);            /* 0x40D170 */
                    break;
                }
                child = *reinterpret_cast<void* const*>(
                    child_bytes + 0x28);                       /* linked list: sub_resource */
            }

            /* Save current world if in build mode */
            if (g_in_build_mode != 0) {
                INPUT_SaveCurrentWorld(&g_input_mgr, "curr"); /* 0x41D9B0 */
            }

            /* Re-init panel */
            this->Init(0x2400, 2, 0);
            this->mode = 2;

            /* Update tooltip state */
            if (this->tooltip_handle != 0) {
                tooltip_set_state(this->tooltip_handle, 2);
            }

            /* Restore audio volume */
            if (g_audio != nullptr) {
                GameAudio_UpdateVolume(g_audio, 0);
            }
        }
    }
    else {
        /* === ENTER BUILD MODE === */
        if (this->update_child_flags == 0) {
            /* Init panel for build mode */
            this->Init(0x2400, 1, 0);
            this->mode = 1;

            /* Reposition with offset */
            this->MoveTo(
                this->screen_rect.left - 0x31,
                this->screen_rect.top  - 0x2F);

            CGWND_SetMode(4);

            /* Destroy old tooltip if exists */
            if (this->tooltip_handle != 0) {
                UI_DestroyTooltip(g_tooltip_mgr, this->tooltip_handle);
                this->tooltip_handle = 0;
            }

            /* Create new tooltip if visible */
            if (this->initialized != 0) {
                this->tooltip_handle = static_cast<int32_t>(reinterpret_cast<intptr_t>(
                    UI_CreateTooltip(g_tooltip_mgr, 0x3879, 1,
                    this->screen_rect.left + 0x32,
                    this->screen_rect.top  + 0x32)));
            }

            /* Mute audio */
            if (g_audio != NULL) {
                GameAudio_UpdateVolume(g_audio, 1);
            }
        }
    }
}

/* ================================================================== */
/* Stub implementations for methods not yet decompiled                  */
/* ================================================================== */

void ScriptedObject::OnUpdateChild()
{
    /* TODO: decompile 0x454890 — delegates to Panel::UpdateChild */
}

int ScriptedObject::IsDragging(int x, int y)
{
    /* TODO: decompile 0x449CE0 — delegates to GameObject::PtInRect */
    GameObject_PtInRect(this, x, y);
    return 0;
}

int ScriptedObject::HitTest(int x, int y)
{
    /* TODO: decompile 0x44A0C0 */
    return 0;
}

void ScriptedObject::Update()
{
    /* TODO: decompile 0x4497A0 */
}

int ScriptedObject::InitState()
{
    /* TODO: decompile 0x44ADF0 */
    return 0;
}

uint32_t ScriptedObject::HandleToolClick(int* tool, int x, int y)
{
    /* TODO: decompile 0x44A250 */
    return 0;
}

int ScriptedObject::GetDragOffset(int x, int y)
{
    /* TODO: decompile 0x449D80 */
    return 0;
}

bool ScriptedObject::CheckClick(int x, int y)
{
    /* TODO: decompile 0x449D00 */
    return false;
}

char ScriptedObject::LoadFromStream(void* stream)
{
    /* TODO: decompile — called from HandleEvent */
    return 0;
}
