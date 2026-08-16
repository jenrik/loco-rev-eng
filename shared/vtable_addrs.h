/**
 * vtable_addrs.h — Vtable address constants for Lego Loco C++ decompilation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These are the concrete vtable pointers found in the .rdata section
 * and assigned during object construction. Each vtable is an array of
 * function pointers; slot 0 is always the scalar-deleting destructor
 * (MSVC convention).
 *
 * Naming: VTBL_<ClassName>[_<variant>]
 *   _BASE  = intermediate vtable set by base constructor
 *   _FULL  = final vtable set by most-derived constructor
 *   _ALT   = alternative entry (same table, different view)
 */

#pragma once

/* ================================================================== */
/* GameObject hierarchy                                                */
/* ================================================================== */
#define VTBL_GAMEOBJECT                0x00477820  /* GameObject root vtable
    Base vtable: 4 slots only:
    [0] +0x00: BaseScalarDtor (0x412600)
    [1] +0x04: InvalidateRect (0x436AB0)
    [2] +0x08: PtInRect (0x436A10)
    [3] +0x0C: MoveTo (0x436A60)
    Full 15-slot layout in VTBL_ENTITY below.                             */
#define VTBL_GAMEOBJECT_SCALAR_DTOR    0x00477820  /* vtable[0] — scalar dtor, 0x412600 */

#define VTBL_ENTITY                    0x00477488  /* Entity vtable (15 slots)
    [0]  +0x00: scalar deleting destructor  (Entity_ScrDtor, 0x405850)
    [1]  +0x04: InvalidateRect              (0x436AB0)
    [2]  +0x08: PtInRect                    (0x436A10)
    [3]  +0x0C: HitTest dispatch            (0x405680, overrides MoveTo)
    [4]  +0x10: (unknown)
    [5]  +0x14: (unknown)
    [6]  +0x18: InitBase                    (0x405900)
    [7]  +0x1C: StopSound                   (0x405A20)
    [8]  +0x20: SetFrame                    (0x405DE0)
    [9]  +0x24: (unknown)
    [10] +0x28: (unknown)
    [11] +0x2C: Draw                        (0x405E60)
    [12] +0x30: DrawConnected               (0x405FD0)
    [13] +0x34: SetName                     (0x405E20)
    [14] +0x38: SetAnimState                (0x405A50)                     */

#define VTBL_GAME                      0x00477718  /* Game vtable
    [0]  +0x00: scalar deleting destructor (Game_Dtor, 0x410660)
                    -> calls ~Game() base destructor (Game_BaseDtor, 0x410680)
    [1]  +0x04: StopSound                    (Game_StopSound, 0x410CF0)
    [2]  +0x08: release resource             (Game_ReleaseResource, 0x410D60)
    [3]  +0x0C: HitTest dispatch             (Game_HitTest, 0x40F160)
    [4]  +0x10: (unknown)                    (Game_method4, 0x410DC0)
    [5]  +0x14: (unknown)                    (Game_method5, 0x410DF0)
    [6]  +0x18: InitBase                     (Game_InitBase, 0x40E1A0)
    [7]  +0x1C: SetAnimState                 (Game_ScreenModeSet, 0x40EA10)
    [8]  +0x20: SetFrame                     (Game_SetFrame, 0x40E9E0)
    [9]  +0x24: SetName                      (Game_SetName, 0x410E20)
    [10] +0x28: Draw                         (Game_Draw, 0x40F170)
    [11] +0x2C: DrawConnected                (Game_DrawConnected, 0x4108B0)
    [12] +0x30: OnTimerTick                  (Game_OnTimerTick, 0x410A50)
    [13] +0x34: (unknown)                    (Game_method13, 0x40FFE0)
    [14] +0x38: AnimStateSelect              (Game_AnimStateSelect, 0x40EA30) */

/* ================================================================== */
/* CGWND — Main game window / application                              */
/* ================================================================== */
#define VTBL_CGWND                     0x004774C4  /* CGWND vtable                */

/* ================================================================== */
/* Building — In-game building/vehicle entity                           */
/* ================================================================== */
#define VTBL_BUILDING_FULL             0x00477EB8  /* Building complete vtable    */
#define VTBL_BUILDING_BASE             0x00477F18  /* Building_BaseCtor vtable    */

#define VTBL_BUILDING_CARRIAGE         0x00477EF8  /* Carriage/vehicle part vtable */

/* 0x478008 was formerly labeled VTBL_BUILDING_COMPLEX. loco_v8 shows
 * collection methods there (0x436040, 0x424010, 0x424760), and no code
 * installs or references 0x478008 as a Building-derived vtable. */
#define VTBL_COLLECTION_TAIL_478008    0x00478008  /* legacy analysis marker */

/* ================================================================== */
/* TrackPos — Grid-cell track position struct (20 bytes, vtable at 0x477840) */
/* ================================================================== */
#define VTBL_TRACKPOS                  0x00477840  /* TrackPos vtable
    Struct size: 0x14 (20 bytes): vtable + 4 int32_t fields.
    Functions:
      TrackPos_Init       (0x412620)  — full init: vtable + all 4 fields to -1
      TrackPos_BaseInit   (0x412660)  — lightweight: vtable only
      TrackPos_IsObjectBetween (0x412670) — 1D circular track overlap check     */

/* ================================================================== */
/* Train / Subsystem — Locomotive train and rail subsystem              */
/* ================================================================== */
#define VTBL_TRAIN_ENTITY              0x004780B8  /* TrainEntity (Building-derived) vtable */
#define VTBL_TRAIN_SUBSYSTEM           0x004781C4  /* TrainSubsystem (network manager) vtable */
#define VTBL_TRAIN_STATION_WINDOW      0x00478130  /* TrainStationWindow (GameWindow-derived) vtable */

/* ================================================================== */
/* Netman — Multiplayer networking manager                              */
/* ================================================================== */
#define VTBL_NETMAN                    0x004781C8  /* Netman vtable
    [0]  +0x00: scalar deleting destructor (NET_Dtor_ScalarDeleting, 0x453C10) */

/* ================================================================== */
/* DPlayConfig (GameConfig) — Network session configuration manager     */
/* ================================================================== */
#define VTBL_DPLAY_CONFIG              0x004781CC  /* DPlayConfig vtable
    [0]  +0x00: scalar deleting destructor (NETMAN_FreeProviderList, 0x440CC0)
    Size: 0xB0 bytes.
    Manages: session name, player counts, provider list, timeout.
    Persisted to NetSettings.dat via LoadSettings/SaveSettings.        */

/* ================================================================== */
/* NetworkPlayerList — DPLAY-level player list / surface cache          */
/* ================================================================== */
#define VTBL_NETWORK_PLAYER_LIST       0x00478268  /* NetworkPlayerList vtable
    [1]  +0x04: (unknown)
    Full struct derives from a base with surface cache arrays.          */

/* ================================================================== */
/* DPLAY_PlayerSlot — Per-player slot with track entries                */
/* ================================================================== */
#define VTBL_DPLAY_PLAYER_SLOT         0x00478264  /* DPLAY_PlayerSlot vtable
    [0]  +0x00: scalar deleting destructor (DPLAY_CleanupPlayer, 0x442A00) */

/* ================================================================== */
/* DPLAY_SessionData — Serialized player snapshot for network use       */
/* ================================================================== */
#define VTBL_DPLAY_SESSION_DATA        0x00478268  /* Alias: same as VTBL_NETWORK_PLAYER_LIST */

/* ================================================================== */
/* NameEntryPanel — Multiplayer name-entry and lobby panel              */
/* Derived from UI_WindowBase (vtable 0x477C30). Size: ~0x178 bytes.    */
/* ================================================================== */
#define VTBL_NAMEENTRYPANEL            0x004781D0  /* NameEntryPanel vtable
    [0]  +0x00: scalar deleting destructor (0x440F80)
    [1]  +0x04: Hide                        (inherited: UI_WindowBase_Hide, 0x425990)
    [2]  +0x08: Show                        (inherited: UI_WindowBase_Show, 0x4259C0)
    [3]  +0x0C: virtual (default stub)      (inherited: 0x425FD0)
    [4]  +0x10: virtual (default stub)      (inherited: 0x426020)
    [5]  +0x14: virtual (default stub)      (inherited: 0x426130)
    [6]  +0x18: CreateFullWindow            (inherited: UI_CreateFullWindow, 0x425B70)
    [7]  +0x1C: OnCreate                    (inherited: UI_WindowBase_OnCreate, 0x425D30)
    [8]  +0x20: Render / draw               (NameEntryPanel_Draw, 0x441190)
    [9]  +0x24: virtual (default no-op)     (inherited: 0x4661A0)
    [10] +0x28: virtual (default stub)      (inherited: 0x426140)
    [11] +0x2C: WindowProc                  (inherited: UI_DefWndProc, 0x422EA0) */

/* ================================================================== */
/* GameSetupPanel — City/scenario selection lobby panel                 */
/* Derived from UI_WindowBase (vtable 0x477C30). Size: 0x260 bytes.    */
/* ================================================================== */
#define VTBL_GAMESETUPPANEL            0x004774D0  /* GameSetupPanel vtable
    [0]  +0x00: scalar deleting destructor (0x408B00)
    [1]  +0x04: Hide                        (inherited: UI_WindowBase_Hide, 0x425990)
    [2]  +0x08: Show                        (inherited: UI_WindowBase_Show, 0x4259C0)
    [3]  +0x0C: virtual (default stub)      (inherited: 0x425FD0)
    [4]  +0x10: virtual (default stub)      (inherited: 0x426020)
    [5]  +0x14: virtual (default stub)      (inherited: 0x426130)
    [6]  +0x18: CreateFullWindow            (inherited: UI_CreateFullWindow, 0x425B70)
    [7]  +0x1C: OnCreate                    (inherited: UI_WindowBase_OnCreate, 0x425D30)
    [8]  +0x20: Render/Update               (GameSetupPanel_Render, 0x409280)
    [9]  +0x24: virtual (default no-op)     (inherited: 0x4661A0)
    [10] +0x28: virtual (default stub)      (inherited: 0x426140)
    [11] +0x2C: WindowProc                  (inherited: UI_DefWndProc, 0x422EA0)
    [12] +0x30: HandleMapClick              (0x40ABA0)
    [13] +0x34: SelectLayoutEntry           (0x40AAF0)
    [14] +0x38: SendScenarioSelect          (0x40AC50)
    [15] +0x3C: ConnectToNetworkGame        (0x40AA20) */

/* ================================================================== */
/* PostcardAlbum — Postcard collection album window                     */
/* Derived from UI_WindowBase (vtable 0x477C30).                       */
/* ================================================================== */
#define VTBL_POSTCARD_ALBUM            0x004773F0  /* PostcardAlbum window vtable */
#define VTBL_LOCOBITMAP_ALT            0x004773F0  /* Alias: same table, used in FreeAllSprites */

/* ================================================================== */
/* PostcardPreviewWindow — Postcard send/preview dialog                 */
/* Derived from UI_WindowBase (vtable 0x477C30).                       */
/* ================================================================== */
#define VTBL_POSTCARD_PREVIEW          0x004778F8  /* PostcardPreviewWindow vtable (TBD) */

/* ================================================================== */
/* PixelDataCache — Resource frame data cache object                   */
/* ================================================================== */
#define VTBL_PIXELDATA_CACHE           0x004773E8  /* PixelDataCache vtable        */

/* ================================================================== */
/* SaveSprite — savegame/backdrop file-list entry (ui/SaveSprite.h)     */
/* Root class, single-slot vtable (destructor only). Immediately        */
/* precedes VTBL_UIPANEL_Surface below in the .rdata section -- the two */
/* are unrelated, adjacent single-entry vtables, NOT one multi-slot     */
/* table (confirmed by decompiling the dwords following 0x477D24: they  */
/* belong to UIPANEL_Surface/GameView/GameObject, not SaveSprite).      */
/* ================================================================== */
#define VTBL_SAVESPRITE                0x00477D24  /* SaveSprite vtable
    [0] +0x00: ~SaveSprite (scalar deleting dtor, 0x429830)               */

/* ================================================================== */
/* UIPANEL_Surface — Embedded offscreen DDraw surface wrapper           */
/* ================================================================== */
#define VTBL_UIPANEL_Surface           0x00477D28  /* UIPANEL_Surface vtable      */

/* ================================================================== */
/* Timer classes                                                        */
/* ================================================================== */
#define VTBL_TIMER2_BASE              0x00477FE0  /* Timer2 base vtable
    Parallel variant of Timer with separate vtable.                       */
#define VTBL_TIMER_BASE                0x00478070  /* Timer base vtable
    The Timer class inherits from SortedCollection, which inherits from
    Collection. Its vtable embeds the Collection virtual methods at
    known offsets:
      [0]  +0x00: Timer_Resize          (0x435D10)
      [3]  +0x0C: Collection_RemoveAt   (0x4356B0)
      [12] +0x30: SortedCollection_SetAt (0x435A10)
      [15] +0x3C: SortedCollection_QuickSortRange (0x435AA0)
      [18] +0x48: Compare (or Timer_IsSorted dispatch)
    This vtable is the "Timer as Collection" view set during destruction.
    The full vtable for the most-derived Timer class is larger and
    contains additional TimerList-specific methods.                     */

/* ================================================================== */
/* Collection / SortedCollection — Generic dynamic array containers     */
/*                                                                       */
/* These are NOT standalone classes with their own vtables — instead,   */
/* the vtable entries for Collection/SortedCollection operations are     */
/* embedded in the vtables of derived classes (Timer, TimerList, etc.).  */
/*                                                                       */
/* Virtual method offsets (shared across all Collection derivatives):   */
/*   [0]  +0x00: Resize           (e.g. Timer_Resize @ 0x435D10)        */
/*   [3]  +0x0C: RemoveAt         (Collection_RemoveAt @ 0x4356B0)      */
/*   [7]  +0x1C: GetAt            (varies by class)                     */
/*   [12] +0x30: SetAt            (SortedCollection_SetAt @ 0x435A10)   */
/*   [15] +0x3C: SortRange        (SortedCollection_QuickSortRange @ 0x435AA0) */
/*   [18] +0x48: Compare          (varies by element type)              */
/*                                                                       */
/* Data references to Collection_RemoveAt (0x4356B0) appear at vtable   */
/* offset +0x0C (slot 3) across: 0x4777A4, 0x477BDC, 0x477B4C,         */
/* 0x47807C, 0x477FEC.                                                  */
/*                                                                       */
/* SortedCollection2 (templated variant, byte-identical to              */
/* SortedCollection) has its SetAt at 0x4360B0, referenced from         */
/* vtable slot at 0x477FB0.                                             */
/* ================================================================== */
#define VTBL_SORTED_COLLECTION2         0x00477FB0  /* SortedCollection2 vtable
    Byte-identical twin of SortedCollection (different template instantiation).
    SortedCollection2_SetAt @ 0x4360B0 at vtable offset +0x30 from 0x477FB0.  */

/* ================================================================== */
/* PlayerConfig / PlayerRecord                                          */
/* ================================================================== */
#define VTBL_PLAYERCONFIG              0x004784BC  /* PlayerConfig descriptor     */
#define VTBL_PLAYERRECORD              0x004784C0  /* PlayerRecord descriptor     */

/* ================================================================== */
/* Panel — UI widget panel (GameObject subclass)                        */
/* ================================================================== */
#define VTBL_PANEL                     0x004784C8  /* Panel vtable                */

/* ================================================================== */
/* UIPANEL — Scrollable building picker panel (Panel subclass)          */
/* ================================================================== */
#define VTBL_UIPANEL                   0x00477CC8  /* UIPANEL vtable              */

/* ================================================================== */
/* GameWindow — Base for DDraw-backed overlay windows                   */
/* ================================================================== */
#define VTBL_GAMEWINDOW                0x00477898  /* GameWindow vtable
    [0]  +0x00: scalar deleting destructor (0x413B50)
    [1]  +0x04: hide                   (0x413C10)
    [2]  +0x08: show                   (0x413D10)
    [3]  +0x0C: set_mode               (0x414340)
    [4]  +0x10: cleanup_sprites        (0x426130)
    [5]  +0x14: create                 (0x413DE0)
    [6]  +0x18: init                   (0x4140A0)
    [7]  +0x1C: update_anim            (0x426130)              */

/* ================================================================== */
/* AboutDialog — About/Credits dialog and idle screensaver             */
/* Derived from GameWindow (vtable 0x477898). Size: 0x1184 bytes.      */
/* ================================================================== */
#define VTBL_ABOUTDIALOG              0x00477680  /* AboutDialog vtable
    [0]  +0x00: scalar deleting destructor (CGWND_AboutDialog_Dtor,    0x40F270)
    [1]  +0x04: Hide / screensaver_hide     (CGWND_Screensaver_Hide,   0x40F480)
    [2]  +0x08: Show / screensaver_show     (CGWND_AboutDialog_Show,   0x40F2A0)
    [3]  +0x0C: set_mode                    (inherited: GameWindow_SetMode,   0x414340)
    [4]  +0x10: method_4                    (inherited: stub,                 0x426130)
    [5]  +0x14: Create                      (inherited: GameWindow_Create,    0x413DE0)
    [6]  +0x18: Init / load_credits         (CGWND_AboutDialog_Init,          0x40F5C0)
    [7]  +0x1C: Update screensaver/tick     (CGWND_AboutDialog_UpdateScrn,    0x40F890)  */

/* ================================================================== */
/* HelpWnd (AudioMgr) — Tutorial/help window subsystem                  */
/* Derived from GameWindow (vtable 0x477898). Size: ~0x3190 bytes.     */
/* ================================================================== */
#define VTBL_HELPWND                  0x00478428  /* HelpWnd vtable
    [0]  +0x00: scalar deleting destructor
    [1]  +0x04: Hide                        (HelpWnd_Hide,      0x450AE0)
    [2]  +0x08: Show                        (HelpWnd_Show,      0x450240)
    [3]  +0x0C: set_mode / Cursor dispatch  (inherited)
    [4]  +0x10: method_4 / cleanup_sprites  (HelpWnd_CleanupSprites, 0x451440)
    [5]  +0x14: Create                      (HelpWnd_Create,    0x450CA0)
    [6]  +0x18: Init callback               (HelpWnd_Init,      0x451180)
    [7]  +0x1C: on_show callback            (HelpWnd_UpdateAnim, 0x450450)
    Slots beyond [7] are HelpWnd-specific (WndProc handler, etc.)    */

/* ================================================================== */
/* HelpPageNode — Linked list node for help page data                   */
/* Sub-object used by HelpWnd's page system. Standalone class            */
/* (not a window, not a GameObject).                                    */
/* ================================================================== */
#define VTBL_HELPPAGE_NODE            0x004783D8  /* HelpPageNode vtable
    [0]  +0x00: scalar deleting destructor (HelpWnd_GetPageCount 0x44F2A0)
    [1]  +0x04: base destructor             (HelpWnd_GetPageTitle 0x44F2C0)  */

/* ================================================================== */
/* UI_WindowBase — base class for all game UI windows                   */
/* ================================================================== */
#define VTBL_UI_WINDOWBASE             0x00477C30  /* UI_WindowBase vtable        */

/* Global state shared by all UI_WindowBase instances */
#define ADDR_g_cursor_back             0x004FD3CC  /* shared cursor backbuffer      */
#define ADDR_g_cursor_refcount         0x004FD3D0  /* refcount for cursor backbuf   */

/* ================================================================== */
/* EditWindow (UI_MainMenu) — full-screen main menu dialog              */
/* Derived from UI_WindowBase (vtable 0x477C30).                        */
/* ================================================================== */
#define VTBL_EDITWINDOW                0x004779F8  /* EditWindow vtable
    [0]  +0x00: scalar deleting destructor (0x4203A0)
    [1]  +0x04: Hide (0x420860)
    [2]  +0x08: Show (0x4206B0)
    [3]  +0x0C: virtual method (default stub, 0x425FD0)
    [4]  +0x10: virtual method (default stub, 0x426020)
    [5]  +0x14: virtual method (default stub, 0x426130)
    [6]  +0x18: CreateFullWindow (inherited: UI_CreateFullWindow, 0x425B70)
    [7]  +0x1C: OnCreate (overridden: EditWindow_OnCreate, 0x422930)
    [8]  +0x20: Render/Update (EditWindow_Render, 0x422AA0)
    [9]  +0x24: MouseWheel (overridden: EditWindow_MouseWheel, 0x422950)
    [10] +0x28: virtual method (default stub, 0x426140)
    [11] +0x2C: WindowProc (overridden: EditWindow_WndProc, 0x422600) */

/* ================================================================== */
/* Town — In-game town/city game view window                            */
/* Derived from UI_WindowBase (vtable 0x477C30).                        */
/* ================================================================== */
#define VTBL_TOWN                     0x00477D88  /* Town vtable
    [0]  +0x00: scalar deleting destructor (UI_DtorWrapper, 0x4234E0)
    ... standard UI_WindowBase vtable layout ...
    [8]  +0x20: Render/Update (Town_DrawDispatch, 0x42EDF0)
    ... + more Town-specific virtual slots ...                          */

/* ================================================================== */
/* GameView (historically "TownGameView / ScrollView") — viewport         */
/* scrolling / building-selection helper. Derived GameObject -> Entity -> */
/* Panel -> GameView (verified: game/Panel.h's real base is Entity, not   */
/* GameObject directly).                                                 */
/*                                                                       */
/* This block used to document a stale, unverified 15-slot layout        */
/* (Draw at 0x42F900, etc.) — none of those addresses had any real        */
/* xrefs. core/GameView.h is now the canonical, byte-verified 22-slot     */
/* layout (read directly from 0x477D30..0x477D88, the latter being       */
/* Town's own unrelated vtable and the hard ceiling); see that header's   */
/* class doc comment rather than duplicating it here.                    */
/* ================================================================== */
#define VTBL_GAMEVIEW                  0x00477D30  /* GameView vtable — see core/GameView.h */

/* ================================================================== */
/* PostcardPreviewWindow — Postcard preview/send dialog                 */
/* Derived from UI_WindowBase (vtable 0x477C30). Size: 0x150+ bytes.   */
/* ================================================================== */
#define VTBL_POSTCARD_PREVIEW_WINDOW   0x00477E20  /* PostcardPreviewWindow vtable
    [0]  +0x00: scalar deleting destructor
    [1]  +0x04: Hide (overridden: PostcardPreviewWindow_Hide, 0x430B40)
    [2]  +0x08: Show (overridden: PostcardPreviewWindow_Show, 0x430C40)
    [2]..[11]: inherited from UI_WindowBase (see VTBL_UI_WINDOWBASE)   */

/* ================================================================== */
/* GameVehicle — Vehicle destination management (extends RESDATA_GameObject) */
/* ================================================================== */
#define VTBL_RESDATA_GAMEVEHICLE        0x00478308  /* RESDATA_GameVehicle vtable
    Base class (type=4, 0x11C bytes, extends RESDATA_GameObject).
    [0]  +0x00: scalar deleting destructor (RESDATA_GameVehicle_Dtor, 0x44B030)
    Constructor: RESDATA_GameVehicle_Ctor (0x44AE80)
    Sets vehicle_kind at +0x10C based on tile type byte at RESDATA+0x63a.         */

#define VTBL_GAMEVEHICLE                0x00477848  /* GameVehicle vtable
    Derived from RESDATA_GameVehicle (vtable 0x478308) -> Entity -> GameObject.
    Adds destination queue management (linked list at +0x124). Size: ~0x12C bytes.
    15-slot layout matches Entity; overrides at [0], [7], [10].
    [0]  +0x00: scalar deleting destructor (0x4128B0)           — OVERRIDDEN
    [1]  +0x04: InvalidateRect (0x436AB0)                       — Entity
    [2]  +0x08: PtInRect (0x436A10)                             — Entity
    [3]  +0x0C: MoveTo (0x405C00)                               — Entity
    [4]  +0x10: InvokeCallback1 (0x436AE0)                      — Entity
    [5]  +0x14: InvokeCallback2 (0x436B00)                      — Entity
    [6]  +0x18: InitBase (0x405900)                             — Entity
    [7]  +0x1C: SetOccupantState (0x44B130)                     — RESDATA_GameVehicle
    [8]  +0x20: SetFrame (0x405DE0)                             — Entity
    [9]  +0x24: SetVisible (0x4061B0)                           — Entity
    [10] +0x28: Update (0x412A80)                               — OVERRIDDEN
    [11] +0x2C: Draw (0x4343B0)                                 — RESDATA_GameVehicle
    [12] +0x30: DrawConnected (0x405FD0)                        — Entity
    [13] +0x34: SetName (0x405E20)                              — Entity
    [14] +0x38: SetAnimState (0x405A50)                         — Entity
    Constructor: GameVehicle_Ctor (0x412870), chains to Entity(), then calls
    RESDATA_GameVehicle_Ctor (0x44AE80) for base init, zeros extended fields,
    sets vehicle_kind (+0x10C) to 4.
    StartMoving (0x4129C0) is NON-VIRTUAL — not in vtable.               */

/* ================================================================== */
/* SoundObject — TrackPiece subclass with text label (vtable at 0x478280) */
/* Used for sound-editor/resource-editor UI elements with text labels.  */
/* ================================================================== */
#define VTBL_SOUND_OBJECT              0x00478280  /* SoundObject vtable
    [0]  +0x00: scalar deleting destructor (Ghidra: SoundObject_ScalarDeletingDtor,
                0x448FE0 — thin 30-byte thunk; calls the real base destructor
                body at 0x449000 (Ghidra: SoundObject_Dtor == this codebase's
                SoundObject::~SoundObject, resources/ResourceManager.cpp), then
                conditionally frees the object) */

/* ================================================================== */
/* ScriptEngine / ScriptedObject vtables                                 */
/* ================================================================== */
#define VTBL_SCRIPTENGINE_BASE          0x004782A4  /* ScriptEngine base (RESDATA) vtable
    [0]  +0x00: Cleanup / scalar-deleting dtor (RESDATA_ScriptEngine_Cleanup, 0x4493C0)
    [1]  +0x04: Lock (RESDATA_Lock, 0x449410)
    [2]  +0x08: Unlock (RESDATA_Unlock, 0x449420)                      */
#define VTBL_SCRIPTENGINE_FULL          0x00478378  /* ScriptEngine full vtable
    [0]  +0x00: (scalar deleting destructor)
    [1]  +0x04: Lock
    [2]  +0x08: Unlock
    [3]  +0x0C: Call/OnInitFromStream body destructor (ScriptEngine_Call, 0x44E930) */

/* ScriptedObject vtable at 0x4782A8 (22+ slots).
   Extends Panel -> GameObject. Has at least 23 vtable slots (indices 0-22).
   Slots 0-14 match GameObject base; slots 15-22 are ScriptedObject-specific. */
#define VTBL_SCRIPTED_OBJECT            0x004782A8  /* ScriptedObject vtable
    [0]  +0x00: scalar deleting destructor (RESDATA_ScriptedObject_Dtor,       0x4494C0)
    [1]  +0x04: UpdateChild/InvalidateRect  (RESDATA_UpdateChild,             0x454890)
    [2]  +0x08: IsDragging / PtInRect       (RESDATA_ScriptedObject_IsDragging,0x449CE0)
    [3]  +0x0C: MoveTo                       (RESDATA_ScriptedObject_MoveTo,   0x449DC0)
    [4]  +0x10: HitTest                      (RESDATA_ScriptedObject_HitTest,  0x44A0C0)
    [5]  +0x14: (unknown)                    (0x454A60)
    [6]  +0x18: Init/InitBase                (Panel_Init,                      0x454680)
    [7]  +0x1C: StopSound                    (GameObject_StopSound,            0x405A20)
    [8]  +0x20: SetPause/callback            (CGWND_SetPause,                  0x4061B0)
    [9]  +0x24: Update callback              (RESDATA_ScriptedObject_Update,   0x4497A0)
    [10] +0x28: Draw (override)              (Panel::Draw,                     0x454900)
    [11] +0x2C: DrawConnected                (GameObject_DrawConnected,        0x405FD0)
    [12] +0x30: SetName                      (GameObject_SetName,              0x405E20)
    [13] +0x34: SetAnimState                 (GameObject_SetAnimState,         0x405A50)
    [14] +0x38: Shutdown/cleanup             (RESDATA_ScriptedObject_Shutdown, 0x4495B0)
    [15] +0x3C: InitState                    (RESDATA_GameVehicle_InitState,   0x44ADF0)
    [16] +0x40: HandleToolClick              (RESDATA_ScriptedObject_HandleToolClick, 0x44A250)
    [17] +0x44: (unknown, shared by [18])    (0x44EF00)
    [18] +0x48: (unknown, shared by [17])    (0x44EF00)
    [19] +0x4C: UpdateToolState              (RESDATA_ScriptedObject_UpdateToolState, 0x44AC20)
    [20] +0x50: GetDragOffset                (RESDATA_ScriptedObject_GetDragOffset,   0x449D80)
    [21] +0x54: CheckClick                   (RESDATA_ScriptedObject_CheckClick,      0x449D00)
    [22] +0x58: (unknown)                                                                   */

#define VTBL_SCRIPTED_OBJECT_CHILD      0x00478358  /* Child ScriptedObject vtable
    [0]  +0x00: scalar deleting destructor (RESDATA_ScriptedObject_DtorChain, 0x44B200) */

/* ================================================================== */
/* Vehicle — Road vehicle class (0x94-byte standalone, ScriptedObject-like) */
/* ================================================================== */
#define VTBL_VEHICLE                    0x0047836C  /* Vehicle vtable
    Road vehicle class (cars, trucks, buses). Standalone 0x94-byte object
    with its own struct layout (NOT derived from GameObject/Entity).
    Vtable layout is identical to VTBL_SCRIPTED_OBJECT but with different
    concrete function pointers:
    [0]  +0x00: scalar deleting destructor (RESDATA_ScriptedObject_DtorList, 0x44C0B0)
    [1]  +0x04: cleanup (inherited layout)
    [2]  +0x08: PtInRect / CheckClick
    [3]  +0x0C: MoveTo / HitTest dispatch
    [4]  +0x10: method_4
    [5]  +0x14: method_5
    [6]  +0x18: InitBase
    [7]  +0x1C: SetAnimState (editor vtable dispatch: 0=forward, 1=reverse)
    [8]  +0x20: SetFrame (editor vtable dispatch: (anim_data, frame))
    [9]  +0x24: SetName
    [10] +0x28: Draw / Update
    [11] +0x2C: DrawConnected / Dispatch
    [12] +0x30: OnTimerTick
    [13] +0x34: method_13
    [14] +0x38: AnimStateSelect
    Constructor: Vehicle_Ctor (0x44BE50)
    Creates EditorState (0x20) and VehicleEditor (0x450) sub-objects.
    Sub-objects: up to 4 VehicleEditor at +0x10, EditorState at +0x20.   */

/* ================================================================== */
/* Resource manager / data structures                                   */
/* ================================================================== */
#define VTBL_RESDATA                   0x00478274  /* RESDATA vtable              */
#define VTBL_RESOURCE_ENTRY             0x00478278  /* ResourceEntry vtable        */

/* ================================================================== */
/* UI sprite / widget classes                                           */
/* ================================================================== */
#define VTBL_BUTTONSPRITE              0x0047851C  /* ButtonSprite vtable         */

/* ================================================================== */
/* TileMap Manager — TileMap wrapper/manager class                      */
/* Set by Sprite_Create (0x454CF0). Operates on TileMap struct data.    */
/* Despite "Sprite_" names, these are TileMap management functions.     */
/* ================================================================== */
#define VTBL_TILEMAP_MANAGER           0x00478520  /* TileMap manager vtable
    [0]  +0x00: (scalar deleting destructor)
    Used by Sprite_Create/Shutdown/LockAll/UnlockAll functions which
    operate on the TileMap global struct despite "Sprite" naming.       */

/* ================================================================== */
/* ChildWindow — Root base class (no parent) for resource-loading child */
/* windows. Confirmed real single inheritance (not a field-layout        */
/* convention — see ui/UI_ChildWindow.h's evidence trail) via vtable-    */
/* patch-after-base-ctor disassembly, matching object sizes, and         */
/* standalone (unpatched) instantiation at 9 sites in                    */
/* ResourceManager::AddString.                                           */
/* ================================================================== */
#define VTBL_CHILD_WINDOW              0x00477C18  /* ChildWindow vtable, 6 slots (24 bytes)
    [0] +0x00: ~ChildWindow (scalar-deleting-dtor thunk 0x424B40; real body 0x424BA0)
    [1] +0x04: OnMouseMove   (0x425670, Ghidra label "UI_PaintWindow")
    [2] +0x08: OnMouseLeave  (0x4257F0)
    [3] +0x0C: Render        (0x424E00 — TODO: decompile, deferred; blocked on
                               unresolved WNDPROC_Stream* signatures)
    [4] +0x10: constructor init body (0x424BF0, Ghidra label "UI_ChildWindow_Create")
               — NOT a runtime-dispatched slot; folded into ChildWindow's own
               constructor. Reachable via vtable dispatch during base
               construction only (a virtual Render call inside it resolves to
               the base's own Render at that point, since the derived vtable
               isn't installed yet — normal C++ semantics).
    [5] +0x14: reserved/NULL (confirmed empty in this table and in all three
               known derived tables below)
    IsBitmapReady (0x4255F0) is NOT in this vtable — non-virtual member.     */

/* ================================================================== */
/* CursorEditWindow : public ChildWindow — cursor .dat/.bmp loader       */
/* ================================================================== */
#define VTBL_CURSOREDITWINDOW          0x00477610  /* CursorEditWindow vtable, size 0x7AC
    [0] +0x00: ~CursorEditWindow (own scalar-deleting-dtor, 0x40E660)
    [1] +0x04: OnMouseMove   (0x425670 — INHERITED verbatim from ChildWindow)
    [2] +0x08: OnMouseLeave  (0x4257F0 — INHERITED verbatim from ChildWindow)
    [3] +0x0C: Render        (0x40E8D0 — own override, decompiled)
    [4] +0x10: init body     (0x40E690, "CursorEditWindow__init")
    [5] +0x14: reserved/NULL                                                */

/* ================================================================== */
/* TrainStation : public ChildWindow — city-view station interaction obj */
/* ================================================================== */
#define VTBL_TRAIN_STATION              0x00478118  /* TrainStation vtable, size 0x178
    [0] +0x00: ~TrainStation (own scalar-deleting-dtor, 0x436460; body 0x436480)
    [1] +0x04: OnMouseMove   (0x436960 — own override, plays hover sound then
                               chains to ChildWindow::OnMouseMove)
    [2] +0x08: OnMouseLeave  (0x4369A0 — own override, releases hover sound then
                               chains to ChildWindow::OnMouseLeave)
    [3] +0x0C: Render        (0x436750 — own override, decompiled; parses
                               "walk_speed"/"Employable"/"sex"/"groundwidth"/
                               "SpawnLimit"/"PickUpSoundId" directives)
    [4] +0x10: Init body     (0x436490, "TrainStation_Init")
    [5] +0x14: reserved/NULL
    Do not confuse with VTBL_TRAIN_STATION_WINDOW (0x478130) above — that is
    a different, GameWindow-derived popup class showing animated train car
    sprites, unrelated to this ChildWindow-derived view-level object.       */

/* ================================================================== */
/* BuildingDescriptorEditor : public ChildWindow — building/tile         */
/* placement .dat descriptor loader (see input/BuildingDescriptorEditor.h */
/* for the full name-evidence trail — no original symbol names it).      */
/* ================================================================== */
#define VTBL_BUILDING_DESCRIPTOR_EDITOR 0x004779E0  /* BuildingDescriptorEditor vtable, size 0x630
    [0] +0x00: ~BuildingDescriptorEditor (own scalar-deleting-dtor, 0x41E600,
                               Ghidra label "INPUT_DtorWrapper")
    [1] +0x04: OnMouseMove   (0x425670 — INHERITED verbatim from ChildWindow,
                               confirmed via direct vtable-slot check)
    [2] +0x08: OnMouseLeave  (0x4257F0 — INHERITED verbatim from ChildWindow)
    [3] +0x0C: Render        (0x41E9F0 — own override, Ghidra label
                               "INPUT_EditWndProc"; a cascading .dat-directive
                               keyword parser, substantially decompiled)
    [4] +0x10: handle_edit_message (0x41E6E0, Ghidra label
                               "INPUT_HandleEditMessage" — occupies the same
                               slot position as ChildWindow's own "ctor init
                               body" slot; modeled as a non-virtual member in
                               input/BuildingDescriptorEditor.h per that
                               header's own precedent, not a real declared
                               C++ virtual). CORRECTED 2026-08-16: this entry
                               previously read "0x41E570 — ctor body", which
                               was wrong on two counts — 0x41E570 is BDE's own
                               *constructor* address (never itself a vtable
                               slot value), and raw `read_bytes` at
                               0x4779E0+0x10 shows 0x0041E6E0, not 0x41E570.
    Table ends here — 5 slots total (0x14 bytes), matching ChildWindow's own
    5-slot layout, NOT 6. CORRECTED 2026-08-16: this entry previously listed
    a spurious "[5] +0x14: reserved/NULL" slot. Direct `read_bytes` at
    0x4779E0+0x14 shows 0x0041F4B0 (non-null!) — but `get_xrefs_to` on that
    address (0x4779F4) shows it is independently installed as a vtable
    pointer by 3 *different* functions (INPUT_EditKillFocus 0x41F48E,
    InputEventList::Ctor 0x41F4D0, and one more at 0x41F4B9) — i.e. it is
    the START of `InputEventList`'s own, entirely unrelated vtable, placed
    immediately adjacent in .rdata, not BDE's own slot [5]. Found while
    verifying input/TrackTileDescriptor.h's vtable-boundary evidence (see
    VTBL_TRACK_TILE_DESCRIPTOR below) — the same adjacent-table-in-.rdata
    pattern that class's own vtable (0x478358) exhibits at ITS boundary. */

/* ================================================================== */
/* TrackTileDescriptor : public BuildingDescriptorEditor — scripted-      */
/* object child descriptor with track-tile classification (see           */
/* input/TrackTileDescriptor.h for the full evidence trail — no original  */
/* symbol names it; "RESDATA_ScriptedObject_AddChild" is a Ghidra          */
/* misnomer, since ECX is the child object, not a ScriptedObject).        */
/* ================================================================== */
#define VTBL_TRACK_TILE_DESCRIPTOR      0x00478358  /* TrackTileDescriptor vtable, size 0x63C
    [0] +0x00: ~TrackTileDescriptor (own scalar-deleting-dtor, "RESDATA_
                               ScriptedObject_DtorChain" 0x44B200; body
                               "RESDATA_ScriptedObject_RemoveChild" 0x44B220,
                               chains to BuildingDescriptorEditor's own
                               destructor body 0x41E620 directly)
    [1] +0x04: OnMouseMove   (0x425670 — INHERITED verbatim from ChildWindow,
                               confirmed via direct vtable-slot check)
    [2] +0x08: OnMouseLeave  (0x4257F0 — INHERITED verbatim from ChildWindow)
    [3] +0x0C: Render        (0x44B4F0, "RESDATA_ScriptedObject_
                               ClassifyTileType" — own override, classifies
                               a tunnel/depot/bridge/points/switch/
                               crosstrack/levelcrossing/station keyword
                               section into tile_type, +0x63A)
    [4] +0x10: HandleEvent   (0x44B290, "RESDATA_ScriptedObject_HandleEvent"
                               — occupies the same slot position as
                               BuildingDescriptorEditor's own
                               handle_edit_message; modeled non-virtual per
                               that established precedent)
    Table ends here — 5 slots total (0x14 bytes), same as ChildWindow/
    BuildingDescriptorEditor. Confirmed via `get_xrefs_to` on every
    dword-aligned offset from 0x478358 up to 0x478378: the next address
    anything installs as a vtable pointer is 0x47836C, exactly 5 slots
    later — a wholly different, unidentified 3rd class (installed by
    RESDATA_ScriptedObject_CleanupChildren/Vehicle_Ctor, receiver has small
    offsets +0x10..+0x20/+0x7A/+0x88, matching neither this class nor
    ScriptedObject's own VTBL_SCRIPTED_OBJECT above) — deliberately NOT
    modeled here, out of scope for this class. Immediately after that:
    CollisionData's own vtable at 0x478370, then ScriptEngine's own at
    0x478378 — all adjacent, unrelated tables in the same .rdata region. */

/* ================================================================== */
/* UI Manager — UI component manager singleton                           */
/* ================================================================== */
#define VTBL_UI_MANAGER                 0x00477AD0  /* UI Manager vtable           */

/* ================================================================== */
/* Timer list variants — used for timed events and callbacks            */
/* ================================================================== */
#define VTBL_TIMERLIST_A                0x00477BD0  /* TimerList variant A        */
#define VTBL_TIMERLIST_B                0x00477B78  /* TimerList variant B        */
#define VTBL_TIMERLIST_C                0x00477B40  /* TimerList variant C        */
#define VTBL_TIMERLIST_WRAPPER          0x00477AE8  /* TimerList wrapper          */
#define VTBL_TIMERLIST_WRAPPER2         0x00477AEC  /* TimerList wrapper 2        */
/* Byte-dumped and decompiled in full this session (ui/UI_Utils.h/.cpp's
 * UITimerList): VTBL_TIMERLIST_A/VTBL_TIMERLIST_C are transient
 * construction-time vtables (UI_Manager's ctor, 0x4238C0, sets each
 * TimerList's vtable pointer to A/C first, then B/WRAPPER once fully
 * constructed — A/C are never the *live* vtable for any TimerList that
 * survives construction). VTBL_TIMERLIST_B (text_list's real vtable) and
 * VTBL_TIMERLIST_WRAPPER (pos_list's and update_list's real vtable) are
 * both 22 slots (0x58 bytes) and share identical function pointers at
 * every slot this class's own methods use: slot0=Resize (0x435D10),
 * slot3=RemoveAndGet (0x4241E0), slot4=RemoveAt (0x4356E0),
 * slot6=RemoveAll (0x424270), slot7=GetItemRaw (0x424530),
 * slot8=GetItem (0x424030), slot10=SetAt (0x424790),
 * slot11=GetCount (0x424000), slot12=HasLiveSlot (0x424760),
 * slot13=Add (0x4362B0), slot17=InsertAt (0x4248C0),
 * slot18=Compare (0x424960, keyed-insert only — not reconstructed).
 * This is the SAME collection-vtable layout other classes reuse for
 * their own unrelated collections at some of the same slot addresses
 * (e.g. game/BuildingMgr.cpp's Building collection: slot3=RemoveElement
 * at this same 0x4241E0) — a shared template instantiation family, not
 * something specific to UI_Manager. */

/* ================================================================== */
/* TimerSlotList — Timer slot list (Collection layout, 16 bytes)       */
/* Dead marker vtable used during destruction:                          */
/*   [0] +0x00: Timer_Resize (minimal resize-only slot)                */
/*   [1] +0x04: TimerSlotList_ScalarDeletingDtor_Dead (0x412580)      */
/* ================================================================== */
#define VTBL_TIMERSLOT_DEAD_MARKER      0x00477798  /* TimerSlotList dead marker vtable
    [0] +0x00: Timer_Resize
    [1] +0x04: TimerSlotList_ScalarDeletingDtor_Dead (0x412580)         */

/* VTBL at 0x477790 — vtable[14] (FindIndex) slice used by Timer slots */
/* Entry: Game_CheckIdleTimeout (0x412540) — linear search in items array */

/* ================================================================== */
/* Vehicle Editor — Track/vehicle editing UI                            */
/* ================================================================== */
#define VTBL_UIENTITY                  0x00477A90  /* UIEntity vtable */

#define VTBL_VEHICLE_EDITOR             0x00477590  /* VehicleEditor vtable
    [0]  +0x00: scalar deleting destructor (VehicleEditor_Dtor, 0x43C9C0)
    [1]  +0x04: (inherited from GameObject)
    [2]  +0x08: (inherited)
    [3]  +0x0C: HitTest dispatch (overridden: VehicleEditor_HitTest)
    [4]  +0x10: (overridden)
    [5]  +0x14: (inherited)
    [6]  +0x18: Init (overridden: VehicleEditor_Init)
    [7]  +0x1C: SetAnimState
    [8]  +0x20: SetFrame (overridden)
    [9]  +0x24: SetName
    [10] +0x28: Draw (VehicleEditor_Draw, 0x43CFB0)
    [11] +0x2C: DrawConnected
    [12] +0x30: OnTimerTick
    [13] +0x34: (overridden)
    [14] +0x38: AnimStateSelect/VehicleCtor (VehicleEditor_VehicleCtor, 0x43CAB0) */

/* ================================================================== */
/* EditorState — Per-endpoint track editor state machine               */
/* Size: 0x20 bytes. Vtable: 0x477564                                  */
/* ================================================================== */
#define VTBL_EDITORSTATE               0x00477564  /* EditorState vtable
    [0] +0x00: scalar deleting destructor (EditorState::~EditorState, 0x40B550)  */

/* ================================================================== */
/* TrackPiece — Individual rail track piece (derives from GameObject)   */
/* ================================================================== */
#define VTBL_TRACK_PIECE                0x00477568  /* TrackPiece vtable
    Derives directly from GameObject (NOT from Entity — type=7).
    GameObject base vtable at 0x477820 has 4 slots (0-3).
    TrackPiece adds slots 4-8.
    [0]  +0x00: scalar deleting destructor        (TrackPiece_ScrDtor, 0x40D020)
    [1]  +0x04: InvalidateRect                    (inherited: 0x436AB0)
    [2]  +0x08: PtInRect                          (inherited: 0x436A10)
    [3]  +0x0C: HitTest dispatch                  (TrackPiece_HitTest, 0x43E9A0)
    [4]  +0x10: (unknown — TrackPiece-specific)
    [5]  +0x14: (unknown — TrackPiece-specific)
    [6]  +0x18: SetFrame                          (TrackPiece_SetFrame, 0x40D2A0)
    [7]  +0x1C: UpdateAnim                        (TrackPiece_UpdateAnim, 0x40D2F0)
    [8]  +0x20: Render                            (TrackPiece_Render, 0x40D340)
    Note: Init() at 0x40D0B0 is NOT a virtual method — it's called
    directly from the constructor only.                                 */

/* ================================================================== */
/* DDraw building sprite resource                                       */
/* ================================================================== */
#define VTBL_DDRAW_BUILDING_SPRITE      0x00478548

/* ================================================================== */
/* Win32 Thread wrapper                                                 */
/* ================================================================== */
#define VTBL_WIN32_THREAD               0x00479168

/* ================================================================== */
/* WNDPROC_Stream / WIN32_Stream family — see resources/StreamObject.h, */
/* WndProcStream.h, Win32Stream.h, WndProcOStream.h, Win32OStream.h.    */
/*                                                                       */
/* Each entry below is a 2-slot MSVC vbtable ([0]=self-offset (always   */
/* 0), [1]=byte offset from `this` to the StreamObject virtual base),   */
/* NOT a method-pointer vtable — confirmed via read_bytes. The separate */
/* "_VIEW" entries ARE real (1-entry, scalar-deleting-destructor)       */
/* method vtables, used when the object is accessed through its        */
/* StreamObject-view adjustor (this+vbase offset above).                */
/* ================================================================== */
#define VTBL_WNDPROC_STREAM             0x00479238  /* WNDPROC_Stream's own vbtable
    [0]=0, [1]=0xC (own data: _reserved_04@+4, gcount_@+8, before StreamObject).
    Poked into `*this` only when WNDPROC_Stream::AttachBuffer (0x464840) runs
    with initBase set (i.e. constructed as the most-derived object) — never
    observed in this codebase's evidence, since every real caller constructs
    the derived WIN32_Stream instead. */
#define VTBL_WNDPROC_STREAM_VIEW        0x00479234  /* WNDPROC_Stream-as-StreamObject-view
    vtable (1 slot: scalar deleting destructor, target 0x464810 ==
    WNDPROC_Stream_ScalarDtor — confirmed via read_bytes, NOT
    WNDPROC_Stream_DtorVftableReset itself, which that scalar dtor calls
    internally). Poked into the StreamObject subobject by
    WNDPROC_Stream::AttachBuffer's initBase branch. */
#define VTBL_STREAMOBJECT_ALONE         0x0047922C  /* "PTR_WNDPROC_StreamDtor" —
    StreamObject's OWN bare identity vtable (poked in by
    WNDPROC_StreamCleanup, 0x464620, StreamObject::~StreamObject()'s real
    address, as its very first instruction: `*param_1 =
    &PTR_WNDPROC_StreamDtor_0047922c;`). This is the terminal step of the
    same re-tagging chain as VTBL_WIN32_STREAM_VIEW/VTBL_WNDPROC_STREAM_VIEW
    below — WIN32_StreamDestroy pokes VTBL_WIN32_STREAM_VIEW, then
    WNDPROC_Stream_DtorVftableReset pokes VTBL_WNDPROC_STREAM_VIEW, then
    WNDPROC_StreamCleanup pokes this one, immediately before doing its own
    real cleanup (free owned rdbuf, tear down locks/refcount) — the point
    at which the object's identity has unwound all the way down to "bare
    StreamObject, nothing more derived." Real C++ virtual-base destruction
    provides the equivalent identity-narrowing automatically; only the
    real cleanup body is reproduced, as StreamObject::~StreamObject()
    (resources/StreamObject.h/.cpp). */
#define VTBL_WIN32_STREAM               0x00479188  /* WIN32_Stream's own vbtable
    [0]=0, [1]=0xC (WIN32_Stream adds no own fields beyond WNDPROC_Stream,
    same vbase offset). Poked into `*this` by WIN32_StreamOpen/OpenFile
    (0x463890/0x463970) on construction. */
#define VTBL_WIN32_STREAM_VIEW          0x00479184  /* WIN32_Stream-as-StreamObject-view
    vtable (1 slot: scalar deleting destructor, target 0x463940 ==
    WIN32_Stream_ScalarDtor, chains to WIN32_StreamDestroy 0x463A80 then
    WNDPROC_Stream_DtorVftableReset 0x4648E0). Poked in by WIN32_StreamOpen/
    OpenFile after WNDPROC_Stream::AttachBuffer runs. WIN32_StreamDestroy/
    WNDPROC_Stream_DtorVftableReset are both pure MSVC vptr-retagging
    bookkeeping (poke a constant into this exact vbtable-relative slot,
    nothing else) — documented, not reimplemented, exactly like
    VTBL_WNDPROC_OSTREAM_VIEW's/VTBL_WIN32_OSTREAM_VIEW's equivalent pair
    below; see resources/Win32Stream.h's doc comment on 0x463A80 for the
    full evidence trail (real C++ virtual-base destruction provides the
    same guarantee for free). */

#define VTBL_WIN32_MEMORYSTREAM         0x00479210  /* WIN32_MemoryStream's own vbtable
    [0]=0, [1]=0xC (WIN32_MemoryStream adds no own fields beyond WNDPROC_Stream,
    same vbase offset as WIN32_Stream/plain WNDPROC_Stream). Poked into `*this`
    by WNDPROC_StreamFromMemory (0x464490) on construction. Confirmed a
    genuinely distinct concrete class from WIN32_Stream (not the same class
    constructed at a different most-derived level): its own scalar-deleting-
    destructor address (0x464460) differs from both WIN32_Stream's (0x463940)
    and plain WNDPROC_Stream's (0x464810). rdbuf is a heap WIN32_StreamMem
    (see resources/Win32StreamMem.h) instead of WIN32_Stream's WIN32_StreamFile. */
#define VTBL_WIN32_MEMORYSTREAM_VIEW    0x0047920C  /* WIN32_MemoryStream-as-StreamObject-
    view vtable (1 slot: scalar deleting destructor, target 0x464460 ==
    WIN32_MemoryStream_ScalarDtor, chains to WIN32_MemoryStream_DtorVftableReset
    0x464550 [Ghidra auto-name "WNDPROC_StreamSeek" was misleading -- it does
    not seek] then WNDPROC_Stream_DtorVftableReset 0x4648E0). Poked in by
    WNDPROC_StreamFromMemory after WNDPROC_Stream::AttachBuffer runs. Both
    retagging functions are pure MSVC vptr-retagging bookkeeping, documented
    not reimplemented, exactly like VTBL_WIN32_STREAM_VIEW's pair above. */

#define VTBL_WNDPROC_OSTREAM            0x00479288  /* WNDPROC_OStream's own vbtable
    [0]=0, [1]=8 (own data: _reserved_04@+4 ONLY — no gcount_-equivalent,
    write-only facade, one dword smaller than VTBL_WNDPROC_STREAM above).
    Poked into `*this` by WNDPROC_OStream::AttachBuffer (0x465A30) when
    initBase is set; never observed constructed bare in this codebase. */
#define VTBL_WNDPROC_OSTREAM_VIEW       0x00479284  /* WNDPROC_OStream-as-StreamObject-
    view vtable (1 slot: scalar deleting destructor, target 0x465A00 ==
    WNDPROC_OStream_ScalarDtor — confirmed via read_bytes, NOT
    WNDPROC_OStream_DtorVftableReset itself, which that scalar dtor calls
    internally). Poked into the StreamObject subobject by
    WNDPROC_OStream::AttachBuffer's initBase branch. */
#define VTBL_WIN32_OSTREAM              0x00479248  /* WIN32_OStream's own vbtable
    [0]=0, [1]=8 (WIN32_OStream adds no own fields beyond WNDPROC_OStream,
    same vbase offset; 4 bytes smaller than VTBL_WIN32_STREAM above —
    confirmed via this class's own allocation: operator_new(0x58) at its
    sole call site, RESMGR_LoadResourceData 0x447E63, vs. WIN32_Stream's
    documented 0x5C). Poked into `*this` by WIN32_StreamOpenWriteFile
    (0x465090, formerly Ghidra-mislabeled "CRT_floor") on construction. */
#define VTBL_WIN32_OSTREAM_VIEW         0x00479244  /* WIN32_OStream-as-StreamObject-view
    vtable (1 slot: scalar deleting destructor, target 0x465060 ==
    WIN32_OStream_ScalarDtor, chains to WIN32_OStream_DtorVftableReset
    0x465180 then WNDPROC_OStream_DtorVftableReset 0x465AC0). Poked in by
    WIN32_StreamOpenWriteFile after WNDPROC_OStream::AttachBuffer runs. */

/* ================================================================== */
/* Unidentified vtables — addresses only, class TBD                     */
/* ================================================================== */
#define VTBL_0047703C                  0x0047703C
#define VTBL_00477044                  0x00477044
#define VTBL_00477048                  0x00477048
#define VTBL_00477058                  0x00477058
#define VTBL_00477060                  0x00477060
#define VTBL_00477064                  0x00477064
#define VTBL_00477070                  0x00477070
#define VTBL_00477078                  0x00477078
#define VTBL_004770A0                  0x004770A0
#define VTBL_004770C4                  0x004770C4
#define VTBL_0047726C                  0x0047726C
