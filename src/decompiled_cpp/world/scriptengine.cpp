/**
 * scriptengine.cpp — ScriptEngine and ScriptedObject implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The ScriptEngine is a RESDATA-derived class that manages script-like
 * animation/callback dispatch. The RESDATA_ScriptedObject singleton
 * (~0x74C bytes at 0x4A99E0) manages the interactive scripted objects
 * (buildings with click/pickup/drop behavior).
 *
 * These classes are C++ with virtual methods:
 *   - ScriptEngine inherits from RESDATA base (vtable 0x4782A4/0x478378)
 *   - ScriptedObject inherits from RESDATA base (vtable 0x4782A8, type=10)
 *
 * Key field offsets (byte offsets, verified from disassembly):
 *   ScriptEngine: vtable=+0x00, CS=+0x04 (sizeof 0x18 on Win9x)
 *   ScriptedObject: dispatch_state at +0x740, child list at +0x0D0,
 *     GameObject at +0xE0, ScriptEngine at +0x178, ScrollPanel at +0x260,
 *     ScriptEngine visible flag at +0x200, ScrollPanel visible flag at +0x2E8
 */

#include "scriptengine.h"
#include "../resources/ResourceManager.h"  /* for PlaySoundAt, etc. */
#include "../core/Entity.h"                /* embedded resource-backed entity fields */

/* ================================================================== */
/* External CRT / Windows helpers                                      */
/* ================================================================== */
/* Win32 imports retain C linkage; game-internal functions use C++ linkage. */
extern "C" {
    void __stdcall InitializeCriticalSection(void* lpCriticalSection);
    void __stdcall DeleteCriticalSection(void* lpCriticalSection);
    void __stdcall EnterCriticalSection(void* lpCriticalSection);
    void __stdcall LeaveCriticalSection(void* lpCriticalSection);
}
    void __thiscall RESDATA_BaseInit(void* ptr);        /* @ 0x44E6F0 */
    void __thiscall RESDATA_DtorBase(void* ptr);        /* @ 0x44E820 */
    void GLOBAL_free(void* ptr);                        /* @ 0x465E10 */
    void GameObject_BaseCtor(void* obj, int32_t x, int32_t y, int32_t w, int32_t h); /* @ 0x4369D0 */
    void __thiscall GameObject_Update(void* obj);       /* @ 0x436BD0 */
    void __thiscall GameObject_Draw(void* obj);         /* @ 0x436C80 */
    void __thiscall GameObject_DtorBody(void* obj);     /* @ 0x436C50 */
    char __thiscall GameObject_PtInRect(void* obj, int32_t x, int32_t y); /* @ 0x436A10 */
    void __thiscall UIPANEL_InitScrollPanel(void* panel); /* @ 0x426F60 */
    void __thiscall UIPANEL_ScrollPanel_Dtor(void* panel); /* @ 0x427EF0 */
    void __thiscall Panel_DtorBody(void* panel);        /* @ 0x425E80 */
    void __thiscall UIPANEL_ScrollPanel_HandleDrag(void* panel, int32_t param, int32_t action); /* @ 0x427BD0 */
    void __thiscall RESDATA_SetPosition(void* obj, int32_t x, int32_t y);  /* @ 0x44E700 */
    char __thiscall RESDATA_HitTestChildren(void* obj, int32_t x, int32_t y); /* @ 0x44E6C0 */
    void __thiscall RESDATA_CreateChildSprite(void* obj, int32_t resId, int32_t param3, int32_t param4); /* @ 0x44E530 */
    int32_t __thiscall ResourceManager_GetById(void* resmgr, int32_t resId); /* @ 0x446EA0 */
    int32_t __thiscall UI_IsBitmapReady(int32_t resPtr);  /* @ 0x4249D0 */
    void* __thiscall UI_CreateTooltip(void* mgr, int32_t resId, int32_t param, int32_t x, int32_t y); /* @ 0x428DA0 */
    void* __thiscall UI_DestroyTooltip(void* mgr, int32_t tooltipId);  /* @ 0x428E40 */
    int32_t __thiscall CGWND_TrackPiece_SetZoom(void* obj, int32_t zoom); /* @ 0x40DD90 */
    void __thiscall CGWND_SetBuildMode(int32_t mode);     /* @ 0x40E7A0 */
    void __thiscall CGWND_SetMode(void* mode);            /* @ 0x40DF50 */
    void __thiscall GameAudio_UpdateVolume(void* audio, uint8_t mute);    /* @ 0x413150 */
    int32_t __thiscall HelpWnd_PlayNarration(void* audioMgr, int32_t page, int32_t param); /* @ 0x4510B0 */
    void __fastcall INPUT_ExitGame(void* obj, uint32_t resId, int32_t strPtr); /* @ 0x41E570 */
    void __thiscall INPUT_CreateEditControl(void* obj);   /* @ 0x41ED80 */
    uint8_t __fastcall INPUT_EditWndProc(void* obj, void* stream); /* @ 0x41EE50 */
    uint8_t __thiscall UI_ChildWindow_Render(void* obj, void* stream); /* @ 0x4244D0 */
    uint8_t __thiscall CGWND_TrackPiece_UpdateAnim(void* obj);  /* @ 0x40D2F0 */
    void __fastcall ScriptEngine_Init(void* engine);      /* @ 0x44E8D0 */
    void __fastcall ScriptEngine_Call(void* engine);      /* @ 0x44E930 */

    /* Globals */
    /* g_resmgr is declared in ResourceManager.h as ResourceManager */
    extern void* g_tilemap;                    /* @ 0x4A99DC */
    extern void* g_tooltip_mgr;               /* @ 0x4AA500 */
    extern void* g_audio;                      /* @ 0x4FD3BC */
    extern void* g_audio_mgr;                  /* @ 0x4A9E0C */
    extern void* g_town_view;                  /* @ 0x4A9E1C */
    extern void* g_ddraw_active;               /* @ 0x4AA4A4 */
    extern void* g_ddraw_building;             /* @ 0x4FD758 */
    extern void* g_town_mode;                  /* @ 0x485490 */
    extern void* g_netman;                     /* @ 0x4FD3AC */
    extern int32_t g_world_width;              /* @ 0x4AA42C */
    extern int32_t g_world_height;             /* @ 0x4A9E28 */
    extern int32_t g_cursor_world_x;           /* @ 0x4AA410 */
    extern int32_t g_cursor_world_y;           /* @ 0x4AA414 */
    extern int32_t g_drag_start_x;             /* @ 0x4AA418 */
    extern int32_t g_drag_start_y;             /* @ 0x4AA41C */
    extern int32_t g_disable_input;            /* @ 0x4AA418 (overlaps with drag_start_x?) */
    extern int32_t g_demo_mode;                /* @ 0x4A9918 */
    extern int32_t g_screen_width;             /* @ 0x4AA420 */
    extern uint8_t g_is_fullscreen;            /* @ 0x4AA418 */
    extern uint8_t g_allow_building_placement; /* @ 0x4AA428 */
    extern int32_t g_in_build_mode;            /* @ 0x4A9E10 */
    extern int32_t g_viewport_x;               /* @ 0x4AA43C */
    extern int32_t g_viewport_y;               /* @ 0x4AA440 */
    extern int32_t g_active_panel;             /* @ 0x4AA428 */
    extern void* g_trainstation_window;        /* @ 0x4A9E04 */
    extern int32_t g_game;                     /* @ 0x4FCCCC */
    void __thiscall Game_CheckScreensaverTimeout(int32_t* game); /* @ 0x40E160 */
extern "C" {
    void __stdcall ClientToScreen(void* hwnd, void* point);
    void __stdcall SetCursorPos(int32_t x, int32_t y);
}

    /* External helpers used by HandleEvent */
    int32_t __fastcall CRT_sprintf_buf(void* buf, const char* format, ...); /* @ 0x00467F60 */
    void* __thiscall WNDPROC_StreamFromMemory(void* stream, char* data, int32_t size, int32_t flag);  /* @ 0x00460C10 */
    void* __thiscall AssetMgr_LoadFile(void* mgr, void* name, int32_t* outSize);  /* @ 0x00457C00 */
    void CRT_free(void* ptr);                       /* @ 0x00465280 */
    void* operator_new(uint32_t size);              /* @ 0x00465CE0 */
    void __thiscall WIN32_StreamOpen(void* stream, int32_t flag);        /* @ 0x00461600 */
    void __thiscall WIN32_StreamOpenPath(void* stream, const char* path, uint32_t mode, void* param); /* @ 0x00461640 */
    void __thiscall WIN32_StreamDestroyImmediate(void* stream);          /* @ 0x00461800 */
    void __thiscall WIN32_StreamDestroy(void* stream);                   /* @ 0x004617C0 */
    void __thiscall WNDPROC_StreamCleanup(void* stream);                 /* @ 0x00460D50 */
extern "C" {
    void __stdcall SetRect(void* lprc, int32_t left, int32_t top, int32_t right, int32_t bottom);
}
    void __thiscall TileMap_InvalidateRect(void* tilemap, int32_t left, int32_t top, int32_t right, int32_t bottom);

/* ==================================================================== */
/* ScriptEngine::ScriptEngine — Constructor                              */
/* Address: 0x4493A0                                                    */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Compiler-managed base table is at 0x4782A4; initializes critical    */
/* section for thread safety. CRITICAL_SECTION starts at +0x04.        */
/* ==================================================================== */
ScriptEngine::ScriptEngine()
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    InitializeCriticalSection(&this->cs);  /* CRITICAL_SECTION at +0x04 */
}

/* ==================================================================== */
/* ScriptEngine::Init — Full initialization                              */
/* Address: 0x44E8D0                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Sets up RESDATA base, installs full vtable, clears state flags.     */
/* ==================================================================== */
void __fastcall ScriptEngine::Init()
{
    RESDATA_BaseInit(this);
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    this->script_state = 0;                     /* +0x1F */
    this->active_flag = 0;                      /* +0x1C */
    this->field_2C = 0;                         /* +0x2C */
    this->field_30 = 0;                         /* +0x30 */
    this->script_flags = 0xB;                     /* init type = 0xB */
}

/* ==================================================================== */
/* ScriptEngine::Cleanup — Constructor failure / memory cleanup          */
/* Address: 0x4493C0                                                   */
/* __thiscall (this, flags)                                             */
/*                                                                      */
/* Sets vtable to safety sentinel, deletes critical section.           */
/* Returns this. Virtual slot [0] of the table at 0x4782A4.            */
/* ==================================================================== */
void* __thiscall ScriptEngine::Cleanup(uint8_t flags)
{
    DeleteCriticalSection(&this->cs);
    if (flags & 1) {
        GLOBAL_free(this);
    }
    return this;
}

/* ==================================================================== */
/* ScriptEngine::Dtor — Destructor                                       */
/* Address: 0x4493F0                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Sets vtable to safety sentinel, deletes critical section.           */
/* ==================================================================== */
void __fastcall ScriptEngine::Dtor()
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    DeleteCriticalSection(&this->cs);
}

/* ==================================================================== */
/* ScriptEngine::Lock — EnterCriticalSection wrapper                     */
/* Address: 0x449410                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* RESDATA base vtable[1] method. Returns 1.                           */
/* ==================================================================== */
uint8_t __fastcall ScriptEngine::Lock()
{
    EnterCriticalSection(&this->cs);
    return 1;
}

/* ==================================================================== */
/* ScriptEngine::Unlock — LeaveCriticalSection wrapper                   */
/* Address: 0x449420                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* RESDATA base vtable[2] method. Returns 1.                           */
/* ==================================================================== */
uint8_t __fastcall ScriptEngine::Unlock()
{
    LeaveCriticalSection(&this->cs);
    return 1;
}

/* ==================================================================== */
/* ScriptEngine::Call — Shutdown/dispatch method                         */
/* Address: 0x44E930                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Resets vtable to base RESDATA table and calls Panel_DtorBody() to    */
/* dispatch to base destructor logic. Used by Reset to clear state.     */
/* NOTE: Despite the name "Call", this is actually a shutdown/body-     */
/* destructor call, not a script invocation. The vtable entry at        */
/* vtable[2] (offset +0x08) is the "OnInitFromStream" body-destructor. */
/* ==================================================================== */
void __fastcall ScriptEngine::Call()
{
    /* Reset vtable to base RESDATA table */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    /* Delegate to Panel base body destructor */
    Panel_DtorBody(this);
}

/* ==================================================================== */
/* ScriptEngine::Reset — Reset/Clear state with optional free           */
/* Address: 0x44E910                                                   */
/* __thiscall (this, param_1=bit0=free_memory)                          */
/*                                                                      */
/* Calls ScriptEngine::Call (vtable reset + body dtor), then optionally */
/* frees memory via GLOBAL_free if param_1 & 1.                         */
/* ==================================================================== */
void* __thiscall ScriptEngine::Reset(uint8_t free_memory)
{
    this->Call();             /* vtable reset + body destructor */
    if (free_memory & 1) {
        GLOBAL_free(this);
    }
    return this;
}

/* ==================================================================== */
/* ScriptEngine_Run — Run the script engine with parameters              */
/* Address: 0x44EF10                                                   */
/* __cdecl (param_1=ScriptEngine*, param_2, param_3)                    */
/*                                                                      */
/* Checks if engine's flags have bit 1 set (flags&2). If set, dispatches*/
/* vtable[2] method with 2 parameters. On success (non-zero return),    */
/* sets zoom=2. On failure or type==2, sets zoom=1.                    */
/* ==================================================================== */
int CDECL ScriptEngine_Run(void* engine, int param_2, int param_3)
{
    int result;

    if (engine == NULL) {
        return 0;
    }

    /* Check bit 1 of flags at offset +0x0C (relative to base) */
    if ((*(uint8_t*)((uint8_t*)engine + 0x0C) & 2) == 0) {
        return 0;
    }

    /* Dispatch vtable[2] */
    result = (*(int (**)(int, int))((uint8_t*)(*(void**)engine) + 2))(param_2, param_3);

    if ((char)result != 0) {
        /* Success — check type at +0x48 */
        if (*(int16_t*)((uint8_t*)engine + 0x48) == 1) {
            result = CGWND_TrackPiece_SetZoom(engine, 2);
        }
        return 1;
    } else {
        /* Failure — check type at +0x48 */
        if (*(int16_t*)((uint8_t*)engine + 0x48) == 2) {
            result = CGWND_TrackPiece_SetZoom(engine, 1);
        }
        return result & 0xFFFFFF00;
    }
}

/* ==================================================================== */
/* RESDATA_Lock — EnterCriticalSection wrapper (free function)          */
/* Address: 0x449410                                                   */
/* __fastcall (ECX = ptr, CRITICAL_SECTION at +0x04)                    */
/*                                                                      */
/* RESDATA base vtable[1] method. Returns 1.                           */
/* ==================================================================== */
uint8_t __fastcall RESDATA_Lock(void* ptr)
{
    EnterCriticalSection((uint8_t*)ptr + 0x04);
    return 1;
}

/* ==================================================================== */
/* RESDATA_Unlock — LeaveCriticalSection wrapper (free function)        */
/* Address: 0x449420                                                   */
/* __fastcall (ECX = ptr, CRITICAL_SECTION at +0x04)                    */
/*                                                                      */
/* RESDATA base vtable[2] method. Returns 1.                           */
/* ==================================================================== */
uint8_t __fastcall RESDATA_Unlock(void* ptr)
{
    LeaveCriticalSection((uint8_t*)ptr + 0x04);
    return 1;
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::Ctor — Constructor                            */
/* Address: 0x449430                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Constructs the singleton ScriptedObject (~0x74C bytes total).        */
/*   - Initializes RESDATA base via RESDATA_BaseInit                   */
/*   - Creates inline GameObject at +0xE0                              */
/*   - Creates inline ScriptEngine at +0x178                           */
/*   - Creates inline UIPANEL ScrollPanel at +0x260                     */
/*   - Compiler supplies the ScriptedObject table at 0x4782A8; type=10 */
/*   - Zeroes child sprite ptr at +0x744/+0x748                        */
/* ==================================================================== */
void __fastcall RESDATA_ScriptedObject::Ctor()
{
    RESDATA_BaseInit(this);

    /* Initialize inline sub-objects */
    GameObject_BaseCtor(this->gameobject, -1, -1, 0, 0);            /* +0xE0 */
    ScriptEngine_Init(this->scriptengine);                           /* +0x178 */
    UIPANEL_InitScrollPanel(this->scrollpanel);                      /* +0x260 */

    /* Set final vtable and type */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    this->type = 10;                                                 /* +0x04 */

    /* Clear child sprite pointers */
    this->child_sprite1 = nullptr;                                   /* +0x744 */
    this->child_sprite2 = nullptr;                                   /* +0x748 */
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::Dtor — Destructor                             */
/* Address: 0x4494C0                                                   */
/* __thiscall (this, flags)                                             */
/*                                                                      */
/* Calls InitSubObjects (teardown), frees memory if flags&1.           */
/* Virtual slot [0] of the ScriptedObject table at 0x4782A8.          */
/* ==================================================================== */
void* __thiscall RESDATA_ScriptedObject::Dtor(uint8_t flags)
{
    this->InitSubObjects();  /* teardown sub-objects */
    if (flags & 1) {
        GLOBAL_free(this);
    }
    return this;
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::InitSubObjects — Teardown (misleading name)   */
/* Address: 0x4494E0                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Destroys all embedded sub-objects in reverse construction order.     */
/* Called from Dtor and init failure paths.                             */
/* ==================================================================== */
void __fastcall RESDATA_ScriptedObject::InitSubObjects()
{
    /* The compiler preserves the class's virtual dispatch table. */
    /* Stop child GameObject via vtable dispatch */
    (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->gameobject) + 6))(
        *(void**)this->gameobject, 0, -1);                           /* +0xE0 */

    /* Stop own base via vtable[6] */
    (*(void (**)(int32_t, int32_t, int32_t))((uint8_t*)(*(void**)this) + 6))(0, -1, 0);

    /* Call ScriptEngine shutdown via vtable[15] (slot 0x3C/4) */
    (*(void (**)(void))((uint8_t*)(*(void**)this->scriptengine) + 15))();        /* +0x178 */

    /* Call ScrollPanel shutdown via vtable[15] */
    (*(void (**)(void))((uint8_t*)(*(void**)this->scrollpanel) + 15))();         /* +0x260 */

    /* Destroy RESDATA base */
    RESDATA_DtorBase(this);

    /* Destroy ScrollPanel */
    UIPANEL_ScrollPanel_Dtor(this->scrollpanel);                     /* +0x260 */

    /* Call ScriptEngine body destructor */
    ScriptEngine_Call(this->scriptengine);                           /* +0x178 */

    /* Destroy GameObject body */
    GameObject_DtorBody(this->gameobject);                           /* +0xE0 */

    /* Destroy Panel body */
    Panel_DtorBody(this);
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::Shutdown — Lightweight shutdown               */
/* Address: 0x4495B0                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Called from CGWND_Cleanup on game exit. Object stays for re-init.    */
/* ==================================================================== */
void __fastcall RESDATA_ScriptedObject::Shutdown()
{
    /* Stop child GameObject */
    (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->gameobject) + 6))(
        *(void**)this->gameobject, 0, -1);                           /* +0xE0 */

    /* Stop own base via vtable[6] */
    (*(void (**)(int32_t, int32_t, int32_t))((uint8_t*)(*(void**)this) + 6))(0, -1, 0);

    /* Shutdown ScriptEngine via vtable[15] */
    (*(void (**)(void))((uint8_t*)(*(void**)this->scriptengine) + 15))();        /* +0x178 */

    /* Shutdown ScrollPanel via vtable[15] */
    (*(void (**)(void))((uint8_t*)(*(void**)this->scrollpanel) + 15))();         /* +0x260 */

    /* Destroy RESDATA base */
    RESDATA_DtorBase(this);
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::Start — Activate scripted object              */
/* Address: 0x449600                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Loads resources 0x2400-0x2413, creates sprites, tooltips.           */
/* Returns non-zero on success.                                         */
/* ==================================================================== */
uint32_t __fastcall RESDATA_ScriptedObject::Start()
{
    /* Load resources 0x2400-0x2413 */
    int32_t* initResult = (int32_t*)(*(int32_t* (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 6))(
        this, 0x2400, -1);

    /* Set sub-object mapping flag */
    if (this->resource != NULL) {                                    /* +0x40 */
        ((RESDATA*)this->resource)->frame_width = 1;
    }

    if ((char)initResult != 0) {
        /* Init child GameObject */
        initResult = (int32_t*)(*(int32_t* (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->gameobject) + 6))(
            *(void**)this->gameobject, 0x2402, -1);                  /* +0xE0 */
        if ((char)initResult != 0) {
            uint32_t panelResult = (*(uint32_t (**)(void*))((uint8_t*)(*(void**)this->scrollpanel) + 6))(
                this->scrollpanel);                                  /* +0x260 */
            if ((char)panelResult == 0) {
                return panelResult;
            }

            /* Create sprites for resources 0x2400-0x2413 */
            for (int32_t resId = 0x2400, count = 0; count < 20; resId++, count++) {
                int32_t resource = ResourceManager_GetById(&g_resmgr, resId);
                if (resource != 0) {
                    if (UI_IsBitmapReady(resource) != 0) {
                        int32_t resType = *(int32_t*)((uint8_t*)(uintptr_t)resource + 4);
                        if (resType == 0x2406) {
                            RESDATA_CreateChildSprite(this, resource, 0, 0);
                            this->child_sprite1 = nullptr;  /* sprite handle stored internally */            /* +0x744 */
                        } else if (resType == 0x240C) {
                            RESDATA_CreateChildSprite(this, resource, 0, 0);
                            this->child_sprite2 = nullptr;  /* sprite handle stored internally */            /* +0x748 */
                        } else {
                            RESDATA_CreateChildSprite(this, resource, 0, 0);
                        }
                    }
                }
            }

            /* Load game mode resource */
            char modeResult = (char)(intptr_t)(*(int32_t* (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 6))(
                this, 0x2401, -1);

            if (g_demo_mode == 1) {
                /* Pause if in demo mode */
                extern void __thiscall CGWND_SetPause(void* obj, uint8_t pause);
                CGWND_SetPause(this, 0);
            }

            /* Destroy old tooltip */
            if (this->tooltip_id != 0) {                             /* +0xA0 */
                UI_DestroyTooltip(&g_tooltip_mgr, (int32_t)this->tooltip_id);
            }

            /* Create tooltip */
            if (this->drag_flag != 0) {                              /* +0x24 */
                void* tooltip = UI_CreateTooltip(
                    &g_tooltip_mgr, 0x3887, 1,
                    this->x + 0x32,                                   /* +0x08 */
                    this->y + 0x32);                                  /* +0x0C */
                this->tooltip_id = tooltip;                          /* +0xA0 */
            }

            if (modeResult != 0) {
                /* Set resource mapping flag */
                if (this->resource != NULL) {                        /* +0x40 */
                    ((RESDATA*)this->resource)->frame_width = 1;
                    *(uint32_t*)((uint8_t*)this->resource + 0x164) |=
                        *(uint32_t*)((uint8_t*)this->resource + 0x164) | 2;
                }

                /* Set dispatch state = 0 (idle) */
                this->dispatch_state = 0;                            /* +0x740 */
                this->field_2C |= 2;                                 /* +0x2C */
                this->field_88 = 0;                                     /* +0x88 */
                g_active_panel = 0;

                /* Set animation state 0 */
                (*(void (**)(void*, int32_t))((uint8_t*)(*(void**)this) + 7))(this, 0);

                /* Move to position (50, 10) */
                (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 3))(this, 0x32, 10);
                return 1;
            }
        }
    }

    return (uint32_t)initResult & 0xFFFFFF00;
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::Update — Per-frame update                     */
/* Address: 0x4497A0                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* 4-state machine: 0=idle (hover), 1=in-world (bounds), 2=drag,       */
/* 3=placed/active. Called every frame from GameLoop_FrameUpdate.      */
/* ==================================================================== */
void __fastcall RESDATA_ScriptedObject::Update()
{
    int16_t dispatchState = this->dispatch_state;                   /* +0x740 */

    /* State 1: In-world — clamp to world bounds */
    if (dispatchState == 1) {
        if (g_town_mode != 0) {
            extern void __thiscall Town_SelectBuilding(void* town, int32_t building);
            Town_SelectBuilding(g_town_view, 0);
        }
        if (g_ddraw_active != 0) {
            extern void __thiscall DDRAW_SelectBuilding(void* ddraw, int32_t building);
            DDRAW_SelectBuilding(g_ddraw_building, 0);
        }

        int32_t* objPtr = (int32_t*)this;
        int32_t x = this->x;                  /* +0x08 */
        int32_t y = this->y;                  /* +0x0C */

        /* Clamp to world bounds */
        if (x < 0) {
            (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 3))(this, x - 1, y);
        }
        if (g_world_width < objPtr[4]) {
            (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 3))(this, 
                objPtr[4] - 1 + x, y);
        }
        if (y < 0) {
            (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 3))(this, 
                x, y - 1);
        }
        if (g_world_height - objPtr[15] < y) {
            (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 3))(this, 
                x, y + objPtr[15] - 1);
        }
    }

    /* States 1 or 2: Animation update + tooltip + entity update */
    if (dispatchState == 1 || dispatchState == 2) {
        GameObject_Update(this);

        if (this->tooltip_id != NULL) {                             /* +0xA0 */
            (*(void (**)(void))((uint8_t*)(*(void**)this->tooltip_id) + 10))();
        }

        (*(void (**)(void))((uint8_t*)(*(void**)this) + 1))();

        /* Check animation frame completion for state transitions */
        int32_t animIndex = this->anim_index;                        /* +0x54 */
        void* frameData = this->resource;                            /* +0x40 */
        uint16_t* animTable = *(uint16_t**)((uint8_t*)frameData + 0x20);
        uint16_t startFrame = animTable[0x19];  /* anim_table[start_frame] */

        if (animIndex == (uint32_t)startFrame) {
            /* Animation reached start — return to idle */
            (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 6))(this, 0x2401, -1);
            (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 3))(this, 
                x + 0x31, y + 0x2F);
            this->dispatch_state = 0;                                /* +0x740 */

            if (this->tooltip_id != 0) {                             /* +0xA0 */
                UI_DestroyTooltip(&g_tooltip_mgr, (int32_t)this->tooltip_id);
            }
            if (this->drag_flag != 0) {                              /* +0x24 */
                void* tooltip = UI_CreateTooltip(
                    &g_tooltip_mgr, 0x3887, 0,
                    this->x + 0x32,                                   /* +0x08 */
                    this->y + 0x32);                                  /* +0x0C */
                this->tooltip_id = tooltip;                          /* +0xA0 */
            }
            g_active_panel = NULL;
            CGWND_SetMode((void*)3);

            SetRect(&this->drag_handle,                          /* drag_handle inside GameObject */
                this->x + 0x18,                                       /* +0x08 */
                this->y + 3,                                          /* +0x0C */
                this->x + 0x2C,
                this->y + 0x0D);
            TileMap_InvalidateRect(g_tilemap,
                this->drag_handle.left,
                this->drag_handle.top,
                this->drag_handle.right,
                this->drag_handle.bottom);

        } else {
            uint16_t endFrame = *(uint16_t*)((uint8_t*)frameData + 0x1A);
            if (animIndex == (uint32_t)endFrame) {
                /* Animation reached end — switch to placed state */
                this->field_88 = 1;                                     /* +0x88 */
                (*(void (**)(void))((uint8_t*)(*(void**)this) + 1))();
                this->dispatch_state = 3;                            /* +0x740 */

                g_active_panel = this;
                this->drag_handle.left = ((GameObject*)this->gameobject)->screen_rect.left;
                this->drag_handle.top = ((GameObject*)this->gameobject)->screen_rect.top;
                this->drag_handle.right = ((GameObject*)this->gameobject)->screen_rect.right;
                this->drag_handle.bottom = ((GameObject*)this->gameobject)->screen_rect.bottom;
                TileMap_InvalidateRect(g_tilemap,
                    ((GameObject*)this->gameobject)->screen_rect.left,
                    ((GameObject*)this->gameobject)->screen_rect.top,
                    ((GameObject*)this->gameobject)->screen_rect.right,
                    ((GameObject*)this->gameobject)->screen_rect.bottom);

                /* Update children via linked list at/* +0xD0 */
                for (void* child = this->child_list_head;            /* +0xD0 */
                     child != NULL;
                     child = *(void**)((uint8_t*)child + 0x28)) {
                    (*(void (**)(void))((uint8_t*)(*(void**)child) + 8))();
                }

                /* Play narration if not scenario 2 */
                if (*(int32_t*)((uint8_t*)g_netman + 0x5C) != 2) {
                    HelpWnd_PlayNarration(g_audio_mgr, 6, 0);
                }
            }
        }
    }

    /* State 2 (dragging): follow cursor */
    if (this->drag_flag == 1) {                                    /* +0x24 */
        (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 3))(this, 
            g_cursor_world_x - this->drag_offset_x,
            g_cursor_world_y - this->drag_offset_y);
        return;
    }

    /* State 0 (idle): hover detection */
    if (dispatchState == 0) {
        char hitResult = (char)(*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 0x15))(this, 
            g_cursor_world_x, g_cursor_world_y);
        if (hitResult == 0 && this->drag_flag != 1) {              /* +0x24 */
            if (this->tooltip_state == 1) {
                (*(void (**)(void*, int32_t))((uint8_t*)(*(void**)this) + 7))(this, 0);
            } else {
                GameObject_Update(this);
            }
        } else if (this->tooltip_state != 1) {
            (*(void (**)(void*, int32_t))((uint8_t*)(*(void**)this) + 7))(this, 1);
        }
    }

    /* State 3 (placed): dispatch to sub-objects */
    if (dispatchState != 3) return;

    char hoverResult = (char)(*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 0x15))(this, 
        g_cursor_world_x, g_cursor_world_y);
    if ((hoverResult == 0 && this->drag_flag == 0)) {              /* +0x24 */
        if (((Entity*)this->gameobject)->anim_index == 0) {
            goto update_children;
        }
        (*(void (**)(void*, int32_t))((uint8_t*)(*(void**)this->gameobject) + 7))(this, 0);
    } else {
        if (((Entity*)this->gameobject)->anim_index == 1) {
            goto update_children;
        }
        (*(void (**)(void*, int32_t))((uint8_t*)(*(void**)this->gameobject) + 7))(this, 1);
    }
    TileMap_InvalidateRect(g_tilemap,
        this->drag_handle.left,
        this->drag_handle.top,
        this->drag_handle.right,
        this->drag_handle.bottom);

update_children:
    /* Dispatch to children via linked list */
    for (int32_t child = (int32_t)this->child_list_head;          /* +0xD0 */
         child != 0;
         child = *(int32_t*)((uint8_t*)(uintptr_t)child + 0x28)) {
        (*(void (**)(void*, int32_t))((uint8_t*)(*(void**)this) + 0x14))(this, child);
    }

    /* Update ScriptEngine if active */
    if (this->scriptengine_visible != 0) {                /* inside scriptengine sub-object */
        (*(void (**)(void))((uint8_t*)(*(void**)this->scriptengine) + 10))();  /* +0x178 */
    }

    /* Update ScrollPanel if active */
    if (this->scrollpanel_visible != 0) {                          /* +0x2E8 */
        (*(void (**)(void))((uint8_t*)(*(void**)this->scrollpanel) + 10))();   /* +0x260 */
    }
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::Dispatch (Draw)                               */
/* Address: 0x449C00                                                   */
/* __thiscall (this)                                                    */
/*                                                                      */
/* Draws ScriptedObject and conditionally draws sub-objects.            */
/* ==================================================================== */
void __thiscall RESDATA_ScriptedObject::Dispatch()
{
    GameObject_Draw(this);

    if (this->dispatch_state == 3) {                               /* +0x740 */
        /* Draw child GameObject via its vtable[11] */
        (*(void (**)(void))((uint8_t*)(*(void**)this->gameobject) + 11))();
    }
    if (this->scriptengine_visible == 1) {
        /* Draw ScriptEngine via vtable[11] */
        (*(void (**)(void))((uint8_t*)(*(void**)this->scriptengine) + 11))();
    }
    if (this->scrollpanel_visible == 1) {
        /* Draw ScrollPanel via vtable[11] */
        (*(void (**)(void))((uint8_t*)(*(void**)this->scrollpanel) + 11))();
    }
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::IsDragging (PtInRect)                         */
/* Address: 0x449CE0                                                   */
/* __thiscall (this, x, y)                                              */
/*                                                                      */
/* Delegates to GameObject_PtInRect.                                    */
/* ==================================================================== */
bool __thiscall RESDATA_ScriptedObject::IsDragging(int32_t x, int32_t y)
{
    return GameObject_PtInRect(this, x, y) != 0;
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::CheckClick                                    */
/* Address: 0x449D00                                                   */
/* __thiscall (this, x, y)                                              */
/*                                                                      */
/* Multi-layered click hit-test.                                        */
/* ==================================================================== */
bool __thiscall RESDATA_ScriptedObject::CheckClick(int32_t x, int32_t y)
{
    bool hit = false;

    /* Try own PtInRect (vtable[2]) */
    char result = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 2))(this, x, y);
    if (result == 0) {
        /* Try secondary hit-test (vtable[0x15] = slot 21) */
        result = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 0x15))(this, x, y);
        if (result == 0) {
            /* Try ScriptEngine (vtable[8]) if visible */
            if (this->scriptengine_visible != 0) {
                result = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->scriptengine) + 2))(this, x, y);
                hit = (result != 0);
            }
        } else {
            hit = true;
        }
    } else {
        hit = true;
    }

    /* Try ScrollPanel if not hit yet */
    if (!hit && this->scrollpanel_visible != 0) {
        result = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->scrollpanel) + 2))(this, x, y);
        hit = (result != 0);
    }

    return hit;
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::GetDragOffset                                  */
/* Address: 0x449D80                                                   */
/* __thiscall (this, x, y)                                              */
/*                                                                      */
/* Tests drag-handle rect (+0x168..+0x174). Returns 1 if inside.        */
/* ==================================================================== */
uint32_t __thiscall RESDATA_ScriptedObject::GetDragOffset(int32_t x, int32_t y)
{
    if (this->drag_handle.left <= x &&
        x < this->drag_handle.right &&
        this->drag_handle.top <= y &&
        y < this->drag_handle.bottom) {
        return 1;
    }
    return 0;
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::MoveTo                                       */
/* Address: 0x449DC0                                                   */
/* __thiscall (this, x, y)                                              */
/*                                                                      */
/* Move object to (x,y) with boundary clamping.                         */
/* ==================================================================== */
void __thiscall RESDATA_ScriptedObject::MoveTo(int32_t x, int32_t y)
{
    int16_t state = this->dispatch_state;                          /* +0x740 */

    /* Clamp position unless in placement mode (state 1) */
    if (state != 1) {
        if (this->direction == 1) {                                /* +0xAD */
            /* Direction = right */
            if (x < 0) x = 0;
            int32_t maxX;
            if (this->scriptengine_visible != 0 &&        /* inside scriptengine */
                (maxX = g_world_width - ((ScriptEngine*)this->scriptengine)->field_38,
                 maxX < x)) {
                x = maxX;
            } else if (this->scrollpanel_visible != 0 &&           /* +0x2E8 */
                       (maxX = g_world_width - *(int32_t*)(this->scrollpanel + 0x38),
                        maxX < x)) {
                x = maxX;
            } else {
                uint16_t frameWidth = ((RESDATA*)this->resource)->frame_width;  /* +0x40 */
                maxX = g_world_width - (uint32_t)frameWidth;
                if (maxX < x) x = maxX;
            }
        } else {
            /* Direction = left */
            if (x < 0) x = 0;
            int32_t minX;
            if (this->scriptengine_visible != 0 &&
                x < (minX = ((ScriptEngine*)this->scriptengine)->field_38)) {
                x = minX;
            } else if (this->scrollpanel_visible != 0 &&           /* +0x2E8 */
                       x < (minX = *(int32_t*)(this->scrollpanel + 0x38))) {
                x = minX;
            } else {
                uint16_t frameWidth = ((RESDATA*)this->resource)->frame_width;
                int32_t maxX = g_world_width - (uint32_t)frameWidth;
                if (maxX < x) x = maxX;
            }
        }

        /* Clamp Y */
        if (y < 0) y = 0;
        int32_t maxY = g_world_height - this->field_3C;
        if (maxY < y) y = maxY;
    }

    /* Set position */
    RESDATA_SetPosition(this, x, y);

    /* Update child GameObject position with offsets */
    (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->gameobject) + 3))(this, 
        ((RESDATA*)((Entity*)this->gameobject)->resource)->offset_x + x,
        ((RESDATA*)((Entity*)this->gameobject)->resource)->offset_y + y);

    /* Update ScriptEngine if active */
    if (this->direction == 0) {
        /* Left direction */
        if (this->scriptengine_visible != 0) {
            (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->scriptengine) + 3))(this, 
                x - ((ScriptEngine*)this->scriptengine)->field_38, y + 14);
        }
        if (this->scrollpanel_visible != 0) {
            (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->scrollpanel) + 3))(this, 
                x - *(int32_t*)(this->scrollpanel + 0x38), y + 14);
        }
    } else {
        /* Right direction */
        if (this->scriptengine_visible != 0) {
            uint16_t frameWidth = ((RESDATA*)this->resource)->frame_width;
            (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->scriptengine) + 3))(this, 
                (uint32_t)frameWidth + x, y + 14);
        }
        if (this->scrollpanel_visible != 0) {
            uint16_t frameWidth = ((RESDATA*)this->resource)->frame_width;
            (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->scrollpanel) + 3))(this, 
                (uint32_t)frameWidth + x, y + 14);
        }
    }

    /* Update drag-handle rect */
    if (state == 0) {
        SetRect(&this->drag_handle,
            this->x + 0x18,
            this->y + 3,
            this->x + 0x2C,
            this->y + 0x0D);
    } else {
        this->drag_handle.left = ((GameObject*)this->gameobject)->screen_rect.left;
        this->drag_handle.top = ((GameObject*)this->gameobject)->screen_rect.top;
        this->drag_handle.right = ((GameObject*)this->gameobject)->screen_rect.right;
        this->drag_handle.bottom = ((GameObject*)this->gameobject)->screen_rect.bottom;
    }

    /* Update cursor position if tracking enabled */
    if (this->drag_state != 0 &&
        (x != this->x || y != this->y)) {
        int32_t offsetX = x - g_viewport_x + this->drag_offset_x;
        int32_t offsetY = y - g_viewport_y + this->drag_offset_y;

        /* Perform ClientToScreen + SetCursorPos */
        extern void* g_main_window;
        int32_t screenPoint[2];
        screenPoint[0] = offsetX;
        screenPoint[1] = offsetY;
        ClientToScreen(*(void**)((uint8_t*)g_main_window + 8), screenPoint);
        SetCursorPos(screenPoint[0], screenPoint[1]);

        /* Update last cursor position */
        extern int32_t g_last_cursor_pos;
        g_last_cursor_pos = offsetX | (offsetY << 16);

        Game_CheckScreensaverTimeout(&g_game);
    }

    /* Update tooltip position */
    if (this->tooltip_id != NULL) {
        (*(void (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)(this->tooltip_id)) + 3))(this, 
            this->x + 0x32,
            this->y + 0x32);
    }
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::HitTest                                       */
/* Address: 0x44A0C0                                                   */
/* __thiscall (this, x, y)                                              */
/*                                                                      */
/* Hit-test against world-space point. Handles drag, child objects,     */
/* and build-mode entry.                                                */
/* ==================================================================== */
uint8_t __thiscall RESDATA_ScriptedObject::HitTest(int32_t x, int32_t y)
{
    int16_t state = this->dispatch_state;                          /* +0x740 */

    /* Skip if in placement/drag mode, or placed mode with train station active */
    if (state == 1 || state == 2) return 0;
    if (state == 3) {
        if (*(uint8_t*)((uint8_t*)g_trainstation_window + 0x1BC) != 0) return 0;
    }

    /* Cancel any active drag tracking */
    if (this->drag_state != 0) {
        this->drag_state = 0;
        return 1;
    }

    /* Try secondary hit-test (vtable[0x15]) */
    char result = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 0x15))(this, x, y);
    if (result != 0 && (state == 0 || state == 3)) {
        /* Initiate drag — store cursor offset */
        this->drag_offset_x = g_drag_start_x - this->x;               /* +0x94, +0x08 */
        this->drag_offset_y = g_drag_start_y - this->y;               /* +0x98, +0x0C */
        this->drag_state = 1;
        return 1;
    }

    /* If in non-idle state, try child objects */
    if (state != 0) {
        /* Try ScriptEngine child */
        if (this->scriptengine_visible != 0) {            /* inside scriptengine */
            result = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->scriptengine) + 2))(this, x, y);  /* +0x178 */
            if (result != 0) {
                result = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->scriptengine) + 4))(this, x, y);
                if (this->scriptengine_visible != 0) return (uint8_t)result;
                this->direction = 1;                               /* +0xAD */
                return (uint8_t)result;
            }
        }

        /* Try ScrollPanel child */
        if (this->scrollpanel_visible != 0) {                      /* +0x2E8 */
            result = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->scrollpanel) + 2))(this, x, y);  /* +0x260 */
            if (result != 0) {
                result = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this->scrollpanel) + 4))(this, x, y);
                if (this->scrollpanel_visible != 0) return (uint8_t)result;
                this->direction = 1;                               /* +0xAD */
                return (uint8_t)result;
            }
        }

        /* Try children linked list */
        return (uint8_t)RESDATA_HitTestChildren(this, x, y);
    }

    /* Idle state: try own hit-test -> enter build mode */
    if (g_disable_input == 0) {
        result = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)this) + 2))(this, x, y);
        if (result != 0) {
            RESDATA_ScriptedObject::EnterBuildMode(1);
            return 1;
        }
    }

    return 0;
}

/* ==================================================================== */
/* RESDATA_ScriptedObject::HandleToolClick                                */
/* Address: 0x44A250                                                   */
/* __thiscall (this, toolObj, x, y)                                     */
/*                                                                      */
/* Central tool interaction handler. Dispatches by tool type.           */
/* Tool types: 0x2403-0x240E = track placement, build, switch, etc.    */
/* ==================================================================== */
uint32_t __thiscall RESDATA_ScriptedObject::HandleToolClick(void* toolObj, int32_t x, int32_t y)
{
    if (toolObj == NULL) return 0;

    /* Check tool is active and in valid position */
    if (*(uint8_t*)((uint8_t*)toolObj + 0x56) == 0) return 0;

    /* Check PtInRect on tool */
    char hitResult = (*(char (**)(void*, int32_t, int32_t))((uint8_t*)(*(void**)toolObj) + 2))(this, x, y);
    if (hitResult == 0) return 0;

    int32_t toolType = *(int32_t*)((uint8_t*)(uintptr_t)(*(int32_t*)((uint8_t*)toolObj + 0x44)) + 4);
    int32_t toolIndex = toolType - 0x2403;

    switch (toolIndex) {
    case 0:  /* 0x2403 — Track tool */
    case 1:  /* 0x2404 — Track tool */
        if (*(int16_t*)((uint8_t*)toolObj + 0x48) != 1) {
            CGWND_TrackPiece_SetZoom(toolObj, 1);
            this->direction = 0;                                   /* +0xAD */
            return (*(uint32_t (**)(void*, void*, int32_t))((uint8_t*)(*(void**)this->scriptengine) + 0x15))(this, 
                toolObj, 0) | 1;                                   /* +0x178 */
        }
        /* Fall through to common tool activation */
        goto common_tool_activate;

    case 2:  /* 0x2405 — Track tool */
        if (*(int16_t*)((uint8_t*)toolObj + 0x48) != 1) {
            CGWND_TrackPiece_SetZoom(toolObj, 1);
            this->direction = 0;                                   /* +0xAD */
            return (*(uint32_t (**)(void*, void*, int32_t))((uint8_t*)(*(void**)this->scriptengine) + 0x15))(this, 
                toolObj, 0) | 1;                                   /* +0x178 */
        }
        goto common_tool_activate;

    case 3:  /* 0x2406 — Build mode toggle */
        if (*(int16_t*)((uint8_t*)toolObj + 0x48) != 1) {
            CGWND_TrackPiece_SetZoom(toolObj, 1);
            CGWND_SetBuildMode(0);
            return 1;
        }
        HelpWnd_PlayNarration(g_audio_mgr, 7, 0x2406);
        CGWND_TrackPiece_SetZoom(toolObj, 2);
        CGWND_SetBuildMode(1);
        return 1;

    case 6:  /* 0x2409 — Track tool */
        if (*(int16_t*)((uint8_t*)toolObj + 0x48) != 1) {
            CGWND_TrackPiece_SetZoom(toolObj, 1);
            this->direction = 0;                                   /* +0xAD */
            UIPANEL_ScrollPanel_HandleDrag(this->scrollpanel, (int32_t)toolObj, 0);  /* +0x260 */
            return 1;
        }
        goto common_tool_activate;
    }

common_tool_activate:
    return;
}