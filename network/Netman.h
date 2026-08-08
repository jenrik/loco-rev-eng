/**
 * Netman.h — High-level network session manager (Netman class)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Netman is the high-level network manager singleton for multiplayer
 * sessions. It manages 9 player slots (each with dpId, layout name, player
 * identity, pixel overlay cache, and a per-slot message queue), the
 * game-mode state machine (waiting/hosting/joined/error), and serialization
 * of map/building overlay pixel data for network sync. It builds on
 * DPlayManager for the actual DirectPlay transport and uses the global
 * _g_train object as the message dispatch target.
 *
 * Size: ~0x804 bytes
 * Vtable: 0x4781C8 (single entry: scalar deleting destructor)
 *
 * Class hierarchy:
 *   (standalone — no base class)
 *
 * Vtable layout:
 *   [0] +0x00: scalar deleting destructor (compiler-generated, 0x43D110)
 *
 * === Message types dispatched by ProcessMessage (0x43F2B0) ===
 *   type 2:  SYNC_GAME_STATE    — receive and sync remote game state data
 *   type 3:  HOST_SESSION_START — begin hosting session
 *   type 4:  LAYOUT_SELECT      — assign/reassign player layout slots
 *   type 5:  NET_RESET          — reset and reinitialize network state
 *   type 9:  GAME_STATE_SYNC    — sync game state (client variant)
 *   type 0xB: REMOVE_INBOUND    — remove player and their inbound trains
 *   type 0xC: PLAYER_JOIN       — new player joined session
 *   type 0xF: INBOUND_APPEND    — append inbound train node to list
 *   type 0x11: GAME_START_HOST  — host-side game start with train spawning
 *   type 0x12/0x15/0x17: FILE_TRANSFER — protocol messages
 *   type 0x13: FLAG_SET         — set per-slot flag
 *   type 0x14: FLAG_CLEAR       — clear per-slot flag
 *   type 0x16: PIXEL_DATA       — receive pixel overlay data for a slot
 *   type 0x18: TRAIN_POSITION   — acknowledge train position update
 *   type 0x1A: PLAYER_LEAVE     — player left session
 *   type 0x1B: REFRESH_REQUEST  — request building overlay refresh
 *   type 0x1C: TIMEOUT_CHECK    — trigger timeout check
 *
 * InboundTrainNode and PingEntry (per-ping tracking node in transfer_list)
 * field layouts now live in network/NetmanTypes.h, alongside PlayerSlot,
 * CarObject/SpriteObject, and the `class Netman` declaration itself.
 *
 * Status: TRANSCRIBED
 */

#pragma once

/* PlayerSlot / PingEntry / InboundTrainNode / CarObject / SpriteObject /
 * class Netman itself now live in NetmanTypes.h — split out so TUs that
 * only need the complete types for named-field access don't pull in this
 * header's extern "C" Win32 block and mislinked free-function decls (see
 * NetmanTypes.h's header comment for why that split exists). */
#include "NetmanTypes.h"

/* ================================================================== */
/* Standalone network helper functions (C++ linkage)                    */
/* ================================================================== */

/** Return the current mode from an optional Netman instance. */
int32_t NETMAN_GetGameMode(const void* netman);

/**
 * NETMAN_SendDisconnect — Build and queue a DISCONNECT packet.
 * Address: 0x43D250
 */
void NETMAN_SendDisconnect(int32_t dpId);

/**
 * NETMAN_QueueMessage — Queue or free a network message.
 * Address: 0x43F140
 */
void NETMAN_QueueMessage(TrainMessage* msg);

/**
 * NETMAN_StartHostSession — Queue HOST_SESSION_START (type 3) message.
 * Address: 0x43F000
 */
void NETMAN_StartHostSession();

/**
 * NETMAN_StartClientSession — Queue CLIENT_SESSION_START (type 1) message.
 * Address: 0x43F030
 */
void NETMAN_StartClientSession();

/**
 * NETMAN_ReceiveLayoutSelect — Serialize all 9 slots into packet and send.
 * Address: 0x440070
 *
 * Despite "Receive" name, this creates a 0x228-byte packet with all
 * player slots compacted via DPLAY_FreePlayerSlot and queues it.
 *
 * @param netman  Netman instance pointer
 */
void NETMAN_ReceiveLayoutSelect(Netman* netman);

/**
 * NETMAN_ReceiveFileTransfer — Mark receiving state, send type 0x3F4 packet.
 * Address: 0x440310
 */
void NETMAN_ReceiveFileTransfer(Netman* netman);

/**
 * NETMAN_SendAck — Mark idle state, send type 0x3F5 packet.
 * Address: 0x440390
 */
void NETMAN_SendAck(Netman* netman);

/**
 * NETMAN_SendTrainPosition — Queue type 0xE signal_change message.
 * Address: 0x43EE80
 *
 * @param vehicle  Vehicle pointer
 * @return         1
 */
bool NETMAN_SendTrainPosition(InboundTrainNode* vehicle);

/**
 * NETMAN_ReceiveTrainPosition — Process received position, compute route.
 * Address: 0x43EEC0
 *
 * Computes track route from source/dest params, validates connectivity,
 * queues TRAIN_POSITION message (type 0x10).
 */
bool NETMAN_ReceiveTrainPosition(int32_t position_x, int32_t position_y,
                                 InboundTrainNode* vehicle);

/**
 * NETMAN_ReceiveSignalChange — Resolve remote player address from PostBag files.
 * Address: 0x43E900
 *
 * Enumerates DPLAY players, matches player name against param+0x10,
 * reads route/address PostBag files, picks random route entry, resolves
 * address via NET_ResolveAddress, copies data into resolved DPlayData.
 *
 * @param playerDPlayData  Source DPlayData with player name to resolve
 * @return                 Resolved DPlayData pointer, or NULL on failure
 */
void* NETMAN_ReceiveSignalChange(void* playerDPlayData);

#ifndef _WIN32
/**
 * NETMAN_HostLocalSlotIndex — m_mySlotIndex accessor for translation units
 * (game/Train_network.cpp) that only need this one bit through the global
 * Netman singleton and can't include this header directly.
 * Defined in network/Netman.cpp.
 */
int32_t NETMAN_HostLocalSlotIndex();
#endif

/**
 * NETMAN_CheckTimeout — call-site adapter for translation units that hold
 * g_netman as void* and cannot include Netman.h (town/Town.cpp). Forwards
 * to Netman::CheckTimeout (0x440820). Defined in network/Netman.cpp.
 */
void NETMAN_CheckTimeout(void* netman, int32_t timeoutVal);

/* ================================================================== */
/* Win32 API imports (C linkage only)                                   */
/* ================================================================== */

extern "C" {
    /* Win32 API */
    int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);
    void*   __stdcall GetProcessHeap(void);
    int32_t __stdcall HeapFree(void* hHeap, uint32_t dwFlags, void* lpMem);
    int32_t __stdcall IsWindowVisible(void* hWnd);
    void*   __stdcall CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess,
                                   uint32_t dwShareMode, void* lpSecurityAttributes,
                                   uint32_t dwCreationDisposition,
                                   uint32_t dwFlagsAndAttributes, void* hTemplateFile);
    int32_t __stdcall ReadFile(void* hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead,
                                uint32_t* lpNumberOfBytesRead, void* lpOverlapped);
    int32_t __stdcall WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
                                 uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
    int32_t __stdcall CloseHandle(void* hObject);
    int32_t __stdcall CopyRect(RECT* lprcDst, const RECT* lprcSrc);
    int32_t __stdcall OffsetRect(RECT* lprc, int32_t dx, int32_t dy);
    int32_t __stdcall SetWindowTextA(void* hWnd, const char* lpString);
    int32_t __stdcall GetWindowTextA(void* hWnd, char* lpString, int32_t nMaxCount);
    int32_t __stdcall PostMessageA(void* hWnd, uint32_t Msg, uint32_t wParam, uint32_t lParam);
    void*   __stdcall SetTimer(void* hWnd, uint32_t nIDEvent, uint32_t uElapse, void* lpTimerFunc);
    int32_t __stdcall KillTimer(void* hWnd, uint32_t nIDEvent);
    void    __stdcall SetFocus(void* hWnd);
    void    __stdcall ShowWindow(void* hWnd, int32_t nCmdShow);
    void*   __stdcall SetWindowLongA(void* hWnd, int32_t nIndex, void* dwNewLong);
    void*   __stdcall CreateWindowExA(uint32_t dwExStyle, const char* lpClassName,
                                       const char* lpWindowName, uint32_t dwStyle,
                                       int32_t x, int32_t y, int32_t nWidth, int32_t nHeight,
                                       void* hWndParent, void* hMenu,
                                       void* hInstance, void* lpParam);
    void    __stdcall MessageBeep(uint32_t uType);
    int32_t __stdcall MessageBoxA(void* hWnd, const char* lpText,
                                   const char* lpCaption, uint32_t uType);
    void    __stdcall OutputDebugStringA(const char* lpOutputString);
    /* void (matches tilemap.h's extern void Sleep(uint32_t) — InputMgr.cpp
     * includes both headers, and the Win32 Sleep return value is never
     * used; tilemap.cpp calls it without checking a result). */
    void    __stdcall Sleep(uint32_t dwMilliseconds);
}

/* ================================================================== */
/* C++ extern declarations for globals and game engine functions       */
/* (NOT inside extern "C" — these have C++ linkage)                    */
/* ================================================================== */

/* -- Globals -- */
class TileMap;   /* forward decl for g_tilemap below (tilemap.h) */
struct UIPANEL_Surface;   /* forward decl (graphics/LOCOBITMAP.h) */
extern int32_t  g_game_mode;          /* 0x4851F4 — global game mode   */
extern char     g_install_path[];     /* 0x4A99C8 — installation path  */
extern int32_t  g_player_id;          /* 0x4AAD46 — global player ID   */
extern int32_t  g_player_color;       /* 0x4AAD48 — host-declared 32-bit for
                                          *   uniformity (16-bit storage + 16-bit
                                          *   loads in the binary) */
extern TileMap* g_tilemap;            /* 0x4AAD08 — tilemap object     */
extern void*    g_asset_mgr;          /* 0x485600 — asset manager ptr  */
extern void*    g_net_host_info;      /* 0x4FD3A8 — net host info struct */
extern void*    g_ui_main;            /* 0x4A8860 — main UI window ptr */
extern void*    g_listener_x;         /* 0x486BBC — listener position x */
extern void*    g_listener_y;         /* 0x486BC0 — listener position y */
extern void*    _g_dplay;             /* 0x4FD3A8 — DPLAY/NetworkPlayerList instance */
extern void*    _g_dplay_config;      /* 0x4FD3AC — DPLAY config instance */
extern int32_t  g_object_count;       /* 0x4AAD04 — object count       */
extern Netman*  _g_netman;            /* 0x4FD3AC — Netman singleton pointer */
extern PlayerConfig* g_player_config; /* 0x4AA4A8 — PlayerConfig singleton */
extern char     g_empty_string;       /* 0x4851D0 — empty string constant  */

/* -- CRT helpers (C++ linkage) -- */
void* operator_new(size_t size);              /* 0x465CE0 */
void  GLOBAL_free(void* ptr);                  /* 0x465CD0 */
void  CRT_exit(const char** msg, const char** fileLine);  /* 0x466CE0 */
int32_t CRT_atoi(const char* str);            /* 0x466390 */
void  CRT_itoa(int32_t val, char* buf, int32_t radix); /* 0x4663D0 */
int32_t CRT_rand(void);                       /* 0x466150 */
void  CRT_time(void);                         /* 0x466490 */

/* -- Game engine functions -- */
void  Train_QueueMessage(void* train, TrainMessage* msg);
void* UIPANEL_CreateSurface(void* surface);
void  TileMap_CreateOverlay(void* tilemap, void* surface, int32_t flags);

/* -- Resource manager functions -- */
void    ResourceManager_Init(void* resdata);         /* was RESMGR_ResourceData_Init */
uint8_t ResourceManager_LoadResource(void* resdata, const char* path); /* was RESMGR_LoadResource */
void    ResourceManager_ReleaseResource(void* resdata); /* was RESMGR_ReleaseResource */
void    ResourceManager_LoadSoundResource(int32_t resId); /* was RESMGR_LoadSoundResource */
void*   ResourceManager_GetById(void* resmgr, uint32_t id); /* was ResourceManager_GetById */
int32_t ResourceManager_GetStringById(void* resmgr, uint32_t id);

/* -- UI functions -- */
void  EditorState_LoadExistingGame(void* uiPanel);     /* was GAMESTATE_LoadExistingGame */
void  EditorState_HandleNetworkGame(void* uiPanel);    /* was GAMESTATE_HandleNetworkGame */
void  EditorState_SelectLayout(void* uiPanel, int32_t layoutData); /* was GAMESTATE_SelectLayout */
void  EditorState_StartGameTimer(int32_t* uiPanel);    /* was GAMESTATE_StartGameTimer */
void  EditorState_SetDifficulty(void* uiPanel, int32_t difficulty); /* was GAMESTATE_SetDifficulty */
void  CGWND_GameSetup_DrawGrid_Thunk(void* uiPanel);
void  CGWND_QuitToMenu(void);
void  UIPANEL_EndPaintEx(void* panel, void* hwnd, int32_t hdc,
                          uint8_t repaint, void* updateRect);
void  UIPANEL_EndPaint(void* panel);
void  UI_MainMenu_SetState(void* ui_main, int32_t state);

/* -- Audio -- */
/* Canonical signature (ResourceManager.h, 0x447930).  The old int32_t
 * forms hijacked overload resolution in TUs including both headers,
 * binding calls to the never-defined _Z9PlaySoundi /
 * _Z11PlaySoundAtiiii instead of the real _Z9PlaySoundj (runtime
 * crash with the ignore-all link). */
void    PlaySound(unsigned int soundId);
void    PlaySoundAt(unsigned int soundId, int32_t x, int32_t y, int32_t flags);
int32_t PlaySoundFile(const char* path, void* x, void* y, int32_t flags);

/* -- Network helpers -- */
void*   NET_CreateSession(void* dplay, uint8_t param1, uint8_t param2,
                           uint8_t param3, char param4);
void*   NET_RegisterPlayer(void* dplay, void* playerData, int32_t type, int32_t param);
void*   NET_ResolveAddress(void* dplay, const char* path);
/* NET_GetHostName / NET_UpdatePlayerList / NET_DownloadAsset (real bodies in
 * network/NetworkPlayerList.cpp, addresses 0x4446F0/0x445170/0x445A40) and
 * the PostBagFileNode type they share are declared in PostBagFileNode.h —
 * split out so town/Town.cpp can pull in just that instead of this whole
 * header (see PostBagFileNode.h's comment for why). */
#include "PostBagFileNode.h"
void    NET_GetAttFilePath(uint16_t id, int32_t type, char* outPath);
void    NET_SendFile(const char* name, uint8_t flag, char* pathBuf);

/* -- VehicleEditor helpers -- */
int32_t VehicleEditor_GetDPlayData(int32_t trackPiece);
void    VehicleEditor_SetDPlayData(void* trackPiece, int32_t data);
int32_t VehicleEditor_GetResourceId(int32_t trackPiece);

/* -- World / Game functions -- */
void    World_SerializeObject(void* world, int32_t param);
void __stdcall World_GetObjectAt(void* object);
uint8_t World_FinalizeLoad(void* world, InboundTrainNode* node, void* param, uint8_t dir);
InboundTrainNode* Vehicle_Ctor(InboundTrainNode* vehicle, int32_t resourceId, int32_t param2,
                      uint8_t param3, uint8_t param4);
void    Vehicle_CalcSpeed(InboundTrainNode* vehicle, int16_t speed);
void    Vehicle_InitRoute(InboundTrainNode* vehicle, int32_t resourceId, int32_t param2, uint8_t param3);
void    Vehicle_SetState(InboundTrainNode* vehicle, int32_t state);

/* -- UI helpers -- */
void    FormatResourceString(void* resmgr, uint32_t id, char* buf, int32_t bufsize);
void    Sprite_Init(void* sprite);
void    Sprite_Destroy(void* sprite);
void    Sprite_SetState(void* sprite, int32_t state, int32_t* unk);
void    UI_WindowBase_Show(void* window);
void    UI_WindowBase_Hide(void* window);
void    UIPANEL_StretchBlit(void* surface, const char* path, int32_t x, int32_t y, int32_t flags);
/* Real def: shared/defsym_stubs.cpp — signature corrected to match
 * network/NetworkPlayerList.cpp's evidenced declaration (see that file's
 * own comment on this symbol); no caller in this codebase currently
 * declares it via this header. */
void*   UIPANEL_CopySurface(void* dst, UIPANEL_Surface* src);
void    Config_GetIniInt(void* ini, const char* section, const char* key, int32_t def);
void    Config_WriteInt(void* ini, const char* section, const char* key, int32_t val);

/* -- Direction mapping (binary ABI: return the output pointer, EAX;  --
 * -- callers dereference it — 0x43E252 mov (%eax),%ecx)             -- */
int32_t* INPUT_DirToOffset_Up(int32_t* param);
int32_t* INPUT_DirToOffset_Down(int32_t* param);
int32_t* INPUT_DirToOffset_Left(int32_t* param);
int32_t* INPUT_DirToOffset_Right(int32_t* param);

/* -- DPLAY helper functions -- */
void*   DPLAY_CreatePlayer(void* slot);
int32_t DPLAY_GetPlayerName(void* slot, const char* path);
int32_t DPLAY_SetPlayerData(void* slot, const char* path);
void    DPLAY_SetPlayerName(void* slot, int32_t trainId, int8_t specific);
void    DPLAY_CopyPlayerData(void* dstSlot, const void* packet); /* 0x4426D0 */
void    DPLAY_InitPlayerSlot(void* dstSlot, const void* srcSlot); /* 0x442750 */
void    DPLAY_FreePlayerSlot(void* packet, const int32_t* slotSrc); /* 0x4427D0 */
#ifndef _WIN32
void*   DPLAY_DecodePlayerSlots(const void* firstCompactSlot);
#endif  /* @ 0x442750 */
int16_t DPLAY_GetMessageCount(int32_t dplay);
void    DPLAY_EnumeratePlayers(int32_t dplay);

/* -- Stream I/O -- */
void* WIN32_StreamFromMemory(void* stream, const char* data,
                              int32_t size, int32_t flags);
void* WIN32_StreamOpenFile(void* stream, const char* path,
                            int32_t mode, int32_t flags, int32_t param);
void  WIN32_StreamRead(void* stream, void* buf, int32_t size);

/* -- Asset manager -- */
uint8_t* AssetMgr_LoadFile(AssetMgr* self, uint8_t* filename, int32_t* out_size);

/* -- NET class helpers -- */
uint32_t NET_Dtor(uint8_t param1, uint8_t param2, uint8_t param3);
void     NET_Ctor(void* dplay, void* param1, uint32_t param2, uint32_t param3,
                   int32_t param4, uint32_t param5, uint8_t* param6);
void     NET_BaseDtor(void* dplay);

/* -- Network UI helpers -- */
void  NETMAN_EnumerateSessions(int32_t panel);
void  NETMAN_JoinSession(void* panel);
void  NETMAN_CreateSession(int32_t panel);
void  NETMAN_LeaveSession(int32_t panel);
void  NETMAN_UpdateSessionInfo(void* panel);
void  NETMAN_GetSessionInfo(int32_t panel);
void  NETMAN_SetSessionInfo(void* panel);
void* NETMAN_DestroySession(void* panel, void* hWnd, uint32_t msg,
                             uint32_t wParam, uint32_t lParam);

/* -- DirectPlay message/file management -- */
void  DPLAY_SendMessages(void);
void __stdcall DPLAY_ReceiveMessage(const char* path);

/* -- Network settings persistence -- */
void  NETMAN_FreePacket(int32_t packetPtr);
void  NETMAN_SendPacket(int32_t packetPtr);

/* -- DPlayConfig destructor -- */
void* NETMAN_FreeProviderList(void* config, uint8_t flags);

/* -- Postcard attachment ID -- */
uint16_t NET_BaseDtor(void);
